/* mf_variance.h - per-failure variance (owner 2026-09-29).
 *
 * "Every failure should start and develop differently each time." Each armed
 * failure rolls one profile:
 *   kind   steady    - constant pace, small wobble
 *          worsening - starts slow, speeds up over 1-3 min
 *          on-off    - active spells 8-30 s, quiet spells 10-40 s
 *          fading    - starts strong, slows almost to a stop
 *   speed  0.55..1.75x, skewed toward 1x
 * mfv_mult() returns the current pace multiplier (1 = the old fixed pace).
 * Continuous failures multiply their rate by it; event failures divide
 * their gaps by it (mfv_gap()).
 *
 * mfv_publish() writes one line per family to mf_variance_<fam>.txt next to
 * the DLL so the test panel can show which variance is in place.
 * Header-only: every family is its own translation unit. */
#ifndef MF_VARIANCE_H
#define MF_VARIANCE_H

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

#define MFV_FN static __attribute__((unused))
#define MFV_STEADY 0
#define MFV_WORSE  1
#define MFV_ONOFF  2
#define MFV_FADE   3
#define MFV_ALL    0x0Fu
#define MFV_BIT(k) (1u << (k))

typedef struct {
    int   kind;
    float speed;
    float t;          /* seconds of progress since arm */
    float grow;       /* worsening: 1/s */
    float tau;        /* fading: s */
    float jit;
    DWORD jit_next;
    int   on;         /* on-off: 1 = active spell */
    DWORD sw_next;
} mfv_t;

static const char *const mfv_kind_txt[4] __attribute__((unused)) = { "steady", "worsening", "on-off", "fading" };

MFV_FN float mfv_r(void) { return (float)(rand() % 1000) / 1000.0f; }

/* weights steady 35 / worsening 30 / on-off 20 / fading 15, limited to allow */
MFV_FN void mfv_roll(mfv_t *v, unsigned allow)
{
    static const int w[4] = { 35, 30, 20, 15 };
    int total = 0, k, r;
    float s = mfv_r();
    if (!(allow & MFV_ALL)) allow = MFV_BIT(MFV_STEADY);
    for (k = 0; k < 4; k++) if (allow & MFV_BIT(k)) total += w[k];
    r = rand() % total;
    v->kind = MFV_STEADY;
    for (k = 0; k < 4; k++) {
        if (!(allow & MFV_BIT(k))) continue;
        if (r < w[k]) { v->kind = k; break; }
        r -= w[k];
    }
    v->speed = 0.55f + 1.2f * s * s;
    v->t = 0.0f;
    v->grow = 1.0f / (60.0f + 120.0f * mfv_r());
    v->tau = 40.0f + 80.0f * mfv_r();
    v->jit = 1.0f;
    v->jit_next = 0;
    v->on = 1;
    v->sw_next = 0;
}

MFV_FN float mfv_mult(mfv_t *v, DWORD now, float dt)
{
    float m = v->speed;
    v->t += dt;
    if (!v->jit_next || (long)(now - v->jit_next) >= 0) {
        v->jit = 0.85f + 0.3f * mfv_r();
        v->jit_next = now + 3000 + (DWORD)(rand() % 5001);
    }
    switch (v->kind) {
    case MFV_WORSE:
        m *= 0.5f + v->grow * v->t;
        if (m > 3.0f * v->speed) m = 3.0f * v->speed;
        break;
    case MFV_ONOFF:
        if (!v->sw_next) v->sw_next = now + 8000 + (DWORD)(rand() % 22001);
        else if ((long)(now - v->sw_next) >= 0) {
            v->on = !v->on;
            v->sw_next = now + (v->on ? 8000 + (DWORD)(rand() % 22001)
                                      : 10000 + (DWORD)(rand() % 30001));
        }
        m *= v->on ? 1.6f : 0.1f;
        break;
    case MFV_FADE:
        m *= 1.5f * (float)exp(-v->t / v->tau);
        if (m < 0.03f) m = 0.03f;
        break;
    }
    return m * v->jit;
}

/* event gap scaled by the current pace (faster pace = shorter gaps) */
MFV_FN DWORD mfv_gap(mfv_t *v, DWORD now, DWORD gap_ms)
{
    float m = mfv_mult(v, now, 0.0f);
    if (m < 0.05f) m = 0.05f;
    return (DWORD)((float)gap_ms / m);
}

MFV_FN void mfv_desc(const mfv_t *v, char *buf, int n)
{
    _snprintf(buf, n, "%s, speed %.2fx", mfv_kind_txt[v->kind & 3], v->speed);
    buf[n - 1] = '\0';
}

/* one line per family, next to the DLL; "-" = nothing armed */
MFV_FN void mfv_publish(const char *fam, const char *fmt, ...)
{
    char path[MAX_PATH], line[256], *slash;
    HMODULE h = NULL;
    FILE *f;
    va_list ap;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)(void*)&mfv_publish, &h)) return;
    if (!GetModuleFileNameA(h, path, MAX_PATH)) return;
    slash = strrchr(path, '\\');
    if (!slash) return;
    _snprintf(slash + 1, MAX_PATH - (int)(slash + 1 - path) - 1,
              "mf_variance_%s.txt", fam);
    path[MAX_PATH - 1] = '\0';
    va_start(ap, fmt);
    _vsnprintf(line, sizeof line - 1, fmt, ap);
    va_end(ap);
    line[sizeof line - 1] = '\0';
    f = fopen(path, "w");
    if (!f) return;
    fputs(line, f);
    fputs("\n", f);
    fclose(f);
}

#endif
