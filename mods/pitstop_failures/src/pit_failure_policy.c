/*
 * pit_failure_policy.c — pure policy, validation, RNG and timing math.
 * See pit_failure_policy.h for the units contract. No OS/game dependencies.
 */
#include "pit_failure_policy.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ RNG -- */

uint64_t pf_seed_mix(uint64_t x)
{
    x += UINT64_C(0x9E3779B97F4A7C15);
    x = (x ^ (x >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94D049BB133111EB);
    return x ^ (x >> 31);
}

void pf_rng_seed(pf_rng_t *rng, uint64_t seed)
{
    if (!rng) {
        return;
    }
    /* All uint64 values are legal, including zero. */
    rng->state = seed;
}

uint64_t pf_rng_next_u64(pf_rng_t *rng)
{
    uint64_t z;

    if (!rng) {
        return 0;
    }
    rng->state += UINT64_C(0x9E3779B97F4A7C15);
    z = rng->state;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

uint64_t pf_stream_seed(uint64_t seed, uint64_t epoch, uint64_t vehicle_id,
                        uint64_t service_token, uint32_t domain)
{
    uint64_t h = seed;
    h = pf_seed_mix(h ^ (epoch * UINT64_C(0xD6E8FEB86659FD93)));
    h = pf_seed_mix(h ^ (vehicle_id * UINT64_C(0xA24BAED4963EE407)));
    h = pf_seed_mix(h ^ (service_token * UINT64_C(0x9FB21C651E98DF25)));
    h = pf_seed_mix(h ^ (uint64_t)domain);
    return h;
}

uint64_t pf_rng_below(pf_rng_t *rng, uint64_t bound)
{
    uint64_t threshold;
    uint64_t value;

    if (!rng || bound == 0) {
        return 0;
    }
    /* Rejection sampling: unbiased for every bound, including huge ones. */
    threshold = (UINT64_C(0) - bound) % bound;
    for (;;) {
        value = pf_rng_next_u64(rng);
        if (value >= threshold) {
            return value % bound;
        }
    }
}

double pf_rng_unit(pf_rng_t *rng)
{
    uint64_t value = pf_rng_next_u64(rng) >> 11;
    return (double)value * (1.0 / 9007199254740992.0);
}

double pf_sample_triangular(pf_rng_t *rng, pf_duration_band_t band)
{
    double lower = band.min;
    double mode = band.mode;
    double upper = band.max;
    double span;
    double split;
    double u;
    double sample;

    if (upper <= lower) {
        return lower;
    }
    span = upper - lower;
    split = (mode - lower) / span;
    if (split < 0.0) {
        split = 0.0;
    }
    if (split > 1.0) {
        split = 1.0;
    }
    u = pf_rng_unit(rng);
    if (u < split) {
        sample = lower + sqrt(u * span * (mode - lower));
    } else {
        sample = upper - sqrt((1.0 - u) * span * (upper - mode));
    }
    if (!(sample >= lower)) {
        sample = lower;
    }
    if (sample > upper) {
        sample = upper;
    }
    return sample;
}

/* --------------------------------------------------------- configuration -- */

static void set_band(pf_duration_band_t *band, double min, double mode, double max)
{
    band->min = min;
    band->mode = mode;
    band->max = max;
}

void pf_config_defaults(pf_config_t *cfg)
{
    if (!cfg) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->crew_risk_percent = 100;
    cfg->weights.slow_corner_bp = 280;
    cfg->weights.wheelgun_bp = 100;
    cfg->weights.fuel_rig_bp = 90;
    cfg->weights.stall_delay_bp = 20;
    cfg->weights.cross_thread_bp = 10;
    cfg->weights.terminal_wheel_bp = 1;
    cfg->recoverable_cap_bp = 1200;
    set_band(&cfg->durations.slow_corner, 1.0, 2.5, 5.0);
    set_band(&cfg->durations.wheelgun, 3.0, 5.0, 10.0);
    set_band(&cfg->durations.fuel_slow_flow, 3.0, 6.0, 12.0);
    set_band(&cfg->durations.fuel_temporary_no_flow, 4.0, 8.0, 16.0);
    set_band(&cfg->durations.fuel_interrupted_flow, 3.0, 6.0, 12.0);
    set_band(&cfg->durations.stall_delay, 2.0, 4.0, 8.0);
    set_band(&cfg->durations.cross_thread, 10.0, 18.0, 35.0);
    set_band(&cfg->durations.terminal_diagnosis, 8.0, 12.0, 20.0);
    cfg->corner_weight[PF_WHEEL_FL] = 25;
    cfg->corner_weight[PF_WHEEL_FR] = 25;
    cfg->corner_weight[PF_WHEEL_RL] = 25;
    cfg->corner_weight[PF_WHEEL_RR] = 25;
    cfg->fuel_weight[0] = 0;
    cfg->fuel_weight[PF_FUEL_SUBTYPE_SLOW_FLOW] = 70;
    cfg->fuel_weight[PF_FUEL_SUBTYPE_TEMPORARY_NO_FLOW] = 20;
    cfg->fuel_weight[PF_FUEL_SUBTYPE_INTERRUPTED_FLOW] = 10;
    cfg->late_request_bonus = 15;
    cfg->late_change_bonus = 15;
    cfg->wet_bonus = 10;
    cfg->positioning_bonus = 25;
}

static int band_valid(const pf_duration_band_t *band)
{
    if (!(band->min >= PF_SECONDS_MIN && band->min <= PF_SECONDS_MAX)) {
        return 0;
    }
    if (!(band->mode >= band->min && band->mode <= PF_SECONDS_MAX)) {
        return 0;
    }
    if (!(band->max >= band->mode && band->max <= PF_SECONDS_MAX)) {
        return 0;
    }
    return 1;
}

static int range_int(const char *name, long value, long min, long max,
                     char *err, size_t err_len)
{
    if (value < min || value > max) {
        if (err && err_len) {
            snprintf(err, err_len, "%s out of range: %ld (allowed %ld..%ld)",
                     name, value, min, max);
        }
        return 0;
    }
    return 1;
}

pf_config_status_t pf_config_validate(pf_config_t *cfg, char *err, size_t err_len)
{
    int i;
    int recoverable_sum;
    int effective_bp;

    if (!cfg) {
        return PF_CONFIG_RANGE;
    }
    if (!range_int("crew_risk_percent", cfg->crew_risk_percent,
                   PF_CREW_RISK_MIN, PF_CREW_RISK_MAX, err, err_len)) {
        return PF_CONFIG_RANGE;
    }
    if (!range_int("slow_corner_bp", cfg->weights.slow_corner_bp,
                   0, PF_BP_BASE_SUM_MAX, err, err_len) ||
        !range_int("wheelgun_bp", cfg->weights.wheelgun_bp,
                   0, PF_BP_BASE_SUM_MAX, err, err_len) ||
        !range_int("fuel_rig_bp", cfg->weights.fuel_rig_bp,
                   0, PF_BP_BASE_SUM_MAX, err, err_len) ||
        !range_int("stall_delay_bp", cfg->weights.stall_delay_bp,
                   0, PF_BP_BASE_SUM_MAX, err, err_len) ||
        !range_int("cross_thread_bp", cfg->weights.cross_thread_bp,
                   0, PF_BP_BASE_SUM_MAX, err, err_len)) {
        return PF_CONFIG_RANGE;
    }
    recoverable_sum = cfg->weights.slow_corner_bp + cfg->weights.wheelgun_bp +
                      cfg->weights.fuel_rig_bp + cfg->weights.stall_delay_bp +
                      cfg->weights.cross_thread_bp;
    if (recoverable_sum > PF_BP_BASE_SUM_MAX) {
        if (err && err_len) {
            snprintf(err, err_len,
                     "recoverable base sum %d bp exceeds %d bp",
                     recoverable_sum, PF_BP_BASE_SUM_MAX);
        }
        return PF_CONFIG_RANGE;
    }
    if (!range_int("terminal_wheel_bp", cfg->weights.terminal_wheel_bp,
                   0, PF_BP_TERMINAL_MAX, err, err_len)) {
        return PF_CONFIG_RANGE;
    }
    if (!range_int("recoverable_cap_bp", cfg->recoverable_cap_bp,
                   PF_BP_RECOVERABLE_CAP_MIN, PF_BP_RECOVERABLE_CAP_MAX,
                   err, err_len)) {
        return PF_CONFIG_RANGE;
    }
    if (!range_int("late_request_bonus", cfg->late_request_bonus,
                   0, PF_CONTEXT_BONUS_MAX, err, err_len) ||
        !range_int("late_change_bonus", cfg->late_change_bonus,
                   0, PF_CONTEXT_BONUS_MAX, err, err_len) ||
        !range_int("wet_bonus", cfg->wet_bonus,
                   0, PF_CONTEXT_BONUS_MAX, err, err_len) ||
        !range_int("positioning_bonus", cfg->positioning_bonus,
                   0, PF_CONTEXT_BONUS_MAX, err, err_len)) {
        return PF_CONFIG_RANGE;
    }
    {
        int corner_total = 0;
        for (i = 0; i < PF_WHEEL_COUNT; i++) {
            if (!range_int("corner_weight", cfg->corner_weight[i], 0, 1000000,
                           err, err_len)) {
                return PF_CONFIG_RANGE;
            }
            corner_total += cfg->corner_weight[i];
        }
        if (corner_total <= 0) {
            if (err && err_len) {
                snprintf(err, err_len, "corner weights must have a positive sum");
            }
            return PF_CONFIG_RANGE;
        }
    }
    {
        int fuel_total = 0;
        for (i = 1; i <= 3; i++) {
            if (!range_int("fuel_subtype_weight", cfg->fuel_weight[i], 0, 1000000,
                           err, err_len)) {
                return PF_CONFIG_RANGE;
            }
            fuel_total += cfg->fuel_weight[i];
        }
        if (fuel_total <= 0) {
            if (err && err_len) {
                snprintf(err, err_len, "fuel subtype weights must have a positive sum");
            }
            return PF_CONFIG_RANGE;
        }
    }
    if (!band_valid(&cfg->durations.slow_corner) ||
        !band_valid(&cfg->durations.wheelgun) ||
        !band_valid(&cfg->durations.fuel_slow_flow) ||
        !band_valid(&cfg->durations.fuel_temporary_no_flow) ||
        !band_valid(&cfg->durations.fuel_interrupted_flow) ||
        !band_valid(&cfg->durations.stall_delay) ||
        !band_valid(&cfg->durations.cross_thread) ||
        !band_valid(&cfg->durations.terminal_diagnosis)) {
        if (err && err_len) {
            snprintf(err, err_len,
                     "duration bands must satisfy 0 <= min <= mode <= max <= 60");
        }
        return PF_CONFIG_RANGE;
    }

    /* Stacked worst case is bounded and cannot overflow int64 arithmetic. */
    {
        int64_t scaled_max = (int64_t)recoverable_sum * PF_MULTIPLIER_MAX;
        int64_t capped_max;
        if (scaled_max > INT64_C(0) &&
            scaled_max / PF_MULTIPLIER_MAX != recoverable_sum) {
            if (err && err_len) {
                snprintf(err, err_len, "risk arithmetic overflow");
            }
            return PF_CONFIG_RANGE;
        }
        capped_max = scaled_max / 100;
        if (capped_max > cfg->recoverable_cap_bp) {
            capped_max = cfg->recoverable_cap_bp;
        }
        effective_bp = (int)capped_max + cfg->weights.terminal_wheel_bp;
        if (effective_bp > PF_BP_UNIVERSE) {
            if (err && err_len) {
                snprintf(err, err_len,
                         "combined effective probability %d bp exceeds 10000 bp",
                         effective_bp);
            }
            return PF_CONFIG_RANGE;
        }
    }
    return PF_CONFIG_OK;
}

/* ------------------------------------------------------------- rate math -- */

static int score_context_multiplier(const pf_config_t *cfg, const pf_context_t *ctx)
{
    int multiplier = cfg->crew_risk_percent;

    if (!ctx) {
        return multiplier;
    }
    /* True adds the configured bonus; false and unknown add zero. */
    if (ctx->late_request == PF_TRISTATE_TRUE) {
        multiplier += cfg->late_request_bonus;
    }
    if (ctx->late_change == PF_TRISTATE_TRUE) {
        multiplier += cfg->late_change_bonus;
    }
    if (ctx->wet == PF_TRISTATE_TRUE) {
        multiplier += cfg->wet_bonus;
    }
    if (ctx->poor_positioning == PF_TRISTATE_TRUE) {
        multiplier += cfg->positioning_bonus;
    }
    if (multiplier < PF_MULTIPLIER_MIN) {
        multiplier = PF_MULTIPLIER_MIN;
    }
    if (multiplier > PF_MULTIPLIER_MAX) {
        multiplier = PF_MULTIPLIER_MAX;
    }
    return multiplier;
}

static int category_base(const pf_config_t *cfg, int index)
{
    switch (index) {
    case 0: return cfg->weights.slow_corner_bp;
    case 1: return cfg->weights.wheelgun_bp;
    case 2: return cfg->weights.fuel_rig_bp;
    case 3: return cfg->weights.stall_delay_bp;
    case 4: return cfg->weights.cross_thread_bp;
    default: return cfg->weights.terminal_wheel_bp;
    }
}

static int eligibility_mask(const pf_eligibility_t *e)
{
    int mask = 0;
    int fuel_ready;
    int service_ready;

    if (!e) {
        return 0;
    }
    fuel_ready = e->fuel_allowed && e->fuel_add_liters > 0;
    service_ready = e->real_service_possible;
    if (e->tyre_work) {
        mask |= 1 << 0; /* slow_corner */
        mask |= 1 << 1; /* wheelgun */
        mask |= 1 << 5; /* terminal_wheel */
    }
    if (fuel_ready) {
        mask |= 1 << 2; /* fuel_rig */
    }
    if (service_ready && e->release_possible) {
        mask |= 1 << 3; /* stall_delay */
    }
    if (e->tyre_work) {
        mask |= 1 << 4; /* cross_thread */
    }
    return mask;
}

static int compute_effective(const pf_config_t *cfg, const pf_context_t *ctx,
                             int multiplier_out[1], int effective[PF_CATEGORY_COUNT],
                             int scaled_sum_out[1])
{
    int mask;
    int multiplier;
    int64_t base_sum = 0;
    int64_t scaled[PF_CATEGORY_COUNT];
    int64_t remainder[PF_CATEGORY_COUNT];
    int64_t scaled_sum = 0;
    int64_t target;
    int64_t floor_sum = 0;
    int i;

    multiplier = score_context_multiplier(cfg, ctx);
    mask = eligibility_mask(ctx ? &ctx->eligibility : NULL);

    for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
        base_sum += category_base(cfg, i);
    }
    for (i = 0; i < PF_CATEGORY_COUNT; i++) {
        scaled[i] = 0;
        remainder[i] = 0;
    }

    /* Scale the recoverable categories before eligibility, in table order. */
    for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
        int64_t value = (int64_t)category_base(cfg, i) * multiplier;
        scaled[i] = value / 100;
        remainder[i] = value % 100;
    }

    /* Round the scaled total half-up and distribute by largest remainder. */
    target = (base_sum * (int64_t)multiplier + 50) / 100;
    for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
        floor_sum += scaled[i];
    }
    {
        int64_t deficit = target - floor_sum;
        while (deficit > 0) {
            int best = -1;
            for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
                if (best < 0 || remainder[i] > remainder[best]) {
                    best = i;
                }
            }
            if (best < 0 || remainder[best] <= 0) {
                break;
            }
            scaled[best] += 1;
            remainder[best] = 0;
            deficit--;
        }
        /* Exact sums can leave a remainder-free deficit; fill in table order. */
        i = 0;
        while (deficit > 0 && i < PF_RECOVERABLE_COUNT) {
            scaled[i] += 1;
            deficit--;
            i++;
        }
    }
    for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
        scaled_sum += scaled[i];
    }
    if (scaled_sum_out) {
        scaled_sum_out[0] = (int)scaled_sum;
    }

    /* Cap apportionment: largest-remainder over the scaled categories. */
    if (scaled_sum > cfg->recoverable_cap_bp) {
        int64_t cap = cfg->recoverable_cap_bp;
        int64_t assigned[PF_CATEGORY_COUNT];
        int64_t cap_floor_sum = 0;
        for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
            int64_t product = scaled[i] * cap;
            assigned[i] = product / scaled_sum;
            remainder[i] = product % scaled_sum;
            cap_floor_sum += assigned[i];
        }
        {
            int64_t deficit = cap - cap_floor_sum;
            while (deficit > 0) {
                int best = -1;
                for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
                    if (scaled[i] <= 0) {
                        continue;
                    }
                    if (best < 0 || remainder[i] > remainder[best]) {
                        best = i;
                    }
                }
                if (best < 0) {
                    break;
                }
                assigned[best] += 1;
                remainder[best] = 0;
                deficit--;
            }
        }
        for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
            scaled[i] = assigned[i];
        }
    }

    /* Apply eligibility: excluded categories are exactly zero and clean. */
    for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
        if (!(mask & (1 << i))) {
            scaled[i] = 0;
        }
    }
    scaled[5] = (mask & (1 << 5)) ? cfg->weights.terminal_wheel_bp : 0;

    for (i = 0; i < PF_CATEGORY_COUNT; i++) {
        int64_t value = scaled[i];
        if (value < 0) {
            value = 0;
        }
        if (value > PF_BP_UNIVERSE) {
            value = PF_BP_UNIVERSE;
        }
        effective[i] = (int)value;
    }
    if (multiplier_out) {
        multiplier_out[0] = multiplier;
    }
    return mask;
}

uint64_t pf_category_draw(const pf_identity_t *id, uint64_t seed)
{
    pf_rng_t rng;
    if (!id) {
        return PF_DRAW_MAX;
    }
    rng.state = pf_stream_seed(seed, id->epoch, id->vehicle_id, id->service_token,
                               PF_DOMAIN_CATEGORY);
    return pf_rng_below(&rng, PF_DRAW_MAX);
}

pf_outcome_t pf_bucket_for_draw(const pf_config_t *cfg, const pf_context_t *ctx,
                                uint32_t draw)
{
    int effective[PF_CATEGORY_COUNT];
    int64_t cumulative = 0;
    int i;

    if (!cfg || draw >= PF_DRAW_MAX) {
        return PF_OUTCOME_CLEAN;
    }
    compute_effective(cfg, ctx, NULL, effective, NULL);
    for (i = 0; i < PF_CATEGORY_COUNT; i++) {
        cumulative += effective[i];
        if ((int64_t)draw < cumulative) {
            /* Bucket order maps one-to-one onto the outcome enum, whose
             * clean member is zero; the cast is an identity here. */
            return (pf_outcome_t)(i + 1);
        }
    }
    return PF_OUTCOME_CLEAN;
}

pf_outcome_t pf_plan_category(const pf_config_t *cfg, const pf_identity_t *id,
                              const pf_context_t *ctx, uint64_t seed)
{
    if (!cfg || !id || !ctx) {
        return PF_OUTCOME_CLEAN;
    }
    return pf_bucket_for_draw(cfg, ctx, (uint32_t)pf_category_draw(id, seed));
}

void pf_rate_report(const pf_config_t *cfg, const pf_context_t *ctx,
                    pf_rate_report_t *out)
{
    int effective[PF_CATEGORY_COUNT];
    int multiplier = 100;
    int base_sum = 0;
    int scaled_sum_pre_cap = 0;
    int i;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!cfg) {
        return;
    }
    compute_effective(cfg, ctx, &multiplier, effective, &scaled_sum_pre_cap);
    for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
        base_sum += category_base(cfg, i);
    }
    out->multiplier_percent = multiplier;
    out->base_recoverable_sum_bp = base_sum;
    out->scaled_recoverable_sum_bp = scaled_sum_pre_cap;
    out->capped_recoverable_sum_bp = 0;
    for (i = 0; i < PF_RECOVERABLE_COUNT; i++) {
        out->capped_recoverable_sum_bp += effective[i];
    }
    out->terminal_bp = effective[5];
    out->effective_trouble_bp = out->capped_recoverable_sum_bp + effective[5];
    for (i = 0; i < PF_CATEGORY_COUNT; i++) {
        out->effective[i] = effective[i];
    }
}

/* ----------------------------------------------------------- plan build -- */

static int pick_weighted(pf_rng_t *rng, const int *weights, int count)
{
    int64_t total = 0;
    uint64_t draw;
    int64_t cumulative = 0;
    int i;

    for (i = 0; i < count; i++) {
        if (weights[i] > 0) {
            total += weights[i];
        }
    }
    if (total <= 0) {
        return 0;
    }
    draw = pf_rng_below(rng, (uint64_t)total);
    for (i = 0; i < count; i++) {
        if (weights[i] <= 0) {
            continue;
        }
        cumulative += weights[i];
        if ((int64_t)draw < cumulative) {
            return i;
        }
    }
    return count - 1;
}

int pf_plan_recoverable_attempts(pf_outcome_t outcome)
{
    switch (outcome) {
    case PF_OUTCOME_SLOW_CORNER:
        return 1;
    case PF_OUTCOME_STALL_DELAY:
        return 1;
    case PF_OUTCOME_WHEELGUN:
    case PF_OUTCOME_FUEL_RIG:
    case PF_OUTCOME_CROSS_THREAD:
        return 2;
    default:
        return 0;
    }
}

pf_outcome_t pf_plan_build(const pf_config_t *cfg, const pf_identity_t *id,
                           const pf_context_t *ctx, uint64_t seed,
                           pf_plan_t *out)
{
    pf_rng_t category_rng;
    pf_rng_t duration_rng;
    pf_outcome_t outcome;

    if (!out) {
        return PF_OUTCOME_CLEAN;
    }
    memset(out, 0, sizeof(*out));
    out->outcome = PF_OUTCOME_CLEAN;
    out->severity = PF_CLEAN;
    /* Sentinel must not collide with any outcome enum member; PF_CATEGORY_COUNT
     * equals the terminal outcome, so use one past it. */
    out->category_index = PF_CATEGORY_COUNT + 1;
    out->affected_job_kind = PF_JOB_UNKNOWN;
    out->wheel = -1;
    out->fuel_subtype = PF_FUEL_SUBTYPE_NONE;
    if (!cfg || !id) {
        return out->outcome;
    }

    category_rng.state = pf_stream_seed(seed, id->epoch, id->vehicle_id,
                                        id->service_token, PF_DOMAIN_CATEGORY);
    {
        uint64_t draw = pf_rng_below(&category_rng, PF_DRAW_MAX);
        outcome = pf_bucket_for_draw(cfg, ctx, (uint32_t)draw);
        /* category_index is the outcome enum, or PF_CATEGORY_COUNT + 1 for a
         * clean draw; the sentinel must not collide with terminal_wheel. */
        out->category_index = outcome == PF_OUTCOME_CLEAN
                                  ? PF_CATEGORY_COUNT + 1
                                  : (int)outcome;
    }
    out->outcome = outcome;
    if (outcome == PF_OUTCOME_CLEAN) {
        return out->outcome;
    }

    duration_rng.state = pf_stream_seed(seed, id->epoch, id->vehicle_id,
                                        id->service_token, PF_DOMAIN_DURATION);

    switch (outcome) {
    case PF_OUTCOME_SLOW_CORNER:
        out->severity = PF_RECOVERABLE;
        out->wheel = pick_weighted(&category_rng, cfg->corner_weight, PF_WHEEL_COUNT);
        out->attempts = pf_plan_recoverable_attempts(outcome);
        out->affected_job_kind = PF_JOB_TYRE;
        out->extra_job_seconds =
            pf_sample_triangular(&duration_rng, cfg->durations.slow_corner);
        break;
    case PF_OUTCOME_WHEELGUN:
        out->severity = PF_RECOVERABLE;
        out->wheel = pick_weighted(&category_rng, cfg->corner_weight, PF_WHEEL_COUNT);
        out->attempts = pf_plan_recoverable_attempts(outcome);
        out->affected_job_kind = PF_JOB_TYRE;
        out->extra_job_seconds =
            pf_sample_triangular(&duration_rng, cfg->durations.wheelgun);
        break;
    case PF_OUTCOME_FUEL_RIG: {
        int subtype;
        out->severity = PF_RECOVERABLE;
        out->attempts = pf_plan_recoverable_attempts(outcome);
        out->affected_job_kind = PF_JOB_FUEL;
        subtype = pick_weighted(&category_rng, cfg->fuel_weight, 4);
        if (subtype < PF_FUEL_SUBTYPE_SLOW_FLOW ||
            subtype > PF_FUEL_SUBTYPE_INTERRUPTED_FLOW) {
            subtype = PF_FUEL_SUBTYPE_SLOW_FLOW;
        }
        out->fuel_subtype = (pf_fuel_subtype_t)subtype;
        switch (out->fuel_subtype) {
        case PF_FUEL_SUBTYPE_SLOW_FLOW:
            out->extra_job_seconds =
                pf_sample_triangular(&duration_rng, cfg->durations.fuel_slow_flow);
            break;
        case PF_FUEL_SUBTYPE_TEMPORARY_NO_FLOW:
            out->extra_job_seconds = pf_sample_triangular(
                &duration_rng, cfg->durations.fuel_temporary_no_flow);
            break;
        default:
            out->extra_job_seconds = pf_sample_triangular(
                &duration_rng, cfg->durations.fuel_interrupted_flow);
            break;
        }
        break;
    }
    case PF_OUTCOME_STALL_DELAY:
        out->severity = PF_RECOVERABLE;
        out->attempts = pf_plan_recoverable_attempts(outcome);
        out->affected_job_kind = PF_JOB_STALL;
        out->extra_job_seconds =
            pf_sample_triangular(&duration_rng, cfg->durations.stall_delay);
        break;
    case PF_OUTCOME_CROSS_THREAD:
        out->severity = PF_RECOVERABLE;
        out->wheel = pick_weighted(&category_rng, cfg->corner_weight, PF_WHEEL_COUNT);
        out->attempts = pf_plan_recoverable_attempts(outcome);
        out->affected_job_kind = PF_JOB_TYRE;
        out->extra_job_seconds =
            pf_sample_triangular(&duration_rng, cfg->durations.cross_thread);
        break;
    case PF_OUTCOME_TERMINAL_WHEEL:
        out->severity = PF_TERMINAL;
        out->terminal = 1;
        out->wheel = pick_weighted(&category_rng, cfg->corner_weight, PF_WHEEL_COUNT);
        out->attempts = 0;
        out->affected_job_kind = PF_JOB_TYRE;
        out->extra_job_seconds = 0.0;
        out->terminal_presentation_seconds =
            pf_sample_triangular(&duration_rng, cfg->durations.terminal_diagnosis);
        break;
    default:
        break;
    }
    return out->outcome;
}

/* --------------------------------------------------------------- timing -- */

static int collect_end(const pf_job_t *jobs, size_t n, pf_job_kind_t kind,
                       double *out)
{
    int found = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        double value = jobs[i].end_seconds;
        if (jobs[i].kind != kind) {
            continue;
        }
        if (!isfinite(value) || value < 0.0) {
            return 0;
        }
        if (!found || value > *out) {
            *out = value;
        }
        found = 1;
    }
    return found;
}

int pf_plan_apply_timing(const pf_plan_t *plan, const pf_job_t *jobs, size_t n,
                         pf_timing_result_t *out)
{
    double normal_end = 0.0;
    double affected_end = 0.0;
    double other_max = 0.0;
    int have_any = 0;
    int have_timing = 0;
    size_t i;

    if (!out) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (!plan) {
        return 0;
    }
    if (plan->severity == PF_CLEAN) {
        out->known = 0;
        out->added_stop_time = 0.0;
        return 0;
    }
    if (plan->severity == PF_TERMINAL) {
        /* No supplied timing may be invented for terminal retirement. */
        out->known = 0;
        out->added_stop_time = 0.0;
        return 0;
    }

    for (i = 0; i < n && i < PF_JOB_MAX; i++) {
        double value = jobs[i].end_seconds;
        if (jobs[i].kind == PF_JOB_UNKNOWN) {
            continue;
        }
        if (!isfinite(value) || value < 0.0) {
            out->known = 0;
            return 0;
        }
        if (!have_any || value > normal_end) {
            normal_end = value;
        }
        have_any = 1;
    }
    if (!have_any) {
        out->known = 0;
        return 0;
    }

    if (plan->affected_job_kind == PF_JOB_STALL) {
        /* Stall occurs after normal work and adds directly. */
        affected_end = normal_end;
        have_timing = 1;
    } else {
        pf_job_kind_t kind = (pf_job_kind_t)plan->affected_job_kind;
        if (kind == PF_JOB_OTHER) {
            kind = PF_JOB_OTHER;
        }
        if (!collect_end(jobs, n, kind, &affected_end)) {
            out->known = 0;
            return 0;
        }
        have_timing = 1;
    }
    if (!have_timing) {
        out->known = 0;
        return 0;
    }

    for (i = 0; i < n && i < PF_JOB_MAX; i++) {
        double value = jobs[i].end_seconds;
        if (jobs[i].kind == PF_JOB_UNKNOWN || !isfinite(value) || value < 0.0) {
            continue;
        }
        if (plan->affected_job_kind == PF_JOB_STALL ||
            jobs[i].kind != (pf_job_kind_t)plan->affected_job_kind) {
            if (value > other_max) {
                other_max = value;
            }
        }
    }
    out->known = 1;
    out->normal_end = normal_end;
    out->fault_end = other_max > affected_end + plan->extra_job_seconds
                         ? other_max
                         : affected_end + plan->extra_job_seconds;
    out->added_stop_time = out->fault_end - normal_end;
    if (out->added_stop_time < 0.0) {
        out->added_stop_time = 0.0;
    }
    return 1;
}

/* --------------------------------------------------------------- naming -- */
const char *pf_outcome_name(pf_outcome_t outcome)
{
    switch (outcome) {
    case PF_OUTCOME_CLEAN: return "clean";
    case PF_OUTCOME_SLOW_CORNER: return "slow_corner";
    case PF_OUTCOME_WHEELGUN: return "wheelgun";
    case PF_OUTCOME_FUEL_RIG: return "fuel_rig";
    case PF_OUTCOME_STALL_DELAY: return "stall_delay";
    case PF_OUTCOME_CROSS_THREAD: return "cross_thread";
    case PF_OUTCOME_TERMINAL_WHEEL: return "terminal_wheel";
    default: return "unknown";
    }
}

const char *pf_fuel_subtype_name(pf_fuel_subtype_t subtype)
{
    switch (subtype) {
    case PF_FUEL_SUBTYPE_SLOW_FLOW: return "slow_flow";
    case PF_FUEL_SUBTYPE_TEMPORARY_NO_FLOW: return "temporary_no_flow";
    case PF_FUEL_SUBTYPE_INTERRUPTED_FLOW: return "interrupted_flow";
    case PF_FUEL_SUBTYPE_SPILL: return "spill";
    default: return "none";
    }
}

const char *pf_wheel_name(int wheel)
{
    switch (wheel) {
    case PF_WHEEL_FL: return "FL";
    case PF_WHEEL_FR: return "FR";
    case PF_WHEEL_RL: return "RL";
    case PF_WHEEL_RR: return "RR";
    default: return "none";
    }
}

const char *pf_session_state_name(int state)
{
    switch (state) {
    case PF_SESSION_IDLE: return "idle";
    case PF_SESSION_SERVICE_PLANNED: return "service-planned";
    case PF_SESSION_RESOLVING: return "resolving";
    case PF_SESSION_RESOLVED: return "resolved";
    case PF_SESSION_CANCELLED: return "cancelled";
    case PF_SESSION_TERMINAL_REQUESTED: return "terminal-requested";
    default: return "unknown";
    }
}

const char *pf_command_kind_name(int kind)
{
    switch (kind) {
    case PF_CMD_FAULT_NOTICE: return "fault-notice";
    case PF_CMD_RECOVERY_ATTEMPT: return "recovery-attempt";
    case PF_CMD_RECOVERED: return "recovered";
    case PF_CMD_TERMINAL_REQUEST: return "terminal-request";
    default: return "none";
    }
}

/* ---------------------------------------------------------------- parser --
 *
 * Strict INI schema (schema_version=2). Duplicate sections or keys, unknown
 * keys, malformed numbers, out-of-range values, a missing schema marker and
 * unparseable lines are all fatal. Validation happens on a temporary copy;
 * the caller's config is only published after complete success.
 */

enum {
    SEC_GENERAL = 0,
    SEC_PROBABILITY = 1,
    SEC_DURATION = 2,
    SEC_CORNER = 3,
    SEC_FUEL = 4,
    SEC_CONTEXT = 5,
    SEC_COUNT = 6
};

enum {
    GEN_CREW_RISK = 0,
    GEN_SCHEMA_VERSION = 1,
    GEN_KEY_COUNT = 2
};

enum {
    PROB_SLOW_CORNER = 0,
    PROB_WHEELGUN = 1,
    PROB_FUEL_RIG = 2,
    PROB_STALL_DELAY = 3,
    PROB_CROSS_THREAD = 4,
    PROB_TERMINAL_WHEEL = 5,
    PROB_RECOVERABLE_CAP = 6,
    PROB_KEY_COUNT = 7
};

enum {
    DUR_SLOW_CORNER_MIN = 0,
    DUR_SLOW_CORNER_MODE,
    DUR_SLOW_CORNER_MAX,
    DUR_WHEELGUN_MIN,
    DUR_WHEELGUN_MODE,
    DUR_WHEELGUN_MAX,
    DUR_FUEL_SLOW_MIN,
    DUR_FUEL_SLOW_MODE,
    DUR_FUEL_SLOW_MAX,
    DUR_FUEL_NO_FLOW_MIN,
    DUR_FUEL_NO_FLOW_MODE,
    DUR_FUEL_NO_FLOW_MAX,
    DUR_FUEL_INTERRUPTED_MIN,
    DUR_FUEL_INTERRUPTED_MODE,
    DUR_FUEL_INTERRUPTED_MAX,
    DUR_STALL_MIN,
    DUR_STALL_MODE,
    DUR_STALL_MAX,
    DUR_CROSS_THREAD_MIN,
    DUR_CROSS_THREAD_MODE,
    DUR_CROSS_THREAD_MAX,
    DUR_TERMINAL_MIN,
    DUR_TERMINAL_MODE,
    DUR_TERMINAL_MAX,
    DUR_KEY_COUNT
};

enum {
    CORNER_FL = 0,
    CORNER_FR,
    CORNER_RL,
    CORNER_RR,
    CORNER_KEY_COUNT
};

enum {
    FUEL_SLOW_FLOW = 0,
    FUEL_TEMPORARY_NO_FLOW,
    FUEL_INTERRUPTED_FLOW,
    FUEL_KEY_COUNT
};

enum {
    CTX_LATE_REQUEST = 0,
    CTX_LATE_CHANGE,
    CTX_WET,
    CTX_POSITIONING,
    CTX_KEY_COUNT
};

typedef struct {
    int seen;
    int value;
} pf_seen_int_t;

typedef struct {
    pf_seen_int_t gen[GEN_KEY_COUNT];
    pf_seen_int_t prob[PROB_KEY_COUNT];
    pf_seen_int_t dur[DUR_KEY_COUNT];
    pf_seen_int_t corner[CORNER_KEY_COUNT];
    pf_seen_int_t fuel[FUEL_KEY_COUNT];
    pf_seen_int_t ctx[CTX_KEY_COUNT];
} pf_seen_t;

static const char *const PF_SECTION_NAMES[SEC_COUNT] = {
    "general", "probability", "duration", "corner", "fuel_subtypes", "context"
};

static const char *const PF_GEN_KEYS[GEN_KEY_COUNT] = {
    "crew_risk_percent", "schema_version"
};

static const char *const PF_PROB_KEYS[PROB_KEY_COUNT] = {
    "slow_corner_bp", "wheelgun_bp", "fuel_rig_bp", "stall_delay_bp",
    "cross_thread_bp", "terminal_wheel_bp", "recoverable_cap_bp"
};

static const char *const PF_DUR_KEYS[DUR_KEY_COUNT] = {
    "slow_corner_min", "slow_corner_mode", "slow_corner_max",
    "wheelgun_min", "wheelgun_mode", "wheelgun_max",
    "fuel_slow_flow_min", "fuel_slow_flow_mode", "fuel_slow_flow_max",
    "fuel_temporary_no_flow_min", "fuel_temporary_no_flow_mode",
    "fuel_temporary_no_flow_max", "fuel_interrupted_flow_min",
    "fuel_interrupted_flow_mode", "fuel_interrupted_flow_max",
    "stall_delay_min", "stall_delay_mode", "stall_delay_max",
    "cross_thread_min", "cross_thread_mode", "cross_thread_max",
    "terminal_diagnosis_min", "terminal_diagnosis_mode", "terminal_diagnosis_max"
};

static const char *const PF_CORNER_KEYS[CORNER_KEY_COUNT] = {
    "fl_weight", "fr_weight", "rl_weight", "rr_weight"
};

static const char *const PF_FUEL_KEYS[FUEL_KEY_COUNT] = {
    "slow_flow_weight", "temporary_no_flow_weight", "interrupted_flow_weight"
};

static const char *const PF_CTX_KEYS[CTX_KEY_COUNT] = {
    "late_request_bonus", "late_change_bonus", "wet_bonus", "positioning_bonus"
};

static const char *section_name_or_unknown(int section)
{
    if (section < 0 || section >= SEC_COUNT) {
        return "unknown-section";
    }
    return PF_SECTION_NAMES[section];
}

static int section_key_count(int section);

static pf_seen_int_t *find_slot(pf_seen_t *seen, int section, int index)
{
    if (section < 0 || section >= SEC_COUNT || index < 0 ||
        index >= section_key_count(section)) {
        return NULL;
    }
    switch (section) {
    case SEC_GENERAL: return &seen->gen[index];
    case SEC_PROBABILITY: return &seen->prob[index];
    case SEC_DURATION: return &seen->dur[index];
    case SEC_CORNER: return &seen->corner[index];
    case SEC_FUEL: return &seen->fuel[index];
    case SEC_CONTEXT: return &seen->ctx[index];
    default: return NULL;
    }
}

static int section_key_count(int section)
{
    switch (section) {
    case SEC_GENERAL: return GEN_KEY_COUNT;
    case SEC_PROBABILITY: return PROB_KEY_COUNT;
    case SEC_DURATION: return DUR_KEY_COUNT;
    case SEC_CORNER: return CORNER_KEY_COUNT;
    case SEC_FUEL: return FUEL_KEY_COUNT;
    case SEC_CONTEXT: return CTX_KEY_COUNT;
    default: return 0;
    }
}

static const char *section_key_name(int section, int index)
{
    if (section < 0 || section >= SEC_COUNT || index < 0 ||
        index >= section_key_count(section)) {
        return "?";
    }
    switch (section) {
    case SEC_GENERAL: return PF_GEN_KEYS[index];
    case SEC_PROBABILITY: return PF_PROB_KEYS[index];
    case SEC_DURATION: return PF_DUR_KEYS[index];
    case SEC_CORNER: return PF_CORNER_KEYS[index];
    case SEC_FUEL: return PF_FUEL_KEYS[index];
    case SEC_CONTEXT: return PF_CTX_KEYS[index];
    default: return "?";
    }
}

static pf_config_status_t parse_fail(char *err, size_t err_len, int line,
                                     const char *section, const char *key,
                                     const char *problem)
{
    if (err && err_len) {
        if (section && key) {
            snprintf(err, err_len, "line %d: [%s] %s: %s", line, section, key, problem);
        } else if (section) {
            snprintf(err, err_len, "line %d: [%s]: %s", line, section, problem);
        } else {
            snprintf(err, err_len, "line %d: %s", line, problem);
        }
    }
    return PF_CONFIG_SYNTAX;
}

/* Digits only, no sign, no whitespace, full consumption, full uint64 range.
 * Exposed for the CLI and replay tools so all numeric input shares one
 * parser. */
int pf_parse_u64_strict(const char *text, uint64_t *out)
{
    const char *p = text;
    uint64_t value = 0;

    if (!p || !*p) {
        return 0;
    }
    while (*p) {
        unsigned digit;
        if (*p < '0' || *p > '9') {
            return 0;
        }
        digit = (unsigned)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        p++;
    }
    *out = value;
    return 1;
}

int pf_parse_double_strict(const char *text, double *out)
{
    const char *p;
    char cleaned[64];
    char *end = NULL;
    size_t i = 0;
    double value;

    if (!text || !*text) {
        return 0;
    }
    for (p = text; *p; p++) {
        if (*p == '_') {
            continue;
        }
        if (*p == '-' || *p == '+') {
            return 0; /* signs are not part of the accepted grammar */
        }
        if (*p == ' ' || *p == '\t') {
            return 0; /* no embedded or surrounding whitespace */
        }
        if (i + 1 >= sizeof(cleaned)) {
            return 0;
        }
        cleaned[i++] = *p;
    }
    cleaned[i] = '\0';
    if (i == 0) {
        return 0;
    }
    /* strtod accepts inf/nan; the grammar does not. */
    if (strstr(cleaned, "nan") || strstr(cleaned, "Nan") || strstr(cleaned, "NAN") ||
        strstr(cleaned, "inf") || strstr(cleaned, "Inf") || strstr(cleaned, "INF")) {
        return 0;
    }
    value = strtod(cleaned, &end);
    if (end == cleaned || !end || *end != '\0') {
        return 0;
    }
    if (!isfinite(value)) {
        return 0;
    }
    *out = value;
    return 1;
}

static int parse_long_strict(const char *text, long *out)
{
    const char *p = text;
    int negative = 0;
    long value = 0;

    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }
    while (*p >= '0' && *p <= '9') {
        int digit = *p - '0';
        if (value > (LONG_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        p++;
    }
    if (*p != '\0') {
        return 0;
    }
    *out = negative ? -value : value;
    return 1;
}

static pf_config_status_t assign_value(int section, int index, long ivalue,
                                       double dvalue, pf_config_t *cfg,
                                       int *schema_version,
                                       pf_seen_t *seen, int line,
                                       char *err, size_t err_len)
{
    pf_seen_int_t *slot = find_slot(seen, section, index);
    const char *key = section_key_name(section, index);

    if (!slot) {
        return parse_fail(err, err_len, line, section_name_or_unknown(section), key,
                          "internal schema error");
    }
    if (slot->seen) {
        if (err && err_len) {
            snprintf(err, err_len, "line %d: [%s] %s: duplicate key",
                     line, section_name_or_unknown(section), key);
        }
        return PF_CONFIG_DUPLICATE;
    }
    slot->seen = 1;

    if (section == SEC_GENERAL) {
        if (index == GEN_CREW_RISK) {
            cfg->crew_risk_percent = (int)ivalue;
        } else {
            *schema_version = (int)ivalue;
        }
    } else if (section == SEC_PROBABILITY) {
        switch (index) {
        case PROB_SLOW_CORNER: cfg->weights.slow_corner_bp = (int)ivalue; break;
        case PROB_WHEELGUN: cfg->weights.wheelgun_bp = (int)ivalue; break;
        case PROB_FUEL_RIG: cfg->weights.fuel_rig_bp = (int)ivalue; break;
        case PROB_STALL_DELAY: cfg->weights.stall_delay_bp = (int)ivalue; break;
        case PROB_CROSS_THREAD: cfg->weights.cross_thread_bp = (int)ivalue; break;
        case PROB_TERMINAL_WHEEL: cfg->weights.terminal_wheel_bp = (int)ivalue; break;
        default: cfg->recoverable_cap_bp = (int)ivalue; break;
        }
    } else if (section == SEC_DURATION) {
        pf_duration_band_t *bands[8] = {
            &cfg->durations.slow_corner,
            &cfg->durations.wheelgun,
            &cfg->durations.fuel_slow_flow,
            &cfg->durations.fuel_temporary_no_flow,
            &cfg->durations.fuel_interrupted_flow,
            &cfg->durations.stall_delay,
            &cfg->durations.cross_thread,
            &cfg->durations.terminal_diagnosis
        };
        int band_index = index / 3;
        int part = index % 3;
        double *target = part == 0 ? &bands[band_index]->min
                         : part == 1 ? &bands[band_index]->mode
                                     : &bands[band_index]->max;
        *target = dvalue;
    } else if (section == SEC_CORNER) {
        cfg->corner_weight[index] = (int)ivalue;
    } else if (section == SEC_FUEL) {
        cfg->fuel_weight[index + 1] = (int)ivalue;
    } else if (section == SEC_CONTEXT) {
        switch (index) {
        case CTX_LATE_REQUEST: cfg->late_request_bonus = (int)ivalue; break;
        case CTX_LATE_CHANGE: cfg->late_change_bonus = (int)ivalue; break;
        case CTX_WET: cfg->wet_bonus = (int)ivalue; break;
        default: cfg->positioning_bonus = (int)ivalue; break;
        }
    }
    return PF_CONFIG_OK;
}

pf_config_status_t pf_config_parse(const char *text, pf_config_t *out,
                                   char *err, size_t err_len)
{
    pf_config_t temp;
    pf_seen_t seen;
    int dup_section[SEC_COUNT];
    int schema_version = -1;
    int section = -1;
    int line = 0;
    const char *cursor;
    pf_config_status_t status = PF_CONFIG_OK;

    if (err && err_len) {
        err[0] = '\0';
    }
    if (!text || !out) {
        return PF_CONFIG_SYNTAX;
    }
    pf_config_defaults(&temp);
    memset(&seen, 0, sizeof(seen));
    memset(dup_section, 0, sizeof(dup_section));

    cursor = text;
    for (;;) {
        const char *line_start = cursor;
        const char *line_end;
        size_t len;
        char buffer[512];
        size_t i;
        int key_index = -1;
        int is_double = 0;
        long ivalue = 0;
        double dvalue = 0.0;

        while (*cursor && *cursor != '\n' && *cursor != '\r') {
            cursor++;
        }
        line_end = cursor;
        while (*cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
        line++;
        len = (size_t)(line_end - line_start);
        if (len >= sizeof(buffer)) {
            if (err && err_len) {
                snprintf(err, err_len, "line %d: line too long", line);
            }
            return PF_CONFIG_SYNTAX;
        }
        memcpy(buffer, line_start, len);
        buffer[len] = '\0';

        /* Trim. */
        i = 0;
        while (buffer[i] == ' ' || buffer[i] == '\t') {
            i++;
        }
        {
            size_t j = len;
            while (j > i && (buffer[j - 1] == ' ' || buffer[j - 1] == '\t')) {
                j--;
            }
            buffer[j] = '\0';
        }
        if (buffer[i] == '\0' || buffer[i] == ';' || buffer[i] == '#') {
            if (*cursor == '\0' && line_end == cursor) {
                break;
            }
            continue;
        }
        if (buffer[i] == '[') {
            size_t j = i + 1;
            int found = -1;
            int s;
            while (buffer[j] && buffer[j] != ']') {
                j++;
            }
            if (buffer[i + 1] == '\0' || buffer[j] != ']' || buffer[j + 1] != '\0') {
                return parse_fail(err, err_len, line, NULL, NULL,
                                  "malformed section header");
            }
            buffer[j] = '\0';
            for (s = 0; s < SEC_COUNT; s++) {
                if (strcmp(&buffer[i + 1], PF_SECTION_NAMES[s]) == 0) {
                    found = s;
                    break;
                }
            }
            if (found < 0) {
                return parse_fail(err, err_len, line, NULL, &buffer[i + 1],
                                  "unknown section");
            }
            if (dup_section[found]) {
                return parse_fail(err, err_len, line, NULL, &buffer[i + 1],
                                  "duplicate section");
            }
            dup_section[found] = 1;
            section = found;
            if (*cursor == '\0') {
                break;
            }
            continue;
        }
        if (buffer[i] == '=') {
            return parse_fail(err, err_len, line, NULL, NULL, "missing key");
        }
        if (section < 0) {
            return parse_fail(err, err_len, line, NULL, NULL,
                              "key outside any section");
        }
        {
            char *equals = strchr(&buffer[i], '=');
            char *key_start = &buffer[i];
            char *key_end;
            char *value_start;
            char *value_end;
            int k;
            if (!equals) {
                return parse_fail(err, err_len, line, section_name_or_unknown(section),
                                  NULL, "missing '='");
            }
            key_end = equals;
            while (key_end > key_start &&
                   (key_end[-1] == ' ' || key_end[-1] == '\t')) {
                key_end--;
            }
            *key_end = '\0';
            value_start = equals + 1;
            while (*value_start == ' ' || *value_start == '\t') {
                value_start++;
            }
            value_end = value_start + strlen(value_start);
            while (value_end > value_start &&
                   (value_end[-1] == ' ' || value_end[-1] == '\t')) {
                value_end--;
            }
            *value_end = '\0';
            if (*key_start == '\0') {
                return parse_fail(err, err_len, line, section_name_or_unknown(section),
                                  NULL, "empty key");
            }
            for (k = 0; k < section_key_count(section); k++) {
                if (strcmp(key_start, section_key_name(section, k)) == 0) {
                    key_index = k;
                    break;
                }
            }
            if (key_index < 0) {
                return parse_fail(err, err_len, line, section_name_or_unknown(section),
                                  key_start, "unknown key");
            }
            if (*value_start == '\0') {
                return parse_fail(err, err_len, line, section_name_or_unknown(section),
                                  key_start, "missing value");
            }
            is_double = section == SEC_DURATION;
            if (is_double) {
                if (!pf_parse_double_strict(value_start, &dvalue)) {
                    return parse_fail(err, err_len, line,
                                      section_name_or_unknown(section), key_start,
                                      "invalid number");
                }
            } else {
                if (!parse_long_strict(value_start, &ivalue)) {
                    return parse_fail(err, err_len, line,
                                      section_name_or_unknown(section), key_start,
                                      "invalid integer");
                }
            }
            status = assign_value(section, key_index, ivalue, dvalue,
                                  &temp, &schema_version, &seen, line, err, err_len);
            if (status != PF_CONFIG_OK) {
                return status;
            }
        }
        if (*cursor == '\0') {
            break;
        }
    }

    if (schema_version != PF_SCHEMA_VERSION) {
        if (err && err_len) {
            snprintf(err, err_len,
                     "missing or invalid schema marker: expected [general] schema_version = %d",
                     PF_SCHEMA_VERSION);
        }
        return PF_CONFIG_SCHEMA;
    }
    {
        int s;
        for (s = 0; s < SEC_COUNT; s++) {
            int k;
            for (k = 0; k < section_key_count(s); k++) {
                pf_seen_int_t *slot = find_slot(&seen, s, k);
                if (slot && !slot->seen) {
                    if (err && err_len) {
                        snprintf(err, err_len, "[%s] missing required key: %s",
                                 PF_SECTION_NAMES[s], section_key_name(s, k));
                    }
                    return PF_CONFIG_SCHEMA;
                }
            }
        }
    }
    status = pf_config_validate(&temp, err, err_len);
    if (status != PF_CONFIG_OK) {
        return status;
    }
    *out = temp;
    return PF_CONFIG_OK;
}

pf_config_status_t pf_config_parse_file(const char *path, pf_config_t *out,
                                        char *err, size_t err_len)
{
    FILE *file;
    char *buffer = NULL;
    long size = 0;
    size_t read_bytes = 0;
    pf_config_status_t status;

    if (err && err_len) {
        err[0] = '\0';
    }
    if (!path || !out) {
        return PF_CONFIG_IO;
    }
    file = fopen(path, "rb");
    if (!file) {
        if (err && err_len) {
            snprintf(err, err_len, "cannot open configuration file: %s", path);
        }
        return PF_CONFIG_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return PF_CONFIG_IO;
    }
    if (size > 1024L * 1024L) {
        fclose(file);
        if (err && err_len) {
            snprintf(err, err_len, "configuration file too large: %s", path);
        }
        return PF_CONFIG_IO;
    }
    buffer = (char *)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return PF_CONFIG_IO;
    }
    read_bytes = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[read_bytes] = '\0';
    status = pf_config_parse(buffer, out, err, err_len);
    free(buffer);
    return status;
}

const char *pf_config_status_name(pf_config_status_t status)
{
    switch (status) {
    case PF_CONFIG_OK: return "ok";
    case PF_CONFIG_IO: return "io-error";
    case PF_CONFIG_SYNTAX: return "syntax-error";
    case PF_CONFIG_SCHEMA: return "schema-error";
    case PF_CONFIG_DUPLICATE: return "duplicate-key";
    case PF_CONFIG_UNKNOWN: return "unknown-key";
    case PF_CONFIG_NUMBER: return "number-error";
    case PF_CONFIG_RANGE: return "range-error";
    default: return "unknown";
    }
}

static size_t append_text(char *buf, size_t buf_len, size_t offset,
                          const char *text)
{
    size_t len;
    if (!buf || offset >= buf_len) {
        return offset;
    }
    len = strlen(text);
    if (offset + len >= buf_len) {
        len = buf_len - offset - 1;
    }
    memcpy(buf + offset, text, len);
    offset += len;
    buf[offset] = '\0';
    return offset;
}

static size_t append_int(char *buf, size_t buf_len, size_t offset,
                         const char *label, int value)
{
    char line[128];
    snprintf(line, sizeof(line), "%s%d\n", label, value);
    return append_text(buf, buf_len, offset, line);
}

static size_t append_double(char *buf, size_t buf_len, size_t offset,
                            const char *label, double value)
{
    char line[128];
    snprintf(line, sizeof(line), "%s%.6f\n", label, value);
    return append_text(buf, buf_len, offset, line);
}

size_t pf_config_render(const pf_config_t *cfg, char *buf, size_t buf_len)
{
    size_t offset = 0;
    int i;

    if (!cfg) {
        return 0;
    }
    if (buf && buf_len) {
        buf[0] = '\0';
    }
    offset = append_text(buf, buf_len, offset, "[general]\n");
    offset = append_int(buf, buf_len, offset, "schema_version = ", PF_SCHEMA_VERSION);
    offset = append_int(buf, buf_len, offset, "crew_risk_percent = ",
                        cfg->crew_risk_percent);
    offset = append_text(buf, buf_len, offset, "[probability]\n");
    offset = append_int(buf, buf_len, offset, "slow_corner_bp = ",
                        cfg->weights.slow_corner_bp);
    offset = append_int(buf, buf_len, offset, "wheelgun_bp = ",
                        cfg->weights.wheelgun_bp);
    offset = append_int(buf, buf_len, offset, "fuel_rig_bp = ",
                        cfg->weights.fuel_rig_bp);
    offset = append_int(buf, buf_len, offset, "stall_delay_bp = ",
                        cfg->weights.stall_delay_bp);
    offset = append_int(buf, buf_len, offset, "cross_thread_bp = ",
                        cfg->weights.cross_thread_bp);
    offset = append_int(buf, buf_len, offset, "terminal_wheel_bp = ",
                        cfg->weights.terminal_wheel_bp);
    offset = append_int(buf, buf_len, offset, "recoverable_cap_bp = ",
                        cfg->recoverable_cap_bp);
    offset = append_text(buf, buf_len, offset, "[duration]\n");
    {
        static const char *const names[8][3] = {
            { "slow_corner_min", "slow_corner_mode", "slow_corner_max" },
            { "wheelgun_min", "wheelgun_mode", "wheelgun_max" },
            { "fuel_slow_flow_min", "fuel_slow_flow_mode", "fuel_slow_flow_max" },
            { "fuel_temporary_no_flow_min", "fuel_temporary_no_flow_mode",
              "fuel_temporary_no_flow_max" },
            { "fuel_interrupted_flow_min", "fuel_interrupted_flow_mode",
              "fuel_interrupted_flow_max" },
            { "stall_delay_min", "stall_delay_mode", "stall_delay_max" },
            { "cross_thread_min", "cross_thread_mode", "cross_thread_max" },
            { "terminal_diagnosis_min", "terminal_diagnosis_mode",
              "terminal_diagnosis_max" }
        };
        const pf_duration_band_t *bands[8] = {
            &cfg->durations.slow_corner,
            &cfg->durations.wheelgun,
            &cfg->durations.fuel_slow_flow,
            &cfg->durations.fuel_temporary_no_flow,
            &cfg->durations.fuel_interrupted_flow,
            &cfg->durations.stall_delay,
            &cfg->durations.cross_thread,
            &cfg->durations.terminal_diagnosis
        };
        char label[64];
        for (i = 0; i < 8; i++) {
            const double values[3] = { bands[i]->min, bands[i]->mode, bands[i]->max };
            int part;
            for (part = 0; part < 3; part++) {
                snprintf(label, sizeof(label), "%s = ", names[i][part]);
                offset = append_double(buf, buf_len, offset, label, values[part]);
            }
        }
    }
    offset = append_text(buf, buf_len, offset, "[corner]\n");
    {
        static const char *const names[CORNER_KEY_COUNT] = {
            "fl_weight = ", "fr_weight = ", "rl_weight = ", "rr_weight = "
        };
        for (i = 0; i < CORNER_KEY_COUNT; i++) {
            offset = append_int(buf, buf_len, offset, names[i], cfg->corner_weight[i]);
        }
    }
    offset = append_text(buf, buf_len, offset, "[fuel_subtypes]\n");
    {
        static const char *const names[FUEL_KEY_COUNT] = {
            "slow_flow_weight = ", "temporary_no_flow_weight = ",
            "interrupted_flow_weight = "
        };
        for (i = 0; i < FUEL_KEY_COUNT; i++) {
            offset = append_int(buf, buf_len, offset, names[i], cfg->fuel_weight[i + 1]);
        }
    }
    offset = append_text(buf, buf_len, offset, "[context]\n");
    offset = append_int(buf, buf_len, offset, "late_request_bonus = ",
                        cfg->late_request_bonus);
    offset = append_int(buf, buf_len, offset, "late_change_bonus = ",
                        cfg->late_change_bonus);
    offset = append_int(buf, buf_len, offset, "wet_bonus = ", cfg->wet_bonus);
    offset = append_int(buf, buf_len, offset, "positioning_bonus = ",
                        cfg->positioning_bonus);
    return offset;
}
