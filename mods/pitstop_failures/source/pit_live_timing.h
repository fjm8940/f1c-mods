#pragma once
#include <math.h>
#include "pit_failure_policy.h"
/* Native timer reconstruction, read-only. See LIVE_TRIAL.md/source evidence. */
typedef enum {
    PIT_LIVE_SERVICE_OK = 0,
    /* A penalty/drive-through or another no-work visit has no safe failure
     * target. It must not consume a natural incident draw. */
    PIT_LIVE_SERVICE_SKIP_NO_RECOGNISED_WORK = 1,
    /* A wing/body repair is native work, but there is no repair-specific
     * failure family. It must not become a stall or a wheel/fuel fault. */
    PIT_LIVE_SERVICE_SKIP_REPAIR_ONLY = 2,
    PIT_LIVE_SERVICE_INVALID = 3
} pit_live_service_result_t;

static int pit_live_jobs(double start, double baseline, int tyres, double tyre_end,
    int fuel, double fuel_start, double quantity, int wing, int repair,
    pf_job_t *jobs, size_t *count, pf_eligibility_t *eligibility,
    pit_live_service_result_t *result)
{
    double maximum = 0;
    size_t n = 0, i;
    int other = wing || repair;
    if (result) *result = PIT_LIVE_SERVICE_INVALID;
    if (!isfinite(start) || !isfinite(baseline) || baseline <= start || baseline-start > 120) return 0;
    if (!tyres && !fuel && !other) {
        if (result) *result = PIT_LIVE_SERVICE_SKIP_NO_RECOGNISED_WORK;
        return 0;
    }
    if (!tyres && !fuel && other) {
        if (result) *result = PIT_LIVE_SERVICE_SKIP_REPAIR_ONLY;
        return 0;
    }
    *eligibility = (pf_eligibility_t){0,0,0,0,1};
    if (tyres) {
        if (!isfinite(tyre_end) || tyre_end < start || tyre_end+1 > baseline+0.15) return 0;
        jobs[n++] = (pf_job_t){PF_JOB_TYRE, tyre_end+1-start};
        eligibility->tyre_work = 1;
    }
    if (fuel) {
        double end;
        if (!isfinite(quantity) || quantity <= 0 || quantity > 300 || !isfinite(fuel_start) || fuel_start < start) return 0;
        end = fuel_start + quantity/12.0 + 0.5;
        if (end > baseline+0.15) return 0;
        jobs[n++] = (pf_job_t){PF_JOB_FUEL, end-start};
        eligibility->fuel_allowed = 1;
        eligibility->fuel_add_liters = (int)ceil(quantity);
    }
    if (other) jobs[n++] = (pf_job_t){PF_JOB_OTHER, baseline-start};
    if (!n) return 0;
    for (i=0; i<n; i++) if (jobs[i].end_seconds > maximum) maximum=jobs[i].end_seconds;
    if (fabs(maximum-(baseline-start)) > 0.15) return 0;
    /* Preserve native baseline even across single precision rounding. */
    jobs[n++] = (pf_job_t){PF_JOB_OTHER, baseline-start};
    eligibility->real_service_possible = 1;
    *count = n;
    if (result) *result = PIT_LIVE_SERVICE_OK;
    return 1;
}

static const char *pit_live_service_result_name(pit_live_service_result_t result)
{
    switch (result) {
    case PIT_LIVE_SERVICE_SKIP_NO_RECOGNISED_WORK: return "no recognised work";
    case PIT_LIVE_SERVICE_SKIP_REPAIR_ONLY: return "repair-only";
    case PIT_LIVE_SERVICE_INVALID: return "invalid native timing";
    default: return "eligible service";
    }
}

static double pit_live_hold_value(double now, double baseline, double target, double current)
{
    double next;
    if (!isfinite(now) || !isfinite(baseline) || !isfinite(target) || !isfinite(current) || target < baseline) return current;
    next = fmin(target, now+0.5);
    if (next < baseline) next=baseline;
    return next > current ? next : current;
}
