/* ========================================================================
 * SESSION ARCHIVE (Telemetry 1.0.1)
 *
 * Owner rule 2026-10-12: the telemetry app accumulates data for live AND
 * post-session review. Everything the console shows for a session is written
 * to disk while the session runs, so nothing is lost when the session ends,
 * restarts, or the game exits:
 *
 *   <module dir>\telemetry_data\<session id>\
 *     meta.json        identity, counts, drivers (rewritten, tmp + rename)
 *     frame.json       latest timing frame (/telemetry shape), every 30 s + at close
 *     weather.json     /weather document, every 30 s + at close
 *     track.json       /track document, once
 *     traces.bin       every completed trace lap of every car (append)
 *     laps.bin         every completed lap row (append)
 *     pits.bin         every completed pit stop (append)
 *     incidents.bin    every incident (append)
 *     journal.jsonl    the race journal stream (lap/wear/heartbeat/... lines)
 *
 * Threads. Producers (ar_on_*, ar_journal_line, ar_tick_locked) run only on the
 * worker thread inside read_telemetry / the worker's locked block, and only
 * append to memory. ar_flush_io runs on the worker thread OUTSIDE
 * g_telemetry_cs and does every file write. The HTTP thread reads archived
 * files and deletes sessions under g_ar_lock; the worker takes g_ar_lock only for
 * the session identity, retention deletes and the directory create.
 *
 * A session = one (broker generation, session type, track) key; a session
 * clock restart starts a new one. A folder is created only when the session
 * has its first completed lap, so loading screens and instant restarts leave
 * nothing behind (the 1.0.0 journal created 798 files).
 * ======================================================================== */
#define AR_ROOT_NAME       "telemetry_data"
#define AR_ID_LEN          48
#define AR_BUF_CAP         (4u * 1024u * 1024u)  /* per pending stream before drop */
#define AR_PENDING_LAPS    256                   /* queued trace laps before drop */
#define AR_DOC_MS          30000                 /* frame/weather/meta rewrite cadence */
#define AR_FRAME_MS        2000                  /* in-memory frame snapshot cadence */
#define AR_FLUSH_MS        1000                  /* append-stream flush cadence */
#define AR_TRACE_MAGIC     0x4C433146u           /* "F1CL" */
#define AR_TRACE_VERSION   1u
#define AR_DISK_SAMPLE     ((int)offsetof(trace_sample_t, rx))  /* rx (RESEARCH) not archived */
#define AR_DOC_CAP         262144
#define AR_FILE_MAX        (64u * 1024u * 1024u) /* HTTP read cap per archived file */
#define AR_LIST_MAX        400

typedef struct {
    uint32_t magic, version;
    int32_t  slot, lap, flags, samples, sample_bytes;
    float    lap_time, start_sim, lap_len;
    uint32_t gen, rev, team_id, channels_seen;
    int32_t  is_ai, is_remote;
    char     driver[NAME_LEN], car[NAME_LEN];
} ar_lap_hdr_t;

typedef struct {
    int32_t   slot, position;
    char      name[NAME_LEN], car[NAME_LEN];
    lap_row_t row;
} ar_lap_rec_t;

typedef struct {
    int32_t     slot;
    pit_event_t e;
} ar_pit_rec_t;

typedef struct {
    incident_t e;
    char       name[NAME_LEN];
} ar_inc_rec_t;

typedef struct { char *p; size_t n, cap; } ar_buf_t;

typedef struct ar_tlap {
    struct ar_tlap *next;
    ar_lap_hdr_t    h;
    unsigned char  *samples;
} ar_tlap_t;

typedef struct {
    int      active, materialized;
    char     id[AR_ID_LEN];
    char     dir[MAX_PATH];
    unsigned gen;
    int      ty;
    char     track[64];
    float    lap_len;
    double   start_unix, last_unix;
    int      laps, trace_laps, pits, incidents;
    unsigned dropped;
    ar_buf_t journal, lapsb, pitsb, incsb;
    ar_tlap_t *tl_head, *tl_tail;
    int      tl_count;
    char    *frame, *weather, *trackdoc;
    int      frame_len, weather_len, trackdoc_len;
    int      frame_dirty, weather_dirty, track_dirty, meta_dirty;
    DWORD    last_frame, last_doc, last_flush, last_write;
    char     drv_name[MAX_CARS][NAME_LEN];
    char     drv_car[MAX_CARS][NAME_LEN];
    int      drv_laps[MAX_CARS];
} ar_session_t;

static SRWLOCK g_ar_lock = SRWLOCK_INIT;   /* static init: usable before ar_init */
static char g_ar_root[MAX_PATH];       /* ...\telemetry_data\ ; empty = archive off */
static char g_ar_module_dir[MAX_PATH]; /* ...\plugins\lightloader\ */
static char g_ar_current_id[AR_ID_LEN];/* materialized current session, for HTTP */
static ar_session_t g_ar, g_ar_done;   /* current, and the one being finalized */
static ar_buf_t g_ar_pre;              /* journal lines before a session is active */
static unsigned g_ar_total_dropped;
static int g_ar_last_retention_deleted;

static double ar_now_unix(void) {
    FILETIME ft; ULARGE_INTEGER u;
    GetSystemTimeAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    return ((double)u.QuadPart - 116444736000000000.0) / 10000000.0;
}

static int ar_buf_add(ar_buf_t *b, const void *data, size_t len) {
    if (b->n + len > AR_BUF_CAP) return 0;
    if (b->n + len > b->cap) {
        size_t cap = b->cap ? b->cap : 16384;
        while (cap < b->n + len) cap *= 2;
        if (cap > AR_BUF_CAP) cap = AR_BUF_CAP;
        char *p = (char *)realloc(b->p, cap);
        if (!p) return 0;
        b->p = p; b->cap = cap;
    }
    memcpy(b->p + b->n, data, len);
    b->n += len;
    return 1;
}
static void ar_buf_free(ar_buf_t *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* Session ids are produced here only; HTTP accepts nothing else. */
static int ar_id_valid(const char *id) {
    size_t n = id ? strlen(id) : 0;
    if (n < 8 || n >= AR_ID_LEN) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_'))
            return 0;
    }
    return 1;
}

/* Called once at activation with the module directory (ends in '\'). */
static void ar_init(const char *module_dir) {
    g_ar_root[0] = g_ar_module_dir[0] = 0;
    if (!module_dir || !module_dir[0]) return;
    if (snprintf(g_ar_module_dir, sizeof g_ar_module_dir, "%s", module_dir) >= (int)sizeof g_ar_module_dir ||
        snprintf(g_ar_root, sizeof g_ar_root, "%s" AR_ROOT_NAME "\\", module_dir) >= (int)sizeof g_ar_root - AR_ID_LEN - 24) {
        g_ar_root[0] = g_ar_module_dir[0] = 0;
    }
}

static int ar_path(char *out, size_t sz, const char *id, const char *file) {
    if (!g_ar_root[0] || !ar_id_valid(id)) return 0;
    return snprintf(out, sz, "%s%s\\%s", g_ar_root, id, file ? file : "") < (int)sz;
}

static void ar_session_free(ar_session_t *s) {
    ar_buf_free(&s->journal); ar_buf_free(&s->lapsb); ar_buf_free(&s->pitsb); ar_buf_free(&s->incsb);
    for (ar_tlap_t *t = s->tl_head; t; ) { ar_tlap_t *n = t->next; free(t->samples); free(t); t = n; }
    free(s->frame); free(s->weather); free(s->trackdoc);
    memset(s, 0, sizeof *s);
}

/* ---------------- producers (worker thread, under g_telemetry_cs) -------- */

static void ar_note_driver(int slot, const char *name, const char *car) {
    if (slot < 0 || slot >= MAX_CARS) return;
    if (name && name[0]) snprintf(g_ar.drv_name[slot], NAME_LEN, "%s", name);
    if (car && car[0]) snprintf(g_ar.drv_car[slot], NAME_LEN, "%s", car);
}

static void ar_journal_line(const char *line, int len) {
    ar_buf_t *b = g_ar.active ? &g_ar.journal : &g_ar_pre;
    if (!g_ar_root[0] || len <= 0) return;
    if (!ar_buf_add(b, line, (size_t)len) || !ar_buf_add(b, "\n", 1)) {
        g_ar.dropped++; g_ar_total_dropped++;
    }
}

static void ar_on_lap_row(int slot, const car_telemetry_t *c, const lap_row_t *r) {
    ar_lap_rec_t rec;
    if (!g_ar_root[0] || !g_ar.active || slot < 0 || slot >= MAX_CARS) return;
    memset(&rec, 0, sizeof rec);
    rec.slot = slot; rec.position = c->position;
    snprintf(rec.name, sizeof rec.name, "%s", c->name);
    snprintf(rec.car, sizeof rec.car, "%s", c->car_name);
    rec.row = *r;
    if (!ar_buf_add(&g_ar.lapsb, &rec, sizeof rec)) { g_ar.dropped++; g_ar_total_dropped++; return; }
    ar_note_driver(slot, c->name, c->car_name);
    g_ar.drv_laps[slot]++;
    g_ar.laps++;
    g_ar.meta_dirty = 1;
}

static void ar_on_trace_lap(const trace_car_t *t, int idx) {
    ar_tlap_t *job;
    int count;
    if (!g_ar_root[0] || !g_ar.active || idx < 0 || idx >= TRACE_LAP_SLOTS) return;
    count = t->lap_samples[idx];
    if (count <= 0 || count > TRACE_SAMPLES) return;
    if (g_ar.tl_count >= AR_PENDING_LAPS) { g_ar.dropped++; g_ar_total_dropped++; return; }
    job = (ar_tlap_t *)calloc(1, sizeof *job);
    if (!job) { g_ar.dropped++; g_ar_total_dropped++; return; }
    job->samples = (unsigned char *)malloc((size_t)count * AR_DISK_SAMPLE);
    if (!job->samples) { free(job); g_ar.dropped++; g_ar_total_dropped++; return; }
    for (int k = 0; k < count; k++)
        memcpy(job->samples + (size_t)k * AR_DISK_SAMPLE, &t->laps[idx][k], AR_DISK_SAMPLE);
    job->h.magic = AR_TRACE_MAGIC; job->h.version = AR_TRACE_VERSION;
    job->h.slot = t->slot; job->h.lap = t->lap_num[idx]; job->h.flags = t->lap_valid[idx];
    job->h.samples = count; job->h.sample_bytes = AR_DISK_SAMPLE;
    job->h.lap_time = t->lap_time[idx]; job->h.start_sim = t->lap_start_sim[idx];
    job->h.lap_len = g_telemetry.lap_len;
    job->h.gen = capture_generation; job->h.rev = t->revision;
    job->h.team_id = t->team_id; job->h.channels_seen = t->channels_seen;
    job->h.is_ai = t->is_ai; job->h.is_remote = t->is_remote;
    snprintf(job->h.driver, sizeof job->h.driver, "%s", t->driver_name);
    snprintf(job->h.car, sizeof job->h.car, "%s", t->car_name);
    if (g_ar.tl_tail) g_ar.tl_tail->next = job; else g_ar.tl_head = job;
    g_ar.tl_tail = job;
    g_ar.tl_count++;
    g_ar.trace_laps++;
    ar_note_driver(t->slot, t->driver_name, t->car_name);
    g_ar.meta_dirty = 1;
}

static void ar_on_pit(int slot, const pit_event_t *e) {
    ar_pit_rec_t rec;
    if (!g_ar_root[0] || !g_ar.active) return;
    memset(&rec, 0, sizeof rec);
    rec.slot = slot; rec.e = *e;
    if (!ar_buf_add(&g_ar.pitsb, &rec, sizeof rec)) { g_ar.dropped++; g_ar_total_dropped++; return; }
    g_ar.pits++;
}

static void ar_on_incident(const incident_t *e, const char *name) {
    ar_inc_rec_t rec;
    if (!g_ar_root[0] || !g_ar.active) return;
    memset(&rec, 0, sizeof rec);
    rec.e = *e;
    snprintf(rec.name, sizeof rec.name, "%s", name ? name : "");
    if (!ar_buf_add(&g_ar.incsb, &rec, sizeof rec)) { g_ar.dropped++; g_ar_total_dropped++; return; }
    g_ar.incidents++;
}

static int ar_build_doc(char **dst, int *len, int (*builder)(char *, int)) {
    int n;
    if (!*dst) { *dst = (char *)malloc(AR_DOC_CAP); if (!*dst) return 0; }
    format_failed = 0;
    n = builder(*dst, AR_DOC_CAP);
    if (format_failed || n <= 0) { format_failed = 0; return 0; }
    *len = n;
    return 1;
}
static int ar_track_builder(char *b, int n) { return build_track_json(b, n); }

static void ar_begin(void) {
    SYSTEMTIME st;
    memset(&g_ar, 0, sizeof g_ar);
    g_ar.active = 1;
    g_ar.gen = capture_generation;
    g_ar.ty = g_telemetry.session_type;
    snprintf(g_ar.track, sizeof g_ar.track, "%s", g_telemetry.track_name);
    g_ar.lap_len = g_telemetry.lap_len;
    g_ar.start_unix = g_ar.last_unix = ar_now_unix();
    GetLocalTime(&st);
    snprintf(g_ar.id, sizeof g_ar.id, "%04d%02d%02d-%02d%02d%02d_s%d_g%u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             g_ar.ty < 0 || g_ar.ty > 99 ? 99 : g_ar.ty, g_ar.gen % 100000u);
    /* journal lines written before the key was known belong to this session */
    if (g_ar_pre.n) { ar_buf_add(&g_ar.journal, g_ar_pre.p, g_ar_pre.n); g_ar_pre.n = 0; }
}

/* Worker thread, inside g_telemetry_cs, after read_telemetry. Decides session
 * boundaries and refreshes the in-memory documents. No file IO here. */
static void ar_tick_locked(int fresh) {
    DWORD now = GetTickCount();
    if (!g_ar_root[0]) return;
    int live = fresh && g_telemetry.vehicle_count > 0;
    if (g_ar.active && live) {
        int track_differs = g_ar.track[0] && g_telemetry.track_name[0] &&
                            strcmp(g_ar.track, g_telemetry.track_name) != 0;
        if (g_ar.gen != capture_generation || g_ar.ty != g_telemetry.session_type ||
            track_differs || g_ar_restart_pending) {
            /* Hand the finished session to the IO step; its buffers move with it.
             * frame/weather keep the last snapshot taken under the old key. */
            if (g_ar_done.active) ar_session_free(&g_ar_done);   /* flush step missed; never leak */
            g_ar_done = g_ar;
            memset(&g_ar, 0, sizeof g_ar);
        }
    }
    g_ar_restart_pending = 0;
    if (!g_ar.active && live) ar_begin();
    if (!g_ar.active || !live) return;
    if (!g_ar.track[0] && g_telemetry.track_name[0])
        snprintf(g_ar.track, sizeof g_ar.track, "%s", g_telemetry.track_name);
    if (g_telemetry.lap_len > 0.0f) g_ar.lap_len = g_telemetry.lap_len;
    if (!g_ar.last_frame || now - g_ar.last_frame >= AR_FRAME_MS) {
        if (ar_build_doc(&g_ar.frame, &g_ar.frame_len, json_telemetry)) g_ar.frame_dirty = 1;
        g_ar.last_frame = now;
        g_ar.last_unix = ar_now_unix();
        for (int i = 0; i < g_telemetry.vehicle_count && i < MAX_CARS; i++)
            if (g_telemetry.cars[i].position > 0 && !g_ar.drv_name[i][0])
                ar_note_driver(i, g_telemetry.cars[i].name, g_telemetry.cars[i].car_name);
    }
    if (!g_ar.last_doc || now - g_ar.last_doc >= AR_DOC_MS) {
        if (ar_build_doc(&g_ar.weather, &g_ar.weather_len, build_weather_json)) g_ar.weather_dirty = 1;
        if (!g_ar.trackdoc_len && g_wp_count > 0 &&
            ar_build_doc(&g_ar.trackdoc, &g_ar.trackdoc_len, ar_track_builder)) g_ar.track_dirty = 1;
        g_ar.meta_dirty = 1;
        g_ar.last_doc = now;
    }
}

/* ---------------- IO (worker thread, outside g_telemetry_cs) ------------- */

static int ar_write_atomic(const char *dir, const char *name, const char *data, size_t len) {
    char path[MAX_PATH], tmp[MAX_PATH];
    FILE *f;
    if (snprintf(path, sizeof path, "%s%s", dir, name) >= (int)sizeof path ||
        snprintf(tmp, sizeof tmp, "%s%s.tmp", dir, name) >= (int)sizeof tmp) return 0;
    f = fopen(tmp, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, len, f) == len;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { DeleteFileA(tmp); return 0; }
    return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING) != 0;
}

static int ar_append(const char *dir, const char *name, const void *data, size_t len) {
    char path[MAX_PATH];
    FILE *f;
    if (!len) return 1;
    if (snprintf(path, sizeof path, "%s%s", dir, name) >= (int)sizeof path) return 0;
    f = fopen(path, "ab");
    if (!f) return 0;
    int ok = fwrite(data, 1, len, f) == len;
    return (fclose(f) == 0) && ok;
}

static void ar_write_meta(ar_session_t *s, int closed) {
    static char buf[65536];
    char trk[64 * 6 + 1];
    int p;
    esc_json(trk, sizeof trk, s->track);
    format_failed = 0;
    p = _snprintf(buf, sizeof buf,
        "{\"schema\":\"f1c.session/1\",\"id\":\"%s\",\"product\":\"" PRODUCT_LABEL "\",\"engine\":\"" TELEMETRY_VERSION "\","
        "\"start_unix\":%.0f,\"end_unix\":%.0f,\"closed\":%s,\"track\":\"%s\",\"ty\":%d,\"gen\":%u,\"lap_len\":%.1f,"
        "\"laps\":%d,\"trace_laps\":%d,\"pits\":%d,\"incidents\":%d,\"dropped\":%u,\"drivers\":[",
        s->id, s->start_unix, s->last_unix, closed ? "true" : "false", trk, s->ty, s->gen, s->lap_len,
        s->laps, s->trace_laps, s->pits, s->incidents, s->dropped);
    int first = 1;
    for (int i = 0; i < MAX_CARS; i++) {
        if (!s->drv_name[i][0]) continue;
        char en[NAME_LEN * 6 + 1], ec[NAME_LEN * 6 + 1];
        esc_json(en, sizeof en, s->drv_name[i]);
        esc_json(ec, sizeof ec, s->drv_car[i]);
        p += _snprintf(buf + p, sizeof buf - p, "%s{\"id\":%d,\"n\":\"%s\",\"cn\":\"%s\",\"laps\":%d}",
                       first ? "" : ",", i, en, ec, s->drv_laps[i]);
        first = 0;
    }
    p += _snprintf(buf + p, sizeof buf - p, "]}");
    if (!format_failed && p > 0) ar_write_atomic(s->dir, "meta.json", buf, (size_t)p);
    format_failed = 0;
}

static int ar_materialize(ar_session_t *s) {
    if (s->materialized) return 1;
    if (!s->laps && !s->trace_laps) return 0;      /* nothing worth a folder yet */
    if (snprintf(s->dir, sizeof s->dir, "%s%s\\", g_ar_root, s->id) >= (int)sizeof s->dir) return 0;
    AcquireSRWLockExclusive(&g_ar_lock);
    CreateDirectoryA(g_ar_root, NULL);
    for (int k = 2; k < 50 && GetFileAttributesA(s->dir) != INVALID_FILE_ATTRIBUTES; k++) {
        /* same second, same session type: keep ids unique */
        char base[AR_ID_LEN];
        snprintf(base, sizeof base, "%s", s->id);
        char *dash = strstr(base, "_n");
        if (dash) *dash = 0;
        if (snprintf(s->id, sizeof s->id, "%.*s_n%d", AR_ID_LEN - 6, base, k) >= (int)sizeof s->id ||
            snprintf(s->dir, sizeof s->dir, "%s%s\\", g_ar_root, s->id) >= (int)sizeof s->dir) break;
    }
    int ok = CreateDirectoryA(s->dir, NULL) != 0;
    if (ok) snprintf(g_ar_current_id, sizeof g_ar_current_id, "%s", s->id);
    ReleaseSRWLockExclusive(&g_ar_lock);
    if (!ok) { log_f("archive: cannot create %s (%lu)", s->dir, (unsigned long)GetLastError()); return 0; }
    s->materialized = 1;
    s->meta_dirty = s->frame_dirty = s->weather_dirty = s->track_dirty = 1;
    log_f("archive: session %s recording to %s", s->id, s->dir);
    return 1;
}

static void ar_flush_session(ar_session_t *s, int final) {
    DWORD now = GetTickCount();
    if (!s->active || !ar_materialize(s)) return;
    if (final || !s->last_flush || now - s->last_flush >= AR_FLUSH_MS) {
        s->last_flush = now;
        if (s->tl_head) {
            char path[MAX_PATH];
            FILE *f = NULL;
            if (snprintf(path, sizeof path, "%straces.bin", s->dir) < (int)sizeof path) f = fopen(path, "ab");
            while (s->tl_head) {
                ar_tlap_t *t = s->tl_head;
                if (f) {
                    fwrite(&t->h, sizeof t->h, 1, f);
                    fwrite(t->samples, (size_t)AR_DISK_SAMPLE, (size_t)t->h.samples, f);
                }
                s->tl_head = t->next;
                free(t->samples); free(t);
            }
            s->tl_tail = NULL; s->tl_count = 0;
            if (f) fclose(f);
        }
        if (s->lapsb.n && ar_append(s->dir, "laps.bin", s->lapsb.p, s->lapsb.n)) s->lapsb.n = 0;
        if (s->pitsb.n && ar_append(s->dir, "pits.bin", s->pitsb.p, s->pitsb.n)) s->pitsb.n = 0;
        if (s->incsb.n && ar_append(s->dir, "incidents.bin", s->incsb.p, s->incsb.n)) s->incsb.n = 0;
        if (s->journal.n && ar_append(s->dir, "journal.jsonl", s->journal.p, s->journal.n)) s->journal.n = 0;
    }
    if (final || !s->last_write || now - s->last_write >= AR_DOC_MS) {
        s->last_write = now;
        if (s->frame_dirty && s->frame_len > 0 && ar_write_atomic(s->dir, "frame.json", s->frame, (size_t)s->frame_len))
            s->frame_dirty = 0;
        if (s->weather_dirty && s->weather_len > 0 && ar_write_atomic(s->dir, "weather.json", s->weather, (size_t)s->weather_len))
            s->weather_dirty = 0;
        if (s->track_dirty && s->trackdoc_len > 0 && ar_write_atomic(s->dir, "track.json", s->trackdoc, (size_t)s->trackdoc_len))
            s->track_dirty = 0;
        if (s->meta_dirty || final) { ar_write_meta(s, final); s->meta_dirty = 0; }
    }
}

static unsigned long long ar_dir_bytes(const char *dir_with_slash) {
    char pat[MAX_PATH];
    WIN32_FIND_DATAA fd;
    unsigned long long total = 0;
    if (snprintf(pat, sizeof pat, "%s*", dir_with_slash) >= (int)sizeof pat) return 0;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            total += ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return total;
}

/* Deletes one session folder (flat; only regular files). Caller holds g_ar_lock. */
static int ar_delete_session_locked(const char *id, unsigned long long *freed) {
    char dir[MAX_PATH], pat[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA fd;
    if (!ar_id_valid(id) || !strcmp(id, g_ar_current_id)) return 0;
    if (snprintf(dir, sizeof dir, "%s%s\\", g_ar_root, id) >= (int)sizeof dir) return 0;
    DWORD attr = GetFileAttributesA(dir);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY) ||
        (attr & FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
    if (snprintf(pat, sizeof pat, "%s*", dir) >= (int)sizeof pat) return 0;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (snprintf(path, sizeof path, "%s%s", dir, fd.cFileName) >= (int)sizeof path) continue;
            if (DeleteFileA(path) && freed)
                *freed += ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    return RemoveDirectoryA(dir) != 0;
}

typedef struct { char id[AR_ID_LEN]; unsigned long long bytes; } ar_entry_t;
static int ar_entry_cmp_desc(const void *a, const void *b) {
    return strcmp(((const ar_entry_t *)b)->id, ((const ar_entry_t *)a)->id);
}

/* Session folders, newest first (ids start with the local start time). */
static int ar_list_sessions(ar_entry_t *out, int max) {
    char pat[MAX_PATH];
    WIN32_FIND_DATAA fd;
    int n = 0;
    if (!g_ar_root[0] || snprintf(pat, sizeof pat, "%s*", g_ar_root) >= (int)sizeof pat) return 0;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        if (!ar_id_valid(fd.cFileName) || n >= max) continue;
        memcpy(out[n].id, fd.cFileName, strlen(fd.cFileName) + 1);   /* ar_id_valid: < AR_ID_LEN */
        char dir[MAX_PATH];
        out[n].bytes = snprintf(dir, sizeof dir, "%s%s\\", g_ar_root, fd.cFileName) < (int)sizeof dir ? ar_dir_bytes(dir) : 0;
        n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    qsort(out, (size_t)n, sizeof *out, ar_entry_cmp_desc);
    return n;
}

/* Storage cap: delete the oldest finished sessions until the archive fits.
 * [Mod] MaxStorageMB=0 disables the cap. The current session is never deleted. */
static void ar_retention(void) {
    static ar_entry_t list[AR_LIST_MAX];
    if (g_ar_max_mb <= 0 || !g_ar_root[0]) return;
    AcquireSRWLockExclusive(&g_ar_lock);
    int n = ar_list_sessions(list, AR_LIST_MAX);
    unsigned long long total = 0, cap = (unsigned long long)g_ar_max_mb * 1024ull * 1024ull;
    for (int i = 0; i < n; i++) total += list[i].bytes;
    for (int i = n - 1; i >= 0 && total > cap; i--) {
        unsigned long long freed = 0;
        if (!strcmp(list[i].id, g_ar_current_id)) continue;
        if (ar_delete_session_locked(list[i].id, &freed)) {
            total -= list[i].bytes;
            g_ar_last_retention_deleted++;
            log_f("archive: storage cap %d MB: removed oldest session %s", g_ar_max_mb, list[i].id);
        }
    }
    ReleaseSRWLockExclusive(&g_ar_lock);
}

/* Worker thread, outside g_telemetry_cs. */
static void ar_flush_io(int final) {
    if (!g_ar_root[0]) return;
    if (g_ar_done.active) {
        int had = g_ar_done.materialized || g_ar_done.laps || g_ar_done.trace_laps;
        g_ar_done.last_unix = ar_now_unix();
        ar_flush_session(&g_ar_done, 1);
        if (g_ar_done.materialized)
            log_f("archive: session %s closed (%d laps, %d trace laps, %d pits, %d incidents)",
                  g_ar_done.id, g_ar_done.laps, g_ar_done.trace_laps, g_ar_done.pits, g_ar_done.incidents);
        AcquireSRWLockExclusive(&g_ar_lock);
        if (!strcmp(g_ar_current_id, g_ar_done.id)) g_ar_current_id[0] = 0;
        ReleaseSRWLockExclusive(&g_ar_lock);
        ar_session_free(&g_ar_done);
        if (had) ar_retention();
    }
    if (g_ar.active) {
        if (final) g_ar.last_unix = ar_now_unix();
        ar_flush_session(&g_ar, final);
    }
}

/* ---------------- HTTP side (HTTP thread) -------------------------------- */

static char *ar_read_file(const char *id, const char *name, size_t *len_out) {
    char path[MAX_PATH];
    FILE *f;
    long n;
    char *buf;
    *len_out = 0;
    if (!ar_path(path, sizeof path, id, name)) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || (unsigned long)n > AR_FILE_MAX ||
        fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    buf[n] = 0;
    *len_out = (size_t)n;
    return buf;
}

static int ar_session_exists(const char *id) {
    char dir[MAX_PATH];
    if (!ar_path(dir, sizeof dir, id, "")) return 0;
    DWORD a = GetFileAttributesA(dir);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}

static unsigned ar_meta_gen(const char *id) {
    size_t n;
    unsigned gen = 0;
    char *m = ar_read_file(id, "meta.json", &n);
    if (m) {
        const char *g = strstr(m, "\"gen\":");
        if (g) gen = (unsigned)strtoul(g + 6, NULL, 10);
        free(m);
    }
    return gen;
}

/* GET /sessions: every archived session (meta verbatim) + sizes, plus the
 * 1.0.0 journal files still lying beside the DLL. */
static int ar_build_sessions_json(char *buf, int bufsz) {
    static ar_entry_t list[AR_LIST_MAX];
    char cur[AR_ID_LEN], root[MAX_PATH * 6 + 1];
    unsigned long long total = 0, legacy_bytes = 0;
    int legacy_n = 0, n, p;
    AcquireSRWLockExclusive(&g_ar_lock);
    snprintf(cur, sizeof cur, "%s", g_ar_current_id);
    n = ar_list_sessions(list, AR_LIST_MAX);
    for (int i = 0; i < n; i++) total += list[i].bytes;
    if (g_ar_module_dir[0]) {
        char pat[MAX_PATH];
        WIN32_FIND_DATAA fd;
        if (snprintf(pat, sizeof pat, "%sweb_telemetry_race_*.jsonl", g_ar_module_dir) < (int)sizeof pat) {
            HANDLE h = FindFirstFileA(pat, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    legacy_n++;
                    legacy_bytes += ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
        }
    }
    esc_json(root, sizeof root, g_ar_root);
    p = _snprintf(buf, bufsz,
        "{\"enabled\":%s,\"dir\":\"%s\",\"current\":\"%s\",\"total_bytes\":%llu,\"max_mb\":%d,"
        "\"legacy_journals\":{\"files\":%d,\"bytes\":%llu},\"sessions\":[",
        g_ar_root[0] ? "true" : "false", root, cur, total, g_ar_max_mb, legacy_n, legacy_bytes);
    for (int i = 0; i < n; i++) {
        size_t ml = 0;
        char *meta = ar_read_file(list[i].id, "meta.json", &ml);
        int ok_meta = meta && ml > 1 && ml < 60000 && meta[0] == '{' && meta[ml - 1] == '}';
        p += _snprintf(buf + p, bufsz - p, "%s{\"id\":\"%s\",\"bytes\":%llu,\"current\":%s,\"meta\":%s}",
                       i ? "," : "", list[i].id, list[i].bytes, strcmp(list[i].id, cur) ? "false" : "true",
                       ok_meta ? meta : "null");
        free(meta);
        if (p >= bufsz - 4096) break;
    }
    ReleaseSRWLockExclusive(&g_ar_lock);
    p += _snprintf(buf + p, bufsz - p, "]}");
    return p;
}

/* POST /sessions/delete body: one id per line, or "legacy-journals". */
static int ar_build_delete_json(const char *body, size_t len, char *buf, int bufsz) {
    char line[AR_ID_LEN + 8];
    unsigned long long freed = 0;
    int p = _snprintf(buf, bufsz, "{\"deleted\":["), nd = 0, nr = 0;
    char refused[4096]; int rp = 0; refused[0] = 0;
    size_t i = 0;
    AcquireSRWLockExclusive(&g_ar_lock);
    while (i < len) {
        size_t k = 0;
        while (i < len && (body[i] == '\r' || body[i] == '\n' || body[i] == ' ')) i++;
        while (i < len && body[i] != '\r' && body[i] != '\n' && k < sizeof line - 1) line[k++] = body[i++];
        while (i < len && body[i] != '\n') i++;      /* overlong token: skip rest */
        line[k] = 0;
        if (!k) continue;
        int ok = 0;
        const char *why = "unknown session";
        if (!strcmp(line, "legacy-journals")) {
            char pat[MAX_PATH], path[MAX_PATH];
            WIN32_FIND_DATAA fd;
            if (g_ar_module_dir[0] && snprintf(pat, sizeof pat, "%sweb_telemetry_race_*.jsonl", g_ar_module_dir) < (int)sizeof pat) {
                HANDLE h = FindFirstFileA(pat, &fd);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        if (snprintf(path, sizeof path, "%s%s", g_ar_module_dir, fd.cFileName) >= (int)sizeof path) continue;
                        if (DeleteFileA(path)) freed += ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                    } while (FindNextFileA(h, &fd));
                    FindClose(h);
                }
                ok = 1;
            }
        } else if (!ar_id_valid(line)) {
            why = "invalid id";
        } else if (!strcmp(line, g_ar_current_id)) {
            why = "session is still recording";
        } else {
            ok = ar_delete_session_locked(line, &freed);
            if (!ok) why = "not found or in use";
        }
        if (ok) {
            p += _snprintf(buf + p, bufsz - p, "%s\"%s\"", nd ? "," : "", line);
            nd++;
        } else if (rp < (int)sizeof refused - 160) {
            char esc_id[(AR_ID_LEN + 8) * 6 + 1];
            esc_json(esc_id, sizeof esc_id, line);
            rp += snprintf(refused + rp, sizeof refused - rp, "%s{\"id\":\"%s\",\"why\":\"%s\"}", nr ? "," : "", esc_id, why);
            nr++;
        }
    }
    ReleaseSRWLockExclusive(&g_ar_lock);
    p += _snprintf(buf + p, bufsz - p, "],\"refused\":[%s],\"freed_bytes\":%llu}", refused, freed);
    return p;
}

/* Archived lap rows of one session (laps.bin). Caller frees. */
static ar_lap_rec_t *ar_load_laps(const char *id, int *count) {
    size_t n;
    char *b = ar_read_file(id, "laps.bin", &n);
    *count = b ? (int)(n / sizeof(ar_lap_rec_t)) : 0;
    return (ar_lap_rec_t *)b;
}

static int ar_build_laps_json(const char *id, char *buf, int bufsz) {
    int n, p, first = 1;
    ar_lap_rec_t *r = ar_load_laps(id, &n);
    p = _snprintf(buf, bufsz, "{\"gen\":%u,\"archive\":\"%s\",\"cars\":[", ar_meta_gen(id), id);
    for (int slot = 0; slot < MAX_CARS && r; slot++) {
        int last = -1, any = 0;
        for (int k = 0; k < n; k++) if (r[k].slot == slot) { last = k; any = 1; }
        if (!any) continue;
        char esc[NAME_LEN * 6 + 1];
        esc_json(esc, sizeof esc, r[last].name);
        p += _snprintf(buf + p, bufsz - p, "%s{\"id\":%d,\"p\":%d,\"n\":\"%s\",\"laps\":[",
                       first ? "" : ",", slot, r[last].position, esc);
        first = 0;
        int j = 0;
        for (int k = 0; k < n; k++)
            if (r[k].slot == slot) p += _snprintf(buf + p, bufsz - p, "%s%.3f", j++ ? "," : "", r[k].row.t);
        p += _snprintf(buf + p, bufsz - p, "]}");
        if (p >= bufsz - 256) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    free(r);
    return p;
}

static int ar_fmt_pit(char *buf, int bufsz, int p, const pit_event_t *e, int first, int with_name) {
    double boxdur = (e->t_box_stop > 0 && e->t_exit_start > 0) ? (e->t_exit_start - e->t_box_stop) : 0.0;
    double reqdelay = (e->t_requested > 0) ? (e->t_in - e->t_requested) : 0.0;
    char esc[NAME_LEN * 6 + 1];
    char nm[NAME_LEN * 6 + 32];
    nm[0] = 0;
    if (with_name) {
        char tmpn[NAME_LEN];
        snprintf(tmpn, sizeof tmpn, "%s", e->name);
        esc_json(esc, sizeof esc, tmpn);
        snprintf(nm, sizeof nm, "\"id\":%d,\"n\":\"%s\",", e->car_id, esc);
    }
    /* Same keys as the live documents: /pits rows carry id+n, /driver rows add. */
    char add[48];
    add[0] = 0;
    if (!with_name) snprintf(add, sizeof add, "\"add\":%.1f,", e->completed ? (e->fuel_out - e->fuel_in) : 0.0);
    return p + _snprintf(buf + p, bufsz - p,
        "%s{%s\"li\":%d,\"lo\":%d,\"d\":%.2f,\"boxdur\":%.2f,\"reqdelay\":%.2f,\"ti\":%d,\"to\":%d,"
        "\"fi\":%.1f,\"fo\":%.1f,%s\"rfw\":%d,\"rrw\":%d,\"dmg\":%d,\"done\":%d}",
        first ? "" : ",", nm, e->lap_in, e->lap_out,
        e->completed ? (e->t_out - e->t_in) : 0.0, boxdur, reqdelay,
        e->tire_in, e->tire_out, e->fuel_in, e->fuel_out, add,
        e->fixed_fw, e->fixed_rw, e->dmg_in, e->completed);
}

static int ar_build_pits_json(const char *id, char *buf, int bufsz) {
    size_t n;
    char *b = ar_read_file(id, "pits.bin", &n);
    int cnt = b ? (int)(n / sizeof(ar_pit_rec_t)) : 0, p;
    const ar_pit_rec_t *r = (const ar_pit_rec_t *)b;
    p = _snprintf(buf, bufsz, "{\"gen\":%u,\"archive\":\"%s\",\"pits\":[", ar_meta_gen(id), id);
    for (int k = 0; k < cnt; k++) {
        p = ar_fmt_pit(buf, bufsz, p, &r[k].e, k == 0, 1);
        if (p >= bufsz - 512) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    free(b);
    return p;
}

static int ar_build_incidents_json(const char *id, char *buf, int bufsz) {
    size_t n;
    char *b = ar_read_file(id, "incidents.bin", &n);
    int cnt = b ? (int)(n / sizeof(ar_inc_rec_t)) : 0, p;
    const ar_inc_rec_t *r = (const ar_inc_rec_t *)b;
    p = _snprintf(buf, bufsz, "{\"gen\":%u,\"archive\":\"%s\",\"ev\":[", ar_meta_gen(id), id);
    for (int k = 0; k < cnt; k++) {
        char esc[NAME_LEN * 6 + 1], nm[NAME_LEN], ty[sizeof r[k].e.type + 1];
        snprintf(nm, sizeof nm, "%s", r[k].name[0] ? r[k].name : "SESSION");
        memcpy(ty, r[k].e.type, sizeof r[k].e.type); ty[sizeof r[k].e.type] = 0;
        esc_json(esc, sizeof esc, nm);
        char ety[sizeof ty * 6 + 1];
        esc_json(ety, sizeof ety, ty);
        p += _snprintf(buf + p, bufsz - p,
            "%s{\"tk\":%lu,\"id\":%d,\"n\":\"%s\",\"ty\":\"%s\",\"lp\":%d,\"a\":%d,\"b\":%d}",
            k ? "," : "", (unsigned long)r[k].e.tick, r[k].e.car_id, esc, ety, r[k].e.lap, r[k].e.a, r[k].e.b);
        if (p >= bufsz - 256) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    free(b);
    return p;
}

static int ar_build_driver_json(const char *id, int slot, char *buf, int bufsz) {
    int n, p, last = -1, j = 0;
    ar_lap_rec_t *r = ar_load_laps(id, &n);
    for (int k = 0; k < n; k++) if (r[k].slot == slot) last = k;
    char esc[NAME_LEN * 6 + 1] = "", ecn[NAME_LEN * 6 + 1] = "";
    if (last >= 0) { esc_json(esc, sizeof esc, r[last].name); esc_json(ecn, sizeof ecn, r[last].car); }
    p = _snprintf(buf, bufsz, "{\"gen\":%u,\"archive\":\"%s\",\"id\":%d,\"n\":\"%s\",\"cn\":\"%s\",\"p\":%d,\"laps\":[",
                  ar_meta_gen(id), id, slot, esc, ecn, last >= 0 ? r[last].position : 0);
    for (int k = 0; k < n; k++) {
        if (r[k].slot != slot) continue;
        const lap_row_t *q = &r[k].row;
        float s2d = (q->s1 > 0 && q->s2 > q->s1) ? (q->s2 - q->s1) : -1.0f;
        float s3 = (q->t > 0 && q->s2 > 0 && q->t > q->s2) ? (q->t - q->s2) : -1.0f;
        p += _snprintf(buf + p, bufsz - p,
            "%s{\"l\":%d,\"t\":%.3f,\"s1\":%.3f,\"s2\":%.3f,\"s2d\":%.3f,\"s3\":%.3f,"
            "\"p\":%d,\"tc\":%d,\"f\":%.1f,\"pit\":%d,\"lf\":[%.4f,%.4f,%.4f,%.4f]}",
            j++ ? "," : "", q->lap, q->t, q->s1, q->s2, s2d, s3,
            q->pos, q->tire, q->fuel, q->pitted, q->life[0], q->life[1], q->life[2], q->life[3]);
        if (p >= bufsz - 2048) break;
    }
    free(r);
    p += _snprintf(buf + p, bufsz - p, "],\"pits\":[");
    {
        size_t pn;
        char *b = ar_read_file(id, "pits.bin", &pn);
        int cnt = b ? (int)(pn / sizeof(ar_pit_rec_t)) : 0, first = 1;
        const ar_pit_rec_t *pr = (const ar_pit_rec_t *)b;
        for (int k = 0; k < cnt; k++) {
            if (pr[k].slot != slot) continue;
            p = ar_fmt_pit(buf, bufsz, p, &pr[k].e, first, 0);
            first = 0;
            if (p >= bufsz - 512) break;
        }
        free(b);
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    return p;
}

/* Walks traces.bin headers. cb returns nonzero to stop. */
typedef int (*ar_lap_cb)(const ar_lap_hdr_t *h, FILE *f, void *ctx);
static int ar_walk_traces(const char *id, ar_lap_cb cb, void *ctx) {
    char path[MAX_PATH];
    ar_lap_hdr_t h;
    FILE *f;
    int laps = 0;
    if (!ar_path(path, sizeof path, id, "traces.bin")) return -1;
    f = fopen(path, "rb");
    if (!f) return -1;
    while (fread(&h, sizeof h, 1, f) == 1) {
        if (h.magic != AR_TRACE_MAGIC || h.version != AR_TRACE_VERSION || h.samples <= 0 ||
            h.samples > TRACE_SAMPLES || h.sample_bytes != AR_DISK_SAMPLE) break;   /* torn tail */
        long next = ftell(f) + (long)h.samples * h.sample_bytes;
        laps++;
        if (cb(&h, f, ctx)) break;
        if (fseek(f, next, SEEK_SET) != 0) break;
    }
    fclose(f);
    return laps;
}

typedef struct {
    ar_lap_hdr_t meta[MAX_CARS];
    int laps_n[MAX_CARS];
    uint32_t seen[MAX_CARS];
    /* per-slot list of laps, gathered then emitted grouped by slot */
    struct { int32_t slot, lap, flags, samples; float t, start; uint32_t rev; } *rows;
    int nrows, caprows;
} ar_status_ctx;

static int ar_status_cb(const ar_lap_hdr_t *h, FILE *f, void *vctx) {
    ar_status_ctx *c = (ar_status_ctx *)vctx;
    (void)f;
    if (h->slot < 0 || h->slot >= MAX_CARS) return 0;
    if (c->nrows >= c->caprows) {
        int cap = c->caprows ? c->caprows * 2 : 256;
        void *np = realloc(c->rows, (size_t)cap * sizeof *c->rows);
        if (!np) return 1;
        c->rows = np; c->caprows = cap;
    }
    c->rows[c->nrows].slot = h->slot; c->rows[c->nrows].lap = h->lap;
    c->rows[c->nrows].flags = h->flags; c->rows[c->nrows].samples = h->samples;
    c->rows[c->nrows].t = h->lap_time; c->rows[c->nrows].start = h->start_sim;
    c->rows[c->nrows].rev = h->rev;
    c->nrows++;
    c->meta[h->slot] = *h;
    c->laps_n[h->slot]++;
    c->seen[h->slot] |= h->channels_seen & ~(uint32_t)(TRF_RX_SU | TRF_RX_LD | TRF_RX_SL | TRF_RX_FO);
    return 0;
}

static int ar_build_trace_status_json(const char *id, char *buf, int bufsz) {
    ar_status_ctx *c = (ar_status_ctx *)calloc(1, sizeof *c);
    unsigned gen = ar_meta_gen(id);
    int p;
    if (!c) return -1;
    ar_walk_traces(id, ar_status_cb, c);
    float lap_len = 0.0f;
    for (int s = 0; s < MAX_CARS; s++) if (c->laps_n[s] && c->meta[s].lap_len > 0) { lap_len = c->meta[s].lap_len; break; }
    p = _snprintf(buf, bufsz,
        "{\"gen\":%u,\"archive\":\"%s\",\"lap_len\":%.1f,\"laps_max\":%d,\"samples_max\":%d,\"mode\":\"archive\",\"cars\":[",
        gen, id, lap_len, 100000, TRACE_SAMPLES);
    int emitted = 0;
    for (int s = 0; s < MAX_CARS; s++) {
        if (!c->laps_n[s]) continue;
        const ar_lap_hdr_t *m = &c->meta[s];
        char escn[NAME_LEN * 6 + 1], escc[NAME_LEN * 6 + 1];
        esc_json(escn, sizeof escn, m->driver);
        esc_json(escc, sizeof escc, m->car);
        p += _snprintf(buf + p, bufsz - p,
            "%s{\"rev\":%u,\"which\":\"\",\"slot\":%d,\"used\":1,\"auto\":0,\"n\":\"%s\",\"cn\":\"%s\","
            "\"ai\":%d,\"remote\":%d,\"team\":%u,\"dropped_laps\":0,\"open\":0,\"open_lap\":-1,"
            "\"open_samples\":0,\"open_dropped\":0,\"channels_seen\":%u,\"total_samples\":0,\"laps\":[",
            emitted ? "," : "", m->rev, s, escn, escc, m->is_ai, m->is_remote, m->team_id, c->seen[s]);
        emitted++;
        int first = 1;
        for (int k = 0; k < c->nrows; k++) {
            if (c->rows[k].slot != s) continue;
            p += _snprintf(buf + p, bufsz - p,
                "%s{\"lap\":%d,\"t\":%.3f,\"samples\":%d,\"flags\":%d,\"start\":%.3f,"
                "\"lid\":{\"gen\":%u,\"rev\":%u,\"slot\":%d,\"lap\":%d,\"src\":\"archive\"}}",
                first ? "" : ",", c->rows[k].lap, c->rows[k].t, c->rows[k].samples, c->rows[k].flags,
                c->rows[k].start, gen, c->rows[k].rev, s, c->rows[k].lap);
            first = 0;
            if (p >= bufsz - 1024) break;
        }
        p += _snprintf(buf + p, bufsz - p, "],\"unavailable\":[],\"unavailable_why\":{},\"evicted\":[]}");
        if (p >= bufsz - 1024) break;
    }
    p += _snprintf(buf + p, bufsz - p, "]}");
    free(c->rows);
    free(c);
    return p;
}

typedef struct { int slot, lap, found; ar_lap_hdr_t h; trace_sample_t *s; } ar_find_ctx;
static int ar_find_cb(const ar_lap_hdr_t *h, FILE *f, void *vctx) {
    ar_find_ctx *c = (ar_find_ctx *)vctx;
    if (h->slot != c->slot || h->lap != c->lap) return 0;
    /* a later copy of the same lap (same slot, same lap number) wins: keep scanning */
    trace_sample_t *s = (trace_sample_t *)calloc((size_t)h->samples, sizeof *s);
    if (!s) return 1;
    for (int k = 0; k < h->samples; k++) {
        if (fread(&s[k], (size_t)h->sample_bytes, 1, f) != 1) { free(s); return 1; }
        /* rx (RESEARCH) is not archived: its validity bits must not survive */
        s[k].flags &= ~(uint32_t)(TRF_RX_SU | TRF_RX_LD | TRF_RX_SL | TRF_RX_FO);
    }
    free(c->s);
    c->s = s; c->h = *h; c->found = 1;
    return 0;
}

/* /trace/data for an archived lap: the same f1c.trace/1 document as live,
 * source "archive". Returns -1 when the lap is not in the archive. */
static int ar_build_trace_data_json(const char *id, int slot, int lap, char *buf, int bufsz) {
    ar_find_ctx c;
    memset(&c, 0, sizeof c);
    c.slot = slot; c.lap = lap;
    ar_walk_traces(id, ar_find_cb, &c);
    if (!c.found) { free(c.s); return -1; }
    unsigned gen = ar_meta_gen(id);
    char escn[NAME_LEN * 6 + 1], escc[NAME_LEN * 6 + 1];
    esc_json(escn, sizeof escn, c.h.driver);
    esc_json(escc, sizeof escc, c.h.car);
    int p = _snprintf(buf, bufsz,
        "{\"schema\":\"f1c.trace/1\",\"source\":\"archive\",\"archive\":\"%s\",\"open\":false,"
        "\"gen\":%u,\"rev\":%u,\"which\":\"\",\"slot\":%d,"
        "\"driver\":\"%s\",\"car\":\"%s\",\"ai\":%d,\"team\":%u,\"lap\":%d,"
        "\"lap_time\":%.3f,\"start_sim\":%.3f,\"lap_len\":%.1f,\"count\":%d,"
        "\"flags\":%d,\"channels_seen\":%u,\"track\":\"\","
        "\"lid\":{\"gen\":%u,\"rev\":%u,\"slot\":%d,\"lap\":%d,\"src\":\"archive\"},"
        "\"samples\":[",
        id, gen, c.h.rev, slot, escn, escc, c.h.is_ai, c.h.team_id, c.h.lap,
        c.h.lap_time, c.h.start_sim, c.h.lap_len, c.h.samples, c.h.flags, c.h.channels_seen,
        gen, c.h.rev, slot, c.h.lap);
    int truncated = 0;
    for (int i = 0; i < c.h.samples; i++) {
        p = trace_emit_sample_json(buf, bufsz, p, &c.s[i], i == 0);
        if (p >= bufsz - 256) { truncated = 1; break; }
    }
    p += _snprintf(buf + p, bufsz - p, "]%s}", truncated ? ",\"truncated\":true" : "");
    free(c.s);
    return p;
}

/* Live fallback: a lap evicted from the 6-lap memory ring is still on disk in
 * the current session. Returns -1 when not available. */
static int ar_live_fallback_trace(int slot, int lap, char *buf, int bufsz) {
    char cur[AR_ID_LEN];
    int n = -1;
    if (!g_ar_root[0] || slot < 0 || slot >= MAX_CARS) return -1;
    AcquireSRWLockExclusive(&g_ar_lock);
    snprintf(cur, sizeof cur, "%s", g_ar_current_id);
    if (cur[0] && ar_meta_gen(cur) == capture_generation) n = ar_build_trace_data_json(cur, slot, lap, buf, bufsz);
    ReleaseSRWLockExclusive(&g_ar_lock);
    return n;
}

static int ar_health_json(char *buf, int bufsz) {
    char root[MAX_PATH * 6 + 1], cur[AR_ID_LEN];
    AcquireSRWLockExclusive(&g_ar_lock);
    snprintf(cur, sizeof cur, "%s", g_ar_current_id);
    ReleaseSRWLockExclusive(&g_ar_lock);
    esc_json(root, sizeof root, g_ar_root);
    return _snprintf(buf, bufsz,
        "{\"enabled\":%s,\"dir\":\"%s\",\"current\":\"%s\",\"max_mb\":%d,\"dropped\":%u,\"retention_deleted\":%d}",
        g_ar_root[0] ? "true" : "false", root, cur, g_ar_max_mb, g_ar_total_dropped, g_ar_last_retention_deleted);
}
