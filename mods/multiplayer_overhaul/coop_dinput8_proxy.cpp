/* multiplayer_overhaul laptop loader: dinput8 proxy that preloads multiplayer_overhaul.dll
 * (falls back to the legacy coop_mode.dll / coop_mode.ini names when the new ones are absent).
 * Derived from multiplayer_ai_host/dinput8_proxy.cpp (2026-10-09). */
#include <windows.h>
#include <dinput.h>
#include <cstring>

typedef HRESULT (WINAPI *DirectInput8CreateFn)(HINSTANCE, DWORD, REFIID,
                                               LPVOID *, LPUNKNOWN);
static HMODULE g_proxy_module;

static bool coop_mode_enabled(void)
{
    char path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(g_proxy_module, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    char *slash = strrchr(path, '\\');
    if (!slash) return false;
    lstrcpynA(slash + 1, "multiplayer_overhaul.ini",
              static_cast<int>(MAX_PATH - (slash + 1 - path)));
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        lstrcpynA(slash + 1, "coop_mode.ini", static_cast<int>(MAX_PATH - (slash + 1 - path)));
    return GetPrivateProfileIntA("Mod", "Enabled", 1, path) != 0;
}

static HMODULE load_system_dinput8(void)
{
    static HMODULE module;
    if (!module) {
        char path[MAX_PATH] = {};
        UINT length = GetSystemDirectoryA(path, MAX_PATH);
        if (!length || length + 13 >= MAX_PATH) return NULL;
        lstrcatA(path, "\\dinput8.dll");
        module = LoadLibraryA(path);
    }
    return module;
}

static void preload_coop_mode(void)
{
    static LONG attempted;
    if (InterlockedExchange(&attempted, 1)) return;
    if (!coop_mode_enabled()) return;
    /* Loading starts the mod worker. Do not poll an export afterward: strict
     * target rejection intentionally unloads the module, so a cached export
     * pointer can become invalid between polling iterations. Startup occurs
     * long before a multiplayer session reaches driver processing. */
    if (!LoadLibraryA("multiplayer_overhaul.dll")) LoadLibraryA("coop_mode.dll");
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE instance, DWORD version, REFIID iid, LPVOID *output, LPUNKNOWN outer)
{
    preload_coop_mode();
    HMODULE system_module = load_system_dinput8();
    if (!system_module) return E_FAIL;
    union {
        FARPROC raw;
        DirectInput8CreateFn call;
    } function = {};
    function.raw = GetProcAddress(system_module, "DirectInput8Create");
    if (!function.call) return E_FAIL;
    return function.call(instance, version, iid, output, outer);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) g_proxy_module = instance;
    return TRUE;
}
