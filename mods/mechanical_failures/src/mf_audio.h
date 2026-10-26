/* mf_audio.h - private waveOut voices with the game's own level chain (E7).
 *
 * Approach copied (not code) from the pitstop mod's wheel_audio_spatial.h /
 * legacy_audio.h (release_candidates/pitstop_failures-v1.0.6-rc.4/source) and
 * its GAME_3D_AUDIO_MODEL_2026-09-28.md research:
 *  - the game mixes 2D Miles samples in software; a centred effect reaches a
 *    channel at 0.5 x its attenuated level, so class_gain scales that chain;
 *  - the player's sliders live in the newest <exe>\Save\<name>\<name>.PLR
 *    (read once; fallback 0.5 / 1.0);
 *  - each sound is scaled (16-bit PCM into a private buffer) and played on
 *    one of 4 private waveOut voices. The DEVICE VOLUME IS NEVER TOUCHED,
 *    and playback does not go through the game's Miles mixer (honest scope,
 *    handoff E7).
 *
 * Threading: mf_audio_play() may be called from any thread - it only queues
 * (thread-safe). mf_audio_pump() drains the queue, opens the devices and
 * plays, so waveOutOpen only ever runs on the family workers, never in
 * DllMain (loader-lock deadlock, see engine_failure.c ~L3104 note).
 * mf_audio_shutdown() must run before the DLL self-ejects: the WAVEHDRs and
 * PCM buffers live in our data segment.
 *
 * One self-contained static copy per translation unit (project convention:
 * the families share no hook site or state). MFA_FN marks every function
 * unused-safe so any family can include the header as-is.
 *
 * Diagnostics: register the family logger with mf_audio_set_logger() (e.g.
 * mf_audio_set_logger(logf_);) after the logger exists. The default is
 * silent - this also makes the header safe to force-include from a compile
 * line before the source defines its logger. */
#ifndef MF_AUDIO_H
#define MF_AUDIO_H

#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mf_coop.h"     /* 2026-10-10 CO-OP: local player slot */
#include "mf_spatial.h"   /* R3j: 3D placement maths (pure, unit-tested) */

#define MFA_FN      static __attribute__((unused))

/* ---- R3j camera tap (defined by engine_failure.c in the merged DLL) ------
 * The engine family installs a read-only detour on the camera transform
 * funnel 0x541CD0 (camera-funnel-541cd0-freecam-hook, freecam-render-path-
 * 541cd0-18c) and samples receiver+0x18C..0x194 = camera world x/y/z plus the
 * camera right axis from the rotation matrix at +0x15C. These are plain
 * globals so every family's header-only audio copy can read them; a stale or
 * missing sample falls back to the player car (no spatial effect). */
#ifdef MF_MERGED
extern volatile float mf_cam_x, mf_cam_y, mf_cam_z;
extern volatile float mf_cam_rx, mf_cam_ry, mf_cam_rz;
extern volatile unsigned mf_cam_ms;
extern volatile int mf_cam_valid;
#endif

#define MFA_VOICES  4       /* handoff E7: four private voices */
#define MFA_QUEUE   8       /* play requests in flight */
#define MFA_WAVS    48      /* cached WAVs per family (2026-10-07: 35 engineer lines) */

typedef void (*MfaLogFn)(const char *fmt, ...);
static MfaLogFn mfa_log_fn = NULL;

MFA_FN void mfa_log(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
MFA_FN void mfa_log(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    if (!mfa_log_fn) return;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    buf[sizeof buf - 1] = '\0';
    mfa_log_fn("%s", buf);
}

MFA_FN void mf_audio_set_logger(MfaLogFn fn)
{
    mfa_log_fn = fn;
}

typedef struct {
    char  path[MAX_PATH];
    float class_gain;       /* 1.0 normal; 2.0 = BLOW UP (+6 dB) */
    int   spatial;          /* R3j: 1 = place at (sx,sy,sz) in 3D */
    float sx, sy, sz;       /* R3j: source world position, metres */
    int   voice;            /* 2026-10-07: engineer line - Announcer Volume chain */
} MfaReq;

typedef struct {
    char           path[MAX_PATH];
    WAVEFORMATEX   fmt;
    unsigned char *data;
    DWORD          len;
    int            loaded;  /* 0 free, 1 loaded, -1 known bad */
} MfaWav;

static MfaReq            mfa_q[MFA_QUEUE];
static volatile LONG     mfa_qn = 0;
static CRITICAL_SECTION  mfa_cs;
static volatile LONG     mfa_cs_ok = 0;
static int               mfa_q_dropped = 0;
static int               mfa_ready = -1;   /* -1 idle, 1 up (pump is lazy) */
static int               mfa_plr_done = 0;
static float             mfa_sfx = 0.5f;   /* conservative fallback chain */
static float             mfa_ratio = 1.0f;
static float             mfa_user = 1.0f;  /* 2026-10-06 INI [Sound] EffectsVolume/100 */
static float             mfa_announcer = 0.5f; /* 2026-10-07 PLR Announcer Volume */
static float             mfa_voice_user = 1.0f; /* 2026-10-07 INI [Sound] VoiceVolume/100 */
/* 2026-10-07 owner: every mod sound only while the player is on the circuit.
 * Set by the family (engine: on_circuit). NULL = no gate. When it says no,
 * new requests are refused and the pump silences anything still playing. */
static int (*mfa_gate)(void) = NULL;
static HWAVEOUT          mfa_voice[MFA_VOICES];
static DWORD             mfa_rate[MFA_VOICES];
static int               mfa_chans[MFA_VOICES];   /* R3j: open format channels */
static WAVEHDR           mfa_hdr[MFA_VOICES];
static unsigned char    *mfa_pcm[MFA_VOICES];
static int               mfa_next = 0;
static MfaWav            mfa_wav[MFA_WAVS];

/* ---- R3j: chunked 3D voices --------------------------------------------
 * A spatial request plays on the SAME private waveOut devices but as a
 * sequence of MFA_CHUNK_MS stereo chunks: the level/pan/Doppler are
 * recomputed from the live camera on every refill, so a sound follows the
 * camera while it plays (owner 2026-10-01). Two ping-pong headers per voice;
 * a chunk is refilled only after the previous one is done. */
#define MFA_CHUNK_MS 100
typedef struct {
    int            on;
    MfaWav        *w;
    double         phase;        /* next source frame (fractional: Doppler) */
    float          level;        /* class chain, pre-spatial */
    float          sx, sy, sz;   /* source world position */
    double         dist_prev;    /* for the radial closing speed */
    DWORD          ms_prev;
    unsigned char *buf[2];
    DWORD          cap[2];       /* bytes allocated in buf[h] */
    WAVEHDR        hdr[2];
    int            written[2];
    int            ended;        /* source exhausted: stop after the tail */
} MfaSpatVoice;
static MfaSpatVoice mfa_sv[MFA_VOICES];

MFA_FN int mfa_voice_usable(int v);
MFA_FN int mfa_voice_open(int v, const WAVEFORMATEX *fmt);

/* Listener = the current camera when the engine family's tap is fresh,
 * else the player car itself (spatial effect collapses to full volume). */
MFA_FN void mfa_listener(float *x, float *y, float *z,
                         float *rx, float *ry, float *rz)
{
#ifdef MF_MERGED
    if (mf_cam_valid && (GetTickCount() - mf_cam_ms) < 1000u
        && mf_cam_x == mf_cam_x) {          /* NaN-safe: any finite sample */
        *x = mf_cam_x; *y = mf_cam_y; *z = mf_cam_z;
        *rx = mf_cam_rx; *ry = mf_cam_ry; *rz = mf_cam_rz;
        return;
    }
#endif
    *x = *(volatile float*)(mfc_player_base() + 0x34E0u);
    *y = *(volatile float*)(mfc_player_base() + 0x34E4u);
    *z = *(volatile float*)(mfc_player_base() + 0x34E8u);
    *rx = 1.0f; *ry = 0.0f; *rz = 0.0f;
}

#define MFA_S16(v) ((short)((v) > 32767.0f ? 32767.0f \
                            : ((v) < -32768.0f ? -32768.0f : (v))))

/* Fill one chunk; returns 1 when the source ended inside this chunk. */
MFA_FN int mfa_sp_fill(int v, int h)
{
    MfaSpatVoice *s = &mfa_sv[v];
    DWORD frames_out = (DWORD)((double)s->w->fmt.nSamplesPerSec
                               * MFA_CHUNK_MS / 1000.0);
    DWORD bytes = frames_out * 4u;
    unsigned char *pcm = s->buf[h];
    DWORD fi;
    int ended = 0;
    float lx, ly, lz, rx, ry, rz, att, pan, gl, gr, dol;
    double dist, closing = 0.0;
    DWORD now = GetTickCount();
    int src_nch = (int)s->w->fmt.nChannels;

    /* 1.0.7: the buffer is kept across sounds, and a voice can be reopened
     * at a higher rate (mf_engine_blow.wav is 44.1 kHz, the rest 22.05 kHz):
     * grow it, or the fill below writes past the end (crash at +0x75C4). */
    if (!pcm || s->cap[h] < bytes) {
        if (pcm) HeapFree(GetProcessHeap(), 0, pcm);
        s->buf[h] = NULL;
        s->cap[h] = 0;
        pcm = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, bytes);
        if (!pcm) return 1;
        s->buf[h] = pcm;
        s->cap[h] = bytes;
    }
    mfa_listener(&lx, &ly, &lz, &rx, &ry, &rz);
    dist = mf_sp_dist(s->sx, s->sy, s->sz, lx, ly, lz);
    if (dist < 0.0) dist = 0.0;
    if (s->dist_prev >= 0.0 && now > s->ms_prev) {
        double dt = (double)(now - s->ms_prev) / 1000.0;
        closing = (s->dist_prev - dist) / dt;    /* + = approaching */
    }
    s->dist_prev = dist;
    s->ms_prev = now;
    att = mf_sp_atten(dist);
    pan = mf_sp_pan(s->sx - lx, s->sy - ly, s->sz - lz, dist, rx, ry, rz);
    mf_sp_levels(att, pan, &gl, &gr);
    dol = mf_sp_doppler(closing);
    {
        double total = (double)(s->w->len / (2u * (DWORD)src_nch));
        gl *= s->level;
        gr *= s->level;
        for (fi = 0; fi < frames_out; fi++) {
            DWORD idx;
            short *out = (short*)(pcm + (size_t)fi * 4u);
            if (s->phase >= total) { ended = 1; break; }
            idx = (DWORD)s->phase;
            if (src_nch == 1) {
                short s0;
                memcpy(&s0, s->w->data + (size_t)idx * 2u, 2);
                out[0] = MFA_S16((float)s0 * gl);
                out[1] = MFA_S16((float)s0 * gr);
            } else {
                short s0, s1;
                memcpy(&s0, s->w->data + (size_t)idx * 4u, 2);
                memcpy(&s1, s->w->data + (size_t)idx * 4u + 2u, 2);
                out[0] = MFA_S16((float)s0 * gl);
                out[1] = MFA_S16((float)s1 * gr);
            }
            s->phase += (double)dol;
        }
        if (fi < frames_out)
            memset(pcm + (size_t)fi * 4u, 0, (size_t)(frames_out - fi) * 4u);
    }
    memset(&s->hdr[h], 0, sizeof s->hdr[h]);
    s->hdr[h].lpData = (LPSTR)pcm;
    s->hdr[h].dwBufferLength = bytes;
    if (waveOutPrepareHeader(mfa_voice[v], &s->hdr[h], sizeof s->hdr[h])
        != MMSYSERR_NOERROR
        || waveOutWrite(mfa_voice[v], &s->hdr[h], sizeof s->hdr[h])
        != MMSYSERR_NOERROR) {
        if (s->hdr[h].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(mfa_voice[v], &s->hdr[h], sizeof s->hdr[h]);
        memset(&s->hdr[h], 0, sizeof s->hdr[h]);
        mfa_log("AUDIO: 3D chunk write failed (voice %d)", v);
        return 1;
    }
    s->written[h] = 1;
    return ended;
}

MFA_FN void mfa_sp_stop(int v)
{
    MfaSpatVoice *s = &mfa_sv[v];
    int h;
    if (mfa_voice[v]) waveOutReset(mfa_voice[v]);
    for (h = 0; h < 2; h++) {
        if (mfa_voice[v] && (s->hdr[h].dwFlags & WHDR_PREPARED))
            waveOutUnprepareHeader(mfa_voice[v], &s->hdr[h], sizeof s->hdr[h]);
        memset(&s->hdr[h], 0, sizeof s->hdr[h]);
        s->written[h] = 0;
    }
    s->on = 0;
    s->ended = 0;
}

MFA_FN void mfa_sp_start(MfaWav *w, float level, float sx, float sy, float sz)
{
    int v = -1, i;

    for (i = 0; i < MFA_VOICES; i++) {
        int c = (mfa_next + i) % MFA_VOICES;
        if (!mfa_sv[c].on && mfa_voice_usable(c)) { v = c; break; }
    }
    if (v < 0) {
        v = mfa_next;
        if (mfa_sv[v].on) mfa_sp_stop(v);
        else if (mfa_voice[v]) waveOutReset(mfa_voice[v]);
    }
    mfa_next = (v + 1) % MFA_VOICES;
    mfa_sp_stop(v);                       /* clear any residue, keep buffers */
    {
        MfaSpatVoice *s = &mfa_sv[v];
        WAVEFORMATEX f = w->fmt;
        f.nChannels = 2;                  /* pan needs two channels */
        f.nBlockAlign = 4;
        f.nAvgBytesPerSec = f.nSamplesPerSec * 4u;
        f.cbSize = 0;
        if (!mfa_voice_open(v, &f)) return;
        s->w = w;
        s->phase = 0.0;
        s->level = level;
        s->sx = sx; s->sy = sy; s->sz = sz;
        s->dist_prev = -1.0;
        s->ms_prev = GetTickCount();
        s->ended = 0;
        s->on = 1;
    }
}

/* Every pump: refill finished chunks with fresh camera placement. */
MFA_FN void mfa_sp_pump(void)
{
    int v, h;
    for (v = 0; v < MFA_VOICES; v++) {
        MfaSpatVoice *s = &mfa_sv[v];
        if (!s->on) continue;
        if (!mfa_voice[v]) { s->on = 0; continue; }
        for (h = 0; h < 2; h++) {
            if (!s->written[h]) continue;
            if (s->hdr[h].dwFlags & WHDR_DONE) {
                if (s->hdr[h].dwFlags & WHDR_PREPARED)
                    waveOutUnprepareHeader(mfa_voice[v], &s->hdr[h],
                                           sizeof s->hdr[h]);
                memset(&s->hdr[h], 0, sizeof s->hdr[h]);
                s->written[h] = 0;
            }
        }
        for (h = 0; h < 2 && !s->ended; h++) {
            if (s->written[h]) continue;
            if (mfa_sp_fill(v, h)) s->ended = 1;
        }
        if (s->ended && !s->written[0] && !s->written[1]) mfa_sp_stop(v);
    }
}

/* ------------------------------------------------------------------ */
/* WAV loading (minimal RIFF reader; 16-bit PCM only)                  */
/* ------------------------------------------------------------------ */

MFA_FN int mfa_wav_load(const char *path, MfaWav *w)
{
    HANDLE f;
    DWORD size, got = 0, p;
    unsigned char *buf;
    int have_fmt = 0;

    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    size = GetFileSize(f, NULL);
    if (size == INVALID_FILE_SIZE || size < 44 || size > 8u * 1024u * 1024u) {
        CloseHandle(f);
        return 0;
    }
    buf = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, size);
    if (!buf || !ReadFile(f, buf, size, &got, NULL) || got != size) {
        CloseHandle(f);
        if (buf) HeapFree(GetProcessHeap(), 0, buf);
        return 0;
    }
    CloseHandle(f);
    if (memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        HeapFree(GetProcessHeap(), 0, buf);
        return 0;
    }
    p = 12;
    while (p + 8 <= size) {
        DWORD csz;
        memcpy(&csz, buf + p + 4, 4);
        if (csz > size - p - 8) break;
        if (memcmp(buf + p, "fmt ", 4) == 0 && csz >= 16) {
            memset(&w->fmt, 0, sizeof w->fmt);
            memcpy(&w->fmt, buf + p + 8,
                   csz < sizeof w->fmt ? csz : sizeof w->fmt);
            w->fmt.cbSize = 0;
            have_fmt = 1;
        } else if (memcmp(buf + p, "data", 4) == 0 && !w->data) {
            DWORD dsz = csz;
            unsigned char *d;
            if (p + 8 + dsz > size) dsz = size - (p + 8);
            d = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, dsz);
            if (!d) break;
            memcpy(d, buf + p + 8, dsz);
            w->data = d;
            w->len = dsz;
        }
        p += 8 + csz + (csz & 1u);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    if (!have_fmt || !w->data || !w->len
        || w->fmt.wFormatTag != WAVE_FORMAT_PCM
        || w->fmt.wBitsPerSample != 16
        || w->fmt.nChannels < 1 || w->fmt.nChannels > 2
        || w->fmt.nBlockAlign != w->fmt.nChannels * 2
        || (w->len % w->fmt.nBlockAlign) != 0) {
        if (w->data) { HeapFree(GetProcessHeap(), 0, w->data); w->data = NULL; }
        return 0;
    }
    w->loaded = 1;
    lstrcpynA(w->path, path, MAX_PATH);
    return 1;
}

MFA_FN MfaWav *mfa_wav_get(const char *path)
{
    int i, slot = -1;
    for (i = 0; i < MFA_WAVS; i++) {
        if (mfa_wav[i].loaded != 0 && lstrcmpA(mfa_wav[i].path, path) == 0)
            return mfa_wav[i].loaded == 1 ? &mfa_wav[i] : NULL;
        if (slot < 0 && mfa_wav[i].loaded == 0) slot = i;
    }
    if (slot < 0) {
        mfa_log("AUDIO: wav cache full, skipped %s", path);
        return NULL;
    }
    if (!mfa_wav_load(path, &mfa_wav[slot])) {
        lstrcpynA(mfa_wav[slot].path, path, MAX_PATH);
        mfa_wav[slot].loaded = -1;      /* known bad: do not retry every pop */
        mfa_log("AUDIO: cannot load %s", path);
        return NULL;
    }
    mfa_log("AUDIO: loaded %s (%lu Hz, %u ch)",
                 path, (unsigned long)mfa_wav[slot].fmt.nSamplesPerSec,
                 (unsigned)mfa_wav[slot].fmt.nChannels);
    return &mfa_wav[slot];
}

/* ------------------------------------------------------------------ */
/* The player's slider chain: newest .PLR once (handoff E7)            */
/* ------------------------------------------------------------------ */

/* one "Key=\"value\"" line; each key independent (pitstop parse approach) */
MFA_FN int mfa_plr_value(const char *buf, DWORD len, const char *key,
                         float max, float *out)
{
    char needle[64];
    size_t klen, at;

    _snprintf(needle, sizeof needle, "%s=\"", key);
    needle[sizeof needle - 1] = '\0';
    klen = strlen(needle);
    if (klen >= (size_t)len) return 0;
    for (at = 0; at <= (size_t)len - klen; at++) {
        char *end = NULL;
        double v;
        if (at > 0 && buf[at - 1] != '\n') continue;
        if (memcmp(buf + at, needle, klen) != 0) continue;
        v = strtod(buf + at + klen, &end);
        if (end == buf + at + klen || !end || *end != '"') return 0;
        if (!isfinite(v) || v < 0.0 || v > (double)max) return 0;
        *out = (float)v;
        return 1;
    }
    return 0;
}

MFA_FN void mfa_plr_parse(const char *path)
{
    HANDLE f;
    DWORD size, got = 0;
    char *buf;
    float sfx = 0.5f, ratio = 1.0f;    /* fallback: 0.5 / 1.0 */
    float ann = 0.5f;

    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(f, NULL);
    if (size == INVALID_FILE_SIZE || size > 4u * 1024u * 1024u) {
        CloseHandle(f);
        return;
    }
    buf = (char*)HeapAlloc(GetProcessHeap(), 0, (size_t)size + 1);
    if (!buf) { CloseHandle(f); return; }
    if (!ReadFile(f, buf, size, &got, NULL) || got != size) {
        CloseHandle(f);
        HeapFree(GetProcessHeap(), 0, buf);
        return;
    }
    CloseHandle(f);
    buf[size] = '\0';
    mfa_plr_value(buf, size, "SoundFX Volume", 1.0f, &sfx);
    mfa_plr_value(buf, size, "Player Volume Ratio", 4.0f, &ratio);
    mfa_plr_value(buf, size, "Announcer Volume", 1.0f, &ann);
    mfa_sfx = sfx;
    mfa_ratio = ratio;
    mfa_announcer = ann;
    mfa_log("AUDIO: %s - SoundFX %.3f x Player %.2f (class 1 level %.3f), Announcer %.3f",
                 path, mfa_sfx, mfa_ratio, mfa_sfx * mfa_ratio * 0.5, mfa_announcer);
    HeapFree(GetProcessHeap(), 0, buf);
}

/* newest <exe>\Save\<name>\<name>.PLR; once, on the worker */
MFA_FN void mfa_plr_read_once(void)
{
    char exe[MAX_PATH], pat[MAX_PATH], best_path[MAX_PATH], *slash;
    WIN32_FIND_DATAA fd, fd2;
    HANDLE h, h2;
    FILETIME best;
    DWORD n;

    if (mfa_plr_done) return;
    mfa_plr_done = 1;
    best.dwLowDateTime = 0;
    best.dwHighDateTime = 0;
    best_path[0] = '\0';

    n = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    slash = strrchr(exe, '\\');
    if (!slash) return;
    *slash = '\0';

    _snprintf(pat, sizeof pat, "%s\\Save\\*", exe);
    pat[sizeof pat - 1] = '\0';
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        mfa_log("AUDIO: no %s\\Save - fallback level", exe);
        return;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == '.') continue;
        _snprintf(pat, sizeof pat, "%s\\Save\\%s\\*.PLR", exe, fd.cFileName);
        pat[sizeof pat - 1] = '\0';
        h2 = FindFirstFileA(pat, &fd2);
        if (h2 == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (CompareFileTime(&fd2.ftLastWriteTime, &best) > 0) {
                best = fd2.ftLastWriteTime;
                _snprintf(best_path, sizeof best_path, "%s\\Save\\%s\\%s",
                          exe, fd.cFileName, fd2.cFileName);
                best_path[sizeof best_path - 1] = '\0';
            }
        } while (FindNextFileA(h2, &fd2));
        FindClose(h2);
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    if (!best_path[0]) {
        mfa_log("AUDIO: no .PLR found - fallback level 0.5 x 1.0");
        return;
    }
    mfa_plr_parse(best_path);
}

/* ------------------------------------------------------------------ */
/* Voices                                                              */
/* ------------------------------------------------------------------ */

MFA_FN int mfa_voice_usable(int v)
{
    if (!mfa_voice[v]) return 1;                     /* not opened yet */
    if (!(mfa_hdr[v].dwFlags & WHDR_PREPARED)) return 1;
    return (mfa_hdr[v].dwFlags & WHDR_DONE) != 0;
}

MFA_FN void mfa_voice_free(int v)
{
    if (mfa_voice[v]) {
        waveOutReset(mfa_voice[v]);
        if (mfa_hdr[v].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(mfa_voice[v], &mfa_hdr[v], sizeof mfa_hdr[v]);
        if (mfa_sv[v].hdr[0].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(mfa_voice[v], &mfa_sv[v].hdr[0],
                                   sizeof mfa_sv[v].hdr[0]);
        if (mfa_sv[v].hdr[1].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(mfa_voice[v], &mfa_sv[v].hdr[1],
                                   sizeof mfa_sv[v].hdr[1]);
        waveOutClose(mfa_voice[v]);
        mfa_voice[v] = NULL;
    }
    if (mfa_pcm[v]) { HeapFree(GetProcessHeap(), 0, mfa_pcm[v]); mfa_pcm[v] = NULL; }
    memset(&mfa_hdr[v], 0, sizeof mfa_hdr[v]);
    memset(mfa_sv[v].hdr, 0, sizeof mfa_sv[v].hdr);
    mfa_sv[v].written[0] = mfa_sv[v].written[1] = 0;
    mfa_sv[v].on = 0;
    mfa_rate[v] = 0;
    mfa_chans[v] = 0;
}

MFA_FN int mfa_voice_open(int v, const WAVEFORMATEX *fmt)
{
    if (mfa_voice[v] && mfa_rate[v] == fmt->nSamplesPerSec
        && mfa_chans[v] == (int)fmt->nChannels) return 1;
    mfa_voice_free(v);
    if (waveOutOpen(&mfa_voice[v], WAVE_MAPPER, fmt, 0, 0, CALLBACK_NULL)
        != MMSYSERR_NOERROR) {
        mfa_voice[v] = NULL;
        mfa_log("AUDIO: waveOutOpen failed (voice %d, %lu Hz)",
                     v, (unsigned long)fmt->nSamplesPerSec);
        return 0;
    }
    mfa_rate[v] = fmt->nSamplesPerSec;
    mfa_chans[v] = (int)fmt->nChannels;
    return 1;
}

/* level chain: SoundFX Volume x Player Volume Ratio x 0.5 x class_gain
 * x the INI EffectsVolume, clamped to full scale (handoff E7) */
MFA_FN float mfa_level(float class_gain)
{
    double level = (double)mfa_sfx * (double)mfa_ratio * 0.5
                 * (double)class_gain * (double)mfa_user;
    if (!isfinite(level) || level <= 0.0) return 0.0f;
    return level > 1.0 ? 1.0f : (float)level;
}

/* 2026-10-07: engineer lines follow the game's Announcer Volume slider x the
 * INI VoiceVolume (the clips are already levelled to -9 dBFS like the
 * driver-name clips), not the effects chain. */
MFA_FN float mfa_voice_level(float class_gain)
{
    double level = (double)mfa_announcer * (double)class_gain
                 * (double)mfa_voice_user;
    if (!isfinite(level) || level <= 0.0) return 0.0f;
    return level > 1.0 ? 1.0f : (float)level;
}

MFA_FN void mfa_play_wav(MfaWav *w, float level)
{
    int v = -1, i;
    unsigned char *pcm;
    DWORD at;

    /* prefer an idle voice already opened at this rate; else any idle; else
     * steal the oldest (which stops it) - never a voice that is playing a
     * spatial sound (mfa_sp_pump owns that one) */
    for (i = 0; i < MFA_VOICES; i++) {
        int c = (mfa_next + i) % MFA_VOICES;
        if (mfa_sv[c].on) continue;
        if (mfa_voice_usable(c) && mfa_rate[c] == w->fmt.nSamplesPerSec) {
            v = c;
            break;
        }
    }
    if (v < 0)
        for (i = 0; i < MFA_VOICES; i++) {
            int c = (mfa_next + i) % MFA_VOICES;
            if (mfa_sv[c].on) continue;
            if (mfa_voice_usable(c)) { v = c; break; }
        }
    if (v < 0) {
        for (i = 0; i < MFA_VOICES; i++) {
            int c = (mfa_next + i) % MFA_VOICES;
            if (!mfa_sv[c].on) { v = c; break; }
        }
        if (v < 0) v = mfa_next;   /* all spatial: drop this classic request */
        if (mfa_sv[v].on) return;
        if (mfa_voice[v]) waveOutReset(mfa_voice[v]);   /* steal: stop it */
    }
    mfa_next = (v + 1) % MFA_VOICES;

    if (!mfa_voice_open(v, &w->fmt)) return;
    if (mfa_hdr[v].dwFlags & WHDR_PREPARED) {
        if (waveOutUnprepareHeader(mfa_voice[v], &mfa_hdr[v], sizeof mfa_hdr[v])
            != MMSYSERR_NOERROR) return;
        memset(&mfa_hdr[v], 0, sizeof mfa_hdr[v]);
    }
    if (mfa_pcm[v]) { HeapFree(GetProcessHeap(), 0, mfa_pcm[v]); mfa_pcm[v] = NULL; }

    /* scale into a private PCM buffer; the device volume stays untouched */
    pcm = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, w->len);
    if (!pcm) return;
    for (at = 0; at + 1 < w->len; at += 2) {
        short s;
        float f;
        memcpy(&s, w->data + at, 2);
        f = (float)s * level;
        if (f > 32767.0f) f = 32767.0f;
        if (f < -32768.0f) f = -32768.0f;
        s = (short)f;
        memcpy(pcm + at, &s, 2);
    }
    mfa_pcm[v] = pcm;

    memset(&mfa_hdr[v], 0, sizeof mfa_hdr[v]);
    mfa_hdr[v].lpData = (LPSTR)pcm;
    mfa_hdr[v].dwBufferLength = w->len;
    if (waveOutPrepareHeader(mfa_voice[v], &mfa_hdr[v], sizeof mfa_hdr[v])
        != MMSYSERR_NOERROR
        || waveOutWrite(mfa_voice[v], &mfa_hdr[v], sizeof mfa_hdr[v])
        != MMSYSERR_NOERROR) {
        if (mfa_hdr[v].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(mfa_voice[v], &mfa_hdr[v], sizeof mfa_hdr[v]);
        memset(&mfa_hdr[v], 0, sizeof mfa_hdr[v]);
        HeapFree(GetProcessHeap(), 0, mfa_pcm[v]);
        mfa_pcm[v] = NULL;
        mfa_log("AUDIO: waveOut prepare/write failed (voice %d)", v);
    }
}

/* ------------------------------------------------------------------ */
/* Public entry points                                                 */
/* ------------------------------------------------------------------ */

/* Queue a sound. Any thread; the worker's pump() does the real work. */
MFA_FN void mf_audio_play_common2(const char *path, float class_gain,
                                  int spatial, float sx, float sy, float sz,
                                  int voice)
{
    if (!path || !path[0]) return;
    if (mfa_gate && !mfa_gate()) return;
    if (!mfa_cs_ok) {
        if (InterlockedExchange(&mfa_cs_ok, 2) == 0)
            InitializeCriticalSection(&mfa_cs);
        else while (mfa_cs_ok != 2) Sleep(0);
    }
    EnterCriticalSection(&mfa_cs);
    if (mfa_qn < MFA_QUEUE) {
        lstrcpynA(mfa_q[mfa_qn].path, path, MAX_PATH);
        mfa_q[mfa_qn].class_gain = class_gain;
        mfa_q[mfa_qn].spatial = spatial;
        mfa_q[mfa_qn].sx = sx;
        mfa_q[mfa_qn].sy = sy;
        mfa_q[mfa_qn].sz = sz;
        mfa_q[mfa_qn].voice = voice;
        mfa_qn++;
    } else {
        mfa_q_dropped++;
    }
    LeaveCriticalSection(&mfa_cs);
}

MFA_FN void mf_audio_play_common(const char *path, float class_gain,
                                 int spatial, float sx, float sy, float sz)
{
    mf_audio_play_common2(path, class_gain, spatial, sx, sy, sz, 0);
}

/* 2026-10-07: an engineer radio line from a loose WAV (no SDT edit) */
MFA_FN void mf_audio_play_voice(const char *path, float class_gain)
{
    mf_audio_play_common2(path, class_gain, 0, 0.0f, 0.0f, 0.0f, 1);
}

MFA_FN void mf_audio_play(const char *path, float class_gain)
{
    mf_audio_play_common(path, class_gain, 0, 0.0f, 0.0f, 0.0f);
}

/* R3j: play at a world position (the failing car). The listener is the live
 * camera; level/pan/Doppler are re-evaluated every 100 ms chunk. */
MFA_FN void mf_audio_play_pos(const char *path, float class_gain,
                              float sx, float sy, float sz)
{
    mf_audio_play_common(path, class_gain, 1, sx, sy, sz);
}

/* Worker thread, once per tick: lazy PLR read, then drain and play. */
MFA_FN void mf_audio_pump(void)
{
    MfaReq req[MFA_QUEUE];
    int n = 0, i, dropped = 0;

    if (mfa_ready < 0) {
        mfa_ready = 1;
        mfa_plr_read_once();
        mfa_log("AUDIO: ready (%d voices, class chain %.3f x %.2f)",
                     MFA_VOICES, mfa_sfx, mfa_ratio);
    }
    if (!mfa_cs_ok) return;

    EnterCriticalSection(&mfa_cs);
    dropped = mfa_q_dropped;
    mfa_q_dropped = 0;
    n = (int)mfa_qn;
    if (n > MFA_QUEUE) n = MFA_QUEUE;
    for (i = 0; i < n; i++) req[i] = mfa_q[i];
    mfa_qn = 0;
    LeaveCriticalSection(&mfa_cs);

    if (dropped)
        mfa_log("AUDIO: queue full - %d request(s) dropped", dropped);

    {
        static int silenced = 0;
        if (mfa_gate && !mfa_gate()) {     /* off the circuit: nothing plays */
            int v;
            if (!silenced) {
                for (v = 0; v < MFA_VOICES; v++) {
                    if (mfa_sv[v].on) mfa_sp_stop(v);
                    else if (mfa_voice[v]) waveOutReset(mfa_voice[v]);
                }
                silenced = 1;
                mfa_log("AUDIO: off the circuit - %d request(s) dropped, voices silenced", n);
            }
            return;
        }
        silenced = 0;
    }

    for (i = 0; i < n; i++) {
        MfaWav *w = mfa_wav_get(req[i].path);
        float level;
        if (!w) continue;
        level = req[i].voice ? mfa_voice_level(req[i].class_gain)
                             : mfa_level(req[i].class_gain);
        if (level <= 0.0f) continue;
        if (req[i].spatial)
            mfa_sp_start(w, level, req[i].sx, req[i].sy, req[i].sz);
        else
            mfa_play_wav(w, level);
    }
    mfa_sp_pump();
}

/* Worker teardown, before the DLL self-ejects (buffers live in our data). */
MFA_FN void mf_audio_shutdown(void)
{
    int v, i;

    if (mfa_cs_ok == 2) {
        EnterCriticalSection(&mfa_cs);
        mfa_qn = 0;
        mfa_q_dropped = 0;
        LeaveCriticalSection(&mfa_cs);
    }
    for (v = 0; v < MFA_VOICES; v++) {
        MfaSpatVoice *s = &mfa_sv[v];
        if (s->buf[0]) { HeapFree(GetProcessHeap(), 0, s->buf[0]); s->buf[0] = NULL; }
        if (s->buf[1]) { HeapFree(GetProcessHeap(), 0, s->buf[1]); s->buf[1] = NULL; }
        s->cap[0] = s->cap[1] = 0;
    }
    for (v = 0; v < MFA_VOICES; v++) mfa_voice_free(v);
    for (i = 0; i < MFA_WAVS; i++) {
        if (mfa_wav[i].data) {
            HeapFree(GetProcessHeap(), 0, mfa_wav[i].data);
            mfa_wav[i].data = NULL;
        }
        mfa_wav[i].loaded = 0;
        mfa_wav[i].path[0] = '\0';
    }
    mfa_ready = -1;
    mfa_plr_done = 0;
}

#endif /* MF_AUDIO_H */
