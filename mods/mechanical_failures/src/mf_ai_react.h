/* mf_ai_react.h - AI driver symptoms + reactions (ADDENDUM 14, B1-B7).
 *
 * Specification: docs/AI_SYMPTOMS_AND_REACTIONS_SPEC_2026-10-01.md (Claude,
 * 2026-10-01 g; supersedes ADDENDUM 13 where they conflict). Pure C: no game
 * addresses, no Windows, no globals - every function is deterministic from
 * its inputs + the caller's per-slot MfRng, so tools/ai_react_test.c and the
 * car sweep can exercise the exact same code the DLL runs.
 *
 *   mf_ai_present(): MfOut/MfEngine/MfCar -> AiSymptom  (spec section 4)
 *   mf_ai_react():   AiSymptom + AiTraits + context -> AiReaction
 *                    (spec sections 3.2-3.5, 5: notice, level, margin,
 *                     perceived limit, mistakes; escalation only)
 *
 * The DLL (engine_failure.c) applies the reaction through the proven levers
 * (rev-cap triple, power shape, banks, blue queue, sounds, Option A ending);
 * the reaction changes the real AI rpm/load the model reads next step, so the
 * physics pays off (spec 3.4). No input is ever faked.
 *
 * Numbering in comments = the spec section. Tunables are MF_AIR_* and carry
 * their citation; nothing here is calibrated beyond the spec's starting
 * values - B8 holds the field calibration. */
#ifndef MF_AI_REACT_H
#define MF_AI_REACT_H

#include "mf_engine_model.h"

/* ------------------------------------------------------------------ */
/* Constants (spec 3.2-3.5, 4.x, 5)                                    */
/* ------------------------------------------------------------------ */

/* MF 1.0.3 N1 (owner D2/D6): AI blue smoke starts later than the player's.
 * The AI density is 0 below MF_AIR_BLUE_ONSET_LOST of the oil lost and 1 at
 * MF_AIR_BLUE_FULL_LOST and above; the leak progression itself is unchanged. */
#define MF_AIR_BLUE_ONSET_LOST  0.25   /* -  lost fraction where smoke starts */
#define MF_AIR_BLUE_FULL_LOST   0.50   /* -  lost fraction at full density */

/* MF 1.0.3 M2 (owner: "HE SHOULD RETIRE"): crawler guard. An AI car with an
 * active fault that stays below MF_AI_CRAWL_FRAC of the field median speed
 * for MF_AI_CRAWL_HOLD_S game seconds escalates to the existing R4 retire.
 * M2b (Claude STOP M/N review 2026-10-08): both sides are TIME-AVERAGED -
 * a car in a genuinely slow sector (Monaco Loews) must not be retired while
 * the field median is sampled on straights. Per car: EMA of speed with tau
 * MF_AI_CRAWL_EMA_S; the field median is taken over those EMAs. */
#define MF_AI_CRAWL_FRAC        0.60   /* -  of the field median speed */
#define MF_AI_CRAWL_HOLD_S      20.0   /* s  below the fraction -> retire */
#define MF_AI_CRAWL_MIN_MPS     15.0   /* m/s  median floor: ignore a field that
                                        *      is itself in the pits / slow */
#define MF_AI_CRAWL_EMA_S       20.0   /* s  per-car speed EMA time constant */

/* MF 1.0.3 N3d (owner D4/D6): an existing valve/oil/fuel fault that retires
 * with a badly damaged engine ends in a blow-up on this roll. */
#define MF_AI_LIMP_BLOW_HEALTH  0.15   /* -  h_vt or h_b below this = ruined */
#define MF_AI_LIMP_BLOW_P       0.50   /* -  chance the retire becomes a blow-up */

/* MF 1.0.3 N3e (owner D3): the AI blow-up fireball in world time. The shared
 * FLAMES descriptor is tuned while ANY blow (player or AI) is burning; the
 * big flash lasts MF_BLOW_FX_FLASH_MS from that blow's start, the mid burn
 * until the owner releases (player: BLOW_FIRE_MS; AI: the P5 bank fade). */
#define MF_BLOW_FX_FLASH_MS     1500.0 /* ms owner age for the x3.5 flash */
#define MF_BLOW_FX_NONE         0
#define MF_BLOW_FX_FLASH        1
#define MF_BLOW_FX_BURN         2

/* 3.2 rev cap perception: the driver does not know the true limit. */
#define MF_AIR_SIGMA_P_SENS     0.02   /* -  sigma at sensitivity 1.0 */
#define MF_AIR_SIGMA_P_LOW      0.05   /* -  sigma at sensitivity 0.0 */
#define MF_AIR_MARGIN_R1_LO     0.02   /* -  R1 margin low  (spec 3.2/5) */
#define MF_AIR_MARGIN_R1_HI     0.04   /* -  R1 margin high (sympathy) */
#define MF_AIR_MARGIN_R2        0.05   /* -  R2 margin (spec 5) */

/* 3.3 mistakes */
#define MF_AIR_MISTAKE_RATE     (1.0 / 600.0)  /* 1/s base per active fault */
#define MF_AIR_MISTAKE_CTX_BATTLE 2.0          /* -  within 1.0 s of a car */
#define MF_AIR_MISTAKE_CTX_END    1.5          /* -  last 10 % of the race */
#define MF_AIR_MISTAKE_CTX_NURSE  0.5          /* -  already nursing R2+ */
#define MF_AIR_MISTAKE_COOL_S     30.0         /* s  no chains */
#define MF_AIR_M1_T_MIN_S         1.0          /* s  missed upshift cap lift */
#define MF_AIR_M1_T_MAX_S         3.0
#define MF_AIR_M2_LAP_S           90.0         /* s  push-lap margin 0 */
#define MF_AIR_MISTAKE_P_M1       0.40         /* -  weighted kinds */
#define MF_AIR_MISTAKE_P_M2       0.40

/* 3.4 notice delay: base(fault) x sens_factor x LogNormal(0, 0.4) */
#define MF_AIR_NOTICE_SIGMA       0.4
#define MF_AIR_SENS_FACTOR_HI     0.5          /* -  best driver */
#define MF_AIR_SENS_FACTOR_LO     2.0          /* -  worst driver */

/* 4.x notice bases (s). Quiet faults take longer than the dash light. */
#define MF_AIR_NOTICE_VALVE       8.0
#define MF_AIR_NOTICE_OILP        4.0
#define MF_AIR_NOTICE_OILLEAK     30.0
#define MF_AIR_NOTICE_WATER       40.0
#define MF_AIR_NOTICE_RAD         15.0
#define MF_AIR_NOTICE_FUELP       4.0
#define MF_AIR_NOTICE_FUELLEAK    300.0        /* team sees the fuel, 1-3 laps */
#define MF_AIR_NOTICE_BLOWBY      5.0
#define MF_AIR_NOTICE_EXHAUST     20.0

/* 5 no-fault-limit caps (fraction of the car's rev limit). The table is per
 * fault per level; R1/R2 only (R0 keeps the file, R3/R4 have no cap). */
#define MF_AIR_CAP_R1_OILP        0.90
#define MF_AIR_CAP_R2_OILP        0.85
#define MF_AIR_CAP_R1_OILLEAK     0.92
#define MF_AIR_CAP_R2_OILLEAK     0.88
#define MF_AIR_CAP_R1_WATER       0.93
#define MF_AIR_CAP_R2_WATER       0.88
#define MF_AIR_CAP_R1_FUELP       0.90
#define MF_AIR_CAP_R2_FUELP       0.85
#define MF_AIR_CAP_R1_FUELLEAK    0.92
#define MF_AIR_CAP_R2_FUELLEAK    0.88
#define MF_AIR_CAP_R1_BLOWBY      0.92
#define MF_AIR_CAP_R2_BLOWBY      0.88
#define MF_AIR_CAP_R1_EXHAUST     0.94

/* 4.2-4.9 cue thresholds (structure the escalation signature) */
#define MF_AIR_HB_LIMP       0.50   /* -  oil pressure R2 threshold */
#define MF_AIR_HB_RETIRE     0.25   /* -  R4 weight high below this */
#define MF_AIR_SUMP_RETIRE   0.30   /* -  oil leak R4 below 30 % sump */
#define MF_AIR_HG_RETIRE     0.40   /* -  water leak R4 below this gasket */
#define MF_AIR_RING_R2       0.50   /* -  blow-by R2 threshold */
#define MF_AIR_POWER_RETIRE  0.70   /* -  blow-by R4 below this power */
#define MF_AIR_VALVE_R2      0.80   /* -  valve R2 when ceiling below this */
#define MF_AIR_THERM_R2      0.20   /* -  coolant fraction R2 step (spec 4.4) */

/* 5 context modifiers */
#define MF_AIR_END_FRAC      0.10   /* -  last 10 % of the race */
#define MF_AIR_CLOSE_GAP_S   1.0    /* -  battle = within 1.0 s */

/* 5 escalation steps (a clear worsening: severity stage, ceiling -3 %,
 * health/coolant step 0.1; a level is re-decided only upward) */
#define MF_AIR_CEIL_STEP     0.03
#define MF_AIR_HEALTH_STEP   0.10

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    double sensitivity;   /* 0..1 better = notices sooner (RaceAbility) */
    double sympathy;      /* 0..1 smoother = bigger margin (low Crash) */
    double racer;         /* 0..1 keeps pushing (Passing) */
    double team_order;    /* 0..1 bring-it-home bias */
    double crash;         /* 0..1 mistake tendency (Crash) */
    int    valid;
} AiTraits;

typedef struct {
    int    fault;             /* MF_FAULT_* (0 = none yet) */
    int    onset;             /* 1 = the fault is active */
    double power_mult;        /* symptom power shape [MF_POWER_MIN..1] */
    double loss_top;          /* shaped top-end loss */
    double loss_all;
    double loss_load;
    double true_limit_rads;   /* physical rev limit this fault allows;
                               * 0 = none (reaction caps by level only) */
    double rev_ceiling_frac;  /* valve ceiling fraction, -1 = none */
    double top_cut_rads;      /* hard top-cut rpm (rows >= zeroed), -1 = none */
    double misfire_depth;     /* 0..1 pulse depth (bearing knock / stumbles) */
    double steam;             /* 0..1 white steam density (water / rad) */
    double blue;              /* 0..1 blue oil smoke density */
    double pop_rate_s;        /* exhaust pop chance per second */
    int    flame_spit;        /* 1 = emit a flame spit this step */
    int    ending;            /* MF_AI_END_* */
    int    ending_reason;     /* MF_FAULT_* blamed at the terminal */
} AiSymptom;

#define MF_AI_END_NONE   0
#define MF_AI_END_STOP   1   /* engine stop, no fire (rings/gasket/fuel) */
#define MF_AI_END_BLOWN  2   /* blow-up, the only fireball (4.1/4.2/4.7) */
#define MF_AI_END_REAR   3   /* exhaust rear-suspension break (4.9) */

typedef struct {
    int    started;           /* onset seen; delay rolled */
    int    noticed;           /* the driver has felt the symptom */
    double notice_at_s;       /* t_s of the notice roll */
    double notice_delay_s;    /* the rolled delay, for logs */
    int    level;             /* R0..R4 (spec 5) */
    double level_t_s;         /* t_s of the last level change */
    double margin;            /* under the perceived limit */
    double perceived_off;     /* N(0, sigma_p) fraction, rolled per escalation */
    double cap_rads;          /* cap computed this step (<= 0 = none) */
    double no_limit_frac;     /* cap fraction when the fault has no limit */
    double passing_mult;      /* PassingNorm multiplier (1 = file) */
    double aggression_mult;   /* TrackAggression multiplier (1 = file) */
    int    pit;               /* R3 active (callToBox requested) */
    int    retire;            /* R4 chosen */
    int    mistake;           /* active mistake kind 1..3, 0 = none */
    int    mistake_count;
    double mistake_cool_s;    /* cooldown after the last mistake */
    double m1_left_s;         /* cap lifted to the full limit */
    double m2_left_s;         /* margin 0 (push lap) */
    double best_sig;          /* escalation signature, -1 before notice */
} AiReaction;

/* Restore-completeness bookkeeping (spec B7): the DLL fills this snapshot at
 * the first lever write and clears it on the last restore. The unit test
 * proves the pure fill/clear cycle leaves every field clean. */
typedef struct {
    int    rev_valid;
    double rev_limit, rev_upshift, rev_down[8];
    int    rev_down_n;
    int    power_valid;
    int    banks_valid;
    double bank[3];
    int    maxspeed_valid;
    double maxspeed;
    int    passing_valid;
    double passing;
    int    aggr_valid;
    double aggression;
} MfAiLeverSave;

static inline void mf_ai_lever_save_clear(MfAiLeverSave *sv)
{
    memset(sv, 0, sizeof *sv);
}

static inline int mf_ai_lever_save_clean(const MfAiLeverSave *sv)
{
    return !sv->rev_valid && !sv->power_valid && !sv->banks_valid
        && !sv->maxspeed_valid && !sv->passing_valid && !sv->aggr_valid;
}

/* 2026-10-07 audit COMP-01 (owner D3): MF only lowers an AI maxSpeed.
 * Write the limp cap only when the current value is above it; a value at
 * or below the cap (stock, or another mod's Safety Car / Red Flag cap)
 * is left alone. */
static inline int mf_ai_maxspeed_should_write(double cur, double cap)
{
    return cur == cur && cap > 0.0 && cur > cap + 0.5;
}

/* 2026-10-07 audit COMP-01b: while another controller (Safety Car / Red
 * Flag) owns a lever, MF must keep its restore record PENDING - the live
 * value is not MF's own write yet, so retry on a later pass instead of
 * dropping the record. Restore (write the saved value and clear the record)
 * only when the live value is still MF's own last write within tol. A stale
 * session generation drops the record without writing (handled by the
 * caller). */
#define MF_AI_LEVER_RESTORE_PENDING 0
#define MF_AI_LEVER_RESTORE_WRITE   1
static inline int mf_ai_lever_restore_decide(double cur, double ours, double tol)
{
    return (cur == cur && ours == ours && fabs(cur - ours) <= tol)
        ? MF_AI_LEVER_RESTORE_WRITE : MF_AI_LEVER_RESTORE_PENDING;
}

/* ------------------------------------------------------------------ */
/* Small pure helpers                                                  */
/* ------------------------------------------------------------------ */

static inline double mf_ai_clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* standard normal from the per-slot rng (Box-Muller; two uniforms, the
 * second draw is discarded when the first is unusable) */
static inline double mf_ai_normal(MfRng *rng)
{
    double u1, u2;
    u1 = mf_rng_d(rng);
    u2 = mf_rng_d(rng);
    if (u1 < 1e-12) u1 = 1e-12;
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

/* smoothstep(peak, limit, w) inverted: the rpm where s(w) = s0 (0..1) */
static inline double mf_ai_smoothstep_inv(double s0, double peak, double limit)
{
    double lo, hi, i;
    if (!(limit > peak) || peak <= 0.0) return limit;
    if (s0 <= 0.0) return peak;
    if (s0 >= 1.0) return limit;
    /* s(x) is monotonic in x = (w-peak)/(limit-peak); 40 bisections is far
     * beyond float precision and stays allocation-free. */
    lo = 0.0; hi = 1.0;
    for (i = 0; i < 40; i++) {
        double x = 0.5 * (lo + hi);
        double s = x * x * (3.0 - 2.0 * x);
        if (s < s0) lo = x; else hi = x;
    }
    return peak + (limit - peak) * 0.5 * (lo + hi);
}

static inline const char *mf_ai_fault_name(int f)
{
    switch (f) {
    case MF_FAULT_VALVE_AIR:     return "valve";
    case MF_FAULT_OIL_PRESSURE:  return "oilp";
    case MF_FAULT_OIL_LEAK:      return "oilleak";
    case MF_FAULT_WATER_LEAK:    return "waterleak";
    case MF_FAULT_RAD_DEBRIS:    return "rad";
    case MF_FAULT_FUEL_PRESSURE: return "fuelp";
    case MF_FAULT_FUEL_LEAK:     return "fuelleak";
    case MF_FAULT_BLOWBY:        return "blowby";
    case MF_FAULT_EXHAUST:       return "exhaust";
    case MF_FAULT_BLOWUP:        return "blowup";
    default:                     return "none";
    }
}

/* N3c gate (owner D5, vanilla+): "Failure Rate none" = no mod failures at
 * all - no onset of any kind, BLOW-UP included. Pure so the rule is testable
 * without the game: the DLL's ai_engines_pump and the player gate both use
 * it. failrate is the .PLR value (0 = none). */
static inline int mf_ai_faults_enabled(int ai_engines_enable, int failrate)
{
    return ai_engines_enable && failrate != 0;
}

/* N1: AI blue density from the oil-lost fraction. 0 below ONSET, linear to 1
 * at FULL, clamped above. Player path unchanged (starts at 2 %). */
static inline double mf_ai_blue_curve(double lost)
{
    if (!isfinite(lost) || lost <= MF_AIR_BLUE_ONSET_LOST) return 0.0;
    return mf_ai_clampd((lost - MF_AIR_BLUE_ONSET_LOST)
                        / (MF_AIR_BLUE_FULL_LOST - MF_AIR_BLUE_ONSET_LOST),
                        0.0, 1.0);
}

/* M1 (2026-10-08 ratchet fix): the value the AI car derive must use for the
 * rev limit. While our rev lever is applied the live field holds OUR cap;
 * deriving the car from it shrank rev_limit every tick (cap of a cap) down
 * to the 100 rad/s guard - tiny upshift points, top gear, ~68 km/h crawl.
 * While a saved original exists, derive from the saved value. Pure: the
 * ratchet test drives this exact helper. */
static inline double mf_ai_rev_derive(double live_revlim, int rev_valid,
                                      double saved_revlim)
{
    if (rev_valid && saved_revlim > 100.0) return saved_revlim;
    return live_revlim;
}

/* M3 gear safety: the effective shift points for a capped rev band. The
 * upshift threshold is 0.975 x cap (unchanged); a valid downshift point
 * (> 1.5 x idle and > 1.0) scales by cap / original limit - computed from
 * the SNAPSHOT, never the already-capped live value - so the car keeps the
 * right gear and can always downshift under the cap. Returns the upshift
 * threshold; down_out[] receives the scaled points. */
#define MF_AI_REV_UPSHIFT_FRAC  0.975
#define MF_AI_REV_DOWN_IDLE_X   1.5
static inline double mf_ai_rev_points(double rev_limit, double cap, double idle,
                                      const double *down_in, int n,
                                      double *down_out)
{
    double ratio = (rev_limit > 1.0) ? cap / rev_limit : 1.0;
    int i;
    for (i = 0; i < n; i++) {
        double d = down_in[i];
        if (d > MF_AI_REV_DOWN_IDLE_X * idle && d > 1.0) d *= ratio;
        down_out[i] = d;
    }
    return MF_AI_REV_UPSHIFT_FRAC * cap;
}

/* M2b: one EMA step for the crawler guard (pure). ema <= 0 seeds from the
 * first valid sample; a non-positive speed/dt keeps the average. */
static inline double mf_ai_crawl_ema(double ema, double speed, double dt)
{
    double a;
    if (!(speed > 0.0) || !(dt > 0.0)) return ema;
    if (!(ema > 0.0)) return speed;
    a = 1.0 - exp(-dt / MF_AI_CRAWL_EMA_S);
    return ema + (speed - ema) * a;
}

/* M2: one crawler-guard step for a slot (pure). `speed_mps` is the car's
 * speed EMA and `median_mps` the field median of the EMAs (M2b). Since = the
 * game second the car first fell below the fraction (0 = not below).
 * Returns 1 when the hold time has elapsed - the caller retires through the
 * existing R4 path. */
static inline int mf_ai_crawl_step(double *since, double speed_mps,
                                   double median_mps, double now_s,
                                   double frac, double hold_s)
{
    if (!(median_mps >= MF_AI_CRAWL_MIN_MPS)
        || !(speed_mps > 0.0) || speed_mps >= frac * median_mps) {
        *since = 0.0;
        return 0;
    }
    if (*since <= 0.0) { *since = now_s + 1e-3; return 0; }
    return now_s - *since >= hold_s;
}

/* N3e: which tuning phase the shared FLAMES descriptor needs, from the start
 * times (game ms; 0 = inactive) of every blow owner (slot 0 = the player,
 * slots 1.. = AI). FLASH while ANY owner is younger than flash_ms, BURN while
 * ANY is active, NONE when the LAST one has been released. Pure: the test
 * drives two AI blows + one player blow in every release order. */
static inline int mf_blow_fx_phase_ms(const double *start_ms, int n,
                                      double now_ms, double flash_ms)
{
    int i, any = 0, flash = 0;
    for (i = 0; i < n; i++) {
        if (!(start_ms[i] > 0.0)) continue;
        any = 1;
        if (now_ms - start_ms[i] >= 0.0
            && now_ms - start_ms[i] < flash_ms)
            flash = 1;
    }
    if (flash) return MF_BLOW_FX_FLASH;
    if (any) return MF_BLOW_FX_BURN;
    return MF_BLOW_FX_NONE;
}

/* ------------------------------------------------------------------ */
/* B1: mf_ai_present - model state -> the symptoms the CAR shows       */
/* (spec section 4; the driver may not have noticed yet - R0)          */
/* ------------------------------------------------------------------ */

static inline AiSymptom mf_ai_present(const MfOut *o, const MfEngine *e,
                                      const MfCar *c)
{
    AiSymptom s;
    memset(&s, 0, sizeof s);
    s.fault = e->onset ? e->fault : MF_FAULT_NONE;
    s.onset = e->onset ? 1 : 0;
    s.power_mult = mf_ai_clampd(o->power_mult, MF_POWER_MIN, 1.0);
    s.loss_top = o->loss_top;
    s.loss_all = o->loss_all;
    s.loss_load = o->loss_load;
    s.true_limit_rads = 0.0;
    s.rev_ceiling_frac = -1.0;
    s.top_cut_rads = -1.0;
    s.ending = MF_AI_END_NONE;
    s.ending_reason = e->fault;

    if (e->onset) {
        switch (e->fault) {
        case MF_FAULT_VALVE_AIR:
            /* 4.1: the falling ceiling is the true limit; above it the
             * float misfire already comes from the model (vfuel_valve). */
            if (o->rev_ceiling >= 0.0) {
                s.rev_ceiling_frac = mf_ai_clampd(o->rev_ceiling, 0.0, 1.0);
                s.true_limit_rads = c->rev_limit_rads * s.rev_ceiling_frac;
            }
            s.misfire_depth = o->misfire_depth > 0.0 ? o->misfire_depth : 0.0;
            if (o->vfuel_valve > 0.0 && s.misfire_depth < 0.3)
                s.misfire_depth = 0.3;
            s.misfire_depth = mf_ai_clampd(s.misfire_depth, 0.0, MF_MISFIRE_MAX);
            break;
        case MF_FAULT_OIL_PRESSURE:
            /* 4.2: loss_all power shape; blue only when oil is being lost
             * (a pressure fault alone loses no oil - matches the player);
             * bearing knock (roughness) when h_b < 0.3 (inference). */
            if (e->h_b < 0.3) s.misfire_depth = 0.25;
            break;
        case MF_FAULT_OIL_LEAK:
            /* 4.3: blue density follows the oil-lost fraction; N1 (owner
             * 2026-10-08 evening): the AI onset moves to 25 % lost, full at
             * 50 % - the leak progression itself is unchanged */
            s.blue = mf_ai_blue_curve(1.0 - e->v_oil_l / MF_OIL_START_L);
            if (e->h_b < 0.3) s.misfire_depth = 0.25;
            break;
        case MF_FAULT_WATER_LEAK:
            /* 4.4: white steam = the model's graded bank (escaping + hot) */
            s.steam = o->smoke_bank1 < 0.0 ? -o->smoke_bank1 : 0.0;
            if (e->v_w_l < MF_WATER_SENSOR_FRAC * MF_WATER_START_L
                && s.steam < 0.3)
                s.steam = 0.3;
            break;
        case MF_FAULT_RAD_DEBRIS:
            /* 4.5: no visible debris; wisps near boiling only */
            s.steam = o->smoke_bank1 < 0.0 ? -o->smoke_bank1 : 0.0;
            break;
        case MF_FAULT_FUEL_PRESSURE:
            /* 4.6: stumbles = misfire pulses; a hard top cut when the lean
             * loss passes half (FACT top-cut lever). */
            s.misfire_depth = mf_ai_clampd(o->misfire_depth, 0.0, MF_MISFIRE_MAX);
            if (o->loss_top > 0.5 && c->rev_limit_rads > c->peak_tq_rads) {
                double s0 = 0.5 / mf_ai_clampd(o->loss_top, 0.01, 1.0);
                s.top_cut_rads = mf_ai_smoothstep_inv(s0, c->peak_tq_rads,
                                                      c->rev_limit_rads);
                s.true_limit_rads = s.top_cut_rads;
            }
            break;
        case MF_FAULT_FUEL_LEAK:
            /* 4.7 open item: no visible AI symptom (owner default) */
            break;
        case MF_FAULT_BLOWBY:
            /* 4.8: blue = 1 - ring_s, loss_load shape */
            s.blue = mf_ai_clampd(1.0 - e->ring_s, 0.0, 1.0);
            break;
        case MF_FAULT_EXHAUST:
            /* 4.9: pops (Paris-law chance) and flame spits (levers) */
            s.pop_rate_s = o->pop_chance_s;
            break;
        case MF_FAULT_BLOWUP:
            /* N3b (owner D3): the rolled warning before the engine blows -
             * rough running only (misfire pulses = the rev dip; the grey
             * bank puff is applied by the caller). No notice ladder, no
             * limp, no reaction: the end is sudden. */
            s.misfire_depth = 0.3;
            break;
        default:
            break;
        }
    }
    /* 6 endings: blow-up is the only fireball; seize/stop differ by banks */
    if (e->blown) {
        s.ending = MF_AI_END_BLOWN;
        s.ending_reason = e->fault;
    } else if (e->stopped || e->culminated) {
        s.ending = MF_AI_END_STOP;
        s.ending_reason = e->fault;
    } else if (e->rear_broken) {
        s.ending = MF_AI_END_REAR;
        s.ending_reason = MF_FAULT_EXHAUST;
    }
    return s;
}

/* ------------------------------------------------------------------ */
/* B1: traits                                                          */
/* ------------------------------------------------------------------ */

/* .rcd values arrive as 0..1 (race_ability, crash) - the DLL normalises;
 * N(0,0.15) per trait, clamped (spec 3.5). */
static inline AiTraits mf_ai_traits_roll(double race_ability, double crash_val,
                                         MfRng *rng)
{
    AiTraits t;
    t.sensitivity = mf_ai_clampd(race_ability + mf_ai_normal(rng) * 0.15, 0.0, 1.0);
    t.sympathy    = mf_ai_clampd((1.0 - crash_val) + mf_ai_normal(rng) * 0.15, 0.0, 1.0);
    t.racer       = mf_ai_clampd(0.5 + mf_ai_normal(rng) * 0.15, 0.0, 1.0);
    t.team_order  = mf_ai_clampd(0.5 + mf_ai_normal(rng) * 0.15, 0.0, 1.0);
    t.crash       = mf_ai_clampd(crash_val + mf_ai_normal(rng) * 0.15, 0.0, 1.0);
    t.valid = 1;
    return t;
}

static inline double mf_ai_notice_base(int fault)
{
    switch (fault) {
    case MF_FAULT_VALVE_AIR:     return MF_AIR_NOTICE_VALVE;
    case MF_FAULT_OIL_PRESSURE:  return MF_AIR_NOTICE_OILP;
    case MF_FAULT_OIL_LEAK:      return MF_AIR_NOTICE_OILLEAK;
    case MF_FAULT_WATER_LEAK:    return MF_AIR_NOTICE_WATER;
    case MF_FAULT_RAD_DEBRIS:    return MF_AIR_NOTICE_RAD;
    case MF_FAULT_FUEL_PRESSURE: return MF_AIR_NOTICE_FUELP;
    case MF_FAULT_FUEL_LEAK:     return MF_AIR_NOTICE_FUELLEAK;
    case MF_FAULT_BLOWBY:        return MF_AIR_NOTICE_BLOWBY;
    case MF_FAULT_EXHAUST:       return MF_AIR_NOTICE_EXHAUST;
    case MF_FAULT_BLOWUP:        return 0.0;   /* N3b: no ladder at all */
    default:                     return 20.0;
    }
}

/* 3.4: base x sens_factor x LogNormal(0, 0.4) */
static inline double mf_ai_notice_delay(int fault, const AiTraits *t, MfRng *rng)
{
    double sens = 2.0 - 1.5 * mf_ai_clampd(t->sensitivity, 0.0, 1.0);
    double ln = exp(mf_ai_normal(rng) * MF_AIR_NOTICE_SIGMA);
    return mf_ai_notice_base(fault) * sens * ln;
}

/* no-fault-limit cap fraction (spec section 5 + per-fault overrides) */
static inline double mf_ai_no_limit_frac(int fault, int level)
{
    if (level <= 1) {
        switch (fault) {
        case MF_FAULT_OIL_PRESSURE:  return MF_AIR_CAP_R1_OILP;
        case MF_FAULT_OIL_LEAK:      return MF_AIR_CAP_R1_OILLEAK;
        case MF_FAULT_WATER_LEAK:
        case MF_FAULT_RAD_DEBRIS:    return MF_AIR_CAP_R1_WATER;
        case MF_FAULT_FUEL_PRESSURE: return MF_AIR_CAP_R1_FUELP;
        case MF_FAULT_FUEL_LEAK:     return MF_AIR_CAP_R1_FUELLEAK;
        case MF_FAULT_BLOWBY:        return MF_AIR_CAP_R1_BLOWBY;
        case MF_FAULT_EXHAUST:       return MF_AIR_CAP_R1_EXHAUST;
        default:                     return 1.0;
        }
    }
    switch (fault) {
    case MF_FAULT_OIL_PRESSURE:  return MF_AIR_CAP_R2_OILP;
    case MF_FAULT_OIL_LEAK:      return MF_AIR_CAP_R2_OILLEAK;
    case MF_FAULT_WATER_LEAK:
    case MF_FAULT_RAD_DEBRIS:    return MF_AIR_CAP_R2_WATER;
    case MF_FAULT_FUEL_PRESSURE: return MF_AIR_CAP_R2_FUELP;
    case MF_FAULT_FUEL_LEAK:     return MF_AIR_CAP_R2_FUELLEAK;
    case MF_FAULT_BLOWBY:        return MF_AIR_CAP_R2_BLOWBY;
    case MF_FAULT_EXHAUST:       return MF_AIR_CAP_R1_EXHAUST;
    default:                     return 1.0;
    }
}

/* ------------------------------------------------------------------ */
/* B5: level choice (spec section 5 + per-fault mixes in 4.x)          */
/* ------------------------------------------------------------------ */

/* allowed levels per fault from the current state; returns a bitmask
 * (bit L = level L allowed) */
static inline unsigned mf_ai_level_mask(int fault, const MfEngine *e,
                                        const AiSymptom *s, double coolant_frac,
                                        double race_left_frac)
{
    unsigned m = (1u << 0);                       /* R0 always possible */
    switch (fault) {
    case MF_FAULT_VALVE_AIR:
        /* 4.1: never R3/R4 while the ceiling is above the floor */
        m |= (1u << 1);
        if (s->rev_ceiling_frac >= 0.0 && s->rev_ceiling_frac < MF_AIR_VALVE_R2)
            m |= (1u << 2);
        break;
    case MF_FAULT_OIL_PRESSURE:
        m |= (1u << 1);
        if (e->h_b < MF_AIR_HB_LIMP) m |= (1u << 2);
        m |= (1u << 3);                            /* stop to save the engine */
        if (e->h_b < MF_AIR_HB_RETIRE) m |= (1u << 4);
        break;
    case MF_FAULT_OIL_LEAK:
        m |= (1u << 1);
        if (e->v_oil_l < MF_CASCADE_OIL_FRAC * MF_OIL_START_L) m |= (1u << 2);
        if (e->v_oil_l < MF_AIR_SUMP_RETIRE * MF_OIL_START_L) m |= (1u << 4);
        break;
    case MF_FAULT_WATER_LEAK:
        m |= (1u << 1);
        if (coolant_frac < MF_AIR_THERM_R2) m |= (1u << 2);
        if (e->h_g < MF_AIR_HG_RETIRE) m |= (1u << 4);
        break;
    case MF_FAULT_RAD_DEBRIS:
        m |= (1u << 1);
        m |= (1u << 2);
        m |= (1u << 3);                            /* pit clear */
        break;
    case MF_FAULT_FUEL_PRESSURE:
        m |= (1u << 1);
        m |= (1u << 2);
        m |= (1u << 3);                            /* pit check */
        break;
    case MF_FAULT_FUEL_LEAK:
        m |= (1u << 1);
        m |= (1u << 3);                            /* splash */
        break;
    case MF_FAULT_BLOWBY:
        m |= (1u << 1);
        if (e->ring_s < MF_AIR_RING_R2) m |= (1u << 2);
        if (s->power_mult < MF_AIR_POWER_RETIRE) m |= (1u << 4);
        break;
    case MF_FAULT_EXHAUST:
        m |= (1u << 1);
        m |= (1u << 2);
        if (e->rear_broken) m |= (1u << 4);
        break;
    default:
        m |= (1u << 1);
        break;
    }
    (void)race_left_frac;
    return m;
}

/* weighted dice over the allowed levels (spec 4.x mixes + section 5
 * context modifiers). Always returns an allowed level. */
static inline int mf_ai_level_pick(unsigned mask, const AiTraits *t,
                                   const MfEngine *e, double race_left_frac,
                                   MfRng *rng)
{
    double w[5];
    double tot = 0.0, u;
    int f = e->fault, i;
    static const double mix[MF_FAULT_COUNT][5] = {
        /* R0  R1  R2  R3  R4   (reaction plan 4.1; section 4 overrides) */
        { 0.0, 0.0, 0.0, 0.0, 0.0 },
        { 10.0, 50.0, 30.0,  0.0,  0.0 },   /* valve: no pit fix (owner) */
        {  5.0, 25.0, 35.0,  5.0, 30.0 },   /* oil pressure */
        { 15.0, 45.0, 25.0,  0.0, 15.0 },   /* oil leak */
        { 15.0, 45.0, 25.0,  0.0, 15.0 },   /* water leak */
        { 10.0, 30.0, 20.0, 40.0,  0.0 },   /* radiator debris */
        { 15.0, 45.0, 35.0,  5.0,  0.0 },   /* fuel pressure */
        { 30.0, 40.0,  0.0, 30.0,  0.0 },   /* fuel leak */
        { 10.0, 40.0, 30.0,  0.0, 20.0 },   /* blow-by */
        { 30.0, 50.0, 15.0,  0.0,  5.0 },   /* exhaust */
        {  0.0,  0.0,  0.0,  0.0,  0.0 },   /* blow-up: no reaction ladder */
    };
    if (f < 1 || f >= MF_FAULT_COUNT) f = MF_FAULT_OIL_PRESSURE;
    for (i = 0; i < 5; i++) w[i] = mix[f][i];

    /* section 5 context modifiers (weights shift, never hard rules) */
    if (e->severity == MF_SEV_BIG) {
        w[0] *= 0.3;
        w[2] *= 1.3;
        w[4] *= 1.5;
    }
    w[0] *= 1.0 + 0.6 * t->racer;
    w[4] *= 1.0 - 0.5 * t->racer;
    w[1] *= 1.0 + 0.6 * t->team_order;
    w[4] *= 1.0 - 0.5 * t->team_order;
    if (race_left_frac < MF_AIR_END_FRAC) {           /* bring it home */
        w[0] *= 1.5;
        w[3] *= 0.5;
        w[4] *= 0.5;
    }
    for (i = 0; i < 5; i++)
        if (!(mask & (1u << i))) w[i] = 0.0;
    /* a noticed driver never drops to R0 (spec 5) */
    if (mask & (1u << 1)) w[0] = 0.0;
    for (i = 0; i < 5; i++) tot += w[i];
    if (!(tot > 0.0)) return (mask & (1u << 1)) ? 1 : 0;
    u = mf_rng_d(rng) * tot;
    for (i = 0; i < 5; i++) {
        if (u < w[i]) return i;
        u -= w[i];
    }
    for (i = 4; i >= 0; i--)
        if (mask & (1u << i)) return i;
    return 1;
}

/* clear worsening signature (spec 5: severity stage, ceiling -3 %, health
 * step 0.1); -1 before onset */
static inline double mf_ai_sig(const MfEngine *e, const AiSymptom *s,
                               double coolant_frac)
{
    double sig;
    if (!e->onset) return -1.0;
    sig = (double)e->severity * 10.0;
    if (s->rev_ceiling_frac >= 0.0)
        sig += (1.0 - s->rev_ceiling_frac) / MF_AIR_CEIL_STEP * 0.1;
    sig += (1.0 - mf_ai_clampd(e->h_b, 0.0, 1.0)) / MF_AIR_HEALTH_STEP * 0.2;
    sig += (1.0 - mf_ai_clampd(e->h_vt, 0.0, 1.0)) / MF_AIR_HEALTH_STEP * 0.2;
    sig += (1.0 - mf_ai_clampd(e->ring_s, 0.0, 1.0)) / MF_AIR_HEALTH_STEP * 0.2;
    sig += (1.0 - mf_ai_clampd(e->h_g, 0.0, 1.0)) / MF_AIR_HEALTH_STEP * 0.2;
    sig += (1.0 - coolant_frac) * 0.5;
    return sig;
}

/* 3.2 cap from the current reaction (M1 lifts it to the full limit) */
static inline double mf_ai_cap_rads(const AiReaction *r, const AiSymptom *s,
                                    const MfCar *c)
{
    double marg;
    if (!s->onset) return c->rev_limit_rads;
    if (r->m1_left_s > 0.0) return c->rev_limit_rads;   /* mistake M1 */
    if (r->level <= 0) return c->rev_limit_rads;
    marg = r->m2_left_s > 0.0 ? 0.0 : r->margin;        /* mistake M2 */
    if (s->true_limit_rads > 0.0)
        return s->true_limit_rads * (1.0 + r->perceived_off) * (1.0 - marg);
    return c->rev_limit_rads * mf_ai_no_limit_frac(s->fault, r->level)
           * (1.0 - 0.0 * marg);                        /* table already has
                                                         * the margin built in */
}

/* ------------------------------------------------------------------ */
/* B5: the reaction step                                               */
/* ------------------------------------------------------------------ */

static inline void mf_ai_react_init(AiReaction *r)
{
    memset(r, 0, sizeof *r);
    r->level = 0;
    r->margin = 0.0;
    r->passing_mult = 1.0;
    r->aggression_mult = 1.0;
    r->best_sig = -1.0;
}

/* Context: race_left_frac = fraction of the race distance still to run
 * (1 at the green flag, 0 at the flag); gap_s = time gap to the car ahead
 * (< 0 = none); coolant_frac = v_w_l / MF_WATER_START_L. */
static inline void mf_ai_react(AiReaction *r, const AiSymptom *s,
                               const MfEngine *e, const AiTraits *t,
                               MfRng *rng, double dt, double t_s,
                               double race_left_frac, double gap_s,
                               double coolant_frac)
{
    double sig;
    int battle;

    if (dt <= 0.0) return;
    if (!e->onset || s->ending != MF_AI_END_NONE) {
        /* fault over: back to R0; the DLL restores the levers */
        mf_ai_react_init(r);
        return;
    }
    /* N3b (owner D3): BLOW-UP has no notice/reaction ladder and no limp -
     * the driver gets no time to react to a sudden mechanical failure. */
    if (e->fault == MF_FAULT_BLOWUP) {
        mf_ai_react_init(r);
        return;
    }

    /* 3.4: roll the notice once at onset (M3 before notice can push it) */
    if (!r->started) {
        r->started = 1;
        r->notice_delay_s = mf_ai_notice_delay(e->fault, t, rng);
        r->notice_at_s = e->onset_t_s + r->notice_delay_s;
        r->level = 0;
        r->best_sig = -1.0;
    }

    battle = (gap_s >= 0.0 && gap_s < MF_AIR_CLOSE_GAP_S) ? 1 : 0;

    /* 3.3 mistakes: pre-notice only M3 amplifies the delay; after notice
     * M1/M2 affect the behaviour. One mistake at a time, cooldown between. */
    if (r->mistake_cool_s > 0.0) r->mistake_cool_s -= dt;
    if (r->mistake <= 0 && r->mistake_cool_s <= 0.0) {
        double ctx = 1.0, lam, p, u;
        int kind;
        if (battle) ctx *= MF_AIR_MISTAKE_CTX_BATTLE;
        if (race_left_frac < MF_AIR_END_FRAC) ctx *= MF_AIR_MISTAKE_CTX_END;
        if (r->level >= 2) ctx *= MF_AIR_MISTAKE_CTX_NURSE;
        lam = MF_AIR_MISTAKE_RATE * (1.0 + 2.0 * t->crash) * ctx;
        p = 1.0 - exp(-lam * dt);
        u = mf_rng_d(rng);
        if (u < p) {
            double k = mf_rng_d(rng);
            if (!r->noticed) {
                kind = k < MF_AIR_MISTAKE_P_M1 + MF_AIR_MISTAKE_P_M2 ? 0 : 3;
            } else {
                kind = k < MF_AIR_MISTAKE_P_M1 ? 1
                     : (k < MF_AIR_MISTAKE_P_M1 + MF_AIR_MISTAKE_P_M2 ? 2 : 1);
            }
            r->mistake_count++;
            if (kind == 3 && !r->noticed) {
                /* M3: another notice delay on top (driver missed it) */
                double extra = mf_ai_notice_delay(e->fault, t, rng);
                r->notice_delay_s += extra;
                r->notice_at_s += extra;
            } else if (kind == 1) {
                r->mistake = 1;
                r->m1_left_s = MF_AIR_M1_T_MIN_S
                    + (MF_AIR_M1_T_MAX_S - MF_AIR_M1_T_MIN_S) * mf_rng_d(rng);
            } else if (kind == 2) {
                r->mistake = 2;
                r->m2_left_s = MF_AIR_M2_LAP_S;
            }
            r->mistake_cool_s = MF_AIR_MISTAKE_COOL_S;
        }
    }
    if (r->mistake == 1 && r->m1_left_s > 0.0) {
        r->m1_left_s -= dt;
        if (r->m1_left_s <= 0.0) { r->m1_left_s = 0.0; r->mistake = 0; }
    } else if (r->mistake == 2 && r->m2_left_s > 0.0) {
        r->m2_left_s -= dt;
        if (r->m2_left_s <= 0.0) { r->m2_left_s = 0.0; r->mistake = 0; }
    }

    /* notice + level */
    if (!r->noticed && t_s >= r->notice_at_s) {
        unsigned mask;
        r->noticed = 1;
        mask = mf_ai_level_mask(e->fault, e, s, coolant_frac, race_left_frac);
        r->level = mf_ai_level_pick(mask, t, e, race_left_frac, rng);
        if (r->level < 1) r->level = 1;
        r->level_t_s = t_s;
        r->perceived_off = mf_ai_normal(rng)
            * (MF_AIR_SIGMA_P_LOW
               - (MF_AIR_SIGMA_P_LOW - MF_AIR_SIGMA_P_SENS)
                 * mf_ai_clampd(t->sensitivity, 0.0, 1.0));
        r->best_sig = mf_ai_sig(e, s, coolant_frac);
    } else if (r->noticed) {
        /* escalation only on a clear worsening (spec 5) */
        sig = mf_ai_sig(e, s, coolant_frac);
        if (sig > r->best_sig + 1e-9) {
            unsigned mask = mf_ai_level_mask(e->fault, e, s, coolant_frac,
                                             race_left_frac);
            int n = mf_ai_level_pick(mask, t, e, race_left_frac, rng);
            if (n > r->level) r->level = n;
            r->best_sig = sig;
            r->perceived_off = mf_ai_normal(rng)
                * (MF_AIR_SIGMA_P_LOW
                   - (MF_AIR_SIGMA_P_LOW - MF_AIR_SIGMA_P_SENS)
                     * mf_ai_clampd(t->sensitivity, 0.0, 1.0));
        }
        if (r->level < 1) r->level = 1;      /* never below R1 once noticed */
    }

    /* level -> margins, passing, aggression (section 5 table) */
    switch (r->level) {
    case 0:
        r->margin = 0.0;
        r->passing_mult = 1.0;
        r->aggression_mult = 1.0;
        break;
    case 1:
        r->margin = MF_AIR_MARGIN_R1_LO
                  + (MF_AIR_MARGIN_R1_HI - MF_AIR_MARGIN_R1_LO)
                    * mf_ai_clampd(t->sympathy, 0.0, 1.0);
        r->passing_mult = 1.0;
        r->aggression_mult = 0.8;
        break;
    case 2:
        r->margin = MF_AIR_MARGIN_R2;
        r->passing_mult = 0.3;
        r->aggression_mult = 0.5;
        break;
    case 3:
        r->margin = MF_AIR_MARGIN_R2;
        r->passing_mult = 0.3;
        r->aggression_mult = 0.5;
        r->pit = 1;
        break;
    default:
        r->margin = MF_AIR_MARGIN_R2;
        r->passing_mult = 0.0;
        r->aggression_mult = 0.5;
        r->retire = 1;
        break;
    }
    r->cap_rads = 0.0;   /* caller computes with mf_ai_cap_rads (car needed) */
}

#endif /* MF_AI_REACT_H */
