# 41 — RaceAbility: how the rcd driver stat drives AI pace in F1 Challenge 99-02

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)
**Date:** 2026-07-26 · **Author:** f1c_ida corpus · **Status:** RUNTIME-VERIFIED (formulas of §4.1 confirmed live, incl. quadratic slider damping; new finding: 0x44C4 clamps at 1.0)

## Abstract (TLDR)

**What RaceAbility actually does in the game:**

- When a session loads, the game reads each driver's `Raceability` number from their
  driver file **once**. From that single number it pre-computes five things: the
  driver's normalized ability, their baseline race pace, how much extra pace they
  can find when "pushing", how likely they are to win a wheel-to-wheel fight, and
  (via the ability norm) how quickly they adapt their braking and grip.
- During the race, every AI driver's pace is constantly nudged toward their personal
  target — a small step every frame. RaceAbility is what sets where that target sits.
  **A high RaceAbility driver is not just "faster" in one way — they settle at a
  better pace, push harder when challenged, and are less likely to back off.**
- When two AI cars fight, the game literally rolls dice: the attacker's RaceAbility
  is weighed against the defender's, and the winner of the roll either pushes harder
  or backs off. Strong drivers win these rolls more often.
- RaceAbility also controls how the AI's laptimes are spread out in sessions the game
  simulates (e.g. when the game computes AI results) — it's the main thing separating
  the front of the grid from the back.
- **The difficulty slider interacts with it:** below 100%, every AI driver's ability
  is damped, and the damping is *quadratic* — lowering the slider a little already
  weakens the field a lot, and it compresses the differences between drivers. At 100%
  you see the true spread written in the driver files.
- Wet races are a partial exception: in the wet, a different driver stat
  (start/weather performance) partly takes over the car-control side.

**Practical takeaways:**

- Editing `Raceability` in a driver's `.rcd` file works and is the clean way to make
  a driver faster or slower — no tools needed, the game reads it fresh each session.
- One quirk for rcd editors: the game uses the raw number on **two different scales**
  in the same formula (it divides it by 100 in one place and by 22 in another), so
  the exact intended range is **[unverified]** — treat the shipped drivers' values as
  the reference and stay within their spread.
- Memory-hacking a driver's speed live does not stick: the pace factor is recalculated
  every frame from the baked-in RaceAbility values. The `.rcd` file (or the baked
  values right after load) is the correct place to intervene.

## 1. Introduction

**Question:** what does the `Raceability` value in a driver's `.rcd` file actually
control in-game, through what mechanism, and where does the difficulty slider come in?

In scope: single-player AI behavior, race and simulated sessions. Out of scope:
multiplayer, physics tire model, other rcd keys except where they interact.

Motivation: rcd editing is the oldest F1C modding practice, but the community
documentation of what `Raceability` does is folklore. This paper traces the value
from file to on-track behavior.

## 2. Method: static analysis

Full decompilation of the game binary (Hex-Rays), ~7,100 functions. The driver-config
loader is one function that runs once per AI driver at session init; it parses the
driver's keyed rcd data (including the literal key string `"Raceability"`) and writes
a block of computed floats into that driver's vehicle/AI state structure. Four
consumer functions were then identified by tracing reads of those computed fields:

- a per-frame "AI ability" updater (runs every frame during a race),
- an overtake-decision function,
- the AI racing-line/braking updater,
- a session-simulation chain that assigns AI laptimes in simulated sessions.

All formulas in §4 are quoted directly from the decompiled code (FACT tier for the
arithmetic; the *meaning* labels are DERIVED).

## 3. Method: runtime verification

**DONE 2026-07-26.** Live race, Monza lap 53, 22 cars, difficulty slider 92 (read
from the game's own difficulty global). A memory reader sampled the three key
per-driver fields on 10 drivers mid-race, and the live pace factor `0x453C` was
watched continuously (4 Hz) on the two primary drivers during green-flag running.

Raw evidence (rcd = the driver-file value; predicted = §4.1 formulas with slider 92,
i.e. damping factor 0.8464, and the measured global scalar 0.96):

| Driver | RA (rcd) | `0x44C4` measured | predicted | `0x44F4` measured | predicted |
|---|---|---|---|---|---|
| M. Schumacher | 1.8 | 0.777149 | 0.777151 | 0.94272 | 0.94272 |
| Hakkinen | 2.8 | 0.738676 | 0.738622 | — | — |
| R. Schumacher | 4.0 | 0.692509 | 0.692423 | — | — |
| Frentzen | 5.3 | 0.642494 | 0.642346 | — | — |
| Irvine | 6.2 | 0.607869 | 0.607815 | — | — |
| Salo | 8.2 | 0.530924 | 0.530831 | — | — |
| De La Rosa | 10.6 | 0.438589 | 0.438554 | 0.85824 | 0.85824 |
| Montoya | 10.38 | 0.447053 | 0.446892 | — | — |

All residuals < 0.001. Two further drivers (Webber, Alonso) matched the formulas
only with an *effective* RA that differs from their driver file — see §5. Alonso's
extreme effective RA exposed a clamp: his `0x44C4` reads exactly 1.0 while his
`0x44F4` runs unclamped at 1.7504.

Dynamics: Schumacher's `0x453C` settled exactly on his `0x44F4` baseline (0.94272)
in normal running — one −0.01 step (0.95272 → 0.94272) was caught on camera, the
predicted per-frame walk. De La Rosa's sat stable at 0.953219, which is exactly his
predicted attack target `baseline + (1 − baseline) × 0.67` = 0.85824 + 0.094979.

## 4. Results

### 4.1 Load path (once per driver, at session init)

The loader reads the rcd key `"Raceability"` (missing → default raw **3.0**) and
bakes five values into the driver's state (offsets relative to the vehicle struct):

| Offset | Formula | Meaning [DERIVED] |
|---|---|---|
| `0x44C4` | `(22 − RA) / 22`; if difficulty slider <100: `× (slider/100)²`; **clamped to ≤ 1.0** [CONFIRMED live, 8 drivers, slider 92 → exact factor 0.8464] | normalized race ability — difficulty-quadratic scaled |
| `0x44F4` | `(1 − RA/100) × GlobalDifficultyScalar` [CONFIRMED live, exact to float precision; global = 0.96 at slider 92] | baseline ability scalar |
| `0x44D8` | `(1 − baseline) × 0.67` [CONFIRMED live: De La Rosa's pace factor sat exactly at baseline+boost] | "push" boost addend |
| `0x44D4` | `(22 − RA×0.67)/22` | 2/3-weighted norm, used in the duel roll |
| `0x44C0` | `(22 − QualifyingAbility)/22` [CONFIRMED-by-absence 2026-07-26: **no on-track consumer exists** — watched qualifying is paced by the race fields, QualifyingAbility only feeds simulated ("Finish Session") results] | qualifying twin (simulated sessions only) |

Runtime additions (2026-07-26): the 1.0 clamp on `0x44C4` was found live — a driver
with effective RA ≈ −82 in memory shows `0x44C4` = 1.0 exactly while `0x44F4` runs
unclamped at 1.7504. The live pace factor `0x453C` was observed settling exactly on
the `0x44F4` baseline (strong driver, normal running) and exactly on
baseline+boost (weak driver, pushing) — both steady-state targets of §4.2 confirmed.

Sibling rcd keys parsed by the same loader (context for rcd editors): StartsDry /
StartsWet → start performance; StartStalls → stall probability; RainAbility → wet
grip adjust; Passing → scaled to [0.9, 1.0]; Crash → crash probability;
TrackAggression → scaled by a config float, clamped ≥0.01; CorneringAdd /
CorneringMult → cornering tweaks; CompletedLaps → expected race distance.

### 4.2 Per-frame pace control (race)

Every frame, each AI driver's live pace factor (init 1.0) is walked **±0.01 per
frame** toward a target computed from the baked values:

- normal running → target = the baseline scalar (`0x44F4`): **RA sets steady-state pace**;
- "attack" flag set → target = baseline + boost (`0x44F4 + 0x44D8`);
- opponent engaged → dice roll weighted by attacker's duel norm vs the strongest
  opponent's ability: win → push toward the boosted target; lose → back off toward
  **half** the baseline;
- pit-exit phase → forced to 85% of baseline.

Because the pace factor is rewritten every frame, externally poking *it* does not
persist. The correct intervention points are the baked fields or the rcd file —
and runtime verification sharpened this further (2026-07-26): the baseline
`0x44F4` is **read** every frame but never *rewritten* during a session, so a
mid-race write to `0x44F4` **persists** and the pace factor walks to it at the
game's own ±0.01/frame (proven live: a −0.03 baseline write on a midfield car was
adopted over ~8 s, on-screen indistinguishable from natural pace loss). This makes
`0x44F4` a live "form" knob mid-session; `0x453C` remains write-useless.

### 4.3 Overtaking

The overtake-decision function records the strongest opponent ability encountered so
far into a per-driver slot; that value feeds the duel roll above. Net effect:
duels against stronger opponents are statistically harder for weak-RA drivers.

### 4.4 Braking / grip adaptation

In the dry, the AI's brake/grip blend target is approached in steps capped at
**0.2 × ability norm** — higher-RA drivers converge on the correct braking/grip
application faster. In the wet, the cap uses the start-performance field instead.

### 4.5 Simulated sessions

For sessions the game simulates, it finds the field's best and worst laptimes,
interpolates a per-car target by ability norm, then adds per-car random noise whose
magnitude scales with `(1 − norm + random)`, clamped to [0,1]. Race sessions use the
race ability norm; other sessions use the qualifying norm. **This is the main
mechanism that spreads the grid apart in computed results.**

### 4.6 Difficulty slider interaction

- Slider < 100: the ability norm is multiplied by `(slider/100)²` — quadratic damping
  (95% slider ≈ 0.90×, 80% ≈ 0.64×, 70% ≈ 0.49× ability). **CONFIRMED live
  2026-07-26:** at slider 92 the measured damping across 8 drivers is exactly
  0.8464 = (92/100)², residual < 0.001 per driver.
- Slider < 95: an additional global scalar ramps from 0.75 to 1.0.
- Slider > 100: a separate high-range linear extrapolation takes over.

## 5. Discussion

- **Scale ambiguity (resolved 2026-07-26):** the sign convention is confirmed —
  lower raw RA = faster (RA 1.8 → norm 0.777; RA 10.6 → norm 0.439 at slider 92) —
  and both the 22-scale and the 100-scale formulas reproduce live values exactly.
  Shipped 2001 spread observed: 1.8 (M. Schumacher) to 12.2+ (backmarkers).
- **Effective RA ≠ loose rcd in some installs (narrowed 2026-07-26):** 2 of 10
  sampled drivers (Webber, Alonso) ran with effective RA values that match neither
  their loose `SeasonData\Drivers\2001\*.rcd` nor any randomizer dry-run manifest
  on disk (Alonso's implied ≈ −82, deep negative). All formulas still hold with
  the effective value, confirming a single baked RA source per driver. Follow-up
  observation same day: in the *next* session Alonso baked completely normally
  (baseline 0.84288 = file RA 12.2 exactly) — so the corruption is
  **session-scoped, not persistent**: whatever writes it does so per session
  load, inconsistently. Where the divergence is written remains unknown, but it
  is not a one-time baked-in error.
- Wet behavior partially bypasses ability for car control (uses start performance) —
  wet-weather rcd editing should target RainAbility/starts, not RaceAbility.
- The per-frame ±0.01 walk means ability changes manifest smoothly, not instantly —
  a duel outcome takes seconds to play out.
- Because the bake is deterministic, the lowest-RA driver wins the AI pecking order
  every session; in-game noise is ~10x too small to overturn RA gaps. Result
  variation must be injected at the rcd before the bake — the corpus ships a
  form-jitter tool that does exactly this (paper #43 §3.7).

## 6. Conclusion

`Raceability` is the master AI-pace stat: it sets steady-state pace, push reserve,
duel odds, braking adaptation rate, and simulated-session laptime spread — read once
from the rcd, baked into per-driver state, and enforced every frame. Editing the rcd
is the clean, persistent way to change a driver; live memory edits are not. The
difficulty slider damps the whole system quadratically below 100%.

## Appendix A — Corrections to prior notes/tools

- Internal SDK header: an accessor macro for "RaceAbility_scaled" used byte offset
  `0x1131` — wrong (a dword index misread as a byte offset). True offset: `0x44C4`.
- Internal analysis notes mislabeled three loader fields (start-stall vs rain,
  passing vs aggression). Corrected in §4.1.
- Internal notes on the session-simulation function had the session check inverted;
  race sessions use the race ability norm, others the qualifying norm.

## References

- F1 Challenge 99-02, `F1 Challenge Online.exe` (non-ASLR, base 0x400000), full
  decompilation (private corpus).
- Community rcd file format documentation (folklore tier — used for key names only).

---
*Changelog: 2026-07-26 — initial (static only); same-day runtime verification: §4.1
formulas CONFIRMED live (8-driver sample, slider 92, quadratic damping exact at
0.8464), 1.0 clamp on `0x44C4` discovered, §4.2 steady-state targets observed live,
effective-RA anomaly noted in §5 (later narrowed to session-scoped). Follow-ups same
day: `0x44C0`/QualifyingAbility CONFIRMED-by-absence as simulated-sessions-only;
`0x44F4` POKE-PROVEN as a persistent mid-session form knob (see paper #43 §3.8).*
