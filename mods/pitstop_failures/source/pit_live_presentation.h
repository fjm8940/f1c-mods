#pragma once
#include <stdint.h>
#include <math.h>
#include "pit_failure_policy.h"

/* Timings measured and used by the original pit-stop failure mod.  The tyre
 * operation is armed before the visible crew action, so all wheel audio and
 * wheel-fault presentation is anchored to the native service start instead
 * of to policy commit/pit-box entry. */
static const double PIT_GUN_OFF_SECONDS[4] = {7.3, 7.5, 6.0, 6.0};
static const double PIT_GUN_ON_SECONDS[4]  = {10.8, 11.0, 9.5, 9.5};

static inline uint64_t pit_presentation_mix(uint64_t x)
{
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static inline double pit_presentation_jitter(uint64_t seed, uint64_t vehicle,
                                              uint64_t token, uint64_t domain,
                                              double span)
{
    uint64_t x = pit_presentation_mix(seed ^ pit_presentation_mix(vehicle) ^
                                      pit_presentation_mix(token) ^ domain);
    return ((double)(x % UINT64_C(1000000)) / 1000000.0) * span;
}

static inline int pit_presentation_is_wheel_failure(const pf_plan_t *plan)
{
    return plan && plan->wheel >= 0 && plan->wheel < 4 &&
           (plan->outcome == PF_OUTCOME_SLOW_CORNER ||
            plan->outcome == PF_OUTCOME_WHEELGUN ||
            plan->outcome == PF_OUTCOME_CROSS_THREAD);
}

static inline void pit_presentation_wheel_audio(double service_start,
                                                 double tyre_end,
                                                 uint64_t seed,
                                                 uint64_t vehicle,
                                                 uint64_t token,
                                                 double off_at[4],
                                                 double on_at[4])
{
    /* Retain the old script's corner order and 7.3/7.5/6.0/6.0 then
     * 10.8/11.0/9.5/9.5 pattern. Its absolute offsets assumed a long tyre
     * animation; fit the pattern into this stop's native tyre job so both
     * bursts finish before the crew completes on a short stop. */
    double duration=tyre_end-service_start;
    double scale=isfinite(duration) && duration>=3.0 && duration<=60.0 ?
                 (duration-1.0)/11.3 : 1.0;
    int corner;
    for (corner = 0; corner < 4; ++corner) {
        double jitter = pit_presentation_jitter(seed, vehicle, token,
                                                UINT64_C(0x574845454c000000) +
                                                (uint64_t)corner, 0.30);
        off_at[corner] = service_start + (PIT_GUN_OFF_SECONDS[corner] + jitter)*scale;
        on_at[corner] = service_start + (PIT_GUN_ON_SECONDS[corner] + jitter)*scale;
    }
}

static inline double pit_presentation_failure_time(const pf_plan_t *plan,
                                                    double service_start,
                                                    double native_end,
                                                    double fuel_start,
                                                    uint64_t seed,
                                                    uint64_t vehicle,
                                                    uint64_t token)
{
    double at;
    double jitter = pit_presentation_jitter(seed, vehicle, token,
                                            UINT64_C(0x4641494c55524500), 0.70);
    if (pit_presentation_is_wheel_failure(plan))
        at = service_start + PIT_GUN_OFF_SECONDS[plan->wheel] + jitter * (0.30 / 0.70);
    else if (plan && plan->outcome == PF_OUTCOME_FUEL_RIG &&
             isfinite(fuel_start) && fuel_start >= service_start)
        at = fuel_start + 0.75 + jitter;
    else if (plan && plan->outcome == PF_OUTCOME_STALL_DELAY)
        at = native_end - 0.35 - jitter;
    else
        at = service_start + (native_end - service_start) * 0.55 + jitter * 0.25;

    if (at < service_start + 0.50) at = service_start + 0.50;
    if (at > native_end - 0.10 && native_end > service_start + 0.60)
        at = native_end - 0.10;
    return at;
}

static inline double pit_presentation_command_time(const pf_plan_t *plan,
                                                    double reveal_at,
                                                    double target,
                                                    int kind,
                                                    int attempt)
{
    if (kind == PF_CMD_FAULT_NOTICE) return reveal_at;
    if (kind == PF_CMD_RECOVERED) {
        /* The game's box-release edge may arrive on the target tick. Give
         * wheel-clear narration room to queue while service is still active. */
        if (pit_presentation_is_wheel_failure(plan) && target>reveal_at+0.60)
            return fmax(reveal_at+0.35,target-1.0);
        return target > reveal_at ? target : reveal_at;
    }
    if (kind == PF_CMD_RECOVERY_ATTEMPT) {
        int attempts = plan && plan->attempts > 0 ? plan->attempts : 1;
        double fraction = (double)(attempt + 1) / (double)(attempts + 1);
        if (fraction < 0.20) fraction = 0.20;
        if (fraction > 0.85) fraction = 0.85;
        return reveal_at + (target > reveal_at ? target - reveal_at : 0.0) * fraction;
    }
    return reveal_at;
}
