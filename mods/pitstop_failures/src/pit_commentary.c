/*
 * pit_commentary.c — merged pit exit commentary + pit-stop failures
 *
 * Exit commentary (pit_exit_commentary.c, CONFIRMED live):
 *   PIT06 "Limiter On"  (eid 51)  pittingStatus 1->2
 *   PIT10 "Go"          (eid 45)  pittingStatus 3->4
 *   PIT12 "Limiter Off" (eid 43)  pittingStatus 4->0
 *
 * Pit failures (this file):
 *   Wheel gun / stall / slow tire / fuel / AI failures
 *
 * BUILD
 *   powershell -NoProfile -File ..\build.ps1
 * Development port 0.1.0-port.1: Development build; runtime adaptation pending.
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

/* ---------- globals ------------------------------------------------------ */
#define RACE_ACTIVE       (*(volatile int*)0x007D0308)
#define SESSION_TYPE      (*(volatile int*)0x007C56A4)  /* 5=qual 7=race */
/* dword_7C569C. The observable that MOVES when RestartGate_004a0fd0 fires the
 * teardown - used by retire_player_to_pits() to know the gate has run, since
 * the flag it consumes (dword_7CE258) is left at 1 rather than cleared. */
#define GAME_STATE        (*(volatile int*)0x007C569C)
#define CONTROLLER        0x005EA618
#define EVENT_TABLE       0x005BF9A0
#define COMMENTARY_ON     ((volatile int*)(CONTROLLER + 0xAF0))
#define EVENT_TS(eid)     (*(volatile float*)(CONTROLLER + 0xAF8 + (eid) * 4))
#define VEH_BASE          0x005EC6D8
#define VEH_STRIDE        0x48C0
#define SIMTIME           0x008AFEA0

/* per-vehicle offsets (FACT/CONFIRMED) */
#define OFF_ACTIVE        0x32C8
#define OFF_PITTING       0x3450
#define OFF_SVCRDY        0x345C
#define OFF_ENDT          0x3470
#define OFF_FUEL          0x3984   /* float, current fuel level (L) */
#define OFF_FUEL_START    0x347C   /* float, fuel level when service began */
#define OFF_RAMP          0x3480   /* float, refuel ramp start sim-time */
#define OFF_FUEL_TGT      0x3484   /* float, fuel target (clamp ceiling) */
#define OFF_TYRE_START    0x349C   /* float, tire animation start */
#define OFF_CARC          0x3D6C   /* carConditions: 0=clean 1=damage 2=terminal */
#define OFF_REASON        0x4068   /* ReasonCode: 6=wheel loss */
#define OFF_FINISH        0x4064   /* FinishState: 2=retired */
/* player-out flag. RestartGate_004a0fd0 consumes it on its next tick and runs
 * ConditionalInitGuard() -> sub_4A0250() = session teardown to the menu. This is
 * the game's own path: sub_4F1FE0's player-retire branch ends with the same
 * write. FACT, live-proven 2026-07-29. */
#define PLAYER_OUT        (*(volatile int*)0x007CE258)
#define EID_RETIRE        117      /* PWS_RETIRE02 "and that's the end" */
#define OFF_LOLLI         0x3478   /* int, lollipop: 1=up 0=down (RECALCULATED) */

/* HUD text overlay (proven from red_flag/gr41.c) */
#define HUD_OUTER_PTR     0x007CD204
#define HUD_SLOT_COUNT    8
#define HUD_SLOT_STRIDE   0x5C
#define HUD_SLOT_TEXT     0x3C
#define HUD_SLOT_TIMER    0x90

/* ---------- player failure eids ------------------------------------------ */
#define EID_P_WHEELGUN    61
#define EID_P_FL_SLOW     63
#define EID_P_FR_SLOW     65
#define EID_P_RL_SLOW     68
#define EID_P_RR_SLOW     70
#define EID_P_WHEELJAM    66
#define EID_P_REFUEL      56
#define EID_P_FUELHOSE    56   /* PWS_2001_79 "fuel hose problem" */
#define EID_P_STALL       54
/* pseudo-eids (>=9000, never collide with real eid table entries). */
#define EID_P_CROSSTHREAD 9001  /* cross-threaded nut, long, recoverable */
#define EID_P_FUEL_SLOW   9002  /* fuel hose: slow flow (variant A) */
#define EID_P_FUEL_NOFUEL 9003  /* fuel hose: no flow (variant B) */
#define EID_P_FUEL_CUT    9004  /* fuel hose: cuts off after 3L (variant C) */
#define EID_P_FUEL_SPILL  9005  /* fuel spill: lose fuel, extra time (variant D) */
#define EID_RETIRE_PITS   9100  /* GUI: retire player to pits (SessionType flip) */
#define EID_RETIRE_MENU   9101  /* GUI: retire player to menu (original path) */

/* per-corner wheel gun: slow clip → wheel jammed → resolved */
static const int WG_SLOW[4]    = { 63, 65, 68, 70 };  /* "problem with X" */
static const int WG_OK[4]      = { 62, 64, 67, 69 };  /* "X clear" */
#define EID_WHEEL_JAMMED 66                              /* "wheel jammed" */
static const char* WG_NAMES[4] = { "FL", "FR", "RL", "RR" };
static const char* WG_FAIL[4] = {
    "WHEEL GUN FAILURE (FL)", "WHEEL GUN FAILURE (FR)",
    "WHEEL GUN FAILURE (RL)", "WHEEL GUN FAILURE (RR)"
};
static const char* WG_FIXED[4] = {
    "WHEEL GUN FIXED (FL)", "WHEEL GUN FIXED (FR)",
    "WHEEL GUN FIXED (RL)", "WHEEL GUN FIXED (RR)"
};

/* ---------- pit exit commentary eids (proven, pit_exit_commentary.c) ----- */
#define EID_EXIT_LIMITER_ON  51
#define EID_EXIT_GO          45
#define EID_EXIT_LIMITER_OFF 43
#define EID_PIT_BRAKE        53   /* PWS_PIT08 "Brake" — entering pit box */

/* ---------- AI failure eids ---------------------------------------------- */
#define EID_AI_GENERIC    183
#define EID_AI_STALL      184
#define EID_AI_FUEL       185
#define EID_AI_FL_STUCK   186
#define EID_AI_FR_STUCK   187
#define EID_AI_RL_STUCK   188
#define EID_AI_RR_STUCK   189

/* ---------- tunables (all overridable from pit_failures.ini) --------------
 *
 * Durations are triangular(min, mode, max) and sampled PER STOP, not fixed:
 * a fixed constant made every wheel gun failure cost exactly the same time,
 * which was the loudest tell that the whole thing was scripted.
 *
 * Probabilities are PER MILLE of a SINGLE draw (see pick_player). They used to
 * be independent sequential rolls, which stacked to a ~55% failure rate per
 * stop - the per-corner loop alone was 1 - 0.88^4 = 40%. */
typedef struct { float lo, mode, hi; } Tri;

static struct {
    /* per-mille, single draw, cumulative buckets - must sum to <= 1000 */
    int p_slow_corner;      /* one corner slower than usual        */
    int p_wheelgun;         /* wheel gun trouble, recovers         */
    int p_fuel;             /* fuel rig problem                    */
    int p_stall;            /* stall on release                    */
    int p_crossthread;      /* cross-threaded nut, long, recovers  */
    int p_jam_terminal;     /* wheel jammed -> DNF                 */

    int corner_w[4];        /* FL FR RL RR relative weights        */

    Tri d_slow_corner;
    Tri d_wheelgun;
    Tri d_crossthread;
    Tri d_stall;
    Tri d_fuel_slow, d_fuel_none, d_fuel_cut;
    Tri d_fuel_spill;       /* delay until spill is contained */
    float fuel_spill_pct;   /* fraction of target lost to spill (0.0-1.0) */
    Tri d_jam_dnf;          /* box-pinned time between retire call and DNF */

    /* Each wheelguns_N.wav is already a COMPLETE gun sequence (BRRT BRRT), not
     * a single burst - so one clip per corner per stop is the whole job. Only
     * a FAILING corner repeats, and sparsely, capped. */
    Tri  d_gun_repeat;      /* gap between retries on a failing corner */
    int  gun_max_repeats;   /* hard cap on retries, so it can never run away */

    float fuel_ramp_delay;  /* seconds from ramp capture to detect failure  */
    int   fuel_recover_pct; /* chance fuel hose recovers first attempt       */
    Tri   d_fuel_reattempt; /* extra time when fuel recovery fails           */

    int  recover_pct;       /* chance a recoverable failure clears first try */
    Tri  d_reattempt;       /* extra time when it does not                   */
    int  escalate_pct;      /* chance a twice-extended crossthread goes DNF  */
    float rear_penalty;     /* rear corners take this much longer to clear   */
} CFG = {
    /* 88.0% clean, 12.0% failure, 0.4% terminal */
    .p_slow_corner  = 40,
    .p_wheelgun     = 25,
    .p_fuel         = 20,
    .p_stall        = 15,
    .p_crossthread  = 10,
    .p_jam_terminal = 4,

    .corner_w = { 28, 28, 22, 22 },

    .d_slow_corner = {  1.0f,  2.0f,  4.5f },
    .d_wheelgun    = {  4.0f,  7.0f, 14.0f },
    .d_crossthread = { 10.0f, 15.0f, 25.0f },
    .d_stall       = {  2.5f,  4.5f,  9.0f },
    .d_fuel_slow   = {  6.0f, 10.0f, 18.0f },
    .d_fuel_none   = {  5.0f,  8.0f, 15.0f },
    .d_fuel_cut    = {  4.0f,  7.0f, 12.0f },
    .d_fuel_spill  = {  3.0f,  5.0f, 10.0f },
    .fuel_spill_pct = 0.30f,  /* lose 30% of target to spill */
    .d_jam_dnf     = {  6.0f,  8.0f, 12.0f },

    .d_gun_repeat    = { 2.5f, 3.5f, 5.0f },
    .gun_max_repeats = 3,

    .fuel_ramp_delay  = 1.0f,
    .fuel_recover_pct = 80,
    .d_fuel_reattempt = {  2.0f,  3.5f,  7.0f },

    .recover_pct   = 85,
    .d_reattempt   = {  3.0f,  5.0f,  9.0f },
    .escalate_pct  = 20,
    .rear_penalty  = 0.5f,
};

#define DELAY_AI_GENERIC  3.0f
#define DELAY_AI_STALL    6.0f
#define DELAY_AI_FUEL     4.0f
#define DELAY_AI_WHEEL    5.0f

#define SLOTS             104
#define POLL_MS           100
#define HOLD_LEAD         3.0f
#define INI_NAME          "pit_failures.ini"
#define FLT_MAX_GUARD     1e37f
#define UNLOAD_EVENT_NAME "F1C_EVF_UNLOAD_pit_commentary"
#define PAUSE_EVENT_NAME  "F1C_EVF_PAUSE_pit_failure_commentary"
#define OFF_QUEUE_EID     0x34A4   /* int32, GUI writes queued eid here */
#define OFF_QUEUE_DELAY   0x34A8   /* float, GUI writes queued delay here */

/* refuel constants — game reads float at VA 0x59E96C every frame during
 * PitStopProgress, never writes. Page is in .rdata (read-only) — must
 * VirtualProtect before writing. Restore 12.0 on recovery/exit. */
#define REFUEL_RATE_VA    0x0059E96C
#define REFUEL_RATE       12.0f        /* L/s normal */
#define SLOW_RATE         4.0f         /* L/s during fuel hose failure */

static void set_refuel_rate(float rate) {
    DWORD old;
    VirtualProtect((LPVOID)REFUEL_RATE_VA, 4, PAGE_READWRITE, &old);
    *(volatile float*)REFUEL_RATE_VA = rate;
    VirtualProtect((LPVOID)REFUEL_RATE_VA, 4, old, &old);
}

/* The game's fuel level is ABSOLUTE, not integrated (PitStopProgress_004a68b0):
 *     fuel = fuelStart + (simTime - refuelRampStart) * RATE,  clamped to target
 * so changing RATE alone teleports the level by elapsed*(oldRate-newRate).
 * Dropping 12 -> 4 ten seconds in yanks 80 L back out of the tank; restoring
 * 4 -> 12 on recovery instantly fills it (the "magic full tank").
 * Re-anchor fuelStart on every rate change so the level stays continuous. */
static float g_cur_rate = REFUEL_RATE;

static void  log_w(const char* fmt, ...);
static float rd_ramp(int s);
static float rd_fuel(int s);
static float rd_fuel_start(int s);
static void  wr_fuel_start(int s, float v);
static float sim_t(void);

static void set_refuel_rate_c(float rate) {
    if (rate == g_cur_rate) return;

    float ramp = rd_ramp(0);
    if (ramp > 0) {
        float el = sim_t() - ramp;
        if (el > 0) {
            float fuel = rd_fuel(0);
            wr_fuel_start(0, fuel - el * rate);
            log_w("RATE %.1f -> %.1f: re-anchor fuelStart %.2f -> %.2f "
                  "(fuel %.2f, elapsed %.2f)",
                  g_cur_rate, rate, rd_fuel_start(0), fuel - el * rate, fuel, el);
        }
    }
    set_refuel_rate(rate);
    g_cur_rate = rate;
}
#define DETECT_SLOW       3.0f    /* observe slow rate for N seconds */
#define DETECT_NONE       2.0f    /* observe no-flow for N seconds */
#define DETECT_CUT        2.0f    /* observe flow stop for N seconds */

/* fuel failure variants */
#define FUEL_VAR_NONE     0
#define FUEL_VAR_SLOW     1    /* A: fuel pumping slowly */
#define FUEL_VAR_NOFUEL   2    /* B: fuel not coming at all */
#define FUEL_VAR_CUT      3    /* C: fuel intermittent (cuts off) */
#define FUEL_VAR_SPILL    4    /* D: fuel spill — lose fuel, extra time */

static volatile BOOL g_running = TRUE;
static HINSTANCE g_hinst = NULL;
static HANDLE    g_unload_ev = NULL;
static char      g_log[MAX_PATH];

/* per-vehicle state */
static int   g_prev_pitting[SLOTS];
static int   g_failure_eid[SLOTS];
static float g_failure_delay[SLOTS];

/* fuel manipulation state (player slot 0 only)
 *
 * Game formula: FuelLevel = fuelAtStart + (simTime - refuelRampStart) * 12.0
 * We manipulate refuelRampStart (veh+0x3480) to change apparent flow rate:
 *   A (slow):  push ramp forward so apparent rate = 4 L/s
 *   B (none):  push ramp to +inf so fuel never rises
 *   C (cut):   let flow normally, then push ramp to +inf after some fuel */
static int   g_fuel_variant;        /* FUEL_VAR_NONE or 1/2/3 */
static float g_fuel_orig_ramp;      /* original refuelRampStart (captured before manipulation) */
static float g_fuel_srv_start;      /* sim_time when service became ready */
static float g_fuel_cut_since;      /* sim_time when cut started (variant C) */
static float g_fuel_last_level;     /* previous poll fuel level */
static float g_fuel_trace_at;       /* sim-time of last FUELTRACE line */
static int   g_fuel_fired;          /* 1 once we've fired audio */
static float g_fuel_delay;          /* penalty delay for this variant */
static float g_fuel_resolved_at;    /* sim_time to check fuel hose recovery */
static int   g_fuel_extends;        /* fuel recovery attempts so far */
static float g_fuel_hud_at;         /* sim_time to show fuel HUD (delayed) */
static float g_fuel_spill_loss;     /* liters lost to spill (applied at resolution) */
static float g_fuel_spill_orig_tgt; /* original fuel target before spill reduction */
static int   g_wheelgun_armed;       /* 1 = wheel gun failure deferred, waiting for tyre anims */
static int   g_wheelgun_corner;      /* 0=FL 1=FR 2=RL 3=RR, which corner's gun failed */
static int   g_wheelgun_terminal;    /* 1 = terminal DNF at resolved time (wheel jammed) */
static int   g_wheelgun_cross;       /* 1 = cross-threaded nut (long, can escalate) */
static float g_wheelgun_delay;       /* sampled duration for THIS stop (was a fixed 25.0) */
static int   g_wheelgun_extends;     /* how many times this failure has been re-extended */
static float g_wheelgun_fire_at;     /* sim_time when to fire audio after tyre anims start */
static float g_wheelgun_jammed_at;   /* sim_time when to fire "wheel jammed" clip */
static float g_wheelgun_resolved_at; /* sim_time when to show "RESOLVED" HUD */
static float g_last_go_time;          /* sim_time when "Go" was fired (gates Limiter Off) */
static int   g_dnf_active;            /* 1 = terminal DNF, keep poking flags */
static int   g_jam_hold;              /* 1 = car pinned in the box until the DNF fires */
static float g_jam_dnf_at;            /* sim_time to execute the DNF (car never leaves) */
static int   g_wg_sfx_played;         /* 1 = this stop's gun bursts are scheduled */
static float g_burst_off_at[4];       /* sim_time of each corner's next gun clip */
static float g_burst_on_at[4];        /* sim_time of ON (tightening) burst */
static int   g_burst_reps[4];         /* clips played this stop, per corner */
static int   g_burst_on_done[4];      /* 1 = ON burst already fired */

/* ---------- inline RNG --------------------------------------------------- */
static unsigned long long g_rand_seed;
static void rand_seed(unsigned s) { g_rand_seed = s; }
static unsigned rand_u32(void) {
    g_rand_seed = g_rand_seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(g_rand_seed >> 32);
}
static int roll_pct(void) { return (int)(rand_u32() % 100); }
static int roll_mille(void) { return (int)(rand_u32() % 1000); }

/* Triangular sample: most stops land near `mode`, with a long thin tail out to
 * `hi`. That tail is what makes an occasional disaster stop memorable without
 * making every failure feel identical. */
static float tri(Tri t) {
    float u = (float)(rand_u32() % 100000) / 100000.0f;
    float span = t.hi - t.lo;
    if (span <= 0.0f) return t.lo;
    float c = (t.mode - t.lo) / span;
    if (u < c) return t.lo + sqrtf(u * span * (t.mode - t.lo));
    return t.hi - sqrtf((1.0f - u) * span * (t.hi - t.mode));
}

/* Weighted corner pick: 0=FL 1=FR 2=RL 3=RR. Fronts are marginally more
 * exposed than rears in practice; the weights are config, not physics. */
static int pick_corner(void) {
    int total = 0;
    for (int i = 0; i < 4; i++) total += CFG.corner_w[i];
    if (total <= 0) return (int)(rand_u32() % 4);
    int r = (int)(rand_u32() % (unsigned)total);
    for (int i = 0; i < 4; i++) {
        r -= CFG.corner_w[i];
        if (r < 0) return i;
    }
    return 3;
}

/* Rear corners are harder to get at, so they clear slower. */
static float corner_penalty(int c) { return (c >= 2) ? CFG.rear_penalty : 0.0f; }

/* ---------- config ------------------------------------------------------- */
static void load_cfg(void) {
    char ini[MAX_PATH];
    GetModuleFileNameA(NULL, ini, MAX_PATH);
    char* slash = strrchr(ini, '\\');
    if (slash) strcpy(slash + 1, INI_NAME); else strcpy(ini, INI_NAME);
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) return;  /* keep defaults */

    #define GETI(sec, key, dst) do { \
        int v = GetPrivateProfileIntA(sec, key, -999999, ini); \
        if (v != -999999) (dst) = v; \
    } while (0)
    #define GETF(sec, key, dst) do { \
        char b[64]; \
        if (GetPrivateProfileStringA(sec, key, "", b, sizeof(b), ini) > 0) \
            (dst) = (float)atof(b); \
    } while (0)
    #define GETTRI(sec, key, dst) do { \
        GETF(sec, key "_min",  (dst).lo); \
        GETF(sec, key "_mode", (dst).mode); \
        GETF(sec, key "_max",  (dst).hi); \
    } while (0)

    GETI("probability", "slow_corner",  CFG.p_slow_corner);
    GETI("probability", "wheelgun",     CFG.p_wheelgun);
    GETI("probability", "fuel",         CFG.p_fuel);
    GETI("probability", "stall",        CFG.p_stall);
    GETI("probability", "crossthread",  CFG.p_crossthread);
    GETI("probability", "jam_terminal", CFG.p_jam_terminal);

    GETI("corner", "fl", CFG.corner_w[0]);
    GETI("corner", "fr", CFG.corner_w[1]);
    GETI("corner", "rl", CFG.corner_w[2]);
    GETI("corner", "rr", CFG.corner_w[3]);

    GETTRI("duration", "slow_corner", CFG.d_slow_corner);
    GETTRI("duration", "wheelgun",    CFG.d_wheelgun);
    GETTRI("duration", "crossthread", CFG.d_crossthread);
    GETTRI("duration", "stall",       CFG.d_stall);
    GETTRI("duration", "fuel_slow",   CFG.d_fuel_slow);
    GETTRI("duration", "fuel_none",   CFG.d_fuel_none);
    GETTRI("duration", "fuel_cut",    CFG.d_fuel_cut);
    GETTRI("duration", "fuel_spill",  CFG.d_fuel_spill);
    GETTRI("duration", "jam_dnf",     CFG.d_jam_dnf);
    GETTRI("duration", "reattempt",   CFG.d_reattempt);
    GETTRI("duration", "gun_repeat",  CFG.d_gun_repeat);
    GETI("audio", "gun_max_repeats", CFG.gun_max_repeats);

    GETF("duration", "fuel_ramp_delay", CFG.fuel_ramp_delay);
    GETTRI("duration", "fuel_reattempt", CFG.d_fuel_reattempt);
    GETI("recovery", "fuel_recover_pct", CFG.fuel_recover_pct);
    GETI("recovery", "recover_pct",  CFG.recover_pct);
    GETI("recovery", "escalate_pct", CFG.escalate_pct);
    GETF("recovery", "rear_penalty", CFG.rear_penalty);
    GETF("fuel", "spill_loss_pct", CFG.fuel_spill_pct);
    #undef GETTRI
    #undef GETF
    #undef GETI
}

/* ---------- helpers ------------------------------------------------------ */
static void log_w(const char* fmt, ...) {
    FILE* f = fopen(g_log, "a");
    if (!f) return;
    va_list ap;
    fprintf(f, "[%lu] ", GetTickCount());
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

typedef void (__thiscall *StructInitFn)(void* buf);
typedef void (__thiscall *DriverLookupFn)(void* buf, int eid, int a3, int a4, int a5, int a6, float a7, float a8);
typedef char (__thiscall *InsertEntryFn)(void* controller, void* buf);
#define FN_STRUCT_INIT   ((StructInitFn)0x004144D0)
#define FN_DRIVER_LOOKUP ((DriverLookupFn)0x004144F0)
#define FN_INSERT_ENTRY  ((InsertEntryFn)0x00418560)

static void inject_commentary(int eid) {
    float buf[8];
    FN_STRUCT_INIT(buf);
    FN_DRIVER_LOOKUP(buf, eid, -1, -1, -1, -1, -2.0f, -2.0f);
    FN_INSERT_ENTRY((void*)CONTROLLER, buf);
}

static int  is_active(int s)     { return *(volatile int*)  (VEH_BASE + s*VEH_STRIDE + OFF_ACTIVE) != -1; }
static int  rd_pit(int s)        { return *(volatile int*)  (VEH_BASE + s*VEH_STRIDE + OFF_PITTING); }
static int  rd_svcrdy(int s)     { return *(volatile int*)  (VEH_BASE + s*VEH_STRIDE + OFF_SVCRDY); }
static float rd_endt(int s)      { return *(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_ENDT); }
static void wr_endt(int s,float v){*(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_ENDT) = v; }
static float rd_fuel(int s)      { return *(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_FUEL); }
static float rd_fuel_start(int s){ return *(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_FUEL_START); }
static float rd_ramp(int s)      { return *(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_RAMP); }
static void wr_ramp(int s,float v){ *(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_RAMP) = v; }
static void wr_fuel_start(int s,float v){ *(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_FUEL_START) = v; }
static float rd_fuel_target(int s){ return *(volatile float*)(VEH_BASE + s*VEH_STRIDE + OFF_FUEL_TGT); }
static int  rd_queue_eid(void)     { return *(volatile int*)  (VEH_BASE + OFF_QUEUE_EID); }
static void wr_queue_eid(int v)    { *(volatile int*)  (VEH_BASE + OFF_QUEUE_EID) = v; }
static float rd_queue_delay(void)  { return *(volatile float*)(VEH_BASE + OFF_QUEUE_DELAY); }
static void wr_queue_delay(float v){ *(volatile float*)(VEH_BASE + OFF_QUEUE_DELAY) = v; }
static float sim_t(void)         { return (float)(*(volatile double*)SIMTIME); }

/* ---------- wheel gun timing, read from the live crew ---------------------
 *
 * The crew actors name themselves, so the gun windows come from the game's own
 * animation data rather than hardcoded numbers - this survives different pit
 * animation sets (52 .ANM per variant).
 *
 *   veh+0x32D4 -> crew;  crew+0x0C count;  crew+0x10 actor array (stride 120)
 *   actor+100  = ASCII name       actor+112/+116 = channel count / array (28)
 *   channel+12 / +16 = baseStart / baseEnd
 *
 * Corner groups are 6-11 front LEFT, 12-17 front RIGHT, 18-23 rear LEFT,
 * 24-29 rear RIGHT; within a group the names are FMAN/GMAN/NMAN/WHEELGUN/
 * WHEELOFF/WHEELON. We want WHEELGUN (the gun itself) and WHEELON (nut
 * tightening, which ends later than the gun window). */
#define OFF_CREW 0x32D4

static float g_gun_start[4];   /* seconds after tyre anim start */
static float g_gun_end[4];
static int   g_gun_have;

static int rd_ptr_safe(DWORD addr, DWORD* out) {
    if (addr < 0x10000 || addr > 0x7FFF0000) return 0;
    *out = *(volatile DWORD*)addr;
    return 1;
}

static void read_gun_windows(void) {
    /* fallback: fronts 2.3 -> 9.8, rears start at 1.0 (FACT: rears use
     * baseStart 1.0, not 2.3/2.5 - that mismatch broke earlier tooling) */
    g_gun_start[0] = g_gun_start[1] = 2.3f;
    g_gun_start[2] = g_gun_start[3] = 1.0f;
    g_gun_end[0] = g_gun_end[1] = g_gun_end[2] = g_gun_end[3] = 9.8f;
    g_gun_have = 0;

    DWORD crew = 0, arr = 0;
    if (!rd_ptr_safe(VEH_BASE + OFF_CREW, &crew) || !crew) return;
    if (!rd_ptr_safe(crew + 0x10, &arr) || !arr) return;
    int count = *(volatile int*)(crew + 0x0C);
    if (count <= 0 || count > 256) return;

    int found = 0;
    for (int i = 0; i < count; i++) {
        DWORD a = arr + (DWORD)i * 120;
        /* actor+100 is a char* TO the name, not inline chars - reading it as
         * inline text is why the first attempt matched nothing */
        DWORD np = 0;
        if (!rd_ptr_safe(a + 100, &np) || !np) continue;
        const char* nm = (const char*)np;
        if (IsBadReadPtr(nm, 24)) continue;
        if (strncmp(nm, "WHEELGUN", 8) != 0) continue;

        /* corner from the name itself: WHEELGUN_FRONT_LEFT / _FRONT_RIGHT /
         * _REAR_LEFT / _REAR_RIGHT. Names are more robust than index bands. */
        int corner = -1;
        if      (strstr(nm, "FRONT_LEFT"))  corner = 0;
        else if (strstr(nm, "FRONT_RIGHT")) corner = 1;
        else if (strstr(nm, "REAR_LEFT"))   corner = 2;
        else if (strstr(nm, "REAR_RIGHT"))  corner = 3;
        if (corner < 0) continue;

        int cc = *(volatile int*)(a + 112);
        DWORD cp = 0;
        if (!rd_ptr_safe(a + 116, &cp) || !cp || cc <= 0 || cc > 64) continue;
        float bs = *(volatile float*)(cp + 12);
        float be = *(volatile float*)(cp + 16);
        if (bs != bs || be != be || be <= bs || be > 60.0f) continue;
        g_gun_start[corner] = bs;
        g_gun_end[corner]   = be;
        found++;
    }
    if (found) {
        g_gun_have = 1;
        log_w("GUN WINDOWS: %d corners from crew  FL %.1f-%.1f  FR %.1f-%.1f  RL %.1f-%.1f  RR %.1f-%.1f",
              found, g_gun_start[0], g_gun_end[0], g_gun_start[1], g_gun_end[1],
              g_gun_start[2], g_gun_end[2], g_gun_start[3], g_gun_end[3]);
    } else {
        log_w("GUN WINDOWS: crew walk found no WHEELGUN actors, using fallback");
    }
}

/* ---------- failure pickers ---------------------------------------------- */
static const int SLOW_EIDS[4] = { EID_P_FL_SLOW, EID_P_FR_SLOW, EID_P_RL_SLOW, EID_P_RR_SLOW };
static const int AI_WHEEL_EIDS[4] = { EID_AI_FL_STUCK, EID_AI_FR_STUCK, EID_AI_RL_STUCK, EID_AI_RR_STUCK };

static void pick_player(int slot, int* out_eid, float* out_delay, int* out_fuel_var) {
    *out_eid = -1; *out_delay = 0; *out_fuel_var = FUEL_VAR_NONE;
    (void)slot;

    /* ONE draw, cumulative buckets. Previously these were independent
     * sequential rolls that stacked to ~55% failure per stop. */
    int r = roll_mille();
    int edge = 0;

    edge += CFG.p_slow_corner;
    if (r < edge) {
        int c = pick_corner();
        *out_eid   = SLOW_EIDS[c];
        *out_delay = tri(CFG.d_slow_corner) + corner_penalty(c);
        return;
    }

    edge += CFG.p_wheelgun;
    if (r < edge)  { *out_eid = EID_P_WHEELGUN; *out_delay = tri(CFG.d_wheelgun);    return; }

    edge += CFG.p_fuel;
    if (r < edge) {
        *out_eid = EID_P_REFUEL;
        int v = (int)(rand_u32() % 4) + 1;   /* SLOW / NOFUEL / CUT / SPILL */
        *out_fuel_var = v;
        *out_delay = (v == FUEL_VAR_SLOW)   ? tri(CFG.d_fuel_slow) :
                     (v == FUEL_VAR_NOFUEL) ? tri(CFG.d_fuel_none) :
                     (v == FUEL_VAR_CUT)    ? tri(CFG.d_fuel_cut)  :
                                              tri(CFG.d_fuel_spill);
        return;
    }

    edge += CFG.p_stall;
    if (r < edge)  { *out_eid = EID_P_STALL;    *out_delay = tri(CFG.d_stall);       return; }

    edge += CFG.p_crossthread;
    if (r < edge)  { *out_eid = EID_P_CROSSTHREAD; *out_delay = tri(CFG.d_crossthread); return; }

    edge += CFG.p_jam_terminal;
    if (r < edge)  { *out_eid = EID_P_WHEELJAM; *out_delay = tri(CFG.d_jam_dnf);     return; }

    /* else: clean stop */
}

static void pick_ai(int slot, int* out_eid, float* out_delay) {
    *out_eid = -1; *out_delay = 0; (void)slot;
    if (roll_pct() < 3)    { *out_eid = EID_AI_STALL;   *out_delay = DELAY_AI_STALL;   return; }
    if (roll_pct() < 6)    { *out_eid = EID_AI_FUEL;    *out_delay = DELAY_AI_FUEL;    return; }
    for (int c = 0; c < 4; c++) {
        if (roll_pct() < 4) { *out_eid = AI_WHEEL_EIDS[c]; *out_delay = DELAY_AI_WHEEL; return; }
    }
    if (roll_pct() < 8)    { *out_eid = EID_AI_GENERIC; *out_delay = DELAY_AI_GENERIC; }
}

/* ---------- HUD text overlay (red_flag pattern) -------------------------- */
static void hud_write(const char* msg) {
    DWORD inner = *(volatile DWORD*)HUD_OUTER_PTR;
    if (!inner) return;
    for (int n = 0; n < HUD_SLOT_COUNT; n++) {
        float* timer = (float*)(inner + n * HUD_SLOT_STRIDE + HUD_SLOT_TIMER);
        if (*timer <= 0.0f) {
            char* text = (char*)(inner + n * HUD_SLOT_STRIDE + HUD_SLOT_TEXT);
            strncpy(text, msg, 31);
            *timer = 4.0f;  /* show for 4 seconds */
            log_w("HUD: '%s' written to slot %d", msg, n);
            return;
        }
    }
    log_w("HUD: no free slot for '%s'", msg);
}

/* ---------- polyphonic WAV playback --------------------------------------
 *
 * PlaySoundA plays ONE sound at a time per process - a second call cuts the
 * first off. That never showed up while the DLL played a single WAV per stop,
 * but four wheel guns overlapping is the whole point, so it needs real voices.
 *
 * One waveOut voice per corner, WAVs decoded once into memory at first use. */
#define WAV_VOICES  8          /* shared pool - more than corners, so clips layer */
#define WAV_SLOTS   4          /* headers in flight per voice */

typedef struct {
    WAVEFORMATEX fmt;
    unsigned char* data;
    DWORD len;
    int loaded;
} Wav;

static Wav      g_wav[3];
static HWAVEOUT g_voice[WAV_VOICES];
static WAVEHDR  g_hdr[WAV_VOICES][WAV_SLOTS];
static int      g_hdr_i[WAV_VOICES];
static int      g_voice_next;
static int      g_audio_ready;

/* Minimal RIFF/WAVE reader: locate 'fmt ' and 'data'. Good enough for the
 * shipped 16-bit PCM effects; anything else is logged and skipped. */
static int wav_load(const char* path, Wav* w) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { log_w("WAV: cannot open %s", path); return 0; }
    DWORD size = GetFileSize(f, NULL);
    if (size < 44 || size > 8u * 1024u * 1024u) { CloseHandle(f); log_w("WAV: bad size %s", path); return 0; }
    unsigned char* buf = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, size);
    DWORD got = 0;
    if (!buf || !ReadFile(f, buf, size, &got, NULL) || got != size) {
        CloseHandle(f); if (buf) HeapFree(GetProcessHeap(), 0, buf);
        log_w("WAV: read failed %s", path); return 0;
    }
    CloseHandle(f);
    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) {
        HeapFree(GetProcessHeap(), 0, buf); log_w("WAV: not RIFF/WAVE %s", path); return 0;
    }
    DWORD p = 12; int have_fmt = 0;
    while (p + 8 <= size) {
        DWORD csz = *(DWORD*)(buf + p + 4);
        if (!memcmp(buf + p, "fmt ", 4) && csz >= 16) {
            memcpy(&w->fmt, buf + p + 8, sizeof(PCMWAVEFORMAT));
            w->fmt.cbSize = 0;
            have_fmt = 1;
        } else if (!memcmp(buf + p, "data", 4)) {
            if (p + 8 + csz > size) csz = size - (p + 8);
            w->data = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, csz);
            if (!w->data) break;
            memcpy(w->data, buf + p + 8, csz);
            w->len = csz;
        }
        p += 8 + csz + (csz & 1);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    if (!have_fmt || !w->data || !w->len) { log_w("WAV: no fmt/data %s", path); return 0; }
    w->loaded = 1;
    return 1;
}

static void audio_init(const char* const* paths, int n) {
    if (g_audio_ready) return;
    int ok = 0;
    for (int i = 0; i < n && i < 3; i++) if (wav_load(paths[i], &g_wav[i])) ok++;
    if (!ok) { log_w("AUDIO: no WAVs loaded, SFX disabled"); g_audio_ready = -1; return; }

    const Wav* ref = NULL;
    for (int i = 0; i < 3; i++) if (g_wav[i].loaded) { ref = &g_wav[i]; break; }
    for (int v = 0; v < WAV_VOICES; v++) {
        if (waveOutOpen(&g_voice[v], WAVE_MAPPER, (WAVEFORMATEX*)&ref->fmt,
                        0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            log_w("AUDIO: waveOutOpen failed for voice %d", v);
            g_voice[v] = NULL;
        }
    }
    g_audio_ready = 1;
    log_w("AUDIO: %d WAV(s), %dHz %dch, %d voices", ok,
          (int)ref->fmt.nSamplesPerSec, (int)ref->fmt.nChannels, WAV_VOICES);
}

/* Fire one burst on the voice belonging to `corner`. Overlapping corners no
 * longer cut each other off; a corner cutting its own previous burst is fine
 * and is what a stuttering gun sounds like. */
static void play_burst(int corner, int which) {
    if (g_audio_ready != 1) return;

    /* Voices are a shared POOL, not one-per-corner: pick any voice whose last
     * buffer has finished, so clips layer over each other freely - including a
     * corner over itself. Falls back to round-robin (cutting the oldest) when
     * every voice is busy. */
    int v = -1;
    for (int i = 0; i < WAV_VOICES; i++) {
        int cand = (g_voice_next + i) % WAV_VOICES;
        if (!g_voice[cand]) continue;
        WAVEHDR* ph = &g_hdr[cand][(g_hdr_i[cand] + WAV_SLOTS - 1) % WAV_SLOTS];
        if (!(ph->dwFlags & WHDR_PREPARED) || (ph->dwFlags & WHDR_DONE)) { v = cand; break; }
    }
    if (v < 0) {
        for (int i = 0; i < WAV_VOICES; i++) {
            int cand = (g_voice_next + i) % WAV_VOICES;
            if (g_voice[cand]) { v = cand; break; }
        }
    }
    if (v < 0) return;
    g_voice_next = (v + 1) % WAV_VOICES;
    if (which < 0 || which > 2 || !g_wav[which].loaded) {
        for (which = 0; which < 3 && !g_wav[which].loaded; which++) ;
        if (which >= 3) return;
    }
    WAVEHDR* h = &g_hdr[v][g_hdr_i[v]];
    g_hdr_i[v] = (g_hdr_i[v] + 1) % WAV_SLOTS;
    if (h->dwFlags & WHDR_PREPARED) {
        if (!(h->dwFlags & WHDR_DONE)) { log_w("SFX: corner %d slot busy, skipped", corner); return; }
        waveOutUnprepareHeader(g_voice[v], h, sizeof(*h));
    }
    memset(h, 0, sizeof(*h));
    h->lpData = (LPSTR)g_wav[which].data;
    h->dwBufferLength = g_wav[which].len;
    MMRESULT pr = waveOutPrepareHeader(g_voice[v], h, sizeof(*h));
    if (pr != MMSYSERR_NOERROR) { log_w("SFX: prepare failed corner %d mmr=%d", corner, (int)pr); return; }
    MMRESULT wr = waveOutWrite(g_voice[v], h, sizeof(*h));
    log_w("SFX: burst corner %d wav %d %s", corner, which,
          wr == MMSYSERR_NOERROR ? "OK" : "WRITE FAILED");
}

static void audio_shutdown(void) {
    for (int v = 0; v < WAV_VOICES; v++) {
        if (!g_voice[v]) continue;
        waveOutReset(g_voice[v]);
        for (int s = 0; s < WAV_SLOTS; s++)
            if (g_hdr[v][s].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(g_voice[v], &g_hdr[v][s], sizeof(WAVEHDR));
        waveOutClose(g_voice[v]);
        g_voice[v] = NULL;
    }
    for (int i = 0; i < 3; i++)
        if (g_wav[i].data) { HeapFree(GetProcessHeap(), 0, g_wav[i].data); g_wav[i].data = NULL; }
    g_audio_ready = 0;
}

/* Development port: resolve assets from the host executable, never cwd. */
static char g_wg_paths[3][MAX_PATH];
static const char* WG_SOUNDS[] = { g_wg_paths[0], g_wg_paths[1], g_wg_paths[2] };
static int init_sound_paths(void) {
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    char* slash = strrchr(exe, '\\');
    if (!slash) return 0;
    *slash = '\0';
    for (int i = 0; i < 3; i++) {
        int written = snprintf(g_wg_paths[i], MAX_PATH,
            "%s\\Audio\\Vehicle\\wheelguns_%d.wav", exe, i + 1);
        if (written < 0 || written >= MAX_PATH) return 0;
    }
    return 1;
}

/* Retire the player for real: set the DNF state the game itself sets, then raise
 * the player-out flag and let RestartGate tear the session down.
 *
 * Do NOT try to replicate the Ignition keybind. DISPROVEN live 2026-07-29:
 * engine speed (veh+0x39A0) is recalculated every frame and restores within one
 * 30 ms frame, veh+0x3984 is fuel and not an ignition gate, and writes to both
 * the game's key queue (word_7CB21C) and the DirectInput keyboard buffer are
 * overwritten by DirectInput before the game reads them. Key posting never
 * worked; this two-write path does. */
static void retire_player(void) {
    *(volatile int*)(VEH_BASE + OFF_CARC)   = 2;   /* terminal condition */
    *(volatile int*)(VEH_BASE + OFF_REASON) = 6;   /* wheel loss */
    *(volatile int*)(VEH_BASE + OFF_FINISH) = 2;   /* DNF */
    /* garaged state */
    *(volatile int*)(VEH_BASE + 0x4488) = 1;
    *(volatile int*)(VEH_BASE + 0x4578) = 1;
    PLAYER_OUT = 1;
}

static void hide_driver_model(void) {
    /* PitCrewStateMachine_004bbfc0 resets this during teardown —
     * write AFTER PLAYER_OUT is consumed. */
    *(volatile char*)(VEH_BASE + 0x105D) = 0;
}

/* Same retirement, but to PITS instead of menu.
 *
 * sub_4A0250 reads SessionType at 0x7C56A4. When ==7 (race), the race block
 * runs LookupDispatch("POST RACE MONITOR") -> menu. Temporarily flipping it
 * to 5 (qualifying) skips the race block and routes to the garage instead.
 *
 * The flip is done inside this DLL (same process), so the write is atomic
 * from the game's perspective — no VirtualProtectEx needed. */
/* THE SESSIONTYPE FLIP IS A RACE AGAINST SessionSynchronizer_0045f950, AND
 * ALWAYS WAS. THAT - NOT THE TIMING - IS WHY THE PITS RETIREMENT BREAKS.
 *
 * Driver reports 2026-08-02, in order: "why is the pit failure script
 * restarting the race"; then, after a first fix, "why is the race not
 * continuing"; then "threw me back into the grid causing an effective race
 * restart. When I was in the pit box, I noticed that the race was restarted."
 *
 * SessionSynchronizer is a session-type CHANGE DETECTOR:
 *
 *     v13 = *v26;                                    // authoritative type
 *     if ( dword_7C56A4 != v13 && v13 < 8 ) {        // our flip IS this
 *         if (!byte_7CE254) sub_4A0250(&unk_7CE24C);
 *         MultiplayerVehiclePlacement(&dword_7C569C, v13);  // RE-GRIDS THE FIELD
 *         GlobalFlagSetter(&unk_7CE24D);             // = byte_7CE25D = 1
 *     }
 *
 * GlobalFlagSetter_004a0da0 is one statement, `byte_7CE25D = 1`, and
 * RestartGate_004a0fd0's own header comment reads "byte_7CE25D -> return 4
 * (mini-restart with cleanup)". The live log caught precisely that: GameState
 * 5 -> 4 at the instant of the retirement. MultiplayerVehiclePlacement is the
 * function that logs "Multiplayer mode: setting vehicle places for RACE" - it
 * is what puts the player back on the grid.
 *
 * So the flip has never been safe. It only ever worked when SessionSynchronizer
 * happened not to tick inside the window, which is why 500 ms was a coin flip
 * and a longer hold made the restart near-certain. Lengthening or shortening
 * the hold only changes the odds; it cannot fix this.
 *
 * TWO DEFENCES, because the flip is still the only known way past
 * sub_4A0250's "POST RACE MONITOR" branch:
 *
 *   1. The window is as short as the gate allows - ConditionalInitGuard_004a0cf0
 *      clears dword_7CE258 back to 0 once it has called sub_4A0250, so
 *      PLAYER_OUT going 1 -> 0 is the EXACT consumption signal. (RestartGate
 *      itself never clears it; that is why the earlier attempt had to watch
 *      GameState instead, which is a weaker proxy.)
 *   2. Both restart flags are held at 0 across the window and a short tail.
 *      Even if the synchronizer fires, RestartGate then finds nothing to
 *      consume, so there is no mini-restart and no re-grid. This is the part
 *      that actually fixes the reported symptom.
 *
 * Anything cleared is logged, so the next run says whether the synchronizer
 * fired at all.
 */
#define RETIRE_HOLD_MAX_MS  900u   /* cap on waiting for PLAYER_OUT to clear */
#define RETIRE_GUARD_MS     2500u  /* how long the restart flags stay pinned */

/* byte_7CE25D: mini-restart with cleanup (RestartGate returns 4).
 * byte_7CE25E: full restart, no cleanup (returns 6).
 * Both are raised by other code during the teardown; neither is ours. */
#define MINI_RESTART_FLAG   (*(volatile BYTE*)0x007CE25D)
#define FULL_RESTART_FLAG   (*(volatile BYTE*)0x007CE25E)

static void retire_player_to_pits(void) {
    int   saved  = SESSION_TYPE;
    int   state0 = GAME_STATE;
    DWORD t0;
    int   ate_mini = 0, ate_full = 0;

    /* Anything already pending before we start is not ours to keep either -
     * it would fire the moment the gate runs. */
    MINI_RESTART_FLAG = 0;
    FULL_RESTART_FLAG = 0;

    if (saved == 7) {
        SESSION_TYPE = 5;
        log_w("RETIRE: SessionType flipped 7->5 for pits path (GameState=%d)", state0);
    }

    retire_player();                    /* writes PLAYER_OUT = 1 */

    /* Phase 1 - wait for the gate. PLAYER_OUT 1 -> 0 is the exact signal:
     * ConditionalInitGuard_004a0cf0 clears it right after calling sub_4A0250.
     * The flip is re-asserted and the restart flags pinned every poll. */
    t0 = GetTickCount();
    for (;;) {
        DWORD dt = GetTickCount() - t0;
        if (saved == 7) SESSION_TYPE = 5;
        if (MINI_RESTART_FLAG) { MINI_RESTART_FLAG = 0; ate_mini++; }
        if (FULL_RESTART_FLAG) { FULL_RESTART_FLAG = 0; ate_full++; }

        if (PLAYER_OUT == 0) {
            log_w("RETIRE: gate consumed PLAYER_OUT after %ums (GameState %d -> %d)",
                  dt, state0, GAME_STATE);
            break;
        }
        if (dt >= RETIRE_HOLD_MAX_MS) {
            log_w("RETIRE: PLAYER_OUT still %d after %ums - continuing anyway "
                  "(GameState %d)", PLAYER_OUT, RETIRE_HOLD_MAX_MS, GAME_STATE);
            break;
        }
        Sleep(5);
    }

    /* SessionType goes back FIRST and immediately: every extra millisecond at
     * 5 is another chance for SessionSynchronizer to see the mismatch, re-grid
     * the field and raise the restart flag. */
    if (saved == 7) {
        SESSION_TYPE = saved;
        log_w("RETIRE: SessionType restored to %d", saved);
    }

    /* Phase 2 - keep eating restart flags for a moment after. The synchronizer
     * can fire on the tick that sees either edge of the flip, and RestartGate
     * only consumes a flag on ITS next tick, so a short tail covers both. */
    t0 = GetTickCount();
    while (GetTickCount() - t0 < RETIRE_GUARD_MS) {
        if (MINI_RESTART_FLAG) { MINI_RESTART_FLAG = 0; ate_mini++; }
        if (FULL_RESTART_FLAG) { FULL_RESTART_FLAG = 0; ate_full++; }
        Sleep(5);
    }

    if (ate_mini || ate_full)
        log_w("RETIRE: swallowed %d mini-restart and %d full-restart request(s) "
              "- SessionSynchronizer fired on the flip", ate_mini, ate_full);
    else
        log_w("RETIRE: no restart was requested (synchronizer never saw the flip)");
}

/* ---------- fire --------------------------------------------------------- */
static void fire(int slot, int eid, float delay);
static void fire_pri(int slot, int eid, float delay, int priority) {
    if (eid < 0) return;
    if (!*COMMENTARY_ON) { log_w("slot %d eid %d skip cm off", slot, eid); return; }
    float cd = *(volatile float*)(EVENT_TABLE + eid * 0x14 + 0x0C);
    if (cd >= FLT_MAX_GUARD) { log_w("slot %d eid %d FLT_MAX skip", slot, eid); return; }

    EVENT_TS(eid) = 0.0f;

    if (priority > 0) {
        DWORD qhead = CONTROLLER + 0x864;
        int table_score = *(volatile int*)(EVENT_TABLE + eid * 0x14 + 0x10);
        *(volatile int*)  (qhead + 0x00) = eid;
        *(volatile int*)  (qhead + 0x04) = table_score;
        *(volatile int*)  (qhead + 0x08) = 0;
        *(volatile float*)(qhead + 0x0C) = -1.0f;
        *(volatile int*)  (qhead + 0x10) = -1;
        *(volatile int*)  (qhead + 0x14) = -1;
        *(volatile int*)  (qhead + 0x18) = -1;
        *(volatile int*)  (qhead + 0x1C) = priority;
        *(volatile float*)(CONTROLLER + 0xAE8) = 0.0f;
        *(volatile int*)  (CONTROLLER + 0x135C) = 1;
        log_w("slot %d eid %d FIRED pri=%d", slot, eid, priority);
    } else {
        float ts_before = EVENT_TS(eid);
        inject_commentary(eid);
        log_w("slot %d eid %d FIRED (ts %.3f -> %.3f)", slot, eid,
              ts_before, EVENT_TS(eid));
    }

    float cur = rd_endt(slot);
    float want = sim_t() + HOLD_LEAD + delay;
    if (want > cur) {
        wr_endt(slot, want);
        log_w("slot %d stopEndTime %.3f->%.3f (+%.1fs)", slot, cur, want, delay);
    }
}

static void fire(int slot, int eid, float delay) {
    fire_pri(slot, eid, delay, 0);
}

/* ---------- fuel manipulation (player slot 0 only) -----------------------
 *
 * Game formula: FuelLevel = fuelAtStart + (simTime - refuelRampStart) * 12.0
 * We manipulate refuelRampStart (veh+0x3480) to change apparent flow rate:
 *   A (slow):  push ramp forward so apparent rate drops to 4 L/s
 *   B (none):  push ramp to +inf so fuel never rises
 *   C (cut):   let flow normally, then push ramp to +inf after ~3L delivered
 *
 * Each variant fires PWS_2001_79 (eid 56) when "detected" during the stop. */
static const char* fuel_var_name(int v) {
    switch (v) {
        case FUEL_VAR_SLOW:   return "SLOW";
        case FUEL_VAR_NOFUEL: return "NONE";
        case FUEL_VAR_CUT:    return "CUT";
        case FUEL_VAR_SPILL:  return "SPILL";
        default:              return "NONE";
    }
}

static int fuel_eid(int variant) {
    (void)variant;
    return EID_P_FUELHOSE;
}

static void apply_fuel_manipulation(void) {
    if (g_fuel_variant == FUEL_VAR_NONE) return;

    float now = sim_t();
    float ramp = rd_ramp(0);
    float fuel_start = rd_fuel_start(0);

    if (ramp > 0 && g_fuel_orig_ramp == 0) {
        g_fuel_orig_ramp = ramp;
        g_fuel_srv_start = now;
        fire(0, EID_P_FUELHOSE, g_fuel_delay);
        g_fuel_resolved_at = sim_t() + g_fuel_delay;
        g_fuel_hud_at = sim_t() + 2.0f;
        g_fuel_fired = 1;
        log_w("FUEL-VAR %s: ramp=%.1f FIRED +%.1fs",
              fuel_var_name(g_fuel_variant), ramp, g_fuel_delay);
    }
    if (g_fuel_orig_ramp <= 0) return;

    float fuel = rd_fuel(0);

    /* trace: who moves ramp/fuelStart while the variant is armed, and does the
     * rate constant we patched still read back as ours? */
    if (now - g_fuel_trace_at >= 0.5f) {
        g_fuel_trace_at = now;
        log_w("FUELTRACE t=%.2f ramp=%.3f (orig %.3f) start=%.2f fuel=%.2f "
              "tgt=%.2f rate=%.2f dfuel=%.2f",
              now, ramp, g_fuel_orig_ramp, fuel_start, fuel, rd_fuel_target(0),
              *(volatile float*)REFUEL_RATE_VA,
              g_fuel_last_level > 0 ? fuel - g_fuel_last_level : 0.0f);
        g_fuel_last_level = fuel;
    }

    switch (g_fuel_variant) {

    case FUEL_VAR_SLOW:
        set_refuel_rate_c(SLOW_RATE);
        break;

    case FUEL_VAR_NOFUEL:
        set_refuel_rate_c(0.0f);
        break;

    case FUEL_VAR_CUT: {
        float risen = fuel - fuel_start;
        if (risen < 3.0f) {
            set_refuel_rate_c(REFUEL_RATE);
        } else if (g_fuel_cut_since == 0) {
            g_fuel_cut_since = now;
            log_w("FUEL-VAR C: cut at +%.1fs (%.1fL delivered)",
                  now - g_fuel_orig_ramp, risen);
            set_refuel_rate_c(0.0f);
        }
        break;
    }

    case FUEL_VAR_SPILL:
        /* fuel flows at normal rate — no rate manipulation needed.
         * spill resolves at g_fuel_resolved_at with target reduction. */
        break;
    }
}

static void reset_fuel_state(void) {
    g_fuel_variant = FUEL_VAR_NONE;
    g_fuel_orig_ramp = 0;
    g_fuel_srv_start = 0;
    g_fuel_cut_since = 0;
    g_fuel_last_level = 0;
    g_fuel_trace_at = 0;
    g_fuel_fired = 0;
    g_fuel_delay = 0;
    g_fuel_resolved_at = 0;
    g_fuel_extends = 0;
    g_fuel_hud_at = 0;
}

/* ---------- main thread -------------------------------------------------- */
static DWORD WINAPI thread_main(LPVOID p) {
    (void)p;
    for (int s = 0; s < SLOTS; s++) {
        g_prev_pitting[s] = -99;
        g_failure_eid[s] = -1;
        g_failure_delay[s] = 0;
    }
    reset_fuel_state();
    rand_seed(GetTickCount());
    if (init_sound_paths()) audio_init(WG_SOUNDS, 3);
    else log_w("AUDIO: host path unavailable or too long; SFX disabled");
    log_w("armed — poll %dms, %d slots", POLL_MS, SLOTS);

    while (g_running) {
        Sleep(POLL_MS);

        if (g_unload_ev && WaitForSingleObject(g_unload_ev, 0) == WAIT_OBJECT_0) {
            log_w("unload signalled — self-ejecting");
            break;
        }
        if (RACE_ACTIVE == 0) continue;

        HANDLE pev = OpenEventA(SYNCHRONIZE, FALSE, PAUSE_EVENT_NAME);
        int paused = (pev && WaitForSingleObject(pev, 0) == WAIT_OBJECT_0);
        if (pev) CloseHandle(pev);

        /* --- player (slot 0) --- */
        if (is_active(0) && !paused) {

            int ps = rd_pit(0);
            int prev = g_prev_pitting[0];
            g_prev_pitting[0] = ps;

            /* --- pit exit commentary (edge-triggered, suppress when DNF active) --- */
            if (!g_dnf_active) {
                if ((prev == 0 || prev == 1) && ps == 2) fire(0, EID_EXIT_LIMITER_ON, 0);
                else if (prev != 3 && ps == 3)            fire(0, EID_PIT_BRAKE, 0);
                else if (prev == 3 && ps == 4)            { fire(0, EID_EXIT_GO, 0); g_last_go_time = sim_t(); }
                else if (prev == 4 && ps == 0 && g_last_go_time > 0 && sim_t() - g_last_go_time < 60.0f) {
                    fire(0, EID_EXIT_LIMITER_OFF, 0);
                }
            }

            /* --- wheel jam: pin the car in the box until the DNF fires ------
             * stopEndTime is re-armed to an ABSOLUTE time every tick, never
             * incremented: if this DLL dies mid-hold the car frees itself
             * within HOLD_LEAD instead of being stranded (the += hazard). */
            if (g_jam_hold) {
                if (sim_t() >= g_jam_dnf_at) {
                    retire_player_to_pits();
                    g_jam_hold = 0;
                    g_dnf_active = 1;
                    log_w("WHEEL JAMMED: DNF executed in box (->pits)");
                } else if (ps == 3) {
                    wr_endt(0, sim_t() + HOLD_LEAD);
                } else {
                    /* car got out anyway — retire immediately rather than let
                     * it drive off with a terminal failure */
                    retire_player_to_pits();
                    g_jam_hold = 0;
                    g_dnf_active = 1;
                    log_w("WHEEL JAMMED: car left box early, DNF executed (->pits)");
                }
            }

            /* entering box — roll failure (or use queued if set by GUI) */
            if (prev != 3 && ps == 3) {
                int fuel_var = FUEL_VAR_NONE;
                int queued = 0;
                g_dnf_active = 0;
                g_jam_hold = 0;     /* fresh stop */
                g_jam_dnf_at = 0;

                /* check GUI queue at veh+0x34A4 */
                int qeid = rd_queue_eid();
                float qdelay = rd_queue_delay();
                if (qeid > 0) {
                    g_failure_eid[0] = qeid;
                    g_failure_delay[0] = qdelay;
                    queued = 1;
                    log_w("QUEUED from GUI: eid=%d +%.1fs", qeid, qdelay);
                    /* clear queue */
                    wr_queue_eid(0);
                    wr_queue_delay(0);

                    /* immediate-retirement eids — fire now, don't wait for box */
                    if (qeid == EID_RETIRE_PITS) {
                        log_w("GUI: immediate retire to pits");
                        retire_player_to_pits();
                        g_failure_eid[0] = -1;
                        continue;  /* skip normal failure flow */
                    }
                    if (qeid == EID_RETIRE_MENU) {
                        log_w("GUI: immediate retire to menu");
                        retire_player();
                        g_failure_eid[0] = -1;
                        continue;
                    }
                }

                if (!queued) {
                    pick_player(0, &g_failure_eid[0], &g_failure_delay[0], &fuel_var);
                } else if (g_failure_eid[0] == EID_P_REFUEL) {
                    /* queued fuel failure — assign random variant */
                    fuel_var = roll_pct() % 4 + 1;
                    g_failure_delay[0] = (fuel_var == FUEL_VAR_SLOW)   ? tri(CFG.d_fuel_slow) :
                                         (fuel_var == FUEL_VAR_NOFUEL) ? tri(CFG.d_fuel_none) :
                                         (fuel_var == FUEL_VAR_CUT)    ? tri(CFG.d_fuel_cut) :
                                                                         tri(CFG.d_fuel_spill);
                } else if (g_failure_eid[0] == EID_P_FUEL_SLOW) {
                    fuel_var = FUEL_VAR_SLOW;
                    g_failure_delay[0] = tri(CFG.d_fuel_slow);
                } else if (g_failure_eid[0] == EID_P_FUEL_NOFUEL) {
                    fuel_var = FUEL_VAR_NOFUEL;
                    g_failure_delay[0] = tri(CFG.d_fuel_none);
                } else if (g_failure_eid[0] == EID_P_FUEL_CUT) {
                    fuel_var = FUEL_VAR_CUT;
                    g_failure_delay[0] = tri(CFG.d_fuel_cut);
                } else if (g_failure_eid[0] == EID_P_FUEL_SPILL) {
                    fuel_var = FUEL_VAR_SPILL;
                    g_failure_delay[0] = tri(CFG.d_fuel_spill);
                }
                if (g_failure_eid[0] >= 0) {
                    if ((g_failure_eid[0] == EID_P_REFUEL ||
                         g_failure_eid[0] == EID_P_FUEL_SLOW ||
                         g_failure_eid[0] == EID_P_FUEL_NOFUEL ||
                         g_failure_eid[0] == EID_P_FUEL_CUT ||
                         g_failure_eid[0] == EID_P_FUEL_SPILL) && fuel_var > 0) {
                        /* deferred fuel failure — manipulate refuelRampStart */
                        reset_fuel_state();
                        g_fuel_variant = fuel_var;
                        g_fuel_delay = g_failure_delay[0];
                        g_failure_eid[0] = -1;
                        log_w("PLAYER enter box — fuel VAR=%s ARMED (+%.1fs)",
                              fuel_var_name(fuel_var), g_fuel_delay);
                    } else if (g_failure_eid[0] == EID_P_WHEELGUN ||
                               g_failure_eid[0] == EID_P_CROSSTHREAD ||
                               g_failure_eid[0] == EID_P_WHEELJAM) {
                        /* one state machine, three shapes: recoverable /
                         * cross-threaded (long, may escalate) / terminal */
                        int cross    = (g_failure_eid[0] == EID_P_CROSSTHREAD);
                        int terminal = (g_failure_eid[0] == EID_P_WHEELJAM);
                        g_wheelgun_armed    = 1;
                        g_wheelgun_corner   = pick_corner();
                        g_wheelgun_terminal = terminal;
                        g_wheelgun_cross    = cross;
                        g_wheelgun_extends  = 0;
                        /* the sampled duration is USED now - it used to be
                         * discarded and replaced by a hardcoded 25.0f */
                        g_wheelgun_delay = g_failure_delay[0]
                                         + corner_penalty(g_wheelgun_corner);
                        g_failure_eid[0] = -1;
                        log_w("PLAYER enter box — %s armed (%s) %.1fs",
                              terminal ? "wheel JAMMED" : cross ? "CROSS-THREAD" : "wheel gun",
                              WG_NAMES[g_wheelgun_corner], g_wheelgun_delay);
                    } else {
                        log_w("PLAYER enter box — failure eid=%d +%.1fs",
                              g_failure_eid[0], g_failure_delay[0]);
                    }
                }
            }

            /* run fuel manipulation + detection while in box */
            if (ps == 3 && rd_svcrdy(0) == 1) {
                /* re-check queue every tick — GUI may queue a fuel variant
                 * mid-stop, after the entry edge has already passed */
                if (g_fuel_variant == FUEL_VAR_NONE) {
                    int qeid = rd_queue_eid();
                    float qdelay = rd_queue_delay();

                    /* immediate retirement — fire now regardless of fuel state */
                    if (qeid == EID_RETIRE_PITS) {
                        wr_queue_eid(0); wr_queue_delay(0);
                        log_w("GUI mid-stop: immediate retire to pits");
                        retire_player_to_pits();
                    } else if (qeid == EID_RETIRE_MENU) {
                        wr_queue_eid(0); wr_queue_delay(0);
                        log_w("GUI mid-stop: immediate retire to menu");
                        retire_player();
                    }

                    int fvar = FUEL_VAR_NONE;
                    if (qeid == EID_P_FUEL_SLOW)   { fvar = FUEL_VAR_SLOW;   qdelay = tri(CFG.d_fuel_slow); }
                    else if (qeid == EID_P_FUEL_NOFUEL) { fvar = FUEL_VAR_NOFUEL; qdelay = tri(CFG.d_fuel_none); }
                    else if (qeid == EID_P_FUEL_CUT)    { fvar = FUEL_VAR_CUT;    qdelay = tri(CFG.d_fuel_cut); }
                    else if (qeid == EID_P_FUEL_SPILL)  { fvar = FUEL_VAR_SPILL;  qdelay = tri(CFG.d_fuel_spill); }
                    else if (qeid == EID_P_REFUEL)       { fvar = roll_pct() % 4 + 1; }
                    if (fvar != FUEL_VAR_NONE) {
                        reset_fuel_state();
                        g_fuel_variant = fvar;
                        g_fuel_delay = qdelay;
                        wr_queue_eid(0);
                        wr_queue_delay(0);
                        log_w("PLAYER mid-stop — fuel VAR=%s ARMED (+%.1fs)",
                              fuel_var_name(fvar), qdelay);
                    }
                }

                apply_fuel_manipulation();

                /* delayed HUD — show after slow rate is visible.
                 * Skip for SPILL: its HUD fires at resolution only. */
                if (g_fuel_hud_at > 0 && sim_t() >= g_fuel_hud_at) {
                    const char* hud = "?";
                    if (g_fuel_variant != FUEL_VAR_SPILL) {
                        hud = (g_fuel_variant == FUEL_VAR_NOFUEL)
                            ? "FUEL PUMP MALFUNCTION" : "FUEL PUMP ISSUE";
                        hud_write(hud);
                    }
                    g_fuel_hud_at = 0;
                    float r = rd_ramp(0);
                    log_w("FUEL HUD: %s  ramp_now=%.3f orig=%.3f fuel=%.1f",
                          hud, r, g_fuel_orig_ramp, rd_fuel(0));
                }

                /* SLOW is a partial blockage, not a stoppage: it degrades the
                 * flow for the WHOLE stop, there is nothing to recover from.
                 * The box lock below holds the car until the tank reaches
                 * target at 4 L/s. NOFUEL and CUT still re-roll — without a
                 * recovery those two never deliver fuel and would strand the
                 * car in the box forever. */
                if (g_fuel_variant == FUEL_VAR_SLOW && g_fuel_resolved_at > 0) {
                    g_fuel_resolved_at = 0;
                    log_w("FUEL: SLOW holds for the whole stop (no recovery roll)");
                }

                /* SPILL resolution: reduce fuel target by spill loss, add extra
                 * time. Audio (FUEL HOSE ISSUE) already fired at ramp detection
                 * when fuel started flowing — don't fire again here. */
                if (g_fuel_variant == FUEL_VAR_SPILL && g_fuel_resolved_at > 0
                    && sim_t() >= g_fuel_resolved_at) {
                    float target = rd_fuel_target(0);
                    float fuel_now = rd_fuel(0);
                    float needed = target - fuel_now;
                    if (needed < 0.5f) needed = 0.5f;
                    float loss = needed * CFG.fuel_spill_pct;
                    float new_target = target - loss;
                    if (new_target <= fuel_now + 0.5f) new_target = fuel_now + 0.5f;

                    *(volatile float*)(VEH_BASE + 0x3484) = new_target;

                    float extra = tri(CFG.d_reattempt);
                    float want = sim_t() + extra;
                    if (want > rd_endt(0)) wr_endt(0, want);
                    hud_write("FUEL SPILL - TARGET REDUCED");

                    log_w("FUEL SPILL resolved: needed=%.1fL lost=%.1fL (%.0f%%) target %.1f->%.1f +%.1fs",
                          needed, loss, CFG.fuel_spill_pct * 100.0f, target, new_target, extra);
                    set_refuel_rate_c(REFUEL_RATE);
                    reset_fuel_state();
                }

                /* fuel hose recovery re-roll */
                if (g_fuel_resolved_at > 0 && sim_t() >= g_fuel_resolved_at) {
                    g_fuel_extends++;
                    if (roll_pct() < CFG.fuel_recover_pct) {
                        log_w("FUEL: hose cleared after %d attempts", g_fuel_extends);
                        set_refuel_rate_c(REFUEL_RATE);
                        /* extend timer to let remaining fuel flow at 12 L/s */
                        float fuel = rd_fuel(0);
                        float target = rd_fuel_target(0);
                        float needed = target - fuel;
                        if (needed > 0) {
                            float extra = needed / REFUEL_RATE + 1.0f;
                            float want = sim_t() + extra;
                            if (want > rd_endt(0)) wr_endt(0, want);
                            log_w("FUEL: recovery needs %.1fL, timer extended +%.1fs", needed, extra);
                        }
                        reset_fuel_state();
                    } else {
                        float more = tri(CFG.d_fuel_reattempt);
                        fire(0, fuel_eid(g_fuel_variant), more);
                        g_fuel_resolved_at = sim_t() + more;
                        log_w("FUEL: hose still stuck, +%.1fs (attempt %d)",
                              more, g_fuel_extends);
                    }
                }

                /* keep box locked while fuel failure active and fuel below
                 * target — stopEndTime was computed at 12 L/s, expires too
                 * early at the slow/stopped rate, causing fuel cap jump.
                 * must hold even before ramp appears, otherwise the stop
                 * ends during the pre-ramp window. */
                if (g_fuel_variant != FUEL_VAR_NONE &&
                    rd_fuel(0) < rd_fuel_target(0)) {
                    float want = sim_t() + 3.0f;
                    if (want > rd_endt(0)) wr_endt(0, want);
                }

                /* tank full at the degraded rate — job done, hand the flow
                 * constant back before the car leaves the box */
                if (g_fuel_variant != FUEL_VAR_NONE && g_fuel_orig_ramp > 0 &&
                    rd_fuel(0) >= rd_fuel_target(0)) {
                    log_w("FUEL: target reached at degraded rate (%.1fL, %.1fs in)",
                          rd_fuel(0), sim_t() - g_fuel_orig_ramp);
                    set_refuel_rate_c(REFUEL_RATE);
                    reset_fuel_state();
                }

                /* detect tyre crew activity + play SFX (every pit stop) */
                float tyre_start = *(volatile float*)(VEH_BASE + OFF_TYRE_START);
                float srv_start = *(volatile float*)(VEH_BASE + 0x346C);
                static float last_tyre_log = 0;
                if (tyre_start != last_tyre_log && tyre_start > 0) {
                    log_w("TYRE CREW: tyre_start=%.1f srv=%.1f sim=%.1f",
                          tyre_start, srv_start, sim_t());
                    last_tyre_log = tyre_start;
                }
                /* Per-corner wheel gun bursts, relative to srv_start
                 * (tyre_start fires ~5s before crew animation begins).
                 * Timing from live observation (trace_pitstop_manual.py):
                 *   crew starts at srv+5.1s, gun OFF at srv+6.2s, ON at srv+9.7s
                 *   RL/RR start ~1.3s earlier than fronts. */
                static const float GUN_OFF[4] = { 7.3f, 7.5f, 6.0f, 6.0f };  /* FL FR RL RR, from srv_start */
                static const float GUN_ON[4]  = { 10.8f, 11.0f, 9.5f, 9.5f };
                if (srv_start > 0) {
                    if (!g_wg_sfx_played) {
                        for (int c = 0; c < 4; c++) {
                            float j = (float)(rand_u32() % 30) * 0.01f;
                            g_burst_off_at[c] = srv_start + GUN_OFF[c] + j;
                            g_burst_on_at[c]  = srv_start + GUN_ON[c]  + j;
                            g_burst_reps[c]   = 0;
                            g_burst_on_done[c] = 0;
                        }
                        log_w("WHEEL GUN SFX: OFF %.1f/%.1f/%.1f/%.1f  ON %.1f/%.1f/%.1f/%.1f",
                              GUN_OFF[0], GUN_OFF[1], GUN_OFF[2], GUN_OFF[3],
                              GUN_ON[0],  GUN_ON[1],  GUN_ON[2],  GUN_ON[3]);
                        g_wg_sfx_played = 1;
                    }

                    int fail_c = (g_wheelgun_fire_at > 0 || g_wheelgun_resolved_at > 0)
                                 ? g_wheelgun_corner : -1;

                    for (int c = 0; c < 4; c++) {
                        /* OFF burst */
                        if (g_burst_off_at[c] > 0 && sim_t() >= g_burst_off_at[c]) {
                            play_burst(c, (int)(rand_u32() % 3));
                            g_burst_reps[c]++;
                            if (c == fail_c && g_burst_reps[c] < CFG.gun_max_repeats)
                                g_burst_off_at[c] = sim_t() + tri(CFG.d_gun_repeat);
                            else
                                g_burst_off_at[c] = 0;
                        }
                        /* ON burst — single shot, no repeats */
                        if (!g_burst_on_done[c] && g_burst_on_at[c] > 0 && sim_t() >= g_burst_on_at[c]) {
                            play_burst(c, (int)(rand_u32() % 3));
                            g_burst_on_done[c] = 1;
                            g_burst_on_at[c] = 0;
                        }
                    }
                } else {
                    g_wg_sfx_played = 0;
                    for (int c = 0; c < 4; c++) { g_burst_off_at[c] = 0; g_burst_on_at[c] = 0; g_burst_reps[c] = 0; g_burst_on_done[c] = 0; }
                }

                /* wheel gun failure: arm timer when tyre anims trigger */
                if (g_wheelgun_armed && srv_start > 0 && tyre_start >= srv_start) {
                    float audio_delay = 7.0f + (float)(roll_pct() % 30) * 0.1f;
                    g_wheelgun_fire_at = sim_t() + audio_delay;
                    g_wheelgun_armed = 0;
                    log_w("WHEEL GUN: tyre anims started at %.1f, fire in %.1fs", tyre_start, audio_delay);
                }

                /* fire audio + extend timer when delay expires */
                if (g_wheelgun_fire_at > 0 && sim_t() >= g_wheelgun_fire_at) {
                    float extra = g_wheelgun_delay;
                    int c = g_wheelgun_corner;
                    log_w("WHEEL GUN: firing %s slow=%d +%.1fs", WG_NAMES[c], WG_SLOW[c], extra);
                    fire(0, WG_SLOW[c], extra);
                    hud_write(WG_FAIL[c]);
                    /* the "wheel jammed" call only makes sense once the stop is
                     * already visibly long - skip it on short failures */
                    g_wheelgun_jammed_at   = (extra >= 6.0f) ? sim_t() + 4.0f : 0.0f;
                    g_wheelgun_resolved_at = sim_t() + extra - 3.0f;
                    g_wheelgun_fire_at = 0;
                }

                /* "wheel jammed" clip 4s after primary */
                if (g_wheelgun_jammed_at > 0 && sim_t() >= g_wheelgun_jammed_at) {
                    fire(0, EID_WHEEL_JAMMED, 0);
                    log_w("WHEEL GUN: jammed clip fired");
                    g_wheelgun_jammed_at = 0;
                }

                /* resolved: terminal DNF or "X clear" */
                if (g_wheelgun_resolved_at > 0 && sim_t() >= g_wheelgun_resolved_at) {
                    int c = g_wheelgun_corner;
                    int reextended = 0;

                    /* a recoverable failure does not always clear first try -
                     * this is what produces the rare catastrophic stop without
                     * needing a separate failure type for it */
                    if (!g_wheelgun_terminal && roll_pct() >= CFG.recover_pct) {
                        g_wheelgun_extends++;

                        /* a cross-thread that has resisted twice can turn
                         * terminal, which ties the DNF into the failure tree
                         * instead of it being an unrelated coin flip */
                        if (g_wheelgun_cross && g_wheelgun_extends >= 2 &&
                            roll_pct() < CFG.escalate_pct) {
                            g_wheelgun_terminal = 1;
                            log_w("WHEEL GUN: %s escalated to terminal after %d extends",
                                  WG_NAMES[c], g_wheelgun_extends);
                        } else {
                            float more = tri(CFG.d_reattempt) + corner_penalty(c);
                            fire(0, EID_WHEEL_JAMMED, more);
                            hud_write(WG_FAIL[c]);
                            g_wheelgun_resolved_at = sim_t() + more;
                            reextended = 1;
                            log_w("WHEEL GUN: %s still stuck, +%.1fs (extend %d)",
                                  WG_NAMES[c], more, g_wheelgun_extends);
                        }
                    }

                    if (reextended) {
                        /* nothing further this tick */
                    } else if (g_wheelgun_terminal) {
                        float dnf_in = tri(CFG.d_jam_dnf);
                        fire(0, EID_RETIRE, 0);
                        hud_write("WHEEL JAMMED - RETIRE");
                        /* the car is NOT released — it stays in the box and
                         * retires there. See the hold block in the tick. */
                        g_jam_hold = 1;
                        g_jam_dnf_at = sim_t() + dnf_in;
                        log_w("WHEEL JAMMED: car pinned in box, DNF in %.1fs", dnf_in);
                        g_wheelgun_resolved_at = 0;
                    } else {
                        fire(0, WG_OK[c], 0);
                        hud_write(WG_FIXED[c]);
                        g_wheelgun_resolved_at = 0;
                    }
                }
            }

            /* fire non-fuel, non-wheelgun failures once service is running */
            if (ps == 3 && rd_svcrdy(0) == 1 && g_failure_eid[0] >= 0) {
                fire(0, g_failure_eid[0], g_failure_delay[0]);
                g_failure_eid[0] = -1;
            }

            /* clear on box exit */
            if (ps != 3) {
                if (g_failure_eid[0] >= 0) {
                    g_failure_eid[0] = -1;
                    g_failure_delay[0] = 0;
                }
                /* the in-box branch cannot reset this - it only runs while
                 * ps==3, so without this the bursts are scheduled once ever */
                g_wg_sfx_played = 0;
                for (int c = 0; c < 4; c++) { g_burst_off_at[c] = 0; g_burst_on_at[c] = 0; g_burst_reps[c] = 0; g_burst_on_done[c] = 0; }

                if (g_wheelgun_armed) {
                    log_w("WHEEL GUN: box exit before tyre anims — cleared");
                    g_wheelgun_armed = 0;
                }
                if (g_wheelgun_fire_at > 0) {
                    log_w("WHEEL GUN: box exit before audio fired — cleared");
                    g_wheelgun_fire_at = 0;
                }
                if (g_wheelgun_jammed_at > 0) {
                    g_wheelgun_jammed_at = 0;
                }
                if (g_wheelgun_resolved_at > 0) {
                    g_wheelgun_resolved_at = 0;
                }
                if (g_dnf_active) {
                    log_w("WHEEL JAMMED: box exit — clearing DNF state");
                    g_dnf_active = 0;
                }
                if (g_fuel_variant != FUEL_VAR_NONE && !g_fuel_fired) {
                    log_w("FUEL: box exit before detection — clearing variant %s",
                          fuel_var_name(g_fuel_variant));
                    set_refuel_rate_c(REFUEL_RATE);
                    reset_fuel_state();
                }
                /* always clear recovery on exit — a box release ends the stop */
                if (g_fuel_resolved_at > 0) {
                    log_w("FUEL: box exit during recovery — cleared");
                    set_refuel_rate_c(REFUEL_RATE);
                    reset_fuel_state();
                }
            }
        }

        /* --- AI (slots 1..103) --- */
        if (!paused) {
        for (int s = 1; s < SLOTS; s++) {
            if (!is_active(s)) continue;
            int ps = rd_pit(s);
            int prev = g_prev_pitting[s];
            g_prev_pitting[s] = ps;
            if (prev != 3 && ps == 3) {
                pick_ai(s, &g_failure_eid[s], &g_failure_delay[s]);
                if (g_failure_eid[s] >= 0)
                    log_w("AI slot %d enter box — failure eid=%d +%.1fs",
                          s, g_failure_eid[s], g_failure_delay[s]);
            }
            if (ps == 3 && rd_svcrdy(s) == 1 && g_failure_eid[s] >= 0) {
                fire(s, g_failure_eid[s], g_failure_delay[s]);
                g_failure_eid[s] = -1;
            }
            if (ps != 3 && g_failure_eid[s] >= 0) {
                g_failure_eid[s] = -1;
                g_failure_delay[s] = 0;
            }
        }
        } /* !paused */
    }

    set_refuel_rate(REFUEL_RATE);  /* never leave the global flow constant patched */
    audio_shutdown();   /* voices must be closed before the module goes away */
    if (g_unload_ev) { CloseHandle(g_unload_ev); g_unload_ev = NULL; }
    FreeLibraryAndExitThread(g_hinst, 0);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hinst = hinst;
        DisableThreadLibraryCalls(hinst);
        GetModuleFileNameA(NULL, g_log, MAX_PATH);
        char* slash = strrchr(g_log, '\\');
        if (slash) strcpy(slash + 1, "pit_commentary_log.txt");
        else strcpy(g_log, "pit_commentary_log.txt");
        load_cfg();
        /* NOTE: audio_init() must NOT be called here. waveOutOpen pulls in the
         * audio driver DLLs, and doing that under the loader lock deadlocks
         * DllMain - it hung the injecting thread on first try. It runs at the
         * top of thread_main instead. */
        g_unload_ev = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT_NAME);
        CreateThread(NULL, 0, thread_main, NULL, 0, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = FALSE;
    }
    return TRUE;
}
