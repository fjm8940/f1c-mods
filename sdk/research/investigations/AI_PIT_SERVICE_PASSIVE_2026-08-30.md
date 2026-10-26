# Natural AI pit-service passive observation — 2026-08-30

## Scope and result

Requested scope: passively observe a naturally occurring AI pit stop in the
already-running race, without game input, process-memory writes, asset changes,
restart, forced pit call, debugger, injector, or internal helper.

**Result: natural AI pit-service behavior captured successfully after two
preserved launch failures.**  The supported front door correctly refused the
normal-integrity read with `OpenProcess access denied`, and two early hidden
elevation wrappers exited before creating an artifact.  A corrected
owner-authorized direct elevated PowerShell pattern then completed five 5 Hz
correlations and one 2 Hz combined correlation.  The successful evidence is
reported in the addenda below; the failed attempts remain documented as part
of the operational record.

## Additive read-only recipe

`runtime_memory/observations/ai_pit_service_passive_20260830_recipes.json`
was added only as a read-only input to `python tools/f1c_mod.py`.  It contains
no writer, hook, or control instruction.  Its fields are restricted to
canonical SDK field names or names that explicitly retain their tentative
evidence tier:

| Observation group | Fields available to the front door | Evidence status |
|---|---|---|
| State/request | `PitStatus`, `PitEntryLatch`, `PitServiceReadyLatch`, `CallToBox` | Canonical output/state fields; observe only.  Historical AI traces conflict on `PitStatus`, so its enum is not assumed for this race. |
| Times and job fields | `PitStopStartTime`, `PitStopEndTime`, `PitMeasuredDuration`, `PitFuelAtStart`, `PitRefuelRampStart`, `PitTargetFuel`, `PitRefuelFlag`, `PitTyreChangeFlag`, `PitWingChangeFlag`, `PitRepairFlag`, `PitTyreJobStartTime`, `pitTime_0_` | The first two are canonical SDK fields.  Remaining job/timing interpretations are static-inference candidates until a natural transition correlates them. |
| Vehicle/track context | `PitTeamIndex`, `ActivePitCrewSetPtr`, `CurrentWaypointPtr`, `Speed` | Canonical state/pointer/readout fields.  The crew pointer alone cannot prove crew work. |
| Fuel/tyre context | `FuelLevelDerived`, `TireChoice` | Fuel level retains its SDK `DERIVED` tier; tire choice is confirmed `int32`. |

The intended observer uses only `runtime-correlate`, triggering from
`PitStatus` and pairing it with one contextual field/one additional trigger.
Separate bounded runs cover service/end-time, fuel/tyre choice, team/request,
entry/speed, and waypoint/crew-pointer transitions.  This avoids claiming a
single field proves approach, service, refuelling, tyre work, completion, or
release.

## Static/inherited interpretation limits

Existing evidence supports a candidate sequence in which pit-related fields
transition around entry, service readiness, scheduled completion, and exit.
It also maps a possible cluster for fuel/tyre/wing/repair jobs.  Those are
static or prior-session findings only for this task.  In particular:

- the older and later evidence for AI `PitStatus` differs, so this task must
  observe the value rather than assign it a current semantic;
- `PitStopEndTime`, service/job flags, fuel and tyre fields are correlation
  targets, not inputs;
- `PitTeamIndex` is an index field, not direct proof of a particular physical
  stall; and
- a non-null `ActivePitCrewSetPtr` is not proof that a pit crew is currently
  performing work.

No duration/rate formula is promoted from static analysis without a successful
natural-stop capture.

## Commands and outcomes

All game-facing commands used the sole supported front door.

1. `start --json`, `sdk-query`, `evidence-query`, `recommend`, and runtime
   command help identified the field candidates and selected
   `runtime-correlate` as the preferred passive capability.
2. An owner-authorized hidden elevated PowerShell wrapper attempted five
   concurrent 300-second, 5 Hz correlations.  It exited before creating any
   front-door artifact.
3. A second owner-authorized hidden elevated wrapper attempted one 60-second,
   5 Hz correlation and a diagnostic transcript.  It too exited before output
   existed.
4. A single five-second normal-integrity `runtime-correlate` probe returned
   `ERROR: OpenProcess access denied; run Codex/terminal at the game's
   integrity level`.  It did not read or write game memory and produced no
   observation artifact.

## Safe follow-up

Do not retry with an injector, debugger, direct memory reader, game input, or
forced pit state.  Re-run the same bounded read-only front-door correlations
only after a functioning owner-approved elevated Codex/terminal session is
available.  Preserve each generated correlation artifact and analyse only a
natural stop; if no natural transition occurs, record that negative result
without forcing one.

No lifecycle-stage change is justified.  Pending validation is a successful
elevated front-door passive capture of a natural AI stop.

## Addendum — successful elevated passive capture

After the initial elevation-wrapper failures above, the owner-approved direct
elevated PowerShell pattern completed five distinct `runtime-correlate` runs.
The raw artifacts are preserved under:

- `runtime_memory/observations/ai_pit_service_20260830_state_long/`
- `runtime_memory/observations/ai_pit_service_20260830_fuel_tyre_long/`
- `runtime_memory/observations/ai_pit_service_20260830_team_request_long/`
- `runtime_memory/observations/ai_pit_service_20260830_entry_motion_long/`
- `runtime_memory/observations/ai_pit_service_20260830_topology_crew_long/`

Each run lasted approximately 300 seconds at 5 Hz, saw all 22 active slots,
and reported zero unreadable reads.  The output files state that no memory was
written.  These are sampled observations; timestamps and durations are
therefore bounded by the polling cadence and are not frame-exact.

### Observed state progression

The state correlation captured 14 `0 -> 1`, 13 `1 -> 2`, 12 `2 -> 3`, 12
`3 -> 4`, and 12 `4 -> 0` `PitStatus` transitions.  Eleven complete cycles
began and ended inside the window, while the aligned cross-stream evidence
provides 12 matched `2 -> 3 -> 4` service intervals.  This is direct evidence
for the candidate progression in this running race, not a permission to write
any field.

One slot-5 `2 -> 0` transition occurred at 32.953 seconds, 7.032 seconds
after its observed `1 -> 2` transition.  The sampled `PitServiceReadyLatch`
was 0; the team/request trace independently observed `CallToBox` clearing
`1 -> 0` with sampled status 0 at the same point.  No sampled status-3 event
occurred for that attempt.  This is conservatively recorded as an unclassified
natural return from sampled state 2 to 0, not as a successful service, a crash,
or a proven cancellation mechanism.

For the 12 matched service intervals (`2 -> 3` to `3 -> 4`), observed sampled
durations were 7.234--9.421 seconds (mean 8.630 seconds).  `PitStopEndTime`
changed on the `2 -> 3` transition in each of those intervals.  The
`PitServiceReadyLatch` was already 1 in the sampled context for those events;
the run does not establish when it was first set.

### Observed request, entry, service, and release context

- `CallToBox` changed `0 -> 1` 14 times with `PitStatus` sampled as 1.  It
  changed `1 -> 0` 12 times with sampled status 3, plus one exception at
  status 0.  This is co-occurrence evidence only.
- `PitEntryLatch` changed `0 -> 1` with sampled status 2 in all captured entry
  events.  Its sampled speed range at those events was 16.56--30.69.  Its
  `1 -> 0` changes occurred with sampled status 0 and speeds near 22.2--23.3.
- During the 12 matched service intervals, `FuelLevelDerived` rose by
  34.481--44.071 (mean 41.227).  `TireChoice` was 1 at both matched service
  endpoints and had no independent change event in the run.  This establishes
  the sampled fuel-field correlation, but not the unit, exact fill rate, or a
  claim that no tyre work occurred.
- `PitTeamIndex` stayed stable per observed vehicle slot across its captured
  transitions.  That supports it as a stable associated index in this session;
  it does not identify a physical stall or prove allocation policy.
- `ActivePitCrewSetPtr` generated no independent change events.  It remained
  a non-null contextual pointer across observed transitions, which is not
  proof of crew activity.  `CurrentWaypointPtr` was captured as context only;
  no route or location meaning is assigned here.

The job/timing candidate fields retained in the recipe (`PitRefuelFlag`,
`PitTyreChangeFlag`, `PitWingChangeFlag`, `PitRepairFlag`, and related times)
were not used as correlation contexts in these five bounded traces.  Their
static meanings remain unpromoted pending a separate passive observation.

## Addendum — 1,200-second combined passive correlation

The parent-run raw artifact
`runtime_memory/observations/ai_pit_service_20260830_combined_1200s/correlation.json`
completed successfully: 1,200.079 seconds at 2 Hz, 2,397 samples, all 22
active slots, zero unreadable reads, and 231 recorded events.  It included
the state/request/entry fields above plus the fuel, tyre, job, timing, team,
crew-pointer, waypoint, and speed contexts.

Its lower 2 Hz cadence independently recaptured `PitStatus`, `CallToBox`, and
`PitEntryLatch` activity.  It also recorded direct `0 -> 2` and `2 -> 4`
samples in addition to the adjacent-state transitions captured at 5 Hz.  That
does **not** prove a different state-machine path: at this cadence an
intermediate state can occur between samples.  The 5 Hz intervals above are
the appropriate evidence for phase-duration statements.

At captured natural-stop state transitions, the fields labelled
`PitRefuelFlag` and `PitTyreChangeFlag` were observed as 1, and the field
labelled `PitWingChangeFlag` was observed as 0.  The `PitRepairFlag` candidate
changed `1 -> 0` twice, both while sampled `PitStatus` was 3 (slots 11 and 17).
This is valuable correlation evidence for the fields, but it does not yet
validate the human-readable job labels, identify a repair type, or establish
that an unchanged flag means a job did not occur.  No write, input, restart,
or forced pit transition was used.

The exact cross-run counts, sampled timings, and caveats are also available in
the additive machine-readable sidecar
`runtime_memory/observations/ai_pit_service_20260830_summary.json`.
