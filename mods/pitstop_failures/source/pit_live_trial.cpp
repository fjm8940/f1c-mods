#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "f1c_runtime.h"

#if F1C_SDK_VERSION_MAJOR != 1
#error This mod requires the F1C SDK 1.x API
#endif

#define MOD_ID "pit_live_trial"
#define MOD_NAME "Pit Stop Live Trial"
#define UNLOAD_EVENT "F1C_PIT_LIVE_TRIAL_UNLOAD"

static HMODULE g_module;
static HANDLE g_unload_event;
static char g_log_path[MAX_PATH];
static char g_config_path[MAX_PATH];

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
    FILE *file;
    va_list args;
    file = fopen(g_log_path, "a");
    if (!file) return;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

static DWORD WINAPI mod_worker(void *unused)
{
    F1CTargetResult target;
    F1CTargetResult strict;
    (void)unused;
    module_sibling_path(g_log_path, sizeof(g_log_path), MOD_ID "_log.txt");
    module_sibling_path(g_config_path, sizeof(g_config_path), MOD_ID ".ini");
    if (!GetPrivateProfileIntA("Mod", "Enabled", 1, g_config_path)) {
        log_line("%s disabled by configuration", MOD_NAME);
        FreeLibraryAndExitThread(g_module, 0);
    }
    target = f1c_target_validate_headers();
    if (target != F1C_TARGET_OK) {
        log_line("%s refused to start: %s", MOD_NAME,
                 f1c_target_result_string(target));
        FreeLibraryAndExitThread(g_module, 2);
    }
    strict = f1c_target_validate_strict();
    log_line("%s loaded; SDK %s; target %s; code probe %s",
             MOD_NAME, F1C_SDK_VERSION_STRING,
             f1c_target_result_string(target),
             f1c_target_result_string(strict));

    g_unload_event = CreateEventA(NULL, TRUE, FALSE, UNLOAD_EVENT);
    if (!g_unload_event) {
        log_line("CreateEvent failed: %lu", (unsigned long)GetLastError());
        FreeLibraryAndExitThread(g_module, 3);
    }

    /* Passive starter: add observations here. Queue simulation mutations with
     * f1c_dispatch_post and drain them only from a reviewed game-tick hook. */
    WaitForSingleObject(g_unload_event, INFINITE);
    CloseHandle(g_unload_event);
    g_unload_event = NULL;
    log_line("%s unloading cleanly", MOD_NAME);
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
    HANDLE thread;
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = (HMODULE)instance;
    DisableThreadLibraryCalls(instance);
    thread = CreateThread(NULL, 0, mod_worker, NULL, 0, NULL);
    if (!thread) return FALSE;
    CloseHandle(thread);
    return TRUE;
}
