/* pit_usage.h - per-session usage summary for Pit Stop Failures (batch U2).
 *
 * Built on the shared f1c_usage.h core. The DLL writes ONE JSON per session to
 * <game>\plugins\lightloader\reports\usage\ and Lightloader uploads it only when
 * the player's report switch is on. Words only: event codes/counts/versions, no
 * paths, user or driver names.
 *
 * Key events: initialized, wheel/pit incidents, refusals, retirements, crash.
 */
#ifndef PIT_USAGE_H
#define PIT_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "f1c_usage.h"

#define PITU_MOD_ID      "pitstop_failures"
#define PITU_MOD_VERSION "1.1.4"

typedef struct {
    int   active;
    DWORD start_tick;
    int   n_lines, n_incident, n_refused, n_retire, n_crash;
    char  head[F1C_USAGE_HEAD][F1C_USAGE_LINE];
    int   head_n;
    char  tail[F1C_USAGE_TAIL][F1C_USAGE_LINE];
    int   tail_n, tail_pos;
} PitUsage;

static PitUsage      g_pitu;
static volatile LONG g_pitu_lock;

static int pitu_numeric(const char *t, size_t n)
{
    size_t i = 0;
    int digits = 0;
    if (n && (t[0] == '-' || t[0] == '+')) i = 1;
    if (n >= i + 2 && t[i] == '0' && (t[i + 1] == 'x' || t[i + 1] == 'X')) i += 2;
    for (; i < n; i++) {
        char c = t[i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) digits++;
        else if (c != '.' && c != '%' && c != '(' && c != ')' && c != ',' && c != '=' && c != '|') return 0;
    }
    for (i = 0; i < n; i++) if (t[i] >= '0' && t[i] <= '9') return digits > 0;
    return 0;
}

static void pitu_sanitize(char *dst, const char *src)
{
    size_t o = 0;
    int first = 1;
    while (*src == ' ') src++;
    while (*src && o + 8 < F1C_USAGE_LINE) {
        const char *e = src;
        size_t n;
        while (*e && *e != ' ') e++;
        n = (size_t)(e - src);
        if ((first && n <= 24 && (strncmp(src, "PIT-E", 5) == 0 || src[0] == '[')) ||
            (n <= 24 && pitu_numeric(src, n))) {
            if (o + n >= F1C_USAGE_LINE - 1) n = F1C_USAGE_LINE - 1 - o;
            memcpy(dst + o, src, n); o += n;
        } else {
            if (o + 3 >= F1C_USAGE_LINE - 1) break;
            memcpy(dst + o, "<s>", 3); o += 3;
        }
        first = 0;
        src = e;
        while (*src == ' ') src++;
        if (*src && o + 1 < F1C_USAGE_LINE - 1) dst[o++] = ' ';
    }
    dst[o] = '\0';
    for (o = 0; dst[o]; o++)
        if ((unsigned char)dst[o] < 0x20 || (unsigned char)dst[o] > 0x7e ||
            dst[o] == '"' || dst[o] == '\\' || dst[o] == '/' || dst[o] == ':') dst[o] = ' ';
}

static void pitu_tap(const char *s)
{
    char clean[F1C_USAGE_LINE];
    if (!s || !*s) return;
    f1c_usage_lock(&g_pitu_lock);
    if (!g_pitu.active) { g_pitu.active = 1; g_pitu.start_tick = GetTickCount(); }
    if (strstr(s, "incident") || strstr(s, "failure") || strstr(s, "wheelgun")) g_pitu.n_incident++;
    if (strstr(s, "refused") || strstr(s, "rejected") || strstr(s, "unavailable")) g_pitu.n_refused++;
    if (strstr(s, "retire") || strstr(s, "RETIRE")) g_pitu.n_retire++;
    if (f1c_usage_is_crash(s, NULL)) g_pitu.n_crash++;
    pitu_sanitize(clean, s);
    if (clean[0]) {
        g_pitu.n_lines++;
        if (g_pitu.head_n < F1C_USAGE_HEAD) {
            memcpy(g_pitu.head[g_pitu.head_n++], clean, F1C_USAGE_LINE);
        } else {
            memcpy(g_pitu.tail[g_pitu.tail_pos], clean, F1C_USAGE_LINE);
            g_pitu.tail_pos = (g_pitu.tail_pos + 1) % F1C_USAGE_TAIL;
            if (g_pitu.tail_n < F1C_USAGE_TAIL) g_pitu.tail_n++;
        }
    }
    f1c_usage_unlock(&g_pitu_lock);
}

static void pitu_flush(const char *why)
{
    static char buf[F1C_USAGE_FILE_CAP];
    char dir[MAX_PATH], w[24], mid[128];
    SYSTEMTIME st;
    DWORD secs;
    size_t o;

    f1c_usage_lock(&g_pitu_lock);
    if (!g_pitu.active) { f1c_usage_unlock(&g_pitu_lock); return; }
    secs = (GetTickCount() - g_pitu.start_tick) / 1000u;
    if ((secs < 10 && g_pitu.n_crash == 0 && !(why && strcmp(why, "crash") == 0)) ||
        !f1c_usage_dir(dir) ||
        f1c_usage_count(dir, PITU_MOD_ID) >= F1C_USAGE_MAX_FILES) {
        memset(&g_pitu, 0, sizeof g_pitu);
        f1c_usage_unlock(&g_pitu_lock);
        return;
    }
    f1c_usage_why(w, why);
    GetSystemTime(&st);
    _snprintf(mid, sizeof mid,
              "\"counts\":{\"incident\":%d,\"refused\":%d,\"retire\":%d,\"crash\":%d,\"lines\":%d},",
              g_pitu.n_incident, g_pitu.n_refused, g_pitu.n_retire, g_pitu.n_crash, g_pitu.n_lines);
    mid[sizeof mid - 1] = '\0';
    o = f1c_usage_render(buf, sizeof buf, PITU_MOD_ID, PITU_MOD_VERSION, w,
                         "", (unsigned long)secs, &st, mid,
                         (const char *)g_pitu.head, g_pitu.head_n,
                         (const char *)g_pitu.tail, g_pitu.tail_n, g_pitu.tail_pos);
    memset(&g_pitu, 0, sizeof g_pitu);
    f1c_usage_unlock(&g_pitu_lock);
    if (o + 2 >= sizeof buf) return;
    f1c_usage_write(dir, PITU_MOD_ID, &st, buf, o,
                    (unsigned long)(GetTickCount() ^ (DWORD)(uintptr_t)&o));
}

/* 1.0.9: game-thread broker session-end callback sets a flag only; the Pit
 * worker loop performs the file write. Session end is the only flush point that
 * survives a normal return to the menu (the DLL is not unloaded), which is why
 * 1.1.1 wrote no usage file at all. */
static volatile LONG g_pitu_request;

static void pitu_request(void) { InterlockedExchange(&g_pitu_request, 1); }

/* worker loop: honour a pending session-end request. */
static void pitu_pump(void)
{
    if (InterlockedExchange(&g_pitu_request, 0)) pitu_flush("session_end");
}

/* Crash summary, once per process, from the VEH on a real game-code fault. */
static volatile LONG g_pitu_crash_done;
static PVOID         g_pitu_veh;

static void pitu_crash(void)
{
    if (InterlockedCompareExchange(&g_pitu_crash_done, 1, 0) != 0) return;
    f1c_usage_lock(&g_pitu_lock);
    if (!g_pitu.active) { g_pitu.active = 1; g_pitu.start_tick = GetTickCount(); }
    f1c_usage_unlock(&g_pitu_lock);
    /* 1.1.1 (report review 2026-10-09): exception context + first-chance label */
    {
        char l[F1C_USAGE_LINE];
        f1c_usage_exc_line(l, sizeof l);
        pitu_tap(l);
    }
    pitu_flush("exception");
}

static LONG CALLBACK pitu_crash_veh(EXCEPTION_POINTERS *ep)
{
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    if ((uintptr_t)ep->ExceptionRecord->ExceptionAddress < 0x401000u ||
        (uintptr_t)ep->ExceptionRecord->ExceptionAddress > 0x600000u)
        return EXCEPTION_CONTINUE_SEARCH;
    f1c_usage_exc_note(ep);
    pitu_crash();
    return EXCEPTION_CONTINUE_SEARCH;
}

static void pitu_veh_install(void)
{
    if (!g_pitu_veh) g_pitu_veh = AddVectoredExceptionHandler(1, pitu_crash_veh);
}
static void pitu_veh_remove(void)
{
    if (g_pitu_veh) { RemoveVectoredExceptionHandler(g_pitu_veh); g_pitu_veh = NULL; }
}

#endif /* PIT_USAGE_H */
