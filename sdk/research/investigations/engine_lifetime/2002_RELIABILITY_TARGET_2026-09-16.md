# 2002 reliability target and bounded tuning proposal — 2026-09-16

Status: investigation and proposed trial specification, **not an installed change**. Supersedes the unanswered baseline questions in [the earlier file audit](2002_V1_AUDIT_2026-09-16.md). The owner reports roughly 60% retirement in a race and asks for a realistic 2002 solution.

## Historical evidence and target

Primary source: [Formula 1's 2002 results archive](https://www.formula1.com/en/results/2002/races), 17 race-result pages (`/en/results/2002/races/720/.../race-result` through `/736/...`). Read the results-table rows, count `DNF` in the Time/Retired column, and exclude the one `DNS` row from the denominator. The pages list **151 DNF among 352 starters = 42.9%** overall. Race rates range **20–60%**; Australia had 12/20 (60%), Germany 12/21 (57.1%), while Hungary and the United States had 4/20 each (20%). This is an all-cause DNF measure, not engine failure. The real 2002 grid had 11 teams; the local season also includes fictional/alternate 2002 Prost, so its reliability must be a design choice rather than a historical statistic.

Selected team aggregates across the same official tables: Ferrari 4/33 DNF (12.1%), Williams 8/34 (23.5%), Sauber 11/34 (32.4%), BAR 20/34 (58.8%), Jaguar 19/34 (55.9%), Arrows 12/20 (60.0%). These are all-cause, team-level outcomes, not engine-specific calibration targets. The sensible season-level goal is **about 40–45% all-cause DNF**, allowing high-attrition individual races; do not target zero failures or force every race to exactly 43%.

## Local evidence and causal boundary

Four read-only exports in `%USERPROFILE%\Documents\F1 Challenge V10\RaceData` (Sepang 2026-09-15; Albert Park, Interlagos, Imola 2026-09-16) each have a 24-car initial grid. `retirementReasons` counts are respectively **9, 19, 11, 17 = 56/96 (58.3%)**. Their reason-code breakdown in total is: engine 1 = 14, gearbox 2 = 6, suspension 4 = 19, brakes 5 = 11, other 0 = 4, other 6 = 2. The four mechanical categories are **50/96 (52.1%)**. This is consistent with the owner's approximate 60% report, but not an independently controlled A/B test. One export says `completeHistory=true`; the other three say false. The collector marks the classification `final=true`, `official=false`, `closeReason=all_observed_cars_terminal`; its limitations and native acceptance work remain in `development/mods/race_data_collector/STATUS.md`. Do not treat the captured reason codes as a definitive cause for every retirement, or infer driver-specific reliability from these four samples.

The current `Save\Matias\Matias.PLR` records `GPRIX Failure Rate="2"` (timescaled), `GPRIX Race Length="0.20293"`; `CHAMP Failure Rate="2"`, `CHAMP Race Length="0.10000"`. These are settings-file observations, not proof of the exact mode of each capture. The source-backed player/AI distinction and race-length divisor are in `mods/extended_vanilla_features/docs/ENGINE_FAILURE.md`: the four AI mechanical timers use a draw based on **3 × LifetimeAvg and 3 × LifetimeVar**; the player has a separate engine-life path. Therefore the 2002 engine-file pair affects AI engine, gearbox, brake and suspension timer incidence, not just engine-reason retirements. Collision and other paths will remain. Altering `LifetimeEngineRPM`/`LifetimeOilTemp` alone is not the correct first test for the observed mixture.

## Proposed first candidate (heuristic, not calibrated result)

In a **versioned development trial**, multiply only each team's `LifetimeAvg` and `LifetimeVar` by **1.25**, rounding half away from zero to an integer. Preserve the four RPM/oil lifetime thresholds, rev limits, cooling, torque, `.hdv`, `.veh`, and `.PLR` settings. This keeps the existing team ranking and roughly the same relative random spread while making all four AI mechanical timers longer. It is a bounded first step aimed at reducing the roughly 14 retirements per 24-car captured race toward roughly 10–11; **a 25% lifetime increase is not a proven 25% DNF reduction**. Do not borrow the 2001 9,389-second mean wholesale: that could erase the unusually high real-2002 attrition and changes the character of the season.

| Team | Current Avg / Var | Trial Avg / Var (seconds) |
|---|---:|---:|
| Arrows | 3400 / 1106 | 4250 / 1383 |
| BAR | 2806 / 913 | 3508 / 1141 |
| Ferrari | 4232 / 1302 | 5290 / 1628 |
| Jaguar | 2818 / 924 | 3523 / 1155 |
| Jordan | 3378 / 1091 | 4223 / 1364 |
| McLaren | 3128 / 1006 | 3910 / 1258 |
| Minardi | 3269 / 1063 | 4086 / 1329 |
| Prost (alternate) | 3041 / 982 | 3801 / 1228 |
| Renault | 3045 / 985 | 3806 / 1231 |
| Sauber | 4193 / 1281 | 5241 / 1601 |
| Toyota | 3291 / 1078 | 4114 / 1348 |
| Williams | 3838 / 1219 | 4798 / 1524 |

## Trial and decision gate

1. Preserve the exact 12-file baseline hashes and full before-state. Create a separate candidate outside any live, production, or game-installation tree. An installation/test transaction needs a reviewed scope, restoration map, collision check and Matias's approval under the workspace rules. The current base-season files must remain unchanged.
2. With the game closed, apply only to an approved test installation, verify all 12 exact values and hashes, and run guarded full weekends (not QuickRace) using the same failure mode, distance, grid and comparable circuits as baseline. Capture final classification and reason codes; distinguish true retirements from DNS/DSQ and incomplete collector history. Reverse by restoring verified before-state, never by deleting artifacts.
3. Measure pooled all-cause DNF over more than four races and track mechanical subcategories and team ranking. Provisionally accept a season sample near **40–45%**, with broad single-race variation, rather than judging one race. If still near 60%, make one further small step; if well below historical range or Ferrari loses its reliability edge, back off. Human gameplay judgment decides the balance. No statistical guarantee or release authority is claimed from the four existing captures.

No game, profile, or season files were edited in this task. `PRODUCT_STATUS.json` has no evidence-backed stage/path/gate change. Workspace validator started at the existing 11-error baseline; verify again after the documentation index edit. Wasted-effort check: one failed UserData path probe and one PowerShell pipeline syntax retry; no broad builds or idle polling.
