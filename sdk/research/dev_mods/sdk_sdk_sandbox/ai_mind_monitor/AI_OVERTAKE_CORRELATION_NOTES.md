# AI overtake / traffic correlation notes

Status: read-only sandbox evidence. A correlated field transition is not proof
of driver intention, causation, or write safety.

## Static execution chain

`AISteering_004270a0` evaluates up to five nearby cars through spatial and
vehicle-dynamics queries. It assigns a transient lane/conflict mode to each
candidate. Only modes `1`, `2`, `7`, and `8` reach
`Overtaking_0042af70`; `7`/`8` occur in close-conflict variants of the lane
selection logic.

`Overtaking_0042af70` is a final gate/commit routine, not the whole decision
maker. Its proven requirements for a lateral commit are:

1. replay state must not block it;
2. its incoming decision must be nonzero;
3. `carConditions` at `+0x3D6C` must not be `2`;
4. the `+0x3984` gate scalar must exceed `0.001`;
5. session/lap/vehicle-state comparisons must allow the supplied candidate.

On the commit branch it writes `+3.0` or `-3.0` to `+0x4420`; raw static code
also writes a candidate pointer to `+0x45FC`.

## What the commit routine actually decides

`Overtaking_0042AF70` receives its candidate and a preselected side code from
`AISteering`; it does not perform the original spatial search or choose the
side itself. On a successful commit it records the candidate at `+0x45FC`.
Side code `2` writes `+3.0` to `+0x4420`; any other nonzero side code writes
`-3.0`.

`+0x45FC` is **not exclusive** to this routine. Earlier in the same main AI
update, `OpponentProximityScanTeamRecognition_0042A7D0` can write that pointer
for a nearby, lap-ahead vehicle during race session type 7. Its associated
lateral directive is `+2.5` or `-2.5`, not the `+3.0`/`-3.0` literals of
`Overtaking`. Therefore the pointer by itself is a shared transient target;
a `+0x45FC` transition is commit evidence only when joined to the exact
`+/-3.0` directive and the other commit gates. This corrects the former
over-broad label “new overtake target.”

For race session type `7`, it accepts the supplied decision immediately when
the candidate's lap count is greater than the local driver's lap count. If
that is not true, the candidate must match the session-initialized `+0x45F8`
teammate/paired-entry pointer and pass additional vehicle-state comparisons.
`AIDriverStructInit_00424790` creates that pointer by comparing each active
vehicle's text field at `+0x46B8`; it is not produced by the nearby-candidate
scan. The 60-second live capture resolves 24 of 25 entries into 12 reciprocal
pointer pairs, and `AIAbility` reads the paired car's pit state through this
same field. This is sufficient to identify a teammate/paired-entry relation,
while still rejecting the old label “durable overtake target.” The routine
therefore has a distinct lapping/race-progress path and a teammate-continuation
path.

Before those session-specific tests it stores the maximum of the two drivers'
normalized RaceAbility scalars at `+0x450C`. The field is a comparison
artifact within this commit path, not a direct speed reading.

### Recovered session gates

The static routine now has a more precise bounded contract. In addition to the
replay, car-condition, and `+0x3984` gates above, it first updates `+0x450C`
to `max(local +0x44C4, candidate +0x44C4)`. It then decides whether the
already-selected candidate/side is eligible as follows:

- session type `5`: calls the server-session gate for both cars; the exact
  semantic polarity of that helper is still unreviewed, so this is recorded
  only as a dual eligibility gate;
- session type other than `5` or `7`: rejects when the candidate's confirmed
  `+0x409C` best-lap value is at least the local value; otherwise the supplied
  side is eligible;
- session type `7`: accepts immediately when the candidate's `+0x403C` lap
  count is greater than the local count. Otherwise it requires the candidate
  to equal the session-initialized `+0x45F8` teammate pointer and then applies
  additional vehicle-state comparisons (including the `+0x48A8` values and
  local lap count). The exact human labels for those remaining vehicle-state
  comparisons remain unproven.

Only after that eligibility decision does the routine write `+0x45FC` and the
literal lateral value. This makes `+0x45FC` a commit artifact rather than the
input candidate list, and explains why a short-lived target must be joined to
the candidate snapshot from the same sample.

## High-rate target-pointer proof

Capture `runtime_memory/observations/ai_commit_target_hz90_20/correlation.json`
ran for 20 seconds at 90 Hz, observed all 25 active slots, and had zero
unreadable reads. It captured five `+0x45FC` target-pointer transitions.

The decisive event occurred at 3.203 seconds: source slot 24 changed its
shared-target pointer from zero to `6434776`, exactly the same pointer held in
its first named nearby-candidate record. The snapshot simultaneously held the
literal commit value `-3.0` at `+0x4420`, a candidate mode-array index of 2,
and racing-line profile index 7. The `-3.0` literal distinguishes this from
the proximity scan's `-2.5` directive. The pointer returned to zero in the
next observed event at the same timestamp, explaining why low-rate captures
had missed it.

This establishes the following evidence-backed relation:

```text
named nearby candidate -> short-lived new-target pointer -> literal -3 commit value
```

It still does not establish pass completion; race order/progress must agree
before that label is used. The candidate's actual traffic mode is selected via
its mode-array index (2 in this event), so a capture must include every lane
mode slot rather than assuming lane-mode slot 0 belongs to that candidate.

## Later live capture: moving-car commit, still no completion proof

`runtime_memory/observations/ai_commit_completion_hz90_60/correlation.json`
ran for 60 seconds at 90 Hz (6,308 samples, all 25 slots, zero unreadable
reads) with the confirmed `lapNumber` and `racePosition` fields added to the
context.  It captured four nonzero `+0x45FC` edges, each followed by a clear,
all from moving slot 22 on lap 9 at standing position 17.  The first target
pointer resolves to slot 10; the later ones resolve to slot 5.  The literal
lateral value was `-3.0` at every nonzero edge, qualifying these as
`Overtaking`-branch commit evidence rather than the proximity scan's
`+/-2.5` target updates.

This removes the earlier stationary-slot ambiguity: the commit path is active
for a normal, circulating AI car.  It also tightens a limitation.  At the
90-Hz snapshots, neither resolved commit target was necessarily present in
the source car's concurrently sampled nearby-five list.  Candidate records
are updated per frame, so a point sample cannot substitute for a temporal
join of the call-side candidate argument.  The capture therefore supports
**commit** but does not identify the exact candidate-list entry for each edge.
No standing change occurred in any of those event contexts.

The raw corpus has one direct reset helper for this shared pointer:
`AIDriverRandomSelector_0042AE20` clears `+0x45FC` (and resets `+0x4420`) when
its supplied vehicle pointer equals the stored target. Its only recovered
caller is the vehicle cleanup path `sub_4C6E80`, which propagates a removed
vehicle pointer through the active AI list. This proves a lifecycle reset path
but does **not** yet explain the observed 15-ms clears: neither target's
cleanup was established in the capture. The short lifetime must therefore
remain observed behavior, not an inferred normal commit duration.

The follow-on
`runtime_memory/observations/ai_position_completion_hz60_120/correlation.json`
captured the `racePosition` trigger for 120 seconds at 60 Hz (6,889 samples,
zero unreadable reads), including lap, pit, commit, and candidate context. It
recorded zero standing-position transitions.  This is preserved negative
evidence only: no completion event happened in that window; it neither
disproves completed passes generally nor promotes any commit to a pass.

The longer
`runtime_memory/observations/ai_position_completion_hz30_300/correlation.json`
repeated the same read-only standing, lap, pit, and target-context capture for
300 seconds at 30 Hz (8,778 samples, zero unreadable reads). It also recorded
zero standing-position transitions. Together these two captures show that
the monitored race supplied no position crossover in seven sampled minutes;
they do not weaken the confirmed `position` field or imply that all traffic
commits complete. They simply leave the required commit-to-crossover proof
open for a race interval containing an actual pass.

## Long-race standing outcomes

`runtime_memory/observations/ai_position_completion_hz30_300_longrace_01/correlation.json`
captured the new long race for 300.016 seconds at 30 Hz (8,775 samples,
zero unreadable reads). It recorded 76 position-field transitions in 36
timestamp groups. Every group was a reciprocal two-car (or, once, four-car)
standing swap. Nine groups involved a nonzero `pitStatus` and are excluded.
The remaining 27 pit-free, same-lap reciprocal swaps are credible on-track
outcomes, including swaps on laps 0, 1, and 2.

This validates the `racePosition` completion-observation field in an active
race. It does **not** complete the pass proof: the trigger snapshot contained
zero `newOvertakeTargetCandidate` and zero `overtakeSideOffset`, so a preceding
same-pair literal `+/-3.0` `Overtaking` commit was not captured. Keep these as
candidate pass outcomes until a temporally overlapping commit capture joins
the pair identity and excludes pit/session effects.

## Long-race exact commit capture

`runtime_memory/observations/ai_commit_target_hz90_120_longrace_02/correlation.json`
is an elevated, read-only 120-second capture at 90 Hz (10,505 samples, zero
unreadable reads). It contains 18 zero-to-nonzero `+0x45FC` edges. Every edge
had a literal `-3.0` `+0x4420` directive, `pitStatus = 0`, and resolves to the
source driver's paired `+0x45F8` teammate: slot 10 -> 12 (14 edges) or slot
23 -> 24 (4 edges). Neither source driver's `racePosition` changed during the
capture.

This independently reinforces the exact-literal `Overtaking` write. That
particular 120-second window selected only paired teammates, but it was a
sample-specific pattern, not a universal target rule: see the later long-race
capture below. Keep the compatibility recipe names, but do not use `+0x45FC`
alone to establish a completed pass. A future proof needs the exact commit and
a pit-free standing crossover for the same pair.

Static control flow reconciles the repeat pairing. `AISteering_004270A0`
iterates the five candidate records and calls `Overtaking_0042AF70` for lane
modes 1, 2, 7, or 8; the recovered callee parameter is the candidate vehicle
and the mode selects `+3.0` only for mode 2, otherwise `-3.0`. In session type
7, the callee admits a candidate immediately when its lap count exceeds the
local car's; otherwise it requires that candidate to equal `+0x45F8` and then
passes additional vehicle-state gates. It writes that selected candidate to
`+0x45FC`. The capture did not include the target's lap count, so it cannot
choose between those two branches, but it is fully consistent with the
teammate relation and proves that `+0x45FC` is a selected steering candidate,
not an independently durable pass-completion record.

## Paired long-race correlation attempt

The elevated read-only
`runtime_memory/observations/ai_commit_target_hz90_180_longrace_03/correlation.json`
ran for 180 seconds at 90 Hz (15,953 samples, zero unreadable reads). Its 25
zero-to-nonzero selected-candidate edges were all pit-free exact commits:
24 selected non-teammate candidates across source slots 21--24, and one
selected the teammate. Twenty-four used `-3.0`; one used `+3.0`. This overturns
the previous teammate-only generalization while retaining that earlier capture
as valid evidence for its own interval.

The companion
`runtime_memory/observations/ai_position_completion_hz90_180_longrace_03/correlation.json`
ran for 180 seconds at 90 Hz (15,747 samples, zero unreadable reads) and
recorded one pit-free reciprocal crossover, slots 19 and 20 on lap 7. Neither
driver appears as a commit source in the commit capture, so the two captures
provide no commit-to-outcome pair join. Their starts were close but have no
shared frame clock; do not infer exact temporal causation.

## Shared-clock capture

The runtime correlator now accepts `--also-trigger`, recording all requested
known vehicle-field transitions in one read-only sampling loop and one capture
clock. Its first use,
`runtime_memory/observations/ai_commit_position_sharedclock_hz90_180_longrace_01/correlation.json`,
ran for 180 seconds at 90 Hz (15,654 samples, zero unreadable reads). It
recorded 15 pit-free exact candidate commits, all to non-teammates, but no
`racePosition` transitions. This validates the shared-clock mechanism and
adds no completion claim. It is the canonical next method for a same-pair
commit/outcome join.

The longer follow-up,
`runtime_memory/observations/ai_commit_position_sharedclock_hz90_300_longrace_02/correlation.json`,
ran for 300 seconds at 90 Hz (26,080 samples, zero unreadable reads). It
recorded 25 pit-free exact selected-candidate commits, all to non-teammates,
with both `-3.0` and `+3.0` directives represented. It recorded zero
`racePosition` transitions. This is strong live confirmation of the selected
candidate/lateral-directive path, but it supplies no completed-pass evidence:
without a standing crossover there is no outcome to join. Do not infer that
the commits failed, succeeded, or correspond to any particular overtaking
event outside the capture window.

## Session-lifecycle exclusion

`runtime_memory/observations/ai_lap_progress_hz30_120/correlation.json` is a
separate 120-second, 30-Hz read-only capture (3,525 samples, zero unreadable
reads). It recorded 98 `lapNumber` transitions, but around 63--67 seconds many
cars jumped by several laps simultaneously and then reset to zero. This is
session-end/reset behavior, not normal lap progression or a sequence of
passes. Do not join its standing values to a commit as completion evidence.

## Preserved follow-up capture

`runtime_memory/observations/ai_commit_target_modes_hz90_20/correlation.json`
was collected immediately afterwards with all five lane-mode slots included.
The capture completed (3 target-pointer transitions, 20 seconds at 90 Hz),
but detailed review was interrupted. It is preserved as the next-session
analysis input; do not replace or discard it.

## Live 30-second correlation

Capture: `runtime_memory/observations/ai_overtake_context_30/correlation.json`
at 10 Hz, 25 active cars, zero unreadable reads.

- 13 transitions of `+0x4420` were captured.
- Every captured transition had `carConditions=0` and a positive `+0x3984`
  gate scalar (about 68--77 for the sample cars), so those two static gates
  were open.
- The observed `+0x45FC` target was zero at every 10 Hz sample. It may be
  transient, cleared by a subsequent frame, or not be the durable target
  relationship expected from the decompiled prototype.
- Some side changes occurred under full braking, others under throttle. Thus
  `+0x4420` is a lateral planner field, but should not yet be displayed as
  “overtaking now” in the GUI.

## Restarted-race correction

The later `ai_pedal_target_candidate_30` capture observed values around
`+10` to `+12` and `-11` at this same field during low-speed congestion. That
contradicts treating its nonzero value as the exact `+3/-3` commit value from
one static `Overtaking` branch. The field either has additional writers or
stores a wider planner state after that branch. It remains useful context, but
its pass-side semantics are now explicitly **unproven**.

## Planner command labels resolved live

At every correlated sample:

| Offset | Live correlation | Meaning carried forward |
| --- | --- | --- |
| `+0x4434` | matches `ThrottleFactor` | throttle command candidate |
| `+0x4438` | matches `BrakeFactor` | brake command candidate |

Earlier provisional labels for those offsets were reversed. The historical
recipe keys remain only so older evidence paths remain readable; use
`aiThrottleCommand` and `aiBrakeCommand` going forward.

## What would establish an actual pass decision

A future capture must join three independent observations for the same driver:

1. a side directive with its per-candidate lane mode;
2. a stable nonzero candidate/target identity, or a safely captured short-lived
   target write;
3. a race-order/track-progress change consistent with passing that same car.

Until all three agree, the monitor should report “traffic lateral action” and
show the raw context rather than claiming an overtake intent.

## 2026-08-18 static writer/consumer direction recovery

### Verified static facts

- `Overtaking_0042AF70(this, a2, a3)` writes `a2` to the *current* vehicle's
  `this + 0x45FC` only after its session/eligibility decision and the
  `carConditions`/`+0x3984` commit gates pass.  `AISteering_004270A0` is its
  sole recovered caller and supplies the current steering-update vehicle as
  `this` and a selected nearby-candidate vehicle as `a2`.
- `OpponentProximityScanTeamRecognition_0042A7D0` uses the same ownership:
  during session type 7 it writes a scanned vehicle pointer into the current
  vehicle's `+0x45FC` when the scan predicate succeeds, squared distance is
  below `625.0`, and the scanned vehicle's `+0x403C` lap count is greater than
  the current vehicle's.  Its accompanying `+0x4420` literal is `+/-2.5`,
  selected by a 2-D cross-product sign.
- The recovered per-frame downstream read is in
  `AISteeringCorrection_00426CF0`: a nonzero current-vehicle `+0x45FC` causes
  its `+0x457C` steering-limit field to be set to `AIDifficultyScalar() *
  0.75`.  This is a local steering-control consequence, not a standings write
  and not a read of the selected vehicle's state.
- `AIDriverRandomSelector_0042AE20` only clears a current vehicle's `+0x45FC`
  when its supplied cleanup vehicle pointer equals that stored value.  Its
  recovered caller is the vehicle-reset path; it does not explain normal
  short-lived frame-to-frame clears.

### Bounded interpretation

`+0x45FC` is now proven to be a **current-vehicle-owned selected-interaction
pointer**: the writing car is the car whose AI update is running, and the
stored pointer identifies its selected other vehicle.  It can feed a local
steering-limit adjustment, while its `+/-3.0` form additionally identifies
the exact `Overtaking` commit path.  This proves source/target storage
direction, but not attack/defence role and not which participant will win a
subsequent standing crossover.  The named Monza result remains compatible:
Räikkönen's AI update selected Coulthard, yet Coulthard gained the position.

### Still open

`PositionLapStateUpdate_00447E00` is a per-frame notification/UI-state
consumer: it receives an already-computed position value, compares it with a
saved value, and emits gained/lost presentation state.  It is not the
standings-ranking writer or a completed-pass success branch.  The ordering
producer that supplies that value, and its exclusions for pit, lapping, grid,
and session transitions, remain to be recovered.

## 2026-08-18 race-order promotion branch recovered

### Verified static facts

- The live `racePosition` field is vehicle `+0x4060` (decimal `16480`).
  `AIDriverSorting_004BF390` is its recovered race-session writer.  It calls
  `ConditionalFieldSetter_004BC2F0` with a state subobject at vehicle
  `+0x4034`; that helper compares its `+0x2C` member, which is exactly the
  enclosing vehicle's `+0x4060`, and writes the new position when it differs.
- For session type `7`, `AIDriverSorting` scans position values in ascending
  order.  For the vehicle currently being ranked (`a1`) and a vehicle already
  occupying a candidate position (`v6`), it does **not** promote `a1` when
  `v6.lapNumber > a1.lapNumber`, or when their lap numbers are equal and
  `a1`'s ordering scalar at `+0x4098` is not smaller than `v6`'s.  Otherwise
  `a1` is better ordered: it receives `v6`'s position and the displaced car
  is shifted back one place; later affected cars are shifted as necessary.
- `AITickUpdate_004BF4F0`, `AIBehaviorInitialization_004BF0A0`, and the
  collision/engine-failure loop all update the same ordering inputs before
  calling `AIDriverSorting`.  Their normal path advances `lapNumber` and
  writes the per-car scalar at `+0x4098` before sorting.  Cars in state `2` or
  `3` are excluded by `AITickUpdate` before this update.
- A changed position flows through `ConditionalFieldSetter` to
  `AIDriverStateTransition_00419120`, which has race-active,
  nonzero-track-condition, and non-finish-phase gates before it emits its
  commentary/UI records.  This is downstream notification of a position
  change, not the ranking decision itself.

### Bounded interpretation

The actual completed **standing-crossover** success branch is therefore the
race-session promotion in `AIDriverSorting`, not the `+0x45FC` commit and not
`PositionLapStateUpdate`.  Its direct ordering rule is lexicographic:
`higher lapNumber` wins; on equal laps, the smaller `+0x4098` ordering scalar
wins.  The writer does not require a `+0x45FC` interaction, a `+/-3.0`
directive, or a 225/226 event.  Those signals can corroborate a traffic
interaction but cannot define every standing crossover.

### Still open

The recovered ranking routine contains no explicit pit-state exclusion.
Accordingly, a live same-pair `+0x4060` crossover still must exclude pit and
session/grid transitions in capture analysis before it is labelled an
on-track completed pass.  The exact semantic name and update source of
`+0x4098` also need a dedicated static/runtime validation; its present role is
only the proven equal-lap tie-break scalar.

## 2026-08-18 pit-state boundary around ranking

### Verified static facts

- Vehicle `+0x4064` is the SDK's `FinishState`.  The active ranking callers
  skip values `2` and `3`; value `3` is directly used by
  `DisqualificationCheck_0049BAB0`, so it is a terminal/disqualification
  state.  The exact human label for value `2` remains unproven.
- `PitStateUpdateMachine_004A6E50` owns a separate pit-machine state at its
  own `+0x184` (decimal `388`) and has no recovered write to vehicle
  `+0x4064`.  `PitExit_0041C0F0` reads existing race positions only to choose
  commentary; it does not rank vehicles.
- Therefore, no recovered code establishes that entering, stopping in, or
  leaving the pits automatically sets `FinishState` to `2` or `3`, or that the
  race sorter explicitly excludes a pitting car.

### Bounded interpretation

The sorter's verified exclusion is for terminal/finish-state values, not a
proven pit exclusion.  Pit-free filtering remains necessary in live analysis
because pit timing can change lap/progress ordering without any explicit
`AIDriverSorting` pit gate.  Do not describe the `+0x4064` check as a pit
filter unless a future trace proves a pit-to-finish-state transition.

### Still open

The semantic name of the equal-lap `+0x4098` scalar should be retained as
**race-order / finish-line timestamp scalar**.  Its writers show that it is
derived from session-time calculations and `AIDriverSorting` proves that
smaller wins on equal laps, but the code reviewed here does not yet prove a
more precise public-facing label.

## Separate race-event path: useful completion evidence, not a proof by itself

`OvertakingEventTracker_0041AD80` is a distinct, per-frame path called from
`CollisionDetectionSystem_004EA970`, not from `Overtaking_0042AF70`.  When its
race-active/session gates are open, it applies a two-second per-driver-pair
cooldown and a `> 1250.0` threshold to the caller-supplied geometry magnitude.
It then records both driver identities and emits a player-involved or AI-vs-AI
session event (AI event IDs `225` or `226`, selected partly by a noise table).

### Event 225/226 mechanics resolved statically

The tracker receives two vehicles from the collision system, not from the
lane-selection or `Overtaking` commit path.  It records a pair only after both
vehicles have passed its per-pair two-second cooldown and the collision caller
supplies a geometry magnitude greater than `1250.0`.  It then compares the
two vehicles' projected track distances:

- If the sampled `flt_8AB274[dword_8AAE70++]` value is positive, it emits
  event `225`, ordering the two driver IDs so the vehicle with the lower
  projected distance is first.
- If that sampled value is non-positive, it emits event `226` with the same
  two IDs in the opposite order.

Thus `225` and `226` are alternate collision/proximity commentary variants
for the same tracker condition.  They do not encode a lane-commit side, an AI
intention, or a completed race-order pass.  The existing pair-resolved `226`
capture is therefore corroborating traffic context only; a completed-pass
claim still requires the independently sampled selected candidate, exact
`+/-3.0` commit, pit-free same-pair order crossover, and the event pair.

This gives a practical fourth corroborator for a future capture: an
AI-vs-AI event carrying the same two vehicle identities near a confirmed
progress/race-order crossover.  It must **not** be used alone as the
definition of a completed pass: the caller builds the threshold input from
collision/geometry state, and the tracker has no direct call edge from the
lane-commit routine.  The still-required proof is the four-way join:

```text
candidate -> commit -> relative progress/order crossover -> matching session event
```

## 2026-08-18 shared-clock teammate crossover: linked outcome, not a universal pass proof

`runtime_memory/observations/ai_exact_commit_position_hz90_600_20260818c/correlation.json`
is a 600-second, 90-Hz elevated read-only capture using `overtakeSideOffset`
as its primary trigger, with `newOvertakeTargetCandidate` and `racePosition`
on the same clock.  It completed with 49,946 samples and zero unreadable
reads.

At `t=540.500` and `t=543.578`, slot 21 selected vehicle pointer `0x00650758`
(slot 22) while holding `-3.0`, with pit status zero, lap 31, and speeds about
46.2 and 55.6.  At `t=548.062`, the same two slots made a pit-free reciprocal
standing transition: slot 21 changed P20 -> P21 and slot 22 P21 -> P20.  The
selected pointer equals slot 21's recorded `teammateVehicle` pointer, so this
is a **same-pair, same-lap commit-to-crossover linkage for the paired teammate
path**.

Do not generalize it into a completed overtake definition.  The crossover is
in the direction opposite slot 21's standing improvement, and no matching
`OvertakingEventTracker` session event was captured.  It proves that an exact
paired-target lateral commit can precede a same-pair standings outcome; it
does not yet establish whether the source was attacking, defending, yielding,
or which game-level event labels the maneuver a completed pass.

## 2026-08-18 pit-stop exclusion confirmed

`runtime_memory/observations/ai_pit_traffic_commit_hz90_600_20260818a/correlation.json`
completed with 49,372 samples and zero unreadable reads.  Its 109
`racePosition` transitions include a sustained pit sequence for slot 17 on
lap 2: it fell from P15 to P24 while `pitStatus=1` and its speed was roughly
0.1--1.2.  During that same pit state the capture records repeated exact
`overtakeSideOffset=-3.0` transitions, sometimes with a nonzero candidate
pointer.  Therefore an exact side directive, candidate selection, and a
standing loss may co-occur during service without representing an on-track
pass.  `pitStatus==0` remains a mandatory filter for any pass-outcome claim.

## Event-correlation capture extension (static design; runtime validation pending)

The event corroborator should be captured from the commentary controller, not
treated as a separate session-event table. Static cross-reference evidence
shows that `OvertakingEventTracker_0041AD80` prepares event code `225` or
`226` and the two driver identities, then calls
`InsertDriverTableEntry_00418560` with `PointerTableEntry` (`0x005EA618`) as
its controller object. That insertion function is also the shared
commentary-queue path used by AI position, overtake/defend, pit-strategy, and
other race event emitters.

The safe read-only extension is therefore a controller observation rather
than a claim that a raw `+0x86C` buffer is a durable event history:

1. read the controller at the module-relative direct base `0x005EA618`
   (`module + 0x1EA618`) without writing it; it is not an extra pointer
   indirection;
2. sample its per-event timestamp array (`controller + 0xAF8 + eventId * 4`)
   for IDs `225` and `226`, alongside the existing shared-clock vehicle
   samples;
3. when either timestamp changes, preserve the vehicle-slot/driver-ID mapping
   and current candidate, exact side directive, progress/order, pit state,
   and lap in the same record;
4. accept an event only when its two tracker-supplied driver identities can be
   recovered from the insertion path or otherwise verified in that capture;
   the timestamp alone identifies dispatch, not the two-car pair.

`InsertDriverTableEntry_00418560` reads a transient twenty-entry, 32-byte
buffer at controller `+0x86C`; it is useful only for a future synchronized
read/trace and must be live-validated before field offsets are relied upon.
The controller's `+0xAF8` timestamps have independent live evidence for
event-dispatch observation. No event-table field layout or event-to-pass
meaning is asserted here until a read-only capture ties it to a specific
vehicle pair.

### 2026-08-18 direct-controller capture result

`runtime_memory/observations/ai_event_clock_direct_hz90_600_20260818b/`
completed for 600 seconds at 90 Hz with 47,150 samples, 25 active slots, and
zero unreadable reads. It observed 447 vehicle-field transitions (295 side
offsets, 110 candidate selections, and 42 standing changes). The controller
timestamps were readable and stable throughout: event 225 was `2600.6025`
and event 226 was `2069.2009`. Thus this window has **no 225/226 dispatch**
to correlate with its traffic activity. It validates the direct-controller
read model but supplies no completed-maneuver evidence and no pair identity.

The remaining event-correlation requirement is a longer or differently
seeded read-only capture that sees a timestamp change, plus an observation at
the insertion boundary (or an independently recoverable event record) that
retains both driver IDs. Do not infer a pass from a stationary event timestamp
or from a traffic transition without that pair linkage.

## Event-pair retention recovered statically (runtime pair capture pending)

`DriverDataTableLookupAndTimeConversion_004144F0` constructs an eight-DWORD
event record, and `InsertDriverTableEntry_00418560` retains it in the
controller's twenty-record priority table. The table begins at controller
`+0x864`, has `20 * 0x20` bytes, and is shifted/inserted as complete
eight-DWORD records. The recovered field layout is:

| Record offset | Meaning |
| --- | --- |
| `+0x00` | event code (225 or 226 for the tracker path) |
| `+0x04` | controller-resolved event score/priority |
| `+0x08`, `+0x0C` | converted dispatch-time fields |
| `+0x10`, `+0x14` | the two tracker-supplied driver identities |
| `+0x18`, `+0x1C` | remaining context/default fields |

For an `OvertakingEventTracker` AI event, the two driver IDs are supplied to
the constructor before insertion and therefore survive at `+0x10/+0x14` in
the queued record. The next read-only pair-capture extension should sample
all twenty records at `0x005EA618 + 0x864 + recordIndex*0x20`, detect a
new/changed code 225 or 226, and resolve both IDs against the capture's
vehicle-slot driver-ID map. A controller timestamp change can then be used as
the shared-clock dispatch marker, but must not replace record-level pair
matching. This is static retention proof; the exact live queue timing and
record eviction behavior are still unvalidated.

### 2026-08-18 live AI pair capture

`runtime_memory/observations/ai_event_pair_final_lap_hz90_120_20260818a/`
completed with 9,769 samples, 25 active slots, zero unreadable reads, and 91
transitions. At `t=54.734`, its controller queue gained event `226` with
driver IDs `226` and `264`. The contemporaneous driver-ID map resolves these
to slots 23 and 9, respectively. The same record left the filtered queue at
`t=55.781`; this is consistent with controller queue turnover, not a second
AI event.

Slot 23 had a pit-free exact `-3.0` side transition at `t=49.953`, returned
to zero at `t=54.047`, and committed `-3.0` again at `t=55.468`. Its standing
was P22 on lap 36 during those transitions. This is the first live,
pair-resolved AI event/traffic linkage: event 226 is tied to slots 23 and 9
and occurs between nearby exact lateral commits for slot 23. It remains **not
a completed-pass proof**: no selected candidate was retained for the pair and
no same-pair pit-free standing crossover occurred in this short final-lap
window.

### 2026-08-18 Monaco shared-clock capture results

Two elevated, read-only 600-second captures ran against the live Monaco GP;
each sampled 22 active slots with zero unreadable reads.

`monaco_ai_event_pair_sharedclock_20260818a` collected 49,020 samples and
captured four retained tracker events: `226` for driver-ID pairs `41/5`,
`97/15`, and `41/30`, plus `225` for `15/97`. The concurrent ID maps resolve
those pairs to slots `6/1`, `15/3`, `6/2`, and `3/15`, respectively. This
confirms live queue-pair capture and usable two-driver identity resolution.

`monaco_ai_pass_join_sharedclock_20260818a` collected 49,998 samples with
queue, selected-candidate, and `racePosition` triggers on one clock. It
recorded 77 candidate/order transitions, including pit-free reciprocal
standing swaps, but no retained 225/226 record in that later window. Its
exact candidate transitions were mostly from non-circulating slot 0; the sole
circulating source with a short-lived exact candidate had no same-pair
standing crossover.

Monaco therefore strengthens validation of queue-pair identity and the
independently sampled candidate/order streams, but **not** the required
four-way same-pair completion proof. The monitor's remaining limitation is
sampling opportunity: a queue-event and candidate/order window must overlap
for a real circulating pair. Do not treat a queue event or standings swap
alone as a completed pass.

### 2026-08-18 Monza 2002 named pass join

The `monza_2002_aggression90_named_pass_join_20260818a` elevated, read-only
capture ran for 600 seconds at 90 Hz (22 active cars, zero unreadable reads)
with immutable driver IDs and `F1CVehicle::DriverName` on the same clock. It
recorded 16 reciprocal, pit-free, lap-positive standing swaps. Notable named
examples include Coulthard over Räikkönen (lap 9), a Trulli/Sato pass-repass-
pass sequence (lap 10), and Ralf Schumacher over Button (lap 14).

The decisive same-pair join occurred at `t=45.109`: Kimi Räikkönen (P5,
lap 9, pit-free) wrote the transient candidate pointer for David Coulthard
(slot 6) with lateral value `-3.0`. At `t=49.453`, Coulthard moved P6 -> P5
while Räikkönen moved P5 -> P6. This is a direct target-selection/lateral-
command/standing-crossover join for one circulating pair.

It **does not** establish that the writer is the eventual passer: the selected
source was Räikkönen, while Coulthard gained the standing. The `+0x45FC`
signal is therefore an active opponent/candidate interaction signal; source-
to-target pass direction remains ambiguous because the field also has a
proximity/lapping writer. Do not present this field as a committed
source-overtakes-target claim without a separately recovered decision branch.
