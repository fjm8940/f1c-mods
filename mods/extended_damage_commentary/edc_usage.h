/* edc_usage.h - per-session usage summary for Extended Damage Commentary (U2).
 *
 * Built on the shared f1c_usage.h core. The DLL writes ONE JSON per process to
 * <game>\plugins\lightloader\reports\usage\ ; Lightloader uploads it only when
 * the player's report switch is on. Words only: counts/codes/versions, no names
 * or paths. Key events: sites applied, refusal, crash.
 *
 * Threads (MOD_STANDARD R2): edcu_tap() runs on EDC's log path. The broker
 * session-end callback sets a request flag only (edcu_request, game thread);
 * the EDC worker thread calls edcu_pump() and performs the flush. edcu_crash()
 * runs in the shared VEH handler. No file I/O happens in DllMain (loader lock)
 * or on the game thread.
 */
#ifndef EDC_USAGE_H
#define EDC_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "f1c_usage.h"

#define EDCU_MOD_ID      "extended_damage_commentary"
#define EDCU_MOD_VERSION "1.0.5"

typedef struct {
    int   active;
    DWORD start_tick;
    int   n_lines, n_active, n_refused, n_crash;
    char  head[F1C_USAGE_HEAD][F1C_USAGE_LINE];
    int   head_n;
    char  tail[F1C_USAGE_TAIL][F1C_USAGE_LINE];
    int   tail_n, tail_pos;
} EdcUsage;

static EdcUsage      g_edcu;
static volatile LONG g_edcu_lock;

static int edcu_numeric(const char *t, size_t n)
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

/* Stable per-template event code: FNV-1a 32 over the line with every digit run
 * collapsed to '#'. Two lines from the same template share one code; the code
 * is reproducible offline from the private log for triage. */
static void edcu_code(const char *s, char *out)
{
    unsigned long h = 2166136261u;
    int indigit = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9') {
            if (indigit) continue;
            c = '#'; indigit = 1;
        } else indigit = 0;
        h ^= (unsigned long)c; h *= 16777619u;
    }
    sprintf(out, "EDC-E%08lX", h & 0xffffffffu);
}

/* kept line = stable event code, then the numeric value of each token (the
 * part after the last '=' or ':' when present, else the whole token). Words,
 * names and paths are never kept. */
static void edcu_sanitize(char *dst, const char *src)
{
    char code[16];
    size_t o;
    edcu_code(src, code);
    o = strlen(code);
    memcpy(dst, code, o);
    while (*src && o + 8 < F1C_USAGE_LINE) {
        const char *e = src, *val = src, *q;
        size_t vn;
        while (*src == ' ') src++;
        if (!*src) break;
        e = src; while (*e && *e != ' ') e++;
        for (q = src; q < e; q++)
            if (*q == '=' || *q == ':') val = q + 1;
        vn = (size_t)(e - val);
        if (vn && edcu_numeric(val, vn)) {
            if (o + vn >= F1C_USAGE_LINE - 1) vn = F1C_USAGE_LINE - 1 - o;
            if (o + 1 < F1C_USAGE_LINE - 1) dst[o++] = ' ';
            memcpy(dst + o, val, vn); o += vn;
        } else {
            /* keep embedded number runs (6/6, site 12) */
            const char *p = src;
            while (p < e) {
                const char *d = p, *z;
                while (d < e && !(*d >= '0' && *d <= '9')) d++;
                if (d >= e) break;
                z = d;
                while (z < e && ((*z >= '0' && *z <= '9') || *z == '.')) z++;
                if (o + 1 + (size_t)(z - d) < F1C_USAGE_LINE - 1) {
                    dst[o++] = ' ';
                    memcpy(dst + o, d, (size_t)(z - d)); o += (size_t)(z - d);
                }
                p = z;
            }
        }
        src = e;
    }
    dst[o] = '\0';
    for (o = 0; dst[o]; o++)
        if ((unsigned char)dst[o] < 0x20 || (unsigned char)dst[o] > 0x7e ||
            dst[o] == '"' || dst[o] == '\\' || dst[o] == '/' || dst[o] == ':') dst[o] = ' ';
}

static void edcu_tap(const char *s)
{
    char clean[F1C_USAGE_LINE];
    if (!s || !*s) return;
    f1c_usage_lock(&g_edcu_lock);
    if (!g_edcu.active) { g_edcu.active = 1; g_edcu.start_tick = GetTickCount(); }
    if (strstr(s, "sites active")) g_edcu.n_active++;
    if (strstr(s, "refused")) g_edcu.n_refused++;
    if (f1c_usage_is_crash(s, NULL)) g_edcu.n_crash++;
    edcu_sanitize(clean, s);
    if (clean[0]) {
        g_edcu.n_lines++;
        if (g_edcu.head_n < F1C_USAGE_HEAD) {
            memcpy(g_edcu.head[g_edcu.head_n++], clean, F1C_USAGE_LINE);
        } else {
            memcpy(g_edcu.tail[g_edcu.tail_pos], clean, F1C_USAGE_LINE);
            g_edcu.tail_pos = (g_edcu.tail_pos + 1) % F1C_USAGE_TAIL;
            if (g_edcu.tail_n < F1C_USAGE_TAIL) g_edcu.tail_n++;
        }
    }
    f1c_usage_unlock(&g_edcu_lock);
}

static void edcu_flush(const char *why)
{
    static char buf[F1C_USAGE_FILE_CAP];
    char dir[MAX_PATH], w[24], mid[96];
    SYSTEMTIME st;
    DWORD secs;
    size_t o;

    f1c_usage_lock(&g_edcu_lock);
    if (!g_edcu.active) { f1c_usage_unlock(&g_edcu_lock); return; }
    secs = (GetTickCount() - g_edcu.start_tick) / 1000u;
    if ((secs < 1 && g_edcu.n_crash == 0 && !(why && strcmp(why, "crash") == 0)) ||
        !f1c_usage_dir(dir) ||
        f1c_usage_count(dir, EDCU_MOD_ID) >= F1C_USAGE_MAX_FILES) {
        memset(&g_edcu, 0, sizeof g_edcu);
        f1c_usage_unlock(&g_edcu_lock);
        return;
    }
    f1c_usage_why(w, why);
    GetSystemTime(&st);
    _snprintf(mid, sizeof mid,
              "\"counts\":{\"active\":%d,\"refused\":%d,\"crash\":%d,\"lines\":%d},",
              g_edcu.n_active, g_edcu.n_refused, g_edcu.n_crash, g_edcu.n_lines);
    mid[sizeof mid - 1] = '\0';
    o = f1c_usage_render(buf, sizeof buf, EDCU_MOD_ID, EDCU_MOD_VERSION, w,
                         "", (unsigned long)secs, &st, mid,
                         (const char *)g_edcu.head, g_edcu.head_n,
                         (const char *)g_edcu.tail, g_edcu.tail_n, g_edcu.tail_pos);
    memset(&g_edcu, 0, sizeof g_edcu);
    f1c_usage_unlock(&g_edcu_lock);
    if (o + 2 >= sizeof buf) return;
    f1c_usage_write(dir, EDCU_MOD_ID, &st, buf, o,
                    (unsigned long)(GetTickCount() ^ (DWORD)(uintptr_t)&o));
}

/* game-thread safe: a flag only; the EDC worker loop writes the file.
 * Set from the broker session-end callback; the game thread must never do the
 * I/O (MOD_STANDARD R2). */
static volatile LONG g_edcu_request;

static void edcu_request(void) { InterlockedExchange(&g_edcu_request, 1); }

/* worker loop: honour a pending session-end request. */
static void edcu_pump(void)
{
    if (InterlockedExchange(&g_edcu_request, 0)) edcu_flush("session_end");
}

/* Crash summary, once per process, via the shared VEH rule in f1c_usage.h. */
static volatile LONG g_edcu_crash_done;

static void edcu_crash(void)
{
    if (InterlockedCompareExchange(&g_edcu_crash_done, 1, 0) != 0) return;
    f1c_usage_lock(&g_edcu_lock);
    if (!g_edcu.active) { g_edcu.active = 1; g_edcu.start_tick = GetTickCount(); }
    f1c_usage_unlock(&g_edcu_lock);
    /* 1.0.5 (report review 2026-10-09): exception context + first-chance label */
    {
        char l[F1C_USAGE_LINE];
        f1c_usage_exc_line(l, sizeof l);
        edcu_tap(l);
    }
    edcu_flush("exception");
}

static void edcu_veh_install(void) { f1c_usage_veh_install(edcu_crash); }
static void edcu_veh_remove(void) { f1c_usage_veh_remove(); }

#endif /* EDC_USAGE_H */
