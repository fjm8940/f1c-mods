/* mf_spatial.h - 3D sound placement maths (R3j, ADDENDUM 11).
 *
 * Pure C, no game memory, no Windows: the same maths the audio pump runs is
 * unit-tested in tools/engine_model_test.c (test_r3j_spatial).
 *
 * Model copied from the game itself (docs in pitstop_modding/
 * docs/GAME_3D_AUDIO_MODEL_2026-09-28.md, function F_3DSoundPositioningUpdate
 * 0x405B90): inverse-distance attenuation with a full-volume radius, a pan
 * of (dot(dir, listener_right)*0.6 + 1) * 0.5 (so a centred source reaches
 * each channel at 0.5 x its attenuated level), and Doppler at 330 m/s.
 * A hard silent radius avoids audible far bursts (pitstop wheel_audio_spatial.h
 * approach, tools gate: attenuation/pan harness against this formula).
 *
 * Conventions: all positions in world metres; `right` is the listener's
 * right unit axis; dir = normalize(source - listener). Result gains are
 * pre-slider-chain linear factors. */
#ifndef MF_SPATIAL_H
#define MF_SPATIAL_H

#include <math.h>

#define MFSP_FN static __attribute__((unused))

/* full volume inside this radius (car-scale), then 1/d; smooth edge to a
 * hard cut at MF_SP_SILENT_M */
#define MF_SP_FULL_M      8.0
#define MF_SP_FADE_M      60.0
#define MF_SP_SILENT_M    120.0

#define MF_SP_PAN_MIN     0.2f    /* the game's own clamp */
#define MF_SP_PAN_MAX     0.8f

#define MF_SP_SOUND_MPS   330.0    /* F_3DSoundPositioningUpdate Doppler */
#define MF_SP_RATE_MIN    0.125
#define MF_SP_RATE_MAX    8.0

/* -1 when any input is not finite (caller treats as silent) */
MFSP_FN double mf_sp_dist(double sx, double sy, double sz,
                          double lx, double ly, double lz)
{
    double dx, dy, dz;
    if (!isfinite(sx) || !isfinite(sy) || !isfinite(sz)
        || !isfinite(lx) || !isfinite(ly) || !isfinite(lz))
        return -1.0;
    dx = sx - lx; dy = sy - ly; dz = sz - lz;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

/* inverse-distance with a full-volume radius + smoothstep edge to silence */
MFSP_FN float mf_sp_atten(double dist)
{
    double g;
    if (!isfinite(dist) || dist < 0.0 || dist >= MF_SP_SILENT_M) return 0.0f;
    if (dist <= MF_SP_FULL_M) return 1.0f;
    g = MF_SP_FULL_M / dist;
    if (dist > MF_SP_FADE_M) {
        double t = (MF_SP_SILENT_M - dist) / (MF_SP_SILENT_M - MF_SP_FADE_M);
        g *= t * t * (3.0 - 2.0 * t);
    }
    return (float)g;
}

/* (dot(dir, right)*0.6 + 1) / 2, clamped to the game's 0.2..0.8.
 * dir is the raw (not normalised) source-listener vector; dist > 0. */
MFSP_FN float mf_sp_pan(double dx, double dy, double dz, double dist,
                        double rx, double ry, double rz)
{
    double d, pan;
    if (!isfinite(dist) || dist <= 1e-9
        || !isfinite(dx) || !isfinite(dy) || !isfinite(dz)
        || !isfinite(rx) || !isfinite(ry) || !isfinite(rz))
        return 0.5f;
    d = (dx * rx + dy * ry + dz * rz) / dist;
    if (d > 1.0) d = 1.0;
    if (d < -1.0) d = -1.0;
    pan = (d * 0.6 + 1.0) * 0.5;
    if (pan < MF_SP_PAN_MIN) pan = MF_SP_PAN_MIN;
    if (pan > MF_SP_PAN_MAX) pan = MF_SP_PAN_MAX;
    return (float)pan;
}

/* per-channel level: centred source -> 0.5 x att each side (game model) */
MFSP_FN void mf_sp_levels(float att, float pan, float *left, float *right)
{
    if (left)  *left  = att * (1.0f - pan);
    if (right) *right = att * pan;
}

/* Doppler playback rate for a closing speed (positive = approaching).
 * 330 m/s, clamped 0.125..8 like the game's 0x405F80 path. */
MFSP_FN float mf_sp_doppler(double closing_mps)
{
    double r;
    if (!isfinite(closing_mps)) return 1.0f;
    if (closing_mps >= MF_SP_SOUND_MPS - 1.0) closing_mps = MF_SP_SOUND_MPS - 1.0;
    r = MF_SP_SOUND_MPS / (MF_SP_SOUND_MPS - closing_mps);
    if (r < MF_SP_RATE_MIN) r = MF_SP_RATE_MIN;
    if (r > MF_SP_RATE_MAX) r = MF_SP_RATE_MAX;
    return (float)r;
}

#endif
