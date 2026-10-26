/* storage_api.inc: Telemetry 1.1.0 recordings API (STORAGE_MANAGER_SPEC 8.1-8.4, batch 4).
 * Included after storage_jobs.inc. Contents:
 *  - the recordings snapshot (GET /recordings): built by the writer thread on every index write, published under sa_cs;
 *  - request parsing for the three write routes (flat JSON: strings without escapes, integers, booleans, string lists);
 *  - validation and job submission for POST /recordings/load, /archive, /delete (the HTTP guards are in web_telemetry.c);
 *  - the job list (GET /recordings/jobs) and the /health storage object.
 * Rules: no absolute path appears in any response (M11); the file tree is re-checked on disk at request time, so the
 * snapshot is only a display copy; the open session is never touched. Reads of writer state on the HTTP thread are
 * best effort; the authoritative open-part refusal is sj_is_open_part in storage_jobs.inc. */

#define SA_SNAP_CAP  (1u << 18)
#define SA_BODY_MAX  4096u   /* M8 cap for write routes (security review); spec 8.6 said 64 KB, this is stricter */
#define SA_STR_MAX   1024u
#define SA_LIST      8u
#define SA_KEYS      6u

enum { SA_K_NONE = 0, SA_K_WORK, SA_K_ARCH, SA_K_PART, SA_K_DAMAGED, SA_K_PARTIAL };

/* ---------- output buffer (never writes past cap; bad = overflow) ---------- */
typedef struct { char *p; size_t cap, n; int bad; } sa_buf_t;

static void sa_f(sa_buf_t *b, const char *fmt, ...) {
    va_list ap;
    int k;
    if (b->bad) return;
    va_start(ap, fmt);
    k = vsnprintf(b->p + b->n, b->cap - b->n, fmt, ap);
    va_end(ap);
    if (k < 0 || (size_t)k >= b->cap - b->n) { b->bad = 1; b->p[b->n] = 0; return; }
    b->n += (size_t)k;
}

/* JSON string: quote and backslash escaped, control bytes and bytes >= 0x80 become '?'. */
static void sa_s(sa_buf_t *b, const char *s) {
    char tmp[SA_STR_MAX];
    size_t k = 0;
    if (b->bad) return;
    for (; *s && k < sizeof tmp - 3; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { tmp[k++] = '\\'; tmp[k++] = (char)c; }
        else if (c < 0x20 || c >= 0x80) tmp[k++] = '?';
        else tmp[k++] = (char)c;
    }
    tmp[k] = 0;
    sa_f(b, "\"%s\"", tmp);
}

/* {"error":"..."} with the given HTTP status. Resets the buffer first. */
static int sa_err(sa_buf_t *b, int code, const char *msg) {
    b->n = 0;
    b->bad = 0;
    sa_f(b, "{\"error\":");
    sa_s(b, msg);
    sa_f(b, "}");
    return code;
}

static const char *sa_status_line(int code) {
    switch (code) {
    case 200: return "200 OK";
    case 202: return "202 Accepted";
    case 400: return "400 Bad Request";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 409: return "409 Conflict";
    case 413: return "413 Payload Too Large";
    case 415: return "415 Unsupported Media Type";
    case 422: return "422 Unprocessable Entity";
    case 503: return "503 Service Unavailable";
    default:  return "500 Internal Server Error";
    }
}

/* ---------- published snapshot ---------- */
static char sa_build[SA_SNAP_CAP];   /* writer thread only */
static char sa_pub[SA_SNAP_CAP];     /* published copy, read under sa_cs */
static size_t sa_pub_len;
static int sa_pub_over;
static char sa_pub_open[160];
static char sa_pub_alg[16] = "none";
static CRITICAL_SECTION sa_cs;
static int sa_cs_ready;

/* Per-entry classification, writer thread only (rebuilt with the snapshot). */
static int sa_k[SW_ENTRIES];
static int sa_el[SW_ENTRIES];        /* 1 = session file (.f1t, .f1a, or an open .part) */
static char sa_st[SW_ENTRIES][200];  /* stem of each entry */

/* ---------- review ids (R1-A, 2026-10-14; spec 5.1). A v2 session is named by an opaque id, never by its path:
 * "v2_" + 16 lowercase hex digits of FNV-1a 64 over folder + "/" + stem (19 characters). The id table is built with
 * the snapshot (writer thread) and published under sa_cs; the review route resolves ids only through this table. */
#define SA_RID_MAX 256u

/* open = 1: the session is still recording (R1-B). Its id is kept only inside the server, never in GET /recordings,
 * so a review request for it answers 409 "session is still recording" instead of "unknown". */
typedef struct { char id[20], folder[120], file[180]; int open; } sa_rid_t;   /* file: the .f1t name (or .part when open) */

static sa_rid_t sa_rid_build[SA_RID_MAX];   /* writer thread only */
static unsigned sa_rid_build_n;
static sa_rid_t sa_rid_pub[SA_RID_MAX];     /* published copy, read under sa_cs */
static unsigned sa_rid_pub_n;

static void sa_review_id(const char *folder, const char *stem, char *out, size_t cap) {
    uint64_t h = 1469598103934665603ULL;   /* FNV-1a 64 offset basis */
    const char *s;
    for (s = folder; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
    h ^= (unsigned char)'/'; h *= 1099511628211ULL;
    for (s = stem; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
    sw_fmt(out, cap, "v2_%016llx", (unsigned long long)h);
}

static void sa_rid_note(const char *folder, const char *file, const char *id, int open) {
    sa_rid_t *r;
    if (sa_rid_build_n >= SA_RID_MAX) return;   /* table full: that session has no review id (listed, not reviewable) */
    r = &sa_rid_build[sa_rid_build_n++];
    sv2_copy(r->id, sizeof r->id, id);
    sv2_copy(r->folder, sizeof r->folder, folder);
    sv2_copy(r->file, sizeof r->file, file);
    r->open = open;
}

static void sa_init(void) {
    if (!sa_cs_ready) { InitializeCriticalSection(&sa_cs); sa_cs_ready = 1; }
    sv2_copy(sa_pub_alg, sizeof sa_pub_alg, sv2_alg_available(SV2_COMP_XPRESS) ? "xpress" : "none");
}

/* Kind of a file name by suffix; stem receives the name without the suffix ("" when none matches). */
static int sa_kind(const char *file, char *stem, size_t cap) {
    static const struct { const char *suf; int kind; } tab[] = {
        { ".f1t.part.damaged", SA_K_DAMAGED }, { ".f1t.part", SA_K_PART }, { ".f1a.partial", SA_K_PARTIAL },
        { ".f1t", SA_K_WORK }, { ".f1a", SA_K_ARCH } };
    size_t L = strlen(file);
    if (stem && cap) stem[0] = 0;
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        size_t sl = strlen(tab[i].suf);
        if (L > sl && strcmp(file + L - sl, tab[i].suf) == 0) {
            if (stem && cap) {
                size_t k = L - sl;
                if (k >= cap) k = cap - 1;
                memcpy(stem, file, k);
                stem[k] = 0;
            }
            return tab[i].kind;
        }
    }
    return SA_K_NONE;
}

static void sa_classify_all(void) {
    for (unsigned i = 0; i < sw_nentries && i < SW_ENTRIES; i++) {
        sa_k[i] = sa_kind(sw_entries[i].file, sa_st[i], sizeof sa_st[i]);
        sa_el[i] = sa_k[i] == SA_K_WORK || sa_k[i] == SA_K_ARCH ||
                   (sa_k[i] == SA_K_PART && sw_entries[i].state == SW_ST_OPEN);
    }
}

/* 1 when an earlier entry is in the same recording folder (the folder was already listed). */
static int sa_folder_seen(unsigned i) {
    for (unsigned j = 0; j < i; j++) if (!strcmp(sw_entries[j].folder, sw_entries[i].folder)) return 1;
    return 0;
}

/* 1 when an earlier entry is the same session (same folder, session file, same stem). */
static int sa_session_seen(unsigned j) {
    for (unsigned k = 0; k < j; k++) {
        if (!sa_el[k] || strcmp(sw_entries[k].folder, sw_entries[j].folder) || strcmp(sa_st[k], sa_st[j])) continue;
        return 1;
    }
    return 0;
}

/* 1 when the session of entry j has an entry of the given kind (same folder and stem). */
static int sa_session_has(unsigned j, int kind) {
    for (unsigned k = 0; k < sw_nentries; k++) {
        if (!sa_el[k] || strcmp(sw_entries[k].folder, sw_entries[j].folder) || strcmp(sa_st[k], sa_st[j])) continue;
        if (sa_k[k] == kind) return 1;
    }
    return 0;
}

/* One session object (spec 8.3 shape): state from its files. */
static void sa_session_json(sa_buf_t *b, unsigned j, int comma) {
    int open = 0, recovered = 0, work = 0, arch = 0, first = 1;
    double bytes = 0.0;
    unsigned ticks = 0;
    const char *state, *wfile = "", *pfile = "";
    char rid[24] = "";
    for (unsigned k = 0; k < sw_nentries; k++) {
        if (!sa_el[k] || strcmp(sw_entries[k].folder, sw_entries[j].folder) || strcmp(sa_st[k], sa_st[j])) continue;
        bytes += sw_entries[k].bytes;
        if (sa_k[k] == SA_K_PART) { open = 1; ticks = sw_entries[k].ticks; pfile = sw_entries[k].file; }
        if (sa_k[k] == SA_K_WORK) { work = 1; wfile = sw_entries[k].file; ticks = sw_entries[k].ticks; if (sw_entries[k].state == SW_ST_RECOVERED) recovered = 1; }
        if (sa_k[k] == SA_K_ARCH) arch = 1;
    }
    state = open ? "open" : recovered ? "recovered" : work ? "closed" : "archived";
    if (work && !open) {   /* closed or recovered: the only sessions that carry review_id in the listing (spec 5.1) */
        sa_review_id(sw_entries[j].folder, sa_st[j], rid, sizeof rid);
        sa_rid_note(sw_entries[j].folder, wfile, rid, 0);
    }
    if (open && pfile[0]) {   /* R1-B: internal id only, so the review route can answer 409 (not in the JSON) */
        char oid[24];
        sa_review_id(sw_entries[j].folder, sa_st[j], oid, sizeof oid);
        sa_rid_note(sw_entries[j].folder, pfile, oid, 1);
    }
    if (comma) sa_f(b, ",");
    sa_f(b, "{\"id\":");
    sa_s(b, sa_st[j]);
    if (rid[0]) sa_f(b, ",\"review_id\":\"%s\"", rid);
    sa_f(b, ",\"state\":\"%s\",\"archived\":%s,\"bytes\":%.0f,\"ticks\":%u,\"files\":[", state, arch ? "true" : "false", bytes, ticks);
    for (unsigned k = 0; k < sw_nentries; k++) {
        const char *fmt;
        if (!sa_el[k] || strcmp(sw_entries[k].folder, sw_entries[j].folder) || strcmp(sa_st[k], sa_st[j])) continue;
        fmt = sa_k[k] == SA_K_PART ? "open" : sa_k[k] == SA_K_ARCH ? "archived" : "working";
        sa_f(b, "%s{\"name\":", first ? "" : ",");
        first = 0;
        sa_s(b, sw_entries[k].file);
        sa_f(b, ",\"format\":\"%s\",\"bytes\":%.0f}", fmt, sw_entries[k].bytes);
    }
    sa_f(b, "]}");
}

/* Size text for the notice: "304 MB", "1.20 GB". */
static void sa_size_text(double bytes, char *out, size_t cap) {
    if (bytes >= 1073741824.0) sw_fmt(out, cap, "%.2f GB", bytes / 1073741824.0);
    else if (bytes >= 1048576.0) sw_fmt(out, cap, "%.0f MB", bytes / 1048576.0);
    else if (bytes >= 1024.0) sw_fmt(out, cap, "%.0f KB", bytes / 1024.0);
    else sw_fmt(out, cap, "%.0f B", bytes);
}

/* Rebuilds the snapshot text from the entry table (writer thread, with every index write). */
static void sa_snapshot_rebuild(void) {
    sa_buf_t b;
    double total;
    int over;
    char size_b[32], notice[480], warn_b[480], open_name[160] = "";
    unsigned nrec = 0;
    if (!sw_enabled) return;
    if (!sa_cs_ready) sa_init();
    sa_classify_all();
    sa_rid_build_n = 0;
    total = sw_total_bytes();
    over = total >= SW_WARN_BYTES;
    sa_size_text(total, size_b, sizeof size_b);
    sw_fmt(notice, sizeof notice, "Recordings are kept until you delete them. Total size now: %s. Warning level: 1 GB (warning only). Nothing is deleted automatically. Deleting moves files to the Windows Recycle Bin after you confirm.", size_b);
    sw_fmt(warn_b, sizeof warn_b, "The recordings folder is over 1 GB (now %s). This is a warning only: nothing is deleted automatically. Delete the recordings you no longer need from this page.", size_b);
    b.p = sa_build;
    b.cap = sizeof sa_build;
    b.n = 0;
    b.bad = 0;
    sa_f(&b, "{\"schema\":\"f1c.recordings/1\",\"enabled\":true,\"total_bytes\":%.0f,\"warn_bytes\":%.0f,\"over_warn\":%s,\"notice\":",
         total, SW_WARN_BYTES, over ? "true" : "false");
    sa_s(&b, notice);
    sa_f(&b, ",\"warning\":");
    if (over) sa_s(&b, warn_b); else sa_f(&b, "null");
    if (sw_wr.have) {
        sw_fmt(open_name, sizeof open_name, "%s", sw_wr.stem);
        sa_f(&b, ",\"recording\":{\"session\":");
        sa_s(&b, sw_wr.stem);
        sa_f(&b, ",\"folder\":");
        sa_s(&b, sw_wr.folder_name);
        sa_f(&b, ",\"state\":\"open\",\"ticks\":%u,\"bytes\":%.0f},", (unsigned)sw_wr.total_ticks, sw_wr.bytes);
    } else {
        sa_f(&b, ",\"recording\":null,");
    }
    sa_f(&b, "\"recordings\":[");
    for (unsigned i = 0; i < sw_nentries; i++) {
        const char *fold = sw_entries[i].folder;
        double rb = 0.0;
        unsigned sess = 0, arch = 0, nd = 0;
        if (sa_folder_seen(i)) continue;
        for (unsigned j = 0; j < sw_nentries; j++) if (!strcmp(sw_entries[j].folder, fold)) rb += sw_entries[j].bytes;
        for (unsigned j = 0; j < sw_nentries; j++) {
            if (!sa_el[j] || strcmp(sw_entries[j].folder, fold) || sa_session_seen(j)) continue;
            sess++;
            if (sa_session_has(j, SA_K_ARCH)) arch++;
        }
        sa_f(&b, "%s{\"id\":", nrec++ ? "," : "");
        sa_s(&b, fold);
        sa_f(&b, ",\"weekend\":");
        sa_s(&b, sw_entries[i].weekend);
        sa_f(&b, ",\"bytes\":%.0f,\"sessions_count\":%u,\"archived_count\":%u,\"sessions\":[", rb, sess, arch);
        nd = 0;
        for (unsigned j = 0; j < sw_nentries; j++) {
            if (!sa_el[j] || strcmp(sw_entries[j].folder, fold) || sa_session_seen(j)) continue;
            sa_session_json(&b, j, nd++ ? 1 : 0);
        }
        sa_f(&b, "],\"damaged\":[");
        nd = 0;
        for (unsigned j = 0; j < sw_nentries; j++) {
            if (strcmp(sw_entries[j].folder, fold)) continue;
            if (!(sa_k[j] == SA_K_DAMAGED || (sa_k[j] == SA_K_PART && sw_entries[j].state != SW_ST_OPEN))) continue;
            sa_f(&b, "%s{\"file\":", nd++ ? "," : "");
            sa_s(&b, sw_entries[j].file);
            sa_f(&b, ",\"bytes\":%.0f,\"format\":\"damaged\"}", sw_entries[j].bytes);
        }
        sa_f(&b, "],\"partial\":[");
        nd = 0;
        for (unsigned j = 0; j < sw_nentries; j++) {
            if (strcmp(sw_entries[j].folder, fold) || sa_k[j] != SA_K_PARTIAL) continue;
            sa_f(&b, "%s{\"file\":", nd++ ? "," : "");
            sa_s(&b, sw_entries[j].file);
            sa_f(&b, ",\"bytes\":%.0f,\"format\":\"archive_interrupted\"}", sw_entries[j].bytes);
        }
        sa_f(&b, "]}");
    }
    sa_f(&b, "],\"ring_drops\":%ld,", (long)InterlockedCompareExchange(&sw_ring.dropped, 0, 0));
    if (b.bad) return;   /* too large: keep the published copy */
    EnterCriticalSection(&sa_cs);
    memcpy(sa_pub, sa_build, b.n);
    sa_pub_len = b.n;
    sa_rid_pub_n = sa_rid_build_n;
    memcpy(sa_rid_pub, sa_rid_build, (size_t)sa_rid_build_n * sizeof sa_rid_build[0]);
    sa_pub_over = over;
    sv2_copy(sa_pub_open, sizeof sa_pub_open, open_name);
    LeaveCriticalSection(&sa_cs);
}

/* ---------- GET /recordings ---------- */
static int sa_loaded_copy(char *out, size_t cap) {
    out[0] = 0;
    if (!sj_cs_ready) return 0;
    EnterCriticalSection(&sj_cs);
    sv2_copy(out, cap, sj_loaded);
    LeaveCriticalSection(&sj_cs);
    return out[0] != 0;
}

static int sa_api_recordings(char *out, size_t cap, size_t *len) {
    sa_buf_t b = { out, cap, 0, 0 };
    char loaded[160];
    int has_loaded;
    if (!sa_cs_ready) sa_init();
    EnterCriticalSection(&sa_cs);
    if (sa_pub_len) {
        if (sa_pub_len + 256 > cap) {
            LeaveCriticalSection(&sa_cs);
            sa_err(&b, 500, "The recordings list is too large for one response.");
            *len = b.n;
            return 500;
        }
        memcpy(out, sa_pub, sa_pub_len);
        b.n = sa_pub_len;
    } else {
        sa_f(&b, "{\"schema\":\"f1c.recordings/1\",\"enabled\":%s,\"total_bytes\":0,\"warn_bytes\":%.0f,\"over_warn\":false,"
                 "\"notice\":", sw_enabled ? "true" : "false", SW_WARN_BYTES);
        sa_s(&b, "Recordings are kept until you delete them. Nothing is deleted automatically.");
        sa_f(&b, ",\"warning\":null,\"recording\":null,\"recordings\":[],\"ring_drops\":0,");
    }
    LeaveCriticalSection(&sa_cs);
    has_loaded = sa_loaded_copy(loaded, sizeof loaded);
    sa_f(&b, "\"loaded\":");
    if (has_loaded) sa_s(&b, loaded); else sa_f(&b, "null");
    sa_f(&b, "}");
    *len = b.n;
    return b.bad ? 500 : 200;
}

/* ---------- GET /recordings/jobs ---------- */
static int sa_api_jobs(char *out, size_t cap, size_t *len) {
    sa_buf_t b = { out, cap, 0, 0 };
    unsigned shown = 0;
    sa_f(&b, "{\"schema\":\"f1c.recordings.jobs/1\",\"jobs\":[");
    if (sj_cs_ready) EnterCriticalSection(&sj_cs);
    for (unsigned i = 0; i < SJ_QUEUE_N; i++) {
        sj_job_t *j = &sj_jobs[i];
        const char *state = j->state == 0 ? "queued" : j->state == 1 ? "running" : "done";
        const char *result = j->state != 2 ? "pending" : j->result == SJ_OK ? "ok" : "error";
        if (!j->used) continue;
        sa_f(&b, "%s{\"job\":\"%s-%u\",\"kind\":\"%s\",\"state\":\"%s\",\"result\":\"%s\",\"message\":",
             shown++ ? "," : "", j->kind == SJ_JOB_ARCHIVE ? "archive" : "delete", j->seq,
             j->kind == SJ_JOB_ARCHIVE ? "archive" : "delete", state, result);
        sa_s(&b, j->msg);
        sa_f(&b, ",\"items\":%u,\"recycle_working\":%s,\"bytes_working\":%.0f,\"bytes_archived\":%.0f,\"bytes_moved\":%.0f,\"moved\":[",
             j->n, j->recycle_working ? "true" : "false", j->bytes_working, j->bytes_archived, j->bytes_moved);
        for (unsigned k = 0; k < j->nmoved; k++) { if (k) sa_f(&b, ","); sa_s(&b, j->moved[k]); }
        sa_f(&b, "],\"not_moved\":[");
        for (unsigned k = 0; k < j->nleft; k++) { if (k) sa_f(&b, ","); sa_s(&b, j->left[k]); }
        sa_f(&b, "]}");
    }
    if (sj_cs_ready) LeaveCriticalSection(&sj_cs);
    sa_f(&b, "]}");
    *len = b.n;
    return b.bad ? 500 : 200;
}

/* ---------- request body: flat JSON object (strings without escapes, digits, booleans, lists of strings) ---------- */
typedef struct {
    int n;
    char key[SA_KEYS][24];
    int type[SA_KEYS];                      /* 1 string, 2 number, 3 boolean, 4 string list */
    char str[SA_KEYS][SJ_NAME];
    double num[SA_KEYS];
    int flag[SA_KEYS];
    unsigned cnt[SA_KEYS];
    char list[SA_KEYS][SA_LIST][SJ_NAME];
} sa_obj_t;
static sa_obj_t sa_req;   /* HTTP thread only */

static const char *sa_ws(const char *p, const char *e) {
    while (p < e && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    return p;
}

/* String without escapes and without bytes >= 0x80. Returns the position after the closing quote, or NULL. */
static const char *sa_jstr(const char *p, const char *e, char *out, size_t cap) {
    size_t k = 0;
    if (p >= e || *p != '"') return NULL;
    for (p++; p < e && *p != '"'; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c < 0x20 || c >= 0x80 || k + 1 >= cap) return NULL;
        out[k++] = (char)c;
    }
    if (p >= e) return NULL;
    out[k] = 0;
    return p + 1;
}

static int sa_parse(const char *body, size_t n) {
    const char *p = body, *e = body + n;
    memset(&sa_req, 0, sizeof sa_req);
    p = sa_ws(p, e);
    if (p >= e || *p != '{') return 0;
    p = sa_ws(p + 1, e);
    if (p < e && *p == '}') return sa_ws(p + 1, e) == e;
    for (;;) {
        char key[24];
        unsigned i = (unsigned)sa_req.n;
        p = sa_jstr(sa_ws(p, e), e, key, sizeof key);
        if (!p || i >= SA_KEYS) return 0;
        p = sa_ws(p, e);
        if (p >= e || *p != ':') return 0;
        p = sa_ws(p + 1, e);
        sv2_copy(sa_req.key[i], sizeof sa_req.key[i], key);
        if (p < e && *p == '"') {
            p = sa_jstr(p, e, sa_req.str[i], sizeof sa_req.str[i]);
            if (!p) return 0;
            sa_req.type[i] = 1;
        } else if (p < e && *p == '[') {
            unsigned c = 0;
            p = sa_ws(p + 1, e);
            if (p < e && *p == ']') {
                p++;
            } else {
                for (;;) {
                    if (c >= SA_LIST) return 0;
                    p = sa_jstr(sa_ws(p, e), e, sa_req.list[i][c], sizeof sa_req.list[i][c]);
                    if (!p) return 0;
                    c++;
                    p = sa_ws(p, e);
                    if (p < e && *p == ',') { p++; continue; }
                    if (p < e && *p == ']') { p++; break; }
                    return 0;
                }
            }
            sa_req.type[i] = 4;
            sa_req.cnt[i] = c;
        } else if (e - p >= 4 && !strncmp(p, "true", 4)) {
            sa_req.type[i] = 3; sa_req.flag[i] = 1; p += 4;
        } else if (e - p >= 5 && !strncmp(p, "false", 5)) {
            sa_req.type[i] = 3; sa_req.flag[i] = 0; p += 5;
        } else if (p < e && *p >= '0' && *p <= '9') {
            double v = 0.0;
            unsigned digits = 0;
            while (p < e && *p >= '0' && *p <= '9') {
                if (++digits > 15) return 0;
                v = v * 10.0 + (double)(*p - '0');
                p++;
            }
            sa_req.type[i] = 2;
            sa_req.num[i] = v;
        } else {
            return 0;
        }
        sa_req.n++;
        p = sa_ws(p, e);
        if (p < e && *p == ',') { p = sa_ws(p + 1, e); continue; }
        if (p < e && *p == '}') return sa_ws(p + 1, e) == e;
        return 0;
    }
}

static int sa_find(const char *key) {
    for (int i = 0; i < sa_req.n; i++) if (!strcmp(sa_req.key[i], key)) return i;
    return -1;
}

/* Recording and file names: no separators, no drive or stream syntax, no "..", printable ASCII. */
static int sa_name_ok(const char *s) {
    size_t L = strlen(s);
    if (L == 0 || L > 119 || strstr(s, "..") || strcmp(s, ".") == 0) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < 0x20 || *p >= 0x80 || strchr("\\/:*?\"<>|", *p)) return 0;
    }
    return 1;
}

static void sa_folder_path(char *out, size_t cap, const char *folder) { sw_fmt(out, cap, "%s%s", sw_rec_dir, folder); }

static int sa_folder_exists(const char *folder) {
    char p[SW_PATHBUF];
    sa_folder_path(p, sizeof p, folder);
    return sw_is_dir(p);
}

/* Best effort on the HTTP thread (see the file header). */
static int sa_open_session(const char *folder, const char *stem) {
    return sw_wr.have && _stricmp(folder, sw_wr.folder_name) == 0 && (!stem || strcmp(stem, sw_wr.stem) == 0);
}

static int sa_is_loaded(const char *folder) {
    int r = 0;
    if (sj_cs_ready) EnterCriticalSection(&sj_cs);
    r = sj_loaded[0] && _stricmp(sj_loaded, folder) == 0;
    if (sj_cs_ready) LeaveCriticalSection(&sj_cs);
    return r;
}

typedef struct { char name[SJ_NAME]; double bytes; } sa_file_t;

/* Files of one recording folder (names and sizes). Returns the count, or -1 when missing, too many or a name is too long. */
static int sa_list_folder(const char *folder, sa_file_t *out, unsigned cap) {
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pat[SW_PATHBUF];
    int n = 0;
    sw_fmt(pat, sizeof pat, "%s%s\\*", sw_rec_dir, folder);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if ((unsigned)n >= cap || strlen(fd.cFileName) >= SJ_NAME) { FindClose(h); return -1; }
        sv2_copy(out[n].name, sizeof out[n].name, fd.cFileName);
        out[n].bytes = (double)fd.nFileSizeHigh * 4294967296.0 + (double)fd.nFileSizeLow;
        n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}

static int sa_file_exists(const char *folder, const char *name) {
    char p[SW_PATHBUF];
    sw_fmt(p, sizeof p, "%s%s\\%s", sw_rec_dir, folder, name);
    return sw_path_exists(p);
}

/* ---------- GET /recordings/review (R1-A, 2026-10-14; spec 5.3, 7) ----------
 * One review index per session; at most SA_REV_SLOTS kept (LRU). One build runs at a time on a worker thread that reads
 * the closed .f1t through sv2_index_build_rd (one block in memory at a time, never the whole file). The HTTP thread never
 * reads the file here: it copies the slot state out under rv_cs. Deviation (R1A record): the spec puts this build on
 * storage_jobs.inc; that file is outside the R1-A edit list, so the build runs on a short-lived thread of its own. */
#define SA_REV_SLOTS  2u
#define SA_REV_BLOCKS 4096u
#define SA_REV_LAPS   2048u
enum { SA_REV_NONE = 0, SA_REV_BUILDING, SA_REV_READY, SA_REV_ERROR };

typedef struct {
    int used, state, reason;
    char id[20], folder[120], file[180], path[SW_PATHBUF];
    uint32_t nlaps, ncars;
    DWORD built_ms;
    uint64_t use;                       /* LRU stamp (rv_clock) */
    sv2_idx_block_t blocks[SA_REV_BLOCKS];
    sv2_idx_lap_t laps[SA_REV_LAPS];
    sv2_idx_t ix;                       /* worker-owned until READY or ERROR */
} sa_rev_slot_t;

static sa_rev_slot_t sa_rev[SA_REV_SLOTS];
static CRITICAL_SECTION rv_cs;
static int rv_cs_ready;
static int rv_busy;                     /* 1 while the worker runs (rv_cs) */
static uint64_t rv_clock;

/* Reads exactly n bytes at off. Never more than one block (SV2_MAX_PAYLOAD + 40 B) per call. */
static int sa_rev_read_at(void *ctx, uint64_t off, uint8_t *dst, size_t n) {
    HANDLE f = *(HANDLE *)ctx;
    LARGE_INTEGER pos;
    size_t done = 0;
    if (n > SV2_MAX_PAYLOAD + SV2_BLOCK_HEADER_BYTES) return -1;
    pos.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx(f, pos, NULL, FILE_BEGIN)) return -1;
    while (done < n) {
        DWORD got = 0;
        size_t left = n - done;
        DWORD want = left > 65536u ? 65536u : (DWORD)left;
        if (!ReadFile(f, dst + done, want, &got, NULL) || got == 0) return -1;
        done += got;
    }
    return 0;
}

static const char *sa_rev_reason_name(int r) {
    switch (r) {
    case SV2_R_HEADER: return "SV2_R_HEADER";
    case SV2_R_TABLE: return "SV2_R_TABLE";
    case SV2_R_BLOCK_HEADER: return "SV2_R_BLOCK_HEADER";
    case SV2_R_BLOCK_CRC: return "SV2_R_BLOCK_CRC";
    case SV2_R_BLOCK_DECODE: return "SV2_R_BLOCK_DECODE";
    case SV2_R_DECODER_UNAVAILABLE: return "SV2_R_DECODER_UNAVAILABLE";
    case SV2_R_VERSION: return "SV2_R_VERSION";
    case SV2_IDX_R_CAPACITY: return "SV2_IDX_R_CAPACITY";
    default: return "SV2_R_OTHER";
    }
}

static DWORD WINAPI sa_rev_worker(LPVOID arg) {
    sa_rev_slot_t *sl = &sa_rev[(unsigned)(uintptr_t)arg];
    uint8_t *stored = (uint8_t *)malloc(SV2_MAX_PAYLOAD);
    uint8_t *work = (uint8_t *)malloc(SV2_MAX_PAYLOAD);
    /* Each decoded value takes at least one payload byte, so SV2_MAX_PAYLOAD words always fit one block. */
    uint32_t *pool = (uint32_t *)malloc(SV2_MAX_PAYLOAD * sizeof(uint32_t));
    sv2_data_t d;
    sv2_rd_t rd;
    HANDLE f = INVALID_HANDLE_VALUE;
    LARGE_INTEGER sz;
    int r = -1;
    unsigned cars = 0;
    DWORD t0 = GetTickCount();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    sv2_idx_init(&sl->ix, sl->blocks, SA_REV_BLOCKS, sl->laps, SA_REV_LAPS);
    if (stored && work && pool) {
        f = CreateFileA(sl->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE || !GetFileSizeEx(f, &sz)) {
            r = -1;   /* R1-C: the file could not be opened or sized; not a header fault (was SV2_R_HEADER). -1 maps to SV2_R_OTHER */
        } else {
            rd.read_at = sa_rev_read_at;
            rd.ctx = &f;
            rd.len = (uint64_t)sz.QuadPart;
            memset(&d, 0, sizeof d);
            d.pool = pool;
            d.pool_cap = SV2_MAX_PAYLOAD;
            r = sv2_index_build_rd(&rd, &d, stored, work, SV2_MAX_PAYLOAD, &sl->ix);
        }
    }
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    free(stored);
    free(work);
    free(pool);
    for (unsigned c = 0; c < SV2_MAX_CARS; c++) if (sl->ix.car[c].seen) cars++;
    EnterCriticalSection(&rv_cs);
    sl->reason = r;
    sl->nlaps = sl->ix.nlaps;
    sl->ncars = cars;
    sl->built_ms = GetTickCount() - t0;
    /* A valid prefix (blocks indexed before a bad one) is READY with its reason; nothing usable is ERROR. */
    sl->state = (r == SV2_OK || sl->ix.nblocks > 0) ? SA_REV_READY : SA_REV_ERROR;
    rv_busy = 0;
    LeaveCriticalSection(&rv_cs);
    return 0;
}

/* Starts the build of slot s. rv_cs is held. */
static void sa_rev_start_locked(unsigned s) {
    HANDLE th;
    sa_rev[s].state = SA_REV_BUILDING;
    rv_busy = 1;
    th = CreateThread(NULL, 0, sa_rev_worker, (LPVOID)(uintptr_t)s, 0, NULL);
    if (!th) { sa_rev[s].state = SA_REV_ERROR; sa_rev[s].reason = -1; rv_busy = 0; return; }
    CloseHandle(th);
}

/* A review id is "v2_" + 16 lowercase hex digits (spec 5.1). Anything else is refused before any lookup. */
static int sa_rid_id_ok(const char *id) {
    size_t L = strlen(id);
    if (L != 19 || strncmp(id, "v2_", 3) != 0) return 0;
    for (size_t i = 3; i < L; i++)
        if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return 0;
    return 1;
}

/* Resolves a review id through the published id table (sa_cs). Returns 1 and the folder and .f1t name when found. */
static int sa_rid_find(const char *id, char *folder, size_t fcap, char *file, size_t flcap, int *is_open) {
    int found = 0;
    EnterCriticalSection(&sa_cs);
    for (unsigned i = 0; i < sa_rid_pub_n && !found; i++) {
        if (strcmp(sa_rid_pub[i].id, id)) continue;
        sv2_copy(folder, fcap, sa_rid_pub[i].folder);
        sv2_copy(file, flcap, sa_rid_pub[i].file);
        *is_open = sa_rid_pub[i].open;
        found = 1;
    }
    LeaveCriticalSection(&sa_cs);
    return found;
}

/* Index slot for a review id (rv_cs held). Returns the slot, or -1 when the id is not cached and another build runs.
 * A missing id starts its build here (state BUILDING, LRU slot reused). */
static int sa_rev_pick_locked(const char *id, const char *folder, const char *file, const char *path) {
    int slot = -1;
    for (unsigned s = 0; s < SA_REV_SLOTS; s++)
        if (sa_rev[s].used && !strcmp(sa_rev[s].id, id)) { slot = (int)s; break; }
    if (slot >= 0) {
        sa_rev[slot].use = ++rv_clock;
        return slot;
    }
    if (rv_busy) return -1;
    {
        unsigned pick = 0;
        int have_free = 0;
        for (unsigned s = 0; s < SA_REV_SLOTS && !have_free; s++)
            if (!sa_rev[s].used) { pick = s; have_free = 1; }
        if (!have_free)
            for (unsigned s = 1; s < SA_REV_SLOTS; s++)
                if (sa_rev[s].use < sa_rev[pick].use) pick = s;   /* least recently used; none is building */
        sa_rev[pick].used = 1;
        sv2_copy(sa_rev[pick].id, sizeof sa_rev[pick].id, id);
        sv2_copy(sa_rev[pick].folder, sizeof sa_rev[pick].folder, folder);
        sv2_copy(sa_rev[pick].file, sizeof sa_rev[pick].file, file);
        sv2_copy(sa_rev[pick].path, sizeof sa_rev[pick].path, path);
        sa_rev[pick].reason = SV2_OK;
        sa_rev[pick].nlaps = sa_rev[pick].ncars = 0;
        sa_rev[pick].built_ms = 0;
        sa_rev[pick].use = ++rv_clock;
        sa_rev_start_locked(pick);
        slot = (int)pick;
    }
    return slot;
}

/* GET /recordings/review?session=<review_id>: the review index state (spec 5.3). Starts a build when the worker is idle.
 * While another session is being built, an uncached id answers state "none" and the viewer asks again. */
static int sa_api_review(const char *id, char *out, size_t cap, size_t *len) {
    sa_buf_t b = { out, cap, 0, 0 };
    char folder[120], file[180], path[SW_PATHBUF];
    int state = SA_REV_NONE, reason = SV2_OK, slot = -1, is_open = 0;
    uint32_t nlaps = 0, ncars = 0;
    DWORD built = 0;
    if (!sa_cs_ready) sa_init();
    if (!rv_cs_ready) { InitializeCriticalSection(&rv_cs); rv_cs_ready = 1; }   /* HTTP thread only */
    if (!sa_rid_id_ok(id)) { sa_err(&b, 404, "unknown or deleted session"); *len = b.n; return 404; }   /* R1-C: contract 17 says 404 for a non-hex id (was 400) */
    if (!sa_rid_find(id, folder, sizeof folder, file, sizeof file, &is_open) || !sa_file_exists(folder, file)) {
        sa_err(&b, 404, "unknown or deleted session");
        *len = b.n;
        return 404;
    }
    if (is_open) { sa_err(&b, 409, "session is still recording"); *len = b.n; return 409; }
    sw_fmt(path, sizeof path, "%s%s\\%s", sw_rec_dir, folder, file);
    EnterCriticalSection(&rv_cs);
    slot = sa_rev_pick_locked(id, folder, file, path);
    if (slot >= 0) {
        state = sa_rev[slot].state;
        reason = sa_rev[slot].reason;
        nlaps = sa_rev[slot].nlaps;
        ncars = sa_rev[slot].ncars;
        built = sa_rev[slot].built_ms;
    }
    LeaveCriticalSection(&rv_cs);
    sa_f(&b, "{\"schema\":\"f1c.recordings.review/1\",\"session\":");
    sa_s(&b, id);
    sa_f(&b, ",\"state\":\"%s\",\"laps\":%u,\"cars\":%u,\"lap_len\":null,\"reason\":",
         state == SA_REV_BUILDING ? "building" : state == SA_REV_READY ? "ready" :
         state == SA_REV_ERROR ? "error" : "none", nlaps, ncars);
    if ((state == SA_REV_READY || state == SA_REV_ERROR) && reason != SV2_OK) sa_s(&b, sa_rev_reason_name(reason));
    else sa_f(&b, "null");
    sa_f(&b, ",\"built_ms\":%lu}", (unsigned long)built);
    *len = b.n;
    return b.bad ? 500 : 200;
}

/* ---------- R1-B: review adapter for recorded (v2) sessions (2026-10-14; spec 5.3, 5.5; map 3.5) ----------
 * The 1.0.1 review routes answer a v2 review id from the review index (sa_rev) and the closed .f1t, one block at a
 * time (sa_rev_read_at). Not recorded (404): pits and incidents (the index keeps lap events only, map section 0),
 * weather, track, driver. A channel the file does not store is null (trace) or 0 with its f bit clear. Names are
 * "Car <slot>": v2 has no roster. Lap times are the game lap times of the index (B2 rule); a lap without a game time
 * or without a start (the first partial lap) is not listed. Builders take a READY slot and a reader, so the runtime
 * tests run them on a memory copy of the file. */
#define SA_RV_POS_BLOCKS 64u   /* newest blocks read for each car's last race position and AI flag */
#define SA_RV_CH_POS     68u   /* writer channel: race position (RAW32) */
#define SA_RV_CH_LAP     SV2_CH_LAP   /* writer channel: lap number (RAW32) */
#define SA_RV_CAR_BASE   1000  /* 1.0.1 convention: /trace/data?car=1000+slot */

/* Name table of one review id (TAPE2 N1, 2026-10-15). Per car, the first rule that applies: the newest driver EVENT (name and
 * team); the season .veh name of its race number (season year from the recording stem, see vehn_stem_year); "#num" (race
 * number, channel 97); "Car N". team is "" unless the first two rules give one. Every review doc uses this one table. */
#define SA_NM_CAR   0u
#define SA_NM_NUM   1u
#define SA_NM_VEH   2u
#define SA_NM_EVENT 3u
typedef struct { char name[NAME_LEN + 16], team[NAME_LEN]; int32_t num; uint8_t src; } sa_rv_nm_t;
static void sa_rv_nm_name(const sa_rv_nm_t *nmt, uint32_t slot, char *out, size_t cap) {
    if (slot >= SV2_MAX_CARS) snprintf(out, cap, "Car %u", (unsigned)slot);
    else snprintf(out, cap, "%s", nmt[slot].name);
}

/* Sentinel fields of the synthesized /telemetry car and frame (schemas/telemetry.json, API contract section 3). */
static const char sa_rv_car_rest[] =
    ",\"s\":0,\"r\":0,\"mr\":0,\"rpm\":0,\"max_rpm\":0,\"g\":0,\"bl\":0,\"ll\":0,\"tt\":0,\"tp\":0,"
    "\"bs1\":0,\"bs2\":0,\"ls1\":0,\"ls2\":0,\"fs\":0,\"a\":0,\"ot2\":0,\"sgi\":0,\"yf\":0,\"pt\":0,"
    "\"tc\":0,\"th\":0,\"br\":0,\"rt\":0,\"cs\":0,\"px\":0,\"pz\":0,\"mx\":0,\"mz\":0,\"dx\":0,"
    "\"dy\":0,\"pn\":0,\"cb\":0,\"pt0\":0,\"gar_team\":0,\"gar_bay\":0,\"et\":0,\"ms\":0,\"ds\":0,"
    "\"gp\":0,\"rc\":0,\"qt\":0,\"lr\":0,\"cc\":0,\"tl\":0,\"fc\":0,\"hr\":0,\"rl\":0,\"hd\":0,"
    "\"dd\":0,\"tp2\":0,\"p1\":0,\"p2\":0,\"lp\":0,\"ps\":0,\"nf\":0,\"pe0\":0,\"pe1\":0,\"pe2\":0,"
    "\"wfl\":0,\"wfr\":0,\"wrl\":0,\"wrr\":0,\"nx\":0,\"nl\":0,\"nc\":0,\"ut\":0,\"cn\":\"\","
    "\"sc\":0,\"ap\":0,\"dt\":0,\"fw\":0,\"rw\":0,\"dm\":0,\"dc\":0,\"dpr\":0,\"u4574\":0,\"ed\":0,"
    "\"eav\":0,\"ot\":0,\"wt\":0,\"fl\":0,\"lfe\":0,\"lfb\":0,\"lfg\":0,\"lfs\":0,\"lpe\":0,"
    "\"lpb\":0,\"lpg\":0,\"lps\":0,\"wg\":0,\"ga\":0,\"gb\":0,\"gai\":-1,\"gbi\":-1,\"lsd\":0,"
    "\"lsn\":0,\"boxsoon\":0,\"boxwhy\":\"\",\"topl\":0,\"toppl\":0,\"tops\":0,\"favg\":0,\"faut\":0,"
    "\"cw\":-1,\"vf\":0,\"steer\":0,\"steerwhy\":\"\",\"glat\":0,\"glon\":0,\"tid\":0,\"cr\":0,"
    "\"secel\":0,\"lspb\":0,\"lspn\":0,\"hlost\":0,\"vf6\":0,\"gv\":0,\"yaw\":0,\"ttc\":[-999,-999,"
    "-999,-999],\"tcc\":[-999,-999,-999,-999],\"rx\":[-999,-999,-999,-999,-999,-999,-999,-999,-999,"
    "-999,-999,-999,-999,-999,-999,-999],\"abt\":[-999,-999],\"aws\":[-999,-999,-999,-999]}";

static const char sa_rv_frame_rest[] =
    "\"sample_sequence\":0,\"sample_age_ms\":0,\"session_generation\":0,\"ph\":0,\"ac\":0,\"acr\":0,"
    "\"tcf\":0,\"tgf\":0,\"tw\":0,\"tl\":0,\"vc\":0,\"cm\":0,\"rm\":0,\"pi\":0,\"ps\":0,\"ai\":0,"
    "\"rt\":0,\"st\":0,\"tk\":0,\"wc\":0,\"at\":0,\"trkt\":0,\"onw\":0,\"offw\":0,\"sg\":\"\","
    "\"sgn\":0,\"sgl\":0,\"trkl\":0,\"yc\":0,\"btmp\":[0,0,0,0],\"ttmp\":[[0,0,0],[0,0,0],[0,0,0],[0,"
    "0,0]],\"trd\":[0,0,0,0],\"bls\":[0,0,0,0],\"otp\":0,\"wtp\":0,\"pel\":0,"
    "\"pch\":{\"rh\":[-999,-999,-999,-999],\"sus\":[-999,-999,-999,-999],\"tdf\":[-999,-999,-999,-999],"
    "\"pr\":[-999,-999,-999,-999],\"prs\":[-999,-999,-999,-999],\"gr\":[-999,-999,-999,-999],"
    "\"opt\":[-999,-999,-999,-999],\"cam\":[-999,-999,-999,-999],\"ws\":[-999,-999,-999,-999],"
    "\"roll\":[-999,-999,-999,-999],\"disc\":[-999,-999,-999,-999],\"discf\":[-999,-999,-999,-999],"
    "\"bend\":[-999,-999,-999,-999],\"inr\":[-999,-999,-999,-999],\"inu\":[-999,-999,-999,-999],"
    "\"bent\":[-999,-999,-999,-999],\"det\":[-999,-999,-999,-999],\"v\":0,\"bot\":0,\"cmp\":0,\"ewr\":0,"
    "\"fuel\":0,\"rpm\":0,\"crpm\":0,\"oilp\":0,\"bias\":0,\"drag\":0,\"lift\":0,\"dragu\":0},\"sc1\":0,"
    "\"sc2\":0,\"lhlim\":0,\"flapt\":0,\"flapid\":-1,\"dotdid\":-1,\"dotdsc\":-1,\"ll\":0";

typedef struct { sv2_data_t d; uint8_t *stored, *work; uint32_t *pool; } sa_rv_bufs_t;

static int sa_rv_bufs_alloc(sa_rv_bufs_t *B) {
    B->stored = (uint8_t *)malloc(SV2_MAX_PAYLOAD);
    B->work = (uint8_t *)malloc(SV2_MAX_PAYLOAD);
    B->pool = (uint32_t *)malloc(SV2_MAX_PAYLOAD * sizeof(uint32_t));   /* one block always fits (R1-A deviation D-4) */
    memset(&B->d, 0, sizeof B->d);
    B->d.pool = B->pool;
    B->d.pool_cap = SV2_MAX_PAYLOAD;
    if (B->stored && B->work && B->pool) return 1;
    free(B->stored); free(B->work); free(B->pool);
    B->stored = B->work = NULL;
    B->pool = NULL;
    return 0;
}

static void sa_rv_bufs_free(sa_rv_bufs_t *B) {
    free(B->stored); free(B->work); free(B->pool);
    B->stored = B->work = NULL;
    B->pool = NULL;
}

/* Raw stored value of channel id for the car at its tick i. Returns 0 when the block does not carry the channel. */
static int sa_rv_raw(const sv2_car_t *car, uint32_t id, uint32_t i, uint32_t *raw) {
    uint32_t rank;
    if (id >= SV2_TABLE_MAX_CHANNELS || !sv2_mask_bit(car->mask_lo, car->mask_hi, id)) return 0;
    if (id < 64u) rank = sv2_popcount64(car->mask_lo & ((((uint64_t)1) << id) - 1u));
    else rank = sv2_popcount64(car->mask_lo) + sv2_popcount64(car->mask_hi & ((((uint64_t)1) << (id - 64u)) - 1u));
    *raw = car->vals[(size_t)rank * car->count + i];
    return 1;
}

static const sv2_channel_t *sa_rv_chan(const sv2_idx_t *ix, uint32_t id) {
    for (uint32_t i = 0; i < ix->nch; i++) if (ix->ch[i].id == id) return &ix->ch[i];
    return NULL;
}

/* Real value of channel id: Q16 mapped to the table range of the file, RAW32 as the integer. */
static double sa_rv_real(const sv2_idx_t *ix, uint32_t id, uint32_t raw) {
    const sv2_channel_t *c = sa_rv_chan(ix, id);
    if (!c) return 0.0;
    if (c->type == SV2_TYPE_Q16) return (double)sv2_q16_decode((uint16_t)raw, c->min, c->max);
    /* 2026-10-14 (Claude review): RAW32 holds f32 bits except the integer ids (the writer's own rule, sw_raw_is_int).
     * Before this fix every float RAW32 channel (86 last lap, 24..63 pch, 102..117 rx) decoded as its bit pattern. */
    if (!sw_raw_is_int(id)) { float f = sv2_f32_from(raw); return isfinite(f) ? (double)f : 0.0; }
    return (double)(int32_t)raw;
}

/* TRF bit of a stored channel (1.0.1 trace flags). */
static uint32_t sa_rv_trf_of(uint32_t id) {
    switch (id) {
    case 1: return TRF_DIST;
    case 2: return TRF_SPEED;
    case 3: return TRF_THROTTLE;
    case 4: return TRF_BRAKE;
    case 5: return TRF_STEER;
    case 6: return TRF_GLAT;
    case 7: return TRF_GLON;
    case 8: return TRF_GVERT;
    case 9: return TRF_YAW;
    case 10: return TRF_YAWRATE;
    case 11: case 12: case 13: case 14: return TRF_TREAD;
    case 15: return TRF_RPM;
    case 16: return TRF_GEAR;
    case 17: case 18: return TRF_TRACK;
    case 20: case 21: case 22: case 23: return TRF_TTEMP;
    default: return 0;
    }
}

/* channels_seen: the channels the file declares (not checked per sample; see the R1B record). */
static uint32_t sa_rv_declared_trf(const sv2_idx_t *ix) {
    uint32_t m = 0;
    for (uint32_t i = 0; i < ix->nch; i++) m |= sa_rv_trf_of(ix->ch[i].id);
    return m;
}

/* One channel of one sample: sets trf in f and returns 1 when the block carries the channel. */
static int sa_rv_chan_val(const sv2_idx_t *ix, const sv2_car_t *car, uint32_t id, uint32_t i, uint32_t trf, uint32_t *f, double *v) {
    uint32_t raw = 0;
    *v = 0.0;
    if (!sa_rv_raw(car, id, i, &raw)) return 0;
    *v = sa_rv_real(ix, id, raw);
    *f |= trf;
    return 1;
}

static void sa_rv_num(char *o, size_t cap, int ok, double v, const char *fmt) {
    if (ok) snprintf(o, cap, fmt, v);
    else snprintf(o, cap, "null");
}

/* Opens the closed .f1t of a READY slot for reading. The caller closes h when it is not INVALID_HANDLE_VALUE. */
static int sa_rv_open(const sa_rev_slot_t *sl, HANDLE *h, sv2_rd_t *rd) {
    LARGE_INTEGER sz;
    *h = CreateFileA(sl->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (*h == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(*h, &sz)) { CloseHandle(*h); *h = INVALID_HANDLE_VALUE; return 0; }
    rd->read_at = sa_rev_read_at;
    rd->ctx = h;
    rd->len = (uint64_t)sz.QuadPart;
    return 1;
}

/* Last race position and AI flag of each car, from the newest blocks (at most SA_RV_POS_BLOCKS). A car with no
 * flag in those blocks keeps pos_ok 0 / ai_ok 0 and is answered as position 0, AI 0 (documented limit). */
static void sa_rv_scan_cars(sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, int32_t pos[], uint8_t pos_ok[],
                            uint8_t ai[], uint8_t ai_ok[]) {
    uint32_t steps = 0;
    for (uint32_t bi = ix->nblocks; bi-- > 0 && steps < SA_RV_POS_BLOCKS; steps++) {
        int done = 1;
        if (sv2_idx_read_block_rd(rd, ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) break;
        for (uint32_t c = 0; c < B->d.n_cars; c++) {
            const sv2_car_t *car = &B->d.cars[c];
            uint32_t raw = 0;
            if (car->slot >= SV2_MAX_CARS) continue;
            if (!ai_ok[car->slot]) { ai[car->slot] = car->scope == 0 ? 1 : 0; ai_ok[car->slot] = 1; }
            if (!pos_ok[car->slot] && sa_rv_raw(car, SA_RV_CH_POS, car->count - 1u, &raw)) {
                pos[car->slot] = (int32_t)raw;
                pos_ok[car->slot] = 1;
            }
        }
        for (unsigned s = 0; s < SV2_MAX_CARS; s++)
            if (ix->car[s].seen && (!ai_ok[s] || !pos_ok[s])) { done = 0; break; }
        if (done) break;
    }
}

/* GET /laps (1.0.1 shape plus source, lap_len null, validated false). */
static void sa_rv_laps_doc(sa_buf_t *b, const sv2_idx_t *ix, const char *rid, const int32_t pos[], const uint8_t pos_ok[],
                           const sa_rv_nm_t *nmt) {
    int first = 1;
    sa_f(b, "{\"gen\":0,\"archive\":");
    sa_s(b, rid);
    sa_f(b, ",\"source\":\"recording\",\"lap_len\":null,\"validated\":false,\"cars\":[");
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) {
        int nl = 0;
        if (!ix->car[s].seen) continue;
        sa_f(b, "%s{\"id\":%u,\"p\":%d,\"n\":", first ? "" : ",", s, pos_ok[s] ? (int)pos[s] : 0);
        sa_s(b, nmt[s].name);
        sa_f(b, ",\"laps\":[");
        first = 0;
        for (uint32_t k = 0; k < ix->nlaps; k++) {
            const sv2_idx_lap_t *r = &ix->laps[k];
            if (r->car != s || !r->has_start || !r->has_game) continue;
            sa_f(b, "%s%.3f", nl ? "," : "", r->game_time);
            nl = 1;
        }
        sa_f(b, "]}");
    }
    sa_f(b, "]}");
}

/* GET /trace/status and /trace/select (1.0.1 shape; mode archive kept for the console). */
static void sa_rv_status_doc(sa_buf_t *b, const sv2_idx_t *ix, const char *rid, const uint8_t ai[], const uint8_t ai_ok[],
                             const sa_rv_nm_t *nmt) {
    uint32_t chan = sa_rv_declared_trf(ix);
    int first = 1;
    sa_f(b, "{\"gen\":0,\"archive\":");
    sa_s(b, rid);
    sa_f(b, ",\"source\":\"recording\",\"lap_len\":0,\"laps_max\":100000,\"samples_max\":%u,\"mode\":\"archive\",\"cars\":[",
         (unsigned)TRACE_SAMPLES);
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) {
        int nl = 0;
        uint64_t total = 0;
        if (!ix->car[s].seen) continue;
        for (uint32_t k = 0; k < ix->nlaps; k++)
            if (ix->laps[k].car == s && sv2_idx_lap_listed(&ix->laps[k])) total += ix->laps[k].samples;
        sa_f(b, "%s{\"rev\":0,\"which\":\"\",\"slot\":%u,\"used\":1,\"auto\":0,\"n\":", first ? "" : ",", s);
        sa_s(b, nmt[s].name);
        sa_f(b, ",\"cn\":");
        sa_s(b, nmt[s].team);
        sa_f(b, ",\"ai\":%d,"
                "\"remote\":0,\"team\":0,\"dropped_laps\":0,\"open\":0,\"open_lap\":-1,\"open_samples\":0,"
                "\"open_dropped\":0,\"channels_seen\":%u,\"total_samples\":%llu,\"laps\":[",
             (ai_ok[s] && ai[s]) ? 1 : 0, chan, (unsigned long long)total);
        first = 0;
        for (uint32_t k = 0; k < ix->nlaps; k++) {
            const sv2_idx_lap_t *r = &ix->laps[k];
            if (r->car != s || !sv2_idx_lap_listed(r)) continue;
            /* 1.2.5: flags as live (1 = completed; 2 = no game time, listed like a pit lap). 1.2.4 sent 0, so the
             * Export dialog found no lap in a recording. */
            sa_f(b, "%s{\"lap\":%u,\"t\":%.3f,\"samples\":%u,\"flags\":%d,\"start\":%.3f,"
                    "\"lid\":{\"gen\":0,\"rev\":0,\"slot\":%u,\"lap\":%u,\"src\":\"recording\"}}",
                 nl ? "," : "", r->lap, r->has_game ? r->game_time : r->end_sim - r->start_sim, r->samples,
                 r->has_game ? 1 : 3, r->start_sim, s, r->lap);
            nl = 1;
        }
        sa_f(b, "],\"unavailable\":[],\"unavailable_why\":{},\"evicted\":[]}");
    }
    sa_f(b, "]}");
}

/* One sample of one car (the 19 short keys of schemas/trace_data.json). f carries the bits of the channels present. */
static void sa_rv_emit_sample(sa_buf_t *b, const sv2_idx_t *ix, const sv2_car_t *car, uint32_t i, double t, int first) {
    uint32_t f = 0;
    double dv = 0, sv = 0, thv = 0, brv = 0, stv = 0, glv = 0, gov = 0, gvv = 0, yv = 0, yrv = 0, rpv = 0, gev = 0;
    double txv = 0, tzv = 0, trv[4] = {0, 0, 0, 0}, ttv[4] = {0, 0, 0, 0};
    int oks, okth, okbr, okst, okgl, okgo, okgv, oky, okyr, okrp, okge, oktx, oktz, okt = 1, okty = 1;
    char so[32], tho[32], bro[32], sto[32], glo[32], goo[32], gvo[32], yo[32], yro[32], rpo[32], geo[32], txo[32], tzo[32],
         tto[96];
    (void)sa_rv_chan_val(ix, car, 1, i, TRF_DIST, &f, &dv);   /* d: always a number (0 when absent) */
    oks = sa_rv_chan_val(ix, car, 2, i, TRF_SPEED, &f, &sv);
    okth = sa_rv_chan_val(ix, car, 3, i, TRF_THROTTLE, &f, &thv);
    okbr = sa_rv_chan_val(ix, car, 4, i, TRF_BRAKE, &f, &brv);
    okst = sa_rv_chan_val(ix, car, 5, i, TRF_STEER, &f, &stv);
    okgl = sa_rv_chan_val(ix, car, 6, i, TRF_GLAT, &f, &glv);
    okgo = sa_rv_chan_val(ix, car, 7, i, TRF_GLON, &f, &gov);
    okgv = sa_rv_chan_val(ix, car, 8, i, TRF_GVERT, &f, &gvv);
    oky = sa_rv_chan_val(ix, car, 9, i, TRF_YAW, &f, &yv);
    okyr = sa_rv_chan_val(ix, car, 10, i, TRF_YAWRATE, &f, &yrv);
    okrp = sa_rv_chan_val(ix, car, 15, i, TRF_RPM, &f, &rpv);
    okge = sa_rv_chan_val(ix, car, 16, i, TRF_GEAR, &f, &gev);
    oktx = sa_rv_chan_val(ix, car, 17, i, 0, &f, &txv);
    oktz = sa_rv_chan_val(ix, car, 18, i, 0, &f, &tzv);
    if (oktx && oktz) f |= TRF_TRACK;
    for (uint32_t k = 0; k < 4; k++) okt = sa_rv_chan_val(ix, car, 11 + k, i, 0, &f, &trv[k]) && okt;
    if (okt) f |= TRF_TREAD;
    for (uint32_t k = 0; k < 4; k++) okty = sa_rv_chan_val(ix, car, 20 + k, i, 0, &f, &ttv[k]) && okty;
    if (okty) f |= TRF_TTEMP;
    sa_rv_num(so, sizeof so, oks, sv, "%.2f");
    sa_rv_num(tho, sizeof tho, okth, thv, "%.2f");
    sa_rv_num(bro, sizeof bro, okbr, brv, "%.2f");
    sa_rv_num(sto, sizeof sto, okst, stv, "%.4f");
    sa_rv_num(glo, sizeof glo, okgl, glv, "%.2f");
    sa_rv_num(goo, sizeof goo, okgo, gov, "%.2f");
    sa_rv_num(gvo, sizeof gvo, okgv, gvv, "%.2f");
    sa_rv_num(yo, sizeof yo, oky, yv, "%.3f");
    sa_rv_num(yro, sizeof yro, okyr, yrv, "%.3f");
    sa_rv_num(rpo, sizeof rpo, okrp, rpv, "%.1f");
    sa_rv_num(geo, sizeof geo, okge, gev, "%.0f");
    sa_rv_num(txo, sizeof txo, oktx && oktz, txv, "%.1f");
    sa_rv_num(tzo, sizeof tzo, oktx && oktz, tzv, "%.1f");
    if (okty) snprintf(tto, sizeof tto, "[%.1f,%.1f,%.1f,%.1f]", ttv[0], ttv[1], ttv[2], ttv[3]);
    else snprintf(tto, sizeof tto, "null");
    sa_f(b, "%s{\"t\":%.3f,\"d\":%.1f,\"s\":%s,\"th\":%s,\"br\":%s,\"st\":%s,\"gl\":%s,\"go\":%s,\"gv\":%s,"
            "\"y\":%s,\"yr\":%s,\"rpm\":%s,\"ge\":%s,\"tx\":%s,\"tz\":%s,\"tr\":[%.4f,%.4f,%.4f,%.4f],"
            "\"tt\":%s,\"rx\":null,\"f\":%u}",
         first ? "" : ",", t, dv, so, tho, bro, sto, glo, goo, gvo, yo, yro, rpo, geo, txo, tzo,
         trv[0], trv[1], trv[2], trv[3], tto, f);
}

/* GET /trace/data?car=1000+slot&lap=N: f1c.trace/1 with source recording. Returns the HTTP code; on an error the body
 * is the error JSON (sa_err resets b first). */
static int sa_rv_trace_doc(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, const char *rid, uint32_t slot,
                           uint32_t lap, const char *track, const sa_rv_nm_t *nmt) {
    const sv2_idx_lap_t *r = NULL;
    uint32_t chan = sa_rv_declared_trf(ix);
    char drv[NAME_LEN + 16];
    int ai = 0, have_ai = 0, trunc = 0;
    uint64_t n = 0;
    for (uint32_t k = 0; k < ix->nlaps && !r; k++) {
        const sv2_idx_lap_t *c = &ix->laps[k];
        if (c->car == slot && c->lap == lap && sv2_idx_lap_listed(c)) r = c;
    }
    if (!r) { sa_err(b, 404, "lap not recorded in this session"); return 404; }
    /* pass 1: the AI flag of this car, from the first block in the lap's time range that carries it */
    for (uint32_t bi = 0; bi < ix->nblocks && !have_ai; bi++) {
        const sv2_idx_block_t *ib = &ix->blocks[bi];
        if (ib->t_last < r->start_sim - 0.001 || ib->t_first > r->end_sim + 0.001) continue;
        if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) {
            sa_err(b, 500, "the recording could not be read"); return 500;
        }
        for (uint32_t c = 0; c < B->d.n_cars && !have_ai; c++)
            if (B->d.cars[c].slot == slot) { ai = B->d.cars[c].scope == 0 ? 1 : 0; have_ai = 1; }
    }
    sa_f(b, "{\"schema\":\"f1c.trace/1\",\"source\":\"recording\",\"archive\":");
    sa_s(b, rid);
    sa_rv_nm_name(nmt, slot, drv, sizeof drv);
    sa_f(b, ",\"open\":false,\"gen\":0,\"rev\":0,\"which\":\"\",\"slot\":%u,\"driver\":", slot);
    sa_s(b, drv);
    sa_f(b, ",\"car\":");   /* TAPE2 N1: the team name (cn of the other review docs), "" when unknown */
    sa_s(b, slot < SV2_MAX_CARS ? nmt[slot].team : "");
    /* 1.2.5 (DVOA report 2026-10-18): a timed lap is complete ("partial":false, flags 1) and carries the game time and the
     * line-to-line time; an untimed lap is partial (flags 3). 1.2.4 sent neither, so every recorded lap loaded as
     * partial and PAST never showed a Fastest Lap. */
    sa_f(b, ",\"ai\":%d,"
            "\"team\":0,\"lap\":%u,\"lap_time\":%.3f,\"lap_time_src\":\"%s\",\"derived_lap_time\":%.3f,"
            "\"partial\":%s,\"start_sim\":%.3f,\"lap_len\":0,\"count\":%u,\"flags\":%d,"
            "\"channels_seen\":%u,\"track\":",
         ai, lap, r->has_game ? r->game_time : r->end_sim - r->start_sim, r->has_game ? "game" : "derived",
         r->end_sim - r->start_sim, r->has_game ? "false" : "true", r->start_sim, r->samples, r->has_game ? 1 : 3, chan);
    sa_s(b, track);
    sa_f(b, ",\"lid\":{\"gen\":0,\"rev\":0,\"slot\":%u,\"lap\":%u,\"src\":\"recording\"},\"samples\":[", slot, lap);
    /* pass 2: the samples of this lap (same car, same lap number, inside the lap's sim-time range), one block at a time */
    for (uint32_t bi = 0; bi < ix->nblocks && !trunc; bi++) {
        const sv2_idx_block_t *ib = &ix->blocks[bi];
        const sv2_car_t *car = NULL;
        if (ib->t_last < r->start_sim - 0.001 || ib->t_first > r->end_sim + 0.001) continue;
        if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) {
            sa_err(b, 500, "the recording could not be read"); return 500;
        }
        for (uint32_t c = 0; c < B->d.n_cars && !car; c++)
            if (B->d.cars[c].slot == slot) car = &B->d.cars[c];
        if (!car) continue;
        for (uint32_t i = 0; i < car->count && !trunc; i++) {
            uint32_t lapraw = 0;
            double t = ib->t_first + (double)B->d.sim_ms[car->first_tick + i] / 1000.0;
            if (!sa_rv_raw(car, SA_RV_CH_LAP, i, &lapraw) || (int32_t)lapraw != (int32_t)lap) continue;
            if (t < r->start_sim - 0.001 || t > r->end_sim + 0.001) continue;
            if (n >= TRACE_SAMPLES) { trunc = 1; break; }
            sa_rv_emit_sample(b, ix, car, i, t, n == 0);
            n++;
        }
    }
    sa_f(b, "]%s}", trunc ? ",\"truncated\":true" : "");
    return b->bad ? 500 : 200;
}

/* GET /telemetry: one synthesized frame at the last committed tick (spec 5.5). Every required key of the live frame
 * is present with the API contract sentinels; c lists the cars seen in the file. */
static void sa_rv_frame_doc(sa_buf_t *b, const sv2_idx_t *ix, const sv2_header_t *h, const char *rid, const char *stem,
                            const int32_t pos[], const uint8_t pos_ok[], const sa_rv_nm_t *nmt) {
    unsigned long long ticks = 0;
    double end_t = 0.0;
    int first = 1;
    for (uint32_t i = 0; i < ix->nblocks; i++) {
        ticks += ix->blocks[i].ticks;
        if (ix->blocks[i].t_last > end_t) end_t = ix->blocks[i].t_last;
    }
    sa_f(b, "{\"version\":");
    sa_s(b, TELEMETRY_VERSION);
    sa_f(b, ",\"ty\":%d,\"trk\":", (int)h->session_type);
    sa_s(b, h->track);
    sa_f(b, ",\"lapm\":0,\"review\":{\"session\":");   /* 0 = unknown (API contract section 3; schema minimum 0) */
    sa_s(b, rid);
    sa_f(b, ",\"stem\":");
    sa_s(b, stem);
    sa_f(b, ",\"ticks\":%llu,\"end_t\":%.3f},", ticks, end_t);
    sa_f(b, "%s,\"c\":[", sa_rv_frame_rest);
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) {
        if (!ix->car[s].seen) continue;
        sa_f(b, "%s{\"id\":%u,\"l\":%d,\"p\":%d,\"n\":", first ? "" : ",", s, (int)ix->car[s].lap,
             pos_ok[s] ? (int)pos[s] : 0);
        sa_s(b, nmt[s].name);
        sa_f(b, "%s", sa_rv_car_rest);
        first = 0;
    }
    sa_f(b, "]}");
}

/* ---------- REPLAY 2.1 (2026-10-14): /replay/info and /replay, and the event and driver reader they share ----------
 * Contract: REPLAY_SPEC_2026-10-14.md 2.1 and 2.2 (fixed). Events come from the EVENT blocks (writer: lap, pit,
 * incident, driver; session_start and session_end are not listed). Names come from the newest driver EVENT of a slot.
 * Each request walks the EVENT blocks once (sv2_event_walk_rd) and reads only the DATA blocks it needs. */
#define SA_RV_EV_MAX     4000u   /* events kept in file order (= sim order); more sets events_truncated */
#define SA_RV_ROWS_MAX   160u    /* window rows of one car: 30 s at 0.2 s spacing = 151, plus the last tick */
#define SA_RV_OUT_PTS    600u    /* outline points at most */
#define SA_RV_OUT_RAW    16384u  /* raw points of one lap read before decimation */
#define SA_RV_WIN_MAX    30.0    /* s, window length limit */
#define SA_RV_DECIM      0.2     /* s, minimum sim time between rows of one car (5 Hz) */
#define SA_RV_OUT_MIN    8.0     /* m, minimum outline spacing */
#define SA_RV_TXT        64u     /* driver name and team copies */
#define SA_RV_EV_LAP 1u
#define SA_RV_EV_PIT 2u
#define SA_RV_EV_INC 3u

typedef struct { double t; uint32_t order, car, lap; double last; int state, with, count; uint8_t type; char kind[12]; } sa_rv_ev_t;
typedef struct { uint8_t have; int32_t num; char name[SA_RV_TXT], team[SA_RV_TXT]; } sa_rv_drv_t;
/* 1.2.5: ll / ll_have = the car's previous accepted lap time (the index rule for a repeated in-lap time). */
typedef struct { sa_rv_ev_t *ev; uint32_t n; int truncated; sa_rv_drv_t drv[SV2_MAX_CARS];
                 double ll[SV2_MAX_CARS]; uint8_t ll_have[SV2_MAX_CARS]; } sa_rv_evs_t;

/* Copies the JSON string member key of j (\" and \\ unescaped) into out. Returns 1 when the member exists. */
static int sa_rv_json_str(const char *j, const char *key, char *out, size_t cap) {
    char pat[32];
    const char *p;
    size_t k = 0;
    snprintf(pat, sizeof pat, "\"%s\":\"", key);
    p = strstr(j, pat);
    if (cap == 0) return 0;
    if (!p) { out[0] = 0; return 0; }
    p += strlen(pat);
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p++;
        if (k + 1 < cap) out[k++] = *p;
        p++;
    }
    out[k] = 0;
    return *p == '"';
}

/* One EVENT block: driver names are kept per slot (newest wins); lap, pit and incident rows go to the event list. */
static int sa_rv_ev_cb(void *ctx, double t, const uint8_t *p, size_t n) {
    sa_rv_evs_t *E = (sa_rv_evs_t *)ctx;
    char js[SV2_EVENT_MAX + 1];
    double v = 0.0;
    sa_rv_ev_t *ev;
    if (n == 0 || n > SV2_EVENT_MAX || !sv2_event_ok(p, n)) return SV2_OK;   /* the index build already refused bad ones */
    memcpy(js, p, n);
    js[n] = '\0';
    if (strstr(js, "\"type\":\"driver\"")) {
        if (sv2_json_num(js, "car", &v) && v >= 0.0 && v < (double)SV2_MAX_CARS) {
            sa_rv_drv_t *d = &E->drv[(unsigned)v];
            char nm[SA_RV_TXT];
            if (sa_rv_json_str(js, "name", nm, sizeof nm) && nm[0]) {
                d->have = 1;
                sv2_copy(d->name, sizeof d->name, nm);
                if (!sa_rv_json_str(js, "team", d->team, sizeof d->team)) d->team[0] = 0;
                d->num = sv2_json_num(js, "num", &v) ? (int32_t)v : -1;
            }
        }
        return SV2_OK;
    }
    if (strstr(js, "\"type\":\"lap\"")) {
        if (!sv2_json_num(js, "car", &v)) return SV2_OK;
        if (E->n >= SA_RV_EV_MAX) { E->truncated = 1; return SV2_OK; }
        ev = &E->ev[E->n];
        memset(ev, 0, sizeof *ev);
        ev->type = SA_RV_EV_LAP; ev->t = t; ev->car = (uint32_t)v; ev->order = E->n;
        ev->lap = sv2_json_num(js, "prev_lap", &v) ? (uint32_t)v : 0u;   /* the completed lap (index rule) */
        ev->last = sv2_json_num(js, "last_lap_s", &v) ? v : -1.0;
        if (ev->car < SV2_MAX_CARS && ev->last > 0.0) {   /* 1.2.5: a repeated time is the previous lap's (untimed in lap) */
            if (E->ll_have[ev->car] && fabs(ev->last - E->ll[ev->car]) < 0.0005) ev->last = -1.0;
            else { E->ll_have[ev->car] = 1; E->ll[ev->car] = ev->last; }
        }
        E->n++;
    } else if (strstr(js, "\"type\":\"pit\"")) {
        if (!sv2_json_num(js, "car", &v)) return SV2_OK;
        if (E->n >= SA_RV_EV_MAX) { E->truncated = 1; return SV2_OK; }
        ev = &E->ev[E->n];
        memset(ev, 0, sizeof *ev);
        ev->type = SA_RV_EV_PIT; ev->t = t; ev->car = (uint32_t)v; ev->order = E->n;
        ev->state = sv2_json_num(js, "state", &v) ? (int)v : 0;
        E->n++;
    } else if (strstr(js, "\"type\":\"incident\"")) {
        if (!sv2_json_num(js, "car", &v)) return SV2_OK;
        if (E->n >= SA_RV_EV_MAX) { E->truncated = 1; return SV2_OK; }
        ev = &E->ev[E->n];
        memset(ev, 0, sizeof *ev);
        ev->type = SA_RV_EV_INC; ev->t = t; ev->car = (uint32_t)v; ev->order = E->n;
        if (!sa_rv_json_str(js, "kind", ev->kind, sizeof ev->kind)) ev->kind[0] = 0;
        ev->with = sv2_json_num(js, "with", &v) ? (int)v : -1;
        ev->count = sv2_json_num(js, "count", &v) ? (int)v : -1;
        E->n++;
    }
    return SV2_OK;
}

static int sa_rv_ev_cmp(const void *a, const void *b) {
    const sa_rv_ev_t *x = (const sa_rv_ev_t *)a, *y = (const sa_rv_ev_t *)b;
    if (x->t < y->t) return -1;
    if (x->t > y->t) return 1;
    return x->order < y->order ? -1 : x->order > y->order;
}

/* Loads the EVENT list and the driver names of the file. The caller frees E->ev. 0 when the file cannot be read. */
static int sa_rv_events_load(sv2_rd_t *rd, sa_rv_bufs_t *B, sa_rv_evs_t *E) {
    int r;
    memset(E, 0, sizeof *E);
    E->ev = (sa_rv_ev_t *)malloc(SA_RV_EV_MAX * sizeof(sa_rv_ev_t));
    if (!E->ev) return 0;
    r = sv2_event_walk_rd(rd, sa_rv_ev_cb, E, B->stored, B->work, SV2_MAX_PAYLOAD);
    if (r != SV2_OK) { free(E->ev); E->ev = NULL; return 0; }
    if (E->n) qsort(E->ev, E->n, sizeof(sa_rv_ev_t), sa_rv_ev_cmp);
    return 1;
}

/* Newest value of channel id of each car that carries it, read from the newest DATA blocks first. */
static void sa_rv_newest(const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, uint32_t id, double val[], uint8_t ok[], uint32_t max_steps) {
    uint32_t steps = 0;
    for (uint32_t bi = ix->nblocks; bi-- > 0 && steps < max_steps; steps++) {
        int done = 1;
        if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) break;
        for (uint32_t c = 0; c < B->d.n_cars; c++) {
            const sv2_car_t *car = &B->d.cars[c];
            uint32_t raw = 0;
            if (car->slot >= SV2_MAX_CARS || ok[car->slot] || car->count == 0) continue;
            if (sa_rv_raw(car, id, car->count - 1u, &raw)) { val[car->slot] = sa_rv_real(ix, id, raw); ok[car->slot] = 1; }
        }
        for (unsigned s = 0; s < SV2_MAX_CARS; s++)
            if (ix->car[s].seen && !ok[s]) { done = 0; break; }
        if (done) break;
    }
}

/* Span of the file: first and last tick sim time over all DATA blocks; ticks is their sum. */
static void sa_rv_span(const sv2_idx_t *ix, double *t_first, double *t_last, unsigned long long *ticks) {
    *t_first = 0.0; *t_last = 0.0; *ticks = 0;
    for (uint32_t i = 0; i < ix->nblocks; i++) {
        if (i == 0 || ix->blocks[i].t_first < *t_first) *t_first = ix->blocks[i].t_first;
        if (i == 0 || ix->blocks[i].t_last > *t_last) *t_last = ix->blocks[i].t_last;
        *ticks += ix->blocks[i].ticks;
    }
}

/* REPLAY 2.1 role of a slot (Claude follow-up): remote when channel 98 was 2 at any tick of the session, else player when it
 * was 0 at any tick, else ai. From the index flags (sv2_idx_apply_data); no extra read. */
static const char *sa_rv_role_of(const sv2_idx_t *ix, uint32_t s) {
    uint8_t f = ix->car[s].ctl;
    return (f & 2u) ? "remote" : (f & 1u) ? "player" : "ai";
}
static int sa_rv_role_is(const sv2_idx_t *ix, uint32_t s, const char *role) { return !strcmp(sa_rv_role_of(ix, s), role); }

/* One optional value of an array or object: ",<value>" with fmt, or ",null" when absent or not finite. */
static void sa_rv_put(sa_buf_t *b, int ok, double v, const char *fmt) {
    sa_f(b, ",");
    if (ok && isfinite(v)) sa_f(b, fmt, v);
    else sa_f(b, "null");
}

/* Track outline (REPLAY 2.1): the raw points of one complete timed lap, the player's fastest one when a player lap
 * exists, else the first complete lap of the index. Returns 0 when no lap is complete (outline stays empty). */
static int sa_rv_outline_lap(const sv2_idx_t *ix, const uint8_t player[], uint32_t *lap_k) {
    int best = -1, first = -1;
    for (uint32_t k = 0; k < ix->nlaps; k++) {
        const sv2_idx_lap_t *r = &ix->laps[k];
        if (!r->has_start || !r->has_game) continue;
        if (first < 0) first = (int)k;
        if (r->car < SV2_MAX_CARS && player[r->car] && (best < 0 || r->game_time < ix->laps[best].game_time)) best = (int)k;
    }
    if (best < 0) best = first;
    if (best < 0) return 0;
    *lap_k = (uint32_t)best;
    return 1;
}

/* Outline points of the lap k: track_x/track_z, decimated to at most SA_RV_OUT_PTS (spacing >= 8 m, raised for long laps). */
static uint32_t sa_rv_outline(const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, uint32_t k, double (*out)[2]) {
    const sv2_idx_lap_t *r = &ix->laps[k];
    double *raw = (double *)malloc(SA_RV_OUT_RAW * 2u * sizeof(double));
    uint32_t n = 0, m = 0;
    double total = 0.0, step, lx = 0.0, lz = 0.0;
    if (!raw) return 0;
    for (uint32_t bi = 0; bi < ix->nblocks && n < SA_RV_OUT_RAW; bi++) {
        const sv2_idx_block_t *ib = &ix->blocks[bi];
        if (ib->t_last < r->start_sim - 0.001 || ib->t_first > r->end_sim + 0.001) continue;
        if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) break;
        for (uint32_t c = 0; c < B->d.n_cars; c++) {
            const sv2_car_t *car = &B->d.cars[c];
            if (car->slot != r->car) continue;
            for (uint32_t i = 0; i < car->count && n < SA_RV_OUT_RAW; i++) {
                uint32_t lapraw = 0, xr = 0, zr = 0;
                double t = ib->t_first + (double)B->d.sim_ms[car->first_tick + i] / 1000.0;
                if (t < r->start_sim - 0.001 || t > r->end_sim + 0.001) continue;
                if (!sa_rv_raw(car, SA_RV_CH_LAP, i, &lapraw) || (int32_t)lapraw != (int32_t)r->lap) continue;
                if (!sa_rv_raw(car, 17u, i, &xr) || !sa_rv_raw(car, 18u, i, &zr)) continue;
                raw[2 * n] = sa_rv_real(ix, 17u, xr);
                raw[2 * n + 1] = sa_rv_real(ix, 18u, zr);
                n++;
            }
        }
    }
    for (uint32_t i = 1; i < n; i++) total += hypot(raw[2 * i] - raw[2 * i - 2], raw[2 * i + 1] - raw[2 * i - 1]);
    step = total / (double)(SA_RV_OUT_PTS - 10u);
    if (step < SA_RV_OUT_MIN) step = SA_RV_OUT_MIN;
    for (uint32_t i = 0; i < n && m < SA_RV_OUT_PTS; i++) {
        if (m == 0 || hypot(raw[2 * i] - lx, raw[2 * i + 1] - lz) >= step) {
            out[m][0] = raw[2 * i];
            out[m][1] = raw[2 * i + 1];
            lx = raw[2 * i]; lz = raw[2 * i + 1];
            m++;
        }
    }
    free(raw);
    return m;
}

/* GET /replay/info (REPLAY 2.1): span, cars, events, outline. Read from the file on each request (no cache). */
static int sa_rv_info_doc(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, const sv2_header_t *h,
                          const char *rid, const char *stem, const sa_rv_nm_t *nmt) {
    sa_rv_evs_t *E = (sa_rv_evs_t *)calloc(1, sizeof(sa_rv_evs_t));
    uint8_t player[SV2_MAX_CARS];
    double t_first = 0.0, t_last = 0.0, hz = 0.0;
    unsigned long long ticks = 0;
    uint32_t lap_k = 0, npts = 0;
    double (*pts)[2] = NULL;
    int first = 1;
    if (!E) return 0;
    if (!sa_rv_events_load(rd, B, E)) { free(E); return 0; }
    memset(player, 0, sizeof player);
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) player[s] = sa_rv_role_is(ix, s, "player");
    sa_rv_span(ix, &t_first, &t_last, &ticks);
    if (t_last > t_first) hz = (double)ticks / (t_last - t_first);
    if (sa_rv_outline_lap(ix, player, &lap_k)) {
        pts = (double (*)[2])malloc(SA_RV_OUT_PTS * 2u * sizeof(double));
        if (pts) npts = sa_rv_outline(ix, rd, B, lap_k, pts);
    }
    sa_f(b, "{\"schema\":\"f1c.replay.info/1\",\"session\":");
    sa_s(b, rid);
    sa_f(b, ",\"stem\":");
    sa_s(b, stem);
    sa_f(b, ",\"track\":");
    sa_s(b, h->track);
    sa_f(b, ",\"session_type\":%d,\"t_first\":%.3f,\"t_last\":%.3f,\"hz\":%.2f,\"cars\":[", (int)h->session_type, t_first, t_last, hz);
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) {
        if (!ix->car[s].seen) continue;
        sa_f(b, "%s{\"slot\":%u,\"name\":", first ? "" : ",", s);
        sa_s(b, nmt[s].name);
        sa_f(b, ",\"team\":");
        sa_s(b, nmt[s].team);
        sa_f(b, ",\"num\":%ld,\"role\":\"%s\",\"laps\":%lu}", (long)nmt[s].num,
             sa_rv_role_of(ix, s),
             (unsigned long)ix->car[s].lap);
        first = 0;
    }
    sa_f(b, "],\"events\":[");
    for (uint32_t k = 0; k < E->n; k++) {
        const sa_rv_ev_t *e = &E->ev[k];
        sa_f(b, "%s{\"t\":%.3f,\"type\":\"%s\",\"car\":%u", k ? "," : "", e->t,
             e->type == SA_RV_EV_LAP ? "lap" : e->type == SA_RV_EV_PIT ? "pit" : "incident", e->car);
        if (e->type == SA_RV_EV_LAP) sa_f(b, ",\"lap\":%u,\"last_lap_s\":%.3f", e->lap, e->last);
        else if (e->type == SA_RV_EV_PIT) sa_f(b, ",\"state\":%d", e->state);
        else {
            sa_f(b, ",\"kind\":");
            sa_s(b, e->kind);
            if (e->with >= 0) sa_f(b, ",\"with\":%d", e->with);
            if (e->count >= 0) sa_f(b, ",\"count\":%d", e->count);
        }
        sa_f(b, "}");
    }
    sa_f(b, "],\"events_truncated\":%s,\"outline\":[", E->truncated ? "true" : "false");
    for (uint32_t i = 0; i < npts; i++) sa_f(b, "%s[%.1f,%.1f]", i ? "," : "", pts[i][0], pts[i][1]);
    sa_f(b, "]}");
    free(pts);
    free(E->ev);
    free(E);
    return b->bad ? 0 : 1;
}

/* One window row: 11 columns x, z, spd, pos, lap, pit, gear, thr, brk, dist, fin (ok = bit per present column). */
typedef struct { double t; double v[11]; uint16_t ok; } sa_rv_row_t;
static const uint32_t sa_rv_win_ids[11] = { 17, 18, 2, 68, 69, 70, 16, 3, 4, 1, 122 };

static void sa_rv_row_fill(const sv2_idx_t *ix, const sv2_car_t *car, uint32_t i, double t, sa_rv_row_t *r) {
    uint32_t raw = 0;
    r->t = t;
    r->ok = 0;
    for (uint32_t k = 0; k < 11; k++) {
        r->v[k] = 0.0;
        if (sa_rv_raw(car, sa_rv_win_ids[k], i, &raw)) { r->v[k] = sa_rv_real(ix, sa_rv_win_ids[k], raw); r->ok |= (uint16_t)(1u << k); }
    }
}

static void sa_rv_row_emit(sa_buf_t *b, const sa_rv_row_t *r, int first) {
    sa_f(b, "%s[%.3f", first ? "" : ",", r->t);
    sa_rv_put(b, r->ok & 1u, r->v[0], "%.1f");
    sa_rv_put(b, r->ok & 2u, r->v[1], "%.1f");
    sa_rv_put(b, r->ok & 4u, r->v[2], "%.1f");
    sa_rv_put(b, r->ok & 8u, r->v[3], "%.0f");
    sa_rv_put(b, r->ok & 16u, r->v[4], "%.0f");
    sa_rv_put(b, r->ok & 32u, r->v[5], "%.0f");
    sa_rv_put(b, r->ok & 64u, r->v[6], "%.0f");
    sa_rv_put(b, r->ok & 128u, r->v[7], "%.2f");
    sa_rv_put(b, r->ok & 256u, r->v[8], "%.2f");
    sa_rv_put(b, r->ok & 512u, r->v[9], "%.1f");
    sa_rv_put(b, r->ok & 1024u, r->v[10], "%.0f");
    sa_f(b, "]");
}

/* GET /replay (REPLAY 2.2): window [t0, t1] (clamped by the caller) at 5 Hz per car, every car with rows listed. */
static int sa_rv_window_doc(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, const char *rid,
                            double t0, double t1) {
    sa_rv_row_t *rows = (sa_rv_row_t *)malloc((size_t)SV2_MAX_CARS * SA_RV_ROWS_MAX * sizeof(sa_rv_row_t));
    uint32_t nrows[SV2_MAX_CARS];
    uint8_t has_last[SV2_MAX_CARS], pend[SV2_MAX_CARS];
    double last_t[SV2_MAX_CARS];
    sa_rv_row_t pendrow[SV2_MAX_CARS], cur;
    int first = 1;
    if (!rows) return 0;
    memset(nrows, 0, sizeof nrows);
    memset(has_last, 0, sizeof has_last);
    memset(pend, 0, sizeof pend);
    for (uint32_t bi = 0; bi < ix->nblocks; bi++) {
        const sv2_idx_block_t *ib = &ix->blocks[bi];
        if (ib->t_last < t0 - 1e-9 || ib->t_first > t1 + 1e-9) continue;
        if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) { free(rows); return 0; }
        for (uint32_t c = 0; c < B->d.n_cars; c++) {
            const sv2_car_t *car = &B->d.cars[c];
            if (car->slot >= SV2_MAX_CARS) continue;
            for (uint32_t i = 0; i < car->count; i++) {
                unsigned s = car->slot;
                double tt = ib->t_first + (double)B->d.sim_ms[car->first_tick + i] / 1000.0;
                if (tt < t0 - 1e-9 || tt > t1 + 1e-9) continue;
                sa_rv_row_fill(ix, car, i, tt, &cur);
                if (!has_last[s] || tt - last_t[s] >= SA_RV_DECIM - 1e-9) {
                    if (nrows[s] < SA_RV_ROWS_MAX) rows[(size_t)s * SA_RV_ROWS_MAX + nrows[s]++] = cur;
                    last_t[s] = tt;
                    has_last[s] = 1;
                    pend[s] = 0;
                } else {
                    pendrow[s] = cur;   /* newest tick not emitted yet: listed at the end so the last tick is always there */
                    pend[s] = 1;
                }
            }
        }
    }
    for (unsigned s = 0; s < SV2_MAX_CARS; s++)
        if (pend[s] && nrows[s] < SA_RV_ROWS_MAX) rows[(size_t)s * SA_RV_ROWS_MAX + nrows[s]++] = pendrow[s];
    sa_f(b, "{\"schema\":\"f1c.replay/1\",\"session\":");
    sa_s(b, rid);
    sa_f(b, ",\"t0\":%.3f,\"t1\":%.3f,\"cols\":[\"t\",\"x\",\"z\",\"spd\",\"pos\",\"lap\",\"pit\",\"gear\",\"thr\",\"brk\",\"dist\",\"fin\"],\"cars\":[", t0, t1);
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) {
        if (!nrows[s]) continue;
        sa_f(b, "%s{\"slot\":%u,\"rows\":[", first ? "" : ",", s);
        first = 0;
        for (uint32_t k = 0; k < nrows[s]; k++) sa_rv_row_emit(b, &rows[(size_t)s * SA_RV_ROWS_MAX + k], k == 0);
        sa_f(b, "]}");
    }
    sa_f(b, "]}");
    free(rows);
    return b->bad ? 0 : 1;
}

/* Parses a query number: finite, the whole text. Returns 0 when absent or invalid. */
static int sa_rv_parse_num(const char *s, double *out) {
    char *end = NULL;
    double v;
    if (!s || !s[0]) return 0;
    v = strtod(s, &end);
    if (end == s || *end || !isfinite(v)) return 0;
    *out = v;
    return 1;
}

/* REPLAY 2.2 window arguments: t0 and t1 finite, t0 < t1, at most SA_RV_WIN_MAX long, then clamped to [tf, tl].
 * Returns 0 with the clamped window, or 400 with msg (the body the route sends). Pure: the runtime test calls it too. */
static int sa_rv_window_args(const char *s0, const char *s1, double tf, double tl, double *o0, double *o1, const char **msg) {
    double t0 = 0.0, t1 = 0.0;
    if (!sa_rv_parse_num(s0, &t0) || !sa_rv_parse_num(s1, &t1)) { *msg = "t0 and t1 are required and must be finite numbers"; return 400; }
    if (t1 <= t0) { *msg = "t0 must be less than t1"; return 400; }
    if (t1 - t0 > SA_RV_WIN_MAX) { *msg = "window too long"; return 400; }
    if (t0 < tf) t0 = tf;
    if (t1 > tl) t1 = tl;
    if (t1 < t0) t1 = t0;
    *o0 = t0;
    *o1 = t1;
    return 0;
}

/* ---------- REPLAY 2.2 / TAPE 2.2 (2026-10-14): review /track, /pits, /incidents and /driver read from the file ----------
 * Live shapes: build_track_json, build_pits_json, build_incidents_json, build_driver_json. Contract clarification
 * (2026-10-14): every review /pits and /incidents item carries "t" = sim time in seconds (3 decimals). A value the file
 * does not hold is the sentinel 0. /weather stays 404 with the message of the contract. */
#define SA_RV_SNAP_S     2.0    /* s: a snapshot is the newest tick at or before t, no older than this */
#define SA_RV_TICK_NAME  16u

/* Newest tick of car slot at or before t (no older than SA_RV_SNAP_S): the values of ids[0..n-1], ok bit per id.
 * Returns 1 when a tick was found. Only the blocks around t are read (newest first). */
static int sa_rv_snap_ids(const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, uint32_t slot, double t,
                          const uint32_t *ids, uint32_t n, double *val, uint8_t *ok) {
    uint32_t bi = ix->nblocks;
    while (bi-- > 0) {
        const sv2_idx_block_t *ib = &ix->blocks[bi];
        int found = 0;
        if (ib->t_first > t + 1e-9) continue;
        if (ib->t_last < t - SA_RV_SNAP_S) break;
        if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) return 0;
        for (uint32_t c = 0; c < B->d.n_cars && !found; c++) {
            const sv2_car_t *car = &B->d.cars[c];
            if (car->slot != slot) continue;
            for (uint32_t i = car->count; i-- > 0 && !found;) {
                double tt = ib->t_first + (double)B->d.sim_ms[car->first_tick + i] / 1000.0;
                if (tt > t + 1e-9) continue;
                if (tt < t - SA_RV_SNAP_S) break;
                for (uint32_t k = 0; k < n; k++) {
                    uint32_t raw = 0;
                    ok[k] = sa_rv_raw(car, ids[k], i, &raw) ? 1 : 0;
                    val[k] = ok[k] ? sa_rv_real(ix, ids[k], raw) : 0.0;
                }
                found = 1;
            }
        }
        if (found) return 1;
    }
    return 0;
}

/* Sim time of the file's ticks before t, as a tick count (the tk of an incident item; approximate, see the record). */
static unsigned long long sa_rv_ticks_before(const sv2_idx_t *ix, double t) {
    unsigned long long n = 0;
    for (uint32_t i = 0; i < ix->nblocks && ix->blocks[i].t_last < t; i++) n += ix->blocks[i].ticks;
    return n;
}

/* Names of the review docs: sa_rv_names (below) builds the table, sa_rv_nm_name reads it. */

/* Pit stops as the live /pits items: an entry (state 1) and the next entry of the same car (state 0). Events with
 * sim time after tmax are left out; car < 0 lists every car. *first controls the comma. */
static void sa_rv_pit_items(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, const sa_rv_evs_t *E,
                            int car, double tmax, int *first, const sa_rv_nm_t *nmt) {
    static const uint32_t ids[3] = { 69u, 79u, 76u };   /* lap, fuel level, damage tier */
    char name[SA_RV_TXT + 16];
    for (uint32_t k = 0; k < E->n; k++) {
        const sa_rv_ev_t *in = &E->ev[k], *out = NULL;
        double vi[3] = { 0, 0, 0 }, vo[3] = { 0, 0, 0 };
        uint8_t oi[3] = { 0, 0, 0 }, oo[3] = { 0, 0, 0 };
        if (in->type != SA_RV_EV_PIT || in->state != 1 || in->t > tmax) continue;
        if (car >= 0 && (int)in->car != car) continue;
        for (uint32_t j = k + 1; j < E->n; j++) {
            const sa_rv_ev_t *e = &E->ev[j];
            if (e->type != SA_RV_EV_PIT || e->car != in->car) continue;
            if (e->state == 0 && e->t <= tmax) out = e;
            break;
        }
        sa_rv_snap_ids(ix, rd, B, in->car, in->t, ids, 3, vi, oi);
        if (out) sa_rv_snap_ids(ix, rd, B, in->car, out->t, ids, 3, vo, oo);
        sa_rv_nm_name(nmt, in->car, name, sizeof name);
        if (car < 0) {   /* /pits item: id, n and t (the contract clarification of 2026-10-14) */
            sa_f(b, "%s{\"id\":%u,\"n\":", *first ? "" : ",", in->car);
            sa_s(b, name);
            sa_f(b, ",");
        } else {         /* /driver pits: the live driver item has no id, n or t */
            sa_f(b, "%s{", *first ? "" : ",");
        }
        sa_f(b, "\"li\":%d,\"lo\":%d,\"d\":%.2f,\"boxdur\":0,\"reqdelay\":0,\"ti\":0,\"to\":0,\"fi\":%.1f,\"fo\":%.1f",
             oi[0] ? (int)vi[0] : 0, (out && oo[0]) ? (int)vo[0] : 0, out ? out->t - in->t : 0.0,
             oi[1] ? vi[1] : 0.0, (out && oo[1]) ? vo[1] : 0.0);
        if (car >= 0) sa_f(b, ",\"add\":%.1f", (out && oo[1] && oi[1]) ? vo[1] - vi[1] : 0.0);   /* driver form only */
        sa_f(b, ",\"rfw\":0,\"rrw\":0,\"dmg\":%d,\"done\":%d", oi[2] ? (int)vi[2] : 0, out ? 1 : 0);
        if (car < 0) sa_f(b, ",\"t\":%.3f}", in->t);
        else sa_f(b, "}");
        *first = 0;
    }
}

/* GET /pits (review): every pit stop of the recording (no time filter). */
static int sa_rv_pits_doc(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, const char *rid, const sa_rv_nm_t *nmt) {
    sa_rv_evs_t *E = (sa_rv_evs_t *)calloc(1, sizeof(sa_rv_evs_t));
    int first = 1;
    if (!E) return 0;
    if (!sa_rv_events_load(rd, B, E)) { free(E); return 0; }
    (void)rid;
    sa_f(b, "{\"gen\":0,\"pits\":[");
    sa_rv_pit_items(b, ix, rd, B, E, -1, 1e300, &first, nmt);
    sa_f(b, "]}");
    free(E->ev);
    free(E);
    return b->bad ? 0 : 1;
}

/* GET /incidents (review): every incident EVENT. Item: tk approximate tick count before the event (DERIVED), id, n,
 * ty = kind, lp = lap at the event, a = partner (contact) or track-limit count (limits), b = 0, t = sim time. */
static int sa_rv_incidents_doc(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, const char *rid,
                               const sa_rv_nm_t *nmt) {
    sa_rv_evs_t *E = (sa_rv_evs_t *)calloc(1, sizeof(sa_rv_evs_t));
    static const uint32_t ids[1] = { 69u };
    char name[SA_RV_TXT + 16];
    int first = 1;
    if (!E) return 0;
    if (!sa_rv_events_load(rd, B, E)) { free(E); return 0; }
    (void)rid;
    sa_f(b, "{\"gen\":0,\"ev\":[");
    for (uint32_t k = 0; k < E->n; k++) {
        const sa_rv_ev_t *e = &E->ev[k];
        double lap = 0.0;
        uint8_t ok = 0;
        if (e->type != SA_RV_EV_INC) continue;
        sa_rv_snap_ids(ix, rd, B, e->car, e->t, ids, 1, &lap, &ok);
        sa_rv_nm_name(nmt, e->car, name, sizeof name);
        sa_f(b, "%s{\"tk\":%llu,\"id\":%u,\"n\":", first ? "" : ",", sa_rv_ticks_before(ix, e->t), e->car);
        sa_s(b, name);
        sa_f(b, ",\"ty\":");
        sa_s(b, e->kind[0] ? e->kind : "incident");
        sa_f(b, ",\"lp\":%d,\"a\":%d,\"b\":0,\"t\":%.3f}", ok ? (int)lap : 0,
             e->with >= 0 ? e->with : (e->count >= 0 ? e->count : 0), e->t);
        first = 0;
    }
    sa_f(b, "]}");
    free(E->ev);
    free(E);
    return b->bad ? 0 : 1;
}

/* GET /track (review): the outline of one timed lap (REPLAY 2.1). wc = number of points (the live wc is the waypoint
 * count; the recording has no waypoints). */
static int sa_rv_track_doc(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B) {
    uint8_t player[SV2_MAX_CARS];
    uint32_t lap_k = 0, npts = 0;
    double (*pts)[2] = NULL;
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) player[s] = sa_rv_role_is(ix, s, "player");
    if (sa_rv_outline_lap(ix, player, &lap_k)) {
        pts = (double (*)[2])malloc(SA_RV_OUT_PTS * 2u * sizeof(double));
        if (pts) npts = sa_rv_outline(ix, rd, B, lap_k, pts);
    }
    sa_f(b, "{\"wc\":%u,\"pts\":[", npts);
    for (uint32_t i = 0; i < npts; i++) sa_f(b, "%s[%.1f,%.1f]", i ? "," : "", pts[i][0], pts[i][1]);
    sa_f(b, "]}");
    free(pts);
    return b->bad ? 0 : 1;
}

/* GET /driver?id=<slot>[&t=<s>] (review): the live dossier of one car at sim time t. Laps are the lap EVENTs up to t
 * (t = game lap time, s1 and s2 are 0, s2d and s3 are -1: not in the file, p and f are read at the lap end); pits as
 * /pits up to t. Position p and the name are read at t. */
static int sa_rv_driver_doc(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, uint32_t car, double t,
                            const sa_rv_nm_t *nmt) {
    sa_rv_evs_t *E = (sa_rv_evs_t *)calloc(1, sizeof(sa_rv_evs_t));
    static const uint32_t ids_p[2] = { 68u, 79u };   /* position, fuel level */
    double vp[2] = { 0, 0 };
    uint8_t op[2] = { 0, 0 };
    char name[SA_RV_TXT + 16];
    int first = 1;
    if (!E) return 0;
    if (!sa_rv_events_load(rd, B, E)) { free(E); return 0; }
    sa_rv_nm_name(nmt, car, name, sizeof name);
    sa_rv_snap_ids(ix, rd, B, car, t, ids_p, 2, vp, op);
    sa_f(b, "{\"gen\":0,\"id\":%u,\"n\":", car);
    sa_s(b, name);
    sa_f(b, ",\"cn\":");
    sa_s(b, nmt[car].team);
    sa_f(b, ",\"p\":%d,\"laps\":[", op[0] ? (int)vp[0] : 0);
    for (uint32_t k = 0; k < E->n; k++) {
        const sa_rv_ev_t *e = &E->ev[k];
        double vl[2] = { 0, 0 };
        uint8_t ol[2] = { 0, 0 };
        if (e->type != SA_RV_EV_LAP || e->car != car || e->t > t) continue;
        sa_rv_snap_ids(ix, rd, B, car, e->t, ids_p, 2, vl, ol);
        sa_f(b, "%s{\"l\":%u,\"t\":%.3f,\"s1\":0,\"s2\":0,\"s2d\":-1,\"s3\":-1,\"p\":%d,\"tc\":0,\"f\":%.1f,\"pit\":0,"
                "\"lf\":[0,0,0,0]}", first ? "" : ",", e->lap, e->last, ol[0] ? (int)vl[0] : 0, ol[1] ? vl[1] : 0.0);
        first = 0;
    }
    sa_f(b, "],\"pits\":[");
    first = 1;
    sa_rv_pit_items(b, ix, rd, B, E, (int)car, t, &first, nmt);
    sa_f(b, "]}");
    free(E->ev);
    free(E);
    return b->bad ? 0 : 1;
}

/* ---------- REPLAY 2.3 / TAPE 2.3 (2026-10-14): /replay/frame, one LIVE-shaped /telemetry frame at sim time t ----------
 * Same JSON as the live frame (json_telemetry_from, byte-identical for the same input: golden check in the record)
 * plus the review object of the synthesized frame and the tape object. Reads only the blocks around t (one pass
 * over them for all cars); never takes g_telemetry_cs and never writes g_telemetry (a scratch telemetry_t). */
#define SA_RV_NCH        125u   /* channel ids 0..124 (the catalog and the table max of the file) */

static double sa_rv_fr_val[SV2_MAX_CARS][SA_RV_NCH];
static uint8_t sa_rv_fr_ok[SV2_MAX_CARS][SA_RV_NCH];
static double sa_rv_fr_t[SV2_MAX_CARS];
static uint8_t sa_rv_fr_have[SV2_MAX_CARS], sa_rv_fr_scope[SV2_MAX_CARS];
static telemetry_t sa_rv_tp;              /* scratch frame state (HTTP thread only); g_telemetry is never touched */
static sa_rv_evs_t *sa_rv_fr_ev;          /* driver names of the last frame's session (cached by review id) */
static char sa_rv_fr_ev_id[32];

/* Newest tick at or before t of every car (no older than SA_RV_SNAP_S): all channels into sa_rv_fr_val/ok, read block by
 * block once. Returns the number of cars found. */
static uint32_t sa_rv_frame_snap(const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, double t) {
    uint32_t bi = ix->nblocks, found = 0;
    memset(sa_rv_fr_have, 0, sizeof sa_rv_fr_have);
    while (bi-- > 0) {
        const sv2_idx_block_t *ib = &ix->blocks[bi];
        if (ib->t_first > t + 1e-9) continue;
        if (ib->t_last < t - SA_RV_SNAP_S) break;
        if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, bi, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK) break;
        for (uint32_t c = 0; c < B->d.n_cars; c++) {
            const sv2_car_t *car = &B->d.cars[c];
            if (car->slot >= SV2_MAX_CARS || sa_rv_fr_have[car->slot]) continue;
            for (uint32_t i = car->count; i-- > 0;) {
                double tt = ib->t_first + (double)B->d.sim_ms[car->first_tick + i] / 1000.0;
                if (tt > t + 1e-9) continue;
                if (tt < t - SA_RV_SNAP_S) break;
                for (uint32_t k = 0; k < SA_RV_NCH; k++) {
                    uint32_t raw = 0;
                    sa_rv_fr_ok[car->slot][k] = sa_rv_raw(car, k, i, &raw) ? 1 : 0;
                    sa_rv_fr_val[car->slot][k] = sa_rv_fr_ok[car->slot][k] ? sa_rv_real(ix, k, raw) : 0.0;
                }
                sa_rv_fr_have[car->slot] = 1;
                sa_rv_fr_scope[car->slot] = (uint8_t)(car->scope == 1 ? 1 : 0);
                sa_rv_fr_t[car->slot] = tt;
                found++;
                break;
            }
        }
    }
    return found;
}

/* 1.2.4 (DVOA report 2026-10-17: qualifying replay drops cars that finished their runs). The writer records only cars on
 * track, so a car in its garage for good has no ticks after its last run and fell out of every frame. A car whose newest tick
 * in the whole file is older than t (index t_last) is added with that last state, held (sa_rv_fr_held: off track in the
 * frame). The last state of each car is read once per review id (one block per car) and cached. */
static double sa_rv_hold_val[SV2_MAX_CARS][SA_RV_NCH];
static uint8_t sa_rv_hold_ok[SV2_MAX_CARS][SA_RV_NCH], sa_rv_hold_have[SV2_MAX_CARS], sa_rv_hold_scope[SV2_MAX_CARS];
static char sa_rv_hold_id[32];
static uint8_t sa_rv_fr_held[SV2_MAX_CARS];

/* Pure: hold this car at t? It was seen, has no tick in the snapshot, and its newest tick is before t. */
static int sa_rv_hold_due(int any, int have_now, double t_last, double t) {
    return any && !have_now && t_last < t;
}

static void sa_rv_frame_hold(const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, double t, const char *rid) {
    memset(sa_rv_fr_held, 0, sizeof sa_rv_fr_held);
    if (strcmp(sa_rv_hold_id, rid)) {
        memset(sa_rv_hold_have, 0, sizeof sa_rv_hold_have);
        sv2_copy(sa_rv_hold_id, sizeof sa_rv_hold_id, rid);
    }
    for (uint32_t s = 0; s < SV2_MAX_CARS; s++) {
        const sv2_idx_car_t *z = &ix->car[s];
        if (!sa_rv_hold_due(z->any, sa_rv_fr_have[s], z->t_last, t) || z->last_block >= ix->nblocks) continue;
        if (!sa_rv_hold_have[s]) {
            if (sv2_idx_read_block_rd(rd, (sv2_idx_t *)ix, z->last_block, &B->d, B->stored, B->work, SV2_MAX_PAYLOAD) != SV2_OK)
                continue;
            for (uint32_t c = 0; c < B->d.n_cars; c++) {
                const sv2_car_t *car = &B->d.cars[c];
                if (car->slot != s || car->count == 0) continue;
                for (uint32_t k = 0; k < SA_RV_NCH; k++) {
                    uint32_t raw = 0;
                    sa_rv_hold_ok[s][k] = sa_rv_raw(car, k, car->count - 1u, &raw) ? 1 : 0;
                    sa_rv_hold_val[s][k] = sa_rv_hold_ok[s][k] ? sa_rv_real(ix, k, raw) : 0.0;
                }
                sa_rv_hold_scope[s] = (uint8_t)(car->scope == 1 ? 1 : 0);
                sa_rv_hold_have[s] = 1;
                break;
            }
            if (!sa_rv_hold_have[s]) continue;
        }
        memcpy(sa_rv_fr_val[s], sa_rv_hold_val[s], sizeof sa_rv_fr_val[s]);
        memcpy(sa_rv_fr_ok[s], sa_rv_hold_ok[s], sizeof sa_rv_fr_ok[s]);
        sa_rv_fr_scope[s] = sa_rv_hold_scope[s];
        sa_rv_fr_t[s] = z->t_last;
        sa_rv_fr_have[s] = 1;
        sa_rv_fr_held[s] = 1;
    }
}

/* Inverse of sw_car_from (storage_writer.inc): every recorded channel back to its car_telemetry_t field. A channel the
 * file does not hold leaves the field at 0 (the sentinel). Player-only channels (11..14 tread, 20..23 ttemp, 24..63 pch,
 * 93..96 tcarc) go to the car when the car is the player (scope 1) and to tp->tread / tp->pch_* for the session. The
 * AI tread is not invertible (clamped, sw_car_from) and stays 0. */
static void sa_rv_tape_map(telemetry_t *tp, uint32_t s, int player) {
    car_telemetry_t *c = &tp->cars[s];
    const double *v = sa_rv_fr_val[s];
    const uint8_t *ok = sa_rv_fr_ok[s];
    unsigned k;
#define RV_F(id, field) do { if (ok[id]) c->field = (float)v[id]; } while (0)
#define RV_I(id, field) do { if (ok[id]) c->field = (int)llround(v[id]); } while (0)
    memset(c, 0, sizeof *c);
    c->on_track = 1;
    c->ai_control = player ? 0 : 1;
    RV_F(1, lap_dist); RV_F(9, yaw); RV_F(10, yaw_rate); RV_F(15, rpm);
    RV_F(17, track_x); RV_F(18, track_z);
    RV_F(2, speed); RV_F(3, throttle); RV_F(4, brake); RV_F(5, steer);
    RV_F(6, glat); RV_F(7, glon); RV_F(8, gvert);
    RV_I(16, gear);
    for (k = 0; k < 4; k++) {
        if (player && ok[11 + k]) tp->tread[k] = (float)v[11 + k];
        if (player && ok[20 + k]) c->ttemp[k] = (float)v[20 + k];
        if (player && ok[93 + k]) c->tcarc[k] = (float)v[93 + k];
        if (ok[89 + k]) c->aws[k] = (float)v[89 + k];
        if (ok[102 + k]) c->rx_su[k] = (float)v[102 + k];
        if (ok[106 + k]) c->rx_ld[k] = (float)v[106 + k];
        if (ok[110 + k]) c->rx_sl[k] = (float)v[110 + k];
        if (ok[114 + k]) c->rx_fo[k] = (float)v[114 + k];
    }
    if (ok[87]) c->abt[0] = (float)v[87];
    if (ok[88]) c->abt[1] = (float)v[88];
    RV_F(64, tire_wear_fl); RV_F(65, tire_wear_fr); RV_F(66, tire_wear_rl); RV_F(67, tire_wear_rr);
    RV_I(68, position); RV_I(69, lap_number); RV_I(70, pitting); RV_I(71, call_to_box);
    RV_I(72, tire_choice); RV_I(73, track_limits); RV_I(74, fw_flag); RV_I(75, rw_flag); RV_I(76, dmg_tier);
    RV_F(77, oil_temp); RV_F(78, water_temp); RV_F(79, fuel_level);
    RV_F(80, gap_ahead_m); RV_F(81, gap_behind_m); RV_I(82, gap_ahead_id); RV_I(83, gap_behind_id);
    RV_F(84, live_sector_delta); RV_F(85, last_sector_delta); RV_F(86, last_lap);
    RV_I(97, race_num); RV_I(98, controller_role); RV_I(99, car_valid); RV_I(100, vf6); RV_I(101, contact_with);
    /* TAPE2 e2e (Claude 2026-10-15): a remote human (channel 98 = 2) is not AI; same rule as sa_rv_role_of, so frame
     * badges and the CO-OP footer agree with /replay/info. ai_control itself is not recorded. */
    if (ok[98] && c->controller_role == CTRL_REMOTE) c->ai_control = 0;
    RV_I(118, penalties); RV_F(119, fuel_avg_per_lap); RV_F(120, fuel_autonomy_laps);
    RV_I(121, box_soon); RV_I(122, finish_state); RV_I(123, driver_status); RV_I(124, reason_code);
    if (player && ok[24]) {   /* player chassis: pch_valid 1 while the channels are present (sw_car_from) */
        tp->pch_valid = 1;
        for (k = 0; k < 4; k++) {
            if (ok[24 + k]) tp->pch_ride_mm[k] = (float)v[24 + k];
            if (ok[28 + k]) tp->pch_susp_mm[k] = (float)v[28 + k];
            if (ok[32 + k]) tp->pch_tdefl_mm[k] = (float)v[32 + k];
            if (ok[36 + k]) tp->pch_press[k] = (float)v[36 + k];
            if (ok[40 + k]) tp->pch_grip[k] = (float)v[40 + k];
            if (ok[44 + k]) tp->pch_camber_deg[k] = (float)v[44 + k];
            if (ok[48 + k]) tp->pch_spin[k] = (float)v[48 + k];
            if (ok[52 + k]) tp->pch_disc_mm[k] = (float)v[52 + k];
            if (ok[56 + k]) tp->pch_bend[k] = (float)v[56 + k];
        }
        if (ok[60]) tp->pch_bottom = (int)llround(v[60]);
        if (ok[61]) tp->pch_compound = (int)llround(v[61]);
        if (ok[62]) tp->pch_fuel = (float)v[62];
        if (ok[63]) tp->pch_rpm = (float)v[63];
    }
#undef RV_F
#undef RV_I
}

/* Driver names of the session (driver EVENTs, cached by review id): NULL when the file cannot be read. */
static sa_rv_evs_t *sa_rv_names_for(const char *rid, sv2_rd_t *rd, sa_rv_bufs_t *B) {
    if (sa_rv_fr_ev && !strcmp(sa_rv_fr_ev_id, rid)) return sa_rv_fr_ev;
    if (sa_rv_fr_ev) { free(sa_rv_fr_ev->ev); free(sa_rv_fr_ev); sa_rv_fr_ev = NULL; }
    sa_rv_fr_ev = (sa_rv_evs_t *)calloc(1, sizeof(sa_rv_evs_t));
    if (!sa_rv_fr_ev) return NULL;
    if (!sa_rv_events_load(rd, B, sa_rv_fr_ev)) { free(sa_rv_fr_ev); sa_rv_fr_ev = NULL; return NULL; }
    sv2_copy(sa_rv_fr_ev_id, sizeof sa_rv_fr_ev_id, rid);
    return sa_rv_fr_ev;
}

/* Name table of a review id (TAPE2 N1; rule order in sa_rv_nm_t). Cached per review id AND stem: the stem sets the season.
 * The driver EVENTs come from sa_rv_names_for (cached too); the race number is the newest channel 97 value of each car. */
static char sa_rv_nm_id[24], sa_rv_nm_stem[200];
static int sa_rv_nm_have = 0;
static sa_rv_nm_t sa_rv_nm_tab[SV2_MAX_CARS];

static const sa_rv_nm_t *sa_rv_names(const char *rid, const char *stem, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B) {
    double num_v[SV2_MAX_CARS];
    uint8_t num_ok[SV2_MAX_CARS];
    sa_rv_evs_t *E;
    int year;
    if (sa_rv_nm_have && !strcmp(sa_rv_nm_id, rid) && !strcmp(sa_rv_nm_stem, stem)) return sa_rv_nm_tab;
    E = sa_rv_names_for(rid, rd, B);   /* NULL when the EVENTs cannot be read: the names then come from the .veh files and the race numbers */
    year = vehn_stem_year(stem);
    memset(num_v, 0, sizeof num_v);
    memset(num_ok, 0, sizeof num_ok);
    sa_rv_newest(ix, rd, B, 97u, num_v, num_ok, ix->nblocks);     /* race number of each car: newest first, the whole file if a car is missing */
    for (unsigned s = 0; s < SV2_MAX_CARS; s++) {
        sa_rv_nm_t *o = &sa_rv_nm_tab[s];
        const sa_rv_drv_t *d = E ? &E->drv[s] : NULL;
        memset(o, 0, sizeof *o);
        o->num = (d && d->have) ? d->num : (num_ok[s] ? (int32_t)num_v[s] : -1);
        if (d && d->have) {
            sv2_copy(o->name, sizeof o->name, d->name);
            sv2_copy(o->team, sizeof o->team, d->team);
            o->src = SA_NM_EVENT;
        } else if (o->num > 0 && vehn_lookup(year, (int)o->num, o->name, sizeof o->name, o->team, sizeof o->team)) {
            o->src = SA_NM_VEH;
        } else if (o->num > 0) {
            snprintf(o->name, sizeof o->name, "#%ld", (long)o->num);
            o->src = SA_NM_NUM;
        } else {
            snprintf(o->name, sizeof o->name, "Car %u", s);
            o->src = SA_NM_CAR;
        }
    }
    sv2_copy(sa_rv_nm_id, sizeof sa_rv_nm_id, rid);
    sv2_copy(sa_rv_nm_stem, sizeof sa_rv_nm_stem, stem);
    sa_rv_nm_have = 1;
    return sa_rv_nm_tab;
}

/* GET /replay/frame?t=<s> (REPLAY 2.3): the LIVE /telemetry frame of the recording at sim time t (already clamped). */
static int sa_rv_frame_tape(sa_buf_t *b, const sv2_idx_t *ix, sv2_rd_t *rd, sa_rv_bufs_t *B, const sv2_header_t *h,
                            const char *rid, const char *stem, double t, double tf, double tl, const sa_rv_nm_t *nmt) {
    static char nm[MAX_CARS][NAME_LEN], cn[MAX_CARS][NAME_LEN];
    static int num[MAX_CARS], zero[MAX_CARS];
    telemetry_t *tp = &sa_rv_tp;
    sa_rv_evs_t *E;
    unsigned gen = 2166136261u, seq = 0;
    unsigned long long ticks = 0;
    double ts = 0.0, te = 0.0;
    uint32_t s, cnt = 0;
    int n, player_slot = 0;
    const char *q;
    /* session_generation 0 (Claude review 2026-10-14): the review trace docs carry gen 0 and the viewer drops seeded laps whose gen
     * differs from the frame's; a per-id hash broke every analysis page in review. Same value as sa_rv_frame_rest. */
    gen = 0u;
    (void)q;
    memset(tp, 0, sizeof *tp);
    memset(zero, 0, sizeof zero);
    sa_rv_span(ix, &ts, &te, &ticks);
    sa_rv_frame_snap(ix, rd, B, t);
    sa_rv_frame_hold(ix, rd, B, t, rid);                   /* 1.2.4: cars that left the track for good stay in the frame */
    seq = (unsigned)sa_rv_ticks_before(ix, t);            /* sample_sequence: approximate tick index (DERIVED) */
    E = sa_rv_names_for(rid, rd, B);
    tp->session_type = h->session_type;
    sv2_copy(tp->track_name, sizeof tp->track_name, h->track);
    tp->session_time = t;
    tp->tick = (DWORD)seq;
    for (s = 0; s < SV2_MAX_CARS; s++) {
        if (!sa_rv_fr_have[s]) continue;
        sa_rv_tape_map(tp, s, sa_rv_fr_scope[s]);
        if (sa_rv_fr_held[s]) {   /* 1.2.4: held (no ticks since t_last): in its garage or out; stopped */
            tp->cars[s].on_track = 0;
            tp->cars[s].speed = 0.0f; tp->cars[s].throttle = 0.0f; tp->cars[s].brake = 0.0f; tp->cars[s].rpm = 0.0f;
            if (tp->cars[s].pitting < 2) tp->cars[s].pitting = 4;   /* garage, as the game reads it in a garage (ch 70 = 4) */
        }
        if (sa_rv_fr_scope[s] == 1) player_slot = (int)s;
        cnt = s + 1;
        sv2_copy(nm[s], NAME_LEN, nmt[s].name);
        sv2_copy(cn[s], NAME_LEN, nmt[s].team);
        num[s] = (int)nmt[s].num;
        sv2_copy(tp->cars[s].name, NAME_LEN, nm[s]);
        sv2_copy(tp->cars[s].car_name, NAME_LEN, cn[s]);
        tp->cars[s].race_num = num[s];
    }
    /* Claude follow-up (REPLAY 2.3): tt = total race time at the car's last line crossing, i.e. the sim time of its newest
     * lap EVENT at or before t minus t_first (the first tick of the file; the session_start EVENT is not read); bl = best
     * timed lap (min last_lap_s > 0) of its lap EVENTs up to t. 0 when none. DERIVED from the cached EVENT list. Live tt is
     * total_time, veh+0x4098 (web_telemetry.c OFF_TOTAL_TIME_SF, read at line ~5308). */
    {
        double lastt[SV2_MAX_CARS], best[SV2_MAX_CARS];
        uint8_t has[SV2_MAX_CARS];
        memset(lastt, 0, sizeof lastt);
        memset(best, 0, sizeof best);
        memset(has, 0, sizeof has);
        if (E) for (uint32_t k = 0; k < E->n && E->ev[k].t <= t + 1e-9; k++) {
            const sa_rv_ev_t *e = &E->ev[k];
            if (e->type != SA_RV_EV_LAP || e->car >= SV2_MAX_CARS) continue;
            if (!has[e->car] || e->t > lastt[e->car]) lastt[e->car] = e->t;
            has[e->car] = 1;
            if (e->last > 0.0 && (best[e->car] == 0.0 || e->last < best[e->car])) best[e->car] = e->last;
        }
        for (s = 0; s < SV2_MAX_CARS; s++) {
            if (!sa_rv_fr_have[s]) continue;
            tp->cars[s].total_time = has[s] ? (float)(lastt[s] - tf) : 0.0f;
            tp->cars[s].best_lap = (float)best[s];
        }
    }
    tp->vehicle_count = (int)cnt;
    tp->player_slot = player_slot;
    tp->player_index = player_slot;
    tyre_refresh_names((int)cnt, nm, cn, num);
    n = json_telemetry_from(b->p + b->n, (int)(b->cap - b->n), tp, seq, gen, 0ul, 0, zero);
    if (n <= 1 || b->p[b->n + (size_t)n - 1] != '}') { b->bad = 1; return 0; }
    b->n += (size_t)n - 1;   /* the final brace is replaced by the two review objects */
    sa_f(b, ",\"review\":{\"session\":");
    sa_s(b, rid);
    sa_f(b, ",\"stem\":");
    sa_s(b, stem);
    sa_f(b, ",\"ticks\":%llu,\"end_t\":%.3f},\"tape\":{\"session\":", ticks, tl);
    sa_s(b, rid);
    sa_f(b, ",\"t\":%.3f,\"t_first\":%.3f,\"t_last\":%.3f}}", t, tf, tl);
    return b->bad ? 0 : 1;
}

/* Routes the review adapter answers (the 1.0.1 review routes, and the ones refused for recordings). */
static int sa_rv_is_api_route(const char *route) {
    static const char *const routes[] = {
        "/laps", "/trace/status", "/trace/select", "/trace/data", "/telemetry", "/pits", "/incidents",
        "/weather", "/track", "/driver", "/trace/open", "/trace/compare", "/trace/export",
        "/replay", "/replay/info", "/replay/frame"
    };
    for (size_t i = 0; i < sizeof routes / sizeof routes[0]; i++)
        if (!strcmp(route, routes[i])) return 1;
    return 0;
}

/* Review id checks for every adapter route: 404 unknown or deleted, 409 still recording. */
static int sa_rv_id_check(const char *rid, const char **msg) {
    char folder[120], file[180];
    int is_open = 0;
    if (!sa_cs_ready) sa_init();
    if (!sa_rid_id_ok(rid) || !sa_rid_find(rid, folder, sizeof folder, file, sizeof file, &is_open) ||
        !sa_file_exists(folder, file)) {
        *msg = "unknown or deleted session";
        return 404;
    }
    if (is_open) { *msg = "session is still recording"; return 409; }
    return 0;
}

/* Index gate for the adapter: 0 with a READY slot; 503 while the index builds (never waits on it; starts the build
 * when none runs); 404 when the build failed. */
static int sa_rv_ready(const char *rid, int *slot_out, const char **msg) {
    char folder[120], file[180], path[SW_PATHBUF];
    int is_open = 0, slot, state;
    if (!sa_rid_find(rid, folder, sizeof folder, file, sizeof file, &is_open)) { *msg = "unknown or deleted session"; return 404; }
    sw_fmt(path, sizeof path, "%s%s\\%s", sw_rec_dir, folder, file);
    if (!rv_cs_ready) { InitializeCriticalSection(&rv_cs); rv_cs_ready = 1; }
    EnterCriticalSection(&rv_cs);
    slot = sa_rev_pick_locked(rid, folder, file, path);
    state = slot >= 0 ? sa_rev[slot].state : SA_REV_NONE;
    LeaveCriticalSection(&rv_cs);
    if (slot < 0 || state == SA_REV_BUILDING || state == SA_REV_NONE) { *msg = "review index is building"; return 503; }
    if (state == SA_REV_ERROR) { *msg = "review index could not be built"; return 404; }
    *slot_out = slot;
    return 0;
}

/* Answers one review route for a v2 id: returns the HTTP code and the body in out, or -1 when route is not a review
 * route (the console page, which then loads normally). car is 1000+slot (-1 when absent); lap is the raw lap number. */
static int sa_api_review_route(const char *route, const char *rid, int car, int lap, const char *q_id, const char *q_t,
                               const char *q_t0, const char *q_t1, char *out, size_t cap, size_t *len) {
    sa_buf_t b = { out, cap, 0, 0 };
    const char *msg = "";
    int code = 0, slot = -1;
    sv2_rd_t rd;
    HANDLE h = INVALID_HANDLE_VALUE;
    sa_rv_bufs_t B;
    sv2_header_t hdr;
    char stem[200];
    const sa_rv_nm_t *nmt = NULL;
    int32_t pos[SV2_MAX_CARS];
    uint8_t pos_ok[SV2_MAX_CARS], ai[SV2_MAX_CARS], ai_ok[SV2_MAX_CARS];
    if (!sa_rv_is_api_route(route)) return -1;
    /* 2026-10-14 (Claude review): the console asks /trace/data?car=<slot> like the live route (http_car_select maps a
     * slot to 1000+slot); this adapter took only the raw 1000+slot, so every v2 review seed got 400. Accept both. */
    if (car >= 0 && car < SA_RV_CAR_BASE) car += SA_RV_CAR_BASE;
    code = sa_rv_id_check(rid, &msg);
    if (!code && (!strcmp(route, "/trace/open") || !strcmp(route, "/trace/compare") || !strcmp(route, "/trace/export"))) {
        code = 404; msg = "not available for a recorded session";
    } else if (!code && !strcmp(route, "/weather")) {
        code = 404; msg = "weather is not recorded in this recording";
    } else if (!code && !strcmp(route, "/trace/data") && (car < SA_RV_CAR_BASE || car > SA_RV_CAR_BASE + 103 || lap < 0)) {
        code = 400; msg = "car=1000+slot and lap=N are required";
    }
    if (!code) code = sa_rv_ready(rid, &slot, &msg);
    if (code) { sa_err(&b, code, msg); *len = b.n; return code; }
    if (!sa_rv_open(&sa_rev[slot], &h, &rd)) { sa_err(&b, 404, "unknown or deleted session"); *len = b.n; return 404; }
    if (!sa_rv_bufs_alloc(&B)) { CloseHandle(h); sa_err(&b, 500, "out of memory"); *len = b.n; return 500; }
    if (sv2_rd_header(&rd, &hdr) != SV2_OK) {
        sa_rv_bufs_free(&B);
        CloseHandle(h);
        sa_err(&b, 500, "the recording header could not be read");
        *len = b.n;
        return 500;
    }
    memset(pos, 0, sizeof pos); memset(pos_ok, 0, sizeof pos_ok);
    memset(ai, 0, sizeof ai); memset(ai_ok, 0, sizeof ai_ok);
    sv2_copy(stem, sizeof stem, sa_rev[slot].file);
    if (strlen(stem) > 4 && !strcmp(stem + strlen(stem) - 4, ".f1t")) stem[strlen(stem) - 4] = '\0';
    nmt = sa_rv_names(rid, stem, &sa_rev[slot].ix, &rd, &B);   /* TAPE2 N1: the names of this review, one table */
    if (!strcmp(route, "/replay/info")) {   /* REPLAY 2.1 */
        if (sa_rv_info_doc(&b, &sa_rev[slot].ix, &rd, &B, &hdr, rid, stem, nmt)) code = 200;
        else { sa_err(&b, 500, "the recording could not be read"); code = 500; }
    } else if (!strcmp(route, "/replay")) {   /* REPLAY 2.2: window, 30 s at most, clamped to the file span */
        double t0 = 0.0, t1 = 0.0, tf = 0.0, tl = 0.0;
        unsigned long long nticks = 0;
        const char *why = "";
        sa_rv_span(&sa_rev[slot].ix, &tf, &tl, &nticks);
        code = sa_rv_window_args(q_t0, q_t1, tf, tl, &t0, &t1, &why);
        if (code) sa_err(&b, code, why);
        else if (sa_rv_window_doc(&b, &sa_rev[slot].ix, &rd, &B, rid, t0, t1)) code = 200;
        else { sa_err(&b, 500, "the recording could not be read"); code = 500; }
    } else if (!strcmp(route, "/replay/frame")) {   /* REPLAY 2.3 / TAPE 2.3: the frame at t (clamped to the span) */
        double tv = 0.0, tf = 0.0, tl = 0.0;
        unsigned long long nt = 0;
        sa_rv_span(&sa_rev[slot].ix, &tf, &tl, &nt);
        if (!sa_rv_parse_num(q_t, &tv)) {
            code = 400; sa_err(&b, 400, "t must be a finite number of sim seconds");
        } else {
            if (tv < tf) tv = tf;
            if (tv > tl) tv = tl;
            if (sa_rv_frame_tape(&b, &sa_rev[slot].ix, &rd, &B, &hdr, rid, stem, tv, tf, tl, nmt)) code = 200;
            else { sa_err(&b, 500, "the recording could not be read"); code = 500; }
        }
    } else if (!strcmp(route, "/track")) {   /* REPLAY 2.2 */
        if (sa_rv_track_doc(&b, &sa_rev[slot].ix, &rd, &B)) code = 200;
        else { sa_err(&b, 500, "the recording could not be read"); code = 500; }
    } else if (!strcmp(route, "/pits") || !strcmp(route, "/incidents")) {   /* REPLAY 2.2 / TAPE 2.2: items carry t */
        int ok = !strcmp(route, "/pits") ? sa_rv_pits_doc(&b, &sa_rev[slot].ix, &rd, &B, rid, nmt)
                                         : sa_rv_incidents_doc(&b, &sa_rev[slot].ix, &rd, &B, rid, nmt);
        if (ok) code = 200;
        else { sa_err(&b, 500, "the recording could not be read"); code = 500; }
    } else if (!strcmp(route, "/driver")) {   /* REPLAY 2.2: id=<slot>, t=<sim s> (default: the last tick) */
        double idv = 0.0, tv = 0.0, tf = 0.0, tl = 0.0;
        unsigned long long nt = 0;
        sa_rv_span(&sa_rev[slot].ix, &tf, &tl, &nt);
        if (!sa_rv_parse_num(q_id, &idv) || idv < 0.0 || idv >= (double)SV2_MAX_CARS || idv != floor(idv)) {
            code = 400; sa_err(&b, 400, "id=<car slot 0..103> is required");
        } else if (!sa_rev[slot].ix.car[(unsigned)idv].seen) {
            code = 404; sa_err(&b, 404, "car not recorded in this recording");
        } else if (q_t && !sa_rv_parse_num(q_t, &tv)) {
            code = 400; sa_err(&b, 400, "t must be a finite number");
        } else {
            if (!q_t) tv = tl;
            if (tv < tf) tv = tf;
            if (tv > tl) tv = tl;
            if (sa_rv_driver_doc(&b, &sa_rev[slot].ix, &rd, &B, (uint32_t)idv, tv, nmt)) code = 200;
            else { sa_err(&b, 500, "the recording could not be read"); code = 500; }
        }
    } else if (!strcmp(route, "/laps")) {
        sa_rv_scan_cars(&sa_rev[slot].ix, &rd, &B, pos, pos_ok, ai, ai_ok);
        sa_rv_laps_doc(&b, &sa_rev[slot].ix, rid, pos, pos_ok, nmt);
        code = b.bad ? 500 : 200;
    } else if (!strcmp(route, "/trace/status") || !strcmp(route, "/trace/select")) {
        sa_rv_scan_cars(&sa_rev[slot].ix, &rd, &B, pos, pos_ok, ai, ai_ok);
        sa_rv_status_doc(&b, &sa_rev[slot].ix, rid, ai, ai_ok, nmt);
        code = b.bad ? 500 : 200;
    } else if (!strcmp(route, "/trace/data")) {
        code = sa_rv_trace_doc(&b, &sa_rev[slot].ix, &rd, &B, rid, (uint32_t)(car - SA_RV_CAR_BASE), (uint32_t)lap, hdr.track, nmt);
    } else {   /* /telemetry */
        sa_rv_scan_cars(&sa_rev[slot].ix, &rd, &B, pos, pos_ok, ai, ai_ok);
        sa_rv_frame_doc(&b, &sa_rev[slot].ix, &hdr, rid, stem, pos, pos_ok, nmt);
        code = b.bad ? 500 : 200;
    }
    sa_rv_bufs_free(&B);
    CloseHandle(h);
    *len = b.n;
    return code;
}

/* ---------- POST /recordings/load (selection only; an empty name unloads) ---------- */
static int sa_api_load(const char *body, size_t n, sa_buf_t *o) {
    static sa_file_t list[SJ_MAX_PATHS];
    int i, cnt;
    unsigned sessions = 0;
    if (!sa_parse(body, n)) return sa_err(o, 400, "The body must be a JSON object: {\"recording\": \"<name>\"}.");
    i = sa_find("recording");
    if (i < 0 || sa_req.type[i] != 1) return sa_err(o, 400, "The body must contain \"recording\" as a string.");
    if (!sa_req.str[i][0]) {
        if (sj_cs_ready) EnterCriticalSection(&sj_cs);
        sj_loaded[0] = 0;
        if (sj_cs_ready) LeaveCriticalSection(&sj_cs);
        o->n = 0;
        sa_f(o, "{\"loaded\":null,\"sessions\":0,\"note\":\"Nothing is loaded now. Nothing was changed.\"}");
        return 200;
    }
    if (!sa_name_ok(sa_req.str[i])) return sa_err(o, 422, "Invalid recording name.");
    if (!sa_folder_exists(sa_req.str[i])) return sa_err(o, 404, "Unknown recording. Reload the list.");
    cnt = sa_list_folder(sa_req.str[i], list, SJ_MAX_PATHS);
    for (int a = 0; a < cnt; a++) {   /* sessions = distinct stems of .f1t and .f1a files */
        char stem_a[SJ_NAME], stem_b[SJ_NAME];
        int ka = sa_kind(list[a].name, stem_a, sizeof stem_a), dup = 0;
        if (ka != SA_K_WORK && ka != SA_K_ARCH) continue;
        for (int b = 0; b < a && !dup; b++) {
            int kb = sa_kind(list[b].name, stem_b, sizeof stem_b);
            dup = (kb == SA_K_WORK || kb == SA_K_ARCH) && !strcmp(stem_a, stem_b);
        }
        if (!dup) sessions++;
    }
    if (sj_cs_ready) EnterCriticalSection(&sj_cs);
    sv2_copy(sj_loaded, sizeof sj_loaded, sa_req.str[i]);
    if (sj_cs_ready) LeaveCriticalSection(&sj_cs);
    sa_f(o, "{\"loaded\":");
    sa_s(o, sa_req.str[i]);
    sa_f(o, ",\"sessions\":%u,\"note\":", sessions);
    {
        char note[300];
        sw_fmt(note, sizeof note, "Loaded: %s. Loading does not change any file.", sa_req.str[i]);
        sa_s(o, note);
    }
    sa_f(o, "}");
    return 200;
}

/* ---------- POST /recordings/archive ---------- */
static int sa_api_archive(const char *body, size_t n, sa_buf_t *o) {
    static char fold[SA_LIST][120], fname[SA_LIST][SJ_NAME];
    const char *folders[SA_LIST], *files[SA_LIST];
    unsigned cnt, seq = 0;
    int i, rc, recycle = 0, slot;
    char why[SJ_MSG];
    if (!sa_parse(body, n)) return sa_err(o, 400, "The body must be a JSON object: {\"sessions\": [\"<recording>/<session>\"], \"recycle_working\": false}.");
    i = sa_find("sessions");
    if (i < 0 || sa_req.type[i] != 4 || sa_req.cnt[i] == 0 || sa_req.cnt[i] > SA_LIST)
        return sa_err(o, 422, "\"sessions\" must be a list of 1 to 8 \"<recording>/<session>\" ids.");
    cnt = sa_req.cnt[i];
    rc = sa_find("recycle_working");
    if (rc >= 0) {
        if (sa_req.type[rc] != 3) return sa_err(o, 422, "\"recycle_working\" must be true or false.");
        recycle = sa_req.flag[rc];
    }
    if (!sv2_alg_available(SV2_COMP_LZMS))
        return sa_err(o, 503, "Archive needs the Windows compression feature (LZMS) on this PC, and it is not available. Nothing was changed.");
    for (unsigned k = 0; k < cnt; k++) {
        char *item = sa_req.list[i][k], *slash = strchr(item, '/');
        char stem[SJ_NAME];
        if (!slash || strchr(slash + 1, '/')) return sa_err(o, 422, "Each session id must be \"<recording>/<session>\".");
        *slash = 0;
        sv2_copy(stem, sizeof stem, slash + 1);
        if (!sa_name_ok(item) || !sa_name_ok(stem)) return sa_err(o, 422, "Invalid recording or session name.");
        if (!sa_folder_exists(item)) return sa_err(o, 404, "Unknown recording. Reload the list.");
        {
            char f1t[SJ_NAME], part[SJ_NAME];
            sw_fmt(f1t, sizeof f1t, "%s.f1t", stem);
            sw_fmt(part, sizeof part, "%s.f1t.part", stem);
            if (!sa_file_exists(item, f1t)) {
                if (sa_file_exists(item, part)) return sa_err(o, 409, "The session is still open or was not closed. Nothing was archived.");
                return sa_err(o, 404, "Unknown session. Reload the list.");
            }
            if (sa_open_session(item, stem)) return sa_err(o, 409, "The session is still open. It can be archived after it ends.");
            sv2_copy(fold[k], sizeof fold[k], item);
            sv2_copy(fname[k], sizeof fname[k], f1t);
        }
        folders[k] = fold[k];
        files[k] = fname[k];
    }
    if (recycle && !sj_recycle_on(sw_rec_dir, why, sizeof why)) return sa_err(o, 503, why);
    slot = sj_submit(SJ_JOB_ARCHIVE, cnt, folders, files, 0.0, recycle, &seq);
    if (slot < 0) return sa_err(o, 503, "The job queue is full. Wait for the running jobs to finish, then try again.");
    o->n = 0;
    sa_f(o, "{\"job\":\"archive-%u\",\"kind\":\"archive\",\"state\":\"queued\",\"items\":%u,\"recycle_working\":%s}",
         seq, cnt, recycle ? "true" : "false");
    return 202;
}

/* ---------- POST /recordings/delete (Windows Recycle Bin only; size confirmed; re-checked on disk) ---------- */
static int sa_api_delete(const char *body, size_t n, sa_buf_t *o) {
    static char fold[SJ_MAX_PATHS][120], fname[SJ_MAX_PATHS][SJ_NAME];
    static sa_file_t list[SJ_MAX_PATHS];
    const char *folders[SJ_MAX_PATHS], *files[SJ_MAX_PATHS];
    const char *target, *id, *rec, *folder;
    unsigned cnt = 0;
    double total = 0.0, confirm;
    int ti, ii, ri, ci, nl, recycle_ok;
    char why[SJ_MSG], stem[SJ_NAME];
    unsigned seq = 0;
    int slot;
    if (!sa_parse(body, n)) return sa_err(o, 400, "The body must be a JSON object with target, id, recording and confirm_bytes.");
    ti = sa_find("target");
    ii = sa_find("id");
    ri = sa_find("recording");
    ci = sa_find("confirm_bytes");
    if (ti < 0 || sa_req.type[ti] != 1 || ii < 0 || sa_req.type[ii] != 1 || ci < 0 || sa_req.type[ci] != 2)
        return sa_err(o, 400, "The body needs \"target\" (recording, session or file), \"id\", \"confirm_bytes\" and, for session or file, \"recording\".");
    target = sa_req.str[ti];
    id = sa_req.str[ii];
    confirm = sa_req.num[ci];
    if (!strcmp(target, "recording")) {
        folder = id;
        stem[0] = 0;
    } else if (!strcmp(target, "session") || !strcmp(target, "file")) {
        if (ri < 0 || sa_req.type[ri] != 1) return sa_err(o, 400, "\"recording\" is needed for a session or file.");
        folder = sa_req.str[ri];
    } else {
        return sa_err(o, 422, "\"target\" must be recording, session or file.");
    }
    rec = folder;
    if (!sa_name_ok(id) || !sa_name_ok(rec)) return sa_err(o, 422, "Invalid name.");
    if (!sa_folder_exists(rec)) return sa_err(o, 404, "Unknown recording. Reload the list.");
    if (sa_is_loaded(rec)) return sa_err(o, 409, "The recording is loaded. Load an empty recording name to unload it, then delete.");
    nl = sa_list_folder(rec, list, SJ_MAX_PATHS - 1);
    if (nl < 0) return sa_err(o, 422, "This recording has too many files to delete in one step. Delete one session at a time.");
    for (int k = 0; k < nl; k++) {
        int kind = sa_kind(list[k].name, stem, sizeof stem);
        int take;
        if (!strcmp(target, "recording")) take = 1;
        else if (!strcmp(target, "session")) take = kind != SA_K_NONE && !strcmp(stem, id);
        else take = kind != SA_K_NONE && !strcmp(list[k].name, id);
        if (!take) continue;
        if (kind == SA_K_PART && sa_open_session(rec, stem)) return sa_err(o, 409, "The session is open. An open session cannot be deleted.");
        sv2_copy(fold[cnt], sizeof fold[cnt], rec);
        sv2_copy(fname[cnt], sizeof fname[cnt], list[k].name);
        total += list[k].bytes;
        cnt++;
    }
    if (!strcmp(target, "recording")) {
        if (sa_open_session(rec, NULL)) return sa_err(o, 409, "The recording has an open session. It can be deleted after the session ends.");
        sv2_copy(fold[cnt], sizeof fold[cnt], rec);
        fname[cnt][0] = 0;   /* the recording folder itself, after its files */
        cnt++;
    }
    if (!cnt) return sa_err(o, 404, "Nothing matches that id. Reload the list.");
    if (fabs(total - confirm) > 0.5) return sa_err(o, 409, "The recording size changed since the list was loaded. Reload the list and confirm again.");
    recycle_ok = sj_recycle_on(sw_rec_dir, why, sizeof why);
    if (!recycle_ok) return sa_err(o, 503, why);
    for (unsigned k = 0; k < cnt; k++) { folders[k] = fold[k]; files[k] = fname[k]; }
    slot = sj_submit(SJ_JOB_DELETE, cnt, folders, files, total, 0, &seq);
    if (slot < 0) return sa_err(o, 503, "The job queue is full. Wait for the running jobs to finish, then try again.");
    o->n = 0;
    sa_f(o, "{\"job\":\"delete-%u\",\"kind\":\"delete\",\"state\":\"queued\",\"items\":%u,\"bytes\":%.0f}", seq, cnt, total);
    return 202;
}

/* Dispatch for POST /recordings/<action> after the HTTP guards. Returns the status code; the JSON is in out. */
static int sa_api_action(const char *action, const char *body, size_t n, char *out, size_t cap, size_t *len) {
    sa_buf_t o = { out, cap, 0, 0 };
    int code;
    if (!strcmp(action, "load")) code = sa_api_load(body, n, &o);
    else if (!sw_enabled) code = sa_err(&o, 503, "Telemetry storage is off on this PC. Nothing was changed.");
    else if (!strcmp(action, "archive")) code = sa_api_archive(body, n, &o);
    else if (!strcmp(action, "delete")) code = sa_api_delete(body, n, &o);
    else code = sa_err(&o, 404, "Unknown action.");
    if (o.bad) code = sa_err(&o, 500, "The response was too large.");
    *len = o.n;
    return code;
}

/* /health storage object (spec 8.4, without absolute paths). Returns the length or -1. */
static int sa_health_json(char *buf, size_t cap) {
    sa_buf_t b = { buf, cap, 0, 0 };
    char open_name[160] = "";
    int over = 0;
    if (!sa_cs_ready) return -1;
    EnterCriticalSection(&sa_cs);
    over = sa_pub_over;
    sv2_copy(open_name, sizeof open_name, sa_pub_open);
    LeaveCriticalSection(&sa_cs);
    sa_f(&b, "{\"enabled\":%s,\"dir\":\"recordings\",\"compression\":\"%s\",\"recording\":", sw_enabled ? "true" : "false", sa_pub_alg);
    if (open_name[0]) sa_s(&b, open_name); else sa_f(&b, "null");
    sa_f(&b, ",\"ring_drops\":%ld,\"over_warn\":%s,\"recycle_bin\":\"checked per delete\"}",
         (long)InterlockedCompareExchange(&sw_ring.dropped, 0, 0), over ? "true" : "false");
    return b.bad ? -1 : (int)b.n;
}
