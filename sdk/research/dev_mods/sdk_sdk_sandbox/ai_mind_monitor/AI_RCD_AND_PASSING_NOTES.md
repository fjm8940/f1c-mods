# RCD inputs and AI passing: working evidence map

Status: sandbox research note, read-only observations only.  It is not a
claim that a field is safe to write, and it does not modify either game
installation.

## What the RCD parser contributes

`sub_424080` loads driver configuration values including `RaceAbility`,
`Passing`, `CorneringAdd`, and `TrackAggression` into vehicle-local AI state.
That means an RCD is an input to the same live state used by the driving and
traffic routines; it is not just menu metadata.

The active clean installation's own RCD comments remove two common
ambiguities: `RaceAbility` is a 0--6.2 ranking where **0 is best**, while
`Passing` is the driver's percentage of completed successful passes (excluding
pit stops and lapped traffic). For example, the currently inspected driver
files list Alesi as `RaceAbility=9.1`, `Passing=48.0`, and Hakkinen as
`RaceAbility=2.8`, `Passing=88.0`; this particular season/mod extends the
documented RaceAbility range, so its absolute numbers must be interpreted in
that mod's context. Both files also state the exact corner adjustment formula:
`adjusted speed = CorneringAdd + CorneringMult * original speed`.

| Candidate vehicle offset | Static origin | Live observation | Interpretation |
| --- | --- | --- | --- |
| `+0x44CC` | `0.9 + RCD Passing * 0.001`, capped at 1.0 | 18 distinct stable values across 25 cars, approximately 0.971-1.000 | Strong `Passing`-derived per-driver-state signature. |
| `+0x44D4` | RaceAbility-derived scalar | 22 distinct stable values across 25 cars, approximately 0.516-0.924 | Strong RaceAbility-derived per-driver-state signature. |
| `+0x44D8` | Parser decompile had labelled it `CorneringAdd`; canonical SDK derived alias is `RaceAbilityPushScalar` | 22 distinct stable values, approximately 0.041-0.119 | **Resolved enough to reject raw CorneringAdd storage:** the live values are incompatible with the literal `CorneringAdd=1.5` RCD values. It is likely a RaceAbility-derived corner-speed/push scalar; exact transform remains open. |
| `+0x44F4` | RaceAbility-derived setup scalar | 22 distinct stable values, approximately 0.804-0.932 | Strong per-driver-state signature; transform still needs controlled proof. |
| `+0x453C` | Dynamic multiplier in the AI corner-speed expression | Supported: 5 natural changes in 10 seconds; values overlap the RaceAbility candidate range | `AIAbility_00429470` is the confirmed producer; its exact human-facing semantic name and complete probability model remain unmapped. |
| `+0x44F8` | RCD TrackAggression after global scaling and a 0.01 minimum clamp | Two stable values across 25 cars: 0.6878 and 1.0 | The correct TrackAggression candidate; the former `+0x44CC` label was wrong. |

The static offsets are mapping candidates kept in `telemetry_recipes.json`.
Their live observer results and samples are preserved under
`runtime_memory/observations/ai_rcd_*` and
`runtime_memory/observations/ai_pass_ability`.

## Recovered session-initialization transforms

The direct initializer is now identified as `sub_424080`.  It runs once for a
vehicle's AI state, looks up the RCD keys by name, and then overwrites several
raw fields with the values that the frame loop actually consumes.  This
settles an important lifecycle distinction: RCD values are session inputs;
the `+0x44C4`--`+0x44F8` values below are their derived per-driver state, not
live copies of the text values.

For the mapped fields, the raw control flow establishes these transforms
(where `D` is the already-built global difficulty scalar and `r` is the
global realism/config scalar):

```text
effective TrackAggression (+0x44F8) = max(0.01, r * RCD TrackAggression)
RaceAbility pace base (+0x44F4)    = (1 - RCD RaceAbility / 100) * D
RaceAbility push term (+0x44D8)    = (1 - +0x44F4) * 0.67
Passing normalized (+0x44CC)       = min(1.0, 0.9 + RCD Passing / 1000)
```

It also converts the raw RaceAbility field into an ability-normalization term
based on `(22 - RaceAbility) / 22`; below a strength setting of 100, that
term is further multiplied by the square of the strength fraction.  This
explains why the raw RCD number should not be inferred directly from the
later `+0x44C4`/`+0x44D4` family of fields.  `CorneringAdd` and
`CorneringMult` remain direct RCD values at `+0x4500` and `+0x4504` before
their racing-line consumer applies them.

## The corner-speed equation

The previously labelled `AIComputePassScore` routine is structurally a
corner-speed calculation. The raw loader proves `+0x4500` is RCD
`CorneringAdd` and `+0x4504` is RCD `CorneringMult`, which matches the RCD
comment exactly:

```text
adjusted corner speed = (input * self[+0x4504] + self[+0x4500])
                        * self[+0x4580]
                        * self[+0x453C]
                        * other-context factor
```

It is invoked from the racing-line/traffic path. This confirms that
`CorneringAdd` and `CorneringMult` directly shape the speed the AI attempts in
a corner. RaceAbility supplies several derived vehicle fields (`+0x44D4`,
`+0x44D8`, `+0x44F4`) which are the leading candidates for the straight/corner
pace effect the user observed. The link from RCD `Passing` to actual overtake
commit is still open: it must be mapped through `Overtaking_0042af70` rather
than inferred from this corner-speed formula.

## TrackAggression directly biases traffic geometry

`AISteering_004270a0` directly reads the live, scaled RCD TrackAggression
field at `+0x44F8` while it scores nearby candidates. In the raw control flow
it changes the lateral lane margin, candidate-distance/traffic score term,
minimum candidate threshold, and the accumulation rate of a candidate traffic
score. Higher TrackAggression reduces the relevant spacing/threshold terms.

This is direct static evidence that TrackAggression affects *where and when*
the AI enters the traffic-mode state machine, not merely overall pace.

More precisely, the scaled effective aggression is used as the `a` term in
the steering traffic calculation. The code forms spacing/window terms
equivalent to `2 - a`, `4 - a`, and `20 - 10a`, and accumulates nearby-car
pressure in proportion to `a` while the candidate is within an
aggression-scaled distance window. Consequently, raising TrackAggression
does not grant power or grip: it reduces the conservative space requirement
and makes the nearby-car branch accumulate toward a maneuver state sooner.

## RaceAbility reaches throttle and brake through corner target speed

`AIAbility_00429470` recalculates the dynamic multiplier at `+0x453C` during
the AI update. Its inputs include the RaceAbility-derived fields at `+0x44D4`,
`+0x44D8`, and `+0x44F4`; the routine can adjust the dynamic value in 0.01
steps and contains a probability branch based on the RaceAbility-derived
state. This is stronger than a correlation: the corner-speed equation above
directly consumes `+0x453C`.

The verified causal chain for pace is therefore:

```text
RCD RaceAbility
  -> baked RaceAbility scalars (+0x44D4/+0x44D8/+0x44F4)
  -> AIAbility dynamic multiplier (+0x453C)
  -> racing-line corner target speed
  -> AI throttle/brake command
  -> vehicle ThrottleFactor/BrakeFactor
```

This explains why RaceAbility changes both how early a driver brakes and how
strongly it returns to throttle at the same section of track. It is not a
single global straight-line-speed variable. The exact transform from the RCD
number to each baked scalar still requires a controlled one-field RCD test.

### Recovered bounded update of the live multiplier

The dynamic field is no longer only a correlation candidate.  Session setup
calls `LateralForceTorqueCalculation_0042AD00(this, 100)`, which writes:

```text
+0x453C = +0x44F4
```

In race session type 7, `AIAbility_00429470` then moves `+0x453C` by at most
`0.01` per update.  With no current duel-pressure flag (`+0x4508 == 0`), it
cannot fall below the baked baseline `+0x44F4`.  With that flag set, it rises
toward the boosted target `+0x44F4 + +0x44D8`.  If the shared duel metric at
`+0x450C` is nonzero, a random comparison chooses between the same boosted
approach and a `0.5 * +0x44F4` retreat target, again rate-limited to one
`0.01` step.

Thus the supported mechanism is a bounded, smoothly changing pace state:

```text
RaceAbility baseline (+0x44F4)
  -> initial live multiplier (+0x453C)
  -> +/-0.01 approach toward baseline / boost / retreat
  -> corner-speed calculation -> planner pedal commands
```

The remaining uncertainty is the human meaning of the pressure flag and the
full probability calibration, not the update limits or the three targets.

## Observed command chain

In the Alesi follow incident, the target/side decision candidates (`+0x45F8`,
`+0x45FC`, `+0x4420`) did not change.  Meanwhile the planner outputs changed:

```text
waypoint progression -> planner brake/throttle targets -> BrakeFactor/ThrottleFactor
```

This is direct evidence for *following rather than attempting a pass* during
that sampled interval. It does not identify every gate that rejected the
attempt; that is the next mapping target.

## Next controlled proof

Use one isolated development-copy `.RCD` change at a time, then compare a
fresh-session live snapshot (same car/driver) against its preserved baseline:

1. Change only `Passing`; verify `+0x44CC` moves by exactly 0.001 per RCD
   point, then quantify its `AIRacingLine` speed-profile and planner-command
   effect on matching track segments.
2. Change only `RaceAbility`; verify `+0x44D4`, `+0x44D8`, and `+0x44F4`, then correlate
   with waypoint speed/brake targets across matching track segments.
3. Change only `CorneringAdd`; locate its separate live storage/consumer and
   confirm that `+0x44D8` does not move. This turns the above inference into
   controlled proof.
4. Change only `TrackAggression`; observe the rate of viable pass-side/target
   decisions, holding field/track conditions as constant as practical.

Do this only on a disposable development-copy configuration; do not alter the
active clean test installation merely to gather evidence.

## Passing is a racing-line multiplier, not a direct pass-commit switch

The direct downstream reader is now located: `AIRacingLine_00425920` reads
the normalized `Passing` field at `+0x44CC` on its per-frame speed-profile
path. In the raw expression it multiplies a subtraction from a locally
computed force/speed-profile value, which is then zeroed if it falls below a
fixed low cutoff. Thus `Passing` can change the thresholded racing-line result
that feeds the planner's throttle/brake behavior.

This is deliberately not labelled “overtake chance.” `Overtaking_0042AF70`
does not consume this field directly in the mapped path. The supported
interpretation is narrower: RCD Passing affects a per-frame racing-line pace
calculation, while TrackAggression affects candidate traffic geometry and
RaceAbility affects the dynamic corner-speed multiplier. A controlled
single-RCD-field comparison is still required to measure the gameplay size and
direction of Passing's effect.
