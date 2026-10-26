/*
 * esc_pause_menu.dll -- vanilla ESC pause menu for F1 Challenge 99-02.
 *
 * Vanilla behavior: ESC (key-queue DIK 1) makes RestartGate_004a0fd0 call
 * MainMenuTransition and abandon the race straight to RACE RESULTS
 * (findings.jsonl "escape-scancode-1-abandons-race", FACT). This mod arms
 * the broker's ABI-10 pause-menu capability instead: ESC opens a frozen
 * in-race menu (CONTINUE / RETIRE TO PITS / EXIT RACE; qualifying and
 * practice get CONTINUE / RETURN TO PITS / EXIT), navigated
 * with Up/Down and confirmed with Space. ESC while the menu is open resumes
 * the race (CONTINUE), matching NT2004's same-engine RestartGate.
 *
 * The broker owns all game-thread work (ESC interception, sim freeze via
 * byte_7C56AC, menu rendering on the HUD strip, key swallowing, the EXIT
 * leaf call). This mod is a thin client: it discovers and arms the broker,
 * polls F1C_BROKER_EVENT_PAUSE_MENU, and owns the one thing the game thread
 * must not block on -- the retirement handoff. For an in-race RETIRE TO PITS,
 * the client temporarily presents the handoff as session type 5 while the
 * game consumes PLAYER_OUT. This is the proven route to the garage spectator
 * view; the original type is restored immediately after consumption.
 */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include "f1c_runtime.h"
#include "f1c_broker.h"
#include "f1c_broker_session_end.h"
#include "esc_usage.h"
#include "protected_plugin_abi.h"

#define ESC_MARK_BEGIN(name) ((void)(name))
#define ESC_MARK_END() ((void)0)

#if F1C_SDK_VERSION_MAJOR != 1
#error This mod requires the F1C SDK 1.x API
#endif

#define MOD_ID "esc_pause_menu"
#define MOD_NAME "ESC Pause Menu"
#define UNLOAD_EVENT "F1C_ESC_PAUSE_MENU_UNLOAD"

/* --- retirement anchors (FACT/CONFIRMED, pit_commentary.c) -------------- */
#define SESSION_TYPE      (*(volatile int32_t *)0x007C56A4u) /* 5=qual 7=race */
#define GAME_STATE        (*(volatile int32_t *)0x007C569Cu)
#define PLAYER_OUT        (*(volatile int32_t *)0x007CE258u)
#define MINI_RESTART_FLAG (*(volatile uint8_t *)0x007CE25Du)
#define FULL_RESTART_FLAG (*(volatile uint8_t *)0x007CE25Eu)
/* VEH_BASE comes from f1c_sdk.h (0x5EC6D8, slot 0 = player). */
#define VEH_OFF_CARC      0x3D6Cu     /* carConditions: 2 = terminal */
#define VEH_OFF_FINISH    0x4064u     /* FinishState: 2 = retired, 3 = DQ */
#define VEH_OFF_GARAGE_A  0x4488u     /* garaged state pair */
#define VEH_OFF_GARAGE_B  0x4578u
#define VEH_OFF_DRIVER_VIS 0x105Du    /* driver model visible */

static HMODULE g_module;
static HANDLE g_unload_event;
static F1CBrokerSetSessionEndFn g_session_end_bind;
static char g_log_path[MAX_PATH];
static char g_config_path[MAX_PATH];
static volatile LONG g_retire_active;

/* Singleplayer only. GameModuleManager+0x3FC4 is the multiplayer session state
 * (Red Flag mp_state; measured offline value 0). In multiplayer, or when the
 * state cannot be read, the menu stays disarmed so ESC keeps its vanilla
 * behaviour and time never stops. Read-only. */
#define GAME_MODULE_MANAGER 0x007CD25Cu
#define GAME_MODULE_MP_STATE 0x3FC4u

static int mp_state(void)
{
    MEMORY_BASIC_INFORMATION region;
    uintptr_t inner = *(volatile uintptr_t *)GAME_MODULE_MANAGER;
    uintptr_t field = inner + GAME_MODULE_MP_STATE;
    if (!inner || !VirtualQuery((LPCVOID)field, &region, sizeof(region)) ||
        region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
        field + 4u > (uintptr_t)region.BaseAddress + region.RegionSize)
        return -1;
    return *(volatile int32_t *)field;
}
static volatile LONG g_loader_init_state;

/* Only Lightloader 1.1.0's exclusive host contract may arm this client.
 * DllMain remains inert so ordinary DLL loaders cannot activate the menu. */
static __attribute__((noinline)) BOOL loader_policy_matches(
    const LightLoaderProtectedHostV2 *host)
{
    BOOL matches;
    ESC_MARK_BEGIN("esc_loader_exclusivity_policy");
    matches = host->abi_version >= LLP_ABI_VERSION &&
        host->product_id == LLP_PRODUCT_ID &&
        (host->capabilities & (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2)) ==
            (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2) &&
        host->compatibility_generation >= LLP_HUB_2_GENERATION;
    ESC_MARK_END();
    return matches;
}

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
        !loader_policy_matches(host) ||
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

static void module_sibling_path(char *out, size_t capacity, const char *name)
{
    char *slash;
    DWORD length = GetModuleFileNameA(g_module, out, (DWORD)capacity);
    if (!length || length >= capacity) {
        lstrcpynA(out, name, (int)capacity);
        return;
    }
    slash = strrchr(out, '\\');
    if (slash) lstrcpynA(slash + 1, name, (int)(capacity - (slash + 1 - out)));
}

static void log_line(const char *format, ...)
{
    char msg[512];
    FILE *file;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof msg, format, args);
    va_end(args);
    escu_tap(msg);   /* batch U2: feed the usage summary */
    file = fopen(g_log_path, "a");
    if (!file) return;
    fprintf(file, "[%lu] ", (unsigned long)GetTickCount());
    fputs(msg, file);
    fputc('\n', file);
    fclose(file);
}

typedef struct RetireContext {
    F1CBrokerApi api;
    F1CBrokerToken token;
    uint32_t session_generation;
    DWORD hold_max_ms;
    DWORD guard_ms;
} RetireContext;

/* Menu retirement is not a mechanical failure. Preserve the game's existing
 * ReasonCode: forcing 6 here labels an ordinary retirement "Accident". A DQ
 * is already FinishState 3 and must not be replaced by retirement state 2. */
static void retire_player(void)
{
    volatile LONG *finish = (volatile LONG *)(VEH_BASE + VEH_OFF_FINISH);
    LONG previous_finish;
    *(volatile int32_t *)(VEH_BASE + VEH_OFF_CARC) = 2;
    previous_finish = InterlockedCompareExchange(finish, 2, 0);
    log_line("RETIRE: FinishState before=%ld after=%ld",
             (long)previous_finish, (long)*finish);
    *(volatile int32_t *)(VEH_BASE + VEH_OFF_GARAGE_A) = 1;
    *(volatile int32_t *)(VEH_BASE + VEH_OFF_GARAGE_B) = 1;
    PLAYER_OUT = 1;
}

static DWORD WINAPI retire_worker(void *context)
{
    RetireContext *ctx = (RetireContext *)context;
    int session0;
    int state0 = GAME_STATE;
    DWORD t0;
    int ate_mini = 0, ate_full = 0;

    /* Fail closed: if the session moved on between the broker executing the
     * command and this thread starting, do nothing. */
    F1CBrokerSessionSnapshot snapshot = {0};
    snapshot.structure_size = sizeof(snapshot);
    if (!ctx->api.get_session_snapshot ||
        ctx->api.get_session_snapshot(ctx->token, ctx->session_generation,
                                      &snapshot) != F1C_BROKER_OK ||
        !snapshot.session_active) {
        log_line("RETIRE: session moved on; aborting");
        InterlockedDecrement(&g_retire_active);
        HeapFree(GetProcessHeap(), 0, ctx);
        return 0;
    }
    session0 = SESSION_TYPE;
    if (snapshot.session_type != session0) {
        log_line("RETIRE: session type changed (%d snapshot, %d live); aborting",
                 snapshot.session_type, session0);
        InterlockedDecrement(&g_retire_active);
        HeapFree(GetProcessHeap(), 0, ctx);
        return 0;
    }
    log_line("RETIRE: snapshot ok (type %d), writing retire set", session0);

    MINI_RESTART_FLAG = 0;
    FULL_RESTART_FLAG = 0;
    if (session0 == 7) {
        SESSION_TYPE = 5;
        log_line("RETIRE: garage route armed (SessionType 7 -> 5)");
    }
    retire_player();

    /* Phase 1: wait for the gate. PLAYER_OUT 1->0 is the exact consumption
     * signal (ConditionalInitGuard_004a0cf0 clears it after sub_4A0250). */
    t0 = GetTickCount();
    for (;;) {
        DWORD dt = GetTickCount() - t0;
        if (session0 == 7) SESSION_TYPE = 5;
        if (MINI_RESTART_FLAG) { MINI_RESTART_FLAG = 0; ate_mini++; }
        if (FULL_RESTART_FLAG) { FULL_RESTART_FLAG = 0; ate_full++; }
        if (PLAYER_OUT == 0) {
            log_line("RETIRE: gate consumed PLAYER_OUT after %ums (GameState %d -> %d)",
                     dt, state0, GAME_STATE);
            break;
        }
        if (dt >= ctx->hold_max_ms) {
            log_line("RETIRE: PLAYER_OUT still %d after %ums - continuing (GameState %d)",
                     PLAYER_OUT, ctx->hold_max_ms, GAME_STATE);
            break;
        }
        Sleep(5);
    }

    if (session0 == 7) {
        SESSION_TYPE = session0;
        log_line("RETIRE: SessionType restored to %d", session0);
    }

    /* Phase 2: keep eating restart flags through the tail. */
    t0 = GetTickCount();
    while (GetTickCount() - t0 < ctx->guard_ms) {
        if (MINI_RESTART_FLAG) { MINI_RESTART_FLAG = 0; ate_mini++; }
        if (FULL_RESTART_FLAG) { FULL_RESTART_FLAG = 0; ate_full++; }
        Sleep(5);
    }
    if (ate_mini || ate_full)
        log_line("RETIRE: ate mini=%d full=%d restart flags", ate_mini, ate_full);

    /* Driver model hide AFTER the teardown consumed PLAYER_OUT (the crew
     * state machine resets it during teardown; writing before loses it). */
    if (PLAYER_OUT == 0)
        *(volatile uint8_t *)(VEH_BASE + VEH_OFF_DRIVER_VIS) = 0;

    InterlockedDecrement(&g_retire_active);
    log_line("RETIRE: complete; SessionType %d restored; GameState %d -> %d",
             session0, state0, GAME_STATE);
    HeapFree(GetProcessHeap(), 0, ctx);
    return 0;
}

static void start_retire(const F1CBrokerApi *api, F1CBrokerToken token,
                         uint32_t session_generation)
{
    RetireContext *ctx;
    HANDLE thread;
    if (InterlockedCompareExchange(&g_retire_active, 1, 0) != 0) {
        log_line("RETIRE: already active, ignoring duplicate command");
        return;
    }
    ctx = (RetireContext *)HeapAlloc(GetProcessHeap(), 0, sizeof(*ctx));
    if (!ctx) {
        InterlockedDecrement(&g_retire_active);
        log_line("RETIRE: allocation failed");
        return;
    }
    *ctx = (RetireContext){
        .api = *api,
        .token = token,
        .session_generation = session_generation,
        .hold_max_ms = (DWORD)GetPrivateProfileIntA("Retire", "HoldMaxMs", 900,
                                                    g_config_path),
        .guard_ms = (DWORD)GetPrivateProfileIntA("Retire", "GuardMs", 2500,
                                                 g_config_path)
    };
    thread = CreateThread(NULL, 0, retire_worker, ctx, 0, NULL);
    if (!thread) {
        HeapFree(GetProcessHeap(), 0, ctx);
        InterlockedDecrement(&g_retire_active);
        log_line("RETIRE: thread creation failed: %lu", (unsigned long)GetLastError());
    } else {
        CloseHandle(thread);
    }
}

/* 1.0.9: game-thread broker session-end callback. Sets a flag only; the ESC
 * worker loop calls escu_pump() to perform the file write. */
static void __cdecl esc_session_end(void *context)
{
    (void)context;
    escu_request();
}

static DWORD WINAPI mod_worker(void *unused)
{
    F1CBrokerApi api;
    F1CBrokerToken token = 0;
    F1CBrokerDiscoveryResult discovery;
    F1CBrokerResult result;
    uint64_t cursor = 0;
    int armed = 0;
    int observed_menu_open = 0;
    int menu_armed = -1;
    DWORD next_arm_refresh = GetTickCount(); /* first deadline must share the uptime epoch */
    /* 2026-10-10 (owner): in multiplayer/CO-OP the menu is offered without
     * pausing (broker arm value 2). Older brokers refuse 2 -> vanilla ESC. */
    int mp_menu = GetPrivateProfileIntA("Multiplayer", "MenuWithoutPause", 1, g_config_path) != 0;
    (void)unused;
    module_sibling_path(g_log_path, sizeof(g_log_path), MOD_ID "_log.txt");
    module_sibling_path(g_config_path, sizeof(g_config_path), MOD_ID ".ini");
    if (!GetPrivateProfileIntA("Mod", "Enabled", 1, g_config_path)) {
        log_line("%s disabled by configuration", MOD_NAME);
        FreeLibraryAndExitThread(g_module, 0);
    }
    log_line("%s loaded through Lightloader; SDK %s", MOD_NAME,
             F1C_SDK_VERSION_STRING);

    /* Negotiate only what this mod needs: lifecycle, the pause menu, and the
     * event poll the retirement handoff depends on. LightLoader scans DLLs
     * alphabetically, so this client can start just before the broker. Retry
     * NOT_FOUND and QUERY_FAILED can both be transient while the broker DLL
     * is loading: its GetInfo export exists before g_ready is set. Retry
     * either for the same bounded startup window. Persistent malformed
     * providers still fail closed after the window; ABI/capability/duplicate
     * failures remain immediate. */
    {
        DWORD discovery_started = GetTickCount();
        do {
            memset(&api, 0, sizeof(api));
            api.structure_size = sizeof(api);
            discovery = f1c_broker_discover(5u, F1C_BROKER_ABI_VERSION,
                                            F1C_BROKER_CAP_PAUSE_MENU |
                                                F1C_BROKER_CAP_POLL_EVENTS,
                                            &api);
            if (discovery != F1C_BROKER_DISCOVERY_NOT_FOUND &&
                discovery != F1C_BROKER_DISCOVERY_QUERY_FAILED)
                break;
            Sleep(25);
        } while (GetTickCount() - discovery_started < 10000u);
        if (discovery == F1C_BROKER_DISCOVERY_OK)
            log_line("broker discovery completed after %ums",
                     (unsigned)(GetTickCount() - discovery_started));
    }
    if (discovery != F1C_BROKER_DISCOVERY_OK) {
        log_line("%s refused to start: broker discovery %d", MOD_NAME,
                 (int)discovery);
        FreeLibraryAndExitThread(g_module, 4);
    }
    log_line("broker discovered: ABI %u capabilities 0x%llX",
             api.negotiated_abi, (unsigned long long)api.capabilities);

    g_unload_event = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT);
    if (!g_unload_event) {
        log_line("CreateEvent failed: %lu", (unsigned long)GetLastError());
        f1c_broker_release(&api);
        FreeLibraryAndExitThread(g_module, 3);
    }

    result = api.register_client(g_module, MOD_ID, &token);
    if (result != F1C_BROKER_OK) {
        log_line("register failed: %d", (int)result);
        CloseHandle(g_unload_event);
        f1c_broker_release(&api);
        FreeLibraryAndExitThread(g_module, 5);
    }
    /* 1.0.9: optional ABI15 session-end hook writes the usage summary after the
     * race, outside DllMain and off the game thread. Absence only disables the
     * session-end flush. */
    g_session_end_bind = f1c_broker_session_end_bind(&api);
    if (g_session_end_bind &&
        g_session_end_bind(token, esc_session_end, NULL) != F1C_BROKER_OK) {
        g_session_end_bind = NULL;
        log_line("session-end usage hook unavailable; summary flushes only at unload");
    }
    result = api.pause_menu_arm(token, 0u);
    if (result != F1C_BROKER_OK) {
        log_line("arm failed: %d", (int)result);
        api.unregister_client(token, 5000u);
        CloseHandle(g_unload_event);
        f1c_broker_release(&api);
        FreeLibraryAndExitThread(g_module, 6);
    }
    armed = 1;
    log_line("pause menu registered; armed only in singleplayer");
    escu_veh_install();   /* batch U2: crash summary on a real fault */

    for (;;) {
        F1CBrokerEvent events[16];
        uint32_t count = 0;
        uint64_t latest = 0, dropped = 0;
        uint32_t i;
        DWORD wait;
        DWORD now = GetTickCount();
        if ((LONG)(now - next_arm_refresh) >= 0) {
            int mp = mp_state();
            uint32_t want = mp == 0 ? 1u : (mp > 0 && mp_menu) ? 2u : 0u;
            result = api.pause_menu_arm(token, want);
            if (result == F1C_BROKER_BAD_ARGUMENT && want == 2u) {
                log_line("broker has no menu-without-pause; multiplayer keeps vanilla ESC");
                mp_menu = 0;
                want = 0u;
                result = api.pause_menu_arm(token, want);
            }
            if (result != F1C_BROKER_OK)
                log_line("arm refresh failed: %d", (int)result);
            else if ((int)want != menu_armed) {
                log_line("pause menu %s (mp_state=%d)",
                         want == 1u ? "armed: singleplayer" :
                         want == 2u ? "armed: multiplayer, no pause" : "off: unknown state, vanilla ESC",
                         mp);
                menu_armed = (int)want;
            }
            next_arm_refresh = now + 250u;
        }
        if (api.poll_events(token, cursor, events, 16u, &count, &cursor,
                            &latest, &dropped) == F1C_BROKER_OK) {
            for (i = 0; i < count; ++i) {
                if (events[i].type != F1C_BROKER_EVENT_PAUSE_MENU) continue;
                if (events[i].value == F1C_BROKER_MENU_OPEN) {
                    observed_menu_open = 1;
                } else if (events[i].value == F1C_BROKER_MENU_RETIRE_PITS &&
                           observed_menu_open) {
                    observed_menu_open = 0;
                    log_line("RETIRE command received (session gen %u)",
                             events[i].session_generation);
                    /* 1.0.6: the race retire handoff forced SessionType 7->5 from
                     * this thread; players reported races jumping back to Q2 /
                     * Warm-up and unlimited laps. Race retire is disabled until a
                     * game-thread handoff exists. Use EXIT RACE instead. */
                    if (SESSION_TYPE == 7) {
                        log_line("RETIRE: disabled in race (1.0.6); no game state changed");
                    } else {
                        log_line("RETIRE: starting retire thread");
                        start_retire(&api, token, events[i].session_generation);
                    }
                } else if (events[i].value == F1C_BROKER_MENU_RETIRE_PITS) {
                    log_line("RETIRE: ignored external command without local menu open");
                } else if (events[i].value == F1C_BROKER_MENU_CONTINUE ||
                           events[i].value == F1C_BROKER_MENU_EXIT_RACE ||
                           events[i].value == F1C_BROKER_MENU_REPLAY) {
                    observed_menu_open = 0;
                }
            }
        }
        escu_pump();   /* 1.0.9: honour a pending session-end usage request */
        wait = WaitForSingleObject(g_unload_event, 50);
        if (wait == WAIT_OBJECT_0) break;
    }

    log_line("%s unloading", MOD_NAME);
    if (armed) api.pause_menu_arm(token, 0u);
    /* Wait briefly for an in-flight retirement to finish so the broker never
     * sees the client vanish mid-sequence. */
    {
        uint32_t wait_loop;
        for (wait_loop = 0; wait_loop < 100 && g_retire_active; ++wait_loop)
            Sleep(50);
    }
    if (g_session_end_bind) {
        g_session_end_bind(token, NULL, NULL);
        g_session_end_bind = NULL;
    }
    api.unregister_client(token, 5000u);
    CloseHandle(g_unload_event);
    g_unload_event = NULL;
    f1c_broker_release(&api);
    log_line("%s unloading cleanly", MOD_NAME);
    escu_veh_remove();      /* batch U2 */
    escu_flush("unload");   /* batch U2: write the usage summary */
    FreeLibraryAndExitThread(g_module, 0);
    return 0;
}

__declspec(dllexport) const char *F1CModId(void) { return MOD_ID; }
__declspec(dllexport) const char *F1CModSdkVersion(void)
{
    return F1C_SDK_VERSION_STRING;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = (HMODULE)instance;
    DisableThreadLibraryCalls(instance);
    return TRUE;
}

__declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(
    const LightLoaderProtectedHostV2 *host)
{
    HANDLE thread;
    if (InterlockedCompareExchange(&g_loader_init_state, 1, 0) != 0)
        return FALSE;
    if (!validate_loader_host(host)) {
        InterlockedExchange(&g_loader_init_state, 0);
        return FALSE;
    }
    thread = CreateThread(NULL, 0, mod_worker, NULL, 0, NULL);
    if (!thread) {
        InterlockedExchange(&g_loader_init_state, 0);
        return FALSE;
    }
    CloseHandle(thread);
    return TRUE;
}
