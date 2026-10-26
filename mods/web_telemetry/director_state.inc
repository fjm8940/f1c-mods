/* director_state.inc - TV Director PHASE 2 (2026-10-14; phase 1 2026-10-13). Reads always; writes only with [Director] Enabled=1, and car writes also need CarSwitch=1.
 * Included by web_telemetry.c before storage_v2.inc. Writes: broker game-thread tasks only (section 'TV Director phase 1'); car change (2026-10-14): an AI car gets index-1 in 0x7C54DC and DIK 78 (numpad +); the player car and RESTORE inject DIK 156 (GAME_CONTROL.md section 4, owner corrections). Spec: TV_DIRECTOR_SPEC_2026-10-13.md (sections 4.2, 6.2, 6.3, 13).
 *
 * Threading (same model as the mod's game sampling, see compatibility_runtime.inc):
 *   - dcam_try_resolve() runs on the telemetry worker thread. It reads the executable sections with copy_game
 *     (guarded ReadProcessMemory) and never calls game code. Retries every 2 s, at most DCAM_MAX_ATTEMPTS.
 *   - dcam_capture_locked() runs INSIDE the broker capture callback (game thread, under g_telemetry_cs,
 *     TryEnter only). It reads a handful of guarded words and writes g_dcam_snap.
 *   - dcam_state_json() runs on the HTTP thread. It copies the snapshot under g_telemetry_cs and parses the
 *     names file (disk, not game memory). It never calls game code.
 *
 * Copied parts of development\mods\camera_editor\plugin\cam_sig.h (the Camera Editor is the source; this copy is
 * read-only and does not change it):
 *   C-a  cam_hex/cam_compile/cam_find/cam_u32            -> dcam_hex/dcam_compile/dcam_find/dcam_u32 (cam_one replaced by dcam_count1)
 *   C-b  cam_resolve, WITHOUT the APPLY (line 46) and LOCALCAM (40-43) patterns and without camera_apply
 *   C-c  CamAddrs, subset: director_ptr, manager, dispatch, mirror, target_ptr, player_ptr, active_ptrs,
 *        veh_base, veh_stride, roster_count
 *   C-d  DIRECTOR_MODE 0x34, COUNT 0x38, INDEX 0x3C (camera_editor.c:38-40)
 *   C-e  read pattern of fisheye_strength_now (camera_editor.c:466-473): director = *director_ptr, plausible,
 *        mode = director+0x34, index = director+0x3C
 *   C-g  parse_file LocalCam rule (camera_editor.c:186-243, names only) and read_save_dir (:825-834)
 *   C-h  NOT copied (profiles, write, rigid mount, fisheye apply, detours, shared memory).
 */

/* ---- C-a/C-b/C-c: copied signature resolver (cam_sig.h:59-146, reduced) ---- */
#define DCAM_SIG_MAX              96
#define DCAM_EXPECTED_VEH_STRIDE  0x48C0u           /* cam_sig.h:32 */
#define DCAM_DIRECTOR_FROM_MANAGER 0x18u            /* cam_sig.h:33 */
/* Camera switch routine. cam_sig.h:47 (CAM_SIG_DISPATCH) is "A0????????" + 7 bytes. Camera Editor's dispatch_hook
 * overwrites the first DISPATCH_STOLEN = 12 bytes with E9 rel32 + 7 x 90 (camera_editor.c:52, detour 712-731).
 * That is why the whole-pattern match failed in the live game (2026-10-13). The match is therefore made on the body
 * after the 12-byte prologue (offset 0x40944C here, unique in .text) and the routine start is 12 bytes before it. */
#define DCAM_DISPATCH_PROLOGUE 12u
#define DCAM_SIG_DISPATCH_BODY "8B4424108BF1BF02000000752183F803751C8B0D"
#define DCAM_SIG_MIRROR   "568BF18B0D????????57E8????????85C00F84????????68????????8BC8E8????????8BF885FF0F84"  /* :48 */
#define DCAM_SIG_DIRECTOR "A1????????3BD0751083783401"                       /* :49, +1 director_ptr */
#define DCAM_SIG_SETVEH   "8B4C240469C9????????81C1????????890D????????33C0390C85????????7409"  /* :50 */
                                                       /* +6 stride, +12 base, +18 written ptr, +27 active ptrs */
#define DCAM_SIG_PLAYER   "A1????????8B8894480000894E2C895E28"               /* :52, +1 player_ptr */
#define DCAM_SIG_ROSTER   "E8????????A1????????85C07F38684F010000"           /* :53, +6 roster_count */

typedef struct DcamAddrs {
    uint32_t director_ptr;   /* address holding the active camera director (0x5EA528 in this build) */
    uint32_t manager;        /* director_ptr - 0x18 (0x5EA510) */
    uint32_t view_dispatch;  /* ViewCameraModeDispatch (0x409440) */
    uint32_t mirror_setup;   /* cockpit mirror setup (0x409890) */
    uint32_t target_ptr;     /* followed vehicle pointer global (0x7C54E0) */
    uint32_t player_ptr;     /* player vehicle pointer global (0x7C54E8) */
    uint32_t active_ptrs;    /* vehicle pointer table (0x7C54F0) */
    uint32_t veh_base;       /* vehicle array (0x5EC6D8) */
    uint32_t veh_stride;     /* vehicle stride (0x48C0) */
    uint32_t roster_count;   /* roster count global (0x8AC390) */
} DcamAddrs;

typedef struct DcamPattern { uint8_t bytes[DCAM_SIG_MAX]; uint8_t mask[DCAM_SIG_MAX]; size_t len; } DcamPattern;

static int dcam_hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int dcam_compile(const char *text, DcamPattern *p)
{
    p->len = 0;
    while (*text) {
        if (*text == ' ') { ++text; continue; }
        if (!text[1] || p->len >= DCAM_SIG_MAX) return 0;
        if (text[0] == '?' && text[1] == '?') {
            p->bytes[p->len] = 0; p->mask[p->len] = 0;
        } else {
            int hi = dcam_hex(text[0]), lo = dcam_hex(text[1]);
            if (hi < 0 || lo < 0) return 0;
            p->bytes[p->len] = (uint8_t)(hi * 16 + lo); p->mask[p->len] = 1;
        }
        ++p->len;
        text += 2;
    }
    return p->len > 0;
}

/* Every match offset (up to max) in [data, data+len). Returns the match count (may exceed max). */
static size_t dcam_find(const uint8_t *data, size_t len, const char *pattern, size_t *found, size_t max)
{
    DcamPattern p;
    size_t count = 0, i, j;
    if (!dcam_compile(pattern, &p) || len < p.len) return 0;
    for (i = 0; i + p.len <= len; ++i) {
        if (data[i] != p.bytes[0]) continue;
        for (j = 1; j < p.len; ++j)
            if (p.mask[j] && data[i + j] != p.bytes[j]) break;
        if (j == p.len) {
            if (count < max) found[count] = i;
            ++count;
        }
    }
    return count;
}

static uint32_t dcam_u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }


/* Exactly-one count; *at receives the offset when the count is 1. */
static size_t dcam_count1(const uint8_t *data, size_t len, const char *pattern, size_t *at)
{
    size_t found[2], n = dcam_find(data, len, pattern, found, 2);
    if (n == 1 && at) *at = found[0];
    return n;
}

/* Diagnostic line builder: appends text to diag (no-op when diag is NULL). */
static void dcam_diag_put(char *diag, size_t cap, const char *text)
{
    size_t L;
    if (!diag || cap == 0) return;
    L = strlen(diag);
    if (L < cap) snprintf(diag + L, cap - L, "%s", text);
}

static void dcam_diag_count(char *diag, size_t cap, const char *name, size_t count)
{
    char one[48];
    snprintf(one, sizeof one, "%s=%u ", name, (unsigned)count);
    dcam_diag_put(diag, cap, one);
}

/* Routine start (12 bytes before the body anchor): the original prologue, or Camera Editor's detour form
 * (E9 rel32 at +0 and NOP fill to +12). Any other prologue is refused. */
static int dcam_prologue_ok(const uint8_t *fn, int *detoured)
{
    static const uint8_t tail[7] = { 0x53, 0x56, 0x33, 0xDB, 0x57, 0x3A, 0xC3 };   /* push ebx; push esi; xor ebx,ebx; push edi; cmp al,bl */
    unsigned k;
    *detoured = 0;
    if (fn[0] == 0xA0 && memcmp(fn + 5, tail, sizeof tail) == 0) return 1;         /* mov al,[abs]; the original 12 bytes */
    if (fn[0] == 0xE9) {
        for (k = 5; k < DCAM_DISPATCH_PROLOGUE; ++k) if (fn[k] != 0x90) return 0;
        *detoured = 1;
        return 1;
    }
    return 0;
}

/* cam_sig.h cam_resolve (114-146), reduced. va = virtual address of code[0]. Every pattern is counted first, so
 * diag (may be NULL, size diagcap) gets one "name=count" entry per pattern for the log, including the failing ones. */
static int dcam_resolve(const uint8_t *code, size_t len, uint32_t va, DcamAddrs *out, const char **why,
                        char *diag, size_t diagcap)
{
    size_t at_disp = 0, at_mir = 0, at_dir = 0, at_pl = 0, at_ro = 0, setv[4], n, i;
    size_t n_disp, n_mir, n_dir, n_pl, n_ro;
    uint32_t written[4], other = 0;
    int others = 0, detoured = 0;
    memset(out, 0, sizeof *out);
    n_disp = dcam_count1(code, len, DCAM_SIG_DISPATCH_BODY, &at_disp);
    n_mir  = dcam_count1(code, len, DCAM_SIG_MIRROR, &at_mir);
    n_dir  = dcam_count1(code, len, DCAM_SIG_DIRECTOR, &at_dir);
    n_pl   = dcam_count1(code, len, DCAM_SIG_PLAYER, &at_pl);
    n_ro   = dcam_count1(code, len, DCAM_SIG_ROSTER, &at_ro);
    n = dcam_find(code, len, DCAM_SIG_SETVEH, setv, 4);
    dcam_diag_count(diag, diagcap, "dispatch_body", n_disp);
    dcam_diag_count(diag, diagcap, "mirror", n_mir);
    dcam_diag_count(diag, diagcap, "director", n_dir);
    dcam_diag_count(diag, diagcap, "player", n_pl);
    dcam_diag_count(diag, diagcap, "roster", n_ro);
    dcam_diag_count(diag, diagcap, "setveh", n);
    if (n_disp != 1 || at_disp < DCAM_DISPATCH_PROLOGUE) { *why = "camera switch routine"; return 0; }
    if (!dcam_prologue_ok(code + at_disp - DCAM_DISPATCH_PROLOGUE, &detoured)) {
        *why = "camera switch routine (prologue is neither the original nor a 5-byte jump)"; return 0;
    }
    if (detoured) dcam_diag_put(diag, diagcap, "dispatch_prologue=jump(detoured by another mod) ");
    out->view_dispatch = va + (uint32_t)(at_disp - DCAM_DISPATCH_PROLOGUE);
    if (n_mir != 1) { *why = "mirror setup routine"; return 0; }
    out->mirror_setup = va + (uint32_t)at_mir;
    if (n_dir != 1) { *why = "camera director"; return 0; }
    out->director_ptr = dcam_u32(code + at_dir + 1);
    if (n_pl != 1) { *why = "player vehicle pointer"; return 0; }
    out->player_ptr = dcam_u32(code + at_pl + 1);
    if (n_ro != 1) { *why = "vehicle roster"; return 0; }
    out->roster_count = dcam_u32(code + at_ro + 6);
    if (n < 2 || n > 4) { *why = "vehicle pointer setters"; return 0; }
    for (i = 0; i < n; ++i) {
        const uint8_t *s = code + setv[i];
        written[i] = dcam_u32(s + 18);
        if (i == 0) {
            out->veh_stride = dcam_u32(s + 6);
            out->veh_base = dcam_u32(s + 12);
            out->active_ptrs = dcam_u32(s + 27);
        } else if (dcam_u32(s + 6) != out->veh_stride || dcam_u32(s + 12) != out->veh_base) {
            *why = "vehicle pointer setters disagree"; return 0;
        }
        if (written[i] != out->player_ptr) { other = written[i]; ++others; }
    }
    if (others != 1) { *why = "camera target pointer"; return 0; }
    out->target_ptr = other;
    if (out->veh_stride != DCAM_EXPECTED_VEH_STRIDE) { *why = "vehicle layout (different code base)"; return 0; }
    if (out->director_ptr < DCAM_DIRECTOR_FROM_MANAGER) { *why = "camera director"; return 0; }
    out->manager = out->director_ptr - DCAM_DIRECTOR_FROM_MANAGER;
    return 1;
}

/* ---- C-d: director struct offsets used in phase 0 (camera_editor.c:38-40) ---- */
#define DCAM_MODE   0x34u
#define DCAM_COUNT  0x38u
#define DCAM_INDEX  0x3Cu
#define DCAM_MODE_ONBOARD 4u                 /* camera_editor.c:459: 0..3 fixed, 4 = ONBOARD index */
#define DCAM_MODE_CIRCUIT 5u                 /* owner T1: not a director mode; the camera manager's TrackingCam state 5 (PGDN) */

/* Vehicle fields (FACT, web_telemetry.c OFF_* and findings): roster id is veh+0x4894, the value the chain
 * CameraViewAssignment compares with (functions\CameraViewAssignment_0040c650.c:6, 18580 = 0x4894). The roster
 * index 0x4898 (Camera Editor VEH_ROSTER_INDEX) and driver id 0x489C (OFF_DRIVER_ID) are read raw for comparison. */
#define DCAM_ROSTER_ID    0x4894u
#define DCAM_ROSTER_INDEX 0x4898u

/* ---- View owner (2026-10-13 fix). FACT (TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION_2026-10-13.md 4.3-4.4): the manager
 * (director_ptr - 0x18) holds the active view pointer at +0x08 (0x5EA518). The game's own test that the director is on
 * screen is equality with the director object (ModeBasedDispatch_0049f790.c:9). StaticCam group at +0x1C (0x5EA52C),
 * TrackingCam group at +0x20 (0x5EA530); each group's entry count is at +0x10. Vtables come from the on-disk EXE .rdata.
 * Read only: no call into game code, no write. */
#define DCAM_VIEW_ACTIVE_OFF  0x08u
#define DCAM_VIEW_STATIC_OFF  0x1Cu
#define DCAM_VIEW_TRACK_OFF   0x20u
#define DCAM_GROUP_COUNT_OFF  0x10u
#define DCAM_GROUP_MAX        128u
#define DCAM_VT_DIRECTOR      0x0059E60Cu
#define DCAM_VT_STATIC        0x0059E678u
#define DCAM_VT_TRACKING      0x0059E6ACu
#define DCAM_OWNER_UNKNOWN          0
#define DCAM_OWNER_DIRECTOR         1
#define DCAM_OWNER_CIRCUIT_STATIC   2
#define DCAM_OWNER_CIRCUIT_TRACKING 3

typedef char dcam_stride_is_18624[(VEH_STRIDE == 0x48C0u) ? 1 : -1];   /* this mod's pool stride */

#define DCAM_NOT_TRIED 0
#define DCAM_RETRY     1
#define DCAM_RESOLVED  2
#define DCAM_REFUSED   3
#define DCAM_MAX_ATTEMPTS 60
#define DCAM_SCAN_CHUNK 65536u
#define DCAM_SCAN_MAX   0x2000000u
#define DCAM_FIXED_COUNT 4
#define DCAM_NAME_MAX  32            /* camera_editor.c NAME_LEN */
#define DCAM_MAX_CAMS  64            /* camera_editor.c MAX_CAMS */
#define DCAM_REASON_MAX 224          /* refusal text: longest is about 200 bytes */

static const char *const DCAM_FIXED_NAMES[DCAM_FIXED_COUNT] = { "NOSECAM", "COCKPIT", "TVCOCKPIT", "SWINGMAN" };

static volatile LONG g_dcam_state = DCAM_NOT_TRIED;   /* written under g_telemetry_cs; read lock-free by the callback */
static int g_dcam_attempts;                             /* worker thread only */
static int g_dcam_diag_logged;                         /* worker thread only: the per-pattern line is logged once */
static DcamAddrs g_dcam_addr;                           /* valid when DCAM_RESOLVED; written under g_telemetry_cs */
static char g_dcam_reason[DCAM_REASON_MAX] = "not tried yet";  /* under g_telemetry_cs */
static int g_dcam_enabled = 0;                          /* web_telemetry.ini [Director] Enabled (default 0) */
static int g_dcam_carswitch_enabled = 0;               /* web_telemetry.ini [Director] CarSwitch (default 0, phase 2) */
static volatile LONG g_dcam_hide_hud = 1;              /* [Director] HideHudOnCinematic (default 1, owner T4); the TV Director page switches it (POST /director/options) */
static int g_dcam_mute_commentary = 1;                 /* web_telemetry.ini [Director] MuteCarChangeCommentary (default 1, owner E1) */

/* One consistent sample, written by the capture callback under g_telemetry_cs. */
typedef struct DcamSnap {
    int valid;                     /* director read and plausible */
    uint32_t director, mode, count, index;
    int player_ok;
    uint32_t player_ptr, player_roster;
    int followed;                  /* followed pointer is a vehicle of the pool */
    uint32_t followed_ptr, followed_roster, followed_roster_index, followed_driver_id;
    int table_index;               /* 0x7C54DC, the mirrored spectated slot (raw) */
    double sim_time;               /* session time, sim seconds (raw mirror) */
    int owner;                     /* DCAM_OWNER_*: what the active view pointer selects */
    int dir_vt_ok;                 /* director object vtable == DCAM_VT_DIRECTOR */
    uint32_t active_val;           /* raw value at manager+0x08 (0 = not readable) */
    uint32_t static_val, tracking_val;   /* raw values at manager+0x1C / +0x20 (0 = not readable) */
    int static_vt_ok, tracking_vt_ok;
    int circuit_count_ok;          /* circuit_count valid for the owning group */
    uint32_t circuit_count;        /* entry count (+0x10) of the owning circuit group: a count, not an index */
    char driver_name[NAME_LEN], car_name[NAME_LEN];
    int session_live;              /* phase 2: capture sample taken in a live race session (sampled) */
    int32_t manager_state;         /* phase 2: manager this[3] (+0x0C), diagnostic */
    int manager_state_ok;
    uint32_t view_index, view_target, circuit_flag;   /* 2026-10-14 diagnostics: 0x7C54DC, 0x7C54E0, 0x7CFCC8 (reads only) */
    int view_index_ok, view_target_ok, circuit_flag_ok;
} DcamSnap;
static DcamSnap g_dcam_snap;

/* Onboard names (C-g). Worker-free: refreshed on the HTTP thread only. */
static char g_dcam_onboard[DCAM_MAX_CAMS][DCAM_NAME_MAX];
static int g_dcam_onboard_n;
static int g_dcam_names_ok;
static DWORD g_dcam_names_at;

static int dcam_plausible(uint32_t p) { return p >= 0x10000u && p <= 0x7FFE0000u && (p & 3u) == 0; }   /* camera_editor.c:152 */

static int dcam_in_pool(uint32_t p)
{
    return p >= VEH_BASE && p < VEH_BASE + VEH_STRIDE * VEH_SLOTS && (p - VEH_BASE) % VEH_STRIDE == 0;
}

static void dcam_clean_name(char *s, size_t cap)
{
    size_t i;
    if (cap == 0) return;
    s[cap - 1] = 0;
    for (i = 0; s[i]; ++i) if ((unsigned char)s[i] < 32 || (unsigned char)s[i] > 126) s[i] = '?';
}

/* Pure (unit-tested offline): which system the active view pointer selects. Owner is named only when the pointer
 * equals the director object or a group object AND that object's vtable matches. Anything else is unknown. */
static int dcam_view_owner(int have_active, uint32_t active, uint32_t director, int dir_vt_ok,
                           uint32_t stat, int stat_vt_ok, uint32_t trk, int trk_vt_ok)
{
    if (!have_active || !dcam_plausible(director)) return DCAM_OWNER_UNKNOWN;
    if (dir_vt_ok && active == director) return DCAM_OWNER_DIRECTOR;
    if (stat_vt_ok && stat && active == stat) return DCAM_OWNER_CIRCUIT_STATIC;
    if (trk_vt_ok && trk && active == trk) return DCAM_OWNER_CIRCUIT_TRACKING;
    return DCAM_OWNER_UNKNOWN;
}

/* Read only: a group object's vtable (+0x00, identity) and entry count (+0x10, a count, not an index). */
static void dcam_group_probe(uint32_t ptr, uint32_t vt_want, int *vt_ok, uint32_t *count, int *count_ok)
{
    uint32_t vt = 0, n = 0;
    *vt_ok = 0; *count = 0; *count_ok = 0;
    if (!dcam_plausible(ptr) || !copy_game(&vt, ptr, 4) || vt != vt_want) return;
    *vt_ok = 1;
    if (copy_game(&n, (uintptr_t)ptr + DCAM_GROUP_COUNT_OFF, 4) && n <= DCAM_GROUP_MAX) { *count = n; *count_ok = 1; }
}

static const char *dcam_hex_or_null(char *buf, size_t cap, uint32_t v)
{
    if (!v) snprintf(buf, cap, "null");
    else snprintf(buf, cap, "\"0x%08lX\"", (unsigned long)v);
    return buf;
}

/* ---- TV Director phase 1: commands (2026-10-13). Spec 4.4, 4.5, 6.2, 6.3, 9; owner D-TV1 (broker, no detour), D-TV2.
 * Path: POST handler (HTTP thread) -> one pending slot -> telemetry broker task (game thread, dcam_cmd_task) ->
 * writes -> read-back -> result. The HTTP thread never calls game code and never writes game memory. Writes run only
 * when [Director] Enabled=1 (default 0). Phase 2: a car change also needs [Director] CarSwitch=1 (see the car change block). */
#define DCAM_CMD_CUT      1
#define DCAM_CMD_ANGLE    2
#define DCAM_CMD_RESTORE  3
#define DCAM_CMD_WAIT_MS  1000u
#define DCAM_BODY_MAX     4096
#define DCAM_VEH_ACTIVE   0x32C8u     /* live pool slot: int32 at +0x32C8 is not -1 (camera_editor.c VEH_ACTIVE) */
#define DCAM_MIRROR_READY 0x2Du       /* manager+0x2D (camera_editor.c MANAGER_MIRROR_READY) */
/* ---- TV Director camera change (2026-10-14, final owner design). The game's camera-target path ends in GameStateDispatchChain
 * 0x40D240 (thiscall, ECX = camera manager, one stack argument = roster id, callee pops it). Numpad Enter (0x40D340) calls it with the
 * roster of the player car (dword_7C54F0[dword_7C54E4]); numpad + / - (CameraTargetRaceLeader 0x40D290) call it with a standings car.
 * This mod makes ONE such call on the game thread for the car it was asked for, after a byte check of the 40 bytes at 0x40D240 (the
 * reviewed prologue, verified against F1 Challenge Online.exe at file offset 0xD240). It then verifies on the next frames that the
 * followed car (0x7C54E0) is the target, and applies the angle (dcam_set_angle). No key queue, no write of 0x7C54DC and no direct
 * 0x42BBD0 call: those were tested and rejected (GAME_CONTROL.md section 4; findings keyboard-action-queue-background-input,
 * numpadenter-returns-camera-to-player-car). The chain's side effects are the ones a numpad key has. */
#define DCAM_CHAIN_ADDR        0x0040D240u  /* GameStateDispatchChain (thiscall, one stack argument: roster id) */
#define DCAM_CHAIN_LEN         40u
static const uint8_t DCAM_CHAIN_PROLOGUE[DCAM_CHAIN_LEN] = {
    0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0xF1, 0x57, 0xB9, 0xD0, 0xC6, 0x5E, 0x00, 0xE8, 0x7D, 0xE9, 0x01, 0x00, 0x8B,
    0xCE, 0xE8, 0x46, 0xF5, 0xFF, 0xFF, 0x57, 0x8B, 0xCE, 0xE8, 0xEE, 0xF3, 0xFF, 0xFF, 0x8B, 0xCE, 0xE8, 0x27, 0xFE, 0xFF };
#define DCAM_VIEW_INDEX_ADDR   0x007C54DCu  /* viewed table index: read only (diagnostics) */
#define DCAM_VIEW_TARGET_ADDR  0x007C54E0u  /* followed vehicle pointer: set by the game, read only here */
#define DCAM_PTR_TABLE_ADDR    0x007C54F0u  /* dword_7C54F0: active vehicle pointer table */
#define DCAM_PLAYER_SLOT_ADDR  0x007C54E4u  /* dword_7C54E4: the player's table slot (numpad Enter reads it) */
#define DCAM_MGR_STATE_OFF     0x0Cu        /* manager this[3] (research A11): diagnostic only */
#define DCAM_WATCH_MAX_STEPS   10           /* owner: give up after about 10 frames; one step = one game frame */
#define DCAM_WATCH_MAX_MS      5000         /* wall-clock guard: a paused or unreadable game gives up here */
#define DCAM_VERIFY_NONE     0
#define DCAM_VERIFY_PENDING  1
#define DCAM_VERIFY_TRUE     2
#define DCAM_VERIFY_FALSE    3
#define DCAM_STEP_CAR        1
#define DCAM_STEP_ANGLE      2
#define DCAM_WAIT_DONE      0
#define DCAM_WAIT_RUNNING   1
#define DCAM_WAIT_ABANDONED 2
#define DCAM_STATUS_MAX   16
#define DCAM_RESULT_REASON 240

typedef int (__attribute__((thiscall)) *DcamChainFn)(void *manager, int roster);

typedef int (__attribute__((thiscall)) *DcamDispatchFn)(void *receiver, int mode, int direction, int immediate);
typedef int (__attribute__((thiscall)) *DcamMirrorFn)(void *director, int argument);
/* T1 circuit camera (RESEARCH_T1_CIRCUIT_CAM_2026-10-14.md, findings t1-circuitcam-*): PGDN = DIK 209 in KeyboardInputActionMapper
 * 0x40BE70: if (CameraLockCheck(5) && !GameActionStateMachine(5, 1)). Both thiscall on the camera manager 0x5EA510. State 5 makes the
 * TrackingCam group the active view; it follows the car at 0x7C54E0, so it is called after our car change. */
typedef char (__attribute__((thiscall)) *DcamLockFn)(void *manager, int state);
typedef int (__attribute__((thiscall)) *DcamStateFn)(void *manager, int state, int immediate);
#define DCAM_LOCKCHECK_ADDR  0x0040C720u   /* CameraLockCheck */
#define DCAM_STATEMACH_ADDR  0x0040CDE0u   /* GameActionStateMachine */
#define DCAM_PROLOGUE16      16u
static const uint8_t DCAM_LOCKCHECK_PROLOGUE[DCAM_PROLOGUE16] = {
    0x8B, 0x15, 0xE4, 0x54, 0x7C, 0x00, 0x8B, 0x14, 0x95, 0xF0, 0x54, 0x7C, 0x00, 0x56, 0x8B, 0x35 };
static const uint8_t DCAM_STATEMACH_PROLOGUE[DCAM_PROLOGUE16] = {
    0x53, 0x55, 0x8B, 0x6C, 0x24, 0x0C, 0x56, 0x57, 0x33, 0xFF, 0x32, 0xDB, 0x83, 0xFD, 0x0A, 0x8B };
static int32_t g_dcam_prev_mgr_state = -1;   /* manager state before our circuit camera; game thread only */

typedef struct DcamCmd {
    int kind;                       /* DCAM_CMD_* */
    uint32_t seq;
    char driver[NAME_LEN];          /* CUT only: exact driver name (vehicle +0x4620) */
    int mode;                       /* 0..4 (4 = ONBOARD); unused for RESTORE */
    uint32_t index;                 /* ONBOARD index */
    int position;                   /* CUT by race position (1..104) instead of the driver name; 0 = by name */
} DcamCmd;

typedef struct DcamResult {
    uint32_t seq;
    int done;
    char status[DCAM_STATUS_MAX];   /* applied | partial | refused | redirected | queued */
    char reason[DCAM_RESULT_REASON];
    DWORD thread_id;                /* game thread that ran the command (0 = never ran) */
    int readback_ok;
    uint32_t mode, index;           /* read back after the write */
    int car_step;                   /* car change: the car step (index arm or DIK) reached the car */
    int viewed_slot_ok;             /* phase 2: pool slot of the followed car read back after the car step */
    uint32_t viewed_slot;
    int verify_state;               /* DCAM_VERIFY_*: next-frame verify */
    int exp_check_car;              /* verify also compares the followed car */
    uint32_t exp_mode, exp_index, exp_veh;
    int nf_ok;                      /* next-frame sample taken */
    uint32_t nf_mode, nf_index, nf_veh;
    uint32_t verify_capture_seq;    /* capture_sequence of the verifying sample */
} DcamResult;

static DcamCmd g_dcam_cmd;                   /* under g_telemetry_cs */
static int g_dcam_cmd_pending;               /* under g_telemetry_cs */
static uint32_t g_dcam_cmd_seq;              /* under g_telemetry_cs */
static DcamResult g_dcam_result;             /* under g_telemetry_cs */
static int g_dcam_saved_ok;                  /* under g_telemetry_cs: first valid sample of a game session */
static uint32_t g_dcam_saved_gen, g_dcam_saved_mode, g_dcam_saved_index;

/* Game thread only. One car change in progress (2026-10-14): started by dcam_car_begin in a broker task, advanced by
 * dcam_watch_step once per capture sample (capture_on_game_thread). The HTTP thread never reads it. */
typedef struct DcamWatch {
    int active;
    uint32_t veh;               /* target vehicle pointer */
    uint32_t slot;              /* its pool slot (reported as viewed_slot) */
    int steps;                  /* game frames since the chain call */
    uint32_t want_mode, want_index;
    DcamResult r;               /* result under construction */
    int posted;                 /* one watch task is queued in the broker (worker sets, game thread clears) */
    int sim_seen;               /* last_sim is valid */
    double last_sim;            /* game session time at the last step: one step per sim advance */
    DWORD t0;                   /* GetTickCount at the start (wall-clock guard) */
} DcamWatch;
static DcamWatch g_dcam_watch;
static DcamWatch g_dcam_watch;

static void dcam_res(DcamResult *r, const char *status, const char *reason)
{
    /* _snprintf with an explicit terminator: the same truncation as before, without -Wformat-truncation */
    _snprintf(r->status, sizeof r->status - 1, "%s", status);
    r->status[sizeof r->status - 1] = '\0';
    _snprintf(r->reason, sizeof r->reason - 1, "%s", reason);
    r->reason[sizeof r->reason - 1] = '\0';
}

/* ---- TV Director car change: pure helpers (offline tests in runtime_test.c test_director_phase2). No game memory. ---- */
static int dcam_slot_of(uint32_t veh, uint32_t *slot)
{
    if (!dcam_in_pool(veh)) return 0;
    *slot = (veh - VEH_BASE) / VEH_STRIDE;
    return 1;
}

/* CameraModeChangeRequest 0x40C5B0 (raw Hex-Rays): when the viewed car is not the player, states -1 and 1 always, and state 2
 * only while dword_7CFCC8 == 0, hand the view to a circuit camera. DIAGNOSTIC ONLY since 2026-10-14: the car change is not gated
 * on it (the game applies its own rule to its own state). Exposed in GET /director/state. */
#define DCAM_CIRCUIT_FLAG_ADDR 0x007CFCC8u
static int dcam_manager_state_ok(int32_t state, int32_t circuit_flag)
{
    if (state == -1 || state == 1) return 0;
    if (state == 2 && circuit_flag == 0) return 0;
    return 1;
}

/* Byte check of GameStateDispatchChain: the 40 bytes read from the running game must equal the reviewed prologue. */
static int dcam_chain_prologue_ok(const uint8_t *p)
{
    return memcmp(p, DCAM_CHAIN_PROLOGUE, DCAM_CHAIN_LEN) == 0;
}

/* Table entry of the player (numpad Enter reads dword_7C54F0[dword_7C54E4]). Pure: slot and entry are read by the caller. */
static int dcam_player_pick(uint32_t slot, uint32_t entry, uint32_t *veh)
{
    if (slot >= VEH_SLOTS || !dcam_in_pool(entry)) return 0;
    *veh = entry;
    return 1;
}

/* Race position (vehicle +0x4060 = the field the telemetry driver list uses): the single live pool slot with that position.
 * pos[i] / act[i] are read per pool slot; act[i] == -1 means not live. Returns 1 and the slot index, or 0 with a reason. */
static int dcam_pick_position(const int32_t *pos, const int32_t *act, int n, int32_t want, uint32_t *slot, const char **why)
{
    int i, hits = 0;
    uint32_t pick = 0;
    if (want < 1 || want > n) { *why = "position is out of range (1 to 104); nothing was written"; return 0; }
    for (i = 0; i < n; ++i) {
        if (act[i] == -1 || pos[i] <= 0) continue;
        if (pos[i] == want) { hits++; pick = (uint32_t)i; }
    }
    if (hits == 0) { *why = "no live car is at that race position; nothing was written"; return 0; }
    if (hits > 1) { *why = "more than one live car shows that race position; nothing was written"; return 0; }
    *slot = pick;
    return 1;
}

/* Whole number value of a small body key ("key": 12). Returns 1 and the value, or 0 when absent or not digits. */
static int dcam_json_int(const char *body, const char *key, int *out)
{
    char pat[48];
    const char *p;
    long v = 0;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(body, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (!(*p >= '0' && *p <= '9')) return 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > 100000) v = 100000;
        p++;
    }
    *out = (int)v;
    return 1;
}

/* Gate for a camera change, in order. NULL = every gate passes; otherwise the refusal reason. Enabled is checked by the caller. */
static const char *dcam_chain_gate(int carswitch_on, int resolved, int session_ok, int target_live, int manager_ok, int prologue_ok)
{
    if (!carswitch_on) return "car switch is off ([Director] CarSwitch=0); nothing was written";
    if (!resolved) return "camera engine is not resolved; nothing was written";
    if (!session_ok) return "no live race session in the capture; nothing was written";
    if (!target_live) return "driver is not a live car in the roster; nothing was written";
    if (!manager_ok) return "camera manager pointer not readable; nothing was written";
    if (!prologue_ok) return "GameStateDispatchChain bytes at 0x40D240 are not the reviewed bytes; nothing was written";
    return NULL;
}

#define DCAM_WATCH_REACHED  0
#define DCAM_WATCH_CONTINUE 1
#define DCAM_WATCH_GIVE_UP  2
/* One watch decision per game frame: reached wins; otherwise continue until max_steps frames have been seen. */
static int dcam_watch_decide(int reached, int steps, int max_steps)
{
    if (reached) return DCAM_WATCH_REACHED;
    if (steps >= max_steps) return DCAM_WATCH_GIVE_UP;
    return DCAM_WATCH_CONTINUE;
}

/* Give-up reason (pure). Shows the viewed index, the followed car and the diagnostics the owner asked for. */
static void dcam_watch_reason(char *buf, size_t cap, int steps, uint32_t cur, uint32_t tgt, int32_t mgr, uint32_t flag)
{
    snprintf(buf, cap, "the game did not move the view to that car (after %d frames; 0x7C54DC=%lu, 0x7C54E0=0x%08lX, manager state %ld, 0x7CFCC8=%lu); nothing else was written",
             steps, (unsigned long)cur, (unsigned long)tgt, (long)mgr, (unsigned long)flag);
}

/* Ordered steps. CAR runs for CUT and RESTORE only, and is skipped when that car is already on screen. */
static int dcam_plan_steps(int kind, int target_viewed, int *steps)
{
    int n = 0;
    if ((kind == DCAM_CMD_CUT || kind == DCAM_CMD_RESTORE) && !target_viewed) steps[n++] = DCAM_STEP_CAR;
    steps[n++] = DCAM_STEP_ANGLE;
    return n;
}

/* Final status: applied when the angle read back as requested; partial when only the car step ran before a failed angle. */
static const char *dcam_combine_status(int car_done, const char *angle_status)
{
    if (strcmp(angle_status, "applied") == 0) return "applied";
    if (car_done) return "partial";
    return angle_status;
}

/* Next-frame verify: mode (and index for ONBOARD) must equal the write; the followed car must equal it when check_car. */
static int dcam_verify_match(int check_car, uint32_t want_mode, uint32_t want_index, uint32_t want_veh,
                             uint32_t mode, uint32_t index, int followed, uint32_t followed_ptr)
{
    if (want_mode != DCAM_MODE_CIRCUIT && mode != want_mode) return 0;   /* circuit: no director mode; the car is checked */
    if (want_mode == DCAM_MODE_ONBOARD && index != want_index) return 0;
    if (check_car && (!followed || followed_ptr != want_veh)) return 0;
    return 1;
}

/* JSON literal for verified_next_frame: "pending" | true | false | null (no write, nothing to verify). */
static const char *dcam_verify_token(int state)
{
    if (state == DCAM_VERIFY_PENDING) return "\"pending\"";
    if (state == DCAM_VERIFY_TRUE) return "true";
    if (state == DCAM_VERIFY_FALSE) return "false";
    return "null";
}

/* HTTP thread. Quoted JSON string of a small body value. No escapes, printable ASCII only. */
static int dcam_json_string(const char *body, const char *key, char *out, size_t cap)
{
    char pat[48];
    const char *p, *q;
    size_t n, i;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(body, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return 0;
    p++;
    q = strchr(p, '"');
    if (!q) return 0;
    n = (size_t)(q - p);
    if (n == 0 || n >= cap) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (c < 32 || c > 126 || c == '\\') return 0;
    }
    memcpy(out, p, n);
    out[n] = 0;
    return 1;
}

/* HTTP thread (names refreshed first). Angle key -> mode and index. Fixed keys 0-3; ONBOARD:<name> -> mode 4 and the
 * position of <name> in the names list (LocalCam order, camera_editor.c:349-354). The name is sent, not the index. */
static int dcam_angle_key(const char *key, int *mode, uint32_t *index)
{
    int i;
    for (i = 0; i < DCAM_FIXED_COUNT; ++i) {
        if (!strcmp(key, DCAM_FIXED_NAMES[i])) { *mode = i; *index = 0; return 1; }
    }
    if (!strcmp(key, "CIRCUIT")) { *mode = (int)DCAM_MODE_CIRCUIT; *index = 0; return 1; }
    if (strncmp(key, "ONBOARD:", 8) != 0) return 0;
    for (i = 0; i < g_dcam_onboard_n; ++i) {
        if (!strcmp(key + 8, g_dcam_onboard[i])) { *mode = DCAM_MODE_ONBOARD; *index = (uint32_t)i; return 1; }
    }
    return 0;
}

/* HTTP thread. One pending slot. Returns 1 queued, 0 when another command is still pending. */
static int dcam_cmd_submit(const DcamCmd *cmd, uint32_t *seq_out)
{
    int ok = 0;
    EnterCriticalSection(&g_telemetry_cs);
    if (!g_dcam_cmd_pending) {
        g_dcam_cmd = *cmd;
        g_dcam_cmd_seq += 1u;
        if (!g_dcam_cmd_seq) g_dcam_cmd_seq = 1u;
        g_dcam_cmd.seq = g_dcam_cmd_seq;
        g_dcam_cmd_pending = 1;
        memset(&g_dcam_result, 0, sizeof g_dcam_result);
        g_dcam_result.seq = g_dcam_cmd_seq;
        *seq_out = g_dcam_cmd_seq;
        ok = 1;
    }
    LeaveCriticalSection(&g_telemetry_cs);
    return ok;
}

/* HTTP thread. Drops a command the game thread has not taken yet (timeout or broker post failure). */
static void dcam_cmd_abandon(uint32_t seq, const char *reason)
{
    EnterCriticalSection(&g_telemetry_cs);
    if (g_dcam_cmd_pending && g_dcam_cmd.seq == seq) {
        g_dcam_cmd_pending = 0;
        memset(&g_dcam_result, 0, sizeof g_dcam_result);
        g_dcam_result.seq = seq;
        g_dcam_result.done = 1;
        dcam_res(&g_dcam_result, "refused", reason);
    }
    LeaveCriticalSection(&g_telemetry_cs);
}

/* HTTP thread. Waits up to DCAM_CMD_WAIT_MS for the result. */
static int dcam_cmd_wait(uint32_t seq, DcamResult *out)
{
    DWORD t0 = GetTickCount();
    for (;;) {
        int pending;
        EnterCriticalSection(&g_telemetry_cs);
        if (g_dcam_result.seq == seq && g_dcam_result.done) {
            *out = g_dcam_result;
            LeaveCriticalSection(&g_telemetry_cs);
            return DCAM_WAIT_DONE;
        }
        pending = g_dcam_cmd_pending && g_dcam_cmd.seq == seq;
        LeaveCriticalSection(&g_telemetry_cs);
        if ((GetTickCount() - t0) >= DCAM_CMD_WAIT_MS) {
            if (pending) {
                dcam_cmd_abandon(seq, "the game thread did not take the command within 1 s; nothing was written");
                return DCAM_WAIT_ABANDONED;
            }
            return DCAM_WAIT_RUNNING;
        }
        Sleep(10);
    }
}

/* Game thread only. Live driver name -> vehicle pointer and roster id (+0x4894). Reads only. */
static int dcam_find_driver(const char *name, uint32_t *veh_out, uint32_t *roster_out)
{
    int i;
    for (i = 0; i < VEH_SLOTS; ++i) {
        uint32_t p = VEH_BASE + (uint32_t)i * VEH_STRIDE, act = 0, rid = 0;
        char nm[NAME_LEN];
        if (!copy_game(&act, p + DCAM_VEH_ACTIVE, 4) || act == 0xFFFFFFFFu) continue;
        if (!copy_game(nm, p + OFF_DRIVER_NAME, NAME_LEN)) continue;
        dcam_clean_name(nm, NAME_LEN);
        if (strcmp(nm, name) != 0) continue;
        if (!copy_game(&rid, p + DCAM_ROSTER_ID, 4)) continue;
        *veh_out = p;
        *roster_out = rid;
        return 1;
    }
    return 0;
}

/* T4 (owner 2026-10-14; RESEARCH_T4_HUD_2026-10-14.md, findings t4-hud-*): key 2 = sub_4D8450 toggles dword_7CF788 ("HUD On" when 1).
 * Cinematic cameras (SWINGMAN, ONBOARD, circuit) hide it; NOSECAM, COCKPIT and TVCOCKPIT (player views) give back the state the
 * player had. The flag has one writer (the key handler) and is not rewritten per frame. Guards: prologue bytes of the writer and of
 * the reader 0x4D8880, and a value in {0,1}. Game thread only. */
#define DCAM_HUD_FLAG_ADDR   0x007CF788u
#define DCAM_HUD_KEY_ADDR    0x004D8450u
#define DCAM_HUD_READ_ADDR   0x004D8880u
static const uint8_t DCAM_HUD_KEY_PROLOGUE[DCAM_PROLOGUE16] = {
    0x81, 0xEC, 0x00, 0x01, 0x00, 0x00, 0x53, 0x56, 0x8B, 0xF1, 0xE8, 0x71, 0xFE, 0xFF, 0xFF, 0x8B };
static const uint8_t DCAM_HUD_READ_PROLOGUE[DCAM_PROLOGUE16] = {
    0x8B, 0x44, 0x24, 0x04, 0x83, 0xF8, 0xFF, 0x75, 0x06, 0x89, 0x41, 0x08, 0xC2, 0x04, 0x00, 0x8B };
static int32_t g_dcam_hud_saved = -1;   /* the player's HUD state before our first cinematic camera; -1 = not hidden by us */

static int dcam_hud_code_ok(void)
{
    uint8_t b1[DCAM_PROLOGUE16], b2[DCAM_PROLOGUE16];
    return copy_game(b1, DCAM_HUD_KEY_ADDR, sizeof b1) && !memcmp(b1, DCAM_HUD_KEY_PROLOGUE, sizeof b1) &&
           copy_game(b2, DCAM_HUD_READ_ADDR, sizeof b2) && !memcmp(b2, DCAM_HUD_READ_PROLOGUE, sizeof b2);
}

static void dcam_hud_for_mode(uint32_t mode)
{
    int32_t cur = -1;
    int cinematic = mode == 3u || mode == DCAM_MODE_ONBOARD || mode == DCAM_MODE_CIRCUIT;
    if (!dcam_hud_code_ok() || !copy_game(&cur, DCAM_HUD_FLAG_ADDR, 4) || (cur != 0 && cur != 1)) return;
    if (!g_dcam_hide_hud) {   /* switched off on the page: give the player's HUD back once, then never touch it */
        if (g_dcam_hud_saved >= 0) { *(volatile int32_t *)(uintptr_t)DCAM_HUD_FLAG_ADDR = g_dcam_hud_saved; g_dcam_hud_saved = -1; }
        return;
    }
    if (cinematic) {
        if (g_dcam_hud_saved < 0) g_dcam_hud_saved = cur;
        if (cur != 0) *(volatile int32_t *)(uintptr_t)DCAM_HUD_FLAG_ADDR = 0;
    } else if (g_dcam_hud_saved >= 0) {
        *(volatile int32_t *)(uintptr_t)DCAM_HUD_FLAG_ADDR = g_dcam_hud_saved;
        g_dcam_hud_saved = -1;
    }
}

/* Game thread (broker task, posted by POST /director/options when the page switches HideHud off): give the HUD back now. */
static void __cdecl dcam_hud_option_task(void *context)
{
    (void)context;
    telemetry_native_budget();
    if (!g_dcam_hide_hud) dcam_hud_for_mode(0u);
}

/* Game thread only. T1: the PGDN sequence on the camera manager. Prologue bytes of both functions are checked first; the state
 * before (manager+0x0C) is kept so a later director angle or Restore can leave the circuit camera the same way. */
static void dcam_set_circuit(const DcamAddrs *a, DcamResult *r)
{
    uint8_t b1[DCAM_PROLOGUE16], b2[DCAM_PROLOGUE16];
    int32_t before = -1, after = -1;
    DcamLockFn lock = (DcamLockFn)(uintptr_t)DCAM_LOCKCHECK_ADDR;
    DcamStateFn state = (DcamStateFn)(uintptr_t)DCAM_STATEMACH_ADDR;
    if (!copy_game(b1, DCAM_LOCKCHECK_ADDR, sizeof b1) || memcmp(b1, DCAM_LOCKCHECK_PROLOGUE, sizeof b1) ||
        !copy_game(b2, DCAM_STATEMACH_ADDR, sizeof b2) || memcmp(b2, DCAM_STATEMACH_PROLOGUE, sizeof b2)) {
        dcam_res(r, "refused", "circuit camera code bytes differ from the reviewed game build; nothing was written");
        return;
    }
    if (!dcam_plausible(a->manager) || !copy_game(&before, (uintptr_t)a->manager + DCAM_MGR_STATE_OFF, 4)) {
        dcam_res(r, "refused", "camera manager not readable; nothing was written");
        return;
    }
    if (before != (int32_t)DCAM_MODE_CIRCUIT) g_dcam_prev_mgr_state = before;
    if (!lock((void *)(uintptr_t)a->manager, (int)DCAM_MODE_CIRCUIT)) {
        dcam_res(r, "refused", "the game refused the circuit camera now (camera lock, as for PGDN); nothing was changed");
        return;
    }
    (void)state((void *)(uintptr_t)a->manager, (int)DCAM_MODE_CIRCUIT, 1);
    r->readback_ok = copy_game(&after, (uintptr_t)a->manager + DCAM_MGR_STATE_OFF, 4);
    r->mode = DCAM_MODE_CIRCUIT;
    r->index = 0;
    if (r->readback_ok && after == (int32_t)DCAM_MODE_CIRCUIT) dcam_res(r, "applied", "circuit camera on (camera state 5, as PGDN)");
    else dcam_res(r, "redirected", "circuit camera not taken by the game (no circuit cameras on this track?)");
}

/* Game thread only. Before a director angle: when our circuit camera is on screen (manager state 5), go back to the state the game
 * had before it, through the same state machine the keys use. Nothing happens when the view is not ours or not circuit. */
static void dcam_leave_circuit(const DcamAddrs *a)
{
    int32_t cur = -1;
    DcamStateFn state = (DcamStateFn)(uintptr_t)DCAM_STATEMACH_ADDR;
    uint8_t b2[DCAM_PROLOGUE16];
    if (g_dcam_prev_mgr_state < 0 || !dcam_plausible(a->manager)) return;
    if (!copy_game(&cur, (uintptr_t)a->manager + DCAM_MGR_STATE_OFF, 4) || cur != (int32_t)DCAM_MODE_CIRCUIT) return;
    if (!copy_game(b2, DCAM_STATEMACH_ADDR, sizeof b2) || memcmp(b2, DCAM_STATEMACH_PROLOGUE, sizeof b2)) return;
    (void)state((void *)(uintptr_t)a->manager, (int)g_dcam_prev_mgr_state, 1);
    g_dcam_prev_mgr_state = -1;
}

/* Game thread only. Angle write (same sequence as camera_editor.c redirect_disabled and onboard_mirrors): fixed modes
 * via the game's dispatch; ONBOARD via dispatch to mode 4, then the index, the mirror-ready byte cleared, mirror setup.
 * Read-back is taken right after the write (spec 4.5 asks for the next frame; test T4 records which one was used). */
static void dcam_set_angle(uintptr_t director, const DcamAddrs *a, uint32_t want_mode, uint32_t want_index, DcamResult *r)
{
    DcamDispatchFn dispatch = (DcamDispatchFn)(uintptr_t)a->view_dispatch;
    DcamMirrorFn mirror = (DcamMirrorFn)(uintptr_t)a->mirror_setup;
    uint32_t blk[3] = { 0, 0, 0 }, count = 0;
    char msg[DCAM_RESULT_REASON];
    if (want_mode > DCAM_MODE_CIRCUIT || !dispatch || !mirror) {
        dcam_res(r, "refused", "bad angle or camera addresses; nothing was written");
        return;
    }
    if (want_mode == DCAM_MODE_CIRCUIT) {
        dcam_set_circuit(a, r);
        if (strcmp(r->status, "applied") == 0) dcam_hud_for_mode(want_mode);
        return;
    }
    dcam_leave_circuit(a);
    dcam_hud_for_mode(want_mode);
    if (want_mode == DCAM_MODE_ONBOARD) {
        if (!copy_game(&count, director + DCAM_COUNT, 4) || want_index >= count) {
            snprintf(msg, sizeof msg, "ONBOARD index %lu is out of range for count %lu; nothing was written",
                     (unsigned long)want_index, (unsigned long)count);
            dcam_res(r, "refused", msg);
            return;
        }
        dispatch((void *)director, (int)DCAM_MODE_ONBOARD, 0, 1);
        *(volatile uint32_t *)(director + DCAM_INDEX) = want_index;
        *(volatile uint8_t *)(a->manager + DCAM_MIRROR_READY) = 0;
        mirror((void *)director, 0);
    } else {
        dispatch((void *)director, (int)want_mode, 0, 1);
    }
    if (!copy_game(blk, director + DCAM_MODE, sizeof blk)) {
        dcam_res(r, "refused", "read-back failed after the write");
        return;
    }
    r->readback_ok = 1;
    r->mode = blk[0];
    r->index = blk[2];
    if (blk[0] == want_mode && (want_mode != DCAM_MODE_ONBOARD || blk[2] == want_index)) {
        dcam_res(r, "applied", "mode and index read back as requested");
    } else {
        snprintf(msg, sizeof msg, "read back mode %lu index %lu, not the request (game remap or Camera Editor redirect)",
                 (unsigned long)blk[0], (unsigned long)blk[2]);
        dcam_res(r, "redirected", msg);
    }
}

/* Game thread only. The one call of the game's own camera-target function. Only dcam_chain_begin calls it, after the gates. */
/* Commentary gate (findings h4-ridingwith-*): inside the chain, AIOutOfTrackEvent 0x41A0C0 queues the "camera switched to
 * driver" line (eid 264) only when dword_7D0308 != 0 and flt_7CF3C8 != 0. Owner E1: the TV Director change must not trigger it.
 * The gate is cleared for our ONE call only and restored right after, on the same game-thread task; the player's own camera
 * changes are untouched. */
#define DCAM_COMMENTARY_GATE_ADDR 0x007D0308u

static void dcam_chain_call_once(uint32_t manager, uint32_t roster)
{
    DcamChainFn chain = (DcamChainFn)(uintptr_t)DCAM_CHAIN_ADDR;
    volatile int32_t *gate = (volatile int32_t *)(uintptr_t)DCAM_COMMENTARY_GATE_ADDR;
    int32_t saved = 0;
    int muted = g_dcam_mute_commentary && copy_game(&saved, DCAM_COMMENTARY_GATE_ADDR, 4);
    if (muted) *gate = 0;
    (void)chain((void *)(uintptr_t)manager, (int)roster);
    if (muted) *gate = saved;
}

/* Game thread only. Standings snapshot (reads only): per pool slot the race position (+0x4060) and the active flag (+0x32C8). */
static void dcam_standings_read(int32_t *pos, int32_t *act)
{
    int i;
    for (i = 0; i < VEH_SLOTS; ++i) {
        uintptr_t p = (uintptr_t)VEH_BASE + (uintptr_t)i * (uintptr_t)VEH_STRIDE;
        if (!copy_game(&act[i], p + DCAM_VEH_ACTIVE, 4)) act[i] = -1;
        if (!copy_game(&pos[i], p + OFF_POSITION, 4)) pos[i] = 0;
    }
}

/* Game thread only. The player car and its roster, read the way numpad Enter reads them (0x40D340). Reads only. */
static int dcam_player_roster(uint32_t *veh, uint32_t *roster)
{
    uint32_t slot = 0, entry = 0, rid = 0;
    if (!copy_game(&slot, DCAM_PLAYER_SLOT_ADDR, 4)) return 0;
    if (slot >= VEH_SLOTS || !copy_game(&entry, (uintptr_t)DCAM_PTR_TABLE_ADDR + 4u * (uintptr_t)slot, 4)) return 0;
    if (!dcam_player_pick(slot, entry, veh)) return 0;
    if (!copy_game(&rid, (uintptr_t)*veh + DCAM_ROSTER_ID, 4)) return 0;
    *roster = rid;
    return 1;
}

/* Game thread only. Starts one camera change: byte check of 0x40D240, gates, ONE call of GameStateDispatchChain with the roster of
 * the car veh (ECX = the camera manager), then the watch: dcam_watch_step verifies 0x7C54E0 on the following frames and applies the
 * angle. Returns 1 when the watch started, 0 when refused (r holds the reason; nothing was called). */
static int dcam_chain_begin(const DcamAddrs *a, uint32_t veh, uint32_t roster, uint32_t want_mode, uint32_t want_index,
                            DcamResult *r, int session_ok, LONG st)
{
    uint8_t bytes[DCAM_CHAIN_LEN];
    int prologue_ok, target_ok;
    const char *gate;
    uint32_t slot = 0;
    if (g_dcam_watch.active) { dcam_res(r, "refused", "another camera change is still running; nothing was written"); return 0; }
    prologue_ok = copy_game(bytes, (uintptr_t)DCAM_CHAIN_ADDR, sizeof bytes) && dcam_chain_prologue_ok(bytes);
    target_ok = dcam_in_pool(veh);   /* roster id equals the pool slot, so 0 is the player (research A13): no roster > 0 test */
    gate = dcam_chain_gate(g_dcam_carswitch_enabled, st == DCAM_RESOLVED, session_ok, target_ok,
                           dcam_plausible(a->manager), prologue_ok);
    if (gate) { dcam_res(r, "refused", gate); return 0; }
    dcam_slot_of(veh, &slot);
    log_f("director: camera change to roster %lu (vehicle 0x%08lX) by GameStateDispatchChain on game thread", (unsigned long)roster,
          (unsigned long)veh);
    dcam_chain_call_once(a->manager, roster);
    g_dcam_watch.veh = veh;
    g_dcam_watch.slot = slot;
    g_dcam_watch.steps = 0;
    g_dcam_watch.want_mode = want_mode;
    g_dcam_watch.want_index = want_index;
    g_dcam_watch.r = *r;
    EnterCriticalSection(&g_telemetry_cs);
    g_dcam_watch.posted = 0;
    g_dcam_watch.sim_seen = 0;
    g_dcam_watch.t0 = GetTickCount();
    g_dcam_watch.active = 1;
    LeaveCriticalSection(&g_telemetry_cs);
    return 1;
}

/* Game thread only. Arms the next-frame verify for a command that wrote (applied or partial). */
static void dcam_arm(DcamResult *r, int check_car, uint32_t mode, uint32_t index, uint32_t veh)
{
    if (strcmp(r->status, "applied") != 0 && strcmp(r->status, "partial") != 0) return;
    r->verify_state = DCAM_VERIFY_PENDING;
    r->exp_check_car = check_car;
    r->exp_mode = mode;
    r->exp_index = index;
    r->exp_veh = veh;
}

/* Game thread only. Combines the car step with the angle step (dcam_combine_status). */
static void dcam_finish_car(DcamResult *r, int car_done)
{
    char msg[DCAM_RESULT_REASON * 2];
    if (!car_done) return;
    if (strcmp(r->status, "applied") == 0) {
        dcam_res(r, "applied", "car switched and angle read back as requested");
        return;
    }
    snprintf(msg, sizeof msg, "car switched; angle not applied: %s", r->reason);
    dcam_res(r, "partial", msg);
}

/* Game thread only. Publishes the finished result under g_telemetry_cs (done = 1) and ends the watch. */
static void dcam_watch_publish(DcamResult *r)
{
    r->done = 1;
    EnterCriticalSection(&g_telemetry_cs);
    g_dcam_watch.active = 0;
    g_dcam_result = *r;
    LeaveCriticalSection(&g_telemetry_cs);
    log_f("director: camera change seq %lu finished on game thread %lu: %s (%s)", (unsigned long)r->seq,
          (unsigned long)r->thread_id, r->status, r->reason);
}

/* Game thread only, once per game frame (broker task queued by dcam_watch_post_due). Verifies the followed car (0x7C54E0). When it is
 * the target: applies the angle and publishes applied / partial. After DCAM_WATCH_MAX_STEPS frames, or the wall-clock guard: refused. */
static void dcam_watch_step(void)
{
    DcamAddrs a;
    DcamResult r;
    uint32_t followed = 0, cur = 0, director = 0, cflag = 0;
    int32_t mgr = 0;
    int decision;
    char msg[DCAM_RESULT_REASON];
    if (!g_dcam_watch.active) return;
    EnterCriticalSection(&g_telemetry_cs);
    a = g_dcam_addr;
    LeaveCriticalSection(&g_telemetry_cs);
    g_dcam_watch.steps += 1;
    if (!copy_game(&followed, a.target_ptr, 4)) followed = 0;
    if (!copy_game(&cur, DCAM_VIEW_INDEX_ADDR, 4)) cur = 0;
    log_f("director: camera change frame %d: 0x7C54DC=%lu 0x7C54E0=0x%08lX", g_dcam_watch.steps,
          (unsigned long)cur, (unsigned long)followed);
    decision = dcam_watch_decide(followed == g_dcam_watch.veh, g_dcam_watch.steps, DCAM_WATCH_MAX_STEPS);
    if (decision == DCAM_WATCH_CONTINUE && (GetTickCount() - g_dcam_watch.t0) < DCAM_WATCH_MAX_MS) return;
    r = g_dcam_watch.r;
    if (decision == DCAM_WATCH_REACHED) {
        r.car_step = 1;
        r.viewed_slot_ok = 1;
        r.viewed_slot = g_dcam_watch.slot;
        if (!copy_game(&director, a.director_ptr, 4) || !dcam_plausible(director)) {
            dcam_res(&r, "refused", "director pointer not readable after the camera change; the angle was not written");
        } else {
            dcam_set_angle((uintptr_t)director, &a, g_dcam_watch.want_mode, g_dcam_watch.want_index, &r);
        }
        dcam_finish_car(&r, 1);
        dcam_arm(&r, 1, g_dcam_watch.want_mode, g_dcam_watch.want_index, g_dcam_watch.veh);
        dcam_watch_publish(&r);
        return;
    }
    if (decision == DCAM_WATCH_CONTINUE) decision = DCAM_WATCH_GIVE_UP;   /* wall-clock guard */
    if (!copy_game(&mgr, (uintptr_t)a.manager + DCAM_MGR_STATE_OFF, 4)) mgr = 0;
    if (!copy_game(&cflag, DCAM_CIRCUIT_FLAG_ADDR, 4)) cflag = 0;
    dcam_watch_reason(msg, sizeof msg, g_dcam_watch.steps, cur, followed, mgr, cflag);
    dcam_res(&r, "refused", msg);
    dcam_watch_publish(&r);
}

/* Game thread (broker task). One watch step per game frame. Clears the queued flag and records the sim time the step ran at. */
static void __cdecl dcam_watch_task(void *context)
{
    double t = 0.0;
    (void)context;
    telemetry_native_budget();
    EnterCriticalSection(&g_telemetry_cs);
    g_dcam_watch.posted = 0;
    LeaveCriticalSection(&g_telemetry_cs);
    if (copy_game(&t, GAME_ADDR_SESSION_TIME, sizeof t) && t == t) {
        EnterCriticalSection(&g_telemetry_cs);
        g_dcam_watch.last_sim = t;
        g_dcam_watch.sim_seen = 1;
        LeaveCriticalSection(&g_telemetry_cs);
    }
    dcam_watch_step();
}

/* Worker thread. Called once per worker loop pass. While a camera change runs it queues one dcam_watch_task per game frame: the game
 * session time must have advanced since the last step, and at most one task is outstanding. After the wall-clock guard it queues one
 * more step so that the step can give up. Returns 1 while a change runs (the worker then sleeps 2 ms instead of 50 ms). Game memory is
 * read with the guarded copy only. Never posts from inside a broker task (research D1). */
static int dcam_watch_post_due(void)
{
    double t = 0.0;
    int busy, due, overdue, readable;
    EnterCriticalSection(&g_telemetry_cs);
    busy = g_dcam_watch.active;
    LeaveCriticalSection(&g_telemetry_cs);
    if (!busy) return 0;
    readable = copy_game(&t, GAME_ADDR_SESSION_TIME, sizeof t) ? 1 : 0;
    if (readable && t != t) readable = 0;
    EnterCriticalSection(&g_telemetry_cs);
    overdue = (GetTickCount() - g_dcam_watch.t0) >= DCAM_WATCH_MAX_MS;
    due = g_dcam_watch.active && !g_dcam_watch.posted &&
          (overdue || (readable && !(g_dcam_watch.sim_seen && t == g_dcam_watch.last_sim)));
    if (due) g_dcam_watch.posted = 1;
    LeaveCriticalSection(&g_telemetry_cs);
    if (due && (!telemetry_broker.post || telemetry_broker.post(telemetry_token, dcam_watch_task, NULL) != F1C_BROKER_OK)) {
        EnterCriticalSection(&g_telemetry_cs);
        g_dcam_watch.posted = 0;
        LeaveCriticalSection(&g_telemetry_cs);
    }
    return 1;
}

/* Owner 2026-10-14: COCKPIT (fixed mode 1) is the player's simulated cockpit; AI cars have none. Game thread only. */
#define DCAM_MODE_COCKPIT 1u
static int dcam_is_player(uint32_t veh)
{
    uint32_t pv = 0, pr = 0;
    return veh && dcam_player_roster(&pv, &pr) && pv == veh;
}

/* Game thread only. Runs one command. Order: Enabled, resolved, director. ANGLE: angle only. The car: RESTORE takes the player car
 * (dcam_player_roster), CUT takes the named driver or the car at {"position": N} (dcam_standings_read, dcam_pick_position). Car already
 * on screen: angle only. RESTORE without CarSwitch: angle only (partial). Otherwise dcam_chain_begin. Returns 1 when a camera change
 * started (the caller must not publish), 0 otherwise. */
static int dcam_exec(const DcamCmd *cmd, const DcamAddrs *a, LONG st, int saved_ok, uint32_t saved_mode,
                     uint32_t saved_index, int session_ok, DcamResult *r)
{
    uint32_t d = 0, veh = 0, roster = 0, followed = 0, want_mode = 0, want_index = 0, slot = 0;
    int32_t pos[VEH_SLOTS], act[VEH_SLOTS];
    uintptr_t director;
    int found = 0, restore = (cmd->kind == DCAM_CMD_RESTORE);
    const char *why = NULL;
    if (!g_dcam_enabled) { dcam_res(r, "refused", "TV Director writes are off ([Director] Enabled=0); nothing was written"); return 0; }
    if (st != DCAM_RESOLVED) { dcam_res(r, "refused", "camera engine is not resolved; nothing was written"); return 0; }
    if (!copy_game(&d, a->director_ptr, 4) || !dcam_plausible(d)) {
        dcam_res(r, "refused", "director pointer not readable; nothing was written");
        return 0;
    }
    director = d;
    if (cmd->kind == DCAM_CMD_ANGLE) {
        if (!copy_game(&followed, a->target_ptr, 4)) followed = 0;
        if ((uint32_t)cmd->mode == DCAM_MODE_COCKPIT && !dcam_is_player(followed)) {
            dcam_res(r, "refused", "COCKPIT exists only for your own car (AI cars have no simulated cockpit view); nothing was written");
            return 0;
        }
        dcam_set_angle(director, a, (uint32_t)cmd->mode, cmd->index, r);
        dcam_arm(r, 0, (uint32_t)cmd->mode, cmd->index, 0);
        return 0;
    }
    if (restore) {
        /* RESTORE: saved mode (first game frame of the session, spec R4) and the player car (numpad Enter's read). */
        if (!saved_ok) { dcam_res(r, "refused", "no saved mode yet (first game frame not seen); nothing was written"); return 0; }
        want_mode = saved_mode;
        want_index = saved_index;
        found = dcam_player_roster(&veh, &roster);
    } else {
        want_mode = (uint32_t)cmd->mode;
        want_index = cmd->index;
        if (cmd->position > 0) {
            dcam_standings_read(pos, act);
            if (!dcam_pick_position(pos, act, VEH_SLOTS, cmd->position, &slot, &why)) {
                dcam_res(r, "refused", why ? why : "position not resolved; nothing was written");
                return 0;
            }
            veh = (uint32_t)VEH_BASE + slot * (uint32_t)VEH_STRIDE;
            found = copy_game(&roster, (uintptr_t)veh + DCAM_ROSTER_ID, 4);
        } else {
            found = dcam_find_driver(cmd->driver, &veh, &roster);
        }
    }
    if (!found) {
        dcam_res(r, "refused", restore ? "player car not readable; nothing was written"
                                       : "driver is not a live car in the roster; nothing was written");
        return 0;
    }
    if (!restore && want_mode == DCAM_MODE_COCKPIT && !dcam_is_player(veh)) {
        dcam_res(r, "refused", "COCKPIT exists only for your own car (AI cars have no simulated cockpit view); nothing was written");
        return 0;
    }
    if (!copy_game(&followed, a->target_ptr, 4)) followed = 0;
    if (followed != veh) {
        if (restore && !g_dcam_carswitch_enabled) {
            dcam_set_angle(director, a, want_mode, want_index, r);
            if (strcmp(r->status, "applied") == 0)
                dcam_res(r, "partial", "saved angle restored on the followed car; car restore off ([Director] CarSwitch=0)");
            dcam_arm(r, 0, want_mode, want_index, 0);
            return 0;
        }
        return dcam_chain_begin(a, veh, roster, want_mode, want_index, r, session_ok, st);
    }
    /* car already on screen: angle only */
    dcam_set_angle(director, a, want_mode, want_index, r);
    dcam_arm(r, 1, want_mode, want_index, veh);
    return 0;
}

/* Game thread (broker task). Takes the pending command under g_telemetry_cs, runs it, publishes the result. */
static void __cdecl dcam_cmd_task(void *context)
{
    DcamCmd cmd;
    DcamAddrs a;
    DcamResult r;
    LONG st;
    int taken, saved_ok, session_ok;
    uint32_t saved_mode, saved_index;
    (void)context;
    memset(&cmd, 0, sizeof cmd);
    EnterCriticalSection(&g_telemetry_cs);
    taken = g_dcam_cmd_pending;
    if (taken) {
        cmd = g_dcam_cmd;
        g_dcam_cmd_pending = 0;
    }
    a = g_dcam_addr;
    st = InterlockedCompareExchange(&g_dcam_state, 0, 0);
    saved_ok = g_dcam_saved_ok;
    saved_mode = g_dcam_saved_mode;
    saved_index = g_dcam_saved_index;
    session_ok = g_dcam_snap.session_live;
    LeaveCriticalSection(&g_telemetry_cs);
    if (!taken) return;
    telemetry_native_budget();   /* 1.2.2: the car change and angle calls run native game code */
    memset(&r, 0, sizeof r);
    r.seq = cmd.seq;
    r.thread_id = GetCurrentThreadId();
    if (dcam_exec(&cmd, &a, st, saved_ok, saved_mode, saved_index, session_ok, &r)) {
        /* car change started: dcam_watch_step publishes the result over the next capture samples */
        log_f("director: command %d seq %lu started a car change on game thread %lu", cmd.kind,
              (unsigned long)cmd.seq, (unsigned long)r.thread_id);
        return;
    }
    r.done = 1;
    EnterCriticalSection(&g_telemetry_cs);
    g_dcam_result = r;
    LeaveCriticalSection(&g_telemetry_cs);
    log_f("director: command %d seq %lu on game thread %lu: %s (%s)", cmd.kind, (unsigned long)cmd.seq,
          (unsigned long)r.thread_id, r.status, r.reason);
}

/* HTTP thread. Command result object for the POST response and for GET /director/state (last_result).
 * Phase 2: readback carries viewed_slot; verified_next_frame is "pending" | true | false | null; next_frame is the sample
 * taken at the verifying capture. */
static int dcam_cmd_json(char *buf, size_t cap, const char *kind, const DcamResult *r)
{
    char esc_r[DCAM_RESULT_REASON * 2 + 8], rb[160], nf[120], vs[24];
    int n;
    esc_json(esc_r, sizeof esc_r, r->reason);
    if (r->readback_ok) {
        if (r->viewed_slot_ok) snprintf(vs, sizeof vs, "%lu", (unsigned long)r->viewed_slot);
        else snprintf(vs, sizeof vs, "null");
        snprintf(rb, sizeof rb, "{\"mode\":%lu,\"index\":%lu,\"viewed_slot\":%s}",
                 (unsigned long)r->mode, (unsigned long)r->index, vs);
    } else {
        snprintf(rb, sizeof rb, "null");
    }
    if (r->nf_ok) snprintf(nf, sizeof nf, "{\"mode\":%lu,\"index\":%lu,\"viewed\":\"0x%08lX\"}",
                           (unsigned long)r->nf_mode, (unsigned long)r->nf_index, (unsigned long)r->nf_veh);
    else snprintf(nf, sizeof nf, "null");
    n = _snprintf(buf, cap, "{\"api\":\"director\",\"command\":\"%s\",\"seq\":%lu,\"status\":\"%s\",\"reason\":\"%s\","
                  "\"thread_id\":%lu,\"readback\":%s,\"verified_next_frame\":%s,\"next_frame\":%s}", kind,
                  (unsigned long)r->seq, r->status, esc_r, (unsigned long)r->thread_id, rb,
                  dcam_verify_token(r->verify_state), nf);
    if (cap) buf[cap - 1] = 0;
    return n;
}

/* Game thread, inside capture_on_game_thread, under g_telemetry_cs, after the mirrors were copied.
 * sampled = the mirror copy was valid and a session is active. Reads only; never writes game memory. */
static void dcam_capture_locked(int sampled)
{
    DcamSnap *s = &g_dcam_snap;
    uint32_t director = 0, blk[3] = { 0, 0, 0 }, ptr = 0, veh[3] = { 0, 0, 0 };
    LONG st = InterlockedCompareExchange(&g_dcam_state, 0, 0);
    memset(s, 0, sizeof *s);
    /* sim tick (session time, raw mirror) is reported whenever the sample is good, even before resolution */
    s->sim_time = finite_double(*(const double *)ADDR_SESSION_TIME);
    s->table_index = PLAYER_INDEX;
    s->session_live = sampled;
    if (!sampled || st != DCAM_RESOLVED) return;
    if (!copy_game(&director, g_dcam_addr.director_ptr, 4) || !dcam_plausible(director)) return;
    if (!copy_game(blk, (uintptr_t)director + DCAM_MODE, sizeof blk)) return;
    s->director = director;
    s->mode = blk[0];
    s->count = blk[1];
    s->index = blk[2];
    s->valid = 1;
    {
        int32_t ms = 0;
        if (copy_game(&ms, (uintptr_t)g_dcam_addr.manager + DCAM_MGR_STATE_OFF, 4)) { s->manager_state = ms; s->manager_state_ok = 1; }
    }
    /* 2026-10-14 car change diagnostics (reads only, never gates). */
    {
        uint32_t vi = 0, vt = 0, cf = 0;
        if (copy_game(&vi, DCAM_VIEW_INDEX_ADDR, 4)) { s->view_index = vi; s->view_index_ok = 1; }
        if (copy_game(&vt, DCAM_VIEW_TARGET_ADDR, 4)) { s->view_target = vt; s->view_target_ok = 1; }
        if (copy_game(&cf, DCAM_CIRCUIT_FLAG_ADDR, 4)) { s->circuit_flag = cf; s->circuit_flag_ok = 1; }
    }
    /* View owner (2026-10-13 fix): same snapshot, reads only. */
    {
        uint32_t manager = g_dcam_addr.manager, dvt = 0, stat = 0, trk = 0, sc = 0, tc = 0;
        int have_active, sc_ok = 0, tc_ok = 0;
        s->dir_vt_ok = copy_game(&dvt, director, 4) && dvt == DCAM_VT_DIRECTOR;
        have_active = copy_game(&s->active_val, (uintptr_t)manager + DCAM_VIEW_ACTIVE_OFF, 4) && dcam_plausible(s->active_val);
        if (!have_active) s->active_val = 0;
        if (copy_game(&stat, (uintptr_t)manager + DCAM_VIEW_STATIC_OFF, 4) && dcam_plausible(stat)) s->static_val = stat;
        if (copy_game(&trk, (uintptr_t)manager + DCAM_VIEW_TRACK_OFF, 4) && dcam_plausible(trk)) s->tracking_val = trk;
        dcam_group_probe(s->static_val, DCAM_VT_STATIC, &s->static_vt_ok, &sc, &sc_ok);
        dcam_group_probe(s->tracking_val, DCAM_VT_TRACKING, &s->tracking_vt_ok, &tc, &tc_ok);
        s->owner = dcam_view_owner(have_active, s->active_val, director, s->dir_vt_ok,
                                   s->static_val, s->static_vt_ok, s->tracking_val, s->tracking_vt_ok);
        if (s->owner == DCAM_OWNER_CIRCUIT_STATIC) { s->circuit_count = sc; s->circuit_count_ok = sc_ok; }
        else if (s->owner == DCAM_OWNER_CIRCUIT_TRACKING) { s->circuit_count = tc; s->circuit_count_ok = tc_ok; }
    }
    /* Saved mode for restore (spec 9, R4): the first valid director sample of each game session. */
    if (s->mode <= DCAM_MODE_ONBOARD && (!g_dcam_saved_ok || g_dcam_saved_gen != (uint32_t)capture_generation)) {
        g_dcam_saved_ok = 1;
        g_dcam_saved_gen = (uint32_t)capture_generation;
        g_dcam_saved_mode = s->mode;
        g_dcam_saved_index = s->index;
    }
    if (copy_game(&s->player_ptr, g_dcam_addr.player_ptr, 4) && dcam_in_pool(s->player_ptr) &&
        copy_game(veh, (uintptr_t)s->player_ptr + DCAM_ROSTER_ID, sizeof veh)) {
        s->player_ok = 1;
        s->player_roster = veh[0];
    }
    if (copy_game(&ptr, g_dcam_addr.target_ptr, 4) && dcam_in_pool(ptr) &&
        copy_game(veh, (uintptr_t)ptr + DCAM_ROSTER_ID, sizeof veh)) {
        s->followed = 1;
        s->followed_ptr = ptr;
        s->followed_roster = veh[0];
        s->followed_roster_index = veh[1];
        s->followed_driver_id = veh[2];
        if (copy_game(s->driver_name, (uintptr_t)ptr + OFF_DRIVER_NAME, NAME_LEN)) dcam_clean_name(s->driver_name, NAME_LEN);
        else s->driver_name[0] = 0;
        if (copy_game(s->car_name, (uintptr_t)ptr + OFF_CAR_NAME, NAME_LEN)) dcam_clean_name(s->car_name, NAME_LEN);
        else s->car_name[0] = 0;
    }
    /* TV Director phase 2: next-frame verify. The first valid sample after a written command is compared with the mode,
     * index and followed car that were written. Read only. */
    if (g_dcam_result.verify_state == DCAM_VERIFY_PENDING) {
        DcamResult *vr = &g_dcam_result;
        vr->nf_ok = 1;
        vr->nf_mode = s->mode;
        vr->nf_index = s->index;
        vr->nf_veh = s->followed_ptr;
        vr->verify_capture_seq = capture_sequence;
        vr->verify_state = dcam_verify_match(vr->exp_check_car, vr->exp_mode, vr->exp_index, vr->exp_veh,
                                             s->mode, s->index, s->followed, s->followed_ptr) ? DCAM_VERIFY_TRUE : DCAM_VERIFY_FALSE;
    }
}

/* Worker thread. Publishes the resolved addresses or the refusal (under g_telemetry_cs). */
static void dcam_publish(LONG state, const char *reason, const DcamAddrs *a)
{
    EnterCriticalSection(&g_telemetry_cs);
    if (a) g_dcam_addr = *a;
    snprintf(g_dcam_reason, sizeof g_dcam_reason, "%s", reason);
    InterlockedExchange(&g_dcam_state, state);
    LeaveCriticalSection(&g_telemetry_cs);
    log_f("director: camera engine %s: %s", state == DCAM_RESOLVED ? "resolved" : (state == DCAM_REFUSED ? "refused" : "retrying"), reason);
}

static int dcam_pending(void)
{
    LONG st = InterlockedCompareExchange(&g_dcam_state, 0, 0);
    return (st == DCAM_NOT_TRIED || st == DCAM_RETRY) && g_dcam_attempts < DCAM_MAX_ATTEMPTS;
}

static size_t dcam_count_pushes(const uint8_t *code, size_t len)
{
    size_t probe[1];
    return dcam_find(code, len, "5356", probe, 1);   /* push ebx; push esi: camera_editor.c resolve() */
}

/* Worker thread only. Scans every executable section of the running EXE (read-only, copy_game chunks). */
static void dcam_try_resolve(void)
{
    uintptr_t image = (uintptr_t)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    uintptr_t sh;
    int nsec, s;
    size_t pushes = 0;
    const char *why = "code headers not readable";
    DcamAddrs a;
    int refuse_now = 0;
    char diag[320] = "";
    size_t sec_pushes = 0;
    g_dcam_attempts++;
    if (!copy_game(&dos, image, sizeof dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < (LONG)sizeof dos || dos.e_lfanew > 0x1000 ||
        !copy_game(&nt, image + (uintptr_t)dos.e_lfanew, sizeof nt) || nt.Signature != IMAGE_NT_SIGNATURE)
        goto unresolved;
    nsec = nt.FileHeader.NumberOfSections;
    if (nsec <= 0 || nsec > 32) { why = "section table"; goto unresolved; }
    sh = image + (uintptr_t)dos.e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + nt.FileHeader.SizeOfOptionalHeader;
    for (s = 0; s < nsec; ++s) {
        IMAGE_SECTION_HEADER sec;
        uintptr_t base;
        size_t len, off;
        uint8_t *buf;
        int ok = 1;
        if (!copy_game(&sec, sh + (uintptr_t)s * sizeof sec, sizeof sec)) continue;
        if (!(sec.Characteristics & IMAGE_SCN_MEM_EXECUTE) || !sec.Misc.VirtualSize) continue;
        len = sec.Misc.VirtualSize;
        if (len > DCAM_SCAN_MAX) len = DCAM_SCAN_MAX;
        base = image + sec.VirtualAddress;
        buf = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, len);
        if (!buf) continue;
        for (off = 0; off < len && ok; off += DCAM_SCAN_CHUNK) {
            size_t n = (len - off) < DCAM_SCAN_CHUNK ? (len - off) : DCAM_SCAN_CHUNK;
            if (!copy_game(buf + off, base + off, n)) ok = 0;
        }
        if (!ok) { HeapFree(GetProcessHeap(), 0, buf); continue; }
        diag[0] = 0;
        if (dcam_resolve(buf, len, (uint32_t)base, &a, &why, diag, sizeof diag)) {
            HeapFree(GetProcessHeap(), 0, buf);
            if (a.veh_base != VEH_BASE || a.veh_stride != VEH_STRIDE) {
                dcam_publish(DCAM_REFUSED, "vehicle array differs from the reviewed layout", NULL);
                return;
            }
            dcam_publish(DCAM_RESOLVED, "signatures matched (read-only resolver)", &a);
            return;
        }
        if (strncmp(why, "vehicle layout", 14) == 0) refuse_now = 1;
        sec_pushes = dcam_count_pushes(buf, len);
        pushes += sec_pushes;
        if (!g_dcam_diag_logged && sec_pushes > 0) {
            char secname[9];
            memcpy(secname, sec.Name, 8);
            secname[8] = 0;
            g_dcam_diag_logged = 1;
            log_f("director: pattern counts in %s (first step failing: %s): %s", secname, why, diag);
        }
        HeapFree(GetProcessHeap(), 0, buf);
        if (refuse_now) break;
    }
unresolved:
    if (refuse_now) { dcam_publish(DCAM_REFUSED, why, NULL); return; }
    if (pushes >= 100) {
        char msg[200];
        snprintf(msg, sizeof msg, "code recognised as unknown (not this camera code base); last failing step: %s", why);
        dcam_publish(DCAM_REFUSED, msg, NULL);
        return;
    }
    if (g_dcam_attempts >= DCAM_MAX_ATTEMPTS) {
        dcam_publish(DCAM_REFUSED, "gave up after 60 attempts (code still not matched)", NULL);
        return;
    }
    dcam_publish(DCAM_RETRY, why, NULL);
}

/* ---- C-g: names (camera_editor.c parse_file LocalCam rule; read_save_dir). HTTP thread only. ---- */
static char *dcam_trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static int dcam_is_fixed(const char *name)
{
    int i;
    for (i = 0; i < DCAM_FIXED_COUNT; ++i) if (!strcmp(name, DCAM_FIXED_NAMES[i])) return 1;
    return 0;
}

static void dcam_names_refresh(void)
{
    char root[MAX_PATH], ini[MAX_PATH + 16], save[MAX_PATH] = "SAVE", value[MAX_PATH], path[MAX_PATH * 2], line[512];
    char *slash;
    FILE *f;
    DWORD n;
    if (g_dcam_names_at && GetTickCount() - g_dcam_names_at < 5000) return;
    g_dcam_names_at = GetTickCount() | 1u;
    g_dcam_onboard_n = 0;
    g_dcam_names_ok = 0;
    n = GetModuleFileNameA(NULL, root, sizeof root);
    if (!n || n >= sizeof root) return;
    slash = strrchr(root, '\\');
    if (!slash) return;
    *slash = 0;
    snprintf(ini, sizeof ini, "%s\\Config.ini", root);
    GetPrivateProfileStringA("COMPONENTS", "SAVEDIR", "SAVE", value, sizeof value, ini);
    for (size_t k = strlen(value); k && (value[k - 1] == '\\' || value[k - 1] == '/' || value[k - 1] == ' '); --k)
        value[k - 1] = 0;
    if (value[0] && !strstr(value, "..") && !strchr(value, ':')) snprintf(save, sizeof save, "%s", value);
    {
        int w = snprintf(path, sizeof path, "%s\\%s\\Default.cfg", root, save);
        if (w < 0 || (size_t)w >= sizeof path) return;   /* path too long: no ONBOARD names */
    }
    f = fopen(path, "r");
    if (!f) return;
    g_dcam_names_ok = 1;
    while (fgets(line, sizeof line, f)) {
        char *c, *s, *eq, *name;
        int i, dup = 0;
        if ((c = strstr(line, "//")) != NULL) *c = 0;
        if ((c = strstr(line, "\\\\")) != NULL) *c = 0;
        s = dcam_trim(line);
        if (!*s || *s == '{' || *s == '}') continue;
        eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        if (_stricmp(dcam_trim(s), "LocalCam") != 0) continue;
        name = dcam_trim(eq + 1);
        if (!name[0]) continue;
        for (i = 0; name[i] && i < DCAM_NAME_MAX - 1; ++i)
            if (name[i] >= 'a' && name[i] <= 'z') name[i] = (char)(name[i] - 32);
        name[i] = 0;
        if (dcam_is_fixed(name)) continue;
        for (i = 0; i < g_dcam_onboard_n; ++i) if (!strcmp(g_dcam_onboard[i], name)) { dup = 1; break; }
        if (dup) continue;
        if (g_dcam_onboard_n >= DCAM_MAX_CAMS) break;
        snprintf(g_dcam_onboard[g_dcam_onboard_n], DCAM_NAME_MAX, "%s", name);
        g_dcam_onboard_n++;
    }
    fclose(f);
}

/* web_telemetry.ini [Director] Enabled (default 0). 0 = writes off (POST answers 404); GET still answers. */
static void dcam_settings_read(const char *ini_path)
{
    char line[256];
    int in_dir = 0;
    FILE *f;
    if (!ini_path || !ini_path[0]) return;
    f = fopen(ini_path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *p = line, *eq, *key, *value;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '[') {
            char *close = strchr(p, ']');
            in_dir = close && close - p == 9 && !strnicmp(p + 1, "Director", 8);
            continue;
        }
        if (!in_dir || *p == ';' || *p == '#' || *p == '\r' || *p == '\n') continue;
        eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        key = dcam_trim(p);
        value = dcam_trim(eq + 1);
        if (!strnicmp(key, "enabled", 7) && !key[7]) {
            if (value[0] == '0') g_dcam_enabled = 0;
            else if (value[0] == '1') g_dcam_enabled = 1;
            log_f("settings: [Director] Enabled=%d (TV Director: writes need Enabled=1)", g_dcam_enabled);
        }
        if (!strnicmp(key, "carswitch", 9) && !key[9]) {
            if (value[0] == '0') g_dcam_carswitch_enabled = 0;
            else if (value[0] == '1') g_dcam_carswitch_enabled = 1;
            log_f("settings: [Director] CarSwitch=%d (phase 2: car writes need Enabled=1 and CarSwitch=1)", g_dcam_carswitch_enabled);
        }
        if (!strnicmp(key, "hidehudoncinematic", 18) && !key[18]) {
            if (value[0] == '0') InterlockedExchange(&g_dcam_hide_hud, 0);
            else if (value[0] == '1') InterlockedExchange(&g_dcam_hide_hud, 1);
            log_f("settings: [Director] HideHudOnCinematic=%d (1 = SWINGMAN, ONBOARD and circuit cameras hide the key-2 HUD)", (int)g_dcam_hide_hud);
        }
        if (!strnicmp(key, "mutecarchangecommentary", 23) && !key[23]) {
            if (value[0] == '0') g_dcam_mute_commentary = 0;
            else if (value[0] == '1') g_dcam_mute_commentary = 1;
            log_f("settings: [Director] MuteCarChangeCommentary=%d (1 = no commentary line on a TV Director car change)", g_dcam_mute_commentary);
        }
    }
    fclose(f);
}

/* Host header allowlist (spec 6.3; security review 2026-10-13 M3, applied to every route by http_serve):
 * localhost, 127.0.0.1 or [::1], plus the [Mod] BindAddress when Remote=1 resolved one. The port must
 * be the port this listener is bound to (no port means port 80, which is never served). A request needs
 * exactly one Host header. */
static int dcam_host_value_ok(const char *v, size_t n)
{
    static const char *const names[3] = { "localhost", "127.0.0.1", "[::1]" };
    char port[16];
    size_t pl;
    int i;
    if (g_http_bound_port <= 0) return 0;
    _snprintf(port, sizeof port, "%ld", (long)g_http_bound_port);
    pl = strlen(port);
    for (i = 0; i < 4; ++i) {
        const char *name = i < 3 ? names[i] : g_bind_text;
        size_t L = strlen(name);
        if (i == 3 && (g_bind_s_addr == 0 || !strcmp(g_bind_text, "127.0.0.1"))) break;
        if (n < L || strnicmp(v, name, L) != 0) continue;
        if (n == L) return 0;
        if (v[L] != ':' || n != L + 1 + pl) continue;
        if (!strncmp(v + L + 1, port, pl)) return 1;
    }
    return 0;
}

static int dcam_host_ok(const char *req)
{
    const char *line = strchr(req, '\n');
    const char *value = NULL;
    size_t vn = 0;
    int count = 0;
    while (line) {
        const char *h = line + 1;
        if (!h[0] || h[0] == '\r' || h[0] == '\n') break;
        if (!strnicmp(h, "Host:", 5)) {
            const char *v = h + 5;
            while (*v == ' ' || *v == '\t') v++;
            if (count++ == 0) { value = v; vn = strcspn(v, "\r\n"); }
        }
        line = strchr(h, '\n');
    }
    return count == 1 && dcam_host_value_ok(value, vn);
}

/* GET /director/state body (phase 0 fields). Returns the length, or -1 when the buffer is too small. */
static int dcam_state_json(char *buf, int bufsz, int viewer_local)
{
    DcamSnap s;
    DcamAddrs a;
    char reason[DCAM_REASON_MAX];
    LONG st;
    DWORD now = GetTickCount(), t_cap, age;
    unsigned seq, gen;
    int p = 0, game_running, i;
    char esc[1024];
    static const char *const engine[4] = { "not_tried", "retrying", "resolved", "refused" };
    DcamResult res;
    int sok;
    uint32_t sm, si;

    EnterCriticalSection(&g_telemetry_cs);
    s = g_dcam_snap;
    a = g_dcam_addr;
    snprintf(reason, sizeof reason, "%s", g_dcam_reason);
    st = InterlockedCompareExchange(&g_dcam_state, 0, 0);
    t_cap = capture_time;
    seq = capture_sequence;
    gen = capture_generation;
    res = g_dcam_result;
    sok = g_dcam_saved_ok;
    sm = g_dcam_saved_mode;
    si = g_dcam_saved_index;
    LeaveCriticalSection(&g_telemetry_cs);
    dcam_names_refresh();

    age = seq ? now - t_cap : 0;
    game_running = seq && age <= 1000;
    format_failed = 0;
    p += _snprintf(buf + p, bufsz - p,
        "{\"api\":\"director\",\"phase\":1,\"write_enabled\":%s,\"viewer_local\":%s,\"game_running\":%s,"
        "\"sample_age_ms\":%lu,\"capture_sequence\":%u,\"session_generation\":%u,\"sim_time_s\":%.3f,\"carswitch_enabled\":%s,"
        "\"hide_hud\":%s,\"mute_commentary\":%s,",
        g_dcam_enabled ? "true" : "false", viewer_local ? "true" : "false", game_running ? "true" : "false",
        (unsigned long)age, seq, gen, s.sim_time, g_dcam_carswitch_enabled ? "true" : "false",
        g_dcam_hide_hud ? "true" : "false", g_dcam_mute_commentary ? "true" : "false");
    esc_json(esc, sizeof esc, reason);
    p += _snprintf(buf + p, bufsz - p, "\"camera_engine\":\"%s\",\"camera_engine_reason\":\"%s\",",
                   engine[st >= 0 && st <= 3 ? st : 0], esc);
    if (st == DCAM_RESOLVED)
        p += _snprintf(buf + p, bufsz - p,
            "\"camera_addresses\":{\"director_ptr\":\"0x%08lX\",\"manager\":\"0x%08lX\",\"view_dispatch\":\"0x%08lX\","
            "\"mirror_setup\":\"0x%08lX\",\"target_ptr\":\"0x%08lX\",\"player_ptr\":\"0x%08lX\",\"active_ptrs\":\"0x%08lX\","
            "\"veh_base\":\"0x%08lX\",\"veh_stride\":\"0x%04lX\",\"roster_count\":\"0x%08lX\","
            "\"active_view_ptr\":\"0x%08lX\",\"static_group_ptr\":\"0x%08lX\",\"tracking_group_ptr\":\"0x%08lX\"},",
            (unsigned long)a.director_ptr, (unsigned long)a.manager, (unsigned long)a.view_dispatch,
            (unsigned long)a.mirror_setup, (unsigned long)a.target_ptr, (unsigned long)a.player_ptr,
            (unsigned long)a.active_ptrs, (unsigned long)a.veh_base, (unsigned long)a.veh_stride,
            (unsigned long)a.roster_count, (unsigned long)(a.manager + DCAM_VIEW_ACTIVE_OFF),
            (unsigned long)(a.manager + DCAM_VIEW_STATIC_OFF), (unsigned long)(a.manager + DCAM_VIEW_TRACK_OFF));
    else
        p += _snprintf(buf + p, bufsz - p, "\"camera_addresses\":null,");
    p += _snprintf(buf + p, bufsz - p, "\"snapshot_valid\":%s,", s.valid ? "true" : "false");
    {
        static const char *const owner_name[4] = { "unknown", "director", "circuit_static", "circuit_tracking" };
        const char *why_owner;
        char av[16], sv[16], tv[16], cc[16];
        int owner = s.valid ? s.owner : DCAM_OWNER_UNKNOWN;
        if (owner == DCAM_OWNER_DIRECTOR) why_owner = "active view pointer equals the director object";
        else if (owner == DCAM_OWNER_CIRCUIT_STATIC) why_owner = "active view pointer equals the StaticCam group object";
        else if (owner == DCAM_OWNER_CIRCUIT_TRACKING) why_owner = "active view pointer equals the TrackingCam group object";
        else if (!s.valid) why_owner = "no live director sample";
        else if (!s.active_val) why_owner = "active view pointer not readable";
        else why_owner = "active view pointer matches no known view object (or vtable differs)";
        dcam_hex_or_null(av, sizeof av, s.active_val);
        dcam_hex_or_null(sv, sizeof sv, s.static_val);
        dcam_hex_or_null(tv, sizeof tv, s.tracking_val);
        if ((owner == DCAM_OWNER_CIRCUIT_STATIC || owner == DCAM_OWNER_CIRCUIT_TRACKING) && s.circuit_count_ok)
            snprintf(cc, sizeof cc, "%lu", (unsigned long)s.circuit_count);
        else
            snprintf(cc, sizeof cc, "null");
        p += _snprintf(buf + p, bufsz - p,
            "\"view_owner\":\"%s\",\"view_owner_reason\":\"%s\",\"active_view_value\":%s,\"static_group_value\":%s,"
            "\"tracking_group_value\":%s,\"circuit_count\":%s,",
            owner_name[owner], why_owner, av, sv, tv, cc);
    }
    {
        char vs[24] = "null", ms[24] = "null", vis[24] = "null", vts[24] = "null", cfs[24] = "null";
        uint32_t vslot = 0;
        if (s.followed && dcam_slot_of(s.followed_ptr, &vslot)) snprintf(vs, sizeof vs, "%lu", (unsigned long)vslot);
        if (s.manager_state_ok) snprintf(ms, sizeof ms, "%ld", (long)s.manager_state);
        if (s.view_index_ok) snprintf(vis, sizeof vis, "%lu", (unsigned long)s.view_index);
        if (s.view_target_ok) snprintf(vts, sizeof vts, "\"0x%08lX\"", (unsigned long)s.view_target);
        if (s.circuit_flag_ok) snprintf(cfs, sizeof cfs, "%lu", (unsigned long)s.circuit_flag);
        p += _snprintf(buf + p, bufsz - p, "\"viewed_slot\":%s,\"manager_state\":%s,\"view_index_0x7C54DC\":%s,"
                       "\"view_target_0x7C54E0\":%s,\"circuit_flag_0x7CFCC8\":%s,", vs, ms, vis, vts, cfs);
    }
    if (s.player_ok)
        p += _snprintf(buf + p, bufsz - p, "\"player\":{\"roster_id\":%lu,\"vehicle\":\"0x%08lX\"},",
                       (unsigned long)s.player_roster, (unsigned long)s.player_ptr);
    else
        p += _snprintf(buf + p, bufsz - p, "\"player\":null,");
    if (s.followed) {
        esc_json(esc, sizeof esc, s.driver_name);
        p += _snprintf(buf + p, bufsz - p, "\"followed\":{\"roster_id\":%lu,\"roster_index_4898\":%lu,\"driver_id_489c\":%lu,"
                       "\"table_index\":%d,\"vehicle\":\"0x%08lX\",\"driver_name\":\"%s\",",
                       (unsigned long)s.followed_roster, (unsigned long)s.followed_roster_index,
                       (unsigned long)s.followed_driver_id, s.table_index, (unsigned long)s.followed_ptr, esc);
        esc_json(esc, sizeof esc, s.car_name);
        p += _snprintf(buf + p, bufsz - p, "\"car_name\":\"%s\"},", esc);
    } else {
        p += _snprintf(buf + p, bufsz - p, "\"followed\":null,");
    }
    if (s.valid && s.owner == DCAM_OWNER_DIRECTOR) {
        char angle[DCAM_NAME_MAX + 8] = "";
        if (s.mode < DCAM_MODE_ONBOARD) snprintf(angle, sizeof angle, "%s", DCAM_FIXED_NAMES[s.mode]);
        else if (s.mode == DCAM_MODE_ONBOARD && s.index < (uint32_t)g_dcam_onboard_n)
            snprintf(angle, sizeof angle, "%s", g_dcam_onboard[s.index]);
        esc_json(esc, sizeof esc, angle);
        p += _snprintf(buf + p, bufsz - p, "\"mode\":%lu,\"index\":%lu,\"count\":%lu,\"angle\":\"%s\",",
                       (unsigned long)s.mode, (unsigned long)s.index, (unsigned long)s.count, esc);
    } else if (s.valid && s.owner != DCAM_OWNER_UNKNOWN) {
        /* Circuit (trackside) view on screen: the director mode is stored, not on screen (see director_mode_raw). */
        p += _snprintf(buf + p, bufsz - p, "\"mode\":null,\"index\":null,\"count\":null,\"angle\":\"CIRCUIT CAMERA\",");
    } else {
        p += _snprintf(buf + p, bufsz - p, "\"mode\":null,\"index\":null,\"count\":null,\"angle\":null,");
    }
    if (s.valid) p += _snprintf(buf + p, bufsz - p, "\"director_mode_raw\":%lu,", (unsigned long)s.mode);
    else p += _snprintf(buf + p, bufsz - p, "\"director_mode_raw\":null,");
    p += _snprintf(buf + p, bufsz - p, "\"names_source\":\"%s\",\"names\":[", g_dcam_names_ok ? "Default.cfg" : "missing");
    for (i = 0; i < DCAM_FIXED_COUNT + g_dcam_onboard_n; ++i) {
        const char *nm = i < DCAM_FIXED_COUNT ? DCAM_FIXED_NAMES[i] : g_dcam_onboard[i - DCAM_FIXED_COUNT];
        esc_json(esc, sizeof esc, nm);
        p += _snprintf(buf + p, bufsz - p, "%s\"%s\"", i ? "," : "", esc);
    }
    {
        char lastseq[24] = "null", lastres[1024], saved_s[64] = "null", car_s[1100] = "null";
        if (res.done && (strcmp(res.status, "applied") == 0 || strcmp(res.status, "partial") == 0))
            snprintf(lastseq, sizeof lastseq, "%lu", (unsigned long)res.seq);
        if (res.done) dcam_cmd_json(lastres, sizeof lastres, "last", &res);
        else snprintf(lastres, sizeof lastres, "null");
        if (sok) snprintf(saved_s, sizeof saved_s, "{\"mode\":%lu,\"index\":%lu}", (unsigned long)sm, (unsigned long)si);
        if (s.followed) { esc_json(esc, sizeof esc, s.car_name); _snprintf(car_s, sizeof car_s, "\"%s\"", esc); }
        p += _snprintf(buf + p, bufsz - p, "],\"last_applied_seq\":%s,\"last_result\":%s,\"saved\":%s,"
                       "\"on_air\":{\"car_name\":%s,\"scope\":\"own game\"}}", lastseq, lastres, saved_s, car_s);
    }
    if (format_failed || p >= bufsz) return -1;
    return p;
}
