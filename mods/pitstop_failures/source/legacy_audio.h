#pragma once
#include <mmsystem.h>
#include "wheel_audio_spatial.h"
/* Extracted original polyphonic WAV implementation.
 * Source SHA256: 0e1233a31fe344d88ef8fbfd15b816fd6e818a412042b94a69fa595b5202e576
 * Bounds/PCM checks and host-relative asset path repaired; see reuse report. */
#define WAV_VOICES  8          /* shared pool - more than corners, so clips layer */
#define WAV_SLOTS   4          /* headers in flight per voice */

typedef struct {
    WAVEFORMATEX fmt;
    unsigned char* data;
    DWORD len;
    int loaded;
} Wav;

static Wav      g_wav[3];
static HWAVEOUT g_voice[WAV_VOICES];
static WAVEHDR  g_hdr[WAV_VOICES][WAV_SLOTS];
static unsigned char *g_play_pcm[WAV_VOICES][WAV_SLOTS];
static int      g_hdr_i[WAV_VOICES];
static int      g_voice_next;
static int      g_audio_ready;

/* Minimal RIFF/WAVE reader: locate 'fmt ' and 'data'. Good enough for the
 * shipped 16-bit PCM effects; anything else is logged and skipped. */
static int wav_load(const char* path, Wav* w) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { log_w("WAV: cannot open %s", path); return 0; }
    DWORD size = GetFileSize(f, NULL);
    if (size < 44 || size > 8u * 1024u * 1024u) { CloseHandle(f); log_w("WAV: bad size %s", path); return 0; }
    unsigned char* buf = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, size);
    DWORD got = 0;
    if (!buf || !ReadFile(f, buf, size, &got, NULL) || got != size) {
        CloseHandle(f); if (buf) HeapFree(GetProcessHeap(), 0, buf);
        log_w("WAV: read failed %s", path); return 0;
    }
    CloseHandle(f);
    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) {
        HeapFree(GetProcessHeap(), 0, buf); log_w("WAV: not RIFF/WAVE %s", path); return 0;
    }
    DWORD p = 12; int have_fmt = 0;
    while (p + 8 <= size) {
        DWORD csz = *(DWORD*)(buf + p + 4);
        if (csz > size-p-8) break;
        if (!memcmp(buf + p, "fmt ", 4) && csz >= 16) {
            memcpy(&w->fmt, buf + p + 8, sizeof(PCMWAVEFORMAT));
            w->fmt.cbSize = 0;
            have_fmt = 1;
        } else if (!memcmp(buf + p, "data", 4) && !w->data) {
            if (p + 8 + csz > size) csz = size - (p + 8);
            w->data = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, csz);
            if (!w->data) break;
            memcpy(w->data, buf + p + 8, csz);
            w->len = csz;
        }
        p += 8 + csz + (csz & 1);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    if (!have_fmt || !w->data || !w->len || w->fmt.wFormatTag != WAVE_FORMAT_PCM || w->fmt.wBitsPerSample != 16 || w->fmt.nChannels < 1 || w->fmt.nChannels > 2 || w->fmt.nBlockAlign != w->fmt.nChannels*2 || w->len % w->fmt.nBlockAlign) { log_w("WAV: no fmt/data %s", path); return 0; }
    w->loaded = 1;
    return 1;
}

static void audio_init(const char* const* paths, int n) {
    if (g_audio_ready) return;
    int ok = 0;
    for (int i = 0; i < n && i < 3; i++) if (wav_load(paths[i], &g_wav[i])) ok++;
    if (!ok) { log_w("AUDIO: no WAVs loaded, SFX disabled"); g_audio_ready = -1; return; }

    const Wav* ref = NULL;
    for (int i = 0; i < 3; i++) if (g_wav[i].loaded) { ref = &g_wav[i]; break; }
    /* Every voice is opened with the first WAV's format; a replaced WAV with
     * another rate or channel count would play at the wrong pitch/speed. */
    for (int i = 0; i < 3; i++) {
        if (!g_wav[i].loaded || &g_wav[i] == ref) continue;
        if (g_wav[i].fmt.nSamplesPerSec != ref->fmt.nSamplesPerSec ||
            g_wav[i].fmt.nChannels != ref->fmt.nChannels) {
            log_w("AUDIO: wheelguns_%d.wav format differs from the first WAV; skipped", i + 1);
            g_wav[i].loaded = 0;
        }
    }
    for (int v = 0; v < WAV_VOICES; v++) {
        if (waveOutOpen(&g_voice[v], WAVE_MAPPER, (WAVEFORMATEX*)&ref->fmt,
                        0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            log_w("AUDIO: waveOutOpen failed for voice %d", v);
            g_voice[v] = NULL;
        }
    }
    g_audio_ready = 1;
    log_w("AUDIO: %d WAV(s), %dHz %dch, %d voices", ok,
          (int)ref->fmt.nSamplesPerSec, (int)ref->fmt.nChannels, WAV_VOICES);
}

/* Playing length of a loaded clip, 0 when unavailable. */
static float wav_seconds(int i) {
    if (g_audio_ready != 1 || i < 0 || i > 2 || !g_wav[i].loaded ||
        !g_wav[i].fmt.nAvgBytesPerSec) return 0;
    return (float)g_wav[i].len / (float)g_wav[i].fmt.nAvgBytesPerSec;
}

/* Fire one burst on the voice belonging to `corner`. Overlapping corners no
 * longer cut each other off; a corner cutting its own previous burst is fine
 * and is what a stuttering gun sounds like. */
static void play_burst(int corner, int which, float gain, unsigned source_slot,
                       double distance_m) {
    if (g_audio_ready != 1 || gain <= 0) return;

    /* Voices are a shared POOL, not one-per-corner: pick any voice whose last
     * buffer has finished, so clips layer over each other freely - including a
     * corner over itself. Falls back to round-robin (cutting the oldest) when
     * every voice is busy. */
    int v = -1;
    for (int i = 0; i < WAV_VOICES; i++) {
        int cand = (g_voice_next + i) % WAV_VOICES;
        if (!g_voice[cand]) continue;
        WAVEHDR* ph = &g_hdr[cand][(g_hdr_i[cand] + WAV_SLOTS - 1) % WAV_SLOTS];
        if (!(ph->dwFlags & WHDR_PREPARED) || (ph->dwFlags & WHDR_DONE)) { v = cand; break; }
    }
    if (v < 0) {
        for (int i = 0; i < WAV_VOICES; i++) {
            int cand = (g_voice_next + i) % WAV_VOICES;
            if (g_voice[cand]) { v = cand; break; }
        }
    }
    if (v < 0) return;
    g_voice_next = (v + 1) % WAV_VOICES;
    if (which < 0 || which > 2 || !g_wav[which].loaded) {
        for (which = 0; which < 3 && !g_wav[which].loaded; which++) ;
        if (which >= 3) return;
    }
    int header_slot=g_hdr_i[v];
    WAVEHDR* h = &g_hdr[v][header_slot];
    g_hdr_i[v] = (header_slot + 1) % WAV_SLOTS;
    if (h->dwFlags & WHDR_PREPARED) {
        if (!(h->dwFlags & WHDR_DONE)) { log_w("SFX: car %u corner %d slot busy, skipped", source_slot, corner); return; }
        if(waveOutUnprepareHeader(g_voice[v],h,sizeof(*h))!=MMSYSERR_NOERROR)return;
    }
    if(g_play_pcm[v][header_slot]) {
        HeapFree(GetProcessHeap(),0,g_play_pcm[v][header_slot]);
        g_play_pcm[v][header_slot]=NULL;
    }
    unsigned char *pcm=(unsigned char*)HeapAlloc(GetProcessHeap(),0,g_wav[which].len);
    if(!pcm || !pit_wheel_scale_pcm16(g_wav[which].data,pcm,g_wav[which].len,gain)) {
        if(pcm)HeapFree(GetProcessHeap(),0,pcm);
        log_w("SFX: car %u PCM allocation/scale failed",source_slot);
        return;
    }
    g_play_pcm[v][header_slot]=pcm;
    memset(h, 0, sizeof(*h));
    h->lpData = (LPSTR)pcm;
    h->dwBufferLength = g_wav[which].len;
    MMRESULT pr = waveOutPrepareHeader(g_voice[v], h, sizeof(*h));
    if (pr != MMSYSERR_NOERROR) {
        HeapFree(GetProcessHeap(),0,pcm);g_play_pcm[v][header_slot]=NULL;
        log_w("SFX: prepare failed car %u corner %d mmr=%d",source_slot,corner,(int)pr);
        return;
    }
    MMRESULT wr = waveOutWrite(g_voice[v], h, sizeof(*h));
    log_w("SFX: car %u corner %d wav %d distance %.1fm gain %.3f %s",
          source_slot,corner,which,distance_m,gain,
          wr==MMSYSERR_NOERROR?"OK":"WRITE FAILED");
    if(wr!=MMSYSERR_NOERROR &&
       waveOutUnprepareHeader(g_voice[v],h,sizeof(*h))==MMSYSERR_NOERROR) {
        HeapFree(GetProcessHeap(),0,pcm);g_play_pcm[v][header_slot]=NULL;
    }
}

static void audio_shutdown(void) {
    for (int v = 0; v < WAV_VOICES; v++) {
        if (!g_voice[v]) continue;
        waveOutReset(g_voice[v]);
        for (int s = 0; s < WAV_SLOTS; s++)
            if (g_hdr[v][s].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(g_voice[v], &g_hdr[v][s], sizeof(WAVEHDR));
        for (int s = 0; s < WAV_SLOTS; s++)
            if(g_play_pcm[v][s]) {
                HeapFree(GetProcessHeap(),0,g_play_pcm[v][s]);
                g_play_pcm[v][s]=NULL;
            }
        waveOutClose(g_voice[v]);
        g_voice[v] = NULL;
    }
    for (int i = 0; i < 3; i++)
        if (g_wav[i].data) { HeapFree(GetProcessHeap(), 0, g_wav[i].data); g_wav[i].data = NULL; }
    g_audio_ready = 0;
}

/* Development port: resolve assets from the host executable, never cwd. */
static char g_wg_paths[3][MAX_PATH];
static const char* WG_SOUNDS[] = { g_wg_paths[0], g_wg_paths[1], g_wg_paths[2] };
static int init_sound_paths(void) {
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    char* slash = strrchr(exe, '\\');
    if (!slash) return 0;
    *slash = '\0';
    for (int i = 0; i < 3; i++) {
        int written = snprintf(g_wg_paths[i], MAX_PATH,
            "%s\\Audio\\Vehicle\\wheelguns_%d.wav", exe, i + 1);
        if (written < 0 || written >= MAX_PATH) return 0;
    }
    return 1;
}

