#include <winsock2.h>
#include <windows.h>
#include <math.h>
#include <wincrypt.h>
#include <iphlpapi.h>
#include <commctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "f1c_runtime.h"
#include "f1c_hook.h"
#include "coop_target_probes.h"
#include "f1c_pit_hud.h"
#include "protected_plugin_abi.h"

/* Layout-probe target gate (Lightloader style): no exe hash, checksum or
 * timestamp gate. The loader proves the fixed image base and the code bytes of
 * every game function this mod calls (coop_target_probes.h); every hook site is
 * byte-checked by f1c_detour_install_prevalidated_layout before it is written.
 * The generated hook installers below are routed through that reviewed path. */
static F1CDetourResult coop_registry_install(
    F1CHookRegistry *registry, F1CDetour *hook,
    void *target, void *replacement, const void *expected, size_t stolen_length)
{
    F1CDetourResult result;
    if (!registry || registry->count >= F1C_HOOK_REGISTRY_CAPACITY)
        return F1C_DETOUR_BAD_ARGUMENT;
    result = f1c_detour_install_prevalidated_layout(hook, target, replacement,
                                                    expected, stolen_length);
    if (result == F1C_DETOUR_OK) registry->hooks[registry->count++] = hook;
    return result;
}
#define f1c_hook_registry_install coop_registry_install
#include "generated/mod_bindings.hpp"
#include "generated/hooks.hpp"
#include "generated/commands.hpp"
#include "coop_menu_logic.h"
#include "coop_mf_bridge.h"   /* 2026-10-10: Mechanical Failures sync bridge */
#include "coop_trust.h"       /* 2026-10-13: F-19/F-20 sender trust rules (pure, unit-tested) */
#include "coop_ini_layout.h"  /* 1.0.3: plain-language settings layout + upgrade of older INIs */

#define MOD_ID "multiplayer_overhaul"
#define COOP_LEGACY_ID "coop_mode"
#define MOD_NAME "Multiplayer Overhaul"
#define MOD_VERSION "1.0.4"
#define UNLOAD_EVENT "F1C_COOP_MODE_UNLOAD"

namespace multiplayer_ai_host {

static HMODULE g_module;
static HANDLE g_unload_event;
static F1CHookRegistry g_registry;
static char g_log_path[MAX_PATH];
static char g_config_path[MAX_PATH];
static int32_t g_expected_humans = 2;
static int32_t g_requested_ai = 22;   /* 1.0.3: = shipped AIToAdd (key hidden from the settings page) */
static const int32_t MAX_VEHICLES = 104;
static const int32_t MAX_AI = MAX_VEHICLES - 1;
static char g_selected_ai_paths[MAX_AI][MAX_PATH];
static int32_t g_selected_ai_count;
static char g_reserved_human_paths[MAX_VEHICLES][MAX_PATH];
static int32_t g_reserved_human_count;
static const char *const g_2001_candidates[] = {
    "SEASONDATA\\VEHICLES\\BAR\\2001_BAR003\\2001_bar09.veh",
    "SEASONDATA\\VEHICLES\\Ferrari\\2001_F2001\\2001_ferrari01.veh",
    "SEASONDATA\\VEHICLES\\Ferrari\\2001_F2001\\2001_ferrari02.veh",
    "SEASONDATA\\VEHICLES\\McLaren\\2001_MP4-16\\2001_mclaren03.veh",
    "SEASONDATA\\VEHICLES\\McLaren\\2001_MP4-16\\2001_mclaren04.veh",
    "SEASONDATA\\VEHICLES\\Williams\\2001_FW23\\2001_williams05.veh",
    "SEASONDATA\\VEHICLES\\Williams\\2001_FW23\\2001_williams06.veh",
    "SEASONDATA\\VEHICLES\\Benetton\\2001_B201\\2001_benetton07.veh",
    "SEASONDATA\\VEHICLES\\Benetton\\2001_B201\\2001_benetton08.veh",
    "SEASONDATA\\VEHICLES\\BAR\\2001_BAR003\\2001_bar10.veh",
    "SEASONDATA\\VEHICLES\\Jordan\\2001_EJ11\\2001_jordan11.veh",
    "SEASONDATA\\VEHICLES\\Jordan\\2001_EJ11\\2001_jordan12.veh",
    "SEASONDATA\\VEHICLES\\Arrows\\2001_A22\\2001_arrows14.veh",
    "SEASONDATA\\VEHICLES\\Arrows\\2001_A22\\2001_arrows15.veh",
    "SEASONDATA\\VEHICLES\\Sauber\\2001_C20\\2001_sauber16.veh",
    "SEASONDATA\\VEHICLES\\Sauber\\2001_C20\\2001_sauber17.veh",
    "SEASONDATA\\VEHICLES\\Jaguar\\2001_R2\\2001_jaguar18.veh",
    "SEASONDATA\\VEHICLES\\Jaguar\\2001_R2\\2001_jaguar19.veh",
    "SEASONDATA\\VEHICLES\\Minardi\\2001_PS01\\2001_minardi20.veh",
    "SEASONDATA\\VEHICLES\\Minardi\\2001_PS01\\2001_minardi21.veh",
    "SEASONDATA\\VEHICLES\\Prost\\2001_AP04\\2001_prost22.veh",
    "SEASONDATA\\VEHICLES\\Prost\\2001_AP04\\2001_prost23.veh"
};
static volatile LONG g_active_calls;
static volatile LONG g_stopping;
static volatile LONG g_ready;
static volatile LONG g_prepare_running;
static volatile LONG g_min_humans = 1;
static volatile LONG g_last_human_count = -1;
static volatile LONG g_registration_refreshed;
static volatile LONG g_event_log_count;
static volatile LONG g_placement_log_count;
static volatile LONG g_ai_count;
/* Qualifying keep (owner 2026-10-10, from qualifying_preservation): best laps of
 * every car captured when qualifying is left; the race grid ranks from these.
 * 0 = untimed. Cleared when practice is left; replaced at the next qualifying. */
static float g_quali_keep_time[MAX_VEHICLES];
/* Session type in which a human left; the bot for that car is rebuilt only
 * after the session changes (INI [CoOp] RefillBotMidSession=1 = at once). */
static int32_t g_refill_defer_session = -1;
static int g_refill_mid_session;
static int g_quali_keep_valid;
static volatile LONG g_ai_slots[MAX_AI];
static volatile LONG g_ai_roster_indices[MAX_AI];
static volatile LONG g_ai_roles[MAX_AI];
static char g_ai_paths[MAX_AI][MAX_PATH];  /* .veh path per bot, served to clients */
static volatile LONG g_ai_parked[MAX_AI];   /* bot removed because a human drives its car */
static volatile LONG g_sync_log_count;
static volatile LONG g_scene_entered_mask;
static volatile LONG g_scene_exited_mask;
static volatile LONG g_suspension_entered_mask;
static volatile LONG g_suspension_exited_mask;
static volatile LONG g_grid_assignment_count;

static int read_numbered_paths(const char *section, const char *prefix,
                               char paths[][MAX_PATH], int capacity)
{
    int requested = GetPrivateProfileIntA(section, "Count", 0, g_config_path);
    int loaded = 0;
    if (requested < 0) requested = 0;
    if (requested > capacity) requested = capacity;
    for (int index = 0; index < requested; ++index) {
        char key[32];
        wsprintfA(key, "%s%02d", prefix, index + 1);
        if (GetPrivateProfileStringA(section, key, "", paths[loaded], MAX_PATH,
                                     g_config_path) > 0)
            ++loaded;
    }
    return loaded;
}

static void load_grid_configuration(void)
{
    int configured = GetPrivateProfileIntA("Bots", "Configured", 0, g_config_path);
    g_reserved_human_count = read_numbered_paths(
        "HumanCars", "Human", g_reserved_human_paths, MAX_VEHICLES);
    if (configured) {
        g_selected_ai_count = read_numbered_paths(
            "Bots", "Bot", g_selected_ai_paths, MAX_AI);
        g_requested_ai = g_selected_ai_count;
        return;
    }
    g_selected_ai_count = 0;
}

static int path_is_reserved_for_human(const char *path)
{
    for (int index = 0; index < g_reserved_human_count; ++index)
        if (lstrcmpiA(path, g_reserved_human_paths[index]) == 0) return 1;
    return 0;
}

static void module_sibling_path(char *out, size_t capacity, const char *name)
{
    char *slash;
    DWORD length = GetModuleFileNameA(g_module, out, (DWORD)capacity);
    if (!length || length >= capacity) {
        lstrcpynA(out, name, (int)capacity);
        return;
    }
    slash = strrchr(out, '\\');
    if (slash) lstrcpynA(slash + 1, name,
                         (int)(capacity - (size_t)(slash + 1 - out)));
}

/* Logging (release 0.9.0-wip): every line carries a UTC timestamp. INI [Log]
 * Debug=0 (default) drops high-rate diagnostic lines; Debug=1 keeps everything
 * (for players who send bug reports). Bounded: at MaxMB (default 20) the file
 * moves to coop_mode_log.old.txt and restarts. */
static int g_log_debug = 0;
static DWORD g_log_max_bytes = 20u * 1024u * 1024u;
static CRITICAL_SECTION g_log_lock;
static volatile LONG g_log_lock_state;   /* 0 none, 1 initialising, 2 ready */

#include "coop_usage.h"

static int log_is_debug_only(const char *format)
{
    static const char *const prefixes[] = {
        "race diag", "net rx", "callout", "slot sync", "suspension enter", "suspension exit",
        "scene exit", "skipped duplicate", "prepared AI from host", "authority selection",
        "race grid enforce", "placement call", "entry order", "COOP-E10 content mismatch",
        "debug state"
    };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
        if (!strncmp(format, prefixes[i], strlen(prefixes[i]))) return 1;
    return 0;
}

static void log_line(const char *format, ...)
{
    int debug_only = log_is_debug_only(format);
    {   /* 0.9.1: every line feeds the opt-in usage summary (counters always,
         * text only for non-debug lines); the file write below is unchanged. */
        char text[512];
        va_list copy;
        va_start(copy, format);
        _vsnprintf(text, sizeof(text), format, copy);
        va_end(copy);
        text[sizeof(text) - 1] = 0;
        coopu_tap(text, debug_only);
    }
    if (!g_log_debug && debug_only) return;
    if (InterlockedCompareExchange(&g_log_lock_state, 1, 0) == 0) {
        InitializeCriticalSection(&g_log_lock);
        InterlockedExchange(&g_log_lock_state, 2);
    }
    while (g_log_lock_state != 2) Sleep(0);
    EnterCriticalSection(&g_log_lock);
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExA(g_log_path, GetFileExInfoStandard, &info) &&
        (info.nFileSizeHigh || info.nFileSizeLow > g_log_max_bytes)) {
        char old_path[MAX_PATH + 16];
        lstrcpynA(old_path, g_log_path, MAX_PATH);
        char *dot = strrchr(old_path, '.');
        if (dot) lstrcpyA(dot, ".old.txt");
        MoveFileExA(g_log_path, old_path, MOVEFILE_REPLACE_EXISTING);
    }
    FILE *file = fopen(g_log_path, "a");
    if (file) {
        SYSTEMTIME now;
        GetSystemTime(&now);
        fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u.%03u ", now.wYear, now.wMonth, now.wDay,
                now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
        va_list args;
        va_start(args, format);
        vfprintf(file, format, args);
        va_end(args);
        fputc('\n', file);
        fclose(file);
    }
    LeaveCriticalSection(&g_log_lock);
}


/* Legacy coop_mode.ini -> multiplayer_overhaul.ini. Copy first, then rename the old file
 * to coop_mode.ini.migrated-2026-10-13. Nothing is deleted; a failed copy keeps defaults. */
static void coop_migrate_legacy_ini(void)
{
    char legacy[MAX_PATH], migrated[MAX_PATH];
    module_sibling_path(legacy, sizeof(legacy), COOP_LEGACY_ID ".ini");
    module_sibling_path(migrated, sizeof(migrated), COOP_LEGACY_ID ".ini.migrated-2026-10-13");
    if (GetFileAttributesA(g_config_path) != INVALID_FILE_ATTRIBUTES) return;
    if (GetFileAttributesA(legacy) == INVALID_FILE_ATTRIBUTES) return;
    if (!CopyFileA(legacy, g_config_path, TRUE)) {
        log_line("COOP-M01 ini migration copy failed: %lu; running with defaults",
                 (unsigned long)GetLastError());
        return;
    }
    if (!MoveFileExA(legacy, migrated, 0))
        log_line("COOP-M02 ini migrated copy, but the old file was not renamed: %lu",
                 (unsigned long)GetLastError());
    else
        log_line("ini migrated: coop_mode.ini -> " MOD_ID ".ini (old file kept as .migrated)");
}

/* 1.0.3: rewrite an older INI into the plain-language layout, keeping the player's values
 * (coop_ini_layout.h). The template is the packaged INI, embedded as RCDATA. */
static void coop_upgrade_ini_layout(void)
{
    const void *tmpl = NULL;
    DWORD size = 0;
    HRSRC res = FindResourceA(g_module, "COOP_INI_TEMPLATE", MAKEINTRESOURCEA(10));
    HGLOBAL loaded = res ? LoadResource(g_module, res) : NULL;
    if (loaded) { tmpl = LockResource(loaded); size = SizeofResource(g_module, res); }
    unsigned long error = 0;
    CoopIniUpgrade result = coop_ini_upgrade(g_config_path, tmpl, size, &error);
    if (result == COOP_INI_UPGRADED)
        log_line("ini upgraded to the 1.0.3 layout; values kept, old file kept as " MOD_ID ".ini" COOP_INI_BACKUP_SUFFIX);
    else if (result == COOP_INI_FAILED)
        log_line("COOP-M04 ini layout upgrade failed: %lu; old file kept, old key names still read", error);
}

/* ---- CO-OP main-menu entry ------------------------------------------------
 * The main-menu mode spinner (gizmo APP_SpinBox_GameOpt_GameModeSelect) has its
 * own vtable. Three slots are replaced so the spinner gains a ninth item
 * "CO-OP" after MULTIPLAYER. The game mode stays 5 (multiplayer); g_coop marks
 * CO-OP. Evidence: docs/INVESTIGATION_2026-10-09.md. Addresses are listed in
 * dll/ADDRESS_INVENTORY.md. */
static const uintptr_t ADDR_GAME_MODE = 0x007D0410;
/* Native connection type (0 modem <56K .. 4 LAN). Read natively by 0x460520, 0x461610
 * and 0x463F70 (packet window and budget), not hooked; see docs/SPEC_MODERN_NET_PLAIN_MP_2026-10-13.md. */
static const uintptr_t ADDR_CONN_TYPE = 0x007D0E50;
static const uintptr_t ADDR_SESSION_TYPE = 0x007C56A4;
#ifdef COOP_DEV_TOOLS
static int g_spike_enabled;  /* INI [Spike] Enabled, Development build */
static char g_spike_vehicle_path[MAX_PATH] = "SeasonData\\Vehicles\\Ferrari\\2001_F2001\\2001_ferrari01.veh";
#endif
static int g_race_grid_rewrite = 0;  /* INI [CoOp] RaceGridRewrite, default off: native MP grid (isolation 2026-10-09) */
static const uintptr_t ADDR_MODE_SPINNER_VTABLE = 0x005A2400;
static const uintptr_t ADDR_SPINNER_REBUILD = 0x00472D20;  /* slot 40 */
static const uintptr_t ADDR_SPINNER_SYNC = 0x00472FB0;     /* slot 43 */
static const uintptr_t ADDR_SPINNER_CHANGE = 0x00472E30;   /* slot 46 */
static const uintptr_t ADDR_SPINNER_ADD_ITEM = 0x00486F30;
static const uintptr_t ADDR_SPINNER_GET_ITEM = 0x00487010;
static const uintptr_t ADDR_TEXT_SET = 0x00566650;
static const int SLOT_REBUILD = 40;
static const int SLOT_SYNC = 43;
static const int SLOT_CHANGE = 46;
static const uintptr_t SPINNER_TEXT_OFFSET = 0xB0;

typedef int32_t(__attribute__((thiscall)) *SpinnerNoArgFn)(void *self);
typedef int32_t(__attribute__((thiscall)) *SpinnerChangeFn)(void *self, int32_t dir);
typedef int32_t(__attribute__((thiscall)) *SpinnerAddItemFn)(void *self, const char *label,
                                                          int32_t index);
typedef const char *(__attribute__((thiscall)) *SpinnerGetItemFn)(void *self, int32_t index);
typedef int32_t(__attribute__((thiscall)) *TextSetFn)(void *text, const char *label);

static volatile LONG g_coop;
/* [Menu] LastSelected in multiplayer_overhaul.ini: 0 unknown, 1 coop, 2 multiplayer, 3 other.
 * Only "coop" re-arms CO-OP, and only when vanilla restored mode 5 at the main menu. */
enum { COOP_MENU_UNKNOWN = 0, COOP_MENU_COOP = 1, COOP_MENU_MULTIPLAYER = 2, COOP_MENU_OTHER = 3 };
static int g_menu_saved_state = COOP_MENU_UNKNOWN;
static volatile LONG g_menu_restore_frames = -1;  /* frames left to wait for mode 5; -1 = not armed */
static volatile LONG g_menu_hooked;
static volatile LONG g_menu_active_calls;
static volatile LONG g_inert_logged;

static volatile int32_t *game_mode_ptr(void)
{
    return (volatile int32_t *)ADDR_GAME_MODE;
}

static uintptr_t *spinner_slot(int slot)
{
    return (uintptr_t *)ADDR_MODE_SPINNER_VTABLE + slot;
}

static int coop_mode_is_selected(void)
{
    return coop_sanitize(*game_mode_ptr(), g_coop);
}

static int32_t network_state(void);

/* Atomic ini update: copy to a sibling temp file, write the key there, replace. */
static void coop_save_menu_choice(int state)
{
    const char *value = state == COOP_MENU_COOP ? "coop"
                      : state == COOP_MENU_MULTIPLAYER ? "multiplayer" : "other";
    char temp[MAX_PATH];
    if (state == g_menu_saved_state || !g_config_path[0]) return;
    _snprintf(temp, sizeof(temp), "%s.tmp", g_config_path);
    temp[sizeof(temp) - 1] = '\0';
    if (!CopyFileA(g_config_path, temp, FALSE) ||
        !WritePrivateProfileStringA("Menu", "LastSelected", value, temp) ||
        !MoveFileExA(temp, g_config_path, MOVEFILE_REPLACE_EXISTING)) {
        log_line("COOP-M03 menu choice save failed: %lu", (unsigned long)GetLastError());
        DeleteFileA(temp);
        return;
    }
    g_menu_saved_state = state;
    log_line("menu: saved [Menu] LastSelected=%s", value);
}

static void spinner_show_coop_label(void *self)
{
    const char *label = ((SpinnerGetItemFn)ADDR_SPINNER_GET_ITEM)(self, COOP_SPINNER_INDEX);
    ((TextSetFn)ADDR_TEXT_SET)((char *)self + SPINNER_TEXT_OFFSET, label);
}

int32_t __attribute__((thiscall)) spinner_rebuild_hook(void *self)
{
    int32_t result;
    InterlockedIncrement(&g_menu_active_calls);
    result = ((SpinnerNoArgFn)ADDR_SPINNER_REBUILD)(self);
    /* Native rebuild clears the list and adds 8 labels (indices 0..7). */
    ((SpinnerAddItemFn)ADDR_SPINNER_ADD_ITEM)(self, "CO-OP", -1);
    if (coop_display_index(*game_mode_ptr(), g_coop) == COOP_SPINNER_INDEX)
        spinner_show_coop_label(self);
    InterlockedDecrement(&g_menu_active_calls);
    return result;
}

int32_t __attribute__((thiscall)) spinner_sync_hook(void *self)
{
    int32_t result;
    InterlockedIncrement(&g_menu_active_calls);
    InterlockedExchange(&g_coop, coop_sanitize(*game_mode_ptr(), g_coop));
    /* Restore once per process: wait (max 600 frames) for vanilla to settle on mode 5. */
    if (g_menu_restore_frames >= 0) {
        LONG frames = InterlockedDecrement(&g_menu_restore_frames);
        if (*game_mode_ptr() == 5 && network_state() == 0 && !g_coop) {
            InterlockedExchange(&g_coop, 1);
            g_menu_restore_frames = -1;
            spinner_rebuild_hook(self);
            log_line("menu: CO-OP re-armed from [Menu] LastSelected=coop");
        } else if (frames <= 0) {
            g_menu_restore_frames = -1;
            log_line("menu: CO-OP restore skipped: mode %ld, network %ld",
                     (long)*game_mode_ptr(), (long)network_state());
        }
    }
    result = ((SpinnerNoArgFn)ADDR_SPINNER_SYNC)(self);
    InterlockedDecrement(&g_menu_active_calls);
    return result;
}

int32_t __attribute__((thiscall)) spinner_change_hook(void *self, int32_t dir)
{
    int32_t result = 0;
    int mode, coop, handled;
    InterlockedIncrement(&g_menu_active_calls);
    coop_on_change(*game_mode_ptr(), g_coop, dir, &mode, &coop, &handled);
    if (handled) {
        *game_mode_ptr() = mode;
        InterlockedExchange(&g_coop, coop);
        coop_save_menu_choice(coop ? COOP_MENU_COOP : COOP_MENU_MULTIPLAYER);
        spinner_rebuild_hook(self);
        log_line("menu: %s", coop ? "CO-OP selected" : "CO-OP left");
    } else {
        InterlockedExchange(&g_coop, 0);
        coop_save_menu_choice(COOP_MENU_OTHER);
        result = ((SpinnerChangeFn)ADDR_SPINNER_CHANGE)(self, dir);
    }
    InterlockedDecrement(&g_menu_active_calls);
    return result;
}

/* All-or-nothing: patch only when every slot still holds its native target. */
static int install_menu_hooks(void)
{
    const int slots[3] = {SLOT_REBUILD, SLOT_SYNC, SLOT_CHANGE};
    const uintptr_t natives[3] = {ADDR_SPINNER_REBUILD, ADDR_SPINNER_SYNC, ADDR_SPINNER_CHANGE};
    const uintptr_t hooks[3] = {(uintptr_t)&spinner_rebuild_hook, (uintptr_t)&spinner_sync_hook,
                                (uintptr_t)&spinner_change_hook};
    DWORD old_protection, ignored;
    for (int i = 0; i < 3; ++i) {
        if (*spinner_slot(slots[i]) != natives[i]) {
            log_line("COOP-E01 menu slot %d holds %08lX, expected %08lX; CO-OP entry not installed",
                     slots[i], (unsigned long)*spinner_slot(slots[i]), (unsigned long)natives[i]);
            return 0;
        }
    }
    if (!VirtualProtect(spinner_slot(SLOT_REBUILD), sizeof(uintptr_t) * 7,
                        PAGE_READWRITE, &old_protection)) {
        log_line("COOP-E02 menu vtable protect failed: %lu", (unsigned long)GetLastError());
        return 0;
    }
    for (int i = 0; i < 3; ++i)
        InterlockedExchange((volatile LONG *)spinner_slot(slots[i]), (LONG)hooks[i]);
    VirtualProtect(spinner_slot(SLOT_REBUILD), sizeof(uintptr_t) * 7, old_protection, &ignored);
    InterlockedExchange(&g_menu_hooked, 1);
    log_line("menu: CO-OP entry installed (vtable %08lX slots 40/43/46)",
             (unsigned long)ADDR_MODE_SPINNER_VTABLE);
    return 1;
}

/* Restore only slots that still hold our hook (never overwrite a later hook). */
static int remove_menu_hooks(void)
{
    const int slots[3] = {SLOT_REBUILD, SLOT_SYNC, SLOT_CHANGE};
    const uintptr_t natives[3] = {ADDR_SPINNER_REBUILD, ADDR_SPINNER_SYNC, ADDR_SPINNER_CHANGE};
    const uintptr_t hooks[3] = {(uintptr_t)&spinner_rebuild_hook, (uintptr_t)&spinner_sync_hook,
                                (uintptr_t)&spinner_change_hook};
    DWORD old_protection, ignored;
    int foreign = 0;
    if (!g_menu_hooked) return 1;
    if (!VirtualProtect(spinner_slot(SLOT_REBUILD), sizeof(uintptr_t) * 7,
                        PAGE_READWRITE, &old_protection))
        return 0;
    for (int i = 0; i < 3; ++i) {
        if (*spinner_slot(slots[i]) == hooks[i])
            InterlockedExchange((volatile LONG *)spinner_slot(slots[i]), (LONG)natives[i]);
        else
            ++foreign;
    }
    VirtualProtect(spinner_slot(SLOT_REBUILD), sizeof(uintptr_t) * 7, old_protection, &ignored);
    InterlockedExchange(&g_coop, 0);
    InterlockedExchange(&g_menu_hooked, 0);
    if (foreign) log_line("COOP-E03 %d menu slot(s) were re-hooked by another module", foreign);
    return foreign == 0;
}

static const char *g_probe_failure = "";
static int g_probe_detoured;

/* Exact bytes, or another module's entry detour (E9 rel32, stolen bytes NOPed)
 * over the same function: the mod only CALLS these, so the call chains through
 * that detour exactly like the game's own callers. The bytes past the longest
 * x86 prologue a detour may steal (8) must still match. */
static int probe_matches(const uint8_t *live, const uint8_t *expected)
{
    size_t index;
    if (memcmp(live, expected, 16) == 0) return 1;
    if (live[0] != 0xE9) return 0;
    for (index = 5; index < 8; ++index)
        if (live[index] != expected[index] && live[index] != 0x90) return 0;
    if (memcmp(live + 8, expected + 8, 8) != 0) return 0;
    ++g_probe_detoured;
    return 1;
}

static F1CTargetResult validate_supported_target(const char **variant_name)
{
    HMODULE module = GetModuleHandleA(NULL);
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)module;
    const IMAGE_NT_HEADERS32 *nt;
    size_t index;
    if (!module) return F1C_TARGET_NO_MODULE;
    if ((uintptr_t)module != F1C_TARGET_IMAGE_BASE) return F1C_TARGET_WRONG_BASE;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return F1C_TARGET_BAD_DOS;
    nt = (const IMAGE_NT_HEADERS32 *)((const BYTE *)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return F1C_TARGET_BAD_PE;
    if (nt->FileHeader.Machine != F1C_TARGET_MACHINE) return F1C_TARGET_WRONG_MACHINE;
    for (index = 0; index < sizeof(COOP_TARGET_PROBES) / sizeof(COOP_TARGET_PROBES[0]); ++index) {
        const CoopTargetProbe *probe = &COOP_TARGET_PROBES[index];
        if (probe->address - F1C_TARGET_IMAGE_BASE + sizeof(probe->bytes) >
                nt->OptionalHeader.SizeOfImage ||
            !probe_matches((const uint8_t *)probe->address, probe->bytes)) {
            g_probe_failure = probe->name;
            return F1C_TARGET_CODE_PATCHED;
        }
    }
    *variant_name = nt->OptionalHeader.CheckSum == F1C_TARGET_CHECKSUM
        ? "layout probes OK (canonical 1A70E5ED checksum)" : "layout probes OK (other exe build)";
    return F1C_TARGET_OK;
}

/* Drop entries whose ownership was lost so they are not re-checked (and
 * re-logged) every frame; resets the settled-field gate. Game thread only. */
static void compact_ai_entries(void)
{
    int32_t kept = 0, count = g_ai_count;
    for (int32_t index = 0; index < count; ++index) {
        if (g_ai_slots[index] < 0) continue;
        g_ai_slots[kept] = g_ai_slots[index];
        g_ai_roster_indices[kept] = g_ai_roster_indices[index];
        g_ai_roles[kept] = g_ai_roles[index];
        if (kept != index) lstrcpynA(g_ai_paths[kept], g_ai_paths[index], MAX_PATH);
        g_ai_parked[kept] = g_ai_parked[index];
        ++kept;
    }
    if (kept == count) return;
    for (int32_t index = kept; index < count; ++index) {
        g_ai_slots[index] = -1;
        g_ai_roster_indices[index] = -1;
        g_ai_roles[index] = -1;
    }
    InterlockedExchange(&g_ai_count, kept);
    log_line("AI entries compacted: %ld -> %ld", (long)count, (long)kept);
}

static F1CVehicle *owned_ai(int32_t index)
{
    if (index < 0 || index >= g_ai_count) return NULL;
    LONG slot = g_ai_slots[index];
    if (slot < 0 || slot >= MAX_VEHICLES) return NULL;
    F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot((int)slot);
    if (!vehicle || vehicle->VehicleRosterIndex != g_ai_roster_indices[index]) return NULL;
    return vehicle;
}

static int32_t active_slot_count(void)
{
    int32_t count = 0;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (vehicle && vehicle->WearTickGate__was__slot_live_marker_ != -1) ++count;
    }
    return count;
}

static int32_t owned_ai_count(void)
{
    int32_t owned = 0;
    for (int32_t index = 0; index < g_ai_count; ++index)
        if (owned_ai(index)) ++owned;
    return owned;
}

static int32_t parked_ai_count(void);
static int32_t human_slot_count(void)
{
    int32_t humans = active_slot_count() - (owned_ai_count() - parked_ai_count());
    return humans < 0 ? 0 : humans;
}

/* Bots live at slot 32 and up. The multiplayer layer assigns each network
 * player a slot index from the bottom (stock limit 16 humans) and reloads that
 * slot on every NetComm sync while its live marker is -1. With bots in slots
 * 1..21 a joining human was assigned a bot slot; the sync hook's temporary -1
 * marker made the game reload the human car into it on every sync (laptop
 * resets, lag, host crash in mesh update 0x54D9F0, 2026-10-09). */
static const int32_t AI_FIRST_SLOT = 32;

static int32_t first_inactive_slot(void)
{
    for (int32_t slot = AI_FIRST_SLOT; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (vehicle && vehicle->WearTickGate__was__slot_live_marker_ == -1) return slot;
    }
    return -1;
}

static int32_t local_human_slot(void)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (vehicle && vehicle->WearTickGate__was__slot_live_marker_ == 0)
            return slot;
    }
    return -1;
}

static int roster_is_already_active(int32_t roster_index)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (vehicle && vehicle->WearTickGate__was__slot_live_marker_ != -1 &&
            vehicle->VehicleRosterIndex == roster_index) return 1;
    }
    return 0;
}

static int32_t vehicle_slot_of(const F1CVehicle *vehicle)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot)
        if (vehicle == (F1CVehicle *)f1c_vehicle_slot(slot)) return slot;
    return -1;
}

static int32_t dynamics_slot_of(const void *vehicle_dynamics)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        const uint8_t *vehicle = (const uint8_t *)f1c_vehicle_slot(slot);
        if (vehicle && vehicle + 20 == vehicle_dynamics) return slot;
    }
    return -1;
}

/* ---- Season-matched bot field --------------------------------------------
 * CO-OP fills the grid from the season of the host's own car: every .veh under
 * SeasonData\Vehicles whose Classes= lists that year and not NotAI (the game's
 * own AI-pool rule, findings f1c-notai-class-token-excludes-a-car-from-the-ai-pool).
 * Sorted by Number then path so host and clients build the same list. */
static const size_t CATALOG_STRIDE = 1060;
static char g_game_root[MAX_PATH];
static int32_t g_season = 0;
static char g_season_paths[MAX_AI][MAX_PATH];
static int32_t g_season_numbers[MAX_AI];
static int32_t g_season_rosters[MAX_AI];
static int32_t g_season_count;
static int g_use_season_bots = 1;

static void read_text_file(const char *path, char *out, DWORD capacity)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD got = 0;
    out[0] = 0;
    if (file == INVALID_HANDLE_VALUE) return;
    if (ReadFile(file, out, capacity - 1, &got, NULL)) out[got] = 0;
    CloseHandle(file);
}

/* Value of `Key=...` (first match, case-insensitive key at line start). */
static int flat_value(const char *text, const char *key, char *out, size_t capacity)
{
    size_t key_length = strlen(key);
    const char *line = text;
    while (line && *line) {
        while (*line == ' ' || *line == '\t') ++line;
        if (_strnicmp(line, key, key_length) == 0) {
            const char *p = line + key_length;
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == '=') {
                size_t n = 0;
                ++p;
                while (*p == ' ' || *p == '\t' || *p == '"') ++p;
                while (p[n] && p[n] != '\r' && p[n] != '\n' && p[n] != '"' && n + 1 < capacity) {
                    if (p[n] == '/' && p[n + 1] == '/') break;
                    out[n] = p[n];
                    ++n;
                }
                while (n && (out[n - 1] == ' ' || out[n - 1] == '\t')) --n;
                out[n] = 0;
                return 1;
            }
        }
        line = strchr(line, '\n');
        if (line) ++line;
    }
    out[0] = 0;
    return 0;
}

static int classes_has(const char *classes, const char *wanted)
{
    const char *cursor = classes;
    size_t wanted_length = strlen(wanted);
    while (*cursor) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        const char *end = cursor;
        while (*end && *end != ',') ++end;
        const char *trim = end;
        while (trim > cursor && (trim[-1] == ' ' || trim[-1] == '\t')) --trim;
        if ((size_t)(trim - cursor) == wanted_length &&
            _strnicmp(cursor, wanted, wanted_length) == 0) return 1;
        cursor = end;
    }
    return 0;
}

static int32_t classes_season(const char *classes)
{
    const char *p = classes;
    while (*p) {
        if (p[0] >= '1' && p[0] <= '2' && p[1] >= '0' && p[1] <= '9' && p[2] >= '0' &&
            p[2] <= '9' && p[3] >= '0' && p[3] <= '9' &&
            (p == classes || p[-1] == ' ' || p[-1] == ',') &&
            (p[4] == 0 || p[4] == ',' || p[4] == ' ')) {
            int32_t year = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
            if (year >= 1950 && year <= 2100) return year;
        }
        ++p;
    }
    return 0;
}

/* Catalog path -> absolute path. Catalog paths are game-root relative. */
static void absolute_game_path(const char *path, char *out, size_t capacity)
{
    if (path[0] && path[1] == ':') lstrcpynA(out, path, (int)capacity);
    else wsprintfA(out, "%s\\%s", g_game_root, path);
}

static int32_t vehicle_season(F1CVehicle *vehicle)
{
    static char text[16384];
    char absolute[MAX_PATH], classes[512];
    uint8_t *catalog = *(uint8_t **)G_VehicleCatalog;
    int32_t count = *(int32_t *)G_VehicleCatalogCount;
    int32_t roster = vehicle ? vehicle->VehicleRosterIndex : -1;
    if (!catalog || roster < 0 || roster >= count) return 0;
    const char *path = *(const char **)(catalog + (size_t)roster * CATALOG_STRIDE);
    if (!path || !path[0]) return 0;
    absolute_game_path(path, absolute, sizeof(absolute));
    read_text_file(absolute, text, sizeof(text));
    flat_value(text, "Classes", classes, sizeof(classes));
    log_line("season source: roster=%ld veh=%s classes='%s'", (long)roster, path, classes);
    return classes_season(classes);
}

static void scan_vehicles(const char *directory, int32_t season)
{
    static char text[16384];
    char pattern[MAX_PATH], path[MAX_PATH], classes[512], number[32];
    WIN32_FIND_DATAA data;
    wsprintfA(pattern, "%s\\*", directory);
    HANDLE search = FindFirstFileA(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if (!lstrcmpA(data.cFileName, ".") || !lstrcmpA(data.cFileName, "..")) continue;
        wsprintfA(path, "%s\\%s", directory, data.cFileName);
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_vehicles(path, season);
            continue;
        }
        const char *dot = strrchr(data.cFileName, '.');
        if (!dot || lstrcmpiA(dot, ".veh") != 0 || g_season_count >= MAX_AI) continue;
        read_text_file(path, text, sizeof(text));
        char year[8];
        wsprintfA(year, "%ld", (long)season);
        if (!flat_value(text, "Classes", classes, sizeof(classes)) ||
            !classes_has(classes, year) || classes_has(classes, "NotAI")) continue;
        flat_value(text, "Number", number, sizeof(number));
        lstrcpynA(g_season_paths[g_season_count], path + strlen(g_game_root) + 1, MAX_PATH);
        g_season_numbers[g_season_count] = atoi(number);
        ++g_season_count;
    } while (FindNextFileA(search, &data));
    FindClose(search);
}

/* Returns the number of season candidates (cached per season). */
static int32_t g_season_roster = -1;
static volatile LONG g_settled_humans = -1;
static volatile LONG g_settled_owned = -1;
static int32_t build_season_candidates(int32_t human_slot)
{
    char root[MAX_PATH];
    F1CVehicle *human = human_slot >= 0 ? (F1CVehicle *)f1c_vehicle_slot(human_slot) : NULL;
    if (!human) return 0;
    if (human->VehicleRosterIndex == g_season_roster && g_season_count > 0) return g_season_count;
    int32_t season = vehicle_season(human);
    g_season_roster = human->VehicleRosterIndex;
    if (season <= 0) {
        log_line("COOP-E04 host car season unknown; using INI bot list");
        return 0;
    }
    if (season == g_season && g_season_count > 0) return g_season_count;
    g_season = season;
    g_season_count = 0;
    for (int32_t i = 0; i < MAX_AI; ++i) g_season_rosters[i] = -1;
    wsprintfA(root, "%s\\SeasonData\\Vehicles", g_game_root);
    scan_vehicles(root, season);
    for (int32_t i = 1; i < g_season_count; ++i)        /* insertion sort: tiny list */
        for (int32_t j = i; j > 0; --j) {
            int swap = g_season_numbers[j] < g_season_numbers[j - 1] ||
                (g_season_numbers[j] == g_season_numbers[j - 1] &&
                 lstrcmpiA(g_season_paths[j], g_season_paths[j - 1]) < 0);
            if (!swap) break;
            char tmp[MAX_PATH];
            int32_t n = g_season_numbers[j];
            lstrcpynA(tmp, g_season_paths[j], MAX_PATH);
            lstrcpynA(g_season_paths[j], g_season_paths[j - 1], MAX_PATH);
            lstrcpynA(g_season_paths[j - 1], tmp, MAX_PATH);
            g_season_numbers[j] = g_season_numbers[j - 1];
            g_season_numbers[j - 1] = n;
        }
    log_line("season field: season=%ld candidates=%ld", (long)season, (long)g_season_count);
    return g_season_count;
}


/* ---- Host field manifest (CO-OP host -> client) ----------------------------
 * The client does not pick bots itself. The host serves its exact field over
 * UDP ([CoOp] ManifestPort, default 34500): one line per active bot "slot<TAB>
 * veh path<TAB>veh SHA-256<TAB>driver name". The client polls "COOP1?" every
 * 2 s at [CoOp] HostAddress (default: LAN broadcast), verifies every .veh hash
 * against its own copy, and mirrors the host: same bots, same slots; bots the
 * host parked (a human took the car) are parked on the client too. Any missing
 * or different file refuses the client field (COOP-E10). */
static int32_t network_state(void);
/* 1.0.2 (owner 2026-10-15): Modern disabled. It sends every car at SendHz (60) with no byte budget, ~4x the
 * native LAN rate; on home uplinks the host upload queues (bufferbloat, docs/STALL_ANALYSIS_2026-10-11.md) and
 * clients see AI lag and spikes, while LAN is clean. Record: docs/MODERN_DISABLED_2026-10-15.md.
 * 0 = INI Mode, host NET/NET1 lines and the menu entry cannot turn Modern on. */
static const int NET_MODERN_AVAILABLE = 0;
static int g_net_modern = 0;          /* INI [Net] Mode; in-game menu; client: host value via manifest NET line */
static int g_net_send_hz = 60;        /* INI [Net] SendHz; client: host value */
static int g_plain_mp = 1;            /* INI [Net] PlainMP (0.9.7): Modern also in plain MP (mode 5, no CO-OP) */
static volatile LONG g_plain_ok = 0;  /* 0.9.7 verdict for plain MP Modern; written by the manifest thread */
#include "coop_node_policy.h"
static volatile LONG g_own_node_id = -1; /* 1.0.0: own native netcomm node id, written on the game thread */
static int g_require_mod_coop = 0;   /* 1.0.1 (2026-10-14): forced 0, removal disabled. Was: INI [Net] RequireModCoop, default 1 (2026-10-13 owner decision); 0 = dry-run (log only) in CO-OP */
static int g_require_mod = 0;        /* 1.0.1 (2026-10-14): forced 0, removal disabled. Was: 1.0.0 INI [Net] RequireMod: host boots plain-MP nodes with no NET1 node claim */
static int g_kick_message = 1;       /* 1.0.0 INI [Net] KickMessage: 1 = native chat notice before a real boot */
static int coop_host_now(void);

/* Plain MP = game mode 5 without CO-OP. Modern there is the plain-MP scope. */
static int plain_mode_now(void)
{
    return g_plain_mp && *game_mode_ptr() == 5 && !coop_mode_is_selected();
}
static int g_bot_stream = 0;             /* INI [Net] BotStream (owner 2026-10-11: OFF by default); manifest override on clients */
static int g_bot_stream_port = 34501;
static int g_bot_stream_hz = 30;          /* INI [Net] BotStreamHz; host value via manifest */
static int g_native_bot_hz = 30;          /* INI [Net] NativeBotHz; host-only owned AI rate, >= BotStreamHz */
static int g_bot_stream_hold_ms = 500;    /* INI [Net] BotStreamHoldMs */
static int g_bot_stream_blend_ms = 400;   /* INI [Net] BotStreamBlendMs */
static const int MANIFEST_MAX = 60000;
static char g_manifest_host_address[256] = "";
static int g_manifest_port = 34500;
static CRITICAL_SECTION g_manifest_lock;
static volatile LONG g_manifest_ready;      /* client: a verified manifest is available */
static volatile LONG g_manifest_refused;    /* client: integrity failure */
static volatile LONG g_manifest_generation; /* bumps on every verified change */
/* 0.9.1 countdown sync: race start HUD time in centiseconds. Host: its own value
 * (set by the time-step hook); client: the value from the host manifest header
 * "COOP1 <count> <season> <start_cs>". 30000 = default 5:00. */
static volatile LONG g_host_start_cs = 30000;
static volatile LONG g_client_start_cs = 30000;
static LONG g_manifest_applied = -1;        /* game thread: generation last applied */
static int32_t g_manifest_count;
static int32_t g_manifest_slots[MAX_AI];
static char g_manifest_paths[MAX_AI][MAX_PATH];
static char g_manifest_names[MAX_AI][64];
/* Garage boxes decided by the host (owner 2026-10-10): veh+0x48A4 team garage,
 * veh+0x48A8 bay. The native picker 0x4C6F10 runs only while the team is -1
 * (0x4D1100/0x4D1140, inside SceneRenderUpdate), so presetting both before the
 * scene setup makes every PC use the host's box. */
static const uint32_t VEH_GARAGE_TEAM = 0x48A4, VEH_GARAGE_BAY = 0x48A8;
static int32_t g_host_box_team[MAX_VEHICLES];     /* client: from manifest BOX line */
static int32_t g_host_box_bay[MAX_VEHICLES];
static uint8_t g_host_box_valid[MAX_VEHICLES];
/* 2026-10-12: host best lap per slot (ms) from the BOX line's optional 4th field;
 * a client that joins mid-session fills cars that show no time (0.9.5 clients ignore it). */
static int32_t g_host_best_ms[MAX_VEHICLES];
static volatile LONG g_host_best_gen;
static int32_t g_host_grid_count;                 /* client: host race grid (slot, position) */
static int32_t g_host_grid_slot[MAX_VEHICLES];
static int32_t g_host_grid_pos[MAX_VEHICLES];
namespace hooks { static int coop_grid_text(char *out, int capacity); }
static HANDLE g_net_thread;
static volatile LONG g_sync_season;              /* client: season named by the host */
static volatile LONG g_restart_required;         /* client: host content installed, restart needed */

static int file_sha256_hex(const char *path, char out[65])
{
    BYTE buffer[64 * 1024], digest[32];
    DWORD read_count = 0, digest_size = 32;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    int ok = 0;
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) goto done;
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) goto done;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &read_count, NULL)) goto done;
        if (!read_count) break;
        if (!CryptHashData(hash, buffer, read_count, 0)) goto done;
    }
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_size, 0) || digest_size != 32) goto done;
    for (int i = 0; i < 32; ++i) wsprintfA(out + i * 2, "%02x", digest[i]);
    ok = 1;
done:
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
}

/* Host: snapshot of the active (not parked) field. 0 if no field yet. */
static int build_manifest(char *out, int capacity)
{
    int length, count = g_ai_count;
    char hash[65], absolute[MAX_PATH];
    if (count <= 0 || g_prepare_running) return 0;
    length = wsprintfA(out, "COOP1 %ld %ld %ld\n", (long)count, (long)g_season, (long)g_host_start_cs);
    for (int index = 0; index < count; ++index) {
        F1CVehicle *vehicle = owned_ai(index);
        if (!vehicle || g_ai_parked[index] || !g_ai_paths[index][0]) continue;
        absolute_game_path(g_ai_paths[index], absolute, sizeof(absolute));
        if (!file_sha256_hex(absolute, hash)) lstrcpyA(hash, "-");
        if (length + MAX_PATH + 200 >= capacity) break;
        length += wsprintfA(out + length, "%ld\t%s\t%s\t%s\n", (long)g_ai_slots[index],
                            g_ai_paths[index], hash, vehicle->DriverName);
    }
    length += hooks::coop_grid_text(out + length, capacity - length - 8);
    {   /* every active car's garage box, as the host has it */
        int boxes = 0;
        for (int32_t slot = 0; slot < MAX_VEHICLES && length < capacity - 40; ++slot) {
            uint8_t *car = (uint8_t *)f1c_vehicle_slot(slot);
            if (!car || *(int32_t *)(car + 0x32C8) == -1) continue;
            int32_t team = *(int32_t *)(car + VEH_GARAGE_TEAM), bay = *(int32_t *)(car + VEH_GARAGE_BAY);
            if (team < 0 || bay < 0) continue;
            if (!boxes++) length += wsprintfA(out + length, "BOX");
            float best = *(float *)(car + 0x409C);   /* bestLap, -1 = none */
            length += wsprintfA(out + length, " %ld:%ld:%ld:%ld", (long)slot, (long)team, (long)bay,
                                best > 0.0f && best < 3600.0f ? (long)(best * 1000.0f + 0.5f) : 0L);
        }
        if (boxes) length += wsprintfA(out + length, "\n");
    }
    length += wsprintfA(out + length, "NET %s %d %d %d\n", g_net_modern ? "Modern" : "Vanilla",
                        g_net_send_hz, g_bot_stream, g_bot_stream_hz);
    length += wsprintfA(out + length, "END\n");
    return length;
}

/* Client (net thread): parse + verify. 1 = verified, -1 = refused, 0 = malformed. */
static int accept_manifest(char *text)
{
    static int32_t slots[MAX_AI];
    static char paths[MAX_AI][MAX_PATH];
    static char names[MAX_AI][64];
    char *line = strtok(text, "\n");
    int32_t count = 0, mismatches = 0;
    if (!line || strncmp(line, "COOP1 ", 6) != 0) return 0;
    {
        const char *season = strchr(line + 6, ' ');
        if (season) InterlockedExchange(&g_sync_season, atol(season + 1));
        const char *start = season ? strchr(season + 1, ' ') : NULL;
        LONG start_cs = start ? atol(start + 1) : 30000;
        if (start_cs < 100 || start_cs > 30000) start_cs = 30000;
        if (InterlockedExchange(&g_client_start_cs, start_cs) != start_cs)
            log_line("start timer (client): host start at %.2f", start_cs / 100.0);
    }
    int32_t grid_count = 0;
    static int32_t grid_slot[MAX_VEHICLES], grid_pos[MAX_VEHICLES];
    while ((line = strtok(NULL, "\n")) != NULL && strcmp(line, "END") != 0) {
        if (!strncmp(line, "BOX", 3)) {
            static int32_t team[MAX_VEHICLES], bay[MAX_VEHICLES];
            static uint8_t valid[MAX_VEHICLES];
            static int32_t best[MAX_VEHICLES];
            memset(valid, 0, sizeof(valid));
            memset(best, 0, sizeof(best));
            for (char *item = line + 3; *item; ) {
                while (*item == ' ') ++item;
                if (!*item) break;
                int32_t slot = atoi(item);
                char *c1 = strchr(item, ':'); if (!c1) break;
                char *c2 = strchr(c1 + 1, ':'); if (!c2) break;
                char *next = strchr(c2, ' ');
                char *c3 = strchr(c2 + 1, ':');
                if (slot >= 0 && slot < MAX_VEHICLES) {
                    team[slot] = atoi(c1 + 1); bay[slot] = atoi(c2 + 1); valid[slot] = 1;
                    best[slot] = (c3 && (!next || c3 < next)) ? atol(c3 + 1) : 0;
                }
                item = next;
                if (!item) break;
            }
            EnterCriticalSection(&g_manifest_lock);
            for (int32_t i = 0; i < MAX_VEHICLES; ++i) {
                g_host_box_valid[i] = valid[i]; g_host_box_team[i] = team[i]; g_host_box_bay[i] = bay[i];
                g_host_best_ms[i] = best[i];
            }
            LeaveCriticalSection(&g_manifest_lock);
            InterlockedIncrement(&g_host_best_gen);
            continue;
        }
        if (!strncmp(line, "NET ", 4)) {   /* 0.9.5: host net mode wins on the client (both PCs must match) */
            int modern = NET_MODERN_AVAILABLE && strncmp(line + 4, "Vanilla", 7) != 0;
            const char *rate = strchr(line + 4, ' ');
            int hz = rate ? atoi(rate + 1) : 60;
            hz = hz < 10 ? 10 : (hz > 100 ? 100 : hz);
            const char *stream_value = rate ? strchr(rate + 1, ' ') : NULL;
            int bot_stream = stream_value ? atoi(stream_value + 1) : 0;
            bot_stream = bot_stream < 0 || bot_stream > 2 ? 0 : bot_stream;
            const char *stream_hz_value = stream_value ? strchr(stream_value + 1, ' ') : NULL;
            int stream_hz = stream_hz_value ? atoi(stream_hz_value + 1) : 30;
            stream_hz = stream_hz < 15 ? 15 : (stream_hz > 60 ? 60 : stream_hz);
            if (modern != g_net_modern || hz != g_net_send_hz || bot_stream != g_bot_stream ||
                stream_hz != g_bot_stream_hz)
                log_line("net mode (client): host uses %s %d Hz BotStream=%d/%d Hz",
                         modern ? "Modern" : "Vanilla", hz, bot_stream, stream_hz);
            g_net_modern = modern;
            g_net_send_hz = hz;
            g_bot_stream = bot_stream;
            g_bot_stream_hz = stream_hz;
            continue;
        }
        if (!strncmp(line, "GRID", 4)) {
            for (char *item = line + 4; *item && grid_count < MAX_VEHICLES; ) {
                while (*item == ' ') ++item;
                if (!*item) break;
                char *colon = strchr(item, ':');
                if (!colon) break;
                grid_slot[grid_count] = atoi(item);
                grid_pos[grid_count] = atoi(colon + 1);
                ++grid_count;
                item = strchr(colon, ' ');
                if (!item) break;
            }
            continue;
        }
        char *fields[4] = {};
        char local_hash[65], absolute[MAX_PATH];
        int field = 0;
        for (char *cursor = line; field < 4 && cursor; ++field) {
            fields[field] = cursor;
            cursor = strchr(cursor, '\t');
            if (cursor) *cursor++ = 0;
        }
        if (field < 4 || count >= MAX_AI) return 0;
        slots[count] = atoi(fields[0]);
        lstrcpynA(paths[count], fields[1], MAX_PATH);
        lstrcpynA(names[count], fields[3], 64);
        absolute_game_path(fields[1], absolute, sizeof(absolute));
        if (!file_sha256_hex(absolute, local_hash)) {
            log_line("COOP-E10 content mismatch: slot=%s missing %s (host driver %s)",
                     fields[0], fields[1], fields[3]);
            ++mismatches;
        } else if (lstrcmpiA(local_hash, fields[2]) != 0) {
            log_line("COOP-E10 content mismatch: slot=%s %s differs from host (host driver %s)",
                     fields[0], fields[1], fields[3]);
            ++mismatches;
        }
        ++count;
    }
    if (!line) return 0;
    if (mismatches) {
        log_line("COOP-E10 client field refused: %ld of %ld host bots differ from this PC",
                 (long)mismatches, (long)count);
        return -1;
    }
    EnterCriticalSection(&g_manifest_lock);
    for (int32_t i = 0; i < count; ++i) {
        g_manifest_slots[i] = slots[i];
        lstrcpynA(g_manifest_paths[i], paths[i], MAX_PATH);
        lstrcpynA(g_manifest_names[i], names[i], 64);
    }
    g_manifest_count = count;
    /* 2026-10-12 (laptop log 21:21:16/21:21:29): the host sometimes sends a
     * partial GRID (2-3 entries) between full ones; applied in practice/
     * qualifying/warmup it scrambled the timing table for 2 s. Keep the last
     * full grid: a grid shorter than the bots listed is ignored. */
    if (grid_count == 0 || grid_count >= count) {   /* none = clear (as before); partial = keep last */
        for (int32_t i = 0; i < grid_count; ++i) {
            g_host_grid_slot[i] = grid_slot[i];
            g_host_grid_pos[i] = grid_pos[i];
        }
        g_host_grid_count = grid_count;
    }
    InterlockedIncrement(&g_manifest_generation);
    LeaveCriticalSection(&g_manifest_lock);
    log_line("host manifest verified: %ld active bots, all .veh files identical, grid entries=%ld", (long)count, (long)grid_count);
    return 1;
}

/* ---- Season content sync (host serves, client downloads with backup) -------
 * Spec: docs/SPEC_HOST_AUTHORITY_2026-10-09.md section B. TCP on ManifestPort.
 * Requests: "LIST\n" -> "<size>\t<sha256>\t<SeasonData\relative path>\n"... "END\n";
 * "GET <relative path>\n" -> "SIZE <n>\n" + bytes. The host serves only files
 * of its current season set; the client accepts only SeasonData\ paths without
 * "..". Downloads are staged and hash-checked; every existing file is copied to
 * CoOpBackup\<stamp>\backup\ before it is replaced (RESTORE.txt lists all). */
static const DWORD SYNC_FILE_MAX = 100u * 1024u * 1024u;
static const DWORD SYNC_TOTAL_MAX = 800u * 1024u * 1024u;
static const int SYNC_FILES_MAX = 4000;
static volatile LONG g_sync_started;             /* client: one check per join */
static volatile ULONG g_sync_host_ip;            /* client: host IPv4 from the manifest reply */
static HANDLE g_content_thread;

struct SyncEntry { char path[MAX_PATH]; DWORD size; char sha[65]; };

/* Owner 2026-10-11: content sync never installs program files (Safety Car
 * DLL or any other plug-in). Only SeasonData content is synced; this refuses
 * executable code even if a host lists it. */
static int sync_program_file(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot && (!lstrcmpiA(dot, ".dll") || !lstrcmpiA(dot, ".exe") || !lstrcmpiA(dot, ".asi") ||
                   !lstrcmpiA(dot, ".bat") || !lstrcmpiA(dot, ".cmd") || !lstrcmpiA(dot, ".ps1"));
}

/* Owner 2026-10-18: Camera Editor files are each player's own camera setup
 * (camera_editor.profile.cfg, <driver>.camera_editor.cfg and their .tmp/.bak
 * copies in the car folders). Never listed, served or accepted. */
static int sync_camera_editor_file(const char *path)
{
    const char *slash = strrchr(path, '\\');
    const char *name = slash ? slash + 1 : path;
    for (; *name; ++name)
        if (!_strnicmp(name, "camera_editor", 13)) return 1;
    return 0;
}

static int sync_path_ok(const char *path)
{
    return path[0] && !sync_program_file(path) && !sync_camera_editor_file(path) &&
           _strnicmp(path, "SeasonData\\", 11) == 0 && !strstr(path, "..") &&
           !strchr(path, ':') && !strchr(path, '/') && path[strlen(path) - 1] != '\\';
}

static int sync_skip_name(const char *name)
{
    const char *dot = strrchr(name, '.');
    size_t length = strlen(name);
    if (!lstrcmpiA(name, "Thumbs.db") || sync_camera_editor_file(name)) return 1;
    if (dot && (!lstrcmpiA(dot, ".bak") || !lstrcmpiA(dot, ".log"))) return 1;
    if (length > 8 && !lstrcmpiA(name + length - 8, "_log.txt")) return 1;
    return 0;
}

/* Host: collect the season set. Returns entry count. */
static int collect_dir_files(const char *relative_dir, SyncEntry *out, int count, int recurse)
{
    char pattern[MAX_PATH], relative[MAX_PATH], absolute[MAX_PATH];
    WIN32_FIND_DATAA data;
    wsprintfA(pattern, "%s\\%s\\*", g_game_root, relative_dir);
    HANDLE search = FindFirstFileA(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) return count;
    do {
        if (!lstrcmpA(data.cFileName, ".") || !lstrcmpA(data.cFileName, "..")) continue;
        wsprintfA(relative, "%s\\%s", relative_dir, data.cFileName);
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recurse) count = collect_dir_files(relative, out, count, 1);
            continue;
        }
        if (sync_skip_name(data.cFileName) || sync_program_file(data.cFileName) || count >= SYNC_FILES_MAX ||
            data.nFileSizeHigh || data.nFileSizeLow > SYNC_FILE_MAX) continue;
        absolute_game_path(relative, absolute, sizeof(absolute));
        lstrcpynA(out[count].path, relative, MAX_PATH);
        out[count].size = data.nFileSizeLow;
        if (file_sha256_hex(absolute, out[count].sha)) ++count;
    } while (FindNextFileA(search, &data));
    FindClose(search);
    return count;
}

static int dir_has_season_veh(const char *relative_dir, int32_t season)
{
    static char text[16384];
    char pattern[MAX_PATH], absolute[MAX_PATH], classes[512], year[8];
    WIN32_FIND_DATAA data;
    int found = 0;
    wsprintfA(year, "%ld", (long)season);
    wsprintfA(pattern, "%s\\%s\\*.veh", g_game_root, relative_dir);
    HANDLE search = FindFirstFileA(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) return 0;
    do {
        wsprintfA(absolute, "%s\\%s\\%s", g_game_root, relative_dir, data.cFileName);
        read_text_file(absolute, text, sizeof(text));
        if (flat_value(text, "Classes", classes, sizeof(classes)) && classes_has(classes, year)) found = 1;
    } while (!found && FindNextFileA(search, &data));
    FindClose(search);
    return found;
}

static int walk_vehicle_dirs(const char *relative_dir, int32_t season, SyncEntry *out, int count)
{
    char pattern[MAX_PATH], relative[MAX_PATH];
    WIN32_FIND_DATAA data;
    if (dir_has_season_veh(relative_dir, season)) return collect_dir_files(relative_dir, out, count, 0);
    wsprintfA(pattern, "%s\\%s\\*", g_game_root, relative_dir);
    HANDLE search = FindFirstFileA(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) return count;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            !lstrcmpA(data.cFileName, ".") || !lstrcmpA(data.cFileName, "..")) continue;
        wsprintfA(relative, "%s\\%s", relative_dir, data.cFileName);
        count = walk_vehicle_dirs(relative, season, out, count);
    } while (FindNextFileA(search, &data));
    FindClose(search);
    return count;
}

static int build_season_set(int32_t season, SyncEntry *out)
{
    char pattern[MAX_PATH], relative[MAX_PATH], absolute[MAX_PATH], year[8];
    WIN32_FIND_DATAA data;
    int count = walk_vehicle_dirs("SeasonData\\Vehicles", season, out, 0);
    wsprintfA(year, "%ld", (long)season);
    wsprintfA(pattern, "%s\\SeasonData\\Drivers\\%s*", g_game_root, year);
    HANDLE search = FindFirstFileA(pattern, &data);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            wsprintfA(relative, "SeasonData\\Drivers\\%s", data.cFileName);
            count = collect_dir_files(relative, out, count, 1);
        } while (FindNextFileA(search, &data));
        FindClose(search);
    }
    wsprintfA(pattern, "%s\\SeasonData\\F1_%s*", g_game_root, year);
    search = FindFirstFileA(pattern, &data);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || count >= SYNC_FILES_MAX ||
                sync_skip_name(data.cFileName)) continue;
            wsprintfA(relative, "SeasonData\\%s", data.cFileName);
            absolute_game_path(relative, absolute, sizeof(absolute));
            lstrcpynA(out[count].path, relative, MAX_PATH);
            out[count].size = data.nFileSizeLow;
            if (file_sha256_hex(absolute, out[count].sha)) ++count;
        } while (FindNextFileA(search, &data));
        FindClose(search);
    }
    return count;
}

static int send_all(SOCKET s, const char *data, int length)
{
    while (length > 0) {
        int sent = send(s, data, length, 0);
        if (sent <= 0) return 0;
        data += sent;
        length -= sent;
    }
    return 1;
}

static int recv_line(SOCKET s, char *out, int capacity)
{
    int length = 0;
    while (length < capacity - 1) {
        char c;
        if (recv(s, &c, 1, 0) != 1) return 0;
        if (c == '\n') { out[length] = 0; return 1; }
        out[length++] = c;
    }
    return 0;
}

/* Host: one request per connection. "GET <path>" or "GETR <offset> <path>"
 * (resume): reply "SIZE <total>" line, then bytes from offset. The cache lock
 * is held only while the request is matched, never during the transfer, so a
 * stalled connection cannot block the client's retry. */
static CRITICAL_SECTION g_content_lock;
static void serve_content_client(SOCKET client, const char *peer)
{
    static SyncEntry entries[SYNC_FILES_MAX];
    static int32_t cached_season;
    static int cached_count;
    static char line[MAX_PATH + 16];
    DWORD timeout = 10000, send_timeout = 60000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&send_timeout, sizeof(send_timeout));
    if (!recv_line(client, line, sizeof(line))) {
        log_line("content request from %s: no request line (error %d)", peer, WSAGetLastError());
        return;
    }
    int32_t season = g_season;
    if (season <= 0) return;
    EnterCriticalSection(&g_content_lock);
    if (season != cached_season) {
        cached_count = build_season_set(season, entries);
        cached_season = season;
        log_line("content set: season=%ld files=%d", (long)season, cached_count);
    }
    if (!strcmp(line, "LIST")) {
        char row[MAX_PATH + 100];
        for (int i = 0; i < cached_count; ++i) {
            int n = wsprintfA(row, "%lu\t%s\t%s\n", (unsigned long)entries[i].size, entries[i].sha,
                              entries[i].path);
            if (!send_all(client, row, n)) break;
        }
        send_all(client, "END\n", 4);
        LeaveCriticalSection(&g_content_lock);
        return;
    }
    char wanted[MAX_PATH];
    DWORD offset = 0;
    int listed = 0;
    wanted[0] = 0;
    if (!strncmp(line, "GET ", 4)) {
        lstrcpynA(wanted, line + 4, MAX_PATH);
    } else if (!strncmp(line, "GETR ", 5)) {
        char *rest = NULL;
        offset = (DWORD)strtoul(line + 5, &rest, 10);
        if (rest && *rest == ' ') lstrcpynA(wanted, rest + 1, MAX_PATH);
    }
    for (int i = 0; i < cached_count && !listed && wanted[0]; ++i)
        if (!lstrcmpiA(entries[i].path, wanted)) listed = 1;
    LeaveCriticalSection(&g_content_lock);
    if (wanted[0]) {
        if (!listed || !sync_path_ok(wanted)) {
            log_line("COOP-E13 refused content request: %s", wanted);
            return;
        }
        char absolute[MAX_PATH], header[32];
        absolute_game_path(wanted, absolute, sizeof(absolute));
        HANDLE file = CreateFileA(absolute, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            log_line("COOP-E13 cannot open %s: %lu", wanted, (unsigned long)GetLastError());
            return;
        }
        DWORD size = GetFileSize(file, NULL), got = 0, sent = 0, started = GetTickCount();
        if (offset > size) offset = 0;
        if (offset) SetFilePointer(file, (LONG)offset, NULL, FILE_BEGIN);
        sent = offset;
        int n =wsprintfA(header, "SIZE %lu\n", (unsigned long)size);
        if (send_all(client, header, n)) {
            char *chunk = (char *)HeapAlloc(GetProcessHeap(), 0, 64 * 1024);
            while (chunk && ReadFile(file, chunk, 64 * 1024, &got, NULL) && got) {
                if (!send_all(client, chunk, (int)got)) break;
                sent += got;
            }
            if (chunk) HeapFree(GetProcessHeap(), 0, chunk);
        }
        CloseHandle(file);
        if (sent != size || size > 512 * 1024 || offset)
            log_line("content %s %s to %s: %lu/%lu bytes (from %lu) in %lu ms (error %d)",
                     sent == size ? "sent" : "SEND FAILED", wanted, peer, (unsigned long)sent,
                     (unsigned long)size, (unsigned long)offset,
                     (unsigned long)(GetTickCount() - started), WSAGetLastError());
    }
}

static DWORD WINAPI content_connection_thread(void *argument)
{
    SOCKET client = (SOCKET)(uintptr_t)argument;
    sockaddr_in from = {};
    int from_length = sizeof(from);
    char peer[32] = "?";
    if (getpeername(client, (sockaddr *)&from, &from_length) == 0)
        wsprintfA(peer, "%s:%u", inet_ntoa(from.sin_addr), (unsigned)ntohs(from.sin_port));
    serve_content_client(client, peer);
    shutdown(client, SD_SEND);
    closesocket(client);
    return 0;
}

static DWORD WINAPI content_server_thread(void *)
{
    SOCKET listener = INVALID_SOCKET;
    while (!g_stopping) {
        int32_t state = coop_mode_is_selected() ? network_state() : 0;
        if (state != 1 && state != 2) {
            if (listener != INVALID_SOCKET) { closesocket(listener); listener = INVALID_SOCKET; }
            Sleep(500);
            continue;
        }
        if (listener == INVALID_SOCKET) {
            sockaddr_in address = {};
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            address.sin_family = AF_INET;
            address.sin_port = htons((u_short)g_manifest_port);
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(listener, (sockaddr *)&address, sizeof(address)) != 0 || listen(listener, 4) != 0) {
                log_line("COOP-E12 content port %d (TCP) unavailable: %d", g_manifest_port, WSAGetLastError());
                closesocket(listener);
                listener = INVALID_SOCKET;
                Sleep(3000);
                continue;
            }
            log_line("content server: TCP %d", g_manifest_port);
        }
        fd_set readable;
        timeval wait = {0, 500000};
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        if (select(0, &readable, NULL, NULL, &wait) <= 0) continue;
        SOCKET client = accept(listener, NULL, NULL);
        if (client == INVALID_SOCKET) {
            log_line("content server: accept failed (error %d)", WSAGetLastError());
            continue;
        }
        HANDLE worker = CreateThread(NULL, 0, content_connection_thread, (void *)(uintptr_t)client, 0, NULL);
        if (worker) CloseHandle(worker);
        else closesocket(client);
    }
    if (listener != INVALID_SOCKET) closesocket(listener);
    return 0;
}

static SOCKET connect_host(void)
{
    sockaddr_in address = {};
    DWORD timeout = 60000;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    address.sin_family = AF_INET;
    address.sin_port = htons((u_short)g_manifest_port);
    address.sin_addr.s_addr = g_sync_host_ip;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    if (connect(s, (sockaddr *)&address, sizeof(address)) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static void ensure_parent_dirs(const char *absolute)
{
    char path[MAX_PATH];
    lstrcpynA(path, absolute, MAX_PATH);
    for (char *cursor = path + 3; *cursor; ++cursor)
        if (*cursor == '\\') { *cursor = 0; CreateDirectoryA(path, NULL); *cursor = '\\'; }
}

/* ---- Content download window ------------------------------------------------
 * Plain Win32 window (no resources) owned by the content-check thread:
 * season, host, totals, backup folder, a file table (path, size, reason,
 * live status), overall progress, current file and speed. Download runs on a
 * worker thread; the window polls shared state on a 100 ms timer. Cancel at
 * any point installs nothing. */
enum SyncRowState { ROW_MISSING, ROW_DIFFERENT, ROW_QUEUED, ROW_DOWNLOADING, ROW_VERIFIED,
                    ROW_RETRY, ROW_FAILED, ROW_INSTALLED, ROW_INSTALL_FAILED };
static SyncEntry g_need[SYNC_FILES_MAX];
static volatile LONG g_need_state[SYNC_FILES_MAX];
static volatile LONG g_need_attempt[SYNC_FILES_MAX];
static int g_need_count;
static DWORD g_need_total;
static volatile LONG g_xfer_current = -1;        /* row being downloaded */
static volatile LONG g_xfer_file_done;           /* bytes of the current file */
static volatile LONG g_xfer_bytes_done;          /* bytes of finished files */
static volatile LONG g_xfer_cancel;
static volatile LONG g_xfer_phase;               /* 0 idle, 1 downloading, 2 installing, 3 done, 4 failed, 5 cancelled */
static volatile LONG g_xfer_installed;
static DWORD g_xfer_started;
static char g_xfer_base[MAX_PATH];
static char g_xfer_error[MAX_PATH + 64];

/* Download one file into staging. A retry resumes from the bytes already
 * staged (GETR) instead of starting over; a hash mismatch discards the staged
 * copy so the next attempt starts clean. Every failure logs its cause. */
static int download_file(const SyncEntry *entry, const char *staged, int attempt)
{
    char line[MAX_PATH + 32], hash[65];
    DWORD done = 0, written, started = GetTickCount();
    const char *cause = "connect";
    int ok = 0, error = 0;
    ensure_parent_dirs(staged);
    HANDLE file = CreateFileA(staged, GENERIC_WRITE, 0, NULL, attempt ? OPEN_ALWAYS : CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        log_line("download attempt %d %s: cannot open staging (%lu)", attempt + 1, entry->path,
                 (unsigned long)GetLastError());
        return 0;
    }
    done = GetFileSize(file, NULL);
    if (done == INVALID_FILE_SIZE || done >= entry->size) {
        SetFilePointer(file, 0, NULL, FILE_BEGIN);
        SetEndOfFile(file);
        done = 0;
    }
    SetFilePointer(file, (LONG)done, NULL, FILE_BEGIN);
    DWORD resumed = done;
    InterlockedExchange(&g_xfer_file_done, (LONG)done);
    SOCKET s = connect_host();
    if (s != INVALID_SOCKET) {
        int n = wsprintfA(line, "GETR %lu %s\n", (unsigned long)done, entry->path);
        cause = "request";
        if (send_all(s, line, n) && recv_line(s, line, sizeof(line)) && !strncmp(line, "SIZE ", 5)) {
            DWORD size = (DWORD)strtoul(line + 5, NULL, 10);
            cause = "size";
            if (size == entry->size && size <= SYNC_FILE_MAX) {
                static char chunk[64 * 1024];
                cause = "receive";
                while (done < size && !g_xfer_cancel) {
                    int want = (int)(size - done < sizeof(chunk) ? size - done : sizeof(chunk));
                    int got = recv(s, chunk, want, 0);
                    if (got <= 0) break;
                    if (!WriteFile(file, chunk, (DWORD)got, &written, NULL)) { cause = "write"; break; }
                    done += (DWORD)got;
                    InterlockedExchange(&g_xfer_file_done, (LONG)done);
                }
            }
        }
        error = WSAGetLastError();
        closesocket(s);
    } else {
        error = WSAGetLastError();
    }
    CloseHandle(file);
    if (done == entry->size) {
        ok = file_sha256_hex(staged, hash) && !lstrcmpiA(hash, entry->sha);
        if (!ok) { cause = "hash"; DeleteFileA(staged); done = 0; }
    }
    if (!ok && !g_xfer_cancel)
        log_line("download attempt %d %s failed at %s: %lu/%lu bytes (resumed from %lu) in %lu ms (error %d)",
                 attempt + 1, entry->path, cause, (unsigned long)done, (unsigned long)entry->size,
                 (unsigned long)resumed, (unsigned long)(GetTickCount() - started), error);
    return ok;
}

static DWORD WINAPI download_worker(void *)
{
    char restore_path[MAX_PATH];
    int replaced = 0;
    InterlockedExchange(&g_xfer_phase, 1);
    g_xfer_started = GetTickCount();
    for (int i = 0; i < g_need_count; ++i) InterlockedExchange(&g_need_state[i], ROW_QUEUED);
    for (int i = 0; i < g_need_count; ++i) {
        char staged[MAX_PATH];
        int ok = 0;
        wsprintfA(staged, "%s\\incoming\\%s", g_xfer_base, g_need[i].path);
        InterlockedExchange(&g_xfer_current, i);
        for (int attempt = 0; attempt < 8 && !ok && !g_xfer_cancel; ++attempt) {
            InterlockedExchange(&g_need_attempt[i], attempt + 1);
            InterlockedExchange(&g_need_state[i], attempt ? ROW_RETRY : ROW_DOWNLOADING);
            ok = download_file(&g_need[i], staged, attempt);
            if (!ok && !g_xfer_cancel) Sleep(attempt < 3 ? 1000 * (attempt + 1) : 5000);
        }
        if (g_xfer_cancel) {
            InterlockedExchange(&g_xfer_phase, 5);
            log_line("content download cancelled by the player; nothing installed");
            return 0;
        }
        if (!ok) {
            InterlockedExchange(&g_need_state[i], ROW_FAILED);
            wsprintfA(g_xfer_error, "Download failed: %s", g_need[i].path);
            log_line("COOP-E15 download failed: %s; nothing installed (staging kept in %s)", g_need[i].path, g_xfer_base);
            InterlockedExchange(&g_xfer_phase, 4);
            return 0;
        }
        InterlockedExchange(&g_need_state[i], ROW_VERIFIED);
        InterlockedExchangeAdd(&g_xfer_bytes_done, (LONG)g_need[i].size);
    }
    InterlockedExchange(&g_xfer_current, -1);
    InterlockedExchange(&g_xfer_phase, 2);
    wsprintfA(restore_path, "%s\\RESTORE.txt", g_xfer_base);
    HANDLE restore = CreateFileA(restore_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    for (int i = 0; i < g_need_count; ++i) {
        char target[MAX_PATH], backup[MAX_PATH], staged[MAX_PATH], row[MAX_PATH * 2];
        DWORD written;
        absolute_game_path(g_need[i].path, target, sizeof(target));
        wsprintfA(staged, "%s\\incoming\\%s", g_xfer_base, g_need[i].path);
        wsprintfA(backup, "%s\\backup\\%s", g_xfer_base, g_need[i].path);
        int existed = GetFileAttributesA(target) != INVALID_FILE_ATTRIBUTES;
        if (existed) {
            ensure_parent_dirs(backup);
            if (!CopyFileA(target, backup, FALSE)) {
                InterlockedExchange(&g_need_state[i], ROW_INSTALL_FAILED);
                wsprintfA(g_xfer_error, "Backup failed: %s (installed %d so far, see RESTORE.txt)", g_need[i].path, replaced);
                log_line("COOP-E15 backup failed for %s; stopping (installed %d so far)", g_need[i].path, replaced);
                break;
            }
        }
        ensure_parent_dirs(target);
        if (!CopyFileA(staged, target, FALSE)) {
            InterlockedExchange(&g_need_state[i], ROW_INSTALL_FAILED);
            wsprintfA(g_xfer_error, "Install failed: %s (installed %d so far, see RESTORE.txt)", g_need[i].path, replaced);
            log_line("COOP-E15 install failed for %s", g_need[i].path);
            break;
        }
        ++replaced;
        InterlockedExchange(&g_xfer_installed, replaced);
        InterlockedExchange(&g_need_state[i], ROW_INSTALLED);
        int n = wsprintfA(row, "%s\t%s\r\n", existed ? "REPLACED (original in backup\\)" : "NEW (delete to restore)",
                          g_need[i].path);
        if (restore != INVALID_HANDLE_VALUE) WriteFile(restore, row, (DWORD)n, &written, NULL);
    }
    if (restore != INVALID_HANDLE_VALUE) CloseHandle(restore);
    log_line("content installed: %d of %d files; backup %s", replaced, g_need_count, g_xfer_base);
    /* The game loads season content at start only: the restart notice is shown
     * in the native message log + HUD strip by post_restart_notice(). */
    if (replaced) InterlockedExchange(&g_restart_required, 1);
    InterlockedExchange(&g_xfer_phase, replaced == g_need_count ? 3 : 4);
    return 0;
}

enum { ID_LIST = 100, ID_PROGRESS, ID_HEADER, ID_STATUS, ID_DOWNLOAD, ID_CANCEL, ID_TIMER };
static HWND g_sync_window, g_sync_list, g_sync_progress, g_sync_status, g_sync_download, g_sync_cancel;

static const char *row_state_text(int i)
{
    static char text[48];
    switch (g_need_state[i]) {
    case ROW_MISSING: return "missing here";
    case ROW_DIFFERENT: return "different here";
    case ROW_QUEUED: return "queued";
    case ROW_DOWNLOADING:
        wsprintfA(text, "downloading %lu%%", g_need[i].size
                  ? (unsigned long)((ULONGLONG)g_xfer_file_done * 100 / g_need[i].size) : 100ul);
        return text;
    case ROW_RETRY: wsprintfA(text, "retry %ld", (long)g_need_attempt[i] - 1); return text;
    case ROW_VERIFIED: return "downloaded, verified";
    case ROW_FAILED: return "FAILED";
    case ROW_INSTALLED: return "installed";
    case ROW_INSTALL_FAILED: return "INSTALL FAILED";
    }
    return "";
}

static void format_size(DWORD bytes, char *out)
{
    if (bytes >= 1024 * 1024) wsprintfA(out, "%lu.%lu MB", bytes / 1048576, (bytes % 1048576) * 10 / 1048576);
    else wsprintfA(out, "%lu KB", (bytes + 1023) / 1024);
}

static void refresh_sync_window(void)
{
    char text[MAX_PATH + 200], done_text[32], total_text[32];
    LONG phase = g_xfer_phase, current = g_xfer_current;
    DWORD done = (DWORD)g_xfer_bytes_done + (current >= 0 ? (DWORD)g_xfer_file_done : 0);
    for (int i = 0; i < g_need_count; ++i) {
        LVITEMA item = {};
        item.iSubItem = 3;
        item.pszText = (char *)row_state_text(i);
        SendMessageA(g_sync_list, LVM_SETITEMTEXTA, (WPARAM)i, (LPARAM)&item);
    }
    if (current >= 0) SendMessageA(g_sync_list, LVM_ENSUREVISIBLE, (WPARAM)current, FALSE);
    SendMessageA(g_sync_progress, PBM_SETPOS, g_need_total ? (WPARAM)((ULONGLONG)done * 1000 / g_need_total) : 0, 0);
    format_size(done, done_text);
    format_size(g_need_total, total_text);
    DWORD elapsed = phase ? GetTickCount() - g_xfer_started : 0;
    DWORD rate = elapsed > 500 ? (DWORD)((ULONGLONG)done * 1000 / elapsed) : 0;
    switch (phase) {
    case 0:
        wsprintfA(text, "Ready. Download %d file(s), %s. Your current files are backed up first.", g_need_count, total_text);
        break;
    case 1:
        wsprintfA(text, "Downloading %ld / %d: %s\n%s of %s  -  %lu KB/s", (long)current + 1, g_need_count,
                  current >= 0 ? g_need[current].path : "", done_text, total_text, (unsigned long)(rate / 1024));
        break;
    case 2:
        wsprintfA(text, "Installing: backing up and copying %ld / %d files...", (long)g_xfer_installed, g_need_count);
        break;
    case 3:
        wsprintfA(text, "Done: %ld file(s) installed. Backup + RESTORE.txt in %s\n"
                  "RESTART THE GAME, then join the host again.", (long)g_xfer_installed, g_xfer_base);
        break;
    case 4:
        wsprintfA(text, "%s\nNothing else was changed. Details: coop_mode_log.txt", g_xfer_error);
        break;
    case 5:
        wsprintfA(text, "Cancelled. Nothing was installed.");
        break;
    }
    SetWindowTextA(g_sync_status, text);
    EnableWindow(g_sync_download, phase == 0);
    SetWindowTextA(g_sync_cancel, phase >= 3 ? "Close" : "Cancel");
}

static LRESULT CALLBACK sync_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_TIMER:
        refresh_sync_window();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_DOWNLOAD && g_xfer_phase == 0) {
            HANDLE worker = CreateThread(NULL, 0, download_worker, NULL, 0, NULL);
            if (worker) CloseHandle(worker);
            log_line("content download accepted by the player");
        } else if (LOWORD(wparam) == ID_CANCEL) {
            if (g_xfer_phase == 1) InterlockedExchange(&g_xfer_cancel, 1);
            else if (g_xfer_phase != 2) DestroyWindow(window);
        }
        return 0;
    case WM_CLOSE:
        if (g_xfer_phase == 1) InterlockedExchange(&g_xfer_cancel, 1);
        else if (g_xfer_phase != 2) DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (g_xfer_phase == 0) log_line("content download declined by the player");
        KillTimer(window, ID_TIMER);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

static void run_sync_window(const char *host_text)
{
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
    WNDCLASSA window_class = {};
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    char header[1024], size_text[32], total_text[32];
    MSG message;
    InitCommonControlsEx(&controls);
    window_class.lpfnWndProc = sync_window_proc;
    window_class.hInstance = g_module;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    window_class.lpszClassName = "F1CCoopContentSync";
    RegisterClassA(&window_class);
    g_sync_window = CreateWindowExA(WS_EX_TOPMOST | WS_EX_APPWINDOW, window_class.lpszClassName,
        "F1C CO-OP - host content", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 560, NULL, NULL, g_module, NULL);
    if (!g_sync_window) return;
    int missing = 0;
    for (int i = 0; i < g_need_count; ++i) if (g_need_state[i] == ROW_MISSING) ++missing;
    format_size(g_need_total, total_text);
    wsprintfA(header,
        "The host plays the %ld season with content this PC does not have.\n"
        "Host: %s     Files: %d (%d missing, %d different)     Size: %s\n"
        "Backup of every file that gets replaced: %s",
        (long)g_sync_season, host_text, g_need_count, missing, g_need_count - missing, total_text, g_xfer_base);
    HWND header_label = CreateWindowExA(0, "STATIC", header, WS_CHILD | WS_VISIBLE, 12, 10, 730, 52,
                                        g_sync_window, (HMENU)ID_HEADER, g_module, NULL);
    g_sync_list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT |
        LVS_SINGLESEL | LVS_NOSORTHEADER, 12, 68, 730, 330, g_sync_window, (HMENU)ID_LIST, g_module, NULL);
    SendMessageA(g_sync_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    const char *titles[4] = {"File", "Size", "Reason", "Status"};
    const int widths[4] = {420, 70, 100, 130};
    for (int c = 0; c < 4; ++c) {
        LVCOLUMNA column = {};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = (char *)titles[c];
        column.cx = widths[c];
        column.iSubItem = c;
        SendMessageA(g_sync_list, LVM_INSERTCOLUMNA, (WPARAM)c, (LPARAM)&column);
    }
    for (int i = 0; i < g_need_count; ++i) {
        LVITEMA item = {};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = g_need[i].path + 11;          /* without "SeasonData\" */
        SendMessageA(g_sync_list, LVM_INSERTITEMA, 0, (LPARAM)&item);
        format_size(g_need[i].size, size_text);
        item.iSubItem = 1; item.pszText = size_text;
        SendMessageA(g_sync_list, LVM_SETITEMTEXTA, (WPARAM)i, (LPARAM)&item);
        item.iSubItem = 2; item.pszText = (char *)(g_need_state[i] == ROW_MISSING ? "missing" : "different");
        SendMessageA(g_sync_list, LVM_SETITEMTEXTA, (WPARAM)i, (LPARAM)&item);
    }
    g_sync_progress = CreateWindowExA(0, PROGRESS_CLASSA, "", WS_CHILD | WS_VISIBLE, 12, 408, 730, 18,
                                      g_sync_window, (HMENU)ID_PROGRESS, g_module, NULL);
    SendMessageA(g_sync_progress, PBM_SETRANGE32, 0, 1000);
    g_sync_status = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 12, 432, 730, 50,
                                    g_sync_window, (HMENU)ID_STATUS, g_module, NULL);
    g_sync_download = CreateWindowExA(0, "BUTTON", "Download and install", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                      442, 490, 180, 28, g_sync_window, (HMENU)ID_DOWNLOAD, g_module, NULL);
    g_sync_cancel = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    632, 490, 110, 28, g_sync_window, (HMENU)ID_CANCEL, g_module, NULL);
    HWND children[] = {header_label, g_sync_list, g_sync_progress, g_sync_status, g_sync_download, g_sync_cancel};
    for (HWND child : children) SendMessageA(child, WM_SETFONT, (WPARAM)font, TRUE);
    refresh_sync_window();
    SetTimer(g_sync_window, ID_TIMER, 100, NULL);
    ShowWindow(g_sync_window, SW_SHOWNORMAL);
    SetForegroundWindow(g_sync_window);
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(g_sync_window, &message)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    g_sync_window = NULL;
}

static DWORD WINAPI content_check_thread(void *)
{
    static SyncEntry remote[SYNC_FILES_MAX];
    static char line[MAX_PATH + 100], stamp[32], host_text[32];
    int remote_count = 0;
    SYSTEMTIME now;
    in_addr host;
    SOCKET s = connect_host();
    if (s == INVALID_SOCKET) {
        log_line("COOP-E12 content check: cannot reach host TCP %d", g_manifest_port);
        return 0;
    }
    send_all(s, "LIST\n", 5);
    while (recv_line(s, line, sizeof(line)) && strcmp(line, "END") != 0 && remote_count < SYNC_FILES_MAX) {
        char *size = line, *sha = strchr(line, '\t'), *path;
        if (!sha) continue;
        *sha++ = 0;
        path = strchr(sha, '\t');
        if (!path) continue;
        *path++ = 0;
        if (!sync_path_ok(path) || strlen(sha) != 64) continue;
        lstrcpynA(remote[remote_count].path, path, MAX_PATH);
        lstrcpynA(remote[remote_count].sha, sha, 65);
        remote[remote_count].size = (DWORD)strtoul(size, NULL, 10);
        ++remote_count;
    }
    closesocket(s);
    g_need_count = 0;
    g_need_total = 0;
    for (int i = 0; i < remote_count; ++i) {
        char absolute[MAX_PATH], hash[65];
        absolute_game_path(remote[i].path, absolute, sizeof(absolute));
        int exists = GetFileAttributesA(absolute) != INVALID_FILE_ATTRIBUTES;
        if (exists && file_sha256_hex(absolute, hash) && !lstrcmpiA(hash, remote[i].sha)) continue;
        g_need[g_need_count] = remote[i];
        g_need_state[g_need_count] = exists ? ROW_DIFFERENT : ROW_MISSING;
        g_need_attempt[g_need_count] = 0;
        g_need_total += remote[i].size;
        ++g_need_count;
    }
    log_line("content check: season=%ld host files=%d differ=%d bytes=%lu", (long)g_sync_season,
             remote_count, g_need_count, (unsigned long)g_need_total);
    if (!g_need_count) return 0;
    if (g_need_total > SYNC_TOTAL_MAX) {
        log_line("COOP-E14 content differs by %lu bytes; above the download limit", (unsigned long)g_need_total);
        return 0;
    }
    GetLocalTime(&now);
    wsprintfA(stamp, "%04d%02d%02d-%02d%02d%02d", now.wYear, now.wMonth, now.wDay,
              now.wHour, now.wMinute, now.wSecond);
    wsprintfA(g_xfer_base, "%s\\CoOpBackup\\%s", g_game_root, stamp);
    host.s_addr = g_sync_host_ip;
    lstrcpynA(host_text, inet_ntoa(host), sizeof(host_text));
    InterlockedExchange(&g_xfer_phase, 0);
    InterlockedExchange(&g_xfer_cancel, 0);
    InterlockedExchange(&g_xfer_bytes_done, 0);
    InterlockedExchange(&g_xfer_installed, 0);
    InterlockedExchange(&g_xfer_current, -1);
    run_sync_window(host_text);
    return 0;
}

/* Client: the host's address as this game process sees it - the remote end of
 * its established TCP session to the game port (34447). Works over the
 * internet without any HostAddress setting. 0 if not connected. */
static const u_short GAME_TCP_PORT = 34447;
static ULONG game_session_host_ip(void)
{
    static BYTE table[64 * 1024];
    DWORD size = sizeof(table);
    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0) != NO_ERROR)
        return 0;
    const MIB_TCPTABLE_OWNER_PID *rows = (const MIB_TCPTABLE_OWNER_PID *)table;
    DWORD self = GetCurrentProcessId();
    for (DWORD i = 0; i < rows->dwNumEntries; ++i) {
        const MIB_TCPROW_OWNER_PID *row = &rows->table[i];
        if (row->dwOwningPid == self && row->dwState == MIB_TCP_STATE_ESTAB &&
            ntohs((u_short)row->dwRemotePort) == GAME_TCP_PORT)
            return row->dwRemoteAddr;
    }
    return 0;
}

/* Host: remote addresses of this process's established connections to the game
 * port (34447), i.e. the peers that joined this host. 0.9.7; same table as
 * game_session_host_ip. INFERENCE: the host accepts on GAME_TCP_PORT (not live-verified). */
static int game_session_peer_ips(ULONG *out, int max)
{
    static BYTE table[64 * 1024];
    DWORD size = sizeof(table);
    int count = 0;
    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0) != NO_ERROR)
        return 0;
    const MIB_TCPTABLE_OWNER_PID *rows = (const MIB_TCPTABLE_OWNER_PID *)table;
    DWORD self = GetCurrentProcessId();
    for (DWORD i = 0; i < rows->dwNumEntries && count < max; ++i) {
        const MIB_TCPROW_OWNER_PID *row = &rows->table[i];
        if (row->dwOwningPid == self && row->dwState == MIB_TCP_STATE_ESTAB &&
            ntohs((u_short)row->dwLocalPort) == GAME_TCP_PORT && row->dwRemoteAddr)
            out[count++] = row->dwRemoteAddr;
    }
    return count;
}

/* F-19/F-20 (2026-10-13): rejected UDP senders are logged at most once per 10 s. */
static CoopTrustLogState g_trust_log_client;
static CoopTrustLogState g_trust_log_host;

/* Client: rule "trusted host", trusted_host = session host (0 = none). Host: rule "game peer", trusted_host 0. */
static void log_untrusted_datagram(CoopTrustLogState &state, const char *what, const sockaddr_in &from,
                                   const char *rule, ULONG trusted_host)
{
    uint32_t suppressed = 0;
    char sender[32], expected[64];
    if (!coop_trust_should_log(&state, (uint32_t)GetTickCount(), 10000u, &suppressed)) return;
    /* inet_ntoa returns one shared buffer: copy each address before the next call. */
    lstrcpynA(sender, inet_ntoa(from.sin_addr), sizeof(sender));
    if (trusted_host) {
        char host[32];
        lstrcpynA(host, inet_ntoa(*(const in_addr *)&trusted_host), sizeof(host));
        wsprintfA(expected, "%s %s", rule, host);
    } else {
        wsprintfA(expected, "no %s", rule);
    }
    log_line("COOP-W30 %s from %s:%u ignored (%s); %lu rejects since last line",
             what, sender, (unsigned)ntohs(from.sin_port), expected, (unsigned long)suppressed);
}

/* Host (F-20): 1 when the datagram sender holds an established game TCP session to this host. */
static int coop_host_peer_ok(const sockaddr_in &from)
{
    ULONG peers[64];
    uint32_t list[64];
    int count = game_session_peer_ips(peers, 64);
    for (int i = 0; i < count; ++i) list[i] = (uint32_t)peers[i];
    return coop_trust_host_sender_ok(list, count, (uint32_t)from.sin_addr.s_addr);
}

/* ---- Plain MP Modern: NET1 handshake (0.9.7, spec SPEC_MODERN_NET_PLAIN_MP_2026-10-13.md)
 * Client -> host: "NET1? <link type> <last rtt ms or -1>" every 2 s on ManifestPort.
 * Host -> client: "NET1 <Modern|Vanilla> <SendHz> <host link type>" (the host's verdict).
 * Host peers = its established TCP connections on GAME_TCP_PORT. Every peer must send
 * NET1? within NET1_GRACE_MS, with a usable link, or the session stays Vanilla (sticky).
 * Vanilla fallback leaves the native connection type (0x7D0E50) untouched. All
 * thresholds marked INFERENCE need live tuning. Only the manifest thread writes here. */
static const DWORD NET1_GRACE_MS = 10000;      /* INFERENCE: silent-peer deadline */
static const DWORD NET1_PERIOD_MS = 2000;      /* request period (client) */
static const DWORD NET1_EVAL_MS = 1000;        /* host verdict period */
static const int32_t NET1_MIN_LINK = 2;        /* INFERENCE: native 0-1 = modem classes (<56K, 56K) refuse Modern */
static const int32_t NET1_MAX_RTT_MS = 250;    /* INFERENCE: round-trip ceiling for Modern */
static const int NET1_PEERS = 16;
struct Net1Peer {
    ULONG ip;
    DWORD first_ms;
    DWORD seen_ms;     /* 0 = no NET1? yet */
    int32_t link;      /* -1 unknown */
    int32_t rtt;       /* -1 unknown */
};
static Net1Peer g_net1_peers[NET1_PEERS];
static int g_net1_failed;               /* sticky for the session (host) */
static DWORD g_net1_eval_ms;
static DWORD g_net1_sent_ms;            /* client: last request */
static DWORD g_net1_host_seen_ms;       /* client: last host reply */
static int32_t g_net1_last_rtt = -1;

/* 1.0.0 (spec section 11): NET1? carries the sender's own native netcomm node id as a
 * 4th token. Claims are written by the manifest thread under g_manifest_lock; node1 = id+1
 * so that zero means empty. INFERENCE: client period 2 s, claim window 6 s. */
static const DWORD NET1_CLAIM_MS = 6000;
static const DWORD NET1_NODE_GRACE_MS = 10000;
static const int NET1_CLAIMS = 32;
struct Net1Claim {
    int32_t node1;
    DWORD seen_ms;
};
static Net1Claim g_net1_claims[NET1_CLAIMS];

static void net1_claim_store(int32_t node, DWORD now)
{
    if (node < 0) return;
    EnterCriticalSection(&g_manifest_lock);
    int slot = -1;
    for (int i = 0; i < NET1_CLAIMS; ++i) {
        if (g_net1_claims[i].node1 == node + 1) { slot = i; break; }
        if (slot < 0 && (g_net1_claims[i].node1 == 0 || now - g_net1_claims[i].seen_ms > NET1_CLAIM_MS)) slot = i;
    }
    if (slot >= 0) {
        g_net1_claims[slot].node1 = node + 1;
        g_net1_claims[slot].seen_ms = now ? now : 1;
    }
    LeaveCriticalSection(&g_manifest_lock);
}

static uint32_t net1_claim_list(int32_t *out, uint32_t max)
{
    uint32_t count = 0;
    DWORD now = GetTickCount();
    EnterCriticalSection(&g_manifest_lock);
    for (int i = 0; i < NET1_CLAIMS && count < max; ++i)
        if (g_net1_claims[i].node1 > 0 && now - g_net1_claims[i].seen_ms <= NET1_CLAIM_MS)
            out[count++] = g_net1_claims[i].node1 - 1;
    LeaveCriticalSection(&g_manifest_lock);
    return count;
}

static int32_t net_link_type_now(void)
{
    int32_t value = *(volatile int32_t *)ADDR_CONN_TYPE;
    return value >= 0 && value <= 4 ? value : -1;
}

static void net1_reset(void)
{
    InterlockedExchange(&g_plain_ok, 0);
    memset(g_net1_peers, 0, sizeof(g_net1_peers));
    EnterCriticalSection(&g_manifest_lock);
    memset(g_net1_claims, 0, sizeof(g_net1_claims));
    LeaveCriticalSection(&g_manifest_lock);
    g_net1_failed = 0;
    g_net1_eval_ms = 0;
    g_net1_sent_ms = 0;
    g_net1_host_seen_ms = 0;
    g_net1_last_rtt = -1;
}

static void net1_set_verdict(int ok, const char *why)
{
    if (InterlockedExchange(&g_plain_ok, ok ? 1 : 0) != (ok ? 1 : 0))
        log_line("net mode (plain MP): %s%s%s", ok ? "Modern active" : "Vanilla",
                 why ? " - " : "", why ? why : "");
}

static Net1Peer *net1_peer(ULONG ip, DWORD now)
{
    Net1Peer *free_slot = NULL;
    for (int i = 0; i < NET1_PEERS; ++i) {
        if (g_net1_peers[i].ip == ip) return &g_net1_peers[i];
        if (!free_slot && g_net1_peers[i].ip == 0) free_slot = &g_net1_peers[i];
    }
    if (!free_slot) return NULL;
    free_slot->ip = ip;
    free_slot->first_ms = now ? now : 1;
    free_slot->seen_ms = 0;
    free_slot->link = -1;
    free_slot->rtt = -1;
    return free_slot;
}

/* Host: called every loop while plain MP is hosted. Verdict once per NET1_EVAL_MS. */
static void net1_host_evaluate(void)
{
    DWORD now = GetTickCount();
    ULONG ips[NET1_PEERS];
    if (now - g_net1_eval_ms < NET1_EVAL_MS && g_net1_eval_ms) return;
    g_net1_eval_ms = now ? now : 1;
    int count = game_session_peer_ips(ips, NET1_PEERS);
    for (int i = 0; i < count; ++i)
        if (!net1_peer(ips[i], now)) { g_net1_failed = 1; break; }
    int pending = 0;
    for (int i = 0; i < NET1_PEERS && !g_net1_failed; ++i) {
        const Net1Peer *peer = &g_net1_peers[i];
        if (!peer->ip) continue;
        char ip_text[24];
        lstrcpynA(ip_text, inet_ntoa(*(const in_addr *)&peer->ip), sizeof(ip_text));
        if (peer->seen_ms == 0) {
            if (now - peer->first_ms > NET1_GRACE_MS) {
                g_net1_failed = 1;
                log_line("net mode (plain MP): peer %s gave no NET1 reply in %lu s, Vanilla for this session",
                         ip_text, (unsigned long)(NET1_GRACE_MS / 1000));
            } else {
                ++pending;
            }
        } else if (peer->link < NET1_MIN_LINK || (peer->rtt >= 0 && peer->rtt > NET1_MAX_RTT_MS)) {
            g_net1_failed = 1;
            log_line("net mode (plain MP): peer %s link %ld rtt %ld ms cannot carry Modern, Vanilla for this session",
                     ip_text, (long)peer->link, (long)peer->rtt);
        }
    }
    if (!g_net1_failed && net_link_type_now() < NET1_MIN_LINK) {
        g_net1_failed = 1;
        log_line("net mode (plain MP): host link type %ld cannot carry Modern, Vanilla for this session",
                 (long)net_link_type_now());
    }
    int ok = !g_net1_failed && pending == 0 && g_net_modern;
    net1_set_verdict(ok, ok ? "every peer answered" : (g_net1_failed ? "fallback to native" : NULL));
}

/* Host: answer "NET1? <link> <rtt>" from a peer and record the peer's link. */
static void net1_host_answer(SOCKET sock, const sockaddr_in &from, int from_length, const char *text)
{
    DWORD now = GetTickCount();
    Net1Peer *peer = net1_peer(from.sin_addr.s_addr, now);
    if (!peer) { g_net1_failed = 1; return; }
    const char *link_text = strchr(text, ' ');
    const char *rtt_text = link_text ? strchr(link_text + 1, ' ') : NULL;
    peer->link = link_text ? atoi(link_text + 1) : -1;
    peer->rtt = rtt_text ? atoi(rtt_text + 1) : -1;
    int32_t claim_node = -1;
    if (node_policy_parse_net1(text, &claim_node)) {
        net1_claim_store(claim_node, now);
    } else if (peer->seen_ms == 0) {
        log_line("net mode (plain MP): peer %s sent NET1? without a node id (pre-1.0.0 client); its node is unclaimed",
                 inet_ntoa(from.sin_addr));
    }
    peer->seen_ms = now ? now : 1;
    char reply[64];
    int ok = g_plain_ok != 0;
    wsprintfA(reply, "NET1 %s %d %ld", ok ? "Modern" : "Vanilla", g_net_send_hz, (long)net_link_type_now());
    sendto(sock, reply, (int)strlen(reply), 0, (const sockaddr *)&from, from_length);
}

/* Client: send the periodic request to the game host and expire a lost verdict. */
static void net1_client_tick(SOCKET sock)
{
    DWORD now = GetTickCount();
    if (now - g_net1_sent_ms >= NET1_PERIOD_MS) {
        ULONG host = game_session_host_ip();
        char message[64];
        sockaddr_in to = {};
        g_net1_sent_ms = now ? now : 1;   /* throttle the TCP table lookup even without a host */
        if (host) {
            to.sin_family = AF_INET;
            to.sin_port = htons((u_short)g_manifest_port);
            to.sin_addr.s_addr = host;
            wsprintfA(message, "NET1? %ld %ld %ld", (long)net_link_type_now(), (long)g_net1_last_rtt,
                      (long)g_own_node_id);
            sendto(sock, message, (int)strlen(message), 0, (sockaddr *)&to, sizeof(to));
        }
    }
    if (g_plain_ok && g_net1_host_seen_ms && now - g_net1_host_seen_ms > NET1_GRACE_MS)
        net1_set_verdict(0, "host reply lost");
}

/* Client: apply the host's verdict. Returns 1 when the datagram was a NET1 reply. */
static int net1_client_reply(const sockaddr_in &from, const char *text, int received)
{
    if (received < 5 || memcmp(text, "NET1 ", 5) != 0) return 0;
    ULONG host = game_session_host_ip();
    if (!host || from.sin_addr.s_addr != host) return 1;
    DWORD now = GetTickCount();
    int modern = NET_MODERN_AVAILABLE && strncmp(text + 5, "Modern", 6) == 0;
    const char *rate = strchr(text + 5, ' ');
    int hz = rate ? atoi(rate + 1) : 60;
    hz = hz < 10 ? 10 : (hz > 100 ? 100 : hz);
    if (g_net1_sent_ms && now >= g_net1_sent_ms && now - g_net1_sent_ms < NET1_GRACE_MS)
        g_net1_last_rtt = (int32_t)(now - g_net1_sent_ms);
    g_net1_host_seen_ms = now ? now : 1;
    if (modern) {
        g_net_modern = 1;
        g_net_send_hz = hz;
    } else {
        g_net_modern = 0;
    }
    net1_set_verdict(modern, modern ? "host uses Modern" : "host uses Vanilla");
    return 1;
}

/* ---- Mechanical Failures sync (2026-10-10) ---------------------------------
 * Spec: development/mods/mechanical_failures/docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md
 * This DLL creates the named block Local\COOP_MF_BRIDGE_V1 (coop_mf_bridge.h)
 * and carries MF's fault events on the ManifestPort UDP link: the PC that
 * simulates a car (host = bots + own car, client = own car) sends what its car
 * shows (smoke, fire, sounds, retirement); the host relays client events to
 * the other clients. Datagram: "MFEV1" + count byte + count x CoopMfEvent.
 * Reliable events (fire, retirement) go out 3 times; MF drops duplicates.
 * INI [CoOp] MfSync=0 turns the whole channel off (block stays inactive). */
static int g_mf_sync = 0;  /* 0.9.3 ships OFF: no in-game test yet */
static const int MFB_PEERS = 8;
static const int MFB_RESEND = 16;
static const int MFB_PACKET = 1400;
static const DWORD MFB_PEER_TTL_MS = 15000;
struct MfbPeer { sockaddr_in addr; DWORD last_ms; };
struct MfbResend { char data[1400]; int len; int left; DWORD next_ms; int to_host; sockaddr_in exclude; int has_exclude; };
static MfbPeer g_mfb_peers[MFB_PEERS];
static MfbResend g_mfb_resend[MFB_RESEND];
/* 2026-10-10: second channel for Pit Stop Failures (same bridge layout,
 * Local\COOP_PIT_BRIDGE_V1, wire tag "PFEV1"); peers and resends shared. */
static int g_pit_sync = 1;  /* INI [CoOp] PitSync */
struct MfbChannel { CoopMfBridge *b; const char *name; const char *tag; int *enabled; const char *label; };
static MfbChannel g_mf_channel = { NULL, COOP_MF_BRIDGE_NAME, COOP_MF_WIRE_TAG, &g_mf_sync, "MF" };
static MfbChannel g_pit_channel = { NULL, "Local\\COOP_PIT_BRIDGE_V1", "PFEV1", &g_pit_sync, "Pit" };
static MfbChannel *const g_channels[] = { &g_mf_channel, &g_pit_channel };

static void mfb_create(MfbChannel &ch)
{
    HANDLE map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                    sizeof(CoopMfBridge), ch.name);
    if (!map) {
        log_line("COOP-W10 %s sync bridge not created: %lu", ch.label, GetLastError());
        return;
    }
    int existed = GetLastError() == ERROR_ALREADY_EXISTS;
    CoopMfBridge *bridge = (CoopMfBridge *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CoopMfBridge));
    if (!bridge) {
        log_line("COOP-W10 %s sync bridge not mapped: %lu", ch.label, GetLastError());
        CloseHandle(map);
        return;
    }
    /* the mapping handle stays open for the process lifetime (MF may open late) */
    if (!existed || bridge->magic != COOP_MF_MAGIC || bridge->version != COOP_MF_VERSION) {
        memset(bridge, 0, sizeof(*bridge));
        bridge->version = COOP_MF_VERSION;
        bridge->magic = COOP_MF_MAGIC;
    }
    bridge->active = 0;
    ch.b = bridge;
    log_line("%s sync bridge ready (%s)", ch.label, *ch.enabled ? "on" : "off by INI");
}

static void mfb_set_state(MfbChannel &ch, int32_t state)
{
    if (!ch.b) return;
    LONG active = *ch.enabled && !g_stopping && state >= 1 && state <= 3;
    InterlockedExchange(&ch.b->role, state == 3 ? 2 : 1);
    if (InterlockedExchange(&ch.b->active, active) != active)
        log_line("%s sync %s (%s, mod %s)", ch.label, active ? "active" : "inactive",
                 state == 3 ? "client" : "host", ch.b->mf_attached ? "attached" : "not attached");
}

static int mfb_same(const sockaddr_in &a, const sockaddr_in &b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

static void mfb_peer_seen(const sockaddr_in &from)
{
    int free_index = -1, oldest = 0;
    DWORD now = GetTickCount();
    for (int i = 0; i < MFB_PEERS; ++i) {
        if (g_mfb_peers[i].last_ms && mfb_same(g_mfb_peers[i].addr, from)) {
            g_mfb_peers[i].last_ms = now ? now : 1;
            return;
        }
        if (free_index < 0 && (!g_mfb_peers[i].last_ms || now - g_mfb_peers[i].last_ms > MFB_PEER_TTL_MS))
            free_index = i;
        if (now - g_mfb_peers[i].last_ms > now - g_mfb_peers[oldest].last_ms) oldest = i;
    }
    int slot = free_index >= 0 ? free_index : oldest;
    g_mfb_peers[slot].addr = from;
    g_mfb_peers[slot].last_ms = now ? now : 1;
    log_line("MF sync peer %s:%u", inet_ntoa(from.sin_addr), (unsigned)ntohs(from.sin_port));
}

static int mfb_is_packet(const MfbChannel &ch, const char *buffer, int length)
{
    return length >= 6 && memcmp(buffer, ch.tag, 5) == 0 &&
           length == 6 + (int)(uint8_t)buffer[5] * (int)sizeof(CoopMfEvent);
}

static void mfb_ingest(MfbChannel &ch, const char *buffer, int length)
{
    if (!ch.b || !ch.b->active || !mfb_is_packet(ch, buffer, length)) return;
    int count = (uint8_t)buffer[5];
    for (int i = 0; i < count; ++i) {
        CoopMfEvent event;
        memcpy(&event, buffer + 6 + i * (int)sizeof(CoopMfEvent), sizeof(event));
        event.name[sizeof(event.name) - 1] = 0;
        event.text[sizeof(event.text) - 1] = 0;
        if (coop_mf_push_in(ch.b, &event)) InterlockedIncrement(&ch.b->recv);
        else InterlockedIncrement(&ch.b->dropped);
    }
}

static void mfb_send_raw(SOCKET sock, const char *data, int length, int to_host,
                         const sockaddr_in *exclude)
{
    if (to_host) {
        if (!g_sync_host_ip) return;
        sockaddr_in to = {};
        to.sin_family = AF_INET;
        to.sin_port = htons((u_short)g_manifest_port);
        to.sin_addr.s_addr = g_sync_host_ip;
        sendto(sock, data, length, 0, (sockaddr *)&to, sizeof(to));
        return;
    }
    DWORD now = GetTickCount();
    for (int i = 0; i < MFB_PEERS; ++i) {
        if (!g_mfb_peers[i].last_ms || now - g_mfb_peers[i].last_ms > MFB_PEER_TTL_MS) continue;
        if (exclude && mfb_same(g_mfb_peers[i].addr, *exclude)) continue;
        sendto(sock, data, length, 0, (sockaddr *)&g_mfb_peers[i].addr, sizeof(g_mfb_peers[i].addr));
    }
}

static void mfb_send(SOCKET sock, const char *data, int length, int to_host,
                     const sockaddr_in *exclude, int reliable)
{
    mfb_send_raw(sock, data, length, to_host, exclude);
    if (!reliable || length > MFB_PACKET) return;
    for (int i = 0; i < MFB_RESEND; ++i) {
        MfbResend &r = g_mfb_resend[i];
        if (r.left > 0) continue;
        memcpy(r.data, data, (size_t)length);
        r.len = length;
        r.left = 2;
        r.next_ms = GetTickCount() + 120;
        r.to_host = to_host;
        r.has_exclude = exclude != NULL;
        if (exclude) r.exclude = *exclude;
        return;
    }
}

static void mfb_resend_pump(SOCKET sock)
{
    DWORD now = GetTickCount();
    for (int i = 0; i < MFB_RESEND; ++i) {
        MfbResend &r = g_mfb_resend[i];
        if (r.left <= 0 || (LONG)(now - r.next_ms) < 0) continue;
        mfb_send_raw(sock, r.data, r.len, r.to_host, r.has_exclude ? &r.exclude : NULL);
        --r.left;
        r.next_ms = now + 250;
    }
}

/* drain MF's out ring into datagrams (host: to every client, client: to host) */
static void mfb_flush(MfbChannel &ch, SOCKET sock, int to_host)
{
    if (!ch.b) return;
    if (!ch.b->active) {             /* not in a CO-OP session: discard */
        CoopMfEvent discard;
        while (coop_mf_pop(ch.b->out, &ch.b->out_r, &ch.b->out_w, &discard)) {}
        return;
    }
    const int per_packet = (MFB_PACKET - 6) / (int)sizeof(CoopMfEvent);
    char packet[MFB_PACKET];
    for (int guard = 0; guard < COOP_MF_RING / 4; ++guard) {
        int count = 0, reliable = 0;
        CoopMfEvent event;
        while (count < per_packet && coop_mf_pop(ch.b->out, &ch.b->out_r, &ch.b->out_w, &event)) {
            memcpy(packet + 6 + count * (int)sizeof(CoopMfEvent), &event, sizeof(event));
            if (event.type & COOP_MF_RELIABLE) reliable = 1;
            ++count;
        }
        if (!count) return;
        memcpy(packet, ch.tag, 5);
        packet[5] = (char)count;
        int length = 6 + count * (int)sizeof(CoopMfEvent);
        mfb_send(sock, packet, length, to_host, NULL, reliable);
        InterlockedExchangeAdd(&ch.b->sent, count);
    }
}

/* host: a client's events -> this PC's MF + every other client */
static void mfb_host_receive(MfbChannel &ch, SOCKET sock, const char *buffer, int length, const sockaddr_in &from)
{
    mfb_peer_seen(from);
    mfb_ingest(ch, buffer, length);
    int reliable = 0;
    int count = (uint8_t)buffer[5];
    for (int i = 0; i < count; ++i) {
        uint16_t type;
        memcpy(&type, buffer + 6 + i * (int)sizeof(CoopMfEvent), sizeof(type));
        if (type & COOP_MF_RELIABLE) reliable = 1;
    }
    if (ch.b && ch.b->active) mfb_send(sock, buffer, length, 0, &from, reliable);
}

static void coop1_legacy_log(const sockaddr_in &from)
{
    static ULONG logged[16];
    for (int i = 0; i < 16; ++i)
        if (logged[i] == from.sin_addr.s_addr) return;
    for (int i = 0; i < 16; ++i)
        if (logged[i] == 0) { logged[i] = from.sin_addr.s_addr; break; }
    log_line("manifest (CO-OP): peer %s sent COOP1? without a node id (pre-1.0.0 client); its node is unclaimed",
             inet_ntoa(from.sin_addr));
}

static DWORD WINAPI manifest_net_thread(void *)
{
    static char buffer[MANIFEST_MAX + 1];
    static char last_text[MANIFEST_MAX + 1];
    WSADATA wsa;
    SOCKET host_socket = INVALID_SOCKET, client_socket = INVALID_SOCKET;
    DWORD timeout = (g_mf_sync || g_pit_sync) ? 50 : 500, last_request = 0, last_log = 0;   /* 50 ms: MF sync latency */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        log_line("COOP-E12 manifest network unavailable");
        return 0;
    }
    while (!g_stopping) {
        /* 0.9.7: plain MP (mode 5, no CO-OP) runs only the NET1 exchange. The bot
         * manifest, content and MF channels stay CO-OP only (coop below). */
        int coop = coop_mode_is_selected();
        int plain = !coop && plain_mode_now();
        int32_t state = (coop || plain) ? network_state() : 0;
        int plain_active = plain && state != 0;
        if (!plain_active) net1_reset();
        for (MfbChannel *ch : g_channels) mfb_set_state(*ch, coop ? state : 0);
        if (state == 1 || state == 2) {
            if (plain_active) net1_host_evaluate();
            if (host_socket == INVALID_SOCKET) {
                sockaddr_in bind_address = {};
                host_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                bind_address.sin_family = AF_INET;
                bind_address.sin_port = htons((u_short)g_manifest_port);
                bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
                setsockopt(host_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
                if (bind(host_socket, (sockaddr *)&bind_address, sizeof(bind_address)) != 0) {
                    log_line("COOP-E12 manifest port %d bind failed: %d", g_manifest_port, WSAGetLastError());
                    closesocket(host_socket);
                    host_socket = INVALID_SOCKET;
                    Sleep(2000);
                    continue;
                }
                log_line("manifest server: UDP %d", g_manifest_port);
            }
            sockaddr_in from = {};
            int from_length = sizeof(from);
            int received = recvfrom(host_socket, buffer, MANIFEST_MAX, 0, (sockaddr *)&from, &from_length);
            if (received > 0 && received <= MANIFEST_MAX) buffer[received] = 0;
            if (received >= 6 && memcmp(buffer, "COOP1?", 6) == 0) {
                int32_t coop_claim = -1;
                int game_peer = coop_host_peer_ok(from);
                /* 2026-10-13 review: a node claim only counts from a game peer (no spoofed kick bypass). */
                if (!node_policy_parse_coop1(buffer, &coop_claim)) coop1_legacy_log(from);
                else if (game_peer) net1_claim_store(coop_claim, GetTickCount());
                else log_untrusted_datagram(g_trust_log_host, "COOP1? node claim", from, "game peer", 0);
                /* F-20: only game peers join the MF relay list. The manifest reply itself is
                 * still sent to any asker (bot list, not secret); recorded as open in the fix record. */
                if (coop_host_peer_ok(from)) mfb_peer_seen(from);
                int length = coop ? build_manifest(buffer, MANIFEST_MAX) : 0;   /* plain MP: no bot manifest */
                if (length > 0)
                    sendto(host_socket, buffer, length, 0, (sockaddr *)&from, from_length);
            } else if (received >= 5 && memcmp(buffer, "NET1?", 5) == 0) {
                /* 2026-10-13 review: a non-peer NET1? must not add a Net1Peer slot (one bad link or 16
                 * strangers would force the session to Vanilla) nor store a node claim. */
                if (plain_active && coop_host_peer_ok(from)) net1_host_answer(host_socket, from, from_length, buffer);
                else if (plain_active) log_untrusted_datagram(g_trust_log_host, "NET1?", from, "game peer", 0);
            } else {
                for (MfbChannel *ch : g_channels)
                    if (mfb_is_packet(*ch, buffer, received)) {
                        /* F-20: MF datagrams are relayed to every peer, so accept them only from game peers. */
                        if (coop_host_peer_ok(from)) mfb_host_receive(*ch, host_socket, buffer, received, from);
                        else log_untrusted_datagram(g_trust_log_host, "MF datagram", from, "game peer", 0);
                    }
            }
            for (MfbChannel *ch : g_channels) mfb_flush(*ch, host_socket, 0);
            mfb_resend_pump(host_socket);
            continue;
        }
        if (host_socket != INVALID_SOCKET) { closesocket(host_socket); host_socket = INVALID_SOCKET; }
        if (state == 3 && (!coop || !g_manifest_refused)) {   /* CO-OP: 0.9.6 gate unchanged */
            if (client_socket == INVALID_SOCKET) {
                BOOL broadcast = TRUE;
                client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                setsockopt(client_socket, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast));
                setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
            }
            if (plain_active) net1_client_tick(client_socket);
            if (coop && !g_manifest_refused && GetTickCount() - last_request >= (g_manifest_ready ? 2000u : 1000u)) {
                /* Once a host answered, ask only it. Before that, ask every
                 * HostAddress entry (comma list: LAN, Tailscale, ...) and the
                 * LAN broadcast; the first reply fixes the host address. */
                sockaddr_in to = {};
                char addresses[sizeof(g_manifest_host_address)];
                char coop_request[32];
                int coop_request_len = wsprintfA(coop_request, "COOP1? %ld", (long)g_own_node_id);
                int log_now = !g_manifest_ready && GetTickCount() - last_log >= 10000;
                to.sin_family = AF_INET;
                to.sin_port = htons((u_short)g_manifest_port);
                if (g_manifest_ready && g_sync_host_ip) {
                    to.sin_addr.s_addr = g_sync_host_ip;
                    sendto(client_socket, coop_request, coop_request_len, 0, (sockaddr *)&to, sizeof(to));
                } else {
                    ULONG session_host = game_session_host_ip();
                    if (session_host) {
                        to.sin_addr.s_addr = session_host;
                        sendto(client_socket, coop_request, coop_request_len, 0, (sockaddr *)&to, sizeof(to));
                        if (log_now) log_line("manifest request -> game host %s:%d",
                                              inet_ntoa(to.sin_addr), g_manifest_port);
                    } else if (log_now) {
                        log_line("COOP-W02 no TCP %u session to a host found", (unsigned)GAME_TCP_PORT);
                    }
                    lstrcpynA(addresses, g_manifest_host_address, sizeof(addresses));
                    for (char *item = strtok(addresses, ", "); item; item = strtok(NULL, ", ")) {
                        to.sin_addr.s_addr = inet_addr(item);
                        if (to.sin_addr.s_addr == INADDR_NONE) continue;
                        if (sendto(client_socket, coop_request, coop_request_len, 0, (sockaddr *)&to, sizeof(to)) < 0 && log_now)
                            log_line("manifest request to %s failed: %d", item, WSAGetLastError());
                    }
                    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
                    sendto(client_socket, coop_request, coop_request_len, 0, (sockaddr *)&to, sizeof(to));
                }
                last_request = GetTickCount();
                if (log_now) {
                    log_line("manifest request -> [%s] + broadcast, port %d (no host reply yet)",
                             g_manifest_host_address, g_manifest_port);
                    last_log = GetTickCount();
                }
            }
            sockaddr_in from = {};
            int from_length = sizeof(from);
            int received = recvfrom(client_socket, buffer, MANIFEST_MAX, 0, (sockaddr *)&from, &from_length);
            if (received > 0 && received <= MANIFEST_MAX) buffer[received] = 0;
            if (net1_client_reply(from, buffer, received)) received = 0;
            /* F-19 (2026-10-13): the trusted host is the remote end of this game's own TCP session
             * to the game port, never the sender of a datagram. No session = nothing trusted. */
            ULONG session_host = received > 0 ? game_session_host_ip() : 0;
            int from_host = coop_trust_client_sender_ok(session_host, from.sin_addr.s_addr);
            for (MfbChannel *ch : g_channels) if (coop && mfb_is_packet(*ch, buffer, received)) {
                if (from_host) mfb_ingest(*ch, buffer, received);
                else log_untrusted_datagram(g_trust_log_client, "MF datagram", from, "trusted host", session_host);
                received = 0;
            }
            for (MfbChannel *ch : g_channels) mfb_flush(*ch, client_socket, 1);
            mfb_resend_pump(client_socket);
            if (received > 0 && coop && !from_host)   /* F-19: manifest text from a non-host sender */
                log_untrusted_datagram(g_trust_log_client, "manifest text", from, "trusted host", session_host);
            if (received > 0 && coop && from_host) {   /* plain MP never accepts the bot manifest */
                buffer[received] = 0;
                InterlockedExchange((volatile LONG *)&g_sync_host_ip, (LONG)session_host);
                if (strcmp(buffer, last_text) != 0) {
                    lstrcpynA(last_text, buffer, sizeof(last_text));
                    int verdict = accept_manifest(buffer);
                    if (verdict > 0) InterlockedExchange(&g_manifest_ready, 1);
                    else if (verdict < 0) InterlockedExchange(&g_manifest_refused, 1);
                    else last_text[0] = 0;
                    if (verdict != 0 && g_sync_season > 0 && InterlockedExchange(&g_sync_started, 1) == 0) {
                        HANDLE check = CreateThread(NULL, 0, content_check_thread, NULL, 0, NULL);
                        if (check) CloseHandle(check);
                    }
                }
            }
            continue;
        }
        if (client_socket != INVALID_SOCKET) { closesocket(client_socket); client_socket = INVALID_SOCKET; }
        if (state != 3) {
            InterlockedExchange(&g_manifest_ready, 0);
            InterlockedExchange(&g_manifest_refused, 0);
            InterlockedExchange(&g_sync_started, 0);
            last_text[0] = 0;
        }
        Sleep(250);
    }
    if (host_socket != INVALID_SOCKET) closesocket(host_socket);
    if (client_socket != INVALID_SOCKET) closesocket(client_socket);
    WSACleanup();
    return 0;
}

namespace hooks { static void apply_entry_order(int32_t session_type); }

static int32_t parked_ai_count(void)
{
    int32_t parked = 0;
    for (int32_t index = 0; index < g_ai_count; ++index)
        if (g_ai_parked[index] && owned_ai(index)) ++parked;
    return parked;
}

static int slot_is_owned_ai(int32_t slot)
{
    for (int32_t index = 0; index < g_ai_count; ++index)
        if (g_ai_slots[index] == slot) return 1;
    return 0;
}

/* Game thread. Release = what the game does when a player leaves
 * (NetCommPlayerSlotSync 0x45FC50): controller -1, out of the active array,
 * then native vehicle unload 0x4C6E80 (scene/physics/UI, roster -> -1), so no
 * model stays on track. The entry is forgotten; prepare rebuilds the bot when
 * it is wanted again (host: field refill outside the race; client: manifest). */
typedef int(__attribute__((thiscall)) *VehicleNativeClearFn)(void *vehicle);
static const uintptr_t ADDR_VEHICLE_NATIVE_CLEAR = 0x004C6E80;


/* AI setups (owner 2026-10-10 live: every CO-OP bot had an EMPTY setup path
 * veh+0x4794). The native loader AISettingsFileLoader 0x4333E0 (thiscall
 * 0x7C73E8) runs only in DynMan_Restart before the session's scene setup; bots
 * added later never got a setup, and the physics config (0x4E45B0, inside
 * SceneRenderUpdate) parsed only the .hdv defaults. Run the native loader
 * (vehicle-specific <track>.<qual|race>.svm / <track>.svm / ai.svm, else the
 * game default) before each bot's scene setup. It walks every active non-player
 * car and only rewrites setup paths. */
typedef int(__attribute__((thiscall)) *AISettingsFileLoaderFn)(void *self);
static const uintptr_t ADDR_AI_SETTINGS_LOADER = 0x004333E0;
static const uintptr_t ADDR_AI_SETTINGS_OWNER = 0x007C73E8;
static void load_ai_setup_files(F1CVehicle *vehicle)
{
    static volatile LONG logged;
    ((AISettingsFileLoaderFn)ADDR_AI_SETTINGS_LOADER)((void *)ADDR_AI_SETTINGS_OWNER);
    if (vehicle && InterlockedIncrement(&logged) <= 30)
        log_line("ai setup: %s -> %s", vehicle->DriverName, (const char *)((uint8_t *)vehicle + 0x4794));
}

/* 2026-10-11 crash fix (Kornel logs: every client crash process released bots
 * first). Native unload NetCommPlayerSlotSync 0x45FC50 (l.4111-4164) moves the
 * VIEWED car pointer 0x7C54E0 off a car before unloading it: back to the
 * player's car (SetPlayerVehicleIndex 0x42BBD0) or, if that car is gone, to the
 * first car whose controller (+0x32C8) != -1. BuildActiveVehiclePointerArray
 * 0x42BC80 only re-derives the index 0x7C54DC from that pointer. release_bot
 * skipped this, so camera/sound/HUD could keep using an unloaded car's freed
 * scene objects (use-after-free -> random heap corruption). */
static const uintptr_t ADDR_VIEW_VEHICLE = 0x007C54E0;
static const uintptr_t ADDR_VIEW_INDEX = 0x007C54DC;
static const uintptr_t ADDR_ACTIVE_VEHICLES = 0x007C54F0;
static const uintptr_t ADDR_SET_VIEW_VEHICLE = 0x0042BBD0;
static const uintptr_t ADDR_PLAYER_VEHICLE_PTR = 0x007C54E8;
typedef int(__attribute__((stdcall)) *SetViewVehicleFn)(int32_t slot);
static volatile LONG g_view_retargets;

static void retarget_view_off(F1CVehicle *leaving)
{
    uint8_t *view = *(uint8_t *volatile *)ADDR_VIEW_VEHICLE;
    if (view != (uint8_t *)leaving) return;
    uint8_t *player = *(uint8_t *volatile *)ADDR_PLAYER_VEHICLE_PTR;
    if (player && player != (uint8_t *)leaving && *(int32_t *)(player + 0x32C8) != -1) {
        ((SetViewVehicleFn)ADDR_SET_VIEW_VEHICLE)(*(int32_t *)(player + 0x4894));
    } else {
        uint8_t **active = (uint8_t **)ADDR_ACTIVE_VEHICLES;
        for (int32_t i = 0; i < f1c_vehicle_count_get() && i < MAX_VEHICLES; ++i) {
            uint8_t *car = active[i];
            if (car && car != (uint8_t *)leaving && *(int32_t *)(car + 0x32C8) != -1) {
                *(volatile int32_t *)ADDR_VIEW_INDEX = i;
                *(uint8_t *volatile *)ADDR_VIEW_VEHICLE = car;
                break;
            }
        }
    }
    InterlockedIncrement(&g_view_retargets);
}

static void release_bot(int32_t index, const char *reason)
{
    F1CVehicle *vehicle = owned_ai(index);
    if (!vehicle) return;
    char name[64];
    lstrcpynA(name, vehicle->DriverName, sizeof(name));
    int32_t slot = g_ai_slots[index];
    int viewed = *(uint8_t *volatile *)ADDR_VIEW_VEHICLE == (uint8_t *)vehicle;
    retarget_view_off(vehicle);
    /* native order (0x45FC50 l.4157-4158): unload, then controller -1 */
    ((VehicleNativeClearFn)ADDR_VEHICLE_NATIVE_CLEAR)(vehicle);
    game::SessionStateTransition(f1c_vehicle_controller(vehicle), -1, 0);
    commands::set_WearTickGate__was__slot_live_marker_(vehicle, -1);
    f1c_vehicle_count_set(game::BuildActiveVehiclePointerArray());
    if (viewed) log_line("released bot: view moved off slot %ld before unload", (long)slot);
    g_ai_slots[index] = -1;
    g_ai_roster_indices[index] = -1;
    g_ai_roles[index] = -1;
    g_ai_paths[index][0] = 0;
    InterlockedExchange(&g_ai_parked[index], 0);
    compact_ai_entries();
    log_line("released bot: slot=%ld name=%s (%s) roster_after=%ld total=%ld", (long)slot, name, reason,
             (long)vehicle->VehicleRosterIndex, (long)f1c_vehicle_count_get());
}

static int roster_driven_by_human(int32_t roster_index)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (vehicle && vehicle->WearTickGate__was__slot_live_marker_ != -1 &&
            !slot_is_owned_ai(slot) && vehicle->VehicleRosterIndex == roster_index)
            return 1;
    }
    return 0;
}

/* Host: a human in a bot's car replaces that bot. When the human leaves the
 * bot returns, except during the race (retire path for host bots: OPEN). */
static void update_car_replacements(void)
{
    static LONG race_hold_logged;
    int32_t session_type = *(volatile int32_t *)ADDR_SESSION_TYPE;
    (void)race_hold_logged;
    (void)session_type;
    for (int32_t index = g_ai_count - 1; index >= 0; --index) {
        if (!owned_ai(index)) continue;
        if (roster_driven_by_human(g_ai_roster_indices[index]))
            release_bot(index, "a human took this car");
    }
}


/* 2026-10-11 crash fix: the native slot LOAD (0x45FC50, 0x45FD57-0x45FD87)
 * resets the slot's netcomm state before building a car; coop's own builds
 * did not, so a reused slot kept the previous car's record double-buffer and
 * per-slot counters. netcomm = *(0x7CD25C); all three are thiscall. */
typedef void(__attribute__((thiscall)) *SlotResetFn)(void *self);
typedef void(__attribute__((thiscall)) *SlotArrayResetFn)(void *self, int32_t slot);
static const uintptr_t ADDR_NET_SLOT_RESET = 0x0045C030;        /* netcomm + 0x1B7A0 + 0x38*slot */
static const uintptr_t ADDR_NET_RECORD_BUFFER_RESET = 0x004FFFA0; /* netcomm + 0x4180 + 0x368*slot */
static const uintptr_t ADDR_NET_SLOT_ARRAY_RESET = 0x0045C850;  /* netcomm + 0x1A3C0, slot */
static void reset_net_slot(int32_t slot)
{
    uint8_t *netcomm = *(uint8_t *volatile *)0x007CD25C;
    if (!netcomm || slot < 0 || slot >= MAX_VEHICLES) return;
    ((SlotResetFn)ADDR_NET_SLOT_RESET)(netcomm + 0x1B7A0 + 0x38 * slot);
    ((SlotResetFn)ADDR_NET_RECORD_BUFFER_RESET)(netcomm + 0x4180 + 0x368 * slot);
    ((SlotArrayResetFn)ADDR_NET_SLOT_ARRAY_RESET)(netcomm + 0x1A3C0, slot);
}

/* 2026-10-12 live fix: the join pre-release alone looped (host log 21:20:32,
 * Burti released 9x in 1.1 s: host field refill rebuilt the bot between native
 * slot-sync passes). Car file of a player in the native player table
 * (*(0x7CD25C)+0x3D0, stride 140, count +0x3E14, entry = .veh file name)? */
static int vehicle_file_in_player_table(const char *path)
{
    uint8_t *nc = *(uint8_t *volatile *)0x007CD25C;
    if (!nc || !path || !path[0]) return 0;
    const char *file = path;
    for (const char *p = path; *p; ++p) if (*p == '\\' || *p == '/') file = p + 1;
    int32_t count = *(int32_t *)(nc + 0x3E14);
    if (count <= 0 || count > MAX_VEHICLES) return 0;
    for (int32_t i = 0; i < count; ++i) {
        const char *entry = (const char *)(nc + 0x3D0 + 140 * i);
        size_t len = strnlen(entry, 32);
        if (len > 0 && len < 32 && lstrcmpiA(entry, file) == 0) return 1;
    }
    return 0;
}

/* Client: mirror the host's verified manifest - build missing bots in the same
 * slots, park bots the host no longer lists, restore ones it lists again. */
static const uintptr_t ADDR_PLAYER_VEHICLE = 0x007C54E8;  /* local player vehicle pointer */
static void prepare_client_from_manifest(void)
{
    static LONG refused_logged;
    static int32_t slots[MAX_AI];
    static char paths[MAX_AI][MAX_PATH];
    static char names[MAX_AI][64];
    int32_t count;
    LONG generation;
    if (g_manifest_refused) {
        if (InterlockedExchange(&refused_logged, 1) == 0)
            log_line("COOP-E10 no bots on this client: host content differs (see lines above)");
        return;
    }
    InterlockedExchange(&refused_logged, 0);
    if (!g_manifest_ready || g_manifest_generation == g_manifest_applied) return;
    {   /* Garage box (Tester A log 10:33:53-54): the client built every host bot,
         * incl. the one for its own car, BEFORE its own car entered; the garage
         * picker 0x4C6F10 then found that team full and used the Ferrari garage.
         * Build bots only once the local player car is active; the roster check
         * below then skips the bot for the car this player drives. */
        static LONG wait_logged;
        uint8_t *local = *(uint8_t *volatile *)ADDR_PLAYER_VEHICLE;
        if (!local || *(int32_t *)(local + 0x32C8) != 0) {
            if (!InterlockedExchange(&wait_logged, 1))
                log_line("client field: waiting for the local car before building host bots");
            return;
        }
        InterlockedExchange(&wait_logged, 0);
    }
    EnterCriticalSection(&g_manifest_lock);
    generation = g_manifest_generation;
    count = g_manifest_count;
    for (int32_t i = 0; i < count; ++i) {
        slots[i] = g_manifest_slots[i];
        lstrcpynA(paths[i], g_manifest_paths[i], MAX_PATH);
        lstrcpynA(names[i], g_manifest_names[i], 64);
    }
    LeaveCriticalSection(&g_manifest_lock);

    /* Park local bots the host no longer lists; restore listed parked ones. */
    for (int32_t index = g_ai_count - 1; index >= 0; --index) {
        int listed = 0;
        for (int32_t i = 0; i < count; ++i)
            if (slots[i] == g_ai_slots[index]) listed = 1;
        if (!listed) release_bot(index, "host removed it");
    }

    int32_t previous_roster_index = f1c_vehicle_lookup_index_get();
    int32_t built = 0;
    for (int32_t entry = 0; entry < count; ++entry) {
        int32_t slot = slots[entry];
        if (slot_is_owned_ai(slot)) continue;
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle) continue;
        if (vehicle->WearTickGate__was__slot_live_marker_ != -1) {
            log_line("COOP-E11 host bot slot %ld is busy on this client; %s not built",
                     (long)slot, names[entry]);
            continue;
        }
        if (vehicle_file_in_player_table(paths[entry])) {
            log_line("manifest bot slot %ld skipped: a player is joining in %s", (long)slot, names[entry]);
            continue;
        }
        f1c_vehicle_lookup_index_set(-1);
        game::VehicleLookupByName(paths[entry]);
        int32_t roster_index = f1c_vehicle_lookup_index_get();
        if (roster_index < 0) {
            log_line("COOP-E11 host bot vehicle not in this client's catalog: %s", paths[entry]);
            continue;
        }
        if (roster_driven_by_human(roster_index)) {
            log_line("manifest bot slot %ld skipped: a human drives %s here", (long)slot, names[entry]);
            continue;
        }
        reset_net_slot(slot);
        game::SessionStateTransition(f1c_vehicle_controller(vehicle), 2, 0);
        game::NationalityDriverInfoLoader(vehicle, roster_index);
        game::TeamIDSetter(vehicle, slot);
        f1c_vehicle_count_set(f1c_vehicle_count_get() + 1);
        game::BuildActiveVehiclePointerArray();
        game::VehgenScnFileLoader(vehicle, 1);
        load_ai_setup_files(vehicle);
        game::SceneRenderUpdate(vehicle, 1);

        int32_t index = g_ai_count;
        g_ai_slots[index] = slot;
        g_ai_roster_indices[index] = roster_index;
        g_ai_roles[index] = 2;
        g_ai_parked[index] = 0;
        lstrcpynA(g_ai_paths[index], paths[entry], MAX_PATH);
        InterlockedExchange(&g_ai_count, index + 1);
        ++built;
        /* 2026-10-12: building resets bestLap to -1; give the host's time at once
         * (the manifest was just verified) so a mid-session joiner's table does not
         * show bots without times until the periodic fill. */
        if (g_host_best_ms[slot] > 0 && g_host_best_ms[slot] < 3600000 && vehicle->bestLap <= 0.0f)
            vehicle->bestLap = g_host_best_ms[slot] / 1000.0f;
        if (lstrcmpA(vehicle->DriverName, names[entry]) != 0)
            log_line("COOP-E11 driver differs in slot %ld: host '%s', client '%s'",
                     (long)slot, names[entry], vehicle->DriverName);
        log_line("prepared AI from host: index=%ld slot=%ld roster=%ld name=%s",
                 (long)index, (long)slot, (long)roster_index, vehicle->DriverName);
    }
    f1c_vehicle_lookup_index_set(previous_roster_index);
    f1c_vehicle_count_set(game::BuildActiveVehiclePointerArray());
    g_manifest_applied = generation;
    {
        int32_t st = *(volatile int32_t *)ADDR_SESSION_TYPE;
        if (st >= 1 && st <= 6) hooks::apply_entry_order(st);
    }
    log_line("client field from host manifest: built=%ld active_bots=%ld parked=%ld total=%ld",
             (long)built, (long)(owned_ai_count() - parked_ai_count()), (long)parked_ai_count(),
             (long)f1c_vehicle_count_get());
    if (built && InterlockedExchange(&g_registration_refreshed, 1) == 0)
        f1c::reviewed::game_thread::CommentaryNameListRebuild(G_CommentaryNameList);
}

/* COOP-R01 (2026-10-13): the mod kept its bots registered after the game left CO-OP,
 * so the post-race replay ran with 22 hidden bots (investigation Fix 2026-10-13).
 * Release them once per inert run: game mode is not 5 with CO-OP (COOP-I02), no
 * network session (state 0, the post-race replay log showed net_state=0) and the
 * inert run has lasted COOP_R01_HOLD_MS. A live CO-OP race is never inert. */
static const uint32_t COOP_R01_HOLD_MS = 5000u;
static volatile LONG g_inert_started;     /* 1 while an inert run is being timed */
static volatile LONG g_inert_since_ms;    /* GetTickCount at the start of the inert run */
static volatile LONG g_inert_released;    /* 1 after COOP-R01 for this inert run */

static void release_bots_after_leaving_coop(void)
{
    uint32_t now = (uint32_t)GetTickCount();
    if (InterlockedExchange(&g_inert_started, 1) == 0)
        InterlockedExchange(&g_inert_since_ms, (LONG)now);
    if (g_inert_released) return;
    int32_t owned = owned_ai_count();
    if (!coop_bot_release_due(1, network_state(), owned, (uint32_t)g_inert_since_ms, now,
                              COOP_R01_HOLD_MS))
        return;
    InterlockedExchange(&g_inert_released, 1);
    int32_t released = 0;
    for (int32_t index = g_ai_count - 1; index >= 0; --index) {
        if (!owned_ai(index)) continue;
        release_bot(index, "left CO-OP (COOP-R01)");
        ++released;
    }
    f1c_vehicle_count_set(game::BuildActiveVehiclePointerArray());
    log_line("COOP-R01 released %ld bot(s) after leaving CO-OP (game mode %ld, network state 0 for %lu ms)",
             (long)released, (long)*game_mode_ptr(), (unsigned long)(now - (uint32_t)g_inert_since_ms));
}

static void prepare_ai_field(void)
{
    int32_t local_slot = local_human_slot();
    int32_t required = local_slot == 0 ? g_min_humans : g_expected_humans;

    /* CO-OP gate: plain MULTIPLAYER stays vanilla (no bots). */
    if (!coop_mode_is_selected()) {
        if (InterlockedExchange(&g_inert_logged, 1) == 0)
            log_line("COOP-I02 inert: game mode %ld without CO-OP selected",
                     (long)*game_mode_ptr());
        release_bots_after_leaving_coop();
        return;
    }
    InterlockedExchange(&g_inert_logged, 0);
    InterlockedExchange(&g_inert_started, 0);
    InterlockedExchange(&g_inert_released, 0);
    if (InterlockedExchange(&g_prepare_running, 1) != 0) return;
    if (g_stopping) {
        InterlockedExchange(&g_prepare_running, 0);
        return;
    }

    /* Settled field: last full pass added nothing and nothing changed since.
     * Prevents a per-frame catalog walk (and 1M log lines) when a human car
     * occupies one of the season entries. */
    if (g_settled_humans == human_slot_count() && g_settled_owned == owned_ai_count() &&
        !(network_state() == 3 && g_manifest_ready && g_manifest_generation != g_manifest_applied)) {
        InterlockedExchange(&g_prepare_running, 0);
        return;
    }
    if (g_refill_defer_session >= 0 && *(volatile int32_t *)ADDR_SESSION_TYPE == g_refill_defer_session) {
        /* Owner 2026-10-10: a player leaving froze the game ~130 ms (host log
         * 19:24:31: rebuilding the returned bot = car model, setup and scene
         * load on the game thread; clients then build it too). The bot returns
         * at the next session load instead, where loading is expected. */
        InterlockedExchange(&g_prepare_running, 0);
        return;
    }
    g_refill_defer_session = -1;
    if (network_state() == 3) {           /* CO-OP client: host manifest only */
        prepare_client_from_manifest();
        g_settled_humans = human_slot_count();
        g_settled_owned = owned_ai_count();
        InterlockedExchange(&g_prepare_running, 0);
        return;
    }
    const int season_list = g_use_season_bots && build_season_candidates(local_slot) > 0;
    if (season_list) g_requested_ai = g_season_count;
    if (owned_ai_count() >= g_requested_ai) {
        InterlockedExchange(&g_prepare_running, 0);
        return;
    }
    if (g_ai_count > 0 && *(volatile int32_t *)ADDR_SESSION_TYPE == 7) {
        /* Human left during the race: no bot refill mid-race (retire path pending). */
        InterlockedExchange(&g_prepare_running, 0);
        return;
    }
    if (f1c_vehicle_count_get() < required || active_slot_count() < required) {
        InterlockedExchange(&g_prepare_running, 0);
        return;
    }

    int32_t previous_roster_index = f1c_vehicle_lookup_index_get();
    int32_t ai_role = local_slot == 0 ? 1 : 2;
    log_line("authority selection: pid=%lu local_human_slot=%ld testera_role=%ld "
             "humans=%ld required=%ld bots=%ld/%ld",
             (unsigned long)GetCurrentProcessId(), (long)local_slot, (long)ai_role,
             (long)active_slot_count(), (long)required,
             (long)g_ai_count, (long)g_requested_ai);
    const size_t candidate_count = season_list ? (size_t)g_season_count
        : g_selected_ai_count > 0
        ? (size_t)g_selected_ai_count
        : sizeof(g_2001_candidates) / sizeof(g_2001_candidates[0]);
    for (size_t candidate = 0; candidate < candidate_count; ++candidate) {
        if (owned_ai_count() >= g_requested_ai) break;
        const char *candidate_path = season_list ? g_season_paths[candidate]
            : g_selected_ai_count > 0
            ? g_selected_ai_paths[candidate]
            : g_2001_candidates[candidate];
        /* Season mode always fills the full grid; a joining human takes a bot's
         * car via release_bot. [HumanCars] reservation applies to the ini list only. */
        if (!season_list && path_is_reserved_for_human(candidate_path)) {
            log_line("skipped reserved human car: path=%s", candidate_path);
            continue;
        }
        if (vehicle_file_in_player_table(candidate_path)) {
            /* Keep the car's roster for apply_entry_order: the human in this
             * car takes its car-number place (else car_number_of -> 1000 = last). */
            if (season_list) {
                f1c_vehicle_lookup_index_set(-1);
                game::VehicleLookupByName(candidate_path);
                g_season_rosters[candidate] = f1c_vehicle_lookup_index_get();
            }
            static LONG join_skip_logged;
            if (InterlockedIncrement(&join_skip_logged) <= 20)
                log_line("skipped bot: a player is joining in this car: path=%s", candidate_path);
            continue;
        }
        f1c_vehicle_lookup_index_set(-1);
        game::VehicleLookupByName(candidate_path);
        int32_t roster_index = f1c_vehicle_lookup_index_get();
        if (season_list) g_season_rosters[candidate] = roster_index;
        if (roster_index < 0) {
            log_line("skipped unresolved bot vehicle: path=%s", candidate_path);
            continue;
        }
        if (roster_is_already_active(roster_index)) {
            log_line("skipped duplicate active car: roster=%ld path=%s",
                     (long)roster_index, candidate_path);
            continue;
        }
        int32_t slot = first_inactive_slot();
        if (slot < 0) break;
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        reset_net_slot(slot);
        game::SessionStateTransition(f1c_vehicle_controller(vehicle), ai_role, 0);
        game::NationalityDriverInfoLoader(vehicle, roster_index);
        game::TeamIDSetter(vehicle, slot);
        f1c_vehicle_count_set(f1c_vehicle_count_get() + 1);
        game::BuildActiveVehiclePointerArray();
        game::VehgenScnFileLoader(vehicle, 1);
        load_ai_setup_files(vehicle);
        game::SceneRenderUpdate(vehicle, 1);

        int32_t index = g_ai_count;
        g_ai_slots[index] = slot;
        g_ai_roster_indices[index] = roster_index;
        g_ai_roles[index] = ai_role;
        lstrcpynA(g_ai_paths[index], candidate_path, MAX_PATH);
        InterlockedExchange(&g_ai_count, index + 1);
        log_line("prepared AI: pid=%lu index=%ld slot=%ld roster=%ld name=%s",
                 (unsigned long)GetCurrentProcessId(), (long)index, (long)slot,
                 (long)roster_index, vehicle->DriverName);
    }
    f1c_vehicle_lookup_index_set(previous_roster_index);
    game::BuildActiveVehiclePointerArray();
    g_settled_humans = human_slot_count();
    g_settled_owned = owned_ai_count();
    {
        int32_t st = *(volatile int32_t *)ADDR_SESSION_TYPE;
        if (st >= 1 && st <= 6) hooks::apply_entry_order(st);
    }
    log_line("prepared selected co-op field: pid=%lu humans=%ld ai=%ld requested=%ld total=%ld",
             (unsigned long)GetCurrentProcessId(), (long)human_slot_count(),
             (long)owned_ai_count(), (long)g_requested_ai,
             (long)f1c_vehicle_count_get());
    if (InterlockedExchange(&g_registration_refreshed, 1) == 0) {
        int32_t rebuild = f1c::reviewed::game_thread::CommentaryNameListRebuild(
            G_CommentaryNameList);
        log_line("commentary name registration refreshed: result=%ld",
                 (long)rebuild);
    }
    InterlockedExchange(&g_prepare_running, 0);
}

/* Build short-name candidates for the commentary driver-id table from a
 * year-suffixed catalog name such as "Michael Schumacher (2002)". The stock
 * lookup only matches short forms ("Michael S", "Juan", ...). */
static int build_callout_candidates(const char *full, char out[4][64])
{
    char base[64];
    char *year;
    int length;
    int written = 0;

    lstrcpynA(base, full, sizeof(base));
    year = strstr(base, " (20");
    if (year) *year = 0;
    length = (int)strlen(base);
    while (length > 0 && base[length - 1] == ' ') base[--length] = 0;
    if (length == 0) return 0;
    lstrcpynA(out[written++], base, 64);

    {
        const char *space = strchr(base, ' ');
        if (space)
        {
            char first[64];
            char last[64];
            size_t first_length = (size_t)(space - base);
            const char *cursor = space + 1;
            const char *next;
            char with_initial[64];

            if (first_length >= sizeof(first)) first_length = sizeof(first) - 1;
            memcpy(first, base, first_length);
            first[first_length] = 0;
            while ((next = strchr(cursor, ' ')) != NULL) cursor = next + 1;
            lstrcpynA(last, cursor, sizeof(last));

            wsprintfA(with_initial, "%s %c", first, last[0]);
            lstrcpynA(out[written++], with_initial, 64);

            next = strchr(space + 1, ' ');
            if (next)
            {
                char first_two[64];
                size_t two_length = (size_t)(next - base);
                if (two_length >= sizeof(first_two))
                    two_length = sizeof(first_two) - 1;
                memcpy(first_two, base, two_length);
                first_two[two_length] = 0;
                lstrcpynA(out[written++], first_two, 64);
            }
            lstrcpynA(out[written++], first, 64);
        }
    }
    return written;
}

namespace hooks {

int32_t __attribute__((stdcall)) driver_name_lookup_hook(
    int32_t *entry_field, int32_t record_index, uint32_t context,
    int8_t variant_a, int8_t variant_b)
{
    int32_t result = original_DriverNameIDLookup()(entry_field, record_index,
        context, variant_a, variant_b);

    if (result < 0 || !entry_field) return result;
    if (entry_field[2] > 0) {
        if (record_index >= 0) {
            uint8_t *stock_catalog = *(uint8_t **)G_VehicleCatalog;
            int32_t stock_count = *(int32_t *)G_VehicleCatalogCount;
            if (stock_catalog && record_index < stock_count) {
                char *stock_name = (char *)(stock_catalog
                    + (size_t)record_index * 1060 + 188);
                if (strstr(stock_name, " (20") != NULL)
                    log_line("callout stock resolved: index=%ld name='%s' id=%ld",
                             (long)record_index, stock_name,
                             (long)entry_field[2]);
            }
        }
        return result;
    }
    if (record_index < 0 || g_stopping) return result;

    {
        uint8_t *catalog = *(uint8_t **)G_VehicleCatalog;
        int32_t count = *(int32_t *)G_VehicleCatalogCount;
        char *name;
        char original[64];
        char candidates[4][64];
        int candidate_count;
        const char *matched = NULL;
        int index;

        if (!catalog || record_index >= count) return result;
        name = (char *)(catalog + (size_t)record_index * 1060 + 188);
        if (!name[0] || strstr(name, " (20") == NULL) return result;

        lstrcpynA(original, name, sizeof(original));
        candidate_count = build_callout_candidates(original, candidates);
        for (index = 0; index < candidate_count; ++index)
        {
            lstrcpynA(name, candidates[index], 64);
            original_DriverNameIDLookup()(entry_field, record_index, context,
                variant_a, variant_b);
            if (entry_field[2] >= 0)
            {
                matched = candidates[index];
                break;
            }
        }
        lstrcpynA(name, original, 64);
        if (matched)
            log_line("callout name resolved: index=%ld full='%s' via='%s' id=%ld",
                     (long)record_index, original, matched,
                     (long)entry_field[2]);
        else
            log_line("callout name unresolved: index=%ld full='%s'",
                     (long)record_index, original);
    }
    return result;
}

void __attribute__((thiscall)) commentary_event_hook(void *entry, int32_t event_id,
    int32_t driver1, int32_t driver2, int32_t arg5, int32_t arg6,
    float arg7, float arg8)
{
    original_CommentaryEventQueue()(entry, event_id, driver1, driver2, arg5, arg6,
        arg7, arg8);
    if (g_stopping || InterlockedIncrement(&g_event_log_count) > 4000) return;
    if (driver1 < 0 && driver2 < 0) return;
    log_line("commentary event: pid=%lu eid=%ld d1=%ld d2=%ld a5=%ld a6=%ld",
             (unsigned long)GetCurrentProcessId(), (long)event_id,
             (long)driver1, (long)driver2, (long)arg5, (long)arg6);
}

int32_t __attribute__((thiscall)) commentary_entry_hook(void *entry)
{
    int32_t result = original_CommentaryDriverEntryInit()(entry);

    if (g_stopping || !entry) return result;

    {
        int32_t index = *(int32_t *)((uint8_t *)entry + 8);
        uint8_t *catalog = *(uint8_t **)G_VehicleCatalog;
        int32_t count = *(int32_t *)G_VehicleCatalogCount;
        char *name;
        char original_name[64];
        char candidates[4][64];
        int candidate_count;
        int resolved = 0;
        int slot;

        if (!catalog || index < 0 || index >= count) return result;
        name = (char *)(catalog + (size_t)index * 1060 + 188);
        if (!name[0] || strstr(name, " (20") == NULL) return result;

        lstrcpynA(original_name, name, sizeof(original_name));
        candidate_count = build_callout_candidates(original_name, candidates);
        if (candidate_count <= 0) return result;

        for (slot = 0; slot < 8; ++slot) {
            static const uint8_t contexts[8][3] = {
                {0, 1, 1}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0},
                {1, 1, 1}, {1, 1, 0}, {1, 0, 1}, {1, 0, 0},
            };
            int32_t *field = (int32_t *)((uint8_t *)entry
                + (slot < 4 ? 16 : 96) + 20 * (slot & 3));
            int candidate;
            if (field[2] > 0) continue;
            for (candidate = 0; candidate < candidate_count; ++candidate) {
                lstrcpynA(name, candidates[candidate], 64);
                f1c::reviewed::game_thread::DriverNameIDLookup(field, index,
                    contexts[slot][0], contexts[slot][1], contexts[slot][2]);
                if (field[2] > 0) {
                    ++resolved;
                    break;
                }
            }
        }
        lstrcpynA(name, original_name, 64);
        if (resolved > 0)
            log_line("callout entry resolved: index=%ld full='%s' slots=%ld",
                     (long)index, original_name, (long)resolved);
    }
    return result;
}

char * __attribute__((thiscall)) process_driver_entries_hook(void *entries)
{
    F1CVehicle *vehicles[MAX_AI] = {};
    LONG slots[MAX_AI] = {};
    LONG rosters[MAX_AI] = {};
    char *result;
    int32_t before_count;
    InterlockedIncrement(&g_active_calls);

    before_count = g_ai_count;
    /* COOP-R01: outside an active CO-OP session no bot is hidden or restored, so the
     * replay and menu see the stock field (the bots are released by prepare_ai_field). */
    if (!coop_mode_is_selected()) before_count = 0;
    for (int32_t index = 0; index < before_count; ++index) {
        slots[index] = g_ai_slots[index];
        rosters[index] = g_ai_roster_indices[index];
        vehicles[index] = owned_ai(index);
        if (!g_stopping && vehicles[index] && !g_ai_parked[index])
            commands::set_WearTickGate__was__slot_live_marker_(vehicles[index], -1);
    }
    if (!g_stopping && before_count > 0) {
        int32_t visible_count = game::BuildActiveVehiclePointerArray();
        f1c_vehicle_count_set(visible_count);
        log_line("driver processing mask: pid=%lu stock_count=%ld ai_hidden=%ld",
                 (unsigned long)GetCurrentProcessId(), (long)visible_count,
                 (long)before_count);
    }

    result = original_ProcessDriverEntries()(entries);

    if (!g_stopping && before_count > 0) {
        int32_t restored = 0;
        for (int32_t index = 0; index < before_count; ++index) {
            F1CVehicle *vehicle = vehicles[index];
            if (vehicle && g_ai_parked[index]) continue;
            if (vehicle && slots[index] == g_ai_slots[index] &&
                rosters[index] == g_ai_roster_indices[index] &&
                vehicle->VehicleRosterIndex == rosters[index]) {
                commands::set_WearTickGate__was__slot_live_marker_(vehicle, g_ai_roles[index]);
                ++restored;
            } else {
                log_line("AI ownership lost during driver processing: index=%ld slot=%ld roster=%ld",
                         (long)index, (long)slots[index], (long)rosters[index]);
                g_ai_slots[index] = -1;
                g_ai_roster_indices[index] = -1;
                g_ai_roles[index] = -1;
            }
        }
        compact_ai_entries();
        int32_t active_count = game::BuildActiveVehiclePointerArray();
        f1c_vehicle_count_set(active_count);
        log_line("driver processing restore: pid=%lu ai_restored=%ld active_total=%ld",
                 (unsigned long)GetCurrentProcessId(),
                 (long)restored, (long)f1c_vehicle_count_get());
    }
    if (!g_stopping)
        prepare_ai_field();
    InterlockedDecrement(&g_active_calls);
    return result;
}

/* Build the race grid from the qualifying classification. Preference order:
 * 1. qualifying best time (0x405C): timed cars sorted ascending, untimed cars
 *    after all of them (a driver with no time starts last);
 * 2. the native qualifying position field (0x4060) when it is a complete
 *    permutation of the active cars;
 * 3. otherwise the pre-assignment planned positions (previous behaviour). */
static int slot_is_human(int32_t slot)
{
    for (int32_t index = 0; index < g_ai_count; ++index)
        if (g_ai_slots[index] == slot) return 0;
    return 1;
}

static int build_qualifying_grid(int32_t *desired, const int32_t *previous,
                                  int32_t active)
{
    int32_t slots[MAX_VEHICLES];
    int32_t timed[MAX_VEHICLES];
    uint8_t seen[MAX_VEHICLES + 1] = {};
    int32_t slot;
    int32_t count = 0;
    int32_t timed_count = 0;
    int valid_positions = active > 0;

    for (slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        slots[count++] = slot;
    }
    if (count != active) return 0;

    float qtime[MAX_VEHICLES] = {};
    for (int32_t index = 0; index < count; ++index) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slots[index]);
        qtime[slots[index]] = g_quali_keep_valid ? g_quali_keep_time[slots[index]]
                                                 : vehicle->timeOfQualySession;
        if (qtime[slots[index]] > 0.0f) timed[timed_count++] = slots[index];
    }
    {   /* CO-OP: always rank. Timed cars by time; untimed after them, bots
         * before humans (a human without a time starts at the back). */
        for (int32_t outer = 1; outer < timed_count; ++outer) {
            int32_t key = timed[outer];
            int32_t inner = outer - 1;
            while (inner >= 0) {
                if (qtime[timed[inner]] <= qtime[key])
                    break;
                timed[inner + 1] = timed[inner];
                --inner;
            }
            timed[inner + 1] = key;
        }
        {
            int32_t position = 1;
            for (int32_t index = 0; index < timed_count; ++index)
                desired[timed[index]] = position++;
            for (int32_t rank = 1; rank <= active; ++rank) {
                int32_t best_slot = -1;
                for (int32_t index = 0; index < count; ++index) {
                    if (qtime[slots[index]] > 0.0f) continue;
                    if (desired[slots[index]] > 0) continue;
                    if (best_slot < 0) { best_slot = slots[index]; continue; }
                    int human = slot_is_human(slots[index]), best_human = slot_is_human(best_slot);
                    if (human != best_human) { if (!human) best_slot = slots[index]; continue; }
                    if (previous[slots[index]] < previous[best_slot])
                        best_slot = slots[index];
                }
                if (best_slot < 0) break;
                desired[best_slot] = position++;
            }
        }
        return 2;
    }

    for (int32_t index = 0; index < count; ++index) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slots[index]);
        int32_t position = vehicle->position;
        if (position < 1 || position > active || seen[position]) {
            valid_positions = 0;
            break;
        }
        seen[position] = 1;
    }
    if (valid_positions) {
        for (int32_t index = 0; index < count; ++index) {
            F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slots[index]);
            desired[slots[index]] = vehicle->position;
        }
        return 1;
    }

    return 0;
}

/* Session start order = entry order by car Number (as single player shows it):
 * a human takes the place of the car he drives. Only before anyone has a time. */
static int32_t car_number_of(F1CVehicle *vehicle)
{
    for (int32_t i = 0; i < g_season_count; ++i)
        if (g_season_rosters[i] >= 0 && g_season_rosters[i] == vehicle->VehicleRosterIndex)
            return g_season_numbers[i];
    return 1000;
}

static void apply_entry_order(int32_t session_type)
{
    int32_t slots[MAX_VEHICLES];
    int32_t count = 0, carried_times = 0;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        if (vehicle->bestLap > 0.0f) ++carried_times;
        slots[count++] = slot;
    }
    if (count == 0) return;
    for (int32_t i = 1; i < count; ++i)
        for (int32_t j = i; j > 0; --j) {
            int32_t a = car_number_of((F1CVehicle *)f1c_vehicle_slot(slots[j]));
            int32_t b = car_number_of((F1CVehicle *)f1c_vehicle_slot(slots[j - 1]));
            if (a > b || (a == b && slots[j] > slots[j - 1])) break;
            int32_t t = slots[j]; slots[j] = slots[j - 1]; slots[j - 1] = t;
        }
    int32_t changed = 0;
    for (int32_t i = 0; i < count; ++i) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slots[i]);
        if (vehicle->position != i + 1 || vehicle->plannedPositionAtStart != i + 1) {
            commands::set_plannedPositionAtStart(vehicle, i + 1);
            commands::set_position(vehicle, i + 1);
            ++changed;
        }
    }
    log_line("entry order: session_type=%ld cars=%ld changed=%ld cars_with_best_lap=%ld",
             (long)session_type, (long)count, (long)changed, (long)carried_times);
}

/* "GRID slot:pos ..." for the host manifest (host: qualifying ranking of the
 * active field; empty when it cannot be built). Also used for logging. */
static int coop_grid_text(char *out, int capacity)
{
    int32_t desired[MAX_VEHICLES] = {}, previous[MAX_VEHICLES] = {}, active = 0;
    int length;
    out[0] = 0;
    if (capacity < 64) return 0;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        previous[slot] = vehicle->plannedPositionAtStart;
        ++active;
    }
    if (network_state() == 3) {           /* client: report what it holds */
        for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) desired[slot] = previous[slot];
    } else if (active <= 0 || build_qualifying_grid(desired, previous, active) != 2) {
        return 0;
    }
    length = wsprintfA(out, "GRID");
    for (int32_t slot = 0; slot < MAX_VEHICLES && length < capacity - 16; ++slot)
        if (desired[slot] > 0) length += wsprintfA(out + length, " %ld:%ld", (long)slot, (long)desired[slot]);
    length += wsprintfA(out + length, "\n");
    return length;
}

int32_t __attribute__((thiscall)) multiplayer_vehicle_placement_hook(
    void *session, int32_t session_type)
{
    int32_t result = original_MultiplayerVehiclePlacement()(session, session_type);

    if (InterlockedIncrement(&g_placement_log_count) <= 24)
        log_line("placement call: pid=%lu session_type=%ld humans=%ld bots=%ld",
                 (unsigned long)GetCurrentProcessId(), (long)session_type,
                 (long)human_slot_count(), (long)owned_ai_count());

    if (!g_stopping && session_type >= 1 && session_type <= 6 && coop_mode_is_selected())
        apply_entry_order(session_type);
    if (g_stopping || session_type != 7) return result;
    if (!g_race_grid_rewrite) {
        /* FACT (0x462A10): native MP race ordering sets every slot's grid
         * position to slot+1 and numbers only network players 1..N, so bots in
         * slots 32+ start on spots 33+. CO-OP grid (owner): qualifying order for
         * everyone, decided by the host. Host ranks (timed by time, untimed
         * bots, untimed humans last) and sends it in the manifest GRID line;
         * the client applies the host's grid. Fallback: bots right after the
         * humans in slot order. Written after the native call, before cars are
         * placed on their spots at race start. */
        int32_t desired[MAX_VEHICLES] = {};
        int32_t previous[MAX_VEHICLES] = {};
        int32_t active = 0, moved = 0;
        const char *source = "fallback";
        for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
            F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
            if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
            previous[slot] = vehicle->plannedPositionAtStart;
            ++active;
        }
        if (network_state() == 3) {
            int32_t matched = 0;
            EnterCriticalSection(&g_manifest_lock);
            for (int32_t i = 0; i < g_host_grid_count; ++i) {
                int32_t slot = g_host_grid_slot[i];
                if (slot < 0 || slot >= MAX_VEHICLES) continue;
                F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
                if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
                desired[slot] = g_host_grid_pos[i];
                ++matched;
            }
            LeaveCriticalSection(&g_manifest_lock);
            if (matched == active) source = "host";
            else memset(desired, 0, sizeof(desired));
            if (matched != active)
                log_line("COOP-W03 host grid covers %ld of %ld cars; fallback order", (long)matched, (long)active);
        } else if (build_qualifying_grid(desired, previous, active) == 2) {
            source = "qualifying";
        } else {
            memset(desired, 0, sizeof(desired));
        }
        if (desired[0] == 0 && strcmp(source, "fallback") == 0) {
            int32_t humans = 0, next;
            for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot)
                if (previous[slot] && !slot_is_owned_ai(slot)) ++humans;
            next = humans + 1;
            for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
                if (!previous[slot]) continue;
                desired[slot] = slot_is_owned_ai(slot) ? next++ : previous[slot];
            }
        }
        for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
            F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
            if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1 || desired[slot] <= 0) continue;
            if (vehicle->plannedPositionAtStart != desired[slot] || vehicle->position != desired[slot]) {
                commands::set_plannedPositionAtStart(vehicle, desired[slot]);
                commands::set_position(vehicle, desired[slot]);
                ++moved;
            }
        }
        {
            char text[1024];
            coop_grid_text(text, sizeof(text));
            log_line("race grid: source=%s cars=%ld moved=%ld %s", source, (long)active, (long)moved, text);
        }
        return result;
    }

    {
        int32_t previous[MAX_VEHICLES] = {};
        int32_t desired[MAX_VEHICLES] = {};
        int32_t active = 0;
        int32_t slot;

        for (slot = 0; slot < MAX_VEHICLES; ++slot) {
            F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
            if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1)
                continue;
            previous[slot] = vehicle->plannedPositionAtStart;
            ++active;
        }
        {
            F1CVehicle *player = (F1CVehicle *)f1c_vehicle_slot(0);
            int built = active > 0
                ? build_qualifying_grid(desired, previous, active) : 0;
            log_line("race grid data: pid=%lu active=%ld built=%d player_time=%.3f player_pos=%ld player_planned=%ld",
                     (unsigned long)GetCurrentProcessId(), (long)active, built,
                     player ? (double)player->timeOfQualySession : 0.0,
                     player ? (long)player->position : -1L,
                     player ? (long)player->plannedPositionAtStart : -1L);
            if (built == 2) {
                int32_t corrected = 0;
                for (slot = 0; slot < MAX_VEHICLES; ++slot) {
                    F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
                    if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1)
                        continue;
                    if (desired[slot] <= 0) continue;
                    if (vehicle->plannedPositionAtStart != desired[slot] ||
                        vehicle->position != desired[slot]) {
                        commands::set_plannedPositionAtStart(vehicle, desired[slot]);
                        commands::set_position(vehicle, desired[slot]);
                        ++corrected;
                    }
                }
                log_line("race grid set from qualifying times: pid=%lu vehicles=%ld corrected=%ld",
                         (unsigned long)GetCurrentProcessId(), (long)active,
                         (long)corrected);
            }
        }
    }
    return result;
}

uint8_t __attribute__((thiscall)) multiplayer_qualification_grid_hook(
    void *session_manager)
{
    int32_t positions[MAX_VEHICLES] = {};
    int32_t desired[MAX_VEHICLES] = {};
    uint8_t seen[MAX_VEHICLES + 1] = {};
    int32_t active = 0;
    int valid_permutation = 1;
    int have_qualifying = 0;
    const char *source = "none";
    uint8_t result;

    if (!g_race_grid_rewrite) {
        static LONG logged;
        if (InterlockedExchange(&logged, 1) == 0)
            log_line("qualification grid: native (RaceGridRewrite=0)");
        return original_MultiplayerQualificationGridAssignment()(session_manager);
    }
    InterlockedIncrement(&g_active_calls);
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        positions[slot] = vehicle->plannedPositionAtStart;
        ++active;
    }
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        int32_t position = positions[slot];
        if (position < 1 || position > active || seen[position]) {
            valid_permutation = 0;
            break;
        }
        seen[position] = 1;
    }

    result = original_MultiplayerQualificationGridAssignment()(session_manager);

    if (!g_stopping && active > 0) {
        have_qualifying = build_qualifying_grid(desired, positions, active);
        if (have_qualifying) {
            int32_t slot;
            source = have_qualifying == 2 ? "time" : "position";
            for (slot = 0; slot < MAX_VEHICLES; ++slot) {
                F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
                if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1)
                    continue;
                if (desired[slot] <= 0) continue;
                if (vehicle->plannedPositionAtStart != desired[slot]) {
                    void *grid_state = f1c_vehicle_grid_state(vehicle);
                    game::ConditionalFieldSetter(grid_state, desired[slot]);
                    game::FieldCopy(grid_state);
                }
            }
        }
    }

    if (!g_stopping && have_qualifying) {
        log_line("qualification grid rebuilt: pid=%lu source=%s vehicles=%ld call=%ld",
                 (unsigned long)GetCurrentProcessId(), source, (long)active,
                 (long)InterlockedIncrement(&g_grid_assignment_count));
    } else if (!g_stopping && valid_permutation && active == f1c_vehicle_count_get()) {
        int32_t corrected = 0;
        for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
            F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
            if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
            if (vehicle->plannedPositionAtStart != positions[slot]) {
                void *grid_state = f1c_vehicle_grid_state(vehicle);
                game::ConditionalFieldSetter(grid_state, positions[slot]);
                game::FieldCopy(grid_state);
                ++corrected;
            }
        }
        log_line("qualification grid preserved: pid=%lu vehicles=%ld corrected=%ld call=%ld",
                 (unsigned long)GetCurrentProcessId(), (long)active, (long)corrected,
                 (long)InterlockedIncrement(&g_grid_assignment_count));
    } else {
        log_line("qualification grid preservation skipped: pid=%lu active=%ld total=%ld permutation=%d",
                 (unsigned long)GetCurrentProcessId(), (long)active,
                 (long)f1c_vehicle_count_get(), valid_permutation);
    }
    InterlockedDecrement(&g_active_calls);
    return result;
}

static int car_is_human(uint8_t *car)
{
    int32_t role = *(int32_t *)(car + 0x32C8);
    return role == 0 || role == 2;
}
/* Garage box de-duplication (Tester A/Burti 2026-10-11, live host read: Tester A t1b2 =
 * Burti t1b2 while t2b2 was empty). MultiplayerVehiclePlacement 0x42CF10 re-runs
 * the native picker 0x4C6F10 for every car after our scene preset, so the preset
 * alone cannot hold. After every native pick, and once a second on the game
 * thread, any two active cars in one box are split: a human (role 0/2) keeps the
 * box, the other car moves to the first free bay 0/2 of a team already in use. */
static const uintptr_t ADDR_GARAGE_PICK = 0x004C6F10;
static const uint8_t GaragePick_expected[] = {0x83, 0xEC, 0x4C, 0x53, 0x55, 0x56, 0x8B, 0xF1};
static F1CDetour g_garage_pick_detour;
typedef void(__attribute__((thiscall)) *GaragePickFn)(void *vehicle);


static int32_t g_want_team[MAX_VEHICLES], g_want_bay[MAX_VEHICLES];   /* human's own team box; -1 none */
static volatile LONG g_want_init;

static void want_init(void)
{
    if (InterlockedExchange(&g_want_init, 1)) return;
    for (int32_t i = 0; i < MAX_VEHICLES; ++i) g_want_team[i] = g_want_bay[i] = -1;
}

static int box_holder(int32_t team, int32_t bay, uint8_t *except)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        uint8_t *car = (uint8_t *)f1c_vehicle_slot(slot);
        if (!car || car == except || *(int32_t *)(car + 0x32C8) == -1) continue;
        if (*(int32_t *)(car + VEH_GARAGE_TEAM) == team && *(int32_t *)(car + VEH_GARAGE_BAY) == bay)
            return slot;
    }
    return -1;
}

static int dedupe_garage_boxes(void)
{
    want_init();
    /* 1. a human whose own team box is free goes back there (native re-pick moved him) */
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        uint8_t *car = (uint8_t *)f1c_vehicle_slot(slot);
        if (!car || *(int32_t *)(car + 0x32C8) == -1 || !car_is_human(car) || g_want_team[slot] < 0) continue;
        int32_t team = g_want_team[slot], bay = g_want_bay[slot];
        if (*(int32_t *)(car + VEH_GARAGE_TEAM) == team && *(int32_t *)(car + VEH_GARAGE_BAY) == bay) continue;
        if (box_holder(team, bay, car) >= 0) continue;
        log_line("garage box: %s back to own team box team %ld bay %ld (was team %ld bay %ld)",
                 ((F1CVehicle *)car)->DriverName, (long)team, (long)bay,
                 (long)*(int32_t *)(car + VEH_GARAGE_TEAM), (long)*(int32_t *)(car + VEH_GARAGE_BAY));
        *(int32_t *)(car + VEH_GARAGE_TEAM) = team;
        *(int32_t *)(car + VEH_GARAGE_BAY) = bay;
    }
    /* 2. any box still shared: split it */
    uint8_t *cars[MAX_VEHICLES];
    int32_t n = 0, max_team = -1, moved = 0;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        uint8_t *car = (uint8_t *)f1c_vehicle_slot(slot);
        if (!car || *(int32_t *)(car + 0x32C8) == -1) continue;
        int32_t team = *(int32_t *)(car + VEH_GARAGE_TEAM);
        if (team > max_team) max_team = team;
        cars[n++] = car;
    }
    for (int32_t a = 0; a < n; ++a) {
        for (int32_t b = a + 1; b < n; ++b) {
            int32_t team = *(int32_t *)(cars[a] + VEH_GARAGE_TEAM), bay = *(int32_t *)(cars[a] + VEH_GARAGE_BAY);
            if (team < 0 || bay < 0 || *(int32_t *)(cars[b] + VEH_GARAGE_TEAM) != team ||
                *(int32_t *)(cars[b] + VEH_GARAGE_BAY) != bay) continue;
            uint8_t *mover = car_is_human(cars[b]) && !car_is_human(cars[a]) ? cars[a] : cars[b];
            uint8_t *keeper = mover == cars[a] ? cars[b] : cars[a];
            int32_t new_team = -1, new_bay = -1;
            for (int32_t t = 0; t <= max_team && new_team < 0; ++t) {
                for (int32_t pass = 0; pass < 2 && new_team < 0; ++pass) {
                    int32_t bb = pass ? 2 : 0, used = 0;
                    for (int32_t c = 0; c < n && !used; ++c)
                        used = *(int32_t *)(cars[c] + VEH_GARAGE_TEAM) == t &&
                               *(int32_t *)(cars[c] + VEH_GARAGE_BAY) == bb;
                    if (!used) { new_team = t; new_bay = bb; }
                }
            }
            if (new_team < 0) {
                static volatile LONG full_logged;
                if (InterlockedIncrement(&full_logged) <= 5)
                    log_line("garage box: %s and %s share team %ld bay %ld; no free box",
                             ((F1CVehicle *)keeper)->DriverName, ((F1CVehicle *)mover)->DriverName,
                             (long)team, (long)bay);
                continue;
            }
            *(int32_t *)(mover + VEH_GARAGE_TEAM) = new_team;
            *(int32_t *)(mover + VEH_GARAGE_BAY) = new_bay;
            ++moved;
            log_line("garage box: %s shared team %ld bay %ld with %s -> moved to team %ld bay %ld",
                     ((F1CVehicle *)mover)->DriverName, (long)team, (long)bay,
                     ((F1CVehicle *)keeper)->DriverName, (long)new_team, (long)new_bay);
        }
    }
    return moved;
}

void __attribute__((thiscall)) garage_pick_hook(void *vehicle)
{
    ((GaragePickFn)g_garage_pick_detour.trampoline)(vehicle);
    if (!g_stopping && coop_mode_is_selected() && network_state() != 0)
        dedupe_garage_boxes();
}

/* Garage box before the native scene setup (see VEH_GARAGE_TEAM). Host: a
 * joining human (role 2) takes the box of the bot on the same car. Client: its
 * own car (role 0) takes the box the host gave the bot on the same .veh; every
 * other car takes the host's box for its slot. */
static void preset_garage_box(F1CVehicle *vehicle, int32_t slot)
{
    uint8_t *car = (uint8_t *)vehicle;
    int32_t role = vehicle->WearTickGate__was__slot_live_marker_;
    int32_t team = -1, bay = -1;
    const char *how = NULL;
    if (coop_host_now()) {
        if (role != 2 || slot_is_owned_ai(slot)) return;
        for (int32_t index = 0; index < g_ai_count; ++index) {
            F1CVehicle *bot = owned_ai(index);
            if (!bot || g_ai_roster_indices[index] != vehicle->VehicleRosterIndex) continue;
            team = *(int32_t *)((uint8_t *)bot + VEH_GARAGE_TEAM);
            bay = *(int32_t *)((uint8_t *)bot + VEH_GARAGE_BAY);
            how = "host: box of the replaced bot";
            break;
        }
    } else if (network_state() == 3) {
        EnterCriticalSection(&g_manifest_lock);
        if (role == 0) {
            uint8_t *catalog = *(uint8_t **)G_VehicleCatalog;
            int32_t roster = vehicle->VehicleRosterIndex;
            const char *path = catalog && roster >= 0 && roster < *(int32_t *)G_VehicleCatalogCount
                ? *(const char **)(catalog + (size_t)roster * CATALOG_STRIDE) : NULL;
            for (int32_t i = 0; path && i < g_manifest_count; ++i) {
                int32_t bot_slot = g_manifest_slots[i];
                if (bot_slot >= 0 && bot_slot < MAX_VEHICLES && g_host_box_valid[bot_slot] &&
                    !lstrcmpiA(path, g_manifest_paths[i])) {
                    team = g_host_box_team[bot_slot]; bay = g_host_box_bay[bot_slot];
                    how = "client: own car takes the host box of the bot on this car";
                    break;
                }
            }
        }
        if (!how && g_host_box_valid[slot]) {
            team = g_host_box_team[slot]; bay = g_host_box_bay[slot];
            how = "client: host box";
        }
        LeaveCriticalSection(&g_manifest_lock);
    }
    if (!how || team < 0 || bay < 0) return;
    want_init();
    if (car_is_human(car)) { g_want_team[slot] = team; g_want_bay[slot] = bay; }
    /* Never hand out a box another active car holds (Tester A/Burti 2026-10-11):
     * skip the preset and let the native picker choose a free box. On the host
     * the replaced bot (same roster) is ignored: it leaves right after. */
    for (int32_t other = 0; other < MAX_VEHICLES; ++other) {
        uint8_t *held = (uint8_t *)f1c_vehicle_slot(other);
        if (!held || held == car || *(int32_t *)(held + 0x32C8) == -1) continue;
        if (((F1CVehicle *)held)->VehicleRosterIndex == vehicle->VehicleRosterIndex) continue;
        if (*(int32_t *)(held + VEH_GARAGE_TEAM) == team && *(int32_t *)(held + VEH_GARAGE_BAY) == bay) {
            log_line("garage box: slot %ld %s team %ld bay %ld already held by slot %ld %s; native pick",
                     (long)slot, vehicle->DriverName, (long)team, (long)bay, (long)other,
                     ((F1CVehicle *)held)->DriverName);
            return;
        }
    }
    *(int32_t *)(car + VEH_GARAGE_TEAM) = team;
    *(int32_t *)(car + VEH_GARAGE_BAY) = bay;
    static volatile LONG logged;
    if (InterlockedIncrement(&logged) <= 80)
        log_line("garage box: slot %ld %s -> team %ld bay %ld (%s)", (long)slot, vehicle->DriverName,
                 (long)team, (long)bay, how);
}

void __attribute__((thiscall)) scene_render_update_hook(F1CVehicle *vehicle,
                                                         uint8_t full_setup)
{
    int32_t slot = vehicle_slot_of(vehicle);
    LONG bit = slot >= 0 && slot < 31 ? (1L << slot) : 0;
    InterlockedIncrement(&g_active_calls);
    if (bit && !(InterlockedOr(&g_scene_entered_mask, bit) & bit))
        log_line("scene enter: pid=%lu slot=%ld role=%ld roster=%ld name=%s",
                 (unsigned long)GetCurrentProcessId(), (long)slot,
                 (long)vehicle->WearTickGate__was__slot_live_marker_,
                 (long)vehicle->VehicleRosterIndex, vehicle->DriverName);
    if (!g_stopping && coop_mode_is_selected() && slot >= 0 && slot < MAX_VEHICLES)
        preset_garage_box(vehicle, slot);
    /* 0.9.3-dev2 released the replaced bot here (before the garage pick). REMOVED
     * in dev3: inside the native scene setup the active-car rebuild returned 2,
     * release_bot cut the field (22 -> 5 bots) and the game later crashed in
     * the scene graph (0x545155, 2026-10-10 14:06). Garage-box fix pending
     * client-side evidence; client manifest skip kept. */
    original_SceneRenderUpdate()(vehicle, full_setup);
    if (bit && !(InterlockedOr(&g_scene_exited_mask, bit) & bit))
        log_line("scene exit: pid=%lu slot=%ld box=%ld:%ld %s", (unsigned long)GetCurrentProcessId(),
                 (long)slot, (long)*(int32_t *)((uint8_t *)vehicle + VEH_GARAGE_TEAM),
                 (long)*(int32_t *)((uint8_t *)vehicle + VEH_GARAGE_BAY), vehicle->DriverName);
    InterlockedDecrement(&g_active_calls);
}

uint8_t __attribute__((thiscall)) suspension_force_hook(void *vehicle_dynamics)
{
    int32_t slot = dynamics_slot_of(vehicle_dynamics);
    LONG bit = slot >= 0 && slot < 31 ? (1L << slot) : 0;
    uint8_t result;
    InterlockedIncrement(&g_active_calls);
    if (bit && !(InterlockedOr(&g_suspension_entered_mask, bit) & bit))
        log_line("suspension enter: pid=%lu slot=%ld",
                 (unsigned long)GetCurrentProcessId(), (long)slot);
    result = original_SuspensionForceComputationWithPerCornerVirtualDevices()(vehicle_dynamics);
    if (bit && !(InterlockedOr(&g_suspension_exited_mask, bit) & bit))
        log_line("suspension exit: pid=%lu slot=%ld",
                 (unsigned long)GetCurrentProcessId(), (long)slot);
    InterlockedDecrement(&g_active_calls);
    return result;
}

/* 2026-10-12 crash fix (live: third client pid 29944 died at 21:12:04 UTC in
 * VehicleGraphicsSceneLoader 0x4DFE4F <- VehgenScnFileLoader <- native slot LOAD
 * 0x45FC50, NULL scene object, the moment a human joined in the car a coop bot
 * still held; the host manifest removing that bot arrived 0.6 s later). Before
 * the native slot sync runs, release any coop bot whose .veh file equals the car
 * of an incoming player entry. Entry table (decompile 0x45FC50 l.55-152):
 * netcomm+0x3D0, stride 140, count netcomm+0x3E14; entry = .veh file name
 * (compared with PathFilenameExtractor(vehicle)), byte +64 = slot. */
static const char *veh_file_part(const char *path)
{
    const char *b = path;
    for (const char *p = path; *p; ++p) if (*p == '\\' || *p == '/') b = p + 1;
    return b;
}
static void release_bots_for_joining_players(void *netcomm)
{
    uint8_t *nc = (uint8_t *)netcomm;
    if (!nc || g_ai_count <= 0) return;
    int32_t count = *(int32_t *)(nc + 0x3E14);
    if (count <= 0 || count > MAX_VEHICLES) return;
    for (int32_t i = 0; i < count; ++i) {
        const char *entry = (const char *)(nc + 0x3D0 + 140 * i);
        int32_t slot = (uint8_t)entry[64];
        size_t len = strnlen(entry, 32);
        if (len == 0 || len >= 32 || slot_is_owned_ai(slot)) continue;
        for (int32_t index = g_ai_count - 1; index >= 0; --index) {
            if (!owned_ai(index) || lstrcmpiA(veh_file_part(g_ai_paths[index]), entry) != 0) continue;
            release_bot(index, "a human is joining in this car");
            break;
        }
    }
}

/* 1.0.0 (spec section 11, owner design 2026-10-13): host-only, plain MP, lobby/grid only.
 * Node list = netcomm+15476 (count netcomm+0x3E14), walked exactly as ShortComputation_13e0
 * 0x4613E0 does. Own id = sub_462300(*netcomm) (dispatcher 0x45DCE9 compares it with the
 * boot target). Boot = sub_462940 with ecx = &0x7CD25C (kick caller 0x46E1B0, call 0x46E212).
 * Chat: 1.0.0 sends the kick notice (send_kick_chat) at the WARN event, before the boot. */
typedef int32_t(__attribute__((thiscall)) *OwnNodeFn)(void *netcomm_object);
typedef char(__attribute__((thiscall)) *NodeBootFn)(void *netcomm_global, int32_t node);
static const uintptr_t ADDR_NETCOMM_GLOBAL = 0x007CD25C;
static const uintptr_t ADDR_OWN_NODE_FN = 0x00462300;
static const uintptr_t ADDR_NODE_BOOT_FN = 0x00462940;
static const uintptr_t NETCOMM_NODE_LIST_OFF = 15476;
static const uintptr_t NETCOMM_NODE_COUNT_OFF = 15892;
static const int32_t SESSION_TYPE_RACE = 7;
static NodePolicyState g_node_policy;   /* game thread only; decision logic in coop_node_policy.h */

/* 1.0.0 (spec section 13, 2026-10-13): one native chat line before a real boot, so the player
 * sees why. Recipe checked in the exe (native chat sender 0x470490, call asm 0x470503-0x47055C):
 *   ProgressBarEventEncode(P, class 4, opcode 6, len, byte0 0, (float)*(double*)0x008AFEA0, flag 1)
 *   memcpy(P + 8, text, len); P[8 + len] = 0
 *   TextureCreationWrapper(&netcomm, P, mode 0, target -1 = broadcast to every peer, flag -1)
 * Not proven in game: display in vanilla clients (byte_7CE254 / byte_7D1044 gates) and the sender
 * name (byte0 = 0). Game thread only. Unguarded like the boot call (this file has no SEH). */
typedef void(__attribute__((thiscall)) *ChatEncodeFn)(void *packet, int32_t cls, int32_t opcode,
                                                    int32_t len, int32_t byte0, float time, int32_t flag);
typedef char(__attribute__((thiscall)) *ChatSendFn)(void *netcomm_global, void *packet, int32_t mode,
                                                   int32_t target, int32_t flag);
static const uintptr_t ADDR_CHAT_ENCODE_FN = 0x004B33B0;
static const uintptr_t ADDR_CHAT_SEND_FN = 0x00462250;
static const uintptr_t ADDR_CHAT_TIME = 0x008AFEA0;
static const char KICK_NOTICE_TEXT[] = "Multiplayer Overhaul required. Get Lightloader: mods.palloset.com/lightloader";
static const size_t KICK_NOTICE_MAX = 159;

static void send_kick_chat(int32_t node)
{
    const size_t len = strlen(KICK_NOTICE_TEXT);
    uint32_t packet[108];   /* 432 bytes: the encoder writes dwords 0..107 */
    if (len > KICK_NOTICE_MAX) {
        log_line("COOP-K02 kick notice not sent for node %ld: text %lu bytes exceeds %lu",
                 (long)node, (unsigned long)len, (unsigned long)KICK_NOTICE_MAX);
        return;
    }
    memset(packet, 0, sizeof(packet));
    ((ChatEncodeFn)ADDR_CHAT_ENCODE_FN)(packet, 4, 6, (int32_t)len, 0,
                                        (float)*(volatile double *)ADDR_CHAT_TIME, 1);
    memcpy((uint8_t *)packet + 8, KICK_NOTICE_TEXT, len);
    ((uint8_t *)packet)[8 + len] = 0;
    char failed = ((ChatSendFn)ADDR_CHAT_SEND_FN)((void *)ADDR_NETCOMM_GLOBAL, packet, 0, -1, -1);
    /* 0x462250 returns 1 when NetworkSendDispatch 0x57A6A0 returned 0 (no error), 0 when it returned
     * non-zero (10 = no connection object). FACT from decompile + disassembly; delivery not shown here. */
    log_line("COOP-K02 kick notice handed to native broadcast for node %ld: %lu text bytes, send result %d "
             "(1 = NetworkSendDispatch returned 0, no error; 0 = it returned non-zero; delivery to peers not confirmed by this value)",
             (long)node, (unsigned long)len, (int)failed);
}

/* Bot guard (spec section 14, 2026-10-13). Native entry i (netcomm+0x3D0 + 140*i: .veh name at
 * +0, slot byte at +64) pairs with node list index i (netcomm+15476 + 4*i); sub_462460 DriverDataCopyByID
 * walks both with one index. Returns NULL only for an entry that looks like a human game peer; any other
 * entry is never a boot candidate. Before this guard nothing in the code kept bots out of the node list. */
static const char *node_boot_block_reason(const uint8_t *nc, uint32_t index)
{
    const char *entry = (const char *)(nc + 0x3D0 + 140 * index);
    size_t len = strnlen(entry, 32);
    if (len == 0 || len >= 32) return "no loaded player entry (empty or long name)";
    if (slot_is_owned_ai((uint8_t)entry[64])) return "entry slot is a mod AI bot";
    for (int32_t i = 0; i < g_ai_count; ++i)
        if (owned_ai(i) && lstrcmpiA(veh_file_part(g_ai_paths[i]), entry) == 0)
            return "entry car is a mod AI bot";
    return NULL;
}

/* Once per node per session (g_bot_logged is cleared when the host is not in a session). */
static int32_t g_bot_logged[NODE_POLICY_WATCH_MAX];
static uint32_t g_bot_logged_count = 0;
static void log_bot_block_once(int32_t node, const char *why)
{
    for (uint32_t i = 0; i < g_bot_logged_count; ++i)
        if (g_bot_logged[i] == node) return;
    if (g_bot_logged_count < NODE_POLICY_WATCH_MAX) g_bot_logged[g_bot_logged_count++] = node;
    log_line("COOP-K00 node %ld not a boot candidate: %s (bot guard, spec 14)", (long)node, why);
}

static void require_mod_tick(void)
{
    if (g_stopping) return;
    uint8_t *nc = *(uint8_t *volatile *)ADDR_NETCOMM_GLOBAL;
    if (!nc) return;
    void *own_arg = *(void *volatile *)nc;
    if (!own_arg) return;
    int32_t own = ((OwnNodeFn)ADDR_OWN_NODE_FN)(own_arg);
    InterlockedExchange(&g_own_node_id, (LONG)own);
    /* Spec 14: plain MP follows RequireMod. CO-OP follows RequireModCoop (default 1 = real boot), and
     * runs when either flag is on, so RequireMod=0 with RequireModCoop=1 still boots CO-OP vanilla peers. */
    int plain = plain_mode_now();
    int coop = coop_mode_is_selected();
    if (!((plain && g_require_mod) || (coop && (g_require_mod || g_require_mod_coop)))) return;
    int32_t state = network_state();
    int is_host = state == 1 || state == 2;
    if (!is_host) {
        /* session over (1.0.0): forget the warned set so a new session may warn again */
        memset(&g_node_policy, 0, sizeof(g_node_policy));
        g_bot_logged_count = 0;
        return;
    }
    DWORD now = GetTickCount();
    uint32_t count = *(volatile uint32_t *)(nc + NETCOMM_NODE_COUNT_OFF);
    int32_t native[NODE_POLICY_WATCH_MAX] = {};
    int32_t list[NODE_POLICY_WATCH_MAX] = {};
    uint32_t list_count = count;   /* above the watch limit the policy reports overflow and reads no list */
    if (count <= NODE_POLICY_WATCH_MAX) {
        uint8_t ineligible[NODE_POLICY_WATCH_MAX] = {};
        for (uint32_t i = 0; i < count; ++i) {
            native[i] = *(volatile int32_t *)(nc + NETCOMM_NODE_LIST_OFF + 4 * i);
            const char *why = node_boot_block_reason(nc, i);
            if (why) {
                ineligible[i] = 1;
                log_bot_block_once(native[i], why);
            }
        }
        list_count = node_policy_filter_eligible(native, ineligible, count, list);
    }
    int32_t claims[NET1_CLAIMS];
    uint32_t claim_count = net1_claim_list(claims, NET1_CLAIMS);
    int in_race = *(volatile int32_t *)ADDR_SESSION_TYPE == SESSION_TYPE_RACE;
    NodePolicyEvent events[NODE_POLICY_EVENT_MAX];
    uint32_t event_count = 0;
    node_policy_boot_mode(&g_node_policy, coop && !g_require_mod_coop);
    node_policy_step(&g_node_policy, is_host, own, list, list_count, claims, claim_count, now,
                     NET1_NODE_GRACE_MS, in_race, events, NODE_POLICY_EVENT_MAX, &event_count);
    for (uint32_t i = 0; i < event_count; ++i) {
        const NodePolicyEvent &e = events[i];
        if (e.kind == NODE_EV_CLAIMED) {
            log_line("net mode: node %ld claims the mod (NET1?/COOP1?), not booted", (long)e.node);
        } else if (e.kind == NODE_EV_WARN) {
            if (g_kick_message) {
                send_kick_chat(e.node);
            } else {
                log_line("COOP-K02 kick notice off ([Net] KickMessage=0): node %ld boots in %lu s",
                         (long)e.node, (unsigned long)(NODE_WARN_DELAY_MS / 1000));
            }
        } else if (e.kind == NODE_EV_BOOT && coop_mode_is_selected() && !g_require_mod_coop) {
            log_line("COOP-K01 dry-run (RequireModCoop=0): would boot node %ld, no mod claim in %lu s; nothing sent",
                     (long)e.node, (unsigned long)(NET1_NODE_GRACE_MS / 1000));
        } else if (node_policy_should_boot(e.kind, 0)) {
            char sent = ((NodeBootFn)ADDR_NODE_BOOT_FN)((void *)ADDR_NETCOMM_GLOBAL, e.node);
            /* 0x462940 returns 1 when NetworkSendDispatch 0x57A6A0 returned 0 (no error); 0 when non-zero. */
            long entry_slot = -1;
            char entry_name[33] = "?";
            for (uint32_t k = 0; k < count && count <= NODE_POLICY_WATCH_MAX; ++k) {
                if (native[k] != e.node) continue;
                const char *entry = (const char *)(nc + 0x3D0 + 140 * k);
                memcpy(entry_name, entry, 32);
                entry_name[32] = '\0';
                entry_slot = (uint8_t)entry[64];
                break;
            }
            log_line("COOP-K01 booted native node %ld (entry '%s', slot %ld): no mod claim in %lu s (vanilla or pre-1.0.0 client); "
                     "send result %d (1 = NetworkSendDispatch returned 0, no error; 0 = non-zero; delivery not confirmed by this value)",
                     (long)e.node, entry_name, entry_slot, (unsigned long)(NET1_NODE_GRACE_MS / 1000), (int)sent);
        } else if (e.kind == NODE_EV_RACE_DEFER) {
            log_line("COOP-K01 race running: node %ld has no mod claim, boot deferred to lobby", (long)e.node);
        } else if (e.kind == NODE_EV_OVERFLOW) {
            log_line("net mode: %ld nodes exceed watch limit %lu, no node check this tick",
                     (long)e.node, (unsigned long)NODE_POLICY_WATCH_MAX);
        }
    }
}

uint8_t __attribute__((thiscall)) netcomm_slot_sync_hook(void *netcomm)
{
    F1CVehicle *vehicles[MAX_AI] = {};
    LONG slots[MAX_AI] = {};
    LONG rosters[MAX_AI] = {};
    uint8_t result;
    int32_t before_count;

    InterlockedIncrement(&g_active_calls);
    if (!g_stopping && coop_mode_is_selected()) release_bots_for_joining_players(netcomm);
    before_count = g_ai_count;
    for (int32_t index = 0; index < before_count; ++index) {
        slots[index] = g_ai_slots[index];
        rosters[index] = g_ai_roster_indices[index];
        vehicles[index] = owned_ai(index);
        if (!g_stopping && vehicles[index] && !g_ai_parked[index])
            commands::set_WearTickGate__was__slot_live_marker_(vehicles[index], -1);
    }

    result = original_NetCommPlayerSlotSync()(netcomm);
    require_mod_tick();

    if (!g_stopping && before_count > 0) {
        int32_t restored = 0;
        for (int32_t index = 0; index < before_count; ++index) {
            F1CVehicle *vehicle = vehicles[index];
            if (vehicle && g_ai_parked[index]) continue;
            if (vehicle && slots[index] == g_ai_slots[index] &&
                rosters[index] == g_ai_roster_indices[index] &&
                vehicle->VehicleRosterIndex == rosters[index]) {
                commands::set_WearTickGate__was__slot_live_marker_(vehicle, g_ai_roles[index]);
                ++restored;
            } else {
                log_line("AI ownership lost: index=%ld slot=%ld roster=%ld",
                         (long)index, (long)slots[index], (long)rosters[index]);
                g_ai_slots[index] = -1;
                g_ai_roster_indices[index] = -1;
                g_ai_roles[index] = -1;
            }
        }
        compact_ai_entries();
        int32_t active_count = game::BuildActiveVehiclePointerArray();
        f1c_vehicle_count_set(active_count);
        if (InterlockedIncrement(&g_sync_log_count) <= 16)
            log_line("slot sync restore: pid=%lu ai_restored=%ld active_total=%ld",
                     (unsigned long)GetCurrentProcessId(), (long)restored,
                     (long)active_count);
    }
    if (!g_stopping) {
        int32_t humans = human_slot_count();
        if (humans != g_last_human_count) {
            log_line("human count changed: pid=%lu humans=%ld bots=%ld/%ld",
                     (unsigned long)GetCurrentProcessId(), (long)humans,
                     (long)owned_ai_count(), (long)g_requested_ai);
            if (humans < g_last_human_count && humans > 0 && g_ai_count > 0 && !g_refill_mid_session &&
                (network_state() == 1 || network_state() == 2)) {
                g_refill_defer_session = *(volatile int32_t *)ADDR_SESSION_TYPE;
                log_line("bot refill deferred to the next session (no load freeze mid-session; RefillBotMidSession=0)");
            }
            g_last_human_count = humans;
        }
        prepare_ai_field();
        if (coop_mode_is_selected() && (network_state() == 1 || network_state() == 2))
            update_car_replacements();
    }

    if (InterlockedIncrement(&g_sync_log_count) <= 8) {
        int32_t owned = 0;
        for (int32_t index = 0; index < g_ai_count; ++index)
            if (owned_ai(index)) ++owned;
        log_line("slot sync: pid=%lu total=%ld ai_owned=%ld ai_expected=%ld",
                 (unsigned long)GetCurrentProcessId(), (long)f1c_vehicle_count_get(),
                 (long)owned, (long)g_ai_count);
    }
    InterlockedDecrement(&g_active_calls);
    return result;
}

} // namespace hooks

/* Restore entry bytes without freeing trampolines that an in-flight hook may
 * still be using. The worker frees them only after all replacements drain. */
static F1CDetourResult detach_preserving_trampoline(F1CDetour *hook)
{
    DWORD old_protection;
    DWORD ignored;
    int32_t expected_jump;
    size_t index;
    if (!hook || !hook->installed) return F1C_DETOUR_NOT_INSTALLED;
    if (hook->target[0] != 0xE9 ||
        !f1c_rel32(hook->target + 5, hook->replacement, &expected_jump) ||
        memcmp(hook->target + 1, &expected_jump, sizeof(expected_jump)) != 0)
        return F1C_DETOUR_BYTES_MISMATCH;
    for (index = 5; index < hook->stolen_length; ++index)
        if (hook->target[index] != 0x90) return F1C_DETOUR_BYTES_MISMATCH;
    if (!VirtualProtect(hook->target, hook->stolen_length,
                        PAGE_EXECUTE_READWRITE, &old_protection))
        return F1C_DETOUR_PROTECT_FAILED;
    memcpy(hook->target, hook->original, hook->stolen_length);
    FlushInstructionCache(GetCurrentProcess(), hook->target, hook->stolen_length);
    VirtualProtect(hook->target, hook->stolen_length, old_protection, &ignored);
    hook->installed = 0;
    return F1C_DETOUR_OK;
}

/* ---- CO-OP NEXT SESSION simulation ---------------------------------------
 * Offline, leaving a session (sub_46DC60) runs the simulate-rest wrapper
 * sub_46BF10 so AI get lap times. In a network session the game skips it
 * (findings mp-next-session-skips-simulate-rest-20261009). On the CO-OP host
 * we run the same wrapper before the native handler, for practice/qualifying/
 * warmup. The simulator's own eligibility rules apply (controller != 0, laps
 * below the limit, not finished/out); remote human cars (controller 2, not a
 * bot) are made ineligible for the call by holding their lap counter at the
 * limit, then restored exactly. */
static const uintptr_t ADDR_NEXT_SESSION = 0x0046DC60;
static const uintptr_t ADDR_SIMULATE_REST = 0x0046BF10;
static const uintptr_t ADDR_AFTER_SIMULATE = 0x004D2450;
static const uintptr_t ADDR_AFTER_SIMULATE_ARG = 0x008ACEE0;
static const uintptr_t ADDR_GAME_MODULE_MANAGER = 0x007CD25C;
static const uintptr_t ADDR_SESSION_LAP_LIMIT = 0x008ACF8C;
static const uintptr_t NETWORK_STATE_OFFSET = 0x3FC4;
static const uint8_t NextSession_expected[] = {0x56, 0x8B, 0xF1, 0xB9, 0x94, 0xC4, 0x8A, 0x00};
static F1CDetour g_next_session_detour;

typedef int32_t(__attribute__((thiscall)) *NextSessionFn)(void *self, int8_t show);
typedef int32_t(__attribute__((thiscall)) *SimulateRestFn)(void *ui, int8_t show);
typedef void *(__attribute__((thiscall)) *PageLookupFn)(void *pages, int32_t id);
typedef int32_t(__attribute__((thiscall)) *AfterSimulateFn)(void *object);

static int32_t network_state(void)
{
    uint8_t *manager = *(uint8_t **)ADDR_GAME_MODULE_MANAGER;
    return manager ? *(int32_t *)(manager + NETWORK_STATE_OFFSET) : 0;
}

/* Snapshot every live car's qualifying best lap and write a Grid Restore
 * compatible qualifying_<time>_<pid>_coop.tsv next to this DLL. */
static void quali_keep_capture(int completed)
{
    int32_t slots[MAX_VEHICLES], count = 0, timed = 0;
    float times[MAX_VEHICLES];
    memset(g_quali_keep_time, 0, sizeof(g_quali_keep_time));
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        float best = vehicle->bestLap;
        if (!(best > 0.0f) && vehicle->timeOfQualySession > 0.0f) best = vehicle->timeOfQualySession;
        g_quali_keep_time[slot] = best > 0.0f ? best : 0.0f;
        if (best > 0.0f) ++timed;
        times[count] = g_quali_keep_time[slot];
        slots[count++] = slot;
    }
    g_quali_keep_valid = count > 0;
    for (int32_t a = 1; a < count; ++a) {  /* timed by time, untimed after in slot order */
        int32_t key = slots[a], b = a - 1;
        float kt = times[a];
        while (b >= 0) {
            float ot = times[b];
            if (!(kt > 0.0f && (ot <= 0.0f || ot > kt))) break;
            slots[b + 1] = slots[b];
            times[b + 1] = times[b];
            --b;
        }
        slots[b + 1] = key;
        times[b + 1] = kt;
    }
    log_line("qualifying keep: captured %ld cars, %ld timed (%s)", (long)count, (long)timed,
             completed ? "completed session, no simulation" : "early exit, after native simulation");
    char path[MAX_PATH];
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&quali_keep_capture, &self);
    DWORD n = GetModuleFileNameA(self, path, sizeof(path));
    char *slash = n ? strrchr(path, '\\') : NULL;
    if (!slash) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    wsprintfA(slash + 1, "qualifying_%04u%02u%02u_%02u%02u%02u_%lu_coop.tsv", t.wYear, t.wMonth, t.wDay,
              t.wHour, t.wMinute, t.wSecond, (unsigned long)GetCurrentProcessId());
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        log_line("qualifying keep: archive not written (error %lu)", (unsigned long)GetLastError());
        return;
    }
    char line[256];
    DWORD written;
    int len = wsprintfA(line, "# CO-OP qualifying; %s\nposition\tdriver\tbest_lap_seconds\n",
                        completed ? "completed session, native simulation skipped" : "ended early, native simulation");
    WriteFile(file, line, (DWORD)len, &written, NULL);
    for (int32_t i = 0; i < count; ++i) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slots[i]);
        float best = times[i];
        if (best > 0.0f) {
            int ms = (int)(best * 1000.0f + 0.5f);
            len = wsprintfA(line, "%ld\t%s\t%d.%03d\n", (long)(i + 1), vehicle ? vehicle->DriverName : "?",
                            ms / 1000, ms % 1000);
        } else {
            len = wsprintfA(line, "%ld\t%s\t\n", (long)(i + 1), vehicle ? vehicle->DriverName : "?");
        }
        WriteFile(file, line, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);
    log_line("qualifying keep: archive %s", path);
}

int32_t __attribute__((thiscall)) next_session_hook(void *self, int8_t show)
{
    int32_t session_type = *(volatile int32_t *)ADDR_SESSION_TYPE;
    int32_t state = network_state();
    InterlockedIncrement(&g_active_calls);
    if (!g_stopping && coop_mode_is_selected() && (state == 1 || state == 2) &&
        session_type >= 1 && session_type <= 6) {
        void *pages = ((void **)self)[16];
        void *ui = pages ? ((PageLookupFn)(*(uintptr_t **)pages)[7])(pages, 30) : NULL;
        int32_t limit = *(volatile int32_t *)ADDR_SESSION_LAP_LIMIT;
        int32_t held_slots[MAX_VEHICLES];
        int32_t held_laps[MAX_VEHICLES];
        int32_t held = 0, eligible_bots = 0;
        for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
            F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
            if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ <= 0) continue;
            if (hooks::slot_is_human(slot)) {
                held_slots[held] = slot;
                held_laps[held++] = vehicle->lapNumber;
                vehicle->lapNumber = limit;
            } else if (vehicle->lapNumber < limit && vehicle->FinishState != 1) {
                ++eligible_bots;
            }
        }
        int32_t phase = *(volatile int32_t *)0x007C56A8;  /* RacePhase */
        /* Qualifying keep: after the chequer (phase >= 8) the native simulation
         * gives every unfinished bot extra simulated laps and overwrites the
         * real order (2026-10-10 Silverstone: grid P1-P6 all went to simulated
         * bots). Same rule as qualifying_preservation: completed qualifying is
         * never re-simulated. Leaving early still simulates (vanilla). */
        int keep = session_type == 5 && phase >= 8;
        log_line("next session: %s session_type=%ld phase=%ld lap_limit=%ld eligible_bots=%ld remote_humans_held=%ld ui=%p",
                 keep ? "qualifying complete, simulation SKIPPED (order kept)" : "simulate",
                 (long)session_type, (long)phase, (long)limit, (long)eligible_bots, (long)held, ui);
        if (ui && !keep) {
            ((SimulateRestFn)ADDR_SIMULATE_REST)(ui, show);
            ((AfterSimulateFn)ADDR_AFTER_SIMULATE)((void *)ADDR_AFTER_SIMULATE_ARG); /* thiscall, ecx = 0x8ACEE0 */
        }
        for (int32_t i = 0; i < held; ++i) {
            F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(held_slots[i]);
            if (vehicle) vehicle->lapNumber = held_laps[i];
        }
        int32_t timed = 0;
        for (int32_t index = 0; index < g_ai_count; ++index) {
            F1CVehicle *vehicle = owned_ai(index);
            if (vehicle && vehicle->bestLap > 0.0f) ++timed;
        }
        log_line("next session: %s, bots with a best lap=%ld/%ld", keep ? "not simulated" : "simulated",
                 (long)timed, (long)g_ai_count);
        if (session_type == 5) quali_keep_capture(keep);
        else if (session_type < 5) g_quali_keep_valid = 0;
    }
    int32_t result = ((NextSessionFn)g_next_session_detour.trampoline)(self, show);
    InterlockedDecrement(&g_active_calls);
    return result;
}

/* ---- CO-OP race start hold + host time warp ------------------------------
 * Main time step 0x4FA7E0 advances session time dbl_8AFEA0 by scale * dt.
 * Offline the game pauses while the player is in the garage; online it never
 * does, so the race start (PitTimer phase machine) runs while the host is still
 * in the garage. CO-OP host, race, RacePhase <= 4 (MP enters the race at phase 4; lights at 5), host car not on track
 * (AIControlFlag 0): restore the session time after the step (hold).
 * Time warp (owner: host only, any time): the step forces scale 1.0 through
 * 0x4F9F70(0) in network sessions; with warp toggled by the host's Ctrl+T the
 * hook turns that call into 0x4F9F70(1) = x30. Clients are NOT synced yet. */
static const uintptr_t ADDR_TIME_STEP = 0x004FA7E0;
static const uintptr_t ADDR_TIME_SCALE_SET = 0x004F9F70;
static const uintptr_t ADDR_KEY_DOWN = 0x0044F460;
static const uintptr_t ADDR_KEY_STATE = 0x007CA594;
static const uintptr_t ADDR_SESSION_TIME = 0x008AFEA0;
static const uintptr_t ADDR_RACE_PHASE = 0x007C56A8;
static const uintptr_t ADDR_REALTIME_WINDOW = 0x007CE2E0;
static const int32_t KEY_CONTROL = 29;  /* DIK_LCONTROL */
static const int32_t KEY_T = 20;        /* DIK_T */
static const uint8_t TimeStep_expected[] = {0x57, 0x8B, 0xF9, 0x6A, 0x1D, 0xB9, 0x94, 0xA5, 0x7C, 0x00};
static const uint8_t TimeScaleSet_expected[] = {0x8B, 0x44, 0x24, 0x04, 0x83, 0xE8, 0x00};
static F1CDetour g_time_step_detour;
static const uintptr_t ADDR_MONITOR_FRAME = 0x00485B90;
static const uint8_t MonitorFrame_expected[] = {0xA1, 0xC0, 0xD8, 0x7C, 0x00, 0x85, 0xC0, 0x53, 0x55};
static F1CDetour g_monitor_detour;
static volatile LONG g_monitor_tick;
typedef int32_t(__attribute__((fastcall)) *MonitorFrameFn)(int32_t a1, int32_t a2);

/* Native game UI: the multiplayer message log (same call the game uses for
 * "has entered the game", thiscall on 0x7CD204) plus the HUD message strip.
 * Game thread only; repeated every 6 s until the game is restarted. */
static const uintptr_t ADDR_MESSAGE_LOG_ADD = 0x0045BD10;
static const uintptr_t ADDR_MESSAGE_LOG = 0x007CD204;
typedef char(__attribute__((thiscall)) *MessageLogAddFn)(void *log, const char *text, int32_t level, int32_t color);

static void post_restart_notice(void)
{
    static DWORD last;
    if (!g_restart_required || g_stopping) return;
    if (last && GetTickCount() - last < 6000) return;
    last = GetTickCount();
    ((MessageLogAddFn)ADDR_MESSAGE_LOG_ADD)((void *)ADDR_MESSAGE_LOG,
        "CO-OP: host content installed. RESTART THE GAME to load it, then join again.", 3, 16909060);
    f1c_pit_hud_post("CO-OP: RESTART GAME TO LOAD");
}

/* ms since the garage monitor last drew. The tick is stored as GetTickCount()|1
 * (0 = never), so it can be 1 ms ahead of an even GetTickCount(): clamp that
 * to 0 (2026-10-11: unsigned wrap read as ~49 days, warp switched off at once). */
static DWORD monitor_age_ms(void)
{
    DWORD tick = (DWORD)g_monitor_tick;
    if (!tick) return 0xFFFFFFFFu;
    LONG age = (LONG)(GetTickCount() - tick);
    return age < 0 ? 0u : (DWORD)age;
}

int32_t __attribute__((fastcall)) monitor_frame_hook(int32_t a1, int32_t a2)
{
    InterlockedExchange(&g_monitor_tick, (LONG)(GetTickCount() | 1));
    post_restart_notice();
    return ((MonitorFrameFn)g_monitor_detour.trampoline)(a1, a2);
}
/* CO-OP race start timer (owner 2026-10-09): replaces the vanilla MP ~33.5 s
 * start with 60 s. FACT (disassembly, installed exe): host sends the lights
 * message once session time >= 31.0 (0x4D2315 fcomp qword [0x5ACEF0], only
 * reference); lights follow +2.5 s. HUD countdown = 33.5 - time (0x4D3258
 * fsubr dword [0x5ACEF8], only reference). Operands are redirected to our
 * constants while CO-OP is selected and restored otherwise / on unload. */
static const uintptr_t ADDR_START_SEND_OPERAND = 0x004D2317;
static const uintptr_t ADDR_START_HUD_OPERAND = 0x004D325A;
static const uintptr_t START_SEND_VANILLA = 0x005ACEF0;
static const uintptr_t START_HUD_VANILLA = 0x005ACEF8;
/* Owner 2026-10-10: start timer 5 min; when the host presses DRIVE it drops to
 * 10 s. Lights message is sent at g_start_send_time, lights follow +2.5 s. */
static const double START_DEFAULT_SECONDS = 300.0;
static const double START_AFTER_DRIVE_SECONDS = 60.0;  /* owner 2026-10-10: time for setups */
static volatile double g_start_send_time = 300.0 - 2.5;
static volatile float g_start_hud_time = 300.0f;
static int g_start_timer_applied = -1;

static int write_operand(uintptr_t at, uintptr_t expected, uintptr_t value)
{
    DWORD old_protection, ignored;
    if (*(volatile uintptr_t *)at != expected) return 0;
    if (!VirtualProtect((void *)at, sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old_protection)) return 0;
    *(volatile uintptr_t *)at = value;
    VirtualProtect((void *)at, sizeof(uintptr_t), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, sizeof(uintptr_t));
    return 1;
}

static void apply_start_timer(int on)
{
    if (on == g_start_timer_applied) return;
    uintptr_t send_ours = (uintptr_t)&g_start_send_time, hud_ours = (uintptr_t)&g_start_hud_time;
    int ok = on ? write_operand(ADDR_START_SEND_OPERAND, START_SEND_VANILLA, send_ours) &&
                  write_operand(ADDR_START_HUD_OPERAND, START_HUD_VANILLA, hud_ours)
                : (write_operand(ADDR_START_SEND_OPERAND, send_ours, START_SEND_VANILLA) |
                   write_operand(ADDR_START_HUD_OPERAND, hud_ours, START_HUD_VANILLA));
    if (on && !ok) {  /* partial apply: put back whatever we changed */
        write_operand(ADDR_START_SEND_OPERAND, send_ours, START_SEND_VANILLA);
        write_operand(ADDR_START_HUD_OPERAND, hud_ours, START_HUD_VANILLA);
        log_line("COOP-E20 start timer: operands not vanilla; 60 s timer NOT applied");
    } else if (g_start_timer_applied != -1 || on) {
        log_line("start timer: %s", on ? "300 s, 60 s after host DRIVE (CO-OP)" : "vanilla");
    }
    g_start_timer_applied = on;
}


/* Fix D (2026-10-10 slot sweep): sub_4DA840 (Steward session-end step) gets the
 * classification leader's SLOT from 0x4D1B50 and requires `slot < active count`
 * (cmp eax,[0x5C329C] at 0x4DA95F) before reading its FinishState; a CO-OP bot
 * leader (slot 32+) fails it. While CO-OP is selected that operand points to
 * a constant 104 (slot table size); restored otherwise / on unload. */
static const uintptr_t ADDR_LEADER_SLOT_LIMIT_OPERAND = 0x004DA961;
static volatile int32_t g_slot_table_size = 104;
static int g_leader_limit_applied = -1;

static void apply_leader_slot_limit(int on)
{
    if (on == g_leader_limit_applied) return;
    uintptr_t ours = (uintptr_t)&g_slot_table_size;
    int ok = on ? write_operand(ADDR_LEADER_SLOT_LIMIT_OPERAND, (uintptr_t)0x005C329C, ours)
                : write_operand(ADDR_LEADER_SLOT_LIMIT_OPERAND, ours, (uintptr_t)0x005C329C);
    if (on && !ok) log_line("COOP-E21 leader slot limit: operand not vanilla; not applied");
    else if (g_leader_limit_applied != -1 || on)
        log_line("leader slot limit: %s", on ? "104 (CO-OP)" : "vanilla");
    g_leader_limit_applied = on;
}

static F1CDetour g_time_scale_detour;
static volatile LONG g_warp;
static int g_warp_enabled = 1;  /* INI [CoOp] HostTimeWarp (owner 2026-10-10: on) */
static double g_warp_scale = 10.0;  /* INI [CoOp] HostTimeWarpScale 2..30; native Ctrl+T = 30 (blow-up seen at 30 in MP 2026-10-09) */
static const uintptr_t ADDR_TIME_SCALE = 0x005D6D70;  /* dbl_5D6D70 time scale */
static int g_warp_key_was_down;
static int g_start_hold_logged = -1;
static int32_t g_warp_session_type = -1;

typedef int8_t(__attribute__((thiscall)) *TimeStepFn)(void *self);
typedef int32_t(__attribute__((stdcall)) *TimeScaleSetFn)(int32_t mode);
typedef int8_t(__attribute__((thiscall)) *KeyDownFn)(void *keys, int32_t key);

static int coop_host_now(void)
{
    int32_t state = network_state();
    return coop_mode_is_selected() && (state == 1 || state == 2);
}


/* ---- CO-OP AI fixes 2026-10-10 (docs/PLAN_AI_FIXES_2026-10-10.md) --------
 * Fix A (PitExitFix): the AI pit-exit scheduler sub_4A4110 (object 0x7CE2F0)
 * keeps a "cars out" array indexed by veh+4 (TeamIDSetter = slot) but scans it
 * only for i < active count (0x5C329C). CO-OP bots live in slots 32+, so it
 * never sees them out: crowd factor pinned at minimum, pit-lane traffic check
 * blind. Host: call the original on a packed copy (non-null entries of 0..103
 * at 0..n-1, [104] = next-exit time), then write back [104] and any entry the
 * original cleared. Finding ai-garage-exit-scheduler-slot-indexed-20261010.
 * Fix B (RemoteCarTracking): AIMainUpdate 0x424E70 returns at once for role
 * (veh+0x32C8) > 1, so a remote human copy (role 2) never gets the path
 * tracking of sub_426430: Speed 0x4440 stays 0 and track waypoint 0x4354 stays
 * stale. Bots read both (AISteering 0x4270A0 projects the car's velocity on
 * the waypoint found from 0x4354) and saw the human as stopped / reversing.
 * Host, every frame before the native step: Speed = |velocity| and refresh
 * 0x4354 exactly as sub_426430 does (re-query when NULL or > sqrt(3200) m). */
static int g_pit_exit_fix = 1;        /* INI [CoOp] PitExitFix */
static int g_remote_tracking_fix = 1; /* INI [CoOp] RemoteCarTracking */
static const uintptr_t ADDR_PIT_EXIT_GATE = 0x004A4110;
static const uint8_t PitExitGate_expected[] = {0x83, 0xEC, 0x0C, 0x8B, 0x15, 0x9C, 0x32, 0x5C, 0x00};
static F1CDetour g_pit_exit_detour;
static volatile LONG g_pit_exit_logged;
typedef uint8_t(__attribute__((thiscall)) *PitExitGateFn)(void *scheduler, void *vehicle);
static const uintptr_t ADDR_ACTIVE_COUNT = 0x005C329C;
static const int PIT_EXIT_ENTRIES = 104;   /* [104] holds the next allowed exit time */
static const uintptr_t ADDR_PIT_SCHEDULER = 0x007CE2F0;  /* object passed as `this` to 0x4A4110/0x4A4710/0x4A46F0 */
static const uintptr_t ADDR_ACTIVE_ARRAY = 0x007C54F0;   /* dword_7C54F0[104] active vehicle pointers */

/* Fix A2 (PitExitFix): FindNearestVehicle 0x4A4710 (pit-lane car ahead, from
 * OpponentProximityScan) reads the slot-indexed scheduler array by ACTIVE-ARRAY
 * position. Host CO-OP: give it a copy re-ordered by active position. */
static const uintptr_t ADDR_PIT_NEAREST = 0x004A4710;
static const uint8_t PitNearest_expected[] = {0x83, 0xEC, 0x08, 0x55, 0x8B, 0x6C, 0x24, 0x10, 0x57};
static F1CDetour g_pit_nearest_detour;
typedef void *(__attribute__((thiscall)) *PitNearestFn)(void *scheduler, int32_t own_index);

/* Fix A3 (QualiAllTimed, 0.9.3-dev11): native qualifying gives no priority to
 * cars without a time, so in a 20-min session with 24 bots some get their first
 * run too late (live 2026-10-10: CO-OP 5/24, offline 2/23 untimed). Host CO-OP
 * qualifying (session 5), from 30% of the session:
 *  - untimed bot asks: run the original with the session start moved back so it
 *    takes its native last-quarter branch (pit-lane traffic check + shared 18 s
 *    spacing, no random roll); runs-left check stays native.
 *  - timed bot asks while an untimed bot is still garaged with runs left and its
 *    go flag set: deny (no state touched).
 * Once every bot has a time the native scheduler runs unchanged (late rush). */
static int g_quali_all_timed = 1;     /* INI [CoOp] QualiAllTimed */
static volatile LONG g_quali_prio_logged;
static const uintptr_t ADDR_SESSION_START = 0x008ACF90;  /* float flt_8ACF90 */
static const uintptr_t ADDR_SESSION_END = 0x008ACF94;    /* float flt_8ACF94 */
static const double QUALI_PRIORITY_FROM = 0.30;
enum { VEH_DRIVER_STATUS = 0x4510, VEH_RUNS_PLANNED = 0x4588, VEH_RUNS_USED = 0x45E0, VEH_GO_FLAG = 0x45F4 };

static int quali_bot_waiting_untimed(F1CVehicle *v)
{
    const uint8_t *b = (const uint8_t *)v;
    return v->bestLap <= 0.0f && *(const int32_t *)(b + VEH_DRIVER_STATUS) == 5 &&
           *(const int32_t *)(b + VEH_RUNS_USED) < *(const int32_t *)(b + VEH_RUNS_PLANNED) &&
           *(const uint8_t *)(b + VEH_GO_FLAG) != 0;
}

/* 0 = native, 1 = force untimed bot via last-quarter branch, 2 = deny timed bot */
static int quali_priority_mode(void *vehicle, double *progress_out)
{
    if (!g_quali_all_timed || *(volatile int32_t *)ADDR_SESSION_TYPE != 5 || !vehicle) return 0;
    int is_bot = 0;
    for (int32_t i = 0; i < g_ai_count; ++i) if ((void *)owned_ai(i) == vehicle) { is_bot = 1; break; }
    if (!is_bot) return 0;
    double now = *(volatile double *)ADDR_SESSION_TIME;
    double start = *(volatile float *)ADDR_SESSION_START, end = *(volatile float *)ADDR_SESSION_END;
    double len = (start + 3600.0 < end ? start + 3600.0 : end) - start;
    if (len < 1.0) len = 1.0;
    double progress = (now - start) / len;
    *progress_out = progress;
    if (progress < QUALI_PRIORITY_FROM) return 0;
    F1CVehicle *v = (F1CVehicle *)vehicle;
    if (v->bestLap <= 0.0f) return 1;
    for (int32_t i = 0; i < g_ai_count; ++i) {
        F1CVehicle *other = owned_ai(i);
        if (other && other != v && quali_bot_waiting_untimed(other)) return 2;
    }
    return 0;
}

uint8_t __attribute__((thiscall)) pit_exit_gate_hook(void *scheduler, void *vehicle)
{
    PitExitGateFn original = (PitExitGateFn)g_pit_exit_detour.trampoline;
    if (g_stopping || !g_pit_exit_fix || !coop_host_now() || !scheduler)
        return original(scheduler, vehicle);
    uint32_t *live = (uint32_t *)scheduler;
    static uint32_t packed[PIT_EXIT_ENTRIES + 1];  /* game thread only */
    static int16_t source[PIT_EXIT_ENTRIES];
    int32_t active = *(volatile int32_t *)ADDR_ACTIVE_COUNT, count = 0;
    if (active < 0) active = 0;
    if (active > PIT_EXIT_ENTRIES) active = PIT_EXIT_ENTRIES;
    memset(packed, 0, sizeof(packed));
    for (int i = 0; i < PIT_EXIT_ENTRIES && count < active; ++i)
        if (live[i]) { source[count] = (int16_t)i; packed[count++] = live[i]; }
    packed[PIT_EXIT_ENTRIES] = live[PIT_EXIT_ENTRIES];
    /* dev12: native quirk (offline too, 2026-10-10): bots still leave the garage
     * after the chequered flag (phase 8) in practice/qualifying and start laps
     * that can no longer count. Host CO-OP: no garage exits once the session is over. */
    int32_t session_type = *(volatile int32_t *)ADDR_SESSION_TYPE;
    if (g_quali_all_timed && session_type != 7 && *(volatile int32_t *)ADDR_RACE_PHASE >= 8) {
        static volatile LONG blocked_logged;
        if (InterlockedIncrement(&blocked_logged) <= 20)
            log_line("pit exit blocked: %s (session over, session_type=%ld)",
                     vehicle ? ((F1CVehicle *)vehicle)->DriverName : "?", (long)session_type);
        return 0;
    }
    /* dev12: also no exit that cannot reach the line before the flag. An out lap
     * takes about one lap; Alesi left 6 s before the flag (2026-10-10), so his
     * out lap ended after it and the run was wasted. Estimate = best lap x1.1,
     * or 130 s when the car has no time yet. Session end = flt_8ACF94. */
    if (g_quali_all_timed && session_type != 7 && vehicle) {
        double remaining = *(volatile float *)ADDR_SESSION_END - *(volatile double *)ADDR_SESSION_TIME;
        float best = ((F1CVehicle *)vehicle)->bestLap;
        double out_lap = best > 0.0f ? best * 1.1 : 130.0;
        if (remaining < out_lap) {
            static volatile LONG late_logged;
            if (InterlockedIncrement(&late_logged) <= 20)
                log_line("pit exit blocked: %s (%.0f s left, out lap ~%.0f s)",
                         ((F1CVehicle *)vehicle)->DriverName, remaining, out_lap);
            return 0;
        }
    }
    double progress = 0.0;
    int prio = quali_priority_mode(vehicle, &progress);
    if (prio == 2) return 0;
    uint8_t result;
    if (prio == 1) {
        volatile float *start = (volatile float *)ADDR_SESSION_START;
        float saved = *start;
        *start = (float)(*(volatile double *)ADDR_SESSION_TIME - 3000.0);  /* progress >= 0.83 */
        result = original(packed, vehicle);
        *start = saved;
        if (result && InterlockedIncrement(&g_quali_prio_logged) <= 40)
            log_line("quali priority: untimed %s sent out (progress=%.2f)",
                     ((F1CVehicle *)vehicle)->DriverName, progress);
    } else {
        result = original(packed, vehicle);
    }
    for (int k = 0; k < count; ++k)
        if (!packed[k]) live[source[k]] = 0;   /* original dropped a stale entry */
    live[PIT_EXIT_ENTRIES] = packed[PIT_EXIT_ENTRIES];
    if (result && InterlockedIncrement(&g_pit_exit_logged) <= 60) {
        F1CVehicle *v = (F1CVehicle *)vehicle;
        log_line("pit exit: %s leaves (cars out seen=%ld/%ld, session_type=%ld t=%.1f)",
                 v ? v->DriverName : "?", (long)count, (long)active,
                 (long)*(volatile int32_t *)ADDR_SESSION_TYPE, *(volatile double *)ADDR_SESSION_TIME);
    }
    return result;
}


void *__attribute__((thiscall)) pit_nearest_hook(void *scheduler, int32_t own_index)
{
    PitNearestFn original = (PitNearestFn)g_pit_nearest_detour.trampoline;
    if (g_stopping || !g_pit_exit_fix || !coop_host_now() || !scheduler ||
        own_index < 0 || own_index >= PIT_EXIT_ENTRIES)
        return original(scheduler, own_index);
    const uint32_t *live = (const uint32_t *)scheduler;
    const uint32_t *active = (const uint32_t *)ADDR_ACTIVE_ARRAY;
    static uint32_t ordered[PIT_EXIT_ENTRIES + 1];  /* game thread only */
    int32_t own_position = -1;
    memset(ordered, 0, sizeof(ordered));
    for (int i = 0; i < PIT_EXIT_ENTRIES; ++i) {
        uint8_t *car = (uint8_t *)(uintptr_t)active[i];
        if (!car) continue;
        int32_t index = *(int32_t *)(car + 4);
        if (index >= 0 && index < PIT_EXIT_ENTRIES) ordered[i] = live[index];
        if (index == own_index) own_position = i;
    }
    if (own_position < 0) return original(scheduler, own_index);
    return original(ordered, own_position);
}


/* Fix C (FastestLapFix, owner 2026-10-10: "X has put up the fastest lap of the
 * race" voice on every S/F crossing). sub_4D1C40 (Steward 0x8ACEE0, called by
 * LapCompletion 0x4BC4E0 on a personal best) keeps the slot of the session's
 * fastest holder per sector at this+400+sector and treats a holder slot >=
 * active count (0x5C329C) as "no holder" -> announces + takes over. CO-OP bots
 * are slots 32+, so once a bot holds the record every personal best by anyone
 * is announced. Host and client: if the holder is a live car in a high slot,
 * compare the bests (veh+0x409C + 4*sector) like the native valid-holder path
 * and suppress the call when the new time is not quicker. */
static int g_fastest_lap_fix = 1;   /* INI [CoOp] FastestLapFix */
static const uintptr_t ADDR_FASTEST_HOLDER_UPDATE = 0x004D1C40;
static const uint8_t FastestHolder_expected[] = {0x8A, 0x44, 0x24, 0x0C, 0x84, 0xC0, 0x53};
static F1CDetour g_fastest_holder_detour;
static volatile LONG g_fastest_logged;
typedef uint8_t(__attribute__((thiscall)) *FastestHolderFn)(void *steward, int32_t sector, int32_t slot, uint8_t force);

uint8_t __attribute__((thiscall)) fastest_holder_hook(void *steward, int32_t sector, int32_t slot, uint8_t force)
{
    FastestHolderFn original = (FastestHolderFn)g_fastest_holder_detour.trampoline;
    if (!g_stopping && g_fastest_lap_fix && !force && steward && coop_mode_is_selected() &&
        sector >= 0 && sector < 4 && slot >= 0 && slot < MAX_VEHICLES) {
        int32_t holder = ((uint8_t *)steward)[400 + sector];
        int32_t active = *(volatile int32_t *)ADDR_ACTIVE_COUNT;
        uint8_t *holder_car = holder < MAX_VEHICLES ? (uint8_t *)f1c_vehicle_slot(holder) : NULL;
        if (holder >= active && holder_car && *(int32_t *)(holder_car + 0x32C8) != -1) {
            uint8_t *car = (uint8_t *)f1c_vehicle_slot(slot);
            float mine = car ? *(float *)(car + 0x409C + 4 * sector) : -1.0f;
            float best = *(float *)(holder_car + 0x409C + 4 * sector);
            if (!(mine != -1.0f && best >= mine)) {
                if (InterlockedIncrement(&g_fastest_logged) <= 20)
                    log_line("fastest lap: kept holder slot %ld (%.3f) over slot %ld (%.3f), sector %ld",
                             (long)holder, best, (long)slot, mine, (long)sector);
                return 0;
            }
        }
    }
    return original(steward, sector, slot, force);
}

typedef void *(__attribute__((thiscall)) *WaypointPlaneQueryFn)(void *tree, float *pos, int32_t set);
typedef void *(__attribute__((thiscall)) *WaypointTreeSearchFn)(void *tree, float *pos, void *hint, int32_t set, char flag);
static const uintptr_t ADDR_WAYPOINT_TREE = 0x005EA2C0;
static const uintptr_t ADDR_WAYPOINT_PLANE_QUERY = 0x004038E0;
static const uintptr_t ADDR_WAYPOINT_TREE_SEARCH = 0x00403AA0;
static volatile LONG g_remote_track_logged;
/* 2026-10-10 crash guard (docs/CRASH_4038E1_2026-10-10.md): two host crashes inside
 * 0x4038E0 (+0x01 tree->count, +0x32 node walk). The tree header is count +0x130,
 * node array +0x134, node stride 316 (0x13C) - from 0x4038E0. Before any native
 * query: tree, count and the whole node array must be readable, and the car's hint
 * must be a node of THIS array (else it is treated as NULL). g_track_in_query marks
 * the call so a crash inside it is attributed in the log/usage summary. */
static volatile LONG g_track_in_query;
static volatile LONG g_track_bad_logged;
static const int WAYPOINT_NODE_SIZE = 316;
static int waypoint_tree_ok(void *tree, uint8_t **array_out, int32_t *count_out)
{
    if (!tree || IsBadReadPtr(tree, 0x138)) return 0;
    int32_t count = *(int32_t *)((uint8_t *)tree + 0x130);
    uint8_t *array = *(uint8_t **)((uint8_t *)tree + 0x134);
    if (count <= 0 || count > 200000 || !array ||
        IsBadReadPtr(array, (UINT_PTR)count * WAYPOINT_NODE_SIZE)) return 0;
    *array_out = array;
    *count_out = count;
    return 1;
}
static int waypoint_node_ok(const uint8_t *array, int32_t count, const void *node)
{
    const uint8_t *n = (const uint8_t *)node;
    if (n < array || n >= array + (size_t)count * WAYPOINT_NODE_SIZE) return 0;
    return ((size_t)(n - array) % WAYPOINT_NODE_SIZE) == 0;
}

/* dev13 SlowLaps (owner 2026-10-10): practice/qualifying bots drive the out lap
 * and the in/cool-down lap at ~70% pace, flying laps flat out. Out lap: slow until
 * SLOW_LAPS_PUSH_M before the S/F line (or the line itself), then full pace so the
 * flying lap starts at full speed. In lap: PitStatus 0x3450 == 1 (heading in) or
 * FinishState == 1 (took the flag). Lever: native throttle cap (see slow_lap_set). Track length learned as the
 * largest LapDistance 0x4044 seen this session. Game thread only. */
static int g_slow_laps = 0;                /* INI [CoOp] SlowLaps (0.9.3: off by default, cosmetic) */
static float g_slow_laps_factor = 0.75f;   /* INI [CoOp] SlowLapsThrottle (percent, native cool-down = 75) */
static const float SLOW_LAPS_PUSH_M = 300.0f;
enum { VEH_PIT_STATUS = 0x3450, VEH_LAP_NUMBER = 0x403C, VEH_LAP_DIST = 0x4044, VEH_FINISH = 0x4064 };
static int8_t g_slow_on[MAX_VEHICLES];
static int8_t g_outlap[MAX_VEHICLES];
static int32_t g_outlap_lap[MAX_VEHICLES];
static int32_t g_prev_pit[MAX_VEHICLES];
static float g_prev_dist[MAX_VEHICLES];
static float g_track_len;
static int32_t g_slow_session = -1;
static volatile LONG g_slow_logged;

/* dev16: lever = the game's own cool-down throttle cap (FACT
 * ai-throttle-cap-457c-cooldown-20261010). After the flag AIAbility 0x429470 sets
 * veh+0x457C = AIDifficultyScalar x 0.75 and AISteeringCorrection 0x426CF0 clamps
 * the AI throttle 0x4434 to it. SlowLaps applies the same cap on out laps and in
 * laps: saved cap x SlowLapsThrottle/100 (default 75), written EVERY frame because
 * AIAbility rewrites 0x457C every 4 s. Release writes the saved cap back. The in
 * lap's "pit this lap" flag 0x454C is the game's own (ForcePitExit / phase 8);
 * SlowLaps never writes it (on an out lap it would send the car into the pits).
 * dev13-dev15 levers (pace scalar, speed governor, rev limit) replaced. */
enum { VEH_THROTTLE_CAP = 0x457C };
static float g_saved_cap[MAX_VEHICLES];

static void slow_lap_set(int32_t slot, uint8_t *v, int want)
{
    float *cap = (float *)(v + VEH_THROTTLE_CAP);
    if (want) {
        if (!g_slow_on[slot]) {
            g_saved_cap[slot] = *cap > 0.5f && *cap <= 1.5f ? *cap : 1.0f;
            g_slow_on[slot] = 1;
        }
        *cap = g_saved_cap[slot] * g_slow_laps_factor;
    } else if (g_slow_on[slot]) {
        *cap = g_saved_cap[slot];
        g_slow_on[slot] = 0;
    }
}

/* dev17 BlueFlags (owner 2026-10-10: an in-lap car ruined Montermini's lap).
 * Vanilla qualifying: only the FLYING car reacts to a slow car (interaction
 * target 0x45FC, lateral -3 m) and AISteeringCorrection then caps ITS throttle to
 * 0.75 (findings ai-quali-flying-lap-car-steers-around-2026-10-02,
 * ai-throttle-cap-457c-latches-after-traffic-target-2026-10-03); the slow car holds
 * its line. Host CO-OP practice/qualifying:
 *  - slow-lap bot (out/in lap) with a faster car (bot on a flying lap or any human)
 *    0..BLUE_FLAG_M behind on track: yield like the native team-order yield -
 *    interaction target 0x45FC = that car (never its own pointer: crash finding
 *    ai-yield-target-self-pointer-crash-2026-10-02), lateral request 0x4420 +3 m;
 *    released (only if still ours) once the car is past or out of range.
 *  - flying-lap bot: throttle cap 0x457C held at its full value (no 0.75 penalty). */
static int g_blue_flags = 0;              /* INI [CoOp] BlueFlags (0.9.3: off by default, not live-tested) */
static const float BLUE_FLAG_M = 150.0f;
enum { VEH_TARGET = 0x45FC, VEH_LATERAL_REQ = 0x4420 };
static float g_full_cap[MAX_VEHICLES];
static void *g_blue_target[MAX_VEHICLES];
static volatile LONG g_blue_logged;

static void blue_flag_release(int32_t slot, uint8_t *v)
{
    if (!g_blue_target[slot]) return;
    if (v && *(void **)(v + VEH_TARGET) == g_blue_target[slot]) {
        *(void **)(v + VEH_TARGET) = NULL;
        *(float *)(v + VEH_LATERAL_REQ) = 0.0f;
    }
    g_blue_target[slot] = NULL;
}

static void step_blue_flags(void)
{
    if (!g_blue_flags || g_track_len < 1000.0f) return;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        uint8_t *v = (uint8_t *)f1c_vehicle_slot(slot);
        if (!v) { g_blue_target[slot] = NULL; continue; }
        int bot = *(int32_t *)(v + 0x32C8) == 1 && slot_is_owned_ai(slot);
        int on_track = *(int32_t *)(v + VEH_PIT_STATUS) == 0 || *(int32_t *)(v + VEH_PIT_STATUS) == 1;
        if (bot && on_track && !g_slow_on[slot]) {           /* flying lap: no vanilla 0.75 penalty */
            float *cap = (float *)(v + VEH_THROTTLE_CAP);
            if (*cap > g_full_cap[slot] && *cap <= 1.5f) g_full_cap[slot] = *cap;
            if (g_full_cap[slot] > 0.5f && *cap < g_full_cap[slot] && *(int32_t *)(v + VEH_FINISH) != 1)
                *cap = g_full_cap[slot];
        }
        if (!bot || !g_slow_on[slot] || !on_track) { blue_flag_release(slot, v); continue; }
        float my_dist = *(float *)(v + VEH_LAP_DIST), my_speed = *(float *)(v + 0x4440);
        uint8_t *best = NULL; float best_gap = BLUE_FLAG_M;
        for (int32_t other = 0; other < MAX_VEHICLES; ++other) {
            if (other == slot) continue;
            uint8_t *o = (uint8_t *)f1c_vehicle_slot(other);
            if (!o) continue;
            int32_t role = *(int32_t *)(o + 0x32C8);
            if (role < 0 || role > 2 || *(int32_t *)(o + VEH_PIT_STATUS) != 0) continue;
            int fast = role != 1 ? 1 : (slot_is_owned_ai(other) && !g_slow_on[other] &&
                                        *(int32_t *)(o + VEH_FINISH) != 1);
            if (!fast) continue;
            float gap = my_dist - *(float *)(o + VEH_LAP_DIST);
            if (gap < 0.0f) gap += g_track_len;
            if (gap <= 0.0f || gap >= best_gap) continue;
            if (role == 1 && *(float *)(o + 0x4440) < my_speed - 5.0f) continue;
            best = o; best_gap = gap;
        }
        if (!best) { blue_flag_release(slot, v); continue; }
        void *cur = *(void **)(v + VEH_TARGET);
        if (cur && cur != g_blue_target[slot] && cur != best) continue;   /* game's own target */
        if (g_blue_target[slot] != best && InterlockedIncrement(&g_blue_logged) <= 60)
            log_line("blue flag: %s yields to %s (gap %.0f m)", ((F1CVehicle *)v)->DriverName,
                     ((F1CVehicle *)best)->DriverName, best_gap);
        *(void **)(v + VEH_TARGET) = best;
        *(float *)(v + VEH_LATERAL_REQ) = 3.0f;
        g_blue_target[slot] = best;
    }
}

static void blue_flags_release_all(void)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot)
        if (g_blue_target[slot]) blue_flag_release(slot, (uint8_t *)f1c_vehicle_slot(slot));
}

static void slow_laps_release_all(void)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        if (!g_slow_on[slot]) continue;
        uint8_t *v = (uint8_t *)f1c_vehicle_slot(slot);
        if (v) slow_lap_set(slot, v, 0); else g_slow_on[slot] = 0;
    }
}

static void step_slow_laps(int host, int32_t session_type)
{
    if (g_stopping || !g_slow_laps || !host || session_type == 7 || session_type < 1 || session_type > 6) {
        slow_laps_release_all();
        blue_flags_release_all();
        g_slow_session = -1;
        return;
    }
    if (session_type != g_slow_session) {
        slow_laps_release_all();
        blue_flags_release_all();
        memset(g_full_cap, 0, sizeof(g_full_cap));
        memset(g_outlap, 0, sizeof(g_outlap));
        for (int32_t i = 0; i < MAX_VEHICLES; ++i) { g_prev_pit[i] = 4; g_prev_dist[i] = 0.0f; }
        g_track_len = 0.0f;
        g_slow_session = session_type;
    }
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        uint8_t *v = (uint8_t *)f1c_vehicle_slot(slot);
        if (!v || *(int32_t *)(v + 0x32C8) != 1 || !slot_is_owned_ai(slot)) {
            if (g_slow_on[slot] && v) slow_lap_set(slot, v, 0);
            g_slow_on[slot] = 0;
            continue;
        }
        int32_t pit = *(int32_t *)(v + VEH_PIT_STATUS);
        int32_t lap = *(int32_t *)(v + VEH_LAP_NUMBER);
        float dist = *(float *)(v + VEH_LAP_DIST);
        /* dev14: learn the track length only from a lap wrap (LapDistance falls back
         * near 0). dev13 used the running maximum, which grew with the lead car's own
         * out lap and released it at once (Salo push at dist=1008/1008). */
        if (g_prev_dist[slot] > 1000.0f && dist < g_prev_dist[slot] * 0.5f && g_prev_dist[slot] < 20000.0f &&
            g_prev_dist[slot] > g_track_len)
            g_track_len = g_prev_dist[slot];
        g_prev_dist[slot] = dist;
        if (pit == 0 && g_prev_pit[slot] != 0) { g_outlap[slot] = 1; g_outlap_lap[slot] = lap; }
        g_prev_pit[slot] = pit;
        if (g_outlap[slot] && (pit != 0 || lap != g_outlap_lap[slot] ||
                               (g_track_len > 1000.0f && dist > g_track_len - SLOW_LAPS_PUSH_M)))
            g_outlap[slot] = 0;
        /* dev16: after the flag (FinishState 1) the game already applies its own 0.75
         * cap; SlowLaps only covers in laps before the flag (PitStatus 1). */
        int in_lap = pit == 1 && *(int32_t *)(v + VEH_FINISH) != 1;
        int want = pit == 0 ? (g_outlap[slot] || in_lap) : in_lap;
        if (want != g_slow_on[slot] && InterlockedIncrement(&g_slow_logged) <= 60)
            log_line("slow laps: %s %s (%s, lap=%ld dist=%.0f/%.0f)", ((F1CVehicle *)v)->DriverName,
                     want ? "slow" : "push", g_outlap[slot] ? "out lap" : (in_lap ? "in lap" : "flying"),
                     (long)lap, dist, g_track_len);
        slow_lap_set(slot, v, want);
    }
    step_blue_flags();
}

static void refresh_remote_car_tracking(void)
{
    if (g_stopping || !g_remote_tracking_fix || !coop_host_now()) return;
    /* 2026-10-11 FACT: the waypoint tree object lives AT 0x5EA2C0 (native code
     * passes ECX=0x5EA2C0 at 53 sites; never loads a pointer from it). The old
     * dereference read 0x41033333 (float 8.2), so this whole waypoint half was
     * skipped every session (findings: coop-remote-tracking-waypoint-tree-is-
     * address-not-pointer-20261011). */
    void *tree = (void *)ADDR_WAYPOINT_TREE;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        uint8_t *v = (uint8_t *)f1c_vehicle_slot(slot);
        if (!v || *(int32_t *)(v + 0x32C8) != 2) continue;          /* remote human copy only */
        float *pos = (float *)(v + 0x34E0);
        float *vel = (float *)(v + 0x34F8);
        *(float *)(v + 0x4440) = sqrtf(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);
        if (!tree) continue;
        uint8_t *nodes;
        int32_t node_count;
        if (!waypoint_tree_ok(tree, &nodes, &node_count)) {
            if (InterlockedExchange(&g_track_bad_logged, 1) == 0)
                log_line("COOP-W11 remote car tracking: waypoint tree %p not readable - skipped", tree);
            return;
        }
        if (!(pos[0] == pos[0]) || !(pos[1] == pos[1]) || !(pos[2] == pos[2])) continue;
        float *hint = *(float **)(v + 0x4354);
        if (hint && !waypoint_node_ok(nodes, node_count, hint)) {
            if (InterlockedExchange(&g_track_bad_logged, 1) == 0)
                log_line("COOP-W11 remote car tracking: slot %ld waypoint %p is not a node of tree %p - re-query",
                         (long)slot, (void *)hint, tree);
            hint = NULL;
        }
        int32_t set = *(int32_t *)(v + 0x4550);
        void *found;
        InterlockedExchange(&g_track_in_query, 1);
        if (!hint) {
            found = ((WaypointPlaneQueryFn)ADDR_WAYPOINT_PLANE_QUERY)(tree, pos, set);
        } else {
            float dx = pos[0] - hint[37], dz = pos[2] - hint[39];
            found = dx * dx + dz * dz > 3200.0f
                ? ((WaypointPlaneQueryFn)ADDR_WAYPOINT_PLANE_QUERY)(tree, pos, set)
                : ((WaypointTreeSearchFn)ADDR_WAYPOINT_TREE_SEARCH)(tree, pos, hint, set, 0);
        }
        InterlockedExchange(&g_track_in_query, 0);
        if (found && !waypoint_node_ok(nodes, node_count, found)) found = NULL;
        if (found) {
            *(void **)(v + 0x4354) = found;
            /* sub_426430 l.126-138: waypoint set from the node (+0x12C); entering
             * the pit route registers the car in the pit-exit scheduler (by
             * veh+4), leaving it unregisters - so bots see a human in the pit lane. */
            int32_t now_set = *(int32_t *)((uint8_t *)found + 0x12C);
            int32_t index = *(int32_t *)(v + 4);
            if (index >= 0 && index < PIT_EXIT_ENTRIES) {
                uint32_t *sched = (uint32_t *)ADDR_PIT_SCHEDULER;
                if (set == 0 && now_set == 1) sched[index] = (uint32_t)(uintptr_t)v;
                else if (set == 1 && now_set == 0) sched[index] = 0;
            }
            *(int32_t *)(v + 0x4550) = now_set;
        }
        if (InterlockedIncrement(&g_remote_track_logged) == 1)
            log_line("remote car tracking: slot %ld speed=%.1f m/s waypoint=%p (first update)",
                     (long)slot, *(float *)(v + 0x4440), found);
    }
}

int32_t __attribute__((stdcall)) time_scale_set_hook(int32_t mode)
{
    if (mode == 0 && g_warp && !g_stopping && coop_host_now()) {
        int32_t result = ((TimeScaleSetFn)g_time_scale_detour.trampoline)(1);
        *(volatile double *)ADDR_TIME_SCALE = g_warp_scale;
        return result;
    }
    return ((TimeScaleSetFn)g_time_scale_detour.trampoline)(mode);
}

/* Client, race before the lights: the host's native order packet (0x462E20)
 * can arrive after our placement hook and put network players back on 1..N.
 * Re-apply the host GRID while RacePhase <= 4. Game thread. */
static void enforce_host_grid_client(void)
{
    static DWORD last;
    static LONG logged;
    if (GetTickCount() - last < 250) return;
    last = GetTickCount();
    int32_t desired[MAX_VEHICLES] = {}, active = 0, matched = 0, moved = 0;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (vehicle && vehicle->WearTickGate__was__slot_live_marker_ != -1) ++active;
    }
    EnterCriticalSection(&g_manifest_lock);
    for (int32_t i = 0; i < g_host_grid_count; ++i) {
        int32_t slot = g_host_grid_slot[i];
        if (slot < 0 || slot >= MAX_VEHICLES) continue;
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        desired[slot] = g_host_grid_pos[i];
        ++matched;
    }
    LeaveCriticalSection(&g_manifest_lock);
    if (matched != active || active == 0) return;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || desired[slot] <= 0) continue;
        if (vehicle->plannedPositionAtStart != desired[slot] || vehicle->position != desired[slot]) {
            commands::set_plannedPositionAtStart(vehicle, desired[slot]);
            commands::set_position(vehicle, desired[slot]);
            ++moved;
        }
    }
    if (moved && InterlockedIncrement(&logged) <= 20)
        log_line("race grid enforce (client): host GRID re-applied, moved=%ld", (long)moved);
}

/* ---- N2 host-bot state stream (stage 1 measure / stage 2 client apply) ----
 * Uses the 64-byte native record proven by the COOP_DEV_TOOLS LINK spike.
 * The worker owns UDP; hooks only copy a record under a short critical section. */
static const uintptr_t ADDR_BOT_STATE_ENCODE = 0x004C71C0;
static const uintptr_t ADDR_BOT_REMOTE_UPDATE = 0x004F03E0;
static const uintptr_t ADDR_BOT_REMOTE_APPLY = 0x004C73F0;
static const uint8_t BotStateEncode_expected[] = {0x51, 0xDD, 0x05, 0xA0, 0xFE, 0x8A, 0x00};
static const uint8_t BotRemoteUpdate_expected[] = {0x81, 0xEC, 0xB0, 0x01, 0x00, 0x00};
typedef int(__attribute__((thiscall)) *BotStateEncodeFn)(void *vehicle, void *out);
typedef void(__attribute__((thiscall)) *BotRemoteUpdateFn)(void *controller, float time);
typedef void(__attribute__((thiscall)) *BotRemoteApplyFn)(void *vehicle, void *record, float time);
static F1CDetour g_bot_state_encode_detour, g_bot_remote_update_detour;
enum { BOT_STREAM_IDS = 256, BOT_STREAM_RING = 64, BOT_STREAM_RECORD = 64, BOT_STREAM_HEADER = 17,
       BOT_STREAM_ENTRY = 69, BOT_STREAM_MAX_DGRAM = 1200,
       BOT_STREAM_PER_PACKET = (BOT_STREAM_MAX_DGRAM - BOT_STREAM_HEADER) / BOT_STREAM_ENTRY };
struct BotLatest { LONG valid; double captured_clock; uint8_t record[BOT_STREAM_RECORD]; };
struct BotSample { double host_clock, arrival_clock; uint8_t record[BOT_STREAM_RECORD]; };
struct BotPlayback {
    double clock, last_sample_clock, last_arrival_clock, blend_from_clock, blend_to_clock, blend_start_clock, hold_start_clock;
    float last_record_time;
    int has_pose, blending, holding;
    uint8_t record[BOT_STREAM_RECORD];
};
static BotLatest g_bot_latest[BOT_STREAM_IDS];
static BotSample g_bot_samples[BOT_STREAM_IDS][BOT_STREAM_RING];
static BotPlayback g_bot_playback[BOT_STREAM_IDS];
static int g_bot_sample_head[BOT_STREAM_IDS], g_bot_sample_count[BOT_STREAM_IDS];
static LONG g_bot_stream_seen[BOT_STREAM_IDS];
static DWORD g_bot_seen_tick[BOT_STREAM_IDS];
static CRITICAL_SECTION g_bot_stream_lock;
static HANDLE g_bot_stream_thread;
static volatile LONG g_bot_tx_packets, g_bot_tx_bytes, g_bot_rx_packets, g_bot_rx_gaps, g_bot_rx_reorders;
static volatile LONG g_bot_fallbacks, g_bot_driven, g_bot_hold_events, g_bot_hold_longest_ms, g_bot_recover_blends;
static volatile LONG g_bot_legacy_logged;
static volatile LONG g_stream_reshown;   /* stale-monitor keeper re-shows, client */
static double g_bot_clock_offset = 1e30;
static double g_bot_offset_current_min = 1e30, g_bot_offset_previous_min = 1e30;
static DWORD g_bot_offset_window;
static const int BOT_STREAM_PEERS = 8;
static const DWORD BOT_STREAM_PEER_TTL_MS = 5000;
struct BotStreamPeer { sockaddr_in addr; DWORD last_ms; };
static BotStreamPeer g_bot_peers[BOT_STREAM_PEERS];
static LONG g_bot_hello_nonce;
static double g_bot_late_sum;
static LONG g_bot_late_count;
static double g_bot_late_max;
static LARGE_INTEGER g_bot_qpc_frequency;
static LONG g_bot_sequence;
static LONG g_bot_encode_count;

static double bot_stream_now(void)
{
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    if (!g_bot_qpc_frequency.QuadPart) QueryPerformanceFrequency(&g_bot_qpc_frequency);
    return (double)qpc.QuadPart / (double)g_bot_qpc_frequency.QuadPart;
}

static int bot_stream_is_host(void)
{
    return !g_stopping && g_bot_stream > 0 && coop_mode_is_selected() && coop_host_now();
}

static int bot_stream_slot_for_vehicle(void *vehicle)
{
    if (!vehicle) return -1;
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot)
        if (f1c_vehicle_slot(slot) == vehicle) return slot;
    return -1;
}

static void bot_stream_max_longest(LONG value)
{
    LONG old = InterlockedCompareExchange(&g_bot_hold_longest_ms, 0, 0);
    while (value > old) {
        LONG seen = InterlockedCompareExchange(&g_bot_hold_longest_ms, value, old);
        if (seen == old) break;
        old = seen;
    }
}

static void bot_stream_reset(void)
{
    EnterCriticalSection(&g_bot_stream_lock);
    memset(g_bot_latest, 0, sizeof(g_bot_latest));
    memset(g_bot_samples, 0, sizeof(g_bot_samples));
    memset(g_bot_playback, 0, sizeof(g_bot_playback));
    memset(g_bot_sample_head, 0, sizeof(g_bot_sample_head));
    memset(g_bot_sample_count, 0, sizeof(g_bot_sample_count));
    for (int id = 0; id < BOT_STREAM_IDS; ++id) InterlockedExchange(&g_bot_stream_seen[id], 0);
    memset(g_bot_seen_tick, 0, sizeof(g_bot_seen_tick));
    g_bot_clock_offset = g_bot_offset_current_min = g_bot_offset_previous_min = 1e30;
    g_bot_offset_window = 0;
    g_bot_late_sum = g_bot_late_max = 0;
    g_bot_late_count = 0;
    LeaveCriticalSection(&g_bot_stream_lock);
    InterlockedExchange(&g_bot_hold_events, 0);
    InterlockedExchange(&g_bot_hold_longest_ms, 0);
    InterlockedExchange(&g_bot_recover_blends, 0);
    InterlockedExchange(&g_bot_fallbacks, 0);
    InterlockedExchange(&g_bot_driven, 0);
}

static void bot_stream_store_packet(const char *packet, int length)
{
    uint32_t sequence;
    double host_clock, arrival = bot_stream_now(), late;
    uint8_t count;
    memcpy(&sequence, packet + 4, sizeof(sequence));
    memcpy(&host_clock, packet + 8, sizeof(host_clock));
    count = (uint8_t)packet[16];
    if (length != BOT_STREAM_HEADER + count * BOT_STREAM_ENTRY || count > BOT_STREAM_PER_PACKET) return;
    EnterCriticalSection(&g_bot_stream_lock);
    {
        DWORD tick = GetTickCount();
        double sample_offset = arrival - host_clock;
        if (!g_bot_offset_window) {
            g_bot_offset_window = tick;
            g_bot_offset_current_min = sample_offset;
        } else if (tick - g_bot_offset_window >= 10000) {
            g_bot_offset_previous_min = g_bot_offset_current_min;
            g_bot_offset_current_min = sample_offset;
            g_bot_offset_window = tick;
        } else if (sample_offset < g_bot_offset_current_min) {
            g_bot_offset_current_min = sample_offset;
        }
        g_bot_clock_offset = g_bot_offset_current_min < g_bot_offset_previous_min
            ? g_bot_offset_current_min : g_bot_offset_previous_min;
    }
    late = arrival - host_clock - g_bot_clock_offset;
    if (late < 0) late = 0;
    g_bot_late_sum += late;
    ++g_bot_late_count;
    if (late > g_bot_late_max) g_bot_late_max = late;
    for (int i = 0; i < count; ++i) {
        const char *entry = packet + BOT_STREAM_HEADER + i * BOT_STREAM_ENTRY;
        int id = (uint8_t)entry[0];
        float capture_offset_ms;
        memcpy(&capture_offset_ms, entry + 1, sizeof(capture_offset_ms));
        if (!(capture_offset_ms >= 0.0f && capture_offset_ms <= 5000.0f)) continue;
        double sample_clock = host_clock - (double)capture_offset_ms / 1000.0;
        BotPlayback *playback = &g_bot_playback[id];
        if (g_bot_sample_count[id] > 0) {
            int last = (g_bot_sample_head[id] - 1 + BOT_STREAM_RING) % BOT_STREAM_RING;
            double last_clock = g_bot_samples[id][last].host_clock;
            if (sample_clock < last_clock) continue;
            if (sample_clock == last_clock) {
                /* 30 Hz packets can repeat a 10 Hz native capture; count transport
                 * activity without inserting a duplicate sample into the ring. */
                if (playback->last_arrival_clock > 0.0 && arrival - playback->last_arrival_clock > 0.100 &&
                    playback->has_pose && !playback->blending) {
                    playback->blending = 1;
                    playback->blend_from_clock = playback->clock;
                    playback->blend_to_clock = arrival - g_bot_clock_offset;
                    playback->blend_start_clock = arrival;
                    InterlockedIncrement(&g_bot_recover_blends);
                }
                if (playback->holding) {
                    bot_stream_max_longest((LONG)((arrival - playback->hold_start_clock) * 1000.0));
                    playback->holding = 0;
                }
                playback->last_arrival_clock = arrival;
                g_bot_seen_tick[id] = GetTickCount();
                continue;
            }
        }
        if (playback->last_arrival_clock > 0.0 && arrival - playback->last_arrival_clock > 0.100 &&
            playback->has_pose && !playback->blending) {
            playback->blending = 1;
            playback->blend_from_clock = playback->clock;
            playback->blend_to_clock = arrival - g_bot_clock_offset;
            playback->blend_start_clock = arrival;
            InterlockedIncrement(&g_bot_recover_blends);
        }
        if (playback->holding) {
            bot_stream_max_longest((LONG)((arrival - playback->hold_start_clock) * 1000.0));
            playback->holding = 0;
        }
        playback->last_arrival_clock = arrival;
        int head = g_bot_sample_head[id];
        BotSample *sample = &g_bot_samples[id][head];
        sample->host_clock = sample_clock;
        sample->arrival_clock = arrival;
        memcpy(sample->record, entry + 5, BOT_STREAM_RECORD);
        g_bot_sample_head[id] = (head + 1) % BOT_STREAM_RING;
        if (g_bot_sample_count[id] < BOT_STREAM_RING) ++g_bot_sample_count[id];
        InterlockedExchange(&g_bot_stream_seen[id], 1);
        g_bot_seen_tick[id] = GetTickCount();
    }
    LeaveCriticalSection(&g_bot_stream_lock);
    InterlockedIncrement(&g_bot_rx_packets);
    (void)sequence;
}

/* The inspected native apply consumes packed pose fields independently of its
 * time argument. Keep the last pose through stalls; do not synthesize packed
 * coordinates. The playback clock advances through samples and blends on return. */
static int bot_stream_pick(int net_id, double *sample_clock, double *delta,
                           uint8_t out[BOT_STREAM_RECORD])
{
    int found = 0;
    double now = bot_stream_now();
    double max_gap = 1.0 / (double)(g_bot_stream_hz > 0 ? g_bot_stream_hz : 30);
    if (net_id < 0 || net_id >= BOT_STREAM_IDS) return 0;
    EnterCriticalSection(&g_bot_stream_lock);
    BotPlayback *playback = &g_bot_playback[net_id];
    if (g_bot_clock_offset < 1e29) {
        double live_target = now - g_bot_clock_offset;
        double target = live_target;
        if (playback->blending) {
            double progress = (now - playback->blend_start_clock) * 1000.0 /
                              (double)(g_bot_stream_blend_ms > 0 ? g_bot_stream_blend_ms : 400);
            if (progress >= 1.0) playback->blending = 0;
            else if (progress > 0.0)
                target = playback->blend_from_clock +
                         (playback->blend_to_clock - playback->blend_from_clock) * progress +
                         (now - playback->blend_start_clock);
        }
        if (playback->clock > 0.0 && target < playback->clock) target = playback->clock;
        if (playback->last_arrival_clock > 0.0 &&
            now - playback->last_arrival_clock > g_bot_stream_hold_ms / 1000.0 && playback->clock > 0.0)
            target = playback->clock;
        double best_clock = -1e30;
        BotSample *best = NULL;
        for (int n = 0; n < g_bot_sample_count[net_id]; ++n) {
            int idx = (g_bot_sample_head[net_id] - 1 - n + BOT_STREAM_RING) % BOT_STREAM_RING;
            BotSample *sample = &g_bot_samples[net_id][idx];
            if (sample->host_clock <= target && sample->host_clock >= playback->last_sample_clock &&
                sample->host_clock > best_clock) {
                best_clock = sample->host_clock;
                best = sample;
            }
        }
        if (best) {
            memcpy(playback->record, best->record, BOT_STREAM_RECORD);
            playback->last_sample_clock = best->host_clock;
            if (best->host_clock > playback->clock) playback->clock = best->host_clock;
            playback->has_pose = 1;
        }
        if (playback->has_pose) {
            double age = target - playback->last_sample_clock;
            if (age < 0.0) age = 0.0;
            if (age > max_gap && age < g_bot_stream_hold_ms / 1000.0) age = max_gap;
            if (age > g_bot_stream_hold_ms / 1000.0) age = g_bot_stream_hold_ms / 1000.0;
            *delta = age;
            *sample_clock = playback->last_sample_clock;
            memcpy(out, playback->record, BOT_STREAM_RECORD);
            found = 1;
            if (playback->last_arrival_clock > 0.0 &&
                now - playback->last_arrival_clock > g_bot_stream_hold_ms / 1000.0) {
                if (!playback->holding) {
                    playback->holding = 1;
                    playback->hold_start_clock = playback->last_arrival_clock + g_bot_stream_hold_ms / 1000.0;
                    InterlockedIncrement(&g_bot_hold_events);
                }
                bot_stream_max_longest((LONG)((now - playback->hold_start_clock) * 1000.0));
            }
        }
    }
    LeaveCriticalSection(&g_bot_stream_lock);
    return found;
}

static void bot_stream_send_tick(SOCKET sock, DWORD *last_report)
{
    uint8_t records[BOT_STREAM_IDS][BOT_STREAM_ENTRY];
    uint8_t packet[BOT_STREAM_MAX_DGRAM];
    int n = 0, peers = 0;
    DWORD tick = GetTickCount();
    double host_clock = bot_stream_now();
    sockaddr_in peer_copy[BOT_STREAM_PEERS];
    EnterCriticalSection(&g_bot_stream_lock);
    for (int id = 0; id < BOT_STREAM_IDS; ++id) {
        if (!g_bot_latest[id].valid || host_clock - g_bot_latest[id].captured_clock > 0.5) continue;
        records[n][0] = (uint8_t)id;
        float capture_offset_ms = (float)((host_clock - g_bot_latest[id].captured_clock) * 1000.0);
        memcpy(records[n] + 1, &capture_offset_ms, sizeof(capture_offset_ms));
        memcpy(records[n] + 5, g_bot_latest[id].record, BOT_STREAM_RECORD);
        if (++n == BOT_STREAM_IDS) break;
    }
    LeaveCriticalSection(&g_bot_stream_lock);
    for (int i = 0; i < BOT_STREAM_PEERS; ++i) {
        if (!g_bot_peers[i].last_ms || tick - g_bot_peers[i].last_ms > BOT_STREAM_PEER_TTL_MS) continue;
        peer_copy[peers++] = g_bot_peers[i].addr;
    }
    for (int base = 0; base < n; base += BOT_STREAM_PER_PACKET) {
        int count = n - base;
        if (count > BOT_STREAM_PER_PACKET) count = BOT_STREAM_PER_PACKET;
        uint32_t seq = (uint32_t)InterlockedIncrement(&g_bot_sequence);
        memcpy(packet, "CB02", 4);
        memcpy(packet + 4, &seq, sizeof(seq));
        memcpy(packet + 8, &host_clock, sizeof(host_clock));
        packet[16] = (uint8_t)count;
        memcpy(packet + BOT_STREAM_HEADER, &records[base][0], count * BOT_STREAM_ENTRY);
        int length = BOT_STREAM_HEADER + count * BOT_STREAM_ENTRY;
        for (int p = 0; p < peers; ++p) {
            if (sendto(sock, (const char *)packet, length, 0,
                       (sockaddr *)&peer_copy[p], sizeof(peer_copy[p])) >= 0) {
                InterlockedIncrement(&g_bot_tx_packets);
                InterlockedExchangeAdd(&g_bot_tx_bytes, length);
            }
        }
    }
    if (!*last_report) *last_report = tick;
    else if (tick - *last_report >= 5000) {
        LONG packets = InterlockedExchange(&g_bot_tx_packets, 0);
        LONG bytes = InterlockedExchange(&g_bot_tx_bytes, 0);
        LONG encodes = InterlockedExchange(&g_bot_encode_count, 0);
        DWORD elapsed = tick - *last_report;
        log_line("bot stream: host sent %ld pkts/s %.1f KB/s to %d peers, encodes %ld/s, bots %d",
                 (long)(packets * 1000 / elapsed), bytes * 1.0 / elapsed, peers,
                 (long)(encodes * 1000 / elapsed), n);
        *last_report = tick;
    }
}

static void bot_stream_remember_peer(const sockaddr_in &from)
{
    int free_index = -1, oldest = 0;
    DWORD now = GetTickCount();
    for (int i = 0; i < BOT_STREAM_PEERS; ++i) {
        BotStreamPeer *peer = &g_bot_peers[i];
        if (peer->last_ms && peer->addr.sin_addr.s_addr == from.sin_addr.s_addr &&
            peer->addr.sin_port == from.sin_port) {
            peer->last_ms = now ? now : 1;
            return;
        }
        if (free_index < 0 && (!peer->last_ms || now - peer->last_ms > BOT_STREAM_PEER_TTL_MS))
            free_index = i;
        if (now - g_bot_peers[i].last_ms > now - g_bot_peers[oldest].last_ms) oldest = i;
    }
    int slot = free_index >= 0 ? free_index : oldest;
    g_bot_peers[slot].addr = from;
    g_bot_peers[slot].last_ms = now ? now : 1;
    log_line("bot stream: peer %s:%u", inet_ntoa(from.sin_addr), (unsigned)ntohs(from.sin_port));
}

static void bot_stream_receive_hello(SOCKET sock)
{
    char packet[16];
    sockaddr_in from = {};
    int from_len = sizeof(from);
    int got = recvfrom(sock, packet, sizeof(packet), 0, (sockaddr *)&from, &from_len);
    if (got == 9 && memcmp(packet, "CB02?", 5) == 0)
        bot_stream_remember_peer(from);
}

static void bot_stream_send_hello(SOCKET sock, DWORD *last_hello)
{
    DWORD tick = GetTickCount();
    sockaddr_in host = {};
    char packet[9] = "CB02?";
    uint32_t nonce;
    if (!g_sync_host_ip || tick - *last_hello < 1000) return;
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = g_sync_host_ip;
    host.sin_port = htons((u_short)g_bot_stream_port);
    nonce = (uint32_t)InterlockedIncrement(&g_bot_hello_nonce);
    memcpy(packet + 5, &nonce, sizeof(nonce));
    if (sendto(sock, packet, sizeof(packet), 0, (sockaddr *)&host, sizeof(host)) >= 0)
        *last_hello = tick;
}

static DWORD WINAPI bot_stream_worker(void *)
{
    WSADATA wsa;
    SOCKET sock;
    sockaddr_in bind_address = {};
    DWORD timeout = 50, last_report = 0, last_hello = 0;
    uint32_t last_sequence = 0;
    int32_t last_state = -1;
    double next_send = 0.0;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { log_line("bot stream: WSA startup failed"); return 0; }
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return 0; }
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons((u_short)g_bot_stream_port);
    bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    if (bind(sock, (sockaddr *)&bind_address, sizeof(bind_address)) != 0) {
        log_line("bot stream: UDP %d bind failed: %d", g_bot_stream_port, WSAGetLastError());
        closesocket(sock); WSACleanup(); return 0;
    }
    log_line("bot stream: UDP %d ready", g_bot_stream_port);
    last_report = GetTickCount();
    while (!g_stopping) {
        int32_t state = coop_mode_is_selected() ? network_state() : 0;
        DWORD tick = GetTickCount();
        if (last_state >= 0 && state != last_state) {
            bot_stream_reset();
            last_sequence = 0;
            next_send = 0.0;
            last_report = tick;
        }
        last_state = state;
        if ((state == 1 || state == 2) && g_bot_stream > 0) {
            u_long nonblocking = 1;
            double now = bot_stream_now();
            double period = 1.0 / (double)(g_bot_stream_hz > 0 ? g_bot_stream_hz : 30);
            ioctlsocket(sock, FIONBIO, &nonblocking);
            bot_stream_receive_hello(sock);
            if (next_send <= 0.0) next_send = now;
            if (now >= next_send) {
                bot_stream_send_tick(sock, &last_report);
                do { next_send += period; } while (next_send <= now);
            }
            Sleep(1);
        } else if (state == 3 && g_bot_stream > 0) {
            u_long blocking = 0;
            ioctlsocket(sock, FIONBIO, &blocking);
            bot_stream_send_hello(sock, &last_hello);
            char packet[BOT_STREAM_MAX_DGRAM];
            sockaddr_in from = {};
            int from_len = sizeof(from);
            int got = recvfrom(sock, packet, sizeof(packet), 0, (sockaddr *)&from, &from_len);
            if (got >= 4 && memcmp(packet, "CB01", 4) == 0 && g_sync_host_ip &&
                from.sin_addr.s_addr == g_sync_host_ip && InterlockedCompareExchange(&g_bot_legacy_logged, 1, 0) == 0)
                log_line("bot stream: rejected legacy CB01 packet; CB02 required");
            if (got >= BOT_STREAM_HEADER && memcmp(packet, "CB02", 4) == 0 &&
                g_sync_host_ip && from.sin_addr.s_addr == g_sync_host_ip) {
                uint32_t seq;
                memcpy(&seq, packet + 4, sizeof(seq));
                if (last_sequence && seq > last_sequence + 1) InterlockedExchangeAdd(&g_bot_rx_gaps, (LONG)(seq - last_sequence - 1));
                else if (last_sequence && (int32_t)(seq - last_sequence) <= 0) InterlockedIncrement(&g_bot_rx_reorders);
                if (!last_sequence || (int32_t)(seq - last_sequence) > 0) last_sequence = seq;
                bot_stream_store_packet(packet, got);
            }
            if (tick - last_report >= 5000) {
                LONG packets = InterlockedExchange(&g_bot_rx_packets, 0);
                LONG gaps = InterlockedExchange(&g_bot_rx_gaps, 0);
                LONG reorders = InterlockedExchange(&g_bot_rx_reorders, 0);
                double late_sum, late_max, offset;
                LONG late_count, bots = 0;
                EnterCriticalSection(&g_bot_stream_lock);
                late_sum = g_bot_late_sum; late_max = g_bot_late_max; late_count = g_bot_late_count;
                offset = g_bot_clock_offset;
                g_bot_late_sum = g_bot_late_max = 0; g_bot_late_count = 0;
                for (int id = 0; id < BOT_STREAM_IDS; ++id)
                    if (g_bot_stream_seen[id] && tick - g_bot_seen_tick[id] <= 5000) ++bots;
                LeaveCriticalSection(&g_bot_stream_lock);
                log_line("bot stream: client rx %ld pkts/s loss %.1f%% late avg %.1f ms max %.1f ms, bots %ld, offset %.3f ms reorders %ld fallback %ld stream-driven %ld holds %ld longest %ld ms recover-blends %ld reshown %ld",
                         (long)(packets / 5), packets + gaps ? 100.0 * gaps / (packets + gaps) : 0.0,
                         late_count ? late_sum * 1000.0 / late_count : 0.0, late_max * 1000.0,
                         (long)bots, offset < 1e29 ? offset * 1000.0 : -1.0, (long)reorders,
                         (long)InterlockedExchange(&g_bot_fallbacks, 0), (long)InterlockedExchange(&g_bot_driven, 0),
                         (long)InterlockedExchange(&g_bot_hold_events, 0),
                         (long)InterlockedExchange(&g_bot_hold_longest_ms, 0),
                         (long)InterlockedExchange(&g_bot_recover_blends, 0),
                         (long)InterlockedExchange(&g_stream_reshown, 0));
                last_report = tick;
            }
        } else Sleep(5);
    }
    closesocket(sock);
    WSACleanup();
    return 0;
}

static int __attribute__((thiscall, unused)) bot_state_encode_hook(void *vehicle, void *out)
{
    int result = ((BotStateEncodeFn)g_bot_state_encode_detour.trampoline)(vehicle, out);
    if (bot_stream_is_host() && vehicle && out) {
        int slot = bot_stream_slot_for_vehicle(vehicle);
        if (slot >= 0 && slot_is_owned_ai(slot)) {
            int net_id = *(int32_t *)((uint8_t *)vehicle + 0x4894);
            if (net_id >= 0 && net_id < BOT_STREAM_IDS) {
                EnterCriticalSection(&g_bot_stream_lock);
                g_bot_latest[net_id].captured_clock = bot_stream_now();
                memcpy(g_bot_latest[net_id].record, out, BOT_STREAM_RECORD);
                InterlockedExchange(&g_bot_latest[net_id].valid, 1);
                LeaveCriticalSection(&g_bot_stream_lock);
                InterlockedIncrement(&g_bot_encode_count);
            }
        }
    }
    return result;
}

static void __attribute__((thiscall, unused)) bot_remote_update_hook(void *controller, float time)
{
    void *vehicle = controller ? *(void **)controller : NULL;
    int net_id = vehicle ? *(int32_t *)((uint8_t *)vehicle + 0x4894) : -1;
    int stream_active = g_bot_stream == 2 && !g_stopping && coop_mode_is_selected() &&
                        network_state() == 3 && vehicle && net_id >= 0 && net_id < BOT_STREAM_IDS;
    if (stream_active && InterlockedCompareExchange(&g_bot_stream_seen[net_id], 0, 0)) {
        /* 0x4C73F0 reads a full 432-byte native record; pad the stream prefix. */
        uint32_t record_full[108] = {};
        uint8_t *record = (uint8_t *)record_full;
        double sample_clock = 0.0, delta = 0.0;
        if (bot_stream_pick(net_id, &sample_clock, &delta, record)) {
            float apply_time = time - (float)delta;
            EnterCriticalSection(&g_bot_stream_lock);
            /* Session time restarts at 0 on every session change while the
             * client net state stays 3. Clamp only small backward steps; a big
             * one is a new session (live 2026-10-11 19:47: the clamp kept the
             * qualifying time, all bots vanished on the client at race start). */
            if (apply_time < g_bot_playback[net_id].last_record_time &&
                g_bot_playback[net_id].last_record_time - apply_time < 1.0f)
                apply_time = g_bot_playback[net_id].last_record_time;
            g_bot_playback[net_id].last_record_time = apply_time;
            LeaveCriticalSection(&g_bot_stream_lock);
            uint32_t header;
            memcpy(&header, record, sizeof(header));
            header = (header & 0xFFFFFF00u) | ((uint32_t)net_id & 0xFFu);
            memcpy(record, &header, sizeof(header));
            memcpy(record + 4, &apply_time, sizeof(apply_time));
            ((BotRemoteApplyFn)ADDR_BOT_REMOTE_APPLY)(vehicle, record, time);
            InterlockedIncrement(&g_bot_driven);
            (void)sample_clock;
            return;
        }
        /* Once seen, never hand an AI bot back to native packet state mid-race. */
        return;
    }
    if (stream_active) InterlockedIncrement(&g_bot_fallbacks); /* native until this net id is first stream-seen */
    ((BotRemoteUpdateFn)g_bot_remote_update_detour.trampoline)(controller, time);
}

/* Client visibility keeper (live 2026-10-11 19:46-19:51, own laptop on mobile).
 * Native staleness monitor 0x460DD0 (fastcall netcomm) hides a remote car
 * (SetDriverVisibility 0x4C7AA0 false, clears veh+0x48BC bits 2|4) when its
 * netcomm lag / 0x7D108C > 8 or the time error > 0.75, and shows it again only
 * when both recover. With N2 fix3 the native update of stream-driven bots no
 * longer runs, so after one 2.4 s stall 21 of 22 bots stayed hidden for the
 * rest of the race (laptop log: "visibility (client): hidden=21"). After the
 * native monitor, re-show every bot the stream drove within the last 3 s. */
static const uintptr_t ADDR_STALE_MONITOR = 0x00460DD0;
static const uintptr_t ADDR_SET_DRIVER_VISIBLE = 0x004C7AA0;
static const uintptr_t ADDR_SET_AI_CONTROL_FLAG = 0x004C7A70;
static const uint8_t StaleMonitor_expected[] = {0x83, 0xEC, 0x0C, 0x53, 0x56, 0x57};
typedef void(__attribute__((fastcall)) *StaleMonitorFn)(void *netcomm);
typedef int(__attribute__((thiscall)) *VehicleFlagFn)(void *vehicle, char on);
static F1CDetour g_stale_monitor_detour;

static void __attribute__((fastcall)) stale_monitor_hook(void *netcomm)
{
    ((StaleMonitorFn)g_stale_monitor_detour.trampoline)(netcomm);
    if (g_bot_stream != 2 || g_stopping || !coop_mode_is_selected() || network_state() != 3) return;
    double now = bot_stream_now();
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        uint8_t *vehicle = (uint8_t *)f1c_vehicle_slot(slot);
        if (!vehicle || *(int32_t *)(vehicle + 0x32C8) != 2) continue;
        int net_id = *(int32_t *)(vehicle + 0x4894);
        if (net_id < 0 || net_id >= BOT_STREAM_IDS || !InterlockedCompareExchange(&g_bot_stream_seen[net_id], 0, 0))
            continue;
        EnterCriticalSection(&g_bot_stream_lock);
        double arrival = g_bot_playback[net_id].last_arrival_clock;
        int has_pose = g_bot_playback[net_id].has_pose;
        LeaveCriticalSection(&g_bot_stream_lock);
        if (!has_pose || arrival <= 0.0 || now - arrival > 3.0) continue;
        uint32_t bits = *(uint32_t *)(vehicle + 0x48BC);
        if ((bits & 7) != 7) {
            ((VehicleFlagFn)ADDR_SET_AI_CONTROL_FLAG)(vehicle, 1);
            ((VehicleFlagFn)ADDR_SET_DRIVER_VISIBLE)(vehicle, 1);
            InterlockedIncrement(&g_stream_reshown);
        }
    }
}

#ifdef COOP_DEV_TOOLS  /* R11: developer-only (spike/link), absent from release builds */
/* ---- SPIKE (Development build): offline remote car over our own data ----
 * Owner 2026-10-09: prove a car in a single-player race can be driven by our
 * own stream instead of NetComm. INI [Spike] Enabled=1 (default 0). Offline only.
 * Ctrl+Shift+F9: record the player's native state record each frame (encoder
 * 0x4C71C0, the record the game writes for replay/MP). Ctrl+Shift+F10: add one
 * car in controller mode 2 (remote) and replay the lap into it through the
 * native remote apply 0x4C73F0 (detour on mode-2 update 0x4F03E0). */
static const uintptr_t ADDR_STATE_ENCODE = 0x004C71C0;
static const uintptr_t ADDR_REMOTE_UPDATE = 0x004F03E0;
static const uintptr_t ADDR_REMOTE_APPLY = 0x004C73F0;
static const uint8_t StateEncode_expected[] = {0x51, 0xDD, 0x05, 0xA0, 0xFE, 0x8A, 0x00};
static const uint8_t RemoteUpdate_expected[] = {0x81, 0xEC, 0xB0, 0x01, 0x00, 0x00};
typedef int(__attribute__((thiscall)) *StateEncodeFn)(void *vehicle, void *out);
typedef void(__attribute__((thiscall)) *RemoteUpdateFn)(void *controller, float time);
typedef void(__attribute__((thiscall)) *RemoteApplyFn)(void *vehicle, void *record, float time);
static F1CDetour g_state_encode_detour, g_remote_update_detour;
static const int SPIKE_MAX = 20000;          /* ~5.5 min at 60 fps */
static uint8_t (*g_spike_records)[64];
static int g_spike_count, g_spike_recording, g_spike_playing, g_spike_index, g_spike_keys_was;
static int32_t g_spike_slot = -1;
static double g_spike_play_start;

/* ---- LINK (Development build): live two-PC car link, offline sessions ----
 * Owner 2026-10-09 after the spike PASS. INI [Link] Enabled=1, Port (34510),
 * Peer (IP; empty = learn from first packet), DelayMs (100), Vehicle (remote car
 * .veh). Each PC sends its player's native state record (encoder 0x4C71C0) at
 * ~60 Hz over UDP; NUMPAD0 adds the peer car in controller mode 2 and drives
 * it through native apply 0x4C73F0 (detour 0x4F03E0). Render target = local
 * wall time - clock offset (min of arrival - send) - delay; nearest record. */
static int g_link_enabled, g_link_port = 34510, g_link_delay_ms = 100;
static char g_link_peer[64];
static SOCKET g_link_socket = INVALID_SOCKET;
static volatile LONG g_link_peer_ip, g_link_peer_port;
static HANDLE g_link_thread;
static CRITICAL_SECTION g_link_lock;
static const int LINK_RING = 512;
struct LinkSample { double sent; double arrived; uint8_t record[64]; };
static LinkSample g_link_ring[512];
static int g_link_head, g_link_count;
static double g_link_offset = 1e30;
static LONG g_link_rx, g_link_tx;
static uint8_t g_link_latest[64];
static volatile LONG g_link_have_latest;
static uint32_t g_link_seq;

static double link_now(void)
{
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

static DWORD WINAPI link_thread(LPVOID)
{
    char buffer[256];
    while (!g_stopping) {
        sockaddr_in from = {};
        int from_length = sizeof(from);
        int received = recvfrom(g_link_socket, buffer, sizeof(buffer), 0, (sockaddr *)&from, &from_length);
        if (received != 4 + 4 + 8 + 64 || memcmp(buffer, "CL1\0", 4) != 0) continue;
        double sent;
        memcpy(&sent, buffer + 8, 8);
        double arrived = link_now();
        InterlockedExchange(&g_link_peer_ip, (LONG)from.sin_addr.s_addr);
        InterlockedExchange(&g_link_peer_port, (LONG)from.sin_port);
        EnterCriticalSection(&g_link_lock);
        LinkSample *sample = &g_link_ring[g_link_head];
        sample->sent = sent;
        sample->arrived = arrived;
        memcpy(sample->record, buffer + 16, 64);
        g_link_head = (g_link_head + 1) % LINK_RING;
        if (g_link_count < LINK_RING) ++g_link_count;
        if (arrived - sent < g_link_offset) g_link_offset = arrived - sent;
        else g_link_offset += 0.00001;   /* slow drift allowance */
        LeaveCriticalSection(&g_link_lock);
        InterlockedIncrement(&g_link_rx);
    }
    return 0;
}

static void link_start(void)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
    InitializeCriticalSection(&g_link_lock);
    g_link_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in bind_address = {};
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons((u_short)g_link_port);
    if (g_link_socket == INVALID_SOCKET || bind(g_link_socket, (sockaddr *)&bind_address, sizeof(bind_address)) != 0) {
        log_line("link: UDP %d bind failed (%d)", g_link_port, WSAGetLastError());
        g_link_enabled = 0;
        return;
    }
    DWORD timeout = 250;
    setsockopt(g_link_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    if (g_link_peer[0]) {
        ULONG ip = inet_addr(g_link_peer);
        if (ip != INADDR_NONE) {
            InterlockedExchange(&g_link_peer_ip, (LONG)ip);
            InterlockedExchange(&g_link_peer_port, (LONG)htons((u_short)g_link_port));
        }
    }
    g_link_thread = CreateThread(NULL, 0, link_thread, NULL, 0, NULL);
    log_line("link: UDP %d up, peer=%s, delay=%d ms", g_link_port, g_link_peer[0] ? g_link_peer : "(learn)", g_link_delay_ms);
}

/* Game thread: send the latest player record at ~60 Hz. */
static void link_send_tick(void)
{
    static double last;
    double now = link_now();
    if (now - last < 1.0 / 60.0 || !g_link_have_latest || !g_link_peer_ip) return;
    last = now;
    char packet[4 + 4 + 8 + 64];
    memcpy(packet, "CL1\0", 4);
    ++g_link_seq;
    memcpy(packet + 4, &g_link_seq, 4);
    memcpy(packet + 8, &now, 8);
    memcpy(packet + 16, g_link_latest, 64);
    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = (ULONG)g_link_peer_ip;
    to.sin_port = (u_short)g_link_peer_port;
    sendto(g_link_socket, packet, sizeof(packet), 0, (sockaddr *)&to, sizeof(to));
    InterlockedIncrement(&g_link_tx);
}

/* Game thread: record for the peer car at the delayed render time. */
static int link_pick(uint8_t out[64])
{
    int found = 0;
    EnterCriticalSection(&g_link_lock);
    if (g_link_count > 0 && g_link_offset < 1e29) {
        double target = link_now() - g_link_offset - g_link_delay_ms / 1000.0;
        double best = 1e30;
        for (int i = 0; i < g_link_count; ++i) {
            const LinkSample *sample = &g_link_ring[(g_link_head - 1 - i + LINK_RING) % LINK_RING];
            double error = sample->sent - target;
            if (error < 0) error = -error;
            if (error < best) { best = error; memcpy(out, sample->record, 64); found = 1; }
            if (sample->sent < target - 1.0) break;
        }
    }
    LeaveCriticalSection(&g_link_lock);
    return found;
}

int __attribute__((thiscall)) state_encode_hook(void *vehicle, void *out)
{
    int result = ((StateEncodeFn)g_state_encode_detour.trampoline)(vehicle, out);
    if (g_spike_recording && vehicle == f1c_vehicle_slot(0) && g_spike_count < SPIKE_MAX)
        memcpy(g_spike_records[g_spike_count++], out, 64);
    if (g_link_enabled && vehicle == f1c_vehicle_slot(0)) {
        memcpy(g_link_latest, out, 64);
        InterlockedExchange(&g_link_have_latest, 1);
    }
    return result;
}

void __attribute__((thiscall)) remote_update_hook(void *controller, float time)
{
    void *vehicle = controller ? *(void **)controller : NULL;
    if (g_link_enabled && g_spike_slot >= 0 && vehicle == f1c_vehicle_slot(g_spike_slot)) {
        uint32_t record[108] = {};
        if (link_pick((uint8_t *)record)) {
            record[0] = (record[0] & 0xFFFFFF00u) | ((uint32_t)*(int32_t *)((uint8_t *)vehicle + 0x4894) & 0xFFu);
            memcpy(&record[1], &time, sizeof(float));
            ((RemoteApplyFn)ADDR_REMOTE_APPLY)(vehicle, record, time);
        }
        return;
    }
    if (g_spike_playing && g_spike_slot >= 0 && vehicle == f1c_vehicle_slot(g_spike_slot) && g_spike_count > 1) {
        const float t0 = *(const float *)(g_spike_records[0] + 4);
        const float span = *(const float *)(g_spike_records[g_spike_count - 1] + 4) - t0;
        double elapsed = *(volatile double *)ADDR_SESSION_TIME - g_spike_play_start;
        if (span > 0.5f) elapsed = fmod(elapsed, (double)span);
        if (g_spike_index >= g_spike_count ||
            *(const float *)(g_spike_records[g_spike_index] + 4) - t0 > elapsed) g_spike_index = 0;
        while (g_spike_index + 1 < g_spike_count &&
               *(const float *)(g_spike_records[g_spike_index + 1] + 4) - t0 <= elapsed) ++g_spike_index;
        uint32_t record[108] = {};
        memcpy(record, g_spike_records[g_spike_index], 64);
        record[0] = (record[0] & 0xFFFFFF00u) | ((uint32_t)*(int32_t *)((uint8_t *)vehicle + 0x4894) & 0xFFu);
        memcpy(&record[1], &time, sizeof(float));
        ((RemoteApplyFn)ADDR_REMOTE_APPLY)(vehicle, record, time);
        {
            static DWORD last;
            if (GetTickCount() - last > 2000) {
                last = GetTickCount();
                log_line("spike: apply index=%d/%d elapsed=%.2f span=%.2f rec_t=%.2f",
                         g_spike_index, g_spike_count, elapsed, (double)span,
                         (double)(*(const float *)(g_spike_records[g_spike_index] + 4) - t0));
            }
        }
        return;
    }
    ((RemoteUpdateFn)g_remote_update_detour.trampoline)(controller, time);
}

static void spike_file_path(char *path)
{
    GetTempPathA(MAX_PATH, path);
    lstrcatA(path, "coop_spike_lap.bin");
}

static void spike_save(void)
{
    char path[MAX_PATH];
    spike_file_path(path);
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(file, g_spike_records, (DWORD)g_spike_count * 64u, &written, NULL);
    CloseHandle(file);
    log_line("spike: saved %d records to %s", g_spike_count, path);
}

static void spike_load(void)
{
    char path[MAX_PATH];
    spike_file_path(path);
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD read_count = 0;
    ReadFile(file, g_spike_records, (DWORD)SPIKE_MAX * 64u, &read_count, NULL);
    CloseHandle(file);
    g_spike_count = (int)(read_count / 64u);
    log_line("spike: loaded %d records from %s", g_spike_count, path);
}

static void spike_notify(const char *text)
{
    ((MessageLogAddFn)ADDR_MESSAGE_LOG_ADD)((void *)ADDR_MESSAGE_LOG, text, 3, 16909060);
    f1c_pit_hud_post(text);
}

/* Game thread (time step). */
static void spike_tick(void)
{
    if (!g_spike_enabled || !g_spike_records) return;
    {
        static int32_t logged_state = -99;
        int32_t state = network_state();
        if (state != logged_state) {
            log_line("spike: network_state=%ld (%s)", (long)state, state == 0 ? "offline, keys active" : "not offline, keys off");
            logged_state = state;
        }
        if (state != 0) return;
    }
    if (g_link_enabled) {
        link_send_tick();
        static DWORD last_stats;
        if (GetTickCount() - last_stats > 5000) {
            last_stats = GetTickCount();
            log_line("link: tx=%ld rx=%ld offset=%.3f peer=%s car_slot=%ld", (long)g_link_tx, (long)g_link_rx,
                     g_link_offset < 1e29 ? g_link_offset : -1.0,
                     g_link_peer_ip ? inet_ntoa(*(in_addr *)&g_link_peer_ip) : "-", (long)g_spike_slot);
        }
    }
    KeyDownFn key_down = (KeyDownFn)ADDR_KEY_DOWN;
    void *keys = (void *)ADDR_KEY_STATE;
    /* Owner: one key, NUMPAD0 (DIK 0x52) cycles: start recording -> stop + play -> stop playback. */
    int now = (key_down(keys, 0x52) || key_down(keys, 0x32)) ? 1 : 0;  /* NUMPAD0 or M (laptop has no numpad) */
    int pressed = 0;
    if (now && !g_spike_keys_was) {
        if (g_spike_recording) pressed = 1 | 2;   /* stop recording, then start playback */
        else if (g_spike_playing) pressed = 2;    /* stop playback */
        else pressed = 1;                         /* start recording */
    }
    g_spike_keys_was = now;
    if (g_link_enabled) {
        if (pressed && g_spike_slot < 0) pressed = 4;
        else pressed = 0;
    }
    if (pressed & 4) {
        int32_t slot = first_inactive_slot();
        F1CVehicle *vehicle = slot >= 0 ? (F1CVehicle *)f1c_vehicle_slot(slot) : NULL;
        if (!vehicle) { spike_notify("LINK: NO FREE SLOT"); return; }
        int32_t previous_roster_index = f1c_vehicle_lookup_index_get();
        f1c_vehicle_lookup_index_set(-1);
        game::VehicleLookupByName(g_spike_vehicle_path);
        int32_t roster_index = f1c_vehicle_lookup_index_get();
        f1c_vehicle_lookup_index_set(previous_roster_index);
        if (roster_index < 0) { log_line("link: vehicle not found: %s", g_spike_vehicle_path); spike_notify("LINK: VEHICLE NOT FOUND"); return; }
        game::SessionStateTransition(f1c_vehicle_controller(vehicle), 2, 0);
        game::NationalityDriverInfoLoader(vehicle, roster_index);
        game::TeamIDSetter(vehicle, slot);
        f1c_vehicle_count_set(f1c_vehicle_count_get() + 1);
        game::BuildActiveVehiclePointerArray();
        game::VehgenScnFileLoader(vehicle, 1);
        load_ai_setup_files(vehicle);
        game::SceneRenderUpdate(vehicle, 1);
        g_spike_slot = slot;
        log_line("link: peer car slot=%ld roster=%ld name=%s", (long)slot, (long)roster_index, vehicle->DriverName);
        spike_notify("LINK: PEER CAR ADDED");
        return;
    }
    if (pressed & 1) {
        if (!g_spike_recording) {
            g_spike_count = 0;
            g_spike_recording = 1;
            log_line("spike: recording player state records");
            spike_notify("SPIKE: RECORDING - NUMPAD0 TO STOP + PLAY");
        } else {
            g_spike_recording = 0;
            log_line("spike: recording stopped, %d records", g_spike_count);
            spike_notify("SPIKE: RECORDING STOPPED");
            spike_save();
        }
    }
    if (pressed & 2) {
        if (g_spike_playing) {
            g_spike_playing = 0;
            log_line("spike: playback stopped (car stays in slot %ld)", (long)g_spike_slot);
            spike_notify("SPIKE: PLAYBACK STOPPED - NUMPAD0 TO RECORD");
            return;
        }
        if (g_spike_count < 2) spike_load();
        if (g_spike_count < 2) { log_line("spike: no recording"); spike_notify("SPIKE: NO RECORDING"); return; }
        if (g_spike_slot < 0) {
            int32_t slot = first_inactive_slot();
            F1CVehicle *vehicle = slot >= 0 ? (F1CVehicle *)f1c_vehicle_slot(slot) : NULL;
            if (!vehicle) { log_line("spike: no free slot"); return; }
            int32_t previous_roster_index = f1c_vehicle_lookup_index_get();
            f1c_vehicle_lookup_index_set(-1);
            game::VehicleLookupByName(g_spike_vehicle_path);
            int32_t roster_index = f1c_vehicle_lookup_index_get();
            f1c_vehicle_lookup_index_set(previous_roster_index);
            if (roster_index < 0) { log_line("spike: vehicle not found: %s", g_spike_vehicle_path); return; }
            game::SessionStateTransition(f1c_vehicle_controller(vehicle), 2, 0);
            game::NationalityDriverInfoLoader(vehicle, roster_index);
            game::TeamIDSetter(vehicle, slot);
            f1c_vehicle_count_set(f1c_vehicle_count_get() + 1);
            game::BuildActiveVehiclePointerArray();
            game::VehgenScnFileLoader(vehicle, 1);
            load_ai_setup_files(vehicle);
            game::SceneRenderUpdate(vehicle, 1);
            g_spike_slot = slot;
            log_line("spike: remote car slot=%ld roster=%ld name=%s", (long)slot, (long)roster_index, vehicle->DriverName);
        }
        g_spike_index = 0;
        g_spike_play_start = *(volatile double *)ADDR_SESSION_TIME;
        g_spike_playing = 1;
        log_line("spike: playback started (%d records)", g_spike_count);
        spike_notify("SPIKE: PLAYBACK - NUMPAD0 TO STOP");
    }
}

#endif  /* COOP_DEV_TOOLS */

/* ---- N0 diagnostic (client): native car-record arrivals per slot --------
 * Owner 2026-10-10: client sees AI wrong at race start, normal later. Hook the
 * native per-slot record intake 0x463D10 (thiscall netcomm, id, record) and log
 * record age per CO-OP bot during the first 90 s of the race. */
static const uintptr_t ADDR_RECORD_INTAKE = 0x00463D10;
static const uint8_t RecordIntake_expected[] = {0x8B, 0x44, 0x24, 0x04, 0x8B, 0xD0};
typedef char(__attribute__((thiscall)) *RecordIntakeFn)(void *netcomm, int32_t id, void *record);
static F1CDetour g_record_intake_detour;
static volatile DWORD g_record_tick[MAX_VEHICLES];
static volatile LONG g_record_count[MAX_VEHICLES];

char __attribute__((thiscall)) record_intake_hook(void *netcomm, int32_t id, void *record)
{
    if (id >= 0 && id < MAX_VEHICLES) {
        g_record_tick[id] = GetTickCount();
        InterlockedIncrement(&g_record_count[id]);
    }
    return ((RecordIntakeFn)g_record_intake_detour.trampoline)(netcomm, id, record);
}

/* ---- Net mode (0.9.5-dev2, owner 2026-10-11 "remove any caps, Modern option") ----
 * INI [Net] Mode=Modern|Vanilla (default Modern), SendHz=10..100 (default 60).
 * Modern, while CO-OP is selected:
 *  - 0x462FF0 per-car send interval (thiscall netcomm, slot, float override):
 *    returns 1/SendHz for every car. Vanilla scales the base up to x2.25 below
 *    25 mph, x4 for hidden cars, cap 0.5 s (N0 measured 13-16 Hz per bot).
 *  - 0x464E90 connection-type byte budget (stdcall type): returns 8 MB/s for
 *    every type. Vanilla 1487/2975/7200/28800/625000 B/s; the same value picks
 *    the per-client distribution period in 0x465460 (> 28800 = 1/64 s).
 * Vanilla mode, or CO-OP not selected: native code runs unchanged.
 * Not changed: collision threshold 0x7D108C (host session setting) and the
 * staleness monitor 0x460DD0. Audit: docs/NETCODE_AUDIT_2026-10-11.md. */
static const uintptr_t ADDR_SEND_INTERVAL = 0x00462FF0;
static const uint8_t SendInterval_expected[] = {0x8B, 0x54, 0x24, 0x04, 0xD9, 0x44, 0x24, 0x08};
typedef double(__attribute__((thiscall)) *SendIntervalFn)(void *netcomm, int32_t slot, float override_s);
static F1CDetour g_send_interval_detour;
static const uintptr_t ADDR_CONN_BUDGET = 0x00464E90;
static const uint8_t ConnBudget_expected[] = {0x8B, 0x44, 0x24, 0x04, 0x48};
typedef int32_t(__attribute__((stdcall)) *ConnBudgetFn)(int32_t type);
static F1CDetour g_conn_budget_detour;
static volatile LONG g_net_logged;
static const int32_t MODERN_BUDGET_BYTES = 8000000;

/* 0.9.7: CO-OP keeps its 0.9.6 scope. Plain MP (mode 5, no CO-OP) needs the NET1 verdict. */
static int net_modern_scope(void)
{
    if (g_stopping) return 0;
    if (coop_mode_is_selected()) return 1;
    return plain_mode_now() && g_plain_ok != 0;
}

/* Menu scope: the connection-type spinner and its label, CO-OP or plain MP (mode 5). */
static int net_menu_scope(void)
{
    return coop_mode_is_selected() || (g_plain_mp && *game_mode_ptr() == 5);
}

static int net_modern_now(void)
{
    return g_net_modern && net_modern_scope();
}

static void net_mode_log_once(void)
{
    if (InterlockedIncrement(&g_net_logged) == 1)
        log_line("net mode: Modern active (send %d Hz per car, native AI %d Hz when streamed, budget %ld B/s, distribution 64 Hz)",
                 g_net_send_hz, g_native_bot_hz, (long)MODERN_BUDGET_BYTES);
}

double __attribute__((thiscall)) send_interval_hook(void *netcomm, int32_t slot, float override_s)
{
    if (!g_stopping && coop_mode_is_selected() && g_bot_stream == 2 &&
        coop_host_now() && slot_is_owned_ai(slot))
        return 1.0 / (double)g_native_bot_hz;
    if (net_modern_now()) {
        net_mode_log_once();
        return 1.0 / (double)g_net_send_hz;
    }
    return ((SendIntervalFn)g_send_interval_detour.trampoline)(netcomm, slot, override_s);
}

/* 2026-10-11 crash fix (dev3): native 0x464E90 touches only EAX, and its caller
 * 0x465430 keeps using ECX after the call (`mov ecx,[ecx]; call 0x464E90; ...
 * lea ecx,[edx+ecx+0x8C]`). A plain C hook clobbered ECX -> AV at 0x464A6C on
 * join (host and client). The entry point is an asm wrapper that preserves
 * ECX and EDX around the C body. */
extern "C" int32_t __attribute__((stdcall, used)) conn_budget_body(int32_t type)
{
    if (net_modern_now()) return MODERN_BUDGET_BYTES;
    return ((ConnBudgetFn)g_conn_budget_detour.trampoline)(type);
}
extern "C" void conn_budget_hook(void);
__asm__(
    ".text\n"
    ".globl _conn_budget_hook\n"
    "_conn_budget_hook:\n"
    "    push %ecx\n"
    "    push %edx\n"
    "    pushl 12(%esp)\n"
    "    call _conn_budget_body@4\n"
    "    pop %edx\n"
    "    pop %ecx\n"
    "    ret $4\n");

/* ---- [Net] Mode in the game's connection-type spinner (0.9.5-dev4) --------
 * Owner 2026-10-11: set Modern/Vanilla from the in-game menu. Gizmo
 * APP_SpinBox_MultPlay_ConnectionType (factory ID 405, ctor 0x474B00) has its
 * own vtable 0x5A3F00 (pages MPRULESSETTINGS + MP options). Its value is the
 * profile connection type at 0x7D0E50 (0..4, LAN = 4). Native code indexes a
 * jump table with that value (0x464127), so it is never set to 5: in CO-OP the
 * spinner gets one extra position after LAN that only flips g_net_modern and
 * shows "Modern"; the native value stays 0..4. Slot 46 0x466DA0 = change
 * (dir 1 = +step, clamps), slot 47 0x4671F0 = label from value, ending in
 * 0x48B810(this, text) which copies the text. Evidence:
 * docs/NET_SETTINGS_UI_2026-10-11.md section 3. */
static const uintptr_t ADDR_CONN_SPINNER_VTABLE = 0x005A3F00;
static const uintptr_t ADDR_CONN_SPINNER_CHANGE = 0x00466DA0;   /* slot 46 */
static const uintptr_t ADDR_CONN_SPINNER_LABEL = 0x004671F0;    /* slot 47 */
static const uintptr_t ADDR_SPINNER_SET_LABEL = 0x0048B810;
static const int SLOT_CONN_LABEL = 47;
static const int32_t CONN_TYPE_LAN = 4;
static volatile LONG g_conn_menu_hooked;
typedef int32_t(__attribute__((thiscall)) *SpinnerSetLabelFn)(void *self, const char *text);

static uintptr_t *conn_spinner_slot(int slot)
{
    return (uintptr_t *)ADDR_CONN_SPINNER_VTABLE + slot;
}

int32_t __attribute__((thiscall)) conn_label_hook(void *self)
{
    int32_t result = ((SpinnerNoArgFn)ADDR_CONN_SPINNER_LABEL)(self);
    if (g_net_modern && net_menu_scope())   /* 0.9.7: CO-OP or plain MP (mode 5) */
        ((SpinnerSetLabelFn)ADDR_SPINNER_SET_LABEL)(self, "Modern");
    return result;
}

static void net_mode_set_from_menu(int modern)
{
    g_net_modern = modern;
    WritePrivateProfileStringA("Net", "Mode", modern ? "Modern" : "Vanilla", g_config_path);
    log_line("net mode: %s (in-game menu, saved to INI)", modern ? "Modern" : "Vanilla");
}

int32_t __attribute__((thiscall)) conn_change_hook(void *self, int32_t dir)
{
    if (net_menu_scope()) {
        int32_t value = *(volatile int32_t *)ADDR_CONN_TYPE;
        if (g_net_modern) {
            if (dir != 1) net_mode_set_from_menu(0);   /* back to the native value (LAN after a forward step) */
            conn_label_hook(self);
            return 0;
        }
        if (dir == 1 && value >= CONN_TYPE_LAN) {
            net_mode_set_from_menu(1);
            conn_label_hook(self);
            return 0;
        }
    }
    return ((SpinnerChangeFn)ADDR_CONN_SPINNER_CHANGE)(self, dir);
}

/* All-or-nothing like install_menu_hooks; failure only loses the menu entry. */
static int install_conn_menu_hooks(void)
{
    DWORD old_protection, ignored;
    if (*conn_spinner_slot(SLOT_CHANGE) != ADDR_CONN_SPINNER_CHANGE ||
        *conn_spinner_slot(SLOT_CONN_LABEL) != ADDR_CONN_SPINNER_LABEL) {
        log_line("COOP-E04 connection spinner slots hold %08lX/%08lX; net mode menu not installed",
                 (unsigned long)*conn_spinner_slot(SLOT_CHANGE), (unsigned long)*conn_spinner_slot(SLOT_CONN_LABEL));
        return 0;
    }
    if (!VirtualProtect(conn_spinner_slot(SLOT_CHANGE), sizeof(uintptr_t) * 2, PAGE_READWRITE, &old_protection)) {
        log_line("COOP-E04 connection spinner protect failed: %lu", (unsigned long)GetLastError());
        return 0;
    }
    InterlockedExchange((volatile LONG *)conn_spinner_slot(SLOT_CHANGE), (LONG)(uintptr_t)&conn_change_hook);
    InterlockedExchange((volatile LONG *)conn_spinner_slot(SLOT_CONN_LABEL), (LONG)(uintptr_t)&conn_label_hook);
    VirtualProtect(conn_spinner_slot(SLOT_CHANGE), sizeof(uintptr_t) * 2, old_protection, &ignored);
    InterlockedExchange(&g_conn_menu_hooked, 1);
    log_line("menu: net mode entry installed (vtable %08lX slots 46/47)", (unsigned long)ADDR_CONN_SPINNER_VTABLE);
    return 1;
}

static int remove_conn_menu_hooks(void)
{
    DWORD old_protection, ignored;
    int foreign = 0;
    if (!g_conn_menu_hooked) return 1;
    if (!VirtualProtect(conn_spinner_slot(SLOT_CHANGE), sizeof(uintptr_t) * 2, PAGE_READWRITE, &old_protection))
        return 0;
    if (*conn_spinner_slot(SLOT_CHANGE) == (uintptr_t)&conn_change_hook)
        InterlockedExchange((volatile LONG *)conn_spinner_slot(SLOT_CHANGE), (LONG)ADDR_CONN_SPINNER_CHANGE);
    else ++foreign;
    if (*conn_spinner_slot(SLOT_CONN_LABEL) == (uintptr_t)&conn_label_hook)
        InterlockedExchange((volatile LONG *)conn_spinner_slot(SLOT_CONN_LABEL), (LONG)ADDR_CONN_SPINNER_LABEL);
    else ++foreign;
    VirtualProtect(conn_spinner_slot(SLOT_CHANGE), sizeof(uintptr_t) * 2, old_protection, &ignored);
    InterlockedExchange(&g_conn_menu_hooked, 0);
    if (foreign) log_line("COOP-E03 %d net menu slot(s) were re-hooked by another module", foreign);
    return foreign == 0;
}

/* Game thread: client, race, first 90 s of session time. */
static void n0_race_start_diag(double session_time)
{
    static DWORD last;
    static LONG previous[MAX_VEHICLES];
    if (GetTickCount() - last < 2000) return;
    DWORD elapsed_ms = last ? GetTickCount() - last : 2000;
    last = GetTickCount();
    char line[1400];
    int length = 0, bots = 0, never = 0, stale = 0;
    DWORD worst = 0;
    for (int32_t index = 0; index < g_ai_count; ++index) {
        int32_t slot = g_ai_slots[index];
        F1CVehicle *vehicle = owned_ai(index);
        if (!vehicle || slot < 0 || slot >= MAX_VEHICLES) continue;
        ++bots;
        LONG count = g_record_count[slot];
        LONG rate = (LONG)((count - previous[slot]) * 1000 / (LONG)(elapsed_ms ? elapsed_ms : 1));
        previous[slot] = count;
        DWORD age = g_record_tick[slot] ? GetTickCount() - g_record_tick[slot] : 999999;
        if (!g_record_tick[slot]) ++never;
        else if (age > 1000) ++stale;
        if (g_record_tick[slot] && age > worst) worst = age;
        if (length < (int)sizeof(line) - 40)
            length += wsprintfA(line + length, " %ld:p%ld/%ldHz/%lums", (long)slot,
                                (long)vehicle->plannedPositionAtStart, (long)rate, (unsigned long)(age > 99999 ? 99999 : age));
    }
    line[length] = 0;
    log_line("net rx (client) t=%.1f phase=%ld bots=%d never=%d stale>1s=%d worst=%lums |%s",
             session_time, (long)*(volatile int32_t *)ADDR_RACE_PHASE, bots, never, stale,
             (unsigned long)worst, line);
}


/* Client diagnostic (owner 2026-10-10: Sarrazin invisible on Tester A's PC). The
 * native staleness monitor 0x460DD0 hides a remote car (clears veh+0x48BC bits
 * 2|4) when its record lag / 0x7D108C > 8 or the node time error > 0.75. Log
 * every hide/show of a car on the client with role and record age, plus a
 * hidden summary every 10 s, for the whole session. */
static void client_visibility_diag(void)
{
    static uint8_t was_hidden[MAX_VEHICLES];
    static DWORD last_summary;
    char line[600];
    int length = 0, hidden = 0;
    DWORD now = GetTickCount();
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle) continue;
        int32_t role = vehicle->WearTickGate__was__slot_live_marker_;
        uint32_t bits = *(uint32_t *)((uint8_t *)vehicle + 0x48BC);
        int is_hidden = role == 2 && !(bits & 6);
        DWORD age = g_record_tick[slot] ? now - g_record_tick[slot] : 999999;
        if (role != 2) { was_hidden[slot] = 0; continue; }
        if (is_hidden != was_hidden[slot])
            log_line("visibility (client): slot %ld %s %s (record age %lums, records %ld, bits %lX)",
                     (long)slot, vehicle->DriverName, is_hidden ? "HIDDEN" : "visible again",
                     (unsigned long)age, (long)g_record_count[slot], (unsigned long)bits);
        was_hidden[slot] = (uint8_t)is_hidden;
        if (is_hidden) {
            ++hidden;
            if (length < (int)sizeof(line) - 48)
                length += wsprintfA(line + length, " %ld:%lums", (long)slot, (unsigned long)(age > 99999 ? 99999 : age));
        }
    }
    line[length] = 0;
    if (now - last_summary >= 10000) {
        last_summary = now;
        if (hidden)
            log_line("visibility (client): hidden=%d expected_interval=%.3f |%s", hidden,
                     (double)*(volatile float *)0x007D108C, line);
    }
}


/* ---- Native network message copy guard (2026-10-11 crash fix) -------------
 * 0x4B3A90 (thiscall dst=ECX, src=[esp+4], ret 4) copies ((hdr>>8)&0x3FF)+8
 * bytes (up to 1031) into a 432-byte record (receive queue 0x7D2E50 slots,
 * relay heap blocks 0x465300, record store 0x500010), then copies dword +0x1AC.
 * A bad length field overran the record (I21 client crash at 0x4B3AB4 rep
 * movsd). Replacement: identical code and register results (EAX=dst,
 * ECX=[src+0x1AC], EDX=src, EBX/ESI/EDI kept) with the length capped at 432.
 * Never calls the original; counts each clamp. Audit: findings
 * native-net-message-copy-4b3a90-variable-length-20261011. */
static const uintptr_t ADDR_NET_MESSAGE_COPY = 0x004B3A90;
static const uint8_t NetMessageCopy_expected[] = {0x8B, 0x54, 0x24, 0x04, 0x8B, 0xC1, 0x8B, 0x0A};
static F1CDetour g_net_message_copy_detour;
extern "C" { volatile LONG g_net_message_clamps; }
extern "C" void net_message_copy_hook(void);
__asm__(
    ".text\n"
    ".globl _net_message_copy_hook\n"
    "_net_message_copy_hook:\n"
    "    movl 4(%esp), %edx\n"
    "    movl %ecx, %eax\n"
    "    movl (%edx), %ecx\n"
    "    shrl $8, %ecx\n"
    "    andl $0x3FF, %ecx\n"
    "    addl $8, %ecx\n"
    "    cmpl $0x1B0, %ecx\n"
    "    jbe 1f\n"
    "    movl $0x1B0, %ecx\n"
    "    lock incl _g_net_message_clamps\n"
    "1:\n"
    "    pushl %ebx\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    movl %ecx, %ebx\n"
    "    shrl $2, %ecx\n"
    "    movl %edx, %esi\n"
    "    movl %eax, %edi\n"
    "    rep movsl\n"
    "    movl %ebx, %ecx\n"
    "    andl $3, %ecx\n"
    "    rep movsb\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    popl %ebx\n"
    "    movl 0x1AC(%edx), %ecx\n"
    "    movl %ecx, 0x1AC(%eax)\n"
    "    ret $4\n");

/* Client, game thread, once a second: a car with no best lap here gets the
 * host's (live: mid-session joiner saw the order but no qualifying times; lap
 * events before the join never reach it). Only fills missing times. */
static void apply_host_best_laps(void)
{
    static DWORD last;
    static LONG logged;
    static int32_t session = -100;
    static LONG gen_at_change;
    int32_t st = *(volatile int32_t *)ADDR_SESSION_TYPE;
    if (st != session) {               /* new session: only a manifest received after it counts */
        session = st;
        gen_at_change = g_host_best_gen;
    }
    if (GetTickCount() - last < 250) return;
    last = GetTickCount();
    if (g_host_best_gen - gen_at_change < 2) return;   /* two fresh manifests (2 s poll) */
    int32_t filled = 0, ms[MAX_VEHICLES];
    EnterCriticalSection(&g_manifest_lock);
    for (int32_t i = 0; i < MAX_VEHICLES; ++i) ms[i] = g_host_best_ms[i];
    LeaveCriticalSection(&g_manifest_lock);
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        if (ms[slot] <= 0 || ms[slot] >= 3600000) continue;
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1 || vehicle->bestLap > 0.0f) continue;
        vehicle->bestLap = ms[slot] / 1000.0f;
        ++filled;
    }
    if (filled && InterlockedIncrement(&logged) <= 20)
        log_line("best laps from host: filled %ld car(s) without a time", (long)filled);
}

static void report_crash_guards(void)
{
    static LONG seen_clamps, seen_retargets;
    static DWORD last;
    if (GetTickCount() - last < 10000) return;
    last = GetTickCount();
    LONG clamps = g_net_message_clamps, retargets = g_view_retargets;
    if (clamps != seen_clamps) {
        log_line("COOP-W20 net message length clamped to 432 bytes: total=%ld (+%ld)",
                 (long)clamps, (long)(clamps - seen_clamps));
        seen_clamps = clamps;
    }
    if (retargets != seen_retargets) {
        log_line("view retarget before bot unload: total=%ld", (long)retargets);
        seen_retargets = retargets;
    }
}

int8_t __attribute__((thiscall)) time_step_hook(void *self)
{
    int host = !g_stopping && coop_host_now();
    double before = *(volatile double *)ADDR_SESSION_TIME;
    int32_t session_type = *(volatile int32_t *)ADDR_SESSION_TYPE;

    InterlockedIncrement(&g_active_calls);
    post_restart_notice();
    if (!g_stopping && coop_mode_is_selected() && network_state() != 0) {
        static DWORD last_box_check;
        DWORD now = GetTickCount();
        if (now - last_box_check >= 1000) { last_box_check = now; hooks::dedupe_garage_boxes(); }
    }
    if (host) {
        KeyDownFn key_down = (KeyDownFn)ADDR_KEY_DOWN;
        int down = key_down((void *)ADDR_KEY_STATE, KEY_CONTROL) &&
                   key_down((void *)ADDR_KEY_STATE, KEY_T);
        if (down && !g_warp_key_was_down && !g_warp_enabled)
            log_line("time warp: Ctrl+T ignored (HostTimeWarp=0 in " MOD_ID ".ini)");
        DWORD monitor_age = monitor_age_ms();
        int in_garage = monitor_age < 1500;
        if (down && !g_warp_key_was_down && g_warp_enabled) {
            if (!g_warp && !in_garage) {
                log_line("time warp: Ctrl+T ignored (host is driving; use it from the garage monitor)");
            } else {
                InterlockedExchange(&g_warp, !g_warp);
                g_warp_session_type = session_type;
                log_line("time warp: %s x%.0f (host Ctrl+T, session_type=%ld; clients not synced)",
                         g_warp ? "ON" : "OFF", g_warp_scale, (long)session_type);
                f1c_pit_hud_post(g_warp ? "TIME WARP ON" : "TIME WARP OFF");
            }
        }
        g_warp_key_was_down = down;
        if (g_warp && !in_garage) {
            InterlockedExchange(&g_warp, 0);
            log_line("time warp: OFF (host left the garage monitor)");
            f1c_pit_hud_post("TIME WARP OFF");
        }
    }
    if (g_warp && (!host || session_type != g_warp_session_type)) {
        InterlockedExchange(&g_warp, 0);
        log_line("time warp: OFF (session change or not CO-OP host)");
    }

    report_crash_guards();
    if (!g_stopping && coop_mode_is_selected() && network_state() == 3) {
        int32_t st_now = *(volatile int32_t *)ADDR_SESSION_TYPE;
        if (st_now >= 1 && st_now <= 6) apply_host_best_laps();
    }
    if (host) refresh_remote_car_tracking();
    step_slow_laps(host, session_type);
    int8_t result = ((TimeStepFn)g_time_step_detour.trampoline)(self);

    int hold = 0;  /* race-start hold removed 2026-10-09 (owner): 60 s start timer instead */
    apply_start_timer(!g_stopping && coop_mode_is_selected());
    apply_leader_slot_limit(!g_stopping && coop_mode_is_selected());
#ifdef COOP_DEV_TOOLS
    if (!g_stopping) spike_tick();
#endif
    {   /* CO-OP host: 5 min start timer, 60 s once the host leaves the garage monitor (DRIVE). */
        static int drive_done;
        int32_t phase = *(volatile int32_t *)ADDR_RACE_PHASE;
        double now = *(volatile double *)ADDR_SESSION_TIME;
        if (session_type != 7 || !coop_mode_is_selected()) {
            drive_done = 0;
            g_start_send_time = START_DEFAULT_SECONDS - 2.5;
            g_start_hud_time = (float)START_DEFAULT_SECONDS;
            InterlockedExchange(&g_host_start_cs, 30000);
        } else if (!host && network_state() == 3 && phase <= 4) {
            /* 0.9.1: client countdown follows the host's DRIVE (manifest START). */
            float host_start = (float)g_client_start_cs / 100.0f;
            if (host_start < (float)START_DEFAULT_SECONDS && host_start != g_start_hud_time) {
                g_start_hud_time = host_start;
                log_line("start timer (client): countdown set to host lights at %.2f (session_time=%.2f)",
                         (double)host_start, now);
            }
        } else if (host && !drive_done && phase <= 4 && now > 2.0) {
            DWORD monitor_age = monitor_age_ms();
            if (monitor_age >= 1500) {
                double send = now + START_AFTER_DRIVE_SECONDS - 2.5;
                if (send < g_start_send_time) {
                    g_start_send_time = send;
                    g_start_hud_time = (float)(now + START_AFTER_DRIVE_SECONDS);
                }
                drive_done = 1;
                InterlockedExchange(&g_host_start_cs, (LONG)(g_start_hud_time * 100.0f));
                log_line("start timer: host DRIVE at session_time=%.2f -> lights at %.2f", now,
                         (double)g_start_hud_time);
            }
        }
    }
    if (!g_stopping && coop_mode_is_selected() && network_state() == 3 && !g_race_grid_rewrite &&
        session_type == 7 && *(volatile int32_t *)ADDR_RACE_PHASE <= 4)
        enforce_host_grid_client();
    if (!g_stopping && coop_mode_is_selected() && network_state() == 3 && session_type == 7 &&
        *(volatile double *)ADDR_SESSION_TIME < 90.0)
        n0_race_start_diag(*(volatile double *)ADDR_SESSION_TIME);
    if (!g_stopping && coop_mode_is_selected() && network_state() >= 1) {
        /* frame-time diagnostic (owner 2026-10-10: client low FPS): every 10 s */
        static DWORD window_start, frames, worst_ms, last_frame;
        DWORD now = GetTickCount();
        if (last_frame && now - last_frame > worst_ms) worst_ms = now - last_frame;
        last_frame = now;
        ++frames;
        if (!window_start) window_start = now;
        if (now - window_start >= 10000) {
            log_line("frames: avg %.1f fps, worst frame %lums, cars=%ld, session_type=%ld",
                     frames * 1000.0 / (double)(now - window_start), (unsigned long)worst_ms,
                     (long)f1c_vehicle_count_get(), (long)session_type);
            window_start = now; frames = 0; worst_ms = 0;
        }
    }
    if (!g_stopping && coop_mode_is_selected() && network_state() == 3)
        client_visibility_diag();
    if (host && session_type == 7) {
        static DWORD last_diag;
        DWORD now = GetTickCount();
        if (now - last_diag > 3000) {
            int32_t slot = local_human_slot();
            F1CVehicle *me = slot >= 0 ? (F1CVehicle *)f1c_vehicle_slot(slot) : NULL;
            log_line("race diag: phase=%ld slot=%ld ai_control=%ld controller=%ld session_time=%.2f hold=%d "
                     "c7CC4F4=%ld b7CE254=%d b7C56AC=%d d8AB6E4=%ld d7CE258=%ld d5CC9E8=%ld realtime_wnd=%08lX monitor_age_ms=%lu",
                     (long)*(volatile int32_t *)ADDR_RACE_PHASE, (long)slot,
                     me ? (long)me->AIControlFlag : -9L,
                     me ? (long)me->WearTickGate__was__slot_live_marker_ : -9L, before, hold,
                     (long)*(volatile int32_t *)0x007CC4F4, (int)*(volatile uint8_t *)0x007CE254,
                     (int)*(volatile uint8_t *)0x007C56AC, (long)*(volatile int32_t *)0x008AB6E4,
                     (long)*(volatile int32_t *)0x007CE258, (long)*(volatile int32_t *)0x005CC9E8,
                     (unsigned long)*(volatile uintptr_t *)ADDR_REALTIME_WINDOW,
                     (unsigned long)(GetTickCount() - (DWORD)g_monitor_tick));
            last_diag = now;
        }
    }
    {   /* opt-in diagnostics (0.9.0): transitions always, state heartbeat with [Log] Debug=1 */
        static int32_t last_type = -100, last_phase = -100, last_net = -100;
        static DWORD last_beat;
        int32_t phase = *(volatile int32_t *)ADDR_RACE_PHASE;
        int32_t net = network_state();
        if (net != last_net && last_net > 0 && net == 0) coopu_request("session_end");
        if (session_type != last_type || phase != last_phase || net != last_net) {
            log_line("session change: session_type=%ld phase=%ld net_state=%ld coop=%d host=%d session_time=%.2f",
                     (long)session_type, (long)phase, (long)net, (int)coop_mode_is_selected(), host,
                     *(volatile double *)ADDR_SESSION_TIME);
            last_type = session_type; last_phase = phase; last_net = net;
        }
        DWORD tick = GetTickCount();
        if (g_log_debug && coop_mode_is_selected() && tick - last_beat >= 2000) {
            int32_t slot = local_human_slot();
            F1CVehicle *me = slot >= 0 ? (F1CVehicle *)f1c_vehicle_slot(slot) : NULL;
            log_line("debug state: session_type=%ld phase=%ld net_state=%ld host=%d session_time=%.2f "
                     "humans=%ld bots=%ld manifest_ready=%ld manifest_gen=%ld refused=%ld my_slot=%ld my_lap=%ld "
                     "my_pos=%ld start_send=%.2f start_hud=%.2f monitor_age_ms=%lu active_calls=%ld",
                     (long)session_type, (long)phase, (long)net, host, *(volatile double *)ADDR_SESSION_TIME,
                     (long)g_last_human_count, (long)g_ai_count, (long)g_manifest_ready,
                     (long)g_manifest_generation, (long)g_manifest_refused, (long)slot,
                     me ? (long)me->lapNumber : -1L, me ? (long)me->position : -1L,
                     (double)g_start_send_time, (double)g_start_hud_time,
                     (unsigned long)(g_monitor_tick ? GetTickCount() - (DWORD)g_monitor_tick : 0u),
                     (long)g_active_calls);
            last_beat = tick;
        }
    }
    if (hold != g_start_hold_logged) {
        if (session_type == 7)
            log_line("race start: %s (phase=%ld session_time=%.2f)",
                     hold ? "HELD" : "running",
                     (long)*(volatile int32_t *)ADDR_RACE_PHASE, before);
        g_start_hold_logged = hold;
    }
    InterlockedDecrement(&g_active_calls);
    return result;
}

/* ---- CO-OP driver-name callouts ------------------------------------------
 * FACT (decompile 0x4122C0): the commentary name resolver uses the TEAM phrase
 * ("the Ferrari driver") when `!dword_7D0398 && (entry flag +12 || game mode == 5)`.
 * Mode 5 = multiplayer, so every car gets the team phrase online. In CO-OP the
 * resolver runs with mode 3 (single-player race) for bot entries only; entries
 * driven by a human keep mode 5 (team phrase, as single player does for the
 * player). Entry index = per-car commentary id veh+0x489C. */
static const uintptr_t ADDR_NAME_RESOLVER = 0x004122C0;
static const uintptr_t VEH_COMMENTARY_ID = 0x489C;
static const uint8_t NameResolver_expected[] = {0x83, 0xEC, 0x1C, 0x8B, 0x44, 0x24, 0x28};
static F1CDetour g_name_resolver_detour;
static volatile LONG g_name_log_count;

typedef int32_t(__attribute__((stdcall)) *NameResolverFn)(int32_t out, int32_t kind, int32_t entry,
                                                         int8_t a4, int8_t a5);

static int entry_is_human_car(int32_t entry)
{
    for (int32_t slot = 0; slot < MAX_VEHICLES; ++slot) {
        F1CVehicle *vehicle = (F1CVehicle *)f1c_vehicle_slot(slot);
        if (!vehicle || vehicle->WearTickGate__was__slot_live_marker_ == -1) continue;
        if (*(int32_t *)((uint8_t *)vehicle + VEH_COMMENTARY_ID) == entry &&
            hooks::slot_is_human(slot)) return 1;
    }
    return 0;
}

int32_t __attribute__((stdcall)) name_resolver_hook(int32_t out, int32_t kind, int32_t entry,
                                                    int8_t a4, int8_t a5)
{
    NameResolverFn original = (NameResolverFn)g_name_resolver_detour.trampoline;
    volatile int32_t *mode = game_mode_ptr();
    if (g_stopping || *mode != COOP_MODE_MULTIPLAYER || !g_coop || entry < 0 ||
        entry_is_human_car(entry))
        return original(out, kind, entry, a4, a5);
    *mode = COOP_MODE_RACE;
    int32_t result = original(out, kind, entry, a4, a5);
    *mode = COOP_MODE_MULTIPLAYER;
    if (InterlockedIncrement(&g_name_log_count) <= 40)
        log_line("callout: bot entry=%ld kind=%ld resolved by name (result=%ld)",
                 (long)entry, (long)kind, (long)result);
    return result;
}

static int install_hooks(void)
{
    F1CDetourResult result;
    f1c_hook_registry_init(&g_registry);
    result = f1c_hook_registry_install(&g_registry, &g_name_resolver_detour,
        (void *)ADDR_NAME_RESOLVER, (void *)&name_resolver_hook,
        NameResolver_expected, sizeof(NameResolver_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_monitor_detour,
        (void *)ADDR_MONITOR_FRAME, (void *)&monitor_frame_hook,
        MonitorFrame_expected, sizeof(MonitorFrame_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_time_step_detour,
        (void *)ADDR_TIME_STEP, (void *)&time_step_hook,
        TimeStep_expected, sizeof(TimeStep_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_time_scale_detour,
        (void *)ADDR_TIME_SCALE_SET, (void *)&time_scale_set_hook,
        TimeScaleSet_expected, sizeof(TimeScaleSet_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &hooks::g_garage_pick_detour,
        (void *)hooks::ADDR_GARAGE_PICK, (void *)&hooks::garage_pick_hook,
        hooks::GaragePick_expected, sizeof(hooks::GaragePick_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_fastest_holder_detour,
        (void *)ADDR_FASTEST_HOLDER_UPDATE, (void *)&fastest_holder_hook,
        FastestHolder_expected, sizeof(FastestHolder_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_pit_nearest_detour,
        (void *)ADDR_PIT_NEAREST, (void *)&pit_nearest_hook,
        PitNearest_expected, sizeof(PitNearest_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_pit_exit_detour,
        (void *)ADDR_PIT_EXIT_GATE, (void *)&pit_exit_gate_hook,
        PitExitGate_expected, sizeof(PitExitGate_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_next_session_detour,
        (void *)ADDR_NEXT_SESSION, (void *)&next_session_hook,
        NextSession_expected, sizeof(NextSession_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = hooks::install_ProcessDriverEntries(&g_registry);
    if (result != F1C_DETOUR_OK) return (int)result;
    result = hooks::install_MultiplayerQualificationGridAssignment(&g_registry);
    if (result != F1C_DETOUR_OK) return (int)result;
    result = hooks::install_NetCommPlayerSlotSync(&g_registry);
    if (result != F1C_DETOUR_OK) return (int)result;
    result = hooks::install_SceneRenderUpdate(&g_registry);
    if (result != F1C_DETOUR_OK) return (int)result;
    result = hooks::install_SuspensionForceComputationWithPerCornerVirtualDevices(&g_registry);
    if (result != F1C_DETOUR_OK) return (int)result;
    result = hooks::install_DriverNameIDLookup(&g_registry);
    if (result != F1C_DETOUR_OK) return (int)result;
    result = hooks::install_MultiplayerVehiclePlacement(&g_registry);
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_record_intake_detour,
        (void *)ADDR_RECORD_INTAKE, (void *)&record_intake_hook,
        RecordIntake_expected, sizeof(RecordIntake_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_send_interval_detour,
        (void *)ADDR_SEND_INTERVAL, (void *)&send_interval_hook,
        SendInterval_expected, sizeof(SendInterval_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_conn_budget_detour,
        (void *)ADDR_CONN_BUDGET, (void *)&conn_budget_hook,
        ConnBudget_expected, sizeof(ConnBudget_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_net_message_copy_detour,
        (void *)ADDR_NET_MESSAGE_COPY, (void *)&net_message_copy_hook,
        NetMessageCopy_expected, sizeof(NetMessageCopy_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
#ifndef COOP_DEV_TOOLS
    result = f1c_hook_registry_install(&g_registry, &g_bot_state_encode_detour,
        (void *)ADDR_BOT_STATE_ENCODE, (void *)&bot_state_encode_hook,
        BotStateEncode_expected, sizeof(BotStateEncode_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_bot_remote_update_detour,
        (void *)ADDR_BOT_REMOTE_UPDATE, (void *)&bot_remote_update_hook,
        BotRemoteUpdate_expected, sizeof(BotRemoteUpdate_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
    result = f1c_hook_registry_install(&g_registry, &g_stale_monitor_detour,
        (void *)ADDR_STALE_MONITOR, (void *)&stale_monitor_hook,
        StaleMonitor_expected, sizeof(StaleMonitor_expected));
    if (result != F1C_DETOUR_OK) return (int)result;
#endif
#ifdef COOP_DEV_TOOLS
    if (g_spike_enabled) {
        g_spike_records = (uint8_t (*)[64])VirtualAlloc(NULL, (SIZE_T)SPIKE_MAX * 64u, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        log_line("spike: %s (offline only; NUMPAD0: record -> play -> stop)",
                 g_spike_records ? "ENABLED" : "alloc failed");
        if (g_link_enabled) link_start();
        result = f1c_hook_registry_install(&g_registry, &g_state_encode_detour,
            (void *)ADDR_STATE_ENCODE, (void *)&state_encode_hook,
            StateEncode_expected, sizeof(StateEncode_expected));
        if (result != F1C_DETOUR_OK) return (int)result;
        result = f1c_hook_registry_install(&g_registry, &g_remote_update_detour,
            (void *)ADDR_REMOTE_UPDATE, (void *)&remote_update_hook,
            RemoteUpdate_expected, sizeof(RemoteUpdate_expected));
        if (result != F1C_DETOUR_OK) return (int)result;
    }
#endif
    /* CommentaryEventQueue diagnostic logging is disabled: it is only needed
     * for the parked callout investigation, and its first 5-byte hook length
     * split a 4-byte instruction and crashed the game on 2026-09-20. The
     * manifest now carries the corrected 8-byte length for a future re-enable.
     * CommentaryDriverEntryInit slot-filling is likewise disabled (it crashed
     * the registration rebuild). */
    return (int)result;
}

static DWORD WINAPI mod_worker(void *)
{
    F1CTargetResult target;
    const char *target_variant = "unrecognized";
    int hook_result;
    size_t index;
    unsigned detach_failures = 0;
    DWORD wait_count = 0;

    module_sibling_path(g_log_path, sizeof(g_log_path), MOD_ID "_log.txt");
    module_sibling_path(g_config_path, sizeof(g_config_path), MOD_ID ".ini");
    {
        char exe[MAX_PATH] = "";
        OSVERSIONINFOA os;
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        memset(&os, 0, sizeof(os));
        os.dwOSVersionInfoSize = sizeof(os);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        GetVersionExA(&os);
#pragma GCC diagnostic pop
        log_line("==== %s %s start (pid=%lu, Windows %lu.%lu.%lu, exe=%s, loader=Lightloader host)",
                 MOD_NAME, MOD_VERSION, (unsigned long)GetCurrentProcessId(),
                 (unsigned long)os.dwMajorVersion, (unsigned long)os.dwMinorVersion,
                 (unsigned long)os.dwBuildNumber, exe);
    }
    coop_migrate_legacy_ini();
    coop_upgrade_ini_layout();
    {
        char state[16] = "";
        GetPrivateProfileStringA("Menu", "LastSelected", "", state, sizeof(state), g_config_path);
        g_menu_saved_state = strcmp(state, "coop") == 0 ? COOP_MENU_COOP
                           : strcmp(state, "multiplayer") == 0 ? COOP_MENU_MULTIPLAYER
                           : strcmp(state, "other") == 0 ? COOP_MENU_OTHER : COOP_MENU_UNKNOWN;
        if (g_menu_saved_state == COOP_MENU_COOP) {
            InterlockedExchange(&g_menu_restore_frames, 600);
            log_line("menu: saved choice is CO-OP; restore armed");
        }
    }
    if (!GetPrivateProfileIntA("Mod", "Enabled", 1, g_config_path)) {
        log_line("%s is disabled in its INI", MOD_NAME);
        FreeLibraryAndExitThread(g_module, 0);
    }
    /* EnableDistributedAI no longer gates arming: bots arm only when the
     * player picked CO-OP in the main menu (coop_mode_is_selected). */
    g_expected_humans = coop_ini_int(g_config_path, "Prototype", "ExpectedPlayers", "ExpectedHumans", 2);
    g_min_humans = coop_ini_int(g_config_path, "Prototype", "PlayersBeforeBots", "MinHumans", 1);
    if (g_min_humans < 1) g_min_humans = 1;
    if (g_min_humans > g_expected_humans) g_min_humans = g_expected_humans;
    g_requested_ai = GetPrivateProfileIntA(
        "Prototype", "AIToAdd", 22, g_config_path);
    load_grid_configuration();
    {
        char bot_source[32];
        GetPrivateProfileStringA("CoOp", "BotSource", "season", bot_source, sizeof(bot_source),
                                 g_config_path);
        g_use_season_bots = lstrcmpiA(bot_source, "ini") != 0;
        g_warp_enabled = GetPrivateProfileIntA("CoOp", "HostTimeWarp", 1, g_config_path) != 0;
        {
            int scale = coop_ini_int(g_config_path, "CoOp", "HostTimeWarpSpeed", "HostTimeWarpScale", 10);
            if (scale < 2) scale = 2;
            if (scale > 30) scale = 30;
            g_warp_scale = (double)scale;
        }
        g_race_grid_rewrite = GetPrivateProfileIntA("CoOp", "RaceGridRewrite", 0, g_config_path) != 0;
        g_pit_exit_fix = GetPrivateProfileIntA("CoOp", "PitExitFix", 1, g_config_path) != 0;
        g_remote_tracking_fix = GetPrivateProfileIntA("CoOp", "RemoteCarTracking", 1, g_config_path) != 0;
        g_fastest_lap_fix = GetPrivateProfileIntA("CoOp", "FastestLapFix", 1, g_config_path) != 0;
        g_quali_all_timed = coop_ini_int(g_config_path, "CoOp", "EveryBotSetsQualiTime", "QualiAllTimed", 1) != 0;
        g_refill_mid_session = coop_ini_int(g_config_path, "CoOp", "BotTakesOverAtOnce", "RefillBotMidSession", 0) != 0;
        g_slow_laps = coop_ini_int(g_config_path, "CoOp", "BotSlowOutAndInLaps", "SlowLaps", 0) != 0;
        g_blue_flags = GetPrivateProfileIntA("CoOp", "BlueFlags", 0, g_config_path) != 0;
        {
            int pct = coop_ini_int(g_config_path, "CoOp", "SlowLapThrottle", "SlowLapsThrottle", 75);
            if (pct < 50) pct = 50;
            if (pct > 100) pct = 100;
            g_slow_laps_factor = pct / 100.0f;
        }
        log_line("config: PitExitFix=%d RemoteCarTracking=%d FastestLapFix=%d QualiAllTimed=%d SlowLaps=%d SlowLapsThrottle=%.0f BlueFlags=%d", g_pit_exit_fix, g_remote_tracking_fix, g_fastest_lap_fix, g_quali_all_timed, g_slow_laps, g_slow_laps_factor * 100.0f, g_blue_flags);
        {
            char net_mode[16] = "";
            GetPrivateProfileStringA("Net", "Mode", "Vanilla", net_mode, sizeof(net_mode), g_config_path);
            g_net_modern = NET_MODERN_AVAILABLE && lstrcmpiA(net_mode, "Vanilla") != 0;
            if (!NET_MODERN_AVAILABLE && lstrcmpiA(net_mode, "Vanilla") != 0)
                log_line("config: Net Mode=%s in INI ignored: Modern disabled in 1.0.2", net_mode);
            int hz = GetPrivateProfileIntA("Net", "SendHz", 60, g_config_path);
            g_net_send_hz = hz < 10 ? 10 : (hz > 100 ? 100 : hz);
            g_plain_mp = GetPrivateProfileIntA("Net", "PlainMP", 1, g_config_path) != 0;
            /* 1.0.1 (owner 2026-10-14): player removal disabled. A blocked UDP ManifestPort made the host
             * boot modded clients (docs/BUG_WRONG_SEASON_CONTENT_2026-10-14.md, issue 3). The INI keys
             * are read for the log only; both flags stay 0, so require_mod_tick never boots or warns. */
            int ini_require_mod = GetPrivateProfileIntA("Net", "RequireMod", 1, g_config_path) != 0;
            int ini_require_mod_coop = GetPrivateProfileIntA("Net", "RequireModCoop", 1, g_config_path) != 0;
            g_require_mod = 0;
            g_require_mod_coop = 0;
            log_line("config: Net RequireMod=%d RequireModCoop=%d (INI %d/%d ignored: player removal disabled in 1.0.1)",
                     g_require_mod, g_require_mod_coop, ini_require_mod, ini_require_mod_coop);
            g_kick_message = GetPrivateProfileIntA("Net", "KickMessage", 1, g_config_path) != 0;
            log_line("config: Net KickMessage=%d", g_kick_message);
            log_line("config: Net Mode=%s SendHz=%d PlainMP=%d", g_net_modern ? "Modern" : "Vanilla", g_net_send_hz, g_plain_mp);
            g_bot_stream = GetPrivateProfileIntA("Net", "BotStream", 0, g_config_path);
            if (g_bot_stream < 0 || g_bot_stream > 2) g_bot_stream = 0;
            g_bot_stream_port = GetPrivateProfileIntA("Net", "BotStreamPort", 34501, g_config_path);
            if (g_bot_stream_port < 1024 || g_bot_stream_port > 65535) g_bot_stream_port = 34501;
            int stream_hz = GetPrivateProfileIntA("Net", "BotStreamHz", 30, g_config_path);
            g_bot_stream_hz = stream_hz < 15 ? 15 : (stream_hz > 60 ? 60 : stream_hz);
            int native_hz = GetPrivateProfileIntA("Net", "NativeBotHz", 30, g_config_path);
            g_native_bot_hz = native_hz < 1 ? 1 : (native_hz > 60 ? 60 : native_hz);
            /* Review 2026-10-11: the stream copies the native encoder output
             * (0x4C71C0 runs once per native send), so a native rate below the
             * stream rate only resends duplicate poses. Clamp up. */
            if (g_native_bot_hz < g_bot_stream_hz) g_native_bot_hz = g_bot_stream_hz;
            int hold_ms = GetPrivateProfileIntA("Net", "BotStreamHoldMs", 500, g_config_path);
            g_bot_stream_hold_ms = hold_ms < 0 ? 0 : (hold_ms > 2000 ? 2000 : hold_ms);
            int blend_ms = GetPrivateProfileIntA("Net", "BotStreamBlendMs", 400, g_config_path);
            g_bot_stream_blend_ms = blend_ms < 1 ? 1 : (blend_ms > 2000 ? 2000 : blend_ms);
            log_line("config: Net BotStream=%d Port=%d StreamHz=%d NativeBotHz=%d HoldMs=%d BlendMs=%d",
                     g_bot_stream, g_bot_stream_port, g_bot_stream_hz, g_native_bot_hz,
                     g_bot_stream_hold_ms, g_bot_stream_blend_ms);
        }
        g_log_debug = coop_ini_int(g_config_path, "Log", "DetailedLog", "Debug", 0) != 0;
        {
            int max_mb = coop_ini_int(g_config_path, "Log", "LogSizeLimitMB", "MaxMB", 20);
            if (max_mb < 1) max_mb = 1;
            if (max_mb > 500) max_mb = 500;
            g_log_max_bytes = (DWORD)max_mb * 1024u * 1024u;
        }
#ifdef COOP_DEV_TOOLS
        g_spike_enabled = GetPrivateProfileIntA("Spike", "Enabled", 0, g_config_path) != 0;
        g_link_enabled = GetPrivateProfileIntA("Link", "Enabled", 0, g_config_path) != 0;
        g_link_port = GetPrivateProfileIntA("Link", "Port", 34510, g_config_path);
        g_link_delay_ms = GetPrivateProfileIntA("Link", "DelayMs", 100, g_config_path);
        GetPrivateProfileStringA("Link", "Peer", "", g_link_peer, sizeof(g_link_peer), g_config_path);
        if (g_link_enabled) {
            g_spike_enabled = 1;
            GetPrivateProfileStringA("Link", "Vehicle", g_spike_vehicle_path, g_spike_vehicle_path,
                                     sizeof(g_spike_vehicle_path), g_config_path);
        }
        GetPrivateProfileStringA("Spike", "Vehicle", g_spike_vehicle_path, g_spike_vehicle_path,
                                 sizeof(g_spike_vehicle_path), g_config_path);
#endif
        g_manifest_port = GetPrivateProfileIntA("CoOp", "ManifestPort", 34500, g_config_path);
        g_mf_sync = coop_ini_int(g_config_path, "CoOp", "ShareMechanicalFailures", "MfSync", 0) != 0;
        g_pit_sync = coop_ini_int(g_config_path, "CoOp", "SharePitStopFailures", "PitSync", 1) != 0;
        GetPrivateProfileStringA("CoOp", "HostAddress", "", g_manifest_host_address,
                                 sizeof(g_manifest_host_address), g_config_path);
        DWORD n = GetModuleFileNameA(NULL, g_game_root, sizeof(g_game_root));
        char *slash = n ? strrchr(g_game_root, '\\') : NULL;
        if (slash) *slash = 0;
        log_line("bot source: %s (game root %s)", g_use_season_bots ? "season" : "ini", g_game_root);
        log_line("config: ExpectedHumans=%ld MinHumans=%ld AIToAdd=%ld HostTimeWarp=%d x%.0f RaceGridRewrite=%d "
                 "ManifestPort=%ld HostAddress=%s Debug=%d MaxMB=%lu",
                 (long)g_expected_humans, (long)g_min_humans, (long)g_requested_ai, (int)g_warp_enabled,
                 g_warp_scale, (int)g_race_grid_rewrite, (long)g_manifest_port,
                 g_manifest_host_address[0] ? "set" : "(LAN broadcast)", g_log_debug,
                 (unsigned long)(g_log_max_bytes / (1024u * 1024u)));
    }
    if (g_requested_ai == 0) {
        log_line("%s is inert; the saved co-op grid contains no AI bots", MOD_NAME);
        FreeLibraryAndExitThread(g_module, 0);
    }
    if (g_expected_humans < 1 || g_requested_ai < 1 || g_requested_ai > MAX_AI ||
        g_expected_humans + g_requested_ai > MAX_VEHICLES) {
        log_line("%s refused to start: ExpectedHumans/AIToAdd is out of range", MOD_NAME);
        FreeLibraryAndExitThread(g_module, 2);
    }
    for (int32_t index = 0; index < MAX_AI; ++index) {
        g_ai_slots[index] = -1;
        g_ai_roster_indices[index] = -1;
        g_ai_roles[index] = -1;
    }
    target = validate_supported_target(&target_variant);
    if (target != F1C_TARGET_OK) {
        log_line("%s refused target: %s %s", MOD_NAME,
                 f1c_target_result_string(target), g_probe_failure);
        FreeLibraryAndExitThread(g_module, 3);
    }
    log_line("target accepted: %s; %d called function(s) already detoured by another module",
             target_variant, g_probe_detoured);
    g_unload_event = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT);
    if (!g_unload_event) FreeLibraryAndExitThread(g_module, 4);

    InitializeCriticalSection(&g_bot_stream_lock);
    QueryPerformanceFrequency(&g_bot_qpc_frequency);
    hook_result = install_hooks();
    if (hook_result != F1C_DETOUR_OK) {
        log_line("hook installation refused: %d", hook_result);
        f1c_hook_registry_remove_all(&g_registry);
        CloseHandle(g_unload_event);
        FreeLibraryAndExitThread(g_module, 5);
    }
    log_line("prototype armed: %ld selected unique AI use stock full vehicle/AI setup; host adds at %ld human(s), client at %ld (reserved human cars=%ld)",
             (long)g_requested_ai, (long)g_min_humans, (long)g_expected_humans,
             (long)g_reserved_human_count);
    install_menu_hooks();
    if (NET_MODERN_AVAILABLE) install_conn_menu_hooks();   /* 1.0.2: no Modern step after LAN */
    InitializeCriticalSection(&g_manifest_lock);
    for (MfbChannel *ch : g_channels) mfb_create(*ch);   /* MF + Pit Stop Failures sync bridges */
    g_net_thread = CreateThread(NULL, 0, manifest_net_thread, NULL, 0, NULL);
    g_bot_stream_thread = CreateThread(NULL, 0, bot_stream_worker, NULL, 0, NULL);
    InitializeCriticalSection(&g_content_lock);
    g_content_thread = CreateThread(NULL, 0, content_server_thread, NULL, 0, NULL);
    InterlockedExchange(&g_ready, 1);
    g_coopu_track_flag = &g_track_in_query;   /* 2026-10-10 crash attribution */
    coopu_veh_install();   /* 0.9.1: crash summary on a real fault */
    while (WaitForSingleObject(g_unload_event, 500) == WAIT_TIMEOUT)
        coopu_pump();       /* 0.9.1: session-end usage write off the game thread */
    InterlockedExchange(&g_stopping, 1);
    if (g_net_thread) {
        WaitForSingleObject(g_net_thread, 3000);
        CloseHandle(g_net_thread);
    }
    if (g_bot_stream_thread) {
        WaitForSingleObject(g_bot_stream_thread, 3000);
        CloseHandle(g_bot_stream_thread);
    }
    if (g_content_thread) {
        WaitForSingleObject(g_content_thread, 3000);
        CloseHandle(g_content_thread);
    }
    if (!remove_menu_hooks()) ++detach_failures;
    if (!remove_conn_menu_hooks()) ++detach_failures;

    for (index = g_registry.count; index > 0; --index)
        if (detach_preserving_trampoline(g_registry.hooks[index - 1]) != F1C_DETOUR_OK)
            ++detach_failures;
    while ((g_active_calls != 0 || g_menu_active_calls != 0) && wait_count++ < 2000) Sleep(1);
    apply_start_timer(0);  /* after the time-step hook is detached and idle */
    apply_leader_slot_limit(0);
    slow_laps_release_all();  /* dev13: restore bot pace scalars */
    blue_flags_release_all();
    if (!detach_failures && g_active_calls == 0) {
        for (index = 0; index < g_registry.count; ++index) {
            VirtualFree(g_registry.hooks[index]->trampoline, 0, MEM_RELEASE);
            g_registry.hooks[index]->trampoline = NULL;
        }
    }
    log_line("unload: detach_failures=%u active_calls=%ld",
             detach_failures, (long)g_active_calls);
    coopu_veh_remove();
    coopu_flush("unload");
    CloseHandle(g_unload_event);
    g_unload_event = NULL;
    if (!detach_failures && g_active_calls == 0 && g_menu_active_calls == 0)
        FreeLibraryAndExitThread(g_module, 0);
    return 0;
}

} // namespace multiplayer_ai_host

extern "C" __declspec(dllexport) const char *F1CModId(void) { return MOD_ID; }
extern "C" __declspec(dllexport) const char *F1CModSdkVersion(void)
{
    return F1C_SDK_VERSION_STRING;
}
extern "C" __declspec(dllexport) int F1CModReady(void)
{
    return multiplayer_ai_host::g_ready != 0;
}

/* Replay naming (REPLAY_NAMING_SPEC 22): read-only session kind for the loader.
 * 0 = not in multiplayer (game mode != 5) or unknown, 1 = plain MULTIPLAYER,
 * 2 = CO-OP. CO-OP is g_coop (set by the main-menu spinner, coop_menu_logic.c
 * coop_sanitize: mode 5 AND g_coop); the same test as coop_mode_is_selected(). */
extern "C" __declspec(dllexport) int F1CMultiplayerOverhaulSessionKind(void)
{
    if (*multiplayer_ai_host::game_mode_ptr() != 5) return 0;
    return multiplayer_ai_host::coop_mode_is_selected() ? 2 : 1;
}

/* 0.9.0: MOD_STANDARD R1. Only the Lightloader protected host contract arms the
 * mod (same checks as ESC Pause Menu 1.1.1); DllMain stays inert so ordinary
 * DLL loaders and stray proxies cannot activate it. */
static volatile LONG g_loader_init_state;

static BOOL validate_loader_host(const LightLoaderProtectedHostV2 *host)
{
    wchar_t path[MAX_PATH];
    wchar_t *base;
    union { FARPROC raw; LightLoaderProofProc typed; } proof = {0};
    if (!host || host->size < sizeof(*host) ||
        host->magic != LLP_ABI_MAGIC ||
        host->process_id != GetCurrentProcessId() ||
        !host->loader_module ||
        host->loader_module != GetModuleHandleW(L"dinput8.dll") ||
        host->abi_version < LLP_ABI_VERSION ||
        host->product_id != LLP_PRODUCT_ID ||
        (host->capabilities & (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2)) !=
            (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2) ||
        host->compatibility_generation < LLP_HUB_2_GENERATION ||
        host->seal != LightLoaderHostSeal(host))
        return FALSE;
    if (!GetModuleFileNameW(host->loader_module, path, MAX_PATH))
        return FALSE;
    path[MAX_PATH - 1] = L'\0';
    base = wcsrchr(path, L'\\');
    if (_wcsicmp(base ? base + 1 : path, L"dinput8.dll") != 0)
        return FALSE;
    proof.raw = GetProcAddress(host->loader_module, LLP_PROOF_NAME);
    return proof.typed &&
        proof.typed(LightLoaderProofChallenge(host)) == host->proof;
}

extern "C" __declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(
    const LightLoaderProtectedHostV2 *host)
{
    HANDLE thread;
    if (InterlockedCompareExchange(&g_loader_init_state, 1, 0) != 0)
        return FALSE;
    if (!validate_loader_host(host)) {
        InterlockedExchange(&g_loader_init_state, 0);
        return FALSE;
    }
    thread = CreateThread(NULL, 0, multiplayer_ai_host::mod_worker, NULL, 0, NULL);
    if (!thread) {
        InterlockedExchange(&g_loader_init_state, 0);
        return FALSE;
    }
    CloseHandle(thread);
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH) {
        multiplayer_ai_host::coopu_exit();   /* 0.9.1: summary of a session that ended by process exit */
        return TRUE;
    }
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    multiplayer_ai_host::g_module = (HMODULE)instance;
    DisableThreadLibraryCalls(instance);
    return TRUE;
}
