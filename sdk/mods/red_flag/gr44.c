/*
 * gr44.c - Red flag mod for F1 Challenge 99-02
 *
 * THIS FILE IS THE SOURCE. redflag.c is GENERATED from it by make_release.py
 * (which compiles the logging out); editing redflag.c loses your work on the
 * next regen. Build:  python make_release.py && bash build_release.sh redflag.dll
 * Hot-swap without restarting the game:
 *     python ../extended_vanilla_features/tools/dll_unload_signal.py redflag
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
 *   (4) MULTIPLAYER LOCKOUT, fail-open.
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
* Fork of gr43.c. HARDENING: (1) COUNTDOWN_SEC restored to 30 (was 10 for
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
#include <stdio.h>
#include <stdint.h>
#include <mmsystem.h>

#define HOTKEY          VK_NUMPAD1   /* timer-based restart (30s countdown)     */
#define HOTKEY_REDFLAG  VK_NUMPAD2   /* full red flag: AI to pits, restart when
                                        the PLAYER reaches his own pit box      */
#define HOTKEY_PITRET   VK_NUMPAD3   /* pit-retire every AI car (player exempt) */

/* Safety valve for NUMPAD2: if the player never pits, abort and put the field
 * back rather than leaving the race in limbo. */
#define REDFLAG_WAIT_TIMEOUT_MS  (5 * 60 * 1000)

/* Hard ceiling on the post-restart phases (2 and 3). See the SESSION WATCHDOG
 * block: their own exit conditions are reads of game state that a session
 * change can freeze, and a stuck phase 3 re-arms the flag icon for every later
 * race. Nothing legitimate in those phases takes two minutes. */
#define PHASE_MAX_MS             (2 * 60 * 1000)
#define VEHICLE_COUNT   (*(volatile int*)0x005C329C)
#define VEHICLE_ARRAY   ((volatile void**)0x007C54F0)
#define PLAYER_VEH_PTR  (*(volatile uintptr_t*)0x007C54E8)
#define SESSION_TYPE    (*(volatile int*)0x007C56A4)
#define GAME_STATE      (*(volatile int*)0x007C569C)
#define RESTART_FLAG    ((volatile char*)0x007CE25D)
#define PAUSE_FLAG      (*(volatile BYTE*)0x007C56AC)
#define RACE_PHASE      (*(volatile int*)0x007C56A8)

#define CONTROLLER      0x005EA618
#define EVENT_TABLE     0x005BF9A0
#define COMMENTARY_ENABLED ((volatile BYTE*)(CONTROLLER + 0xAF0))

static BYTE g_commentary_saved = 1;
static BOOL g_commentary_suppressed = FALSE;

#define OFF_QUALPOS     0x4058
#define OFF_RACEPOS     0x4060
#define OFF_FINISHSTATE 0x4064
#define OFF_REASONCODE  0x4068
#define OFF_PITSTATE    0x4040
#define OFF_RETIREDFLAG 0x4410
#define OFF_ACTIVEFLAG  0x45F4
#define OFF_DRIVERNAME  0x4620   /* ASCIIZ, e.g. "Mika Hakkinen (2001)" */

/* --- pit-retire trigger (CONFIRMED 2026-07-24, name_index.json) ---
 * 0xD18/0xD58 = rear-wing damage record (ptr + intact flag, delta 0x40).
 * 0xCD4/0xD14 = front-wing record; deliberately NOT used — the front record is
 * not in PitStopBlockingCondition, so arming it yields a REPAIR, not a retire.
 * The flag holds when poked (it is not recomputed per frame) and the game
 * resets it to 1 once the retirement completes. */
#define OFF_REARWING_PTR  0xD18
#define OFF_REARWING_FLAG 0xD58
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
#define VEH_SLOTS       104
#define OFF_SLOTLIVE    0x32C8u   /* int32, -1 = dead slot, 0 = player, 1 = AI  */
#define OFF_LAPDIST     0x4044u   /* float, metres along the lap [FACT]         */
#define OFF_SPEED       0x4440u   /* float, actual speed         [FACT]         */
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
#define TOTAL_LAPS_PTR  (*(volatile int*)0x007D05A0)

#define MAX_CARS        26
#define RETIRE_SECS     3
#define COUNTDOWN_SEC   30   /* Normal play. Was temporarily 10 during the
                              * gr32-gr43 debugging; set to 10 again if you need
                              * fast iteration. */

#define CALL_SITE           0x0042C409
#define CALL_SIZE           5
#define SCENE_RENDER_UPDATE 0x004C6D90
#define TICK_CALL_SITE      0x0042D1E1
#define SESSION_TICK_UPDATE 0x004C6B60

#define HUD_OUTER_PTR   0x007cd204
#define SLOT_COUNT      8
#define SLOT_STRIDE     0x5c
#define SLOT_TEXT_OFF   0x3c
#define SLOT_TIMER_OFF  0x90

/* Red flag sprite patch — FUN_004D82D0 VehicleFlagCheck branch.
 * Byte at 0x4D8338 is the immediate in: mov [esp+esi+0Ch], 41h
 * Changing 0x41 (blue FLAG sprite) to 0x40 (red FLAG sprite) in FTingame01.BMP.
 * Trigger: write veh+0x4108 = 1 (VehicleFlagCheck reads DAT_7C54E0+0x4108). */
#define SPRITE_PATCH_SITE   0x4D8338
#define SPRITE_PATCH_SITE2  0x4DBAE3
#define SPRITE_PATCH_OLD    0x41
#define SPRITE_PATCH_NEW    0x40
#define VEH_OBJ_PTR_GLOBAL  0x007C54E0
#define OFF_VEHICLE_FLAG    0x4108

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
static HANDLE    g_unload_ev = NULL;

/* Slot index of a vehicle pointer in VEHICLE_ARRAY, or -1. Logging only. */
static int veh_slot(char* veh) {
    int count = VEHICLE_COUNT;
    for (int i = 0; i < count && i < MAX_CARS; i++)
        if ((char*)VEHICLE_ARRAY[i] == veh) return i;
    return -1;
}

static const char* veh_name(char* veh) {
    return veh ? (const char*)(veh + OFF_DRIVERNAME) : "?";
}

/* --- Auto red-flag detection: terminal-damage cluster (>=3 cars in 8s) ---
 * "Terminal damage" = ActiveFlag (veh+0x45F4, float) drops nonzero -> 0.0,
 * the game's visual kill switch (~4-5s after a crash). This is much earlier
 * and more correct than FinishState/DNF (the late pit-classification). */
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
 * left to corrupt races. Re-enabling needs a detector that ignores restart
 * transients — e.g. gate on GAME_STATE/RACE_PHASE being settled racing, require
 * the car to stay killed for N consecutive polls, and arm a cooldown on MANUAL
 * triggers too (only check_auto_redflag() sets one today, which is why NUMPAD1
 * did not protect itself).
 *
 * Set to 1 to restore the old behaviour. */
#define AUTO_REDFLAG_ENABLED 0

#define AUTO_TERM_THRESHOLD 3
#define AUTO_TERM_WINDOW_MS 8000
#define AUTO_COOLDOWN_MS    120000   /* after a fire, don't re-arm for 2 min */

static BOOL  g_last_killed[MAX_CARS];   /* per-slot: was ActiveFlag ~0 last poll */
static BOOL  g_det_seeded = FALSE;
static DWORD g_term_times[MAX_CARS];
static int   g_term_times_n = 0;
static DWORD g_auto_cooldown_until = 0;

/* QualPos/lap a LATE-swept car had before we stamped it 200/0, so an
 * unconfirmed sweep can be undone. See revalidate_dnf(). */
static int    g_late_saved_qp[MAX_CARS];
static int    g_late_saved_lap[MAX_CARS];
static void*  g_late_ptrs[MAX_CARS];
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

static void*  g_kill_ptrs[MAX_CARS];
static DWORD  g_kill_since[MAX_CARS];
static int    g_kill_count = 0;

/* How long this car has read act==0 continuously, in ms. 0 = not killed. */
static DWORD kill_duration(char* veh) {
    DWORD now = GetTickCount();
    BOOL killed = veh_killed(veh);
    for (int i = 0; i < g_kill_count; i++) {
        if (g_kill_ptrs[i] != (void*)veh) continue;
        if (!killed) { g_kill_since[i] = 0; return 0; }
        if (g_kill_since[i] == 0) { g_kill_since[i] = now; return 0; }
        return now - g_kill_since[i];
    }
    if (g_kill_count < MAX_CARS) {
        g_kill_ptrs[g_kill_count]  = veh;
        g_kill_since[g_kill_count] = killed ? now : 0;
        g_kill_count++;
    }
    return 0;
}

static void kill_tracker_reset(void) { g_kill_count = 0; }

static void*  g_dnf_ptrs[MAX_CARS];
static int    g_dnf_count = 0;
static BOOL   g_dnf_hidden = FALSE;

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
static void*  g_hidden_ptrs[MAX_CARS];
static float  g_hidden_node_y[MAX_CARS];
static float  g_hidden_phys_y[MAX_CARS];
static int    g_hidden_count = 0;

static char*  g_active[MAX_CARS];
static int    g_active_rp[MAX_CARS];
static int    g_active_count = 0;
static int    g_placed = 0;

/* ---- no-overtake pace hold state, one entry per SLOT (not per g_active) --
 * Slot-indexed, because the law needs the whole field including cars this mod
 * has already classified as DNF: a wreck still has to be seen as an obstacle
 * even though it is not governed. */
static float  g_np_saved[VEH_SLOTS];    /* pre-hold maxSpeed                  */
static float  g_np_pass[VEH_SLOTS];     /* pre-hold PassingNorm               */
static float  g_np_aggro[VEH_SLOTS];    /* pre-hold TrackAggression           */
static float  g_np_lastw[VEH_SLOTS];    /* what WE last wrote (override test) */
static char   g_np_armed[VEH_SLOTS];    /* 1 = this slot is ours              */
static float  g_np_hist[VEH_SLOTS][SPD_HIST];
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
static int    g_locked_racepos[MAX_CARS];
static BOOL   g_pos_lock_active = FALSE;

/* Saved fuel levels per active car (including player) */
static float  g_saved_fuel[MAX_CARS];

/* Saved current laps and total laps for lap adjustment */
static int    g_saved_currentlap[MAX_CARS];
static int    g_saved_total_laps = 0;
static int    g_max_driven_laps = 0;

static BYTE*  g_trampoline = NULL;
static BYTE   g_orig_bytes[CALL_SIZE];
static BOOL   g_hook_installed = FALSE;

static BYTE*  g_trampoline2 = NULL;
static BYTE   g_orig_bytes2[CALL_SIZE];
static BOOL   g_hook2_installed = FALSE;

static void log_w(const char* s) {
    FILE* f = fopen(g_log, "a");
    if (f) { fprintf(f, "[%lu] %s\n", GetTickCount(), s); fclose(f); }
}

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
        wsprintfA(buf, "PWS_RED01 (eid %d): gave up after %ums (sent=%d played=%d)",
                  g_say_eid, SAY_MAX_WAIT_MS, (int)g_say_sent, (int)g_say_played);
        log_w(buf);
        g_say_eid = -1;
        return;
    }

    if (!g_say_played && EV_TS(g_say_eid) > 0.0f) {
        g_say_played    = TRUE;
        g_say_played_ms = now;
        char buf[96];
        wsprintfA(buf, "PWS_RED01 (eid %d) PLAYED (EVENT_TS=%d ms after request)",
                  g_say_eid, (int)(now - g_say_since));
        log_w(buf);
        return;
    }
    if (g_say_played) return;

    /* Only ever fire into a gap. A line queued while another is playing comes
     * out as that other line's clip, not ours. */
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
            wsprintfA(lb, "PWS_RED01 (eid %d) inserted into a free channel", g_say_eid);
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
    g_trampoline2 = (BYTE*)VirtualAlloc(NULL, 32,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampoline2) return FALSE;

    BYTE* p = g_trampoline2;
    *p++ = 0x80; *p++ = 0xB9;
    *(DWORD*)p = OFF_RETIREDFLAG; p += 4;
    *p++ = 0x00;
    *p++ = 0x75; *p++ = 0x07;
    *p++ = 0xB8; *(DWORD*)p = SESSION_TICK_UPDATE; p += 4;
    *p++ = 0xFF; *p++ = 0xE0;
    *p++ = 0xC2; *(WORD*)p = 4; p += 2;

    return TRUE;
}

static BOOL install_hook(void) {
    if (g_hook_installed) return TRUE;
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
    DWORD old;
    VirtualProtect((void*)CALL_SITE, CALL_SIZE, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)CALL_SITE, g_orig_bytes, CALL_SIZE);
    VirtualProtect((void*)CALL_SITE, CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)CALL_SITE, CALL_SIZE);
    g_hook_installed = FALSE;
}

static BOOL install_hook2(void) {
    if (g_hook2_installed) return TRUE;
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
    DWORD old;
    VirtualProtect((void*)TICK_CALL_SITE, CALL_SIZE, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)TICK_CALL_SITE, g_orig_bytes2, CALL_SIZE);
    VirtualProtect((void*)TICK_CALL_SITE, CALL_SIZE, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)TICK_CALL_SITE, CALL_SIZE);
    g_hook2_installed = FALSE;
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
static void*  g_pitret_ptrs[MAX_CARS];
static int    g_pitret_count = 0;

/* NUMPAD3: arm the whole field except the player. The game does the rest. */
static void pit_retire_all(void) {
    int count = VEHICLE_COUNT;
    int armed = 0, held = 0;
    g_pitret_count = 0;
    for (int i = 0; i < count && i < MAX_CARS; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (arm_pit_retire(veh)) {
            armed++;
            g_pitret_ptrs[g_pitret_count++] = veh;
            if (*(volatile BYTE*)(veh + OFF_REARWING_FLAG) == 0) held++;
        }
    }
    char buf[128];
    wsprintfA(buf, "PIT-RETIRE: armed %d cars (%d flags held). "
                   "They retire on reaching their own pit box.", armed, held);
    log_w(buf);
    if (armed != held)
        log_w("PIT-RETIRE WARNING: some flags did not hold - "
              "expected only if the game recalculated them.");
}

/* Undo an arming that has not yet fired (car still en route). Once the game has
 * retired the car the write is spent — it resets the flag itself. */
static void pit_retire_revert_all(void) {
    int count = VEHICLE_COUNT;
    int n = 0;
    for (int i = 0; i < count && i < MAX_CARS; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh || (uintptr_t)veh == PLAYER_VEH_PTR) continue;
        if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) continue;
        *(volatile BYTE*)(veh + OFF_REARWING_FLAG) = 1;
        *(volatile int*) (veh + OFF_CALLTOBOX)     = 0;
        n++;
    }
    char buf[64];
    wsprintfA(buf, "PIT-RETIRE: reverted %d cars", n);
    log_w(buf);
}

/* Undo the retirement on exactly the cars WE retired for the red flag, so the
 * restart puts them back on the grid. Called immediately before RESTART_FLAG.
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
        *(volatile BYTE*)(veh + OFF_REARWING_FLAG) = 1;   /* disarm trigger  */
        *(volatile int*) (veh + OFF_CALLTOBOX)     = 0;
        *(volatile int*) (veh + OFF_PITTING)       = 0;
        *(volatile int*) (veh + OFF_FINISHSTATE)   = 0;
        *(volatile BYTE*)(veh + OFF_RETIREDFLAG)   = 0;
        *(volatile BYTE*)(veh + OFF_ACTIVEFLAG)    = 1;
        n++;
    }
    char buf[80];
    wsprintfA(buf, "RED FLAG: un-retired %d cars for the restart grid", n);
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
    static DWORD still_since = 0;
    uintptr_t veh = PLAYER_VEH_PTR;
    if (!veh) { still_since = 0; return FALSE; }

    int   pit   = *(volatile int*)(veh + OFF_PITTING);
    float speed = *(volatile float*)(veh + OFF_SPEED);

    if (pit >= 3) { still_since = 0; return TRUE; }

    if (pit >= 2 && speed < 0.5f) {
        DWORD now = GetTickCount();
        if (!still_since) still_since = now;
        if (now - still_since >= BOX_STILL_MS) {
            still_since = 0;
            log_w("Player stopped in the pit lane for 1.2s - treating that as "
                  "his box (pittingStatus never reached 3).");
            return TRUE;
        }
        return FALSE;
    }

    still_since = 0;
    return FALSE;
}

/* NOTE: this forges the end-state and is LEFT AS-IS deliberately. It re-pins
 * cars that were ALREADY DNF before the red flag, during the restart — they
 * are not candidates for the pit path (they cannot drive to a box). Cars
 * retired via NUMPAD3 are parked in their garages by the game and need
 * neither this nor hide_dnf_cars(). */
static void re_retire_dnf(void) {
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
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

/* quiet=TRUE suppresses the per-car log line (used when re-sinking every tick). */
static void hide_dnf_cars_quiet(BOOL quiet) {
    int nodeTable = *(volatile int*)(SCENE_STRUCT + OFF_NODETABLE);
    int dim       = *(volatile int*)(SCENE_STRUCT + OFF_DIM);
    if (nodeTable == 0 || dim == 0) return;
    char buf[128];
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        int nodeIdx = *(volatile int*)(veh + OFF_NODEIDX);
        int subIdx  = *(volatile int*)(veh + OFF_SUBIDX);
        char* entry = (char*)(nodeTable + 28 * (nodeIdx + subIdx * dim));

        /* Remember what it looked like before we sank it, so the next red flag
         * can hand the car back to the game in a placeable state. Skip if this
         * car is already recorded — never overwrite a good saved Y with -1000. */
        BOOL already = FALSE;
        for (int k = 0; k < g_hidden_count; k++)
            if (g_hidden_ptrs[k] == (void*)veh) { already = TRUE; break; }
        if (!already && g_hidden_count < MAX_CARS) {
            g_hidden_ptrs[g_hidden_count]   = veh;
            g_hidden_node_y[g_hidden_count] = *(volatile float*)(entry + 4);
            g_hidden_phys_y[g_hidden_count] = *(volatile float*)(veh + OFF_PHYS_Y);
            g_hidden_count++;
        }

        *(volatile float*)(entry + 4) = HIDE_Y;
        *(volatile float*)(veh + OFF_PHYS_Y) = HIDE_Y;
        if (!quiet) {
            wsprintfA(buf, "DNF car %d (slot %d %s): scene node Y=-1000, "
                           "physics Y=-1000", i, veh_slot(veh), veh_name(veh));
            log_w(buf);
        }
    }
}

static void hide_dnf_cars(void) { hide_dnf_cars_quiet(FALSE); }

/* Undo every previous hide_dnf_cars(). Must run at the START of a new red-flag
 * sequence, BEFORE the snapshot: the game can only place a car in its pit box
 * if that car is not still sunk to Y=-1000 from the last sequence. The cars get
 * re-hidden in phase 3 as usual. */
static void unhide_all(void) {
    if (g_hidden_count == 0) return;
    int nodeTable = *(volatile int*)(SCENE_STRUCT + OFF_NODETABLE);
    int dim       = *(volatile int*)(SCENE_STRUCT + OFF_DIM);
    char buf[128];
    int n = 0;
    for (int i = 0; i < g_hidden_count; i++) {
        char* veh = (char*)g_hidden_ptrs[i];
        if (!veh) continue;
        *(volatile float*)(veh + OFF_PHYS_Y) = g_hidden_phys_y[i];
        if (nodeTable && dim) {
            int nodeIdx = *(volatile int*)(veh + OFF_NODEIDX);
            int subIdx  = *(volatile int*)(veh + OFF_SUBIDX);
            char* entry = (char*)(nodeTable + 28 * (nodeIdx + subIdx * dim));
            *(volatile float*)(entry + 4) = g_hidden_node_y[i];
        }
        n++;
    }
    g_hidden_count = 0;
    wsprintfA(buf, "Un-hid %d previously sunk DNF cars (restored Y) so the game "
                   "can place them this sequence.", n);
    log_w(buf);
}

static void snapshot_positions(void) {
    unhide_all();
    int count = VEHICLE_COUNT;
    g_late_count   = 0;      /* fresh sequence: no pending late sweeps */
    kill_tracker_reset();
    g_dnf_count    = 0;
    g_active_count = 0;
    g_placed       = 0;

    for (int i = 0; i < count && i < MAX_CARS; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        int  finish  = *(volatile int*)(veh + OFF_FINISHSTATE);
        BYTE retired = *(volatile BYTE*)(veh + OFF_RETIREDFLAG);
        if (finish != 0 || retired != 0) {
            g_dnf_ptrs[g_dnf_count++] = veh;
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
        g_saved_currentlap[i] = *(volatile int*)(g_active[i] + OFF_CURRENTLAP);
        if (g_saved_currentlap[i] > g_max_driven_laps)
            g_max_driven_laps = g_saved_currentlap[i];
    }
    g_saved_total_laps = TOTAL_LAPS_PTR;

    char buf[128];
    wsprintfA(buf, "Snapshot: %d active, %d DNF, maxLap=%d, totalLaps=%d",
              g_active_count, g_dnf_count, g_max_driven_laps, g_saved_total_laps);
    log_w(buf);

    /* Full field at trigger time, so a later LATE DNF can be judged against
     * what the car looked like when the red flag started. */
    for (int i = 0; i < g_active_count; i++) {
        char* veh = g_active[i];
        if (!veh) continue;
        wsprintfA(buf, "  ACTIVE[%d]: slot %d %s act=%d fin=%d ret=%d rp=%d",
                  i, veh_slot(veh), veh_name(veh),
                  *(volatile int*) (veh + OFF_ACTIVEFLAG),
                  *(volatile int*) (veh + OFF_FINISHSTATE),
                  *(volatile BYTE*)(veh + OFF_RETIREDFLAG),
                  g_active_rp[i]);
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
 *     before the restart; sweeping them into g_dnf_ptrs would strand them. */
static BOOL veh_killed(char* veh);   /* defined with the auto-detect block */

static BOOL is_pitret(char* veh) {
    for (int i = 0; i < g_pitret_count; i++)
        if ((char*)g_pitret_ptrs[i] == veh) return TRUE;
    return FALSE;
}

static void sweep_late_dnf(void) {
    uintptr_t player = PLAYER_VEH_PTR;

    /* SANITY GUARD. A detection bug here is catastrophic and silent: gr32 read
     * ActiveFlag as a float, called every healthy car "terminal", and DNF-ed the
     * entire field on every red flag. Count first, and refuse to act if the
     * result is implausible — losing a legitimate late DNF is a far cheaper
     * failure than destroying the race. */
    int would_sweep = 0;
    for (int r = 0; r < g_active_count; r++) {
        char* veh = g_active[r];
        if (!veh || (uintptr_t)veh == player || is_pitret(veh)) continue;
        if (veh_killed(veh)
            || *(volatile int*)(veh + OFF_FINISHSTATE) != 0
            || *(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) would_sweep++;
    }
    if (g_active_count >= 4 && would_sweep > g_active_count / 2) {
        static BOOL warned = FALSE;
        if (!warned) {
            char buf[144];
            wsprintfA(buf, "LATE DNF ABORTED: %d of %d active cars look terminal "
                           "at once - that is a detection fault, not a race "
                           "incident. Sweep disabled for this sequence.",
                      would_sweep, g_active_count);
            log_w(buf);
            warned = TRUE;
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
        if ((uintptr_t)veh != player && !is_pitret(veh)) {
            /* Sustained kill, not a single sample — see KILL_CONFIRM_MS. */
            if (kill_duration(veh) >= KILL_CONFIRM_MS)        terminal = TRUE;
            if (*(volatile int*)(veh + OFF_FINISHSTATE) != 0)  terminal = TRUE;
            if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) terminal = TRUE;
        }

        if (terminal) {
            if (g_dnf_count < MAX_CARS) g_dnf_ptrs[g_dnf_count++] = veh;

            /* gr17_CONFIRMED writes these AT CLASSIFICATION, not later. Doing
             * it immediately closes the window in which the game can assign its
             * own grid slot (the QualPos=198 bug). apply_qualpos() rewrites the
             * same values later; that is harmless and keeps one source of
             * truth for the pre-existing DNFs. */
            /* Remember what we are about to overwrite. ActiveFlag==0 is only a
             * LEAD — a car can read act=0 transiently without ever being
             * retired. revalidate_dnf() undoes this if the game never confirms. */
            if (g_late_count < MAX_CARS) {
                g_late_ptrs[g_late_count]      = veh;
                g_late_saved_qp[g_late_count]  = *(volatile int*)(veh + OFF_QUALPOS);
                g_late_saved_lap[g_late_count] = *(volatile int*)(veh + OFF_CURRENTLAP);
                g_late_count++;
            }
            *(volatile int*)(veh + OFF_QUALPOS)    = 200;
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
            wsprintfA(buf, "LATE DNF: slot %d %s | act=%d fin=%d ret=%d "
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
            w++;
        }
    }
    g_active_count = w;
    g_placed       = w;
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
        for (int r = 0; r < g_dnf_count; r++)
            if (g_dnf_ptrs[r] != (void*)veh) g_dnf_ptrs[w++] = g_dnf_ptrs[r];
        g_dnf_count = w;

        wsprintfA(buf, "REVALIDATE: slot %d %s RECOVERED (act=1, fin=0 ret=0) - false "
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
        wsprintfA(buf, "DNF SETTLE: gave up after %dms with %d car(s) still "
                       "unclassified - they may be gridded. Restarting anyway.",
                  (int)waited_ms, pending);
        log_w(buf);
        return TRUE;
    }
    return FALSE;
}

static void apply_qualpos(void) {
    revalidate_dnf();
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        *(volatile int*)(veh + OFF_QUALPOS) = 200;
        *(volatile int*)(veh + OFF_CURRENTLAP) = 0;
    }
    if (g_max_driven_laps < 1) {
        log_w("QualPos: <1 lap driven, keeping original qualification grid.");
    } else {
        for (int i = 0; i < g_active_count; i++) {
            *(volatile int*)(g_active[i] + OFF_QUALPOS) = i + 1;
        }
    }
    char buf[128];
    wsprintfA(buf, "QualPos applied: %d active, %d DNF, reorder=%s",
              g_placed, g_dnf_count, g_max_driven_laps < 1 ? "NO(qual)" : "YES(race)");
    log_w(buf);

    /* Per-car state at the moment of the restart. If a wreck still ends up on
     * the grid or parked on track, this line says which of the three things is
     * wrong: it was never classified (absent here), the game re-gridded it
     * (QP != 200), or it was never physics-killed (act=1). */
    for (int i = 0; i < g_dnf_count; i++) {
        char* veh = (char*)g_dnf_ptrs[i];
        if (!veh) continue;
        wsprintfA(buf, "  DNF[%d]: slot %d %s QP=%d fin=%d ret=%d act=%d lap=%d",
                  i, veh_slot(veh), veh_name(veh),
                  *(volatile int*) (veh + OFF_QUALPOS),
                  *(volatile int*) (veh + OFF_FINISHSTATE),
                  *(volatile BYTE*)(veh + OFF_RETIREDFLAG),
                  *(volatile int*) (veh + OFF_ACTIVEFLAG),
                  *(volatile int*) (veh + OFF_CURRENTLAP));
        log_w(buf);
    }
}

/* ======================================================================== *
 *  NO-OVERTAKE PACE HOLD
 *  Ported from patches/full_safety_car/dll/pace_probe.c (2026-08-01, the
 *  version the driver accepted: "Seems to be all set now"). The safety car,
 *  wave-around, caution lifecycle and GUI control block are all dropped -
 *  a red flag has no pace car and no restart procedure of its own, it just
 *  needs the field slowed, bunched and unable to pass until the restart.
 * ======================================================================== */

#define VEH_AT(i)     (VEH_BASE + VEH_STRIDE * (DWORD)(i))
#define V_I32(i, off) (*(volatile int32_t*)(VEH_AT(i) + (off)))
#define V_F32(i, off) (*(volatile float*)  (VEH_AT(i) + (off)))

typedef void (__attribute__((thiscall)) *set_maxspeed_fn)(void* veh, int n);
static const set_maxspeed_fn set_maxspeed =
    (set_maxspeed_fn)ADDR_SET_MAXSPEED;

static int np_live(int i)   { return V_I32(i, OFF_SLOTLIVE) != -1; }
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
    if (np_pit_owned(i)) return 0;
    return 1;
}

/* Learn the lap length from the largest lap distance ever seen. Cheap, needs
 * no .aiw parsing, converges as soon as any car approaches the start line.
 * Until it has, gaps are unmeasurable and every car simply holds the flat
 * pace speed - which is safe, not a release. */
static void np_learn_lap(void) {
    for (int i = 0; i < VEH_SLOTS; i++) {
        if (!np_live(i)) continue;
        float d = V_F32(i, OFF_LAPDIST);
        if (d > g_np_lap_len && d < 30000.0f) g_np_lap_len = d;
    }
    if (g_np_lap_len < 500.0f) g_np_lap_len = 0.0f;
}

static void np_hist_pump(void) {
    g_np_hidx = (g_np_hidx + 1) % SPD_HIST;
    for (int i = 0; i < VEH_SLOTS; i++)
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

    for (int j = 0; j < VEH_SLOTS; j++) {
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
    for (int i = 0; i < VEH_SLOTS; i++) {
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

    for (int i = 0; i < VEH_SLOTS; i++) {
        g_np_armed[i] = 0;
        for (int k = 0; k < SPD_HIST; k++)
            g_np_hist[i][k] = np_live(i) ? V_F32(i, OFF_SPEED) : 0.0f;
    }
    g_np_lap_len = 0.0f;
    g_np_leader  = -1;
    np_learn_lap();

    for (int i = 0; i < VEH_SLOTS; i++) {
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

    wsprintfA(buf, "PaceHold engaged on %d AI cars, ramping %d -> %d m/s "
                   "over %d s (lap_len %d m)",
              n, (int)g_np_ramp_from, PACE_TARGET, (int)RAMP_SECONDS,
              (int)g_np_lap_len);
    log_w(buf);
}

static void restore_speed_cap(void) {
    if (!g_np_active) return;
    int n = 0;
    for (int i = 0; i < VEH_SLOTS; i++) {
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
    wsprintfA(buf, "PaceHold released on %d AI cars", n);
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
        wsprintfA(buf, "Fuel restored for %d cars", g_active_count);
        log_w(buf);
        g_fuel_logged = TRUE;
    }
}

static void adjust_laps(void) {
    int remaining = g_saved_total_laps - g_max_driven_laps;
    if (remaining < 1) remaining = 1;
    TOTAL_LAPS_PTR = remaining;
    char buf[128];
    wsprintfA(buf, "Laps adjusted: %d total - %d driven = %d remaining",
              g_saved_total_laps, g_max_driven_laps, remaining);
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
    if (VEHICLE_COUNT < 2)  return;

    np_hist_pump();
    np_learn_lap();
    g_np_leader = np_find_leader();
    g_np_cmd    = np_commanded_speed();

    for (int i = 0; i < VEH_SLOTS; i++) {
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
            wsprintfA(buf, "PaceHold: slot %d entered the pit lane, released "
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
                wsprintfA(buf, "PaceHold override: slot %d maxSpeed was %d, we "
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
        for (int i = 0; i < VEH_SLOTS; i++) {
            if (g_np_armed[i] || !np_governed(i)) continue;
            if (V_F32(i, OFF_SPEED) < OBSTACLE_MIN) continue;
            np_arm(i);
            set_maxspeed((void*)VEH_AT(i), g_np_cmd);
            g_np_lastw[i] = (float)g_np_cmd;
            char buf[96];
            wsprintfA(buf, "PaceHold: slot %d joined mid-window, saved "
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
                wsprintfA(buf, "PaceHold DRIFT car%d: %d -> %d (a pass got "
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
        log_w("Commentary suppressed.");
    }
    *COMMENTARY_ENABLED = 0;
}

static void commentary_restore(void) {
    if (g_commentary_suppressed) {
        *COMMENTARY_ENABLED = g_commentary_saved;
        g_commentary_suppressed = FALSE;
        log_w("Commentary restored.");
    }
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
            log_w("HUD: RED FLAG written to slot");
            return;
        }
    }
    log_w("HUD: no free slot for RED FLAG");
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

static void hud_clear_countdown(void) {
    uintptr_t inner = *(volatile uint32_t*)HUD_OUTER_PTR;
    if (!inner) return;
    for (int n = 0; n < SLOT_COUNT; n++) {
        char  *text  = (char*)(inner + n * SLOT_STRIDE + SLOT_TEXT_OFF);
        float *timer = (float*)(inner + n * SLOT_STRIDE + SLOT_TIMER_OFF);
        if (strncmp(text, "RACE RESTART IN", 15) == 0) {
            *timer = -1.0f;
            return;
        }
    }
}

/* Patch FUN_004D82D0: change VehicleFlagCheck output from blue (0x41) to red (0x40) flag sprite.
 * Called once at DLL load. Safe: .text is PAGE_EXECUTE_READ, VP makes it writable transiently. */
static BOOL patch_one_byte(DWORD site, BYTE expect, BYTE replacement, const char* label) {
    BYTE cur = *(volatile BYTE*)site;
    char buf[128];
    if (cur == replacement) {
        wsprintfA(buf, "%s: already 0x%02X at 0x%X", label, replacement, site);
        log_w(buf);
        return TRUE;
    }
    if (cur != expect) {
        wsprintfA(buf, "%s: unexpected 0x%02X at 0x%X, skipping", label, cur, site);
        log_w(buf);
        return FALSE;
    }
    DWORD old;
    if (!VirtualProtect((LPVOID)site, 1, PAGE_EXECUTE_READWRITE, &old)) {
        wsprintfA(buf, "%s: VirtualProtect failed at 0x%X", label, site);
        log_w(buf);
        return FALSE;
    }
    *(BYTE*)site = replacement;
    VirtualProtect((LPVOID)site, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (LPVOID)site, 1);
    wsprintfA(buf, "%s: 0x%02X -> 0x%02X at 0x%X", label, expect, replacement, site);
    log_w(buf);
    return TRUE;
}

static void patch_red_flag_sprite(void) {
    patch_one_byte(SPRITE_PATCH_SITE,  SPRITE_PATCH_OLD, SPRITE_PATCH_NEW, "sprite_patch1");
    patch_one_byte(SPRITE_PATCH_SITE2, SPRITE_PATCH_OLD, SPRITE_PATCH_NEW, "sprite_patch2");
}

/* Put the blue-flag immediate back. Needed for the hot-swap: this byte patch
 * lives in the EXE, not in this module, so without it an ejected DLL would
 * leave the game drawing a red flag sprite for the rest of the session - and
 * it is also exactly the write that got this mod pulled from the pack for
 * being invasive merely by being loaded. Leave nothing behind. */
static void unpatch_red_flag_sprite(void) {
    patch_one_byte(SPRITE_PATCH_SITE,  SPRITE_PATCH_NEW, SPRITE_PATCH_OLD, "sprite_unpatch1");
    patch_one_byte(SPRITE_PATCH_SITE2, SPRITE_PATCH_NEW, SPRITE_PATCH_OLD, "sprite_unpatch2");
}

static void show_red_flag_icon(void) {
    uintptr_t veh = *(volatile uintptr_t*)VEH_OBJ_PTR_GLOBAL;
    if (!veh) { log_w("show_red_flag_icon: veh ptr null"); return; }
    *(volatile BYTE*)(veh + OFF_VEHICLE_FLAG) = 1;
    BYTE rb = *(volatile BYTE*)(veh + OFF_VEHICLE_FLAG);
    char buf[96];
    wsprintfA(buf, "show_red_flag_icon: veh=0x%X +0x4108=1 readback=%d", (unsigned)veh, rb);
    log_w(buf);
}

/* Re-write every loop tick — game clears veh+0x4108 each frame. */
static void refresh_red_flag_icon(void) {
    uintptr_t veh = *(volatile uintptr_t*)VEH_OBJ_PTR_GLOBAL;
    if (veh) *(volatile BYTE*)(veh + OFF_VEHICLE_FLAG) = 1;
}

static void hide_red_flag_icon(void) {
    uintptr_t veh = *(volatile uintptr_t*)VEH_OBJ_PTR_GLOBAL;
    if (!veh) return;
    *(volatile BYTE*)(veh + OFF_VEHICLE_FLAG) = 0;
    log_w("hide_red_flag_icon: veh+0x4108=0");
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

/* Seed the per-slot killed cache so cars already out / not yet spawned
 * (or before a new race) are NOT counted as fresh terminal-damage edges. */
static void detect_seed(void) {
    int count = VEHICLE_COUNT;
    for (int i = 0; i < MAX_CARS; i++) g_last_killed[i] = FALSE;
    for (int i = 0; i < count && i < MAX_CARS; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        g_last_killed[i] = veh_killed(veh);
    }
    g_term_times_n = 0;
    g_det_seeded = TRUE;
    log_w("AutoDetect: terminal-damage cache seeded.");
}

/* Poll every car for a fresh running -> killed transition (ActiveFlag ~0).
 * Push its timestamp into a ring; if >=AUTO_TERM_THRESHOLD land within
 * AUTO_TERM_WINDOW_MS, it's a pileup -> return TRUE to fire the red flag.
 * Cooldown-gated so the cars we re-retire during the sequence don't loop it. */
static BOOL check_auto_redflag(void) {
    DWORD now = GetTickCount();
    if (now < g_auto_cooldown_until) return FALSE;
    if (!g_det_seeded) { detect_seed(); return FALSE; }

    uintptr_t player = PLAYER_VEH_PTR;
    int count = VEHICLE_COUNT;
    for (int i = 0; i < count && i < MAX_CARS; i++) {
        char* veh = (char*)VEHICLE_ARRAY[i];
        if (!veh) continue;
        if ((uintptr_t)veh == player) continue;   /* player crash isn't AI pileup */
        BOOL killed = veh_killed(veh);
        if (killed && !g_last_killed[i]) {
            if (g_term_times_n < MAX_CARS) g_term_times[g_term_times_n++] = now;
            char buf[96];
            wsprintfA(buf, "AutoDetect: car%d terminal damage (ActiveFlag->0)", i);
            log_w(buf);
        }
        g_last_killed[i] = killed;
    }

    int recent = 0;
    for (int k = 0; k < g_term_times_n; k++)
        if (now - g_term_times[k] <= AUTO_TERM_WINDOW_MS) recent++;

    if (recent >= AUTO_TERM_THRESHOLD) {
        char buf[96];
        wsprintfA(buf, "AutoDetect: %d terminal within %dms -> AUTO RED FLAG",
                  recent, AUTO_TERM_WINDOW_MS);
        log_w(buf);
        g_auto_cooldown_until = now + AUTO_COOLDOWN_MS;
        g_det_seeded = FALSE;   /* reseed baseline after the sequence */
        return TRUE;
    }
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
 *  So an UNREADABLE pointer, or any failure to resolve the state, ALLOWS the
 *  hotkeys. Blocking on a detector that has never been seen working would
 *  break the offline mod - which is the whole mod - on a guess. Every refusal
 *  and the raw state value are logged, so one offline run and one MP run
 *  settle it; if offline turns out to be nonzero, this is the only place that
 *  has to change.
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
    return (s > 0);            /* -1 unreadable and 0 offline both allow */
}

/* Returns TRUE and logs if the key must be refused. */
static BOOL mp_blocks(const char* what) {
    int s = mp_state();
    if (s <= 0) return FALSE;
    char buf[160];
    wsprintfA(buf, "%s REFUSED: multiplayer session (GameModuleManager state=%d). "
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

/* Cheap identity for the current field: if either half changes, the session
 * was rebuilt and every vehicle pointer we hold is stale. */
static uintptr_t session_epoch(void) {
    int count = VEHICLE_COUNT;
    uintptr_t first = (count > 0) ? (uintptr_t)VEHICLE_ARRAY[0] : 0;
    return first ^ ((uintptr_t)count << 1);
}

static BOOL session_is_live_race(void) {
    if (SESSION_TYPE != 7) return FALSE;
    if (VEHICLE_COUNT < 2)  return FALSE;
    if (!VEHICLE_ARRAY[0])  return FALSE;
    return TRUE;
}

/* Drop the sunk-car bookkeeping WITHOUT writing the restore. Used by the cold
 * path only: those Y values belong to structs the loader has already
 * rebuilt. */
static void hidden_forget(void) {
    if (!g_hidden_count) return;
    char buf[96];
    wsprintfA(buf, "Session gone: forgetting %d sunk DNF cars without "
                   "restoring Y (their structs are stale).", g_hidden_count);
    log_w(buf);
    g_hidden_count = 0;
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
    commentary_restore();
    hide_red_flag_icon();

    /* 5. the two call-site trampolines. AFTER this the game can no longer
     *    enter this module through them. */
    remove_hook();
    remove_hook2();

    /* 6. the two EXE bytes (blue flag sprite immediate) */
    unpatch_red_flag_sprite();

    /* 7. let any call already INSIDE a trampoline finish before the module
     *    goes away. The hooks run on the render/session tick, so a couple of
     *    frames is ample; this only costs time on an eject. */
    Sleep(300);
}

/* Tear down a red-flag sequence that will not finish. The caller sets its own
 * phase back to 0. See the SESSION WATCHDOG block for why `warm` exists. */
static void abort_sequence(BOOL warm, const char* why) {
    char buf[160];
    wsprintfA(buf, "ABORT (%s): %s", warm ? "warm" : "cold", why);
    log_w(buf);

    if (warm) {
        restore_speed_cap();
        unretire_pitret();
        unhide_all();
        hide_red_flag_icon();
    } else {
        /* No vehicle struct is touched. Just drop what we know about them. */
        g_np_active    = FALSE;
        g_pitret_count = 0;
        g_dnf_count    = 0;
        g_active_count = 0;
        g_late_count   = 0;
        hidden_forget();
        for (int i = 0; i < VEH_SLOTS; i++) g_np_armed[i] = 0;
    }

    /* Safe either way: a process-wide flag, two call sites in the EXE, and
     * our own bookkeeping. None of them live in a vehicle struct. */
    g_pos_lock_active = FALSE;
    g_dnf_hidden      = FALSE;
    g_det_seeded      = FALSE;
    kill_tracker_reset();
    commentary_restore();
    remove_hook();
    remove_hook2();
}

static DWORD WINAPI thread_main(LPVOID p) {
    (void)p;
    BOOL prev = FALSE;
    BOOL prev_pitret = FALSE;
    BOOL prev_redflag = FALSE;
    DWORD g_pitret_wait = 0;   /* ms spent waiting for DNF classification (NUMPAD2) */
    int  phase = 0;
    DWORD phase_time = 0;
    DWORD pause_accum = 0;
    DWORD last_tick = 0;
    uintptr_t seq_epoch = 0;   /* field identity when this sequence started */
    int  last_beep_sec = -1;
    BOOL speed_cap_applied = FALSE;

    log_w("gr44.dll loaded. AUTO-DETECT DISABLED (manual only). "
          "NUMPAD1 = timer restart (30s countdown). "
          "NUMPAD2 = FULL RED FLAG: AI to pits, restart when the player reaches his box "
          "(SHIFT+NUMPAD2 aborts). "
          "NUMPAD3 = pit-retire all AI (SHIFT+NUMPAD3 reverts).");

    while (g_running) {
        BOOL key = (GetAsyncKeyState(HOTKEY) & 0x8000) != 0;

        /* ---- SESSION WATCHDOG, before anything else reads a vehicle ------
         * Runs every tick, whatever the phase. A sequence that cannot finish
         * is torn down here rather than left to strand the next race. The
         * epoch test is what catches ESC: the session is rebuilt, every
         * pointer we hold is stale, and the cold path never writes one. */
        if (phase == 1 || phase == 10) {
            /* PRE-RESTART ONLY. Phases 2 and 3 are the restart itself, which
             * legitimately rebuilds the field - running the epoch test there
             * would abort the very thing we asked for. They get a wall-clock
             * backstop instead, below. */
            if (!session_is_live_race()) {
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
            if (phase_time && GetTickCount() - phase_time > PHASE_MAX_MS) {
                abort_sequence(session_is_live_race(),
                               "post-restart phase ran past its time limit");
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

        /* NUMPAD3: pit-retire every AI car. SHIFT+NUMPAD3 reverts arms that
         * have not fired yet. Race only; independent of the red-flag phase. */
        BOOL kp = (GetAsyncKeyState(HOTKEY_PITRET) & 0x8000) != 0;
        if (kp && !prev_pitret && mp_blocks("NUMPAD3")) {
            /* refused: multiplayer */
        } else if (kp && !prev_pitret) {
            if (SESSION_TYPE == 7) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) pit_retire_revert_all();
                else                                     pit_retire_all();
                (void)0;   /* Beep removed */
            } else {
                log_w("NUMPAD3 outside race (SessionType != 7), ignored.");
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
            if (SESSION_TYPE == 7) {
                log_w("TRIGGER: FULL RED FLAG (NUMPAD2) - AI to pits, "
                      "restart when the player reaches his pit box.");
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
                g_pitret_wait = 0;
                seq_epoch = session_epoch();
                phase = 10;
                phase_time = GetTickCount();
                log_w("Waiting for the player to reach his pit BOX "
                      "(pittingStatus 3, or stopped in the lane). "
                      "SHIFT+NUMPAD2 aborts.");
            } else {
                log_w("NUMPAD2 outside race (SessionType != 7), ignored.");
            }
        } else if (kr && !prev_redflag && phase == 10 &&
                   (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            log_w("RED FLAG aborted by SHIFT+NUMPAD2.");
            restore_speed_cap();
            g_pos_lock_active = FALSE;
            unretire_pitret();
            commentary_restore();
            hide_red_flag_icon();
            phase = 0;
        }
        prev_redflag = kr;

        /* Reset detection baseline whenever we're not in a live race. */
        if (SESSION_TYPE != 7) g_det_seeded = FALSE;

        BOOL manual = (key && !prev);
        BOOL autofire = FALSE;
#if AUTO_REDFLAG_ENABLED
        if (phase == 0 && SESSION_TYPE == 7) autofire = check_auto_redflag();
#endif

        if ((manual || autofire) && phase == 0 && mp_blocks("NUMPAD1")) {
            /* refused: multiplayer */
        } else if ((manual || autofire) && phase == 0) {
            if (SESSION_TYPE == 7) {
                log_w(autofire ? "TRIGGER: auto (DNF pileup)" : "TRIGGER: manual (NUMPAD1)");
                g_dnf_hidden      = FALSE;
                g_pos_lock_active = FALSE;
                speed_cap_applied = FALSE;
                g_fuel_logged     = FALSE;
                snapshot_positions();
                inject_commentary(0x0083);
                log_w("PWS_RED01 injected.");
                show_red_flag_icon();
                (void)0;   /* Beep removed */
                seq_epoch = session_epoch();
                phase = 1;
                phase_time = GetTickCount();
                pause_accum = 0;
                last_tick = phase_time;
                last_beep_sec = COUNTDOWN_SEC;
                log_w("Countdown started (30s). Speed cap + position lock will engage after 500ms.");
            } else {
                log_w("NUMPAD1 outside race (SessionType != 7), ignored.");
            }
        }
        prev = key;

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

            /* After 500ms: suppress commentary, apply speed cap, lock positions */
            if (pause_accum >= 500) {
                commentary_suppress();
                if (!speed_cap_applied) {
                    apply_speed_cap();
                    g_pos_lock_active = TRUE;
                    speed_cap_applied = TRUE;
                    log_w("Position lock engaged.");
                }
            }

            /* Catch cars wrecked DURING the countdown before they get a grid
             * slot. Must run BEFORE enforce_positions(), which walks g_active. */
            sweep_late_dnf();

            /* Enforce no-overtaking every tick during countdown */
            if (g_pos_lock_active) enforce_positions();

            int sec_left = COUNTDOWN_SEC - (int)(pause_accum / 1000);
            if (sec_left >= 1) hud_countdown(sec_left);
            /* The per-second countdown beeps are gone (owner's request,
             * 2026-08-02): PC-speaker tones over the game audio. The HUD
             * countdown above is the whole cue now. last_beep_sec is kept
             * only so the tick-edge test still exists if audio comes back. */
            if (sec_left != last_beep_sec && sec_left >= 1) last_beep_sec = sec_left;

            if (pause_accum >= (DWORD)(COUNTDOWN_SEC * 1000)) {
                /* Countdown is up, but do not fire the restart while the game is
                 * still classifying a car we swept — an unclassified wreck gets
                 * a grid slot. Bounded wait; see dnf_settled(). */
                if (!dnf_settled(pause_accum - (DWORD)(COUNTDOWN_SEC * 1000))) {
                    Sleep(1);
                    continue;
                }
                /* Restore speed before restart so cars launch normally */
                restore_speed_cap();
                g_pos_lock_active = FALSE;
                hud_clear_countdown();
                log_w("Countdown done. Restoring speed + applying grid + adjusting laps.");
                apply_qualpos();
                adjust_laps();
                if (g_dnf_count > 0) {
                    if (install_hook())
                        log_w("SceneRenderUpdate hook installed");
                    else
                        log_w("WARNING: SceneRenderUpdate hook failed");
                    if (install_hook2())
                        log_w("SessionTickUpdate hook installed (safety net)");
                    else
                        log_w("WARNING: SessionTickUpdate hook failed");
                }
                *RESTART_FLAG = 1;
                log_w("Mini-restart triggered.");
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

            /* Hold the pace and the order for whoever is still on track. */
            enforce_positions();

            if (player_in_pit_box()) {
                /* Same guard as the countdown path: an unclassified wreck would
                 * be handed a grid slot. g_pitret_wait accumulates only once we
                 * are otherwise ready to fire. */
                if (!dnf_settled(g_pitret_wait)) {
                    g_pitret_wait += 1;
                    Sleep(1);
                    continue;
                }
                log_w("Player is in his pit box -> restarting.");
                /* Restore speed before the restart so the cars launch
                 * normally instead of leaving the grid on the pace cap. */
                restore_speed_cap();
                g_pos_lock_active = FALSE;
                unretire_pitret();      /* put our retirees back on the grid */
                apply_qualpos();
                adjust_laps();
                if (g_dnf_count > 0) {  /* pre-existing DNFs only */
                    if (install_hook())  log_w("SceneRenderUpdate hook installed");
                    if (install_hook2()) log_w("SessionTickUpdate hook installed");
                }
                *RESTART_FLAG = 1;
                log_w("Red-flag restart triggered.");
                (void)0;   /* Beep removed */
                phase = 2;
                phase_time = GetTickCount();
            } else if (GetTickCount() - phase_time > REDFLAG_WAIT_TIMEOUT_MS) {
                log_w("RED FLAG timeout: player never pitted. Reverting field.");
                restore_speed_cap();
                g_pos_lock_active = FALSE;
                unretire_pitret();
                commentary_restore();
                hide_red_flag_icon();
                phase = 0;
            }
        } else if (phase == 2) {
            refresh_red_flag_icon();
            commentary_suppress();
            if (GAME_STATE == 5 && GetTickCount() - phase_time > 500) {
                log_w("Restart complete. Restoring fuel, re-retiring DNF cars.");
                restore_fuel();
                (void)0;   /* Beep removed */
                phase = 3;
                phase_time = GetTickCount();
            } else if (GetTickCount() - phase_time > 10000) {
                log_w("Restart timeout.");
                commentary_restore();
                remove_hook();
                remove_hook2();
                hide_red_flag_icon();
                phase = 0;
            }
        } else if (phase == 3) {
            refresh_red_flag_icon();
            re_retire_dnf();
            restore_fuel();
            /* Sink DNF cars EVERY tick, not once.
             *
             * hide_dnf_cars() used to be one-shot (guarded by g_dnf_hidden). On
             * a FIRST red flag that was enough, because the cars had never been
             * un-hidden and the game was not touching them. From gr40 onward
             * unhide_all() restores their Y at the start of each sequence, so
             * they are live scene objects again — and the game grids them at the
             * BACK (that is what QualPos=200 does; it places them last, it does
             * not park them in the pits). Anything the game did to them after
             * our single 1.4s hide therefore stuck, and they reappeared at the
             * end of the grid on the second red flag. Reported in-game
             * 2026-07-24.
             *
             * re_retire_dnf() directly above already runs every tick for exactly
             * the same reason. g_dnf_hidden now only gates the LOGGING, so the
             * log does not fill with one line per car per tick. */
            if (GetTickCount() - phase_time > 500) {
                hide_dnf_cars_quiet(g_dnf_hidden);
                g_dnf_hidden = TRUE;
            }
            if (GetTickCount() - phase_time > RETIRE_SECS * 1000) {
                remove_hook();
                log_w("SceneRenderUpdate hook removed.");
            }
            if (RACE_PHASE != 2) {
                remove_hook();
                remove_hook2();
                commentary_restore();
                hide_red_flag_icon();
                char buf[64];
                wsprintfA(buf, "RacePhase=%d: gridwalk done. Phase complete.", RACE_PHASE);
                log_w(buf);
                phase = 0;
            } else {
                commentary_suppress();
            }
        }

        /* Hot-swap: the worker ejects the module itself. Polled here rather
         * than at the top of the loop so a red-flag sequence in progress gets
         * torn down through the same path as a normal one. */
        if (g_unload_ev && WaitForSingleObject(g_unload_ev, 0) == WAIT_OBJECT_0) {
            log_w("unload signalled - self-ejecting");
            g_running = FALSE;
            break;
        }

        Sleep(phase == 0 ? 50 : 1);
    }

    eject_teardown();

    if (g_unload_ev) { CloseHandle(g_unload_ev); g_unload_ev = NULL; }

    /* Nothing else in this module runs after this line: FreeLibraryAndExitThread
     * drops the refcount AND ends this thread atomically, so no code is left
     * executing in freed memory. */
    FreeLibraryAndExitThread(g_hinst, 0);
    return 0;   /* not reached */
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hinst = h;
        DisableThreadLibraryCalls(h);
        timeBeginPeriod(1);
        strcpy(g_log, "%USERPROFILE%\\Desktop\\f1c_ida_decompiled\\mods\\grid_reorder\\gr_log.txt");
        patch_red_flag_sprite();
        /* Manual reset, so a signal is not lost if it lands between polls. */
        g_unload_ev = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT_NAME);
        CreateThread(NULL, 0, thread_main, NULL, 0, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        /* Reached on the self-eject too (FreeLibraryAndExitThread). The worker
         * has already run eject_teardown() by then and every step of it is
         * idempotent, so a process shutdown that never went through the worker
         * still gets a clean teardown here. */
        g_running = FALSE;
        Sleep(100);
        eject_teardown();
        timeEndPeriod(1);
    }
    return TRUE;
}
