/*
 * pit_failure_policy.h — pure pit-stop failure policy for pitstop-modding.
 *
 * Version 0.2.0-policy.3 (Development build).
 *
 * Units contract (never mixed):
 *   - basis points (bp): 10000 bp = 100%, 1 bp = 0.01% (all *_bp fields)
 *   - seconds (double):  all duration fields
 *   - relative weights:  corner/fuel-subtype integers, not probabilities
 *
 * This module has no game, OS, Windows, thread or I/O dependencies. It is
 * compiled into the offline simulator, its tests, and (later, under separate
 * review) a runtime adapter. The legacy runtime in src/pit_commentary.c is not
 * connected to this code.
 *
 * Public API layout note: the policy and the session lifecycle are declared in
 * this single header. There is no separate session header.
 */
#ifndef PIT_FAILURE_POLICY_H
#define PIT_FAILURE_POLICY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- limits ------------------------------------------------------------- */

#define PF_SCHEMA_VERSION              2
#define PF_BP_BASE_SUM_MAX             1200
#define PF_BP_TERMINAL_MAX             10
#define PF_BP_RECOVERABLE_CAP_MIN      1
#define PF_BP_RECOVERABLE_CAP_MAX      1200
#define PF_CREW_RISK_MIN               80
#define PF_CREW_RISK_MAX               120
#define PF_CONTEXT_BONUS_MAX           100
#define PF_MULTIPLIER_MIN              60
#define PF_MULTIPLIER_MAX              240
#define PF_SECONDS_MIN                 0.0
#define PF_SECONDS_MAX                 60.0
#define PF_BP_UNIVERSE                 10000

/* Draw range used for one category selection. */
#define PF_DRAW_MAX                    10000

/* Bounded recovery attempts per recoverable outcome. */
#define PF_ATTEMPTS_MAX                3

/* Number of rule-defined categories. */
#define PF_RECOVERABLE_COUNT           5
#define PF_CATEGORY_COUNT              6

/* Bounded session storage (R1/R2/R9). The defaults below are the shipped
 * limits; a build may override them (e.g. the bounded-limit acceptance binary)
 * by defining the macros before including this header. */
#ifndef PF_ACTIVE_MAX
#define PF_ACTIVE_MAX                  4096
#endif
#ifndef PF_HISTORY_MAX
#define PF_HISTORY_MAX                 2097152
#endif
#ifndef PF_QUEUE_MAX
#define PF_QUEUE_MAX                   65536
#endif
#define PF_SLOTS_ALL                   (-1)
#define PF_COMMANDS_MAX                96

/* --- enums -------------------------------------------------------------- */

typedef enum {
    PF_WHEEL_FL = 0,
    PF_WHEEL_FR = 1,
    PF_WHEEL_RL = 2,
    PF_WHEEL_RR = 3,
    PF_WHEEL_COUNT = 4
} pf_wheel_t;

typedef enum {
    PF_OUTCOME_CLEAN = 0,
    PF_OUTCOME_SLOW_CORNER = 1,
    PF_OUTCOME_WHEELGUN = 2,
    PF_OUTCOME_FUEL_RIG = 3,
    PF_OUTCOME_STALL_DELAY = 4,
    PF_OUTCOME_CROSS_THREAD = 5,
    PF_OUTCOME_TERMINAL_WHEEL = 6
} pf_outcome_t;

typedef enum {
    PF_CLEAN = 0,
    PF_RECOVERABLE = 1,
    PF_TERMINAL = 2
} pf_severity_t;

typedef enum {
    PF_FUEL_SUBTYPE_NONE = 0,
    PF_FUEL_SUBTYPE_SLOW_FLOW = 1,
    PF_FUEL_SUBTYPE_TEMPORARY_NO_FLOW = 2,
    PF_FUEL_SUBTYPE_INTERRUPTED_FLOW = 3,
    PF_FUEL_SUBTYPE_SPILL = 4
} pf_fuel_subtype_t;

/* Job baselines supplied by the caller. PF_JOB_UNKNOWN mixes with others. */
typedef enum {
    PF_JOB_UNKNOWN = 0,
    PF_JOB_TYRE = 1,
    PF_JOB_FUEL = 2,
    PF_JOB_OTHER = 3,
    PF_JOB_STALL = 4
} pf_job_kind_t;

typedef enum {
    PF_SESSION_IDLE = 0,
    PF_SESSION_SERVICE_PLANNED = 1,
    PF_SESSION_RESOLVING = 2,
    PF_SESSION_RESOLVED = 3,
    PF_SESSION_CANCELLED = 4,
    PF_SESSION_TERMINAL_REQUESTED = 5
} pf_session_state_t;

typedef enum {
    PF_SESSION_OK = 0,
    PF_SESSION_ERR_INVALID_STATE = 1,  /* not initialised, or requires reset */
    PF_SESSION_ERR_INVALID_TIME = 2,   /* NaN/Inf/backwards; session invalid */
    PF_SESSION_ERR_CAPACITY = 3,       /* bounded storage exhausted */
    PF_SESSION_ERR_REFUSED = 4,        /* unknown identity / epoch mismatch */
    PF_SESSION_ERR_TOKEN_CANCELLED = 5,
    PF_SESSION_ERR_DUPLICATE = 6
} pf_session_error_t;

typedef enum {
    PF_CMD_NONE = 0,
    PF_CMD_FAULT_NOTICE = 1,   /* problem observed; presentation/result cue */
    PF_CMD_RECOVERY_ATTEMPT = 2,
    PF_CMD_RECOVERED = 3,      /* final attempt; service resumes */
    PF_CMD_TERMINAL_REQUEST = 4
} pf_command_kind_t;

typedef enum {
    PF_CONFIG_OK = 0,
    PF_CONFIG_IO = 1,
    PF_CONFIG_SYNTAX = 2,
    PF_CONFIG_SCHEMA = 3,
    PF_CONFIG_DUPLICATE = 4,
    PF_CONFIG_UNKNOWN = 5,
    PF_CONFIG_NUMBER = 6,
    PF_CONFIG_RANGE = 7
} pf_config_status_t;

/* --- configuration ------------------------------------------------------ */

typedef struct {
    double min;
    double mode;
    double max;
} pf_duration_band_t;

typedef struct {
    int slow_corner_bp;
    int wheelgun_bp;
    int fuel_rig_bp;
    int stall_delay_bp;
    int cross_thread_bp;
    int terminal_wheel_bp;
} pf_bp_weights_t;

typedef struct {
    pf_duration_band_t slow_corner;
    pf_duration_band_t wheelgun;
    pf_duration_band_t fuel_slow_flow;
    pf_duration_band_t fuel_temporary_no_flow;
    pf_duration_band_t fuel_interrupted_flow;
    pf_duration_band_t stall_delay;
    pf_duration_band_t cross_thread;
    pf_duration_band_t terminal_diagnosis;
} pf_duration_set_t;

typedef struct {
    int crew_risk_percent;         /* 80..120; neutral 100 */
    pf_bp_weights_t weights;
    int recoverable_cap_bp;        /* 1..1200 */
    pf_duration_set_t durations;
    int corner_weight[PF_WHEEL_COUNT];
    int fuel_weight[4];            /* [0]=unused; subtypes 1..3 */
    int late_request_bonus;        /* 0..100 */
    int late_change_bonus;         /* 0..100 */
    int wet_bonus;                 /* 0..100 */
    int positioning_bonus;         /* 0..100 */
} pf_config_t;

/* --- committed service snapshot ----------------------------------------- */

typedef struct {
    uint64_t epoch;
    uint64_t vehicle_id;
    uint64_t service_token;
} pf_identity_t;

typedef struct {
    int tyre_work;            /* 1 when a tyre service job exists */
    int fuel_allowed;         /* refuelling permitted for this stop */
    int fuel_add_liters;      /* requested fuel addition, liters (> 0) */
    int real_service_possible;/* the crew performs work at all */
    int release_possible;     /* a release/restart opportunity exists */
} pf_eligibility_t;

typedef struct {
    pf_eligibility_t eligibility;
    int late_request;         /* true/1, false/0, or PF_TRISTATE_UNKNOWN */
    int late_change;
    int wet;
    int poor_positioning;
} pf_context_t;

enum {
    PF_TRISTATE_FALSE = 0,
    PF_TRISTATE_TRUE = 1,
    PF_TRISTATE_UNKNOWN = 2
};

/* --- resolved plan ------------------------------------------------------ */

typedef struct {
    pf_outcome_t outcome;             /* clean or fault category */
    pf_severity_t severity;           /* clean / recoverable / terminal */
    int category_index;               /* bucket index actually charged */
    int affected_job_kind;            /* pf_job_kind_t for the faulted work */
    double affected_job_extra_seconds;/* 0 for clean/terminal */
    double terminal_presentation_seconds; /* terminal only */
    int wheel;                        /* valid when severity >= recoverable */
    pf_fuel_subtype_t fuel_subtype;   /* valid for fuel rig */
    double extra_job_seconds;
    int attempts;                     /* bounded: 1..PF_ATTEMPTS_MAX */
    int terminal;
} pf_plan_t;

/* --- effective rates snapshot ------------------------------------------- */

typedef struct {
    int multiplier_percent;           /* 60..240 */
    int base_recoverable_sum_bp;
    int scaled_recoverable_sum_bp;    /* before cap */
    int capped_recoverable_sum_bp;    /* after cap apportionment */
    int terminal_bp;
    int effective_trouble_bp;         /* capped + terminal */
    int effective[PF_CATEGORY_COUNT]; /* per bucket in table order */
} pf_rate_report_t;

/* --- deterministic RNG --------------------------------------------------- */

typedef struct {
    uint64_t state;
} pf_rng_t;

/* Domain separators for seed derivation (never reordered). */
enum {
    PF_DOMAIN_CATEGORY = 1,
    PF_DOMAIN_SUBTYPE = 2,
    PF_DOMAIN_DURATION = 3,
    PF_DOMAIN_COSMETIC = 4
};

void pf_rng_seed(pf_rng_t *rng, uint64_t seed);
uint64_t pf_rng_next_u64(pf_rng_t *rng);
uint64_t pf_seed_mix(uint64_t x);
uint64_t pf_stream_seed(uint64_t seed, uint64_t epoch, uint64_t vehicle_id,
                        uint64_t service_token, uint32_t domain);
/* Unbiased value in [0, bound); bound == 0 returns 0; rejection sampling. */
uint64_t pf_rng_below(pf_rng_t *rng, uint64_t bound);
double pf_rng_unit(pf_rng_t *rng);       /* [0,1), 53-bit */
double pf_sample_triangular(pf_rng_t *rng, pf_duration_band_t band);

/* --- configuration API --------------------------------------------------- */

void pf_config_defaults(pf_config_t *cfg);
pf_config_status_t pf_config_validate(pf_config_t *cfg, char *err, size_t err_len);
pf_config_status_t pf_config_parse(const char *text, pf_config_t *out,
                                   char *err, size_t err_len);
pf_config_status_t pf_config_parse_file(const char *path, pf_config_t *out,
                                        char *err, size_t err_len);
const char *pf_config_status_name(pf_config_status_t status);
size_t pf_config_render(const pf_config_t *cfg, char *buf, size_t buf_len);

/* --- strict numeric parsing (shared by CLI, replay and config) ---------- */

/* Digits only for integers: no sign, no whitespace, no trailing junk, full
 * range enforced. Returns 1 on success. */
int pf_parse_u64_strict(const char *text, uint64_t *out);
/* Optional single '.' decimal point, no sign/whitespace/NaN/Inf, full
 * consumption, finite result. Returns 1 on success. */
int pf_parse_double_strict(const char *text, double *out);

/* --- pure decision API --------------------------------------------------- */

/* Deterministic per-stop draw in [0, PF_DRAW_MAX) for one identity and seed. */
uint64_t pf_category_draw(const pf_identity_t *id, uint64_t seed);
/* Pure mapping from a draw value to a bucket outcome. Draw values must be in
 * [0, PF_DRAW_MAX); values outside the universe are clean. This is the single
 * selection mapping used by the planning path and by exhaustive tests. */
pf_outcome_t pf_bucket_for_draw(const pf_config_t *cfg, const pf_context_t *ctx,
                                uint32_t draw);
pf_outcome_t pf_plan_category(const pf_config_t *cfg, const pf_identity_t *id,
                              const pf_context_t *ctx, uint64_t seed);
void pf_rate_report(const pf_config_t *cfg, const pf_context_t *ctx,
                    pf_rate_report_t *out);
pf_outcome_t pf_plan_build(const pf_config_t *cfg, const pf_identity_t *id,
                           const pf_context_t *ctx, uint64_t seed,
                           pf_plan_t *out);
int pf_plan_recoverable_attempts(pf_outcome_t outcome);

/* --- overlap / added stop time ------------------------------------------ */

typedef struct {
    double normal_end;
    double fault_end;
    double added_stop_time;
    int known;
} pf_timing_result_t;

/* pf_plan_apply_timing() computes added stop time for a plan against the
 * supplied job baselines. `jobs` holds at most PF_JOB_MAX entries. */
#define PF_JOB_MAX 8

typedef struct {
    pf_job_kind_t kind;
    double end_seconds;
} pf_job_t;

int pf_plan_apply_timing(const pf_plan_t *plan, const pf_job_t *jobs, size_t n,
                         pf_timing_result_t *out);

/* --- session lifecycle --------------------------------------------------- */

typedef struct {
    int kind;                  /* pf_command_kind_t */
    int sequence;              /* per-service event index, 1-based and stable */
    uint64_t identity_epoch;
    uint64_t identity_vehicle;
    uint64_t identity_token;
    pf_outcome_t outcome;
    pf_severity_t severity;
    int attempt;               /* 1-based; 0 when not an attempt */
    double due_seconds;        /* elapsed job seconds when due */
    int delay_known;           /* 0 when baseline timing is unknown */
    double added_stop_time;    /* 0 when unknown or no measurable delay */
    int terminal;
} pf_command_t;

typedef struct pf_session pf_session_t;

/* Creates a session with an explicit immutable decision seed. All randomness
 * (category, subselection, duration) derives from this seed plus the stable
 * identity and domain; it is never re-derived mid-session. The seed may be
 * any uint64 value, including zero. */
pf_session_t *pf_session_create(const pf_config_t *cfg, uint64_t seed);
void pf_session_destroy(pf_session_t *session);
uint64_t pf_session_seed(const pf_session_t *session);

/* Clock contract: ONE shared, monotonic simulated time per session.
 *  - new_epoch() resets the clock and all storage; it is the only recovery
 *    from an invalid time.
 *  - commit/advance/pause/resume require finite time >= the last accepted
 *    time.
 *  - NaN, Inf or backwards time is a discontinuity: every live service in the
 *    current epoch is CANCELLED, undelivered commands for the epoch are
 *    discarded, and every time-bearing call fails until new_epoch().
 *  - pause() freezes progression; resume() excludes wall-clock pause time.
 */
int pf_session_new_epoch(pf_session_t *session, uint64_t epoch);
int pf_session_pause(pf_session_t *session, double now);
int pf_session_resume(pf_session_t *session, double now);

/* Commits one service; freezes config/context and decides once with the
 * session seed. The plan is written to the caller-owned *out_plan; no borrowed
 * pointer is ever returned, so a retained copy stays valid across any later
 * session call (active compaction, array growth, completion into history and
 * epoch reset included).
 * Returns 0 on success:
 *   *out_committed == 1  new service accepted
 *   *out_committed == 0  duplicate notification; the stored plan is copied
 * Returns -1 when no decision is possible; last_error carries the reason:
 *   PF_SESSION_ERR_REFUSED / TOKEN_CANCELLED / INVALID_TIME / CAPACITY.
 * History space is reserved before a service is accepted, so an accepted
 * service can always be retained at completion; exhaustion is reported at
 * this call instead of failing later. */
int pf_session_commit(pf_session_t *session, const pf_identity_t *id,
                      const pf_context_t *ctx,
                      const pf_eligibility_t *eligibility,
                      double now, pf_plan_t *out_plan, int *out_committed);

/* Advance the shared clock and drain due commands.
 *  - Due events are queued exactly once; each event is delivered exactly once.
 *  - cmds == NULL or cmd_capacity <= 0 is a query: validates/advances the
 *    clock, queues newly due events, copies nothing and returns the number of
 *    queued (undelivered) commands.
 *  - otherwise copies at most cmd_capacity commands from the front of the
 *    queue, removing them, and returns the number written (never larger than
 *    cmd_capacity).
 *  - lifecycle transitions happen when the final event is queued, not when it
 *    is drained; delivery is transport and never changes state.
 *  - returns -1 only when the session requires a reset (last_error
 *    INVALID_STATE or INVALID_TIME). Producer backpressure (delivery queue
 *    full) or a retention failure is reported through last_error
 *    (PF_SESSION_ERR_CAPACITY) while already queued events are still drained;
 *    unqueued events are retried on later calls. Nothing is silently
 *    discarded and a saturated queue never blocks draining. */
int pf_session_advance(pf_session_t *session, double now,
                       pf_command_t *cmds, int cmd_capacity);

/* Number of events queued but not yet delivered. */
int pf_session_pending(const pf_session_t *session);
/* Last session error; PF_SESSION_OK after a successful call. */
pf_session_error_t pf_session_last_error(const pf_session_t *session);

/* Optional: bind caller-supplied normal job baselines to a committed stop.
 * Returns 1 when timing is known, 0 when unknown, -1 on invalid arguments. */
int pf_session_bind_timing(pf_session_t *session, const pf_identity_t *id,
                           const pf_job_t *jobs, size_t n);

/* Cancels a service and purges ALL of its undelivered queued commands
 * (exact epoch/vehicle/token), preserving other identities and their order.
 * The identity may still be active or may already have completed into history
 * (its final event queued but undrained); the cancelled-token tombstone is
 * retained, so later commits of the token are refused.
 * Events already delivered to a caller cannot be recalled; that boundary is
 * documented. Returns 0 on success, -1 when the identity is unknown or already
 * cancelled. */
int pf_session_cancel(pf_session_t *session, const pf_identity_t *id, double now);
int pf_session_state(const pf_session_t *session, const pf_identity_t *id);
int pf_session_plan(const pf_session_t *session, const pf_identity_t *id,
                    pf_plan_t *out);
int pf_session_count(const pf_session_t *session);
uint64_t pf_session_epoch_get(const pf_session_t *session);

const char *pf_outcome_name(pf_outcome_t outcome);
const char *pf_fuel_subtype_name(pf_fuel_subtype_t subtype);
const char *pf_wheel_name(int wheel);
const char *pf_session_state_name(int state);
const char *pf_command_kind_name(int kind);

#ifdef __cplusplus
}
#endif

#endif /* PIT_FAILURE_POLICY_H */
