/* rf_usage.h - per-session usage summary for Red Flag (batch U2, 1.1.6).
 *
 * Built on the shared f1c_usage.h core. The DLL never sends anything: it
 * writes ONE small JSON file per finished session to
 *     <game>\plugins\lightloader\reports\usage\red_flag-<utc>-<hex>.json
 * and Lightloader uploads it only when the player's "Send crash, bug and mod
 * usage reports automatically" switch is on (at most RFU_MAX_FILES queued).
 *
 * Privacy by construction: a kept line is its first token (event code or
 * [tag]) followed by numeric/hex tokens only; every other token becomes "<s>".
 * No paths, user names or driver names.
 *
 * Threads: rfu_tap() runs inside Red Flag's log_w (worker and hook threads).
 * The broker session-end callback sets a request flag only (rfu_request, game
 * thread); the RF worker loop (thread_main) calls rfu_pump() and performs the
 * flush, the same way sc_usage.h does. rfu_flush() also runs on the worker's
 * unload path; rfu_crash() runs in the shared VEH handler. No file I/O happens
 * in DllMain (loader lock; dead threads at process exit) or on the game thread.
 */
#ifndef RF_USAGE_H
#define RF_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "f1c_usage.h"

#define RFU_MOD_ID        "red_flag"
#define RFU_MOD_VERSION   "1.1.9"
#define RFU_HEAD          F1C_USAGE_HEAD
#define RFU_TAIL          F1C_USAGE_TAIL
#define RFU_LINE          F1C_USAGE_LINE
#define RFU_MAX_FILES     F1C_USAGE_MAX_FILES
#define RFU_MIN_SECONDS   10
#define RFU_FILE_CAP      F1C_USAGE_FILE_CAP

typedef struct {
    int   active;
    DWORD start_tick;
    int   n_lines, n_start, n_redflag, n_refused, n_crash;
    char  head[RFU_HEAD][RFU_LINE];
    int   head_n;
    char  tail[RFU_TAIL][RFU_LINE];
    int   tail_n, tail_pos;
} RfUsage;

static RfUsage       g_rfu;
static F1cUsageEvents g_rfu_ev;   /* schema 2 timeline (USAGE_REPORTS_V2_SPEC) */
static volatile LONG g_rfu_lock;
static volatile LONG g_rfu_request;

static void rfu_lock(void)   { f1c_usage_lock(&g_rfu_lock); }
static void rfu_unlock(void) { f1c_usage_unlock(&g_rfu_lock); }

static int rfu_numeric(const char *t, size_t n)
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
static void rfu_code(const char *s, char *out)
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
    sprintf(out, "RF-E%08lX", h & 0xffffffffu);
}

/* kept line = stable event code, then the numeric value of each token (the
 * part after the last '=' or ':' when present, else the whole token). Words,
 * driver names, paths and free text are never kept. */
static void rfu_sanitize(char *dst, const char *src)
{
    char code[16];
    size_t o;
    rfu_code(src, code);
    o = strlen(code);
    memcpy(dst, code, o);
    while (*src && o + 8 < RFU_LINE) {
        const char *e = src, *val = src, *q;
        size_t vn;
        while (*src == ' ') src++;
        if (!*src) break;
        e = src; while (*e && *e != ' ') e++;
        for (q = src; q < e; q++)
            if (*q == '=' || *q == ':') val = q + 1;
        vn = (size_t)(e - val);
        if (vn && rfu_numeric(val, vn)) {
            if (o + vn >= RFU_LINE - 1) vn = RFU_LINE - 1 - o;
            if (o + 1 < RFU_LINE - 1) dst[o++] = ' ';
            memcpy(dst + o, val, vn); o += vn;
        } else {
            /* keep embedded number runs (6/6, slot 12) */
            const char *p = src;
            while (p < e) {
                const char *d = p, *z;
                while (d < e && !(*d >= '0' && *d <= '9')) d++;
                if (d >= e) break;
                z = d;
                while (z < e && ((*z >= '0' && *z <= '9') || *z == '.')) z++;
                if (o + 1 + (size_t)(z - d) < RFU_LINE - 1) {
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

/* called by log_w with each formatted line (no newline) */
static void rfu_tap(const char *s)
{
    char clean[RFU_LINE];
    if (!s || !*s) return;
    rfu_lock();
    if (!g_rfu.active) { g_rfu.active = 1; g_rfu.start_tick = GetTickCount(); f1c_usage_events_reset(&g_rfu_ev); }
    f1c_usage_ev_line(&g_rfu_ev, s);
    if (strstr(s, "Red Flag loaded") || strstr(s, "Red Flag trial loaded")) g_rfu.n_start++;
    if (strstr(s, "TRIGGER:") || strstr(s, "RED FLAG:") || strstr(s, "RED FLAG RESTART")) g_rfu.n_redflag++;
    if (strstr(s, "REFUSED") || strstr(s, "BLOCKED") || strstr(s, "REFUSAL")) g_rfu.n_refused++;
    if (f1c_usage_is_crash(s, NULL)) g_rfu.n_crash++;
    rfu_sanitize(clean, s);
    if (clean[0]) {
        g_rfu.n_lines++;
        if (g_rfu.head_n < RFU_HEAD) {
            memcpy(g_rfu.head[g_rfu.head_n++], clean, RFU_LINE);
        } else {
            memcpy(g_rfu.tail[g_rfu.tail_pos], clean, RFU_LINE);
            g_rfu.tail_pos = (g_rfu.tail_pos + 1) % RFU_TAIL;
            if (g_rfu.tail_n < RFU_TAIL) g_rfu.tail_n++;
        }
    }
    rfu_unlock();
}

/* write the summary (if the session was long enough or crashed) and reset */
static void rfu_flush(const char *why)
{
    static char buf[RFU_FILE_CAP];
    char dir[MAX_PATH], w[24]; static char mid[F1C_USAGE_FILE_CAP / 2 + 1024];
    SYSTEMTIME st;
    DWORD secs;
    size_t o;

    rfu_lock();
    if (!g_rfu.active) { rfu_unlock(); return; }
    secs = (GetTickCount() - g_rfu.start_tick) / 1000u;
    if ((secs < RFU_MIN_SECONDS && g_rfu.n_crash == 0 && !(why && strcmp(why, "crash") == 0)) ||
        !f1c_usage_dir(dir) ||
        f1c_usage_count(dir, RFU_MOD_ID) >= RFU_MAX_FILES) {
        memset(&g_rfu, 0, sizeof g_rfu);
        rfu_unlock();
        return;
    }
    f1c_usage_why(w, why);
    GetSystemTime(&st);
    _snprintf(mid, sizeof mid,
              "\"counts\":{\"start\":%d,\"redflag\":%d,\"refused\":%d,\"crash\":%d,\"lines\":%d},",
              g_rfu.n_start, g_rfu.n_redflag, g_rfu.n_refused, g_rfu.n_crash, g_rfu.n_lines);
    mid[sizeof mid - 1] = '\0';
    { size_t m_ = strlen(mid); f1c_usage_events_json(&g_rfu_ev, mid + m_, sizeof mid - m_); }
    o = f1c_usage_render(buf, sizeof buf, RFU_MOD_ID, RFU_MOD_VERSION, w,
                         "", (unsigned long)secs, &st, mid,
                         (const char *)g_rfu.head, g_rfu.head_n,
                         (const char *)g_rfu.tail, g_rfu.tail_n, g_rfu.tail_pos);
    memset(&g_rfu, 0, sizeof g_rfu);
    rfu_unlock();
    if (o + 2 >= sizeof buf) return;
    f1c_usage_mark_v2(buf);
    f1c_usage_write(dir, RFU_MOD_ID, &st, buf, o,
                    (unsigned long)(GetTickCount() ^ (DWORD)(uintptr_t)&o));
}

/* game-thread safe: a flag only; the RF worker (thread_main) writes the file */
static void rfu_request(void) { InterlockedExchange(&g_rfu_request, 1); }

/* worker loop: honour a pending session-end request */
static void rfu_pump(void) {
    if (InterlockedExchange(&g_rfu_request, 0)) rfu_flush("session_end");
}

/* Crash summary, once per process, via the shared VEH rule in f1c_usage.h. */
static volatile LONG g_rfu_crash_done;

static void rfu_crash(void)
{
    if (InterlockedCompareExchange(&g_rfu_crash_done, 1, 0) != 0) return;
    rfu_lock();
    if (!g_rfu.active) { g_rfu.active = 1; g_rfu.start_tick = GetTickCount(); }
    rfu_unlock();
    /* 1.1.8 (report review 2026-10-09): exception context + first-chance label */
    {
        char l[F1C_USAGE_LINE];
        f1c_usage_exc_line(l, sizeof l);
        rfu_tap(l);
    }
    rfu_flush("exception");
}

static void rfu_veh_install(void) { f1c_usage_veh_install(rfu_crash); }
static void rfu_veh_remove(void) { f1c_usage_veh_remove(); }

#endif /* RF_USAGE_H */
