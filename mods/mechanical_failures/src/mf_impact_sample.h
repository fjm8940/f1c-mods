/* AUDITFIX X1: value-only contact burst; callers serialize all access. */
#ifndef MF_IMPACT_SAMPLE_H
#define MF_IMPACT_SAMPLE_H
#include <string.h>
typedef struct {
    unsigned contacts;
    float peak_raw, peak, speed_kmh, engine_seize, aero_min;
    int damage_mult;
    double last_t;
} MfImpactSample;
static inline void mf_impact_add(MfImpactSample *s, float raw, float impulse,
                                 float speed, float engine, float aero,
                                 int damage_mult, double t)
{
    if (s->contacts && t < s->last_t) memset(s, 0, sizeof(*s));
    if (!s->contacts) s->speed_kmh = speed;
    if (raw > s->peak_raw) s->peak_raw = raw;
    if (!s->contacts || impulse > s->peak) {
        s->peak = impulse; s->engine_seize = engine;
        s->aero_min = aero; s->damage_mult = damage_mult;
    }
    if (s->contacts != ~0u) ++s->contacts;
    s->last_t = t;
}
static inline int mf_impact_take(MfImpactSample *s, double t, MfImpactSample *out)
{
    if (!s->contacts) return 0;
    /* Session rewind: discard the previous session's burst. */
    if (t < s->last_t) { memset(s, 0, sizeof(*s)); return 0; }
    if (t - s->last_t < 0.3) return 0;
    *out = *s; memset(s, 0, sizeof(*s)); return 1;
}
#endif
