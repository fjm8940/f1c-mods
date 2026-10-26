/* esc_usage.h - per-session usage summary for ESC Pause Menu (batch U2).
 *
 * Built on the shared f1c_usage.h core. The DLL writes ONE JSON per session to
 * <game>\plugins\lightloader\reports\usage\ and Lightloader uploads it only when
 * the player's report switch is on. Event codes/counts/versions only: no paths,
 * user or driver names (path-like tokens become <path>, files that name a
 * profile/veh/ini are dropped).
 *
 * Key events: session start, RETIRE commands, refusals/failures, unload, crash.
 */
#ifndef ESC_USAGE_H
#define ESC_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "f1c_usage.h"

#define ESCU_MOD_ID      "esc_pause_menu"
#define ESCU_MOD_VERSION "1.1.4"

typedef struct {
    int   active;
    DWORD start_tick;
    int   n_lines, n_retire, n_refused, n_unload, n_crash;
    char  head[F1C_USAGE_HEAD][F1C_USAGE_LINE];
    int   head_n;
    char  tail[F1C_USAGE_TAIL][F1C_USAGE_LINE];
    int   tail_n, tail_pos;
} EscUsage;

static EscUsage       g_escu;
static volatile LONG  g_escu_lock;

static int escu_starts(const char *s, const char *p)
{
    return strncmp(s, p, strlen(p)) == 0;
}

/* "words only": keep the first token if it is a code/[tag], then numeric/hex
 * tokens only; everything else becomes <s>. No names, paths or free text. */
static int escu_numeric(const char *t, size_t n)
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

static void escu_sanitize(char *dst, const char *src)
{
    size_t o = 0;
    int first = 1;
    while (*src == ' ') src++;
    while (*src && o + 8 < F1C_USAGE_LINE) {
        const char *e = src;
        size_t n;
        while (*e && *e != ' ') e++;
        n = (size_t)(e - src);
        if ((first && n <= 24 && (strncmp(src, "ESC-E", 5) == 0 || src[0] == '[')) ||
            (n <= 24 && escu_numeric(src, n))) {
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

static void escu_tap(const char *s)
{
    char clean[F1C_USAGE_LINE];
    if (!s || !*s) return;
    f1c_usage_lock(&g_escu_lock);
    if (!g_escu.active) { g_escu.active = 1; g_escu.start_tick = GetTickCount(); }
    if (escu_starts(s, "RETIRE command received")) g_escu.n_retire++;
    if (strstr(s, "refused") || strstr(s, "failed") || strstr(s, "abort")) g_escu.n_refused++;
    if (strstr(s, "unloading")) g_escu.n_unload++;
    if (f1c_usage_is_crash(s, NULL)) g_escu.n_crash++;
    escu_sanitize(clean, s);
    if (clean[0]) {
        g_escu.n_lines++;
        if (g_escu.head_n < F1C_USAGE_HEAD) {
            memcpy(g_escu.head[g_escu.head_n++], clean, F1C_USAGE_LINE);
        } else {
            memcpy(g_escu.tail[g_escu.tail_pos], clean, F1C_USAGE_LINE);
            g_escu.tail_pos = (g_escu.tail_pos + 1) % F1C_USAGE_TAIL;
            if (g_escu.tail_n < F1C_USAGE_TAIL) g_escu.tail_n++;
        }
    }
    f1c_usage_unlock(&g_escu_lock);
}

static void escu_flush(const char *why)
{
    static char buf[F1C_USAGE_FILE_CAP];
    char dir[MAX_PATH], w[24], mid[128];
    SYSTEMTIME st;
    DWORD secs;
    size_t o;

    f1c_usage_lock(&g_escu_lock);
    if (!g_escu.active) { f1c_usage_unlock(&g_escu_lock); return; }
    secs = (GetTickCount() - g_escu.start_tick) / 1000u;
    if ((secs < 10 && g_escu.n_crash == 0 && !(why && strcmp(why, "crash") == 0)) ||
        !f1c_usage_dir(dir) ||
        f1c_usage_count(dir, ESCU_MOD_ID) >= F1C_USAGE_MAX_FILES) {
        memset(&g_escu, 0, sizeof g_escu);
        f1c_usage_unlock(&g_escu_lock);
        return;
    }
    f1c_usage_why(w, why);
    GetSystemTime(&st);
    _snprintf(mid, sizeof mid,
              "\"counts\":{\"retire\":%d,\"refused\":%d,\"unload\":%d,\"crash\":%d,\"lines\":%d},",
              g_escu.n_retire, g_escu.n_refused, g_escu.n_unload, g_escu.n_crash, g_escu.n_lines);
    mid[sizeof mid - 1] = '\0';
    o = f1c_usage_render(buf, sizeof buf, ESCU_MOD_ID, ESCU_MOD_VERSION, w,
                         "", (unsigned long)secs, &st, mid,
                         (const char *)g_escu.head, g_escu.head_n,
                         (const char *)g_escu.tail, g_escu.tail_n, g_escu.tail_pos);
    memset(&g_escu, 0, sizeof g_escu);
    f1c_usage_unlock(&g_escu_lock);
    if (o + 2 >= sizeof buf) return;
    f1c_usage_write(dir, ESCU_MOD_ID, &st, buf, o,
                    (unsigned long)(GetTickCount() ^ (DWORD)(uintptr_t)&o));
}

/* 1.0.9: game-thread broker session-end callback sets a flag only; the ESC
 * worker loop performs the file write. Session end is the only flush point that
 * survives a normal return to the menu (the DLL is not unloaded), which is why
 * 1.0.8 wrote no usage file at all. */
static volatile LONG g_escu_request;

static void escu_request(void) { InterlockedExchange(&g_escu_request, 1); }

/* worker loop: honour a pending session-end request. */
static void escu_pump(void)
{
    if (InterlockedExchange(&g_escu_request, 0)) escu_flush("session_end");
}

/* Crash summary, once per process, via the shared VEH rule in f1c_usage.h. */
static volatile LONG g_escu_crash_done;

static void escu_crash(void)
{
    if (InterlockedCompareExchange(&g_escu_crash_done, 1, 0) != 0) return;
    f1c_usage_lock(&g_escu_lock);
    if (!g_escu.active) { g_escu.active = 1; g_escu.start_tick = GetTickCount(); }
    f1c_usage_unlock(&g_escu_lock);
    /* 1.1.1 (report review 2026-10-09): carry the exception context (counts
     * it as crash via the ACCESS VIOLATION marker) and label it first-chance:
     * the shared VEH cannot know whether the game handles it. */
    {
        char l[F1C_USAGE_LINE];
        f1c_usage_exc_line(l, sizeof l);
        escu_tap(l);
    }
    escu_flush("exception");
}

static void escu_veh_install(void) { f1c_usage_veh_install(escu_crash); }
static void escu_veh_remove(void) { f1c_usage_veh_remove(); }

#endif /* ESC_USAGE_H */
