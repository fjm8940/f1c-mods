/* mechanical_failures.c - the merged Mechanical Failures DLL (2026-09-28).
 * =========================================================================
 * Owner directive: "all of these should be merged into one large mechanical
 * failures DLL." This module links the four family translation units
 * (gearbox, engine, suspension, electronics) into ONE DLL while each family
 * keeps its own source file, its own shared-memory control block
 * (F1C_GEARFAIL_CTL / F1C_ENGFAIL_CTL / F1C_SUSFAIL_CTL / F1C_ELECFAIL_CTL)
 * and its own unload event. The existing test panel works unchanged.
 *
 * STARTUP CONTRACT (owner standard 2026-09-24):
 *   - The DLL does NOT activate from DllMain. Activation happens only in the
 *     Lightloader protected entry point rfe_1d3b7a91, after the full v2 host
 *     proof verifies (magic, ABI, product id, exclusive-host capability,
 *     generation, live process id, seal, and the loader-supplied proof).
 *     Loaded by an ordinary injector, the module stays inert and the loader
 *     reports "FAILED protected plug-in initialization".
 *   - DEV EXCEPTION (this development tree only): if
 *     mechanical_failures_dev.ini next to the DLL has [Dev]
 *     AllowInjector=1, DllMain activates anyway so the test panel can
 *     hot-swap builds during development. RELEASE PACKAGING MUST SHIP
 *     WITHOUT THAT INI.
 *
 * Mechanics of the merge:
 *   - Every family source is compiled as its own object with its DllMain
 *     renamed (DllMain_gear / DllMain_engine / DllMain_susp / DllMain_elec);
 *     see dll/build_merged.bat. No family source logic is altered.
 *   - At activation the host creates the four named unload events, starts a
 *     monitor thread, then calls each family DllMain (each spawns its proven
 *     worker thread with its own statics - zero cross-family state).
 *   - Unload: any ONE of the four named events wakes the monitor, which sets
 *     the other three; every family worker ends with MF_EXIT (see mf_exit.h).
 *     Engine hook activation pins this candidate for the process lifetime
 *     (AUDITFIX X1). Workers stop and owned detours are restored on unload;
 *     the module stays mapped. A never-hooked module uses the prior exit path.
 *
 * Host status block (panel): F1C_MECHFAIL_CTL, read-only.
 *
 * The four individual DLLs remain as legacy development artifacts; do NOT
 * inject a legacy DLL together with the merged one (double workers).
 */

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "mf_exit.h"
#include "mf_compat.h"
#include "lightloader_plugin_abi.h"

BOOL WINAPI DllMain_gear(HINSTANCE h, DWORD reason, LPVOID r);
BOOL WINAPI DllMain_engine(HINSTANCE h, DWORD reason, LPVOID r);
BOOL WINAPI DllMain_susp(HINSTANCE h, DWORD reason, LPVOID r);
BOOL WINAPI DllMain_elec(HINSTANCE h, DWORD reason, LPVOID r);

/* E9-H1a: per-family code-pattern manifests, run by the host BEFORE any
 * family DllMain starts. */
const char *mf_engine_compat(void);
const char *mf_gear_compat(void);

#ifndef MF_ENGINE_ONLY
#define MF_ENGINE_ONLY 0   /* 1 = release 1.0.0 build (build_release.bat) */
#endif

#if MF_ENGINE_ONLY
/* engine reads the electronics LIMP MODE rev cap; with no electronics family
 * linked the cap stays 0 = never active */
volatile float g_mf_limp_cap_rads = 0.0f;
volatile DWORD g_mf_limp_cap_tick = 0;
#endif

#define MF_EV_GEAR "F1C_GEARFAIL_UNLOAD"
#define MF_EV_ENG  "F1C_ENGFAIL_UNLOAD"
#define MF_EV_SUS  "F1C_SUSFAIL_UNLOAD"
#define MF_EV_ELEC "F1C_ELECFAIL_UNLOAD"

#define MF_HOST_SHM   "F1C_MECHFAIL_CTL"
#define MF_HOST_MAGIC 0x4D48464Du   /* 'MFHM' */

typedef struct {
    volatile unsigned magic;
    volatile int abi_version;
    volatile int struct_size;
    volatile int host_verified;     /* 1 = activated by the Lightloader proof */
    volatile int dev_injector;      /* 1 = activated by the dev ini exception */
    volatile int families_started;
    volatile int game_pid;
    volatile int activated;
} MfHostCtl;

_Static_assert(sizeof(MfHostCtl) == 32, "MfHostCtl size changed");

static HINSTANCE     g_host = NULL;
static HANDLE        g_host_shm = NULL;
static MfHostCtl    *g_host_ctl = NULL;
static volatile LONG g_exited = 0;
static volatile LONG g_target = 4;
static volatile LONG g_activated = 0;
static volatile LONG g_host_verified = 0;
static volatile LONG g_dev_injector = 0;
static volatile LONG g_families_started = 0;
static volatile LONG g_pin_required = 0;

static void mf_host_log(const char *fmt, ...);

void mf_exit_target_set(int count)
{
    if (count > 0) g_target = count;
}

/* E9-H1b: a family proved that a game hook (or its bytes) may still be live.
 * The module must NEVER be freed then - the host pins it instead. */
void mf_exit_pin_required(void)
{
    InterlockedExchange(&g_pin_required, 1);
}

/* Called by every family worker at the very end (MF_EXIT). */
void mf_thread_exit(HMODULE h)
{
    (void)h;
    if (InterlockedIncrement(&g_exited) >= g_target) {
        if (g_pin_required) {
            /* E9-H1b: never unload unsafe. Pin this module for the process
             * lifetime; the hook bytes/trampoline stay mapped and valid. */
            HMODULE self = NULL;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN
                               | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               (LPCSTR)(const void*)&mf_thread_exit, &self))
                mf_host_log("UNLOAD: PIN failed (%lu); retaining original load reference",
                            GetLastError());
            ExitThread(0);
        }
        Sleep(50);                    /* others are inside ExitThread now */
        FreeLibraryAndExitThread(g_host, 0);
    }
    ExitThread(0);
}

/* E9-H1a: plain append logger (the family log file, same as engine's) so a
 * refusal is durable even though no family worker ran. */
static void mf_host_log(const char *fmt, ...)
{
#if MF_ENGINE_ONLY
    FILE *f = fopen("plugins\\lightloader\\mechanical_failures.log", "a");
#else
    FILE *f = fopen("engine_failure_log.txt", "a");
#endif
    va_list ap;
    if (!f) return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* E9-H1a: read bytes from the live image; 0 = unreadable. */
static int mf_compat_fetch(unsigned int addr, unsigned char *dst, int len)
{
    if (len < 0) return 0;
    if (IsBadReadPtr((const void*)(uintptr_t)addr, (UINT_PTR)len)) return 0;
    memcpy(dst, (const void*)(uintptr_t)addr, (size_t)len);
    return 1;
}

/* E9-H1a: ONE aggregate refusal gate - every required code pattern the merged
 * families patch or depend on, plus the common layout reads (engine's own
 * function additionally checks its session globals). NULL = compatible. */
static const char *mf_compat_check(void)
{
    /* the SDK suspension family's hook site (suspension_failure.c STOLEN at
     * HOOK_SITE, 12 bytes); it is an external source, so the pattern is
     * recorded here as part of the aggregate manifest. */
    static const MfCompatItem items[] = {
        { 0x004441F0u,
          { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0x68, 0x01, 0x00, 0x00 },
          12, "suspension detour prologue 0x4441F0" },
    };
    const char *bad;

    bad = mf_engine_compat();
    if (bad) return bad;
#if MF_ENGINE_ONLY
    /* release 1.0.0 (owner 2026-10-06): engine family only - the gearbox and
     * suspension patterns are not required when those families never start */
    (void)items;
    return NULL;
#endif
    bad = mf_gear_compat();
    if (bad) return bad;
    bad = mf_compat_verify(items, (int)(sizeof items / sizeof items[0]),
                           mf_compat_fetch);
    return bad;
}

/* Any single unload event stops all four families. */
static DWORD WINAPI mf_monitor(void *unused)
{
    const char *names[4] = { MF_EV_GEAR, MF_EV_ENG, MF_EV_SUS, MF_EV_ELEC };
    HANDLE ev[4];
    int i;
    (void)unused;

    for (i = 0; i < 4; i++)
        ev[i] = CreateEventA(NULL, TRUE, FALSE, names[i]);

    if (ev[0] && ev[1] && ev[2] && ev[3]) {
        DWORD r = WaitForMultipleObjects(4, ev, FALSE, INFINITE);
        if (r < 4) {
            for (i = 0; i < 4; i++) SetEvent(ev[i]);
        }
    }

    for (i = 0; i < 4; i++)
        if (ev[i]) CloseHandle(ev[i]);
    return 0;
}

static void mf_publish_host(void)
{
    MfHostCtl *c;
    if (!g_host_shm) {
        g_host_shm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                        PAGE_READWRITE, 0, sizeof(MfHostCtl),
                                        MF_HOST_SHM);
        if (g_host_shm) {
            g_host_ctl = (MfHostCtl*)MapViewOfFile(g_host_shm,
                                                   FILE_MAP_ALL_ACCESS,
                                                   0, 0, sizeof(MfHostCtl));
            if (g_host_ctl) memset((void*)g_host_ctl, 0, sizeof(MfHostCtl));
        }
        if (g_host_ctl) {
            g_host_ctl->magic = MF_HOST_MAGIC;
            g_host_ctl->abi_version = 1;
            g_host_ctl->struct_size = (unsigned)sizeof(MfHostCtl);
        }
    }
    c = g_host_ctl;
    if (!c || c->magic != MF_HOST_MAGIC) return;
    c->host_verified = (int)g_host_verified;
    c->dev_injector = (int)g_dev_injector;
    c->families_started = (int)g_families_started;
    c->game_pid = (int)GetCurrentProcessId();
    /* E9-H1a: a refused activation reports INACTIVE (no family started). */
    c->activated = (int)(g_families_started > 0);
}

static void mf_activate(HINSTANCE inst, int via_loader)
{
    HANDLE th;
    int started = 0;
    const char *bad;

    if (InterlockedCompareExchange(&g_activated, 1, 0) != 0)
        return;                        /* only one activation, ever */

    /* E9-H1a (R03): ONE aggregate compatibility check of every required code
     * pattern and layout read runs BEFORE the first family DllMain - a
     * mismatch starts nothing and reports the host block as inactive. */
    bad = mf_compat_check();
    if (bad) {
        mf_host_log("COMPAT: refused (%s) - no family started", bad);
        mf_publish_host();
        return;
    }

    if (via_loader) InterlockedExchange(&g_host_verified, 1);
    else            InterlockedExchange(&g_dev_injector, 1);

    th = CreateThread(NULL, 0, mf_monitor, NULL, 0, NULL);
    if (th) CloseHandle(th);

#if MF_ENGINE_ONLY
    if (DllMain_engine(inst, DLL_PROCESS_ATTACH, NULL)) started++;
#else
    if (DllMain_gear(inst, DLL_PROCESS_ATTACH, NULL))   started++;
    if (DllMain_engine(inst, DLL_PROCESS_ATTACH, NULL)) started++;
    if (DllMain_susp(inst, DLL_PROCESS_ATTACH, NULL))   started++;
    if (DllMain_elec(inst, DLL_PROCESS_ATTACH, NULL))   started++;
#endif
    InterlockedExchange(&g_families_started, started);
    mf_exit_target_set(started);
    mf_publish_host();
}

/* ---- Lightloader protected plug-in entry (ABI v2) -----------------------
 * Reference implementation: lightloader v1.1.0 protected_plugin_smoke.c.
 * Fail-closed: any mismatch returns FALSE and the loader frees the module. */
static uint64_t mf_resolve_proof(const LightLoaderProtectedHostV2 *host)
{
    union {
        FARPROC raw;
        LightLoaderProofProc typed;
    } procedure = {0};
    procedure.raw = GetProcAddress(host->loader_module, LLP_PROOF_NAME);
    return procedure.typed
        ? procedure.typed(LightLoaderProofChallenge(host)) : 0;
}

__declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(
    const LightLoaderProtectedHostV2 *host)
{
    if (!host || host->size < sizeof(*host) ||
        host->magic != LLP_ABI_MAGIC ||
        host->abi_version < LLP_MIN_EXCLUSIVE_ABI ||
        host->product_id != LLP_PRODUCT_ID ||
        !(host->capabilities & LLP_CAP_EXCLUSIVE_HOST) ||
        host->compatibility_generation < LLP_EXCLUSIVE_GENERATION ||
        host->process_id != GetCurrentProcessId() ||
        host->seal != LightLoaderHostSeal(host) ||
        mf_resolve_proof(host) != host->proof) {
        return FALSE;
    }
    mf_activate(g_host, 1);
    return g_families_started > 0;
}

/* ---- dev injector exception --------------------------------------------- */
static int mf_dev_allow_injector(void)
{
    char ini[MAX_PATH];
    char *s;
    DWORD n = GetModuleFileNameA(g_host, ini, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    s = strrchr(ini, '\\');
    if (!s) return 0;
    strcpy(s + 1, "mechanical_failures_dev.ini");
    return GetPrivateProfileIntA("Dev", "AllowInjector", 0, ini) == 1;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_host = inst;
    DisableThreadLibraryCalls(inst);
    /* Inert unless the Lightloader proof path runs, or the explicit dev
     * exception file says the test injector may activate it. */
    if (mf_dev_allow_injector())
        mf_activate(inst, 0);
    return TRUE;
}
