# 42 — AI Realism (aggression) and the complete difficulty model: how the two PLR knobs drive AI behavior

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)
**Date:** 2026-07-26 · **Author:** f1c_ida corpus · **Status:** RUNTIME-VERIFIED (pace model exact; aggression chain confirmed, behavioral *outcome* mapping DERIVED)

## Abstract (TLDR)

The player's difficulty options live in the `.PLR` file as two independent knobs,
and they control **completely different things**:

- **AI Driver Strength (70–120, the "slider")** controls *how fast* the AI is.
  It maps linearly onto a global pace scalar whose endpoints come from the track's
  own `.AIW` file (`WorstAdjust` at 70%, `MidAdjust` at 100%, `BestAdjust` at 120%),
  and that scalar multiplies every AI driver's baseline pace. At 92% the scalar is
  exactly `0.85 + (1.0−0.85)×(92−70)/30 = 0.96` (verified live, Monza).
- **AI Realism (0.0–1.0, default 0.5)** is the *aggression* knob. It does **not**
  affect lap times. It is multiplied by each driver's rcd `TrackAggression`
  (default 1.0) into a per-driver aggression scalar, which the AI steering code
  uses every frame to decide: how closely a car dares follow (floor
  `20 − 10·a` meters), how quickly "stuck behind you" frustration builds into an
  overtake attempt (rate ∝ `a` while within `(1+a)×20` m of the car ahead), and how
  quickly that frustration cools when the gap opens.

**Consequence for tuning:** pace and racecraft are orthogonal dials. You can have a
fast-but-polite field (high strength, low realism) or a slow-but-ferocious one.
A calculator that recommends both from the player's lap time is not only possible
but mostly exact — the pace side is a closed formula, the aggression side is a
bounded one-parameter choice. Such a calculator exists (`tools/ai_difficulty_calculator.py`,
usage in §6).

## 1. Introduction

**Questions:** (1) What does the `.PLR` key `AI Realism` actually change in the
game? (2) Given a player's skill level and a track, can we compute the `AI Driver
Strength` and `AI Realism` values that keep races challenging, consistent and
rewarding?

In scope: single-player race behavior. Prior paper #41 established how the rcd
`RaceAbility` stat sets per-driver pace; this paper completes the picture with the
two player-facing knobs.

## 2. Method

Static: full decompilation (~7,100 functions). The PLR option registration table,
the per-driver AI config loader, the AI database initializer, and the per-frame AI
steering function were traced end to end.

Runtime (2026-07-26, live race, Monza, slider 92, realism 0.68780): the realism
float was located in process memory by scanning for its exact PLR bit pattern
(unique hit), all formula fields were read on multiple drivers, and the
slider→scalar interpolation was checked against live values to float precision.

## 3. Where the knobs live

The player's `.PLR` (e.g. `Save\<name>\<name>.PLR`) carries, among others:

| Key | Player value | Role |
|---|---|---|
| `AI Driver Strength` | 92 | pace slider (this paper, §4) |
| `AI Realism` | 0.68780 | aggression scalar (this paper, §5) |
| `AI Power Calibration` | 7 | bitfield: AI power/gearing/fuel adjustments with strength (1+2+4) |
| `AI Brake Power Usage` | 0.98 | physics cap: fraction of brake power AI may use |
| `AI Brake Grip Usage` | 0.97 | physics cap: fraction of brake grip AI may use |
| `AI Corner Grip Usage` | 0.93 | physics cap: fraction of cornering grip AI may use |

The option registration code gives `AI Realism` a default of **0.5** and a hard
range of **[0.0, 1.0]**. The physics-usage caps are constants that bound the AI's
absolute pace ceiling regardless of the slider.

## 4. AI Driver Strength → pace (exact)

### 4.1 The chain

1. At game init, the track's AI database (`.AIW`) provides three anchor values —
   here Monza's: `WorstAdjust=0.85` ("used for 70% AI strength"),
   `MidAdjust=1.0` (100%), `BestAdjust=1.1` (120%).
2. A global difficulty scalar is linearly interpolated from the slider:
   - slider s ≤ 100: `GDS = Worst + (Mid − Worst) × (s − 70)/30`
   - slider s > 100: `GDS = Mid + (Best − Mid) × (s − 100) × 0.05`
   At s=92, Monza: `0.85 + 0.15×0.7333 = 0.96` — **live value read: 0.96. Exact.**
3. Per driver at session init (paper #41, runtime-verified):
   - baseline pace scalar `= (1 − RaceAbility/100) × GDS`
   - ability norm `= (22 − RaceAbility)/22`, multiplied by `(s/100)²` when s < 100
     (quadratic damping), taken unmodified when s ≥ 100; clamped at 1.0.
4. Every frame, each driver's live pace factor walks ±0.01 toward targets built
   from the baseline — so the slider's effect on lap time flows through `GDS`.

### 4.2 Slider → lap time

Over the useful range, lap time is locally inversely proportional to the pace
scalar: `T(s) ≈ T_ref × (GDS_ref / GDS(s))^e`, with track exponent `e ≈ 1.0`
(refinable by two-point calibration, §6). At Monza's adjustments, one slider click
moves GDS by 0.005 — roughly **0.4–0.5 s per click on a ~90 s lap**.

The model's hard bounds: slider is clamped to [70, 120]; above ~100% the AIW
comments note AIs use full throttle, and the PLR physics-usage caps (§3) still
bound the absolute ceiling. A player faster than the 120% ceiling cannot be
matched by the slider alone — raise the caps or the grid's rcd values instead.

## 5. AI Realism → aggression (mechanism confirmed)

### 5.1 The chain

1. The realism float `r` is stored once from the PLR (player: 0.6878).
2. At session init, per driver: `a = r × TrackAggression_rcd` (rcd key
   `TrackAggression`; default 1.0 when absent — most shipped drivers), clamped to a
   minimum of 0.01. Live check: two drivers without the rcd key both read exactly
   **0.6878** in their aggression field. This is the realism global's **only**
   consumer in the entire binary — realism affects nothing else.
3. The per-frame AI steering loop uses `a` on every nearby opponent:

   | Expression | Effect as `a` rises [DERIVED meaning] |
   |---|---|
   | desired gap × `(2 − a)`, floor `20 − 10·a` m | follows closer: 16 m at a=0.4, 12 m at a=0.8 |
   | proximity window `4 − a` | tolerates tighter side-by-side running |
   | pressure `+= a × max(1, closing_rate)` while gap < `(1+a)×20` m | frustration builds faster, from further back |
   | pressure `−= 5` when gap ≥ `(1+a)×20` m | cools off once clear |
   | pressure cap 450 (250 under one flag) | bounded persistence of an attack |

   The per-opponent pressure accumulator is what turns "stuck behind a slower car"
   into a committed overtake attempt: low realism = patient, processional AI;
   high realism = early, repeated dive attempts.

### 5.2 What realism is NOT

- Not a pace knob (no path from `a` to lap time).
- Not a mistake/crash knob — crash probability comes from the rcd `Crash` key.
- Not difficulty-scaled — it applies identically at every slider value.

### 5.3 Per-driver personalization

`TrackAggression` in a driver's rcd scales their personal aggression relative to
the field (e.g. 1.2 → 20% hotter than everyone else at the same realism setting).
An rcd editor can therefore make individual hotheads without touching the global
knob. (One sampled driver showed a pathologically large aggression value consistent
with a third-party randomizer having written an out-of-range rcd stat — see #41 §5.)

## 6. The calculator

**Full walkthrough — inputs, worked math, tuning loop — in paper #43
(`43-ai-difficulty-calculator.md`).** `tools/ai_difficulty_calculator.py`
implements the model:

```
# recommend settings from player pace (un-anchored: assumes you == front AI at 100%)
python tools/ai_difficulty_calculator.py calc --aiw <track>.AIW --player-lap 85.0 --margin-pct 0.5 --preset balanced

# anchored: you measured the front AI lapping 84.2 at slider 92
python tools/ai_difficulty_calculator.py calc --aiw <track>.AIW --player-lap 85.0 --ref-lap 84.2 --ref-slider 92

# one-time per-track refinement from two observed (slider, lap) points
python tools/ai_difficulty_calculator.py calibrate --aiw <track>.AIW --s1 88 --t1 85.8 --s2 96 --t2 84.1
```

- **Slider choice:** solve `GDS(s*) = GDS_ref × (T_ref / T_player)^(1/e)`, clamp to
  [70, 120]; the tool warns when the player is outside the AI's range (then adjust
  the PLR physics caps or grid rcd, not the slider).
- **Realism choice:** presets map to observed aggression geometry — `calm` 0.40
  (16 m follow floor, processional), `balanced` 0.55, `spicy` 0.70 (13 m floor,
  frequent duels), `max` 1.0 (10 m floor, relentless). This side is deliberately a
  *preset*, not a solved value: frustration→overtake conversion is stochastic, so
  "rewarding aggression" is a taste parameter, not a solvable one.
- **Consistency loop:** after each race, nudge the slider by the finishing gap
  (e.g. ±1–2 clicks ≈ ±0.4–1.0 s/lap) — the linear GDS model makes the correction
  size predictable.

**Limits:** pace predictions inherit the §4.2 approximation (exact only after
per-track calibration); aggression presets are geometry-grounded but not
outcome-proven; randomizer-written driver stats can invalidate rcd-based
assumptions for individual drivers (see #41 §5).

## 7. Conclusion

`AI Realism` is the aggression knob and nothing else: a single multiplier baked
per driver at session init, scaling how closely the AI follows, how fast
frustration builds, and how eagerly it commits to overtakes. `AI Driver Strength`
is the pace knob, mapped linearly through track-defined anchors onto a global pace
scalar with a quadratic ability-norm damper below 100%. The two are orthogonal,
both are now formula-complete at the mechanism level, and a per-track
calculator that keeps a player in the challenging window is practical — exact on
pace after a two-lap calibration, preset-based on aggression.

## References

- F1 Challenge 99-02, `F1 Challenge Online.exe` (non-ASLR, base 0x400000), full
  decompilation (private corpus).
- Paper #41 (RaceAbility mechanism), same series.
- Player `.PLR` and track `.AIW` files (game install data).

---
*Changelog: 2026-07-26 — initial; realism chain + slider model runtime-verified
same session (live values quoted inline).*
