#include <stddef.h>
#define RF_HARD_BEGIN(n) ((void)0)
/*
 * gr44.c - Red flag mod for F1 Challenge 99-02
 *
 * THIS FILE IS THE SOURCE. redflag.c is GENERATED from it by make_release.py;
 * editing redflag.c loses your work on the
 * next regen. Build:  python make_release.py && bash build_release.sh redflag.dll
 * Hot-swap without restarting the game:
 *     python ../extended_vanilla_features/tools/dll_unload_signal.py redflag
 *
 * 2026-08-24 v32. OWNER-DIRECTED RETURN TO THE WORKING v13 RETIREMENT UNIT.
 *   Live v31 evidence proved the cached "garage" transforms were grid
 *   placements: Panis, Bernoldi, Webber and Montoya were deliberately pinned
 *   back into grid boxes. Restore the earlier tested unit without variation:
 *   RetiredFlag/ActiveFlag are repeated, retired render/update hooks remain
 *   installed, and scene/physics objects are suppressed every tick from 500ms
 *   after GRID_READY through all of RacePhase 4 and one stable second of live
 *   RacePhase 5. No cached transform and no FinishState write participates.
 *
 * REJECTED 2026-08-24 v31. POST-RESTART RETIREMENT CARRY-OVER: a live automatic-stop
 *   test proved six correctly classified DNFs were rebuilt as pristine grid
 *   entrants. The phase-3 pin wrote RetiredFlag/ActiveFlag but omitted
 *   FinishState and ended at the next race-phase transition. The accepted DNF
 *   list now owns the complete retirement tuple for the remainder of this
 *   exact session and pins each car to its validated per-driver garage
 *   transform. Y=-1000 remains only the missing-transform fallback.
 *
 * 2026-08-02. Six changes; full writeup in README.md "Status 2026-08-02".
 *   (1) NO-OVERTAKE REWRITTEN. The old pair did nothing to the cars: 0x453C is
 *       recalculated every frame by AIAbility, and rewriting race position
 *       0x4060 only froze the timing screen while the AI raced on underneath.
 *       Replaced with pace_probe.c's accepted law through the game's own
 *       governor sub_42ACD0(veh,N) -> veh+0x4538.
 *   (2) NUMPAD2 fired at the pit LANE. pittingStatus 2 is the lane; the BOX
 *       is 3.
 *   (3) SESSION WATCHDOG. `phase` was never reset when a session ended - one
 *       defect behind three tester reports (red flag stuck on in later races,
 *       NUMPAD2 silently dead, a stranded countdown firing RESTART_FLAG in an
 *       unrelated session).
 *   (4) MULTIPLAYER LOCKOUT, now fail-closed.
 *   (5) PWS_RED01 was silent because the mod muted itself 50 ms after firing;
 *       the injector is now the proven emitter path, confirmed via EVENT_TS.
 *   (6) SELF-EJECT, with a teardown that restores the two EXE sprite bytes and
 *       both call-site trampolines. All Beep() calls removed.
 *
 * The 2026-07-25 "REVERTED - DO NOT USE" verdict on this file is WITHDRAWN:
 * the crash it referred to was never attributed (the sequence completed
 * cleanly and dnf_settled() never engaged), and the file has been driven
 * repeatedly since.
 *
 * Fork of gr43.c. HARDENING: (1) restart countdown default set to 15 seconds (was 10 for
 *   debugging). (2) dnf_settled() holds the restart for up to DNF_SETTLE_MAX_MS
 *   (2s) while any swept car is still unclassified by the game. Placement at
 *   restart is decided by FinishState/RetiredFlag, so firing while a wreck is
 *   still unclassified hands it a GRID SLOT. gr43 avoided this only by luck of
 *   timing; this makes the assumption explicit and bounded. Applied to BOTH the
 *   NUMPAD1 countdown path and the NUMPAD2 pit-box path.
 *
 * Fork of gr42.c. FIX: gr42 undid a sweep whenever the game had not stamped
 *   fin/ret by restart time — but that delay is VARIABLE and lands right around
 *   the countdown, so a genuine wreck (Hakkinen) was put back on the grid. The
 *   discriminator is DURATION, not which flag is set: sweep now commits only
 *   after a car has read act==0 continuously for KILL_CONFIRM_MS (4s), and
 *   revalidate_dnf() only undoes cars that actually RECOVERED (act back to 1).
 *   (Reported in-game 2026-07-24.)
 *
 * Fork of gr41.c. FIX: ActiveFlag==0 is a LEAD, not a verdict. gr41 swept Nick
 *   Heidfeld on act=0, the game never retired him (still fin=0 ret=0 ten seconds
 *   later), but he had already been stamped QualPos=200 — so the game gridded
 *   him LAST. revalidate_dnf() now runs just before apply_qualpos() and undoes
 *   any late sweep the game did not confirm, restoring the saved QualPos/lap.
 *   (Reported in-game 2026-07-24.)
 *
 * Fork of gr40.c. FIX: hide_dnf_cars() was ONE-SHOT. From gr40 the cars are
 *   un-hidden at the start of each sequence, so they are live scene objects and
 *   the game re-places them at the BACK of the grid (QualPos=200 places last, it
 *   does NOT park a car in the pits). The single 1.4s-after-restart hide was
 *   then overwritten and they reappeared at the end of the grid on the second
 *   red flag. Now re-sunk EVERY tick in phase 3, like re_retire_dnf() already
 *   was; g_dnf_hidden only gates logging. (Reported in-game 2026-07-24.)
 *
 * Fork of gr39.c. FIX: hide_dnf_cars() sank cars to Y=-1000 and NOTHING ever
 *   restored it, so the state leaked across red-flag sequences. On a SECOND red
 *   flag the game could not place those cars in their pit boxes — physics Y was
 *   still -1000 and the SceneRenderUpdate hook skips DNF cars, so the node never
 *   re-synced. Now the pre-hide scene-node Y and physics Y are saved, and
 *   unhide_all() restores them at the start of every new sequence, before the
 *   snapshot. (Reported in-game 2026-07-24.)
 *
 * Fork of gr38.c. DIAGNOSTICS ONLY, no behaviour change: every LATE DNF /
 *   ACTIVE / DNF log line now carries the slot index, driver name and the raw
 *   act/fin/ret fields that triggered it. gr32-gr38 were all debugged from
 *   symptoms because the log never said WHICH car was binned.
 *
 * Fork of gr37.c. FIX: sweep_late_dnf() now follows gr17_CONFIRMED exactly —
 *   classify on the kill switch (ActiveFlag==0) OR the game verdict, and write
 *   QualPos=200 + lap=0 IMMEDIATELY at classification. gr37 classified only on
 *   FinishState/RetiredFlag, which a fresh wreck may not reach for a long time,
 *   so wrecks were never classified and grouped on the straight. Wrecks are
 *   removed by re_retire_dnf() (ActiveFlag=0 ENDS THE PHYSICS STEP) followed by
 *   hide_dnf_cars() (scene node + physics Y=-1000), both in phase 3. Adds
 *   per-car DNF diagnostics to apply_qualpos(). (Reported in-game 2026-07-24.)
 *
 * SUPERSEDED gr37 note: classifies on the GAME S OWN verdict
 *   (FinishState/RetiredFlag) instead of ActiveFlag. ActiveFlag==0 is the visual
 *   kill switch and fires seconds BEFORE the game finishes retiring the car, so
 *   gr35 could sweep a car that was not yet DNF; the restart then revived it and
 *   the GAME gridded it at the back with QualPos=198, a value this mod never
 *   writes. (Hakkinen, reported in-game 2026-07-24.)
 *
 * Fork of gr34.c. FIX: sweep_late_dnf() no longer forces the retirement
 *   end-state. Forging it pre-empted the game's own cleanup, so terminally
 *   damaged cars sat frozen on track instead of being teleported to their
 *   garages. It now CLASSIFIES ONLY. (Reported in-game 2026-07-24.)
 *
 * Fork of gr32.c. BUGFIX: veh_killed() read ActiveFlag(0x45F4) as a FLOAT, but
 *   the field holds the INTEGER 1 for a running car. Reinterpreting int 1 as a
 *   float gives 1.4e-45, which falls inside the (-0.5,0.5) "killed" window — so
 *   veh_killed() returned TRUE for EVERY healthy car. gr30 never noticed: the
 *   auto-detect only fires on a false->true transition and detect_seed() seeds
 *   every car as already-killed, so auto red flag has in fact NEVER fired.
 *   gr32.sweep_late_dnf() acted on the value directly and therefore DNF-ed the
 *   whole field the moment a red flag started (reported in-game 2026-07-24).
 *   Now compared as int32.
 *
 * Fork of gr31.c. Feature: LATE-DNF SWEEP — cars that go terminal DURING
 *   the red-flag window (countdown, or the NUMPAD2 wait) are migrated out of
 *   the restart grid into the DNF list. Previously snapshot_positions() froze
 *   that classification at trigger time, so a car wrecked during the 30s
 *   countdown reappeared on the grid. See sweep_late_dnf().
 *
 * Fork of gr30.c. New feature: PIT-RETIRE (NUMPAD3).
 *
 *   Retires AI cars through THE GAME'S OWN retirement path instead of forging
 *   a retirement end-state. Two writes per car and then hands off:
 *
 *       veh+0xD58 = 0   (byte)  RearWingIntactFlag -> "armed"
 *       veh+0x454C = 1  (int32) callToBox
 *
 *   The car then drives itself to the pits, and at the box
 *   PitStopBlockingCondition_0042aee0's damage term
 *       *(veh+0xD18) && !*(byte*)(veh+0xD58)
 *   goes true, so PitStopDecisionTireCompound_0042a3d0 performs the retirement
 *   itself: RetiredFlag=1 + ActiveFlag=0 in one tick, AND the garage teleport.
 *
 *   WHY THIS MATTERS FOR THIS MOD: re_retire_dnf() below forges the end-state,
 *   which strands cars at the pit box (PhysX ~10.25) instead of the garage
 *   (PhysX ~17.0) — which is the whole reason hide_dnf_cars() has to shove them
 *   to Y=-1000. Cars retired via NUMPAD3 need no such hack; the game parks them
 *   correctly on its own.
 *
 *   In-game confirmed 2026-07-24: 21/22 cars, every one at PhysX 16.91-17.15.
 *   Mechanism + evidence: investigations/part_detach_commentary/README.md
 *   Standalone equivalent: tools/force_wing_retire.py
 *
 *   NOTE: the flag is only a TRIGGER. The car must still reach its pit box, so
 *   retirements cascade over a lap or two rather than firing at once. On lap 1
 *   (cars near pit entry) the whole field goes within ~60s; mid-lap it is slower.
 *
 * Fork of gr29.c. New feature: AUTOMATIC red-flag detection.
 *   Polls the vehicle array in phase 0; when >=3 cars go DNF within an
 *   8s rolling window (multi-car pileup), auto-fires the same red-flag
 *   sequence NUMPAD1 triggers manually. 2-min cooldown after each fire
 *   so the re-retired DNF cars don't re-trigger it. See check_auto_redflag().
 *
 * Fork of gr28.c. New feature: red flag HUD sprite icon.
 *
 *  3. Red flag sprite icon — patches byte at 0x4D8338 in FUN_004D82D0
 *     (VehicleFlagCheck output path) from 0x41 (blue FLAG sprite) to
 *     0x40 (red FLAG sprite in FTingame01.BMP). Writes veh+0x4108 = 1
 *     at NUMPAD1, clears it when phase returns to 0.
 *     Offset 0x4108 confirmed: VehicleFlagCheck() = *(byte*)(DAT_7C54E0 + 0x4108).
 *
 * Build:
 *   C:\msys64\mingw32\bin\gcc.exe -shared -O2 -o gr29.dll gr29.c -lkernel32
 * Inject:
 *   mods\red-flag-dll\inject.exe mods\grid_reorder\gr29.dll
 */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <limits.h>
#include "rf_completion_policy.h"
#include <mmsystem.h>
#define F1C_CUSTOM_VEH_ACCESS 1
#include "tools/sdk/f1c_sdk.h"
#include "tools/sdk/f1c_orientation.h"
#include "tools/sdk/f1c_incident_geometry.h"
#include "tools/sdk/f1c_race_control.h"
#include "tools/sdk/f1c_restart_penalties.h"
#include "tools/sdk/f1c_broker_penalty_hold.h"
#include "tools/sdk/f1c_restart_fuel.h"
#include "tools/sdk/f1c_pit_strategy.h"
#include "tools/sdk/f1c_vehicle_lifecycle.h"
#include "tools/sdk/f1c_sc_fault.h"
#include "tools/sdk/f1c_live_replay.h"
/* 1.1.6: consent-gated per-session usage summary, built on the shared SDK core
 * (f1c_usage.h). The summary is flushed from the broker session-end callback
 * and the worker unload path, never from DllMain. */
#include "rf_usage.h"
static F1CRaceControl *g_rc;
static F1CSCFault *g_sc_fault_signal;
static int rf_rc_acquire(void) {
    if(!g_rc)return 0;
    if(!f1c_rc_read(&g_rc->sc_registered)) {
        HANDLE old=OpenFileMappingA(FILE_MAP_READ,FALSE,"F1C_PACEPROBE_CTL");
        if(old){CloseHandle(old);f1c_rc_request(g_rc,2);return 0;}
    }
    return f1c_rc_rf_acquire(g_rc);
}

/* Section markers (no-ops in the public build). */
#define RF_MARK_BEGIN_VIRTUALIZATION(name) ((void)0)
#define RF_MARK_BEGIN_MUTATION(name) ((void)0)
#define RF_MARK_END() ((void)0)

/* Private LightLoader exclusivity ABI v2. DllMain is deliberately inert; the gameplay
 * worker starts only after the loaded LightLoader module supplies and proves
 * a fresh process-bound host record through the exported entry below. */
#define RFP_ABI_MAGIC              0x7B314C50u
#define RFP_MIN_EXCLUSIVE_ABI      0x00020000u
#define RFP_HUB_2_GENERATION       2u
#define RFP_CAP_EXCLUSIVE_HOST     UINT64_C(0x0000000000000001)
#define RFP_CAP_HUB_2              UINT64_C(0x0000000000000002)
#define RFP_PRODUCT_ID             UINT64_C(0x9d4b1c72e6a30f58)

typedef uint64_t (__cdecl *RfLoaderProofProc)(uint64_t nonce);

typedef struct RfLoaderHostV2 {
    uint32_t size;
    uint32_t magic;
    uint32_t abi_version;
    uint32_t process_id;
    HMODULE loader_module;
    uint64_t nonce;
    uint64_t product_id;
    uint64_t capabilities;
    uint32_t compatibility_generation;
    uint32_t reserved;
    uint64_t seal;
    uint64_t proof;
} RfLoaderHostV2;

static __attribute__((always_inline)) inline uint64_t rf_mix64(uint64_t value)
{
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

static __attribute__((always_inline)) inline uint64_t rf_host_seal(const RfLoaderHostV2 *host)
{
    return rf_mix64(
        host->nonce ^ UINT64_C(0xd6e8feb86659fd93) ^
        (uint64_t)(uintptr_t)host->loader_module ^
        ((uint64_t)host->process_id << 32) ^ host->magic ^ host->abi_version ^
        host->product_id ^ host->capabilities ^
        ((uint64_t)host->compatibility_generation << 32));
}

static __attribute__((always_inline)) inline uint64_t rf_loader_proof_challenge(const RfLoaderHostV2 *host)
{
    return rf_mix64(
        host->nonce ^ host->product_id ^ host->capabilities ^
        ((uint64_t)host->compatibility_generation << 32) ^
        host->abi_version);
}

static __attribute__((always_inline)) inline BOOL rf_loader_exclusivity_matches(
    const RfLoaderHostV2 *host)
{
    BOOL matches;

    matches = host->abi_version >= RFP_MIN_EXCLUSIVE_ABI &&
        host->product_id == RFP_PRODUCT_ID &&
        (host->capabilities & (RFP_CAP_EXCLUSIVE_HOST | RFP_CAP_HUB_2)) ==
            (RFP_CAP_EXCLUSIVE_HOST | RFP_CAP_HUB_2) &&
        host->compatibility_generation >= RFP_HUB_2_GENERATION;

    return matches;
}

#define HOTKEY          VK_NUMPAD1   /* debug: organic automatic-stop sequence  */
#define HOTKEY_REDFLAG  VK_NUMPAD2   /* full red flag: AI to pits, restart when
                                        the PLAYER reaches his own pit box      */
#define HOTKEY_PITRET   VK_NUMPAD3   /* pit-retire every AI car (player exempt) */

/* Safety valve for NUMPAD2: if the player never pits, abort and put the field
 * back rather than leaving the race in limbo. */
#define REDFLAG_WAIT_TIMEOUT_MS  (5 * 60 * 1000)

/* Hard ceilings on post-restart phases. Phase 2 deliberately includes the
 * player-controlled garage/setup screen, so it gets a longer bound. Phase 3
 * is automatic cleanup and must never take two minutes. */
#define PHASE_MAX_MS             (2 * 60 * 1000)
#define RESTART_GARAGE_MAX_MS    (10 * 60 * 1000)
#define GREEN_RESTORE_STABLE_MS  1000u
#define GREEN_RESTORE_TIMEOUT_MS 5000u
#define VEHICLE_COUNT   (*(volatile int*)0x005C329C)
#define VEHICLE_ARRAY   ((volatile void**)0x007C54F0)
#define PLAYER_VEH_PTR  (*(volatile uintptr_t*)0x007C54E8)
#define SESSION_TYPE    (*(volatile int*)0x007C56A4)
#define GAME_STATE      (*(volatile int*)0x007C569C)
#define RESTART_FLAG    ((volatile char*)0x007CE25D)
#define PAUSE_FLAG      (*(volatile BYTE*)0x007C56AC)
#define RACE_PHASE      (*(volatile int*)0x007C56A8)
#define RACE_DISTANCE_SCALE (*(volatile float*)G_RaceSetupScaling)
#define BUILD_ACTIVE_VEHICLE_POINTER_ARRAY 0x0042BC80u
typedef int (__cdecl *BuildActiveVehiclePointerArrayFn)(void);

#define CONTROLLER      0x005EA618
#define EVENT_TABLE     0x005BF9A0
#define COMMENTARY_ENABLED ((volatile BYTE*)(CONTROLLER + 0xAF0))
#define ON_PATH_WETNESS  (*(volatile float*)0x008AFEE8)
#define OFF_PATH_WETNESS (*(volatile float*)0x008AFEEC)

/* UIControlWindowInitialization constructs the persistent PlayerOptions object
 * at byte_7CE498 (sub_4A6FE0 passes that exact address). 0x007CE2E8 is NOT the
 * object pointer: FlagSetterWithGlobalUpdate uses it only while constructing
 * the option registry and clears it at the end of PlayerOptionsInitializer.
 *
 * Vehicle Removal is the first scalar entry after the seven 36-byte Opponents
 * entries, at object+9696. Its live/default float pair is +20/+24. A bounded
 * pre-restart stage temporarily makes the native stationary-removal path
 * immediate, waits for RetiredFlag (the confirmed garage discriminator),
 * caches the resulting real garage transforms, and restores all three reviewed
 * values to 60 before RESTART_FLAG is armed. */
#define PLAYER_OPTIONS_BASE      ((uintptr_t)0x007CE498)
#define VEHICLE_REMOVAL_ENTRY_OFF 9696
#define CLEANUP_OPTION_STRIDE    36
#define CLEANUP_OPTION_COUNT     3  /* Vehicle Removal, Debris Removal, Parts Duration */
#define OPTION_VALUE_A_OFF       20
#define OPTION_VALUE_B_OFF       24
#define VEHICLE_REMOVAL_RUNTIME  ((volatile DWORD*)0x007D0A8C)
#define CLEANUP_NORMAL_BITS      0x42700000u /* IEEE-754 60.0f */
#define OFF_AI_PIT_TRIGGER       0x4170
#define OFF_AI_CURRENT_TIME      0x4480
#define FLT_MAX_BITS             0x7F7FFFFFu
#define DYNAMIC_MANAGER_OBJECT     0x005EA560u
#define DYNAMIC_MANAGER_MAX_ACTIVE 4096
#define DYNAMIC_MANAGER_FIELDS      27u
#define DYNAMIC_LOOSE_POOL_TYPES     6
#define DYNAMIC_LOOSE_POOL_BASE      9u
#define DYNAMIC_LOOSE_POOL_STRIDE    3u
#define DYNAMIC_DESCRIPTOR_SIZE    36u
#define DYNAMIC_DESCRIPTOR_TYPE    28u
#define DYNAMIC_DESCRIPTOR_BODY    32u
#define DYNAMIC_BODY_EXPIRY        28u
#define DYNAMIC_BODY_CURRENT_TIME  36u

/* PitTimer's phase-4 branch calls the event processor at 0x4D3083. Investigation
 * proved subtypes 2..10 also mutate session/player/car-selection state, so the
 * old cinematic trampoline is retained only for restoration compatibility and
 * is never installed by the restart path. The original processor must run. */
#define GRID_CAMERA_CALL_SITE    0x004D3083
#define GRID_CAMERA_ORIGINAL     0x0040DE60
#define GRID_CAMERA_CALL_SIZE    5
#define CAMERA_MANAGER           ((void*)0x005EA510)
#define CAMERA_ACTION            (*(volatile int*)(0x005EA510 + 12))

static BYTE g_commentary_saved = 1;
static BOOL g_commentary_suppressed = FALSE;
static BOOL g_vehicle_removal_forced = FALSE;
static uintptr_t g_vehicle_removal_options_base = 0;
static DWORD g_vehicle_removal_saved_a[CLEANUP_OPTION_COUNT] = {0};
static DWORD g_vehicle_removal_saved_b[CLEANUP_OPTION_COUNT] = {0};
static DWORD g_vehicle_removal_runtime_saved = 0;
static volatile LONG g_cleanup_loose_active = 0;
static volatile LONG g_cleanup_loose_task_pending = 0;
static volatile LONG g_cleanup_loose_generation = 0;
static volatile LONG g_cleanup_loose_pending_generation = 0;
static volatile LONG g_cleanup_loose_remaining = 0;
static volatile LONG g_cleanup_loose_observed = 0;
static volatile LONG g_cleanup_loose_clamped = 0;
static volatile LONG g_cleanup_loose_invalid = 0;
static volatile uintptr_t g_cleanup_loose_session_epoch = 0;
static DWORD g_cleanup_loose_last_post = 0;
static BOOL g_cleanup_loose_post_failed = FALSE;
static BOOL g_cleanup_loose_clamp_logged = FALSE;
static BOOL g_cleanup_loose_invalid_logged = FALSE;
static volatile LONG g_skip_prerace_cinematic = 0;
static int  g_saved_camera_action = 0;
static BOOL g_camera_action_saved = FALSE;

#define OFF_QUALPOS     0x4058
#define OFF_RACEPOS     0x4060
#define OFF_FINISHSTATE 0x4064
#define OFF_REASONCODE  0x4068
#define OFF_PITSTATE    0x4040
#define OFF_RETIREDFLAG 0x4410
#define OFF_ACTIVEFLAG  0x45F4
#define OFF_DRIVERSTATUS 0x4510 /* telemetry only; NOT a garage discriminator */
#define OFF_GARAGEFLAG_A 0x4488 /* telemetry only; lifecycle bits are ambiguous */
#define OFF_GARAGEFLAG_B 0x4578
#define PLAYER_OUT_FLAG  (*(volatile int*)0x007CE258)
#define OFF_DRIVERNAME  0x4620   /* ASCIIZ, e.g. "Mika Hakkinen (2001)" */
#define OFF_POSX        0x34E0   /* authoritative world position [FACT] */
#define OFF_POSY        0x34E4
#define OFF_POSZ        0x34E8

/* Read-only powertrain and damage fields from the reviewed SDK schema.
 * These are used only to distinguish a crash-disabled car from an ordinary
 * game retirement; this mod never writes them. */
#define OFF_ENGINE_RPM       0x39A0
#define OFF_CAR_CONDITIONS   0x3D6C
#define OFF_ENGINE_LIFE      0x3D70
#define OFF_BRAKE_LIFE       0x3D74
#define OFF_GEARBOX_LIFE     0x3D78
#define OFF_SUSPENSION_LIFE  0x3D7C
#define ENGINE_OFF_RPM       100.0f
#define CRITICAL_LIFE         0.25f

/* --- pit-retire trigger (CONFIRMED 2026-07-24, name_index.json) ---
 * 0xD18/0xD58 = rear-wing damage record (ptr + intact flag, delta 0x40).
 * 0xCD4/0xD14 = front-wing record; deliberately NOT used — the front record is
 * not in PitStopBlockingCondition, so arming it yields a REPAIR, not a retire.
 * The flag holds when poked (it is not recomputed per frame) and the game
 * resets it to 1 once the retirement completes. */
#define OFF_REARWING_PTR  0xD18
#define OFF_REARWING_FLAG 0xD58
#define OFF_FRONTWING_PTR 0xCD4
#define OFF_FRONTWING_FLAG 0xD14
#define OFF_CALLTOBOX     0x454C
#define OFF_PITTING       0x3450

/* ---- the no-overtake pace hold -----------------------------------------
 *
 * REPLACES the old pair (0x453C speed-cap multiplier + a per-tick rewrite of
 * race position at 0x4060). Both were wrong:
 *
 *   - 0x453C is RECALCULATED by the AI every frame (AIAbility moves it by
 *     +-0.01/frame), so an external poke does not hold. Root CLAUDE.md, and
 *     confirmed live during the full_safety_car sessions.
 *   - rewriting +0x4060 changes the STANDINGS, not the cars. The AI kept
 *     racing; only the number on the timing screen was frozen.
 *
 * What actually holds a field is the game's own per-car governor,
 * ParameterValueSetter_0042acd0(veh, N) -> veh+0x4538 maxSpeed. Proven
 * 2026-07-31 and driven end to end in patches/full_safety_car/dll/pace_probe.c:
 * 21 AI cars held at a commanded speed for 143 s, zero game overrides, zero
 * race-position changes, driver verdict "stay in line, no overtakes,
 * deceleration realistic and gradual".
 *
 * The law ported here is pace_probe's accepted one (2026-08-01):
 *
 *     queue leader     -> the pace speed (the only absolute number in the set)
 *     every other car  -> max(current, recent min) speed of the car PHYSICALLY
 *                         ahead  +  APPROACH_GAIN * (gap - TARGET_GAP)
 *
 * No term is independent of the car in front, so at the target gap a pass is
 * arithmetically impossible; inside it the car is commanded slower and drops
 * back; outside it the allowance decays with the room, so a car closing on the
 * pack decelerates continuously instead of arriving at 35 m/s of closing speed.
 * A car with clear air inside APPROACH_RANGE has no obstacle and therefore no
 * cap at all, so it closes at its own race pace.
 *
 * Three guards, each of which was a live regression before it existed:
 *   - the base is max(current, rolling min), never the rolling min alone (a
 *     sagging base ratchets the whole queue down every time anyone lifts);
 *   - an abreast car is an obstacle only to the one worse off in the standings
 *     (mutual caps once deadlocked eleven cars at maxSpeed 5);
 *   - a 60%-of-car-ahead floor and a 45%-of-pace floor, so a queue can
 *     compress but never collapse.
 */
#define VEH_BASE        0x005EC6D8u
#define VEH_STRIDE      0x48C0u
#define OFF_SLOTLIVE    0x32C8u   /* int32, -1 = dead slot, 0 = player, 1 = AI  */
#define OFF_LAPDIST     0x4044u   /* float, metres along the lap [FACT]         */
#define OFF_SPEED       0x4440u   /* float, actual speed         [FACT]         */
#define OFF_UP_VECTOR_Y 0x0720u   /* chassis local-up dot world-up [CONFIRMED] */
#define OFF_OFF_TRACK   0x40E8u   /* byte, broker track-limits snapshot source  */
#define OFF_WHEEL_HIT   0x3674u   /* current collision hit ptr, four wheels     */
#define WHEEL_HIT_STRIDE 0xF8u
#define PIT_STATE_CREDIBLE_MAX_SPEED 30.0f
#define POST_RESTART_NORMALIZE_MS    10000u
#define OFF_MAXSPEED    0x4538u   /* float, the governor         [CONFIRMED]    */
#define OFF_PASSING     0x44CCu   /* float, PassingNorm          [CONFIRMED]    */
#define OFF_AGGRO       0x44F8u   /* float, TrackAggression      [CONFIRMED]    */

#define ADDR_SET_MAXSPEED  0x0042ACD0u
#define SPEED_RELEASE      99999   /* the game's own release value              */
#define SPEED_IDLE_SENT    18      /* a2 == 18 means PitSpeedLimit, never send  */
#define PACE_MIN           5
#define PACE_TARGET        30      /* m/s = 108 km/h, the red-flag pace         */
#define RAMP_SECONDS       6.0f    /* commanding the target directly braked the
                                    * whole grid at ~15 m/s^2                   */
#define TARGET_GAP         20.0f   /* metres of mandatory spacing               */
#define APPROACH_RANGE     400.0f  /* metres: start easing off this far out     */
#define APPROACH_GAIN      0.22f   /* m/s of closing allowed per metre of room  */
#define BLOCK_RANGE        30.0f   /* collision floor vs a car out of position  */
#define BLOCK_GAP          12.0f
#define ABREAST_WINDOW     7.0f    /* metres either side = level with us        */
#define OBSTACLE_MIN       8.0f    /* below this it is a wreck, not a pace car  */
#define SPD_HIST           10      /* samples at ~20 Hz = 0.5 s of history      */
#define PASSING_MIN        0.9f    /* = .rcd Passing 0, "never try"             */
#define AGGRO_DEFAULT      0.65f   /* what the whole 1999 field reads           */

#define OFF_CURRENTLAP  0x403C
#define OFF_FUELLEVEL   0x3984
#define OFF_DRIVER_VISIBLE 0x105D
/* Restart configuration input and the confirmed live HUD/scoring total are
 * different globals. Game_Restart copies the former into the latter. The HUD
 * formatter at 0x0049C670 reads LIVE_TOTAL_LAPS_PTR directly. */
#define RESTART_TOTAL_LAPS_PTR (*(volatile int*)0x007D05A0)
#define LIVE_TOTAL_LAPS_PTR    (*(volatile int*)0x008ACF8C)

/* Per-vehicle storage is allocated from VEHICLE_COUNT when a race session is
 * entered. There is no product-defined car cap: corrupt/unaddressable arrays
 * and allocation failure are rejected, otherwise the live field size owns the
 * capacity. */
static int g_session_capacity = 0;
static BOOL g_session_active = FALSE;
static uintptr_t g_session_epoch_id = 0;
static unsigned long g_session_serial = 0;
static DWORD g_player_box_still_since = 0;
static BOOL g_dnf_sweep_warning_logged = FALSE;
static void** g_removal_repair_ptrs = NULL;
static DWORD* g_removal_repair_saved = NULL;
static int g_removal_repair_count = 0;
static uintptr_t g_removal_repair_epoch = 0;

/* LightLoader-owned common game-update broker. Red Flag never LoadLibrarys
 * this service: it accepts only the already loaded broker instance, registers
 * its own module, and posts bounded callbacks onto the proven game thread. */
typedef DWORD RfBrokerToken;
typedef int (__cdecl *RfBrokerRegisterFn)(HMODULE, const char*, RfBrokerToken*);
typedef int (__cdecl *RfBrokerUnregisterFn)(RfBrokerToken, DWORD);
typedef void (__cdecl *RfBrokerTaskFn)(void*);
typedef int (__cdecl *RfBrokerPostFn)(RfBrokerToken, RfBrokerTaskFn, void*);
typedef struct RfBrokerInfo {
    DWORD structure_size;
    DWORD abi_version;
    DWORD min_compatible_abi;
    DWORD max_vehicle_slots;
    unsigned __int64 capabilities;
} RfBrokerInfo;
typedef int (__cdecl *RfBrokerGetInfoFn)(RfBrokerInfo*);
typedef enum RfBrokerRestartDecision {
    RF_BROKER_RESTART_ALLOW = 0,
    RF_BROKER_RESTART_BLOCK = 1
} RfBrokerRestartDecision;
typedef RfBrokerRestartDecision (__cdecl *RfBrokerRestartGuardFn)(
    void*, DWORD, int);
typedef int (__cdecl *RfBrokerSetRestartGuardFn)(
    RfBrokerToken, RfBrokerRestartGuardFn, void*);
/* ABI 15 optional service: a game-thread callback run after the session ends.
 * Red Flag uses it to write the consent-gated usage summary outside DllMain. */
typedef int (__cdecl *RfBrokerSetSessionEndFn)(
    RfBrokerToken, RfBrokerTaskFn, void*);
#define RF_BROKER_OK 0
#define RF_BROKER_STALE_TOKEN 4
#define RF_BROKER_MIN_ABI 13u
#define RF_BROKER_DEPENDENCY_WAIT_MS 15000u
#define RF_BROKER_CAP_LIFECYCLE 1ull
#define RF_BROKER_CAP_RESTART_GUARD (1ull << 8)
#define RF_BROKER_CAP_SESSION_END (1ull << 11)

/* Diagnostic-only HUD denominator isolation. The native formatter at
 * 0x0049C6D0 builds " position / entrants " by loading VehicleCount directly:
 *
 *   A1 9C 32 5C 00       mov eax,[0x005C329C]
 *   8B 0D E0 54 7C 00    mov ecx,[0x007C54E0]
 *
 * A reduced restart roster must remain reduced for replay/scene ownership,
 * but the display denominator still belongs to the original entrant field.
 * The trial redirects only the four-byte absolute operand at +1 to an aligned
 * DLL-owned shadow. It never changes VehicleCount for presentation. */
#ifndef RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR
#define RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR 0
#endif
#define HUD_POSITION_COUNT_LOAD       0x0049C6D0u
#define HUD_POSITION_COUNT_OPERAND    (HUD_POSITION_COUNT_LOAD + 1u)
#define HUD_POSITION_COUNT_ORIGINAL   0x005C329Cu
static const BYTE g_hud_position_count_prefix[11] = {
    0xA1, 0x9C, 0x32, 0x5C, 0x00,
    0x8B, 0x0D, 0xE0, 0x54, 0x7C, 0x00
};
static __attribute__((aligned(4))) volatile LONG
    g_hud_position_denominator = 0;
static volatile LONG g_hud_patch_task_state = 0; /* 0 idle,1 queued,2 claimed,3 done */
static volatile LONG g_hud_patch_task_op = 0;    /* 1 install, 2 remove */
static volatile LONG g_hud_patch_task_result = 0;
static BOOL g_hud_position_patch_installed = FALSE;
static BOOL g_hud_position_denominator_frozen = FALSE;
static BOOL g_hud_patch_unload_safe = TRUE;

static HMODULE g_broker_module = NULL;
static RfBrokerRegisterFn g_broker_register = NULL;
static RfBrokerUnregisterFn g_broker_unregister = NULL;
static RfBrokerPostFn g_broker_post = NULL;
static RfBrokerGetInfoFn g_broker_get_info = NULL;
static RfBrokerSetRestartGuardFn g_broker_set_restart_guard = NULL;
static RfBrokerSetSessionEndFn g_broker_set_session_end = NULL;
/* Set only after the broker accepted the session-end registration; cleared
 * before the client is unregistered so no stale callback survives. */
static BOOL g_broker_session_end_registered = FALSE;
/* Replay finalization and DynMan cleanup can legitimately exceed the broker's
 * 5 ms callback budget and quarantine their clients. Never let either
 * invalidate the separate race-control token used for the atomic results-
 * roster restoration. */
static RfBrokerToken g_broker_control_token = 0;
static RfBrokerToken g_broker_replay_token = 0;
static RfBrokerToken g_broker_cleanup_token = 0;
static volatile LONG g_broker_green_task_pending = 0;
static volatile LONG g_grid_fuel_restored = 0;
static volatile LONG g_green_restore_posts = 0;
static volatile LONG g_green_restore_callbacks = 0;
static volatile LONG g_green_restore_post_failures = 0;
static volatile LONG g_green_restore_last_post_result = 0;
/* Post-red-flag vanilla Y restart handshake. 0=idle, 1=requested,
 * 2=worker preparing the reduced roster, 3=complete. The broker holds the
 * game thread at its existing RestartGate while this worker owns preparation. */
static volatile LONG g_y_restart_guard_armed = 0;
static volatile LONG g_y_restart_prepare_state = 0;
static volatile LONG g_y_restart_prepare_result = 0;
static volatile LONG g_owned_restart_gate_pending = 0;
static volatile LONG g_restart_cancelled = 0;
static volatile LONG g_restart_commit_blocked = 0;
/* Vehicle pointers and SessionType alone survive the results screen. */
static BOOL runtime_region_supported(uintptr_t address, SIZE_T size, BOOL executable);
static BOOL restart_context_allowed(int game, int race, int session,
                                    BOOL paused, int player_out,
                                    BOOL cancelled, BOOL escape) {
    (void)escape; /* Actual session exit is checked; Esc alone is not RF cancellation. */
    return session == 7 && (game == 5 || game == 7) &&
        (race == 3 || race == 5) && !paused && !player_out &&
        !cancelled;
}
/* RF84: native restart resets a terminal player even when PLAYER_OUT is zero.
 * Until that lifecycle can be preserved, reject before constructing a reduced
 * field. ActiveFlag/speed are deliberately not retirement verdicts. */
static BOOL restart_player_supported(void) {
    uintptr_t player = PLAYER_VEH_PTR;
    if (!player || !runtime_region_supported(player, VEH_STRIDE, FALSE))
        return FALSE;
    int finish = *(volatile int*)(player + OFF_FINISHSTATE);
    BYTE retired = *(volatile BYTE*)(player + OFF_RETIREDFLAG);
    return PLAYER_VEH_PTR == player && finish == 0 && retired == 0;
}
static BOOL restart_context_live(void) {
    return restart_context_allowed(GAME_STATE, RACE_PHASE, SESSION_TYPE,
        PAUSE_FLAG != 0, PLAYER_OUT_FLAG,
        InterlockedCompareExchange(&g_restart_cancelled, 0, 0) != 0,
        (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) &&
        restart_player_supported();
}
static volatile LONG g_full_y_reset_pending = 0;
static BOOL g_broker_registered = FALSE;
#define RETIRE_SECS     3

#define CALL_SITE           0x0042C409
#define CALL_SIZE           5
#define SCENE_RENDER_UPDATE 0x004C6D90
#define TICK_CALL_SITE      0x0042D1E1
#define SESSION_TICK_UPDATE 0x004C6B60
#define REPLAY_RECORDER_PTR_GLOBAL 0x008AB6ECu
#define REPLAY_FINALIZE_ORIGINAL   0x004B7500u

/* Red Flag is deliberately executable-hash agnostic. Different F1C EXEs may
 * change unrelated bytes and remain compatible, but this source still uses
 * fixed 32-bit addresses. Accept only an executable whose required layout,
 * original call targets, patch bytes and callable regions all validate. */
static BOOL call_site_targets(DWORD site, DWORD expected_target) {
    if (IsBadReadPtr((const void*)site, CALL_SIZE)) return FALSE;
    const BYTE* bytes = (const BYTE*)site;
    if (bytes[0] != 0xE8) return FALSE;
    int32_t rel = 0;
    memcpy(&rel, bytes + 1, sizeof(rel));
    return (DWORD)((intptr_t)(uintptr_t)site + CALL_SIZE + (intptr_t)rel) ==
           expected_target;
}

#define HUD_OUTER_PTR   0x007cd204
#define SLOT_COUNT      8
#define SLOT_STRIDE     0x5c
#define SLOT_TEXT_OFF   0x3c
#define SLOT_TIMER_OFF  0x90

/* Red flag sprite patch — FUN_004D82D0 VehicleFlagCheck branch.
 * Byte at 0x4D8338 is the immediate in: mov [esp+esi+0Ch], 41h
 * Changing 0x41 (blue FLAG sprite) to 0x40 (red FLAG sprite) in FTingame01.BMP.
 * Trigger: write veh+0x4108 = 1 (VehicleFlagCheck reads DAT_7C54E0+0x4108).
 * The patch is applied only during a red-flag sequence and restored on every
 * exit. Normal blue-flag procedure therefore remains sprite 0x41. */
#define SPRITE_PATCH_SITE   0x4D8338
#define SPRITE_PATCH_SITE2  0x4DBAE3
#define SPRITE_PATCH_OLD    0x41
#define SPRITE_PATCH_NEW    0x40
/* v69: the two existing renderer sites share TEST AL,AL; JE +6;
 * MOV byte [ESP+ESI+disp],sprite. While stopped, JE +0 makes both
 * outcomes draw the sprite. Only one-byte operands change: no torn opcode
 * and no native vehicle flag writes or DLL callback in the render path. */
static const BYTE g_icon_renderer1[] = {0x84,0xC0,0x74,0x06,0xC6,0x44,0x34,0x0C,0x41};
static const BYTE g_icon_renderer2[] = {0x84,0xC0,0x74,0x06,0xC6,0x44,0x34,0x08,0x41};
static BOOL g_icon_owned[2] = {FALSE, FALSE};
#define VEH_OBJ_PTR_GLOBAL  0x007C54E0
#define OFF_VEHICLE_FLAG    0x4108

static BOOL runtime_region_supported(uintptr_t address, SIZE_T size,
                                     BOOL executable) {
    uintptr_t cursor = address;
    uintptr_t end = address + size;
    if (!size || end < address) return FALSE;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T queried = VirtualQuery((const void*)cursor, &mbi, sizeof(mbi));
        if (queried != sizeof(mbi) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return FALSE;
        if (executable) {
            DWORD p = mbi.Protect & 0xff;
            if (p != PAGE_EXECUTE && p != PAGE_EXECUTE_READ &&
                p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY)
                return FALSE;
        }
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= cursor) return FALSE;
        cursor = next;
    }
    return TRUE;
}

static BOOL runtime_pe32_layout_supported(void) {
    HMODULE module = GetModuleHandleW(NULL);
    uintptr_t base = (uintptr_t)module;
    if (!module || base != 0x00400000u ||
        !runtime_region_supported(base, sizeof(IMAGE_DOS_HEADER), FALSE))
        return FALSE;
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return FALSE;
    uintptr_t nt_address = base + (uintptr_t)dos->e_lfanew;
    if (!runtime_region_supported(nt_address, sizeof(IMAGE_NT_HEADERS32), FALSE))
        return FALSE;
    const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)nt_address;
    return nt->Signature == IMAGE_NT_SIGNATURE &&
           nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
           nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
           nt->OptionalHeader.ImageBase == 0x00400000u &&
           nt->OptionalHeader.SizeOfImage >= (0x008AFEF0u - 0x00400000u);
}

static BOOL runtime_layout_supported(void) {
    static const uintptr_t callable[] = {
        ADDR_SET_MAXSPEED, GRID_CAMERA_ORIGINAL, SCENE_RENDER_UPDATE,
        SESSION_TICK_UPDATE, REPLAY_FINALIZE_ORIGINAL,
        0x004144D0u, 0x004144F0u, 0x00418560u,
        0x0040CDE0u, BUILD_ACTIVE_VEHICLE_POINTER_ARRAY
    };
    static const uintptr_t readable[] = {
        0x005C329Cu, 0x007C54F0u, 0x007C54E8u, 0x007C56A4u,
        0x007C569Cu, 0x007CE258u, 0x007CE25Du, 0x007C56ACu, 0x007C56A8u,
        CONTROLLER, EVENT_TABLE, 0x008AFEE8u, 0x008AFEECu,
        PLAYER_OPTIONS_BASE, (uintptr_t)VEHICLE_REMOVAL_RUNTIME,
        (uintptr_t)CAMERA_MANAGER, VEH_BASE, 0x007D05A0u,
        (uintptr_t)G_RaceSetupScaling,
        HUD_OUTER_PTR,
        VEH_OBJ_PTR_GLOBAL, 0x005EA2C0u, 0x008ACEE0u, 0x007C54E0u,
        0x005EA3FCu, 0x005EA400u, 0x005EA404u, 0x007CD25Cu,
        REPLAY_RECORDER_PTR_GLOBAL
    };
    size_t i;
    if (!runtime_pe32_layout_supported() ||
        !call_site_targets(CALL_SITE, SCENE_RENDER_UPDATE) ||
        !call_site_targets(TICK_CALL_SITE, SESSION_TICK_UPDATE) ||
        !call_site_targets(GRID_CAMERA_CALL_SITE, GRID_CAMERA_ORIGINAL) ||
        !runtime_region_supported(SPRITE_PATCH_SITE, 1, FALSE) ||
        !runtime_region_supported(SPRITE_PATCH_SITE2, 1, FALSE) ||
        *(const BYTE*)SPRITE_PATCH_SITE != SPRITE_PATCH_OLD ||
        *(const BYTE*)SPRITE_PATCH_SITE2 != SPRITE_PATCH_OLD)
        return FALSE;
    if (!runtime_region_supported(SPRITE_PATCH_SITE - 8, 9, TRUE) ||
        !runtime_region_supported(SPRITE_PATCH_SITE2 - 8, 9, TRUE) ||
        memcmp((const void*)(SPRITE_PATCH_SITE - 8), g_icon_renderer1, 9) ||
        memcmp((const void*)(SPRITE_PATCH_SITE2 - 8), g_icon_renderer2, 9))
        return FALSE;
#if RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR
    if (!runtime_region_supported(HUD_POSITION_COUNT_LOAD,
                                  sizeof(g_hud_position_count_prefix), TRUE) ||
        memcmp((const void*)HUD_POSITION_COUNT_LOAD,
               g_hud_position_count_prefix,
               sizeof(g_hud_position_count_prefix)) != 0)
        return FALSE;
#endif
    for (i = 0; i < ARRAYSIZE(callable); ++i)
        if (!runtime_region_supported(callable[i], 1, TRUE)) return FALSE;
    for (i = 0; i < ARRAYSIZE(readable); ++i)
        if (!runtime_region_supported(readable[i], sizeof(DWORD), FALSE))
            return FALSE;
    return TRUE;
}

static volatile BOOL g_running = TRUE;
static char g_log[MAX_PATH];

/* ---- SELF-EJECT, so this DLL can be hot-swapped -------------------------
 *
 * DO NOT UNLOAD THIS FROM OUTSIDE. A CreateRemoteThread(FreeLibrary) ejector
 * CRASHED THE GAME on the first real attempt (findings.jsonl DISPROVEN,
 * 2026-07-28): it frees the module while this file's own worker thread is
 * still executing inside it. The safe idiom is the reverse - the worker
 * watches a named event, breaks its own loop, tears everything down and
 * calls FreeLibraryAndExitThread, so the LAST instruction that touches this
 * module is the unload and there is no race.
 *
 * The ejector (tools/dll_unload_signal.py) only has to SetEvent. Named kernel
 * objects cross the 32/64-bit boundary, so 64-bit Python can signal this
 * 32-bit DLL by name.
 *
 * THIS ONE HAS MORE TO UNDO THAN THE COMMENTARY DLLS. It writes two bytes
 * into the EXE (the red-flag sprite immediate) and installs two call-site
 * trampolines. All of that lives OUTSIDE the module and would survive the
 * unload, so eject_teardown() below reverses every one of them, and the
 * worker then waits before freeing so any call already inside a trampoline
 * can finish. The trampoline pages themselves are deliberately NOT freed:
 * unreachable once the call sites are restored, and freeing them is the one
 * thing that could still be jumped into. */
#define UNLOAD_EVENT_NAME "F1C_EVF_UNLOAD_redflag"
static HINSTANCE g_hinst     = NULL;
static volatile LONG g_loader_init_state = 0;
static BOOL g_timer_period_active = FALSE;
static BOOL g_lightloader_owned = FALSE;
static HANDLE    g_unload_ev = NULL;

/* --- Live automatic-trigger monitor -------------------------------------
 * A small modeless Win32 window, owned by the DLL worker thread. It is a
 * read-only display of the same state the automatic trigger evaluates.
 * NUMPAD0 hides/shows it without affecting the race (F10 is an in-game
 * keybind and must not be hijacked).
 *
 * The class name is per-module-instance: a hot-swap cycle that left an older
 * build's class registered (its window was never destroyed) must not make the
 * next build create its window against a dangling window proc. */
#define STATUS_WINDOW_CLASS_PREFIX "F1CRedFlagMonitor"
#define HOTKEY_STATUS       VK_NUMPAD0
#define IN_GAME_MONITOR_ENABLED 0  /* Python field_gui.py is the only GUI. */
static char g_status_class[64] = "";
static HWND  g_status_window = NULL;
static HWND  g_status_text   = NULL;
static BOOL  g_status_visible = TRUE;
static DWORD g_status_last_update = 0;

static BOOL  g_monitor_native_yellow = FALSE;
static DWORD g_monitor_yellow_ms = 0;
static int   g_monitor_terminals = 0;       /* fresh-cluster window count     */
static int   g_monitor_terminal_now = 0;    /* current crash-terminal AI cars */
static DWORD g_monitor_cooldown_ms = 0;
static BOOL  g_monitor_auto_candidate = FALSE;
static BOOL  g_monitor_settled = FALSE;      /* GameState==7 && RacePhase==3 */
static int   g_monitor_game_state = -1;
static int   g_monitor_race_phase = -1;
static int   g_monitor_stalled = 0;          /* engine-off, alive, on-track AI */
static int   g_monitor_stopped = 0;          /* engine on, speed < 0.1 m/s AI */
static int   g_monitor_crashed = 0;          /* engine-off + damage, alive     */
static int   g_monitor_incident_score = 0;   /* best local incident score       */
static int   g_monitor_incident_cars = 0;    /* cars contributing to best score */
static int   g_monitor_incident_terminals = 0; /* terminal cars in best cluster */
static int   g_monitor_incident_damaged_stationary = 0;
static int   g_monitor_incident_ontrack_terminals = 0;
static int   g_monitor_blocking_road_contacts = 0;
static int   g_monitor_inverted_blocker = 0;
static int   g_monitor_passive_road_hazard = 0;
static BOOL  g_player_terminal_detector_logged = FALSE;
static float g_monitor_incident_lapdist = -1.0f;
static int   g_monitor_paused = 0;
static int   g_monitor_multiplayer = 0;
static int   g_monitor_opening_window = 0;
static DWORD g_monitor_yellow_required_ms = 3000;
static DWORD g_monitor_candidate_confirm_ms = 0;
static int   g_monitor_auto_enabled = 0;
static int   g_monitor_stop_reason = 0;
static DWORD g_monitor_prestop_elapsed_ms = 0;
static int   g_monitor_phase = 0;            /* sequence phase for the bridge  */
static DWORD g_post_restart_normalize_until = 0;
static BOOL* g_post_restart_pit_fixed = NULL;

/* AIW corridor telemetry. The player values are published unconditionally so
 * Test Day can calibrate the spatial rule before an accident is staged. The
 * blocking values describe the closest stationary terminal candidate seen by
 * the automatic detector. Branch 0 is the main circuit; branch 1 is the
 * active pit corridor. Grid, garage and auxiliary branches stay excluded. */
static int   g_aiw_valid = 0;
static int   g_aiw_main_waypoints = 0;
static int   g_aiw_pit_waypoints = 0;
static int   g_aiw_player_found = 0;
static int   g_aiw_player_nearest_index = -1;
static int   g_aiw_player_segment_index = -1;
static int   g_aiw_player_band = 0;
static int   g_aiw_player_corridor = -1;
static int   g_aiw_player_main_band = 0;
static int   g_aiw_player_pit_band = 0;
static float g_aiw_player_main_line_m = 0.0f;
static float g_aiw_player_pit_line_m = 0.0f;
static float g_aiw_player_node_m = 0.0f;
static float g_aiw_player_path_m = 0.0f;
static float g_aiw_player_dry_line_m = 0.0f;
static float g_aiw_player_wet_line_m = 0.0f;
static float g_aiw_player_closest_line_m = 0.0f;
static float g_aiw_player_road_left_m = 0.0f;
static float g_aiw_player_road_right_m = 0.0f;
static float g_aiw_player_road_width_m = 0.0f;
static float g_aiw_player_lateral_m = 0.0f;
static float g_aiw_player_free_left_m = 0.0f;
static float g_aiw_player_free_right_m = 0.0f;
static float g_aiw_player_best_pass_m = 0.0f;
static float g_aiw_player_critical_radius_m = 0.0f;
static float g_aiw_player_danger_radius_m = 0.0f;
static float g_aiw_player_speed = 0.0f;
static DWORD g_aiw_player_stationary_ms = 0;
static DWORD g_aiw_player_stationary_last_tick = 0;
static uintptr_t g_aiw_player_stationary_ptr = 0;
static int   g_aiw_player_stationary_confirmed = 0;
static int   g_aiw_player_game_paused = 0;
static int   g_aiw_player_on_road_geom = 0;
static int   g_aiw_player_surface_contacts = 0;
static int   g_aiw_player_off_track = 0;
static int g_impassable_cluster = 0;
static int   g_aiw_blocking_valid = 0;
static int   g_aiw_blocking_nearest_index = -1;
static float g_aiw_blocking_path_m = 0.0f;
static float g_aiw_blocking_line_m = 0.0f;
static int   g_aiw_blocking_band = 0;
static int   g_aiw_blocking_corridor = -1;
static float g_aiw_weather_on_wetness = 0.0f;
static float g_aiw_weather_off_wetness = 0.0f;
static float g_aiw_weather_mean_wetness = 0.0f;
static int   g_aiw_weather_class = 0;

/* --- shared-memory monitor bridge ---------------------------------------
 * Publishes the monitor state so an external viewer (the Python field GUI,
 * tools/field_gui.py) can read cooldown/candidate/counters without the
 * in-game window. 44 bytes, no locks: every field is one aligned int written
 * by the single worker thread. The mapping is named, so a second DLL
 * instance attaches to the same object harmlessly. */
#define MON_MAP_NAME "F1C_REDFLAG_MON"
typedef struct {
    volatile int   game_state;
    volatile int   race_phase;
    volatile int   settled;
    volatile int   yellow_active;
    volatile DWORD yellow_ms;
    volatile int   terminals_window;
    volatile int   terminals_now;
    volatile int   stalled;
    volatile int   stopped;
    volatile int   crashed;
    volatile DWORD cooldown_ms;
    volatile int   candidate;
    volatile int   phase;       /* red-flag sequence phase (0 = idle)      */
    volatile int   held;        /* wrecks held on track by the anti-removal */
    volatile int   incident_score;
    volatile int   incident_cars;
    volatile int   incident_terminals;
    volatile int   paused;
    volatile int   multiplayer;
    volatile int   opening_window;
    volatile DWORD yellow_required_ms;
    volatile DWORD candidate_confirm_ms;
    volatile int   auto_enabled;
    volatile int   incident_damaged_stationary;
    volatile int   stop_reason;
    volatile DWORD prestop_elapsed_ms;
    volatile int   blocking_road_contacts;
} RFMonShared;
static HANDLE       g_mon_map = NULL;
static RFMonShared* g_mon_shared = NULL;

/* F1 Live HUD compatibility ABI v1.
 *
 * F1CUILive v0.6.0.1 rebuilds its complete driver table every refresh, but
 * bounds that fixed table with the game's VehicleCount. Red Flag must reduce
 * VehicleCount while F1C constructs a survivor-only restart grid, so an
 * unadapted HUD can ingest a truncated/misaligned field during that window.
 * The exact-hash HUD adapter reads original_entrant_count from this named,
 * read-only-to-the-consumer mapping and falls back to vanilla VehicleCount
 * whenever the mapping is absent, inactive or invalid. No game address or
 * game behavior is changed for presentation. */
#define LIVEHUD_MAP_NAME    "F1C_REDFLAG_LIVEHUD_V1"
#define LIVEHUD_MAP_MAGIC   0x31484652u /* "RFH1" little-endian */
#define LIVEHUD_MAP_VERSION 1u
#define LIVEHUD_STATE_ACTIVE          0x00000001u
#define LIVEHUD_STATE_SESSION         0x00000002u
#define LIVEHUD_STATE_ROSTER_REDUCED  0x00000004u
#define LIVEHUD_STATE_RESTART         0x00000008u
typedef struct {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile LONG original_entrant_count;
    volatile LONG native_vehicle_count;
    volatile uint32_t state_flags;
    volatile uint32_t session_serial;
    volatile LONG sequence;
} RfLiveHudShared;
static HANDLE           g_livehud_map = NULL;
static RfLiveHudShared* g_livehud_shared = NULL;

/* Separate versioned mapping keeps the established 27-field red-flag bridge
 * backward compatible with older GUIs during a cooperative hot-swap. */
#define AIW_MON_MAP_NAME "F1C_REDFLAG_AIW_V4"
#define AIW_MON_MAGIC    0x34574941u /* "AIW4" little-endian */
typedef struct {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile LONG sequence;
    volatile int valid;
    volatile int main_waypoints;
    volatile int player_found;
    volatile int player_nearest_index;
    volatile int player_segment_index;
    volatile int player_band;
    volatile float player_node_m;
    volatile float player_path_m;
    volatile int player_surface_contacts;
    volatile int player_off_track;
    volatile int blocking_valid;
    volatile int blocking_nearest_index;
    volatile float blocking_path_m;
    volatile float player_dry_line_m;
    volatile float player_wet_line_m;
    volatile float player_closest_line_m;
    volatile float player_road_left_m;
    volatile float player_road_right_m;
    volatile float player_road_width_m;
    volatile float player_lateral_m;
    volatile float player_free_left_m;
    volatile float player_free_right_m;
    volatile float player_best_pass_m;
    volatile float player_critical_radius_m;
    volatile float player_danger_radius_m;
    volatile DWORD player_stationary_ms;
    volatile int player_stationary_confirmed;
    volatile int player_game_paused;
    volatile int player_on_road_geom;
    volatile float player_speed;
    volatile float blocking_line_m;
    volatile int blocking_band;
    volatile int player_corridor;       /* 0 main track, 1 active pit lane */
    volatile int player_main_band;
    volatile int player_pit_band;
    volatile int pit_waypoints;
    volatile float player_main_line_m;
    volatile float player_pit_line_m;
    volatile int blocking_corridor;
    volatile float weather_on_wetness;
    volatile float weather_off_wetness;
    volatile float weather_mean_wetness;
    volatile int weather_class;          /* 0 dry, 1 inter, 2 wet, 3 monsoon */
} AIWMonShared;
static HANDLE        g_aiw_mon_map = NULL;
static AIWMonShared* g_aiw_mon_shared = NULL;

static int g_late_count;   /* forward (tentative) decl; defined with the
                              late-sweep bookkeeping below */
static BOOL  g_monitor_jos_found = FALSE;
static int   g_monitor_jos_active = -1;
static int   g_monitor_jos_retired = -1;
static int   g_monitor_jos_pitstate = -1;
static int   g_monitor_jos_damage = 0;
static float g_monitor_jos_rpm = 0.0f;
static float g_monitor_jos_engine_life = 0.0f;
static float g_monitor_jos_gearbox_life = 0.0f;
static float g_monitor_jos_suspension_life = 0.0f;

static void status_gui_create(void);
static void status_gui_pump(void);
static void status_gui_update(int phase);
static BOOL race_is_settled(void);
static BOOL is_multiplayer(void);
static BOOL field_size_supported(void);
static BOOL session_arrays_allocate(int count);
static void session_arrays_release(void);
static BOOL veh_crash_terminal(char* veh);
static BOOL veh_damage_evidence(char* veh);
static BOOL veh_effectively_in_pits(char* veh);
static int  count_stalled(void);
static int  count_stopped(void);
static int  count_crashed(void);
static DWORD kill_duration(char* veh);
static void refresh_aiw_player_metric(void);
static BOOL vehicle_looks_live(char* veh);
static void refresh_yellow_state(DWORD now);
static void refresh_terminal_state(DWORD now);
static int weather_incident_assist_level(void);
static uintptr_t session_epoch(void);
static void restore_fuel(void);
static BOOL restore_pit_strategy(void);
static void replay_roster_guard_poll(int phase);
static void replay_roster_guard_reset(void);
static void restore_driver_visibility(void);
static void log_w(const char* s);
static void auto_notice_reset(void);

/* Slot index of a vehicle pointer in VEHICLE_ARRAY, or -1. Logging only. */
static int veh_slot(char* veh) {
    int count = VEHICLE_COUNT;
    for (int i = 0; i < count; i++)
        if ((char*)VEHICLE_ARRAY[i] == veh) return i;
    return -1;
}

static const char* veh_name(char* veh) {
    return veh ? (const char*)(veh + OFF_DRIVERNAME) : "?";
}

/* PitState survives some red-flag restarts after the car has returned to the
 * circuit. A car travelling faster than 30 m/s (~108 km/h, well above the
 * pit-limiter regime) is not under credible pit-lane ownership even if the raw
 * enum still says 2+. Treat that combination as stale everywhere that decides
 * status or incident eligibility. The short post-restart normalizer below also
 * returns the raw game field to zero once the live race is established. */
static BOOL veh_effectively_in_pits(char* veh) {
    if (!veh) return FALSE;
    if (*(volatile int*)(veh + OFF_PITSTATE) < 2) return FALSE;
    return *(volatile float*)(veh + OFF_SPEED) <= PIT_STATE_CREDIBLE_MAX_SPEED;
}

/* The restart rebuild can copy a pre-restart PitState into a car which is
 * already travelling at racing speed. Besides presenting the wrong status,
 * that stale enum excludes a real crash from the automatic incident model.
 * Correct only this impossible combination, only for ten seconds after the
 * restarted race becomes live; genuine slow cars in the lane are untouched. */
static void normalize_post_restart_pit_states(DWORD now) {
    if (!g_post_restart_normalize_until || now > g_post_restart_normalize_until)
        return;
    uintptr_t epoch0 = session_epoch();
    uintptr_t player = PLAYER_VEH_PTR;
    int count = VEHICLE_COUNT;
    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh || (uintptr_t)veh == player || !vehicle_looks_live(veh)) continue;
        if (*(volatile int*)(veh + OFF_ACTIVEFLAG) != 1) continue;
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
        int pit = *(volatile int*)(veh + OFF_PITSTATE);
        float speed = *(volatile float*)(veh + OFF_SPEED);
        if (pit < 2 || speed <= PIT_STATE_CREDIBLE_MAX_SPEED) continue;
        if (session_epoch() != epoch0) return;
        *(volatile int*)(veh + OFF_PITSTATE) = 0;
        if (!g_post_restart_pit_fixed[i]) {
            char buf[192];
            wsprintfA(buf, "RF-E72F9F20762 %d %d %s %d",
                      pit, i, veh_name(veh), (int)speed);
            log_w(buf);
            g_post_restart_pit_fixed[i] = TRUE;
        }
    }
}

/* --- Auto red-flag detection: native-yellow-gated terminal cluster --------
 *
 * The native yellow predicate is sub_49BAD0(). F1C itself calls it from the
 * HUD flag renderer and flag-audio path; it returns true where native race
 * control has marked a track location yellow.
 *
 * An automatic red flag needs all of these at once:
 *   1. native yellow has remained active for AUTO_YELLOW_CONFIRM_MS — FULL
 *      COURSE: any zone in the steward table reading yellow, NOT just the
 *      player's own zone (a crash away from the player could never arm the
 *      old per-player gate, and a red flag is a full-course condition);
 *   2. three non-player cars, within one spatial cluster (AUTO_CLUSTER_M),
 *      are terminal for AUTO_TERMINAL_CONFIRM_MS; and
 *   3. the race is SETTLED (GameState 5/7 && RacePhase 3/5, evidence-verified;
 *      see race_is_settled()) and the normal race-session and cooldown gates
 *      are open.
 *
 * The exact per-AI native-sector lookup has not been verified yet. Until it
 * is, the full-course yellow gate and the spatial terminal cluster are the
 * deliberately conservative incident-locality proxies: the yellow says race
 * control has acknowledged an incident somewhere on the circuit, and the
 * cluster says three cars went down together. */
/* AUTO-DETECT IS DISABLED (gr34, 2026-07-24).
 *
 * It had never actually fired before gr33 — veh_killed() misread ActiveFlag as a
 * float, so detect_seed() seeded every car as already-killed and there was never
 * a false->true edge. gr33 fixed the read, the feature came alive for the first
 * time, and it immediately mis-fired: a grid RESTART transiently drops several
 * cars to ActiveFlag=0, which looks exactly like a 3-car pileup inside the 8s
 * window. Result was a red flag auto-triggering right after the previous one
 * finished, leaving AI stuck on the grid (reported in-game 2026-07-24).
 *
 * The manual triggers do not depend on this, so it is switched off rather than
 * left to corrupt races.
 *
 * RE-ENABLE WORK (2026-08-17) - every fix the mis-fire required is now in the
 * source; only the guarded full-race incident test and Matias's validation
 * remain before the default flips:
 *   - GATE: race_is_settled() accepts the racing value sets observed live
 *     (GameState 5/7, RacePhase 3/5 - the 2026-08-17 live yellow-flag race
 *     reads 5/5) and excludes RacePhase 2 (GRID_READY) and 8 (DONE). The
 *     restart/formation/grid phases the gr34 mis-fire came from cannot arm
 *     the detector at all, and on the edge into a settled race every stale
 *     kill/terminal sample is dropped so a car that died outside the gate
 *     cannot count as a fresh pileup.
 *   - SUSTAINED KILL: veh_crash_terminal + kill_duration (4 s) - already in.
 *   - MANUAL COOLDOWN: NUMPAD1/NUMPAD2 arm g_auto_cooldown_until - already in.
 *   - NATIVE-YELLOW GATE: 3 s persisted yellow, read as pure memory reads
 *     (native_yellow_read, calibrated 2026-08-17) - sub_49BAD0 itself is
 *     NOT called from this DLL's thread: it hangs the worker (see
 *     native_yellow_read). 2026-08-17 fix: FULL COURSE - any steward zone
 *     reading yellow, not just the player's own zone.
 *   - LOCALITY: the sustained terminals must be one incident - a terminal
 *     car counts only if AUTO_TERM_THRESHOLD cars sit within AUTO_CLUSTER_M
 *     of each other; scattered retirements 2 km apart are not a pileup.
 *   - PAUSE CONSISTENCY: yellow persistence, the terminal sustain clock and
 *     the 5 s window are all frozen while the game is paused - red-flag
 *     timing is race time, not wall time.
 *
 * The canonical source remains manual-only by default. A guarded validation
 * build may opt in with -DAUTO_REDFLAG_ENABLED=1 without changing this
 * source default. Set to 1 only for guarded full-race validation.
 * (Settled-race gate verified against docs/49-formation-lap.md 4.3/4.4:
 *  GameState 4=restart-in-progress, 5=formation/grid, 7=race running;
 *  RacePhase 2=GRID_READY, 3=GREEN_FLAG, 5=FINISHING, 8=DONE.) */
/* The canonical source remains manual-only. A guarded validation build may
 * opt in with -DAUTO_REDFLAG_ENABLED=1 without changing this source default. */
#ifndef AUTO_REDFLAG_ENABLED
#define AUTO_REDFLAG_ENABLED 0
#endif

/* Diagnostic-only replay isolation. A value of 1 deliberately keeps the
 * survivor-only roster after green so native replay/scene objects never see a
 * post-construction VehicleCount expansion. This sacrifices complete results
 * and must never be used for distribution. Canonical builds keep it at 0. */
#ifndef RF_DIAGNOSTIC_KEEP_SURVIVOR_ROSTER
#define RF_DIAGNOSTIC_KEEP_SURVIVOR_ROSTER 0
#endif
#ifndef RF_DIAGNOSTIC_OBSERVE_NATIVE_PLAYER_PLACEMENT
#define RF_DIAGNOSTIC_OBSERVE_NATIVE_PLAYER_PLACEMENT 0
#endif
#ifndef RF_DIAGNOSTIC_PLAYER_GRID_GATE
#define RF_DIAGNOSTIC_PLAYER_GRID_GATE 0
#endif
#ifndef RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE
#define RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE RF_DIAGNOSTIC_PLAYER_GRID_GATE
#endif
#ifndef RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE
#define RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE 0
#endif
#ifndef RF_DIAGNOSTIC_PLAYER_GARAGE_RETIRE_LOCK
#define RF_DIAGNOSTIC_PLAYER_GARAGE_RETIRE_LOCK 0
#endif

#if RF_DIAGNOSTIC_PLAYER_GRID_GATE
__attribute__((used))
volatile const char RF_PLAYER_GRID_GATE_SIGNATURE[] =
    "RF_DIAGNOSTIC_PLAYER_GRID_GATE=1";
#endif
#if RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE
__attribute__((used))
volatile const char RF_PLAYER_TUPLE_RESTORE_SIGNATURE[] =
    "RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE=1";
#endif
#if RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE
__attribute__((used))
volatile const char RF_PLAYER_NATIVE_GARAGE_SIGNATURE[] =
    "RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE=1";
#endif
#if RF_DIAGNOSTIC_PLAYER_GARAGE_RETIRE_LOCK
__attribute__((used))
volatile const char RF_PLAYER_GARAGE_RETIRE_LOCK_SIGNATURE[] =
    "RF_DIAGNOSTIC_PLAYER_GARAGE_RETIRE_LOCK=1";
#endif

#if AUTO_REDFLAG_ENABLED
__attribute__((used))
volatile const char RF_AUTOMODE_SIGNATURE[] = "AUTO_REDFLAG_ENABLED=1";
#endif
__attribute__((used))
volatile const char RF_MANUAL_ACCEPT_SIGNATURE[] =
    "RF_MANUAL_ACCEPT_HOTKEY=T";

#define AUTO_TERM_THRESHOLD 3
#define AUTO_TERM_WINDOW_MS 10000
#define AUTO_COOLDOWN_MS    120000   /* after a fire, don't re-arm for 2 min */
#define AUTO_YELLOW_CONFIRM_MS   3000
#define AUTO_TERMINAL_CONFIRM_MS 4000
#define AUTO_STOP_CONFIRM_MS     3000
#define AUTO_DAMAGE_CONFIRM_MS   1000
#define AUTO_FINAL_CONFIRM_MS    1000
#define AUTO_PRESTOP_NOTICE_MS  10000
#define AUTO_RESTART_COUNTDOWN_MS 15000
#define AUTO_OPENING_WINDOW_MS  30000
#define AUTO_OPENING_YELLOW_MS   1000
#define AUTO_OPENING_CONFIRM_MS  2000
#define AUTO_CLUSTER_M          300.0f
#define AUTO_OPENING_CLUSTER_M  200.0f
#define AUTO_BLOCKING_CONFIRM_MS 3000
#define AUTO_WEATHER_ASSIST_WETNESS 0.90f
#define AUTO_WEATHER_STRONG_WETNESS 0.96f
#define RF_LINE_BLOCK_MAX_SPEED_MPS 1.0f
#define AIW_LINE_OVERLAP_MARGIN_M 0.5f
#define RF_Y_RESTART_FULL_RACE       0
#define RF_Y_RESTART_POST_RED_FLAG   1
#define RF_RESTART_PROCEDURE_COUNTDOWN     0
#define RF_RESTART_PROCEDURE_DRIVE_TO_PITS 1

/* Player-editable automatic-activation policy. The file is always named
 * redflag.ini and is read from beside this DLL once during LightLoader
 * initialization. Missing, malformed and out-of-range values fall back to or
 * clamp against the reviewed defaults below. Runtime/layout, offline-only,
 * settled-race, pause and restart-sequence gates remain non-configurable. */
typedef struct {
    int automatic_enabled;
    int manual_enabled;
    int activation_key, allow_cancel, sc_fault_enabled, weather_enabled;
    DWORD weather_confirm_ms,weather_resume_ms,sc_handoff_timeout_ms;
    float weather_resume_max;
    int weather_override;
    DWORD acceptance_timeout_ms;
    int require_native_yellow;
    int require_manual_acceptance;
    int minimum_race_distance_percent;
    float vehicle_removal_seconds,debris_removal_seconds,parts_duration_seconds;
    int y_restart_mode;
    int restart_procedure;
    DWORD cooldown_ms;
    DWORD restart_countdown_ms;
    DWORD manual_activation_countdown_ms;
    DWORD opening_window_ms;
    DWORD incident_window_ms;
    DWORD yellow_ms;
    DWORD opening_yellow_ms;
    DWORD wet_yellow_ms;
    DWORD terminal_confirm_ms;
    DWORD stationary_confirm_ms;
    DWORD damage_confirm_ms;
    DWORD opening_incident_confirm_ms;
    DWORD blocking_confirm_ms;
    DWORD inverted_confirm_ms;
    DWORD final_confirm_ms;
    DWORD wet_final_confirm_ms;
    DWORD strong_wet_final_confirm_ms;
    DWORD prestop_notice_ms;
    float cluster_m;
    float opening_cluster_m;
    float ontrack_pileup_cluster_m;
    float stationary_speed_mps;
    float slow_damage_speed_mps;
    float racing_line_stationary_speed_mps;
    float terminal_racing_line_jitter_speed_mps;
    float racing_line_overlap_margin_m;
    float inverted_up_dot_max;
    float wet_assist_threshold;
    float strong_wet_threshold;
    int enable_blocked_corridor;
    int enable_inverted_blocker;
    int enable_multi_terminal;
    int multi_terminal_cars;
    int enable_mixed_blockage;
    int mixed_terminal_cars;
    int mixed_stationary_damaged_cars;
    int enable_strong_wet_pattern;
    int strong_wet_terminal_cars;
    int strong_wet_stationary_damaged_cars;
    int minimum_cluster_cars;
    int minimum_cluster_terminals;
    int log_passive_placeholder;
    int split_replay_on_restart;
} RFActivationConfig;

static RFActivationConfig g_rf_config;
static void hud_auto_notice_write(int line,const char* detail);
static BOOL veh_heavy_damage_evidence(char* veh);
static BOOL veh_killed(char* veh);
static char g_rf_config_path[MAX_PATH];
static BOOL g_race_distance_gate_logged = FALSE;

/* GetPrivateProfileStringA may return an inline comment as part of the value.
 * Strip it before strict numeric parsing so player-facing lines can safely use
 * "Key=value ; explanation" without silently falling back to defaults. */
static void rf_ini_strip_inline_comment(char* raw) {
    char* comment;
    char* end;
    if (!raw) return;
    comment = strpbrk(raw, ";#");
    if (comment) *comment = '\0';
    end = raw + strlen(raw);
    while (end > raw && (end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
}

static int rf_ini_int(const char* section, const char* key, int fallback,
                      int minimum, int maximum) {
    char raw[64], *end;
    long value;
    if (!GetPrivateProfileStringA(section, key, "", raw, sizeof(raw),
                                  g_rf_config_path)) return fallback;
    rf_ini_strip_inline_comment(raw);
    value = strtol(raw, &end, 10);
    while (*end == ' ' || *end == '\t') end++;
    if (end == raw || *end != '\0') return fallback;
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    return (int)value;
}

static float rf_ini_float(const char* section, const char* key, float fallback,
                          float minimum, float maximum) {
    char raw[64], *end;
    double value;
    if (!GetPrivateProfileStringA(section, key, "", raw, sizeof(raw),
                                  g_rf_config_path)) return fallback;
    rf_ini_strip_inline_comment(raw);
    value = strtod(raw, &end);
    while (*end == ' ' || *end == '\t') end++;
    if (end == raw || *end != '\0' || !isfinite(value)) return fallback;
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    return (float)value;
}

static int rf_ini_y_restart_mode(void) {
    char raw[64];
    if (!GetPrivateProfileStringA("Restart", "YRestartMode", "", raw,
                                  sizeof(raw), g_rf_config_path))
        return RF_Y_RESTART_POST_RED_FLAG;
    rf_ini_strip_inline_comment(raw);
    if (_stricmp(raw, "FullRace") == 0)
        return RF_Y_RESTART_FULL_RACE;
    if (_stricmp(raw, "PostRedFlag") == 0)
        return RF_Y_RESTART_POST_RED_FLAG;
    return RF_Y_RESTART_POST_RED_FLAG;
}

static int rf_ini_restart_procedure(void) {
    char raw[64];
    if (!GetPrivateProfileStringA("Restart", "RestartProcedure", "", raw,
                                  sizeof(raw), g_rf_config_path))
        return RF_RESTART_PROCEDURE_COUNTDOWN;
    rf_ini_strip_inline_comment(raw);
    if (_stricmp(raw, "DriveToPits") == 0)
        return RF_RESTART_PROCEDURE_DRIVE_TO_PITS;
    if (_stricmp(raw, "Countdown") == 0)
        return RF_RESTART_PROCEDURE_COUNTDOWN;
    return RF_RESTART_PROCEDURE_COUNTDOWN;
}

/* Same letter / decimal VK syntax as Safety Car. RF owns its binding; reject
 * Y and the effective SC controls rather than sharing a destructive key. */
static int rf_read_key(const char* section, const char* key, int fallback,
                       const char* path) {
    char raw[64], *end, *q; long value;
    if (!GetPrivateProfileStringA(section,key,"",raw,sizeof(raw),path)) return fallback;
    rf_ini_strip_inline_comment(raw); q=raw;
    while (*q==' ' || *q=='\t') ++q;
    if (q[0] && !q[1] && ((q[0]>='A'&&q[0]<='Z') || (q[0]>='a'&&q[0]<='z')))
        return q[0]>='a' ? q[0]-'a'+'A' : q[0];
    value=strtol(q,&end,10);
    if (end==q || *end || value<48 || value>255 || (value>=97&&value<=99)) return fallback;
    return (int)value;
}
static void rf_key_hint(int row,const char* suffix) {
    char label[32], text[96]; int key=g_rf_config.activation_key;
    if (key>='A'&&key<='Z') snprintf(label,sizeof(label),"%c",key);
    else snprintf(label,sizeof(label),"KEY %d",key);
    snprintf(text,sizeof(text),"PRESS %s %s",label,suffix);
    hud_auto_notice_write(row,text);
}
static void rf_load_activation_config(void) {
    char* slash;
    DWORD path_len;
    ZeroMemory(&g_rf_config, sizeof(g_rf_config));
    path_len = GetModuleFileNameA(g_hinst, g_rf_config_path,
                                  sizeof(g_rf_config_path));
    if (path_len == 0 || path_len >= sizeof(g_rf_config_path)) {
        g_rf_config_path[0] = '\0';
    } else {
        slash = strrchr(g_rf_config_path, '\\');
        if (!slash || (size_t)(slash - g_rf_config_path) + 13 >=
                      sizeof(g_rf_config_path)) {
            g_rf_config_path[0] = '\0';
        } else {
            strcpy(slash + 1, "redflag.ini");
        }
    }

    g_rf_config.vehicle_removal_seconds=(float)rf_ini_int("Removal","VehicleRemovalSeconds",60,1,600);
    g_rf_config.debris_removal_seconds=(float)rf_ini_int("Removal","DebrisRemovalSeconds",60,1,600);
    g_rf_config.parts_duration_seconds=(float)rf_ini_int("Removal","PartsDurationSeconds",60,1,600);
    { char removal_log[160]; snprintf(removal_log, sizeof(removal_log), "REMOVAL CONFIG: vehicle=%.0fs debris=%.0fs parts=%.0fs", (double)g_rf_config.vehicle_removal_seconds, (double)g_rf_config.debris_removal_seconds, (double)g_rf_config.parts_duration_seconds); log_w(removal_log); }
    g_rf_config.automatic_enabled = rf_ini_int(
        "Activation", "AutomaticEnabled", 1, 0, 1);
    g_rf_config.require_native_yellow = rf_ini_int(
        "Activation", "RequireNativeYellow", 1, 0, 1);
    g_rf_config.require_manual_acceptance = rf_ini_int(
        "Activation", "RequireManualRedFlagAcceptance", 0, 0, 1);
    g_rf_config.manual_enabled = rf_ini_int(
        "Activation", "ManualEnabled", 0, 0, 1);
    g_rf_config.weather_enabled=rf_ini_int("Activation","PersistentWeatherRedFlag",1,0,1);
    g_rf_config.weather_confirm_ms=(DWORD)rf_ini_int("Timing","PersistentWeatherSeconds",60,15,600)*1000u;
    g_rf_config.weather_resume_ms=(DWORD)rf_ini_int("Timing","WeatherResumeSeconds",30,5,600)*1000u;
    g_rf_config.weather_resume_max=rf_ini_float("Restart","WeatherResumeWetness",0.90f,0.0f,0.95f);
    g_rf_config.weather_override=rf_ini_int("Restart","AllowWeatherRestartOverride",1,0,1);
    g_rf_config.sc_handoff_timeout_ms=(DWORD)rf_ini_int("Timing","SafetyCarHandoffTimeoutSeconds",60,15,600)*1000u;
    g_rf_config.allow_cancel=rf_ini_int("Activation","AllowCancellation",0,0,1);
    g_rf_config.sc_fault_enabled=rf_ini_int("Activation","SafetyCarFaultRedFlag",1,0,1);
    g_rf_config.acceptance_timeout_ms=(DWORD)rf_ini_int("Timing","AcceptanceTimeoutSeconds",30,5,120)*1000u;
    {
        char sc_path[MAX_PATH], *tail, msg[160];
        int deploy=85, emergency=73;
        strcpy(sc_path,g_rf_config_path); tail=strrchr(sc_path,'\\');
        if(tail) { strcpy(tail+1,"safetycar.ini");
            deploy=rf_read_key("sc","DeployKey",85,sc_path);
            emergency=rf_read_key("sc","EmergencyKey",73,sc_path); }
        /* Mirror the existing SC reserved/duplicate fallback. */
        if(deploy==emergency||deploy==84||emergency==84||deploy==89||emergency==89){deploy=85;emergency=73;}
        g_rf_config.activation_key=rf_read_key("Activation","RedFlagKey",84,g_rf_config_path);
        if(g_rf_config.activation_key==89 || g_rf_config.activation_key==deploy ||
           g_rf_config.activation_key==emergency) {
            g_rf_config.activation_key=84;
            log_w("RF-E0C6A82DAD9");
        }
        snprintf(msg,sizeof(msg),"RF CONTROLS: activation VK=%d cancel=%d SC-fault=%d",
            g_rf_config.activation_key,g_rf_config.allow_cancel,g_rf_config.sc_fault_enabled);log_w(msg);
    }
    g_rf_config.minimum_race_distance_percent = rf_ini_int(
        "Activation", "MinimumRaceDistancePercent", 25, 0, 100);
    g_rf_config.y_restart_mode = rf_ini_y_restart_mode();
    g_rf_config.restart_procedure = rf_ini_restart_procedure();
    g_rf_config.cooldown_ms = (DWORD)rf_ini_int(
        "Timing", "CooldownSeconds", AUTO_COOLDOWN_MS / 1000, 0, 3600) * 1000u;
    g_rf_config.restart_countdown_ms = (DWORD)rf_ini_int(
        "Timing", "RestartCountdownSeconds",
        AUTO_RESTART_COUNTDOWN_MS / 1000, 5, 120) * 1000u;
    g_rf_config.manual_activation_countdown_ms = (DWORD)rf_ini_int(
        "Timing", "ManualActivationCountdownSeconds", 10, 1, 120) * 1000u;
    g_rf_config.opening_window_ms = (DWORD)rf_ini_int(
        "Timing", "OpeningWindowSeconds", AUTO_OPENING_WINDOW_MS / 1000,
        0, 600) * 1000u;
    g_rf_config.incident_window_ms = (DWORD)rf_ini_int(
        "Timing", "IncidentFreshnessSeconds", AUTO_TERM_WINDOW_MS / 1000,
        1, 120) * 1000u;
    g_rf_config.yellow_ms = (DWORD)rf_ini_int(
        "Timing", "YellowConfirmMs", AUTO_YELLOW_CONFIRM_MS, 0, 60000);
    g_rf_config.opening_yellow_ms = (DWORD)rf_ini_int(
        "Timing", "OpeningYellowConfirmMs", AUTO_OPENING_YELLOW_MS, 0, 60000);
    g_rf_config.wet_yellow_ms = (DWORD)rf_ini_int(
        "Timing", "WetYellowConfirmMs", 2000, 0, 60000);
    g_rf_config.terminal_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "TerminalConfirmMs", AUTO_TERMINAL_CONFIRM_MS, 0, 60000);
    g_rf_config.stationary_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "StationaryConfirmMs", AUTO_STOP_CONFIRM_MS, 0, 60000);
    g_rf_config.damage_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "SlowDamageConfirmMs", AUTO_DAMAGE_CONFIRM_MS, 0, 60000);
    g_rf_config.opening_incident_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "OpeningIncidentConfirmMs", AUTO_OPENING_CONFIRM_MS,
        0, 60000);
    g_rf_config.blocking_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "BlockedCorridorConfirmMs", AUTO_BLOCKING_CONFIRM_MS,
        0, 60000);
    g_rf_config.inverted_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "InvertedCarConfirmMs", 2500, 0, 60000);
    g_rf_config.final_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "FinalConfirmMs", AUTO_FINAL_CONFIRM_MS, 0, 60000);
    g_rf_config.wet_final_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "WetFinalConfirmMs", 750, 0, 60000);
    g_rf_config.strong_wet_final_confirm_ms = (DWORD)rf_ini_int(
        "Timing", "StrongWetFinalConfirmMs", 500, 0, 60000);
    g_rf_config.prestop_notice_ms = (DWORD)rf_ini_int(
        "Timing", "PreStopNoticeSeconds", AUTO_PRESTOP_NOTICE_MS / 1000,
        0, 120) * 1000u;

    g_rf_config.cluster_m = rf_ini_float(
        "Locality", "ClusterRadiusMeters", AUTO_CLUSTER_M, 5.0f, 5000.0f);
    g_rf_config.opening_cluster_m = rf_ini_float(
        "Locality", "OpeningClusterRadiusMeters", AUTO_OPENING_CLUSTER_M,
        5.0f, 5000.0f);
    g_rf_config.ontrack_pileup_cluster_m = rf_ini_float(
        "Locality", "OnTrackPileupRadiusMeters", 40.0f, 5.0f, 100.0f);
    g_rf_config.stationary_speed_mps = rf_ini_float(
        "Classification", "StationarySpeedMps", 0.5f, 0.0f, 20.0f);
    g_rf_config.slow_damage_speed_mps = rf_ini_float(
        "Classification", "SlowDamagedSpeedMps", 8.0f, 0.1f, 100.0f);
    if (g_rf_config.slow_damage_speed_mps < g_rf_config.stationary_speed_mps)
        g_rf_config.slow_damage_speed_mps = g_rf_config.stationary_speed_mps;
    g_rf_config.racing_line_stationary_speed_mps = rf_ini_float(
        "Classification", "RacingLineStationarySpeedMps", 1.0f,
        0.1f, RF_LINE_BLOCK_MAX_SPEED_MPS);
    g_rf_config.terminal_racing_line_jitter_speed_mps = rf_ini_float(
        "Classification", "TerminalRacingLineJitterSpeedMps", 3.0f,
        RF_LINE_BLOCK_MAX_SPEED_MPS, 5.0f);
    g_rf_config.racing_line_overlap_margin_m = rf_ini_float(
        "Classification", "RacingLineOverlapMarginMeters",
        AIW_LINE_OVERLAP_MARGIN_M, 0.0f, 0.75f);
    g_rf_config.inverted_up_dot_max = rf_ini_float(
        "Classification", "InvertedUpDotMax", -0.25f, -0.95f, 0.0f);

    g_rf_config.wet_assist_threshold = rf_ini_float(
        "Weather", "WetAssistThreshold", AUTO_WEATHER_ASSIST_WETNESS,
        0.0f, 1.0f);
    g_rf_config.strong_wet_threshold = rf_ini_float(
        "Weather", "StrongWetThreshold", AUTO_WEATHER_STRONG_WETNESS,
        0.0f, 1.0f);
    if (g_rf_config.strong_wet_threshold < g_rf_config.wet_assist_threshold)
        g_rf_config.strong_wet_threshold = g_rf_config.wet_assist_threshold;

    g_rf_config.enable_blocked_corridor = rf_ini_int(
        "Patterns", "EnableBlockedCorridor", 1, 0, 1);
    g_rf_config.enable_inverted_blocker = rf_ini_int(
        "Patterns", "EnableInvertedRacingLineBlocker", 1, 0, 1);
    g_rf_config.enable_multi_terminal = rf_ini_int(
        "Patterns", "EnableMultiTerminal", 1, 0, 1);
    g_rf_config.multi_terminal_cars = rf_ini_int(
        "Patterns", "MultiTerminalCars", 2, 1, INT_MAX);
    g_rf_config.enable_mixed_blockage = rf_ini_int(
        "Patterns", "EnableMixedBlockage", 1, 0, 1);
    g_rf_config.mixed_terminal_cars = rf_ini_int(
        "Patterns", "MixedTerminalCars", 1, 1, INT_MAX);
    g_rf_config.mixed_stationary_damaged_cars = rf_ini_int(
        "Patterns", "MixedStationaryDamagedCars", 2, 1, INT_MAX);
    g_rf_config.enable_strong_wet_pattern = rf_ini_int(
        "Patterns", "EnableStrongWetPattern", 1, 0, 1);
    g_rf_config.strong_wet_terminal_cars = rf_ini_int(
        "Patterns", "StrongWetTerminalCars", 1, 1, INT_MAX);
    g_rf_config.strong_wet_stationary_damaged_cars = rf_ini_int(
        "Patterns", "StrongWetStationaryDamagedCars", 1, 1, INT_MAX);
    g_rf_config.minimum_cluster_cars = rf_ini_int(
        "Patterns", "MinimumClusterCars", 2, 1, INT_MAX);
    g_rf_config.minimum_cluster_terminals = rf_ini_int(
        "Patterns", "MinimumClusterTerminals", 1, 1, INT_MAX);
    g_rf_config.log_passive_placeholder = rf_ini_int(
        "Placeholder", "LogNonRedFlagIncidents", 1, 0, 1);
    g_rf_config.split_replay_on_restart = rf_ini_int(
        "Replay", "SplitOnRedFlagRestart", 1, 0, 1);

    /* Relational safety: preserve customization while preventing combinations
     * that silently disable every pattern or turn confirmation into a
     * single-frame transient. These floors are deliberately conservative. */
    if (g_rf_config.automatic_enabled) {
        if (g_rf_config.cooldown_ms < 10000u) g_rf_config.cooldown_ms = 10000u;
        if (g_rf_config.yellow_ms < 500u) g_rf_config.yellow_ms = 500u;
        if (g_rf_config.opening_yellow_ms < 500u)
            g_rf_config.opening_yellow_ms = 500u;
        if (g_rf_config.wet_yellow_ms < 500u) g_rf_config.wet_yellow_ms = 500u;
        if (g_rf_config.terminal_confirm_ms < 500u)
            g_rf_config.terminal_confirm_ms = 500u;
        if (g_rf_config.stationary_confirm_ms < 500u)
            g_rf_config.stationary_confirm_ms = 500u;
        if (g_rf_config.damage_confirm_ms < 250u)
            g_rf_config.damage_confirm_ms = 250u;
        if (g_rf_config.opening_incident_confirm_ms < 500u)
            g_rf_config.opening_incident_confirm_ms = 500u;
        if (g_rf_config.blocking_confirm_ms < 500u)
            g_rf_config.blocking_confirm_ms = 500u;
        if (g_rf_config.inverted_confirm_ms < 1000u)
            g_rf_config.inverted_confirm_ms = 1000u;
        if (g_rf_config.final_confirm_ms < 500u)
            g_rf_config.final_confirm_ms = 500u;
        if (g_rf_config.wet_final_confirm_ms < 500u)
            g_rf_config.wet_final_confirm_ms = 500u;
        if (g_rf_config.strong_wet_final_confirm_ms < 500u)
            g_rf_config.strong_wet_final_confirm_ms = 500u;
        if (g_rf_config.prestop_notice_ms < 1000u)
            g_rf_config.prestop_notice_ms = 1000u;
        if (g_rf_config.incident_window_ms <
            g_rf_config.terminal_confirm_ms + 1000u)
            g_rf_config.incident_window_ms =
                g_rf_config.terminal_confirm_ms + 1000u;
    }
    if (g_rf_config.minimum_cluster_terminals >
        g_rf_config.minimum_cluster_cars)
        g_rf_config.minimum_cluster_terminals =
            g_rf_config.minimum_cluster_cars;
    if (g_rf_config.multi_terminal_cars < g_rf_config.minimum_cluster_cars)
        g_rf_config.multi_terminal_cars = g_rf_config.minimum_cluster_cars;
    if (g_rf_config.multi_terminal_cars <
        g_rf_config.minimum_cluster_terminals)
        g_rf_config.multi_terminal_cars =
            g_rf_config.minimum_cluster_terminals;
    if (g_rf_config.mixed_terminal_cars <
        g_rf_config.minimum_cluster_terminals)
        g_rf_config.mixed_terminal_cars =
            g_rf_config.minimum_cluster_terminals;
    if (g_rf_config.mixed_terminal_cars < g_rf_config.minimum_cluster_cars &&
        g_rf_config.mixed_stationary_damaged_cars <
            g_rf_config.minimum_cluster_cars - g_rf_config.mixed_terminal_cars)
        g_rf_config.mixed_stationary_damaged_cars =
            g_rf_config.minimum_cluster_cars - g_rf_config.mixed_terminal_cars;
    if (g_rf_config.strong_wet_terminal_cars <
        g_rf_config.minimum_cluster_terminals)
        g_rf_config.strong_wet_terminal_cars =
            g_rf_config.minimum_cluster_terminals;
    if (g_rf_config.strong_wet_terminal_cars <
            g_rf_config.minimum_cluster_cars &&
        g_rf_config.strong_wet_stationary_damaged_cars <
            g_rf_config.minimum_cluster_cars -
            g_rf_config.strong_wet_terminal_cars)
        g_rf_config.strong_wet_stationary_damaged_cars =
            g_rf_config.minimum_cluster_cars -
            g_rf_config.strong_wet_terminal_cars;
}

/* F1C's own yellow-state predicate. Evidence:
 * functions/ShortComputation_bad0_0049bad0.c, used by
 * HUDFlagRendererMain_004d82d0.c and sub_49D4C0_0049d4c0.c.
 * NOTE: NOT CALLED by this DLL. Calling it from the worker thread hangs the
 * caller (2026-08-17, verified by a frozen monitor + unresponsive unload
 * event). Its behaviour is replicated as pure memory reads by
 * native_yellow_read(); the entry point is kept for reference only. */
typedef BOOL (__cdecl *NativeYellowFn)(void);
#define FN_NATIVE_YELLOW ((NativeYellowFn)0x0049BAD0)

static BOOL* g_last_killed = NULL;   /* per-slot: was ActiveFlag ~0 last poll */
static BOOL  g_det_seeded = FALSE;
static DWORD* g_term_times = NULL;
static DWORD* g_incident_since = NULL;
static DWORD* g_stationary_hazard_since = NULL;
static DWORD* g_inverted_hazard_since = NULL;
static int   g_term_times_n = 0;
static DWORD g_auto_cooldown_until = 0;
static DWORD g_native_yellow_since = 0;
static DWORD g_auto_candidate_since = 0;
static uintptr_t g_auto_candidate_signature = 0;
static DWORD g_auto_notice_elapsed_ms = 0;
static DWORD g_auto_notice_last_tick = 0;
static int   g_auto_notice_reason = 0;
static BOOL  g_auto_notice_fired = FALSE;
static int   g_auto_notice_sector = 0;
static int   g_auto_notice_cars = 0;
static int   g_auto_notice_terminals = 0;
static int   g_auto_notice_stationary_damaged = 0;
static int   g_auto_notice_blocking_contacts = 0;
static int   g_auto_notice_corridor = -1;
/* The automatic notice intentionally has no visible namespace prefix. Track
 * the exact three slots/texts we own so cleanup never erases a native/game or
 * another mod's HUD message merely because its wording happens to overlap. */
#define AUTO_NOTICE_HUD_LINES 3
static int g_auto_notice_hud_slots[AUTO_NOTICE_HUD_LINES] = {-1, -1, -1};
static char g_auto_notice_hud_text[AUTO_NOTICE_HUD_LINES][32] = {{0}};
#ifndef RF_RELEASE
static BOOL  g_debug_organic_notice = FALSE;
__attribute__((used))
static volatile const char RF_DEBUG_HOTKEY_SIGNATURE[] =
    "RF_DEBUG_HOTKEYS=NUMPAD1_ORGANIC";
#endif
static DWORD g_settled_since = 0;
static BOOL  g_prev_settled = FALSE;   /* last race_is_settled() verdict */
static BOOL  g_placeholder_logged = FALSE;

static LRESULT CALLBACK status_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    (void)w; (void)l;
    if (msg == WM_CLOSE) {
        ShowWindow(hwnd, SW_HIDE);
        g_status_visible = FALSE;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, w, l);
}

static void status_gui_create(void) {
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wsprintfA(g_status_class, "%s_%08X", STATUS_WINDOW_CLASS_PREFIX,
              (unsigned)(uintptr_t)g_hinst);
    wc.lpfnWndProc = status_wndproc;
    wc.hInstance = g_hinst;
    wc.lpszClassName = g_status_class;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);
    g_status_window = CreateWindowExA(WS_EX_TOPMOST, g_status_class,
        "F1C Red Flag Monitor", WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        24, 24, 300, 260, NULL, NULL, g_hinst, NULL);
    if (!g_status_window) return;
    g_status_text = CreateWindowExA(0, "STATIC", "Starting monitor...",
        WS_CHILD | WS_VISIBLE, 12, 12, 264, 220,
        g_status_window, NULL, g_hinst, NULL);
}

/* The worker thread owns the monitor window. It MUST be destroyed here,
 * before the module frees itself: a window left behind keeps the class
 * registered against a dangling window proc, and the next injected build
 * then creates its own monitor against a class that points into freed code.
 * This was the "GUI not visible after a reload" bug. */
static void destroy_status_gui(void) {
    if (g_status_window) {
        DestroyWindow(g_status_window);
        g_status_window = NULL;
        g_status_text = NULL;
    }
}

/* --- preserved legacy .PLR VehicleRemoval patch --------------------------
 *
 * This historical helper is retained as source evidence but is disabled for
 * every supported build. A monitoring DLL must not rewrite every player
 * profile at load, and clean-install validation must not depend on a modified
 * PLR. Countdown-end cleanup uses the bounded in-memory option override and
 * restores it before restart. */
#define LEGACY_PLR_REMOVAL_PATCH_ENABLED 0
#if LEGACY_PLR_REMOVAL_PATCH_ENABLED
static void plr_removal_60(void) {
    char base[MAX_PATH], dirpat[MAX_PATH], plrpat[MAX_PATH];
    if (!GetModuleFileNameA(NULL, base, MAX_PATH)) return;
    char* s = strrchr(base, '\\');
    if (!s) return;
    *s = 0;
    wsprintfA(dirpat, "%s\\Save\\*", base);
    WIN32_FIND_DATAA fd;
    HANDLE hd = FindFirstFileA(dirpat, &fd);
    if (hd == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == '.') continue;
        wsprintfA(plrpat, "%s\\Save\\%s\\*.PLR", base, fd.cFileName);
        WIN32_FIND_DATAA pf;
        HANDLE hp = FindFirstFileA(plrpat, &pf);
        if (hp == INVALID_HANDLE_VALUE) continue;
        do {
            char full[MAX_PATH], bak[MAX_PATH], text[65536];
            wsprintfA(full, "%s\\Save\\%s\\%s", base, fd.cFileName, pf.cFileName);
            wsprintfA(bak, "%s.bak", full);
            HANDLE f = CreateFileA(full, GENERIC_READ, FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING, 0, NULL);
            if (f == INVALID_HANDLE_VALUE) continue;
            DWORD sz = GetFileSize(f, NULL);
            if (sz == 0 || sz >= sizeof(text)) { CloseHandle(f); continue; }
            DWORD got = 0;
            ReadFile(f, text, sz, &got, NULL);
            CloseHandle(f);
            text[got] = 0;
            char* hit = strstr(text, "Vehicle Removal=\"10.");
            if (!hit) continue;   /* already 60, or a different layout */
            if (GetFileAttributesA(bak) == INVALID_FILE_ATTRIBUTES)
                CopyFileA(full, bak, TRUE);
            hit[17] = '6';        /* "10.00000" -> "60.00000" */
            HANDLE w = CreateFileA(full, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, 0, NULL);
            if (w != INVALID_HANDLE_VALUE) {
                DWORD wr;
                WriteFile(w, text, got, &wr, NULL);
                CloseHandle(w);
            }
        } while (FindNextFileA(hp, &pf));
        FindClose(hp);
    } while (FindNextFileA(hd, &fd));
    FindClose(hd);
}
#endif

/* --- shared-memory bridge lifecycle (see the globals block) ------------- */
static BOOL mon_shared_open(void) {
    /* The game and Python GUI may run at different integrity levels. Give the
     * read-only telemetry mapping an explicit null DACL so the elevated viewer
     * can open it while the DLL remains inside the ordinary game process. */
    SECURITY_DESCRIPTOR descriptor;
    SECURITY_ATTRIBUTES attributes;
    SECURITY_ATTRIBUTES* security = NULL;
    if (InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) &&
        SetSecurityDescriptorDacl(&descriptor, TRUE, NULL, FALSE)) {
        ZeroMemory(&attributes, sizeof(attributes));
        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = &descriptor;
        security = &attributes;
    }

    g_livehud_map = CreateFileMappingA(INVALID_HANDLE_VALUE, security,
                                       PAGE_READWRITE, 0,
                                       sizeof(RfLiveHudShared),
                                       LIVEHUD_MAP_NAME);
    if (g_livehud_map) {
        g_livehud_shared = (RfLiveHudShared*)MapViewOfFile(
            g_livehud_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    }
    if (!g_livehud_shared) {
        if (g_livehud_map) {
            CloseHandle(g_livehud_map);
            g_livehud_map = NULL;
        }
        return FALSE;
    }
    ZeroMemory(g_livehud_shared, sizeof(*g_livehud_shared));
    g_livehud_shared->magic = LIVEHUD_MAP_MAGIC;
    g_livehud_shared->version = LIVEHUD_MAP_VERSION;
    g_livehud_shared->original_entrant_count = VEHICLE_COUNT;
    g_livehud_shared->native_vehicle_count = VEHICLE_COUNT;
    g_livehud_shared->state_flags = LIVEHUD_STATE_ACTIVE;
    MemoryBarrier();

    g_mon_map = CreateFileMappingA(INVALID_HANDLE_VALUE, security,
                                   PAGE_READWRITE, 0,
                                   sizeof(RFMonShared), MON_MAP_NAME);
    if (!g_mon_map) return TRUE;
    g_mon_shared = (RFMonShared*)MapViewOfFile(g_mon_map, FILE_MAP_ALL_ACCESS,
                                               0, 0, 0);

    g_aiw_mon_map = CreateFileMappingA(INVALID_HANDLE_VALUE, security,
                                       PAGE_READWRITE, 0,
                                       sizeof(AIWMonShared), AIW_MON_MAP_NAME);
    if (g_aiw_mon_map) {
        g_aiw_mon_shared = (AIWMonShared*)MapViewOfFile(
            g_aiw_mon_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (g_aiw_mon_shared) {
            ZeroMemory(g_aiw_mon_shared, sizeof(*g_aiw_mon_shared));
            g_aiw_mon_shared->magic = AIW_MON_MAGIC;
            g_aiw_mon_shared->version = 4;
            g_aiw_mon_shared->player_nearest_index = -1;
            g_aiw_mon_shared->player_segment_index = -1;
            g_aiw_mon_shared->blocking_nearest_index = -1;
            g_aiw_mon_shared->player_corridor = -1;
            g_aiw_mon_shared->blocking_corridor = -1;
        }
    }
    return TRUE;
}

static void livehud_shared_publish(void) {
    LONG sequence;
    LONG entrant_count;
    uint32_t flags = LIVEHUD_STATE_ACTIVE;
    if (!g_livehud_shared) return;
    entrant_count = (g_session_active && g_session_capacity >= 2)
        ? g_session_capacity : VEHICLE_COUNT;
    if (entrant_count < 0 || entrant_count > 1024) entrant_count = 0;
    if (g_session_active) flags |= LIVEHUD_STATE_SESSION;
    if (entrant_count >= 2 && VEHICLE_COUNT != entrant_count)
        flags |= LIVEHUD_STATE_ROSTER_REDUCED;
    if (g_monitor_phase != 0) flags |= LIVEHUD_STATE_RESTART;
    sequence = InterlockedCompareExchange(&g_livehud_shared->sequence, 0, 0);
    if (sequence & 1) ++sequence;
    InterlockedExchange(&g_livehud_shared->sequence, sequence + 1);
    MemoryBarrier();
    g_livehud_shared->original_entrant_count = entrant_count;
    g_livehud_shared->native_vehicle_count = VEHICLE_COUNT;
    g_livehud_shared->state_flags = flags;
    g_livehud_shared->session_serial = g_session_serial;
    MemoryBarrier();
    InterlockedExchange(&g_livehud_shared->sequence, sequence + 2);
}

static void mon_shared_publish(void) {
    livehud_shared_publish();
    if (!g_mon_shared) return;
    g_mon_shared->game_state      = g_monitor_game_state;
    g_mon_shared->race_phase      = g_monitor_race_phase;
    g_mon_shared->settled         = g_monitor_settled ? 1 : 0;
    g_mon_shared->yellow_active   = g_monitor_native_yellow ? 1 : 0;
    g_mon_shared->yellow_ms       = g_monitor_yellow_ms;
    g_mon_shared->terminals_window = g_monitor_terminals;
    g_mon_shared->terminals_now   = g_monitor_terminal_now;
    g_mon_shared->stalled         = g_monitor_stalled;
    g_mon_shared->stopped         = g_monitor_stopped;
    g_mon_shared->crashed         = g_monitor_crashed;
    g_mon_shared->cooldown_ms     = g_monitor_cooldown_ms;
    g_mon_shared->candidate       = g_monitor_auto_candidate ? 1 : 0;
    g_mon_shared->phase           = g_monitor_phase;
    g_mon_shared->held            = g_late_count;
    g_mon_shared->incident_score  = g_monitor_incident_score;
    g_mon_shared->incident_cars   = g_monitor_incident_cars;
    g_mon_shared->incident_terminals = g_monitor_incident_terminals;
    g_mon_shared->paused          = g_monitor_paused;
    g_mon_shared->multiplayer     = g_monitor_multiplayer;
    g_mon_shared->opening_window  = g_monitor_opening_window;
    g_mon_shared->yellow_required_ms = g_monitor_yellow_required_ms;
    g_mon_shared->candidate_confirm_ms = g_monitor_candidate_confirm_ms;
    g_mon_shared->auto_enabled    = g_monitor_auto_enabled;
    g_mon_shared->incident_damaged_stationary =
        g_monitor_incident_damaged_stationary;
    g_mon_shared->stop_reason = g_monitor_stop_reason;
    g_mon_shared->prestop_elapsed_ms = g_monitor_prestop_elapsed_ms;
    g_mon_shared->blocking_road_contacts = g_monitor_blocking_road_contacts;

    if (g_aiw_mon_shared) {
        LONG sequence = g_aiw_mon_shared->sequence;
        InterlockedExchange(&g_aiw_mon_shared->sequence, sequence + 1);
        g_aiw_mon_shared->valid = g_aiw_valid;
        g_aiw_mon_shared->main_waypoints = g_aiw_main_waypoints;
        g_aiw_mon_shared->player_found = g_aiw_player_found;
        g_aiw_mon_shared->player_nearest_index = g_aiw_player_nearest_index;
        g_aiw_mon_shared->player_segment_index = g_aiw_player_segment_index;
        g_aiw_mon_shared->player_band = g_aiw_player_band;
        g_aiw_mon_shared->player_node_m = g_aiw_player_node_m;
        g_aiw_mon_shared->player_path_m = g_aiw_player_path_m;
        g_aiw_mon_shared->player_surface_contacts =
            g_aiw_player_surface_contacts;
        g_aiw_mon_shared->player_off_track = g_aiw_player_off_track;
        g_aiw_mon_shared->blocking_valid = g_aiw_blocking_valid;
        g_aiw_mon_shared->blocking_nearest_index =
            g_aiw_blocking_nearest_index;
        g_aiw_mon_shared->blocking_path_m = g_aiw_blocking_path_m;
        g_aiw_mon_shared->player_dry_line_m = g_aiw_player_dry_line_m;
        g_aiw_mon_shared->player_wet_line_m = g_aiw_player_wet_line_m;
        g_aiw_mon_shared->player_closest_line_m =
            g_aiw_player_closest_line_m;
        g_aiw_mon_shared->player_road_left_m = g_aiw_player_road_left_m;
        g_aiw_mon_shared->player_road_right_m = g_aiw_player_road_right_m;
        g_aiw_mon_shared->player_road_width_m = g_aiw_player_road_width_m;
        g_aiw_mon_shared->player_lateral_m = g_aiw_player_lateral_m;
        g_aiw_mon_shared->player_free_left_m = g_aiw_player_free_left_m;
        g_aiw_mon_shared->player_free_right_m = g_aiw_player_free_right_m;
        g_aiw_mon_shared->player_best_pass_m = g_aiw_player_best_pass_m;
        g_aiw_mon_shared->player_critical_radius_m =
            g_aiw_player_critical_radius_m;
        g_aiw_mon_shared->player_danger_radius_m =
            g_aiw_player_danger_radius_m;
        g_aiw_mon_shared->player_stationary_ms = g_aiw_player_stationary_ms;
        g_aiw_mon_shared->player_stationary_confirmed =
            g_aiw_player_stationary_confirmed;
        g_aiw_mon_shared->player_game_paused = g_aiw_player_game_paused;
        g_aiw_mon_shared->player_on_road_geom = g_aiw_player_on_road_geom;
        g_aiw_mon_shared->player_speed = g_aiw_player_speed;
        g_aiw_mon_shared->blocking_line_m = g_aiw_blocking_line_m;
        g_aiw_mon_shared->blocking_band = g_aiw_blocking_band;
        g_aiw_mon_shared->player_corridor = g_aiw_player_corridor;
        g_aiw_mon_shared->player_main_band = g_aiw_player_main_band;
        g_aiw_mon_shared->player_pit_band = g_aiw_player_pit_band;
        g_aiw_mon_shared->pit_waypoints = g_aiw_pit_waypoints;
        g_aiw_mon_shared->player_main_line_m = g_aiw_player_main_line_m;
        g_aiw_mon_shared->player_pit_line_m = g_aiw_player_pit_line_m;
        g_aiw_mon_shared->blocking_corridor = g_aiw_blocking_corridor;
        g_aiw_mon_shared->weather_on_wetness = g_aiw_weather_on_wetness;
        g_aiw_mon_shared->weather_off_wetness = g_aiw_weather_off_wetness;
        g_aiw_mon_shared->weather_mean_wetness = g_aiw_weather_mean_wetness;
        g_aiw_mon_shared->weather_class = g_aiw_weather_class;
        MemoryBarrier();
        InterlockedExchange(&g_aiw_mon_shared->sequence, sequence + 2);
    }
}

static void mon_shared_close(void) {
    if (g_livehud_shared) {
        g_livehud_shared->state_flags = 0;
        g_livehud_shared->original_entrant_count = VEHICLE_COUNT;
        g_livehud_shared->native_vehicle_count = VEHICLE_COUNT;
        MemoryBarrier();
        UnmapViewOfFile(g_livehud_shared);
        g_livehud_shared = NULL;
    }
    if (g_livehud_map) { CloseHandle(g_livehud_map); g_livehud_map = NULL; }
    if (g_aiw_mon_shared) {
        UnmapViewOfFile(g_aiw_mon_shared);
        g_aiw_mon_shared = NULL;
    }
    if (g_aiw_mon_map) { CloseHandle(g_aiw_mon_map); g_aiw_mon_map = NULL; }
    if (g_mon_shared) { UnmapViewOfFile(g_mon_shared); g_mon_shared = NULL; }
    if (g_mon_map)    { CloseHandle(g_mon_map);         g_mon_map = NULL; }
}

static void status_gui_pump(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static void status_gui_update(int phase) {
    DWORD now = GetTickCount();
    if (now - g_status_last_update < 150) return;
    g_status_last_update = now;
    /* Refresh the gate fields here too: check_auto_redflag() only runs inside
     * a race, so without this the monitor would show stale "Live race" state
     * from a previous session while sitting in the menu. */
    g_monitor_game_state = GAME_STATE;
    g_monitor_race_phase = RACE_PHASE;
    g_monitor_settled    = race_is_settled();
    g_monitor_stalled    = count_stalled();
    g_monitor_stopped    = count_stopped();
    g_monitor_crashed    = count_crashed();
    g_monitor_phase      = phase;
    g_monitor_paused     = PAUSE_FLAG ? 1 : 0;
    g_monitor_multiplayer = is_multiplayer() ? 1 : 0;
    g_monitor_auto_enabled =
        (AUTO_REDFLAG_ENABLED && g_rf_config.automatic_enabled) ? 1 : 0;
    g_monitor_opening_window = g_settled_since != 0 &&
        now - g_settled_since <= g_rf_config.opening_window_ms;
    g_monitor_yellow_required_ms = g_monitor_opening_window
        ? g_rf_config.opening_yellow_ms : g_rf_config.yellow_ms;
    if (!g_monitor_opening_window && weather_incident_assist_level() >= 1 &&
        g_monitor_yellow_required_ms > g_rf_config.wet_yellow_ms)
        g_monitor_yellow_required_ms = g_rf_config.wet_yellow_ms;
    if (!g_rf_config.require_native_yellow)
        g_monitor_yellow_required_ms = 0;
    g_monitor_candidate_confirm_ms = g_auto_candidate_since
        ? now - g_auto_candidate_since : 0;
    if (g_monitor_candidate_confirm_ms > g_rf_config.final_confirm_ms)
        g_monitor_candidate_confirm_ms = g_rf_config.final_confirm_ms;
    /* Yellow + wrecked-AI counters are live here unconditionally, independent
     * of the auto-fire gate: the gate can be closed (wrong phase) while the
     * game still shows a yellow and cars are terminal, and the monitor must
     * never display stale zeros in that situation. */
    refresh_yellow_state(now);
    refresh_aiw_player_metric();
    refresh_terminal_state(now);
#if IN_GAME_MONITOR_ENABLED
    char text[512];
    wsprintfA(text,
        "RED FLAG MONITOR\r\n"
        "Trigger: %s\r\n"
        "\r\n"
        "Live race:          %s\r\n"
        "Yellow flag for 3s: %s  (%lu / %d ms)\r\n"
        "Wrecked AI:         %d now | %d terminal in cluster\r\n"
        "Severe pattern:     %d terminal + %d stationary damaged (%d cars)\r\n"
        "Crashed cars:       %d\r\n"
        "Stalled cars:       %d\r\n"
        "Stopped cars:       %d\r\n"
        "Cooldown clear:     %s  (%lu ms left)\r\n"
        "\r\n"
        "Result: %s\r\n"
        "\r\n"
        "NUMPAD0: hide/show",
        g_monitor_auto_enabled ? "AUTO - will fire" : "DISPLAY ONLY",
        g_monitor_settled ? "YES" : "NO",
        g_monitor_yellow_ms >= g_monitor_yellow_required_ms ? "YES" : "NO",
        (unsigned long)g_monitor_yellow_ms,
        (int)g_monitor_yellow_required_ms,
        g_monitor_terminal_now, g_monitor_incident_terminals,
        g_monitor_incident_terminals,
        g_monitor_incident_damaged_stationary, g_monitor_incident_cars,
        g_monitor_crashed,
        g_monitor_stalled,
        g_monitor_stopped,
        g_monitor_cooldown_ms == 0 ? "YES" : "NO",
        (unsigned long)g_monitor_cooldown_ms,
        g_monitor_auto_candidate ? "READY - auto countdown will fire" : "not armed");
    SetWindowTextA(g_status_text, text);
#endif
    mon_shared_publish();
    (void)phase;
}

/* QualPos/lap a LATE-swept car had before we stamped it 200/0, so an
 * unconfirmed sweep can be undone. See revalidate_dnf(). */
static int*   g_late_saved_qp = NULL;
static int*   g_late_saved_lap = NULL;
static void** g_late_ptrs = NULL;
static int    g_late_count = 0;

/* Sustained-kill tracking.
 *
 * A single act==0 sample cannot distinguish a wreck from a blip:
 *   - Heidfeld (gr41) read act=0 momentarily and was never retired -> healthy.
 *   - Hakkinen  (gr42) read act=0 and STAYED there, genuinely wrecked, but the
 *     game had not stamped fin/ret before the restart, so revalidate_dnf()
 *     wrongly put him back on the grid.
 * The game's classification delay is variable and lands right around the 10s
 * debug countdown, so racing it is not viable. DURATION is the discriminator:
 * a car continuously act==0 for KILL_CONFIRM_MS is terminal regardless of
 * whether the game has caught up yet. */
#define KILL_CONFIRM_MS 4000

static BOOL veh_killed(char* veh);   /* defined with the auto-detect block */

static void** g_kill_ptrs = NULL;
static DWORD* g_kill_since = NULL;
static int    g_kill_count = 0;
static DWORD  g_gate_pause_started = 0;

/* How long this car has read act==0 continuously, in ms. 0 = not killed.
 *
 * PAUSE-FROZEN (2026-08-17): the sustain is race time, not wall time. While
 * the game is paused the clock neither starts nor advances, so a car paused
 * at 3.9 s cannot cross the 4 s confirm threshold on resume. The DNF late-
 * sweep path that also calls this never runs while paused (the worker skips
 * phase processing on PAUSE_FLAG), so the freeze is safe for both callers. */
static DWORD kill_duration(char* veh) {
    DWORD now = GetTickCount();
    BOOL killed = veh_killed(veh);
    for (int i = 0; i < g_kill_count; i++) {
        if (g_kill_ptrs[i] != (void*)veh) continue;
        if (!killed) { g_kill_since[i] = 0; return 0; }
        if (PAUSE_FLAG) return 0;
        if (g_kill_since[i] == 0) { g_kill_since[i] = now; return 0; }
        return now - g_kill_since[i];
    }
    if (g_kill_count < g_session_capacity) {
        g_kill_ptrs[g_kill_count]  = veh;
        g_kill_since[g_kill_count] = (killed && !PAUSE_FLAG) ? now : 0;
        g_kill_count++;
    }
    return 0;
}

static void kill_tracker_reset(void) { g_kill_count = 0; }

/* Convert wall-clock timestamps used by automatic qualification into
 * race-time clocks. Merely skipping polling during PAUSE is insufficient:
 * the first post-pause subtraction otherwise includes the whole pause. Shift
 * every live deadline/start timestamp forward by the paused duration. */
static void freeze_gate_clocks(DWORD now) {
    if (PAUSE_FLAG) {
        if (!g_gate_pause_started) g_gate_pause_started = now;
        return;
    }
    if (!g_gate_pause_started) return;
    DWORD paused_ms = now - g_gate_pause_started;
    g_gate_pause_started = 0;
    if (!paused_ms) return;

#define SHIFT_LIVE_TICK(value) do { if ((value) != 0) (value) += paused_ms; } while (0)
    SHIFT_LIVE_TICK(g_native_yellow_since);
    SHIFT_LIVE_TICK(g_auto_candidate_since);
    SHIFT_LIVE_TICK(g_settled_since);
    SHIFT_LIVE_TICK(g_auto_cooldown_until);
    SHIFT_LIVE_TICK(g_post_restart_normalize_until);
    for (int i = 0; i < g_session_capacity; i++) {
        SHIFT_LIVE_TICK(g_term_times[i]);
        SHIFT_LIVE_TICK(g_incident_since[i]);
        SHIFT_LIVE_TICK(g_stationary_hazard_since[i]);
        SHIFT_LIVE_TICK(g_inverted_hazard_since[i]);
        SHIFT_LIVE_TICK(g_kill_since[i]);
    }
#undef SHIFT_LIVE_TICK
}

static void** g_dnf_ptrs = NULL;
static int*   g_dnf_qualpos = NULL;
static int    g_dnf_count = 0;
static BOOL   g_dnf_hidden = FALSE;

/* Restart-roster ownership. Retired cars must be removed BEFORE the game's
 * restart grid constructor runs. The game's own no-argument rebuild routine
 * packs VEHICLE_ARRAY from OFF_SLOTLIVE; VEHICLE_COUNT then bounds every grid,
 * scoring and simulation walk. This list persists for the exact race session
 * so later red flags can add more DNFs without reviving earlier ones. */
static void** g_roster_ptrs = NULL;
static void** g_roster_full_ptrs = NULL;
static int*   g_roster_saved_roles = NULL;
static int*   g_roster_saved_finish = NULL;
static int*   g_roster_saved_reason = NULL;
static int*   g_roster_saved_lap = NULL;
static int*   g_roster_saved_racepos = NULL;
static int    g_roster_count = 0;
static int    g_roster_original_count = 0;
static uintptr_t g_roster_reduced_epoch = 0;
static BOOL   g_roster_exclusion_active = FALSE;
static BOOL   g_roster_results_restored = FALSE;

/* A retired/disqualified player is not an AI DNF. The game, camera and replay
 * systems retain PLAYER_VEH_PTR identity, while the restart grid must treat
 * only g_active[] as starters. Keep the native terminal tuple in a separate
 * sidecar so the player is never role-quarantined, assigned a grid position,
 * hidden at Y=-1000 or rewritten as a generic FinishState=2 retirement. */
static BOOL      g_player_out_active = FALSE;
static BOOL      g_player_out_committed = FALSE;
static char*     g_player_out_ptr = NULL;
static uintptr_t g_player_out_epoch = 0;
static int       g_player_out_finish = 0;
static int       g_player_out_reason = 0;
static int       g_player_out_lap = 0;
static int       g_player_out_racepos = 0;
static int       g_player_out_qualpos = 0;
static int       g_player_out_role = 0;
static BYTE      g_player_out_retired = 0;
static BYTE      g_player_out_active_flag = 0;
static int       g_player_out_player_out = 0;
static BOOL      g_player_out_garage_valid = FALSE;
static float     g_player_out_px = 0.0f;
static float     g_player_out_py = 0.0f;
static float     g_player_out_pz = 0.0f;
static float     g_player_out_nx = 0.0f;
static float     g_player_out_ny = 0.0f;
static float     g_player_out_nz = 0.0f;
static int       g_player_out_pitstate = 0;

/* Diagnostic-only native grid-placement gate. VehicleCount and the active
 * pointer array remain untouched: only the exact captured terminal player is
 * denied a grid transform while this DLL owns the matching restart epoch. */
#define PLAYER_GRID_PLACE_ENTRY 0x004D10E0u
#define PLAYER_GRID_PLACE_STEAL 10u
static BYTE g_player_grid_place_orig[PLAYER_GRID_PLACE_STEAL];
static BYTE* g_player_grid_place_trampoline = NULL;
static BOOL g_player_grid_place_installed = FALSE;
static volatile LONG g_player_grid_gate_armed = 0;
static volatile LONG g_player_grid_gate_inflight = 0;
static uintptr_t g_player_grid_gate_epoch = 0;
static void** g_player_tuple_order = NULL;
static int g_player_tuple_order_count = 0;
static uintptr_t g_player_tuple_epoch = 0;
static volatile LONG g_player_tuple_armed = 0;
static volatile LONG g_player_tuple_pending = 0;
static volatile LONG g_player_tuple_result = 0;
static volatile LONG g_player_tuple_callbacks = 0;
static volatile LONG g_player_native_garage_result = 0;
static int g_player_native_garage_pit = -1;
static int g_player_native_garage_ordinal = -1;
static float g_player_native_garage_pos[3] = {0.0f, 0.0f, 0.0f};
/* The pit-failure module's SessionType flip is explicitly race-prone and can
 * re-grid the field. Once the terminal player is already at a proven native
 * garage pose, reuse only the durable retirement invariant: retain the exact
 * player's SessionTickUpdate guard until session end. This prevents the car
 * returning to physics/control without rewriting DQ/retirement classification,
 * PLAYER_OUT, roster count/order, or the native pose. */
static BOOL g_player_terminal_garage_lock_active = FALSE;
static BOOL snapshot_player_tuple_order(void);
static BOOL      g_post_constructor_hold = FALSE;

/* The worker arms this only after the requested restart has reached the real
 * GRID_READY state. The existing game-thread SessionTickUpdate call-site hook
 * claims it on the first live RacePhase=5 tick, before continuing the vehicle
 * update. This closes the polling gap between restoring a role and pinning the
 * same entrant as a vanilla-shaped DNF. */
static volatile LONG g_green_restore_armed = 0;
static volatile LONG g_green_restore_result = 0;
/* F1C performs additional survivor initialization after the first live-edge
 * broker callback returns. A second game-thread transaction on the following
 * update restores continuity fields that native initialization may overwrite. */
static volatile LONG g_post_green_continuity_armed = 0;
static volatile LONG g_post_green_continuity_pending = 0;
static volatile LONG g_post_green_continuity_result = 0;

/* Cars currently sunk to Y=-1000 by hide_dnf_cars(), with the values they had
 * BEFORE we sank them.
 *
 * hide_dnf_cars() used to write HIDE_Y and never restore it. That leaked across
 * red-flag sequences: on a SECOND red flag the game tries to place the DNF car
 * in its pit box, but physics Y is still -1000 from last time, and the
 * SceneRenderUpdate hook skips DNF cars so the node never re-syncs either — so
 * the car never appeared in its box. Reported in-game 2026-07-24.
 *
 * This list is deliberately independent of g_dnf_ptrs, which is rebuilt from
 * scratch by every snapshot_positions(). */
static void** g_hidden_ptrs = NULL;
static float* g_hidden_node_y = NULL;
static float* g_hidden_phys_y = NULL;
static int    g_hidden_count = 0;
static uintptr_t g_hidden_epoch = 0;

static char** g_active = NULL;
static int*   g_active_rp = NULL;
static int    g_active_count = 0;
static int    g_placed = 0;

/* ---- no-overtake pace hold state, one entry per SLOT (not per g_active) --
 * Slot-indexed, because the law needs the whole field including cars this mod
 * has already classified as DNF: a wreck still has to be seen as an obstacle
 * even though it is not governed. */
static float* g_np_saved = NULL;    /* pre-hold maxSpeed                  */
static float* g_np_pass = NULL;     /* pre-hold PassingNorm               */
static float* g_np_aggro = NULL;    /* pre-hold TrackAggression           */
static float* g_np_lastw = NULL;    /* what WE last wrote (override test) */
static char*  g_np_armed = NULL;    /* 1 = this slot is ours              */
static float (*g_np_hist)[SPD_HIST] = NULL;
static int    g_np_hidx = 0;
static float  g_np_lap_len = 0.0f;
static int    g_np_leader = -1;
static int    g_np_cmd = PACE_TARGET;
static float  g_np_ramp_from = 0.0f;
static DWORD  g_np_ramp_t0 = 0;
static BOOL   g_np_active = FALSE;

/* Snapshotted race positions. LOGGING / GRID ONLY.
 *
 * The old enforce_positions() wrote these back to +0x4060 every tick to
 * "prevent overtaking". It does no such thing: +0x4060 is the standings number
 * the game recomputes from lap number + lap distance, so the write froze the
 * timing screen while the cars carried on racing underneath it. The array is
 * kept because apply_qualpos() builds the restart grid from the same order,
 * and because drift against it is now an honest measurement of whether the
 * pace hold is working. */
static int*   g_locked_racepos = NULL;
static BOOL   g_pos_lock_active = FALSE;

/* Saved fuel and driver-model visibility per active car (including player). */
static float* g_saved_fuel = NULL;
static BYTE*  g_saved_driver_visible = NULL;

/* Reviewed AI pit-strategy plan and completed-stop cursor per active car.
 * Native Game_Restart rebuilds every survivor's plan and zeroes its cursor in
 * sub_429960 (vehicle+0x45E0), so a car restored to a late lap could see an
 * already-served early stop as overdue and request a fresh stop. Captured on
 * the game thread immediately before the native restart, restored before the
 * first post-restart AI decision. Retirees never enter g_active and are never
 * captured or restored here. See SDK f1c_pit_strategy.h for the reviewed
 * layout, bounds and evidence. */
static F1CPitStrategyState* g_saved_pit_strategy = NULL;
static BYTE* g_saved_pit_valid = NULL;

/* Live replay roster baseline. Native instant replay saves exactly
 * VehicleCount entries (LoadingPhase1), can destroy an existing vehicle with
 * no selected replay identity and decrement VehicleCount unconditionally
 * (ProcessDriverEntries - a parked Safety Car at +13000 == -1), then restores
 * using the shrunken count (LoadingPhase2), stranding the last saved entry.
 * RF captures and publishes the baseline ONLY on the game thread (a broker
 * task), so replay state can never contaminate a sample, and keeps it in
 * process-lifetime static storage so a queued task never reads freed memory.
 * Restore re-verifies the reviewed identity fields of every captured entry
 * before any write. Pointers, excluded vehicles and DNF tuples are never
 * touched; every step fails closed. See f1c_live_replay.h. */
#define RF_REPLAY_MAX 104
#define RF_REPLAY_CAPTURE_MS 250
#define RF_REPLAY_MODE (*(volatile int*)F1C_REPLAY_MODE_ADDRESS)
static void*     g_replay_base_ptr[RF_REPLAY_MAX];
static int       g_replay_base_state[RF_REPLAY_MAX];   /* +13000 control */
static BYTE      g_replay_base_visible[RF_REPLAY_MAX]; /* +4189 driver  */
static int       g_replay_base_render[RF_REPLAY_MAX];  /* +18620 bit1  */
static int       g_replay_base_roster[RF_REPLAY_MAX];
static uintptr_t g_replay_base_scene[RF_REPLAY_MAX];
static char      g_replay_base_name[RF_REPLAY_MAX][F1C_REPLAY_NAME_BYTES];
static int       g_replay_base_count = 0;
static BOOL      g_replay_base_valid = FALSE;
static uintptr_t g_replay_base_epoch = 0;
static int       g_replay_last_mode = F1C_REPLAY_MODE_IDLE;
static volatile LONG g_replay_capture_pending = 0;
static volatile LONG g_replay_restore_pending = 0;
static volatile LONG g_replay_restore_result = 0;
static volatile LONG g_replay_restore_posts = 0;
static DWORD     g_replay_last_capture = 0;
/* Validated once per session: the reviewed SetDriverVisibility bytes. */
static F1CReplayVisibilitySet g_replay_visibility_fn = NULL;

/* Saved current laps and total laps for lap adjustment */
static int*   g_saved_currentlap = NULL;
static int    g_saved_total_laps = 0;
static int    g_max_driven_laps = 0;

/* v70: session-owned restart checkpoint, independent of live snapshot arrays.
 * Captures the restart transaction's saved values, never later racing state. */
typedef struct {
    void* full;
    char* active;
    int rank, grid, lap;
    float fuel;
    BYTE visible;
    void* dnf;
    int dnf_qp;
    F1CRestartPenalties penalties;
    F1CPitStrategyState strategy;
    BYTE strategy_valid;
} RfRestartCheckpointCar;
static RfRestartCheckpointCar* g_checkpoint_cars = NULL;
typedef struct {char* vehicle; F1CRestartPenalties state;} RfPenaltySnapshot;
static RfPenaltySnapshot* g_penalty_snapshot=NULL;
static int g_penalty_count=0;
static BOOL g_penalty_valid=FALSE;
static F1CRestartPenalties* penalty_snapshot_for(char* vehicle) {
    for(int i=0;i<g_penalty_count;i++)
        if(g_penalty_snapshot[i].vehicle==vehicle)return &g_penalty_snapshot[i].state;
    return NULL;
}

static void** g_initial_ptrs = NULL;
static int* g_initial_qp = NULL;
static int g_initial_count = 0;
static int g_checkpoint_count = 0, g_checkpoint_active = 0, g_checkpoint_dnf = 0;
static int g_checkpoint_total = 0, g_checkpoint_maxlap = 0;
static unsigned long g_checkpoint_serial = 0;
static BOOL g_checkpoint_valid = FALSE, g_checkpoint_replay = FALSE;
static BOOL g_manual_prompt = FALSE;
static DWORD g_player_refusal_until = 0;
static DWORD g_manual_prompt_since = 0;
static BOOL g_manual_countdown = FALSE;
static BOOL g_manual_countdown_from_detector = FALSE;
static DWORD g_manual_countdown_since = 0;
static BOOL g_announcement_snapshot = FALSE;
static DWORD g_rf_policy_now = 0, g_rf_policy_last = 0;
static DWORD g_player_arrived_at = 0;
static DWORD g_drive_started=0, g_weather_rf_since=0;
static RfWeatherRestart g_weather_restart;
static RfHandoffWait g_handoff_wait;
static BOOL g_weather_wait_input=FALSE;
static int g_weather_restart_verdict=RF_WEATHER_READY;
static void weather_restart_message(void) {
    g_weather_wait_input=TRUE;
    hud_auto_notice_write(0,"RED FLAG: WAITING FOR WEATHER");
    if(g_rf_config.weather_override)rf_key_hint(1,
        g_weather_restart_verdict==RF_WEATHER_CONFIRM?"AGAIN TO RESTART":"TO REQUEST RESTART");
    else hud_auto_notice_write(1,"WAIT FOR CONDITIONS TO IMPROVE");
    hud_auto_notice_write(2,"WEATHER STILL UNSAFE OR UNCONFIRMED");
}
static BOOL g_player_box_confirmed = FALSE;
/* Observed active time; a pause or long unsampled interval proves no dwell. */
static void rf_policy_clock(DWORD now, BOOL paused) {
    if(g_rf_policy_last && !paused) {
        DWORD delta=now-g_rf_policy_last;
        if(delta<=1000u)g_rf_policy_now+=delta;
    }
    g_rf_policy_last=now;
}
/* 4 = started, 2 = due, 3 = cancelled, 0 = waiting/inactive.
 * Cancellation wins even on the exact expiry tick. A start edge arms the
 * clock only; a later fresh T edge cancels it. DWORD subtraction tolerates wrap. */
static int manual_countdown_update(BOOL start, BOOL safe, BOOL edge, BOOL cancel,
                                   DWORD now, DWORD duration) {
    if (g_manual_countdown) {
        if (!safe || (g_rf_config.allow_cancel && edge)) { g_manual_countdown=FALSE; return 3; }
        (void)cancel; /* Esc/menu is not permission to cancel a committed RF. */
        if ((DWORD)(now-g_manual_countdown_since) >= duration) {
            g_manual_countdown=FALSE;
            return 2;
        }
        return 0;
    }
    if (!start || !safe || cancel) return 0;
    g_manual_countdown=TRUE;
    g_manual_countdown_since=now;
    return 4;
}
static unsigned manual_countdown_seconds(DWORD now, DWORD duration) {
    DWORD elapsed=now-g_manual_countdown_since;
    return elapsed >= duration ? 0 : (unsigned)((duration-elapsed+999u)/1000u);
}
/* Track raw T separately: releasing Ctrl while T stays held must not create
 * a fresh plain-T press. Applies to initiation AND detector acceptance. */
static BOOL manual_t_edge(BOOL down, BOOL previous, BOOL modifiers) {
    return down && !previous && !modifiers;
}
/* Foreground ownership is separate from raw T state. Returning to the game
 * with T held must not open a prompt or accept an automatic offer. */
static BOOL manual_t_foreground_edge(BOOL down, BOOL previous, BOOL modifiers,
                                     BOOL foreground, BOOL previous_foreground) {
    return foreground && previous_foreground && manual_t_edge(down, previous, modifiers);
}
/* Result: 0 unchanged, 1 prompt opened, 2 confirmed, 3 cancelled.
 * Edge detection stays in the release input loop; held T cannot confirm. */
static int manual_request_update(BOOL enabled, BOOL safe, BOOL edge,
                                 BOOL cancel, BOOL automatic_notice, DWORD now) {
    if (g_manual_prompt && (!enabled || !safe || cancel ||
        (DWORD)(now-g_manual_prompt_since) >= 15000u)) {
        g_manual_prompt=FALSE;
        return 3;
    }
    if (!enabled || !safe || !edge || cancel ||
        (!g_manual_prompt && automatic_notice)) return 0;
    if (g_manual_prompt) { g_manual_prompt=FALSE; return 2; }
    g_manual_prompt=TRUE;
    g_manual_prompt_since=now;
    return 1;
}
static struct {
    BOOL g_player_out_active;
    BOOL g_player_out_committed;
    char* g_player_out_ptr;
    uintptr_t g_player_out_epoch;
    int g_player_out_finish;
    int g_player_out_reason;
    int g_player_out_lap;
    int g_player_out_racepos;
    int g_player_out_qualpos;
    int g_player_out_role;
    BYTE g_player_out_retired;
    BYTE g_player_out_active_flag;
    int g_player_out_player_out;
    BOOL g_player_out_garage_valid;
    float g_player_out_px;
    float g_player_out_py;
    float g_player_out_pz;
    float g_player_out_nx;
    float g_player_out_ny;
    float g_player_out_nz;
    int g_player_out_pitstate;
} g_checkpoint_player;
static __attribute__((noinline, optimize("O0"))) BOOL checkpoint_capture(void);
static __attribute__((noinline, optimize("O0"))) BOOL checkpoint_restore(void);
static void initial_grid_capture(void);
static BOOL initial_grid_restore(void);

static BYTE*  g_trampoline = NULL;
static BYTE   g_orig_bytes[CALL_SIZE];
static BOOL   g_hook_installed = FALSE;

static BYTE*  g_trampoline2 = NULL;
static BYTE   g_orig_bytes2[CALL_SIZE];
static BOOL   g_hook2_installed = FALSE;

static BYTE*  g_grid_camera_trampoline = NULL;
static BYTE   g_grid_camera_orig[GRID_CAMERA_CALL_SIZE];
static BOOL   g_grid_camera_hook_installed = FALSE;

/* Game_Restart writes GameState=4 before it reaches the replay writer, and
 * Replay_Save_State_Machine explicitly refuses to serialize while state 4 is
 * active. The failed v44 filename hook therefore created only the NEXT replay
 * container. V45 posts the game's own writer onto LightLoader's proven game
 * thread immediately BEFORE RESTART_FLAG, while the race is still saveable,
 * then moves the closed vanilla file to Part N. The restart initializes the
 * same vanilla filename for the next segment. */
typedef void* (__attribute__((thiscall)) *replay_finalize_fn)(void*, int);
static volatile LONG g_replay_save_pending = 0;
static volatile LONG g_replay_save_result = 0;
/* RF85: positive ticket=queued, negative ticket=in flight, zero=idle.
 * A delayed cancelled callback cannot claim a later request's ticket. */
static LONG g_replay_ticket_counter = 0;
static LONG g_replay_ticket = 0;
static BOOL g_replay_prepare_started = FALSE;
static BOOL g_replay_prepare_done = FALSE;
static DWORD g_replay_prepare_deadline = 0;
static DWORD g_replay_retry_at = 0;
/* RF88: readiness failures (-4/-5/-6 mean the recorder is not producing a
 * saveable span yet) get a short bounded window; a queued save keeps the full
 * queue deadline. An in-flight native writer (negative pending) is never
 * cancelled regardless of either window. */
#define REPLAY_READINESS_MAX_MS 1000u
#define REPLAY_QUEUE_MAX_MS     5000u

static BOOL replay_prepare_reset(void) {
    LONG pending = InterlockedCompareExchange(&g_replay_save_pending, 0, 0);
    if (pending < 0) return FALSE; /* never invalidate an executing native writer */
    if (pending > 0 && InterlockedCompareExchange(
            &g_replay_save_pending, 0, pending) != pending) return FALSE;
    g_replay_ticket = 0;
    g_replay_prepare_started = g_replay_prepare_done = FALSE;
    g_replay_prepare_deadline = g_replay_retry_at = 0;
    return TRUE;
}
static volatile LONG g_replay_diag_handle = 0;
static volatile LONG g_replay_diag_mode = 0;
static volatile float g_replay_diag_start = 0.0f;
static volatile float g_replay_diag_end = 0.0f;
static LONG g_replay_current_part = 1;
static BOOL g_replay_series_active = FALSE;
static BOOL g_replay_finalized_at_results = FALSE;
static char g_replay_vanilla_path[MAX_PATH];
static char g_replay_series_prefix[MAX_PATH];

static void replay_session_reset(void) {
    if (!replay_prepare_reset()) return;
    InterlockedExchange(&g_replay_save_pending, 0);
    InterlockedExchange(&g_replay_save_result, 0);
    InterlockedExchange(&g_replay_diag_handle, 0);
    InterlockedExchange(&g_replay_diag_mode, 0);
    g_replay_diag_start = g_replay_diag_end = 0.0f;
    g_replay_current_part = 1;
    g_replay_series_active = FALSE;
    g_replay_finalized_at_results = FALSE;
    g_replay_vanilla_path[0] = '\0';
    g_replay_series_prefix[0] = '\0';
}

static BOOL replay_part_path(LONG part, char* out, size_t out_size) {
    int written;
    if (!g_replay_series_prefix[0] || part < 1 || !out || out_size < 2)
        return FALSE;
    written = snprintf(out, out_size, "%s - Part %ld.Vcr",
                       g_replay_series_prefix, part);
    if (written < 0 || (size_t)written >= out_size) {
        out[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL replay_begin_series(const char* vanilla_path) {
    SYSTEMTIME now;
    char stem[MAX_PATH];
    char candidate[MAX_PATH];
    char part1_path[MAX_PATH];
    char part2_path[MAX_PATH];
    char* extension;
    int collision;

    if (!vanilla_path || !*vanilla_path || strlen(vanilla_path) >= sizeof(stem))
        return FALSE;
    strcpy(stem, vanilla_path);
    extension = strrchr(stem, '.');
    if (extension && _stricmp(extension, ".Vcr") == 0) *extension = '\0';
    if (!stem[0]) return FALSE;

    GetLocalTime(&now);
    for (collision = 0; collision < 100; ++collision) {
        int written;
        if (collision == 0) {
            written = snprintf(candidate, sizeof(candidate),
                "%s - RedFlag %04u%02u%02u-%02u%02u%02u",
                stem, now.wYear, now.wMonth, now.wDay,
                now.wHour, now.wMinute, now.wSecond);
        } else {
            written = snprintf(candidate, sizeof(candidate),
                "%s - RedFlag %04u%02u%02u-%02u%02u%02u-%d",
                stem, now.wYear, now.wMonth, now.wDay,
                now.wHour, now.wMinute, now.wSecond, collision + 1);
        }
        if (written < 0 || (size_t)written >= sizeof(candidate)) return FALSE;
        strcpy(g_replay_series_prefix, candidate);
        if (!replay_part_path(1, part1_path, sizeof(part1_path))) return FALSE;
        if (!replay_part_path(2, part2_path, sizeof(part2_path))) return FALSE;
        if (GetFileAttributesA(part1_path) == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesA(part2_path) == INVALID_FILE_ATTRIBUTES)
            break;
    }
    if (collision == 100) return FALSE;
    strcpy(g_replay_vanilla_path, vanilla_path);
    return TRUE;
}

static void __cdecl broker_replay_save_task(void* context) {
    replay_finalize_fn finalize =
        (replay_finalize_fn)(uintptr_t)REPLAY_FINALIZE_ORIGINAL;
    uintptr_t recorder;
    const char* output_path;
    (void)context;
    LONG ticket = (LONG)(uintptr_t)context;
    if (ticket <= 0 || InterlockedCompareExchange(
            &g_replay_save_pending, -ticket, ticket) != ticket) return;
    if (SESSION_TYPE != 7 || GAME_STATE == 4) {
        InterlockedExchange(&g_replay_save_result, -1);
        InterlockedExchange(&g_replay_save_pending, 0);
        return;
    }
    recorder = *(volatile uintptr_t*)REPLAY_RECORDER_PTR_GLOBAL;
    if (!recorder ||
        !runtime_region_supported(recorder, 3992, FALSE)) {
        InterlockedExchange(&g_replay_save_result, -2);
        InterlockedExchange(&g_replay_save_pending, 0);
        return;
    }
    output_path = *(const char**)(recorder + 1804);
    if (!output_path || IsBadStringPtrA(output_path, MAX_PATH - 1) ||
        !*output_path || strlen(output_path) >= sizeof(g_replay_vanilla_path)) {
        InterlockedExchange(&g_replay_save_result, -3);
        InterlockedExchange(&g_replay_save_pending, 0);
        return;
    }
    strcpy(g_replay_vanilla_path, output_path);
    InterlockedExchange(&g_replay_diag_handle,
                        (LONG)*(volatile uintptr_t*)(recorder + 1764));
    InterlockedExchange(&g_replay_diag_mode,
                        *(volatile LONG*)(recorder + 1792));
    g_replay_diag_start = *(volatile float*)(recorder + 3888);
    g_replay_diag_end = *(volatile float*)(recorder + 3892);
    MemoryBarrier();
    if (InterlockedCompareExchange(&g_replay_diag_handle, 0, 0) == 0) {
        InterlockedExchange(&g_replay_save_result, -4);
        InterlockedExchange(&g_replay_save_pending, 0);
        return;
    }
    if (InterlockedCompareExchange(&g_replay_diag_mode, 0, 0) != 1) {
        InterlockedExchange(&g_replay_save_result, -5);
        InterlockedExchange(&g_replay_save_pending, 0);
        return;
    }
    if (g_replay_diag_end <= g_replay_diag_start) {
        InterlockedExchange(&g_replay_save_result, -6);
        InterlockedExchange(&g_replay_save_pending, 0);
        return;
    }
    (void)finalize((void*)recorder, 1);
    InterlockedExchange(&g_replay_save_result, 1);
    InterlockedExchange(&g_replay_save_pending, 0);
}

static BOOL replay_move_saved_part(void) {
    char part_path[MAX_PATH];
    char line[640];
    DWORD error;
    if (!g_replay_series_prefix[0] &&
        !replay_begin_series(g_replay_vanilla_path)) return FALSE;
    if (!replay_part_path(g_replay_current_part, part_path, sizeof(part_path)))
        return FALSE;
    if (GetFileAttributesA(part_path) != INVALID_FILE_ATTRIBUTES) {
        snprintf(line, sizeof(line),
                 "REPLAY WARNING: refused to overwrite existing Part %ld at %s",
                 g_replay_current_part, part_path);
        line[sizeof(line) - 1] = '\0';
        log_w(line);
        return FALSE;
    }
    if (!MoveFileA(g_replay_vanilla_path, part_path)) {
        error = GetLastError();
        snprintf(line, sizeof(line),
                 "REPLAY WARNING: native Part %ld save could not be preserved (Win32 error %lu); race restart continued.",
                 g_replay_current_part, error);
        line[sizeof(line) - 1] = '\0';
        log_w(line);
        return FALSE;
    }
    snprintf(line, sizeof(line), "REPLAY: native writer preserved Part %ld as %s",
             g_replay_current_part, part_path);
    line[sizeof(line) - 1] = '\0';
    log_w(line);
    g_replay_series_active = TRUE;
    ++g_replay_current_part;
    return TRUE;
}

/* RF85: one nonblocking preparation step. TRUE means replay work is finished
 * (saved or explicitly skipped); FALSE means keep holding the field and poll.
 * No game-thread work is awaited inside the worker, and no recorder field is
 * forged. A queued timeout is advisory; it never releases race control. */
static BOOL replay_finalize_current_part(const char* reason) {
    DWORD now = GetTickCount();
    LONG pending, result;
    char line[320];
    if (g_replay_prepare_done) return TRUE;
    if (!g_rf_config.split_replay_on_restart) {
        g_replay_prepare_done = TRUE;
        return TRUE;
    }
    if (!g_replay_prepare_started) {
        g_replay_prepare_started = TRUE;
        g_replay_prepare_deadline = now + REPLAY_QUEUE_MAX_MS;
        log_w("RF-EE221688B5E");
    }
    pending = InterlockedCompareExchange(&g_replay_save_pending, 0, 0);
    if (pending < 0) return FALSE;
    if (pending > 0) {
        if ((LONG)(now - g_replay_prepare_deadline) < 0) return FALSE;
        if (InterlockedCompareExchange(&g_replay_save_pending, 0, pending) != pending)
            return FALSE; /* writer won the claim; never race its native call */
        log_w("RF-E0B4714CF5F");
        g_replay_ticket = 0;
        g_replay_prepare_done = TRUE;
        return TRUE;
    }
    if (g_replay_ticket) {
        result = InterlockedCompareExchange(&g_replay_save_result, 0, 0);
        g_replay_ticket = 0;
        if (result == 1) {
            (void)replay_move_saved_part();
            g_replay_prepare_done = TRUE;
            return TRUE;
        }
        /* Retry only native readiness gates, without forging any recorder field.
         * RF88: never hold the restart for the full queue deadline when the
         * recorder is simply not recording; the short bounded window above is
         * the whole allowance. In-flight or queued saves keep their own bounds. */
        if (result == -4 || result == -5 || result == -6) {
            DWORD readiness_start =
                g_replay_prepare_deadline - REPLAY_QUEUE_MAX_MS;
            if ((LONG)(now - readiness_start) < (LONG)REPLAY_READINESS_MAX_MS) {
                g_replay_retry_at = now + 25u;
                return FALSE;
            }
        }
        snprintf(line, sizeof(line),
            "REPLAY WARNING: native recorder stayed unsaveable (%ld, %s); restart preparation continues with field held.",
            result, reason ? reason : "unknown");
        log_w(line);
        g_replay_prepare_done = TRUE;
        return TRUE;
    }
    if (g_replay_retry_at && (LONG)(now - g_replay_retry_at) < 0) return FALSE;
    if (!g_broker_registered || !g_broker_post || g_replay_ticket_counter >= 0x3FFFFFFF) {
        log_w("RF-ED27DE4EFD7");
        g_replay_prepare_done = TRUE;
        return TRUE;
    }
    g_replay_ticket = ++g_replay_ticket_counter;
    InterlockedExchange(&g_replay_save_result, 0);
    InterlockedExchange(&g_replay_save_pending, g_replay_ticket);
    int post_result = g_broker_post(g_broker_replay_token, broker_replay_save_task,
                                  (void*)(uintptr_t)g_replay_ticket);
    if (post_result == RF_BROKER_STALE_TOKEN) {
        g_broker_replay_token = 0;
        post_result = g_broker_register(g_hinst, "red_flag_replay", &g_broker_replay_token);
        if (post_result == RF_BROKER_OK)
            post_result = g_broker_post(g_broker_replay_token, broker_replay_save_task,
                                       (void*)(uintptr_t)g_replay_ticket);
    }
    if (post_result != RF_BROKER_OK) {
        InterlockedCompareExchange(&g_replay_save_pending, 0, g_replay_ticket);
        snprintf(line, sizeof(line),
            "REPLAY WARNING: broker refused save (%d); restart preparation continues with field held.", post_result);
        log_w(line);
        g_replay_ticket = 0;
        g_replay_prepare_done = TRUE;
        return TRUE;
    }
    return FALSE;
}

/* Pause is a reversible wait, not permission to restart and not a lost race.
 * Log exact predicates on cancellation instead of blaming every case on exit. */
static int restart_preparation_context(uintptr_t expected_epoch, const char* stage) {
    int game=GAME_STATE, race=RACE_PHASE, session=SESSION_TYPE;
    int paused=PAUSE_FLAG!=0, out=PLAYER_OUT_FLAG;
    int cancelled=InterlockedCompareExchange(&g_restart_cancelled,0,0)!=0;
    int escape=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
    int player_ok=restart_player_supported();
    uintptr_t epoch=session_epoch();
    if (!restart_context_allowed(game,race,session,FALSE,out,cancelled,escape) ||
        !player_ok || !expected_epoch || epoch!=expected_epoch) {
        char line[320];
        snprintf(line,sizeof(line),
            "RESTART PREPARATION CANCELLED: stage=%s game=%d race=%d session=%d pause=%d playerOut=%d cancelled=%d escape=%d playerSupported=%d epoch=%08lX expected=%08lX replayPending=%ld",
            stage,game,race,session,paused,out,cancelled,escape,player_ok,
            (unsigned long)epoch,(unsigned long)expected_epoch,
            InterlockedCompareExchange(&g_replay_save_pending,0,0));
        log_w(line);
        return -1;
    }
    return paused ? 0 : 1;
}

static void replay_preserve_final_part_on_exit(void) {
    char part_path[MAX_PATH];
    char line[640];
    DWORD deadline;
    if (!g_replay_series_active || g_replay_finalized_at_results ||
        !g_replay_vanilla_path[0] ||
        !replay_part_path(g_replay_current_part, part_path, sizeof(part_path)))
        return;
    deadline = GetTickCount() + 1000u;
    do {
        if (GetFileAttributesA(part_path) == INVALID_FILE_ATTRIBUTES &&
            MoveFileA(g_replay_vanilla_path, part_path)) {
            snprintf(line, sizeof(line),
                     "REPLAY: session-exit writer preserved final Part %ld as %s",
                     g_replay_current_part, part_path);
            line[sizeof(line) - 1] = '\0';
            log_w(line);
            ++g_replay_current_part;
            g_replay_finalized_at_results = TRUE;
            return;
        }
        Sleep(25);
    } while ((LONG)(GetTickCount() - deadline) < 0);
    log_w("RF-ED03D84E6C5");
}

static void rf_set_log_path(void) {
    static const char name[] = "RedFlag.log";
    DWORD length = GetModuleFileNameA(g_hinst, g_log, ARRAYSIZE(g_log));
    char* slash;
    g_log[ARRAYSIZE(g_log) - 1] = '\0';
    if (!length || length >= ARRAYSIZE(g_log)) { g_log[0] = '\0'; return; }
    slash = strrchr(g_log, '\\');
    if (!slash || (size_t)(slash - g_log) + sizeof(name) >= ARRAYSIZE(g_log)) {
        g_log[0] = '\0';
        return;
    }
    memcpy(slash + 1, name, sizeof(name));
}

static void rf_log_begin(void) {
    FILE* f;
    SYSTEMTIME now;
    if (!g_log[0]) return;
    f = fopen(g_log, "w");
    if (!f) return;
    GetLocalTime(&now);
    fprintf(f, "F1 Challenge Red Flag bug report log\n");
    fprintf(f, "Started %04u-%02u-%02u %02u:%02u:%02u local time\n",
            now.wYear, now.wMonth, now.wDay,
            now.wHour, now.wMinute, now.wSecond);
    fprintf(f, "Loader ownership: LightLoader exclusivity ABI v2+, product/capability/generation/seal/proof required\n");
    fprintf(f, "Older LightLoader ABI v1 and ordinary DLL/ASI loaders are rejected; compatible future LightLoader versions must preserve the exclusivity contract\n");
    fclose(f);
}

static void log_w(const char* s) {
    rfu_tap(s);   /* 1.1.6: feed the consent-gated usage summary */
    if (!g_log[0]) return;
    FILE* f = fopen(g_log, "a");
    if (f) { fprintf(f, "[%lu] %s\n", GetTickCount(), s); fclose(f); }
}

/* ---- PRODUCTION BUILD (generated by make_release.py from gr44.c) ----
 * RedFlag.log diagnostics retained for end-user bug reports. */
#define RF_RELEASE 1
__attribute__((used))
volatile const char RF_SIGNATURE[] =
    "F1 Challenge RED FLAG mod - by Matias - "
    "please keep this credit if you redistribute";


static void rf_log_effective_config(void) {
    char line[640];
    DWORD attrs = g_rf_config_path[0]
        ? GetFileAttributesA(g_rf_config_path) : INVALID_FILE_ATTRIBUTES;
    snprintf(line, sizeof(line),
        "CONFIG source: path=%s present=%d; all following values are effective after safe defaulting/clamping.",
        g_rf_config_path[0] ? g_rf_config_path : "<unresolved>",
        attrs != INVALID_FILE_ATTRIBUTES ? 1 : 0);
    line[sizeof(line) - 1] = '\0';
    log_w(line);
    log_w((const char*)RF_MANUAL_ACCEPT_SIGNATURE);
    snprintf(line, sizeof(line),
        "CONFIG Activation: AutomaticEnabled=%d RequireNativeYellow=%d RequireManualRedFlagAcceptance=%d MinimumRaceDistancePercent=%d ManualEnabled=%d.",
        g_rf_config.automatic_enabled, g_rf_config.require_native_yellow,
        g_rf_config.require_manual_acceptance,
        g_rf_config.minimum_race_distance_percent, g_rf_config.manual_enabled);
    log_w(line);
    snprintf(line, sizeof(line),
        "CONFIG Timing: CooldownMs=%lu RestartCountdownMs=%lu OpeningWindowMs=%lu IncidentFreshnessMs=%lu YellowMs=%lu OpeningYellowMs=%lu WetYellowMs=%lu TerminalMs=%lu StationaryMs=%lu SlowDamageMs=%lu OpeningIncidentMs=%lu BlockedLineMs=%lu InvertedMs=%lu FinalMs=%lu WetFinalMs=%lu StrongWetFinalMs=%lu PreStopNoticeMs=%lu.",
        (unsigned long)g_rf_config.cooldown_ms,
        (unsigned long)g_rf_config.restart_countdown_ms,
        (unsigned long)g_rf_config.opening_window_ms,
        (unsigned long)g_rf_config.incident_window_ms,
        (unsigned long)g_rf_config.yellow_ms,
        (unsigned long)g_rf_config.opening_yellow_ms,
        (unsigned long)g_rf_config.wet_yellow_ms,
        (unsigned long)g_rf_config.terminal_confirm_ms,
        (unsigned long)g_rf_config.stationary_confirm_ms,
        (unsigned long)g_rf_config.damage_confirm_ms,
        (unsigned long)g_rf_config.opening_incident_confirm_ms,
        (unsigned long)g_rf_config.blocking_confirm_ms,
        (unsigned long)g_rf_config.inverted_confirm_ms,
        (unsigned long)g_rf_config.final_confirm_ms,
        (unsigned long)g_rf_config.wet_final_confirm_ms,
        (unsigned long)g_rf_config.strong_wet_final_confirm_ms,
        (unsigned long)g_rf_config.prestop_notice_ms);
    log_w(line);
    snprintf(line, sizeof(line),
        "CONFIG Geometry: ClusterM=%.3f OpeningClusterM=%.3f OnTrackPileupClusterM=%.3f StationaryMps=%.3f SlowDamageMps=%.3f RacingLineStationaryMps=%.3f TerminalLineJitterMps=%.3f RacingLineOverlapMarginM=%.3f InvertedUpDotMax=%.3f.",
        g_rf_config.cluster_m, g_rf_config.opening_cluster_m,
        g_rf_config.ontrack_pileup_cluster_m,
        g_rf_config.stationary_speed_mps,
        g_rf_config.slow_damage_speed_mps,
        g_rf_config.racing_line_stationary_speed_mps,
        g_rf_config.terminal_racing_line_jitter_speed_mps,
        g_rf_config.racing_line_overlap_margin_m,
        g_rf_config.inverted_up_dot_max);
    log_w(line);
    snprintf(line, sizeof(line),
        "CONFIG Weather: WetAssist=%.3f StrongWet=%.3f.",
        g_rf_config.wet_assist_threshold,
        g_rf_config.strong_wet_threshold);
    log_w(line);
    snprintf(line, sizeof(line),
        "CONFIG Patterns: BlockedLine=%d InvertedLine=%d MultiTerminal=%d/%d Mixed=%d/%d/%d StrongWet=%d/%d/%d MinimumCluster=%d/%d PlaceholderLog=%d.",
        g_rf_config.enable_blocked_corridor,
        g_rf_config.enable_inverted_blocker,
        g_rf_config.enable_multi_terminal, g_rf_config.multi_terminal_cars,
        g_rf_config.enable_mixed_blockage, g_rf_config.mixed_terminal_cars,
        g_rf_config.mixed_stationary_damaged_cars,
        g_rf_config.enable_strong_wet_pattern,
        g_rf_config.strong_wet_terminal_cars,
        g_rf_config.strong_wet_stationary_damaged_cars,
        g_rf_config.minimum_cluster_cars,
        g_rf_config.minimum_cluster_terminals,
        g_rf_config.log_passive_placeholder);
    log_w(line);
    snprintf(line, sizeof(line),
        "CONFIG Replay: SplitOnRedFlagRestart=%d.",
        g_rf_config.split_replay_on_restart);
    log_w(line);
    snprintf(line, sizeof(line),
        "CONFIG Restart: YRestartMode=%s RestartProcedure=%s (Countdown uses Timing.RestartCountdownSeconds; DriveToPits uses the former NUMPAD2 pit-return procedure).",
        g_rf_config.y_restart_mode == RF_Y_RESTART_FULL_RACE
            ? "FullRace" : "PostRedFlag",
        g_rf_config.restart_procedure == RF_RESTART_PROCEDURE_DRIVE_TO_PITS
            ? "DriveToPits" : "Countdown");
    log_w(line);
    log_w("RF-E4F7F7EF1F7");
}

#if RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR
static BOOL hud_position_prefix_matches(DWORD operand) {
    const BYTE* code = (const BYTE*)HUD_POSITION_COUNT_LOAD;
    if (!runtime_region_supported(HUD_POSITION_COUNT_LOAD,
                                  sizeof(g_hud_position_count_prefix), TRUE) ||
        code[0] != g_hud_position_count_prefix[0] ||
        memcmp(code + 5, g_hud_position_count_prefix + 5, 6) != 0)
        return FALSE;
    return *(const volatile DWORD*)HUD_POSITION_COUNT_OPERAND == operand;
}

static BOOL hud_position_write_operand(DWORD operand) {
    DWORD old_protect;
    DWORD ignored;
    BOOL protection_restored;
    if (!VirtualProtect((void*)HUD_POSITION_COUNT_OPERAND, sizeof(DWORD),
                        PAGE_EXECUTE_READWRITE, &old_protect))
        return FALSE;
    *(volatile DWORD*)HUD_POSITION_COUNT_OPERAND = operand;
    MemoryBarrier();
    protection_restored = VirtualProtect(
        (void*)HUD_POSITION_COUNT_OPERAND, sizeof(DWORD),
        old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(),
                          (const void*)HUD_POSITION_COUNT_LOAD,
                          sizeof(g_hud_position_count_prefix));
    return protection_restored && hud_position_prefix_matches(operand);
}

static void __cdecl broker_hud_position_patch_task(void* context) {
    LONG op;
    DWORD shadow_address = (DWORD)(uintptr_t)&g_hud_position_denominator;
    (void)context;
    if (InterlockedCompareExchange(&g_hud_patch_task_state, 2, 1) != 1)
        return;
    op = InterlockedCompareExchange(&g_hud_patch_task_op, 0, 0);
    if (op == 1) {
        if (g_hud_position_patch_installed) {
            InterlockedExchange(&g_hud_patch_task_result,
                hud_position_prefix_matches(shadow_address) ? 1 : -2);
        } else if (!hud_position_prefix_matches(HUD_POSITION_COUNT_ORIGINAL)) {
            InterlockedExchange(&g_hud_patch_task_result, -3);
        } else if (!hud_position_write_operand(shadow_address)) {
            BOOL restored = hud_position_write_operand(HUD_POSITION_COUNT_ORIGINAL);
            g_hud_position_patch_installed =
                !restored && hud_position_prefix_matches(shadow_address);
            if (!restored &&
                !hud_position_prefix_matches(HUD_POSITION_COUNT_ORIGINAL))
                g_hud_patch_unload_safe = FALSE;
            InterlockedExchange(&g_hud_patch_task_result, -4);
        } else {
            g_hud_position_patch_installed = TRUE;
            InterlockedExchange(&g_hud_patch_task_result, 1);
        }
    } else if (op == 2) {
        if (hud_position_prefix_matches(HUD_POSITION_COUNT_ORIGINAL)) {
            g_hud_position_patch_installed = FALSE;
            InterlockedExchange(&g_hud_patch_task_result, 1);
        } else if (!hud_position_prefix_matches(shadow_address)) {
            InterlockedExchange(&g_hud_patch_task_result, -5);
        } else if (!hud_position_write_operand(HUD_POSITION_COUNT_ORIGINAL)) {
            InterlockedExchange(&g_hud_patch_task_result, -6);
        } else {
            g_hud_position_patch_installed = FALSE;
            InterlockedExchange(&g_hud_patch_task_result, 1);
        }
    } else {
        InterlockedExchange(&g_hud_patch_task_result, -1);
    }
    MemoryBarrier();
    InterlockedExchange(&g_hud_patch_task_state, 3);
}

static BOOL hud_position_patch_sync(LONG op) {
    DWORD unclaimed_deadline;
    LONG state;
    LONG result;
    int post_result;
    if (!g_broker_registered || !g_broker_post || !g_broker_control_token)
        return FALSE;
    if (InterlockedCompareExchange(&g_hud_patch_task_state, 1, 0) != 0)
        return FALSE;
    InterlockedExchange(&g_hud_patch_task_op, op);
    InterlockedExchange(&g_hud_patch_task_result, 0);
    MemoryBarrier();
    post_result = g_broker_post(g_broker_control_token,
                                broker_hud_position_patch_task, NULL);
    if (post_result != RF_BROKER_OK) {
        InterlockedExchange(&g_hud_patch_task_state, 0);
        return FALSE;
    }
    unclaimed_deadline = GetTickCount() + 2000u;
    for (;;) {
        state = InterlockedCompareExchange(&g_hud_patch_task_state, 0, 0);
        if (state == 3) break;
        if (state == 1 && (LONG)(GetTickCount() - unclaimed_deadline) >= 0) {
            if (InterlockedCompareExchange(&g_hud_patch_task_state, 0, 1) == 1)
                return FALSE;
        }
        /* Claimed state 2 owns a bounded game-thread write. Never unload
         * underneath it, even after the unclaimed deadline. */
        Sleep(1);
    }
    result = InterlockedCompareExchange(&g_hud_patch_task_result, 0, 0);
    InterlockedExchange(&g_hud_patch_task_state, 0);
    return result == 1;
}

static void hud_position_denominator_mirror_live(void) {
    if (!g_hud_position_denominator_frozen) {
        int count = VEHICLE_COUNT;
        if (count >= 0)
            InterlockedExchange(&g_hud_position_denominator, count);
    }
}

static void hud_position_denominator_freeze(int original_count) {
    if (original_count > 0) {
        InterlockedExchange(&g_hud_position_denominator, original_count);
        g_hud_position_denominator_frozen = TRUE;
    }
}

static void hud_position_denominator_release(void) {
    g_hud_position_denominator_frozen = FALSE;
    hud_position_denominator_mirror_live();
}
#else
#define hud_position_denominator_mirror_live() ((void)0)
#define hud_position_denominator_freeze(count) ((void)(count))
#define hud_position_denominator_release() ((void)0)
#endif

#include "rf_penalty_hold.h"
static BOOL capture_restart_fuel_service(void);
static void capture_restart_pit_strategy(void);
static void refresh_restart_pit_strategy(void);

static RfBrokerRestartDecision __cdecl rf_broker_restart_guard(
    void* context, DWORD session_generation, int race_phase) {
    DWORD unclaimed_deadline;
    LONG state;
    LONG result;
    (void)context;
    (void)session_generation;

    /* Red Flag's own restart already performed the roster transaction before
     * arming the byte. Consume this one-shot marker so FullRace mode cannot
     * mistake an owned restart for the player's later Y command. */
    /* A paused owned request remains reserved. The worker re-arms its native
       request byte on resume; no callback cancels or consumes it while paused. */
    if(InterlockedCompareExchange(&g_owned_restart_gate_pending,0,0)==1 && PAUSE_FLAG &&
       SESSION_TYPE==7 && (GAME_STATE==5 || GAME_STATE==7) && (RACE_PHASE==3 || RACE_PHASE==5))
        return RF_BROKER_RESTART_BLOCK;
    if (InterlockedCompareExchange(&g_owned_restart_gate_pending, 0, 1) == 1) {
        /* Last authority is the existing game-thread gate, after any replay
         * finalization/worker delay. Never release a request into menus. */
        if (!restart_context_live() || !capture_restart_fuel_service() || !rf_penalty_seal()) {
            InterlockedExchange(&g_restart_cancelled, 1);
            InterlockedExchange(&g_restart_commit_blocked, 1);
            log_w("RF-E55C6493E92");
            return RF_BROKER_RESTART_BLOCK;
        }
        /* Last read of the live field before native Game_Restart rebuilds
         * every survivor's strategy. Only genuine race progress is folded in;
         * plans RF itself rewrote while parking cars are positively held. The
         * restore runs later on this same game thread. */
        refresh_restart_pit_strategy();
        return RF_BROKER_RESTART_ALLOW;
    }

    if(g_rc && f1c_rc_read(&g_rc->rf_request)!=0)return RF_BROKER_RESTART_BLOCK;
    if(g_rc && (g_rf_config.y_restart_mode==RF_Y_RESTART_FULL_RACE ||
       InterlockedCompareExchange(&g_y_restart_guard_armed,0,0))) {
        if(!rf_rc_acquire()) {
            /* Never wait for a game-thread SC callback inside this guard. */
            f1c_rc_request(g_rc,0);
            log_w("RF-ED22C2367BD");
            return RF_BROKER_RESTART_BLOCK;
        }
    }

    /* FullRace is intentionally vanilla: allow the restart with the complete
     * roster/live roles so F1C resets laps and revives retirees. The worker
     * separately forgets Red Flag's prior-session bookkeeping. */
    if (g_rf_config.y_restart_mode == RF_Y_RESTART_FULL_RACE) {
        if (!initial_grid_restore()) return RF_BROKER_RESTART_BLOCK;
        InterlockedExchange(&g_y_restart_guard_armed, 0);
        InterlockedExchange(&g_full_y_reset_pending, 1);
        return RF_BROKER_RESTART_ALLOW;
    }

    /* The callback becomes active after a completed Red Flag restart,
     * including a manual stop with no DNFs. Pre-Red-Flag Y stays vanilla. */
    if (InterlockedCompareExchange(&g_y_restart_guard_armed, 0, 1) != 1)
        return RF_BROKER_RESTART_ALLOW;
    if (!g_running || !g_session_active || race_phase != 5 ||
        !g_checkpoint_valid || (g_roster_count > 0 && !g_roster_results_restored)) {
        InterlockedExchange(&g_y_restart_guard_armed, 1);
        return RF_BROKER_RESTART_BLOCK;
    }
    if (InterlockedCompareExchange(&g_y_restart_prepare_state, 1, 0) != 0) {
        InterlockedExchange(&g_y_restart_guard_armed, 1);
        return RF_BROKER_RESTART_BLOCK;
    }
    InterlockedExchange(&g_y_restart_prepare_result, 0);
    MemoryBarrier();

    /* The phase-0 worker normally claims within its 50 ms idle cadence. If it
     * is not alive, cancel before any roster write can begin. Once claimed,
     * wait for the bounded in-process transaction to finish: returning while
     * the worker is changing VEHICLE_ARRAY would be unsafe. */
    unclaimed_deadline = GetTickCount() + 1000u;
    for (;;) {
        state = InterlockedCompareExchange(&g_y_restart_prepare_state, 0, 0);
        if (state == 3) break;
        if (state == 1 && (LONG)(GetTickCount() - unclaimed_deadline) >= 0) {
            if (InterlockedCompareExchange(&g_y_restart_prepare_state, 0, 1) == 1) {
                InterlockedExchange(&g_y_restart_guard_armed, 1);
                return RF_BROKER_RESTART_BLOCK;
            }
        }
        Sleep(1);
    }
    result = InterlockedCompareExchange(&g_y_restart_prepare_result, 0, 0);
    InterlockedExchange(&g_y_restart_prepare_state, 0);
    if (result == 1) return RF_BROKER_RESTART_ALLOW;
    if (g_running && g_session_active && g_checkpoint_valid)
        InterlockedExchange(&g_y_restart_guard_armed, 1);
    return RF_BROKER_RESTART_BLOCK;
}

/* 1.1.6: game-thread broker session-end callback. Sets a flag only; the RF
 * worker loop (thread_main -> rfu_pump) performs the file write. The game
 * thread must never do the I/O (a flush here could stall the race thread). */
static void __cdecl rf_session_end(void *unused) {
    (void)unused;
    rfu_request();
}

/* 1.1.5: the broker is found the SDK way, as the one loaded module exporting
 * F1CBrokerGetInfo: the stand-alone f1c_game_update_broker.dll under
 * Lightloader 1.x, or dinput8.dll itself under Lightloader 2.0, which has the
 * broker built in. More than one broker-shaped module is ambiguous: none. */
static HMODULE rf_find_broker_module(void) {
    HANDLE snapshot;
    MODULEENTRY32 entry;
    HMODULE found = NULL;
    int count = 0;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return NULL;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Module32First(snapshot, &entry)) {
        do {
            if (GetProcAddress(entry.hModule, "F1CBrokerGetInfo")) {
                found = entry.hModule;
                ++count;
            }
        } while (Module32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count == 1 ? found : NULL;
}

static BOOL rf_broker_bind(void) {
    RfBrokerInfo info;
    int result;
    int attempt;
    if (g_broker_registered) return TRUE;
    g_broker_module = rf_find_broker_module();
    if (!g_broker_module) {
        log_w("RF-E2696AE2BB8");
        return FALSE;
    }
    g_broker_register = (RfBrokerRegisterFn)GetProcAddress(
        g_broker_module, "F1CBrokerRegister");
    g_broker_unregister = (RfBrokerUnregisterFn)GetProcAddress(
        g_broker_module, "F1CBrokerUnregister");
    g_broker_post = (RfBrokerPostFn)GetProcAddress(
        g_broker_module, "F1CBrokerPost");
    g_broker_get_info = (RfBrokerGetInfoFn)GetProcAddress(
        g_broker_module, "F1CBrokerGetInfo");
    g_broker_set_restart_guard = (RfBrokerSetRestartGuardFn)GetProcAddress(
        g_broker_module, "F1CBrokerSetRestartGuard");
    g_broker_set_session_end = (RfBrokerSetSessionEndFn)GetProcAddress(
        g_broker_module, "F1CBrokerSetSessionEnd");
    g_broker_penalty_begin=(F1CBrokerPenaltyBeginFn)GetProcAddress(g_broker_module,"F1CBrokerPenaltyBegin");
    g_broker_penalty_read=(F1CBrokerPenaltyReadFn)GetProcAddress(g_broker_module,"F1CBrokerPenaltyRead");
    g_broker_penalty_end=(F1CBrokerPenaltyEndFn)GetProcAddress(g_broker_module,"F1CBrokerPenaltyEnd");
    g_broker_penalty_seal=(F1CBrokerPenaltySealFn)GetProcAddress(g_broker_module,"F1CBrokerPenaltySeal");
    if (!g_broker_register || !g_broker_unregister || !g_broker_post ||
        !g_broker_get_info || !g_broker_set_restart_guard ||
        !g_broker_penalty_begin || !g_broker_penalty_read || !g_broker_penalty_end || !g_broker_penalty_seal) {
        log_w("RF-EA0F1776742");
        return FALSE;
    }
    ZeroMemory(&info, sizeof(info));
    info.structure_size = sizeof(info);
    result = -1;
    for (attempt = 0; attempt < 200; ++attempt) {
        result = g_broker_get_info(&info);
        if (result == RF_BROKER_OK) break;
        Sleep(10);
    }
    if (result != RF_BROKER_OK || !(info.capabilities & F1C_BROKER_CAP_PENALTY_HOLD) ||
        info.abi_version < RF_BROKER_MIN_ABI ||
        (info.capabilities & (RF_BROKER_CAP_LIFECYCLE |
                              RF_BROKER_CAP_RESTART_GUARD)) !=
            (RF_BROKER_CAP_LIFECYCLE | RF_BROKER_CAP_RESTART_GUARD)) {
        log_w("RF-E9125F07D2A");
        return FALSE;
    }
    result = -1;
    for (attempt = 0; attempt < 200; ++attempt) {
        result = g_broker_register(
            g_hinst, "red_flag_control", &g_broker_control_token);
        if (result == RF_BROKER_OK) break;
        Sleep(10);
    }
    if (result != RF_BROKER_OK || !g_broker_control_token) {
        log_w("RF-EC9E9698CC2");
        g_broker_control_token = 0;
        return FALSE;
    }
    result = -1;
    for (attempt = 0; attempt < 200; ++attempt) {
        result = g_broker_register(
            g_hinst, "red_flag_replay", &g_broker_replay_token);
        if (result == RF_BROKER_OK) break;
        Sleep(10);
    }
    if (result != RF_BROKER_OK || !g_broker_replay_token) {
        log_w("RF-E262EBEDBA3");
        (void)g_broker_unregister(g_broker_control_token, 2000u);
        g_broker_control_token = 0;
        g_broker_replay_token = 0;
        return FALSE;
    }
    result = -1;
    for (attempt = 0; attempt < 200; ++attempt) {
        result = g_broker_register(
            g_hinst, "red_flag_cleanup", &g_broker_cleanup_token);
        if (result == RF_BROKER_OK) break;
        Sleep(10);
    }
    if (result != RF_BROKER_OK || !g_broker_cleanup_token) {
        log_w("RF-EADB01D48B5");
        (void)g_broker_unregister(g_broker_replay_token, 2000u);
        (void)g_broker_unregister(g_broker_control_token, 2000u);
        g_broker_control_token = 0;
        g_broker_replay_token = 0;
        g_broker_cleanup_token = 0;
        return FALSE;
    }
    result = g_broker_set_restart_guard(
        g_broker_control_token, rf_broker_restart_guard, NULL);
    if (result != RF_BROKER_OK) {
        log_w("RF-EE22F1EA481");
        (void)g_broker_unregister(g_broker_cleanup_token, 2000u);
        (void)g_broker_unregister(g_broker_replay_token, 2000u);
        (void)g_broker_unregister(g_broker_control_token, 2000u);
        g_broker_control_token = 0;
        g_broker_replay_token = 0;
        g_broker_cleanup_token = 0;
        return FALSE;
    }
    g_broker_registered = TRUE;
    log_w("RF-E98DA8024BC");
    /* 1.1.6: optional ABI15 session-end hook writes the usage summary after the
     * race, outside DllMain. Absence only disables the session-end flush. */
    if (g_broker_set_session_end &&
        (info.capabilities & RF_BROKER_CAP_SESSION_END) &&
        g_broker_set_session_end(g_broker_control_token, rf_session_end, NULL) == RF_BROKER_OK) {
        g_broker_session_end_registered = TRUE;
    } else {
        log_w("RF-E7C38CE3608");
    }
    return TRUE;
}

/* Protected plug-in initialization can run before the broker's directory
 * entry is enumerated. Wait on the worker thread so filesystem order never
 * decides whether Red Flag starts. rf_broker_bind then performs its own
 * bounded readiness, ABI and capability negotiation. */
static BOOL rf_broker_bind_after_dependency_wait(void) {
    DWORD started = GetTickCount();
    while (g_running &&
           GetTickCount() - started < RF_BROKER_DEPENDENCY_WAIT_MS) {
        if (rf_find_broker_module()) {
            return rf_broker_bind();
        }
        Sleep(10);
    }
    log_w("RF-EA0F146578B");
    return FALSE;
}

static void rf_broker_unbind(void) {
    while(g_penalty_hold_active && !rf_penalty_end())Sleep(1);
    InterlockedExchange(&g_green_restore_armed, 0);
    InterlockedExchange(&g_post_green_continuity_armed, 0);
    InterlockedExchange(&g_post_green_continuity_pending, 0);
    InterlockedExchange(&g_post_green_continuity_result, 0);
    InterlockedExchange(&g_y_restart_guard_armed, 0);
    if (g_broker_registered && g_broker_set_restart_guard &&
        g_broker_control_token)
        (void)g_broker_set_restart_guard(g_broker_control_token, NULL, NULL);
    if (g_broker_registered && g_broker_session_end_registered &&
        g_broker_set_session_end && g_broker_control_token)
        (void)g_broker_set_session_end(g_broker_control_token, NULL, NULL);
    g_broker_session_end_registered = FALSE;
    g_broker_set_session_end = NULL;
    if (g_broker_registered && g_broker_unregister && g_broker_replay_token)
        (void)g_broker_unregister(g_broker_replay_token, 2000u);
    if (g_broker_registered && g_broker_unregister && g_broker_cleanup_token)
        (void)g_broker_unregister(g_broker_cleanup_token, 2000u);
    if (g_broker_registered && g_broker_unregister && g_broker_control_token)
        (void)g_broker_unregister(g_broker_control_token, 2000u);
    g_broker_registered = FALSE;
    g_broker_control_token = 0;
    g_broker_replay_token = 0;
    g_broker_cleanup_token = 0;
    InterlockedExchange(&g_broker_green_task_pending, 0);
    InterlockedExchange(&g_y_restart_prepare_state, 0);
    InterlockedExchange(&g_y_restart_prepare_result, 0);
    InterlockedExchange(&g_owned_restart_gate_pending, 0);
    InterlockedExchange(&g_full_y_reset_pending, 0);
}

/* DNF "grid-tail" QualPos marker.
 *
 * MUST stay inside the game's grid-position table. The restart placement
 * sub_403CD0 indexes v4[7*(QualPos-1)] UNCHECKED; the historic 200 read
 * 1393 floats past the table and AV'd the game (0xc0000005, fault offset
 * 0x403CE9, crash report 2026-08-17 17:04, F1 Challenge Online.exe). The
 * offline build survived only because slack memory happened to be mapped.
 *
 * The game itself assigns QualPos 1..N to an N-car field, so the table holds
 * >= N entries. Capping the DNF marker at VEHICLE_COUNT (the field size) is
 * always in bounds, still sorts DNF cars behind every active car (active
 * cars take 1..g_active_count < VEHICLE_COUNT), and the anti-reactivation
 * hooks + re_retire_dnf() remain the real exclusion mechanism. */
#define DNF_QUALPOS  VEHICLE_COUNT

/* ======================================================================== *
 *  PWS_RED01 - THE RED FLAG'S OWN VOICE LINE
 *
 *  DRIVER REPORT 2026-08-02: NUMPAD2 played NOTHING.
 *
 *  The eid was never the problem. eid 131 reads priority 70, cooldown 1.0,
 *  score 129 = PWS_RED01 - confirmed against a live table read, and it is not
 *  blocked. TWO OTHER THINGS WERE WRONG, AND THE FIRST ONE ALONE EXPLAINS
 *  THE SILENCE:
 *
 *  1. THE MOD MUTED ITSELF IMMEDIATELY AFTER FIRING. The trigger injected the
 *     line and then the very next loop iteration - 50 ms later on the NUMPAD2
 *     path, 500 ms on NUMPAD1 - called commentary_suppress(), which zeroes
 *     CONTROLLER+0xAF0. PWS_RED01 is seconds long, so on NUMPAD2 it never got
 *     to start. The suppression itself is wanted (the game would otherwise
 *     commentate the whole field retiring), just not before our own line has
 *     been said. say_busy() below holds it off.
 *
 *  2. THE INJECTION WAS A RAW QUEUE-HEAD WRITE. It stamped CONTROLLER+0x864
 *     directly instead of going through the game's own emitter path, and it
 *     ignored the audio channel entirely. Firing into a busy channel is what
 *     produces the CARRIER's clip instead of the requested one - the trap
 *     that produced SLOWCAR01 in the safety-car work earlier today. Replaced
 *     with the proven three-call path used by dll/retire_commentary.c and
 *     pace_probe.c: FN_STRUCT_INIT -> FN_DRIVER_LOOKUP -> FN_INSERT_ENTRY,
 *     fired only into a free channel, retried while it is busy.
 *
 *  CONFIRMATION IS EVENT_TS, NEVER THE QUEUE HEAD. The queue drains within
 *  one poll, so reading the head back proves nothing; the per-event timestamp
 *  going non-zero is the signal the clip was actually consumed. It is zeroed
 *  before the insert so the previous line's stamp cannot be mistaken for
 *  this one's.
 * ======================================================================== */
#define EV_TS(eid)      (*(volatile float*)(CONTROLLER + 0xAF8u + (eid) * 4u))
#define AUDIO_BUSY      (*(volatile int*)(CONTROLLER + 0xAE4u))

typedef void (__thiscall *StructInitFn)(void* buf);
typedef void (__thiscall *DriverLookupFn)(void* buf, int eid, int a3, int a4,
                                          int a5, int a6, float a7, float a8);
typedef char (__thiscall *InsertEntryFn)(void* controller, void* buf);
#define FN_STRUCT_INIT   ((StructInitFn)0x004144D0)
#define FN_DRIVER_LOOKUP ((DriverLookupFn)0x004144F0)
#define FN_INSERT_ENTRY  ((InsertEntryFn)0x00418560)

#define SAY_MAX_WAIT_MS  10000u   /* give up protecting the channel after this */
#define SAY_RETRY_MS     400u     /* how often to re-check a busy channel      */
#define SAY_TAIL_MS      1200u    /* let the clip finish before muting         */

static int   g_say_eid    = -1;    /* line in flight, -1 = none      */
static DWORD g_say_since  = 0;     /* when it was requested          */
static DWORD g_say_try    = 0;     /* last insert attempt            */
static BOOL  g_say_sent   = FALSE; /* an entry has been inserted     */
static BOOL  g_say_played = FALSE; /* EVENT_TS went non-zero         */
static DWORD g_say_played_ms = 0;

/* Request a line. Nothing is inserted here if the channel is busy - say_pump()
 * retries. Only ONE line is ever in flight; this mod says exactly one. */
static void inject_commentary(int eid) {
    g_say_eid    = eid;
    g_say_since  = GetTickCount();
    g_say_try    = 0;
    g_say_sent   = FALSE;
    g_say_played = FALSE;
    EV_TS(eid)   = 0.0f;    /* clear BEFORE any insert - see header */
}

/* Is our own line still owed the audio channel? While this is true the mod
 * must not mute commentary. */
static BOOL say_busy(void) {
    if (g_say_eid < 0) return FALSE;
    if (GetTickCount() - g_say_since > SAY_MAX_WAIT_MS) return FALSE;
    if (!g_say_played) return TRUE;
    if (GetTickCount() - g_say_played_ms < SAY_TAIL_MS) return TRUE;
    return (AUDIO_BUSY != 0);
}

static void say_pump(void) {
    DWORD now;
    if (g_say_eid < 0) return;
    now = GetTickCount();

    if (now - g_say_since > SAY_MAX_WAIT_MS) {
        char buf[128];
        wsprintfA(buf, "RF-E292D9AC83E %d %u %d %d",
                  g_say_eid, SAY_MAX_WAIT_MS, (int)g_say_sent, (int)g_say_played);
        log_w(buf);
        g_say_eid = -1;
        return;
    }

    if (!g_say_played && EV_TS(g_say_eid) > 0.0f) {
        g_say_played    = TRUE;
        g_say_played_ms = now;
        char buf[96];
        wsprintfA(buf, "RF-E130603CB8C %d %d",
                  g_say_eid, (int)(now - g_say_since));
        log_w(buf);
        return;
    }
    if (g_say_played) return;

    /* ONE insert per request, no re-inserts after it. The old code kept
     * inserting every SAY_RETRY_MS while EVENT_TS had not stamped; a long
     * clip ends, AUDIO_BUSY drops to 0, EVENT_TS is still pending, and a
     * fresh copy lands in the queue -> the line loops. A retry is only for
     * the pre-first-success busy-channel case (insert never happened); once
     * g_say_sent is set the clip is out and the EVENT_TS wait above decides. */
    if (g_say_sent) return;
    if (AUDIO_BUSY != 0) return;
    if (g_say_try && now - g_say_try < SAY_RETRY_MS) return;

    {
        float buf[8];
        g_say_try = now;
        EV_TS(g_say_eid) = 0.0f;
        FN_STRUCT_INIT(buf);
        FN_DRIVER_LOOKUP(buf, g_say_eid, -1, -1, -1, -1, -2.0f, -2.0f);
        FN_INSERT_ENTRY((void*)CONTROLLER, buf);
        if (!g_say_sent) {
            char lb[96];
            g_say_sent = TRUE;
            wsprintfA(lb, "RF-EEB68079212 %d", g_say_eid);
            log_w(lb);
        }
    }
}

static BOOL build_trampoline(void) {
    g_trampoline = (BYTE*)VirtualAlloc(NULL, 64,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampoline) return FALSE;

    BYTE* p = g_trampoline;
    *p++ = 0x50;
    *p++ = 0x52;
    *p++ = 0xB8; *(DWORD*)p = 0; p += 4;
    *p++ = 0x3B; *p++ = 0x05;
    *(DWORD*)p = (DWORD)(uintptr_t)&g_dnf_count; p += 4;
    *p++ = 0x7D; *p++ = 0x13;
    *p++ = 0x8B; *p++ = 0x14; *p++ = 0x85;
    *(DWORD*)p = (DWORD)(uintptr_t)&g_dnf_ptrs; p += 4;
    *p++ = 0x39; *p++ = 0xD1;
    *p++ = 0x74; *p++ = 0x03;
    *p++ = 0x40;
    *p++ = 0xEB;
    *(char*)p = (char)(7 - (int)(p - g_trampoline + 1)); p++;
    *p++ = 0x5A;
    *p++ = 0x58;
    *p++ = 0xC2; *(WORD*)p = 4; p += 2;
    *p++ = 0x5A;
    *p++ = 0x58;
    *p++ = 0xB8; *(DWORD*)p = SCENE_RENDER_UPDATE; p += 4;
    *p++ = 0xFF; *p++ = 0xE0;

    return TRUE;
}

static BOOL build_trampoline2(void) {
    g_trampoline2 = (BYTE*)VirtualAlloc(NULL, 64,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampoline2) return FALSE;

    BYTE* p = g_trampoline2;
    *p++ = 0x80; *p++ = 0xB9;
    *(DWORD*)p = OFF_RETIREDFLAG; p += 4;
    *p++ = 0x00;
    *p++ = 0x75; BYTE* jne_retired = p++;
#if !RF_DIAGNOSTIC_OBSERVE_NATIVE_PLAYER_PLACEMENT
    /* A native DQ can have FinishState=3 while RetiredFlag remains zero.
     * Protect the exact captured player from SessionTickUpdate without
     * fabricating RetiredFlag=1 or role=-1. This hook is installed before the
     * restart flag is armed. */
    *p++ = 0x83; *p++ = 0x3D;
    *(DWORD*)p = (DWORD)(uintptr_t)&g_player_out_active; p += 4;
    *p++ = 0x00;
    *p++ = 0x74; BYTE* je_disabled = p++;
    *p++ = 0x3B; *p++ = 0x0D;
    *(DWORD*)p = (DWORD)(uintptr_t)&g_player_out_ptr; p += 4;
    *p++ = 0x74; BYTE* je_player = p++;
#endif
    BYTE* original = p;
    *p++ = 0xB8; *(DWORD*)p = SESSION_TICK_UPDATE; p += 4;
    *p++ = 0xFF; *p++ = 0xE0;
    BYTE* skip = p;
    *p++ = 0xC2; *(WORD*)p = 4; p += 2;
    *jne_retired = (BYTE)(skip - (jne_retired + 1));
#if !RF_DIAGNOSTIC_OBSERVE_NATIVE_PLAYER_PLACEMENT
    *je_disabled = (BYTE)(original - (je_disabled + 1));
    *je_player = (BYTE)(skip - (je_player + 1));
#endif

    return TRUE;
}

static BOOL install_hook(void) {
    if (g_hook_installed) return TRUE;
    if (!call_site_targets(CALL_SITE, SCENE_RENDER_UPDATE)) return FALSE;
    if (!g_trampoline && !build_trampoline()) return FALSE;
    DWORD old;
    memcpy(g_orig_bytes, (void*)CALL_SITE, CALL_SIZE);
    if (!VirtualProtect((void*)CALL_SITE, CALL_SIZE, PAGE_EXECUTE_READWRITE, &old)) return FALSE;
    BYTE patch[CALL_SIZE];
    patch[0] = 0xE8;
    DWORD rel = (DWORD)((uintptr_t)g_trampoline - (CALL_SITE + 5));
    memcpy(patch + 1, &rel, 4);
    memcpy((void*)CALL_SITE, patch, CALL_SIZE);
    VirtualProtect((void*)CALL_SITE, CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)CALL_SITE, CALL_SIZE);
    g_hook_installed = TRUE;
    return TRUE;
}

static void remove_hook(void) {
    if (!g_hook_installed) return;
    if (!call_site_targets(CALL_SITE, (DWORD)(uintptr_t)g_trampoline)) {
        g_hook_installed = FALSE;
        return;
    }
    DWORD old;
    if (!VirtualProtect((void*)CALL_SITE, CALL_SIZE,
                        PAGE_EXECUTE_READWRITE, &old)) return;
    memcpy((void*)CALL_SITE, g_orig_bytes, CALL_SIZE);
    VirtualProtect((void*)CALL_SITE, CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)CALL_SITE, CALL_SIZE);
    g_hook_installed = FALSE;
}

static BOOL install_hook2(void) {
#if RF_DIAGNOSTIC_OBSERVE_NATIVE_PLAYER_PLACEMENT
    /* Diagnostic contract: observe F1C's native player lifecycle without
     * suppressing SessionTickUpdate for any player state. AI DNFs are already
     * role-quarantined and absent from the reduced counted roster. */
    return TRUE;
#endif
    if (g_hook2_installed) return TRUE;
    if (!call_site_targets(TICK_CALL_SITE, SESSION_TICK_UPDATE)) return FALSE;
    if (!g_trampoline2 && !build_trampoline2()) return FALSE;
    DWORD old;
    memcpy(g_orig_bytes2, (void*)TICK_CALL_SITE, CALL_SIZE);
    if (!VirtualProtect((void*)TICK_CALL_SITE, CALL_SIZE, PAGE_EXECUTE_READWRITE, &old)) return FALSE;
    BYTE patch[CALL_SIZE];
    patch[0] = 0xE8;
    DWORD rel = (DWORD)((uintptr_t)g_trampoline2 - (TICK_CALL_SITE + 5));
    memcpy(patch + 1, &rel, 4);
    memcpy((void*)TICK_CALL_SITE, patch, CALL_SIZE);
    VirtualProtect((void*)TICK_CALL_SITE, CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)TICK_CALL_SITE, CALL_SIZE);
    g_hook2_installed = TRUE;
    return TRUE;
}

static void remove_hook2(void) {
    if (!g_hook2_installed) return;
    if (!call_site_targets(TICK_CALL_SITE, (DWORD)(uintptr_t)g_trampoline2)) {
        g_hook2_installed = FALSE;
        return;
    }
    DWORD old;
    if (!VirtualProtect((void*)TICK_CALL_SITE, CALL_SIZE,
                        PAGE_EXECUTE_READWRITE, &old)) return;
    memcpy((void*)TICK_CALL_SITE, g_orig_bytes2, CALL_SIZE);
    VirtualProtect((void*)TICK_CALL_SITE, CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)TICK_CALL_SITE, CALL_SIZE);
    g_hook2_installed = FALSE;
}

/* 0x4D10E0 begins with exactly:
 *   mov edx,[esp+4]          8B 54 24 04
 *   mov eax,[edx+0x4058]    8B 82 58 40 00 00
 * Refuse any different bytes: another plugin or executable revision owns the
 * site. The trampoline replays those ten bytes and jumps to 0x4D10EA. */
static const BYTE g_player_grid_place_signature[PLAYER_GRID_PLACE_STEAL] = {
    0x8B, 0x54, 0x24, 0x04, 0x8B, 0x82, 0x58, 0x40, 0x00, 0x00
};

static int __stdcall player_grid_place_detour(int vehicle, void* a2, void* a3) {
    int skip;
    int result;
    InterlockedIncrement(&g_player_grid_gate_inflight);
    skip =
#if RF_DIAGNOSTIC_PLAYER_GRID_GATE
        InterlockedCompareExchange(&g_player_grid_gate_armed, 0, 0) == 1 &&
        SESSION_TYPE == 7 &&
        g_player_out_committed &&
        g_player_grid_gate_epoch != 0 &&
        g_player_grid_gate_epoch == g_roster_reduced_epoch &&
        g_player_out_epoch == g_session_epoch_id &&
        g_player_out_ptr != NULL &&
        (uintptr_t)vehicle == (uintptr_t)g_player_out_ptr &&
        (uintptr_t)vehicle == PLAYER_VEH_PTR;
#else
        0;
#endif
    if (skip) {
        InterlockedDecrement(&g_player_grid_gate_inflight);
        return 0;
    }
    result = ((int (__stdcall*)(int, void*, void*))
              g_player_grid_place_trampoline)(vehicle, a2, a3);
    InterlockedDecrement(&g_player_grid_gate_inflight);
    return result;
}

static BOOL install_player_grid_gate(void) {
#if !RF_DIAGNOSTIC_PLAYER_GRID_GATE
    return TRUE;
#else
    BYTE patch[PLAYER_GRID_PLACE_STEAL];
    DWORD old;
    DWORD rel;
    BYTE* p;
    if (g_player_grid_place_installed) return TRUE;
    if (memcmp((const void*)PLAYER_GRID_PLACE_ENTRY,
               g_player_grid_place_signature,
               PLAYER_GRID_PLACE_STEAL) != 0) return FALSE;
    if (!g_player_grid_place_trampoline) {
        g_player_grid_place_trampoline = (BYTE*)VirtualAlloc(
            NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_player_grid_place_trampoline) return FALSE;
        p = g_player_grid_place_trampoline;
        memcpy(p, g_player_grid_place_signature, PLAYER_GRID_PLACE_STEAL);
        p += PLAYER_GRID_PLACE_STEAL;
        *p++ = 0xE9;
        rel = (DWORD)((PLAYER_GRID_PLACE_ENTRY + PLAYER_GRID_PLACE_STEAL) -
                      ((uintptr_t)p + 4));
        memcpy(p, &rel, 4);
    }
    memcpy(g_player_grid_place_orig, (const void*)PLAYER_GRID_PLACE_ENTRY,
           PLAYER_GRID_PLACE_STEAL);
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0xE9;
    rel = (DWORD)((uintptr_t)player_grid_place_detour -
                  (PLAYER_GRID_PLACE_ENTRY + 5));
    memcpy(patch + 1, &rel, 4);
    if (!VirtualProtect((void*)PLAYER_GRID_PLACE_ENTRY,
                        PLAYER_GRID_PLACE_STEAL,
                        PAGE_EXECUTE_READWRITE, &old)) return FALSE;
    memcpy((void*)PLAYER_GRID_PLACE_ENTRY, patch, sizeof(patch));
    VirtualProtect((void*)PLAYER_GRID_PLACE_ENTRY,
                   PLAYER_GRID_PLACE_STEAL, old, &old);
    FlushInstructionCache(GetCurrentProcess(),
                          (void*)PLAYER_GRID_PLACE_ENTRY,
                          PLAYER_GRID_PLACE_STEAL);
    g_player_grid_place_installed = TRUE;
    return TRUE;
#endif
}

static BOOL arm_player_grid_gate(void) {
#if !RF_DIAGNOSTIC_PLAYER_GRID_GATE
    return TRUE;
#else
    uintptr_t epoch = g_roster_reduced_epoch;
    InterlockedExchange(&g_player_tuple_armed, 0);
    InterlockedExchange(&g_player_tuple_pending, 0);
    InterlockedExchange(&g_player_tuple_result, 0);
    InterlockedExchange(&g_player_tuple_callbacks, 0);
    InterlockedExchange(&g_player_native_garage_result, 0);
    g_player_terminal_garage_lock_active = FALSE;
    g_player_native_garage_pit = -1;
    g_player_native_garage_ordinal = -1;
    g_player_native_garage_pos[0] = 0.0f;
    g_player_native_garage_pos[1] = 0.0f;
    g_player_native_garage_pos[2] = 0.0f;
    g_player_tuple_epoch = 0;
    g_player_tuple_order_count = 0;
    if (!g_player_out_active) return TRUE;
    /* g_session_epoch_id/g_player_out_epoch identify the original entrant
     * field.  Roster quarantine deliberately changes session_epoch(), so the
     * gate must own g_roster_reduced_epoch instead of requiring the two
     * necessarily different hashes to compare equal. */
    if (!g_player_out_committed ||
        !g_player_out_ptr || (uintptr_t)g_player_out_ptr != PLAYER_VEH_PTR ||
        !epoch || session_epoch() != epoch ||
        g_player_out_epoch != g_session_epoch_id)
        return FALSE;
    if (!snapshot_player_tuple_order() || !install_player_grid_gate())
        return FALSE;
    g_player_grid_gate_epoch = epoch;
    g_player_tuple_epoch = epoch;
    MemoryBarrier();
    InterlockedExchange(&g_player_grid_gate_armed, 1);
#if RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE
    InterlockedExchange(&g_player_tuple_armed, 1);
#endif
    return TRUE;
#endif
}

static void remove_player_grid_gate(void) {
#if RF_DIAGNOSTIC_PLAYER_GRID_GATE
    DWORD old;
    DWORD started;
    BYTE expected[PLAYER_GRID_PLACE_STEAL];
    DWORD rel;
    InterlockedExchange(&g_player_grid_gate_armed, 0);
    InterlockedExchange(&g_player_tuple_armed, 0);
    (void)InterlockedCompareExchange(&g_player_tuple_pending, 0, 1);
    g_player_grid_gate_epoch = 0;
    g_player_tuple_epoch = 0;
    g_player_tuple_order_count = 0;
    MemoryBarrier();
    if (g_player_grid_place_installed) {
        memset(expected, 0x90, sizeof(expected));
        expected[0] = 0xE9;
        rel = (DWORD)((uintptr_t)player_grid_place_detour -
                      (PLAYER_GRID_PLACE_ENTRY + 5));
        memcpy(expected + 1, &rel, 4);
        /* Never overwrite a site another plugin replaced after our install. */
        if (memcmp((const void*)PLAYER_GRID_PLACE_ENTRY, expected,
                   PLAYER_GRID_PLACE_STEAL) == 0 &&
            VirtualProtect((void*)PLAYER_GRID_PLACE_ENTRY,
                           PLAYER_GRID_PLACE_STEAL,
                           PAGE_EXECUTE_READWRITE, &old)) {
            memcpy((void*)PLAYER_GRID_PLACE_ENTRY,
                   g_player_grid_place_orig, PLAYER_GRID_PLACE_STEAL);
            VirtualProtect((void*)PLAYER_GRID_PLACE_ENTRY,
                           PLAYER_GRID_PLACE_STEAL, old, &old);
            FlushInstructionCache(GetCurrentProcess(),
                                  (void*)PLAYER_GRID_PLACE_ENTRY,
                                  PLAYER_GRID_PLACE_STEAL);
        }
        g_player_grid_place_installed = FALSE;
    }
    /* The trampoline is intentionally process-lifetime storage. Drain any
     * detour already executing before a hot-unload can free this DLL. */
    started = GetTickCount();
    while (InterlockedCompareExchange(&g_player_grid_gate_inflight, 0, 0) != 0 &&
           GetTickCount() - started < 500u) Sleep(1);
#endif
}

static BOOL build_grid_camera_trampoline(void) {
    g_grid_camera_trampoline = (BYTE*)VirtualAlloc(NULL, 64,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_grid_camera_trampoline) return FALSE;

    BYTE* p = g_grid_camera_trampoline;
    *p++ = 0xA1; *(DWORD*)p = (DWORD)(uintptr_t)&g_skip_prerace_cinematic; p += 4;
    *p++ = 0x85; *p++ = 0xC0;             /* test eax,eax */
    *p++ = 0x74; BYTE* jz_disabled = p++;  /* disabled -> original */
    *p++ = 0x8B; *p++ = 0x81;             /* mov eax,[ecx+0x88] */
    *(DWORD*)p = 0x88; p += 4;
    *p++ = 0x85; *p++ = 0xC0;             /* test eax,eax */
    *p++ = 0x74; BYTE* jz_no_event = p++;
    *p++ = 0x8B; *p++ = 0x50; *p++ = 0x24; /* mov edx,[eax+36] */
    *p++ = 0x83; *p++ = 0xFA; *p++ = 0x02; /* cmp edx,2 */
    *p++ = 0x72; BYTE* jb_not_visual = p++;
    *p++ = 0x83; *p++ = 0xFA; *p++ = 0x0A; /* cmp edx,10 */
    *p++ = 0x77; BYTE* ja_not_visual = p++;
    /* Convert this visual event to Wait/already-complete. The original
     * processor then advances the queue without executing its side effect. */
    *p++ = 0xC7; *p++ = 0x40; *p++ = 0x24;
    *(DWORD*)p = 11; p += 4;               /* event subtype = Wait */
    *p++ = 0xC7; *p++ = 0x40; *p++ = 0x28;
    *(DWORD*)p = 4; p += 4;                /* event action = complete */
    BYTE* original = p;
    *p++ = 0xB8; *(DWORD*)p = GRID_CAMERA_ORIGINAL; p += 4;
    *p++ = 0xFF; *p++ = 0xE0;             /* jmp eax */
    *jz_disabled = (BYTE)(original - (jz_disabled + 1));
    *jz_no_event = (BYTE)(original - (jz_no_event + 1));
    *jb_not_visual = (BYTE)(original - (jb_not_visual + 1));
    *ja_not_visual = (BYTE)(original - (ja_not_visual + 1));
    return TRUE;
}

static BOOL install_grid_camera_hook(void) {
    if (g_grid_camera_hook_installed) return TRUE;
    if (!g_grid_camera_trampoline && !build_grid_camera_trampoline()) return FALSE;
    DWORD old;
    memcpy(g_grid_camera_orig, (void*)GRID_CAMERA_CALL_SITE, GRID_CAMERA_CALL_SIZE);
    if (g_grid_camera_orig[0] != 0xE8) return FALSE;
    int32_t original_rel = *(int32_t*)(g_grid_camera_orig + 1);
    if ((uintptr_t)(GRID_CAMERA_CALL_SITE + 5 + original_rel)
        != (uintptr_t)GRID_CAMERA_ORIGINAL) return FALSE;
    if (!VirtualProtect((void*)GRID_CAMERA_CALL_SITE, GRID_CAMERA_CALL_SIZE,
                        PAGE_EXECUTE_READWRITE, &old)) return FALSE;
    BYTE patch[GRID_CAMERA_CALL_SIZE];
    patch[0] = 0xE8;
    DWORD rel = (DWORD)((uintptr_t)g_grid_camera_trampoline
                        - (GRID_CAMERA_CALL_SITE + 5));
    memcpy(patch + 1, &rel, 4);
    memcpy((void*)GRID_CAMERA_CALL_SITE, patch, GRID_CAMERA_CALL_SIZE);
    VirtualProtect((void*)GRID_CAMERA_CALL_SITE, GRID_CAMERA_CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)GRID_CAMERA_CALL_SITE,
                          GRID_CAMERA_CALL_SIZE);
    g_grid_camera_hook_installed = TRUE;
    return TRUE;
}

static void remove_grid_camera_hook(void) {
    InterlockedExchange(&g_skip_prerace_cinematic, 0);
    if (!g_grid_camera_hook_installed) return;
    if (!call_site_targets(GRID_CAMERA_CALL_SITE,
                           (DWORD)(uintptr_t)g_grid_camera_trampoline)) {
        g_grid_camera_hook_installed = FALSE;
        return;
    }
    DWORD old;
    if (!VirtualProtect((void*)GRID_CAMERA_CALL_SITE, GRID_CAMERA_CALL_SIZE,
                        PAGE_EXECUTE_READWRITE, &old)) return;
    memcpy((void*)GRID_CAMERA_CALL_SITE, g_grid_camera_orig,
           GRID_CAMERA_CALL_SIZE);
    VirtualProtect((void*)GRID_CAMERA_CALL_SITE, GRID_CAMERA_CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)GRID_CAMERA_CALL_SITE,
                          GRID_CAMERA_CALL_SIZE);
    g_grid_camera_hook_installed = FALSE;
}

/* Arm ONE car onto the game's own retirement path. Returns TRUE if armed.
 * Skips the player, cars already retired, and cars whose rear-wing record
 * pointer is NULL (the retire term can never be satisfied without it). */
static BOOL arm_pit_retire(char* veh) {
    if (!veh) return FALSE;
    if ((uintptr_t)veh == PLAYER_VEH_PTR) return FALSE;
    if (*(volatile int*)(veh + OFF_FINISHSTATE) != 0) return FALSE;
    if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) return FALSE;
    if (*(volatile int*)(veh + OFF_REARWING_PTR) == 0) return FALSE;

    *(volatile BYTE*)(veh + OFF_REARWING_FLAG) = 0;   /* arm the trigger  */
    *(volatile int*) (veh + OFF_CALLTOBOX)     = 1;   /* send it to pits  */
    return TRUE;
}

/* Cars armed by the current NUMPAD2 red-flag sequence. Tracked separately from
 * g_dnf_ptrs (pre-existing DNFs) so we can UN-retire exactly these before the
 * restart, and so re_retire_dnf() never re-pins them. */
static void** g_pitret_ptrs = NULL;
static int    g_pitret_count = 0;

/* RF88: cars from the current pit-retire set observed entering the pit lane
 * (pittingStatus >= 2). Only these count as normal drive-to-box retirees. A
 * pit-armed car that is never seen in the lane and is killed before its stop
 * is NOT a temporary retiree: it is classified as a late DNF by
 * sweep_late_dnf(). Absence of evidence keeps the old exemption (fail safe). */
static void** g_pit_arrived_ptrs = NULL;
static int    g_pit_arrived_count = 0;

/* Per-car strategy-contamination ownership for the WHOLE red-flag sequence,
 * deliberately independent of the transient retirement/arrival lists. A car
 * RF arms for pit retirement may have its native plan entry rewritten by the
 * forced-retirement service path before it even reaches the box; the trigger
 * snapshot is the true plan for every car this list names. The
 * DriveToPits->Countdown timeout fallback clears the retirement/arrival lists
 * (retirement/readiness semantics) but must not erase this ownership. Reset
 * only at the sequence boundary in snapshot_positions(). */
static void** g_strategy_contaminated_ptrs = NULL;
static int    g_strategy_contaminated_count = 0;

/* Read-only native garage evidence. A pit-lane latch is never a garage pose. */
static int rf_lifecycle_read(uintptr_t address,void* out,size_t size) {
    if(!out || !runtime_region_supported(address,size,FALSE))return 0;
    memcpy(out,(const void*)address,size);return 1;
}
static F1CVehicleLifecycle rf_pit_lifecycle(char* vehicle) {
    return f1c_vehicle_lifecycle_read(rf_lifecycle_read,(uintptr_t)vehicle);
}

static BOOL captured_dnf_contains(char* veh) {
    if (!veh) return FALSE;
    for (int i = 0; i < g_dnf_count; i++)
        if ((char*)g_dnf_ptrs[i] == veh) return TRUE;
    return FALSE;
}

static BOOL pit_arrived_contains(char* veh) {
    if (!veh) return FALSE;
    for (int i = 0; i < g_pit_arrived_count; i++)
        if ((char*)g_pit_arrived_ptrs[i] == veh) return TRUE;
    return FALSE;
}

static void pit_arrived_latch(char* veh) {
    if (!veh || !g_pit_arrived_ptrs) return;
    if (g_pit_arrived_count >= g_session_capacity) return;
    if (pit_arrived_contains(veh)) return;
    g_pit_arrived_ptrs[g_pit_arrived_count++] = veh;
}

static BOOL strategy_contaminated_contains(char* veh) {
    if (!veh) return FALSE;
    for (int i = 0; i < g_strategy_contaminated_count; i++)
        if ((char*)g_strategy_contaminated_ptrs[i] == veh) return TRUE;
    return FALSE;
}

/* Latched the moment RF arms a car for pit retirement, BEFORE the native
 * service can rewrite its plan entry. Never cleared by retirement/arrival
 * list maintenance; only snapshot_positions() starts a new ownership set. */
static void strategy_contaminate_latch(char* veh) {
    if (!veh || !g_strategy_contaminated_ptrs) return;
    if (g_strategy_contaminated_count >= g_session_capacity) return;
    if (strategy_contaminated_contains(veh)) return;
    g_strategy_contaminated_ptrs[g_strategy_contaminated_count++] = veh;
}

/* NUMPAD3: arm the whole field except the player. The game does the rest. */
static void pit_retire_all(void) {
    int count = VEHICLE_COUNT;
    int armed = 0, held = 0;
    g_pitret_count = 0;
    g_pit_arrived_count = 0;
    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        /* snapshot_positions() owns pre-existing DNFs, including physics-dead
         * terminal wrecks that F1C has not yet stamped fin/ret. Never arm them
         * as temporary pit retirees: unretire_pitret() must not revive them. */
        if (captured_dnf_contains(veh)) continue;
        if (arm_pit_retire(veh)) {
            armed++;
            g_pitret_ptrs[g_pitret_count++] = veh;
            strategy_contaminate_latch(veh);
            if (*(volatile BYTE*)(veh + OFF_REARWING_FLAG) == 0) held++;
        }
    }
    char buf[192];
    wsprintfA(buf, "RF-EB01278D97C %d %d"
                   "They retire on reaching their own pit box.", armed, held);
    log_w(buf);
    if (armed != held)
        log_w("RF-EE8D92FB9F6"
              "expected only if the game recalculated them.");
}

/* Undo an arming that has not yet fired (car still en route). Once the game has
 * retired the car the write is spent — it resets the flag itself. */
static void pit_retire_revert_all(void) {
    int count = VEHICLE_COUNT;
    int n = 0;
    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh || (uintptr_t)veh == PLAYER_VEH_PTR) continue;
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
        *(volatile BYTE*)(veh + OFF_REARWING_FLAG) = 1;
        *(volatile int*) (veh + OFF_CALLTOBOX)     = 0;
        n++;
    }
    char buf[64];
    wsprintfA(buf, "RF-EF37F1A2405 %d", n);
    log_w(buf);
}

/* Undo the retirement on exactly the cars WE retired for the red flag, so the
 * restart puts them back on the grid. Called immediately before RESTART_FLAG.
 *
 * RF88: a car WE armed that was killed before it ever reached the pit lane has
 * already been captured into g_dnf_ptrs by sweep_late_dnf(), so the
 * captured-DNF guard below leaves it retired instead of reviving it.
 *
 * We do NOT rely on the restart reviving DNF cars on its own. It appears to
 * (that is why re_retire_dnf() and the anti-reactivation hooks exist), but that
 * has never been verified for a whole field, and getting it wrong means the
 * player restarts alone. Clearing the flags ourselves is cheap insurance. */
static void unretire_pitret(void) {
    int n = 0;
    for (int i = 0; i < g_pitret_count; i++) {
        char* veh = (char*)g_pitret_ptrs[i];
        if (!veh) continue;
        if (captured_dnf_contains(veh)) {
            log_w("RF-EDC4F502280");
            continue;
        }
        *(volatile BYTE*)(veh + OFF_REARWING_FLAG) = 1;   /* disarm trigger  */
        *(volatile int*) (veh + OFF_CALLTOBOX)     = 0;
        *(volatile int*) (veh + OFF_PITTING)       = 0;
        *(volatile int*) (veh + OFF_FINISHSTATE)   = 0;
        *(volatile BYTE*)(veh + OFF_RETIREDFLAG)   = 0;
        *(volatile BYTE*)(veh + OFF_ACTIVEFLAG)    = 1;
        n++;
    }
    char buf[80];
    wsprintfA(buf, "RF-EC01231F76F %d", n);
    log_w(buf);
}

/* Is the player sitting in his OWN PIT BOX?
 *
 * WAS `pittingStatus == 2`, AND THAT IS THE PIT LANE, NOT THE BOX. The full
 * enum is now CONFIRMED from the pit-stop reverse engineering
 * (patches/pitstop_modding/docs/PIT_STOP_FIRING_ORDER.md, five live stops):
 *
 *     0 = on track      1 = requested / in-lap     2 = PIT LANE
 *     3 = IN THE BOX (StateChangeNotify(this,3), the box stop)
 *     4 = leaving the box / exit
 *
 * So the old test fired the restart the moment the player crossed the pit
 * ENTRY, which is exactly what was reported in game. The box stop is 3.
 *
 * Two ways in, because a red-flag stop is not an ordinary pit stop and the
 * crew may never be armed for it:
 *   - pittingStatus >= 3: the game itself says he is in the box;
 *   - pittingStatus >= 2 AND stopped for BOX_STILL_MS: he has driven down the
 *     lane and parked. A car doing under 0.5 m/s in the pit lane has arrived;
 *     the dwell only stops a momentary stall at the limiter counting.
 *
 * The dwell is measured in calls, and this is called once per ~1 ms tick of
 * the phase-10 loop, so it is wall time. It resets whenever he is moving or
 * out of the lane, so a lap through the pits without stopping never fires. */
#define BOX_STILL_MS 1200

static BOOL player_in_pit_box(void) {
    uintptr_t veh=PLAYER_VEH_PTR;
    if(!veh || !runtime_region_supported(veh,VEH_STRIDE,FALSE))return FALSE;
    int pit=*(volatile int*)(veh+OFF_PITTING);
    float speed=*(volatile float*)(veh+OFF_SPEED);
    /* Native service can advance to state 4 while the player remains stopped
       in the box. It still needs an explicit fresh confirmation, like state 2;
       it is not automatic arrival proof and moving cars are never accepted. */
    if(!isfinite(speed)||fabsf(speed)>=0.5f || (pit!=2&&pit!=3&&pit!=4)) {
        g_player_box_confirmed=FALSE;return FALSE;
    }
    return pit==3 || g_player_box_confirmed;
}

/* All missing survivors must be positively healthy before the one-minute
 * escape is allowed. Unknown memory and terminal flags never count as healthy. */
static BOOL pit_survivors_ready(void) {
    if(!player_in_pit_box()){g_player_arrived_at=0;return FALSE;}
    if(!g_player_arrived_at)g_player_arrived_at=g_rf_policy_now;
    BOOL all_arrived=TRUE, missing_healthy=TRUE;
    for(int i=0;i<g_active_count;i++) {
        char* v=g_active[i];
        if((uintptr_t)v==PLAYER_VEH_PTR)continue;
        if(!v || !runtime_region_supported((uintptr_t)v,VEH_STRIDE,FALSE)) {
            all_arrived=FALSE;missing_healthy=FALSE;continue;
        }
        float speed=*(volatile float*)(v+OFF_SPEED);
        int pit=*(volatile int*)(v+OFF_PITTING);
        BOOL arrived=isfinite(speed)&&fabsf(speed)<0.5f && pit==3;
        /* Native temporary retirement after our box order also counts as arrival. */
        if(!arrived && pit_arrived_contains(v) && !veh_heavy_damage_evidence(v)) {
            F1CVehicleLifecycle life=rf_pit_lifecycle(v);
            arrived=life.valid && life.garage_clear;
        }
        if(arrived)continue;
        all_arrived=FALSE;
        if(!isfinite(speed)||veh_killed(v)||veh_heavy_damage_evidence(v)||
           *(volatile int*)(v+OFF_CAR_CONDITIONS)!=0 ||
           *(volatile int*)(v+OFF_FINISHSTATE)!=0 || *(volatile BYTE*)(v+OFF_RETIREDFLAG)!=0 ||
           !isfinite(*(volatile float*)(v+OFF_ENGINE_RPM)) ||
           *(volatile float*)(v+OFF_ENGINE_RPM)<=ENGINE_OFF_RPM)missing_healthy=FALSE;
        const int life_fields[]={OFF_ENGINE_LIFE,OFF_BRAKE_LIFE,OFF_GEARBOX_LIFE,OFF_SUSPENSION_LIFE};
        for(unsigned j=0;j<sizeof(life_fields)/sizeof(life_fields[0]);j++) {
            float life=*(volatile float*)(v+life_fields[j]);
            if(!isfinite(life)||life<=CRITICAL_LIFE)missing_healthy=FALSE;
        }
    }
    return all_arrived || (missing_healthy && g_rf_policy_now-g_player_arrived_at>=60000u);
}

/* NOTE: this forges the end-state and is LEFT AS-IS deliberately. It re-pins
 * cars that were ALREADY DNF before the red flag, during the restart — they
 * are not candidates for the pit path (they cannot drive to a box). Cars
 * retired via NUMPAD3 are parked in their garages by the game and need
 * neither this nor hide_dnf_cars(). */
static void re_retire_dnf(void) {
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE))
            continue;
        *(volatile BYTE*)(veh + OFF_RETIREDFLAG) = 1;
        *(volatile BYTE*)(veh + OFF_ACTIVEFLAG)  = 0;
    }
}

#define SCENE_STRUCT    0x005EA2C0
#define OFF_NODETABLE   344
#define OFF_DIM         356
#define OFF_NODEIDX     0x48A4
#define OFF_SUBIDX      0x48A8
#define OFF_PHYS_Y      0x20
#define HIDE_Y          (-1000.0f)

/* Validated garage placement cache. OFF_POS* is the reviewed authoritative
 * vehicle position and the first three floats of the scene entry are the
 * rendered position already used by hold_wrecks_on_track(). Cache every car
 * observed in a pit/garage before the race settles, plus any already-retired
 * garage car observed later. This gives each DNF its own real garage location
 * without guessing coordinates or calling an unreviewed game function. */
static void**    g_garage_ptrs = NULL;
static float*    g_garage_px = NULL;
static float*    g_garage_py = NULL;
static float*    g_garage_pz = NULL;
static float*    g_garage_nx = NULL;
static float*    g_garage_ny = NULL;
static float*    g_garage_nz = NULL;
static int       g_garage_count = 0;
static uintptr_t g_garage_epoch = 0;

static int garage_cache_find(char* veh) {
    for (int i = 0; i < g_garage_count; i++)
        if (g_garage_ptrs[i] == (void*)veh) return i;
    return -1;
}

static char* checked_scene_entry(char* veh);
static void hidden_forget(void);

static void cache_garage_positions(void) {
    uintptr_t epoch = session_epoch();
    if (!epoch || SESSION_TYPE != 7) {
        g_garage_epoch = 0;
        g_garage_count = 0;
        return;
    }
    if (epoch != g_garage_epoch) {
        g_garage_epoch = epoch;
        g_garage_count = 0;
    }
    if (g_garage_count >= g_session_capacity) return;

    int nodeTable = *(volatile int*)(SCENE_STRUCT + OFF_NODETABLE);
    int dim = *(volatile int*)(SCENE_STRUCT + OFF_DIM);
    if (!nodeTable || !dim) return;

    BOOL settled = race_is_settled();
    int count = VEHICLE_COUNT;
    for (int i = 0; i < count; i++) {
        if (g_garage_count >= g_session_capacity || session_epoch() != epoch)
            return;
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE) ||
            garage_cache_find(veh) >= 0) continue;
        int pit = *(volatile int*)(veh + OFF_PITSTATE);
        BOOL retired = *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0;
        if (pit < 2 || (settled && !retired)) continue;

        char* entry = checked_scene_entry(veh);
        if (!entry) continue;
        int n = g_garage_count++;
        g_garage_ptrs[n] = veh;
        g_garage_px[n] = *(volatile float*)(veh + OFF_POSX);
        g_garage_py[n] = *(volatile float*)(veh + OFF_POSY);
        g_garage_pz[n] = *(volatile float*)(veh + OFF_POSZ);
        g_garage_nx[n] = *(volatile float*)(entry + 0);
        g_garage_ny[n] = *(volatile float*)(entry + 4);
        g_garage_nz[n] = *(volatile float*)(entry + 8);
    }
}

/* Keep every DNF for which a validated garage transform exists at that exact
 * garage position. A missing cache is fail-safe: leave the retired car where
 * the game put it and log once; never send it to an invented/off-world Y. */
static void restore_dnf_garages(BOOL quiet) {
    char buf[160];
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE))
            continue;
        int n = garage_cache_find(veh);
        if (n < 0) {
            if (!quiet) {
                wsprintfA(buf, "RF-E7C37672A2D %d %s",
                          veh_slot(veh), veh_name(veh));
                log_w(buf);
            }
            continue;
        }

        *(volatile float*)(veh + OFF_POSX) = g_garage_px[n];
        *(volatile float*)(veh + OFF_POSY) = g_garage_py[n];
        *(volatile float*)(veh + OFF_POSZ) = g_garage_pz[n];
        {
            char* entry = checked_scene_entry(veh);
            if (entry) {
            /* Historical cache is not authoritative; no AIW writes. */
            }
        }
        if (!quiet) {
            wsprintfA(buf, "RF-E9391964AA5 %d %s",
                      veh_slot(veh), veh_name(veh));
            log_w(buf);
        }
    }
}

static char* checked_scene_entry(char* veh) {
    int nodeTable;
    int dim;
    int nodeIdx;
    int subIdx;
    uintptr_t linear;
    uintptr_t entry;
    uintptr_t maxp = (uintptr_t)-1;
    if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE))
        return NULL;
    nodeTable = *(volatile int*)(SCENE_STRUCT + OFF_NODETABLE);
    dim = *(volatile int*)(SCENE_STRUCT + OFF_DIM);
    nodeIdx = *(volatile int*)(veh + OFF_NODEIDX);
    subIdx = *(volatile int*)(veh + OFF_SUBIDX);
    if (!nodeTable || dim <= 0 || nodeIdx < 0 || subIdx < 0) return NULL;
    if (subIdx > (INT_MAX - nodeIdx) / dim) return NULL;
    linear = (uintptr_t)(nodeIdx + subIdx * dim);
    if (linear > (maxp - (uintptr_t)nodeTable) / 28u) return NULL;
    entry = (uintptr_t)nodeTable + 28u * linear;
    if (!runtime_region_supported(entry, 12u, FALSE)) return NULL;
    return (char*)entry;
}

/* Capture only a native, authoritative player terminal state. ActiveFlag is
 * deliberately insufficient: it is transient during ordinary session work.
 * Physics/scene coordinates are a diagnostic baseline only, never labelled a
 * garage and never written back. The diagnostic grid gate prevents a native
 * grid transform; GRID_READY records the resulting pose for confirmation. */
static BOOL capture_player_out(char* veh) {
    char buf[224];
    if (!veh || (uintptr_t)veh != PLAYER_VEH_PTR ||
        !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE))
        return FALSE;

    int finish = *(volatile int*)(veh + OFF_FINISHSTATE);
    BYTE retired = *(volatile BYTE*)(veh + OFF_RETIREDFLAG);
    /* RetiredFlag can lead the final classification/reason by several ticks.
     * Wait for the native nonzero FinishState before treating the tuple as
     * settled enough to preserve. */
    if (finish == 0) return FALSE;

    if (!g_player_out_active) {
        g_player_out_active = TRUE;
        g_player_out_ptr = veh;
        g_player_out_epoch = g_session_epoch_id;
        g_player_out_finish = finish;
        g_player_out_reason = *(volatile int*)(veh + OFF_REASONCODE);
        g_player_out_lap = *(volatile int*)(veh + OFF_CURRENTLAP);
        g_player_out_racepos = *(volatile int*)(veh + OFF_RACEPOS);
        g_player_out_qualpos = *(volatile int*)(veh + OFF_QUALPOS);
        g_player_out_role = *(volatile int*)(veh + OFF_SLOTLIVE);
        g_player_out_retired = retired;
        g_player_out_active_flag = *(volatile BYTE*)(veh + OFF_ACTIVEFLAG);
        g_player_out_player_out = PLAYER_OUT_FLAG;
        wsprintfA(buf, "RF-EBCD811C4F2 %d %d %d %d %d %d %d",
                  g_player_out_finish, g_player_out_reason,
                  (int)g_player_out_retired, (int)g_player_out_active_flag,
                  g_player_out_role, g_player_out_lap,
                  g_player_out_player_out);
        log_w(buf);
    } else if (!g_player_out_committed) {
        /* The native reason/retired flags may settle after FinishState. Keep
         * identity/lap/order from the first authoritative sample, but refresh
         * the classification tuple until the pre-restart commit. */
        g_player_out_finish = finish;
        g_player_out_reason = *(volatile int*)(veh + OFF_REASONCODE);
        g_player_out_retired = retired;
        g_player_out_active_flag = *(volatile BYTE*)(veh + OFF_ACTIVEFLAG);
    }

    if (!g_player_out_garage_valid &&
        g_player_out_ptr == veh && g_player_out_epoch == g_session_epoch_id) {
        char* entry = checked_scene_entry(veh);
        if (entry) {
                float px = *(volatile float*)(veh + OFF_POSX);
                float py = *(volatile float*)(veh + OFF_POSY);
                float pz = *(volatile float*)(veh + OFF_POSZ);
                float nx = *(volatile float*)(entry + 0);
                float ny = *(volatile float*)(entry + 4);
                float nz = *(volatile float*)(entry + 8);
                if (isfinite(px) && isfinite(py) && isfinite(pz) &&
                    isfinite(nx) && isfinite(ny) && isfinite(nz)) {
                    g_player_out_px = px; g_player_out_py = py; g_player_out_pz = pz;
                    g_player_out_nx = nx; g_player_out_ny = ny; g_player_out_nz = nz;
                    g_player_out_pitstate = *(volatile int*)(veh + OFF_PITSTATE);
                    g_player_out_garage_valid = TRUE;
                    wsprintfA(buf, "RF-E01E4BAEBCC %d %d %d %d",
                              g_player_out_pitstate, (int)px, (int)py, (int)pz);
                    log_w(buf);
                }
        }
    }
    return TRUE;
}

static BOOL player_out_is_counted(void) {
    int found = 0;
    if (!g_player_out_active) return TRUE;
    if (!g_player_out_ptr || (uintptr_t)g_player_out_ptr != PLAYER_VEH_PTR ||
        g_player_out_epoch != g_session_epoch_id ||
        g_player_out_role != 0 || VEHICLE_COUNT < 1 ||
        VEHICLE_COUNT > g_session_capacity) return FALSE;
    for (int i = 0; i < VEHICLE_COUNT; ++i)
        if (VEHICLE_ARRAY[i] == (void*)g_player_out_ptr) ++found;
    return found == 1;
}

static BOOL player_out_native_state_matches(void) {
    char* veh = g_player_out_ptr;
    if (!g_player_out_active) return TRUE;
    if (!player_out_is_counted() || !veh ||
        !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE) ||
        PLAYER_OUT_FLAG != g_player_out_player_out ||
        *(volatile int*)(veh + OFF_FINISHSTATE) != g_player_out_finish ||
        *(volatile int*)(veh + OFF_REASONCODE) != g_player_out_reason ||
        *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != g_player_out_retired ||
        *(volatile BYTE*)(veh + OFF_ACTIVEFLAG) != g_player_out_active_flag ||
        *(volatile int*)(veh + OFF_SLOTLIVE) != g_player_out_role)
        return FALSE;
    return TRUE;
}

static BOOL player_out_restored_tuple_matches(void) {
    return player_out_native_state_matches() &&
        (!g_player_out_active ||
         *(volatile int*)(g_player_out_ptr + OFF_CURRENTLAP) ==
             g_player_out_lap);
}

/* Capture and later prove the exact reduced pointer order. VehicleCount alone
 * is not a construction-lifetime proof for replay or the player object. */
static BOOL snapshot_player_tuple_order(void) {
#if !RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE
    return TRUE;
#else
    int count = VEHICLE_COUNT;
    uintptr_t epoch = session_epoch();
    if (!g_player_tuple_order || count < 1 || count > g_session_capacity ||
        !epoch || epoch != g_roster_reduced_epoch ||
        g_player_out_epoch != g_session_epoch_id) return FALSE;
    for (int i = 0; i < count; ++i) {
        void* veh = (void*)VEHICLE_ARRAY[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh,
                                               VEH_STRIDE, FALSE))
            return FALSE;
        g_player_tuple_order[i] = veh;
    }
    MemoryBarrier();
    if (VEHICLE_COUNT != count || session_epoch() != epoch) return FALSE;
    for (int i = 0; i < count; ++i)
        if (VEHICLE_ARRAY[i] != g_player_tuple_order[i]) return FALSE;
    g_player_tuple_order_count = count;
    return TRUE;
#endif
}

static BOOL player_tuple_order_matches(char** current_player) {
    int found = 0;
    char* player = (char*)PLAYER_VEH_PTR;
    if (current_player) *current_player = NULL;
    if (!g_player_tuple_order || g_player_tuple_order_count < 1 ||
        g_player_tuple_order_count > g_session_capacity ||
        VEHICLE_COUNT != g_player_tuple_order_count ||
        !g_player_out_ptr || player != g_player_out_ptr ||
        !runtime_region_supported((uintptr_t)player, VEH_STRIDE, FALSE))
        return FALSE;
    for (int i = 0; i < g_player_tuple_order_count; ++i) {
        void* live = (void*)VEHICLE_ARRAY[i];
        if (live != g_player_tuple_order[i]) return FALSE;
        if (live == (void*)player) ++found;
    }
    if (found != 1) return FALSE;
    if (current_player) *current_player = player;
    return TRUE;
}

/* F1C's native garage path. IndexedStructDataRead reads a validated AIW
 * GarPos/GarOri record from the database at 0x005EA2C0, using the vehicle's
 * pit/team index (+0x48A4) and garage ordinal (+0x48A8). The second call uses
 * the effective receiver vehicle+0x0C, matching the native call sites, and
 * synchronizes orientation, render/chassis translation and the player rigid
 * body. This runs only on the broker-owned game thread at GRID_READY. */
#define AI_DATABASE_BASE             0x005EA2C0u
#define AI_DATABASE_GARAGE_PTR_OFF   0x0158u
#define AI_DATABASE_PITSPOTS_OFF     0x0164u
#define AI_DATABASE_GARAGESPOTS_OFF  0x0168u
#define OFF_NATIVE_GARAGE_PIT        0x48A4u
#define OFF_NATIVE_GARAGE_ORDINAL    0x48A8u
#define NATIVE_GARAGE_POSE_READ      0x004D1140u
#define NATIVE_VEHICLE_POSE_APPLY    0x004EF7C0u
typedef int (__stdcall *NativeGaragePoseReadFn)(void*, float*, float*);
typedef void* (__attribute__((thiscall)) *NativeVehiclePoseApplyFn)(
    void*, float*, float*);

/* v73: replay/native restart can leave a quarantined AI with a stale pose.
 * Read its actual allocated garage, then apply the full native transform on
 * the broker game thread. Never infer a garage from a pit-state observation.
 * Inputs are preflighted for the whole owned roster before the first call. */
static __attribute__((noinline, optimize("O0"))) BOOL dnf_garage_pose(char* veh, float pos[3], float ori[3]) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("dnf_garage_pose");

    int pits, garages, pit, ordinal;
    char* records;
    char* record;
    if (!veh || (uintptr_t)veh == PLAYER_VEH_PTR ||
        !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE) ||
        !runtime_region_supported(AI_DATABASE_BASE + AI_DATABASE_GARAGE_PTR_OFF,
                                  20u, FALSE)) { rf_protected_result = (FALSE); goto rf_protected_exit; }
    records = *(char**)(AI_DATABASE_BASE + AI_DATABASE_GARAGE_PTR_OFF);
    pits = *(int*)(AI_DATABASE_BASE + AI_DATABASE_PITSPOTS_OFF);
    garages = *(int*)(AI_DATABASE_BASE + AI_DATABASE_GARAGESPOTS_OFF);
    pit = *(int*)(veh + OFF_NATIVE_GARAGE_PIT);
    ordinal = *(int*)(veh + OFF_NATIVE_GARAGE_ORDINAL);
    if (!records || pits < 1 || pits > 104 || garages < 1 || garages > 16 ||
        pit < 0 || pit >= pits || ordinal < 0 || ordinal >= garages ||
        !runtime_region_supported((uintptr_t)records, (size_t)pits * garages * 28u,
                                  FALSE)) { rf_protected_result = (FALSE); goto rf_protected_exit; }
    record = records + 28u * (pit + ordinal * pits);
    memcpy(pos, record, 12u);
    memcpy(ori, record + 12u, 12u);
    for (int axis = 0; axis < 3; ++axis)
        if (!isfinite(pos[axis]) || !isfinite(ori[axis]) ||
            fabsf(pos[axis]) > 1000000.0f || fabsf(ori[axis]) > 6.4f)
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
    /* Reject the exact legacy sink sentinel, never invent a replacement Y. */
    { rf_protected_result = (pos[1] != HIDE_Y &&
        (fabsf(pos[0]) > 0.001f || fabsf(pos[1]) > 0.001f || fabsf(pos[2]) > 0.001f)); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

static __attribute__((noinline, optimize("O0"))) BOOL restore_owned_dnf_garage_poses(void) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("restore_owned_dnf_garage_poses");

    float pos[3], ori[3];
    NativeVehiclePoseApplyFn apply_pose =
        (NativeVehiclePoseApplyFn)NATIVE_VEHICLE_POSE_APPLY;
    static const BYTE apply_bytes[] = {
        0x83,0xEC,0x30,0x8B,0x44,0x24,0x38,0x8B,0x10,0x53,0x56,0x8B,
        0xF1,0x8D,0x9E,0xE0,0x34,0x00,0x00,0x8B,0xCB,0x89,0x11,0x8B
    };
    if (!g_roster_count) { rf_protected_result = (TRUE); goto rf_protected_exit; }
    if (SESSION_TYPE != 7 || RACE_PHASE != 5 || !g_roster_ptrs ||
        g_roster_count < 0 || g_roster_count > g_session_capacity ||
        !g_roster_results_restored || !g_roster_full_ptrs ||
        VEHICLE_COUNT != g_roster_original_count ||
        !runtime_region_supported(NATIVE_VEHICLE_POSE_APPLY, sizeof(apply_bytes), TRUE) ||
        memcmp((void*)NATIVE_VEHICLE_POSE_APPLY, apply_bytes, sizeof(apply_bytes)))
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    for (int i = 0; i < g_roster_original_count; ++i)
        if (VEHICLE_ARRAY[i] != g_roster_full_ptrs[i]) { rf_protected_result = (FALSE); goto rf_protected_exit; }
    for (int i = 0; i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        if (!dnf_garage_pose(veh, pos, ori) ||
            *(BYTE*)(veh + OFF_RETIREDFLAG) != 1 ||
            *(BYTE*)(veh + OFF_ACTIVEFLAG) != 0 ||
            *(int*)(veh + OFF_FINISHSTATE) != 2) { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
    for (int i = 0; i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        if (!dnf_garage_pose(veh, pos, ori)) { rf_protected_result = (FALSE); goto rf_protected_exit; }
        (void)apply_pose(veh + 0x0C, pos, ori);
    }
    log_w("RF-E0499D8EBAC");
    { rf_protected_result = (TRUE); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

static BOOL native_player_place_in_garage(char* player) {
#if !RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE
    (void)player;
    return TRUE;
#else
    void* garage_records;
    int pitspots;
    int garagespots;
    int pit;
    int ordinal;
    size_t record_count;
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float ori[3] = {0.0f, 0.0f, 0.0f};
    float dx;
    float dy;
    float dz;
    NativeGaragePoseReadFn read_pose =
        (NativeGaragePoseReadFn)NATIVE_GARAGE_POSE_READ;
    NativeVehiclePoseApplyFn apply_pose =
        (NativeVehiclePoseApplyFn)NATIVE_VEHICLE_POSE_APPLY;

    InterlockedExchange(&g_player_native_garage_result, -1);
    if (!player || (uintptr_t)player != PLAYER_VEH_PTR ||
        !runtime_region_supported((uintptr_t)player, VEH_STRIDE, FALSE) ||
        !runtime_region_supported(AI_DATABASE_BASE +
            AI_DATABASE_GARAGE_PTR_OFF, sizeof(void*), FALSE) ||
        !runtime_region_supported(AI_DATABASE_BASE +
            AI_DATABASE_PITSPOTS_OFF, sizeof(int) * 2u, FALSE) ||
        !runtime_region_supported(NATIVE_GARAGE_POSE_READ, 1, TRUE) ||
        !runtime_region_supported(NATIVE_VEHICLE_POSE_APPLY, 1, TRUE))
        return FALSE;

    garage_records = *(void**)(AI_DATABASE_BASE +
                               AI_DATABASE_GARAGE_PTR_OFF);
    pitspots = *(volatile int*)(AI_DATABASE_BASE +
                               AI_DATABASE_PITSPOTS_OFF);
    garagespots = *(volatile int*)(AI_DATABASE_BASE +
                                  AI_DATABASE_GARAGESPOTS_OFF);
    if (!garage_records || pitspots < 1 || pitspots > 104 ||
        garagespots < 1 || garagespots > 16)
        return FALSE;
    record_count = (size_t)pitspots * (size_t)garagespots;
    if (record_count > (SIZE_MAX / 28u) ||
        !runtime_region_supported((uintptr_t)garage_records,
                                  record_count * 28u, FALSE))
        return FALSE;

    (void)read_pose(player, pos, ori);
    pit = *(volatile int*)(player + OFF_NATIVE_GARAGE_PIT);
    ordinal = *(volatile int*)(player + OFF_NATIVE_GARAGE_ORDINAL);
    if (pit < 0 || pit >= pitspots || ordinal < 0 || ordinal >= garagespots ||
        !isfinite(pos[0]) || !isfinite(pos[1]) || !isfinite(pos[2]) ||
        !isfinite(ori[0]) || !isfinite(ori[1]) || !isfinite(ori[2]) ||
        (fabsf(pos[0]) < 0.001f && fabsf(pos[1]) < 0.001f &&
         fabsf(pos[2]) < 0.001f)) {
        InterlockedExchange(&g_player_native_garage_result, -2);
        return FALSE;
    }

    (void)apply_pose(player + 0x0C, pos, ori);
    MemoryBarrier();
    dx = *(volatile float*)(player + OFF_POSX) - pos[0];
    dy = *(volatile float*)(player + OFF_POSY) - pos[1];
    dz = *(volatile float*)(player + OFF_POSZ) - pos[2];
    if (!isfinite(dx) || !isfinite(dy) || !isfinite(dz) ||
        fabsf(dx) > 2.0f || fabsf(dy) > 5.0f || fabsf(dz) > 2.0f) {
        InterlockedExchange(&g_player_native_garage_result, -3);
        return FALSE;
    }
    g_player_native_garage_pit = pit;
    g_player_native_garage_ordinal = ordinal;
    g_player_native_garage_pos[0] = pos[0];
    g_player_native_garage_pos[1] = pos[1];
    g_player_native_garage_pos[2] = pos[2];
#if RF_DIAGNOSTIC_PLAYER_GARAGE_RETIRE_LOCK
    g_player_terminal_garage_lock_active = TRUE;
#endif
    MemoryBarrier();
    InterlockedExchange(&g_player_native_garage_result, 1);
    return TRUE;
#endif
}

static void __cdecl broker_player_tuple_restore_task(void* context) {
    char* player = NULL;
    uintptr_t epoch;
    (void)context;
    InterlockedIncrement(&g_player_tuple_callbacks);
    if (InterlockedCompareExchange(&g_player_tuple_armed, 0, 0) != 1 ||
        RACE_PHASE != 2 || GAME_STATE != 5) {
        InterlockedExchange(&g_player_tuple_pending, 0);
        return;
    }
    if (InterlockedCompareExchange(&g_player_tuple_armed, 2, 1) != 1) {
        InterlockedExchange(&g_player_tuple_pending, 0);
        return;
    }
    epoch = session_epoch();
    if (SESSION_TYPE != 7 || !g_player_out_active ||
        !g_player_out_committed || !epoch ||
        g_player_out_epoch != g_session_epoch_id ||
        epoch != g_roster_reduced_epoch || epoch != g_player_tuple_epoch ||
        !player_tuple_order_matches(&player) ||
        *(volatile int*)(player + OFF_SLOTLIVE) != g_player_out_role) {
        InterlockedExchange(&g_player_tuple_result, -1);
        InterlockedExchange(&g_player_tuple_armed, 0);
        InterlockedExchange(&g_player_tuple_pending, 0);
        return;
    }
    /* Final proof immediately before the only write block. The broker owns
     * the game thread, so the native loader cannot mutate this roster midway. */
    if (session_epoch() != epoch || VEHICLE_COUNT != g_player_tuple_order_count) {
        InterlockedExchange(&g_player_tuple_result, -2);
        InterlockedExchange(&g_player_tuple_armed, 0);
        InterlockedExchange(&g_player_tuple_pending, 0);
        return;
    }
    *(volatile int*)(player + OFF_FINISHSTATE) = g_player_out_finish;
    *(volatile int*)(player + OFF_REASONCODE) = g_player_out_reason;
    *(volatile BYTE*)(player + OFF_RETIREDFLAG) = g_player_out_retired;
    *(volatile BYTE*)(player + OFF_ACTIVEFLAG) = g_player_out_active_flag;
    *(volatile int*)(player + OFF_CURRENTLAP) = g_player_out_lap;
    PLAYER_OUT_FLAG = g_player_out_player_out;
    MemoryBarrier();
    if (session_epoch() != epoch ||
        !player_tuple_order_matches(&player) ||
        *(volatile int*)(player + OFF_SLOTLIVE) != g_player_out_role ||
        *(volatile int*)(player + OFF_FINISHSTATE) != g_player_out_finish ||
        *(volatile int*)(player + OFF_REASONCODE) != g_player_out_reason ||
        *(volatile BYTE*)(player + OFF_RETIREDFLAG) != g_player_out_retired ||
        *(volatile BYTE*)(player + OFF_ACTIVEFLAG) != g_player_out_active_flag ||
        *(volatile int*)(player + OFF_CURRENTLAP) != g_player_out_lap ||
        PLAYER_OUT_FLAG != g_player_out_player_out) {
        InterlockedExchange(&g_player_tuple_result, -3);
    } else if (!native_player_place_in_garage(player)) {
        InterlockedExchange(&g_player_tuple_result, -6);
    } else {
        InterlockedExchange(&g_player_tuple_result, 1);
    }
    InterlockedExchange(&g_player_tuple_armed, 0);
    InterlockedExchange(&g_player_tuple_pending, 0);
}

/* quiet=TRUE suppresses the per-car log line (used when re-sinking every tick). */
static void hide_dnf_cars_quiet(BOOL quiet) {
    /* v73: checked_scene_entry is an AIW GarPos/GarOri record, NOT a
     * render node. Never sink it or vehicle physics. Roster exclusion and
     * the existing constructor/update guards own restart suppression. */
    (void)quiet;
}

static void hide_dnf_cars(void) { hide_dnf_cars_quiet(FALSE); }

/* Undo every previous hide_dnf_cars(). Must run at the START of a new red-flag
 * sequence, BEFORE the snapshot: the game can only place a car in its pit box
 * if that car is not still sunk to Y=-1000 from the last sequence. The cars get
 * re-hidden in phase 3 as usual. */
static void unhide_all(void) {
    if (g_hidden_count == 0) return;
    if (!g_hidden_epoch || session_epoch() != g_hidden_epoch) {
        hidden_forget();
        return;
    }
    char buf[128];
    int n = 0;
    for (int i = 0; i < g_hidden_count; i++) {
        char* veh = (char*)g_hidden_ptrs[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE))
            continue;
        *(volatile float*)(veh + OFF_PHYS_Y) = g_hidden_phys_y[i];
        {
            char* entry = checked_scene_entry(veh);
            if (entry)
            *(volatile float*)(entry + 4) = g_hidden_node_y[i];
        }
        n++;
    }
    g_hidden_count = 0;
    g_hidden_epoch = 0;
    wsprintfA(buf, "RF-E6E1933195F %d"
                   "can place them this sequence.", n);
    log_w(buf);
}

static void snapshot_positions(void) {
    int count = VEHICLE_COUNT;
    /* The deployed SC is temporarily counted by native F1C. Its owner parks
     * and removes it during RF handover; it is never a starter or a DNF.
     * Match session_epoch's existing exact shared-identity exclusion. */
    uintptr_t sc = g_rc ? (uintptr_t)(uint32_t)f1c_rc_read(&g_rc->sc_vehicle) : 0;
    g_player_out_active = FALSE;
    g_player_out_committed = FALSE;
    g_player_out_ptr = NULL;
    g_player_out_epoch = 0;
    g_player_out_garage_valid = FALSE;
    g_player_out_pitstate = 0;
    g_player_out_player_out = 0;
    g_player_terminal_garage_lock_active = FALSE;
    g_player_tuple_order_count = 0;
    g_player_tuple_epoch = 0;
    InterlockedExchange(&g_player_tuple_armed, 0);
    InterlockedExchange(&g_player_tuple_pending, 0);
    InterlockedExchange(&g_player_tuple_result, 0);
    InterlockedExchange(&g_player_tuple_callbacks, 0);
    g_pitret_count=0;g_pit_arrived_count=0;g_strategy_contaminated_count=0;
    g_late_count   = 0;      /* fresh sequence: no pending late sweeps */
    kill_tracker_reset();
    g_dnf_count    = 0;
    g_active_count = 0;
    g_placed       = 0;

    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        if (sc && (uintptr_t)veh == sc) continue;
        int  finish  = *(volatile int*)(veh + OFF_FINISHSTATE);
        BYTE retired = *(volatile BYTE*)(veh + OFF_RETIREDFLAG);
        BOOL unclassified_terminal =
            (uintptr_t)veh != PLAYER_VEH_PTR && veh_crash_terminal(veh);
        if ((uintptr_t)veh == PLAYER_VEH_PTR &&
            capture_player_out(veh)) {
            /* Counted for camera/replay identity, but not a restart starter. */
        } else if (finish != 0 || retired != 0 || unclassified_terminal) {
            int n = g_dnf_count++;
            g_dnf_ptrs[n] = veh;
            g_dnf_qualpos[n] = *(volatile int*)(veh + OFF_QUALPOS);
            if (unclassified_terminal && finish == 0 && retired == 0) {
                char terminal_buf[176];
                wsprintfA(terminal_buf,
                    "RF-E3EEB84847F %d %s",
                    i, veh_name(veh));
                log_w(terminal_buf);
            }
        } else {
            int rp = *(volatile int*)(veh + OFF_RACEPOS);
            int j = g_active_count;
            while (j > 0 && g_active_rp[j-1] > rp) {
                g_active[j]    = g_active[j-1];
                g_active_rp[j] = g_active_rp[j-1];
                j--;
            }
            g_active[j]    = veh;
            g_active_rp[j] = rp;
            g_active_count++;
        }
    }
    g_placed = g_active_count;

    /* Save locked race positions for no-overtaking enforcement */
    g_max_driven_laps = 0;
    for (int i = 0; i < g_active_count; i++) {
        g_locked_racepos[i] = g_active_rp[i];
        g_saved_fuel[i] = *(volatile float*)(g_active[i] + OFF_FUELLEVEL);
        g_saved_driver_visible[i] =
            *(volatile BYTE*)(g_active[i] + OFF_DRIVER_VISIBLE) ? 1 : 0;
        g_saved_currentlap[i] = *(volatile int*)(g_active[i] + OFF_CURRENTLAP);
        if (g_saved_currentlap[i] > g_max_driven_laps)
            g_max_driven_laps = g_saved_currentlap[i];
    }
    /* True racing plan and legitimate request latch, captured before RF arms
     * any pit retirement or mutation of its own. */
    capture_restart_pit_strategy();
    g_saved_total_laps = LIVE_TOTAL_LAPS_PTR;
    g_penalty_count=0;
    g_penalty_valid=g_checkpoint_replay ? TRUE : rf_penalty_capture_sync();
    if(!g_penalty_valid)log_w("RF-E9B108516BB");

    char buf[128];
    wsprintfA(buf, "RF-EE2F1E40FEC %d %d %d %d",
              g_active_count, g_dnf_count, g_max_driven_laps, g_saved_total_laps);
    log_w(buf);

    /* Full field at trigger time, so a later LATE DNF can be judged against
     * what the car looked like when the red flag started. */
    for (int i = 0; i < g_active_count; i++) {
        char* veh = g_active[i];
        if (!veh) continue;
        wsprintfA(buf, "RF-EB2629FE4D3 %d %d %s %d %d %d %d %d",
                  i, veh_slot(veh), veh_name(veh),
                  *(volatile int*) (veh + OFF_ACTIVEFLAG),
                  *(volatile int*) (veh + OFF_FINISHSTATE),
                  *(volatile BYTE*)(veh + OFF_RETIREDFLAG),
                  g_active_rp[i], g_saved_currentlap[i]);
        log_w(buf);
    }
}

/* Cars that go terminal DURING the red-flag window.
 *
 * snapshot_positions() classifies DNF-vs-active ONCE, at trigger time. A car
 * wrecked during the 30s countdown therefore stayed in g_active and was handed
 * a grid slot — it reappeared on the restart grid instead of staying out.
 * Reported in-game 2026-07-24.
 *
 * Sweep g_active every tick and migrate anything that has since gone terminal
 * into g_dnf_ptrs, so apply_qualpos() gives it QualPos=200 and re_retire_dnf()
 * pins it in phase 3.
 *
 * Terminal = the same signal check_auto_redflag() uses (ActiveFlag -> 0), plus
 * an explicit FinishState/RetiredFlag check for cars the game classified itself.
 *
 * Exclusions:
 *   - the player: never remove him from his own restart grid.
 *   - g_pitret_ptrs (NUMPAD2): those are retired ON PURPOSE and get un-retired
 *     before the restart; sweeping them into g_dnf_ptrs would strand them.
 *     RF88: only while they are on the normal drive-to-box path, proven by
 *     having been seen in the pit lane (pittingStatus >= 2). A pit-armed car
 *     killed before it ever reaches the lane is a real late DNF and is
 *     swept like any other car. */
static BOOL veh_killed(char* veh);   /* defined with the auto-detect block */

static BOOL is_pitret(char* veh) {
    for (int i = 0; i < g_pitret_count; i++)
        if ((char*)g_pitret_ptrs[i] == veh) return TRUE;
    return FALSE;
}

static BOOL pit_retirement_exempt(char* veh) {
    if(!is_pitret(veh))return FALSE;
    if(!g_pit_arrived_ptrs)return TRUE;
    if(*(volatile int*)(veh+OFF_PITTING)>=2)pit_arrived_latch(veh);
    if(!pit_arrived_contains(veh))return FALSE;
    /* A killed car with independent severe damage is a wreck even after entry.
       Our own rear-wing flag and a normal stopped engine prove neither. */
    DWORD killed_for=kill_duration(veh);
    if(killed_for>=KILL_CONFIRM_MS && veh_heavy_damage_evidence(veh))return FALSE;
    F1CVehicleLifecycle life=rf_pit_lifecycle(veh);
    if(killed_for>=KILL_CONFIRM_MS && life.valid && life.garage_valid &&
       f1c_lifecycle_terminal(life.retired,life.finish) && !life.garage_clear &&
       *(volatile int*)(veh+OFF_PITTING)!=3)return FALSE;
    return TRUE;
}

static void sweep_late_dnf(void) {
    uintptr_t player = PLAYER_VEH_PTR;

    /* Player DQ is independent of the bulk-AI plausibility guard. Compact it
     * first so even a simultaneous mass-AI terminal sample cannot strand the
     * player in g_active when that guard returns early. */
    for (int r = 0; r < g_active_count; ++r) {
        char* veh = g_active[r];
        if (!veh || (uintptr_t)veh != player || !capture_player_out(veh))
            continue;
        for (int k = r; k + 1 < g_active_count; ++k) {
            g_active[k] = g_active[k + 1];
            g_active_rp[k] = g_active_rp[k + 1];
            g_locked_racepos[k] = g_locked_racepos[k + 1];
            g_saved_fuel[k] = g_saved_fuel[k + 1];
            g_saved_driver_visible[k] = g_saved_driver_visible[k + 1];
            g_saved_currentlap[k] = g_saved_currentlap[k + 1];
            g_saved_pit_strategy[k] = g_saved_pit_strategy[k + 1];
            g_saved_pit_valid[k] = g_saved_pit_valid[k + 1];
        }
        --g_active_count;
        g_placed = g_active_count;
        {
            char buf[192];
            wsprintfA(buf, "RF-E0ED2A49A28 %d %d",
                      g_player_out_finish, g_player_out_reason);
            log_w(buf);
        }
        break;
    }

    /* SANITY GUARD. A detection bug here is catastrophic and silent: gr32 read
     * ActiveFlag as a float, called every healthy car "terminal", and DNF-ed the
     * entire field on every red flag. Count first, and refuse to act if the
     * result is implausible — losing a legitimate late DNF is a far cheaper
     * failure than destroying the race. */
    int would_sweep = 0;
    for (int r = 0; r < g_active_count; r++) {
        char* veh = g_active[r];
        /* Player terminal state is processed independently in the mutation
         * pass below. It must never participate in the bulk-AI plausibility
         * guard, whose early return would otherwise leave a DQ player active. */
        if (!veh || (uintptr_t)veh == player) continue;
        if(pit_retirement_exempt(veh))continue;
        if (veh_killed(veh)
            || *(volatile int*)(veh + OFF_FINISHSTATE) != 0
            || *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) would_sweep++;
    }
    if (g_active_count >= 4 && would_sweep > g_active_count / 2) {
        if (!g_dnf_sweep_warning_logged) {
            char buf[144];
            wsprintfA(buf, "RF-EFBC85C1576 %d %d"
                           "at once - that is a detection fault, not a race "
                           "incident. Sweep disabled for this sequence.",
                      would_sweep, g_active_count);
            log_w(buf);
            g_dnf_sweep_warning_logged = TRUE;
        }
        return;
    }

    int w = 0;                                  /* write cursor for compaction */
    for (int r = 0; r < g_active_count; r++) {  /* read cursor */
        char* veh = g_active[r];
        if (!veh) continue;

        /* Classification, per the PROVEN gr17_CONFIRMED logic.
         *
         * History of getting this wrong, so nobody re-derives it:
         *  - gr32/33/34 swept on ActiveFlag AND forged FinishState/RetiredFlag/
         *    ActiveFlag immediately. The forge pre-empted the game and the wreck
         *    froze on track.
         *  - gr35 swept on ActiveFlag and forged nothing, but deferred QualPos to
         *    the end of the countdown — 30s in which the game gridded the car
         *    itself at QualPos 198.
         *  - gr37 swept only on FinishState/RetiredFlag. Too strict: a fresh
         *    wreck has ActiveFlag=0 but the game may not set FinishState for a
         *    long time, so the car was never classified at all and grouped on
         *    the straight.
         *
         * A crashed car is NEVER teleported to a garage by the game — that only
         * happens on the pit-retirement path (NUMPAD2/NUMPAD3). Wrecks are made
         * to disappear by re_retire_dnf() setting ActiveFlag=0, which ENDS THE
         * PHYSICS STEP, followed by hide_dnf_cars() moving the scene node and
         * physics Y to -1000. Physics off first, then the teleport sticks.
         * Both already run in phase 3, AFTER the restart, which is the safe
         * place to force flags — the game has no cleanup left to pre-empt.
         *
         * So: catch the wreck EARLY on the kill switch (ActiveFlag==0), accept
         * the game's own verdict too, and do exactly what gr17 does at
         * classification time — QualPos=200 + lap=0, written immediately. */
        BOOL terminal = FALSE;
        BOOL player_terminal = FALSE;
        if ((uintptr_t)veh == player) {
            player_terminal = capture_player_out(veh);
            terminal = player_terminal;
        } else {
            /* RF88: a pit-armed car is exempt only while it is on the normal
             * drive-to-box path, proven by having been seen in the pit lane.
             * One killed before it ever reaches the lane is a late DNF,
             * exactly like a car on the Countdown path. Unknown keeps the
             * exemption (the arrival latch is positive evidence only). */
            BOOL pit_exempt = pit_retirement_exempt(veh);
            if (!pit_exempt) {
                /* Sustained kill, not a single sample — see KILL_CONFIRM_MS. */
                if (kill_duration(veh) >= KILL_CONFIRM_MS)        terminal = TRUE;
                if (*(volatile int*)(veh + OFF_FINISHSTATE) != 0)  terminal = TRUE;
                if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) terminal = TRUE;
            }
        }

        if (terminal && player_terminal) {
            char buf[192];
            wsprintfA(buf, "RF-E1D2F423FD6 %d %d",
                      g_player_out_finish, g_player_out_reason);
            log_w(buf);
        } else if (terminal) {
            if (g_dnf_count < g_session_capacity) {
                int n = g_dnf_count++;
                g_dnf_ptrs[n] = veh;
                g_dnf_qualpos[n] = *(volatile int*)(veh + OFF_QUALPOS);
            }

            /* gr17_CONFIRMED writes these AT CLASSIFICATION, not later. Doing
             * it immediately closes the window in which the game can assign its
             * own grid slot (the QualPos=198 bug). apply_qualpos() rewrites the
             * same values later; that is harmless and keeps one source of
             * truth for the pre-existing DNFs. */
            /* Remember what we are about to overwrite. ActiveFlag==0 is only a
             * LEAD — a car can read act=0 transiently without ever being
             * retired. revalidate_dnf() undoes this if the game never confirms. */
            if (g_late_count < g_session_capacity) {
                g_late_ptrs[g_late_count]      = veh;
                g_late_saved_qp[g_late_count]  = *(volatile int*)(veh + OFF_QUALPOS);
                g_late_saved_lap[g_late_count] = *(volatile int*)(veh + OFF_CURRENTLAP);
                g_late_count++;
            }
            *(volatile int*)(veh + OFF_QUALPOS)    = DNF_QUALPOS;
            *(volatile int*)(veh + OFF_CURRENTLAP) = 0;
            /* CLASSIFY ONLY — do NOT force the retirement end-state here.
             *
             * gr32/gr33/gr34 wrote FinishState=2 / RetiredFlag=1 / ActiveFlag=0
             * the moment they spotted the damage. That short-circuits the
             * game's OWN retirement: it sees an already-retired car, never runs
             * its cleanup, and the wreck sits frozen on track instead of being
             * teleported to its garage (reported in-game 2026-07-24).
             *
             * Same trap as mods/ai_pit_retire — forging the end-state skips the
             * work the real path does for you. The game retires a terminally
             * damaged car by itself within a few seconds, garage placement
             * included. All this sweep needs to do is note that the car is out,
             * so apply_qualpos() denies it a grid slot. re_retire_dnf() in
             * phase 3 still pins it AFTER the restart, which is what keeps it
             * out of the new race. */
            /* Identity + the raw fields that triggered it. Without this the log
             * cannot tell "correctly binned a wreck" from "wrongly binned a
             * healthy car", which is exactly where the previous four builds
             * were debugged blind. */
            char buf[224];
            wsprintfA(buf, "RF-E16C1049AB3 %d %s %d %d %d"
                           "-> DNF list (%d active, %d DNF)",
                      veh_slot(veh), veh_name(veh),
                      *(volatile int*) (veh + OFF_ACTIVEFLAG),
                      *(volatile int*) (veh + OFF_FINISHSTATE),
                      *(volatile BYTE*)(veh + OFF_RETIREDFLAG),
                      g_active_count - 1, g_dnf_count);
            log_w(buf);
        } else {
            g_active[w]         = g_active[r];
            g_active_rp[w]      = g_active_rp[r];
            g_locked_racepos[w] = g_locked_racepos[r];
            g_saved_fuel[w]     = g_saved_fuel[r];
            g_saved_driver_visible[w] = g_saved_driver_visible[r];
            g_saved_currentlap[w] = g_saved_currentlap[r];
            g_saved_pit_strategy[w] = g_saved_pit_strategy[r];
            g_saved_pit_valid[w] = g_saved_pit_valid[r];
            w++;
        }
    }
    g_active_count = w;
    g_placed       = w;
}

/* --- anti-removal hold ------------------------------------------------
 *
 * The game removes a stationary wrecked car after VehicleRemoval seconds
 * (Matias.PLR, 10 by default): it retires the car (FinishState=2 +
 * RetiredFlag=1) and teleports it into its pit garage. Mid-countdown that
 * breaks the red-flag illusion - a real red flag leaves wrecks on track.
 * (The header comment above claiming the game never garages crashes is
 * superseded: the VehicleRemoval stationary path does exactly that, verified
 * in-game 2026-08-17.)
 *
 * POLICY: from the moment a red-flag sequence starts until the restart,
 * every DNF car still sitting on the track is this mod's property - the
 * countdown must not see them vanish into the garages. Every tick of the
 * countdown/pit-wait the hold reverts the game's retirement flags (the
 * removal cannot complete its sequence) and, if a teleport landed anyway,
 * restores the car's saved position. Garaged cars (pitstate >= 2, already
 * parked before the red flag) are left to the game - clearing their flags
 * would revive them. Released at sequence end; the game's normal removal
 * resumes, and after the restart re_retire_dnf()/hide_dnf_cars() take over. */
static float* g_held_px = NULL;          /* first-seen physics position    */
static float* g_held_py = NULL;
static float* g_held_pz = NULL;
static float* g_held_nx = NULL;          /* first-seen scene-node position */
static float* g_held_ny = NULL;
static float* g_held_nz = NULL;
static BOOL*  g_held_saved = NULL;

/* Detector scratch is session-sized too; no large variable-length arrays are
 * placed on the 32-bit worker stack. */
/* Stable vehicle identity, not slot/classification, survives roster movement.
 * Only members of a confirmed RF spatial proof contribute severity bits. */
typedef struct {char* vehicle; unsigned observed,handled;} RfHazardHistory;
static RfHazardHistory* g_hazard_history=NULL;
static int g_hazard_history_count=0;
static void hazard_observe(char* vehicle,unsigned severity) {
    if(!g_hazard_history || !vehicle || !severity)return;
    int i;
    for(i=0;i<g_hazard_history_count;i++)
        if(g_hazard_history[i].vehicle==vehicle)break;
    if(i==g_hazard_history_count) {
        if(i>=g_session_capacity)return; /* ambiguous replacement: no bypass */
        g_hazard_history[i]=(RfHazardHistory){vehicle,0,0};
        g_hazard_history_count++;
    }
    g_hazard_history[i].observed|=severity;
}
static void hazard_clear_observations(void) {
    for(int i=0;i<g_hazard_history_count;i++)g_hazard_history[i].observed=0;
}
static BOOL hazard_is_fresh(void) {
    for(int i=0;i<g_hazard_history_count;i++)
        if(g_hazard_history[i].observed & ~g_hazard_history[i].handled)return TRUE;
    return FALSE;
}
static uintptr_t hazard_signature(void) {
    uintptr_t value=0;
    for(int i=0;i<g_hazard_history_count;i++)if(g_hazard_history[i].observed)
        value+=((uintptr_t)g_hazard_history[i].vehicle>>4)*16777619u ^
            (uintptr_t)g_hazard_history[i].observed*2166136261u;
    return value;
}
static void hazard_mark_handled(void) {
    for(int i=0;i<g_hazard_history_count;i++)
        g_hazard_history[i].handled|=g_hazard_history[i].observed;
}
static int*   g_incident_points_scratch = NULL;
static BOOL*  g_incident_terminal_scratch = NULL;
static BOOL*  g_incident_damaged_scratch = NULL;
static BOOL*  g_incident_ontrack_scratch = NULL;
static float* g_incident_cx_scratch = NULL;
static float* g_incident_cy_scratch = NULL;
static float* g_incident_cz_scratch = NULL;
static float* g_incident_lapdist_scratch = NULL;

static void initial_grid_capture(void) {
    int n = VEHICLE_COUNT;
    g_initial_count = 0;
    if (!g_initial_ptrs || n < 2 || n > g_session_capacity) return;
    for (int i=0;i<n;++i) {
        char* v=(char*)VEHICLE_ARRAY[i];
        if (!v || !runtime_region_supported((uintptr_t)v, VEH_STRIDE, FALSE)) return;
        g_initial_ptrs[i]=v;
        g_initial_qp[i]=*(volatile int*)(v+OFF_QUALPOS);
    }
    g_initial_count=n;
}

/* Called by the existing broker restart guard on the game thread. Validate
 * the entire field before changing any qualifying position. */
static BOOL initial_grid_restore(void) {
    if (!g_initial_count) return !g_checkpoint_valid;
    if (VEHICLE_COUNT != g_initial_count || g_roster_exclusion_active) return FALSE;
    for (int i=0;i<g_initial_count;++i)
        if (VEHICLE_ARRAY[i] != g_initial_ptrs[i] ||
            !runtime_region_supported((uintptr_t)g_initial_ptrs[i], VEH_STRIDE, FALSE))
            return FALSE;
    for (int i=0;i<g_initial_count;++i)
        *(volatile int*)((char*)g_initial_ptrs[i]+OFF_QUALPOS)=g_initial_qp[i];
    return TRUE;
}

static __attribute__((noinline, optimize("O0"))) BOOL checkpoint_capture(void) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("checkpoint_capture");

    int n=VEHICLE_COUNT;
    g_checkpoint_valid=FALSE;
    if (!g_checkpoint_cars || !g_saved_pit_strategy || !g_saved_pit_valid ||
        n != g_session_capacity || g_active_count < 1 ||
        g_active_count+g_dnf_count+(g_player_out_active ? 1 : 0) != n) { rf_protected_result = (FALSE); goto rf_protected_exit; }
    for (int i=0;i<n;++i) {
        if (!VEHICLE_ARRAY[i]) { rf_protected_result = (FALSE); goto rf_protected_exit; }
        int owners=0;
        for (int j=0;j<n;++j)
            if (i != j && VEHICLE_ARRAY[i] == VEHICLE_ARRAY[j]) { rf_protected_result = (FALSE); goto rf_protected_exit; }
        for (int j=0;j<g_active_count;++j)
            if (VEHICLE_ARRAY[i] == (void*)g_active[j]) ++owners;
        for (int j=0;j<g_dnf_count;++j)
            if (VEHICLE_ARRAY[i] == g_dnf_ptrs[j]) ++owners;
        if (g_player_out_active && VEHICLE_ARRAY[i] == (void*)g_player_out_ptr) ++owners;
        if (owners != 1) { rf_protected_result = (FALSE); goto rf_protected_exit; }
        g_checkpoint_cars[i].full=(void*)VEHICLE_ARRAY[i];
    }
    for (int i=0;i<g_active_count;++i) {
        RfRestartCheckpointCar* c=&g_checkpoint_cars[i];
        c->active=g_active[i]; c->rank=g_active_rp[i];
        c->grid=*(volatile int*)(g_active[i]+OFF_QUALPOS);
        c->lap=g_saved_currentlap[i]; c->fuel=g_saved_fuel[i];
        c->visible=g_saved_driver_visible[i];
        c->strategy=g_saved_pit_strategy[i];
        c->strategy_valid=g_saved_pit_valid[i];
        F1CRestartPenalties* penalties=penalty_snapshot_for(g_active[i]);
        if(!penalties || !g_penalty_valid){ rf_protected_result = (FALSE); goto rf_protected_exit; }
        c->penalties=*penalties;
    }
    for (int i=0;i<g_dnf_count;++i) {
        g_checkpoint_cars[i].dnf=g_dnf_ptrs[i];
        g_checkpoint_cars[i].dnf_qp=g_dnf_qualpos[i];
    }
    g_checkpoint_player.g_player_out_active = g_player_out_active;
    g_checkpoint_player.g_player_out_committed = g_player_out_committed;
    g_checkpoint_player.g_player_out_ptr = g_player_out_ptr;
    g_checkpoint_player.g_player_out_epoch = g_player_out_epoch;
    g_checkpoint_player.g_player_out_finish = g_player_out_finish;
    g_checkpoint_player.g_player_out_reason = g_player_out_reason;
    g_checkpoint_player.g_player_out_lap = g_player_out_lap;
    g_checkpoint_player.g_player_out_racepos = g_player_out_racepos;
    g_checkpoint_player.g_player_out_qualpos = g_player_out_qualpos;
    g_checkpoint_player.g_player_out_role = g_player_out_role;
    g_checkpoint_player.g_player_out_retired = g_player_out_retired;
    g_checkpoint_player.g_player_out_active_flag = g_player_out_active_flag;
    g_checkpoint_player.g_player_out_player_out = g_player_out_player_out;
    g_checkpoint_player.g_player_out_garage_valid = g_player_out_garage_valid;
    g_checkpoint_player.g_player_out_px = g_player_out_px;
    g_checkpoint_player.g_player_out_py = g_player_out_py;
    g_checkpoint_player.g_player_out_pz = g_player_out_pz;
    g_checkpoint_player.g_player_out_nx = g_player_out_nx;
    g_checkpoint_player.g_player_out_ny = g_player_out_ny;
    g_checkpoint_player.g_player_out_nz = g_player_out_nz;
    g_checkpoint_player.g_player_out_pitstate = g_player_out_pitstate;
    g_checkpoint_count=n; g_checkpoint_active=g_active_count;
    g_checkpoint_dnf=g_dnf_count; g_checkpoint_total=g_saved_total_laps;
    g_checkpoint_maxlap=g_max_driven_laps; g_checkpoint_serial=g_session_serial;
    MemoryBarrier(); g_checkpoint_valid=TRUE;
    log_w("RF-EF775092585");
    { rf_protected_result = (TRUE); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

/* Broker holds the game thread at RestartGate while this worker transaction
 * prepares the existing survivor-only native restart. No snapshot of today's
 * retirements/order/fuel may replace the checkpoint. Native restart resets
 * cars that retired AFTER this checkpoint, making them starters again. */
static __attribute__((noinline, optimize("O0"))) BOOL checkpoint_restore(void) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("checkpoint_restore");

    if (!g_checkpoint_valid || g_checkpoint_serial != g_session_serial ||
        !g_saved_pit_strategy || !g_saved_pit_valid ||
        VEHICLE_COUNT != g_checkpoint_count || g_roster_exclusion_active) { rf_protected_result = (FALSE); goto rf_protected_exit; }
    for (int i=0;i<g_checkpoint_count;++i)
        if (VEHICLE_ARRAY[i] != g_checkpoint_cars[i].full ||
            !runtime_region_supported((uintptr_t)g_checkpoint_cars[i].full, VEH_STRIDE, FALSE))
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
    /* Reset transient per-restart hooks/late sweeps using the established
     * initializer, then discard its live snapshot in favor of saved data. */
    g_checkpoint_replay=TRUE; /* game thread waits here; never post a capture */
    snapshot_positions();
    g_player_out_active = g_checkpoint_player.g_player_out_active;
    g_player_out_committed = g_checkpoint_player.g_player_out_committed;
    g_player_out_ptr = g_checkpoint_player.g_player_out_ptr;
    g_player_out_epoch = g_checkpoint_player.g_player_out_epoch;
    g_player_out_finish = g_checkpoint_player.g_player_out_finish;
    g_player_out_reason = g_checkpoint_player.g_player_out_reason;
    g_player_out_lap = g_checkpoint_player.g_player_out_lap;
    g_player_out_racepos = g_checkpoint_player.g_player_out_racepos;
    g_player_out_qualpos = g_checkpoint_player.g_player_out_qualpos;
    g_player_out_role = g_checkpoint_player.g_player_out_role;
    g_player_out_retired = g_checkpoint_player.g_player_out_retired;
    g_player_out_active_flag = g_checkpoint_player.g_player_out_active_flag;
    g_player_out_player_out = g_checkpoint_player.g_player_out_player_out;
    g_player_out_garage_valid = g_checkpoint_player.g_player_out_garage_valid;
    g_player_out_px = g_checkpoint_player.g_player_out_px;
    g_player_out_py = g_checkpoint_player.g_player_out_py;
    g_player_out_pz = g_checkpoint_player.g_player_out_pz;
    g_player_out_nx = g_checkpoint_player.g_player_out_nx;
    g_player_out_ny = g_checkpoint_player.g_player_out_ny;
    g_player_out_nz = g_checkpoint_player.g_player_out_nz;
    g_player_out_pitstate = g_checkpoint_player.g_player_out_pitstate;
    if (g_player_out_active) g_player_out_epoch=g_session_epoch_id;
    g_active_count=g_checkpoint_active; g_placed=g_active_count;
    g_dnf_count=g_checkpoint_dnf;
    g_penalty_count=g_checkpoint_active;g_penalty_valid=TRUE; g_late_count=0;
    for (int i=0;i<g_active_count;++i) {
        const RfRestartCheckpointCar* c=&g_checkpoint_cars[i];
        g_penalty_snapshot[i].vehicle=c->active;g_penalty_snapshot[i].state=c->penalties;
        g_active[i]=c->active; g_active_rp[i]=c->rank;
        g_locked_racepos[i]=c->rank; g_saved_currentlap[i]=c->lap;
        g_saved_fuel[i]=c->fuel; g_saved_driver_visible[i]=c->visible;
        g_saved_pit_strategy[i]=c->strategy; g_saved_pit_valid[i]=c->strategy_valid;
    }
    for (int i=0;i<g_dnf_count;++i) {
        g_dnf_ptrs[i]=g_checkpoint_cars[i].dnf;
        g_dnf_qualpos[i]=g_checkpoint_cars[i].dnf_qp;
    }
    g_saved_total_laps=g_checkpoint_total; g_max_driven_laps=g_checkpoint_maxlap;
    g_checkpoint_replay=TRUE;
    log_w("RF-E6E0E2FBBAD");
    { rf_protected_result = (TRUE); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

static void session_arrays_release(void) {
#define RF_FREE_ARRAY(name) do { free(name); name = NULL; } while (0)
    g_checkpoint_valid = FALSE;
    g_initial_count = 0;
    RF_FREE_ARRAY(g_checkpoint_cars);
    RF_FREE_ARRAY(g_penalty_snapshot);g_penalty_count=0;g_penalty_valid=FALSE;
    RF_FREE_ARRAY(g_initial_ptrs);
    RF_FREE_ARRAY(g_initial_qp);
    RF_FREE_ARRAY(g_removal_repair_ptrs);
    RF_FREE_ARRAY(g_removal_repair_saved);
    RF_FREE_ARRAY(g_post_restart_pit_fixed);
    RF_FREE_ARRAY(g_last_killed);
    RF_FREE_ARRAY(g_term_times);
    RF_FREE_ARRAY(g_incident_since);
    RF_FREE_ARRAY(g_stationary_hazard_since);
    RF_FREE_ARRAY(g_inverted_hazard_since);
    RF_FREE_ARRAY(g_late_saved_qp);
    RF_FREE_ARRAY(g_late_saved_lap);
    RF_FREE_ARRAY(g_late_ptrs);
    RF_FREE_ARRAY(g_kill_ptrs);
    RF_FREE_ARRAY(g_kill_since);
    RF_FREE_ARRAY(g_dnf_ptrs);
    RF_FREE_ARRAY(g_dnf_qualpos);
    RF_FREE_ARRAY(g_roster_ptrs);
    RF_FREE_ARRAY(g_roster_full_ptrs);
    RF_FREE_ARRAY(g_roster_saved_roles);
    RF_FREE_ARRAY(g_roster_saved_finish);
    RF_FREE_ARRAY(g_roster_saved_reason);
    RF_FREE_ARRAY(g_roster_saved_lap);
    RF_FREE_ARRAY(g_roster_saved_racepos);
    RF_FREE_ARRAY(g_player_tuple_order);
    RF_FREE_ARRAY(g_hidden_ptrs);
    RF_FREE_ARRAY(g_hidden_node_y);
    RF_FREE_ARRAY(g_hidden_phys_y);
    RF_FREE_ARRAY(g_active);
    RF_FREE_ARRAY(g_active_rp);
    RF_FREE_ARRAY(g_np_saved);
    RF_FREE_ARRAY(g_np_pass);
    RF_FREE_ARRAY(g_np_aggro);
    RF_FREE_ARRAY(g_np_lastw);
    RF_FREE_ARRAY(g_np_armed);
    RF_FREE_ARRAY(g_np_hist);
    RF_FREE_ARRAY(g_locked_racepos);
    RF_FREE_ARRAY(g_saved_fuel);
    RF_FREE_ARRAY(g_saved_driver_visible);
    RF_FREE_ARRAY(g_saved_currentlap);
    RF_FREE_ARRAY(g_saved_pit_strategy);
    RF_FREE_ARRAY(g_saved_pit_valid);
    RF_FREE_ARRAY(g_pitret_ptrs);
    RF_FREE_ARRAY(g_pit_arrived_ptrs);
    RF_FREE_ARRAY(g_strategy_contaminated_ptrs);
    RF_FREE_ARRAY(g_garage_ptrs);
    RF_FREE_ARRAY(g_garage_px);
    RF_FREE_ARRAY(g_garage_py);
    RF_FREE_ARRAY(g_garage_pz);
    RF_FREE_ARRAY(g_garage_nx);
    RF_FREE_ARRAY(g_garage_ny);
    RF_FREE_ARRAY(g_garage_nz);
    RF_FREE_ARRAY(g_held_px);
    RF_FREE_ARRAY(g_held_py);
    RF_FREE_ARRAY(g_held_pz);
    RF_FREE_ARRAY(g_held_nx);
    RF_FREE_ARRAY(g_held_ny);
    RF_FREE_ARRAY(g_held_nz);
    RF_FREE_ARRAY(g_held_saved);
    RF_FREE_ARRAY(g_hazard_history);g_hazard_history_count=0;
    RF_FREE_ARRAY(g_incident_points_scratch);
    RF_FREE_ARRAY(g_incident_terminal_scratch);
    RF_FREE_ARRAY(g_incident_damaged_scratch);
    RF_FREE_ARRAY(g_incident_ontrack_scratch);
    RF_FREE_ARRAY(g_incident_cx_scratch);
    RF_FREE_ARRAY(g_incident_cy_scratch);
    RF_FREE_ARRAY(g_incident_cz_scratch);
    RF_FREE_ARRAY(g_incident_lapdist_scratch);
#undef RF_FREE_ARRAY
    g_session_capacity = 0;
}

static BOOL session_arrays_allocate(int count) {
    if (count < 2) return FALSE;
    session_arrays_release();
#define RF_ALLOC_ARRAY(name) do { \
        name = calloc((size_t)count, sizeof(*name)); \
        if (!(name)) goto allocation_failed; \
    } while (0)
    RF_ALLOC_ARRAY(g_checkpoint_cars);
    RF_ALLOC_ARRAY(g_penalty_snapshot);
    RF_ALLOC_ARRAY(g_initial_ptrs);
    RF_ALLOC_ARRAY(g_initial_qp);
    RF_ALLOC_ARRAY(g_removal_repair_ptrs);
    RF_ALLOC_ARRAY(g_removal_repair_saved);
    RF_ALLOC_ARRAY(g_post_restart_pit_fixed);
    RF_ALLOC_ARRAY(g_last_killed);
    RF_ALLOC_ARRAY(g_term_times);
    RF_ALLOC_ARRAY(g_incident_since);
    RF_ALLOC_ARRAY(g_stationary_hazard_since);
    RF_ALLOC_ARRAY(g_inverted_hazard_since);
    RF_ALLOC_ARRAY(g_late_saved_qp);
    RF_ALLOC_ARRAY(g_late_saved_lap);
    RF_ALLOC_ARRAY(g_late_ptrs);
    RF_ALLOC_ARRAY(g_kill_ptrs);
    RF_ALLOC_ARRAY(g_kill_since);
    RF_ALLOC_ARRAY(g_dnf_ptrs);
    RF_ALLOC_ARRAY(g_dnf_qualpos);
    RF_ALLOC_ARRAY(g_roster_ptrs);
    RF_ALLOC_ARRAY(g_roster_full_ptrs);
    RF_ALLOC_ARRAY(g_roster_saved_roles);
    RF_ALLOC_ARRAY(g_roster_saved_finish);
    RF_ALLOC_ARRAY(g_roster_saved_reason);
    RF_ALLOC_ARRAY(g_roster_saved_lap);
    RF_ALLOC_ARRAY(g_roster_saved_racepos);
    RF_ALLOC_ARRAY(g_player_tuple_order);
    RF_ALLOC_ARRAY(g_hidden_ptrs);
    RF_ALLOC_ARRAY(g_hidden_node_y);
    RF_ALLOC_ARRAY(g_hidden_phys_y);
    RF_ALLOC_ARRAY(g_active);
    RF_ALLOC_ARRAY(g_active_rp);
    RF_ALLOC_ARRAY(g_np_saved);
    RF_ALLOC_ARRAY(g_np_pass);
    RF_ALLOC_ARRAY(g_np_aggro);
    RF_ALLOC_ARRAY(g_np_lastw);
    RF_ALLOC_ARRAY(g_np_armed);
    RF_ALLOC_ARRAY(g_np_hist);
    RF_ALLOC_ARRAY(g_locked_racepos);
    RF_ALLOC_ARRAY(g_saved_fuel);
    RF_ALLOC_ARRAY(g_saved_driver_visible);
    RF_ALLOC_ARRAY(g_saved_currentlap);
    RF_ALLOC_ARRAY(g_saved_pit_strategy);
    RF_ALLOC_ARRAY(g_saved_pit_valid);
    RF_ALLOC_ARRAY(g_pitret_ptrs);
    RF_ALLOC_ARRAY(g_pit_arrived_ptrs);
    RF_ALLOC_ARRAY(g_strategy_contaminated_ptrs);
    RF_ALLOC_ARRAY(g_garage_ptrs);
    RF_ALLOC_ARRAY(g_garage_px);
    RF_ALLOC_ARRAY(g_garage_py);
    RF_ALLOC_ARRAY(g_garage_pz);
    RF_ALLOC_ARRAY(g_garage_nx);
    RF_ALLOC_ARRAY(g_garage_ny);
    RF_ALLOC_ARRAY(g_garage_nz);
    RF_ALLOC_ARRAY(g_held_px);
    RF_ALLOC_ARRAY(g_held_py);
    RF_ALLOC_ARRAY(g_held_pz);
    RF_ALLOC_ARRAY(g_held_nx);
    RF_ALLOC_ARRAY(g_held_ny);
    RF_ALLOC_ARRAY(g_held_nz);
    RF_ALLOC_ARRAY(g_held_saved);
    RF_ALLOC_ARRAY(g_hazard_history);g_hazard_history_count=0;
    RF_ALLOC_ARRAY(g_incident_points_scratch);
    RF_ALLOC_ARRAY(g_incident_terminal_scratch);
    RF_ALLOC_ARRAY(g_incident_damaged_scratch);
    RF_ALLOC_ARRAY(g_incident_ontrack_scratch);
    RF_ALLOC_ARRAY(g_incident_cx_scratch);
    RF_ALLOC_ARRAY(g_incident_cy_scratch);
    RF_ALLOC_ARRAY(g_incident_cz_scratch);
    RF_ALLOC_ARRAY(g_incident_lapdist_scratch);
#undef RF_ALLOC_ARRAY
    g_session_capacity = count;
    return TRUE;

allocation_failed:
#undef RF_ALLOC_ARRAY
    session_arrays_release();
    log_w("RF-EAC95F86D29");
    return FALSE;
}

static void hold_wrecks_on_track(void) {
    int nodeTable = *(volatile int*)(SCENE_STRUCT + OFF_NODETABLE);
    int dim       = *(volatile int*)(SCENE_STRUCT + OFF_DIM);
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        /* Only on-track dead cars. The removal targets stationary cars on
         * the circuit; a garaged car must keep its retirement. */
        if (*(volatile int*)(veh + OFF_ACTIVEFLAG) != 0) continue;
        if (*(volatile int*)(veh + OFF_PITSTATE) >= 2) continue;

        /* The visible car is the SCENE NODE, not the physics position: the
         * game's removal teleports the node and physics re-syncs from it, so
         * restoring only 0x34E0 left the car warping between the two. Pin
         * both (same node layout hide_dnf_cars uses). */
        char* entry = NULL;
        if (nodeTable != 0 && dim != 0) {
            int nodeIdx = *(volatile int*)(veh + OFF_NODEIDX);
            int subIdx  = *(volatile int*)(veh + OFF_SUBIDX);
            entry = (char*)(nodeTable + 28 * (nodeIdx + subIdx * dim));
        }
        if (!g_held_saved[i]) {
            g_held_px[i] = *(volatile float*)(veh + OFF_POSX);
            g_held_py[i] = *(volatile float*)(veh + OFF_POSY);
            g_held_pz[i] = *(volatile float*)(veh + OFF_POSZ);
            if (entry) {
                g_held_nx[i] = *(volatile float*)(entry + 0);
                g_held_ny[i] = *(volatile float*)(entry + 4);
                g_held_nz[i] = *(volatile float*)(entry + 8);
            }
            g_held_saved[i] = TRUE;
        }
        if (*(volatile int*)(veh + OFF_FINISHSTATE) != 0 ||
            *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) {
            *(volatile int*)(veh + OFF_FINISHSTATE)  = 0;
            *(volatile BYTE*)(veh + OFF_RETIREDFLAG) = 0;
        }
        float px = *(volatile float*)(veh + OFF_POSX);
        float py = *(volatile float*)(veh + OFF_POSY);
        float pz = *(volatile float*)(veh + OFF_POSZ);
        float dx = px - g_held_px[i];
        float dy = py - g_held_py[i];
        float dz = pz - g_held_pz[i];
        if (dx*dx + dy*dy + dz*dz > 1.0f) {   /* the game teleported it */
            *(volatile float*)(veh + OFF_POSX) = g_held_px[i];
            *(volatile float*)(veh + OFF_POSY) = g_held_py[i];
            *(volatile float*)(veh + OFF_POSZ) = g_held_pz[i];
        }
    }
}

/* Undo LATE sweeps the game never confirmed.
 *
 * sweep_late_dnf() commits on ActiveFlag==0, which is a LEAD, not a verdict: a
 * healthy car can read act=0 transiently. gr41 binned Nick Heidfeld that way —
 * still fin=0 ret=0 ten seconds later at restart, but stamped QualPos=200, so
 * the game gridded him LAST. That is the "cars at the end of the grid" bug
 * (reported in-game 2026-07-24).
 *
 * Run immediately before apply_qualpos(): any late-swept car the game has NOT
 * classified by now (fin==0 && ret==0) was a false positive. Restore its
 * QualPos/lap and drop it from the DNF list. Genuine wrecks reach fin/ret
 * within a few seconds, well inside the countdown, so they are unaffected. */
static void revalidate_dnf(void) {
    char buf[192];
    for (int i = 0; i < g_late_count; i++) {
        char* veh = (char*)g_late_ptrs[i];
        if (!veh) continue;
        int  fin = *(volatile int*) (veh + OFF_FINISHSTATE);
        BYTE ret = *(volatile BYTE*)(veh + OFF_RETIREDFLAG);
        if (fin != 0 || ret != 0) continue;          /* game confirmed — keep out */

        /* Still physics-dead? Then it IS wrecked, the game simply has not
         * stamped fin/ret yet — its classification delay is variable and can
         * exceed the countdown. Only a car that RECOVERED (act back to 1) was a
         * false positive. gr42 undid Hakkinen here purely because the game was
         * slow, and put a wreck back on the grid. */
        if (veh_killed(veh)) continue;

        *(volatile int*)(veh + OFF_QUALPOS)    = g_late_saved_qp[i];
        *(volatile int*)(veh + OFF_CURRENTLAP) = g_late_saved_lap[i];

        int w = 0;                                    /* drop from DNF list */
        for (int r = 0; r < g_dnf_count; r++) {
            if (g_dnf_ptrs[r] == (void*)veh) continue;
            g_dnf_ptrs[w] = g_dnf_ptrs[r];
            g_dnf_qualpos[w] = g_dnf_qualpos[r];
            w++;
        }
        g_dnf_count = w;

        wsprintfA(buf, "RF-E6D6991F7BD %d %s"
                       "positive, restoring QP=%d lap=%d, "
                       "back in the race",
                  veh_slot(veh), veh_name(veh),
                  g_late_saved_qp[i], g_late_saved_lap[i]);
        log_w(buf);
    }
    g_late_count = 0;
}

/* Hold the restart until the game has finished classifying every car we swept.
 *
 * Placement at restart is decided by FinishState/RetiredFlag, NOT by QualPos:
 * a car the game still considers a runner gets a GRID SLOT, and QualPos only
 * orders it within the grid. So firing RESTART_FLAG while a wreck is still
 * unclassified puts it back in the race.
 *
 * gr43 avoided this only by luck of timing — the 4s kill-confirm plus the rest
 * of the countdown happened to give the game enough time. Nothing enforced it,
 * and a car wrecked late in the countdown could still slip through. This makes
 * the assumption explicit and bounded.
 *
 * Returns TRUE when it is safe to restart (all DNF cars classified, or the wait
 * budget is spent — we restart anyway rather than hang the sequence). */
#define DNF_SETTLE_MAX_MS 2000
#define VEHICLE_REMOVAL_CLEANUP_MAX_MS 10000
#define WHEELLOSS_REMOVAL_GRACE_MS 2000
#define VEHICLE_REMOVAL_STABLE_MS 300

static BOOL dnf_settled(DWORD waited_ms) {
    int pending = 0;
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        if (*(volatile int*)(veh + OFF_FINISHSTATE) == 0
            && *(volatile BYTE*)(veh + OFF_RETIREDFLAG) == 0) pending++;
    }
    if (pending == 0) return TRUE;

    if (waited_ms >= DNF_SETTLE_MAX_MS) {
        char buf[160];
        wsprintfA(buf, "RF-EE6DC7A052A %d %d"
                       "unclassified - they may be gridded. Restarting anyway.",
                  (int)waited_ms, pending);
        log_w(buf);
        return TRUE;
    }
    return FALSE;
}

static int dnf_waiting_for_native_garage(void) {
    int pending = 0;
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        /* RetiredFlag is the live-confirmed visual garage discriminator. */
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) == 0) pending++;
    }
    return pending;
}

/* F1C can leave wheel-less cars physics-dead in the road without ever setting
 * RetiredFlag. carConditions>=2 is the validated wheel-loss/severe-damage
 * bucket used by both the DLL and GUI. Return true only when every remaining
 * ungaraged DNF is in that bucket; mixed/ordinary removals retain the full
 * native cleanup budget. */
static BOOL all_pending_dnfs_are_wheelless(void) {
    int pending = 0;
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
        pending++;
        if (*(volatile int*)(veh + OFF_CAR_CONDITIONS) < 2) return FALSE;
    }
    return pending > 0;
}

static int roster_find(char* veh) {
    for (int i = 0; i < g_roster_count; ++i)
        if (g_roster_ptrs[i] == (void*)veh) return i;
    return -1;
}

/* Remove every cumulative DNF from the exact restart-construction roster.
 *
 * This is the live-proven v33 transaction: role -1 -> native pointer-array
 * rebuild -> publish only the exact survivor count. v37 later restores the
 * complete entrant view from the game-thread hook at the first live update,
 * after grid construction and before that hooked update continues. */
static __attribute__((noinline, optimize("O0"))) BOOL exclude_dnfs_from_restart_roster(void) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("exclude_dnfs_from_restart_roster");

    BuildActiveVehiclePointerArrayFn rebuild =
        (BuildActiveVehiclePointerArrayFn)BUILD_ACTIVE_VEHICLE_POINTER_ARRAY;
    int old_count;
    int old_roster_count = g_roster_count;
    int newly_excluded = 0;
    char buf[224];

    /* Last pre-mutation guard also covers PostRedFlag Y/checkpoint callers. */
    if (g_player_out_active || !restart_player_supported()) {
        log_w("RF-E556A93C59D");
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
    old_count = VEHICLE_COUNT;
    revalidate_dnf();
    if (old_count < 2 || old_count > g_session_capacity) {
        log_w("RF-ECC4D1D25DE");
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
    if (g_player_out_active) {
        (void)capture_player_out(g_player_out_ptr);
        if (!player_out_is_counted()) {
            log_w("RF-EC293CE6144");
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
        }
        if (!player_out_native_state_matches()) {
            log_w("RF-E9DABE98F90");
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
        }
        if (g_player_out_player_out != 0) {
            log_w("RF-E2ADA9035A2");
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
        }
        if (g_active_count < 2) {
            log_w("RF-E0D31066FE8");
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
        }
    }
    if (g_dnf_count <= 0) {
        g_player_out_committed = g_player_out_active;
        /* Even without AI roster compaction, saved survivor pointers belong to
         * this exact ordered counted field and must survive Game_Restart. */
        hud_position_denominator_freeze(old_count);
        g_roster_reduced_epoch = session_epoch();
        { rf_protected_result = (TRUE); goto rf_protected_exit; }
    }
    /* Preserve the complete ordered entrant view before the native builder
     * compacts it. Green restoration writes this array first and publishes the
     * larger count last; no partially rebuilt roster can be observed. */
    for (int i = 0; i < old_count; ++i)
        g_roster_full_ptrs[i] = (void*)VEHICLE_ARRAY[i];

    for (int i = 0; i < g_dnf_count; ++i) {
        char* veh = (char*)g_dnf_ptrs[i];
        BOOL in_live_roster = FALSE;
        if (!veh || (uintptr_t)veh == PLAYER_VEH_PTR ||
            !runtime_region_supported((uintptr_t)(veh + OFF_SLOTLIVE),
                                      sizeof(int), FALSE)) {
            log_w("RF-E01A98C9C50");
            goto rollback;
        }
        if (roster_find(veh) >= 0) continue;
        for (int j = 0; j < old_count; ++j) {
            if (VEHICLE_ARRAY[j] == (void*)veh) {
                in_live_roster = TRUE;
                break;
            }
        }
        if (!in_live_roster || g_roster_count >= g_session_capacity) {
            log_w("RF-E577A20785F");
            goto rollback;
        }
        g_roster_ptrs[g_roster_count] = veh;
        g_roster_saved_roles[g_roster_count] =
            *(volatile int*)(veh + OFF_SLOTLIVE);
        g_roster_saved_finish[g_roster_count] =
            *(volatile int*)(veh + OFF_FINISHSTATE);
        g_roster_saved_reason[g_roster_count] =
            *(volatile int*)(veh + OFF_REASONCODE);
        g_roster_saved_lap[g_roster_count] =
            *(volatile int*)(veh + OFF_CURRENTLAP);
        g_roster_saved_racepos[g_roster_count] =
            *(volatile int*)(veh + OFF_RACEPOS);
        ++g_roster_count;
        ++newly_excluded;
    }

    if (!g_roster_original_count) g_roster_original_count = old_count;
    if (old_count != g_roster_original_count) {
        log_w("RF-E2483502F52");
        goto rollback;
    }
    if (old_count - g_roster_count < 2) {
        log_w("RF-E1176A38E3B");
        goto rollback;
    }

    hud_position_denominator_freeze(old_count);
    for (int i = 0; i < g_roster_count; ++i)
        *(volatile int*)((char*)g_roster_ptrs[i] + OFF_SLOTLIVE) = -1;

    {
        int rebuilt_count = rebuild();
        int expected_count = old_count - g_roster_count;
        if (rebuilt_count != expected_count) {
            wsprintfA(buf, "RF-E5936813A44 %d %d",
                      rebuilt_count, expected_count);
            log_w(buf);
            for (int i = 0; i < g_roster_count; ++i)
                *(volatile int*)((char*)g_roster_ptrs[i] + OFF_SLOTLIVE) =
                    g_roster_saved_roles[i];
            (void)rebuild();
            g_roster_count = old_roster_count;
            hud_position_denominator_release();
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
        }
        VEHICLE_COUNT = rebuilt_count;
        g_roster_reduced_epoch = session_epoch();
    }

    g_roster_exclusion_active = TRUE;
    g_roster_results_restored = FALSE;
    InterlockedExchange(&g_green_restore_armed, 0);
    InterlockedExchange(&g_green_restore_result, 0);
    InterlockedExchange(&g_post_green_continuity_armed, 0);
    InterlockedExchange(&g_post_green_continuity_pending, 0);
    InterlockedExchange(&g_post_green_continuity_result, 0);
    InterlockedExchange(&g_green_restore_posts, 0);
    InterlockedExchange(&g_green_restore_callbacks, 0);
    InterlockedExchange(&g_green_restore_post_failures, 0);
    InterlockedExchange(&g_green_restore_last_post_result, 0);
    g_player_out_committed = g_player_out_active;
    wsprintfA(buf, "RF-E0E490D2C26 %d %d %d %d",
              old_count, VEHICLE_COUNT, newly_excluded, g_roster_count);
    log_w(buf);
    for (int i = old_roster_count; i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        wsprintfA(buf, "RF-EA016DDE9D3 %d %s %d %d %d %d",
                  veh_slot(veh), veh_name(veh), g_roster_saved_roles[i],
                  g_roster_saved_finish[i], g_roster_saved_reason[i],
                  g_roster_saved_lap[i]);
        log_w(buf);
    }
    { rf_protected_result = (TRUE); goto rf_protected_exit; }

rollback:
    for (int i = old_roster_count; i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        if (veh)
            *(volatile int*)((char*)g_roster_ptrs[i] + OFF_SLOTLIVE) =
                g_roster_saved_roles[i];
    }
    g_roster_count = old_roster_count;
    { rf_protected_result = (FALSE); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

/* Called only through the installed SessionTickUpdate call-site trampoline.
 * The DNF tuple is written while every saved role is still -1. Only after all
 * cars are already vanilla-shaped retirees are roles and the saved ordered
 * pointer array restored; the larger count is published last. Thus the game
 * cannot observe a counted active DNF or a partially restored pointer array. */
static BOOL lap_restore_dnf_contains(char* veh) {
    for (int i = 0; g_roster_exclusion_active && i < g_roster_count; ++i) {
        if (g_roster_ptrs[i] == veh) return TRUE;
    }
    return FALSE;
}

/* Restore the exact trigger-time lap value for every surviving driver and the
 * original event total. Called once when GRID_READY becomes visible and again
 * on the proven game-thread green edge because F1C may reset these fields at
 * either transition. Each driver owns a distinct captured value; no field-wide
 * leader value and no fixed example lap count is used. */
static int restore_individual_lap_continuity(void) {
    int restored = 0;
    if (SESSION_TYPE != 7 || g_active_count <= 0 ||
        !g_saved_currentlap || g_saved_total_laps < 1)
        return -1;

    for (int i = 0; i < g_active_count; ++i) {
        char* veh = g_active[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE) ||
            g_saved_currentlap[i] < 0 ||
            g_saved_currentlap[i] > g_saved_total_laps)
            return -1;
    }
    for (int i = 0; i < g_active_count; ++i) {
        char* veh = g_active[i];
        if (lap_restore_dnf_contains(veh)) continue;
        *(volatile int*)(veh + OFF_CURRENTLAP) = g_saved_currentlap[i];
        ++restored;
    }
    RESTART_TOTAL_LAPS_PTR = g_saved_total_laps;
    LIVE_TOTAL_LAPS_PTR = g_saved_total_laps;
    MemoryBarrier();
    return restored;
}

/* Native mode selector ABI is reviewed in the SDK evidence. This helper is
 * called only from the existing broker green/post-green restoration callbacks. */
static BOOL restore_penalties_on_game_thread(void) {
    typedef int (__attribute__((thiscall)) *ModeFn)(void*,char);
    if(!rf_penalty_refresh())return FALSE;
    if(!g_penalty_valid || !g_penalty_snapshot || SESSION_TYPE!=7 || RACE_PHASE!=5 ||
       !g_rc || f1c_rc_read(&g_rc->owner)!=F1C_RC_RF ||
       !runtime_region_supported(F1C_RESTART_PENALTY_MODE_ADDRESS,65,TRUE) ||
       !f1c_restart_penalty_mode_code_valid((const void*)F1C_RESTART_PENALTY_MODE_ADDRESS,65))return FALSE;
    /* Full preflight: no partial-field restore after a stale survivor identity. */
    for(int i=0;i<g_active_count;i++) {
        char* v=g_active[i];F1CRestartPenalties* state=penalty_snapshot_for(v);
        if(!v || !state || !f1c_restart_penalties_valid(state) ||
           !runtime_region_supported((uintptr_t)v,VEH_STRIDE,FALSE) || veh_slot(v)<0 ||
           *(volatile int*)(v+OFF_FINISHSTATE)!=0 || *(volatile BYTE*)(v+OFF_RETIREDFLAG)!=0 ||
           *(volatile uintptr_t*)(v+F1C_OFF_lapNumber-8)!=(uintptr_t)v)return FALSE;
    }
    for(int i=0;i<g_active_count;i++) {
        char* v=g_active[i];F1CRestartPenalties* state=penalty_snapshot_for(v);
        if(!f1c_restart_penalties_restore(v,VEH_STRIDE,state))return FALSE;
        ((ModeFn)F1C_RESTART_PENALTY_MODE_ADDRESS)(v+F1C_RESTART_PENALTY_PIT_OFFSET,(char)(state->count>0));
    }
    /* Normal RF retains its sealed broker ledger through the constructor;
       checkpoint replay starts a hold here for the continuity pass. */
    if(!g_penalty_hold_active && !rf_penalty_begin_on_game_thread())return FALSE;
    log_w("RF-EB1D5994E30");
    return TRUE;
}
/* Keep the announcement snapshot as an upper bound. Native pit retirement
 * resets AI tanks; only observed consumption before service may reduce fuel.
 * Never interpret terminal/box service transients as race consumption. */
static void sample_restart_race_fuel(void) {
    if(g_checkpoint_replay || !g_saved_fuel || g_active_count<1 || g_active_count>104 ||
       SESSION_TYPE!=7 || RACE_PHASE!=5 || PAUSE_FLAG)return;
    for(int i=0;i<g_active_count;i++) {
        char *v=g_active[i];float current;
        if(!v || veh_slot(v)<0 || !runtime_region_supported((uintptr_t)v,VEH_STRIDE,FALSE))continue;
        int pit=*(volatile int*)(v+OFF_PITTING);
        if(pit<0 || pit>=3 || *(volatile int*)(v+OFF_FINISHSTATE)!=0 ||
           *(volatile BYTE*)(v+OFF_RETIREDFLAG)!=0 || pit_arrived_contains(v))continue;
        if(f1c_restart_fuel_target(v,VEH_STRIDE,0,&current) &&
           f1c_restart_fuel_value_valid(v,VEH_STRIDE,g_saved_fuel[i]) && current<g_saved_fuel[i])
            g_saved_fuel[i]=current;
    }
}
static BOOL capture_restart_fuel_service(void) {
    float targets[104];
    if(g_checkpoint_replay)return TRUE;
    if(!g_saved_fuel || g_active_count<1 || g_active_count>104 || SESSION_TYPE!=7 || RACE_PHASE!=5 ||
       !g_rc || f1c_rc_read(&g_rc->owner)!=F1C_RC_RF)return FALSE;
    for(int i=0;i<g_active_count;i++) {
        char *v=g_active[i];
        if(!v || veh_slot(v)<0 || !runtime_region_supported((uintptr_t)v,VEH_STRIDE,FALSE) ||
           *(volatile int*)(v+OFF_FINISHSTATE)!=0 || *(volatile BYTE*)(v+OFF_RETIREDFLAG)!=0 ||
           !f1c_restart_fuel_value_valid(v,VEH_STRIDE,g_saved_fuel[i])) {
            log_w("RF-E6B22F41828");return FALSE;
        }
        targets[i]=g_saved_fuel[i];
    }
    memcpy(g_saved_fuel,targets,(size_t)g_active_count*sizeof targets[0]);
    for(int i=0;i<g_active_count;i++) {
        float observed;char message[144];
        int current_ml=f1c_restart_fuel_target(g_active[i],VEH_STRIDE,0,&observed)?(int)(observed*1000.0f):-1;
        wsprintfA(message,"RACE FUEL RETAINED: slot=%d saved_ml=%d native_ml=%d.",
            veh_slot(g_active[i]),(int)(targets[i]*1000.0f),current_ml);log_w(message);
    }
    for(int i=0;i<g_active_count;i++)if(veh_slot(g_active[i])==0) {
        char message[112];
        wsprintfA(message,"PLAYER FUEL SAVED: millilitres=%d.",(int)(targets[i]*1000.0f));
        log_w(message);
    }
    log_w("RF-E8C52B1F716");
    return TRUE;
}
static BOOL restart_fuel_preflight(void) {
    int reason=0, failed=-1;
    LONG owner=g_rc?f1c_rc_read(&g_rc->owner):-1;
    if(!g_saved_fuel || g_active_count<=0 || g_active_count>104)reason=1;
    else if(SESSION_TYPE!=7 || (RACE_PHASE!=5 && !(RACE_PHASE==4 &&
            InterlockedCompareExchange(&g_green_restore_armed,0,0)==1)))reason=2;
    else if(owner!=F1C_RC_RF)reason=3;
    else if(!runtime_region_supported(F1C_RESTART_FUEL_ADDRESS,33,TRUE))reason=4;
    else if(!f1c_restart_fuel_code_valid((const void*)F1C_RESTART_FUEL_ADDRESS,33))reason=5;
    for(int i=0;!reason && i<g_active_count;i++) {
        char* v=g_active[i];
        if(!v || !runtime_region_supported((uintptr_t)v,VEH_STRIDE,FALSE) || veh_slot(v)<0) {
            reason=6;failed=i;
        } else if(!f1c_restart_fuel_value_valid(v,VEH_STRIDE,g_saved_fuel[i])) {
            reason=7;failed=i;
        }
    }
    if(reason) {
        char message[224];
        wsprintfA(message,"FUEL PREFLIGHT: reason=%d survivor=%d owner=%d phase=%d session=%d armed=%d count=%d.",
            reason,failed,(int)owner,RACE_PHASE,SESSION_TYPE,
            (int)InterlockedCompareExchange(&g_green_restore_armed,0,0),g_active_count);
        log_w(message);
        if(reason==7) {
            float capacity=*(float*)(g_active[failed]+F1C_RESTART_FUEL_CAPACITY_OFFSET);
            wsprintfA(message,"FUEL VALUE: saved_ml=%d capacity_ml=%d finite=%d/%d.",
                isfinite(g_saved_fuel[failed])?(int)(g_saved_fuel[failed]*1000):-1,
                isfinite(capacity)?(int)(capacity*1000):-1,
                isfinite(g_saved_fuel[failed])!=0,isfinite(capacity)!=0);log_w(message);
        }
        if(reason==5) {
            const unsigned char* code=(const unsigned char*)F1C_RESTART_FUEL_ADDRESS;
            for(int i=0;i<33;i++) {
                wsprintfA(message,"FUEL CODE: offset=%d byte=%02X.",i,(unsigned)code[i]);log_w(message);
            }
        }
        return FALSE;
    }
    return TRUE;
}
/* Capture the true racing plan (count/entries/cursor) and the legitimate
 * native request latch for every survivor at red-flag trigger time, before RF
 * arms pit retirement or lets any native service rewrite the plan. Called
 * from snapshot_positions() on the worker path, like the fuel/lap snapshot;
 * this is a read. A car whose block is unreadable or out of the reviewed
 * bounds is marked invalid and keeps its native post-restart plan. Never
 * blocks the restart: refusing to preserve a plan must not cancel the RF. */
static void capture_restart_pit_strategy(void) {
    int kept=0,refused=0;
    if(!g_saved_pit_strategy || !g_saved_pit_valid) return;
    for(int i=0;i<g_active_count && i<104;i++) g_saved_pit_valid[i]=0;
    if(g_checkpoint_replay)return;
    if(g_active_count<1 || g_active_count>104 || SESSION_TYPE!=7) {
        log_w("RF-E42ABBB2187");
        return;
    }
    for(int i=0;i<g_active_count;i++) {
        char *v=g_active[i];F1CPitStrategyState state;char message[176];
        if(!v || veh_slot(v)<0 || !runtime_region_supported((uintptr_t)v,VEH_STRIDE,FALSE) ||
           !f1c_pit_strategy_capture(v,VEH_STRIDE,&state)) {
            if(refused<8) {
                wsprintfA(message,"PIT STRATEGY REFUSED: slot=%d unsupported or out-of-bounds plan; native plan retained for this car.",
                          v?veh_slot(v):-1);log_w(message);
            }
            ++refused;continue;
        }
        g_saved_pit_strategy[i]=state;g_saved_pit_valid[i]=1;++kept;
        wsprintfA(message,"PIT STRATEGY CAPTURED: slot=%d stops=%d cursor=%d next_lap=%d request=%d.",
                  veh_slot(v),state.count,state.cursor,(int)state.entries[state.cursor][0],
                  state.call_to_box);
        log_w(message);
    }
    {char message[176];
     wsprintfA(message,"PIT STRATEGY SNAPSHOT: %d survivor plans and request latches retained, %d refused.",kept,refused);
     log_w(message);}
}
/* Game-thread refresh immediately before the native restart. A car RF armed
 * for pit retirement has had its own plan rewritten by the forced-retirement
 * service path (PitStopDecisionTireCompound rewrites the current entry and
 * PostPitCleanup advances the cursor); that is parking, not race progress, and
 * the trigger-time snapshot is held for exactly those cars. Ownership is the
 * dedicated per-sequence contamination list, which survives the
 * DriveToPits->Countdown timeout fallback's retirement/arrival clears. Cars
 * RF never contaminated may have completed a genuine stop during the window,
 * so their live plan is accepted when it only moves progress forward. Reads
 * only; per-car refusals keep the snapshot state. */
static void refresh_restart_pit_strategy(void) {
    int refreshed=0,held=0,unavailable=0;
    if(g_checkpoint_replay || !g_saved_pit_strategy || !g_saved_pit_valid ||
       g_active_count<1 || g_active_count>104 || SESSION_TYPE!=7) return;
    for(int i=0;i<g_active_count;i++) {
        char *v=g_active[i];F1CPitStrategyState live;
        if(!g_saved_pit_valid[i]) continue;
        if(!v || veh_slot(v)<0 || !runtime_region_supported((uintptr_t)v,VEH_STRIDE,FALSE)) {
            ++unavailable;continue;
        }
        /* Positive RF ownership for the whole sequence: the live plan may
         * contain forced-retirement parking mutations. */
        if(strategy_contaminated_contains(v)) { ++held; continue; }
        if(!f1c_pit_strategy_capture(v,VEH_STRIDE,&live)) { ++unavailable; continue; }
        if(live.cursor < g_saved_pit_strategy[i].cursor) { ++held; continue; }
        g_saved_pit_strategy[i]=live; ++refreshed;
    }
    {char message[176];
     wsprintfA(message,"PIT STRATEGY REFRESH: %d survivor plans confirmed, %d RF-armed plans held, %d unavailable.",
               refreshed,held,unavailable);
     log_w(message);}
}
/* Called only on the broker game thread, including the rebuilt starting grid. */
static BOOL restore_fuel_on_game_thread(void) {
    if(!restart_fuel_preflight())return FALSE;
    /* GCC 15.2 folds the constant-address callback path to refusal. Retain a
     * runtime-loaded function pointer; exact native bytes remain checked. */
    F1CRestartFuelSet volatile native_setter=(F1CRestartFuelSet)F1C_RESTART_FUEL_ADDRESS;
    for(int i=0;i<g_active_count;i++)
        if(!f1c_restart_fuel_apply(g_active[i],VEH_STRIDE,g_saved_fuel[i],
                (const void*)F1C_RESTART_FUEL_ADDRESS,33,
                native_setter))return FALSE;
    return TRUE;
}
static void restore_grid_fuel_on_game_thread(void) {
    if(RACE_PHASE!=4 || InterlockedCompareExchange(&g_green_restore_armed,0,0)!=1 ||
       InterlockedCompareExchange(&g_grid_fuel_restored,2,0)!=0)return;
    BOOL valid_epoch=!g_roster_reduced_epoch || session_epoch()==g_roster_reduced_epoch;
    if(!valid_epoch) {
        char message[144];
        wsprintfA(message,"GRID FUEL EPOCH: expected=%08X observed=%08X.",
            (unsigned)g_roster_reduced_epoch,(unsigned)session_epoch());log_w(message);
    }
    BOOL restored=valid_epoch && restore_fuel_on_game_thread();
    InterlockedExchange(&g_grid_fuel_restored,restored?1:-1);
    log_w(restored ? "GRID FUEL: native player physics and AI fuel restored before lights out." :
                    "GRID FUEL REFUSED: saved field/epoch/native layout invalid; green preflight remains required.");
}
/* --- live replay roster guard -------------------------------------------
 * Native instant replay saves exactly VehicleCount entries (LoadingPhase1
 * 004b87c0), reconciles all 104 physical slots and destroys an existing
 * vehicle with no selected replay identity while unconditionally decrementing
 * VehicleCount (ProcessDriverEntries 004b8da0 - a parked Safety Car sits at
 * +13000 == -1 and has no replay identity), then restores with the shrunken
 * count (LoadingPhase2 004b8940), skipping the last saved entry. RF keeps the
 * last provably coherent live-race roster, captured on the game thread only,
 * and on replay exit restores the native fields the replay loader itself
 * saves/restores (control word, +4189 visibility and the +18620 render bit via
 * the reviewed SetDriverVisibility helper) for exactly the entries whose
 * reviewed identity fields still match. Pointers, excluded vehicles and DNF
 * tuples are never touched; any mismatch fails closed. */
static void replay_roster_guard_reset(void) {
    g_replay_base_count = 0;
    g_replay_base_valid = FALSE;
    g_replay_base_epoch = 0;
    g_replay_last_mode = RF_REPLAY_MODE;
    g_replay_last_capture = 0;
    InterlockedExchange(&g_replay_capture_pending, 0);
    InterlockedExchange(&g_replay_restore_pending, 0);
    InterlockedExchange(&g_replay_restore_result, 0);
}
/* R1: the native replay count shrink is not a session teardown. It is
 * covered only while playback is active (or a restore is queued), the epoch
 * is unchanged and every captured identity still verifies. A real field
 * rebuild or session change fails these checks and takes the cold path. */
static BOOL replay_roster_transition_covers(int observed_count) {
    if (RF_REPLAY_MODE != F1C_REPLAY_MODE_PLAYBACK &&
        InterlockedCompareExchange(&g_replay_restore_pending, 0, 0) == 0) return FALSE;
    if (!g_replay_base_valid || g_replay_base_count < 2 ||
        g_replay_base_epoch != g_session_epoch_id) return FALSE;
    if (observed_count < 2 || observed_count > g_replay_base_count) return FALSE;
    for (int i = 0; i < g_replay_base_count; ++i) {
        char* v = (char*)VEHICLE_ARRAY[i];
        char name[F1C_REPLAY_NAME_BYTES];
        if (v != (char*)g_replay_base_ptr[i] || !v ||
            !runtime_region_supported((uintptr_t)v, VEH_STRIDE, FALSE))
            return FALSE;
        memcpy(name, v + F1C_REPLAY_DRIVER_NAME_OFF, F1C_REPLAY_NAME_BYTES);
        name[F1C_REPLAY_NAME_BYTES - 1] = 0;
        if (*(volatile uintptr_t*)(v + F1C_REPLAY_SELF_OFF) != (uintptr_t)v ||
            *(volatile int*)(v + F1C_REPLAY_ROSTER_OFF) != g_replay_base_roster[i] ||
            *(volatile uintptr_t*)(v + F1C_REPLAY_SCENE_ROOT_OFF) != g_replay_base_scene[i] ||
            memcmp(name, g_replay_base_name[i], F1C_REPLAY_NAME_BYTES) != 0)
            return FALSE;
    }
    return TRUE;
}
/* Game-thread capture. Samples are taken with game logic quiesced inside the
 * broker pump, so a replay transition can never interleave with the reads.
 * Replay states (3) or an empty identity reject the whole sample. */
static void __cdecl broker_replay_capture_task(void* context) {
    (void)context;
    if (InterlockedCompareExchange(&g_replay_capture_pending, 0, 0) != 1) return;
    if (RF_REPLAY_MODE == F1C_REPLAY_MODE_PLAYBACK ||
        !g_running || !g_session_active || !g_session_epoch_id ||
        SESSION_TYPE != 7 || RACE_PHASE != 5 ||
        g_roster_exclusion_active || g_post_constructor_hold ||
        InterlockedCompareExchange(&g_replay_restore_pending, 0, 0) != 0) {
        InterlockedExchange(&g_replay_capture_pending, 0);
        return;
    }
    int count = VEHICLE_COUNT;
    int valid = count >= 2 && count <= RF_REPLAY_MAX && count <= g_session_capacity;
    for (int i = 0; valid && i < count; ++i) {
        char* v = (char*)VEHICLE_ARRAY[i];
        if (!v || !runtime_region_supported((uintptr_t)v, VEH_STRIDE, FALSE) ||
            *(volatile uintptr_t*)(v + F1C_REPLAY_SELF_OFF) != (uintptr_t)v)
            valid = 0;
    }
    if (!valid || VEHICLE_COUNT != count) {
        g_replay_base_valid = FALSE;
        g_replay_base_count = 0;
        InterlockedExchange(&g_replay_capture_pending, 0);
        return;
    }
    for (int i = 0; i < count; ++i) {
        char* v = (char*)VEHICLE_ARRAY[i];
        int state = *(volatile int*)(v + F1C_REPLAY_VEHICLE_STATE_OFF);
        BYTE visible = *(volatile BYTE*)(v + F1C_REPLAY_VISIBLE_A_OFF) ? 1 : 0;
        int render = (int)((*(volatile uint32_t*)(v + F1C_REPLAY_VISIBLE_B_OFF) >> 1) & 1u);
        uintptr_t scene = *(volatile uintptr_t*)(v + F1C_REPLAY_SCENE_ROOT_OFF);
        char name[F1C_REPLAY_NAME_BYTES];
        memcpy(name, v + F1C_REPLAY_DRIVER_NAME_OFF, F1C_REPLAY_NAME_BYTES);
        name[F1C_REPLAY_NAME_BYTES - 1] = 0;
        if (state == 3 || state < -1 || visible > 1 || !scene || name[0] == 0) {
            g_replay_base_valid = FALSE;
            g_replay_base_count = 0;
            InterlockedExchange(&g_replay_capture_pending, 0);
            return;
        }
        g_replay_base_ptr[i] = v;
        g_replay_base_state[i] = state;
        g_replay_base_visible[i] = visible;
        g_replay_base_render[i] = render;
        g_replay_base_roster[i] = *(volatile int*)(v + F1C_REPLAY_ROSTER_OFF);
        g_replay_base_scene[i] = scene;
        memcpy(g_replay_base_name[i], name, F1C_REPLAY_NAME_BYTES);
    }
    g_replay_base_count = count;
    g_replay_base_epoch = g_session_epoch_id;
    g_replay_base_valid = TRUE;
    g_replay_last_capture = GetTickCount();
    InterlockedExchange(&g_replay_capture_pending, 0);
}
static void __cdecl broker_replay_roster_restore_task(void* context) {
    (void)context;
    const char* fail = NULL;
    char message[224];
    if (InterlockedCompareExchange(&g_replay_restore_pending, 0, 0) != 1) return;
    int base = g_replay_base_count;
    int count = VEHICLE_COUNT;
    int render_needed = 0;
    if (!g_replay_base_valid || base < 2 || base > RF_REPLAY_MAX) fail = "no valid baseline";
    else if (RF_REPLAY_MODE == F1C_REPLAY_MODE_PLAYBACK) fail = "replay active again";
    else if (!g_running || !g_session_active || SESSION_TYPE != 7 || RACE_PHASE != 5 ||
             g_roster_exclusion_active || g_post_constructor_hold ||
             g_session_epoch_id != g_replay_base_epoch) fail = "non-live roster context";
    else if (count < 2 || count > base) fail = "count outside the captured range";
    if (!fail) {
        for (int i = 0; i < base; ++i) {
            char* v = (char*)VEHICLE_ARRAY[i];
            char name[F1C_REPLAY_NAME_BYTES];
            if (v != (char*)g_replay_base_ptr[i] || !v ||
                !runtime_region_supported((uintptr_t)v, VEH_STRIDE, FALSE)) {
                fail = (i < count) ? "counted identity changed" : "last baseline identity changed";
                break;
            }
            memcpy(name, v + F1C_REPLAY_DRIVER_NAME_OFF, F1C_REPLAY_NAME_BYTES);
            name[F1C_REPLAY_NAME_BYTES - 1] = 0;
            if (*(volatile uintptr_t*)(v + F1C_REPLAY_SELF_OFF) != (uintptr_t)v ||
                *(volatile int*)(v + F1C_REPLAY_ROSTER_OFF) != g_replay_base_roster[i] ||
                *(volatile uintptr_t*)(v + F1C_REPLAY_SCENE_ROOT_OFF) != g_replay_base_scene[i] ||
                memcmp(name, g_replay_base_name[i], F1C_REPLAY_NAME_BYTES) != 0) {
                fail = (i < count) ? "counted vehicle identity replaced"
                                   : "last baseline vehicle identity replaced";
                break;
            }
        }
    }
    if (!fail) {
        for (int i = 0; i < base; ++i) {
            char* v = (char*)g_replay_base_ptr[i];
            if ((int)((*(volatile uint32_t*)(v + F1C_REPLAY_VISIBLE_B_OFF) >> 1) & 1u) !=
                g_replay_base_render[i]) render_needed = 1;
        }
        if (render_needed && !g_replay_visibility_fn)
            fail = "render visibility helper unavailable";
    }
    if (fail) {
        wsprintfA(message, "REPLAY ROSTER GUARD FAILED CLOSED: %s; native roster retained (count=%d baseline=%d).",
                  fail, count, base);
        log_w(message);
        g_replay_base_valid = FALSE;
        g_replay_base_count = 0;
        InterlockedExchange(&g_replay_restore_result, -1);
        InterlockedExchange(&g_replay_restore_pending, 0);
        return;
    }
    int controls = 0, visibility = 0;
    for (int i = 0; i < base; ++i) {
        char* v = (char*)g_replay_base_ptr[i];
        if (*(volatile int*)(v + F1C_REPLAY_VEHICLE_STATE_OFF) != g_replay_base_state[i]) {
            *(volatile int*)(v + F1C_REPLAY_VEHICLE_STATE_OFF) = g_replay_base_state[i];
            ++controls;
        }
        BYTE want = g_replay_base_visible[i] ? 1 : 0;
        if ((*(volatile BYTE*)(v + F1C_REPLAY_VISIBLE_A_OFF) ? 1 : 0) != want) {
            *(volatile BYTE*)(v + F1C_REPLAY_VISIBLE_A_OFF) = want;
            ++controls;
        }
        if ((int)((*(volatile uint32_t*)(v + F1C_REPLAY_VISIBLE_B_OFF) >> 1) & 1u) !=
            g_replay_base_render[i]) {
            g_replay_visibility_fn(v, (char)(g_replay_base_render[i] ? 1 : 0));
            ++visibility;
        }
    }
    if (fail) {
        wsprintfA(message, "REPLAY ROSTER GUARD FAILED CLOSED: %s; native roster retained (count=%d baseline=%d).",
                  fail, count, base);
        log_w(message);
        g_replay_base_valid = FALSE;
        g_replay_base_count = 0;
        InterlockedExchange(&g_replay_restore_result, -1);
        InterlockedExchange(&g_replay_restore_pending, 0);
        return;
    }
    int repaired = 0;
    if (count < base) {
        MemoryBarrier();
        VEHICLE_COUNT = base;
        repaired = base - count;
    }
    wsprintfA(message, "REPLAY ROSTER GUARD: %d counted entries retained, %d restored to the live baseline, %d control/visibility restores; excluded vehicles untouched.",
              base, repaired, controls + visibility);
    log_w(message);
    InterlockedExchange(&g_replay_restore_result, (repaired || controls || visibility) ? 1 : 0);
    InterlockedExchange(&g_replay_restore_pending, 0);
}
/* First live tick after playback ends: queue the game-thread restore before
 * the generic session watchdog can interpret the shrunk count as a teardown. */
static void replay_roster_guard_exit(void) {
    if (!g_replay_base_valid) {
        log_w("RF-E761CEB8E57");
        return;
    }
    if (InterlockedCompareExchange(&g_replay_restore_pending, 1, 0) != 0) return;
    int post = (!g_broker_registered || !g_broker_post || !g_broker_control_token) ? -1
        : g_broker_post(g_broker_control_token, broker_replay_roster_restore_task, NULL);
    if (post != RF_BROKER_OK) {
        InterlockedExchange(&g_replay_restore_pending, 0);
        log_w("RF-E5E1069E9BD");
    } else {
        InterlockedIncrement(&g_replay_restore_posts);
    }
}
/* One explicit replay lifecycle tick, evaluated before the session watchdog:
 * observe entry, hold covered playback changes, queue the restore on the first
 * live tick, then refresh the baseline only while fully live. */
static void replay_roster_guard_lifecycle_tick(int phase) {
    int mode = RF_REPLAY_MODE;
    if (g_replay_last_mode == F1C_REPLAY_MODE_PLAYBACK &&
        mode != F1C_REPLAY_MODE_PLAYBACK)
        replay_roster_guard_exit();
    if (mode == F1C_REPLAY_MODE_PLAYBACK) {
        g_replay_last_mode = mode;   /* hold the pre-replay baseline */
        return;
    }
    g_replay_last_mode = mode;
    replay_roster_guard_poll(phase);
}
static void replay_roster_guard_poll(int phase) {
    if (InterlockedCompareExchange(&g_replay_restore_pending, 0, 0) != 0) return;
    if (!g_running || !g_session_active || !g_session_epoch_id ||
        SESSION_TYPE != 7 || RACE_PHASE != 5 || phase != 0 ||
        g_roster_exclusion_active || g_post_constructor_hold ||
        InterlockedCompareExchange(&g_green_restore_armed, 0, 0) != 0 ||
        InterlockedCompareExchange(&g_broker_green_task_pending, 0, 0) != 0) {
        /* RF or the session owns the roster now: no baseline may be applied. */
        g_replay_base_valid = FALSE;
        g_replay_base_count = 0;
        return;
    }
    /* Capture only on the game thread at a bounded cadence. */
    if (InterlockedCompareExchange(&g_replay_capture_pending, 0, 0) == 0 &&
        (!g_replay_base_valid ||
         (DWORD)(GetTickCount() - g_replay_last_capture) >= RF_REPLAY_CAPTURE_MS)) {
        if (!g_broker_registered || !g_broker_post || !g_broker_control_token) return;
        if (InterlockedCompareExchange(&g_replay_capture_pending, 1, 0) == 0) {
            if (g_broker_post(g_broker_control_token, broker_replay_capture_task, NULL) != RF_BROKER_OK)
                InterlockedExchange(&g_replay_capture_pending, 0);
        }
    }
}
static void __cdecl broker_green_restore_task(void* context) {
    (void)context;
    BOOL restore_roster;
    BOOL publish_roster;
    InterlockedIncrement(&g_green_restore_callbacks);
    restore_grid_fuel_on_game_thread();
    if (InterlockedCompareExchange(&g_green_restore_armed, 0, 0) != 1 ||
        RACE_PHASE != 5) {
        InterlockedExchange(&g_broker_green_task_pending, 0);
        return;
    }
    if (InterlockedCompareExchange(&g_green_restore_armed, 2, 1) != 1) {
        InterlockedExchange(&g_broker_green_task_pending, 0);
        return;
    }

    restore_roster = g_roster_exclusion_active;
    publish_roster = restore_roster;
#if RF_DIAGNOSTIC_KEEP_SURVIVOR_ROSTER
    if (restore_roster) publish_roster = FALSE;
#endif
    /* Prove exact ordered pointer ownership before even interpreting count
     * arithmetic. A count change is itself an epoch failure, not a generic
     * fallback condition. This also covers terminal-player restarts with no AI
     * DNF compaction. */
    if (g_roster_reduced_epoch &&
        session_epoch() != g_roster_reduced_epoch) {
        InterlockedExchange(&g_green_restore_result, -8);
        InterlockedExchange(&g_green_restore_armed, 0);
        InterlockedExchange(&g_broker_green_task_pending, 0);
        return;
    }
    if (SESSION_TYPE != 7 || g_active_count <= 0 ||
        !g_saved_currentlap || !g_saved_fuel || !g_saved_driver_visible ||
        g_saved_total_laps < 1 ||
        (restore_roster &&
         (g_roster_count <= 0 || g_roster_original_count < 2 ||
          VEHICLE_COUNT + g_roster_count != g_roster_original_count))) {
        InterlockedExchange(&g_green_restore_result, -1);
        InterlockedExchange(&g_green_restore_armed, 0);
        InterlockedExchange(&g_broker_green_task_pending, 0);
        return;
    }
    /* VehicleCount alone is not a lifetime proof. Game_Restart must also
     * retain the exact ordered survivor pointer array used to capture every
     * saved lap. Refuse before dereferencing any saved vehicle pointer. */
    if (g_player_out_active &&
#if RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE
        (!player_out_restored_tuple_matches() ||
         !player_tuple_order_matches(NULL))) {
#else
        !player_out_native_state_matches()) {
#endif
        InterlockedExchange(&g_green_restore_result, -7);
        InterlockedExchange(&g_green_restore_armed, 0);
        InterlockedExchange(&g_broker_green_task_pending, 0);
        return;
    }

    /* Validate the complete lap transaction before changing any race state.
     * These are the exact survivor pointers and per-driver lap values captured
     * when the Red Flag began; no example/fixed lap number is involved. */
    for (int i = 0; i < g_active_count; ++i) {
        char* veh = g_active[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE) ||
            !isfinite(g_saved_fuel[i]) || g_saved_fuel[i] < 0.0f ||
            g_saved_fuel[i] > 1000.0f || g_saved_driver_visible[i] > 1 ||
            g_saved_currentlap[i] < 0 ||
            g_saved_currentlap[i] > g_saved_total_laps) {
            InterlockedExchange(&g_green_restore_result, -5);
            InterlockedExchange(&g_green_restore_armed, 0);
            InterlockedExchange(&g_broker_green_task_pending, 0);
            return;
        }
    }
    if(!restart_fuel_preflight()) {
        log_w("RF-E76D2328D4B");
        InterlockedExchange(&g_green_restore_result,-11);
        InterlockedExchange(&g_green_restore_armed,0);
        InterlockedExchange(&g_broker_green_task_pending,0);
        return;
    }
    for (int i = 0; publish_roster && i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE)) {
            InterlockedExchange(&g_green_restore_result, -2);
            InterlockedExchange(&g_green_restore_armed, 0);
            InterlockedExchange(&g_broker_green_task_pending, 0);
            return;
        }
    }
    for (int i = 0; publish_roster && i < g_roster_original_count; ++i) {
        char* veh = (char*)g_roster_full_ptrs[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE)) {
            InterlockedExchange(&g_green_restore_result, -3);
            InterlockedExchange(&g_green_restore_armed, 0);
            InterlockedExchange(&g_broker_green_task_pending, 0);
            return;
        }
    }

    /* The worker may cancel only an unclaimed state-1 request. Once this
     * callback owns state 2 it is the sole transaction publisher. Recheck
     * immediately before the first game-memory write. */
    if (InterlockedCompareExchange(&g_green_restore_armed, 2, 2) != 2) {
        InterlockedExchange(&g_broker_green_task_pending, 0);
        return;
    }

    /* Game_Restart resets survivor lap counters. Restore each survivor's own
     * trigger-time value (so lapped cars keep their deficit) and restore the
     * original event distance. A car that became a DNF during the Red Flag is
     * skipped here because the vanilla-shaped DNF tuple above owns its actual
     * retirement-lap value. */
    if(!restore_penalties_on_game_thread()) {
        log_w("RF-EE68C519305");
        InterlockedExchange(&g_post_green_continuity_result,-5);
        InterlockedExchange(&g_green_restore_result,-10);
        InterlockedExchange(&g_green_restore_armed,0);
        InterlockedExchange(&g_broker_green_task_pending,0);
        InterlockedExchange(&g_post_green_continuity_pending,0);
        return;
    }
    if(!restore_fuel_on_game_thread()) {
        InterlockedExchange(&g_green_restore_result,-11);
        InterlockedExchange(&g_green_restore_armed,0);
        InterlockedExchange(&g_broker_green_task_pending,0);
        return;
    }
    restore_pit_strategy();
    restore_driver_visibility();
    if (restore_individual_lap_continuity() < 0) {
        InterlockedExchange(&g_green_restore_result, -5);
        InterlockedExchange(&g_green_restore_armed, 0);
        InterlockedExchange(&g_broker_green_task_pending, 0);
        return;
    }
    /* All fallible survivor restores above use the still-consistent reduced
     * roster. In particular veh_slot must see every survivor, including the
     * last original slot. Publish the DNF tuples, full pointer list and count
     * only now, with no fallible helper or early return in this commit. */
    for (int i = 0; publish_roster && i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        *(volatile int*)(veh + OFF_FINISHSTATE) = 2;
        *(volatile int*)(veh + OFF_REASONCODE) = g_roster_saved_reason[i];
        *(volatile int*)(veh + OFF_CURRENTLAP) = g_roster_saved_lap[i];
        *(volatile int*)(veh + OFF_RACEPOS) = g_roster_saved_racepos[i];
        *(volatile BYTE*)(veh + OFF_RETIREDFLAG) = 1;
        *(volatile BYTE*)(veh + OFF_ACTIVEFLAG) = 0;
    }
    for (int i = 0; publish_roster && i < g_roster_count; ++i)
        *(volatile int*)((char*)g_roster_ptrs[i] + OFF_SLOTLIVE) =
            g_roster_saved_roles[i];
    for (int i = 0; publish_roster && i < g_roster_original_count; ++i)
        VEHICLE_ARRAY[i] = g_roster_full_ptrs[i];

    MemoryBarrier();
    if (publish_roster)
        VEHICLE_COUNT = g_roster_original_count;

    if (publish_roster) {
        g_roster_exclusion_active = FALSE;
        g_roster_results_restored = TRUE;
        g_roster_reduced_epoch = 0;
        hud_position_denominator_release();
        InterlockedExchange(&g_y_restart_guard_armed,
            (g_rf_config.y_restart_mode == RF_Y_RESTART_POST_RED_FLAG &&
             g_roster_count > 0) ? 1 : 0);
    } else if (!restore_roster) {
        g_roster_reduced_epoch = 0;
        hud_position_denominator_release();
    }
    InterlockedExchange(&g_green_restore_result,
                        (restore_roster && !publish_roster) ? -6
                                                           : (restore_roster ? 1 : 2));
    InterlockedExchange(&g_post_green_continuity_result, 0);
    InterlockedExchange(&g_post_green_continuity_armed, 1);
    InterlockedExchange(&g_green_restore_armed, 0);
    InterlockedExchange(&g_broker_green_task_pending, 0);
}

/* Runs on the next broker pump after broker_green_restore_task returns. The
 * native SessionTickUpdate that follows the first callback is therefore
 * complete, so its restart initialization can no longer replace the captured
 * fuel, driver-model visibility or per-driver lap values. */
static void __cdecl broker_post_green_continuity_task(void* context) {
    (void)context;
    if (InterlockedCompareExchange(&g_post_green_continuity_armed, 0, 0) != 1 ||
        SESSION_TYPE != 7 || RACE_PHASE != 5) {
        InterlockedExchange(&g_post_green_continuity_pending, 0);
        return;
    }
    if (InterlockedCompareExchange(&g_post_green_continuity_armed, 2, 1) != 1) {
        InterlockedExchange(&g_post_green_continuity_pending, 0);
        return;
    }
    if (g_active_count <= 0 || !g_saved_fuel || !g_saved_driver_visible ||
        !g_saved_currentlap || g_saved_total_laps < 1) {
        InterlockedExchange(&g_post_green_continuity_result, -1);
        goto continuity_done;
    }
    for (int i = 0; i < g_active_count; ++i) {
        char* veh = g_active[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE) ||
            veh_slot(veh) < 0 || !isfinite(g_saved_fuel[i]) ||
            g_saved_fuel[i] < 0.0f || g_saved_fuel[i] > 1000.0f ||
            g_saved_driver_visible[i] > 1 || g_saved_currentlap[i] < 0 ||
            g_saved_currentlap[i] > g_saved_total_laps) {
            InterlockedExchange(&g_post_green_continuity_result, -2);
            goto continuity_done;
        }
    }
    if(!restart_fuel_preflight()) {
        InterlockedExchange(&g_post_green_continuity_result,-6);
        goto continuity_done;
    }
    if (!restore_owned_dnf_garage_poses()) {
        log_w("RF-E0209400DB6");
        InterlockedExchange(&g_post_green_continuity_result, -4);
        goto continuity_done;
    }
    if(!restore_penalties_on_game_thread()) {
        log_w("RF-EE68C519305");
        InterlockedExchange(&g_post_green_continuity_result,-5);
        InterlockedExchange(&g_green_restore_result,-10);
        InterlockedExchange(&g_broker_green_task_pending,0);
        InterlockedExchange(&g_post_green_continuity_pending,0);
        goto continuity_done;
    }
    if(!restore_fuel_on_game_thread()) {
        InterlockedExchange(&g_post_green_continuity_result,-6);
        goto continuity_done;
    }
    restore_driver_visibility();
    if (restore_individual_lap_continuity() < 0) {
        InterlockedExchange(&g_post_green_continuity_result, -3);
        goto continuity_done;
    }
    MemoryBarrier();
    if(!rf_penalty_end()) {
        InterlockedExchange(&g_post_green_continuity_result,-5);
        goto continuity_done;
    }
    InterlockedExchange(&g_post_green_continuity_result, 1);

continuity_done:
    InterlockedExchange(&g_post_green_continuity_armed, 0);
    InterlockedExchange(&g_post_green_continuity_pending, 0);
}

/* Restore the original ordered field only while the reduced survivor roster
 * is still provably the exact live session that we quarantined. This is the
 * fail-safe for a warm pre-restart abort or deliberate in-race unload.
 *
 * IMPORTANT: never call F1C's active-vehicle builder here. The builder owns
 * vehicle/audio construction and is unsafe once race teardown has begun. The
 * saved ordered pointer array is the transaction's rollback data, so publish
 * it atomically in the same order as the broker green-edge path. */
static __attribute__((noinline, optimize("O0"))) BOOL restore_excluded_restart_roster(void) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("restore_excluded_restart_roster");

    char buf[192];
    if (g_roster_results_restored) {
        BOOL restored = VEHICLE_COUNT == g_roster_original_count;
        if (restored) hud_position_denominator_release();
        { rf_protected_result = (restored); goto rf_protected_exit; }
    }
    if (!g_roster_exclusion_active) {
        hud_position_denominator_release();
        { rf_protected_result = (TRUE); goto rf_protected_exit; }
    }
    if (!g_session_active || SESSION_TYPE != 7 ||
        (GAME_STATE != 5 && GAME_STATE != 7) ||
        (RACE_PHASE != 3 && RACE_PHASE != 5) ||
        session_epoch() != g_roster_reduced_epoch ||
        g_roster_count <= 0 || g_roster_original_count < 2 ||
        VEHICLE_COUNT + g_roster_count != g_roster_original_count) {
        log_w("RF-E3FF15C27A0");
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
    for (int i = 0; i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE)) {
            log_w("RF-E7C986A5BC7");
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
        }
    }
    for (int i = 0; i < g_roster_original_count; ++i) {
        char* veh = (char*)g_roster_full_ptrs[i];
        if (!veh || !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE)) {
            log_w("RF-E26DBD1EC3F");
            { rf_protected_result = (FALSE); goto rf_protected_exit; }
        }
    }

    for (int i = 0; i < g_roster_count; ++i) {
        char* veh = (char*)g_roster_ptrs[i];
        *(volatile int*)(veh + OFF_FINISHSTATE) = 2;
        *(volatile int*)(veh + OFF_REASONCODE) = g_roster_saved_reason[i];
        *(volatile int*)(veh + OFF_CURRENTLAP) = g_roster_saved_lap[i];
        *(volatile int*)(veh + OFF_RACEPOS) = g_roster_saved_racepos[i];
        *(volatile BYTE*)(veh + OFF_RETIREDFLAG) = 1;
        *(volatile BYTE*)(veh + OFF_ACTIVEFLAG) = 0;
    }
    for (int i = 0; i < g_roster_count; ++i)
        *(volatile int*)((char*)g_roster_ptrs[i] + OFF_SLOTLIVE) =
            g_roster_saved_roles[i];
    for (int i = 0; i < g_roster_original_count; ++i)
        VEHICLE_ARRAY[i] = g_roster_full_ptrs[i];
    MemoryBarrier();
    VEHICLE_COUNT = g_roster_original_count;

    wsprintfA(buf, "RF-E0D681661B3 %d %d",
              g_roster_original_count, g_roster_count);
    log_w(buf);
    g_roster_exclusion_active = FALSE;
    g_roster_results_restored = TRUE;
    g_roster_reduced_epoch = 0;
    hud_position_denominator_release();
    { rf_protected_result = (TRUE); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

static __attribute__((noinline, optimize("O0"))) void apply_qualpos(void) {
    RF_HARD_BEGIN("apply_qualpos");

    /* DNFs have already been removed from the bounded restart roster and are
     * deliberately not assigned a grid position. First-lap survivors keep
     * qualifying order, compacted to 1..N so a removed P3 cannot leave a P22
     * index in a 19-entry placement table. Ties retain pointer-array order.
     * Later restarts use the captured running order directly. */

    if (g_checkpoint_replay) {
        for (int i=0;i<g_active_count;++i)
            *(volatile int*)(g_active[i]+OFF_QUALPOS)=g_checkpoint_cars[i].grid;
    } else if (g_max_driven_laps < 1) {
        for (int i = 0; i < g_active_count; ++i) {
            int qp = *(volatile int*)(g_active[i] + OFF_QUALPOS);
            int rank = 1;
            for (int j = 0; j < g_active_count; ++j) {
                int other_qp = *(volatile int*)(g_active[j] + OFF_QUALPOS);
                if (other_qp < qp || (other_qp == qp && j < i)) ++rank;
            }
            g_active_rp[i] = rank;
        }
        for (int i = 0; i < g_active_count; ++i)
            *(volatile int*)(g_active[i] + OFF_QUALPOS) = g_active_rp[i];
        log_w("RF-EE3B506DE1A");
    } else {
        for (int i = 0; i < g_active_count; i++) {
            *(volatile int*)(g_active[i] + OFF_QUALPOS) = i + 1;
        }
    }
    char buf[128];
    wsprintfA(buf, "RF-E69EB1F8CD2 %d %d %s",
              g_placed, g_dnf_count, g_max_driven_laps < 1 ? "NO(qual)" : "YES(race)");
    log_w(buf);

    /* Per-car state at the moment of restart. DNF pointers remain owned by the
     * session arrays and return only through the green-edge transaction. */
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        wsprintfA(buf, "RF-E36060F8D82 %d %d %s %d %d %d %d %d",
                  i, veh_slot(veh), veh_name(veh),
                  *(volatile int*) (veh + OFF_QUALPOS),
                  *(volatile int*) (veh + OFF_FINISHSTATE),
                  *(volatile BYTE*)(veh + OFF_RETIREDFLAG),
                  *(volatile int*) (veh + OFF_ACTIVEFLAG),
                  *(volatile int*) (veh + OFF_CURRENTLAP));
        log_w(buf);
    }


rf_protected_exit:
    RF_MARK_END();
    return;
}

/* ======================================================================== *
 *  NO-OVERTAKE PACE HOLD
 *  Ported from patches/full_safety_car/dll/pace_probe.c (2026-08-01, the
 *  version the driver accepted: "Seems to be all set now"). The safety car,
 *  wave-around, caution lifecycle and GUI control block are all dropped -
 *  a red flag has no pace car and no restart procedure of its own, it just
 *  needs the field slowed, bunched and unable to pass until the restart.
 * ======================================================================== */

#define VEH_AT(i)     ((uintptr_t)VEHICLE_ARRAY[(i)])
#define V_I32(i, off) (*(volatile int32_t*)(VEH_AT(i) + (off)))
#define V_F32(i, off) (*(volatile float*)  (VEH_AT(i) + (off)))

typedef void (__attribute__((thiscall)) *set_maxspeed_fn)(void* veh, int n);
static const set_maxspeed_fn set_maxspeed =
    (set_maxspeed_fn)ADDR_SET_MAXSPEED;

static int np_live(int i) {
    uintptr_t veh;
    if (i < 0 || i >= g_session_capacity) return 0;
    veh = VEH_AT(i);
    if (!veh || IsBadReadPtr((const void*)veh, VEH_STRIDE)) return 0;
    return V_I32(i, OFF_SLOTLIVE) != -1;
}
static int np_moving(int i) { return V_F32(i, OFF_SPEED) >= 0.5f; }

/* pittingStatus >= 2, NOT != 0. Status 1 is the in-lap: the car is still out
 * on the circuit and must stay under pace control. Only from 2 does the game
 * drive it itself and apply its own 80 km/h limiter through this SAME
 * maxSpeed field - governing it there would fight the game at 20 Hz. */
static int np_pit_owned(int i) { return V_I32(i, OFF_PITTING) >= 2; }

/* Does this slot belong in the governed set? The player never does: a human
 * is not steered by maxSpeed. */
static int np_governed(int i) {
    if (!np_live(i)) return 0;
    if ((uintptr_t)VEH_AT(i) == PLAYER_VEH_PTR) return 0;
    if(g_rc && (LONG)VEH_AT(i)==f1c_rc_read(&g_rc->sc_vehicle))return 0;
    if (np_pit_owned(i)) return 0;
    return 1;
}

/* Learn the lap length from the largest lap distance ever seen. Cheap, needs
 * no .aiw parsing, converges as soon as any car approaches the start line.
 * Until it has, gaps are unmeasurable and every car simply holds the flat
 * pace speed - which is safe, not a release. */
static void np_learn_lap(void) {
    for (int i = 0; i < g_session_capacity; i++) {
        if (!np_live(i)) continue;
        float d = V_F32(i, OFF_LAPDIST);
        if (d > g_np_lap_len && d < 30000.0f) g_np_lap_len = d;
    }
    if (g_np_lap_len < 500.0f) g_np_lap_len = 0.0f;
}

static void np_hist_pump(void) {
    g_np_hidx = (g_np_hidx + 1) % SPD_HIST;
    for (int i = 0; i < g_session_capacity; i++)
        g_np_hist[i][g_np_hidx] = np_live(i) ? V_F32(i, OFF_SPEED) : 0.0f;
}

/* Lowest speed this car has shown in the last half second. A car that spins or
 * is damaged sheds speed in far less than one poll interval. */
static float np_speed_floor(int i) {
    float lo = g_np_hist[i][0];
    for (int n = 1; n < SPD_HIST; n++)
        if (g_np_hist[i][n] < lo) lo = g_np_hist[i][n];
    return lo;
}

/* MAY CAR j DICTATE CAR i's PACE, OR IS IT THE ONE OUT OF PLACE?
 *
 * Distance alone cannot answer this: a 20 m gap looks identical whether the
 * car ahead is the true queue predecessor or a lapped car fifteen places down
 * that has ended up between P4 and P5. Before this test that lapped car became
 * P5's pace setter, and because the law chains, every car behind P5 inherited
 * the cap too.
 *
 * The test is deliberately ASYMMETRIC, which is what keeps it safe: if j is
 * out of place for i then i is the better-classified car, and from j's side i
 * is BEHIND and not an obstacle at all. No pair can cap each other, so this
 * cannot reproduce the mutual-cap deadlock. Unreadable data keeps the old
 * behaviour rather than ignoring a real car. */
static int np_is_pace_setter(int i, int j) {
    int li = V_I32(i, OFF_CURRENTLAP), lj = V_I32(j, OFF_CURRENTLAP);
    if (lj < li) return 0;             /* a lap or more down, and in the way  */
    if (lj > li) return 1;             /* a lap up: legitimately ahead of me  */
    int pi = V_I32(i, OFF_RACEPOS), pj = V_I32(j, OFF_RACEPOS);
    if (pi <= 0 || pj <= 0) return 1;  /* unmeasurable: keep old behaviour    */
    return (pj < pi);
}

/* The nearest car this one must not run into: anything from ABREAST_WINDOW
 * behind us (already alongside) out to APPROACH_RANGE ahead. Returns the gap,
 * clamped at 0 for a car level with us, or -1 if there is nothing to respect.
 *
 * pace_only = 1 considers only cars entitled to set this car's pace.
 * pace_only = 0 returns the nearest physical car of any kind - being out of
 * position does not make a car intangible. */
static float np_nearest(int i, int* out_slot, int pace_only) {
    int   best_j = -1;
    double best  = 1e18;

    if (out_slot) *out_slot = -1;
    if (g_np_lap_len <= 0.0f) return -1.0f;
    double mine = (double)V_F32(i, OFF_LAPDIST);
    if (mine <= 0.0) return -1.0f;

    for (int j = 0; j < g_session_capacity; j++) {
        if (!np_live(j) || j == i) continue;
        if (!np_moving(j)) continue;
        /* A CRAWLING CAR IS NOT A PACE SETTER, IT IS AN OBSTRUCTION. Under a
         * real yellow you pass a wrecked car; you do not form up behind it.
         * Without this, one upside-down car pinned everything behind it on the
         * floor speed, one car at a time (live 2026-08-01). */
        if (np_speed_floor(j) < OBSTACLE_MIN) continue;
        if (np_pit_owned(j)) continue;   /* not on the racing line */
        if (V_F32(j, OFF_LAPDIST) <= 0.0f) continue;
        if (pace_only && !np_is_pace_setter(i, j)) continue;

        double d = (double)V_F32(j, OFF_LAPDIST) - mine;
        while (d < 0.0)                    d += g_np_lap_len;
        while (d >= (double)g_np_lap_len)  d -= g_np_lap_len;

        /* Just behind us reads as almost a full lap ahead - fold it back so an
         * abreast car is seen as the small negative number it really is. */
        if (d > (double)g_np_lap_len - (double)ABREAST_WINDOW)
            d -= (double)g_np_lap_len;

        if (d < -(double)ABREAST_WINDOW) continue;   /* genuinely behind  */
        if (d > (double)APPROACH_RANGE)  continue;   /* too far to matter */

        /* AN ABREAST CAR IS AN OBSTACLE TO ONE OF THE PAIR, NOT TO BOTH.
         * Applied to both it is mutual: each caps the other at the other's
         * speed, and any dip either takes is copied back and forth until the
         * pair crawls. The standings break the tie - only the car behind IN
         * THE ORDER respects it. */
        if (d < 0.0) {
            int pj = V_I32(j, OFF_RACEPOS), pi = V_I32(i, OFF_RACEPOS);
            if (!(pj > 0 && pi > 0 && pj < pi)) continue;
        }

        if (d < best) { best = d; best_j = j; }
    }

    if (best_j < 0) return -1.0f;
    if (out_slot) *out_slot = best_j;
    return (best < 0.0) ? 0.0f : (float)best;
}

/* The approach law. Does the no-passing job and the braking job at once. */
static float np_approach_cap(int i, float want) {
    int   ahead = -1, phys = -1;
    float gap   = np_nearest(i, &ahead, 1);   /* who may set my pace       */
    float pgap  = np_nearest(i, &phys,  0);   /* what is physically there  */

    if (ahead >= 0 && gap >= 0.0f) {
        /* THE BASE IS THE CAR AHEAD'S CURRENT SPEED OR ITS RECENT MINIMUM,
         * WHICHEVER IS HIGHER.
         *
         * The rolling minimum alone sags: a car that lifts for one corner
         * drags down everything behind it, and the queue ratchets to the
         * floor. Taking the maximum with the current speed removes the sag
         * without introducing any number independent of the car ahead - so at
         * the follow distance a pass stays arithmetically impossible.
         *
         * The distance term is allowed to SUBTRACT, which makes the gap
         * mandatory: nose-to-tail at matched speed is exactly where a small
         * differential turns into a passing attempt. The 60% floor below is
         * what stops that turning back into a collapse. */
        float room  = gap - TARGET_GAP;
        float cur   = V_F32(ahead, OFF_SPEED);
        float flo   = np_speed_floor(ahead);
        float base  = (cur > flo) ? cur : flo;
        float allow = base + APPROACH_GAIN * room;

        if (allow < base * 0.6f)     allow = base * 0.6f;
        if (allow < (float)PACE_MIN) allow = (float)PACE_MIN;
        if (want > allow) want = allow;
    }

    /* A CAR THAT MAY NOT SET MY PACE IS STILL SOLID. Refusing to inherit a
     * lapped car's pace must not become permission to drive through it. So the
     * nearest PHYSICAL car still applies a cap, but only inside BLOCK_RANGE
     * and tucking up to BLOCK_GAP rather than the full queue spacing. Beyond
     * BLOCK_RANGE it has no effect at all. */
    if (phys >= 0 && phys != ahead && pgap >= 0.0f && pgap < BLOCK_RANGE) {
        float cur   = V_F32(phys, OFF_SPEED);
        float flo   = np_speed_floor(phys);
        float base  = (cur > flo) ? cur : flo;
        float allow = base + APPROACH_GAIN * (pgap - BLOCK_GAP);

        if (allow < base * 0.6f)     allow = base * 0.6f;
        if (allow < (float)PACE_MIN) allow = (float)PACE_MIN;
        if (want > allow) want = allow;
    }
    return want;
}

/* The car the pack forms up behind: best race position among governed cars
 * that are actually moving. A parked car must never become the pace setter. */
static int np_find_leader(void) {
    int best = -1, best_pos = 1 << 30;
    for (int i = 0; i < g_session_capacity; i++) {
        if (!g_np_armed[i] || !np_live(i)) continue;
        if (!np_moving(i)) continue;
        int p = V_I32(i, OFF_RACEPOS);
        if (p > 0 && p < best_pos) { best_pos = p; best = i; }
    }
    return best;
}

/* Ramp the commanded pace down from whatever the field was doing. Commanding
 * the target directly braked the whole grid at ~15 m/s^2 (live 2026-07-31). */
static int np_commanded_speed(void) {
    if (g_np_ramp_from <= (float)PACE_TARGET) return PACE_TARGET;
    float t = (float)(GetTickCount() - g_np_ramp_t0) / 1000.0f;
    if (t >= RAMP_SECONDS) return PACE_TARGET;
    float v = g_np_ramp_from -
              (g_np_ramp_from - (float)PACE_TARGET) * (t / RAMP_SECONDS);
    int   n = (int)(v + 0.5f);
    if (n == SPEED_IDLE_SENT) n = 19;
    return n;
}

/* Per-car cap. ONE CAR IS GIVEN A SPEED, EVERYONE ELSE IS GIVEN A RULE. */
static int np_speed_for(int i, int base) {
    /* The pace setter, and the fallback for anything unmeasurable. Without a
     * learned lap length there are no gaps and so no rule to apply; the flat
     * pace is then the only safe cap, or a half-learned track would release
     * the whole field. */
    if (i == g_np_leader || g_np_leader < 0 || g_np_lap_len <= 0.0f ||
        V_F32(i, OFF_LAPDIST) <= 0.0f)
        return base;

    float want = np_approach_cap(i, (float)SPEED_RELEASE);
    if (want > (float)SPEED_RELEASE) want = (float)SPEED_RELEASE;

    /* THE FLOOR IS RELATIVE TO THE PACE, NOT AN ABSOLUTE 5. PACE_MIN is a
     * legality clamp, not a speed anyone should be asked to drive at - using
     * it as the floor is how one crawling car pinned the cars behind it at
     * maxSpeed 5. At a 30 m/s pace, 45% is 13.5 m/s: slow enough to lose
     * ground quickly, fast enough to look like a racing car obeying a flag. */
    int v  = (int)(want + 0.5f);
    int lo = (int)((float)base * 0.45f + 0.5f);
    if (lo < PACE_MIN) lo = PACE_MIN;
    if (v < lo) v = lo;
    if (v == SPEED_IDLE_SENT) v = 19;   /* 18 means PitSpeedLimit */
    return v;
}

/* Capture and arm one slot. */
static void np_arm(int i) {
    float v = V_F32(i, OFF_MAXSPEED);
    /* A car that has just left the pit lane can still carry the 22.2 m/s pit
     * limit for a tick or two. Restoring THAT to a car out on track would pin
     * it at 80 km/h for the rest of the race. */
    if (v < 1000.0f) v = (float)SPEED_RELEASE;
    g_np_saved[i] = v;
    g_np_pass[i]  = V_F32(i, OFF_PASSING);
    g_np_aggro[i] = V_F32(i, OFF_AGGRO);

    /* Take the AI's appetite for overtaking away. PassingNorm is the
     * overtaking knob; 0.9 is .rcd Passing 0, "never try". Both fields are
     * baked once at session init and never recomputed, so the write holds.
     *
     * TRACKAGGRESSION IS NOT TOUCHED. It is the FOLLOWING-GAP knob (AISteering
     * floors the desired gap at 20 - 10a metres), so flooring it at 0.01 tells
     * every car to hold ~20 m of clear air permanently - the opposite of what
     * a bunched field needs. It cost a live session. It is only repaired here
     * if an older build already clobbered it. */
    V_F32(i, OFF_PASSING) = PASSING_MIN;
    if (V_F32(i, OFF_AGGRO) < 0.05f) V_F32(i, OFF_AGGRO) = AGGRO_DEFAULT;

    g_np_armed[i] = 1;
    g_np_lastw[i] = 0.0f;
}

static void apply_speed_cap(void) {
    if (g_np_active) return;
    char buf[128];
    int  n = 0;
    float fastest = 0.0f;

    for (int i = 0; i < g_session_capacity; i++) {
        g_np_armed[i] = 0;
        for (int k = 0; k < SPD_HIST; k++)
            g_np_hist[i][k] = np_live(i) ? V_F32(i, OFF_SPEED) : 0.0f;
    }
    g_np_lap_len = 0.0f;
    g_np_leader  = -1;
    np_learn_lap();

    for (int i = 0; i < g_session_capacity; i++) {
        if (!np_governed(i)) continue;
        float s = V_F32(i, OFF_SPEED);
        if (s > fastest) fastest = s;
        np_arm(i);
        n++;
    }

    g_np_ramp_from = (fastest > (float)PACE_TARGET) ? fastest : (float)PACE_TARGET;
    g_np_ramp_t0   = GetTickCount();
    g_np_cmd       = np_commanded_speed();
    g_np_active    = TRUE;

    wsprintfA(buf, "RF-E916DFCC36C %d %d %d"
                   "over %d s (lap_len %d m)",
              n, (int)g_np_ramp_from, PACE_TARGET, (int)RAMP_SECONDS,
              (int)g_np_lap_len);
    log_w(buf);
}

static void restore_speed_cap(void) {
    if (!g_np_active) return;
    int n = 0;
    for (int i = 0; i < g_session_capacity; i++) {
        if (!g_np_armed[i]) continue;
        g_np_armed[i] = 0;
        if (!np_live(i)) continue;
        /* Hand the car back through the game's own release value, not by
         * writing the float: 0x4538 is the field the governor call owns. */
        set_maxspeed((void*)VEH_AT(i), SPEED_RELEASE);
        if (g_np_pass[i]  > 0.0f) V_F32(i, OFF_PASSING) = g_np_pass[i];
        if (g_np_aggro[i] > 0.0f) V_F32(i, OFF_AGGRO)   = g_np_aggro[i];
        n++;
    }
    g_np_active = FALSE;
    char buf[64];
    wsprintfA(buf, "RF-EB53B4AF77D %d", n);
    log_w(buf);
}

static BOOL g_fuel_logged = FALSE;

static void restore_fuel(void) {
    for (int i = 0; i < g_active_count; i++) {
        char* veh = g_active[i];
        if (!veh) continue;
        *(volatile float*)(veh + OFF_FUELLEVEL) = g_saved_fuel[i];
    }
    if (!g_fuel_logged) {
        char buf[64];
        wsprintfA(buf, "RF-ECCB426BAAE %d", g_active_count);
        log_w(buf);
        g_fuel_logged = TRUE;
    }
}

/* Write the plans captured before the native reset back into each survivor
 * that still has a valid captured state. Runs at GRID_READY, on the green
 * game-thread task and again in the post-green continuity pass, so the plan is
 * in place before the first AI decision of the restarted race. Each car is
 * independent: an invalid/stale saved state or unsupported target is skipped
 * and only that car keeps the native post-restart plan. */
static BOOL restore_pit_strategy(void) {
    int restored=0,refused=0,skipped=0;
    if (!g_saved_pit_strategy || !g_saved_pit_valid || g_active_count < 1 ||
        g_active_count > 104 || SESSION_TYPE != 7) return FALSE;
    for (int i = 0; i < g_active_count; ++i) {
        char* veh = g_active[i];
        if (!g_saved_pit_valid[i]) { ++skipped; continue; }
        if (!veh || veh_slot(veh) < 0 ||
            !runtime_region_supported((uintptr_t)veh, VEH_STRIDE, FALSE) ||
            !f1c_pit_strategy_restore(veh, VEH_STRIDE, &g_saved_pit_strategy[i])) {
            if (refused < 8) {
                char buf[176];
                wsprintfA(buf, "RF-E980A9DAECB %d",
                          veh ? veh_slot(veh) : -1);
                log_w(buf);
            }
            ++refused;
            continue;
        }
        ++restored;
    }
    if (restored) {
        char buf[192];
        wsprintfA(buf, "RF-EA8E8349A34 %d",
                  restored);
        log_w(buf);
    }
    if (skipped || refused) {
        char buf[224];
        wsprintfA(buf, "RF-EF87F7F073C %d %d %d",
                  skipped + refused, skipped, refused);
        log_w(buf);
    }
    return restored > 0;
}

static void restore_driver_visibility(void) {
    for (int i = 0; i < g_active_count; i++) {
        char* veh = g_active[i];
        if (!veh) continue;
        *(volatile BYTE*)(veh + OFF_DRIVER_VISIBLE) =
            g_saved_driver_visible[i] ? 1 : 0;
    }
}

static void prepare_original_lap_total(void) {
    /* Per-driver current laps are restored at GRID_READY and green, so the
     * restart must be constructed with the original event total. Subtracting
     * the leader's laps here created the incorrect /36, /47, etc. HUD total. */
    RESTART_TOTAL_LAPS_PTR = g_saved_total_laps;
    LIVE_TOTAL_LAPS_PTR = g_saved_total_laps;
    char buf[128];
    wsprintfA(buf, "RF-EE1D2FE29AE %d %d %d",
              g_saved_total_laps, g_saved_total_laps, g_max_driven_laps);
    log_w(buf);
}

/*
 * Re-arm the pace hold. Call every tick of the red-flag window.
 *
 * NAME KEPT for the call sites, but this no longer writes race positions:
 * see the comment on g_locked_racepos. It now re-arms the maxSpeed governor,
 * which is what actually stops the AI passing.
 */
static void enforce_positions(void) {
    if (!g_np_active) return;
    /* NEVER WRITE A VEHICLE STRUCT OUTSIDE A LIVE RACE. Four crashes in the
     * full_safety_car sessions all had this one shape: race-only state written
     * through a teardown, while the loader was rebuilding those same structs.
     * SESSION_TYPE is the gate the rest of this DLL already uses; the car count
     * catches the window where the session is nominally a race but the field
     * does not exist yet. */
    if (SESSION_TYPE != 7) return;
    if (!field_size_supported()) return;

    np_hist_pump();
    np_learn_lap();
    g_np_leader = np_find_leader();
    g_np_cmd    = np_commanded_speed();

    for (int i = 0; i < g_session_capacity; i++) {
        if (!g_np_armed[i]) continue;
        if (!np_live(i)) { g_np_armed[i] = 0; continue; }

        /* Entered the pit lane: the game drives this same maxSpeed field
         * itself now (its own 80 km/h limiter). Hand the car back rather than
         * fight it at 20 Hz. This is the normal exit for every AI car on the
         * NUMPAD2 path, which sends the whole field to the pits. */
        if (np_pit_owned(i)) {
            if (g_np_pass[i]  > 0.0f) V_F32(i, OFF_PASSING) = g_np_pass[i];
            if (g_np_aggro[i] > 0.0f) V_F32(i, OFF_AGGRO)   = g_np_aggro[i];
            g_np_armed[i] = 0;
            char buf[96];
            wsprintfA(buf, "RF-EA7B77FA4E9 %d"
                           "(pittingStatus=%d)", i, V_I32(i, OFF_PITTING));
            log_w(buf);
            continue;
        }

        /* A WRECK IS NOT GOVERNED. Nothing good comes of holding a commanded
         * speed on a car that cannot drive, and it also stops it ever being
         * seen as recovered - the mid-window pickup below re-arms it if it
         * gets going again. Stopped, not merely slow: under a pace hold every
         * car is slow by construction. */
        if (V_F32(i, OFF_SPEED) < 0.5f) {
            set_maxspeed((void*)VEH_AT(i), SPEED_RELEASE);
            g_np_armed[i] = 0;
            continue;
        }

        int   want   = np_speed_for(i, g_np_cmd);
        float target = (float)want;
        float cur    = V_F32(i, OFF_MAXSPEED);

        if (cur != target) {
            /* A GAME write is one that differs from what WE last put there. A
             * difference that merely matches our previous command is the ramp
             * (or the approach law) moving, not the game fighting us. */
            if (cur != g_np_lastw[i] && g_np_lastw[i] != 0.0f) {
                char buf[144];
                wsprintfA(buf, "RF-E535259BF75 %d %d"
                               "last wrote %d - re-arming to %d",
                          i, (int)cur, (int)g_np_lastw[i], want);
                log_w(buf);
            }
            set_maxspeed((void*)VEH_AT(i), want);
            g_np_lastw[i] = target;
        }
    }

    /* A car that becomes governable mid-window (it was in the pits, or its
     * slot went live) is picked up here rather than left racing. Not on the
     * NUMPAD2 path: there the whole field is deliberately going to the pits
     * and must not be re-captured on the way. */
    if (!g_pitret_count) {
        for (int i = 0; i < g_session_capacity; i++) {
            if (g_np_armed[i] || !np_governed(i)) continue;
            if (V_F32(i, OFF_SPEED) < OBSTACLE_MIN) continue;
            np_arm(i);
            set_maxspeed((void*)VEH_AT(i), g_np_cmd);
            g_np_lastw[i] = (float)g_np_cmd;
            char buf[96];
            wsprintfA(buf, "RF-EF76A0ABCA7 %d"
                           "maxSpeed=%d", i, (int)g_np_saved[i]);
            log_w(buf);
        }
    }

    /* Drift against the snapshot is now a MEASUREMENT, not something to
     * correct. If this logs, the pace hold let a pass through. */
    if (g_pos_lock_active) {
        for (int i = 0; i < g_active_count; i++) {
            char* veh = g_active[i];
            if (!veh) continue;
            int current = *(volatile int*)(veh + OFF_RACEPOS);
            if (current != g_locked_racepos[i]) {
                char buf[128];
                wsprintfA(buf, "RF-EC3CEC5144B %d %d %d"
                               "through)", i, g_locked_racepos[i], current);
                log_w(buf);
                g_locked_racepos[i] = current;
            }
        }
    }
}

static void commentary_suppress(void) {
    /* HOLD OFF WHILE OUR OWN LINE IS STILL OWED THE CHANNEL. This is the whole
     * fix for "NUMPAD2 plays nothing": muting 50 ms after the inject killed
     * PWS_RED01 before it started. Bounded by SAY_MAX_WAIT_MS, so a line that
     * never plays cannot leave the game commentating the pit-in indefinitely. */
    if (say_busy()) return;

    if (!g_commentary_suppressed) {
        g_commentary_saved = *COMMENTARY_ENABLED;
        g_commentary_suppressed = TRUE;
        log_w("RF-EDB9C3FDDC3");
    }
    *COMMENTARY_ENABLED = 0;
}

static void commentary_restore(void) {
    if (g_commentary_suppressed) {
        *COMMENTARY_ENABLED = g_commentary_saved;
        g_commentary_suppressed = FALSE;
        log_w("RF-E1951A4AF61");
    }
}

typedef int (__attribute__((thiscall)) *game_action_state_fn)(void*, int, char);
static const game_action_state_fn game_action_state =
    (game_action_state_fn)0x0040CDE0;

static void camera_action_save(void) {
    int action = CAMERA_ACTION;
    /* GameActionStateMachine has no case 6; all other observed action IDs are
     * accepted. Fail closed instead of inventing a camera mode. */
    if ((action >= 0 && action <= 5) || (action >= 7 && action <= 10)) {
        g_saved_camera_action = action;
        g_camera_action_saved = TRUE;
    }
}

static void camera_action_restore(void) {
    if (!g_camera_action_saved) return;
    game_action_state(CAMERA_MANAGER, g_saved_camera_action, 1);
    g_camera_action_saved = FALSE;
    log_w("RF-EE0CD0EA55C");
}

/* Read-only optional SC integration, ABI1. Does not activate or stop RF. */
__declspec(dllexport) int __cdecl F1C_QueryRemovalPolicy(unsigned *out,unsigned bytes) {
    if(!out || bytes<24)return 0;
    out[0]=0x524D5631u;out[1]=1;out[2]=24;
    out[3]=!g_running?0u:(g_rc?1u:2u);
    out[4]=out[3]==1?*VEHICLE_REMOVAL_RUNTIME:0;
    out[5]=g_vehicle_removal_forced?1u:0u;
    return 1;
}

static DWORD rf_cleanup_normal_bits(int slot){
    float seconds=slot==0?g_rf_config.vehicle_removal_seconds:slot==1?g_rf_config.debris_removal_seconds:g_rf_config.parts_duration_seconds;
    DWORD bits;if(!isfinite(seconds)||seconds<1||seconds>600)seconds=60;memcpy(&bits,&seconds,4);return bits;
}
static void cleanup_values_keep_sixty(void) {
    uintptr_t base = PLAYER_OPTIONS_BASE;
    for (int i = 0; i < CLEANUP_OPTION_COUNT; ++i) {
        uintptr_t entry = base + VEHICLE_REMOVAL_ENTRY_OFF
                        + i * CLEANUP_OPTION_STRIDE;
        *(volatile DWORD*)(entry + OPTION_VALUE_A_OFF) = rf_cleanup_normal_bits(i);
        *(volatile DWORD*)(entry + OPTION_VALUE_B_OFF) = rf_cleanup_normal_bits(i);
    }
    *VEHICLE_REMOVAL_RUNTIME = rf_cleanup_normal_bits(0);
}

/* Parts Duration is baked into loose collision objects when types 3/4/5 are
 * spawned (sub_411330 writes body+28 = PartsDuration + spawn time). Changing
 * the option at restart therefore cannot expire objects that already exist.
 * Run only through LightLoader's game-thread broker: while cleanup owns the
 * session, make one bounded pass that clamps loose-part expiries to their last
 * native time. Native cleanup consumes them on its following update; rescanning
 * the swap-compacted pools after that update can race freed bodies and must not
 * be scheduled.
 * Never call the game's remove or destructor functions from this plug-in. */
static void __cdecl broker_cleanup_loose_objects_task(void* context) {
    uintptr_t manager;
    int remaining = 0;
    int clamped = 0;
    int invalid_entries = 0;
    BOOL unobservable_pool = FALSE;
    LONG generation = (LONG)(intptr_t)context;
    if (InterlockedCompareExchange(
            &g_cleanup_loose_pending_generation, 0, 0) != generation)
        return;
    if (InterlockedCompareExchange(&g_cleanup_loose_task_pending, 2, 1) != 1)
        return;
    if (InterlockedCompareExchange(&g_cleanup_loose_active, 0, 0) != 1 ||
        InterlockedCompareExchange(&g_cleanup_loose_generation, 0, 0) !=
            generation ||
        session_epoch() != g_cleanup_loose_session_epoch) goto stale;
    /* 0x005EA560 is the embedded DynMan object, not a global pointer to one.
     * The old dereference read object dword 0 (active count) and treated that
     * small integer as an address, so every live cleanup failed at the root
     * region check before reaching any wing descriptor. */
    manager = DYNAMIC_MANAGER_OBJECT;
    if (!manager || !runtime_region_supported(
            manager, DYNAMIC_MANAGER_FIELDS * sizeof(uintptr_t), FALSE))
        goto invalid;
    int active_count = *(volatile int*)manager;
    if (active_count < 0 || active_count > DYNAMIC_MANAGER_MAX_ACTIVE)
        goto invalid;

    /* DynMan's active pointer list is swap-compacted while the physics update
     * removes objects. The former probe required descriptor[0] to equal the
     * list-loop index and failed live before it could clamp any wing expiry.
     * The native constructor provides a safer stable layout: loose types 0..5
     * each own a descriptor pool at manager dwords 9/10, 12/13, ... 24/25.
     * Every descriptor has its immutable pool type at +28; +0 is -1 while
     * inactive and otherwise its current active-list index. Scan those pools
     * directly, validating all bounds, without touching list ownership. */
    /* Types 3/4/5 are the wing/body pools whose body+28 expiry is baked at
     * spawn by sub_411330. Types 0..2 have no reviewed +28 PartsDuration
     * contract and remain owned by native DebrisRemoval. */
    for (int type = 3; type < DYNAMIC_LOOSE_POOL_TYPES; ++type) {
        size_t count_field = DYNAMIC_LOOSE_POOL_BASE +
            (size_t)type * DYNAMIC_LOOSE_POOL_STRIDE;
        int pool_count = *(volatile int*)(manager +
            count_field * sizeof(uintptr_t));
        uintptr_t pool = *(volatile uintptr_t*)(manager +
            (count_field + 1u) * sizeof(uintptr_t));
        if (pool_count < 0 || pool_count > DYNAMIC_MANAGER_MAX_ACTIVE ||
            (pool_count > 0 && (!pool || !runtime_region_supported(
                pool, (size_t)pool_count * DYNAMIC_DESCRIPTOR_SIZE, FALSE)))) {
            ++invalid_entries;
            unobservable_pool = TRUE;
            continue;
        }

        for (int i = 0; i < pool_count; ++i) {
            uintptr_t descriptor = pool +
                (size_t)i * DYNAMIC_DESCRIPTOR_SIZE;
            if (*(volatile int*)(descriptor + DYNAMIC_DESCRIPTOR_TYPE) != type) {
                ++invalid_entries;
                unobservable_pool = TRUE;
                continue;
            }
            int active_index = *(volatile int*)descriptor;
            if (active_index == -1) continue;
            /* +0 is maintained by DynMan's swap-compacted list. It can change
             * between native updates; -1 is the only inactive sentinel. Do not
             * reject a valid part merely because its transient index is above
             * the separately sampled active count. */
            if (active_index < 0) {
                ++invalid_entries;
                unobservable_pool = TRUE;
                continue;
            }
            ++remaining;
            if (type >= 3) {
                uintptr_t body = *(volatile uintptr_t*)(descriptor +
                    DYNAMIC_DESCRIPTOR_BODY);
                if (!body || !runtime_region_supported(body, 40u, FALSE)) {
                    ++invalid_entries;
                    continue;
                }
                float expiry = *(volatile float*)(body + DYNAMIC_BODY_EXPIRY);
                float current = *(volatile float*)(body +
                    DYNAMIC_BODY_CURRENT_TIME);
                if (!isfinite(expiry) || !isfinite(current)) {
                    ++invalid_entries;
                    continue;
                }
                if (expiry > current) {
                    if (InterlockedCompareExchange(
                            &g_cleanup_loose_active, 0, 0) != 1 ||
                        InterlockedCompareExchange(
                            &g_cleanup_loose_generation, 0, 0) != generation ||
                        session_epoch() != g_cleanup_loose_session_epoch)
                        goto stale;
                    *(volatile float*)(body + DYNAMIC_BODY_EXPIRY) = current;
                    ++clamped;
                }
            }
        }
    }
    /* A bad pool cannot be treated as empty. Hold the bounded cleanup window
     * instead of repeating v56/v58's false zero and immediate restart. */
    if (unobservable_pool && remaining == 0) remaining = 1;
    if (InterlockedCompareExchange(&g_cleanup_loose_generation, 0, 0) !=
            generation || session_epoch() != g_cleanup_loose_session_epoch)
        goto stale;
    InterlockedExchange(&g_cleanup_loose_observed, remaining);
    InterlockedExchange(&g_cleanup_loose_clamped, clamped);
    InterlockedExchange(&g_cleanup_loose_invalid, invalid_entries);
    InterlockedExchange(&g_cleanup_loose_remaining, 0);
    InterlockedExchange(&g_cleanup_loose_active, 0);
    InterlockedExchange(&g_cleanup_loose_task_pending, 0);
    return;

invalid:
    if (InterlockedCompareExchange(&g_cleanup_loose_active, 0, 0) != 1 ||
        InterlockedCompareExchange(&g_cleanup_loose_generation, 0, 0) !=
            generation ||
        InterlockedCompareExchange(
            &g_cleanup_loose_pending_generation, 0, 0) != generation ||
        session_epoch() != g_cleanup_loose_session_epoch)
        goto stale;
    InterlockedExchange(&g_cleanup_loose_clamped, -1);
    InterlockedExchange(&g_cleanup_loose_invalid, -1);
    InterlockedExchange(&g_cleanup_loose_remaining, -1);
    InterlockedExchange(&g_cleanup_loose_active, 0);
    InterlockedExchange(&g_cleanup_loose_task_pending, 0);
    return;

stale:
    /* A queued callback can arrive after abort/teardown. Publish nothing and
     * clear only the pending generation that this exact callback owns. */
    if (InterlockedCompareExchange(
            &g_cleanup_loose_pending_generation, 0, 0) == generation)
        InterlockedExchange(&g_cleanup_loose_task_pending, 0);
}

static void cleanup_loose_objects_poll(DWORD now) {
    int post_result;
    LONG generation;
    if (InterlockedCompareExchange(&g_cleanup_loose_active, 0, 0) != 1 ||
        g_cleanup_loose_post_failed ||
        InterlockedCompareExchange(&g_cleanup_loose_task_pending, 0, 0) != 0 ||
        (g_cleanup_loose_last_post != 0 &&
         now - g_cleanup_loose_last_post < 100u)) return;
    if (!g_broker_registered || !g_broker_post || !g_broker_cleanup_token) {
        g_cleanup_loose_post_failed = TRUE;
        log_w("RF-E8D916C37B9");
        return;
    }
    if (InterlockedCompareExchange(&g_cleanup_loose_task_pending, 1, 0) != 0)
        return;
    generation = InterlockedCompareExchange(
        &g_cleanup_loose_generation, 0, 0);
    InterlockedExchange(&g_cleanup_loose_pending_generation, generation);
    MemoryBarrier();
    g_cleanup_loose_last_post = now;
    post_result = g_broker_post(g_broker_cleanup_token,
        broker_cleanup_loose_objects_task, (void*)(intptr_t)generation);
    if (post_result == RF_BROKER_STALE_TOKEN) {
        int attempt;
        g_broker_cleanup_token = 0;
        for (attempt = 0; attempt < 200; ++attempt) {
            post_result = g_broker_register(
                g_hinst, "red_flag_cleanup", &g_broker_cleanup_token);
            if (post_result == RF_BROKER_OK) break;
            Sleep(10);
        }
        if (post_result == RF_BROKER_OK)
            post_result = g_broker_post(g_broker_cleanup_token,
                broker_cleanup_loose_objects_task,
                (void*)(intptr_t)generation);
    }
    if (post_result != RF_BROKER_OK) {
        InterlockedExchange(&g_cleanup_loose_task_pending, 0);
        g_cleanup_loose_post_failed = TRUE;
        {
            char buf[160];
            wsprintfA(buf, "RF-EB429999F15 %d",
                      post_result);
            log_w(buf);
        }
    }
}

static int cleanup_loose_objects_waiting(void) {
    LONG clamped = InterlockedCompareExchange(&g_cleanup_loose_clamped, 0, 0);
    LONG remaining = InterlockedCompareExchange(
        &g_cleanup_loose_remaining, 0, 0);
    LONG observed = InterlockedCompareExchange(
        &g_cleanup_loose_observed, 0, 0);
    LONG invalid_entries = InterlockedCompareExchange(
        &g_cleanup_loose_invalid, 0, 0);
    if (clamped > 0 && !g_cleanup_loose_clamp_logged) {
        char buf[160];
        wsprintfA(buf, "RF-E306A654745 %ld %ld",
                  clamped, observed);
        log_w(buf);
        g_cleanup_loose_clamp_logged = TRUE;
    }
    if (remaining < 0) {
        if (!g_cleanup_loose_post_failed)
            log_w("RF-EDFB30F6036");
        g_cleanup_loose_post_failed = TRUE;
        return 0;
    }
    if (invalid_entries > 0 && !g_cleanup_loose_invalid_logged) {
        char buf[176];
        wsprintfA(buf, "RF-E25EE0629C4 %ld",
                  invalid_entries);
        log_w(buf);
        g_cleanup_loose_invalid_logged = TRUE;
    }
    return (int)remaining;
}

static void vehicle_removal_force_begin(void) {
    uintptr_t base = PLAYER_OPTIONS_BASE;
    if (!g_vehicle_removal_forced) {
        g_vehicle_removal_options_base = base;
        g_vehicle_removal_runtime_saved = rf_cleanup_normal_bits(0);
        g_removal_repair_count = 0;
        g_removal_repair_epoch = session_epoch();
        {
            LONG generation = InterlockedIncrement(&g_cleanup_loose_generation);
            if (generation == 0)
                generation = InterlockedIncrement(&g_cleanup_loose_generation);
            g_cleanup_loose_session_epoch = g_removal_repair_epoch;
        }
        InterlockedExchange(&g_cleanup_loose_active, 1);
        InterlockedExchange(&g_cleanup_loose_remaining, 0);
        InterlockedExchange(&g_cleanup_loose_observed, 0);
        InterlockedExchange(&g_cleanup_loose_clamped, 0);
        InterlockedExchange(&g_cleanup_loose_invalid, 0);
        g_cleanup_loose_last_post = 0;
        g_cleanup_loose_post_failed = FALSE;
        g_cleanup_loose_clamp_logged = FALSE;
        g_cleanup_loose_invalid_logged = FALSE;
        for (int i = 0; i < CLEANUP_OPTION_COUNT; i++) {
            g_vehicle_removal_saved_a[i] = rf_cleanup_normal_bits(i);
            g_vehicle_removal_saved_b[i] = rf_cleanup_normal_bits(i);
        }
        g_vehicle_removal_forced = TRUE;
        log_w("RF-EAA2342E06F");
    }

    if (base == g_vehicle_removal_options_base) {
        for (int i = 0; i < CLEANUP_OPTION_COUNT; i++) {
            uintptr_t entry = base + VEHICLE_REMOVAL_ENTRY_OFF
                            + i * CLEANUP_OPTION_STRIDE;
            *(volatile float*)(entry + OPTION_VALUE_A_OFF) = 0.0f;
            *(volatile float*)(entry + OPTION_VALUE_B_OFF) = 0.0f;
        }
    }
    /* PlayerOptions is configuration storage; sub_4F1FE0 compares its timer
     * against this separate runtime float. The old build changed only the
     * descriptor, so the live threshold remained 60 seconds. */
    *VEHICLE_REMOVAL_RUNTIME = 0u;
    cleanup_loose_objects_poll(GetTickCount());

    /* Complete-failure repair (live traced on Trulli, 2026-08-19): a wheel-loss
     * crash can clear ActiveFlag while leaving an expired finite AI pit trigger
     * at +0x4170. AIMainUpdate then returns before clearing it, while the native
     * removal consumer refuses to start until it is FLT_MAX. Repair only that
     * impossible deadlock for a captured, physics-dead, unretired DNF. */
    if (session_epoch() == g_removal_repair_epoch) {
        for (int i = 0; i < g_dnf_count; i++) {
            char* veh = (char*)g_dnf_ptrs[i];
            if (!veh || *(volatile int*)(veh + OFF_ACTIVEFLAG) != 0 ||
                *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
            float trigger = *(volatile float*)(veh + OFF_AI_PIT_TRIGGER);
            float ai_time = *(volatile float*)(veh + OFF_AI_CURRENT_TIME);
            DWORD trigger_bits = *(volatile DWORD*)(veh + OFF_AI_PIT_TRIGGER);
            if (trigger_bits == FLT_MAX_BITS || trigger >= ai_time) continue;

            BOOL known = FALSE;
            for (int r = 0; r < g_removal_repair_count; r++)
                if (g_removal_repair_ptrs[r] == veh) known = TRUE;
            if (!known && g_removal_repair_count < g_session_capacity) {
                int r = g_removal_repair_count++;
                g_removal_repair_ptrs[r] = veh;
                g_removal_repair_saved[r] = trigger_bits;
                char buf[176];
                wsprintfA(buf, "RF-E3A4A4A7C1D %d %s %d %d",
                          veh_slot(veh), veh_name(veh), (int)trigger, (int)ai_time);
                log_w(buf);
            }
            *(volatile DWORD*)(veh + OFF_AI_PIT_TRIGGER) = FLT_MAX_BITS;
        }
    }
}

static void vehicle_removal_restore(void) {
    if (!g_vehicle_removal_forced) return;
    InterlockedExchange(&g_cleanup_loose_active, 0);
    InterlockedIncrement(&g_cleanup_loose_generation);
    /* A task not yet executing can be cancelled locally. An executing task
     * observes the changed generation before any later write/publication. */
    (void)InterlockedCompareExchange(&g_cleanup_loose_task_pending, 0, 1);
    uintptr_t base = PLAYER_OPTIONS_BASE;
    *VEHICLE_REMOVAL_RUNTIME = rf_cleanup_normal_bits(0);
    if (base == g_vehicle_removal_options_base) {
        for (int i = 0; i < CLEANUP_OPTION_COUNT; i++) {
            uintptr_t entry = base + VEHICLE_REMOVAL_ENTRY_OFF
                            + i * CLEANUP_OPTION_STRIDE;
            *(volatile DWORD*)(entry + OPTION_VALUE_A_OFF) = rf_cleanup_normal_bits(i);
            *(volatile DWORD*)(entry + OPTION_VALUE_B_OFF) = rf_cleanup_normal_bits(i);
        }
        log_w("RF-EF1B368069A");
    } else {
        log_w("RF-EA4E2597E23");
    }
    /* If native removal completed, RetiredFlag is set and the repaired sentinel
     * is now legitimate game state. On abort/fallback, restore the exact stale
     * trigger only if the same field still exists and the game did not replace
     * our FLT_MAX write with a newer value. */
    if (session_epoch() == g_removal_repair_epoch) {
        for (int r = 0; r < g_removal_repair_count; r++) {
            char* veh = (char*)g_removal_repair_ptrs[r];
            if (!veh || *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
            if (*(volatile DWORD*)(veh + OFF_AI_PIT_TRIGGER) == FLT_MAX_BITS)
                *(volatile DWORD*)(veh + OFF_AI_PIT_TRIGGER) =
                    g_removal_repair_saved[r];
        }
    }
    g_vehicle_removal_forced = FALSE;
    g_vehicle_removal_options_base = 0;
    g_vehicle_removal_runtime_saved = 0;
    g_removal_repair_count = 0;
    g_removal_repair_epoch = 0;
}

static void hud_write_red_flag(void) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    if (!inner) return;
    for (int n = 0; n < SLOT_COUNT; n++) {
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (*timer <= 0.0f) {
            char *text = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
            strncpy(text, "RED FLAG", 9);
            *timer = 1.5f;
            log_w("RF-EA16A7971EE");
            return;
        }
    }
    log_w("RF-E03FB120EEC");
}

static void hud_refresh_red_flag(void) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    if (!inner) return;
    BOOL found = FALSE;
    for (int n = 0; n < SLOT_COUNT; n++) {
        char  *text  = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (strncmp(text, "RED FLAG", 8) == 0) {
            *timer = 1.5f;
            found = TRUE;
            break;
        }
    }
    if (!found) hud_write_red_flag();
}

static void hud_countdown(int sec) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    if (!inner) return;
    char msg[32];
    wsprintfA(msg, "RACE RESTART IN %d", sec);
    for (int n = 0; n < SLOT_COUNT; n++) {
        char  *text  = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (strncmp(text, "RACE RESTART IN", 15) == 0) {
            strncpy(text, msg, 31);
            text[31] = '\0';
            *timer = 1.5f;
            return;
        }
    }
    for (int n = 0; n < SLOT_COUNT; n++) {
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (*timer <= 0.0f) {
            char *text = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
            strncpy(text, msg, 31);
            text[31] = '\0';
            *timer = 1.5f;
            return;
        }
    }
}

static void hud_preparing_restart(void) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    if (!inner) return;
    for (int n = 0; n < SLOT_COUNT; n++) {
        char  *text  = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (strncmp(text, "RACE RESTART IN", 15) == 0
            || strncmp(text, "PREPARING RESTART", 17) == 0) {
            strncpy(text, "PREPARING RESTART", 31);
            text[31] = '\0';
            *timer = 1.5f;
            return;
        }
    }
}

static void hud_clearing_track(void) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    if (!inner) return;
    for (int n = 0; n < SLOT_COUNT; n++) {
        char  *text  = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (strncmp(text, "RACE RESTART IN", 15) == 0
            || strncmp(text, "PREPARING RESTART", 17) == 0
            || strncmp(text, "CLEARING THE TRACK", 18) == 0) {
            strncpy(text, "CLEARING THE TRACK", 31);
            text[31] = '\0';
            *timer = 1.5f;
            return;
        }
    }
}

static void hud_clear_countdown(void) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    if (!inner) return;
    for (int n = 0; n < SLOT_COUNT; n++) {
        char  *text  = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (strncmp(text, "RACE RESTART IN", 15) == 0
            || strncmp(text, "PREPARING RESTART", 17) == 0
            || strncmp(text, "CLEARING THE TRACK", 18) == 0) {
            *timer = -1.0f;
            text[0] = '\0';
        }
    }
}

#define TRACK_LAP_LENGTH_OFF      316u
#define TRACK_SECTOR1_LENGTH_OFF  320u
#define TRACK_SECTOR2_LENGTH_OFF  324u

/* Sector boundaries come from the active track database. Most tracks store
 * Sector1Length/Sector2Length as individual lengths; tolerate a cumulative
 * second split too. AIW progress is a bounded fallback for incomplete track
 * metadata. This is captured when the incident qualifies, never recomputed
 * after wrecks are removed or the field is rebuilt. */
static int incident_sector_from_location(float lap_distance) {
    float lap = *(volatile float*)(SCENE_STRUCT + TRACK_LAP_LENGTH_OFF);
    float first = *(volatile float*)(SCENE_STRUCT + TRACK_SECTOR1_LENGTH_OFF);
    float second = *(volatile float*)(SCENE_STRUCT + TRACK_SECTOR2_LENGTH_OFF);
    if (isfinite(lap_distance) && isfinite(lap) && isfinite(first) &&
        isfinite(second) && lap > 100.0f && first > 0.0f && first < lap &&
        second > 0.0f && second < lap) {
        while (lap_distance < 0.0f) lap_distance += lap;
        while (lap_distance >= lap) lap_distance -= lap;
        float second_boundary = first + second;
        /* A cumulative Sector2Length is normally greater than sector 1 and
         * makes first+second implausibly close to/past the full lap. */
        if (second > first && second_boundary > lap * 0.90f)
            second_boundary = second;
        if (second_boundary > first && second_boundary < lap) {
            if (lap_distance < first) return 1;
            if (lap_distance < second_boundary) return 2;
            return 3;
        }
    }
    if (g_aiw_main_waypoints > 0 && g_aiw_blocking_nearest_index >= 0 &&
        g_aiw_blocking_nearest_index < g_aiw_main_waypoints) {
        int sector = 1 + (g_aiw_blocking_nearest_index * 3) /
                         g_aiw_main_waypoints;
        return sector > 3 ? 3 : sector;
    }
    return 0;
}

static void hud_auto_notice_clear(void) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    for (int line = 0; line < AUTO_NOTICE_HUD_LINES; ++line) {
        int slot = g_auto_notice_hud_slots[line];
        if (inner && slot >= 0 && slot < SLOT_COUNT &&
            g_auto_notice_hud_text[line][0]) {
            char* text = (char*)(inner + slot * SLOT_STRIDE + SLOT_TEXT_OFF);
            float* timer = (float*)(inner + slot * SLOT_STRIDE +
                                    SLOT_TIMER_OFF);
            if (strncmp(text, g_auto_notice_hud_text[line],
                        sizeof(g_auto_notice_hud_text[line])) == 0)
                *timer = -1.0f;
        }
        g_auto_notice_hud_slots[line] = -1;
        g_auto_notice_hud_text[line][0] = '\0';
    }
}

static void hud_auto_notice_write(int line, const char* detail) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    char msg[32];
    if (!inner || line < 0 || line >= AUTO_NOTICE_HUD_LINES || !detail) return;
    strncpy(msg, detail, 31);
    msg[31] = '\0';
    if (g_auto_notice_hud_slots[line] >= 0 &&
        g_auto_notice_hud_slots[line] < SLOT_COUNT &&
        g_auto_notice_hud_text[line][0]) {
        int n = g_auto_notice_hud_slots[line];
        char* text = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
        float* timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (strncmp(text, g_auto_notice_hud_text[line],
                    sizeof(g_auto_notice_hud_text[line])) == 0) {
            strncpy(text, msg, 31);
            text[31] = '\0';
            strncpy(g_auto_notice_hud_text[line], msg, 31);
            g_auto_notice_hud_text[line][31] = '\0';
            *timer = 1.5f;
            return;
        }
        /* The slot was reused by the game or another producer. Forget it and
         * acquire only a genuinely free slot below. */
        g_auto_notice_hud_slots[line] = -1;
        g_auto_notice_hud_text[line][0] = '\0';
    }
    for (int n = 0; n < SLOT_COUNT; ++n) {
        float* timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (*timer <= 0.0f) {
            char* text = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
            strncpy(text, msg, 31);
            text[31] = '\0';
            g_auto_notice_hud_slots[line] = n;
            strncpy(g_auto_notice_hud_text[line], msg, 31);
            g_auto_notice_hud_text[line][31] = '\0';
            *timer = 1.5f;
            return;
        }
    }
}

/* Owner-approved fixed presentation: exactly three simultaneous lines, with
 * no rotating counts, secondary diagnosis, acceptance prompt or location
 * prose. Each line fits the proven 31-character native HUD limit. */
static void hud_auto_notice_update(void) {
    char reason[32];
    char sector[32];
    if (g_rf_config.require_manual_acceptance) {
        hud_auto_notice_write(0, "RED FLAG ACTIVATION");
        hud_auto_notice_write(1, "RACE WILL BE STOPPED");
        rf_key_hint(2,"TO CONFIRM");
        return;
    }
    switch (g_auto_notice_reason) {
    case 1: strcpy(reason, "MULTI-CAR CRASH"); break;
    case 2: strcpy(reason, "MULTI-CAR CRASH"); break;
    case 4: strcpy(reason, "BLOCKED RACING LINE"); break;
    case 5: strcpy(reason, "WEATHER"); break;
    case 6: strcpy(reason, "OVERTURNED CAR"); break;
    default: strcpy(reason, "SEVERE INCIDENT"); break;
    }
    if (g_auto_notice_sector > 0)
        wsprintfA(sector, "SECTOR %d", g_auto_notice_sector);
    else
        strcpy(sector, "SECTOR UNKNOWN");
    reason[31] = '\0';
    sector[31] = '\0';
    hud_auto_notice_write(0, "RED FLAG WARNING");
    hud_auto_notice_write(1, reason);
    hud_auto_notice_write(2, sector);
}

/* Patch FUN_004D82D0: change VehicleFlagCheck output from blue (0x41) to red (0x40) flag sprite.
 * Called once at DLL load. Safe: .text is PAGE_EXECUTE_READ, VP makes it writable transiently. */
static BOOL patch_one_byte(DWORD site, BYTE expect, BYTE replacement, const char* label) {
    BYTE cur = *(volatile BYTE*)site;
    char buf[128];
    if (cur == replacement) {
        wsprintfA(buf, "RF-E2E1A5544C5 %s %02X %X", label, replacement, site);
        log_w(buf);
        return TRUE;
    }
    if (cur != expect) {
        wsprintfA(buf, "RF-E4B3BC78A05 %s %02X %X", label, cur, site);
        log_w(buf);
        return FALSE;
    }
    DWORD old;
    if (!VirtualProtect((LPVOID)site, 1, PAGE_EXECUTE_READWRITE, &old)) {
        wsprintfA(buf, "RF-EBF109C8ADD %s %X", label, site);
        log_w(buf);
        return FALSE;
    }
    *(BYTE*)site = replacement;
    VirtualProtect((LPVOID)site, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (LPVOID)site, 1);
    wsprintfA(buf, "RF-EDBC34B28B7 %s %02X %02X %X", label, expect, replacement, site);
    log_w(buf);
    return TRUE;
}

static void unpatch_red_flag_sprite(void);

static BOOL patch_red_flag_sprite(void) {
    const DWORD sites[2] = {SPRITE_PATCH_SITE, SPRITE_PATCH_SITE2};
    const BYTE* expected[2] = {g_icon_renderer1, g_icon_renderer2};
    int i;
    if (g_icon_owned[0] || g_icon_owned[1])
        return g_icon_owned[0] && g_icon_owned[1];
    /* Validate BOTH complete instruction windows before changing either. */
    for (i = 0; i < 2; ++i) {
        if (memcmp((const void*)(sites[i] - 8), expected[i], 9)) {
            log_w("RF-E52E36439AF");
            return FALSE;
        }
    }
    for (i = 0; i < 2; ++i) {
        g_icon_owned[i] = TRUE;
        /* Red first, force visibility second. Restore in reverse order. */
        if (!patch_one_byte(sites[i], 0x41, 0x40, "red_icon_sprite") ||
            !patch_one_byte(sites[i] - 5, 0x06, 0x00, "red_icon_visible")) {
            unpatch_red_flag_sprite();
            return FALSE;
        }
    }
    log_w("RF-EEC1D1B42F3");
    return TRUE;
}

/* Put the blue-flag immediate back. Needed for the hot-swap: this byte patch
 * lives in the EXE, not in this module, so without it an ejected DLL would
 * leave the game drawing a red flag sprite for the rest of the session - and
 * it is also exactly the write that got this mod pulled from the pack for
 * being invasive merely by being loaded. Leave nothing behind. */
static void unpatch_red_flag_sprite(void) {
    const DWORD sites[2] = {SPRITE_PATCH_SITE, SPRITE_PATCH_SITE2};
    int i;
    for (i = 0; i < 2; ++i) {
        if (!g_icon_owned[i]) continue;
        if (!patch_one_byte(sites[i] - 5, 0x00, 0x06, "red_icon_native_gate"))
            continue;
        if (patch_one_byte(sites[i], 0x40, 0x41, "red_icon_native_sprite"))
            g_icon_owned[i] = FALSE;
    }
}

static void show_red_flag_icon(void) {
    (void)patch_red_flag_sprite();
}

/* Visibility belongs to the scoped renderer override, not a worker write
 * racing the native blue-flag producer. Retained as a phase-loop no-op. */
static void refresh_red_flag_icon(void) {
}

static void hide_red_flag_icon(void) {
    /* Preserve native blue-flag state; restoration needs no vehicle pointer. */
    unpatch_red_flag_sprite();
}

/* ActiveFlag is a float ~1.0 while running, 0.0 once the car is killed. */
/* ActiveFlag (veh+0x45F4) is an INT32: 1 = car is live, 0 = killed/retired.
 *
 * gr30/gr31/gr32 read it as a float and tested (a > -0.5 && a < 0.5). Integer 1
 * reinterpreted as a float is 1.4e-45, which is inside that window, so the test
 * reported EVERY healthy car as killed. Verified live 2026-07-24: running cars
 * read int 1 (raw bytes 01000000), retired cars read int 0; a float 1.0 would
 * have read as 1065353216.
 *
 * This was latent in gr30 — the auto-detect only fires on a false->true edge and
 * detect_seed() seeds every car as already-killed, so it never triggered (which
 * also means the "auto red flag on 3-car pileup" feature has never once fired).
 * gr32's sweep_late_dnf() used the value directly and DNF-ed the whole field. */
static BOOL veh_killed(char* veh) {
    return *(volatile int*)(veh + OFF_ACTIVEFLAG) == 0;
}

/* An ordinary retirement also clears ActiveFlag, so it must never be enough
 * to qualify for an automatic red flag. A qualifying terminal needs a dead
 * engine AND evidence of physical/mechanical damage. The rear-wing condition
 * is the game's own confirmed damage record; the remaining fields are
 * reviewed, read-only vehicle-health signals. */
static BOOL veh_crash_terminal(char* veh) {
    if (!veh_killed(veh)) return FALSE;
    /* A car whose official retirement flag is already set is historical
     * race state, including a carry-over DNF from an earlier red flag. It may
     * remain damaged and engine-off forever, but it is not a new incident. */
    if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) return FALSE;
    if (*(volatile float*)(veh + OFF_ENGINE_RPM) > ENGINE_OFF_RPM) return FALSE;
    return veh_damage_evidence(veh);
}

/* Any of the crash-damage signals. Shared by veh_crash_terminal (which also
 * requires ActiveFlag==0) and the crashed/immobilized counter (which does
 * not: a car the player just hit hard reads engine-off + damage while the
 * game is still classifying it, and calling that a "stall" was wrong).
 * Life fields are SECONDS remaining (name_index FACT: drain 1.000/s, fail
 * at <=0, healthy 20000-41000); CRITICAL_LIFE is the death zone. */
static BOOL veh_damage_evidence(char* veh) {
    if (*(volatile int*)(veh + OFF_REARWING_PTR) != 0 &&
        *(volatile BYTE*)(veh + OFF_REARWING_FLAG) == 0)
        return TRUE;
    if (*(volatile int*)(veh + OFF_FRONTWING_PTR) != 0 &&
        *(volatile BYTE*)(veh + OFF_FRONTWING_FLAG) == 0)
        return TRUE;
    if (*(volatile int*)(veh + OFF_CAR_CONDITIONS) != 0) return TRUE;
    if (*(volatile float*)(veh + OFF_ENGINE_LIFE) <= CRITICAL_LIFE) return TRUE;
    if (*(volatile float*)(veh + OFF_BRAKE_LIFE) <= CRITICAL_LIFE) return TRUE;
    if (*(volatile float*)(veh + OFF_GEARBOX_LIFE) <= CRITICAL_LIFE) return TRUE;
    if (*(volatile float*)(veh + OFF_SUSPENSION_LIFE) <= CRITICAL_LIFE) return TRUE;
    return FALSE;
}

/* Damage severe enough to support an incident score while the car is still
 * moving. A detached wing alone is intentionally excluded: F1C commonly lets
 * cars continue after wing loss, which is a yellow-flag incident, not a red
 * flag. carConditions >= 2 is wheel-loss/severe damage; exhausted component
 * life is an equivalent mechanical immobilisation signal. */
static BOOL veh_heavy_damage_evidence(char* veh) {
    if (*(volatile int*)(veh + OFF_CAR_CONDITIONS) >= 2) return TRUE;
    if (*(volatile float*)(veh + OFF_ENGINE_LIFE) <= CRITICAL_LIFE) return TRUE;
    if (*(volatile float*)(veh + OFF_BRAKE_LIFE) <= CRITICAL_LIFE) return TRUE;
    if (*(volatile float*)(veh + OFF_GEARBOX_LIFE) <= CRITICAL_LIFE) return TRUE;
    if (*(volatile float*)(veh + OFF_SUSPENSION_LIFE) <= CRITICAL_LIFE) return TRUE;
    return FALSE;
}

/* F1C stores the chassis rotation as a 3x3 local-to-world matrix at
 * veh+0x700..0x724. WheelContactTransform_004EF180 proves that 0x720 (physics +0x714, vehicle +12) is the
 * world-Y component of the car's local up axis: +1 is upright, 0 is on its
 * side and -1 is roof-down. This is read-only and deliberately requires a
 * clearly negative, finite value; banking, kerbs and ordinary spins cannot
 * satisfy the default -0.25 threshold. */
static BOOL veh_is_confirmably_inverted(char* veh) {
    F1COrientation orientation=f1c_orientation_vehicle(veh);
    return orientation.valid && orientation.up_dot<=g_rf_config.inverted_up_dot_max;
}

/* A notice begins only while the ordinary freshness window is valid. Keep
 * that already-proven evidence eligible for one complete notice interval so
 * IncidentFreshnessSeconds == PreStopNoticeSeconds cannot expire a few
 * milliseconds before the mandatory final recheck. Physical state is still
 * evaluated on every refresh: a recovered, retired, moved or pitted car clears
 * immediately. Only the age ceiling is extended while a notice is active. */
static DWORD incident_freshness_limit(void) {
    DWORD limit = g_rf_config.incident_window_ms;
    if (g_auto_notice_reason != 0 && !g_auto_notice_fired) {
        DWORD extension = g_rf_config.prestop_notice_ms + 1000u;
        if (limit > MAXDWORD - extension) return MAXDWORD;
        limit += extension;
    }
    return limit;
}

/* Score one AI car for the automatic red-flag incident model. Owner policy
 * excludes the player even for damage, terminal state or inversion. Manual
 * activation is separate and keeps its existing eligibility checks.
 *
 *   terminal crash                         4 points
 *   stationary, damaged, still active      3 points
 *   stationary, engine-running car         2 points
 *   slow car with severe damage             1 point
 *
 * A benign engine-off stall without damage scores zero. Every non-terminal
 * state must persist before it contributes, preventing braking/spins and a
 * single noisy sample from arming the sequence. confirmed_at is the start of
 * the six-second fresh-incident window shared with terminal cars. */
static int incident_points(char* veh, int slot, DWORD now, BOOL opening,
                           BOOL* terminal_out,
                           BOOL* damaged_stationary_out) {
    *terminal_out = FALSE;
    *damaged_stationary_out = FALSE;
    if (slot < 0 || slot >= g_session_capacity) return 0;
    if (!veh || (uintptr_t)veh == PLAYER_VEH_PTR) goto clear;
    if (veh_effectively_in_pits(veh)) goto clear;

    if (veh_crash_terminal(veh)) {
        DWORD need = opening ? g_rf_config.opening_incident_confirm_ms
                             : g_rf_config.terminal_confirm_ms;
        g_incident_since[slot] = 0;
        if (kill_duration(veh) < need) {
            g_term_times[slot] = 0;
            return 0;
        }
        *terminal_out = TRUE;
        if (g_term_times[slot] == 0) g_term_times[slot] = now;
        return now - g_term_times[slot] <= incident_freshness_limit() ? 4 : 0;
    }

    if (*(volatile int*)(veh + OFF_ACTIVEFLAG) != 1 ||
        *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0)
        goto clear;

    float speed = *(volatile float*)(veh + OFF_SPEED);
    float rpm = *(volatile float*)(veh + OFF_ENGINE_RPM);
    BOOL damaged = veh_damage_evidence(veh);
    int points = 0;
    DWORD need = 0;

    if (speed < g_rf_config.stationary_speed_mps) {
        /* A dead-engine, undamaged car is the recoverable stalled bucket. */
        if (rpm <= ENGINE_OFF_RPM && !damaged) goto clear;
        points = damaged ? 3 : 2;
        *damaged_stationary_out = damaged;
        need = opening ? g_rf_config.opening_incident_confirm_ms
                       : g_rf_config.stationary_confirm_ms;
    } else if (speed < g_rf_config.slow_damage_speed_mps &&
               veh_heavy_damage_evidence(veh)) {
        points = 1;
        need = g_rf_config.damage_confirm_ms;
    } else {
        goto clear;
    }

    if (g_incident_since[slot] == 0) {
        g_incident_since[slot] = now;
        g_term_times[slot] = 0;
        return 0;
    }
    if (now - g_incident_since[slot] < need) return 0;
    if (g_term_times[slot] == 0) g_term_times[slot] = now;
    return now - g_term_times[slot] <= incident_freshness_limit()
        ? points : 0;

clear:
    g_incident_since[slot] = 0;
    g_term_times[slot] = 0;
    return 0;
}

/* Non-player cars that are engine-off (RPM <= ENGINE_OFF_RPM) but otherwise
 * alive: Active=1, not retired, on track, and NOT crash-terminal. A stall is
 * the recoverable Diniz/Panis case - indistinguishable from a healthy car on
 * every field except RPM - and must never count toward a red-flag cluster.
 * Mirrors tools/field_reviewer.py's STALLED bucket. */
static int count_stalled(void) {
    if (g_monitor_phase >= 2 && g_monitor_phase <= 3) return 0;
    uintptr_t epoch0 = session_epoch();
    uintptr_t player = PLAYER_VEH_PTR;
    int count = VEHICLE_COUNT;
    int n = 0;
    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        if ((uintptr_t)veh == player) continue;
        if (!vehicle_looks_live(veh)) continue;
        if (*(volatile int*)(veh + OFF_ACTIVEFLAG) != 1) continue;
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
        if (veh_effectively_in_pits(veh)) continue;
        if (veh_crash_terminal(veh)) continue;
        if (veh_damage_evidence(veh)) continue;
        if (*(volatile float*)(veh + OFF_ENGINE_RPM) <= ENGINE_OFF_RPM) n++;
    }
    if (session_epoch() != epoch0) return 0;
    return n;
}

/* Non-player cars that are engine-on but braked to a halt (speed < 0.1 m/s) -
 * NT2004's "stopped" predicate (sub_4F93E0). Distinct from STALLED (engine
 * off): a coasting dead-engine car is stalled, a braked-running car is
 * stopped. Never counts toward a red-flag cluster on its own. */
static int count_stopped(void) {
    if (g_monitor_phase >= 2 && g_monitor_phase <= 3) return 0;
    uintptr_t epoch0 = session_epoch();
    uintptr_t player = PLAYER_VEH_PTR;
    int count = VEHICLE_COUNT;
    int n = 0;
    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        if ((uintptr_t)veh == player) continue;
        if (!vehicle_looks_live(veh)) continue;
        if (*(volatile int*)(veh + OFF_ACTIVEFLAG) != 1) continue;
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
        if (veh_effectively_in_pits(veh)) continue;
        if (veh_crash_terminal(veh)) continue;
        if (*(volatile float*)(veh + OFF_ENGINE_RPM) <= ENGINE_OFF_RPM) continue;
        if (*(volatile float*)(veh + OFF_SPEED) < 0.1f) n++;
    }
    if (session_epoch() != epoch0) return 0;
    return n;
}

/* Non-player cars that are engine-off AND damaged but still alive (Active==1,
 * not retired): a hard crash the game has not yet classified. Distinct from
 * STALLED (engine off, no damage - a benign stall) and TERMINAL (the game
 * has killed the car). */
static int count_crashed(void) {
    if (g_monitor_phase >= 2 && g_monitor_phase <= 3) return 0;
    uintptr_t epoch0 = session_epoch();
    uintptr_t player = PLAYER_VEH_PTR;
    int count = VEHICLE_COUNT;
    int n = 0;
    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        if ((uintptr_t)veh == player) continue;
        if (!vehicle_looks_live(veh)) continue;
        if (*(volatile int*)(veh + OFF_ACTIVEFLAG) != 1) continue;
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
        if (veh_effectively_in_pits(veh)) continue;
        if (*(volatile float*)(veh + OFF_ENGINE_RPM) > ENGINE_OFF_RPM) continue;
        if (veh_damage_evidence(veh)) n++;
    }
    if (session_epoch() != epoch0) return 0;
    return n;
}

static void monitor_jos_verstappen(char* veh) {
    if (!veh || !strstr(veh_name(veh), "Jos Verstappen")) return;
    g_monitor_jos_found = TRUE;
    g_monitor_jos_active = *(volatile int*)(veh + OFF_ACTIVEFLAG);
    g_monitor_jos_retired = *(volatile BYTE*)(veh + OFF_RETIREDFLAG);
    g_monitor_jos_pitstate = *(volatile int*)(veh + OFF_PITSTATE);
    g_monitor_jos_damage = *(volatile int*)(veh + OFF_CAR_CONDITIONS);
    g_monitor_jos_rpm = *(volatile float*)(veh + OFF_ENGINE_RPM);
    g_monitor_jos_engine_life = *(volatile float*)(veh + OFF_ENGINE_LIFE);
    g_monitor_jos_gearbox_life = *(volatile float*)(veh + OFF_GEARBOX_LIFE);
    g_monitor_jos_suspension_life = *(volatile float*)(veh + OFF_SUSPENSION_LIFE);
}

/* Seed the per-slot killed cache so cars already out / not yet spawned
 * (or before a new race) are NOT counted as fresh terminal-damage edges. */
static void detect_seed(void) {
    int count = VEHICLE_COUNT;
    for (int i = 0; i < g_session_capacity; i++) {
        g_last_killed[i] = FALSE;
        g_term_times[i] = 0;
        g_incident_since[i] = 0;
        g_stationary_hazard_since[i] = 0;
        g_inverted_hazard_since[i] = 0;
    }
    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        g_last_killed[i] = veh_killed(veh);
    }
    g_term_times_n = 0;
    g_auto_candidate_since = 0;
    g_placeholder_logged = FALSE;
    auto_notice_reset();
    g_monitor_stop_reason = 0;
    g_monitor_prestop_elapsed_ms = 0;
    g_monitor_blocking_road_contacts = 0;
    g_det_seeded = TRUE;
    log_w("RF-EA72D927CB0");
}

/* Settled-race gate. Auto-detect must never run while a grid restart or
 * formation is dropping cars to ActiveFlag=0 - the gr34 mis-fire came from
 * exactly that (a restart looks like a pileup inside the terminal window).
 *
 * GATE VALUES ARE EVIDENCE-VERIFIED, NOT TAKEN FROM DOCS 49. Docs 49 (4.3/4.4)
 * lists GameState 7=race running and RacePhase 3=GREEN_FLAG as the racing
 * values, but a live yellow-flag race on the trial build (2026-08-17, PID
 * 28244) reads GameState=5 AND RacePhase=5, sample-stable, with cars at
 * 34-62 m/s - and the mods' own proven usage agrees (pace_probe gated every
 * pace call on RacePhase==5; red_flag phase 2 already treats GameState==5 as
 * "restart complete / race running"). Docs 49's "5=FINISHING" is contradicted
 * by this live read. So the gate accepts the union of both value sets and
 * excludes the phases that can never be racing: RacePhase 2 (GRID_READY /
 * gridwalk - the gr34 mis-fire state) and 8 (DONE). */
static BOOL race_is_settled(void) {
    if (SESSION_TYPE != 7) return FALSE;
    if (!field_size_supported()) return FALSE;
    int gs = GAME_STATE;
    int rp = RACE_PHASE;
    if (gs != 5 && gs != 7) return FALSE;
    if (rp != 3 && rp != 5) return FALSE;
    return TRUE;
}

/* Native-yellow read WITHOUT calling the game's sub_49BAD0().
 *
 * Calling sub_49BAD0 from a foreign thread HANGS the caller (observed
 * 2026-08-17: the worker thread froze on the first call and the monitor was
 * stuck on "Starting monitor..."). It runs DistanceProjectionCalc +
 * PositionChangeMonitor + ring-buffer lookups against game-thread state.
 *
 * FULL-COURSE READ (2026-08-17 fix): the original gate projected only the
 * PLAYER's position into the steward zone table, so a crash away from the
 * player could never satisfy it while an unrelated local yellow in the
 * player's sector could satisfy it for a crash elsewhere. A red flag is a
 * full-course condition, so the gate now returns TRUE when ANY zone in the
 * steward table reads yellow. The zone table and state encoding were
 * calibrated live against a displayed yellow flag (PID 28244, 29 zones,
 * zones 2+3 state 2):
 *
 *   steward static  = 0x8ACEE0  (scale float +264, count +268, table +272)
 *   zone state      = *(table + 8*i) == 2   (yellow), for any i in [0, count)
 *
 * The player-vehicle projection offsets below are preserved calibration
 * evidence for the future per-AI native-sector lookup; the detector itself
 * no longer needs them.
 */
#define YELLOW_STEWARD      0x008ACEE0u
#define YELLOW_OFF_SCALE    264
#define YELLOW_OFF_COUNT    268
#define YELLOW_OFF_TABLE    272
#define YELLOW_VEH_PTR      0x007C54E0u
#define YELLOW_OFF_MONITOR  15016
#define YELLOW_OFF_SEGSTART 268
#define YELLOW_OFF_SEGEND   272
#define YELLOW_OFF_DISTAT   72
#define YELLOW_OFF_PLAYERPOS 13536
#define YELLOW_TRACKLEN     0x005EA3FCu
#define YELLOW_ZONE_STATE   2

static BOOL native_yellow_read(void) {
    int count = *(volatile int*)(YELLOW_STEWARD + YELLOW_OFF_COUNT);
    uintptr_t table = *(volatile uintptr_t*)(YELLOW_STEWARD + YELLOW_OFF_TABLE);
    if (count <= 0 || count > 64 || !table ||
        IsBadReadPtr((void*)table, count * 8))
        return FALSE;
    for (int i = 0; i < count; i++)
        if (*(volatile int*)(table + 8 * i) == YELLOW_ZONE_STATE)
            return TRUE;
    return FALSE;
}

/* Native-yellow refresh. Mirrors the predicate F1C's HUD flag renderer uses
 * (sub_49BAD0, replicated as pure reads - see native_yellow_read), so when
 * the game is showing a yellow the monitor must read TRUE here. Refreshed by
 * BOTH the monitor (every 150 ms) and the detector, so the GUI never shows
 * stale flag state. */
static void refresh_yellow_state(DWORD now) {
    /* A paused game must not accumulate yellow persistence time: the flag
     * can be up while the user pauses, and the 3 s gate is race time, not
     * wall time. Freeze the clock; it resumes on unpause. */
    if (PAUSE_FLAG) return;
    /* Field-rebuild guard, same class as the counter walks. */
    uintptr_t epoch0 = session_epoch();
    g_monitor_native_yellow = native_yellow_read();
    if (!g_monitor_native_yellow) {
        g_native_yellow_since = 0;
        g_monitor_yellow_ms = 0;
    } else if (g_native_yellow_since == 0) {
        g_native_yellow_since = now;
        g_monitor_yellow_ms = 0;
    } else {
        g_monitor_yellow_ms = now - g_native_yellow_since;
    }
    if (session_epoch() != epoch0) {
        g_monitor_native_yellow = FALSE;
        g_monitor_yellow_ms = 0;
    }
}

/* A zeroed or half-populated vehicle struct (menu, track load, session
 * setup) must never count as an incident: everything reads 0 there and would
 * look crash-terminal (life 0 passes the <= CRITICAL_LIFE test). Names and
 * QualPos are assigned before the life counters are seeded, so QualPos alone
 * is NOT enough - require real car evidence: motion, damage, or seeded
 * component lives. */
static BOOL vehicle_looks_live(char* veh) {
    if (*(volatile float*)(veh + OFF_ENGINE_RPM) > 0.0f) return TRUE;
    if (*(volatile float*)(veh + OFF_SPEED) > 0.1f) return TRUE;
    if (*(volatile int*)(veh + OFF_CAR_CONDITIONS) != 0) return TRUE;
    if (*(volatile float*)(veh + OFF_ENGINE_LIFE) > 1.0f) return TRUE;
    if (*(volatile float*)(veh + OFF_BRAKE_LIFE) > 1.0f) return TRUE;
    if (*(volatile float*)(veh + OFF_GEARBOX_LIFE) > 1.0f) return TRUE;
    if (*(volatile float*)(veh + OFF_SUSPENSION_LIFE) > 1.0f) return TRUE;
    return FALSE;
}

/* AIW spatial model (static + live-validated layout shared with the synthetic
 * pit-stop work): scene this[76]=count, this[77]=316-byte node array;
 * SpatialNearestNeighborQueryWithPlaneTest reads node position at +148/152/156,
 * branch ID at +300, and the path renderer follows +272. Branch 0 is the main
 * track, branch 1 is pit lane, 2..25 are grid rows, and 106+ are pit boxes. */
#define AIW_COUNT_OFF             (76u * 4u)
#define AIW_BASE_OFF              (77u * 4u)
#define AIW_NODE_STRIDE           316u
#define AIW_POS_X                 148u
#define AIW_POS_Y                 152u
#define AIW_POS_Z                 156u
#define AIW_PATH_DRY              24u  /* wp_path[0]: dry lateral line */
#define AIW_PATH_WET              28u  /* wp_path[1]: wet lateral line */
#define AIW_PERP_X                32u
#define AIW_PERP_Z                40u
#define AIW_ROAD_LEFT             44u  /* wp_width: road left */
#define AIW_ROAD_RIGHT            48u  /* wp_width: road right */
#define AIW_NEXT_PTR              272u
#define AIW_BRANCH_ID             300u
#define AIW_MAIN_BRANCH           0
#define AIW_PIT_BRANCH            1
#define AIW_MAX_NODES             12000
#define AIW_CAR_HALF_WIDTH_M      1.0f
#define AIW_PASS_CRITICAL_M       3.0f
#define AIW_PASS_DANGEROUS_M      5.0f
#define AIW_STATIONARY_CONFIRM_MS 3000u

/* Largest traversable gap between real wreck footprints and road edges.
 * Inputs are one local cross-section, not unrelated cars around the circuit. */
static int rf_same_cross_section(float a,float b,float lap,float dx,float dy,float dz) {
 if(!isfinite(a)||!isfinite(b)||!isfinite(dx)||!isfinite(dy)||!isfinite(dz)||a<0||b<0)return 0;
 float gap=fabsf(a-b);
 if(isfinite(lap)&&lap>100 && a<=lap && b<=lap)gap=fminf(gap,lap-gap);
 return gap<=4.0f && fabsf(dy)<=2.0f && dx*dx+dz*dz<=64.0f;
}
static int rf_passage_blocked(float road_left,float road_right,float *left,float *right,int n) {
    if(n<2 || n>104 || !isfinite(road_left) || !isfinite(road_right) ||
       road_left<=0 || road_right<=0)return 0;
    for(int i=0;i<n;i++)if(!isfinite(left[i]) || !isfinite(right[i]) || left[i]>right[i])return 0;
    for(int i=1;i<n;i++)for(int j=i;j>0 && left[j]<left[j-1];j--) {
        float t=left[j];left[j]=left[j-1];left[j-1]=t;
        t=right[j];right[j]=right[j-1];right[j-1]=t;
    }
    float edge=-road_left,best=0;
    for(int i=0;i<n;i++) {
        float l=fmaxf(-road_left,fminf(road_right,left[i]));
        float r=fmaxf(-road_left,fminf(road_right,right[i]));
        if(l-edge>best)best=l-edge;
        if(r>edge)edge=r;
    }
    if(road_right-edge>best)best=road_right-edge;
    return best<AIW_PASS_CRITICAL_M;
}

typedef struct {
    int valid;
    int main_waypoints;
    int nearest_index;
    int segment_index;
    float node_m;
    float path_m;
    float dry_line_m;
    float wet_line_m;
    float weather_line_m;
    float closest_line_m;
    float road_left_m;
    float road_right_m;
    float road_width_m;
    float lateral_m;
    float normal_x,normal_z;
    float free_left_m;
    float free_right_m;
    float best_pass_m;
    float critical_radius_m;
    float danger_radius_m;
    int on_road_geom;
} AIWDistance;

static BOOL aiw_finite_coord(float value) {
    return value == value && value > -1000000.0f && value < 1000000.0f;
}

static float aiw_clampf(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float aiw_live_wetness(float value) {
    if (!aiw_finite_coord(value)) return 0.0f;
    return aiw_clampf(value, 0.0f, 1.0f);
}

static float aiw_segment_distance_sq(float px, float pz,
                                     float ax, float az,
                                     float bx, float bz, float* out_t) {
    float sx = bx - ax;
    float sz = bz - az;
    float length_sq = sx * sx + sz * sz;
    float t = 0.0f;
    if (length_sq > 0.0001f)
        t = ((px - ax) * sx + (pz - az) * sz) / length_sq;
    t = aiw_clampf(t, 0.0f, 1.0f);
    float qx = ax + t * sx;
    float qz = az + t * sz;
    float dx = px - qx;
    float dz = pz - qz;
    if (out_t) *out_t = t;
    return dx * dx + dz * dz;
}

/* Shortest X/Z distance to the branch-0 polyline. The nearest-node distance
 * is also retained for calibration, but the segment projection is the danger
 * metric: it is independent of how far apart a track author's AIW points are. */
static AIWDistance aiw_branch_path_distance(char* veh, int branch,
                                             float on_path_wetness) {
    AIWDistance out;
    ZeroMemory(&out, sizeof(out));
    out.nearest_index = -1;
    out.segment_index = -1;
    if (!veh) return out;

    int count = *(volatile int*)(SCENE_STRUCT + AIW_COUNT_OFF);
    uintptr_t base = *(volatile uintptr_t*)(SCENE_STRUCT + AIW_BASE_OFF);
    if (count <= 0 || count > AIW_MAX_NODES || !base) return out;
    uintptr_t bytes = (uintptr_t)count * AIW_NODE_STRIDE;
    uintptr_t end = base + bytes;
    if (end <= base || IsBadReadPtr((void*)base, bytes)) return out;

    float car_x = *(volatile float*)(veh + OFF_POSX);
    float car_z = *(volatile float*)(veh + OFF_POSZ);
    float car_y = *(volatile float*)(veh + OFF_POSY);
    if (!aiw_finite_coord(car_y) || !aiw_finite_coord(car_x) || !aiw_finite_coord(car_z)) return out;

    float best_node_sq = 3.4028235e38f;
    float best_path_sq = 3.4028235e38f;
    float best_dry_sq = 3.4028235e38f;
    float best_wet_sq = 3.4028235e38f;
    float best_weather_sq = 3.4028235e38f;
    int segments = 0;
    for (int i = 0; i < count; i++) {
        uintptr_t node = base + (uintptr_t)i * AIW_NODE_STRIDE;
        if (*(volatile int*)(node + AIW_BRANCH_ID) != branch)
            continue;
        out.main_waypoints++;
        float ax = *(volatile float*)(node + AIW_POS_X);
        float az = *(volatile float*)(node + AIW_POS_Z);
        float ay = *(volatile float*)(node + AIW_POS_Y);
        if (!aiw_finite_coord(ay) || !aiw_finite_coord(ax) || !aiw_finite_coord(az)) continue;

        float ndx = car_x - ax;
        float ndz = car_z - az;
        float node_sq = ndx * ndx + ndz * ndz;
        if (fabsf(car_y-ay)<=2.0f && node_sq < best_node_sq) {
            best_node_sq = node_sq;
            out.nearest_index = i;
        }

        uintptr_t next = *(volatile uintptr_t*)(node + AIW_NEXT_PTR);
        if (next < base || next >= end ||
            ((next - base) % AIW_NODE_STRIDE) != 0 ||
            *(volatile int*)(next + AIW_BRANCH_ID) != branch)
            continue;
        float bx = *(volatile float*)(next + AIW_POS_X);
        float bz = *(volatile float*)(next + AIW_POS_Z);
        float by = *(volatile float*)(next + AIW_POS_Y);
        if (!aiw_finite_coord(by) || !aiw_finite_coord(bx) || !aiw_finite_coord(bz)) continue;
        float t = 0.0f;
        float path_sq = aiw_segment_distance_sq(
            car_x, car_z, ax, az, bx, bz, &t);
        if(fabsf(car_y-(ay+t*(by-ay)))>2.0f)continue; /* different road level */
        if (path_sq < best_path_sq) {
            best_path_sq = path_sq;
            out.segment_index = i;

            float perpx = *(volatile float*)(node + AIW_PERP_X) * (1.0f - t) +
                *(volatile float*)(next + AIW_PERP_X) * t;
            float perpz = *(volatile float*)(node + AIW_PERP_Z) * (1.0f - t) +
                *(volatile float*)(next + AIW_PERP_Z) * t;
            float plen = sqrtf(perpx * perpx + perpz * perpz);
            if (plen > 0.001f) { perpx /= plen; perpz /= plen; }
            out.normal_x=perpx;out.normal_z=perpz;
            float base_x = ax + t * (bx - ax);
            float base_z = az + t * (bz - az);
            out.lateral_m = (car_x - base_x) * perpx +
                            (car_z - base_z) * perpz;
            out.road_left_m =
                *(volatile float*)(node + AIW_ROAD_LEFT) * (1.0f - t) +
                *(volatile float*)(next + AIW_ROAD_LEFT) * t;
            out.road_right_m =
                *(volatile float*)(node + AIW_ROAD_RIGHT) * (1.0f - t) +
                *(volatile float*)(next + AIW_ROAD_RIGHT) * t;
        }

        float adry = *(volatile float*)(node + AIW_PATH_DRY);
        float bdry = *(volatile float*)(next + AIW_PATH_DRY);
        float awet = *(volatile float*)(node + AIW_PATH_WET);
        float bwet = *(volatile float*)(next + AIW_PATH_WET);
        float apx = *(volatile float*)(node + AIW_PERP_X);
        float apz = *(volatile float*)(node + AIW_PERP_Z);
        float bpx = *(volatile float*)(next + AIW_PERP_X);
        float bpz = *(volatile float*)(next + AIW_PERP_Z);
        float dry_sq = aiw_segment_distance_sq(
            car_x, car_z, ax + apx * adry, az + apz * adry,
            bx + bpx * bdry, bz + bpz * bdry, NULL);
        float wet_sq = aiw_segment_distance_sq(
            car_x, car_z, ax + apx * awet, az + apz * awet,
            bx + bpx * bwet, bz + bpz * bwet, NULL);
        float weather_a = adry * (1.0f - on_path_wetness) +
                          awet * on_path_wetness;
        float weather_b = bdry * (1.0f - on_path_wetness) +
                          bwet * on_path_wetness;
        float weather_sq = aiw_segment_distance_sq(
            car_x, car_z, ax + apx * weather_a, az + apz * weather_a,
            bx + bpx * weather_b, bz + bpz * weather_b, NULL);
        if (dry_sq < best_dry_sq) best_dry_sq = dry_sq;
        if (wet_sq < best_wet_sq) best_wet_sq = wet_sq;
        if (weather_sq < best_weather_sq) best_weather_sq = weather_sq;
        segments++;
    }
    if (out.main_waypoints <= 0 || out.nearest_index < 0) return out;
    out.node_m = sqrtf(best_node_sq);
    if (segments > 0 && out.segment_index >= 0)
        out.path_m = sqrtf(best_path_sq);
    else {
        out.path_m = out.node_m;
        out.segment_index = out.nearest_index;
    }
    out.dry_line_m = best_dry_sq < 3.0e38f
        ? sqrtf(best_dry_sq) : out.path_m;
    out.wet_line_m = best_wet_sq < 3.0e38f
        ? sqrtf(best_wet_sq) : out.path_m;
    out.weather_line_m = best_weather_sq < 3.0e38f
        ? sqrtf(best_weather_sq) : out.dry_line_m;
    /* Pit branch wp_path[0] is the commanded pit driving line; wp_path[1]
     * is commonly zero rather than a meaningful wet line. */
    out.closest_line_m = branch == AIW_PIT_BRANCH
        ? out.dry_line_m
        : out.weather_line_m;
    if (out.road_left_m > 0.0f && out.road_left_m < 100.0f &&
        out.road_right_m > 0.0f && out.road_right_m < 100.0f) {
        out.road_width_m = out.road_left_m + out.road_right_m;
        /* Test the vehicle footprint, not only its centre point. A car with
         * its centre just beyond an AIW road edge can still occupy a full
         * metre of usable asphalt and be the most dangerous obstruction. */
        out.on_road_geom =
            out.lateral_m >= -out.road_left_m - AIW_CAR_HALF_WIDTH_M &&
            out.lateral_m <= out.road_right_m + AIW_CAR_HALF_WIDTH_M;
        out.free_left_m = out.road_left_m + out.lateral_m -
                          AIW_CAR_HALF_WIDTH_M;
        out.free_right_m = out.road_right_m - out.lateral_m -
                           AIW_CAR_HALF_WIDTH_M;
        if (out.free_left_m < 0.0f) out.free_left_m = 0.0f;
        if (out.free_right_m < 0.0f) out.free_right_m = 0.0f;
        out.best_pass_m = out.free_left_m > out.free_right_m
            ? out.free_left_m : out.free_right_m;
        /* Track-relative safety envelope: wider corners need a wider danger
         * corridor. The car half-width is included explicitly and the clamps
         * prevent malformed/extreme AIW widths from dominating the rule. */
        out.critical_radius_m = aiw_clampf(
            out.road_width_m * 0.40f + AIW_CAR_HALF_WIDTH_M, 4.5f, 7.5f);
        out.danger_radius_m = aiw_clampf(
            out.road_width_m * 0.70f + AIW_CAR_HALF_WIDTH_M, 7.0f, 12.0f);
    } else {
        out.critical_radius_m = 5.0f;
        out.danger_radius_m = 8.0f;
    }
    float normal_sq=out.normal_x*out.normal_x+out.normal_z*out.normal_z;
    out.valid = segments>0 && isfinite(normal_sq) && normal_sq>=0.99f && normal_sq<=1.01f;
    return out;
}

static int aiw_danger_band(const AIWDistance* metric) {
    if (!metric || !metric->valid) return 0;
    /* The band is deliberately spatial. Collision material and the game's
     * centre-point OffTrack byte are supporting evidence, not reasons to hide
     * a car whose footprint intrudes into the usable road. This also keeps a
     * wheel-less terminal car detectable when all four hit pointers vanish. */
    if (!metric->on_road_geom)
        return metric->closest_line_m <= metric->danger_radius_m + 5.0f ? 2 : 1;
    if (metric->closest_line_m <= metric->critical_radius_m ||
        metric->best_pass_m < AIW_PASS_CRITICAL_M)
        return 4;
    if (metric->closest_line_m <= metric->danger_radius_m ||
        metric->best_pass_m < AIW_PASS_DANGEROUS_M)
        return 3;
    if (metric->closest_line_m <= metric->danger_radius_m + 5.0f)
        return 2;
    return 1;
}

/* Track Status Monitor's physics-thread recipe, generalized from the player to
 * any vehicle. WheelContactTransform writes four current hit pointers at
 * +0x3674 (stride 0xF8); hit+0x54 resolves the material whose sound triplet
 * identifies road/wet-road/rumble versus grass/dirt/gravel/wall. A blocking
 * car may straddle an edge, so count the drivable wheels without discarding
 * them when another wheel is on grass or gravel. */
static int drivable_surface_contacts(char* veh) {
    int contacts = 0;
    for (int wheel = 0; wheel < 4; wheel++) {
        uintptr_t hit = *(volatile uintptr_t*)(veh + OFF_WHEEL_HIT +
                                               wheel * WHEEL_HIT_STRIDE);
        if (!hit || IsBadReadPtr((void*)hit, 0x58)) continue;
        uintptr_t material = *(volatile uintptr_t*)(hit + 0x54u);
        if (!material || IsBadReadPtr((void*)material, 0x3Cu)) continue;
        uint32_t a = *(volatile uint32_t*)(material + 0x30u);
        uint32_t b = *(volatile uint32_t*)(material + 0x34u);
        uint32_t c = *(volatile uint32_t*)(material + 0x38u);
        BOOL drivable = (a == 6u && b == 21u && c == 27u) ||
                        (a == 7u && b == 22u && c == 27u) ||
                        (a == 11u && b == 21u && c == 27u);
        if (drivable) contacts++;
    }
    return contacts;
}

static void refresh_aiw_player_metric(void) {
    g_aiw_valid = 0;
    g_aiw_main_waypoints = 0;
    g_aiw_pit_waypoints = 0;
    g_aiw_player_found = 0;
    g_aiw_player_nearest_index = -1;
    g_aiw_player_segment_index = -1;
    g_aiw_player_band = 0;
    g_aiw_player_corridor = -1;
    g_aiw_player_main_band = 0;
    g_aiw_player_pit_band = 0;
    g_aiw_player_main_line_m = 0.0f;
    g_aiw_player_pit_line_m = 0.0f;
    g_aiw_player_node_m = 0.0f;
    g_aiw_player_path_m = 0.0f;
    g_aiw_player_dry_line_m = 0.0f;
    g_aiw_player_wet_line_m = 0.0f;
    g_aiw_player_closest_line_m = 0.0f;
    g_aiw_player_road_left_m = 0.0f;
    g_aiw_player_road_right_m = 0.0f;
    g_aiw_player_road_width_m = 0.0f;
    g_aiw_player_lateral_m = 0.0f;
    g_aiw_player_free_left_m = 0.0f;
    g_aiw_player_free_right_m = 0.0f;
    g_aiw_player_best_pass_m = 0.0f;
    g_aiw_player_critical_radius_m = 0.0f;
    g_aiw_player_danger_radius_m = 0.0f;
    g_aiw_player_on_road_geom = 0;
    g_aiw_player_surface_contacts = 0;
    g_aiw_player_off_track = 0;
    g_aiw_player_game_paused = PAUSE_FLAG ? 1 : 0;
    g_aiw_weather_on_wetness = aiw_live_wetness(ON_PATH_WETNESS);
    g_aiw_weather_off_wetness = aiw_live_wetness(OFF_PATH_WETNESS);
    g_aiw_weather_mean_wetness =
        (g_aiw_weather_on_wetness + g_aiw_weather_off_wetness) * 0.5f;
    g_aiw_weather_class = g_aiw_weather_on_wetness < 0.01f ? 0 :
        (g_aiw_weather_on_wetness < 0.33f ? 1 :
         (g_aiw_weather_on_wetness < 0.66f ? 2 : 3));
    char* player = (char*)PLAYER_VEH_PTR;
    if (!player) {
        g_aiw_player_stationary_ms = 0;
        g_aiw_player_stationary_last_tick = 0;
        g_aiw_player_stationary_ptr = 0;
        g_aiw_player_stationary_confirmed = 0;
        g_aiw_player_speed = 0.0f;
        return;
    }
    AIWDistance main_metric = aiw_branch_path_distance(
        player, AIW_MAIN_BRANCH, g_aiw_weather_on_wetness);
    AIWDistance pit_metric = aiw_branch_path_distance(
        player, AIW_PIT_BRANCH, g_aiw_weather_on_wetness);
    g_aiw_player_found = 1;
    g_aiw_player_surface_contacts = drivable_surface_contacts(player);
    g_aiw_player_off_track = *(volatile BYTE*)(player + OFF_OFF_TRACK) ? 1 : 0;
    g_aiw_player_speed = *(volatile float*)(player + OFF_SPEED);
    DWORD now = GetTickCount();
    if ((uintptr_t)player != g_aiw_player_stationary_ptr) {
        g_aiw_player_stationary_ptr = (uintptr_t)player;
        g_aiw_player_stationary_ms = 0;
        g_aiw_player_stationary_last_tick = now;
    } else if (g_aiw_player_speed >= 0.5f) {
        g_aiw_player_stationary_ms = 0;
        g_aiw_player_stationary_last_tick = now;
    } else if (PAUSE_FLAG) {
        /* Whole-game pause must never manufacture a stationary hazard. */
        g_aiw_player_stationary_last_tick = now;
    } else {
        DWORD delta = now - g_aiw_player_stationary_last_tick;
        if (delta > 1000u) delta = 1000u;
        g_aiw_player_stationary_ms += delta;
        g_aiw_player_stationary_last_tick = now;
    }
    g_aiw_player_stationary_confirmed =
        g_aiw_player_stationary_ms >= AIW_STATIONARY_CONFIRM_MS ? 1 : 0;
    g_aiw_main_waypoints = main_metric.main_waypoints;
    g_aiw_pit_waypoints = pit_metric.main_waypoints;
    if (main_metric.valid) {
        g_aiw_player_main_band = aiw_danger_band(&main_metric);
        g_aiw_player_main_line_m = main_metric.closest_line_m;
    }
    if (pit_metric.valid) {
        g_aiw_player_pit_band = aiw_danger_band(&pit_metric);
        g_aiw_player_pit_line_m = pit_metric.closest_line_m;
    }
    const AIWDistance* metric = NULL;
    if (main_metric.valid) {
        metric = &main_metric;
        g_aiw_player_corridor = AIW_MAIN_BRANCH;
    }
    if (pit_metric.valid &&
        (!metric || g_aiw_player_pit_band > g_aiw_player_main_band ||
         (g_aiw_player_pit_band == g_aiw_player_main_band &&
          pit_metric.closest_line_m < main_metric.closest_line_m))) {
        metric = &pit_metric;
        g_aiw_player_corridor = AIW_PIT_BRANCH;
    }
    if (!metric) return;
    g_aiw_valid = 1;
    g_aiw_player_nearest_index = metric->nearest_index;
    g_aiw_player_segment_index = metric->segment_index;
    g_aiw_player_node_m = metric->node_m;
    g_aiw_player_path_m = metric->path_m;
    g_aiw_player_dry_line_m = metric->dry_line_m;
    g_aiw_player_wet_line_m = metric->wet_line_m;
    g_aiw_player_closest_line_m = metric->closest_line_m;
    g_aiw_player_road_left_m = metric->road_left_m;
    g_aiw_player_road_right_m = metric->road_right_m;
    g_aiw_player_road_width_m = metric->road_width_m;
    g_aiw_player_lateral_m = metric->lateral_m;
    g_aiw_player_free_left_m = metric->free_left_m;
    g_aiw_player_free_right_m = metric->free_right_m;
    g_aiw_player_best_pass_m = metric->best_pass_m;
    g_aiw_player_critical_radius_m = metric->critical_radius_m;
    g_aiw_player_danger_radius_m = metric->danger_radius_m;
    g_aiw_player_on_road_geom = metric->on_road_geom;
    g_aiw_player_band = aiw_danger_band(metric);
}

static int blocking_lane_road_contacts(char* wreck, int slot, DWORD now,
                                       int* active_qualifying_out,
                                       int* passive_road_hazard_out,
                                       float* aiw_path_m,
                                       float* aiw_line_m, int* aiw_band,
                                       int* aiw_index, int* has_aiw,
                                       int* aiw_corridor) {
    if (active_qualifying_out) *active_qualifying_out = 0;
    if (passive_road_hazard_out) *passive_road_hazard_out = 0;
    if (aiw_path_m) *aiw_path_m = 0.0f;
    if (aiw_line_m) *aiw_line_m = 0.0f;
    if (aiw_band) *aiw_band = 0;
    if (aiw_index) *aiw_index = -1;
    if (has_aiw) *has_aiw = 0;
    if (aiw_corridor) *aiw_corridor = -1;
    if (!wreck || slot < 0 || slot >= g_session_capacity ||
        veh_effectively_in_pits(wreck)) {
        if (slot >= 0 && slot < g_session_capacity) {
            g_stationary_hazard_since[slot] = 0;
            g_inverted_hazard_since[slot] = 0;
        }
        return 0;
    }

    /* Automatic RED FLAG proof is deliberately narrow: a car must be
     * effectively stationary and its physical footprint must overlap the
     * active branch-0 weather racing line. A broad AIW danger radius, limited
     * passing space, pit-lane proximity, damage count or wheel-material hit
     * is not enough. Those remain useful passive diagnostic evidence but
     * cannot stop the race. Active cars retain the strict speed cap; only an
     * already-terminal wreck receives the separate bounded jitter tolerance. */
    BOOL terminal_blocker = veh_crash_terminal(wreck);
    /* A physics-dead wheel-loss chassis can jitter even though it cannot drive
     * away. Give that already-terminal class a separate bounded tolerance;
     * active/recoverable cars retain the strict 1 m/s maximum. */
    float stationary_limit = terminal_blocker
        ? g_rf_config.terminal_racing_line_jitter_speed_mps
        : g_rf_config.racing_line_stationary_speed_mps;
    BOOL active_vehicle =
        *(volatile int*)(wreck + OFF_ACTIVEFLAG) == 1 &&
        *(volatile BYTE*)(wreck + OFF_RETIREDFLAG) == 0;
    /* F1C does not reliably promote a visibly detached wheel to
     * carConditions >= 2 while the chassis remains ActiveFlag==1. Requiring
     * the heavy-only predicate therefore downgraded an immobilised, damaged
     * Alesi on the racing line to a passive road hazard. Any game-confirmed
     * damage is sufficient here because the independent strict speed, exact
     * line-overlap and continuous-dwell gates still have to pass. */
    BOOL active_damaged_blocker = active_vehicle &&
        veh_damage_evidence(wreck);
    if(!terminal_blocker && !veh_is_confirmably_inverted(wreck) &&
       f1c_rc_controlled(g_rc,slot,(uintptr_t)wreck)) {
        g_stationary_hazard_since[slot]=0;g_inverted_hazard_since[slot]=0;
        if(passive_road_hazard_out)*passive_road_hazard_out=1;
        return 0;
    }
    BOOL active_inverted_blocker = active_vehicle &&
        g_rf_config.enable_inverted_blocker &&
        veh_is_confirmably_inverted(wreck);
    if ((!terminal_blocker && !active_vehicle) ||
        *(volatile float*)(wreck + OFF_SPEED) >= stationary_limit) {
        g_stationary_hazard_since[slot] = 0;
        g_inverted_hazard_since[slot] = 0;
        return 0;
    }

    float wetness = aiw_live_wetness(ON_PATH_WETNESS);
    AIWDistance main_metric = aiw_branch_path_distance(
        wreck, AIW_MAIN_BRANCH, wetness);
    /* Exact racing-line overlap is itself the strongest road-location proof.
     * Do not reject it merely because the broader AIW road polygon disagrees,
     * as happened live with Bernoldi. Missing branch geometry still fails
     * closed, and non-overlapping off-road cars remain ineligible below. */
    if (!main_metric.valid) {
        g_stationary_hazard_since[slot] = 0;
        g_inverted_hazard_since[slot] = 0;
        return 0;
    }

    if (aiw_path_m) *aiw_path_m = main_metric.path_m;
    if (aiw_line_m) *aiw_line_m = main_metric.closest_line_m;
    if (aiw_band) *aiw_band = 4;
    if (aiw_index) *aiw_index = main_metric.segment_index;
    if (has_aiw) *has_aiw = 1;
    if (aiw_corridor) *aiw_corridor = AIW_MAIN_BRANCH;
    if (!main_metric.on_road_geom || !isfinite(main_metric.best_pass_m) ||
        main_metric.best_pass_m >= AIW_PASS_CRITICAL_M ||
        main_metric.closest_line_m > AIW_CAR_HALF_WIDTH_M +
        g_rf_config.racing_line_overlap_margin_m) {
        g_stationary_hazard_since[slot] = 0;
        g_inverted_hazard_since[slot] = 0;
        if (main_metric.on_road_geom && passive_road_hazard_out)
            *passive_road_hazard_out = 1;
        return 0;
    }
    /* Both clocks measure continuous overlap, not merely time spent inverted
     * or stationary somewhere else before sliding onto the racing line. */
    if (g_stationary_hazard_since[slot] == 0)
        g_stationary_hazard_since[slot] = now;
    if (active_inverted_blocker) {
        if (g_inverted_hazard_since[slot] == 0)
            g_inverted_hazard_since[slot] = now;
    } else {
        g_inverted_hazard_since[slot] = 0;
    }
    if (now - g_stationary_hazard_since[slot] <
        g_rf_config.blocking_confirm_ms)
        return 0;
    /* A healthy recoverable active stall remains passive. A damaged active
     * car is exceptional only after the strict speed, exact-line and dwell
     * gates above have all held continuously. */
    BOOL inverted_confirmed = active_inverted_blocker &&
        g_inverted_hazard_since[slot] != 0 &&
        now - g_inverted_hazard_since[slot] >=
            g_rf_config.inverted_confirm_ms;
    if (!terminal_blocker && !active_damaged_blocker && !inverted_confirmed) {
        if (passive_road_hazard_out) *passive_road_hazard_out = 1;
        return 0;
    }
    if (active_qualifying_out)
        *active_qualifying_out = inverted_confirmed ? 2 :
            (active_damaged_blocker ? 1 : 0);

    /* Preserve the established positive-proof counter for HUD/shared status.
     * Geometry remains authoritative when a destroyed car has no wheel hits. */
    int contacts = drivable_surface_contacts(wreck);
    return contacts > 0 ? contacts : 1;
}

static __attribute__((noinline, optimize("O0"))) BOOL incident_counts_qualify(int terminals, int stationary_damaged,
                                    int weather_assist) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("incident_counts_qualify");

    if (g_rf_config.enable_multi_terminal &&
        terminals >= g_rf_config.multi_terminal_cars) { rf_protected_result = (TRUE); goto rf_protected_exit; }
    if (g_rf_config.enable_mixed_blockage &&
        terminals >= g_rf_config.mixed_terminal_cars &&
        stationary_damaged >=
            g_rf_config.mixed_stationary_damaged_cars) { rf_protected_result = (TRUE); goto rf_protected_exit; }
    if (g_rf_config.enable_strong_wet_pattern && weather_assist >= 2 &&
        terminals >= g_rf_config.strong_wet_terminal_cars &&
        stationary_damaged >=
            g_rf_config.strong_wet_stationary_damaged_cars) { rf_protected_result = (TRUE); goto rf_protected_exit; }
    { rf_protected_result = (FALSE); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

/* Crash-terminal scan for the monitor. Same counting rule as the detector:
 * veh_crash_terminal sustained for AUTO_TERMINAL_CONFIRM_MS within the
 * AUTO_TERM_WINDOW_MS rolling window. Runs unconditionally
 * from status_gui_update so "Wrecked AI" on screen is always live, even when
 * the auto-fire gate is closed.
 *
 * LOCALITY (2026-08-17 fix): three terminals anywhere on the circuit are not
 * automatically a red flag - they must be one incident. A recently-terminal
 * car counts only if AUTO_TERM_THRESHOLD cars sit within AUTO_CLUSTER_M of
 * one another (nearest-neighbour clique). Scattered retirements kilometres
 * apart never form a cluster.
 *
 * PAUSE (2026-08-17 fix): frozen while the game is paused, matching the
 * yellow gate - the 4 s sustain and the 5 s window are race time, not wall
 * time. kill_duration() independently refuses to start/advance its own clock
 * while paused, so a car paused at 3.9 s cannot cross 4 s on resume.
 *
 * FIELD-REBUILD GUARD: every walk of the vehicle array can race the game
 * rebuilding the field (restart, session reload) and deref a freed struct -
 * the crash class that took down the DLL twice on 2026-08-17 (fault in
 * vehicle_looks_live, stale pointer). The epoch is sampled before and after;
 * if it moved, the results are discarded (the game was mid-rebuild). The
 * phase 2/3 gate (restart/gridwalk) further skips the window where the churn
 * is guaranteed. */
static void refresh_terminal_state(DWORD now) {
    hazard_clear_observations();
    g_impassable_cluster=0;
    if (PAUSE_FLAG) return;
    if (g_monitor_phase >= 2 && g_monitor_phase <= 3) {
        g_monitor_terminals = 0;
        g_monitor_terminal_now = 0;
        g_monitor_incident_score = 0;
        g_monitor_incident_cars = 0;
        g_monitor_incident_terminals = 0;
        g_monitor_incident_damaged_stationary = 0;
        g_monitor_incident_ontrack_terminals = 0;
        g_monitor_blocking_road_contacts = 0;
        g_monitor_inverted_blocker = 0;
        g_monitor_passive_road_hazard = 0;
        g_monitor_incident_lapdist = -1.0f;
        g_aiw_blocking_valid = 0;
        g_aiw_blocking_nearest_index = -1;
        g_aiw_blocking_path_m = 0.0f;
        g_aiw_blocking_line_m = 0.0f;
        g_aiw_blocking_band = 0;
        g_aiw_blocking_corridor = -1;
        return;
    }
    uintptr_t epoch0 = session_epoch();
    uintptr_t player = PLAYER_VEH_PTR;
    int count = VEHICLE_COUNT;
    int now_count = 0;
    int incident_count = 0;
    int best_passive_road_hazard = 0;
    int best_blocking_road_contacts = 0;
    int best_blocking_aiw_valid = 0;
    int best_blocking_aiw_index = -1;
    float best_blocking_aiw_path_m = 0.0f;
    float best_blocking_aiw_line_m = 0.0f;
    int best_blocking_aiw_band = 0;
    int best_blocking_aiw_corridor = -1;
    int best_blocking_active_qualifying = 0;
    float best_blocking_lapdist = -1.0f;
    AIWDistance geometry[104];
    char* incident_vehicles[104]={0};
    int* points = g_incident_points_scratch;
    BOOL* terminal = g_incident_terminal_scratch;
    BOOL* damaged_stationary = g_incident_damaged_scratch;
    BOOL* ontrack = g_incident_ontrack_scratch;
    float* cx = g_incident_cx_scratch;
    float* cy = g_incident_cy_scratch;
    float* cz = g_incident_cz_scratch;
    float* lapdist = g_incident_lapdist_scratch;
    BOOL opening = g_settled_since != 0 &&
        now - g_settled_since <= g_rf_config.opening_window_ms;
    float radius = opening ? g_rf_config.opening_cluster_m
                           : g_rf_config.cluster_m;

    for (int i = 0; i < count; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh || (uintptr_t)veh == player || (g_rc && (LONG)(uintptr_t)veh==f1c_rc_read(&g_rc->sc_vehicle)) || !vehicle_looks_live(veh)) {
            g_incident_since[i] = 0;
            g_stationary_hazard_since[i] = 0;
            g_inverted_hazard_since[i] = 0;
            g_term_times[i] = 0;
            continue;
        }
        if (veh_crash_terminal(veh)) {
            now_count++;
        }
        /* Share spatial eligibility even when RF's stronger blockage gate fails. */
        if(isfinite(*(float*)(veh+OFF_SPEED)) && *(float*)(veh+OFF_SPEED)<8.0f &&
           *(int*)(veh+OFF_PITSTATE)==0){
            AIWDistance m=aiw_branch_path_distance(veh,AIW_MAIN_BRANCH,aiw_live_wetness(ON_PATH_WETNESS));
            int known=m.valid && m.road_left_m>0 && m.road_right_m>0;
            int near_road=known && m.lateral_m>=-m.road_left_m-AIW_CAR_HALF_WIDTH_M-2 &&
                m.lateral_m<=m.road_right_m+AIW_CAR_HALF_WIDTH_M+2;
            int blocked=near_road && m.on_road_geom && m.best_pass_m<AIW_PASS_CRITICAL_M &&
                m.closest_line_m<=AIW_CAR_HALF_WIDTH_M+g_rf_config.racing_line_overlap_margin_m;
            f1c_geometry_publish(i,(uintptr_t)veh,known,near_road,blocked);
        }else f1c_geometry_publish(i,(uintptr_t)veh,0,0,0);
        float road_aiw_path_m = 0.0f;
        float road_aiw_line_m = 0.0f;
        int road_aiw_band = 0;
        int road_aiw_index = -1;
        int road_aiw_valid = 0;
        int road_aiw_corridor = -1;
        int road_active_qualifying = 0;
        int road_passive_hazard = 0;
        int road_contacts = blocking_lane_road_contacts(
            veh, i, now, &road_active_qualifying, &road_passive_hazard,
            &road_aiw_path_m,
            &road_aiw_line_m, &road_aiw_band, &road_aiw_index,
            &road_aiw_valid, &road_aiw_corridor);
        F1CIncidentGeometry shared_geometry=f1c_geometry_read(i,(uintptr_t)veh);
        f1c_decision_publish(1,i,(uintptr_t)veh,!veh_is_confirmably_inverted(veh)?1:
            *(int*)(veh+OFF_PITSTATE)!=0?2:!shared_geometry.known?3:
            !shared_geometry.near_road?4:road_contacts>0?6:5);
        if(g_rf_config.enable_blocked_corridor && road_contacts>0 &&
           road_aiw_valid && road_aiw_corridor==AIW_MAIN_BRANCH &&
           road_aiw_line_m<=AIW_CAR_HALF_WIDTH_M+g_rf_config.racing_line_overlap_margin_m)
            hazard_observe(veh,1u | (veh_is_confirmably_inverted(veh)?2u:0u) |
                (veh_crash_terminal(veh)?4u:0u));
        if (road_passive_hazard) best_passive_road_hazard = 1;
        if (road_contacts > best_blocking_road_contacts ||
            (road_contacts == best_blocking_road_contacts && road_contacts > 0 &&
             road_aiw_valid && (!best_blocking_aiw_valid ||
              road_aiw_path_m < best_blocking_aiw_path_m))) {
            best_blocking_road_contacts = road_contacts;
            best_blocking_aiw_valid = road_aiw_valid;
            best_blocking_aiw_index = road_aiw_index;
            best_blocking_aiw_path_m = road_aiw_path_m;
            best_blocking_aiw_line_m = road_aiw_line_m;
            best_blocking_aiw_band = road_aiw_band;
            best_blocking_aiw_corridor = road_aiw_corridor;
            best_blocking_active_qualifying = road_active_qualifying;
            best_blocking_lapdist = *(volatile float*)(veh + OFF_LAPDIST);
        }
        BOOL is_terminal = FALSE;
        BOOL is_damaged_stationary = FALSE;
        int score = incident_points(veh, i, now, opening, &is_terminal,
                                    &is_damaged_stationary);
        if(!is_terminal && !veh_is_confirmably_inverted(veh) &&
           f1c_rc_controlled(g_rc,i,(uintptr_t)veh)) {
            score=0;is_damaged_stationary=FALSE;
            g_incident_since[i]=0;
        }
        if (score > 0 && incident_count < g_session_capacity) {
            AIWDistance incident_metric = aiw_branch_path_distance(
                veh, AIW_MAIN_BRANCH, aiw_live_wetness(ON_PATH_WETNESS));
            if(incident_count<104){geometry[incident_count]=incident_metric;
                incident_vehicles[incident_count]=veh;}
            points[incident_count] = score;
            terminal[incident_count] = is_terminal;
            damaged_stationary[incident_count] = is_damaged_stationary;
            ontrack[incident_count] = is_terminal && incident_metric.valid &&
                incident_metric.on_road_geom;
            cx[incident_count] = *(volatile float*)(veh + OFF_POSX);
            cy[incident_count] = *(volatile float*)(veh + OFF_POSY);
            cz[incident_count] = *(volatile float*)(veh + OFF_POSZ);
            lapdist[incident_count] = *(volatile float*)(veh + OFF_LAPDIST);
            incident_count++;
        }
    }
    if (session_epoch() != epoch0) {
        hazard_clear_observations();
        incident_count = 0;
        now_count = 0;
        best_passive_road_hazard = 0;
        best_blocking_road_contacts = 0;
        best_blocking_aiw_valid = 0;
        best_blocking_aiw_index = -1;
        best_blocking_aiw_path_m = 0.0f;
        best_blocking_aiw_line_m = 0.0f;
        best_blocking_aiw_band = 0;
        best_blocking_aiw_corridor = -1;
        best_blocking_active_qualifying = 0;
        best_blocking_lapdist = -1.0f;
    }

    if(incident_count<=104)for(int a=0;a<incident_count;a++) {
        if(!ontrack[a] || !geometry[a].valid)continue;
        float left[104],right[104];int members[104];int n=0;
        for(int b=0;b<incident_count;b++) {
            if(!ontrack[b] || !geometry[b].valid ||
               !rf_same_cross_section(lapdist[a],lapdist[b],
                 *(volatile float*)(SCENE_STRUCT+TRACK_LAP_LENGTH_OFF),
                 cx[a]-cx[b],cy[a]-cy[b],cz[a]-cz[b]))continue;
            /* Project each centre into the anchor's road cross-section. Native
               segment labels may differ at a perfectly continuous road seam. */
            float lateral=geometry[a].lateral_m+(cx[b]-cx[a])*geometry[a].normal_x+
                          (cz[b]-cz[a])*geometry[a].normal_z;
            left[n]=lateral-AIW_CAR_HALF_WIDTH_M;
            right[n]=lateral+AIW_CAR_HALF_WIDTH_M;members[n]=b;n++;
        }
        if(n>=g_rf_config.multi_terminal_cars &&
           rf_passage_blocked(geometry[a].road_left_m,geometry[a].road_right_m,left,right,n))
        {
            g_impassable_cluster=1;
            if(g_rf_config.enable_multi_terminal)
                for(int m=0;m<n;m++)hazard_observe(incident_vehicles[members[m]],8u);
        }
    }

    /* Find the highest-scoring local incident around any contributing car.
     * A qualifying cluster must contain at least two cars and one terminal;
     * this prevents one destroyed car or several unrelated circuit-wide
     * retirements from becoming a red flag. */
    int best_score = 0;
    int best_cars = 0;
    int best_terminals = 0;
    int best_damaged_stationary = 0;
    int best_ontrack_terminals = 0;
    float best_lapdist = -1.0f;
    int weather_assist = weather_incident_assist_level();
    for (int a = 0; a < incident_count; a++) {
        int score = 0;
        int cars = 0;
        int terminals = 0;
        int stationary_damaged = 0;
        int ontrack_score = 0;
        int ontrack_cars = 0;
        int ontrack_terminals = 0;
        for (int b = 0; b < incident_count; b++) {
            float dx = cx[a] - cx[b];
            float dy = cy[a] - cy[b];
            float dz = cz[a] - cz[b];
            float distance_sq = dx*dx + dy*dy + dz*dz;
            if (distance_sq <= radius * radius) {
                score += points[b];
                cars++;
                if (terminal[b]) terminals++;
                if (damaged_stationary[b]) stationary_damaged++;
            }
            /* Exceptional second proof: multiple terminal cars physically
             * close together on branch-0 road geometry. This catches the live
             * Panis/de la Rosa false negative without treating grass pileups,
             * unrelated circuit-wide DNFs, or ordinary stalls as Red Flags. */
            if (ontrack[b] && distance_sq <=
                    g_rf_config.ontrack_pileup_cluster_m *
                    g_rf_config.ontrack_pileup_cluster_m) {
                ontrack_score += points[b];
                ontrack_cars++;
                ontrack_terminals++;
            }
        }
        BOOL ontrack_qualifies = g_rf_config.enable_multi_terminal &&
            ontrack_terminals >= g_rf_config.multi_terminal_cars;
        BOOL best_ontrack_qualifies = g_rf_config.enable_multi_terminal &&
            best_ontrack_terminals >= g_rf_config.multi_terminal_cars;
        BOOL qualifies = incident_counts_qualify(
            terminals, stationary_damaged, weather_assist);
        BOOL best_qualifies = incident_counts_qualify(
            best_terminals, best_damaged_stationary, weather_assist);
        if (ontrack_qualifies && (!best_ontrack_qualifies ||
                                 ontrack_score > best_score)) {
            best_score = ontrack_score;
            best_cars = ontrack_cars;
            best_terminals = ontrack_terminals;
            best_damaged_stationary = 0;
            best_ontrack_terminals = ontrack_terminals;
            best_lapdist = lapdist[a];
        } else if (!best_ontrack_qualifies &&
                   ((qualifies && !best_qualifies) ||
                    (qualifies == best_qualifies && score > best_score))) {
            best_score = score;
            best_cars = cars;
            best_terminals = terminals;
            best_damaged_stationary = stationary_damaged;
            best_ontrack_terminals = 0;
            best_lapdist = lapdist[a];
        }
    }
    if (best_cars < g_rf_config.minimum_cluster_cars ||
        best_terminals < g_rf_config.minimum_cluster_terminals) {
        best_score = 0;
        best_cars = 0;
        best_terminals = 0;
        best_damaged_stationary = 0;
        best_ontrack_terminals = 0;
        best_lapdist = -1.0f;
    }
    /* A spatially proven damaged active obstruction is deliberately a
     * one-car blocked-corridor pattern, not a fake terminal cluster. Preserve
     * truthful HUD/log counts while the separate blockage proof qualifies it. */
    if (best_blocking_road_contacts > 0 && best_blocking_active_qualifying &&
        best_cars == 0) {
        best_score = 3;
        best_cars = 1;
        best_damaged_stationary = 1;
        best_lapdist = best_blocking_lapdist;
    }

    g_monitor_terminals = best_terminals;
    g_monitor_terminal_now = now_count;
    g_monitor_incident_score = best_score;
    g_monitor_incident_cars = best_cars;
    g_monitor_incident_terminals = best_terminals;
    g_monitor_incident_damaged_stationary = best_damaged_stationary;
    g_monitor_incident_ontrack_terminals = best_ontrack_terminals;
    g_monitor_blocking_road_contacts = best_blocking_road_contacts;
    g_monitor_inverted_blocker = best_blocking_active_qualifying == 2;
    g_monitor_passive_road_hazard = best_passive_road_hazard;
    g_monitor_incident_lapdist = best_blocking_road_contacts > 0
        ? best_blocking_lapdist : best_lapdist;
    g_aiw_blocking_valid = best_blocking_aiw_valid;
    g_aiw_blocking_nearest_index = best_blocking_aiw_index;
    g_aiw_blocking_path_m = best_blocking_aiw_path_m;
    g_aiw_blocking_line_m = best_blocking_aiw_line_m;
    g_aiw_blocking_band = best_blocking_aiw_band;
    g_aiw_blocking_corridor = best_blocking_aiw_corridor;
}

static BOOL native_yellow_persisted(DWORD now) {
    refresh_yellow_state(now);
    return g_monitor_yellow_ms >= AUTO_YELLOW_CONFIRM_MS;
}

/* Weather is an incident amplifier, never an independent red-flag cause.
 * All tiers below still require a confirmed terminal/blocked crash pattern.
 * Ordinary front-wing damage, a moving damaged car and bad weather alone
 * therefore remain passive/no-Red-Flag territory. */
static int weather_incident_assist_level(void) {
    float wetness = aiw_live_wetness(ON_PATH_WETNESS);
    if (wetness >= g_rf_config.strong_wet_threshold) return 2;
    if (wetness >= g_rf_config.wet_assist_threshold) return 1;
    return 0;
}

/* Multi-car/locality/weather rules describe a significant sub-Red-Flag
 * incident. They produce only an optional log placeholder and never authorize
 * this plug-in to stop the race or operate a nonexistent Safety Car. */
static BOOL passive_placeholder_qualifies(void) {
    return g_monitor_passive_road_hazard ||
           incident_counts_qualify(g_monitor_incident_terminals,
                                   g_monitor_incident_damaged_stationary,
                                   weather_incident_assist_level());
}

/* Automatic RED FLAG authorization remains fail-closed, with two exceptional
 * spatial proofs: a sustained direct racing-line obstruction, or the configured
 * number of terminal wrecks close together on branch-0 road geometry. General
 * damage counts, rain, grass pileups and unrelated circuit-wide DNFs remain
 * passive and cannot stop the race. */
static __attribute__((noinline, optimize("O0"))) BOOL incident_pattern_qualifies(void) {
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("incident_pattern_qualifies");

    BOOL direct_line = g_rf_config.enable_blocked_corridor &&
        g_monitor_blocking_road_contacts > 0 &&
        g_aiw_blocking_valid &&
        g_aiw_blocking_corridor == AIW_MAIN_BRANCH &&
        g_aiw_blocking_line_m <= AIW_CAR_HALF_WIDTH_M +
            g_rf_config.racing_line_overlap_margin_m;
    BOOL ontrack_pileup = g_rf_config.enable_multi_terminal && g_impassable_cluster &&
        g_monitor_incident_ontrack_terminals >=
            g_rf_config.multi_terminal_cars;
    { rf_protected_result = (direct_line || ontrack_pileup); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

/* 0x007D0798 is F1C's live race-distance scale (1.0 = 100%). Automatic Red
 * Flag is unavailable below the configured percentage. Unreadable/corrupt
 * values fail closed; 0 explicitly disables only this distance gate. */
static BOOL race_distance_eligible(void) {
    float scale = RACE_DISTANCE_SCALE;
    int minimum = g_rf_config.minimum_race_distance_percent;
    int current;
    if (minimum <= 0) {
        g_race_distance_gate_logged = FALSE;
        return TRUE;
    }
    if (!isfinite(scale) || scale <= 0.0f || scale > 1.001f) {
        if (!g_race_distance_gate_logged) {
            log_w("RF-E6CFCDCBFE3");
            g_race_distance_gate_logged = TRUE;
        }
        return FALSE;
    }
    current = (int)(scale * 100.0f + 0.5f);
    if (current < minimum) {
        if (!g_race_distance_gate_logged) {
            char buf[160];
            wsprintfA(buf,
                "RF-E12676A4F1C %d %% %d",
                current, minimum);
            log_w(buf);
            g_race_distance_gate_logged = TRUE;
        }
        return FALSE;
    }
    g_race_distance_gate_logged = FALSE;
    return TRUE;
}

/* A notice is evidence, not irrevocable authority. Re-read every mutable gate
 * at the actual acceptance/stop edge so a cleared yellow, recovered/moved car,
 * session transition, or newly active cooldown cannot turn a stale warning
 * into a race stop. */
static BOOL auto_notice_live_gates_valid(DWORD now) {
    BOOL opening;
    DWORD yellow_need;
    refresh_yellow_state(now);
    refresh_terminal_state(now);
    opening = g_settled_since != 0 &&
        now - g_settled_since <= g_rf_config.opening_window_ms;
    yellow_need = opening ? g_rf_config.opening_yellow_ms
                          : g_rf_config.yellow_ms;
    if (!opening && weather_incident_assist_level() >= 1 &&
        yellow_need > g_rf_config.wet_yellow_ms)
        yellow_need = g_rf_config.wet_yellow_ms;
    return race_is_settled() && race_distance_eligible() &&
        g_monitor_cooldown_ms == 0 &&
        (!g_rf_config.require_native_yellow ||
         g_monitor_yellow_ms >= yellow_need) &&
        incident_pattern_qualifies();
}

static int incident_pattern_reason(void) {
    if (g_monitor_inverted_blocker &&
        g_monitor_blocking_road_contacts > 0) return 6;
    if (g_rf_config.enable_blocked_corridor &&
        g_monitor_blocking_road_contacts > 0) return 4;
    if (g_rf_config.enable_multi_terminal &&
        g_monitor_incident_terminals >=
            g_rf_config.multi_terminal_cars) return 1;
    if (g_rf_config.enable_mixed_blockage &&
        g_monitor_incident_terminals >= g_rf_config.mixed_terminal_cars &&
        g_monitor_incident_damaged_stationary >=
            g_rf_config.mixed_stationary_damaged_cars) return 2;
    if (g_rf_config.enable_strong_wet_pattern &&
        weather_incident_assist_level() >= 2 &&
        g_monitor_incident_terminals >=
            g_rf_config.strong_wet_terminal_cars &&
        g_monitor_incident_damaged_stationary >=
            g_rf_config.strong_wet_stationary_damaged_cars) return 5;
    return 0;
}

static void auto_notice_reset(void) {
    hud_auto_notice_clear();
    g_auto_notice_elapsed_ms = 0;
    g_auto_notice_last_tick = 0;
    g_auto_notice_reason = 0;
    g_auto_notice_fired = FALSE;
    g_auto_notice_sector = 0;
    g_auto_notice_cars = 0;
    g_auto_notice_terminals = 0;
    g_auto_notice_stationary_damaged = 0;
    g_auto_notice_blocking_contacts = 0;
    g_auto_notice_corridor = -1;
    g_monitor_stop_reason = 0;
    g_monitor_prestop_elapsed_ms = 0;
#ifndef RF_RELEASE
    g_debug_organic_notice = FALSE;
#endif
}

#ifndef RF_RELEASE
/* Test-only initiation. It bypasses only the detector's eligibility decision;
 * from the reason-notice latch onward it uses check_auto_redflag() and the
 * exact automatic phase-1/replay/cleanup/restart/green-restoration path. */
static void debug_prime_organic_redflag(DWORD now) {
    char buf[240];
    auto_notice_reset();
    g_debug_organic_notice = TRUE;
    g_auto_notice_reason = incident_pattern_reason();
    if (g_auto_notice_reason == 0) g_auto_notice_reason = 3;
    g_auto_notice_sector = incident_sector_from_location(
        g_monitor_incident_lapdist);
    g_auto_notice_cars = g_monitor_incident_cars;
    g_auto_notice_terminals = g_monitor_incident_terminals;
    g_auto_notice_stationary_damaged =
        g_monitor_incident_damaged_stationary;
    g_auto_notice_blocking_contacts = g_monitor_blocking_road_contacts;
    g_auto_notice_corridor = g_aiw_blocking_corridor;
    g_auto_notice_elapsed_ms = 0;
    g_auto_notice_last_tick = now;
    g_monitor_stop_reason = g_auto_notice_reason;
    g_monitor_prestop_elapsed_ms = 0;
    wsprintfA(buf, "RF-EBC6622F56D %d %d %d %d",
              g_auto_notice_reason, g_auto_notice_sector,
              g_auto_notice_cars, g_auto_notice_terminals);
    log_w(buf);
}
#endif

/* Automatic red flag: combine F1C's native yellow with a fresh, local severe
 * incident pattern. The pattern must remain qualifying for a final second;
 * this filters field-rebuild edges and brief pile-up geometry overlaps. */
static BOOL check_auto_redflag(BOOL manual_accept_edge) {
    DWORD now = GetTickCount();
    g_monitor_cooldown_ms = now < g_auto_cooldown_until
        ? g_auto_cooldown_until - now : 0;

    g_monitor_game_state = GAME_STATE;
    g_monitor_race_phase = RACE_PHASE;
    g_monitor_settled    = race_is_settled();

    if (!race_distance_eligible()) {
        g_auto_candidate_since = 0;
        g_monitor_auto_candidate = FALSE;
        auto_notice_reset();
        return FALSE;
    }

    /* Terminal timestamps stay warm only while the race is genuinely running.
     * During a restart/formation/grid the same ActiveFlag=0 samples exist but
     * are not a pileup; gating here means they never even start accumulating.
     * The samples below this line never run in an unsettled race. */
    if (!g_monitor_settled) {
        g_prev_settled = FALSE;
        g_settled_since = 0;
        g_auto_candidate_since = 0;
        g_monitor_auto_candidate = FALSE;
        g_placeholder_logged = FALSE;
        auto_notice_reset();
        return FALSE;
    }

    /* The completed notice stays published throughout the stop/countdown.
     * check_auto_redflag() is not called again until phase returns to zero;
     * that first later call clears the old reason before re-arming. */
    if (g_auto_notice_fired) auto_notice_reset();

    /* Once the severe pattern passes its final confirmation, the decision is
     * latched and its reason is shown for ten seconds before phase 1 begins.
     * This clock is race-time: pausing freezes it instead of allowing the stop
     * to arrive while the player is reading the announcement. */
    if (g_auto_notice_reason != 0) {
        if(!g_rf_config.require_manual_acceptance && g_rf_config.allow_cancel && manual_accept_edge) {
            g_announcement_snapshot=FALSE;
            (void)rf_penalty_end();
            auto_notice_reset();g_auto_candidate_since=0;
            g_auto_cooldown_until=now+g_rf_config.cooldown_ms;
            log_w("RF-E4F78BBE9A2");
            return FALSE;
        }
        g_monitor_stop_reason = g_auto_notice_reason;
        hud_auto_notice_update();
        if (PAUSE_FLAG) {
            g_auto_notice_last_tick = now;
            g_monitor_prestop_elapsed_ms = g_auto_notice_elapsed_ms;
            return FALSE;
        }
        /* A pending offer is not ownership: expire stale evidence even when
         * the player never presses T. Pause above freezes the offer; active
         * sequences are not polled here. Reset also clears the shared veto. */
        if (g_rf_config.require_manual_acceptance && !auto_notice_live_gates_valid(now)) {
            log_w("RF-E9E71FCCE8F");
            auto_notice_reset();
            g_monitor_auto_candidate = FALSE;
            g_auto_candidate_since = 0;
            return FALSE;
        }
        if (g_auto_notice_last_tick == 0) g_auto_notice_last_tick = now;
        DWORD delta = now - g_auto_notice_last_tick;
        g_auto_notice_last_tick = now;
        if (delta > 1000) delta = 1000;
        DWORD notice_limit=g_rf_config.require_manual_acceptance ?
            g_rf_config.acceptance_timeout_ms : g_rf_config.prestop_notice_ms;
        if (g_auto_notice_elapsed_ms < notice_limit) {
            DWORD room = notice_limit -
                         g_auto_notice_elapsed_ms;
            g_auto_notice_elapsed_ms += delta < room ? delta : room;
        }
        g_monitor_prestop_elapsed_ms = g_auto_notice_elapsed_ms;
        if (g_rf_config.require_manual_acceptance) {
            if(g_auto_notice_elapsed_ms>=g_rf_config.acceptance_timeout_ms) {
                auto_notice_reset();g_auto_cooldown_until=now+g_rf_config.cooldown_ms;
                log_w("RF-E06D656B5E1");return FALSE;
            }
            if (!manual_accept_edge) return FALSE;
            /* T accepts only a currently valid severe incident. Refresh the
             * evidence at the key edge so a stale prompt cannot stop a race
             * after its yellow/pattern/session gates have disappeared. */
            if (!auto_notice_live_gates_valid(now)) {
                log_w("RF-EFBF0D6B0B9");
                auto_notice_reset();
                return FALSE;
            }
            log_w("RF-EA44FB71CB7");
            g_monitor_auto_candidate = TRUE;
            g_auto_notice_fired = TRUE;
            return TRUE;
        }
        if (g_auto_notice_elapsed_ms < g_rf_config.prestop_notice_ms)
            return FALSE;

        log_w("RF-E732F366F27");
        /* Cooldown starts only after coordinated ownership is granted. */
        g_monitor_auto_candidate = TRUE;
        g_auto_notice_fired = TRUE;
        return TRUE;
    }

    /* PAUSE-FROZEN: a red flag fires on race time, not wall time. The frozen
     * yellow and terminal counters would otherwise satisfy the gates from a
     * pre-pause incident the moment the loop next polls. */
    if (PAUSE_FLAG) {
        g_auto_candidate_since = 0;
        g_monitor_auto_candidate = FALSE;
        return FALSE;
    }
    /* Edge INTO a settled race: forget every kill/terminal sample collected
     * outside it. A car that died during formation or on the grid would
     * otherwise read kill_duration >= 4s on our first poll and count as a
     * fresh pileup the moment we open the gate. */
    if (!g_prev_settled) {
        g_prev_settled = TRUE;
        g_settled_since = now;
        kill_tracker_reset();
        for (int i = 0; i < g_session_capacity; i++) {
            g_term_times[i] = 0;
            g_incident_since[i] = 0;
            g_stationary_hazard_since[i] = 0;
            g_inverted_hazard_since[i] = 0;
        }
        g_auto_candidate_since = 0;
        g_placeholder_logged = FALSE;
        log_w("RF-EBFF7FC4518");
    }

    /* Keep the terminal timestamps warm even when there is no yellow. This
     * prevents pre-existing retirements from looking like a new pile-up when
     * a yellow elsewhere on the circuit comes up later. */
    refresh_yellow_state(now);
    refresh_terminal_state(now);
    BOOL red_flag_proven = incident_pattern_qualifies();
    BOOL passive_placeholder = passive_placeholder_qualifies();
    if (g_rf_config.log_passive_placeholder && passive_placeholder &&
        !red_flag_proven && !g_placeholder_logged) {
        char placeholder_buf[320];
        wsprintfA(placeholder_buf, "RF-E03CCF348FA %d %d %d %d",
                  g_monitor_incident_terminals,
                  g_monitor_incident_damaged_stationary,
                  g_monitor_incident_cars,
                  g_monitor_passive_road_hazard);
        log_w(placeholder_buf);
        g_placeholder_logged = TRUE;
    } else if (!passive_placeholder || red_flag_proven) {
        g_placeholder_logged = FALSE;
    }
    BOOL opening = g_settled_since != 0 &&
        now - g_settled_since <= g_rf_config.opening_window_ms;
    int weather_assist = weather_incident_assist_level();
    DWORD yellow_need = opening ? g_rf_config.opening_yellow_ms
                                : g_rf_config.yellow_ms;
    DWORD final_confirm_need = g_rf_config.final_confirm_ms;
    /* Wetness shortens confirmation only after a real incident has already
     * met the gate below. It never independently starts a candidate. */
    if (!opening && weather_assist >= 1 &&
        yellow_need > g_rf_config.wet_yellow_ms)
        yellow_need = g_rf_config.wet_yellow_ms;
    if (weather_assist >= 1)
        final_confirm_need = g_rf_config.wet_final_confirm_ms;
    if (weather_assist >= 2)
        final_confirm_need = g_rf_config.strong_wet_final_confirm_ms;

    if ((g_monitor_cooldown_ms != 0 && !hazard_is_fresh()) ||
        (g_rf_config.require_native_yellow &&
         g_monitor_yellow_ms < yellow_need) ||
        !red_flag_proven) {
        g_auto_candidate_since = 0;
        g_monitor_auto_candidate = FALSE;
        return FALSE;
    }

    uintptr_t candidate_signature=hazard_signature();
    if (g_auto_candidate_since == 0 || candidate_signature!=g_auto_candidate_signature) {
        g_auto_candidate_signature=candidate_signature;
        g_auto_candidate_since = now;
        g_monitor_auto_candidate = FALSE;
        return FALSE;
    }
    if (now - g_auto_candidate_since < final_confirm_need) {
        g_monitor_auto_candidate = FALSE;
        return FALSE;
    }

    char buf[240];
    wsprintfA(buf, "RF-E67FC5EC2C3 %d %d %d %d %d",
              g_monitor_incident_terminals,
              g_monitor_incident_ontrack_terminals,
              g_monitor_incident_damaged_stationary,
              g_monitor_incident_cars,
              g_monitor_blocking_road_contacts);
    log_w(buf);
    /* The ordinary spatial/yellow/dwell gates above still apply. Only this
     * fresh, fully confirmed hazard may retire a cooldown early. */
    if(g_monitor_cooldown_ms && hazard_is_fresh()) {
        g_auto_cooldown_until=0;g_monitor_cooldown_ms=0;
        log_w("RF-E9237F09E29");
    }
    hazard_mark_handled();
    g_auto_notice_reason = incident_pattern_reason();
    f1c_rc_request(g_rc,1);
    if(!g_rf_config.require_manual_acceptance && !g_announcement_snapshot){snapshot_positions();g_announcement_snapshot=TRUE;}
    g_auto_notice_sector = incident_sector_from_location(
        g_monitor_incident_lapdist);
    g_auto_notice_cars = g_monitor_incident_cars;
    g_auto_notice_terminals = g_monitor_incident_terminals;
    g_auto_notice_stationary_damaged =
        g_monitor_incident_damaged_stationary;
    g_auto_notice_blocking_contacts = g_monitor_blocking_road_contacts;
    g_auto_notice_corridor = g_aiw_blocking_corridor;
    g_auto_notice_elapsed_ms = 0;
    g_auto_notice_last_tick = now;
    g_monitor_stop_reason = g_auto_notice_reason;
    g_monitor_prestop_elapsed_ms = 0;
    g_auto_candidate_since = 0;
    g_placeholder_logged = FALSE;
    g_monitor_auto_candidate = FALSE;
    wsprintfA(buf, "RF-E9E48BF5CD5 %d %d %d %d %d %d %d",
              g_auto_notice_reason, g_auto_notice_sector,
              g_auto_notice_corridor, g_auto_notice_cars,
              g_auto_notice_terminals,
              g_auto_notice_stationary_damaged,
              g_auto_notice_blocking_contacts);
    log_w(buf);
    /* Every automatic reason, including a blocked driving corridor, follows
     * the documented ten-second visible warning before the race is stopped. */
    return FALSE;
}

/* ======================================================================== *
 *  MULTIPLAYER LOCKOUT
 *
 *  Tester report (Tester J, Tester J, 2026-07-27): in multiplayer every
 *  client runs its own copy of this DLL and NOTHING is synchronised, so any
 *  player pressing NUMPAD1 restarts only his own session. Tester J: "1 second it
 *  is race, other second it's restart timer. Game is undecided between who
 *  take the Red flag." There is no fix for that short of a network protocol,
 *  so the mod refuses to arm at all in a network session.
 *
 *  THE DETECTOR IS DERIVED, NOT VERIFIED, AND IS DELIBERATELY FAIL-OPEN.
 *  MultiplayerVehiclePlacement_0042cf10 takes its multiplayer branch (and
 *  logs "Multiplayer mode: setting vehicle places for RACE") under
 *  SessionStateCheckWrapper(&dword_7CD25C), which is
 *  SessionStateCheck != 0, and SessionStateCheck reads
 *  *(*(GameModuleManager) + 16324). dword_7CD25C is named GameModuleManager
 *  at DERIVED tier in name_index.json.
 *
 *  THE OFFLINE VALUE IS CONFIRMED 0, read live 2026-08-02 in a running
 *  single-player process (pid 11568): *(0x7CD25C) = 0x04630048, and
 *  *(0x04630048 + 16324) = 0. So the offline half is measured, not assumed,
 *  and the only unverified half is whether a network session makes it
 *  nonzero. That is the safe way round: if it does not, the lockout simply
 *  never fires and the mod behaves exactly as it does today.
 *
 *  An unreadable pointer is not proof of an offline session. Red Flag mutates
 *  local race state and is therefore fail-closed: only the measured offline
 *  value 0 permits a trigger. Unknown or nonzero states are refused.
 * ======================================================================== */
#define GAME_MODULE_MGR   0x007CD25Cu
#define OFF_SESSION_STATE 16324      /* 0x3FC4 */

/* Raw state, or -1 if it cannot be read. */
static int mp_state(void) {
    uintptr_t inner = *(volatile uintptr_t*)GAME_MODULE_MGR;
    if (!inner || IsBadReadPtr((void*)inner, OFF_SESSION_STATE + 4)) return -1;
    return *(volatile int*)(inner + OFF_SESSION_STATE);
}

static BOOL is_multiplayer(void) {
    int s = mp_state();
    return (s != 0);            /* only verified state 0 is offline */
}

/* Returns TRUE and logs if the key must be refused. */
static BOOL mp_blocks(const char* what) {
    int s = mp_state();
    if (s == 0) return FALSE;
    char buf[160];
    wsprintfA(buf, "RF-EA390B4A8FC %s"
                   "(GameModuleManager state=%d). "
                   "This mod is single-player only - it restarts one client's "
                   "session and nothing is synchronised.", what, s);
    log_w(buf);
    return TRUE;
}

/* ======================================================================== *
 *  SESSION WATCHDOG
 *
 *  Tester report (Tester J, 2026-07-27): "If I press ESC when red flag is on,
 *  the next races I do from the start there is red flag ... Also weirdly 2
 *  times Numpad2 didn't work."
 *
 *  ONE ROOT CAUSE FOR BOTH. The worker's `phase` was never reset when the
 *  session ended. Leaving a race mid-sequence therefore left it at 1/2/3/10
 *  forever, and:
 *    - refresh_red_flag_icon() kept re-arming veh+0x4108 every tick, so every
 *      later race showed the flag;
 *    - NUMPAD2 is gated on phase == 0, so it silently did nothing - the
 *      "didn't work twice";
 *    - worst of the three, a stranded phase 1 kept counting and would fire
 *      RESTART_FLAG inside an unrelated session.
 *
 *  Same shape as the four full_safety_car crashes: race-only state written
 *  through a teardown. So the abort has two flavours, and which one runs is
 *  decided by whether the FIELD still exists, never by the phase:
 *
 *    WARM - the race is still live: give the cars back properly.
 *    COLD - the session is gone or has been rebuilt: touch NO vehicle struct.
 *           The loader has already rebuilt them, and the scene node table
 *           with them, so a "restore" would write through freed or reused
 *           memory. Our bookkeeping is dropped instead.
 * ======================================================================== */

/* Validate the live pointer array without depending on session-owned storage.
 * No product car limit is imposed: addressability and allocation are the only
 * bounds. Every pointer is checked before it can become session identity. */
static BOOL session_field_ready(int* count_out) {
    int count;
    SIZE_T bytes;
    if (SESSION_TYPE != 7) return FALSE;
    count = VEHICLE_COUNT;
    if (count < 2 || (SIZE_T)count > ((SIZE_T)-1) / sizeof(void*)) return FALSE;
    bytes = (SIZE_T)count * sizeof(void*);
    if (!runtime_region_supported((uintptr_t)VEHICLE_ARRAY, bytes, FALSE))
        return FALSE;
    for (int i = 0; i < count; ++i) {
        uintptr_t veh = (uintptr_t)VEHICLE_ARRAY[i];
        if (!veh || !runtime_region_supported(veh, VEH_STRIDE, FALSE))
            return FALSE;
    }
    if (count_out) *count_out = count;
    return TRUE;
}

/* Hash every vehicle pointer. A changed first car alone is not a sufficient
 * session boundary; any rebuilt/reordered field invalidates held pointers. */
static uintptr_t session_epoch(void) {
    int count;
    uintptr_t hash = (uintptr_t)2166136261u;
    if (!session_field_ready(&count)) return 0;
    int race_count=0;
    uintptr_t sc=g_rc?(uintptr_t)(uint32_t)f1c_rc_read(&g_rc->sc_vehicle):0;
    for (int i = 0; i < count; ++i) {
        uintptr_t vehicle=(uintptr_t)VEHICLE_ARRAY[i];
        if(sc && vehicle==sc)continue;
        hash ^= vehicle;hash *= (uintptr_t)16777619u;race_count++;
    }
    hash ^= (uintptr_t)race_count;
    return hash ? hash : 1u;
}

static BOOL field_size_supported(void) {
    int count = VEHICLE_COUNT;
    return count >= 2 && g_session_active && g_session_capacity >= count;
}

static BOOL session_is_live_race(void) {
    int count;
    return g_session_active && session_field_ready(&count) &&
           count <= g_session_capacity;
}

static BOOL restart_abort_warm(void) {
    uintptr_t epoch;
    if (!session_is_live_race() || !race_is_settled() || PLAYER_OUT_FLAG)
        return FALSE;
    epoch=session_epoch();
    return epoch && (epoch == g_session_epoch_id ||
        (g_roster_exclusion_active && epoch == g_roster_reduced_epoch));
}

/* Drop the sunk-car bookkeeping WITHOUT writing the restore. Used by the cold
 * path only: those Y values belong to structs the loader has already
 * rebuilt. */
static void hidden_forget(void) {
    if (!g_hidden_count) return;
    char buf[96];
    wsprintfA(buf, "RF-E50AD6A118F %d"
                   "restoring Y (their structs are stale).", g_hidden_count);
    log_w(buf);
    g_hidden_count = 0;
    g_hidden_epoch = 0;
}

/* EVERY EFFECT THIS DLL HAS OUTSIDE ITS OWN MODULE, REVERSED.
 *
 * The list is the whole point of the hot-swap: anything missed here survives
 * the unload, and the next injected build then races a leftover. Order
 * matters only for the last two - the call sites must be restored before the
 * wait, and the wait must happen before the module is freed.
 *
 * Every step is idempotent and guarded, so calling this twice (worker, then
 * DLL_PROCESS_DETACH) is safe. */
static void eject_teardown(void) {
    /* 1.1.6: stop the crash VEH and write the usage summary on the worker
     * before the broker is unbound. This is the host-unload flush; DllMain
     * never flushes (loader lock / dead threads). */
    rfu_veh_remove();
    rfu_flush("unload");
#if RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR
    /* The EXE operand must stop pointing into this module before broker drain
     * and before any legacy FreeLibrary path. A failed verified removal makes
     * the module deliberately stay resident. */
    if (!hud_position_patch_sync(2)) {
        g_hud_patch_unload_safe = FALSE;
        log_w("RF-E77397ED40C");
    } else {
        log_w("RF-EAF131032EF");
    }
#endif
    /* Stop and drain broker callbacks before any worker-thread roster restore
     * can touch the same vehicle pointers. */
    rf_broker_unbind();
    /* A deliberate unload while the exact race is still owned must not leave
     * its reduced VehicleCount behind. Failure is logged and no stale pointer
     * is touched after the session boundary. */
    (void)restore_excluded_restart_roster();

    /* 1. hand every governed car back to the game */
    restore_speed_cap();
    g_pos_lock_active = FALSE;

    /* 2. un-retire anything WE armed for the pits; the game would otherwise
     *    carry on retiring cars for a red flag that no longer exists */
    unretire_pitret();

    /* 3. put hidden DNF cars back where the game left them. Their Y was sunk
     *    to -1000 by us; nothing else will ever restore it. */
    unhide_all();

    /* 4. game state we borrowed */
    vehicle_removal_restore();
    camera_action_restore();
    commentary_restore();
    hide_red_flag_icon();

    /* 5. the two call-site trampolines. AFTER this the game can no longer
     *    enter this module through them. */
    remove_hook();
    remove_hook2();
    remove_player_grid_gate();
    remove_grid_camera_hook();

        /* 6. restore the blue flag immediately if a sequence was active */
    unpatch_red_flag_sprite();

    /* 7. our own monitor window. Destroying it from the creating thread
     *    unregisters its per-instance class with the module, so the next
     *    injected build never creates against a stale window proc. */
#if IN_GAME_MONITOR_ENABLED
    destroy_status_gui();
#endif

    /* 8. the shared-memory monitor bridge (external viewers go stale) */
    mon_shared_close();

    /* 7. let any call already INSIDE a trampoline finish before the module
     *    goes away. The hooks run on the render/session tick, so a couple of
     *    frames is ample; this only costs time on an eject. */
    Sleep(300);
}

/* Tear down a red-flag sequence that will not finish. The caller sets its own
 * phase back to 0. See the SESSION WATCHDOG block for why `warm` exists. */
static void abort_sequence(BOOL warm, const char* why) {
    (void)rf_penalty_end();
    replay_prepare_reset();
    char buf[160];
    wsprintfA(buf, "RF-E119CD86208 %s %s", warm ? "warm" : "cold", why);
    log_w(buf);
    InterlockedExchange(&g_restart_cancelled, 1);
    /* Only clear a restart byte still explicitly owned by this module. */
    if (InterlockedCompareExchange(&g_owned_restart_gate_pending, 0, 1) == 1)
        *RESTART_FLAG = 0;
    auto_notice_reset();
    memset(&g_weather_restart,0,sizeof(g_weather_restart));g_weather_wait_input=FALSE;
    g_manual_prompt=FALSE;
    g_manual_countdown=FALSE;
    g_manual_countdown_from_detector=FALSE;
    g_auto_cooldown_until=GetTickCount()+g_rf_config.cooldown_ms;
    InterlockedExchange(&g_green_restore_armed, 0);
    InterlockedExchange(&g_post_green_continuity_armed, 0);
    InterlockedExchange(&g_post_green_continuity_pending, 0);
    InterlockedExchange(&g_post_green_continuity_result, 0);

    InterlockedExchange(&g_y_restart_guard_armed, 0);
    if (warm) {
        restore_speed_cap();
        unretire_pitret();
        unhide_all();
        hide_red_flag_icon();
        /* A failed pre-restart transaction must not leave confirmed entrants
         * role-quarantined in the still-running race. */
        (void)restore_excluded_restart_roster();
    } else {
        /* No vehicle struct is touched. Just drop what we know about them. */
        g_np_active    = FALSE;
        g_pitret_count = 0;
        g_dnf_count    = 0;
        g_active_count = 0;
        g_late_count   = 0;
        hidden_forget();
        for (int i = 0; i < g_session_capacity; i++) g_np_armed[i] = 0;
    }

    /* Safe either way: a process-wide flag, two call sites in the EXE, and
     * our own bookkeeping. None of them live in a vehicle struct. */
    g_pos_lock_active = FALSE;
    hide_red_flag_icon(); /* also restore renderer gates on the cold path */
    g_dnf_hidden      = FALSE;
    g_det_seeded      = FALSE;
    kill_tracker_reset();
    hud_clear_countdown();
    vehicle_removal_restore();
    camera_action_restore();
    commentary_restore();
    remove_hook();
    remove_hook2();
    remove_player_grid_gate();
    remove_grid_camera_hook();
}

/* GRID_READY is already post-constructor. A failed player proof must not call
 * the pre-constructor warm rollback, because republishing excluded AI here
 * would create the exact post-construction VehicleCount discontinuity under
 * investigation. Tear down only this mod's transient UI/hooks and retain the
 * exact reduced roster until the owner exits the disposable session. */
static void abort_post_constructor_retain_roster(const char* why) {
    char buf[224];
    wsprintfA(buf, "RF-E64E2EA7BDD %s", why);
    log_w(buf);
    InterlockedExchange(&g_green_restore_armed, 0);
    InterlockedExchange(&g_post_green_continuity_armed, 0);
    InterlockedExchange(&g_post_green_continuity_pending, 0);
    InterlockedExchange(&g_post_green_continuity_result, 0);
    InterlockedExchange(&g_y_restart_guard_armed, 0);
    g_post_constructor_hold = TRUE;
    restore_speed_cap();
    g_pos_lock_active = FALSE;
    g_dnf_hidden = FALSE;
    hide_red_flag_icon();
    hud_clear_countdown();
    vehicle_removal_restore();
    camera_action_restore();
    commentary_restore();
    remove_hook();
    remove_hook2();
    remove_player_grid_gate();
    remove_grid_camera_hook();
    wsprintfA(buf, "RF-E83E07DB3D2 %d %d",
              VEHICLE_COUNT, g_roster_count);
    log_w(buf);
}

static void session_state_reset_scalars(void) {
    (void)rf_penalty_end();
    InterlockedExchange(&g_restart_cancelled, 0);
    InterlockedExchange(&g_restart_commit_blocked, 0);
    g_checkpoint_valid=FALSE;
    g_checkpoint_replay=FALSE;
    g_manual_prompt=FALSE;
    g_manual_prompt_since=0;
    g_manual_countdown=FALSE;
    g_manual_countdown_since=0;
    g_manual_countdown_from_detector=FALSE;
    remove_player_grid_gate();
    hud_position_denominator_release();
    replay_session_reset();
    replay_roster_guard_reset();
    g_player_box_still_since = 0;
    g_announcement_snapshot=FALSE;g_player_arrived_at=0;g_player_box_confirmed=FALSE;
    g_rf_policy_now=0;g_rf_policy_last=0;g_weather_rf_since=0;g_drive_started=0;
    g_hazard_history_count=0;
    memset(&g_weather_restart,0,sizeof(g_weather_restart));
    memset(&g_handoff_wait,0,sizeof(g_handoff_wait));
    g_weather_wait_input=FALSE;g_weather_restart_verdict=RF_WEATHER_READY;
    g_dnf_sweep_warning_logged = FALSE;
    g_removal_repair_count = 0;
    g_removal_repair_epoch = 0;
    g_det_seeded = FALSE;
    g_term_times_n = 0;
    g_auto_cooldown_until = 0;
    g_native_yellow_since = 0;
    g_auto_candidate_since = 0;
    g_placeholder_logged = FALSE;
    auto_notice_reset();
    g_settled_since = 0;
    g_prev_settled = FALSE;
    g_gate_pause_started = 0;
    g_late_count = 0;
    g_kill_count = 0;
    g_dnf_count = 0;
    g_dnf_hidden = FALSE;
    g_roster_count = 0;
    g_roster_original_count = 0;
    g_roster_reduced_epoch = 0;
    g_roster_exclusion_active = FALSE;
    g_roster_results_restored = FALSE;
    g_player_out_active = FALSE;
    g_player_out_committed = FALSE;
    g_player_out_ptr = NULL;
    g_player_out_epoch = 0;
    g_player_out_garage_valid = FALSE;
    g_player_out_pitstate = 0;
    g_player_out_player_out = 0;
    g_player_terminal_garage_lock_active = FALSE;
    g_player_tuple_order_count = 0;
    g_player_tuple_epoch = 0;
    InterlockedExchange(&g_player_tuple_armed, 0);
    InterlockedExchange(&g_player_tuple_pending, 0);
    InterlockedExchange(&g_player_tuple_result, 0);
    InterlockedExchange(&g_player_tuple_callbacks, 0);
    g_post_constructor_hold = FALSE;
    InterlockedExchange(&g_green_restore_armed, 0);
    InterlockedExchange(&g_green_restore_result, 0);
    InterlockedExchange(&g_post_green_continuity_armed, 0);
    InterlockedExchange(&g_post_green_continuity_pending, 0);
    InterlockedExchange(&g_post_green_continuity_result, 0);
    InterlockedExchange(&g_y_restart_guard_armed, 0);
    InterlockedExchange(&g_y_restart_prepare_state, 0);
    InterlockedExchange(&g_y_restart_prepare_result, 0);
    InterlockedExchange(&g_owned_restart_gate_pending, 0);
    InterlockedExchange(&g_full_y_reset_pending, 0);
    g_hidden_count = 0;
    g_hidden_epoch = 0;
    g_active_count = 0;
    g_placed = 0;
    g_np_hidx = 0;
    g_np_lap_len = 0.0f;
    g_np_leader = -1;
    g_np_cmd = PACE_TARGET;
    g_np_ramp_from = 0.0f;
    g_np_ramp_t0 = 0;
    g_np_active = FALSE;
    g_pos_lock_active = FALSE;
    g_saved_total_laps = 0;
    g_max_driven_laps = 0;
    g_pitret_count = 0;
    g_garage_count = 0;
    g_garage_epoch = 0;
    g_post_restart_normalize_until = 0;
    g_say_eid = -1;
    g_say_since = g_say_try = g_say_played_ms = 0;
    g_say_sent = g_say_played = FALSE;
    g_monitor_native_yellow = FALSE;
    g_monitor_yellow_ms = 0;
    g_monitor_terminals = g_monitor_terminal_now = 0;
    g_monitor_cooldown_ms = 0;
    g_monitor_auto_candidate = FALSE;
    g_monitor_settled = FALSE;
    g_monitor_game_state = g_monitor_race_phase = -1;
    g_monitor_stalled = g_monitor_stopped = g_monitor_crashed = 0;
    g_monitor_incident_score = g_monitor_incident_cars = 0;
    g_monitor_incident_terminals = 0;
    g_monitor_incident_damaged_stationary = 0;
    g_monitor_incident_ontrack_terminals = 0;
    g_monitor_blocking_road_contacts = 0;
    g_monitor_inverted_blocker = 0;
    g_monitor_passive_road_hazard = 0;
    g_player_terminal_detector_logged = FALSE;
    g_monitor_incident_lapdist = -1.0f;
    g_monitor_paused = g_monitor_multiplayer = 0;
    g_monitor_opening_window = 0;
    g_monitor_candidate_confirm_ms = 0;
    g_monitor_stop_reason = 0;
    g_monitor_prestop_elapsed_ms = 0;
    g_monitor_phase = 0;
    g_monitor_jos_found = FALSE;
    g_monitor_jos_active = g_monitor_jos_retired = -1;
    g_monitor_jos_pitstate = -1;
    g_monitor_jos_damage = 0;
    g_monitor_jos_rpm = 0.0f;
    g_monitor_jos_engine_life = 0.0f;
    g_monitor_jos_gearbox_life = 0.0f;
    g_monitor_jos_suspension_life = 0.0f;
    g_aiw_valid = g_aiw_main_waypoints = g_aiw_pit_waypoints = 0;
    g_aiw_player_found = 0;
    g_aiw_player_nearest_index = g_aiw_player_segment_index = -1;
    g_aiw_player_band = 0;
    g_aiw_player_corridor = -1;
    g_aiw_player_main_band = g_aiw_player_pit_band = 0;
    g_aiw_player_main_line_m = g_aiw_player_pit_line_m = 0.0f;
    g_aiw_player_node_m = g_aiw_player_path_m = 0.0f;
    g_aiw_player_dry_line_m = g_aiw_player_wet_line_m = 0.0f;
    g_aiw_player_closest_line_m = 0.0f;
    g_aiw_player_road_left_m = g_aiw_player_road_right_m = 0.0f;
    g_aiw_player_road_width_m = g_aiw_player_lateral_m = 0.0f;
    g_aiw_player_free_left_m = g_aiw_player_free_right_m = 0.0f;
    g_aiw_player_best_pass_m = 0.0f;
    g_aiw_player_critical_radius_m = g_aiw_player_danger_radius_m = 0.0f;
    g_aiw_player_speed = 0.0f;
    g_aiw_player_stationary_ms = g_aiw_player_stationary_last_tick = 0;
    g_aiw_player_stationary_ptr = 0;
    g_aiw_player_stationary_confirmed = 0;
    g_aiw_player_game_paused = g_aiw_player_on_road_geom = 0;
    g_aiw_player_surface_contacts = g_aiw_player_off_track = 0;
    g_aiw_blocking_valid = 0;
    g_aiw_blocking_nearest_index = -1;
    g_aiw_blocking_path_m = g_aiw_blocking_line_m = 0.0f;
    g_aiw_blocking_band = 0;
    g_aiw_blocking_corridor = -1;
    g_aiw_weather_on_wetness = g_aiw_weather_off_wetness = 0.0f;
    g_aiw_weather_mean_wetness = 0.0f;
    g_aiw_weather_class = 0;
}

static void session_leave_cold(const char* why) {
    char buf[192];
    if (!g_session_active) return;
    abort_sequence(FALSE, why);
    wsprintfA(buf, "RF-EB3D9499A78 %lu %s", g_session_serial, why);
    log_w(buf);
    g_session_active = FALSE;
    g_session_epoch_id = 0;
    if (SESSION_TYPE != 7 || (GAME_STATE != 5 && GAME_STATE != 7))
        replay_preserve_final_part_on_exit();
    session_state_reset_scalars();
    /* Hook entry can already be in flight when its call site is restored. */
    Sleep(100);
    session_arrays_release();
    mon_shared_publish();
}

static BOOL session_enter_if_ready(void) {
    int count;
    uintptr_t epoch;
    char buf[160];
    if (g_session_active) return TRUE;
    if (!session_field_ready(&count)) return FALSE;
#if RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR
    if (!g_hud_position_patch_installed) {
        hud_position_denominator_release();
        if (!hud_position_patch_sync(1)) {
            log_w("RF-EBA0AA0BFE5");
        }
    }
#endif
    if (!session_arrays_allocate(count)) return FALSE;
    session_state_reset_scalars();
    g_session_active = TRUE;
    epoch = session_epoch();
    if (!epoch) {
        g_session_active = FALSE;
        session_arrays_release();
        return FALSE;
    }
    g_session_epoch_id = epoch;
    ++g_session_serial;
    /* Validate the reviewed replay-exit visibility helper once per session. */
    g_replay_visibility_fn =
        (runtime_region_supported(F1C_REPLAY_SET_DRIVER_VISIBILITY_ADDRESS, 20, TRUE) &&
         f1c_replay_visibility_code_valid(
             (const void*)F1C_REPLAY_SET_DRIVER_VISIBILITY_ADDRESS, 20))
        ? (F1CReplayVisibilitySet)F1C_REPLAY_SET_DRIVER_VISIBILITY_ADDRESS : NULL;
    initial_grid_capture();
    cleanup_values_keep_sixty();
    wsprintfA(buf, "RF-E914FCD4F1C %lu %d %d",
              g_session_serial, count, g_session_capacity);
    log_w(buf);
    return TRUE;
}

static DWORD WINAPI thread_main(LPVOID p) {
    (void)p;
#ifndef RF_RELEASE
    BOOL prev = FALSE;
    BOOL prev_pitret = FALSE;
    BOOL prev_redflag = FALSE;
#endif
#if IN_GAME_MONITOR_ENABLED
    BOOL prev_status = FALSE;
#endif
    BOOL rf_waiting_fire=FALSE;
    BOOL rf_waiting_manual=FALSE;
    BOOL rf_waiting_sc_fault=FALSE;
    F1CSCFaultRequest rf_fault_request={0};
    int rf_active_sc_fault=0;
    int  phase = 0;
    DWORD phase_time = 0;
    DWORD pause_accum = 0;
    DWORD last_tick = 0;
    uintptr_t seq_epoch = 0;   /* field identity when this sequence started */
    int  last_beep_sec = -1;
    BOOL speed_cap_applied = FALSE;
    BOOL restart_transition_seen = FALSE;
    BOOL restart_grid_seen = FALSE;
    BOOL prev_manual_accept = FALSE;
    BOOL prev_manual_foreground = FALSE;
    DWORD restart_live_since = 0;
    int restart_start_game_state = 0;
    int restart_start_race_phase = 0;
    BOOL removal_cleanup_started = FALSE;
    DWORD removal_cleanup_since = 0;
    DWORD removal_cleanup_stable_since = 0;

#define RESET_SEQUENCE_LOCALS() do { \
    rf_waiting_fire=FALSE; rf_waiting_manual=FALSE; \
    rf_waiting_sc_fault=FALSE; rf_active_sc_fault=0; \
    phase = 0; phase_time = 0; pause_accum = 0; last_tick = 0; \
    seq_epoch = 0; last_beep_sec = -1; speed_cap_applied = FALSE; \
    restart_transition_seen = FALSE; restart_grid_seen = FALSE; \
    restart_live_since = 0; restart_start_game_state = 0; \
    restart_start_race_phase = 0; removal_cleanup_started = FALSE; \
    removal_cleanup_since = 0; removal_cleanup_stable_since = 0; \
} while (0)

    if (!runtime_layout_supported()) {
        g_running = FALSE;
        mon_shared_close();
        if (g_unload_ev) { CloseHandle(g_unload_ev); g_unload_ev = NULL; }
        if (g_lightloader_owned) {
            if (g_timer_period_active) {
                timeEndPeriod(1);
                g_timer_period_active = FALSE;
            }
            return ERROR_BAD_EXE_FORMAT;
        }
        FreeLibraryAndExitThread(g_hinst, ERROR_BAD_EXE_FORMAT);
        return ERROR_BAD_EXE_FORMAT;
    }
    if (!rf_broker_bind_after_dependency_wait()) {
        g_running = FALSE;
        mon_shared_close();
        if (g_unload_ev) { CloseHandle(g_unload_ev); g_unload_ev = NULL; }
        if (g_timer_period_active) {
            timeEndPeriod(1);
            g_timer_period_active = FALSE;
        }
        InterlockedExchange(&g_loader_init_state, 3);
        return ERROR_MOD_NOT_FOUND;
    }
    g_rc=f1c_rc_open();
    if(!g_rc){g_running=FALSE;rf_broker_unbind();mon_shared_close();return ERROR_INVALID_DATA;}
    InterlockedExchange(&g_rc->rf_registered,1);
    g_sc_fault_signal=f1c_scf_open();
    if(g_sc_fault_signal)InterlockedExchange(&g_sc_fault_signal->rf_registered,1);
    /* 1.1.6: crash summary on a real fault, installed only after the broker and
     * race-control handles are live. Removed on unload; never in DllMain. */
    rfu_veh_install();
#if RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR
    hud_position_denominator_release();
    if (hud_position_patch_sync(1)) {
        log_w("RF-EE30E1586E9");
    } else {
        log_w("RF-E7AF6E97A27");
    }
#endif
    /* Safe only after the executable layout checks above. Loading the DLL in
     * a different but layout-compatible EXE is supported; incompatible layouts
     * must perform no raw-address write. */
    unpatch_red_flag_sprite();
    log_w(g_rf_config.split_replay_on_restart
        ? "REPLAY ownership: LightLoader game-thread task invokes F1C's native writer before GameState=4, then preserves the closed vanilla file as Part N."
        : "REPLAY: automatic restart splitting disabled by redflag.ini; vanilla recorder behavior retained.");

#if IN_GAME_MONITOR_ENABLED
    status_gui_create();
#endif
#if LEGACY_PLR_REMOVAL_PATCH_ENABLED
    plr_removal_60();
#endif
#ifdef RF_RELEASE
    log_w("RF-EF878211A33");
#else
    log_w("RF-EF28AF21D11"
          "NUMPAD1 = DEBUG ORGANIC RED FLAG (same automatic notice and restart path). "
          "NUMPAD2 = FULL RED FLAG: AI to pits, restart when the player reaches his box "
          "(SHIFT+NUMPAD2 aborts). "
          "NUMPAD3 = pit-retire all AI (SHIFT+NUMPAD3 reverts).");
#endif

    while (g_running) {
        /* 1.1.6: write the usage summary on the worker when the game-thread
         * session-end callback has requested it. No file I/O on the game
         * thread; eject_teardown keeps its own unload flush. */
        rfu_pump();
        if (InterlockedCompareExchange(&g_replay_save_pending,0,0) < 0) {
            Sleep(1); continue;
        }
        int observed_count = 0;
        /* SESSION_TYPE is the absolute ownership boundary. Nothing from a
         * race survives even one menu tick. Phase 0 additionally watches the
         * full pointer-array epoch so a direct race reload is hygienic too. */
        /* Replay lifecycle before the generic session decision: entry, covered
         * playback changes and the first live tick after replay exit form one
         * ordered transition, so the native replay count shrink is never
         * mistaken for a session teardown. */
        replay_roster_guard_lifecycle_tick(phase);

        if (SESSION_TYPE != 7) {
            prev_manual_foreground = FALSE;
            if (g_session_active) {
                session_leave_cold("race session ended or returned to menu");
                RESET_SEQUENCE_LOCALS();
#ifndef RF_RELEASE
                prev = prev_pitret = prev_redflag = FALSE;
#endif
                prev_manual_accept = FALSE;
            }
            f1c_rc_rf_release(g_rc);
            Sleep(50);
            continue;
        }
        if (g_post_constructor_hold) {
            prev_manual_foreground = FALSE;
            /* Deliberately do not treat the reduced roster as a new session.
             * Only a real exit through SESSION_TYPE!=7 clears this latch. */
            Sleep(50);
            continue;
        }
        if (!g_session_active) {
            if (!session_enter_if_ready()) { Sleep(50); continue; }
            RESET_SEQUENCE_LOCALS();
#ifndef RF_RELEASE
            prev = (GetAsyncKeyState(HOTKEY) & 0x8000) != 0;
            prev_pitret = (GetAsyncKeyState(HOTKEY_PITRET) & 0x8000) != 0;
            prev_redflag = (GetAsyncKeyState(HOTKEY_REDFLAG) & 0x8000) != 0;
#endif
            prev_manual_accept =
                (GetAsyncKeyState(g_rf_config.activation_key) & 0x8000) != 0;
        } else if (session_field_ready(&observed_count) &&
                   ((phase == 0 && session_epoch() != g_session_epoch_id) ||
                    observed_count > g_session_capacity)) {
            if (replay_roster_transition_covers(observed_count)) {
                /* Native instant replay removed the parked/excluded vehicle
                 * and decremented VehicleCount; the session itself is still
                 * live. Keep the session and the guard baseline, and let the
                 * replay-exit callback restore on the game thread. A real
                 * reload/teardown fails the identity checks above and still
                 * takes the cold path below. */
                log_w("RF-EEFACB9E220");
                Sleep(1);
                continue;
            }
            /* A direct reload/teardown can rebuild the pointer array while
             * SessionType still says race. This is also how replay playback
             * starts. Never invoke the native recorder here: the loader may
             * already be destroying/replacing its vehicle and audio objects.
             * Only preserve a file that F1C has independently closed; a file
             * still owned by the game fails the MoveFile check harmlessly. */
            if (g_replay_series_active && !g_replay_finalized_at_results) {
                log_w("RF-E7230A0198F");
                replay_preserve_final_part_on_exit();
            }
            session_leave_cold("vehicle field changed outside the owned restart transition");
            RESET_SEQUENCE_LOCALS();
            if (!session_enter_if_ready()) { Sleep(50); continue; }
        }
        hud_position_denominator_mirror_live();
        if (phase == 0 &&
            InterlockedCompareExchange(&g_full_y_reset_pending, 0, 1) == 1) {
            log_w("RF-EC5ED5CF616");
            remove_hook();
            remove_hook2();
            remove_player_grid_gate();
            unhide_all();
            session_state_reset_scalars();
            g_session_epoch_id = session_epoch();
            RESET_SEQUENCE_LOCALS();
        }
        if (phase == 0 &&
            InterlockedCompareExchange(&g_y_restart_prepare_state, 2, 1) == 1) {
            LONG prepare_result = -1;
            log_w("RF-E04E70805C7");
            if (!g_running || !session_is_live_race() || RACE_PHASE != 5 ||
                !restart_player_supported() ||
                !g_checkpoint_valid || (g_roster_count > 0 && !g_roster_results_restored)) {
                prepare_result = -2;
                log_w("RF-EA24A67A1C4");
            } else {
                /* Restore the latest Red Flag checkpoint. Do not
                 * finalize replay here: the broker is deliberately holding
                 * the game thread and posting back to it would deadlock. */
                if (!checkpoint_restore()) {
                    prepare_result = -3;
                    log_w("RF-E98143093D5");
                } else if (!exclude_dnfs_from_restart_roster()) {
                    prepare_result = -4;
                    log_w("RF-EBDE4FD4AC6");
                } else if (!install_hook() || !install_hook2() ||
                           !arm_player_grid_gate()) {
                    prepare_result = -5;
                    log_w("RF-ED8B0F5C706");
                } else {
                    apply_qualpos();
                    prepare_original_lap_total();
                    restart_transition_seen = FALSE;
                    restart_grid_seen = FALSE;
                    restart_live_since = 0;
                    restart_start_game_state = GAME_STATE;
                    restart_start_race_phase = RACE_PHASE;
                    seq_epoch = 0;
                    phase = 2;
                    phase_time = GetTickCount();
                    prepare_result = 1;
                    log_w("RF-E1E85E5B4B5");
                }
            }
            if (prepare_result == -4 || prepare_result == -5) {
                remove_hook();
                remove_hook2();
                remove_player_grid_gate();
                (void)restore_excluded_restart_roster();
                re_retire_dnf();
                hide_dnf_cars();
                g_session_epoch_id = session_epoch();
            }
            InterlockedExchange(&g_y_restart_prepare_result, prepare_result);
            if (prepare_result != 1) g_checkpoint_replay=FALSE;
            MemoryBarrier();
            InterlockedExchange(&g_y_restart_prepare_state, 3);
        }
        if (phase == 0 && RACE_PHASE == 8 && g_replay_series_active &&
            !g_replay_finalized_at_results) {
            if (replay_finalize_current_part("race results"))
                g_replay_finalized_at_results = TRUE;
        }
        freeze_gate_clocks(GetTickCount());
#if IN_GAME_MONITOR_ENABLED
        status_gui_pump();
#endif
#ifndef RF_RELEASE
        BOOL key = (GetAsyncKeyState(HOTKEY) & 0x8000) != 0;
#endif
        BOOL manual_accept_down = (GetAsyncKeyState(g_rf_config.activation_key) & 0x8000) != 0;
        BOOL manual_modifiers =
            ((GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) |
              GetAsyncKeyState(VK_SHIFT) | GetAsyncKeyState(VK_LWIN) |
              GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
        HWND manual_foreground_window = GetForegroundWindow();
        DWORD manual_foreground_pid = 0;
        if (manual_foreground_window)
            GetWindowThreadProcessId(manual_foreground_window, &manual_foreground_pid);
        BOOL manual_foreground = manual_foreground_window != NULL &&
            manual_foreground_pid == GetCurrentProcessId();
        BOOL manual_accept_edge = manual_t_foreground_edge(manual_accept_down,
            prev_manual_accept, manual_modifiers, manual_foreground, prev_manual_foreground);
        prev_manual_accept = manual_accept_down;
        prev_manual_foreground = manual_foreground;

#if IN_GAME_MONITOR_ENABLED
        BOOL ks = (GetAsyncKeyState(HOTKEY_STATUS) & 0x8000) != 0;
        if (ks && !prev_status && g_status_window) {
            g_status_visible = !g_status_visible;
            ShowWindow(g_status_window, g_status_visible ? SW_SHOW : SW_HIDE);
        }
        prev_status = ks;
#endif

        /* ---- SESSION WATCHDOG, before anything else reads a vehicle ------
         * Runs every tick, whatever the phase. A sequence that cannot finish
         * is torn down here rather than left to strand the next race. The
         * epoch test is what catches ESC: the session is rebuilt, every
         * pointer we hold is stale, and the cold path never writes one. */
        /* Esc can arrive after the worker arms the byte but before the game
         * thread consumes it. Claim only our still-pending request. */
        if(phase==2 && InterlockedCompareExchange(&g_owned_restart_gate_pending,0,0)==1 &&
           PAUSE_FLAG && session_is_live_race()) {
            *RESTART_FLAG=0;Sleep(1);continue;
        }
        if(phase==2 && InterlockedCompareExchange(&g_owned_restart_gate_pending,0,0)==1 &&
           restart_context_live())*RESTART_FLAG=1;
        if (phase == 2 &&
            InterlockedCompareExchange(&g_owned_restart_gate_pending, 0, 0) &&
            !restart_context_live()) {
            InterlockedExchange(&g_restart_cancelled, 1);
            if (InterlockedCompareExchange(&g_owned_restart_gate_pending, 0, 1) == 1) {
                *RESTART_FLAG=0;
                abort_sequence(restart_abort_warm(), "pending restart cancelled before game-thread consumption");
                phase=0; seq_epoch=0; continue;
            }
        }
        if (InterlockedCompareExchange(&g_restart_commit_blocked, 0, 1) == 1) {
            abort_sequence(restart_abort_warm(), "restart refused at the final game-thread gate");
            phase=0;
            seq_epoch=0;
            continue;
        }
        if (phase == 1 || phase == 10) {
            /* PRE-RESTART ONLY. Phases 2 and 3 are the restart itself, which
             * legitimately rebuilds the field - running the epoch test there
             * would abort the very thing we asked for. They get a wall-clock
             * backstop instead, below. */
            if (PLAYER_OUT_FLAG || !race_is_settled() || !restart_player_supported()) {
                (void)restart_preparation_context(seq_epoch,"pre-restart watchdog");
                abort_sequence(restart_abort_warm(), "Esc/player exit/results cancelled the pending Red Flag (including unsupported player state before restart)");
                phase=0;
                seq_epoch=0;
                continue;
            } else if (!session_is_live_race()) {
                abort_sequence(FALSE, "session is no longer a live race "
                                      "(ESC to the menu, or the session ended)");
                phase = 0;
                seq_epoch = 0;
            } else if (seq_epoch && session_epoch() != seq_epoch) {
                abort_sequence(FALSE, "the field was rebuilt under us "
                                      "(session restarted or reloaded)");
                phase = 0;
                seq_epoch = 0;
            }
        } else if (phase == 2 || phase == 3) {
            /* Backstop for the post-restart phases. Both have their own exit
             * conditions (GAME_STATE, then RACE_PHASE), but each of those is a
             * read of game state that a session change can freeze, and a stuck
             * phase 3 keeps the flag icon armed for every later race - which is
             * the bug this whole block exists for. Nothing legitimate takes two
             * minutes. */
            DWORD phase_limit = phase == 2
                ? RESTART_GARAGE_MAX_MS : PHASE_MAX_MS;
            if (phase_time && GetTickCount() - phase_time > phase_limit) {
                if (phase == 3 &&
                    InterlockedCompareExchange(&g_green_restore_armed, 0, 0) == 2) {
                    phase_time = GetTickCount();
                    log_w("RF-EB3E0733E67");
                    continue;
                }
                abort_post_constructor_retain_roster(
                    "owned restart phase ran past its time limit");
                phase = 0;
                seq_epoch = 0;
            }
        } else if (seq_epoch) {
            seq_epoch = 0;
        }

        /* Drive the one-line say machine: retry into a free channel, watch
         * EVENT_TS for the confirmation. Must run every tick, in every phase,
         * because the line is requested in phase 0 and consumed in 1 or 10. */
        say_pump();

        /* Repair only impossible, fast-moving stale pit states during the
         * bounded window armed when a restarted race reaches RacePhase 5. */
        normalize_post_restart_pit_states(GetTickCount());


        /* Learn real per-driver garage coordinates only while no red-flag
         * sequence is manipulating the field. The working retirement path
         * deliberately does not use this preserved experimental cache. */
        if (phase == 0) cache_garage_positions();

#ifndef RF_RELEASE
        /* NUMPAD3: pit-retire every AI car. SHIFT+NUMPAD3 reverts arms that
         * have not fired yet. Race only; independent of the red-flag phase. */
        BOOL kp = (GetAsyncKeyState(HOTKEY_PITRET) & 0x8000) != 0;
        if (kp && !prev_pitret && mp_blocks("NUMPAD3")) {
            /* refused: multiplayer */
        } else if (kp && !prev_pitret) {
            if (session_is_live_race()) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) pit_retire_revert_all();
                else                                     pit_retire_all();
                (void)0;   /* Beep removed */
            } else {
                log_w("RF-E77717F08B5");
            }
        }
        prev_pitret = kp;

        /* NUMPAD2: full red-flag procedure.
         *   snapshot the grid NOW (everyone still active, positions are the
         *   order at the moment of the red flag) -> send every AI to the pits
         *   -> wait for the player to reach his own box -> restart.
         * Because the snapshot happens BEFORE they retire, the cars we retire
         * never enter g_dnf_ptrs, so re_retire_dnf() in phase 3 leaves them
         * alone and they come back on the grid. */
        BOOL kr = (GetAsyncKeyState(HOTKEY_REDFLAG) & 0x8000) != 0;
        if (kr && !prev_redflag && phase == 0 && mp_blocks("NUMPAD2")) {
            /* refused: multiplayer */
        } else if (kr && !prev_redflag && phase == 0) {
            if (session_is_live_race()) {
                log_w("RF-E743EF4D39D"
                      "restart when the player reaches his pit box.");
                InterlockedExchange(&g_y_restart_guard_armed, 0);
                g_dnf_hidden      = FALSE;
                g_pos_lock_active = FALSE;
                speed_cap_applied = FALSE;
                g_fuel_logged     = FALSE;
                snapshot_positions();          /* grid order, pre-retirement */
                inject_commentary(0x0083);     /* PWS_RED01 */
                show_red_flag_icon();
                pit_retire_all();              /* AI drive in and park */
                /* Hold the field to the pace and forbid passing on the way in.
                 * Each car releases itself the moment it enters the pit lane
                 * (pittingStatus >= 2), where the game's own limiter takes the
                 * same maxSpeed field over. */
                apply_speed_cap();
                g_pos_lock_active = TRUE;
                (void)0;   /* Beep removed */
                removal_cleanup_started = FALSE;
                removal_cleanup_since = 0;
                removal_cleanup_stable_since = 0;
                seq_epoch = session_epoch();
                phase = 10;
                phase_time = GetTickCount();
                log_w("RF-E75316011F9"
                      "(pittingStatus 3, or stopped in the lane). "
                      "SHIFT+NUMPAD2 aborts.");
            } else {
                log_w("RF-EC56F8B3B39");
            }
        } else if (kr && !prev_redflag && phase == 10 &&
                   (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            log_w("RF-ECCB5AABDC2");
            restore_speed_cap();
            g_pos_lock_active = FALSE;
            unretire_pitret();
            commentary_restore();
            hide_red_flag_icon();
            phase = 0;
        }
        prev_redflag = kr;
#endif

        /* Reset automatic-trigger state whenever we're not in a live race. */
        if (SESSION_TYPE != 7) {
            g_gate_pause_started = 0;
            g_det_seeded = FALSE;
            g_post_restart_normalize_until = 0;
            g_native_yellow_since = 0;
            g_auto_candidate_since = 0;
            auto_notice_reset();
        }

#ifndef RF_RELEASE
        BOOL debug_edge = key && !prev;
        if (debug_edge && phase == 0 && mp_blocks("NUMPAD1 debug Red Flag")) {
            /* refused: multiplayer */
        } else if (debug_edge && phase == 0) {
            if (session_is_live_race())
                debug_prime_organic_redflag(GetTickCount());
            else
                log_w("RF-E04D6D846A4");
        }
#endif
        if(g_announcement_snapshot || phase==1 || phase==10)sample_restart_race_fuel();
        BOOL auto_candidate = FALSE;
        BOOL autofire = FALSE;
        BOOL manual_fire=FALSE;
        BOOL manual_safe=phase == 0 && session_is_live_race() &&
            race_is_settled() && !is_multiplayer() && !PAUSE_FLAG &&
            !PLAYER_OUT_FLAG && !g_post_constructor_hold;
        if (manual_safe && !restart_player_supported()) {
            g_manual_prompt=FALSE;
            g_manual_countdown=FALSE;
            g_manual_countdown_from_detector=FALSE;
            rf_waiting_fire=FALSE;
            auto_notice_reset();
            f1c_rc_rf_release(g_rc);
            if (manual_accept_edge) {
                g_player_refusal_until=GetTickCount()+3000u;
                log_w("RF-E13B4D03B58");
            }
            if (g_player_refusal_until &&
                (LONG)(g_player_refusal_until-GetTickCount()) > 0) {
                hud_auto_notice_write(0,"RED FLAG UNAVAILABLE");
                hud_auto_notice_write(1,"PLAYER STATE NOT SUPPORTED");
            }
            Sleep(50);
            continue;
        }
        g_player_refusal_until=0;
        DWORD policy_tick=GetTickCount();
        if(g_rf_policy_last && !PAUSE_FLAG && policy_tick-g_rf_policy_last>1000u)
            g_weather_rf_since=0;
        BOOL weather_fresh=!g_rf_policy_last || policy_tick-g_rf_policy_last<=1000u;
        rf_policy_clock(policy_tick,PAUSE_FLAG!=0);
        if(g_handoff_wait.failed) {
            f1c_rc_request(g_rc,2);
            hud_auto_notice_write(0,"RED FLAG: SC HANDOFF FAILED");
            hud_auto_notice_write(1,"RESTART THE SESSION");
            hud_auto_notice_write(2,"SAFETY CAR RETURN NOT CONFIRMED");
            Sleep(50);continue;
        }
        if(phase==1 || phase==10) {
            int old_override=g_weather_restart.overridden;
            g_weather_restart_verdict=rf_weather_restart_step(&g_weather_restart,
                g_rf_policy_now,PAUSE_FLAG!=0,weather_fresh,ON_PATH_WETNESS,
                g_rf_config.weather_resume_max,g_rf_config.weather_resume_ms,
                g_weather_wait_input,manual_accept_edge,g_rf_config.weather_override);
            if(!old_override && g_weather_restart.overridden)
                log_w("RF-EBF46B901DD");
        }
        DWORD manual_now=g_rf_policy_now;
        BOOL manual_cancel=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
        int manual_result=(g_manual_countdown || rf_waiting_fire) ? 0 : manual_request_update(g_rf_config.manual_enabled,
            manual_safe, manual_accept_edge, (GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0,
            g_auto_notice_reason != 0, manual_now);
        if (manual_result == 3) hud_auto_notice_clear();
        if (manual_result == 1 || manual_result == 2) {
            f1c_rc_request(g_rc,1);auto_notice_reset();
        }
        if (manual_result == 2) {
            g_manual_countdown_from_detector=FALSE;
            snapshot_positions();g_announcement_snapshot=TRUE;
        }
        BOOL committed_context=phase==0 && session_is_live_race() &&
            race_is_settled() && !is_multiplayer() && !PLAYER_OUT_FLAG && !g_post_constructor_hold;
        BOOL countdown_safe=committed_context && (g_manual_countdown_from_detector
            ? (g_rf_config.automatic_enabled && g_rf_config.require_manual_acceptance)
            : g_rf_config.manual_enabled);
        int countdown_result=manual_countdown_update(manual_result == 2,
            countdown_safe, manual_accept_edge, manual_cancel, manual_now,
            g_rf_config.manual_activation_countdown_ms);
        if (countdown_result == 3) {
            (void)rf_penalty_end();
            g_announcement_snapshot=FALSE;
            auto_notice_reset();
            if (g_manual_countdown_from_detector)
                g_auto_cooldown_until=GetTickCount()+g_rf_config.cooldown_ms;
        }
        manual_fire=countdown_result == 2;
        /* Accepted countdown retains its evidence while waiting for SC. */
        if (g_manual_prompt) {
            hud_auto_notice_write(0, "RED FLAG ACTIVATION");
            hud_auto_notice_write(1, "RACE WILL BE STOPPED");
            rf_key_hint(2,"AGAIN");
        }
        BOOL detector_enabled = !rf_waiting_fire && g_rf_config.automatic_enabled && !g_manual_prompt && !manual_fire &&
            !g_manual_countdown && manual_result != 3 && countdown_result != 3;
#ifndef RF_RELEASE
        if (g_debug_organic_notice) detector_enabled = TRUE;
#endif
        /* Fail closed before detector side effects: disabled or non-offline
         * sessions must not receive warnings, prompts, cooldowns, or latches. */
        if (phase == 0 && SESSION_TYPE == 7 && detector_enabled &&
            !is_multiplayer())
            auto_candidate = check_auto_redflag(manual_accept_edge);
        if (auto_candidate && g_rf_config.require_manual_acceptance) {
            if (manual_countdown_update(TRUE, manual_safe, FALSE, manual_cancel,
                    manual_now, g_rf_config.manual_activation_countdown_ms) == 4) {
                g_manual_countdown_from_detector=TRUE;
                snapshot_positions();g_announcement_snapshot=TRUE;
                hud_auto_notice_clear();
            } else auto_notice_reset();
            auto_candidate=FALSE;
        }
        if (g_manual_countdown) {
            char manual_countdown_text[32];
            wsprintfA(manual_countdown_text, "RED FLAG IN %u SECONDS",
                manual_countdown_seconds(manual_now, g_rf_config.manual_activation_countdown_ms));
            hud_auto_notice_write(0, manual_countdown_text);
            if(g_rf_config.allow_cancel)rf_key_hint(1,"TO CANCEL");
            else hud_auto_notice_write(1,"RED FLAG CONFIRMED");
        }
#if AUTO_REDFLAG_ENABLED
        autofire = g_rf_config.automatic_enabled && auto_candidate;
#ifndef RF_RELEASE
        if (g_debug_organic_notice && auto_candidate) autofire = TRUE;
#endif
#endif

        /* A failed Safety Car is an operational red flag, not a scored AI
           incident or manual offer. All restart and lease guards still apply. */
        F1CSCFaultRequest incoming_fault={0};
        BOOL sc_fault_candidate=phase==0 && g_rf_config.sc_fault_enabled && g_sc_fault_signal &&
            f1c_rc_read(&g_rc->sc_registered) &&
            f1c_scf_peek(g_sc_fault_signal,f1c_rc_read(&g_rc->sc_generation),&incoming_fault);
        if(sc_fault_candidate && manual_safe && !manual_cancel) {
            rf_fault_request=incoming_fault;rf_waiting_sc_fault=TRUE;
        }
        BOOL weather_fire=FALSE;
        float weather=ON_PATH_WETNESS;
        if(g_rf_config.weather_enabled && committed_context && g_rc &&
           f1c_rc_read(&g_rc->owner)==F1C_RC_SC && isfinite(weather) && weather>=0.96f && weather<=1.0f) {
            if(!g_weather_rf_since)g_weather_rf_since=g_rf_policy_now;
            weather_fire=g_rf_policy_now-g_weather_rf_since>=g_rf_config.weather_confirm_ms;
        } else g_weather_rf_since=0;
        if(weather_fire && !rf_waiting_fire && !g_manual_countdown) {
            memset(&g_weather_restart,0,sizeof(g_weather_restart));g_weather_restart.required=1;
            g_weather_restart_verdict=RF_WEATHER_WAIT;
            f1c_rc_request(g_rc,1);
            if(!g_announcement_snapshot){snapshot_positions();g_announcement_snapshot=TRUE;}
            log_w("RF-EE556C76B3F");
            hud_auto_notice_write(0,"RED FLAG: EXTREME WEATHER");
        }
        autofire = autofire || manual_fire || (weather_fire && manual_safe) ||
            (sc_fault_candidate && manual_safe && !manual_cancel);
        /* Preserve an accepted request across callbacks while SC restores on
           the broker thread. Only optional pre-mutation activation-key cancellation applies. */
        if(autofire){rf_waiting_fire=TRUE;rf_waiting_manual=!rf_waiting_sc_fault && manual_fire && !g_manual_countdown_from_detector;}
        if(!rf_waiting_fire)rf_handoff_wait_step(&g_handoff_wait,g_rf_policy_now,0,0,0,g_rf_config.sc_handoff_timeout_ms);
        if(rf_waiting_fire) {
            BOOL user_cancel=g_rf_config.allow_cancel && manual_accept_edge && !manual_fire && !auto_candidate;
            BOOL paused_wait=PAUSE_FLAG && !user_cancel && committed_context;
            BOOL still_valid=committed_context && !user_cancel;
            if(paused_wait) {
                autofire=FALSE; /* preserve request; revalidate on resume, never acquire while paused */
            } else if(!still_valid) {
                (void)rf_penalty_end();
                g_announcement_snapshot=FALSE;
                memset(&g_weather_restart,0,sizeof(g_weather_restart));
                rf_handoff_wait_step(&g_handoff_wait,g_rf_policy_now,0,0,0,g_rf_config.sc_handoff_timeout_ms);
                if(rf_waiting_sc_fault)f1c_scf_ack(g_sc_fault_signal,rf_fault_request.sequence);
                rf_waiting_sc_fault=FALSE;
                rf_waiting_fire=FALSE;autofire=FALSE;auto_notice_reset();
                f1c_rc_request(g_rc,0);
            } else {
                autofire=rf_rc_acquire();
                rf_handoff_wait_step(&g_handoff_wait,g_rf_policy_now,1,0,autofire,g_rf_config.sc_handoff_timeout_ms);
                if(g_handoff_wait.failed) {
                    log_w("RF-E4424065412");
                    continue;
                }
                if(!autofire){hud_auto_notice_write(0,rf_waiting_sc_fault ? "RED FLAG: SAFETY CAR FAULT" : "RED FLAG WAIT: SAFETY CAR");
                    if(g_rf_config.allow_cancel)rf_key_hint(1,"TO CANCEL");
                    else hud_auto_notice_write(1,"RED FLAG CONFIRMED");}
            }
        }

        if (!restart_context_allowed(GAME_STATE, RACE_PHASE, SESSION_TYPE,
                PAUSE_FLAG != 0, PLAYER_OUT_FLAG, FALSE,
                (GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0)) autofire=FALSE;
        if (autofire && phase == 0 && !restart_player_supported()) {
            rf_waiting_fire=FALSE; autofire=FALSE;
            auto_notice_reset(); f1c_rc_rf_release(g_rc);
            log_w("RF-E13B4D03B58");
            hud_auto_notice_write(0,"RED FLAG UNAVAILABLE");
            hud_auto_notice_write(1,"PLAYER STATE NOT SUPPORTED");
            hud_auto_notice_write(2,"");
        }
#ifdef RF_RELEASE
        if (autofire && phase == 0 && mp_blocks("automatic Red Flag")) {
#else
        if (autofire && phase == 0 && mp_blocks("NUMPAD1")) {
#endif
            /* refused: multiplayer */
        } else if (autofire && phase == 0) {
            rf_active_sc_fault=rf_waiting_sc_fault?rf_fault_request.reason:0;
            if(rf_waiting_sc_fault){
                char fault_log[192];
                wsprintfA(fault_log,"TRIGGER: dedicated SAFETY CAR FAULT reason=%ld request=%ld generation=%ld; physical handoff acknowledged.",
                    rf_fault_request.reason,rf_fault_request.sequence,rf_fault_request.generation);
                log_w(fault_log);f1c_scf_ack(g_sc_fault_signal,rf_fault_request.sequence);
                rf_waiting_sc_fault=FALSE;
            }
            rf_waiting_fire=FALSE;
            if (session_is_live_race()) {
                refresh_terminal_state(GetTickCount());hazard_mark_handled();
                g_auto_cooldown_until=GetTickCount()+g_rf_config.cooldown_ms;
                g_native_yellow_since=0;
                if(!rf_active_sc_fault) {
#ifndef RF_RELEASE
                log_w(g_debug_organic_notice
                    ? "TRIGGER: debug NUMPAD1 after the organic automatic reason announcement"
                    : "TRIGGER: auto after severe-incident announcement");
#else
                log_w("RF-E6247989179");
#endif
                }
                if (manual_fire) log_w(g_manual_countdown_from_detector
                    ? "TRIGGER: T-approved incident still qualifies after activation countdown."
                    : "TRIGGER: player confirmed manual Red Flag with T; activation countdown completed.");
                g_checkpoint_replay=FALSE;
                InterlockedExchange(&g_restart_cancelled, 0);
                InterlockedExchange(&g_restart_commit_blocked, 0);
                hud_auto_notice_clear();
                InterlockedExchange(&g_y_restart_guard_armed, 0);
                g_dnf_hidden      = FALSE;
                g_pos_lock_active = FALSE;
                speed_cap_applied = FALSE;
                g_fuel_logged     = FALSE;
                if(!g_announcement_snapshot)snapshot_positions();
                g_announcement_snapshot=FALSE;
                unhide_all();
                if(!g_penalty_valid) {
                    hud_auto_notice_write(0,"RED FLAG HELD: PENALTY DATA");
                    g_post_constructor_hold=TRUE; /* explicit refusal; only real session exit clears */
                    phase=0;continue;
                }
                sweep_late_dnf();
                g_player_arrived_at=0;g_player_box_confirmed=FALSE;g_drive_started=g_rf_policy_now;
                inject_commentary(0x0083);
                log_w("RF-E5939AAF6B4");
                show_red_flag_icon();
                (void)0;   /* Beep removed */
                seq_epoch = session_epoch();
                phase_time = GetTickCount();
                removal_cleanup_started = FALSE;
                removal_cleanup_since = 0;
                removal_cleanup_stable_since = 0;
                if (g_rf_config.restart_procedure ==
                        RF_RESTART_PROCEDURE_DRIVE_TO_PITS &&
                    !g_player_out_active && !rf_active_sc_fault) {
                    pit_retire_all();
                    apply_speed_cap();
                    g_pos_lock_active = TRUE;
                    phase = 10;
                    log_w("RF-E55959FDF35");
                } else {
                    if (g_rf_config.restart_procedure ==
                            RF_RESTART_PROCEDURE_DRIVE_TO_PITS &&
                        g_player_out_active)
                        log_w("RF-E3B6FE6F3EF");
                    phase = 1;
                    pause_accum = 0;
                    last_tick = phase_time;
                    last_beep_sec =
                        (int)(g_rf_config.restart_countdown_ms / 1000u);
                    /* Native Vehicle/Debris/Parts timing remains untouched
                     * during the visible countdown. All cleanup overrides
                     * begin only after it expires. */
                    {
                        char countdown_log[192];
                        wsprintfA(countdown_log,
                                  "RF-ECD47D5547C %lu",
                                  (unsigned long)g_rf_config.restart_countdown_ms);
                        log_w(countdown_log);
                    }
                }
            } else {
                log_w("RF-E01B5728A99");
            }
        }
#ifndef RF_RELEASE
        prev = key;
#endif

        if (phase == 1) {
            refresh_red_flag_icon();
            DWORD now = GetTickCount();
            if (PAUSE_FLAG) {
                last_tick = now;
                Sleep(1);
                continue;
            }
            pause_accum += now - last_tick;
            last_tick = now;
            if(rf_active_sc_fault)hud_auto_notice_write(2,"SAFETY CAR FAULT");

            /* After 500ms: suppress commentary, apply speed cap, lock positions */
            if (pause_accum >= 500) {
                commentary_suppress();
                if (!speed_cap_applied) {
                    apply_speed_cap();
                    g_pos_lock_active = TRUE;
                    speed_cap_applied = TRUE;
                    log_w("RF-E94913CCB57");
                }
            }

            /* Catch cars wrecked DURING the countdown before they get a grid
             * slot. Must run BEFORE enforce_positions(), which walks g_active. */
            sweep_late_dnf();
            /* Do not fight native stationary removal. At countdown end the
             * bounded cleanup stage deliberately lets that path place every
             * DNF in its real garage before the restart is armed. */

            /* Enforce no-overtaking every tick during countdown */
            if (g_pos_lock_active) enforce_positions();

            DWORD countdown_ms = g_rf_config.restart_countdown_ms;
            int sec_left = pause_accum < countdown_ms
                ? (int)((countdown_ms - pause_accum + 999u) / 1000u) : 0;
            if (sec_left >= 1) hud_countdown(sec_left);
            /* The per-second countdown beeps are gone (owner's request,
             * 2026-08-02): PC-speaker tones over the game audio. The HUD
             * countdown above is the whole cue now. last_beep_sec is kept
             * only so the tick-edge test still exists if audio comes back. */
            if (sec_left != last_beep_sec && sec_left >= 1) last_beep_sec = sec_left;

            if (pause_accum >= countdown_ms) {
                if(g_weather_restart_verdict!=RF_WEATHER_READY) {
                    weather_restart_message();Sleep(1);continue;
                }
                g_weather_wait_input=FALSE;
                DWORD cleanup_now = GetTickCount();
                if (!removal_cleanup_started) {
                    replay_prepare_reset();
                    removal_cleanup_started = TRUE;
                    removal_cleanup_since = cleanup_now;
                    removal_cleanup_stable_since = 0;
                    vehicle_removal_force_begin();
                    log_w("RF-EFE83994829");
                }
                vehicle_removal_force_begin();
                hud_clearing_track();

                int waiting_for_garage = dnf_waiting_for_native_garage();
                int waiting_for_loose = cleanup_loose_objects_waiting();
                BOOL loose_probe_pending = InterlockedCompareExchange(
                    &g_cleanup_loose_task_pending, 0, 0) != 0;
                if (waiting_for_garage > 0 || waiting_for_loose > 0 ||
                    loose_probe_pending) {
                    DWORD cleanup_elapsed = cleanup_now - removal_cleanup_since;
                    if (cleanup_elapsed >= VEHICLE_REMOVAL_CLEANUP_MAX_MS) {
                        /* Native removal occasionally leaves a physically dead
                         * car at Active=0/Retired=0 forever (live example:
                         * Trulli, 2026-08-19). Cancelling here strands the whole
                         * race. Keep the car in g_dnf_ptrs and use the proven
                         * phase-3 carry-over fallback after restart: re-retire
                         * it, stop its physics and suppress its scene object. */
                        if (!removal_cleanup_stable_since) {
                            char buf[224];
                            wsprintfA(buf,
                                "RF-EF6284B585C %d %d",
                                waiting_for_garage, waiting_for_loose);
                            log_w(buf);
                            removal_cleanup_stable_since = cleanup_now;
                        }
                    } else {
                        removal_cleanup_stable_since = 0;
                        Sleep(1);
                        continue;
                    }
                }

                /* Native cleanup completed. Learn the game's exact garage
                 * transforms before any restart placement can overwrite them. */
                cache_garage_positions();
                if (!removal_cleanup_stable_since) {
                    removal_cleanup_stable_since = cleanup_now;
                    Sleep(1);
                    continue;
                }
                if (cleanup_now - removal_cleanup_stable_since
                    < VEHICLE_REMOVAL_STABLE_MS) {
                    Sleep(1);
                    continue;
                }
                vehicle_removal_restore();

                if (!replay_finalize_current_part("timer Red Flag restart")) {
                    Sleep(1); continue;
                }
                int prepare = restart_preparation_context(seq_epoch,"countdown");
                if (prepare == 0) { Sleep(1); continue; }
                if (prepare < 0) {
                    abort_sequence(restart_abort_warm(), "restart preparation context invalid; see exact predicates above");
                    phase=0; seq_epoch=0; continue;
                }
                /* Release only after replay completion and final context proof. */
                restore_speed_cap();
                g_pos_lock_active = FALSE;
                hud_clear_countdown();
                log_w("RF-E6FE07825C9");
                if (!exclude_dnfs_from_restart_roster()) {
                    abort_sequence(TRUE, "DNF role-quarantine transaction failed; restart not armed");
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                }
                if (!install_hook() || !install_hook2() ||
                    !arm_player_grid_gate()) {
                    abort_sequence(TRUE, "DNF restart-constructor guards failed to install; restart not armed");
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                }
                log_w("RF-E105DF56A65");
                apply_qualpos();
                prepare_original_lap_total();
                restart_transition_seen = FALSE;
                restart_grid_seen = FALSE;
                restart_live_since = 0;
                restart_start_game_state = GAME_STATE;
                restart_start_race_phase = RACE_PHASE;
                /* Do not filter the restart event stream. Event subtypes 2-10
                 * include session, player and car-selection state as well as
                 * cameras; completing them as synthetic Wait events corrupted
                 * pause/PitState and could prevent the race from starting. */
                log_w("RF-E3200337B01");
                hide_red_flag_icon();
                unpatch_red_flag_sprite();
                InterlockedExchange(&g_owned_restart_gate_pending, 1);
                *RESTART_FLAG = 1;
                log_w("RF-EA7FA15E6B3");
                (void)0;   /* Beep removed */
                phase = 2;
                phase_time = GetTickCount();
            }
        } else if (phase == 10) {
            /* NUMPAD2 wait state: AI are driving in / parking themselves.
             * Fire the restart the moment the PLAYER reaches his own box. */
            refresh_red_flag_icon();
            commentary_suppress();
            if (PAUSE_FLAG) { Sleep(1); continue; }

            /* Same problem as the countdown: the NUMPAD2 wait can be minutes
             * long, so cars wrecked while we wait must not get a grid slot. */
            sweep_late_dnf();
            /* Native vehicle removal owns pre-existing on-track DNFs. */

            /* Hold the pace and the order for whoever is still on track. */
            enforce_positions();

            if(manual_accept_edge && !g_weather_wait_input && !removal_cleanup_started &&
               g_rf_policy_now-g_drive_started>=REDFLAG_WAIT_TIMEOUT_MS) {
                /* Explicit timeout fallback, still an RF and still excludes real DNFs. */
                unretire_pitret();g_pitret_count=0;g_pit_arrived_count=0;
                phase=1;pause_accum=0;last_tick=GetTickCount();
                log_w("RF-E2EE4E7F2ED");
                continue;
            }
            if(manual_accept_edge && !g_weather_wait_input && !removal_cleanup_started) {
                g_player_box_confirmed=TRUE;
                if(player_in_pit_box()) {
                    hud_auto_notice_write(0,"RESTART CONFIRMED");
                    hud_auto_notice_write(1,"WAITING FOR REMAINING CARS");
                    log_w("RF-EA98F1A0402");
                }
            }
            if (removal_cleanup_started || pit_survivors_ready()) {
                if(g_weather_restart_verdict!=RF_WEATHER_READY) {
                    weather_restart_message();Sleep(1);continue;
                }
                g_weather_wait_input=FALSE;
                DWORD cleanup_now = GetTickCount();
                if (!removal_cleanup_started) {
                    replay_prepare_reset();
                    removal_cleanup_started = TRUE;
                    removal_cleanup_since = cleanup_now;
                    removal_cleanup_stable_since = 0;
                    vehicle_removal_force_begin();
                }
                vehicle_removal_force_begin();
                hud_auto_notice_write(0,"RACE WILL RESTART");
                hud_auto_notice_write(1,"PREPARING RESTART");
                int waiting_for_garage = dnf_waiting_for_native_garage();
                int waiting_for_loose = cleanup_loose_objects_waiting();
                BOOL loose_probe_pending = InterlockedCompareExchange(
                    &g_cleanup_loose_task_pending, 0, 0) != 0;
                if (waiting_for_garage > 0 || waiting_for_loose > 0 ||
                    loose_probe_pending) {
                    DWORD cleanup_elapsed = cleanup_now - removal_cleanup_since;
                    if (cleanup_elapsed >= VEHICLE_REMOVAL_CLEANUP_MAX_MS) {
                        if (!removal_cleanup_stable_since) {
                            char buf[224];
                            wsprintfA(buf,
                                "RF-E80BB46C85E %d %d",
                                waiting_for_garage, waiting_for_loose);
                            log_w(buf);
                            removal_cleanup_stable_since = cleanup_now;
                        }
                    } else {
                        removal_cleanup_stable_since = 0;
                        Sleep(1);
                        continue;
                    }
                }
                cache_garage_positions();
                if (!removal_cleanup_stable_since) {
                    removal_cleanup_stable_since = cleanup_now;
                    Sleep(1);
                    continue;
                }
                if (cleanup_now - removal_cleanup_stable_since
                    < VEHICLE_REMOVAL_STABLE_MS) {
                    Sleep(1);
                    continue;
                }
                vehicle_removal_restore();
                log_w("RF-E4B489A97CF");
                if (!replay_finalize_current_part("full Red Flag restart")) {
                    Sleep(1); continue;
                }
                int prepare = restart_preparation_context(seq_epoch,"pit-return");
                if (prepare == 0) { Sleep(1); continue; }
                if (prepare < 0) {
                    abort_sequence(restart_abort_warm(), "restart preparation context invalid; see exact predicates above");
                    phase=0; seq_epoch=0; continue;
                }
                restore_speed_cap();
                g_pos_lock_active = FALSE;
                unretire_pitret();      /* put our retirees back on the grid */
                if (!exclude_dnfs_from_restart_roster()) {
                    abort_sequence(TRUE, "DNF role-quarantine transaction failed; restart not armed");
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                }
                if (!install_hook() || !install_hook2() ||
                    !arm_player_grid_gate()) {
                    abort_sequence(TRUE, "DNF restart-constructor guards failed to install; restart not armed");
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                }
                log_w("RF-E105DF56A65");
                apply_qualpos();
                prepare_original_lap_total();
                restart_transition_seen = FALSE;
                restart_grid_seen = FALSE;
                restart_live_since = 0;
                restart_start_game_state = GAME_STATE;
                restart_start_race_phase = RACE_PHASE;
                log_w("RF-E3200337B01");
                hide_red_flag_icon();
                unpatch_red_flag_sprite();
                InterlockedExchange(&g_owned_restart_gate_pending, 1);
                *RESTART_FLAG = 1;
                log_w("RF-E0C5AB9682A");
                (void)0;   /* Beep removed */
                phase = 2;
                phase_time = GetTickCount();
            } else {
                g_weather_wait_input=FALSE;
                hud_auto_notice_write(0,"RED FLAG: WAITING FOR CARS");
                if(g_rf_policy_now-g_drive_started>=REDFLAG_WAIT_TIMEOUT_MS)
                    rf_key_hint(1,"FOR COUNTDOWN FALLBACK");
                else if(g_player_box_confirmed) {
                    hud_auto_notice_write(0,"RESTART CONFIRMED");
                    hud_auto_notice_write(1,"WAITING FOR REMAINING CARS");
                } else rf_key_hint(1,"WHEN STOPPED IN PITS");
                /* Holding is intentional. Neither wall-time nor a pause may
                   silently return the field to green. */
            }
        } else if (phase == 2) {
            commentary_suppress();

            /* Do not accept the pre-trigger GameState==5 as a completed
             * restart. First observe the game leave either starting state;
             * then wait for the real GRID_READY phase. This naturally leaves
             * the player in the garage/setup interface until RACE is clicked. */
            if (GAME_STATE != restart_start_game_state
                || RACE_PHASE != restart_start_race_phase) {
                restart_transition_seen = TRUE;
            }
            if (restart_transition_seen && RACE_PHASE == 2) {
                restart_grid_seen = TRUE;
            }

            if (restart_transition_seen && restart_grid_seen
                && GAME_STATE == 5 && GetTickCount() - phase_time > 500) {
                if (g_roster_reduced_epoch &&
                    session_epoch() != g_roster_reduced_epoch) {
                    char eb[192];
                    wsprintfA(eb, "RF-E040C9FE37B %08X %08X",
                              (unsigned int)g_roster_reduced_epoch,
                              (unsigned int)session_epoch());
                    log_w(eb);
                    abort_post_constructor_retain_roster(
                        "ordered survivor pointer identity changed before GRID_READY");
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                }
                if (g_player_out_active) {
#if RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE
                    LONG tuple_result = InterlockedCompareExchange(
                        &g_player_tuple_result, 0, 0);
                    if (tuple_result == 0) {
                        if (RACE_PHASE != 2) {
                            InterlockedExchange(&g_player_tuple_result, -4);
                            tuple_result = -4;
                        } else if (InterlockedCompareExchange(
                                       &g_player_tuple_pending, 1, 0) == 0) {
                            int tuple_post = (!g_broker_registered ||
                                              !g_broker_post ||
                                              !g_broker_control_token)
                                ? -1
                                : g_broker_post(g_broker_control_token,
                                    broker_player_tuple_restore_task, NULL);
                            if (tuple_post != RF_BROKER_OK) {
                                InterlockedExchange(&g_player_tuple_pending, 0);
                                InterlockedExchange(&g_player_tuple_result, -5);
                                tuple_result = -5;
                            }
                        }
                        if (tuple_result == 0) {
                            Sleep(1);
                            continue;
                        }
                    }
                    if (tuple_result < 0 || !player_out_restored_tuple_matches()) {
                        char tb[224];
                        wsprintfA(tb, "RF-EF228785BA9 %d",
                                  (int)tuple_result);
                        log_w(tb);
                        abort_post_constructor_retain_roster(
                            "terminal player tuple could not be restored on the GRID_READY game-thread edge");
                        phase = 0;
                        seq_epoch = 0;
                        continue;
                    }
#if RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE
                    {
                        char gb[240];
                        wsprintfA(gb, "RF-E4B8AFC0D88 %d %d %d %d %d",
                                  g_player_native_garage_pit,
                                  g_player_native_garage_ordinal,
                                  (int)g_player_native_garage_pos[0],
                                  (int)g_player_native_garage_pos[1],
                                  (int)g_player_native_garage_pos[2]);
                        log_w(gb);
                    }
#else
                    log_w("RF-E2E7B6460F8");
#endif
#else
                    if (!player_out_native_state_matches()) {
                    char pb[512];
                    char* pv = g_player_out_ptr;
                    BOOL pv_readable = pv &&
                        runtime_region_supported((uintptr_t)pv, VEH_STRIDE, FALSE);
                    wsprintfA(pb, "RF-EA5F8FBF3B3 %d %d %d %d %d %d",
                              pv_readable ? *(volatile int*)(pv + OFF_FINISHSTATE) : -1,
                              pv_readable ? *(volatile int*)(pv + OFF_REASONCODE) : -1,
                              pv_readable ? *(volatile BYTE*)(pv + OFF_RETIREDFLAG) : -1,
                              pv_readable ? *(volatile BYTE*)(pv + OFF_ACTIVEFLAG) : -1,
                              pv_readable ? *(volatile int*)(pv + OFF_PITSTATE) : -1,
                              pv_readable ? *(volatile int*)(pv + OFF_SLOTLIVE) : -1);
                    log_w(pb);
                    abort_post_constructor_retain_roster(
                        "native restart moved or revived the player before GRID_READY");
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                    }
#endif
                }
                log_w("RF-E14CE8BD382"
                      "Restoring fuel; survivor-only roster retained and green-edge restoration armed.");
                if (g_player_out_active) {
                    char pb[512];
                    char* pv = g_player_out_ptr;
                    BOOL pv_readable = pv &&
                        runtime_region_supported((uintptr_t)pv, VEH_STRIDE, FALSE);
#if RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE
                    wsprintfA(pb, "RF-EC5CE6118C9 %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
#else
                    wsprintfA(pb, "RF-EA46426011C %d %d %d %d %d %d %d %d %d %d %d %d %d",
#endif
                              player_out_restored_tuple_matches(),
                              pv_readable ? *(volatile int*)(pv + OFF_PITSTATE) : -1,
                              pv_readable ? *(volatile int*)(pv + OFF_DRIVERSTATUS) : -1,
                              pv_readable ? *(volatile int*)(pv + OFF_GARAGEFLAG_A) : -1,
                              pv_readable ? *(volatile int*)(pv + OFF_GARAGEFLAG_B) : -1,
                              PLAYER_OUT_FLAG,
                              pv_readable ? (int)*(volatile float*)(pv + OFF_POSX) : -1,
                              pv_readable ? (int)*(volatile float*)(pv + OFF_POSY) : -1,
                              pv_readable ? (int)*(volatile float*)(pv + OFF_POSZ) : -1,
                              g_player_out_garage_valid,
                              (int)g_player_out_px, (int)g_player_out_py,
                              (int)g_player_out_pz
#if RF_DIAGNOSTIC_PLAYER_NATIVE_GARAGE
                              , (int)InterlockedCompareExchange(
                                  &g_player_native_garage_result, 0, 0)
#endif
                              );
                    log_w(pb);
                }
                restore_fuel();
                restore_driver_visibility();
                {
                    int grid_laps = restore_individual_lap_continuity();
                    char lb[176];
                    if (grid_laps >= 0) {
                        wsprintfA(lb, "RF-EBCC7AED94E %d %d",
                                  g_saved_total_laps, grid_laps);
                    } else {
                        wsprintfA(lb, "RF-E471819DB2D");
                    }
                    log_w(lb);
                }
                /* Always use the proven game-thread green edge. Even when no
                 * DNF roster needs restoring, Game_Restart reset every
                 * survivor's lap counter and the original race length must be
                 * published atomically with those per-driver lap values. */
                InterlockedExchange(&g_green_restore_result, 0);
                InterlockedExchange(&g_grid_fuel_restored, 0);
                InterlockedExchange(&g_green_restore_armed, 1);
                InterlockedExchange(&g_broker_green_task_pending, 0);
                InterlockedExchange(&g_post_green_continuity_armed, 0);
                InterlockedExchange(&g_post_green_continuity_pending, 0);
                InterlockedExchange(&g_post_green_continuity_result, 0);
                (void)0;   /* Beep removed */
                phase = 3;
                phase_time = GetTickCount();
                restart_live_since = 0;
            }
        } else if (phase == 3) {
            /* Keep every excluded DNF physics-dead and out of the rendered
             * circuit until the complete results roster is safely published.
             * This is especially important for wheel-less chassis for which
             * native garage removal may never complete. */
            re_retire_dnf();
            hide_dnf_cars_quiet(TRUE);
            /* v38's live-proven timing: keep exactly one callback queued from
             * GRID_READY onward. F1C invokes the broker at the first live frame
             * but may not invoke it again afterward. Waiting until the worker
             * observes RacePhase 5 therefore misses the only usable edge and
             * leaves the reduced survivor count permanent (v48-v52 code -4).
             * A callback consumed before phase 5 clears pending and is posted
             * again on the next worker tick. */
            if (InterlockedCompareExchange(&g_green_restore_armed, 0, 0) == 1 &&
                InterlockedCompareExchange(&g_broker_green_task_pending, 1, 0) == 0) {
                int green_post_result = (!g_broker_registered || !g_broker_post)
                    ? -1
                    : g_broker_post(g_broker_control_token,
                                    broker_green_restore_task, NULL);
                if (green_post_result != RF_BROKER_OK) {
                    InterlockedExchange(&g_green_restore_last_post_result,
                                        green_post_result);
                    InterlockedIncrement(&g_green_restore_post_failures);
                    InterlockedExchange(&g_broker_green_task_pending, 0);
                } else {
                    InterlockedIncrement(&g_green_restore_posts);
                }
            }
            {
                LONG green_result_now = InterlockedCompareExchange(
                    &g_green_restore_result, 0, 0);
                if ((green_result_now > 0 || green_result_now == -6) &&
                    InterlockedCompareExchange(
                        &g_post_green_continuity_armed, 0, 0) == 1 &&
                    InterlockedCompareExchange(
                        &g_post_green_continuity_pending, 1, 0) == 0) {
                    int continuity_post = (!g_broker_registered || !g_broker_post)
                        ? -1
                        : g_broker_post(g_broker_control_token,
                            broker_post_green_continuity_task, NULL);
                    if (continuity_post != RF_BROKER_OK)
                        InterlockedExchange(
                            &g_post_green_continuity_pending, 0);
                }
            }
            if (RACE_PHASE == 5) {
                if (!restart_live_since) {
                    char gb[176];
                    restart_live_since = GetTickCount();
                    wsprintfA(gb, "RF-E0891B44516 %ld %ld %ld %ld %ld %ld",
                              InterlockedCompareExchange(&g_green_restore_posts, 0, 0),
                              InterlockedCompareExchange(&g_green_restore_callbacks, 0, 0),
                              InterlockedCompareExchange(&g_green_restore_post_failures, 0, 0),
                              InterlockedCompareExchange(&g_green_restore_last_post_result, 0, 0),
                              InterlockedCompareExchange(&g_broker_green_task_pending, 0, 0),
                              InterlockedCompareExchange(&g_green_restore_result, 0, 0));
                    log_w(gb);
                }
            } else {
                restart_live_since = 0;
            }
            if (restart_grid_seen && restart_live_since
                && GetTickCount() - restart_live_since >= GREEN_RESTORE_STABLE_MS) {
                LONG green_result =
                    InterlockedCompareExchange(&g_green_restore_result, 0, 0);
                LONG continuity_result = InterlockedCompareExchange(
                    &g_post_green_continuity_result, 0, 0);
                DWORD green_elapsed = GetTickCount() - restart_live_since;
                if ((green_result > 0 || green_result == -6) &&
                    continuity_result == 0) {
                    if (green_elapsed < GREEN_RESTORE_TIMEOUT_MS)
                        continue;
                    if (InterlockedCompareExchange(
                            &g_post_green_continuity_armed, 0, 1) == 1) {
                        InterlockedExchange(
                            &g_post_green_continuity_result, -4);
                        continuity_result = -4;
                    } else {
                        continue;
                    }
                }
                if (continuity_result < 0) {
                    char cb[176];
                    wsprintfA(cb,
                        "RF-EB8A4DFE241 %d",
                        (int)continuity_result);
                    abort_post_constructor_retain_roster(cb);
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                }
                if (green_result != 0 ||
                    green_elapsed >= GREEN_RESTORE_TIMEOUT_MS) {
                  if (green_result == 0) {
                    char gb[176];
                    wsprintfA(gb, "RF-E0F830E466B %ld %ld %ld %ld %ld",
                              InterlockedCompareExchange(&g_green_restore_posts, 0, 0),
                              InterlockedCompareExchange(&g_green_restore_callbacks, 0, 0),
                              InterlockedCompareExchange(&g_green_restore_post_failures, 0, 0),
                              InterlockedCompareExchange(&g_green_restore_last_post_result, 0, 0),
                              InterlockedCompareExchange(&g_broker_green_task_pending, 0, 0));
                    log_w(gb);
                    /* Cancel only a queued, unclaimed callback. State 2 means
                     * the game thread owns the transaction; wait for its
                     * result and never abort/remove guards underneath it. */
                    LONG green_owner = InterlockedCompareExchange(
                        &g_green_restore_armed, 0, 1);
                    if (green_owner == 1) {
                        InterlockedExchange(&g_green_restore_result, -4);
                        green_result = -4;
                    } else {
                        continue;
                    }
                  }
                  if (green_result < 0 && green_result != -6) {
                    char hb[160];
                    wsprintfA(hb, "RF-EF979D96F45 %d",
                              (int)green_result);
                    abort_post_constructor_retain_roster(
                        hb);
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                  }
#if RF_DIAGNOSTIC_PLAYER_TUPLE_RESTORE
                  if (g_player_out_active &&
                      (session_epoch() != g_player_tuple_epoch ||
                       !player_out_restored_tuple_matches() ||
                       !player_tuple_order_matches(NULL))) {
                    abort_post_constructor_retain_roster(
                        "terminal player tuple/order changed at green before grid-guard removal");
                    phase = 0;
                    seq_epoch = 0;
                    continue;
                  }
#endif
                  if (green_result == 1) {
                    char rb[208];
                    wsprintfA(rb, "RF-ED73192AF12 %d %d %d",
                              VEHICLE_COUNT, g_roster_count, g_saved_total_laps);
                    log_w(rb);
                  } else if (green_result == 2) {
                    char rb[176];
                    wsprintfA(rb, "RF-E2AC10A1311 %d %d",
                              g_saved_total_laps, g_active_count);
                    log_w(rb);
                  } else if (green_result == -6) {
                    char rb[208];
                    wsprintfA(rb, "RF-E55840DD089 %d %d %d",
                              VEHICLE_COUNT, g_saved_total_laps, g_active_count);
                    log_w(rb);
                  } else if (green_result < 0) {
                    char rb[176];
                    wsprintfA(rb, "RF-EE131081C1F %d",
                              (int)green_result);
                    log_w(rb);
                  }
                  log_w("RF-EDE51D8D400");
                  memset(&g_weather_restart,0,sizeof(g_weather_restart));g_weather_wait_input=FALSE;
                  if (g_player_out_active) {
                    char pb[512];
                    char* pv = g_player_out_ptr;
                    BOOL pv_readable = pv &&
                        runtime_region_supported((uintptr_t)pv, VEH_STRIDE, FALSE);
                    wsprintfA(pb, "RF-E35AEBB32CB %d %d %d %d %d %d",
                              player_out_restored_tuple_matches(),
                              PLAYER_OUT_FLAG,
                              pv_readable ? *(volatile int*)(pv + OFF_PITSTATE) : -1,
                              pv_readable ? (int)*(volatile float*)(pv + OFF_POSX) : -1,
                              pv_readable ? (int)*(volatile float*)(pv + OFF_POSY) : -1,
                              pv_readable ? (int)*(volatile float*)(pv + OFF_POSZ) : -1);
                    log_w(pb);
                  }
                  remove_hook();
#if RF_DIAGNOSTIC_PLAYER_GARAGE_RETIRE_LOCK
                  if (g_player_terminal_garage_lock_active &&
                      g_player_out_active && g_player_out_ptr &&
                      (uintptr_t)g_player_out_ptr == PLAYER_VEH_PTR &&
                      InterlockedCompareExchange(
                          &g_player_native_garage_result, 0, 0) == 1) {
                      log_w("RF-E762F9ACC82");
                  } else {
                      g_player_terminal_garage_lock_active = FALSE;
                      remove_hook2();
                      log_w("RF-E8A77956FC1");
                  }
#else
                  remove_hook2();
#endif
                  remove_player_grid_gate();
                  remove_grid_camera_hook();
                  camera_action_restore();
                  hide_red_flag_icon();
                  commentary_restore();
                  DWORD now = GetTickCount();
                  g_session_epoch_id = session_epoch();
                  g_post_restart_normalize_until = now + POST_RESTART_NORMALIZE_MS;
                  ZeroMemory(g_post_restart_pit_fixed,
                             (size_t)g_session_capacity *
                             sizeof(*g_post_restart_pit_fixed));
                  detect_seed();
                  log_w(green_result == 1
                    ? "Live RacePhase=5 stable for 1s: full results roster and per-driver lap continuity active; post-restart normalization armed."
                    : (green_result == 2
                        ? "Live RacePhase=5 stable for 1s: per-driver lap continuity active; no DNF roster restoration was required."
                        : (green_result == -6
                            ? "Live RacePhase=5 stable for 1s: intentional diagnostic survivor-only roster with fuel/lap continuity active."
                            : "Live RacePhase=5 stable for 1s: green restoration unavailable; safe survivor-only fallback retained.")));
                  /* Cooldown is a post-sequence re-arm delay. Re-arming it here
                   * covers NUMPAD2 procedures that can outlast the trigger-time
                   * timer and prevents restart transients from immediately
                   * qualifying after any manual or automatic sequence. */
                  g_auto_cooldown_until = GetTickCount() +
                                          g_rf_config.cooldown_ms;
                  /* A later vanilla Y restart must repeat the DNF quarantine
                   * before its grid constructor sees the restored results
                   * roster. No-DNF and failed-restore sequences stay vanilla. */
                  BOOL checkpoint_ready = (green_result == 1 || green_result == 2) &&
                      (g_checkpoint_replay ? g_checkpoint_valid : checkpoint_capture());
                  if (!checkpoint_ready && (green_result == 1 || green_result == 2))
                      log_w("RF-E12E910DBA8");
                  InterlockedExchange(&g_y_restart_guard_armed,
                      (g_rf_config.y_restart_mode == RF_Y_RESTART_POST_RED_FLAG &&
                       (green_result == 1 || green_result == 2)) ? 1 : 0);
                  g_checkpoint_replay=FALSE;
                  phase = 0;
                }
            } else {
                commentary_suppress();
            }
        }

        /* Hot-swap: the worker ejects the module itself. Polled here rather
         * than at the top of the loop so a red-flag sequence in progress gets
         * torn down through the same path as a normal one. */
        if (g_unload_ev && WaitForSingleObject(g_unload_ev, 0) == WAIT_OBJECT_0) {
            log_w("RF-E4599632DBD");
            g_running = FALSE;
            break;
        }

        if(phase==0 && !g_rf_config.automatic_enabled && !rf_waiting_fire) {
            auto_notice_reset();g_auto_candidate_since=0;
        }
        if(phase==0 && !rf_waiting_fire && !g_np_active && !g_pos_lock_active &&
           !g_post_constructor_hold &&
           !InterlockedCompareExchange(&g_full_y_reset_pending,0,0) &&
           !InterlockedCompareExchange(&g_y_restart_prepare_state,0,0) &&
           (!g_post_restart_normalize_until || GetTickCount()>g_post_restart_normalize_until) &&
           !InterlockedCompareExchange(&g_broker_green_task_pending,0,0)) {
            if(!g_manual_countdown && g_auto_notice_reason==0) {
                g_announcement_snapshot=FALSE;
                (void)rf_penalty_end();
            }
            if(f1c_rc_read(&g_rc->owner)==F1C_RC_RF)f1c_rc_rf_release(g_rc);
            f1c_rc_request(g_rc,(g_manual_prompt || g_manual_countdown ||
                g_auto_notice_reason!=0 || g_auto_candidate_since!=0)?1:0);
        }
        status_gui_update(phase);
        Sleep(phase == 0 ? 50 : 1);
    }

#undef RESET_SEQUENCE_LOCALS

    eject_teardown();
    g_session_active = FALSE;
    g_session_epoch_id = 0;
    session_state_reset_scalars();
    session_arrays_release();

    if (g_unload_ev) { CloseHandle(g_unload_ev); g_unload_ev = NULL; }

    /* LightLoader retains plug-in module handles for the process lifetime.
     * In that ownership mode the worker exits without altering the loader's
     * reference count. The legacy injection path keeps atomic self-eject. */
    if (g_lightloader_owned || !g_hud_patch_unload_safe) {
        if (g_timer_period_active) {
            timeEndPeriod(1);
            g_timer_period_active = FALSE;
        }
        return 0;
    }
    FreeLibraryAndExitThread(g_hinst, 0);
    return 0;   /* not reached */
}

static __attribute__((always_inline)) inline BOOL rf_validate_loader_host(const RfLoaderHostV2 *host)
{
    char proof_name[] = {203,203,209,248,145,198,149,151,196,147,194,144,167}; volatile unsigned proof_name_key = 167;
    for (unsigned i=0; i<sizeof(proof_name)/sizeof(proof_name[0]); ++i) proof_name[i] ^= proof_name_key;
    wchar_t expected_name[] = {20935,20938,20941,20947,20950,20951,20891,20877,20935,20943,20943,20899}; volatile unsigned expected_name_key = 20899;
    for (unsigned i=0; i<sizeof(expected_name)/sizeof(expected_name[0]); ++i) expected_name[i] ^= expected_name_key;
    wchar_t path[MAX_PATH];
    wchar_t *base;
    RfLoaderProofProc proof_proc;

    if (!host || host->size < sizeof(*host) ||
        host->magic != RFP_ABI_MAGIC ||
        host->process_id != GetCurrentProcessId() || !host->loader_module ||
        host->loader_module != GetModuleHandleW(expected_name) ||
        !rf_loader_exclusivity_matches(host) ||
        host->seal != rf_host_seal(host)) {
        return FALSE;
    }
    if (!GetModuleFileNameW(host->loader_module, path, ARRAYSIZE(path))) {
        return FALSE;
    }
    path[ARRAYSIZE(path) - 1] = L'\0';
    base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    if (_wcsicmp(base, expected_name) != 0) return FALSE;
    proof_proc = (RfLoaderProofProc)GetProcAddress(host->loader_module, proof_name);
    if (!proof_proc) return FALSE;
    return proof_proc(rf_loader_proof_challenge(host)) == host->proof;
}

static __attribute__((noinline, optimize("O0"))) BOOL rf_secure_loader_start(const RfLoaderHostV2 *host)
{
    BOOL rf_protected_result = FALSE;
    RF_HARD_BEGIN("rf_secure_loader_start");

    HANDLE worker;
    if (InterlockedCompareExchange(&g_loader_init_state, 1, 0) != 0) {
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
    if (!rf_validate_loader_host(host)) {
        InterlockedExchange(&g_loader_init_state, 0);
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
#ifdef RF_HOST_GATE_TEST
    InterlockedExchange(&g_loader_init_state, 0);
    { rf_protected_result = (TRUE); goto rf_protected_exit; }
#endif
    /* Reject a non-game host before a worker exists. This keeps loader smoke
     * harnesses and accidental copies inert and avoids racing module teardown
     * against LightLoader's initialization call. */
    if (!runtime_layout_supported()) {
        InterlockedExchange(&g_loader_init_state, 0);
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }

    rf_set_log_path();
    rf_log_begin();
    rf_load_activation_config();
    rf_log_effective_config();
    if (!mon_shared_open()) {
        log_w("RF-EE69500D35B");
        InterlockedExchange(&g_loader_init_state, 0);
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
    log_w("RF-E12B3428D48");
    log_w("RF-EE134C7647A");
#if RF_DIAGNOSTIC_KEEP_SURVIVOR_ROSTER
    log_w("RF-E51CC774E9A");
#endif
#if RF_DIAGNOSTIC_OBSERVE_NATIVE_PLAYER_PLACEMENT
    log_w("RF-E52FA36AAB5");
#endif
#if RF_DIAGNOSTIC_HUD_ORIGINAL_DENOMINATOR
    log_w("RF-E3F3BA87784");
#endif
    g_lightloader_owned = TRUE;
    g_running = TRUE;
#if AUTO_REDFLAG_ENABLED
    { volatile char auto_mode = RF_AUTOMODE_SIGNATURE[0]; (void)auto_mode; }
#endif
    timeBeginPeriod(1);
    g_timer_period_active = TRUE;
    g_unload_ev = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT_NAME);
    worker = CreateThread(NULL, 0, thread_main, NULL, 0, NULL);
    if (!worker) {
        rf_broker_unbind();
        mon_shared_close();
        if (g_unload_ev) { CloseHandle(g_unload_ev); g_unload_ev = NULL; }
        timeEndPeriod(1);
        g_timer_period_active = FALSE;
        InterlockedExchange(&g_loader_init_state, 0);
        { rf_protected_result = (FALSE); goto rf_protected_exit; }
    }
    CloseHandle(worker);
    InterlockedExchange(&g_loader_init_state, 2);
    { rf_protected_result = (TRUE); goto rf_protected_exit; }

rf_protected_exit:
    RF_MARK_END();
    return rf_protected_result;
}

__declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(const RfLoaderHostV2 *host) { return rf_secure_loader_start(host); }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hinst = h;
        DisableThreadLibraryCalls(h);
        { volatile char _rfk = RF_SIGNATURE[0]; (void)_rfk; }
#ifndef RF_RELEASE
        { volatile char debug_profile = RF_DEBUG_HOTKEY_SIGNATURE[0];
          (void)debug_profile; }
#endif
    } else if (reason == DLL_PROCESS_DETACH) {
        /* Never wait or run complex teardown while holding the loader lock.
         * Cooperative self-eject performs eject_teardown() on the worker
         * before FreeLibraryAndExitThread; process exit needs no manual frees. */
        g_running = FALSE;
        /* 1.1.6: detach the crash VEH only. NEVER flush here: at process exit
         * the loader lock is held and other threads are already dead, so a
         * held f1c_usage spin lock could hang the game. The summary is written
         * from the broker session-end callback and the worker unload path. */
        rfu_veh_remove();
        if (g_timer_period_active) {
            timeEndPeriod(1);
            g_timer_period_active = FALSE;
        }
    }
    return TRUE;
}
