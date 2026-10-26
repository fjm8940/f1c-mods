/* web_telemetry_usage.h - usage and crash reports for Web Telemetry (rc.17,
 * MOD_STANDARD section 4, USAGE_REPORTS_V2_SPEC schema 2).
 *
 * The DLL writes ONE JSON per game session (and on unload, process exit and a
 * crash) to <game>\plugins\lightloader\reports\usage\. Lightloader uploads it
 * only when the player has turned on "Send crash, bug and mod usage reports
 * automatically". The DLL never sends anything itself.
 *
 * Content: counters, timings, numeric settings, the EXE layout verdict and a
 * structured event timeline. Log lines are reduced to lowercase codes and
 * numbers by the shared filters: driver names (capitalised words), paths and
 * IP addresses never survive (f1c_usage_ev_line / wtu_sanitize).
 */
#ifndef WEB_TELEMETRY_USAGE_H
#define WEB_TELEMETRY_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "f1c_usage.h"

#define WTU_MOD_ID "web_telemetry"

typedef struct {
    int   active;
    DWORD start_tick;
    int   n_sessions, n_http, n_http_errors, n_sse_max, n_refused, n_errors, n_crash, n_lines;
    int   layout_mode, layout_anchors, port, remote, last_type, cars_max;
    char  head[F1C_USAGE_HEAD][F1C_USAGE_LINE];
    int   head_n;
    char  tail[F1C_USAGE_TAIL][F1C_USAGE_LINE];
    int   tail_n, tail_pos;
} WtUsage;

static WtUsage        g_wtu;
static F1cUsageEvents g_wtu_ev;
static volatile LONG  g_wtu_lock;

/* Words only: a token survives when it is all lowercase letters / digits /
 * '_' '.' '=' '%' '-' (no capitals = no names) and has no '\\', '/' or ':'
 * (no paths, no addresses). Everything else becomes <s>. */
static int wtu_token_ok(const char *t, size_t n)
{
    size_t i;
    int dots = 0;
    if (!n || n > 24) return 0;
    for (i = 0; i < n; i++) {
        char c = t[i];
        if (c == '.') { if (++dots > 1) return 0; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '=' || c == '%' || c == '-')) return 0;
    }
    return 1;
}

static void wtu_sanitize(char *dst, const char *src)
{
    size_t o = 0;
    while (*src == ' ' || *src == '[') { if (*src == '[') { while (*src && *src != ']') src++; if (*src) src++; } else src++; }
    while (*src && o + 8 < F1C_USAGE_LINE) {
        const char *e = src;
        size_t n;
        while (*e && *e != ' ') e++;
        n = (size_t)(e - src);
        if (wtu_token_ok(src, n)) {
            if (o + n >= F1C_USAGE_LINE - 1) n = F1C_USAGE_LINE - 1 - o;
            memcpy(dst + o, src, n); o += n;
        } else {
            if (o + 3 >= F1C_USAGE_LINE - 1) break;
            memcpy(dst + o, "<s>", 3); o += 3;
        }
        src = e;
        while (*src == ' ') src++;
        if (*src && o + 1 < F1C_USAGE_LINE - 1) dst[o++] = ' ';
    }
    dst[o] = '\0';
}

/* Every log line is fed here (log_w / log_f). */
static void wtu_tap(const char *s)
{
    char clean[F1C_USAGE_LINE];
    if (!s || !*s) return;
    f1c_usage_lock(&g_wtu_lock);
    if (!g_wtu.active) { g_wtu.active = 1; g_wtu.start_tick = GetTickCount(); }
    if (strstr(s, "REFUSED")) g_wtu.n_refused++;
    if (strstr(s, "failed") || strstr(s, "error") || strstr(s, "overflow")) g_wtu.n_errors++;
    if (f1c_usage_is_crash(s, NULL)) g_wtu.n_crash++;
    wtu_sanitize(clean, s);
    if (clean[0]) {
        g_wtu.n_lines++;
        if (g_wtu.head_n < F1C_USAGE_HEAD) {
            memcpy(g_wtu.head[g_wtu.head_n++], clean, F1C_USAGE_LINE);
        } else {
            memcpy(g_wtu.tail[g_wtu.tail_pos], clean, F1C_USAGE_LINE);
            g_wtu.tail_pos = (g_wtu.tail_pos + 1) % F1C_USAGE_TAIL;
            if (g_wtu.tail_n < F1C_USAGE_TAIL) g_wtu.tail_n++;
        }
    }
    f1c_usage_unlock(&g_wtu_lock);
    f1c_usage_ev_line(&g_wtu_ev, s);
}

/* Structured counters from the mod (any thread). */
static void wtu_note_layout(int mode, int anchors)
{
    f1c_usage_lock(&g_wtu_lock);
    g_wtu.layout_mode = mode; g_wtu.layout_anchors = anchors;
    f1c_usage_unlock(&g_wtu_lock);
    F1C_USAGE_EV2(&g_wtu_ev, "layout", mode, anchors);
}
static void wtu_note_listen(int port, int remote)
{
    f1c_usage_lock(&g_wtu_lock);
    g_wtu.port = port; g_wtu.remote = remote;
    f1c_usage_unlock(&g_wtu_lock);
    F1C_USAGE_EV2(&g_wtu_ev, "listen", port, remote);
}
static void wtu_note_session(int type, int cars)
{
    f1c_usage_lock(&g_wtu_lock);
    if (!g_wtu.active) { g_wtu.active = 1; g_wtu.start_tick = GetTickCount(); }
    g_wtu.n_sessions++; g_wtu.last_type = type;
    if (cars > g_wtu.cars_max) g_wtu.cars_max = cars;
    f1c_usage_unlock(&g_wtu_lock);
    F1C_USAGE_EV2(&g_wtu_ev, "session", type, cars);
}
static void wtu_note_http(int status, int sse_clients)
{
    f1c_usage_lock(&g_wtu_lock);
    g_wtu.n_http++;
    if (status >= 400) g_wtu.n_http_errors++;
    if (sse_clients > g_wtu.n_sse_max) g_wtu.n_sse_max = sse_clients;
    f1c_usage_unlock(&g_wtu_lock);
    if (status >= 500) F1C_USAGE_EV1(&g_wtu_ev, "http_5xx", status);
}

static void wtu_reset_locked(void)
{
    int mode = g_wtu.layout_mode, anchors = g_wtu.layout_anchors, port = g_wtu.port, remote = g_wtu.remote;
    memset(&g_wtu, 0, sizeof g_wtu);
    g_wtu.layout_mode = mode; g_wtu.layout_anchors = anchors; g_wtu.port = port; g_wtu.remote = remote;
}

/* Builds and writes one summary. skip_if_busy: never wait (process exit). */
static void wtu_write(const char *why, int skip_if_busy)
{
    static char buf[F1C_USAGE_FILE_CAP];
    static char mid[F1C_USAGE_FILE_CAP / 2 + 1024];
    char dir[MAX_PATH], w[24];
    SYSTEMTIME st;
    DWORD secs;
    size_t o;

    if (skip_if_busy) {
        if (InterlockedCompareExchange(&g_wtu_lock, 1, 0)) return;
    } else {
        f1c_usage_lock(&g_wtu_lock);
    }
    if (!g_wtu.active || (!g_wtu.n_sessions && !g_wtu.n_crash && !g_wtu.n_refused)) {
        f1c_usage_unlock(&g_wtu_lock);
        return;
    }
    secs = (GetTickCount() - g_wtu.start_tick) / 1000u;
    if (!f1c_usage_dir(dir) || f1c_usage_count(dir, WTU_MOD_ID) >= F1C_USAGE_MAX_FILES) {
        wtu_reset_locked();
        f1c_usage_unlock(&g_wtu_lock);
        return;
    }
    f1c_usage_why(w, why ? why : "end");
    GetSystemTime(&st);
    _snprintf(mid, sizeof mid,
              "\"settings\":{\"port\":%d,\"remote\":%d},"
              "\"layout\":{\"mode\":%d,\"anchors\":%d},"
              "\"counts\":{\"sessions\":%d,\"http\":%d,\"http_errors\":%d,\"sse_max\":%d,"
              "\"refused\":%d,\"errors\":%d,\"crash\":%d,\"lines\":%d},"
              "\"session\":{\"session_type\":%d,\"cars_max\":%d},",
              g_wtu.port, g_wtu.remote, g_wtu.layout_mode, g_wtu.layout_anchors,
              g_wtu.n_sessions, g_wtu.n_http, g_wtu.n_http_errors, g_wtu.n_sse_max,
              g_wtu.n_refused, g_wtu.n_errors, g_wtu.n_crash, g_wtu.n_lines,
              g_wtu.last_type, g_wtu.cars_max);
    mid[sizeof mid - 1] = '\0';
    {
        size_t m = strlen(mid);
        f1c_usage_events_json(&g_wtu_ev, mid + m, sizeof mid - m);
    }
    o = f1c_usage_render(buf, sizeof buf, WTU_MOD_ID, TELEMETRY_VERSION, w,
                         "", (unsigned long)secs, &st, mid,
                         (const char *)g_wtu.head, g_wtu.head_n,
                         (const char *)g_wtu.tail, g_wtu.tail_n, g_wtu.tail_pos);
    wtu_reset_locked();
    f1c_usage_events_reset(&g_wtu_ev);
    f1c_usage_unlock(&g_wtu_lock);
    if (o + 2 >= sizeof buf) return;
    f1c_usage_mark_v2(buf);
    f1c_usage_write(dir, WTU_MOD_ID, &st, buf, o, (unsigned long)(GetTickCount() ^ (DWORD)(uintptr_t)&o));
}

/* Crash summary, once per process, via the shared VEH rule in f1c_usage.h. */
static volatile LONG g_wtu_crash_done;
static void wtu_crash(void)
{
    char l[F1C_USAGE_LINE];
    if (InterlockedCompareExchange(&g_wtu_crash_done, 1, 0) != 0) return;
    f1c_usage_exc_line(l, sizeof l);
    wtu_tap(l);
    wtu_write("exception", 1);
}
static void wtu_start(void)
{
    f1c_usage_events_reset(&g_wtu_ev);
    f1c_usage_crash_identity(WTU_MOD_ID, TELEMETRY_VERSION);
    f1c_usage_veh_install(wtu_crash);
}

#endif /* WEB_TELEMETRY_USAGE_H */
