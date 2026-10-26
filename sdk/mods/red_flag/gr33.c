/*
 * gr33.c - Grid Reorder mod for F1 Challenge 99-02
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

/* Speed cap multiplier at +0x453C. 1.0=normal, 0.35≈110km/h.
 * Confirmed in red_flag_v2 (rf29.c). 120 km/h ≈ 0.38. */
#define OFF_SPEEDCAP    0x453C
#define SPEEDCAP_SLOW   0.38f
#define SPEEDCAP_NORMAL 1.0f

#define OFF_CURRENTLAP  0x403C
#define OFF_FUELLEVEL   0x3984
#define TOTAL_LAPS_PTR  (*(volatile int*)0x007D05A0)

#define MAX_CARS        26
#define RETIRE_SECS     3
#define COUNTDOWN_SEC   30

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

/* --- Auto red-flag detection: terminal-damage cluster (>=3 cars in 8s) ---
 * "Terminal damage" = ActiveFlag (veh+0x45F4, float) drops nonzero -> 0.0,
 * the game's visual kill switch (~4-5s after a crash). This is much earlier
 * and more correct than FinishState/DNF (the late pit-classification). */
#define AUTO_TERM_THRESHOLD 3
#define AUTO_TERM_WINDOW_MS 8000
#define AUTO_COOLDOWN_MS    120000   /* after a fire, don't re-arm for 2 min */

static BOOL  g_last_killed[MAX_CARS];   /* per-slot: was ActiveFlag ~0 last poll */
static BOOL  g_det_seeded = FALSE;
static DWORD g_term_times[MAX_CARS];
static int   g_term_times_n = 0;
static DWORD g_auto_cooldown_until = 0;

static void*  g_dnf_ptrs[MAX_CARS];
static int    g_dnf_count = 0;
static BOOL   g_dnf_hidden = FALSE;

static char*  g_active[MAX_CARS];
static int    g_active_rp[MAX_CARS];
static int    g_active_count = 0;
static int    g_placed = 0;

/* Saved original speed cap values for each active AI car */
static float  g_saved_speed[MAX_CARS];
static char*  g_speed_veh[MAX_CARS];
static int    g_speed_count = 0;
static BOOL   g_speed_cap_active = FALSE;

/* Snapshotted race positions for no-overtaking enforcement */
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

static void inject_commentary(int eid) {
    int table_pri   = *(volatile int*)(EVENT_TABLE + eid * 0x14 + 0x00);
    int table_score = *(volatile int*)(EVENT_TABLE + eid * 0x14 + 0x10);
    uintptr_t qhead = CONTROLLER + 0x864;
    *(volatile int*)(qhead + 0x00) = eid;
    *(volatile int*)(qhead + 0x04) = table_score;
    *(volatile int*)(qhead + 0x08) = 0;
    *(volatile float*)(qhead + 0x0c) = -1.0f;
    *(volatile int*)(qhead + 0x10) = -1;
    *(volatile int*)(qhead + 0x14) = -1;
    *(volatile int*)(qhead + 0x18) = -1;
    *(volatile int*)(qhead + 0x1c) = table_pri;
    *(volatile float*)(CONTROLLER + 0xaf8 + eid * 4) = 0.0f;
    *(volatile float*)(CONTROLLER + 0xae8) = 0.0f;
    *(volatile int*)(CONTROLLER + 0x135c) = 1;
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

/* Is the player sitting in his own pit box? pitting (+0x3450) == 2 is the
 * "in the box" state, verified on AI cars this session. */
static BOOL player_in_pit_box(void) {
    uintptr_t veh = PLAYER_VEH_PTR;
    if (!veh) return FALSE;
    return *(volatile int*)(veh + OFF_PITTING) == 2;
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

static void hide_dnf_cars(void) {
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
        *(volatile float*)(entry + 4) = HIDE_Y;
        *(volatile float*)(veh + OFF_PHYS_Y) = HIDE_Y;
        wsprintfA(buf, "DNF car %d: scene node Y=-1000, physics Y=-1000", i);
        log_w(buf);
    }
}

static void snapshot_positions(void) {
    int count = VEHICLE_COUNT;
    g_dnf_count    = 0;
    g_active_count = 0;
    g_speed_count  = 0;
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

        BOOL terminal = FALSE;
        if ((uintptr_t)veh != player && !is_pitret(veh)) {
            if (veh_killed(veh)) terminal = TRUE;
            if (*(volatile int*)(veh + OFF_FINISHSTATE) != 0)  terminal = TRUE;
            if (*(volatile BYTE*)(veh + OFF_RETIREDFLAG) != 0) terminal = TRUE;
        }

        if (terminal) {
            if (g_dnf_count < MAX_CARS) g_dnf_ptrs[g_dnf_count++] = veh;
            /* pin it immediately so it cannot un-DNF before the restart */
            *(volatile int*) (veh + OFF_FINISHSTATE) = 2;
            *(volatile BYTE*)(veh + OFF_RETIREDFLAG) = 1;
            *(volatile BYTE*)(veh + OFF_ACTIVEFLAG)  = 0;
            char buf[112];
            wsprintfA(buf, "LATE DNF: car went terminal during the red-flag "
                           "window -> moved to DNF list (%d active, %d DNF)",
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

static void apply_qualpos(void) {
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
}

static void apply_speed_cap(void) {
    if (g_speed_cap_active) return;
    g_speed_count = 0;
    char buf[128];
    uintptr_t player = PLAYER_VEH_PTR;

    for (int i = 0; i < g_active_count; i++) {
        char* veh = g_active[i];
        if (!veh) continue;
        if ((uintptr_t)veh == player) continue;

        float orig = *(volatile float*)(veh + OFF_SPEEDCAP);
        g_speed_veh[g_speed_count]   = veh;
        g_saved_speed[g_speed_count] = orig;
        g_speed_count++;

        *(volatile float*)(veh + OFF_SPEEDCAP) = SPEEDCAP_SLOW;
        wsprintfA(buf, "SpeedCap car%d: applied (120km/h)", i);
        log_w(buf);
    }
    g_speed_cap_active = TRUE;
    wsprintfA(buf, "SpeedCap applied to %d AI cars", g_speed_count);
    log_w(buf);
}

static void restore_speed_cap(void) {
    if (!g_speed_cap_active) return;
    for (int i = 0; i < g_speed_count; i++) {
        char* veh = g_speed_veh[i];
        if (!veh) continue;
        *(volatile float*)(veh + OFF_SPEEDCAP) = g_saved_speed[i];
    }
    g_speed_cap_active = FALSE;
    char buf[64];
    wsprintfA(buf, "SpeedCap restored for %d AI cars", g_speed_count);
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
 * Re-write race positions to snapshotted values each tick.
 * Prevents AI from overtaking during countdown.
 * Logs any drift so we can tell if writes are being overwritten.
 */
static void enforce_positions(void) {
    if (!g_pos_lock_active) return;
    for (int i = 0; i < g_active_count; i++) {
        char* veh = g_active[i];
        if (!veh) continue;
        int current = *(volatile int*)(veh + OFF_RACEPOS);
        if (current != g_locked_racepos[i]) {
            char buf[128];
            wsprintfA(buf, "PosEnforce car%d: drift %d→%d, writing back %d",
                      i, current, g_locked_racepos[i], g_locked_racepos[i]);
            log_w(buf);
        }
        *(volatile int*)(veh + OFF_RACEPOS) = g_locked_racepos[i];
    }
}

static void commentary_suppress(void) {
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

static DWORD WINAPI thread_main(LPVOID p) {
    (void)p;
    BOOL prev = FALSE;
    BOOL prev_pitret = FALSE;
    BOOL prev_redflag = FALSE;
    int  phase = 0;
    DWORD phase_time = 0;
    DWORD pause_accum = 0;
    DWORD last_tick = 0;
    int  last_beep_sec = -1;
    BOOL speed_cap_applied = FALSE;

    log_w("gr33.dll loaded. "
          "NUMPAD1 = timer restart (30s countdown, auto-fires on 3-car DNF pileup in 8s). "
          "NUMPAD2 = FULL RED FLAG: AI to pits, restart when the player reaches his box "
          "(SHIFT+NUMPAD2 aborts). "
          "NUMPAD3 = pit-retire all AI (SHIFT+NUMPAD3 reverts).");

    while (g_running) {
        BOOL key = (GetAsyncKeyState(HOTKEY) & 0x8000) != 0;

        /* NUMPAD3: pit-retire every AI car. SHIFT+NUMPAD3 reverts arms that
         * have not fired yet. Race only; independent of the red-flag phase. */
        BOOL kp = (GetAsyncKeyState(HOTKEY_PITRET) & 0x8000) != 0;
        if (kp && !prev_pitret) {
            if (SESSION_TYPE == 7) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) pit_retire_revert_all();
                else                                     pit_retire_all();
                Beep(900, 120);
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
        if (kr && !prev_redflag && phase == 0) {
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
                Beep(1200, 200);
                phase = 10;
                phase_time = GetTickCount();
                log_w("Waiting for player to enter his pit box (pitting==2). "
                      "SHIFT+NUMPAD2 aborts.");
            } else {
                log_w("NUMPAD2 outside race (SessionType != 7), ignored.");
            }
        } else if (kr && !prev_redflag && phase == 10 &&
                   (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            log_w("RED FLAG aborted by SHIFT+NUMPAD2.");
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
        if (phase == 0 && SESSION_TYPE == 7) autofire = check_auto_redflag();

        if ((manual || autofire) && phase == 0) {
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
                Beep(1200, 200);
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
            if (sec_left != last_beep_sec && sec_left >= 1) {
                last_beep_sec = sec_left;
                if (sec_left <= 5 || sec_left % 10 == 0) Beep(800, 80);
            }

            if (pause_accum >= (DWORD)(COUNTDOWN_SEC * 1000)) {
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
                Beep(600, 300);
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

            if (player_in_pit_box()) {
                log_w("Player is in his pit box -> restarting.");
                unretire_pitret();      /* put our retirees back on the grid */
                apply_qualpos();
                adjust_laps();
                if (g_dnf_count > 0) {  /* pre-existing DNFs only */
                    if (install_hook())  log_w("SceneRenderUpdate hook installed");
                    if (install_hook2()) log_w("SessionTickUpdate hook installed");
                }
                *RESTART_FLAG = 1;
                log_w("Red-flag restart triggered.");
                Beep(600, 300);
                phase = 2;
                phase_time = GetTickCount();
            } else if (GetTickCount() - phase_time > REDFLAG_WAIT_TIMEOUT_MS) {
                log_w("RED FLAG timeout: player never pitted. Reverting field.");
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
                Beep(1200, 100);
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
            if (!g_dnf_hidden && GetTickCount() - phase_time > 500) {
                hide_dnf_cars();
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

        Sleep(phase == 0 ? 50 : 1);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        timeBeginPeriod(1);
        strcpy(g_log, "%USERPROFILE%\\Desktop\\f1c_ida_decompiled\\mods\\grid_reorder\\gr_log.txt");
        patch_red_flag_sprite();
        CreateThread(NULL, 0, thread_main, NULL, 0, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = FALSE;
        Sleep(100);
        restore_speed_cap();
        commentary_restore();
        hide_red_flag_icon();
        remove_hook();
        remove_hook2();
        timeEndPeriod(1);
    }
    return TRUE;
}
