/* mf_ai_lat.h - AI write stage 1 (2026-10-03, Claude): a commanded lateral offset for ONE AI car, fed through the
 * AI's own line chain so the AI keeps doing its own steering, braking and cornering.
 *
 * Spec: f1c_ida_decompiled/investigations/ai_brain_map/phase3/WRITE_STAGE1_SPEC_DRAFT.md (REVIEW_17 accepted).
 * Chain (finding ai-brain-4408-44b4-writers-closed-2026-10-02): the AISteering epilogue 0x4293F0 publishes
 *   0x4408 = 0x440C, then = 0x4420 if byte 0x4548, then = 0x4424 if non-zero (last wins); the steering law follows 0x4408.
 * The engine_failure.c call-site hook at 0x425403 (call AISteering) writes the command just before AISteering, i.e.
 * after AIRacingLine (which can write 0x4424 itself) and after AIMainUpdate's per-tick zeroing of 0x4420.
 *
 * This header is PURE (no game access): the decision per AI tick. engine_failure.c does the reads/writes.
 * Unit test: tools/ai_lat_test.c.  Client: tools/ai_line_cmd.py.  Never touches 0x45FC (self-pointer crash finding).
 */
#ifndef MF_AI_LAT_H
#define MF_AI_LAT_H

/* Shared memory: fields APPENDED to AiOverride (Local\MF_AI_OVERRIDE_V1, docs/AI_OVERRIDE_API.md). Byte offsets are
 * asserted against the struct in engine_failure.c and parsed by tools/ai_line_cmd.py - keep the three in sync. */
#define MF_LAT_OFF_ON       48   /* i32 client: 1 = command active                                   */
#define MF_LAT_OFF_TARGET   52   /* f32 client: target offset in metres (signed, - left / + right)   */
#define MF_LAT_OFF_MODE     56   /* i32 client: 1 = forced 0x4424, 2 = request 0x4548 + 0x4420       */
#define MF_LAT_OFF_RATE     60   /* f32 client: ramp m/s (0 = default 2.0, max 3.0; 1.0 above 120 km/h) */
#define MF_LAT_OFF_MAXOFF   64   /* f32 client: |offset| clamp m (0 = default 2.0, hard max 6.0)     */
#define MF_LAT_OFF_Q        68   /* f32 DLL: offset written this tick                                */
#define MF_LAT_OFF_PUB      72   /* f32 DLL: 0x4408 read back right after AISteering                 */
#define MF_LAT_OFF_POS      76   /* f32 DLL: 0x4428 measured lateral position                        */
#define MF_LAT_OFF_CALLS    80   /* u32 DLL: +1 per tick the latched car passed the hook             */
#define MF_LAT_OFF_STATE    84   /* i32 DLL: MF_LAT_IDLE / ACTIVE / RETURN / ABORTED                 */
#define MF_LAT_OFF_ABORT    88   /* i32 DLL: MF_LAT_AB_* reason of the last abort                    */
#define MF_LAT_OFF_SITE     92   /* u32 DLL: 1 = call-site hook installed (bytes verified)           */
#define MF_LAT_OFF_SLOTL    96   /* i32 DLL: slot latched by the active command (-1 = none)          */
#define MF_LAT_SHM_SIZE    100

#define MF_LAT_MODE_FORCED  1
#define MF_LAT_MODE_REQUEST 2

enum { MF_LAT_IDLE = 0, MF_LAT_ACTIVE = 1, MF_LAT_RETURN = 2, MF_LAT_ABORTED = 3 };
enum { MF_LAT_AB_NONE = 0, MF_LAT_AB_MODE = 1, MF_LAT_AB_COND = 2, MF_LAT_AB_CHAIN = 3, MF_LAT_AB_PIT = 4,
       MF_LAT_AB_RATE = 5, MF_LAT_AB_BADCMD = 6, MF_LAT_AB_SESSION = 7 };

#define MF_LAT_RATE_DEF   2.0f      /* m/s (spec section 3)            */
#define MF_LAT_RATE_MAX   3.0f
#define MF_LAT_RATE_MIN   0.1f
#define MF_LAT_RATE_FAST  1.0f      /* m/s above 120 km/h              */
#define MF_LAT_FAST_MS    33.333f   /* 120 km/h                        */
#define MF_LAT_MAX_DEF    2.0f      /* first owner steps: +-2 m        */
#define MF_LAT_MAX_HARD   6.0f
#define MF_LAT_DT_DEF     0.025f    /* one AI tick                     */
#define MF_LAT_DT_MAX     0.1f
#define MF_LAT_RATE_ABORT 6.0f      /* m/s lateral, or 0.9 x speed     */

typedef struct {
    float q;             /* current commanded offset (m)                     */
    int   state;         /* MF_LAT_*                                         */
    int   abort;         /* MF_LAT_AB_* of the last abort                    */
    int   mode;          /* mode latched at activation                       */
    float last_t;        /* session time of the previous tick (< 0 = none)   */
    float last_pos;      /* previous measured lateral position               */
    int   have_pos;
    int   wrote;         /* 1 = our value sits in the game fields now        */
    float last_written;  /* the last q we wrote                              */
} MfLatState;

typedef struct {
    int   on;            /* client command active                            */
    float target, rate, maxoff;
    int   mode;
    float now;           /* session time veh+0x4480 (s)                      */
    float speed;         /* veh+0x4440 (m/s)                                 */
    int   driver_status; /* veh+0x4510                                       */
    int   car_conditions;/* veh+0x3D6C                                       */
    int   pit_status;    /* veh+0x3450 (>= 1 = pit lane)                     */
    int   chain_ok;      /* veh+0x4354 non-null                              */
    int   session_ok;    /* session live, not frozen                         */
    float lat_pos;       /* veh+0x4428                                       */
} MfLatIn;

typedef struct {
    int   write;         /* 1 = write q into the chain this tick             */
    int   release;       /* 1 = clear our fields this tick                   */
    float q;
    int   mode;          /* which chain path to write / release              */
} MfLatOut;

static void mf_lat_init(MfLatState *s)
{
    s->q = 0.0f; s->state = MF_LAT_IDLE; s->abort = MF_LAT_AB_NONE; s->mode = 0;
    s->last_t = -1.0f; s->last_pos = 0.0f; s->have_pos = 0; s->wrote = 0; s->last_written = 0.0f;
}

static float mf_lat_clampf(float v, float lo, float hi)
{
    if (!(v >= lo)) return lo;           /* also catches NaN */
    if (v > hi) return hi;
    return v;
}

static float mf_lat_absf(float v) { return v < 0.0f ? -v : v; }

/* Immediate release: the game state machine owns the car (spec section 5 items 1-3, 6). */
static MfLatOut mf_lat_hard_abort(MfLatState *s, int reason)
{
    MfLatOut o = {0, 0, 0.0f, s->mode};
    if (s->state == MF_LAT_ACTIVE || s->state == MF_LAT_RETURN) {
        o.release = s->wrote;
        s->state = MF_LAT_ABORTED;
        s->abort = reason;
    }
    s->q = 0.0f;
    s->wrote = 0;
    return o;
}

static MfLatOut mf_lat_step(MfLatState *s, const MfLatIn *in)
{
    MfLatOut o = {0, 0, 0.0f, s->mode};
    float dt = MF_LAT_DT_DEF, rate, maxoff, desired, step;
    int valid;

    if (s->last_t >= 0.0f && in->now > s->last_t)
        dt = in->now - s->last_t;
    if (dt > MF_LAT_DT_MAX) dt = MF_LAT_DT_MAX;
    s->last_t = in->now;

    /* hard aborts: release now, do not fight the game */
    if (!in->session_ok)            { s->have_pos = 0; return mf_lat_hard_abort(s, MF_LAT_AB_SESSION); }
    if (in->driver_status != 0)     { s->have_pos = 0; return mf_lat_hard_abort(s, MF_LAT_AB_MODE); }
    if (in->car_conditions == 2)    { s->have_pos = 0; return mf_lat_hard_abort(s, MF_LAT_AB_COND); }
    if (!in->chain_ok)              { s->have_pos = 0; return mf_lat_hard_abort(s, MF_LAT_AB_CHAIN); }
    if (in->pit_status >= 1)        { s->have_pos = 0; return mf_lat_hard_abort(s, MF_LAT_AB_PIT); }

    if (s->state == MF_LAT_ABORTED) {           /* re-arm only after the client turns the command off */
        if (!in->on) { s->state = MF_LAT_IDLE; }
        s->have_pos = 0;
        return o;
    }

    valid = in->on && (in->mode == MF_LAT_MODE_FORCED || in->mode == MF_LAT_MODE_REQUEST)
            && in->target == in->target;        /* not NaN */
    if (in->on && !valid && s->state == MF_LAT_ACTIVE) { s->state = MF_LAT_RETURN; s->abort = MF_LAT_AB_BADCMD; }

    if (s->state == MF_LAT_IDLE) {
        if (!valid) { s->have_pos = 0; return o; }
        s->state = MF_LAT_ACTIVE;
        s->mode = in->mode;                     /* latched until back to idle */
        s->abort = MF_LAT_AB_NONE;
        s->q = 0.0f;
        s->have_pos = 0;
    }
    o.mode = s->mode;

    /* soft abort: lateral rate too high (spin / push) -> ramp back to 0 (spec section 5 item 5) */
    if (s->have_pos && dt > 0.0f && s->state == MF_LAT_ACTIVE) {
        float lim = in->speed * 0.9f;
        if (lim < MF_LAT_RATE_ABORT) lim = MF_LAT_RATE_ABORT;
        if (mf_lat_absf(in->lat_pos - s->last_pos) / dt > lim) {
            s->state = MF_LAT_RETURN;
            s->abort = MF_LAT_AB_RATE;
        }
    }
    s->last_pos = in->lat_pos;
    s->have_pos = 1;

    if (s->state == MF_LAT_ACTIVE && !in->on) s->state = MF_LAT_RETURN;

    rate = (in->rate > 0.0f) ? in->rate : MF_LAT_RATE_DEF;
    rate = mf_lat_clampf(rate, MF_LAT_RATE_MIN, MF_LAT_RATE_MAX);
    if (in->speed > MF_LAT_FAST_MS && rate > MF_LAT_RATE_FAST) rate = MF_LAT_RATE_FAST;
    maxoff = (in->maxoff > 0.0f) ? in->maxoff : MF_LAT_MAX_DEF;
    maxoff = mf_lat_clampf(maxoff, 0.0f, MF_LAT_MAX_HARD);

    desired = (s->state == MF_LAT_ACTIVE) ? mf_lat_clampf(in->target, -maxoff, maxoff) : 0.0f;
    step = rate * dt;
    if (desired > s->q + step)      s->q += step;
    else if (desired < s->q - step) s->q -= step;
    else                            s->q = desired;

    if (s->state == MF_LAT_RETURN && s->q == 0.0f) {
        o.release = s->wrote;                   /* back on its own line: hand the fields back */
        s->wrote = 0;
        s->state = (s->abort == MF_LAT_AB_NONE) ? MF_LAT_IDLE : MF_LAT_ABORTED;
        if (s->state == MF_LAT_ABORTED && !in->on) s->state = MF_LAT_IDLE;
        return o;
    }
    o.write = 1;
    o.q = s->q;
    s->wrote = 1;
    s->last_written = s->q;
    return o;
}

#endif
