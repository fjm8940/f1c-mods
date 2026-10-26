# H12 - Prior art for predicting AI lap times and AI difficulty - 2026-10-15

Scope: desk review only. Read-only except this file. No game process, poke, memory reader or runtime tool was used. Evidence tiers follow the corpus: FACT / CONFIRMED (code or live), MEASURED / OBSERVED (one run or session), DERIVED (inference), OPEN.

Source keys: D41 = docs/41-raceability-mechanism.md; D42 = docs/42-ai-realism-and-difficulty-model.md; D43 = docs/43-ai-difficulty-calculator.md; BR = adaptive_ai_difficulty/docs/baseline-2001-equal-ra-2026-10-14/README.md; H = HANDOFF_2026-10-15.md; RV = career_mod_platform/research/raceability-review-2026-09-07/README.md; TR = career_mod_platform/verification/live-qualifying-trace-2026-09-03/README.md.

## 0. Bottom line

- The pace mechanism is well mapped: RaceAbility (RA) and the slider feed a baked baseline (0x44F4) and push target, and those formulas are live-confirmed.
- The lap-time conversion is not: measured sensitivity is about 0.15-0.22 s per 1 % pace, roughly a quarter of the 0.8-1.0 s per 1 % implied by D42/D43.
- Several premises behind the runs are contradicted or unverified: strength 100 is treated as neutral, the pace update rate differs between sources, and the RA values in the 2026-10-14 backups do not match D41.
- No accurate predictor exists yet. It needs absolute clean-air track base times, tyre and fuel terms, a per-track session offset, and at least one replicated sweep per track.

## 1. Quantitative relations already measured or derived

| # | Relation | Value | Conditions | Tier | Source |
|---|---|---|---|---|---|
| 1 | Slider -> GDS: GDS(s) = Worst + (Mid-Worst)(s-70)/30 for s<=100; Mid + (Best-Mid)(s-100)*0.05 above | Monza 92 -> 0.96 | Monza AIW 0.85/1.00/1.10 | CONFIRMED (live exact) | D42 4.1, D43 3.1 |
| 2 | Baseline 0x44F4 = (1-RA/100) x GDS | 0.94272 (RA 1.8); 0.85824 (RA 10.6) | slider 92 | CONFIRMED (live) | D41 4.1 |
| 3 | Norm 0x44C4 = (22-RA)/22 x (s/100)^2, clamped at 1.0 | damping 0.8464 exact at 92 | s<100 | CONFIRMED (8 drivers) | D41 4.1, 4.6 |
| 4 | Push target = B + 0.67(1-B) | 0.85824 -> 0.953219 | De La Rosa, pushing | CONFIRMED (live) | D41 3 |
| 5 | Pace walk per step | +/-0.01 per step; a -0.03 write was adopted over about 8 s | race | CONFIRMED (live), rate conflicts (sec 2.2) | D41 4.2, D43 3.8 |
| 6 | RA 1.0 and strength 100 -> B = 0.99 on every AI | 0.99000 exact | Austria 2001 | CONFIRMED (live) | BR run 3 |
| 7 | Global pace lever (x k on 0x44F4) | x0.97 / x1.03 -> about 0.15-0.2 s per 1 % | Austria 2001, 18 laps, 2x fuel | MEASURED (one run) | BR run 3 |
| 8 | Same lever, dose response | x1.02 -> -0.44 s (SE 0.07), about 0.22 s per 1 % | 100 % race, normal fuel, 839 laps, 21 cars | MEASURED (one run) | BR run 6, H |
| 9 | CorneringMult (veh+0x4504) | x1.03 -0.59 (SE 0.07); x1.06 -0.72; x1.10 -0.55; x0.97 +0.80 | run 6 | MEASURED (saturates above 1.03) | BR run 6 |
| 10 | CorneringMult x1.03 replicate | -0.46 s (SE 0.06, laps 3-17) | 17-lap race, 2x fuel | MEASURED | BR run 5 |
| 11 | PLR AI brake/grip caps 0.98/0.97/0.93 -> 1.00 | +0.10 to -0.21 s; noise +/-0.4 s; no effect | Austria 2001 | MEASURED (null) | BR run 4 |
| 12 | Traffic penalty per s of gap under 1.5 s | +0.71 s (run 6); +1.01 s (run 2); 0.8-1.4 (earlier) | free-air filter | MEASURED | BR runs 2, 6 |
| 13 | Fuel/stint slope | -0.054 (run 6); -0.09 (run 5, laps 3-17); -0.15 (run 5, laps 2-17); about -0.12 to -0.16 (run 2) s/lap | differs by run | MEASURED, unstable | BR runs 2, 5, 6 |
| 14 | Car tiers, equal drivers (least squares, 96 laps) | Williams 69.18 +/-0.27 ... Minardi 71.93 +/-0.30; spread about 2.7 s | Austria 2001, RA 1.0 | MEASURED (n 1-8 per team) | BR run 2 |
| 15 | Lap residual sd per AI lap | 1.03 s (run 2); 1.39 s (run 4) | regressions | MEASURED | BR runs 2, 4 |
| 16 | AIW MidAdjust / RaceQualRatio | Austria 1.000 / 0.988; Nurburgring 2002 0.960 / 0.985; shipped MidAdjust 0.935-0.99+ | - | FACT (file); consumer OPEN | BR per-track, H |
| 17 | GDB simulated lap time | T = base x (1 + 0.15(1 - norm)); floor 97 % of fastest | Finish Session / simulated only | CONFIRMED (static) | BR; finding gdb-laptime-keys-simulated-only |
| 18 | Qualifying vs race, RA 0 -> 1 matched | +0.093 s qualifying mean; +0.229 s race best-lap mean (9 drivers) | 2002 Hockenheim; QA also changed | OBSERVED, not calibrated | RV (sec. "Direct" and static note) |
| 19 | Equal-RA qualifying spread | 23 AI span 5.541 s; 11 teammate pairs 1.322 s mean gap (0.416-2.061) | 2002 Hockenheim, RA 1.0 | OBSERVED (one session) | RV |
| 20 | Live pace vs best lap | r about -0.58 (13 complete laps); push group 0.98876 vs low group 0.96819 | watched qualifying 2026-09-03 | OBSERVED, not causal | TR |
| 21 | Traffic and cap during a lap | Montoya 102.1 s lap: 52 latch and 71 cap samples | same | OBSERVED | TR |
| 22 | Wet effect on corner target | wet targets 75 % of dry; lap +20 %; factor max(0.5, 1 - wetness x RainAbilityNorm) | wet Imola | OBSERVED / DERIVED | finding ai-brain-wet-corner-braking-live-2026-10-02; RV |
| 23 | Session IDs | 1 practice, 5 qualifying, 6 warm-up, 7 race | - | FACT / CONFIRMED | finding session-type-ladder-1-7-live-20261013 |
| 24 | Strength -> physics at init | v = curve(s) x 0.995 rescales tyre per-wheel floats, torque, gears and fuel use | session init only | DERIVED (static), unmeasured | finding aad-strength-scales-ai-physics-2026-10-15; H2 |
| 25 | Slider step, Monza | 0.005 GDS = 0.52 % pace -> stated 0.4-0.5 s per click | Monza | DERIVED (from D42/D43) | D42 4.2, D43 3.1 |

## 2. Contradictions between sources

1. **Lap seconds per 1 % pace.** D42 4.2 says 0.4-0.5 s per slider click, i.e. 0.8-1.0 s per 1 % at Monza (0.005 GDS = 0.52 %). D43 3.8 says 1 % = 0.9 s. H and BR run 3 describe the paper assumption as about 0.7 s per 1 %. Measured: 0.15-0.22 s per 1 % (BR runs 3, 6). Lap-time exponent e (lap % per pace %) is about 0.25-0.3, not the assumed 1.0. Unresolved.
2. **Pace-walk rate.** D41 4.2 and D43 3.8 say +/-0.01 every frame; the adaptive spec agrees. TR says 0.01 transition steps. Findings ai-carconditions-1-pace-085 and ai-pace-nonrace-ramp-only and the ai_brain_lab MANUAL say AIAbility updates every 4 s. RV says four-session-second gated, planner nominally 25 ms. The rate changes any convergence estimate. Unresolved.
3. **Strength 100 as neutral.** BR runs 2-6 and several findings assume strength 100 gives GDS = MidAdjust and AIDifficultyScalar = 1.0. H2 (static) says the slider curve is multiplied by 0.995 and used to rescale physics arrays at init (finding aad-strength-scales-ai-physics), so strength 100 is not neutral unless the stored value equals the scaled curve. H2 also gives the default slider as 75; D41-D43 do not state a default. Unmeasured.
4. **RA values in the files.** D41 section 3 table: M. Schumacher 1.8, De La Rosa 10.6, Montoya 10.38, Webber about 7.3 effective (file 11.3), Alonso effective about -82 (file 12.2); D41 section 5 gives spread "1.8 to 12.2+". The 2026-10-14 original_rcd backups (BR) show RA 0.51 (Schumacher, Barrichello) to 3.89 (Alonso), spread 0.51-3.89. Either the install changed (randomizer) or D41 read a different state. BR notes the originals may be randomizer values. The RA-to-pace range therefore is about 10 % (D43 3.4) or 3.4 % (BR).
5. **RA 1 % claim.** D41 and D43 say 1 RA point = 1 % pace = 0.9 s on a 90 s lap. RV (practical interpretation) says a per-RA seconds coefficient is not established and warns against that conversion. Treat 0.9 s as unverified.
6. **Throttle cap 0.75 (latch).** The adaptive spec (sections 1 and 8) and the ai_brain_lab MANUAL ("the game never restores the cap") describe a vanilla latch bug. BR (2026-10-14) marks the latch finding DISPROVEN: it is native local yellow. The spec was not updated and still lists the latch as CONFIRMED.
7. **MidAdjust consumer.** D42/D43 say the AIW anchors drive GDS. H1 (haiku_H1_aiw_adjust) found no runtime reader for MidAdjust/BestAdjust beyond an init-time formula, and no reader for RaceQualRatio. OPEN.
8. **Fuel per lap.** Estimates range from 0.054 to 0.16 s/lap across runs (row 13). Runs 2-5 used double fuel and run 6 normal fuel. Not reconciled.

## 3. Model sketch

predicted AI lap = T_base(track) x car(car file, team) x pace(RA, slider, track) x corner(CorneringMult) x wet(RainAbility, wetness) + traffic(gap) + fuel(load) + tyre(compound, wear) + session(qual or race, RaceQualRatio) + random (sd about 1.0-1.4 s per lap)

| Term | State | Notes |
|---|---|---|
| T_base(track) | MISSING | No absolute clean-air base. Austria 2001 free-air medians 69.5-72 s. Nurburgring 2002 owner report 100-108 s (not comparable without scenario check). |
| car(team) | MEASURED (relative) | Tier spread about 2.7 s (row 14). Physics path known (tyre force, aero, SVM). No predictor. |
| RA -> B = (1-RA/100) x GDS | KNOWN formula; lever MEASURED | Lap sensitivity 0.15-0.22 s per 1 %; exponent about 0.3. Per-driver RA values disputed (sec 2.4). |
| slider -> GDS | KNOWN formula | Track anchors known for 2 tracks. |
| strength physics rescale (0.995; tyre, torque, gear, fuel at init) | DERIVED, UNMEASURED | Not in any model; may explain part of the gap between the lever and the formula. |
| CorneringMult | MEASURED | Asymmetric; saturates above x1.03 (about -0.6 s). |
| PLR grip/brake caps | MEASURED null at Austria | Needs other tracks or conditions. |
| traffic (gap under 1.5 s) | MEASURED | +0.7 to +1.0 s per s of gap; latch and cap stochastic. |
| fuel | MEASURED, unstable | -0.05 to -0.16 s/lap. Effect of AI Additional Fuel Mult (0.95 default) not measured. |
| tyre wear and compound | MISSING | Not logged in any run. |
| weather (wet) | KNOWN formula; magnitude OBSERVED | Wet Imola only. RainAbility effect unquantified. |
| session (qual vs race) | MISSING magnitude | RaceQualRatio 0.985-1.0 (no reader). AI svm qual/race differ in fuel and pit flags (DERIVED). |
| simulated (Finish Session) | KNOWN formula | Not driven pace; must not be mixed with driven laps. |
| random | MEASURED | Sd 1.0-1.4 s per lap; LCG branches. Per-driver means need about 47-86 clean laps for +/-0.3 s at 2 SE. |

## 4. Minimal live measurements still needed for +/-0.3 s

Sample-size rule: with per-lap sd 1.0-1.4 s, +/-0.3 s at 2 SE needs about 47-86 clean laps per condition, pooled across cars. Each item is one bounded experiment. Writes follow the existing poke and live-form rules and restore originals.

1. Pace-update rate: trace 0x453C steps over 60 s to settle 4 s vs per-frame (cheap, one run).
2. Strength physics isolation: same track and RA, strength 100 vs 90 with 0x44F4 held equal by write, two session loads. Separates the init-time physics rescale from the pace lever. About 40 clean laps per arm.
3. Dose response on a second track (for example Monza or Nurburgring 2002), free-air laps only, pace x0.98 / x1.00 / x1.02. Gives the track exponent e. About 50 laps per level.
4. Fuel-controlled stint: three fuel loads (or AI Additional Fuel Mult fixed), same car, to fix the fuel term to +/-0.03 s/lap.
5. Qualifying vs race pair for the same cars and strength (watched qualifying only), to measure the session offset and RaceQualRatio effect.
6. Traffic check: gap bins 0-0.5, 0.5-1.5 and over 1.5 s, on two tracks.
7. Tyre state logging (wear, compound) across a stint, to fill the missing tyre term.
8. Wet pair: one wet race and one wet qualifying with RainAbility fixed, to quantify the wet term.
9. Holdout: before a new session, predict the mean lap of 3 AI cars within +/-0.3 s, then compare. This scores the model rather than the fit.

Record for each run: game build hash, track, weather, PLR values, RCD hashes, and whether the player car was off track (yellow risk).

## 5. Limits, gate and pending items

- Desk review of recorded evidence only. No new measurement. No entry was added to findings.jsonl, to the component README or to HANDOFF (outside the one-file scope).
- Live numbers come from single runs or single sessions. BR runs 5 and 6 agree within noise but share track and mod state.
- Not verified: whether the 2026-09-03 trace RA values (norm 0.861, baseline 0.965) were recorded at slider 95 or 100.
- Validator: tools/workspace/check-workspace-structure.ps1 reported 2 errors (unexpected top-level directories .claude and plans) before this task. Not caused by this task; not changed here.
- Pending: owner or Claude to (a) add this record to the adaptive_ai_difficulty index, (b) resolve contradictions 2.1-2.4 before Batch 0 approval, and (c) correct the spec's latch claim (2.6).
