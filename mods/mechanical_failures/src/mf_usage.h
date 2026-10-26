/* mf_usage.h - per-session usage summary for opted-in diagnostics (release 1.0.0).
 *
 * Owner 2026-10-06: "those who have given consent on sending bug reports, make
 * it so that the mod automatically sends mod use logs". The DLL never sends
 * anything. It writes ONE small JSON file per finished session to
 *     <game>\plugins\lightloader\reports\usage\
 * and Lightloader (2.2.2+) uploads it only when the player's bug-report switch
 * is on; otherwise the files stay local (at most MFU_MAX_FILES, then no more
 * are written).
 *
 * Privacy by construction: no paths, no Windows user name, no player name.
 * Every log line passes mfu_sanitize(): a token containing '\' or '/' or ':'
 * becomes "<path>", lines that name the player profile (.PLR) or a .veh/.ini
 * file are not kept at all. Only mod event lines are kept (not the 5 s model
 * telemetry), first MFU_HEAD and last MFU_TAIL of them, each cut to 160 chars.
 *
 * Usage: logf_ calls mfu_tap(line) after formatting; engine_invalidate and the
 * worker exit call mfu_flush(why). Thread-safe through a spin lock (logf_ runs
 * on the worker and on hook threads).
 */
#ifndef MF_USAGE_H
#define MF_USAGE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "f1c_usage.h"

#define MF_MOD_ID        "mechanical_failures"
#define MF_MOD_VERSION   "1.0.7"

#define MFU_HEAD         F1C_USAGE_HEAD
#define MFU_TAIL         F1C_USAGE_TAIL
#define MFU_LINE         F1C_USAGE_LINE
#define MFU_MAX_FILES    F1C_USAGE_MAX_FILES
#define MFU_MIN_SECONDS  60      /* shorter sessions are not reported */
#define MFU_FILE_CAP     F1C_USAGE_FILE_CAP

typedef struct {
    int  active;                 /* a live session was seen               */
    DWORD start_tick;
    int  session_type;
    int  fail_rate;
    int  ai_on, overrev_on, minor_on, fx_pct, voice_on;
    int  n_onset, n_dead, n_blowup, n_voice, n_radio, n_repair, n_warn_hud;
    int  n_ai_onset, n_ai_terminal, n_ai_retired_other, n_ai_pulloff;
    int  n_problem;              /* refused / mismatch / missing / dropped */
    int  n_lines;                /* notable lines seen                     */
    char faults[256];            /* "oilleak,rad,..." player onsets        */
    char head[MFU_HEAD][MFU_LINE];
    int  head_n;
    char tail[MFU_TAIL][MFU_LINE];
    int  tail_n, tail_pos;
} MfUsage;

static MfUsage       g_mfu;
static volatile LONG g_mfu_lock = 0;

static void mfu_lock(void)   { f1c_usage_lock(&g_mfu_lock); }
static void mfu_unlock(void) { f1c_usage_unlock(&g_mfu_lock); }

static int mfu_starts(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

static int mfu_has_ci(const char *s, const char *n)
{
    size_t nl = strlen(n);
    for (; *s; s++)
        if (_strnicmp(s, n, nl) == 0) return 1;
    return 0;
}

/* telemetry noise and lines that would carry identifying names */
static int mfu_skip(const char *s)
{
    static const char *noise[] = {
        "MODEL 5s", "TEMPS 5s", "DMG:", "GRADE SRC", "GRADE:", "AI: slot",
        "AI LAW", "AI PULLOFF DIAG", "STATE ", "      corners", "AUDIO: loaded",
        "commentary_say", "TEAM QUALITY",
        "IMPACT PROBE",   /* 2026-10-07: log-only probe, 14 % of all uploaded lines */
        NULL };
    int i;
    for (i = 0; noise[i]; i++)
        if (mfu_starts(s, noise[i])) {
            /* AI onset/terminal lines are counted, kept below */
            if (mfu_starts(s, "AI: slot")
                && (strstr(s, " onset") || strstr(s, " terminal"))) return 0;
            return 1;
        }
    if (mfu_has_ci(s, ".plr") || mfu_has_ci(s, ".veh") || mfu_has_ci(s, ".ini")
        || mfu_has_ci(s, "\\save\\"))
        return 1;
    return 0;
}

/* copy with every path-like token replaced by <path>, cut to MFU_LINE */
static void mfu_sanitize(char *dst, const char *src)
{
    size_t o = 0;
    while (*src && o + 8 < MFU_LINE) {
        const char *e = src;
        int pathy = 0;
        while (*e && *e != ' ') {
            if (*e == '\\' || *e == '/' || (*e == ':' && e[1] && e[1] != ' ')) pathy = 1;
            e++;
        }
        if (pathy) {
            memcpy(dst + o, "<path>", 6); o += 6;
        } else {
            size_t n = (size_t)(e - src);
            if (o + n >= MFU_LINE - 1) n = MFU_LINE - 1 - o;
            memcpy(dst + o, src, n); o += n;
        }
        src = e;
        while (*src == ' ' && o + 1 < MFU_LINE) { dst[o++] = ' '; src++; }
    }
    dst[o] = '\0';
    for (o = 0; dst[o]; o++)
        if ((unsigned char)dst[o] < 0x20 || dst[o] == '"' || dst[o] == '\\') dst[o] = ' ';
}

static void mfu_fault_add(const char *line)
{
    const char *f = strstr(line, "fault=");
    char name[24];
    int i = 0;
    if (!f) return;
    f += 6;
    while (*f && *f != ' ' && *f != '(' && i < 23) name[i++] = *f++;
    name[i] = '\0';
    if (!i) return;
    if (strlen(g_mfu.faults) + strlen(name) + 2 < sizeof g_mfu.faults) {
        if (g_mfu.faults[0]) strcat(g_mfu.faults, ",");
        strcat(g_mfu.faults, name);
    }
}

/* called by logf_ with the formatted line (no newline) */
static void mfu_tap(const char *s)
{
    char clean[MFU_LINE];
    if (!s || !*s) return;
    mfu_lock();
    if (mfu_starts(s, "MODEL: session live")) {
        if (!g_mfu.active) { g_mfu.active = 1; g_mfu.start_tick = GetTickCount(); }
    }
    if (!g_mfu.active) { mfu_unlock(); return; }
    /* 2026-10-09 (report review): every player onset path counts - the
     * hazard is the default since Q3 and was never counted (live 1.0.3:
     * n_onset 0, faults empty while onsets ran). "ONSET: engine COLD" is a
     * note, not an onset. */
    if (mfu_starts(s, "ONSET: countdown") || mfu_starts(s, "ONSET: hazard")
        || mfu_starts(s, "ONSET: impact")) { g_mfu.n_onset++; mfu_fault_add(s); }
    else if (mfu_starts(s, "DEAD: latched")) g_mfu.n_dead++;
    else if (mfu_starts(s, "MODEL: BLOW UP")) g_mfu.n_blowup++;
    else if (mfu_starts(s, "VOICE: MF_")) g_mfu.n_voice++;
    else if (mfu_starts(s, "RADIO REQ")) g_mfu.n_radio++;
    else if (mfu_starts(s, "REPAIR: life")) g_mfu.n_repair++;
    else if (mfu_starts(s, "HUD")) g_mfu.n_warn_hud++;
    /* 2026-10-09: the AI onset line is "AI: slot N ONSET: ..." (upper case);
     * the old " onset" match hit terminal lines ("... onset yes") instead. */
    else if (mfu_starts(s, "AI: slot") && strstr(s, " ONSET: ")) g_mfu.n_ai_onset++;
    else if (mfu_starts(s, "AI: slot") && strstr(s, " terminal ")) g_mfu.n_ai_terminal++;
    else if (mfu_starts(s, "AI: slot") && strstr(s, "not an engine death")) g_mfu.n_ai_retired_other++;
    else if (mfu_starts(s, "AI PULLOFF: slot")) g_mfu.n_ai_pulloff++;
    /* 2026-10-09: guarded suppression and routine cleanup are not problems
     * ("RADIO DROPPED (not on circuit|engine finished|not driving)", "AUDIO:
     * off the circuit - N request(s) dropped, voices silenced"). */
    if (mfu_starts(s, "RADIO DROPPED (") || mfu_starts(s, "AUDIO: off the circuit")
        || mfu_has_ci(s, "restore dropped ("))   /* guarded stale-generation / moved restores */
        ;
    else if (mfu_has_ci(s, "refused") || mfu_has_ci(s, "mismatch") || mfu_has_ci(s, "missing")
        || mfu_has_ci(s, "dropped") || mfu_has_ci(s, "queue full") || mfu_has_ci(s, "cannot"))
        g_mfu.n_problem++;
    if (!mfu_skip(s)) {
        mfu_sanitize(clean, s);
        g_mfu.n_lines++;
        if (g_mfu.head_n < MFU_HEAD) {
            memcpy(g_mfu.head[g_mfu.head_n++], clean, MFU_LINE);
        } else {
            memcpy(g_mfu.tail[g_mfu.tail_pos], clean, MFU_LINE);
            g_mfu.tail_pos = (g_mfu.tail_pos + 1) % MFU_TAIL;
            if (g_mfu.tail_n < MFU_TAIL) g_mfu.tail_n++;
        }
    }
    mfu_unlock();
}

static void mfu_settings(int type, int rate, int ai, int ovr, int minor, int fx, int voice)
{
    mfu_lock();
    g_mfu.session_type = type; g_mfu.fail_rate = rate;
    g_mfu.ai_on = ai; g_mfu.overrev_on = ovr; g_mfu.minor_on = minor;
    g_mfu.fx_pct = fx; g_mfu.voice_on = voice;
    mfu_unlock();
}

static int mfu_dir(char *dir)
{
    return f1c_usage_dir(dir);
}

static int mfu_count_files(const char *dir)
{
    return f1c_usage_count(dir, MF_MOD_ID);
}

/* write the summary (if the session was long enough) and reset */
static void mfu_flush(const char *why)
{
    static char buf[MFU_FILE_CAP];
    char dir[MAX_PATH], w[24];
    SYSTEMTIME st;
    DWORD secs;
    size_t o;

    mfu_lock();
    if (!g_mfu.active) { mfu_unlock(); return; }
    secs = (GetTickCount() - g_mfu.start_tick) / 1000u;
    if (secs < MFU_MIN_SECONDS || !mfu_dir(dir) || mfu_count_files(dir) >= MFU_MAX_FILES) {
        int keep_ai = g_mfu.ai_on, keep_o = g_mfu.overrev_on, keep_m = g_mfu.minor_on,
            keep_fx = g_mfu.fx_pct, keep_v = g_mfu.voice_on;
        memset(&g_mfu, 0, sizeof g_mfu);
        g_mfu.ai_on = keep_ai; g_mfu.overrev_on = keep_o; g_mfu.minor_on = keep_m;
        g_mfu.fx_pct = keep_fx; g_mfu.voice_on = keep_v;
        mfu_unlock();
        return;
    }
    f1c_usage_why(w, why);
    GetSystemTime(&st);
    {
        char pre[64], mid[768];
        size_t po = 0, mo = 0;
#define MFU_PRE(...) do { int k_ = _snprintf(pre + po, sizeof pre - po, __VA_ARGS__); \
                          if (k_ > 0 && (size_t)k_ < sizeof pre - po) po += (size_t)k_; } while (0)
#define MFU_MID(...) do { int k_ = _snprintf(mid + mo, sizeof mid - mo, __VA_ARGS__); \
                          if (k_ > 0 && (size_t)k_ < sizeof mid - mo) mo += (size_t)k_; } while (0)
        MFU_PRE("\"session_type\":%d,\"failure_rate\":%d,",
                g_mfu.session_type, g_mfu.fail_rate);
        MFU_MID("\"settings\":{\"ai\":%d,\"overrev\":%d,\"minor\":%d,\"effects\":%d,\"voice\":%d},",
                g_mfu.ai_on, g_mfu.overrev_on, g_mfu.minor_on, g_mfu.fx_pct, g_mfu.voice_on);
        MFU_MID("\"counts\":{\"onset\":%d,\"dead\":%d,\"blowup\":%d,\"voice\":%d,\"radio\":%d,"
                "\"repair\":%d,\"ai_onset\":%d,\"ai_terminal\":%d,\"ai_retired_other\":%d,"
                "\"ai_pulloff\":%d,\"problem\":%d,\"lines\":%d},\"faults\":\"%s\",",
                g_mfu.n_onset, g_mfu.n_dead, g_mfu.n_blowup, g_mfu.n_voice, g_mfu.n_radio,
                g_mfu.n_repair, g_mfu.n_ai_onset, g_mfu.n_ai_terminal, g_mfu.n_ai_retired_other,
                g_mfu.n_ai_pulloff, g_mfu.n_problem, g_mfu.n_lines, g_mfu.faults);
#undef MFU_PRE
#undef MFU_MID
        o = f1c_usage_render(buf, sizeof buf, MF_MOD_ID, MF_MOD_VERSION, w,
                             pre, (unsigned long)secs, &st, mid,
                             (const char *)g_mfu.head, g_mfu.head_n,
                             (const char *)g_mfu.tail, g_mfu.tail_n,
                             g_mfu.tail_pos);
    }
    {
        int keep_ai = g_mfu.ai_on, keep_o = g_mfu.overrev_on, keep_m = g_mfu.minor_on,
            keep_fx = g_mfu.fx_pct, keep_v = g_mfu.voice_on;
        memset(&g_mfu, 0, sizeof g_mfu);
        g_mfu.ai_on = keep_ai; g_mfu.overrev_on = keep_o; g_mfu.minor_on = keep_m;
        g_mfu.fx_pct = keep_fx; g_mfu.voice_on = keep_v;
    }
    mfu_unlock();
    if (o + 2 >= sizeof buf) return;   /* truncated: never write broken JSON */
    f1c_usage_write(dir, MF_MOD_ID, &st, buf, o,
                    (unsigned long)(GetTickCount() ^ (DWORD)(uintptr_t)&o));
}

#endif /* MF_USAGE_H */
