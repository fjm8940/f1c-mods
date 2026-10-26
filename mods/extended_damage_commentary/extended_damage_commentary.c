/*
 * extended_damage_commentary.c — Extended Damage Commentary
 *
 * Restores: PWS_HIT01, PWS_HIT02, PWS_HIT03, PWS_HIT04, PWS_HIT05, PWS_HIT06
 * Status:   RESTORED — heard in-game 2026-07-27
 *
 * WHAT IT DOES
 *   The race engineer calls out the player losing a front wing, a rear wing,
 *   or any of the four wheels.
 *
 * WHY IT IS NEEDED
 *   The game ships a working emitter for all six events. Each one's cooldown
 *   in the event table was set to FLT_MAX, which the engine reads as 'never
 *   replay this' — so the emitter fired and the line was silently dropped.
 *   Setting the cooldown to a real number is the entire fix. 15.0 s is not
 *   invented: it is the cooldown the game itself gives the AI counterparts
 *   of these same events (AIHit01/02/07/08, eids 220-223).
 *
 * HOW IT IS APPLIED
 *   This is a RUNTIME patch, not a file patch. The same bytes that
 *   patches/manifest.json would write into the .exe on disk are written
 *   into the running process at DllMain attach. The player's game files
 *   are never modified; not loading the DLL is the uninstall.
 *
 *   Every site verifies the ORIGINAL bytes before writing. A mismatch
 *   means a different game build (or another mod got there first), and
 *   that site is skipped rather than corrupted.
 *
 * EVIDENCE
 *   findings.jsonl (FACT). First six DONE rows in progress.json.
 *
 * ADDRESSES
 *   Generated from patches/manifest.json by tools/build_pack.py.
 *   F1 Challenge is non-ASLR (image base 0x400000), so these virtual
 *   addresses are literal in the running process.
 *
 * BUILD
 *   C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall \
 *       -o extended_damage_commentary.dll extended_damage_commentary.c -lkernel32
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "protected_plugin_abi.h"
#include "edc_usage.h"
#include "f1c_broker.h"
#include "f1c_broker_session_end.h"

static void log_line(const char* fmt, ...);

static const char g_log_filename[] = {
    'e','x','t','e','n','d','e','d','_','d','a','m','a','g','e','_',
    'c','o','m','m','e','n','t','a','r','y','.','l','o','g','\0'
};

#define SITE_KEY 0xA37C19E5UL
#define SITE_COUNT 6

static const unsigned long g_encoded_sites[SITE_COUNT] = {
    0xA3201BF9UL, 0xA3201BBDUL, 0xA3201811UL,
    0xA3201BEDUL, 0xA3201BD5UL, 0xA3201BA1UL
};
static volatile LONG g_loader_init_state;
static HMODULE g_edc_module;
static F1CBrokerApi g_edc_api;
static F1CBrokerToken g_edc_token;
static F1CBrokerSetSessionEndFn g_edc_set_session_end;

/* Broker game-thread session-end callback: set a request flag only. The EDC
 * worker thread performs the file write (MOD_STANDARD R2: no file I/O on the
 * game thread). Batch U2 fix E-a kept the write out of DllMain (loader lock;
 * dead threads at process exit can hang on the usage spin lock). */
static void __cdecl edc_session_end(void *unused)
{
    (void)unused;
    edcu_request();
}

/* EDC worker loop: the only place a session-end summary is written. A normal
 * return to the menu keeps the game process and the DLL loaded, so this pump
 * (not unload) is what produces the session-end usage file. */
static volatile LONG g_edc_worker_stop;

static DWORD WINAPI edc_worker(void *unused)
{
    (void)unused;
    while (!InterlockedCompareExchange(&g_edc_worker_stop, 0, 0)) {
        edcu_pump();
        Sleep(100);
    }
    return 0;
}

static unsigned long decode_site_value(unsigned long value)
{
    return value ^ SITE_KEY;
}

static void log_line(const char* fmt, ...) {
    char path[MAX_PATH];
    char msg[512];
    va_list ap;
    FILE* f;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    edcu_tap(msg);   /* batch U2: feed the usage summary */
    /* The launcher verifies logs relative to the game root. Keep the license
     * beside the DLL, but write runtime status beside the game executable. */
    if (!GetModuleFileNameA(NULL, path, MAX_PATH) || !strrchr(path, '\\'))
        strcpy(path, g_log_filename);
    else
        strcpy(strrchr(path, '\\') + 1, g_log_filename);
    f = fopen(path, "a");
    if (!f) return;
    fputs(msg, f);
    fputc('\n', f);
    fclose(f);
}

/* Minimal broker client: registration only for the session-end summary. If the
 * broker or capability is unavailable the patch still applies and the usage
 * summary stays local/off (logged). */
static void edc_broker_init(void)
{
    F1CBrokerDiscoveryResult d;
    memset(&g_edc_api, 0, sizeof g_edc_api);
    g_edc_api.structure_size = sizeof g_edc_api;
    d = f1c_broker_discover(5u, F1C_BROKER_ABI_VERSION,
                            F1C_BROKER_CAP_SESSION_END, &g_edc_api);
    if (d != F1C_BROKER_DISCOVERY_OK) {
        log_line("usage: broker discovery %d - session-end summary off", (int)d);
        return;
    }
    if (g_edc_api.register_client(g_edc_module, "extended_damage_commentary",
                                  &g_edc_token) != F1C_BROKER_OK) {
        log_line("usage: broker register failed - session-end summary off");
        f1c_broker_release(&g_edc_api);
        return;
    }
    g_edc_set_session_end = f1c_broker_session_end_bind(&g_edc_api);
    if (!g_edc_set_session_end ||
        g_edc_set_session_end(g_edc_token, edc_session_end, NULL) != F1C_BROKER_OK) {
        log_line("usage: session-end bind failed - summary off");
        g_edc_api.unregister_client(g_edc_token, 0);
        f1c_broker_release(&g_edc_api);
        return;
    }
    {
        HANDLE worker = CreateThread(NULL, 0, edc_worker, NULL, 0, NULL);
        if (worker) CloseHandle(worker);
        else log_line("usage: worker thread creation failed - session-end summary off");
    }
}

/* Returns the number of sites successfully written. */
static int site_is_readable(const void *address)
{
    MEMORY_BASIC_INFORMATION mbi;
    HMODULE exe = GetModuleHandleW(NULL);
    if (!VirtualQuery(address, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        mbi.AllocationBase != exe || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return 0;
    return 1;
}

/* 2026-10-07 audit COMP-04: check every site first, write only when all six
 * are compatible. 2026-10-08 batch E (COMP-04b): the write pass is
 * transactional too - if any protection change or write fails, every site
 * already written is put back to the expected original, so a refusal never
 * leaves some cooldowns patched.
 * Returns SITE_COUNT on success, -1 when the layout is unavailable, any site
 * holds unexpected bytes, or a write fails (all writes reverted then). */
typedef struct {
    int (*readable)(const void *address);
    int (*vprotect)(void *address, unsigned long newprot, unsigned long *oldprot);
} EdcSiteOps;

static int edc_patch_sites(unsigned char *const sites[SITE_COUNT],
                           const unsigned char *expected,
                           const unsigned char *patched, unsigned long width,
                           const EdcSiteOps *ops)
{
    unsigned char *done[SITE_COUNT];
    int i, n = 0;
    /* phase 1: nothing is written until every site is readable and is either
     * the expected original or already patched */
    for (i = 0; i < SITE_COUNT; i++) {
        if (!ops->readable(sites[i])) return -1;
        if (memcmp(sites[i], patched, width) != 0 &&
            memcmp(sites[i], expected, width) != 0)
            return -1;
    }
    /* phase 2: transactional write; any failure rolls every write back */
    for (i = 0; i < SITE_COUNT; i++) {
        unsigned long old;
        if (memcmp(sites[i], patched, width) == 0) continue;   /* already patched */
        if (!ops->vprotect(sites[i], PAGE_EXECUTE_READWRITE, &old))
            goto rollback;
        memcpy(sites[i], patched, width);
        done[n++] = sites[i];
        if (!ops->vprotect(sites[i], old, &old))
            goto rollback;
    }
    return SITE_COUNT;
rollback:
    while (n > 0) {
        unsigned long old;
        n--;
        if (ops->vprotect(done[n], PAGE_EXECUTE_READWRITE, &old)) {
            memcpy(done[n], expected, width);
            ops->vprotect(done[n], old, &old);
        }
    }
    return -1;
}

static int edc_live_readable(const void *address)
{
    return site_is_readable(address);
}

static int edc_live_vprotect(void *address, unsigned long newprot,
                             unsigned long *oldprot)
{
    return VirtualProtect(address, sizeof(unsigned long), newprot,
                          (PDWORD)oldprot) ? 1 : 0;
}

static int apply_all(void) {
    size_t i;
    const unsigned long expected = decode_site_value(0xDC03E61AUL);
    const unsigned long patched = decode_site_value(0xE20C19E5UL);
    unsigned char* sites[SITE_COUNT];
    const EdcSiteOps ops = { edc_live_readable, edc_live_vprotect };
    for (i = 0; i < SITE_COUNT; i++)
        sites[i] = (unsigned char*)decode_site_value(g_encoded_sites[i]);
    return edc_patch_sites(sites, (const unsigned char*)&expected,
                           (const unsigned char*)&patched,
                           (unsigned long)sizeof(patched), &ops);
}

static BOOL validate_loader_host(const LightLoaderProtectedHostV2 *host)
{
    union { FARPROC raw; LightLoaderProofProc typed; } proof = {0};
    if (!host || host->size < sizeof(*host) ||
        host->magic != LLP_ABI_MAGIC ||
        host->abi_version < LLP_ABI_VERSION ||
        host->process_id != GetCurrentProcessId() ||
        !host->loader_module ||
        host->loader_module != GetModuleHandleW(L"dinput8.dll") ||
        host->product_id != LLP_PRODUCT_ID ||
        (host->capabilities & (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2)) !=
            (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2) ||
        host->compatibility_generation < LLP_HUB_2_GENERATION ||
        host->seal != LightLoaderHostSeal(host))
        return FALSE;
    proof.raw = GetProcAddress(host->loader_module, LLP_PROOF_NAME);
    return proof.typed &&
        proof.typed(LightLoaderProofChallenge(host)) == host->proof;
}

__declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(
    const LightLoaderProtectedHostV2 *host)
{
    int written;
    if (InterlockedCompareExchange(&g_loader_init_state, 1, 0) != 0)
        return FALSE;
    if (!validate_loader_host(host)) {
        InterlockedExchange(&g_loader_init_state, 0);
        return FALSE;
    }
#ifdef EDC_HOST_GATE_TEST
    InterlockedExchange(&g_loader_init_state, 0);
    return TRUE;
#endif
    written = apply_all();
    if (written < 0) {
        log_line("extended_damage_commentary: refused; executable layout is unavailable or a site is changed (nothing patched)");
        InterlockedExchange(&g_loader_init_state, 0);
        return FALSE;   /* E-a: on refusal register nothing (no VEH, no broker client) */
    }
    edcu_veh_install();   /* batch U2: crash summary on a real fault */
    edc_broker_init();    /* batch U2 E-a: session-end summary via the broker */
    log_line("extended_damage_commentary: %d/%d sites active", written, SITE_COUNT);
    InterlockedExchange(&g_loader_init_state, 2);
    return written == SITE_COUNT;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r) {
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); g_edc_module = h; }
    /* Batch U2 fix E-a: NO file I/O here - at process exit the loader lock is
     * held and other threads are dead, so a flush could hang the game. The
     * summary is written from the broker session-end callback instead. */
    if (reason == DLL_PROCESS_DETACH) edcu_veh_remove();
    return TRUE;
}
