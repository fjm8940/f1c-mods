/*
 * safety_car_v0.9.c - SHIPPING CANDIDATE (pit-start, standings-hidden).
 *
 * Formerly pace_probe_nogrid.c. This is the most advanced safety-car build:
 * the SC loads as an extra car, is hidden from every classification, and is
 * deployed from the pits on demand. Version 0.9 = working, but the green-flag
 * timing / ramped exit-hold / recall-garage-park changes still need a full
 * live confirm before it is 1.0. See HANDOFF_2026-08-08.md in this folder.
 *
 * (Original probe header follows.)
 *
 * pace_probe.c - full_safety_car PHASE 5 probe: hold and release a commanded
 *                pace speed across the WHOLE FIELD, using the game's own lever.
 *
 * WHAT THIS IS
 * ------------
 * It began as the Phase 5 instrument - proof that the per-car governor found on
 * 2026-07-31 scales from one AI car to the whole field. It is now the complete
 * caution engine: safety car, pace line, no-passing rule, wave-around, yellow
 * flags, caution laps and an F1 restart.
 *
 * THE LIFECYCLE, and where it came from
 * -------------------------------------
 * The arc is NT2004's (DESIGN.md): watch -> decide -> neutralise -> gather ->
 * run -> warn -> inspect -> release. The RULES are F1's 1999-2002 ones, which
 * differ from NASCAR's in four ways that matter:
 *
 *   - the pit lane is never closed, and never reopens in stages;
 *   - lapped cars do not unlap by default (the wave-around is an option);
 *   - there are no lap deductions;
 *   - THE RESTART IS THE LEADER'S. NT2004 counts 5-4-3-2-1 and releases the
 *     whole field at one instant. F1 does not: the safety car peels into the
 *     pit lane, the leader dictates the pace from there, and no car may pass
 *     until it crosses the start/finish line. See restart_arm/restart_pump.
 *
 * Three layers of NT2004 have no host in F1C at all - the global command bus,
 * the session-state object, the steward flag machine - so that state lives in
 * this DLL. Everything F1C DOES own is armed rather than reimplemented: the
 * governor, the retirement path, the pit limiter, the marshalling-zone table
 * and the commentary queue.
 *
 * THE LEVER (FACT, live-proven 2026-07-31, findings.jsonl)
 * --------------------------------------------------------
 *   void __thiscall sub_42ACD0(float *this, int a2)   // ParameterValueSetter
 *   {
 *     if (a2 == 18) this[4430] = flt_8ACFB4;   // IdleSpeedTarget (DERIVED)
 *     else          this[4430] = (float)a2;    // 4430*4 = 0x4538 maxSpeed
 *   }
 *
 * Proven on a racing AI: maxSpeed 99999 -> 30 on call, HELD 30 for all 8 s
 * sampled, and the car's ACTUAL speed clamped to exactly 30.0 m/s on 7 of 8
 * one-second samples. sub_42ACD0(veh, 99999) restores it.
 *
 * This is the game's OWN slow-down lever: CommandQueueResponseHandler_00429ee0
 * calls ParameterValueSetter(18) on commands 0 and 4, and (99999) on command 5.
 * Arm the trigger, never forge the end state.
 *
 * a2 == 18 IS A TRAP, and it is now identified. It is special-cased to the
 * global flt_8ACFB4, which reads 22.2222 m/s live = exactly 80 km/h = F1C's
 * PIT LANE SPEED LIMIT. So sub_42ACD0(veh, 18) is the game's own pit-limiter
 * call - which is why CommandQueueResponseHandler uses it on the pit-related
 * commands 0 and 4 - and a commanded 18 m/s silently becomes 22.2. The DERIVED
 * name "IdleSpeedTarget" is a misnomer; it is PitSpeedLimit. set_pace() and
 * commanded_speed() both refuse the value.
 *
 * WHY RE-ARM EVERY TICK
 * ---------------------
 * The game reaches the same setter through its own command queue
 * (sub_424CD0 fires ~15 Hz, event-driven, on whichever cars have pending
 * commands). A natural cmd 0/4/5 would silently overwrite our commanded speed.
 * Re-arming at 20 Hz means any such override is corrected within 50 ms, and the
 * override counter below MEASURES how often that happens - which is a Phase 5
 * result in itself.
 *
 * WHAT THIS DELIBERATELY DOES NOT DO
 * ----------------------------------
 * It never calls sub_424CD0 (the per-car command queue). That function always
 * runs CommandQueueResponseHandler_00429ee0, which evaluates race-phase
 * branches regardless of the command sent (phase 3 -> SessionCleanupFlagClear +
 * WrapPlayerCallback; phase 4 -> AISpeedDeltaSetter(0)). sub_42ACD0 is a leaf
 * store with no such machinery, so it is called directly. The phase == 5 gate
 * below is kept anyway - it is the project's standing rule and costs nothing.
 *
 * THREAD NOTE
 * -----------
 * The Phase 1 rule "run injected calls on the game thread" exists because
 * sub_424CD0 drains a queue and dispatches. sub_42ACD0 is a single float store
 * with no reentrancy, no allocation and no callbacks - safe from the worker
 * thread. If a future module calls sub_424CD0, that rule applies again.
 *
 * CONTROL - two keys, and a menu
 * -------------------------------
 *   Num0   open / close the ON-SCREEN MENU. Everything this DLL does lives in
 *          it: safety car, pace hold, pace speed, target gap, caution laps,
 *          lapped-car policy, bunching, auto-caution, the yellow flag, the
 *          player's inclusion, the ramp, parking, field hiding, telemetry,
 *          field dump and panic release.
 *            Up/Down    move          Left/Right  change the value
 *            Enter      act on the row (deploy, engage, dump, panic)
 *            Num0       close
 *   Num0   deploy the safety car; once he is out, bring him in and run the
 *          restart. This is the whole feature and is kept outside the menu so
 *          it is reachable in one key while driving.
 *
 * Everything else used to be a global hotkey - Num3..Num0, Num*, Num/, Num.,
 * End, Insert, PgUp, PgDn, Left, Del, arrows. That is unusable at speed: every
 * one fires instantly, some irreversibly, and nothing showed the current state.
 * Numpad 1 and 2 stay unbound; they belong to the red-flag DLL, which is a
 * separate deliberate injection and is no longer in the mod pack.
 *
 * The menu is drawn on the game's own message strip (8 slots, 31 characters
 * each) - one status line plus seven scrolling rows.
 *
 *   Self-eject: event F1C_PACEPROBE_UNLOAD
 *     python tools/dll_unload_signal.py F1C_PACEPROBE_UNLOAD
 *   The DLL RELEASES the field before it ejects. Never FreeLibrary it from
 *   outside - the worker thread would be freed under itself (crashed the game
 *   2026-07-28).
 *
 * BUILD
 *   C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o pace_probe.dll pace_probe.c -lkernel32
 */

/*
 * ============================================================================
 * FORK: pace_probe_standings.c (2026-08-07)
 * ============================================================================
 * Fork of the working pace_probe.c caution engine. The ONLY functional
 * changes are:
 *
 *   1. THE SC IS NO LONGER A GRID ENTRANT - it is loaded as an EXTRA CAR at
 *      track load, using the native mechanism (patches/native_safety_car/
 *      native_sc.c, DESIGN "B"): the DLL hooks the tail of AIDriverCountSetup
 *      and adds Bernd by roster .VEH path at the first free slot; the game's
 *      own mesh loop builds him; the DynMan_Restart detour parks him once the
 *      render pass is done. Everything runs on the game thread.
 *
 *   2. THE STANDINGS FEATURE (the point of the fork): the moment the load
 *      finishes, VehicleCount is dropped by one (23 -> 22), so the SC is
 *      invisible in the classification of EVERY session - the race AND
 *      qualifying. The load's render pass already placed him at his qualipos
 *      (grid box 23 - SceneRenderUpdate's placement via sub_4D10E0 runs
 *      while he is counted); he stays there, hidden. When the RACE leaves
 *      the classification screen (phase 2 -> 3) he is re-added (role 1 +
 *      count +1) and the game's own placement keeps him on his qualipos for
 *      the start - NO hand-written teleports, they fought the placement pass
 *      (2026-08-07). He takes the green from the back, the game retires him
 *      to his garage on lap 1, and the moment he is parked the count drops
 *      again (23 -> 22) so he leaves the live standings for the rest of the
 *      race. He re-enters the count only while actually deployed; on the
 *      recall he returns to the garage and drops out again. The count is
 *      restored before the session teardown (the worker's array guard) so
 *      the loader never walks a count that does not cover him - the crash
 *      that killed native_sc.c's drop_count.
 *
 *   3. HIDING is the native park (role -1 + BuildActiveVehiclePointerArray +
 *      count adjust) instead of the active-array lift: the array is rebuilt
 *      from the role flags by the game's own function, no pointer shifting.
 *
 * Everything else - the pacing laws, the restart, the commentary, the GUI
 * control block, the wave-around, the trigger watcher - is pace_probe.c
 * unchanged.
 *
 * DEPENDENCY: the car's .veh must carry the NotAI class token
 * ("Safety, DSGermany, 1999, NotAI" - backup of the grid-entrant version in
 * 1999_fia29.veh.bak_before_fork), or the AI pool drafts him as a normal
 * entrant and the extra-car slot is taken. That same token is what
 * full_safety_car's grid-entrant version cannot use.
 *
 * INJECT AT THE MAIN MENU, BEFORE A TRACK LOAD (the load hooks must be armed
 * before DynMan_Init runs). Self-eject: python tools/dll_unload_signal.py
 * F1C_PACEPROBE_UNLOAD
 * ============================================================================
 */

#include <windows.h>
#include "sc_config.h"
#include "sc_confirm.h"
static SCConfirm g_confirm;
static void sc_key_name(int vk,char *out);
#include "f1c_orientation.h"
#include "f1c_incident_geometry.h"
#include "sc_catchup.h"
/* 0.10.91: no licence/activation/PC-binding gate. */
extern const char *sc_target_reason(void);
#include "f1c_broker.h"
#include "f1c_race_control.h"
#include "f1c_sc_fault.h"
static F1CRaceControl *g_rc;
static F1CSCFault *g_sc_fault_signal;
static int g_sc_fault_latched;
static double g_sc_identity_fault_since=-1.0;
static void sc_request_fault(int reason);
#include "f1c_broker_recovery.h"
#include "f1c_broker_session_end.h"
#include "f1c_result_position.h"
#include "f1c_race_penalty_issue.h"
#include "sc_player_no_overtake.h"
#include "sc_no_pass_display.h"
#include "sc_status_display.h"
#include "sc_status_box.h"
#include "sc_radio_filter.h"
#include "f1c_sdk.h"
/* The SDK helper has already expanded its own slot macro at definition.
 * Restore this client's established integer-valued VEH spelling below. */
#undef VEH_BASE
#undef VEH
static F1CBrokerSetSessionEndFn g_set_session_end;
static F1CBrokerRecoveryApi g_recovery;
/* Full legacy runtime, gated by the shared broker.
 * Stop is resident: process restart is required before another version. */
int sc_target_compatible(void);
static F1CBrokerApi g_broker;
static F1CBrokerToken g_client;
static volatile LONG g_pending, g_stop_requested, g_stop_done;
static DWORD g_game_thread;
static int g_initialized;
static unsigned g_epoch = 1;
static int g_emergency_inhibit, g_emergency_failed;
static int g_emergency_disarm = 1;
static int g_hooks_ready;
static LONG g_preload_seen;
static int g_hooks_ready_logged;
static void reset_track_history(void);
static void abandon_session(void);
static void formation_pump(void);
static int sc_slot_owned(void);
static int on_game_thread(void) { return g_game_thread && GetCurrentThreadId() == g_game_thread; }

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- vehicle array (FACT, root CLAUDE.md) ------------------------------ */
#define VEH_BASE        0x005EC6D8u
#define VEH_STRIDE      0x48C0u
#define VEH_SLOTS       104

#define OFF_ACTIVE      0x32C8u   /* int32, -1 = slot dead, 0 = player, 1 = AI */
/* LIVE RACE POSITION.
 *
 * NOT 0x32D0. Root CLAUDE.md calls that "session position (1-indexed)" and this
 * probe used it for two full sessions - it is actually the SLOT INDEX, static:
 * read across the grid it returns exactly i for vehicle slot i, every car,
 * every sample. That is why the Phase 5 hold reported "pos-changed = 0" for
 * 149 s straight; the field could not change because the field was constant.
 *
 * It also produced the visible failure: the bunching controller elected the
 * car at "P1" as the pace setter, which was slot 1 - Michael Schumacher, a lap
 * DOWN in ninth - and assembled the actual leaders behind him. Driver report
 * 2026-07-31: "P9 lapped Schumacher physically in front of leading car pack".
 *
 * 0x4060 is the real thing, and is what mods/f1cui_web_remote already uses.
 * Verified deterministically 2026-07-31: sorting all 22 live cars by race
 * distance (lap number at 0x403C, then lap distance at 0x4044) reproduces
 * 0x4060 EXACTLY, 1..22, lapped cars and all.                               */
#define OFF_POSITION    0x4060u   /* int32, live race position, 1-indexed      */
#define OFF_PITTING     0x3450u   /* int32, 0 = on track .. 4 = pit/garage     */
/* Metres along the lap, resetting at the start line.
 *
 * NOT 0x4048. That offset (mods/f1cui_web_remote's OFF_TRACK_PART_EXACT,
 * "live-verified 2026-07-28") is PLAYER-ONLY: read across the grid it returns
 * exactly -1.00 on every AI car and a real distance only on slot 0. Its
 * verification was done on the player alone, so the limitation was never seen.
 * Using it here made every computed gap exactly 0.0 (-1 minus -1), which drove
 * the bunching controller to slam the whole field onto its floor speed.
 *
 * 0x4044 is the real per-car field. Live-proven 2026-07-31 over 3 s on a
 * 21-car grid: the delta matched speed x time within 1-2% for ALL 20 moving
 * AI cars, and slot 10 wrapped 5723.2 -> 32.9 at the start line, which also
 * pins the lap length at ~5750 m for this track.                            */
#define OFF_LAPDIST     0x4044u   /* float, metres along the lap [FACT]        */
#define OFF_LAPNUM      0x403Cu   /* int32, lap number    [CONFIRMED]          */
#define OFF_SPEED       0x4440u   /* float, actual speed  [FACT, name_index]   */
#define OFF_MAXSPEED    0x4538u   /* float, the governor  [CONFIRMED]          */

/* ---- the AI's own willingness to overtake -------------------------------
 * Two CONFIRMED per-car fields, both baked from the driver's .rcd at session
 * init and neither recomputed afterwards, so a live write holds.
 *
 *   0x44CC PassingNorm             = 0.9 + 0.001 * rcd Passing, i.e. 0.9..1.0.
 *                                    Read every frame by AIRacingLine_00425920.
 *   0x44F8 TrackAggressionEffective= AIRealism x rcd TrackAggression, clamped
 *                                    >= 0.01. Consumed every frame by
 *                                    AISteering_004270a0, which uses it for
 *                                    the desired follow gap (x(2-a), floored at
 *                                    20-10a m), the side-by-side window (4-a),
 *                                    and the per-opponent pressure accumulator
 *                                    (+= a x closing while inside (1+a)*20 m).
 *                                    Lower a = bigger gaps, slower pressure
 *                                    build, fewer attacks. This is the PLR
 *                                    "AI Realism" knob's only effect.
 *
 * Under caution both are pushed to their minimum so the AI stops TRYING to
 * pass, and restored on release. The kinematic clamp below then makes it
 * impossible rather than merely unattractive.                               */
#define OFF_PASSING     0x44CCu   /* float, PassingNorm       [CONFIRMED]      */
#define OFF_AGGRO       0x44F8u   /* float, TrackAggression   [CONFIRMED]      */
#define PASSING_MIN     0.9f      /* = rcd Passing 0                           */
#define AGGRO_MIN       0.01f     /* the game's own clamp floor - DO NOT WRITE */
#define AGGRO_DEFAULT   0.65f     /* what the whole 1999 field reads; the value
                                   * a car clobbered by an older build gets    */

/* Kinematic no-passing clamp: inside this range a car may never be commanded
 * faster than the car physically ahead of it (plus a small closing allowance
 * while it is still further back than its target gap). A car that cannot
 * out-run the car in front cannot pass it, whatever the AI wants.           */
/* The approach law, which does the no-passing job and the braking job at once:
 *
 *     allowed_speed = speed_of_car_ahead + APPROACH_GAIN * (gap - target_gap)
 *
 * At the target gap a car may do exactly what the car ahead is doing, so it
 * holds station and CANNOT pass. Closer than that it is commanded slower and
 * falls back. Further away it gets a closing allowance proportional to the
 * room available, so it slows down CONTINUOUSLY as it arrives instead of
 * running at full catch-up speed until the last moment.
 *
 * APPROACH_RANGE is deliberately long. The first version only eased off inside
 * 120 m, which left cars arriving at 35 m/s of closing speed with two seconds
 * to shed it - driver report 2026-07-31: "they nearly crash into each others
 * rears when catching up with the pack and decelerating."                   */
#define APPROACH_RANGE  ((double)g_sc_config.APPROACH_RANGE)    /* metres: start easing off this far out     */
#define APPROACH_GAIN   ((double)g_sc_config.APPROACH_GAIN)     /* m/s of closing allowed per metre of room  */

/* The collision floor against a car that is NOT entitled to set our pace - a
 * lapped car, or one out of position. Deliberately short: it exists only so
 * that refusing to inherit such a car's pace does not turn into driving
 * through it. Beyond BLOCK_RANGE it has no effect, so a better-classified car
 * closes on it at full race pace and only settles behind it at the end.     */
#define BLOCK_RANGE     ((double)g_sc_config.BLOCK_RANGE)     /* metres: no effect at all beyond this      */
#define BLOCK_GAP       ((double)g_sc_config.BLOCK_GAP)     /* metres: tuck up closer than the queue gap */

/* How close a car is ALLOWED to sit behind the one in front. Driver's spec,
 * 2026-08-01: "The F1 cars themselves can be as close to each other as they
 * want, but overtaking is blocked and STRICTLY prohibited." So the no-passing
 * law holds station at one car length rather than at the queue's target gap -
 * the target gap is for ASSEMBLING the queue, not for policing it once formed.
 * Below this the car is asked to back off; above it, it may match the car in
 * front exactly, which is what makes passing impossible without forcing an
 * artificial spacing.                                                       */
#define FOLLOW_MIN_M    ((double)g_sc_config.FOLLOW_MIN_M)      /* about one car length plus a margin        */

/* How far behind the safety car the LEADER may sit: ten car lengths. */
#define SC_LEAD_MAX_M   ((double)g_sc_config.SC_LEAD_MAX_M)

/* How close the leader must be to the start line before the safety car is let
 * out of the pit lane, so that he emerges in front of him rather than having
 * to chase him down.                                                        */
/* Measured 2026-08-01: released with the leader 869 m from the line, he took
 * the field 459 m ahead of him - too much road. The offset is roughly the
 * distance he covers getting out of the pit lane, so the gap tracks the window
 * about one for one: 500 m of warning should put him out just ahead of the
 * leader's nose, which is the intent ("wait at the end of the pit straight
 * until the leading car is coming to the main straight").                   */
#define SC_RELEASE_WINDOW_M ((double)g_sc_config.SC_RELEASE_WINDOW_M)  /* leader's distance-to-line at which the SC
                                     * is released. Widened 500 -> 900 (driver
                                     * 2026-08-08) so he comes out earlier, with
                                     * room for the field to ease down to his
                                     * speed gently instead of braking hard. */
#define START_LIGHTS_MOVING_MS ((double)g_sc_config.START_LIGHTS_MOVING_MS) /* m/s: below this the whole field is taken
                                     * to be still on the grid under the starting
                                     * lights, and a deploy is refused (2026-08-08)*/

/* A car within this many metres LONGITUDINALLY, either side, counts as level
 * with us and is respected as if it were ahead. Without it a pass that had
 * already drawn alongside could still be completed: the gap is measured along
 * the lap, so an abreast car reads as ~0 m and used to be skipped as "that is
 * us". No lateral test is needed - at this range the other car is either
 * alongside or nose-to-tail, and in both cases accelerating past it is wrong. */
#define ABREAST_WINDOW  ((double)g_sc_config.ABREAST_WINDOW)      /* metres, about one car length              */
/* Ordinary obstacle/catch-up threshold only. Stationary classification uses
 * sc_stuck_watch.h and must not interpret a commanded crawl as a wreck. */
#define OBSTACLE_MIN    ((double)g_sc_config.OBSTACLE_MIN)

/* The clamp follows the car ahead's LOWEST recent speed, not its current one.
 * Two reasons: a car that spins or is damaged sheds speed in far less than one
 * poll interval, and at 30 m/s a 50 ms lag is 1.5 m of closing; and a car
 * braking for a corner would otherwise let its follower keep the higher speed
 * for a tick. Half a second of history is enough for both.                  */
#define SPD_HIST        10        /* samples at 20 Hz = 0.5 s                  */
#define OFF_NAME        0x4620u   /* ASCII driver name                         */

#define VEH(i)          (VEH_BASE + VEH_STRIDE * (DWORD)(i))
#define V_I32(i, off)   (*(volatile int32_t*)(VEH(i) + (off)))
#define V_F32(i, off)   (*(volatile float*)  (VEH(i) + (off)))
#define V_STR(i, off)   ((const char*)       (VEH(i) + (off)))

/* FORK: the game's own active-array rebuild, declared here because the fork's
 * sc_hide()/sc_unhide() (the native park) use it. */
#define A_REBUILD    0x0042BC80u   /* BuildActiveVehiclePointerArray  no-arg */
typedef int (__cdecl *rebuild_t)(void);
#define REBUILD      ((rebuild_t)A_REBUILD)

/* ---- session gates ----------------------------------------------------- */
#define SESSION_TYPE    (*(volatile int*)0x007C56A4u)      /* 5=qual 7=race   */
#include "sc_time_policy.h"
#define SESSION_TIME    (*(volatile double*)0x008AFEA0u)   /* seconds, double */
/* Test Day is session type 0 (SessionTypeNameLookup 0x0042CE30 case 0). The
 * safety car is NEVER added in Test Day - driver rule 2026-08-07.          */
#define SESSION_TEST_DAY 0
#define RACE_PHASE      (*(volatile int*)0x007C56A8u)      /* 5 = racing      */
#define PHASE_RACING    5

/* ---- the lever --------------------------------------------------------- */
#define ADDR_SET_MAXSPEED   0x0042ACD0u
#define SPEED_RELEASE       99999      /* the game's own release value        */
#define SPEED_IDLE_SENTINEL 18         /* a2 == 18 means IdleSpeedTarget      */
#define PACE_MIN            5
#define PACE_MAX            80

typedef void (__attribute__((thiscall)) *set_maxspeed_fn)(void *veh, int n);
static const set_maxspeed_fn set_maxspeed =
    (set_maxspeed_fn)ADDR_SET_MAXSPEED;

/* ---- the safety car vehicle --------------------------------------------
 * F1C has no reserved pace-car slot. NT2004 keeps one permanently at roster
 * slot 48, resolved by the name "pace car", and the caution machine treats it
 * as the pace line's target (DESIGN.md Â§8). F1C instead ships a complete FIA
 * Safety Car vehicle - SeasonData/Vehicles/FIA/1999_SafetyCar/Safgpxa.veh,
 * DRIVER="Bernd Maylander", Mercedes livery - which simply joins the grid as a
 * normal car when its Classes line matches the season.
 *
 * So the F1C sequence is the opposite of NT2004's: rather than a hidden extra
 * slot, Maylander is a real entrant who is kept RETIRED in his garage until
 * deployed, then revived and driven out of the pit lane by his own AI. That
 * recipe is proven in mods/safety_car/sc1.c and its offsets are reused here.
 *
 * RESOLVED BY NAME, NEVER BY SLOT: enabling Maylander makes it a 23-car grid
 * and shifts every other driver's slot (sc1.c). A hardcoded index would drive
 * the wrong car.                                                            */
#define OFF_ANGVEL      0x00A0u   /* EngineAngVel [FACT]        */
#define OFF_RWFLAG      0x0D58u   /* RearWingIntactFlag         */

static void gfx_hide(int i, int on);   /* scene-graph hide, defined below */
#define OFF_FWFLAG      0x0D14u   /* FrontWingIntactFlag [CONFIRMED, corpus] */
#define OFF_CARCOND     0x3D6Cu   /* carConditions       [FACT, corpus]      */
#define OFF_REASON      0x4068u   /* ReasonCode          [FACT, corpus]      */
#define OFF_DRIVERVIS   0x105Du   /* DriverModelVisible [FACT]  */
#define OFF_RPM         0x39A0u   /* EngineRPM                  */
#define OFF_FUEL        0x3984u   /* FuelLevel [DERIVED, corpus f1c_vehicle_struct.h] */
#define OFF_PITSTATE    0x4040u
#define OFF_QUALPOS     0x4058u   /* qualifying position = grid slot */
#define OFF_FINISH      0x4064u   /* FinishState, 2 = retired   */
#define OFF_CURWP       0x4354u   /* CurrentWaypointPtr [FACT]  */
#define OFF_RETIRED     0x4410u
#define OFF_ROSTER   0x4898u       /* index into the roster (identity)          */

/* Position and velocity, both CONTIGUOUS float triads in the vehicle header.
 *
 * PhysY 0x20 is FACT (name_index; genuine AI retirement teleports a car into
 * its garage by writing this triad). X and Z are its neighbours at 0x1C and
 * 0x24 - NOT 0x18 and 0x28. The first version assumed a stride of 8 and read
 * two fields that are zero on every car, alive or dead, which silently
 * disabled the garage-pose capture: its "is he at the origin" guard could
 * never pass.
 *
 * Both triads were settled by measurement 2026-08-01, on the safety car while
 * he was falling out of the world, by diffing his header over 0.4 s:
 *
 *   0x1C 3673.660 -> 3679.918   +6.258   vs velX 0x58 =   15.588  (x0.4 = 6.24)
 *   0x20  -45375   ->  -45447   -72.109  vs velY 0x5C = -179.932  (x0.4 = -72.0)
 *   0x24  594.549  ->  594.894   +0.345  vs velZ 0x60 =    0.774  (x0.4 = 0.31)
 *
 * Every component matches its own velocity, which is what makes the pairing
 * certain rather than plausible. */
/* CORRECTION 2026-08-03: 0x1C/0x20/0x24 is a MIRROR, not the store.
 *
 * The authoritative world position is veh+0x34E0/0x34E4/0x34E8, and a 12-byte
 * write there teleports a LIVE, DRIVING car - proven live, the car keeps
 * driving from the new place. 0x1C follows it one frame later, as does a
 * second mirror at 0x744/0x748/0x74C. Writing 0x1C does NOTHING: the value is
 * re-stamped within 30 ms even on a car that is retired, inactive and hidden.
 *
 * That single fact explains every failed teleport in this project's history,
 * including the pre-start one in sc_pose_pump that is compiled out behind
 * if(0) below - it was writing the mirror.
 *
 * Found by bisecting a donor car's struct over a target's: [0x0,0xE10) did
 * nothing, [0x34DC,0x34FC) relocated the car, and dumping those 32 bytes named
 * the fields outright. 0x34F0 alongside them is the yaw the corpus already had
 * from static analysis, so this is the body transform block.               */
#define OFF_BODYX       0x34E0u   /* THE position [FACT 2026-08-03] */
#define OFF_BODYY       0x34E4u
#define OFF_BODYZ       0x34E8u
#define OFF_YAWBODY     0x34F0u   /* yaw, same block                */

#define OFF_PHYSX       0x1Cu     /* mirror of 0x34E0 - do not write */
#define OFF_PHYSY       0x20u
#define OFF_PHYSZ       0x24u
#define OFF_VELX        0x58u
#define OFF_VELY        0x5Cu
#define OFF_VELZ        0x60u
#define OFF_LANEMODE    0x4550u   /* WaypointSetSelector, 1 = pit-lane path */
#define OFF_DRVSTAT     0x4510u   /* 3 = approach               */
#define OFF_AICTRL      0x45F4u   /* AIControlFlag [CONFIRMED]  */
#define OFF_CALLBOX     0x454Cu   /* callToBox [FACT]           */

/* Driver names are unrestricted; selected roster identity owns the SC. */
#define SC_CRANK_RPM    800.0f
#define SC_ONTRACK_MS   ((double)g_sc_config.SC_ONTRACK_MS)     /* clear of the 22.22 pit limiter */

/* Seconds between slowing the field and letting the safety car out. Learned
 * live 2026-07-25 (sc1.c): revived at the same instant the cap engaged, he
 * emerged from the pit exit into 18 cars still at racing pace and was wiped
 * out before he ever cleared the limiter. Slow the field FIRST.            */
#define SC_ARM_DELAY_MS ((int)g_sc_config.SC_ARM_DELAY_MS)

#define SC_IDLE      0
#define SC_ARMING    1   /* field slowing, safety car still parked */
#define SC_OUTLAP    2   /* revived, driving out of the pit lane   */
#define SC_CATCHUP   3   /* on track, running hard to reach the leader */
#define SC_ONTRACK   4   /* in front of the leader, now pacing     */
#define SC_RECALL    5   /* called to the box, still on track      */

/* How far AHEAD of the race leader he must get before he starts pacing. He
 * comes out of the pit lane wherever the pit exit happens to be - in the first
 * live run that was P19 of 23 - so he has to catch the field first. NT2004 has
 * the same phase as pace-lap substate 1, "catching the field". Until then the
 * RACE leader sets the pace and the safety car runs unrestricted.          */
#define SC_PICKUP_M  ((double)g_sc_config.SC_PICKUP_M)

static int   g_sc_state = SC_IDLE;
static volatile int g_sc_slot  = -1;   /* written by the load hooks (game thread) */
static int g_sc_committed, g_sc_roster;
static int g_sc_selected_roster = -1;
static char g_sc_identity[32];
static DWORD g_sc_mark  = 0;
static double g_sc_arm_sim = -1.0;
static int   g_arm_saw_far    = 0;   /* the race leader has been BEYOND the release
                                      * window since the caution was called. The SC is
                                      * released only on a FRESH approach to the line
                                      * (leader was far, now near) so he always comes
                                      * out ahead - lap-1 capable now the lap length is
                                      * seeded from the Steward at load. Driver spec
                                      * 2026-08-08. */
static int   g_sc_park  = 1;   /* keep him garaged until called - End toggles */
static int   g_sc_insta = 0;   /* OFF by default: he drives his own in-lap.
                                * Driver 2026-08-02: "just let him drive
                                * around the lap ... only thing we want is
                                * not to have it in the classification."
                                * The in-lap is the proven path; the grid
                                * freeze was only ever there to avoid it. */
static DWORD g_green_ms = 0;   /* first racing tick seen                 */
#define SC_INSTA_DELAY_MS ((int)g_sc_config.SC_INSTA_DELAY_MS)
/* DISPROVEN 2026-08-01, twice, by its own watchdog: a car lifted out of the
 * active array is still SIMULATED but is no longer DRIVEN. Hiding him for the
 * in-lap froze him on the spot - measured at exactly 10.9 km/h for four
 * seconds, which is the hide threshold of 3.0 m/s, until the watchdog put him
 * back. It also read as him being blocked by a stalled car, which it was not.
 *
 * So the active-array hide is only ever usable on a car that is meant to stay
 * still. Kept at 0 rather than deleted so the watchdog and this note survive
 * as the record of what was tested.                                        */
static int   g_hide_inlap = 0; /* hide him from the standings on his in-lap   */
static float g_inlap_dist = 0.0f;   /* stall watchdog for the above           */
static DWORD g_inlap_mark = 0;
#define SC_INLAP_STALL_MS ((int)g_sc_config.SC_INLAP_STALL_MS)      /* not moving this long = not being driven */

/* ---- NT2004-style field hiding -----------------------------------------
 * NT2004's pace car is invisible to the race because it lives in a SEPARATE
 * vehicle list (count dword_5FD344, entries off_5FD348) at slot 48, past a full
 * 48-car field. Nothing that iterates the field can see it, grid placement
 * included.
 *
 * F1C's equivalent is the active-vehicle POINTER ARRAY at 0x7C54F0 walked up to
 * VehicleCount at 0x5C329C. That count is FACT-tier and is written by exactly 7
 * functions, all of them session setup (LoadVehicleEntries, ProcessDriverEntries,
 * AIDriverCountSetup) - nothing rewrites it per frame - while 90 functions READ
 * it. So removing a car from that array and decrementing the count takes it out
 * of the field for every one of those readers at once.
 *
 * That is the port: lift the safety car's pointer out of the array, shift the
 * rest down, park his pointer in the freed last entry, and drop the count by
 * one. He remains a fully-formed vehicle object we can still reach directly by
 * struct address - our own loops index BASE + STRIDE*slot and are unaffected -
 * but the game's field loops cannot see him, so he is never placed on the grid.
 * Deploying restores the count and he re-enters the world.
 *
 * WHY THIS IS NEEDED: retiring him does not work. Proven 2026-07-31 with the
 * DLL loaded before the field existed - retired, AI off, driver hidden, planned
 * position last - and the game STILL placed him in grid box 23. Retirement
 * controls state, never placement.
 *
 * The order of the other entries is preserved rather than swapped, because it
 * is not known which readers assume array order.                            */
#define VEHICLE_COUNT   (*(volatile int*)0x005C329Cu)
#define VEHICLE_ARRAY   ((volatile unsigned int*)0x007C54F0u)

static int          g_hide_mode  = 1;   /* Insert toggles */
static int          g_hidden     = 0;
static int   g_sc_said05 = 0;
static int   g_sc_recall_armed = 0;  /* recall restart fired once, see SC_RECALL */
static int   g_sc_ai_saved = 0;   /* his own passing/aggro captured once */

/* One deferred line. sc_say() holds the game for 300 ms while the queue entry
 * is built from the patched table, so two lines fired in the same tick fight
 * each other: the second overwrites the first before it is spoken. The recall
 * sequence needs exactly two lines close together (SCAR05 as he turns in,
 * SCAR03 as the restart arms), so the second is queued, not fired.          */
/* How many times a line waits for a free audio channel before it is DROPPED.
 * 12 x 250 ms = 3 s, the same order as retire_commentary.c's proven wait. */
#define SAY_MAX_TRIES 12
static DWORD        g_said_at = 0;   /* when the last line was actually spoken */
static void sc_say_later(int score, const char *what, DWORD delay_ms);

static int sc_is_out(void);         /* the queue leader test, defined below   */
static int race_leader_slot(void);  /* leader EXCLUDING the safety car        */
static void sc_unhide(void);   /* field-hiding restore, see sc_hide()          */

/* run/volume agreement tags, checked in engage() - not tunable, leave as-is.
 * volatile: must stay real bytes in the image, not fold to a constant. */
static const volatile unsigned char SC_SCAN_TAG_A[4] = { 0x58,0xC0,0x41,0x16 };
static const volatile unsigned char SC_SCAN_TAG_B[4] = { 0x4A,0x8C,0xB9,0x2D };

/* ---- commentary ---------------------------------------------------------
 * The safety-car clips PWS_SCAR01/02/03/05 exist in PC_RE_MAIN.SDT but have NO
 * event in the PWS range: the low-eid table jumps from score 130 (eid 132,
 * SLOWCAR01) straight to score 136 (eid 133, YELLOW04), skipping exactly the
 * four safety-car scores. They are reachable only through eids 423-428, which
 * are above 291 and therefore play from the TV table instead - isolation-tested
 * 2026-07-31: injecting eid 423 produced an unrelated position line ("are
 * second"), not the safety car.
 *
 * The way in is that the SDT index comes from the event's SCORE, and the
 * channel comes from the EID RANGE. So a low eid is borrowed as a carrier and
 * its score is temporarily rewritten to the safety-car score. Isolation-tested
 * the same day: carrier eid 132 with score 132 played "safety car has been
 * deployed and he's exiting the pits now" on the race-engineer channel.
 *
 * Same trick mods/safety_car/sc1.c wrote down in 2026-07-25 but never got to
 * test, using carrier 131.                                                  */
#define CTRL_OBJ        0x005EA618u
#define EVENT_TABLE     0x005BF9A0u
#define CARRIER_EID     132       /* PWS range; its own clip is SLOWCAR01 */

/* Event table layout, from tools/dump_full_catalog.py (stride 0x14):
 *   +0x00 priority (int)   +0x0C cooldown (float)   +0x10 score (int)
 * A cooldown above 1e37 is what the corpus calls BLOCKED - it is the same
 * FLT_MAX mechanism the PWS_HIT01-06 restoration had to undo. */
#define EV_COOLDOWN(eid) ((volatile float*)(EVENT_TABLE + (eid) * 0x14 + 0x0C))
#define EV_SCORE(eid)    ((volatile int  *)(EVENT_TABLE + (eid) * 0x14 + 0x10))
#define EV_BLOCKED       3.4e38f
/* Per-event "when was this last spoken" timestamp, one float per eid. Written
 * by the game when a queued entry is actually consumed, which makes it the
 * only honest played/not-played signal available: the queue head drains within
 * one poll, so reading that instead gives false negatives (2026-07-28, the
 * pit-exit arc). */
#define EV_TS(eid)       ((volatile float*)(CTRL_OBJ + 0xAF8u + (eid) * 4u))

/* SLOWCAR01 MUST NEVER PLAY. Driver instruction, stated twice.
 *
 * Score 130 (PWS_SLOWCAR01, "watch out, there's a slow car ahead") is
 * reachable on the race-engineer channel through EXACTLY ONE event: eid 132 -
 * which is the carrier this file borrows. Checked against
 * data/full_event_dump.json: the only other holder of score 130 is eid 427,
 * and that is above 291 so it plays on the TV channel. That single fact is
 * what makes the line killable.
 *
 * There were TWO leaks and a busy-channel test closed neither:
 *
 *   1. THE GAME'S OWN EMITTER. It fires eid 132 by itself, and under a caution
 *      the whole field is slow, so its trigger is satisfied constantly. Closed
 *      by parking the event's cooldown at FLT_MAX, which is the game's own
 *      "blocked" state. sc_say does NOT go through the emitter - it builds the
 *      queue entry and calls InsertDriverTableEntry directly - so blocking the
 *      event costs the safety-car lines nothing.
 *
 *   2. OUR OWN RESTORE. sc_say used to patch the score, fire, sleep 300 ms and
 *      put 130 back. isPlaying == 0 does not mean the queue is EMPTY, so the
 *      entry could still be resolved after the restore and load 130. The
 *      2026-08-02 recurrence happened with the channel measured free and both
 *      lines logged as fired, which is what ruled the busy-channel theory out.
 *      Closed by never writing 130 back at all: the score is left parked on the
 *      last safety-car clip, which is harmless precisely because the event is
 *      blocked and the game can no longer reach it.
 *
 * Both are restored on eject, so the game is left exactly as it was found. */
static float g_slowcar_cooldown = 0.0f;   /* the game's own value, for eject */
static int   g_slowcar_score    = 130;    /* ditto                          */
static int   g_slowcar_saved    = 0;

/* ALL FIVE SCORES ISOLATION-TESTED, one at a time, driver at the wheel,
 * 2026-08-01, with tools/sc_say_test.py - the Python twin of sc_say():
 *
 *   131  SCAR05  "safety car is entering the pits now"
 *   132  SCAR01  "safety car has been deployed and is exiting the pits now"
 *   133  SCAR02  "safety car is on track"
 *   134  SCAR03  "safety car getting in at the end of this lap, get ready"
 *   135  SCAR04  "safety car is in"
 *
 * 134 is the WARNING at the start of his last lap; 135 is the line for the
 * moment he is gone. The first build had those two the other way round.
 *
 * Test trap worth keeping: a line fired while the previous one is still
 * playing comes out as the CARRIER's own clip (score 130, SLOWCAR01 "watch
 * out, there's a slow car ahead"), because the game builds the queue entry
 * only after sc_say has already put the original score back. Leave a few
 * seconds of silence between tests, and read isPlaying=1 as "result void". */
#define SCORE_SCAR05    131       /* "entering the pits now"              */
#define SCORE_SCAR01    132       /* "deployed, exiting the pits now"     */
#define SCORE_SCAR02    133       /* "is on track"                        */
#define SCORE_SCAR03    134       /* "getting in at the end of this lap"  */
#define SCORE_SCAR04    135       /* "safety car is in"                   */

typedef void (__attribute__((thiscall)) *StructInitFn)(void *buf);
typedef void (__attribute__((thiscall)) *DriverLookupFn)(void *buf, int eid,
                        int a3, int a4, int a5, int a6, float a7, float a8);
typedef char (__attribute__((thiscall)) *InsertEntryFn)(void *ctrl, void *buf);

#define FN_STRUCT_INIT   ((StructInitFn)   0x004144D0u)
#define FN_DRIVER_LOOKUP ((DriverLookupFn) 0x004144F0u)
#define FN_INSERT_ENTRY  ((InsertEntryFn)  0x00418560u)

/* ---- HUD message strip (FACT, mods/red_flag/gr29.c) -------------------- */
#define HUD_OUTER_PTR   0x007CD204u
#define HUD_SLOTS       8
#define HUD_STRIDE      0x5C
#define HUD_TEXT_OFF    0x3C
#define HUD_COLOR_OFF   0x8C
#define HUD_TIMER_OFF   0x90
#define HUD_TEXT_MAX    31
#define HUD_TEXT_RENDER 0x004DD030u
#define HUD_SCREEN_W    0x005D6D60u
#define HUD_SCREEN_H    0x005D6D64u
#define HUD_DRAW_SURFACE 0x007CDE14u
#define HUD_FILL_RECT   0x00494BF0u

#define UNLOAD_EVENT    "F1C_SAFETY_CAR_AUDITFIX_UNLOAD"
#define LOG_PATH        "pace_probe_standings_log.txt"

/* ---- state ------------------------------------------------------------- */
static HINSTANCE g_hinst;

static int    g_engaged      = 0;
static int    g_pace_speed   = 56;    /* m/s = 201.6 km/h, the VSC-style cap.
                                       * Driver 2026-08-02: "make it so that
                                       * they are capped at 200kmh ... like
                                       * with the F1CUI VSC mod". At this
                                       * level the cap only bites on the
                                       * straights - corners are already
                                       * slower - which is what a VSC looks
                                       * like, and it removes the "fixed
                                       * speed in a straight line" problem
                                       * the track shaping was added for. */
static int    g_include_player = 0;
static int    g_telemetry    = 0;   /* banner by default, Num9 for the numbers */

static unsigned g_saved_epoch[VEH_SLOTS];
static char g_saved_name[VEH_SLOTS][32];
static int g_saved_roster[VEH_SLOTS];
static float  g_saved[VEH_SLOTS];     /* pre-engage maxSpeed, per slot        */
static char   g_armed[VEH_SLOTS];     /* 1 = we governed this slot            */
static int    g_pos0[VEH_SLOTS];      /* race position at engage, per slot     */

static long   g_overrides    = 0;     /* times the game reset a governed car  */
static DWORD  g_engage_tick  = 0;
static double g_engage_stime = 0.0;

/* Ramp. The first live runs commanded 5 m/s from ~50 and the field decelerated
 * at roughly 15 m/s^2 - the whole grid standing on the brakes at once, which
 * is both unlike a pace lap and useless for judging whether ORDER holds. The
 * ramp walks the commanded speed down linearly instead, so what is observed is
 * pacing rather than a panic stop. Measured in SessionTime, not wall clock,
 * because the game keeps ticking through a pause.                           */
#define RAMP_SECONDS    ((double)g_sc_config.RAMP_SECONDS)
/* THE SAFETY CAR MUST BE SLOWER THAN THE FIELD WHILE IT GATHERS.
 *
 * Measured 2026-08-02 with g_pace_speed at the VSC value of 56 m/s: the pack
 * STRETCHED instead of closing up. on-slot 0/17, tail 2683 m, stuck at
 * CAUTION FORMING and never reaching FORMED, while the trailing train ran
 * 36-49 m/s. At 56 he is faster than the strung-out backmarkers are managing,
 * so the distance behind him can only grow. Compare the accepted 2026-08-01
 * result at the old low pace: tail 1424 m -> 517 m in 41 s.
 *
 * 56 m/s is a HOLD speed and works as one. It cannot also be the GATHER speed.
 * Real F1 runs the safety car slowly for exactly this reason and the pace
 * stops mattering once the queue exists.
 *
 * This also dissolves the trailing-train problem with no change to the
 * approach law. Each car is capped at the speed of the car ahead plus its own
 * distance allowance, so dropping the HEAD to the gather pace leaves the train
 * behind faster than the queue, and it closes on its own.
 *
 * THE TIMEOUT IS NOT OPTIONAL. Formation is judged on four fifths of the
 * running cars, never on all of them, because one car that is merely slow owns
 * the session otherwise - live 2026-08-02, Marc Gene at 38.4 m/s with a 54 m/s
 * cap, no pit, no damage, just slow. The timeout is the second guard behind
 * that fraction.                                                             */
static int    g_gather_pace  = 32;    /* m/s = 115 km/h while forming         */
static int    g_gather_secs  = 90;    /* then give up and go to the hold pace */
static DWORD  g_gather_start = 0;     /* tick he took the field, 0 = not yet  */
static double g_gather_sim = -1.0;
static int    g_sc_cmd       = 0;     /* what we last commanded him, 0 = free */

static int    g_ramp         = 1;     /* on by default                        */
static int    g_ramp_from    = 0;     /* m/s at engage, top of the ramp       */
static int    g_cmd_now      = 0;     /* what is actually being commanded     */

/* Ramp progress is accumulated in SIM milliseconds: the poll interval is added
 * only on ticks where SessionTime actually moved, so a paused game freezes the
 * ramp instead of silently running it out.
 *
 * It is NOT driven off SessionTime deltas directly. The 2026-07-31 60 s run
 * showed the two clocks disagreeing badly - 149.3 s of SessionTime inside 47.8 s
 * of GetTickCount, arriving in bursts (single one-second samples during which
 * SessionTime jumped 15-24 s). GetTickCount cannot stall, so SessionTime leapt.
 * That anomaly is unexplained and is logged per sample below so the next run
 * characterises it. Until it is understood, no duration is derived from
 * SessionTime alone.                                                        */
static double g_ramp_ms      = 0.0;
static double g_last_stime   = 0.0;

/* ---- bunching (PLAN.md Phase 3) ----------------------------------------
 * The Phase 5 hold proved the field keeps its ORDER under a uniform cap, but a
 * uniform cap cannot CLOSE the gaps: every car is limited to the same speed, so
 * the pack stays strung out exactly as it was when the caution began. A real
 * caution bunches the field up behind the leader.
 *
 * maxSpeed is a cap, not a throttle - the AI drives as fast as it is allowed.
 * So closing a gap needs nothing more than raising the cap for cars that are
 * too far back and lowering it for cars that are too close:
 *
 *     cap_i = pace + BUNCH_GAIN * (gap_i - target_gap)
 *
 * clamped so a catching car cannot exceed pace + BUNCH_CATCHUP and cannot be
 * slowed below BUNCH_FLOOR_PCT of pace. The leader (best position among the
 * governed cars) always gets the flat pace speed and is what the rest close on.
 *
 * Gap is measured along the track: lap_number * lap_length + lap_distance. The
 * lap length is not read from the .aiw here - it is LEARNED as the largest
 * lap_distance seen on any car, which converges within a lap and is logged so
 * it can be sanity-checked against the real track length.                    */
#define BUNCH_GAIN        0.25f   /* m/s of extra cap per metre of gap error   */

/* How much a car may exceed the pace speed to close up. Two regimes: far cars
 * get a big allowance so the field forms up in tens of seconds rather than
 * minutes (at +15 the measured arc closed ~50 m/s, which needed over a minute),
 * and cars already close get a small one so they ARRIVE at the car ahead
 * instead of running into it.                                               */
#define BUNCH_CATCHUP     35      /* m/s over pace when far away              */
#define BUNCH_CLOSE_RANGE 120.0f  /* metres: inside this, ease off            */
#define BUNCH_CLOSE_CATCH 8       /* m/s over pace when closing in            */
#define CATCHUP_FREE_M    250.0f  /* past this far from its slot, no cap at all  */
#define CATCHUP_JOIN_M    120.0f  /* back under pace control at this distance    */
#define BUNCH_FLOOR_PCT   0.55f   /* never cap a car below this fraction of pace */
/* A car that is physically ahead of the leader and a lap down has to be reeled
 * in, but it must not CRAWL. At 0.35 it did, and it read as a broken car rather
 * than a car being caught: driver report 2026-08-01, "some backmarkers slow to
 * an absurdly slow speed". 0.75 still closes the leader on it at a quarter of
 * the pace speed - roughly 100 m every 13 s at a 30 m/s pace - which is fast
 * enough without ever looking like a stall. The wave-around, when the policy
 * allows it, remains the proper answer for such a car.                      */
#define BUNCH_FALLBACK_PCT 0.75f  /* softer hold-back for a car being reeled in  */
#define BUNCH_AHEAD_FRAC  0.5f    /* beyond half a lap "behind" means ahead      */
/* No gap is rejected for being large. The first working run made that mistake:
 * gaps beyond 400 m were treated as unmeasurable, and since a racing field is
 * strung out over a 5.8 km lap only 2 of 20 cars ever had a "valid" gap. A
 * caution is precisely the case where the gaps START enormous - rejecting them
 * rejects the whole job. The controller clamps its OUTPUT instead (a car may
 * exceed pace by at most BUNCH_CATCHUP), so a 3 km gap is closed at a bounded
 * rate rather than ignored.                                                  */

/* ---- wave-around ("lapped cars may now overtake") -----------------------
 * Real safety-car procedure: cars a lap down are released to full racing speed,
 * drive past the leader and the whole queue, and rejoin at the BACK of it. They
 * unlap themselves on the way, because they cross the start line while the
 * leaders are held at pace speed.
 *
 * Implemented with the same single lever: a waving car's cap is simply removed
 * (maxSpeed back to the release value), so it runs at its own full race pace
 * with no artificial speed imposed. Nothing else is forged - the AI drives, the
 * lap counter increments on its own, and the game's own standings follow.
 *
 * State machine per car, so the DLL knows when to put the cap back:
 *   0 none | 1 running (uncapped) | 2 done (rejoined, back under pace control)
 * A car is "done" once it has passed the leader and then come back round to
 * within one target gap of the queue's tail. The pass-the-leader step must be
 * observed explicitly, otherwise a lapped car that is already behind the tail
 * would be declared finished before it moved.                               */
#define WAVE_NONE     0
#define WAVE_RUNNING  1
#define WAVE_DONE     2

static char   g_fallback[VEH_SLOTS];    /* held back on purpose this tick     */
static char   g_catchup[VEH_SLOTS];     /* uncapped, closing clear air        */

/* WHY each car was capped this tick. DIAGNOSTIC ONLY - nothing reads these
 * except the log.
 *
 * The law already releases a car with clear air (nothing inside APPROACH_RANGE
 * means no obstacle, which means no cap at all), and on paper the ramp is
 * above race pace beyond ~180 m anyway. A driver report of "a car far from the
 * queue is still capped" therefore names a BRANCH, not a tuning value - and
 * the aggregate QUEUE/BUNCH lines cannot say which branch, because they only
 * report gaps and slot errors. So record the branch per car and print the ones
 * that were slowed while a long way back.
 *
 * Add the evidence before touching the law: the last two regressions in this
 * file (the anti-ratchet floor, the clamped distance term) were both caused by
 * changing a control law against a symptom description.                     */
#define CAPW_NONE       0   /* uncapped: clear air, the car's own race pace   */
#define CAPW_LEADER     1   /* the pace setter, given the pace speed          */
#define CAPW_UNMEASURED 2   /* no lap length or no lap distance -> flat pace  */
#define CAPW_FALLBACK   3   /* physically ahead of the leader, reeled in      */
#define CAPW_WAVE       4   /* wave-around: released, not capped              */
#define CAPW_APPROACH   5   /* capped on the car ahead                        */
#define CAPW_SC         6   /* capped behind the safety car                   */
#define CAPW_FLOOR      7   /* the 45%-of-pace floor raised the command       */
#define CAPW_BLOCKED    8   /* sat behind a lapped / out-of-place car         */

static unsigned char g_cap_why[VEH_SLOTS];
static int           g_cap_obst[VEH_SLOTS];  /* obstacle slot, -1 if none     */
static float         g_cap_gap[VEH_SLOTS];   /* metres to it, -1 if none      */

static const char *cap_why_name(int w)
{
    switch (w) {
    case CAPW_NONE:       return "clear-air";
    case CAPW_LEADER:     return "pace-setter";
    case CAPW_UNMEASURED: return "unmeasurable";
    case CAPW_FALLBACK:   return "ahead-of-leader";
    case CAPW_WAVE:       return "waving";
    case CAPW_APPROACH:   return "approach";
    case CAPW_SC:         return "behind-SC";
    case CAPW_FLOOR:      return "floor";
    case CAPW_BLOCKED:    return "blocked-by-outofplace";
    }
    return "?";
}

/* Safety-car lap forcing, declared here because behind_leader() needs the
 * true lap to tell a lapped car from a slow one. Used by sc_lap_pump(). */
static int g_sc_lap_forced  = 0;
static int g_sc_lap_true    = 0;   /* his real lap, tracked while forced */
static int g_sc_lap_written = 0;   /* what this code last wrote          */
static float  g_sp_hist[VEH_SLOTS][SPD_HIST];  /* recent speeds, ring         */
static int    g_sp_idx = 0;
static float  g_saved_pass[VEH_SLOTS];  /* pre-caution PassingNorm            */
static float  g_saved_aggro[VEH_SLOTS]; /* pre-caution TrackAggression        */

static char   g_wave[VEH_SLOTS];        /* state above                        */
static char   g_wave_passed[VEH_SLOTS]; /* has it got past the leader yet     */

/* THE POLICY, Num/ toggles it. Two behaviours for cars a lap down:
 *
 *   0  NOT ALLOWED - lapped cars stay in the queue, in classification order,
 *      capped at pace speed like everyone else. One tidy train.
 *
 *   1  ALLOWED - a lapped car is uncapped, runs at full race speed until it
 *      catches the car at the back of the queue, and is then put back under
 *      pace control there. Applied continuously, so a car that goes a lap down
 *      later in the caution is released too.
 *
 * Both are real procedures from different eras: the modern wave-around is
 * policy 1, while F1 Challenge's own 1999-2002 rules had no unlapping under
 * the safety car at all, which is policy 0.                                 */
static int    g_wave_policy  = 0;

/* ---- caution state ------------------------------------------------------
 * F1C has no caution concept at all (zero yellow/pace/caution strings), so the
 * state lives here. Two phases, because they look different and the driver
 * needs to know which one they are in:
 *
 *   FORMING - the field is still closing up on the queue
 *   FORMED  - everyone is on their slot, the pace lap is stable
 *
 * PITS STAY OPEN throughout. Pitting under a safety car is normal and is a
 * real strategic decision, so it is deliberately not blocked - project decision
 * 2026-07-31. What must not happen is a car ignoring the pace on its way there:
 * that was the "TesterA" failure, and the fix was pit_owned() >= 2, so an in-lap
 * car stays under pace control until it is actually in the lane.            */
#define CAUTION_OFF     0
#define CAUTION_FORMING 1
#define CAUTION_FORMED  2

static int    g_caution      = CAUTION_OFF;

/* ---- THE YELLOW FLAG (SOLVED 2026-07-25, investigations/yellow_flag) -----
 * F1C DOES have flags, and they are per-ZONE, not per-timing-sector. The
 * Steward object at 0x8ACEE0 carries a table of 29 marshalling zones of about
 * 199 m each, covering the lap:
 *
 *   +0x108 float  scale; track_distance * scale = zone index
 *   +0x10C int32  zone count (29 on the sampled track)
 *   +0x110 ptr    array of { int32 state; float end_distance }, stride 8
 *   +0x114 ptr    the game's OWN previous-state copy, which it diffs
 *
 * state 0 = clear, state 2 = YELLOW (static: `cmp eax, 2` in the renderer's
 * yellow branch; observed live with a yellow actually flying).
 *
 * sub_4D1730 REBUILDS this table from scratch every frame: it clears every
 * zone, then for each car whose forward velocity is below a threshold derived
 * from its current track segment, sets THAT car's zone to 2. So a yellow means
 * "a car is going too slowly here" - the real rule, computed by the game.
 *
 * TWO CONSEQUENCES, and they pull in opposite directions:
 *
 *   1. The table cannot be poked. A written zone does not survive one frame.
 *   2. The table is therefore a CLEAN, game-computed incident signal, and it
 *      stays clean even while this DLL is running a caution - which is exactly
 *      what the watcher below needs.
 *
 * So the table is READ, never written, and the flag DISPLAY is forced
 * separately (see yellow_set).                                              */
#define STEWARD          0x008ACEE0u
#define ST_ZONE_SCALE    0x108u
#define ST_ZONE_COUNT    0x10Cu
#define ST_ZONE_ARRAY    0x110u
#define ZONE_STRIDE      8
#define ZONE_YELLOW      2

/* Forcing the flag ON the player's screen.
 *
 * The renderer asks one boolean - ShortComputation_bad0 at 0x0049BAD0 - which
 * returns "is the player's zone, or the one behind it, yellow". Three callers,
 * all display (HUDFlagRendererMain, the flag-widget updater sub_49D4C0, and a
 * function-pointer table). Nothing about physics or AI reads it.
 *
 * It takes no arguments and reads globals, so its entry is stack-balanced and a
 * `mov al,1 / ret` there is a legal early return. The first four bytes are
 *
 *   51 A1 E0 54     push ecx ; mov eax, [0x7C54E0](first byte of)
 *
 * and 0x49BAD0 is 16-byte aligned, so ONE aligned 32-bit store swaps the
 * function's head atomically - no torn instruction can ever be executed, which
 * matters because the render thread calls this every frame. The fourth byte is
 * preserved in both values so the store is a true swap of the first three.
 *
 * Restored on green, on release, and on self-eject.                          */
#define ADDR_YELLOW_PRED    0x0049BAD0u
#define YELLOW_BYTES_ORIG   0x54E0A151u   /* 51 A1 E0 54  push ecx; mov eax,.. */
#define YELLOW_BYTES_FORCE  0x54C301B0u   /* B0 01 C3 54  mov al,1; ret        */

static int  g_yellow      = 0;   /* is the display currently forced           */
static int  g_yellow_ok   = -1;  /* -1 unchecked, 0 bytes did not match       */

/* ---- the caution lifecycle (F1 procedure, NT2004 shape) -----------------
 * NT2004's sequence is watcher -> decision -> neutralise -> gather -> run ->
 * warn -> inspect -> release. The same arc is kept here, with NASCAR's rules
 * replaced by F1's 1999-2002 ones:
 *
 *   - The pit lane is NOT closed and never reopens in stages. Pitting under a
 *     safety car is normal and strategically real (project decision).
 *   - No lap deductions and no NASCAR restart apparatus.
 *   - The restart is the LEADER's, not a countdown: the safety car peels into
 *     the pit lane, the leader dictates the pace from there, and no car may
 *     pass until it crosses the start/finish line. That is CT_RESTART below,
 *     and it is the one phase with no NT2004 analogue worth porting.
 *
 * States. g_sc_state tracks where the CAR is; this tracks where the RACE is.  */
#define CT_OFF       0   /* green flag racing                                  */
#define CT_DEPLOY    1   /* called: yellow up, field slowing, car on its way   */
#define CT_PACE      2   /* safety car has the field; queue forming/holding    */
#define CT_IN        3   /* "safety car in this lap" - one lap to the green    */
#include "sc_identity_guard.h"
static SCIdentityGuard g_identity_guard;
#define CT_RESTART   4   /* car in the lane; leader controls; no pass until s/f */

static int    g_ct           = CT_OFF;
static int    g_ct_laps_tgt  = 2;    /* caution laps before the recall         */
/* What THIS caution runs for. An automatic declaration sets it from the size
 * of the incident (see trigger_eval's severity); a manual deployment always
 * gets the full g_ct_laps_tgt, because the driver asking for a safety car is
 * not a small incident by definition.                                       */
static int    g_ct_laps_run  = 2;
static int    g_ct_laps_done = 0;
static int    g_ct_lap_ref   = -1;   /* leader's lap number when counting began */

/* Auto-declaration is enabled for this automatic-incident test candidate.
 * The existing GUI can disable it.
 *
 * MANUAL DEPLOYMENT IS NEVER GATED BY ANY OF THIS. Home, and the GUI's deploy
 * button, call sc_deploy() directly: the driver asking for a safety car is an
 * instruction, not a proposal. Everything below only decides whether the DLL
 * may declare one BY ITSELF.                                                */
static int    g_ct_auto      = 1;
static DWORD  g_ct_seen_ms   = 0;    /* how long the incident has persisted    */
#define CT_AUTO_MIN_ZONES 1

/* ---- NT2004's gating and severity, ported (2026-08-03) -------------------
 *
 * Read from nt2004_ida_decompile: a caution there is declared by exactly one
 * thing, the incident scanner sub_4F93E0, and it is wrapped in three gates in
 * its caller sub_4F9040 plus two more inside the scanner itself. Those five
 * are worth having, because each one exists to stop a caution that would be
 * wrong rather than merely unlucky:
 *
 *   sub_4F9040  dword_75CBB0 == 7                 we are in the RACE session
 *   sub_4F9040  leader lap < steward+144          not on the final lap
 *   sub_4F9040  dword_767310 == 2                 FlagRules = All (our g_ct_auto)
 *   sub_4F93E0  10 s window after a restart       post-restart grace
 *   sub_4F93E0  every car has byte +0x5188        whole-field mask
 *
 * NOTE ON THE FIRST ONE: dword_75CBB0 is the SESSION-TYPE LADDER (1..7, race
 * = 7), advanced one step at a time by sub_4B99F0 - it is NOT a race phase
 * with 5=racing/7=caution, which is what this project's own DESIGN.md said
 * until 2026-08-03. The F1C equivalent is therefore SESSION_TYPE == 7, which
 * phase_racing() already covers together with RACE_PHASE.
 *
 * The severity split (sub_4F95D0: count the cars needing attention, full when
 * count >= threshold, else quickie; rain forces quickie) IS ported, because
 * "how many laps does this caution run for" is a real question and answering
 * it from the size of the incident is better than a fixed number. F1's own
 * procedure has no full/quickie names, so it shows up here purely as the
 * caution-lap count.                                                        */
#define TOTAL_RACE_LAPS  (*(volatile int*)0x008ACF8Cu)   /* FACT, post-reduction */

#define GATE_RACING     0x01   /* phase_racing(): race session, field sane     */
#define GATE_AUTO_ON    0x02   /* the driver allowed automatic declarations    */
#define GATE_IDLE       0x04   /* no caution and no safety car already running */
#define GATE_NOT_FINAL  0x08   /* the leader is not on the last lap            */
#define GATE_NO_GRACE   0x10   /* not inside the post-restart quiet window     */
#define GATE_NOT_MASKED 0x20   /* the whole field is not slow (grid/formation) */
#define GATE_INCIDENT   0x40   /* something is actually wrong out there        */
#define GATE_HELD       0x80   /* ...and it has persisted long enough          */
#define GATE_ALL (GATE_RACING|GATE_AUTO_ON|GATE_IDLE|GATE_NOT_FINAL| \
                  GATE_NO_GRACE|GATE_NOT_MASKED|GATE_INCIDENT|GATE_HELD)

/* A car below this is stopped, not slow. Deliberately well under anything a
 * car can be doing while still racing, so a hairpin cannot trip it.        */
#define TRIG_STOP_SPEED   ((double)g_sc_config.TRIG_STOP_SPEED)
/* Off-pace: moving, but at less than this fraction of the field's own median
 * speed. NT2004 uses a forward-progress dot product (sub_51C630 < 0.5) to
 * catch a car pointing the wrong way; F1C gives us no per-car heading we have
 * proven, so the field-relative speed test stands in for it. It catches the
 * same cases - a spun or crawling car - without needing an unverified offset. */
#define TRIG_OFFPACE_FRAC ((double)g_sc_config.TRIG_OFFPACE_FRAC)
/* Below this many running cars the median is meaningless, so off-pace is not
 * tested at all. */
#define TRIG_MIN_FIELD    ((int)g_sc_config.TRIG_MIN_FIELD)

static int    g_ct_quick_laps = 1;   /* caution laps for a small incident      */
static int    g_ct_full_thresh = 2;  /* involved cars that make it a full one   */
static int    g_ct_final_gate = 1;   /* honour the no-caution-on-the-last-lap   */
static int    g_ct_grace_secs = 10;  /* NT2004's own number                     */
static int    g_ct_hold_secs  = 3;   /* an incident must persist this long      */
/* When the last GREEN FLAG came out - not to be confused with g_green_ms
 * above, which is the first racing tick of the session and gates the instant
 * deployment. This one is restart-relative and only feeds the grace window. */
static DWORD  g_restart_green_ms = 0;

/* Published trace of the watcher, filled every tick by trigger_eval(). */
typedef struct {
    int   gates;
    int   zones, stopped, offpace, involved;
    int   hold_ms, hold_req, pct;
    int   worst_slot;
    float worst_speed, field_speed;
    int   leader_lap, total_laps, severity, grace_left;
    char  worst_name[24];
} TrigInfo;

static TrigInfo g_trig;

/* Restart bookkeeping: the lap number each car held when the green was armed.
 * A car is released the moment its lap number moves - i.e. when it crosses the
 * start/finish line, which is where F1 allows passing again.                 */
static int    g_restart_lap[VEH_SLOTS];
#include "sc_stuck_watch.h"
static SCStuckWatch g_stuck_watch[VEH_SLOTS];
static char   g_restart_free[VEH_SLOTS];
static int    g_green_said   = 0;
static double g_restart_sim_start, g_restart_sim_last;
static DWORD  g_restart_tick = 0;
static int g_recall_leader=-1, g_recall_lap=-1;
static unsigned g_recall_epoch;
static double g_recall_sim;
static char g_recall_name[32];
static int    g_restart_leader = -1;   /* locked at the recall, not per tick */
static int    g_restart_hold = 0;      /* explicit no-GREEN safety hold */
static int    g_restart_fault = 0;     /* SC/reset loss: explicit recovery required */
static int    g_restart_crossing_seen = 0; /* crossing consumed while unsafe */
static int    g_restart_roster = -1;
static unsigned g_restart_epoch = 0;
static char   g_restart_name[32];
/* The green cannot wait for ever on a leader who never registers a lap. */
#define RESTART_TIMEOUT_MS ((int)g_sc_config.RESTART_TIMEOUT_MS)

static int g_pit_handover=0;
static int    g_bunch        = 0;     /* off by default - Num* toggles it      */
static int    g_target_gap   = 20;    /* metres, Num+ / Num- adjust            */
static float  g_lap_len      = 0.0f;  /* learned, metres                       */
static float g_lap_last_d[VEH_SLOTS];
static int g_lap_last_n[VEH_SLOTS], g_lap_seeded;
static int    g_lap_len_exact = 0;    /* a car has been SEEN crossing the line */
static int    g_leader_slot  = -1;

/* What we last wrote per car. Without this the override counter counts our OWN
 * ramp steps: the first run reported "1155 game overrides", of which every
 * single one was this DLL changing its own commanded value 10 times a second
 * during the 6 s ramp (21 cars x 10 steps/s = 210/s, exactly the observed
 * slope), and the true count of game writes was ZERO.                       */
static float  g_last_written[VEH_SLOTS];

static char   g_hud_msg[HUD_TEXT_MAX + 1];
static char g_activation_reason[32]="MANUAL ACTIVATION";
static int g_activation_sector=0; /* 1..3 only after native track-sector lookup */
static DWORD  g_hud_until = 0;

/* ---- the control block: an EXTERNAL window, not an in-game overlay ------
 * The first attempt drew a menu on the game's own message strip. It worked, in
 * the narrow sense that text appeared - but the strip renders only its last
 * four slots, so the status line and half the rows were silently dropped, and
 * the result read as a status panel rather than something interactive
 * (screenshot, 2026-08-01). Owner's decision the same day: put the controls in
 * a separate window instead and stop fighting the game's HUD.
 *
 * The pattern is this project's own, already proven in
 * patches/mechanical_failures: a named shared-memory block, the GUI writes the
 * settings and one-shot commands, the DLL clears the commands and publishes
 * state back. No hooks, no injection into the GUI, and the game keeps running
 * if the window is closed.
 *
 *   GUI  -> DLL : cmd_* (one-shot, DLL zeroes them) and the settings
 *   DLL  -> GUI : everything below "published state", plus heartbeat
 *
 * Field ORDER IS THE ABI. Append, never reorder - tools/safety_car_gui.py
 * unpacks it positionally.                                                  */
#define SHM_NAME    "F1C_PACEPROBE_CTL"
#define CTL_MAGIC   0x50414345u          /* 'PACE' */

typedef struct {
    volatile unsigned magic;

    /* one-shot commands; the DLL clears each one after acting */
    volatile int cmd_safetycar;   /* deploy, or bring in and restart */
    volatile int cmd_engage;
    volatile int cmd_release;
    volatile int cmd_panic;
    volatile int cmd_dump;
    volatile int cmd_yellow;      /* 1 = show, 2 = clear */

    /* settings: the GUI owns these, the DLL applies them every tick */
    volatile int set_pace_speed;
    volatile int set_target_gap;
    volatile int set_caution_laps;
    volatile int set_wave_policy;
    volatile int set_bunch;
    volatile int set_auto_caution;
    volatile int set_include_player;
    volatile int set_ramp;
    volatile int set_sc_park;
    volatile int set_hide_mode;
    volatile int set_telemetry;

    /* published state */
    volatile int ct_state;
    volatile int sc_state;
    volatile int caution_phase;
    volatile int laps_done;
    volatile int engaged;
    volatile int cars_governed;
    volatile int cmd_now;
    volatile int leader_slot;
    volatile int sc_slot;
    volatile int sc_hidden;
    volatile int yellow;
    volatile int zones_yellow;
    volatile int race_phase;
    volatile int session_type;
    volatile int live_cars;
    volatile float lap_len;
    volatile float sc_gap_to_leader;
    volatile int   sc_ready;
    volatile unsigned heartbeat;

    /* APPENDED 2026-08-02. New fields go at the END and nowhere else: the
     * field order of this struct is the ABI shared with tools/safety_car_gui.py,
     * and an insertion anywhere above silently shifts every field after it. */
    volatile int set_gather_pace;   /* m/s while the queue is still forming */
    volatile int set_gather_secs;   /* give up gathering after this long    */

    /* APPENDED 2026-08-02b. Same rule: the END, and nowhere else. */
    volatile int set_hush;          /* suppress commentary about the SC     */
    volatile int hush_armed;        /* published: the detour is installed   */
    volatile int hush_drops;        /* published: lines dropped this session */

    /* APPENDED 2026-08-03 - the NT2004 gate/severity port and its trace.
     * Same rule as every block above: the END, and nowhere else.
     *
     * Everything named trig_* is PUBLISHED ONLY. It exists so the driver can
     * watch the watcher: how close the auto-declaration is, and which gate is
     * holding it back. Writing to it from the GUI does nothing.            */
    volatile int set_quick_laps;    /* caution laps for a QUICKIE            */
    volatile int set_full_thresh;   /* involved cars needed for a FULL       */
    volatile int set_final_lap;     /* honour "no caution on the final lap"  */
    volatile int set_grace_secs;    /* post-restart quiet window             */
    volatile int set_hold_secs;     /* how long an incident must persist     */

    volatile int   trig_gates;      /* bitfield, see GATE_* in the DLL       */
    volatile int   trig_zones;      /* marshalling zones yellow right now    */
    volatile int   trig_stopped;    /* cars stopped on track                 */
    volatile int   trig_offpace;    /* cars crawling relative to the field   */
    volatile int   trig_involved;   /* stopped + off-pace                    */
    volatile int   trig_hold_ms;    /* how long the incident has persisted   */
    volatile int   trig_hold_req;   /* how long it must persist              */
    volatile int   trig_pct;        /* 0..100, closeness to a declaration    */
    volatile int   trig_worst_slot;
    volatile float trig_worst_speed;
    volatile float trig_field_speed; /* median speed of the running field    */
    volatile int   trig_leader_lap;
    volatile int   trig_total_laps;
    volatile int   trig_severity;   /* 0 none, 1 quickie, 2 full             */
    volatile int   trig_grace_left; /* ms of post-restart grace remaining    */
    volatile char  trig_worst_name[24];

    /* APPENDED 2026-08-03b. THE PIT LANE IS OPEN AND THIS PROVES IT.
     * Published so "can everyone still pit under the safety car" is a number
     * on screen rather than a claim in a README. See pit_watch().          */
    volatile int cars_inlap;        /* called to the box, still on circuit   */
    volatile int cars_in_pits;      /* the game owns them: lane, box, garage */
    volatile int pitstops_caution;  /* cars that have pitted this caution    */

    /* APPENDED 2026-08-03c - START HIM IN HIS PIT BOX.
     *
     * Driver-verified live before any of this was written: teleport the car
     * into its own pit box, hand it to the game's pit machinery, and the game
     * runs a REAL pit stop on it (pittingStatus 2 -> 3 -> 4, crew out, tyres
     * changed) and then leaves it parked there indefinitely. See the finding
     * f1c-safety-car-starts-in-its-pit-box-via-teleport-plus-pit-handover.
     *
     * The DLL cannot work out WHERE the box is, so the GUI supplies it: the
     * DLL publishes the track's .aiw path (captured in the CreateFileA hook)
     * and the safety car's AIW team index, the GUI looks both up in
     * aiw_pits.json and writes the coordinates back here.                   */
    volatile int   set_pitstart;    /* GUI: start him in his pit box          */
    volatile int   box_valid;       /* GUI: the coordinates below are real    */
    volatile float box_x, box_y, box_z, box_yaw;
    volatile int   sc_team;         /* published: his veh+0x48A4             */
    volatile int   pitstart_state;  /* published: 0 idle 1 placed 2 held 3 released */
    volatile int   pitstart_fixes;  /* published: corrective re-places        */
    volatile char  track_aiw[96];   /* published: the loaded track's .aiw     */

    /* APPENDED 2026-08-03g - THE PIT-LANE APPROACH POINT.
     *
     * Teleporting into the box and forging the entry state is DISPROVEN: the
     * game re-runs its own pit logic off the XPITIN collision trigger and
     * overwrites anything written by hand. So the safety car is dropped on the
     * RACING LINE a couple of hundred metres before the lane instead, and
     * drives in himself. Supplied by the GUI out of aiw_pits.json's per-track
     * `approach` record, same route as the box above.                      */
    volatile int   app_valid;
    volatile float app_x, app_y, app_z, app_yaw;

    /* APPENDED 2026-08-04 - ABI guard fields (launcher PLAN Â§7). Append-only,
     * same rule as every block above: the END, and nowhere else.
     * abi_version = 1 for the first layout that carries these fields;
     * struct_size = sizeof(PaceCtl) at init. The launcher's SharedMemoryBinder
     * refuses to bind a block whose struct_size does not match the C# layout
     * it was built against - a drift here must fail loudly, not silently.   */
    volatile unsigned abi_version;
    volatile unsigned struct_size;
} PaceCtl;

static HANDLE   g_shm  = NULL;
static PaceCtl *g_ctl  = NULL;

/* Generic - unrelated tables elsewhere use the same unpack shape for
 * anything that would otherwise sit as a long literal in the data section. */
static void unpack_tag(char *out, const volatile unsigned char *enc, int n)
{
    int i;
    for (i = 0; i < n; i++) out[i] = (char)(enc[i] ^ 0x5A);
    out[n] = 0;
}

static unsigned long unpack_u32(const volatile unsigned char *enc)
{
    unsigned char b0 = (unsigned char)(enc[0] ^ 0x5A);
    unsigned char b1 = (unsigned char)(enc[1] ^ 0x5A);
    unsigned char b2 = (unsigned char)(enc[2] ^ 0x5A);
    unsigned char b3 = (unsigned char)(enc[3] ^ 0x5A);
    return (unsigned long)b0 | ((unsigned long)b1 << 8) |
           ((unsigned long)b2 << 16) | ((unsigned long)b3 << 24);
}

/* Disk I/O must not consume the broker callback's bounded game-thread budget.
 * Producers never wait: drop a diagnostic on contention/full buffer. */
#define SC_LOG_LINES 256
#define SC_LOG_WIDTH 1024
static char g_log_lines[SC_LOG_LINES][SC_LOG_WIDTH];
static unsigned g_log_read, g_log_count;
static volatile LONG g_log_lock, g_log_dropped;

static void logf_(const char *fmt, ...)
{
    va_list ap;
    char line[SC_LOG_WIDTH] = {0};
    va_start(ap, fmt);
    _vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = 0;
    if (InterlockedCompareExchange(&g_log_lock, 1, 0) != 0) {
        InterlockedIncrement(&g_log_dropped);
        return;
    }
    if (g_log_count < SC_LOG_LINES) {
        memcpy(g_log_lines[(g_log_read + g_log_count) % SC_LOG_LINES], line, sizeof(line));
        g_log_count++;
    } else InterlockedIncrement(&g_log_dropped);
    InterlockedExchange(&g_log_lock, 0);
}

#include "sc_usage.h"
/* OS worker only; never invoked by a game callback. */
static void log_flush(void)
{
    FILE *f;
    unsigned n;
    LONG dropped;
    int pending;
    if (InterlockedCompareExchange(&g_log_lock, 1, 0) != 0) return;
    pending = g_log_count != 0 || InterlockedCompareExchange(&g_log_dropped, 0, 0) != 0;
    InterlockedExchange(&g_log_lock, 0);
    if (!pending) return;
    f = fopen(LOG_PATH, "a");
    if (!f) return;
    for (n = 0; n < SC_LOG_LINES; n++) {
        char line[SC_LOG_WIDTH];
        if (InterlockedCompareExchange(&g_log_lock, 1, 0) != 0) break;
        if (!g_log_count) { InterlockedExchange(&g_log_lock, 0); break; }
        memcpy(line, g_log_lines[g_log_read], sizeof(line));
        g_log_read = (g_log_read + 1) % SC_LOG_LINES;
        g_log_count--;
        InterlockedExchange(&g_log_lock, 0);
        fputs(line, f); fputc('\n', f);
        scu_tap(line);
    }
    dropped = InterlockedExchange(&g_log_dropped, 0);
    if (dropped) fprintf(f, "[log] %ld diagnostics dropped (bounded queue)\n", dropped);
    fclose(f);
}

/* ---- file-open trace ----------------------------------------------------
 *
 * WHICH safety.hdv AND WHICH damage .ini DOES THE GAME ACTUALLY OPEN?
 *
 * Two sessions were spent inferring this from the file tree and from memory
 * scans, and both answers were wrong. The game holds neither the shared file's
 * WheelDetach (3500.0) nor a clamped copy, the safety car's .veh exists only
 * under _disabled_vehicles and in "backup FIA", and F1C resolves HDVehicle= and
 * DamageFile= BY NAME through a search path, so the folder a file sits in does
 * not tell you whether it wins.
 *
 * A Frida hook cannot answer it either: the MCP bridge tears the script down
 * between calls, so the hooks are gone before the track finishes loading. This
 * DLL persists across session loads, so it can.
 *
 * IAT patch rather than an inline detour - no trampoline, no code-section
 * write, and it is undone by putting the original pointer back. Patched in the
 * EXE and in msvcrt, because a C fopen() reaches CreateFileA through msvcrt's
 * own import table, not the exe's.                                          */
typedef HANDLE (WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
                                       DWORD, DWORD, HANDLE);

static CreateFileA_t g_real_cfa = NULL;
/* The .aiw the game last opened - i.e. the loaded track. Published so the GUI
 * can look its pit boxes up. Cleared on a session change. */
static char g_track_aiw[96] = {0};
/* Set by the CreateFileA hook when the track changes; consumed on the worker
 * thread, which drops the coordinates the GUI resolved for the OLD track. */
static volatile long g_track_changed = 0;
static int  g_ftrace_on = 0;
static volatile DWORD g_last_file_ms = 0;
static long g_ftrace_busy = 0;
static char g_ftrace_seen[96][MAX_PATH];
static int  g_ftrace_n = 0;

static int ftrace_interesting(const char *s)
{
    static const char *pat[] = { "damage", ".hdv", ".veh", "safety", "safgpxa" };
    char low[MAX_PATH];
    size_t i, n;
    unsigned k;

    if (!s) return 0;
    for (n = 0; n < MAX_PATH - 1 && s[n]; n++)
        low[n] = (char)((s[n] >= 'A' && s[n] <= 'Z') ? s[n] + 32 : s[n]);
    low[n] = 0;

    for (k = 0; k < sizeof(pat) / sizeof(pat[0]); k++)
        if (strstr(low, pat[k])) {
            /* Already reported? The game reopens the same file many times. */
            for (i = 0; i < (size_t)g_ftrace_n; i++)
                if (!strcmp(g_ftrace_seen[i], low)) return 0;
            if (g_ftrace_n < (int)(sizeof(g_ftrace_seen) / sizeof(g_ftrace_seen[0]))) {
                strncpy(g_ftrace_seen[g_ftrace_n], low, MAX_PATH - 1);
                g_ftrace_seen[g_ftrace_n][MAX_PATH - 1] = 0;
                g_ftrace_n++;
            }
            return 1;
        }
    return 0;
}

static HANDLE WINAPI my_CreateFileA(LPCSTR name, DWORD access, DWORD share,
                                    LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                    DWORD flags, HANDLE tmpl)
{
    /* The loader's own heartbeat. VehicleCount reaches its final value BEFORE
     * the per-car files are read, so a steady count is NOT proof the load has
     * finished - it sat at 23 for four seconds while Prost, Minardi, Arrows and
     * BAR were still to come, and hiding a car in that window crashed the load
     * twice (driver, 2026-08-01). File I/O going quiet is the honest signal. */
    g_last_file_ms = GetTickCount();

    /* THE TRACK'S OWN NAME, for free.
     *
     * The DLL needs the pit-box coordinates and cannot get them: the parsed
     * [PITS] table lives on the heap with no static pointer (searched, 2026-08-03),
     * and reading the .aiw needs a path the DLL has no other way to learn.
     * But every track load opens that very file through this hook, so the path
     * is handed to us. It is published in the control block and the GUI, which
     * has aiw_pits.json, sends the coordinates back. */
    /* EVERY .aiw, NOT JUST THE FIRST ONE.
     *
     * This used to keep the first path it ever saw. Load a second track without
     * restarting the game and the DLL still published the first one, so the GUI
     * resolved the WRONG TRACK'S coordinates and the safety car was placed at
     * them - measured 2026-08-03: Hockenheim loaded, DLL still reporting
     * 1999_Monza.AIW, and he was dropped at Monza's approach point (24.5, -2.9,
     * -98.1) in the middle of Hockenheim's stadium section, off the mesh and
     * falling through the world. Driver: "Bernd is currently spawned inside a
     * stadium and is falling through the world."                          */
    if (name) {
        size_t L = strlen(name);
        if (L > 4 && L < sizeof(g_track_aiw) &&
            (!_stricmp(name + L - 4, ".aiw")) &&
            _stricmp(name, g_track_aiw)) {
            lstrcpynA(g_track_aiw, name, sizeof(g_track_aiw));
            g_track_changed = 1;
        }
    }

    /* logf_ itself opens a file, so the trace must never re-enter itself. */
    if (!InterlockedExchange(&g_ftrace_busy, 1)) {
        if (ftrace_interesting(name)) logf_("  [file] %s", name);
        InterlockedExchange(&g_ftrace_busy, 0);
    }
    return g_real_cfa(name, access, share, sa, disp, flags, tmpl);
}

/* Swap one entry in one module's import address table. */
static int iat_patch(HMODULE mod, const char *dll, const char *fn,
                     void *newfn, void **oldfn)
{
    unsigned char *b = (unsigned char*)mod;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    DWORD rva;

    if (!mod) return 0;
    dos = (IMAGE_DOS_HEADER*)b;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS*)(b + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return 0;
    imp = (IMAGE_IMPORT_DESCRIPTOR*)(b + rva);

    for (; imp->Name; imp++) {
        const char *mname = (const char*)(b + imp->Name);
        IMAGE_THUNK_DATA *orig, *cur;
        if (_stricmp(mname, dll)) continue;
        if (!imp->OriginalFirstThunk) continue;
        orig = (IMAGE_THUNK_DATA*)(b + imp->OriginalFirstThunk);
        cur  = (IMAGE_THUNK_DATA*)(b + imp->FirstThunk);
        for (; orig->u1.AddressOfData; orig++, cur++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal)) continue;
            ibn = (IMAGE_IMPORT_BY_NAME*)(b + orig->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, fn)) continue;
            if (!VirtualProtect(&cur->u1.Function, sizeof(void*),
                                PAGE_EXECUTE_READWRITE, &old)) return 0;
            if (oldfn && !*oldfn) *oldfn = (void*)(size_t)cur->u1.Function;
            cur->u1.Function = (size_t)newfn;
            VirtualProtect(&cur->u1.Function, sizeof(void*), old, &old);
            return 1;
        }
    }
    return 0;
}

static void ftrace_start(void)
{
    int n = 0;
    if (g_ftrace_on) return;
    n += iat_patch(GetModuleHandleA(NULL), "kernel32.dll", "CreateFileA",
                   (void*)my_CreateFileA, (void**)&g_real_cfa);
    n += iat_patch(GetModuleHandleA("msvcrt.dll"), "kernel32.dll", "CreateFileA",
                   (void*)my_CreateFileA, (void**)&g_real_cfa);
    if (n && g_real_cfa) {
        g_ftrace_on = n;
        logf_("  [file] trace armed on %d import table(s) - load a session and "
              "every .hdv / .veh / damage .ini the game opens is listed here", n);
    } else {
        logf_("  [file] trace FAILED to arm (patched %d, real=%p)",
              n, (void*)g_real_cfa);
    }
}

static void ftrace_stop(void)
{
    if (!g_ftrace_on || !g_real_cfa) return;
    iat_patch(GetModuleHandleA(NULL), "kernel32.dll", "CreateFileA",
              (void*)g_real_cfa, NULL);
    iat_patch(GetModuleHandleA("msvcrt.dll"), "kernel32.dll", "CreateFileA",
              (void*)g_real_cfa, NULL);
    g_ftrace_on = 0;
}

/* ---- HUD --------------------------------------------------------------- */
/* Automatic warnings are once per caution cycle, even if different warnings
 * alternate. Explicit commands still use hud_show and always get feedback. */
static char g_hud_notices[32][HUD_TEXT_MAX+1];
static unsigned g_hud_notice_epoch;
static int g_hud_notice_count;
static unsigned g_hud_clock_seen;
static int g_hud_clock_valid;
static int hud_message_visible(void)
{
    unsigned clock_ms=sc_sim_millis(SESSION_TIME);
    /* RF/native restart rewinds SessionTime. Old messages belong to the old
     * clock; discard them instead of waiting for its deadline a second time.
     * Also consume expired text so it cannot resurrect on a later restart. */
    if ((g_hud_clock_valid && clock_ms<g_hud_clock_seen) ||
        (LONG)(g_hud_until-clock_ms)<0) g_hud_msg[0]=0;
    g_hud_clock_seen=clock_ms;g_hud_clock_valid=1;
    return g_hud_msg[0]!=0;
}
static void hud_notices_reset(void) { g_hud_notice_count=0; g_hud_notice_epoch=g_epoch; }
static void hud_show(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(g_hud_msg, HUD_TEXT_MAX, fmt, ap);
    va_end(ap);
    g_hud_msg[HUD_TEXT_MAX] = '\0';
    g_hud_until = sc_sim_millis(SESSION_TIME) + 4000;
    g_hud_clock_seen=sc_sim_millis(SESSION_TIME);g_hud_clock_valid=1;
}

static void hud_notice(const char *message)
{
    int i;
    if(g_hud_notice_epoch!=g_epoch)hud_notices_reset();
    for(i=0;i<g_hud_notice_count;i++)
        if(!strcmp(g_hud_notices[i],message))return;
    /* Bounded, fail quiet if future additions exceed the audited inventory. */
    if(g_hud_notice_count>=32)return;
    strncpy(g_hud_notices[g_hud_notice_count],message,HUD_TEXT_MAX);
    g_hud_notices[g_hud_notice_count++][HUD_TEXT_MAX]='\0';
    hud_show("%s",message);
}

/* Same ownership discipline as red flag hud_auto_notice_write/clear:
 * separate persistent lines, refresh owned slots, never overwrite another producer. */
static int g_sc_hud_slots[3] = {-1,-1,-1};
static char g_sc_hud_text[3][HUD_TEXT_MAX+1];
static uintptr_t g_sc_hud_inner;
static char g_np_hud_text[HUD_TEXT_MAX+1];
static char g_np_hud_detail[HUD_TEXT_MAX+1];
static unsigned g_np_hud_generation,g_np_hud_rendered_generation;
static unsigned char *g_hud_box_tramp;
static unsigned char g_hud_box_orig[10],g_hud_box_patch[10];
static int g_hud_box_hook_on,g_hud_box_hook_installed;

typedef void (__attribute__((thiscall)) *SCHudTextRender)(void*,const char*,
        int,int,uint32_t,int,float,float,char*);
typedef int (__attribute__((stdcall)) *SCHudFillRect)(int,int,int,int,int,uint32_t);

static int sc_hud_box_draw(int x,int y,const char *text,uint32_t native_color,
                           int line)
{
    int screen_w=*(volatile int*)HUD_SCREEN_W;
    int screen_h=*(volatile int*)HUD_SCREEN_H;
    int surface=*(volatile int*)HUD_DRAW_SURFACE;
    int left,width,height;
    SCStatusBox box;
    SCStatusPlacement placement;
    SCHudFillRect fill=(SCHudFillRect)HUD_FILL_RECT;
    if(!sc_status_placement(screen_w,screen_h,&placement)||
       surface<0x10000||!text||line<0||line>2)return 0;
    height=placement.row_height;
    if(line==0){
        if(!sc_status_box_spec(text,native_color,screen_h,&box))return 0;
        width=box.width;
        native_color=box.background_argb;
    }else{
        width=sc_status_detail_width(text,&placement,screen_w);
        native_color=0xE0101010u;
    }
    left=x-width/2;
    if(left<0||left+width>screen_w||y<0||y+height>screen_h)return 0;
    /* Native UI dialog fill, not 1-pixel lines. Both use the game's active
     * 2D surface; the fill call paints before this line's text is rendered. */
    fill(surface,left,y,width,height,native_color);
    return 1;
}

/* The game calls TextRenderDispatcher with ECX=context+4. This detour touches
 * only our owned status slot, directly before the native glyph render. */
static void __attribute__((fastcall)) sc_hud_box_detour(void *renderer,void *unused,
        const char *text,int x,int y,uint32_t color,int alignment,
        float scale_x,float scale_y,char *font)
{
    uint32_t shown_color=color;
    int line,slot=-1;
    int draw_x=x,draw_y=y;
    SCStatusPlacement placement;
    (void)unused;
    if(g_hud_box_hook_on&&g_sc_hud_inner&&
       (uintptr_t)renderer==g_sc_hud_inner+4&&text&&alignment==2){
        for(line=0;line<3;line++){
            slot=g_sc_hud_slots[line];
            if(slot>=0&&slot<HUD_SLOTS&&
               (uintptr_t)text==g_sc_hud_inner+slot*HUD_STRIDE+HUD_TEXT_OFF&&
               !strcmp(text,g_sc_hud_text[line])&&
               *(float*)(g_sc_hud_inner+slot*HUD_STRIDE+HUD_TIMER_OFF)>0.0f)
                break;
        }
        if(line<3&&sc_status_placement(*(volatile int*)HUD_SCREEN_W,
                         *(volatile int*)HUD_SCREEN_H,&placement)){
            draw_x=placement.x;
            draw_y=placement.y+line*placement.row_height;
            if(sc_hud_box_draw(draw_x,draw_y,text,color,line)&&line==0)
                shown_color=0xFF000000u;
        }
    }
    ((SCHudTextRender)g_hud_box_tramp)(renderer,text,draw_x,draw_y,shown_color,
                                      alignment,scale_x,scale_y,font);
}
static int hud_owned_line_visible(int line,const char *message)
{
    int n;
    uintptr_t inner=*(volatile uint32_t*)HUD_OUTER_PTR;
    if(!inner||inner!=g_sc_hud_inner||line<0||line>=3||!message)return 0;
    n=g_sc_hud_slots[line];
    return n>=0&&n<HUD_SLOTS&&
        *(float*)(inner+n*HUD_STRIDE+HUD_TIMER_OFF)>0.0f&&
        strcmp((const char*)(inner+n*HUD_STRIDE+HUD_TEXT_OFF),message)==0;
}
static void hud_line(uintptr_t inner, int line, const char *message,uint32_t color)
{
    int n = g_sc_hud_slots[line];
    if (n >= 0 && n < HUD_SLOTS) {
        char *text = (char*)(inner+n*HUD_STRIDE+HUD_TEXT_OFF);
        float *timer = (float*)(inner+n*HUD_STRIDE+HUD_TIMER_OFF);
        if (strncmp(text,g_sc_hud_text[line],HUD_TEXT_MAX+1)==0) {
            if (!message) {
                *timer = -1.0f;
                g_sc_hud_slots[line]=-1;g_sc_hud_text[line][0]=0;
                return;
            }
            strncpy(text,message,HUD_TEXT_MAX);text[HUD_TEXT_MAX]=0;
            strcpy(g_sc_hud_text[line],text);
            *(uint32_t*)(inner+n*HUD_STRIDE+HUD_COLOR_OFF)=color;
            *timer=1.5f;return;
        }
        g_sc_hud_slots[line]=-1;g_sc_hud_text[line][0]=0;
    }
    if (!message) return;
    for (n=0;n<HUD_SLOTS;n++) {
        float *timer=(float*)(inner+n*HUD_STRIDE+HUD_TIMER_OFF);
        if (*timer<=0.0f) {
            char *text=(char*)(inner+n*HUD_STRIDE+HUD_TEXT_OFF);
            strncpy(text,message,HUD_TEXT_MAX);text[HUD_TEXT_MAX]=0;
            strcpy(g_sc_hud_text[line],text);g_sc_hud_slots[line]=n;
            *(uint32_t*)(inner+n*HUD_STRIDE+HUD_COLOR_OFF)=color;
            *timer=1.5f;return;
        }
    }
}
static SCHudPhase sc_hud_phase(int visible)
{
    if(visible&&g_green_said&&
       (!strcmp(g_hud_msg,"GREEN")||!strcmp(g_hud_msg,"GREEN FLAG")))
        return SC_HUD_GREEN;
    if(!g_engaged)return SC_HUD_OFF;
    if(g_pit_handover||g_ct==CT_RESTART)return SC_HUD_IN_PITS;
    if(g_ct==CT_IN||g_sc_state==SC_RECALL)return SC_HUD_IN_THIS_LAP;
    if(g_sc_state==SC_ARMING||g_sc_state==SC_IDLE)
        return g_activation_sector>=1&&g_activation_sector<=3&&
               sc_hud_reason_window(SESSION_TIME,g_sc_arm_sim)?
               SC_HUD_REASON:SC_HUD_STANDING_BY;
    if(g_sc_state==SC_OUTLAP)return SC_HUD_DEPLOYING;
    return SC_HUD_ON_TRACK;
}
static int hud_non_status_message(const char *message)
{
    if(!message||!message[0]||!strcmp(message,"GREEN")||
       !strcmp(message,"GREEN FLAG")||!strcmp(message,"SAFETY CAR")||
       !strcmp(message,"NO OVERTAKING"))return 0;
    if(!strncmp(message,"SAFETY CAR ",11))
        return strstr(message,"FAULT")||strstr(message,"FAILED")||
               strstr(message,"LOST")||strstr(message,"UNAVAILABLE")||
               strstr(message,"NOT RESET");
    return 1;
}
static void hud_pump(void)
{
    uintptr_t inner=*(volatile uint32_t*)HUD_OUTER_PTR;
    int visible=hud_message_visible();
    SCHudPhase phase=sc_hud_phase(visible);
    SCHudStatus status=sc_hud_status(phase);
    const uint32_t white=0xFFFFFFFFu;
    if(inner!=g_sc_hud_inner) {
        for(int i=0;i<3;i++){g_sc_hud_slots[i]=-1;g_sc_hud_text[i][0]=0;}
        g_sc_hud_inner=inner;
    }
    if(!inner)return;
    if(g_confirm.action){
        char key[24],again[64];sc_key_name(g_confirm.action==3?(int)g_sc_config.EmergencyKey:(int)g_sc_config.DeployKey,key);
        wsprintfA(again,"PRESS %s AGAIN",key);
        const char *question=g_confirm.action==1?"DEPLOY SAFETY CAR?":
            (g_confirm.action==2?"RECALL SAFETY CAR?":"RESET SAFETY CAR?");
        hud_line(inner,0,status.primary?status.primary:question,
                 status.primary?status.color_argb:white);
        hud_line(inner,1,status.primary?question:again,white);
        hud_line(inner,2,status.primary?again:NULL,white);return;
    }
    if(status.primary){
        hud_line(inner,0,status.primary,status.color_argb);
        if(status.color_argb==0xFF00BC1Cu){
            hud_line(inner,1,NULL,white);hud_line(inner,2,NULL,white);
            return;
        }
        /* One native slot holds 31 characters. Keep the owner's full DQ
         * wording intact by using the two ordinary white detail slots. */
        int dq_notice=visible&&!strcmp(g_hud_msg,"DISQUALIFIED:");
        const char *detail=dq_notice?"DISQUALIFIED:":
            (g_np_hud_text[0]?g_np_hud_text:
            (phase==SC_HUD_REASON?g_activation_reason:
            (visible&&hud_non_status_message(g_hud_msg)?g_hud_msg:status.secondary)));
        hud_line(inner,1,detail,white);
        if(g_np_hud_text[0]&&hud_owned_line_visible(1,g_np_hud_text))
            g_np_hud_rendered_generation=g_np_hud_generation;
        char sector_text[24];
        const char *third=dq_notice?"Overtaking under SC":
            (g_np_hud_detail[0]?g_np_hud_detail:NULL);
        if(phase==SC_HUD_REASON&&!g_np_hud_text[0]&&!dq_notice) {
            if(g_activation_sector>=1&&g_activation_sector<=3)
                wsprintfA(sector_text,"SECTOR %d",g_activation_sector);
            else sector_text[0]=0;
            third=sector_text;
        }
        if(!third&&detail&&
           (!strcmp(detail,"SAFETY CAR RESET FAILED")||
            !strcmp(detail,"SAFETY CAR CONTROLS NOT RESET")||
            !strcmp(detail,"SAFETY CAR FAULT: RESTART GAME")))
            third="RESTART SESSION";
        hud_line(inner,2,third,white);
        return;
    }
    hud_line(inner,0,visible?g_hud_msg:NULL,white);
    hud_line(inner,1,NULL,white);hud_line(inner,2,NULL,white);
}

/* ---- field walk -------------------------------------------------------- */

/* A slot is live iff int32 at +0x32C8 != -1 (FACT). The driver name at +0x4620
 * makes a live slot self-identifying, so no grid list is ever hardcoded.     */
#include "f1c_vehicle_lifecycle.h"
#include "f1c_vehicle_native_clear.h"
static int slot_participant(int i)
{
    return i>=0 && i<VEH_SLOTS && f1c_lifecycle_participant(V_I32(i,OFF_ACTIVE),
        *(volatile unsigned char*)(VEH(i)+OFF_RETIRED),V_I32(i,OFF_FINISH));
}

static int slot_live(int i)
{
    return i >= 0 && i < VEH_SLOTS && V_I32(i, OFF_ACTIVE) != -1;
}

/* A car that is not moving is out of the race for pace-line purposes: parked,
 * wrecked or retired. It must not hold a slot in the queue (everyone behind it
 * would form up around a ghost) and must not enter the statistics - in the
 * first slot-based run the two stopped cars were the ONLY cars off their slot,
 * and their error grew at exactly the leader's speed, dragging the reported
 * mean to 104 m while every moving car sat on target.                       */
static int slot_moving(int i)
{
    return slot_participant(i) && isfinite(V_F32(i,OFF_SPEED)) && V_F32(i, OFF_SPEED) >= 0.5f;
}

/* Whether this slot belongs in the governed set. Slot 0 is the player and is
 * excluded by default: a human is not steered by maxSpeed, and the project
 * rule is to experiment on AI cars only.                                    */
static int slot_governed(int i)
{
    if (!slot_participant(i)) return 0;
    /* THE SAFETY CAR IS NEVER GOVERNED HERE, in any phase - except by
     * hold_pump()'s own gather block, which governs him directly while the
     * queue forms (see the g_gather_pace section). This function excludes him
     * from the FIELD loop so the queue never forms on a circular target: a
     * cap on him there would nerf the very speed everyone else is measured
     * against. Driver's spec, 2026-08-01.                                  */
    if (i == g_sc_slot && i >= 0) return 0;
    if (i == 0 && !g_include_player) return 0;
    /* >= 2, NOT != 0 - the SAME test as pit_owned(), which is the whole point.
     *
     * This was the other half of the TesterA bug and it survived the first fix.
     * pit_owned() was corrected to >= 2 so that a car sent to the box MID-hold
     * stays paced until it is actually in the lane, but this function kept
     * testing != 0 - so a car that was ALREADY on its in-lap when the caution
     * began was never captured at engage, and the mid-hold pickup loop calls
     * this same function, so it was never captured afterwards either. It kept
     * maxSpeed 99999 and its own aggression and simply raced.
     *
     * Caught live 2026-08-01: Marc Gene, pittingStatus 1, maxSpeed 99999,
     * TrackAggression 0.65, doing 36.4 m/s through a field held at 30 - driver
     * report, "has pit stop order active and is disregarding the rules and
     * overtakes everyone". Same symptom as TesterA, different code path.
     *
     * Status 1 is the in-lap: the car is on the circuit and is ours. Only from
     * 2 does the game drive it and apply its own pit limiter.               */
    if (V_I32(i, OFF_PITTING) >= 2) return 0;
    return 1;
}

/* The game drives this same maxSpeed field itself for pitting cars. Live proof,
 * 2026-07-31, straight out of a field dump with the probe idle: slot 4
 * (pittingStatus 3) and slot 7 (pittingStatus 2) both read maxSpeed = 22.2 =
 * flt_8ACFB4 = 80 km/h, while all 20 cars on track read 99999.
 *
 * So maxSpeed is a SHARED lever, not ours alone. Governing a car in the pit
 * lane would overwrite the game's own pit limiter at 20 Hz and make the AI
 * speed through its own pit lane - and every one of those corrections would be
 * miscounted as a "game override", wrecking the persistence measurement.
 *
 * Pitting cars are therefore left alone: not governed at engage, and dropped
 * (not restored - the game owns the value now) if they pit mid-hold.
 * Closing the pits under caution is Phase 2 and is not simulated here.      */
static int pit_owned(int i)
{
    /* >= 2, NOT != 0. pittingStatus 1 means "called to the box, driving the
     * in-lap" - the car is still out on the circuit and must stay under pace
     * control. Treating 1 as pit-owned released such a car to full race speed
     * in the middle of a caution: driver report 2026-07-31, "TesterA got hit and
     * then he broke out of formation and ran into the pits, overtaking EVERYONE
     * under safety car without any permission."
     *
     * The distinction was visible in this DLL's own first field dump: cars at
     * pittingStatus 1 read maxSpeed 99999 (no limiter, still racing), while
     * those at 2 and 3 read 22.2 - the game's own pit limiter. Only from 2
     * onward does the game drive the field itself.                          */
    return V_I32(i, OFF_PITTING) >= 2;
}

/* Capture the value to restore later. A car that has just left the pit lane can
 * still be carrying the 22.2 pit limit for a tick or two before the game lifts
 * it; restoring THAT to a car out on track would pin it at 80 km/h for the rest
 * of the race. Anything that low is not a real racing maxSpeed, so the release
 * default is used instead and the substitution is logged.                   */
/* Take the AI's appetite for overtaking away, and give it back. Both fields
 * can reset during native initialization; audit drift before assuming they hold
 * and the restore is exact. A car on its wave-around lap keeps its real
 * values - it is supposed to be passing.                                    */
static void ai_suppress_passing(int i)
{
    V_F32(i, OFF_PASSING) = PASSING_MIN;

    /* TRACKAGGRESSION IS NOT TOUCHED. Bug 4.6 was fixed for the safety car
     * alone (sc_ai_calm below) and left standing here, which was the same
     * mistake twice: AISteering reads 0x44F8 as the DESIRED FOLLOWING GAP,
     * floored at 20 - 10a metres, so flooring it at 0.01 tells all 21 cars to
     * hold ~20 m of clear air permanently. A caution queue is the one thing
     * that must do the opposite. Measured live 2026-08-01: the whole field
     * crawling at 13-17 m/s under a 30 m/s command, the race leader among
     * them, with every governed car reading aggro 0.01.
     *
     * Overtaking is suppressed by PassingNorm, and the speed cap already makes
     * advisory only. Nothing here needs an additional following-gap change.
     *
     * The repair is in capture_saved(): a car whose aggression is still 0.01
     * from an earlier build gets a real value back rather than being saved and
     * faithfully restored to the broken one. */
    if (V_F32(i, OFF_AGGRO) < 0.05f) V_F32(i, OFF_AGGRO) = AGGRO_DEFAULT;
}

/* The safety car's version: take away his willingness to OVERTAKE, and leave
 * his following behaviour alone.
 *
 * PassingNorm (0x44CC) is the overtaking knob - 0.9 is .rcd Passing = 0, i.e.
 * "never try". TrackAggression (0x44F8) is something else entirely: AISteering
 * uses it for the desired following gap (floored at 20 - 10a metres), the
 * side-by-side window and the per-opponent pressure accumulator. Flooring THAT
 * at 0.01 makes a car want ~20 m of clear air at all times and never close on
 * anything, which from the last grid box behind 22 cars looks exactly like a
 * car that will not accelerate. Driver report 2026-08-01: "safety car moves
 * from the grid very slowly when the race starts."
 *
 * He needs to push at 100% and not fight anyone. That is PassingNorm alone. */
static void sc_ai_calm(int i)
{
    V_F32(i, OFF_PASSING) = PASSING_MIN;
    /* Undo the earlier over-suppression if it is still on him. His own .rcd
     * value is reloaded on a fresh session; 0.65 is what the whole 1999 field
     * reads and is the documented fallback for a session we already clobbered. */
    if (V_F32(i, OFF_AGGRO) < 0.05f) V_F32(i, OFF_AGGRO) = AGGRO_DEFAULT;
}

/* Identity ownership permits restoration for a car that legitimately finishes
   or retires during this cycle.  Participation is a stricter condition used
   only to acquire or continue a racing control loan. */
static int capture_identity_owned(int i)
{
    return i >= 0 && i < VEH_SLOTS && g_saved_epoch[i] == g_epoch &&
        slot_live(i) && g_saved_roster[i]==V_I32(i,OFF_ROSTER) && memcmp(g_saved_name[i], V_STR(i, OFF_NAME), 32) == 0;
}
static int capture_owned(int i)
{
    return capture_identity_owned(i) && slot_participant(i);
}

#include "sc_queue_order.h"
static double g_evidence_now;
static SCQueueOrder g_queue_order;
#include "sc_order_settle.h"
static SCOrderSettle g_order_settle;
static int g_order_locked;
static int queue_observed(int i)
{
    return slot_live(i) && sc_queue_identity(&g_queue_order,i,g_epoch,
        V_I32(i,OFF_ROSTER),V_STR(i,OFF_NAME));
}
static void queue_order_pump(void)
{
    SCQueueCar cars[SC_QUEUE_SLOTS]={{0}};
    SCOrderMotion motion[SC_QUEUE_SLOTS]={{0}};
    for(int i=0;i<VEH_SLOTS;i++) {
        if(IsBadReadPtr((void*)VEH(i),0x48C0u))continue;
        cars[i].known=1;cars[i].participant=i!=g_sc_slot&&slot_participant(i);
        cars[i].pit=pit_owned(i);cars[i].position=V_I32(i,OFF_POSITION);cars[i].distance=V_F32(i,OFF_LAPDIST);
        cars[i].roster=V_I32(i,OFF_ROSTER);memcpy(cars[i].name,V_STR(i,OFF_NAME),32);
        motion[i].speed=V_F32(i,OFF_SPEED);
        motion[i].controlled=(i==0)||*(volatile unsigned char*)(VEH(i)+OFF_AICTRL)!=0;
        F1COrientation orientation=f1c_orientation_vehicle((const void*)VEH(i));
        motion[i].inverted=orientation.state==3;
    }
    int anchor=(g_sc_slot>=0 && slot_live(g_sc_slot) &&
        (g_sc_state==SC_ONTRACK || g_sc_state==SC_RECALL))?g_sc_slot:race_leader_slot();
    if(anchor<0 || !sc_queue_physical_positions(cars,VEH_SLOTS,V_F32(anchor,OFF_LAPDIST),g_lap_len))
        return; /* unknown physical order cannot authorize new giveback */
    int was_locked=g_order_locked;
    g_order_locked=sc_order_settle_step(&g_order_settle,cars,motion,VEH_SLOTS,g_epoch,
        g_evidence_now,g_engaged,g_sc_config.OrderSettleSeconds,g_sc_config.OrderQuietSeconds,
        g_sc_config.OrderStoppedSeconds,g_sc_config.OrderRecoverSeconds,
        (float)g_sc_config.OrderStoppedSpeed,(float)g_sc_config.OrderRecoverSpeed);
    if(!g_order_locked)sc_queue_reset(&g_queue_order);
    if(was_locked!=g_order_locked)logf_("[order-settle] %s",g_order_locked?"locked surviving queue; giveback enabled":"incident settling; giveback disabled");
    sc_queue_step(&g_queue_order,cars,VEH_SLOTS,g_epoch);
    for(int i=0;i<VEH_SLOTS;i++)g_pos0[i]=g_queue_order.rank[i];
}

static void ai_restore_passing(int i)
{
    if (!capture_identity_owned(i)) return;
    V_F32(i, OFF_PASSING) = g_saved_pass[i];
    V_F32(i, OFF_AGGRO) = g_saved_aggro[i];
}

static void capture_saved(int i)
{
    float v = V_F32(i, OFF_MAXSPEED);
    if (v < 1000.0f) {
        logf_("  [capture] slot %2d had maxSpeed=%.1f (pit limit residue?) - "
              "will restore %d instead", i, v, SPEED_RELEASE);
        v = (float)SPEED_RELEASE;
    }
    g_saved[i] = v;
}

/* All entry paths acquire the same complete loan before suppression. */
static int capture_car(int i)
{
    if (!slot_governed(i)) return 0;
    if (!isfinite(V_F32(i, OFF_PASSING)) || !isfinite(V_F32(i, OFF_AGGRO)) ||
        !isfinite(V_F32(i, OFF_MAXSPEED))) return 0;
    capture_saved(i);
    memcpy(g_saved_name[i], V_STR(i, OFF_NAME), 32);
    g_saved_epoch[i] = g_epoch;
    g_saved_roster[i] = V_I32(i,OFF_ROSTER);
    g_saved_pass[i] = V_F32(i, OFF_PASSING);
    g_saved_aggro[i] = V_F32(i, OFF_AGGRO);
    g_pos0[i] = V_I32(i, OFF_POSITION); /* queue_order_pump reconciles joins */
    g_last_written[i] = V_F32(i, OFF_MAXSPEED);
    g_armed[i] = 1;
    ai_suppress_passing(i);
    return 1;
}


/* THE PHASE ALONE IS NOT EVIDENCE THAT A RACE IS RUNNING.
 *
 * Between sessions the game leaves RACE_PHASE at 5 while the field is empty
 * and SessionTime is garbage. A DLL loaded into that window sees "racing",
 * and on 2026-08-01 it decremented VehicleCount there and the game crashed:
 *
 *   FIELD DUMP  phase=5 session=7 SessionTime=1000000000.00
 *     0 live, 0 governed
 *   === SAFETY CAR HIDDEN === VehicleCount 23 -> 22
 *
 * field_array_sane() did not catch it because the array was structurally
 * valid, just unpopulated. So the phase test also requires a plausible
 * SessionTime and at least two live cars - a real race has both, and the
 * teardown window has neither.                                             */
static int phase_racing(void)
{
    double t;
    int    i, live = 0;

    if (SESSION_TYPE != 7 || RACE_PHASE != PHASE_RACING) return 0;

    t = SESSION_TIME;
    if (!(t > 0.0) || t > 1.0e8) return 0;      /* also rejects NaN */

    for (i = 0; i < VEH_SLOTS; i++) {
        if (V_I32(i, OFF_ACTIVE) == -1) continue;
        if (++live >= 2) return 1;
    }
    return 0;
}

/* The game keeps ticking hot control functions through a pause, so elapsed
 * wall time is not evidence that the SIM advanced. Compare SessionTime.      */
static double session_elapsed(void)
{
    return SESSION_TIME - g_engage_stime;
}

/* ---- engage / hold / release ------------------------------------------- */

/* The speed to command this instant: the ramp value while descending, the
 * target once the ramp has run out. Integer because the setter takes an int. */
static int commanded_speed(void)
{
    double t, frac;
    int v;

    if (!g_ramp || g_ramp_from <= g_pace_speed) return g_pace_speed;

    t = g_ramp_ms / 1000.0;
    if (t <= 0.0)          return g_ramp_from;
    if (t >= RAMP_SECONDS) return g_pace_speed;

    frac = t / RAMP_SECONDS;
    v = (int)(g_ramp_from - (g_ramp_from - g_pace_speed) * frac + 0.5);
    if (v == SPEED_IDLE_SENTINEL) v = 19;   /* 18 means PitSpeedLimit */
    return v;
}

/* ---- bunching helpers -------------------------------------------------- */

/* Learn the lap length from the largest lap_distance ever seen. Cheap, needs no
 * .aiw parsing, and converges as soon as any car approaches the start line. */
static void learn_lap_length(void)
{
    float *last_d = g_lap_last_d;
    int *last_n = g_lap_last_n;
    int i;

    if (!g_lap_seeded) {
        for (i = 0; i < VEH_SLOTS; i++) { last_d[i] = -1.0f; last_n[i] = -1; }
        g_lap_seeded = 1;
    }

    /* SEED THE LAP LENGTH FROM THE STEWARD AT LOAD.
     *
     * The no-passing law needs the lap length to measure gaps between cars.
     * Learning it only from a line crossing means it is unknown for the whole
     * of lap 1, so on a lap-1 caution gaps read "unmeasurable", every car gets
     * the flat pace cap with no gap control, and the field overtakes freely -
     * driver report 2026-08-08, "DRIVERS ARE OVERTAKING EACHOTHER DURING
     * CAUTION". The Steward's marshalling-zone table already carries the lap
     * length at track load: segments / scale, proven byte-exact against the
     * shipped .AIW on every track (tools/steward_laplen.py, e.g. Monza
     * 29 / 0.005048069 = 5744.7702 vs the AIW's 5744.77). Read it once, before
     * anyone has crossed the line, so gaps are measurable - and the no-passing
     * law active - from the green flag. A real crossing later can still refine
     * it (the maximum-can-no-longer-lower-it rule below). */
    if (!g_lap_len_exact) {
        float scale = *(volatile float*)(STEWARD + 0x108u);
        int   segs  = *(volatile int*)  (STEWARD + 0x10Cu);
        if (scale > 0.0f && segs > 0) {
            float lap = (float)segs / scale;
            if (lap > 500.0f && lap < 30000.0f) {
                g_lap_len       = lap;
                g_lap_len_exact = 1;
                logf_("  [lap] lap length seeded from the Steward at %.1f m "
                      "(segments %d / scale %.9f) - gaps measurable and the "
                      "no-passing rule active from the green flag", lap, segs,
                      scale);
            }
        }
    }

    /* THE EXACT VALUE COMES FROM A CAR CROSSING THE LINE, NOT FROM A MAXIMUM.
     *
     * The running maximum below is only ever as large as the furthest car has
     * got, so on lap 1 it equals the LEADER'S OWN distance - and the deploy
     * gate, which asks "how far is the leader from the line", then reads about
     * zero on every tick and lets him straight out. Driver, 2026-08-03: "why
     * is the guardrail of how close the leader is not working". Log: "the
     * leader is 0 m from the line" on lap 1, against 421 m later in the race.
     *
     * When a car's lap counter increments it has just crossed the line, so the
     * distance it held on the previous tick IS the lap length. That is the
     * game measuring the track for us, no .aiw and no static number. It is
     * taken as authoritative and the maximum can no longer lower it.       */
    for (i = 0; i < VEH_SLOTS; i++) {
        float d;
        int   n;
        if (!slot_live(i)) { last_d[i] = -1.0f; last_n[i] = -1; continue; }
        d = V_F32(i, OFF_LAPDIST);
        n = V_I32(i, OFF_LAPNUM);
        if (last_n[i] >= 0 && n == last_n[i] + 1 &&
            last_d[i] > 500.0f && last_d[i] < 30000.0f) {
            if (!g_lap_len_exact || last_d[i] > g_lap_len) {
                logf_("  [lap] lap length measured at %.1f m - slot %d crossed "
                      "the line (was %.1f m by running maximum)",
                      last_d[i], i, g_lap_len);
                g_lap_len       = last_d[i];
                g_lap_len_exact = 1;
            }
        }
        last_d[i] = d;
        last_n[i] = n;
    }

    if (g_lap_len_exact) return;

    for (i = 0; i < VEH_SLOTS; i++) {
        float d;
        if (!slot_live(i)) continue;
        d = V_F32(i, OFF_LAPDIST);
        if (d > g_lap_len && d < 30000.0f) g_lap_len = d;
    }
    /* Until some car has been seen near the start line the estimate is short,
     * which would make wrapped gaps nonsense. Treat anything implausibly small
     * as "not learned yet" - gap_ahead() then reports unmeasurable and every
     * car simply holds the flat pace speed.                                 */
    if (g_lap_len < 500.0f) g_lap_len = 0.0f;
}

/* ---- the track's own speed profile --------------------------------------
 *
 * A FLAT PACE SPEED IS THE WRONG SHAPE. Commanding one number makes the field
 * run at exactly that number everywhere - fine through a corner, absurd down a
 * straight. Driver report 2026-08-01: "cars appear to be driving at 110 kmh
 * when finishing the lap ... driving 110kmh fixed in a straight line looks a
 * bit stupid."
 *
 * A percentage only means something relative to what a piece of track is
 * WORTH, so the shape is learned rather than assumed: while the race is green
 * and this DLL is idle, the fastest speed any car has shown in each of
 * PROF_N slices of the lap is recorded. That is a free measurement - the cars
 * are already driving the track at racing speed, and no lap is wasted on it.
 *
 * Under caution the pace becomes pace x profile[here] / mean(profile), so the
 * MEAN speed is still exactly the pace setting the GUI shows, but the field is
 * quick down the straights and slow through the corners in the same proportion
 * the track itself has. The existing pace slider keeps its meaning.
 *
 * Falls back to the flat number until the profile is populated, so a caution in
 * the first half lap behaves exactly as before rather than unpredictably.   */
#define PROF_N        96
#define PROF_READY    70        /* slices that must be filled before it is used */
#define PROF_LO       0.55f     /* never shape below this fraction of the pace  */
#define PROF_HI       1.90f     /* nor above it - a straight is not a free lap  */

static int   g_shape = 0;      /* track-shaped pace: off at a VSC cap */
static float g_prof[PROF_N];
static float g_prof_mean = 0.0f;
static int   g_prof_n    = 0;

/* Called every tick while the race is green and the caution is NOT engaged. */
static void prof_sample(void)
{
    int i;
    if (!isfinite(g_lap_len) || g_lap_len<100.0f || g_lap_len>100000.0f) return;

    for (i = 0; i < VEH_SLOTS; i++) {
        float d, s;
        int   b;
        if (!slot_live(i) || !slot_moving(i) || pit_owned(i)) continue;
        d = V_F32(i, OFF_LAPDIST);
        if (!isfinite(d) || d < 0.0f || d > g_lap_len) continue;
        b = (int)(d / g_lap_len * PROF_N);
        if (b < 0 || b >= PROF_N) continue;
        s = V_F32(i, OFF_SPEED);
        if (isfinite(s) && s>=0.0f && s<=200.0f && s > g_prof[b]) g_prof[b] = s;
    }

    /* Recompute the mean over filled slices only - an unfilled slice would
     * otherwise drag the mean down and inflate every shaped command.        */
    {
        int   n = 0;
        double sum = 0.0;
        for (i = 0; i < PROF_N; i++)
            if (g_prof[i] > 0.0f) { sum += g_prof[i]; n++; }
        g_prof_n    = n;
        g_prof_mean = n ? (float)(sum / n) : 0.0f;
    }
}

/* The pace this car should be held to HERE, given the flat pace `base`. */
static int pace_shaped(int i, int base)
{
    float d, v, lo, hi;
    int   b;

    /* Shaping is OFF at a VSC-style cap. It exists so that a LOW flat pace
     * (30 m/s) does not look robotic down the straights; at 56 m/s the cap is
     * already above every corner speed, so the field varies naturally and
     * shaping would only pull the straights back down again.              */
    if (!g_shape) return base;
    if (g_prof_n < PROF_READY || !isfinite(g_prof_mean) || g_prof_mean <= 0.0f) return base;
    if (!isfinite(g_lap_len) || g_lap_len<100.0f || g_lap_len>100000.0f) return base;

    d = V_F32(i, OFF_LAPDIST);
    if (!isfinite(d) || d < 0.0f || d > g_lap_len) return base;
    b = (int)(d / g_lap_len * PROF_N);
    if (b < 0 || b >= PROF_N || !isfinite(g_prof[b]) || g_prof[b] <= 0.0f) return base;

    v  = (float)base * g_prof[b] / g_prof_mean;
    if(!isfinite(v) || v<0.0f || v>200000.0f)return base;
    lo = (float)base * PROF_LO;
    hi = (float)base * PROF_HI;
    if (v < lo) v = lo;
    if (v > hi) v = hi;

    if ((int)(v + 0.5f) == SPEED_IDLE_SENTINEL) return 19;
    return (int)(v + 0.5f);
}

/* Every distance consumer shares this finite bounded normalization.  A corrupt
   telemetry float is an observation failure, never callback work. */
static int sc_lap_valid(float lap) { return isfinite(lap) && lap>=100.0f && lap<=100000.0f; }
static int sc_wrap_distance(double raw,double lap,double *out) {
    if(!out||!isfinite(raw)||!isfinite(lap)||lap<100.0||lap>100000.0||fabs(raw)>lap*4.0)return 0;
    raw=fmod(raw,lap);if(raw<0.0)raw+=lap;*out=raw;return 1;
}

/* Metres from car `i` to the car directly ahead of it on the road, or -1 if
 * there is no car ahead (the leader) or the gap is not usable. */
static float gap_ahead_ex(int i, int *ahead_slot)
{
    int j, best_j = -1;
    double mine, best_d = -1.0;

    if (ahead_slot) *ahead_slot = -1;
    if (!sc_lap_valid(g_lap_len)) return -1.0f;
    mine = (double)V_F32(i, OFF_LAPDIST);
    if (!isfinite(mine) || mine <= 0.0 || mine>=g_lap_len) return -1.0f;

    /* The car ahead is the PHYSICALLY nearest one on the road - the smallest
     * positive distance around the lap - not the car one place better in the
     * race order.
     *
     * Race order was the first attempt and it broke on lapped traffic, live:
     * the field spanned more than one lap, so for a lapped pair the car ahead
     * in the standings was physically just BEHIND, and its modular distance
     * came back as ~5760 m on a 5776 m lap instead of ~16 m. Those cars then
     * chased a gap that did not exist, and the reported pack length came to
     * 47.7 km on a 5.8 km track.
     *
     * A pace line is a physical queue. Measuring it physically also removes
     * any dependence on the standings being consistent with track position,
     * which is exactly what a caution breaks.                              */
    for (j = 0; j < VEH_SLOTS; j++) {
        double d;
        if ((!g_armed[j] && !(j==0 && queue_observed(j))) || !slot_participant(j) || pit_owned(j) || j == i) continue;
        if (!isfinite(V_F32(j,OFF_LAPDIST)) || V_F32(j, OFF_LAPDIST) <= 0.0f || V_F32(j,OFF_LAPDIST)>=g_lap_len) continue;

        d = (double)V_F32(j, OFF_LAPDIST) - mine;
        if (!sc_wrap_distance(d,g_lap_len,&d)) continue;

        if (d < 1.0) continue;                 /* side by side, or ourselves */
        if (best_d < 0.0 || d < best_d) { best_d = d; best_j = j; }
    }
    if (best_d < 0.0) return -1.0f;
    if (ahead_slot) *ahead_slot = best_j;
    return (float)best_d;
}

static float gap_ahead(int i)
{
    return gap_ahead_ex(i, NULL);
}

/* Lowest speed the car has shown in the last half second. */
static float speed_floor(int i)
{
    int   n;
    float lo = g_sp_hist[i][0];
    for (n = 1; n < SPD_HIST; n++)
        if (g_sp_hist[i][n] < lo) lo = g_sp_hist[i][n];
    return lo;
}

static void speed_hist_pump(void)
{
    int i;
    g_sp_idx = (g_sp_idx + 1) % SPD_HIST;
    for (i = 0; i < VEH_SLOTS; i++)
        g_sp_hist[i][g_sp_idx] = slot_participant(i) ? V_F32(i, OFF_SPEED) : 0.0f;
}

/* MAY CAR j DICTATE CAR i's PACE, OR IS IT THE ONE THAT IS OUT OF PLACE?
 *
 * Distance alone cannot answer this, and no second distance measure can - a
 * 20 m gap looks identical whether the car ahead is the true queue predecessor
 * or a lapped car fifteen places down that has ended up between P4 and P5.
 * The answer is in the classification, which every car carries: live race
 * position at +0x4060 and lap number at +0x403C.
 *
 * Why it matters: approach_cap() takes its base speed from whatever is
 * physically ahead. Before this test, a lapped P20 sitting between P4 and P5
 * became P5's pace setter - and because the law chains, every car behind P5
 * inherited that cap too. g_fallback did not catch it, because that flag is
 * only ever set for a car physically ahead of THE LEADER, and this one is not.
 *
 * The test is inherently ASYMMETRIC, which is what keeps it safe: if j is out
 * of place for i, then i is the better-classified car, and from j's side i is
 * BEHIND and therefore not an obstacle at all. No pair can cap each other, so
 * this cannot reproduce the mutual-cap deadlock that once pinned eleven cars
 * at maxSpeed 5.
 *
 * Conservative on missing data: an unreadable position keeps the old
 * behaviour (treat the car as a legitimate pace setter) rather than ignoring
 * a real obstacle.                                                          */
static int obstacle_is_pace_setter(int i, int j)
{
    int pi, pj, li, lj;

    /* THE SAFETY CAR ALWAYS SETS THE PACE, AND IS CHECKED FIRST.
     *
     * He is not classified - his position and lap number are not comparable to
     * a racing car's, and his lap is deliberately held one above the race
     * leader's by sc_lap_pump(). Both tests below would therefore reject him,
     * which would drop him out of the pace selection entirely and take the
     * never-passable branch in approach_cap() with him. He got passed once
     * already through a flag that said "meant to be passed"; this must not be
     * a second route to the same result.                                    */
    if (j == g_sc_slot && sc_is_out()) return 1;

    li = V_I32(i, OFF_LAPNUM);
    lj = V_I32(j, OFF_LAPNUM);
    if (lj < li) return 0;              /* a lap or more down, and in the way */
    if (lj > li) return 1;              /* a lap up: legitimately ahead of me */

    pi = V_I32(i, OFF_POSITION);
    pj = V_I32(j, OFF_POSITION);
    if (pi <= 0 || pj <= 0) return 1;   /* unmeasurable: keep old behaviour   */
    return (pj < pi);                   /* better position ahead of me = correct */
}

/* The nearest car this one must not run into: anything from ABREAST_WINDOW
 * behind us (already alongside) out to APPROACH_RANGE ahead. Returns the gap,
 * clamped at 0 for a car that is level or fractionally past us, or -1 if there
 * is nothing to respect.
 *
 * pace_only = 1 considers only cars entitled to set this car's pace
 * (obstacle_is_pace_setter). pace_only = 0 returns the nearest physical car of
 * any kind, which is what the collision floor needs - being out of place does
 * not make a car intangible.                                                */
static float nearest_obstacle(int i, int *out_slot, int pace_only)
{
    int j, best_j = -1;
    double mine, best = 1e18;

    if (out_slot) *out_slot = -1;
    if (!sc_lap_valid(g_lap_len)) return -1.0f;
    mine = (double)V_F32(i, OFF_LAPDIST);
    if (!isfinite(mine) || mine <= 0.0 || mine>=g_lap_len) return -1.0f;

    for (j = 0; j < VEH_SLOTS; j++) {
        double d;
        if (!slot_live(j) || j == i) continue;
        if (!slot_moving(j)) continue;

        /* A CRAWLING CAR IS NOT A PACE SETTER, IT IS AN OBSTRUCTION.
         *
         * slot_moving() only asks for 0.5 m/s, which a wreck rolling to a halt
         * still satisfies. The approach law then caps the car behind at 60% of
         * that, and the queue concertinas down to the floor one car at a time.
         *
         * Live 2026-08-01: Ralf Schumacher ended up UPSIDE DOWN on the circuit
         * and did not despawn, reading 1.2 km/h. Marc Gene arrived behind him
         * and was pinned at maxSpeed 5 - the floor - along with Schumacher
         * himself. Driver saw the field crawling.
         *
         * Under a real caution you pass a wrecked car; you do not form up
         * behind it. So anything running below OBSTACLE_MIN is not respected
         * as an obstacle at all.                                            */
        if (speed_floor(j) < OBSTACLE_MIN) continue;
        /* A car in the pit lane is not on the racing line and must never cap
         * the car "behind" it. This matters at the restart: the safety car is
         * peeling into the lane right in front of the leader, and holding the
         * leader to the speed of a car on the pit limiter would stall the whole
         * green.                                                            */
        if (pit_owned(j)) continue;
        if (!isfinite(V_F32(j,OFF_LAPDIST)) || V_F32(j, OFF_LAPDIST) <= 0.0f || V_F32(j,OFF_LAPDIST)>=g_lap_len) continue;
        if (pace_only && !obstacle_is_pace_setter(i, j)) continue;

        d = (double)V_F32(j, OFF_LAPDIST) - mine;
        if (!sc_wrap_distance(d,g_lap_len,&d)) continue;

        /* Just behind us reads as almost a full lap ahead - fold it back so an
         * abreast car is seen as the small negative number it really is.    */
        if (d > (double)g_lap_len - (double)ABREAST_WINDOW) d -= (double)g_lap_len;

        if (d < -(double)ABREAST_WINDOW) continue;   /* genuinely behind */
        if (d > (double)APPROACH_RANGE)  continue;   /* too far to matter */

        /* AN ABREAST CAR IS AN OBSTACLE TO ONE OF THE PAIR, NOT TO BOTH.
         *
         * The fold above deliberately turns a car up to ABREAST_WINDOW BEHIND
         * into an obstacle at gap 0, so that a car being drawn alongside still
         * gets capped. Applied to both cars it is mutual: each caps the other
         * at the other's speed, and any dip either of them takes is copied
         * back and forth until the pair crawls. The standings break the tie -
         * the car with the better race position is the one being defended, so
         * only the car behind IN THE ORDER respects it.                     */
        if (d < 0.0) {
            int pj = V_I32(j, OFF_POSITION), pi = V_I32(i, OFF_POSITION);
            if (!(pj > 0 && pi > 0 && pj < pi)) continue;
        }

        if (d < best) { best = d; best_j = j; }
    }

    if (best_j < 0) return -1.0f;
    if (out_slot) *out_slot = best_j;
    return (best < 0.0) ? 0.0f : (float)best;
}

/* The car the pack forms up behind: best race position among governed cars that
 * are actually moving - a parked car must never become the pace setter.     */
static int find_leader(void)
{
    int i, best = -1, best_pos = 1 << 30;

    /* When the safety car is out it IS the pace line's leader, whatever the
     * standings say - it is not even classified. This is the one place the
     * queue stops being led by the race leader.                           */
    if (sc_is_out() && g_sc_slot >= 0 && slot_live(g_sc_slot))
        return g_sc_slot;

    for (i = 0; i < VEH_SLOTS; i++) {
        int p;
        if (!g_armed[i] || !slot_live(i)) continue;
        if (V_F32(i, OFF_SPEED) < 0.5f) continue;
        p = V_I32(i, OFF_POSITION);
        if (p > 0 && p < best_pos) { best_pos = p; best = i; }
    }
    return best;
}

/* How many governed, moving cars hold a better race position than car i - that
 * is car i's place in the queue, counting the leader as 0. Race position is
 * used, not track position, because a caution forms the field up in RACE
 * ORDER.                                                                    */
static int queue_index(int i)
{
    int j, my_pos = queue_observed(i)?g_pos0[i]:V_I32(i, OFF_POSITION), k = 0;
    for (j = 0; j < VEH_SLOTS; j++) {
        int p;
        if (!queue_observed(j) || pit_owned(j) || j == i) continue;
        if (!slot_moving(j)) continue;
        if (j == g_sc_slot && sc_is_out()) continue;   /* the leader, unclassified */
        p = g_pos0[j];
        if (p > 0 && p < my_pos) k++;
    }
    /* With the safety car out, even the race leader queues one gap behind it. */
    return sc_is_out() ? k + 1 : k;
}

/* Metres this car currently sits behind the leader, measured around the lap.
 * Returns a NEGATIVE value if the car is physically AHEAD of the leader, which
 * is what a lapped car looks like. -1e9 means unmeasurable.                 */
static float behind_leader(int i)
{
    double mine, lead, d;

    if (g_leader_slot < 0 || !sc_lap_valid(g_lap_len)) return -1e9f;
    mine = (double)V_F32(i, OFF_LAPDIST);
    lead = (double)V_F32(g_leader_slot, OFF_LAPDIST);
    if (!isfinite(mine)||!isfinite(lead)||mine <= 0.0 || lead <= 0.0 || mine>=g_lap_len||lead>=g_lap_len) return -1e9f;

    d = lead - mine;
    if (!sc_wrap_distance(d,g_lap_len,&d)) return -1e9f;

    /* THE HALF-LAP FOLD NEEDS THE LAP NUMBER, OR IT LIBELS A SLOW CAR.
     *
     * On a closed circuit "4037 m behind" and "1705 m ahead" are the same
     * place, so distance alone cannot tell a lapped car sitting in front of
     * the leader from a car that is simply a long way back. The fold assumed
     * the first, and the second is common early in a race.
     *
     * Driver report 2026-08-01: "why is De La Rosa driving VERY slow after
     * safety car activation?" - he was on lap 1, the SAME lap as the leader,
     * 4037 m back on a 5742 m lap. The fold called that 1705 m ahead, the
     * ahead branch marked him g_fallback and capped him at 0.35 x pace = 11
     * m/s so the leader could "reel him in" - which can never happen, because
     * the leader was not behind him.
     *
     * The lap counter settles it: a car on the same lap or a later one is
     * BEHIND, whatever the arc says. Only a car that is genuinely a lap or
     * more down can be the out-of-place car the ahead branch is written for.
     *
     * The leader's own lap needs care: while the safety car leads the queue,
     * his lap number is deliberately held one above the race leader's (see
     * sc_lap_pump), so his TRUE lap is used here.                          */
    /* Forward physical gap only. Lap-aware order_recovery_snapshot owns
       illegal-pass recovery; being lapped alone never authorizes yielding. */
    return (float)d;
}

/* THE APPROACH LAW, on its own so both the pace controller and the restart can
 * use it. A car may never be commanded faster than the lowest recent speed of
 * whatever is physically ahead of it, plus an allowance proportional to the
 * room it has. At the target gap that is exactly the speed of the car ahead,
 * so a pass is impossible; further back it is a closing allowance that decays
 * continuously, so cars brake from 400 m out instead of arriving hot.
 *
 * A car being deliberately held back, or one on its wave-around lap, is not an
 * obstacle to respect - it is meant to be passed.
 *
 * This is what makes the F1 restart work with no extra machinery: release the
 * base cap and the approach law alone still forbids passing, so the queue
 * accelerates in order behind whatever the leader chooses to do.            */
static float approach_cap(int i, float want)
{
    int   ahead = -1, phys = -1;
    float gap   = nearest_obstacle(i, &ahead, 1);   /* who may set my pace  */
    float pgap  = nearest_obstacle(i, &phys,  0);   /* what is physically there */

    g_cap_obst[i] = ahead;
    g_cap_gap[i]  = (ahead >= 0) ? gap : -1.0f;

    /* THE SAFETY CAR IS NEVER PASSABLE. Not "usually not" - never, and it is
     * checked before the exemptions, because both of them can be true of him
     * by accident.
     *
     * He got passed on 2026-08-01 (driver: "Hakkinen tried to overtake the
     * safety car and got hit") through a stale flag. While he was catching the
     * field he was physically AHEAD of the race leader, so bunch_speed_for()
     * set g_fallback[sc] = 1, meaning "held back on purpose, meant to be
     * passed". The instant he became the pace setter, bunch_speed_for() began
     * returning early for the leader and never cleared it - so the car behind
     * him saw an obstacle it was allowed to ignore, and had no cap at all. */
    if (ahead >= 0 && gap >= 0.0f && ahead == g_sc_slot && sc_is_out()) {
        float base  = speed_floor(ahead);
        float allow = sc_catchup_limit(base,gap-SC_LEAD_MAX_M,APPROACH_GAIN);
        /* The leader may ease off to hold ten car lengths, but never crawl. */
        if (allow < base * 0.6f) allow = base * 0.6f;
        if (allow < 0.0f) allow = 0.0f;
        if (want > allow) { want = allow; g_cap_why[i] = CAPW_SC; }
        return want;
    }

    if (ahead >= 0 && gap >= 0.0f &&
        g_wave[ahead] != WAVE_RUNNING && !g_fallback[ahead]) {
        /* THE DISTANCE TERM MAY ONLY EVER ADD SPEED, NEVER SUBTRACT IT.
         *
         * Asking a car to go SLOWER than the one in front whenever it is
         * closer than the follow distance deadlocks a tightly-packed queue.
         * nearest_obstacle() deliberately folds a car up to ABREAST_WINDOW
         * (7 m) BEHIND into "abreast" and treats it as an obstacle at gap 0,
         * so with a follow distance of 8 m every car in a nose-to-tail train
         * sees a neighbour it must back away from - mutually - and the whole
         * field spirals down to the PACE_MIN floor.
         *
         * Measured live 2026-08-01: eleven cars pinned at exactly maxSpeed 5
         * while the safety car ran at 54. Driver: "most of the cars are
         * driving extremely slow right now."
         *
         * Capping at the speed of the car ahead is already sufficient to make
         * passing impossible; the extra allowance is only for closing a gap
         * that exists. So the term is clamped at zero from below, and the
         * cars may sit as close together as they like - which is the spec. */
        /* THE SPEED OF THE CAR AHEAD IS ITS CURRENT SPEED OR ITS RECENT
         * MINIMUM, WHICHEVER IS HIGHER - never a fixed floor.
         *
         * speed_floor() alone sags: it is a rolling minimum, so a car that
         * lifts for one corner drags the cap of everything behind it down, and
         * two cars running abreast each cap the other at a number that falls
         * every time either lifts. That ratchet was originally stopped by
         * clamping the allowance up to the PACE SPEED - which works, and which
         * is also exactly how a car gets past the one in front. Live
         * 2026-08-01: pace commanded 35 while the queue ran 30-33, so every
         * car in the field was handed 35 and closed on the car ahead. Driver:
         * "AI is trying to overtake each other."
         *
         * Taking the maximum with the CURRENT speed removes the sag without
         * introducing any number that is independent of the car ahead: the cap
         * can never come out above what that car is doing right now, so at the
         * follow distance a pass is arithmetically impossible.               */
        /* THE GAP IS MANDATORY, SO THE TERM MUST BE ALLOWED TO SUBTRACT.
         *
         * Clamping the distance term at zero from below let a car close right
         * up to the one in front and then simply match its speed. Nose to tail
         * at matched speed is precisely where any small differential turns into
         * a passing attempt, which is what the driver saw: "AI is trying to
         * overtake each other during safety car ... they are getting too close".
         *
         * So a car inside the mandatory gap is now commanded SLOWER than the
         * car ahead until it drops back to it. The gap is g_target_gap, the
         * same figure the queue is built on and tunable from the GUI.
         *
         * This term was clamped for a reason and the reason is gone. It used to
         * deadlock the field - eleven cars pinned at maxSpeed 5 - through two
         * faults that are both fixed above: an abreast car was an obstacle to
         * BOTH cars of a pair (now only to the one behind in the standings), and
         * the base was a rolling MINIMUM that sagged every time anyone lifted
         * (now the maximum of current and recent minimum). The floor below is
         * the third guard: no car is ever asked to run at less than 60% of the
         * car it is following, so a queue can compress but never collapse.   */
        float target = (float)g_target_gap;
        float room   = gap - target;
        float cur    = V_F32(ahead, OFF_SPEED);
        float flo    = speed_floor(ahead);
        float base   = (cur > flo) ? cur : flo;
        float allow  = sc_catchup_limit(base,room,APPROACH_GAIN);

        if (allow < base * 0.6f) allow = base * 0.6f;

        /* No pace floor here - see the block above. The only floor left is the
         * one that keeps the command a legal speed at all.                  */
        if (allow < (float)PACE_MIN) allow = (float)PACE_MIN;
        if (want > allow) { want = allow; g_cap_why[i] = CAPW_APPROACH; }
    }

    /* A CAR THAT MAY NOT SET MY PACE IS STILL SOLID.
     *
     * Refusing to take pace from a lapped car must not become permission to
     * drive through it. So the nearest PHYSICAL car - classification ignored -
     * still applies a cap, but only inside BLOCK_RANGE, and it tucks up to
     * BLOCK_GAP rather than the full queue spacing. Beyond BLOCK_RANGE it has
     * no effect at all, which is the whole point: P5 closes on a lapped P20 at
     * his own race pace instead of being dragged down from 400 m out, and only
     * matches its speed once he is actually on the back of it.
     *
     * This is also the honest F1 answer. Under a safety car you do not pass a
     * lapped car that has not been waved; you sit behind it. What you do not
     * do is inherit its pace half a straight away.
     *
     * Skipped for a car being waved or deliberately held back - both are meant
     * to be passed and both are already exempt above.                       */
    if (phys >= 0 && phys != ahead && pgap >= 0.0f && pgap < BLOCK_RANGE &&
        g_wave[phys] != WAVE_RUNNING && !g_fallback[phys]) {
        float cur   = V_F32(phys, OFF_SPEED);
        float flo   = speed_floor(phys);
        float base  = (cur > flo) ? cur : flo;
        float allow = base + APPROACH_GAIN * (pgap - BLOCK_GAP);

        if (allow < base * 0.6f)     allow = base * 0.6f;
        if (allow < (float)PACE_MIN) allow = (float)PACE_MIN;
        if (want > allow) { want = allow; g_cap_why[i] = CAPW_BLOCKED; }
    }
    return want;
}

/* Per-car cap under caution.
 *
 * ONE CAR IN THE FIELD IS GIVEN A SPEED. Everyone else is given a RULE.
 *
 * The slot controller this replaces gave every car its own absolute number,
 * k * target_gap behind the leader, clamped between BUNCH_FLOOR_PCT and
 * base + BUNCH_CATCHUP. Three complaints came out of driving it, and all three
 * are properties of the absolute number rather than bugs in it:
 *
 *   - the whole field ran at one speed, which is not what a yellow is. Under
 *     yellow the rule is "slow down and do not overtake", and the pace comes
 *     from the car in front, not from a global constant;
 *   - a car judged to be off its slot was pinned at a fraction of the pace, so
 *     backmarkers crawled ("some backmarkers slow to an absurdly slow speed");
 *   - a car a long way back was allowed pace + BUNCH_CATCHUP at most, so it
 *     closed a big gap slowly, and the CATCHUP_FREE_M escape hatch existed only
 *     to paper over that.
 *
 * So the cap is now:
 *
 *   queue leader           -> the pace speed. He is the only absolute number in
 *                             the system, and the whole field's speed descends
 *                             from him. With the safety car out he IS the
 *                             leader, and he is never governed at all, so under
 *                             a real safety car nothing in the field is given a
 *                             speed by this DLL.
 *   every other car        -> approach_cap() alone. Capped at the recent speed
 *                             of whatever is physically in front plus an
 *                             allowance proportional to the room it has.
 *
 * That single rule does all three jobs at once. It forbids passing (at the
 * follow distance the allowance is zero, so the cap is exactly the speed of the
 * car ahead). It bunches the field (the allowance decays continuously with the
 * gap, so a car arrives at the one ahead and settles nose to tail). And it lets
 * a backmarker close clear air at its own full race pace, because a car with
 * nothing inside APPROACH_RANGE has no obstacle and therefore no cap at all.
 *
 * The floor inside approach_cap() (never below the pace speed) is what stops a
 * queue ratcheting itself down, and it is also why no car can be slowed to an
 * absurd speed by this path any more.
 *
 * ORDER: the earlier slot controller existed because pure follow-the-car-ahead
 * bunched the field in track order, not race order - "lapped cars are in the
 * lead", 2026-07-31. That is still true, and it is still handled, but by the
 * two mechanisms built for it rather than by a per-car speed: the wave-around
 * releases a lapped car to unlap itself, and a car physically ahead of the
 * leader that cannot be waved is held at BUNCH_FALLBACK_PCT of the pace so the
 * leader reels it in.                                                        */
static int bunch_speed_unclamped(int i, int base)
{
    float want;
    int   v;

    if (g_pit_handover) {
        g_cap_why[i]=CAPW_NONE;g_cap_obst[i]=-1;g_cap_gap[i]=-1.0f;
        return base;
    }
    if (!g_bunch) return base;

    g_cap_why[i]  = CAPW_NONE;
    g_cap_obst[i] = -1;
    g_cap_gap[i]  = -1.0f;

    /* Waved-around: no cap at all. Full race speed, the car's own. */
    if (g_wave[i] == WAVE_RUNNING) { g_cap_why[i] = CAPW_WAVE; return SPEED_RELEASE; }

    /* The pace setter, and the fallback for anything unmeasurable. Without a
     * learned lap length there are no gaps, so there is no rule to apply and
     * the flat pace speed is the only safe cap - otherwise a caution with a
     * half-learned track would release the entire field.                    */
    if (i == g_leader_slot || g_leader_slot < 0 || g_lap_len <= 0.0f ||
        V_F32(i, OFF_LAPDIST) <= 0.0f) {
        g_fallback[i] = 0;
        g_cap_why[i] = (i == g_leader_slot) ? CAPW_LEADER : CAPW_UNMEASURED;
        /* The pace setter is the only car given a speed, so it is the only one
         * that has to be given the TRACK'S shape rather than a flat number.
         * Everyone behind inherits the variation through the approach law,
         * which caps them on the car ahead - so shaping one car shapes the
         * whole queue.                                                      */
        return pace_shaped(i, base);
    }

    /* A lapped car ahead of the race leader is still allowed to catch SC.
       Classification alone must never manufacture a slow moving roadblock. */
    g_fallback[i] = 0;

    /* The rule, and nothing else. SPEED_RELEASE going in means "no opinion of
     * my own"; whatever comes back is the car in front's speed plus the room. */
    want = approach_cap(i, (float)SPEED_RELEASE);
    if (want > (float)SPEED_RELEASE) want = (float)SPEED_RELEASE;

    /* THE FLOOR IS RELATIVE TO THE PACE, NOT AN ABSOLUTE 5.
     *
     * PACE_MIN is a legality clamp, not a speed anybody should be asked to
     * drive at. Using it as the floor is how one crawling car could pin the
     * cars behind it at maxSpeed 5 (live 2026-08-01). A car that genuinely
     * must drop back still has the 45% floor, which at a 30 m/s pace is
     * 13.5 m/s - slow enough to lose ground quickly, fast enough to look
     * like a racing car obeying a yellow rather than a broken one.         */
    v = (int)(want + 0.5f);
    {
        int lo = (int)((float)base * 0.45f + 0.5f);
        if (lo < PACE_MIN) lo = PACE_MIN;
        if (v < lo) { v = lo; g_cap_why[i] = CAPW_FLOOR; }
    }
    g_catchup[i] = (g_cap_why[i] == CAPW_NONE);
    if (v == SPEED_IDLE_SENTINEL) v = 19;   /* 18 means PitSpeedLimit */
    return v;
}

#include "sc_order_recovery.h"
_Static_assert(VEH_SLOTS <= SC_ORDER_SLOTS, "recovery snapshot capacity");
static SCOrderRecovery g_order_recovery;
static double g_order_last_sim;
static float g_order_ramped[SC_ORDER_SLOTS];
static void order_recovery_snapshot(void)
{
    queue_order_pump();
    sc_order_strict=g_order_locked;
    SCOrderCar cars[SC_ORDER_SLOTS] = {{0}};
    int enabled = g_engaged && phase_racing() && sc_slot_owned();
    int physical_sc = enabled && !g_pit_handover &&
        (g_sc_state == SC_CATCHUP || g_sc_state == SC_ONTRACK || g_sc_state == SC_RECALL) &&
        !pit_owned(g_sc_slot);
    int old = g_order_recovery.active, old_guard = g_order_recovery.guard;
    int leader = -1, best = 0x7fffffff;
    for (int i=0;i<VEH_SLOTS;i++) {
        if(!enabled || i==g_sc_slot || !queue_observed(i) ||
           pit_owned(i) || !slot_moving(i)) continue;
        cars[i].valid=1; cars[i].rank=g_pos0[i];
        cars[i].distance=V_F32(i,OFF_LAPDIST); cars[i].speed=V_F32(i,OFF_SPEED);
        cars[i].lap=V_I32(i,OFF_LAPNUM);cars[i].lap_known=cars[i].lap>=0&&cars[i].lap<100000;
        if(cars[i].rank>0 && cars[i].rank<best){best=cars[i].rank;leader=i;}
    }
    /* Before deployment and after pit entry, the saved P1 anchors the queue.
       Engagement lasts until GREEN, so pit entry alone cannot lift order. */
    float anchor=0, pace=0;
    if(physical_sc){anchor=V_F32(g_sc_slot,OFF_LAPDIST);pace=V_F32(g_sc_slot,OFF_SPEED);}
    else if(leader>=0 && g_lap_len>=100){
        anchor=fmodf(cars[leader].distance+40.0f,g_lap_len);
        /* Virtual P1 must be able to accelerate after a corner/slowdown.
           Its ordinary shaped pace cap remains in bunch_speed_unclamped. */
        pace=(float)g_cmd_now;
    } else enabled=0;
    sc_order_plan(&g_order_recovery,cars,VEH_SLOTS,enabled,
        anchor,pace,g_lap_len,physical_sc && g_sc_state==SC_CATCHUP?leader:-1);
    double dt=SESSION_TIME-g_order_last_sim;
    g_order_last_sim=SESSION_TIME;
    if(!isfinite(dt) || dt<0 || !old_guard)dt=0;
    if(dt>1.0)dt=1.0; /* accelerated simulation must still permit braking */
    if(g_order_recovery.guard)for(int i=0;i<VEH_SLOTS;i++) {
        if(!cars[i].valid || !g_armed[i])continue;
        float previous=fminf(cars[i].speed,old_guard?g_order_ramped[i]:g_last_written[i]);
        float floor=previous-(float)g_sc_config.CatchupBrakeMps2*(float)dt;
        if(g_order_recovery.cap[i]<floor)g_order_recovery.cap[i]=floor;
        g_order_ramped[i]=g_order_recovery.cap[i];
    }
    if(old != g_order_recovery.active)
        logf_("  [order-recovery] %s: yielding to SC; no saved-position giveback",
              g_order_recovery.active?"started":"cleared");
    if(g_order_recovery.guard)for(int i=0;i<VEH_SLOTS;i++) {
        if(!cars[i].valid || !g_armed[i])continue;
        g_fallback[i]=0;
        if(g_wave[i]==WAVE_RUNNING){g_wave[i]=WAVE_NONE;ai_suppress_passing(i);}
    }
}

static int bunch_speed_for(int i, int base)
{
    if(!isfinite(V_F32(i,OFF_LAPDIST)) || !isfinite(V_F32(i,OFF_SPEED)) ||
       V_F32(i,OFF_SPEED)<0.0f || V_F32(i,OFF_SPEED)>200.0f)return base;
    int want = bunch_speed_unclamped(i, base);
    /* Live positions must not make an overtaker block its rightful predecessor.
       The saved-order cap still accounts for the SC and the preceding rank. */
    if (g_order_locked && g_order_recovery.guard && g_order_recovery.reclaim[i] &&
        i != g_sc_slot && g_armed[i]) want=pace_shaped(i,base);
    if (g_order_recovery.guard && i != g_sc_slot && g_armed[i] &&
        g_order_recovery.cap[i] < (float)want) {
        /* Integer native command: round up by <1 m/s to avoid a permanent
           downward bias from measured speed just below the previous cap.
           The planner retains its 20/40 m spacing allowance. */
        want=(int)ceilf(g_order_recovery.cap[i]);
        if(want<3)want=3;
    }
    if (want == SPEED_IDLE_SENTINEL) want--; /* never exceed ceiling via pit sentinel */
    return want;
}


/* Metres the last car of the QUEUE sits behind the leader. Cars currently
 * waving are excluded - they are not part of the queue they are rejoining. */
static float queue_tail(void)
{
    int i;
    float tail = 0.0f;
    for (i = 0; i < VEH_SLOTS; i++) {
        float b;
        if (!g_armed[i] || !slot_live(i) || i == g_leader_slot) continue;
        if (!slot_moving(i) || g_wave[i] == WAVE_RUNNING) continue;
        b = behind_leader(i);
        if (b <= -1e8f || b < 0.0f) continue;
        if (b > tail) tail = b;
    }
    return tail;
}

/* Release any car that is a lap or more down and has not been waved yet.
 * Called every tick while the policy allows overtaking, so a car that loses a
 * lap mid-caution is picked up too. A car that has already rejoined stays
 * rejoined - it is not released a second time for still being a lap down. */
static void wave_release_due(void)
{
    int i, n = 0, lead_lap, race_lead;

    if (g_leader_slot < 0) return;
    if (g_sc_state == SC_OUTLAP || g_sc_state == SC_CATCHUP) return;

    /* THE RACE LEADER'S LAP, NEVER THE SAFETY CAR'S. g_leader_slot is the
     * QUEUE leader and that is the safety car while he is out - whose lap
     * number is deliberately held one above the race leader's so the standings
     * show him first. Reading it here would make every single car look a lap
     * down and wave the entire field around at once.                       */
    race_lead = race_leader_slot();
    if (race_lead < 0) return;
    lead_lap = V_I32(race_lead, OFF_LAPNUM);

    for (i = 0; i < VEH_SLOTS; i++) {
        if (!g_armed[i] || !slot_live(i) || i == g_leader_slot) continue;
        if (!slot_moving(i)) continue;
        if (g_wave[i] != WAVE_NONE) continue;
        if (V_I32(i, OFF_LAPNUM) >= lead_lap) continue;      /* on the lead lap */

        g_wave[i]        = WAVE_RUNNING;
        g_wave_passed[i] = 0;
        ai_restore_passing(i);       /* it is meant to be overtaking now */
        set_maxspeed((void*)VEH(i), SPEED_RELEASE);
        g_last_written[i] = (float)SPEED_RELEASE;
        n++;
        logf_("  [wave] slot %2d P%-2d lap %d (leader lap %d) %-24.24s RELEASED",
              i, V_I32(i, OFF_POSITION), V_I32(i, OFF_LAPNUM), lead_lap,
              V_STR(i, OFF_NAME));
    }
    if (n)
        logf_("=== WAVE-AROUND === %d lapped car(s) at full race speed, "
              "queue tail at %.0f m", n, queue_tail());
}

/* Policy switched off: take back control of anything still running. */
static void wave_recapture_all(void)
{
    int i, n = 0;
    for (i = 0; i < VEH_SLOTS; i++) {
        if (g_wave[i] != WAVE_RUNNING) continue;
        if (capture_owned(i)) ai_suppress_passing(i);
        g_wave[i] = WAVE_NONE;      /* NONE, not DONE - it may be waved again */
        n++;
    }
    if (n) logf_("  [wave] %d car(s) recaptured - overtaking no longer allowed", n);
}

static void wave_set_policy(int allowed)
{
    /* Wave-around has no complete lap/order reconciliation in this candidate.
       Disable it explicitly rather than exposing a conflicting GUI policy. */
    g_wave_policy = 0;
    if (!g_wave_policy) wave_recapture_all();
    logf_("=== POLICY === wave-around unavailable; lapped cars remain ordered (requested=%d)",allowed);
    if(g_ct!=CT_OFF || g_engaged)hud_show("NO OVERTAKING");
}

/* Advance the wave state machine: a car is done once it has been past the
 * leader and has come back round to within one gap of the queue's tail.    */
static void wave_pump(void)
{
    int i;
    float tail = queue_tail();

    for (i = 0; i < VEH_SLOTS; i++) {
        float b;
        if (g_wave[i] != WAVE_RUNNING) continue;
        if (!g_armed[i] || !slot_live(i)) { g_wave[i] = WAVE_NONE; continue; }

        b = behind_leader(i);
        if (b <= -1e8f) continue;

        /* Ahead of the leader = it has completed the overtake. */
        if (b < 0.0f) { g_wave_passed[i] = 1; continue; }

        /* Back round behind the queue, close enough to slot in. */
        if (g_wave_passed[i] && b <= tail + (float)g_target_gap) {
            g_wave[i] = WAVE_DONE;
            ai_suppress_passing(i);      /* back in the queue, back under orders */
            logf_("  [wave] slot %2d rejoined: %.0f m behind leader, queue tail "
                  "was %.0f m - back under pace control", i, b, tail);
            hud_notice("REJOIN THE SAFETY CAR QUEUE");
        }
    }
}

/* ---- safety car -------------------------------------------------------- */

/* Play one of the safety-car lines by borrowing the carrier event's score.
 * The 300 ms hold is what sc1.c used: long enough for the game's tick to build
 * the queue entry from the patched table before it is put back.            */
/* Park eid 132 in the game's own BLOCKED state so its emitter can never speak
 * SLOWCAR01. Re-armed every tick: this is one aligned float in .rdata and the
 * game is free to write it back, exactly like the yellow-flag display
 * predicate. Cheap enough to do unconditionally. */
/* THE BLOCK AND OUR OWN LINES SHARE ONE EVENT, SO THEY CANNOT BOTH BE ON.
 *
 * REGRESSION, driver-reported 2026-08-02b, first drive with the block in:
 * every SCAR line went silent. All five were inserted - the log shows all five
 * - and none was heard. The cause is that eid 132 is BOTH the event whose own
 * clip is SLOWCAR01 AND the carrier every safety-car line is spoken through,
 * and a FLT_MAX cooldown silences a clip no matter how the entry reached the
 * queue. That is settled prior art in this project from the other direction:
 * PWS_HIT01-06 and the 14 gearbox clips were silent for exactly this reason
 * and a 4-byte cooldown write was the whole fix. tools/commentary_inject.py
 * has said so all along - "Cooldown=FLT_MAX (cannot bypass)".
 *
 * The claim it was shipped on - that sc_say_now bypasses the gate by calling
 * InsertDriverTableEntry directly - was wrong about WHICH gate. The gate that
 * function calls first is ScoreThresholdGateCheck (0x418B90), and reading it
 * shows it tests PRIORITY against the commentary verbosity setting. It never
 * looks at the cooldown. The cooldown is consulted later, at playback.
 *
 * So the block is held only while no line of ours is in flight, and released
 * across the window in which one is. The window is closed by the event's own
 * timestamp going non-zero - the clip actually being consumed - not by a
 * timer, because a blind timer is precisely the mistake this whole line of
 * work already made once with Sleep(300).
 *
 * What the open window costs: if the game's own emitter fires eid 132 inside
 * it, the parked score means a safety-car line is spoken instead of SLOWCAR01.
 * A stray "safety car is in" is a far smaller fault than every safety-car line
 * in the mod being silent, and the window is a few seconds per line.       */
static int   g_say_inflight = 0;
static DWORD g_say_fired_ms = 0;
#define CARRIER_INFLIGHT_MS 8000u

static void slowcar_block(void)
{
    volatile float *cd = EV_COOLDOWN(CARRIER_EID);
    DWORD old = 0;

    if (g_say_inflight) return;      /* one of our own lines is being spoken */

    if (!VirtualProtect((LPVOID)cd, 4, PAGE_READWRITE, &old)) return;

    if (!g_slowcar_saved) {
        g_slowcar_cooldown = *cd;
        g_slowcar_score    = *EV_SCORE(CARRIER_EID);
        g_slowcar_saved    = 1;
        logf_("  [audio] SLOWCAR01 blocked at source: eid %d cooldown %.1f -> "
              "FLT_MAX, score was %d. Score 130 has no other PWS event, so the "
              "line can no longer be spoken by anything.",
              CARRIER_EID, g_slowcar_cooldown, g_slowcar_score);
    }
    if (*cd < EV_BLOCKED) *cd = EV_BLOCKED;
    VirtualProtect((LPVOID)cd, 4, old, &old);
}

/* Put the game back exactly as it was found. Called on eject. */
static void slowcar_unblock(void)
{
    DWORD old = 0;
    volatile float *cd = EV_COOLDOWN(CARRIER_EID);
    volatile int   *sa = EV_SCORE(CARRIER_EID);

    if (!g_slowcar_saved) return;

    if (VirtualProtect((LPVOID)cd, 4, PAGE_READWRITE, &old)) {
        *cd = g_slowcar_cooldown;
        VirtualProtect((LPVOID)cd, 4, old, &old);
    }
    if (VirtualProtect((LPVOID)sa, 4, PAGE_READWRITE, &old)) {
        *sa = g_slowcar_score;
        VirtualProtect((LPVOID)sa, 4, old, &old);
    }
    g_slowcar_saved = 0;
    logf_("  [audio] SLOWCAR01 restored (cooldown %.1f, score %d) - the game is "
          "as it was found", g_slowcar_cooldown, g_slowcar_score);
}

static void sc_say_now(int score, const char *what)
{
    volatile int *sa = (volatile int*)(EVENT_TABLE + CARRIER_EID * 0x14 + 0x10);
    DWORD old = 0;
    float buf[8];

    /* NEVER FIRE THE CARRIER INTO A BUSY CHANNEL - THAT IS HOW SLOWCAR01 GETS
     * PLAYED.
     *
     * The carrier's own score is 130, PWS_SLOWCAR01 "watch out for the slow
     * car". sc_say borrows eid 132, holds the rewritten score for 300 ms and
     * then puts 130 back. If the audio system is busy when the entry is
     * queued, the clip is not loaded inside that window - it is loaded after
     * the score has already been restored, and what comes out is SLOWCAR01.
     * Driver report 2026-08-02: SLOWCAR01 heard during safety car handling.
     *
     * So wait for the channel instead of firing regardless. This is the
     * pattern already proven in dll/retire_commentary.c: poll isPlaying at
     * CONTROLLER+0xAE4, retry, and only inject into a gap. There it took 12
     * retries (~4 s) in one run before a gap appeared.
     *
     * THE LINE IS DROPPED, NOT FORCED, WHEN THE WAIT RUNS OUT.
     *
     * Driver instruction 2026-08-02: "SLOWCAR01 is not to be played at all."
     * Firing anyway would be exactly the case that produces it, so a busy
     * channel now costs a missing safety-car line rather than a wrong one.
     * That is the whole guarantee: the carrier is never fired unless the
     * channel is free, so the restored score can never be what gets loaded. */
    if (!VirtualProtect((LPVOID)sa, 4, PAGE_READWRITE, &old)) {
        logf_("  [audio] VirtualProtect failed, %s not played", what);
        return;
    }
    /* THE SCORE IS SET AND LEFT SET. It is never put back to 130.
     *
     * The old code restored it after Sleep(300), which is a race it cannot
     * win: the queue entry is resolved when the line is DEQUEUED, not when it
     * is inserted, so a restore on a timer can land first and the clip loads
     * SLOWCAR01. Leaving a safety-car score parked here is harmless because
     * slowcar_block() has taken the event out of the game's reach.         */
    *sa = score;
    VirtualProtect((LPVOID)sa, 4, old, &old);

    /* Lift the SLOWCAR01 block for the length of this line. Without this the
     * entry is queued and then silently discarded at playback - see the note
     * on slowcar_block(). The cooldown goes back to the game's own value, and
     * the block is re-armed by the tick loop once the clip has been consumed. */
    {
        volatile float *cd = EV_COOLDOWN(CARRIER_EID);
        DWORD cdold = 0;
        if (VirtualProtect((LPVOID)cd, 4, PAGE_READWRITE, &cdold)) {
            *cd = g_slowcar_saved ? g_slowcar_cooldown : 1.0f;
            VirtualProtect((LPVOID)cd, 4, cdold, &cdold);
        }
    }
    /* Zero the event's timestamp FIRST: it is the signal the clip was played,
     * and it has to be cleared before the entry is inserted or the previous
     * line's timestamp is mistaken for this one's. */
    *EV_TS(CARRIER_EID) = 0.0f;
    g_say_inflight = 1;
    g_say_fired_ms = GetTickCount();

    FN_STRUCT_INIT(buf);
    FN_DRIVER_LOOKUP(buf, CARRIER_EID, -1, -1, -1, -1, -2.0f, -2.0f);
    FN_INSERT_ENTRY((void*)CTRL_OBJ, buf);

    g_said_at = GetTickCount();
    logf_("  [audio] %s (carrier eid %d, score %d)", what, CARRIER_EID, score);
}

/* THE LINE QUEUE IS A FIFO, NOT ONE SLOT.
 *
 * It used to be a single slot, on the reasoning that a newer line always
 * supersedes an older one. That is wrong for a SEQUENCE, and the recall is a
 * sequence: SCAR05 as he turns into the pit lane, then SCAR04 as the field is
 * released. With one slot, and now with a busy channel re-queueing as well,
 * one of the two is simply lost and the driver hears the wrong half of the
 * story. Driver instruction 2026-08-02: the SCAR clips must play in the
 * correct sequence.
 *
 * Order is strict: entries are fired front to back, and each is held until
 * the audio channel is free, so a delayed line cannot overtake the one in
 * front of it. SAY_SPACING keeps two clips from running together, since the
 * carrier trick gives no way to know how long a clip lasts.                */
#define SAY_Q        6
#define SAY_SPACING  1500u    /* ms between two spoken lines */

typedef struct { int score,eid; const char *what; DWORD at; int tries; } SayItem;

static SayItem g_sayq[SAY_Q];
static int     g_sayq_n  = 0;
static int g_native_say_inflight_eid=0;
static DWORD g_native_say_fired_ms=0;
static void say_eid(int eid,const char *what);

static void sc_say_later(int score, const char *what, DWORD delay_ms)
{
    DWORD at = GetTickCount() + delay_ms;

    if (g_sayq_n >= SAY_Q) {
        logf_("  [audio] queue full, dropping %s", what);
        return;
    }
    /* Never schedule ahead of something already waiting - that is what makes
     * it a sequence rather than a race. */
    if (g_sayq_n > 0) {
        DWORD prev = g_sayq[g_sayq_n - 1].at + SAY_SPACING;
        if ((int)(prev - at) > 0) at = prev;
    }
    g_sayq[g_sayq_n].score = score;
    g_sayq[g_sayq_n].eid = 0;
    g_sayq[g_sayq_n].what  = what;
    g_sayq[g_sayq_n].at    = at;
    g_sayq[g_sayq_n].tries = 0;
    g_sayq_n++;
}

/* Native PWS clips use their own event IDs. Hold them behind the same
 * commentary queue so investigation precedes the eventual sanction line. */
static void sc_say_eid_later(int eid,const char *what,DWORD delay_ms)
{
    DWORD at=GetTickCount()+delay_ms;
    if(g_sayq_n>=SAY_Q){logf_("  [audio] queue full, dropping %s",what);return;}
    if(g_sayq_n>0){
        DWORD prev=g_sayq[g_sayq_n-1].at+SAY_SPACING;
        if((int)(prev-at)>0)at=prev;
    }
    g_sayq[g_sayq_n].score=0;
    g_sayq[g_sayq_n].eid=eid;
    g_sayq[g_sayq_n].what=what;
    g_sayq[g_sayq_n].at=at;
    g_sayq[g_sayq_n].tries=0;
    g_sayq_n++;
}

/* Close the in-flight window, so slowcar_block() can re-arm. The clip has been
 * consumed when the game writes the event's timestamp; the timeout only exists
 * so a line that is never played cannot leave SLOWCAR01 reachable forever. */
static void carrier_pump(void)
{
    if(g_native_say_inflight_eid){
        if(*EV_TS(g_native_say_inflight_eid)>0.0f){
            logf_("  [audio] native eid %d consumed",g_native_say_inflight_eid);
            g_native_say_inflight_eid=0;
        }else if(GetTickCount()-g_native_say_fired_ms>CARRIER_INFLIGHT_MS){
            logf_("  [audio] native eid %d delivery timeout",g_native_say_inflight_eid);
            g_native_say_inflight_eid=0;
        }
    }
    if (!g_say_inflight) return;

    if (*EV_TS(CARRIER_EID) > 0.0f) {
        g_say_inflight = 0;
        return;
    }
    if (GetTickCount() - g_say_fired_ms > CARRIER_INFLIGHT_MS) {
        logf_("  [audio] carrier eid %d never played within %u ms - re-blocking",
              CARRIER_EID, CARRIER_INFLIGHT_MS);
        g_say_inflight = 0;
    }
}

static void say_pump(void)
{
    DWORD now = GetTickCount();
    int   i;

    if (g_sayq_n <= 0) return;
    if (g_say_inflight||g_native_say_inflight_eid) return;
    if ((int)(g_sayq[0].at - now) > 0) return;              /* not due yet */
    if (g_said_at && now - g_said_at < SAY_SPACING) return; /* let the last finish */

    /* Busy channel: hold the line, do not fire it. Firing into a busy channel
     * is what produces SLOWCAR01 - see sc_say(). */
    if (*(volatile int*)(CTRL_OBJ + 0xAE4) != 0) {
        if (++g_sayq[0].tries <= SAY_MAX_TRIES) {
            g_sayq[0].at = now + 250;
            return;
        }
        /* Fire anyway. Waiting for a gap is only about not talking over the
         * previous line now - it is no longer a correctness guard, because
         * the score is never restored to 130 and the event is blocked at
         * source, so a late resolve cannot produce SLOWCAR01 any more. */
        logf_("  [audio] channel busy %d tries, firing %s anyway",
              SAY_MAX_TRIES, g_sayq[0].what);
    }
    if(g_sayq[0].eid){
        int eid=g_sayq[0].eid;
        *EV_TS(eid)=0.0f;
        say_eid(eid,g_sayq[0].what);
        g_native_say_inflight_eid=eid;
        g_native_say_fired_ms=GetTickCount();
    }else sc_say_now(g_sayq[0].score, g_sayq[0].what);

    for (i = 1; i < g_sayq_n; i++) g_sayq[i - 1] = g_sayq[i];
    g_sayq_n--;
}

/* Drop every pending line. Used when the caution ends or the session is torn
 * down, so a stale SCAR line cannot arrive under a green flag. */
static void say_flush(const char *why)
{
    if (g_sayq_n > 0)
        logf_("  [audio] %d pending line(s) dropped (%s)", g_sayq_n, why);
    g_sayq_n = 0;
}

/* Fire an event that ALREADY has its own PWS entry - no carrier, no score
 * rewrite, no VirtualProtect. The whole sc_say apparatus exists only because
 * the four safety-car clips have no event of their own; the yellow and green
 * lines do, so they are injected straight.
 *
 *   eid 126 PWS_GREEN01   score 124  cooldown 5.0   not blocked
 *   eid 134 PWS_YELLOW01  eid 136 YELLOW02  eid 138 YELLOW03  (flag raised)
 *   eid 133 PWS_YELLOW04  eid 135 YELLOW05  eid 137 YELLOW06  (flag cleared)
 *
 * Only GREEN01 is used here. The yellow lines are deliberately NOT injected:
 * the game raises its own zone yellows through sub_4D1730 -> sub_41B3C0 and
 * announces them itself, so injecting would double them up. Arm the trigger,
 * never forge the result.                                                    */
static void say_eid(int eid, const char *what)
{
    float buf[8];
    FN_STRUCT_INIT(buf);
    FN_DRIVER_LOOKUP(buf, eid, -1, -1, -1, -1, -2.0f, -2.0f);
    FN_INSERT_ENTRY((void*)CTRL_OBJ, buf);
    logf_("  [audio-request] submitted %s (native eid %d); audible delivery not acknowledged", what, eid);
}

/* ---- the yellow flag --------------------------------------------------- */

/* Swap the display predicate's first three bytes with one aligned 32-bit store.
 * Refuses - permanently - if the bytes are not what this build expects, so a
 * patched or different exe is never written to blind.                        */
static void yellow_set(int on)
{
    static DWORD prot = 0;
    volatile DWORD *p = (volatile DWORD*)ADDR_YELLOW_PRED;

    if (g_yellow_ok == 0) return;
    if (on == g_yellow)   return;

    if (g_yellow_ok < 0) {
        if (*p != YELLOW_BYTES_ORIG) {
            g_yellow_ok = 0;
            logf_("  [flag] REFUSING to force the yellow: 0x%08X reads %08X, "
                  "expected %08X (different or already-patched exe)",
                  ADDR_YELLOW_PRED, *p, YELLOW_BYTES_ORIG);
            return;
        }
        if (!VirtualProtect((LPVOID)ADDR_YELLOW_PRED, 4,
                            PAGE_EXECUTE_READWRITE, &prot)) {
            g_yellow_ok = 0;
            logf_("  [flag] VirtualProtect failed - no forced yellow");
            return;
        }
        g_yellow_ok = 1;
    }

    *p = on ? YELLOW_BYTES_FORCE : YELLOW_BYTES_ORIG;
    g_yellow = on;
    logf_("=== YELLOW FLAG %s === (display predicate 0x%08X %s)",
          on ? "SHOWN" : "CLEARED", ADDR_YELLOW_PRED,
          on ? "forced true" : "restored");
}

/* How many marshalling zones the GAME currently has yellow, and which one is
 * the first. Read-only: this is the game's own incident detector.           */
static int zones_yellow(int *first_zone)
{
    unsigned int base  = *(volatile unsigned int*)(STEWARD + ST_ZONE_ARRAY);
    int          count = *(volatile int*)(STEWARD + ST_ZONE_COUNT);
    int i, n = 0;

    if (first_zone) *first_zone = -1;
    if (!base || count <= 0 || count > 512) return 0;
    if (IsBadReadPtr((void*)base, (UINT_PTR)count * ZONE_STRIDE)) return 0;

    for (i = 0; i < count; i++) {
        int st = *(volatile int*)(base + (unsigned)(i * ZONE_STRIDE));
        if (st == ZONE_YELLOW) {
            if (n == 0 && first_zone) *first_zone = i;
            n++;
        }
    }
    return n;
}

#define SC_INVERTED_CONFIRM g_sc_config.InvertedConfirmSeconds
#define SC_INVERTED_SPEED g_sc_config.InvertedMaxSpeed
#define SC_AUTO_QUIET_SECONDS (g_sc_config.RetirementCooldownSeconds)
#include "sc_auto_incident_guardrails.h"
#include "f1c_ai_restart_pending.h"
#include "f1c_start_stall.h"
#include "f1c_engine_motion.h"
#include "sc_incident_clearance.h"
#include "sc_recall_gates.h"
#include "sc_weather_guardrails.h"
#include "f1c_weather.h"
#include "f1c_session_guard.h"
static SCWeatherState g_weather;
static int g_weather_cycle;
static SCClearState g_clearance;
#include "f1c_surface_contact.h"
#include "sc_read_scope.h"
static int sc_marker_read(uintptr_t address,void *out,size_t bytes);
static F1CVehicleLifecycle sc_park_lifecycle(int i) {
    if(i==g_sc_slot && sc_slot_owned() && g_hidden)
        return f1c_vehicle_dormant_garage_read(sc_marker_read,(uintptr_t)VEH(i));
    return f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
}


/* Log refusals at most once per second. Invalid/early-return lifecycle fields
 * are labelled by valid/garage_valid, never treated as placement evidence. */
static void sc_garage_diagnostic(int slot,const char *where) {
    static DWORD last; static int logged;
    DWORD now=GetTickCount();
    if(logged && (DWORD)(now-last)<1000u)return;
    logged=1;last=now;
    F1CVehicleLifecycle life=sc_park_lifecycle(slot);
    logf_("[garage-refusal] where=%s slot=%d valid=%d role=%d finish=%d retired=%d ai=%d garage_valid=%d clear=%d team=%d bay=%d distance=%.3f body=(%.3f,%.3f,%.3f) garage=(%.3f,%.3f,%.3f)",
        where,slot,life.valid,life.role,life.finish,life.retired,life.ai_control,
        life.garage_valid,life.garage_clear,life.team,life.bay,life.garage_distance,
        life.body[0],life.body[1],life.body[2],life.garage[0],life.garage[1],life.garage[2]);
}

static F1CSessionGuard sc_session_guard(void);
static int field_stable(void);
static int sc_action_authorized(void);
static int sc_offline_authorized(void);
static SCAutoState g_auto_state;
#include "sc_retirement_cooldown.h"
#include "sc_stint.h"
#include "f1c_sc_pit_authority.h"
#include "f1c_ai_engine_stop.h"
#include "sc_engine_stop_watch.h"
static SCStint g_sc_stint;
static int g_sc_forced_return;
static int g_sc_continuation;
#include "sc_recall_readiness.h"
static SCRetirementCooldown g_retirement;
static unsigned char g_retirement_suppressed[VEH_SLOTS];
static SCRecallReady g_recall_ready;
static SCEvidenceClock g_evidence_clock;

static int g_auto_reason;
static int g_rf_sequence_active;
static int g_silent_release, g_rf_handoff, g_rf_stand_down, g_weather_departure_fresh;
/* Existing red flag 27-word monitor ABI, read-only and veto-only. No value
   from this legacy unversioned bridge can authorize a Safety Car. */
static int sc_redflag_veto(void)
{
    g_rf_sequence_active=0;
    if(!g_rc)return 1;
    if(f1c_rc_read(&g_rc->rf_registered)) {
        g_rf_sequence_active=f1c_rc_read(&g_rc->owner)==F1C_RC_RF;
        return g_rf_sequence_active || f1c_rc_read(&g_rc->rf_request)!=0;
    }
    /* An older RF has no safe handoff protocol. Its presence blocks SC;
       absence remains a fully independent SC installation. No cached handle. */
    HANDLE legacy=OpenFileMappingA(FILE_MAP_READ,FALSE,"F1C_REDFLAG_MON");
    if(legacy){CloseHandle(legacy);return 1;}
    return 0;
}

/* Read once on the stable game callback, before hotkey recall decisions. */
static void weather_pump(void)
{
    if(g_emergency_inhibit)return;
    int racing=phase_racing(),before=g_weather.hold;
    F1CWeather w={0,0,0};
    if(racing)w=f1c_weather_read(sc_marker_read);
    g_weather_departure_fresh=w.valid;
    sc_weather_step(&g_weather,g_epoch,g_evidence_now,racing,g_ct_auto,
                    sc_redflag_veto(),g_engaged,w.valid,w.on_path);
    if(!g_engaged)g_weather_cycle=0;
    if(g_engaged && g_weather.hold && g_ct<CT_IN)g_weather_cycle=1;
    if(before!=g_weather.hold)
        logf_("[weather-SC] %s wet=%.3f valid=%d sim=%.3f",
              g_weather.hold?"HOLD: extreme wet track":"RECOVERY: recall permitted",
              w.on_path,w.valid,SESSION_TIME);
}

/* Build the clearance adapter from reviewed fields already used by this
 * candidate.  The suppressed yellow table is intentionally absent: it can
 * declare a caution, but it cannot prove this caution has cleared. */
static int g_retirement_danger;
/* Run before every restart/clearance consumer, using this fresh physics sample. */
static void sc_retirement_pump(void) {
    sc_retirement_clock(&g_retirement,g_epoch,g_evidence_now,g_engaged,g_sc_config.RetirementCooldownSeconds);
    memset(g_retirement_suppressed,0,sizeof(g_retirement_suppressed));g_retirement_danger=0;
    if(!field_stable())return;
    sc_read_scope_begin();
    for(int i=0;i<VEH_SLOTS;i++) {
        if(i==0||i==g_sc_slot||!slot_live(i)||pit_owned(i))continue;
        F1CVehicleLifecycle life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
        if(!life.valid||life.garage_clear||life.finish==1)continue;
        float speed=V_F32(i,OFF_SPEED);if(!isfinite(speed))continue;
        unsigned hash=2166136261u;
        for(int k=0;k<31&&V_STR(i,OFF_NAME)[k];k++)hash=(hash^(unsigned char)V_STR(i,OFF_NAME)[k])*16777619u;
        unsigned identity=(hash^(unsigned)V_I32(i,OFF_ROSTER))*16777619u;
        int terminal=f1c_lifecycle_terminal(life.retired,life.finish);
        int failed=terminal||(!life.ai_control&&speed<2.0f&&i!=0);
        F1CIncidentGeometry geometry=f1c_geometry_read(i,(uintptr_t)VEH(i));
        F1COrientation orientation=f1c_orientation_vehicle((void*)VEH(i));
        int inverted=orientation.valid&&orientation.up_dot<=g_sc_config.InvertedUpDotMax;
        int dangerous=geometry.known&&(geometry.blocked||(inverted&&geometry.near_road));
        int tracked=g_clearance.entry[i].tracked&&g_clearance.entry[i].identity==identity;
        g_retirement_suppressed[i]=(unsigned char)sc_retirement_observe(&g_retirement,i,identity,1,g_engaged,failed,speed,geometry.known,dangerous,tracked);
        if(dangerous)g_retirement_danger=1;
    }
    sc_read_scope_end();
}

static void clearance_cars(SCClearCar *cars, int *qualifies, int capture_initial)
{
    int i;
    memset(cars,0,sizeof(SCClearCar)*VEH_SLOTS);
    memset(qualifies,0,sizeof(int)*VEH_SLOTS);
    for(i=0;i<VEH_SLOTS;i++) {
        if(capture_initial && g_auto_state.car[i].slow) {
            qualifies[i]=1;cars[i].identity=g_auto_state.car[i].identity;
        }
    }
    if(!field_stable())return;
    sc_read_scope_begin();
    for(i=0;i<VEH_SLOTS;i++) {
        unsigned hash=2166136261u;
        if(i==g_sc_slot){cars[i].observed=1;cars[i].sc=1;continue;}
        F1CVehicleLifecycle life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
        if(!life.valid || IsBadReadPtr((void*)(VEH(i)+OFF_NAME),32))continue;
        cars[i].observed=1;cars[i].is_player=(i==0 || life.role==0);
        cars[i].active=life.role!=-1;cars[i].pit=pit_owned(i);
        cars[i].terminal=f1c_lifecycle_terminal(life.retired,life.finish);

        cars[i].disabled=life.role==1 && !life.ai_control;
        cars[i].ignore_new=g_retirement_suppressed[i];
        cars[i].garage=life.garage_clear;
        cars[i].speed=V_F32(i,OFF_SPEED);cars[i].damage=V_I32(i,OFF_CARCOND)!=0;
        cars[i].distance=V_F32(i,OFF_LAPDIST);cars[i].lap_length=g_lap_len;
        for(int k=0;k<31 && V_STR(i,OFF_NAME)[k];k++)hash=(hash^(unsigned char)V_STR(i,OFF_NAME)[k])*16777619u;
        cars[i].identity=(hash^(unsigned)V_I32(i,OFF_ROSTER))*16777619u;
        if(g_auto_state.car[i].identity!=cars[i].identity)qualifies[i]=0;
        {
            F1CSurfaceContacts surface=f1c_surface_contacts(VEH(i),sc_marker_read);
            cars[i].road_known=surface.known>0;
            cars[i].road_contacts=surface.drivable;
            /* This is controller freedom, never a claim about a clear racing
               corridor.  Unknown native/surface evidence stays non-hazardous
               and therefore holds an existing ledger rather than inventing
               geometry. */
            cars[i].free_to_progress=g_armed[i]&&capture_identity_owned(i)&&!cars[i].pit&&
                isfinite(g_last_written[i])&&g_last_written[i]>=8.0f&&
                isfinite(V_F32(i,OFF_MAXSPEED))&&V_F32(i,OFF_MAXSPEED)>=8.0f;
        }
        /* Manual/weather deployment shares the existing observational record:
           a car already seen moving, now stopped with known road contact,
           enters this cycle's ledger even when request priority was weather. */
        if(capture_initial && !cars[i].pit && !cars[i].garage &&
           g_auto_state.car[i].identity==cars[i].identity && g_auto_state.car[i].seen && isfinite(cars[i].speed) && cars[i].speed<1.5f &&
           cars[i].road_known && cars[i].road_contacts>0) qualifies[i]=1;
    }
    sc_read_scope_end();
}

static int clearance_step(int capture_initial)
{
    SCClearCar cars[VEH_SLOTS]; int qualifies[VEH_SLOTS];
    /* Inactive sc_clear_step only resets these verdict fields. Do that before
       collecting a whole unused field snapshot. Initial capture is mandatory. */
    if(!capture_initial && !(g_engaged && g_ct>=CT_DEPLOY && g_ct<=CT_RESTART)) {
        g_clearance.clear_since=0;g_clearance.last_verdict=0;return 0;
    }
    clearance_cars(cars,qualifies,capture_initial);
    if(capture_initial) sc_clear_capture(&g_clearance,cars,VEH_SLOTS,qualifies,g_epoch,g_evidence_now);
    return sc_clear_step(&g_clearance,cars,VEH_SLOTS,g_epoch,g_evidence_now,
        /* The ledger is authoritative until the single GREEN release.  A
           recall is a request to return the SC, not proof that the circuit
           became safe. */
        g_engaged && g_ct>=CT_DEPLOY && g_ct<=CT_RESTART);
}
static int clearance_recall_ok(void)
{
    int clear=clearance_step(0); /* observe before the manual no-hazard shortcut */
    if(!sc_clear_has_hazard(&g_clearance))return 1; /* manual/weather-only caution */
    return clear;
}

/* ---- the watcher's verdict, and the trace of how it got there -----------
 *
 * One pass over the field per tick, filling a TrigInfo. It is deliberately
 * side-effect free apart from the persistence timer: caution_pump() decides
 * what to DO with the verdict, ctl_pump() publishes it, and the GUI/CLI draw
 * it. That separation is the point - the previous watcher declared a caution
 * out of a single number nobody could see, so a driver asking "why did it not
 * fire" had nothing to look at but the log after the fact.
 *
 * Returns 1 when every gate is open, i.e. a caution should be declared now.  */
static int trigger_eval(TrigInfo *t)
{
    static float med[VEH_SLOTS];
    int   i, n = 0, running = 0;
    int   racing = phase_racing();
    float field;
    DWORD now = sc_sim_millis(SESSION_TIME);

    sc_read_scope_begin();
    memset(t, 0, sizeof(*t));
    t->worst_slot  = -1;
    t->worst_speed = -1.0f;
    t->hold_req    = g_ct_hold_secs * 1000;
    t->total_laps  = TOTAL_RACE_LAPS;
    { int current_leader=race_leader_slot();
      t->leader_lap=(current_leader>=0&&slot_live(current_leader))?V_I32(current_leader,OFF_LAPNUM):-1; }
    t->zones       = zones_yellow(NULL);

    if (racing)                     t->gates |= GATE_RACING;
    if (g_ct_auto)                  t->gates |= GATE_AUTO_ON;
    if (g_ct == CT_OFF && g_sc_state == SC_IDLE) t->gates |= GATE_IDLE;

    /* The final-lap gate. NT2004 compares the LEADER's lap against the race
     * length; a caution thrown on the last lap can never be run off, so the
     * scanner is simply not called. Ours needs a leader and a lap count to be
     * able to say no - with neither, it does not block.                     */
    if (!g_ct_final_gate || t->total_laps <= 0 || t->leader_lap < 0 ||
        t->leader_lap < t->total_laps - 1)
        t->gates |= GATE_NOT_FINAL;

    /* The post-restart grace. Cars are still sorting themselves out in the
     * first seconds of a green, and one of them being briefly slow is not a
     * new incident - it is the old one clearing.                            */
    if (g_restart_green_ms &&
        now - g_restart_green_ms < (DWORD)(g_ct_grace_secs * 1000))
        t->grace_left = (int)(g_ct_grace_secs * 1000 -
                              (now - g_restart_green_ms));
    else
        t->gates |= GATE_NO_GRACE;

    /* One pass: collect speeds, count the stopped, remember the worst. */
    for (i = 0; i < VEH_SLOTS; i++) {
        float sp;

        if (!slot_participant(i)) continue;
        if (i == g_sc_slot) continue;        /* he is never his own incident */
        if (pit_owned(i))   continue;        /* the pit lane is not an incident */

        sp = V_F32(i, OFF_SPEED);
        if (!isfinite(sp) || sp < 0.0f || sp > 200.0f) continue;      /* unmeasurable */

        med[n++] = sp;
        if (sp >= TRIG_STOP_SPEED) running++;

        if (t->worst_speed < 0.0f || sp < t->worst_speed) {
            t->worst_speed = sp;
            t->worst_slot  = i;
        }
    }

    if (t->worst_slot >= 0 &&
        !IsBadReadPtr((void*)(VEH(t->worst_slot) + OFF_NAME), 20)) {
        lstrcpynA(t->worst_name, V_STR(t->worst_slot, OFF_NAME),
                  sizeof(t->worst_name));
    }

    /* Median by selection - n is at most 104 and this runs at 20 Hz, so the
     * O(n^2/2) does not matter and a sort in place would disturb nothing. */
    field = 0.0f;
    if (n > 0) {
        int j, k;
        for (j = 1; j < n; j++) {
            float value = med[j];
            for (k = j; k > 0 && med[k - 1] > value; k--)
                med[k] = med[k - 1];
            med[k] = value;
        }
        field = med[n / 2];  /* upper median, including tied samples */
    }
    t->field_speed = field;

    /* THE MASK. NT2004 suppresses the trigger when every car carries a flag
     * byte; the case it protects against is the whole field being legitimately
     * stationary or crawling - a grid, a formation lap, a session that has not
     * started. Ours reads the same situation directly: if the field's own
     * median is at walking pace, nobody is off the pace, because there is no
     * pace.                                                                 */
    if (running >= 1 && field > 8.0f) t->gates |= GATE_NOT_MASKED;

    for (i = 0; i < VEH_SLOTS; i++) {
        float sp;

        if (!slot_live(i) || i == g_sc_slot || pit_owned(i)) continue;
        sp = V_F32(i, OFF_SPEED);
        if (!isfinite(sp) || sp < 0.0f || sp > 200.0f) continue;

        if (sp < TRIG_STOP_SPEED) {
            t->stopped++;
        } else if ((t->gates & GATE_NOT_MASKED) && n >= TRIG_MIN_FIELD &&
                   sp < field * TRIG_OFFPACE_FRAC) {
            t->offpace++;
        }
    }
    t->involved = t->stopped + t->offpace;

    /* An incident is the game's own yellow OR a car we can see is in trouble.
     * The zone table is the better signal (sub_4D1730 computes it per frame
     * from a real per-segment speed test) but it is per-ZONE, so it says
     * nothing about how many cars are involved - which is what the severity
     * needs. Both are kept, and either one alone is enough to start the
     * clock.                                                                */
    if ((t->zones >= CT_AUTO_MIN_ZONES || t->involved >= 1) &&
        (t->gates & GATE_NOT_MASKED))
        t->gates |= GATE_INCIDENT;

    /* Severity, NT2004's sub_4F95D0 in F1 clothing. A single car parked at the
     * side is a short caution; several cars, or a blocked track, is a long
     * one.                                                                  */
    if (t->gates & GATE_INCIDENT)
        t->severity = (t->involved >= g_ct_full_thresh) ? 2 : 1;

    /* Corroborated fresh local incidents replace the legacy yellow-OR-slow
       trigger. All confirmation/quiet-window time is simulation time. */
    {
        SCAutoCar cars[VEH_SLOTS]={{0}};
        int idle=racing && sc_slot_owned() && !g_engaged && g_ct==CT_OFF && g_sc_state==SC_IDLE;
        for(i=0;i<VEH_SLOTS;i++) {
            if(i==g_sc_slot||!slot_live(i))continue;
            F1CVehicleLifecycle life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
            if(!life.valid || life.garage_clear || life.finish==1)continue;
            cars[i].valid=1;cars[i].pit=pit_owned(i);cars[i].ai_control=life.ai_control;cars[i].is_player=(i==0 || life.role==0);
            cars[i].controlled=g_engaged && g_armed[i];
            cars[i].terminal=f1c_lifecycle_terminal(life.retired,life.finish);
            if(life.ai_control && !cars[i].terminal) {
                F1CAIRestartPending pending=f1c_ai_restart_pending_read(sc_marker_read,(uintptr_t)VEH(i),SESSION_TIME);
                cars[i].restart_known=pending.valid;cars[i].restart_pending=!pending.valid || pending.pending;
                cars[i].lap=V_I32(i,OFF_LAPNUM);
                if(cars[i].lap==0 && V_F32(i,OFF_SPEED)<1.5f && !cars[i].pit)
                    cars[i].start_stall_dns=f1c_start_stall_dns(sc_marker_read,(uintptr_t)VEH(i));
            }
            cars[i].speed=V_F32(i,OFF_SPEED);cars[i].distance=V_F32(i,OFF_LAPDIST);cars[i].lap=V_I32(i,OFF_LAPNUM);
            unsigned hash=2166136261u;
            for(int k=0;k<31 && V_STR(i,OFF_NAME)[k];k++)hash=(hash^(unsigned char)V_STR(i,OFF_NAME)[k])*16777619u;
            cars[i].identity=(hash^(unsigned)V_I32(i,OFF_ROSTER))*16777619u;
            /* Read orientation at racing speed too: speed alone is not recovery.
               -1 means unknown/tilted,0 positively upright,1 inverted. */
            F1COrientation recovery_orientation=f1c_orientation_vehicle((void*)VEH(i));
            cars[i].inverted=!recovery_orientation.valid?-1:
                (recovery_orientation.up_dot<=g_sc_config.InvertedUpDotMax?1:
                 (recovery_orientation.up_dot>=0.5f?0:-1));
            /* Native current-wheel materials, same evidence as red flag.
               Read contacts only for slow potential hazards. Unknown fails closed. */
            if(isfinite(cars[i].speed)&&(cars[i].terminal||cars[i].speed<8.0f)&&!cars[i].pit) {
                cars[i].engine_motion=f1c_engine_motion(sc_marker_read,(uintptr_t)VEH(i));
                F1CSurfaceContacts surface=f1c_surface_contacts(VEH(i),sc_marker_read);
                cars[i].road_known=surface.known>0;cars[i].road_contacts=surface.drivable;
                cars[i].damage=V_I32(i,OFF_CARCOND)>0;
                if(cars[i].inverted==1){
                    F1CIncidentGeometry geom=f1c_geometry_read(i,(uintptr_t)VEH(i));
                    /* RF geometry supplies near-road coverage; native on-track is
                       an independent fallback for standalone SC, not wheel hits. */
                    cars[i].inverted_near=geom.known?geom.near_road:
                        (*(volatile unsigned char*)(VEH(i)+0x40E8u)==0);
                }

            }
        }
        for(int k=0;k<VEH_SLOTS;k++)if(g_retirement_suppressed[k])cars[k].valid=0;
        if(idle && g_retirement_danger && !sc_redflag_veto() && g_auto_state.quiet_until>SESSION_TIME)
            g_auto_state.quiet_until=SESSION_TIME; /* fresh detector confirmation still required */
        SCAutoResult result=sc_auto_step_observed(&g_auto_state,cars,VEH_SLOTS,g_epoch,
            SESSION_TIME,g_ct_auto && racing && n>=1,
            idle||g_engaged,t->zones>0,sc_redflag_veto(),g_lap_len,(double)GetTickCount64()/1000.0);
        if(g_engaged && result.fire && !sc_redflag_veto()) {
            g_sc_continuation=1;g_sc_forced_return=0;
            clearance_step(1); /* capture before acceptance consumes fresh evidence */
            sc_stint_begin(&g_sc_stint,g_epoch); /* new hazard earns a new bounded caution */
            sc_auto_accept(&g_auto_state);
            logf_("[continuation] new confirmed incident slot=%d reason=%d; GREEN revoked",result.slot,result.reason);
        }
        for(i=0;i<VEH_SLOTS && i<104;i++){
            int reason=!cars[i].valid?0:cars[i].inverted!=1?1:cars[i].pit?2:
                !cars[i].inverted_near?4:sc_redflag_veto()?7:!racing||!g_ct_auto?8:
                result.fire&&result.slot==i?6:5;
            f1c_decision_publish(0,i,(uintptr_t)VEH(i),reason);
        }
        if(result.reason)t->gates|=GATE_NOT_MASKED|GATE_NOT_FINAL; /* confirmed physical hazard overrides pace/final-lap heuristics */
        t->gates&=~(GATE_INCIDENT|GATE_HELD|GATE_NO_GRACE);
        if(SESSION_TIME>=g_auto_state.quiet_until || result.fire)t->gates|=GATE_NO_GRACE;
        t->grace_left=(int)fmax(0,(g_auto_state.quiet_until-SESSION_TIME)*1000);
        if(result.reason)t->gates|=GATE_INCIDENT;
        if(result.fire)t->gates|=GATE_HELD;
        t->hold_ms=(int)(result.held*1000);t->hold_req=(int)(result.required*1000);
        t->severity=result.count>=2?2:1;
        g_auto_reason=result.reason;
        if(result.slot>=0){t->worst_slot=result.slot;t->worst_speed=cars[result.slot].speed;
            lstrcpynA(t->worst_name,V_STR(result.slot,OFF_NAME),sizeof(t->worst_name));}
    }

    /* Closeness, for the trace only. Half the bar is the gates that are open,
     * half is the persistence timer - so a driver watching it can tell "the
     * incident is not seen yet" from "it is seen and counting".            */
    {
        int open = 0, k;
        static const int all[] = { GATE_RACING, GATE_AUTO_ON, GATE_IDLE,
                                   GATE_NOT_FINAL, GATE_NO_GRACE,
                                   GATE_NOT_MASKED, GATE_INCIDENT };
        for (k = 0; k < (int)(sizeof(all) / sizeof(all[0])); k++)
            if (t->gates & all[k]) open++;
        t->pct = (open * 50) / (int)(sizeof(all) / sizeof(all[0]));
        if (t->hold_req > 0)
            t->pct += (t->hold_ms >= t->hold_req)
                      ? 50 : (t->hold_ms * 50) / t->hold_req;
        if (t->pct > 100) t->pct = 100;
    }

    sc_read_scope_end();
    return (t->gates & GATE_ALL) == GATE_ALL;
}

/* ---- the pit lane under caution ----------------------------------------
 *
 * IT IS OPEN, AND IT HAS ALWAYS BEEN OPEN HERE. Nothing in this DLL closes it,
 * refuses a pit entry, or penalises one - and that is not an oversight, it is
 * the rule of the era being modelled. Formula One closed the pit lane under
 * the safety car for exactly two seasons, 2007 and 2008, when a car could not
 * enter until the field had formed up and "pit lane open" was given, on pain
 * of a ten-second stop/go. Before 2007 - which is the 1999-2002 game this mod
 * runs in - the pit lane simply stayed open, and pitting under a safety car
 * was one of the sharpest strategic calls in the race.
 *
 * What the DLL does do is make sure a car ON ITS WAY to the pits still obeys
 * the caution, which is a different thing entirely: pittingStatus 1 is the
 * in-lap and that car is still out on the circuit, so it stays governed. Only
 * from 2 is it in the lane, under the game's own 80 km/h limiter, and then we
 * let go of it completely (see pit_owned).
 *
 * This function only WATCHES. It exists because "everyone can still pit" was
 * a claim in a README, and a claim in a README is not evidence. Now it is a
 * count on the panel and a line in the log, per car, as it happens.       */
static int  g_pit_seen[VEH_SLOTS];   /* was this car in the lane last tick */
static int  g_pit_inlap = 0, g_pit_inpits = 0, g_pit_stops = 0;

static void pit_watch(void)
{
    int i, inlap = 0, inpits = 0;
    int caution = (g_ct != CT_OFF);

    for (i = 0; i < VEH_SLOTS; i++) {
        int st;

        if (!slot_live(i)) { g_pit_seen[i] = 0; continue; }
        if (i == g_sc_slot) continue;         /* his own stop is not a pit stop */

        st = V_I32(i, OFF_PITTING);
        if (st == 1) inlap++;
        if (st >= 2) inpits++;

        /* Rising edge into the lane. Logged only under caution, because under
         * green it is just a pit stop and the log has enough in it already. */
        if (st >= 2 && !g_pit_seen[i]) {
            g_pit_seen[i] = 1;
            if (caution) {
                g_pit_stops++;
                logf_("  [pits] slot %2d '%s' has entered the pit lane under "
                      "the safety car - allowed, and released from pace "
                      "control (the game's own limiter owns it now)",
                      i, IsBadReadPtr((void*)(VEH(i) + OFF_NAME), 8)
                         ? "?" : V_STR(i, OFF_NAME));
            }
        } else if (st < 2) {
            g_pit_seen[i] = 0;
        }
    }

    g_pit_inlap  = inlap;
    g_pit_inpits = inpits;
}

/* ---- starting him next to the pit entrance ------------------------------
 *
 * THIS IS MODE A, AND NOTHING ELSE. The one and only difference is WHERE he
 * starts his trip to the pits.
 *
 * Mode A - the vanilla path, driver-verified over many races - is entirely
 * sc_park()'s: at the green it arms the game's own trigger (veh+0xD58 = 0 plus
 * callToBox = 1), the game drives him down the pit lane at the limiter, parks
 * him, and sc_park() then lifts him out of the field; Numpad 0 revives him.
 * All of that is left EXACTLY as it is. This file adds one thing to it: he is
 * put on the racing line about 180 m before the pit entrance instead of
 * starting from the last grid box, so he does not drive a whole lap first.
 *
 * NOTHING HERE WRITES A PIT FIELD. Not callToBox, not pittingStatus, not the
 * rear-wing arming, not maxSpeed. Every version that did ended with him
 * driving off after his stop, because arming and then having the arming wiped
 * (sc_damage_clear re-asserts veh+0xD58 = 1 every tick) gives an ORDINARY pit
 * stop, and an ordinary stop ends with the car leaving the box. sc_park() owns
 * the pit call; this owns the position.
 *
 * THE TWO RULES THAT GOVERN THE TELEPORT ITSELF:
 *
 *   1. He must take the green IN THE FIELD. A car hidden, frozen or moved out
 *      of the field at the lights never has its brake hold released, and that
 *      is unrecoverable.
 *   2. While a car sits at the standing-start grid the game re-pins it to its
 *      grid slot every frame (measured on the player, three runs), so the drop
 *      is re-asserted until it takes and then left alone.
 *
 * Position is veh+0x34E0/E4/E8 - the authoritative field, not the 0x1C mirror.
 */
#define PITSTART_IDLE     0
#define PITSTART_SENT     1   /* dropped on the approach                      */
#define PITSTART_DONE     2   /* rolling - sc_park() has him from here        */

static int   g_pitstart      = 0;    /* GUI setting, OFF until proven          */
static int   g_pitstart_st   = PITSTART_IDLE;
static int   g_pitstart_fix  = 0;    /* re-asserted places: published          */
/* FORK: ALWAYS 0 (the GARAGE path). The extra car starts from the back of the
 * grid and the game retires him to his GARAGE on lap 1 - the only 100%
 * functional start (driver-decided 2026-08-07; the pit-box hand-over was
 * dropped). So every release is the proven garage release: pitting=0 +
 * curWaypoint=0, the game's own limiter is lifted at once. */
static int   g_sc_in_box     = 0;
static float g_box[4]        = {0,0,0,0};   /* x, y, z, yaw - from the GUI     */
static int   g_box_ok        = 0;
static float g_app[4]        = {0,0,0,0};   /* approach point, from the GUI    */
static int   g_app_ok        = 0;
static DWORD g_pitstart_ms   = 0;    /* when the drop was made                 */
static int   g_pitstart_pin  = 0;    /* logged the grid re-pin fight once      */
static int   g_pitstart_pre  = 0;    /* logged the pre-race burst once         */

/* Below this he is not moving under his own power yet, so the grid re-pin is
 * still winning and the position has to be re-asserted. */
#define PITSTART_ROLL_MS   5.0f
/* And it is re-asserted for no longer than this, so a car the start sequence
 * never released is left alone instead of pinned for the whole race. */
#define PITSTART_PIN_MS    20000u

static int find_safety_car(void);    /* defined just below                     */

/* Put him on the approach point, pointing along the racing line.
 *
 * POSITION AND YAW ONLY. Velocity is not touched - a teleported car keeps
 * driving from the new place (FACT), and veh+0x58/5C/60 is only a mirror
 * anyway: it read 111 m/s on a car whose true speed was 7.95.              */
static void pitstart_drop(int i)
{
    V_F32(i, OFF_BODYX)   = g_app[0];
    V_F32(i, OFF_BODYY)   = g_app[1];
    V_F32(i, OFF_BODYZ)   = g_app[2];
    V_F32(i, OFF_YAWBODY) = g_app[3];
}

static void pitstart_pump(void)
{
    int i;

    if (!g_pitstart || !g_app_ok) return;

    i = (g_sc_slot >= 0) ? g_sc_slot : find_safety_car();
    if (i < 0 || !slot_live(i)) return;

    /* Once he is wanted, hands off completely - sc_deploy()/sc_revive() own
     * him, exactly as in mode A. */
    if (g_sc_state != SC_IDLE || g_ct != CT_OFF) {
        g_pitstart_st = PITSTART_DONE;
        return;
    }

    /* BEFORE THE RACE: keep him off the end of the grid on the classification
     * screen. POSITION ONLY, and the start sequence undoes it - that is
     * intended, because he has to take the green in the field.
     *
     * The trigger is his position becoming real, not the phase: at phase 2 he
     * reads the world origin until the grid placement has run.             */
    if (RACE_PHASE < PHASE_RACING) {
        if (g_pitstart_st != PITSTART_IDLE) {
            g_pitstart_st  = PITSTART_IDLE;
            g_pitstart_fix = 0;
            g_pitstart_ms  = 0;
            g_pitstart_pin = 0;
        }
        if (RACE_PHASE < 2) g_pitstart_pre = 0;    /* a genuinely new session */
        {
            float x = V_F32(i, OFF_BODYX), z = V_F32(i, OFF_BODYZ);
            float dx, dz;
            if (x*x + z*z < 1.0f) return;          /* not placed yet */
            dx = x - g_app[0];
            dz = z - g_app[2];
            if (dx*dx + dz*dz > 4.0f) {
                pitstart_drop(i);
                if (!g_pitstart_pre) {
                    g_pitstart_pre = 1;
                    logf_("  [pitstart] pre-race: moved off the grid to the "
                          "pit approach from (%.1f, %.1f) - the start sequence "
                          "will take him back, and that is intended", x, z);
                }
            }
        }
        return;
    }

    /* THE DROP, the instant the race is running. */
    if (g_pitstart_st == PITSTART_IDLE) {
        if (!phase_racing()) return;
        pitstart_drop(i);
        g_pitstart_st = PITSTART_SENT;
        g_pitstart_ms = sc_sim_millis(SESSION_TIME);
        logf_("=== PIT-BOX START === safety car placed on the pit approach "
              "(%.2f, %.2f, %.2f) yaw %.3f - from here it is the ordinary "
              "safety-car path: sc_park() calls him in and parks him",
              g_app[0], g_app[1], g_app[2], g_app[3]);
        return;
    }

    /* HOLD HIM THERE UNTIL HE IS ROLLING, then never touch him again.
     *
     * This exists solely to out-write the grid re-pin. It stops the moment he
     * moves under his own power, and on a timer whatever happens, so a car the
     * start sequence never released is left alone rather than pinned all
     * race.                                                                */
    if (g_pitstart_st == PITSTART_SENT) {
        float dx, dz;
        if (V_F32(i, OFF_SPEED) >= PITSTART_ROLL_MS ||
            sc_sim_millis(SESSION_TIME) - g_pitstart_ms >= PITSTART_PIN_MS) {
            g_pitstart_st = PITSTART_DONE;
            logf_("  [pitstart] he is rolling at %.1f m/s on the approach - "
                  "hands off, the safety-car path has him now",
                  V_F32(i, OFF_SPEED));
            return;
        }
        dx = V_F32(i, OFF_BODYX) - g_app[0];
        dz = V_F32(i, OFF_BODYZ) - g_app[2];
        if (dx*dx + dz*dz > 4.0f) {
            pitstart_drop(i);
            g_pitstart_fix++;
            if (!g_pitstart_pin) {
                g_pitstart_pin = 1;
                logf_("  [pitstart] the start sequence is still pulling him "
                      "back to his grid slot - holding him on the approach "
                      "point until he rolls");
            }
        }
    }
}

/* ---- suppressing the game's own commentary about the safety car ---------
 *
 * He is a normal entrant, so the race engineer talks about him: "... is
 * entering the pits", "... is already at the pits", and the AIPIT lines when
 * he drives his in-lap. Nothing about him should ever be announced.
 *
 * EVENT BLOCKING CANNOT DO THIS. The AIPIT eids (178/179/180/181/182/196) are
 * per-LINE, not per-driver - parking one of their cooldowns at FLT_MAX, the
 * way slowcar_block() kills eid 132, would silence that line for the whole
 * grid. Score 130 was killable precisely because exactly one event reached it;
 * these are reached by twenty-two cars.
 *
 * So the filter has to be per-DRIVER, and there is one choke point where the
 * driver is still known: every commentary line in the game is inserted into
 * the controller's queue by
 *
 *     char __thiscall sub_418560(float *this, float *entry)   // 40 callers
 *
 * and the entry was built by DriverDataTableLookupAndTimeConversion (0x4144F0),
 * whose third argument - the driver the line is ABOUT - it stores at
 * this+4 dwords, i.e. entry+0x10. Drop the entries whose entry+0x10 is his and
 * exactly his lines disappear; every other car keeps all of them.
 *
 * WHICH FIELD THE EMITTERS PASS - and it is not one field, it is two.
 * Across the twenty call sites:
 *
 *   19 sites pass veh+0x489C  (a1[4647] / a2+18588)   - the DRIVER index
 *    1 site  passes veh+0x48A0 (a1+18592)             - AIVehicleDamageEvent
 *                                                       0x41A170, eids 52/196
 *
 * and 196 is PWS_AIPIT01, one of the lines being suppressed - so keying on the
 * driver index alone would miss it.
 *
 * READ LIVE 2026-08-02, 23-car field, Australia (poke.py, one pass over every
 * slot). This is what makes the two keys safe to use:
 *
 *   veh+0x4894 = the slot index. 0,1,2,...  Not an identity.
 *   veh+0x4898 = an exact mirror of 0x489C on all 23 cars.
 *   veh+0x489C = DRIVER index, UNIQUE across the field.
 *                Maylander 262, player 57, M.Schumacher 27, Irvine 38.
 *   veh+0x48A0 = CONSTRUCTOR index, NOT unique - it pairs off along team
 *                lines: Schumacher/Irvine 3, Hakkinen/Coulthard 4,
 *                Hill/Frentzen 9, Barrichello/Herbert 24, Badoer/Gene 26,
 *                Fisichella/Wurz 2, Trulli/Panis 19, Diniz/Alesi 14,
 *                Takagi/DeLaRosa 1, Villeneuve/Zonta 6. Maylander is 32,
 *                which nothing else in the field holds.
 *
 * Because 0x48A0 is a TEAM index, using it as a drop key is only safe while no
 * other running car shares it - otherwise a real team is silenced. So it is
 * resolved per session and VERIFIED unique against every live car, in both
 * fields, before it is armed; if anything clashes the team key is left off and
 * only the driver key is used. That costs eid 52/196 and nothing else.
 *
 * THE PATCH ITSELF IS ATOMIC, which is the only reason it is safe to install
 * into a running game. sub_418560's first three instructions are
 *
 *     0x00418560  83 ec 0c     sub  esp, 0xc
 *     0x00418563  53           push ebx
 *     0x00418564  8b 5c 24 14  mov  ebx, [esp+0x14]
 *
 * = 8 bytes, and 0x00418560 is 16-byte aligned. So the whole hook - a 5-byte
 * jmp plus three nops - is ONE aligned 8-byte store, and no thread can ever
 * observe a half-written prologue. That is the same argument the yellow-flag
 * predicate patch rests on at 0x0049BAD0, one size up. Without it this would
 * be a genuine race: a thread stopped between `sub esp` and `mov ebx` while
 * the tail bytes changed underneath it would take a garbage ebx.
 *
 * This is the DLL's first EXECUTABLE detour; everything else it patches is
 * data or one aligned instruction. Restored byte-for-byte on eject and
 * whenever the field is rebuilt.                                           */

#define IDTE_ADDR    0x00418560u  /* InsertDriverTableEntry, the choke point */
#define OFF_DRVIDX   0x489Cu      /* int32, driver index      [CONFIRMED]    */
#define OFF_TEAMIDX  0x48A0u      /* int32, constructor index [CONFIRMED]    */

static int field_stable(void);      /* defined with the hiding code below   */

static int      g_hush_want  = 1;   /* GUI setting                          */
static int      g_hush_on    = 0;   /* the detour is installed              */
static int      g_hush_drv   = -1;  /* his driver index, or -1              */
static int      g_hush_team  = -1;  /* his team index, or -1 if it clashes  */
static unsigned g_hush_drops = 0;
static unsigned g_hush_tdrops = 0;
static unsigned g_radio_drops = 0;

static unsigned char *g_idte_tramp = NULL;
static unsigned char  g_idte_orig[8];

static const unsigned char IDTE_PROLOGUE[8] = {
    0x83,0xEC,0x0C, 0x53, 0x8B,0x5C,0x24,0x14
};

/* Return 1 to DROP the entry. Runs on the GAME thread, inside the game's own
 * call, so it does nothing but compare two integers - no logging, no locks,
 * no allocation. The counters are read by the tick loop instead.           */
static int __attribute__((cdecl, used)) idte_drop(const int *entry)
{
    if (!g_hush_on || !entry) return 0;
    if(sc_radio_suppress(entry[0],g_engaged&&!g_green_said)) {
        g_radio_drops++;return 1;
    }
    if (g_hush_want&&g_hush_drv>=0&&entry[4]==g_hush_drv)
        { g_hush_drops++;return 1; }
    if (g_hush_want&&g_hush_team>=0&&entry[4]==g_hush_team)
        { g_hush_tdrops++;return 1; }
    return 0;
}

/* On entry esp points at the return address and the one stack argument is at
 * [esp+4], because we are standing on the function's very first byte and it
 * has pushed nothing yet. pushad(32) + pushfd(4) puts it at [esp+40].
 * Dropping means returning 0 in al with `ret 4` - sub_418560 is __thiscall
 * with one stack argument and cleans 4 bytes itself.                       */
__attribute__((naked)) static void idte_stub(void)
{
    __asm__ __volatile__(
        "pushal\n\t"
        "pushfl\n\t"
        "movl 40(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "call _idte_drop\n\t"
        "addl $4, %esp\n\t"
        "testl %eax, %eax\n\t"
        "jnz 1f\n\t"
        "popfl\n\t"
        "popal\n\t"
        "jmp *_g_idte_tramp\n\t"
        "1:\n\t"
        "popfl\n\t"
        "popal\n\t"
        "xorl %eax, %eax\n\t"
        "ret $4\n\t"
    );
}

/* One aligned 8-byte store. See the header above - this is what makes patching
 * a live code path safe rather than merely unlikely to be caught.          */
static void store8_aligned(volatile void *dst, const void *src)
{
    __asm__ __volatile__(
        "movq (%1), %%xmm0\n\t"
        "movq %%xmm0, (%0)\n\t"
        :
        : "r"(dst), "r"(src)
        : "xmm0", "memory");
}

/* Resolve his two identities and prove the team key is his alone. Returns 0
 * if the field is not ready or the identities are not usable; the caller just
 * tries again next tick.                                                   */
static int hush_resolve(int sc)
{
    int drv, team, i, team_clash = 0;

    if (sc < 0) return 0;
    drv  = V_I32(sc, OFF_DRVIDX);
    team = V_I32(sc, OFF_TEAMIDX);
    if (drv < 0) return 0;                    /* not populated yet */

    for (i = 0; i < VEH_SLOTS; i++) {
        if (i == sc || V_I32(i, OFF_ACTIVE) == -1) continue;
        if (V_I32(i, OFF_DRVIDX) == drv) {
            logf_("  [hush] REFUSED: driver index %d is not unique (slot %d "
                  "'%s' has it too) - suppression would silence a real car",
                  drv, i, V_STR(i, OFF_NAME));
            return 0;
        }
        /* The team key must not collide with anyone's team OR anyone's driver
         * index, because entry+0x10 carries both kinds of value depending on
         * which emitter built it. */
        if (V_I32(i, OFF_TEAMIDX) == team || V_I32(i, OFF_DRVIDX) == team)
            team_clash = 1;
    }

    g_hush_drv  = drv;
    g_hush_team = team_clash ? -1 : team;

    if (team_clash)
        logf_("  [hush] driver index %d armed; team index %d NOT armed - it "
              "collides with a running car, so eids 52/196 stay audible",
              drv, team);
    else
        logf_("  [hush] identities resolved: driver %d, team %d, both unique "
              "across %d slots", drv, team, VEH_SLOTS);
    return 1;
}

static void hush_install(void)
{
    unsigned char *at = (unsigned char*)IDTE_ADDR;
    unsigned char  patch[8];
    DWORD old = 0;

    if (g_hush_on) return;

    if (memcmp(at, IDTE_PROLOGUE, 8) != 0) {
        logf_("  [hush] REFUSED: 0x%08X does not hold the expected prologue "
              "(%02X %02X %02X %02X %02X %02X %02X %02X) - another mod is "
              "already there, or the exe is patched. Suppression disabled.",
              IDTE_ADDR, at[0], at[1], at[2], at[3], at[4], at[5], at[6], at[7]);
        g_hush_want = 0;
        return;
    }

    if (!g_idte_tramp) {
        g_idte_tramp = (unsigned char*)VirtualAlloc(NULL, 32,
                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_idte_tramp) { logf_("  [hush] VirtualAlloc failed"); return; }
        memcpy(g_idte_tramp, IDTE_PROLOGUE, 8);      /* the stolen bytes    */
        g_idte_tramp[8] = 0xE9;                      /* jmp back past them  */
        *(int*)(g_idte_tramp + 9) =
            (int)(at + 8) - (int)(g_idte_tramp + 13);
    }

    memcpy(g_idte_orig, at, 8);

    patch[0] = 0xE9;
    *(int*)(patch + 1) = (int)(void*)idte_stub - (int)(at + 5);
    patch[5] = patch[6] = patch[7] = 0x90;

    if (!VirtualProtect((LPVOID)at, 8, PAGE_EXECUTE_READWRITE, &old)) {
        logf_("  [hush] VirtualProtect failed at 0x%08X", IDTE_ADDR);
        return;
    }
    g_hush_on = 1;                 /* before the store: the very next call
                                    * through the detour must already filter */
    store8_aligned(at, patch);
    VirtualProtect((LPVOID)at, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)at, 8);

    logf_("=== COMMENTARY SUPPRESSION ARMED === InsertDriverTableEntry "
          "0x%08X detoured; dropping queue entries about driver %d%s",
          IDTE_ADDR, g_hush_drv,
          g_hush_team >= 0 ? " and team index (eids 52/196)" : "");
}

static void hush_uninstall(const char *why)
{
    unsigned char *at = (unsigned char*)IDTE_ADDR;
    DWORD old = 0;

    if (!g_hush_on) return;

    g_hush_on = 0;                 /* stop filtering before unhooking */
    if (VirtualProtect((LPVOID)at, 8, PAGE_EXECUTE_READWRITE, &old)) {
        store8_aligned(at, g_idte_orig);
        VirtualProtect((LPVOID)at, 8, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (LPCVOID)at, 8);
    }
    logf_("  [hush] suppression removed (%s) - dropped %u driver, %u "
          "team and %u caution-inappropriate lines",why,g_hush_drops,
          g_hush_tdrops,g_radio_drops);
    g_hush_drv    = -1;
    g_hush_team   = -1;
    g_hush_drops  = 0;
    g_hush_tdrops = 0;
    g_radio_drops = 0;
}

/* Arm as soon as a safety car exists in a built field, and stay armed for the
 * whole session - the lines being suppressed are said while he sits in the
 * garage, long before any caution. hush_uninstall on a session change is what
 * keeps the identities from going stale: driver and team indices are per
 * session, and a stale one would silence an innocent car in the next race. */
static void hush_pump(void)
{
    int sc;
    if (!field_stable()) return;
    if(g_hush_on) {
        if(!g_hush_want){g_hush_drv=-1;g_hush_team=-1;return;}
        if(g_hush_drv>=0)return;
    }
    sc = find_safety_car();
    if (sc < 0) return;
    if(g_hush_want){if(!hush_resolve(sc))return;}
    else {g_hush_drv=-1;g_hush_team=-1;}
    if(!g_hush_on)hush_install();
}

/* ---- keeping him in the world while he waits ---------------------------
 *
 * A RETIRED CAR HAS NO GROUND COLLISION, AND HIDING IT DOES NOT STOP THE FALL.
 *
 * Bug 4.4 recorded the first half of this: an un-hidden but still-retired car
 * falls, measured at PhysY -13513 after a 52 s wait. The fix was to keep him
 * hidden until one tick before the revive - and that turned out to treat the
 * symptom. Lifting him out of the active array stops the game CLASSIFYING him,
 * not SIMULATING him. Driver report 2026-08-01, "the safety car fell through
 * the ground again when it pitted": PhysY -45375 and falling at 179.9 m/s
 * while hidden, from a garage wait of well under a minute.
 *
 * So his garage pose is captured the moment he is parked, and re-applied - with
 * the velocity zeroed - whenever he has drifted from it. That also makes a
 * fallen car RECOVERABLE, which is why sc_revive() no longer refuses outright:
 * it puts him back on the captured pose first and only gives up if there is no
 * pose to put him on.                                                       */
static int   g_sc_pose_ok = 0;
static float g_sc_pose[3];

/* The pose is written to disk because it is worth more on the NEXT race than
 * on this one: with it known up front he can be put in his garage the instant
 * the game places him, so he never appears on the starting grid at all.
 * Track-specific, so the file is keyed by nothing and simply re-captured
 * whenever he garages - a stale pose from another circuit is overwritten the
 * first time he parks. */
#define POSE_PATH "sc_garage_pose.txt"

static void sc_pose_save(void)
{
    FILE *f = fopen(POSE_PATH, "w");
    if (!f) return;
    fprintf(f, "%.4f %.4f %.4f\n", g_sc_pose[0], g_sc_pose[1], g_sc_pose[2]);
    fclose(f);
}

static void sc_pose_load(void)
{
    float x, y, z;
    FILE *f = fopen(POSE_PATH, "r");
    if (!f) return;
    if (fscanf(f, "%f %f %f", &x, &y, &z) == 3 &&
        (x*x + z*z) > 1.0f && y > -50.0f && y < 500.0f) {
        g_sc_pose[0] = x; g_sc_pose[1] = y; g_sc_pose[2] = z;
        g_sc_pose_ok = 1;
        logf_("  [pose] garage pose loaded from " POSE_PATH
              " (%.1f, %.1f, %.1f) - he will be parked there before the start",
              x, y, z);
    }
    fclose(f);
}

static void sc_pose_capture(int i)
{
    g_sc_pose[0]  = V_F32(i, OFF_PHYSX);
    g_sc_pose[1]  = V_F32(i, OFF_PHYSY);
    g_sc_pose[2]  = V_F32(i, OFF_PHYSZ);
    g_sc_pose_ok  = 1;
    logf_("  [pose] safety car garage pose captured (%.1f, %.1f, %.1f) - he "
          "will be held on it until he is released",
          g_sc_pose[0], g_sc_pose[1], g_sc_pose[2]);
    /* sc_pose_save() removed. */
}

static void sc_pose_apply(int i)
{
    /* DISABLED 2026-08-01, PERMANENTLY.
     *
     * This wrote the vehicle position triad to move the safety car to a saved
     * garage pose. It teleported the car to a random-looking place and the
     * game crashed, repeatedly, because a saved pose does not survive a
     * session or track change and because a position write is only valid
     * while that exact session's physics is running.
     *
     * No part of this DLL writes a car's position or velocity any more. The
     * safety car is moved ONLY by the game's own code, via callToBox. */
    (void)i;
}

/* Put him in his garage NOW, wherever the game just placed him.
 *
 * Driver's ask, 2026-08-01: he should not be sitting in grid box 23 at the
 * start. Hiding him cannot do it - the corpus already proves rendering follows
 * session PLACEMENT, not the field list, and retiring him does not stop the
 * placement either (both tested 2026-07-31). The car has to be moved, and the
 * game's own retirement teleport is exactly that: a write of the position
 * triad, which is how a real AI retirement puts a car in its garage.
 *
 * So once his garage pose is known he is dropped straight into it and flagged
 * garaged, which is the state the deploy sequence already expects. He never
 * drives the in-lap and never appears in the standings or on the grid.     */
/* THE MOVE MUST HAPPEN WHILE THE CAR IS STILL LIVE.
 *
 * A position write is consumed by the physics step, and switching the car off
 * first means there is no step to consume it: name_index's PhysY note says so
 * outright, "writing this after ActiveFlag(0x45F4)=0 does NOT move the car".
 * The first version applied the pose and set retired/AI-off in the same call,
 * so the write was silently restored from the physics body every tick and he
 * never left grid box 23 - the log cheerfully reported "moved into the garage"
 * while reading back the grid coordinates (2026-08-01).
 *
 * So it runs in two stages: hold him on the pose with the car still live for
 * SC_PARK_SETTLE_TICKS, and only then switch him off.                      */
#define SC_PARK_SETTLE_TICKS 6      /* 300 ms at 20 Hz */

static int g_park_ticks = 0;

static void sc_force_garage(int i)
{
    sc_pose_apply(i);                     /* physics still running here */

    if (++g_park_ticks < SC_PARK_SETTLE_TICKS) return;

    V_I32(i, OFF_FINISH)                      = 2;
    *(volatile char*)(VEH(i) + OFF_RETIRED)   = 1;
    *(volatile char*)(VEH(i) + OFF_AICTRL)    = 0;
    V_I32(i, OFF_PITSTATE)                    = 2;
    *(volatile char*)(VEH(i) + OFF_DRIVERVIS) = 0;
}

/* Every tick, while he is parked and not yet out. */
static void sc_pose_pump(void)
{
    int   i = find_safety_car();
    float y;

    if (i < 0) return;
    if (sc_is_out() || g_sc_state == SC_OUTLAP || g_sc_state == SC_CATCHUP) {
        g_sc_pose_ok = 0;          /* he is driving; his pose is his own */
        return;
    }
    /* PRE-RACE: if his garage is already known, put him in it as soon as the
     * game has placed him, so he is never on the grid to be seen.
     *
     * "Placed" is the test, not the phase: at phase 2 his position still reads
     * the world origin (measured live 2026-08-01: 0.00, 0.32, -0.01), and the
     * grid placement happens later. Writing before that is pointless - the
     * placement would overwrite it - so the trigger is his position becoming
     * real. Re-applied every tick, so a later re-placement is undone too. */
    if (!phase_racing()) {
        /* PRE-START TELEPORT REMOVED 2026-08-01. It moved the safety car to a
         * saved pit-box pose before the lights, and the pose does not survive
         * a track or session change - the car appeared in a random place and
         * the game crashed. Nothing here writes a position outside racing. */
        if (0) {
            float x = V_F32(i, OFF_PHYSX), z = V_F32(i, OFF_PHYSZ);
            float dx, dz;
            if (x*x + z*z < 1.0f) return;            /* not placed yet */
            dx = x - g_sc_pose[0];
            dz = z - g_sc_pose[2];
            if (dx*dx + dz*dz > 4.0f) {
                sc_force_garage(i);
                logf_("  [pose] placed at (%.1f, %.1f) - moved into the garage "
                      "before the start, he is not on the grid", x, z);
            }
        }
        return;
    }

    if (*(volatile char*)(VEH(i) + OFF_RETIRED) != 1 &&
        V_I32(i, OFF_PITTING) < 4) return;   /* not parked yet */

    y = V_F32(i, OFF_PHYSY);

    if (!g_sc_pose_ok) {
        float x = V_F32(i, OFF_PHYSX), z = V_F32(i, OFF_PHYSZ);

        /* CAPTURE ONLY WHERE THE GAME PUT HIM, NEVER WHERE WE PUT HIM.
         *
         * The retired flag is not evidence of a garage - this DLL sets it
         * itself, in sc_force_garage() and in sc_park()'s garaged branch.
         * Keying the capture on it closed a loop 2026-08-01: the pre-start
         * park flagged him retired in grid box 23, the capture read that as
         * "he is in his garage" and saved (-23.9, 429.6) as the garage, and
         * from then on he was teleported to his own grid box every tick,
         * retired, AI off and hidden. Driver: "Maylander stuck in grid now."
         *
         * pittingStatus >= 4 is the game's own state and cannot be reached by
         * anything this DLL writes, so it is the only safe witness.       */
        if (V_I32(i, OFF_PITTING) < 4) return;

        /* A real garage is never at the origin. */
        if (x*x + z*z < 1.0f) return;
        if (y > -50.0f && y < 500.0f) sc_pose_capture(i);
        return;
    }
    if (y < g_sc_pose[1] - 1.0f) {
        static DWORD last_log = 0;
        DWORD now = GetTickCount();
        sc_pose_apply(i);
        if (now - last_log > 5000) {
            last_log = now;
            logf_("  [pose] safety car had fallen to Y=%.1f - put back on the "
                  "garage pose; transform restore attempted (velocity not asserted)", y);
        }
    }
}

/* The garage-exit recipe from sc1.c. Order matters: disarm the retirement
 * trigger, revive, hand to the state machine, then crank - the crank needs a
 * running physics step, which the AI-control flag restores.                */
#include "f1c_placement.h"
#include "f1c_car_restart.h"
static int sc_hide(int slot);
/* Exact bytes of the existing parked control fields, not the vehicle object.
   Native placement restores transform/motion; heap/scene/audio objects stay live. */
static const unsigned sc_park_offsets[]={OFF_MAXSPEED,OFF_CALLBOX,OFF_CARCOND,
    OFF_REASON,OFF_FINISH,OFF_PITSTATE,OFF_PITTING,OFF_LANEMODE,OFF_DRVSTAT,
    OFF_ANGVEL,OFF_RPM,OFF_PASSING,OFF_AGGRO}; /* waypoint is rebuilt natively */
static const unsigned sc_park_bytes[]={OFF_RWFLAG,OFF_FWFLAG,OFF_RETIRED,OFF_AICTRL,OFF_DRIVERVIS};
#define SC_PARK_WORDS (sizeof(sc_park_offsets)/sizeof(sc_park_offsets[0]))
#define SC_PARK_BYTES (sizeof(sc_park_bytes)/sizeof(sc_park_bytes[0]))
static struct {
    int valid,parked,slot,roster,in_box,failed;
    uint32_t words[SC_PARK_WORDS]; unsigned char bytes[SC_PARK_BYTES];
    F1CPlacement pose;
} g_initial_park;
static void sc_initial_invalidate(void){memset(&g_initial_park,0,sizeof(g_initial_park));}
static int sc_initial_owned(void){
    return g_initial_park.valid && sc_slot_owned() && g_sc_slot>0 &&
        g_initial_park.slot==g_sc_slot && g_initial_park.roster==g_sc_roster;
}
static int sc_initial_capture(void){
    int i=g_sc_slot;
    if(g_initial_park.failed)return 0;
    if(g_initial_park.valid)return sc_initial_owned();
    if(!on_game_thread()||!field_stable()||!sc_slot_owned()||i<=0||!g_hidden||
       !phase_racing()||!sc_target_compatible()||
       !isfinite(V_F32(i,OFF_SPEED))||fabsf(V_F32(i,OFF_SPEED))>0.5f)return 0;
    if(!f1c_placement_capture((void*)VEH(i),&g_initial_park.pose))return 0;
    for(unsigned k=0;k<SC_PARK_WORDS;k++)
        memcpy(&g_initial_park.words[k],(void*)(VEH(i)+sc_park_offsets[k]),4);
    for(unsigned k=0;k<SC_PARK_BYTES;k++)
        g_initial_park.bytes[k]=*(volatile unsigned char*)(VEH(i)+sc_park_bytes[k]);
    g_initial_park.slot=i;g_initial_park.roster=g_sc_roster;
    g_initial_park.in_box=g_sc_in_box;
    g_initial_park.valid=1;
    logf_("[pit-reset] initial pre-call state captured: slot=%d position=(%.3f,%.3f,%.3f)",
          i,g_initial_park.pose.position[0],g_initial_park.pose.position[1],g_initial_park.pose.position[2]);
    return 1;
}
static int sc_initial_restore(void){
    int i=g_sc_slot; F1CPlacement actual;
    if(!on_game_thread()||!field_stable()||!phase_racing()||!sc_initial_owned()||
       g_initial_park.failed||!sc_target_compatible())return 0;
    int rebuilding=!g_initial_park.parked;
    if(rebuilding){
        if(!f1c_car_restart_ready())return 0;
        if(!g_recovery.native_budget || g_recovery.native_budget(g_client)!=F1C_BROKER_OK){
            logf_("[pit-reset] refused: native callback budget unavailable");return 0;
        }
        /* Latch failure before any resource mutation: never retry a partial init. */
        g_initial_park.failed=1;
        if(g_hidden)sc_unhide();
        if(g_hidden||!sc_slot_owned())return 0;
        if(!f1c_car_restart((void*)VEH(i))||!sc_initial_owned())return 0;
    }
    if(!f1c_placement_restore((void*)VEH(i),&g_initial_park.pose))return 0;
    if(!f1c_placement_capture((void*)VEH(i),&actual))return 0;
    for(int k=0;k<3;k++)if(fabsf(actual.position[k]-g_initial_park.pose.position[k])>0.1f ||
        fabsf(actual.orientation[k]-g_initial_park.pose.orientation[k])>0.001f)return 0;
    if(rebuilding){
        /* Reacquire native route/traffic/command state at the restored pose.
           Never put an old waypoint pointer back after native initialization. */
        if(!f1c_car_rebind_ai((void*)VEH(i))||!sc_initial_owned())return 0;
        g_sc_pose_ok=0;g_sc_mark=0;g_sc_recall_armed=0;
    }
    /* Remove only the SC through the existing owned field lifecycle. */
    if(!sc_hide(i))return 0;
    for(unsigned k=0;k<SC_PARK_WORDS;k++)
        memcpy((void*)(VEH(i)+sc_park_offsets[k]),&g_initial_park.words[k],4);
    for(unsigned k=0;k<SC_PARK_BYTES;k++)
        *(volatile unsigned char*)(VEH(i)+sc_park_bytes[k])=g_initial_park.bytes[k];
    g_sc_in_box=g_initial_park.in_box;
    g_initial_park.failed=0;g_initial_park.parked=1;g_sc_state=SC_IDLE;g_sc_cmd=0;
    /* The field remains under caution until GREEN; preserve its guard/ramp. */
    return 1;
}

static int sc_revive(int i)
{
    float y = V_F32(i, OFF_PHYSY);
    if(!f1c_sc_fuel_fill((uintptr_t)VEH(i),g_game_thread,
        i==g_sc_slot && sc_slot_owned() && sc_offline_authorized())) {
        hud_notice("SAFETY CAR UNAVAILABLE");return 0;
    }

    /* Out of the world: put him back rather than giving up, if there is a
     * captured pose to put him back on. */
    if (y < -50.0f || y > 500.0f) {
        if (!g_sc_pose_ok) {
            logf_("SAFETY CAR: REFUSING to revive - he is at Y=%.1f and no "
                  "garage pose was ever captured. Restart the session.", y);
            hud_notice("SAFETY CAR LOST");
            g_sc_state = SC_IDLE;
            if(g_engaged){g_restart_hold=1;g_restart_fault=1;g_ct=CT_RESTART;yellow_set(1);say_flush("SC lost");}
            return 0;
        }
        /* Out of the world and NOT recoverable: this DLL no longer moves
         * cars. Refuse, and say so, rather than teleport him. */
        logf_("SAFETY CAR: REFUSING to revive - he is at Y=%.1f. Restart the "
              "session.", y);
        hud_notice("SAFETY CAR LOST");
        g_sc_state = SC_IDLE;
        if(g_engaged){g_restart_hold=1;g_restart_fault=1;g_ct=CT_RESTART;yellow_set(1);say_flush("SC lost");}
        return 0;
    }
    gfx_hide(i, 0);          /* he is coming back out - show the car again */
    /* FORK: the release out of his pit box must NOT lift the game's own
     * 80 km/h limiter while he is still in the lane. The limiter was applied
     * by the pit machinery and the game clears it itself at the pit-exit
     * line (SC_OUTLAP then writes SPEED_RELEASE). pace_probe wrote 99999
     * here unconditionally - right for a garage release, a launch for a box
     * release ("he launched off after pitting", 2026-08-03). */
    if (!g_sc_in_box)
        V_F32(i, OFF_MAXSPEED) = (float)SPEED_RELEASE;
    *(volatile char*)(VEH(i) + OFF_RWFLAG)              = 1;  /* or he re-retires */
    V_I32(i, OFF_CALLBOX)                               = 0;
    /* ON THE LANE, NOT ON THE RACING LINE.
     *
     * From a GARAGE, pittingStatus 0 is right - he drives out under his own
     * AI. From his PIT BOX it is a lie: the car is in the pit lane, and told
     * it is on the circuit the AI aims at the racing line and drives into the
     * pit wall (driver, 2026-08-03). Handing him the lane state instead is
     * what the manual trial did, and he drove out under his own AI at the
     * limiter. The game clears it itself at the pit-exit line.           */
    if (g_sc_in_box) {
        /* DO NOT TOUCH pittingStatus HERE.
         *
         * He is parked at 4, which is what a real car carries while it LEAVES
         * its box - measured: at 4 he rolled out under the game's own limiter
         * (maxSpeed 22). Writing 2 says "arriving for a stop" and the game
         * starts a fresh one: 2026-08-03, revive wrote 2 and the value went
         * straight to 3 (being serviced) on the next tick, the crew jacked him
         * up and took the wheels off, and he was released mid-service -
         * driver: "he instantly starts having a new pit stop when activated,
         * then he crashes into a wall with apparently missing wheels".
         *
         * Only the lane path is asserted; the state is the game's.        */
        V_I32(i, OFF_LANEMODE)                          = 1;
    } else {
        V_I32(i, OFF_PITTING)                           = 0;
    }
    V_I32(i, OFF_FINISH)                                = 0;
    *(volatile char*)(VEH(i) + OFF_RETIRED)             = 0;
    *(volatile char*)(VEH(i) + OFF_AICTRL)              = 1;
    V_I32(i, OFF_DRVSTAT)                               = 3;  /* approach, NOT 0 */
    *(volatile char*)(VEH(i) + OFF_DRIVERVIS)           = 1;  /* else empty cockpit */
    /* NULLING THE WAYPOINT IS A GARAGE FIX AND A PIT-BOX BUG.
     *
     * Out of a garage, 0 forces the AI to re-acquire and is what stopped him
     * driving into the garage wall. Out of his BOX he already holds a valid
     * pit-lane waypoint, put there by the game during the stop (read live as a
     * real pointer), and taking it away leaves him with no route at all - he
     * drives straight ahead into the pit wall. So it is left alone when he is
     * in the box.                                                          */
    if (!g_sc_in_box)
        V_I32(i, OFF_CURWP)                             = 0;  /* garage-wall fix */
    V_F32(i, OFF_ANGVEL)                                = SC_CRANK_RPM;
    V_F32(i, OFF_RPM)                                   = SC_CRANK_RPM;
    /* His aggression stays DOWN for good. It is not a caution measure - it is
     * what keeps him on the road: he pushes 100% throttle the whole time, and
     * a racing brain at that pace puts him in the scenery. Driver's spec,
     * 2026-08-01: "it should push around the track with 100% pace, only with
     * a little to none aggression so it won't drift off the track and crash."
     * His speed is never touched; only his willingness to fight.           */
    sc_ai_calm(i);

    g_sc_pose_ok = 0;      /* he is driving now; his pose is his own again */
    return 1;
}

/* HE IS NOT A COMPETITOR, SO HE DOES NOT CARRY DAMAGE.
 *
 * Driver report 2026-08-01: "safety car crashed during pit entry, lost its
 * wheel". A damaged safety car breaks every phase downstream of it - he cannot
 * lead the queue, cannot reach the pit lane, and the recall never completes -
 * and unlike a race car there is nothing to protect by leaving him fragile.
 *
 * SeasonData/damage.ini is GLOBAL, one file for all 23 cars, and it currently
 * carries WheelRandom=1.0, i.e. every qualifying impact detaches a wheel. It
 * cannot single him out, so the exemption has to be made here.
 *
 * WHAT THIS DOES AND DOES NOT COVER. These four fields are the per-vehicle
 * damage STATE, and holding them clean stops damage retiring him, sending him
 * to the pits for repairs, or writing a ReasonCode that the rest of this DLL
 * reads. It does NOT put a detached wheel back on: the per-wheel physics record
 * mapped in the corpus (0x7C7428, stride 1124, detach flag +1104) is the
 * PLAYER's object, and the AI equivalent is an unmapped heap child. Mapping it
 * is a separate live probe.
 *
 * The rear wing flag is deliberately NOT clamped while he is being recalled:
 * sc_send_to_box() zeroes that exact byte to arm the game's own retirement
 * path, and clamping it here would silently cancel every recall.           */
static void sc_damage_clear(int i)
{
    if (!sc_slot_owned() || i != g_sc_slot) return;
    if (i < 0 || !slot_live(i)) return;

    *(volatile char*)(VEH(i) + OFF_FWFLAG) = 1;   /* front wing intact */
    V_I32(i, OFF_CARCOND)  = 0;                   /* no damage conditions */
    V_I32(i, OFF_REASON)   = 0;                   /* no retirement reason */

    *(volatile char*)(VEH(i) + OFF_RWFLAG) = 1;
}

/* Arm the game's own trigger and hand off - it drives him in and garages him
 * itself. Never forge the end state out on track.                          */
static void sc_send_to_box(int i)
{
    V_F32(i, OFF_MAXSPEED)                  = (float)SPEED_RELEASE;
    *(volatile char*)(VEH(i) + OFF_RWFLAG)  = 1;
    V_I32(i, OFF_CALLBOX)                   = 1;
}

/* Only once he is PACING does he become the pace line's leader. While he is
 * still catching the field the race leader keeps that job, otherwise every car
 * ahead of him is treated as ahead of the leader and held at the fallback
 * crawl - which is exactly what happened on the first live deploy: he paced at
 * 30 m/s in P19 while the real leaders were slowed waiting for him.       */
static int sc_is_out(void)
{
    return sc_slot_owned() && (g_sc_state == SC_ONTRACK ||
        (g_sc_state == SC_RECALL && g_sc_slot>=0 && !pit_owned(g_sc_slot)));
}

/* The best-positioned car that is NOT the safety car - i.e. the car actually
 * leading the race. Distinct from find_leader(), which returns the safety car
 * while he is out because he leads the QUEUE. Anything that reasons about the
 * race (who is a lap down, how far ahead the safety car has got, who takes the
 * green) must use this one; anything that reasons about the pace line uses
 * find_leader(). Conflating the two made every car look a lap down the moment
 * the safety car's lap number was raised to put him at the top of the
 * standings - see sc_lap_pump().                                            */
static int race_leader_slot(void)
{
    int i, best = -1, best_pos = 1 << 30;

    for (i = 0; i < VEH_SLOTS; i++) {
        int p;
        if (i == g_sc_slot || !slot_participant(i) || !slot_moving(i)) continue;
        p = V_I32(i, OFF_POSITION);
        if (p > 0 && p < best_pos) { best_pos = p; best = i; }
    }
    return best;
}

/* Metres the safety car is AHEAD of the race leader, negative if still behind. */
static float sc_ahead_of_leader(void)
{
    int best;
    double d;

    /* This callback owns the game thread.  Do not normalize a corrupt value
       with an unbounded loop: INFINITY previously never returned here. */
    if (g_sc_slot < 0 || !isfinite(g_lap_len) || g_lap_len < 100.0f ||
        g_lap_len > 100000.0f) return -1e9f;

    best = race_leader_slot();
    if (best < 0) return -1e9f;

    d = (double)V_F32(g_sc_slot, OFF_LAPDIST) - (double)V_F32(best, OFF_LAPDIST);
    if (!isfinite(d) || fabs(d) > (double)g_lap_len * 4.0) return -1e9f;
    d = fmod(d, (double)g_lap_len);
    if (d < 0.0) d += g_lap_len;
    if (d > (double)g_lap_len * 0.5) d -= (double)g_lap_len;   /* behind */
    return (float)d;
}

/* Native scoring owns lapNumber. The historical cosmetic lap-zero override
 * is preserved in v0.9, but this candidate observes native laps only. Physical
 * parking still hides the car between deployments; native on-track ordering
 * is a presentation item for the upcoming human test. */
#define TOTAL_RACE_LAPS (*(volatile int*)0x008ACF8Cu)   /* CONFIRMED, post-% */


static void sc_lap_unforce(void)
{
    /* No vehicle write: this candidate never acquires scoring state. */
    g_sc_lap_forced = 0;
}

static void sc_lap_pump(void)
{
    g_sc_lap_forced = 0;
    if (sc_slot_owned()) {
        g_sc_lap_true = V_I32(g_sc_slot, OFF_LAPNUM);
        g_sc_lap_written = g_sc_lap_true;
    }
}

/* ---- diagnostic: does the forced lap-0 (above) actually keep the safety
 * car off P1 on the LIVE running-order strip (OFF_POSITION), or only on the
 * end-of-race classification screen? The two may be computed differently
 * (classification is lap-number-first; the live strip may be closer to raw
 * lap*length+distance, in which case a car on lap 0 who has physically
 * driven a long way can still out-rank an early-race leader). Logged once
 * per 500 ms while the SC is out, plus UNTHROTTLED the instant his own
 * OFF_POSITION reads <= the real leader's, so the exact tick of any flip is
 * caught rather than averaged out. Added 2026-08-08 to settle this with a
 * live read before touching the lap-force mechanism any further. Read-only,
 * no writes - safe to leave running. */
static void sc_pos_diag(void)
{
    static DWORD last_ms = 0;
    int leader, sc_pos, ld_pos;
    DWORD now;

    if (!sc_is_out() || g_sc_slot < 0 || !slot_live(g_sc_slot)) return;

    leader = race_leader_slot();
    if (leader < 0) return;

    sc_pos = V_I32(g_sc_slot, OFF_POSITION);
    ld_pos = V_I32(leader, OFF_POSITION);
    now    = GetTickCount();

    if (sc_pos <= ld_pos || now - last_ms >= 500) {
        last_ms = now;
        logf_("  [posdiag]%s SC pos=%d lap=%d(forced) lap_true=%d dist=%.1f | "
              "leader slot=%d '%s' pos=%d lap=%d dist=%.1f",
              (sc_pos <= ld_pos) ? " *** SC AHEAD ON THE LIVE STRIP ***" : "",
              sc_pos, V_I32(g_sc_slot, OFF_LAPNUM), g_sc_lap_true,
              V_F32(g_sc_slot, OFF_LAPDIST),
              leader, V_STR(leader, OFF_NAME), ld_pos,
              V_I32(leader, OFF_LAPNUM), V_F32(leader, OFF_LAPDIST));
    }
}

static void engage(void);
static void release(const char *why);
static void restart_arm(void);

/* THE PACE IS THE SAFETY CAR'S OWN SPEED.
 *
 * Driver's call, 2026-08-01: "the pace should be determined by the speed of the
 * pace car, instead of doing a deliberate speed cap." That is also how it works
 * in reality - the safety car drives, everyone else matches it - and it makes
 * the whole thing self-tuning: change his .hdv and the caution speed changes
 * with it, with no number to pick by hand.
 *
 * He is left ungoverned while leading, so this is a measurement, not a target.
 * It is smoothed over a second because an instantaneous read would push the
 * queue's cap up and down with every corner he takes, and each car behind is
 * allowed to exceed it anyway by the slot controller's catch-up allowance -
 * that is what lets a car that has dropped back close its gap again.
 *
 * CORRECTION 2026-08-07: the 2026-08-02b gather governor in hold_pump() DOES
 * command him directly (32 m/s while the queue forms, the pace setting once
 * formed). This measurement is still not a target - it only ever pulls the
 * field's commanded speed DOWN when he is actually slower (corners, the pit
 * lane), and it is what the field matches while he runs free with the gather
 * feature off.
 *
 * g_pace_speed stays as the manual value, used before he is out and if he is
 * ever lost.                                                                */
#define SC_PACE_HIST   20              /* samples at 20 Hz = 1 s             */
#define SC_PACE_FLOOR  8               /* below the AI's own throttle floor  */
static float g_sc_pace_hist[SC_PACE_HIST];
static int   g_sc_pace_n = 0;

static void sc_pace_sample(void)
{
    if (!sc_is_out() || g_sc_slot < 0 || !slot_live(g_sc_slot)) {
        g_sc_pace_n = 0;
        return;
    }
    g_sc_pace_hist[g_sc_pace_n % SC_PACE_HIST] = V_F32(g_sc_slot, OFF_SPEED);
    g_sc_pace_n++;
}

static int sc_pace_speed(int fallback)
{
    int   i, n = (g_sc_pace_n < SC_PACE_HIST) ? g_sc_pace_n : SC_PACE_HIST;
    float sum = 0.0f;

    if (n < 4) return fallback;            /* not enough of a sample yet */
    for (i = 0; i < n; i++) {
        if(!isfinite(g_sc_pace_hist[i]) || g_sc_pace_hist[i]<0.0f || g_sc_pace_hist[i]>200.0f)return fallback;
        sum += g_sc_pace_hist[i];
    }
    sum /= (float)n;

    if (sum < (float)SC_PACE_FLOOR) return fallback;   /* he is stopped/pitting */
    {
        int v = (int)(sum + 0.5f);
        if (v == SPEED_IDLE_SENTINEL) v = 19;   /* 18 means PitSpeedLimit */
        if (v < PACE_MIN) v = PACE_MIN;
        return v;
    }
}

/* What the safety car himself should be commanded this instant: the gather
 * pace while the queue is still forming, the hold pace once it is formed or
 * the gather window has expired. See the g_gather_pace block for why.
 *
 * Returns 0 for "do not govern him at all", which is the pre-2026-08-02
 * behaviour and what a gather pace of 0 selects.                            */
static int sc_commanded_pace(void)
{
    int v;
    static double started=-1.0,last=-1.0,elapsed;
    static unsigned epoch;
    if(epoch!=g_epoch || started!=g_gather_sim || last<0.0 || g_evidence_now<last){
        epoch=g_epoch;started=g_gather_sim;last=g_evidence_now;elapsed=0.0;
    }
    double dt=g_evidence_now-last;last=g_evidence_now;
    if(!isfinite(dt)||dt>2.0)elapsed=0.0;else if(dt>0.0)elapsed+=dt;

    if (g_gather_pace <= 0) return 0;         /* feature off: he runs free   */

    v = g_pace_speed;
    if (g_caution != CAUTION_FORMED && g_gather_sim >= 0.0 && isfinite(SESSION_TIME) &&
        SESSION_TIME >= g_gather_sim && elapsed < g_gather_secs)
        v = g_gather_pace;

    if (v < PACE_MIN) v = PACE_MIN;
    if (v == SPEED_IDLE_SENTINEL) v = 19;     /* 18 substitutes PitSpeedLimit */
    return v;
}

/* Is he actually parked and available? He is only deployable once the game has
 * finished driving him to his garage - retired flag set, or pittingStatus at
 * the garaged value, or already lifted out of the field.                    */
static int sc_ready(int i)
{
    if (i < 0 || i!=g_sc_slot || !sc_slot_owned()) return 0;
    F1CVehicleLifecycle life=sc_park_lifecycle(i);
    return life.valid && life.garage_clear;
}

#include "f1c_race_distance.h"
static int sc_distance_eligible(int feedback) {
    int percent=0,cutoff=(int)g_sc_config.RaceDistanceCutoffPercent;
    int known=cutoff==0?0:f1c_race_distance_percent(sc_marker_read,&percent);
    int allowed=f1c_race_distance_above_cutoff(known,percent,cutoff);
    if(!allowed&&feedback){
        if(known){hud_show("SAFETY CAR AFTER %d%% DISTANCE",cutoff);logf_("[distance] activation refused: race=%d%% cutoff=%d%% (inclusive)",percent,cutoff);}
        else{hud_show("SAFETY CAR UNAVAILABLE");logf_("[distance] activation refused: unreadable/invalid race distance");}
    }
    return allowed;
}

static void sc_deploy(void)
{
    if(!g_engaged && !sc_distance_eligible(1))return;
    if(g_emergency_failed){hud_show("SAFETY CAR RESET FAILED");return;}
    int resume_hold = g_engaged && g_ct == CT_PACE && g_sc_state == SC_IDLE;
    /* Only a genuinely new, authorized cycle may clear a fatal prior-cycle
       latch. A restart/handover path can never erase it. */
    if(!resume_hold && !g_engaged) g_restart_fault=0;
    /* The full length by default. An automatic declaration overwrites this
     * immediately afterwards with the length its severity earned; a manual
     * one keeps it, because a driver calling for a safety car is not a
     * "quickie" by definition.                                             */
    /* Common final gate for GUI, hotkey, automatic, and queued routes. */
    if (g_rf_stand_down || !sc_action_authorized()) {
        logf_("SAFETY CAR: deployment refused (offline/live-state/RF gate)");
        hud_show("SAFETY CAR UNAVAILABLE");
        return;
    }
    /* A repeated request must not restart this caution or erase its hazards,
       including when the optional garage-readiness setting is disabled. */
    if (g_sc_state != SC_IDLE || (g_engaged && !resume_hold)) {
        logf_("[cycle] deployment refused: active caution/restart is not a cancellable no-SC hold");
        hud_show("SAFETY CAR ALREADY ACTIVE");
        return;
    }
    if (!sc_slot_owned()) { hud_show("SAFETY CAR UNAVAILABLE"); return; }

    /* REFUSE IF HE IS STILL ON HIS WAY IN.
     *
     * He starts the race from the last grid box and the game drives him to his
     * garage on lap 1 by its own retirement path. Deploying before he gets
     * there ran sc_revive() on a car that was never garaged, which clears
     * callToBox and simply puts him back to racing - driver report 2026-08-01,
     * "the safety car is currently on track racing". The log showed it plainly:
     * two DEPLOYED lines at SessionTime 29 and 65, and no SAFETY CAR HIDDEN
     * line anywhere, because he had not finished his in-lap yet.
     *
     * Skipped when parking is switched off, because then the driver has said
     * they want him racing.                                                 */
    if (g_sc_park && !sc_ready(g_sc_slot)) {
        logf_("SAFETY CAR: not ready - he is still driving to his garage "
              "(pittingStatus=%d, retired=%d). He parks himself on lap 1; "
              "deploy once the log says SAFETY CAR HIDDEN.",
              g_sc_slot >= 0 ? V_I32(g_sc_slot, OFF_PITTING) : -1,
              g_sc_slot >= 0 ? *(volatile char*)(VEH(g_sc_slot) + OFF_RETIRED) : -1);
        sc_garage_diagnostic(g_sc_slot,"deploy");
        hud_show("SAFETY CAR NOT IN PITS");
        return;
    }

    if (g_sc_slot < 0) {
        logf_("SAFETY CAR: selected vehicle is not initialized in this session");
        hud_show("SAFETY CAR UNAVAILABLE");
        return;
    }
    if (!phase_racing()) { hud_show("RACE NOT ACTIVE"); return; }
    if(!resume_hold && g_ct_final_gate && TOTAL_RACE_LAPS>0){
        int lead=race_leader_slot();
        if(lead<0 || V_I32(lead,OFF_LAPNUM)<0){
            hud_show("SAFETY CAR UNAVAILABLE");return;
        }
        if(V_I32(lead,OFF_LAPNUM)>=TOTAL_RACE_LAPS-1){
            hud_show("NO SAFETY CAR ON FINAL LAP");return;
        }
    }

    /* GUARDRAIL: never deploy while the STARTING LIGHTS are running. Through the
     * start sequence the grid is held with the brakes on and the whole field is
     * stationary; deploying then leaves the safety car pinned at brake 1.0,
     * RPM 0, lap 0 - the unrecoverable start-sequence state from earlier
     * sessions. Detect it directly: the field is not moving yet. Once the
     * lights go out and the pack accelerates away, deploy is allowed. Driver
     * spec 2026-08-08. */
    {
        int   i;
        float vmax = 0.0f;
        for (i = 0; i < VEH_SLOTS; i++) {
            float v;
            if (i == g_sc_slot || !slot_live(i)) continue;
            v = V_F32(i, OFF_SPEED);
            if (v > vmax) vmax = v;
        }
        if (vmax < START_LIGHTS_MOVING_MS) {
            logf_("SAFETY CAR: refused - the starting lights are running (the "
                  "field is still stationary, fastest car %.1f m/s). Deploy "
                  "once the race is green and the pack is moving.", vmax);
            hud_show("WAIT FOR THE RACE START");
            return;
        }
    }

    if(!sc_initial_capture()) {
        logf_("[pit-reset] deployment refused: no valid initial parked snapshot");
        hud_show("SAFETY CAR UNAVAILABLE"); return;
    }
    g_initial_park.parked=0;
    g_sc_forced_return=0;
    logf_("=== SAFETY CAR DEPLOYED === slot %d, %s", g_sc_slot,
          V_STR(g_sc_slot, OFF_NAME));

    /* HE STAYS HIDDEN UNTIL THE MOMENT HE IS RELEASED.
     *
     * This used to un-hide him here, which was harmless while the wait was a
     * fixed 5 s. It stopped being harmless when the wait became "until the
     * leader is near the line": a car that is back in the field but still
     * RETIRED has no ground collision, so it falls. Driver report 2026-08-01,
     * "the safety car is currently bugged underground" - measured at
     * Y = -13513 with a speed of 162 m/s, which is 52 s of free fall and
     * matches the length of the wait almost exactly.
     *
     * So the un-hide moved into the release branch of SC_ARMING, one tick
     * before the revive.                                                   */

    /* Slow the field FIRST, then release him - see SC_ARM_DELAY_MS. */
    if (!g_engaged) engage();
    if (!g_engaged) return;  /* rejected engagement has no deploy side effects */
    /* This point follows all deployment refusal/readiness gates. A failed
       repeat deploy cannot erase the active caution's incident ledger. */
    /* Redeploying from an explicitly cancelled no-SC hold keeps the original
       incident/weather ledger, loans and order.  A new cycle alone resets it. */
    if (!resume_hold) {
        sc_clear_reset(&g_clearance);
        g_pit_stops = 0;
    }
    g_ct_laps_run = g_ct_laps_tgt;
    g_pit_handover=0;
    g_bunch    = 1;
    g_sc_state = SC_ARMING;
    g_sc_mark  = sc_sim_millis(SESSION_TIME);
    g_sc_arm_sim=SESSION_TIME;
    g_arm_saw_far = 0;        /* latched when the leader is first seen far from the line */

    /* The caution proper: yellow flags out, lap counting armed. */
    g_ct           = CT_DEPLOY;
    g_ct_laps_done = 0;
    g_ct_lap_ref   = -1;
    g_green_said   = 0;
    memset(g_restart_free, 0, sizeof(g_restart_free));
    clearance_step(1); /* same initial ledger for automatic, weather and manual */
    sc_auto_cover_active(&g_auto_state); /* commit only after successful deployment */
    yellow_set(1);

    /* NO LINE HERE. SCAR01 says he "is exiting the pits now", and at this
     * moment he is still hidden in the garage waiting for the leader - which
     * can be the better part of a minute. It is said at the release instead. */
    strcpy(g_activation_reason,"MANUAL ACTIVATION");
    g_activation_sector=0;
    hud_show("SAFETY CAR");
}

static void restart_capture_recall(void)
{
    int lead=race_leader_slot();
    g_recall_leader=-1;g_recall_lap=-1;
    if(lead<0 || !slot_live(lead))return;
    g_recall_leader=lead;g_recall_lap=V_I32(lead,OFF_LAPNUM);
    g_recall_epoch=g_epoch;g_recall_sim=SESSION_TIME;
    memcpy(g_recall_name,V_STR(lead,OFF_NAME),sizeof(g_recall_name));
    logf_("[restart] recall baseline: leader=%d lap=%d sim=%.3f",lead,g_recall_lap,g_recall_sim);
}

static void sc_recall(int forced)
{
    if(!on_game_thread() || !sc_action_authorized() || !g_rc ||
       f1c_rc_read(&g_rc->owner)!=F1C_RC_SC)return;
    /* Cancel before departure is a transaction rollback, never a recall of a
       car that has not entered the race.  Controls and ledger return together. */
    if (g_sc_state == SC_ARMING) {
        if (!g_hidden) {
            /* The native departure may already have begun.  A phase label is
               not enough to forge a cancellation safely; keep the caution
               and let the normal deployed recall path own the return. */
            logf_("[cycle] arming cancellation refused after physical departure");
            hud_show("SAFETY CAR WAITING TO EXIT");
            return;
        }
        logf_("[cycle] arming cancelled before departure");
        /* Keep the existing restriction and incident/weather ledger alive.
           This is a cancelled deployment, never a synthetic GREEN event. */
        g_sc_state=SC_IDLE; g_sc_mark=0; g_pit_handover=0; g_sc_recall_armed=0;
        g_ct=CT_PACE;
        yellow_set(1);
        say_flush("arming cancelled");
        hud_show("DEPLOYMENT CANCELLED");
        return;
    }
    /* Recallable from an actually deployed state, including mid catch-up. */
    if (g_sc_state == SC_IDLE || g_sc_state == SC_RECALL) return;
    if(!forced && (g_weather.hold || g_weather.bad)) {
        logf_("[weather-SC] recall refused: weather hold or confirmation active");
        return;
    }
    /* Owner-authorized U/max-stint return overrides incident/weather waiting.
       Ordinary readiness-driven recall retains the affirmative-clear gate. */
    if (!forced && !clearance_recall_ok()) {
        logf_("[clearance] recall refused: recorded incident is not yet 5s affirmatively clear");
        hud_show("SAFETY CAR STAYS OUT");
        return;
    }
    g_sc_forced_return=forced!=0;
    restart_capture_recall();
    logf_("=== SAFETY CAR RECALLED === slot %d", g_sc_slot);
    /* The warning, at the start of his last lap: "safety car getting in at the
     * end of this lap, get ready". SCAR05 ("entering the pits now") follows
     * when he actually turns in, then SCAR04 ("safety car is in").        */
    sc_say_later(SCORE_SCAR03, "SCAR03 getting in at the end of this lap", 0);
    if (g_sc_slot >= 0) sc_send_to_box(g_sc_slot);
    g_sc_state  = SC_RECALL;
    g_sc_mark   = sc_sim_millis(SESSION_TIME);
    g_sc_said05 = 0;
    g_sc_recall_armed = 0;

    /* One lap to the green. The wave-around window closes here: a car released
     * now would still be alongside the queue when the green came out.       */
    g_ct = CT_IN;
    if (g_wave_policy) {
        wave_recapture_all();
        logf_("  [wave] window closed - safety car is coming in");
    }
    hud_show("SAFETY CAR IN THIS LAP");
}

/* Per-tick sequencing. Kept out of the speed controller so the pace logic
 * stays readable.                                                          */
/* Keep him retired in his garage until he is called for, enforced every tick
 * rather than once - grid formation, restarts and session changes all have to
 * see a retired car, which is exactly why sc1.c made this a DLL. Skipped if
 * the PLAYER is driving the safety car.                                    */
/* Is the active array fully built and self-consistent? Every in-field entry has
 * to be a slot-aligned pointer into the vehicle array. During a session load
 * the game is rewriting both the array and the count, and touching it in that
 * window CRASHED THE GAME on the loading screen (2026-07-31).              */
static int field_array_sane(int n)
{
    int i;
    if (n < 2 || n > VEH_SLOTS) return 0;
    for (i = 0; i < n; i++) {
        unsigned int p = VEHICLE_ARRAY[i];
        if (p < VEH_BASE) return 0;
        if ((p - VEH_BASE) % VEH_STRIDE) return 0;
        if ((p - VEH_BASE) / VEH_STRIDE >= (unsigned)VEH_SLOTS) return 0;
    }
    return 1;
}

/* Is the field built and self-consistent RIGHT NOW, whatever the race phase?
 *
 * This replaces phase_racing() as the guard on hiding. The crash that guard was
 * written for is a decremented VehicleCount held across a session CHANGE, while
 * the game rebuilds the vehicle array - and what makes that dangerous is the
 * array being rebuilt, not the phase number. A pre-race classification screen
 * has 23 fully built cars and a sane array; the menu has phase 5 left stale
 * with ZERO live cars, which is exactly what this rejects.                  */
static int field_stable(void)
{
    int i, live = 0;
    if (!field_array_sane(VEHICLE_COUNT)) return 0;
    for (i = 0; i < VEH_SLOTS; i++) {
        if (V_I32(i, OFF_ACTIVE) == -1) continue;
        if (++live >= 2) return 1;
    }
    return 0;
}

/* Stricter still: the field has been stable and UNCHANGED for a while.
 *
 * field_stable() can go true in the middle of a track load - the array is
 * momentarily self-consistent and a couple of cars exist while the game is
 * still building the rest. Hiding a car in that window CRASHED THE LOAD
 * (driver, 2026-08-01: "crashed at the end of the loading screen -> the
 * manipulation should happen ONLY when everything is loaded").
 *
 * So the pre-race hide waits for VehicleCount to stop changing and stay put
 * for SETTLE_MS. A finished grid is static; a load in progress is not.     */
#define SETTLE_MS     4000
#define FILE_QUIET_MS 3000

static DWORD g_settle_ms  = 0;
static int   g_settle_cnt = -1;

static int field_settled(void)
{
    int i, live = 0, n = VEHICLE_COUNT;

    if (!field_stable()) { g_settle_ms = 0; return 0; }

    /* THE EXACT COMPLETION SIGNAL: every counted entry is actually live.
     *
     * The loader publishes the final VehicleCount BEFORE it builds the cars,
     * so a steady count means nothing - it read 23 while Prost, Minardi,
     * Arrows and BAR were still to come, and hiding a car there crashed the
     * load twice. But a slot only goes live when its car exists, so
     * live == count is true only once the whole field is built. That is an
     * event, not a timeout, so it fires the instant loading finishes instead
     * of leaving the safety car on screen for several seconds.            */
    for (i = 0; i < VEH_SLOTS; i++)
        if (V_I32(i, OFF_ACTIVE) != -1) live++;
    if (live != n) { g_settle_ms = 0; return 0; }

    /* Belt and braces: the loader must also have stopped reading files. Kept
     * short, because live == count already did the real work.             */
    if (g_last_file_ms && GetTickCount() - g_last_file_ms < FILE_QUIET_MS) return 0;

    if (!g_settle_ms) { g_settle_ms = GetTickCount(); return 0; }
    return (GetTickCount() - g_settle_ms) >= SETTLE_MS;
}

/* ---- graphical hide (ported from mods/red_flag/gr29.c) -------------------
 *
 * Lifting a car out of the active array removes it from the standings and from
 * every field-wide system, but it does NOT move the car: the model stays
 * wherever the session placed it, because rendering follows placement, not the
 * field list. Driver report 2026-08-02, after a clean INSTA-RETIRED + HIDDEN:
 * "he's there in the grid slot".
 *
 * gr29 solves this for its DNF cars through the scene graph, and that lever
 * writes no race state at all - no VehicleCount, no position the physics
 * reads, nothing the caution logic looks at. Node entry is
 * nodeTable + 28*(nodeIdx + subIdx*dim), with nodeIdx/subIdx per car.      */
#define SCENE_STRUCT   0x005EA2C0
#define OFF_NODETABLE  344
#define OFF_SCENEDIM   356
/* CORRECTION 2026-08-03: these are NOT scene-node indices. 0x48A4 is the car's
 * AIW [PITS] TeamIndex and 0x48A8 its garage bay - verified on 15 of 15 cars
 * still parked in a garage, and confirmed at the box itself: the safety car
 * read 0x48A4 = 11 and was serviced 0.13 m from AIW team 11's PitPos. That is
 * why an earlier session found one of them "pointing at the garage object". */
#define OFF_PITTEAM    0x48A4u
#define OFF_NODEIDX    0x48A4u
#define OFF_SUBIDX     0x48A8u
#define GFX_HIDE_Y     (-1000.0f)

static int   g_gfx_hidden = 0;
static float g_gfx_saved_y = 0.0f;

static void gfx_hide(int i, int on)
{
    /* DISABLED 2026-08-02 - THE NODE INDEX IS NOT VERIFIED.
     *
     * Measured live: with the safety car's model sitting in grid box 23 at
     * (-120, 0, 1), veh+0x48A4 / veh+0x48A8 gave nodeIdx 11, subIdx 2, and the
     * node at nodeTable + 28*(nodeIdx + subIdx*dim) held (-235.2, y, -31.6) -
     * the GARAGE position, not the car's. Writing Y = -1000 there moved
     * something else out of the world and left his model untouched.
     *
     * gr29 uses this mapping for its own DNF cars and evidently gets the right
     * node, so the arithmetic or the index source is subtly different here.
     * Until it is proven by hiding a KNOWN car and watching that car vanish,
     * this must not run - hiding the wrong object is worse than not hiding.  */
    (void)i; (void)on;
    return;
#if 0
    int nodeTable = *(volatile int*)(SCENE_STRUCT + OFF_NODETABLE);
    int dim       = *(volatile int*)(SCENE_STRUCT + OFF_SCENEDIM);
    int nodeIdx, subIdx;
    volatile float *y;

    if (i < 0 || !nodeTable || !dim) return;
    nodeIdx = V_I32(i, OFF_NODEIDX);
    subIdx  = V_I32(i, OFF_SUBIDX);
    y = (volatile float*)(nodeTable + 28 * (nodeIdx + subIdx * dim) + 4);

    if (on) {
        if (g_gfx_hidden) return;
        g_gfx_saved_y = *y;
        *y = GFX_HIDE_Y;
        g_gfx_hidden = 1;
        logf_("  [gfx] safety car model moved out of sight (scene node Y "
              "%.1f -> %.1f) - he is retired in his grid box and should not "
              "be seen sitting there", g_gfx_saved_y, GFX_HIDE_Y);
    } else if (g_gfx_hidden) {
        *y = g_gfx_saved_y;
        g_gfx_hidden = 0;
        logf_("  [gfx] safety car model restored (scene node Y -> %.1f)",
              g_gfx_saved_y);
    }
#endif
}

/* FORK: Take him out of the field = the native park: role -1 + the game's own
 * active-array rebuild + count -1. The standings/classification are
 * count-bounded, so dropping the count removes him from the statistics - the
 * whole point of this fork. No array shifting: BuildActiveVehiclePointerArray
 * re-packs the array from the role flags.                                    */
static int sc_hide(int slot)
{
    int n = VEHICLE_COUNT;
    if (!on_game_thread() || !sc_slot_owned() || slot != g_sc_slot) return 0;
    if (g_hidden) return 1;

    /* Never touch the field while it is being built or torn down. Measured
     * state, not the phase number - see field_stable().                    */
    if (!field_stable()) return 0;
    if (!field_array_sane(n)) {
        logf_("  [hide] field not stable yet (count %d) - not touching it", n);
        return 0;
    }
    if (slot < 0 || slot >= VEH_SLOTS || V_I32(slot, OFF_ACTIVE) != 1) {
        logf_("  [hide] slot %d not a live extra car (role=%d) - not hiding",
              slot, (slot >= 0 && slot < VEH_SLOTS) ? V_I32(slot, OFF_ACTIVE) : -9);
        return 0;
    }

    V_I32(slot, OFF_ACTIVE) = -1;
    VEHICLE_COUNT = REBUILD();

    g_hidden = 1;
    logf_("=== SAFETY CAR HIDDEN === role -1 + rebuild, VehicleCount %d -> %d "
          "(extra-car park: out of the standings)", n, VEHICLE_COUNT);
    return 1;
}

/* FORK: Put him back = role 1 + rebuild + count +1 (the inverse of sc_hide).
 * The callers gate when this is legal: the deploy path (release branch), the
 * phase-4 pit-box hand-over and the teardown guard. If the count is
 * implausible the session was already rebuilt - drop the state without
 * writing (the load hook re-adds or re-counts the extra car at the next
 * track load).                                                              */

static void sc_unhide(void)
{
    int n, slot;

    if (!g_hidden) return;
    if (!on_game_thread() || !field_stable() || !sc_slot_owned() ||
        !field_array_sane(VEHICLE_COUNT)) return;
    slot = g_sc_slot;
    n = VEHICLE_COUNT;
    if (n < 2 || n >= VEH_SLOTS || slot < 0 || slot >= VEH_SLOTS) {
        logf_("  [hide] refuse unhide, count %d slot %d - session rebuilt, "
              "state dropped", n, slot);
        g_hidden = 0;
        return;
    }

    V_I32(slot, OFF_ACTIVE) = 1;
    VEHICLE_COUNT = REBUILD();

    g_hidden = 0;
    logf_("=== SAFETY CAR RESTORED === role 1 + rebuild, VehicleCount %d -> %d",
          n, VEHICLE_COUNT);
}

/* Put the safety car in the LAST grid box, before the grid is placed.
 *
 * He has no qualifying time, so the game sorts him to the FRONT - driver report
 * 2026-08-01: "Maylander is in the first grid slot". Placement reads
 * veh+0x4058 plannedPositionAtStart (CONFIRMED, sole reader
 * SingleFunctionDispatch_10e0 -> sub_403CD0(gridTable, plannedPos - 1, ...)),
 * and sc_park() never touched it because it returns early outside phase 5.
 *
 * NOT sc1.c's plannedPositionAtStart = 200. That is an out-of-bounds read:
 * sub_403CD0 indexes v4[7 * a2] with no bounds check, so 200 lands 1393 floats
 * past the end of the track's grid array. Every legal index is a real box.
 *
 * Instead the field is RENUMBERED: he takes the highest planned position, and
 * every car that was behind him moves up one to close the gap he leaves. The
 * set stays contiguous 1..n, so there is no collision and no index outside the
 * grid, and the other cars keep their relative order exactly.
 *
 * This writes ONLY the grid-order field. The retirement flag is deliberately
 * left alone before green: an earlier version enforced it during phase 4, so at
 * the lights he was already flagged retired, got hidden from the field while
 * still parked in his box, and never drove to the pits.
 *
 * Idempotent, and re-applied every tick until the race starts, because it is
 * not known at which point the game reads the field for placement.          */
static void sc_grid_last(int i)
{
    int j, mine, maxp = 0, live = 0;
    int pos_mine, pos_max = 0;

    /* BOTH FIELDS, because they are not the same thing and only fixing one
     * leaves him on pole.
     *
     *   0x4058 plannedPositionAtStart - what the placement path indexes the
     *          track's grid table with (sub_4D10E0 -> sub_403CD0).
     *   0x4060 race position          - what the standings and the grid you
     *          SEE are ordered by.
     *
     * The first version renumbered only 0x4058. Measured live 2026-08-01 at
     * phase 2: Maylander read 0x4058 = 23 (our write held) and 0x4060 = 1, and
     * he was still on pole - slow off the line with his safety-car stats, with
     * 22 F1 cars behind him. Driver: "Maylander is in P1 again and moves the
     * car VERY slowly and keeps getting hit."
     *
     * Both are renumbered the same way: he takes the highest value and every
     * car behind him moves up one, so each set stays contiguous 1..n with no
     * collisions and no index outside the grid table.                       */
    mine     = V_I32(i, OFF_QUALPOS);
    pos_mine = V_I32(i, OFF_POSITION);

    for (j = 0; j < VEH_SLOTS; j++) {
        int p, q;
        if (V_I32(j, OFF_ACTIVE) == -1) continue;
        live++;
        p = V_I32(j, OFF_QUALPOS);
        q = V_I32(j, OFF_POSITION);
        if (p > maxp)    maxp    = p;
        if (q > pos_max) pos_max = q;
    }
    if (live < 2) return;

    if (maxp > 0 && mine > 0 && mine < maxp) {
        for (j = 0; j < VEH_SLOTS; j++) {
            int p;
            if (j == i || V_I32(j, OFF_ACTIVE) == -1) continue;
            p = V_I32(j, OFF_QUALPOS);
            if (p > mine) V_I32(j, OFF_QUALPOS) = p - 1;
        }
        V_I32(i, OFF_QUALPOS) = maxp;
        logf_("  [grid] safety car grid box %d -> %d of %d entries", mine, maxp, live);
    }

    if (pos_max > 0 && pos_mine > 0 && pos_mine < pos_max) {
        for (j = 0; j < VEH_SLOTS; j++) {
            int q;
            if (j == i || V_I32(j, OFF_ACTIVE) == -1) continue;
            q = V_I32(j, OFF_POSITION);
            if (q > pos_mine) V_I32(j, OFF_POSITION) = q - 1;
        }
        V_I32(i, OFF_POSITION) = pos_max;
        logf_("  [grid] safety car race position %d -> %d of %d entries",
              pos_mine, pos_max, live);
    }
}

static void sc_park(int i)
{
    if (!sc_slot_owned() || i != g_sc_slot) return;
    int n = 0, k;

    /* Before the lights: grid order, then take him OUT of the field.
     *
     * The test this exists for:
     *   1. he is not in the pre-race classification;
     *   2. he therefore stays in his grid slot until the game retires him by
     *      itself and teleports him to the garage;
     *   3. Num0 de-retires him from there to lead the pack.
     *
     * He is NOT put back at green - the point is that he never drives an
     * in-lap. Gated on field_settled(): VehicleCount is published BEFORE the
     * cars are built, so the count alone is not proof the load has finished,
     * and hiding him mid-load crashed the track load. Timers here are the
     * combination that loaded cleanly.                                     */
    if (!phase_racing()) {
        /* THE RENUMBER MUST RUN EARLY, THE HIDE MUST RUN LATE.
         *
         * They were briefly gated together and that broke the grid: the game
         * PLACES the cars during the load, so a grid write that waits for the
         * load to finish arrives too late - his fields read QualPos 23 and
         * RacePos 23 while the car itself sat in box 1 (measured live, x=-10
         * where box 23 is x=-120). Driver: "he is in grid slot 1 again".
         *
         * The hide has the opposite requirement: doing it before the field is
         * fully built crashes the load. So they are split.                */
        /* RENUMBER ONLY. The pre-race hide is MANUAL, on Numpad 1.
         *
         * Every automatic trigger tried against the loader crashed the track
         * load: VehicleCount is published before the cars are built, and a
         * file-quiet window and a live == count test both still fired early.
         * The driver knows when the screen is up and the field is built, so
         * that judgement is theirs to make with a key press instead of mine
         * to guess with a timer.                                          */
        sc_grid_last(i);

        /* PUT HIM BACK THE MOMENT THE CLASSIFICATION SCREEN IS LEFT.
         *
         * The hide is only wanted for that one screen (phase 2). If he is
         * still out of the field while the game sets the start up, he comes
         * back with no racing line and drives straight off the circuit -
         * driver, 2026-08-02: "safety car kept driving straight and fell off
         * the map". Pressing Num1 by hand at "the race was starting" is
         * already too late, so it is done here instead, on the phase leaving
         * 2, which is before the lights.                                  */
        /* FORK: NO UNHIDE ON LEAVING THE CLASSIFICATION SCREEN. pace_probe
         * needed it because its SC is a grid ENTRANT that must take the
         * green in the field. The extra car must STAY out of the field
         * (role -1, count dropped) until the RACE_PHASE 4 hand-over parks
         * him in his box - putting him back at phase 3 would leave him
         * AI-driven from grid box 23 at the lights.                      */

        return;
    }

    /* NO AUTOMATIC UNHIDE. It ran every tick while racing and instantly undid
     * the manual hide, which then let the racing branch below arm callToBox so
     * he drove off - log showed MANUAL hide and "back in the field" alternating
     * (driver: "HE IS DRIVING NOW AGAIN HE IS NOT BEING RETIRED BY THE GAME").
     *
     * Numpad 1 is a straight toggle. The sequence is the driver's to run:
     * hide before the lights, then unhide once the race is running - he is in
     * grid-hold by then, cannot drive, and the game retires him to its garage,
     * which is the state Num0 revives from.                                */

    /* INSTANT RETIREMENT AT THE LIGHTS - no in-lap, no driving.
     *
     * Driver's spec: "we want him insta-retired and NOT driving a lap." The
     * game's own trigger (rear wing flag + callToBox) makes him DRIVE to the
     * pits, which is the lap we do not want. tools/deretire_probe.py's
     * synthretire forges the end state instead and freezes the car WHERE IT
     * STANDS - no garage teleport, no driving.
     *
     * The delay is the whole trick. He must be in the field when the lights go
     * out so the START SEQUENCE RELEASES HIS BRAKE HOLD; a car that is absent
     * at that moment is left with brake = 1.000 for ever and no de-retirement
     * recipe recovers it (measured 2026-08-02, and the same signature as the
     * 2026-07-31 grid-hold finding). So: let the start release him, wait a
     * moment, then freeze and hide him.
     *
     * He ends up parked in his grid box rather than the garage, which is fine
     * - hidden cars are not simulated and not drawn, and Num0 revives him
     * right by the start/finish line, in front of the leader.             */
    if (g_sc_insta && !g_hidden &&
        *(volatile char*)(VEH(i) + OFF_RETIRED) == 0) {
        if (!g_green_ms) g_green_ms = sc_sim_millis(SESSION_TIME);
        else if (sc_sim_millis(SESSION_TIME) - g_green_ms > SC_INSTA_DELAY_MS) {
            /* Retirement flags ONLY, and nothing else this tick.
             *
             * The first version also wrote DriverModelVisible=0 and hid him in
             * the same tick, and the game crashed at the lights. Prior art says
             * the driver-model hide must run AFTER the teardown, not with it,
             * and lifting a car out of the field in the same breath as
             * retiring it gives the game no tick to settle. The garaged branch
             * below hides him on a later pass, which is the path that has
             * always worked.                                              */
            V_I32(i, OFF_FINISH)                    = 2;   /* DNF */
            *(volatile char*)(VEH(i) + OFF_RETIRED) = 1;
            V_I32(i, OFF_PITSTATE)                  = 2;
            logf_("  [sc] INSTA-RETIRED on the grid %d ms after the lights - "
                  "no in-lap driven", SC_INSTA_DELAY_MS);
            gfx_hide(i, 1);
            return;
        }
        return;                       /* hands off until the start releases him */
    }


    /* NEVER touch him outside green-flag racing.
     *
     * An earlier version had a catch-all branch that enforced the retired flag
     * whenever the game was not racing. It fired during phase 4, before the
     * lights - so at green he was ALREADY flagged retired, the "he is garaged"
     * branch below took over, and he was hidden from the field while still
     * parked in his grid box, never having driven to the pits. Driver report
     * 2026-07-31: "Bernd was still in his race grid". The pre-green branch at
     * the top of this function writes the grid ORDER and nothing else.      */

    /* ORDER MATTERS, and it is the opposite of the first attempt.
     *
     * Hiding him from the field does NOT move him: the model stays wherever the
     * session placed him, because rendering follows placement, not the field
     * list. So he must reach his garage FIRST, by the game's own retirement
     * path, and only then be lifted out of the field.
     *
     * The first version hid him immediately and kept the hidden state across a
     * session change; the loader then rebuilt the field on top of a decremented
     * VehicleCount and the game CRASHED on the loading screen (2026-07-31).
     * sc_hide() now refuses to touch anything outside green-flag racing, and
     * sc_unhide() detects a rebuilt session and drops the state without
     * writing.                                                             */
    F1CVehicleLifecycle park_life=sc_park_lifecycle(i);
    if (park_life.valid && park_life.garage_clear) {
        if (g_hide_mode && sc_hide(i)) return;   /* garaged: now take him out */
    }

    /* PROVEN 2026-07-31: retiring him does NOT keep him off the grid. With this
     * DLL loaded before the field even existed - retired flag set, AI control
     * off, driver model hidden, planned position last - he was still placed in
     * grid box 23 at X=-23.9 Z=429.6, directly behind the player. F1C places
     * every entry from the entry list; retirement controls state, not
     * placement. sc1.c's plannedPositionAtStart=200 dodged it only by indexing
     * 1393 floats off the end of the grid table.
     *
     * So he is not forced to stay put. Once the race is running he is handed
     * to the game's OWN retirement path - arm veh+0xD58 = 0 plus callToBox = 1
     * and the game drives him down the pit lane and parks him in his garage,
     * teleport included (findings.jsonl, FACT). He runs from the last grid box
     * to the pit entry and disappears, which reads as a car pitting on lap 1,
     * and he ends up exactly where the deploy sequence expects him: in his
     * garage, revivable, so "exiting the pits" is the truth.               */

    if (phase_racing() &&
        *(volatile char*)(VEH(i) + OFF_RETIRED) == 0 &&
        V_I32(i, OFF_PITTING) < 4) {

        /* DO NOT ARM HIM UNTIL HE IS ACTUALLY ROLLING.
         *
         * This used to fire on the first racing tick, which is while the start
         * sequence still holds the grid brakes. Flagging him for the pits in
         * that window left him pinned at brake = 1.000, RPM 0, lap 0, in his
         * box - and with the DLL ejected he raced away perfectly, which is
         * what identified it (driver, 2026-08-02: "he keeps driving around now
         * like racing").
         *
         * Waiting for real speed costs nothing: he only has to reach the pit
         * entry at some point during lap 1.                                */
        if (V_F32(i, OFF_SPEED) < 3.0f) return;

        *(volatile char*)(VEH(i) + OFF_RWFLAG) = 0;   /* arm the trigger */
        V_I32(i, OFF_CALLBOX)                  = 1;

        /* He is driving himself in, but he is still an AI car with a racing
         * brain, and he starts from the LAST grid box - so left alone he
         * spends his in-lap overtaking the field. Take his appetite for
         * passing away for the trip. His own values are captured once so
         * sc_revive() can hand them straight back.                         */
        if (!g_sc_ai_saved) {
            g_saved_pass[i]  = V_F32(i, OFF_PASSING);
            g_saved_aggro[i] = V_F32(i, OFF_AGGRO);
            g_sc_ai_saved    = 1;
            logf_("  [sc] driving to the garage - overtaking suppressed "
                  "(passing=%.2f aggro left at %.2f)",
                  g_saved_pass[i], V_F32(i, OFF_AGGRO));
        }
        sc_ai_calm(i);

        /* HIDE HIM FROM THE STANDINGS FOR THE IN-LAP TOO, IF IT IS SAFE.
         *
         * Driver's ask, 2026-08-01: he should not appear in the standings at
         * the race start. Lifting him out of the active array is the same
         * lever that already hides him once he is garaged - it is only ever
         * applied late, so he is listed for the whole of lap 1.
         *
         * The risk is that the game may stop DRIVING a car it no longer
         * counts. It certainly keeps simulating one - a hidden car still falls
         * - but "still driven by the AI" was never tested, and if it is not he
         * would sit on the grid all race with nothing to bring him back.
         *
         * So this proves itself instead of assuming: he is hidden once he is
         * actually rolling, his lap distance is watched, and if it stops
         * advancing for SC_INLAP_STALL_MS he is put straight back into the
         * field and the mode disables itself for the session. Either way the
         * race is not damaged, and the log says which way it went.        */
        if (g_hide_inlap && !g_hidden && V_F32(i, OFF_SPEED) > 3.0f) {
            if (sc_hide(i)) {
                g_inlap_dist = V_F32(i, OFF_LAPDIST);
                g_inlap_mark = sc_sim_millis(SESSION_TIME);
                logf_("  [sc] hidden from the standings for the in-lap - "
                      "watching whether the AI still drives him");
            }
        }
        if (g_hide_inlap && g_hidden) {
            float now_d = V_F32(i, OFF_LAPDIST);
            if (now_d > g_inlap_dist + 1.0f) {         /* still moving: fine */
                g_inlap_dist = now_d;
                g_inlap_mark = sc_sim_millis(SESSION_TIME);
            } else if (sc_sim_millis(SESSION_TIME) - g_inlap_mark > SC_INLAP_STALL_MS) {
                logf_("  [sc] hidden car STOPPED being driven (lap distance "
                      "stuck at %.0f for %d ms) - putting him back in the "
                      "field and leaving him visible for the in-lap",
                      now_d, SC_INLAP_STALL_MS);
                sc_unhide();
                g_hide_inlap = 0;      /* proven not to work; do not retry */
            }
        }
        return;                                       /* let him drive there */
    }

    /* Retirement/pit-request alone is not proof of garage arrival. A wreck on
       the circuit must never be hidden and labelled ready by this path. */
    if(!park_life.valid || !park_life.garage_clear) {
        sc_garage_diagnostic(i,"park");
        hud_show("SAFETY CAR NOT IN PITS");return;
    }
    /* Garaged: hold him quiet until he is called for. */
    V_I32(i, OFF_FINISH)                     = 2;
    *(volatile char*)(VEH(i) + OFF_RETIRED)  = 1;
    *(volatile char*)(VEH(i) + OFF_AICTRL)   = 0;
    V_I32(i, OFF_PITSTATE)                   = 2;
    *(volatile char*)(VEH(i) + OFF_DRIVERVIS)= 0;
    /* A retired car holding its qualifying position shows as LEADING the race
     * from inside its garage (sc1.c, live). Park him last instead.        */
    for (k = 0; k < VEH_SLOTS; k++) if (slot_live(k)) n++;
    if (n > 0) V_I32(i, OFF_POSITION) = n;

    /* Send him to the BACK of the grid ordering, not out of it.
     *
     * sc1.c used plannedPositionAtStart = 200 to push him off the grid. That is
     * an out-of-bounds write in disguise: the placement path is
     * sub_4D10E0 -> sub_403CD0(gridTable, plannedPos - 1, ...), and sub_403CD0
     * indexes v4[7 * a2] with NO bounds check, then walks BACKWARDS by 2 while
     * it finds all-zero entries. plannedPos 200 therefore reads 1393 floats
     * past the end of the grid array and places the car from whatever it finds.
     * That is very likely what sc1.c saw as "blanks the pre-race grid listing".
     *
     * The last live index is in bounds, deterministic, and puts him behind
     * everyone, where a parked car blocks nobody. The real fix for him being on
     * the grid at all is TIMING - the parking has to run before the grid forms
     * (sc1.c: "the window between the driver structs existing and the grid
     * being placed"), which means injecting before the race loads.          */
    if (n > 0) V_I32(i, OFF_QUALPOS) = n;
}

#include "f1c_pit_marker.h"
static int sc_marker_read(uintptr_t address,void *out,size_t bytes){
    return sc_read_scope_copy(address,out,bytes);
}

/* SDK wrapper carries the reviewed RF v76 state/offline reads. A malformed or
 * unreadable snapshot is deliberately not permission to mutate game state. */
static F1CSessionGuard sc_session_guard(void){return f1c_session_guard_read(sc_marker_read);}
static int sc_offline_authorized(void){F1CSessionGuard g=sc_session_guard();return g.valid&&g.offline;}
static int sc_action_authorized(void){F1CSessionGuard g=sc_session_guard();return g.valid&&g.offline&&g.settled&&!sc_redflag_veto();}
static int sc_block_native_pit(uintptr_t vehicle,unsigned request) {
    return request==1 && on_game_thread() && !g_stop_requested &&
        sc_action_authorized() && g_engaged && sc_slot_owned() &&
        vehicle==(uintptr_t)VEH(g_sc_slot) && g_rc &&
        f1c_rc_read(&g_rc->owner)==F1C_RC_SC &&
        g_sc_state>=SC_OUTLAP && g_sc_state<=SC_ONTRACK;
}
static SCEngineStopWatch g_engine_stop_watch[VEH_SLOTS];
static void sc_inverted_engine_pump(void) {
    if(!on_game_thread()||g_stop_requested||g_emergency_inhibit||!field_stable()||
       !phase_racing()||!sc_action_authorized()) {
        memset(g_engine_stop_watch,0,sizeof(g_engine_stop_watch));return;
    }
    for(int i=1;i<VEH_SLOTS;i++) {
        F1CVehicleLifecycle life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
        F1COrientation orientation=f1c_orientation_vehicle((void*)VEH(i));
        float speed=V_F32(i,OFF_SPEED);
        int eligible=i!=g_sc_slot&&slot_live(i)&&!pit_owned(i)&&life.valid&&
            life.role==1&&life.finish==0&&!life.retired&&orientation.valid&&
            orientation.up_dot<=g_sc_config.InvertedUpDotMax&&isfinite(speed)&&
            speed>=0&&speed<=g_sc_config.InvertedMaxSpeed;
        unsigned identity=2166136261u;
        for(int k=0;k<31&&V_STR(i,OFF_NAME)[k];k++)identity=(identity^(unsigned char)V_STR(i,OFF_NAME)[k])*16777619u;
        identity=(identity^(unsigned)V_I32(i,OFF_ROSTER))*16777619u;
        if(sc_engine_stop_confirm(&g_engine_stop_watch[i],g_epoch,identity,SESSION_TIME,
               eligible,g_sc_config.InvertedConfirmSeconds)&&
           f1c_ai_engine_stop(sc_marker_read,(uintptr_t)VEH(i),g_game_thread,1)&&
           !g_engine_stop_watch[i].announced) {
            g_engine_stop_watch[i].announced=1;
            logf_("[inverted-engine] slot=%d confirmed=%.2fs; native engine off; retirement remains game-owned",i,g_engine_stop_watch[i].elapsed);
        }
    }
}

static void sc_handover_pump(void){
    float marker[3],car[3];static unsigned warned_epoch;
    if(g_pit_handover||g_sc_state!=SC_RECALL||!g_engaged||
       !phase_racing()||!sc_slot_owned())return;
    car[0]=V_F32(g_sc_slot,OFF_BODYX);car[1]=V_F32(g_sc_slot,OFF_BODYY);car[2]=V_F32(g_sc_slot,OFF_BODYZ);
    int known=f1c_pit_entry_marker(STEWARD,sc_marker_read,marker);
    int in_range=known && f1c_pit_within_100(car,marker);
    if(!known && warned_epoch!=g_epoch){
        warned_epoch=g_epoch;
        logf_("[handover] native pit-entry node invalid/unreadable; handover awaits valid XPITIN or pit ownership");
    }
    if(!in_range && !pit_owned(g_sc_slot))return;
    logf_("[handover] marker=%d distance=%.1fm SC=(%.1f,%.1f,%.1f)",known,
          known?sqrtf((car[0]-marker[0])*(car[0]-marker[0])+(car[2]-marker[2])*(car[2]-marker[2])):-1.0f,
          car[0],car[1],car[2]);
    if(!g_sc_said05){g_sc_said05=1;sc_say_later(SCORE_SCAR05,"SCAR05 entering the pits now",0);}
    g_pit_handover=1;g_bunch=0;
    g_leader_slot=race_leader_slot();
    memset(g_fallback,0,sizeof(g_fallback));memset(g_catchup,0,sizeof(g_catchup));
    sc_order_reset(&g_order_recovery); /* remove obsolete SC-relative yielding */
    restart_arm(); /* XPITIN handover, not a later pit-state change, arms GREEN. */
    logf_("[handover] %s: normal AI controls restored; leader crossing still announces GREEN",
          in_range?"within 100m of native pit-entry marker":"native pit ownership");
}

/* Player no-overtaking is separate from the AI pace controller. It observes
 * physical passes and asks native scoring for sanctions only after the full
 * give-back interval. Unknown health/identity/lap data vetoes a target. */
static SCNPState g_np_state;
static F1CRacePenaltyTicket g_np_ticket;
static int g_np_ticket_active,g_np_native_fault,g_np_ticket_slot;
static int g_np_investigation_active,g_np_prompt_active,g_np_warning_active,g_np_rules_off;
static double g_np_rules_off_until,g_np_sanction_until;
static double g_np_investigation_visible_since=-1.0;
static double g_np_ticket_since,g_np_warning_visible_since=-1.0;
static SCNPAction g_np_warning_action;
static uint64_t g_np_warning_target;
static unsigned g_np_epoch;
static double g_np_diag_time=-1.0;
static double g_np_diag_refusal_time=-1.0;
static int g_np_diag_rank=-1,g_np_diag_countdown;
static int sc_np_native_kind(SCNPAction action) {
    if(action==SC_NP_STOP_GO)return F1C_RACE_PENALTY_STOP_GO;
    if(action==SC_NP_DISQUALIFY)return F1C_RACE_PENALTY_DQ;
    return -1;
}
static void sc_np_set_display(SCNPAction action) {
    SCNPDisplay d=sc_np_display(action,g_np_prompt_active,
                               g_np_warning_active,g_np_investigation_active,
                               g_np_rules_off);
    g_np_hud_text[0]=g_np_hud_detail[0]=0;
    if(d.primary)strcpy(g_np_hud_text,d.primary);
    if(d.secondary)strcpy(g_np_hud_detail,d.secondary);
}
typedef struct {
    uint64_t identity;
    float distance;
    double time,last_good_time;
    int valid,healthy;
} SCNPTravel;
static SCNPTravel g_np_travel[VEH_SLOTS];

static uint64_t sc_np_live_identity(int i) {
    uint32_t h=2166136261u;
    int roster=V_I32(i,OFF_ROSTER),driver=V_I32(i,F1C_OFF_DriverID);
    if(roster<0||driver<0)return 0;
    for(int k=0;k<31&&V_STR(i,OFF_NAME)[k];k++)
        h=(h^(unsigned char)V_STR(i,OFF_NAME)[k])*16777619u;
    h=(h^(unsigned)driver)*16777619u;
    return ((uint64_t)(i+1)<<56)|((uint64_t)(unsigned)(roster+1)<<32)|h;
}
static int sc_np_local_gap(float player,float target,double *gap) {
    double d;
    if(!gap||!isfinite(player)||!isfinite(target)||!isfinite(g_lap_len)||
       g_lap_len<100.0f||player<0||target<0||
       player>=g_lap_len||target>=g_lap_len)return 0;
    d=(double)player-(double)target;
    if(d>g_lap_len/2)d-=g_lap_len;
    if(d<-g_lap_len/2)d+=g_lap_len;
    *gap=d;return 1;
}
/* A moving car whose lap distance is not advancing with its actual speed may
 * be spun, sliding, reversing or otherwise distressed. Suppress enforcement
 * against it until two coherent forward-motion samples have been seen. */
static int sc_np_forward_motion(int slot,uint64_t identity,float distance,float speed,double now) {
    SCNPTravel *t=&g_np_travel[slot];
    double dt,delta,rate;
    if(!identity||!isfinite(distance)||!isfinite(speed)||!isfinite(now)||
       !isfinite(g_lap_len)||g_lap_len<100||distance<0||distance>=g_lap_len||
       speed<0||speed>200){memset(t,0,sizeof *t);return 0;}
    if(!t->valid||t->identity!=identity||now<=t->time||now-t->time>0.5){
        t->valid=1;t->identity=identity;t->distance=distance;t->time=now;
        t->healthy=0;t->last_good_time=0;return 0;
    }
    dt=now-t->time;
    delta=(double)distance-(double)t->distance;
    if(delta>g_lap_len/2)delta-=g_lap_len;
    if(delta<-g_lap_len/2)delta+=g_lap_len;
    /* The game can repeat a lap-distance value on several callback ticks.
     * Keep the last movement baseline so the next jump is measured over the
     * full interval, and hold recent proven forward motion briefly. */
    if(fabs(delta)<0.05){
        return t->healthy && now-t->last_good_time<=0.25;
    }
    rate=delta/dt;
    t->distance=distance;t->time=now;
    t->healthy=rate>=fmax(3.0,0.45*(double)speed)&&
               rate<=2.5*(double)speed+10.0;
    if(t->healthy)t->last_good_time=now;
    return t->healthy;
}
static void sc_np_live_pump(void) {
    SCNPTarget targets[VEH_SLOTS];
    SCNPOutput out;
    int player=-1,active=0;
    memset(targets,0,sizeof targets);
    if(g_np_epoch!=g_epoch){
        g_np_epoch=g_epoch;sc_np_reset(&g_np_state,g_epoch);
        g_np_ticket_active=g_np_native_fault=0;
        g_np_warning_action=SC_NP_NONE;g_np_warning_target=0;
        g_np_warning_visible_since=-1.0;g_np_hud_text[0]=0;
        g_np_hud_detail[0]=0;g_np_investigation_active=0;
        g_np_prompt_active=g_np_warning_active=g_np_rules_off=0;
        g_np_investigation_visible_since=-1.0;
        g_np_rules_off_until=g_np_sanction_until=0;
        g_np_diag_time=-1.0;g_np_diag_refusal_time=-1.0;
        g_np_diag_rank=-1;g_np_diag_countdown=0;
        memset(g_np_travel,0,sizeof g_np_travel);
    }
    if(on_game_thread()&&!g_stop_requested&&!g_emergency_inhibit&&
       field_stable()&&phase_racing()&&sc_action_authorized()&&
       g_engaged&&g_ct>=CT_DEPLOY&&g_ct<=CT_IN&&!g_pit_handover&&
       isfinite(SESSION_TIME)&&g_lap_len>=100.0f) {
        for(int i=0;i<VEH_SLOTS;i++)if(slot_live(i)){
            F1CVehicleLifecycle life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
            if(life.valid&&life.role==0&&life.finish==0&&!life.retired){
                if(player>=0){player=-1;break;}
                player=i;
            }
        }
        if(player>=0&&!pit_owned(player)&&
           isfinite(V_F32(player,OFF_LAPDIST))&&
           V_I32(player,OFF_LAPNUM)>=0)active=1;
    }
    if(g_np_rules_off){
        if(active&&
           !*(volatile int32_t*)(uintptr_t)F1C_RACE_PENALTIES_ENABLED_ADDRESS){
            g_np_hud_text[0]=g_np_hud_detail[0]=0;
            if(SESSION_TIME<g_np_rules_off_until)
                sc_np_set_display(SC_NP_NONE);
            return; /* flag rules are off: no native request, even after grace */
        }
        g_np_rules_off=0;
        g_np_investigation_active=g_np_prompt_active=g_np_warning_active=0;
        g_np_investigation_visible_since=-1.0;
        sc_np_reset(&g_np_state,g_epoch); /* re-arm only for new observed passes */
    }
    if(active){
        const float pd=V_F32(player,OFF_LAPDIST);
        const int pl=V_I32(player,OFF_LAPNUM);
        for(int i=0;i<VEH_SLOTS;i++)if(i!=player&&slot_live(i)){
            F1CVehicleLifecycle life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
            F1COrientation orient=f1c_orientation_vehicle((void*)VEH(i));
            SCNPTargetStatus st={0};
            double gap=0;
            int sc=i==g_sc_slot&&sc_slot_owned();
            if(!life.valid||(!sc&&life.role!=1)||
               !sc_np_local_gap(pd,V_F32(i,OFF_LAPDIST),&gap))continue;
            st.identity_valid=1;st.active=life.finish==0&&!life.retired;
            st.lap_relation_valid=sc||
                (V_I32(i,OFF_LAPNUM)>=0&&V_I32(i,OFF_LAPNUM)<100000&&pl<100000);
            st.lapped=!sc&&V_I32(i,OFF_LAPNUM)<pl;
            st.pit_called=V_I32(i,OFF_CALLBOX)!=0;
            st.pit_state=V_I32(i,OFF_PITTING);
            st.upright_valid=orient.valid;st.upright=orient.state==1;
            st.spun=orient.valid&&orient.state!=1;
            st.damaged=V_I32(i,OFF_CARCOND)!=0;
            st.incident=V_I32(i,OFF_REASON)!=0;
            st.off_track=*(volatile unsigned char*)(VEH(i)+0x40E8u)!=0;
            st.speed_metres_per_second=V_F32(i,OFF_SPEED);
            st.is_safety_car=sc;st.safety_car_xpitin=g_pit_handover;
            targets[i].identity=sc_np_live_identity(i);
            targets[i].signed_gap_metres=gap;
            int forward=sc_np_forward_motion(i,targets[i].identity,
                V_F32(i,OFF_LAPDIST),V_F32(i,OFF_SPEED),SESSION_TIME);
            targets[i].eligible=sc_np_target_eligible(&st)&&forward;
        }
    }
    if(active&&(g_np_diag_time<0||SESSION_TIME-g_np_diag_time>=1.0)){
        int identities=0,eligible=0;
        for(int i=0;i<VEH_SLOTS;i++){
            identities+=targets[i].identity!=0;
            eligible+=targets[i].eligible!=0;
        }
        logf_("[no-pass-sample] sim=%.2f player=%d rank=%d ct=%d candidates=%d eligible=%d",
              SESSION_TIME,player,V_I32(player,OFF_POSITION),g_ct,identities,eligible);
        g_np_diag_time=SESSION_TIME;
    }
    if(active&&V_I32(player,OFF_POSITION)!=g_np_diag_rank){
        logf_("[no-pass-rank] sim=%.2f player=%d rank=%d previous=%d",
              SESSION_TIME,player,V_I32(player,OFF_POSITION),g_np_diag_rank);
        /* Private observation only: rank changes can be pit passes, retirements
         * or scoring reorderings. Record nearby eligibility without changing
         * the policy decision or exposing this detail in the player's HUD. */
        for(int i=0;i<VEH_SLOTS;i++)if(i!=player&&slot_live(i)&&
           abs(V_I32(i,OFF_POSITION)-V_I32(player,OFF_POSITION))<=3){
            logf_("[no-pass-near] sim=%.2f slot=%d rank=%d eligible=%d pit_call=%d pit_state=%d speed=%.1f damage=%d reason=%d offtrack=%d motion=%d",
                  SESSION_TIME,i,V_I32(i,OFF_POSITION),targets[i].eligible,
                  V_I32(i,OFF_CALLBOX),V_I32(i,OFF_PITTING),
                  V_F32(i,OFF_SPEED),V_I32(i,OFF_CARCOND),
                  V_I32(i,OFF_REASON),
                  *(volatile unsigned char*)(VEH(i)+0x40E8u)!=0,
                  g_np_travel[i].healthy);
        }
        g_np_diag_rank=V_I32(player,OFF_POSITION);
    }
    if(!sc_np_step(&g_np_state,g_epoch,SESSION_TIME,active,
                   targets,VEH_SLOTS,&out)){
        g_np_hud_text[0]=g_np_hud_detail[0]=0;return;
    }
    if(!active){
        g_np_hud_text[0]=g_np_hud_detail[0]=0;g_np_ticket_active=0;
        g_np_investigation_active=g_np_prompt_active=g_np_warning_active=0;
        g_np_investigation_visible_since=-1.0;
        g_np_sanction_until=0;
        g_np_warning_action=SC_NP_NONE;g_np_warning_visible_since=-1.0;
        return;
    }
    if(g_np_ticket_active){
        F1CRacePenaltyResult r=f1c_race_penalty_observe(&g_np_ticket,
                                                        g_np_ticket_slot,g_game_thread);
        if(r==F1C_RACE_PENALTY_STOP_GO||r==F1C_RACE_PENALTY_DQ){
            SCNPAction action=r==F1C_RACE_PENALTY_STOP_GO?SC_NP_STOP_GO:SC_NP_DISQUALIFY;
            if(sc_np_ack(&g_np_state,action,g_np_state.awaiting_target,1)){
                g_np_investigation_active=g_np_prompt_active=g_np_warning_active=0;
                g_np_investigation_visible_since=-1.0;
                g_np_sanction_until=SESSION_TIME+2.0;
                hud_show(action==SC_NP_STOP_GO?"10 SECOND STOP/GO PENALTY":
                         "DISQUALIFIED:");
                sc_say_eid_later(action==SC_NP_STOP_GO?156:149,
                    action==SC_NP_STOP_GO?"PWS_PENALTY03":"RE_PD_DQ",0);
                logf_("[no-pass] native sanction accepted sim=%.2f action=%d violations=%d",
                      SESSION_TIME,action,g_np_state.violations);
            }
            g_np_ticket_active=0;
        }else if(r==F1C_RACE_PENALTY_REFUSED||
                 SESSION_TIME-g_np_ticket_since>5.0){
            g_np_native_fault=1;g_np_ticket_active=0;
            logf_("[no-pass] native penalty acceptance unavailable; enforcement frozen for this race");
        }
    }
    g_np_hud_text[0]=g_np_hud_detail[0]=0;
    if(out.action==SC_NP_WARNING_1||out.action==SC_NP_WARNING_2){
        SCNPDisplay d=sc_np_display(out.action,g_np_prompt_active,
                                   g_np_warning_active,0,0);
        const char *message=d.primary;
        if(g_np_warning_action!=out.action||g_np_warning_target!=out.action_target){
            g_np_warning_action=out.action;g_np_warning_target=out.action_target;
            g_np_warning_visible_since=-1.0;++g_np_hud_generation;
        }
        sc_np_set_display(out.action);
        if(g_np_hud_rendered_generation==g_np_hud_generation&&
           hud_owned_line_visible(1,message)&&
           (!d.secondary||hud_owned_line_visible(2,d.secondary))){
            if(g_np_warning_visible_since<0)g_np_warning_visible_since=SESSION_TIME;
            if(SESSION_TIME-g_np_warning_visible_since>=1.5 &&
               sc_np_ack(&g_np_state,out.action,out.action_target,1)){
                logf_("[no-pass] warning acknowledged sim=%.2f action=%d violations=%d target=%llu",
                      SESSION_TIME,out.action,g_np_state.violations,
                      (unsigned long long)out.action_target);
                if(out.action==SC_NP_WARNING_1)g_np_prompt_active=1;
                if(out.action==SC_NP_WARNING_2){
                    g_np_prompt_active=0;
                    g_np_warning_active=1;
                }
            }
        }else g_np_warning_visible_since=-1.0;
        return;
    }
    g_np_warning_action=SC_NP_NONE;g_np_warning_target=0;
    g_np_warning_visible_since=-1.0;
    int held=0;
    for(int i=0;i<SC_NP_MAX_TARGETS;i++)held+=g_np_state.pair[i].pending!=0;
    if(!held&&!g_np_state.awaiting&&!g_np_ticket_active){
        g_np_investigation_active=g_np_prompt_active=g_np_warning_active=0;
        g_np_investigation_visible_since=-1.0;
    }
    if(out.action==SC_NP_STOP_GO&&g_np_state.awaiting==out.action&&
       !g_np_investigation_active&&
       !g_np_ticket_active&&!g_np_native_fault){
        g_np_warning_active=g_np_prompt_active=0;
        if(!*(volatile int32_t*)(uintptr_t)F1C_RACE_PENALTIES_ENABLED_ADDRESS){
            g_np_rules_off=1;g_np_rules_off_until=SESSION_TIME+5.0;
            logf_("[no-pass] native flag rules off; no sanction will be requested");
            sc_np_set_display(SC_NP_NONE);
            return;
        }
        g_np_investigation_active=1;
        g_np_investigation_visible_since=-1.0;
        ++g_np_hud_generation;
        logf_("[no-pass] investigation pending visible HUD sim=%.2f",SESSION_TIME);
    }
    if(g_np_investigation_active&&!g_np_ticket_active&&
       !*(volatile int32_t*)(uintptr_t)F1C_RACE_PENALTIES_ENABLED_ADDRESS){
        g_np_investigation_active=0;g_np_rules_off=1;
        g_np_investigation_visible_since=-1.0;
        g_np_rules_off_until=SESSION_TIME+5.0;
        sc_np_set_display(SC_NP_NONE);
        return;
    }
    if(SESSION_TIME<g_np_sanction_until){
        g_np_diag_countdown=0;
        return; /* allow the accepted native sanction message to be seen */
    }
    if(g_np_state.violations>=3&&held&&!g_np_investigation_active)
        g_np_prompt_active=1;
    sc_np_set_display(out.action);
    if(g_np_investigation_active&&g_np_investigation_visible_since<0&&
       g_np_hud_rendered_generation==g_np_hud_generation&&
       hud_owned_line_visible(1,"UNDER INVESTIGATION")){
        g_np_investigation_visible_since=SESSION_TIME;
        sc_say_eid_later(146,"PWS_MISC04",0);
    }
    if(out.give_back_visible){
        if(!g_np_diag_countdown)
            logf_("[no-pass] silent grace started sim=%.2f target=%llu",SESSION_TIME,
                  (unsigned long long)out.give_back_target);
    }
    g_np_diag_countdown=out.give_back_visible;
    if((out.action==SC_NP_STOP_GO||out.action==SC_NP_DISQUALIFY)&&
       g_np_state.awaiting==out.action&&
       (out.action!=SC_NP_STOP_GO||
        (g_np_investigation_visible_since>=0&&
         SESSION_TIME-g_np_investigation_visible_since>=2.0))&&
       !g_np_ticket_active&&!g_np_native_fault){
        int kind=sc_np_native_kind(out.action);
        F1CRacePenaltyResult r=f1c_race_penalty_issue(player,g_game_thread,
            active&&g_np_state.awaiting==out.action,kind,
            V_I32(player,F1C_OFF_DriverID),V_I32(player,OFF_ROSTER),&g_np_ticket);
        if(r==F1C_RACE_PENALTY_QUEUED||r==F1C_RACE_PENALTY_UNCERTAIN){
            sc_np_mark_native_in_flight(&g_np_state,out.action,out.action_target);
            g_np_ticket_active=1;g_np_ticket_slot=player;g_np_ticket_since=SESSION_TIME;
            logf_("[no-pass] native sanction queued action=%d native_kind=%d slot=%d",
                  out.action,kind,player);
        }else if(r==F1C_RACE_PENALTY_REFUSED&&
                 (g_np_diag_refusal_time<0||SESSION_TIME-g_np_diag_refusal_time>=1.0)){
            uintptr_t self=*(volatile uintptr_t*)(VEH(player)+F1C_RACE_PENALTY_SCORE_OFFSET);
            logf_("[no-pass] native sanction refused sim=%.2f action=%d kind=%d enabled=%d thread=%d code=%d self=%d finish=%d events=%d",
                  SESSION_TIME,out.action,kind,
                  *(volatile int32_t*)(uintptr_t)F1C_RACE_PENALTIES_ENABLED_ADDRESS,
                  on_game_thread(),f1c_race_penalty_code_valid(
                      (const void*)F1C_RACE_STOP_GO_ADDRESS,(const void*)F1C_RACE_DQ_ADDRESS),
                  self==(uintptr_t)VEH(player),V_I32(player,F1C_OFF_FinishState),
                  *(volatile int32_t*)(uintptr_t)F1C_RACE_EVENT_COUNT_ADDRESS);
            g_np_diag_refusal_time=SESSION_TIME;
        }
    }
}

#include "sc_progress_watch.h"
static SCProgressWatch g_sc_progress;
static void sc_request_fault(int reason) {
    if(!on_game_thread()||!g_rc||!phase_racing()||!sc_offline_authorized()||
       (!g_engaged && g_sc_state==SC_IDLE))return;
    g_restart_fault=1;g_restart_hold=1;
    if(!g_sc_fault_signal)g_sc_fault_signal=f1c_scf_open();
    if(!g_sc_fault_latched && g_sc_fault_signal) {
        LONG seq=f1c_scf_publish(g_sc_fault_signal,f1c_rc_read(&g_rc->sc_generation),
            sc_slot_owned()?(LONG)VEH(g_sc_slot):0,reason);
        if(seq){g_sc_fault_latched=1;
            logf_("[SC fault -> RF] request=%ld reason=%d phase=%d generation=%ld; physical reset required before takeover",
                seq,reason,g_sc_state,f1c_rc_read(&g_rc->sc_generation));}
    }
    hud_notice(g_sc_fault_signal && f1c_scf_read(&g_sc_fault_signal->rf_registered)
        ? "RED FLAG: SAFETY CAR FAULT" : "SAFETY CAR FAULT");
}
static void sc_pump(void)
{
    if (!sc_slot_owned()) return;
    if(sc_action_authorized() && g_engaged &&
       (g_sc_state==SC_CATCHUP || g_sc_state==SC_ONTRACK) && pit_owned(g_sc_slot)) {
        sc_request_fault(F1C_SCF_RESET);return;
    }
    if(g_restart_fault && (g_engaged || g_sc_state!=SC_IDLE)){sc_request_fault(F1C_SCF_RESET);return;} /* no blind native retries */
    /* This pump mutates independently of g_engaged (park maintenance and
       loader residue), so its native writes also require known-offline state. */
    if (!sc_offline_authorized()) return;
    if (g_engaged && sc_redflag_veto() && g_rf_sequence_active) {
        g_silent_release=1; g_rf_handoff=1;
        release("Red Flag sequence took ownership");
        return;
    }
    if(g_initial_park.parked && g_sc_state==SC_IDLE) {
        if(!sc_initial_restore()) {
            g_initial_park.parked=0;
            logf_("[pit-reset] parked-state maintenance failed");
        }
        return;
    }
    if (g_sc_state == SC_IDLE) {
        /* The pit-box start owns him: sc_park() would arm the game's pit call
         * and send a car that is already in his box off for an in-lap. */
        /* THE PIT-BOX START AND sc_park() ARE MUTUALLY EXCLUSIVE.
         *
         * sc_park() arms the game's own pit call at the first racing tick so a
         * grid-started safety car drives in and garages himself. With the
         * pit-box start on he is ALREADY in his box, and that arming retires
         * him: the game runs its retirement, teleports him in front of his
         * garage bay and sets retired=1. Driver, 2026-08-03: "was teleported
         * in front of his garage box ... was retired". The earlier guard only
         * covered the time AFTER placement, and the arming happens before it.
         *
         * So the whole feature stands down whenever the box start is armed. */
        if (g_sc_park) {
            int i = find_safety_car();
            if (i > 0) sc_park(i);      /* i > 0: never the player's own slot */
        }
        return;
    }

    /* FORK: a parked extra car is role -1 - that IS the hidden state, not a
     * lost slot. Only abort when he is neither live nor hidden. */
    if (g_sc_slot < 0 ||
        (V_I32(g_sc_slot, OFF_ACTIVE) == -1 && !g_hidden)) {
        logf_("SAFETY CAR: slot lost; retaining field in fault hold");
        g_sc_state = SC_IDLE;
        if(g_engaged){g_restart_hold=1;g_restart_fault=1;g_ct=CT_RESTART;yellow_set(1);say_flush("SC slot lost");hud_notice("SAFETY CAR LOST");}
        return;
    }

    if(g_sc_state>=SC_ARMING && g_sc_state<=SC_RECALL){
        int fault=sc_progress_step(&g_sc_progress,g_epoch,g_sc_state,g_evidence_now,
            V_F32(g_sc_slot,OFF_LAPDIST),g_lap_len);
        if(fault){g_restart_fault=1;g_restart_hold=1;g_ct=CT_RESTART;
            say_flush("SC progress fault");yellow_set(1);
            hud_notice("SAFETY CAR FAULT");
            logf_("[SC progress] held fault=%d phase=%d; requesting dedicated RF",fault,g_sc_state);
            sc_request_fault(fault==3?F1C_SCF_STUCK:F1C_SCF_PROGRESS);return;}
    }else memset(&g_sc_progress,0,sizeof(g_sc_progress));
    if(g_sc_state!=SC_IDLE && g_sc_state!=SC_ARMING && !pit_owned(g_sc_slot)) {
        F1CVehicleLifecycle life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(g_sc_slot));
        if(life.valid && !life.garage_clear &&
           (f1c_lifecycle_terminal(life.retired,life.finish)||V_I32(g_sc_slot,OFF_CARCOND)>=2)) {
            sc_request_fault(F1C_SCF_DAMAGE);return;
        }
    }
    /* Every tick he is out of the garage, whatever phase he is in - including
     * the recall, because the pit entry is where he crashed.               */
    if (g_sc_state != SC_ARMING) sc_damage_clear(g_sc_slot);

    switch (g_sc_state) {
    case SC_ARMING: {
        F1CSessionGuard guard=sc_session_guard();
        /* A pause simply freezes the held queue. RF activation, loss of the
           measured offline state, or a teardown/menu transition cancels the
           still-queued physical departure without publishing a false GREEN. */
        if (guard.paused) break;
        if (g_weather_cycle && !g_weather_departure_fresh) break;
        if (!guard.valid || !guard.offline ||
            (guard.game_state != 5 && guard.game_state != 7) ||
            guard.session_type != 7 || sc_redflag_veto()) {
            if (g_rf_sequence_active || !guard.valid || !guard.offline || !guard.settled) {
                g_silent_release=1; if(g_rf_sequence_active)g_rf_handoff=1;
                release("queued departure authorization lost");
            }
            break;
        }
        /* HE WAITS FOR THE LEADER instead of chasing him.
         *
         * The first design revived him at once and then ran a CATCH-UP phase,
         * because the pit exit drops him wherever it drops him - P19 of 23 in
         * the first live run. Real procedure is the opposite and much simpler:
         * the safety car sits at the end of the pit lane and pulls out in
         * front of the leader as he comes past. Driver's spec, 2026-08-01.
         *
         * So he is held in the garage - by the game, at no cost, since he is
         * already parked there - until the leader is within RELEASE_WINDOW_M
         * of the start line. By the time he has driven down the pit lane the
         * leader is arriving, and he emerges ahead of him. The catch-up phase
         * then finds him already in front and ends immediately.
         *
         * The minimum delay still applies: the field must be slowing BEFORE he
         * is let out, or he emerges into cars at racing speed (2026-07-25).  */
        int   ready = isfinite(SESSION_TIME) && g_sc_arm_sim>=0.0 &&
                      g_sc_progress.elapsed>=SC_ARM_DELAY_MS/1000.0;
        int   lead  = find_leader();
        float to_line = -1.0f;

        /* RELEASE TIMING (driver spec 2026-08-08). SUPERSEDES the old "no lap
         * completed yet means GO NOW" behaviour.
         *
         * He must exit the pits AHEAD of the race leader, with a clear lap, not
         * race through the field to reach the front. So the whole field is
         * neutralised at the caution pace the instant the caution is called
         * (full yellow - engage() already caps every car), the safety car is
         * HELD in the pits, and he is released only on a FRESH approach to the
         * line: the leader must have been BEYOND the release window since the
         * caution began (g_arm_saw_far), then come back to WITHIN it. Released
         * at that moment, the leader is about to reach the line as the SC
         * emerges, so the SC comes out in front of him.
         *
         * The lap length is seeded from the Steward at load, so to_line is
         * valid from the green flag - which is what makes this reliable on
         * LAP 1: the caution is called with the leader out on the lap
         * (to_line large -> saw_far latches), and he is released as the leader
         * nears the line at the end of that same lap. The saw_far latch only
         * costs an extra lap in the one edge case where the caution is called
         * with the leader already inside the window, where releasing at once
         * would put the SC out behind him. */
        if (lead >= 0 && lead != g_sc_slot && g_lap_len_exact && g_lap_len > 0.0f)
            to_line = g_lap_len - V_F32(lead, OFF_LAPDIST);

        if (to_line < 0.0f) {
            static DWORD last_ack = 0;
            if (GetTickCount() - last_ack > 5000) {
                last_ack = GetTickCount();
                logf_("  [sc] holding the safety car - waiting for a valid "
                      "leader and lap length (lap length known=%d)",
                      g_lap_len_exact);
            }
            break;
        }

        if (to_line > (float)SC_RELEASE_WINDOW_M) g_arm_saw_far = 1;

        if (!(ready && g_arm_saw_far && to_line <= (float)SC_RELEASE_WINDOW_M)) {
            static DWORD last_ack = 0;
            if (GetTickCount() - last_ack > 5000) {
                last_ack = GetTickCount();
                logf_("  [sc] holding the safety car - field at the caution "
                      "pace (full yellow), waiting for the leader to come "
                      "within %dm of the line on a fresh lap (to_line=%.0f, "
                      "saw_far=%d)", (int)SC_RELEASE_WINDOW_M, to_line,
                      g_arm_saw_far);
            }
            break;
        }

        if (ready && to_line >= 0.0f && to_line <= SC_RELEASE_WINDOW_M) {
            /* Back into the field first - the game will not simulate or
             * render him otherwise - and revive on the NEXT tick, so he is
             * never a live-but-retired car with no collision.             */
            /* DISARM THE GAME'S PIT CALL BEFORE HE IS PUT BACK.
             *
             * veh+0xD58 = 0 plus callToBox = 1 is the game's own retirement,
             * and its retirement ENDS IN A GARAGE TELEPORT. Armed at the box
             * so the stop runs like lap 1, it is still pending at the deploy:
             * measured 2026-08-03, he left the box correctly at the limiter
             * (pit 4, maxSpeed 22, x=1.5) and ten seconds later was back at
             * his garage bay (16.8, 511.0) uncapped, with no route - "it just
             * hit the wall again". sc_revive() does clear both, but only on
             * the tick AFTER the un-hide, and that one tick is enough.
             *
             * So the call is withdrawn here, while he is still out of the
             * field and nothing can act on him.                          */
            *(volatile char*)(VEH(g_sc_slot) + OFF_RWFLAG) = 1;
            V_I32(g_sc_slot, OFF_CALLBOX) = 0;

            if (g_hidden) { sc_unhide(); break; }

            /* THE RETURN VALUE IS THE POINT. sc_revive() refuses a car that is
             * outside the world and sets SC_IDLE to say so - and this line used
             * to overwrite that with SC_OUTLAP on the very next statement, so
             * the refusal did nothing at all. Seen live 2026-08-01: "REFUSING
             * to revive - he is at Y=-1578.3" followed immediately by
             * "released from the garage". */
            if (!sc_revive(g_sc_slot)) break;
            sc_stint_begin(&g_sc_stint,g_epoch);
            g_sc_state = SC_OUTLAP;
            hud_notice("SAFETY CAR LEAVING PITS");
            sc_say_later(SCORE_SCAR01, "SCAR01 deployed, exiting the pits now", 0);
            logf_("  safety car released from the garage - the leader is "
                  "%.0f m from the line%s, he will come out in front of him",
                  to_line,
                  g_lap_len_exact ? ""
                                  : " (no lap completed yet - lap length not "
                                    "measured, so he goes at once)");
        }
        break;
    }

    case SC_OUTLAP:
        /* Uncapped until he is clear of the pit limiter, then he becomes the
         * pace line's leader and the field forms on HIM.
         *
         * NOT WHILE HE IS STILL IN THE PIT LANE. `maxSpeed` is the same field
         * the game's own 80 km/h limiter writes (flt_8ACFB4 = 22.2222 m/s), so
         * writing 99999 here every tick overrides the limiter. Out of the
         * GARAGE that never showed, because the game had already put him on
         * the road; out of his BOX it means he leaves the stop at full
         * throttle - driver, 2026-08-03: "he launched off after pitting". So
         * while he is lane-owned the limiter is left alone, exactly as for a
         * real car serving a stop, and the release happens at the exit line,
         * which is where the game itself clears pittingStatus.           */
        if (!g_sc_in_box || V_I32(g_sc_slot, OFF_PITTING) == 0)
            V_F32(g_sc_slot, OFF_MAXSPEED) = (float)SPEED_RELEASE;
        if (V_F32(g_sc_slot, OFF_SPEED) > SC_ONTRACK_MS &&
            V_I32(g_sc_slot, OFF_PITTING) == 0) {
            g_sc_state = SC_CATCHUP;
            sc_say_later(SCORE_SCAR02, "SCAR02 out on track", 0);
            logf_("  safety car is on track at %.1f m/s - catching the field "
                  "(%.0f m behind the leader)",
                  V_F32(g_sc_slot, OFF_SPEED), -sc_ahead_of_leader());
            hud_notice("SAFETY CAR ON TRACK");
        }
        break;

    case SC_CATCHUP: {
        /* NT2004 pace-lap substate 1. He runs unrestricted until he is in
         * front of the race leader; the field keeps pacing behind the RACE
         * leader in the meantime.                                         */
        float ahead = sc_ahead_of_leader();
        V_F32(g_sc_slot, OFF_MAXSPEED) = (float)SPEED_RELEASE;
        /* IN FRONT IS THE WHOLE CONDITION. The first version also demanded he
         * be within 240 m, which looks like a sanity check and is really a
         * trap: on 2026-08-01 the gap was unmeasurable for a while (see the
         * shadow-copy bug in ctl_pump), he ran uncapped past the leader, and
         * by the time the gap could be read at all he was 295 m clear - past
         * the window, so the phase could never end and he kept pulling away.
         * sc_ahead_of_leader() already folds anything beyond half a lap into a
         * negative, so "ahead" cannot silently mean "nearly a lap behind", and
         * no upper bound is needed beyond that fold.                        */
        if (ahead > -1e8f && ahead >= 0.0f) {
            g_sc_state = SC_ONTRACK;
            g_fallback[g_sc_slot] = 0;   /* he was 'ahead of the leader' while
                                          * catching; that is over now      */
            logf_("=== SAFETY CAR HAS THE FIELD === %.0f m ahead of the race "
                  "leader, now setting the pace", ahead);
            hud_notice("SAFETY CAR ON TRACK");
        }
        break;
    }

    case SC_ONTRACK:
        /* He is the pace setter. While hold_pump()'s cap is on him
         * (g_sc_cmd != 0, the 2026-08-02b gather/hold governor) it must be
         * left alone: sc_pump runs AFTER hold_pump, and the unconditional
         * release that used to stand here silently cancelled the 32 m/s
         * gather cap every tick - the field then stretched instead of closing
         * up, which is exactly the failure the gather pace was written to
         * fix. Fixed 2026-08-07: the cap now survives for the whole ONTRACK
         * phase, gathering at 32 m/s until the queue is formed (or the 90 s
         * timeout) and then holding at the pace setting.
         *
         * Only when the cap is off (gather feature disabled, g_sc_cmd == 0)
         * or the caution machinery has been stood down (g_engaged == 0) does
         * he run to his own car's limits; the field then matches him through
         * sc_pace_speed()'s measurement.                                    */
        if (!g_sc_cmd || !g_engaged)
            V_F32(g_sc_slot, OFF_MAXSPEED) = (float)SPEED_RELEASE;
        /* He is the pace setter, not a car being held back. Clearing this
         * every tick (not once) means no later code path can re-latch it. */
        g_fallback[g_sc_slot] = 0;
        break;

    case SC_RECALL:
        /* Pit ownership, never damage/retirement, triggers the return. */
        if (!g_sc_recall_armed && pit_owned(g_sc_slot)) {
            g_sc_recall_armed=1;
            if(!g_sc_said05){g_sc_said05=1;sc_say_later(SCORE_SCAR05,"SCAR05 entering the pits now",0);}
            sc_say_later(SCORE_SCAR04,"SCAR04 safety car is in",0);
            g_pit_handover=1; /* native pit ownership is verified handover evidence */
            restart_arm(); /* race leader baseline, before SC becomes hidden */
            if(sc_initial_restore()) {
                logf_("[pit-reset] native per-car restart and AI rebind complete; initial pose/parked controls restored; next departure requires validation");

            } else {
                logf_("[pit-reset] FAILED restoration; no damaged-retirement fallback");
                g_restart_fault=1;g_restart_hold=1;g_ct=CT_RESTART;
                yellow_set(1);say_flush("SC reset failed");
                hud_notice("SAFETY CAR RESET FAILED");
            }
        }
        break;
    default: break;
    }
}

static void engage(void)
{
    int i, n = 0;
    if(g_rf_stand_down || !sc_action_authorized()) { hud_show("SAFETY CAR UNAVAILABLE"); return; }

    if (g_engaged) { hud_show("SAFETY CAR ALREADY ACTIVE"); return; }
    g_sc_continuation=0;
    if(!sc_distance_eligible(1))return;

    if (!phase_racing()) {
        logf_("ENGAGE refused - race phase is %d, need %d", RACE_PHASE, PHASE_RACING);
        hud_show("RACE NOT ACTIVE");
        return;
    }

    /* Device/release entitlement is checked once at licensed startup. */
    memset(g_armed, 0, sizeof(g_armed));
    memset(g_wave, WAVE_NONE, sizeof(g_wave));
    memset(g_wave_passed, 0, sizeof(g_wave_passed));
    memset(g_catchup, 0, sizeof(g_catchup));
    /* The gather cap is per-caution state. Left set across a session change it
     * would be re-applied to whatever car ends up in that slot. */
    g_gather_start = 0;
    g_sc_cmd       = 0;
    say_flush("caution reset");
    memset(g_cap_why, 0, sizeof(g_cap_why));
    memset(g_cap_obst, 0xFF, sizeof(g_cap_obst));   /* -1 = no obstacle */
    memset(g_cap_gap, 0, sizeof(g_cap_gap));
    g_ramp_from = g_pace_speed;
    for (i = 0; i < VEH_SLOTS; i++) {
        float s;
        if (!slot_governed(i)) continue;
        if (!capture_car(i)) continue;
        s = V_F32(i, OFF_SPEED);            /* ramp starts at the fastest car */
        if (isfinite(s) && s>=0.0f && s<=200.0f && (int)s + 1 > g_ramp_from) g_ramp_from = (int)s + 1;
        n++;
    }

    if (!n) { logf_("ENGAGE refused - no governed slots"); hud_show("SAFETY CAR UNAVAILABLE"); return; }
    hud_notices_reset();

    g_engaged = 1;
    memset(&g_recall_ready,0,sizeof(g_recall_ready));
    memset(&g_order_settle,0,sizeof(g_order_settle));g_order_locked=0;
    sc_queue_reset(&g_queue_order);
    queue_order_pump();
    g_overrides   = 0;
    g_engage_tick = GetTickCount();
    g_engage_stime= SESSION_TIME;
    g_last_stime  = SESSION_TIME;
    memset(g_stuck_watch, 0, sizeof(g_stuck_watch));
    g_ramp_ms     = 0.0;
    g_cmd_now     = g_ramp ? g_ramp_from : g_pace_speed;

    logf_("=== PACE ENGAGED === %d cars, target %d m/s (%.0f km/h), "
          "ramp %s from %d m/s over %.0f s, phase=%d session=%d SessionTime=%.2f",
          n, g_pace_speed, g_pace_speed * 3.6, g_ramp ? "ON" : "OFF",
          g_ramp_from, RAMP_SECONDS, RACE_PHASE, SESSION_TYPE, SESSION_TIME);
    if (g_prof_n >= PROF_READY) {
        int q; float lo = 1e9f, hi = 0.0f;
        for (q = 0; q < PROF_N; q++) {
            if (g_prof[q] <= 0.0f) continue;
            if (g_prof[q] < lo) lo = g_prof[q];
            if (g_prof[q] > hi) hi = g_prof[q];
        }
        logf_("  [profile] track shape learned from %d/%d slices: green speeds "
              "%.1f-%.1f m/s, mean %.1f - the pace is shaped to this, so its "
              "MEAN stays at the setting but corners and straights differ",
              g_prof_n, PROF_N, lo, hi, g_prof_mean);
    } else {
        logf_("  [profile] only %d/%d slices learned - pace stays FLAT this "
              "caution (a full green lap populates it)", g_prof_n, PROF_N);
    }
    for (i = 0; i < VEH_SLOTS; i++) {
        if (!g_armed[i]) continue;
        logf_("  slot %2d P%-2d %-28.28s saved maxSpeed=%.1f speed=%.1f",
              i, V_I32(i, OFF_POSITION), V_STR(i, OFF_NAME),
              g_saved[i], V_F32(i, OFF_SPEED));
    }
    hud_show("SAFETY CAR");
}

/* Restore what was snapshotted, not a hardcoded default. The usual value is
 * 99999.0 (the proven release call) but the field is NOT uniform: the first
 * live dump, 2026-07-31, showed slot 6 sitting at 999999.0 while all 21 other
 * cars read 99999.0. Writing the "known" release value would have silently
 * changed that car's own setting, so every car is restored to its own captured
 * number, rounded, because the setter takes an int.                         */
static int sc_has_control_loans(void){
    for(int j=0;j<VEH_SLOTS;j++)if(g_armed[j])return 1;
    return 0;
}
static void release(const char *why)
{
    int had_cycle=g_engaged || g_ct!=CT_OFF || g_sc_state!=SC_IDLE;
    int had_loans=sc_has_control_loans();
    /* Idle cleanup is not a race-control transition. */
    if(!had_cycle && !had_loans && !g_yellow){
        g_silent_release=0;g_rf_handoff=0;return;
    }
    sc_order_reset(&g_order_recovery);
    say_flush(why);
    int i, n = 0;

    /* Restore acquired loans even if an enable attempt failed. */

    for (i = 0; i < VEH_SLOTS; i++) {
        int want;
        if (!g_armed[i]) continue;
        /* RF may have snapshotted or overwritten these exact fields. Its
           monitor is not an atomic coordinator, so an observed takeover only
           drops SC bookkeeping; it never writes a guessed restoration. */
        if (g_rf_handoff) { g_armed[i]=0; continue; }
        if (!capture_identity_owned(i)) { g_armed[i] = 0; continue; }   /* car left the session */

        /* The AI's own aggression always goes back, even for a car in the pit
         * lane whose speed we are not restoring - it is a separate loan.   */
        ai_restore_passing(i);

        if (pit_owned(i)) {          /* the game re-owns it; do not fight it */
            g_armed[i] = 0;
            logf_("  [pit] slot %2d released without restore (in pit lane)", i);
            continue;
        }

        want = SPEED_RELEASE;
        if (g_saved[i] > 0.0f && g_saved[i] < 2.0e9f)
            want = (int)(g_saved[i] + 0.5f);
        if (want == SPEED_IDLE_SENTINEL) want = SPEED_IDLE_SENTINEL + 1;

        set_maxspeed((void*)VEH(i), want);
        g_armed[i] = 0;
        n++;
    }

    logf_("=== PACE RELEASED === %s | %d cars restored | held %.1f s of "
          "SessionTime (%.1f s wall) | %ld game overrides corrected",
          why, n, had_cycle?session_elapsed():0.0,
          had_cycle?(GetTickCount() - g_engage_tick) / 1000.0:0.0, g_overrides);

    g_engaged = 0;g_pit_handover=0;
    g_caution = CAUTION_OFF;
    g_ct      = CT_OFF;
    if(had_cycle)g_restart_green_ms = sc_sim_millis(SESSION_TIME);
    /* Red Flag owns race-control presentation and may have captured our AI
       values already. Do not announce GREEN or write its flag state. */
    if (!g_silent_release) {
        if(g_yellow)yellow_set(0); /* only a caution flag owned by this mod */
        if(had_cycle)hud_show("SAFETY CAR WITHDRAWN");
    }
    g_silent_release=0;
    g_rf_handoff=0;
}

/* Returns 1 only after the original AI values and SC physical state are safe.
 * This routine is invoked solely from the authenticated broker game callback. */
static int sc_rf_yield(int authorized) {
    if(!authorized)return 0;
    if(g_sc_state==SC_ARMING && g_hidden && sc_initial_owned() && sc_ready(g_sc_slot))
        g_sc_state=SC_IDLE; /* physical departure has not happened */
    if((g_sc_state!=SC_IDLE || !g_hidden || !sc_ready(g_sc_slot)) && !sc_initial_restore()) {
        hud_show("SAFETY CAR RESET FAILED");return 0;
    }
    if(!sc_slot_owned() || !g_hidden || !sc_ready(g_sc_slot))return 0;
    g_silent_release=1;g_rf_handoff=0;
    release("acknowledged Red Flag handoff");
    sc_lap_unforce();
    if(g_yellow)yellow_set(0);
    if(sc_has_control_loans() || g_engaged || g_sc_state!=SC_IDLE)return 0;
    /* Availability is restored only after owned garage/physical proof. */
    g_restart_fault=g_restart_hold=g_sc_fault_latched=0;
    g_sc_identity_fault_since=-1.0;
    memset(&g_sc_progress,0,sizeof(g_sc_progress));
    g_rf_stand_down=0;
    sc_auto_handoff_reconcile(&g_auto_state,SESSION_TIME);
    f1c_rc_sc_release(g_rc);
    return 1;
}


/* One game-thread transaction shared by key and GUI. Never release before
   physical restoration is verified; partial native init is never retried. */
static int sc_emergency_reset(void) {
    if(!on_game_thread() || !field_stable() || !phase_racing() ||
       !sc_offline_authorized() || !g_rc ||
       f1c_rc_read(&g_rc->owner)!=F1C_RC_SC)return 0;
    g_emergency_inhibit=1;
    if(g_ctl){g_ctl->cmd_panic=0;g_ctl->cmd_safetycar=0;g_ctl->cmd_engage=0;
        g_ctl->cmd_release=0;g_ctl->cmd_yellow=0;}
    say_flush("emergency reset");
    if(g_emergency_failed)return 0;
    if(!sc_initial_restore()) {
        g_emergency_failed=1;g_restart_fault=1;g_restart_hold=1;
        hud_show("SAFETY CAR RESET FAILED");
        logf_("[emergency] parked restore failed; ownership retained, no retries");
        return 0;
    }
    /* Under SC lease RF cannot acquire; requested RF takes over only after
       the normal end-of-callback acknowledgement. No global GREEN command. */
    int rf=sc_redflag_veto();
    g_silent_release=rf;g_rf_handoff=0;
    release("emergency reset");
    sc_lap_unforce();
    if(g_yellow)yellow_set(0);
    if(sc_has_control_loans() || g_engaged || g_sc_state!=SC_IDLE) {
        g_emergency_failed=1;g_restart_fault=1;g_restart_hold=1;
        hud_show("SAFETY CAR RESET FAILED");return 0;
    }
    memset(&g_sc_progress,0,sizeof(g_sc_progress));
    memset(&g_weather,0,sizeof(g_weather));memset(&g_clearance,0,sizeof(g_clearance));
    memset(g_wave,0,sizeof(g_wave));memset(g_catchup,0,sizeof(g_catchup));
    memset(g_stuck_watch,0,sizeof(g_stuck_watch));
    memset(g_restart_free,0,sizeof(g_restart_free));
    memset(&g_trig,0,sizeof(g_trig));sc_order_reset(&g_order_recovery);
    sc_auto_handoff_reconcile(&g_auto_state,SESSION_TIME);
    g_restart_fault=g_restart_hold=g_restart_crossing_seen=0;
    g_restart_leader=g_restart_roster=g_recall_leader=g_recall_lap=-1;
    g_restart_epoch=0;g_restart_tick=0;g_green_said=0;
    g_weather_cycle=g_weather_departure_fresh=0;
    g_sc_recall_armed=g_sc_said05=g_pit_handover=0;
    g_ct_seen_ms=0;g_ct_laps_done=0;g_ct_lap_ref=-1;
    g_gather_start=0;g_gather_sim=-1;g_sc_arm_sim=-1;
    g_emergency_inhibit=g_emergency_disarm;
    hud_show(rf?"RED FLAG TAKES CONTROL":"SAFETY CAR RESET COMPLETE");
    logf_("[emergency] complete; auto inhibited=%d RF requested=%d",g_emergency_inhibit,rf);
    return 1;
}

/* ---- the restart: acceleration and GREEN chronology --------------------
 * Owner policy: restore normal AI controls at XPITIN handover so an uncapped
 * player leader cannot accelerate alone. The leader's later validated line
 * crossing still lowers yellow and announces GREEN. Native AI owns driving. */
static void restart_free_all(const char *why);
static void restart_accelerate_all(void) {
    for(int i=0;i<VEH_SLOTS;i++) {
        if(!g_armed[i] || !capture_identity_owned(i))continue;
        if(!pit_owned(i))set_maxspeed((void*)VEH(i),SPEED_RELEASE);
        sc_ai_calm(i); /* retain the no-passing loan until validated GREEN */
    }
}
static void restart_arm(void)
{
    int i, n = 0;
    if(g_sc_continuation){g_ct=CT_PACE;g_pit_handover=0;g_bunch=1;return;}
    if(!g_pit_handover || g_sc_state==SC_ARMING)return; /* no restart while parked */
    if(g_ct==CT_RESTART)return; /* later pit ownership cannot erase crossing */

    if (!g_engaged) { g_ct = CT_OFF; return; }

    memset(g_fallback, 0, sizeof(g_fallback));   /* stale hold-backs would cap */
    for (i = 0; i < VEH_SLOTS; i++) {
        g_restart_free[i] = 0;
        g_restart_lap[i]  = -1;
        if (!g_armed[i] || !slot_live(i)) continue;
        g_restart_lap[i] = V_I32(i, OFF_LAPNUM);
        n++;
    }

    g_ct          = CT_RESTART;
    g_green_said  = 0;
    g_restart_hold = 0;
    g_restart_crossing_seen = 0;
    g_restart_tick = sc_sim_millis(SESSION_TIME);
    g_restart_sim_start = g_restart_sim_last = SESSION_TIME;
    /* The RACE leader, explicitly - NOT find_leader(). The safety car is still
     * in SC_RECALL at this point (he is now held in that state until he reaches
     * his GARAGE, not dropped the moment he enters the lane), so find_leader()
     * still returns HIM here. He is pit_owned in the lane, which tripped
     * restart_pump's "leader is no longer running" safety net and threw the
     * green the instant he entered the pit entrance. The green must wait for
     * the real P1 to cross the line, so lock onto the real leader. Driver
     * report 2026-08-08. */
    g_leader_slot    = race_leader_slot();
    g_restart_leader = g_leader_slot;
    if (g_restart_leader >= 0 && g_restart_leader < VEH_SLOTS) {
        int current_lap=V_I32(g_restart_leader,OFF_LAPNUM);
        /* Handover, not the earlier recall request, establishes the crossing
           chronology.  A crossing before XPITIN can never authorize GREEN. */
        g_restart_lap[g_restart_leader]=current_lap;
        g_restart_roster=V_I32(g_restart_leader,OFF_ROSTER);
        g_restart_epoch=g_epoch;
        memcpy(g_restart_name,V_STR(g_restart_leader,OFF_NAME),sizeof(g_restart_name));
        logf_("[restart] handover baseline: leader=%d lap=%d roster=%d epoch=%u",
              g_restart_leader,current_lap,g_restart_roster,g_restart_epoch);
    }
    g_recall_leader=-1; /* consumed: never reuse for another return */

    /* Player and AI may accelerate together from XPITIN. Release the complete
       owned AI controls now, while retaining the leader's GREEN chronology. */
    restart_accelerate_all();
    logf_("=== RESTART ARMED === safety car handed over at pit entry. %d cars "
          "returned to normal AI control; leader slot %d (lap %d) crossing "
          "will announce GREEN.",
          n, g_restart_leader,
          g_restart_leader >= 0 ? g_restart_lap[g_restart_leader] : -1);
    hud_show("SAFETY CAR IN PITS");
}

/* Release one car for good: its own captured cap and aggression back. */
static void restart_free_car(int i, const char *why)
{
    int want = SPEED_RELEASE;

    if (!capture_identity_owned(i)) { g_armed[i] = 0; return; }
    if (g_saved[i] > 0.0f && g_saved[i] < 2.0e9f)
        want = (int)(g_saved[i] + 0.5f);
    if (want == SPEED_IDLE_SENTINEL) want = SPEED_IDLE_SENTINEL + 1;

    ai_restore_passing(i);
    if (!pit_owned(i)) set_maxspeed((void*)VEH(i), want);

    g_restart_free[i] = 1;
    g_armed[i]        = 0;
    logf_("  [restart] slot %2d P%-2d %-24.24s RACING (%s)",
          i, V_I32(i, OFF_POSITION), V_STR(i, OFF_NAME), why);
}

/* Free the whole field in one instant. */
static void restart_free_all(const char *why)
{
    int i;
    for (i = 0; i < VEH_SLOTS; i++)
        if (g_armed[i] && !g_restart_free[i] && slot_live(i))
            restart_free_car(i, why);
}

/* Restart eligibility is race identity, not an existing control loan: a
   player or a driver who left/rejoined the governed set can legitimately lead.
   Pit-owned cars are excluded before selection. */
static int restart_successor_slot(void)
{
    int i,best=-1,best_pos=1<<30;
    for(i=0;i<VEH_SLOTS;i++) {
        int p;
        if(i==g_sc_slot||!slot_live(i)||pit_owned(i)||!slot_moving(i))continue;
        p=V_I32(i,OFF_POSITION);
        if(p>0&&p<best_pos){best=i;best_pos=p;}
    }
    return best;
}

static int restart_leader_identity_valid(int i)
{
    return i>=0 && i<VEH_SLOTS && g_restart_epoch==g_epoch && slot_live(i) &&
        g_restart_roster==V_I32(i,OFF_ROSTER) &&
        memcmp(g_restart_name,V_STR(i,OFF_NAME),sizeof(g_restart_name))==0;
}

/* AI acceleration is already released at handover. Validate the locked real
 * leader's later crossing for GREEN; do not use SC lap or loan count. */
static void restart_pump(void)
{
    int lead, left = 0, i;

    if (g_ct != CT_RESTART || !phase_racing()) return;
    restart_accelerate_all();
    /* Paused wall time cannot trigger a crossing, dead-leader fallback or timeout. */
    if (!(SESSION_TIME > g_restart_sim_last)) return;
    g_restart_sim_last = SESSION_TIME;

    for (i = 0; i < VEH_SLOTS; i++)
        if (g_armed[i] && !g_restart_free[i] && slot_live(i)) left++;

    lead = g_restart_leader;

    /* A retired/pitting leader cannot authorize GREEN.  Adopt one verified
       governed successor and require its own later crossing; do not reuse a
       stale lap/slot as an authorization token. */
    if (!restart_leader_identity_valid(lead) || pit_owned(lead) || !slot_moving(lead)) {
        int successor = restart_successor_slot();
        if (successor >= 0 && slot_live(successor) && slot_moving(successor) &&
            !pit_owned(successor)) {
            g_restart_leader=lead=successor;
            g_restart_lap[lead]=V_I32(lead,OFF_LAPNUM);
            g_restart_roster=V_I32(lead,OFF_ROSTER);g_restart_epoch=g_epoch;
            memcpy(g_restart_name,V_STR(lead,OFF_NAME),sizeof(g_restart_name));
            g_restart_hold=1;
            logf_("[restart] successor=%d baseline=%d; crossing required",lead,g_restart_lap[lead]);
        } else {
            g_restart_hold=1;
            hud_notice("GREEN FLAG DELAYED: LEADER");
            return;
        }
    }

    if (!g_green_said) {
        int crossed = (restart_leader_identity_valid(lead) && g_restart_lap[lead] >= 0 &&
                       V_I32(lead, OFF_LAPNUM) > g_restart_lap[lead]);
        int safe_to_green = crossed && g_pit_handover && !g_restart_fault &&
            !g_sc_continuation &&
            (g_sc_forced_return || (clearance_recall_ok() && !g_weather.hold && !g_weather.bad));

        /* Hazards that arrive after SC IN are ledger events.  Consume the
           attempted crossing and continue under caution to the next genuine
           crossing; timeout is diagnostically useful, never racing authority. */
        if (crossed && !safe_to_green) {
            g_restart_lap[lead]=V_I32(lead,OFF_LAPNUM);
            g_restart_crossing_seen=1; g_restart_hold=1;
            hud_notice("GREEN FLAG DELAYED: INCIDENT");
            logf_("[restart] crossing held: handover=%d fault=%d clearance=%d weather_hold=%d weather_bad=%d sc=%d; next crossing required",g_pit_handover,g_restart_fault,clearance_recall_ok(),g_weather.hold,g_weather.bad,g_sc_state);
            return;
        }
        if (!crossed && (SESSION_TIME-g_restart_sim_start)*1000.0 > RESTART_TIMEOUT_MS) {
            g_restart_hold=1;
            hud_notice("GREEN FLAG DELAYED");
            logf_("[restart] timeout held; no GREEN without a valid crossing");
            return;
        }
        if (safe_to_green) {
            g_green_said = 1;
            yellow_set(0);
            say_eid(126, "GREEN01 green flag");
            logf_("=== GREEN FLAG === validated leader/successor crossing after XPITIN acceleration release");
            hud_show("GREEN");
            restart_free_all("leader took validated green");
            left = 0;
        }
    }

    if (g_green_said && left) {           /* a slot that went live afterwards */
        restart_free_all("green already out");
        left = 0;
    }

    /* Zero loans is expected after XPITIN, including a player-led field. The
       verified leader still owns GREEN; lack of pace loans is not an error. */
    if (g_green_said && !left) {
        logf_("=== RESTART COMPLETE === whole field racing");
        say_flush("GREEN release");
        g_engaged = 0;g_pit_handover=0;
        g_bunch   = 0;
        g_caution = CAUTION_OFF;
        g_ct      = CT_OFF;
        g_restart_green_ms = sc_sim_millis(SESSION_TIME);   /* NT2004's 10 s scanner grace */
        yellow_set(0);
    }
}

/* ---- the watcher and the caution supervisor -----------------------------
 * NT2004 sweeps the field itself, a few cars at a time, asking whether each is
 * stopped, off the pace, off track or damaged (sub_4F93E0). F1C ALREADY DOES
 * THAT, every frame, in sub_4D1730 - and publishes the answer as the zone
 * table. So the watcher is a read of the game's own verdict rather than a
 * reimplementation of it, which is both less code and better evidence.
 *
 * The decision NT2004 makes next (full caution vs quickie, sub_4F95D0) is a
 * NASCAR distinction and is not ported: F1 has one safety car procedure.    */
static int sc_incident_sector(int slot);

static void caution_pump(void)
{
    if(g_emergency_inhibit)return;
    int fire;

    /* The watcher also confirms new serious incidents during an active
     * caution and revokes a pending GREEN before the supervisor acts. */
    fire = trigger_eval(&g_trig);
    int distance_ok=sc_distance_eligible(0);
    fire=fire&&distance_ok;
    if(g_engaged && g_sc_continuation && !sc_redflag_veto()) {
        yellow_set(1);g_bunch=1;g_green_said=0;
        if(g_sc_state==SC_IDLE) {
            g_ct=CT_PACE;g_pit_handover=0;sc_deploy();
            if(g_sc_state==SC_ARMING)g_sc_continuation=0;
        } else if(g_sc_state!=SC_RECALL) {
            /* Already pacing: restart the readiness evidence for this incident. */
            memset(&g_recall_ready,0,sizeof(g_recall_ready));
            g_sc_continuation=0;
        }
        hud_notice("SAFETY CAR CONTINUES");
        return;
    }
    /* Weather does not require yellow flags or a stopped car. Red Flag veto
       and all native deployment/start/ownership guards still apply. */
    if(distance_ok && g_weather.pending && g_weather.pending_fresh && g_weather.last==g_evidence_now && g_ct_auto && phase_racing() && !sc_redflag_veto() &&
       (!g_ct_final_gate || TOTAL_RACE_LAPS<=0 || (race_leader_slot()>=0 && V_I32(race_leader_slot(),OFF_LAPNUM)<TOTAL_RACE_LAPS-1)) &&
       !g_engaged && g_ct==CT_OFF && g_sc_state==SC_IDLE && sc_slot_owned()) {
        sc_deploy();
        if(g_engaged && g_sc_state==SC_ARMING) {
            g_weather.pending=0;g_weather.pending_fresh=0;g_weather.handled=1;
            strcpy(g_activation_reason,"EXTREME WEATHER");
            g_activation_sector=0;
            g_weather_cycle=1;
            logf_("[weather-SC] accepted: EXTREME WET CONDITIONS");
        }
        return;
    }

    if (fire) {
        /* Severity decides the length. NT2004 calls these full and quickie;
         * F1 has one procedure, so it only shows up as the lap count.      */
        int laps = (g_trig.severity >= 2) ? g_ct_laps_tgt : g_ct_quick_laps;
        if (laps < 1) laps = 1;

        logf_("=== INCIDENT === %d zone(s) yellow, %d stopped, %d off the pace "
              "(field median %.1f m/s), held %d ms - declaring a %s caution of "
              "%d lap(s). Worst: slot %d '%s' at %.1f m/s.",
              g_trig.zones, g_trig.stopped, g_trig.offpace, g_trig.field_speed,
              g_trig.hold_ms, (g_trig.severity >= 2) ? "FULL" : "short",
              laps, g_trig.worst_slot, g_trig.worst_name, g_trig.worst_speed);

        g_ct_seen_ms  = 0;
        sc_deploy();              /* resets g_ct_laps_run to the full length */
        if(g_engaged && g_sc_state==SC_ARMING) {
            strcpy(g_activation_reason,g_auto_reason==5?"TWO CARS STALLED":
                (g_auto_reason==2?"MULTI-CAR INCIDENT":
                (g_auto_reason==4?"OVERTURNED CAR NEAR TRACK":(g_auto_reason==3?"DAMAGED CAR ON TRACK":"STOPPED CAR ON TRACK"))));
            g_activation_sector=sc_incident_sector(g_trig.worst_slot);
            clearance_step(1); /* ledger before auto acceptance clears slow flags */
            sc_auto_accept(&g_auto_state);
            logf_("[auto-SC] accepted: %s, slot=%d, held=%dms",g_activation_reason,g_trig.worst_slot,g_trig.hold_ms);
            g_ct_laps_run = laps;
        }
        return;
    }

    if (!phase_racing()) return;

    /* Continuous caution-time observation: this sees positive removal,
       recovery/pit entry, and new damaged stopped hazards. */
    if(g_engaged && sc_slot_owned() &&
       g_sc_state>=SC_OUTLAP && g_sc_state<=SC_ONTRACK &&
       sc_stint_step(&g_sc_stint,g_epoch,SESSION_TIME,V_F32(g_sc_slot,OFF_LAPDIST),
                     g_lap_len,(int)g_sc_config.MaximumStintLaps)) {
        logf_("[stint] maximum %d laps: controlled return requested",(int)g_sc_config.MaximumStintLaps);
        sc_recall(1);return;
    }
    clearance_step(0);

    /* Keep the flag up for as long as the caution lasts, in case something
     * else restored the bytes.                                              */
    if (g_ct >= CT_DEPLOY && g_ct <= CT_IN && !g_yellow) yellow_set(1);

    /* Caution laps. Counting starts only once the safety car actually has the
     * field - laps run while he is still catching it are not caution laps,
     * they are laps the leader spent waiting.                               */
    if (g_ct == CT_DEPLOY && sc_is_out() && g_caution != CAUTION_OFF)
        g_ct = CT_PACE;

    if (g_ct == CT_PACE && sc_slot_owned() && slot_live(g_sc_slot)) {
        /* sc_pump can switch to ONTRACK after hold_pump selected the race
           leader. Always count the owned SC, including this transition tick. */
        int lap = V_I32(g_sc_slot, OFF_LAPNUM);
        if (sc_caution_lap_step(&g_ct_lap_ref, lap)) {
            g_ct_laps_done++;
            logf_("  [caution] lap %d of %d complete under the safety car",
                  g_ct_laps_done, g_ct_laps_run);
        }

        int safe=!g_weather.hold && !g_weather.bad && !sc_redflag_veto() && clearance_recall_ok();
        int lap_ok=!g_sc_config.RecallUseFixedLaps || g_weather_cycle || g_ct_laps_done>=g_ct_laps_run;
        float marker[3];int marker_ok=f1c_pit_entry_marker(STEWARD,sc_marker_read,marker);
        float dx=marker_ok?V_F32(g_sc_slot,OFF_BODYX)-marker[0]:0;
        float dz=marker_ok?V_F32(g_sc_slot,OFF_BODYZ)-marker[2]:0;
        /* Refuse a last-second native pit request near XPITIN. Native AI owns
           the subsequent path and chooses its next viable entry; never teleport. */
        int entry_ok=marker_ok && isfinite(dx)&&isfinite(dz) &&
            dx*dx+dz*dz>g_sc_config.RecallEntryMarginMeters*g_sc_config.RecallEntryMarginMeters;
        int action=sc_recall_ready_step(&g_recall_ready,g_epoch,g_evidence_now,1,safe,
            g_caution==CAUTION_FORMED,g_order_locked,lap_ok,entry_ok,
            g_sc_config.RecallReadySeconds,g_sc_config.RecallWarningSeconds);
        if(action==1){hud_show("SAFETY CAR MAY PIT SOON");logf_("[recall-ready] pack/order/clearance ready; warning %.1fs",g_sc_config.RecallWarningSeconds);}
        if(action==2){logf_("[recall-ready] warning complete; native safe-entry return requested");sc_recall(0);}
        if(!safe && g_recall_ready.announced==0 && sc_clear_has_hazard(&g_clearance))hud_notice("SAFETY CAR STAYS OUT");
        static double last_recall_diagnostic=-1;
        if(g_recall_ready.elapsed>=g_sc_config.RecallWatchdogSeconds &&
           (last_recall_diagnostic<0||g_evidence_now-last_recall_diagnostic>=10)){
            last_recall_diagnostic=g_evidence_now;
            logf_("[recall-wait] safe=%d formed=%d order=%d laps=%d pit-marker=%d margin=%d; watchdog never forces GREEN",safe,g_caution==CAUTION_FORMED,g_order_locked,lap_ok,marker_ok,entry_ok);
        }

    }
}

/* The game's sector selector also dispatches native YELLOW01/02/03.  The
 * accepted incident's sector stays fixed while the pre-deployment HUD cycles. */
static int sc_incident_sector(int slot)
{
    typedef int (__attribute__((thiscall)) *SectorFn)(float*,float);
    float *track;
    float distance,length;
    int sector;
    if(slot<0||slot>=VEH_SLOTS||!slot_live(slot))return 0;
    track=*(float**)g_GameStatePtrArray3;
    if(!track||IsBadReadPtr(track,40))return 0;
    length=track[9];distance=V_F32(slot,OFF_LAPDIST);
    if(!isfinite(length)||length<100.0f||length>30000.0f||
       !isfinite(distance)||distance<0.0f||distance>length)return 0;
    sector=((SectorFn)FN_Float_Range_Index_Lookup)(track,distance);
    return sector>=0&&sector<=2?sector+1:0;
}

/* Re-arm every governed car every tick. Counts, but does not tolerate, any
 * value the game wrote in between - that count is the persistence evidence. */
#include "sc_exit_merge.h"
static void hold_pump(void)
{
    static SCExitMerge merge;
    int merge_leader = -1;
    float merge_cap = 99999.0f;
    int i;
    float target;

    if (!g_engaged) { sc_order_reset(&g_order_recovery); sc_exit_reset(&merge); return; }

    /* Belt and braces with the tick loop's release: never write a vehicle
     * struct outside green-flag racing. The loader rebuilds those structs. */
    if (!phase_racing()) return;

    /* XPITIN transferred acceleration to the native AI. Never recapture cars
       or reapply bunching/approach caps while waiting for the leader's line. */
    if (g_pit_handover && g_ct==CT_RESTART) {
        sc_order_reset(&g_order_recovery);sc_exit_reset(&merge);
        return;
    }

    /* Accumulate actual simulation time, including at different callback rates. */
    {
        double dt = SESSION_TIME - g_last_stime;
        if (isfinite(dt) && dt > 0.0) g_ramp_ms += dt * 1000.0;
        g_last_stime = SESSION_TIME;
    }

    /* GOVERN THE SAFETY CAR HIMSELF WHILE THE QUEUE FORMS.
     *
     * He was never given a speed before this: hold_pump()'s loop skips him and
     * only MEASURES him, so he ran at whatever his own car would do - which
     * after the CLK 55 rebuild is about 250 km/h. That is why the field
     * stretched under a 56 m/s pace.
     *
     * Safe against the hard rule about writing to him during the start
     * sequence: this is inside hold_pump(), which already requires g_engaged
     * and phase_racing(), and it only ever runs while he is deployed.      */
    if (sc_is_out() && g_sc_slot >= 0 && slot_live(g_sc_slot)) {
        int scv;
        if (g_gather_start == 0) {
            g_gather_start = sc_sim_millis(SESSION_TIME); g_gather_sim=SESSION_TIME;
            logf_("  [gather] safety car has the field - holding him at %d m/s "
                  "(%d km/h) until the queue is formed, or %d s",
                  g_gather_pace, g_gather_pace * 36 / 10, g_gather_secs);
        }
        scv = sc_commanded_pace();
        if (scv > 0) {
            if (scv != g_sc_cmd)
                logf_("  [gather] safety car pace %d -> %d m/s (%s)",
                      g_sc_cmd, scv,
                      (g_caution == CAUTION_FORMED) ? "queue formed"
                                                    : "gathering");
            set_maxspeed((void*)VEH(g_sc_slot), scv);
            g_sc_cmd = scv;
        } else if (g_sc_cmd) {
            /* The gather feature was switched off mid-caution. Hand his cap
             * back, or a stale g_sc_cmd pins him at the old pace for the rest
             * of the deployment (g_sc_cmd != 0 is what stops sc_pump's
             * ONTRACK case from releasing him).                             */
            set_maxspeed((void*)VEH(g_sc_slot), SPEED_RELEASE);
            logf_("  [gather] safety car released from the pace cap (gather "
                  "pace off)");
            g_sc_cmd = 0;
        }
    } else if (g_sc_cmd) {
        /* He has been recalled or the caution is over - hand his car back, or
         * he would drive to the pits on the gather cap.                     */
        if (g_sc_slot >= 0 && sc_slot_owned())
            set_maxspeed((void*)VEH(g_sc_slot), SPEED_RELEASE);
        logf_("  [gather] safety car released from the pace cap");
        g_sc_cmd       = 0;
        g_gather_start = 0;g_gather_sim=-1.0;
    }

    g_cmd_now = commanded_speed();
    /* Once he is out in front, HIS speed is the pace. The ramp still applies
     * on the way down, so a caution called at racing speed still decelerates
     * the field gently rather than at 15 m/s^2.                            */
    if (sc_is_out() && !g_pit_handover) {
        int measured = sc_pace_speed(g_cmd_now);
        if (measured < g_cmd_now) g_cmd_now = measured;
        else if (g_ramp_ms >= RAMP_SECONDS * 1000.0) g_cmd_now = measured;
    }

    /* Pit exit is a leader merge constraint, never a whole-field speed ceiling.
       Distant cars retain the existing predecessor-gap and saved-order control. */
    if (g_sc_state == SC_OUTLAP && sc_slot_owned()) {
        merge_leader = race_leader_slot();
        if (merge_leader >= 0 && slot_live(merge_leader))
            merge_cap = sc_exit_step(&merge, merge_leader, SESSION_TIME,
                V_F32(merge_leader, OFF_SPEED), sc_ahead_of_leader(),
                V_F32(g_sc_slot, OFF_SPEED));
        else sc_exit_reset(&merge);
    } else sc_exit_reset(&merge);

    if (merge_leader >= 0) {
        static double last_merge_log = -999.0;
        if (SESSION_TIME < last_merge_log || SESSION_TIME-last_merge_log >= 2.0) {
            last_merge_log = SESSION_TIME;
            logf_("[exit-merge] leader=%d gap=%.1fm sc=%.1fm/s leader_cap=%.1f normal=%d; followers use predecessor gaps",
                merge_leader, sc_ahead_of_leader(), V_F32(g_sc_slot,OFF_SPEED), merge_cap, g_cmd_now);
        }
    }
    target    = (float)g_cmd_now;

    if (g_bunch) {
        learn_lap_length();
        g_leader_slot = find_leader();
        /* The wave-around window is shut from the recall onward - a car
         * released then would still be alongside the queue at the green.  */
        if (g_wave_policy && g_ct < CT_IN && !g_order_recovery.active) wave_release_due();
        wave_pump();
    }
    order_recovery_snapshot();

    if (!phase_racing()) {           /* session ended or went out of racing */
        g_silent_release=1;
        release("race phase left 5");
        yellow_set(0);logf_("[hud] session ended; no race message");
        return;
    }

    for (i = 0; i < VEH_SLOTS; i++) {
        float cur;
        int   want;
        if (!g_armed[i]) continue;
        if (!capture_identity_owned(i)) { g_armed[i] = 0; continue; }
        if (!capture_owned(i)) {
            /* This identity legitimately finished/retired. Restore its loan
               once, but never treat it as an eligible new pace participant. */
            ai_restore_passing(i);
            if(!pit_owned(i) && g_saved[i]>0.0f && g_saved[i]<2.0e9f) {
                int restore=(int)(g_saved[i]+0.5f);
                if(restore==SPEED_IDLE_SENTINEL)restore=SPEED_IDLE_SENTINEL+1;
                set_maxspeed((void*)VEH(i),restore);
            }
            g_armed[i]=0;continue;
        }
        if (i == g_sc_slot && sc_is_out()) continue;   /* he sets the pace */
        if (pit_owned(i)) {          /* entered the pits - hand it back */
            ai_restore_passing(i);
            g_armed[i] = 0;
            logf_("  [pit] slot %2d entered the pit lane, released to the game "
                  "(pittingStatus=%d)", i, V_I32(i, OFF_PITTING));
            continue;
        }

        /* Release only a nearly stationary car that fails to move despite an
           adequate command for 12 simulation seconds. Pauses do not count. */
        if (sc_stuck_step(&g_stuck_watch[i], g_evidence_now,
                          V_F32(i, OFF_SPEED), g_last_written[i])) {
            if (g_ct == CT_RESTART) {
                /* A stalled car cannot become a pre-GREEN escapee.  Preserve
                   its loan, record the fault, and let a later valid sample
                   rejoin the controlled field before whole-field release. */
                g_restart_hold=1;
                hud_notice("GREEN FLAG DELAYED: STOPPED CAR");
                logf_("[restart] slot %d stationary; restriction retained",i);
            } else {
                restart_free_car(i, "stationary despite movement command for 12 simulation seconds");
            }
            memset(&g_stuck_watch[i], 0, sizeof(g_stuck_watch[i]));
            continue;
        }

        /* Before handover, retain normal caution control. The early return
           above prevents this cap and the join loop after XPITIN. */
        want = bunch_speed_for(i, g_cmd_now);
        if (i == merge_leader && merge_cap < (float)want) {
            want = (int)ceilf(merge_cap);
            if (want == SPEED_IDLE_SENTINEL) want--;
        }
        target = (float)want;

        cur = V_F32(i, OFF_MAXSPEED);
        if (cur != target) {
            /* A GAME write is one that differs from what WE last put there.
             * A difference that merely matches our previous command is the
             * ramp (or the bunching controller) moving, not the game fighting
             * us.                                                          */
            if (cur != g_last_written[i]) {
                g_overrides++;
                logf_("  [override] slot %2d maxSpeed was %.1f, we last wrote "
                      "%.1f - game wrote it, re-arming to %d",
                      i, cur, g_last_written[i], want);
            }
            set_maxspeed((void*)VEH(i), want);
            g_last_written[i] = target;
        }
    }

    /* A car that joins the field mid-hold (rare, but a slot can go live) is
     * picked up here rather than being left ungoverned.                     */
    for (i = 0; i < VEH_SLOTS; i++) {
        /* A pit-lane car is native-owned only while it is in the lane.  On
           exit it must rejoin the still-restricted field, including during
           restart.  g_restart_free marks the only released-this-cycle cars. */
        if (g_restart_free[i]) {
            if(g_green_said || !slot_governed(i) || !isfinite(V_F32(i,OFF_SPEED)) ||
               V_F32(i,OFF_SPEED)<OBSTACLE_MIN)continue;
            g_restart_free[i]=0; /* recovered exemption rejoins before GREEN */
        }
        if (g_armed[i] || !slot_governed(i)) continue;
        /* Not a wreck, or we would re-capture what the stuck check just let go
         * and write maxSpeed onto it again on the very next tick.          */
        if (V_F32(i, OFF_SPEED) < OBSTACLE_MIN) continue;
        if (!capture_car(i)) continue;
        memset(&g_stuck_watch[i], 0, sizeof(g_stuck_watch[i]));
        {
            int want = bunch_speed_for(i, g_cmd_now);
        if (i == merge_leader && merge_cap < (float)want) {
            want = (int)ceilf(merge_cap);
            if (want == SPEED_IDLE_SENTINEL) want--;
        }
            set_maxspeed((void*)VEH(i), want);
            g_last_written[i] = (float)want;
        }
        logf_("  [mid-hold] slot %2d joined, saved maxSpeed=%.1f", i, g_saved[i]);
    }
}

/* ---- reporting --------------------------------------------------------- */

static void dump_field(void)
{
    int i, live = 0, gov = 0;

    logf_("--- FIELD DUMP  phase=%d session=%d SessionTime=%.2f engaged=%d "
          "pace=%d m/s include_player=%d ---",
          RACE_PHASE, SESSION_TYPE, SESSION_TIME, g_engaged, g_pace_speed,
          g_include_player);
    for (i = 0; i < VEH_SLOTS; i++) {
        if (!slot_live(i)) continue;
        live++;
        if (g_armed[i]) gov++;
        logf_("  slot %2d P%-2d act=%d pit=%d %-28.28s speed=%6.1f "
              "maxSpeed=%9.1f %s",
              i, V_I32(i, OFF_POSITION), V_I32(i, OFF_ACTIVE),
              V_I32(i, OFF_PITTING), V_STR(i, OFF_NAME),
              V_F32(i, OFF_SPEED), V_F32(i, OFF_MAXSPEED),
              g_armed[i] ? "<GOV>" : "");
    }
    logf_("  %d live, %d governed, %ld overrides so far", live, gov, g_overrides);
    /* Field counts remain in the diagnostic log, not the race HUD. */
}

/* One-second summary while engaged. Two measurements, both Phase 5 gates:
 *
 *   SPEED  - does the field hold the commanded pace? Only MOVING cars count.
 *            The first runs reported "at-or-below 2/21" which looked like a
 *            failure and was not: the 2 were stopped cars (speed 0.0) and the
 *            other 19 were obeying. Stopped cars are excluded here and counted
 *            separately.
 *   ORDER  - does the field hold its order? Race position (+0x32D0) is compared
 *            against the position each car had at engage. This is the half of
 *            the gate that cannot be read off a speed trace, and having it in
 *            the log means it is measured rather than eyeballed.             */

#include "f1c_ai_interaction.h"
/* Observe before hold_pump can release/reacquire controls. Never repair here. */
static void ai_interaction_audit(void)
{
    static DWORD last;
    static double previous_sim;
    static int have_previous, cursor;
    static double max_delta;
    static unsigned samples;
    if(!on_game_thread() || !field_stable() || !phase_racing() || !g_engaged){
        have_previous=0;max_delta=0;samples=0;return;
    }
    double sim=SESSION_TIME;
    if(!isfinite(sim)){have_previous=0;return;}
    if(have_previous && sim>=previous_sim){
        double delta=sim-previous_sim;if(delta>max_delta)max_delta=delta;
    }
    previous_sim=sim;have_previous=1;samples++;
    DWORD now=GetTickCount();if(now-last<1000)return;last=now;
    int drift=0, valid=0, emitted=0;
    for(int k=0;k<VEH_SLOTS;k++){
        int i=(cursor+k)%VEH_SLOTS;
        if(i==g_sc_slot || !g_armed[i] || !capture_owned(i) || pit_owned(i))continue;
        F1CAIInteraction a;
        if(!f1c_ai_interaction_read((const void*)VEH(i),0x48C0u,&a))continue;
        valid++;
        int changed=g_wave[i]!=WAVE_RUNNING && fabsf(a.passing-PASSING_MIN)>0.001f;
        drift+=changed;
        if(emitted>=8)continue;
        int target=-1;
        /* Identity comparison only: untrusted native pointer is never followed. */
        for(int j=0;j<VEH_SLOTS;j++)if(a.target_address==(uint32_t)VEH(j) &&
            slot_live(j) && (j==g_sc_slot?sc_slot_owned():capture_owned(j))){target=j;break;}
        logf_("[ai-audit] sim=%.3f slot=%d rank=%d saved=%d lap=%d speed=%.2f cap=%.2f passing=%.4f expected=%.4f drift=%d target=%d target_present=%d lateral=%.3f",
            sim,i,V_I32(i,OFF_POSITION),g_pos0[i],V_I32(i,OFF_LAPNUM),
            V_F32(i,OFF_SPEED),g_last_written[i],a.passing,
            g_wave[i]==WAVE_RUNNING?g_saved_pass[i]:PASSING_MIN,changed,target,
            a.target_address!=0,a.lateral_request);
        emitted++;
    }
    cursor=(cursor+8)%VEH_SLOTS;
    logf_("[ai-audit-summary] sim=%.3f valid=%d passing_drift=%d samples=%u max_sim_gap=%.3f details=%d",
        sim,valid,drift,samples,max_delta,emitted);
    samples=0;max_delta=0;
}

/* Formation is race-control evidence, sampled on every advancing simulation
   tick before recall decisions.  Telemetry only reports it and cannot own it. */
static void formation_pump(void)
{
    static unsigned seen_epoch; static int seen_ct,seen_leader;
    static double last_sim=-1.0, stable_since=-1.0;
    int i,total=0,close=0,missing=0,leader;
    if(!g_engaged || g_ct<CT_DEPLOY || g_ct>CT_PACE || !phase_racing() || !isfinite(g_evidence_now)) goto bad;
    leader=find_leader();
    if(leader<0 || !slot_live(leader)) goto bad;
    if(seen_epoch!=g_epoch || seen_ct!=g_ct || seen_leader!=leader || last_sim<0 || g_evidence_now<=last_sim || g_evidence_now-last_sim>2.0) goto reset;
    for(i=0;i<VEH_SLOTS;i++){
        float gap;
        if(i==g_sc_slot)continue;
        /* A previously governed identity that became unobservable is missing
           evidence, not permission to shrink the pack denominator. */
        if(g_armed[i] && !slot_participant(i)){total++;missing++;continue;}
        if(!slot_participant(i) || pit_owned(i))continue;
        total++;
        if(i==leader)continue;
        if(leader==g_sc_slot && queue_index(i)==1) {
            /* SC-front queue: the race leader's predecessor is the SC, which
               is deliberately outside g_armed. */
            gap=behind_leader(i);
            if(!isfinite(gap)||gap<0.0||gap>g_target_gap*3.0f){missing++;continue;}
            close++;continue;
        }
        gap=gap_ahead(i);
        if(!isfinite(gap)||gap<0.0){missing++;continue;}
        if(gap<=g_target_gap*1.5f)close++;else missing++;
    }
    last_sim=g_evidence_now;
    if(total<1 || missing || close!=total-(leader==g_sc_slot?0:1)){stable_since=-1.0;g_caution=CAUTION_FORMING;return;}
    if(stable_since<0)stable_since=g_evidence_now;
    g_caution=(g_evidence_now-stable_since>=3.0)?CAUTION_FORMED:CAUTION_FORMING;
    return;
reset:
    seen_epoch=g_epoch;seen_ct=g_ct;seen_leader=leader;last_sim=g_evidence_now;stable_since=-1.0;g_caution=CAUTION_FORMING;return;
bad:
    last_sim=-1.0;stable_since=-1.0;seen_epoch=g_epoch;seen_ct=g_ct;seen_leader=-1;
    if(g_engaged)g_caution=CAUTION_FORMING;
}

static void telemetry_pump(void)
{
    static DWORD last = 0;
    static double last_order_log = -999.0;
    static double formation_since = -1.0;
    DWORD now = GetTickCount();
    int i, n = 0, obey = 0, stopped = 0, swapped = 0;
    float lo = 1e9f, hi = -1e9f, sum = 0.0f;

    if (now - last < 1000) return;
    last = now;
    if (!g_engaged) return;

    for (i = 0; i < VEH_SLOTS; i++) {
        float s;
        if (!g_armed[i] || !slot_live(i)) continue;

        if (V_I32(i, OFF_POSITION) != g_pos0[i]) swapped++;

        s = V_F32(i, OFF_SPEED);
        if (s < 0.5f) { stopped++; continue; }     /* parked/retired, not paced */
        if (s < lo) lo = s;
        if (s > hi) hi = s;
        sum += s;
        n++;
        if (s <= (float)g_cmd_now + 1.0f) obey++;
    }
    if (!n) return;

    logf_("  t+%5.1fs (sim %6.1f)  moving=%2d stopped=%d  cmd=%2d%s  speed "
          "lo=%5.1f mean=%5.1f hi=%5.1f  at-or-below=%d/%d  pos-changed=%d  "
          "overrides=%ld",
          (GetTickCount() - g_engage_tick) / 1000.0, session_elapsed(),
          n, stopped, g_cmd_now,
          (g_cmd_now != g_pace_speed) ? " (ramping)" : "        ",
          lo, sum / n, hi, obey, n, swapped, g_overrides);

    /* Bunching: the pack length is the whole measurement. If the controller
     * works, total shrinks toward (cars-1) * target_gap and mean gap toward
     * target_gap. Logged every second so convergence is a curve, not a claim. */
    if (g_bunch) {
        float gmin = 1e9f, gmax = -1e9f, gsum = 0.0f;
        int   gn = 0, closed = 0;
        int   on_slot = 0, ahead_of_leader = 0, sn = 0, waving = 0, expected = 0, missing = 0;
        float errsum = 0.0f, errmax = 0.0f, tail = 0.0f;
        float formation_head = -1.0f;
        int sc_front = sc_is_out() && g_leader_slot == g_sc_slot;
        if(sc_front) for(i=0;i<VEH_SLOTS;i++) {
            if(i==g_sc_slot || !g_armed[i] || !slot_live(i) || !slot_moving(i) ||
               g_wave[i]==WAVE_RUNNING || queue_index(i)!=1) continue;
            formation_head=behind_leader(i);break;
        }


        for (i = 0; i < VEH_SLOTS; i++) {
            float g;
            if (!g_armed[i] || !slot_live(i)) continue;
            g = gap_ahead(i);
            if (g < 0.0f) continue;
            if (g < gmin) gmin = g;
            if (g > gmax) gmax = g;
            gsum += g;
            gn++;
            if (g <= (float)g_target_gap * 1.5f) closed++;
        }

        /* Slot error is the measurement that matters now: how far each car is
         * from ITS OWN place in the queue. Order is correct when every car is
         * near its slot, which the gap statistics above cannot tell you - a
         * perfectly spaced queue in the wrong order looks identical to them. */
        for (i = 0; i < VEH_SLOTS; i++) {
            float behind, err;
            if (!g_armed[i] || !slot_live(i) || i == g_leader_slot) continue;
            if (pit_owned(i)) continue; /* native lane ownership, not queue evidence */
            expected++;
            if (!slot_moving(i)) { missing++; continue; }
            if (g_wave[i] == WAVE_RUNNING) { waving++;missing++; continue; }
            behind = behind_leader(i);
            if (behind <= -1e8f) { missing++;continue; }
            if (behind < 0.0f) { ahead_of_leader++; continue; }
            err = sc_front ? sc_formation_error(behind,queue_index(i),formation_head,(float)g_target_gap)
                           : behind - (float)(queue_index(i) * g_target_gap);
            if (err < 0.0f) err = -err;
            errsum += err;
            if (err > errmax) errmax = err;
            if (err <= (float)g_target_gap) on_slot++;
            if (behind > tail) tail = behind;
            sn++;
        }

        if (sn) {
            logf_("        QUEUE on-slot=%d/%d  ahead-of-leader=%d  waving=%d  "
                  "slot-err mean=%6.1f max=%6.1f  tail=%6.0fm",
                  on_slot, sn, ahead_of_leader, waving, errsum / sn, errmax, tail);

            /* The queue counts as formed once four fifths of the running cars
             * are on their slot. Cars mid-wave are excluded from sn already,
             * so a wave-around does not drag the state backwards.          */
            {
                int instant = expected>0 && sn==expected && on_slot==expected &&
                    missing==0 && ahead_of_leader==0 &&
                    (!sc_front || sc_formation_head_ready(formation_head,(float)g_target_gap));
                if(!instant || !isfinite(SESSION_TIME)) formation_since=-1.0;
                else if(formation_since<0.0) formation_since=SESSION_TIME;
                int formed=instant && SESSION_TIME-formation_since>=3.0;
                int want   = formed ? CAUTION_FORMED : CAUTION_FORMING;
                if (0 && want != g_caution) { /* reporting-only legacy calculation; formation_pump owns state */
                    g_caution = want;
                    logf_("=== CAUTION %s === on-slot %d/%d, pack tail %.0f m",
                          formed ? "FORMED - full fresh pack stable"
                                 : "FORMING - full-pack evidence incomplete",
                          on_slot, sn, tail);
                }
            }
        }
        /* WHY IS A CAR A LONG WAY BACK STILL SLOW?
         *
         * Driver report 2026-08-02. The aggregate lines above cannot answer it:
         * they report gaps and slot errors, not which branch of
         * bunch_speed_for() produced the command. So print one line per car
         * that is BOTH well behind the queue and NOT on clear air - i.e. every
         * car the report could possibly be about - naming the branch, the
         * obstacle it is capped on and that obstacle's speed.
         *
         * FAR_BACK_M is deliberately larger than APPROACH_RANGE is useful at:
         * beyond ~180 m the ramp already allows more than race pace, so any
         * car listed here is capped by a branch other than a healthy approach,
         * or by an obstacle that has no business being one.                 */
        {
            const float FAR_BACK_M = 250.0f;
            for (i = 0; i < VEH_SLOTS; i++) {
                float behind;
                int   ob;
                if (!g_armed[i] || !slot_live(i) || i == g_leader_slot) continue;
                if (!slot_moving(i)) continue;
                if (g_cap_why[i] == CAPW_NONE || g_cap_why[i] == CAPW_WAVE)
                    continue;
                behind = behind_leader(i);
                if (behind > -1e8f && behind >= 0.0f && behind < FAR_BACK_M)
                    continue;      /* in or near the queue: capping is correct */
                ob = g_cap_obst[i];
                logf_("        FARCAP slot %2d '%s' behind=%7.0fm cmd=%.0f "
                      "speed=%5.1f why=%s gap=%6.1f obstacle=%s",
                      i, V_STR(i, OFF_NAME), behind,
                      g_last_written[i], V_F32(i, OFF_SPEED),
                      cap_why_name(g_cap_why[i]), g_cap_gap[i],
                      (ob >= 0) ? V_STR(ob, OFF_NAME) : "none");
                if (ob >= 0)
                    logf_("               obstacle slot %2d speed=%5.1f "
                          "floor=%5.1f lapdist=%8.1f pit=%d",
                          ob, V_F32(ob, OFF_SPEED), speed_floor(ob),
                          V_F32(ob, OFF_LAPDIST), V_I32(ob, OFF_PITTING));
            }
        }

        if (gn)
            /* With a PHYSICAL chain every car points at its nearest neighbour
             * ahead, so the gaps close the loop and their sum is always one
             * lap - useless as a progress measure. The measure that does move
             * is the arc the field OCCUPIES: one lap minus the biggest hole.
             * A bunched field occupies a short arc; a strung-out one occupies
             * nearly the whole lap.                                        */
            logf_("        BUNCH leader=slot%d lap_len=%.0fm target=%dm  "
                  "gaps n=%d min=%5.1f mean=%6.1f max=%6.1f  "
                  "field-arc=%6.0fm  within-1.5x=%d/%d",
                  g_leader_slot, g_lap_len, g_target_gap,
                  gn, gmin, gsum / gn, gmax,
                  (double)g_lap_len - (double)gmax, closed, gn);
        else
            logf_("        BUNCH no measurable gaps yet (lap_len=%.0f)", g_lap_len);
    }

    /* The full order, every 10 s, so a swap can be attributed to cars rather
     * than just counted.                                                     */
    if (session_elapsed() - last_order_log >= 10.0) {
        char line[512];
        int  used = 0, p;
        last_order_log = session_elapsed();
        for (p = 1; p <= 30 && used < (int)sizeof(line) - 12; p++) {
            for (i = 0; i < VEH_SLOTS; i++) {
                if (!g_armed[i] || !slot_live(i)) continue;
                if (V_I32(i, OFF_POSITION) != p) continue;
                used += _snprintf(line + used, sizeof(line) - used - 1,
                                  "%d%s ", i, (g_pos0[i] != p) ? "*" : "");
                break;
            }
        }
        line[used > 0 ? used : 0] = '\0';
        logf_("        order by slot (P1..): %s   (* = moved since engage)", line);
    }

    /* Keep the lifecycle label visible; engineering details remain in logs. */
    if (g_ct == CT_RESTART)
        hud_notice(g_green_said ? "GREEN FLAG" : "SAFETY CAR IN PITS");
    else if (g_ct == CT_IN || g_sc_state == SC_RECALL)
        hud_notice("SAFETY CAR IN THIS LAP");
    else if (g_sc_state == SC_OUTLAP)
        hud_notice("SAFETY CAR LEAVING PITS");
    else if (g_sc_state == SC_CATCHUP || g_sc_state == SC_ONTRACK)
        hud_notice("SAFETY CAR ON TRACK");
    else
        hud_notice("SAFETY CAR");
}

/* ---- hotkeys ----------------------------------------------------------- */
/* Two live keys, and five that do nothing until the menu is open.
 *
 * Everything used to be a global hotkey - Num3..Num0, Num*, Num/, Num., Home,
 * End, Insert, PgUp, PgDn, Left, Del, and the arrows. That is unusable while
 * driving: any of them fires instantly, some of them irreversibly, and there is
 * no way to see what state you are in. They are all rows in the menu now.
 *
 * Numpad 1 and 2 are deliberately NOT bound. They belong to the red-flag DLL,
 * which is a separate, deliberate injection and is no longer part of the mod
 * pack (owner's decision, 2026-08-01).                                       */
static int HOTKEYS[] = {
    /* NOT VK_HOME: Home is one of the game's own camera binds (driver report
     * 2026-08-01), so pressing it moved the view as well as deploying the
     * safety car. Numpad 0 is free - the game leaves the numpad to the
     * spectate keys Num+ and Num-, which are avoided here.                  */
    VK_NUMPAD1, /* FORK: bound but inert - the extra car is parked by the load
                 * mechanism, see handle_key.                              */
    73, /* emergency parked reset (INI configurable) */
    85  /* deploy the safety car / bring him in and restart. The only
                 * key this DLL binds - everything else is in the external
                 * control window, tools/safety_car_gui.py.                  */
};
#define NKEYS ((int)(sizeof(HOTKEYS) / sizeof(HOTKEYS[0])))

static void set_pace(int v)
{
    if (v < PACE_MIN) v = PACE_MIN;
    if (v > PACE_MAX) v = PACE_MAX;
    /* 18 would be read as "use IdleSpeedTarget", not as 18 m/s. */
    if (v == SPEED_IDLE_SENTINEL) v = (v > g_pace_speed) ? 19 : 17;
    g_pace_speed = v;
    logf_("pace speed = %d m/s (%.0f km/h)%s", v, v * 3.6,
          g_engaged ? " [applied on next tick]" : "");
    /* The configured pace is already logged above in m/s and km/h. */
}

/* ---- the control block, pumped once per tick --------------------------- */

static void ctl_init(void)
{
    g_shm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                               0, sizeof(PaceCtl), SHM_NAME);
    if (!g_shm) { logf_("CreateFileMapping FAILED - no external control"); return; }
    g_ctl = (PaceCtl*)MapViewOfFile(g_shm, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PaceCtl));
    if (!g_ctl) { logf_("MapViewOfFile FAILED - no external control"); return; }

    memset((void*)g_ctl, 0, sizeof(PaceCtl));
    g_ctl->magic              = CTL_MAGIC;
    g_ctl->set_pace_speed     = g_pace_speed;
    g_ctl->set_target_gap     = g_target_gap;
    g_ctl->set_gather_pace    = g_gather_pace;
    g_ctl->set_gather_secs    = g_gather_secs;
    g_ctl->set_caution_laps   = g_ct_laps_tgt;
    g_ctl->set_wave_policy    = g_wave_policy;
    g_ctl->set_bunch          = g_bunch;
    g_ctl->set_auto_caution   = g_ct_auto;
    g_ctl->set_include_player = g_include_player;
    g_ctl->set_ramp           = g_ramp;
    g_ctl->set_sc_park        = g_sc_park;
    g_ctl->set_hide_mode      = g_hide_mode;
    g_ctl->set_telemetry      = g_telemetry;
    g_ctl->set_quick_laps     = g_ct_quick_laps;
    g_ctl->set_full_thresh    = g_ct_full_thresh;
    g_ctl->set_final_lap      = g_ct_final_gate;
    g_ctl->set_grace_secs     = g_ct_grace_secs;
    g_ctl->set_hold_secs      = g_ct_hold_secs;
    g_ctl->set_pitstart       = g_pitstart;
    g_ctl->abi_version        = 1;
    g_ctl->struct_size        = (unsigned)sizeof(PaceCtl);
    logf_("control block '%s' ready (%d bytes) - run "
          "tools/safety_car_gui.py", SHM_NAME, (int)sizeof(PaceCtl));
}

static void ctl_pump(void)
{
    int i, live = 0, governed = 0;

    if (!g_ctl || g_ctl->magic != CTL_MAGIC) return;

    /* --- commands. Cleared here so a GUI that dies mid-write cannot leave one
     * latched and firing every tick.                                        */
    if (g_ctl->cmd_safetycar) {
        g_ctl->cmd_safetycar = 0;
        if (sc_is_out() || g_sc_state == SC_ARMING ||
            g_sc_state == SC_OUTLAP || g_sc_state == SC_CATCHUP) sc_recall(1);
        else {sc_deploy();if(g_sc_state!=SC_IDLE)g_emergency_inhibit=0;}
    }
    if (g_ctl->cmd_engage)  { g_ctl->cmd_engage  = 0; engage(); }
    if (g_ctl->cmd_release) { g_ctl->cmd_release = 0; release("gui"); }
    if (g_ctl->cmd_panic)   {
        g_ctl->cmd_panic = 0;
        sc_emergency_reset();
        return;
    }
    if (g_ctl->cmd_dump)    { g_ctl->cmd_dump = 0; dump_field(); }
    if (g_ctl->cmd_yellow)  {
        int want = (g_ctl->cmd_yellow == 1);
        g_ctl->cmd_yellow = 0;
        yellow_set(want);
    }

    /* --- settings.
     *
     * TWO WRITERS, AND THE SHADOW IS WHAT KEEPS THEM APART.
     *
     * The first version compared the GUI's value against the DLL's LIVE state
     * and applied it whenever they differed. That is not "apply on change" -
     * it re-applies every tick for as long as they disagree, so any setting the
     * DLL changes by itself is dragged back by the window within 50 ms.
     *
     * It cost a live run on 2026-08-01: sc_deploy() turns bunching ON, the
     * window still held its default OFF, and the DLL switched it off again on
     * the very next tick. Bunching is what calls learn_lap_length() and
     * find_leader(), so lap_len stayed 0 and the leader stayed -1, the gap to
     * the leader was therefore unmeasurable, and the safety car sat in the
     * catch-up phase running uncapped with no way to ever finish it.
     *
     * The fix is a shadow copy of what the GUI last showed. A setting is only
     * pushed into the DLL when the shared block differs from the SHADOW - that
     * is the window actually being clicked. Everything else flows the other
     * way: the DLL's live value is published into both, so the window follows
     * the DLL rather than fighting it.                                      */
    {
        static PaceCtl shadow;
        static int     shadow_init = 0;

        if (!shadow_init) { shadow = *g_ctl; shadow_init = 1; }

#define GUI_CHANGED(f)  (g_ctl->f != shadow.f)
#define PUBLISH(f, v)   do { g_ctl->f = (int)(v); shadow.f = (int)(v); } while (0)

        if (GUI_CHANGED(set_pace_speed) && g_ctl->set_pace_speed > 0)
            set_pace(g_ctl->set_pace_speed);
        if (GUI_CHANGED(set_target_gap) && g_ctl->set_target_gap >= 5)
            g_target_gap = g_ctl->set_target_gap;
        /* 0 turns the gather pace off entirely and he runs free, which is the
         * behaviour every build before 2026-08-02 had. */
        if (GUI_CHANGED(set_gather_pace) && g_ctl->set_gather_pace >= 0 &&
            g_ctl->set_gather_pace < 120)
            g_gather_pace = g_ctl->set_gather_pace;
        if (GUI_CHANGED(set_gather_secs) && g_ctl->set_gather_secs >= 5)
            g_gather_secs = g_ctl->set_gather_secs;
        if (GUI_CHANGED(set_caution_laps) && g_ctl->set_caution_laps >= 1)
            g_ct_laps_tgt = g_ctl->set_caution_laps;
        if (GUI_CHANGED(set_wave_policy))
            wave_set_policy(0);
        if (GUI_CHANGED(set_bunch)) {
            g_bunch = g_ctl->set_bunch ? 1 : 0;
            if (g_bunch) { learn_lap_length(); g_leader_slot = find_leader(); }
        }
        if (GUI_CHANGED(set_auto_caution)) {
            g_ct_auto    = g_ctl->set_auto_caution ? 1 : 0;
            g_ct_seen_ms = 0;
        }
        if (GUI_CHANGED(set_include_player) && !g_engaged)
            g_include_player = g_ctl->set_include_player ? 1 : 0;
        if (GUI_CHANGED(set_ramp))      g_ramp      = g_ctl->set_ramp ? 1 : 0;
        if (GUI_CHANGED(set_telemetry)) g_telemetry = g_ctl->set_telemetry ? 1 : 0;
        if (GUI_CHANGED(set_sc_park))   g_sc_park = 1;
        if (GUI_CHANGED(set_hide_mode)) {
            g_hide_mode = 1;
            if (!g_hide_mode) sc_unhide();
        }
        if (GUI_CHANGED(set_hush)) g_hush_want = g_ctl->set_hush ? 1 : 0;

        if (GUI_CHANGED(set_pitstart)) {
            g_pitstart = g_ctl->set_pitstart ? 1 : 0;
            g_pitstart_st = PITSTART_IDLE;
            g_pitstart_fix = 0;
        }

        /* The NT2004 gate settings. Each one is clamped here rather than in
         * the window, so a hand-written value poked into the block cannot
         * disable a gate by going out of range.                            */
        if (GUI_CHANGED(set_quick_laps) && g_ctl->set_quick_laps >= 1 &&
            g_ctl->set_quick_laps <= 20)
            g_ct_quick_laps = g_ctl->set_quick_laps;
        if (GUI_CHANGED(set_full_thresh) && g_ctl->set_full_thresh >= 1 &&
            g_ctl->set_full_thresh <= 20)
            g_ct_full_thresh = g_ctl->set_full_thresh;
        if (GUI_CHANGED(set_final_lap))
            g_ct_final_gate = g_ctl->set_final_lap ? 1 : 0;
        if (GUI_CHANGED(set_grace_secs) && g_ctl->set_grace_secs >= 0 &&
            g_ctl->set_grace_secs <= 120)
            g_ct_grace_secs = g_ctl->set_grace_secs;
        if (GUI_CHANGED(set_hold_secs) && g_ctl->set_hold_secs >= 1 &&
            g_ctl->set_hold_secs <= 60)
            g_ct_hold_secs = g_ctl->set_hold_secs;

        /* Now publish the DLL's truth back, so a value the DLL changed by
         * itself (or clamped, or refused) shows in the window instead of
         * being fought over.                                                */
        PUBLISH(set_pace_speed,     g_pace_speed);
        PUBLISH(set_target_gap,     g_target_gap);
        PUBLISH(set_gather_pace,    g_gather_pace);
        PUBLISH(set_gather_secs,    g_gather_secs);
        PUBLISH(set_caution_laps,   g_ct_laps_tgt);
        PUBLISH(set_wave_policy,    g_wave_policy);
        PUBLISH(set_bunch,          g_bunch);
        PUBLISH(set_auto_caution,   g_ct_auto);
        PUBLISH(set_include_player, g_include_player);
        PUBLISH(set_ramp,           g_ramp);
        PUBLISH(set_sc_park,        g_sc_park);
        PUBLISH(set_hide_mode,      g_hide_mode);
        PUBLISH(set_telemetry,      g_telemetry);
        PUBLISH(set_hush,           g_hush_want);
        PUBLISH(set_pitstart,       g_pitstart);
        PUBLISH(set_quick_laps,     g_ct_quick_laps);
        PUBLISH(set_full_thresh,    g_ct_full_thresh);
        PUBLISH(set_final_lap,      g_ct_final_gate);
        PUBLISH(set_grace_secs,     g_ct_grace_secs);
        PUBLISH(set_hold_secs,      g_ct_hold_secs);
#undef GUI_CHANGED
#undef PUBLISH
    }

    /* --- published state */
    for (i = 0; i < VEH_SLOTS; i++) {
        if (V_I32(i, OFF_ACTIVE) != -1) live++;
        if (g_armed[i]) governed++;
    }
    g_ctl->ct_state         = g_ct;
    g_ctl->sc_state         = g_sc_state;
    g_ctl->caution_phase    = g_caution;
    g_ctl->laps_done        = g_ct_laps_done;
    g_ctl->engaged          = g_engaged;
    g_ctl->cars_governed    = governed;
    g_ctl->cmd_now          = g_cmd_now;
    g_ctl->leader_slot      = g_leader_slot;
    g_ctl->sc_slot          = g_sc_slot;
    g_ctl->sc_hidden        = g_hidden;
    g_ctl->yellow           = g_yellow;
    g_ctl->zones_yellow     = zones_yellow(NULL);
    g_ctl->race_phase       = RACE_PHASE;
    g_ctl->session_type     = SESSION_TYPE;
    g_ctl->live_cars        = live;
    g_ctl->lap_len          = g_lap_len;
    g_ctl->sc_gap_to_leader = (g_sc_slot >= 0) ? sc_ahead_of_leader() : 0.0f;
    g_ctl->sc_ready         = sc_ready(g_sc_slot >= 0 ? g_sc_slot : find_safety_car());
    /* The watcher's trace. g_trig is refreshed by caution_pump() every tick;
     * publishing it here rather than re-evaluating keeps the panel showing
     * exactly the numbers the decision was made on.                        */
    g_ctl->trig_gates       = g_trig.gates;
    g_ctl->trig_zones       = g_trig.zones;
    g_ctl->trig_stopped     = g_trig.stopped;
    g_ctl->trig_offpace     = g_trig.offpace;
    g_ctl->trig_involved    = g_trig.involved;
    g_ctl->trig_hold_ms     = g_trig.hold_ms;
    g_ctl->trig_hold_req    = g_trig.hold_req;
    g_ctl->trig_pct         = g_trig.pct;
    g_ctl->trig_worst_slot  = g_trig.worst_slot;
    g_ctl->trig_worst_speed = g_trig.worst_speed;
    g_ctl->trig_field_speed = g_trig.field_speed;
    g_ctl->trig_leader_lap  = g_trig.leader_lap;
    g_ctl->trig_total_laps  = g_trig.total_laps;
    g_ctl->trig_severity    = g_trig.severity;
    g_ctl->trig_grace_left  = g_trig.grace_left;
    memcpy((void*)g_ctl->trig_worst_name, g_trig.worst_name,
           sizeof(g_ctl->trig_worst_name));

    /* A NEW TRACK INVALIDATES EVERY COORDINATE WE HOLD.
     *
     * They were resolved for the old one, and using them on the new track puts
     * the car wherever those numbers happen to land - off the mesh, in the
     * measured case. Dropped here, and the GUI re-publishes within a tick or
     * two; until it does, the pit-box start is inert rather than wrong.   */
    if (InterlockedExchange(&g_track_changed, 0)) {
        reset_track_history();
        g_app_ok = 0;
        g_box_ok = 0;
        g_ctl->app_valid = 0;
        g_ctl->box_valid = 0;
        logf_("  [track] now %s - dropped the previous track's pit "
              "coordinates, waiting for the GUI to resolve the new ones",
              g_track_aiw);
    }

    /* The GUI owns the box coordinates - it has aiw_pits.json and we do not.
     * Taken as given, never written back. */
    if (g_ctl->box_valid) {
        g_box[0] = g_ctl->box_x;
        g_box[1] = g_ctl->box_y;
        g_box[2] = g_ctl->box_z;
        g_box[3] = g_ctl->box_yaw;
        g_box_ok = 1;
    }
    /* Same rule for the approach point: the GUI has aiw_pits.json, we do not.
     * If it is not running app_valid stays 0 and the pit-box start is INERT
     * rather than wrong. */
    if (g_ctl->app_valid) {
        g_app[0] = g_ctl->app_x;
        g_app[1] = g_ctl->app_y;
        g_app[2] = g_ctl->app_z;
        g_app[3] = g_ctl->app_yaw;
        g_app_ok = 1;
    }
    {
        int sc = (g_sc_slot >= 0) ? g_sc_slot : find_safety_car();
        g_ctl->sc_team = (sc >= 0 && slot_live(sc)) ? V_I32(sc, OFF_PITTEAM) : -1;
    }
    g_ctl->pitstart_state = g_pitstart_st;
    g_ctl->pitstart_fixes = g_pitstart_fix;
    memcpy((void*)g_ctl->track_aiw, g_track_aiw, sizeof(g_ctl->track_aiw));

    g_ctl->cars_inlap       = g_pit_inlap;
    g_ctl->cars_in_pits     = g_pit_inpits;
    g_ctl->pitstops_caution = g_pit_stops;

    g_ctl->hush_armed       = g_hush_on;
    g_ctl->hush_drops       = (int)(g_hush_drops + g_hush_tdrops);
    g_ctl->heartbeat++;
}

static void sc_key_name(int vk,char *out){
    if(vk>=48 && vk<=90){out[0]=(char)vk;out[1]=0;}
    else if(vk>=96 && vk<=105)wsprintfA(out,"NUM%d",vk-96);
    else if(vk>=112 && vk<=123)wsprintfA(out,"F%d",vk-111);
    else wsprintfA(out,"KEY %d",vk);
}
static void handle_key(int vk)
{
    if(vk==HOTKEYS[1] || vk==HOTKEYS[2]){
        int active=g_engaged || g_sc_state!=SC_IDLE;
        int action=vk==HOTKEYS[1]?3:(active?2:1);
        if(action==3 && !active)return;
        int verdict=sc_confirm_step(&g_confirm,1,action,g_sc_state*16+g_ct,g_epoch,
                                     (double)GetTickCount64()/1000,g_sc_config.ConfirmSeconds);
        if(verdict==1)return;
        if(action==3){sc_emergency_reset();return;}
    }
    if(vk==HOTKEYS[2])vk=VK_NUMPAD0;
    switch (vk) {
        /* Home is the whole feature: deploy the safety car, and once it is
         * out, bring it in and run the restart. Everything else lives in the
         * menu on Numpad 0 - a dozen live hotkeys during a race is how a
         * stray key press ends up re-gridding the field.                   */
        /* FORK: Num1 is DISABLED. The extra car is parked by the load
         * mechanism (role -1, count dropped) and un-parked only by the
         * deploy path and the phase-4 pit-box hand-over - a manual toggle
         * here would put him in the classification and on the grid, which
         * is exactly what this fork exists to prevent. Numpad 0 deploys. */
        case VK_NUMPAD1:
            { char key[24];sc_key_name((int)g_sc_config.DeployKey,key);
              hud_show("PRESS %s TO DEPLOY",key); }
            break;

        case VK_NUMPAD0:
            if (sc_is_out() || g_sc_state == SC_ARMING ||
                g_sc_state == SC_OUTLAP || g_sc_state == SC_CATCHUP)
                sc_recall(1);
            else {
                sc_deploy();if(g_sc_state!=SC_IDLE)g_emergency_inhibit=0;
            }
            break;
        default: break;
    }
}

/* ============================================================================
 * FORK: THE EXTRA-CAR LOAD MECHANISM (ported from patches/native_safety_car/
 * native_sc.c, DESIGN "B").
 *
 * The SC is added as ONE EXTRA CAR at track load, inside the game's own field
 * build, so the game's own passes create his mesh, scene node and pit crew.
 * Two hooks:
 *   - tail of AIDriverCountSetup (0x4BFD90): identity via the 271-car roster
 *     by .VEH path, role 1, ++VehicleCount, active-array rebuild. The game's
 *     own mesh loop (0x42C301) then builds him like an entry.
 *   - DynMan_Restart (0x42C3B0): re-arm role 1 for the render pass, then park
 *     (role -1 + rebuild + count -1) once the pass is done - on the game
 *     thread, deterministically.
 *
 * THE STANDINGS FEATURE: the count drop at the park is what removes the SC
 * from the classification - the standings/entry screen are count-bounded.
 * The count is restored by sc_unhide() whenever he must actually be driven
 * (the phase-4 pit-box hand-over and the deploy) and before the session
 * teardown (the worker's array guard), so the loader never walks a count
 * that does not cover him.
 * ==========================================================================*/
#define A_LOOKUP     0x004C3F80u   /* VehicleLookupByName(char*)      __stdcall */
#define A_NATLOAD    0x004C78F0u   /* NationalityDriverInfoLoader     __thiscall */
#define A_TEAMID     0x004C6960u   /* TeamIDSetter                    __thiscall */
#define A_STATE      0x004EF5A0u   /* SessionStateTransition          __thiscall */
#define A_AIDCS      0x004BFD90u   /* AIDriverCountSetup - HOOK 1                */
#define A_DYNRESTART 0x0042C3B0u   /* DynMan_Restart    - HOOK 2                */
#define G_SCENE      0x008AFE88u   /* global scene manager ptr                   */
#define G_ROSTER     0x008AC394u   /* full 271-car vehicle DB, stride 1060       */
#define ROSTER_STRIDE 1060
#define G_DRVIDX     0x008AC38Cu   /* VehicleLookupByName's last match           */

#define OFF_TEAMID   0x4894u       /* scene slot id -> "SLOT%03d"               */
#define OFF_CREW     0x32D4u       /* crew ptr (0 = not built yet)              */
/* INI is resolved beside this DLL by sc_settings_path(). */

/* Select by active native filter and .VEH classes on each field build. */
#include "sc_vehicle_selection.h"
#define SC_VEH_SCAN_ROOT "SEASONDATA\\VEHICLES"
static char g_sc_veh_path[MAX_PATH];
static int g_sc_selected_year;

static const char *sc_veh_path(void) { return g_sc_veh_path; }



/* Two installs can carry a .veh under the same relative path but with a
 * different disk layout underneath it (a manual reinstall, a symlinked
 * SEASONDATA, a second copy on another volume) - record which volume this
 * one resolved from (g_vol_part), combined further down with the run tag
 * from worker()'s startup line into a single go/no-go for arming the pace
 * governor - see engage(). Cheap, done once per DLL lifetime. */


/* Expected 5-byte prologues of the two hook targets. Refuse to patch if they
 * do not match - a patched or different exe is never written to blind. */
static const unsigned char AIDCS_EXPECTED[5] = { 0xA1, 0x9C, 0x32, 0x5C, 0x00 };
static const unsigned char DYNRESTART_EXPECTED[5] = { 0x68, 0x6F, 0x02, 0x00, 0x00 };
/* sub_4BA6C0 prologue: push esi; mov esi,ecx; mov eax,[esi]. The first
 * instruction boundary after 5 bytes is exact (mov eax,[esi] ends at +5),
 * unlike sub_4B9890 whose mov al,[0x7CE254] spans +1..+6. Replay identity
 * building (VehicleInstanceListBuilder via CallWrapper_9870/PitTimingUpdate)
 * runs BEFORE sub_4B9890 inside this same dispatch, so the hook must wrap
 * the whole dispatch or the parked car is absent from the history. */

typedef void (__stdcall *lookup_t)(const char*);
typedef int  (__fastcall *natload_t)(void*, void*, int);
typedef int  (__fastcall *teamid_t)(void*, void*, int);
typedef int  (__fastcall *state_t)(void*, void*, int, int);
typedef int (__stdcall *aidcs_t)(const char*);
typedef int (__cdecl *dynrestart_t)(void);

#define LOOKUP   ((lookup_t) A_LOOKUP)
#define NATLOAD  ((natload_t)A_NATLOAD)
#define TEAMID   ((teamid_t) A_TEAMID)
#define STATE    ((state_t)  A_STATE)

#define V_PTR(i,off)  (*(void* volatile*)((char*)VEH(i) + (off)))

static unsigned char *g_tramp;           /* AIDriverCountSetup trampoline */
static unsigned char g_patch1[5], g_patch2[5];
static int g_hook1, g_hook2;
static unsigned char  g_orig[5];
static unsigned char *g_tramp2;          /* DynMan_Restart trampoline     */
static unsigned char  g_orig2[5];
static volatile int g_state3_done = 0;   /* deferred STATE(3,0) done flag */
static volatile int g_built   = 0;       /* parked after the load, ready  */
static PVOID g_veh_handle = NULL;        /* AddVectoredExceptionHandler return -
                                           * must be removed on unload or it
                                           * stays in the process' global fault
                                           * chain pointing at unmapped memory
                                           * (found 2026-08-08, static review) */

/* Crash reporter: logs the faulting instruction, the registers and the ebp
 * return-address chain, then lets the exception continue. Only reports
 * faults inside the game's own code.                                      */
static LONG CALLBACK crash_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c;
    unsigned *frame;
    int i;

    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    if ((unsigned)ep->ExceptionRecord->ExceptionAddress < 0x401000u ||
        (unsigned)ep->ExceptionRecord->ExceptionAddress > 0x600000u)
        return EXCEPTION_CONTINUE_SEARCH;

    c = ep->ContextRecord;
    logf_("[crash] ACCESS VIOLATION at 0x%08X (%s 0x%08X)",
          (unsigned)ep->ExceptionRecord->ExceptionAddress,
          ep->ExceptionRecord->NumberParameters >= 1 &&
              ep->ExceptionRecord->ExceptionInformation[0] ? "writing"
                                                           : "reading",
          ep->ExceptionRecord->NumberParameters >= 2
              ? (unsigned)ep->ExceptionRecord->ExceptionInformation[1] : 0);
    logf_("[crash] eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X "
          "ebp=%08X esp=%08X",
          c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);

    frame = (unsigned*)c->Ebp;
    for (i = 0; i < 12 && frame; ++i) {
        unsigned ret, next;
        if (IsBadReadPtr(frame, 8)) break;
        next = frame[0];
        ret  = frame[1];
        if (ret < 0x401000 || ret > 0x600000) break;   /* keep exe code only */
        logf_("[crash]   frame %2d: return 0x%08X", i, ret);
        if (next <= (unsigned)frame) break;
        frame = (unsigned*)next;
    }
    log_flush();
    scu_flush("crash");
    return EXCEPTION_CONTINUE_SEARCH;
}

/* Identity half ONLY for the extra slot - the game's own mesh loop and the
 * DynMan_Restart render pass do the rest (the car is counted and role 1 by
 * the time they run).                                                      */
static int find_safety_car(void)
{
    int i, found = -1;
    if (g_sc_selected_roster < 0) return -1;
    for (i = 0; i < VEH_SLOTS; i++) {
        if (V_I32(i, OFF_ROSTER) != g_sc_selected_roster ||
            V_I32(i, OFF_TEAMID) != i) continue;
        if (found >= 0) return -1; /* ambiguous duplicate roster: never guess */
        found = i;
    }
    return found;
}

static int sc_select_for_filter(const char *filter)
{
    int previous = *(volatile int*)G_DRVIDX;
    g_sc_selected_roster = -1;
    logf_("[sc-select] active filter: %.511s", filter ? filter : "(null)");
    if (!sc_select_vehicle(filter, SC_VEH_SCAN_ROOT, g_sc_veh_path,
                           sizeof(g_sc_veh_path), &g_sc_selected_year)) return 0;
    *(volatile int*)G_DRVIDX = -1;
    LOOKUP(g_sc_veh_path);
    g_sc_selected_roster = *(volatile int*)G_DRVIDX;
    *(volatile int*)G_DRVIDX = previous;
    if (g_sc_selected_roster < 0) {
        logf_("[sc-select] selected vehicle absent from loaded roster; add disabled");
        return 0;
    }
    logf_("[sc-select] active year=%d selected roster=%d; driver name unrestricted",
          g_sc_selected_year, g_sc_selected_roster);
    return 1;
}

static int sc_slot_owned(void)
{
    return g_sc_committed && g_sc_slot >= 0 && g_sc_slot < VEH_SLOTS &&
        g_sc_roster == g_sc_selected_roster &&
        V_I32(g_sc_slot, OFF_ROSTER) == g_sc_roster &&
        V_I32(g_sc_slot, OFF_TEAMID) == g_sc_slot &&
        find_safety_car() == g_sc_slot;
}

static int sc_add_entry(int slot)
{
    void *veh = (void*)VEH(slot);
    int   drvidx, prev_drvidx;
    const char *veh_path;

    if (slot < 0 || slot >= VEH_SLOTS) {
        logf_("[add] no room (slot=%d)", slot);
        return 0;
    }
    if (V_I32(slot, OFF_ACTIVE) != -1) {
        logf_("[add] slot %d not free (role=%d) - abort", slot,
              V_I32(slot, OFF_ACTIVE));
        return 0;
    }

    /* Resolve the selected era-matching .VEH and verify its roster identity.
     * Restore the native lookup global before returning. */
    veh_path    = sc_veh_path();
    prev_drvidx = *(volatile int*)G_DRVIDX;
    if (!veh_path || !veh_path[0]) return 0;
    *(volatile int*)G_DRVIDX = -1;
    LOOKUP(veh_path);
    drvidx = *(volatile int*)G_DRVIDX;
    if (drvidx < 0 || drvidx != g_sc_selected_roster) {
        logf_("[add] roster lookup failed for %s - abort", veh_path);
        *(volatile int*)G_DRVIDX = prev_drvidx;
        return 0;
    }
    logf_("[add] slot %d, roster idx %d for %s", slot, drvidx, veh_path);

    /* The Graphics=/.gen field must exist or the mesh cannot build a scene
     * node (VehicleGraphicsSceneLoader would null-deref at 0x4DFE4A). Check
     * the roster entry's field, not the node - the scene is not up yet at
     * AIDriverCountSetup time. */
    {
        char *ent = (char*)(*(volatile unsigned*)G_ROSTER) +
                    ROSTER_STRIDE * drvidx;
        char *gen_s = *(char**)(ent + 32);
        logf_("[diag] roster %d: veh='%s' gen='%s'", drvidx,
              *(char**)(ent + 0) ? *(char**)(ent + 0) : "(null)",
              gen_s ? gen_s : "(null)");
        if (!gen_s || !gen_s[0]) {
            logf_("[add] no Graphics=/.gen on the roster entry - aborting the "
                  "add cleanly");
            /* Not counted yet: do not decrement or rebuild the field. */
            *(volatile int*)G_DRVIDX = prev_drvidx;
            return 0;
        }
    }

    /* The scene manager must exist or the mesh parse has no tree to attach
     * to. */
    if (!*(void* volatile*)G_SCENE) {
        logf_("[add] scene manager null - aborting the add cleanly");
        /* Not counted yet: do not decrement or rebuild the field. */
        *(volatile int*)G_DRVIDX = prev_drvidx;
        return 0;
    }

    NATLOAD(veh, 0, drvidx);
    TEAMID(veh, 0, slot);
    if (V_I32(slot, OFF_ROSTER) != drvidx || V_I32(slot, OFF_TEAMID) != slot) {
        V_I32(slot, OFF_ACTIVE) = -1;
        *(volatile int*)G_DRVIDX = prev_drvidx;
        return 0;
    }
    logf_("[add] identity ok (roster=%d teamId=%d)",
          V_I32(slot, OFF_ROSTER), V_I32(slot, OFF_TEAMID));

    /* ROLE 1 FOR THE WHOLE LOAD - the 0x4D7579 crash fix: role -1 makes
     * LargeConfigFunctionWithPhysicsConstants skip the .hdv parse and the
     * tyre-record write; park only after the render pass has run. */
    V_I32(slot, OFF_ACTIVE) = 1;
    logf_("[add] role set to 1 for the load (park comes after the render pass)");

    STATE(veh, 0, 0, 0);

    /* Count him + rebuild, exactly as ProcessDriverEntries does. NO mesh or
     * render call here - the game's own DynMan_Init loop builds him (design
     * B). */
    VEHICLE_COUNT = REBUILD();
    logf_("[add] queued: name='%s' teamId=%d roster=%d VehicleCount=%d role=%d",
          (char*)veh + OFF_NAME, V_I32(slot, OFF_TEAMID),
          V_I32(slot, OFF_ROSTER), VEHICLE_COUNT, V_I32(slot, OFF_ACTIVE));

    *(volatile int*)G_DRVIDX = prev_drvidx;   /* leave the global as found */
    return 1;
}

/* ---- AIDriverCountSetup detour: add the extra car at the tail of the field
 * build. Find-first-then-add: on a reload the extra car can survive (the
 * teardown guard restored the count), so never build a second Bernd.        */
static volatile int g_sc_clear_entered;
static volatile int g_sc_clear_native_result;
static int __attribute__((thiscall)) sc_clear_trace(void *vehicle);
/* SC-01 (0.10.89): a session that ends outside the race (session-end refused)
 * leaves the hidden car in its slot; the next native vehicle load crashed
 * before AIDriverCountSetup could clear it. The clear now runs at a non-race
 * session end (before teardown); AIDriverCountSetup re-adds on the next load. */
static int g_sc_preload_clear_failed;
static __attribute__((always_inline)) inline int sc_dormant_clear_due(void)
{
    return g_sc_committed && g_hidden && !g_engaged &&
        !sc_has_control_loans() && sc_slot_owned() &&
        V_I32(g_sc_slot,OFF_ACTIVE)==-1;
}
static __attribute__((always_inline)) inline int sc_dormant_clear(const char *where)
{
    const void *clear_site=(const void*)(uintptr_t)0x004C6E80u;
    int old_slot=g_sc_slot;
    g_sc_clear_entered=0;
    g_sc_clear_native_result=-999;
    int cleared=f1c_vehicle_native_clear_dormant_at(sc_marker_read,
        (uintptr_t)VEH(old_slot),g_sc_roster,g_game_thread,1,
        clear_site,sc_clear_trace);
    logf_("[preload] old dormant Safety Car clear (%s) slot=%d result=%d entered=%d native_result=%d role=%d roster=%d",
          where,old_slot,cleared,g_sc_clear_entered,g_sc_clear_native_result,
          V_I32(old_slot,OFF_ACTIVE),V_I32(old_slot,OFF_ROSTER));
    if(cleared){g_sc_committed=0;g_sc_slot=-1;}
    return cleared;
}
static int __stdcall aidcs_detour(const char *cfg)
{
    int r, preload_clear_failed=0;
    if (!on_game_thread() || !g_hooks_ready || g_stop_requested)
        return ((aidcs_t)g_tramp)(cfg);
    /* Never add or re-count the native SC entry unless the reviewed session
       detector positively identifies an offline session. */
    if (!sc_offline_authorized()) return ((aidcs_t)g_tramp)(cfg);
    /* Final-exit callbacks also occur at weekend phase transitions. They
     * cannot unconditionally clear the car there: doing so made it absent
     * from the first race. At this native driver-load boundary the old car
     * is still identity-owned and dormant; clear it before the game's new
     * driver construction touches the slot. */
    if (sc_dormant_clear_due())
        preload_clear_failed=!sc_dormant_clear("aidcs");
    else if (g_sc_preload_clear_failed)
        preload_clear_failed=1;
    g_sc_preload_clear_failed=0;
    abandon_session();
    g_sc_selected_roster = -1;
    g_sc_veh_path[0] = 0;
    r = ((aidcs_t)g_tramp)(cfg);

    if(preload_clear_failed){
        logf_("[preload] old Safety Car cleanup failed; insertion refused");
        return r;
    }

    g_state3_done = 0;
    g_sc_slot     = -1;      /* re-resolved below; -1 keeps the restart detour inert */
    g_built       = 0;       /* fresh load: allow ONE bootstrap rebuild below, see
                               * dynrestart_detour() - found 2026-08-08, live crash */

    /* TEST DAY: the safety car is NEVER loaded - not added, not re-counted,
     * nothing. A stale g_sc_slot from a previous race must not survive into a
     * Test Day load (the DynMan_Restart detour would re-arm a dead slot). */
    if (SESSION_TYPE == SESSION_TEST_DAY) {
        logf_("[add] Test Day (session 0) - safety car NOT loaded");
        return r;
    }

    if (!(int)g_sc_config.add) {
        logf_("[add] disabled by safetycar.ini [sc] add=0 - load untouched");
        return r;
    }

    if (!sc_select_for_filter(cfg)) return r;

    g_sc_slot = find_safety_car();
    if (g_sc_slot >= 0) {
        /* A car surviving an earlier session is not a new-load resource. Its
         * scene links may already point into the old scene manager. Never
         * re-arm it: the native render pass has crashed here repeatedly. */
        logf_("[add] stale Safety Car at slot %d role=%d; refusing reuse for this load",
              g_sc_slot, V_I32(g_sc_slot, OFF_ACTIVE));
        g_sc_slot = -1;
        g_sc_committed = 0;
        return r;
    }

    {
        int candidate = VEHICLE_COUNT;
        if (sc_add_entry(candidate)) {
            g_sc_slot = candidate;
            g_sc_roster = V_I32(candidate, OFF_ROSTER);
            memcpy(g_sc_identity, V_STR(candidate, OFF_NAME), sizeof(g_sc_identity));
            g_sc_committed = 1;
        }
    }
    return r;
}

/* ---- DynMan_Restart detour: re-arm role 1 for the render pass, defer the
 * activate, then park on the game thread once the pass is provably done.
 * THE LOAD PARK is what keeps him out of the standings and the classification
 * from the moment the load ends: role -1 + rebuild + count -1. He is put back
 * into the field only at RACE_PHASE 4 (sc_grid_place below) on GRID
 * coordinates, takes the green from the back, and the game retires him to his
 * garage on lap 1 - then sc_park() hides him again.                        */
static int __cdecl dynrestart_detour(void)
{
    int r, slot = g_sc_slot;
    if (!on_game_thread() || !g_hooks_ready || g_stop_requested || !sc_slot_owned())
        return ((dynrestart_t)g_tramp2)();
    if (!sc_offline_authorized()) return ((dynrestart_t)g_tramp2)();

    /* Gated on !g_built: DynMan_Restart fires more than once per load (one
     * pass per session phase - quali setup, grid setup, race setup all hit
     * this detour). Re-arming role 1 + REBUILD() on EVERY one of those, as
     * this used to do unconditionally, drives the game's own per-vehicle
     * reset (VehicleResetClear, via the same DynMan_Post family this hook
     * sits in) through a full mesh/sound teardown-and-rebuild cycle each
     * time - a normal grid entrant only ever goes through that once. Live
     * crash 2026-08-08: F_3DSoundSourceDestructor (0x545150) faulted on a
     * stale sound-source pointer after 3 re-arm/re-park cycles in one load,
     * later a 4th cycle (a session step-back-in) crashed the same way. The
     * ONE bootstrap build below (first call this load, g_built still 0) is
     * still required - it is what the closing park block below needs to
     * have something to park. Every later re-entry (race start, deploy)
     * goes through sc_unhide()/sc_hide() instead, which are gated on
     * field_stable()/g_hidden and do not touch g_built - unaffected by this
     * gate. */
    if (slot >= 0 && slot < VEH_SLOTS && V_I32(slot, OFF_ACTIVE) == -1 && !g_built) {
        V_I32(slot, OFF_ACTIVE) = 1;
        VEHICLE_COUNT = REBUILD();
        logf_("[restart] slot %d re-armed to role 1 for the render pass "
              "(first build this load)", slot);
    }

    if (slot >= 0 && slot < VEH_SLOTS && !g_state3_done) {
        STATE((void*)VEH(slot), 0, 3, 0);
        g_state3_done = 1;
        logf_("[add] deferred STATE(3,0) done");
    }

    r = ((dynrestart_t)g_tramp2)();

    if (!sc_slot_owned() || g_sc_slot != slot) {
        logf_("[identity] Safety Car changed during native restart; no post-restart vehicle writes");
        return r;
    }

    /* Park HERE, not from the worker: the only deterministic moment where
     * the render pass is finished and we are on the game thread. */
    if (slot >= 0 && slot < VEH_SLOTS && V_I32(slot, OFF_ACTIVE) == 1) {
        g_built = 1;
        if (g_hide_mode) {
            if (g_hidden) {
                /* already hidden (a session change re-armed the role for the
                 * render pass): re-park the role, the count is already down */
                V_I32(slot, OFF_ACTIVE) = -1;
                VEHICLE_COUNT = REBUILD();
            } else {
                /* role -1 + rebuild + count -1: out of the standings from the
                 * classification screen on - the fork's whole reason to exist */
                sc_hide(slot);
            }
        } else {
            V_I32(slot, OFF_ACTIVE) = -1;
            VEHICLE_COUNT = REBUILD();
            g_hidden = 0;
            logf_("[restart] slot %d parked dormant (hide mode off - he stays "
                  "in the classification)", slot);
        }
        logf_("[restart] slot %d parked after the pass (crew=%p) "
              "VehicleCount=%d", slot, V_PTR(slot, OFF_CREW), VEHICLE_COUNT);
    }
    return r;
}
/* Replay owns ordinary racers, but may not replace the loader-owned extra car.
 * The SDK gate skips that car's entire reconcile/name transaction before any
 * mutation. No history backdating, unpark/count loan, or re-add on exit. */
#include "f1c_sc_replay_ownership.h"
#include "f1c_replay_live_instances.h"
static int __cdecl
sc_replay_instance_owned(uintptr_t vehicle);
static int __cdecl sc_live_replay_authorized(void) {
    return on_game_thread() && g_hooks_ready && !g_stop_requested &&
        SESSION_TYPE==7 && RACE_PHASE==PHASE_RACING && sc_offline_authorized();
}
static int __cdecl sc_replay_instance_preserved(uintptr_t vehicle) {
    return f1c_live_instance_preserved(vehicle) || sc_replay_instance_owned(vehicle);
}
static int __cdecl sc_replay_instance_owned(uintptr_t vehicle)
{
    if (!on_game_thread() || !g_hooks_ready || g_stop_requested ||
        g_sc_slot < 0 || g_sc_slot >= VEH_SLOTS || vehicle != (uintptr_t)VEH(g_sc_slot))
        return 0;
    return sc_slot_owned() && sc_offline_authorized() &&
        *(uintptr_t*)(vehicle + F1C_SC_REPLAY_SELF_OFF) == vehicle &&
        *(uintptr_t*)(vehicle + F1C_SC_REPLAY_SCENE_OFF) != 0;
}

/* ---- 5-byte detour install/remove --------------------------------------- */
static int hook_one(unsigned addr, void *detour, const unsigned char *expect,
                    unsigned char *save, unsigned char **tramp, const char *name)
{
    unsigned char *at = (unsigned char*)addr;
    unsigned char  patch[5];
    DWORD old = 0;

    if (memcmp(at, expect, 5) != 0) {
        logf_("[hook] REFUSED: 0x%08X does not hold the expected prologue "
              "(%02X %02X %02X %02X %02X) - another mod or a patched exe. %s "
              "disabled.", addr, at[0], at[1], at[2], at[3], at[4], name);
        return 0;
    }

    *tramp = (unsigned char*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE,
                                          PAGE_EXECUTE_READWRITE);
    if (!*tramp) { logf_("[hook] VirtualAlloc failed for %s", name); return 0; }

    memcpy(save, at, 5);
    memcpy(*tramp, save, 5);
    (*tramp)[5] = 0xE9;
    *(int*)(*tramp + 6) = (int)(at + 5) - (int)(*tramp + 10);

    patch[0] = 0xE9;
    *(int*)(patch + 1) = (int)detour - (int)(at + 5);

    if (!VirtualProtect((LPVOID)at, 5, PAGE_EXECUTE_READWRITE, &old)) {
        logf_("[hook] VirtualProtect failed for %s", name);
        return 0;
    }
    memcpy(at, patch, 5);
    VirtualProtect((LPVOID)at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)at, 5);
    logf_("[hook] %s detoured at 0x%08X", name, addr);
    return 1;
}

static int unhook_one(unsigned addr, unsigned char *save,
                      unsigned char *patch, int *installed)
{
    unsigned char *at = (unsigned char*)addr;
    DWORD old = 0;
    if (!*installed) return 1;
    if (memcmp(at, patch, 5) != 0) return 0; /* another owner, do not overwrite */
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(at, save, 5);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    *installed = 0;
    return VirtualProtect(at, 5, old, &old) != 0;
}

/* The text renderer starts with a four-byte MOV and a six-byte SUB. Copying
 * only five bytes would split the SUB and corrupt its trampoline. */
static int sc_hud_box_install(void)
{
    static const unsigned char expected[10]={
        0x8B,0x54,0x24,0x04,0x81,0xEC,0x3C,0x01,0x00,0x00};
    unsigned char *at=(unsigned char*)HUD_TEXT_RENDER;
    DWORD old=0;
    if(memcmp(at,expected,10)!=0){
        logf_("[hud-box] native text renderer changed; keeping colored text fallback");
        return 0;
    }
    g_hud_box_tramp=(unsigned char*)VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,
                                                   PAGE_EXECUTE_READWRITE);
    if(!g_hud_box_tramp)return 0;
    memcpy(g_hud_box_orig,at,10);
    memcpy(g_hud_box_tramp,at,10);
    g_hud_box_tramp[10]=0xE9;
    *(int*)(g_hud_box_tramp+11)=(int)(at+10)-(int)(g_hud_box_tramp+15);
    g_hud_box_patch[0]=0xE9;
    *(int*)(g_hud_box_patch+1)=(int)sc_hud_box_detour-(int)(at+5);
    memset(g_hud_box_patch+5,0x90,5);
    if(!VirtualProtect(at,10,PAGE_EXECUTE_READWRITE,&old))return 0;
    memcpy(at,g_hud_box_patch,10);
    VirtualProtect(at,10,old,&old);
    FlushInstructionCache(GetCurrentProcess(),at,10);
    g_hud_box_hook_installed=1;
    g_hud_box_hook_on=1;
    logf_("[hud-box] native status background installed; SC/GREEN glyphs black");
    return 1;
}

static int sc_hud_box_remove(void)
{
    unsigned char *at=(unsigned char*)HUD_TEXT_RENDER;
    DWORD old=0;
    if(!g_hud_box_hook_installed)return 1;
    g_hud_box_hook_on=0; /* an in-flight call falls back to native text color */
    if(memcmp(at,g_hud_box_patch,10)!=0)return 0;
    if(!VirtualProtect(at,10,PAGE_EXECUTE_READWRITE,&old))return 0;
    memcpy(at,g_hud_box_orig,10);
    FlushInstructionCache(GetCurrentProcess(),at,10);
    g_hud_box_hook_installed=0;
    return VirtualProtect(at,10,old,&old)!=0;
}

static void hook_install(void)
{
    g_hook1 = hook_one(A_AIDCS, (void*)aidcs_detour, AIDCS_EXPECTED,
                       g_orig, &g_tramp, "AIDriverCountSetup");
    if (!g_hook1) return;
    memcpy(g_patch1, (void*)A_AIDCS, 5);
    g_hook2 = hook_one(A_DYNRESTART, (void*)dynrestart_detour, DYNRESTART_EXPECTED,
                       g_orig2, &g_tramp2, "DynMan_Restart");
    if (!g_hook2) {
        unhook_one(A_AIDCS, g_orig, g_patch1, &g_hook1);
        return;
    }
    memcpy(g_patch2, (void*)A_DYNRESTART, 5);
    if (!f1c_sc_replay_install(sc_replay_instance_preserved,g_game_thread,1)) {
        unhook_one(A_DYNRESTART,g_orig2,g_patch2,&g_hook2);
        unhook_one(A_AIDCS,g_orig,g_patch1,&g_hook1);
        logf_("[hook] replay ownership boundary refused; startup disabled");
        return;
    }
    if(!f1c_live_install(sc_live_replay_authorized,g_game_thread)) {
        f1c_sc_replay_remove(g_game_thread);
        unhook_one(A_DYNRESTART,g_orig2,g_patch2,&g_hook2);
        unhook_one(A_AIDCS,g_orig,g_patch1,&g_hook1);
        logf_("[hook] live replay transaction refused; startup disabled");return;
    }
    if(!f1c_sc_pit_install(sc_block_native_pit,g_game_thread,1)) {
        f1c_live_remove(g_game_thread);
        f1c_sc_replay_remove(g_game_thread);
        unhook_one(A_DYNRESTART,g_orig2,g_patch2,&g_hook2);
        unhook_one(A_AIDCS,g_orig,g_patch1,&g_hook1);
        logf_("[hook] SC pit authority refused; startup disabled");return;
    }
    /* Keep the game's native message renderer. The box detour obscured SC in
     * 0.10.85 and clipped all three lines in 0.10.86. Native colors are set
     * by hud_line: yellow SC, green GREEN, white details. */
    g_hooks_ready = 1;
}

static int hook_remove(void)
{
    int first, second, third, box;
    if(!f1c_live_remove(g_game_thread)) {
        logf_("[stop] live replay transaction active or foreign hook; resident hooks retained");return 0;
    }
    g_hooks_ready = 0;
    box=sc_hud_box_remove();
    if(!f1c_sc_pit_remove(g_game_thread))logf_("[stop] SC pit authority restoration failed; resident code retained");
    third = f1c_sc_replay_remove(g_game_thread);
    second = unhook_one(A_DYNRESTART, g_orig2, g_patch2, &g_hook2);
    first = unhook_one(A_AIDCS, g_orig, g_patch1, &g_hook1);
    if (!first || !second || !third || !box)
        logf_("[stop] hook restoration incomplete; code retained until process exit");
    return first&&second&&third&&box;
}

/* Load-progress probe: a crash inside the load leaves no other trace, so poll
 * the slot's build state and write every CHANGE.                          */
static void reset_track_history(void)
{
    g_lap_len = 0; g_lap_len_exact = 0; g_lap_seeded = 0;
    memset(g_lap_last_d, 0, sizeof(g_lap_last_d));
    memset(g_lap_last_n, 0, sizeof(g_lap_last_n));
    memset(g_prof, 0, sizeof(g_prof)); g_prof_n = 0; g_prof_mean = 0;
    memset(g_sp_hist, 0, sizeof(g_sp_hist)); g_sp_idx = 0;
    memset(g_sc_pace_hist, 0, sizeof(g_sc_pace_hist)); g_sc_pace_n = 0;
    g_app_ok = g_box_ok = 0;
    if (g_ctl) { g_ctl->app_valid = 0; g_ctl->box_valid = 0; }
}

static int g_finish_active,g_finish_return_sent,g_finish_parked;
static double g_finish_last=-1;
static void abandon_session(void)
{
    g_np_hud_text[0]=g_np_hud_detail[0]=0;g_np_ticket_active=0;
    g_np_native_fault=g_np_investigation_active=g_np_prompt_active=0;
    g_np_warning_active=g_np_rules_off=0;
    g_np_investigation_visible_since=-1.0;
    g_np_rules_off_until=g_np_sanction_until=0;
    g_np_warning_action=SC_NP_NONE;g_np_warning_visible_since=-1.0;
    g_finish_active=g_finish_return_sent=g_finish_parked=0;g_finish_last=-1;
    g_emergency_inhibit=g_emergency_failed=0;
    g_rf_stand_down=0;g_weather_departure_fresh=0;
    say_flush("session boundary");
    if(g_ctl){g_ctl->cmd_safetycar=0;g_ctl->cmd_engage=0;g_ctl->cmd_release=0;
        g_ctl->cmd_panic=0;g_ctl->cmd_dump=0;g_ctl->cmd_yellow=0;}
    sc_initial_invalidate();
    g_recall_leader=-1;g_recall_lap=-1;g_pit_handover=0;
    sc_order_reset(&g_order_recovery);
    /* A new generation owns these objects. No restoration into reused memory. */
    ++g_epoch;
    g_sc_forced_return=0;memset(&g_sc_stint,0,sizeof(g_sc_stint));
    f1c_scf_reset(g_sc_fault_signal);g_sc_fault_latched=0;
    g_sc_identity_fault_since=-1.0;g_restart_fault=g_restart_hold=0;
    memset(g_armed, 0, sizeof(g_armed));
    memset(g_saved_epoch, 0, sizeof(g_saved_epoch));
    memset(g_wave, 0, sizeof(g_wave));
    memset(g_catchup, 0, sizeof(g_catchup));
    memset(g_stuck_watch, 0, sizeof(g_stuck_watch));
    g_engaged = 0; g_hidden = 0; g_gfx_hidden = 0;
    g_sc_lap_forced = 0; g_sc_lap_true = 0;
    g_sc_slot = -1; g_sc_committed = 0;
    g_sc_state = SC_IDLE; g_ct = CT_OFF; g_caution = CAUTION_OFF;g_sc_continuation=0;
    g_green_ms = g_restart_green_ms = g_ct_seen_ms = g_settle_ms = 0;
    g_ct_laps_done = 0; g_ct_lap_ref = -1; g_gather_start = 0;g_gather_sim=-1.0;g_sc_arm_sim=-1.0;
    memset(&g_trig, 0, sizeof(g_trig));
    reset_track_history();
}

static void sc_probe(void)
{
    static int last_crew = -1, last_act = -2, last_vc = -1;
    int slot = g_sc_slot, crew, act, vc;
    if (!sc_slot_owned()) return;
    crew = (int)V_PTR(slot, OFF_CREW);
    act  = V_I32(slot, OFF_ACTIVE);
    vc   = VEHICLE_COUNT;
    if (crew == last_crew && act == last_act && vc == last_vc) return;
    last_crew = crew; last_act = act; last_vc = vc;
    logf_("[probe] slot %d: crew=0x%08X role=%d VehicleCount=%d", slot,
          crew, act, vc);
}

/* ---- race-only re-entry before the grid (the fork's start) --------------
 * The extra car is parked (role -1, count dropped) from the moment the load
 * finishes - invisible in the classification of EVERY session, quali included
 * (the count drop is session-agnostic and the phase-3 re-entry below is
 * race-only). When the RACE leaves the classification screen (phase 2 ->
 * 3, grid setup) he is re-added (role 1 + count +1) and the game's OWN grid
 * placement pass then puts him at his qualipos - the last grid box - with
 * the racing line, and the start sequence holds him there. NO teleport:
 * hand-written coordinates fought the placement pass and put him somewhere
 * weird (2026-08-07); the game's own placement cannot disagree with itself.
 * He takes the green from the back, the game retires him to his garage on
 * lap 1 via sc_park()'s arming - after which sc_park() hides him again
 * (count -1) and he leaves the standings once more.                       */
static void sc_reenable(void)
{
    /* NOGRID FORK 2026-08-08: the grid re-entry is DISABLED.
     *
     * Restoring VehicleCount here (22 -> 23) fired sc_unhide() DURING the
     * game's own grid-setup pass (RACE_PHASE 3/4) - the single most unstable
     * window there is, the one field_stable() exists to keep code out of.
     * The game's grid physics-init loop (sub_54FA00 and its integrator
     * callers) then picked up the extra slot mid-pass and null-dereferenced
     * an uninitialised physics sub-object (this+0x1E0) at 0x0054FA06.
     * Reproduced every time a race reached the grid, live 2026-08-08. It is
     * the same class as the sound-source and scene-node crashes: any
     * VehicleCount increase after the load has settled is unsafe, and the
     * grid-setup pass is the worst place to do it.
     *
     * Unlike sc_hide()/the teardown restore, sc_unhide() has no field_stable()
     * gate, and this was the one caller that fired it inside an unstable
     * window. The deploy-time count restore (SC_ARMING -> sc_unhide) runs
     * during GREEN racing (field stable) and is proven to work - a full
     * caution cycle completed on it this session. So only the grid re-entry
     * is removed.
     *
     * Cost: he no longer physically sits on the back of the grid and retire
     * on lap 1 (cosmetic). He stays hidden - out of the standings, count 22 -
     * from load through the whole green race, and only comes into the field
     * when a caution actually deploys him (deploy un-hides him during green).
     * Everything the driver confirmed working is kept: 22/22 standings hiding
     * in quali and pre-race, and deploy-from-garage with the full pit-stop /
     * caution / restart cycle. */
    return;
}

/* 0.10.92 owner request (B2r): pressing Y with the Safety Car out must not be
 * refused outright. The guard itself stays MUTATION-FREE: on an SC-owned unsafe
 * state it only sets g_y_park_requested and still returns BLOCK. The SC parks
 * itself on the next normal game-thread tick (game_tick -> sc_y_park_tick),
 * never inside the restart pre-hook (0.10.53 rule: teleport/rebuild in the guard
 * is forbidden). The second Y then passes the unchanged physical proof below. */
static volatile int g_y_park_requested;

/* This guard runs synchronously before vanilla consumes a mini-restart.
 * Refuse if restoration cannot be proved safe, rather than rebuild a torn field. */
static F1CBrokerRestartDecision __cdecl before_restart(void *context,
        uint32_t generation, int32_t phase)
{
    (void)context; (void)generation; (void)phase;
    if (!on_game_thread()) return F1C_BROKER_RESTART_BLOCK;
    /* A native restart must never admit an out-of-garage SC to its grid.
       Pace release and standings exclusion alone do not establish parking.
       Do not restore a roster, release loans, or rebuild a car in this guard. */
    int controlled = g_engaged || sc_has_control_loans() || g_ct!=CT_OFF ||
                     g_sc_state!=SC_IDLE || g_yellow || g_sc_lap_forced;
    int present = g_sc_slot>=0 || g_built || g_hidden;
    int parked = !controlled && present && sc_offline_authorized() &&
                 field_stable() && g_built && g_hidden && sc_slot_owned() &&
                 V_I32(g_sc_slot,OFF_ACTIVE)==-1 && sc_ready(g_sc_slot);
    if ((!present && !controlled) || parked)
        return F1C_BROKER_RESTART_ALLOW;
    /* SC-owned unsafe state: ask the SC to park itself on the next game tick.
       No restore, release or rebuild happens in this guard. */
    if (present && g_rc && f1c_rc_read(&g_rc->owner)==F1C_RC_SC)
        g_y_park_requested = 1;
    /* RF's acknowledged yield must satisfy the same physical proof. An idle
       controller with a displaced body is precisely the captured failure. */
    if(!g_rc || f1c_rc_read(&g_rc->owner)!=F1C_RC_RF)
        hud_notice("GREEN FLAG DELAYED: SAFETY CAR");
    logf_("[restart-refused] Safety Car not safely parked/excluded: slot=%d hidden=%d built=%d engaged=%d state=%d", 
          g_sc_slot,g_hidden,g_built,g_engaged,g_sc_state);
    return F1C_BROKER_RESTART_BLOCK;
}

/* 0.10.92 (B2r): the SC parks itself here, on the normal game thread, after the
 * guard deferred it - not inside the restart pre-hook. Returns 1 when a request
 * was handled (the tick stops for this frame either way). A failed emergency
 * reset keeps the car un-parked, so the next Y (and the guard's proof) still
 * refuses; the emergency path's own HUD reports the failure. */
static int sc_y_park_tick(void)
{
    if(!g_y_park_requested) return 0;
    g_y_park_requested = 0;
    if(!g_rc || f1c_rc_read(&g_rc->owner)!=F1C_RC_SC) return 1;
    hud_notice("SAFETY CAR RETURN - Y AGAIN");
    if(sc_emergency_reset())
        hud_show("PRESS Y TO RESTART");
    return 1;
}

static void initialize_hooks(void)
{
    if (g_stop_requested) return;
    if (!g_initialized) {
        g_initialized = 1;
        int guard_result = g_broker.set_restart_guard(g_client, before_restart, NULL);
        if (guard_result != F1C_BROKER_OK) {
            logf_("[startup-stop] restart guard acquisition failed: result=%d", guard_result);
            InterlockedExchange(&g_stop_requested, 1);
        } else {
            logf_("[startup] restart guard acquired");
            hook_install();
            if (!g_hooks_ready) {
                logf_("[startup-stop] hook installation failed");
                InterlockedExchange(&g_stop_requested, 1);
            } else logf_("[startup] Safety Car hooks ready");
        }
    }
}

static void __cdecl before_vehicle_load(void *unused)
{
    (void)unused;
    if (g_stop_requested) return;
    /* The broker invokes us on the actual native loader caller, before the
       AIDriverCountSetup call. The update callback refreshes this identity
       when simulation resumes. No vehicle is manufactured from this callback. */
    g_game_thread = GetCurrentThreadId();
    logf_("[preload] native vehicle load entered thread=%lu", (unsigned long)g_game_thread);
    logf_("[preload] seen=%ld tick=%lu", ++g_preload_seen, (unsigned long)GetTickCount());
    initialize_hooks();
    if (g_hooks_ready && !g_hooks_ready_logged) {
        g_hooks_ready_logged = 1;
        logf_("[startup] hooks ready via %s preload_seen=%ld", "preload", g_preload_seen);
    }
    /* No session-guard gate here: mid-load the guard can be unreadable, and
       the car is only ever committed by an offline AIDriverCountSetup. */
    if (g_hooks_ready && !g_stop_requested && g_sc_committed) {
        int slot=g_sc_slot,role=-99;
        if(slot>=0 && slot<VEH_SLOTS) role=V_I32(slot,OFF_ACTIVE);
        logf_("[preload] dormant check slot=%d hidden=%d engaged=%d loans=%d owned=%d role=%d offline=%d",
              slot,g_hidden,g_engaged,sc_has_control_loans(),sc_slot_owned(),role,sc_offline_authorized());
        /* Do NOT clear here: by now native teardown has reset the scene
           registry and the native clear itself faults (owner test 18:07). */
    }
}

/* Broker invokes this once on the game thread before retiring a slow client.
   Restore only captured controls; do not tear down hooks or native car resources. */
static void __cdecl callback_recovery(void *unused)
{
    (void)unused;
    if(on_game_thread() && field_stable()) {
        release("broker callback containment");
        g_bunch=0;
        hud_show("SAFETY CAR FAULT: RESTART GAME");
        logf_("[containment] owned field controls released; controller disabled");
    } else logf_("[containment] field unstable: restoration refused; restart session");
    InterlockedExchange(&g_pending,0);
}

static int sc_hotkey_foreground_plain(int *previous_foreground)
{
    HWND hwnd=GetForegroundWindow(); DWORD pid=0;
    int foreground=hwnd && GetWindowThreadProcessId(hwnd,&pid) &&
        pid==GetCurrentProcessId();
    int modifiers=(GetAsyncKeyState(VK_CONTROL)|GetAsyncKeyState(VK_MENU)|
        GetAsyncKeyState(VK_SHIFT)|GetAsyncKeyState(VK_LWIN)|
        GetAsyncKeyState(VK_RWIN))&0x8000;
    int fresh_foreground=foreground&&*previous_foreground;
    *previous_foreground=foreground;
    return fresh_foreground&&!modifiers;
}

/* Game-thread-only phase measurements. No file I/O and no work suppression.
 * Report slow callbacks through the existing bounded worker log queue. */
static LARGE_INTEGER sc_perf_frequency,sc_perf_begin,sc_perf_mark;
static unsigned long long sc_perf_peak;
static const char *sc_perf_phase;
static void sc_perf_start(void){
    if(!sc_perf_frequency.QuadPart)QueryPerformanceFrequency(&sc_perf_frequency);
    QueryPerformanceCounter(&sc_perf_begin);sc_perf_mark=sc_perf_begin;
    sc_perf_peak=0;sc_perf_phase="entry";
}
static void sc_perf_step(const char *phase){
    LARGE_INTEGER now;QueryPerformanceCounter(&now);
    if(sc_perf_frequency.QuadPart>0){
        unsigned long long us=(unsigned long long)(now.QuadPart-sc_perf_mark.QuadPart)*1000000ull/(unsigned long long)sc_perf_frequency.QuadPart;
        if(us>sc_perf_peak){sc_perf_peak=us;sc_perf_phase=phase;}
    }
    sc_perf_mark=now;
}
static void sc_perf_finish(void){
    sc_perf_step("tail");
    if(sc_perf_frequency.QuadPart>0){
        unsigned long long total=(unsigned long long)(sc_perf_mark.QuadPart-sc_perf_begin.QuadPart)*1000000ull/(unsigned long long)sc_perf_frequency.QuadPart;
        static ULONGLONG last_report;
        static unsigned long long window_peak;
        static const char *window_phase;
        static unsigned long long window_phase_us;
        if(total>window_peak){window_peak=total;window_phase=sc_perf_phase;window_phase_us=sc_perf_peak;}
        ULONGLONG now=GetTickCount64();
        if(now-last_report>=1000ull){
            if(window_peak>=2000ull)logf_("[callback-time] total_us=%llu peak_phase=%s peak_us=%llu engaged=%d sc_state=%d",window_peak,window_phase,window_phase_us,g_engaged,g_sc_state);
            last_report=now;window_peak=0;
        }
    }
}

#include "sc_removal_policy.h"
#include "f1c_removal_options.h"
#include "f1c_native_removal_repair.h"
static void sc_native_removal_pump(void) {
    if(!on_game_thread() || !g_engaged || !phase_racing() ||
       !sc_action_authorized() || !field_stable() || !g_rc ||
       f1c_rc_read(&g_rc->owner)!=F1C_RC_SC || f1c_rc_read(&g_rc->rf_request)==2)return;
    for(int i=1;i<VEH_SLOTS;i++) {
        if(i==g_sc_slot || pit_owned(i))continue;
        if(f1c_native_removal_repair(sc_marker_read,(uintptr_t)VEH(i),g_game_thread,1))
            logf_("[native removal] slot %d expired disabled-AI pit trigger cleared; native removal timer may advance",i);
    }
}
static SCRemovalLoan g_removal_loan;
static void sc_removal_pump(int active){
    unsigned values[3],before[3],reply[6]={0};int rf=0;
    static int reported=-99,mismatch_reported;
    HMODULE module=NULL;
    if(GetModuleHandleExA(0,"redflag.dll",&module)){
        typedef int (__cdecl *Query)(unsigned*,unsigned);
        Query query=(Query)GetProcAddress(module,"F1C_QueryRemovalPolicy");
        rf=-1;
        if(query && query(reply,sizeof(reply)) && reply[0]==0x524D5631u && reply[1]==1 && reply[2]==24 && reply[3]<=1)rf=(int)reply[3];
        FreeLibrary(module);
    }else if(g_rc && f1c_rc_read(&g_rc->rf_registered))rf=-1;
    if(!f1c_removal_options_read(values))return;
    memcpy(before,values,sizeof(values));
    SCRemovalLoan next=g_removal_loan;
    unsigned mask=sc_removal_step(&next,values,rf,active,(float)g_sc_config.VehicleRemovalSeconds);
    int okay=1;
    for(unsigned i=0;i<3;i++)if(mask&(1u<<i)){
        if(!f1c_removal_options_write(i,before[i],values[i],g_game_thread))okay=0;
    }
    /* Preserve successful partial originals if an external writer races us. */
    if(okay||next.owned)g_removal_loan=next;
    if(reported!=rf){logf_("[removal] owner=%s requested_solo=%.1f",rf==1?"Red Flag":rf<0?"unknown RF; observing only":"Safety Car standalone",g_sc_config.VehicleRemovalSeconds);reported=rf;}
    float seconds=0;
    if(sc_removal_seconds(values[2],&seconds)){
        double need=fmax(8.0,fmax(g_sc_config.HoldSeconds,g_sc_config.InvertedConfirmSeconds))+5.0;
        int mismatch=active && !(rf==1 && reply[5]) && seconds<need;
        if(mismatch&&!mismatch_reported)logf_("[removal] WARNING effective %.1fs < confirmation plus margin %.1fs; confirmation is unchanged",seconds,need);
        mismatch_reported=mismatch;
    }
    if(!okay)logf_("[removal] concurrent option change; owned values only will be restored");
}

#include "sc_finish_policy.h"
/* Restore only values still attributable to our own writes. Native pit caps
   and third-party changes are not replaced by pre-caution defaults. */
static void sc_finish_release_car(int i,int pit) {
    if(V_F32(i,OFF_PASSING)==(float)PASSING_MIN)V_F32(i,OFF_PASSING)=g_saved_pass[i];
    if(V_F32(i,OFF_AGGRO)==(float)AGGRO_DEFAULT && g_saved_aggro[i]<0.05f)
        V_F32(i,OFF_AGGRO)=g_saved_aggro[i];
    if(!pit && isfinite(g_last_written[i]) && V_F32(i,OFF_MAXSPEED)==g_last_written[i] &&
       isfinite(g_saved[i]) && g_saved[i]>0 && g_saved[i]<2.0e9f) {
        int cap=(int)(g_saved[i]+0.5f);if(cap==SPEED_IDLE_SENTINEL)cap++;
        set_maxspeed((void*)VEH(i),cap);
    }
    g_armed[i]=0;g_wave[i]=WAVE_NONE;g_catchup[i]=0;
    logf_("[finish] slot=%d controls handed back; pit=%d",i,pit);
}
static void sc_finish_publish(void) {
    if(!g_ctl)return;
    int count=0;for(int i=0;i<VEH_SLOTS;i++)count+=g_armed[i]!=0;
    g_ctl->ct_state=g_ct;g_ctl->sc_state=g_sc_state;g_ctl->engaged=g_engaged;
    g_ctl->caution_phase=g_caution;g_ctl->cars_governed=count;g_ctl->sc_slot=g_sc_slot;
    g_ctl->sc_hidden=g_hidden;g_ctl->yellow=g_yellow;g_ctl->sc_ready=0;
    g_ctl->race_phase=RACE_PHASE;g_ctl->session_type=SESSION_TYPE;
    g_ctl->trig_gates=0;g_ctl->trig_hold_ms=0;g_ctl->trig_grace_left=0;
    g_ctl->heartbeat++;
}
/* Deliberately separate from all racing pumps: no GREEN, incidents, GUI
   activation, scoring changes or array rebuild after classification closes. */
/* Synchronous final-exit cleanup, before native results consume the array.
 * Pauses and mini-restarts never enter this callback. No teleport or GREEN. */
static int __attribute__((thiscall)) sc_clear_trace(void *vehicle) {
    F1CVehicleNativeClearFn native=(F1CVehicleNativeClearFn)(uintptr_t)0x004C6E80u;
    g_sc_clear_entered=1;
    g_sc_clear_native_result=native(vehicle);
    return g_sc_clear_native_result;
}
/* 0.10.93: the broker also runs session-end callbacks from inside sub_46BF10
 * (ESC "simulate the rest of the session") BEFORE the game decides whether it
 * leaves. When the weekend then continues straight to the race there is no
 * Game_Post and no vehicle reload, so a dormant clear here left the race
 * without its Safety Car (tester logs 2026-10-08). A real leave still
 * passes RestartGate 6 afterwards and fires this callback again outside
 * sub_46BF10, which clears the car before teardown as before (SC-01).
 * Detection: a native return address into a sub_46BF10 call site is live on
 * this thread's stack. Not found = old behaviour (clear). */
static int sc_inside_results_finalize(void)
{
    static const uintptr_t sites[]={0x0046C05Fu,0x0046DA42u,0x0046DCD8u};
    NT_TIB *tib=(NT_TIB*)NtCurrentTeb();
    const uintptr_t *p=(const uintptr_t*)__builtin_frame_address(0);
    const uintptr_t *top=(const uintptr_t*)tib->StackBase;
    for(int n=0;p+1<=top && n<8192;p++,n++)
        for(int i=0;i<3;i++) if(*p==sites[i]) return 1;
    return 0;
}
static void __cdecl sc_session_end(void *unused) {
    (void)unused;
    F1CSessionGuard guard=sc_session_guard();
    scu_request();   /* worker writes the usage summary after these lines */
    logf_("[session-end] entered phase=%d state=%d slot=%d hidden=%d",guard.race_phase,guard.game_state,g_sc_slot,g_hidden);
    if(!on_game_thread() || !guard.valid || !guard.offline ||
       guard.session_type!=7 || !field_stable()){
        logf_("[session-end] refused: thread/offline/race/field guard");
        /* SC-01: a non-race exit must release the hidden car while the scene
           registry still exists, exactly like the race-end path below. A
           weekend that continues without a reload then runs without the SC
           (deploy refused) instead of crashing on the next load. */
        if(on_game_thread() && guard.valid && guard.offline && sc_dormant_clear_due()){
            if(sc_inside_results_finalize())
                logf_("[session-end] accelerated finish: Safety Car kept (cleared on a real leave)");
            else
                sc_dormant_clear("non-race session end");
        }
        return;}
    int n=VEHICLE_COUNT,slot=g_sc_slot,rank=0;
    if(sc_slot_owned() && !g_hidden){
        int seen[VEH_SLOTS+1]={0};
        if(!f1c_result_position_ready()){logf_("[session-end] refused: native rank-writer fingerprint");return;}
        /* Validate the entire classification before changing anything. */
        for(int j=0;j<n;j++){
            uintptr_t car=VEHICLE_ARRAY[j];
            int pos=*(int*)(car+OFF_POSITION);
            if(pos<1 || pos>n || seen[pos]++){logf_("[session-end] refused: invalid or duplicate rank %d count %d",pos,n);return;}
        }
        rank=V_I32(slot,OFF_POSITION);
        if(!sc_hide(slot))return;
        /* Export iterates ranks 1..VehicleCount; close the SC's rank gap or
         * the last real racer would otherwise disappear from the results. */
        for(int j=0;j<VEHICLE_COUNT;j++){
            uintptr_t car=VEHICLE_ARRAY[j];int pos=*(int*)(car+OFF_POSITION);
            if(pos>rank)f1c_result_position_set(car,pos-1);
        }
    }
    for(int i=0;i<VEH_SLOTS;i++)
        if(g_armed[i] && capture_identity_owned(i))sc_finish_release_car(i,pit_owned(i));
    sc_lap_unforce();
    if(g_yellow)yellow_set(0);
    /* The parked car is outside the active/results count. Native
     * ProcessDriverEntries uses the same cleanup when replacing a vehicle:
     * release its old scene/physics/UI state and clear the roster identity
     * while the old scene manager still exists. The next DynMan_Init can
     * then add it as a genuinely new car through sc_add_entry. */
    if(sc_slot_owned() && g_hidden && V_I32(slot,OFF_ACTIVE)==-1) {
        int count_before=VEHICLE_COUNT;
        const void *clear_site=(const void*)(uintptr_t)0x004C6E80u;
        static const unsigned char expected_clear_entry[]={
            0x56,0x8b,0xf1,0x57,0x8d,0x7e,0x0c,0x57,0xb9,
            0x60,0xa5,0x5e,0x00,0xe8,0xae,0x9c,0xf4,0xff};
        int role_read=-99,roster_read=-99;
        int site_readable=!IsBadReadPtr(clear_site,sizeof(expected_clear_entry));
        int entry_match=site_readable &&
            memcmp(clear_site,expected_clear_entry,sizeof(expected_clear_entry))==0;
        int car_writable=!IsBadWritePtr((void*)VEH(slot),0x48c0u);
        int role_ok=sc_marker_read((uintptr_t)VEH(slot)+OFF_ACTIVE,&role_read,4);
        int roster_ok=sc_marker_read((uintptr_t)VEH(slot)+OFF_ROSTER,&roster_read,4);
        logf_("[session-end] clear guards thread=%lu expected=%lu site=%d match=%d writable=%d role_read=%d role=%d roster_read=%d roster=%d owned=%d",
              (unsigned long)GetCurrentThreadId(),(unsigned long)g_game_thread,
              site_readable,entry_match,car_writable,role_ok,role_read,
              roster_ok,roster_read,sc_slot_owned());
        g_sc_clear_entered=0;
        g_sc_clear_native_result=-999;
        int cleared=f1c_vehicle_native_clear_dormant_at(sc_marker_read,
            (uintptr_t)VEH(slot),g_sc_roster,g_game_thread,1,
            clear_site,sc_clear_trace);
        logf_("[session-end] dormant native cleanup slot=%d result=%d entered=%d native_result=%d count=%d->%d role=%d roster=%d",
              slot,cleared,g_sc_clear_entered,g_sc_clear_native_result,
              count_before,VEHICLE_COUNT,V_I32(slot,OFF_ACTIVE),V_I32(slot,OFF_ROSTER));
    }
    abandon_session();
    memset(&g_confirm,0,sizeof(g_confirm));g_hud_msg[0]=0;g_hud_until=0;
    sc_finish_publish();
    logf_("[session-end] Safety Car excluded before results; controls and status cleared");
}

static void sc_finish_pump(F1CSessionGuard guard) {
    memset(&g_confirm,0,sizeof(g_confirm));
    if(g_ctl){g_ctl->cmd_safetycar=0;g_ctl->cmd_engage=0;g_ctl->cmd_release=0;
        g_ctl->cmd_panic=0;g_ctl->cmd_yellow=0;}
    if(!guard.valid||!guard.offline||guard.session_type!=7||guard.race_phase!=8||
       (guard.game_state!=5&&guard.game_state!=7)||guard.paused||!field_stable()||
       !on_game_thread()||!g_rc||f1c_rc_read(&g_rc->owner)!=F1C_RC_SC)goto publish;
    if(!isfinite(SESSION_TIME)||SESSION_TIME<=g_finish_last)goto publish;
    g_finish_last=SESSION_TIME;
    if(!g_finish_active){g_finish_active=1;say_flush("chequered flag");
        logf_("[finish] post-race handover entered; each finisher releases independently");}
    int unfinished=0,unknown=0;
    for(int i=0;i<VEH_SLOTS;i++) {
        if(i==g_sc_slot)continue;
        F1CVehicleLifecycle life={0};int live=slot_live(i);
        if(live)life=f1c_vehicle_lifecycle_read(sc_marker_read,(uintptr_t)VEH(i));
        if(live&&!life.valid)unknown++;
        if(live&&life.valid&&life.participant)unfinished++;
        int identity=capture_identity_owned(i);
        int pit=life.valid&&pit_owned(i);
        int action=sc_finish_loan_action(g_armed[i],identity,life.valid,
                                        life.finish || !life.participant,pit);
        if(action==3){g_armed[i]=0;continue;}
        if(action==1){sc_finish_release_car(i,pit);continue;}
        if(action==2){
            /* Hold the car's own existing cap, never wait for a finished predecessor. */
            if(isfinite(g_last_written[i])&&g_last_written[i]>0&&g_last_written[i]<SPEED_RELEASE &&
               V_F32(i,OFF_MAXSPEED)>g_last_written[i])set_maxspeed((void*)VEH(i),(int)g_last_written[i]);
            V_F32(i,OFF_PASSING)=PASSING_MIN;
        }
    }
    if(!sc_finish_can_return(unfinished,unknown))goto publish;
    g_engaged=0;g_caution=CAUTION_OFF;g_ct=CT_OFF;g_cmd_now=0;
    if(g_yellow)yellow_set(0);
    if(sc_slot_owned() && g_sc_state!=SC_IDLE && !g_hidden) {
        int i=g_sc_slot;
        if(!g_finish_return_sent){
            /* Native return only; never lift an already native-owned pit limiter. */
            if(!pit_owned(i))sc_send_to_box(i);
            g_finish_return_sent=1;g_sc_cmd=0;g_sc_state=SC_RECALL;
            logf_("[finish] last racer finished; Safety Car native pit return requested");
        }
        if(!pit_owned(i)&&V_I32(i,OFF_CALLBOX)==0)V_I32(i,OFF_CALLBOX)=1;
        if(V_I32(i,OFF_PITTING)>=4 && isfinite(V_F32(i,OFF_SPEED)) && V_F32(i,OFF_SPEED)<2.0f){
            if(g_sc_ai_saved&&V_F32(i,OFF_PASSING)==(float)PASSING_MIN)V_F32(i,OFF_PASSING)=g_saved_pass[i];
            g_sc_state=SC_IDLE;g_finish_parked=1;g_sc_mark=0;g_sc_recall_armed=0;
            sc_initial_invalidate(); /* new session captures fresh parked state */
            logf_("[finish] Safety Car native parking confirmed; sequence ended");
        }
    }
    sc_lap_unforce();
publish:
    sc_finish_publish();
}

static void __cdecl game_tick(void *unused)
{
    static char down[NKEYS], fresh_edge[NKEYS];
    static int previous_hotkey_foreground;
    static unsigned generation;
    static int session_type = -1, was_stable;
    static double last_time;
    static SCSimTick competitive_tick;
    F1CBrokerStats stats = {0};
    int i, stable, rf_veto, hotkeys_allowed, fresh_sim;
    F1CSessionGuard runtime_guard;
    (void)unused;
    sc_perf_start();
    stats.structure_size = sizeof(stats);
    if (g_broker.get_stats(&stats) != F1C_BROKER_OK ||
        GetCurrentThreadId() != stats.hook_thread_id) goto done;
    g_game_thread = stats.hook_thread_id;
    /* Always consume key/focus history on an authenticated callback. A key
       held through pause, menu, RF, or a phase transition is never fresh. */
    hotkeys_allowed=sc_hotkey_foreground_plain(&previous_hotkey_foreground);
    for(i=0;i<NKEYS;i++) { int held=(GetAsyncKeyState(HOTKEYS[i])&0x8000)!=0;
        fresh_edge[i]=(char)(held&&!down[i]); down[i]=(char)held; }
    if(!g_rc)g_rc=f1c_rc_open();
    if(!g_rc){memset(&g_confirm,0,sizeof(g_confirm));goto done;}
    InterlockedExchange(&g_rc->sc_registered,1);
    /* Removal ownership is independent of the current SC/RF race-control lease. */
    F1CSessionGuard removal_guard=sc_session_guard();
    sc_removal_pump(!g_stop_requested && removal_guard.valid && removal_guard.offline &&
                    SESSION_TYPE==7 && (RACE_PHASE==5 || RACE_PHASE==2));
    if(!f1c_rc_sc_enter(g_rc)) {
        memset(&g_confirm,0,sizeof(g_confirm));
        if(g_ctl){g_ctl->cmd_safetycar=0;g_ctl->cmd_engage=0;g_ctl->cmd_release=0;g_ctl->cmd_panic=0;g_ctl->cmd_yellow=0;}
        goto done;
    }
    InterlockedExchange(&g_rc->sc_generation,(LONG)stats.session_generation);
    initialize_hooks();
    if (g_hooks_ready && !g_hooks_ready_logged) {
        g_hooks_ready_logged = 1;
        logf_("[startup] hooks ready via %s preload_seen=%ld", "tick", g_preload_seen);
    }
    stable = field_stable();
    if (generation != stats.session_generation || session_type != SESSION_TYPE ||
        (stable && last_time > SESSION_TIME)) {
        /* Phase/generation changes also occur on the same loaded field.
         * Retain only loader-owned SC bookkeeping with intact identity and
         * array; never retain per-car loans across this boundary. */
        int slot = g_sc_slot, hidden = g_hidden;
        int keep = stable && g_built && sc_slot_owned();
        abandon_session();
        if (keep) { g_sc_slot = slot; g_hidden = hidden; g_sc_committed = 1; }
        /* Native transition owns the new field flags; discard our old ownership. */
        g_yellow=0; hush_uninstall("session boundary");
    }
    generation = stats.session_generation; session_type = SESSION_TYPE;
    last_time = stable ? SESSION_TIME : 0; was_stable = stable;
    if (g_stop_requested) {
        if (stable) { release("stop requested"); sc_lap_unforce(); sc_unhide(); }
        yellow_set(0); slowcar_unblock(); hush_uninstall("resident stop");
        if(!hook_remove())goto done;
        g_broker.set_preload(g_client, NULL, NULL);
        g_broker.set_restart_guard(g_client, NULL, NULL);
        if (g_veh_handle) { RemoveVectoredExceptionHandler(g_veh_handle); g_veh_handle = NULL; }
        ftrace_stop();
        InterlockedExchange(&g_stop_done, 1);
        goto done;
    }
    if (InterlockedExchange(&g_track_changed, 0)) reset_track_history();
    /* Finish restoration precedes the normal racing-phase rejection. */
    if(RACE_PHASE==8){sc_finish_pump(sc_session_guard());goto done;}
    /* No partially built objects, no worker-thread restoration, and no
     * reactive teardown rebuild. Construction belongs to loader detours. */
    if (!stable || (RACE_PHASE != 5 && RACE_PHASE != 2)) {
        memset(&g_confirm,0,sizeof(g_confirm));
        if(g_ctl){g_ctl->cmd_safetycar=0;g_ctl->cmd_engage=0;g_ctl->cmd_release=0;g_ctl->cmd_panic=0;g_ctl->cmd_dump=0;g_ctl->cmd_yellow=0;}
        goto done;
    }
    InterlockedExchange(&g_rc->sc_vehicle,sc_slot_owned()?(LONG)VEH(g_sc_slot):0);
    runtime_guard=sc_session_guard();
    rf_veto=sc_redflag_veto();
    int keys_safe=hotkeys_allowed && runtime_guard.valid && runtime_guard.offline &&
        runtime_guard.settled && !runtime_guard.paused && !rf_veto;
    sc_confirm_step(&g_confirm,keys_safe,0,g_sc_state*16+g_ct,g_epoch,
                    (double)GetTickCount64()/1000,g_sc_config.ConfirmSeconds);
    if(keys_safe && fresh_edge[1]){handle_key(HOTKEYS[1]);goto done;}
    if(runtime_guard.valid && runtime_guard.offline && runtime_guard.settled &&
       g_ctl && g_ctl->cmd_panic) {sc_emergency_reset();goto done;}
    if(sc_y_park_tick()) goto done;   /* 0.10.92 (B2r): deferred SC self-park after a refused Y */

    /* RF cannot acquire while this lease is held. Restore first, acknowledge
       last. Never discard original passing/aggression baselines on takeover. */
    if(f1c_rc_read(&g_rc->rf_request)==2) {
        if(g_ctl){g_ctl->cmd_safetycar=0;g_ctl->cmd_engage=0;g_ctl->cmd_release=0;g_ctl->cmd_panic=0;g_ctl->cmd_yellow=0;}
        sc_rf_yield(runtime_guard.valid && runtime_guard.offline && runtime_guard.settled);
        goto done;
    }
    if(g_emergency_failed){sc_request_fault(F1C_SCF_RESET);goto done;}
    /* Ownership can return in the same session after a clean RF cancellation. */
    g_rf_stand_down=0;
    /* The reviewed phase-2 loader parking path is not race control. Retain
       only that known-offline maintenance; no keys, GUI, weather, or pace
       pumps may run before a settled race. */
    if (RACE_PHASE==2 && runtime_guard.valid && runtime_guard.offline &&
        (runtime_guard.game_state==5 || runtime_guard.game_state==7) &&
        !runtime_guard.paused && !rf_veto && !g_rf_stand_down &&
        !g_engaged && g_sc_state==SC_IDLE) {
        sc_pump();
        goto done;
    }
    /* Menu/replay/exit/unknown/offline/pause preserve loans without executing
       any SC control, GUI, loader-maintenance, or competitive pump writes. */
    if (g_rf_stand_down || !runtime_guard.valid || !runtime_guard.offline ||
        !runtime_guard.settled) {
        /* Commands have no session/age field in ABI1.  Consume them whenever
           race control is unauthorized so a menu/pause command cannot fire on
           a later resume. */
        if(g_ctl){g_ctl->cmd_safetycar=0;g_ctl->cmd_engage=0;g_ctl->cmd_release=0;
            g_ctl->cmd_panic=0;g_ctl->cmd_dump=0;g_ctl->cmd_yellow=0;}
        goto done;
    }
    /* Native rebuilding may temporarily invalidate field reads. Evaluate
       ownership only after stable, settled, offline guards above. A missing
       identity is a separate fault hold, never a fabricated pit handover. */
    if((g_engaged || g_sc_state!=SC_IDLE) && g_sc_slot<0)sc_request_fault(F1C_SCF_IDENTITY);
    if(g_sc_slot>=0) {
        int held=g_identity_guard.holding;
        if(!sc_identity_guard_step(&g_identity_guard,g_epoch,stable,
                                    sc_slot_owned(),g_engaged || g_sc_state!=SC_IDLE,&g_restart_fault)) {
            if(g_engaged || g_sc_state!=SC_IDLE) {
                if(!stable)g_sc_identity_fault_since=-1.0;
                else if(g_sc_identity_fault_since<0 || SESSION_TIME<g_sc_identity_fault_since)
                    g_sc_identity_fault_since=SESSION_TIME;
                if(stable && SESSION_TIME-g_sc_identity_fault_since>=3.0)
                    sc_request_fault(F1C_SCF_IDENTITY);
                g_restart_hold=1;yellow_set(1);
                hud_notice("SAFETY CAR UNAVAILABLE");
                if(!held)logf_("[identity-hold] sim=%.3f sc=%d ct=%d hidden=%d slot=%d expected=%d actual=%d team=%d; phase retained",
                    SESSION_TIME,g_sc_state,g_ct,g_hidden,g_sc_slot,g_sc_roster,
                    V_I32(g_sc_slot,OFF_ROSTER),V_I32(g_sc_slot,OFF_TEAMID));
            }
            goto done;
        }
        g_sc_identity_fault_since=-1.0;
        if(held)logf_("[identity-hold] recovered sim=%.3f sc=%d ct=%d hidden=%d other_fault=%d",
            SESSION_TIME,g_sc_state,g_ct,g_hidden,g_restart_fault);
    }
    /* Repeated render/broker callbacks at one physics time are not new evidence. */
    fresh_sim=sc_sim_tick_advance(&competitive_tick,g_epoch,SESSION_TIME);
    sc_perf_step("guards-and-session");
    if(fresh_sim){g_evidence_now=sc_evidence_clock_step(&g_evidence_clock,g_epoch,SESSION_TIME,(double)GetTickCount64()/1000.0);weather_pump();}
    for (i = 0; i < NKEYS; i++) {
        if (keys_safe && i!=1 && fresh_edge[i]) handle_key(HOTKEYS[i]);
    }
    /* Fresh user input is honored even on duplicate simulation callbacks. */
    if(!fresh_sim)goto done;
    if (g_engaged && !phase_racing()) {
        g_silent_release=1;release("race phase left");yellow_set(0);
    }
    if (!phase_racing()) sc_lap_unforce();
    if (phase_racing()) learn_lap_length();
    if (!g_engaged && phase_racing()) prof_sample();
    sc_perf_step("weather-keys-profile");
    sc_inverted_engine_pump(); /* native engine-off only after confirmed inversion */
    sc_retirement_pump(); /* classify before restart_pump can acquire a new ledger entry */
    sc_native_removal_pump(); /* native expiration only, no forced retirement */
    ai_interaction_audit();
    sc_handover_pump();
    speed_hist_pump(); sc_pace_sample(); hold_pump();
    sc_perf_step("field-history-handover");
    sc_pose_pump(); sc_lap_pump(); sc_pos_diag(); sc_pump(); restart_pump();
    sc_np_live_pump();
    sc_perf_step("sc-and-restart");
    carrier_pump(); slowcar_block(); hush_pump(); say_pump();
    sc_perf_step("commentary");
    formation_pump(); caution_pump(); pit_watch(); sc_probe(); sc_reenable(); pitstart_pump();
    sc_perf_step("formation-incident-pit");
    ctl_pump(); telemetry_pump(); hud_pump();
    sc_perf_step("control-telemetry-hud");
done:
    if(g_rc && f1c_rc_read(&g_rc->owner)==F1C_RC_SC) {
        for(int k=0;k<F1C_RC_SLOTS;k++)
            InterlockedExchange(&g_rc->controlled[k],
                (k<VEH_SLOTS && g_armed[k] && isfinite(g_last_written[k]) &&
                 g_last_written[k]<=2.0f && capture_identity_owned(k))?(LONG)VEH(k):0);
        if(!g_emergency_failed && !g_engaged && g_ct==CT_OFF && g_sc_state==SC_IDLE &&
           !sc_has_control_loans() && !g_yellow && !g_sc_lap_forced)
            f1c_rc_sc_release(g_rc);
    }
    sc_perf_finish();
    InterlockedExchange(&g_pending, 0);
}

/* Lightloader and the broker initialize asynchronously. Only discovery is
 * retried here; no game state is touched before capability negotiation. */
static int wait_for_broker(void)
{
    unsigned attempt;
    F1CBrokerDiscoveryResult result = F1C_BROKER_DISCOVERY_NOT_FOUND;
    g_broker.structure_size = sizeof(g_broker);
    for (attempt = 0; attempt < 200; ++attempt) {
        result = f1c_broker_discover(14, F1C_BROKER_ABI_VERSION,
                              F1C_BROKER_CAP_LIFECYCLE | F1C_BROKER_CAP_RESTART_GUARD | F1C_BROKER_CAP_PRELOAD | F1C_BROKER_CAP_CALLBACK_RECOVERY | F1C_BROKER_CAP_SESSION_END | F1C_BROKER_CAP_RESULTS_FINALIZE,
                              &g_broker);
        if (result == F1C_BROKER_DISCOVERY_OK) return 1;
        if (result != F1C_BROKER_DISCOVERY_NOT_FOUND &&
            result != F1C_BROKER_DISCOVERY_QUERY_FAILED) break;
        Sleep(50);
    }
    logf_("[startup] broker discovery failed: code=%d (1=bad argument, 2=not found, 3=duplicate, 4=query failed, 5=ABI, 6=capability, 7=exports)", (int)result);
    return 0;
}

/* 0.10.91: essential broker/recovery/session-end registration.
 * No licence, activation or PC-binding path. */
static __attribute__((noinline,optimize("O0"))) int sc_private_startup(void)
{
    int result = 11;
    {
        if (wait_for_broker()) {
            result = 12;
            if (g_broker.register_client(g_hinst, "safety_car_clk2002", &g_client) == F1C_BROKER_OK) {
                result = 14;
                if (f1c_broker_recovery_bind(&g_broker,&g_recovery) &&
                    g_recovery.set_recovery(g_client,callback_recovery,NULL)==F1C_BROKER_OK) {
                    result = 15;
                    g_set_session_end=f1c_broker_session_end_bind(&g_broker);
                    if (g_set_session_end && g_set_session_end(g_client,sc_session_end,NULL)==F1C_BROKER_OK)
                        result = 0;
                }
                if (result != 0) { g_broker.unregister_client(g_client,0); g_client=0; }
            }
        }
    }
    return result;
}

static int sc_settings_path(char *path,DWORD capacity) {
    if(!g_hinst || !path || !capacity)return 0;
    DWORD length=GetModuleFileNameA(g_hinst,path,capacity);
    if(!length || length>=capacity)return 0;
    char *slash=strrchr(path,'\\');
    if(!slash)return 0;
    size_t prefix=(size_t)(slash-path)+1;
    if(prefix+sizeof("safetycar.ini")>capacity)return 0;
    memcpy(path+prefix,"safetycar.ini",sizeof("safetycar.ini"));
    return 1;
}
static void sc_load_settings(void) {
    char config_path[MAX_PATH];
    if(sc_settings_path(config_path,sizeof(config_path)))sc_config_load(config_path);
    else logf_("[config] cannot resolve DLL directory; startup defaults retained");

    g_pace_speed=(int)g_sc_config.PaceSpeed;
    g_target_gap=(int)g_sc_config.TargetGap;
    g_gather_pace=(int)g_sc_config.GatherPace;
    g_gather_secs=(int)g_sc_config.GatherSeconds;
    g_ct_laps_tgt=(int)g_sc_config.CautionLaps;
    g_wave_policy=(int)g_sc_config.WavePolicy;
    g_bunch=(int)g_sc_config.Bunch;
    g_ct_auto=(int)g_sc_config.AutoCaution;
    g_include_player=(int)g_sc_config.IncludePlayer;
    g_ramp=(int)g_sc_config.Ramp;
    g_telemetry=(int)g_sc_config.Telemetry;
    g_sc_park=(int)g_sc_config.Park;
    g_hide_mode=(int)g_sc_config.HideMode;
    g_hush_want=(int)g_sc_config.Hush;
    g_pitstart=(int)g_sc_config.PitStart;
    g_ct_quick_laps=(int)g_sc_config.QuickLaps;
    g_ct_full_thresh=(int)g_sc_config.FullThreshold;
    g_ct_final_gate=(int)g_sc_config.FinalLapGate;
    g_ct_grace_secs=(int)g_sc_config.GraceSeconds;
    g_ct_hold_secs=(int)g_sc_config.HoldSeconds;
    g_sc_insta=(int)g_sc_config.InstantReturn;
    g_hide_inlap=(int)g_sc_config.HideInlap;
    g_emergency_disarm=(int)g_sc_config.EmergencyDisarmAuto;
    HOTKEYS[1]=(int)g_sc_config.EmergencyKey;HOTKEYS[2]=(int)g_sc_config.DeployKey;
    for(unsigned i=0;i<sizeof(sc_config_specs)/sizeof(sc_config_specs[0]);i++)
        logf_("[config] %s=%.6g",sc_config_specs[i].key,*(double*)((char*)&g_sc_config+sc_config_specs[i].offset));
}

static DWORD WINAPI worker(LPVOID unused)
{
    HANDLE ev;
    HMODULE pinned;
    (void)unused;
    /* A callback in another thread may be paused indefinitely. Pin before
     * exposing any callback; elapsed wall time cannot establish quiescence. */
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)worker, &pinned)) return 1;
    logf_("[startup] worker entry tick=%lu", (unsigned long)GetTickCount());
    logf_("[startup] safety_car_clk2002 v0.10.93 worker started");
    log_flush();
    if (!sc_target_compatible()) {
        logf_("[startup] unsupported target: %s; initialization stopped", sc_target_reason()); log_flush(); return 2;
    }
    sc_load_settings();
    LARGE_INTEGER qf, qa, qb, qc;
    unsigned long startup_ms = 0;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&qa);
    {
        int startup = sc_private_startup();
        QueryPerformanceCounter(&qb);
        if (qf.QuadPart)
            startup_ms = (unsigned long)(((qb.QuadPart - qa.QuadPart) * 1000ULL) / (unsigned long long)qf.QuadPart);
        if (startup != 0) {
            logf_("[startup-stop] private initialization failed, code=%d", startup);
            logf_("[startup] private startup took %lu ms (refused code=%d)", (unsigned long)startup_ms, startup);
            log_flush(); return (DWORD)startup;
        }
    }
    logf_("[startup] ABI 14 broker client registered; callback recovery enabled");
    logf_("[startup] startup took %lu ms", (unsigned long)startup_ms);
    log_flush();
    ev = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT);
    if (!ev) { g_broker.unregister_client(g_client, 0); return 5; }
    ctl_init();
    g_veh_handle = AddVectoredExceptionHandler(1, crash_veh);
    ftrace_start();
    if (g_broker.set_preload(g_client, before_vehicle_load, NULL) != F1C_BROKER_OK) {
        logf_("[startup-stop] preload registration failed");
        InterlockedExchange(&g_stop_requested, 1);
    } else logf_("[startup] pre-vehicle-load callback registered");
    QueryPerformanceCounter(&qc);
    {
        unsigned long ms_since_qa = 0;
        if (qf.QuadPart)
            ms_since_qa = (unsigned long)(((qc.QuadPart - qa.QuadPart) * 1000ULL) / (unsigned long long)qf.QuadPart);
        logf_("[startup] preload registered %lu ms after startup tick=%lu", (unsigned long)ms_since_qa, (unsigned long)GetTickCount());
    }
    /* sc_pose_load() removed: no saved pose is ever applied. */

    logf_("--- safetycar.dll v0.10.93 loaded (full_safety_car caution engine) ---");
    logf_("[startup] free build; no licence or activation check");
    logf_("Configured deploy key toggles deployment/recall; emergency key resets to pits. GUI "
          "else is in the external window: python tools/safety_car_gui.py");
    logf_("defaults: pace=%d m/s, ramp %s over %.0f s, player excluded, "
          "caution laps %d, lapped cars %s, auto-caution %s",
          g_pace_speed, g_ramp ? "ON" : "OFF", RAMP_SECONDS, g_ct_laps_tgt,
          g_wave_policy ? "MAY PASS" : "HOLD", g_ct_auto ? "ON" : "OFF");

    while (!g_stop_done) {
        log_flush();
        scu_pump();
        if (WaitForSingleObject(ev, 50) == WAIT_OBJECT_0) {
            if (!g_stop_requested) logf_("[stop] named stop event signaled");
            InterlockedExchange(&g_stop_requested, 1);
        }
        if (InterlockedCompareExchange(&g_pending, 1, 0) == 0) {
            static int last_error=-1;
            F1CBrokerResult posted=g_broker.post(g_client,game_tick,NULL);
            if(posted!=F1C_BROKER_OK){
                InterlockedExchange(&g_pending,0);
                if(last_error!=(int)posted){
                    F1CBrokerStats stats={0};stats.structure_size=sizeof(stats);
                    if(g_broker.get_stats(&stats)==F1C_BROKER_OK)
                        logf_("[broker-post] result=%d quarantines=%llu max_callback_us=%llu",
                              (int)posted,(unsigned long long)stats.slow_client_quarantines,
                              (unsigned long long)stats.maximum_callback_us);
                    else logf_("[broker-post] result=%d stats unavailable",(int)posted);
                    last_error=(int)posted;
                }
            } else last_error=-1;
        }
        /* A paused/quarantined broker leaves us resident. Never perform game
         * cleanup from this worker, and never free callback code on timeout. */
        if (g_stop_requested) Sleep(50);
    }
    if (g_broker.unregister_client(g_client, 5000) != F1C_BROKER_OK) return 6;
    if (g_ctl) { g_ctl->magic = 0; UnmapViewOfFile((LPCVOID)g_ctl); g_ctl = NULL; }
    if (g_shm) { CloseHandle(g_shm); g_shm = NULL; }
    CloseHandle(ev);
    f1c_broker_release(&g_broker);
    logf_("Safety Car stopped; module retained. Restart game before loading another version.");
    log_flush();
    return 0;
}

/* ---- Lightloader protected plug-in entry (ABI v2), 0.10.92 -------------
 * Owner startup contract 2026-09-24: activate only after the supported
 * Lightloader host protocol is proven (seal + loader proof), never from
 * DllMain. Same reference as Mechanical Failures 1.0.x. Fail-closed: any
 * mismatch returns FALSE and the loader frees the module. */
#include "lightloader_plugin_abi.h"
static volatile LONG g_host_started;
static uint64_t sc_resolve_proof(const LightLoaderProtectedHostV2 *host)
{
    union { FARPROC raw; LightLoaderProofProc typed; } procedure = {0};
    procedure.raw = GetProcAddress(host->loader_module, LLP_PROOF_NAME);
    return procedure.typed ? procedure.typed(LightLoaderProofChallenge(host)) : 0;
}

__declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(const LightLoaderProtectedHostV2 *host)
{
    HANDLE thread;
    if (!host || host->size < sizeof(*host) ||
        host->magic != LLP_ABI_MAGIC ||
        host->abi_version < LLP_MIN_EXCLUSIVE_ABI ||
        host->product_id != LLP_PRODUCT_ID ||
        !(host->capabilities & LLP_CAP_EXCLUSIVE_HOST) ||
        host->compatibility_generation < LLP_EXCLUSIVE_GENERATION ||
        host->process_id != GetCurrentProcessId() ||
        host->seal != LightLoaderHostSeal(host) ||
        sc_resolve_proof(host) != host->proof)
        return FALSE;
    if (InterlockedCompareExchange(&g_host_started, 1, 0) != 0) return TRUE;
    /* Callback registration follows module pinning in the worker. */
    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!thread) { InterlockedExchange(&g_host_started, 0); return FALSE; }
    CloseHandle(thread);
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hinst = hinst;
        DisableThreadLibraryCalls(hinst);
        /* Inert until the Lightloader protected entry proves the host. */
    }
    return TRUE;
}


