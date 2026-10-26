# Two-stage pit-failure policy v3 — API, configuration and limits

Version 0.4.0-two-stage.2.. Last updated: 2026-09-22.

This document describes the versioned two-stage extension: the pure policy in
`src/pit_failure_policy_v3.[ch]` and the explicit generated session variant
`src/pit_failure_session_v3.c` (produced from the accepted session by
`tools/build_v3_variant.py`). The accepted v2 sources
(`src/pit_failure_policy.[ch]`, `src/pit_failure_session.c`) are byte-identical
to their preserved baselines and remain the shipping development base; legacy
v2 fixtures stay valid.

Owner direction and the decision contract:
`docs/TWO_STAGE_TUNING_HANDOFF_2026-09-22.md`. Review repairs and evidence:
`verification/two-stage-tuning-2026-09-22/REPORT.md` (revision 2).

## 1. Decision model

1. Eligibility pre-check. If no parent is selectable the plan status is
   `PF_PLAN_V3_SKIPPED_NO_ELIGIBLE_CATEGORY`; no draw is consumed.
2. Incident gate: one Bernoulli draw per eligible committed service,
   `incident_chance_bp` (500..1000, Standard 750); success is strictly below
   the configured chance.
3. Parent draw: one unbiased draw over eligible parent weights; ineligible
   parents are removed before the draw and the remainder renormalized. Mass is
   never moved to clean and draws are never retried.
4. Resolution draw within the selected parent (terminal outcomes are
   resolutions of wheelgun/cross-thread, never a separate bucket).
5. Duration: one triangular total extra-job sample for the resolution.
6. Corner: one draw for wheel parents only.
7. Fuel: ONE authoritative subtype draw (`FUEL_SUBTYPE` stream) that decides
   resolution, duration band, retry schedule and narration together; forced
   fuel resolutions bypass it consistently.
8. Fire: independent selection on the `FIRE` stream (500 bp = 5% of fuel
   incidents), then burn and inspection samples on `FIRE_DURATION` in that
   fixed order. Fire never rerolls the fuel subtype and is recoverable (no
   fire DNF in this version).

Every decision has its own domain derived from
`mix(seed, epoch, vehicle_id, service_token, domain)`; v3 domains are 5..12 and
never reuse v2 numbering. Audio/GUI/cosmetic calls stay off these streams.

## 2. Units

| Unit | Meaning | Where |
|---|---|---|
| basis points `bp` | 10000 bp = 100% | `incident_chance_bp`, `fire_chance_bp` |
| seconds | triangular min/mode/max, total per stop | all `*_min/_mode/_max` |
| relative weights | ordering only, not probabilities | parent, resolution, corner |

## 3. Configuration schema (v3)

`config/pit_failure_policy_v3.ini`, `[general] schema_version = 3`. All keys
are required; unknown keys, duplicates, malformed numbers, unordered durations
and out-of-range values are fatal (no partial config, no silent fallback).
Contextual modifiers are deliberately absent: no wet/late/positioning bonuses,
no pity timers, no reputation risk, no AI/player difference.

| Section | Keys | Range |
|---|---|---|
| `general` | `schema_version` = 3, `incident_chance_bp` | 500..1000 |
| `parents` | five `*_weight` | >= 0, sum > 0 |
| per-parent sections | `*_weight` per resolution and `*_min/_mode/_max` | weights >= 0, sums > 0; durations 0..60 s ordered |
| `fuel_rig` extras | `fire_chance_bp` 0..10000, `fire_burn_*`, `fire_inspection_*` | durations 0..60 s ordered |
| `corner` | four `*_weight` | >= 0, sum > 0 |

`pf3_config_defaults()` is the Standard preset; `pf3_config_render()` is
canonical and round-trippable; `pf3_config_hash()` is a stable digest for
diagnostics. Simulator `--preset calm|standard|dramatic` overrides only
`incident_chance_bp` (500/750/1000). Editing an unused INI never changes a
running DLL; the runtime preset must be embedded from one canonical source.

## 4. API summary

```c
pf3_forcing_t pf3_forcing_natural(void);
int pf3_parent_mask(const pf_context_t *ctx);
uint64_t pf3_gate_draw / pf3_parent_draw / pf3_corner_draw /
         pf3_fuel_subtype_draw / pf3_fire_draw (id, seed);
int pf3_gate_passes_for_draw / pf3_parent_for_draw / pf3_resolution_for_draw /
    pf3_resolution_count / _weight_sum / _is_terminal / _attempts /
    pf3_corner_for_draw / pf3_fuel_subtype_for_draw /
    pf3_fire_selected_for_draw;
int pf3_forcing_valid(cfg, ctx, forcing, reason, reason_len);
pf_outcome_t pf3_plan_build(cfg, id, ctx, seed, pf3_plan_t *out);
pf_outcome_t pf3_plan_build_full(cfg, id, ctx, seed, forcing, out);
int pf3_plan_apply_timing(const pf3_plan_t *, jobs, n, pf_timing_result_t *);
void pf3_rate_report(cfg, ctx, pf3_rate_report_t *);
```

`pf3_plan_t` carries the outcome/severity/attempts/timing fields used by the
scheduler plus the two-stage payload: `status`, `gate_bp`, `gate_draw`,
`gate_passed`, `parent`, `resolution`, `fire_draw`, `fire_selected`,
`fire_supported`, `fire_suppressed`, and the planned fire bands
`fire_burn_seconds`, `fire_inspection_seconds`, `fire_extra_seconds`.
`extra_job_seconds` is the frozen ordinary job delay ONLY; the fire bands are
never folded into it offline and realisation requires provider execution
evidence.

Session variant (generated from the accepted session; same lifecycle and
exactly-once delivery semantics):

```c
pf3_session_t *pf3_session_create(const pf3_config_t *cfg, uint64_t seed);
int pf3_session_commit(session, id, ctx, eligibility, now, forcing, plan,
                       committed);
/* plus destroy/seed/new_epoch/pause/resume/advance/pending/last_error/
   bind_timing/cancel/state/plan/count/epoch_get */
```

Commit order (review R2): model/identity/epoch/shared-clock rules, then
existing active/history/cancelled identities, then validation of NEW selections
against ONE frozen context built from the supplied authoritative eligibility
argument, before acceptance/reservation. Invalid forcing is refused with
`PF_SESSION_ERR_REFUSED`; changed context or forcing can never rewrite or
reject a stored plan; cancellation stays rejected. Only `-1` or a valid index
is a legal forced resolution; capability booleans are 0/1.

## 5. Diagnostics and simulator

`pf3_rate_report()` returns the configured gate, eligible mask, per-parent
conditional shares, terminal given incident/per stop and fire given
incident/per stop. The simulator (`pit_failure_sim --config <v3 ini>
[--preset ...] [--fire-support off|on]`) reports analytic rates, gate and
eligible counts, parent/resolution counts, terminal, fire selected /
supported-selected / suppressed / realised, wheel/subtype counts, commands and
timing. Every JSONL commit row is standard JSON with quoted severity and
identity/seed correlation; `tools/v3_jsonl_check.py` parses every row
independently. `--fire-support on` is a hypothetical capability model; it never
claims realisation and never changes the ordinary plan.

## 6. Test GUI (offline bridge)

`launch-test-gui.ps1 -Offline -TwoStage` builds the hash-guarded dev bridge
from the accepted parents plus the v3 variant and opens the panel with the v3
preset. The panel offers the parent list, an advanced forced-resolution control
(Natural by default) and a fire control that is disabled and labelled
unavailable because the offline provider has no fire effect. The backend
exposes `policy_model`, `selectable_failures`, `failure_labels`,
`failure_resolutions` and `fire_available`; forced fire requests are refused
with an explicit message rather than silently ignored.

## 7. Limits

- Live runtime integration (live client, native effects, retirement, fire,
  fuel suspension) is not part of this candidate; the existing live AI
  engine-stop and retirement mechanics are untouched and v3 is not wired in.
- v3 replay scripts are not implemented; replay/duplicate stability is covered
  by compiled tests.
- Tuning numbers are proposed values pending owner gameplay review.
