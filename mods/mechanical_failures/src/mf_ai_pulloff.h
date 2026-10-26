/* mf_ai_pulloff.h - AI pull-off / "get off the line" (ADDENDUM 15, P2).
 *
 * Spec: docs/AI_PULLOFF_SYSTEM_SPEC_2026-10-02.md. Section 7 is the
 * implementation contract and SUPERSEDES the external-loop notes in
 * section 2 (owner decision 2026-10-02: option A, one writer in the DLL).
 * Behaviour sections 1.1 (dead engine pull-off) and 1.2 (ruined race ASIDE);
 * scanner section 6; control law 7.2.
 *
 * Pure C: no game addresses, no Windows, no globals. The DLL snapshots the
 * live waypoint chain/surface/traffic into MfPoWp/MfPoIn and calls
 * mf_po_step(); the returned MfPoOut is exactly what the AIMainUpdate
 * detour writes at 0x442C/0x3580 (steer), 0x4438/0x3584 (brake) and
 * 0x4434/0x357C (throttle). tools/ai_pulloff_test.c exercises every
 * transition with synthetic snapshots.
 *
 * Sign convention (Claude review fix 2026-10-02, replaces DS's mirrored
 * L=+1): lateral = dot(pos - wp_active_pos, wp_perp), the AISteering 0x4270A0
 * line 508 expression. Positive lateral = RIGHT: AISteering lines 775-786
 * extend the upper lateral bound to +wp+0x38 (wall R) and the lower bound to
 * -wp+0x34 (wall L); AIW files carry wp_perp = (fwd_z, -fwd_x) = right of
 * travel. So free_L = wallL + lat, free_R = wallR - lat, MF_PO_SIDE_L = -1,
 * MF_PO_SIDE_R = +1 (same as the live prototype pulloff7.py). A positive
 * steer command 0x442C turns toward +lateral (AISteeringRacingLineCorrection
 * 0x42B0E0 line 210 + Herbert v6 trial), so steer_add = k x (goal - lat).
 *
 * Numbering in comments = the spec section. Starting values only; the live
 * check list in the run record holds the untested items (traffic behaviour,
 * drift above 140 km/h, apex side sign). */
#ifndef MF_AI_PULLOFF_H
#define MF_AI_PULLOFF_H

#include "mf_engine_model.h"

/* ------------------------------------------------------------------ */
/* Constants (spec 1.1, 1.2, 6, 7.2)                                   */
/* ------------------------------------------------------------------ */

#define MF_PO_SIDE_L            (-1)   /* -  negative lateral = left        */
#define MF_PO_SIDE_R            1

#define MF_PO_NOTICE_MIN_S      0.3    /* s  driver reaction (spec 1.1.1)   */
#define MF_PO_NOTICE_MAX_S      1.0
#define MF_PO_COAST_KMH         1000.0 /* km/h: live 7 (Albers 274 km/h did
                                        * nothing for 2 s, then the AI's own
                                        * stop braked him on the line): the
                                        * drift starts at ANY speed (1 m/s
                                        * ramp > 120 km/h, speed-scaled
                                        * steer); was 140                   */
#define MF_PO_DRIFT_KMH_FAST    120.0  /* km/h: slower ramp above this      */
#define MF_PO_DRIFT_RATE_LO     2.0    /* m/s ramp (spec 1.1.5)             */
#define MF_PO_DRIFT_RATE_HI     1.0
#define MF_PO_ROOM_SAFE_M       4.0    /* m  run-off for a SAFE side (1.1.2)*/
#define MF_PO_HALF_CAR_M        0.9    /* m  wall clearance half width      */
#define MF_PO_WALL_MARGIN_M     0.5    /* m  owner's 0.5 margin (section 6) */
#define MF_PO_CAR_MIN_M         0.5    /* m  pause while car free < this    */
#define MF_PO_TRAFFIC_M         25.0   /* m  AI's own awareness range       */
#define MF_PO_TRAFFIC_BEHIND_M  5.0    /* m  long > this = not "behind"     */
#define MF_PO_TRAFFIC_PATH_M    3.0    /* m  "within 3 m of our path"       */
#define MF_PO_WAIT_MAX_S        4.0    /* s  TOTAL traffic wait per pull-off */
#define MF_PO_WAIT_MIN_MPS      8.33   /* m/s 30 km/h: below, no more waits
                                        * (live 2: Takagi stopped on line)  */
#define MF_PO_GOAL_GRASS_M      2.1    /* m  goal >= road edge + this       */
#define MF_PO_GOAL_GRASS_MAX_M  6.0    /* m  goal <= road edge + this       */
#define MF_PO_GOAL_ROOM_FRAC    0.5    /* -  (superseded by wall hug)       */
#define MF_PO_GOAL_WALL_M       1.0    /* m  min body gap to the AIW wall   */
#define MF_PO_GOAL_EDGE_M       3.0    /* m  option A (owner 2026-10-02):
                                        * goal = AIW road edge + this (AIW
                                        * edge error ~2 m live, run-off
                                        * asphalt = track code 6/21/27)     */
#define MF_PO_GOAL_STREET_M     1.2    /* m  street fallback road edge -    */
#define MF_PO_ASIDE_GOAL_M      1.5   /* m  ASIDE goal = road edge - this  */
#define MF_PO_ASIDE_RECENTER_M  1.0   /* m  goal < this -> re-center       */
#define MF_PO_ASIDE_RATE_M      0.5   /* m/s ASIDE ramp (spec 1.2)         */
/* MF 1.0.3 M4 (owner, Badoer crawl): state 8 ASIDE commands no brake BY
 * DESIGN ("brake/throttle untouched", spec 1.2) - but a ruined car that can
 * no longer keep pace must not sit aside for ever. Below this speed for this
 * long the driver gives up on the race: the ASIDE escalates into the normal
 * stop chain (COAST -> DRIFT -> ROLL -> PARKED, brakes and all). A slow
 * corner lasts seconds; a crawl does not. */
#define MF_PO_ASIDE_STALL_MPS   20.0  /* m/s 72 km/h stall floor           */
#define MF_PO_ASIDE_STALL_S     12.0  /* s  continuous below the floor     */
#define MF_PO_K_LAT             0.02   /* rad/m steer gain (7.2 start)      */
#define MF_PO_K_RATE            0.01   /* rad/(m/s) rate damping (7.2)      */
#define MF_PO_STEER_LIM         0.06   /* rad base additive clamp (7.2)     */
#define MF_PO_STEER_LIM_ASIDE   0.04   /* rad ASIDE clamp (starting value)  */
#define MF_PO_ABORT_RATE_MAX    6.0    /* m/s lateral rate abort floor (7.2)*/
#define MF_PO_ABORT_RATE_FRAC   0.9    /* x speed: only a slide (live 1 Hill
                                        * 7.9 m/s @162 km/h, live 4 Massa
                                        * 9.2 m/s @~70 km/h = ~30 deg turn-
                                        * out: both false aborts at 0.5)    */
#define MF_PO_AI_BRAKE_CAP      0.3    /* -  cap on the AI's own brake in
                                        * COAST/WAIT/DRIFT (live 5 Webber)  */
#define MF_PO_ROLL_BRAKE        0.05   /* -  roll-out brake (owner live 1:
                                        * full brake only at a halt)        */
#define MF_PO_ROLL_BRAKE_SLOW   0.15   /* -  roll-out brake below 15 km/h   */
#define MF_PO_ROLL_SLOW_MPS     4.17   /* m/s 15 km/h                       */
#define MF_PO_ROLL_K_HEAD       2.0    /* steer per sin(heading to +lat)    */
#define MF_PO_ROLL_K_LAT        0.03   /* steer per m lateral error         */
#define MF_PO_ROLL_STEER_MAX    0.4    /* absolute ROLL steer clamp         */
#define MF_PO_ABORT_HEAD_RAD    1.0472 /* rad 60 deg heading abort (7.2)    */
#define MF_PO_STOP_MPS          0.1389 /* m/s 0.5 km/h park threshold (P6)  */
#define MF_PO_LAT_WINDOW_S      0.15   /* s  min lateral-rate sample window */
#define MF_PO_CHOOSE_S          0.5    /* s  side re-check until drift      */
#define MF_PO_LOOKAHEAD_MAX     12     /* wp max look-ahead entries         */
#define MF_PO_LOOKAHEAD_MIN_M   30.0   /* m  minimum scan distance (sec. 6) */
#define MF_PO_STOP_DECEL        8.0    /* m/s2 stopping-distance decel(1.1)*/

/* states (spec 1.1/1.2 + P2: COAST/DRIFT/GOAL/PARKED/ABORT and ASIDE) */
#define MF_PO_IDLE      0
#define MF_PO_NOTICE    1
#define MF_PO_COAST     2
#define MF_PO_WAIT      3
#define MF_PO_DRIFT     4
#define MF_PO_GOAL      5
#define MF_PO_ABORT     6
#define MF_PO_PARKED    7
#define MF_PO_ASIDE     8
#define MF_PO_ROLL      9   /* live 1: off the line, hold lat, roll to a halt */

/* reason (log wording, P7) */
#define MF_PO_REASON_NONE     0
#define MF_PO_REASON_END      1   /* Option A ending, sudden/model terminal */
#define MF_PO_REASON_R4       2   /* R4 retire (also an Option A ending)    */
#define MF_PO_REASON_GAMEKILL 3   /* game-clock engine kill (ReasonCode 1)  */
#define MF_PO_REASON_ASIDE    4   /* R2 limp / ruined race                  */

/* abort reasons */
#define MF_PO_ABORT_NONE     0
#define MF_PO_ABORT_RATE     1
#define MF_PO_ABORT_HEAD     2

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

/* one snapshot of an in-memory AIW waypoint (316-byte record; offsets from
 * docs/52-aiw-navigation-static-architecture.md + the 2026-10-02 findings):
 *   +0x94/+0x9C active pos, +0x20/+0x28 wp_perp, +0xA4/+0xAC forward,
 *   +0x2C/0x30 road edge L/R, +0x34/0x38 wall L/R, +0x3C segment length. */
typedef struct {
    double pos_x, pos_z;
    double perp_x, perp_z;
    double fwd_x, fwd_z;
    double road_l, road_r;
    double wall_l, wall_r;
    double seg_len;
} MfPoWp;

/* one other car in our waypoint frame: positive long_m = ahead. */
typedef struct {
    double long_m;
    double lat_m;
} MfPoTraffic;

#define MF_PO_TRAFFIC_MAX 5

typedef struct {
    double lat;             /* true lateral, sign = MF_PO_SIDE_*        */
    int    lat_frame_ok;    /* 0 = waypoint changed: reset the rate     */
    double speed_mps;       /* physics speed from position deltas (P6)  */
    double heading_err_rad; /* |car fwd - current segment fwd|          */
    double heading_side;    /* dot(car fwd, wp perp): + = heading to +lat */
    double wall_free_l, wall_free_r; /* min over look-ahead - 0.9 m (6)  */
    double room_l, room_r;           /* min (wall - road) (spec 1.1.2)   */
    double car_free_l, car_free_r;   /* min gap - 1.8 m (section 6)      */
    double road_edge_l, road_edge_r; /* CURRENT waypoint (1.1.2)         */
    int    apex_side;       /* -1/+1 = next corner side, 0 straight     */
    int    wheels_known;    /* 0..4 (P4 surface chain)                  */
    int    wheels_drivable;
    int    finish_state;    /* veh+0x4064                               */
    int    pit_status;      /* veh+0x3450, >= 1 = in the pit lane        */
    int    n_traffic;
    MfPoTraffic traffic[MF_PO_TRAFFIC_MAX];
} MfPoIn;

typedef struct {
    int    apply;           /* 1 = the hook writes the pair triple       */
    int    steer_zero;      /* 1 = steer 0 (GOAL/ABORT, owner rule)      */
    double steer_add;       /* additive on the AI's own steer            */
    int    steer_abs_set;   /* 1 = write steer_abs (ROLL: AI steer out)  */
    double steer_abs;       /* absolute steer -1..1                      */
    int    brake_set;       /* 1 = write brake (else AI's own)           */
    double brake;           /* brake value when brake_set                */
    double brake_cap;       /* >0: clamp the AI's OWN brake to this (live
                             * 5: dead-engine AI slams 1.0 by itself)    */
    int    throttle_zero;   /* 1 = write throttle 0                      */
    int    park_now;        /* pulse: write AI mode 0x4510 = 4 once      */
} MfPoOut;

typedef struct {
    int    state;
    int    active;
    int    reason;
    int    side;            /* MF_PO_SIDE_*; 0 = not chosen yet          */
    int    safe_side;       /* side had room >= MF_PO_ROOM_SAFE_M        */
    int    half_rate;       /* traffic timeout: drift at half rate       */
    int    park_written;
    int    abort_reason;
    double t_begin;
    double t_state;
    double t_choose;
    double notice_s;
    double wait_s;
    double goal_mag;        /* last |goal| for the log/panel             */
    double lat_prev;
    double lat_prev_t;
    double lat_rate;        /* m/s, updated every >= 0.15 s sample       */
    double hold_mag;        /* ROLL: |lat| held on the chosen side       */
    int    cmd_init;        /* DRIFT: ramped lateral target initialised   */
    double cmd_lat;         /* DRIFT: ramped lateral target (signed)      */
    double aside_slow_s;    /* M4: continuous seconds below the ASIDE    */
                            /* stall floor (0 = not below)               */
    int    stalled;         /* M4: ASIDE held below the floor too long - */
                            /* the caller retires (R4 stop chain brakes) */
} MfPulloff;

/* ------------------------------------------------------------------ */
/* Pure helpers                                                        */
/* ------------------------------------------------------------------ */

static inline const char *mf_po_state_name(int s)
{
    switch (s) {
    case MF_PO_NOTICE: return "NOTICE";
    case MF_PO_COAST:  return "COAST";
    case MF_PO_WAIT:   return "WAIT_TRAFFIC";
    case MF_PO_DRIFT:  return "DRIFT";
    case MF_PO_GOAL:   return "GOAL";
    case MF_PO_ABORT:  return "ABORT";
    case MF_PO_PARKED: return "PARKED";
    case MF_PO_ASIDE:  return "ASIDE";
    case MF_PO_ROLL:   return "ROLL";
    default:           return "IDLE";
    }
}

static inline const char *mf_po_reason_name(int r)
{
    switch (r) {
    case MF_PO_REASON_END:      return "ending";
    case MF_PO_REASON_R4:       return "R4 retire";
    case MF_PO_REASON_GAMEKILL: return "game kill";
    case MF_PO_REASON_ASIDE:    return "ruined race";
    default:                    return "none";
    }
}

static inline const char *mf_po_abort_name(int a)
{
    switch (a) {
    case MF_PO_ABORT_RATE: return "lateral rate";
    case MF_PO_ABORT_HEAD: return "heading";
    default:               return "none";
    }
}

/* true lateral of a world xz point vs the waypoint frame (spec 6). */
static inline double mf_po_lat(const MfPoWp *w, double x, double z)
{
    return w->perp_x * (x - w->pos_x) + w->perp_z * (z - w->pos_z);
}

/* min wall free space L/R over the look-ahead minus half the car width. */
static inline void mf_po_scan(const MfPoWp *w, int n, double x, double z,
                              double half_car, double *free_l, double *free_r)
{
    double fl = 1e9, fr = 1e9;
    int i;
    /* live 3 fix: the car's offset is measured ONCE in the current
     * waypoint frame and carried along the look-ahead (as the live prototype
     * did); projecting the position into far waypoints of a corner gave
     * garbage (-3.4 m wall free in mid-track). */
    double lat = n > 0 ? mf_po_lat(&w[0], x, z) : 0.0;
    for (i = 0; i < n; i++) {
        double l = w[i].wall_l + lat;
        double r = w[i].wall_r - lat;
        if (l < fl) fl = l;
        if (r < fr) fr = r;
    }
    if (n <= 0) { fl = fr = 0.0; }
    *free_l = fl - half_car;
    *free_r = fr - half_car;
}

/* min (wall - road) run-off on a side over the look-ahead (spec 1.1.2). */
static inline double mf_po_room(const MfPoWp *w, int n, int side)
{
    double room = 1e9;
    int i;
    for (i = 0; i < n; i++) {
        double r = side == MF_PO_SIDE_L ? w[i].wall_l - w[i].road_l
                                        : w[i].wall_r - w[i].road_r;
        if (r < room) room = r;
    }
    return n > 0 ? room : 0.0;
}

/* side choice: SAFE if room >= 4 m on every look-ahead wp; prefer the safe
 * side with the smaller road distance; no safe side (street circuit) = the
 * side with more wall free space, street fallback goal (spec 1.1.2 + 6). */
static inline int mf_po_side_choose(double room_l, double room_r,
                                    double free_l, double free_r,
                                    double road_l, double road_r,
                                    int *safe_out)
{
    /* owner rule (wall hug): the side with more space to the wall */
    (void)road_l; (void)road_r;
    if (safe_out)
        *safe_out = (room_l >= MF_PO_ROOM_SAFE_M || room_r >= MF_PO_ROOM_SAFE_M);
    return free_l >= free_r ? MF_PO_SIDE_L : MF_PO_SIDE_R;
}

/* goal magnitude on the chosen side: grass = road edge + half the run-off
 * (2.1-6 m, owner live 1: "there was room") capped at wall free - 0.5 m;
 * street fallback = road edge - 1.2 m (1.1.2, 6, 7.2). */
static inline double mf_po_goal_mag(int side, int safe, const MfPoIn *in)
{
    /* Option A (owner 2026-10-02, after the live wall-scan tests: AIW wall is
     * not the barrier, AIW edge is ~2 m off, asphalt run-off = track code):
     * goal (|lat| on the chosen side) = AIW road edge + 3 m, capped so the
     * body stays 1 m inside the AIW wall. The cap converts wall FREE (from
     * the car) into |lat| units: side x lat + free - gap. */
    double edge = side == MF_PO_SIDE_L ? in->road_edge_l : in->road_edge_r;
    double free_s = side == MF_PO_SIDE_L ? in->wall_free_l : in->wall_free_r;
    double goal = edge + MF_PO_GOAL_EDGE_M;
    double cap = (double)side * in->lat + free_s - MF_PO_GOAL_WALL_M;
    (void)safe;
    if (goal > cap) goal = cap;
    if (goal < 0.0) goal = 0.0;
    return goal;
}

/* traffic rule (spec 1.1.3): another car within 25 m behind or alongside
 * AND on the target side OF US (relative to our lateral, not the AIW line)
 * within 3 m of our path. */
static inline int mf_po_traffic_blocked(int side, double lat,
                                        const MfPoTraffic *t, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (t[i].long_m <= MF_PO_TRAFFIC_BEHIND_M
            && t[i].long_m >= -MF_PO_TRAFFIC_M) {
            if (side * (t[i].lat_m - lat) > 0.0
                && fabs(t[i].lat_m - lat) <= MF_PO_TRAFFIC_PATH_M)
                return 1;
        }
    }
    return 0;
}

static inline double mf_po_car_free_side(const MfPoIn *in, int side)
{
    return side == MF_PO_SIDE_L ? in->car_free_l : in->car_free_r;
}

static inline double mf_po_wall_free_side(const MfPoIn *in, int side)
{
    return side == MF_PO_SIDE_L ? in->wall_free_l : in->wall_free_r;
}

/* additive steer limit (7.2): base rad, scaled down with speed above
 * 60 km/h so fast cars get a smaller nudge. */
static inline double mf_po_steer_lim(double speed_mps, double base)
{
    double v_kmh = speed_mps * 3.6;
    if (v_kmh < 1.0) v_kmh = 1.0;
    if (v_kmh <= 60.0) return base;
    return base * 60.0 / v_kmh;
}

/* hook-side steer composition (review fix R1). AIMainUpdate rewrites the
 * steer command 0x442C only on its LABEL_86 path; on early-return / park
 * ticks the field still holds the hook's last write. Returns the value to
 * write (AI's own steer + add, clamped to the AI range -1..1) and stores the
 * AI's own steer used in *base_io. wrote = the hook wrote last_written on
 * the previous applied tick. */
static inline float mf_po_steer_compose(int wrote, float last_written,
                                        float *base_io, float cur, float add)
{
    float ai_own = (wrote && cur == last_written) ? *base_io : cur;
    float want = ai_own + add;
    if (want > 1.0f) want = 1.0f;
    if (want < -1.0f) want = -1.0f;
    *base_io = ai_own;
    return want;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

static inline void mf_po_init(MfPulloff *p)
{
    p->state = MF_PO_IDLE;
    p->active = 0;
    p->reason = MF_PO_REASON_NONE;
    p->side = 0;
    p->safe_side = 0;
    p->half_rate = 0;
    p->park_written = 0;
    p->abort_reason = MF_PO_ABORT_NONE;
    p->t_begin = 0.0;
    p->t_state = 0.0;
    p->t_choose = 0.0;
    p->notice_s = 0.0;
    p->wait_s = 0.0;
    p->goal_mag = 0.0;
    p->lat_prev = 0.0;
    p->lat_prev_t = 0.0;
    p->lat_rate = 0.0;
    p->hold_mag = 0.0;
    p->cmd_init = 0;
    p->cmd_lat = 0.0;
    p->aside_slow_s = 0.0;
    p->stalled = 0;
}

static inline void mf_po_stop(MfPulloff *p)
{
    p->state = MF_PO_IDLE;
    p->active = 0;
}

/* dead engine / ending: NOTICE for notice_s (clamped 0.3-1.0 s), then the
 * COAST/DRIFT chain. reason = MF_PO_REASON_*. */
static inline void mf_po_begin(MfPulloff *p, int reason, double now,
                               double notice_s)
{
    mf_po_init(p);
    if (notice_s < MF_PO_NOTICE_MIN_S) notice_s = MF_PO_NOTICE_MIN_S;
    if (notice_s > MF_PO_NOTICE_MAX_S) notice_s = MF_PO_NOTICE_MAX_S;
    p->notice_s = notice_s;
    p->reason = reason;
    p->t_begin = now;
    p->t_state = now;
    p->t_choose = now;
    p->state = MF_PO_NOTICE;
    p->active = 1;
}

/* ruined-race mode (1.2): additive steer only until the pit lane. */
static inline void mf_po_begin_aside(MfPulloff *p, double now)
{
    mf_po_init(p);
    p->reason = MF_PO_REASON_ASIDE;
    p->t_begin = now;
    p->t_state = now;
    p->state = MF_PO_ASIDE;
    p->active = 1;
}

static inline void mf_po_end(MfPulloff *p)
{
    mf_po_stop(p);
}

/* ------------------------------------------------------------------ */
/* Step (spec 7.2)                                                     */
/* ------------------------------------------------------------------ */

/* live 6 (Monteiro): an ADDITIVE nudge never moved the car - the AI's own
 * line-following cancels it (lat stayed +0.2 m from 213 to 27 km/h). DRIFT
 * and ROLL therefore write an ABSOLUTE steer: heading cascade toward a
 * lateral target. Positive steer -> +lat (sign convention). Gains shrink
 * with speed (v <= 60 km/h full, above (60/v)^2) for stability. */
#define MF_PO_ABS_K_POS     0.08   /* desired sin(heading) per m error */
#define MF_PO_ABS_HS_MAX    0.25   /* max desired sin(heading) ~14 deg */
#define MF_PO_ABS_K_HEAD    2.0    /* steer per sin(heading) error     */
#define MF_PO_ABS_LIM       0.4    /* steer clamp at <= 60 km/h        */
static inline double mf_po_abs_steer(double target_lat, const MfPoIn *in)
{
    double v_kmh = in->speed_mps * 3.6;
    double f = v_kmh <= 60.0 ? 1.0 : 60.0 / v_kmh;
    double hs_des = MF_PO_ABS_K_POS * (target_lat - in->lat);
    double steer, lim = MF_PO_ABS_LIM * f;
    if (hs_des > MF_PO_ABS_HS_MAX) hs_des = MF_PO_ABS_HS_MAX;
    if (hs_des < -MF_PO_ABS_HS_MAX) hs_des = -MF_PO_ABS_HS_MAX;
    steer = MF_PO_ABS_K_HEAD * f * f * (hs_des - in->heading_side);
    if (steer > lim) steer = lim;
    if (steer < -lim) steer = -lim;
    return steer;
}

/* ROLL (owner live 1, 2026-10-02): off the line - keep the additive steer
 * holding hold_mag on the chosen side (the car follows the track curve, it
 * does not run straight on into a wall), throttle 0, light brake only; once
 * stopped -> PARKED (native mode 4 = full brake hold). A wall closer than the
 * margin pulls the target inward; a spin (heading) still aborts. */
static inline MfPoOut mf_po_roll_out(MfPulloff *p, const MfPoIn *in)
{
    MfPoOut o;
    double target, free_s, steer, lim;

    memset(&o, 0, sizeof o);
    if (in->speed_mps < MF_PO_STOP_MPS) {
        p->state = MF_PO_PARKED;
        p->park_written = 1;
        o.park_now = 1;
        return o;
    }
    if (in->heading_err_rad > MF_PO_ABORT_HEAD_RAD) {
        p->abort_reason = MF_PO_ABORT_HEAD;
        p->state = MF_PO_ABORT;
        o.apply = 1;
        o.steer_zero = 1;
        o.brake_set = 1;
        o.brake = 1.0;
        o.throttle_zero = 1;
        return o;
    }
    target = p->hold_mag;
    free_s = mf_po_wall_free_side(in, p->side);
    if (free_s < MF_PO_WALL_MARGIN_M) {
        double inward = (double)p->side * in->lat
                        - (MF_PO_WALL_MARGIN_M - free_s);
        if (inward < target) target = inward > 0.0 ? inward : 0.0;
    }
    p->goal_mag = target;
    /* ABSOLUTE steer: the AI's own steer would pull the slowing car back to
     * its racing line. Positive steer turns toward +lat (sign convention). */
    steer = mf_po_abs_steer((double)p->side * target, in);
    (void)lim;
    o.apply = 1;
    o.steer_abs_set = 1;
    o.steer_abs = steer;
    o.brake_set = 1;
    o.brake = in->speed_mps < MF_PO_ROLL_SLOW_MPS ? MF_PO_ROLL_BRAKE_SLOW
                                                 : MF_PO_ROLL_BRAKE;
    o.throttle_zero = 1;
    return o;
}

static inline MfPoOut mf_po_step(MfPulloff *p, const MfPoIn *in, double now,
                                 double dt)
{
    MfPoOut o;
    int parking = 0;

    memset(&o, 0, sizeof o);
    if (!p->active) return o;

    /* lateral rate over a stable sample window; a waypoint change resets it
     * (the external false abort: waypoint change read as -90 m/s). */
    if (!in->lat_frame_ok) {
        p->lat_prev = in->lat;
        p->lat_prev_t = now;
        p->lat_rate = 0.0;
    } else if (now - p->lat_prev_t >= MF_PO_LAT_WINDOW_S) {
        p->lat_rate = (in->lat - p->lat_prev) / (now - p->lat_prev_t);
        p->lat_prev = in->lat;
        p->lat_prev_t = now;
    }

    /* PARKED (P6): native AI mode 4 holds the car; nothing is written until
     * FinishState 2 releases the state (never release while it moves). */
    if (p->state == MF_PO_PARKED) {
        if (!p->park_written) {
            o.park_now = 1;
            p->park_written = 1;
        }
        if (in->finish_state == 2) p->active = 0;
        return o;
    }

    /* ASIDE (1.2): stay off the line, brake/throttle untouched. */
    if (p->state == MF_PO_ASIDE) {
        double edge, goal, steer, lim;
        if (in->pit_status >= 1) {          /* pit lane takes over */
            p->active = 0;
            return o;
        }
        /* M4 (owner, Badoer live): ASIDE writes no brake BY DESIGN, so a car
         * that can no longer keep pace crawls aside for ever. Below the stall
         * floor for MF_PO_ASIDE_STALL_S the pull-off raises `stalled`; the
         * caller then retires the slot through the existing R4 path (Option A
         * ending + the normal stop chain, which brakes). A slow corner lasts
         * seconds; this does not. */
        if (in->speed_mps < MF_PO_ASIDE_STALL_MPS) {
            if (p->aside_slow_s <= 0.0) p->aside_slow_s = now + 1e-3;
            else if (now - p->aside_slow_s >= MF_PO_ASIDE_STALL_S) {
                p->aside_slow_s = 0.0;
                p->stalled = 1;
            }
        } else {
            p->aside_slow_s = 0.0;
        }
        if (p->side == 0) {
            if (in->apex_side != 0)
                p->side = -in->apex_side;   /* away from the apex */
            else
                p->side = in->wall_free_l >= in->wall_free_r
                          ? MF_PO_SIDE_L : MF_PO_SIDE_R;
        }
        edge = p->side == MF_PO_SIDE_L ? in->road_edge_l : in->road_edge_r;
        goal = edge - MF_PO_ASIDE_GOAL_M;
        if (goal < MF_PO_ASIDE_RECENTER_M) goal = 0.0;   /* re-center */
        p->goal_mag = fabs(goal);
        steer = MF_PO_K_LAT * ((double)p->side * goal - in->lat)
                - MF_PO_K_RATE * p->lat_rate;
        lim = mf_po_steer_lim(in->speed_mps, MF_PO_STEER_LIM_ASIDE);
        if (steer > lim) steer = lim;
        if (steer < -lim) steer = -lim;
        o.apply = 1;
        o.steer_add = steer;
        return o;
    }

    /* ---- pull-off chain ---- */
    if (p->state == MF_PO_NOTICE) {
        if (now - p->t_begin < p->notice_s)
            return o;                     /* leave the car alone (1.1.1) */
        p->state = MF_PO_COAST;
        p->t_state = now;
        p->t_choose = now - MF_PO_CHOOSE_S;   /* choose the side now */
    }

    switch (p->state) {
    case MF_PO_COAST:
    case MF_PO_WAIT:
        if (now - p->t_choose >= MF_PO_CHOOSE_S) {
            p->t_choose = now;
            p->side = mf_po_side_choose(in->room_l, in->room_r,
                                        in->wall_free_l, in->wall_free_r,
                                        in->road_edge_l, in->road_edge_r,
                                        &p->safe_side);
        }
        if (p->state == MF_PO_COAST && in->speed_mps * 3.6 < MF_PO_COAST_KMH) {
            int blocked = (mf_po_traffic_blocked(p->side, in->lat,
                                                 in->traffic, in->n_traffic)
                           || mf_po_car_free_side(in, p->side) < MF_PO_CAR_MIN_M)
                          && p->wait_s < MF_PO_WAIT_MAX_S
                          && in->speed_mps >= MF_PO_WAIT_MIN_MPS;
            if (blocked) {
                p->state = MF_PO_WAIT;
                p->t_state = now;
            } else {
                p->state = MF_PO_DRIFT;
                p->t_state = now;
                p->half_rate = 0;
            }
        }
        if (p->state == MF_PO_WAIT) {
            int blocked = mf_po_traffic_blocked(p->side, in->lat,
                                                in->traffic, in->n_traffic)
                          || mf_po_car_free_side(in, p->side) < MF_PO_CAR_MIN_M;
            if (dt > 0.0) p->wait_s += dt;
            if (in->speed_mps < MF_PO_WAIT_MIN_MPS) {
                p->state = MF_PO_DRIFT;   /* live 2: too slow to wait */
                p->t_state = now;
                p->half_rate = 0;
            } else if (!blocked) {
                p->state = MF_PO_DRIFT;
                p->t_state = now;
                p->half_rate = 0;
            } else if (p->wait_s >= MF_PO_WAIT_MAX_S) {
                p->state = MF_PO_DRIFT;   /* drift anyway, half rate (1.1.3) */
                p->t_state = now;
                p->half_rate = 1;
            }
        }
        if (p->state == MF_PO_DRIFT)
            break;                        /* emit the DRIFT output below */
        o.apply = 1;                      /* live 2: coast/wait = no brake */
        o.throttle_zero = 1;
        o.brake_cap = MF_PO_AI_BRAKE_CAP;
        return o;

    case MF_PO_DRIFT:
        break;

    case MF_PO_ROLL:
        return mf_po_roll_out(p, in);

    case MF_PO_GOAL:
    case MF_PO_ABORT:
    case MF_PO_PARKED:
        parking = 1;
        break;

    default:
        return o;
    }

    if (parking) {
        if (in->speed_mps < MF_PO_STOP_MPS) {
            /* stopped: native park from here on, no more writes (P6) */
            p->state = MF_PO_PARKED;
            p->park_written = 1;
            o.park_now = 1;
            return o;
        }
        o.apply = 1;
        o.steer_zero = 1;
        o.brake_set = 1;
        o.brake = 1.0;
        o.throttle_zero = 1;
        return o;
    }

    /* DRIFT (reached by fall-through from COAST/WAIT) */
    {
        double goal, steer, lim, rate_max;

        /* live 1: the rate limit scales with speed (a 10 deg lane change at
         * 160 km/h is 7.9 m/s); a real spin is the heading abort */
        rate_max = MF_PO_ABORT_RATE_FRAC * in->speed_mps;
        if (rate_max < MF_PO_ABORT_RATE_MAX) rate_max = MF_PO_ABORT_RATE_MAX;
        if (fabs(p->lat_rate) > rate_max) {
            p->abort_reason = MF_PO_ABORT_RATE;
            p->state = MF_PO_ABORT;
        } else if (in->heading_err_rad > MF_PO_ABORT_HEAD_RAD) {
            p->abort_reason = MF_PO_ABORT_HEAD;
            p->state = MF_PO_ABORT;
        }

        goal = mf_po_goal_mag(p->side, p->safe_side, in);
        p->goal_mag = goal;

        if (p->state == MF_PO_DRIFT && in->speed_mps < MF_PO_STOP_MPS) {
            p->state = MF_PO_PARKED;     /* stopped before the goal: hold */
            p->park_written = 1;
            o.park_now = 1;
            return o;
        }
        if (p->state == MF_PO_DRIFT) {
            int roll = 0;
            if (in->wheels_known >= 3 && in->wheels_drivable <= 1)
                roll = 1;                /* off the surface (1.1.6) */
            else if (mf_po_wall_free_side(in, p->side) <= MF_PO_WALL_MARGIN_M)
                roll = 1;                /* wall margin (7.2) */
            else if (goal > 0.0 && (double)p->side * in->lat >= goal)
                roll = 1;   /* goal reached ON THE CHOSEN SIDE (1.1.6) */
            else if (!p->half_rate && p->wait_s < MF_PO_WAIT_MAX_S
                       && in->speed_mps >= MF_PO_WAIT_MIN_MPS
                       && (mf_po_traffic_blocked(p->side, in->lat,
                                                 in->traffic, in->n_traffic)
                           || mf_po_car_free_side(in, p->side)
                              < MF_PO_CAR_MIN_M)) {
                p->state = MF_PO_WAIT;   /* hold the line, wait (1.1.3/6);
                                          * wait_s is a per-pull-off budget */
            }
            if (roll) {
                /* owner live 1: no full brake here - keep steering to the
                 * goal (wheels off the asphalt is not the end of the move)
                 * and roll out; full brake only at the halt (PARKED). */
                double now_mag = (double)p->side * in->lat;
                p->hold_mag = goal > now_mag ? goal : now_mag;
                if (p->hold_mag < 0.0) p->hold_mag = 0.0;
                p->state = MF_PO_ROLL;
                return mf_po_roll_out(p, in);
            }
        }

        if (p->state == MF_PO_ABORT) {
            if (in->speed_mps < MF_PO_STOP_MPS) {
                p->state = MF_PO_PARKED;
                p->park_written = 1;
                o.park_now = 1;
                return o;
            }
            o.apply = 1;
            o.steer_zero = 1;
            o.brake_set = 1;
            o.brake = 1.0;
            o.throttle_zero = 1;
            return o;
        }
        if (p->state == MF_PO_WAIT) {
            o.apply = 1;
            o.throttle_zero = 1;
            o.brake_cap = MF_PO_AI_BRAKE_CAP;
            return o;
        }

        /* ramped lateral target (spec 1.1.5 rates), absolute steer */
        {
            double want = (double)p->side * goal;
            double rate = in->speed_mps * 3.6 > MF_PO_DRIFT_KMH_FAST
                          ? MF_PO_DRIFT_RATE_HI : MF_PO_DRIFT_RATE_LO;
            double step;
            if (p->half_rate) rate *= 0.5;
            if (!p->cmd_init) { p->cmd_init = 1; p->cmd_lat = in->lat; }
            step = rate * (dt > 0.0 ? dt : 0.0);
            if (want > p->cmd_lat + step) p->cmd_lat += step;
            else if (want < p->cmd_lat - step) p->cmd_lat -= step;
            else p->cmd_lat = want;
        }
        steer = mf_po_abs_steer(p->cmd_lat, in);
        (void)lim;
        o.apply = 1;
        o.steer_abs_set = 1;
        o.steer_abs = steer;
        o.throttle_zero = 1;            /* live 2: keep momentum, no brake */
        o.brake_cap = MF_PO_AI_BRAKE_CAP;
        return o;
    }
}

#endif /* MF_AI_PULLOFF_H */
