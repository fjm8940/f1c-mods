/*
 * engine_failure.c - manual hotkey activation of PLAYER mechanical failures
 *
 * WHY THE AI MECHANISM DOES NOT WORK FOR THE PLAYER
 * -------------------------------------------------
 * The .PLR key "Mechanical failure rate (0-none, 1-normal, 2-timescaled,
 * 3-season)" lands on dword_7CEA5C. That global drives a four-slot countdown
 * array at engine+0x3D64 (veh+0x3D70/74/78/7C = engine/brakes/gearbox/
 * suspension), armed by EngineWearFailureSimulation_004ee150 from the .hdv
 * [ENGINE] LifetimeAvg/LifetimeVar and ticked by FuelConsumptionAndWear
 * 0x4ED960.
 *
 * That array is AI-ONLY (live-proven 2026-07-30, this session):
 *   Frida Interceptor on 0x4ED960, 5 s sample -> 105 calls, 21 distinct `this`,
 *   all of them 0x5EC6E4 + 0x48C0*n for n >= 1. ZERO calls for slot 0.
 *   Writing veh+0x3D70 = 3.0 on the player LATCHED and did nothing for 12 s.
 * Same shape as the vanilla stuck-gear byte veh+0x39FA (also AI-only).
 *
 * THE REAL PLAYER SITE (live-proven this session)
 * -----------------------------------------------
 *   EngineStressSimulation_0043ef20, this = 0x7C7428 (static player physics
 *   object - the same `this` as the player's SetGear in gear_failure.c).
 *
 *     this+7604 = 0x7C91DC  double  engine life, seconds, ticks DOWN
 *     this+7596 = 0x7C91D4  double  stress rate, 0.0 .. 0.6 (hard cap)
 *     this+7652 = 0x7C920C  double  bank 1 health   (its +4 is the high dword)
 *     this+7660 = 0x7C9214  double  bank 2 health
 *     this+7668 = 0x7C921C  double  bank 3 health
 *
 *   life > 0 : decrements by (2.0 + stress) * dt
 *   life <= 0: writes ReasonCode 1 (Engine) to vehicle+16488 (veh+0x4068),
 *              rolls a damage bank, re-arms life for a STAGED death, and
 *              calls sub_41AC50(vehicle, 0) - the commentary emitter. The
 *              race-engineer line therefore comes free, no unblocking needed.
 *   life < -10.1 : game halves banks 1+2 and clears bank 3 (partial recovery)
 *   life < -15.4 : stress ramps +0.002*dt up to the 0.6 cap. This is the
 *                  terminal region - stress is what actually kills the engine.
 *
 * DRIVER-CONFIRMED 2026-07-30:
 *   life = 1.5  -> ReasonCode 0 -> 1, life re-armed to 168.06, stress 0 ->
 *                  0.111, bank1 -0.3248. Driver: "engine is blowing white
 *                  smoke, still running normally". Stage one, as designed.
 *   life = -16.0 -> stress ramped 0.1113 -> 0.1483 -> 0.4268, climbing toward
 *                  0.6 on its own. ~100 s to full death.
 *
 * IS THERE A POWER DEFICIT? YES - and smoke and power are SEPARATE channels.
 * That is exactly why stage one looked like "smoke but running normally".
 *
 *   STRESS drives POWER. Gearbox_00443b70, end of the engine torque update:
 *       if (this+7596 > 0.0) {
 *           v33 = 11.5 / this[7388] * dbl_8AFEA0 * rpm(this+7260);
 *           v36 = interpolated sample of the noise table flt_8AAE74 at v33;
 *           this[8] = (1.0 - v36 * stress * v36) * this[8];
 *       }
 *   this+8 (= 0x7C7430) is ENGINE TORQUE. So torque *= (1 - v36^2 * stress).
 *   v36 is a noise sample phased by ENGINE RPM, so the loss is a stuttering,
 *   rev-dependent misfire, NOT a flat power cut. stress 0.6 = up to 60% of
 *   torque gone at the peak of the noise. At the 0.111 of stage one the cut
 *   is a few percent - imperceptible, which matches the driver report.
 *
 *   BANKS drive SMOKE. TireSlipForceCalculation_004472b0 (misnamed; it is the
 *   vehicle effects pass) skips the emitter entirely when all three banks are
 *   0.0, otherwise computes per-bank emission into a2[38]/a2[43]/a2[48] and
 *   calls ParticleCollisionEffect. Load term:
 *       v19 = clamp(-1/this[7388] * rpm * torque, -1.0, 0.0)
 *       bank <= 0 ? v19 * bank : bank
 *   So a NEGATIVE bank produces a positive, engine-load-scaled plume. Our
 *   bank1 of -0.3248 is precisely the white smoke that was seen.
 *
 * HOW THIS DIFFERS FROM damage.ini
 *   F1C has NO "EngineSeizure" token - that is the rFactor/GTR name, and it
 *   appears nowhere in strings_index.json. The F1C equivalent is
 *   SeasonData/damage.ini [PHYSICAL] Engine=10000.0 "Impulse to seize engine",
 *   parsed by sub_565870 into offset 0xC of the damage-params object.
 *   Two unrelated systems:
 *     damage.ini Engine=   COLLISION-impulse threshold -> instant seizure.
 *                          One crash, one binary outcome, no progression.
 *     life/stress (here)   WEAR countdown from .hdv LifetimeAvg/LifetimeVar
 *                          -> staged death: smoke, then misfire, then stop.
 *   Editing damage.ini does not affect anything this DLL touches.
 *
 * DESIGN RULE: arm the trigger, never forge the end state. Every command
 * writes a value the game itself tests, then lets the game run its own
 * failure path - which is why the smoke, the damage roll and the radio line
 * all appear without this DLL implementing any of them.
 *
 * CONTROL (GUI only; in-game hotkeys removed 2026-09-27)
 *   Shared memory  F1C_ENGFAIL_CTL   <- tools/mech_failure_test_gui.py
 *     (Engine tab). Command fields are edge-triggered: the GUI sets cmd_*,
 *     the worker clears it after acting. Status fields are republished every
 *     poll: life/stress/torque/rpm/banks as floats, running/reason/phase/
 *     session as ints. The GUI arms carry the same session gate as hotkeys.
 *   Self-eject: event F1C_ENGFAIL_UNLOAD (python tools/dll_unload_signal.py
 *   F1C_ENGFAIL_UNLOAD). NEVER FreeLibrary this DLL from outside - the worker
 *   thread would be freed under itself (crashed the game 2026-07-28).
 *
 * BUILD
 *   C:\msys64\mingw32\bin\gcc.exe -shared -O2 -o engine_failure.dll engine_failure.c -lkernel32
 */

#include <windows.h>
#include <math.h>
#include <mmsystem.h>
#include "mf_exit.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "mf_variance.h"
#include "mf_hud.h"
#include "mf_msg.h"            /* 2026-10-01: HUD/radio arbiter (messaging audit) */
#include "mf_voice.h"          /* V3: engineer voice-line mapping (mf_voice.h) */
#include "mf_engine_model.h"   /* E2: engine sim model v2, shadow mode below */
#include "mf_notify.h"         /* N2 2026-10-06: pure notification triggers */
#include "mf_ai_react.h"       /* B1 (ADDENDUM 14): AI symptoms + reactions */
#include "mf_ai_pulloff.h"     /* P2 (ADDENDUM 15): AI pull-off pure layer */
#include "f1c_surface_contact.h" /* P4: per-wheel material chain (SDK)     */
#include "mf_compat.h"         /* E9-H1a: aggregate compatibility gate */
#include "mf_coop.h"           /* 2026-10-10 CO-OP compatibility (bridge + local player) */

/* ---- 2026-10-10 CO-OP compatibility (docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md)
 * MF AI index i (1..MF_AI_MAX-1) addresses vehicle slot mf_vslot(i); index 0
 * is the player. Single-player (no CO-OP bridge or no CO-OP session): the
 * map is never latched and mf_vslot(i) == i - exactly the old
 * VEH_BASE + i x stride addressing. Inside a CO-OP session (latched in
 * mfc_engine_pump, mf_coop_engine.h) index -> local AI car (marker 1; host
 * bots live in vehicle slots 32-54), index 0 -> the local human car. */
#define MFC_SLOT_UNMAPPED (MFC_VEH_SLOTS - 1)
static volatile LONG g_mfc_latched = 0;
static volatile LONG g_mfc_map[MF_AI_MAX];      /* index -> vslot, 0 = none */
static volatile LONG g_mfc_rev[MFC_VEH_SLOTS];  /* vslot -> index, 0 = none */
static int mf_vslot(int i)
{
    LONG v;
    if (!g_mfc_latched) return i;
    if (i == 0) return mfc_player_vslot();
    if (i < 0 || i >= MF_AI_MAX) return MFC_SLOT_UNMAPPED;
    v = g_mfc_map[i];
    return v > 0 ? (int)v : MFC_SLOT_UNMAPPED;
}
/* vehicle slot -> MF index (-1 = not ours); identity in single-player */
static int mf_islot(int vslot)
{
    if (!g_mfc_latched) return vslot;
    if (vslot < 0 || vslot >= MFC_VEH_SLOTS) return -1;
    if (vslot == mfc_player_vslot()) return 0;
    return g_mfc_rev[vslot] > 0 ? (int)g_mfc_rev[vslot] : -1;
}
#define MF_AIV(i) ((DWORD)(VEH_BASE + AI_VEH_STRIDE * (DWORD)mf_vslot((int)(i))))
#define PLR_VEH   ((DWORD)mfc_player_base())   /* the car this PC drives */
/* emit hooks + session hook (mf_coop_engine.h) */
static void mfc_on_burst(int id, int n, float rate, float spread, float up, int slot);
static void mfc_on_blow(int owner, int on);
static void mfc_on_sound(int owner, int kind, const char *path, float gain);
static void mfc_session_reset(void);
static volatile LONG g_mfc_rq_n = 0;            /* CO-OP remote burst queue fill */
static void mfc_rq_drain(unsigned gen);
static int g_mfc_sync = 1;          /* INI [CoOp] SyncFailures */
static int g_mfc_sync_retire = 1;   /* INI [CoOp] SyncRetire   */
#include "f1c_broker.h"       /* audit MF-09: game-thread commentary posts (SDK) */
#include "f1c_race_control.h" /* Batch C: publish MF's "car limited" record for the AI Toolkit */
#include "mf_patch8.h"
#include "mf_thread_gate.h"
#include "mf_impact_sample.h"
#include "mf_lamps.h"          /* L1 2026-10-05: native lamp mapping (pure) */

/* E9g item 5 (addendum 1): session-end guard. game_ms() (defined below)
 * already returns 0 when SESSION_TIME is not a valid session clock
 * (<= 0, or > 4.0e6 after the session-end jump), so it IS the existing test
 * the addendum asked for. While it holds, every pump returns immediately
 * and writes no game memory (including restores). */
static DWORD game_ms(void);
#define SESSION_OVER() (game_ms() == 0)

/* E4: legacy percentage fault maths, one switch per family (handoff E4).
 * 0 (default) = model state and MfOut drive the fault; 1 = the old maths
 * (kept compiled, owner-test fallback only). */
#define MF_LEGACY_VALVE    0
#define MF_LEGACY_OILP     0      /* lubrication: oil pressure + oil leak */
#define MF_LEGACY_COOLING  0      /* debris + water leak */
#define MF_LEGACY_FUEL     0      /* fuel pressure + fuel leak */
#define MF_LEGACY_BLOWBY   0
#define MF_LEGACY_EXHAUST  0
#define MF_LEGACY_OVERREV  0

/* ---- engine sim model v2 state (E2; moved up in E4 so the fault pumps and
 * arm functions can use it). The model itself is pure C in
 * dll/mf_engine_model.h (handoff section 3). ----------------------------- */
static MfCar    g_model_car;
static MfEngine g_model_eng;
static MfInputs g_model_in;
static MfOut    g_model_out;
static int      g_model_stepped = 0;  /* R1a: 1 = mf_model_step ran this pass */
static int      g_model_live = 0;
static DWORD    g_model_last_ms = 0;
static DWORD    g_model_log_ms = 0;
static int      g_model_terminal_done = 0;
static int      g_model_culm_done = 0;          /* E8g item 5: clock-out shown  */
static void     model_manual_blow(void);        /* E8g item 6: defined below    */
static DWORD    g_model_stop_plume_until = 0;
static int      g_model_stop_blue = 0;
static DWORD    g_model_puff_next = 0;
static int      g_model_car_valid = 0;          /* E8e: raw car values valid   */
static double   g_model_car_base_snapshot = 0.0;/* E8e: last derived rpm base  */
static double   g_model_car_oil_snapshot = 0.0; /* H3a: last derived oil base  */
static int      g_model_stage1_logged = 0;      /* E9a: native stage-one logged */
static unsigned g_model_liferesets_seen = 0;    /* E9g-c: resets logged        */
static int      g_niggle_prev_kind = 0;         /* E9b: DLL-side niggle mapping */
static int      g_early_logged = 0;             /* E9b-2: early failure logged */
static int      g_cascade_seen = 0;             /* E9b-2: first cascade logged */
static int      g_emergency_logged = 0;         /* E9-H2b: emergency onset logged */
static double   g_dmg_log_t = 0.0;              /* P1: last player DMG log (model s) */
static int      g_shadow_logged = 0;            /* P2: player shadow fire logged */
static int      g_player_onset_logged = 0;      /* Q3: player onset path logged */

/* ---- E9-H1c: session generation ----------------------------------------
 * Bumped on every session/stint invalidation (session change, SESSION_OVER,
 * clock rollback, non-race stint end). Every saved lever/FX/AI-table restore
 * record stores the generation it was taken in; a restore whose generation
 * does not match the current one is DROPPED (log once), never written into a
 * newer session/engine. Queued particle bursts carry it too; the hook drops
 * stale ones. The hook thread only reads it, so a plain volatile is enough. */
static volatile unsigned g_session_gen = 1;

static unsigned session_gen_bump(void)
{
    unsigned g = g_session_gen + 1u;
    g_session_gen = g;
    return g;
}

static void model_reset(uint32_t seed)
{
    mf_engine_init(&g_model_eng, seed);
    g_model_terminal_done = 0;
    g_model_culm_done = 0;
    g_model_stop_plume_until = 0;
    g_model_stop_blue = 0;
    g_model_puff_next = 0;
    g_model_stage1_logged = 0;
    g_model_liferesets_seen = 0;
    g_niggle_prev_kind = 0;
    g_early_logged = 0;
    g_cascade_seen = 0;
    g_emergency_logged = 0;
    g_player_onset_logged = 0;
}

/* E4a: the model does not expose the two pressures, so the diagnostics
 * field and the pump HUDs reuse the model's own expressions (lubrication
 * case 4.2 / valve return force 4.1). Diagnostic only - the game levers never
 * take these values. */
static double model_oil_pressure(void)
{
    double rev = g_model_car.rev_limit_rads > 1.0
               ? g_model_in.rpm_rads / g_model_car.rev_limit_rads : 0.0;
    double g_comb = sqrt(g_model_in.g_long * g_model_in.g_long
                         + g_model_in.g_lat * g_model_in.g_lat);
    /* E9g-g item 6 (E9f follow-up): the diagnostic must evaluate the oil at
     * the SAME effective bearing temperature as the model film
     * (MF_FILM_TEMP_GAIN path), not at the raw, physical t_brg */
    double t_visc = g_model_in.oil_k
                  + MF_FILM_TEMP_GAIN * (g_model_eng.t_brg_k - g_model_in.oil_k);
    double mu = mf_mu(t_visc, &g_model_car);
    double pickup = g_comb >= 12.0 * (g_model_eng.v_oil_l / MF_OIL_START_L)
                  ? 0.3 : 1.0;
    double p = g_model_eng.pump_eta * (rev < 0.55 ? rev / 0.55 : 1.0)
               * sqrt(mu > 0.0 ? mu : 0.0) * pickup;
    return p > 1.0 ? 1.0 : p;                 /* relief valve */
}

static double model_valve_pressure(void)
{
    double t_bay = 0.5 * (g_model_in.oil_k + g_model_in.water_k);
    return g_model_eng.valve_m * (t_bay / MF_T_BAY_REF_K);
}


/* per-failure variance (owner 2026-09-29), see mf_variance.h. The legacy
 * (MF_LEGACY_*) families keep theirs; the model families do not use it. */
#if MF_LEGACY_OILP
static mfv_t g_v_oil, g_v_oilp;
#endif
#if MF_LEGACY_COOLING
static mfv_t g_v_cool;
#endif
#if MF_LEGACY_FUEL
static mfv_t g_v_fp, g_v_fl;
#endif
#if MF_LEGACY_BLOWBY
static mfv_t g_v_bb;
#endif
#if MF_LEGACY_EXHAUST
static mfv_t g_v_exh;
#endif
static mfv_t g_v_heat;

static void ev_pub(const char *name, mfv_t *v)
{
    char d[64];
    mfv_desc(v, d, sizeof d);
    mfv_publish("engine", "%s: %s", name, d);
}

static void logf_(const char *fmt, ...);   /* defined below, used earlier */
static int  session_live(void);            /* defined below, used earlier */
static int  rand_range(int lo, int hi);    /* defined below, used earlier */
static int  live_drive(void);              /* E9g-c: defined below, used earlier */
static void engine_invalidate(const char *why, int drop); /* H3b: used earlier */
static void engine_player_reset(const char *why);        /* audit MF-02: below */
static void radio_req(int eid, int prio);  /* 2026-10-01: radio queue, below */
static int commentary_say(int eid);       /* audit MF-09: below (broker post) */
static int on_circuit(void);                /* 2026-10-07: radio containment, below */
static void voice_play_wav(int line);       /* 2026-10-07: loose engineer WAV */
static int mf_sound_path(char *path, const char *file);   /* below */
static int voice_native_install(void);     /* 2026-10-07: game-native WAV path, below */
static int voice_native_say(int line);     /* below */
static void voice_native_remove(void);     /* below */
static volatile int g_engine_dead;
static void voice_instr_clear_all(void);   /* V3: instruction sub-slots, below */
static void voice_pre(int id, int cond, DWORD min_ms, DWORD max_ms,
                      int line, int fb_eid, int arm_pair);   /* V3 */
static void voice_crit(int id, int cond, DWORD min_ms, DWORD max_ms,
                       int line, int fb_eid);                /* V3 */
static void voice_instr(int id, int cond, DWORD min_ms, DWORD max_ms,
                        int line);                           /* V3 */
static void voice_say(int line, int prio);                   /* V3 */
static float rev_frac(void);               /* R3k: smoke drive, used earlier */
static const char *dmg_fault_name_(int f); /* R3l: dead status, used earlier */

/* ---- player physics object (FACT) ------------------------------------- */
#define PLAYER_PHYS_OBJ  0x007C7428u

#define ENG_LIFE         (*(volatile double*)0x007C91DCu)  /* this+7604 */
#define ENG_STRESS       (*(volatile double*)0x007C91D4u)  /* this+7596 */
#define ENG_BANK1        (*(volatile double*)0x007C920Cu)  /* this+7652 */
#define ENG_BANK2        (*(volatile double*)0x007C9214u)  /* this+7660 */
#define ENG_BANK3        (*(volatile double*)0x007C921Cu)  /* this+7668 */

/* LifetimeEngineRPM / LifetimeOilTemp, read by the E2 shadow model
 * (handoff section 2, FACT P0: values compare with the car's engine.ini) */
#define LT_RPM_BASE      (*(volatile double*)0x007C91E4u)  /* rad/s   */
#define LT_RPM_INVR      (*(volatile double*)0x007C91ECu)  /* 1/range */
#define LT_OIL_BASE      (*(volatile double*)0x007C91F4u)  /* K       */
#define LT_OIL_INVR      (*(volatile double*)0x007C91FCu)  /* 1/range */
#define AMBIENT_C        (*(volatile float*)0x008AFEF0u)   /* ambient air, C (E2) */

/* RPMTorque table (DERIVED, master plan section 2 / E9a): ptr + entry count;
 * entry i covers i x MF_TQ_STEP_RADS rad/s, float pair { coast, delta },
 * power torque = coast + delta (sub_43C250). Read once per car derive. */
#define TQ_TABLE_PTR     (*(volatile uint32_t*)0x007C9124u)
#define TQ_TABLE_COUNT   (*(volatile int*)0x007C9130u)
#define MF_TQ_STEP_RADS  32.0

/* E9e (addendum 2 sections 2-3): the remaining engine.ini values, read once
 * per car derive and range-checked with relative fallbacks in mf_car_derive.
 * All DERIVED (this = 0x7C7428). Offsets from docs/46 (this+0x1Dxx block). */
#define ENG_INERTIA      (*(volatile double*)0x007C9094u)  /* this+7276, EngineInertia */
#define IDLE_RPM_LO      (*(volatile double*)0x007C90ACu)  /* this+7300, IdleRPMLogic lo */
#define IDLE_RPM_HI      (*(volatile double*)0x007C90B4u)  /* this+7308, IdleRPMLogic hi */
#define FUEL_CONS        (*(volatile double*)0x007C75D4u)  /* FuelConsumption */
#define FUEL_MULT        (*(volatile double*)0x007C75DCu)  /* FuelConsumption multi */
#define OWT_K0           (*(volatile double*)0x007C91A4u)  /* this+7548, OilWaterHeatTransfer */
#define OWT_K1           (*(volatile double*)0x007C91ACu)  /* this+7556, per rad/s */

/* E9e levers (DERIVED addresses, written only through the MfLever helper):
 * CombustionHeat (lean running), OilMinimumCooling (oil leak), IdleThrottle
 * (rough idle). Range checks and stock logs live in the hold functions. */
#define COMB_HEAT        (*(volatile double*)0x007C9184u)  /* this+7516, CombustionHeat */
#define OIL_MIN_COOL     (*(volatile double*)0x007C9194u)  /* this+7532, OilMinimumCooling */
#define IDLE_THROTTLE    (*(volatile double*)0x007C90A4u)  /* this+7292, IdleThrottle */
/* E9d: the game's own engine-braking map (FACT docs/47) and the cockpit oil
 * pressure gauge (derived, kPa). */
#define ENG_BRAKE_MAP    (*(volatile double*)0x007C9078u)  /* this+7248 */
#define OIL_PRESS_NATIVE (*(volatile double*)0x007C919Cu)  /* this+7540, gauge 3, kPa */
/* the game's 256-float noise table (flt_8AAE74), read-only, used by the
 * native rough-running/misfire pattern (finding native-rough-running-...) */
#define MF_NOISE_TABLE   ((const float*)0x008AAE74u)

#define ENG_STRESS_CAP   0.6      /* game's own clamp in 0x43EF20          */
#define ENG_TERMINAL     (-16.0)  /* below the -15.4 stress-ramp threshold */
#define ENG_STAGE_ONE    1.5      /* expires in ~1.5 s -> smoke + radio    */

/* ---- per-wheel records (FACT, docs/SUSPENSION_DAMAGE.md) --------------- */
#define WHEEL_BASE       0x007C7428u
#define WHEEL_STRIDE     1124
#define W_BENT_FLAG      1105     /* u8   */
#define W_BEND_A         1116     /* float, rad, clamped +/-10 deg */
#define W_BEND_B         1124     /* float */
#define W_ACCUM          1132     /* float, > 0.015 -> ReasonCode 4 */
#define SUS_TRIP         0.020f   /* just past the trip point */

#define WHEEL_PTR(i)     (WHEEL_BASE + (DWORD)(WHEEL_STRIDE) * (DWORD)(i))
#define W_U8(i, off)     (*(volatile unsigned char*)(WHEEL_PTR(i) + (off)))
#define W_F32(i, off)    (*(volatile float*)(WHEEL_PTR(i) + (off)))

/* ---- vehicle slot 0 ---------------------------------------------------- */
#define VEH_BASE         0x005EC6D8u
#define OFF_REASON       0x4068u
#define REASON_CODE      (*(volatile int*)(PLR_VEH + OFF_REASON))
#define OFF_POS_X        0x34E0u
#define OFF_POS_Y        0x34E4u
#define OFF_POS_Z        0x34E8u
#define OFF_FINISH       0x4064u
#define OFF_SPEED        0x4440u   /* FACT, m/s - dll/README.md retire_commentary.c */
/* E9e (addendum 2 section 7): the player vehicle block also carries the local
 * acceleration vector (FACT, finding player-live-telemetry-f2002): lateral
 * veh+0x354C, longitudinal veh+0x3554 (m/s2, forward = negative). Replaces
 * the speed-derived g_long and g_lat = 0 in model_inputs. */
#define OFF_G_LAT        0x354Cu
#define OFF_G_LONG       0x3554u
#define FINISH_STATE     (*(volatile int*)(PLR_VEH + OFF_FINISH))
#define VEH_SPEED        (*(volatile float*)(PLR_VEH + OFF_SPEED))

/* 2026-10-07 audit MF-12 (E2, SDK f1c_live_replay.h): 2 = instant-replay
 * playback, the game skips its main update */
#define REPLAY_MODE (*(volatile int*)0x008AB6E4u)

/* ---- SpecialFX reaction table + ParticleTrailEmitter (FACT, live
 * 2026-08-07, see mods/extended_vanilla_features/patches/mechanical_failures
 * /dll/bluesmoke.c for the full derivation and findings.jsonl
 * bluesmoke-reaction-table-live-confirmed / particletrailemitter-calling-
 * convention-confirmed). Duplicated here rather than shared so this DLL
 * stays self-contained (project convention: DLLs share no hook site or
 * state). Never cache the array base across calls - it moves on a
 * track/session reload. */
#define SPECIALFX_BASE   0x008ACEA0u
#define SFX_COUNT        (*(volatile DWORD*)(SPECIALFX_BASE + 16))
#define SFX_ARRAY_BASE   (*(volatile DWORD*)(SPECIALFX_BASE + 20))
#define SFX_STRIDE       108
#define SFX_ID_OFF       4
#define REACTION_ID_BLUESMOKE 2
#define SESSION_TIME     (*(volatile double*)0x008AFEA0u)  /* dbl_8AFEA0 */

typedef void (__fastcall *ParticleTrailEmitter_t)(
    void *thisPtr, void *edx_unused,
    float simTime, float *pos, float *vel, float rate,
    int a6, char a7, int a8);
#define ParticleTrailEmitter ((ParticleTrailEmitter_t)0x004CBC50u)

/* ---- commentary injection, ported from tools/commentary_inject.py -------
 * Pure memory writes into the game's own commentary queue - no engine call,
 * matches the project's preferred pattern. eid 130 = PWS_MECHANWARN0
 * ("pit on this laps, the marshals are indicating you have a mechanical
 * problem"), isolation-tested and driver-confirmed 2026-08-07 (findings.jsonl
 * pending). NOTE eid 131 is a DIFFERENT clip (PWS_RED01, red flag) - do not
 * reuse that eid for anything mechanical-failure related. */
#define CM_CONTROLLER    0x005EA618u
#define CM_EVENT_TABLE   0x005BF9A0u
#define CM_QHEAD         (CM_CONTROLLER + 0x864u)
#define CM_TS_ARR        (CM_CONTROLLER + 0xAF8u)
#define EID_MECHWARN     130
#define EID_PITCALL      49   /* PWS_PIT01 "pit this lap" - the critical call */

/* 2026-10-01 messaging audit (docs/MESSAGING_AUDIT_2026-10-01.md): a fault
 * is CRITICAL (red HUD + the pit call) only when the engine dies soon if the
 * driver carries on - never on a temperature alone. Health is the model's
 * 1 -> 0 component state; the terminal fires at 0 (fairness floor first). */
/* the notification thresholds that used to be defined here moved to
 * mf_notify.h (N2 2026-10-06) - the trigger conditions and their values now
 * live in one place with the offline test. */

static int commentary_say_now(int eid)
{
    DWORD table = CM_EVENT_TABLE + (DWORD)eid * 0x14u;
    int   table_pri   = *(volatile int*)(table + 0x00);
    int   table_score = *(volatile int*)(table + 0x10);
    float cooldown    = *(volatile float*)(table + 0x0C);

    /* E9g-c: the radio only exists in a live drive. A pending item whose
     * condition is false at fire time is DROPPED, never deferred (owner
     * heard "engine is blown, end of your race" while entering a menu). */
    if (!on_circuit()) {
        logf_("RADIO DROPPED (not on circuit): eid %d", eid);
        return 0;
    }

    if (cooldown > 1e37f) {
        logf_("commentary_say(%d): cooldown FLT_MAX, cannot bypass", eid);
        return 0;
    }

    *(volatile int*)(CM_QHEAD + 0x00) = eid;
    *(volatile int*)(CM_QHEAD + 0x04) = table_score;
    *(volatile int*)(CM_QHEAD + 0x08) = 0;
    *(volatile float*)(CM_QHEAD + 0x0C) = -1.0f;
    *(volatile int*)(CM_QHEAD + 0x10) = -1;
    *(volatile int*)(CM_QHEAD + 0x14) = -1;
    *(volatile int*)(CM_QHEAD + 0x18) = -1;
    *(volatile int*)(CM_QHEAD + 0x1C) = table_pri;
    *(volatile float*)(CM_TS_ARR + (DWORD)eid * 4u) = 0.0f;
    *(volatile float*)(CM_CONTROLLER + 0xAE8u) = 0.0f;
    *(volatile int*)(CM_CONTROLLER + 0x135Cu) = 1;

    logf_("commentary_say(%d): queued (score=%d pri=%d)", eid, table_score, table_pri);
    return 1;
}

/* ---- PWS_RETIRE04 on a real engine death --------------------------------
 * eid 119 (score 117, sdt_index 121), isolation-tested and driver-confirmed
 * 2026-08-07 - findings.jsonl pending. Gated on ReasonCode==1 specifically,
 * not "!= 0" - the game uses the same field for gearbox/suspension/
 * accident/etc and this line is Engine-specific.
 *
 * TWO different triggers, because the certainty differs:
 *
 * 1. KNOWN-CERTAIN death (oil-leak seizure, Num6 manual seizure) - WE set
 *    ENG_RUNNING=0 and ReasonCode=1 ourselves at an exact instant, so there
 *    is nothing to wait for. Fires a fixed 3-5s after that instant
 *    (retire_pending_arm()/retire_pending_pump()) regardless of whether the
 *    car has stopped yet - the driver already knows the engine is dead.
 *
 * 2. UNCERTAIN death (the vanilla life system's own staged ramp reaching
 *    its cap) - ReasonCode can appear without the engine being genuinely,
 *    permanently finished (the game re-arms it for the next stage). This
 *    path ports dll/retire_commentary.c's proven v5 stop-detection logic (5
 *    failed iterations documented there): announcing on the ReasonCode
 *    transition alone fired with the car still doing 88 km/h (v3); this
 *    instead requires the car crossed 10 m/s at some point this session
 *    (has_moved, rules out grid-start false positives) THEN holds under
 *    2 m/s for 2.5s continuous, re-checks ReasonCode right before speaking.
 *
 * Both share one latch (g_retire_fired) so whichever fires first wins and
 * the other never double-speaks. */
#define EID_RETIRE04   119
#define RETIRE_MOVED_MS  10.0f
#define RETIRE_STOP_MS   2.0f
#define RETIRE_DWELL_MS  2500
#define RETIRE_PENDING_MIN_MS 3000
#define RETIRE_PENDING_MAX_MS 5000

static int   g_retire_has_moved = 0;
static int   g_retire_fired     = 0;
static DWORD g_retire_stop_since = 0;
static DWORD g_retire_pending_until = 0;   /* 0 = nothing pending */
/* 2026-10-01 messaging audit: RETIRE04 says "engine is blown" - only for a
 * real blow-up (or the game's own engine death). A mod stop/seize (fuel
 * starvation, bearing seizure, head gasket, blow-by stop) stays silent. */
static int   g_death_quiet = 0;
/* V3: voice-line runtime state (mf_voice.h) - the retire path reads these */
static int   g_voice_on = 0;
static int   g_voice_dead = 0;      /* a mod-killed STOP: DEAD01/02, not RETIRE04 */
static int   g_voice_dead_idx = -1; /* MFV_DEAD01 or MFV_DEAD02 (rolled at the stop) */

static void retire_commentary_reset(void)
{
    g_retire_has_moved = 0;
    g_retire_fired = 0;
    g_retire_stop_since = 0;
    g_retire_pending_until = 0;
    g_death_quiet = 0;
    g_voice_dead = 0;        /* V3 */
    g_voice_dead_idx = -1;
}

/* owner 2026-10-05: RETIRE04 "...that's the end of your race" is wrong outside
 * a race. Blow-ups in practice / qualifying use DEAD01/02 (random); a race
 * keeps the vanilla RETIRE04. Vanilla SDT (voice OFF) -> RETIRE04 everywhere. */
static void death_blowup_say(void)
{
    if (g_voice_on && (*(volatile int*)0x007C56A4u) != 7) {
        int line = (rand() & 1) ? MFV_DEAD01 : MFV_DEAD02;
        radio_req(mfv_eid(line), MFR_P_DEATH);
        logf_("RETIRE04 replaced by %s (session type %d, not a race)",
              mfv_id(line), (*(volatile int*)0x007C56A4u));
        return;
    }
    radio_req(EID_RETIRE04, MFR_P_DEATH);
}

/* Call this at the exact instant of a KNOWN-CERTAIN engine death. */
static void retire_pending_arm(void)
{
    if (g_retire_fired || g_retire_pending_until) return;   /* already handled */
    g_retire_pending_until = GetTickCount()
        + (DWORD)rand_range(RETIRE_PENDING_MIN_MS, RETIRE_PENDING_MAX_MS);
    logf_("RETIRE04 pending in %lu ms",
          (unsigned long)(g_retire_pending_until - GetTickCount()));
}

static void retire_pending_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (!g_retire_pending_until || g_retire_fired) return;
    if (GetTickCount() < g_retire_pending_until) return;
    /* E9g-c: at fire time the car must be in a live drive (was a bare
     * RacePhase check); otherwise the item is DROPPED, never deferred. */
    if (!live_drive()) {
        logf_("RADIO DROPPED (not driving): eid %d", EID_RETIRE04);
        g_retire_pending_until = 0;
        return;
    }
    g_retire_fired = 1;
    g_retire_pending_until = 0;
    if (g_death_quiet) {
        /* V3: a mod-killed STOP gets DEAD01/02 (random) instead of RETIRE04 */
        if (g_voice_on && g_voice_dead && g_voice_dead_idx >= 0) {
            radio_req(mfv_eid(g_voice_dead_idx), MFR_P_DEATH);
            logf_("RETIRE04 replaced by %s (quiet stop)",
                  mfv_id(g_voice_dead_idx));
        } else {
            logf_("RETIRE04 skipped (not a blow-up)");
        }
        return;
    }
    death_blowup_say();
    logf_("RETIRE04 fired (pending timer, known-certain death)");
}

static void retire_commentary_pump(void)
{
    DWORD now;

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (!session_live()) { retire_commentary_reset(); return; }
    if (VEH_SPEED > RETIRE_MOVED_MS) g_retire_has_moved = 1;
    if (g_retire_fired) return;

    if (REASON_CODE != 1 || FINISH_STATE != 0 || !g_retire_has_moved) {
        g_retire_stop_since = 0;
        return;
    }

    if (VEH_SPEED >= RETIRE_STOP_MS) { g_retire_stop_since = 0; return; }

    now = GetTickCount();
    if (!g_retire_stop_since) { g_retire_stop_since = now; return; }
    if (now - g_retire_stop_since < RETIRE_DWELL_MS) return;

    if (REASON_CODE != 1) return;   /* re-check right before speaking */
    g_retire_fired = 1;
    if (g_death_quiet) {
        /* V3: a mod-killed STOP gets DEAD01/02 (random) instead of RETIRE04 */
        if (g_voice_on && g_voice_dead && g_voice_dead_idx >= 0) {
            radio_req(mfv_eid(g_voice_dead_idx), MFR_P_DEATH);
            logf_("RETIRE04 replaced by %s (quiet stop)",
                  mfv_id(g_voice_dead_idx));
        } else {
            logf_("RETIRE04 skipped (not a blow-up)");
        }
        return;
    }
    death_blowup_say();
    logf_("RETIRE04 fired (engine death, stopped %lu ms)",
          (unsigned long)(now - g_retire_stop_since));
}

/* Any SpecialFX reaction descriptor by id (1 white, 2 blue, 3 black smoke,
 * 4 flames - ids from MaterialShaderReactionEffectParser_004ca480). Only id 2
 * is live-proven through this call; 3/4 use the same table and emitter. */
static void* find_sfx_descriptor(int id)
{
    DWORD base = SFX_ARRAY_BASE, count = SFX_COUNT, i;
    if (!base || count == 0 || count > 64) return NULL;
    for (i = 0; i < count; i++) {
        DWORD entry = base + i * SFX_STRIDE;
        if (*(volatile int*)(entry + SFX_ID_OFF) == id)
            return (void*)entry;
    }
    return NULL;
}

static void* find_bluesmoke_descriptor(void)
{
    DWORD base = SFX_ARRAY_BASE, count = SFX_COUNT, i;
    if (!base || count == 0 || count > 64) return NULL;
    for (i = 0; i < count; i++) {
        DWORD entry = base + i * SFX_STRIDE;
        if (*(volatile int*)(entry + SFX_ID_OFF) == REACTION_ID_BLUESMOKE)
            return (void*)entry;
    }
    return NULL;
}

/* ReasonCode enum, FailureCategoryNameLookup_004bb730:
 * 1 Engine, 2 Gearbox, 3/7 Clutch, 4 Suspension, 5 Brakes,
 * 6 Accident, 8 Electronics, 9 Fuel                                       */

/* ---- session gates ----------------------------------------------------- */
#define SESSION_TYPE     (*(volatile int*)0x007C56A4u)   /* 5=qual 7=race   */
#define RACE_PHASE       (*(volatile int*)0x007C56A8u)   /* >4 = on track   */
/* .PLR "Mechanical failure rate" (file header, dword_7CEA5C): 0 none,
 * 1 normal, 2 timescaled, 3 season. 0 = effectively infinite life: the
 * model must not onset (handoff 1.1; E5). Read only. */
#define PLR_FAILRATE     (*(volatile int*)0x007CEA5Cu)
#define VEH_BASE         0x005EC6D8u   /* slot 0 = player */
/* B4 (ADDENDUM 14, plan section 9): AI vehicle stride + world position, used
 * by the blue-smoke queue (defined here because the queue sits above the AI
 * section). Live-verified moving for AI slots (2026-10-01). */
#define AI_VEH_STRIDE    0x48C0u
#define AI_OFF_POS_X     0x34E0u
#define AI_OFF_POS_Y     0x34E4u
#define AI_OFF_POS_Z     0x34E8u
#define OFF_LAP          0x403Cu
#define OFF_FINISH       0x4064u
#define TOTAL_LAPS_PTR   0x008ACF8Cu   /* CONFIRMED post-reduction lap count */

/* auto trigger types (owner 2026-09-28: oil leak joins the automatic mix,
 * weights 30/30/10/10/20) */
#define AUTO_E_SMOKE    1
#define AUTO_E_WEAR     2
#define AUTO_E_INSTANT  3
#define AUTO_E_SEIZURE  4
#define AUTO_E_OIL      5
/* new layer on top of vanilla (2026-09-28, docs/ENGINE_FAILURES_RESEARCH) */
#define AUTO_E_VALVE    6   /* N1 valve return force fault     */
#define AUTO_E_OILPRESS 7   /* N2 oil pressure loss            */
#define AUTO_E_EXHAUST  8   /* N4 exhaust crack -> rear susp   */
#define AUTO_E_BLOWBY   9   /* N5 piston ring blow-by          */
#define AUTO_E_COOLING  10  /* N7 cooling failure (debris/leak) */
#define AUTO_E_FUELPRESS 11 /* N8 fuel pressure loss           */
#define AUTO_E_FUELLEAK 12  /* fuel leak - runs dry (RC 9)      */

/* ---- car flavor (2026-09-28) -------------------------------------------
 * Scale the auto-roll chance by the car's own game-seeded lifetime value.
 * Every vehicle seeds four floats at veh+0x3D70/74/78/7C
 * (engine/brakes/gearbox/suspension) = Normal(3*LifetimeAvg, 3*LifetimeVar)
 * from engine.ini (EngineWearFailureSimulation_004ee150), divided by the race
 * divisor in race + mode 2. The player's copies are seeded but never tick
 * (WearTickGate veh+0x32C8 = 0), so they are a frozen per-session, per-car
 * draw. Mult = clamp(fieldMean / playerCounter, 0.50, 2.00) over the
 * 24-vehicle array (stride 0x48C0, base 0x5EC6D8). Our mechanism consuming
 * game-seeded data - see docs/CAR_FLAVOR_WEIGHTING_2026-09-28.md. */
#define FLAVOR_COUNTER_OFF 0x3D70u   /* engine slot of the four */
#define FLAVOR_VEH_STRIDE  0x48C0u
#define FLAVOR_VEH_COUNT   24
#define FLAVOR_CLAMP_LO    50        /* x0.50 */
#define FLAVOR_CLAMP_HI    200       /* x2.00 */

/* ---- vanilla timer mode (2026-09-28) -----------------------------------
 * The game seeds four lifetime counters per vehicle (veh+0x3D70/74/78/7C =
 * engine/brakes/gearbox/suspension) from engine.ini LifetimeAvg/Var, but its
 * wear ticker (FuelConsumptionAndWear_004ed960) is gated to AI vehicles by
 * the role byte (WearTickGate veh+0x32C8), which must never be flipped for
 * the player. This mode drives the SAME counter at the SAME rate the game
 * uses for AI - 1.0 per game-second from RacePhase > 4 - and fires the
 * family fault when it expires. Values and timing are the game's; the tick
 * and the presentation are ours. */
#define TIMER_COUNTER_OFF 0x3D70u    /* engine slot */
#define LIFE_MULT        1.0f         /* R4: engine life scale */
#define SESSION_TIME (*(volatile double*)0x008AFEA0u)

/* ---- engine torque, the thing stress actually cuts --------------------- */
#define ENG_TORQUE       (*(volatile double*)0x007C7430u) /* this+8    */
#define ENG_RPM          (*(volatile double*)0x007C9084u) /* this+7260, rad/s, live 2026-09-30 */
/* LIVE engine speed (2026-09-28 rpm hunt, trials/realism_2026-09-28/
 * rpm_hunt.csv): veh+0x39A0 float - 372 at idle, 1747 near the limiter,
 * same unit as the limit this+7388 (1895). 0x7C90C4 is this+7324 (unknown),
 * not engine speed - it read a frozen 441 while driving and must NOT be used
 * for live revs. (Moved up in E8b: the oil-heat scaler needs it before the
 * oil pumps.) */
#define ENG_RPM_LIVE     (*(volatile float*)(PLR_VEH + 0x39A0u))

/* ---- engine RUNNING flag = the binary seizure (FACT) -------------------
 * this+7257. sub_447B10 (misnamed RandomPhysicsParameterGenerator) is the
 * engine OUTPUT MULTIPLIER and opens with:
 *     if (!this[7257]) return 0.0;
 * Gearbox_00443b70 ends with  this[8] = sub_447B10(this) * this[8];
 * so clearing the byte zeroes torque through the game's own gate - an
 * instant, binary stop with no wear, no smoke and no progression.
 * The game's own one-line kill is sub_4472A8; its starter is sub_4471F0,
 * which sets the byte AND lifts rpm to idle if it has fallen below.
 *
 * LIVE-PROVEN 2026-07-30: after the stress death ran to the 0.6 cap this
 * byte was ALREADY 0 - the vanilla failure path clears it itself. Writing
 * 1 restored it and it stayed 1 (nothing re-zeroed it).                 */
#define ENG_RUNNING      (*(volatile unsigned char*)0x007C9081u)

/* ---- in-game HUD message strip (FACT, mods/red_flag/gr29.c) -----------
 * The game's own message strip - the one that shows RED FLAG. 8 slots.
 * A slot is free when its countdown float is <= 0. Text is ASCII, max 31
 * chars. The countdown fades on its own, so a message must be re-armed
 * every tick to stay up, and nothing needs cleaning up on unload.       */
#define HUD_OUTER_PTR    0x007CD204u
#define HUD_SLOTS        8
#define HUD_STRIDE       0x5C
#define HUD_TEXT_OFF     0x3C
#define HUD_TIMER_OFF    0x90
#define HUD_TEXT_MAX     31

#define UNLOAD_EVENT     "F1C_ENGFAIL_UNLOAD"
#define SHM_NAME         "F1C_ENGFAIL_CTL"
#if defined(MF_ENGINE_ONLY) && MF_ENGINE_ONLY
/* release: beside the plug-in, so Lightloader's bug report attaches it */
#define LOG_PATH         "plugins\\lightloader\\mechanical_failures.log"
#define LOG_OLD_PATH     "plugins\\lightloader\\mechanical_failures.old.log"
#else
#define LOG_PATH         "engine_failure_log.txt"
#define LOG_OLD_PATH     "engine_failure_log.old.txt"
#endif
#define LOG_ROTATE_BYTES (4u * 1024u * 1024u)
#define CTL_MAGIC        0x46474E46u   /* 'ENGF' */

/* ---- shared control block (GUI writes cmd_*, DLL writes status) --------
 * Layout is the ABI between this DLL and the Engine tab of
 * tools/mechanical_failure_gui.py. Append-only: the END, and nowhere else.
 * Offsets are in int32 units. */
typedef struct {
    volatile unsigned magic;
    volatile int enabled;          /* master on/off (informational)        */
    volatile int heartbeat;

    volatile int cmd_stage1;       /* GUI sets 1 -> smoke stage, DLL clears */
    volatile int cmd_terminal;     /* ramp to death (~100 s)                */
    volatile int cmd_instant;      /* stress straight to the 0.6 cap       */
    volatile int cmd_seizure;      /* binary stop, the ignition byte       */
    volatile int cmd_repair;       /* restore life/stress/banks/engine     */
    volatile int cmd_dump;         /* log + HUD state                      */
    volatile int cmd_emit_on;      /* 0..2 = toggle emitter, -1 = none     */
    volatile int cmd_emit_clear;   /* all emitters off                     */
    volatile int cmd_telemetry;    /* toggle the live telemetry HUD        */

    volatile float life;           /* DLL: engine life (s)                 */
    volatile float stress;         /* DLL: stress rate, 0..0.6             */
    volatile float torque;         /* DLL: engine torque                   */
    volatile float rpm;            /* DLL: engine rpm                      */
    volatile float bank1;          /* DLL: WHITE SMOKE emitter             */
    volatile float bank2;          /* DLL: BLACK SMOKE emitter             */
    volatile float bank3;          /* DLL: FLAMES emitter                  */
    volatile int running;          /* DLL: ignition byte                   */
    volatile int reason;           /* DLL: ReasonCode                      */
    volatile int phase;            /* DLL: RACE_PHASE                      */
    volatile int session;          /* DLL: SESSION_TYPE                    */
    volatile int telemetry_on;     /* DLL: telemetry HUD state             */

    /* --- appended 2026-08-07, cutout + oil leak --- */
    volatile int cmd_cutout_toggle; /* GUI sets 1 -> toggle, DLL clears     */
    volatile int cmd_oil_toggle;    /* GUI sets 1 -> toggle, DLL clears     */
    volatile int cutout_on;         /* DLL: cutout armed 0/1                */
    volatile int oil_on;            /* DLL: oil leak armed 0/1              */
    volatile float oil_pressure;    /* DLL: 100..0                          */

    volatile unsigned abi_version;
    volatile unsigned struct_size;

    /* --- auto trigger (APPENDED 2026-09-27, after the ABI guard fields) -- */
    volatile int auto_enable;       /* GUI: 1 = roll once per session      */
    volatile int auto_chance_pm;    /* GUI: per-mille, 150 = 15%           */
    volatile int auto_earliest_lap; /* GUI: first lap a fault may fire     */
    volatile int auto_guard_laps;   /* GUI: no instant death inside last N */
    volatile int auto_seed;         /* GUI: 0 = derive from tick count     */
    volatile int auto_rolled;       /* DLL: this session already rolled    */
    volatile int auto_fired;        /* DLL: a fault was auto-armed         */
    volatile int auto_type;         /* DLL: AUTO_E_*                       */
    volatile int auto_lap;          /* DLL: lap the fault fired on         */
    volatile int auto_roll_result;  /* DLL: 1 = this session rolled a fault*/

    /* --- car flavor (APPENDED 2026-09-28) -------------------------------- */
    volatile int flavor_enable;     /* GUI: 1 = scale auto chance by car   */
    volatile int flavor_mult_pm;    /* DLL: x0.50..x2.00 as per-mille      */
    volatile int flavor_counter;    /* DLL: player's seeded counter (int)  */

    /* --- vanilla timer mode (APPENDED 2026-09-28) ------------------------ */
    volatile int timer_enable;      /* GUI: 1 = use the game's lifetime timer */
    volatile int timer_remaining;   /* DLL: seconds left (-1 = not seeded)    */

    /* --- timer tamper (APPENDED 2026-09-28) ------------------------------ */
    volatile int cmd_set_timer;     /* GUI: 1 = write timer_set_ms now        */
    volatile int timer_set_ms;      /* GUI: new remaining time, milliseconds  */

    /* --- realism pass (APPENDED 2026-09-28, ABI 6) ----------------------- */
    volatile int cmd_overheat;      /* GUI: 1 = arm overheating now          */
    volatile int heat_s;            /* DLL: overheat seconds since arm, -1 off */

    /* --- oil-on-track spike (APPENDED 2026-09-28, ABI 7) ----------------- */
    volatile int trail_id;          /* GUI: ground-mark reaction id, 0 = none */
    volatile int cmd_trail_toggle;  /* GUI: 1 = toggle the test trail         */
    volatile int trail_on;          /* DLL: trail currently being laid 0/1    */
    volatile int trail_max;         /* DLL: Max of the chosen descriptor, -1 = missing */

    /* --- oil leak v2 (APPENDED 2026-09-28, ABI 8) ------------------------ */
    volatile float oil_temp_c;      /* DLL: vanilla oil temperature, C        */
    volatile float oil_limit_c;     /* DLL: car's LifetimeOilTemp base, C     */
    volatile int   oil_rate_x100;   /* DLL: leak rate, %/s x100               */

    /* --- new engine failures (APPENDED 2026-09-28, ABI 9) ---------------- */
    volatile int cmd_valve;         /* GUI: 1 = arm valve return fault        */
    volatile int cmd_oilpress;      /* GUI: 1 = arm oil pressure loss         */
    volatile int cmd_exhaust;       /* GUI: 1 = arm exhaust crack             */
    volatile int cmd_blowby;        /* GUI: 1 = arm piston blow-by            */
    volatile int valve_air_pct;     /* DLL: valve return force %, -1 = off    */
    volatile int valve_ceiling_pct; /* DLL: allowed revs, % idle->limit      */
    volatile int oilpress_pct;      /* DLL: oil pressure %, -1 = off          */
    volatile int blowby_pct;        /* DLL: blow-by damage %, -1 = off        */
    volatile int exhaust_s;         /* DLL: s until rear suspension, -1 off   */
    volatile int overrev_enable;    /* GUI: 1 = over-rev damage layer on      */
    volatile int overrev_events;    /* DLL: over-revs this session            */
    volatile int rev_pct;           /* DLL: revs, % idle->limit               */

    /* --- cooling + fuel pressure (APPENDED 2026-09-28, ABI 10) ----------- */
    volatile int   cmd_cooling;     /* GUI: 1 = debris, 2 = water leak, 3 = roll */
    volatile int   cmd_fuelpress;   /* GUI: 1 = arm fuel pressure loss        */
    volatile int   cooling_pct;     /* DLL: radiator effectiveness %, -1 off  */
    volatile int   cooling_kind;    /* DLL: 1 debris, 2 water leak            */
    volatile float water_temp_c;    /* DLL: vanilla water temperature, C      */
    volatile int   fuelpress_pct;   /* DLL: fuel pressure %, -1 off           */
    volatile int   coeff_held;      /* DLL: 1 = radiator write held, 0 = game rewrote it */

    /* --- worker #1 levers (APPENDED 2026-09-29, ABI 11) ------------------ */
    volatile int   cmd_fuelleak;    /* GUI: 1 = arm fuel leak                 */
    volatile int   fuelleak_on;     /* DLL: fuel leak running 0/1             */
    volatile float fuel_now;        /* DLL: vanilla fuel (this+412)           */
    volatile int   fuel_laps_x10;   /* DLL: laps the fuel will last x10, -1 unknown */

    /* --- engine sim model v2 diagnostics (APPENDED 2026-09-30, ABI 12) --- */
    volatile int   wear_x100;       /* DLL: drain EMA x100 (s/s)              */
    volatile float time_to_fail_s;  /* DLL: life / drain, -1 unknown          */
    volatile int   onset_state;     /* DLL: 0 none 1 developing 2 stabilised 3 clock out 4 blown 5 stopped */
    volatile int   bearings_pct;    /* DLL: bearing health %                  */
    volatile int   valvetrain_pct;  /* DLL: valvetrain health %               */
    volatile int   rings_pct;       /* DLL: ring seal %                       */
    volatile int   conrods_pct;     /* DLL: conrod health %                   */
    volatile int   gasket_pct;      /* DLL: head gasket health %              */
    volatile int   oil_l_x10;       /* DLL: sump oil, L x10                   */
    volatile int   coolant_l_x10;   /* DLL: coolant, L x10                    */
    volatile int   oil_p_pct;       /* DLL: oil pressure % (model)            */
    volatile int   valve_p_pct;     /* DLL: valve return force % (model)      */
    volatile int   crack_pct;       /* DLL: exhaust crack length %            */
    volatile int   rpm_base_x10;    /* DLL: car LifetimeEngineRPM base, x10 rad/s */
    volatile int   rpm_range_x10;   /* DLL: car LifetimeEngineRPM range, x10 rad/s */
    volatile int   oil_base_x10;    /* DLL: car LifetimeOilTemp base, x10 K   */
    volatile int   oil_range_x100;  /* DLL: car LifetimeOilTemp range, x100 K */
    volatile int   rev_limit_x10;   /* DLL: car rev limit, x10 rad/s          */
    volatile float life_s;          /* DLL: game engine life clock, s         */

    /* --- E8d part temperatures (APPENDED 2026-09-30, ABI 13) ------------- */
    volatile float t_piston_c;      /* DLL: piston crown temperature, C       */
    volatile float t_exh_valve_c;   /* DLL: exhaust valve temperature, C      */
    volatile float t_bearing_c;     /* DLL: main bearing temperature, C       */
    volatile float t_head_c;        /* DLL: cylinder head temperature, C      */
    volatile float t_exhaust_c;     /* DLL: exhaust gas temperature, C        */
    volatile int   film_pct;        /* DLL: oil film / FILM_CRIT x100, cap 999 */
    volatile int   fault_id;        /* DLL: model fault enum, -1 none         */

    /* --- E9b niggle control (APPENDED 2026-09-30; ABI version unchanged) -- */
    volatile int   niggle_enable;   /* GUI: 1 = harmless niggles on (default 1) */

    /* --- E9p AI shadow engines (APPENDED 2026-09-30; ABI version unchanged) */
    volatile int   ai_engines_enable;  /* GUI: 1 = run shadow engines for AI cars
                                        *      (default 0 until the owner tests) */
    volatile int   ai_life_divisor_x10;/* GUI: AI life divisor x10 (10 = 1.0 = the
                                        *      game's own AI reliability; 30 = the
                                        *      AI as reliable as the player) */
    volatile int   ai_engine_law;      /* GUI: 1 = native drain law + thermal ODE,
                                        *      0 = the game's timer is the clock */

    /* --- E9i save mode (APPENDED 2026-10-01; ABI version unchanged) ------- */
    volatile int   save_mode;          /* DLL: "save the engine" state (F8 or GUI
                                        *      request; reset on session/repair) */

    /* --- E9-H4d AI panel layout (APPENDED 2026-10-01; ABI 13 unchanged) --- */
    volatile unsigned ai_tbl_addr;     /* DLL: absolute address of g_ai[0]      */
    volatile int      ai_slot_size;    /* DLL: sizeof(MfAiSlot), bytes          */
    volatile int      ai_slot_count;   /* DLL: MF_AI_MAX (24)                   */
    volatile int      ai_off[24];      /* DLL: field offsets, panel key order   */

    /* --- J6 AI law state offsets (APPENDED 2026-10-01; ABI 13 unchanged) --- */
    volatile int      ai_law_off[4];   /* DLL: MfAiSlot.law offsets: life_s,
                                        *      oil_k, water_k, init (panel; the
                                        *      per-hash fallback table in
                                        *      mech_test_panel.py stays only as
                                        *      a fallback) */

    /* --- P1 component damage (APPENDED 2026-10-01; ABI 13 unchanged) ------ */
    volatile float dmg_share[10];      /* DLL: player dmg[i] / dmg_total, 0..1
                                        *      (fault order MF_FAULT_*)       */
    volatile int   ai_dmg_off;         /* DLL: offsetof(MfAiSlot, eng.dmg)     */

    /* --- Q1 AI wear rate (APPENDED 2026-10-01; ABI 13 unchanged) ---------- */
    volatile int   ai_q1_off[2];       /* DLL: MfAiSlot offsets: ai_life,
                                        *      ai_rate (panel)                */

    /* --- Q3 hazard onset model (APPENDED 2026-10-01; ABI 13 unchanged) ---- */
    volatile int   onset_model;        /* GUI: 1 = hazard onset (default),
                                        *      0 = countdown only (A/B)      */

    /* --- Q6a AI onset model (APPENDED 2026-10-01; ABI 13 unchanged) ------- */
    volatile int   ai_onset_model;     /* GUI: AI onset: 0 = countdown on ai_life
                                        *      = timer/divisor (DEFAULT since
                                        *      Q8a, ADDENDUM 8: the countdown
                                        *      reproduces the realized vanilla
                                        *      WHEN 1:1 - Q7 sweep missed 0 %,
                                        *      false 0 % at rate 1.0); 1 = the
                                        *      F8C78830 hazard (offline A/B).  */

    /* --- R3l dead status (APPENDED 2026-10-01; ABI 13 unchanged) ---------- */
    volatile int   dead_latch;         /* DLL: 1 = our dead latch/model terminal,
                                        *      held until repair/session */
    volatile int   dead_state;         /* DLL: model onset_state at death:
                                        *      4 blown, 5 stopped */
    volatile int   dead_fault;         /* DLL: MF_FAULT_* at death, -1 none */
    volatile char  dead_fault_name[12];/* DLL: terminal reason at death */

    /* --- R3m exhaust rear heat (APPENDED 2026-10-01; ABI 13 unchanged) ---- */
    volatile int   rear_heat_pct;      /* DLL: rear-suspension heat %, 0..100
                                        *      (monotonic; ETA in exhaust_s is
                                        *      smoothed) */

    /* --- ADDENDUM 15 AI pull-off (APPENDED 2026-10-02; ABI 13 unchanged) -- */
    volatile int   ai_po_state[24];    /* DLL: MfPulloff state per slot
                                        *      (0 none; 1 NOTICE ... 8 ASIDE,
                                        *      see mf_ai_pulloff.h)            */
    volatile int   ai_po_side[24];     /* DLL: -1 left, +1 right, 0 none      */
    volatile int   ai_po_goal_x10[24]; /* DLL: |goal| m x10 (panel row)       */

    /* --- N3 dead reason (APPENDED 2026-10-06; ABI 14) --------------------- */
    volatile char  dead_reason[48];    /* DLL: model death reason string
                                        *      ("over-rev: valve hit piston",
                                        *      "engine life expired", "head
                                        *      gasket", ...) - the panel's
                                        *      ENGINE DEAD label (H16)       */
} EngineCtl;

_Static_assert(sizeof(EngineCtl) == 992,
               "EngineCtl ABI 14 (+H4d ai layout, +J6 law offsets, +P1 damage,"
               " +Q1 ai rates, +Q3 onset model, +Q6a ai onset model,"
               " +R3l dead status, +R3m rear heat, +ADD15 pull-off panel,"
               " +N3 dead_reason) size changed");

static HINSTANCE g_hinst;
static double    g_saved_life  = 0.0;   /* captured before the first arm */
static int       g_have_saved  = 0;
static unsigned  g_saved_life_gen = 0;  /* H3c/R09: session gen of the capture */

static EngineCtl* g_ctl = NULL;
static HANDLE     g_shm = NULL;

/* the HUD line currently drawn (mirror of the arbiter, mf_msg.h) */
static char      g_hud_msg[HUD_TEXT_MAX + 1];
static MfMsgArb  g_msg;                 /* 2026-10-01: HUD arbiter */
static MfRadioQ  g_radio;               /* 2026-10-01: radio queue */
static int       g_telemetry   = 0;     /* live readout toggle */
static int       g_pump_frozen;         /* N3: forward (defined with the
                                         * pump pause gate below) */

#include "mf_usage.h"   /* 2026-10-06 release: opted-in usage summary */

static void logf_(const char *fmt, ...)
{
    va_list ap;
    char line[512];
    FILE *f;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    line[sizeof line - 1] = '\0';
    {   /* 2026-10-06 release: one rotation per load when the log is big
         * (the previous .old copy is replaced; nothing else is touched) */
        static volatile LONG rotated = 0;
        if (InterlockedCompareExchange(&rotated, 1, 0) == 0) {
            WIN32_FILE_ATTRIBUTE_DATA fa;
            if (GetFileAttributesExA(LOG_PATH, GetFileExInfoStandard, &fa)
                && (fa.nFileSizeHigh || fa.nFileSizeLow > LOG_ROTATE_BYTES))
                MoveFileExA(LOG_PATH, LOG_OLD_PATH, MOVEFILE_REPLACE_EXISTING);
        }
    }
    f = fopen(LOG_PATH, "a");
    if (f) {
        fputs(line, f);
        fputc('\n', f);
        fclose(f);
    }
    mfu_tap(line);
}

/* E7: private waveOut voices with the game's level chain (after logf_). */
#include "mf_audio.h"

/* R3j (ADDENDUM 11): place a sound at the player car's current world
 * position; the audio pump reads the live camera from the engine family's
 * read-only funnel tap (fallback: the car itself, full volume). */
static void play_at_car(const char *path, float gain)
{
    mf_audio_play_pos(path, gain,
                      *(volatile float*)(PLR_VEH + OFF_POS_X),
                      *(volatile float*)(PLR_VEH + OFF_POS_Y),
                      *(volatile float*)(PLR_VEH + OFF_POS_Z));
    if (g_mfc_latched) mfc_on_sound(0, 0, path, gain);   /* CO-OP mirror */
}

/* A session is live once the race phase is past the pre-green states.
 * sub_4D21D0 uses exactly this test: phase 0..4 -> not running.         */
static int session_live(void)
{
    return RACE_PHASE > 4;
}

/* ---- HUD ---------------------------------------------------------------
 * 2026-10-01 messaging audit (docs/MESSAGING_AUDIT_2026-10-01.md): every
 * line goes through the arbiter (mf_msg.h). hud_show() POSTS a line (4 s,
 * queued behind an equal/higher tier instead of overwriting it); hud_live()
 * shows a line only while its condition holds; hud_pump() draws the
 * arbiter's current line and takes the previous one off the strip at once
 * (mfh_drop - no fade). Clock: the sim clock, so a pause holds the line. */
static void hud_show(const char *fmt, ...)
{
    va_list ap;
    char buf[HUD_TEXT_MAX + 1];
    /* E9g-c: the HUD strip only exists in a live drive (no messages while a
     * menu, the garage or a results screen is up). 2026-10-07: and only on
     * the circuit (not pit lane / box / garage). */
    if (!on_circuit()) return;
    va_start(ap, fmt);
    _vsnprintf(buf, HUD_TEXT_MAX, fmt, ap);
    va_end(ap);
    buf[HUD_TEXT_MAX] = '\0';
    mfm_post(&g_msg, buf, game_ms());
}

/* a line that is on screen exactly while cond holds (owner 2026-10-01:
 * SHIFT NOW must go the moment it no longer applies) */
static void hud_live(const char *msg, int cond)
{
    if (cond && !on_circuit()) cond = 0;
    mfm_live(&g_msg, msg, cond, game_ms());
}

/* R3c (ADDENDUM 11, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): a repeating HUD
 * line shows on its rising edge, then at most one reminder per period while
 * the condition holds (sim clock: a pause holds the cadence too).
 * 2026-10-01 audit: diagnosis 60 s, instruction 45 s, critical 90 s. */
#define MF_HUD_REMIND_MS      45000u
#define MF_HUD_REMIND_DIAG_MS 120000u  /* N3 2026-10-06 item 5: same diagnosis
                                        * text max once per 120 s (due stamp
                                        * persists across re-arms) */
#define MF_HUD_REMIND_CRIT_MS 90000u
typedef struct { int on; DWORD due; } MfHudBlink;
static void hud_blink_p(MfHudBlink *b, int cond, const char *msg, DWORD period)
{
    DWORD now = game_ms();
    /* N3 2026-10-06 (item 5): the due stamp PERSISTS when the condition goes
     * false, so the same text can never re-post sooner than `period` across
     * re-arms (the old `!b->on ||` edge re-posted on every new episode). */
    if (!cond) { b->on = 0; return; }
    if ((long)(now - b->due) >= 0) {
        b->on = 1;
        b->due = now + period;
        hud_show("%s", msg);
    }
}
static void hud_blink(MfHudBlink *b, int cond, const char *msg)
{
    hud_blink_p(b, cond, msg, MF_HUD_REMIND_MS);
}

/* coloured + leak-free via mf_hud.h (owner 2026-09-29) */
static mfh_own_t g_hud_own;

/* drop every queued/shown line (stint end, session change) */
static void hud_clear(void)
{
    mfm_clear(&g_msg);
}

/* take one line out of the arbiter (it no longer applies) */
static void hud_cancel(const char *msg)
{
    mfm_live(&g_msg, msg, 0, game_ms());          /* live copies */
    if (g_msg.has && !strcmp(g_msg.cur.text, msg)) g_msg.has = 0;
    {
        int qi = mfm_find_q(&g_msg, msg);
        if (qi >= 0) mfm_qdel(&g_msg, qi);
    }
}

static void hud_pump(void)
{
    const char *cur;
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (!on_circuit()) {          /* E9g-c + 2026-10-07: HUD only on the circuit */
        if (g_hud_msg[0]) mfh_drop(&g_hud_own, g_hud_msg);
        mfh_release(&g_hud_own);
        hud_clear();
        g_hud_msg[0] = '\0';
        return;
    }
    mfm_tick(&g_msg, game_ms());
    cur = mfm_current(&g_msg);
    if (g_hud_msg[0] && (!cur || strcmp(cur, g_hud_msg) != 0))
        mfh_drop(&g_hud_own, g_hud_msg);          /* old line off at once */
    mfh_release(&g_hud_own);
    if (!cur) { g_hud_msg[0] = '\0'; return; }
    lstrcpynA(g_hud_msg, cur, HUD_TEXT_MAX + 1);
    mfh_keep(&g_hud_own, g_hud_msg, 1.5f);
}

/* ---- E9i: "save the engine" mode ----------------------------------------
 * Off by default. Toggled by MF_SAVE_KEY (rising edge, only while the game
 * owns the foreground window; 0 = no key) or by the GUI writing control-block save_mode (any value
 * different from ours is a request; the DLL publishes the state back). Reset
 * on a session change and on repair. HOW only: the model scales fault growth
 * (x MF_SAVE_GROWTH) and the early hazard (x MF_SAVE_EARLY); the rev band is
 * set by rev_band_pump (x 0.97, combined with any valve ceiling, lower wins,
 * E9g-e dead latch respected). */
/* 2026-10-01 owner: F8 is the game's traction control key - no save key
 * until the owner picks a free one (panel only). Set a VK_ code to enable. */
#define MF_SAVE_KEY 0
static int g_save_mode = 0;
static int g_save_key_prev = 0;

static void save_toggle(int on, const char *source)
{
    on = on ? 1 : 0;
    if (on == g_save_mode) return;
    g_save_mode = on;
    hud_show(on ? "ENGINE SAVE ON" : "ENGINE SAVE OFF");
    logf_("SAVE: %s (source %s)", on ? "on" : "off", source);
    if (on) voice_say(MFV_SAVE01, MFR_P_INFO);   /* V3 */
}

static void save_reset(const char *source)
{
    save_toggle(0, source);
    if (g_ctl && g_ctl->magic == CTL_MAGIC) g_ctl->save_mode = 0;
}

static void save_mode_pump(void)
{
    EngineCtl *c = g_ctl;
    int down;

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (c && c->magic == CTL_MAGIC) {
        if (c->save_mode != g_save_mode)     /* GUI request */
            save_toggle(c->save_mode != 0, "panel");
        c->save_mode = g_save_mode;
    }
    if (!MF_SAVE_KEY) return;
    down = (GetAsyncKeyState(MF_SAVE_KEY) & 0x8000) != 0;
    if (down && !g_save_key_prev) {
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (fg) GetWindowThreadProcessId(fg, &pid);
        if (fg && pid == GetCurrentProcessId())
            save_toggle(!g_save_mode, "key");
    }
    g_save_key_prev = down;
    /* live 2026-10-01: publish after the key too - a stale block was read as a
     * panel request on the next tick and switched save mode straight off */
    if (c && c->magic == CTL_MAGIC) c->save_mode = g_save_mode;
}

/* H3c/R09: the repair lifetime snapshot belongs to one engine/session. A
 * capture from an earlier generation is invalid (repair must never restore
 * another engine's lifetime); recapture from the current valid engine. */
static void save_life_once(void)
{
    if (!g_have_saved || g_saved_life_gen != g_session_gen) {
        g_saved_life = ENG_LIFE;
        g_have_saved = 1;
        g_saved_life_gen = g_session_gen;
        logf_("  [saved original life = %.3f (gen %u)]", g_saved_life,
              g_saved_life_gen);
    }
}

/* ---- realism layer (2026-09-28, R1/R2/R3) - defined further down ------- */
static int  g_wear_watch = 0;       /* wear ramp armed: announce the death */
#if (MF_LEGACY_VALVE || MF_LEGACY_OILP || MF_LEGACY_COOLING || MF_LEGACY_FUEL \
     || MF_LEGACY_EXHAUST || MF_LEGACY_BLOWBY)
static float load_factor(void);
#endif

/* E6: "player on track" for the model gates (handoff 4.7): practice and
 * pit-out before green included. RACE_PHASE alone only turns > 4 once the
 * session is running, so a moving car with the engine on counts too. */
static int player_ontrack(void)
{
    if (FINISH_STATE != 0) return 0;
    if (RACE_PHASE > 4) return 1;
    return ENG_RUNNING && VEH_SPEED > 1.0f;
}

/* E9g-c (owner report 2026-09-30): a sound, radio line or HUD message may
 * only happen during a LIVE DRIVE: a timed session, on track, player not
 * retired. The owner heard "engine is blown, end of your race" while
 * entering a MENU - every play/fire site checks this at the moment of
 * playing, and a pending item whose condition is false is dropped, never
 * deferred. player_ontrack() alone also allows a moving car before green,
 * so RACE_PHASE > 4 is kept on top. */
static int live_drive(void)
{
    return !SESSION_OVER() && RACE_PHASE > 4 && player_ontrack();
}

/* 2026-10-07 owner ("Radiator's clear now" played after returning to the
 * pits): engineer radio only while the player is OUT ON THE CIRCUIT - a
 * live drive AND not in the pit lane, box or garage. pittingStatus
 * veh+0x3450 >= 2 = pit lane / box (1 = in-lap, still on track; FACT,
 * findings f1c-pittingstatus-1-is-still-on-track); garage signature
 * veh+0x4488 = 1 and veh+0x4578 = 1 (FACT, garage-vs-track-signature). */
/* 2026-10-07 c (field evidence, owner session: the player car read
 * pittingStatus 4 and garage bytes 1/1 for the WHOLE session, driving
 * included - those AI-measured fields mean nothing for the player).
 * Signal now: byte_7CE254 = the game's "UI screen up" flag (decompile:
 * sub_4A00A0 enter-the-car hides the cursor and sets 0; sub_4A0250 leave to
 * the monitor/garage shows the cursor and sets 1; sub_4A0CF0 goes to the
 * monitor only while 0; physics and sound test it). On the circuit = live
 * drive AND in the car (flag 0) AND not stopped in the pit box (pittingStatus
 * 3, used live by the mod). A stall or a dead engine on track stays ON the
 * circuit. Changes are logged with the raw values. */
#define UI_SCREEN_UP (*(volatile unsigned char*)0x007CE254u)
/* 2026-10-07 live test (MF-05, Tester C report): a niggle rolled with the car
 * standing at the garage/pit start (pittingStatus 4, 0 m/s). pittingStatus
 * (CONFIRMED): 0 on track, 1 called in (still on track), 2 pit lane, 3 box,
 * 4 leaving the pits - and it can stay 4 for the player after a garage start.
 * So 2/3 are always off circuit, and from 4 the car only counts as out once
 * it has gone faster than the pit limit (80 km/h = 22.2 m/s, FACT
 * f1c-maxspeed-is-the-pit-limiter-too). The latch resets in the lane/box,
 * on any UI screen and outside a live drive. */
#define MF_CIRCUIT_OUT_MS 25.0f
static int on_circuit(void)
{
    static int last = -1;
    static volatile LONG out_latch = 0;
    int ps = *(volatile int*)(PLR_VEH + 0x3450u);
    int ui = UI_SCREEN_UP;
    int live = live_drive();
    int r;
    if (!live || ui != 0 || ps == 2 || ps == 3)
        InterlockedExchange(&out_latch, 0);
    else if (ps == 0 || ps == 1 || VEH_SPEED > MF_CIRCUIT_OUT_MS)
        InterlockedExchange(&out_latch, 1);
    r = live && ui == 0 && ps != 2 && ps != 3 && out_latch
        && REPLAY_MODE != 2;   /* audit MF-12 */
    if (r != last) {
        last = r;
        logf_("CIRCUIT: %s (live %d, ui screen %d, pittingStatus %d, out %d, engine %d, %.1f m/s)",
              r ? "on" : "off", live, ui, ps, (int)out_latch, ENG_RUNNING ? 1 : 0, (double)VEH_SPEED);
    }
    return r;
}

/* E6 containment (owner report #6): no radio line or engineer instruction
 * while the car is wrecked or stopped after an impact.
 *   REASON_CODE 6 = Accident; FINISH_STATE != 0 = classified/retired;
 *   under 1 m/s for > 2 s with the engine off = stopped after an impact. */
static int wrecked_or_stopped(void)
{
    static DWORD slow_since = 0;
    DWORD now = game_ms();
    if (REASON_CODE == 6) return 1;
    if (FINISH_STATE != 0) return 1;
    if (ENG_RUNNING == 0 && VEH_SPEED < 1.0f) {
        if (!slow_since) { slow_since = now; return 0; }
        return (long)(now - slow_since) >= 2000;
    }
    slow_since = 0;
    return 0;
}

/* ---- detection timing (owner 2026-09-28) --------------------------------
 * Symptoms first, diagnosis later - and per failure, by WHO can know:
 *   dash sensor  -> warning 1-3 s after the reading crosses its threshold
 *                   (the reading itself takes time to get there);
 *   engineer     -> radio 5-15 s after the telemetry shows it (only eid 77
 *                   "engine overheating" exists for engine faults);
 *   marshals     -> eid 130 (mechanical-problem flag) 5-30 s after smoke or
 *                   flames become visible;
 *   driver-only  -> no message (blow-by: engineer spots the power loss on
 *                   telemetry 45-90 s later).
 * warn_after(): a condition must STAY true for a rolled delay; a HUD warning
 * then stays up while it holds. say_after(): fires once per repair cycle. */
#define W_VALVE    0
#define W_OILP     1
#define W_OILLOW   2
#define W_OILT     3
#define W_WATER    4
#define W_FUEL     5
#define W_BLOWBY   6
#define R_OIL77    7
#define R_OILSMOKE 8
#define R_WATER77  9
#define R_BBSMOKE  10
#define R_EXHAUST  11
#define W_RADBLOCK 12
#define W_EXHTEMP  13
#define R_PITADV   14
#define R_VALVEPIT 15
#define R_WATERPIT 16
#define R_STEAM    17
#define I_VALVE    18
#define I_VALVEPIT 19
#define I_OILP     20
#define I_OILT     21
#define I_WATER    22
#define I_FUEL     23
#define I_BLOWBY   24
#define I_EXHAUST  25
#define I_WATERPIT 26
#define W_FUELLEAK 27
#define I_FUELLEAK 28
#define R_FUELPIT  29
#define I_FUELPIT  30
/* 2026-10-01 messaging audit: critical HUD lines and critical radio calls */
#define C_WATER    31
#define C_OILP     32
#define C_OILLEAK  33
#define C_BLOWBY   34
#define C_FUELP    35
#define RC_WATER   36
#define RC_ENGINE  37
#define RC_FUELP   38
/* L2 2026-10-05: low-coolant diagnosis (cooling family; owner live report
 * "WATER TEMP at 79 C" - the level, not the temperature) */
#define W_COOLANTLOW 39
/* V3 2026-10-05: voice-line slots (mf_voice.h) */
#define R_OILP1    40   /* oil pressure pre line  */
#define R_VALVE1   41   /* valve pre line         */
#define R_WATER1   42   /* water leak pre line    */
#define I_OILT1    43   /* oil temp instruction   */
#define I_COOLANT1 44   /* coolant lost instruction */
#define R_FUELLEAK1 45  /* fuel leak pre line     */
#define R_FUELP1   46   /* fuel pressure pre line */
#define W_COUNT    47
static DWORD g_w_due[W_COUNT];
static int   g_w_done[W_COUNT];
static int   g_w_held[W_COUNT];   /* L1: last held() result per id (lamp pump) */
/* Claude review 2026-10-05: worker pass in which held() last ran per id. The
 * pumps return early once their fault is off (if (!g_cool_on) return; ...),
 * so a stale g_w_held would keep the lamp on - lamp_pump only counts ids
 * evaluated in the current pass. */
static unsigned g_w_pass[W_COUNT];
static unsigned g_lamp_pass = 1;
static MfHudBlink g_warn_blink[W_COUNT];   /* R3c: edge + reminders */
static MfHudBlink g_instr_blink[W_COUNT];
static int g_radio_pre_done = 0;   /* the one pre-critical call of this fault */
static DWORD g_vcrit_after[W_COUNT];   /* 2026-10-07: critical held after a forced pre */
/* N3 2026-10-06: item 5 fault-type stamps (a new fault TYPE may post at once);
 * item 8 critical ordering (diagnosis timestamp per critical id); voice rate
 * (per MFV line, 120 s, fault-type aware). */
static int      g_w_diag_fault[W_COUNT];
static DWORD    g_crit_diag_at[W_COUNT];
static int      g_crit_diag_forced[W_COUNT];
static int      g_crit_fault[W_COUNT];
static uint32_t g_v_rate[MFV_COUNT];
static int      g_v_rate_fault[MFV_COUNT];
/* N3 item 4: "did the driver hear a line of THIS onset" - set in radio_req */
static int      g_diag_said_fault = -1;
static double   g_diag_said_onset = -1.0;
static uint32_t g_repair_hud = 0;   /* N3 item 5: ENGINE REPAIRED rate */

static void warn_reset(void)
{
    int i;
    for (i = 0; i < W_COUNT; i++) {
        g_w_due[i] = 0; g_w_done[i] = 0; g_w_held[i] = 0;
        g_warn_blink[i].on = 0; g_instr_blink[i].on = 0;
    }
    g_radio_pre_done = 0;   /* 2026-10-01: a new fault gets its own call */
    memset(g_vcrit_after, 0, sizeof g_vcrit_after);   /* 2026-10-07 */
    voice_instr_clear_all();/* V3: the fault's instruction sub-slots are gone */
}

/* 1 when cond has held for a delay rolled in [min_ms, max_ms] */
static int held(int id, int cond, DWORD min_ms, DWORD max_ms)
{
    DWORD now = game_ms();
    g_w_pass[id] = g_lamp_pass;
    if (!cond) { g_w_due[id] = 0; g_w_held[id] = 0; return 0; }
    if (!g_w_due[id])
        g_w_due[id] = now + min_ms + (DWORD)(rand() % (int)(max_ms - min_ms + 1));
    /* L1 2026-10-05: cache the result for the native lamp pump */
    g_w_held[id] = (long)(now - g_w_due[id]) >= 0;
    return g_w_held[id];
}

static void warn_after(int id, int cond, DWORD min_ms, DWORD max_ms, const char *msg)
{
    /* R3c + N3 item 5: held detection window, then at most one show per
     * 120 s (diagnosis cadence); a genuinely new fault TYPE clears the due
     * stamp so its own diagnosis can post at once. */
    if (g_model_eng.fault != g_w_diag_fault[id]) {
        g_w_diag_fault[id] = g_model_eng.fault;
        g_warn_blink[id].due = 0;
    }
    hud_blink_p(&g_warn_blink[id], held(id, cond, min_ms, max_ms), msg,
                MF_HUD_REMIND_DIAG_MS);
}

/* Engineer instruction (owner 2026-09-29, simple English): once the
 * condition has held for the rolled delay, show msg. R3c: rising edge then
 * at most one reminder per 45 s while it holds (was 6 s every 20 s). */
static void instruct(int id, int cond, DWORD min_ms, DWORD max_ms, const char *msg)
{
    if (wrecked_or_stopped()) return;         /* E6: no instruction when wrecked */
    hud_blink(&g_instr_blink[id], held(id, cond, min_ms, max_ms), msg);
}

/* after one of our failures has killed the engine: "STOP THE CAR" */
static DWORD g_stop_at = 0;
static void engine_dead_msg(void) { g_stop_at = game_ms() + 3000; }
static void stop_msg_pump(void)
{
    DWORD now = game_ms();
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (!g_stop_at) return;
    /* 2026-10-01 audit: a live line - from 3 s after the death until the car
     * has stopped (or 8 s), then gone at once */
    if ((long)(now - g_stop_at) >= 8000) {
        g_stop_at = 0;
        hud_live("STOP THE CAR", 0);
        return;
    }
    hud_live("STOP THE CAR", (long)(now - g_stop_at) >= 0 && VEH_SPEED > 1.0f);
}

/* ---- radio (2026-10-01 messaging audit) ---------------------------------
 * Every mod radio call is a REQUEST into one queue (mf_msg.h): >= 15 s
 * between two calls, a death call drops pending older ones, nothing but a
 * death call goes out while the car is wrecked/stopped. radio_pump() says
 * at most one per pass. Budget per fault: ONE "pre-critical" call
 * (marshals / engineer notice), one critical call, one death call. */

static void radio_req(int eid, int prio)
{
    if (!on_circuit()) {   /* 2026-10-07: never queue a call off the circuit */
        logf_("RADIO DROPPED (not on circuit): eid %d", eid);
        return;
    }
    /* live 2026-10-05: "lift and coast" played after the engine had died (a
     * timed instruction fired after the death call). Once the engine is
     * finished only death calls are accepted. */
    if (prio != MFR_P_DEATH && (g_model_eng.culminated || g_model_eng.blown
                                || g_model_eng.stopped)) {
        logf_("RADIO DROPPED (engine finished): eid %d", eid);
        return;
    }
    /* N3 item 4: remember that a line of the current onset was actually
     * requested (the repair/clear gates ask "was the driver told?"). */
    if (prio < MFR_P_DEATH && g_model_live && g_model_eng.onset) {
        g_diag_said_fault = g_model_eng.fault;
        g_diag_said_onset = g_model_eng.onset_t_s;
    }
    if (g_engine_dead || g_model_eng.culminated || g_model_eng.blown || g_model_eng.stopped)
        mfr_terminal(&g_radio);
    mfr_req(&g_radio, eid, prio, game_ms());
    logf_("RADIO REQ eid %d prio %d (queue %d)", eid, prio, g_radio.n);
}

static void radio_pump(void)
{
    int eid;
    if (SESSION_OVER()) return;
    if (g_pump_frozen) return;   /* N3 item 14: no request while paused */
    if (g_engine_dead || g_model_eng.culminated || g_model_eng.blown || g_model_eng.stopped)
        mfr_terminal(&g_radio);
    eid = mfr_tick(&g_radio, game_ms(), wrecked_or_stopped());
    if (eid < 0) return;
    /* 2026-10-07 (owner): the new engineer lines play from loose WAVs; the
     * game's commentary table and SDT are never touched. */
    if (g_voice_on && mfv_line_of_eid(eid) >= 0) voice_play_wav(mfv_line_of_eid(eid));
    else commentary_say(eid);
}

static void __attribute__((unused)) say_after(int id, int cond, DWORD min_ms, DWORD max_ms, int eid)
{
    if (wrecked_or_stopped()) return;         /* E6: no radio when wrecked */
    if (g_w_done[id]) return;
    if (held(id, cond, min_ms, max_ms)) {
        g_w_done[id] = 1;
        radio_req(eid, MFR_P_INFO);
    }
}

/* the one pre-critical radio of a fault: the first condition to hold wins,
 * later pre-critical lines of the same fault stay silent */
static void say_pre(int id, int cond, DWORD min_ms, DWORD max_ms, int eid)
{
    if (g_radio_pre_done) return;
    if (wrecked_or_stopped()) return;
    if (g_w_done[id]) return;
    if (held(id, cond, min_ms, max_ms)) {
        g_w_done[id] = 1;
        g_radio_pre_done = 1;
        radio_req(eid, MFR_P_INFO);
    }
}

/* the critical call (engine dies soon): once per fault */
static void say_crit(int id, int cond, DWORD min_ms, DWORD max_ms, int eid)
{
    if (wrecked_or_stopped()) return;
    if (g_w_done[id]) return;
    if (held(id, cond, min_ms, max_ms)) {
        g_w_done[id] = 1;
        radio_req(eid, MFR_P_CRIT);
    }
}

/* a critical HUD line: edge + one reminder per 90 s, not when wrecked */
static void critical(int id, int cond, DWORD min_ms, DWORD max_ms, const char *msg)
{
    if (wrecked_or_stopped()) return;
    hud_blink_p(&g_instr_blink[id], held(id, cond, min_ms, max_ms), msg,
                MF_HUD_REMIND_CRIT_MS);
}

/* N3 2026-10-06 (item 8): a critical (HUD red line + its radio) never fires
 * before the onset's diagnosis was shown/said. When the critical condition is
 * already true and no diagnosis is out yet, post the diagnosis text once,
 * then allow the critical >= 5 s later (fixes H14 ordering). State resets
 * when the fault TYPE changes; repair/stint call warn_reset(). */
#define MF_CRIT_DIAG_GAP_MS 5000u
static int crit_gate(int id, int crit, int diag_natural,
                     const char *diag_text, DWORD now)
{
    if (g_crit_fault[id] != g_model_eng.fault) {
        g_crit_fault[id] = g_model_eng.fault;
        g_crit_diag_at[id] = 0;
        g_crit_diag_forced[id] = 0;
    }
    if (diag_natural && !g_crit_diag_at[id]) g_crit_diag_at[id] = now;
    if (!crit) return 0;
    if (!g_crit_diag_at[id]) {
        g_crit_diag_at[id] = now;
        g_crit_diag_forced[id] = 1;
        hud_show("%s", diag_text);
        logf_("NOTIFY: %s diagnosis posted before its critical", diag_text);
    }
    return mfn_crit_allowed(crit, g_crit_diag_at[id] != 0, now,
                            g_crit_diag_at[id], MF_CRIT_DIAG_GAP_MS);
}

/* ---- V3: engineer voice lines (mf_voice.h; Claude spec 2026-10-05) --------
 * At load (only when the installed SDT carries the 35 appended clips and the
 * stitch-type guard passes) the 35 donor eids' score->clip lookup entries are
 * repointed to the new clip indices 316..350; restored on unload. The lines
 * are fired through the normal commentary queue (the score is re-read from
 * the event table at play time). When the lines are OFF every call site keeps
 * today's vanilla eid as its fallback. */
#define CM_LOOKUP_PWS   ((volatile unsigned short*)0x005BEBB0u)
#define CM_STITCH_BASE  ((volatile int*)(0x005BEE08u + 4u * MFV_SDT_BASE))
#define CM_STITCH_MIN   0x40
/* 2026-10-06 release: the game's own SDT, next to the running game EXE
 * (was a fixed path on the development PC). 0 = no path. */
static int voice_sdt_path(char *out)
{
    char *sl;
    DWORD n = GetModuleFileNameA(NULL, out, MAX_PATH);
    if (!n || n >= MAX_PATH || !(sl = strrchr(out, '\\'))) return 0;
    if ((size_t)(sl + 1 - out) + 40 >= MAX_PATH) return 0;
    strcpy(sl + 1, "Audio\\Commentary\\Pws\\PC_RE_MAIN.SDT");
    return 1;
}

static MfVoicePlan g_voice_plan;    /* V3b: saved/written rows + lookup entries */
static MfInstrSlot g_vinstr[MFV_COUNT];
static int g_vinstr_fault[MFV_COUNT];
static double g_vinstr_onset[MFV_COUNT];
static int g_voice_stab_seen = 0;   /* CLEAR01 once per model onset */

static void voice_instr_clear_all(void)
{
    int i;
    for (i = 0; i < MFV_COUNT; i++) {
        int q;
        if (g_vinstr[i].armed) for (q = 0; q < g_radio.n; ) {
            if (g_radio.q[q].eid == g_vinstr[i].eid
                && g_radio.q[q].prio < MFR_P_DEATH) mfr_del(&g_radio, q);
            else ++q;
        }
        mfi_clear(&g_vinstr[i]);
    }
}

/* arm one instruction line directly (its own delay window) */
static void voice_arm_line(int line)
{
    uint32_t lo, hi;
    if (!g_voice_on || line < 0 || line >= MFV_COUNT) return;
    mfv_instr_delay_ms(line, &lo, &hi);
    mfi_arm(&g_vinstr[line], mfv_eid(line), (uint32_t)rand(), game_ms(), lo, hi);
    g_vinstr_fault[line] = g_model_eng.fault;
    g_vinstr_onset[line] = g_model_eng.onset_t_s;
    logf_("VOICE: %s scheduled", mfv_id(line));
}

/* arm the 2nd line of a two-line row after its 1st line went out */
static void voice_arm_instr(int line)
{
    int next = mfv_instr_of(line);
    int alt = mfv_instr_alt_of(line);
    if (next < 0) return;
    if (alt >= 0 && (rand() & 1)) next = alt;   /* "pick one at random" */
    voice_arm_line(next);
}

/* read the installed SDT header: 1 when it has >= 351 clips and clip 316 is
 * PWS_MF01. Read-only, never writes the file (V3 spec). */
static int __attribute__((unused)) voice_sdt_installed(void)
{
    HANDLE h;
    DWORD got = 0;
    unsigned char buf[4 + 4 * 351];
    unsigned int count, off;

    char sdt[MAX_PATH];
    if (!voice_sdt_path(sdt)) return 0;
    h = CreateFileA(sdt, GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!ReadFile(h, buf, 4, &got, NULL) || got != 4) { CloseHandle(h); return 0; }
    count = *(unsigned int*)buf;
    if (count < MFV_SDT_BASE + MFV_COUNT) { CloseHandle(h); return 0; }
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    if (!ReadFile(h, buf, 4 + 4 * 351, &got, NULL) || got < 4 + 4 * 351) {
        CloseHandle(h);
        return 0;
    }
    off = *(unsigned int*)(buf + 4 + 4 * MFV_SDT_BASE);
    SetFilePointer(h, (LONG)off, NULL, FILE_BEGIN);
    if (!ReadFile(h, buf, 40, &got, NULL) || got != 40) { CloseHandle(h); return 0; }
    CloseHandle(h);
    return memcmp(buf + 8, MFV_SDT_NAME0, 8) == 0;
}

static int __attribute__((unused)) voice_install_transaction(void)
{
    int i;
    int scores[MFV_COUNT];
    unsigned stitch[MFV_COUNT];
    MfVoiceRow rows[MFV_COUNT], ref;
    DWORD refrow = CM_EVENT_TABLE + 77u * 0x14u;

    if (!mfv_check()) return 0;

    /* V3b item 1 (ADDENDUM 3): read and VALIDATE every donor first - an
     * invalid donor (score range or stitch guard) writes NOTHING. */
    for (i = 0; i < MFV_COUNT; i++) {
        int eid = mfv_eid(i);
        DWORD row = CM_EVENT_TABLE + (DWORD)eid * 0x14u;
        scores[i] = *(volatile int*)(row + 0x10u);
        stitch[i] = (unsigned)CM_STITCH_BASE[i];   /* ADDENDUM 1 A, read-only */
        rows[i].pri        = *(volatile int*)(row + 0x00u);
        rows[i].delay_bits = *(volatile int*)(row + 0x04u);
        rows[i].f3_bits    = *(volatile int*)(row + 0x08u);
        rows[i].cd_bits    = *(volatile int*)(row + 0x0Cu);
        rows[i].score      = scores[i];
    }
    if (!mfv_plan_validate(scores, stitch)) return 0;
    /* V3b item 2: parity fields from eid 77's CURRENT runtime row (read at
     * load, not constants); the score is never touched. */
    ref.pri        = *(volatile int*)(refrow + 0x00u);
    ref.delay_bits = *(volatile int*)(refrow + 0x04u);
    ref.f3_bits    = *(volatile int*)(refrow + 0x08u);
    ref.cd_bits    = *(volatile int*)(refrow + 0x0Cu);
    ref.score      = *(volatile int*)(refrow + 0x10u);
    mfv_plan_fill(rows, &ref, &g_voice_plan);

    for (i = 0; i < MFV_COUNT; i++) {
        int eid = mfv_eid(i);
        DWORD row = CM_EVENT_TABLE + (DWORD)eid * 0x14u;
        *(volatile int*)(row + 0x00u) = g_voice_plan.wrote[i].pri;
        *(volatile int*)(row + 0x04u) = g_voice_plan.wrote[i].delay_bits;
        *(volatile int*)(row + 0x08u) = g_voice_plan.wrote[i].f3_bits;
        *(volatile int*)(row + 0x0Cu) = g_voice_plan.wrote[i].cd_bits;
        g_voice_plan.lk_saved[i] = CM_LOOKUP_PWS[scores[i]];
        g_voice_plan.lk_wrote[i] = (unsigned short)(MFV_SDT_BASE + i);
        CM_LOOKUP_PWS[scores[i]] = g_voice_plan.lk_wrote[i];
    }
    g_voice_on = 1;
    return 1;
}
/* 2026-10-06 ship: mechanical_failures.ini next to the DLL (0 = no path) */
static int mf_user_ini_path(char *ini)
{
    char *sl;
    DWORD n = GetModuleFileNameA(g_hinst, ini, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    sl = strrchr(ini, '\\');
    if (!sl || (size_t)(sl + 1 - ini) + 24 >= MAX_PATH) return 0;
    strcpy(sl + 1, "mechanical_failures.ini");
    return 1;
}

/* 2026-10-07 (owner, tester report): engineer lines are played straight
 * from loose WAVs (<game>\Audio\MechanicalFailures\MF_*.wav,
 * like the game's own pws<Name> clips) through the mod's private player.
 * The SDT and the native commentary tables are no longer modified - the
 * old remap made other callers of the donor events play mod lines and
 * depended on the player's SDT matching ours. The donor eids remain only
 * as radio-queue tokens. */
static int voice_wav_path(char *path, int line)
{
    char file[64];
    _snprintf(file, sizeof file, "%s.wav", mfv_id(line));
    file[sizeof file - 1] = '\0';
    return mf_sound_path(path, file)
        && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static void voice_play_wav(int line)
{
    char path[MAX_PATH];
    if (!on_circuit()) {
        logf_("RADIO DROPPED (not on circuit): %s", mfv_id(line));
        return;
    }
    if (!voice_wav_path(path, line)) {
        logf_("VOICE: %s missing - line skipped", mfv_id(line));
        return;
    }
    /* 2026-10-07 audit MF-10: INI VoiceVolume=0 means off on every path */
    if (mfa_voice_user <= 0.0f) {
        logf_("VOICE: %s muted (VoiceVolume 0)", mfv_id(line));
        return;
    }
    if (voice_native_say(line)) return;   /* the game's own commentary player */
    mf_audio_play_voice(path, 1.0f);
    logf_("VOICE: %s played (private player) (%s)", mfv_id(line), mfv_text(line));
}

static void voice_install(void)
{
    char ini[MAX_PATH], path[MAX_PATH];
    int i, v;
    /* owner 2026-10-06: the player can switch the new engineer lines off;
     * the mod then uses the game's own engineer lines (fallback eids). */
    if (mf_user_ini_path(ini)) {
        if (GetPrivateProfileIntA("Sound", "EngineerVoice", 1, ini) == 0) {
            logf_("VOICE: new engineer lines OFF (INI EngineerVoice=0) - game's own lines");
            return;
        }
        v = GetPrivateProfileIntA("Sound", "VoiceVolume", 100, ini);
        if (v < 0) v = 0;
        if (v > 200) v = 200;
        mfa_voice_user = (float)v / 100.0f;
        if (v != 0 && v != 100)
            logf_("VOICE: VoiceVolume %d%% applies to the private player only; the native "
                  "path follows the game's Announcer volume", v);
    }
    if (!mfv_check()) { logf_("VOICE: mapping check failed - OFF"); return; }
    for (i = 0; i < MFV_COUNT; i++)
        if (!voice_wav_path(path, i)) {
            logf_("VOICE: %s.wav not found - new lines OFF, game's own lines", mfv_id(i));
            return;
        }
    g_voice_on = 1;
    voice_native_install();
    logf_("VOICE: %d engineer WAVs found - new lines ON (loose WAV, SDT untouched, "
          "VoiceVolume %d%%)", MFV_COUNT, (int)(mfa_voice_user * 100.0f + 0.5f));
}

static void voice_restore(void)
{
    g_voice_on = 0;   /* 2026-10-07: no tables written; the detour is removed at unload */
}

/* pre-2026-10-07 SDT remap restore, kept for reference only (unused) */
static void __attribute__((unused)) voice_restore_sdt_remap(void)
{
    MfThreadGate gate; int i, restored = 0, busy = 0;
    if (!g_voice_on) return;
    if (!mft_begin(&gate, 0, 0)) {
        g_voice_on = 0;
        logf_("VOICE: restore thread gate refused (%s, 0x%lX); table retained, module pinned",
              mft_reason(&gate), gate.detail); return;
    }
    for (i = 0; i < MFV_COUNT; ++i)
        if (*(volatile int*)CM_QHEAD == mfv_eid(i)) busy = 1;
    if (!busy) for (i = 0; i < MFV_COUNT; ++i) {
        DWORD row = CM_EVENT_TABLE + (DWORD)mfv_eid(i) * 0x14u;
        int score = g_voice_plan.saved[i].score;
        MfVoiceRow cur;
        cur.pri = *(volatile int*)(row + 0x00u);
        cur.delay_bits = *(volatile int*)(row + 0x04u);
        cur.f3_bits = *(volatile int*)(row + 0x08u);
        cur.cd_bits = *(volatile int*)(row + 0x0Cu);
        cur.score = *(volatile int*)(row + 0x10u);
        if (score < 0 || score >= 300
            || !mfv_restore_owned(&cur, &g_voice_plan, i, CM_LOOKUP_PWS[score])) continue;
        *(volatile int*)(row + 0x00u) = g_voice_plan.saved[i].pri;
        *(volatile int*)(row + 0x04u) = g_voice_plan.saved[i].delay_bits;
        *(volatile int*)(row + 0x08u) = g_voice_plan.saved[i].f3_bits;
        *(volatile int*)(row + 0x0Cu) = g_voice_plan.saved[i].cd_bits;
        CM_LOOKUP_PWS[score] = g_voice_plan.lk_saved[i];
        ++restored;
    }
    g_voice_on = 0;
    mft_end(&gate);
    logf_("VOICE: restore %d/%d whole rows + saved-score lookups; %s",
          restored, MFV_COUNT, busy ? "queued donor: retained, pinned" : "changed rows left alone");
}

/* pre-critical line; keeps the vanilla eid when the new lines are OFF */
static void voice_pre(int id, int cond, DWORD min_ms, DWORD max_ms,
                      int line, int fb_eid, int arm_pair)
{
    int e;
    if (wrecked_or_stopped()) return;
    e = g_voice_on ? mfv_eid(line) : -1;
    if (e < 0) {
        if (fb_eid >= 0) say_pre(id, cond, min_ms, max_ms, fb_eid);
        return;
    }
    if (g_radio_pre_done || g_w_done[id]) return;
    if (held(id, cond, min_ms, max_ms)
        && mfm_rate_ok_fault(&g_v_rate[line], &g_v_rate_fault[line],
                             g_model_eng.fault, game_ms(), 120000)) {
        g_w_done[id] = 1;
        g_radio_pre_done = 1;
        radio_req(e, MFR_P_INFO);
        logf_("VOICE: %s (pre)", mfv_id(line));
        if (arm_pair) voice_arm_instr(line);
    }
}

/* critical line; keeps the vanilla eid when the new lines are OFF */
static void voice_crit(int id, int cond, DWORD min_ms, DWORD max_ms,
                       int line, int fb_eid)
{
    int e;
    if (wrecked_or_stopped()) return;
    e = g_voice_on ? mfv_eid(line) : -1;
    if (e < 0) {
        if (fb_eid >= 0) say_crit(id, cond, min_ms, max_ms, fb_eid);
        return;
    }
    if (g_w_done[id]) return;
    if (held(id, cond, min_ms, max_ms)
        && mfm_rate_ok_fault(&g_v_rate[line], &g_v_rate_fault[line],
                             g_model_eng.fault, game_ms(), 120000)) {
        g_w_done[id] = 1;
        radio_req(e, MFR_P_CRIT);
        logf_("VOICE: %s (critical)", mfv_id(line));
    }
}

/* 2026-10-07 live test (runs 2 and 3): "Coolant low, box this lap" and
 * "Fuel leak, box now" played BEFORE the fault's first diagnosis line - the
 * rule is a critical line never before its diagnosis. When the critical
 * becomes due and this fault has had no pre line yet, say the diagnosis
 * first and hold the critical VCRIT_AFTER_PRE_MS so the queue keeps the
 * order (the critical has the higher radio priority). */
#define VCRIT_AFTER_PRE_MS 6000u
static void voice_crit_after_pre(int id, int cond, DWORD min_ms, DWORD max_ms,
                                 int line, int fb_eid, int pre_id, int pre_line)
{
    if (g_voice_on && cond && !g_w_done[id] && !wrecked_or_stopped()) {
        if (!g_radio_pre_done && !g_w_done[pre_id]) {
            g_w_done[pre_id] = 1;
            g_radio_pre_done = 1;
            radio_req(mfv_eid(pre_line), MFR_P_INFO);
            logf_("VOICE: %s (pre, said before its critical %s)",
                  mfv_id(pre_line), mfv_id(line));
            g_vcrit_after[id] = game_ms() + VCRIT_AFTER_PRE_MS;
            return;
        }
        if (g_vcrit_after[id] && (long)(game_ms() - g_vcrit_after[id]) < 0)
            return;                         /* let the diagnosis play first */
    }
    voice_crit(id, cond, min_ms, max_ms, line, fb_eid);
}

/* one-shot voice call (save / repair / cleared / hit / radiator clear) */
static void voice_say(int line, int prio)
{
    if (!g_voice_on) return;
    if (wrecked_or_stopped() && prio < MFR_P_DEATH) return;
    if (line < 0 || line >= MFV_COUNT) return;
    /* N3 2026-10-06 (item 5): same MFV line at most once per 120 s; death
     * lines are exempt and a genuinely new fault TYPE may say it at once */
    if (prio < MFR_P_DEATH
        && !mfm_rate_ok_fault(&g_v_rate[line], &g_v_rate_fault[line],
                              g_model_eng.fault, game_ms(), 120000)) {
        logf_("VOICE: %s rate-blocked (120 s)", mfv_id(line));
        return;
    }
    radio_req(mfv_eid(line), prio);
    logf_("VOICE: %s (%s)", mfv_id(line), prio >= MFR_P_CRIT ? "crit" : "info");
}

/* an instruction-only line (no pre line of its own) */
static void voice_instr(int id, int cond, DWORD min_ms, DWORD max_ms, int line)
{
    if (!g_voice_on || wrecked_or_stopped()) return;
    if (g_w_done[id]) return;
    if (held(id, cond, min_ms, max_ms)
        && mfm_rate_ok_fault(&g_v_rate[line], &g_v_rate_fault[line],
                             g_model_eng.fault, game_ms(), 120000)) {
        g_w_done[id] = 1;
        voice_arm_line(line);
    }
}

/* every pass: fire due instruction sub-slots; CLEAR01; the DEFENSIVE ENG01 */
static void voice_pump(void)
{
    int i, e;
    if (SESSION_OVER()) return;
    if (!g_voice_on) return;
    if (g_model_eng.culminated || g_model_eng.blown || g_model_eng.stopped) {
        voice_instr_clear_all();          /* live 2026-10-05: nothing after death */
        return;
    }
    if (!g_model_live || !g_model_eng.onset || g_model_eng.stabilised)
        voice_instr_clear_all();
    for (i = 0; i < MFV_COUNT; i++) {
        int same = g_vinstr_fault[i] == g_model_eng.fault
                   && g_vinstr_onset[i] == g_model_eng.onset_t_s;
        int active = g_model_live && g_model_eng.onset && !g_model_eng.stabilised;
        int eligible = mfv_instruction_eligible(i, active, same,
            g_model_out.warn_valve_float, g_model_out.warn_cool_engine,
            mfn_oil_gauge_low(&g_model_out) || g_model_out.warn_oil_pressure,
            g_model_eng.fault == MF_FAULT_RAD_DEBRIS && g_model_eng.f_rad < 0.999,
            g_model_out.warn_cool_oil);
        uint32_t now = (uint32_t)game_ms();
        if (!active || !same) { mfi_clear(&g_vinstr[i]); continue; }
        if (!mfi_due(&g_vinstr[i], now)) continue;
        /* N3 2026-10-06: R7 hold-until-eligible (max 120 s, same fault+onset
         * via the clear above) with a drop log; the per-line 120 s rate
         * consumes only when the line actually fires. */
        {
            int rate = 1;
            if (eligible)
                rate = mfm_rate_ok_fault(&g_v_rate[i], &g_v_rate_fault[i],
                                         g_model_eng.fault, now, 120000);
            e = mfi_tick_if(&g_vinstr[i], now, eligible && rate);
        }
        if (e == MFI_DROPPED) {
            logf_("VOICE: %s dropped (not eligible within %u s)",
                  mfv_id(i), MFI_HOLD_MAX_MS / 1000u);
        } else if (e >= 0) {
            radio_req(e, MFR_P_INFO);
            logf_("VOICE: %s (instruction)", mfv_id(i));
            if (mfv_instr_of(i) >= 0) voice_arm_instr(i);   /* OILLEAK chain */
        }
    }
    /* a fault that stabilises on its own: one CLEAR01 per onset */
    if (g_model_live && g_model_eng.onset && g_model_eng.stabilised) {
        if (!g_voice_stab_seen) {
            g_voice_stab_seen = 1;
            voice_say(MFV_CLEAR01, MFR_P_INFO);
        }
    } else if (!g_model_live || !g_model_eng.onset) {
        g_voice_stab_seen = 0;
    }
}

static void blow_burst_start(void);
static void burn_arm(int kind);            /* R3q: post-death burn, below */
static void burn_clear(void);              /* R3q */

/* ---- the levers -------------------------------------------------------- */

/* E8b: 1 once the mod writes ENG_LIFE / ENG_STRESS / ENG_BANK1-3. The stint
 * clear may then reset those channels; 0 means the values in them (e.g. the
 * game's own staged failure: life re-armed, stress and banks the game wrote)
 * are not ours and are left alone. Reset at every session change. */
static int g_native_touched = 0;

/* R1b (ADDENDUM 9, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): 1 when the game
 * sim clock did not advance since the previous worker pass (pause, menus).
 * Every lever pump returns early while frozen: no game writes, no logs, and
 * every hold state stays frozen. Set once per pass in the worker loop. */
static int   g_pump_frozen = 0;
static DWORD g_pump_prev_ms = 0;
static int   g_pump_prev_valid = 0;

/* ---- E9g-e item 1: the engine-dead latch --------------------------------
 * A terminal failure (our seizure/blow, or the model's no-bang stop) latches
 * here. While latched the FUELCUT detour returns 0 torque (item 2), the life
 * hold keeps ENG_LIFE <= 0 (item 3) and the rev band is not restored (item
 * 4). Cleared only by repair, a session change and unload. The ignition byte
 * is never written: the dash lights stay on, as in the game's own crash
 * seizure (finding engine-seized-state-life-zero-ignition-on). */
static volatile int g_engine_dead = 0;
static int    g_dead_hold_logged = 0;   /* first life-hold write per latch */
static int    g_dead_rev_logged = 0;    /* starter/rev attempt, once per latch */
static double g_dead_prev_rpm = 0.0;    /* crossing detector for the starter log */
/* 2026-10-06 owner live: panel showed 'ENGINE DEAD - engine' - the model's
 * reason exists only in the death step. Latch it with the death. */
static char   g_dead_why[48] = "";

static void engine_dead_latch(const char *why)
{
    if (g_engine_dead) return;
    g_engine_dead = 1;
    mfr_terminal(&g_radio);
    voice_instr_clear_all();
    burn_arm(2);                 /* R3q: seize/stop smoke tail (blow upgrades) */
    g_dead_hold_logged = 0;
    g_dead_rev_logged = 0;
    g_dead_prev_rpm = (double)ENG_RPM_LIVE;
    lstrcpynA(g_dead_why, why ? why : "", (int)sizeof g_dead_why);
    logf_("DEAD: latched (%s)", why);
}

static void engine_dead_clear(const char *why)
{
    if (!g_engine_dead) return;
    g_engine_dead = 0;
    g_dead_why[0] = '\0';
    burn_clear();                /* R3q: banks back to 0 on repair/session */
    logf_("DEAD: cleared (%s)", why);
}

/* E9g-e item 3: while dead, hold life at <= 0. The game may re-arm life in
 * the same call (finding native-life-zero-rearm-same-call); a native value
 * already <= 0 is left untouched and ENG_STRESS is never written. The first
 * hold write per latch is logged. Item 6: if the revs climb back above idle
 * while dead (starter key or a game re-arm trying to revive it), log once -
 * torque stays 0 through the detour regardless. */
static void engine_dead_pump(void)
{
    double rpm, idle;

    if (!g_engine_dead) return;
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */

    if (ENG_LIFE > 0.0) {
        double was = ENG_LIFE;
        ENG_LIFE = 0.0;
        g_native_touched = 1;     /* E8b: mod-written channel */
        if (!g_dead_hold_logged) {
            g_dead_hold_logged = 1;
            logf_("DEAD: life hold (re-armed %.1f -> 0)", was);
        }
    }
    rpm = (double)ENG_RPM_LIVE;
    idle = (g_model_car_valid && g_model_car.idle_hi_rads > 1.0)
         ? g_model_car.idle_hi_rads
         : (IDLE_RPM_HI > 100.0 ? IDLE_RPM_HI : 200.0);
    if (!g_dead_rev_logged && g_dead_prev_rpm <= idle && rpm > idle) {
        g_dead_rev_logged = 1;
        logf_("DEAD: starter/rev attempt blocked");
    }
    g_dead_prev_rpm = rpm;
}

/* E8g item 8 (test 4 fog diagnostic): log every meaningful ENG_BANK1 change
 * with its source, so white fog with no matching line is proven external to
 * the mod. Tags used: oil, cooling, overheat, repair, model-stop (the blow
 * path writes ENG_BANK2/3 - there is no bank-1 blow write). */
static double g_bank1_last = 0.0;
static void bank1_set(double v, const char *src)
{
    double d;
    /* E9a: once the game has re-armed its OWN staged failure (native stage
     * one), never dilute the game's smoke - write only values denser than
     * the game's current bank 1 (our smoke density is negative). */
    if (g_model_eng.native_stage1 && v > ENG_BANK1) {
        g_bank1_last = ENG_BANK1;
        return;
    }
    d = v - g_bank1_last;
    if (d > 0.05 || d < -0.05 || ((v > 0.0) != (g_bank1_last > 0.0)))
        logf_("FOG: bank1 %.2f -> %.2f (%s)", g_bank1_last, v, src);
    g_bank1_last = v;
    ENG_BANK1 = v;
}

/* E8h: realistic mechanical stop. A native crash seizure leaves the ignition
 * byte (ENG_RUNNING) = 1 with ENG_LIFE = 0 - the game stops the engine
 * because VehicleActive needs life > 0, and the dash lights stay on. The old
 * kill cleared the ignition byte (lights off - wrong for a mechanical
 * failure). This never touches ENG_RUNNING; only electrical failures do. */
static void engine_seize(const char *why)
{
    engine_dead_latch(why);      /* E9g-e item 1: terminal - torque goes to 0 */
    g_native_touched = 1;
    ENG_LIFE = 0.0;
    ENG_STRESS = 0.0;
    if (REASON_CODE == 0) REASON_CODE = 1;
    logf_("SEIZE: %s", why);
}

/* Stage one: expires in ~1.5 s. The game then sets ReasonCode 1, rolls one
 * damage bank, fires the radio line, and re-arms itself. White smoke, car
 * still drivable. This is the vanilla progressive failure, entered early. */
static void arm_engine_stage_one(void)
{
    save_life_once();
    g_native_touched = 1;
    ENG_LIFE = ENG_STAGE_ONE;
    logf_("ENGINE stage-one armed: life = %.2f", ENG_STAGE_ONE);
    hud_show("ENGINE PROBLEM");
}

/* Terminal: drop below the -15.4 threshold and let the game ramp stress to
 * its own 0.6 cap. Slow death, ~100 s. Nothing is forged - the ramp, the
 * bank halving at -10.1 and the cap are all the game's own code.        */
static void arm_engine_terminal(void)
{
    save_life_once();
    g_native_touched = 1;
    if (ENG_STRESS <= 0.0)
        ENG_STRESS = 0.05;    /* ramp branch requires stress > 0 to run */
    ENG_LIFE = ENG_TERMINAL;
    logf_("ENGINE terminal armed: life = %.2f, stress = %.4f",
          ENG_TERMINAL, ENG_STRESS);
    /* realism 2026-09-28 (R1): the driver feels power going, not a verdict;
     * ENGINE FAILURE is shown by wear_watch_pump() when the engine dies */
    hud_show("ENGINE PROBLEM");
    g_wear_watch = 1;
}

/* Instant: same terminal region, but jump stress straight to the cap the
 * game would have reached anyway. The one place we skip ahead, because
 * waiting 100 s is not useful for a manual test.
 * E8g item 6: kept for the legacy stage buttons only - the panel BLOW UP
 * button now arms the model terminal (model_manual_blow), so this lever has
 * no live caller. */
static void __attribute__((unused)) arm_engine_instant(void)
{
    save_life_once();
    g_native_touched = 1;
    ENG_LIFE   = ENG_TERMINAL;
    ENG_STRESS = ENG_STRESS_CAP;
    logf_("ENGINE instant: life = %.2f, stress = %.2f (cap)",
          ENG_TERMINAL, ENG_STRESS_CAP);
    hud_show("ENGINE BLOWN");
    mfv_publish("engine", "ENGINE BLOWN (instant, no build-up)");
    engine_dead_latch("instant blow");   /* E9g-e item 1: legacy instant path */
    blow_burst_start();
}

/* ---- emitters ----------------------------------------------------------
 * The three "damage banks" are three INDEPENDENT particle emitters, not
 * severity stages. Isolation-proven live 2026-07-30: each armed alone at
 * +0.85 with the other two at exactly 0.0 produced exactly one visual.
 *     0x7C920C  this+7652  WHITESMOKE   (emitter id 1)
 *     0x7C9214  this+7660  BLACKSMOKE   (id 3)
 *     0x7C921C  this+7668  FLAMES       (id 4)
 * Ids from MaterialShaderReactionEffectParser_004ca480. BLUESMOKE (id 2) is
 * not part of the engine trio.
 *
 * A POSITIVE bank is used directly as the emission rate, so it renders even
 * at a standstill. A NEGATIVE bank is multiplied by the engine-load term
 * v19 = clamp(-1/this[7388]*rpm*torque, -1, 0), so it only shows under load
 * - which is why the first white-smoke run (bank1 = -0.325) needed driving.
 * All three at exactly 0.0 is the ONLY state that disables the emitter.  */
#define EMIT_ON  0.85

static void emitter_toggle(int which)
{
    static const char *nm[3] = { "WHITE SMOKE", "BLACK SMOKE", "FIRE" };
    volatile double *bank;
    int on;

    switch (which) {
        case 0:  bank = &ENG_BANK1; break;
        case 1:  bank = &ENG_BANK2; break;
        case 2:  bank = &ENG_BANK3; break;
        default: return;
    }

    on = (*bank == 0.0);          /* off -> on, anything else -> off */
    *bank = on ? EMIT_ON : 0.0;
    g_native_touched = 1;         /* E8b: mod-written bank */
    logf_("EMITTER %s %s (bank = %.2f)", nm[which], on ? "ON" : "OFF", *bank);
}

static void emitters_clear(void)
{
    ENG_BANK1 = 0.0;
    ENG_BANK2 = 0.0;
    ENG_BANK3 = 0.0;
    g_native_touched = 1;         /* E8b: mod-written banks */
    logf_("EMITTERS all cleared");
}

/* Binary seizure. Not the wear system at all - this is the equivalent of the
 * damage.ini [PHYSICAL] Engine= collision outcome: engine simply stops.
 * No smoke, no misfire, no staging. ReasonCode is FORCED to 1 (Engine), not
 * conditional - a stale nonzero code from an earlier test (suspension,
 * gearbox, whatever) previously left this silently classified as the wrong
 * failure, which is why retire_commentary_pump's ReasonCode==1 gate refused
 * to fire on a real 2026-08-07 test (found stale ReasonCode=4). Pressing
 * this key is an explicit "this is an engine failure now".              */
static void arm_seizure(void)
{
    /* E9g item 3: mechanical stop through engine_seize (life 0, ignition stays
     * lit like a native seizure). An explicit panel seizure still forces
     * ReasonCode 1 - a stale code from another family must not misclassify it. */
    engine_seize("binary seizure (panel SEIZE)");
    REASON_CODE = 1;
    retire_pending_arm();
    logf_("ENGINE SEIZED (binary): life 0, ignition stays, ReasonCode forced to 1");
    hud_show("ENGINE STOPPED");
}

/* ---- intermittent ignition cutout (misfire glitch) ---------------------
 * Reuses the FACT ignition byte ENG_RUNNING (0x7C9081, driver-confirmed
 * 2026-07-30 to behave identically to the real ignition key). Num6/arm_seizure
 * holds it at 0 permanently; this instead flicks it off for a short random
 * burst, then back on, on a random interval - a stuttering/spluttering
 * misfire rather than a dead stop. Nothing forged beyond the one byte the
 * game already tests every tick in sub_447B10.                            */
#define CUTOUT_DUR_MIN_MS   80
#define CUTOUT_DUR_MAX_MS   280
#define CUTOUT_GAP_MIN_MS   800
#define CUTOUT_GAP_MAX_MS   3000

static int   g_cutout_on   = 0;
static DWORD g_cutout_until = 0;   /* current cut ends at this tick, 0 = not cutting */
static DWORD g_cutout_next  = 0;   /* next cut starts at this tick */

static int rand_range(int lo, int hi)
{
    return lo + (rand() % (hi - lo + 1));
}

static void cutout_pump(void)
{
    DWORD now;                    /* E9o: the ignition flicker is simulation time */
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no ignition writes, no logs */
    now = game_ms();

    if (!g_cutout_on) return;

    if (g_cutout_until) {                       /* currently mid-cut */
        ENG_RUNNING = 0;
        if (now >= g_cutout_until) {
            ENG_RUNNING = 1;
            g_cutout_until = 0;
            g_cutout_next = now + (DWORD)rand_range(CUTOUT_GAP_MIN_MS, CUTOUT_GAP_MAX_MS);
        }
        return;
    }

    if (now >= g_cutout_next) {
        g_cutout_until = now + (DWORD)rand_range(CUTOUT_DUR_MIN_MS, CUTOUT_DUR_MAX_MS);
        logf_("CUTOUT: misfire, ignition dropped for %lu ms",
              (unsigned long)(g_cutout_until - now));
    }
}

static void toggle_cutout(void)
{
    g_cutout_on = !g_cutout_on;
    if (g_cutout_on) {
        g_cutout_until = 0;
        g_cutout_next = game_ms() + (DWORD)rand_range(CUTOUT_GAP_MIN_MS, CUTOUT_GAP_MAX_MS);
        logf_("CUTOUT armed");
    } else {
        g_cutout_until = 0;
        ENG_RUNNING = 1;   /* leave the engine running, never mid-cut on exit */
        logf_("CUTOUT cleared");
    }
}

/* ---- E9g-b item 1: ownership-checked restores ---------------------------
 * Every saved-and-restored game value keeps the stock value captured before
 * our first write (`saved`) and the value we wrote most recently (`last`).
 * A restore writes `saved` back only while the field still shows OUR last
 * value; if the game (fresh session physics, a changed setup, another mod)
 * replaced it, writing stock back would stamp stale values over fresh ones,
 * so the restore writes nothing, drops `saved` and logs instead. */
typedef struct {
    double   saved;   /* stock value captured before our first write */
    double   last;    /* value we wrote most recently               */
    int      held;    /* 1 = we wrote and still own the field       */
    unsigned gen;     /* E9-H1c: session generation of the capture  */
} MfLever;

/* Record a write: the first call captures the stock value. */
static void lever_write(MfLever *l, volatile double *addr, double v)
{
    if (!l->held) {
        l->saved = *addr;
        l->held = 1;
        l->gen = g_session_gen;    /* H1c: the generation this record is from */
    }
    *addr = v;
    l->last = v;
}

/* Restore the stock value if we still own the field AND the record's session
 * generation is the current one and the session is not over (H1c). A stale
 * record is dropped without writing (log once), never applied to a newer
 * engine/session. Returns 1 when the stock value was written. */
static int lever_restore(MfLever *l, volatile double *addr, const char *name)
{
    int wrote = 0;

    if (!l->held) return 0;
    if (l->gen != g_session_gen || SESSION_OVER()) {
        logf_("RESTORE DROPPED %s: %s", name,
              l->gen != g_session_gen ? "stale session generation"
                                      : "session over");
        l->held = 0;
        l->saved = 0.0;
        l->last = 0.0;
        l->gen = 0;
        return 0;
    }
    if (*addr == l->last) {
        *addr = l->saved;
        wrote = 1;
    } else {
        logf_("RESTORE SKIPPED %s: game value changed (%.4f)", name, *addr);
    }
    l->held = 0;
    l->saved = 0.0;
    l->last = 0.0;
    l->gen = 0;
    return wrote;
}

/* ---- E8b: extra oil heat through the game's OWN heat input ---------------
 * EngineStressSimulation (0x43EF20) integrates
 *   oilT += fuel_term x COMB_HEAT + |rpm| x SPEED_HEAT + (ambient - oilT) x MIN_COOL
 * every physics tick, so writing a temperature is both unnecessary and
 * intrusive. Instead scale the game's EngineSpeedHeat coefficient
 * (this+7524): scale = 1 + ks / max(stock x |rpm|, 1e-6), clamped 1..4; the
 * extra rate the game integrates then equals the model's K/s request. Stock
 * saved on first use, watched like the radiator coefficients, written back
 * once when the request ends / repair / stint / session / unload. The
 * address is DERIVED (docs/46: +0x1D64); its value is logged per session. */
#define OIL_SPEED_HEAT  (*(volatile double*)0x007C918Cu)   /* this+7524 */
static MfLever g_oil_heat;                 /* E9g-b: stock/last ownership */
static int    g_oil_heat_bad_logged = 0;
/* E9g-e item 7 (bug: 7708 "OIL HEAT: ... saved" lines in one session): two
 * pumps fight over the same lever - the oil-leak pump released it on every
 * tick while the leak was inactive, then the oil-pressure pump captured it
 * again, so "saved" logged every 50 ms and the stock value toggled. One owner
 * now: a release from any other pump is ignored, and a second pump can only
 * take over with a stronger request (max of the two wants wins). The capture
 * log then happens once per hold episode. */
#define OIL_HEAT_OWNER_LEAK   1
#define OIL_HEAT_OWNER_PRESS  2
#define OIL_HEAT_OWNER_WATER  3   /* R3g: engine heat to the oil once the coolant is gone */
static int    g_oil_heat_owner = 0;        /* 0 = lever free */
static double g_oil_heat_last_ks = 0.0;    /* want of the current owner, K/s */
/* R1a (ADDENDUM 9): the latched want - the last STEPPED model output. A
 * non-step tick never changes it and never releases the lever; it is cleared
 * only when the episode ends (leak over, repair, stint, session, unload). */
static double g_oil_heat_want_ks = 0.0;

static void oil_heat_hold(double ks, int owner)
{
    double stock, rpm_abs, denom, scale, want;

    if (g_oil_heat_owner != 0 && g_oil_heat_owner != owner
        && !(ks > g_oil_heat_last_ks))
        return;                            /* other pump holds a stronger want */
    g_oil_heat_owner = owner;
    g_oil_heat_last_ks = ks;

    if (!g_oil_heat.held) {
        stock = OIL_SPEED_HEAT;
        if (!(stock > 0.0) || !isfinite(stock)) {
            if (!g_oil_heat_bad_logged) {
                g_oil_heat_bad_logged = 1;
                logf_("OIL HEAT: EngineSpeedHeat stock invalid (%.6g) - "
                      "no mod heat", stock);
            }
            return;
        }
        logf_("OIL HEAT: stock EngineSpeedHeat %.6g saved", stock);
        /* R1a (ADDENDUM 9): capture the stock HERE. lever_write also captures
         * it, but the read below happens first - without this the first hold
         * of every episode read saved = 0, returned and logged "saved" every
         * tick (live engine_failure_log.txt, 4943 lines one session) while
         * never engaging the lever. Gen stays 0 until lever_write so the
         * H1c record is created by lever_write as usual. */
        g_oil_heat.saved = stock;
    } else if (OIL_SPEED_HEAT != g_oil_heat.last) {
        /* held watch (like coeff_held): the game rewrote it - adopt it */
        logf_("OIL HEAT: EngineSpeedHeat rewritten by the game (%.6g)",
              OIL_SPEED_HEAT);
        g_oil_heat.saved = OIL_SPEED_HEAT;
    }
    stock = g_oil_heat.saved;
    if (!(stock > 0.0) || !isfinite(stock)) return;

    rpm_abs = fabs((double)ENG_RPM_LIVE);
    denom = stock * rpm_abs;
    if (denom < 1e-6) denom = 1e-6;
    scale = 1.0 + ks / denom;
    if (scale < 1.0) scale = 1.0;
    if (scale > 4.0) scale = 4.0;
    want = stock * scale;
    lever_write(&g_oil_heat, &OIL_SPEED_HEAT, want);
}

/* owner 0 = release regardless of the current owner (repair/stint/session/
 * unload). Any other value releases only when that pump owns the lever. */
static void oil_heat_release(int owner)
{
    if (owner != 0 && g_oil_heat_owner != owner) return;
    g_oil_heat_owner = 0;
    g_oil_heat_last_ks = 0.0;
    g_oil_heat_want_ks = 0.0;   /* R1a: episode over - clear the latch */
    if (lever_restore(&g_oil_heat, &OIL_SPEED_HEAT, "OIL_SPEED_HEAT"))
        logf_("OIL HEAT: stock EngineSpeedHeat restored (%.6g)",
              OIL_SPEED_HEAT);
}

/* ---- E9e: the remaining engine.ini levers (MfLever ownership) -----------
 * CombustionHeat (lean running), OilMinimumCooling (oil leak), IdleThrottle
 * (rough idle) and the head-gasket coupling on OilWaterHeatTransfer. Every
 * write goes through lever_write; each hold releases by ownership when its
 * condition ends, and model_levers_release() covers repair, stint end,
 * session change and unload. */
static MfLever g_owt;            /* OilWaterHeatTransfer (head gasket) */
static MfLever g_comb;           /* CombustionHeat (lean running) */
static MfLever g_oilmin;         /* OilMinimumCooling (oil leak) */
static MfLever g_idle;           /* IdleThrottle (rough idle) */
static MfLever g_ebrake;         /* E9d: ENG_BRAKE_MAP (worn-bearing friction) */
static int g_owt_bad = 0, g_comb_bad = 0, g_oilmin_bad = 0, g_idle_bad = 0;
static int g_ebrake_bad = 0;

/* Capture a stock value once, with a range check. Returns 0 (and writes
 * nothing) while the stock is invalid or non-finite. */
static int lever_capture(MfLever *l, volatile double *addr, double lo, double hi)
{
    double stock;
    if (l->held) return 1;
    stock = *addr;
    if (!isfinite(stock) || stock < lo || stock > hi) return 0;
    lever_write(l, addr, stock);
    return 1;
}

/* E9e item 5: while the head gasket is damaged the game couples more oil heat
 * into the water: OWT_K0 x (1 + 3 x (1 - h_g)). Restored by ownership once
 * the gasket is healthy again. */
static void owt_coupling_pump(void)
{
    if (g_model_eng.h_g < 1.0) {
        if (!lever_capture(&g_owt, &OWT_K0, 1e-5, 1.0)) {
            if (!g_owt_bad) {
                g_owt_bad = 1;
                logf_("GASKET: OWT_K0 stock invalid (%.6g) - no coupling", OWT_K0);
            }
            return;
        }
        lever_write(&g_owt, &OWT_K0,
                    g_owt.saved * (1.0 + 3.0 * (1.0 - g_model_eng.h_g)));
    } else if (lever_restore(&g_owt, &OWT_K0, "OWT_K0")) {
        logf_("GASKET: OilWaterHeatTransfer restored");
    }
}

/* E9e item 8: lean running (fuel-pressure shortfall) raises CombustionHeat:
 * x (1 + 0.5 x shortfall). Restored by ownership when the mixture is clean. */
static void lean_heat_pump(void)
{
    /* 2026-10-06: lean_shortfall flickers 0 <-> >0 tick to tick (handoff
     * 7.21: 37 restore lines in 4 min). Hold the lever until the mixture has
     * been clean for 2 s; while held, sf 0 writes the stock value. */
    static DWORD lean_seen_ms = 0;
    DWORD now = game_ms();
    double sf = g_model_live ? g_model_eng.lean_shortfall : 0.0;
    if (sf > 0.0) lean_seen_ms = now;
    if (sf > 0.0 || (g_comb.held && lean_seen_ms
                     && (long)(now - lean_seen_ms) < 2000)) {
        if (sf < 0.0) sf = 0.0;
        if (!lever_capture(&g_comb, &COMB_HEAT, 0.01, 1000.0)) {
            if (!g_comb_bad) {
                g_comb_bad = 1;
                logf_("LEAN: CombustionHeat stock invalid (%.6g) - no heat raise", COMB_HEAT);
            }
            return;
        }
        lever_write(&g_comb, &COMB_HEAT, g_comb.saved * (1.0 + 0.5 * sf));
    } else if (lever_restore(&g_comb, &COMB_HEAT, "COMB_HEAT")) {
        logf_("LEAN: CombustionHeat restored");
    }
}

/* E9e item 9: during an oil leak less oil rejects less heat: OilMinimumCooling
 * x (0.5 + 0.5 x oil_left_fraction). Restored by ownership when the leak is
 * over. */
static void oil_min_cool_pump(void)
{
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_OIL_LEAK);
    if (active) {
        if (!lever_capture(&g_oilmin, &OIL_MIN_COOL, 1e-6, 1.0)) {
            if (!g_oilmin_bad) {
                g_oilmin_bad = 1;
                logf_("OIL COOLING: OilMinimumCooling stock invalid (%.6g) - no change", OIL_MIN_COOL);
            }
            return;
        }
        lever_write(&g_oilmin, &OIL_MIN_COOL,
                    g_oilmin.saved * (0.5 + 0.5 * (g_model_eng.v_oil_l / MF_OIL_START_L)));
    } else if (lever_restore(&g_oilmin, &OIL_MIN_COOL, "OIL_MIN_COOL")) {
        logf_("OIL COOLING: OilMinimumCooling restored");
    }
}

/* E9e item 10: rough idle - with a worn valvetrain or bearings and the revs
 * just above idle, modulate IdleThrottle with the game's own rhythmic noise
 * pattern (the E9d misfire formula) at depth 0.3. No stall logic. */
static double mf_noise_at(double x)
{
    const float *t = MF_NOISE_TABLE;
    double a = fabs(x);
    unsigned i0 = (unsigned)(int)a & 0xFFu;
    double f = a - (double)(int)a;
    double n = (double)t[i0]
             + ((double)t[(i0 + 1u) & 0xFFu] - (double)t[i0]) * f;
    return n;
}

static void rough_idle_pump(void)
{
    double rpm = (double)ENG_RPM_LIVE;
    int want = g_model_live && ENG_RUNNING
               && (g_model_eng.h_vt < 0.6 || g_model_eng.h_b < 0.5);
    if (want && rpm > 0.0 && rpm < g_model_car.idle_hi_rads * 1.5) {
        if (!lever_capture(&g_idle, &IDLE_THROTTLE, 0.0, 2.0)) {
            if (!g_idle_bad) {
                g_idle_bad = 1;
                logf_("ROUGH IDLE: IdleThrottle stock invalid (%.6g) - no modulation",
                      IDLE_THROTTLE);
            }
            return;
        }
        {
            double x = 11.5 / (g_model_car.rev_limit_rads > 100.0
                               ? g_model_car.rev_limit_rads : 1.0)
                       * SESSION_TIME * rpm;
            double n = mf_noise_at(x);
            lever_write(&g_idle, &IDLE_THROTTLE,
                        g_idle.saved * (1.0 - n * n * 0.3));
        }
    } else if (lever_restore(&g_idle, &IDLE_THROTTLE, "IDLE_THROTTLE")) {
        logf_("ROUGH IDLE: IdleThrottle restored");
    }
}

/* E9e: release every model lever with its ownership check (repair, stint end,
 * session change, unload). */
static void model_levers_release(void)
{
    oil_heat_release(0);   /* E9g-e item 7: force-release any owner */
    if (lever_restore(&g_owt, &OWT_K0, "OWT_K0"))
        logf_("GASKET: OilWaterHeatTransfer restored");
    g_owt_bad = 0;
    if (lever_restore(&g_comb, &COMB_HEAT, "COMB_HEAT"))
        logf_("LEAN: CombustionHeat restored");
    g_comb_bad = 0;
    if (lever_restore(&g_oilmin, &OIL_MIN_COOL, "OIL_MIN_COOL"))
        logf_("OIL COOLING: OilMinimumCooling restored");
    g_oilmin_bad = 0;
    if (lever_restore(&g_idle, &IDLE_THROTTLE, "IDLE_THROTTLE"))
        logf_("ROUGH IDLE: IdleThrottle restored");
    g_idle_bad = 0;
    if (lever_restore(&g_ebrake, &ENG_BRAKE_MAP, "ENG_BRAKE_MAP"))
        logf_("FRICTION: engine braking map restored");
    g_ebrake_bad = 0;
}

/* ---- N3 oil leak (E4b: model-driven) -------------------------------------
 * The sump loss, leak-area growth, extra oil heat (starved bearings) and the
 * smoke density live in the model (mf_engine_model.h 4.2). This pump maps
 * model state / MfOut onto the game levers: the measured oil level (gauge),
 * extra heat through the existing OIL_TEMP_K write, the load-scaled plume on
 * ENG_BANK1 and the HUD/radio lines. Bearing seizure is the model's BLOW UP
 * (handled once in model_terminal_pump). A manual trigger is a forced model
 * onset (medium, virtual budget, ignoring the game clock). Old percentage
 * maths: MF_LEGACY_OILP. */
#define OIL_TEMP_K       (*(volatile double*)0x007C9174u)   /* this+7500 */
#define OIL_LIMIT_K      (*(volatile double*)0x007C917Cu)   /* this+7508 */
#define ENG_REV_LIMIT    (*(volatile double*)0x007C9104u)   /* this+7388 */
#define EID_OVERHEAT_OIL 77
#define OIL_HEAT_MAX     3.0f     /* K/s at no oil and full revs; live 2026-09-28:
                                   * this car's limit is only ~9 K above idle oil */
/* OIL_WARN_SEVERITY moved to mf_notify.h (N2 2026-10-06) */

#if MF_LEGACY_OILP
static const float OIL_RATE[3] = { 0.25f, 0.5f, 0.85f };   /* % per second */
#endif

static int    g_oil_on = 0;
static float  g_oil_pressure = 100.0f;
static int    g_oil_hot_said = 0; /* engineer overheating line fired this leak? */
static double g_oil_prev_v = -1.0;/* last sump reading, for the rate */
static int    g_oil_warned = 0;   /* (legacy) marshal-warning line fired? */
#if MF_LEGACY_OILP
static float g_oil_rate = 0.5f;  /* % per second, rolled at arm */
#endif

static float jitterf(float span)
{
    return ((float)(rand() % 1000) / 1000.0f - 0.5f) * span;
}

/* Crash 2026-09-29 (owner: game crashed right after OIL LEAK armed; WER
 * access violation in the game's own matrix transform 0x55EF14): particles
 * were spawned from our worker thread while the game builds/renders them on
 * its own thread. Now the worker only QUEUES bursts; the game thread spawns
 * them inside the 0x447B10 detour (fuelcut_hook). No detour = no particles.
 * Owner 2026-09-29: oil smoke too thick - only every other burst is queued. */
#define PQ_MAX 8
typedef struct { int id, n, slot; float rate, spread, up; unsigned gen; } PBurst;
                                            /* id 2 = blue; gen = E9-H1c;
                                             * slot = AI car (B4, ADDENDUM 14;
                                             * 0 = the player) */
static PBurst g_pq[PQ_MAX];
static volatile LONG g_pq_n = 0;
static CRITICAL_SECTION g_pq_cs;
static volatile LONG g_pq_cs_ok = 0;
static int g_smoke_skip = 0;

static void pq_push(int id, int n, float rate, float spread, float up, int slot)
{
    if (!g_pq_cs_ok) {
        if (InterlockedExchange(&g_pq_cs_ok, 2) == 0) InitializeCriticalSection(&g_pq_cs);
        else while (g_pq_cs_ok != 2) Sleep(0);
    }
    EnterCriticalSection(&g_pq_cs);
    if (g_pq_n < PQ_MAX) {
        g_pq[g_pq_n].id = id; g_pq[g_pq_n].n = n; g_pq[g_pq_n].rate = rate;
        g_pq[g_pq_n].spread = spread; g_pq[g_pq_n].up = up;
        g_pq[g_pq_n].slot = slot;           /* B4: AI car position */
        g_pq[g_pq_n].gen = g_session_gen;   /* H1c: burst generation */
        g_pq_n++;
    }
    LeaveCriticalSection(&g_pq_cs);
    if (g_mfc_latched) mfc_on_burst(id, n, rate, spread, up, slot);   /* CO-OP mirror */
}

/* B4 (ADDENDUM 14, plan section 9): the same blue emitter for an AI car -
 * pos = veh+0x34E0/E4/E8 of the slot (VEH_BASE + slot x 0x48C0). Game thread
 * only (called from the fuelcut drain). */
static void emit_bluesmoke_now(int n, float rate, int slot)
{
    void *d = find_bluesmoke_descriptor();
    float pos[3], vel[3], simTime;
    float bx, by, bz;
    int i;
    if (!d) return;
    simTime = (float)SESSION_TIME;
    bx = *(volatile float*)(MF_AIV(slot) + AI_OFF_POS_X);
    by = *(volatile float*)(MF_AIV(slot) + AI_OFF_POS_Y);
    bz = *(volatile float*)(MF_AIV(slot) + AI_OFF_POS_Z);
    if (!(bx == bx) || !(by == by) || !(bz == bz)) return;   /* NaN guard */
    for (i = 0; i < n; i++) {
        pos[0] = bx + jitterf(0.6f);
        pos[1] = by + jitterf(0.15f);
        pos[2] = bz + jitterf(0.6f);
        vel[0] = jitterf(0.02f);
        vel[1] = 0.02f + jitterf(0.02f);
        vel[2] = jitterf(0.02f);
        ParticleTrailEmitter(d, NULL, simTime, pos, vel, rate, 0, 0, -1);
    }
}

static void emit_bluesmoke_burst(int n, float rate)
{
    g_smoke_skip = !g_smoke_skip;
    if (g_smoke_skip) return;                 /* half density */
    pq_push(2, n, rate, 0.0f, 0.0f, 0);
}

/* R3k (ADDENDUM 11, owner live 2026-10-01): blue-smoke drive shared by the
 * blow-by and oil-leak pumps (the two never run at once). Emission follows
 * load and rpm with a puff envelope on throttle lift/reapply (and a slosh
 * trigger for the oil leak); the maths is the pure mf_smoke_* block in
 * mf_engine_model.h. 0 while paused (callers return at g_pump_frozen). */
static double g_smoke_puff = 0.0;
static double g_smoke_prev_load = 0.0;
static double g_smoke_acc = 0.0;
static int    g_smoke_owner = -1;     /* MF_FAULT_* driving the envelope */
static DWORD  g_smoke_last_ms = 0;

static void smoke_reset(void)
{
    g_smoke_puff = 0.0;
    g_smoke_prev_load = g_model_in.load;
    g_smoke_acc = 0.0;
}

static double smoke_drive_step(int fault, double base, double dt, double slosh_g)
{
    DWORD now = game_ms();
    if (g_smoke_owner != fault || now - g_smoke_last_ms > 3000u) {
        g_smoke_owner = fault;            /* new episode (or stale) */
        smoke_reset();
    }
    g_smoke_last_ms = now;
    g_smoke_puff = mf_smoke_puff_step(g_smoke_puff, g_model_in.load, dt,
                                      slosh_g, &g_smoke_prev_load);
    return mf_smoke_drive(base, g_model_in.load, rev_frac(), g_smoke_puff);
}

static void smoke_emit_drive(double drive, double dt)
{
    int n;
    g_smoke_acc = mf_smoke_acc(g_smoke_acc, drive, dt);
    n = (int)g_smoke_acc;
    if (n <= 0) return;
    g_smoke_acc -= n;
    if (n > 8) n = 8;
    emit_bluesmoke_burst(n, (float)(0.25 + 0.5 * drive));
}

static DWORD g_oil_last_tick = 0;   /* 0 = not currently draining */

/* ---- oil on track, spike A (2026-09-28) ----------------------------------
 * Ground-mark reactions (terrain.ini Particle=Plane+Deformable+SingleSided:
 * 6 skid, 7 softskid, 8 wetskid, 9 groove, 10 wetgroove) emitted under the
 * car through the proven ParticleTrailEmitter path. All of them toggled
 * cleanly in the 2026-08-07 effect browser (only RAIN id 13 crashes - it is
 * refused here). UNKNOWN until seen live: whether they render as ground
 * marks at the given point, how long they last, orientation. Max is never
 * written. Visual only - no grip change. */
#define TRAIL_DROP_Y     0.30f      /* below the body reference point */
static int g_trail_test = 0;        /* panel test toggle */
static int g_trail_logged_id = -1;

static void trail_pump(void)
{
    int id = g_ctl ? g_ctl->trail_id : 0;
    /* DISABLED 2026-09-28: the first live test (SKID, id 6, zero velocity,
     * 40/s) FROZE the game. Kill switch until the cause is understood -
     * neither the oil leak nor the panel test lays a trail. */
    int want = 0;
    (void)g_trail_test;
    void *d;
    float pos[3], vel[3], bx, by, bz;
    int i;

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
    if (g_ctl) g_ctl->trail_on = 0;
    if (!want || id <= 0 || id == 13 || id > 12) return;
    d = find_sfx_descriptor(id);
    if (g_ctl) g_ctl->trail_max = d ? *(volatile int*)((DWORD)d + 88) : -1;
    if (id != g_trail_logged_id) {
        g_trail_logged_id = id;
        logf_("TRAIL: reaction id %d %s (Max=%d)", id, d ? "found" : "MISSING",
              d ? *(volatile int*)((DWORD)d + 88) : -1);
    }
    if (!d || RACE_PHASE <= 4) return;
    bx = *(volatile float*)(PLR_VEH + OFF_POS_X);
    by = *(volatile float*)(PLR_VEH + OFF_POS_Y);
    bz = *(volatile float*)(PLR_VEH + OFF_POS_Z);
    for (i = 0; i < 2; i++) {
        pos[0] = bx + jitterf(0.5f);
        pos[1] = by - TRAIL_DROP_Y;
        pos[2] = bz + jitterf(0.5f);
        vel[0] = vel[1] = vel[2] = 0.0f;
        ParticleTrailEmitter(d, NULL, (float)SESSION_TIME, pos, vel, 1.0f, 0, 0, -1);
    }
    if (g_ctl) g_ctl->trail_on = 1;
}

static void oil_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_OILP
    DWORD now = game_ms();
    float dt, loss, heat, rev, lim_c, temp_c;
    double limit;

    if (g_ctl) {
        g_ctl->oil_temp_c  = (float)(OIL_TEMP_K - 273.16);
        g_ctl->oil_limit_c = (float)(OIL_LIMIT_K - 273.16);
    }
    if (!g_oil_on) return;
    /* reset to 0 on every arm, so a stale timestamp never yields a huge dt */
    if (!g_oil_last_tick) { g_oil_last_tick = now; return; }
    dt = (float)(now - g_oil_last_tick) / 1000.0f;
    g_oil_last_tick = now;
    if (dt > 1.0f) dt = 1.0f;                     /* pause guard */

    g_oil_pressure -= dt * g_oil_rate * load_factor() * mfv_mult(&g_v_oil, now, dt);
    if (g_oil_pressure < 0.0f) g_oil_pressure = 0.0f;
    loss = (100.0f - g_oil_pressure) / 100.0f;   /* 0..1 */

    /* heat into the vanilla thermal model: starvation x revs */
    /* unit-agnostic: position between idle and the limiter (live: idle
     * already reads 0.72 of the limit in raw units) */
    rev = (ENG_REV_LIMIT > 100.0)
        ? ((float)(*(volatile float*)(PLR_VEH + 0x39A0u) / ENG_REV_LIMIT) - 0.20f) / 0.80f
        : 0.5f;
    if (rev < 0.0f) rev = 0.0f;
    if (rev > 1.1f) rev = 1.1f;
    heat = OIL_HEAT_MAX * powf(loss, 1.3f) * (0.25f + 0.75f * rev * rev);
    if (ENG_RUNNING) OIL_TEMP_K = OIL_TEMP_K + heat * dt;

    if (loss > 0.02f)
        emit_bluesmoke_burst(1 + (int)(loss * 2.0f), 0.3f + loss * 0.5f);
    /* marshals see the blue smoke and show the flag 5-20 s later */
    if (loss >= OIL_WARN_SEVERITY) g_oil_warned = 1;
    say_after(R_OILSMOKE, loss >= OIL_WARN_SEVERITY, 5000, 20000, EID_MECHWARN);

    limit = OIL_LIMIT_K;
    temp_c = (float)(OIL_TEMP_K - 273.16);
    lim_c = (float)(limit - 273.16);
    /* dash: oil temp near the limit, then pressure once the sump is low */
    warn_after(W_OILLOW, g_oil_pressure < 25.0f, 1000, 3000, "OIL PRESSURE");
    warn_after(W_OILT, g_oil_pressure >= 25.0f && limit > 300.0
               && OIL_TEMP_K > limit - 5.0, 1000, 3000, "OIL TEMP");
    instruct(I_OILT, limit > 300.0 && OIL_TEMP_K > limit - 5.0, 8000, 14000,
             "COOL THE OIL");
    /* engineer sees the oil past its limit on telemetry, 5-15 s later */
    if (limit > 300.0 && OIL_TEMP_K > limit && !g_oil_hot_said) {
        g_oil_hot_said = 1;
        logf_("OIL LEAK: oil %.1f C past limit %.1f C - vanilla life burn "
              "accelerating", temp_c, lim_c);
    }
    say_after(R_OIL77, limit > 300.0 && OIL_TEMP_K > limit, 5000, 15000,
              EID_OVERHEAT_OIL);

    if (g_oil_pressure <= 0.0f && ENG_RUNNING) {
        engine_seize("oil leak: bearings seized");   /* E9g item 3 */
        retire_pending_arm();
        g_oil_on = 0;
        g_oil_last_tick = 0;
        logf_("OIL LEAK: oil gone at %.1f C - bearings seized", temp_c);
        engine_dead_msg();
        hud_show("ENGINE FAILURE");
    }
#else
    /* E4b: model-driven. The physics runs in the model; here only the
     * levers, gauges and the HUD/radio lines. */
    DWORD now = game_ms();
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_OIL_LEAK);
    double v, loss;
    float dt, temp_c, lim_c;

    if (g_ctl) {
        g_ctl->oil_temp_c  = (float)(OIL_TEMP_K - 273.16);
        g_ctl->oil_limit_c = (float)(OIL_LIMIT_K - 273.16);
        if (g_model_live)
            g_ctl->oil_pressure =
                (float)(100.0 * g_model_eng.v_oil_l / MF_OIL_START_L);
    }
    g_oil_on = active;
    if (!active) {
        g_oil_last_tick = 0;
        g_oil_prev_v = -1.0;
        oil_heat_release(OIL_HEAT_OWNER_LEAK);  /* E8b: restore stock once */
        return;
    }
    if (!g_oil_last_tick) { g_oil_last_tick = now; return; }
    dt = (float)(now - g_oil_last_tick) / 1000.0f;
    g_oil_last_tick = now;
    if (dt > 1.0f) dt = 1.0f;                     /* pause guard */

    v = g_model_eng.v_oil_l;
    loss = 1.0 - v / MF_OIL_START_L;

    /* starvation heat through the game's own EngineSpeedHeat input (E8b).
     * R1a (ADDENDUM 9): the want is LATCHED from stepped model outputs only
     * (mf_oil_heat_latch). A non-step tick (pause, stalled pump, car-valid
     * gate) neither re-saves nor releases the lever; the single release is
     * the !active branch above (leak ended) and repair/stint/session/unload
     * (model_levers_release). */
    g_oil_heat_want_ks = mf_oil_heat_latch(g_oil_heat_want_ks,
                                           g_model_out.oil_heat_ks,
                                           g_model_stepped);
    if (ENG_RUNNING && g_oil_heat_want_ks > 0.0)
        oil_heat_hold(g_oil_heat_want_ks, OIL_HEAT_OWNER_LEAK);

    /* R2a (ADDENDUM 10) + R3k (ADDENDUM 11): oil on the exhaust burns BLUE,
     * none below 2 % loss, engine running only. R3k: density follows load and
     * rpm (hotter exhaust at revs) with a puff envelope on throttle
     * lift/reapply and a braking/cornering slosh trigger - the maths is
     * smoke_drive_step/smoke_emit_drive. Paused -> the pump returns above. */
    if (loss > 0.02 && ENG_RUNNING) {
        double slosh = g_model_in.g_long;
        double alat = fabs(g_model_in.g_lat);
        if (alat > slosh) slosh = alat;
        smoke_emit_drive(smoke_drive_step(MF_FAULT_OIL_LEAK, loss, dt, slosh), dt);
    }

    /* measured leak rate, %/s x100, in the legacy panel field */
    if (g_ctl && g_oil_prev_v >= 0.0 && v <= g_oil_prev_v) {
        float rate = (float)((g_oil_prev_v - v) / dt * 100.0 / MF_OIL_START_L);
        g_ctl->oil_rate_x100 = (int)(rate * 100.0f);
    }
    g_oil_prev_v = v;

    /* marshals see the (blue) smoke and show the flag 5-20 s later - the
     * fault's one pre-critical radio (2026-10-01 audit). V3: MECHWARN0 stays
     * the marshals' line; OILLEAK01 follows it 15-20 s later as the row's
     * instruction (then OILLEAK02), same pre-critical slot. */
    {
        int was = g_w_done[R_OILSMOKE];
        say_pre(R_OILSMOKE, mfn_oil_smoke(loss, ENG_RUNNING),
                5000, 20000, EID_MECHWARN);
        if (g_voice_on && !was && g_w_done[R_OILSMOKE])
            voice_arm_line(MFV_OILLEAK01);
    }

    temp_c = (float)(OIL_TEMP_K - 273.16);
    lim_c = (float)(OIL_LIMIT_K - 273.16);
    /* dash: the gauge (L2 2026-10-05 - oil_gauge_scale follows the cockpit
     * needle; was the bearing film flag), oil temp near the limit */
    /* live 2026-10-05: at thermostat oil the pump barely wears, the gauge ratio
     * stays ~1.0 and the bearings die from film breakdown - no warning, lamp or
     * voice (panel 13 %, h_b 1.00 -> 0.58 silent). Gauge OR active bearing wear. */
    /* 2026-10-06 owner live (OIL PRESSURE at 99 % oil): the dash shows the
     * SENSOR - gauge only. The bearing-film estimate no longer drives it;
     * bearing damage keeps its own critical line (MF_BEAR01). */
    warn_after(W_OILLOW, mfn_oil_gauge_low(&g_model_out), 1000, 3000,
               "OIL PRESSURE");
    warn_after(W_OILT, mfn_oil_temp_warn(&g_model_out), 1000, 3000, "OIL TEMP");
    /* instruction only after its diagnosis is on the dash */
    instruct(I_OILT, mfn_oil_cool_after_diag(&g_model_out, g_warn_blink[W_OILT].on),
             8000, 14000, "COOL THE OIL");
    /* V3: OIL TEMP dash on -> the engineer's oil-temperature line */
    voice_instr(I_OILT1, mfn_oil_cool_after_diag(&g_model_out, g_warn_blink[W_OILT].on),
                8000, 14000, MFV_OILT01);
    /* critical (2026-10-01 audit): the sump nearly dry or the bearings
     * nearly gone - the engine dies soon unless the car comes in */
    {
        int crit = crit_gate(C_OILLEAK, mfn_oil_crit(&g_model_eng),
                             g_warn_blink[W_OILLOW].on
                             || g_warn_blink[W_OILT].on,
                             "OIL PRESSURE", (DWORD)game_ms());
        critical(C_OILLEAK, crit, 2000, 5000, "ENGINE CRITICAL");
        voice_crit(RC_ENGINE, crit, 4000, 8000, MFV_CRIT01, EID_PITCALL);
    }
    /* engineer sees the oil past its limit on telemetry, 5-15 s later */
    if (OIL_LIMIT_K > 300.0 && OIL_TEMP_K > OIL_LIMIT_K && !g_oil_hot_said) {
        g_oil_hot_said = 1;
        logf_("OIL LEAK: oil %.1f C past limit %.1f C - vanilla life burn "
              "accelerating", temp_c, lim_c);
    }
    /* V3: eid 77 stays only for the GAME's own overheat - with the new lines
     * ON the mod's oil-temperature line is OILT01 (above) */
    if (!g_voice_on)
        say_pre(R_OIL77, mfn_oil77(OIL_TEMP_K, OIL_LIMIT_K),
                5000, 15000, EID_OVERHEAT_OIL);
#endif
}

/* Deterministic arm - shared by the GUI toggle and the automatic mix. */
static void arm_oil_leak(void)
{
#if MF_LEGACY_OILP
    int sev = rand() % 3;
    g_oil_on = 1;
    g_oil_pressure = 100.0f;
    g_oil_warned = 0;
    g_oil_hot_said = 0;
    g_oil_rate = OIL_RATE[sev];
    g_oil_last_tick = 0;
    if (g_ctl) g_ctl->oil_rate_x100 = (int)(g_oil_rate * 100.0f);
    mfv_roll(&g_v_oil, MFV_ALL);
    ev_pub(sev == 0 ? "OIL LEAK slow" : sev == 1 ? "OIL LEAK medium" : "OIL LEAK fast", &g_v_oil);
    logf_("OIL LEAK armed - %s leak, %.2f %%/s (~%.0f s to empty); oil %.1f C, "
          "limit %.1f C", sev == 0 ? "slow" : sev == 1 ? "medium" : "fast",
          g_oil_rate, 100.0f / g_oil_rate, OIL_TEMP_K - 273.16,
          OIL_LIMIT_K - 273.16);
#else
    g_oil_hot_said = 0;
    g_oil_last_tick = 0;
    g_oil_prev_v = -1.0;
    mf_fault_arm(&g_model_eng, &g_model_in, MF_FAULT_OIL_LEAK,
                 MF_SEV_MEDIUM, MF_MANUAL_BUDGET_S);
    logf_("OIL LEAK: model fault armed (manual, medium, budget %.0f s)",
          MF_MANUAL_BUDGET_S);
#endif
}

static void toggle_oil_leak(void)
{
#if MF_LEGACY_OILP
    if (!g_oil_on) {
        arm_oil_leak();
    } else {
        g_oil_on = 0;
        logf_("OIL LEAK stopped at pressure=%.1f", g_oil_pressure);
    }
#else
    if (!g_oil_on) {
        arm_oil_leak();
    } else {
        /* test toggle off: stop the leak; levels and wear stay as they are */
        g_model_eng.onset = 0;
        g_model_eng.fault = MF_FAULT_NONE;
        g_model_eng.stabilised = 0;
        g_model_eng.stop_delay_s = -1.0;
        g_oil_on = 0;
        g_oil_last_tick = 0;
        g_oil_prev_v = -1.0;
        logf_("OIL LEAK: model fault cleared (test toggle off)");
    }
#endif
}

/* ---- SUSPENSION: RESOLVED 2026-08-08 - DO NOT re-add here, use the
 * separate suspension_failure.dll (sibling in this folder). The proper hunt
 * concluded: the control input is NOT a field write - it is the function
 * call SteeringGeometryUpdate_00442060(0x7C7428, wheel, severity), the same
 * call the game makes at 0x4EC1A0:515, driven from a per-frame game-thread
 * detour (VehicleMainPhysicsUpdate_004441f0). Live-proven: bends, flags,
 * ReasonCode=4, radio eid 75. History of the failed approach below.
 * This lever wrote the per-corner accumulator at wheel_base + 1124*i + 1132 * to 0.020, past the 0.015 trip in SteeringGeometryUpdate_00442060. It did
 * NOTHING (driver: "doesn't really seem to affect anything"), and a follow-up
 * that additionally wrote the bend angles and the bent flag CRASHED THE GAME
 * (2026-07-30).
 *
 * Two reasons it was wrong, both visible in the source before I wrote it:
 *
 *  1. The trip only raises ReasonCode, it does not bend anything, AND it is
 *     guarded by `if (!ReasonCode)` - so it is silently dead whenever any
 *     other failure already set a code (ours was 1, Engine).
 *
 *  2. The function indexes as this[281*a2 + N] with N reaching 282. 281
 *     dwords is 1124 bytes = the assumed stride, so fields at +279..+282
 *     dwords run PAST the end of the record. The "stride 1124, accumulator
 *     at +1132" model from the earlier suspension session does not
 *     reconcile with this function's own arithmetic. Writing through that
 *     contradiction is what corrupted the object.
 *
 * The earlier session's offsets were validated by READING them during real
 * crash damage. That proves they are readable telemetry; it does not prove
 * they are writable inputs. The real accumulator the function tests is
 * *(double*)(v4 + 480) on a sub-object, not a float on the wheel record.
 *
 * To do this properly: find what v4 is in SteeringGeometryUpdate_00442060
 * (x32dbg bphws on the accumulator during a real bend - NOT a Frida hardware
 * watchpoint, which crashes this process), then arm the input the game
 * itself writes, the way the engine work armed life/stress.
 */

/* ---- overheating / water leak (R2, 2026-09-28) ---------------------------
 * Replaces the vanilla "stage one smoke" as the automatic type: a real
 * driver sees WATER TEMP on the dash and hears the engineer, and the car
 * slowly loses power; visible steam comes late and only under load. Mod-
 * authored model, same levers as the oil leak (ENG_STRESS floor, bank 1,
 * running byte). Timeline from arm:
 *   0 s                 HUD WATER TEMP + radio eid 77 PWS_MECHPROB15
 *                       "Engine overheating" (unblocked, cooldown 30)
 *   HEAT_POWER_S..end   stress floor ramps 0 -> ENG_STRESS_CAP (power loss)
 *   HEAT_STEAM_S..end   bank 1 NEGATIVE = white smoke only under load
 *   HEAT_TOTAL_S        engine fails: running 0, ReasonCode 1, retire line */
#define EID_OVERHEAT     77
#define HEAT_POWER_S     40.0f
#define HEAT_STEAM_S     90.0f
#define HEAT_TOTAL_S     150.0f
#define HEAT_STEAM_BANK  (-0.35)

static int   g_heat_on = 0;
static float g_heat_t = 0.0f;
static DWORD g_heat_last = 0;
static DWORD g_heat_plume_until = 0;

static void arm_overheat(void)
{
    g_heat_on = 1;
    g_heat_t = 0.0f;
    g_heat_last = 0;
    /* 2026-10-01: via the radio queue. V3: eid 77 stays only for the GAME's
     * own overheat - the panel overheat uses the water-temperature line. */
    if (g_voice_on) radio_req(mfv_eid(MFV_WATER01), MFR_P_INFO);
    else            radio_req(EID_OVERHEAT, MFR_P_INFO);
    hud_show("WATER TEMP");
    mfv_roll(&g_v_heat, MFV_BIT(MFV_STEADY) | MFV_BIT(MFV_WORSE) | MFV_BIT(MFV_ONOFF));
    ev_pub("OVERHEAT", &g_v_heat);
    logf_("OVERHEAT armed - power loss from %.0f s, steam from %.0f s, "
          "failure at %.0f s", HEAT_POWER_S, HEAT_STEAM_S, HEAT_TOTAL_S);
}

static void heat_pump(void)
{
    DWORD now = game_ms();
    float dt;

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
    if (g_heat_plume_until && (long)(now - g_heat_plume_until) >= 0) {
        g_heat_plume_until = 0;
        bank1_set(0.0, "overheat");
    }
    if (!g_heat_on) return;
    if (!g_heat_last) { g_heat_last = now; return; }
    dt = (float)(now - g_heat_last) / 1000.0f;
    g_heat_last = now;
    if (dt > 5.0f) dt = 5.0f;
    g_heat_t += dt * mfv_mult(&g_v_heat, now, dt);

    if (g_heat_t >= HEAT_POWER_S) {
        float want = (float)ENG_STRESS_CAP * (g_heat_t - HEAT_POWER_S)
                     / (HEAT_TOTAL_S - HEAT_POWER_S);
        if (want > (float)ENG_STRESS_CAP) want = (float)ENG_STRESS_CAP;
        if (ENG_STRESS < want) { ENG_STRESS = want; g_native_touched = 1; }
    }
    if (g_heat_t >= HEAT_STEAM_S && ENG_BANK1 == 0.0) {
        bank1_set(HEAT_STEAM_BANK, "overheat");
        g_native_touched = 1;
    }

    if (g_heat_t >= HEAT_TOTAL_S) {
        engine_seize("overheat");            /* E9g item 3: no ignition clear */
        retire_pending_arm();
        bank1_set(EMIT_ON, "overheat");      /* steam plume as it stops */
        g_native_touched = 1;
        g_heat_plume_until = now + 8000;
        g_heat_on = 0;
        logf_("OVERHEAT: engine failed after %.0f s", g_heat_t);
        hud_show("ENGINE FAILURE");
    }
}

/* ---- instant blow: FIREBALL (R3, upgraded 2026-09-28) --------------------
 * Owner: "a spectacular engine explosion into flames". Three layers:
 *  1. Sound: mf_engine_blow.wav (original synthesized audio, next to this
 *     DLL) through winmm PlaySound - independent of the game's audio engine,
 *     so no unproven call into Miles.
 *  2. Particles: bursts of the game's own FLAMES (id 4) and BLACKSMOKE (id 3)
 *     reaction effects straight from ParticleTrailEmitter (the proven oil-leak
 *     blue-smoke path), in a timeline: fireball flash -> big fire while the
 *     car coasts -> dying flames -> black smoke trail.
 *  3. Engine banks 2/3 kept on as the continuous base (proven levers).
 * If a descriptor is missing the banks alone still show fire/smoke. */
#define BLOW_FIRE_MS    8000
#define BLOW_SMOKE_MS   16000
#define BLOW_SOUND_FILE "mf_engine_blow.wav"
#define SFX_ID_BLACK    3
#define SFX_ID_FLAMES   4
static DWORD g_blow_start = 0;
static void fx_restore(void);
static DWORD g_blow_fire_until = 0;
static DWORD g_blow_smoke_until = 0;

/* 2026-10-06 release: the mod's own sounds. Lightloader installs them to
 * <game>\Audio\MechanicalFailures\ (package assets); the development tree
 * keeps them next to the DLL. Game folder first, DLL folder second.
 * 0 = no usable path. */
static int mf_sound_path(char *path, const char *file)
{
    char *slash;
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len && len < MAX_PATH && (slash = strrchr(path, '\\')) != NULL) {
        _snprintf(slash + 1, MAX_PATH - (size_t)(slash + 1 - path),
                  "Audio\\MechanicalFailures\\%s", file);
        path[MAX_PATH - 1] = '\0';
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return 1;
    }
    len = GetModuleFileNameA(g_hinst, path, MAX_PATH);
    if (!len || len >= MAX_PATH) return 0;
    slash = strrchr(path, '\\');
    if (!slash) return 0;
    lstrcpynA(slash + 1, file, (int)(MAX_PATH - (slash + 1 - path)));
    return 1;
}

static void blow_sound(void)
{
    char path[MAX_PATH];
    char *slash;
    DWORD len;
    if (!live_drive()) {          /* E9g-c: no blow sound outside a live drive */
        logf_("BLOW: sound skipped (not driving)");
        return;
    }
    (void)len; (void)slash;
    if (!mf_sound_path(path, BLOW_SOUND_FILE)) return;
    play_at_car(path, 2.0f);   /* E7: BLOW UP class, +6 dB, slider-following; R3j 3D */
    logf_("BLOW: sound queued %s", path);
}

static void emit_burst(int id, int n, float rate, float spread, float up)
{
    pq_push(id | 0x100, n, rate, spread, up, 0);   /* game thread spawns it */
}

static void emit_burst_now(int id, int n, float rate, float spread, float up)
{
    void *d = find_sfx_descriptor(id);
    float pos[3], vel[3], simTime;
    float bx, by, bz;
    int i;
    if (!d) return;
    simTime = (float)SESSION_TIME;
    bx = *(volatile float*)(PLR_VEH + OFF_POS_X);
    by = *(volatile float*)(PLR_VEH + OFF_POS_Y);
    bz = *(volatile float*)(PLR_VEH + OFF_POS_Z);
    for (i = 0; i < n; i++) {
        pos[0] = bx + jitterf(spread);
        pos[1] = by + 0.3f + jitterf(spread * 0.4f);
        pos[2] = bz + jitterf(spread);
        vel[0] = jitterf(up * 0.8f);
        vel[1] = up * (0.6f + (float)(rand() % 100) / 100.0f);
        vel[2] = jitterf(up * 0.8f);
        ParticleTrailEmitter(d, NULL, simTime, pos, vel, rate, 0, 0, -1);
    }
}

/* N3e: the same burst at an AI car (slot 0 = the player is never routed
 * here). Game thread only - called from the fuelcut drain like the player
 * one. */
static void emit_burst_now_at(int id, int n, float rate, float spread,
                              float up, int slot)
{
    void *d = find_sfx_descriptor(id);
    float pos[3], vel[3], simTime;
    float bx, by, bz;
    int i;
    if (!d || slot < 1 || slot >= MF_AI_MAX) return;
    simTime = (float)SESSION_TIME;
    bx = *(volatile float*)(MF_AIV(slot) + AI_OFF_POS_X);
    by = *(volatile float*)(MF_AIV(slot) + AI_OFF_POS_Y);
    bz = *(volatile float*)(MF_AIV(slot) + AI_OFF_POS_Z);
    if (!(bx == bx) || !(by == by) || !(bz == bz)) return;
    for (i = 0; i < n; i++) {
        pos[0] = bx + jitterf(spread);
        pos[1] = by + 0.3f + jitterf(spread * 0.4f);
        pos[2] = bz + jitterf(spread);
        vel[0] = jitterf(up * 0.8f);
        vel[1] = up * (0.6f + (float)(rand() % 100) / 100.0f);
        vel[2] = jitterf(up * 0.8f);
        ParticleTrailEmitter(d, NULL, simTime, pos, vel, rate, 0, 0, -1);
    }
}

/* N3e: queue one burst at an AI car (worker side). id | 0x200 = "spawn this
 * sfx descriptor at the queued slot" (0x100 = the player's). */
static void ai_burst_at(int slot, int id, int n, float rate, float spread,
                        float up)
{
    pq_push(id | 0x200, n, rate, spread, up, slot);
}

/* N3e emission numbers (owner D3): the AI flash mirrors the player's 40-
 * particle fireball, then a fire + black smoke trail while the burn runs
 * (the P5 bank fade owns the tail). All named here for later tuning. */
#define MF_AI_BLOW_FLASH_N      40
#define MF_AI_BLOW_FX_FIRE_N    3
#define MF_AI_BLOW_FX_SMOKE_N   2
#define MF_AI_BLOW_FX_PERIOD_S  0.25
#define MF_AI_BLOW_FX_BURN_S    12.0
#define MF_AI_BLOWUP_PUFF       0.15   /* grey bank puff during the warning */

/* FLAMES descriptor tuning during the fireball (offsets from
 * MaterialShaderReactionEffectParser_004ca480, as used live by
 * mods/sdk_sandbox/fuel_fire_lab). Vanilla flames are tiny and short
 * (terrain.ini Scale 0.40, ASD 0.07/0.17/0.4, faint) - that is why the first
 * version read as smoke. Only render fields are changed: Scale, Growth,
 * Power, RampSpeed and the renderer COLOR. The pool cap Max (+88) is NEVER
 * written - raising it mid-session froze the game (fuel_fire_lab 2026-08-08).
 * The originals are restored when the fire ends, on repair and on unload,
 * and only if the table was not rebuilt meanwhile (session reload). */
#define DESC_SCALE_X    12
#define DESC_GROWTH_X   24
#define DESC_POWER      68
#define DESC_RAMPSPEED  72
#define DESC_RENDERER   80
#define RENDER_COLOR    96
#define FIRE_COLOR      0xFFD24A1Au   /* fuel_fire_lab owner-tuned orange */

static int      g_fx_saved = 0;
static unsigned g_fx_gen = 0;              /* E9-H1c: capture generation */
static DWORD    g_fx_key = 0;
static void    *g_fx_desc = NULL;
static float    g_fx_scale[3], g_fx_growth[3], g_fx_power, g_fx_ramp;
static uint32_t g_fx_color;

static DWORD fx_table_key(void) { return SFX_ARRAY_BASE ^ (SFX_COUNT << 20); }

static void fx_save(void *d)
{
    void *r;
    int i;
    if (g_fx_saved || !d) return;
    for (i = 0; i < 3; i++) {
        g_fx_scale[i]  = *(volatile float*)((DWORD)d + DESC_SCALE_X + 4 * i);
        g_fx_growth[i] = *(volatile float*)((DWORD)d + DESC_GROWTH_X + 4 * i);
    }
    g_fx_power = *(volatile float*)((DWORD)d + DESC_POWER);
    g_fx_ramp  = *(volatile float*)((DWORD)d + DESC_RAMPSPEED);
    r = *(void**)((DWORD)d + DESC_RENDERER);
    g_fx_color = r ? *(volatile uint32_t*)((DWORD)r + RENDER_COLOR) : 0;
    g_fx_desc = d;
    g_fx_key = fx_table_key();
    g_fx_gen = g_session_gen;      /* H1c: the generation this save is from */
    g_fx_saved = 1;
    logf_("BLOW: saved vanilla flames scale %.2f growth %.2f color 0x%08X",
          g_fx_scale[0], g_fx_growth[0], (unsigned)g_fx_color);
}

static void fx_apply(float scale_mult, float growth_mult)
{
    void *d = g_fx_desc, *r;
    int i;
    if (!g_fx_saved || !d || fx_table_key() != g_fx_key) return;
    for (i = 0; i < 3; i++) {
        *(volatile float*)((DWORD)d + DESC_SCALE_X + 4 * i)  = g_fx_scale[i] * scale_mult;
        *(volatile float*)((DWORD)d + DESC_GROWTH_X + 4 * i) = g_fx_growth[i] * growth_mult;
    }
    *(volatile float*)((DWORD)d + DESC_POWER)     = g_fx_power * 2.0f;
    *(volatile float*)((DWORD)d + DESC_RAMPSPEED) = g_fx_ramp * 2.5f;
    r = *(void**)((DWORD)d + DESC_RENDERER);
    if (r) *(volatile uint32_t*)((DWORD)r + RENDER_COLOR) = FIRE_COLOR;
}

static void fx_restore(void)
{
    void *d = g_fx_desc, *r;
    int i;
    if (!g_fx_saved) return;
    g_fx_saved = 0;
    /* H1c: a record from another session (or a session that is over) is
     * dropped, never written into a fresh descriptor table. */
    if (g_fx_gen != g_session_gen || SESSION_OVER()) {
        logf_("BLOW: flames restore dropped (%s)",
              g_fx_gen != g_session_gen ? "stale session generation"
                                        : "session over");
        return;
    }
    if (!d || fx_table_key() != g_fx_key) {
        logf_("BLOW: flames table rebuilt - nothing to restore");
        return;
    }
    for (i = 0; i < 3; i++) {
        *(volatile float*)((DWORD)d + DESC_SCALE_X + 4 * i)  = g_fx_scale[i];
        *(volatile float*)((DWORD)d + DESC_GROWTH_X + 4 * i) = g_fx_growth[i];
    }
    *(volatile float*)((DWORD)d + DESC_POWER)     = g_fx_power;
    *(volatile float*)((DWORD)d + DESC_RAMPSPEED) = g_fx_ramp;
    r = *(void**)((DWORD)d + DESC_RENDERER);
    if (r) *(volatile uint32_t*)((DWORD)r + RENDER_COLOR) = g_fx_color;
    logf_("BLOW: flames restored to vanilla");
}

/* ---- N3e (owner D3, MF 1.0.3): shared FLAMES descriptor for every blow ----
 * Choice: option (i) of the Part N prompt - ONE owner list over the existing
 * single-owner fx_save/fx_restore. Why not a queue-only AI flash: the vanilla
 * flames are tiny (the first player fireball read as smoke until the x3.5
 * descriptor tuning); an AI fireball needs the same tuning. The descriptor is
 * a GLOBAL, so correctness is the ownership rule, kept pure and tested:
 *   - the snapshot is taken once (fx_save already skips when g_fx_saved);
 *   - the descriptor stays tuned while ANY owner (player or AI) is burning;
 *   - fx_restore runs only when the LAST owner has been released;
 *   - the player's own restore defers (never restores while an AI blow runs);
 *   - two AI blows + the player's in any order -> the same single snapshot
 *     is written back exactly (offline fake-descriptor test, ai_react_test).
 * g_blow_fx_start[] holds each owner's game-ms start (0 = inactive; index 0 =
 * the player, 1.. = the AI slot). */
#define MFC_REMOTE_FX 8   /* CO-OP: fire owners on other PCs' cars (mf_coop_engine.h) */
static double g_blow_fx_start[MF_AI_MAX + MFC_REMOTE_FX];

static void blow_fx_update(void)
{
    double now = (double)game_ms();
    int ph;
    if (SESSION_OVER()) {                 /* session end: drop every owner */
        memset(g_blow_fx_start, 0, sizeof g_blow_fx_start);
        fx_restore();
        return;
    }
    ph = mf_blow_fx_phase_ms(g_blow_fx_start, MF_AI_MAX + MFC_REMOTE_FX, now,
                             MF_BLOW_FX_FLASH_MS);
    if (ph == MF_BLOW_FX_NONE) {
        fx_restore();                     /* no-op unless a save is held */
        return;
    }
    if (!g_fx_saved) {
        void *fl = find_sfx_descriptor(SFX_ID_FLAMES);
        if (!fl) return;
        fx_save(fl);
    }
    if (ph == MF_BLOW_FX_FLASH) fx_apply(3.5f, 2.5f);
    else                        fx_apply(2.0f, 2.2f);
}

/* owner: 0 = the player, 1..MF_AI_MAX-1 = AI slots */
static void blow_fx_claim(int owner)
{
    if (owner < 0 || owner >= MF_AI_MAX) return;
    g_blow_fx_start[owner] = (double)game_ms();
    blow_fx_update();
    if (g_mfc_latched) mfc_on_blow(owner, 1);   /* CO-OP mirror */
}

static void blow_fx_release(int owner)
{
    if (owner < 0 || owner >= MF_AI_MAX) return;
    g_blow_fx_start[owner] = 0.0;
    blow_fx_update();
    if (g_mfc_latched) mfc_on_blow(owner, 0);   /* CO-OP mirror */
}

/* Timeline from the bang:
 *   0 ms        engine OFF (running 0, ReasonCode 1, retire line), sound,
 *               fireball: flames x3.5 size, 40-particle flash, NO smoke
 *   0-1500      fireball keeps pumping (14/tick)
 *   1500-8000   fire x2.0 size, dying 10 -> 2 per tick, bank 3 on
 *   3000-16000  black smoke starts light and trails off
 *   8000        flames back to vanilla size/colour, bank 3 off           */
static void blow_burst_start(void)
{
    DWORD now = game_ms();
    void *fl = find_sfx_descriptor(SFX_ID_FLAMES);

    engine_dead_latch("blow-up");   /* E9g-e item 1: terminal - torque 0 */
    /* E8h: no ignition clear - a mechanical blow-up leaves the ignition on;
     * the game stops the engine on life 0 (VehicleActive) and the dash lights
     * stay lit, exactly as the game's own crash seizure does */
    engine_dead_msg();
    REASON_CODE = 1;
    retire_pending_arm();
    ENG_BANK2 = 0.0;                      /* no smoke at the bang */
    ENG_BANK3 = EMIT_ON;
    g_native_touched = 1;                 /* E8b: mod-written banks */
    g_blow_start = now;
    g_blow_fire_until = now + BLOW_FIRE_MS;
    g_blow_smoke_until = now + BLOW_SMOKE_MS;
    burn_arm(1);                  /* R3q: long post-death burn timeline */
    blow_sound();
    /* N3e: claim the shared FLAMES descriptor (single snapshot, restore only
     * after the last owner - player or AI - ends; see blow_fx_update) */
    blow_fx_claim(0);
    emit_burst(SFX_ID_FLAMES, 40, 1.0f, 1.8f, 0.30f);
    logf_("BLOW: engine OFF, fireball (flames %s) - fire %d ms, smoke %d ms",
          fl ? "found" : "MISSING", BLOW_FIRE_MS, BLOW_SMOKE_MS);
}

static void blow_pump(void)
{
    DWORD now = game_ms();
    DWORD t;

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
    if (!g_blow_start) return;
    t = now - g_blow_start;
    /* E8h: the engine is dead through life = 0 (engine_seize); the ignition
     * byte is left as the game has it while the fire burns */
    if (t < 1500) {
        fx_apply(3.5f, 2.5f);
        emit_burst(SFX_ID_FLAMES, 14, 1.0f, 1.5f, 0.25f);
    } else if (t < BLOW_FIRE_MS) {
        int n = 2 + (int)(8 * (BLOW_FIRE_MS - t) / (BLOW_FIRE_MS - 1500));
        fx_apply(2.0f, 2.2f);
        emit_burst(SFX_ID_FLAMES, n, 0.9f, 1.1f, 0.15f);
    }
    if (t >= 3000 && t < BLOW_SMOKE_MS) {
        int m = t < BLOW_FIRE_MS ? 2 : 1;
        emit_burst(SFX_ID_BLACK, m, 0.7f, 0.8f, 0.08f);
    }
    if (g_blow_fire_until && (long)(now - g_blow_fire_until) >= 0) {
        g_blow_fire_until = 0;
        ENG_BANK3 = 0.0;
        /* N3e: the player releases the descriptor; fx_restore happens only
         * when no AI blow is still burning (blow_fx_update) */
        blow_fx_release(0);
    }
    if (g_blow_smoke_until && (long)(now - g_blow_smoke_until) >= 0) {
        g_blow_smoke_until = 0;
        g_blow_start = 0;
    }
}

/* ---- R3q (ADDENDUM 11, owner live, manual BLOW UP) -----------------------
 * The game's sub_4F1FE0 zeroes the three smoke banks of a car that has been
 * stationary past its dwell (finding stationary-vehicle-removal-retire-
 * 2026-10-01), and the mod's own BLOW timers end at 8/16 s - the owner saw
 * fire and smoke vanish the moment the car stopped. This pump re-asserts the
 * banks every pass while the dead latch is set, with POSITIVE bank values
 * (bank <= 0 is load-scaled and dies with the engine: rpm/torque are 0):
 *   blow-up:    flames 0-15 s, heavy black smoke 3-75 s decaying, light
 *               smoke until repair/session/garage;
 *   seize/stop: a smaller smoke tail (0-25 s heavier, then light).
 * Paused -> no writes (R1b). Repair/session/unload clears the banks once. */
#define BURN_FIRE_MS       15000u
#define BURN_HEAVY_MS      75000u
#define BURN_STOP_HEAVY_MS 25000u
#define BURN_BANK_FIRE     0.85   /* EMIT_ON */
#define BURN_BANK_HEAVY    0.65
#define BURN_BANK_LIGHT    0.20
#define BURN_BANK_STOP     0.40
static DWORD g_burn_start = 0;
static int   g_burn_kind = 0;      /* 0 none, 1 blow-up, 2 seize/stop */
static DWORD g_burn_emit_next = 0;

static void burn_arm(int kind)
{
    if (kind == 2 && g_burn_kind == 1) return;   /* a blow is already burning */
    g_burn_kind = kind;
    g_burn_start = game_ms();
    g_burn_emit_next = 0;
}

static void burn_clear(void)
{
    if (!g_burn_kind) return;
    g_burn_kind = 0;
    g_burn_start = 0;
    ENG_BANK2 = 0.0;
    ENG_BANK3 = 0.0;
    g_native_touched = 1;
}

static void burn_pump(void)
{
    DWORD now, t;

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
    if (!g_engine_dead || !g_burn_kind) return;
    now = game_ms();
    t = now - g_burn_start;

    if (g_burn_kind == 1) {
        if (t < BURN_FIRE_MS) {
            ENG_BANK3 = BURN_BANK_FIRE;
            if ((long)(now - g_burn_emit_next) >= 0) {
                int n = t < BLOW_FIRE_MS ? 0
                      : (int)(4.0 - 3.0 * (double)t / BURN_FIRE_MS);
                if (n > 0) emit_burst(SFX_ID_FLAMES, n, 0.9f, 1.0f, 0.15f);
                g_burn_emit_next = now + 250;
            }
        } else {
            ENG_BANK3 = 0.0;
        }
        if (t >= 3000) {
            double heavy = t < BURN_HEAVY_MS
                         ? 1.0 - 0.7 * (double)(t - 3000)
                                 / (double)(BURN_HEAVY_MS - 3000)
                         : 0.0;
            ENG_BANK2 = heavy > 0.0 ? (float)(BURN_BANK_HEAVY * heavy)
                                    : BURN_BANK_LIGHT;
        }
    } else {
        ENG_BANK3 = 0.0;
        ENG_BANK2 = t < BURN_STOP_HEAVY_MS ? BURN_BANK_STOP : BURN_BANK_LIGHT;
    }
}

/* Wear ramp death announcement (R1): the game clears the running byte
 * itself when the ramp reaches its cap. */
/* Live 2026-09-28: the ramp can sit at the 0.6 cap with the engine still
 * turning (car crawls on for ever). If stress holds at the cap for
 * WEAR_CAP_KILL_MS and the engine still runs, finish it ourselves. */
#define WEAR_CAP_KILL_MS 5000
static DWORD g_wear_cap_since = 0;

static void wear_watch_pump(void)
{
    DWORD now;                    /* E9o: failure timing follows the game clock */
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    now = game_ms();
    if (!g_wear_watch) { g_wear_cap_since = 0; return; }
    /* owner 2026-09-29: the game also switches the engine off in the pits -
     * that is not a death. Wait until the car is out of the pit lane. */
    if (*(volatile int*)(PLR_VEH + 0x3450u) >= 2)   /* PIT_STATUS */ { g_wear_cap_since = 0; return; }
    if (ENG_RUNNING == 0) {
        g_wear_watch = 0;
        g_wear_cap_since = 0;
        logf_("WEAR: engine died - HUD ENGINE FAILURE");
        hud_show("ENGINE FAILURE");
        return;
    }
    if (ENG_STRESS < ENG_STRESS_CAP - 0.001) { g_wear_cap_since = 0; return; }
    if (!g_wear_cap_since) { g_wear_cap_since = now; return; }
    if ((long)(now - g_wear_cap_since) < WEAR_CAP_KILL_MS) return;
    engine_seize("staged wear terminal");   /* E9g item 3 */
    retire_pending_arm();
    g_wear_watch = 0;
    g_wear_cap_since = 0;
    logf_("WEAR: stress at cap %d ms and still running - engine killed", WEAR_CAP_KILL_MS);
    engine_dead_msg();
    hud_show("ENGINE FAILURE");
}

/* ==== NEW ENGINE FAILURES ON TOP OF VANILLA (2026-09-28) =================
 * Cause is ours; consequences run through vanilla levers (life drain, the
 * ignition byte, stress only as a small floor) and each has driver agency.
 * Real basis + design: docs/ENGINE_FAILURES_RESEARCH_2026-09-28.md. */
#define PIT_STATUS      (*(volatile int*)(PLR_VEH + 0x3450u))  /* 3 = in box */
/* Pit stop end time (veh+0x3470, float session seconds, LATCHED per
 * pitstop_modding DOCS.md:59; written by PitStopReadyCheck once the car has
 * settled - an earlier write is overwritten). Worker result #1 Q10. */
#define PIT_STOP_END    (*(volatile float*)(PLR_VEH + 0x3470u))

/* Add repair time to the running stop, once, as an ABSOLUTE deadline
 * (never +=, per the pit docs). Returns 1 when the repair is done (time
 * added, or the car already left the box), 0 while still waiting. */
static int pit_repair(float extra_s, int *done_flag)
{
    float now = (float)SESSION_TIME, end;
    if (*done_flag) return 1;
    if (PIT_STATUS >= 4) { *done_flag = 1; return 1; }   /* left the box */
    if (PIT_STATUS != 3) return 0;
    end = PIT_STOP_END;
    if (!(end > now + 0.2f) || end > now + 600.0f) return 0;   /* not set yet */
    PIT_STOP_END = end + extra_s;
    *done_flag = 1;
    logf_("PIT: +%.1f s repair time (stop end %.1f -> %.1f)", extra_s, end, end + extra_s);
    return 1;
}

/* revs as a fraction idle -> limiter (unit-agnostic; live 2026-09-28) */
/* ENG_RPM_LIVE 0x39A0 is defined at the top macro block (E8b). */
#define REV_IDLE_FRAC  0.20f      /* idle ~372/1895 */

/* revs as a fraction idle -> limiter */
static float rev_frac(void)
{
    float f;
    if (ENG_REV_LIMIT < 100.0) return 0.5f;
    f = ((float)(ENG_RPM_LIVE / ENG_REV_LIMIT) - REV_IDLE_FRAC)
        / (1.0f - REV_IDLE_FRAC);
    if (f < 0.0f) f = 0.0f;
    return f;
}

static float frand01(void) { return (float)(rand() % 1000) / 1000.0f; }
static void exhaust_pop(void);
/* play one of our sound files that sit next to this DLL (E7 level chain) */
static void play_fx(const char *file)
{
    char path[MAX_PATH], *slash;
    DWORD len;
    if (!live_drive()) return;   /* E9g-c: no sounds outside a live drive */
    (void)len; (void)slash;
    if (!mf_sound_path(path, file)) return;
    play_at_car(path, 1.0f);   /* E7: normal class; R3j 3D placement */
}


/* ---- native low-fuel throttle cut (owner 2026-09-28) ----------------------
 * The game's own "under 1 litre" stutter is sub_447B10 (engine output
 * multiplier, called every physics tick from Gearbox_00443b70:
 * torque *= sub_447B10(this)):
 *     fuel >= 0.9 -> 1.0;  fuel <= 0 -> 0.0;
 *     else  mult = TrigTableLookup(this) * 1.5 + fuel + 0.1, capped at 1.0
 * (TrigTableLookup_00447ab0 = the game's noise table flt_8AAE74).
 * We detour it and, while a failure is active, apply the SAME formula with a
 * VIRTUAL fuel level - the real fuel (this+412) and the gauge are untouched.
 * The result can only ever be lower than the game's own. Prologue
 * `mov al,[ecx+1C59h]` (6 bytes, position independent) is stolen into a
 * trampoline; the hook runs on the game's physics thread. */
#define EID_PIT01_V     49      /* PWS_PIT01 "pit this lap" */
#define FUELCUT_SITE    0x00447B10u
#define FUELCUT_RESUME  0x00447B16u
#define FUELCUT_STOLEN  6
typedef double (__fastcall *FuelCutFn)(void *self, void *edx);
#define TrigTableLookup ((FuelCutFn)0x00447AB0u)
/* AUDITFIX X1: one aligned, ownership-checked transaction for engine detours.
 * The complete RX trampoline and pointer are published before the CAS.
 * Other process threads are paused; any EIP within the first8 refuses the
 * write. This closes execution inside the original instructions during CAS.
 * Native acceptance remains separate from synthetic thread-gate tests.
 * Pin BEFORE reachability: C entry counters cannot cover the pre-increment
 * entry window or the post-decrement return instructions. Never free stubs. */
typedef struct { MfPatch8 bytes; DWORD site; int live; } MfDetour;
static volatile LONG g_hooks_stopping = 0;
static int mf_detour_pin(void)
{
    static int pinned = 0;
    HMODULE self = NULL;
    if (pinned) return 1;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN
                           | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           (LPCSTR)(const void*)&mf_detour_pin, &self)) {
        logf_("HOOK: PIN failed - installation refused (error %lu)", GetLastError());
        return 0;
    }
    mf_exit_pin_required();
    pinned = 1;
    logf_("HOOK: module pinned for process lifetime (entry/return windows)");
    return 1;
}
static int mf_detour_install(MfDetour *d, DWORD va, unsigned stolen,
                             const BYTE *expect, const void *hook,
                             BYTE **tramp_out, const char *what)
{
    BYTE original[8], *tramp, *site = (BYTE*)(uintptr_t)va;
    SIZE_T read = 0;
    DWORD old, ignored, rel;
    LONG64 observed;
    MfThreadGate gate;
    if (d->live) return 1;
    if (*tramp_out) { /* no republish over a retained trampoline */
        logf_("%s: already attempted - refused", what); return 0;
    }
    if (!ReadProcessMemory(GetCurrentProcess(), site, original, 8, &read)
        || read != 8 || !mf_patch8_plan(&d->bytes, va, (DWORD)(uintptr_t)hook,
                                      stolen, expect, original)) {
        logf_("%s: prologue/alignment mismatch at 0x%lX - refused", what, va);
        return 0;
    }
    tramp = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!tramp) { logf_("%s: trampoline alloc failed - refused", what); return 0; }
    memcpy(tramp, original, stolen);
    tramp[stolen] = 0xE9;
    rel = (va + stolen) - ((DWORD)(uintptr_t)tramp + stolen + 5);
    memcpy(tramp + stolen + 1, &rel, sizeof(rel));
    if (!VirtualProtect(tramp, 32, PAGE_EXECUTE_READ, &ignored)
        || !FlushInstructionCache(GetCurrentProcess(), tramp, 32)) {
        logf_("%s: trampoline protection/cache failed - refused", what); return 0;
    }
    if (!mf_detour_pin()) return 0;
    *tramp_out = tramp;
    MemoryBarrier();
    d->site = va;
    if (!VirtualProtect(site, 8, PAGE_EXECUTE_READWRITE, &old)) {
        logf_("%s: site protection failed - refused", what); return 0;
    }
    if (!mft_begin(&gate, va, va + 8u)) {
        VirtualProtect(site, 8, old, &ignored);
        logf_("%s: thread/IP gate refused (%s, 0x%lX) - site untouched, pinned",
              what, mft_reason(&gate), gate.detail); return 0;
    }
    observed = InterlockedCompareExchange64((volatile LONG64*)site,
                    (LONG64)d->bytes.jump, (LONG64)d->bytes.original);
    d->live = mf_patch8_can_install(&d->bytes, (uint64_t)observed);
    {
    int flushed = FlushInstructionCache(GetCurrentProcess(), site, 8);
    mft_end(&gate);
    if (!flushed)
        logf_("%s: cache flush failed - pinned", what);
    }
    if (!VirtualProtect(site, 8, old, &ignored))
        logf_("%s: protection restore failed - pinned", what);
    if (!d->live) {
        logf_("%s: site changed before install at 0x%lX, left alone", what, va);
        return 0;
    }
    logf_("%s: atomic detour installed at 0x%lX (stolen %u)", what, va, stolen);
    return 1;
}
static int mf_detour_remove(MfDetour *d, volatile LONG *inflight, const char *what)
{
    BYTE *site = (BYTE*)(uintptr_t)d->site;
    BYTE verify[8]; SIZE_T read = 0;
    DWORD old, ignored;
    LONG64 observed;
    int i, restored = 0, drained;
    MfThreadGate gate;
    if (!d->live) return 1;
    if (!VirtualProtect(site, 8, PAGE_EXECUTE_READWRITE, &old)) {
        logf_("%s: remove protection failed - pinned", what);
    } else {
        int coordinated = mft_begin(&gate, d->site, d->site + 8u);
        observed = coordinated ? InterlockedCompareExchange64((volatile LONG64*)site,
                         (LONG64)d->bytes.original, (LONG64)d->bytes.jump)
                         : (LONG64)~d->bytes.jump;
        if (coordinated) {
            int flushed = FlushInstructionCache(GetCurrentProcess(), site, 8);
            mft_end(&gate);
            if (!flushed) observed = (LONG64)~d->bytes.jump;
        }

        restored = mf_patch8_can_remove(&d->bytes, (uint64_t)observed);
        if (!coordinated)
            logf_("%s (0x%lX) remove thread/IP gate refused (%s, 0x%lX), left alone",
                  what, d->site, mft_reason(&gate), gate.detail);
        else if (!restored)
            logf_("%s (0x%lX) changed by someone else, left alone", what, d->site);
        if (!FlushInstructionCache(GetCurrentProcess(), site, 8)) restored = 0;
        if (!VirtualProtect(site, 8, old, &ignored)) restored = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), site, verify, 8, &read)
            || read != 8 || mf_patch8_pack(verify) != d->bytes.original) restored = 0;
    }
    for (i = 0; i < 500 && InterlockedCompareExchange(inflight, 0, 0) != 0; ++i)
        Sleep(1);
    drained = InterlockedCompareExchange(inflight, 0, 0) == 0;
    if (!restored || !drained) {
        mf_exit_pin_required();
        logf_("%s: UNLOAD pinned (%s)", what,
              !restored ? "restore/ownership not verified" : "calls still in flight");
        return 0;
    }
    d->live = 0;
    logf_("%s: atomic detour removed; calls drained; module remains pinned", what);
    return 1;
}
static MfDetour g_fc_detour, g_cam_detour, g_imp_detour;

/* ---- 2026-10-07: engineer lines through the game's own commentary player --
 * How the game plays third-party driver-name clips (pws<Name>_FB.wav etc.,
 * FACT, decompile): CommentaryDriverEntryInit 0x414600 / FilePathExtensionCheck
 * 0x414900 store a 20-byte clip ref {+0 char* path (game heap, written by
 * StringCopyWithBufferRealloc 0x566650), +4 capacity, +8 clip -1, +12 -1,
 * +16 0}. A ref with a path is a loose WAV; a ref with a clip index is an SDT
 * clip. The commentary pump TextOverlayDisplayTimer 0x4186F0 asks
 * FontRenderingPrep 0x411840 (stdcall: queue item, out refs[3]) for the
 * clip list of the queued event and hands it to the player (vtable+4), then
 * frees each path with the game allocator.
 * We detour 0x411840: when the queued event is the token of OUR pending line,
 * we return ONE path ref to our WAV - exactly the shape of a name clip - so the
 * game plays it with its own loader, mixer, Announcer volume, radio filter and
 * queue rules. Nothing else changes: no SDT, no lookup/event table write. A
 * token event fired by anyone else (no pending line) goes to the original. */
#define VN_SITE        0x00411840u
#define VN_STOLEN      5          /* push ecx/ebx/ebp/esi/edi */
#define VN_STRCPY      0x00566650u
#define VN_PENDING_MS  5000u    /* audit MF-08: was 30 s */
#define VN_FALLBACK_EID 47         /* MF_ENG01 token for lines whose own row is once-only */
static const BYTE g_vn_expect[VN_STOLEN] = { 0x51, 0x53, 0x55, 0x56, 0x57 };
/* 1.0.1 crash fix: 0x411840 is THISCALL (disassembly: mov esi,ecx at
 * 0x41184C, ecx passed on to 0x411A00/0x411A50) - the decompile shows it
 * as stdcall. ECX must reach the original unchanged. */
typedef int (__attribute__((thiscall)) *VnPrepFn)(void *self, DWORD *item, BYTE *refs);
typedef int (__attribute__((thiscall)) *VnStrCpyFn)(void *ref, const char *src);
static MfDetour g_vn_detour;
static BYTE *g_vn_tramp = NULL;
static volatile LONG g_vn_inflight = 0;
static int g_vn_installed = 0;
static volatile LONG g_vn_line = -1;     /* pending line, -1 none   */
static volatile LONG g_vn_eid = -1;      /* its token event         */
static volatile LONG g_vn_at = 0;        /* GetTickCount at request */
static char g_vn_path[MFV_COUNT][MAX_PATH];

static int __attribute__((thiscall)) voice_native_hook(void *self, DWORD *item, BYTE *refs)
{
    int r;
    InterlockedIncrement(&g_vn_inflight);
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0) && item && refs) {
        LONG line = InterlockedCompareExchange(&g_vn_line, -1, -1);
        if (line >= 0 && line < MFV_COUNT
            && (LONG)item[0] == InterlockedCompareExchange(&g_vn_eid, -1, -1)
            && GetTickCount() - (DWORD)g_vn_at < VN_PENDING_MS
            && InterlockedCompareExchange(&g_vn_line, -1, line) == line) {
            if (!on_circuit()) {         /* entered the pits while queued */
                InterlockedDecrement(&g_vn_inflight);
                return 0;                /* nothing plays (not the donor clip) */
            }
            ((VnStrCpyFn)VN_STRCPY)(refs, g_vn_path[line]);
            *(int*)(refs + 8) = -1;
            *(int*)(refs + 12) = -1;
            refs[16] = 0;
            InterlockedDecrement(&g_vn_inflight);
            return 1;                    /* one clip: our WAV */
        }
    }
    r = ((VnPrepFn)g_vn_tramp)(self, item, refs);
    InterlockedDecrement(&g_vn_inflight);
    return r;
}

/* the path as the game would write a name clip: relative to the game folder
 * (the game's working directory); absolute when the relative one is not seen */
static int voice_native_paths(void)
{
    char abs[MAX_PATH], rel[MAX_PATH];
    int i;
    for (i = 0; i < MFV_COUNT; i++) {
        _snprintf(rel, sizeof rel, "Audio\\MechanicalFailures\\%s.wav", mfv_id(i));
        rel[sizeof rel - 1] = '\0';
        if (GetFileAttributesA(rel) != INVALID_FILE_ATTRIBUTES) {
            lstrcpynA(g_vn_path[i], rel, MAX_PATH);
            continue;
        }
        if (!voice_wav_path(abs, i)) return 0;
        lstrcpynA(g_vn_path[i], abs, MAX_PATH);
    }
    logf_("VOICE NATIVE: clip path %s", g_vn_path[0]);
    return 1;
}

static int voice_native_install(void)
{
    if (g_vn_installed) return 1;
    if (!voice_native_paths()) {
        logf_("VOICE NATIVE: WAV paths unresolved - private player fallback");
        return 0;
    }
    g_vn_installed = mf_detour_install(&g_vn_detour, VN_SITE, VN_STOLEN, g_vn_expect,
                                       (const void*)voice_native_hook, &g_vn_tramp,
                                       "VOICE NATIVE");
    if (!g_vn_installed) logf_("VOICE NATIVE: hook refused - private player fallback");
    return g_vn_installed;
}

static void voice_native_remove(void)
{
    InterlockedExchange(&g_vn_line, -1);
    if (mf_detour_remove(&g_vn_detour, &g_vn_inflight, "VOICE NATIVE")) g_vn_installed = 0;
}

/* 2026-10-07 audit MF-09: the commentary queue head is a multi-field game
 * structure - write it on the game thread (broker post), never from the
 * worker. Without a broker the old direct write stays (logged once). */
static F1CBrokerApi g_cm_api;
static F1CBrokerToken g_cm_token;
static volatile LONG g_cm_broker = 0;   /* 1 = registered: posts only */
static int g_cm_broker_done = 0;
static DWORD g_cm_broker_t0 = 0;

static void __cdecl commentary_task(void *ctx)
{
    int eid = (int)(intptr_t)ctx;
    if (!commentary_say_now(eid)
        && InterlockedCompareExchange(&g_vn_eid, -1, -1) == eid)
        InterlockedExchange(&g_vn_line, -1);   /* dropped: no stale token */
}

static int commentary_say(int eid)
{
    if (InterlockedCompareExchange(&g_cm_broker, 0, 0)) {
        F1CBrokerResult r = g_cm_api.post(g_cm_token, commentary_task,
                                          (void*)(intptr_t)eid);
        if (r == F1C_BROKER_OK) {
            logf_("commentary_say(%d): posted to the game thread", eid);
            return 1;
        }
        logf_("commentary_say(%d): broker post failed (%d) - line dropped",
              eid, (int)r);
        return 0;
    }
    return commentary_say_now(eid);
}

/* worker pass: bounded discovery (10 s, like esc_pause_menu), then register */
static void cm_broker_pump(void)
{
    F1CBrokerDiscoveryResult d;
    if (g_cm_broker_done) return;
    if (!g_cm_broker_t0) g_cm_broker_t0 = GetTickCount() | 1u;
    memset(&g_cm_api, 0, sizeof g_cm_api);
    g_cm_api.structure_size = sizeof g_cm_api;
    d = f1c_broker_discover(5u, F1C_BROKER_ABI_VERSION,
                            F1C_BROKER_CAP_LIFECYCLE, &g_cm_api);
    if ((d == F1C_BROKER_DISCOVERY_NOT_FOUND
         || d == F1C_BROKER_DISCOVERY_QUERY_FAILED)
        && GetTickCount() - g_cm_broker_t0 < 10000u)
        return;                                   /* retry next pass */
    g_cm_broker_done = 1;
    if (d != F1C_BROKER_DISCOVERY_OK) {
        logf_("COMMENTARY: broker discovery %d - direct queue writes (old path)",
              (int)d);
        return;
    }
    if (g_cm_api.register_client(g_hinst, "mechanical_failures.commentary",
                                 &g_cm_token) != F1C_BROKER_OK) {
        logf_("COMMENTARY: broker register failed - direct queue writes (old path)");
        f1c_broker_release(&g_cm_api);
        return;
    }
    InterlockedExchange(&g_cm_broker, 1);
    logf_("COMMENTARY: broker ABI %u - queue writes run on the game thread",
          g_cm_api.negotiated_abi);
}

/* 1 = handed to the game's commentary queue. Token = the line's own donor
 * event; a donor whose native row is once-per-session (cooldown FLT_MAX)
 * borrows MF_ENG01's token. Rows are read, never written. */
static int voice_native_say(int line)
{
    int eid;
    float cd;
    if (!g_vn_installed || line < 0 || line >= MFV_COUNT) return 0;
    eid = mfv_eid(line);
    cd = *(volatile float*)(CM_EVENT_TABLE + (DWORD)eid * 0x14u + 0x0Cu);
    if (!(cd < 1e37f)) eid = VN_FALLBACK_EID;
    {
        LONG pend = InterlockedCompareExchange(&g_vn_line, -1, -1);
        if (pend >= 0 && GetTickCount() - (DWORD)g_vn_at < VN_PENDING_MS) {
            logf_("VOICE: %s not queued natively - %s still pending",
                  mfv_id(line), mfv_id((int)pend));
            return 0;                  /* audit MF-08: never overwrite */
        }
    }
    InterlockedExchange(&g_vn_at, (LONG)GetTickCount());
    InterlockedExchange(&g_vn_eid, eid);
    InterlockedExchange(&g_vn_line, line);
    if (!commentary_say(eid)) {
        InterlockedCompareExchange(&g_vn_line, -1, line);
        logf_("VOICE: %s native hand-off refused - private player", mfv_id(line));
        return 0;                      /* audit MF-08: caller falls back */
    }
    logf_("VOICE: %s queued in the game's commentary (token eid %d) (%s)",
          mfv_id(line), eid, mfv_text(line));
    return 1;
}
static MfDetour g_lamp_eng_detour, g_lamp_fuel_detour, g_po_detour;

static BYTE *g_fc_tramp = NULL;
static int   g_fc_installed = 0;
/* E9-H1b: in-flight hook calls. Incremented on entry to the detour body and
 * decremented on every exit; the unload path waits for 0 (bounded) before
 * restoring the six bytes, and pins the module when it cannot prove safety. */
static volatile LONG g_fc_inflight = 0;
static volatile float g_vfuel_press = -1.0f;   /* fuel pressure, <0 = off */
static volatile float g_vfuel_valve = -1.0f;   /* valve float,   <0 = off */
/* E3: model power lever (handoff 4.8). Multiplies the native output
 * multiplier through the same detour; min(result, orig x mult) can only
 * ever lower torque. 1.0 = no effect; reset on repair_all, session change
 * and unload. */
static volatile float g_power_mult = 1.0f;

/* E9e: torque-curve power shape mirrors. The pump copies the model's located
 * losses here and the hook applies the rpm/throttle shape per physics step
 * (loss_top above the car's peak-torque rpm, loss_all flat, loss_load with
 * throttle). g_v_throttle mirrors MfInputs.load for the hook. */
static volatile float g_loss_top = 0.0f;
static volatile float g_loss_all = 0.0f;
static volatile float g_loss_load = 0.0f;
static volatile float g_v_throttle = 0.0f;
/* E9d item 3: the pump copies MfOut.misfire_depth here; the hook pulses the
 * native output multiplier with the game's own rhythmic pattern. */
static volatile float g_misfire_depth = 0.0f;
/* E9-H2e: 1 = the misfire may run the discrete cuts (worn-component pattern
 * or an emergency onset); 0 = a harmless niggle pattern only. */
static volatile int   g_misfire_discrete = 1;

/* E9b-2 item 9: discrete misfire cuts, scheduled in GAME time (SESSION_TIME)
 * so CTRL+T behaves. Rate MF_MISFIRE_EVT_HZ_MAX x depth / 0.6 per game
 * second; each cut MF_MISFIRE_CUT_MIN_S..MAX_S long; during a cut the output
 * is multiplied by (1 - min(1, 1.5 x depth)). Needs depth >= 0.1 and rpm at
 * 30 % of the limit or more (no stall at idle). Random intervals come from a
 * hook-local xorshift mirror: the hook runs on the game thread and cannot
 * touch the pump's MfEngine rng state. */
static double   g_mis_cut_end = 0.0;    /* session time the cut ends, 0 = none */
static double   g_mis_next_cut = 0.0;   /* session time the next cut starts, 0 = undecided */
static uint32_t g_mis_rng = 0x9E3779B9u;

static double mis_rng_d(void)
{
    uint32_t x = g_mis_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_mis_rng = x;
    return (double)(x >> 8) / 16777216.0;
}

static double __fastcall fuelcut_hook(void *self, void *edx)
{
    double orig;
    float a, b, vf;
    double m, res;
    unsigned gen;

    InterlockedIncrement(&g_fc_inflight);
    gen = g_session_gen;
    orig = ((FuelCutFn)g_fc_tramp)(self, edx);
    if (InterlockedCompareExchange(&g_hooks_stopping, 0, 0)) {
        InterlockedDecrement(&g_fc_inflight);
        return orig;
    }
    a = g_vfuel_press;
    b = g_vfuel_valve;

    /* E9g-e item 2: the trampoline call above is kept (the game relies on its
     * side effects). A latched engine-dead returns 0 torque from here on; the
     * ignition byte is never written, so the dash lights stay on. This sits
     * after the particle queue drain below on purpose: the blow fireball /
     * smoke bursts are spawned in this same hook and must keep rendering
     * after the engine is dead. */
    if (g_pq_n > 0 && g_pq_cs_ok == 2 && TryEnterCriticalSection(&g_pq_cs)) {
        PBurst q[PQ_MAX];                     /* game thread: spawn queued */
        int k, cnt = (int)g_pq_n;
        memcpy(q, g_pq, sizeof(PBurst) * (size_t)cnt);
        g_pq_n = 0;
        LeaveCriticalSection(&g_pq_cs);
        for (k = 0; k < cnt; k++) {
            if (q[k].gen != gen) continue;    /* H1c: stale-session burst */
            if (q[k].id & 0x200)              /* N3e: AI car burst */
                emit_burst_now_at(q[k].id & 0xFF, q[k].n, q[k].rate,
                                  q[k].spread, q[k].up, q[k].slot);
            else if (q[k].id & 0x100)
                emit_burst_now(q[k].id & 0xFF, q[k].n, q[k].rate, q[k].spread, q[k].up);
            else
                emit_bluesmoke_now(q[k].n, q[k].rate, q[k].slot);
        }
    }
    if (g_mfc_rq_n > 0) mfc_rq_drain(gen);   /* CO-OP: effects at other PCs' cars */
    if (g_engine_dead) {              /* E9g-e item 2: dead = no torque at all */
        InterlockedDecrement(&g_fc_inflight);
        return 0.0;
    }
    vf = a < 0.0f ? b : (b < 0.0f ? a : (a < b ? a : b));
    res = orig;
    if (vf >= 0.0f && vf < 0.9f && orig > 0.0) {
        if (vf < 0.02f) vf = 0.02f;
        m = TrigTableLookup(self, edx) * 1.5 + (double)vf + 0.1;
        if (m > 1.0) m = 1.0;
        if (m < res) res = m;
    }
    /* E3: model power lever - min(result, orig x g_power_mult), never higher */
    {
        double lim = orig * (double)g_power_mult;
        if (res > lim) res = lim;
    }
    /* E9e (addendum 2 section 3 item 7): torque-curve power shaping applied
     * per physics step from the pump's mirrored losses; never raises torque,
     * floor MF_POWER_MIN. Same math as mf_power_shape_mult() in the model. */
    {
        double lt = (double)g_loss_top, la = (double)g_loss_all,
               ll = (double)g_loss_load;
        if (lt != 0.0 || la != 0.0 || ll != 0.0) {
            double rpm = (double)ENG_RPM_LIVE;
            double peak = g_model_car.peak_tq_rads;
            double lim_r = g_model_car.rev_limit_rads;
            double s = 0.0, shape, lim2;
            if (rpm > peak) {
                if (!(lim_r > peak) || rpm >= lim_r) {
                    s = 1.0;
                } else {
                    double x = (rpm - peak) / (lim_r - peak);
                    s = x * x * (3.0 - 2.0 * x);
                }
            }
            shape = 1.0 - la - lt * s - ll * (double)g_v_throttle;
            if (shape > 1.0) shape = 1.0;
            if (shape < MF_POWER_MIN) shape = MF_POWER_MIN;
            lim2 = orig * (double)g_power_mult * shape;
            if (res > lim2) res = lim2;
        }
    }
    /* E9d item 3: rhythmic misfire - the game's own rough-running pattern
     * (Gearbox 0x443B70, finding native-rough-running-stress-throttle):
     *   x = 11.5 / revLimit x sessionTime x rpm
     *   n = linear interpolation of the 256-float noise table at |x|
     *   output x (1 - n^2 x depth), depth from the pump (0..0.6).
     * Symptom only: ENG_STRESS is NEVER written (it drains life). */
    {
        float depth = g_misfire_depth;
        if (depth > 0.0f && orig > 0.0) {
            double x = 11.5 / (g_model_car.rev_limit_rads > 100.0
                               ? g_model_car.rev_limit_rads : 1.0)
                       * SESSION_TIME * (double)ENG_RPM_LIVE;
            double a = fabs(x);
            unsigned i0 = (unsigned)(int)a & 0xFFu;
            double f = a - (double)(int)a;
            const float *nt = MF_NOISE_TABLE;
            double n = (double)nt[i0]
                     + ((double)nt[(i0 + 1u) & 0xFFu] - (double)nt[i0]) * f;
            double lim3 = orig * (1.0 - n * n * (double)depth);
            if (res > lim3) res = lim3;
        }
    }
    /* E9b-2 item 9: discrete misfire cuts (game time). */
    {
        float depth = g_misfire_depth;
        double t = SESSION_TIME;
        double lim_r = g_model_car.rev_limit_rads;
        int active = (double)depth >= MF_MISFIRE_CUT_DEPTH_MIN
                     && g_misfire_discrete          /* H2e: not a niggle */
                     && orig > 0.0 && !g_engine_dead && t > 0.0
                     && lim_r > 100.0
                     && (double)ENG_RPM_LIVE >= MF_MISFIRE_CUT_REV_MIN * lim_r;
        if (!active) {
            g_mis_cut_end = 0.0;
            g_mis_next_cut = 0.0;
        } else if (g_mis_cut_end > 0.0 && t < g_mis_cut_end) {
            double lim4 = orig * (1.0 - mf_misfire_cut_strength((double)depth));
            if (res > lim4) res = lim4;
        } else {
            if (g_mis_cut_end > 0.0) {          /* the previous cut just ended */
                g_mis_cut_end = 0.0;
                g_mis_next_cut = 0.0;
            }
            if (g_mis_next_cut <= 0.0) {
                double rate = MF_MISFIRE_EVT_HZ_MAX * (double)depth / 0.6;
                double u = mis_rng_d();
                double gap = rate > 1e-9 ? -log(u > 1e-12 ? u : 1e-12) / rate : 1e9;
                g_mis_next_cut = t + gap;
            }
            if (t >= g_mis_next_cut) {
                double u = mis_rng_d();
                double lim4;
                g_mis_cut_end = t + MF_MISFIRE_CUT_MIN_S
                                + (MF_MISFIRE_CUT_MAX_S - MF_MISFIRE_CUT_MIN_S) * u;
                g_mis_next_cut = 0.0;
                lim4 = orig * (1.0 - mf_misfire_cut_strength((double)depth));
                if (res > lim4) res = lim4;
            }
        }
    }
    InterlockedDecrement(&g_fc_inflight);   /* E9-H1b: hook body done */
    return res;
}

/* E9-H1a: this install REFUSES (returns 0) instead of log-and-continue. The
 * host aggregate gate has already validated the same bytes, so a failure here
 * means the image changed under us - the worker exits, no pump runs. */
static int fuelcut_install(void)
{
    static const BYTE expect[FUELCUT_STOLEN] = { 0x8A, 0x81, 0x59, 0x1C, 0x00, 0x00 };
    g_fc_installed = mf_detour_install(&g_fc_detour, FUELCUT_SITE, FUELCUT_STOLEN,
                        expect, (const void*)fuelcut_hook, &g_fc_tramp, "FUELCUT");
    return g_fc_installed;
}

/* E9-H1b: restore the six bytes, VERIFY they equal the saved original, then
 * wait (bounded, 500 ms) until no hook call is in flight. Returns 1 when it is
 * proved safe to unload; on any failure requests the module PIN (the host
 * keeps it mapped) and logs `UNLOAD: pinned (<why>)`. */
static int fuelcut_remove(void)
{
    int ok;
    g_vfuel_press = -1.0f;
    g_vfuel_valve = -1.0f;
    g_power_mult = 1.0f;                      /* E3: unload reset */
    g_loss_top = 0.0f;                        /* E9e: unload resets the shape */
    g_loss_all = 0.0f;
    g_loss_load = 0.0f;
    g_v_throttle = 0.0f;
    g_misfire_depth = 0.0f;                   /* E9d: unload reset */
    g_misfire_discrete = 1;                   /* H2e: unload reset */
    g_mis_cut_end = 0.0;                      /* E9b-2: unload resets the cuts */
    g_mis_next_cut = 0.0;
    ok = mf_detour_remove(&g_fc_detour, &g_fc_inflight, "FUELCUT");
    if (ok) g_fc_installed = 0;
    return ok;
}

/* ---- R3j: read-only camera tap (3D audio listener) -----------------------
 * Findings: camera-funnel-541cd0-freecam-hook (the funnel 0x541CD0 is the
 * safe choke point; expected stub bytes documented by the freecam SDK mod)
 * and freecam-render-path-541cd0-18c (FACT: the renderer reads
 * receiver+0x18C..0x194 as the camera world x/y/z; the camera basis columns
 * live at +0x15C/0x16C/0x17C, written by RandomRotationMatrixFromEulerAngles
 * 0x541DB0 before OrientationRotationMatrixUpdate 0x541D10 stores R*v).
 * We NEVER write game memory: the detour steals the first 8 prologue bytes
 * (two position-independent movs) into a trampoline, runs the original, then
 * copies the sample out. Vehicle/chassis receivers are skipped (freecam live
 * finding: 0x409A00 also calls the funnel with the chassis transform). */
#define CAM_FUNNEL_SITE   0x00541CD0u
#define CAM_FUNNEL_RESUME 0x00541CD8u
#define CAM_FUNNEL_STOLEN 8
static const BYTE g_cam_expect[CAM_FUNNEL_STOLEN] =
    { 0x8B, 0x44, 0x24, 0x18, 0x8B, 0x54, 0x24, 0x10 };
volatile float mf_cam_x = 0.0f, mf_cam_y = 0.0f, mf_cam_z = 0.0f;
volatile float mf_cam_rx = 1.0f, mf_cam_ry = 0.0f, mf_cam_rz = 0.0f;
volatile unsigned mf_cam_ms = 0;
volatile int mf_cam_valid = 0;

typedef int (__attribute__((thiscall)) *CamFunnelFn)(void*, int, int,
                                                     float, float, float, float);
static BYTE *g_cam_tramp = NULL;
static int   g_cam_installed = 0;
static volatile LONG g_cam_inflight = 0;

static int __attribute__((thiscall)) cam_funnel_hook(void *recv, int a1, int a2,
    float a3, float a4, float a5, float a6)
{
    int r;
    uintptr_t p;
    InterlockedIncrement(&g_cam_inflight);
    p = (uintptr_t)recv;
    r = ((CamFunnelFn)g_cam_tramp)(recv, a1, a2, a3, a4, a5, a6);
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0)
        && p && !(p >= 0x005EC000u && p < 0x00640000u) && p != 0x007C54E0u
        && !IsBadReadPtr((const void*)(p + 0x15Cu), 0x40u)) {
        float x = *(volatile float*)(p + 0x18Cu);
        float y = *(volatile float*)(p + 0x190u);
        float z = *(volatile float*)(p + 0x194u);
        if (x == x && y == y && z == z                      /* finite */
            && fabs((double)x) < 1e6 && fabs((double)y) < 1e6
            && fabs((double)z) < 1e6) {
            mf_cam_x = x; mf_cam_y = y; mf_cam_z = z;
            mf_cam_rx = *(volatile float*)(p + 0x15Cu);
            mf_cam_ry = *(volatile float*)(p + 0x160u);
            mf_cam_rz = *(volatile float*)(p + 0x164u);
            mf_cam_ms = GetTickCount();
            mf_cam_valid = 1;
        }
    }
    InterlockedDecrement(&g_cam_inflight);
    return r;
}

static int cam_tap_install(void)
{
    g_cam_installed = mf_detour_install(&g_cam_detour, CAM_FUNNEL_SITE, CAM_FUNNEL_STOLEN,
                    g_cam_expect, (const void*)cam_funnel_hook, &g_cam_tramp, "CAM TAP");
    return g_cam_installed;
}

static int cam_tap_remove(void)
{
    int ok = mf_detour_remove(&g_cam_detour, &g_cam_inflight, "CAM TAP");
    if (ok) g_cam_installed = 0;
    mf_cam_valid = 0;
    return ok;
}

/* ---- E9-H1a: aggregate executable compatibility gate --------------------
 * One manifest of every code pattern this family requires, plus the fixed
 * layout reads that must be plausible in ANY game state (menu, garage,
 * session). The host calls mf_engine_compat() before starting ANY family; the
 * worker's fuelcut_install re-checks its own site independently. */
static int mf_compat_fetch_live(unsigned int addr, unsigned char *dst, int len)
{
    if (len < 0) return 0;
    if (IsBadReadPtr((const void*)(uintptr_t)addr, (UINT_PTR)len)) return 0;
    memcpy(dst, (const void*)(uintptr_t)addr, (size_t)len);
    return 1;
}

const char *mf_engine_compat(void)
{
    static const MfCompatItem items[] = {
        { FUELCUT_SITE,
          { 0x8A, 0x81, 0x59, 0x1C, 0x00, 0x00 },
          FUELCUT_STOLEN, "engine detour prologue 0x447B10" },
    };
    const char *bad = mf_compat_verify(items,
                                       (int)(sizeof items / sizeof items[0]),
                                       mf_compat_fetch_live);
    double t;
    int st, ph;

    if (bad) return bad;
    /* layout reads: the session globals every pump gates on. A mismatched
     * executable that moved them fails one of these range checks. */
    t = SESSION_TIME;
    if (!isfinite(t) || fabs(t) > 1e9) return "session clock layout 0x8AFEA0";
    st = SESSION_TYPE;
    if (st < -2 || st > 16) return "session type layout 0x7C56A4";
    ph = RACE_PHASE;
    if (ph < -1 || ph > 1000) return "race phase layout 0x7C56A8";
    return NULL;
}

/* Game time in ms (owner 2026-09-28): failures pause with the game (ESC/P)
 * and speed up with time acceleration (CTRL+T). Session clock dbl_8AFEA0. */
static DWORD game_ms(void)
{
    double t = SESSION_TIME;
    if (!(t > 0.0) || t > 4.0e6) return 0;
    return (DWORD)(t * 1000.0);
}

/* Load factor (owner 2026-09-28): problems grow mainly while the engine is
 * worked; at idle they linger. Legacy (MF_LEGACY_*) maths only. */
#if (MF_LEGACY_VALVE || MF_LEGACY_OILP || MF_LEGACY_COOLING || MF_LEGACY_FUEL \
     || MF_LEGACY_EXHAUST || MF_LEGACY_BLOWBY)
static float load_factor(void)
{
    float r = rev_frac();
    if (r > 1.0f) r = 1.0f;
    return 0.1f + 0.9f * r;
}
#endif

/* ---- N1 valve return force fault (E4a: model-driven; E9c era-neutral) ----
 * The physics (return force R, leak, valve-event use, leak area growth with
 * revs, float damage, the rev ceiling) lives in the model (mf_engine_model.h
 * 4.1). This pump maps model state / MfOut onto the game levers: the native
 * rev band (rev_band_set), the valve-float misfire (g_vfuel_valve), exhaust
 * pops and the HUD lines (LOSING REVS / VALVE FLOAT / SHIFT EARLIER). The pit
 * top-up is removed: MF_VALVE_PIT_TOPUP 0 means there is nothing to refill,
 * so no pit advice is shown either. A manual trigger is a forced model onset
 * (medium severity, virtual budget, ignoring the game clock - test use only).
 * Old percentage maths: MF_LEGACY_VALVE. */
#define MF_VALVE_PIT_TOPUP 0      /* E9c: generic fault - nothing to top up */
static int   g_valve_on = 0;      /* model valve fault active (mirror) */
static int   g_valve_cut = 0;
static int   g_valve_topped = 0;  /* only used while MF_VALVE_PIT_TOPUP */
#if MF_LEGACY_VALVE
static float g_valve_air = 100.0f;
static float g_valve_rate = 0.4f;
static float g_valve_abuse = 0.0f;
static DWORD g_valve_last = 0;
#endif
/* Native rev limiter band (worker #1 Q3, sub_43C250:487-492): the cut is
 * the game's torque fade between this+7396 and this+7404 around the limit
 * this+7388 (all double, rad/s); this+7316 = 0.975 x limit. Loaded once per
 * session; we scale all four and restore them (E9g-b: ownership-checked). */
#define REV_LIM   (*(volatile double*)0x007C9104u)   /* +7388 */
#define REV_LO    (*(volatile double*)0x007C910Cu)   /* +7396 */
#define REV_HI    (*(volatile double*)0x007C9114u)   /* +7404 */
#define REV_SHIFT (*(volatile double*)0x007C90BCu)   /* +7316 (hotfix 2026-09-30: was 0x7C90B4 = +7308 idle-high rpm) */
static MfLever g_rev[4];                  /* REV_LIM, REV_LO, REV_HI, REV_SHIFT (E9g-b) */
/* 2026-10-06 shift lights: lowered limit (rad/s) published by every rev-band
 * write/restore (valve ceiling, save mode, dead latch, legacy valve); 0 = stock. */
static volatile float g_shift_lim = 0.0f;
static int    g_valve_pitdone = 0;
static double g_valve_ceil_log = -1.0;   /* E8f: last logged float-ceiling fraction */

/* E9g-b: capture the four stock band values (self-write) on first use. */
static void rev_band_capture(void)
{
    if (g_rev[0].held) return;
    if (!(REV_LIM > 100.0)) return;              /* not loaded */
    lever_write(&g_rev[0], &REV_LIM,   REV_LIM);
    lever_write(&g_rev[1], &REV_LO,    REV_LO);
    lever_write(&g_rev[2], &REV_HI,    REV_HI);
    lever_write(&g_rev[3], &REV_SHIFT, REV_SHIFT);
}

/* H3d: the restore log is labelled by its OWNER (save vs valve), not always
 * "VALVE RETURN" (live finding 3: save-off used the valve label). */
static void rev_band_restore(const char *owner)
{
    double stock;
    int wrote = 0;

    if (!g_rev[0].held) return;
    stock = g_rev[0].saved;
    wrote |= lever_restore(&g_rev[0], &REV_LIM,   "REV_LIM");
    wrote |= lever_restore(&g_rev[1], &REV_LO,    "REV_LO");
    wrote |= lever_restore(&g_rev[2], &REV_HI,    "REV_HI");
    wrote |= lever_restore(&g_rev[3], &REV_SHIFT, "REV_SHIFT");
    g_valve_ceil_log = -1.0;         /* E8f: log the ceiling again after a restore */
    g_shift_lim = 0.0f;
    if (wrote)
        logf_("%s: native rev limit restored (%.1f)", owner, stock);
}

/* limit at `frac` of the stock limit (1.0 = stock); the band keeps its width */
static void rev_band_set(double frac)
{
    double lim;
    rev_band_capture();
    if (!g_rev[0].held) return;                  /* not loaded */
    lim = g_rev[0].saved * frac;
    lever_write(&g_rev[0], &REV_LIM,   lim);
    lever_write(&g_rev[1], &REV_LO,    lim - (g_rev[0].saved - g_rev[1].saved));
    lever_write(&g_rev[2], &REV_HI,    lim + (g_rev[2].saved - g_rev[0].saved));
    lever_write(&g_rev[3], &REV_SHIFT, lim * 0.975);
    g_shift_lim = (lim < g_rev[0].saved * 0.999) ? (float)lim : 0.0f;
}

/* E8f (live check test 2): the model ceiling fraction (of the stock limit,
 * rad/s) must stop the driver BEFORE the native limiter, not above it. The
 * WHOLE limiter band is placed under the ceiling: ceil_rads = ceil_frac x
 * stock limit; band_hi = stock band above the limit; lim = ceil_rads -
 * band_hi - MF_VALVE_LIM_MARGIN x stock limit (0.5 % clearance so sitting on
 * the limiter is not constant valve float). Never above stock. */
#define MF_VALVE_LIM_MARGIN 0.005
static void rev_band_set_ceiling(double ceil_frac)
{
    double ceil_rads, band_hi, lim;
    rev_band_capture();
    if (!g_rev[0].held) return;                  /* not loaded */
    ceil_rads = ceil_frac * g_rev[0].saved;
    band_hi = g_rev[2].saved - g_rev[0].saved;
    lim = ceil_rads - band_hi - MF_VALVE_LIM_MARGIN * g_rev[0].saved;
    if (lim > g_rev[0].saved) lim = g_rev[0].saved;  /* never above stock */
    rev_band_set(lim / g_rev[0].saved);   /* REV_LIM/LO/HI/SHIFT exactly as rev_band_set does */
    if (g_valve_ceil_log < 0.0 || fabs(ceil_frac - g_valve_ceil_log) > 0.01) {
        g_valve_ceil_log = ceil_frac;
        logf_("VALVE: float ceiling %.0f rpm, limiter %.0f rpm",
              ceil_rads * 60.0 / (2.0 * 3.14159265358979323846),
              lim * 60.0 / (2.0 * 3.14159265358979323846));
    }
}

/* E9i: the single owner of the native rev band. The model's valve ceiling and
 * the save-mode 0.97 limit are combined (lower wins); the E9g-e dead latch
 * keeps the band lowered after a terminal. Called from model_pump after each
 * model step, so g_model_out is valid (the old valve_pump band writes are
 * removed - one owner avoids the two pumps fighting over the band). */
static int g_rev_owner = 0;   /* H3a/H3d: 0 none, 1 valve ceiling, 2 save */

static void rev_band_pump(void)
{
    double ceil = -1.0;
    int owner = 0;

    if (g_model_out.rev_ceiling >= 0.0 && g_model_out.rev_ceiling < 0.999) {
        ceil = g_model_out.rev_ceiling;
        owner = 1;
    }
    if (g_save_mode && (ceil < 0.0 || MF_SAVE_REV_MULT < ceil)) {
        ceil = MF_SAVE_REV_MULT;
        owner = 2;
    }
    if (owner == 1) {
        /* 2026-10-07 tester report ("reduced engine limiter part way through
         * the race, no notification"): the valve ceiling lowers the limiter
         * from its first percent, the VALVE warning needs a lower pressure.
         * A noticeable cut (> 2 %) now tells the driver once per fault (HUD max 31 chars). */
        static double said_onset = -1.0;
        if (ceil < 0.98 && g_model_eng.onset_t_s != said_onset
            && live_drive() && !wrecked_or_stopped()) {
            said_onset = g_model_eng.onset_t_s;
            hud_show("REV LIMIT DOWN - VALVE AIR");
            logf_("NOTIFY: rev limit reduced to %.0f%% (valve ceiling) - shown",
                  100.0 * ceil);
        }
        rev_band_set_ceiling(ceil);
        g_rev_owner = 1;
    } else if (owner == 2) {
        /* H3a live fix (rev limit 1710 vs stock 1780 = x0.961): the save
         * limit is the DRIVER's band multiplied by x0.97, not a physical
         * ceiling - applying it through rev_band_set_ceiling subtracted the
         * stock band width plus the limiter clearance on top. The whole band
         * scales here, so the effective limit IS x0.97 of stock. */
        rev_band_set(MF_SAVE_REV_MULT);
        if (g_rev_owner != 2) {
            logf_("SAVE: rev band x%.2f (limit %.0f rpm)", MF_SAVE_REV_MULT,
                  REV_LIM * 60.0 / (2.0 * 3.14159265358979323846));
        }
        g_rev_owner = 2;
    } else if (!g_engine_dead) {           /* E9g-e item 4 */
        rev_band_restore(g_rev_owner == 2 ? "SAVE" : "VALVE RETURN");
        g_rev_owner = 0;
    }
}

/* Variance (owner 2026-09-29: "always the same rate"). Rolled per leak:
 *   hole size   - base rate 0.15..0.9 %/s, skewed small;
 *   character   - 0 seep (steady), 1 worsening seal (rate grows + sudden
 *                 steps), 2 self-sealing (stops at a rolled plateau);
 *   flow        - leak flow falls with pressure (slower as the return force drops);
 *   jitter      - rate wanders +-30% every 3..8 s. (legacy maths only; the
 *   model has its own variance.) */
#if MF_LEGACY_VALVE
static int   g_valve_kind = 0;
static float g_valve_plateau = 0.0f;
static float g_valve_jit = 1.0f;
static DWORD g_valve_jit_next = 0;
static DWORD g_valve_step_next = 0;
#endif

static void arm_valve_leak(float start_air)
{
#if MF_LEGACY_VALVE
    float r = frand01();
    g_valve_on = 1;
    g_valve_air = start_air;
    g_valve_rate = 0.15f + 0.75f * r * r;         /* %/s, skewed small */
    r = frand01();
    g_valve_kind = r < 0.45f ? 0 : (r < 0.80f ? 1 : 2);
    g_valve_plateau = 45.0f + 30.0f * frand01();  /* self-seal stops here */
    g_valve_jit = 1.0f;
    g_valve_jit_next = 0;
    g_valve_step_next = 0;
    g_valve_abuse = 0.0f;
    g_valve_last = 0;
    g_valve_topped = 0;
    logf_("VALVE RETURN fault armed: return %.0f%%, base %.2f %%/s, kind %s", start_air,
          g_valve_rate, g_valve_kind == 0 ? "seep" :
          g_valve_kind == 1 ? "worsening" : "self-restoring");
    if (g_valve_kind == 2) logf_("VALVE RETURN: settles near %.0f%%", g_valve_plateau);
    if (g_valve_kind == 2)
        mfv_publish("engine", "VALVE RETURN: self-restoring near %.0f%%, speed %.2f%%/s",
                    g_valve_plateau, g_valve_rate);
    else
        mfv_publish("engine", "VALVE RETURN: %s, speed %.2f%%/s",
                    g_valve_kind == 0 ? "seep" : "worsening", g_valve_rate);
#else
    (void)start_air;   /* E4a: manual trigger = forced medium onset (handoff E4) */
    g_valve_topped = 0;
    g_valve_pitdone = 0;
    mf_fault_arm(&g_model_eng, &g_model_in, MF_FAULT_VALVE_AIR,
                 MF_SEV_MEDIUM, MF_MANUAL_BUDGET_S);
    logf_("VALVE RETURN: model fault armed (manual, medium, budget %.0f s)",
          MF_MANUAL_BUDGET_S);
#endif
}

#if MF_LEGACY_VALVE
/* current leak rate %/s before load */
static float valve_rate_now(DWORD now, float dt)
{
    float flow = 0.35f + 0.65f * (g_valve_air / 100.0f);
    if (!g_valve_jit_next || (long)(now - g_valve_jit_next) >= 0) {
        g_valve_jit = 0.7f + 0.6f * frand01();
        g_valve_jit_next = now + 3000 + (DWORD)(rand() % 5001);
    }
    if (g_valve_kind == 1) {
        g_valve_rate += dt * 0.004f;              /* seal wears */
        if (!g_valve_step_next)
            g_valve_step_next = now + 20000 + (DWORD)(rand() % 40001);
        else if ((long)(now - g_valve_step_next) >= 0) {
            g_valve_rate *= 1.4f + 0.6f * frand01();  /* seal tears further */
            g_valve_step_next = 0;
            logf_("VALVE RETURN: leak worsened, base now %.2f %%/s", g_valve_rate);
        }
        if (g_valve_rate > 2.5f) g_valve_rate = 2.5f;
    }
    if (g_valve_kind == 2 && g_valve_air <= g_valve_plateau) return 0.0f;
    return g_valve_rate * flow * g_valve_jit;
}
#endif /* MF_LEGACY_VALVE */

static void valve_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_VALVE
    DWORD now = game_ms();
    float dt, ceil, rf;

    if (g_ctl) g_ctl->valve_air_pct = g_valve_on ? (int)g_valve_air : -1;
    if (!g_valve_on) return;
    if (!g_valve_last) { g_valve_last = now; return; }
    dt = (float)(now - g_valve_last) / 1000.0f;
    g_valve_last = now;
    if (dt > 1.0f) dt = 1.0f;

#if MF_VALVE_PIT_TOPUP
    if (PIT_STATUS == 3 || (PIT_STATUS >= 4 && g_valve_pitdone == 0 && g_valve_topped)) {
        /* crew tops up the return force - costs 3-5 s on the stop (Q10) */
        if (!g_valve_topped) {
            g_valve_topped = 1;
            g_valve_pitdone = 0;
            g_valve_air = 100.0f;
            g_valve_abuse = 0.0f;
            logf_("VALVE RETURN: topped up in the pits (leak remains)");
            hud_show("VALVE RETURN REFILLED");
        }
        pit_repair(3.0f + 2.0f * frand01(), &g_valve_pitdone);
    } else
#endif
    {
        g_valve_topped = 0;
        g_valve_pitdone = 0;
        g_valve_air -= dt * valve_rate_now(now, dt) * load_factor();
        if (g_valve_air < 0.0f) g_valve_air = 0.0f;
    }

    ceil = g_valve_air >= 70.0f ? 1.2f : 0.35f + 0.65f * (g_valve_air / 70.0f);
    if (g_ctl) g_ctl->valve_ceiling_pct = (int)(ceil * 100.0f);
    /* return-force sensor on the dash: LOSING REVS (E9c: generic wording) */
    warn_after(W_VALVE, g_valve_air < 70.0f, 1000, 3000, "LOSING REVS");
    instruct(I_VALVE, g_valve_air < 70.0f && g_valve_air >= 60.0f, 6000, 10000,
             "REVS DOWN");

    /* the game's OWN rev limiter at the ceiling (idle is 0.20 of the limit
     * in rev_frac terms), capped at stock; the fade is native */
    {
        double frac = 0.20 + 0.80 * (double)(ceil > 1.0f ? 1.0f : ceil);
        if (g_valve_air < 70.0f) rev_band_set(frac);
        else if (!g_engine_dead) rev_band_restore("VALVE RETURN"); /* E9g-e */
    }
    rf = rev_frac();
    /* valve float = the game's own rev-phased torque flutter (stress),
     * not the ignition byte (live 2026-09-28: ignition flicker flashed the
     * dash LEDs) */
    /* valve float = the game's own low-fuel throttle cut (native
     * multiplier, virtual fuel 0.15) above the ceiling, nothing below */
    if (rf > ceil - 0.03f && g_valve_air < 70.0f && ENG_RUNNING) {
        g_valve_cut = 1;
        g_vfuel_valve = 0.15f;
        if (frand01() < 0.03f) exhaust_pop();   /* unburnt fuel pops */
    } else if (g_valve_cut) {
        g_valve_cut = 0;
        g_vfuel_valve = -1.0f;
    }

    /* owner 2026-09-29: sitting on the (native) limiter blew it in 2.5 s with
     * no warning. Now: on the limiter = slow damage (~25 s), real over-rev
     * past it (downshift / ceiling falling under you) = fast damage; a
     * VALVE FLOAT warning shows well before the end. */
    if (g_valve_air < 70.0f && rf > ceil + 0.05f) g_valve_abuse += dt;
    else if (g_valve_air < 70.0f && rf > ceil - 0.03f) g_valve_abuse += dt * 0.1f;
    else g_valve_abuse -= dt * 0.25f;
    if (g_valve_abuse < 0.0f) g_valve_abuse = 0.0f;
    if (g_valve_abuse >= 0.8f) hud_show("VALVE FLOAT");
    if (g_valve_abuse >= 1.5f && ((now / 1000u) % 4u) < 2u) hud_show("SHIFT EARLIER");
    if (g_valve_abuse >= 2.5f) {
        g_valve_on = 0;
        if (g_valve_cut) { g_valve_cut = 0; }
        logf_("VALVE RETURN: revved past the ceiling too long - valve hit piston");
        arm_engine_instant();
    }
#else
    /* E4a: model-driven. The physics runs in the model; here only the
     * levers, pit top-up and the HUD/radio lines. */
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_VALVE_AIR);
    double p;

    g_valve_on = active;
    p = model_valve_pressure();
    if (g_ctl) g_ctl->valve_air_pct = active ? (int)(p * 100.0 + 0.5) : -1;

    if (!active) {
        if (g_valve_cut) { g_valve_cut = 0; g_vfuel_valve = -1.0f; }
        g_valve_topped = 0;
        g_valve_pitdone = 0;
        if (g_ctl) g_ctl->valve_ceiling_pct = -1;
        /* E9i: the band is owned by rev_band_pump (valve ceiling + save mode);
         * the E9g-e dead latch is honoured there too */
        return;
    }

#if MF_VALVE_PIT_TOPUP
    /* crew tops up the return force - costs 3-5 s on the stop (Q10) */
    if (PIT_STATUS == 3 || (PIT_STATUS >= 4 && g_valve_pitdone == 0 && g_valve_topped)) {
        if (!g_valve_topped) {
            g_valve_topped = 1;
            g_valve_pitdone = 0;
            g_model_eng.valve_m = 1.0;    /* return force refilled, leak unchanged */
            logf_("VALVE RETURN: topped up in the pits (leak remains)");
            hud_show("VALVE RETURN REFILLED");
        }
        pit_repair(3.0f + 2.0f * frand01(), &g_valve_pitdone);
    } else
#endif
    {
        g_valve_topped = 0;
        g_valve_pitdone = 0;
    }

    /* the game's OWN rev limiter: the whole native band is placed under the
     * model ceiling (E8f), never above stock; the fade is native. E9i: the
     * band write itself lives in rev_band_pump (one owner, save mode too). */
    if (g_ctl) g_ctl->valve_ceiling_pct = (int)(g_model_out.rev_ceiling * 100.0 + 0.5);

    /* valve float = the game's own low-fuel throttle cut (native
     * multiplier) above the ceiling, nothing below */
    if (g_model_out.vfuel_valve >= 0.0) {
        /* N3 addendum 16: no backfire sound without a visible flame spit -
         * the valve-float path has none. */
        g_valve_cut = 1;
        g_vfuel_valve = (float)g_model_out.vfuel_valve;
    } else if (g_valve_cut) {
        g_valve_cut = 0;
        g_vfuel_valve = -1.0f;
    }

    /* return-force sensor on the dash + engineer line (E9c: LOSING REVS is
     * generic; no pit advice while the top-up is off) */
    /* 2026-10-01 audit: one line per tier.
     *   diagnosis   LOSING REVS     return pressure low
     *   instruction SHIFT EARLIER   after the diagnosis; R3c: not once the
     *                               (lowered) limiter sits at or below the
     *                               game's shift point - no advice left
     *   critical    SHIFT NOW       LIVE: on exactly while the revs are over
     *                               the float ceiling (valves floating, the
     *                               valvetrain is being hurt) - off at once
     *   critical    ENGINE CRITICAL valvetrain nearly gone + the pit call
     * (REVS DOWN and VALVE FLOAT are retired: same meaning as the above.) */
    warn_after(W_VALVE, mfn_valve_air_low(&g_model_out), 1000, 3000, "LOSING REVS");
    /* V3: LOSING REVS -> VALVE01 (then VALVE02 30-60 s later) */
    voice_pre(R_VALVE1, g_warn_blink[W_VALVE].on, 3000, 8000,
              MFV_VALVE01, -1, 1);
    instruct(I_VALVE, mfn_valve_shift_instr(g_warn_blink[W_VALVE].on,
                                            ENG_REV_LIMIT, REV_SHIFT),
             6000, 10000, "SHIFT EARLIER");
    hud_live("SHIFT NOW", mfn_valve_float(&g_model_out, ENG_RUNNING)
                          && !wrecked_or_stopped());
    {
        int crit_ok = crit_gate(I_VALVEPIT, mfn_valve_crit(&g_model_eng),
                                g_warn_blink[W_VALVE].on, "LOSING REVS",
                                (DWORD)game_ms());
        critical(I_VALVEPIT, crit_ok, 2000, 5000, "ENGINE CRITICAL");
        voice_crit(RC_ENGINE, crit_ok, 4000, 8000,
                   MFV_CRIT01, EID_PITCALL);
    }
#endif
}

/* ---- N2 oil pressure loss (E4b: model-driven) ----------------------------
 * Pump-efficiency wear, oil pressure, bearing film and the Archard wear that
 * ends in seizure live in the model (mf_engine_model.h 4.2). This pump maps
 * the model to the dash (pressure), the rough-running cut when the bearings
 * are worn, the friction heat through the existing oil write and the
 * HUD/radio lines; power loss and the seizure BLOW UP come from MfOut via
 * the shared plumbing in model_pump. Old percentage maths: MF_LEGACY_OILP.
 * (Real: scavenge/pump trouble; M. Schumacher nursed low oil pressure to win
 * Brazil 2000.) */
static int   g_oilp_on = 0;
#if MF_LEGACY_OILP
#define OIL_P_K 160.0f
static float g_oilp = 100.0f;
static float g_oilp_floor = 30.0f;
static DWORD g_oilp_last = 0;
#endif

static void arm_oil_pressure(void)
{
#if MF_LEGACY_OILP
    g_oilp_on = 1;
    g_oilp = 100.0f;
    g_oilp_floor = 20.0f + 25.0f * frand01();
    g_oilp_last = 0;
    mfv_roll(&g_v_oilp, MFV_ALL);
    {
        char d[64];
        mfv_desc(&g_v_oilp, d, sizeof d);
        mfv_publish("engine", "OIL PRESSURE to %.0f%%: %s", g_oilp_floor, d);
    }
    logf_("OIL PRESSURE loss armed: falling to %.0f%%", g_oilp_floor);
#else
    mf_fault_arm(&g_model_eng, &g_model_in, MF_FAULT_OIL_PRESSURE,
                 MF_SEV_MEDIUM, MF_MANUAL_BUDGET_S);
    logf_("OIL PRESSURE loss: model fault armed (manual, medium, budget %.0f s)",
          MF_MANUAL_BUDGET_S);
#endif
}

static void oilp_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_OILP
    DWORD now = game_ms();
    float dt, rf, drain;

    if (g_ctl) g_ctl->oilpress_pct = g_oilp_on ? (int)g_oilp : -1;
    if (!g_oilp_on) return;
    if (!g_oilp_last) { g_oilp_last = now; return; }
    dt = (float)(now - g_oilp_last) / 1000.0f;
    g_oilp_last = now;
    if (dt > 1.0f) dt = 1.0f;

    {
        float m = mfv_mult(&g_v_oilp, now, dt);
        if (g_oilp > g_oilp_floor) g_oilp -= dt * 4.0f * m;
        dt *= m;                      /* the sink below follows the pace too */
    }
    if (g_oilp <= g_oilp_floor) {
        /* live 2026-09-28: sat at 32% - now keeps sinking when worked */
        g_oilp_floor -= dt * 0.12f * rev_frac();
        if (g_oilp_floor < 5.0f) g_oilp_floor = 5.0f;
        g_oilp = g_oilp_floor;
    }
    /* oil pressure sensor on the dash */
    warn_after(W_OILP, g_oilp < 60.0f, 1000, 3000, "OIL PRESSURE");
    instruct(I_OILP, g_oilp < 60.0f, 6000, 10000, "SHIFT EARLY");
    if (!ENG_RUNNING) return;
    rf = rev_frac();
    if (rf > 1.0f) rf = 1.0f;
    drain = OIL_P_K * rf * rf * rf * rf * (1.0f - g_oilp / 100.0f);
    ENG_LIFE = ENG_LIFE - drain * dt;
    if (ENG_LIFE < 0.0 && !g_wear_watch) {
        g_wear_watch = 1;
        logf_("OIL PRESSURE: bearings gone - vanilla failure running");
    }
#else
    /* E4b: model-driven. */
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_OIL_PRESSURE);

    g_oilp_on = active;
    if (g_ctl) g_ctl->oilpress_pct = active
        ? (int)(model_oil_pressure() * 100.0 + 0.5) : -1;
    if (!active) { oil_heat_release(OIL_HEAT_OWNER_PRESS); return; }

    /* worn bearings: friction heat through the game's own heat input (E8b).
     * R1a: same latched want as the leak pump (one episode at a time); the
     * release is the !active branch above / model_levers_release. */
    g_oil_heat_want_ks = mf_oil_heat_latch(g_oil_heat_want_ks,
                                           g_model_out.oil_heat_ks,
                                           g_model_stepped);
    if (ENG_RUNNING && g_oil_heat_want_ks > 0.0)
        oil_heat_hold(g_oil_heat_want_ks, OIL_HEAT_OWNER_PRESS);

    /* dash: OIL PRESSURE when the film breaks; SHORT SHIFT once wear runs
     * (2026-10-01 audit: was SHIFT EARLY - too close to the valve line).
     * One instruction at a time: COOL THE OIL only while SHORT SHIFT is not
     * asked. Critical: bearings nearly gone - no radio before that (nobody
     * outside the car can see a pressure fault). */
    /* L2 2026-10-05: the dash follows the GAUGE (oil_gauge_scale), not the
     * bearing film - the warning names what the driver sees. */
    /* live 2026-10-05: gauge OR active bearing wear (film < crit) - see W_OILLOW */
    /* 2026-10-06 owner live (OIL PRESSURE at 99 % oil): the dash shows the
     * SENSOR - gauge only. The bearing-film estimate no longer drives it;
     * bearing damage keeps its own critical line (MF_BEAR01). */
    warn_after(W_OILP, mfn_oil_gauge_low(&g_model_out), 1000, 3000,
               "OIL PRESSURE");
    /* V3: OIL PRESSURE dash on -> OILP01 (then OILP02/03 30-60 s later) */
    voice_pre(R_OILP1, g_warn_blink[W_OILP].on, 3000, 8000,
              MFV_OILP01, -1, 1);
    instruct(I_OILP, mfn_shift_earlier_instr(&g_model_out, g_warn_blink[W_OILP].on),
             6000, 10000, "SHORT SHIFT");
    instruct(I_OILT, mfn_oil_cool_unasked(&g_model_out, g_instr_blink[I_OILP].on),
             8000, 14000, "COOL THE OIL");
    voice_instr(I_OILT1, mfn_oil_cool_unasked(&g_model_out, g_instr_blink[I_OILP].on),
                8000, 14000, MFV_OILT01);
    {
        int crit_ok = crit_gate(C_OILP, mfn_bearing_crit(&g_model_eng),
                                g_warn_blink[W_OILP].on, "OIL PRESSURE",
                                (DWORD)game_ms());
        critical(C_OILP, crit_ok, 2000, 5000, "ENGINE CRITICAL");
        voice_crit(RC_ENGINE, crit_ok, 4000, 8000,
                   MFV_BEAR01, EID_PITCALL);
    }
    /* worn bearings rough-run the engine through the game's own cut (4.2) */
    if (g_model_eng.h_b < MF_BEARING_ROUGH) {
        if (g_vfuel_press < 0.0f || 0.6f < g_vfuel_press) g_vfuel_press = 0.6f;
    }
#endif
}

/* ---- N4 exhaust crack (E4f: model-driven) --------------------------------
 * Crack growth (Paris-law thermal cycling + vibration), the backfire pop
 * chance and the hot-gas integral that ends in a broken rear corner live in
 * the model (mf_engine_model.h 4.5). This pump maps MfOut.pop_chance_s onto
 * the game's own backfire samples and the flames spit (bank 3), fires the
 * existing suspension control-block break once and keeps the HUD/radio
 * lines. The crack = 1 power loss (x0.9) is in the model power product.
 * Old percentage maths: MF_LEGACY_EXHAUST.
 * (Real: Schumacher, Monaco 2000 - a cracked exhaust cooked the left-rear
 * suspension. Engine survives.) */
#define SUS_SHM_NAME   "F1C_SUSFAIL_CTL"
#define SUS_MAGIC_V    0x46535553u
static int   g_exh_on = 0;
static int   g_exh_corner = 0x04;
static int   g_exh_broke = 0;      /* break already sent to the suspension */
static DWORD g_exh_flame_until = 0;
static DWORD g_exh_ms = 0;         /* new-path dt tracker */
static int   g_exh_pops = 0;       /* 2026-10-01: flame spits shown this fault */
/* R3m (ADDENDUM 11): no symptom at arm - the owner saw one backfire pop the
 * instant the crack armed. First symptoms wait 5-20 s (game time) after the
 * fault becomes active; lift-off overrun pops are explicit. */
static DWORD g_exh_sym_start = 0;
static DWORD g_exh_build_until = 0;
static double g_exh_prev_load = 0.0;
static double g_exh_eta_s = -1.0;  /* smoothed ETA for the panel */
/* MF_EXH_FLAG_POPS moved to mf_notify.h (N2 2026-10-06) */
#if MF_LEGACY_EXHAUST
static float g_exh_left = 0.0f;
static DWORD g_exh_last = 0;
static DWORD g_exh_flame_next = 0;
#endif

static void exhaust_break_suspension(void)
{
    HANDLE hm = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SUS_SHM_NAME);
    volatile int *b;
    if (!hm) { logf_("EXHAUST: suspension block not found"); return; }
    b = (volatile int*)MapViewOfFile(hm, FILE_MAP_ALL_ACCESS, 0, 0, 64);
    if (b && (unsigned)b[0] == SUS_MAGIC_V) {
        b[10] = g_exh_corner;     /* wheels */
        b[5] = 1;                 /* cmd_stage2 = PART BROKEN */
        logf_("EXHAUST: heat broke the %s suspension",
              g_exh_corner == 0x04 ? "left-rear" : "right-rear");
    } else {
        logf_("EXHAUST: suspension block magic mismatch");
    }
    if (b) UnmapViewOfFile((void*)b);
    CloseHandle(hm);
}

/* N3 2026-10-06 (addendum 15): the game folder comes from the GAME EXE
 * (GetModuleFileNameA(NULL)), not "this DLL's folder three levels up" - that
 * only works for the plugin copy and was wrong in the dev tree (owner:
 * "cannot load ...\\development\\mods\\Audio\\Vehicle\\backfire_ext_2.wav").
 * Missing samples are logged once per session. */
static void game_dir(char *out, size_t cap)
{
    DWORD len;
    char *p;
    out[0] = '\0';
    len = GetModuleFileNameA(NULL, out, (DWORD)cap);
    if (!len || len >= cap) { out[0] = '\0'; return; }
    p = strrchr(out, '\\');
    if (p) *p = '\0';
}

/* 2026-10-07 (VPS usage reports: modded installs have no loose
 * Audio\Vehicle\backfire_ext_N.wav): fall back to the mod's own original pops
 * mf_backfire_1..3.wav (tools/make_backfire.py) in Audio\MechanicalFailures.
 * 1 = path holds a playable sample. */
static int backfire_fallback(char *path)
{
    char file[32];
    _snprintf(file, sizeof file, "mf_backfire_%d.wav", 1 + rand() % 3);
    file[sizeof file - 1] = '\0';
    return mf_sound_path(path, file)
        && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

/* backfire pop from the game's own samples (Audio\\Vehicle) */
#define MF_POP_GAIN 3.0f   /* +9.5 dB over the normal class (BLOW UP is 2.0) */
static void exhaust_pop(void)
{
    static const int nums[3] = { 2, 5, 6 };
    static int missing_logged = 0;
    static unsigned missing_gen = 0;
    char path[MAX_PATH];
    int i;
    if (!live_drive()) return;   /* E9g-c: no pops outside a live drive */
    game_dir(path, sizeof path);
    if (!path[0]) return;
    i = (int)strlen(path);
    _snprintf(path + i, MAX_PATH - i, "\\Audio\\Vehicle\\backfire_ext_%d.wav",
              nums[rand() % 3]);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES
        && !backfire_fallback(path)) {
        if (missing_gen != g_session_gen) {
            missing_gen = g_session_gen;
            missing_logged = 0;
        }
        if (!missing_logged) {
            missing_logged = 1;
            logf_("EXHAUST: backfire sample missing (%s)", path);
        }
        return;
    }
    /* 2026-10-06 owner live test 4: pops at the normal class (level 0.19,
     * samples -14..-19 dB RMS) were inaudible under the engine. */
    play_at_car(path, MF_POP_GAIN);
}

static void arm_exhaust_crack(void)
{
#if MF_LEGACY_EXHAUST
    g_exh_on = 1;
    g_exh_left = 90.0f + 150.0f * frand01();
    g_exh_corner = (rand() & 1) ? 0x04 : 0x08;
    g_exh_last = 0;
    g_exh_flame_next = game_ms() + 1500;
    mfv_roll(&g_v_exh, MFV_ALL);
    ev_pub(g_exh_corner == 0x04 ? "EXHAUST CRACK (left-rear)" : "EXHAUST CRACK (right-rear)", &g_v_exh);
    logf_("EXHAUST crack armed: rear suspension in %.0f s", g_exh_left);
#else
    g_exh_corner = (rand() & 1) ? 0x04 : 0x08;
    g_exh_broke = 0;
    g_exh_ms = 0;
    mf_fault_arm(&g_model_eng, &g_model_in, MF_FAULT_EXHAUST,
                 MF_SEV_MEDIUM, MF_MANUAL_BUDGET_S);
    logf_("EXHAUST crack: model fault armed (manual, medium, budget %.0f s, %s)",
          MF_MANUAL_BUDGET_S, g_exh_corner == 0x04 ? "left-rear" : "right-rear");
#endif
}

static void exhaust_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_EXHAUST
    DWORD now = game_ms();
    float dt;

    if (g_ctl) g_ctl->exhaust_s = g_exh_on ? (int)g_exh_left : -1;
    if (g_exh_flame_until && (long)(now - g_exh_flame_until) >= 0) {
        g_exh_flame_until = 0;
        ENG_BANK3 = 0.0;
    }
    if (!g_exh_on) return;
    if (!g_exh_last) { g_exh_last = now; return; }
    dt = (float)(now - g_exh_last) / 1000.0f;
    g_exh_last = now;
    if (dt > 1.0f) dt = 1.0f;
    if (ENG_STRESS < 0.06) ENG_STRESS = 0.06;
    /* no sensor for a cracked exhaust; marshals see the flames 10-30 s in */
    say_after(R_EXHAUST, 1, 10000, 30000, EID_MECHWARN);
    /* exhaust gas temperature sensor: engineer flags it 20-40 s in */
    warn_after(W_EXHTEMP, 1, 20000, 40000, "EXHAUST TEMP");
    instruct(I_EXHAUST, 1, 25000, 45000, "WATCH THE REAR");
    if (!g_exh_flame_until && (long)(now - g_exh_flame_next) >= 0 && ENG_RUNNING) {
        ENG_BANK3 = 0.5;                            /* spit of flame */
        exhaust_pop();
        g_exh_flame_until = now + 300;
        g_exh_flame_next = now + mfv_gap(&g_v_exh, now, 3000 + (DWORD)(rand() % 5000));
    }
    g_exh_left -= dt * load_factor() * mfv_mult(&g_v_exh, now, dt);
    if (g_exh_left <= 0.0f) {
        g_exh_on = 0;
        exhaust_break_suspension();
    }
#else
    /* E4f: model-driven. The physics runs in the model; here the pops, the
     * flame spit, the rear-corner break and the HUD/radio lines. */
    DWORD now = game_ms();
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_EXHAUST);
    double load = g_model_in.load;
    float dt;

    /* 2026-10-06 owner live: BLOW UP with an EXHAUST CRACK active froze the
     * game (log ends right after the blow sound load). A dead engine has no
     * backfire: stop pops, flame spits and every bank-3 write here so the
     * blow-up fire timeline owns the emitter alone. */
    if (g_engine_dead) {
        static unsigned dead_gen = 0;
        if (dead_gen != g_session_gen) {
            dead_gen = g_session_gen;
            if (active) logf_("EXHAUST: stopped (engine dead)");
        }
        g_exh_flame_until = 0;
        return;
    }
    if (g_exh_flame_until && (long)(now - g_exh_flame_until) >= 0) {
        g_exh_flame_until = 0;
        ENG_BANK3 = 0.0;
    }
    g_exh_on = active;
    if (!active) {
        g_exh_ms = 0;
        g_exh_pops = 0;
        g_exh_sym_start = 0;
        g_exh_build_until = 0;
        g_exh_prev_load = load;
        g_exh_eta_s = -1.0;
        if (g_ctl) { g_ctl->exhaust_s = -1; g_ctl->rear_heat_pct = -1; }
        return;
    }
    if (!g_exh_ms) g_exh_ms = now;
    dt = (float)(now - g_exh_ms) / 1000.0f;
    g_exh_ms = now;
    if (dt > 1.0f) dt = 1.0f;
    if (!g_exh_sym_start) {         /* R3m: no symptom at arm */
        g_exh_sym_start = now;
        g_exh_build_until = now + (DWORD)rand_range(5000, 20000);
        logf_("EXHAUST: first symptoms in %.1f s",
              (double)(g_exh_build_until - now) / 1000.0);
    }

    if (g_ctl) {
        int s = -1, rh = -1;
        double amb_k = (double)AMBIENT_C + 273.15;
        double rate = MF_REAR_HEAT_K * g_model_eng.crack_a
                      * (g_model_eng.t_exh_k - amb_k) / MF_EXH_RISE_K;
        rh = (int)(100.0 * g_model_eng.rear_heat / MF_REAR_HEAT_MAX + 0.5);
        if (rh < 0) rh = 0;
        if (rh > 100) rh = 100;
        if (g_model_eng.rear_broken) {
            s = 0;
            g_exh_eta_s = 0.0;
        } else if (rate > 1e-6) {
            double rem = (MF_REAR_HEAT_MAX - g_model_eng.rear_heat) / rate;
            double inst = rem > 0.0 ? rem : 0.0;
            /* R3m add: the raw ETA followed the CURRENT exhaust gas temp and
             * jumped with throttle (owner live 492 -> 542 s); publish the
             * monotonic rear heat % plus a tau 10 s smoothed ETA. */
            if (g_exh_eta_s < 0.0) g_exh_eta_s = inst;
            else {
                double a = dt > 0.0 ? (double)dt / 10.0 : 0.0;
                if (a > 1.0) a = 1.0;
                g_exh_eta_s += (inst - g_exh_eta_s) * a;
            }
            s = (int)(g_exh_eta_s + 0.5);
        }
        g_ctl->exhaust_s = s;
        g_ctl->rear_heat_pct = rh;
    }

    /* 2026-10-01 audit: every line follows a SYMPTOM that really happened
     * (was: fixed timers from the onset - owner live "instant pop + PIT ON
     * THIS LAP"). Exhaust gas temperature: the engineer flags it 3-8 s after
     * the first pop; WATCH THE REAR after that diagnosis; the marshals' flag
     * (the fault's one pre-critical radio) only once flames were shown
     * several times. */
    /* L2 2026-10-05: renamed EXHAUST TEMP -> EXHAUST - the model's exhaust gas
     * temperature has no crack-dependent rise (harness: identical to a healthy
     * run at the same load), so a temperature claim would be false. */
    /* N3 2026-10-06 (H15): "EXHAUST" -> "EXHAUST CRACKED"; the instruction is
     * SHORT SHIFT (lower revs = slower crack + less rear heat), replacing
     * WATCH THE REAR which named no action the model rewards. MFV_EXH02
     * ("keep pushing") is not armed any more (mfv_instr_of(EXH01) = -1). */
    warn_after(W_EXHTEMP, mfn_exh_diag(g_exh_pops), 3000, 8000,
               "EXHAUST CRACKED");
    instruct(I_EXHAUST, g_warn_blink[W_EXHTEMP].on, 20000, 40000,
             "SHORT SHIFT");
    voice_pre(R_EXHAUST, mfn_exh_flag(g_exh_pops), 5000, 20000,
              MFV_EXH01, EID_MECHWARN, 0);

    /* R3m: after the 5-20 s build, backfire pops from the model's chance
     * (4.5) AND on lift-off overrun; each is a VISIBLE flame spit sized by
     * the crack (0.6..1.0 for 300-600 ms; the old fixed 0.5/300 ms was
     * barely visible). Nothing while paused (the pump returned above). */
    if ((long)(now - g_exh_build_until) >= 0) {
        int fire = 0;
        if (ENG_RUNNING && !g_exh_flame_until) {
            if (g_exh_prev_load > 0.40 && load < 0.12) fire = 1;
            else if (g_model_out.pop_chance_s > 0.0
                     && frand01() < (float)(g_model_out.pop_chance_s
                                            * (double)dt))
                fire = 1;
        }
        if (fire) {
            double cf = g_model_eng.crack_a / MF_CRACK_A_BIG;
            if (cf > 1.0) cf = 1.0;
            if (cf < 0.25) cf = 0.25;
            ENG_BANK3 = 0.6 + 0.4 * cf;
            exhaust_pop();
            g_exh_flame_until = now + (DWORD)(300.0 + 300.0 * cf);
            g_exh_pops++;
        }
    }
    g_exh_prev_load = load;

    /* hot gas onto the rear: the model latches the break, send it once */
    if (g_model_out.break_rear_suspension && !g_exh_broke) {
        g_exh_broke = 1;
        exhaust_break_suspension();
    }
#endif
}

/* ---- N5 piston ring blow-by (E4e: model-driven) ---------------------------
 * Ring-seal wear (hot-oil feedback), blow-by flow, crankcase oil loss and
 * the blue smoke density live in the model (mf_engine_model.h 4.4). This
 * pump maps MfOut.smoke_blue onto the proven blue-smoke emitter and keeps
 * the HUD/radio lines; the power product and the no-bang engine stop come
 * from MfOut via the shared plumbing in model_pump. Old percentage maths:
 * MF_LEGACY_BLOWBY. */
static int   g_bb_on = 0;
static DWORD g_bb_smoke_t = 0;     /* R3k: smoke drive timebase (model branch) */
static DWORD g_bb_vis_since = 0;   /* N3 R1: visible blow-by smoke since (ms) */
#if MF_LEGACY_BLOWBY
static float g_bb = 0.0f, g_bb_limit = 200.0f;
static int   g_bb_warned = 0;
static DWORD g_bb_last = 0;
#endif

static void arm_blowby(void)
{
#if MF_LEGACY_BLOWBY
    g_bb_on = 1;
    g_bb = 0.0f;
    g_bb_limit = 150.0f + 150.0f * frand01();
    g_bb_warned = 0;
    g_bb_last = 0;
    mfv_roll(&g_v_bb, MFV_ALL);
    ev_pub("BLOW-BY", &g_v_bb);
    logf_("BLOW-BY armed: piston fails at damage %.0f", g_bb_limit);
#else
    mf_fault_arm(&g_model_eng, &g_model_in, MF_FAULT_BLOWBY,
                 MF_SEV_MEDIUM, MF_MANUAL_BUDGET_S);
    logf_("BLOW-BY: model fault armed (manual, medium, budget %.0f s)",
          MF_MANUAL_BUDGET_S);
#endif
}

static void blowby_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_BLOWBY
    DWORD now = game_ms();
    float dt, rf, k;

    if (g_ctl) g_ctl->blowby_pct = g_bb_on ? (int)(100.0f * g_bb / g_bb_limit) : -1;
    if (!g_bb_on) return;
    if (!g_bb_last) { g_bb_last = now; return; }
    dt = (float)(now - g_bb_last) / 1000.0f;
    g_bb_last = now;
    if (dt > 1.0f) dt = 1.0f;
    rf = rev_frac();
    if (rf > 1.0f) rf = 1.0f;
    if (ENG_RUNNING) g_bb += dt * (0.3f + 1.7f * rf * rf * rf) * mfv_mult(&g_v_bb, now, dt);
    k = g_bb / g_bb_limit;
    if (ENG_STRESS < 0.15 * k) ENG_STRESS = 0.15 * k;
    if (k > 0.05f && ENG_RUNNING)
        emit_bluesmoke_burst(1 + (int)(k * 2.0f), 0.3f + 0.5f * k);
    if (k > 0.3f) {
        g_bb_warned = 1;
    }
    /* no sensor for blow-by: the driver feels it; the engineer spots the
     * power loss on telemetry 45-90 s later; marshals see smoke 8-25 s */
    warn_after(W_BLOWBY, k > 0.3f, 45000, 90000, "ENGINE PROBLEM");
    instruct(I_BLOWBY, k > 0.3f, 50000, 95000, "EASE OFF");
    say_after(R_BBSMOKE, k > 0.3f, 8000, 25000, EID_MECHWARN);
    if (0) {
    }
    if (g_bb >= g_bb_limit) {
        g_bb_on = 0;
        logf_("BLOW-BY: piston failed");
        arm_engine_instant();
    }
#else
    /* E4e: model-driven. The physics runs in the model; here the smoke and
     * the HUD/radio lines. */
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_BLOWBY);
    double k = mfn_blowby_k(&g_model_eng);   /* blow-by severity, 1 - seal */

    g_bb_on = active;
    if (g_ctl) g_ctl->blowby_pct = active ? (int)(k * 100.0 + 0.5) : -1;
    if (!active) { g_bb_smoke_t = 0; g_bb_vis_since = 0; return; }

    /* R3k (ADDENDUM 11): blue smoke follows load and rpm with a lift/reapply
     * puff (mf_smoke_* in mf_engine_model.h), not a constant trail. */
    if (g_model_out.smoke_blue > 0.02 && ENG_RUNNING) {
        DWORD now2 = game_ms();
        double dt2;
        if (!g_bb_smoke_t) g_bb_smoke_t = now2;
        dt2 = (double)(now2 - g_bb_smoke_t) / 1000.0;
        g_bb_smoke_t = now2;
        if (dt2 > 1.0) dt2 = 1.0;
        smoke_emit_drive(
            smoke_drive_step(MF_FAULT_BLOWBY, g_model_out.smoke_blue, dt2, 0.0),
            dt2);
    }

    /* no sensor for blow-by: the driver feels the power loss (model); the
     * engineer spots it on telemetry 45-90 s later; marshals see smoke 8-25 s */
    /* N3 2026-10-06 (R1 blow-by): the diagnosis additionally needs the VISIBLE
     * blow-by smoke for >= 5 s game time (R2 visible effect); k > 0.3 alone is
     * internal-only. */
    {
        DWORD now2 = game_ms();
        if (mfn_blowby_visible(&g_model_out)) {
            if (!g_bb_vis_since) g_bb_vis_since = now2;
        } else {
            g_bb_vis_since = 0;
        }
    }
    warn_after(W_BLOWBY, mfn_blowby_warn(&g_model_eng)
               && mfn_for_ms((unsigned)game_ms(), g_bb_vis_since, 5000),
               45000, 90000, "ENGINE PROBLEM");
    /* 2026-10-01 audit: the instruction after its diagnosis; the marshals'
     * flag (one pre-critical radio) only after the diagnosis and while blue
     * smoke really shows - it was 8-25 s in, long before the dash said
     * anything, on a fault that can run a whole stint. Critical: the rings
     * near the stop seal (MF_RING_STOP). */
    instruct(I_BLOWBY, mfn_blowby_instr(&g_model_eng, g_warn_blink[W_BLOWBY].on),
             5000, 15000, "EASE OFF");
    voice_pre(R_BBSMOKE, mfn_blowby_pre(&g_model_eng, &g_model_out,
                                        g_warn_blink[W_BLOWBY].on, ENG_RUNNING),
              5000, 20000, MFV_BLOWBY01, EID_MECHWARN, 0);
    {
        int crit_ok = crit_gate(C_BLOWBY, mfn_ring_crit(&g_model_eng),
                                g_warn_blink[W_BLOWBY].on, "ENGINE PROBLEM",
                                (DWORD)game_ms());
        critical(C_BLOWBY, crit_ok, 2000, 5000, "ENGINE CRITICAL");
        voice_crit(RC_ENGINE, crit_ok, 4000, 8000,
                   MFV_CRIT01, EID_PITCALL);
    }
#endif
}

/* ---- N6 over-rev damage (E4g: model-driven, always on) -------------------
 * The layer itself is the model (mf_engine_model.h 4.7): Miner rod fatigue
 * above the endurance limit, valvetrain damage above 1.02, the missed-
 * downshift blow-up above 1.15 for 0.3 s, downshift shocks and engine
 * braking. It runs whenever the car is on track with the engine running
 * (practice and pit-out included - the on_track gate is E2's input, E6 owns
 * widening it). This pump keeps only the dash readout, the OVER-REV notice
 * and the session event count. The old x1.04 life drain and the x1.10 valve
 * arm are gone (the model never arms the valve return fault - owner report #5).
 * Old maths: MF_LEGACY_OVERREV. */
static int   g_or_in = 0;
/* R3b (ADDENDUM 11, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): a band LOWERING
 * under the running rpm (valve ceiling, save mode) is not a driver over-rev.
 * Grace until rpm first falls below the new limit, or 2 s - whichever comes
 * first. Repeated small lowerings do not extend the window. */
static int    g_or_grace_on = 0;
static DWORD  g_or_grace_until = 0;
static double g_or_prev_limit = 0.0;
/* V3: OVERREV01 max once per 120 s; OVERREV02 on a measurable health drop */
static uint32_t g_or_rate1 = 0, g_or_rate2 = 0;
static double   g_or_hv_prev = 1.0, g_or_hr_prev = 1.0;
#if MF_LEGACY_OVERREV
static DWORD g_or_last = 0;
static float g_or_hard = 0.0f;
#endif

static void overrev_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_OVERREV
    DWORD now = game_ms();
    float dt, ratio;

    if (!g_or_last) { g_or_last = now; return; }
    dt = (float)(now - g_or_last) / 1000.0f;
    g_or_last = now;
    if (dt > 1.0f) dt = 1.0f;
    if (g_ctl) g_ctl->rev_pct = (int)(rev_frac() * 100.0f);
    if (!g_ctl || !g_ctl->overrev_enable || RACE_PHASE <= 4) return;
    if (ENG_REV_LIMIT < 1.0 || !ENG_RUNNING) return;
    ratio = (float)(ENG_RPM_LIVE / ENG_REV_LIMIT);
    if (ratio > 1.04f) {
        ENG_LIFE = ENG_LIFE - dt * 4000.0f * (ratio - 1.04f);
        if (!g_or_in) {
            g_or_in = 1;
            g_ctl->overrev_events++;
            logf_("OVER-REV: %.0f%% of the limit", ratio * 100.0f);
            hud_show("OVER-REV");
        }
        if (ratio > 1.10f) g_or_hard += dt;
        if (g_or_hard >= 0.2f && !g_valve_on) {
            g_or_hard = 0.0f;
            logf_("OVER-REV: big over-rev - valve damage");
            arm_valve_leak(50.0f);
        }
    } else {
        g_or_in = 0;
        g_or_hard = 0.0f;
    }
#else
    /* E4g: model owns the layer; here only the readout and the notice.
     * R3b: grace after a mod band lowering (see the statics above). */
    float ratio;
    double lim = ENG_REV_LIMIT;
    DWORD now = game_ms();

    if (g_ctl) g_ctl->rev_pct = (int)(rev_frac() * 100.0f);
    if (g_or_prev_limit > 1.0 && lim > 1.0
        && lim < g_or_prev_limit * (1.0 - 1e-4)) {
        if (!g_or_grace_on) {
            g_or_grace_on = 1;
            g_or_grace_until = now + 2000;
        }
        g_or_in = 0;                       /* dropped under the running rpm */
    }
    if (lim > 1.0) g_or_prev_limit = lim;
    if (g_or_grace_on) {
        if (lim < 1.0 || ENG_RPM_LIVE < lim
            || (long)(now - g_or_grace_until) >= 0)
            g_or_grace_on = 0;             /* rpm fell under it, or 2 s done */
        else
            return;                        /* inside the grace: no notice */
    }
    if (lim < 1.0 || !ENG_RUNNING) { g_or_in = 0; return; }
    /* 2026-10-06 vanilla+: Failure Rate 'none' has no over-rev consequence
     * (model layer off), so no notice or radio either */
    if (PLR_FAILRATE == 0) { g_or_in = 0; return; }
    ratio = (float)(ENG_RPM_LIVE / lim);
    if (mfn_overrev(ENG_RPM_LIVE, lim, ENG_RUNNING, PLR_FAILRATE != 0)) {
        if (!g_or_in) {
            g_or_in = 1;
            if (g_ctl) g_ctl->overrev_events++;
            logf_("OVER-REV: %.0f%% of the limit (model layer)", ratio * 100.0f);
            hud_show("OVER-REV");
            /* V3: OVERREV01 rising edge, max once per 120 s */
            if (mfm_rate_ok(&g_or_rate1, now, 120000))
                voice_say(MFV_OVERREV01, MFR_P_INFO);
        }
        /* V3: OVERREV02 - the model lost valvetrain/conrod health while over
         * the edge (measurably damaged) */
        if (mfn_overrev_hurt(g_or_hv_prev, g_model_eng.h_vt,
                             g_or_hr_prev, g_model_eng.h_rod)) {
            if (mfm_rate_ok(&g_or_rate2, now, 120000))
                voice_say(MFV_OVERREV02, MFR_P_INFO);
        }
    } else {
        g_or_in = 0;
    }
    g_or_hv_prev = g_model_eng.h_vt;
    g_or_hr_prev = g_model_eng.h_rod;
#endif
}

/* ---- N7 cooling failure (E4c: model-driven) ------------------------------
 * The vanilla thermal model (EngineStressSimulation_0043ef20) cools the
 * WATER (this+7564, 0x7C91B4, Kelvin) through two radiator coefficients:
 * base this+7572 (0x7C91BC) and airflow-scaled this+7580 (0x7C91C4, from the
 * radiator setup); oil heats water via this+7548. Above the oil limit and
 * above 100 C water the game itself cuts power (this+7540 / this+7588).
 * A cooling failure = the radiator gets worse. Radiator open fraction, leak
 * area, coolant level, steam and head-gasket creep live in the model
 * (mf_engine_model.h 4.3); this pump scales the radiator SOURCE coefficients
 * by MfOut.rad_scale (the game then derives the weakened airflow cooling
 * itself every tick), reports the model steam, keeps the HUD/radio lines and
 * clears debris in the pit box (+4-6 s). The head-gasket stop is the model's
 * engine_stop (handled once in model_terminal_pump). Old percentage maths:
 * MF_LEGACY_COOLING. If the game rewrites the source coefficients we log it
 * (coeff_held = 0). */
#define WATER_K        (*(volatile double*)0x007C91B4u)
#define RAD_BASE       (*(volatile double*)0x007C91BCu)
#define RAD_AIR        (*(volatile double*)0x007C91C4u)
/* AMBIENT_C 0x8AFEF0 is defined at the top (E2 reads it too) */
/* this+7580 is REWRITTEN every physics call by SpringDamperCompute_0043bbd0:
 *   this+7580 = setting * this[774] + this[773]   (doubles, this+6192/6184)
 * (worker brief result Q4, 2026-09-28; source-verified). So we scale the two
 * load-time SOURCE coefficients instead; the game then derives the weakened
 * airflow cooling itself every tick. */
#define RAD_AIR_K0     (*(volatile double*)0x007C8C50u)   /* this[773] */
#define RAD_AIR_K1     (*(volatile double*)0x007C8C58u)   /* this[774] */
#define EID_OVERHEAT_W 77
#define COOL_HEAT_MAX  1.5f     /* K/s added to water at no cooling, full revs */
#define EID_PIT01      49       /* PWS_PIT01 "pit this lap" */
static DWORD g_pitadv_until = 0;   /* HUD "PIT - DEBRIS" shown until */
static int    g_cool_on = 0, g_cool_kind = 0;
static int    g_cool_cleared = 0;  /* RADIATOR CLEARED fired this fault */
static int    g_cool_diag_seen = 0;/* N3 items 3: diagnosis of this onset shown/said */
static int    g_cool_steam_on = 0; /* we wrote the steam plume */
static double g_cool_want_ks = 0.0;/* R3g: latched low-coolant oil heat */
static double g_rad_air0 = 0.0;    /* RAD_AIR snapshot, log only (E9g-b) */
static MfLever g_rad_base, g_rad_k0, g_rad_k1;   /* E9g-b: ownership-checked */
#if MF_LEGACY_COOLING
static int    g_cool_said = 0;
static float  g_cool_eff = 1.0f, g_cool_rate = 0.004f, g_cool_hot_s = 0.0f;
static float  g_cool_clear_t = 0.0f;
static float  g_cool_clear_p = 0.06f;   /* debris: blow-clear chance per 10 s */
static DWORD  g_cool_last = 0, g_cool_plume_until = 0;
#endif

static void cooling_restore(void)
{
    double base0, air0;
    int wrote = 0;

    if (!g_rad_base.held && !g_rad_k0.held && !g_rad_k1.held) return;
    base0 = g_rad_base.saved;
    air0 = g_rad_air0;
    wrote |= lever_restore(&g_rad_base, &RAD_BASE,   "RAD_BASE");
    wrote |= lever_restore(&g_rad_k0,   &RAD_AIR_K0, "RAD_AIR_K0");
    wrote |= lever_restore(&g_rad_k1,   &RAD_AIR_K1, "RAD_AIR_K1");
    if (wrote)
        logf_("COOLING: radiator coefficients restored (%.4g / %.4g)",
              base0, air0);
}

static void arm_cooling(int kind)
{
#if MF_LEGACY_COOLING
    if (kind != 1 && kind != 2) kind = (rand() & 1) ? 1 : 2;
    if (!g_rad_base.held) {
        lever_write(&g_rad_base, &RAD_BASE,   RAD_BASE);
        lever_write(&g_rad_k0,   &RAD_AIR_K0, RAD_AIR_K0);
        lever_write(&g_rad_k1,   &RAD_AIR_K1, RAD_AIR_K1);
        g_rad_air0 = RAD_AIR;
    }
    g_cool_on = 1;
    g_cool_kind = kind;
    g_cool_said = 0;
    g_cool_hot_s = 0.0f;
    g_cool_clear_t = 0.0f;
    g_cool_last = 0;
    if (kind == 1) {
        float r = frand01();
        g_cool_eff = 0.10f + 0.15f * frand01();   /* live: 33% did not heat */
        /* variance: stuck (never blows clear) / normal / loose */
        g_cool_clear_p = r < 0.3f ? 0.0f : (r < 0.75f ? 0.06f : 0.15f);
        mfv_publish("engine", "RADIATOR DEBRIS: radiator %.0f%%, %s", g_cool_eff * 100.0f,
                    g_cool_clear_p == 0.0f ? "stuck (pit only)"
                    : g_cool_clear_p < 0.1f ? "may blow clear" : "loose, likely blows clear");
    } else {
        g_cool_eff = 1.0f;
        g_cool_rate = 0.9f / (150.0f + 150.0f * frand01());   /* to 10% */
        mfv_roll(&g_v_cool, MFV_ALL);
        ev_pub("WATER LEAK", &g_v_cool);
    }
    logf_("COOLING armed: %s, radiator %.0f%% (base %.4g, air %.4g = setting x "
          "%.4g + %.4g), water %.1f C",
          kind == 1 ? "debris in sidepod" : "water leak", g_cool_eff * 100.0f,
          g_rad_base.saved, g_rad_air0, g_rad_k1.saved, g_rad_k0.saved,
          WATER_K - 273.16);
#else
    /* E4c: manual trigger = forced medium model onset (handoff E4) */
    int fault;
    if (kind != 1 && kind != 2) kind = (rand() & 1) ? 1 : 2;
    fault = (kind == 1) ? MF_FAULT_RAD_DEBRIS : MF_FAULT_WATER_LEAK;
    g_cool_cleared = 0;
    g_cool_steam_on = 0;
    mf_fault_arm(&g_model_eng, &g_model_in, fault, MF_SEV_MEDIUM,
                 MF_MANUAL_BUDGET_S);
    logf_("COOLING: model fault armed (manual %s, medium, budget %.0f s)",
          kind == 1 ? "debris" : "water leak", MF_MANUAL_BUDGET_S);
#endif
}

/* E9g item 2: the car-relative water baseline the model derived (K) for the
 * cooling radio thresholds; while the model has none yet the comparison runs
 * against the live water (never true), the same rule the model itself uses. */
static double model_water_base(void)
{
    return g_model_eng.water_base_k > 0.0 ? g_model_eng.water_base_k : WATER_K;
}

/* 2026-10-06 owner: WATER TEMP / COOL THE ENGINE with no cooling fault (bad
 * setup, hot day). Same HUD ids as the fault path (only one runs per pass);
 * radio WATER01 on the diagnosis, WATER02 'lift and coast' on the
 * instruction, each once per episode and at most every 120 s. */
static void water_heat_pump(void)
{
    static int said1 = 0, said2 = 0;
    static uint32_t rate1 = 0, rate2 = 0;
    int ok = g_model_live && !wrecked_or_stopped();
    int w = ok && mfn_water_temp(&g_model_out);
    int c = ok && mfn_cool_engine(&g_model_out);
    uint32_t now = (uint32_t)game_ms();

    warn_after(W_WATER, w, 1000, 3000, "WATER TEMP");
    instruct(I_WATER, c && g_warn_blink[W_WATER].on, 8000, 14000,
             "COOL THE ENGINE");
    if (w && g_warn_blink[W_WATER].on && !said1
        && mfm_rate_ok(&rate1, now, 120000)) {
        said1 = 1;
        logf_("WATER: %.1f C with no cooling fault - WATER TEMP",
              g_model_in.water_k - 273.15);
        voice_say(MFV_WATER01, MFR_P_INFO);
    }
    if (!w) said1 = 0;
    if (c && g_warn_blink[W_WATER].on && !said2
        && mfm_rate_ok(&rate2, now, 120000)) {
        said2 = 1;
        voice_say(MFV_WATER02, MFR_P_INFO);
    }
    if (!c) said2 = 0;
}

static void cooling_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_COOLING
    DWORD now = game_ms();
    float dt, wc;

    if (g_ctl) {
        g_ctl->water_temp_c = (float)(WATER_K - 273.16);
        g_ctl->cooling_pct = g_cool_on ? (int)(g_cool_eff * 100.0f) : -1;
        g_ctl->cooling_kind = g_cool_on ? g_cool_kind : 0;
    }
    if (g_cool_plume_until && (long)(now - g_cool_plume_until) >= 0) {
        g_cool_plume_until = 0;
        ENG_BANK1 = 0.0;
    }
    if (!g_cool_on) return;
    if (!g_cool_last) { g_cool_last = now; return; }
    dt = (float)(now - g_cool_last) / 1000.0f;
    g_cool_last = now;
    if (dt > 1.0f) dt = 1.0f;

    /* did anything rewrite our source coefficients since last tick? */
    if (g_rad_k0.held && RAD_AIR_K0 != g_rad_k0.last) {
        if (g_ctl && g_ctl->coeff_held) logf_("COOLING: radiator source coefficient rewritten");
        if (g_ctl) g_ctl->coeff_held = 0;
    }

    if (g_cool_kind == 1) {
        if (PIT_STATUS >= 3) {           /* in the box or leaving it */
            static int rad_pitdone = 0;
            if (PIT_STATUS == 3 && !pit_repair(4.0f + 2.0f * frand01(), &rad_pitdone))
                return;                  /* wait until the stop is running */
            rad_pitdone = 0;
            logf_("COOLING: crew cleared the sidepod");
            g_pitadv_until = 0;
            hud_show("RADIATOR CLEARED");
            voice_say(MFV_RAD03, MFR_P_INFO);
            g_cool_on = 0;
            cooling_restore();
            return;
        }
        g_cool_clear_t += dt;
        if (g_cool_clear_t >= 10.0f) {
            g_cool_clear_t = 0.0f;
            if (VEH_SPEED > 50.0f && frand01() < g_cool_clear_p) {
                voice_instr_clear_all();
            logf_("COOLING: debris blew clear");
                hud_show("RADIATOR CLEARED");
            voice_say(MFV_RAD03, MFR_P_INFO);
                g_pitadv_until = 0;
                g_cool_on = 0;
                cooling_restore();
                return;
            }
        }
    } else {
        g_cool_eff -= dt * g_cool_rate * load_factor() * mfv_mult(&g_v_cool, now, dt);
        if (g_cool_eff < 0.10f) g_cool_eff = 0.10f;
    }
    /* Live 2026-09-28: radiator at 33% still cooled the water (88 -> 82 C) -
     * the stock radiator has a big margin. So heat also goes straight into
     * the vanilla water temperature (same integrator pattern as the oil,
     * whose write is live-proven): up to COOL_HEAT_MAX K/s x lost cooling x
     * revs. */
    if (ENG_RUNNING) {
        float rv = rev_frac();
        if (rv > 1.0f) rv = 1.0f;
        WATER_K = WATER_K + (double)(COOL_HEAT_MAX * (1.0f - g_cool_eff)
                                     * (0.3f + 0.7f * rv) * dt);
    }
    lever_write(&g_rad_base, &RAD_BASE,   g_rad_base.saved * g_cool_eff);
    lever_write(&g_rad_k0,   &RAD_AIR_K0, g_rad_k0.saved   * g_cool_eff);
    lever_write(&g_rad_k1,   &RAD_AIR_K1, g_rad_k1.saved   * g_cool_eff);

    wc = (float)(WATER_K - 273.16);
    /* water temp sensor on the dash */
    warn_after(W_WATER, wc > 105.0f, 1000, 3000, "WATER TEMP");
    instruct(I_WATER, wc > 105.0f && !(g_cool_kind == 2 && wc > 118.0f),
             8000, 14000, "COOL THE ENGINE");
    /* water leak: steam seen by marshals; engineer calls it in to retire.
     * E9g item 2: car-relative - baseline + MF_WATER_WARN_DK / COOL_DK. */
    say_after(R_STEAM, g_cool_kind == 2
              && WATER_K > model_water_base() + MF_WATER_WARN_DK,
              5000, 20000, EID_MECHWARN);
    say_after(R_WATERPIT, g_cool_kind == 2
              && WATER_K > model_water_base() + MF_WATER_COOL_DK,
              5000, 10000, EID_PIT01);
    instruct(I_WATERPIT, g_cool_kind == 2
             && WATER_K > model_water_base() + MF_WATER_COOL_DK,
             5000, 10000, "PIT AND RETIRE");
    /* the engineer diagnoses debris from the temperature trace, later */
    warn_after(W_RADBLOCK, g_cool_kind == 1 && wc > 105.0f, 8000, 15000,
               "RADIATOR BLOCKED");
    /* owner 2026-09-28: once the block is diagnosed, the engineer calls the
     * car in - radio PWS_PIT01 "pit this lap" (eid 49, unblocked, cooldown
     * 1.0; identity from full_event_dump, first live play = isolation test)
     * + HUD PIT TO CLEAR DEBRIS for 8 s. Debris can still blow clear. */
    if (g_cool_kind == 1 && !g_w_done[R_PITADV] && g_w_due[W_RADBLOCK]
        && (long)(game_ms() - g_w_due[W_RADBLOCK]) >= 3000) {
        g_w_done[R_PITADV] = 1;
        g_pitadv_until = game_ms() + 8000;
        commentary_say(EID_PIT01);
        logf_("COOLING: engineer calls the car in (eid %d)", EID_PIT01);
    }
    if (g_pitadv_until && (long)(game_ms() - g_pitadv_until) < 0)
        hud_show("PIT - DEBRIS");
    /* engineer sees the water temperature on telemetry 5-12 s later.
     * E9g-b item 2: car-relative - baseline + MF_WATER_COOL_DK (was 110 C),
     * silent while the model has no baseline. */
    if (wc > 110.0f) g_cool_said = 1;
    say_after(R_WATER77, WATER_K > model_water_base() + MF_WATER_COOL_DK,
              5000, 12000, EID_OVERHEAT_W);
    /* live 2026-09-28: debris took water to 141 C / oil 133 C with the engine
     * still alive - both kinds now cook the engine past 125 C water */
    {
        if (wc > 110.0f && ENG_BANK1 == 0.0) {
            ENG_BANK1 = -0.30;                  /* steam under load */
            play_fx("mf_steam_hiss.wav");
        }
        if (wc > 125.0f) g_cool_hot_s += dt; else g_cool_hot_s = 0.0f;
        if (g_cool_hot_s >= 10.0f && ENG_RUNNING) {
            engine_seize("cooling legacy: head gasket");   /* E9g item 3 */
            retire_pending_arm();
            ENG_BANK1 = EMIT_ON;
            g_cool_plume_until = now + 8000;
            g_cool_on = 0;
            cooling_restore();
            logf_("COOLING: head gasket at %.1f C water - engine off", wc);
            play_fx("mf_steam_hiss.wav");
            engine_dead_msg();
            hud_show("ENGINE FAILURE");
        }
    }
#else
    /* E4c: model-driven. The physics runs in the model; here only the
     * radiator-source scaling, the steam plume, pit clear and HUD/radio. */
    int active;
    float wc;

    if (g_ctl) {
        float wc_pub = (float)(WATER_K - 273.16);
        /* R3g (ADDENDUM 11): below ~20 % coolant the sensor sits in steam/air
         * - the needle spikes instead of tracking the liquid (chosen sensor
         * behaviour: deterministic spike up to +15 K at ~1 Hz, no RNG). */
        if (g_model_live && g_model_eng.fault == MF_FAULT_WATER_LEAK
            && g_model_eng.v_w_l < MF_WATER_SENSOR_FRAC * MF_WATER_START_L)
            wc_pub += 15.0f * (float)fabs(sin((double)g_ctl->heartbeat * 0.31));
        g_ctl->water_temp_c = wc_pub;
        g_ctl->cooling_pct = -1;
        g_ctl->cooling_kind = 0;
    }
    active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_RAD_DEBRIS)
             || mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_WATER_LEAK);
    if (!active) {
        if (g_cool_on) {
            g_cool_on = 0;
            g_cool_kind = 0;
            g_cool_cleared = 0;
            g_cool_diag_seen = 0;      /* N3 item 3: next onset starts clean */
            if (g_cool_steam_on) { bank1_set(0.0, "cooling"); g_cool_steam_on = 0; }
            cooling_restore();
        }
        oil_heat_release(OIL_HEAT_OWNER_WATER);   /* R3g: heat lever back */
        g_pitadv_until = 0;
        water_heat_pump();                        /* 2026-10-06: no-fault overheat */
        return;
    }
    g_cool_on = 1;
    g_cool_kind = (g_model_eng.fault == MF_FAULT_RAD_DEBRIS) ? 1 : 2;
    /* N3 2026-10-06 (item 3): a clear/repair line is allowed only when this
     * onset's diagnosis was shown (WATER TEMP / RADIATOR BLOCKED blink) or
     * said (the RAD01/WATER01 pre line below). */
    if (g_warn_blink[W_WATER].on || g_warn_blink[W_RADBLOCK].on)
        g_cool_diag_seen = 1;
    if (g_ctl) {
        /* E8g item 4: the effective radiator factor (debris = open fraction,
         * leak = coolant fraction) */
        g_ctl->cooling_pct = (int)(g_model_out.rad_air_scale * 100.0 + 0.5);
        g_ctl->cooling_kind = g_cool_kind;
    }

    /* did anything rewrite our source coefficients since last tick? */
    if (g_rad_k0.held && RAD_AIR_K0 != g_rad_k0.last) {
        if (g_ctl && g_ctl->coeff_held) logf_("COOLING: radiator source coefficient rewritten");
        if (g_ctl) g_ctl->coeff_held = 0;
    }

    /* save the stock coefficients once, then scale from them every tick */
    if (!g_rad_base.held) {
        lever_write(&g_rad_base, &RAD_BASE,   RAD_BASE);
        lever_write(&g_rad_k0,   &RAD_AIR_K0, RAD_AIR_K0);
        lever_write(&g_rad_k1,   &RAD_AIR_K1, RAD_AIR_K1);
        g_rad_air0 = RAD_AIR;
    }

    if (g_model_eng.f_rad < 0.999) g_cool_cleared = 0;

    if (g_cool_kind == 1) {
        if (PIT_STATUS >= 3) {           /* in the box or leaving it */
            static int rad_pitdone = 0;
            if (PIT_STATUS == 3 && !pit_repair(4.0f + 2.0f * frand01(), &rad_pitdone))
                return;                  /* wait until the stop is running */
            rad_pitdone = 0;
            g_model_eng.f_rad = 1.0;     /* crew cleared the sidepod */
            g_pitadv_until = 0;
            if (!g_cool_cleared) {
                g_cool_cleared = 1;
                voice_instr_clear_all();
                logf_("COOLING: crew cleared the sidepod%s",
                      g_cool_diag_seen ? "" : " (no diagnosis shown - silent)");
                if (g_cool_diag_seen) {
                    hud_show("RADIATOR CLEARED");
                    voice_say(MFV_RAD03, MFR_P_INFO);
                }
            }
            /* R3d (ADDENDUM 11): a cleared radiator ENDS the fault like a
             * repair (wear stays); next tick publishes cooling_pct -1 and
             * restores the levers once. */
            g_model_eng.onset = 0;
            g_model_eng.fault = MF_FAULT_NONE;
            g_model_eng.stabilised = 0;
            g_model_eng.stop_delay_s = -1.0;
        } else if (mfn_debris_blown(&g_model_eng) && !g_cool_cleared) {
            /* the model's own blow-clear roll succeeded */
            g_cool_cleared = 1;
            g_pitadv_until = 0;
            logf_("COOLING: debris blew clear%s",
                  g_cool_diag_seen ? "" : " (no diagnosis shown - silent)");
            if (g_cool_diag_seen) {
                hud_show("RADIATOR CLEARED");
                voice_say(MFV_RAD03, MFR_P_INFO);
            }
            /* R3d: end the fault (as above) */
            g_model_eng.onset = 0;
            g_model_eng.fault = MF_FAULT_NONE;
            g_model_eng.stabilised = 0;
            g_model_eng.stop_delay_s = -1.0;
        }
    }

    /* E8g item 4: debris scales ONLY the radiator airflow terms
     * (rad_air_scale), never RAD_BASE; coolant loss scales all three
     * (rad_scale). The game derives the airflow term itself. */
    lever_write(&g_rad_base, &RAD_BASE,   g_rad_base.saved * g_model_out.rad_scale);
    lever_write(&g_rad_k0,   &RAD_AIR_K0, g_rad_k0.saved   * g_model_out.rad_air_scale);
    lever_write(&g_rad_k1,   &RAD_AIR_K1, g_rad_k1.saved   * g_model_out.rad_air_scale);

    /* R3g: once the coolant is gone the engine heat has to go somewhere -
     * the model raises oil_heat_ks; apply it through the same latched
     * EngineSpeedHeat lever as the lubrication faults (owner 3, released the
     * moment the coolant is back or the fault ends). */
    g_cool_want_ks = mf_oil_heat_latch(g_cool_want_ks,
                                       g_cool_kind == 2 ? g_model_out.oil_heat_ks : 0.0,
                                       g_model_stepped);
    if (ENG_RUNNING && g_cool_want_ks > 0.0)
        oil_heat_hold(g_cool_want_ks, OIL_HEAT_OWNER_WATER);
    else
        oil_heat_release(OIL_HEAT_OWNER_WATER);

    wc = (float)(WATER_K - 273.16);
    (void)wc;
    /* 2026-10-01 messaging audit (docs/MESSAGING_AUDIT_2026-10-01.md):
     *   diagnosis   WATER TEMP / RADIATOR BLOCKED (car-relative, was 105 C)
     *   instruction COOL THE ENGINE, BOX - DEBRIS (after the diagnosis)
     *   critical    PIT AND RETIRE + pit call - water leak ONLY when the
     *               coolant is nearly gone (< MF_WATER_SENSOR_FRAC) or the
     *               head gasket nearly gone. Live 2026-10-01 it fired on
     *               +18 K alone for 400+ s while the car ran fine.
     *   radio       ONE pre-critical call per fault: marshals see the steam
     *               (leak) / engineer calls the car in (debris) / engineer
     *               sees the temperature (77) - first to hold wins. */
    /* L2 2026-10-05: low coolant level is its own diagnosis; WATER TEMP is
     * only the real temperature (model: baseline + MF_WATER_WARN_DK). */
    warn_after(W_COOLANTLOW, mfn_coolant_low(&g_model_out), 1000, 3000,
               "COOLANT LOW");
    warn_after(W_WATER, mfn_water_temp(&g_model_out), 1000, 3000, "WATER TEMP");
    instruct(I_WATER, mfn_water_instr(&g_model_out, g_warn_blink[W_WATER].on),
             8000, 14000, "COOL THE ENGINE");
    /* V3: WATER TEMP -> WATER01 (then WATER02 30-60 s later); "you're losing
     * coolant" after 1.0 L lost; the marshals/steam line only without them */
    voice_pre(R_WATER1, mfn_cooling_pre(g_cool_kind, g_warn_blink[W_WATER].on),
              3000, 8000, mfv_water_diagnosis(0), -1, 1);
    voice_instr(I_COOLANT1, mfn_coolant_lost_instr(&g_model_eng, g_cool_kind),
                1000, 3000, MFV_COOLANT01);
    if (!g_voice_on)
        say_pre(R_STEAM, mfn_steam_shown(g_cool_kind, g_cool_steam_on),
                5000, 20000, EID_MECHWARN);
    {
        int crit = crit_gate(C_WATER, mfn_water_crit(&g_model_eng, g_cool_kind),
                             g_warn_blink[W_WATER].on
                             || g_warn_blink[W_COOLANTLOW].on,
                             "WATER TEMP", (DWORD)game_ms());
        critical(C_WATER, crit, 2000, 5000, "PIT AND RETIRE");
        voice_crit_after_pre(RC_WATER, crit, 4000, 8000, MFV_COOLANT02, EID_PITCALL,
                             R_WATER1, mfv_water_diagnosis(0));
    }
    /* the engineer diagnoses debris from the temperature trace, later */
    warn_after(W_RADBLOCK, mfn_rad_blocked(&g_model_out, g_cool_kind),
               8000, 15000, "RADIATOR BLOCKED");
    /* owner 2026-09-28: once the block is diagnosed, the engineer calls the
     * car in (eid 49) + HUD BOX - DEBRIS for 8 s; debris can still blow
     * clear. 2026-10-01: through the radio queue, counts as the fault's one
     * pre-critical call. */
    if (g_cool_kind == 1 && !g_w_done[R_PITADV] && g_warn_blink[W_RADBLOCK].on
        && g_w_due[W_RADBLOCK]
        && (long)(game_ms() - g_w_due[W_RADBLOCK]) >= 3000
        && !wrecked_or_stopped()) {      /* E8b: same gate as say_after */
        g_w_done[R_PITADV] = 1;
        g_pitadv_until = game_ms() + 8000;
        if (!g_radio_pre_done) {
            g_radio_pre_done = 1;
            g_cool_diag_seen = 1;      /* N3 item 3: RAD01 pre line is the diagnosis */
            /* V3: RAD01 (then RAD02 30-60 s later); vanilla eid 49 without */
            if (g_voice_on) {
                radio_req(mfv_eid(mfv_water_diagnosis(1)), MFR_P_INFO);
                logf_("VOICE: %s (pre)", mfv_id(MFV_RAD01));
                voice_arm_instr(MFV_RAD01);
            } else {
                radio_req(EID_PIT01, MFR_P_INFO);
            }
        }
        logf_("COOLING: engineer calls the car in (eid %d)", EID_PIT01);
    }
    if (g_pitadv_until && (long)(game_ms() - g_pitadv_until) < 0)
        hud_show("BOX - DEBRIS");
    /* engineer sees the water temperature on telemetry 5-12 s later.
     * E9g-b item 2: car-relative - baseline + MF_WATER_COOL_DK (was 110 C),
     * silent while the model has no baseline. V3: eid 77 stays only for the
     * GAME's own overheat - the mod's line is WATER02 with the new voices. */
    if (!g_voice_on)
        say_pre(R_WATER77, mfn_water77(WATER_K, model_water_base()),
                5000, 12000, EID_OVERHEAT_W);
    else
        voice_instr(R_WATER77, mfn_cool_engine(&g_model_out),
                    5000, 12000, MFV_WATER02);

    /* steam: model plume on the negative bank; hiss once per episode */
    if (mfn_steam_bank(&g_model_out)) {
        bank1_set(g_model_out.smoke_bank1, "cooling");
        g_native_touched = 1;      /* E8b: mod-written bank */
        if (!g_cool_steam_on) {
            g_cool_steam_on = 1;
            play_fx("mf_steam_hiss.wav");
        }
    } else if (g_cool_steam_on) {
        bank1_set(0.0, "cooling");
        g_cool_steam_on = 0;
    }
#endif
}

/* ---- N8 fuel pressure loss (E4d: model-driven) ---------------------------
 * Pump-capacity wear, demand shortfall, the lean cut and the lean exhaust
 * temperature live in the model (mf_engine_model.h 4.6). This pump maps
 * MfOut.vfuel_press onto the game's own output multiplier, the gauge, the
 * lean pops and the HUD/radio lines. There is no fuel-pressure engine death
 * any more: the model floors the capacity at 0.35 and the outcome is a lean
 * engine, not a stop (handoff 4.6). Old percentage maths: MF_LEGACY_FUEL. */
static int   g_fp_on = 0;
static int   g_fp_cut = 0;   /* legacy mirror */
#if MF_LEGACY_FUEL
static float g_fp = 100.0f, g_fp_rate = 0.5f;
static DWORD g_fp_last = 0;
#endif

static void arm_fuel_pressure(void)
{
#if MF_LEGACY_FUEL
    g_fp_on = 1;
    g_fp = 100.0f;
    g_fp_rate = 100.0f / (120.0f + 180.0f * frand01());
    g_fp_last = 0;
    g_fp_cut = 0;
    mfv_roll(&g_v_fp, MFV_ALL);
    ev_pub("FUEL PRESSURE", &g_v_fp);
    logf_("FUEL PRESSURE loss armed: %.2f %%/s", g_fp_rate);
#else
    g_fp_cut = 0;
    mf_fault_arm(&g_model_eng, &g_model_in, MF_FAULT_FUEL_PRESSURE,
                 MF_SEV_MEDIUM, MF_MANUAL_BUDGET_S);
    logf_("FUEL PRESSURE loss: model fault armed (manual, medium, budget %.0f s)",
          MF_MANUAL_BUDGET_S);
#endif
}

static void fuelp_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_FUEL
    DWORD now = game_ms();
    float dt, load, p_cut;

    if (g_ctl) g_ctl->fuelpress_pct = g_fp_on ? (int)g_fp : -1;
    if (!g_fp_on) return;
    if (!g_fp_last) { g_fp_last = now; return; }
    dt = (float)(now - g_fp_last) / 1000.0f;
    g_fp_last = now;
    if (dt > 1.0f) dt = 1.0f;

    g_fp -= dt * g_fp_rate * load_factor() * mfv_mult(&g_v_fp, now, dt);
    if (g_fp < 0.0f) g_fp = 0.0f;
    /* fuel pressure sensor on the dash */
    warn_after(W_FUEL, g_fp < 70.0f, 1500, 3000, "FUEL PRESSURE");
    instruct(I_FUEL, g_fp < 70.0f, 6000, 10000, "SHIFT EARLY");
    if (g_fp < 80.0f && ENG_RUNNING && frand01() < (1.0f - g_fp / 100.0f) * 0.02f)
        exhaust_pop();                      /* lean misfire pops */


    if (g_fp < 8.0f && ENG_RUNNING) {
        engine_seize("fuel pump gone");   /* E9g item 3: life 0, ignition on */
        REASON_CODE = 9;                  /* keep the game's Fuel category */
        g_fp_on = 0;
        g_vfuel_press = -1.0f;
        logf_("FUEL PRESSURE: pump gone - engine off (ReasonCode 9, fuel)");
        engine_dead_msg();
        hud_show("FUEL PRESSURE");
        return;
    }
    if (!ENG_RUNNING) return;
    /* Live 2026-09-28 (owner): no effect before 0% - the torque>0 "on the
     * throttle" gate evidently never passed. Now: (1) the game's own
     * rev-phased torque flutter (stress) grows as pressure falls below 85%,
     * the same feel as vanilla low-fuel starvation; (2) full cut-outs by
     * revs only. */
    /* owner 2026-09-28: only the game's own low-fuel throttle-cut stutter
     * (top speed must not drop) - virtual fuel 0.9 -> 0 with the pressure */
    g_vfuel_press = 0.9f * g_fp / 100.0f;
    /* no ignition cuts any more (dash LEDs flashed); the stutter above is
     * the game's own torque flutter */
    (void)load; (void)p_cut;
#else
    /* E4d: model-driven. The physics runs in the model; here only the
     * game's own cut lever, the gauge and HUD/radio. */
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_FUEL_PRESSURE);

    g_fp_on = active;
    if (g_ctl) g_ctl->fuelpress_pct = active
        ? (int)(g_model_eng.fuel_cap * 100.0 + 0.5) : -1;
    if (!active) {
        if (g_vfuel_press >= 0.0f) g_vfuel_press = -1.0f;
        return;
    }

    /* lean mixture through the game's own low-fuel output multiplier */
    if (g_model_out.vfuel_press >= 0.0) {
        /* N3 addendum 16: no backfire sound without a visible flame spit -
         * the lean path has none (the exhaust-crack path spits bank 3). */
        g_vfuel_press = (float)g_model_out.vfuel_press;
    }

    /* fuel pressure sensor on the dash. 2026-10-01 audit: thresholds follow
     * the R3h symptom curve - the gauge shows it where the stumble starts
     * (MF_FP_STUMBLE, was 0.70 = symptoms with no reading), SHORT SHIFT
     * where the top-end cut starts (MF_FP_CUT_FULL), FUEL STARVATION (red)
     * + the pit call once the pump sits on its floor (R3i: the engine stops
     * MF_FP_STARVE_S later). No radio before that - invisible from outside. */
    warn_after(W_FUEL, mfn_fuel_cap_stumble(&g_model_eng), 1500, 3000,
               "FUEL PRESSURE");
    /* V3: FUEL PRESSURE -> FUELP01 (then FUELP02 30-60 s later) */
    voice_pre(R_FUELP1, g_warn_blink[W_FUEL].on, 3000, 8000,
              MFV_FUELP01, -1, 1);
    instruct(I_FUEL, mfn_fuel_instr(&g_model_eng, g_warn_blink[W_FUEL].on),
             6000, 10000, "SHORT SHIFT");
    {
        int crit_ok = crit_gate(C_FUELP, mfn_fuel_starve(&g_model_eng),
                                g_warn_blink[W_FUEL].on, "FUEL PRESSURE",
                                (DWORD)game_ms());
        critical(C_FUELP, crit_ok, 1000, 3000, "FUEL STARVATION");
        voice_crit(RC_FUELP, crit_ok, 3000, 6000,
                   MFV_CRIT01, EID_PITCALL);
    }
#endif
}

/* ---- fuel leak (E4d: model-driven) ---------------------------------------
 * Extra drain on the vanilla fuel (this+412, double; the game integrates it,
 * so the gauge, the low-fuel cut and ReasonCode 9 at empty all follow
 * natively). The physical rate (leak area x sqrt(fuel pressure)) and the
 * fire hazard live in the model (mf_engine_model.h 4.6); this pump applies
 * the model's accumulated loss to FUEL_NOW and keeps the measured-fuel
 * advice (SAVE FUEL / PIT FOR FUEL). The fire blows up through the shared
 * model terminal. Old percentage maths: MF_LEGACY_FUEL. Refuelling works;
 * the leak keeps going. */
#define FUEL_NOW (*(volatile double*)0x007C75C4u)
/* L2 2026-10-05: the FUEL LEAK diagnosis waits until this much fuel is really
 * gone (was: unconditionally 20-40 s after onset). */
/* FUEL_LEAK_WARN_L moved to mf_notify.h (N2 2026-10-06); value 0.1 L per the
 * Claude review 2026-10-05 (0.5 L = ~18 min at medium race pace, far too
 * late; 0.1 L ~ 4 min). */
static int    g_fl_on = 0;
static DWORD  g_fl_ms = 0;        /* new-path dt tracker */
static double g_fl_applied = -1.0;/* model fuel_lost_l already applied */
static double g_fl_prev = -1.0;   /* last fuel reading                     */
static float  g_fl_burn = 0.0f;   /* measured total burn, units/s (EMA)     */
static int    g_fl_lap = -1;
static DWORD  g_fl_lap_t0 = 0;
static float  g_fl_lap_s = 0.0f;  /* measured lap time, s                   */
static MfFuelLap g_fl_est;        /* R3o: per-lap estimate state            */
static int    g_fl_advice = 0;    /* R3o: 0 none, 1 save, 2 pit (hysteresis) */
/* 2026-10-07 live test run 3: "Fuel leak, box now" fired at 0.11 L lost in a
 * race fuelled for a planned stop - the advice compared fuel left with the
 * WHOLE race distance. The race-distance check now applies only when the car
 * was fuelled to finish (judged at the first measured lap of the leak);
 * otherwise only the run-dry rule (< 1.8 laps of fuel) calls the car in. */
static int    g_fl_plan = -1;     /* -1 unknown, 0 planned stop, 1 to the flag */
#if MF_LEGACY_FUEL
static float g_fl_rate = 0.12f;
static DWORD g_fl_last = 0;
#endif

static void arm_fuel_leak(void)
{
#if MF_LEGACY_FUEL
    g_fl_on = 1;
    g_fl_rate = 0.08f + 0.12f * frand01();
    g_fl_last = 0;
    g_fl_prev = -1.0; g_fl_burn = 0.0f; g_fl_lap = -1; g_fl_lap_t0 = 0; g_fl_lap_s = 0.0f;
    mfv_roll(&g_v_fl, MFV_ALL);
    ev_pub("FUEL LEAK", &g_v_fl);
    logf_("FUEL LEAK armed: %.3f/s (fuel %.2f)", g_fl_rate, FUEL_NOW);
#else
    g_fl_ms = 0;
    g_fl_applied = -1.0;
    g_fl_prev = -1.0; g_fl_burn = 0.0f; g_fl_lap = -1; g_fl_lap_t0 = 0; g_fl_lap_s = 0.0f;
    /* R3o: start the per-lap measurement; the first completed lap is partial
     * (the fault armed mid-lap) and is skipped */
    mf_fuel_lap_init(&g_fl_est);
    g_fl_est.lap = *(volatile int*)(PLR_VEH + OFF_LAP);
    g_fl_est.fuel0 = FUEL_NOW;
    g_fl_est.t0 = SESSION_TIME;
    g_fl_est.skip = 1;
    g_fl_advice = 0;
    g_fl_plan = -1;
    mf_fault_arm(&g_model_eng, &g_model_in, MF_FAULT_FUEL_LEAK,
                 MF_SEV_MEDIUM, MF_MANUAL_BUDGET_S);
    logf_("FUEL LEAK: model fault armed (manual, medium, budget %.0f s) "
          "(fuel %.2f)", MF_MANUAL_BUDGET_S, FUEL_NOW);
#endif
}

static void fuelleak_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_pump_frozen) return;    /* R1b: pause - no writes, no logs */
#if MF_LEGACY_FUEL
    DWORD now = game_ms();
    float dt;
    double f;

    if (g_ctl) { g_ctl->fuelleak_on = g_fl_on; g_ctl->fuel_now = (float)FUEL_NOW; }
    if (!g_fl_on) return;
    if (!g_fl_last) { g_fl_last = now; return; }
    dt = (float)(now - g_fl_last) / 1000.0f;
    g_fl_last = now;
    if (dt > 1.0f) dt = 1.0f;
    f = FUEL_NOW - (double)(dt * g_fl_rate * (0.6f + 0.4f * load_factor())
                            * mfv_mult(&g_v_fl, now, dt));
    if (f < 0.0) f = 0.0;
    FUEL_NOW = f;
    warn_after(W_FUELLEAK, 1, 20000, 40000, "FUEL LEAK");

    /* Owner 2026-09-29: SAVE FUEL came with 61 L on board (2x consumption).
     * The engineer now works it out like a real one: MEASURED burn (the
     * game's consumption incl. its fuel multiplier + the leak), MEASURED lap
     * time, laps the fuel will last vs laps to go. Nothing before one full
     * lap has been timed. */
    {
        int lap = *(volatile int*)(PLR_VEH + OFF_LAP);
        int total = *(volatile int*)TOTAL_LAPS_PTR;
        int race = (SESSION_TYPE == 7 && total > 0);
        float laps_left = -1.0f, rem = 0.0f;
        int save = 0, pit = 0;

        if (g_fl_prev >= 0.0 && f <= g_fl_prev && dt > 0.0f) {
            float inst = (float)(g_fl_prev - f) / dt;
            g_fl_burn = g_fl_burn > 0.0f ? g_fl_burn * 0.97f + inst * 0.03f : inst;
        }
        g_fl_prev = f;                      /* a refuel (f up) is just skipped */
        if (lap != g_fl_lap) {
            if (g_fl_lap >= 0 && g_fl_lap_t0) {
                float ls = (float)(now - g_fl_lap_t0) / 1000.0f;
                if (ls > 30.0f && ls < 400.0f) g_fl_lap_s = ls;
            }
            g_fl_lap = lap;
            g_fl_lap_t0 = now;
        }
        if (g_fl_lap_s > 0.0f && g_fl_burn > 1e-4f)
            laps_left = (float)f / g_fl_burn / g_fl_lap_s;
        if (race) rem = (float)(total - lap);
        if (laps_left >= 0.0f) {
            if (race && laps_left < rem) {
                if (laps_left >= rem * 0.85f && laps_left >= 1.8f) save = 1;
                else pit = 1;
            }
            if (laps_left < 1.8f) { pit = 1; save = 0; }
        }
        if (g_ctl) g_ctl->fuel_laps_x10 = laps_left >= 0.0f ? (int)(laps_left * 10.0f) : -1;
        instruct(I_FUELLEAK, save, 8000, 15000, "SAVE FUEL");
        say_after(R_FUELPIT, pit, 3000, 8000, EID_PIT01_V);
        instruct(I_FUELPIT, pit, 3000, 8000, "PIT FOR FUEL");
    }
#else
    /* E4d: model-driven. The model accumulates the leak; this pump moves the
     * loss onto the game's own fuel level and keeps the measured advice. */
    DWORD now = game_ms();
    int active = mfn_active_fault(&g_model_eng, g_model_live, MF_FAULT_FUEL_LEAK);
    float dt;
    double f;

    if (g_ctl) { g_ctl->fuelleak_on = active; g_ctl->fuel_now = (float)FUEL_NOW; }
    g_fl_on = active;
    if (!active) { g_fl_ms = 0; g_fl_applied = -1.0; g_fl_advice = 0; g_fl_plan = -1; return; }
    if (!g_fl_ms) { g_fl_ms = now; return; }
    dt = (float)(now - g_fl_ms) / 1000.0f;
    g_fl_ms = now;
    if (dt > 1.0f) dt = 1.0f;

    /* the model's accumulated loss hits the game's own fuel level (4.6); the
     * game then runs its gauge, low-fuel cut and ReasonCode 9 natively */
    if (g_fl_applied >= 0.0) {
        double delta = g_model_eng.fuel_lost_l - g_fl_applied;
        if (delta > 0.0) {
            f = FUEL_NOW - delta;
            FUEL_NOW = f > 0.0 ? f : 0.0;
        }
    }
    g_fl_applied = g_model_eng.fuel_lost_l;
    f = FUEL_NOW;
    /* L2 2026-10-05: only once >= FUEL_LEAK_WARN_L has actually been lost */
    warn_after(W_FUELLEAK, mfn_fuel_leak_warn(&g_model_eng),
               20000, 40000, "FUEL LEAK");
    /* V3: FUEL LEAK dash -> FUELLEAK01 (the critical row is FUELLEAK02) */
    voice_pre(R_FUELLEAK1, g_warn_blink[W_FUELLEAK].on, 3000, 8000,
              MFV_FUELLEAK01, -1, 0);

    /* R3o (ADDENDUM 11, Claude review): the engineer measures FUEL USED PER
     * LAP and the mean of the last 1-2 completed laps - the old per-tick burn
     * EMA (tau ~1.6 s) made laps_left swing with the last throttle burst.
     * Advice needs one full lap (the partial arm lap is skipped), and a
     * hysteresis keeps SAVE/PIT from flipping on estimate wobble. */
    {
        int lap = *(volatile int*)(PLR_VEH + OFF_LAP);
        int total = *(volatile int*)TOTAL_LAPS_PTR;
        int race = (SESSION_TYPE == 7 && total > 0);
        float laps_left = -1.0f, rem = 0.0f;

        mf_fuel_lap_step(&g_fl_est, lap, f, SESSION_TIME);
        laps_left = (float)mf_fuel_laps_left(&g_fl_est, f);
        if (race) rem = (float)(total - lap);
        if (g_fl_plan < 0 && race && laps_left >= 0.0f) {
            g_fl_plan = laps_left >= rem ? 1 : 0;
            logf_("FUEL LEAK: fuel plan %s (%.1f laps of fuel, %.0f laps to go)",
                  g_fl_plan ? "to the flag" : "planned stop", laps_left, rem);
        }
        g_fl_advice = mf_fuel_advice(laps_left, g_fl_plan == 1 ? rem : 0.0f,
                                     g_fl_advice);
        if (g_ctl) g_ctl->fuel_laps_x10 = laps_left >= 0.0f
                                          ? (int)(laps_left * 10.0f) : -1;
        /* 2026-10-01 audit: BOX FOR FUEL (one "box" wording); the pit call
         * is this fault's critical radio */
        instruct(I_FUELLEAK, mfn_fuel_save(g_fl_advice), 8000, 15000,
                 "SAVE FUEL");
        voice_crit_after_pre(R_FUELPIT, mfn_fuel_pit(g_fl_advice), 3000, 8000,
                             MFV_FUELLEAK02, EID_PIT01_V, R_FUELLEAK1, MFV_FUELLEAK01);
        instruct(I_FUELPIT, mfn_fuel_pit(g_fl_advice), 3000, 8000,
                 "BOX FOR FUEL");
    }
#endif
}

static void new_failures_clear(void)
{
    mfv_publish("engine", "-");
    g_fl_on = 0;
    rev_band_restore("REV BAND");
    g_cool_on = 0;
    cooling_restore();
    if (g_fp_cut) { g_fp_cut = 0; }
    g_fp_on = 0;
    g_vfuel_press = -1.0f;
    g_vfuel_valve = -1.0f;
    if (g_valve_cut) { g_valve_cut = 0; }
    g_valve_on = 0;
    g_oilp_on = 0;
    g_exh_on = 0;
    g_exh_flame_until = 0;
    g_bb_on = 0;
}

/* End of stint (owner 2026-09-28: NOTHING carries over): same rule as the
 * suspension family - session left, race retirement, or pits in a non-race
 * session. Clears every running failure and restores borrowed game values;
 * the engine itself is left as the game has it. */
static int g_stint_live = 0;
static void stint_clear_ex(const char *why, int player_only)
{
    /* N3 2026-10-06 (item 3/H13a): a stint/session/pit-lane clear shows NO
     * HUD and says NO radio (the old unconditional MFV_RAD03 here fired on
     * every non-race pit-lane clear, with no debris diagnosis ever given -
     * owner H13). */
    new_failures_clear();
    g_oil_on = 0;
    g_oil_last_tick = 0;
    g_heat_on = 0;
    g_cutout_on = 0;
    g_blow_start = 0;
    g_blow_fire_until = 0;
    g_blow_smoke_until = 0;
    fx_restore();
    g_wear_watch = 0;
    warn_reset();
    /* E8b: reset only the native failure channels the MOD wrote. After the
     * game's OWN staged failure (life re-armed with its stress and banks)
     * a pit stop or stint end must not heal it. E9a review -> E9g item 4:
     * the native-stage-one bank gate must end BEFORE the bank-1 clear, so a
     * session/stint change is never blocked by it. */
    if (g_native_touched) {
        g_model_eng.native_stage1 = 0;
        bank1_set(0.0, "repair");
        ENG_BANK2 = 0.0;
        ENG_BANK3 = 0.0;
        if (ENG_LIFE > 0.0) ENG_STRESS = 0.0;
        g_native_touched = 0;
    }
    retire_commentary_reset();
    hud_clear();                      /* 2026-10-01: arbiter + radio queue */
    mfr_clear(&g_radio);
    g_pitadv_until = 0;
    g_stop_at = 0;
    /* H3b/D1: the one invalidation path clears the MODEL, dead latch, save
     * mode and generation too (live bug 2026-10-01: after STINT END the model
     * kept onset=1/fault=9). drop=0: in-session releases write stock first. */
    if (player_only) engine_player_reset(why);
    else engine_invalidate(why, 0);
    logf_("STINT END (%s): all engine failures cleared%s", why,
          player_only ? " (player only)" : "");
}

static void stint_clear(const char *why) { stint_clear_ex(why, 0); }

static void stint_pump(void)
{
    const char *why;
    int fin, reason;
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    fin = *(volatile int*)(PLR_VEH + OFF_FINISH);
    /* N6 2026-10-06 (owner-approved): the pit lane NEVER clears a mod fault.
     * Cleared only by (a) the panel REPAIR, (b) leaving the session, (c) a
     * race retirement, (d) the game re-seeding the clock (model_pump, reason
     * "game reset"). PIT_STATUS is deliberately not consulted. */
    reason = mfn_stint_clear_reason(RACE_PHASE, SESSION_TYPE, fin);
    why = reason == 1 ? "session left"
        : reason == 2 ? "race retirement" : NULL;
    /* 2026-10-07 (tester report: OIL PRESSURE on every qualifying run after
     * the first): outside the race, a car back IN THE GARAGE is worked on by
     * the mechanics - the mod's faults clear once per garage visit. Garage
     * signature (FACT, findings garage-vs-track-signature): veh+0x4488 = 1,
     * veh+0x4578 = 1, engine off. The pit lane still never clears (N6). */
    {
        static DWORD garage_since = 0;
        static int garage_done = 0;
        /* 1.0.1 crash fix: it fired at the MAIN MENU (no session, garage
         * bytes = 1). Now only inside a live session, and only after the
         * car has been out on track in it (a real return to the garage). */
        static int was_out = 0;
        static unsigned out_gen = 0;
        int in_session = g_model_live && RACE_PHASE > 4 && !SESSION_OVER();
        int garaged;
        if (out_gen != g_session_gen) { out_gen = g_session_gen; was_out = 0; }
        if (in_session && ENG_RUNNING && VEH_SPEED > 5.0f) was_out = 1;
        /* 2026-10-07 c: garage = the game left the car for its monitor
         * (byte_7CE254, see on_circuit) - the AI garage bytes read 1/1 for
         * the player even while driving. */
        garaged = in_session && was_out && SESSION_TYPE != 7
            && *(volatile unsigned char*)0x007CE254u == 1;
        DWORD now = GetTickCount();
        if (!garaged) { garage_since = 0; garage_done = 0; }
        else if (!garage_since) garage_since = now;
        else if (!garage_done && now - garage_since >= 2000u) {
            garage_done = 1;
            was_out = 0;
            stint_clear_ex("garage visit (mechanics)", 1);
        }
    }
    if (!why) { g_stint_live = 1; return; }
    /* 2026-10-07 audit (owner D4): a player race retirement repairs only the
     * player's car - the AI race carries on with its own faults. */
    if (g_stint_live) { g_stint_live = 0; stint_clear_ex(why, reason == 2); }
}

/* Repair: restore the captured life, clear stress and the damage banks,
 * clear the corner accumulators, clear ReasonCode.                      */
static void repair_all(void)
{
    int i;
    int g_rep_was = g_oil_on || g_heat_on || g_valve_on || g_oilp_on || g_exh_on
                  || g_bb_on || g_cool_on || g_fp_on || g_fl_on || g_wear_watch
                  || !ENG_RUNNING || ENG_STRESS > 0.0 || ENG_LIFE < 0.0;
    /* N3 2026-10-06 (item 4): the repair line needs a diagnosis of the
     * CURRENT onset shown (HUD blink) or said (a radio line went out) - a
     * panel REPAIR on a healthy or undiagnosed engine is silent (log only).
     * Computed before the model reset below. */
    int diagnosed = mfn_diag_shown(
        g_diag_said_fault == g_model_eng.fault
        && g_diag_said_onset == g_model_eng.onset_t_s
        && g_model_live && g_model_eng.onset,
        (g_model_live && g_model_eng.onset)
        && (g_warn_blink[W_OILT].on || g_warn_blink[W_WATER].on
            || g_warn_blink[W_RADBLOCK].on || g_warn_blink[W_VALVE].on
            || g_warn_blink[W_OILP].on || g_warn_blink[W_OILLOW].on
            || g_warn_blink[W_FUEL].on || g_warn_blink[W_FUELLEAK].on
            || g_warn_blink[W_COOLANTLOW].on || g_warn_blink[W_BLOWBY].on
            || g_warn_blink[W_EXHTEMP].on));
    g_native_touched = 1;             /* E8b: repair writes the native channels */
    /* H3c/R09: only a snapshot from THIS engine/session generation may be
     * restored; an old engine's lifetime never returns. */
    if (g_have_saved && g_saved_life_gen == g_session_gen)
        ENG_LIFE = g_saved_life;
    else
        ENG_LIFE = 3600.0;
    ENG_STRESS = 0.0;
    /* Healthy is 0.0, NOT 1.0. TireSlipForceCalculation_004472b0 skips the
     * smoke emitter only when all three banks are exactly 0.0, and a
     * POSITIVE bank is used directly as an emission rate - so writing 1.0
     * here would leave the car smoking permanently.                     */
    g_model_eng.native_stage1 = 0;    /* E9g item 4: end the bank gate first */
    bank1_set(0.0, "repair");
    ENG_BANK2  = 0.0;
    ENG_BANK3  = 0.0;
    for (i = 0; i < 4; i++) {
        W_F32(i, W_ACCUM)  = 0.0f;
        W_F32(i, W_BEND_A) = 0.0f;
        W_F32(i, W_BEND_B) = 0.0f;
        W_U8 (i, W_BENT_FLAG) = 0;
    }
    REASON_CODE = 0;

    g_cutout_on = 0;
    g_cutout_until = 0;
    g_oil_on = 0;
    g_oil_pressure = 100.0f;
    g_oil_warned = 0;
    g_oil_last_tick = 0;
    g_heat_on = 0;
    g_heat_t = 0.0f;
    g_heat_last = 0;
    g_heat_plume_until = 0;
    g_blow_fire_until = 0;
    g_blow_smoke_until = 0;
    g_blow_start = 0;
    fx_restore();
    g_wear_watch = 0;
    new_failures_clear();
    g_power_mult = 1.0f;              /* E3: repair resets the power lever */
    engine_dead_clear("repair");      /* E9g-e item 1: the engine lives again */
    save_reset("repair");             /* E9i: save mode is off after a repair */
    /* N5 2026-10-06 (owner-approved): REPAIR on a hot engine - clamp the
     * game's oil/water temperature to the car's WARM NORMALS (never raise a
     * temperature), then give the repaired engine a 60 s grace with no
     * automatic onsets (MfEngine.repair_grace_s). The learned water normal
     * must be captured BEFORE model_reset zeroes it. */
    {
        double water_norm = g_model_eng.water_base_k > 0.0
                          ? g_model_eng.water_base_k : (80.0 + 273.15);
        double oil0, water0, oil_norm;
        model_reset((uint32_t)GetTickCount() | 1u);   /* E4: healthy model */
        oil0 = OIL_TEMP_K;
        water0 = WATER_K;
        oil_norm = (g_model_car_valid && g_model_car.oil_opt_k > 1.0)
                 ? g_model_car.oil_opt_k - 10.0 : oil0;
        OIL_TEMP_K = mf_repair_clamp_temp(oil0, oil_norm);
        WATER_K = mf_repair_clamp_temp(water0, water_norm);
        g_model_eng.repair_grace_s = 60.0;
        logf_("REPAIR: temps oil %.0f -> %.0f C, water %.0f -> %.0f C",
              oil0 - 273.15, OIL_TEMP_K - 273.15,
              water0 - 273.15, WATER_K - 273.15);
    }
    model_levers_release();           /* E8b/E9e: stock values, all levers */
    warn_reset();
    retire_commentary_reset();

    /* Restart the engine the way sub_4471F0 does: set the running byte.
     * E8c: the game sets its own rpm on restart, no lift needed.        */
    ENG_RUNNING = 1;

    logf_("REPAIR: life = %.2f, stress/banks/corners/ReasonCode cleared, "
          "engine restarted (rpm = %.0f)", ENG_LIFE, ENG_RPM);
    /* owner 2026-09-29: panel Reset-all showed this with nothing broken and
     * from the menus - only after a real fault, on track */
    if (g_rep_was && RACE_PHASE > 4 && diagnosed) {
        if (mfm_rate_ok(&g_repair_hud, game_ms(), 120000))
            hud_show("ENGINE REPAIRED");
        voice_say(MFV_REPAIR01, MFR_P_INFO);   /* V3 (rate inside voice_say) */
    } else if (g_rep_was && RACE_PHASE > 4) {
        logf_("REPAIR: no diagnosis shown - repair line suppressed");
    }
}

/* ---- control ----------------------------------------------------------- */
/* GUI only: all arming, glitch and repair commands arrive through the shared
 * control block below. In-game hotkeys were removed 2026-09-27.            */


static void dump_state(void)
{
    logf_("STATE life=%.3f stress=%.4f torque=%.1f rpm=%.0f running=%d "
          "banks=%.3f/%.3f/%.3f ReasonCode=%d phase=%d session=%d",
          ENG_LIFE, ENG_STRESS, ENG_TORQUE, ENG_RPM, (int)ENG_RUNNING,
          ENG_BANK1, ENG_BANK2, ENG_BANK3,
          REASON_CODE, RACE_PHASE, SESSION_TYPE);
    logf_("      corners accum FL=%.4f FR=%.4f RL=%.4f RR=%.4f",
          W_F32(0, W_ACCUM), W_F32(1, W_ACCUM),
          W_F32(2, W_ACCUM), W_F32(3, W_ACCUM));
}

/* Live readout. Torque is the direct evidence of the power deficit: stress
 * multiplies it by (1 - v36^2 * stress) every tick in Gearbox_00443b70, so
 * watching T while S climbs shows the misfire as it happens.            */
static void telemetry_pump(void)
{
    static DWORD last = 0;
    DWORD now = GetTickCount();
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (!g_telemetry || now - last < 250) return;
    last = now;
    logf_("TELEMETRY S=%.2f T=%.0f RUN=%d RC=%d",
          ENG_STRESS, ENG_TORQUE, (int)ENG_RUNNING, REASON_CODE);
}

/* GUI-driven arms (shared memory) carry the live-session gate: a write
 * outside a live session lands in a struct the game re-initialises at
 * session load and nothing shows. */
static void arm_action(int which)
{
    if (!session_live()) {
        logf_("ignored arm %d - no live session (phase = %d)", which, RACE_PHASE);
        return;
    }
    switch (which) {
        case 0: arm_engine_stage_one(); break;
        case 1: arm_engine_terminal();  break;
        case 2: model_manual_blow();    break;   /* E8g item 6: model terminal */
        case 3: arm_seizure();          break;
        default: break;
    }
}

/* ---- auto trigger pump -------------------------------------------------
 * One roll per live race/qualifying session (owner decisions 2026-09-27:
 * per-family percentages, engine 15% default; four core types; instant-death
 * types downgraded inside the final guard laps; never an automatic
 * retirement). The pump arms through the same proven levers the GUI uses. */
static unsigned g_auto_rng = 0;
static int g_auto_pending = 0;
static int g_auto_last_live = 0;
static int g_flavor_mult = 100;
static int g_flavor_done = 0;
static int g_flavor_enable_last = 0;
static int g_timer_seeded = 0;
static int g_timer_fired = 0;
static int g_timer_noseed_logged = 0;
static double g_timer_last = 0.0;

/* Auto mix (owner 2026-09-28): only failures built ON TOP of vanilla -
 * vanilla overheating/lifetime are not recycled; ENGINE STOP belongs to
 * electronics. valve 20 / oil pressure 20 / oil leak 20 / blow-by 15 /
 * exhaust 10 / blow up 15. */
static int roll_engine_type(int r)
{
    /* 2026-09-28 with cooling + fuel pressure: valve 15 / oil pressure 15 /
     * oil leak 15 / cooling 15 / fuel pressure 10 / blow-by 10 / exhaust 8 /
     * blow up 12 */
    if (r < 15) return AUTO_E_VALVE;
    if (r < 30) return AUTO_E_OILPRESS;
    if (r < 45) return AUTO_E_OIL;
    if (r < 60) return AUTO_E_COOLING;
    if (r < 66) return AUTO_E_FUELPRESS;
    if (r < 72) return AUTO_E_FUELLEAK;
    if (r < 80) return AUTO_E_BLOWBY;
    if (r < 88) return AUTO_E_EXHAUST;
    return AUTO_E_INSTANT;
}

static const char* auto_type_name(int t)
{
    switch (t) {
    case AUTO_E_SMOKE:   return "overheating";
    case AUTO_E_WEAR:    return "wear to death";
    case AUTO_E_INSTANT: return "instant blow";
    case AUTO_E_SEIZURE: return "seizure";
    case AUTO_E_OIL:     return "oil leak";
    case AUTO_E_VALVE:   return "valve return";
    case AUTO_E_OILPRESS: return "oil pressure loss";
    case AUTO_E_EXHAUST: return "exhaust crack";
    case AUTO_E_BLOWBY:  return "piston blow-by";
    case AUTO_E_COOLING: return "cooling failure";
    case AUTO_E_FUELPRESS: return "fuel pressure loss";
    case AUTO_E_FUELLEAK: return "fuel leak";
    }
    return "-";
}

/* Compute the per-session car-flavor multiplier from the frozen lifetime
 * seeds. Runs once at session start (or at the first tick after enabling,
 * if enabled before green). Neutral x1.00 whenever data is not usable. */
static void flavor_compute(void)
{
    EngineCtl *c = g_ctl;
    float mine;
    double sum = 0.0;
    int n, valid = 0, pm;

    g_flavor_mult = 100;
    g_flavor_done = 1;
    if (!c || c->magic != CTL_MAGIC || !c->flavor_enable) {
        if (c && c->magic == CTL_MAGIC) c->flavor_mult_pm = 100;
        return;
    }

    mine = *(volatile float*)(PLR_VEH + FLAVOR_COUNTER_OFF);
    c->flavor_counter = (int)mine;

    if (!(mine > 100.0f && mine < 100000.0f)) {
        c->flavor_mult_pm = 100;
        logf_("FLAVOR: player counter not seeded (mode 0/3?) - neutral x1.00");
        return;
    }

    for (n = 1; n < FLAVOR_VEH_COUNT; n++) {
        float v = *(volatile float*)(VEH_BASE
                                     + (size_t)n * FLAVOR_VEH_STRIDE
                                     + FLAVOR_COUNTER_OFF);
        if (v > 100.0f && v < 100000.0f) { sum += v; valid++; }
    }
    if (valid < 6) {
        c->flavor_mult_pm = 100;
        logf_("FLAVOR: too few live counters - neutral x1.00");
        return;
    }

    pm = (int)((sum / valid) / (double)mine * 100.0 + 0.5);
    if (pm < FLAVOR_CLAMP_LO) pm = FLAVOR_CLAMP_LO;
    if (pm > FLAVOR_CLAMP_HI) pm = FLAVOR_CLAMP_HI;
    g_flavor_mult = pm;
    c->flavor_mult_pm = pm;
    logf_("FLAVOR: counter=%.0f field mean=%.0f over %d cars - mult x%d.%02d",
          mine, sum / valid, valid, pm / 100, pm % 100);
}

/* Arm the pending fault through the proven levers. Common to both sources
 * (fixed-chance roll and vanilla timer). */
static void auto_fire(EngineCtl *c, int lap, const char* src)
{
    int total = *(volatile int*)TOTAL_LAPS_PTR;

    if ((g_auto_pending == AUTO_E_INSTANT || g_auto_pending == AUTO_E_SEIZURE)
        && total > 0 && c->auto_guard_laps > 0
        && lap >= total - c->auto_guard_laps) {
        logf_("%s: terminal downgraded by last-lap guard -> oil pressure", src);
        g_auto_pending = AUTO_E_OILPRESS;
        c->auto_type = AUTO_E_OILPRESS;
    }

    switch (g_auto_pending) {
    /* E5: the old forging levers (stage-one / terminal / instant / overheat)
     * write ENG_LIFE / ENG_STRESS and are panel-manual-only now. The old
     * auto sources (test-only, default OFF) fire model faults instead. */
    case AUTO_E_SMOKE:   arm_cooling(2);         break;   /* overheat -> water leak */
    case AUTO_E_WEAR:    arm_oil_pressure();     break;   /* wear -> bearings */
    case AUTO_E_INSTANT: arm_oil_pressure();     break;   /* no forged instant blow */
    case AUTO_E_OIL:     arm_oil_leak();         break;
    case AUTO_E_VALVE:   arm_valve_leak(100.0f); break;
    case AUTO_E_OILPRESS: arm_oil_pressure();    break;
    case AUTO_E_EXHAUST: arm_exhaust_crack();    break;
    case AUTO_E_BLOWBY:  arm_blowby();           break;
    case AUTO_E_COOLING: arm_cooling(3);         break;
    case AUTO_E_FUELPRESS: arm_fuel_pressure();  break;
    case AUTO_E_FUELLEAK: arm_fuel_leak();       break;
    default:             arm_oil_pressure();     break;   /* no forged seizure */
    }
    c->auto_fired = 1;
    c->auto_lap = lap;
    g_auto_pending = 0;
    logf_("%s: FIRED %s at lap %d", src, auto_type_name(c->auto_type), lap);
}

/* Vanilla lifetime timer source: drive the car's own game-seeded engine
 * counter at 1.0 per game-second and fire the family fault on expiry. The
 * fault type is rolled ONCE at arm time (not at expiry) so the panel can
 * show what will fire; the tamper command rewrites the counter live. */
static void timer_force(EngineCtl *c)
{
    float want = (float)c->timer_set_ms / 1000.0f;

    if (want < 0.0f) want = 0.0f;
    *(volatile float*)(PLR_VEH + TIMER_COUNTER_OFF) = want;
    g_timer_seeded = 1;
    g_timer_fired = 0;
    g_timer_noseed_logged = 1;
    g_timer_last = SESSION_TIME;
    logf_("TIMER: forced to %.1f s by the panel", want);
}

static void timer_tick(EngineCtl *c)
{
    double now, delta;
    float cur;
    int lap, r;

    if (c->cmd_set_timer) { c->cmd_set_timer = 0; timer_force(c); }
    if (g_timer_fired) { c->timer_remaining = 0; return; }

    cur = *(volatile float*)(PLR_VEH + TIMER_COUNTER_OFF);

    if (!g_timer_seeded) {
        if (!(cur > 0.0f && cur < 100000.0f)) {
            if (!g_timer_noseed_logged) {
                g_timer_noseed_logged = 1;
                logf_("TIMER: counter not seeded (PLR failure mode 0/3?) - idle");
            }
            c->timer_remaining = -1;
            return;
        }
        g_timer_seeded = 1;
        g_timer_last = SESSION_TIME;
        /* R4 realism: every slot is the ENGINE's LifetimeAvg/Var x3, so all
         * families would fail equally often. Scale this family's life once
         * at seed (engine 1.0 / gearbox 1.3 / electronics 1.5 /
         * suspension 3.0 - owner-approved 2026-09-28, tunable). */
        cur *= LIFE_MULT;
        *(volatile float*)(PLR_VEH + TIMER_COUNTER_OFF) = cur;
        if (!g_auto_pending) {
            g_auto_rng = g_auto_rng * 1664525u + 1013904223u;
            r = (int)((g_auto_rng >> 8) % 100u);
            g_auto_pending = roll_engine_type(r);
            c->auto_type = g_auto_pending;
        }
        logf_("TIMER: armed, counter=%.0f s, next fault = %s",
              cur, auto_type_name(g_auto_pending));
    }

    now = SESSION_TIME;
    delta = now - g_timer_last;
    g_timer_last = now;
    if (delta < 0.0) delta = 0.0;
    if (delta > 5.0) delta = 5.0;          /* pause / session jump guard */
    cur -= (float)delta;
    if (cur < 0.0f) cur = -1.0f;
    *(volatile float*)(PLR_VEH + TIMER_COUNTER_OFF) = cur;
    c->timer_remaining = (int)cur;

    if (cur > 0.0f) return;
    if (REASON_CODE == 1 || ENG_RUNNING == 0) return;  /* engine already failed */

    lap = *(volatile int*)(PLR_VEH + OFF_LAP);
    if (*(volatile int*)(PLR_VEH + OFF_FINISH) != 0) {
        logf_("TIMER: expired after the player finished - ignored");
        g_timer_fired = 1;
        return;
    }

    if (!g_auto_pending) {                 /* defensive - arm roll normally ran */
        g_auto_rng = g_auto_rng * 1664525u + 1013904223u;
        r = (int)((g_auto_rng >> 8) % 100u);
        g_auto_pending = roll_engine_type(r);
        c->auto_type = g_auto_pending;
    }
    g_timer_fired = 1;
    logf_("TIMER: engine counter expired at lap %d", lap);
    auto_fire(c, lap, "TIMER");
}

static void auto_pump(void)
{
    EngineCtl* c = g_ctl;
    int live, st, lap, pm, r;

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (!c || c->magic != CTL_MAGIC) return;

    st   = SESSION_TYPE;
    live = session_live();             /* any on-track session: the vanilla
                                        * timer ticks in all of them */
    /* the fixed-chance ROLL stays race (7) + qualifying (5) only */

    if (!live) {
        if (g_auto_last_live) {
            g_auto_last_live = 0;
            g_auto_pending = 0;
            c->auto_type = 0;
            logf_("AUTO: left the auto-eligible session");
            g_cool_on = 0;
            cooling_restore();
        }
        return;
    }
    if (!g_auto_last_live) {
        g_auto_last_live = 1;
        g_auto_pending = 0;
        c->auto_rolled = 0;
        c->auto_fired = 0;
        c->auto_type = 0;
        c->auto_lap = 0;
        c->auto_roll_result = 0;
        g_flavor_done = 0;
        g_timer_seeded = 0;
        g_timer_fired = 0;
        g_timer_noseed_logged = 0;
        c->timer_remaining = -1;
        if (c->auto_seed) g_auto_rng = (unsigned)c->auto_seed;
        else if (!g_auto_rng) g_auto_rng = GetTickCount();
        logf_("AUTO: session live (type=%d) - one roll, chance %d/1000",
              st, c->auto_chance_pm);
    }

    if (c->flavor_enable && !g_flavor_enable_last && g_auto_last_live)
        logf_("FLAVOR: enabled mid-session - applies next session");
    g_flavor_enable_last = c->flavor_enable;

    if (!g_flavor_done && !c->timer_enable) flavor_compute();

    if (!c->auto_enable) return;

    if (c->timer_enable) { timer_tick(c); return; }

    if ((st == 5 || st == 7) && !c->auto_rolled) {
        pm = c->auto_chance_pm;
        if (c->flavor_enable && g_flavor_mult != 100)
            pm = (pm * g_flavor_mult + 50) / 100;
        if (pm > 1000) pm = 1000;
        c->auto_rolled = 1;
        g_auto_rng = g_auto_rng * 1664525u + 1013904223u;
        if (pm <= 0 || (pm < 1000 && (int)((g_auto_rng >> 16) % 1000u) >= pm)) {
            c->auto_roll_result = 0;
            logf_("AUTO: roll MISS - clean session (chance %d/1000)", pm);
            return;
        }
        c->auto_roll_result = 1;
        r = (int)((g_auto_rng >> 8) % 100u);
        g_auto_pending = roll_engine_type(r);
        c->auto_type = g_auto_pending;
        logf_("AUTO: roll HIT - %s pending (earliest lap %d, chance %d/1000)",
              auto_type_name(g_auto_pending), c->auto_earliest_lap, pm);
    }

    if (!g_auto_pending) return;

    lap = *(volatile int*)(PLR_VEH + OFF_LAP);
    if (lap < c->auto_earliest_lap) return;

    /* never arm after the player has finished/been classified */
    if (*(volatile int*)(PLR_VEH + OFF_FINISH) != 0) {
        logf_("AUTO: player finished - pending fault dropped");
        g_auto_pending = 0;
        return;
    }

    auto_fire(c, lap, "AUTO");
}

/* ---- engine sim model v2 (E2 shadow -> E4 outputs live, 2026-09-30) -----
 * Reads game memory into MfCar / MfInputs and calls the pure model in
 * dll/mf_engine_model.h every pump. E4: outputs drive the game through the
 * family pumps (power via g_power_mult, terminals here, warnings/effects in
 * the valve/oil/cooling/fuel/rings/exhaust/overrev pumps). Model state
 * lives at the top of this file. Logs once per session the derived per-car
 * values and every 5 s the predicted time to failure, onset state and
 * component health. The model never writes game memory (1.4 / 1.5).     */

/* E8e (owner live test 1 BLOCKER): raw car values were read at the session
 * live-edge before the game filled the physics object, so the model ran on
 * the relative fallbacks. The model must not derive or step until the raw
 * values are real; re-derive once when they first become valid and again
 * when the car changes without a session edge (> 0.5 %). */
static int model_car_raw_valid(void)
{
    /* E9-H2c: every raw read must be finite before it reaches derive */
    double rl = ENG_REV_LIMIT, rb = LT_RPM_BASE, ob = LT_OIL_BASE;
    if (!isfinite(rl) || !isfinite(rb) || !isfinite(ob)) return 0;
    return rl > 100.0 && rb > 100.0 && ob > 250.0;
}

static int model_car_changed(void)
{
    if (g_model_car_base_snapshot <= 0.0 || g_model_car_oil_snapshot <= 0.0)
        return 0;
    /* H3a/R06: compare ONLY fields the mod never writes. ENG_REV_LIMIT is
     * lowered by the rev-band levers (save x0.97 / valve ceiling), so using
     * it made our own write look like a car change and the model re-derived
     * from the lowered limiter as if it were stock. The untouched identity
     * fields are the rpm base and the oil base. */
    return fabs(LT_RPM_BASE - g_model_car_base_snapshot)
               > 0.005 * g_model_car_base_snapshot
        || fabs(LT_OIL_BASE - g_model_car_oil_snapshot)
               > 0.005 * g_model_car_oil_snapshot;
}

/* E9a: derive the car's torque curve from its RPMTorque table (see the macros
 * at the top). Returns 0 for anything unreadable or out of range; mf_car_derive
 * then substitutes the relative fallbacks and sets the def_flags bits. */
static void model_torque_table_derive(double *zero_rads, double *peak_rads)
{
    const float *tq = (const float*)(uintptr_t)TQ_TABLE_PTR;
    int count = (int)TQ_TABLE_COUNT;
    int i, peak_i = -1, zero_i = -1;
    double peak_val = 0.0;
    *zero_rads = 0.0;
    *peak_rads = 0.0;
    if (!tq || count < 10 || count > 200
        || IsBadReadPtr(tq, (UINT_PTR)count * 2u * sizeof(float)))
        return;
    for (i = 0; i < count; i++) {
        double power = (double)tq[2 * i] + (double)tq[2 * i + 1];
        if (peak_i < 0 || power > peak_val) { peak_val = power; peak_i = i; }
    }
    if (peak_i < 0) return;
    for (i = peak_i + 1; i < count; i++) {
        double power = (double)tq[2 * i] + (double)tq[2 * i + 1];
        if (power <= 0.0) { zero_i = i; break; }
    }
    *peak_rads = (double)peak_i * MF_TQ_STEP_RADS;
    if (zero_i > peak_i)
        *zero_rads = (double)zero_i * MF_TQ_STEP_RADS;
}

static void model_car_log(void)
{
    double rpm_inv   = LT_RPM_INVR;
    double oil_inv   = LT_OIL_INVR;
    double rpm_range = rpm_inv > 1e-9 ? 1.0 / rpm_inv : 0.0;
    double oil_range = oil_inv > 1e-9 ? 1.0 / oil_inv : 0.0;
    double tq_zero, tq_peak;
    unsigned flags;

    model_torque_table_derive(&tq_zero, &tq_peak);
    flags = mf_car_derive(&g_model_car, ENG_REV_LIMIT, LT_RPM_BASE,
                          rpm_range, LT_OIL_BASE, oil_range, OIL_LIMIT_K,
                          tq_zero, tq_peak,
                          ENG_INERTIA, IDLE_RPM_LO, IDLE_RPM_HI,
                          FUEL_CONS, FUEL_MULT, OWT_K0, OWT_K1);
    logf_("MODEL car valid: raw rev_limit=%.2f rpm_base=%.2f rpm_range=%.3f "
          "oil_base=%.2f oil_range=%.4f oil_opt=%.2f | rel rev_limit=%.1f "
          "rpm_base=%.1f rpm_range=%.2f oil_base=%.1f oil_range=%.3f "
          "oil_opt=%.1f w_oil_ref=%.5f fallbacks=0x%04X speed_heat=%.6g",
          ENG_REV_LIMIT, LT_RPM_BASE, rpm_range, LT_OIL_BASE, oil_range, OIL_LIMIT_K,
          g_model_car.rev_limit_rads, g_model_car.rpm_base_rads,
          g_model_car.rpm_range_rads, g_model_car.oil_base_k,
          g_model_car.oil_range_k, g_model_car.oil_opt_k,
          g_model_car.w_oil_ref, flags, OIL_SPEED_HEAT);
    /* E9a, DERIVED rule: one CARDATA line per torque value. */
    logf_("CARDATA tq_zero_rads=%.1f (%s)", g_model_car.tq_zero_rads,
          (flags & 64u) ? "fallback" : "ok");
    logf_("CARDATA peak_tq_rads=%.1f (%s)", g_model_car.peak_tq_rads,
          (flags & 128u) ? "fallback" : "ok");
    /* E9e, DERIVED rule: one CARDATA line per new value (rel = fallback used) */
    logf_("CARDATA inertia=%.4f (%s)", g_model_car.inertia_kgm2,
          (flags & 256u) ? "fallback" : "ok");
    logf_("CARDATA idle_lo=%.1f idle_hi=%.1f (%s)", g_model_car.idle_lo_rads,
          g_model_car.idle_hi_rads,
          (flags & (512u | 1024u)) ? "fallback" : "ok");
    logf_("CARDATA fuel_cons=%.6g fuel_mult=%.3f (%s)", g_model_car.fuel_cons,
          g_model_car.fuel_mult,
          (flags & (2048u | 4096u)) ? "fallback" : "ok");
    logf_("CARDATA owt_k0=%.6g owt_k1=%.6g (%s)", g_model_car.owt_k0,
          g_model_car.owt_k1, (flags & (8192u | 16384u)) ? "fallback" : "ok");
    logf_("CARDATA comb_heat=%.4g oil_min_cool=%.6g idle_throttle=%.4g",
          COMB_HEAT, OIL_MIN_COOL, IDLE_THROTTLE);
    /* E9b-2 item 8: engine quality, published once per car derive (log only:
     * the control block has no free slot without an ABI change). */
    logf_("QUALITY q=%.3f", g_model_eng.quality);
    /* E9q 2.3: the grade needs each car's LifetimeAvg/Var relative to the
     * session field. The research (E9q DATA SOURCE, log section 8) found no
     * persistent per-vehicle address for the parsed values - they live in the
     * transient per-step parse buffer - so the field grade is NOT wired.
     * DECISION (post-run prompt 2.3): player path only, G stays 1.0. */
    logf_("GRADE: field data source pending");
}

/* E4a: publish the model diagnostics into the control block (ABI 12) */
static void model_publish(void)
{
    EngineCtl *c = g_ctl;
    int st;

    if (!c || c->magic != CTL_MAGIC) return;

    if (g_model_eng.blown) st = 4;
    else if (g_model_eng.stopped) st = 5;
    else if (g_model_eng.culminated) st = 3;
    else if (g_model_eng.onset) st = g_model_eng.stabilised ? 2 : 1;
    else st = 0;

    c->wear_x100 = (int)(g_model_eng.drain_ema_sps * 100.0 + 0.5);
    c->time_to_fail_s = (g_model_eng.drain_valid
                         && g_model_eng.drain_ema_sps > MF_DRAIN_MIN_SPS)
        ? (float)(g_model_in.life_s / g_model_eng.drain_ema_sps) : -1.0f;
    c->onset_state = st;
    c->bearings_pct = (int)(g_model_eng.h_b * 100.0 + 0.5);
    c->valvetrain_pct = (int)(g_model_eng.h_vt * 100.0 + 0.5);
    c->rings_pct = (int)(g_model_eng.ring_s * 100.0 + 0.5);
    c->conrods_pct = (int)(g_model_eng.h_rod * 100.0 + 0.5);
    c->gasket_pct = (int)(g_model_eng.h_g * 100.0 + 0.5);
    c->oil_l_x10 = (int)(g_model_eng.v_oil_l * 10.0 + 0.5);
    c->coolant_l_x10 = (int)(g_model_eng.v_w_l * 10.0 + 0.5);
    c->oil_p_pct = (int)(model_oil_pressure() * 100.0 + 0.5);
    c->valve_p_pct = (int)(model_valve_pressure() * 100.0 + 0.5);
    c->crack_pct = (int)(g_model_eng.crack_a * 100.0 + 0.5);
    c->rpm_base_x10 = (int)(g_model_car.rpm_base_rads * 10.0 + 0.5);
    c->rpm_range_x10 = (int)(g_model_car.rpm_range_rads * 10.0 + 0.5);
    c->oil_base_x10 = (int)(g_model_car.oil_base_k * 10.0 + 0.5);
    c->oil_range_x100 = (int)(g_model_car.oil_range_k * 100.0 + 0.5);
    c->rev_limit_x10 = (int)(g_model_car.rev_limit_rads * 10.0 + 0.5);
    c->life_s = (float)g_model_in.life_s;

    /* E8d (ABI 13): part temperatures (C), oil film and the active fault id */
    c->t_piston_c = (float)(g_model_eng.t_pis_k - 273.15);
    c->t_exh_valve_c = (float)(g_model_eng.t_ev_k - 273.15);
    c->t_bearing_c = (float)(g_model_eng.t_brg_k - 273.15);
    c->t_head_c = (float)(g_model_eng.t_head_k - 273.15);
    c->t_exhaust_c = (float)(g_model_eng.t_exh_k - 273.15);
    c->film_pct = (int)(g_model_eng.film_last / MF_FILM_CRIT * 100.0 + 0.5);
    if (c->film_pct > 999) c->film_pct = 999;
    c->fault_id = (g_model_eng.fault == MF_FAULT_NONE) ? -1 : g_model_eng.fault;
    /* R3m: rear heat is monotonic and never jumps; the ETA (exhaust_s) is
     * smoothed in exhaust_pump */
    c->rear_heat_pct = (int)(100.0 * g_model_eng.rear_heat / MF_REAR_HEAT_MAX
                             + 0.5);
    if (c->rear_heat_pct < 0) c->rear_heat_pct = 0;
    if (c->rear_heat_pct > 100) c->rear_heat_pct = 100;

    /* R3l (ADDENDUM 11): the dead status the panel must trust INSTEAD of the
     * game's ignition/running byte (owner live: IGNITION flipped the panel
     * between ENGINE DEAD and Running normally while the engine stayed dead).
     * Latched until repair/new session clears g_engine_dead. */
    c->dead_latch = g_engine_dead ? 1 : 0;
    if (g_engine_dead) {
        const char *nm;
        int i;
        c->dead_state = (g_model_eng.stopped || st == 5) ? 5
                      : (g_model_eng.blown || st == 4) ? 4 : st;
        c->dead_fault = (g_model_eng.fault == MF_FAULT_NONE)
                        ? -1 : g_model_eng.fault;
        if (g_dead_why[0]) nm = g_dead_why;               /* latched cause */
        else if (g_model_out.reason[0]) nm = g_model_out.reason;
        else if (g_model_eng.fault != MF_FAULT_NONE)
            nm = dmg_fault_name_(g_model_eng.fault);
        else nm = "engine";
        for (i = 0; i < 11 && nm[i]; i++) c->dead_fault_name[i] = nm[i];
        c->dead_fault_name[i] = '\0';
        /* N3 H16 (ABI 14): the full reason string for the panel label -
         * "ENGINE DEAD - fuelleak" must never appear when the real cause was
         * an over-rev, so the panel shows the model's own reason text. */
        {
            int k;
            for (k = 0; k < 47 && nm[k]; k++) c->dead_reason[k] = nm[k];
            c->dead_reason[k] = '\0';
        }
    } else {
        c->dead_state = 0;
        c->dead_fault = -1;
        c->dead_fault_name[0] = '\0';
        c->dead_reason[0] = '\0';
    }
}

/* E4a: model terminal states -> the proven game end states. blow_up is the
 * only terminal with the loud bang (blow_burst_start). engine_stop is the
 * no-bang stop: blue puffs for rings, white steam plume for the gasket. */
static void model_terminal_pump(void)
{
    DWORD now = game_ms();

    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_model_stop_plume_until) {
        if (g_model_stop_blue) {
            if (!g_model_puff_next || (long)(now - g_model_puff_next) >= 0) {
                emit_bluesmoke_burst(3, 1.0f);
                g_model_puff_next = now + 250;
            }
        } else if (!ENG_RUNNING) {
            bank1_set(EMIT_ON, "model-stop"); /* steam while it cools */
            g_native_touched = 1;             /* E8b: mod-written bank */
        }
        if ((long)(now - g_model_stop_plume_until) >= 0) {
            int was_blue = g_model_stop_blue;
            g_model_stop_plume_until = 0;
            g_model_stop_blue = 0;
            g_model_puff_next = 0;
            if (!was_blue && ENG_RUNNING == 0) bank1_set(0.0, "model-stop");
        }
    }

    if (g_model_terminal_done) return;

    if (g_model_out.blow_up) {
        g_model_terminal_done = 1;
        logf_("MODEL: BLOW UP (%s)", g_model_out.reason);
        engine_dead_latch(g_model_out.reason);   /* E9g-e item 1: terminal */
        blow_burst_start();
        engine_seize(g_model_out.reason);   /* E8h: life 0, ignition stays */
    } else if (g_model_out.engine_stop) {
        g_model_terminal_done = 1;
        g_model_stop_blue = (g_model_eng.fault == MF_FAULT_BLOWBY) ? 1 : 0;
        g_model_stop_plume_until = now + 10000;
        g_model_puff_next = now;
        engine_seize(g_model_out.reason);   /* E8h: the game stops it, lights on */
        g_death_quiet = 1;                  /* 2026-10-01: not a blow-up - no RETIRE04 */
        /* V3: gasket death gets its own line before the DEAD line; every
         * mod stop gets DEAD01/02 (random) instead of the blow-up line. */
        if (mfn_reason_gasket(g_model_out.reason))
            voice_say(MFV_GASKET01, MFR_P_DEATH);
        g_voice_dead = 1;
        g_voice_dead_idx = (rand() & 1) ? MFV_DEAD01 : MFV_DEAD02;
        retire_pending_arm();
        engine_dead_msg();
        logf_("MODEL: engine stopped, no bang (%s)", g_model_out.reason);
        hud_show("ENGINE FAILURE");
    }
}

/* E8g item 6: the panel BLOW UP button arms the MODEL terminal - no
 * ENG_LIFE write - and then starts the proven fireball. */
static void model_manual_blow(void)
{
    if (g_model_eng.blown) return;
    g_model_eng.blown = 1;
    g_model_eng.h_vt = 0.0;
    g_model_eng.h_rod = 0.0;
    g_model_eng.h_rod_dmg = 1.0;
    snprintf(g_model_out.reason, sizeof g_model_out.reason, "manual blow-up");
    g_model_terminal_done = 1;    /* the fireball below is the presentation */
    logf_("MODEL: BLOW UP (manual blow-up)");
    engine_dead_latch("manual blow-up");   /* E9g-e item 1: terminal */
    blow_burst_start();
    engine_seize("manual blow-up");   /* E8h: life 0, ignition stays on */
}

/* E8g item 5: the game's OWN clock reached 0 while we were on track: show
 * the engine failure and use the existing retire radio path. No game writes
 * - the game's own smoke/stop stays exactly as the game does it. */
static void model_clockout_pump(void)
{
    if (SESSION_OVER()) return;   /* E9g item 5: session-end guard */
    if (g_model_culm_done || !g_model_eng.culminated) return;
    g_model_culm_done = 1;
    if (g_model_eng.blown || g_model_eng.stopped) return;   /* ours won */
    /* E9g-c: the presentation (blow sound/particles, HUD, retire radio) only
     * in a live drive; otherwise it is dropped, not deferred. */
    if (!live_drive()) return;
    if (g_model_eng.culm_collision) {
        /* E9g-e item 5 (live check 2): the wall crash's zero under the native
         * collision ReasonCode is a failure, not a stint reset. The game's
         * own crash handling is the presentation - no radio arming here. */
        logf_("MODEL: game clock ran out (collision) - failure counted, "
              "not a reset");
        return;
    }
    if (g_model_eng.culm_overrev) {
        /* E9a item 4: the clock ran out within MF_OVR_BLAME_S of an over-rev
         * jump - present it as the torque-zero valve/piston death. The life
         * is already 0 (the game wrote it): no life write, just the proven
         * BLOW UP presentation. */
        logf_("MODEL: game clock ran out %.1f s after an over-rev jump - "
              "BLOW UP presentation (over-rev: valve hit piston)",
              g_model_eng.t_s - g_model_eng.overrev_last_t_s);
        snprintf(g_model_out.reason, sizeof g_model_out.reason,
                 "over-rev: valve hit piston");
        blow_burst_start();
        return;
    }
    logf_("MODEL: game clock ran out (life 0) - ENGINE FAILURE + retire radio");
    /* E9g-f: the plain game clock-out is a real terminal too - latch it (no
     * life write, no ignition write) so the E9g-e dead-engine torque cut holds
     * if the game ever leaves life 0 at speed without its own stress kill. */
    engine_dead_latch("game clock-out");
    hud_show("ENGINE FAILURE");
    engine_dead_msg();
    retire_pending_arm();
}

/* E9-H2c: a nonfinite raw read is a broken sample - fall back to the
 * documented safe default instead of propagating NaN/Inf into the model. */
static double mf_fin(double v, double dflt)
{
    return isfinite(v) ? v : dflt;
}

static void model_inputs(MfInputs *in, double dt)
{
    double speed = (double)VEH_SPEED;
    double gl = (double)(*(volatile float*)(PLR_VEH + OFF_G_LONG));
    double gt = (double)(*(volatile float*)(PLR_VEH + OFF_G_LAT));
    memset(in, 0, sizeof *in);
    in->dt_s      = mf_fin(dt, 0.0);
    in->life_s    = mf_fin(ENG_LIFE, 1e12);   /* READ ONLY; unknown = huge */
    in->rpm_rads  = mf_fin((double)ENG_RPM_LIVE, 0.0); /* never 0x7C90C4 */
    in->oil_k     = mf_fin(OIL_TEMP_K, MF_AMBIENT_FALLBACK_K);
    in->water_k   = mf_fin(WATER_K, MF_AMBIENT_FALLBACK_K);
    in->load      = mf_fin(ENG_TORQUE, 0.0);  /* P0: 0..1 throttle/torque */
    in->speed_mps = mf_fin(speed, 0.0);
    in->fuel_l    = mf_fin(FUEL_NOW, 0.0);
    /* E9e (addendum 2 section 7): the player's own lateral/longitudinal
     * acceleration (FACT); g_long keeps the game's sign (forward negative) -
     * the model uses the combined magnitude */
    in->g_long    = mf_fin(-gl / 9.81, 0.0);
    in->g_lat     = mf_fin(gt / 9.81, 0.0);
    in->ambient_c = mf_fin(AMBIENT_C, MF_AMBIENT_FALLBACK_K - 273.15);
    in->stress    = 0.0;                      /* drain is measured from life */
    in->engine_running = ENG_RUNNING ? 1 : 0;
    /* 2026-10-07 owner containment: the model runs only ON THE CIRCUIT -
     * no onset, growth, impact or warning in the pit lane, box, garage,
     * menus or results; a fault freezes in the pits and carries on after. */
    in->on_track  = on_circuit();
    in->in_pit    = (PIT_STATUS == 3);
    in->off_track = 0;                        /* P0: no surface flag found */
    in->life_infinite = (PLR_FAILRATE == 0);  /* E5: Failure Rate "none" */
    in->self_seize = g_engine_dead ? 1 : 0;   /* E9g-e: our own seizure */
    in->reason_code = REASON_CODE;            /* E9g-e: 4 = collision */
    in->niggles_enable = (g_ctl && g_ctl->magic == CTL_MAGIC)
                       ? (g_ctl->niggle_enable != 0) : 1;   /* E9b item 3 */
    in->early_enable = 1;                     /* E9b-2 item 6: always on in game */
    in->save_mode = g_save_mode;              /* E9i: F8 / panel state */
    /* Q3: hazard onset default (1); the panel can select countdown only (0) */
    in->onset_model = (g_ctl && g_ctl->magic == CTL_MAGIC
                       && g_ctl->onset_model == 0) ? 0 : 1;
}

/* ---- E9d: the game's own symptoms ---------------------------------------- */

/* Item 1: the cockpit oil-pressure gauge follows the model during lubrication
 * faults. The game relaxes the native value toward its own simulation at
 * ~4/s, so a per-tick multiply needs no restore; the first write of each
 * episode is logged with before/after values. DERIVED address. */
static int g_gauge_episode = 0;
static int g_gauge_bad_logged = 0;

static void oil_gauge_pump(void)
{
    double s = g_model_out.oil_gauge_scale;
    double v;

    if (!(s < 0.999)) { g_gauge_episode = 0; return; }
    v = OIL_PRESS_NATIVE;
    if (!(v > 0.0 && v < 5000.0)) {
        if (!g_gauge_bad_logged) {
            g_gauge_bad_logged = 1;
            logf_("GAUGE: native oil pressure out of range (%.4g) - no gauge effect", v);
        }
        return;
    }
    OIL_PRESS_NATIVE = v * s;
    if (!g_gauge_episode) {
        g_gauge_episode = 1;
        logf_("GAUGE: oil pressure %.1f -> %.1f kPa (scale %.3f)", v, v * s, s);
    }
}

/* Item 2: worn bearings raise engine braking through the game's own
 * ENG_BRAKE_MAP; ownership-checked restore once the bearings are healthy
 * again (and at repair / session change / unload via model_levers_release). */
static void friction_pump(void)
{
    double hb = g_model_eng.h_b;

    if (g_model_live && hb < 0.999) {
        if (!lever_capture(&g_ebrake, &ENG_BRAKE_MAP, 0.0, 1.0)) {
            if (!g_ebrake_bad) {
                g_ebrake_bad = 1;
                logf_("FRICTION: ENG_BRAKE_MAP stock invalid (%.4g) - no friction raise",
                      ENG_BRAKE_MAP);
            }
            return;
        }
        lever_write(&g_ebrake, &ENG_BRAKE_MAP,
                    g_ebrake.saved * mf_friction_factor(hb));
    } else if (lever_restore(&g_ebrake, &ENG_BRAKE_MAP, "ENG_BRAKE_MAP")) {
        logf_("FRICTION: engine braking map restored");
    }
}

/* Item 4: blow-by and lean running burn extra fuel; the model accumulates the
 * rate and this pump moves it onto the game's own fuel level (same mechanism
 * as the fuel leak). */
static void fuel_burn_pump(double dt)
{
    double extra = g_model_out.fuel_extra_lps;

    if (extra > 0.0 && ENG_RUNNING && dt > 0.0) {
        double f = FUEL_NOW - extra * dt;
        FUEL_NOW = f > 0.0 ? f : 0.0;
    }
}

/* E9b item 3: harmless niggles - the model rolls them; this pump maps the
 * active kind onto the game's own channels: MISFIRE is already in
 * MfOut.misfire_depth (the E9d hook pulses it), SENSOR shows the dash
 * message, PUFF is a one-second bank-1 puff. The model never touches life
 * and never runs a niggle while an onset is active. */
static void niggle_pump(void)
{
    int k = g_model_live ? g_model_out.niggle_kind : 0;

    if (k != g_niggle_prev_kind && k != 0) {
        const char *name = k == MF_NIGGLE_MISFIRE ? "misfire"
                         : k == MF_NIGGLE_SENSOR ? "sensor" : "puff";
        logf_("NIGGLE: %s (%.1f s)", name, g_model_out.niggle_left_s);
    }
    if (g_niggle_prev_kind == MF_NIGGLE_PUFF && k != MF_NIGGLE_PUFF
        && !g_engine_dead)
        bank1_set(0.0, "niggle");
    /* E9-H2e/D5: niggles off clears the active symptom at once - the puff
     * bank above, the misfire via MfOut (depth drops to 0), and here the
     * sensor message, only when it is still OUR sensor line. */
    if (g_niggle_prev_kind == MF_NIGGLE_SENSOR && k != MF_NIGGLE_SENSOR)
        hud_cancel("SENSOR FAULT");
    {
        static MfHudBlink sensor_blink;   /* R3c: edge + 45 s reminders */
        hud_blink(&sensor_blink, k == MF_NIGGLE_SENSOR, "SENSOR FAULT");
    }
    if (k == MF_NIGGLE_PUFF && !g_engine_dead)
        bank1_set(MF_NIGGLE_PUFF_BANK, "niggle");
    g_niggle_prev_kind = k;
}

/* ---- E9p: AI shadow engines ---------------------------------------------
 * The game gives AI cars only a countdown timer (veh+0x3D70, ReasonCode 1);
 * there is no AI thermal model. This runs one shadow engine per AI car with
 * the same model, driven by the AI's own rpm/throttle/g values and a shadow
 * thermal; the timer (optionally divided) is its life clock, so the same
 * onset rule works unchanged. Effects on AI (ONLY these): the car's torque
 * table power column is scaled like the game does (or the maxSpeed fallback);
 * a model terminal writes the timer to 0.001 so the GAME fires its own AI
 * failure (its own smoke and radio). No HUD/radio from the mod. Works in a
 * noPlayer session (the player model stays idle; this pump runs before the
 * player-car-valid gate). Default OFF until the owner tests it. */
#define AI_VEH_STRIDE    0x48C0u
#define AI_OFF_TIMER     0x3D70u   /* float s (FLT_MAX / >1e30 = no failure armed) */
#define AI_OFF_RPM       0x39A0u   /* float rad/s */
#define AI_OFF_REVLIM    0x39A4u   /* float rad/s */
#define AI_OFF_TQ_PTR    0x39B0u   /* uint32 ptr to float pairs { coast, power-coast } */
#define AI_OFF_TQ_COUNT  0x39BCu   /* int entries */
#define AI_OFF_THROTTLE  0x357Cu   /* float 0..1 (FACT) */
#define AI_OFF_G_LAT     0x354Cu   /* float m/s2 (FACT) */
#define AI_OFF_G_LONG    0x3554u   /* float m/s2, forward negative (FACT) */
#define AI_OFF_SPEED     0x4440u   /* float m/s */
#define AI_OFF_REASON    0x4068u   /* int ReasonCode */
#define AI_OFF_FINISH    0x4064u   /* int FinishState */
#define AI_OFF_MAXSPEED  0x4538u   /* float m/s, 99999 default */
#define AI_MAX_ENTRIES   200       /* torque-table range check (E9a rule) */

/* B3 (ADDENDUM 14, spec section 2 lever table): the AI symptom levers.
 * FACT unless noted: rev cap triple 0x39A4 + 0x3A18 + the downshift array
 * 0x3A1C..0x3A38 (plan section 8/10b: writing 0x39A4 alone sticks 1st gear,
 * the upshift rpm alone does not hold -> the triple is written together and
 * restored together); idle 0x39A8 (Option A ending, plan section 8); banks
 * 0x3D54/58/5C (bank1 - white / + grey, bank3 0.5 black / 1.0 fire); AI world
 * position 0x34E0/E4/E8 (live-verified moving for AI, plan section 9);
 * PassingNorm 0x44CC / TrackAggression 0x44F8 (reaction plan section 2);
 * RaceAbility 0x44C4 / Crash 0x44D0 (.rcd traits, reaction plan 3.1). */
#define AI_OFF_REV_UPSHIFT 0x3A18u /* float rad/s, AI upshift threshold */
#define AI_OFF_DOWNSHIFT   0x3A1Cu /* float[8] downshift thresholds */
#define AI_OFF_IDLE        0x39A8u /* float rad/s idle clamp */
#define AI_OFF_BANK1       0x3D54u /* float white(-)/grey(+) smoke */
#define AI_OFF_BANK2       0x3D58u /* float grey smoke */
#define AI_OFF_BANK3       0x3D5Cu /* float 0.5 black / 1.0 fire */
/* AI_OFF_POS_X/Y/Z + AI_VEH_STRIDE are defined above (blue-smoke queue) */
#define AI_OFF_PASSING     0x44CCu /* float PassingNorm (file + our lever) */
#define AI_OFF_AGGRESSION  0x44F8u /* float TrackAggression */
#define AI_OFF_RACEABILITY 0x44C4u /* float .rcd race ability */
#define AI_OFF_CRASH       0x44D0u /* float .rcd crash tendency */
#define AI_OFF_NAME        0x4620u /* char* driver name */
#define AI_DOWN_N         8
#define AI_REV_CAP_MIN_FRAC 0.55   /* never cap below this (safety floor) */
#define AI_AIR_BLUE_PARTS_S 12.0   /* AI blue particles/s at density 1 (budget:
                                    * <= 2x the player rate; emit_bluesmoke_burst
                                    * halves the player rate at 30/s max) */

/* P1/P3 (ADDENDUM 15, docs/AI_PULLOFF_SYSTEM_SPEC_2026-10-02.md section 7):
 * the one-writer hook. AIMainUpdate 0x424E70 is called from
 * MainGameTickUpdate 0x42C690 for every car (game thread, ecx = vehicle);
 * the detour runs AFTER the original and overwrites the command + actuator
 * pairs for a slot in pull-off/ASIDE mode, so physics consumes only our
 * value (spec 7.1). Prologue read with the SDK exe-disassemble from the
 * installed F1 Challenge Online.exe (SHA-256 1A70E5ED..., 2026-10-02):
 *   83 EC 1C   sub  esp,0x1c
 *   53         push ebx
 *   55         push ebp
 *   56         push esi        <- stolen 6 bytes
 *   8B F1      mov  esi,ecx    <- resume here
 * The command->actuator copies at the end of AIMainUpdate
 * (functions/AIMainUpdate_00424e70.c lines 161-165): steer 0x442C->0x3580,
 * brake 0x4438->0x3584, throttle 0x4434->0x357C (the external scripts had
 * brake/throttle swapped - spec 7.1 correction). Mode 4 = native park
 * (finding ai-mode-4-native-park-2026-10-02). */
#define AI_PULLOFF_SITE      0x00424E70u
#define AI_PULLOFF_RESUME    0x00424E76u
#define AI_PULLOFF_STOLEN    6
#define AI_OFF_STEER_CMD     0x442Cu
#define AI_OFF_BRAKE_CMD     0x4438u
#define AI_OFF_THROTTLE_CMD  0x4434u
#define AI_OFF_STEER_ACT     0x3580u
#define AI_OFF_BRAKE_ACT     0x3584u
#define AI_OFF_THROTTLE_ACT  0x357Cu
#define AI_OFF_MODE          0x4510u
#define AI_OFF_YAW           0x34F0u   /* float rad; fwd = (-sin, -cos)      */
#define AI_OFF_PIT_STATUS    0x3450u   /* int pittingStatus (>=1 = pit lane) */
#define AI_OFF_WP_CUR        0x4354u   /* uint32 current waypoint pointer    */
#define AI_ACTIVE_PTR_ARRAY  0x007C54F0u /* MainGameTickUpdate vehicle array */
#define AI_ACTIVE_COUNT      0x005C329Cu /* VehicleCount (finding
                                          * native-sc-sim-bound-is-vehiclecount) */
#define AI_PO_WP_NEXT        0x110u    /* waypoint next pointer              */
#define AI_PO_LOOKAHEAD_S    30.0      /* min scan distance, spec section 6  */

/* game-thread copy of the worker's step output (seqlock; the worker writes
 * odd -> fields -> even, the hook reads a stable even copy). */
typedef struct {
    volatile int   seq;
    volatile int   apply;
    volatile int   steer_zero;
    volatile float steer_add;
    volatile int   steer_abs_set;
    volatile float steer_abs;
    volatile int   brake_set;
    volatile float brake;
    volatile float brake_cap;
    volatile int   throttle_zero;
} MfPoShare;

typedef struct {
    MfEngine eng;
    MfCar    car;
    MfOut    out;
    int      car_valid;
    double   car_rev_snapshot;
    int      car_thermal_used; /* J3: per-car H5 thermal data applied */
    int      thermal_logged;   /* J3: "car thermal from <engine.ini>" logged */
    int      shadow_logged;    /* P2: hazard shadow fire logged */
    /* item 2 shadow thermal */
    double   oil_k, water_k;
    int      thermal_init;
    double   speed_prev;
    /* item 3 power lever */
    int      tbl_written;
    int      tbl_shared;       /* E9g-g item 4: table already held by another
                                * slot - power lever skipped, log once */
    unsigned tbl_gen;          /* E9-H1c: session generation of the save */
    float   *tbl_ptr;
    int      tbl_count;
    float    tbl_saved[AI_MAX_ENTRIES];
    double   v_ref;            /* fallback lever: rolling max speed (~90 s) */
    double   v_ref_t;
    double   v_last;           /* fallback lever: our last written maxSpeed */
    /* bookkeeping */
    int      done;             /* retired or terminal written: stop, no writes */
    int      terminal_written;
    int      onset_logged;
    double   onset_t;
    /* native-law mode */
    MfAiLaw  law;
    int      law_logged;
    double   accum_dt;         /* round-robin accumulator */
    /* Q1 (ADDENDUM 3): shadow life clock on the per-car game drain law,
     * normalised to the field mean (mf_ai_wear_rate). ai_life starts at
     * timer/divisor on the first valid sample; ai_rate is the current rate
     * (published for the panel). */
    double   ai_life;
    double   ai_rate;
    double   drain_law;        /* Q1: mf_game_drain at the last sample */
    int      ai_life_valid;
    int      drain_running;    /* Q1: rpm > 1 at the last sample */
    unsigned ai_gen;           /* Q2: generation of the ai_life seed */
    int      timer_owned;      /* Q2: "timer owned" logged */
    /* Q7c (ADDENDUM 7, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): scripted
     * vanilla drops (RaceEvents mode 3 / engine re-arm). timer_seen = the raw
     * game timer at the previous pass; timer_written = the last value WE wrote
     * (0 = never); scripted = a scripted drop was honoured: ai_life took the
     * new game value (no divisor) and Q2 ownership stops for the slot. */
    double   timer_seen;
    int      timer_seen_valid;
    double   timer_written;
    double   ai_div;           /* audit MF-02: divisor used to seed ai_life */
    int      scripted;
    /* B (ADDENDUM 14): AI symptoms + driver reactions. traits/reaction are
     * pure (mf_ai_react.h); lsave is the ownership snapshot of every game
     * lever this slot has written (restore completeness, B7). */
    AiTraits  traits;
    AiReaction react;
    MfRng     react_rng;
    MfAiLeverSave lsave;
    unsigned lsave_gen;        /* audit MF-03: session generation of lsave */
    int      react_traits_logged;
    int      react_last_level;
    int      react_last_noticed;
    int      last_mistake_count;
    int      last_mistake_kind;
    double   sym_blue;         /* current blue density (queue pacing) */
    double   blue_acc;
    double   flame_left_s;     /* bank3 1.0 spit remaining */
    double   flame_cool_s;
    double   sound_cool_s;
    double   rev_our_cap;      /* last rev limit we wrote (ownership) */
    double   bank_our[3];      /* last banks we wrote (ownership) */
    double   maxspeed_our;     /* last maxSpeed we wrote (ownership) */
    double   limp_since;       /* 2026-10-07: session time R2+ began, 0 = not limping */
    double   crawl_since;      /* M2 1.0.3: session time below the field fraction, 0 = not */
    double   crawl_ema;        /* M2b: per-car speed EMA (0 = not valid) */
    int      crawl_onset_seen; /* M2b: the current onset reset the EMA */
    int      oil_step_logged;  /* N1 1.0.3: last 10 % oil-lost step logged */
    int      po_empty_logged;  /* 2026-10-07: "chain scan empty" once per episode */
    int      aside_spent;      /* 2026-10-07: ASIDE ended at pit/finish - not restarted */
    double   passing_our;      /* last PassingNorm we wrote (ownership) */
    double   aggr_our;         /* last TrackAggression we wrote (ownership) */
    int      levers_pending;   /* audit COMP-01b: a restore is waiting for our value */
    int      end_done;         /* Option A ending written */
    int      end_kind;         /* MF_AI_END_* of that ending */
    double   top_cut_rads;     /* fuel-pressure hard top cut (rows >= 0) */
    int      top_cut_active;
    double   mis_left_s;       /* active misfire cut remaining */
    /* P1-P7 (ADDENDUM 15): pull-off / off-the-line state. The pure machine
     * (mf_ai_pulloff.h) runs on the worker; po_share carries the step output
     * to the game-thread detour. */
    MfPulloff pulloff;
    MfPoShare po_share;
    int      po_prev_valid;
    double   po_prev_x, po_prev_z, po_prev_t;
    double   po_speed;         /* physics speed from position deltas */
    unsigned po_wp_prev;       /* waypoint ptr of the last lateral sample */
    double   po_bank_fade_t;   /* P5: blow-up fire fade deadline (0 = none) */
    int      po_bank_faded;
    double   ai_blow_fx_until; /* N3e: queued AI fire/smoke window end (0 = none) */
    double   ai_blow_fx_next;  /* N3e: next queued burst time */
    int      po_log_state;     /* last state logged (edge-triggered) */
    int      po_aside_logged;
    int      po_released_logged;
    int      po_verify_bad;    /* per-slot: hook layout check failed */
    /* review fix R1 (game thread only): AIMainUpdate rewrites 0x442C only on
     * its LABEL_86 path; on early-return/park ticks the hook must add to the
     * AI's last own steer, not to its own previous output. */
    int      po_hk_wrote;      /* 1 = po_hk_last is our last 0x442C write */
    float    po_hk_last;       /* value the hook last wrote to 0x442C */
    float    po_hk_base;       /* AI's own steer under that write */
    /* s20 diag (2026-10-02): po_apply outcome counters for an ACTIVE pull-off,
     * game thread increments, worker logs + resets once per second. */
    volatile LONG po_dg[8];
    double   po_dg_t;
#ifdef MF_DEV_TOOLS
    /* B3 (pre-ship batch 2026-10-08): dev-only forced-failure request. The
     * panel writes the AiForceCtl mapping only; the game-thread hook
     * (ai_force_apply) files the request here and the worker's ai_step_slot
     * consumes it. force_seq is cross-thread (volatile + Interlocked),
     * force_seq_seen is worker-side. Release build: fields compiled out, the
     * slot layout and published offsets are unchanged. */
    volatile LONG force_seq;
    int           force_fault;
    int           force_sev;
    int           force_budget_s;
    LONG          force_seq_seen;
#endif
} MfAiSlot;

/* s20 diag reasons (index into MfAiSlot.po_dg) */
enum { PO_DG_CTL, PO_DG_FROZEN, PO_DG_SEQ, PO_DG_NOAPPLY, PO_DG_CAPPED,
       PO_DG_SET, PO_DG_PASS, PO_DG_N };
static const char *const g_po_dg_name[PO_DG_N] = {
    "ctl-off", "session/frozen", "seq-skip", "apply0", "capped", "brake-set",
    "under-cap" };


static MfAiSlot g_ai[MF_AI_MAX];
static uint32_t g_ai_seed = 0;         /* H4a: per-session seed basis */
static uint32_t g_ai_slots_seed = 0;   /* J4: basis the slots were built from */
static uint32_t g_ai_seed_seq = 0;     /* J4: draw counter (same-tick mix) */
static int    g_ai_on = 0;
static int    g_ai_rr = 0;             /* performance guard: round-robin */
static int    g_ai_rr_slot = 1;
static int    g_ai_proxy_logged = 0;

/* Batch C (COMP-02): MF's "car limited" record in its OWN mapping (ABI 2,
 * separate from the frozen SC/RF block). MF is the only writer; it marks the
 * exact vehicle identity of every AI car it has faulted (a faulted car is being
 * degraded, so the AI Toolkit must not override it). The Toolkit only reads.
 * Opened lazily on the worker; if the block cannot be opened the writes are
 * no-ops. */
static F1CMfLimited *g_mf_rc = NULL;
static void mf_rc_mark(int slot, int limited)
{
    uintptr_t v;
    if (!g_mf_rc) return;
    v = (uintptr_t)(MF_AIV(slot));
    if (limited) f1c_rc_mf_set(g_mf_rc, v);
    else         f1c_rc_mf_clear(g_mf_rc, v);
}
static int    g_ai_stat_onset[MF_FAULT_COUNT];
static int    g_ai_stat_term[MF_FAULT_COUNT];
static double g_ai_stat_onset_s = 0.0;
static double g_ai_stat_term_s = 0.0;
static double g_ai_stat_lead_s = 0.0;
static double g_ai_stat_phase_s = 0.0;
static int    g_ai_stat_onset_n = 0;
static int    g_ai_stat_term_n = 0;
static int    g_ai_slot_logged[MF_AI_MAX];
static int    g_ai_early_off_logged = 0;   /* Q7b: once per session */

/* Q1 (ADDENDUM 3): field mean of drain_law over all running AI slots (EMA,
 * tau 600 s game time), valid after 60 s of samples. One pump pass updates
 * it; the next pass's wear rates use it. */
#ifndef MF_AI_RATE_SPREAD
#define MF_AI_RATE_SPREAD 0   /* 0 = every AI clock drains 1.0 s/s (shipped) */
#endif
static double g_ai_field_mean = 0.0;
static double g_ai_field_age_s = 0.0;
static int    g_ai_field_valid = 0;
static int    g_ai_field_init = 0;

/* M2 (2026-10-08 owner retest: every forced fault crawled at ~68 km/h): the
 * field MEDIAN speed of the cars already read, one sample per pump pass.
 * Population excludes pit lane, pull-off/aside, retired and the safety car
 * (done), matching the spec; the crawler guard in ai_step_slot compares a
 * faulted car against it. */
static double ai_off_f(DWORD slot, DWORD off);   /* defined below */
static int    ai_off_i(DWORD slot, DWORD off);
static double g_ai_field_speed = 0.0;      /* median of the EMAs; 0 = invalid */
static int    g_ai_field_speed_n = 0;

/* M2b (Claude review): per-car EMA of speed (tau MF_AI_CRAWL_EMA_S), the
 * field median taken over those EMAs - a genuinely slow sector must not
 * retire a nursing car against a median sampled on straights. EMA reset:
 * pit lane, retired/unloaded, and on every onset (ai_step_slot). */
static void ai_field_speed_update(double dt)
{
    double v[MF_AI_MAX];
    int i, n = 0;

    for (i = 1; i < MF_AI_MAX; i++) {
        MfAiSlot *a = &g_ai[i];
        double s, rl;
        if (a->done || ai_off_i((DWORD)i, AI_OFF_PIT_STATUS) >= 1) {
            a->crawl_ema = 0.0;            /* pit: exit restarts the average */
            continue;
        }
        rl = ai_off_f((DWORD)i, AI_OFF_REVLIM);
        s  = ai_off_f((DWORD)i, AI_OFF_SPEED);
        if (!(rl > 100.0 && rl < 10000.0) || !(s > 0.0) || !(s < 150.0)) {
            a->crawl_ema = 0.0;
            continue;
        }
        a->crawl_ema = mf_ai_crawl_ema(a->crawl_ema, s, dt);
        if (a->pulloff.active) continue;   /* median population skips pull-off */
        v[n++] = a->crawl_ema;
    }
    g_ai_field_speed_n = n;
    g_ai_field_speed = 0.0;
    if (n < 3) return;                     /* too few cars to trust */
    for (i = 1; i < n; i++) {              /* insertion sort, n <= 23 */
        double x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    g_ai_field_speed = (n & 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

static void ai_field_mean_update(double dt)
{
    double sum = 0.0, alpha, mean;
    int i, n = 0;

    if (!(dt > 0.0)) return;
    for (i = 1; i < MF_AI_MAX; i++) {
        const MfAiSlot *a = &g_ai[i];
        if (a->car_valid && !a->done && a->drain_running
            && isfinite(a->drain_law) && a->drain_law > 0.0) {
            sum += a->drain_law;
            n++;
        }
    }
    if (n <= 0) return;
    mean = sum / (double)n;
    alpha = 1.0 - exp(-dt / 600.0);
    if (!g_ai_field_init) {
        g_ai_field_mean = mean;
        g_ai_field_init = 1;
    } else {
        g_ai_field_mean += (mean - g_ai_field_mean) * alpha;
    }
    g_ai_field_age_s += dt;
    if (g_ai_field_age_s >= 60.0) g_ai_field_valid = 1;
}

static double ai_off_f(DWORD slot, DWORD off)
{
    return (double)(*(volatile float*)(MF_AIV(slot) + off));
}

static int ai_off_i(DWORD slot, DWORD off)
{
    return *(volatile int*)(MF_AIV(slot) + off);
}

/* the car's peak-torque rpm from its own table (ptr veh+0x39B0, count
 * veh+0x39BC; the table has no torque-zero point, so tq_zero stays the
 * model's fallback and the E9a hard over-rev death never fires for AI) */
static double ai_tq_peak_rads(DWORD slot)
{
    uint32_t tp = *(volatile uint32_t*)(MF_AIV(slot) + AI_OFF_TQ_PTR);
    int cnt = *(volatile int*)(MF_AIV(slot) + AI_OFF_TQ_COUNT);
    const float *tq = (const float*)(uintptr_t)tp;
    int i, peak_i = -1;
    double peak = 0.0;

    if (!tq || cnt < 10 || cnt > AI_MAX_ENTRIES
        || IsBadReadPtr(tq, (UINT_PTR)cnt * 2u * sizeof(float)))
        return 0.0;
    for (i = 0; i < cnt; i++) {
        double p = (double)tq[2 * i] + (double)tq[2 * i + 1];
        if (peak_i < 0 || p > peak) { peak = p; peak_i = i; }
    }
    return peak_i < 0 ? 0.0 : (double)peak_i * MF_TQ_STEP_RADS;
}

/* put the saved power column back (never after the session-end jump: the game
 * zeroes the column itself then and the memory goes away). E9g-g item 5:
 * write back only if the slot's CURRENT table pointer and count still equal
 * the saved ones - never write a stale pointer. */
static void ai_table_restore(MfAiSlot *a, int slot)
{
    if (!a->tbl_written) return;
    /* H1c: a save from another session (or a session that is over) is never
     * written back into the game's tables - drop the record and log once. */
    if (a->tbl_gen != g_session_gen || SESSION_OVER()) {
        logf_("AI: slot %d table restore dropped (%s)", slot,
              a->tbl_gen != g_session_gen ? "stale session generation"
                                          : "session over");
    } else {
        uint32_t cur_tp = *(volatile uint32_t*)(MF_AIV(slot)
                           + AI_OFF_TQ_PTR);
        int cur_cnt = *(volatile int*)(MF_AIV(slot)
                       + AI_OFF_TQ_COUNT);
        float *cur = (float*)(uintptr_t)cur_tp;
        if (cur && cur == a->tbl_ptr && cur_cnt == a->tbl_count
            && a->tbl_count > 0
            && !IsBadReadPtr(a->tbl_ptr, (UINT_PTR)a->tbl_count * 2u * sizeof(float))) {
            int i;
            for (i = 0; i < a->tbl_count; i++)
                a->tbl_ptr[2 * i + 1] = a->tbl_saved[i];
        } else {
            logf_("AI: slot %d table moved - restore skipped", slot);
        }
    }
    a->tbl_written = 0;
    a->tbl_gen = 0;
    a->tbl_ptr = NULL;
    a->tbl_count = 0;
}

static void ai_slot_reset(MfAiSlot *a, int slot)
{
    a->limp_since = 0.0;
    a->crawl_since = 0.0;
    a->crawl_ema = 0.0;
    a->crawl_onset_seen = 0;
    a->oil_step_logged = 0;
    a->aside_spent = 0;
    a->po_empty_logged = 0;
    a->tbl_written = 0;
    a->tbl_shared = 0;
    a->tbl_gen = 0;
    a->tbl_ptr = NULL;
    a->tbl_count = 0;
    a->car_valid = 0;
    a->car_thermal_used = 0;
    a->thermal_logged = 0;
    a->shadow_logged = 0;
    a->thermal_init = 0;
    a->speed_prev = 0.0;
    a->v_ref = 0.0;
    a->v_ref_t = 0.0;
    a->v_last = 0.0;
    a->done = 0;
    a->terminal_written = 0;
    a->onset_logged = 0;
    a->onset_t = 0.0;
    a->law.init = 0;
    a->law.life_s = 0.0;
    a->law_logged = 0;
    a->accum_dt = 0.0;
    a->ai_life = 0.0;
    a->ai_rate = 1.0;
    a->drain_law = 0.0;
    a->ai_life_valid = 0;
    a->drain_running = 0;
    a->ai_gen = 0;
    a->timer_owned = 0;
    a->timer_seen = 0.0;
    a->timer_seen_valid = 0;
    a->timer_written = 0.0;
    a->ai_div = 1.0;
    a->scripted = 0;
    /* B (ADDENDUM 14) */
    memset(&a->traits, 0, sizeof a->traits);
    mf_ai_react_init(&a->react);
    a->react_rng.s = mf_splitmix32(g_ai_seed ^ (uint32_t)slot ^ 0xB4A7E1u)
                     | 1u;
    mf_ai_lever_save_clear(&a->lsave);
    a->lsave_gen = 0;
    a->levers_pending = 0;
    a->react_traits_logged = 0;
    a->react_last_level = -1;
    a->react_last_noticed = 0;
    a->last_mistake_count = 0;
    a->last_mistake_kind = 0;
    a->sym_blue = 0.0;
    a->blue_acc = 0.0;
    a->flame_left_s = 0.0;
    a->flame_cool_s = 0.0;
    a->sound_cool_s = 0.0;
    a->rev_our_cap = 0.0;
    a->bank_our[0] = a->bank_our[1] = a->bank_our[2] = 0.0;
    a->maxspeed_our = 0.0;
    a->passing_our = 0.0;
    a->aggr_our = 0.0;
    a->end_done = 0;
    a->end_kind = MF_AI_END_NONE;
    a->top_cut_rads = -1.0;
    a->top_cut_active = 0;
    a->mis_left_s = 0.0;
#ifdef MF_DEV_TOOLS
    a->force_seq_seen = a->force_seq;   /* B3 dev: no forced request crosses a session */
#endif
    /* ADDENDUM 15: pull-off state */
    mf_po_init(&a->pulloff);
    memset((void*)&a->po_share, 0, sizeof a->po_share);
    a->po_prev_valid = 0;
    a->po_prev_x = a->po_prev_z = a->po_prev_t = 0.0;
    a->po_speed = 0.0;
    a->po_wp_prev = 0;
    a->po_bank_fade_t = 0.0;
    a->po_bank_faded = 0;
    a->ai_blow_fx_until = 0.0;
    a->ai_blow_fx_next = 0.0;
    a->po_log_state = -1;
    a->po_aside_logged = 0;
    a->po_released_logged = 0;
    a->po_verify_bad = 0;
    a->po_hk_wrote = 0;
    a->po_hk_last = 0.0f;
    a->po_hk_base = 0.0f;
    /* E9p item 8 (team quality): each AI engine rolls the same triangular
     * quality as the player, per slot. No proven team-name address exists in
     * the SDK record, so there is no per-team mode shift (owner-prompt rule:
     * do not guess an address); the pump logs the decision once per enable.
     * H4a: the seed is splitmix32(session seed ^ slot) - the plain XOR seed
     * made adjacent slots' first draw nearly linear (live q 1.043, 1.040,
     * 1.037 ... finding ai-quality-q-linear-in-slot). */
    mf_engine_init(&a->eng, mf_splitmix32(g_ai_seed ^ (uint32_t)slot));
    /* Q4 (ADDENDUM 3): the AI hazard eta is its own constant - the player
     * keeps MF_HAZARD_ETA from mf_engine_init (Q4c player ratio 1.000).
     * 2026-10-09 owner: MF_HAZARD_ETA_AI now equals MF_HAZARD_ETA. */
    a->eng.hz_eta = MF_HAZARD_ETA_AI;
}

static void ai_reseed(void)
{
    /* J4: the ONE place that draws the per-session basis. GetTickCount alone
     * is not enough (15.6 ms grid: two re-seeds inside one tick give the same
     * value), so mix the draw sequence and the session generation. The log
     * line is the live evidence that each session/invalidation got a new
     * basis. */
    g_ai_seed_seq++;
    g_ai_seed = mf_splitmix32(GetTickCount() * 2654435761u
                              ^ (g_ai_seed_seq * 0x9E3779B9u)
                              ^ g_session_gen);
    logf_("AI: session seed 0x%08X (seq %u)", g_ai_seed, g_ai_seed_seq);
}

static void ai_session_reset(void)
{
    int i;
    mfc_session_reset();            /* CO-OP: index map per session (no-op in single-player) */
    ai_reseed();                    /* J4: fresh basis per session */
    for (i = 0; i < MF_AI_MAX; i++) {
        ai_slot_reset(&g_ai[i], i);
        g_ai_slot_logged[i] = 0;
    }
    g_ai_slots_seed = g_ai_seed;
    memset(g_ai_stat_onset, 0, sizeof g_ai_stat_onset);
    memset(g_ai_stat_term, 0, sizeof g_ai_stat_term);
    g_ai_stat_onset_s = g_ai_stat_term_s = 0.0;
    g_ai_stat_lead_s = g_ai_stat_phase_s = 0.0;
    g_ai_stat_onset_n = g_ai_stat_term_n = 0;
    g_ai_proxy_logged = 0;
    g_ai_early_off_logged = 0;  /* Q7b: the OFF line is once per session */
    g_ai_rr = 0;
    g_ai_rr_slot = 1;
    g_ai_field_mean = 0.0;      /* Q1: new field, new mean */
    g_ai_field_age_s = 0.0;
    g_ai_field_valid = 0;
    g_ai_field_init = 0;
}

static void ai_session_stats(void)
{
    int f;
    if (!g_ai_stat_onset_n && !g_ai_stat_term_n) return;
    logf_("AI STATS: onsets %d (mean t %.0f s, mean lead %.0f s), terminals %d "
          "(mean t %.0f s), mean onset->terminal %.0f s",
          g_ai_stat_onset_n,
          g_ai_stat_onset_n ? g_ai_stat_onset_s / g_ai_stat_onset_n : 0.0,
          g_ai_stat_onset_n ? g_ai_stat_lead_s / g_ai_stat_onset_n : 0.0,
          g_ai_stat_term_n,
          g_ai_stat_term_n ? g_ai_stat_term_s / g_ai_stat_term_n : 0.0,
          g_ai_stat_term_n ? g_ai_stat_phase_s / g_ai_stat_term_n : 0.0);
    for (f = 1; f < MF_FAULT_COUNT; f++)
        if (g_ai_stat_onset[f] || g_ai_stat_term[f])
            logf_("AI STATS: fault %d: onsets %d, terminals %d",
                  f, g_ai_stat_onset[f], g_ai_stat_term[f]);
}

static void ai_levers_restore(MfAiSlot *a, int slot);   /* B3, defined below */
static void po_share_clear(MfAiSlot *a);                /* P3, defined below */
static void ai_pulloff_pump(double dt);                 /* P3, defined below */
static double po_notice_delay(const MfAiSlot *a);       /* P5, defined below */
static int g_po_installed;                              /* P1/P3, defined below */
static int ai_pulloff_install(void);                     /* 2026-10-06: late install, defined below */
static int g_po_verify;                                 /* P1/P3, defined below */

/* 2026-10-07 audit MF-02 (owner D2): give the game its own clock back */
static void ai_timer_release(MfAiSlot *a, int slot)
{
    double v;
    if (!a->timer_owned || a->done || a->terminal_written || a->scripted) return;
    if (a->ai_gen != g_session_gen || SESSION_OVER()) return;
    if (!mf_ai_timer_release_value((double)ai_off_f((DWORD)slot, AI_OFF_TIMER),
                                   a->timer_written, a->ai_life, a->ai_div, &v))
        return;
    *(volatile float*)(MF_AIV(slot) + AI_OFF_TIMER)
        = (float)v;
    logf_("AI: slot %d timer released (%.0f s)", slot, v);
    a->timer_owned = 0;
    a->timer_written = 0.0;
}

static void ai_shutdown(void)
{
    int i;
    for (i = 0; i < MF_AI_MAX; i++) {
        ai_timer_release(&g_ai[i], i);
        ai_table_restore(&g_ai[i], i);
        ai_levers_restore(&g_ai[i], i);   /* B3: rev/banks/passing/aggr/max */
        mf_po_stop(&g_ai[i].pulloff);     /* P3: no pull-off across sessions */
        po_share_clear(&g_ai[i]);
    }
    g_ai_on = 0;
}

/* H3b/R07: THE one invalidation path. Runs on a session change, a clock
 * rollback and a non-race stint end - and always BEFORE the SESSION_OVER
 * return, so a leave/re-enter between worker samples cannot carry state.
 * Releases every owned game value under the H1c generation rules:
 *   drop = 0: records from this generation are WRITTEN back (stock restore),
 *             then the generation bumps so anything still held is stale;
 *   drop = 1: the generation bumps FIRST, so every release DROPS its record
 *             without writing (session over / clock rollback).
 * Then it clears g_model_live, the model, the dead latch, save mode and the
 * warning timers. The next live sample re-initialises through the normal
 * !g_model_live branch, so a stint end behaves as a fresh engine (D1). */
static void engine_invalidate(const char *why, int drop)
{
    mfu_flush(why);       /* 2026-10-06: usage summary of the session that ends */
    if (drop) session_gen_bump();
    ai_shutdown();
    f1c_rc_mf_clear_all(g_mf_rc);   /* Batch C: no MF-limited car survives a session */
    ai_reseed();          /* J4: every invalidation draws a new basis (spec) */
    memset(g_blow_fx_start, 0, sizeof g_blow_fx_start);   /* N3e: no stale owner */
    fx_restore();
    cooling_restore();
    rev_band_restore("REV BAND");
    model_levers_release();
    if (!drop) session_gen_bump();
    g_model_live = 0;
    model_reset((uint32_t)GetTickCount() | 1u);
    engine_dead_clear(why);
    save_reset(why);
    warn_reset();
    retire_commentary_reset();
    hud_clear();                      /* 2026-10-01: arbiter + radio queue */
    mfr_clear(&g_radio);
    InterlockedExchange(&g_vn_line, -1);   /* audit MF-08: no token across a reset */
    g_stop_at = 0;
    logf_("ENGINE: state invalidated (%s, gen %u)", why, g_session_gen);
}

/* 2026-10-07 audit MF-02 (owner D1): a garage visit outside the race
 * repairs the PLAYER's engine only. Unlike engine_invalidate it keeps
 * g_model_live, the session generation and every AI slot (faults,
 * reactions, lever ownership, timers) untouched. */
static void engine_player_reset(const char *why)
{
    fx_restore();
    cooling_restore();
    rev_band_restore("REV BAND");
    g_power_mult = 1.0f;
    g_loss_top = 0.0f;
    g_loss_all = 0.0f;
    g_loss_load = 0.0f;
    g_v_throttle = 0.0f;
    g_misfire_depth = 0.0f;
    g_dmg_log_t = 0.0;
    g_shadow_logged = 0;
    model_levers_release();
    model_reset((uint32_t)GetTickCount() | 1u);
    engine_dead_clear(why);
    save_reset(why);
    warn_reset();
    retire_commentary_reset();
    hud_clear();
    mfr_clear(&g_radio);
    InterlockedExchange(&g_vn_line, -1);   /* audit MF-08: no token across a reset */
    g_stop_at = 0;
    logf_("ENGINE: player engine reset (%s, gen %u kept, AI untouched)",
          why, g_session_gen);
}

/* H4b/D4: the slot's table is shared when ANY other loaded AI slot currently
 * points at the same torque table. Returns the other slot index or -1; reads
 * only the live pointer/count fields of already-car-valid slots. */
static int ai_table_shared_with_other(int slot, const float *tbl, int cnt)
{
    int j;
    if (!tbl || cnt < 10 || cnt > AI_MAX_ENTRIES) return -1;
    for (j = 1; j < MF_AI_MAX; j++) {
        uint32_t tp;
        int c;
        if (j == slot || !g_ai[j].car_valid) continue;
        tp = *(volatile uint32_t*)(MF_AIV(j)
                                   + AI_OFF_TQ_PTR);
        c  = *(volatile int*)(MF_AIV(j)
                              + AI_OFF_TQ_COUNT);
        if ((const float*)(uintptr_t)tp == tbl && c == cnt) return j;
    }
    return -1;
}

/* P1 (ADDENDUM 2): component-damage diagnostics. Player: one line per 60 s of
 * model time; AI: once at onset and once at terminal. Shares are fractions of
 * dmg_total (same fault weights as the onset roll). */
static const char *dmg_fault_name_(int f)
{
    switch (f) {
    case MF_FAULT_VALVE_AIR:     return "valve";
    case MF_FAULT_OIL_PRESSURE:  return "oilp";
    case MF_FAULT_OIL_LEAK:      return "oilleak";
    case MF_FAULT_WATER_LEAK:    return "waterleak";
    case MF_FAULT_RAD_DEBRIS:    return "rad";
    case MF_FAULT_FUEL_PRESSURE: return "fuelp";
    case MF_FAULT_FUEL_LEAK:     return "fuelleak";
    case MF_FAULT_BLOWBY:        return "blowby";
    case MF_FAULT_EXHAUST:       return "exhaust";
    case MF_FAULT_BLOWUP:        return "blowup";
    default:                     return "none";
    }
}

static void dmg_log_(int slot, const MfEngine *e)
{
    double b1 = -1.0, b2 = -1.0, b3 = -1.0, sh;
    int i1 = 1, i2 = 1, i3 = 1, i;

    if (!(e->dmg_total > 0.0)) return;
    for (i = 1; i < MF_FAULT_COUNT; i++) {
        sh = e->dmg[i] / e->dmg_total;
        if (sh > b1) {
            b3 = b2; i3 = i2; b2 = b1; i2 = i1; b1 = sh; i1 = i;
        } else if (sh > b2) {
            b3 = b2; i3 = i2; b2 = sh; i2 = i;
        } else if (sh > b3) {
            b3 = sh; i3 = i;
        }
    }
    if (slot >= 0)
        logf_("AI: slot %d DMG: total=%.0f s top3=%s:%.3f %s:%.3f %s:%.3f",
              slot, e->dmg_total, dmg_fault_name_(i1), b1,
              dmg_fault_name_(i2), b2, dmg_fault_name_(i3), b3);
    else
        logf_("DMG: total=%.0f s top3=%s:%.3f %s:%.3f %s:%.3f",
              e->dmg_total, dmg_fault_name_(i1), b1,
              dmg_fault_name_(i2), b2, dmg_fault_name_(i3), b3);
}

/* J3: per-slot thermal values from the H5-parsed engine.ini (rad/s, K;
 * oil_opt = 0 when OptimumOilTemp= is absent). therm_valid = the values
 * passed the mf_car_derive bounds (checked by reading back its fallback
 * flags). Filled by the H5 parse below, consumed by ai_step_slot. */
static double g_grade_rpm_base[MF_AI_MAX];
static double g_grade_rpm_range[MF_AI_MAX];
static double g_grade_oil_base[MF_AI_MAX];
static double g_grade_oil_range[MF_AI_MAX];
static double g_grade_oil_opt[MF_AI_MAX];
static int    g_grade_therm_valid[MF_AI_MAX];
static char   g_grade_eng_path[MF_AI_MAX][MAX_PATH * 4 + 16];

/* T3 (ADDENDUM 12): one ONSET line for every onset path. Logged as soon as the
 * model reports the onset; also called from the terminal branch so a fault
 * that onset and terminated between two logs can never lose its line. */
static void ai_log_onset_if_needed(MfAiSlot *a, int slot)
{
    double x;
    if (a->onset_logged || !a->eng.onset) return;
    x = a->eng.budget_s > 0.0 ? a->eng.dmg_total / a->eng.budget_s : 0.0;
    a->onset_logged = 1;
    a->onset_t = SESSION_TIME;
    g_ai_stat_onset[a->eng.fault]++;
    g_ai_stat_onset_s += SESSION_TIME;
    g_ai_stat_lead_s += a->eng.lead_s;
    g_ai_stat_onset_n++;
    /* R1c (ADDENDUM 9): same impact wording as the player line. */
    if (a->out.impact_fired && a->out.impact_fault == a->eng.fault) {
        logf_("AI: slot %d ONSET: impact g=%.1f x=%.3f t=%.0f fault=%s "
              "(%d) sev=%d budget=%.0f s", slot, a->out.impact_g, x,
              SESSION_TIME, dmg_fault_name_(a->eng.fault), a->eng.fault,
              a->eng.severity, a->eng.ttf_onset_s);
    } else {
        logf_("AI: slot %d ONSET: %s%s x=%.3f t=%.0f fault=%s (%d) sev=%d "
              "budget=%.0f s",
              slot, a->eng.onset_hazard ? "hazard" : "countdown",
              (!a->eng.onset_hazard && a->eng.emergency_onset)
                  ? " (emergency)" : "",
              x, SESSION_TIME, dmg_fault_name_(a->eng.fault), a->eng.fault,
              a->eng.severity, a->eng.ttf_onset_s);
    }
    dmg_log_(slot, &a->eng);   /* P1: damage mix at onset */
}

/* ---- B3-B6 (ADDENDUM 14, spec sections 2-6): AI levers ----------------
 * Every writer is ownership-checked: the original is saved at the first
 * write, the write is skipped when the current value is not ours, and the
 * restore puts the snapshot back once (session end, fault end, retire).
 * The rev cap is ALWAYS the triple (limit + upshift + downshift array) - a
 * single-field write sticks 1st gear or hunts gears (owner-seen, plan
 * sections 8/10b). */

static void ai_driver_name(int slot, char *out, int cap)
{
    const char *p = (const char*)(uintptr_t)(MF_AIV(slot)
                     + AI_OFF_NAME);
    int i = 0;
    out[0] = '\0';
    if (!p || cap <= 1) return;
    if (IsBadReadPtr(p, 32)) return;
    while (i < cap - 1 && i < 31 && p[i]) {
        unsigned char ch = (unsigned char)p[i];
        if (ch < 32 || ch > 126) break;      /* never log binary garbage */
        out[i] = (char)ch;
        i++;
    }
    out[i] = '\0';
}

/* one of our WAVs next to the DLL, placed at an AI car (B4). Queue-only:
 * the audio pump opens the device on the worker, never in a hook. */
static void ai_play_fx_at(int slot, const char *file, float gain)
{
    char path[MAX_PATH], *slash;
    DWORD len;
    float x, y, z;
    if (SESSION_OVER() || !session_live()) return;
    (void)len; (void)slash;
    if (!mf_sound_path(path, file)) return;
    x = ai_off_f((DWORD)slot, AI_OFF_POS_X);
    y = ai_off_f((DWORD)slot, AI_OFF_POS_Y);
    z = ai_off_f((DWORD)slot, AI_OFF_POS_Z);
    if (!(x == x) || !(y == y) || !(z == z)) return;
    mf_audio_play_pos(path, gain, x, y, z);
    if (g_mfc_latched) mfc_on_sound(slot, 0, path, gain);   /* CO-OP mirror */
}

/* the game's own backfire samples, placed at an AI car (B4).
 * N3 addendum 15: same game-EXE-based path as exhaust_pop. */
static void ai_exhaust_pop_at(int slot)
{
    static const int nums[3] = { 2, 5, 6 };
    static int missing_logged = 0;
    static unsigned missing_gen = 0;
    char path[MAX_PATH];
    int i;
    float x, y, z;
    if (SESSION_OVER() || !session_live()) return;
    game_dir(path, sizeof path);
    if (!path[0]) return;
    i = (int)strlen(path);
    _snprintf(path + i, MAX_PATH - i, "\\Audio\\Vehicle\\backfire_ext_%d.wav",
              nums[rand() % 3]);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES
        && !backfire_fallback(path)) {
        if (missing_gen != g_session_gen) {
            missing_gen = g_session_gen;
            missing_logged = 0;
        }
        if (!missing_logged) {
            missing_logged = 1;
            logf_("AI: backfire sample missing (%s)", path);
        }
        return;
    }
    x = ai_off_f((DWORD)slot, AI_OFF_POS_X);
    y = ai_off_f((DWORD)slot, AI_OFF_POS_Y);
    z = ai_off_f((DWORD)slot, AI_OFF_POS_Z);
    if (!(x == x) || !(y == y) || !(z == z)) return;
    mf_audio_play_pos(path, MF_POP_GAIN, x, y, z);
    if (g_mfc_latched) mfc_on_sound(slot, 1, path, MF_POP_GAIN);   /* CO-OP mirror */
}

/* ---- rev-cap triple ---- */
static void ai_rev_restore(MfAiSlot *a, int slot)
{
    DWORD base = MF_AIV(slot);
    int i;
    if (!a->lsave.rev_valid) return;
    /* ownership: only write when the limit is still OUR value (or was reset
     * to the saved value); a game rewrite drops the record instead. */
    if (fabs(ai_off_f((DWORD)slot, AI_OFF_REVLIM) - a->rev_our_cap) < 0.5) {
        *(volatile float*)(base + AI_OFF_REVLIM) = a->lsave.rev_limit;
        *(volatile float*)(base + AI_OFF_REV_UPSHIFT) = a->lsave.rev_upshift;
        for (i = 0; i < AI_DOWN_N; i++)
            *(volatile float*)(base + AI_OFF_DOWNSHIFT + 4u * (DWORD)i)
                = a->lsave.rev_down[i];
        logf_("AI: slot %d rev band restored", slot);
    } else {
        logf_("AI: slot %d rev band restore dropped (game moved it)", slot);
    }
    a->lsave.rev_valid = 0;
    a->rev_our_cap = 0.0;
}

/* audit MF-03: stamp the generation when the first lever is saved */
static void ai_lsave_stamp(MfAiSlot *a)
{
    if (mf_ai_lever_save_clean(&a->lsave)) a->lsave_gen = g_session_gen;
}

static void ai_rev_apply(MfAiSlot *a, int slot, double cap)
{
    DWORD base = MF_AIV(slot);
    float lim = ai_off_f((DWORD)slot, AI_OFF_REVLIM);
    int i, first_save = 0;
    if (!(lim > 100.0 && lim < 10000.0)) return;
    if (!(cap > 0.0)) return;
    if (cap > (double)lim) cap = (double)lim;
    if (!a->lsave.rev_valid) {
        first_save = 1;
        ai_lsave_stamp(a);
        a->lsave.rev_valid = 1;
        a->lsave.rev_limit = lim;
        a->lsave.rev_upshift = ai_off_f((DWORD)slot, AI_OFF_REV_UPSHIFT);
        for (i = 0; i < AI_DOWN_N; i++)
            a->lsave.rev_down[i] = ai_off_f((DWORD)slot,
                                            AI_OFF_DOWNSHIFT + 4u * (DWORD)i);
        a->lsave.rev_down_n = AI_DOWN_N;
    } else {
        /* the base for scaling is the snapshot, not the already-capped live
         * value: recompute from the original every time */
        lim = a->lsave.rev_limit;
    }
    if (fabs(cap - a->rev_our_cap) < 0.25 && a->rev_our_cap > 0.0) return;
    {
        /* M3: the effective shift points come from the pure helper (scaled
         * from the SNAPSHOT, never the capped live value) so the car keeps
         * the right gear and can always downshift under the cap. */
        double idle = ai_off_f((DWORD)slot, AI_OFF_IDLE);
        double dn[AI_DOWN_N];
        double up = mf_ai_rev_points((double)lim, cap, idle,
                                     a->lsave.rev_down, AI_DOWN_N, dn);
        *(volatile float*)(base + AI_OFF_REVLIM) = (float)cap;
        *(volatile float*)(base + AI_OFF_REV_UPSHIFT) = (float)up;
        for (i = 0; i < AI_DOWN_N; i++)
            *(volatile float*)(base + AI_OFF_DOWNSHIFT + 4u * (DWORD)i)
                = (float)dn[i];
        a->rev_our_cap = cap;
        /* M1: every cap write is logged with its source tag (snapshot after
         * the first save; live-first on the very first write). */
        logf_("AI REV APPLY: slot %d cap %.0f source=%s (%.0f%% of %.0f, "
              "upshift %.0f) t=%.0f", slot, cap,
              first_save ? "live" : "snapshot",
              lim > 1.0 ? 100.0 * cap / (double)lim : 100.0,
              (double)lim, up, SESSION_TIME);
    }
}

/* ---- smoke banks ---- */
static void ai_bank_restore(MfAiSlot *a, int slot)
{
    DWORD base = MF_AIV(slot);
    float cur[3];
    int i;
    if (!a->lsave.banks_valid) return;
    cur[0] = ai_off_f((DWORD)slot, AI_OFF_BANK1);
    cur[1] = ai_off_f((DWORD)slot, AI_OFF_BANK2);
    cur[2] = ai_off_f((DWORD)slot, AI_OFF_BANK3);
    for (i = 0; i < 3; i++) {
        if (fabs((double)cur[i] - a->bank_our[i]) < 1e-4) {
            *(volatile float*)(base + AI_OFF_BANK1 + 4u * (DWORD)i)
                = (float)a->lsave.bank[i];
        }
    }
    a->lsave.banks_valid = 0;
    a->bank_our[0] = a->bank_our[1] = a->bank_our[2] = 0.0;
}

static void ai_bank_apply(MfAiSlot *a, int slot, double b1, double b2, double b3)
{
    DWORD base = MF_AIV(slot);
    double w[3];
    int i, changed = 0;
    w[0] = b1; w[1] = b2; w[2] = b3;
    if (!a->lsave.banks_valid) {
        ai_lsave_stamp(a);
        a->lsave.banks_valid = 1;
        a->lsave.bank[0] = ai_off_f((DWORD)slot, AI_OFF_BANK1);
        a->lsave.bank[1] = ai_off_f((DWORD)slot, AI_OFF_BANK2);
        a->lsave.bank[2] = ai_off_f((DWORD)slot, AI_OFF_BANK3);
    }
    for (i = 0; i < 3; i++)
        if (fabs(w[i] - a->bank_our[i]) > 1e-4) changed = 1;
    if (!changed) return;
    for (i = 0; i < 3; i++) {
        *(volatile float*)(base + AI_OFF_BANK1 + 4u * (DWORD)i)
            = (float)w[i];
        a->bank_our[i] = w[i];
    }
}

/* ---- PassingNorm (0x44CC) and TrackAggression (0x44F8) ---- */
static void ai_adj_restore(MfAiSlot *a, int slot)
{
    DWORD base = MF_AIV(slot);
    /* audit COMP-01b: keep the record PENDING while the live value is not
     * ours (Safety Car / Red Flag owns it); drop only on a stale generation
     * (ai_levers_restore). */
    if (a->lsave.passing_valid
        && mf_ai_lever_restore_decide(
               (double)ai_off_f((DWORD)slot, AI_OFF_PASSING),
               a->passing_our, 1e-4) == MF_AI_LEVER_RESTORE_WRITE) {
        *(volatile float*)(base + AI_OFF_PASSING) = (float)a->lsave.passing;
        a->lsave.passing_valid = 0;
        a->passing_our = 0.0;
    }
    if (a->lsave.aggr_valid
        && mf_ai_lever_restore_decide(
               (double)ai_off_f((DWORD)slot, AI_OFF_AGGRESSION),
               a->aggr_our, 1e-4) == MF_AI_LEVER_RESTORE_WRITE) {
        *(volatile float*)(base + AI_OFF_AGGRESSION) = (float)a->lsave.aggression;
        a->lsave.aggr_valid = 0;
        a->aggr_our = 0.0;
    }
}

static void ai_adj_apply(MfAiSlot *a, int slot, double passing_mult,
                         double aggr_mult)
{
    DWORD base = MF_AIV(slot);
    if (passing_mult < 1.0 - 1e-6) {
        float cur = ai_off_f((DWORD)slot, AI_OFF_PASSING);
        if (!a->lsave.passing_valid) {
            ai_lsave_stamp(a);
            a->lsave.passing_valid = 1;
            a->lsave.passing = cur;
        }
        {
            double want = a->lsave.passing * passing_mult;
            if (fabs((double)cur - a->passing_our) < 1e-4
                || fabs(want - (double)cur) > 1e-4) {
                *(volatile float*)(base + AI_OFF_PASSING) = (float)want;
                a->passing_our = want;
            }
        }
    } else if (a->lsave.passing_valid
               && mf_ai_lever_restore_decide(
                      (double)ai_off_f((DWORD)slot, AI_OFF_PASSING),
                      a->passing_our, 1e-4) == MF_AI_LEVER_RESTORE_WRITE) {
        *(volatile float*)(base + AI_OFF_PASSING) = (float)a->lsave.passing;
        a->lsave.passing_valid = 0;
        a->passing_our = 0.0;
    }
    if (aggr_mult < 1.0 - 1e-6) {
        float cur = ai_off_f((DWORD)slot, AI_OFF_AGGRESSION);
        if (!a->lsave.aggr_valid) {
            ai_lsave_stamp(a);
            a->lsave.aggr_valid = 1;
            a->lsave.aggression = cur;
        }
        {
            double want = a->lsave.aggression * aggr_mult;
            if (fabs((double)cur - a->aggr_our) < 1e-4
                || fabs(want - (double)cur) > 1e-4) {
                *(volatile float*)(base + AI_OFF_AGGRESSION) = (float)want;
                a->aggr_our = want;
            }
        }
    } else if (a->lsave.aggr_valid
               && mf_ai_lever_restore_decide(
                      (double)ai_off_f((DWORD)slot, AI_OFF_AGGRESSION),
                      a->aggr_our, 1e-4) == MF_AI_LEVER_RESTORE_WRITE) {
        *(volatile float*)(base + AI_OFF_AGGRESSION)
            = (float)a->lsave.aggression;
        a->lsave.aggr_valid = 0;
        a->aggr_our = 0.0;
    }
}

/* ---- maxSpeed terminal limp (spec 4.10: ONLY here) ---- */
static void ai_maxspeed_apply(MfAiSlot *a, int slot, int limp)
{
    DWORD base = MF_AIV(slot);
    if (!limp) return;
    if (!a->lsave.maxspeed_valid) {
        ai_lsave_stamp(a);
        a->lsave.maxspeed_valid = 1;
        a->lsave.maxspeed = ai_off_f((DWORD)slot, AI_OFF_MAXSPEED);
    }
    if (a->lsave.maxspeed > 45.0) {
        float cur = ai_off_f((DWORD)slot, AI_OFF_MAXSPEED);
        if (mf_ai_maxspeed_should_write((double)cur, 45.0))
            *(volatile float*)(base + AI_OFF_MAXSPEED) = 45.0f;
        a->maxspeed_our = 45.0;
    } else {
        a->maxspeed_our = a->lsave.maxspeed;  /* stock already lower: leave it */
    }
}

static void ai_maxspeed_restore(MfAiSlot *a, int slot)
{
    DWORD base = MF_AIV(slot);
    if (!a->lsave.maxspeed_valid) return;
    /* audit COMP-01b: pending while the value is not ours; only write when the
     * live value is still MF's last write (or the saved stock value it left). */
    if (mf_ai_lever_restore_decide((double)ai_off_f((DWORD)slot, AI_OFF_MAXSPEED),
                                   a->maxspeed_our, 0.5)
        == MF_AI_LEVER_RESTORE_WRITE) {
        *(volatile float*)(base + AI_OFF_MAXSPEED) = (float)a->lsave.maxspeed;
        a->lsave.maxspeed_valid = 0;
        a->maxspeed_our = 0.0;
    }
}

/* restore every lever this slot owns (fault end, retire, session, unload) */
static void ai_levers_restore(MfAiSlot *a, int slot)
{
    if (!mf_ai_lever_save_clean(&a->lsave)
        && (a->lsave_gen != g_session_gen || SESSION_OVER())) {
        logf_("AI: slot %d lever restore dropped (%s)", slot,
              a->lsave_gen != g_session_gen ? "stale session generation"
                                            : "session over");
        mf_ai_lever_save_clear(&a->lsave);
        a->rev_our_cap = 0.0;
        a->bank_our[0] = a->bank_our[1] = a->bank_our[2] = 0.0;
        a->maxspeed_our = 0.0;
        a->passing_our = 0.0;
        a->aggr_our = 0.0;
        a->lsave_gen = 0;
        a->levers_pending = 0;
        return;
    }
    ai_rev_restore(a, slot);
    ai_bank_restore(a, slot);
    ai_adj_restore(a, slot);
    ai_maxspeed_restore(a, slot);
    if (!mf_ai_lever_save_clean(&a->lsave)) {
        /* audit COMP-01b: passing/aggression/maxSpeed stay pending until the
         * live value is MF's own write again (Safety Car / Red Flag owns it).
         * Log the transition once, not every pass. */
        if (!a->levers_pending) {
            a->levers_pending = 1;
            logf_("AI: slot %d lever restore pending (value not ours yet; flags 0x%x)",
                  slot, (a->lsave.rev_valid ? 1 : 0) | (a->lsave.banks_valid ? 2 : 0)
                  | (a->lsave.maxspeed_valid ? 4 : 0)
                  | (a->lsave.passing_valid ? 8 : 0)
                  | (a->lsave.aggr_valid ? 16 : 0));
        }
    } else if (a->levers_pending) {
        a->levers_pending = 0;
        logf_("AI: slot %d lever restore complete", slot);
    }
}

/* ---- B4: AI blue smoke (same queue/slot position; game thread spawn) ---- */
static void ai_blue_pump(MfAiSlot *a, int slot, double dt)
{
    if (!(a->sym_blue > 0.05) || !(dt > 0.0)) return;
    a->blue_acc += a->sym_blue * AI_AIR_BLUE_PARTS_S * dt;
    if (a->blue_acc >= 1.0) {
        int n = (int)a->blue_acc;
        if (n > 2) n = 2;                     /* budget: <= 2x player rate */
        a->blue_acc -= n;
        pq_push(2, n, (float)(0.2 + 0.4 * a->sym_blue), 0.0f, 0.0f, slot);
    }
}

/* ---- B6: Option A ending (plan section 8, live-verified) ----
 * power column 0 + idle 0 + ReasonCode 1 -> the game retires the car cleanly
 * (FinishState 2, results "Engine") with NO vanilla smoke. The only fireball
 * is the blow-up's banks. Returns 1 when the ending was written. */
static int ai_option_a_end(MfAiSlot *a, int slot, const AiSymptom *sym)
{
    DWORD base = MF_AIV(slot);
    uint32_t tp = *(volatile uint32_t*)(base + AI_OFF_TQ_PTR);
    int cnt = *(volatile int*)(base + AI_OFF_TQ_COUNT);
    float *tbl = (float*)(uintptr_t)tp;
    int i, power_ok = 0;

    if (tbl && cnt >= 10 && cnt <= AI_MAX_ENTRIES
        && !IsBadReadPtr(tbl, (UINT_PTR)cnt * 2u * sizeof(float))
        && !a->tbl_shared) {
        if (!a->tbl_written) {
            for (i = 0; i < cnt; i++) a->tbl_saved[i] = tbl[2 * i + 1];
            a->tbl_ptr = tbl;
            a->tbl_count = cnt;
            a->tbl_gen = g_session_gen;
            a->tbl_written = 1;
        }
        if (tbl == a->tbl_ptr && cnt == a->tbl_count) {
            for (i = 0; i < cnt; i++) tbl[2 * i + 1] = 0.0f;
            power_ok = 1;
        }
    }
    *(volatile float*)(base + AI_OFF_IDLE) = 0.0f;
    *(volatile int*)(base + AI_OFF_REASON) = 1;
    /* the fault's own banks (never vanilla black); blow-ups are the only
     * fireball (spec 4) */
    if (sym->ending == MF_AI_END_BLOWN) {
        ai_bank_apply(a, slot, 0.0, 0.5, 1.0);
    } else if (sym->ending == MF_AI_END_REAR) {
        ai_bank_apply(a, slot, 0.3, 0.0, 0.0);   /* rear break: grey puff */
    } else if (sym->fault == MF_FAULT_WATER_LEAK
               || sym->fault == MF_FAULT_RAD_DEBRIS) {
        ai_bank_apply(a, slot, -0.6, 0.0, 0.0);
    } else if (sym->fault == MF_FAULT_OIL_PRESSURE) {
        ai_bank_apply(a, slot, 0.3, 0.0, 0.0);
    }
    a->end_done = 1;
    a->end_kind = sym->ending;
    if (!power_ok)
        logf_("AI: slot %d Option A power column not zeroed (shared/lost table)",
              slot);
    return 1;
}

/* 2026-10-07: AI clock start gate (see ai_step_slot) */
#define MF_AI_LIFE_INIT_MIN_S  60.0    /* s, a smaller game timer is "not armed" */
#define MF_AI_LIFE_INIT_SPEED  10.0    /* m/s, car out on the road */

/* ---- B3 (pre-ship batch 2026-10-08): dev-only FORCED AI failure ---------
 * Owner request: pick an AI driver + failure type in the test panel -> Force.
 * DEV BUILD ONLY (MF_DEV_TOOLS): the release build compiles this whole section
 * out - the release DLL opens no MF_AI_FORCE mapping and carries no command
 * strings (same MF-11 rule as the AI override / gear detour).
 * The command enters on the GAME THREAD: the existing AIMainUpdate post-hook
 * (ai_pulloff_hook, the same place the owner override is applied) validates
 * the requested slot and files it into that slot's request. The worker's next
 * ai_step_slot consumes it and arms the fault with mf_fault_arm - exactly the
 * manual-trigger arm the player's panel buttons use - so everything after
 * that is the EXISTING AI symptom/reaction path (mf_ai_present ->
 * mf_ai_react -> levers -> Option A ending -> pull-off). No new failure
 * mechanism and no new game-memory writer.
 * Panel side: tools/mech_test_panel.py (AiPage, FORCE button). */
#ifdef MF_DEV_TOOLS
#define AI_FORCE_NAME  "Local\\MF_AI_FORCE_V1"
#define AI_FORCE_MAGIC 0x4D464146u   /* 'MFAF' */
typedef struct {
    volatile uint32_t magic;        /* DLL sets once the dev channel is open */
    volatile int32_t  seq;          /* panel -> DLL: bumped per command     */
    volatile int32_t  slot;         /* target AI slot 1..MF_AI_MAX-1        */
    volatile int32_t  fault;        /* MF_FAULT_*                           */
    volatile int32_t  sev;          /* MF_SEV_*                             */
    volatile int32_t  budget_s;     /* virtual development budget, seconds  */
    /* DLL -> panel */
    volatile int32_t  ack_seq;      /* seq accepted on the game thread, -1 none */
    volatile int32_t  ack_result;   /* 0 accepted; 3 bad fault; 4 safety car;
                                     * 5 retired; -1 nothing processed yet   */
    volatile int32_t  armed_seq;    /* seq the model armed, 0 = not yet     */
    volatile int32_t  armed_slot;
    volatile int32_t  armed_fault;
    volatile int32_t  armed_t_x10;  /* session time of the arm x10          */
    volatile int32_t  applied;      /* armed commands this process          */
} AiForceCtl;

/* the panel (tools/mech_test_panel.py FORCE_SIZE/FORCE_FIELDS) hard-codes
 * this layout; a drift is a build error here instead of a live mismatch */
_Static_assert(sizeof(AiForceCtl) == 52, "AiForceCtl size changed");

static HANDLE      g_ai_force_map = NULL;
static AiForceCtl *g_ai_force = NULL;
static LONG        g_ai_force_seen = 0;   /* game thread only (the hook)    */

static void ai_force_open(void)
{
    if (g_ai_force) return;
    g_ai_force_map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                        PAGE_READWRITE, 0, sizeof(AiForceCtl),
                                        AI_FORCE_NAME);
    if (!g_ai_force_map) return;
    g_ai_force = (AiForceCtl*)MapViewOfFile(g_ai_force_map,
                                            FILE_MAP_ALL_ACCESS, 0, 0,
                                            sizeof(AiForceCtl));
    if (!g_ai_force) return;
    if (g_ai_force->magic != AI_FORCE_MAGIC) {
        /* fresh (or stale) view: keep the panel's request counter, clear the
         * rest so a previous game's status can never be read as this one's */
        int32_t seq = g_ai_force->seq;
        memset((void*)g_ai_force, 0, sizeof(AiForceCtl));
        g_ai_force->seq = seq;
    }
    g_ai_force->ack_seq = -1;
    g_ai_force->ack_result = -1;
    g_ai_force->armed_seq = 0;
    g_ai_force->applied = 0;
    g_ai_force->magic = AI_FORCE_MAGIC;
    logf_("AI FORCE: dev channel %s open (dev build only)", AI_FORCE_NAME);
}

/* Game thread (AIMainUpdate post-hook): accept a new force request for the
 * vehicle being stepped. Only the requested slot's car consumes it. */
static void ai_force_apply(uintptr_t veh)
{
    AiForceCtl *f = g_ai_force;
    DWORD slot;
    if (!f || f->magic != AI_FORCE_MAGIC) return;
    if (veh < VEH_BASE || ((veh - VEH_BASE) % AI_VEH_STRIDE) != 0) return;
    slot = (DWORD)mf_islot((int)((veh - VEH_BASE) / AI_VEH_STRIDE));
    if (slot < 1 || slot >= MF_AI_MAX) return;
    if (f->seq == g_ai_force_seen) return;   /* already handled this request */
    if ((int32_t)slot != f->slot) return;    /* not the target car yet       */
    if (f->fault < MF_FAULT_VALVE_AIR || f->fault >= MF_FAULT_COUNT) {
        f->ack_seq = f->seq;
        f->ack_result = 3;                   /* bad failure type */
        g_ai_force_seen = f->seq;
        return;
    }
    if (f->sev < MF_SEV_SMALL || f->sev > MF_SEV_BIG) f->sev = MF_SEV_MEDIUM;
    if (!(f->budget_s > 0 && f->budget_s <= 3600))
        f->budget_s = (int32_t)MF_MANUAL_BUDGET_S;
    {
        MfAiSlot *a = &g_ai[slot];
        a->force_fault = f->fault;
        a->force_sev = f->sev;
        a->force_budget_s = f->budget_s;
        InterlockedExchange(&a->force_seq, f->seq);   /* release */
    }
    f->ack_seq = f->seq;
    f->ack_result = 0;
    g_ai_force_seen = f->seq;
}
#else
static void ai_force_open(void) { }
static void ai_force_apply(uintptr_t veh) { (void)veh; }
#endif

/* N3e: start the player-grade fireball at an AI car - claim the shared
 * FLAMES descriptor (restored after the LAST blow ends, see blow_fx_update)
 * and queue the flash + burn trail. Game thread spawn via the fuelcut drain;
 * P5 fades bank3 later and releases the descriptor at the same event. */
static void ai_blow_fx_begin(MfAiSlot *a, int slot)
{
    blow_fx_claim(slot);
    ai_burst_at(slot, SFX_ID_FLAMES, MF_AI_BLOW_FLASH_N, 1.0f, 1.8f, 0.30f);
    a->ai_blow_fx_until = SESSION_TIME + MF_AI_BLOW_FX_BURN_S;
    a->ai_blow_fx_next = SESSION_TIME;
}

static void ai_step_slot(int slot, double dt, double divisor, int use_accum)
{
    MfAiSlot *a = &g_ai[slot];
    DWORD base = MF_AIV(slot);
    double rpm, revlim, throttle, speed, timer;
    int reason, finish;
    int life_infinite;
#ifdef MF_DEV_TOOLS
    int force_pending = (a->force_seq != a->force_seq_seen);   /* B3 dev */
#endif

    if (!mf_ai_slot_active(slot, a->done)) return;
    /* 2026-10-07 audit MF-12 (E1, CONFIRMED veh+0x32C8): only LOCAL AI cars
     * are modelled - a network car (2/3) belongs to another machine, an
     * empty slot is -1. The game's own AIMainUpdate skips > 1 the same way. */
    if (*(volatile int*)(base + 0x32C8u) != 1) return;
    /* 2026-10-07 (VPS usage reports, install 221bb9: slot 22 "Bernd Maylander"
     * got oil-leak/exhaust faults in 3 races and was rev-capped to 92 %): the
     * Safety Car mod's car is an entrant but not a racer - a failing safety car
     * would break that mod. It is never modelled (no faults, no levers). */
    {
        char nm[32];
        const char *ep = g_grade_eng_path[slot];
        ai_driver_name(slot, nm, (int)sizeof nm);
        if (strstr(nm, "Maylander") || mfu_has_ci(ep, "safetycar")) {
            a->done = 1;
#ifdef MF_DEV_TOOLS
            if (force_pending && g_ai_force
                && g_ai_force->magic == AI_FORCE_MAGIC) {
                a->force_seq_seen = a->force_seq;   /* consume: refused */
                g_ai_force->ack_seq = a->force_seq;
                g_ai_force->ack_result = 4;         /* safety car - never faulted */
                logf_("AI FORCE: slot %d is the safety car - force refused", slot);
            }
#endif
            logf_("AI: slot %d %s is the safety car - not modelled", slot,
                  nm[0] ? nm : "?");
            return;
        }
    }
    if (use_accum) {
        /* H4c/R15: consume at most MF_DT_MAX_S per visit and KEEP the
         * remainder - a >10 s backlog is never dropped (round-robin). */
        if (!(a->accum_dt > 0.0)) return;
        dt = a->accum_dt > MF_DT_MAX_S ? MF_DT_MAX_S : a->accum_dt;
        a->accum_dt -= dt;
        if (a->accum_dt < 1e-9) a->accum_dt = 0.0;
    }
    if (!(dt > 0.0) || dt > MF_DT_MAX_S) return;

    timer  = ai_off_f((DWORD)slot, AI_OFF_TIMER);
    revlim = ai_off_f((DWORD)slot, AI_OFF_REVLIM);
    rpm    = ai_off_f((DWORD)slot, AI_OFF_RPM);
    speed  = ai_off_f((DWORD)slot, AI_OFF_SPEED);
    finish = ai_off_i((DWORD)slot, AI_OFF_FINISH);
    reason = ai_off_i((DWORD)slot, AI_OFF_REASON);
    if (!(revlim > 100.0 && revlim < 10000.0)) return;      /* not loaded */
    if (finish != 0 || reason != 0) {
#ifdef MF_DEV_TOOLS
        if (force_pending) {
            a->force_seq_seen = a->force_seq;   /* consume: refused */
            if (g_ai_force && g_ai_force->magic == AI_FORCE_MAGIC) {
                g_ai_force->ack_seq = a->force_seq;
                g_ai_force->ack_result = 5;     /* already retired */
            }
            logf_("AI FORCE: slot %d already retired - force refused", slot);
        }
#endif
        /* retired: stop stepping AND release any power lever we own (H4b,
         * H1c generation rules) - a retired car must not keep a table scaled.
         * Claude live review 2026-10-01: the GAME's own engine kill (its
         * clock after a scripted drop, or any vanilla engine failure) left no
         * record - Sato died with no terminal line. Book it like our own
         * terminal (T3: ONSET line first), and log every other retirement
         * once with its ReasonCode. */
        if (!a->terminal_written) {
            a->terminal_written = 1;
            if (reason == 1) {
                ai_log_onset_if_needed(a, slot);
                g_ai_stat_term[a->eng.fault]++;
                g_ai_stat_term_s += SESSION_TIME;
                if (a->onset_logged && SESSION_TIME >= a->onset_t)
                    g_ai_stat_phase_s += SESSION_TIME - a->onset_t;
                g_ai_stat_term_n++;
                logf_("AI: slot %d terminal t=%.0f (game engine failure, "
                      "ReasonCode 1, fault %s, onset %s)", slot, SESSION_TIME,
                      dmg_fault_name_(a->eng.fault),
                      a->onset_logged ? "yes" : "NO");
                dmg_log_(slot, &a->eng);
            } else {
                logf_("AI: slot %d retired t=%.0f (ReasonCode %d, finish %d) - "
                      "not an engine death", slot, SESSION_TIME, reason, finish);
            }
        }
        /* P5 (ADDENDUM 15): a game-clock engine kill on a car still on track
         * (finish 0) starts the pull-off; a car already at FinishState 2 is
         * being removed and is left alone. The pure machine waits 0.3-1.0 s
         * (notice) before the first write (spec 1.1.1). */
        if (reason == 1 && finish == 0
            && (!a->pulloff.active || a->pulloff.state == MF_PO_ASIDE)
            && g_po_installed && g_po_verify >= 0)   /* review: ASIDE car dies too */
            mf_po_begin(&a->pulloff, MF_PO_REASON_GAMEKILL, SESSION_TIME,
                        po_notice_delay(a));
        a->done = 1;
        ai_table_restore(a, slot);
        ai_levers_restore(a, slot);   /* B3: no lever survives a slot */
        /* live 6 (Monteiro, owner: "his engine is still running"): the
         * game's own kill only sets ReasonCode 1 - the engine keeps idling
         * and the car crawls. A car still on track gets the Option A engine
         * stop (power column 0 + idle 0, no fire). */
        if (reason == 1 && finish == 0 && !a->end_done) {
            AiSymptom se;
            memset(&se, 0, sizeof se);
            se.ending = MF_AI_END_STOP;
            se.fault = a->eng.fault;
            ai_option_a_end(a, slot, &se);
            logf_("AI END: slot %d game kill -> engine stop (Option A) t=%.0f",
                  slot, SESSION_TIME);
        }
        return;
    }

    /* Q7c (ADDENDUM 7, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): scripted
     * vanilla drop. A > 50 % fall of the game timer in one pass that is NOT
     * our own write (and any finite value after an infinite one - mode 3
     * seeds FLT_MAX) is the game's RaceEvents script / re-arm: honour it as
     * the WHEN - ai_life takes the new game value with NO divisor, and Q2
     * ownership stops for the slot (the game's clock then kills the car). */
    if (a->timer_seen_valid
        && mf_ai_scripted_drop(a->timer_seen, (double)timer, a->timer_written)) {
        logf_("AI: slot %d scripted drop %.0f -> %.0f", slot, a->timer_seen,
              (double)timer);
        a->scripted = 1;
        a->ai_life = (double)timer;
        a->ai_life_valid = 1;
        a->ai_gen = g_session_gen;
    }
    a->timer_seen = (double)timer;
    a->timer_seen_valid = 1;

    /* 2026-10-07 tester report (VPS usage logs: 6 AI "engine life expired"
     * at t=30 s in qualifying, 5 at t=11 s in a race, then parked in their
     * boxes): the AI clock took the game timer at the FIRST sample, while
     * the car still sat in the garage / on the grid with a timer the game
     * had not armed yet (~0). The clock now starts only once the game has
     * armed a real timer AND the car is running on the road; until then the
     * slot is not modelled (the game alone owns the car). */
    if (!a->ai_life_valid && !a->scripted && !(timer > 1e30)
        && !(timer > MF_AI_LIFE_INIT_MIN_S && rpm > 1.0 && speed > MF_AI_LIFE_INIT_SPEED)
#ifdef MF_DEV_TOOLS
        && !force_pending   /* B3 dev: a forced fault models this car now */
#endif
        )
        return;

    /* car: rev limit from memory, peak from the car's own table; lifetime
     * curve and optimum oil are the model's relative fallbacks */
    /* 2026-10-08 ratchet fix (MF 1.0.3 M1): while our rev lever is applied
     * the live field holds OUR cap. Deriving the car from it shrank rev_limit
     * every tick (cap of a cap) down to the 100 rad/s guard: tiny upshift
     * points, top gear, every faulted AI crawling at ~68 km/h. Use the saved
     * original (mf_ai_rev_derive, the same helper the ratchet test drives). */
    revlim = mf_ai_rev_derive(revlim, a->lsave.rev_valid,
                              (double)a->lsave.rev_limit);
    if (!a->car_valid || fabs(revlim - a->car_rev_snapshot) > 0.005 * revlim
        || (g_grade_therm_valid[slot] && !a->car_thermal_used)) {
        double tq_peak = ai_tq_peak_rads((DWORD)slot);
        unsigned flags;
        if (g_grade_therm_valid[slot]) {
            /* J3b: the H5-parsed per-car thermal values (same engine.ini) */
            flags = mf_car_derive(&a->car, revlim,
                                  g_grade_rpm_base[slot],
                                  g_grade_rpm_range[slot],
                                  g_grade_oil_base[slot],
                                  g_grade_oil_range[slot],
                                  g_grade_oil_opt[slot],
                                  0.0, tq_peak, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            a->car_thermal_used = 1;
        } else {
            flags = mf_car_derive(&a->car, revlim, 0.0, 0.0, 0.0, 0.0, 0.0,
                                  0.0, tq_peak, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        }
        a->car_valid = 1;
        a->car_rev_snapshot = revlim;
        if (!g_ai_slot_logged[slot]) {
            g_ai_slot_logged[slot] = 1;
            logf_("AI: slot %d car rev_limit=%.0f peak=%.0f q=%.3f fallbacks=0x%04X "
                  "(relative car values)", slot, a->car.rev_limit_rads,
                  a->car.peak_tq_rads, a->eng.quality, flags);
        }
        if (g_grade_therm_valid[slot] && !a->thermal_logged) {
            a->thermal_logged = 1;
            logf_("AI: slot %d car thermal from %s fallbacks=0x%04X",
                  slot, g_grade_eng_path[slot], flags);
        }
    }

    /* item 2 shadow thermal: oil -> optimum - 10 K (tau 60 s) plus the model's
     * own extra heat; water = oil - 15 K (tau 120 s), the delta above ambient
     * scaled by 1 / airflow when the radiator is degraded */
    if (!a->thermal_init) {
        a->oil_k = a->car.oil_opt_k - 10.0;
        a->water_k = a->oil_k - 15.0;
        a->thermal_init = 1;
    }
    {
        double amb = (double)AMBIENT_C + 273.15;
        a->oil_k += (a->car.oil_opt_k - 10.0 - a->oil_k)
                    * mf_clamp(dt / 60.0, 0.0, 1.0) + a->out.oil_heat_ks * dt;
        {
            double air = mf_clamp(a->out.rad_air_scale, 0.25, 1.0);
            double wtgt = amb + ((a->oil_k - 15.0) - amb) / air;
            a->water_k += (wtgt - a->water_k) * mf_clamp(dt / 120.0, 0.0, 1.0);
        }
    }

    throttle = ai_off_f((DWORD)slot, AI_OFF_THROTTLE);
    if (!(throttle >= 0.0 && throttle <= 1.0)) {
        double rr = a->car.rev_limit_rads > 1.0 ? rpm / a->car.rev_limit_rads : 0.0;
        double rising = speed > a->speed_prev ? 1.0 : 0.2;
        throttle = mf_clamp(rr * rr * rising, 0.0, 1.0);
        if (!g_ai_proxy_logged) {
            g_ai_proxy_logged = 1;
            logf_("AI: throttle out of range on slot %d - proxy in use", slot);
        }
    }

    {
        MfInputs in;
        /* J1 (docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): BOTH modes use the game
         * AI timer (veh+0x3D70) as the life clock - law mode is THERMAL ONLY
         * and never a life clock (WHO = grade + q). Q1 drains the clock by
         * the per-car game-law rate below. */
        memset(&in, 0, sizeof in);
        if (g_ctl && g_ctl->magic == CTL_MAGIC && g_ctl->ai_engine_law) {
            /* law THERMAL mode: the game's thermal ODE per car supplies
             * oil/water to the model; WHEN stays the game timer above */
            MfAiLawCoef k;
            int same_car = ENG_REV_LIMIT > 100.0
                        && fabs(revlim - ENG_REV_LIMIT) <= 0.01 * ENG_REV_LIMIT;
            if (same_car) {
                k.comb_heat = COMB_HEAT;
                k.speed_heat = OIL_SPEED_HEAT;
                k.min_cool = OIL_MIN_COOL;
                k.owt_k0 = OWT_K0;
                k.owt_k1 = OWT_K1;
                k.rad_k0 = RAD_BASE;
                k.rad_k1 = RAD_AIR_K1;
            } else {
                k.comb_heat = 32.0;
                k.speed_heat = 7.759e-4;
                k.min_cool = 9.212e-4;
                k.owt_k0 = 5.312e-3;
                k.owt_k1 = 1.022e-4;
                k.rad_k0 = 2.945e-4;
                k.rad_k1 = 1.022e-4;
            }
            k.stat_target_k = a->car.oil_opt_k - 10.0;
            k.amb_k = (double)AMBIENT_C + 273.15;
            if (!a->law_logged) {
                a->law_logged = 1;
                logf_("AI LAW: slot %d thermal only, life = game timer, "
                      "coefficients %s (test mode - changes AI reliability)",
                      slot, same_car ? "same-car (player object)" : "car-relative defaults");
            }
            {
                /* live 2026-10-01 (Claude): one explicit step of up to 10 s
                 * (round-robin / backlog chunks) diverged the thermal ODE
                 * (thermostat K = 1/s needs h < 2 s): 14 AI engines died at
                 * once with water 115-170 C and oil below 0 C. Substep. */
                double left = dt;
                while (left > 1e-9) {
                    double h = left > 0.05 ? 0.05 : left;
                    mf_ai_law_step(&a->law, &k, &a->car, rpm, throttle, speed, h);
                    left -= h;
                }
            }
        in.oil_k = a->law.oil_k;
        in.water_k = a->law.water_k;
    } else {
        in.oil_k = a->oil_k;
        in.water_k = a->water_k;
    }

    /* Q1 (ADDENDUM 3, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): the AI life
     * clock. ai_life starts at timer/divisor on the first valid sample and
     * drains by the car's own game-law drain (mf_game_drain at the oil the
     * model gets: law oil in law mode, simple oil otherwise), normalised to
     * the running field mean and clamped 0.25-4.0 (mf_ai_wear_rate; 1.0
     * until the field mean is valid, so the average car drains 1 s/s and
     * the vanilla failure count is kept). Timer infinite: unchanged. */
#ifdef MF_DEV_TOOLS
    if (force_pending && !a->ai_life_valid && !(timer > 1e30)) {
        /* B3 dev (see ai_force_apply): model this car even before the game
         * has armed its own clock - the gate above was bypassed for it */
        a->ai_life = 600.0;
        a->ai_life_valid = 1;
        a->ai_gen = g_session_gen;
    }
#endif
    life_infinite = timer > 1e30;
    a->drain_law = mf_game_drain(&a->car, rpm, in.oil_k, 0.0);
    a->drain_running = (rpm > 1.0) ? 1 : 0;
    if (life_infinite) {
        a->ai_rate = 1.0;
        in.life_s = 1e9;
        in.life_infinite = 1;
        in.life_drain_sps = 1.0;
    } else {
        /* Claude live review 2026-10-01 (unattended all-fail race, dll
         * FE8EFB01): the per-car Q1 rate pinned at its clamps live (4.00 for
         * the first deaths, 0.25 for every running car a few minutes later) -
         * mf_game_drain is sampled at the INSTANT rpm (quadratic) against a
         * 600 s field EMA, so the ratio swings clamp to clamp and is not mean
         * preserving: AI engines died up to 1.8x early / 4x late against the
         * vanilla timer (owner rule: AI keeps the vanilla WHEN). Shipped
         * default is now the q6 variant 1 (rate 1.0 = the realized vanilla
         * draw 1:1, 0 missed / 0 extra); the spread stays a build option until
         * it is rebuilt on a smoothed per-car drain. */
        a->ai_rate = MF_AI_RATE_SPREAD
                   ? mf_ai_wear_rate(a->drain_law, g_ai_field_mean,
                                     g_ai_field_valid)
                   : 1.0;
        if (!a->ai_life_valid) {
            a->ai_life = timer / (divisor > 0.0 ? divisor : 1.0);
            a->ai_div = divisor > 0.0 ? divisor : 1.0;
            a->ai_life_valid = 1;
            a->ai_gen = g_session_gen;
        }
        if (a->scripted) {
            /* Q7c: after a honoured scripted drop the GAME's clock is the WHEN
             * (ownership stopped) - follow it exactly so the fault develops
             * to the moment the game ends the car (live: Sato's valves sat at
             * 0 for ~10 min while our clock and the game's drifted apart). */
            a->ai_life = timer > 0.0f ? (double)timer : 0.0;
        } else if (rpm > 1.0) {
            /* 2026-10-07: no wear with the engine off (garage / parked) */
            a->ai_life -= a->ai_rate * dt;
        }
        if (a->ai_life < 0.0) a->ai_life = 0.0;
        in.life_s = a->ai_life;
        in.life_infinite = 0;
        in.life_drain_sps = a->ai_rate;
    }
#ifdef MF_DEV_TOOLS
    if (force_pending) {
        /* B3 dev: consume the request the game thread filed and arm it with
         * the SAME manual-trigger call the player's panel buttons use; the
         * existing symptom/reaction code below runs on a->eng.onset. */
        a->force_seq_seen = a->force_seq;
        if (a->eng.onset && !a->out.blow_up && !a->out.engine_stop) {
            /* M5 1.0.3 (owner live finding: force fuelp on slot 6, then
             * oilleak 9 s later - the second force REPLACED the first):
             * a car that already carries an active MF fault refuses a second
             * force; the panel shows "refused: car already has <fault>". */
            logf_("AI FORCE: slot %d already has %s - force refused", slot,
                  mf_ai_fault_name(a->eng.fault));
            if (g_ai_force && g_ai_force->magic == AI_FORCE_MAGIC) {
                g_ai_force->ack_seq = a->force_seq;
                g_ai_force->ack_result = 6;   /* already has a failure */
            }
        } else {
            mf_fault_arm(&a->eng, &in, a->force_fault, a->force_sev,
                         (double)a->force_budget_s);
            logf_("AI FORCE: slot %d armed %s (sev %d, budget %d s) at t=%.0f "
                  "(dev force)", slot, mf_ai_fault_name(a->force_fault),
                  a->force_sev, a->force_budget_s, SESSION_TIME);
            if (g_ai_force && g_ai_force->magic == AI_FORCE_MAGIC) {
                g_ai_force->armed_seq = a->force_seq;
                g_ai_force->armed_slot = slot;
                g_ai_force->armed_fault = a->force_fault;
                g_ai_force->armed_t_x10 = (int32_t)(SESSION_TIME * 10.0);
                g_ai_force->applied++;
            }
        }
    }
#endif
    a->law.life_s = in.life_s;   /* J1c: the clock actually used (Q1: ai_life) */
    in.dt_s = dt;
        in.rpm_rads = rpm;
        in.load = throttle;
        in.speed_mps = speed;
        in.fuel_l = 60.0;
        in.g_lat = ai_off_f((DWORD)slot, AI_OFF_G_LAT) / 9.81;
        in.g_long = -ai_off_f((DWORD)slot, AI_OFF_G_LONG) / 9.81;
        in.ambient_c = AMBIENT_C;
        in.engine_running = rpm > 1.0 ? 1 : 0;
        in.on_track = 1;
        in.in_pit = 0;
        in.off_track = 0;
        in.self_seize = 0;
        in.reason_code = reason;
        in.niggles_enable = 0;     /* E9p: AI gets no niggles */
        /* Q7b (ADDENDUM 7, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): the early
         * random-failure layer is OFF for AI - vanilla has no counterpart
         * (fairness rule: AI keeps the vanilla WHEN and mean count); the
         * player keeps it (player_inputs early_enable = 1). Log once per
         * session. */
        in.early_enable = 0;
        if (!g_ai_early_off_logged) {
            g_ai_early_off_logged = 1;
            logf_("AI: early failure layer OFF (player keeps it)");
        }
        in.ai_mode = 1;            /* E9a torque-zero death off for AI */
        /* Q6a (ADDENDUM 6, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): ai_onset_model
         * 0 = countdown on ai_life = timer/divisor (DEFAULT since Q8a,
         * ADDENDUM 8); 1 = hazard (offline A/B). Q7 (ADDENDUM 7) made the
         * countdown reproduce the realized vanilla WHEN 1:1 (Q7 sweep:
         * missed-vanilla 0.0 %, ours-not-vanilla 0.0 % at rate 1.0); Claude's
         * Q7 gate-1 ruling set the reference to that realized fixed-seed draw
         * (the 1997 Phi expectation was ~1.4 SE off the sampled draw), and Q8
         * flips the shipped default to the countdown. The player keeps its own
         * Qt onset_model lever. */
        in.onset_model = (g_ctl && g_ctl->magic == CTL_MAGIC
                          && g_ctl->ai_onset_model == 0) ? 0 : 1;
        /* B5/B6 (ADDENDUM 14, spec 0-7): the reaction layer is LIVE for AI:
         * the same damage physics as the player, no blanket fairness hold; a
         * noticed driver who is nursing (R1+) lets the fault physics decide
         * instead of the Q7a clock backstop. */
        in.ai_reaction = 1;
        /* R2+ (limp/defend) hands the clock to the physics; an R1 nurse keeps
         * the exact vanilla WHEN (the Q7a backstop still ends it on time). */
        in.ai_nursed = (a->react.noticed && a->react.level >= 2
                        && !a->react.retire) ? 1 : 0;

        mf_model_step(&a->eng, &a->car, &in, &a->out);
    }

    /* ---- B3-B6 (ADDENDUM 14): symptoms, reaction, levers ----
     * Runs after the model step so the symptoms are the ones the model just
     * produced; the reaction is passed to the NEXT step (in.ai_nursed) and
     * changes the REAL AI behaviour through the levers, which the model then
     * measures - the physics pays off (spec 3.4). Battle gap and race
     * fraction are not proven fields in this run: gap = none (-1), race
     * fraction = 1.0 (context modifiers off; logged in the batch log). */
    if (a->eng.onset) {
        AiSymptom sym = mf_ai_present(&a->out, &a->eng, &a->car);
        mf_rc_mark(slot, 1);   /* Batch C: this car is MF-limited for the AI Toolkit */
        /* M2b: a new onset restarts this car's crawler average (the guard
         * must judge the faulted pace, not the healthy one) */
        if (!a->crawl_onset_seen) {
            a->crawl_onset_seen = 1;
            a->crawl_ema = 0.0;
            a->crawl_since = 0.0;
        }
        double coolant_frac = mf_clamp(a->eng.v_w_l / MF_WATER_START_L, 0.0, 1.0);
        a->sym_blue = sym.blue;
        if (!a->traits.valid) {
            double ra = ai_off_f((DWORD)slot, AI_OFF_RACEABILITY);
            double cr = ai_off_f((DWORD)slot, AI_OFF_CRASH);
            char nm[24];
            if (!(ra >= 0.0 && ra <= 1.0))
                ra = (ra > 1.5 && ra <= 100.0) ? ra / 100.0 : 0.5;
            if (!(cr >= 0.0 && cr <= 1.0))
                cr = (cr > 1.5 && cr <= 100.0) ? cr / 100.0 : 0.5;
            a->traits = mf_ai_traits_roll(mf_clamp(ra, 0.0, 1.0),
                                          mf_clamp(cr, 0.0, 1.0),
                                          &a->react_rng);
            ai_driver_name(slot, nm, (int)sizeof nm);
            if (!a->react_traits_logged) {
                a->react_traits_logged = 1;
                logf_("AI REACT: slot %d %s %s traits s=%.2f y=%.2f r=%.2f "
                      "t=%.2f c=%.2f", slot, nm[0] ? nm : "?",
                      mf_ai_fault_name(a->eng.fault), a->traits.sensitivity,
                      a->traits.sympathy, a->traits.racer,
                      a->traits.team_order, a->traits.crash);
            }
        }
        if (a->traits.valid) {
            mf_ai_react(&a->react, &sym, &a->eng, &a->traits, &a->react_rng,
                        dt, a->eng.t_s, 1.0, -1.0, coolant_frac);
            if (a->react.noticed && !a->react_last_noticed) {
                a->react_last_noticed = 1;
                logf_("AI REACT: slot %d %s noticed %.0f s -> R%d",
                      slot, mf_ai_fault_name(a->eng.fault),
                      a->react.notice_delay_s, a->react.level);
            }
            if (a->react.level != a->react_last_level) {
                double cap = mf_ai_cap_rads(&a->react, &sym, &a->car);
                int lvl = a->react.level;
                a->react_last_level = lvl;
                logf_("AI REACT: slot %d R%d margin %.3f cap %.0f%% t=%.0f",
                      slot, lvl, a->react.margin,
                      a->car.rev_limit_rads > 1.0
                          ? 100.0 * cap / a->car.rev_limit_rads : 0.0,
                      a->eng.t_s);
                if (lvl == 3)
                    logf_("AI PIT: slot %d R3 wants the box (callToBox not "
                          "wired in this run - open item)", slot);
            }
            /* M2 (owner retest 2026-10-08, "HE SHOULD RETIRE"; spec Part M
             * item 2): a faulted car that cannot keep pace must not crawl
             * for laps. Below MF_AI_CRAWL_FRAC of the field median speed for
             * MF_AI_CRAWL_HOLD_S -> the existing R4 retire below (Option A
             * ending + pull-off). The pit option is deliberately not wired:
             * callToBox is an open item, so the log says "would pit" and the
             * car retires. ASIDE cars are handled by the M4 stall escalation
             * in the pull-off machine (they count as pull-off here). */
            if (!a->react.retire
                && ai_off_i((DWORD)slot, AI_OFF_PIT_STATUS) < 1
                && !a->pulloff.active
                && mf_ai_crawl_step(&a->crawl_since, a->crawl_ema,
                                    g_ai_field_speed, SESSION_TIME,
                                    MF_AI_CRAWL_FRAC, MF_AI_CRAWL_HOLD_S)) {
                logf_("AI CRAWL: slot %d %.0f km/h vs field median %.0f km/h "
                      "for %.0f s - would pit (callToBox not wired), retires",
                      slot, speed * 3.6, g_ai_field_speed * 3.6,
                      SESSION_TIME - a->crawl_since);
                a->react.retire = 1;
            }
            /* 2026-10-07 tester report (race + qualifying): an R2/R3 car
             * crawled for half a race - R2+ turns the vanilla clock off
             * (in.ai_nursed), R3's pit call is not wired (callToBox open
             * item) and the ASIDE pull-off only ends in the pit lane, so
             * nothing ever ended it. A limping driver who has not recovered
             * now gives up: R3 (wants the box) after 60 s, R2 after 180 s ->
             * the R4 ending below (engine stop, results "Engine", pull-off). */
            if (a->react.noticed && a->react.level >= 2 && !a->react.retire) {
                if (a->limp_since <= 0.0) a->limp_since = SESSION_TIME + 1e-3;
                else if (SESSION_TIME - a->limp_since
                         >= (a->react.level >= 3 ? 60.0 : 180.0)) {
                    logf_("AI LIMP: slot %d R%d for %.0f s, no pit path - retires",
                          slot, a->react.level, SESSION_TIME - a->limp_since);
                    a->react.retire = 1;
                }
            } else if (!a->react.retire) {
                a->limp_since = 0.0;
            }
            /* B6: R4 = the driver stops to save the engine - Option A ending
             * now, no fire (results "Engine", spec 5) */
            if (a->react.retire && !a->end_done) {
                AiSymptom se = sym;
                double blow_roll = -1.0;
                int blown_end = 0;
                se.ending = MF_AI_END_STOP;
                /* N3d (owner D4, spec N3d): an existing valve/oil/fuel fault
                 * that retires with a badly damaged engine ends in a BLOW-UP
                 * on the 50 % roll (MF_AI_LIMP_BLOW_P), logged with the roll. */
                if ((a->eng.fault == MF_FAULT_VALVE_AIR
                     || a->eng.fault == MF_FAULT_OIL_PRESSURE
                     || a->eng.fault == MF_FAULT_OIL_LEAK
                     || a->eng.fault == MF_FAULT_FUEL_LEAK)
                    && (a->eng.h_vt < MF_AI_LIMP_BLOW_HEALTH
                        || a->eng.h_b < MF_AI_LIMP_BLOW_HEALTH)) {
                    blow_roll = mf_rng_d(&a->react_rng);
                    blown_end = blow_roll < MF_AI_LIMP_BLOW_P;
                    logf_("AI LIMP BLOW: slot %d fault %s h_vt %.2f h_b %.2f "
                          "roll %.2f -> %s", slot,
                          mf_ai_fault_name(a->eng.fault), a->eng.h_vt,
                          a->eng.h_b, blow_roll,
                          blown_end ? "BLOWN" : "STOP");
                    if (blown_end) se.ending = MF_AI_END_BLOWN;
                }
                ai_log_onset_if_needed(a, slot);
                ai_option_a_end(a, slot, &se);
                a->terminal_written = 1;
                a->done = 1;
                g_ai_stat_term[a->eng.fault]++;
                g_ai_stat_term_s += SESSION_TIME;
                if (a->onset_logged && SESSION_TIME >= a->onset_t)
                    g_ai_stat_phase_s += SESSION_TIME - a->onset_t;
                g_ai_stat_term_n++;
                if (blown_end) {
                    ai_play_fx_at(slot, "mf_engine_blow.wav", 2.0f);
                    ai_blow_fx_begin(a, slot);   /* N3e fireball at the car */
                }
                logf_("AI END: slot %d R4 retire t=%.0f fault %s "
                      "(Option A, %s)", slot, SESSION_TIME,
                      mf_ai_fault_name(a->eng.fault),
                      blown_end ? "blow-up" : "no fire");
                dmg_log_(slot, &a->eng);
                /* P5: the R4 stop is an Option A ending -> pull-off */
                if (g_po_installed && g_po_verify >= 0) {
                    mf_po_begin(&a->pulloff, MF_PO_REASON_R4, SESSION_TIME,
                                po_notice_delay(a));
                    if (blown_end) {
                        a->po_bank_fade_t = SESSION_TIME + 10.0
                                            + 10.0 * mf_rng_d(&a->react_rng);
                        a->po_bank_faded = 0;
                    }
                }
                return;
            }
            if (a->react.mistake_count != a->last_mistake_count) {
                a->last_mistake_count = a->react.mistake_count;
                logf_("AI MISTAKE: slot %d M%d %.1f s fault %s t=%.0f",
                      slot, a->react.mistake, a->react.mistake == 1
                          ? a->react.m1_left_s : a->react.m2_left_s,
                      mf_ai_fault_name(a->eng.fault), a->eng.t_s);
            }
            /* P5 (spec 1.2): a ruined race (R2 limp/defend) moves off the
             * line (ASIDE) until the pit lane; released on fault end below. */
            if (a->react.noticed && a->react.level >= 2 && !a->react.retire
                && !a->pulloff.active && !a->aside_spent   /* 2026-10-07: no restart loop */
                && g_po_installed && g_po_verify >= 0)
                mf_po_begin_aside(&a->pulloff, SESSION_TIME);
        }
        /* levers: only while the fault is running (an ending keeps its banks
         * and the terminal branch owns the rest) */
        if (!a->out.blow_up && !a->out.engine_stop
            && !a->out.break_rear_suspension) {
            double cap = mf_ai_cap_rads(&a->react, &sym, &a->car);
            if (a->react.noticed && a->react.level >= 1
                && cap < a->car.rev_limit_rads - 0.5)
                ai_rev_apply(a, slot, cap);
            else
                ai_rev_restore(a, slot);
            ai_adj_apply(a, slot, a->react.passing_mult,
                         a->react.aggression_mult);
            ai_maxspeed_apply(a, slot, a->react.noticed && a->react.level >= 4);
            {
                double b1 = 0.0, b2 = 0.0, b3 = 0.0;
                if (sym.steam > 0.01)
                    b1 = -mf_clamp(0.1 + 0.5 * sym.steam, 0.1, 0.6);
                /* N3b: the BLOW-UP warning shows a small grey bank puff */
                if (a->eng.fault == MF_FAULT_BLOWUP && !a->out.blow_up)
                    b2 = MF_AI_BLOWUP_PUFF;
                if (a->flame_left_s > 0.0) b3 = 1.0;
                if (b1 != 0.0 || b2 != 0.0 || b3 != 0.0)
                    ai_bank_apply(a, slot, b1, b2, b3);
                else
                    ai_bank_restore(a, slot);
            }
        }
        /* fuel pressure: hard top cut + misfire pulse windows (B3) */
        a->top_cut_active = (sym.top_cut_rads >= 0.0) ? 1 : 0;
        a->top_cut_rads = sym.top_cut_rads;
        if (a->mis_left_s > 0.0) a->mis_left_s -= dt;
        if (a->mis_left_s <= 0.0 && sym.misfire_depth >= MF_MISFIRE_CUT_DEPTH_MIN) {
            double rate = MF_MISFIRE_EVT_HZ_MAX * sym.misfire_depth / 0.6;
            double p = 1.0 - exp(-rate * dt);
            if (mf_rng_d(&a->react_rng) < p)
                a->mis_left_s = MF_MISFIRE_CUT_MIN_S
                    + (MF_MISFIRE_CUT_MAX_S - MF_MISFIRE_CUT_MIN_S)
                      * mf_rng_d(&a->react_rng);
        }
        /* exhaust: flame spits on lift + the game's own pop sample */
        if (a->eng.fault == MF_FAULT_EXHAUST && !a->eng.blown
            && !a->eng.stopped) {
            a->flame_cool_s -= dt;
            if (a->flame_left_s > 0.0) a->flame_left_s -= dt;
            if (throttle < 0.1 && a->flame_left_s <= 0.0
                && a->flame_cool_s <= 0.0) {
                double p = 1.0 - exp(-sym.pop_rate_s * dt * 2.0);
                if (mf_rng_d(&a->react_rng) < p) {
                    a->flame_left_s = 0.3;
                    a->flame_cool_s = 1.0;
                    ai_exhaust_pop_at(slot);
                }
            }
        }
        /* water faults: steam hiss at the car, one call per ~4 s */
        if (sym.steam > 0.3) {
            a->sound_cool_s -= dt;
            if (a->sound_cool_s <= 0.0) {
                a->sound_cool_s = 4.0;
                ai_play_fx_at(slot, "mf_steam_hiss.wav", 0.8f);
            }
        }
        /* N1 (owner D2/D6): one AI oil line per 10 % of sump lost, so the
         * smoke-onset timing is measurable from the log (once per step). */
        if (a->eng.fault == MF_FAULT_OIL_LEAK && !a->eng.blown
            && !a->eng.stopped) {
            double lost = 1.0 - a->eng.v_oil_l / MF_OIL_START_L;
            int step = (int)(lost * 10.0);
            if (step > a->oil_step_logged) {
                a->oil_step_logged = step;
                logf_("AI OIL: slot %d lost %.0f%% h_b %.2f t=%.0f", slot,
                      lost * 100.0, a->eng.h_b, SESSION_TIME);
            }
        }
        a->blue_acc += 0.0;   /* keep the accumulator; pump below */
        ai_blue_pump(a, slot, dt);
    } else {
        ai_levers_restore(a, slot);
        mf_rc_mark(slot, 0);   /* Batch C: fault ended - no longer MF-limited */
        a->crawl_onset_seen = 0;   /* M2b: a new fault restarts the average */
        a->crawl_since = 0.0;
        a->sym_blue = 0.0;
        a->top_cut_active = 0;
        a->top_cut_rads = -1.0;
        a->mis_left_s = 0.0;
        /* P5 (ADDENDUM 15): ASIDE is for an ACTIVE fault only - release it
         * when the fault ends (spec 1.2). A terminal slot's pull-off chain
         * never reaches this branch (done slots return above). */
        if (a->pulloff.active && a->pulloff.reason == MF_PO_REASON_ASIDE) {
            po_share_clear(a);
            mf_po_stop(&a->pulloff);
            logf_("AI ASIDE: slot %d released (fault ended) t=%.0f",
                  slot, SESSION_TIME);
            a->po_aside_logged = 0;
        }
        a->flame_left_s = 0.0;
    }

    /* P2: hazard onset shadow (no behaviour) - log once. Q3: when the hazard
     * APPLIED the onset the ONSET line below is the record. */
    if (a->eng.shadow_fired && !a->shadow_logged && !a->eng.onset_hazard) {
        a->shadow_logged = 1;
        logf_("HAZARD (shadow): slot %d x=%.3f t=%.0f would-fault=%s (%d)",
              slot, a->eng.shadow_x, SESSION_TIME,
              dmg_fault_name_(a->eng.shadow_fault), a->eng.shadow_fault);
    }

    if (!a->onset_logged && a->eng.onset)
        ai_log_onset_if_needed(a, slot);

    /* Q2 (ADDENDUM 3, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): own the game
     * timer while the slot is live - keep veh+0x3D70 >= ai_life + 600 s so
     * the vanilla roll cannot kill a car our clock keeps alive. Only a live
     * slot of the current generation is written (never done/stale); the
     * terminal path below still writes 0.001. Q7c: a slot whose scripted
     * drop was honoured is never owned again (the game's clock kills it on
     * time). The panel's "vanilla" box shows the owned value, so log the
     * first write per slot. */
    if (!life_infinite && a->ai_life_valid && !a->scripted
        && mf_ai_timer_own_ok(a->done, a->ai_gen, g_session_gen)
        && !SESSION_OVER()
        && !a->out.blow_up && !a->out.engine_stop
        && mf_ai_timer_needs_own(timer, a->ai_life)) {
        double own = (double)mf_ai_timer_own_value(a->ai_life);
        *(volatile float*)(base + AI_OFF_TIMER) = (float)own;
        a->timer_written = own;   /* Q7c: never read our own write as scripted */
        if (!a->timer_owned) {
            a->timer_owned = 1;
            logf_("AI: slot %d timer owned", slot);
        }
    }

    if ((a->out.blow_up || a->out.engine_stop) && !a->terminal_written) {
        /* B6 (ADDENDUM 14, plan section 8): Option A ending - the mod ends the
         * AI engine itself (power column 0 + idle 0 + ReasonCode 1 -> clean
         * FinishState 2, results "Engine", NO vanilla smoke) and shows the
         * fault's own banks. The only fireball is the blow-up. The power
         * column stays zeroed (no ai_table_restore here); the save record is
         * released at session end (H1c rules). */
        AiSymptom sym_end = mf_ai_present(&a->out, &a->eng, &a->car);
        ai_log_onset_if_needed(a, slot);   /* T3: never a fault terminal
                                            * without its ONSET line */
        ai_option_a_end(a, slot, &sym_end);
        a->terminal_written = 1;
        a->done = 1;
        g_ai_stat_term[a->eng.fault]++;
        g_ai_stat_term_s += SESSION_TIME;
        if (a->onset_logged && SESSION_TIME >= a->onset_t)
            g_ai_stat_phase_s += SESSION_TIME - a->onset_t;
        g_ai_stat_term_n++;
        logf_("AI: slot %d terminal t=%.0f (%s, Option A %s)", slot,
              SESSION_TIME, a->out.reason,
              sym_end.ending == MF_AI_END_BLOWN ? "blow-up"
              : sym_end.ending == MF_AI_END_REAR ? "rear break" : "stop");
        if (sym_end.ending == MF_AI_END_BLOWN
            || sym_end.ending == MF_AI_END_REAR)
            ai_play_fx_at(slot, "mf_engine_blow.wav", 2.0f);
        if (sym_end.ending == MF_AI_END_BLOWN)
            ai_blow_fx_begin(a, slot);   /* N3e: flash + fire + smoke trail */
        dmg_log_(slot, &a->eng);   /* P1: damage mix at terminal */
        /* P5 (ADDENDUM 15): every model terminal starts the pull-off after
         * the notice; a blow-up additionally schedules the P5 fire fade
         * (bank3 -> 0 after 10-20 s, bank1/2 light smoke kept). */
        if (g_po_installed && g_po_verify >= 0) {
            mf_po_begin(&a->pulloff, MF_PO_REASON_END, SESSION_TIME,
                        po_notice_delay(a));
            if (sym_end.ending == MF_AI_END_BLOWN) {
                a->po_bank_fade_t = SESSION_TIME + 10.0
                                    + 10.0 * mf_rng_d(&a->react_rng);
                a->po_bank_faded = 0;
            }
        }
        return;
    }

#if MF_AI_POWER_TABLE
    {
        uint32_t tp = *(volatile uint32_t*)(base + AI_OFF_TQ_PTR);
        int cnt = *(volatile int*)(base + AI_OFF_TQ_COUNT);
        float *tbl = (float*)(uintptr_t)tp;
        if (tbl && cnt >= 10 && cnt <= AI_MAX_ENTRIES
            && !IsBadReadPtr(tbl, (UINT_PTR)cnt * 2u * sizeof(float))) {
            if (!a->tbl_written && !a->tbl_shared && a->eng.onset
                && !a->eng.culminated && !a->eng.blown && !a->eng.stopped) {
                /* H4b/D4: teammates can share one torque table. ANY slot whose
                 * table is shared gets NO power lever (not only the later
                 * ones), so a healthy sharer is never weakened either. */
                int src = ai_table_shared_with_other(slot, tbl, cnt);
                if (src >= 0) {
                    a->tbl_shared = 1;
                    logf_("AI: slot %d shares table with slot %d - power lever skipped",
                          slot, src);
                } else {
                    int i;
                    for (i = 0; i < cnt; i++) a->tbl_saved[i] = tbl[2 * i + 1];
                    a->tbl_ptr = tbl;
                    a->tbl_count = cnt;
                    a->tbl_gen = g_session_gen;   /* H1c: record generation */
                    a->tbl_written = 1;
                    logf_("AI: slot %d torque table power column saved (%d entries)",
                          slot, cnt);
                }
            }
            if (a->tbl_written && !a->tbl_shared
                && ai_table_shared_with_other(slot, a->tbl_ptr, a->tbl_count) >= 0) {
                /* H4b: sharing discovered after our write (a teammate loaded
                 * or faulted later) - put the saved column back and drop the
                 * lever so no sharer keeps a scaled table. */
                if (tbl == a->tbl_ptr && cnt == a->tbl_count) {
                    int i;
                    for (i = 0; i < cnt; i++) tbl[2 * i + 1] = a->tbl_saved[i];
                }
                a->tbl_written = 0;
                a->tbl_gen = 0;
                a->tbl_ptr = NULL;
                a->tbl_count = 0;
                a->tbl_shared = 1;
                logf_("AI: slot %d table shared - power lever released", slot);
            }
            if (a->tbl_written && tbl == a->tbl_ptr && cnt == a->tbl_count) {
                double f = a->out.power_mult
                         * mf_power_shape_mult(&a->car, &a->out, rpm, throttle);
                int i;
                if (f > 1.0) f = 1.0;
                if (f < MF_POWER_MIN) f = MF_POWER_MIN;
                for (i = 0; i < cnt; i++) {
                    float v;
                    double row_rpm = (double)i * MF_TQ_STEP_RADS;
                    v = (float)(a->tbl_saved[i] * f);
                    /* B3: fuel-pressure hard top cut and the misfire pulse
                     * zero the row on top of the shape (same writer -> the
                     * shared-table rule stays intact) */
                    if (a->top_cut_active && row_rpm >= a->top_cut_rads)
                        v = 0.0f;
                    if (a->mis_left_s > 0.0) v = 0.0f;
                    tbl[2 * i + 1] = v;
                }
            }
        }
    }
#else
    {
        /* fallback lever: maxSpeed = v_ref x power_mult^(1/3); only while the
         * field is stock 99999 or our own value; restored when the fault ends */
        int fault_active = a->eng.onset && !a->eng.culminated
                        && !a->eng.blown && !a->eng.stopped;
        if (!a->eng.onset) {
            if (SESSION_TIME - a->v_ref_t > 90.0) {
                a->v_ref = speed;
                a->v_ref_t = SESSION_TIME;
            } else if (speed > a->v_ref) {
                a->v_ref = speed;
            }
        }
        if (fault_active) {
            float cur = *(volatile float*)(base + AI_OFF_MAXSPEED);
            double f = a->out.power_mult;
            double target = (a->v_ref > 1.0 ? a->v_ref : 90.0) * cbrt(f > 0.0 ? f : 0.0);
            if (cur == 99999.0f || fabs((double)cur - a->v_last) < 1e-3) {
                *(volatile float*)(base + AI_OFF_MAXSPEED) = (float)target;
                a->v_last = target;
            }
        } else if (a->v_last > 0.0) {
            float cur = *(volatile float*)(base + AI_OFF_MAXSPEED);
            if (fabs((double)cur - a->v_last) < 1e-3)
                *(volatile float*)(base + AI_OFF_MAXSPEED) = 99999.0f;
            a->v_last = 0.0;
        }
    }
#endif
    a->speed_prev = speed;
}

#include "mf_coop_engine.h"   /* 2026-10-10 CO-OP: index map, fault events */

static void ai_engines_pump(double dt)
{
    EngineCtl *c = g_ctl;
    int i;
    double divisor;

    if (!g_mf_rc) g_mf_rc = f1c_rc_mf_open();   /* Batch C: publish the MF car-limited record */
    if (!c || c->magic != CTL_MAGIC) return;
    /* vanilla+: Failure Rate "none" = no mod failures for anyone */
    if (!c->ai_engines_enable || PLR_FAILRATE == 0) {
        if (g_ai_on) {
            g_ai_on = 0;
            ai_shutdown();
            logf_("AI: shadow engines OFF (power tables restored)");
        }
        return;
    }
    if (!g_ai_on) {
        g_ai_on = 1;
        logf_("AI: shadow engines ON (life "
              "divisor x%.1f, law %s, power %s)",
              c->ai_life_divisor_x10 / 10.0, c->ai_engine_law ? "native" : "timer",
              MF_AI_POWER_TABLE ? "torque table" : "maxSpeed");
        logf_("TEAM QUALITY: no team address - per-car q");
    }
    if (SESSION_OVER() || !session_live()) return;
    {   /* late pull-off detour (see the start-up comment): once per load */
        static int po_tried = 0;
        if (!po_tried) { po_tried = 1; ai_pulloff_install(); }
    }
    if (g_ai_seed != g_ai_slots_seed) {
        /* J4: an invalidation drew a new basis while the model did not pass
         * through the session-start branch (live-to-live session flow): the
         * slot engines still carry the previous session's q. Rebuild them
         * under the new basis. ai_shutdown already released the tables at the
         * invalidation; restore again as a guard before dropping the state. */
        for (i = 0; i < MF_AI_MAX; i++) {
            ai_table_restore(&g_ai[i], i);
            ai_levers_restore(&g_ai[i], i);
            ai_slot_reset(&g_ai[i], i);
        }
        g_ai_slots_seed = g_ai_seed;
        logf_("AI: new session seed - slot engines re-initialised");
    }
    divisor = (double)c->ai_life_divisor_x10 / 10.0;
    if (!(divisor >= 0.1 && divisor <= 100.0)) divisor = 1.0;

    /* Q1: the field mean of the last completed pass feeds this pass's rates */
    ai_field_mean_update(dt);
    /* M2b: the field median of the per-car speed EMAs feeds the crawler guard */
    ai_field_speed_update(dt);

    /* P3 (ADDENDUM 15): pull-off/aside state runs for live AND done slots
     * (a dead car keeps coasting after its terminal); before the round-robin
     * so it is never skipped. */
    ai_pulloff_pump(dt);

    if (g_ai_rr) {
        for (i = 0; i < MF_AI_MAX; i++) g_ai[i].accum_dt += dt;
        for (i = 0; i < 8; i++) {
            ai_step_slot(g_ai_rr_slot, 0.0, divisor, 1);
            g_ai_rr_slot++;
            if (g_ai_rr_slot >= MF_AI_MAX) g_ai_rr_slot = 1;
        }
        return;
    }
    {
        LARGE_INTEGER q0, q1, freq;
        double ms;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&q0);
        for (i = 1; i < MF_AI_MAX; i++)
            ai_step_slot(i, dt, divisor, 0);
        QueryPerformanceCounter(&q1);
        ms = (double)(q1.QuadPart - q0.QuadPart) * 1000.0 / (double)freq.QuadPart;
        if (ms > 2.0) {
            g_ai_rr = 1;
            g_ai_rr_slot = 1;
            logf_("AI: pump %.2f ms > 2 ms - round-robin 8 cars per tick", ms);
        }
    }
}

/* ======================================================================
 * ADDENDUM 15 (P1-P8): AI pull-off / "get off the line".
 * Spec docs/AI_PULLOFF_SYSTEM_SPEC_2026-10-02.md section 7 (in-DLL hook,
 * supersedes the external-loop section 2). The worker gathers inputs from
 * the live car (positions, waypoint chain, traffic, per-wheel surface) and
 * steps the pure machine; the game-thread detour below applies the output
 * as the ONLY writer of the AI controls (external control flickered: finding
 * ai-external-control-two-writer-limit-2026-10-02).
 * ====================================================================== */

static volatile LONG g_po_inflight = 0;
static int   g_po_installed = 0;
static BYTE *g_po_tramp = NULL;
static int   g_po_verify = 0;          /* 0 wait, 1 ok, -1 disabled        */
static int   g_po_verify_logged = 0;
static const BYTE g_po_expect[AI_PULLOFF_STOLEN] =
    { 0x83, 0xEC, 0x1C, 0x53, 0x55, 0x56 };

typedef void (__attribute__((thiscall)) *AiMainUpdateFn)(void *veh);

/* P5: per-driver notice 0.3-1.0 s from the sensitivity trait (spec 1.1.1). */
static double po_notice_delay(const MfAiSlot *a)
{
    double sens = a->traits.valid
                ? mf_clamp(a->traits.sensitivity, 0.0, 1.0) : 0.5;
    return 0.3 + 0.7 * (1.0 - sens);
}

/* worker -> game-thread output copy (seqlock: odd while writing). */
static void po_share_write(MfAiSlot *a, const MfPoOut *o)
{
    a->po_share.seq++;
    a->po_share.apply = o->apply;
    a->po_share.steer_zero = o->steer_zero;
    a->po_share.steer_add = (float)o->steer_add;
    a->po_share.steer_abs_set = o->steer_abs_set;
    a->po_share.steer_abs = (float)o->steer_abs;
    a->po_share.brake_set = o->brake_set;
    a->po_share.brake = (float)o->brake;
    a->po_share.brake_cap = (float)o->brake_cap;
    a->po_share.throttle_zero = o->throttle_zero;
    a->po_share.seq++;
}

static void po_share_clear(MfAiSlot *a)
{
    MfPoOut z;
    memset(&z, 0, sizeof z);
    po_share_write(a, &z);
}

/* P4: per-wheel material through the SDK f1c_surface_contact.h chain
 * (vehicle+0x3674 + i x 0xF8 -> +0x54 -> +0x30; road/wet/rumble = drivable).
 * Bounded read-only, worker and hook safe. */
static int po_read_ok(uintptr_t addr, void *dst, size_t n)
{
    if (!addr || IsBadReadPtr((const void*)addr, n)) return 0;
    memcpy(dst, (const void*)addr, n);
    return 1;
}

static void po_surface(uintptr_t base, MfPoIn *in)
{
    F1CSurfaceContacts sc = f1c_surface_contacts(base, po_read_ok);
    in->wheels_known = sc.known;
    in->wheels_drivable = sc.drivable;
}

/* one live waypoint snapshot (offsets from docs/52 + the 2026-10-02 reads:
 * +0x94/0x9C active pos, +0x20/0x28 perp, +0xA4/0xAC fwd, +0x2C/0x30 road,
 * +0x34/0x38 wall, +0x3C segment length). */
static int po_snapshot_wp(uintptr_t wp, MfPoWp *out)
{
    if (!wp || IsBadReadPtr((const void*)wp, 0x140)) return 0;
    out->pos_x = *(volatile float*)(wp + 0x94u);
    out->pos_z = *(volatile float*)(wp + 0x9Cu);
    out->perp_x = *(volatile float*)(wp + 0x20u);
    out->perp_z = *(volatile float*)(wp + 0x28u);
    out->fwd_x = *(volatile float*)(wp + 0xA4u);
    out->fwd_z = *(volatile float*)(wp + 0xACu);
    out->road_l = *(volatile float*)(wp + 0x2Cu);
    out->road_r = *(volatile float*)(wp + 0x30u);
    out->wall_l = *(volatile float*)(wp + 0x34u);
    out->wall_r = *(volatile float*)(wp + 0x38u);
    out->seg_len = *(volatile float*)(wp + 0x3Cu);
    return (out->pos_x == out->pos_x && out->pos_z == out->pos_z
            && out->perp_x == out->perp_x && out->fwd_x == out->fwd_x);
}

/* walk the AIW chain from veh+0x4354 via wp+0x110 for at least 3 waypoints
 * and at most the stopping distance (spec 1.1.2), capped at 12 entries. */
static int po_scan_chain(uintptr_t base, double speed, MfPoWp *wps, int maxn,
                         unsigned *first_wp)
{
    uintptr_t wp = (uintptr_t)(*(volatile uint32_t*)(base + AI_OFF_WP_CUR));
    double budget = speed * speed / (2.0 * MF_PO_STOP_DECEL);
    double acc = 0.0;
    int n = 0;

    if (first_wp) *first_wp = (unsigned)wp;
    if (budget < AI_PO_LOOKAHEAD_S) budget = AI_PO_LOOKAHEAD_S;
    while (wp && n < maxn) {
        if (!po_snapshot_wp(wp, &wps[n])) break;
        acc += wps[n].seg_len > 0.0 ? wps[n].seg_len : 0.0;
        n++;
        if (n >= 3 && acc >= budget) break;
        if (IsBadReadPtr((const void*)(wp + AI_PO_WP_NEXT), 4)) break;
        wp = (uintptr_t)(*(volatile uint32_t*)(wp + AI_PO_WP_NEXT));
    }
    return n;
}

/* |car heading - segment forward| (car fwd = (-sin(yaw), -cos(yaw)):
 * AIMainUpdate line 181-182 pattern). */
static double po_heading_err(uintptr_t base, const MfPoWp *wp)
{
    double yaw = *(volatile float*)(base + AI_OFF_YAW);
    double hx = -sin(yaw), hz = -cos(yaw);
    double d = hx * wp->fwd_x + hz * wp->fwd_z;
    if (d > 1.0) d = 1.0;
    if (d < -1.0) d = -1.0;
    return acos(d);
}

/* dot(car fwd, wp perp): + = car heading toward +lateral (ROLL steer). */
static double po_heading_side(uintptr_t base, const MfPoWp *wp)
{
    double yaw = *(volatile float*)(base + AI_OFF_YAW);
    return -sin(yaw) * wp->perp_x - cos(yaw) * wp->perp_z;
}

/* next-corner side for ASIDE: the far look-ahead waypoint's lateral in the
 * current waypoint frame (it sits on the inside of the coming corner). Uses
 * the same frame as lat, so no handedness assumption (review fix: the DS
 * cross-product mapping was an unverified sign). */
static int po_apex_side(const MfPoWp *wps, int n)
{
    double dx, dz, along, side;
    if (n < 2) return 0;
    dx = wps[n - 1].pos_x - wps[0].pos_x;
    dz = wps[n - 1].pos_z - wps[0].pos_z;
    along = dx * wps[0].fwd_x + dz * wps[0].fwd_z;
    side = mf_po_lat(&wps[0], wps[n - 1].pos_x, wps[n - 1].pos_z);
    if (!(along > 10.0)) return 0;
    if (side > 0.15 * along) return 1;    /* corner turns to +lateral */
    if (side < -0.15 * along) return -1;
    return 0;
}

/* traffic + car free space (spec 1.1.3 and section 6): other cars in our
 * current-waypoint frame, +-30 m; free space within +-5.6 m longitudinal. */
static void po_traffic_scan(uintptr_t base, int self, const MfPoWp *wp0,
                            double lat, MfPoIn *in)
{
    double px = *(volatile float*)(base + AI_OFF_POS_X);
    double pz = *(volatile float*)(base + AI_OFF_POS_Z);
    int j;

    in->n_traffic = 0;
    in->car_free_l = 99.0;
    in->car_free_r = 99.0;
    if (!(px == px) || !(pz == pz)) return;
    /* CO-OP: scan every vehicle slot (bots 32+, remote humans) */
    int jn = g_mfc_latched ? MFC_VEH_SLOTS : MF_AI_MAX;
    uintptr_t selfb = MF_AIV(self);
    for (j = 0; j < jn; j++) {
        uintptr_t ob;
        double ox, oz, rl, dx, dz, lng, olat, gap;
        if (g_mfc_latched ? (mfc_veh(j) == selfb || mfc_marker(j) < 0)
                          : (j == self)) continue;
        ob = g_mfc_latched ? mfc_veh(j) : MF_AIV(j);
        rl = *(volatile float*)(ob + AI_OFF_REVLIM);
        if (!(rl > 100.0 && rl < 10000.0)) continue;
        ox = *(volatile float*)(ob + AI_OFF_POS_X);
        oz = *(volatile float*)(ob + AI_OFF_POS_Z);
        if (!(ox == ox) || !(oz == oz)) continue;
        dx = ox - px;
        dz = oz - pz;
        lng = dx * wp0->fwd_x + dz * wp0->fwd_z;
        olat = mf_po_lat(wp0, ox, oz);
        if (fabs(olat) > 30.0 || lng < -30.0 || lng > 30.0) continue;
        if (in->n_traffic < MF_PO_TRAFFIC_MAX) {
            in->traffic[in->n_traffic].long_m = lng;
            in->traffic[in->n_traffic].lat_m = olat;
            in->n_traffic++;
        }
        if (fabs(lng) <= 5.6) {
            /* positive lateral = right (mf_ai_pulloff.h sign convention) */
            if (olat > lat + 0.1) {
                gap = olat - lat - 1.8;
                if (gap < in->car_free_r) in->car_free_r = gap;
            } else if (olat < lat - 0.1) {
                gap = lat - olat - 1.8;
                if (gap < in->car_free_l) in->car_free_l = gap;
            }
        }
    }
}

/* physics speed from position deltas (P6: veh+0x4440 goes stale in mode 4);
 * a paused pass advances no game time, so the last value is held. */
static double po_speed_update(MfAiSlot *a, uintptr_t base, double now)
{
    double x = *(volatile float*)(base + AI_OFF_POS_X);
    double z = *(volatile float*)(base + AI_OFF_POS_Z);
    if (x != x || z != z) return a->po_speed;
    if (a->po_prev_valid && now > a->po_prev_t) {
        double dt = now - a->po_prev_t;
        if (dt >= 0.02 && dt <= 1.0) {
            double dx = x - a->po_prev_x, dz = z - a->po_prev_z;
            double v = sqrt(dx * dx + dz * dz) / dt;
            if (v == v && v < 120.0) a->po_speed = v;
        }
    }
    a->po_prev_x = x;
    a->po_prev_z = z;
    a->po_prev_t = now;
    a->po_prev_valid = 1;
    return a->po_speed;
}

/* P7: edge-triggered state logs with side, wall free, car free, wheels. */
static void po_log_transition(MfAiSlot *a, int slot, int prev,
                              const MfPoIn *in)
{
    int s = a->pulloff.state;
    const char *side = a->pulloff.side == MF_PO_SIDE_L ? "L"
                     : a->pulloff.side == MF_PO_SIDE_R ? "R" : "-";
    char nm[24];

    if (s == prev) return;
    ai_driver_name(slot, nm, (int)sizeof nm);
    switch (s) {
    case MF_PO_NOTICE:
        logf_("AI PULLOFF: slot %d %s begin reason=%s notice=%.2f s t=%.0f",
              slot, nm[0] ? nm : "?", mf_po_reason_name(a->pulloff.reason),
              a->pulloff.notice_s, SESSION_TIME);
        break;
    case MF_PO_COAST:
        logf_("AI PULLOFF: slot %d %s notice done -> coast (side %s, room "
              "L/R %.1f/%.1f m) t=%.0f", slot, nm[0] ? nm : "?", side,
              in->room_l, in->room_r, SESSION_TIME);
        break;
    case MF_PO_WAIT:
        logf_("AI PULLOFF: slot %d %s traffic wait (car free L/R %.1f/%.1f m) "
              "t=%.0f", slot, nm[0] ? nm : "?", in->car_free_l,
              in->car_free_r, SESSION_TIME);
        break;
    case MF_PO_DRIFT:
        logf_("AI PULLOFF: slot %d %s drift side %s goal %.1f m (wall free "
              "L/R %.1f/%.1f, road L/R %.1f/%.1f, lat %.1f) t=%.0f",
              slot, nm[0] ? nm : "?", side, a->pulloff.goal_mag,
              in->wall_free_l, in->wall_free_r, in->road_edge_l,
              in->road_edge_r, in->lat, SESSION_TIME);
        break;
    case MF_PO_GOAL:
        logf_("AI PULLOFF: slot %d %s goal reached (wheels off %d/%d, lat "
              "%.1f m) t=%.0f", slot, nm[0] ? nm : "?",
              in->wheels_known - in->wheels_drivable, in->wheels_known,
              in->lat, SESSION_TIME);
        break;
    case MF_PO_ABORT:
        logf_("AI PULLOFF: slot %d %s aborted (%s, lat rate %.1f m/s) t=%.0f",
              slot, nm[0] ? nm : "?", mf_po_abort_name(a->pulloff.abort_reason),
              a->pulloff.lat_rate, SESSION_TIME);
        break;
    case MF_PO_ROLL:
        logf_("AI PULLOFF: slot %d %s roll-out side %s hold %.1f m (wheels "
              "off %d/%d, lat %.1f m, wall free %.1f, %.0f km/h) t=%.0f",
              slot, nm[0] ? nm : "?", side, a->pulloff.hold_mag,
              in->wheels_known - in->wheels_drivable, in->wheels_known,
              in->lat, a->pulloff.side == MF_PO_SIDE_L ? in->wall_free_l
                                                       : in->wall_free_r,
              in->speed_mps * 3.6, SESSION_TIME);
        break;
    case MF_PO_PARKED:
        logf_("AI PULLOFF: slot %d %s parked (mode 4) t=%.0f",
              slot, nm[0] ? nm : "?", SESSION_TIME);
        break;
    case MF_PO_ASIDE:
        if (!a->po_aside_logged) {
            a->po_aside_logged = 1;
            logf_("AI ASIDE: slot %d %s begin goal %.1f m side %s (wall free "
                  "L/R %.1f/%.1f) t=%.0f", slot, nm[0] ? nm : "?", side,
                  a->pulloff.goal_mag, in->wall_free_l, in->wall_free_r,
                  SESSION_TIME);
        }
        break;
    }
    a->po_log_state = s;
}

/* P3/P5/P6: worker pass - gather, step, publish; the hook applies. Runs for
 * done slots too (the car keeps coasting after the terminal). */
static void ai_pulloff_pump(double dt)
{
    int i;

    if (g_pump_frozen) return;   /* R1b: pause - no writes, no progression */
    for (i = 1; i < MF_AI_MAX; i++) {
        MfAiSlot *a = &g_ai[i];
        MfPoIn in;
        MfPoOut o;
        MfPoWp wps[MF_PO_LOOKAHEAD_MAX];
        uintptr_t base;
        unsigned wp0 = 0;
        double px, pz;
        int n, was_active, prev_state;

        /* P5: blow-up fire fade - bank3 -> 0 after 10-20 s, bank1/2 kept.
         * Review fix: runs before the active check so a car released early
         * (FinishState 2, chain lost) still loses the fireball. */
        if (a->po_bank_fade_t > 0.0 && !a->po_bank_faded
            && SESSION_TIME >= a->po_bank_fade_t
            && !SESSION_OVER() && session_live()) {
            a->po_bank_faded = 1;
            ai_bank_apply(a, i, 0.0, 0.5, 0.0);
            logf_("AI PULLOFF: slot %d blow-up fire faded (bank3 0) t=%.0f",
                  i, SESSION_TIME);
            if (a->ai_blow_fx_until > 0.0) {
                a->ai_blow_fx_until = 0.0;   /* N3e: release the descriptor */
                blow_fx_release(i);
            }
        }
        /* N3e: the queued fire + black smoke trail while the AI blow burns
         * (the shared FLAMES descriptor stays tuned until the LAST owner is
         * released; the P5 fade above releases this one). */
        if (a->ai_blow_fx_until > 0.0 && SESSION_TIME < a->ai_blow_fx_until
            && !SESSION_OVER() && session_live()
            && SESSION_TIME >= a->ai_blow_fx_next) {
            a->ai_blow_fx_next = SESSION_TIME + MF_AI_BLOW_FX_PERIOD_S;
            ai_burst_at(i, SFX_ID_FLAMES, MF_AI_BLOW_FX_FIRE_N, 0.9f, 1.0f,
                        0.15f);
            ai_burst_at(i, SFX_ID_BLACK, MF_AI_BLOW_FX_SMOKE_N, 0.7f, 0.8f,
                        0.08f);
        }
        if (!a->pulloff.active) continue;
        if (SESSION_TIME - a->po_dg_t >= 1.0 || SESSION_TIME < a->po_dg_t) {
            LONG c[PO_DG_N]; int r, any = 0;
            char line[256]; int len = 0;
            for (r = 0; r < PO_DG_N; r++) {
                c[r] = InterlockedExchange(&a->po_dg[r], 0);
                if (c[r]) {
                    any = 1;
                    len += snprintf(line + len, sizeof line - (size_t)len,
                                    " %s=%ld", g_po_dg_name[r], (long)c[r]);
                    if (len >= (int)sizeof line) len = (int)sizeof line - 1;
                }
            }
            a->po_dg_t = SESSION_TIME;
            logf_("AI PULLOFF DIAG: slot %d state %d brk act %.2f cmd %.2f |%s"
                  " t=%.1f", i, a->pulloff.state,
                  *(volatile float*)(MF_AIV(i)
                                     + AI_OFF_BRAKE_ACT),
                  *(volatile float*)(MF_AIV(i)
                                     + AI_OFF_BRAKE_CMD),
                  any ? line : " NO HOOK CALLS", SESSION_TIME);
        }
        base = MF_AIV(i);
        was_active = 1;
        if (SESSION_OVER() || !session_live() || !g_po_installed
            || g_po_verify < 0) {
            po_share_clear(a);
            mf_po_stop(&a->pulloff);
            if (!a->po_released_logged) {
                a->po_released_logged = 1;
                logf_("AI PULLOFF: slot %d released (hook off/session end)",
                      i);
            }
            continue;
        }

        memset(&in, 0, sizeof in);
        /* P6: a car already handed to the garage (FinishState 2) or with no
         * readable waypoint chain releases here, so a stale output can never
         * keep being applied to a dormant car. */
        in.finish_state = ai_off_i(i, AI_OFF_FINISH);
        if (in.finish_state == 2) {
            po_share_clear(a);
            mf_po_stop(&a->pulloff);
            if (a->ai_blow_fx_until > 0.0) {   /* N3e: car removed */
                a->ai_blow_fx_until = 0.0;
                blow_fx_release(i);
            }
            logf_("AI PULLOFF: slot %d released (FinishState 2) t=%.0f",
                  i, SESSION_TIME);
            a->po_aside_logged = 0;
            a->po_released_logged = 0;
            continue;
        }
        a->po_speed = po_speed_update(a, base, SESSION_TIME);
        n = po_scan_chain(base, a->po_speed, wps, MF_PO_LOOKAHEAD_MAX, &wp0);
        if (n <= 0) {
            po_share_clear(a);   /* not loaded / chain lost: write nothing */
            if (a->po_empty_logged) continue;   /* 2026-10-07: was logged every tick */
            a->po_empty_logged = 1;
            logf_("AI PULLOFF: slot %d chain scan empty -> output cleared "
                  "(wp %p) t=%.1f", i,
                  (void*)(uintptr_t)*(volatile uint32_t*)(base + AI_OFF_WP_CUR),
                  SESSION_TIME);
            continue;
        }
        a->po_empty_logged = 0;
        px = *(volatile float*)(base + AI_OFF_POS_X);
        pz = *(volatile float*)(base + AI_OFF_POS_Z);
        in.lat_frame_ok = (a->po_wp_prev != 0 && a->po_wp_prev == wp0) ? 1 : 0;
        a->po_wp_prev = wp0;
        in.lat = mf_po_lat(&wps[0], px, pz);
        in.speed_mps = a->po_speed;
        in.heading_err_rad = po_heading_err(base, &wps[0]);
        in.heading_side = po_heading_side(base, &wps[0]);
        mf_po_scan(wps, n, px, pz, MF_PO_HALF_CAR_M,
                   &in.wall_free_l, &in.wall_free_r);
        in.room_l = mf_po_room(wps, n, MF_PO_SIDE_L);
        in.room_r = mf_po_room(wps, n, MF_PO_SIDE_R);
        in.road_edge_l = wps[0].road_l;
        in.road_edge_r = wps[0].road_r;
        po_traffic_scan(base, i, &wps[0], in.lat, &in);
        po_surface(base, &in);
        in.pit_status = ai_off_i(i, AI_OFF_PIT_STATUS);
        in.apex_side = po_apex_side(wps, n);

        prev_state = a->pulloff.state;
        o = mf_po_step(&a->pulloff, &in, SESSION_TIME, dt);
        po_share_write(a, &o);
        po_log_transition(a, i, prev_state, &in);
        /* M4 (owner, Badoer live): state 8 ASIDE writes no brake by design;
         * when the machine raises `stalled` (below the pace floor for the
         * hold time) the slot retires through the existing R4 path, whose
         * Option A ending starts the normal stop chain - which brakes. */
        if (a->pulloff.stalled) {
            a->pulloff.stalled = 0;
            if (!a->react.retire && !a->done) {
                a->react.retire = 1;
                logf_("AI ASIDE: slot %d stalled below the pace floor - "
                      "retires (R4) t=%.0f", i, SESSION_TIME);
            }
        }
        if (o.park_now && !g_pump_frozen)
            *(volatile int*)(base + AI_OFF_MODE) = 4;   /* native park (P6) */

        if (was_active && !a->pulloff.active) {
            po_share_clear(a);
            if (a->pulloff.reason == MF_PO_REASON_ASIDE) {
                a->aside_spent = 1;   /* 2026-10-07: was re-begun every tick (18x log) */
                logf_("AI ASIDE: slot %d released (pit/finish) t=%.0f",
                      i, SESSION_TIME);
            } else {
                logf_("AI PULLOFF: slot %d released (FinishState %d) t=%.0f",
                      i, in.finish_state, SESSION_TIME);
            }
            a->po_aside_logged = 0;
            a->po_released_logged = 0;
        }
    }
}

/* ---- P1: runtime layout verification (fail closed) ---- */
static int po_verify_layout(void)
{
    int count = *(volatile int*)(uintptr_t)AI_ACTIVE_COUNT;
    const volatile uint32_t *arr =
        (const volatile uint32_t*)(uintptr_t)AI_ACTIVE_PTR_ARRAY;
    int i;

    if (count <= 0 || count > 64) return 0;    /* not loaded yet */
    if (IsBadReadPtr((const void*)AI_ACTIVE_PTR_ARRAY,
                     (UINT_PTR)count * sizeof(uint32_t)))
        return 0;
    for (i = 0; i < count; i++) {
        uintptr_t p = (uintptr_t)arr[i];
        uintptr_t d;
        if (!p) continue;
        if (p < VEH_BASE
            || p >= VEH_BASE + (uintptr_t)AI_VEH_STRIDE
                    * ((g_mfc_latched || mfc_active())   /* CO-OP: bots in slots 32+ */
                       ? (uintptr_t)MFC_VEH_SLOTS : (uintptr_t)(MF_AI_MAX + 1u))) {
            logf_("AI PULLOFF: AIMainUpdate layout MISMATCH (entry %d ptr %p)"
                  " - feature disabled", i, (void*)p);
            return -1;
        }
        d = p - VEH_BASE;
        if ((d % AI_VEH_STRIDE) != 0) {
            logf_("AI PULLOFF: AIMainUpdate layout MISMATCH (entry %d ptr %p)"
                  " - feature disabled", i, (void*)p);
            return -1;
        }
    }
    logf_("AI PULLOFF: AIMainUpdate layout verified (%d active cars)", count);
    return 1;
}

/* ---- P3: the game-thread apply ---- */
/* s20: owner live brake/throttle override for one AI slot, applied in the
 * AIMainUpdate post-hook (same thread, right after the AI's own write = full
 * control, no flicker). Written by tools/ai_brake_gui.py. */
#ifdef MF_DEV_TOOLS   /* audit MF-11: dev-only AI override + gear detour */
static MfDetour g_gear_detour;
#define AI_OVR_NAME  "Local\\MF_AI_OVERRIDE_V1"
#define AI_OVR_MAGIC 0x4D46414Fu   /* 'MFAO' */
typedef struct {
    volatile uint32_t magic;
    volatile int32_t  slot;
    volatile int32_t  brake_on;
    volatile float    brake;
    volatile int32_t  throttle_on;
    volatile float    throttle;
    volatile uint32_t hook_calls;   /* DLL -> GUI: proof the hook applies */
    volatile int32_t  steer_on;     /* s22: appended (old GUIs leave 0) */
    volatile float    steer;        /* -1..1, cmd 0x442C + act 0x3580 */
    /* s23: appended - gear override. gear_on stops the AI auto-shift
     * (0x426330); each bump of up_seq/down_seq = one shift via the game's own
     * shift routine 0x4EF930(veh+0xC, gear). */
    volatile int32_t  gear_on;
    volatile uint32_t gear_up_seq;
    volatile uint32_t gear_down_seq;
} AiOverride;
static HANDLE g_ovr_map = NULL;
static AiOverride *g_ovr = NULL;

static void ai_ovr_apply(uintptr_t veh)
{
    AiOverride *o = g_ovr;
    uintptr_t base;
    if (!o || o->magic != AI_OVR_MAGIC) return;
    if (o->slot < 1 || o->slot >= MF_AI_MAX) return;
    base = MF_AIV(o->slot);
    if (veh != base) return;
    if (o->brake_on) {
        float b = o->brake;
        if (!(b >= 0.0f)) b = 0.0f;
        if (b > 1.0f) b = 1.0f;
        *(volatile float*)(base + AI_OFF_BRAKE_CMD) = b;
        *(volatile float*)(base + AI_OFF_BRAKE_ACT) = b;
    }
    if (o->throttle_on) {
        float t = o->throttle;
        if (!(t >= 0.0f)) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        *(volatile float*)(base + AI_OFF_THROTTLE_CMD) = t;
        *(volatile float*)(base + AI_OFF_THROTTLE_ACT) = t;
    }
    if (o->steer_on) {
        float st = o->steer;
        if (!(st >= -1.0f)) st = -1.0f;
        if (st > 1.0f) st = 1.0f;
        *(volatile float*)(base + AI_OFF_STEER_CMD) = st;
        *(volatile float*)(base + AI_OFF_STEER_ACT) = st;
    }
    if (o->brake_on || o->throttle_on || o->steer_on) o->hook_calls++;
}

static void ai_ovr_open(void)
{
    if (g_ovr) return;
    g_ovr_map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                   0, sizeof(AiOverride), AI_OVR_NAME);
    if (!g_ovr_map) return;
    g_ovr = (AiOverride*)MapViewOfFile(g_ovr_map, FILE_MAP_ALL_ACCESS, 0, 0,
                                       sizeof(AiOverride));
    if (g_ovr) logf_("AI OVERRIDE: mapping %s open", AI_OVR_NAME);
}

/* s23: AI gear override - detour on the AI auto-shift 0x426330 (thiscall
 * veh, called from AIMainUpdate). Prologue = fld dword [ecx+0x3A10] (6 bytes,
 * position independent). Gear veh+0x39FC (-1 R, 0 N, 1..), top gear
 * veh+0x3A04, shift timer veh+0x3A10 (> 0 = shift in progress). */
#define AI_GEAR_SITE      0x00426330u
#define AI_GEAR_STOLEN    6
#define AI_GEAR_SHIFT_FN  0x004EF930u
#define AI_OFF_GEAR       0x39FCu
#define AI_OFF_GEAR_TOP   0x3A04u
#define AI_OFF_SHIFT_T    0x3A10u
typedef void (__attribute__((thiscall)) *AiGearFn)(void *veh);
typedef void (__attribute__((thiscall)) *AiShiftFn)(void *obj, int gear);
static const BYTE g_gear_expect[AI_GEAR_STOLEN] =
    { 0xD9, 0x81, 0x10, 0x3A, 0x00, 0x00 };
static BYTE *g_gear_tramp = NULL;
static int   g_gear_installed = 0;
static volatile LONG g_gear_inflight = 0;
static uint32_t g_gear_up_seen = 0, g_gear_down_seen = 0;
static int g_gear_seen_init = 0;

static void __attribute__((thiscall)) ai_gear_hook(void *veh)
{
    AiOverride *o;
    uintptr_t base;
    InterlockedIncrement(&g_gear_inflight);
    if (InterlockedCompareExchange(&g_hooks_stopping, 0, 0)) {
        ((AiGearFn)g_gear_tramp)(veh);
        InterlockedDecrement(&g_gear_inflight);
        return;
    }
    o = g_ovr;
    if (!o || o->magic != AI_OVR_MAGIC || !o->gear_on
        || o->slot < 1 || o->slot >= MF_AI_MAX
        || (uintptr_t)veh != MF_AIV(o->slot)) {
        g_gear_seen_init = 0;
        ((AiGearFn)g_gear_tramp)(veh);
        InterlockedDecrement(&g_gear_inflight);
        return;
    }
    base = (uintptr_t)veh;
    if (!g_gear_seen_init) {          /* presses before ON do not count */
        g_gear_up_seen = o->gear_up_seq;
        g_gear_down_seen = o->gear_down_seq;
        g_gear_seen_init = 1;
    }
    if (!(*(volatile float*)(base + AI_OFF_SHIFT_T) > 0.0f)) {
        int cur = *(volatile int*)(base + AI_OFF_GEAR);
        int top = *(volatile int*)(base + AI_OFF_GEAR_TOP);
        int want = cur;
        if (o->gear_up_seq != g_gear_up_seen) {
            g_gear_up_seen++;
            if (cur < top) want = cur + 1;
        } else if (o->gear_down_seq != g_gear_down_seen) {
            g_gear_down_seen++;
            if (cur > -1) want = cur - 1;
        }
        if (want != cur)
            ((AiShiftFn)AI_GEAR_SHIFT_FN)((void*)(base + 0xC), want);
    }
    o->hook_calls++;
    InterlockedDecrement(&g_gear_inflight);
}

static int ai_gear_install(void)
{
    g_gear_installed = mf_detour_install(&g_gear_detour, AI_GEAR_SITE, AI_GEAR_STOLEN,
                    g_gear_expect, (const void*)ai_gear_hook, &g_gear_tramp, "AI GEAR");
    return g_gear_installed;
}

static void ai_gear_remove(void)
{
    int ok = mf_detour_remove(&g_gear_detour, &g_gear_inflight, "AI GEAR");
    if (ok) g_gear_installed = 0;
}
#else
static void ai_ovr_apply(uintptr_t veh) { (void)veh; }
static void ai_ovr_open(void) { }
static int  ai_gear_install(void) { return 0; }
static void ai_gear_remove(void) { }
static volatile LONG g_gear_inflight = 0;
#endif

/* 2026-10-05: LOG-ONLY impact probe. Detour on the per-contact collision
 * handler 0x4EC1A0 (CollisionSparkParticleEffects, thiscall phys, args
 * a2, a3, float a4). The game computes impulse = |a4| * [0x7D0014] * 0.01
 * and seizes the engine when it exceeds damage.ini [PHYSICAL] Engine
 * (phys+0x3D84). Player car only (*(phys) == [0x7C54E8]). The hook only
 * records the peak; the worker loop logs it. Changes no game behaviour.
 * Prologue: sub esp,0x58 / push esi / mov esi,ecx (6 bytes, PIC). */
#define IMP_SITE        0x004EC1A0u
#define IMP_STOLEN      6
#define IMP_DMG_MULT    (*(volatile int*)0x007D0014u)
#define IMP_PLAYER_ID   (*(volatile DWORD*)0x007C54E8u)
typedef char (__attribute__((thiscall)) *ImpFn)(void *phys, float *a2,
                                                float *a3, float a4);
static const BYTE g_imp_expect[IMP_STOLEN] =
    { 0x83, 0xEC, 0x58, 0x56, 0x8B, 0xF1 };
static BYTE *g_imp_tramp = NULL;
static int   g_imp_installed = 0;
static volatile LONG g_imp_inflight = 0;
static volatile LONG g_imp_sample_lock = 0;
static MfImpactSample g_imp_sample;
static double g_imp_last_t = -1.0;
/* Burst peak handed to the player model (worker thread only). */
static double g_imp_pending = 0.0;
static double g_imp_pending_t = -1.0;

static char __attribute__((thiscall)) imp_hook(void *phys, float *a2,
                                               float *a3, float a4)
{
    char r;
    InterlockedIncrement(&g_imp_inflight);
    /* live 2026-10-05: F6 invulnerability. Mirror the game's own damage gate in
     * 0x4EC1A0 (disasm 0x4EC1B0..0x4EC1C5): damage runs only when
     * byte 0x7C749F == 0 and not (phys+0x3440 && byte 0x7C74A4). */
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0)
        && phys && *(volatile DWORD*)phys == IMP_PLAYER_ID
        && !*(volatile BYTE*)0x007C749Fu
        && !(*(volatile BYTE*)((BYTE*)phys + 0x3440)
             && *(volatile BYTE*)0x007C74A4u)) {
        float raw = a4 < 0.0f ? -a4 : a4;
        int damage_mult = IMP_DMG_MULT;
        float imp = (float)damage_mult * raw * 0.01f;
        /* Copy thresholds while the native call still owns phys. The worker
         * never keeps/dereferences it. No API/log calls while holding lock. */
        float engine = *(volatile float*)((BYTE*)phys + 0x3D84);
        float aero = *(volatile float*)((BYTE*)phys + 0x3D8C);
        float speed = VEH_SPEED * 3.6f;
        double t = SESSION_TIME;
        while (InterlockedCompareExchange(&g_imp_sample_lock, 1, 0) != 0)
            YieldProcessor();
        mf_impact_add(&g_imp_sample, raw, imp, speed, engine, aero, damage_mult, t);
        InterlockedExchange(&g_imp_sample_lock, 0);
    }
    r = ((ImpFn)g_imp_tramp)(phys, a2, a3, a4);
    InterlockedDecrement(&g_imp_inflight);
    return r;
}

static int imp_install(void)
{
    g_imp_installed = mf_detour_install(&g_imp_detour, IMP_SITE, IMP_STOLEN,
                    g_imp_expect, (const void*)imp_hook, &g_imp_tramp, "IMPACT PROBE");
    return g_imp_installed;
}

static void imp_remove(void)
{
    int ok = mf_detour_remove(&g_imp_detour, &g_imp_inflight, "IMPACT PROBE");
    if (ok) g_imp_installed = 0;
}

/* Worker-thread side (20 Hz): one log line per contact burst, peak values.
 * A burst ends after 0.3 s of session time with no new player contact. */
static void imp_pump(void)
{
    MfImpactSample sample;
    double t = SESSION_TIME;
    int ready;
    if (InterlockedCompareExchange(&g_imp_sample_lock, 1, 0) != 0) return;
    ready = mf_impact_take(&g_imp_sample, t, &sample);
    InterlockedExchange(&g_imp_sample_lock, 0);
    if (!ready) return;
    logf_("IMPACT PROBE: t=%.1f speed=%.0f km/h contacts=%u "
          "peak_impulse=%.0f (raw %.0f, dmg_mult=%d) | engine_seize=%.0f "
          "(%.0f%%) aero_min=%.0f | since_prev=%.1f s",
          t, sample.speed_kmh, sample.contacts, sample.peak, sample.peak_raw,
          sample.damage_mult, sample.engine_seize,
          sample.engine_seize > 0.0f ? 100.0f * sample.peak / sample.engine_seize : 0.0f,
          sample.aero_min,
          (g_imp_last_t < 0.0 || t < g_imp_last_t) ? -1.0 : t - g_imp_last_t);
    g_imp_last_t = t;
    if (sample.peak > g_imp_pending) g_imp_pending = sample.peak;
    g_imp_pending_t = t;
}

/* ==========================================================================
 * L1 (2026-10-05) NATIVE WARNING LAMPS
 * Spec: docs\DEEPSEEK_NATIVE_LAMPS_HUD_CALIBRATION_2026-10-05.txt
 *
 * The game's own dash icons (HUDFlagRendererMain 0x4D82D0, UIMessageCompose
 * 0x4DBA00) and the steering-wheel lamps (sub_49D4C0 this[12]/[15]) are
 * driven by two native predicates:
 *   0x49B9A0  overheat icon + radio eid 77 (oil > LifetimeOilTemp+3r, wear on)
 *   0x49BA50  low-fuel icon (fuel below per-lap use)
 * This batch detours both with the imp_hook pattern:
 *   r = original();
 *   0x49B9A0 -> r || (g_lamp_engine && viewed car == player && PLR_FAILRATE)
 *   0x49BA50 -> r || (g_lamp_fuel && viewed car == player)
 * The native radio (byte_5EC354 / sub_41B190) is NEVER touched: only the
 * predicate result changes. g_lamp_* are written by the worker once per pass
 * (lamp_pump, after the warning pumps) and read-only in the hooks.
 *
 * Prologue (capstone 5.0.7 on the shipped exe, 2026-10-05):
 *   0x49B9A0: A1 DC 54 7C 00       mov eax,[0x7C54DC]  (5 bytes, whole)
 *   0x49BA50: 8B 0D 64 0B 7D 00 / 32 C0
 *                                  mov ecx,[0x7D0B64]; xor al,al (steal 6)
 * A mismatch only disables the lamp feature (logged), never the DLL.
 * ======================================================================== */
#define LAMP_ENG_SITE      0x0049B9A0u
#define LAMP_ENG_STOLEN    5
#define LAMP_FUEL_SITE     0x0049BA50u
#define LAMP_FUEL_STOLEN   6
#define LAMP_VIEWED_SLOT   (*(volatile DWORD*)0x007C54DCu)
#define LAMP_PLAYER_SLOT   (*(volatile DWORD*)0x007C54E4u)
#define LAMP_CAR_ID(p)     (*(volatile DWORD*)((p) + 18580u))

volatile int g_lamp_engine = 0;   /* worker writes, game-thread hooks read */
volatile int g_lamp_fuel = 0;

static const BYTE g_lamp_eng_expect[LAMP_ENG_STOLEN] =
    { 0xA1, 0xDC, 0x54, 0x7C, 0x00 };
static const BYTE g_lamp_fuel_expect[LAMP_FUEL_STOLEN] =
    { 0x8B, 0x0D, 0x64, 0x0B, 0x7D, 0x00 };
static BYTE *g_lamp_eng_tramp = NULL;
static BYTE *g_lamp_fuel_tramp = NULL;
static int   g_lamp_eng_installed = 0;
static int   g_lamp_fuel_installed = 0;
static volatile LONG g_lamp_eng_inflight = 0;
static volatile LONG g_lamp_fuel_inflight = 0;
/* Claude review 2026-10-05: both originals return a bool in AL ONLY
 * (0x49BA26 mov al,bl / 0x49BA82 mov al,1) with a pointer left in the upper
 * EAX bytes - testing the whole int read "true" almost always (icon and lamp
 * stuck on). Typed as unsigned char so only AL is read. */
typedef unsigned char (__attribute__((cdecl)) *MfLampFn)(void);

/* the original predicate's own "viewed car is the player car" test
 * (sub_49B9A0 line 7-8: vehicle array [viewed slot] vs [player slot], +18580) */
static int lamp_viewed_is_player(void)
{
    DWORD v = LAMP_VIEWED_SLOT;
    DWORD p = LAMP_PLAYER_SLOT;
    DWORD *arr = (DWORD*)AI_ACTIVE_PTR_ARRAY;
    if (v >= 0x68u || p >= 0x68u) return 0;
    if (!arr[v] || !arr[p]) return 0;
    return LAMP_CAR_ID(arr[v]) == LAMP_CAR_ID(arr[p]);
}

static int __attribute__((cdecl)) lamp_eng_hook(void)
{
    int r;
    InterlockedIncrement(&g_lamp_eng_inflight);
    r = ((MfLampFn)g_lamp_eng_tramp)();
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0))
        r = r || (g_lamp_engine && PLR_FAILRATE != 0 && lamp_viewed_is_player()
                  && on_circuit());   /* 2026-10-07: mod lamp only on the circuit */
    InterlockedDecrement(&g_lamp_eng_inflight);
    return r;
}

static int __attribute__((cdecl)) lamp_fuel_hook(void)
{
    int r;
    InterlockedIncrement(&g_lamp_fuel_inflight);
    r = ((MfLampFn)g_lamp_fuel_tramp)();
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0))
        r = r || (g_lamp_fuel && lamp_viewed_is_player() && on_circuit());
    InterlockedDecrement(&g_lamp_fuel_inflight);
    return r;
}

/* 2026-10-06 shift lights (owner option 1): detour the shift-light mask
 * sub_49B7A0 (thiscall, this = &unk_7CE234; callers: wheel LEDs 0x49D280
 * and two HUD displays). The native result is kept unless it came from the
 * rpm/limit step branch (recomputed here and equal) while the mod holds a
 * lowered rev band or electronics LIMP MODE caps the revs (lower wins) - then the same steps use the lowered limit, so the bar
 * fills early ("short shift"). Limiter blink and pit sweep stay native.
 * Prologue (capstone, F1 Challenge Online.exe, 2026-10-06):
 *   0x49B7A0: 51              push ecx
 *             A1 E0 54 7C 00  mov eax,[0x7C54E0]   (steal 6) */
#define SHIFT_SITE      0x0049B7A0u
#define SHIFT_STOLEN    6
#define SHIFT_VEH       (*(volatile DWORD*)0x007C54E0u)
static const BYTE g_shift_expect[SHIFT_STOLEN] =
    { 0x51, 0xA1, 0xE0, 0x54, 0x7C, 0x00 };
static MfDetour g_shift_detour;
static BYTE *g_shift_tramp = NULL;
static int   g_shift_installed = 0;
static volatile LONG g_shift_inflight = 0;
typedef int (__attribute__((thiscall)) *MfShiftFn)(void *self);
#ifdef MF_MERGED
extern volatile float g_mf_limp_cap_rads;   /* electronics_failure.c LIMP MODE */
extern volatile DWORD g_mf_limp_cap_tick;
#endif

static int __attribute__((thiscall)) shift_hook(void *self)
{
    int r;
    float lim;
    DWORD veh;
    InterlockedIncrement(&g_shift_inflight);
    r = ((MfShiftFn)g_shift_tramp)(self);
    lim = g_shift_lim;
#ifdef MF_MERGED
    {   /* electronics LIMP MODE caps revs by ignition cut, not the limiter */
        float cap = g_mf_limp_cap_rads;
        if (cap > 100.0f && GetTickCount() - g_mf_limp_cap_tick < 1500u
            && (lim <= 100.0f || cap < lim))
            lim = cap;
    }
#endif
    veh = SHIFT_VEH;
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0)
        && lim > 100.0f && veh && lamp_viewed_is_player() && on_circuit()) {
        float rpm = *(volatile float*)(veh + 0x39A0u);
        float nat = *(volatile float*)(veh + 0x39A4u);
        if (nat > 100.0f && r == mf_shift_mask(rpm / nat))
            r = mf_shift_mask(rpm / lim);
    }
    InterlockedDecrement(&g_shift_inflight);
    return r;
}

static void lamp_install(void)
{
    g_lamp_eng_installed = mf_detour_install(&g_lamp_eng_detour,
        LAMP_ENG_SITE, LAMP_ENG_STOLEN, g_lamp_eng_expect,
        (const void*)lamp_eng_hook, &g_lamp_eng_tramp, "LAMP engine");
    g_lamp_fuel_installed = mf_detour_install(&g_lamp_fuel_detour,
        LAMP_FUEL_SITE, LAMP_FUEL_STOLEN, g_lamp_fuel_expect,
        (const void*)lamp_fuel_hook, &g_lamp_fuel_tramp, "LAMP fuel");
    g_shift_installed = mf_detour_install(&g_shift_detour,
        SHIFT_SITE, SHIFT_STOLEN, g_shift_expect,
        (const void*)shift_hook, &g_shift_tramp, "SHIFT LIGHTS");
}
static void lamp_remove(void)
{
    if (mf_detour_remove(&g_lamp_eng_detour, &g_lamp_eng_inflight, "LAMP engine"))
        g_lamp_eng_installed = 0;
    if (mf_detour_remove(&g_lamp_fuel_detour, &g_lamp_fuel_inflight, "LAMP fuel"))
        g_lamp_fuel_installed = 0;
    if (mf_detour_remove(&g_shift_detour, &g_shift_inflight, "SHIFT LIGHTS"))
        g_shift_installed = 0;
}

/* ---- N7 2026-10-06 (owner-approved): over-rev life-drain owner -------------
 * The game's EngineStressSimulation 0x43EF20 drains the player life with the
 * rpm term 0.5 * 2^((|w| - P+7612) * P+7620) * dt. With the F2004's rpm range
 * an over-rev empties the whole clock in one step (HANDOFF 7.26) - that IS
 * the vanilla over-rev death. This detour NEUTRALISES the part of the rpm
 * drain ABOVE the CURRENT limit (the mod-lowered one while a valve/save band
 * is held) by crediting the same amount back to the life before the game
 * subtracts its full drain. Net effect: over-rev wear = the wear AT the
 * limit; the mod's own over-rev layer (torque-zero "valve hit piston",
 * valvetrain/rod damage, OVER-REV notice/radio) is the only consequence.
 * Prologue (capstone, F1 Challenge Online.exe): 0x43EF20: 55 push ebp;
 * 8B EC mov ebp,esp; 83 E4 F8 and esp,-8 (steal 6). Any other game build is
 * refused by the prologue check at install time. */
#define OVR_SITE        0x0043EF20u
#define OVR_STOLEN      6
static const BYTE g_ovr_expect[OVR_STOLEN] =
    { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
static MfDetour g_ovr_detour;
static BYTE *g_ovr_tramp = NULL;
static int   g_ovr_installed = 0;
static volatile LONG g_ovr_inflight = 0;
static volatile LONG g_ovr_steps = 0;   /* compensated physics steps */
static double g_ovr_comp_life = 0.0;    /* life seconds credited (worker logs) */
typedef void (__attribute__((thiscall)) *MfOvrLifeFn)(void *self);

static void __attribute__((thiscall)) overrev_life_hook(void *self)
{
    InterlockedIncrement(&g_ovr_inflight);
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0)) {
        ULONG_PTR p = (ULONG_PTR)self;
        double life = *(volatile double*)(p + 7604u);
        double w    = *(volatile double*)(p + 7260u);
        double lim  = *(volatile double*)(p + 7388u);
        if (PLR_FAILRATE != 0 && life > 0.0 && life == life
            && w == w && lim == lim && lim > 0.0) {
            double base = *(volatile double*)(p + 7612u);
            double inv  = *(volatile double*)(p + 7620u);
            /* Claude review 2026-10-06: P+104 is a DOUBLE (machine code
             * 0x43F209 fmul qword ptr [esi+0x68]); DS read it as a float. */
            double dt   = *(volatile double*)(p + 104u);
            double ex   = mf_overrev_excess(w, lim, base, inv, dt);
            if (ex > 0.0) {
                *(volatile double*)(p + 7604u) = life + ex;
                g_ovr_comp_life += ex;
                InterlockedIncrement(&g_ovr_steps);
            }
        }
    }
    ((MfOvrLifeFn)g_ovr_tramp)(self);
    InterlockedDecrement(&g_ovr_inflight);
}

static void overrev_owner_install(void)
{
    g_ovr_installed = mf_detour_install(&g_ovr_detour, OVR_SITE, OVR_STOLEN,
        g_ovr_expect, (const void*)overrev_life_hook, &g_ovr_tramp,
        "OVERREV OWNER");
}

static void overrev_owner_remove(void)
{
    if (mf_detour_remove(&g_ovr_detour, &g_ovr_inflight, "OVERREV OWNER"))
        g_ovr_installed = 0;
}

/* Worker: one log per over-rev episode, with the neutralised life seconds. */
static void overrev_owner_pump(void)
{
    static LONG seen = 0;
    static int active = 0;
    static DWORD last_ms = 0;
    LONG steps = InterlockedCompareExchange(&g_ovr_steps, 0, 0);
    DWORD now = game_ms();
    if (!g_ovr_installed) return;
    if (steps != seen) {
        seen = steps;
        active = 1;
        last_ms = now;
        return;
    }
    if (active && (long)(now - last_ms) >= 500) {
        double x = g_ovr_comp_life;
        active = 0;
        g_ovr_comp_life = 0.0;
        if (x >= 0.05)   /* 2026-10-06: no cosmetic "(0.0 s)" lines */
            logf_("OVERREV OWNER: vanilla life drain above the limit neutralised "
                  "(%.1f s)", x);
    }
}

/* Worker side, after the warning pumps: the lamp = OR of the held() dash
 * warnings (mf_lamps.h mapping) + the engine-dead latch. Off on repair /
 * stint end / session over (warn_reset clears the cache); while paused keep
 * the last value (no flicker). One log line per lamp edge. */
/* Claude review 2026-10-05: a warning counts only if its pump evaluated it
 * in this worker pass (see g_w_pass). */
#define LAMP_H(id) (g_w_held[id] && g_w_pass[id] == g_lamp_pass)
static void lamp_pump(void)
{
    static int last_engine = 0;
    static int last_fuel = 0;
    unsigned mask = 0;
    int bits, engine, fuel;

    if (g_pump_frozen) return;        /* keep the last value - no flicker */
    if (SESSION_OVER()) {
        bits = 0;
    } else {
        if (LAMP_H(W_OILLOW))   mask |= 1u << MF_LS_OILLOW;
        if (LAMP_H(W_OILT))     mask |= 1u << MF_LS_OILT;
        if (LAMP_H(W_OILP))     mask |= 1u << MF_LS_OILP;
        if (LAMP_H(W_WATER))    mask |= 1u << MF_LS_WATER;
        if (LAMP_H(W_RADBLOCK)) mask |= 1u << MF_LS_RADBLOCK;
        if (LAMP_H(W_VALVE))    mask |= 1u << MF_LS_VALVE;
        if (LAMP_H(W_EXHTEMP))  mask |= 1u << MF_LS_EXHTEMP;
        if (LAMP_H(W_BLOWBY))   mask |= 1u << MF_LS_BLOWBY;
        if (LAMP_H(C_OILLEAK) || LAMP_H(I_VALVEPIT)
            || LAMP_H(C_OILP) || LAMP_H(C_BLOWBY)
            || LAMP_H(C_WATER))             /* review 2026-10-05: PIT AND RETIRE */
            mask |= 1u << MF_LS_CRITICAL;   /* any "ENGINE CRITICAL" line */
        if (g_engine_dead)        mask |= 1u << MF_LS_DEAD;
        if (LAMP_H(W_FUEL))     mask |= 1u << MF_LS_FUEL;
        if (LAMP_H(W_FUELLEAK)) mask |= 1u << MF_LS_FUELLEAK;
        if (LAMP_H(W_COOLANTLOW)) mask |= 1u << MF_LS_COOLANTLOW;  /* review 2026-10-05 */
        bits = mf_lamp_bits(mask);
    }
    g_lamp_pass++;                    /* next pass counts fresh evaluations only */
    engine = (bits & MF_LAMP_ENGINE) != 0;
    fuel   = (bits & MF_LAMP_FUEL) != 0;
    g_lamp_engine = engine;
    g_lamp_fuel = fuel;
    if (engine != last_engine) {
        if (engine)
            logf_("LAMP: engine ON (%s) water %.1f C base %.1f C coolant %.2f L oil %.1f C",
                  mf_lamp_first_name(mask, MF_LAMP_ENGINE),
                  g_model_in.water_k - 273.15,
                  mf_water_base(&g_model_eng, &g_model_in) - 273.15,
                  g_model_eng.v_w_l, g_model_in.oil_k - 273.15);
        else
            logf_("LAMP: engine OFF");
        last_engine = engine;
    }
    if (fuel != last_fuel) {
        if (fuel)
            logf_("LAMP: fuel ON (%s)", mf_lamp_first_name(mask, MF_LAMP_FUEL));
        else
            logf_("LAMP: fuel OFF");
        last_fuel = fuel;
    }
}

static void po_dg(uintptr_t veh, int r)
{
    DWORD slot;
    if (veh < VEH_BASE || ((veh - VEH_BASE) % AI_VEH_STRIDE) != 0) return;
    slot = (DWORD)mf_islot((int)((veh - VEH_BASE) / AI_VEH_STRIDE));
    if (slot < 1 || slot >= MF_AI_MAX || !g_ai[slot].pulloff.active) return;
    InterlockedIncrement(&g_ai[slot].po_dg[r]);
}

static void po_apply(uintptr_t veh)
{
    DWORD slot;
    uintptr_t base;
    int q1, q2;
    int apply, steer_zero, throttle_zero, brake_set, steer_abs_set;
    float steer_add, brake, steer_abs, brake_cap;

    if (!g_ctl || g_ctl->magic != CTL_MAGIC || !g_ctl->ai_engines_enable) {
        po_dg(veh, PO_DG_CTL);
        return;
    }
    if (SESSION_OVER() || !session_live() || g_pump_frozen) {
        po_dg(veh, PO_DG_FROZEN);
        return;
    }
    if (g_po_verify == 0) {
        int v = po_verify_layout();
        if (v != 0) g_po_verify = v;
        if (v <= 0) return;
    }
    if (g_po_verify < 0) return;
    if (veh < VEH_BASE
        || ((veh - VEH_BASE) % AI_VEH_STRIDE) != 0) {
        if (!g_po_verify_logged) {
            g_po_verify_logged = 1;
            logf_("AI PULLOFF: unexpected AIMainUpdate this %p - feature "
                  "disabled", (void*)veh);
        }
        g_po_verify = -1;
        return;
    }
    slot = (DWORD)mf_islot((int)((veh - VEH_BASE) / AI_VEH_STRIDE));
    if (slot < 1 || slot >= MF_AI_MAX) return;
    q1 = g_ai[slot].po_share.seq;
    if (q1 & 1) { po_dg(veh, PO_DG_SEQ); return; }
    apply = g_ai[slot].po_share.apply;
    steer_zero = g_ai[slot].po_share.steer_zero;
    steer_add = g_ai[slot].po_share.steer_add;
    steer_abs_set = g_ai[slot].po_share.steer_abs_set;
    steer_abs = g_ai[slot].po_share.steer_abs;
    brake_set = g_ai[slot].po_share.brake_set;
    brake = g_ai[slot].po_share.brake;
    brake_cap = g_ai[slot].po_share.brake_cap;
    throttle_zero = g_ai[slot].po_share.throttle_zero;
    q2 = g_ai[slot].po_share.seq;
    if (q1 != q2) { po_dg(veh, PO_DG_SEQ); return; }
    if (!apply) {
        g_ai[slot].po_hk_wrote = 0;
        po_dg(veh, PO_DG_NOAPPLY);
        return;
    }

    base = MF_AIV(slot);
    if (steer_zero) {
        g_ai[slot].po_hk_wrote = 0;
        *(volatile float*)(base + AI_OFF_STEER_CMD) = 0.0f;
        *(volatile float*)(base + AI_OFF_STEER_ACT) = 0.0f;
    } else if (steer_abs_set) {
        /* ROLL (owner live 1): our steer only, the AI's is discarded */
        g_ai[slot].po_hk_wrote = 0;
        *(volatile float*)(base + AI_OFF_STEER_CMD) = steer_abs;
        *(volatile float*)(base + AI_OFF_STEER_ACT) = steer_abs;
    } else if (steer_add != 0.0f) {
        /* additive on the AI's own steer (spec 7.2). Review fix R1: if
         * 0x442C still holds our last write, the original did not rewrite it
         * this tick (early return / park tail) - reuse the stored AI base
         * instead of adding on top of our own output (ramp to full lock). */
        MfAiSlot *a = &g_ai[slot];
        float want = mf_po_steer_compose(
            a->po_hk_wrote, a->po_hk_last, &a->po_hk_base,
            *(volatile float*)(base + AI_OFF_STEER_CMD), steer_add);
        *(volatile float*)(base + AI_OFF_STEER_CMD) = want;
        *(volatile float*)(base + AI_OFF_STEER_ACT) = want;
        a->po_hk_last = want;
        a->po_hk_wrote = 1;
    } else {
        g_ai[slot].po_hk_wrote = 0;
    }
    if (brake_set) {
        po_dg(veh, PO_DG_SET);
        *(volatile float*)(base + AI_OFF_BRAKE_CMD) = brake;
        *(volatile float*)(base + AI_OFF_BRAKE_ACT) = brake;
    } else if (!(brake_cap > 0.0f)) {
        po_dg(veh, PO_DG_PASS);   /* no cap requested: AI brake untouched */
    } else if (*(volatile float*)(base + AI_OFF_BRAKE_CMD) <= brake_cap
               && *(volatile float*)(base + AI_OFF_BRAKE_ACT) <= brake_cap) {
        po_dg(veh, PO_DG_PASS);
    } else {
        po_dg(veh, PO_DG_CAPPED);
        /* live 7 (Albers): AIMainUpdate's stop tail writes the ACTUATOR
         * 0x3584 = 1.0 directly (decompile l.310-312), command untouched */
        /* live 5: the dead-engine AI slams brake 1.0 by itself */
        *(volatile float*)(base + AI_OFF_BRAKE_CMD) = brake_cap;
        *(volatile float*)(base + AI_OFF_BRAKE_ACT) = brake_cap;
    }
    if (throttle_zero) {
        *(volatile float*)(base + AI_OFF_THROTTLE_CMD) = 0.0f;
        *(volatile float*)(base + AI_OFF_THROTTLE_ACT) = 0.0f;
    }
}

static void __attribute__((thiscall)) ai_pulloff_hook(void *veh)
{
    InterlockedIncrement(&g_po_inflight);
    ((AiMainUpdateFn)g_po_tramp)(veh);
    if (!InterlockedCompareExchange(&g_hooks_stopping, 0, 0)) {
        po_apply((uintptr_t)veh);
        ai_ovr_apply((uintptr_t)veh);   /* s20: owner override wins last */
        ai_force_apply((uintptr_t)veh); /* B3 dev: forced-failure intake */
    }
    InterlockedDecrement(&g_po_inflight);
}

static int ai_pulloff_install(void)
{
    g_po_installed = mf_detour_install(&g_po_detour, AI_PULLOFF_SITE, AI_PULLOFF_STOLEN,
                    g_po_expect, (const void*)ai_pulloff_hook, &g_po_tramp, "AI PULLOFF");
    return g_po_installed;
}

static int ai_pulloff_remove(void)
{
    int ok = mf_detour_remove(&g_po_detour, &g_po_inflight, "AI PULLOFF");
    if (ok) g_po_installed = 0;
    g_po_verify = 0;
    return ok;
}

/* ---- E9-H5: E9q per-car data path (log ONLY, model G stays 1.0) ---------
 * Live FACT (finding veh-4678-car-4714-veh-path-live-2026-10-01): every live
 * vehicle struct carries its .veh path at +0x4714. Resolution rule, derived
 * from the installed data and documented here:
 *   .veh  -> HDVehicle=<name>.hdv   (same folder as the .veh)
 *   .hdv  -> [ENGINE] Normal=<name> (RestrictorPlate= is the air-restricted
 *            variant; when they differ both are logged, Normal is used)
 *   engine ini -> <hdv folder>\<name>[.ini]; if missing there, a bounded
 *            search of SeasonData\Vehicles stands in for the game FileFinder.
 * Once per session per slot, on the WORKER thread; no parsing in a hook;
 * every read is capped at GRADE_FILE_CAP; a missing file logs and skips.
 * The computed G is LOGGED ONLY - the model keeps grade 1.0 until the live
 * check (Claude) wires it. */
#define GRADE_SRC_MAX   MF_AI_MAX
#define GRADE_VEH_PATH  0x4714u
#define GRADE_FILE_CAP  (256u * 1024u)

static int      g_gradesrc_done = 0;
static int      g_gradesrc_tries = 0;
static int      g_gradesrc_any = 0;   /* J5: a slot has had a readable path */
static int      g_grade_attempted[GRADE_SRC_MAX];
static double   g_grade_avg[GRADE_SRC_MAX];
static double   g_grade_var[GRADE_SRC_MAX];
static int      g_grade_valid[GRADE_SRC_MAX];

static const char *mf_stristr_(const char *h, const char *n)
{
    size_t nl;
    if (!h || !n || !*n) return NULL;
    nl = strlen(n);
    for (; *h; h++) {
        size_t i;
        for (i = 0; i < nl; i++)
            if (!h[i]
                || tolower((unsigned char)h[i]) != tolower((unsigned char)n[i]))
                break;
        if (i == nl) return h;
    }
    return NULL;
}

/* first line-start `key=` value after `text`; comment markers and spaces end
 * an unquoted value, quotes are unwrapped. Returns 1 when found. */
static int mf_ini_value(const char *text, const char *key, char *out, int cap)
{
    const char *p = text;
    size_t klen = strlen(key);
    if (out && cap > 0) out[0] = 0;
    if (!text || !out || cap < 1 || klen < 1) return 0;
    while ((p = mf_stristr_(p, key)) != NULL) {
        const char *ls = p;
        const char *v;
        int n = 0;
        while (ls > text && ls[-1] != '\n') ls--;
        while (*ls == ' ' || *ls == '\t') ls++;
        if (ls != p || p[klen] != '=') { p += klen; continue; }
        v = p + klen + 1;
        while (*v == ' ' || *v == '\t') v++;
        if (*v == '"') {
            v++;
            while (*v && *v != '"' && *v != '\r' && *v != '\n' && n < cap - 1)
                out[n++] = *v++;
        } else {
            int par = 0;               /* keep spaces inside (a, b) values */
            while (*v && *v != '\r' && *v != '\n' && n < cap - 1) {
                if (*v == '(') par++;
                else if (*v == ')') par--;
                if (!par && (*v == ' ' || *v == '\t' || *v == '/')) break;
                out[n++] = *v++;
            }
        }
        out[n] = 0;
        return n > 0;
    }
    return 0;
}

static long mf_read_capped(const char *path, char *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, (size_t)(cap - 1), f);
    fclose(f);
    if (n < 0) n = 0;
    buf[n] = 0;
    return n;
}

/* path join with explicit capacity (keeps GCC's -Wformat-truncation analysis
 * on pointer parameters instead of fixed arrays, where it cannot prove an
 * overflow - the callers size their buffers generously). */
static void mf_path_join(char *out, size_t cap, const char *dir, const char *name)
{
    snprintf(out, cap, "%s\\%s", dir, name);
}

/* bounded recursive search of SeasonData\Vehicles for a file name (the game
 * FileFinder lookup for a shared engine file); depth and directory count are
 * capped so a slot can never walk the whole drive. */
static int mf_find_file(const char *dir, const char *name, char *out, int cap,
                        int depth, int *budget)
{
    char pattern[MAX_PATH * 4 + 16];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (!dir || !name || *budget <= 0) return 0;
    mf_path_join(pattern, sizeof pattern, dir, "*");
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth > 0) {
                char sub[MAX_PATH * 4 + 16];
                (*budget)--;
                mf_path_join(sub, sizeof sub, dir, fd.cFileName);
                if (mf_find_file(sub, name, out, cap, depth - 1, budget)) {
                    FindClose(h);
                    return 1;
                }
            }
        } else if (_stricmp(fd.cFileName, name) == 0) {
            mf_path_join(out, (size_t)cap, dir, fd.cFileName);
            FindClose(h);
            return 1;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
}

static void grade_src_parse_slot(int slot, const char *vpath)
{
    static char buf[GRADE_FILE_CAP];
    char val[64], rpm0[48], oil0[48];
    char norm[64], rest[64], eng[80];
    char folder[MAX_PATH], path[MAX_PATH * 4 + 16];
    const char *slash;
    const char *engsec;
    double avg, var;
    long n;

    n = mf_read_capped(vpath, buf, (long)sizeof buf);
    if (n < 0) {
        logf_("GRADE SRC: slot %d %s - .veh not readable, skipped", slot, vpath);
        return;
    }
    if (!mf_ini_value(buf, "HDVehicle", val, sizeof val)) {
        logf_("GRADE SRC: slot %d %s - no HDVehicle=, skipped", slot, vpath);
        return;
    }
    slash = strrchr(vpath, '\\');
    if (!slash) {
        logf_("GRADE SRC: slot %d %s - no folder in path, skipped", slot, vpath);
        return;
    }
    n = (long)(slash - vpath);
    if (n >= (long)sizeof folder) n = (long)sizeof folder - 1;
    memcpy(folder, vpath, (size_t)n);
    folder[n] = 0;
    mf_path_join(path, sizeof path, folder, val);
    n = mf_read_capped(path, buf, (long)sizeof buf);
    if (n < 0) {
        logf_("GRADE SRC: slot %d %s - hdv %s not readable, skipped", slot,
              vpath, val);
        return;
    }
    engsec = mf_stristr_(buf, "[ENGINE]");
    if (!engsec) {
        logf_("GRADE SRC: slot %d %s - hdv has no [ENGINE], skipped", slot, vpath);
        return;
    }
    {
        int has_n = mf_ini_value(engsec, "Normal", norm, sizeof norm);
        int has_r = mf_ini_value(engsec, "RestrictorPlate", rest, sizeof rest);
        if (has_n && has_r && _stricmp(norm, rest) != 0)
            logf_("GRADE SRC: slot %d %s - Normal=%s RestrictorPlate=%s "
                  "(using Normal)", slot, vpath, norm, rest);
        if (has_n) snprintf(eng, sizeof eng, "%s", norm);
        else if (has_r) snprintf(eng, sizeof eng, "%s", rest);
        else {
            logf_("GRADE SRC: slot %d %s - no [ENGINE] entry, skipped", slot, vpath);
            return;
        }
    }
    if (!strchr(eng, '.'))
        snprintf(eng + strlen(eng), sizeof eng - strlen(eng), ".ini");
    mf_path_join(path, sizeof path, folder, eng);
    n = mf_read_capped(path, buf, (long)sizeof buf);
    if (n < 0) {
        char found[MAX_PATH * 4 + 16];
        int budget = 400;
        if (mf_find_file("SeasonData\\Vehicles", eng, found, sizeof found, 3,
                         &budget)) {
            snprintf(path, sizeof path, "%s", found);
            n = mf_read_capped(path, buf, (long)sizeof buf);
        }
    }
    if (n < 0) {
        logf_("GRADE SRC: slot %d %s - engine %s not found, skipped", slot,
              vpath, eng);
        return;
    }
    if (!mf_ini_value(buf, "LifetimeAvg", val, sizeof val)) {
        logf_("GRADE SRC: slot %d %s - engine %s has no LifetimeAvg, skipped",
              slot, vpath, eng);
        return;
    }
    avg = atof(val);
    if (!mf_ini_value(buf, "LifetimeVar", val, sizeof val)) {
        logf_("GRADE SRC: slot %d %s - engine %s has no LifetimeVar, skipped",
              slot, vpath, eng);
        return;
    }
    var = atof(val);
    if (!(avg > 1.0) || var < 0.0) {
        logf_("GRADE SRC: slot %d %s - engine %s lifetime invalid (avg=%.1f "
              "var=%.1f), skipped", slot, vpath, eng, avg, var);
        return;
    }
    rpm0[0] = oil0[0] = 0;
    {
        /* J3a: per-car thermal values from the SAME engine.ini. Stored in the
         * model's units (rad/s, K); therm_valid only when the values pass the
         * mf_car_derive bounds, checked by passing them and reading back its
         * fallback flags (no duplicated magic numbers). */
        int has_rpm = mf_ini_value(buf, "LifetimeEngineRPM", rpm0, sizeof rpm0);
        int has_oil = mf_ini_value(buf, "LifetimeOilTemp", oil0, sizeof oil0);
        char opt0[48];
        int has_opt = mf_ini_value(buf, "OptimumOilTemp", opt0, sizeof opt0);
        double rb = 0.0, rr = 0.0, ob = 0.0, orr = 0.0, oo = 0.0;
        int ok = has_rpm && mf_pair_parse(rpm0, &rb, &rr)
              && has_oil && mf_pair_parse(oil0, &ob, &orr);
        if (ok) {
            rb *= MF_RPM_TO_RADS;
            rr *= MF_RPM_TO_RADS;
            ob += 273.15;
            /* the oil RANGE is a delta: no C->K offset on it */
        }
        if (ok && has_opt) {
            char *end;
            oo = strtod(opt0, &end);
            if (end == opt0 || !isfinite(oo))
                ok = 0;
            else
                oo += 273.15;
        }
        if (ok) {
            /* bits 2/4 = rpm base/range, 8/16 = oil base/range, 32 = oil
             * optimum (only counted when present). rev_limit 0 sets its own
             * fallback (bit 1, ignored - the live rev limit arrives with the
             * AI step and only widens the rpm bound). */
            MfCar tc;
            unsigned fl = mf_car_derive(&tc, 0.0, rb, rr, ob, orr, oo,
                                        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            if ((fl & (2u | 4u | 8u | 16u)) || (has_opt && (fl & 32u)))
                ok = 0;
        }
        g_grade_therm_valid[slot] = ok;
        if (ok) {
            g_grade_rpm_base[slot] = rb;
            g_grade_rpm_range[slot] = rr;
            g_grade_oil_base[slot] = ob;
            g_grade_oil_range[slot] = orr;
            g_grade_oil_opt[slot] = has_opt ? oo : 0.0;
            snprintf(g_grade_eng_path[slot], sizeof g_grade_eng_path[slot],
                     "%s", path);
        }
        logf_("GRADE SRC: slot %d %s -> %s avg=%.0f var=%.0f rpm=%s oil=%s "
              "therm=%s", slot, vpath, path, avg, var, rpm0, oil0,
              ok ? "ok" : "rejected (model fallbacks)");
    }
    g_grade_avg[slot] = avg;
    g_grade_var[slot] = var;
    g_grade_valid[slot] = 1;
}

static void grade_src_pump(void)
{
    int slot, pending = 0;

    if (g_gradesrc_done) return;
    for (slot = 0; slot < GRADE_SRC_MAX; slot++) {
        const char *vpath;
        if (g_grade_attempted[slot]) continue;
        /* live 2026-10-01 (Claude): veh+0x4714 holds the path TEXT inline
         * (finding veh-4678-car-4714-veh-path-live-2026-10-01), not a pointer -
         * reading it as a pointer found no car ("field of 0 cars"). */
        vpath = (const char*)(uintptr_t)(MF_AIV(slot)
                                         + GRADE_VEH_PATH);
        if (IsBadReadPtr(vpath, 260) || !vpath[0]
            || !memchr(vpath, 0, 260)) {
            pending = 1;      /* car not loaded yet - retry next tick */
            continue;
        }
        g_grade_attempted[slot] = 1;
        g_gradesrc_any = 1;
        grade_src_parse_slot(slot, vpath);
    }
    /* J5 (live bug 2026-10-01): the first session after load hit the 600-try
     * timeout before the cars were loaded -> "field of 0 cars". The cap starts
     * counting only once at least one slot had a readable path; without one
     * there is nothing to parse and the pump simply retries. */
    if (g_gradesrc_any) g_gradesrc_tries++;
    if (!pending || (g_gradesrc_any && g_gradesrc_tries >= 600)) {   /* 30 s at 20 Hz */
        double avg[GRADE_SRC_MAX], var[GRADE_SRC_MAX], g[GRADE_SRC_MAX];
        int i, nfield = 0;
        for (i = 0; i < GRADE_SRC_MAX; i++) {
            avg[i] = g_grade_valid[i] ? g_grade_avg[i] : 0.0;
            var[i] = g_grade_valid[i] ? g_grade_var[i] : 0.0;
            if (g_grade_valid[i]) nfield++;
        }
        mf_grade_field(avg, var, GRADE_SRC_MAX, g);
        for (i = 0; i < GRADE_SRC_MAX; i++) {
            if (g_grade_valid[i])
                logf_("GRADE: slot %d G=%.3f (avg=%.0f var=%.0f)", i, g[i],
                      avg[i], var[i]);
            else
                logf_("GRADE: slot %d no data (model G stays 1.000)", i);
        }
        logf_("GRADE: field of %d cars (log only - model G is still 1.0)",
              nfield);
        g_gradesrc_done = 1;
    }
}

static int g_replay_logged = 0;   /* audit MF-12: replay enter/exit logged once */
static void model_pump(void)
{
    DWORD now = game_ms();   /* E2b: game clock - pause and CTRL+T behave */
    double dt;

    g_model_stepped = 0;     /* R1a: only a real mf_model_step sets this */
    if (REPLAY_MODE == 2) {
        /* audit MF-12: no stepping, no writes during replay playback;
         * re-base so the return to live is not seen as a clock jump */
        if (!g_replay_logged) {
            g_replay_logged = 1;
            logf_("REPLAY: playback - model idle");
        }
        g_model_last_ms = now;
        return;
    }
    if (g_replay_logged) {
        g_replay_logged = 0;
        logf_("REPLAY: back to live");
    }
    if (SESSION_OVER()) {
        /* E9g item 5 guard + H3b/R07: the ONE invalidation path runs BEFORE
         * this return. drop=1 because the session is over: the releases must
         * DROP their records, never write into the ended session. */
        if (g_model_live) engine_invalidate("session over", 1);
        return;
    }
    if (!session_live()) {
        if (g_model_live) {
            ai_session_stats();       /* E9p: one summary line set per session */
            /* H3b: session change; releases write stock (same generation),
             * then the bump makes anything still held stale. */
            engine_invalidate("left the session", 0);
            logf_("MODEL: left the session - shadow model paused");
        }
        return;
    }
    if (!g_model_live) {
        g_model_live = 1;
        session_gen_bump();           /* H1c: a new session generation */
        g_model_last_ms = now;
        g_model_log_ms = now;
        g_power_mult = 1.0f;      /* E3: session change resets the power lever */
        g_loss_top = 0.0f;        /* E9e: session change resets the shape */
        g_loss_all = 0.0f;
        g_loss_load = 0.0f;
        g_v_throttle = 0.0f;
        g_misfire_depth = 0.0f;   /* E9d: session change resets the misfire */
        g_dmg_log_t = 0.0;        /* P1: damage log cadence per engine */
        g_shadow_logged = 0;      /* P2: shadow fire log per engine */
        engine_dead_clear("session");   /* E9g-e item 1: new session is alive */
        save_reset("session");    /* E9i: save mode is off in a new session */
        g_native_touched = 0;     /* E8b: session change resets the flag */
        model_levers_release();   /* E8b/E9e: stock values on a new session */
        model_reset((uint32_t)now ^ (uint32_t)(SESSION_TYPE * 2654435761u));
        ai_session_reset();       /* E9p: no AI state across sessions */
        /* E9-H5: the E9q per-car parse runs once per session per slot */
        g_gradesrc_done = 0;
        g_gradesrc_tries = 0;
        g_gradesrc_any = 0;
        memset(g_grade_attempted, 0, sizeof g_grade_attempted);
        memset(g_grade_valid, 0, sizeof g_grade_valid);
        memset(g_grade_therm_valid, 0, sizeof g_grade_therm_valid);
        memset(g_grade_eng_path, 0, sizeof g_grade_eng_path);
        g_model_car_valid = 0;    /* E8e: wait for the game to fill the car */
        logf_("MODEL: session live (type=%d) - model on, waiting for car values "
              "(E8e); auto source = game clock (E5), failure rate mode %d",
              SESSION_TYPE, PLR_FAILRATE);
        mfu_settings(SESSION_TYPE, PLR_FAILRATE,
                     g_ctl ? g_ctl->ai_engines_enable : 0,
                     g_ctl ? g_ctl->overrev_enable : 0,
                     g_ctl ? g_ctl->niggle_enable : 0,
                     (int)(mfa_user * 100.0f + 0.5f), g_voice_on);
        return;
    }
    if (now < g_model_last_ms) {
        /* H3b/R07: a BACKWARD clock (session restart / reload between worker
         * samples) is a full invalidation, not just a re-based tick. drop=1
         * so no old value is written into whatever engine the clock now
         * belongs to. */
        engine_invalidate("clock rollback", 1);
        g_model_last_ms = now;
        return;
    }
    dt = (double)(now - g_model_last_ms) / 1000.0;
    g_model_last_ms = now;
    /* E9o: at CTRL+T x30 one pump covers ~1.5 s of game time; only a real
     * clock jump (dt <= 0, or > MF_DT_MAX_S = 10 s) is skipped and re-based.
     * 0 < dt <= MF_DT_MAX_S is stepped - the model sub-steps at 10 ms. */
    if (dt <= 0.0 || dt > MF_DT_MAX_S) return;

    /* E9p: AI shadow engines share the same game-time dt. This runs before
     * the player-car-valid gate so a noPlayer session still steps them; the
     * player model itself stays idle without valid car values. */
    mfc_engine_pump();    /* CO-OP sync (returns at once in single-player) */
    ai_engines_pump(dt);

    /* E9-H5: E9q per-car data path - once per session per slot, log only.
     * Also before the car-valid gate (the .veh paths do not need it). */
    grade_src_pump();

    /* E8e: derive and step only on real car values. Until they are valid the
     * model is not stepped at all (no onset, no outputs). */
    if (!g_model_car_valid) {
        if (!model_car_raw_valid()) return;
        g_model_car_valid = 1;
        g_model_car_base_snapshot = LT_RPM_BASE;
        g_model_car_oil_snapshot = LT_OIL_BASE;
        model_car_log();
    } else if (model_car_raw_valid() && model_car_changed()) {
        /* H3a: release our rev-band record FIRST (a different car's values
         * make the restore skip and drop; the same car reloaded restores the
         * stock band) - the new derive must never read our own lowering. */
        rev_band_restore("REV BAND");
        g_model_car_base_snapshot = LT_RPM_BASE;
        g_model_car_oil_snapshot = LT_OIL_BASE;
        logf_("MODEL: car values changed >0.5%% (rpm base %.1f, oil base %.1f) "
              "- re-deriving (rev limit now %.1f)", LT_RPM_BASE, LT_OIL_BASE,
              ENG_REV_LIMIT);
        model_car_log();
    }

    model_inputs(&g_model_in, dt);
    /* 2026-10-05: player impacts come from the game's contact impulse (the
     * 0x4EC1A0 probe). A peak older than 2 s of session time (or from before
     * a clock rollback) is stale and dropped. Without the probe hook the
     * legacy g check stays in charge. */
    g_model_in.impulse_src = g_imp_installed;
    g_model_in.impulse = 0.0;
    if (g_imp_pending > 0.0) {
        double age = SESSION_TIME - g_imp_pending_t;
        if (age >= 0.0 && age <= 2.0) g_model_in.impulse = g_imp_pending;
        g_imp_pending = 0.0;
    }
    mf_model_step(&g_model_eng, &g_model_car, &g_model_in, &g_model_out);
    g_model_stepped = 1;     /* R1a: outputs below are from a real step */

    /* P1 (ADDENDUM 2): one component-damage line per 60 s of model time. */
    if (g_model_eng.t_s - g_dmg_log_t >= 60.0) {
        g_dmg_log_t = g_model_eng.t_s;
        dmg_log_(-1, &g_model_eng);
    }

    /* P2 (ADDENDUM 2): hazard onset shadow (no behaviour) - log once. Q3:
     * when the hazard actually APPLIED the onset (onset_hazard) the ONSET
     * line below is the record; the shadow line would be misleading. */
    if (g_model_eng.shadow_fired && !g_shadow_logged
        && !g_model_eng.onset_hazard) {
        g_shadow_logged = 1;
        logf_("HAZARD (shadow): slot 0 x=%.3f t=%.0f would-fault=%s (%d)",
              g_model_eng.shadow_x, SESSION_TIME,
              dmg_fault_name_(g_model_eng.shadow_fault),
              g_model_eng.shadow_fault);
    }

    /* Q3 (ADDENDUM 3): the player's onset - log once which path fired it
     * (hazard vs countdown backstop), x = dmg_total/budget and the budget the
     * fairness floor and warnings use. */
    if (g_model_eng.onset && !g_player_onset_logged) {
        double x = g_model_eng.budget_s > 0.0
                 ? g_model_eng.dmg_total / g_model_eng.budget_s : 0.0;
        g_player_onset_logged = 1;
        /* 2026-10-06: automatic onsets need a warm engine; a manual test arm
         * on a cold engine is allowed but its timings differ - say so. */
        if (!mf_engine_warm(&g_model_eng, &g_model_car, &g_model_in))
            logf_("ONSET: engine COLD (oil %.1f C < warm %.1f C, %.0f s running) - "
                  "symptom timings differ from a warm engine",
                  g_model_in.oil_k - 273.15,
                  g_model_car.oil_opt_k - MF_WARM_OIL_DK - 273.15,
                  g_model_eng.drain_warm_s);
        /* V3: ENG01 - an onset with no specific line (defensive; every
         * current fault type 1..9 has its own pre line and maps elsewhere) */
        if (mfn_onset_unmapped(g_model_eng.fault))
            voice_say(MFV_ENG01, MFR_P_INFO);
        /* R1c (ADDENDUM 9): an impact-caused onset is not a countdown - the
         * arm came from mf_impact_step in the same step (out.impact_fault is
         * the fault it applied). Live 2026-10-01: the off-track hit logged
         * "ONSET: countdown ... fault=oilleak" although the cause was the
         * impact. */
        if (g_model_out.impact_fired
            && g_model_out.impact_fault == g_model_eng.fault) {
            logf_("ONSET: impact %s=%.1f x=%.3f t=%.0f fault=%s (%d) sev=%d "
                  "budget=%.0f s",
                  g_model_out.impact_impulse >= 0.0 ? "impulse" : "g",
                  g_model_out.impact_impulse >= 0.0
                      ? g_model_out.impact_impulse : g_model_out.impact_g,
                  x, SESSION_TIME,
                  dmg_fault_name_(g_model_eng.fault), g_model_eng.fault,
                  g_model_eng.severity, g_model_eng.ttf_onset_s);
        } else {
            logf_("ONSET: %s x=%.3f t=%.0f fault=%s (%d) sev=%d budget=%.0f s",
                  g_model_eng.onset_hazard ? "hazard" : "countdown", x,
                  SESSION_TIME, dmg_fault_name_(g_model_eng.fault),
                  g_model_eng.fault, g_model_eng.severity,
                  g_model_eng.ttf_onset_s);
        }
    }

    /* E9-H2b: an emergency onset fired (short-horizon drain while the normal
     * warm/confirm path could not fire in time). Log once per engine with the
     * budget it was anchored on; the felt symptom is already in MfOut. */
    if (g_model_eng.emergency_onset && !g_emergency_logged) {
        g_emergency_logged = 1;
        logf_("MODEL: emergency onset (short-horizon drain %.1f s/s, ttf %.1f s) "
              "fault=%d sev=%d", g_model_eng.drain_onset_sps,
              g_model_eng.ttf_onset_s, g_model_eng.fault,
              g_model_eng.severity);
    }

    /* E9a: the game re-armed its own staged failure (life ~0 -> positive);
     * log once. The model fired an onset anchored on the re-armed life (or
     * re-based the active one); bank1_set now spares the game's own smoke. */
    if (g_model_eng.native_stage1 && !g_model_stage1_logged) {
        g_model_stage1_logged = 1;
        logf_("MODEL: native stage one (game re-armed its own failure) - "
              "life=%.1f onset=%d fault=%d sev=%d (E9a)",
              g_model_in.life_s, g_model_eng.onset,
              g_model_eng.fault, g_model_eng.severity);
    }

    /* E9g-c/N6: the game zeroed the clock while off track (return to garage
     * / restart). The model counted it as a reset, not a failure. N6
     * 2026-10-06 (owner-approved): this is the game re-seeding the car - the
     * ONLY remaining automatic clear (with session leave / race retirement);
     * the pit lane never clears anything. */
    if (g_model_eng.life_resets != g_model_liferesets_seen) {
        g_model_liferesets_seen = g_model_eng.life_resets;
        logf_("MODEL: life reset to 0 (not a failure) - fault cleared (game reset)");
        stint_clear("game reset");
        return;
    }

    /* E9b-2 item 6: early random failure just started (once per engine). */
    if (g_model_out.early_fired && !g_early_logged) {
        g_early_logged = 1;
        logf_("EARLY: fault %d sev %d budget %.0f s (drain x%.2f, q %.2f)",
              g_model_eng.fault, g_model_eng.severity,
              g_model_eng.ttf_onset_s,
              mf_clamp(g_model_eng.drain_ema_sps, 1.0, MF_EARLY_ABUSE_MAX),
              g_model_eng.quality);
    }

    /* E9b-2 item 7: the first cascade of this engine (logged once). */
    if (g_model_out.cascade_kind && g_model_out.cascade_kind != g_cascade_seen) {
        g_cascade_seen = g_model_out.cascade_kind;
        logf_("CASCADE: %s -> %s",
              g_model_eng.fault == MF_FAULT_OIL_LEAK ? "oil leak"
              : g_model_eng.fault == MF_FAULT_RAD_DEBRIS ? "radiator debris"
              : "water leak",
              g_model_out.cascade_kind == 1 ? "bearings" : "head gasket");
    }

    /* E9h: an impact was detected - log its severity and the rolled fault
     * (0 = nothing / the game's own native seizure). */
    if (g_model_out.impact_fired) {
        /* V3: an impact that rolled no fault -> HIT01; an impact that armed a
         * fault -> HIT02, which replaces that fault's first diag call (the
         * one pre-critical slot is spent here). */
        if (g_voice_on) {
            if (mfn_impact_armed(&g_model_out)) {
                voice_say(MFV_HIT02, MFR_P_INFO);
                g_radio_pre_done = 1;
            } else if (mfn_impact_only(&g_model_out)) {
                voice_say(MFV_HIT01, MFR_P_INFO);
            }
        }
        if (g_model_out.impact_impulse >= 0.0)
            logf_("IMPACT impulse=%.0f chance=%.0f%% -> fault %d",
                  g_model_out.impact_impulse,
                  100.0 * g_model_out.impact_chance,
                  g_model_out.impact_fault);
        else
            logf_("IMPACT g=%.1f -> fault %d", g_model_out.impact_g,
                  g_model_out.impact_fault);
    }

    /* E4: outputs live - the power product drives the E3 lever (4.8), the
     * terminal states fire the proven end states, diagnostics published */
    g_power_mult = (float)g_model_out.power_mult;
    /* E9e: mirror the torque-curve losses for the fuelcut hook and run the
     * engine.ini levers (gasket coupling, lean heat, oil cooling, rough idle) */
    g_loss_top = (float)g_model_out.loss_top;
    g_loss_all = (float)g_model_out.loss_all;
    g_loss_load = (float)g_model_out.loss_load;
    g_v_throttle = (float)g_model_in.load;
    /* E9d: the game's own symptoms - misfire mirror, gauge, friction, burn */
    g_misfire_depth = (float)g_model_out.misfire_depth;
    g_misfire_discrete = g_model_out.misfire_discrete ? 1 : 0;   /* H2e */
    oil_gauge_pump();
    friction_pump();
    fuel_burn_pump(dt);
    niggle_pump();           /* E9b item 3: harmless niggles */
    owt_coupling_pump();
    lean_heat_pump();
    oil_min_cool_pump();
    rough_idle_pump();
    model_terminal_pump();
    model_clockout_pump();   /* E8g item 5: game-clock failure presentation */
    rev_band_pump();         /* E9i: one band owner (valve ceiling + save) */
    model_publish();

    if ((long)(now - g_model_log_ms) >= 5000) {
        double ttf = g_model_eng.drain_ema_sps > MF_DRAIN_MIN_SPS
                   ? g_model_in.life_s / g_model_eng.drain_ema_sps : -1.0;
        g_model_log_ms = now;
        logf_("MODEL 5s: ttf=%.1f life=%.1f drain=%.4f warm=%.1f onset=%d fault=%d sev=%d "
              "stab=%d h_vt=%.4f h_b=%.4f ring=%.4f h_rod=%.4f h_g=%.4f w_l=%.2f oil_l=%.2f "
              "fuel_lost=%.2f",
              ttf, g_model_in.life_s, g_model_eng.drain_ema_sps,
              g_model_eng.drain_warm_s,
              g_model_eng.onset, g_model_eng.fault, g_model_eng.severity,
              g_model_eng.stabilised, g_model_eng.h_vt, g_model_eng.h_b,
              g_model_eng.ring_s, g_model_eng.h_rod, g_model_eng.h_g,
              g_model_eng.v_w_l, g_model_eng.v_oil_l,
              g_model_eng.fuel_lost_l);   /* N3 H16: litres lost to the leak */
        /* 2026-10-05 calibration: every 5 s the temperatures the warnings use */
        logf_("TEMPS 5s: water %.1f C base %.1f C oil %.1f C oil_base %.1f C opt %.1f C native_hot %.1f C "
              "gauge %.2f p_oil %.2f speed %.0f km/h rpm %.0f",
              g_model_in.water_k - 273.15,
              mf_water_base(&g_model_eng, &g_model_in) - 273.15,
              g_model_in.oil_k - 273.15, g_model_eng.oil_base_k - 273.15,
              g_model_car.oil_opt_k - 273.15,
              (*(volatile double*)0x007C9204u) - 273.15,
              g_model_out.oil_gauge_scale, model_oil_pressure(),
              g_model_in.speed_mps * 3.6, g_model_in.rpm_rads * 60.0 / 6.2831853);
    }
}

/* E9g-b item 3: with no timed session (menus, garage, after the session-end
 * jump) a GUI command must not reach game memory. Pending commands are
 * logged once and cleared here - never queued for the next session. */
/* 2026-10-06 ship: player settings, mechanical_failures.ini next to the DLL.
 * Read once at start, AFTER the defaults; a missing file/key keeps the
 * default. The test panel can still change the same fields live. Failure
 * frequency itself follows the game's own Failure Rate option (vanilla+). */
static void mf_user_ini_load(void)
{
    char ini[MAX_PATH];
    int v;
    if (!g_ctl || !mf_user_ini_path(ini)) return;
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) {
        logf_("INI: %s missing - defaults", ini);
        return;
    }
    g_ctl->ai_engines_enable =
        GetPrivateProfileIntA("Engine", "AIFailures", 1, ini) ? 1 : 0;
    g_ctl->overrev_enable =
        GetPrivateProfileIntA("Engine", "OverRevDamage", 1, ini) ? 1 : 0;
    g_ctl->niggle_enable =
        GetPrivateProfileIntA("Engine", "MinorIssues", 1, ini) ? 1 : 0;
    g_mfc_sync = GetPrivateProfileIntA("CoOp", "SyncFailures", 1, ini) ? 1 : 0;
    g_mfc_sync_retire = GetPrivateProfileIntA("CoOp", "SyncRetire", 1, ini) ? 1 : 0;
    v = GetPrivateProfileIntA("Sound", "EffectsVolume", 100, ini);
    if (v < 0) v = 0;
    if (v > 200) v = 200;
    mfa_user = (float)v / 100.0f;
    logf_("INI: AIFailures %d OverRevDamage %d MinorIssues %d EffectsVolume %d%%",
          g_ctl->ai_engines_enable, g_ctl->overrev_enable,
          g_ctl->niggle_enable, v);
}

static void gui_cmds_drop(void)
{
    if (g_ctl->cmd_stage1)      { g_ctl->cmd_stage1 = 0;      logf_("CMD IGNORED (no session): stage1"); }
    if (g_ctl->cmd_terminal)    { g_ctl->cmd_terminal = 0;    logf_("CMD IGNORED (no session): terminal"); }
    if (g_ctl->cmd_instant)     { g_ctl->cmd_instant = 0;     logf_("CMD IGNORED (no session): instant"); }
    if (g_ctl->cmd_seizure)     { g_ctl->cmd_seizure = 0;     logf_("CMD IGNORED (no session): seizure"); }
    if (g_ctl->cmd_repair)      { g_ctl->cmd_repair = 0;      logf_("CMD IGNORED (no session): repair"); }
    if (g_ctl->cmd_dump)        { g_ctl->cmd_dump = 0;        logf_("CMD IGNORED (no session): dump"); }
    if (g_ctl->cmd_emit_on >= 0) { g_ctl->cmd_emit_on = -1;   logf_("CMD IGNORED (no session): emit_on"); }
    if (g_ctl->cmd_emit_clear)  { g_ctl->cmd_emit_clear = 0;  logf_("CMD IGNORED (no session): emit_clear"); }
    if (g_ctl->cmd_telemetry)   { g_ctl->cmd_telemetry = 0;   logf_("CMD IGNORED (no session): telemetry"); }
    if (g_ctl->cmd_cutout_toggle) { g_ctl->cmd_cutout_toggle = 0; logf_("CMD IGNORED (no session): cutout"); }
    if (g_ctl->cmd_oil_toggle)  { g_ctl->cmd_oil_toggle = 0;  logf_("CMD IGNORED (no session): oil"); }
    if (g_ctl->cmd_set_timer)   { g_ctl->cmd_set_timer = 0;   logf_("CMD IGNORED (no session): set_timer"); }
    if (g_ctl->cmd_overheat)    { g_ctl->cmd_overheat = 0;    logf_("CMD IGNORED (no session): overheat"); }
    if (g_ctl->cmd_trail_toggle) { g_ctl->cmd_trail_toggle = 0; logf_("CMD IGNORED (no session): trail"); }
    if (g_ctl->cmd_valve)       { g_ctl->cmd_valve = 0;       logf_("CMD IGNORED (no session): valve"); }
    if (g_ctl->cmd_oilpress)    { g_ctl->cmd_oilpress = 0;    logf_("CMD IGNORED (no session): oilpress"); }
    if (g_ctl->cmd_exhaust)     { g_ctl->cmd_exhaust = 0;     logf_("CMD IGNORED (no session): exhaust"); }
    if (g_ctl->cmd_blowby)      { g_ctl->cmd_blowby = 0;      logf_("CMD IGNORED (no session): blowby"); }
    if (g_ctl->cmd_cooling)     { g_ctl->cmd_cooling = 0;     logf_("CMD IGNORED (no session): cooling"); }
    if (g_ctl->cmd_fuelpress)   { g_ctl->cmd_fuelpress = 0;   logf_("CMD IGNORED (no session): fuelpress"); }
    if (g_ctl->cmd_fuelleak)    { g_ctl->cmd_fuelleak = 0;    logf_("CMD IGNORED (no session): fuelleak"); }
}

/* 2026-10-07 (owner: "1.0.1 crashed"; VPS: 37 unclean exits, none
 * attributable - no address anywhere): record a fatal exception once - code,
 * address, module+offset, which MF hooks were mid-call - into the log and
 * the usage summary, then hand it to the previous filter unchanged. Never
 * waits on a lock the crashing thread may hold. */
static LPTOP_LEVEL_EXCEPTION_FILTER g_prev_uef = NULL;
static volatile LONG g_crash_once = 0;
static int g_uef_installed = 0;
static LONG WINAPI mf_crash_filter(EXCEPTION_POINTERS *ep)
{
    if (ep && ep->ExceptionRecord && InterlockedExchange(&g_crash_once, 1) == 0) {
        char mod[MAX_PATH] = "?", *b;
        HMODULE hm = NULL;
        DWORD addr = (DWORD)(uintptr_t)ep->ExceptionRecord->ExceptionAddress, off = addr;
        int lock_free;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)addr, &hm) && hm) {
            GetModuleFileNameA(hm, mod, MAX_PATH);
            off = addr - (DWORD)(uintptr_t)hm;
        }
        b = strrchr(mod, '\\');
        b = b ? b + 1 : mod;
        lock_free = InterlockedCompareExchange(&g_mfu_lock, 1, 0) == 0;
        if (lock_free) {
            mfu_unlock();
            logf_("CRASH: exception 0x%08lX at 0x%08lX (%s+0x%lX) thread %lu, session %d "
                  "phase %d, in hook: po %ld vn %ld ovr %ld imp %ld gear %ld",
                  (unsigned long)ep->ExceptionRecord->ExceptionCode,
                  (unsigned long)addr, b, (unsigned long)off,
                  (unsigned long)GetCurrentThreadId(), SESSION_TYPE, RACE_PHASE,
                  (long)g_po_inflight, (long)g_vn_inflight, (long)g_ovr_inflight,
                  (long)g_imp_inflight, (long)g_gear_inflight);
            mfu_flush("crash");
        } else {
            FILE *f = fopen(LOG_PATH, "a");
            if (f) {
                fprintf(f, "CRASH: exception 0x%08lX at 0x%08lX (%s+0x%lX) (usage lock busy)\n",
                        (unsigned long)ep->ExceptionRecord->ExceptionCode,
                        (unsigned long)addr, b, (unsigned long)off);
                fclose(f);
            }
        }
    }
    return g_prev_uef ? g_prev_uef(ep) : EXCEPTION_CONTINUE_SEARCH;
}

static void crash_filter_install(void)
{
    if (g_uef_installed) return;
    g_prev_uef = SetUnhandledExceptionFilter(mf_crash_filter);
    g_uef_installed = 1;
    logf_("CRASH FILTER: installed (previous %s)", g_prev_uef ? "chained" : "none");
}

static void crash_filter_remove(void)
{
    LPTOP_LEVEL_EXCEPTION_FILTER cur;
    if (!g_uef_installed) return;
    cur = SetUnhandledExceptionFilter(g_prev_uef);
    if (cur != mf_crash_filter) SetUnhandledExceptionFilter(cur);   /* someone chained after us: leave theirs */
    else g_uef_installed = 0;
}

static DWORD WINAPI worker(LPVOID unused)
{
    HANDLE ev;

    (void)unused;

    mf_audio_set_logger(logf_);   /* E7: audio diagnostics into our log */

    ev = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT);

    /* E9-H1a: a failed prologue check REFUSES the whole family - no pump and
     * no shared block start, so no fixed-address access can happen. */
    if (!fuelcut_install()) {
        logf_("FUELCUT: install refused - engine family inactive");
        if (ev) CloseHandle(ev);
        MF_EXIT(g_hinst);
        return 0;
    }
    /* R3j: optional camera listener tap for 3D audio. A mismatch only turns
     * the placement fallback on (sound stays full-volume at the car). */
    cam_tap_install();
    /* P1/P3 (ADDENDUM 15): optional AI pull-off detour. 2026-10-06 release:
     * installed LATE (first live session, ai_engines_pump) - Pit Stop
     * Failures checks AIMainUpdate 0x424E70..0x425660 at its own load and
     * refuses when this prologue is already patched (plug-ins load
     * alphabetically, "mechanical" before "pitstop"). */
    ai_ovr_open();
    ai_force_open();           /* B3 dev: forced-failure channel (release: no-op) */
    ai_gear_install();
    imp_install();             /* 2026-10-05: log-only impact probe */
    lamp_install();            /* L1 2026-10-05: native warning lamps */
    overrev_owner_install();   /* N7 2026-10-06: over-rev life-drain owner */
    voice_install();           /* V3: score->new-clip mapping (SDT-gated) */
    mfa_gate = on_circuit;     /* 2026-10-07: mod sounds only on the circuit */
    crash_filter_install();    /* 2026-10-07: crash address into log + usage */
    g_shm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                               0, sizeof(EngineCtl), SHM_NAME);
    if (g_shm) {
        g_ctl = (EngineCtl*)MapViewOfFile(g_shm, FILE_MAP_ALL_ACCESS,
                                          0, 0, sizeof(EngineCtl));
        if (g_ctl) {
            memset((void*)g_ctl, 0, sizeof(EngineCtl));
            g_ctl->magic = CTL_MAGIC;
            g_ctl->enabled = 1;
            g_ctl->cmd_emit_on = -1;
            g_ctl->auto_chance_pm = 150;
            g_ctl->auto_earliest_lap = 3;
            g_ctl->auto_guard_laps = 2;
            g_ctl->flavor_mult_pm = 100;
            /* E5: the model's game-clock onset (handoff 1.1) is the ONLY
             * default auto source. The old roll / car-flavor / vanilla-timer
             * sources are test-only: defaults OFF. */
            g_ctl->auto_enable = 0;
            g_ctl->timer_enable = 0;
            g_ctl->flavor_enable = 0;
            g_ctl->timer_remaining = -1;
            g_ctl->timer_set_ms = 10000;
            g_ctl->abi_version = 14;      /* N3: dead_reason appended (H16) */
            g_ctl->cooling_pct = -1;
            g_ctl->fuelpress_pct = -1;
            g_ctl->coeff_held = 1;
            g_ctl->overrev_enable = 1;
            g_ctl->niggle_enable = 1;      /* E9b item 3: default on */
            g_ctl->ai_engines_enable = 1;  /* 2026-10-06 ship: AI on (owner); INI AIFailures */
            g_ctl->ai_life_divisor_x10 = 10; /* E9p: 1.0 = game's AI reliability */
            g_ctl->ai_engine_law = 0;      /* E9p: the timer is the life clock */
            g_ctl->onset_model = 1;        /* Q3: hazard onset is the default */
            g_ctl->ai_onset_model = 1;     /* 2026-10-09 owner (tester log Tester A):
                                            * AI uses the SAME hazard as the
                                            * player (eta MF_HAZARD_ETA_AI ==
                                            * MF_HAZARD_ETA). Was 0 since Q8a
                                            * (countdown = realized vanilla WHEN
                                            * 1:1); 0 stays selectable for A/B */
            g_ctl->valve_air_pct = -1;
            g_ctl->oilpress_pct = -1;
            g_ctl->blowby_pct = -1;
            g_ctl->exhaust_s = -1;
            g_ctl->trail_id = 0;            /* OFF: SKID froze the game live 2026-09-28 */
            mf_user_ini_load();             /* 2026-10-06 ship INI */
            g_ctl->struct_size = (unsigned)sizeof(EngineCtl);
            /* E9-H4d: publish the AI shadow layout for the panel (read-only
             * view of g_ai). The offset array is in the panel's key order:
             * drain, drain_valid, sev, fault, onset, stab, culm, blown,
             * stopped, h_vt, oil_l, h_b, w_l, h_g, ring, h_rod, q, t_s, rev,
             * car_valid, oil_k, water_k, done. */
            g_ctl->ai_tbl_addr = (unsigned)(uintptr_t)&g_ai[0];
            g_ctl->ai_slot_size = (int)sizeof(MfAiSlot);
            g_ctl->ai_slot_count = MF_AI_MAX;
            {
                static const int offs[24] = {
                    (int)offsetof(MfAiSlot, eng.drain_ema_sps),
                    (int)offsetof(MfAiSlot, eng.drain_valid),
                    (int)offsetof(MfAiSlot, eng.severity),
                    (int)offsetof(MfAiSlot, eng.fault),
                    (int)offsetof(MfAiSlot, eng.onset),
                    (int)offsetof(MfAiSlot, eng.stabilised),
                    (int)offsetof(MfAiSlot, eng.culminated),
                    (int)offsetof(MfAiSlot, eng.blown),
                    (int)offsetof(MfAiSlot, eng.stopped),
                    (int)offsetof(MfAiSlot, eng.h_vt),
                    (int)offsetof(MfAiSlot, eng.v_oil_l),
                    (int)offsetof(MfAiSlot, eng.h_b),
                    (int)offsetof(MfAiSlot, eng.v_w_l),
                    (int)offsetof(MfAiSlot, eng.h_g),
                    (int)offsetof(MfAiSlot, eng.ring_s),
                    (int)offsetof(MfAiSlot, eng.h_rod),
                    (int)offsetof(MfAiSlot, eng.quality),
                    (int)offsetof(MfAiSlot, eng.t_s),
                    (int)offsetof(MfAiSlot, car.rev_limit_rads),
                    (int)offsetof(MfAiSlot, car_valid),
                    (int)offsetof(MfAiSlot, oil_k),
                    (int)offsetof(MfAiSlot, water_k),
                    (int)offsetof(MfAiSlot, done),
                    0
                };
                int oi;
                for (oi = 0; oi < 24; oi++)
                    g_ctl->ai_off[oi] = offs[oi];
                /* J6: the law-state offsets for the panel (AI_LAW_OFFS per
                 * hash becomes fallback only) */
                g_ctl->ai_law_off[0] = (int)offsetof(MfAiSlot, law.life_s);
                g_ctl->ai_law_off[1] = (int)offsetof(MfAiSlot, law.oil_k);
                g_ctl->ai_law_off[2] = (int)offsetof(MfAiSlot, law.water_k);
                g_ctl->ai_law_off[3] = (int)offsetof(MfAiSlot, law.init);
                /* P1: component damage for the panel (player shares + the
                 * per-slot array offset) */
                g_ctl->ai_dmg_off = (int)offsetof(MfAiSlot, eng.dmg);
                /* Q1: the AI life clock + wear rate for the panel */
                g_ctl->ai_q1_off[0] = (int)offsetof(MfAiSlot, ai_life);
                g_ctl->ai_q1_off[1] = (int)offsetof(MfAiSlot, ai_rate);
            }
            mfv_publish("engine", "-");
        }
    }
    if (!g_auto_rng) g_auto_rng = GetTickCount();

    logf_("--- engine_failure.dll loaded ---");
    logf_("GUI-controlled only (in-game hotkeys removed 2026-09-27); "
          "commands arrive via F1C_ENGFAIL_CTL");
    dump_state();

    for (;;) {
        if (ev && WaitForSingleObject(ev, 0) == WAIT_OBJECT_0)
            break;

        cm_broker_pump();    /* audit MF-09: broker discovery/registration */

        /* GUI commands (shared memory), same live-session gate. */
        if (g_ctl && g_ctl->magic == CTL_MAGIC) {
            /* E9g-b item 3: with no timed session the dispatch writes no
             * game memory; pending commands are logged and cleared. */
            if (SESSION_OVER()) {
                gui_cmds_drop();
            } else {
                if (g_ctl->cmd_stage1)   { g_ctl->cmd_stage1 = 0;   arm_action(0); }
                if (g_ctl->cmd_terminal) { g_ctl->cmd_terminal = 0; arm_action(1); }
                if (g_ctl->cmd_instant)  { g_ctl->cmd_instant = 0;  arm_action(2); }
                if (g_ctl->cmd_seizure)  { g_ctl->cmd_seizure = 0;  arm_action(3); }
                if (g_ctl->cmd_repair) {
                    g_ctl->cmd_repair = 0;
                    /* R1d (ADDENDUM 9): a repair must never arm anything.
                     * Consume same-pass toggle commands: the panel repair
                     * used to send cmd_oil_toggle + cmd_repair and the toggle
                     * re-armed the leak the repair had just cleared (live
                     * log 2026-10-01: REPAIR then "OIL LEAK: model fault
                     * armed", twice). Trail/telemetry/emitter commands are
                     * not fault arms and are audited clean. */
                    if (g_ctl->cmd_oil_toggle) {
                        g_ctl->cmd_oil_toggle = 0;
                        logf_("CMD REPAIR: cmd_oil_toggle consumed");
                    }
                    if (g_ctl->cmd_cutout_toggle) {
                        g_ctl->cmd_cutout_toggle = 0;
                        logf_("CMD REPAIR: cmd_cutout_toggle consumed");
                    }
                    repair_all();
                }
                if (g_ctl->cmd_overheat) { g_ctl->cmd_overheat = 0; arm_overheat(); }
                if (g_ctl->cmd_valve)    { g_ctl->cmd_valve = 0;    arm_valve_leak(100.0f); }
                if (g_ctl->cmd_oilpress) { g_ctl->cmd_oilpress = 0; arm_oil_pressure(); }
                if (g_ctl->cmd_exhaust)  { g_ctl->cmd_exhaust = 0;  arm_exhaust_crack(); }
                if (g_ctl->cmd_blowby)   { g_ctl->cmd_blowby = 0;   arm_blowby(); }
                if (g_ctl->cmd_cooling)  { int k = g_ctl->cmd_cooling; g_ctl->cmd_cooling = 0; arm_cooling(k); }
                if (g_ctl->cmd_fuelpress) { g_ctl->cmd_fuelpress = 0; arm_fuel_pressure(); }
                if (g_ctl->cmd_fuelleak) { g_ctl->cmd_fuelleak = 0; arm_fuel_leak(); }
                if (g_ctl->cmd_trail_toggle) {
                    g_ctl->cmd_trail_toggle = 0;
                    g_trail_test = !g_trail_test;
                    logf_("TRAIL test %s (id %d)", g_trail_test ? "ON" : "OFF",
                          g_ctl->trail_id);
                }
                if (g_ctl->cmd_dump)     { g_ctl->cmd_dump = 0;     dump_state(); }
                if (g_ctl->cmd_emit_on >= 0 && g_ctl->cmd_emit_on <= 2) {
                    int w = g_ctl->cmd_emit_on;
                    g_ctl->cmd_emit_on = -1;
                    emitter_toggle(w);
                }
                if (g_ctl->cmd_emit_clear) {
                    g_ctl->cmd_emit_clear = 0;
                    emitters_clear();
                }
                if (g_ctl->cmd_telemetry) {
                    g_ctl->cmd_telemetry = 0;
                    g_telemetry = !g_telemetry;
                    g_ctl->telemetry_on = g_telemetry;
                    logf_("telemetry HUD %s", g_telemetry ? "ON" : "OFF");
                }
                if (g_ctl->cmd_cutout_toggle) {
                    g_ctl->cmd_cutout_toggle = 0;
                    if (session_live()) toggle_cutout();
                    else logf_("cutout ignored - no live session");
                }
                if (g_ctl->cmd_oil_toggle) {
                    g_ctl->cmd_oil_toggle = 0;
                    if (session_live()) toggle_oil_leak();
                    else logf_("oil leak ignored - no live session");
                }
            }
            g_ctl->cutout_on   = g_cutout_on;
            g_ctl->oil_on      = g_oil_on;
            g_ctl->heat_s      = g_heat_on ? (int)g_heat_t : -1;
#if MF_LEGACY_OILP
            /* R2c (ADDENDUM 10): legacy field only - in the model build the
             * oil pressure comes from oil_pump (model value) and must hold
             * its last value while paused (R1b freeze). */
            g_ctl->oil_pressure = g_oil_pressure;
#endif
            g_ctl->life     = (float)ENG_LIFE;
            g_ctl->stress   = (float)ENG_STRESS;
            g_ctl->torque   = (float)ENG_TORQUE;
            g_ctl->rpm      = (float)ENG_RPM;
            g_ctl->bank1    = (float)ENG_BANK1;
            g_ctl->bank2    = (float)ENG_BANK2;
            g_ctl->bank3    = (float)ENG_BANK3;
            g_ctl->running  = (int)ENG_RUNNING;
            g_ctl->reason   = REASON_CODE;
            g_ctl->phase    = RACE_PHASE;
            g_ctl->session  = SESSION_TYPE;
            {
                int di;   /* P1: player damage shares (10-slot panel ABI kept;
                           * N3: BLOW-UP index 10 is AI-only and not published) */
                for (di = 0; di < 10; di++)
                    g_ctl->dmg_share[di] =
                        (float)(g_model_eng.dmg_total > 0.0
                                && di < MF_FAULT_COUNT
                                ? g_model_eng.dmg[di] / g_model_eng.dmg_total
                                : 0.0);
            }
            {
                int pi;   /* ADDENDUM 15: pull-off panel row per slot */
                for (pi = 0; pi < MF_AI_MAX; pi++) {
                    g_ctl->ai_po_state[pi] = g_ai[pi].pulloff.active
                                             ? g_ai[pi].pulloff.state : 0;
                    g_ctl->ai_po_side[pi] = g_ai[pi].pulloff.side;
                    g_ctl->ai_po_goal_x10[pi] =
                        (int)(g_ai[pi].pulloff.goal_mag * 10.0);
                }
            }
            g_ctl->heartbeat++;
            /* R3f (ADDENDUM 11, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): the
             * panel reads the model state; publish it every pass so an
             * explicit command (repair/clear/arm/set) that was just serviced
             * is visible in the SAME pass even when the sim clock is frozen.
             * model_publish is read-only (g_ctl only) - no game writes. */
            model_publish();
        }

        /* R1b (ADDENDUM 9, docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt): one pause
         * decision per pass. When the sim clock did not advance since the
         * previous pass (pause, menus) every lever pump returns early: no
         * game writes, no logs, hold state frozen. */
        {
            DWORD pnow = game_ms();
            g_pump_frozen = g_pump_prev_valid
                            && !mf_sim_advanced(g_pump_prev_ms, pnow);
            g_pump_prev_ms = pnow;
            g_pump_prev_valid = 1;
        }
        auto_pump();
        save_mode_pump();  /* E9i: F8 / panel, publishes the state */
        model_pump();    /* E4: model outputs live */

        /* E9g-e item 3: after model_pump (so a session change has already
         * cleared the latch) hold life at <= 0 while the engine is dead.
         * Item 6: the rev-rise log also runs here. */
        engine_dead_pump();

        cutout_pump();
        oil_pump();
        trail_pump();
        heat_pump();
        stint_pump();
        valve_pump();
        oilp_pump();
        exhaust_pump();
        blowby_pump();
        overrev_pump();
        overrev_owner_pump();   /* N7: one log per over-rev episode */
        cooling_pump();
        fuelp_pump();
        fuelleak_pump();
        lamp_pump();     /* L1: after every warning pump, worker-only write */
        stop_msg_pump();
        blow_pump();
        blow_fx_update(); /* N3e: shared FLAMES owners (player + AI blows) */
        burn_pump();     /* R3q: re-assert the post-death fire/smoke banks */
        wear_watch_pump();
        retire_commentary_pump();
        retire_pending_pump();
        telemetry_pump();
        voice_pump();    /* V3: due instruction lines / CLEAR01 */
        radio_pump();    /* 2026-10-01: one queued radio call at a time */
        hud_pump();      /* must run every tick or the strip fades us */
        mf_audio_pump(); /* E7: drains queued sounds on the worker */
        imp_pump();      /* 2026-10-05: log-only impact probe */

        Sleep(50);   /* 20 Hz */
    }

    InterlockedExchange(&g_hooks_stopping, 1); /* retained/chained hooks pass through */
    fx_restore();
    cooling_restore();
    fuelcut_remove();
    cam_tap_remove();
    mfu_flush("unload");     /* 2026-10-06: usage summary on exit */
    crash_filter_remove();
    ai_pulloff_remove();       /* P1: restore the AIMainUpdate prologue */
    ai_gear_remove();          /* s23: restore the AI auto-shift prologue */
    imp_remove();              /* 2026-10-05: restore 0x4EC1A0 prologue */
    lamp_remove();             /* L1 2026-10-05: restore 0x49B9A0/0x49BA50 */
    overrev_owner_remove();    /* N7 2026-10-06: restore 0x43EF20 prologue */
    voice_native_remove();     /* 2026-10-07: restore 0x411840 prologue */
    if (InterlockedExchange(&g_cm_broker, 0)) {
        /* audit MF-09: purge our queued posts and wait for a running one
         * BEFORE this module can be freed */
        F1CBrokerResult ur = g_cm_api.unregister_client(g_cm_token, 2000u);
        if (ur == F1C_BROKER_TIMEOUT) {
            mf_exit_pin_required();
            logf_("UNLOAD: commentary post still running; pinned");
            if (ev) CloseHandle(ev);
            MF_EXIT(g_hinst);
            return 0;
        }
        if (ur != F1C_BROKER_OK)
            logf_("UNLOAD: broker unregister %d", (int)ur);
        f1c_broker_release(&g_cm_api);
    }
    if (InterlockedCompareExchange(&g_vn_inflight, 0, 0)
        || InterlockedCompareExchange(&g_fc_inflight, 0, 0)
        || InterlockedCompareExchange(&g_cam_inflight, 0, 0)
        || InterlockedCompareExchange(&g_po_inflight, 0, 0)
        || InterlockedCompareExchange(&g_gear_inflight, 0, 0)
        || InterlockedCompareExchange(&g_imp_inflight, 0, 0)
        || InterlockedCompareExchange(&g_ovr_inflight, 0, 0)
        || InterlockedCompareExchange(&g_lamp_eng_inflight, 0, 0)
        || InterlockedCompareExchange(&g_lamp_fuel_inflight, 0, 0)) {
        /* A stuck pre-stop call can still touch worker-owned state. Retain
         * mappings/buffers/tables as well as code; no destructive cleanup. */
        mf_exit_pin_required();
        logf_("UNLOAD: calls still active; retaining engine state and buffers; pinned");
        if (ev) CloseHandle(ev);
        MF_EXIT(g_hinst);
        return 0;
    }
    rev_band_restore("REV BAND");
    engine_dead_clear("unload");   /* E9g-e item 1: no latch across a load */
    ai_shutdown();             /* E9p: restore AI power tables before eject */
    model_levers_release();    /* E8b/E9e: stock values before eject */
    voice_restore();           /* V3: put the donor lookup entries back */
    mf_audio_shutdown();   /* E7: buffers live in our data - free before eject */
    logf_("--- engine_failure worker stopped; hooked module remains pinned ---");
    if (g_ctl) { UnmapViewOfFile((void*)g_ctl); g_ctl = NULL; }
    if (g_shm) { CloseHandle(g_shm); g_shm = NULL; }
    if (ev) CloseHandle(ev);
    MF_EXIT(g_hinst);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hinst = hinst;
        DisableThreadLibraryCalls(hinst);
        /* Spawn only. Never open files, sound devices or sockets from
         * DllMain - loader lock. waveOutOpen here deadlocked the process
         * permanently on 2026-07-29.                                     */
        CreateThread(NULL, 0, worker, NULL, 0, NULL);
    }
    return TRUE;
}
