/*
 * pit_failure_policy_v3.h — versioned two-stage pit-failure policy extension.
 *
 * Version 0.4.0-two-stage.3 (Development build).
 *
 * Owner-approved model (docs/TWO_STAGE_TUNING_HANDOFF_2026-09-22.md):
 *   stage 1: ONE Bernoulli incident gate per eligible committed service,
 *            configured in 0..10000 bp (0..100%; standard 750 bp = 7.5%);
 *   stage 2: an unbiased categorical draw over the ELIGIBLE parent incidents
 *            (ineligible parents are removed before the draw and the rest are
 *            renormalized; mass never moves to clean);
 *   stage 3: one frozen severity/resolution draw per selected parent, with a
 *            triangular total extra-job duration;
 * plus an independent conditional fire-complication selection on FUEL
 * incidents (5% default, recoverable, no DNF in this version).
 *
 * This is an explicit versioned development variant. The accepted v2 sources
 * (pit_failure_policy.[ch], pit_failure_session.c) are byte-identical to their
 * preserved baselines; v3 owns its own plan type and session implementation
 * (src/pit_failure_session_v3.c, generated from the accepted session by
 * tools/build_v3_variant.py). Legacy v2 fixtures remain valid.
 *
 * Stream separation: every decision uses its own domain seed derived from
 *   mix(seed, epoch, vehicle_id, service_token, domain)
 * so changing one selection (or adding audio/cosmetic draws, or processing
 * another car first) cannot change an existing stop's outcome. The v2 domain
 * numbers are frozen; v3 domains are appended after them.
 */
#ifndef PIT_FAILURE_POLICY_V3_H
#define PIT_FAILURE_POLICY_V3_H

#include "pit_failure_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PF3_SCHEMA_VERSION             3
#define PF3_GATE_MIN_BP                0
#define PF3_GATE_MAX_BP                10000
#define PF3_GATE_UNIVERSE              10000
#define PF3_FIRE_UNIVERSE              10000
#define PF3_WEIGHT_MAX                 1000000
#define PF3_PARENT_COUNT               5
#define PF3_RESOLUTION_MAX             4

/* Decision model identifiers (reporting/diagnostics only). */
#define PF_POLICY_MODEL_V2             2
#define PF_POLICY_MODEL_V3             3

/* Result of the v3 eligibility pre-check, gate, parent and severity stages. */
#define PF_PLAN_V3_CLEAN                            0
#define PF_PLAN_V3_INCIDENT                         1
#define PF_PLAN_V3_SKIPPED_NO_ELIGIBLE_CATEGORY     2
#define PF_PLAN_V3_FORCED_INELIGIBLE_PARENT         3
#define PF_PLAN_V3_FORCED_INVALID_RESOLUTION        4
#define PF_PLAN_V3_FORCED_INVALID_FIRE              5

/* Parent incidents (order is frozen: it is the category-draw order). */
typedef enum {
    PF3_PARENT_SLOW_WHEEL = 0,
    PF3_PARENT_WHEELGUN = 1,
    PF3_PARENT_FUEL_RIG = 2,
    PF3_PARENT_STALL = 3,
    PF3_PARENT_CROSS_THREAD = 4
} pf3_parent_t;

/* Resolution indexes within a parent. Values repeat across parents; always
 * interpret them together with the parent. Fuel resolutions are the fuel
 * subtypes: index 0 = slow flow, 1 = temporary no flow, 2 = interrupted flow,
 * 3 = spill/short fill;
 * one authoritative draw feeds resolution, duration, retry schedule and
 * narration. */
typedef enum {
    PF3_RES_SLOW_WHEEL_MINOR = 0,
    PF3_RES_SLOW_WHEEL_AWKWARD = 1,
    PF3_RES_WHEELGUN_QUICK = 0,
    PF3_RES_WHEELGUN_BACKUP = 1,
    PF3_RES_WHEELGUN_IRREPARABLE = 2,
    PF3_RES_FUEL_SLOW_FLOW = 0,
    PF3_RES_FUEL_TEMPORARY_NO_FLOW = 1,
    PF3_RES_FUEL_INTERRUPTED_FLOW = 2,
    PF3_RES_FUEL_SPILL = 3,
    PF3_RES_STALL_STRAIGHTFORWARD = 0,
    PF3_RES_STALL_DIFFICULT = 1,
    PF3_RES_CT_CORRECTED = 0,
    PF3_RES_CT_LENGTHY = 1,
    PF3_RES_CT_IRREPARABLE = 2
} pf3_resolution_t;

/* Domain separators appended after the frozen v2 numbering (never reorder). */
enum {
    PF3_DOMAIN_GATE = 5,
    PF3_DOMAIN_PARENT = 6,
    PF3_DOMAIN_RESOLUTION = 7,
    PF3_DOMAIN_CORNER = 8,
    PF3_DOMAIN_FUEL_SUBTYPE = 9,
    PF3_DOMAIN_DURATION = 10,
    PF3_DOMAIN_FIRE = 11,
    PF3_DOMAIN_FIRE_DURATION = 12
};

typedef struct {
    int incident_chance_bp;                        /* 0..10000 */
    int parent_weight[PF3_PARENT_COUNT];           /* relative; sum > 0 */
    int resolution_weight[PF3_PARENT_COUNT][PF3_RESOLUTION_MAX];
    pf_duration_band_t resolution_band[PF3_PARENT_COUNT][PF3_RESOLUTION_MAX];
    int fire_chance_bp;                            /* 0..10000; 500 = 5% */
    pf_duration_band_t fire_burn;
    pf_duration_band_t fire_inspection;
    int corner_weight[PF_WHEEL_COUNT];             /* relative; sum > 0 */
} pf3_config_t;

/* Forced test selections (test GUI / reviewer use). Natural = no forcing. */
typedef struct {
    int parent;          /* -1 natural; else a pf3_parent_t value */
    int resolution;      /* -1 natural; else an index within the parent */
    int fire;            /* -1 natural; 0 force none; 1 force selected */
    int fire_supported;  /* 0/1 capability model; realisation is runtime work */
} pf3_forcing_t;

/* Resolved v3 plan. Mirrors the accepted plan fields used by the session
 * scheduler and adds the two-stage payload. `extra_job_seconds` is the frozen
 * ordinary (subtype/resolution) job delay ONLY; the fire bands are reported
 * separately and are never folded into the ordinary job delay offline. */
typedef struct {
    pf_outcome_t outcome;             /* clean or fault category */
    pf_severity_t severity;           /* clean / recoverable / terminal */
    int category_index;               /* parent + 1; sentinel when clean */
    int affected_job_kind;            /* pf_job_kind_t for the faulted work */
    double affected_job_extra_seconds;
    double terminal_presentation_seconds; /* terminal only */
    int wheel;                        /* -1 when not applicable */
    pf_fuel_subtype_t fuel_subtype;   /* consistent with fuel resolution */
    double extra_job_seconds;         /* frozen ordinary job delay only */
    int attempts;                     /* bounded: 0..PF_ATTEMPTS_MAX */
    int terminal;
    int status;                       /* PF_PLAN_V3_* */
    int gate_bp;                      /* configured incident chance */
    int gate_draw;                    /* observed gate draw; -1 not drawn */
    int gate_passed;                  /* 1 when the gate selected an incident */
    int parent;                       /* pf3_parent_t value, or -1 */
    int resolution;                   /* index within the parent, or -1 */
    int fire_draw;                    /* observed fire draw; -1 not applicable */
    int fire_selected;
    int fire_supported;
    int fire_suppressed;              /* selected but unsupported: fault kept */
    double fire_burn_seconds;         /* planned band, not added offline */
    double fire_inspection_seconds;   /* planned band, not added offline */
    double fire_extra_seconds;        /* planned burn+inspection sum */
} pf3_plan_t;

pf3_forcing_t pf3_forcing_natural(void);

/* --- configuration -------------------------------------------------------- */

void pf3_config_defaults(pf3_config_t *cfg);
pf_config_status_t pf3_config_validate(pf3_config_t *cfg, char *err,
                                       size_t err_len);
pf_config_status_t pf3_config_parse(const char *text, pf3_config_t *out,
                                    char *err, size_t err_len);
pf_config_status_t pf3_config_parse_file(const char *path, pf3_config_t *out,
                                         char *err, size_t err_len);
size_t pf3_config_render(const pf3_config_t *cfg, char *buf, size_t buf_len);

/* --- pure draws (one declared stream per decision) ------------------------ */

uint64_t pf3_gate_draw(const pf_identity_t *id, uint64_t seed);
uint64_t pf3_parent_draw(const pf_identity_t *id, uint64_t seed);
uint64_t pf3_corner_draw(const pf_identity_t *id, uint64_t seed);
uint64_t pf3_fuel_subtype_draw(const pf_identity_t *id, uint64_t seed);
uint64_t pf3_fire_draw(const pf_identity_t *id, uint64_t seed);

/* --- pure mappings (exhaustively testable; no RNG) ------------------------ */

int pf3_parent_mask(const pf_context_t *ctx);
int pf3_gate_passes_for_draw(const pf3_config_t *cfg, uint32_t draw);
int pf3_parent_for_draw(const pf3_config_t *cfg, const pf_context_t *ctx,
                        uint64_t draw);
int pf3_parent_total_weight(const pf3_config_t *cfg, const pf_context_t *ctx);
int pf3_resolution_for_draw(const pf3_config_t *cfg, int parent, uint64_t draw);
int pf3_resolution_weight_sum(const pf3_config_t *cfg, int parent);
int pf3_resolution_count(int parent);
int pf3_resolution_is_terminal(int parent, int resolution);
int pf3_resolution_attempts(int parent, int resolution);
int pf3_corner_for_draw(const pf3_config_t *cfg, uint64_t draw);
/* Returns the pf_fuel_subtype_t value (1..3) for a draw, or -1. */
int pf3_fuel_subtype_for_draw(const pf3_config_t *cfg, uint64_t draw);
int pf3_fire_selected_for_draw(const pf3_config_t *cfg, uint32_t draw);
/* Returns 1 when the forcing is usable for this service, otherwise 0 and a
 * fixed reason. Strict: only -1 or a valid index is a legal resolution. */
int pf3_forcing_valid(const pf3_config_t *cfg, const pf_context_t *ctx,
                      const pf3_forcing_t *forcing, char *reason,
                      size_t reason_len);

/* --- planning ------------------------------------------------------------- */

pf_outcome_t pf3_plan_build(const pf3_config_t *cfg, const pf_identity_t *id,
                            const pf_context_t *ctx, uint64_t seed,
                            pf3_plan_t *out);
pf_outcome_t pf3_plan_build_full(const pf3_config_t *cfg,
                                 const pf_identity_t *id,
                                 const pf_context_t *ctx, uint64_t seed,
                                 const pf3_forcing_t *forcing,
                                 pf3_plan_t *out);
/* Timing for a v3 plan against supplied job baselines (ordinary job delay
 * only; the planned fire bands are reported separately). */
int pf3_plan_apply_timing(const pf3_plan_t *plan, const pf_job_t *jobs,
                          size_t n, pf_timing_result_t *out);

/* --- effective-rate report (analytic, no RNG) ----------------------------- */

typedef struct {
    int gate_bp;
    int eligible_mask;
    int parent_weight[PF3_PARENT_COUNT];
    int parent_conditional_bp[PF3_PARENT_COUNT];
    double parent_conditional[PF3_PARENT_COUNT];
    double terminal_given_incident;
    double terminal_per_stop;
    double fire_given_incident;
    double fire_per_stop;
} pf3_rate_report_t;

void pf3_rate_report(const pf3_config_t *cfg, const pf_context_t *ctx,
                     pf3_rate_report_t *out);

/* --- names ---------------------------------------------------------------- */

const char *pf3_parent_name(int parent);
const char *pf3_resolution_name(int parent, int resolution);
const char *pf3_plan_status_name(int status);
const char *pf3_policy_version(void);
const char *pf3_config_hash(const pf3_config_t *cfg);

/* --- explicit v3 session variant ------------------------------------------ */

typedef struct pf3_session pf3_session_t;

/* Creates the v3 variant session. The accepted pf_session_create() is
 * untouched and continues to create v2 sessions. */
pf3_session_t *pf3_session_create(const pf3_config_t *cfg, uint64_t seed);
void pf3_session_destroy(pf3_session_t *session);
uint64_t pf3_session_seed(const pf3_session_t *session);
int pf3_session_new_epoch(pf3_session_t *session, uint64_t epoch);
int pf3_session_pause(pf3_session_t *session, double now);
int pf3_session_resume(pf3_session_t *session, double now);

/* Commits one service with optional forced test selections. Object rules and
 * identity resolution match the accepted contract: model/identity/epoch/clock
 * validation, then existing active/history/cancelled identities, then
 * validation of NEW selections against ONE frozen context (the supplied
 * authoritative eligibility argument). Returns 0 on success (*out_committed
 * 1/0 like pf_session_commit), -1 on refusal with pf3_session_last_error()
 * set. */
int pf3_session_commit(pf3_session_t *session, const pf_identity_t *id,
                       const pf_context_t *ctx,
                       const pf_eligibility_t *eligibility, double now,
                       const pf3_forcing_t *forcing, pf3_plan_t *out_plan,
                       int *out_committed);
/* CO-OP sync (2026-10-10): same as pf3_session_commit, but the plan is the
 * caller's (rolled on the PC that drives the car) instead of a local draw. */
int pf3_session_commit_given(pf3_session_t *session, const pf_identity_t *id,
                             const pf_context_t *ctx,
                             const pf_eligibility_t *eligibility, double now,
                             const pf3_plan_t *given, pf3_plan_t *out_plan,
                             int *out_committed);
int pf3_session_advance(pf3_session_t *session, double now,
                        pf_command_t *cmds, int cmd_capacity);
int pf3_session_pending(const pf3_session_t *session);
pf_session_error_t pf3_session_last_error(const pf3_session_t *session);
int pf3_session_bind_timing(pf3_session_t *session, const pf_identity_t *id,
                            const pf_job_t *jobs, size_t n);
int pf3_session_cancel(pf3_session_t *session, const pf_identity_t *id,
                       double now);
int pf3_session_state(const pf3_session_t *session, const pf_identity_t *id);
int pf3_session_plan(const pf3_session_t *session, const pf_identity_t *id,
                     pf3_plan_t *out);
int pf3_session_count(const pf3_session_t *session);
uint64_t pf3_session_epoch_get(const pf3_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* PIT_FAILURE_POLICY_V3_H */
