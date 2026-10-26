# Why an AI driver lifts, brakes, or holds throttle

Status: static control-flow map plus live command correlations. The command
fields are observable outputs, not safe write targets.

## Per-frame control chain

`AIMainUpdate_00424E70` runs the relevant stages in the same AI update:

```text
track waypoint / future path / nearby vehicle geometry
  -> AIRacingLine_00425920 builds a forward speed profile
  -> AI throttle and brake command candidates (+0x4434 / +0x4438)
  -> AISteeringCorrection_00426CF0 smooths or constrains the result
  -> vehicle ThrottleFactor / BrakeFactor
```

Live correlation established that `+0x4434` moves with `ThrottleFactor` and
`+0x4438` moves with `BrakeFactor`; the historical reversed recipe names are
retained only to preserve older captures.

## Track-location reason

`AIRacingLine` follows the current waypoint and a forward chain of future
waypoints, storing a short speed profile. It compares the current vehicle
speed against the profile's next/future target and uses tire/aero force
calculations to decide whether the path can sustain that target. RaceAbility's
dynamic multiplier, RCD corner parameters, weather-related factors, and the
Passing-normalized racing-line multiplier feed this calculation.

So at a particular location the basic reason for lifting is: **the forward
waypoint profile says the available speed for the approaching path is lower
than the car's present/desired speed**. The basic reason for holding throttle
is the opposite: the forward profile remains viable.

## Nearby-car reason

When a nearby vehicle/path condition is active, `AIRacingLine` computes an
avoidance distance from the candidate geometry. On the close-range branch it
sets the brake-side planner output from a braking calculation and zeros the
opposing command; on the clear branch it restores the normal full-command
state. Candidate traffic modes then pass through `AISteering` and
`Overtaking`, which may add a lateral directive and close-range steering
correction.

This is why an AI can lift at the same waypoint where it normally stays flat:
the track profile is the baseline, but nearby-car geometry can replace or
constrain it before the final pedal factors are applied.

## Final modifiers

`AISteeringCorrection` subsequently smooths and limits the command state
using steering/yaw, speed, grip/degradation, and special-state conditions.
Therefore a single snapshot cannot prove one unique reason for a pedal value.
For a defensible explanation, a live event must join: waypoint identity,
forward speed-profile state, selected candidate/mode, and the final command
transition. The current monitor supplies the waypoint, named candidate/mode,
and final commands; the internal future speed-profile slots are the next
telemetry expansion.

## Live target-speed proof

Capture `runtime_memory/observations/ai_pedal_target_candidate_30` sampled 25
active cars for 30 seconds at 15 Hz with zero unreadable reads. It provides a
direct example of the baseline track-speed mechanism: Eddie Irvine (slot 6)
had speed about `58.65`, then the selected racing-line target became about
`40.54`; in that same sample the throttle command dropped to `0.0` and brake
command rose to `0.40`. His active mode was `5` with Jean Alesi as the first
nearby candidate, so this does not prove that no traffic influenced the
target—but it proves the live target-speed value is present exactly when the
planner commands braking.

The common `9999.0` target value is an unbounded/default sentinel, not a
literal desired speed. The monitor now labels it accordingly.

## Profile-to-traffic coupling

The forward profile is also read directly from `AISteering_004270A0` while a
nearby candidate is being evaluated. The raw branch compares a candidate
motion/speed-related value against a profile entry, then forms a positive
margin from their difference plus `5.0`; a very small result is clamped at
`0.01`. Another candidate branch averages a profile entry with an AI steering
speed term before combining it with the candidate distance/motion ratio.

This is direct code evidence that the AI does not first select an abstract
pass and only afterwards choose a pace. Its expected forward speed profile is
an input to traffic/lane feasibility itself. Exact correspondence between each
of the 20 profile entries and physical track distance still needs a controlled
live correlation.

Each of the five candidate records also has a separate profile index, distinct
from its traffic-mode index. `AISteering` stores that index in the candidate
state, uses `-1` as an unavailable sentinel, then indexes the 20-value
racing-line profile with it. The monitor now exposes both indexes: mode tells
us which traffic state is selected; plan index tells us which planned-speed
slot participated in that candidate evaluation.
