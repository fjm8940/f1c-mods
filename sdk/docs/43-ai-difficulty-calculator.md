# 43 — The AI difficulty calculator: matching the field to the player, per track

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)
**Date:** 2026-07-26 · **Author:** f1c_ida corpus · **Status:** RUNTIME-VERIFIED model; calculator is a tool doc (companion to paper #42)

## Abstract (TLDR)

F1C has **no dynamic difficulty**. The AI's pace is fixed at session start by one
number in your player file (`AI Driver Strength`, 70–120) and its racing aggression
by another (`AI Realism`, 0–1). If those numbers don't match your current skill at
the current track, races are either boring (you cruise) or hopeless (you chase).
The calculator (`tools/ai_difficulty_calculator.py`) turns **your lap time** into
the two settings, using the game's own verified formulas, so that:

- the front-running AI is *about your speed* — you can fight for wins but must
  earn them,
- the rest of the field spreads out naturally behind (the game does that itself
  from driver stats),
- the aggression level matches how hard you want to be raced.

You give it a lap time and a track file; it gives you two numbers to set in the
game's options. One optional two-lap measurement makes the pace prediction exact
for that track.

## 1. Why adjust the AI to the player (and not just "set 100% and practice")

The game's AI pace pipeline is (paper #41/#42, verified live):

```
slider ──► GlobalDifficultyScalar (GDS) ──► per-driver baseline pace ──► lap time
              (linear, per-track anchors)      × (1 − RaceAbility/100)
```

Key facts that motivate the calculator:

1. **The slider is static.** It is read from the PLR at init and baked. Nothing in
   the game looks at how your weekend is going and adjusts. Whatever you set is
   what you race, every session, until you change it.
2. **The slider's meaning is track-relative, not absolute.** The same "92" produces
   a different difficulty scalar on different tracks, because each track's AIW file
   supplies the anchors (`WorstAdjust`/`MidAdjust`/`BestAdjust`). "What does 92
   mean?" has no track-independent answer — the calculator resolves it per track.
3. **Your speed changes.** You learn tracks, your setups improve. A fixed slider
   that was challenging in January is trivial by June. The knob exists precisely to
   be re-set; the calculator just makes re-setting it a measurement instead of a
   guessing session.
4. **The rewarding zone is narrow and known.** Racing is most rewarding when the
   fastest AI is *about your pace, usually a touch quicker*: you fight for points
   and wins, you sometimes get beaten, and the midfield behind you is close enough
   to punish mistakes. Because the game spreads the field automatically from driver
   stats, anchoring the *front* of the field to you anchors the whole grid in a
   sensible place.

So the design goal is not "make the AI as fast as possible" — it is **keep the
front AI inside the player's challenge window**, track by track, as the player
evolves.

## 2. What determines the player's base speed value

The calculator's main input is **one lap time: your representative lap at that
track, in seconds.** It is deliberately a *measured* quantity, not something the
tool reads from the game:

| Factor | Why it matters / how to handle it |
|---|---|
| **Player skill** | The dominant term. Use your *typical* race lap, not your personal best — the AI races at its baseline every lap, and you race at your average, not your ceiling. |
| **Race trim** | Measure with race fuel and used-ish tires if you can. A low-fuel qualifying hero lap will make the calculator overshoot the slider by several clicks and the race will be hopeless. |
| **Car/setup** | Included automatically — your lap time *is* your car-plus-you. Change car, re-measure. |
| **Track** | Per-track by construction: you measure at that track, and the tool reads that track's AIW anchors. |
| **Session type** | Use free-practice or race laps, not the simulated qualifying times the game computes for you. |

Rule of thumb: **your representative lap = the lap time you could repeat ten times
in a race stint**, not the one lap you are proud of.

Optionally there is a better anchor: the **front AI's actual lap time** at a known
slider value (read from the results/standings after a session). One such
observation ties the model to reality at that track and removes the
"you ≈ front AI at 100%" default assumption (§3.3).

## 3. How the calculator works, step by step

All formulas below are the game's own, runtime-verified to float precision
(paper #42 §4). Worked example uses Monza (anchors 0.85 / 1.00 / 1.10).

### 3.1 Slider → difficulty scalar (the game's function)

The game maps the slider onto a Global Difficulty Scalar by linear interpolation
between the track's AIW anchors:

```
s ≤ 100:   GDS(s) = Worst + (Mid − Worst) × (s − 70)/30
s > 100:   GDS(s) = Mid  + (Best − Mid) × (s − 100) × 0.05
```

Monza at 92: `0.85 + 0.15 × (22/30) = 0.96` — the exact value read from the running
game. Note the sensitivity: **0.005 GDS per click** on both sides of 100 at Monza
(that is (1.00−0.85)/30 below and (1.10−1.00)×0.05 above). One click ≈ 0.4–0.5 s
on a ~90 s lap.

### 3.2 Difficulty scalar → AI lap time (the model)

Every AI driver's baseline pace scalar is `(1 − RaceAbility/100) × GDS(s)`, and the
per-frame controller walks the live pace factor to it (paper #41). Over the useful
range, lap time responds inversely:

```
T(s) = T_ref × ( GDS(s_ref) / GDS(s) )^e        (e = track exponent, ≈ 1.0)
```

With `e = 1.0` this is exact for a pure speed multiplier and a very good local
approximation in practice. The optional `calibrate` mode fits `e` for a track from
two observations — front-AI lap time at two slider values:

```
e = ln(T2/T1) / ln(GDS1/GDS2)
```

after which predictions at that track are exact within measurement noise.

### 3.3 Solving for the slider

Two modes:

- **Anchored (preferred).** You supply `--ref-lap` (front AI's lap at
  `--ref-slider`). Required scalar:
  `GDS* = GDS(ref) × (T_ref / T_player)^(1/e)`, then inverted through §3.1.
- **Un-anchored.** Assumes your representative lap equals the front AI's lap at
  slider 100, and you want the front AI `margin_pct` quicker than you:
  `GDS* = GDS(100) × (1 + margin)^(1/e)`. A margin of 0.3–0.7% keeps the leaders
  honest without making wins impossible.

The result is clamped to [70, 120]. **Clamping is a signal, not a failure**: if
the tool says you need 125, you are faster than the AI's physics ceiling (the
PLR caps — brake power 0.98, brake grip 0.97, corner grip 0.93 — bound absolute AI
pace). Raise those caps or the grid's rcd stats; don't pretend 120 will be enough.

### 3.4 What the rest of the grid does (you don't tune it)

With the front anchored, the field spreads itself: baseline = (1 − RA/100) × GDS
per driver. For a typical RA spread of 1.8 (champion) to 12 (backmarker) that is a
~10% pace spread baked into the driver files — front group, midfield, tail, all
from one slider. This is why anchoring the front is sufficient.

### 3.5 The realism (aggression) recommendation

Realism is orthogonal to pace (paper #42 §5). The calculator maps it to observable
geometry and offers presets:

| Preset | Value | Follow-gap floor | Duel-pressure range | Character |
|---|---|---|---|---|
| calm | 0.40 | 16 m | 28 m | patient, processional |
| balanced | 0.55 | 14.5 m | 31 m | races you when close |
| spicy | 0.70 | 13 m | 34 m | frequent, early attacks |
| max | 1.00 | 10 m | 40 m | relentless, expect contact |

It is a preset rather than a solved number because frustration→overtake conversion
is stochastic — there is no "correct" aggression, only your taste. Individual
hotheads are better made via per-driver `TrackAggression` in the rcd than by
raising the global knob.

### 3.6 The consistency loop (the actual point)

1. Run a race. Note your representative laps and the finishing gap to the winner.
2. Correct the slider by the gap: **±1–2 clicks ≈ ±0.4–1.0 s/lap** — the linear
   GDS model makes the correction size predictable, so one iteration normally
   lands inside the window.
3. New track → re-anchor (one front-AI observation) and reuse.
4. As you improve, the same loop quietly tracks your skill: the slider becomes a
   *record of your pace* over a season, and the racing stays uniformly hard.

### 3.7 Making the field vary: form jitter (companion tool)

The calculator keeps the field matched to *you*. It does nothing about the other
predictability problem: **the same driver always wins**, because RaceAbility is
baked deterministically and in-game noise is ~10x too small to overturn the gaps
(paper #41 §4: baseline = (1 − RA/100) × GDS, so a 1-point RA gap ≈ 1% pace ≈
0.9 s on a 90 s lap; duel rolls and sim noise move hundredths).

`tools/ai_form_jitter.py` fixes that at the same intervention point the calculator
uses — the rcd file the game reads fresh at every session init:

```
form[d] = 0.5 × form[d] + σ × N(0,1)      (weekly form, momentum = streaks)
RA_eff  = clamp(RA_base + form + event, 0.2, 20)
QA_eff  = QA_base + independent noise      (qualifying order ≠ race order)
event   = 6% chance of ±3.5 "brilliant/awful weekend"
```

With σ = 2.0 (default), the top 5–6 drivers rotate through winning form across a
season while backmarkers (10+ RA behind) stay back — the hierarchy wobbles instead
of shattering. σ ≈ half the RA gap you want to scramble is the sizing rule.
First run stores byte-exact originals; `restore` undoes everything. Run it between
races (the game can stay open — values apply to the next session load).

```
python tools/ai_form_jitter.py jitter --drivers-dir <install>\SeasonData\Drivers\2001 --dry-run
python tools/ai_form_jitter.py restore
```

Combined loop: **jitter sets who's hot this weekend; the calculator sets how hot
the whole field is relative to you; realism sets how hard they race you.**

### 3.8 Live form: changing AI pace mid-session (poke-proven)

The form jitter (§3.7) works *between* sessions. A second lever works *during* one,
because of a verified implementation detail (paper #41 §4.2): the baseline pace
scalar `0x44F4` is read every frame as the pace target but never rewritten after
the session-init bake. A mid-race write therefore **persists**, and the live pace
factor walks to the new value at the game's own ±0.01 per frame — the transition
takes seconds and looks completely natural.

**Proof (2026-07-26, live race):** a midfield driver's baseline was written from
0.88128 to 0.85128 (−0.03, ≈ +2.7 s/lap); his pace factor walked
0.88128 → 0.87128 → 0.86128 → 0.85128 over ~8 s with no other intervention, and
the write was still in force afterward.

`tools/ai_live_form.py` wraps this:

```
python tools/ai_live_form.py list                 # all AI cars: baseline + live pace
python tools/ai_live_form.py set "De La Rosa" 1.01   # hero day: ~1% quicker
python tools/ai_live_form.py fade Hakkinen 0.98 --minutes 8  # slow fade, tire-trouble arc
python tools/ai_live_form.py reset                # restore everyone's original baseline
```

Factors are relative to each driver's **original** session baseline (captured on
first touch), so repeated calls never compound; safety clamp [0.85, 1.08]; the
player's car is refused. Scale guide: 1% baseline ≈ 1% lap time ≈ 0.9 s at a
90-second track.

Use cases the file-based jitter can't do: in-race narratives (leader fades with
"tire trouble" over a stint, underdog charges late), correcting one driver who
baked wrong (see the session-scoped effective-RA anomaly in paper #41 §5 — a live
`set` fixes a corrupted bake without restarting), and testing form sizes before
committing them to the rcd pipeline.

**Limit:** same-session only — session reload rebakes from the rcd and the tool's
captured originals go stale (`clear` then re-touch).

## 4. Usage

```
# un-anchored: player ≈ front AI at 100%, want leaders 0.5% quicker
python tools/ai_difficulty_calculator.py calc \
    --aiw ".../2001_Monza.AIW" --player-lap 85.0 --margin-pct 0.5 --preset balanced

# anchored: front AI lapped 84.2 at your current slider 92
python tools/ai_difficulty_calculator.py calc \
    --aiw ".../2001_Monza.AIW" --player-lap 85.0 --ref-lap 84.2 --ref-slider 92

# one-time per-track exponent fit (then predictions are exact for that track)
python tools/ai_difficulty_calculator.py calibrate \
    --aiw ".../2001_Monza.AIW" --s1 88 --t1 85.8 --s2 96 --t2 84.1
```

Then set the two numbers in the game options (or the PLR) and race.

## 5. Limits (read before trusting it)

- Pace math inherits the §3.2 approximation until you calibrate the track.
- Representative-lap choice is on you: race-trim average, or the tool lies.
- Aggression presets are mechanism-grounded but not outcome-guaranteed.
- Mods that rewrite driver stats (randomizers) can make individual drivers ignore
  rcd-based assumptions — the *system* still behaves, single drivers may not. If a
  randomizer already manages your rcd files, run either it or `ai_form_jitter.py`,
  not both on the same drivers (they will overwrite each other's edits; the
  jitter backup captures whatever state existed at first run).
- The slider cannot exceed the AI's physics ceiling; the tool says so when you hit it.

## References

- Papers #41 (RaceAbility) and #42 (difficulty/realism model), same series.
- F1 Challenge 99-02, `F1 Challenge Online.exe`, full decompilation (private corpus).

---
*Changelog: 2026-07-26 — initial; added §3.7 form jitter (AI result variation).*
