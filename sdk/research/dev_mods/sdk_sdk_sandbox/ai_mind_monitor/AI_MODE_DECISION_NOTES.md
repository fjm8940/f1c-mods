# AISteering candidate-mode decision map

Status: static control-flow map with event-correlated live examples. Numeric
mode meanings remain provisional unless stated as direct code behavior.

For each nearby candidate, `AISteering_004270a0` computes lane bounds and
several candidate-entry metrics before assigning a mode. The relevant entry
fields are relative to the candidate-entry base (five entries, 0x44-byte
stride):

| Relative field | Direct code use | Safe interpretation |
| --- | --- | --- |
| `+0x04` | compared against 4, 5.5, 10, 15, 20 | candidate distance/progress-like metric; exact unit unverified |
| `+0x08` | `abs()` compared against 0.3, 2, 3 | lateral-offset-like metric; exact frame/units unverified |
| `+0x1C` | compared against -2, -4, 0.5, 1 and used as a divisor | closing/relative-motion-like metric; exact sign convention unverified |
| `+0x40` | selects the 0..4 mode-array element | candidate-to-mode index |

## Pre-pass proximity phase

Immediately before `AIRacingLine` and `AISteering` in the main AI update,
`OpponentProximityScanTeamRecognition_0042A7D0` rebuilds the five nearby-car
records.  It clears their transient mode/index state, finds eligible nearby
vehicles, and then assigns the compact candidate records consumed by the
later steering loop.  This is why a sampled nearby-five set can differ one
frame later from the candidate argument observed by the commit branch.

The same scan contains a separate race-session lapping path.  For a
predicate-qualified car within squared planar distance `< 625` (a 25-unit
radius before any unproven coordinate conversion), whose confirmed lap number
is greater than the local driver's, it writes the shared `+0x45FC` target and
a lateral directive of `+2.5` or `-2.5`.  This is not the `Overtaking` commit:
that later routine uses the same target pointer but writes `+3.0` or `-3.0`.
Consequently, candidate/target traces must preserve the literal side value;
the pointer alone cannot identify a pass attempt.

The code first rejects candidates into mode `5` when a close/progress safety
condition fails. It otherwise evaluates left/right lane capacity and
time/distance ratios. The selected mode is then adjusted by lane boundaries,
lateral relation, and steering conflict conditions.

Observed branch facts:

- modes `1`, `2`, `7`, and `8` call `Overtaking_0042af70`;
- mode `7` can be selected when one side's lane/clearance comparison wins;
- mode `8` is the opposing-side variant in the same comparison family;
- mode `2` may follow the active comparison branch and is also passed to
  `Overtaking`;
- modes `5` and `16` are repeatedly observed as non-active/intermediate
  states, but their final human labels remain unproven.

Live example: Martini -> Morbidelli changed `5 -> 7 -> 2` while the source
car held lateral side `-3`. Thus the system is a candidate-specific,
multi-condition state machine: it does not decide “overtake” from one RCD
number or a single distance threshold.

## Distance capture result

`ai_candidate_distance_mode0_60` captured 68 natural transitions at 30 Hz.
The candidate-entry distance/progress-like input varied substantially by mode
transition: active-family changes such as `5 -> 7`, `5 -> 2`, `16 -> 2`, and
`2 -> 7` appeared roughly in the 4--27 range in this race, while transitions
between the two intermediate states (`5 <-> 16`) also occurred at much larger
values. This disproves a simple single-distance “overtake threshold.”

The next capture therefore needs the entry's lateral and relative-motion
inputs on the same named candidate relation. Those are explicitly used by the
mode-selection branch and are expected to explain which side/mode wins at a
given distance.

## Kinematics capture result

`ai_candidate_kinematics_mode0_60b` captured 62 transitions at 30 Hz with
distance, lateral, and relative-motion inputs joined to the named candidate.
The small sample is not enough to declare universal numeric thresholds, but it
does confirm the branch is multi-dimensional:

- observed `5 -> 7` cases had lateral values around 2.6--3.4 and strongly
  positive relative-motion inputs (about 4.4--14.0);
- an observed `5 -> 2` case had a near-centre lateral value (0.87) and a
  negative motion input (-2.73);
- `16 -> 2` and `2 -> 7` appeared across overlapping distance ranges but
  different lateral/motion combinations.

This agrees with the static code: it tests the absolute lateral relation,
compares the two lane-capacity scores, and combines those scores with a ratio
that includes the candidate relative-motion input. The AI therefore chooses
the maneuver branch from **approach geometry plus relative motion**, then
applies the RCD-derived pacing/ability modifiers; it does not use RaceAbility
or Passing as an unconditional overtake switch.

## Post-commit close-range correction

The active-mode branches call `Overtaking_0042AF70`, then immediately apply
additional steering correction when the candidate distance/progress-like input
is below `10` and its absolute lateral-relation input is below `2`. A related
post-call branch also fires when that distance input is below `10` **or** the
relative-motion-like input is below `1`.

This shows that a candidate entering an overtake-capable mode is not the end
of the decision: the same per-candidate geometry continues to adjust steering
at close range. The constants are direct static facts; the physical units and
the exact human labels for the involved modes remain unproven.
