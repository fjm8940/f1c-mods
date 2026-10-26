/*
 * gr29.c - Grid Reorder mod for F1 Challenge 99-02
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

#define HOTKEY          VK_NUMPAD1
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

static DWORD WINAPI thread_main(LPVOID p) {
    (void)p;
    BOOL prev = FALSE;
    int  phase = 0;
    DWORD phase_time = 0;
    DWORD pause_accum = 0;
    DWORD last_tick = 0;
    int  last_beep_sec = -1;
    BOOL speed_cap_applied = FALSE;

    log_w("gr29.dll loaded. NUMPAD1 = red-flag: 30s countdown, speed cap, no overtaking, red flag icon.");

    while (g_running) {
        BOOL key = (GetAsyncKeyState(HOTKEY) & 0x8000) != 0;

        if (key && !prev && phase == 0) {
            if (SESSION_TYPE == 7) {
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
