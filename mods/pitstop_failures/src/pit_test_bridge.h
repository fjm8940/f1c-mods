/* Explicit offline development extension; never enabled in accepted v2 builds.
 * v0.4.0-two-stage.1 adds the versioned v3 pass-through so the offline panel
 * can force a parent and (optionally) a resolution/fire leaf. The v2 entry
 * points keep their exact accepted behavior. */
#ifndef PIT_TEST_BRIDGE_H
#define PIT_TEST_BRIDGE_H
#include "pit_failure_policy.h"
#include "pit_failure_policy_v3.h"
pf_outcome_t pft_plan_build(const pf_config_t *, const pf_identity_t *,
    const pf_context_t *, uint64_t, pf_plan_t *, int force);
int pft_session_commit(pf_session_t *, const pf_identity_t *, const pf_context_t *,
    const pf_eligibility_t *, double, pf_plan_t *, int *, int force);
/* Returns PF_POLICY_MODEL_V2 or PF_POLICY_MODEL_V3 for an opened engine. */
int pft_model(void *handle);
/* v3 commit with forcing. force_parent/force_resolution/force_fire are -1 for
 * natural. fire_supported declares the effect capability (offline: 0). */
int pft_commit_v3_json(void *handle, uint32_t slot, uint64_t generation,
    uint64_t token, int force_parent, int force_resolution, int force_fire,
    int fire_supported, int tyre, int fuel, int release, double now,
    char *out, size_t capacity);
#endif
