# AI lap-time recipe — what makes an AI lap time (2026-10-15)

Synthesis by Claude of Haiku reports H1-H17 (this folder, `haiku_H*_2026-10-15.md`), each reviewed
against the decompiled code before use, plus live runs 3-6 (development/mods/adaptive_ai_difficulty/docs/
baseline-2001-equal-ra-2026-10-14/README.md). Tiers: FACT (code shows it / live-proven), CONFIRMED (two
sources), DERIVED (inference), OPEN. Each item has a finding id in `f1c_ida_decompiled/findings.jsonl`.

## 1. The tree

```
AI lap time = time along the AIW path (20-waypoint look-ahead) of speed v(s)
 v(s) = min(corner target, braking projection, engine/drag physics)                  [H15, DERIVED]

 corner target (AIComputePassScore 0x42A780)                                          [FACT code]
   = (S x CorneringMult 0x4504 + CorneringAdd 0x4500) x wet 0x4580 x pace 0x453C x wp_event[0]
   S = TireForceSolver 0x4E8CE0(car, wp+0x74, wp+0x78, wp+0x70, step 16): speed solve against a
       per-car 14-entry grip-vs-speed table, cap 128 m/s                                 [DERIVED]
       demand per waypoint (LIVE FACT 17:06 UTC): wp+0x70 = curvature 1/r, wp+0x78/0x7C = gravity
       components (slope/banking); constructor defaults are overwritten at runtime (writer OPEN)
       per-car table = tyre normal-load capacity (N), rebuilt by TireBrushModelForceSolver each call;
       solver returns m/s (H18)
   grip per wheel = G_k x (1 + k x min(load, 13300)), k = -0.605/13300 (2001 tyres)     [H13, DERIVED]

 braking: v -= seglen/v x Passing 0x44CC (~0.95) x TireForce x 0.915                    [H15, DERIVED]
 straights: engine torque curve (engine.ini RPMTorque), gearing, drag; no AI speed cap  [H15/H16]

 pace 0x453C (live) walks every 4 s in +-0.01 steps toward:                             [H8, DERIVED]
   race:      baseline 0x44F4 = (1 - RA/100) x GDS (random walk, traffic gate)          [CONFIRMED]
   non-race:  0x44F4 + push 0x44D8 = 0.67 x (0.04 + 0.01 x GDS x RA), +0.003/lap ramp   [H9, DERIVED]

 GDS (strength scalar, 0x5EA3D8) = per-track AIW interpolation by slider 70..120:      [CONFIRMED]
   70 -> WorstAdjust, 100 -> MidAdjust, 120 -> BestAdjust (defaults 0.5 / 1.0 / 1.3)
 physics strength at session init (AIDriverStructInit -> sub_4F1510, ratio new/old):    [DERIVED]
   value = same interpolation x 0.995 (skipped in sessions 3/4/5)
   scales: 4 per-wheel terms (always), torque (bit 1), gear ratios (bit 2), fuel (bit 4)
   (PLR AI Power Calibration = 7 = all); player car probably scaled too (OPEN)
 car files: Minardi vs Ferrari 2001 = -12.7 % peak power, 6 vs 7 gears, +3 % drag;      [H16]
   no AI-only overrides
 AIW: no speed/brake targets; MidAdjust etc. = strength curve; RaceQualRatio unread;   [H1/H3/H6]
   times= not parsed
 setups: <track>.qual|race.svm -> <track>.svm -> ai.svm -> gdb; qual vs race differ in  [H4]
   fuel/stops/tyre only (many 2001 race.svm renamed .bak in this install)
 not ingredients: PLR AI grip/brake usage keys (no reader, run 4 null)                  [CONFIRMED]
 incidents: Crash/100/TotalRaceLaps per roll, no pace term                              [H11, DERIVED]
```

## 2. Measured strengths (Austria 2001, strength 100, RA 1.0)

| Lever | Effect | Source |
|---|---|---|
| Pace 0x44F4 | about 0.22 s/lap per 1 % (about 0.3 % lap time) | Run 3, Run 6 |
| 1 RA point | about 1 % pace, so about 0.22 s/lap | derived from the above |
| CorneringMult | x1.03 -0.59, x1.06 -0.72, x1.10 -0.55 (saturates), x0.97 +0.80 | Run 6 |
| PLR AI grip/brake | none (keys unread) | Run 4 + H14 |
| Running 2026-10-15 | pace x1.05/x1.08, x0.98-x0.92, CorneringAdd, other tracks, rain, quali | aad_runner |

## 3. What this means for the adaptive AI

- Main lever: strength (GDS). Before a session: set the slider (70-120) for the full native effect
  (pace + physics). During a race: pace baseline 0x44F4 (settles in about 20 s), plus possibly a live
  sub_4F1510 call from a DLL (untested; may be reset at AIDriverStructInit events).
- Per-track difficulty comes from AIW Worst/Mid/BestAdjust: the mod can correct it per track.
- Qualifying AI are deliberately faster (push bonus, no x0.995, low fuel): rate the player's qualifying
  laps against AI qualifying pace, race laps against race pace.
- No native rubber-banding to fight. Incident rate does not depend on pace.
- Absolute lap times per car x track still need one AI-only calibration race (the tyre grip table
  builder and the corner-demand input are not recovered).

## 4. Open items (ranked)

1. Live read of AIW waypoint records +0x70..+0x84 (does anything write them at runtime?).
2. Builder of the per-car grip-vs-speed table (TireForceSolver this[3872..]).
3. Live test: DLL call of sub_4F1510 mid-race; does the player car get scaled; when it is re-applied.
4. Live: effective AI qualifying pace (qualifying instance of 2026-10-15).
5. TireForce units; braking decel factor (asm check).
