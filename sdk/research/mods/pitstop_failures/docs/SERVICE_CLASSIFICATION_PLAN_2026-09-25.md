# Service classification plan: penalties and repairs — 2026-09-25

## Objective

Ensure Pit Stop Failures only affects a piece of pit work that is genuinely
being performed, and never corrupts a stop-go/drive-through penalty or a
wing/body repair by attaching an unrelated fault or changing the native repair
clock.

## Current behavior (source evidence, not gameplay acceptance)

| Stop observed at service commitment | Current v3 classification | Current result |
|---|---|---|
| Pure stop-go / drive-through / no-work visit | No tyre, fuel, wing or repair job; `pit_live_jobs` has no job and refuses commitment | No failure plan is committed. |
| Wing/body repair only | `wing_adjustment_active || repair_active` becomes generic `OTHER` work | The default 7.50% gate can select only an engine-stall incident; wheel and fuel parents are ineligible. |
| Tyres (with or without a repair) | Tyre job | Wheel-related parents are eligible. |
| Refuelling (with or without a repair) | Fuel job | Fuel parent is eligible. |

The current pure-stop-go outcome is an implicit consequence of there being no
recognised job. It is not yet protected by an explicit penalty classification
or live scenario test. The repair-only stall is an unrelated fault and is not
the desired behavior defined below.

## Desired behavior contract

1. A penalty-only visit is always **out of scope**. It must create no failure
   plan, add no hold, mutate no fuel/tyre/repair field, schedule no failure
   audio, and never consume a pending natural-failure draw.
2. A wing/body-repair-only visit is also **out of scope** until the product has
   an explicitly designed and validated repair-specific failure family. The
   mod must preserve the game's native repair deadline, including the broken
   rear-wing baseline, and must not select a stall merely because the car has
   stopped for a repair.
3. A wheel-related failure is eligible only if the native stop confirms a tyre
   service job. A wing repair is not evidence of tyre work. Thus, a driver
   who stops only for a front/rear-wing or body repair cannot receive a wheel
   gun, slow wheel, cross-threaded nut, or terminal wheel outcome.
4. When repair and tyres are both genuinely scheduled, a wheel fault may be
   selected because it attaches to the tyre job—not because of the repair. Its
   extra time is added without replacing or shortening the native repair
   baseline. Equivalent rules apply to refuelling and fuel faults.
5. Apply this same classification to player and AI slots. Never infer a job
   from vehicle damage, a pit-box state, radio call, or elapsed stop time.

## Implementation plan

1. Add an explicit, immutable service-classification snapshot at the native
   service-ready edge: `penalty_only`, `tyres`, `fuel`, `wing`, and `body_repair`.
   Obtain penalty state through an existing reviewed SDK/broker capability; if
   it cannot be read reliably, fail closed for any stop with no confirmed work.
2. Refuse commitment before the incident gate when `penalty_only` is true or
   when the snapshot contains wing/body repair but neither tyres nor fuel.
   Record a reason such as `skipped_penalty_only` or `skipped_repair_only`.
3. Make eligibility job-specific: tyre parents require the native tyre job;
   fuel parent requires the native fuel job; a future repair parent requires a
   dedicated repair job and independently reviewed game effect. Do not use the
   general release/stall parent for repair-only stops.
4. Keep native `stop_end_time` as the immutable baseline. Only a selected,
   eligible fault may extend it; do not write repair flags, wing integrity,
   repair duration constants, or the native repair clock itself.
5. Preserve an audit record per stop: classified jobs, penalty flag, eligible
   parent mask, skip reason or selected parent, native baseline, and any
   mod-added seconds. This makes accidental AI behavior diagnosable.

## Acceptance matrix

| Scenario | Required result |
|---|---|
| Player and AI pure stop-go | No plan/draw, no mod timer write, no failure voice/effect. |
| Player and AI front-wing-only repair | `skipped_repair_only`; native time unchanged by the mod. |
| Player rear-wing-only repair | `skipped_repair_only`; the game's long rear-wing baseline remains intact and no extra mod delay occurs. |
| Player and AI tyre + wing repair | Tyre parents may be selected; repair remains untouched; native baseline is never reduced. |
| Player and AI fuel + wing/body repair | Fuel parent may be selected; repair remains untouched. |
| Forced wheel fault on repair-only stop | Refused with an explicit no-tyre message; no side effects. |
| Restart, cancellation, pit exit, and AI replacement in every row | No stale plan, hold, audio, or timer write crosses into a later stop/car. |

Use deterministic adapter fixtures first, then a guarded full-race-weekend
test for each player/AI row. Compare the exact native and mod-adjusted
deadlines, inspect logs, and require human observation of the AI repair stop.
No QuickRace, direct game-memory write, or production deployment is authorized
by this plan.

## Decision needed before implementation

This is a specification only. It deliberately chooses **no mod faults on
repair-only stops**. If desired, a later definition can introduce repair-
specific failures, but that must first define their probability, narration,
native effect, timing ownership, AI behavior, and restoration rules.

## Evidence and limits

The existing current code skips a pure no-work stop because `pit_live_jobs`
requires at least one recognised job. It passes wing/body repair as generic
`OTHER`, which makes a repair-only stop eligible for the stall parent. The
existing policy documentation already requires stop-go/no-work visits not to
roll tyre/fuel failures. Exact player/AI stop-go and wing-repair gameplay has
not been validated on the shipped 1.0.3 build. No code, DLL, configuration,
game installation, production artifact, or product status changed in this
planning task.
