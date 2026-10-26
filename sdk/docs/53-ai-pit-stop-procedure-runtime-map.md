# AI pit-stop procedure — passive runtime map

**Status:** RUNTIME OBSERVED for the active 2002_Monza race; static meanings
remain DERIVED where explicitly stated.  **Date:** 2026-08-30.

## 1. Outcome

A coordinated passive capture established the natural AI pit-stop procedure
from request through release without game input, memory writes, asset changes,
restart, forced pit calls, injection, or debugger control.

The normal sampled sequence is:

`CallToBox request -> PitStatus 1 -> XPITIN/entry latch -> PitStatus 2 -> stop at box -> PitStatus 3 -> timed service -> PitStatus 4 -> pit-exit line -> PitStatus 0`

The route selector and service state are related but distinct.  `CallToBox`
rises while the car is still on the main route.  `PathSelector` then selects
the pit branch.  `CallToBox` normally clears when sampled `PitStatus` becomes
3, while the car is stopped for service; it does not remain set until pit exit.
The route selector returns to the main branch later, after service and pit-lane
exit.

## 2. Evidence and safety boundary

The live target was `F1 Challenge Online.exe`, PID 4836, with module base
`0x00400000`.  All successful observers used the supported
`python tools/f1c_mod.py runtime-correlate` front door through the
owner-authorized hidden elevated PowerShell pattern.  Every successful record
reported zero unreadable reads.  No command path capable of writing game state
was used.

The primary service evidence consists of five approximately 300-second, 5 Hz
correlations over all 22 active slots and one 1,200.079-second, 2 Hz combined
correlation containing 2,397 samples and 231 events.  The primary route
evidence consists of the earlier complete slot-18 main-to-pit-to-main cycle and
a later 1,200.844-second, 1 Hz high-capacity capture with 23,064 retained events
under its 35,000-event limit.

## 3. Normal procedure observed

### 3.1 Request and pit entry

The 5 Hz request trace observed 14 `CallToBox 0 -> 1` events with
`PitStatus == 1`.  Thirteen `PitEntryLatch 0 -> 1` events coincided with
sampled `PitStatus == 2`; vehicle speed at those samples ranged from 16.56 to
30.69.  This is consistent with the static XPITIN collision-trigger chain, but
the zone name and handler causality remain DERIVED from code rather than
proved merely by correlation.

The prior slot-18 route cycle directly observed:

| Stage | Live waypoint | Branch/state |
| --- | ---: | --- |
| request raised | 115 | main branch 0, selector 0 |
| selector entered pit route | 945 | pit branch 1, selector 1 |
| request cleared | 1009 | pit branch 1, selector 1 |
| selector returned to main | 1103 | main branch 0, selector 0 |

The later high-capacity capture independently observed natural pit-route
selection at aligned records 943–955 for most cars, with aligned record 867
for three slots.  It did not directly sample event records 983 or 984.  An
additive arithmetic correction supersedes hexadecimal/index transcription
errors in the first draft of that capture; the raw JSON itself is unchanged.

### 3.2 Box detection and service start

At 5 Hz the state trace contained 12 matched `PitStatus 2 -> 3 -> 4` service
intervals.  `PitStopEndTime` changed at every sampled `2 -> 3` edge, and the
vehicle was stopped at the box.  `CallToBox` cleared `1 -> 0` with sampled
status 3 in all 12 matched intervals.

This directly identifies status 3 as the service interval for natural AI cars
in this race and resolves the older contradictory AI trace that had reported
`PitStatus` remaining zero throughout stops.

### 3.3 Service duration and refuelling

The 5 Hz matched intervals lasted 7.234–9.421 seconds, mean 8.630 seconds.
`FuelLevelDerived` rose by 34.481–44.071 over those intervals, mean 41.227.

Independent review of the longer 2 Hz record found 17 normal service cycles
before the late post-running cluster.  Their sampled status-3 durations were
7.016–9.516 seconds, mean 8.630 seconds.  Fuel rose by 31.958–44.071, mean
39.510, and finished within 0.009 of the captured `PitTargetFuel` value in
every cycle.  This strongly validates the target-fuel relationship for this
session.  The physical fuel unit and exact per-frame flow rate are not claimed
from correlation alone.

`PitRefuelFlag` and `PitTyreChangeFlag` were 1 at captured natural-stop
transitions; `PitWingChangeFlag` was 0.  `PitRepairFlag` changed `1 -> 0` twice
during sampled status 3, for slots 11 and 17.  These names remain candidate
job semantics: the trace did not identify the repair type, and an unchanged
flag does not prove that no job occurred.  `TireChoice` remained 1 at captured
service endpoints, so this earlier trace alone did not prove a physical tyre
change; the fresh-race tread capture in section 6 later supplied that proof.

### 3.4 Release and pit exit

Normal sampled service completion was `PitStatus 3 -> 4`.  The entry latch
later reset `1 -> 0` with sampled status 0 at speeds about 22.21–23.29.  The
earlier route cycle placed final selector return on main-branch record 1103.

Static analysis maps service completion to the `PitStopEndTime` deadline,
state 4, AI command 15, cleanup, and pit-lane routing.  The runtime evidence
validates the state/timing order but does not independently prove every
internal call in that chain.

## 4. Exceptions and late-race behavior

One 5 Hz slot-5 attempt sampled status `1 -> 2`, then `2 -> 0` 7.032 seconds
later.  Fuel rose and `CallToBox` cleared, but status 3 was never sampled and
the service-ready latch was 0 at the return.  It is preserved as an
unclassified natural return, not labelled a completed service, cancellation,
retirement, or failure.

Near the end of the 1,200-second combined trace, multiple cars were stationary
and sampled from status 2 to 4 without an intervening status-3 sample.  This
cluster followed the last observed lap increment and is compatible with
post-running return-to-pits behavior.  The 2 Hz cadence and lack of a session
discriminator change mean it does not prove a direct state-machine edge or a
formal session transition.

## 5. AIW event-node result

The first high-capacity waypoint capture solved the retention problem but not
the sampling-cadence problem.  A fresh-race 20 Hz capture then ran for 600.016
seconds, retained 104,111 events under its 300,000-event cap, covered all 22
active slots, and reported zero unreadable reads.  Correct arithmetic uses
base `0x1784438C`, making record 983 `0x178900F0` and record 984
`0x1789022C`.

Seven AI cars directly traversed record 983 followed by 984 while
`PathSelector == 1` and `CallToBox == 1`: slots 12, 15, 13, 18, 17, 20, and
19.  Every pointer was record-base aligned, contiguous records 980–987 were
retained, and the observed 983→984 intervals were approximately 0.20–0.31
seconds.  This proves natural execution of both event records.  It does not by
itself reveal the downstream visible/result difference created by their
static `(1,18)` values. **OBSERVED passage; causal consumer effect pending**

## 6. Physical tyre and front-wing service

Nine fresh-race 600-second, 5 Hz correlations captured one complete natural
slot-16 stop.  Immediately before service, the four confirmed remaining-tread
fields read RR `0.996295`, RL `0.997107`, FR `0.996476`, and FL `0.996855`.
At sampled `PitStatus 2 -> 3`, all four were exactly `1.0`, remained `1.0`
through state 4, and began wearing again after exit.  This is direct evidence
of a physical tyre refresh even though `tireChoice` stayed at index 1.

The same natural stop had `FrontWingIntactFlag == 0` in states 1 and 2 and
`== 1` from state 3 onward; `carConditions` simultaneously changed 1→0 while
the repair candidate flag was 1.  The confirmed flag meaning plus the sampled
boundary strongly establishes a front-wing repair outcome, while causality
and internal job ordering still rely on the separately mapped static service
chain.  The rear-wing intact flag stayed 1 and the wing-change candidate
stayed 0.  The pit-visit candidate incremented 0→1 at state 4→0. **OBSERVED
physical outcomes + DERIVED internal ordering**

The raw `nextChosenTires__rw_` float remained
`2.802596928649634e-45`, which resembles integer bits rather than a meaningful
float; no compound-intent semantic or type correction is promoted here.

## 7. Restart comparison

The user restarted the race without terminating PID 4836.  An immediate
post-restart read found exact equality with the prior final header: selected
Monza path, waypoint count 1204, waypoint pointer `0x1784438C`, grid pointer
`0x1330B528`, pit pointer `0x04BA8008`, garage pointer `0x175906B0`, capacities
1/104/52/3, phase 0, cursor -1, and blend 1.0.  A 120.172-second rebuild
monitor produced zero events.  The process diff found 14,789 changed pages,
1,181 added, 5,894 removed, and zero transition candidates; the fresh snapshot
had 19 unreadable pages.

This proves same observable address reuse across this restart comparison.  It
cannot distinguish no reparse from a reparse whose allocator reused the same
addresses because the restart boundary itself was not continuously observed.
**OBSERVED post-restart equality + bounded limitation**

## 8. Remaining questions

1. Correlate an `AIRacingLine` consumer output or pass-score/look-ahead value
   while a car crosses records 983–984 to isolate the visible `(1,18)` effect.
2. Resolve the raw type and purpose of `nextChosenTires__rw_`, and observe a
   stop that selects a different compound index.
3. Capture independent tread/wing triggers at higher cadence to narrow the
   physical job ordering inside state 3.
4. Identify the slot-5 `2 -> 0` return with finish/retirement and garage-state
   fields.
5. Observe a true continuous setup/reparse, track-load, or weather/grip
   boundary.  Post-restart address equality alone cannot prove parser behavior.

That control comprised 1,800.296 seconds of lap observation (1,799 samples,
321 normal increments, last increment at 469.359 seconds), 1,800.469 seconds
of rebuild observation (3,596 samples, zero events), and 1,800.172 seconds of
pointer observation (3,595 samples, zero events).  The pre/post process diff
contained 22,146 changed pages, 520 added, seven removed, and zero transition
candidates.  The selected Monza path, waypoint count/base, placement pointers,
phase 0, cursor -1, and blend 1.0 remained unchanged. **OBSERVED negative
control**

## 9. Evidence index

- Service interpretation and operational history:
  `investigations/AI_PIT_SERVICE_PASSIVE_2026-08-30.md`.
- Machine-readable service summary:
  `runtime_memory/observations/ai_pit_service_20260830_summary.json`.
- Raw 5 Hz service correlations:
  `runtime_memory/observations/ai_pit_service_20260830_*_long/`.
- Raw combined service correlation:
  `runtime_memory/observations/ai_pit_service_20260830_combined_1200s/`.
- High-capacity route draft and authoritative arithmetic correction:
  `runtime_memory/observations/pit_transition_20260830_1546_highcap_waypoint_route_1200s_summary.md`
  and
  `runtime_memory/observations/pit_transition_20260830_1546_highcap_waypoint_route_1200s_arithmetic_correction.md`.
- Earlier complete main-to-pit-to-main route cycle:
  `runtime_memory/observations/pit_transition_20260830_1524_900s_summary.md`.
- Thirty-minute lifecycle control and machine sidecar:
  `runtime_memory/observations/lifecycle_20260830T155628828Z_SUMMARY.md` and
  `runtime_memory/observations/lifecycle_20260830T155628828Z_summary.json`.
- Consolidated route/service/lifecycle sidecar:
  `runtime_memory/observations/pit_procedure_20260830_dynamic_followup_summary.json`.
- Fresh 20 Hz direct event-node passage:
  `runtime_memory/observations/pit_transition_20260830_fresh_20hz_route_600s/`
  and `pit_transition_20260830_fresh_20hz_route_600s_summary.md`.
- Physical tyre/front-wing stop:
  `investigations/AI_PIT_PHYSICAL_NATURAL_STOP_2026-08-30.md` and
  `runtime_memory/observations/ai_pit_physical_20260830_summary.json`.
- Post-restart comparison:
  `runtime_memory/observations/restart_compare_20260830T163941449Z_SUMMARY.md`
  and `restart_compare_20260830T163941449Z_summary.json`.

No lifecycle stage or SDK write surface is changed by this research.

*Changelog: 2026-08-30 — initial passive runtime map; normal AI request,
entry, service, fuel-target, release, and route ordering established; one
unclassified return and late post-running cluster preserved; fresh-race
follow-up directly sampled records 983–984, proved physical tread refresh and
front-wing restoration, and established same-address AIW reuse across a
post-restart comparison.*
