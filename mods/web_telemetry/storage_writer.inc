/* storage_writer.inc: Telemetry 1.1.0 storage writer (STORAGE_MANAGER_SPEC sections 3.1-3.6, 6.1-6.3, 7.1, 7.2).
 *
 * Capture side (sw_capture_tick, sw_capture_idle) runs under g_telemetry_cs on the capture
 * loop. It does no file IO and no allocation: it runs the pure session state machine and
 * copies one sample (or one control event) into a fixed ring. Writer side (sw_writer_main)
 * owns every file operation on its own thread. The ring is single producer, single consumer.
 *
 * Session decisions (live FACTS of 2026-10-13, see STORAGE_V2_BATCH2_RECORD):
 *  - a session opens only when sim_time advances (or drops, a reset) and is below 1e8, with
 *    cars present and a valid session_type (0..7). Stale values after quit never open one.
 *  - a session closes on session_type change, on a sim_time drop (restart), on sim_time >= 1e8
 *    (results: stop sampling, finalize) or when sim_time has not moved for 30 s.
 *  - race_phase 8 marks the session complete and recording continues until sim_time >= 1e8.
 *  - game_state 5 -> 2 is not an input at all: it never closes a session. */

#define SW_RING_CAP       48u   /* ring slots (samples and control events share it) */
#define SW_RESERVE        8u    /* slots kept free for control events: samples drop first */
#define SW_BLOCK_TICKS    20u   /* DATA block = 1 s at 20 Hz */
#define SW_NCH            128u  /* catalog ids 0..127 (2.1 B1: ids 64..124 added; table max 128) */
#define SW_RESULTS_SIM    1.0e8
#define SW_STALE_MS       30000u
#define SW_INDEX_MS       5000u
#define SW_WARN_BYTES     1073741824.0   /* 1 GiB, warn only (spec 0.2 item 5) */
#define SW_ENTRIES        512u
#define SW_PAYLOAD_CAP    SV2_MAX_PAYLOAD   /* 2.1: one DATA payload may use the whole reader limit */
#define SW_FRAME_CAP      (SW_PAYLOAD_CAP + SV2_BLOCK_HEADER_BYTES)
#define SW_PATH_MAX       260
#define SW_PATHBUF        1024   /* every storage path buffer: no truncation warnings */
#define SW_SESSION_COMPLETE (1u << 1)   /* header flag, bit 1 was unused in batch 1 */
#define SW_DRV_TEXT       64u   /* DRIVER event: name and team copies (NAME_LEN of the game text, NUL included) */

enum { SWE_NONE = 0, SWE_SAMPLE, SWE_OPEN, SWE_CLOSE, SWE_COMPLETE, SWE_SHUTDOWN, SWE_DRIVER };
enum { SWR_NONE = 0, SWR_TYPE, SWR_SIM_RESET, SWR_RESULTS, SWR_STALE, SWR_SHUTDOWN };
enum { SWA_CLOSE = 1u, SWA_OPEN = 2u, SWA_SAMPLE = 4u, SWA_COMPLETE = 8u };
enum { SW_ST_OPEN = 1, SW_ST_CLOSED, SW_ST_DAMAGED, SW_ST_RECOVERED, SW_ST_ARCHIVED, SW_ST_ARCH_INT };
enum { SW_OK = 0, SW_REFUSE_OS, SW_REFUSE_PATH, SW_REFUSE_FILE, SW_REFUSE_DIR };

/* ---------- ring element (one copy per sample; no pointers) ---------- */
typedef struct {
    uint8_t slot, scope;          /* scope 1 = player, 0 = AI (sv2_scope_ok convention) */
    uint64_t mask_lo, mask_hi;    /* bit id = catalog id (ids 0..63 lo, 64..127 hi) */
    float v[SW_NCH];
} sw_car_t;

typedef struct {
    uint8_t kind, reason;
    uint32_t tick_ms;
    double sim;
    int32_t session_type;
    int32_t game_mode, mp_kind;   /* OPEN only (1.1.1): game mode global 0x7D0410; multiplayer session kind, -1 when absent */
    uint32_t n_cars, generation;
    char aiw[SW_PATH_MAX];        /* OPEN and CLOSE: the AIW path as read from the game */
    SYSTEMTIME local, utc;        /* OPEN only */
    char drv_name[SW_DRV_TEXT], drv_team[SW_DRV_TEXT];   /* DRIVER only (REPLAY 2.3): copied on the capture side */
    int32_t drv_num;              /* DRIVER only: race number, -1 when out of range */
    uint8_t drv_slot;             /* DRIVER only */
    sw_car_t cars[SV2_MAX_CARS];  /* SAMPLE only */
} sw_event_t;

typedef struct {
    sw_event_t *slots;
    volatile LONG head, tail;     /* head: producer; tail: consumer */
    volatile LONG dropped;        /* samples refused because the ring was full */
    volatile LONG control_lost;   /* control events refused (must stay 0) */
} sw_ring_t;

static sw_ring_t sw_ring;
static HANDLE sw_wake, sw_thread;
static int sw_enabled;
static char sw_dir[SW_PATHBUF], sw_rec_dir[SW_PATHBUF], sw_index_path[SW_PATHBUF];

#ifdef TELEMETRY_TEST
static DWORD sw_test_write_delay_ms;   /* fake slow disk: added to every block write */
#endif

/* printf into a bounded buffer; the wrapper keeps GCC format-truncation checks on literal sizes quiet. */
static int sw_fmt(char *dst, size_t cap, const char *fmt, ...) {
    va_list ap;
    int k;
    va_start(ap, fmt);
    k = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    return k;
}

/* ---------- ring ---------- */
static int sw_ring_init(void) {
    if (sw_ring.slots) return 1;
    sw_ring.slots = (sw_event_t *)calloc(SW_RING_CAP, sizeof(sw_event_t));
    return sw_ring.slots != NULL;
}

/* Producer: returns a free slot or NULL. Samples stop SW_RESERVE slots early. Never blocks. */
static sw_event_t *sw_ring_claim(int kind) {
    LONG h, t, used;
    if (!sw_ring.slots) return NULL;
    h = sw_ring.head;
    t = InterlockedCompareExchange(&sw_ring.tail, 0, 0);
    used = h - t;
    if (kind == SWE_SAMPLE && used >= (LONG)(SW_RING_CAP - SW_RESERVE)) {
        InterlockedIncrement(&sw_ring.dropped);
        return NULL;
    }
    if (used >= (LONG)SW_RING_CAP) {
        InterlockedIncrement(&sw_ring.control_lost);
        return NULL;
    }
    return &sw_ring.slots[h % (LONG)SW_RING_CAP];
}

static void sw_ring_publish(void) {
    MemoryBarrier();
    sw_ring.head = sw_ring.head + 1;
    MemoryBarrier();
    if (sw_wake) SetEvent(sw_wake);
}

/* Consumer. */
static sw_event_t *sw_ring_peek(void) {
    LONG h, t;
    MemoryBarrier();
    h = InterlockedCompareExchange(&sw_ring.head, 0, 0);
    t = sw_ring.tail;
    if (h == t) return NULL;
    return &sw_ring.slots[t % (LONG)SW_RING_CAP];
}

static void sw_ring_pop(void) {
    MemoryBarrier();
    sw_ring.tail = sw_ring.tail + 1;
    MemoryBarrier();
}

/* ---------- session state machine (pure: no IO, no globals) ---------- */
typedef struct {
    int have_last, have_live, open, open_type, complete, results;
    double last_sim;
    uint32_t last_live_ms;
} sw_sm_t;

typedef struct {
    int32_t session_type, race_phase, vehicle_count;
    double sim;
    uint32_t now_ms;
} sw_obs_t;

typedef struct {
    unsigned acts;
    int reason;
    int32_t open_type;
} sw_act_t;

static sw_sm_t sw_sm;

static int sw_type_valid(int32_t t) { return t >= 0 && t <= 7; }

static sw_act_t sw_sm_step(sw_sm_t *m, const sw_obs_t *o) {
    sw_act_t a;
    int drop, adv;
    memset(&a, 0, sizeof a);
    if (!isfinite(o->sim)) return a;
    if (!m->have_live) { m->have_live = 1; m->last_live_ms = o->now_ms; }
    drop = m->have_last && o->sim < m->last_sim - 1e-6;
    adv = m->have_last && o->sim > m->last_sim + 1e-6;
    if (drop || adv) m->last_live_ms = o->now_ms;
    if (o->sim >= SW_RESULTS_SIM) {
        /* Results screen: stop sampling and finalize. Frozen sim after quit lands here too. */
        if (m->open) { a.acts |= SWA_CLOSE; a.reason = SWR_RESULTS; m->open = 0; }
        m->results = 1;
    } else {
        m->results = 0;
        if (m->open) {
            if (o->session_type != m->open_type) { a.acts |= SWA_CLOSE; a.reason = SWR_TYPE; m->open = 0; }
            else if (drop) { a.acts |= SWA_CLOSE; a.reason = SWR_SIM_RESET; m->open = 0; }
            else if (o->now_ms - m->last_live_ms > SW_STALE_MS) { a.acts |= SWA_CLOSE; a.reason = SWR_STALE; m->open = 0; }
        }
        if (!m->open && (adv || drop) && o->vehicle_count > 0 && sw_type_valid(o->session_type)) {
            m->open = 1;
            m->open_type = o->session_type;
            m->complete = 0;
            a.acts |= SWA_OPEN;
            a.open_type = o->session_type;
        }
        if (m->open) {
            a.acts |= SWA_SAMPLE;
            if (o->race_phase == 8 && !m->complete) { m->complete = 1; a.acts |= SWA_COMPLETE; }
        }
    }
    m->have_last = 1;
    m->last_sim = o->sim;
    return a;
}

/* Called when the capture loop has no fresh sample: only the 30 s stale rule can act. */
static sw_act_t sw_sm_idle(sw_sm_t *m, uint32_t now_ms) {
    sw_act_t a;
    memset(&a, 0, sizeof a);
    if (m->open && m->have_live && now_ms - m->last_live_ms > SW_STALE_MS) {
        a.acts = SWA_CLOSE; a.reason = SWR_STALE; m->open = 0;
    }
    return a;
}

/* ---------- car mapping (pure) ---------- */
/* 2.1 (B1): channel ids 64..124 are added from the per-car fields below. Every id is set unconditionally for a car
 * on track, except the player-only tcarc set (player only). Values are the game's own fields; no value is derived here. */
static void sw_car_from(const car_telemetry_t *c, int slot, int player, sw_car_t *s) {
    unsigned k;
    memset(s, 0, sizeof *s);
    s->slot = (uint8_t)slot;
    s->scope = player ? 1u : 0u;
#define SW_SET(id, val) do { s->v[(id)] = (float)(val); sv2_mask_set(&s->mask_lo, &s->mask_hi, (uint32_t)(id)); } while (0)
    SW_SET(1, c->lap_dist);
    SW_SET(9, c->yaw);
    SW_SET(10, c->yaw_rate);
    SW_SET(15, c->rpm);
    SW_SET(17, c->track_x);
    SW_SET(18, c->track_z);
    if (c->car_valid & 1) SW_SET(2, c->speed);
    if (c->car_valid & 2) SW_SET(3, c->throttle);
    if (c->car_valid & 4) SW_SET(4, c->brake);
    if (!player && (c->car_valid & 8)) SW_SET(5, c->steer);
    if (c->car_valid & 32) { SW_SET(6, c->glat); SW_SET(7, c->glon); SW_SET(8, c->gvert); }
    /* Tread (ids 11..14, normalized 0..1 as the sample builder). Player: g_telemetry.tread from the
     * thermal block (web_telemetry.c:5163-5167). AI: vehicle wheel block +0x363C/+0x3734/+0x382C/+0x3924
     * (OFF_TIRE_WEAR_*, web_telemetry.c:5288-5291), normalized (raw - 0.75) * 4 with the same clamp as
     * the sample builder (web_telemetry.c:2003-2009). Gated by car_valid bit 16 (tread valid). */
    if (c->car_valid & 16) {
        for (k = 0; k < WHEEL_COUNT; k++) {
            if (player) {
                SW_SET(11 + k, g_telemetry.tread[k]);
            } else {
                float mir = k == 0 ? c->tire_wear_fl : k == 1 ? c->tire_wear_fr : k == 2 ? c->tire_wear_rl : c->tire_wear_rr;
                float n = (mir - (float)TREAD_DEAD) * 4.0f;
                if (!(n > 0.0f)) n = 0.0f;
                if (n > 1.0f) n = 1.0f;
                SW_SET(11 + k, n);
            }
        }
    }
    /* Gear (id 16, RAW32 integer): c->gear read at web_telemetry.c:5232, same 0..16 gate as the sample builder (:2029). */
    if (c->gear >= 0 && c->gear <= 16) SW_SET(16, c->gear);
    /* Flags (id 19) are NOT written: the TRF word exists only in trace_sample_car_at (web_telemetry.c:1819),
     * which runs for the two selected trace cars, not for every car. See batch 3 record, channel map. */
    if (player) for (k = 0; k < WHEEL_COUNT; k++) SW_SET(20 + k, c->ttemp[k]);
    /* Player chassis (ids 24..63, spec 4.6, trimmed to 40 slots): g_telemetry.pch_* (web_telemetry.c:4514-4555).
     * Only while pch_valid is set; a tick without valid pch lists no pch channel. */
    if (player && g_telemetry.pch_valid) {
        for (k = 0; k < WHEEL_COUNT; k++) {
            SW_SET(24 + k, g_telemetry.pch_ride_mm[k]);
            SW_SET(28 + k, g_telemetry.pch_susp_mm[k]);
            SW_SET(32 + k, g_telemetry.pch_tdefl_mm[k]);
            SW_SET(36 + k, g_telemetry.pch_press[k]);
            SW_SET(40 + k, g_telemetry.pch_grip[k]);
            SW_SET(44 + k, g_telemetry.pch_camber_deg[k]);
            SW_SET(48 + k, g_telemetry.pch_spin[k]);
            SW_SET(52 + k, g_telemetry.pch_disc_mm[k]);
            SW_SET(56 + k, g_telemetry.pch_bend[k]);
        }
        SW_SET(60, g_telemetry.pch_bottom);
        SW_SET(61, g_telemetry.pch_compound);
        SW_SET(62, g_telemetry.pch_fuel);
        SW_SET(63, g_telemetry.pch_rpm);
    }
    /* 2.1 (B1) channels 64..124. Race state and classification. */
    SW_SET(64, c->tire_wear_fl); SW_SET(65, c->tire_wear_fr); SW_SET(66, c->tire_wear_rl); SW_SET(67, c->tire_wear_rr);
    SW_SET(68, c->position); SW_SET(69, c->lap_number); SW_SET(70, c->pitting); SW_SET(71, c->call_to_box);
    SW_SET(72, c->tire_choice); SW_SET(73, c->track_limits);
    SW_SET(74, c->fw_flag); SW_SET(75, c->rw_flag); SW_SET(76, c->dmg_tier);
    SW_SET(77, c->oil_temp); SW_SET(78, c->water_temp); SW_SET(79, c->fuel_level);
    SW_SET(80, c->gap_ahead_m); SW_SET(81, c->gap_behind_m); SW_SET(82, c->gap_ahead_id); SW_SET(83, c->gap_behind_id);
    SW_SET(84, c->live_sector_delta); SW_SET(85, c->last_sector_delta); SW_SET(86, c->last_lap);
    SW_SET(87, c->abt[0]); SW_SET(88, c->abt[1]);
    for (k = 0; k < 4; k++) { SW_SET(89 + k, c->aws[k]); }
    if (player) for (k = 0; k < WHEEL_COUNT; k++) SW_SET(93 + k, c->tcarc[k]);
    SW_SET(97, c->race_num); SW_SET(98, c->controller_role); SW_SET(99, c->car_valid); SW_SET(100, c->vf6);
    SW_SET(101, c->contact_with);
    /* RESEARCH candidates (T35, owner Q2: every car, always). Floats as f32 bit patterns; no range is claimed. */
    for (k = 0; k < 4; k++) {
        SW_SET(102 + k, c->rx_su[k]);
        SW_SET(106 + k, c->rx_ld[k]);
        SW_SET(110 + k, c->rx_sl[k]);
        SW_SET(114 + k, c->rx_fo[k]);
    }
    SW_SET(118, c->penalties); SW_SET(119, c->fuel_avg_per_lap); SW_SET(120, c->fuel_autonomy_laps);
    SW_SET(121, c->box_soon); SW_SET(122, c->finish_state); SW_SET(123, c->driver_status); SW_SET(124, c->reason_code);
#undef SW_SET
}

/* ---------- capture side (runs under g_telemetry_cs; no IO, no allocation) ---------- */
static void sw_fill_sample(sw_event_t *e, uint32_t now_ms) {
    uint32_t n = 0;
    e->tick_ms = now_ms;
    e->sim = g_telemetry.session_time;
    for (int i = 0; i < MAX_CARS && n < SV2_MAX_CARS; i++) {
        const car_telemetry_t *c = &g_telemetry.cars[i];
        if (!c->on_track) continue;
        sw_car_from(c, i, i == g_telemetry.player_slot, &e->cars[n]);
        n++;
    }
    e->n_cars = n;
}

/* REPLAY 2.3: driver EVENT source. Capture side (under g_telemetry_cs, no IO, no allocation): the last name, team and
 * race number written for each slot in this session. A car on track whose name, team or number differs gets one
 * SWE_DRIVER control event; the writer thread formats the JSON. A full ring defers the event to the next tick. */
typedef struct { uint8_t sent; char name[SW_DRV_TEXT], team[SW_DRV_TEXT]; int32_t num; } sw_drv_sent_t;
static sw_drv_sent_t sw_drv_sent[MAX_CARS];

static void sw_driver_check(uint32_t now_ms) {
    for (int i = 0; i < MAX_CARS; i++) {
        const car_telemetry_t *c = &g_telemetry.cars[i];
        sw_drv_sent_t *s = &sw_drv_sent[i];
        sw_event_t *e;
        int32_t num = c->race_num;
        if (!c->on_track || !c->name[0]) continue;
        if (s->sent && !strcmp(s->name, c->name) && !strcmp(s->team, c->car_name) && s->num == num) continue;
        e = sw_ring_claim(SWE_DRIVER);
        if (!e) return;
        memset(e, 0, sizeof *e);
        e->kind = SWE_DRIVER; e->tick_ms = now_ms; e->sim = g_telemetry.session_time;
        e->drv_slot = (uint8_t)i; e->drv_num = num;
        sv2_copy(e->drv_name, sizeof e->drv_name, c->name);
        sv2_copy(e->drv_team, sizeof e->drv_team, c->car_name);
        sw_ring_publish();
        s->sent = 1;
        sv2_copy(s->name, sizeof s->name, c->name);
        sv2_copy(s->team, sizeof s->team, c->car_name);
        s->num = num;
    }
}

/* Multiplayer session kind (CO-OP = 2) from the export the replay names use. -1 when the module or export is absent. */
typedef int (*sw_mp_kind_fn)(void);
static int sw_mp_kind(void) {
    HMODULE m = GetModuleHandleA("multiplayer_overhaul.dll");
    FARPROC fp = m ? GetProcAddress(m, "F1CMultiplayerOverhaulSessionKind") : NULL;
    return fp ? ((sw_mp_kind_fn)(void *)fp)() : -1;
}

static void sw_emit(const sw_act_t *a, uint32_t now_ms, int reason) {
    sw_event_t *e;
    if (a->acts & SWA_CLOSE) {
        e = sw_ring_claim(SWE_CLOSE);
        if (e) {
            memset(e, 0, sizeof *e);
            e->kind = SWE_CLOSE; e->reason = (uint8_t)reason; e->tick_ms = now_ms;
            e->sim = g_telemetry.session_time;
            sv2_copy(e->aiw, sizeof e->aiw, g_telemetry.aiw_path);
            sw_ring_publish();
        }
    }
    if (a->acts & SWA_OPEN) {
        e = sw_ring_claim(SWE_OPEN);
        if (e) {
            memset(e, 0, sizeof *e);
            e->kind = SWE_OPEN; e->tick_ms = now_ms; e->sim = g_telemetry.session_time;
            e->session_type = a->open_type;
            e->game_mode = (int32_t)RACE_MODE;   /* captured at session start, on the capture thread */
            e->mp_kind = sw_mp_kind();
            e->generation = capture_generation;
            sv2_copy(e->aiw, sizeof e->aiw, g_telemetry.aiw_path);
            memset(sw_drv_sent, 0, sizeof sw_drv_sent);   /* REPLAY 2.3: every driver is written again in a new session */
            GetLocalTime(&e->local);
            GetSystemTime(&e->utc);
            sw_ring_publish();
        }
    }
    if (a->acts & SWA_SAMPLE) {
        sw_driver_check(now_ms);
        e = sw_ring_claim(SWE_SAMPLE);
        if (e) { e->kind = SWE_SAMPLE; sw_fill_sample(e, now_ms); sw_ring_publish(); }
    }
    if (a->acts & SWA_COMPLETE) {
        e = sw_ring_claim(SWE_COMPLETE);
        if (e) { memset(e, 0, sizeof *e); e->kind = SWE_COMPLETE; e->tick_ms = now_ms; sw_ring_publish(); }
    }
}

/* One processed capture sample. Called once per game tick under g_telemetry_cs. */
static void sw_capture_tick(uint32_t now_ms) {
    sw_obs_t o;
    sw_act_t a;
    if (!sw_enabled) return;
    o.session_type = g_telemetry.session_type;
    o.race_phase = g_telemetry.race_phase;
    o.vehicle_count = g_telemetry.vehicle_count;
    o.sim = g_telemetry.session_time;
    o.now_ms = now_ms;
    a = sw_sm_step(&sw_sm, &o);
    sw_emit(&a, now_ms, a.reason);
}

/* Capture loop had no fresh sample (stale branch). */
static void sw_capture_idle(uint32_t now_ms) {
    sw_act_t a;
    if (!sw_enabled) return;
    a = sw_sm_idle(&sw_sm, now_ms);
    sw_emit(&a, now_ms, a.reason);
}

/* ---------- OS gate and folder layout (spec 3.1) ---------- */
typedef LONG (WINAPI *sw_rtl_gv_fn)(OSVERSIONINFOW *);

static int sw_os_ok(unsigned major, unsigned minor) {
    return major > 6u || (major == 6u && minor >= 2u);   /* Windows 8 = 6.2 */
}

static int sw_os_version(unsigned *major, unsigned *minor) {
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    FARPROC fp = nt ? GetProcAddress(nt, "RtlGetVersion") : NULL;
    sw_rtl_gv_fn fn;
    OSVERSIONINFOW vi;
    if (!fp) return 0;
    memcpy(&fn, &fp, sizeof fn);
    memset(&vi, 0, sizeof vi);
    vi.dwOSVersionInfoSize = sizeof vi;
    if (fn(&vi) != 0) return 0;
    *major = vi.dwMajorVersion;
    *minor = vi.dwMinorVersion;
    return 1;
}

static int sw_is_dir(const char *p) {
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static int sw_path_exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

/* Game directory (with trailing backslash) -> <game>\Telemetry\ and <game>\Telemetry\recordings\. */
static int sw_root_init(const char *game_dir, unsigned major, unsigned minor, char *why, size_t whycap) {
    size_t n = strlen(game_dir);
    if (!sw_os_ok(major, minor)) { sw_fmt(why, whycap, "Windows 8 or later required (found %u.%u)", major, minor); return SW_REFUSE_OS; }
    if (n == 0 || n > MAX_PATH - 64) { sw_fmt(why, whycap, "game path too long for storage"); return SW_REFUSE_PATH; }
    if (sw_fmt(sw_dir, sizeof sw_dir, "%s%s", game_dir, game_dir[n - 1] == '\\' ? "Telemetry\\" : "\\Telemetry\\") >= (int)sizeof sw_dir) {
        sw_fmt(why, whycap, "game path too long for storage"); return SW_REFUSE_PATH;
    }
    sw_dir[strlen(sw_dir) - 1] = 0;                       /* without trailing backslash for the file test */
    if (sw_path_exists(sw_dir) && !sw_is_dir(sw_dir)) {
        sw_fmt(why, whycap, "a file named Telemetry is in the game folder; nothing renamed"); return SW_REFUSE_FILE;
    }
    if (!CreateDirectoryA(sw_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        sw_fmt(why, whycap, "cannot create the Telemetry folder (write access to the game folder?)"); return SW_REFUSE_DIR;
    }
    sw_dir[strlen(sw_dir)] = '\\'; sw_dir[strlen(sw_dir) + 1] = 0;
    sw_fmt(sw_rec_dir, sizeof sw_rec_dir, "%srecordings", sw_dir);
    sw_fmt(sw_index_path, sizeof sw_index_path, "%sindex.json", sw_dir);
    if (!CreateDirectoryA(sw_rec_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        sw_fmt(why, whycap, "cannot create the recordings folder"); return SW_REFUSE_DIR;
    }
    sw_fmt(sw_rec_dir, sizeof sw_rec_dir, "%srecordings\\", sw_dir);
    why[0] = 0;
    return SW_OK;
}

/* Module-level entry: the game executable folder (GetModuleFileNameA(NULL), spec 3.1 item 1). */
static int sw_root_init_from_exe(char *why, size_t whycap) {
    char exe[MAX_PATH];
    char *slash;
    unsigned maj = 0, min = 0;
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
    if (!n || n >= sizeof exe) { sw_fmt(why, whycap, "cannot read the game path"); return SW_REFUSE_PATH; }
    slash = strrchr(exe, '\\');
    if (!slash) { sw_fmt(why, whycap, "cannot read the game path"); return SW_REFUSE_PATH; }
    slash[1] = 0;
    if (!sw_os_version(&maj, &min)) { sw_fmt(why, whycap, "cannot read the Windows version"); return SW_REFUSE_OS; }
    return sw_root_init(exe, maj, min, why, whycap);
}

/* ---------- index (spec 3.6) ---------- */
typedef struct {
    char folder[120], file[180], weekend[64];
    int state;
    double bytes;
    uint32_t ticks;
} sw_entry_t;

static sw_entry_t sw_entries[SW_ENTRIES];
static unsigned sw_nentries;
static double sw_overflow_bytes;       /* bytes of files that did not fit in the entry table */

static double sw_total_bytes(void) {
    double t = sw_overflow_bytes;
    for (unsigned i = 0; i < sw_nentries; i++) t += sw_entries[i].bytes;
    return t;
}

static const char *sw_state_name(int s) {
    switch (s) {
    case SW_ST_OPEN: return "open";
    case SW_ST_CLOSED: return "closed";
    case SW_ST_RECOVERED: return "recovered";
    case SW_ST_ARCHIVED: return "archived";
    case SW_ST_ARCH_INT: return "archive_interrupted";
    default: return "damaged";
    }
}

/* Appends to buf; returns the new length, or 0 when the buffer is too small. */
static size_t sw_app(char *buf, size_t cap, size_t len, const char *fmt, ...) {
    va_list ap;
    int k;
    if (len >= cap) return 0;
    va_start(ap, fmt);
    k = vsnprintf(buf + len, cap - len, fmt, ap);
    va_end(ap);
    if (k < 0 || (size_t)k >= cap - len) return 0;
    return len + (size_t)k;
}

static size_t sw_index_format(char *buf, size_t cap) {
    SYSTEMTIME st;
    size_t len = 0;
    unsigned seen = 0;
    GetSystemTime(&st);
    len = sw_app(buf, cap, len,
        "{\"schema\":\"f1c.telemetry.index/1\",\"updated_utc\":\"%04d-%02d-%02dT%02d:%02d:%02dZ\","
        "\"warn_bytes\":%.0f,\"total_bytes\":%.0f,\"warn_active\":%s,\"recordings\":[",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, SW_WARN_BYTES, sw_total_bytes(),
        sw_total_bytes() >= SW_WARN_BYTES ? "true" : "false");
    if (!len) return 0;
    for (unsigned i = 0; i < sw_nentries; i++) {
        int first = 1;
        for (unsigned j = 0; j < i; j++) if (!strcmp(sw_entries[j].folder, sw_entries[i].folder)) { first = 0; break; }
        if (!first) continue;
        {   /* batch 3: per-recording size (sum of its files; spec 8.3 sizes) */
            double rb = 0.0;
            for (unsigned j = 0; j < sw_nentries; j++) if (!strcmp(sw_entries[j].folder, sw_entries[i].folder)) rb += sw_entries[j].bytes;
            len = sw_app(buf, cap, len, "%s{\"id\":\"%s\",\"weekend\":\"%s\",\"bytes\":%.0f,\"sessions\":[", seen ? "," : "",
                         sw_entries[i].folder, sw_entries[i].weekend, rb);
        }
        if (!len) return 0;
        seen++;
        for (unsigned j = 0, k = 0; j < sw_nentries; j++) {
            if (strcmp(sw_entries[j].folder, sw_entries[i].folder)) continue;
            len = sw_app(buf, cap, len, "%s\"%s\"", k ? "," : "", sw_entries[j].file);
            if (!len) return 0;
            k++;
        }
        len = sw_app(buf, cap, len, "]}");
        if (!len) return 0;
    }
    len = sw_app(buf, cap, len, "],\"files\":[");
    if (!len) return 0;
    for (unsigned i = 0; i < sw_nentries; i++) {
        len = sw_app(buf, cap, len, "%s{\"name\":\"%s\",\"recording\":\"%s\",\"state\":\"%s\",\"bytes\":%.0f,\"ticks\":%u}",
                     i ? "," : "", sw_entries[i].file, sw_entries[i].folder, sw_state_name(sw_entries[i].state),
                     sw_entries[i].bytes, (unsigned)sw_entries[i].ticks);
        if (!len) return 0;
    }
    len = sw_app(buf, cap, len, "]}\n");
    return len;
}

/* storage_api.inc (batch 4): publishes the recordings snapshot; rebuilt by the writer on every index write. */
static void sa_snapshot_rebuild(void);
static void sa_init(void);

/* Atomic: write <index>.tmp, then replace. Returns 1 on success. */
static int sw_index_write(void) {
    static char buf[1u << 18];
    char tmp[SW_PATHBUF];
    size_t n;
    FILE *f;
    if (!sw_enabled) return 0;
    sa_snapshot_rebuild();
    n = sw_index_format(buf, sizeof buf);
    if (!n) return 0;
    sw_fmt(tmp, sizeof tmp, "%s.tmp", sw_index_path);
    f = fopen(tmp, "wb");
    if (!f) return 0;
    if (fwrite(buf, 1, n, f) != n) { fclose(f); return 0; }
    fflush(f);
    fclose(f);
    return MoveFileExA(tmp, sw_index_path, MOVEFILE_REPLACE_EXISTING) != 0;
}

/* Cache check only (spec 3.6, 7.5): schema present, braces balanced, closing brace last. */
static int sw_index_valid(const char *path) {
    static char buf[1u << 18];
    size_t n = 0, i;
    int depth = 0, in_str = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    if (n < 2 || buf[0] != '{' || !strstr(buf, "\"schema\":\"f1c.telemetry.index/1\"")) return 0;
    for (i = 0; i < n; i++) {
        char c = buf[i];
        if (c == '"') in_str = !in_str;
        else if (!in_str && c == '{') depth++;
        else if (!in_str && c == '}') { if (--depth < 0) return 0; }
    }
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) n--;
    return depth == 0 && n > 0 && buf[n - 1] == '}';
}

static void sw_entry_add(const char *folder, const char *file, const char *weekend, int state, double bytes, uint32_t ticks) {
    if (sw_nentries >= SW_ENTRIES) { sw_overflow_bytes += bytes; return; }
    sw_entry_t *e = &sw_entries[sw_nentries++];
    sv2_copy(e->folder, sizeof e->folder, folder);
    sv2_copy(e->file, sizeof e->file, file);
    sv2_copy(e->weekend, sizeof e->weekend, weekend);
    e->state = state;
    e->bytes = bytes;
    e->ticks = ticks;
}

/* Full path of a file in a recording folder, in a static buffer (scan thread only). */
static const char *sub_path(const char *folder, const char *file) {
    static char p[SW_PATHBUF];
    sw_fmt(p, sizeof p, "%s%s\\%s", sw_rec_dir, folder, file);
    return p;
}

/* 1 when the .f1t header carries RECOVERED (spec 4.1 bit 6). Header read only. */
static int sw_f1t_recovered(const char *path) {
    uint8_t hb[SV2_HEADER_BYTES];
    sv2_header_t h;
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(hb, 1, sizeof hb, f);
    fclose(f);
    if (sv2_header_decode(hb, n, &h) != SV2_OK) return 0;
    return (h.flags & SV2_FLAG_RECOVERED) != 0;
}

/* Set by the jobs thread after a repair, delete or archive changes files. The writer thread consumes it
 * between writes (sw_rebuild_entries), with or without an open session. */
static volatile LONG sw_rebuild_req;

/* Startup rebuild from the folder (spec 3.6: folder scan wins over the cache). Leftover .part files
 * are listed as damaged here; the jobs thread repairs them (7.5) after registration. */
static void sw_scan_folders(void) {
    WIN32_FIND_DATAA fd, ff;
    char pat[SW_PATHBUF], sub[SW_PATHBUF], fp[SW_PATHBUF];
    HANDLE h;
    if (!sw_enabled) return;
    sw_fmt(pat, sizeof pat, "%s*", sw_rec_dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        sw_fmt(sub, sizeof sub, "%s%s\\*", sw_rec_dir, fd.cFileName);
        HANDLE hf = FindFirstFileA(sub, &ff);
        if (hf == INVALID_HANDLE_VALUE) continue;
        do {
            size_t L;
            int state;
            double bytes;
            if (ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            L = strlen(ff.cFileName);
            bytes = (double)ff.nFileSizeHigh * 4294967296.0 + (double)ff.nFileSizeLow;
            state = -1;
            if (L > 4 && !strcmp(ff.cFileName + L - 4, ".f1t")) {
                state = sw_f1t_recovered(sub_path(fd.cFileName, ff.cFileName)) ? SW_ST_RECOVERED : SW_ST_CLOSED;
            }
            else if (L > 9 && !strcmp(ff.cFileName + L - 9, ".f1t.part")) state = SW_ST_DAMAGED;
            else if (L > 17 && !strcmp(ff.cFileName + L - 17, ".f1t.part.damaged")) state = SW_ST_DAMAGED;
            else if (L > 4 && !strcmp(ff.cFileName + L - 4, ".f1a")) state = SW_ST_ARCHIVED;
            else if (L > 12 && !strcmp(ff.cFileName + L - 12, ".f1a.partial")) state = SW_ST_ARCH_INT;
            if (state < 0) continue;
            sw_fmt(fp, sizeof fp, "%s", ff.cFileName);
            sw_entry_add(fd.cFileName, fp, "", state, bytes, 0);
        } while (FindNextFileA(hf, &ff));
        FindClose(hf);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* ---------- writer state (writer thread only, except sw_thread bookkeeping) ---------- */
typedef struct {
    FILE *f;
    int have, complete, io_failed, cabinet_alg;
    uint32_t alg, flags, table_bytes, gen;
    uint32_t blk_seq, data_blocks, total_ticks, session_dropped, entry;
    int32_t session_type, season, tz_min;
    SYSTEMTIME local, utc;
    char folder[SW_PATHBUF], folder_name[120], part_path[SW_PATHBUF], stem[160], track[64], aiw[SW_PATH_MAX], weekend[160];
    char final_name[200];
    char country[64], mode[24], session[24];   /* 1.1.1 replay-style name parts, fixed at open */
    int32_t has_session;
    unsigned weekend_seq;
    double bytes, start_unix, last_sim;
    /* DATA block assembly */
    unsigned blk_ticks, blk_n;
    uint32_t blk_tick_ms[SW_BLOCK_TICKS];
    double blk_sim_first;
    double blk_sim[SW_BLOCK_TICKS];   /* 2.1: sim time per tick */
    uint32_t events;                 /* 2.1: EVENT blocks written this session */
    uint8_t blk_slot[SV2_MAX_CARS], blk_scope[SV2_MAX_CARS];
    uint64_t blk_mask_lo[SV2_MAX_CARS], blk_mask_hi[SV2_MAX_CARS];
    uint32_t blk_vals[SV2_MAX_CARS][SW_NCH * SW_BLOCK_TICKS];
    uint32_t gap_reported;
    uint64_t clamped;
    sv2_channel_t chan[SW_NCH];
    uint8_t *raw, *packed, *frame;
} sw_wr_t;

static sw_wr_t sw_wr;
static sv2_weekend_t sw_wk;

static void sw_write_bytes(const void *p, size_t n) {
#ifdef TELEMETRY_TEST
    if (sw_test_write_delay_ms) Sleep(sw_test_write_delay_ms);
#endif
    if (!sw_wr.f || sw_wr.io_failed) return;
    if (fwrite(p, 1, n, sw_wr.f) != n) sw_wr.io_failed = 1;
    else sw_wr.bytes += (double)n;
}

static void sw_entry_update(void) {
    if (sw_wr.entry < sw_nentries) {
        sw_entry_t *e = &sw_entries[sw_wr.entry];
        e->bytes = sw_wr.bytes;
        e->ticks = sw_wr.total_ticks;
    }
}

/* Writes one DATA block from the assembly buffer. */
static void sw_flush_block(void) {
    sv2_data_t *d;
    size_t n, stored = 0, fn;
    uint8_t flags = 0;
    sv2_block_t b;
    unsigned t;
    if (sw_wr.blk_ticks == 0) return;
    d = (sv2_data_t *)calloc(1, sizeof *d);
    if (!d) { sw_wr.io_failed = 1; sw_wr.blk_ticks = sw_wr.blk_n = 0; return; }
    d->n_ticks = sw_wr.blk_ticks;
    d->n_cars = sw_wr.blk_n;
    for (t = 0; t < sw_wr.blk_ticks; t++) {
        int64_t dt = (int64_t)sw_wr.blk_tick_ms[t] - (int64_t)sw_wr.blk_tick_ms[0];
        d->dt_ms[t] = dt < 0 ? 0 : (int32_t)dt;
    }
    for (t = 0; t < sw_wr.blk_ticks; t++) {   /* 2.1: sim per tick, ms relative to tick 0 */
        double ds = (sw_wr.blk_sim[t] - sw_wr.blk_sim[0]) * 1000.0;
        if (!isfinite(ds) || ds > 2147483000.0 || ds < -2147483000.0) ds = 0.0;
        d->sim_ms[t] = (int32_t)llround(ds);
    }
    for (unsigned j = 0; j < sw_wr.blk_n; j++) {
        unsigned nr = (unsigned)(sv2_popcount64(sw_wr.blk_mask_lo[j]) + sv2_popcount64(sw_wr.blk_mask_hi[j])), cnt = sw_wr.blk_ticks;
        for (unsigned r = 0; r < nr; r++)   /* repack to rank * count stride (spec 4.4) */
            memmove(&sw_wr.blk_vals[j][r * cnt], &sw_wr.blk_vals[j][r * SW_BLOCK_TICKS], cnt * sizeof(uint32_t));
        d->cars[j].slot = sw_wr.blk_slot[j];
        d->cars[j].scope = sw_wr.blk_scope[j];
        d->cars[j].first_tick = 0;
        d->cars[j].count = (uint8_t)cnt;
        d->cars[j].mask_lo = sw_wr.blk_mask_lo[j];
        d->cars[j].mask_hi = sw_wr.blk_mask_hi[j];
        d->cars[j].vals = sw_wr.blk_vals[j];
    }
    n = sv2_data_encode(d, sw_wr.chan, SV2_CATALOG_COUNT, sw_wr.raw, SW_PAYLOAD_CAP);
    free(d);
    if (!n || !sv2_pack_payload(sw_wr.alg, sw_wr.raw, n, sw_wr.packed, SW_PAYLOAD_CAP, &stored, &flags)) {
        sw_wr.io_failed = 1;
        sw_wr.blk_ticks = sw_wr.blk_n = 0;
        return;
    }
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_DATA;
    b.flags = flags;
    b.seq = sw_wr.blk_seq;
    b.n_ticks = sw_wr.blk_ticks;
    b.raw_len = (uint32_t)n;
    b.stored_len = (uint32_t)stored;
    b.t_first = sw_wr.blk_sim_first;
    fn = sv2_block_put(sw_wr.frame, SW_FRAME_CAP, b, sw_wr.packed);
    if (!fn) { sw_wr.io_failed = 1; sw_wr.blk_ticks = sw_wr.blk_n = 0; return; }
    sw_write_bytes(sw_wr.frame, fn);
    if (sw_wr.f && !sw_wr.io_failed) fflush(sw_wr.f);     /* spec 6.3: per block, process-crash durable */
    sw_wr.blk_seq++;
    sw_wr.data_blocks++;
    sw_wr.total_ticks += sw_wr.blk_ticks;
    sw_entry_update();
    sw_wr.blk_ticks = sw_wr.blk_n = 0;
}

static void sw_write_gap(uint32_t dropped, double t_from, double t_to) {
    uint8_t pl[20], frame[SV2_BLOCK_HEADER_BYTES + 20];
    sv2_block_t b;
    size_t fn;
    sv2_put_le(pl, dropped, 4);
    sv2_put_le(pl + 4, sv2_f64_bits(t_from), 8);
    sv2_put_le(pl + 12, sv2_f64_bits(t_to), 8);
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_GAP;
    b.seq = sw_wr.blk_seq;
    b.raw_len = b.stored_len = 20;
    fn = sv2_block_put(frame, sizeof frame, b, pl);
    if (!fn) { sw_wr.io_failed = 1; return; }
    sw_write_bytes(frame, fn);
    sw_wr.blk_seq++;
    sw_wr.session_dropped += dropped;
    sw_entry_update();
}

/* ---------- META block (spec 4.3, f1c.telemetry.meta/2; batch 4 subset: session identity and counters) ---------- */
static const char *sw_type_name(int32_t t) {
    static const char *names[] = { "Test Day", "Practice 1", "Practice 2", "Practice 3", "Practice 4",
                                   "Qualifying", "Warm-up", "Race" };
    return (t >= 0 && t <= 7) ? names[t] : "Other";
}

/* JSON string body (no quotes). Quote and backslash are escaped; control bytes and bytes >= 0x80 become '?'
 * so the block is always valid UTF-8 (the AIW path is game ANSI text). */
static size_t sw_json_body(char *out, size_t cap, const char *s) {
    size_t k = 0;
    for (; *s && k + 8 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c < 0x20 || c >= 0x80) out[k++] = '?';
        else out[k++] = (char)c;
    }
    out[k] = 0;
    return k;
}

static size_t sw_meta_format(char *buf, size_t cap, int final_block) {
    char track_b[160], raw_b[2 * SW_PATH_MAX + 16], stem_b[320], wk_b[320];
    sw_json_body(track_b, sizeof track_b, sw_wr.track);
    sw_json_body(raw_b, sizeof raw_b, sw_wr.aiw);
    sw_json_body(stem_b, sizeof stem_b, sw_wr.stem);
    sw_json_body(wk_b, sizeof wk_b, sw_wr.weekend);
    return sw_app(buf, cap, 0,
        "{\"schema\":\"f1c.telemetry.meta/2\",\"parts\":[\"session\"],\"final\":%s,\"complete\":%s,"
        "\"session_name\":\"%s\",\"weekend\":\"%s\",\"track\":\"%s\",\"track_raw\":\"%s\",\"season\":%d,"
        "\"session_type\":%d,\"session_type_name\":\"%s\",\"tz_min\":%d,"
        "\"ticks\":%u,\"data_blocks\":%u,\"dropped_ticks\":%u}",
        final_block ? "true" : "false", sw_wr.complete ? "true" : "false",
        stem_b, wk_b, track_b, raw_b, (int)sw_wr.season,
        (int)sw_wr.session_type, sw_type_name(sw_wr.session_type), (int)sw_wr.tz_min,
        (unsigned)sw_wr.total_ticks, (unsigned)sw_wr.data_blocks, (unsigned)sw_wr.session_dropped);
}

/* Writes one META block (raw, never compressed). Writer thread only. Initial after the channel table, final before END. */
static void sw_meta_write(int final_block) {
    char js[1024];
    size_t n = sw_meta_format(js, sizeof js, final_block), fn;
    sv2_block_t b;
    if (!n || !sw_wr.frame || !sw_wr.f || sw_wr.io_failed) return;
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_META;
    b.seq = sw_wr.blk_seq;
    b.raw_len = b.stored_len = (uint32_t)n;
    fn = sv2_block_put(sw_wr.frame, SW_FRAME_CAP, b, (const uint8_t *)js);
    if (!fn) { sw_wr.io_failed = 1; return; }
    sw_write_bytes(sw_wr.frame, fn);
    sw_wr.blk_seq++;
}

/* RAW32 channels that carry integers (exact i32). The others are f32 bit patterns. */
static int sw_raw_is_int(unsigned id) {
    return id == 16 || id == 60 || id == 61 || (id >= 68 && id <= 76) || id == 82 || id == 83 ||
           (id >= 97 && id <= 101) || id == 118 || (id >= 121 && id <= 124);
}

/* Adds one sample to the assembly buffer; flushes when the car set or masks change. */
static void sw_block_add(const sw_event_t *e) {
    int same = sw_wr.blk_ticks > 0 && sw_wr.blk_n == e->n_cars && sw_wr.blk_ticks < SW_BLOCK_TICKS;
    unsigned t;
    if (e->n_cars == 0) return;
    if (same) {
        for (unsigned j = 0; j < e->n_cars; j++)
            if (sw_wr.blk_slot[j] != e->cars[j].slot || sw_wr.blk_mask_lo[j] != e->cars[j].mask_lo ||
                sw_wr.blk_mask_hi[j] != e->cars[j].mask_hi) { same = 0; break; }
    }
    if (sw_wr.blk_ticks > 0 && !same) sw_flush_block();
    if (sw_wr.blk_ticks == 0) {
        sw_wr.blk_n = e->n_cars;
        for (unsigned j = 0; j < e->n_cars; j++) {
            sw_wr.blk_slot[j] = e->cars[j].slot;
            sw_wr.blk_scope[j] = e->cars[j].scope;
            sw_wr.blk_mask_lo[j] = e->cars[j].mask_lo;
            sw_wr.blk_mask_hi[j] = e->cars[j].mask_hi;
        }
        sw_wr.blk_sim_first = e->sim;
    }
    t = sw_wr.blk_ticks;
    for (unsigned j = 0; j < e->n_cars; j++) {
        unsigned rank = 0;
        for (unsigned id = 0; id < SW_NCH; id++) {
            int clamped = 0;
            uint32_t val;
            if (!sv2_mask_bit(e->cars[j].mask_lo, e->cars[j].mask_hi, id)) continue;
            if (sv2_catalog[id].type == SV2_TYPE_RAW32) {
                float fv = e->cars[j].v[id];
                val = sw_raw_is_int(id) ? (uint32_t)(int32_t)lroundf(fv) : sv2_f32_bits(fv);
            } else {
                val = sv2_q16_encode(e->cars[j].v[id], sv2_catalog[id].min, sv2_catalog[id].max, &clamped);
                if (clamped) sw_wr.clamped++;
            }
            sw_wr.blk_vals[j][rank * SW_BLOCK_TICKS + t] = val;
            rank++;
        }
    }
    for (unsigned j = 0; j < e->n_cars; j++) {   /* spec 4.1 bit 5: a player pch channel was written (header rewritten at close) */
        if (e->cars[j].scope && (e->cars[j].mask_lo & ~((1ull << 24) - 1ull)) != 0) sw_wr.flags |= SV2_FLAG_PLAYER_PCH;
    }
    sw_wr.blk_tick_ms[t] = e->tick_ms;
    sw_wr.blk_sim[t] = e->sim;   /* 2.1: per-tick sim time, written as ms deltas at flush */
    sw_wr.blk_ticks++;
    sw_wr.last_sim = e->sim;
}

/* ---------- 2.1 EVENT blocks (B1, spec 5 B1, owner Q7) ----------
 * Writer thread only. An EVENT block is raw JSON with t_first = session sim time of the event. The open DATA block
 * is flushed first, so the file stays in sim order. Events derived from samples are compared with the previous
 * sample of the same car (writer thread, not capture: the capture hot path is unchanged). */
static void sw_write_event(double sim, const char *json) {
    uint8_t frame[SV2_BLOCK_HEADER_BYTES + SV2_EVENT_MAX];
    sv2_block_t b;
    size_t n = strlen(json), fn;
    if (!sw_wr.f || sw_wr.io_failed || n < 2 || n > SV2_EVENT_MAX || !isfinite(sim)) return;
    sw_flush_block();
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_EVENT;
    b.seq = sw_wr.blk_seq;
    b.t_first = sim;
    b.raw_len = b.stored_len = (uint32_t)n;
    fn = sv2_block_put(frame, sizeof frame, b, (const uint8_t *)json);
    if (!fn) { sw_wr.io_failed = 1; return; }
    sw_write_bytes(frame, fn);
    sw_wr.blk_seq++;
    sw_wr.events++;
    sw_entry_update();
}

/* 1.2.5: ll_have / ll = the last_lap_s written in the car's previous lap EVENT (an untimed in lap repeats it). */
typedef struct { uint8_t seen, ll_have; int32_t lap, pit, limits, contact; float ll; } sw_ev_prev_t;
static sw_ev_prev_t sw_ev_prev[SV2_MAX_CARS];   /* reset at every session open */

/* 1.2.5: the last_lap_s a lap EVENT writes: channel 86, or -1 when it repeats the car's last written time. Pure. */
static float sw_event_lap_time(sw_ev_prev_t *p, float ll) {
    if (!(ll > 0.0f)) return ll;
    if (p->ll_have && fabsf(ll - p->ll) < 0.0005f) return -1.0f;   /* last_lap_s is written with 3 decimals */
    p->ll_have = 1;
    p->ll = ll;
    return ll;
}

/* Lap change (game lap time from last_lap_s, channel 86), pit state change, track-limit count change and a new
 * contact partner (heuristic from web_telemetry.c, labelled "contact"). Only comparisons: no value is invented. */
static void sw_events_from_sample(const sw_event_t *e) {
    char js[SV2_EVENT_MAX];
    for (unsigned j = 0; j < e->n_cars; j++) {
        const sw_car_t *c = &e->cars[j];
        unsigned slot = c->slot;
        sw_ev_prev_t *p;
        int lap, pit, lim, con;
        if (slot >= SV2_MAX_CARS || !sv2_mask_bit(c->mask_lo, c->mask_hi, 69)) continue;
        lap = (int)c->v[69];
        pit = (int)c->v[70];
        lim = (int)c->v[73];
        con = (int)c->v[101];
        p = &sw_ev_prev[slot];
        if (p->seen) {
            if (lap != p->lap) {
                /* 1.2.5 (DVOA report 2026-10-18): the game does not time an in lap and keeps the previous lap's value
                 * in channel 86; 1.2.4 wrote that value again. A repeat of the last written time is written as -1
                 * (no game time), so the lap is listed as untimed. Only a comparison: no value is invented. */
                float ll = sw_event_lap_time(p, c->v[86]);
                sw_fmt(js, sizeof js, "{\"type\":\"lap\",\"t\":%.3f,\"car\":%u,\"lap\":%d,\"prev_lap\":%d,\"last_lap_s\":%.3f}",
                       e->sim, slot, lap, p->lap, (double)ll);
                sw_write_event(e->sim, js);
            }
            if (pit != p->pit) {
                sw_fmt(js, sizeof js, "{\"type\":\"pit\",\"t\":%.3f,\"car\":%u,\"state\":%d,\"prev_state\":%d}",
                       e->sim, slot, pit, p->pit);
                sw_write_event(e->sim, js);
            }
            if (lim != p->limits) {
                sw_fmt(js, sizeof js, "{\"type\":\"incident\",\"kind\":\"limits\",\"t\":%.3f,\"car\":%u,\"count\":%d}",
                       e->sim, slot, lim);
                sw_write_event(e->sim, js);
            }
            if (con != p->contact && con >= 0) {
                sw_fmt(js, sizeof js, "{\"type\":\"incident\",\"kind\":\"contact\",\"t\":%.3f,\"car\":%u,\"with\":%d}",
                       e->sim, slot, con);
                sw_write_event(e->sim, js);
            }
        }
        p->seen = 1;
        p->lap = lap;
        p->pit = pit;
        p->limits = lim;
        p->contact = con;
    }
}

/* REPLAY 2.3: driver EVENT {"type":"driver","t","car","name","team","num"} (writer thread only). Names are escaped by
 * sw_json_body (every input byte is at most two output bytes, so no escape is cut); the record is far below the cap. */
static void sw_write_driver_event(const sw_event_t *e) {
    char nb[2 * SW_DRV_TEXT + 8], tb[2 * SW_DRV_TEXT + 8], js[SV2_EVENT_MAX];
    sw_json_body(nb, sizeof nb, e->drv_name);
    sw_json_body(tb, sizeof tb, e->drv_team);
    sw_fmt(js, sizeof js, "{\"type\":\"driver\",\"t\":%.3f,\"car\":%u,\"name\":\"%s\",\"team\":\"%s\",\"num\":%ld}",
           e->sim, (unsigned)e->drv_slot, nb, tb, (long)e->drv_num);
    sw_write_event(e->sim, js);
}

static int32_t sw_tz_minutes(void) {
    TIME_ZONE_INFORMATION tzi;
    DWORD r = GetTimeZoneInformation(&tzi);
    LONG bias = tzi.Bias;
    if (r == TIME_ZONE_ID_DAYLIGHT) bias += tzi.DaylightBias;
    return (int32_t)-bias;
}

static double sw_unix_of(const SYSTEMTIME *st) {
    FILETIME ft;
    ULARGE_INTEGER u;
    if (!SystemTimeToFileTime(st, &ft)) return 0.0;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return ((double)u.QuadPart - 116444736000000000.0) / 1e7;
}

static int sw_name_in_folder(void *ctx, const char *stem) {
    char p[SW_PATHBUF];
    (void)ctx;
    sw_fmt(p, sizeof p, "%s\\%s.f1t", sw_wr.folder, stem);
    if (sw_path_exists(p)) return 1;
    sw_fmt(p, sizeof p, "%s\\%s.f1t.part", sw_wr.folder, stem);
    return sw_path_exists(p);
}

static int sw_name_as_dir(void *ctx, const char *stem) {
    char p[SW_PATHBUF];
    (void)ctx;
    sw_fmt(p, sizeof p, "%s%s", sw_rec_dir, stem);
    return sw_path_exists(p);
}

static void sw_local_of(const SYSTEMTIME *st, sv2_local_t *t) {
    t->year = st->wYear; t->mon = st->wMonth; t->day = st->wDay;
    t->hour = st->wHour; t->min = st->wMinute; t->sec = st->wSecond;
}

/* Opens a session file at the start of a session. Creates the weekend folder when the weekend changes. */
/* Name parts for a session (1.1.1, decision A): country from the AIW path (fallback: parsed track), mode and
 * session labels from the game. Test Day (game mode 1) has no session part. */
static void sw_names_of(const sw_event_t *e, const char *track, char *country, size_t cc, char *mode, size_t mc,
                        char *session, size_t sc, int32_t *has_session) {
    if (!sv2_country_from_aiw(e->aiw, country, cc)) sv2_copy(country, cc, track);
    if (!sv2_mode_token(e->game_mode, e->mp_kind, mode, mc)) mode[0] = 0;
    if (!sv2_session_token(e->session_type, session, sc)) session[0] = 0;
    *has_session = e->game_mode != 1 && session[0] != 0;
}

static void sw_open_session(const sw_event_t *e) {
    char track[64] = "Unknown";
    int year = 0;
    sv2_local_t lt;
    char stem[160], part[SW_PATHBUF];
    char country[64], mode[24], session[24], wkey[160];
    int32_t has_session = 0;
    sv2_header_t h;
    uint8_t hb[SV2_HEADER_BYTES];
    uint8_t *table = NULL;
    size_t tb;
    unsigned seq;
    sv2_aiw_parse(e->aiw, strlen(e->aiw), track, sizeof track, &year);
    sw_local_of(&e->local, &lt);
    sw_names_of(e, track, country, sizeof country, mode, sizeof mode, session, sizeof session, &has_session);
    sv2_weekend_key(wkey, sizeof wkey, country, year, mode);
    seq = (unsigned)sv2_weekend_next(&sw_wk, wkey, e->session_type);
    if (!sw_wr.folder[0] || seq != sw_wr.weekend_seq) {
        sv2_weekend_stem(stem, sizeof stem, country, year, mode, &lt);
        sv2_unique_stem(stem, sizeof stem, sw_name_as_dir, NULL);
        sw_fmt(sw_wr.folder, sizeof sw_wr.folder, "%s%s", sw_rec_dir, stem);
        if (!CreateDirectoryA(sw_wr.folder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            sw_wr.folder[0] = 0; sw_wr.io_failed = 1; return;
        }
        sv2_copy(sw_wr.folder_name, sizeof sw_wr.folder_name, stem);
        sw_wr.weekend_seq = seq;
        sv2_copy(sw_wr.weekend, sizeof sw_wr.weekend, stem);
    }
    sv2_session_stem(stem, sizeof stem, country, year, mode, has_session ? session : NULL, &lt);
    sv2_unique_stem(stem, sizeof stem, sw_name_in_folder, NULL);
    sw_fmt(part, sizeof part, "%s\\%s.f1t.part", sw_wr.folder, stem);
    sw_wr.f = fopen(part, "wb");
    if (!sw_wr.f) { sw_wr.io_failed = 1; sw_wr.have = 0; return; }
    sv2_copy(sw_wr.stem, sizeof sw_wr.stem, stem);
    sv2_copy(sw_wr.part_path, sizeof sw_wr.part_path, part);
    sv2_copy(sw_wr.track, sizeof sw_wr.track, track);
    sv2_copy(sw_wr.aiw, sizeof sw_wr.aiw, e->aiw);
    sw_wr.season = year;
    sv2_copy(sw_wr.country, sizeof sw_wr.country, country);
    sv2_copy(sw_wr.mode, sizeof sw_wr.mode, mode);
    sv2_copy(sw_wr.session, sizeof sw_wr.session, session);
    sw_wr.has_session = has_session;
    sw_wr.session_type = e->session_type;
    sw_wr.gen = e->generation;
    sw_wr.local = e->local;
    sw_wr.utc = e->utc;
    sw_wr.tz_min = sw_tz_minutes();
    sw_wr.start_unix = sw_unix_of(&e->utc);
    sw_wr.have = 1;
    sw_wr.complete = 0;
    sw_wr.io_failed = 0;
    sw_wr.blk_seq = sw_wr.data_blocks = sw_wr.total_ticks = sw_wr.session_dropped = 0;
    sw_wr.blk_ticks = sw_wr.blk_n = 0;
    sw_wr.bytes = 0;
    sw_wr.clamped = 0;
    sw_wr.events = 0;
    sw_wr.alg = sv2_alg_available(SV2_COMP_XPRESS) ? SV2_COMP_XPRESS : SV2_COMP_NONE;
    sw_wr.flags = (sw_wr.alg == SV2_COMP_NONE ? SV2_FLAG_UNCOMPRESSED_FALLBACK : SV2_FLAG_COMPRESSED_BLOCKS) | SV2_FLAG_HAS_SIM;
    table = (uint8_t *)malloc(8192);
    if (!table) { sw_wr.io_failed = 1; return; }
    sv2_catalog_to_table(sw_wr.chan);
    tb = sv2_table_encode(sw_wr.chan, SV2_CATALOG_COUNT, table, 8192);
    memset(&h, 0, sizeof h);
    h.flags = sw_wr.flags;
    h.compression_id = sw_wr.alg;
    h.block_ticks_max = SW_BLOCK_TICKS;
    h.block_ms_max = 1000;
    h.channel_table_offset = SV2_HEADER_BYTES;
    h.channel_table_bytes = (uint32_t)tb;
    h.start_unix_utc = sw_wr.start_unix;
    h.tz_offset_min = sw_wr.tz_min;
    h.session_type = sw_wr.session_type;
    h.broker_generation = sw_wr.gen;
    h.channels_declared = SV2_CATALOG_COUNT;
    sv2_copy(h.track, sizeof h.track, track);
    sv2_copy(h.session_name, sizeof h.session_name, stem);
    sw_wr.table_bytes = (uint32_t)tb;
    sv2_header_encode(&h, hb);
    sw_write_bytes(hb, sizeof hb);
    sw_write_bytes(table, tb);
    free(table);
    sw_meta_write(0);
    memset(sw_ev_prev, 0, sizeof sw_ev_prev);
    {   /* 2.1 EVENT session_start: sim time, session type and track as written in the META block */
        char js[SV2_EVENT_MAX], tb[160];
        sw_json_body(tb, sizeof tb, track);
        sw_fmt(js, sizeof js, "{\"type\":\"session_start\",\"t\":%.3f,\"session_type\":%d,\"track\":\"%s\"}",
               e->sim, (int)e->session_type, tb);
        sw_write_event(e->sim, js);
    }
    fflush(sw_wr.f);
    {
        unsigned before = sw_nentries;
        char fname[200];
        sw_fmt(fname, sizeof fname, "%s.f1t.part", stem);
        sw_entry_add(sw_wr.folder_name, fname, sw_wr.weekend, SW_ST_OPEN, sw_wr.bytes, 0);
        sw_wr.entry = sw_nentries > before ? before : SW_ENTRIES;   /* SW_ENTRIES = not tracked */
    }
    sw_entry_update();
}

/* Closes the open session: last block, END, header with CLOSED, then rename .part to .f1t. */
static void sw_close_session(const char *aiw_now, int reason) {
    uint8_t pl[12], hb[SV2_HEADER_BYTES], frame[SV2_BLOCK_HEADER_BYTES + 12];
    sv2_block_t b;
    sv2_header_t h;
    size_t fn;
    char track[64], final_stem[160], final_path[SW_PATHBUF];
    int year = sw_wr.season;
    (void)reason;
    if (!sw_wr.have) return;
    sw_flush_block();
    {   /* drops after the last applied sample are still recorded before END */
        LONG dropped = InterlockedCompareExchange(&sw_ring.dropped, 0, 0);
        if ((uint32_t)dropped > sw_wr.gap_reported) {
            sw_write_gap((uint32_t)dropped - sw_wr.gap_reported, sw_wr.last_sim, sw_wr.last_sim);
            sw_wr.gap_reported = (uint32_t)dropped;
        }
    }
    {   /* 2.1 EVENT session_end: close reason (SWR_*) and the last sim time written */
        char js[SV2_EVENT_MAX];
        sw_fmt(js, sizeof js, "{\"type\":\"session_end\",\"t\":%.3f,\"reason\":%d}", sw_wr.last_sim, reason);
        sw_write_event(sw_wr.last_sim, js);
    }
    sw_meta_write(1);
    sv2_put_le(pl, sw_wr.data_blocks, 4);
    sv2_put_le(pl + 4, sw_wr.total_ticks, 4);
    sv2_put_le(pl + 8, sw_wr.session_dropped, 4);
    memset(&b, 0, sizeof b);
    b.type = SV2_BLOCK_END;
    b.seq = sw_wr.blk_seq;
    b.raw_len = b.stored_len = 12;
    fn = sv2_block_put(frame, sizeof frame, b, pl);
    if (fn) sw_write_bytes(frame, fn); else sw_wr.io_failed = 1;
    /* Track known only now: re-parse the closing AIW when it is valid (spec 3.4 says the name is fixed at close). */
    sv2_copy(track, sizeof track, sw_wr.track);
    if (aiw_now && aiw_now[0]) {
        char t2[64] = "Unknown";
        int y2 = 0;
        sv2_aiw_parse(aiw_now, strlen(aiw_now), t2, sizeof t2, &y2);
        if (strcmp(t2, "Unknown") != 0) { sv2_copy(track, sizeof track, t2); year = y2; }
    }
    (void)track; (void)year;   /* 1.1.1: the file name uses the open-time name parts (sw_wr.country/mode/session) */
    memset(&h, 0, sizeof h);
    h.flags = sw_wr.flags | SV2_FLAG_CLOSED | (sw_wr.complete ? SW_SESSION_COMPLETE : 0u);
    h.compression_id = sw_wr.alg;
    h.block_ticks_max = SW_BLOCK_TICKS;
    h.block_ms_max = 1000;
    h.channel_table_offset = SV2_HEADER_BYTES;
    h.channel_table_bytes = sw_wr.table_bytes;
    h.start_unix_utc = sw_wr.start_unix;
    h.tz_offset_min = sw_wr.tz_min;
    h.session_type = sw_wr.session_type;
    h.broker_generation = sw_wr.gen;
    h.channels_declared = SV2_CATALOG_COUNT;
    sv2_copy(h.track, sizeof h.track, track);
    sv2_copy(h.session_name, sizeof h.session_name, sw_wr.stem);
    sv2_header_encode(&h, hb);
    if (sw_wr.f) {
        if (fseek(sw_wr.f, 0, SEEK_SET) != 0) sw_wr.io_failed = 1;
        else if (fwrite(hb, 1, sizeof hb, sw_wr.f) != sizeof hb) sw_wr.io_failed = 1;
        fflush(sw_wr.f);
        fclose(sw_wr.f);
    }
    sw_wr.f = NULL;
    {
        int state = sw_wr.io_failed ? SW_ST_DAMAGED : SW_ST_CLOSED;
        if (state == SW_ST_CLOSED) {
            sv2_local_t lt;
            sw_local_of(&sw_wr.local, &lt);
            sv2_session_stem(final_stem, sizeof final_stem, sw_wr.country, sw_wr.season, sw_wr.mode,
                             sw_wr.has_session ? sw_wr.session : NULL, &lt);
            sv2_unique_stem(final_stem, sizeof final_stem, sw_name_in_folder, NULL);
            sw_fmt(final_path, sizeof final_path, "%s\\%s.f1t", sw_wr.folder, final_stem);
            if (MoveFileExA(sw_wr.part_path, final_path, 0)) {
                sw_fmt(sw_wr.final_name, sizeof sw_wr.final_name, "%s.f1t", final_stem);
            } else {
                state = SW_ST_DAMAGED;   /* rename failed: .part stays for repair */
            }
        }
        if (sw_wr.entry < sw_nentries) {
            sw_entry_t *en = &sw_entries[sw_wr.entry];
            en->state = state;
            en->bytes = sw_wr.bytes;
            en->ticks = sw_wr.total_ticks;
            if (state == SW_ST_CLOSED) sv2_copy(en->file, sizeof en->file, sw_wr.final_name);
        }
    }
    sw_wr.have = 0;
}

static void sw_apply(const sw_event_t *e) {
    LONG dropped;
    switch (e->kind) {
    case SWE_OPEN:
        if (sw_wr.have) sw_close_session("", 0);
        sw_open_session(e);
        sw_index_write();
        break;
    case SWE_SAMPLE:
        if (!sw_wr.have) break;
        dropped = InterlockedCompareExchange(&sw_ring.dropped, 0, 0);
        if ((uint32_t)dropped > sw_wr.gap_reported && e->n_cars > 0) {
            sw_flush_block();
            sw_write_gap((uint32_t)dropped - sw_wr.gap_reported, sw_wr.last_sim, e->sim);
            sw_wr.gap_reported = (uint32_t)dropped;
        }
        sw_events_from_sample(e);
        sw_block_add(e);
        break;
    case SWE_COMPLETE:
        if (sw_wr.have) sw_wr.complete = 1;
        break;
    case SWE_DRIVER:
        if (sw_wr.have) sw_write_driver_event(e);
        break;
    case SWE_CLOSE:
        if (sw_wr.have) { sw_close_session(e->aiw, e->reason); sw_index_write(); }
        break;
    default:
        break;
    }
}

/* Index rebuild after a job, writer thread only, between writes (never on the sample path). It rescans the
 * folders like the startup scan, then re-pins the open session: its .part entry is OPEN again and
 * sw_wr.entry follows the new index, so the open session stays listed and its writes stay valid. */
static void sw_rebuild_entries(void) {
    char pname[200];
    unsigned found = SW_ENTRIES, before;
    sw_nentries = 0; sw_overflow_bytes = 0;
    sw_scan_folders();
    if (!sw_wr.have) { sw_wr.entry = SW_ENTRIES; return; }
    sw_fmt(pname, sizeof pname, "%s.f1t.part", sw_wr.stem);
    for (unsigned i = 0; i < sw_nentries; i++) {
        if (!strcmp(sw_entries[i].folder, sw_wr.folder_name) && !strcmp(sw_entries[i].file, pname)) { found = i; break; }
    }
    if (found == SW_ENTRIES) {
        before = sw_nentries;
        sw_entry_add(sw_wr.folder_name, pname, sw_wr.weekend, SW_ST_OPEN, sw_wr.bytes, sw_wr.total_ticks);
        found = sw_nentries > before ? before : SW_ENTRIES;   /* SW_ENTRIES = not tracked (table full) */
    }
    if (found < SW_ENTRIES) {
        sw_entries[found].state = SW_ST_OPEN;
        sw_entries[found].bytes = sw_wr.bytes;
        sw_entries[found].ticks = sw_wr.total_ticks;
    }
    sw_wr.entry = found;
}

/* ---------- writer thread ---------- */
static DWORD WINAPI sw_writer_main(void *unused) {
    DWORD last_index = 0;
    int dirty = 0, stop = 0;
    (void)unused;
    for (;;) {
        WaitForSingleObject(sw_wake, 250);
        for (;;) {
            sw_event_t *e = sw_ring_peek();
            if (!e) break;
            if (e->kind == SWE_SHUTDOWN) { sw_ring_pop(); stop = 1; break; }
            sw_apply(e);
            sw_ring_pop();
            dirty = 1;
        }
        if (InterlockedExchange(&sw_rebuild_req, 0)) {
            sw_rebuild_entries();
            dirty = 1;
        }
        if (dirty && GetTickCount() - last_index >= SW_INDEX_MS) {
            sw_index_write();
            last_index = GetTickCount();
            dirty = 0;
        }
        if (stop) break;
    }
    if (sw_wr.have) sw_close_session("", SWR_SHUTDOWN);
    sw_index_write();
    return 0;
}

/* Forward declarations of the jobs thread entry points (storage_jobs.inc, included after this file). */
static void sj_start(void);
static void sj_stop(DWORD timeout_ms);
static void sj_log_retired_keys(void);

/* Allocates buffers, scans the folder, starts the writer. Call after broker registration (spec 6.6). */
static int sw_start(void) {
    if (!sw_enabled) return 0;
    sa_init();
    if (!sw_ring_init()) { sw_enabled = 0; return 0; }
    sw_wr.raw = (uint8_t *)malloc(SW_PAYLOAD_CAP);
    sw_wr.packed = (uint8_t *)malloc(SW_PAYLOAD_CAP);
    sw_wr.frame = (uint8_t *)malloc(SW_FRAME_CAP);
    if (!sw_wr.raw || !sw_wr.packed || !sw_wr.frame) { sw_enabled = 0; return 0; }
    sw_wake = CreateEventA(NULL, FALSE, FALSE, NULL);
    sw_wr.gap_reported = (uint32_t)InterlockedCompareExchange(&sw_ring.dropped, 0, 0);   /* drops before start are not ours */
    sw_scan_folders();
    sw_index_write();
    sw_thread = CreateThread(NULL, 0, sw_writer_main, NULL, 0, NULL);
    if (!sw_thread) { sw_enabled = 0; return 0; }
    sj_log_retired_keys();
    sj_start();     /* jobs thread: startup repair runs after this registration point (spec 6.6) */
    return 1;
}

/* Shutdown: stop the capture side first (no more producers), then drain and close. */
static int sw_stop(DWORD timeout_ms) {
    DWORD start, r;
    sw_event_t *e;
    sj_stop(timeout_ms);     /* jobs first: no job may touch files while the writer closes them */
    if (!sw_thread) return 1;
    start = GetTickCount();
    for (;;) {
        e = sw_ring_claim(SWE_SHUTDOWN);
        if (e) { e->kind = SWE_SHUTDOWN; sw_ring_publish(); break; }
        if (GetTickCount() - start > timeout_ms) break;
        Sleep(1);
    }
    r = WaitForSingleObject(sw_thread, timeout_ms);
    CloseHandle(sw_thread);
    sw_thread = NULL;
    return r == WAIT_OBJECT_0;
}

/* Process exit (1.1.1, T3), called from DllMain DLL_PROCESS_DETACH. Before this fix nothing closed an open session on
 * exit: DETACH only logged, the writer thread was already gone, and the session stayed .part (repaired as "recovered"
 * at the next start). If the writer still runs, stop it through sw_stop (bounded 2 s). Otherwise drain the ring and
 * close the session on this thread. Does nothing when the writer never started. */
static void sw_process_exit(void) {
    sw_event_t *e;
    if (!sw_thread && !sw_wr.have) return;
    if (sw_thread && WaitForSingleObject(sw_thread, 0) == WAIT_TIMEOUT) {
        if (!sw_stop(2000)) log_w("storage: writer still busy at exit after 2 s; open session may need repair at next start");
        return;
    }
    if (sw_thread) { CloseHandle(sw_thread); sw_thread = NULL; }
    while ((e = sw_ring_peek()) != NULL) {
        if (e->kind != SWE_SHUTDOWN) sw_apply(e);
        sw_ring_pop();
    }
    if (sw_wr.have) sw_close_session("", SWR_SHUTDOWN);
    sw_index_write();
    log_f("storage: session closed at process exit");
}
