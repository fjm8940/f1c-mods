/* coop_usage.h - per-session usage summary for Multiplayer Overhaul.
 *
 * Built on the shared f1c_usage.h core (MOD_STANDARD section 4). The DLL writes
 * ONE JSON per CO-OP multiplayer session to <game>\plugins\lightloader\reports\usage\
 * and Lightloader uploads it only when the player's "Send crash, bug and mod usage
 * reports automatically" switch is on. The DLL never sends anything itself.
 *
 * Content: fixed counters and timings, plus kept log lines reduced to fixed
 * lowercase words, codes and numbers. Driver/profile names (capitalised), paths,
 * IP addresses and host names never survive the filter (test_usage.py checks it).
 *
 * Feed: every log_line() text (coopu_tap). Debug-only lines update counters but
 * are not kept as text. Writes happen on the worker thread (coopu_pump), at
 * unload, on a crash (shared VEH) and best-effort at process exit.
 */
#ifndef COOP_USAGE_H
#define COOP_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "f1c_usage.h"

#define COOPU_MOD_ID      "coop_mode"
#define COOPU_MOD_VERSION "1.0.4"

typedef struct {
    int   active;
    DWORD start_tick;
    int   role;                   /* 0 none, 1 host, 2 client */
    int   n_sessions, n_race_starts, n_joins, n_leaves, humans_max, bots_max;
    int   n_bot_releases, n_manifest_ok, n_content_refused, n_content_differ;
    int   n_errors, n_warnings, n_refused, n_grids, n_client_sync, n_crash;
    int   n_net_samples, net_worst_ms, net_stale_max, net_never_max;
    int   drive_at_cs;            /* host DRIVE session time (last race), -1 none */
    int   last_humans, last_type, last_phase, lines;
    char  head[F1C_USAGE_HEAD][F1C_USAGE_LINE];
    int   head_n;
    char  tail[F1C_USAGE_TAIL][F1C_USAGE_LINE];
    int   tail_n, tail_pos;
} CoopUsage;

static CoopUsage      g_coopu;
static F1cUsageEvents g_coopu_ev;   /* schema 2 timeline (USAGE_REPORTS_V2_SPEC) */
static volatile LONG  g_coopu_lock;

static int coopu_starts(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

/* integer after "key=" in s, or fallback. */
static long coopu_field(const char *s, const char *key, long fallback)
{
    const char *k = strstr(s, key);
    if (!k) return fallback;
    k += strlen(key);
    if (!((*k >= '0' && *k <= '9') || *k == '-')) return fallback;
    return strtol(k, NULL, 10);
}

/* A token is kept only if it is: a COOP-E/COOP-W code; a fixed lowercase word
 * (a-z, '_', '-', ':', '(', ')', '>', '/', '.', at most one '.', no digits mixed
 * with a dot run that could be an address); a number; or key=number. Anything
 * with an uppercase letter (names), a backslash/colon path, or 3+ dots (IP
 * addresses) becomes <s>. */
static int coopu_number(const char *t, size_t n)
{
    size_t i = 0, dots = 0;
    int digits = 0;
    if (n && (t[0] == '-' || t[0] == '+')) i = 1;
    if (n >= i + 2 && t[i] == '0' && (t[i + 1] == 'x' || t[i + 1] == 'X')) {
        for (i += 2; i < n; i++) {
            char c = t[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return 0;
            digits++;
        }
        return digits > 0;
    }
    for (; i < n; i++) {
        char c = t[i];
        if (c >= '0' && c <= '9') digits++;
        else if (c == '.') dots++;
        else if (c != '%' && c != ',' && c != '/') return 0;
    }
    return digits > 0 && dots <= 1;
}

static int coopu_word(const char *t, size_t n)
{
    size_t i, dots = 0;
    if (!n || n > 24) return 0;
    for (i = 0; i < n; i++) {
        char c = t[i];
        if (c >= 'a' && c <= 'z') continue;
        if (c == '.') { dots++; continue; }   /* host names/addresses: refused below */
        if (c == '_' || c == '-' || c == ':' || c == '(' || c == ')' || c == '>' || c == '/' || c == '|' || c == '+')
            continue;
        return 0;
    }
    return dots == 0 || (dots == 1 && t[n - 1] == '.');   /* "word." ok, "a.b" (host names) never */
}

static int coopu_keep(const char *t, size_t n)
{
    const char *eq;
    if (n <= 24 && (strncmp(t, "COOP-E", 6) == 0 || strncmp(t, "COOP-W", 6) == 0)) return 1;
    if (coopu_number(t, n) || coopu_word(t, n)) return 1;
    eq = (const char *)memchr(t, '=', n);
    if (eq && eq > t && n <= 40) {
        size_t k = (size_t)(eq - t), v = n - k - 1;
        size_t i;
        if (!k || k > 24 || !v || !coopu_number(eq + 1, v)) return 0;
        for (i = 0; i < k; i++)   /* config keys keep case (ExpectedHumans=2); value must be a number */
            if (!((t[i] >= 'a' && t[i] <= 'z') || (t[i] >= 'A' && t[i] <= 'Z') || t[i] == '_' ||
                  (i && t[i] >= '0' && t[i] <= '9'))) return 0;
        return 1;
    }
    return 0;
}

static void coopu_sanitize(char *dst, const char *src)
{
    size_t o = 0;
    int last_dropped = 0;
    while (*src == ' ') src++;
    while (*src && o + 8 < F1C_USAGE_LINE) {
        const char *e = src;
        size_t n;
        while (*e && *e != ' ') e++;
        n = (size_t)(e - src);
        if (coopu_keep(src, n)) {
            if (o) dst[o++] = ' ';
            if (o + n >= F1C_USAGE_LINE - 1) n = F1C_USAGE_LINE - 1 - o;
            memcpy(dst + o, src, n); o += n;
            last_dropped = 0;
        } else if (!last_dropped) {   /* one <s> per run of dropped tokens */
            if (o + 4 >= F1C_USAGE_LINE - 1) break;
            if (o) dst[o++] = ' ';
            memcpy(dst + o, "<s>", 3); o += 3;
            last_dropped = 1;
        }
        src = e;
        while (*src == ' ') src++;
    }
    dst[o] = '\0';
    for (o = 0; dst[o]; o++)
        if ((unsigned char)dst[o] < 0x20 || (unsigned char)dst[o] > 0x7e ||
            dst[o] == '"' || dst[o] == '\\') dst[o] = ' ';
}

static void coopu_keep_line(const char *s)
{
    char clean[F1C_USAGE_LINE];
    if (coopu_starts(s, "ACCESS VIOLATION first-chance")) {   /* f1c_usage_exc_line: fixed words + numbers */
        lstrcpynA(clean, s, F1C_USAGE_LINE);
        for (size_t i = 0; clean[i]; i++) if (clean[i] == '"' || clean[i] == '\\') clean[i] = ' ';
    } else {
        coopu_sanitize(clean, s);
    }
    if (!clean[0]) return;
    g_coopu.lines++;
    if (g_coopu.head_n < F1C_USAGE_HEAD) {
        memcpy(g_coopu.head[g_coopu.head_n++], clean, F1C_USAGE_LINE);
    } else {
        memcpy(g_coopu.tail[g_coopu.tail_pos], clean, F1C_USAGE_LINE);
        g_coopu.tail_pos = (g_coopu.tail_pos + 1) % F1C_USAGE_TAIL;
        if (g_coopu.tail_n < F1C_USAGE_TAIL) g_coopu.tail_n++;
    }
}

static void coopu_reset_locked(void)
{
    memset(&g_coopu, 0, sizeof g_coopu);
    g_coopu.drive_at_cs = -1;
    g_coopu.last_humans = -1;
    g_coopu.last_type = -100;
    g_coopu.last_phase = -100;
}

static void coopu_tap(const char *s, int debug_only)
{
    if (!s || !*s) return;
    f1c_usage_lock(&g_coopu_lock);
    if (!g_coopu.active) {
        coopu_reset_locked();
        g_coopu.active = 1;
        g_coopu.start_tick = GetTickCount();
        f1c_usage_events_reset(&g_coopu_ev);
    }
    f1c_usage_ev_line(&g_coopu_ev, s);
    if (coopu_starts(s, "session change:")) {
        long type = coopu_field(s, "session_type=", -1), phase = coopu_field(s, "phase=", -1);
        long net = coopu_field(s, "net_state=", 0), coop = coopu_field(s, "coop=", 0);
        if (coop && net > 0) {
            if (!g_coopu.role) g_coopu.role = (net == 3) ? 2 : 1;
            if (type != g_coopu.last_type) g_coopu.n_sessions++;
            if (type == 7 && phase == 5 && g_coopu.last_phase != 5) g_coopu.n_race_starts++;
        }
        g_coopu.last_type = (int)type;
        g_coopu.last_phase = (int)phase;
    } else if (coopu_starts(s, "human count changed:")) {
        long h = coopu_field(s, "humans=", -1);
        if (h >= 0) {
            if (g_coopu.last_humans >= 0 && h > g_coopu.last_humans) g_coopu.n_joins += (int)(h - g_coopu.last_humans);
            if (g_coopu.last_humans >= 0 && h < g_coopu.last_humans) g_coopu.n_leaves += (int)(g_coopu.last_humans - h);
            if (h > g_coopu.humans_max) g_coopu.humans_max = (int)h;
            g_coopu.last_humans = (int)h;
        }
    } else if (coopu_starts(s, "prepared selected co-op field:") || coopu_starts(s, "client field from host manifest:")) {
        long b = coopu_field(s, " ai=", -1);
        if (b < 0) b = coopu_field(s, "active_bots=", -1);
        if (b > g_coopu.bots_max) g_coopu.bots_max = (int)b;
    } else if (coopu_starts(s, "released bot")) {
        g_coopu.n_bot_releases++;
    } else if (coopu_starts(s, "host manifest verified")) {
        g_coopu.n_manifest_ok++;
    } else if (coopu_starts(s, "content check:")) {
        if (coopu_field(s, "differ=", 0) > 0) g_coopu.n_content_differ++;
    } else if (coopu_starts(s, "race grid:")) {
        g_coopu.n_grids++;
    } else if (coopu_starts(s, "start timer: host DRIVE at session_time=")) {
        g_coopu.drive_at_cs = (int)(atof(s + strlen("start timer: host DRIVE at session_time=")) * 100.0);
    } else if (coopu_starts(s, "start timer (client): countdown set")) {
        g_coopu.n_client_sync++;
    } else if (coopu_starts(s, "net rx (client)")) {
        long worst = coopu_field(s, "worst=", 0), stale = coopu_field(s, "stale>1s=", 0);
        long never = coopu_field(s, "never=", 0);
        g_coopu.n_net_samples++;
        if (worst > g_coopu.net_worst_ms) g_coopu.net_worst_ms = (int)worst;
        if (stale > g_coopu.net_stale_max) g_coopu.net_stale_max = (int)stale;
        if (never > g_coopu.net_never_max) g_coopu.net_never_max = (int)never;
    }
    if (strstr(s, "COOP-E10 client field refused")) g_coopu.n_content_refused++;
    if (strstr(s, "COOP-E")) g_coopu.n_errors++;
    if (strstr(s, "COOP-W")) g_coopu.n_warnings++;
    if (strstr(s, "refused")) g_coopu.n_refused++;
    if (f1c_usage_is_crash(s, NULL)) g_coopu.n_crash++;
    if (!debug_only) coopu_keep_line(s);
    f1c_usage_unlock(&g_coopu_lock);
}

/* Builds and writes the summary. Lock must NOT be held. Only sessions that
 * reached a CO-OP multiplayer session (or crashed) are written. */
static void coopu_write_locked_out(const char *why, int skip_if_busy)
{
    static char buf[F1C_USAGE_FILE_CAP];
    static char mid[F1C_USAGE_FILE_CAP / 2 + 1024];
    char dir[MAX_PATH], w[24];
    SYSTEMTIME st;
    DWORD secs;
    size_t o;

    if (skip_if_busy) {
        if (InterlockedCompareExchange(&g_coopu_lock, 1, 0)) return;   /* process exit: never wait */
    } else {
        f1c_usage_lock(&g_coopu_lock);
    }
    if (!g_coopu.active) { f1c_usage_unlock(&g_coopu_lock); return; }
    secs = (GetTickCount() - g_coopu.start_tick) / 1000u;
    if (!g_coopu.n_sessions && !g_coopu.n_crash) {   /* menu only: nothing to report */
        f1c_usage_unlock(&g_coopu_lock);
        return;
    }
    if (!f1c_usage_dir(dir) || f1c_usage_count(dir, COOPU_MOD_ID) >= F1C_USAGE_MAX_FILES) {
        coopu_reset_locked();
        f1c_usage_unlock(&g_coopu_lock);
        return;
    }
    f1c_usage_why(w, why ? why : "end");
    GetSystemTime(&st);
    _snprintf(mid, sizeof mid,
              "\"role\":\"%s\",\"counts\":{\"sessions\":%d,\"race_starts\":%d,\"joins\":%d,\"leaves\":%d,"
              "\"humans_max\":%d,\"bots_max\":%d,\"bot_releases\":%d,\"manifest_ok\":%d,"
              "\"content_differ\":%d,\"content_refused\":%d,\"grids\":%d,\"client_countdown_sync\":%d,"
              "\"errors\":%d,\"warnings\":%d,\"refused\":%d,\"crash\":%d,\"lines\":%d},"
              "\"net\":{\"samples\":%d,\"worst_ms\":%d,\"stale_max\":%d,\"never_max\":%d},"
              "\"start\":{\"drive_at_s\":%.2f},",
              g_coopu.role == 1 ? "host" : g_coopu.role == 2 ? "client" : "none",
              g_coopu.n_sessions, g_coopu.n_race_starts, g_coopu.n_joins, g_coopu.n_leaves,
              g_coopu.humans_max, g_coopu.bots_max, g_coopu.n_bot_releases, g_coopu.n_manifest_ok,
              g_coopu.n_content_differ, g_coopu.n_content_refused, g_coopu.n_grids, g_coopu.n_client_sync,
              g_coopu.n_errors, g_coopu.n_warnings, g_coopu.n_refused, g_coopu.n_crash, g_coopu.lines,
              g_coopu.n_net_samples, g_coopu.net_worst_ms, g_coopu.net_stale_max, g_coopu.net_never_max,
              g_coopu.drive_at_cs >= 0 ? g_coopu.drive_at_cs / 100.0 : -1.0);
    mid[sizeof mid - 1] = '\0';
    {
        size_t m = strlen(mid);
        f1c_usage_events_json(&g_coopu_ev, mid + m, sizeof mid - m);
    }
    o = f1c_usage_render(buf, sizeof buf, COOPU_MOD_ID, COOPU_MOD_VERSION, w,
                         "", (unsigned long)secs, &st, mid,
                         (const char *)g_coopu.head, g_coopu.head_n,
                         (const char *)g_coopu.tail, g_coopu.tail_n, g_coopu.tail_pos);
    coopu_reset_locked();
    f1c_usage_events_reset(&g_coopu_ev);
    f1c_usage_unlock(&g_coopu_lock);
    if (o + 2 >= sizeof buf) return;
    f1c_usage_mark_v2(buf);
    f1c_usage_write(dir, COOPU_MOD_ID, &st, buf, o,
                    (unsigned long)(GetTickCount() ^ (DWORD)(uintptr_t)&o));
}

static void coopu_flush(const char *why) { coopu_write_locked_out(why, 0); }

/* game thread: request only; the worker writes. */
static volatile LONG g_coopu_request;
static void coopu_request(const char *) { InterlockedExchange(&g_coopu_request, 1); }
static void coopu_pump(void)
{
    if (InterlockedExchange(&g_coopu_request, 0)) coopu_flush("session_end");
}

/* Crash summary, once per process, via the shared VEH rule in f1c_usage.h. */
static volatile LONG g_coopu_crash_done;
static volatile LONG *g_coopu_track_flag = NULL;   /* set by coop_mode.cpp */
static void coopu_crash(void)
{
    char l[F1C_USAGE_LINE];
    if (InterlockedCompareExchange(&g_coopu_crash_done, 1, 0) != 0) return;
    f1c_usage_exc_line(l, sizeof l);
    coopu_tap(l, 0);
    if (g_coopu_track_flag)   /* 2026-10-10: crash inside RemoteCarTracking's native waypoint query? */
        coopu_tap(*g_coopu_track_flag ? "crash in remote car tracking query 1"
                                      : "crash in remote car tracking query 0", 0);
    coopu_flush("exception");
}
static void coopu_veh_install(void)
{
    f1c_usage_crash_identity(COOPU_MOD_ID, COOPU_MOD_VERSION);   /* 2026-10-11 crash detail + minidump */
    f1c_usage_veh_install(coopu_crash);
}
static void coopu_veh_remove(void) { f1c_usage_veh_remove(); }

/* DllMain PROCESS_DETACH: the game closed without the mod unloading (normal quit
 * or the unexplained silent exit). Never waits for the lock. */
static void coopu_exit(void) { coopu_write_locked_out("process_exit", 1); }

#endif /* COOP_USAGE_H */
