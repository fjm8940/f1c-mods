/* f1c_usage.h - shared per-session usage-summary core for first-party mods.
 *
 * Owner 2026-10-08 (audit close-out U1): the common parts of the Mechanical
 * Failures summary (mf_usage.h) and the Safety Car summary (sc_usage.h) live
 * here once. A mod keeps its own tap/sanitize/noise and its own JSON middle
 * fields; this header owns the envelope, the file writer, the caps, the
 * reports directory, the lock and the crash marker.
 *
 * The DLL never sends anything. Each mod writes ONE small JSON file per
 * finished session to <game>\plugins\lightloader\reports\usage\<mod_id>-<utc>-<hex>.json
 * and Lightloader uploads it only when the player's "Send crash, bug and mod
 * usage reports automatically" switch is on.
 *
 * Header-only, pure C, static helpers: no globals of its own, so two mods in
 * one image cannot collide. Include it after <windows.h>/<stdio.h>/<string.h>.
 */
#ifndef F1C_USAGE_H
#define F1C_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <tlhelp32.h>

/* caps (owner contract): head/tail lines kept, per-line cut, file count and
 * per-file size cap. */
#define F1C_USAGE_HEAD      60
#define F1C_USAGE_TAIL      90
#define F1C_USAGE_LINE      160
#define F1C_USAGE_MAX_FILES 20
#define F1C_USAGE_FILE_CAP  (40 * 1024)

/* spin lock (log_flush / worker / game-thread callbacks). */
static void __attribute__((unused)) f1c_usage_lock(volatile LONG *l)
{
    while (InterlockedCompareExchange(l, 1, 0)) Sleep(0);
}
static void __attribute__((unused)) f1c_usage_unlock(volatile LONG *l)
{
    InterlockedExchange(l, 0);
}

/* <game>\plugins\lightloader\reports\usage, created on demand. 0 on failure. */
static int __attribute__((unused)) f1c_usage_dir(char *dir)
{
    char *sl;
    DWORD n = GetModuleFileNameA(NULL, dir, MAX_PATH);
    if (!n || n >= MAX_PATH || !(sl = strrchr(dir, '\\'))) return 0;
    if ((size_t)(sl - dir) + 48 >= MAX_PATH) return 0;
    strcpy(sl + 1, "plugins\\lightloader\\reports");
    CreateDirectoryA(dir, NULL);
    strcat(dir, "\\usage");
    CreateDirectoryA(dir, NULL);
    return GetFileAttributesA(dir) != INVALID_FILE_ATTRIBUTES;
}

/* number of queued <mod_id>-*.json summaries. */
static int __attribute__((unused)) f1c_usage_count(const char *dir, const char *mod_id)
{
    char pat[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int n = 0;
    _snprintf(pat, sizeof pat, "%s\\%s-*.json", dir, mod_id);
    pat[sizeof pat - 1] = '\0';
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do n++; while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}

/* the "end" reason token: strip quotes/backslashes, cut to 23 chars. */
static void __attribute__((unused)) f1c_usage_why(char *w, const char *why)
{
    int i;
    for (i = 0; why[i] && i < 23; i++)
        w[i] = (why[i] == '"' || why[i] == '\\') ? ' ' : why[i];
    w[i] = '\0';
}

/* crash marker: a line that is an access violation (or the mod's own hardened
 * crash code). */
static int __attribute__((unused)) f1c_usage_is_crash(const char *s, const char *code16)
{
    if (strstr(s, "ACCESS VIOLATION")) return 1;
    if (code16 && strncmp(s, code16, 16) == 0) return 1;
    return 0;
}

/* the shared JSON envelope: schema/mod/version/end/client_time, then the
 * mod's "pre" fields, then seconds, then the mod's "middle" fields
 * (settings / counts / faults), then the kept lines.
 * Returns the bytes written (>= cap means truncated: caller must not write). */
static size_t __attribute__((unused)) f1c_usage_render(char *buf, size_t cap,
        const char *mod_id, const char *version, const char *end_why,
        const char *pre, unsigned long seconds, const SYSTEMTIME *st,
        const char *middle,
        const char *head, int head_n,
        const char *tail, int tail_n, int tail_pos)
{
    size_t o = 0;
    int i;
#define F1C_USAGE_P(...) do { int k_ = _snprintf(buf + o, cap - o, __VA_ARGS__); \
        if (k_ > 0 && (size_t)k_ < cap - o) o += (size_t)k_; else o = cap; } while (0)
    F1C_USAGE_P("{\"schema\":1,\"mod\":\"%s\",\"version\":\"%s\",\"end\":\"%s\","
                "\"client_time\":\"%04d-%02d-%02dT%02d:%02d:%02dZ\",",
                mod_id, version, end_why, st->wYear, st->wMonth, st->wDay,
                st->wHour, st->wMinute, st->wSecond);
    if (pre && *pre) F1C_USAGE_P("%s", pre);
    F1C_USAGE_P("\"seconds\":%lu,", seconds);
    if (middle && *middle) F1C_USAGE_P("%s", middle);
    F1C_USAGE_P("\"lines\":[");
    for (i = 0; i < head_n; i++)
        F1C_USAGE_P("%s\"%s\"", i ? "," : "", head + (size_t)i * F1C_USAGE_LINE);
    for (i = 0; i < tail_n; i++) {
        int k = (tail_pos - tail_n + i + F1C_USAGE_TAIL) % F1C_USAGE_TAIL;
        F1C_USAGE_P("%s\"%s\"", (head_n || i) ? "," : "",
                    tail + (size_t)k * F1C_USAGE_LINE);
    }
    F1C_USAGE_P("]}");
#undef F1C_USAGE_P
    return o;
}

/* atomic write of one summary: <dir>\<mod_id>-<utc>-<salt:08x>.json via .tmp.
 * Never leaves a broken file (tmp removed on any failure). */
static int __attribute__((unused)) f1c_usage_write(const char *dir, const char *mod_id,
        const SYSTEMTIME *st, const char *buf, size_t o, unsigned long salt)
{
    char fin[MAX_PATH], tmp[MAX_PATH];
    FILE *f;
    int ok = 0;
    if (o + 2 >= F1C_USAGE_FILE_CAP) return 0;
    _snprintf(fin, sizeof fin, "%s\\%s-%04d%02d%02dT%02d%02d%02dZ-%08lx.json",
              dir, mod_id, st->wYear, st->wMonth, st->wDay, st->wHour,
              st->wMinute, st->wSecond, salt);
    fin[sizeof fin - 1] = '\0';
    _snprintf(tmp, sizeof tmp, "%s.tmp", fin);
    tmp[sizeof tmp - 1] = '\0';
    f = fopen(tmp, "wb");
    if (f) {
        ok = fwrite(buf, 1, o, f) == o;
        ok = (fclose(f) == 0) && ok;
    }
    if (!ok || !MoveFileA(tmp, fin)) DeleteFileA(tmp);
    return ok;
}

/* ---- shared crash VEH (U2 fix) ----------------------------------------
 * A crash summary is written on a real access violation whose faulting
 * address is inside the game exe's own .text (derived from the exe PE header,
 * never a fixed constant) OR inside the reporting module's own image (derived
 * from the module PE header via GetModuleHandleEx FROM_ADDRESS). Everything
 * else (other fault types, addresses outside both images) is ignored. The
 * handler always returns EXCEPTION_CONTINUE_SEARCH. The per-mod callback
 * writes the summary once per process. */
static int __attribute__((unused)) f1c_usage_image_bounds(HMODULE mod,
        uintptr_t *lo, uintptr_t *hi)
{
    unsigned char *b = (unsigned char *)mod;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    if (!b) return 0;
    dos = (IMAGE_DOS_HEADER *)b;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS *)(b + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    *lo = (uintptr_t)b;
    *hi = (uintptr_t)b + nt->OptionalHeader.SizeOfImage;
    return 1;
}

static int __attribute__((unused)) f1c_usage_text_bounds(HMODULE mod,
        uintptr_t *lo, uintptr_t *hi)
{
    unsigned char *b = (unsigned char *)mod;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    int i;
    if (!b) return 0;
    dos = (IMAGE_DOS_HEADER *)b;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS *)(b + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, ".text", 5) == 0) {
            *lo = (uintptr_t)b + sec->VirtualAddress;
            *hi = *lo + (sec->Misc.VirtualSize ? sec->Misc.VirtualSize
                                               : sec->SizeOfRawData);
            return 1;
        }
    }
    return 0;
}

/* ---- module identity without names (2026-10-11 crash capture) -----------
 * Usage lines must be numbers and fixed words, so a module is a number:
 * 1 game exe, 2 the reporting mod itself, 3.. fixed system/driver/mod ids
 * below, 900 = anything else (its lowercase base-name CRC-32 is reported as a
 * number so it can be matched against a private decode list). 0 = no module
 * (an address outside every image: wild jump / freed code). */
static const char *const f1c_usage_known_modules[] = {
    /* 3 */ "ntdll.dll", "kernelbase.dll", "kernel32.dll", "d3d8.dll", "d3d9.dll",
    /* 8 */ "dinput8.dll", "dsound.dll", "nvd3dum.dll", "atiumdag.dll", "igdumdim32.dll",
    /* 13 */ "textinputframework.dll", "msvcrt.dll", "ws2_32.dll", "mswsock.dll", "user32.dll",
    /* 18 */ "gdi32.dll", "d3d8to9.dll", "ucrtbase.dll", "dbghelp.dll", "winmm.dll",
    /* 23 */ "dinput.dll", "xinput1_3.dll", "aticfx32.dll", "amdxc32.dll", "nvwgf2um.dll",
    /* 28 */ "combase.dll", "rpcrt4.dll",
    /* 30 */ "coop_mode.dll", "esc_pause_menu.dll", "extended_damage_commentary.dll",
    /* 33 */ "mechanical_failures.dll", "pitstop_failures.dll", "redflag.dll", "safetycar.dll",
    /* 37 */ "camera_editor.dll", "driver_ai_toolkit.dll", "engine_failure.dll",
    /* 40 */ "qualifying_preservation.dll", "ai_brain_lab.dll", "grid_restore.dll",
    /* 43 */ "race_data_collector.dll", "web_telemetry.dll",
    /* 45 */ "discordhook.dll", "rtsshooks.dll", "gameoverlayrenderer.dll", "nvspcap.dll",
    NULL
};

static unsigned long __attribute__((unused)) f1c_usage_crc32(const char *s)
{
    unsigned long c = 0xFFFFFFFFul;
    int k;
    for (; *s; s++) {
        c ^= (unsigned char)*s;
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320ul & (0ul - (c & 1ul)));
    }
    return c ^ 0xFFFFFFFFul;
}

/* lowercase base name of a module; "" when unknown. */
static void __attribute__((unused)) f1c_usage_module_base(HMODULE m, char *out, size_t n)
{
    char path[MAX_PATH];
    const char *b;
    size_t i;
    out[0] = '\0';
    if (!m || !GetModuleFileNameA(m, path, sizeof path)) return;
    path[sizeof path - 1] = '\0';
    b = strrchr(path, '\\');
    b = b ? b + 1 : path;
    for (i = 0; b[i] && i + 1 < n; i++)
        out[i] = (b[i] >= 'A' && b[i] <= 'Z') ? (char)(b[i] + 32) : b[i];
    out[i] = '\0';
}

/* numeric module id (see table); *crc = base-name CRC for id 900. */
static int __attribute__((unused)) f1c_usage_module_id(HMODULE m, HMODULE self, unsigned long *crc)
{
    char base[MAX_PATH];
    int i;
    if (crc) *crc = 0;
    if (!m) return 0;
    if (m == GetModuleHandleA(NULL)) return 1;
    if (self && m == self) return 2;
    f1c_usage_module_base(m, base, sizeof base);
    for (i = 0; f1c_usage_known_modules[i]; i++)
        if (strcmp(base, f1c_usage_known_modules[i]) == 0) return 3 + i;
    if (crc) *crc = f1c_usage_crc32(base);
    return 900;
}

static HMODULE __attribute__((unused)) f1c_usage_module_of(const void *addr)
{
    HMODULE m = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m))
        return NULL;
    return m;
}

static HMODULE __attribute__((unused)) f1c_usage_self_module(void);

/* 1 = report this exception (2026-10-11: widened; the 0.9.5 CO-OP client
 * crashes faulted in ntdll / an IME DLL / no module and were invisible to
 * every exe-or-own-image filter). Fatal-class codes anywhere in the process,
 * EXCEPT faults inside kernelbase/kernel32: IsBadReadPtr-style probes (used by
 * several mods) raise handled first-chance AVs there. */
static int __attribute__((unused)) f1c_usage_crash_should_report(
        const EXCEPTION_POINTERS *ep)
{
    HMODULE m;
    char base[MAX_PATH];
    DWORD code;
    if (!ep || !ep->ExceptionRecord) return 0;
    code = ep->ExceptionRecord->ExceptionCode;
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION: case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW: case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case 0xC0000374ul: /* heap corruption */ case 0xC0000409ul: /* stack buffer overrun */
        break;
    default:
        return 0;
    }
    m = f1c_usage_module_of(ep->ExceptionRecord->ExceptionAddress);
    if (m && m != GetModuleHandleA(NULL) && m != f1c_usage_self_module()) {
        f1c_usage_module_base(m, base, sizeof base);
        if (strcmp(base, "kernelbase.dll") == 0 || strcmp(base, "kernel32.dll") == 0) return 0;
    }
    return 1;
}

typedef void (*f1c_usage_crash_fn)(void);
static __attribute__((unused)) f1c_usage_crash_fn g_f1c_usage_crash_fn;
static __attribute__((unused)) PVOID g_f1c_usage_veh;

/* 2026-10-09 (report review): the exception context the callback reports.
 * The vectored handler sees FIRST-CHANCE exceptions (they may be handled
 * later), so the line says so; the per-mod callback taps it (the "ACCESS
 * VIOLATION" marker makes counts.crash count it) and flushes with end
 * "exception". Callback signature unchanged so every includer still builds. */
typedef struct {
    DWORD code, thread;
    uintptr_t addr, base;
    int where;                  /* 1 = game exe, 2 = reporting module, 3 = other module, 0 = no module */
    int module;                 /* numeric module id (f1c_usage_module_id) */
    unsigned long crc;          /* base-name CRC-32 when module == 900 */
} F1cUsageExc;
static __attribute__((unused)) F1cUsageExc g_f1c_usage_exc;

static HMODULE __attribute__((unused)) f1c_usage_self_module(void)
{
    return f1c_usage_module_of((const void *)&f1c_usage_self_module);
}

static void __attribute__((unused)) f1c_usage_exc_note(const EXCEPTION_POINTERS *ep)
{
    HMODULE m, self = f1c_usage_self_module();
    memset(&g_f1c_usage_exc, 0, sizeof g_f1c_usage_exc);
    if (!ep || !ep->ExceptionRecord) return;
    g_f1c_usage_exc.code = ep->ExceptionRecord->ExceptionCode;
    g_f1c_usage_exc.addr = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    g_f1c_usage_exc.thread = GetCurrentThreadId();
    m = f1c_usage_module_of(ep->ExceptionRecord->ExceptionAddress);
    g_f1c_usage_exc.base = (uintptr_t)m;
    g_f1c_usage_exc.module = f1c_usage_module_id(m, self, &g_f1c_usage_exc.crc);
    g_f1c_usage_exc.where = !m ? 0 : m == GetModuleHandleA(NULL) ? 1 : m == self ? 2 : 3;
}

/* "ACCESS VIOLATION first-chance code 0xC0000005 image 1 offset 0x4D718 thread 3396"
 * (+ " module N" for image 3/0) - numbers and fixed words only (passes every
 * mod's words-only filter). The code field is the real exception code; the
 * leading words stay "ACCESS VIOLATION" because every mod's crash counter and
 * the CO-OP keep-line filter key on that prefix. */
static void __attribute__((unused)) f1c_usage_exc_line(char *buf, size_t n)
{
    if (g_f1c_usage_exc.where >= 1 && g_f1c_usage_exc.where <= 2)
        _snprintf(buf, n, "ACCESS VIOLATION first-chance code 0x%08lX image %d offset 0x%lX thread %lu",
                  (unsigned long)g_f1c_usage_exc.code, g_f1c_usage_exc.where,
                  (unsigned long)(g_f1c_usage_exc.addr - g_f1c_usage_exc.base),
                  (unsigned long)g_f1c_usage_exc.thread);
    else
        _snprintf(buf, n, "ACCESS VIOLATION first-chance code 0x%08lX image %d offset 0x%lX thread %lu module %d crc %lu",
                  (unsigned long)g_f1c_usage_exc.code, g_f1c_usage_exc.where,
                  (unsigned long)(g_f1c_usage_exc.addr - g_f1c_usage_exc.base),
                  (unsigned long)g_f1c_usage_exc.thread, g_f1c_usage_exc.module, g_f1c_usage_exc.crc);
    if (n) buf[n - 1] = '\0';
}

/* ---- crash detail + minidump (2026-10-11, owner: "all of our mods do
 * extensive crash reporting") -------------------------------------------
 * Once per PROCESS (named mutex shared by every mod that includes this
 * header), the first reported exception writes:
 *  1. reports\usage\<mod>-<utc>-<salt>.json, end "crash_detail": fault
 *     (module id + offset, read/write/execute + target), registers, up to 24
 *     stack return addresses as module id + offset, and every loaded
 *     module's id + timestamp + size. Numbers and fixed words only; uploaded
 *     like any usage summary (consent-gated by the manager).
 *  2. reports\crash\<mod>-<utc>-<pid>.dmp, a minidump (stack, data segments,
 *     referenced memory) for local / bug-report analysis. NEVER uploaded by
 *     this code. At most 5 dumps are kept (oldest of ours removed).
 * A chained unhandled-exception filter repeats both with fatal=1 once per
 * process when the game really dies (the VEH sees first-chance exceptions). */
typedef BOOL (WINAPI *F1cMiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, int, void *, void *, void *);
static __attribute__((unused)) const char *g_f1c_usage_mod_id;
static __attribute__((unused)) const char *g_f1c_usage_mod_version;
static __attribute__((unused)) F1cMiniDumpWriteDumpFn g_f1c_usage_minidump;
static __attribute__((unused)) LPTOP_LEVEL_EXCEPTION_FILTER g_f1c_usage_prev_filter;
static __attribute__((unused)) volatile LONG g_f1c_usage_filter_set;

typedef struct { char line[24][F1C_USAGE_LINE]; int n; } F1cCrashLines;

static void __attribute__((unused)) f1c_crash_add(F1cCrashLines *c, const char *fmt, ...)
{
    va_list ap;
    if (c->n >= 24) return;
    va_start(ap, fmt);
    _vsnprintf(c->line[c->n], F1C_USAGE_LINE, fmt, ap);
    va_end(ap);
    c->line[c->n][F1C_USAGE_LINE - 1] = '\0';
    c->n++;
}

/* return address heuristic: inside a module's .text and preceded by a CALL. */
static int __attribute__((unused)) f1c_crash_is_return(uintptr_t v, HMODULE *mod)
{
    uintptr_t lo, hi;
    const unsigned char *p = (const unsigned char *)v;
    HMODULE m = f1c_usage_module_of((const void *)v);
    if (!m || !f1c_usage_text_bounds(m, &lo, &hi) || v < lo + 7 || v >= hi) return 0;
    *mod = m;
    if (p[-5] == 0xE8) return 1;                                   /* call rel32 */
    if (p[-2] == 0xFF && (p[-1] & 0x38) == 0x10) return 1;          /* call r/m (reg) */
    if (p[-3] == 0xFF && (p[-2] & 0x38) == 0x10) return 1;          /* call [r+d8] */
    if (p[-6] == 0xFF && (p[-5] & 0x38) == 0x10) return 1;          /* call [d32]/[r+d32] */
    if (p[-7] == 0xFF && (p[-6] & 0x38) == 0x10) return 1;          /* call [sib+d32] */
    return 0;
}

static void __attribute__((unused)) f1c_crash_collect(const EXCEPTION_POINTERS *ep, int fatal,
                                                      F1cCrashLines *c)
{
    const CONTEXT *cx = ep->ContextRecord;
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    HMODULE self = f1c_usage_self_module();
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    uintptr_t sp, top;
    char items[F1C_USAGE_LINE];
    size_t o = 0;
    int frames = 0, ln = 0;
    HANDLE snap;
    MODULEENTRY32 me;
    memset(c, 0, sizeof *c);
    f1c_crash_add(c, "fault code 0x%08lX module %d offset 0x%lX crc %lu op %lu target 0x%08lX thread %lu fatal %d",
                  (unsigned long)er->ExceptionCode, g_f1c_usage_exc.module,
                  (unsigned long)(g_f1c_usage_exc.addr - g_f1c_usage_exc.base), g_f1c_usage_exc.crc,
                  er->NumberParameters > 0 ? (unsigned long)er->ExceptionInformation[0] : 9ul,
                  er->NumberParameters > 1 ? (unsigned long)er->ExceptionInformation[1] : 0ul,
                  (unsigned long)GetCurrentThreadId(), fatal);
    if (cx) {
        f1c_crash_add(c, "regs eip 0x%08lX esp 0x%08lX ebp 0x%08lX eax 0x%08lX ebx 0x%08lX ecx 0x%08lX edx 0x%08lX esi 0x%08lX edi 0x%08lX",
                      (unsigned long)cx->Eip, (unsigned long)cx->Esp, (unsigned long)cx->Ebp,
                      (unsigned long)cx->Eax, (unsigned long)cx->Ebx, (unsigned long)cx->Ecx,
                      (unsigned long)cx->Edx, (unsigned long)cx->Esi, (unsigned long)cx->Edi);
        /* stack scan: dwords from ESP up to the thread stack base (max 16 KB) */
        sp = (uintptr_t)cx->Esp & ~(uintptr_t)3;
        top = tib ? (uintptr_t)tib->StackBase : sp + 16384;
        if (top > sp + 16384) top = sp + 16384;
        items[0] = '\0';
        for (; sp + 4 <= top && frames < 24; sp += 4) {
            uintptr_t v = *(const uintptr_t *)sp;
            HMODULE m = NULL;
            unsigned long crc;
            int id;
            if (!f1c_crash_is_return(v, &m)) continue;
            id = f1c_usage_module_id(m, self, &crc);
            o += (size_t)_snprintf(items + o, sizeof items - o, " %d:0x%lX", id,
                                   (unsigned long)(v - (uintptr_t)m));
            frames++;
            if (o > sizeof items - 24 || frames % 8 == 0) {
                f1c_crash_add(c, "stack %d%s", ln++, items);
                o = 0; items[0] = '\0';
            }
        }
        if (o) f1c_crash_add(c, "stack %d%s", ln, items);
    }
    /* loaded modules: id:timestamp:size (unknown ones by crc) */
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        int count = 0;
        o = 0; items[0] = '\0';
        me.dwSize = sizeof me;
        if (Module32First(snap, &me)) do {
            unsigned long crc;
            int id = f1c_usage_module_id(me.hModule, self, &crc);
            IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)me.modBaseAddr;
            IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(me.modBaseAddr + dos->e_lfanew);
            if (id == 900)
                o += (size_t)_snprintf(items + o, sizeof items - o, " 900.%lu:%lX", crc,
                                       (unsigned long)nt->FileHeader.TimeDateStamp);
            else
                o += (size_t)_snprintf(items + o, sizeof items - o, " %d:%lX:%lX", id,
                                       (unsigned long)nt->FileHeader.TimeDateStamp,
                                       (unsigned long)me.modBaseSize);
            if (++count % 5 == 0 || o > sizeof items - 40) {
                f1c_crash_add(c, "modules%s", items);
                o = 0; items[0] = '\0';
            }
        } while (c->n < 24 && Module32Next(snap, &me));
        if (o && c->n < 24) f1c_crash_add(c, "modules%s", items);
        CloseHandle(snap);
    }
}

static DWORD WINAPI f1c_crash_dump_thread(LPVOID p)
{
    struct { DWORD tid; EXCEPTION_POINTERS *ep; BOOL client; } mei;
    const void **arg = (const void **)p;
    HANDLE f = (HANDLE)arg[0];
    mei.tid = (DWORD)(uintptr_t)arg[1];
    mei.ep = (EXCEPTION_POINTERS *)arg[2];
    mei.client = FALSE;
    /* MiniDumpWithDataSegs | MiniDumpScanMemory | MiniDumpWithIndirectlyReferencedMemory */
    g_f1c_usage_minidump(GetCurrentProcess(), GetCurrentProcessId(), f, 0x1 | 0x10 | 0x40, &mei, NULL, NULL);
    return 0;
}

static void __attribute__((unused)) f1c_crash_write_dump(EXCEPTION_POINTERS *ep, const SYSTEMTIME *st)
{
    char dir[MAX_PATH], path[MAX_PATH], pat[MAX_PATH], oldest[MAX_PATH];
    WIN32_FIND_DATAA fd;
    FILETIME old;
    HANDLE h, f, t;
    const void *arg[3];
    int n = 0;
    if (!g_f1c_usage_minidump || !f1c_usage_dir(dir)) return;
    *strrchr(dir, '\\') = '\0';                  /* ...\reports */
    strcat(dir, "\\crash");
    CreateDirectoryA(dir, NULL);
    _snprintf(pat, sizeof pat, "%s\\*.dmp", dir); pat[sizeof pat - 1] = '\0';
    oldest[0] = '\0';
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!n++ || CompareFileTime(&fd.ftLastWriteTime, &old) < 0) {
                old = fd.ftLastWriteTime;
                _snprintf(oldest, sizeof oldest, "%s\\%s", dir, fd.cFileName);
                oldest[sizeof oldest - 1] = '\0';
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (n >= 5 && oldest[0]) DeleteFileA(oldest);
    _snprintf(path, sizeof path, "%s\\%s-%04d%02d%02dT%02d%02d%02dZ-%lu.dmp", dir,
              g_f1c_usage_mod_id ? g_f1c_usage_mod_id : "mod", st->wYear, st->wMonth, st->wDay,
              st->wHour, st->wMinute, st->wSecond, (unsigned long)GetCurrentProcessId());
    path[sizeof path - 1] = '\0';
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    arg[0] = f; arg[1] = (const void *)(uintptr_t)GetCurrentThreadId(); arg[2] = ep;
    /* dbghelp recommends dumping from another thread; bounded wait in case the
     * loader lock is held by the faulting thread. */
    t = CreateThread(NULL, 0, f1c_crash_dump_thread, (LPVOID)arg, 0, NULL);
    if (t) { WaitForSingleObject(t, 8000); CloseHandle(t); }
    CloseHandle(f);
}

/* once per process per kind (0 first-chance, 1 fatal), across all mods */
static int __attribute__((unused)) f1c_crash_claim(int fatal)
{
    char name[64];
    HANDLE m;
    _snprintf(name, sizeof name, "Local\\F1C_CRASH_%s_%lu", fatal ? "FATAL" : "FIRST",
              (unsigned long)GetCurrentProcessId());
    name[sizeof name - 1] = '\0';
    m = CreateMutexA(NULL, FALSE, name);
    if (!m) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(m); return 0; }
    return 1;                                     /* handle kept for the process lifetime */
}

static void __attribute__((unused)) f1c_crash_capture(EXCEPTION_POINTERS *ep, int fatal)
{
    static F1cCrashLines lines;                   /* static: crash path, keep the stack small */
    static char buf[F1C_USAGE_FILE_CAP];
    char dir[MAX_PATH], pre[96];
    SYSTEMTIME st;
    size_t o;
    if (!g_f1c_usage_mod_id || !ep || !ep->ExceptionRecord || !f1c_crash_claim(fatal)) return;
    f1c_usage_exc_note(ep);
    f1c_crash_collect(ep, fatal, &lines);
    GetSystemTime(&st);
    _snprintf(pre, sizeof pre, "\"crash_schema\":1,\"fatal\":%d,", fatal);
    pre[sizeof pre - 1] = '\0';
    o = f1c_usage_render(buf, sizeof buf, g_f1c_usage_mod_id,
                         g_f1c_usage_mod_version ? g_f1c_usage_mod_version : "unknown",
                         "crash_detail", pre, 0, &st, "", &lines.line[0][0], lines.n, NULL, 0, 0);
    if (o < sizeof buf && f1c_usage_dir(dir) &&
        f1c_usage_count(dir, g_f1c_usage_mod_id) < F1C_USAGE_MAX_FILES)
        f1c_usage_write(dir, g_f1c_usage_mod_id, &st, buf, o,
                        (unsigned long)GetCurrentProcessId() ^ (fatal ? 0xFA7A1ul : 0xC0FFEEul));
    f1c_crash_write_dump(ep, &st);
}

static LONG WINAPI f1c_usage_unhandled_filter(EXCEPTION_POINTERS *ep)
{
    f1c_crash_capture(ep, 1);
    return g_f1c_usage_prev_filter ? g_f1c_usage_prev_filter(ep) : EXCEPTION_CONTINUE_SEARCH;
}

static LONG CALLBACK f1c_usage_veh_cb(EXCEPTION_POINTERS *ep)
{
    if (f1c_usage_crash_should_report(ep)) {
        f1c_crash_capture(ep, 0);
        if (g_f1c_usage_crash_fn) {
            f1c_usage_exc_note(ep);
            g_f1c_usage_crash_fn();
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* Every mod calls this once at start (before or after f1c_usage_veh_install):
 * enables the crash-detail summary + minidump under its own mod id. id and
 * version must be string literals / static storage. */
static void __attribute__((unused)) f1c_usage_crash_identity(const char *mod_id, const char *version)
{
    char sys[MAX_PATH];
    HMODULE dbghelp;
    g_f1c_usage_mod_id = mod_id;
    g_f1c_usage_mod_version = version;
    if (!g_f1c_usage_minidump && GetSystemDirectoryA(sys, MAX_PATH - 16)) {
        strcat(sys, "\\dbghelp.dll");                 /* system copy only, never the game folder */
        dbghelp = LoadLibraryA(sys);
        if (dbghelp)
            g_f1c_usage_minidump = (F1cMiniDumpWriteDumpFn)(void *)GetProcAddress(dbghelp, "MiniDumpWriteDump");
    }
    if (InterlockedExchange(&g_f1c_usage_filter_set, 1) == 0)
        g_f1c_usage_prev_filter = SetUnhandledExceptionFilter(f1c_usage_unhandled_filter);
}

/* mod id from the module's own file name (usage id table; "redflag.dll" ->
 * "red_flag", "safetycar.dll" -> "safety_car"); NULL when not a known mod. */
static const char *__attribute__((unused)) f1c_usage_auto_mod_id(void)
{
    static const char *const map[][2] = {
        {"coop_mode.dll", "coop_mode"}, {"esc_pause_menu.dll", "esc_pause_menu"},
        {"extended_damage_commentary.dll", "extended_damage_commentary"},
        {"mechanical_failures.dll", "mechanical_failures"}, {"engine_failure.dll", "mechanical_failures"},
        {"pitstop_failures.dll", "pitstop_failures"}, {"redflag.dll", "red_flag"},
        {"safetycar.dll", "safety_car"}, {"camera_editor.dll", "camera_editor"},
        {"driver_ai_toolkit.dll", "driver_ai_toolkit"}, {NULL, NULL}};
    char base[MAX_PATH];
    int i;
    f1c_usage_module_base(f1c_usage_self_module(), base, sizeof base);
    for (i = 0; map[i][0]; i++) if (strcmp(base, map[i][0]) == 0) return map[i][1];
    return NULL;
}

static void __attribute__((unused)) f1c_usage_veh_install(f1c_usage_crash_fn fn)
{
    g_f1c_usage_crash_fn = fn;
    /* 2026-10-11: every includer gets crash detail + minidump, even without an
     * explicit f1c_usage_crash_identity() call (version then "unknown"). */
    if (!g_f1c_usage_mod_id) {
        const char *id = f1c_usage_auto_mod_id();
        if (id) f1c_usage_crash_identity(id, NULL);
    }
    if (!g_f1c_usage_veh)
        g_f1c_usage_veh = AddVectoredExceptionHandler(1, f1c_usage_veh_cb);
}
static void __attribute__((unused)) f1c_usage_veh_remove(void)
{
    if (g_f1c_usage_veh) { RemoveVectoredExceptionHandler(g_f1c_usage_veh); g_f1c_usage_veh = NULL; }
    g_f1c_usage_crash_fn = NULL;
    /* restore the previous filter only if ours is still installed (never clobber a later one) */
    if (InterlockedExchange(&g_f1c_usage_filter_set, 0)) {
        LPTOP_LEVEL_EXCEPTION_FILTER cur = SetUnhandledExceptionFilter(g_f1c_usage_prev_filter);
        if (cur != f1c_usage_unhandled_filter) SetUnhandledExceptionFilter(cur);
    }
}

/* ---- schema 2 (2026-10-11, development/mods/USAGE_REPORTS_V2_SPEC_2026-10-11.md) ----
 * Structured event timeline: [t, "code", v1..v4]. Codes are fixed lowercase
 * identifiers chosen by the mod ([a-z0-9_.], max 23 chars) - never names or
 * paths; anything else is replaced by "bad_code". Ring of the last
 * F1C_USAGE_EVENTS entries; older ones are counted in events_dropped. */
#define F1C_USAGE_EVENTS 400
typedef struct {
    unsigned t_ds;          /* tenths of a second since the ring was reset */
    char     code[24];
    int      n;
    double   v[4];
} F1cUsageEvent;
typedef struct {
    F1cUsageEvent ev[F1C_USAGE_EVENTS];
    int           count, pos;
    unsigned long dropped;
    DWORD         start;
    volatile LONG lock;
} F1cUsageEvents;

static void __attribute__((unused)) f1c_usage_events_reset(F1cUsageEvents *e)
{
    f1c_usage_lock(&e->lock);
    e->count = e->pos = 0;
    e->dropped = 0;
    e->start = GetTickCount();
    f1c_usage_unlock(&e->lock);
}

static int __attribute__((unused)) f1c_usage_code_ok(const char *c)
{
    size_t i;
    if (!c || !c[0]) return 0;
    for (i = 0; c[i]; i++) {
        char ch = c[i];
        if (i >= 23) return 0;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.')) return 0;
    }
    return 1;
}

/* Record one event with n (0..4) numbers. Thread-safe; never blocks long. */
static void __attribute__((unused)) f1c_usage_ev(F1cUsageEvents *e, const char *code, int n,
        double a, double b, double c, double d)
{
    F1cUsageEvent *x;
    if (!e) return;
    f1c_usage_lock(&e->lock);
    if (!e->start) e->start = GetTickCount();
    x = &e->ev[e->pos];
    x->t_ds = (unsigned)((GetTickCount() - e->start) / 100u);
    lstrcpynA(x->code, f1c_usage_code_ok(code) ? code : "bad_code", sizeof(x->code));
    x->n = n < 0 ? 0 : n > 4 ? 4 : n;
    x->v[0] = a; x->v[1] = b; x->v[2] = c; x->v[3] = d;
    e->pos = (e->pos + 1) % F1C_USAGE_EVENTS;
    if (e->count < F1C_USAGE_EVENTS) e->count++; else e->dropped++;
    f1c_usage_unlock(&e->lock);
}
#define F1C_USAGE_EV0(e, c)             f1c_usage_ev((e), (c), 0, 0, 0, 0, 0)
#define F1C_USAGE_EV1(e, c, a)          f1c_usage_ev((e), (c), 1, (a), 0, 0, 0)
#define F1C_USAGE_EV2(e, c, a, b)       f1c_usage_ev((e), (c), 2, (a), (b), 0, 0)
#define F1C_USAGE_EV3(e, c, a, b, d)    f1c_usage_ev((e), (c), 3, (a), (b), (d), 0)
#define F1C_USAGE_EV4(e, c, a, b, d, f) f1c_usage_ev((e), (c), 4, (a), (b), (d), (f))

/* Append "\"events\":[...],\"events_dropped\":N," to buf (for the render
 * `middle` argument). Returns bytes written (0 if it does not fit). */
static size_t __attribute__((unused)) f1c_usage_events_json(F1cUsageEvents *e, char *buf, size_t cap)
{
    size_t o = 0;
    int i, j;
    if (!e || !buf || cap < 64) return 0;
#define F1C_USAGE_EP(...) do { int k_ = _snprintf(buf + o, cap - o, __VA_ARGS__); \
        if (k_ > 0 && (size_t)k_ < cap - o) o += (size_t)k_; else { buf[0] = 0; f1c_usage_unlock(&e->lock); return 0; } } while (0)
    f1c_usage_lock(&e->lock);
    F1C_USAGE_EP("\"events\":[");
    for (i = 0; i < e->count; i++) {
        const F1cUsageEvent *x = &e->ev[(e->pos - e->count + i + F1C_USAGE_EVENTS) % F1C_USAGE_EVENTS];
        F1C_USAGE_EP("%s[%u.%u,\"%s\"", i ? "," : "", x->t_ds / 10u, x->t_ds % 10u, x->code);
        for (j = 0; j < x->n; j++) {
            double v = x->v[j];
            if (v != v || v > 1e15 || v < -1e15) v = 0;   /* NaN / huge -> 0 */
            if (v == (double)(long long)v) F1C_USAGE_EP(",%lld", (long long)v);
            else F1C_USAGE_EP(",%.3f", v);
        }
        F1C_USAGE_EP("]");
    }
    F1C_USAGE_EP("],\"events_dropped\":%lu,", e->dropped);
    f1c_usage_unlock(&e->lock);
#undef F1C_USAGE_EP
    return o;
}

/* One log line -> one event. code = the first 1-3 plain lowercase words (or an
 * error code like COOP-E13 -> "coop_e13") joined by '_'; values = the first 4
 * numbers (plain, hex or key=number). Tokens with uppercase letters (names),
 * backslashes/colons (paths, addresses) or 2+ dots (IPs) are ignored. */
static int __attribute__((unused)) f1c_usage_tok_number(const char *t, size_t n, double *out)
{
    char tmp[40];
    size_t i = 0, dots = 0;
    if (!n || n >= sizeof tmp) return 0;
    if (t[0] == '-' || t[0] == '+') i = 1;
    if (i >= n) return 0;
    if (n > i + 2 && t[i] == '0' && (t[i + 1] == 'x' || t[i + 1] == 'X')) {
        size_t j;
        for (j = i + 2; j < n; j++)
            if (!((t[j] >= '0' && t[j] <= '9') || (t[j] >= 'a' && t[j] <= 'f') || (t[j] >= 'A' && t[j] <= 'F'))) return 0;
        memcpy(tmp, t, n); tmp[n] = 0;
        *out = (double)strtoull(tmp + i, NULL, 16);
        return 1;
    }
    {
        size_t digits = 0, end = n;
        while (end > i && t[end - 1] >= 'a' && t[end - 1] <= 'z' && n - end < 3) end--;   /* 16ms, 30fps */
        for (; i < end; i++) {
            if (t[i] == '.') { if (++dots > 1) return 0; }
            else if (t[i] >= '0' && t[i] <= '9') digits++;
            else return 0;
        }
        if (!digits) return 0;
        memcpy(tmp, t, end); tmp[end] = 0;
    }
    *out = atof(tmp);
    return 1;
}

static void __attribute__((unused)) f1c_usage_ev_line(F1cUsageEvents *e, const char *line)
{
    char code[24] = "";
    double v[4] = {0, 0, 0, 0};
    int nv = 0, words = 0, stop_words = 0;
    const char *p = line;
    if (!e || !line) return;
    while (*p) {
        const char *s, *t;
        size_t n, k;
        int lower = 1, has_bad = 0, has_digit = 0;
        while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '(' || *p == ')' || *p == '[' || *p == ']') p++;
        if (!*p) break;
        s = p;
        while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != ';' && *p != '(' && *p != ')' && *p != '[' && *p != ']') p++;
        n = (size_t)(p - s);
        t = s;
        /* key=value: the value counts as a number, the key is ignored */
        for (k = 0; k < n; k++) if (s[k] == '=') { t = s + k + 1; n -= k + 1; break; }
        while (n && (t[n - 1] == ':' || t[n - 1] == '.' || t[n - 1] == '%' || t[n - 1] == 's')) {
            if (t[n - 1] == 's' && !(n > 1 && t[n - 2] >= '0' && t[n - 2] <= '9')) break;   /* "5s" -> 5 */
            n--;
        }
        if (nv < 4 && f1c_usage_tok_number(t, n, &v[nv])) { nv++; stop_words = 1; continue; }
        if (t != s || stop_words || words >= 3) continue;
        for (k = 0; k < n; k++) {
            char c = s[k];
            if (c >= 'A' && c <= 'Z') lower = 0;
            else if (c >= '0' && c <= '9') has_digit = 1;
            else if (!((c >= 'a' && c <= 'z') || c == '_' || c == '-')) has_bad = 1;
        }
        if (!lower && !has_bad && n >= 4 && n <= 12) {
            /* error codes such as COOP-E13 / SC-E201 / RF-EE1: UPPER-Xnn */
            int dash = 0, ok = 1;
            for (k = 0; k < n; k++) {
                char c = s[k];
                if (c == '-') dash++;
                else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) ok = 0;
            }
            if (ok && dash == 1 && has_digit) {
                size_t o = strlen(code);
                if (o && o < 22) code[o++] = '_';
                for (k = 0; k < n && o < 23; k++) code[o++] = s[k] == '-' ? '_' : (char)(s[k] >= 'A' && s[k] <= 'Z' ? s[k] + 32 : s[k]);
                code[o] = 0;
                words++;
            }
            continue;
        }
        if (!lower || has_bad || has_digit || n < 2) { if (words) stop_words = 1; continue; }
        {
            size_t o = strlen(code);
            if (o && o < 22) code[o++] = '_';
            for (k = 0; k < n && o < 23; k++) code[o++] = s[k] == '-' ? '_' : s[k];
            code[o] = 0;
            words++;
        }
    }
    if (!code[0]) lstrcpynA(code, "line", sizeof code);
    f1c_usage_ev(e, code, nv, v[0], v[1], v[2], v[3]);
}

/* Schema-2 marker: render() still writes schema 1; mods that add events call
 * this on the rendered buffer to flip it (same length, in place). */
static void __attribute__((unused)) f1c_usage_mark_v2(char *buf)
{
    if (buf && strncmp(buf, "{\"schema\":1,", 12) == 0) buf[10] = '2';
}

#endif /* F1C_USAGE_H */
