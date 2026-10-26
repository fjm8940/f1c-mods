/*
 * fast_loading 1.0.0 (Lightloader plug-in)
 *
 * Speeds up race and game-start loading by using the spare CPU cores.
 *
 * - 0x4DE610 entry: replaced. The small input (a2) is deep-copied, the job is
 *   queued to worker threads, the game thread continues loading.
 * - The six functions that read the built arrays (every reader of car+0xBA8):
 *   0x4E3890, 0x4E38D0, 0x4E3940, 0x4E39D0, 0x4E3A10, 0x4E3A50. Each first waits
 *   until all queued jobs are done.
 * - Worker threads copy the game thread's x87 control word per job.
 * - Other speed-ups: fast gzgets, libdeflate inflate, CPU speed check shortcut,
 *   vehgen.scn content cache, and optional texture caches (TexCache, MipCache,
 *   FileCache; off by default, size capped by CacheMaxMB).
 *
 * Entry: the Lightloader protected host export rfe_1d3b7a91 (same contract as
 * esc_pause_menu). DllMain does nothing. Hooks are installed inside the export,
 * which Lightloader calls from its plug-in pass (see the mod README, timing).
 *
 * Settings: fast_loading.ini beside the DLL, section [fastload]. Log:
 * fast_loading_log.txt beside the DLL (bounded). Caches live under
 * %LOCALAPPDATA%\Lightloader\fast_loading.
 *
 * Build: releases/v1.0.0-rc.1/build.bat (gcc -Wall -Wextra -Werror, static libdeflate).
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <stdint.h>
#include <string.h>
#include "protected_plugin_abi.h"
#include <tlhelp32.h>

/* ------------------------------------------------------------ config/log */
#define FL_MOD_ID "fast_loading"
#define FL_LOG_NAME "fast_loading_log.txt"
#define FL_LOG_PREV "fast_loading_log_prev.txt"
#define FL_LOG_MAX_BYTES (512L * 1024L)
#define FL_INI_NAME "fast_loading.ini"
#define FL_PATH_MAX 1024     /* path buffers */
#define FL_BIG 4096          /* vehgen.scn path buffers (no format truncation) */

static int g_enabled = 1, g_threads;
static uint64_t g_cache_cap = 1024ULL * 1024ULL * 1024ULL;   /* CacheMaxMB, bytes */
static char g_logpath[FL_PATH_MAX];
static char g_inipath[FL_PATH_MAX];
static CRITICAL_SECTION g_logcs;
static LARGE_INTEGER g_freq;
static long g_logbytes;
static int g_logfull;

static uint64_t now(void) { LARGE_INTEGER q; QueryPerformanceCounter(&q); return (uint64_t)q.QuadPart; }
static unsigned long us(uint64_t t) { return (unsigned long)(t * 1000000ULL / (uint64_t)g_freq.QuadPart); }

/* out = a + b, refused (out cleared, 0 returned) when it does not fit */
static int fl_path_cat(char *out, size_t cap, const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    if (!cap) return 0;
    if (la + lb + 1 > cap) { out[0] = '\0'; return 0; }
    memcpy(out, a, la);
    memcpy(out + la, b, lb + 1);
    return 1;
}

/* Bounded log: at most FL_LOG_MAX_BYTES per session. Written only from the
 * init path, the join path and the worker-side counters' reports. */
static void logf_(const char *fmt, ...)
{
    char line[600];
    int n;
    va_list ap;
    if (!g_logpath[0]) return;
    EnterCriticalSection(&g_logcs);
    if (!g_logfull) {
        FILE *f;
        va_start(ap, fmt); n = vsnprintf(line, sizeof line, fmt, ap); va_end(ap);
        if (n < 0) n = 0;
        if (n > (int)sizeof line - 2) n = (int)sizeof line - 2;
        line[n] = '\0';
        if (g_logbytes + n + 1 > FL_LOG_MAX_BYTES) {
            g_logfull = 1;
            strcpy(line, "log size limit reached; further lines suppressed");
            n = (int)strlen(line);
        }
        f = fopen(g_logpath, "a");
        if (f) {
            fputs(line, f); fputc('\n', f); fclose(f);
            g_logbytes += n + 1;
        }
    }
    LeaveCriticalSection(&g_logcs);
}

/* ------------------------------------------------------------ hook table */
typedef struct { uint32_t va; int steal; unsigned char sig[9]; const char *name; } site_t;
static const site_t g_build = { 0x004DE610, 7, {0x83,0xEC,0x58,0x8B,0x44,0x24,0x60}, "build_4DE610" };
#define N_READERS 6
static const site_t g_readers[N_READERS] = {
    { 0x004E3890, 9, {0x53,0x8B,0xD9,0x8B,0x83,0xA8,0x0B,0x00,0x00}, "StateArrayReset_4E3890" },
    { 0x004E38D0, 9, {0x56,0x8B,0xF1,0x8B,0x86,0xA8,0x0B,0x00,0x00}, "clear_4E38D0" },
    { 0x004E3940, 7, {0x83,0xEC,0x18,0x8B,0x44,0x24,0x20},           "RepeatedPosOffset_4E3940" },
    { 0x004E39D0, 9, {0x53,0x8B,0xD9,0x8B,0x83,0xA8,0x0B,0x00,0x00}, "iter_4E39D0" },
    { 0x004E3A10, 9, {0x53,0x8B,0xD9,0x8B,0x83,0xA8,0x0B,0x00,0x00}, "iter_4E3A10" },
    { 0x004E3A50, 9, {0x53,0x8B,0xD9,0x8B,0x83,0xA8,0x0B,0x00,0x00}, "MultiObjectRemove_4E3A50" },
};

typedef void *(__attribute__((thiscall)) *build_fn)(void *self, uint32_t *a2, uint32_t *a3);
static build_fn g_orig;            /* trampoline to the original 0x4DE610 */
static unsigned char *g_exec;      /* trampolines + reader stubs */

/* ------------------------------------------------------------ job queue */
typedef struct job {
    struct job *next, *all;
    uint32_t *self, *a3;
    uint32_t a2[5];                /* deep copy: f, n1, p1 (20 B each), n2, p2 (24 B each) */
    uint16_t cw;                   /* game thread x87 control word */
    uint32_t *scratch;             /* Verify: vanilla synchronous result */
    uint64_t queued, start, end;
    int tid;
} job_t;

static CRITICAL_SECTION g_qcs;
static HANDLE g_qsem, g_done;      /* g_done: manual reset, set when pending == 0 */
static job_t *g_head, *g_tail, *g_alljobs;
static volatile LONG g_pending;
static int g_started, g_njobs, g_joins;
static uint64_t g_first_q, g_last_end, g_wait, g_work;

static uint16_t get_cw(void) { uint16_t cw; __asm__ volatile("fnstcw %0" : "=m"(cw)); return cw; }
static void set_cw(uint16_t cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

static DWORD WINAPI worker(LPVOID p)
{
    int tid = (int)(size_t)p;
    for (;;) {
        job_t *j;
        WaitForSingleObject(g_qsem, INFINITE);
        EnterCriticalSection(&g_qcs);
        j = g_head;
        if (j) { g_head = j->next; if (!g_head) g_tail = NULL; }
        LeaveCriticalSection(&g_qcs);
        if (!j) continue;
        set_cw(j->cw);
        j->tid = tid;
        j->start = now();
        g_orig(j->self, j->a2, j->a3);
        j->end = now();
        if (InterlockedDecrement(&g_pending) == 0) SetEvent(g_done);
    }
    return 0;
}

static void start_pool(void)
{
    int i;
    if (g_started) return;
    g_started = 1;
    for (i = 0; i < g_threads; i++) {
        HANDLE h = CreateThread(NULL, 256 * 1024, worker, (LPVOID)(size_t)i, 0, NULL);
        if (h) CloseHandle(h);
    }
    logf_("worker pool started: %d threads", g_threads);
}

static void *copy_arr(uint32_t p, uint32_t n, uint32_t sz)
{
    void *d;
    /* count 0: the game never reads the array; return NULL so we never
     * HeapFree a game CRT block (crash 2026-10-20, pid 23868, ntdll c0000005) */
    if (!p || (int)n <= 0) return NULL;
    d = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * sz);
    if (d) memcpy(d, (void *)(size_t)p, (size_t)n * sz);
    return d;
}

#ifdef FASTLOAD_DEV_TOOLS
/* Compare a worker result with the vanilla result. Pointers into the mesh are
 * equal in both; pointers into the object's own arrays are compared as
 * (part, offset). Returns 0 when identical. DEV TOOLS ONLY (not in release). */
static int compare(const uint32_t *a, const uint32_t *b, char *why, int whylen)
{
    uint32_t i, k;
    if (a[1] != b[1] || a[2] != b[2] || a[4] != b[4] || a[6] != b[6]) {
        snprintf(why, whylen, "header %X/%X %u/%u %u/%u", a[1], b[1], a[2], b[2], a[4], b[4]);
        return 1;
    }
    for (i = 0; i < a[2]; i++) {
        const uint32_t *pa = (const uint32_t *)(size_t)a[3] + 3 * i, *pb = (const uint32_t *)(size_t)b[3] + 3 * i;
        if (pa[0] != pb[0] || pa[1] != pb[1]) { snprintf(why, whylen, "part %u head", i); return 1; }
        if (memcmp((void *)(size_t)pa[2], (void *)(size_t)pb[2], (size_t)pa[1] * 64)) {
            snprintf(why, whylen, "part %u records (%u x 64 B)", i, pa[1]); return 1;
        }
    }
    for (k = 0; k < a[4]; k++) {
        const uint32_t *ea = (const uint32_t *)(size_t)a[5] + 6 * k, *eb = (const uint32_t *)(size_t)b[5] + 6 * k;
        uint32_t n;
        if (memcmp(ea, eb, 16) || ea[4] != eb[4]) { snprintf(why, whylen, "sphere %u head", k); return 1; }
        for (n = 0; n < ea[4]; n++) {
            uint32_t va = ((uint32_t *)(size_t)ea[5])[n], vb = ((uint32_t *)(size_t)eb[5])[n];
            int found = 0;
            for (i = 0; i < a[2] && !found; i++) {
                const uint32_t *pa = (const uint32_t *)(size_t)a[3] + 3 * i, *pb = (const uint32_t *)(size_t)b[3] + 3 * i;
                if (va >= pa[2] && va < pa[2] + pa[1] * 64) {
                    if (vb < pb[2] || vb - pb[2] != va - pa[2]) { snprintf(why, whylen, "sphere %u ref %u", k, n); return 1; }
                    found = 1;
                }
            }
            if (!found) { snprintf(why, whylen, "sphere %u ref %u outside parts", k, n); return 1; }
        }
    }
    return 0;
}
#endif /* FASTLOAD_DEV_TOOLS */

/* called by every reader before it touches the arrays */
void fl_scn_report(void);
void fl_stats_report(void);
void __cdecl fl_join(void)
{
    uint64_t t0;
    job_t *j;
    int n = 0;
#ifdef FASTLOAD_DEV_TOOLS
    int pass = 0, fail = 0;
#endif
    if (g_pending == 0 && !g_alljobs) return;
    t0 = now();
    WaitForSingleObject(g_done, INFINITE);
    g_wait = now() - t0;
    g_joins++;
    /* report and free the finished batch */
    EnterCriticalSection(&g_qcs);
    j = g_alljobs; g_alljobs = NULL;
    LeaveCriticalSection(&g_qcs);
    g_work = 0; g_last_end = 0;
    {
        job_t *x;
        for (x = j; x; x = x->all) { g_work += x->end - x->start; if (x->end > g_last_end) g_last_end = x->end; n++; }
    }
    logf_("join #%d: %d jobs, wall first-queue -> last-done %lu us, worker CPU sum %lu us, game thread waited %lu us",
          g_joins, n, us(g_last_end - g_first_q), us(g_work), us(g_wait));
    while (j) {
        job_t *nx = j->all;
#ifdef FASTLOAD_DEV_TOOLS
        if (j->scratch) {
            char why[96];
            if (compare(j->self, j->scratch, why, sizeof why)) { fail++; logf_("  VERIFY FAIL obj %p: %s", (void *)j->self, why); }
            else pass++;
        }
        logf_("  job obj %p thread %d queued+%lu us run %lu us", (void *)j->self, j->tid,
              us(j->start - j->queued), us(j->end - j->start));
#endif
        if (j->a2[2]) HeapFree(GetProcessHeap(), 0, (void *)(size_t)j->a2[2]);
        if (j->a2[4]) HeapFree(GetProcessHeap(), 0, (void *)(size_t)j->a2[4]);
        HeapFree(GetProcessHeap(), 0, j);
        j = nx;
    }
#ifdef FASTLOAD_DEV_TOOLS
    logf_("  VERIFY: %d PASS, %d FAIL", pass, fail);
#endif
    fl_scn_report();
    fl_stats_report();
    g_first_q = 0;
}

/* replacement for 0x4DE610 (thiscall, 2 stack args, callee pops) */
static void *__attribute__((thiscall)) hook_build(uint32_t *self, uint32_t *a2, uint32_t *a3)
{
    job_t *j = (job_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *j);
    if (!j || g_threads <= 0) {             /* no pool: run vanilla here */
        if (j) HeapFree(GetProcessHeap(), 0, j);
        return g_orig(self, a2, a3);
    }
    j->self = self; j->a3 = a3; j->cw = get_cw();
    j->a2[0] = a2[0]; j->a2[1] = a2[1]; j->a2[3] = a2[3];
    j->a2[2] = (uint32_t)(size_t)copy_arr(a2[2], a2[1], 20);
    j->a2[4] = (uint32_t)(size_t)copy_arr(a2[4], a2[3], 24);
    if ((a2[2] && (int)a2[1] > 0 && !j->a2[2]) || (a2[4] && (int)a2[3] > 0 && !j->a2[4])) {
        if (j->a2[2]) HeapFree(GetProcessHeap(), 0, (void *)(size_t)j->a2[2]);
        if (j->a2[4]) HeapFree(GetProcessHeap(), 0, (void *)(size_t)j->a2[4]);
        HeapFree(GetProcessHeap(), 0, j);
        return g_orig(self, a2, a3);
    }
#ifdef FASTLOAD_DEV_TOOLS
    if (g_verify) {
        j->scratch = (uint32_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 64);
        if (j->scratch) g_orig(j->scratch, a2, a3);
    }
#endif
    start_pool();
    j->queued = now();
    EnterCriticalSection(&g_qcs);
    if (!g_first_q) g_first_q = j->queued;
    if (InterlockedIncrement(&g_pending) == 1) ResetEvent(g_done);
    j->all = g_alljobs; g_alljobs = j;
    if (g_tail) g_tail->next = j; else g_head = j;
    g_tail = j;
    g_njobs++;
    LeaveCriticalSection(&g_qcs);
    ReleaseSemaphore(g_qsem, 1, NULL);
    return self;
}

/* ------------------------------------------------- vehgen.scn content cache
 * The game writes LOG\vehgen.scn for every car and reads it straight back.
 * Windows Defender scans a freshly modified file on open: ~4.5 ms per read
 * open (measured offline, 26 opens = 117-188 ms). An unchanged file opens in
 * ~0.01 ms. So: capture the bytes the game writes (exe IAT WriteFile), keep
 * one copy per content hash in %TEMP%\f1c_fastload\scn_cache, and redirect the
 * next read open of vehgen.scn to that unchanged copy. Identical bytes, so the
 * game parses the same content. Any seek/truncate on the write handle or any
 * failure = no redirect (vanilla path). */
typedef HANDLE (WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL (WINAPI *WriteFile_t)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL (WINAPI *CloseHandle_t)(HANDLE);
typedef DWORD (WINAPI *SetFilePointer_t)(HANDLE, LONG, PLONG, DWORD);
typedef BOOL (WINAPI *SetEndOfFile_t)(HANDLE);
static CreateFileA_t r_CreateFileA;
static WriteFile_t r_WriteFile;
static CloseHandle_t r_CloseHandle;
static SetFilePointer_t r_SetFilePointer;
static SetEndOfFile_t r_SetEndOfFile;
static CRITICAL_SECTION g_scs;
static HANDLE g_wh = INVALID_HANDLE_VALUE;      /* game's open write handle on vehgen.scn */
static unsigned char *g_wbuf; static DWORD g_wlen, g_wcap; static int g_wbad;
static char g_cachedir[FL_PATH_MAX], g_current[FL_BIG];  /* cache file holding the last written content */
static int g_scn_hits, g_scn_new, g_scn_skip;

static int is_vehgen(LPCSTR n)
{
    size_t l = n ? strlen(n) : 0;
    return l >= 10 && !_stricmp(n + l - 10, "vehgen.scn") && (l == 10 || n[l - 11] == '\\' || n[l - 11] == '/');
}

static HANDLE WINAPI my_CreateFileA(LPCSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
{
    HANDLE h;
    if (!is_vehgen(n)) return r_CreateFileA(n, a, s, sa, d, f, t);
    EnterCriticalSection(&g_scs);
    if (!(a & GENERIC_WRITE) && g_current[0]) {
        h = r_CreateFileA(g_current, a, s, sa, d, f, t);
        if (h != INVALID_HANDLE_VALUE) { g_scn_hits++; LeaveCriticalSection(&g_scs); return h; }
    }
    LeaveCriticalSection(&g_scs);
    h = r_CreateFileA(n, a, s, sa, d, f, t);
    EnterCriticalSection(&g_scs);
    if (a & GENERIC_WRITE) {
        g_current[0] = 0;
        g_wh = h; g_wlen = 0; g_wbad = (h == INVALID_HANDLE_VALUE);
    }
    LeaveCriticalSection(&g_scs);
    return h;
}

static BOOL WINAPI my_WriteFile(HANDLE h, LPCVOID b, DWORD n, LPDWORD w, LPOVERLAPPED o)
{
    BOOL r = r_WriteFile(h, b, n, w, o);
    if (h == g_wh) {
        EnterCriticalSection(&g_scs);
        if (h == g_wh && !g_wbad) {
            DWORD got = (r && w) ? *w : 0;
            if (!r || o || got != n) g_wbad = 1;
            else {
                if (g_wlen + got > g_wcap) {
                    DWORD nc = (g_wlen + got) * 2 + 4096;
                    unsigned char *nb = (unsigned char *)(g_wbuf ? HeapReAlloc(GetProcessHeap(), 0, g_wbuf, nc) : HeapAlloc(GetProcessHeap(), 0, nc));
                    if (!nb) g_wbad = 1; else { g_wbuf = nb; g_wcap = nc; }
                }
                if (!g_wbad) { memcpy(g_wbuf + g_wlen, b, got); g_wlen += got; }
            }
        }
        LeaveCriticalSection(&g_scs);
    }
    return r;
}

static DWORD WINAPI my_SetFilePointer(HANDLE h, LONG d, PLONG hi, DWORD m)
{
    if (h == g_wh) {
        /* CRT may query the position (0, FILE_CURRENT); anything else = no redirect */
        if (!(d == 0 && (!hi || *hi == 0) && m == FILE_CURRENT)) { EnterCriticalSection(&g_scs); if (h == g_wh) g_wbad = 1; LeaveCriticalSection(&g_scs); }
    }
    return r_SetFilePointer(h, d, hi, m);
}

static BOOL WINAPI my_SetEndOfFile(HANDLE h)
{
    if (h == g_wh) { EnterCriticalSection(&g_scs); if (h == g_wh) g_wbad = 1; LeaveCriticalSection(&g_scs); }
    return r_SetEndOfFile(h);
}

static BOOL WINAPI my_CloseHandle(HANDLE h)
{
    BOOL r = r_CloseHandle(h);
    if (h == g_wh) {
        EnterCriticalSection(&g_scs);
        if (h == g_wh) {
            g_wh = INVALID_HANDLE_VALUE;
            if (!g_wbad && r) {
                uint64_t x = 1469598103934665603ULL;
                DWORD i;
                char path[FL_BIG];
                for (i = 0; i < g_wlen; i++) { x ^= g_wbuf[i]; x *= 1099511628211ULL; }
                snprintf(path, sizeof path, "%s\\vehgen_%08lx%08lx_%lu.scn", g_cachedir,
                         (unsigned long)(x >> 32), (unsigned long)x, (unsigned long)g_wlen);
                if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) { strcpy(g_current, path); }
                else {
                    char tmp[FL_BIG + 64];
                    HANDLE c;
                    DWORD wr = 0;
                    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, GetCurrentProcessId());
                    c = r_CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (c != INVALID_HANDLE_VALUE) {
                        BOOL ok = r_WriteFile(c, g_wbuf, g_wlen, &wr, NULL) && wr == g_wlen;
                        r_CloseHandle(c);
                        if (ok && MoveFileExA(tmp, path, 0)) { strcpy(g_current, path); g_scn_new++; }
                        else DeleteFileA(tmp);   /* our own temp file */
                    }
                }
            } else g_scn_skip++;
        }
        LeaveCriticalSection(&g_scs);
    }
    return r;
}

static int iat_patch(HMODULE mod, const char *dll, const char *fn, void *newfn, void **oldfn)
{
    unsigned char *b = (unsigned char *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    if (!rva) return 0;
    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(b + rva); imp->Name; imp++) {
        IMAGE_THUNK_DATA *o, *c;
        if (_stricmp((const char *)(b + imp->Name), dll) || !imp->OriginalFirstThunk) continue;
        o = (IMAGE_THUNK_DATA *)(b + imp->OriginalFirstThunk);
        c = (IMAGE_THUNK_DATA *)(b + imp->FirstThunk);
        for (; o->u1.AddressOfData; o++, c++) {
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL(o->u1.Ordinal)) continue;
            if (strcmp((const char *)((IMAGE_IMPORT_BY_NAME *)(b + o->u1.AddressOfData))->Name, fn)) continue;
            if (!VirtualProtect(&c->u1.Function, 4, PAGE_EXECUTE_READWRITE, &old)) return 0;
            *oldfn = (void *)(size_t)c->u1.Function;
            c->u1.Function = (size_t)newfn;
            VirtualProtect(&c->u1.Function, 4, old, &old);
            return 1;
        }
    }
    return 0;
}

/* Cache base: %LOCALAPPDATA%\Lightloader\fast_loading (created). Returns 0 when
 * LOCALAPPDATA is missing or the folder cannot be created (caches then stay off). */
static int fl_base_dir(char *out, size_t cap)
{
    char root[FL_PATH_MAX], parent[FL_PATH_MAX];
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", root, sizeof root);
    if (!n || n >= sizeof root) return 0;
    fl_path_cat(parent, sizeof parent, root, "\\Lightloader");
    fl_path_cat(out, cap, parent, "\\" FL_MOD_ID);
    CreateDirectoryA(parent, NULL);
    CreateDirectoryA(out, NULL);
    return GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
}

static int install_scn_cache(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    int n = 0;
    char base[FL_PATH_MAX];
    if (!fl_base_dir(base, sizeof base)) return 0;
    InitializeCriticalSection(&g_scs);
    fl_path_cat(g_cachedir, sizeof g_cachedir, base, "\\scn_cache");
    CreateDirectoryA(g_cachedir, NULL);
    /* resolve the originals first so a partial patch can never call NULL */
    r_CreateFileA = CreateFileA; r_WriteFile = WriteFile; r_CloseHandle = CloseHandle;
    r_SetFilePointer = SetFilePointer; r_SetEndOfFile = SetEndOfFile;
    n += iat_patch(exe, "kernel32.dll", "SetFilePointer", (void *)my_SetFilePointer, (void **)&r_SetFilePointer);
    n += iat_patch(exe, "kernel32.dll", "SetEndOfFile", (void *)my_SetEndOfFile, (void **)&r_SetEndOfFile);
    n += iat_patch(exe, "kernel32.dll", "WriteFile", (void *)my_WriteFile, (void **)&r_WriteFile);
    n += iat_patch(exe, "kernel32.dll", "CloseHandle", (void *)my_CloseHandle, (void **)&r_CloseHandle);
    n += iat_patch(exe, "kernel32.dll", "CreateFileA", (void *)my_CreateFileA, (void **)&r_CreateFileA);
    return n;
}

void fl_scn_report(void)
{
    logf_("scn cache: %d reads redirected to cached copy, %d new cache files, %d writes not cacheable",
          g_scn_hits, g_scn_new, g_scn_skip);
}

/* ------------------------------------------------------------ fast gzgets
 * 0x5351D0 is zlib 1.1.x gzgets: one gzread(1) per byte (3.4 M calls a load).
 * For an uncompressed ("transparent") stream gzread(1) serves a byte from
 * inbuf, or when inbuf is empty freads ONE byte straight from the FILE.
 * Replacement for transparent streams: same loop, but when inbuf is empty it
 * refills inbuf with fread(inbuf, 1, 16384) (what the compressed path does).
 * Later gzread/gzseek/gzrewind/gztell all honour avail_in/total_out, so every
 * reader sees the same bytes. Compressed streams take the original code.
 * gz_stream layout (1.1.x, matches 0x534F90): +0 next_in, +4 avail_in,
 * +8 total_in, +12 next_out, +16 avail_out, +20 total_out, +56 z_err,
 * +60 z_eof, +64 FILE*, +68 inbuf, +88 transparent, +92 mode. */
typedef struct {
    unsigned char *next_in; uint32_t avail_in, total_in;
    unsigned char *next_out; uint32_t avail_out, total_out;
    uint32_t z_rest[8];
    int z_err, z_eof; void *file; unsigned char *inbuf;
    uint32_t pad[4];
    int transparent; char mode;
} gz_t;
_Static_assert(__builtin_offsetof(gz_t, z_err) == 56 && __builtin_offsetof(gz_t, inbuf) == 68 && __builtin_offsetof(gz_t, transparent) == 88 && __builtin_offsetof(gz_t, mode) == 92, "gz_stream layout");
static int site_ok(const site_t *s);
static unsigned char *make_tramp(unsigned char *tr, const site_t *s);
static void write_jmp(uint32_t at, void *to, int steal);
typedef char *(__attribute__((fastcall)) *gzgets_fn)(gz_t *, char *, int);
typedef unsigned (__cdecl *fread_fn)(void *, unsigned, unsigned, void *);
static gzgets_fn g_gzgets_orig;
static const fread_fn g_fread = (fread_fn)0x0058803F;   /* game CRT fread */
static const site_t g_gzgets_site = { 0x005351D0, 5, {0x53,0x55,0x56,0x8B,0xF2}, "gzgets_5351D0" };
static const unsigned char g_fread_sig[7] = {0x55,0x8B,0xEC,0x56,0xFF,0x75,0x14};   /* checked at install */
static volatile LONG g_gz_fast, g_gz_slow, g_gz_refill;

static char *__attribute__((fastcall)) fl_gzgets(gz_t *s, char *buf, int len)
{
    char *p = buf;
    int v6 = len;
    if (!buf || len <= 0) return 0;
    if (!s || s->mode != 'r' || !s->transparent || s->z_err == -3 || s->z_err == -1 || s->z_err == 1 || !s->inbuf) {
        InterlockedIncrement(&g_gz_slow);
        return g_gzgets_orig(s, buf, len);
    }
    InterlockedIncrement(&g_gz_fast);
    for (;;) {
        unsigned char c;
        if (--v6 <= 0) break;
        if (s->avail_in == 0) {
            unsigned n = g_fread(s->inbuf, 1, 16384, s->file);
            InterlockedIncrement(&g_gz_refill);
            s->next_in = s->inbuf;
            s->avail_in = n;
            if (n == 0) {                  /* gzread(1) returned 0: z_eof */
                s->next_out = (unsigned char *)p; s->avail_out = 1;
                s->z_eof = 1;
                break;
            }
        }
        c = *s->next_in++;
        s->avail_in--;
        *p++ = (char)c;
        s->total_in++; s->total_out++;
        s->next_out = (unsigned char *)p; s->avail_out = 0;
        if (c == '\n') break;
    }
    *p = 0;
    if (p == buf && v6 > 0) return 0;
    return buf;
}

static int install_gzgets(void)
{
    unsigned char *tr;
    if (!site_ok(&g_gzgets_site) || memcmp((void *)0x0058803F, g_fread_sig, sizeof g_fread_sig)) return 0;
    tr = make_tramp(g_exec + 1024, &g_gzgets_site);
    g_gzgets_orig = (gzgets_fn)(void *)tr;
    write_jmp(g_gzgets_site.va, (void *)fl_gzgets, g_gzgets_site.steal);
    return 1;
}

/* ------------------------------------------------------- libdeflate uncompress
 * 0x59B260 is zlib 1.1.x uncompress(dest, &destLen, src, srcLen) (fastcall
 * ecx/edx + 2 stack args, ret 8): inflateInit, inflate(Z_FINISH), inflateEnd.
 * MAS one-shot entries: 2650 calls = 1.02 s per load. libdeflate decodes the
 * same zlib stream (adler32 checked) to the same bytes, faster. On success:
 * *destLen = bytes produced, return 0 (Z_OK), exactly like zlib. On ANY
 * libdeflate failure the original zlib code runs and decides (so all error
 * returns stay vanilla). Verify=1 also runs zlib into a scratch buffer and
 * compares length + bytes per call. */
#include <libdeflate.h>
typedef int (__attribute__((fastcall)) *uncompress_fn)(unsigned char *, uint32_t *, const unsigned char *, uint32_t);
static uncompress_fn g_unc_orig;
static const site_t g_unc_site = { 0x0059B260, 7, {0x83,0xEC,0x38,0x8B,0x44,0x24,0x3C}, "uncompress_59B260" };
static DWORD g_dtls = TLS_OUT_OF_INDEXES;
static volatile LONG g_unc_fast, g_unc_fallback;
#ifdef FASTLOAD_DEV_TOOLS
static volatile LONG g_unc_vpass, g_unc_vfail;
#endif

static int __attribute__((fastcall)) fl_uncompress(unsigned char *dst, uint32_t *dlen, const unsigned char *src, uint32_t slen)
{
    struct libdeflate_decompressor *d = (struct libdeflate_decompressor *)TlsGetValue(g_dtls);
    size_t in_used = 0, out = 0;
    if (!d) { d = libdeflate_alloc_decompressor(); if (d) TlsSetValue(g_dtls, d); }
    if (d && dst && dlen &&
        libdeflate_zlib_decompress_ex(d, src, slen, dst, *dlen, &in_used, &out) == LIBDEFLATE_SUCCESS) {
#ifdef FASTLOAD_DEV_TOOLS
        if (g_verify) {
            uint32_t vlen = *dlen;
            unsigned char *v = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, vlen ? vlen : 1);
            if (v) {
                int r = g_unc_orig(v, &vlen, src, slen);
                if (r == 0 && vlen == out && !memcmp(v, dst, out)) InterlockedIncrement(&g_unc_vpass);
                else { InterlockedIncrement(&g_unc_vfail); logf_("  INFLATE VERIFY FAIL: zlib r=%d len %u, libdeflate len %u", r, vlen, (unsigned)out); }
                HeapFree(GetProcessHeap(), 0, v);
            }
        }
#endif
        *dlen = (uint32_t)out;
        InterlockedIncrement(&g_unc_fast);
        return 0;
    }
    InterlockedIncrement(&g_unc_fallback);
    return g_unc_orig(dst, dlen, src, slen);
}

static int install_uncompress(void)
{
    if (!site_ok(&g_unc_site)) return 0;
    g_dtls = TlsAlloc();
    if (g_dtls == TLS_OUT_OF_INDEXES) return 0;
    g_unc_orig = (uncompress_fn)(void *)make_tramp(g_exec + 1088, &g_unc_site);
    write_jmp(g_unc_site.va, (void *)fl_uncompress, g_unc_site.steal);
    return 1;
}

/* ------------------------------------------------------ CPU speed check
 * sub_55F0D0 (called once from RenderUISystemGlobalInit) busy-waits until a
 * QPC timer reaches 0.25 s (fcomp [0x59EB50] at 0x55F1D0) and counts RDTSC
 * ticks, then MHz = ticks * 1e-6 * 4.0 (fmul [0x59E7D8] at 0x55F263). The MHz
 * is only compared with 400 and 1200 (feature flags). Measuring for 0.05 s
 * and multiplying by 20.0 gives the same MHz to well under 1 MHz on a modern
 * CPU. Only the two instruction operands are repointed (the 0.25 and 4.0
 * constants are shared with 20+ other places and stay untouched). Saves 0.2 s. */
static void q_patch(uint32_t at, const void *bytes, int n, uint32_t busy_lo, uint32_t busy_hi);
static const double g_cal_secs = 0.05, g_cal_mul = 20.0;
static int install_cpucal(void)
{
    static const unsigned char s1[6] = {0xDC,0x1D,0x50,0xEB,0x59,0x00}, s2[6] = {0xDC,0x0D,0xD8,0xE7,0x59,0x00};
    unsigned char *p1 = (unsigned char *)0x0055F1D0, *p2 = (unsigned char *)0x0055F263;
    uint32_t a1 = (uint32_t)(size_t)&g_cal_secs, a2 = (uint32_t)(size_t)&g_cal_mul;
    if (memcmp(p1, s1, 6) || memcmp(p2, s2, 6)) return 0;
    /* busy range 0x55F1C7..0x55F269: the timing loop up to and including the
     * fmul, so both operands always change together for one measurement */
    q_patch(0x0055F1D2, &a1, 4, 0x0055F1C7, 0x0055F269);
    q_patch(0x0055F265, &a2, 4, 0x0055F1C7, 0x0055F269);
    return 1;
}

#include "texcache.inc"

/* ------------------------------------------------------------ patching */
static int site_ok(const site_t *s)
{
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery((void *)(size_t)s->va, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT &&
           !memcmp((void *)(size_t)s->va, s->sig, s->steal);
}

/* Code patches are queued during install and written together by
 * apply_patches(): Lightloader calls the plug-in from its worker thread while
 * the game already runs, so every other thread of the process is suspended,
 * and the write only happens when no thread's EIP is inside a patched range
 * (or a range declared busy, e.g. the CPU check between its two operands).
 * No heap, CRT or logging calls while threads are suspended (lock safety). */
typedef struct { uint32_t at; unsigned char b[16]; int n; uint32_t busy_lo, busy_hi; int stack_check; } patch_t;
static patch_t g_pq[48];
static int g_npq;

static void q_patch(uint32_t at, const void *bytes, int n, uint32_t busy_lo, uint32_t busy_hi)
{
    if (g_npq >= (int)(sizeof g_pq / sizeof g_pq[0]) || n > 16) return;
    g_pq[g_npq].at = at; memcpy(g_pq[g_npq].b, bytes, n); g_pq[g_npq].n = n;
    g_pq[g_npq].busy_lo = busy_lo; g_pq[g_npq].busy_hi = busy_hi;
    g_pq[g_npq].stack_check = (busy_hi - busy_lo) > 16;   /* only wide code ranges (CPU check loop), never a function entry */
    g_npq++;
}

static void write_jmp(uint32_t at, void *to, int steal)
{
    unsigned char b[16];
    b[0] = 0xE9; *(int32_t *)(b + 1) = (int32_t)((uint32_t)(size_t)to - (at + 5));
    if (steal > 5) memset(b + 5, 0x90, steal - 5);
    q_patch(at, b, steal, at, at + steal);
}

/* returns attempts used (> 0) on success, 0 when it gave up (nothing written) */
static int apply_patches(void)
{
    static HANDLE th[512];
    int attempt, n, i, k, busy;
    DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    if (!g_npq) return 1;
    for (attempt = 1; attempt <= 200; attempt++) {
        HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 te;
        n = 0;
        if (sn == INVALID_HANDLE_VALUE) return 0;
        te.dwSize = sizeof te;
        if (Thread32First(sn, &te)) do {
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != self && n < 512) {
                HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
                if (h) th[n++] = h;
            }
        } while (Thread32Next(sn, &te));
        CloseHandle(sn);
        /* ---- no heap/CRT/log calls from here until the threads are resumed ---- */
        for (i = 0; i < n; i++) if (SuspendThread(th[i]) == (DWORD)-1) { CloseHandle(th[i]); th[i] = NULL; }
        busy = 0;
        for (i = 0; i < n && !busy; i++) {
            CONTEXT c;
            if (!th[i]) continue;
            c.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(th[i], &c)) { busy = 1; break; }
            for (k = 0; k < g_npq && !busy; k++)
                if (c.Eip >= g_pq[k].busy_lo && c.Eip < g_pq[k].busy_hi) busy = 1;
            /* also a busy range as a return address in the top 256 stack slots: a thread
             * that is inside a callee of a busy range (e.g. the CPU check timer) */
            if (!busy) {
                MEMORY_BASIC_INFORMATION mbi;
                if (VirtualQuery((void *)(size_t)c.Esp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
                    const uint32_t *s = (const uint32_t *)(size_t)c.Esp;
                    uint32_t lim = (uint32_t)((size_t)mbi.BaseAddress + mbi.RegionSize), w;
                    for (w = 0; w < 256 && !busy && (uint32_t)(size_t)(s + w + 1) <= lim; w++)
                        for (k = 0; k < g_npq; k++)
                            if (g_pq[k].stack_check && s[w] >= g_pq[k].busy_lo && s[w] < g_pq[k].busy_hi) { busy = 1; break; }
                }
            }
        }
        if (!busy) {
            for (k = 0; k < g_npq; k++) {
                DWORD old;
                unsigned char *t = (unsigned char *)(size_t)g_pq[k].at;
                if (VirtualProtect(t, g_pq[k].n, PAGE_EXECUTE_READWRITE, &old)) {
                    memcpy(t, g_pq[k].b, g_pq[k].n);
                    VirtualProtect(t, g_pq[k].n, old, &old);
                    FlushInstructionCache(GetCurrentProcess(), t, g_pq[k].n);
                }
            }
        }
        for (i = 0; i < n; i++) if (th[i]) { ResumeThread(th[i]); CloseHandle(th[i]); }
        /* ---- threads running again ---- */
        if (!busy) return attempt;
        Sleep(2);
    }
    return 0;
}

static unsigned char *make_tramp(unsigned char *tr, const site_t *s)
{
    memcpy(tr, (void *)(size_t)s->va, s->steal);
    tr[s->steal] = 0xE9;
    *(int32_t *)(tr + s->steal + 1) = (int32_t)((s->va + s->steal) - ((uint32_t)(size_t)tr + s->steal + 5));
    return tr;
}

/* all-or-nothing: every site is checked before anything is written */
static int install(void)
{
    int i;
    if (!site_ok(&g_build)) { logf_("SKIP: %s bytes differ (other exe or foreign hook)", g_build.name); return 0; }
    for (i = 0; i < N_READERS; i++)
        if (!site_ok(&g_readers[i])) { logf_("SKIP: %s bytes differ", g_readers[i].name); return 0; }
    g_exec = (unsigned char *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_exec) return 0;
    g_orig = (build_fn)(void *)make_tramp(g_exec, &g_build);
    for (i = 0; i < N_READERS; i++) {
        unsigned char *tr = make_tramp(g_exec + 64 + i * 32, &g_readers[i]);
        unsigned char *st = g_exec + 512 + i * 32;
        /* pushad; pushfd; call fl_join; popfd; popad; jmp trampoline */
        st[0] = 0x60; st[1] = 0x9C;
        st[2] = 0xE8; *(int32_t *)(st + 3) = (int32_t)((uint32_t)(size_t)fl_join - ((uint32_t)(size_t)st + 7));
        st[7] = 0x9D; st[8] = 0x61;
        st[9] = 0xE9; *(int32_t *)(st + 10) = (int32_t)((uint32_t)(size_t)tr - ((uint32_t)(size_t)st + 14));
    }
    for (i = 0; i < N_READERS; i++) write_jmp(g_readers[i].va, g_exec + 512 + i * 32, g_readers[i].steal);
    write_jmp(g_build.va, (void *)hook_build, g_build.steal);
    return 1;
}

/* ------------------------------------------------------ stats report
 * Runs at every join (once per race load). Counters are cumulative. */
void fl_stats_report(void)
{
    logf_("gzgets: fast %ld, original %ld, inbuf refills %ld", g_gz_fast, g_gz_slow, g_gz_refill);
    logf_("uncompress: libdeflate %ld, zlib fallback %ld", g_unc_fast, g_unc_fallback);
#ifdef FASTLOAD_DEV_TOOLS
    logf_("uncompress verify: pass %ld fail %ld", g_unc_vpass, g_unc_vfail);
#endif
    texcache_report();
    mipcache_report();
    filecache_report();
}

/* ------------------------------------------------------ settings (INI)
 * [fastload] in fast_loading.ini beside the DLL. A missing or invalid value
 * gives the default. Flags accept only 0 or 1. */
static const char *ini_value(const char *key, char *buf, size_t cap)
{
    char *s = buf, *e;
    buf[0] = '\0';
    GetPrivateProfileStringA("fastload", key, "", buf, (DWORD)cap, g_inipath);
    while (*s == ' ' || *s == '\t') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = '\0';
    return s;
}

static int ini_flag(const char *key, int def)
{
    char b[32];
    const char *s = ini_value(key, b, sizeof b);
    if (!strcmp(s, "0")) return 0;
    if (!strcmp(s, "1")) return 1;
    return def;
}

static int ini_int(const char *key, int def, int min, int max)
{
    char b[32];
    char *e = NULL;
    const char *s = ini_value(key, b, sizeof b);
    long v;
    if (!*s) return def;
    v = strtol(s, &e, 10);
    if (e == s || *e != '\0' || v < min || v > max) return def;
    return (int)v;
}

/* ------------------------------------------------------ init
 * Called from the rfe_ export after the host check. Installs every hook
 * synchronously (no thread, no crypto, no network). */

static void fl_init(HMODULE self)
{
    SYSTEM_INFO si;
    char dir[FL_PATH_MAX], prev[FL_PATH_MAX], *s;
    uint64_t t0;
    int cap_mb, want_tc, want_mip, want_fm;

    QueryPerformanceFrequency(&g_freq);
    t0 = now();
    InitializeCriticalSection(&g_logcs);
    InitializeCriticalSection(&g_qcs);
    GetModuleFileNameA(self, dir, sizeof dir);
    s = strrchr(dir, '\\'); if (s) s[1] = 0;
    fl_path_cat(g_inipath, sizeof g_inipath, dir, FL_INI_NAME);
    fl_path_cat(g_logpath, sizeof g_logpath, dir, FL_LOG_NAME);
    fl_path_cat(prev, sizeof prev, dir, FL_LOG_PREV);
    /* keep the previous session's log as *_prev.txt (replaced each session) */
    if (GetFileAttributesA(g_logpath) != INVALID_FILE_ATTRIBUTES)
        MoveFileExA(g_logpath, prev, MOVEFILE_REPLACE_EXISTING);

    g_enabled = ini_flag("Enabled", 1);
    g_threads = ini_int("Threads", 0, 0, 32);
    cap_mb = ini_int("CacheMaxMB", 1024, 1, 3072);
    g_cache_cap = (uint64_t)cap_mb * 1024ULL * 1024ULL;
    GetSystemInfo(&si);
    if (g_threads == 0) g_threads = (int)si.dwNumberOfProcessors - 1;
    if (g_threads < 0) g_threads = 0;
    if (g_threads > 32) g_threads = 32;
    logf_("--- fast_loading 1.0.0 loaded, pid %lu, cores %lu, threads %d, enabled %d, cache cap %d MB ---",
          GetCurrentProcessId(), (unsigned long)si.dwNumberOfProcessors, g_threads, g_enabled, cap_mb);
    if (!g_enabled) { logf_("disabled in fast_loading.ini (Enabled=0): game unchanged"); return; }

    g_qsem = CreateSemaphoreA(NULL, 0, 0x7FFFFFFF, NULL);
    g_done = CreateEventA(NULL, TRUE, TRUE, NULL);
    if (!g_qsem || !g_done) { logf_("init failed: could not create worker objects"); return; }
    if (install()) logf_("armed: build 0x4DE610 + %d reader joins", N_READERS);
    else logf_("not armed (game bytes differ from the expected build); vanilla loading");

    if (ini_flag("ScnCache", 1))
        logf_("vehgen.scn cache: %d/5 IAT hooks", install_scn_cache());
    if (g_exec && ini_flag("FastGets", 1))
        logf_(install_gzgets() ? "fast gzgets: armed" : "fast gzgets: skipped (bytes differ)");
    if (g_exec && ini_flag("FastInflate", 1))
        logf_(install_uncompress() ? "libdeflate inflate: armed" : "libdeflate inflate: skipped");
    want_tc = ini_flag("TexCache", 0);
    want_mip = ini_flag("MipCache", 0);
    want_fm = ini_flag("FileCache", 0);
    if (g_exec && (want_tc || want_mip || want_fm)) tc_ready = tc_open();
    if (g_exec && want_tc)
        logf_(install_texcache() ? "texture cache: armed" : "texture cache: skipped");
    if (g_exec && want_mip)
        logf_(install_mipcache() ? "mip cache: armed" : "mip cache: skipped");
    if (g_exec && want_fm)
        logf_(install_filecache() ? "decode cache: armed" : "decode cache: skipped");
    if (ini_flag("FastCpuCheck", 1))
        logf_(install_cpucal() ? "CPU speed check: 0.25 s -> 0.05 s" : "CPU speed check: skipped (bytes differ)");
#ifdef FASTLOAD_DEV_TOOLS
    g_verify = ini_flag("Verify", 0);
    logf_("DEV build: verify %d", g_verify);
#endif
    {
        int queued = g_npq, a = apply_patches();
        if (a) logf_("code patches written: %d (attempt %d, other threads paused while writing)", queued, a);
        else logf_("code patches NOT written (a thread stayed inside a patch site): loading stays vanilla");
    }
    logf_("init done in %lu us", us(now() - t0));
}

/* ------------------------------------------------------ Lightloader host entry
 * Same contract as esc_pause_menu 1.0.5: only the Lightloader protected host
 * may call this export. Anything else gets FALSE and nothing is changed.
 * DllMain is inert. */
static volatile LONG g_loader_init_state;

static __attribute__((noinline)) BOOL loader_policy_matches(const LightLoaderProtectedHostV2 *host)
{
    return host->abi_version >= LLP_ABI_VERSION &&
        host->product_id == LLP_PRODUCT_ID &&
        (host->capabilities & (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2)) ==
            (LLP_CAP_EXCLUSIVE_HOST | LLP_CAP_HUB_2) &&
        host->compatibility_generation >= LLP_HUB_2_GENERATION;
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

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}

__declspec(dllexport) const char *F1CModId(void) { return FL_MOD_ID; }

__declspec(dllexport) BOOL __cdecl rfe_1d3b7a91(const LightLoaderProtectedHostV2 *host)
{
    HMODULE self = NULL;
    if (InterlockedCompareExchange(&g_loader_init_state, 1, 0) != 0)
        return FALSE;
    if (!validate_loader_host(host)) {
        InterlockedExchange(&g_loader_init_state, 0);
        return FALSE;
    }
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)(void *)fl_init, &self);
    fl_init(self);
    return TRUE;
}
