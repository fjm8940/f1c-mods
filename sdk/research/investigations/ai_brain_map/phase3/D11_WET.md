# D11_WET — wet Imola replays (D11a, BATCH B item 1)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Data: `%TEMP%\ai_brain_live\watch_20261002T194606Z`
(WET Imola, monsoon, 6 laps, 19 AI cars, 20-waypoint records, `cars_static.jsonl` present, no race_start,
registry v1.1 at record time: `unnamed_4554` = raw int bits `1065353216` = 1.0, `command_queue_write_index`
NaN, read with v1.2 types). Dry same-track baseline: `watch_race2` + `watch_race2b` (Imola, old 6-waypoint
records, no cars_static). Dry tyre baseline: Monza `watch_race4fullc` + `watch_race4fulld`.

Script: `phase3/analysis/d11_wet.py` (new; imports `phase2/analysis/c1b_lib.py`,
`phase2/analysis/tireforce.py`, `phase3/analysis/tireforcesolver.py` unmodified). Command:

Owner safety car / red flag mods: not confirmed off for the wet race (NOTES.md). No SC/RF marker exists in
the brain logs; no period was filtered. Wet results are therefore **MOD-INFLUENCED-risk, not vanilla-proven**.

## (a) D1 corner-speed target replay on the wet folder — VERDICT: PASS

Same formula/exclusions as D1 (`(TireForceSolver(wp) * 0x4504 + 0x4500) * 0x4580 * 0x453C * wp+0x84`,
AIComputePassScore 0x42A780; snapshot v1.1 `unnamed_4554` reinterpreted as float = 1.0).

| set | n | med | p10 | p90 | min | max | within ±2 % |
|---|---|---|---|---|---|---|---|
| wet all | 733 | 0.9993 | 0.9985 | 0.9999 | 0.9980 | 1.0012 | 100.0 % |

- 19/19 drivers med 0.9985..0.9998; predicted-target bins 20-40 and 40-60 m/s both med 0.9993.
- Exclusions: 27 970 snapshots with null limiting waypoint (no target); 0 branch, 0 missing waypoint,
  0 missing static row, 0 missing `extra.v3520`.
- Worst case ratio 0.9980 (David Coulthard, target 30.2 m/s) — residual is the same ~0.1 % low bias seen
  on Monza (snapshot-vs-tick timing), not a wet-specific term.
- **VERDICT (a): PASS → the corner-speed formula is live-confirmed on a second track (wet Imola), PASS rule
  (|med−1| ≤ 2 %, p10..p90 within ±5 %) satisfied. Consistent with `ai-brain-corner-speed-live-confirmed-2026-10-02`.**

## (b) Braking-step replay with the TireForce model (c9 method) — VERDICT: CONFIRMED

`v -= seg_len(wp+0x3C)/v * 0x44CC * TireForce(v, wp+0x116/120/124) * 0.915`, tables from `cars_static.jsonl`
+ `extra.v3520/v3524`. 2 042 evaluable steps, all waypoint/static fields present (0 skips).

| step | n | p10 | med | p90 | within ±20 % |
|---|---|---|---|---|---|
| first | 424 | 0.844 | 1.001 | 1.352 | 76.7 % |
| mid | 1247 | 0.852 | 1.003 | 1.335 | 77.9 % |
| last | 371 | 0.358 | 1.000 | 2.047 | 58.5 % |
| all | 2042 | 0.786 | 1.000 | 1.412 | 74.1 % |

- Median 1.000 as on dry Monza (85.4 % within ±20 %, n=1936); the wet spread is wider, concentrated in the
  last step (tiny absolute losses → ratio noise, same behaviour documented for Monza in `phase2/LIVE_RESULTS.md` C9).
- **VERDICT (b): CONFIRMED on wet; no formula change indicated.**

## (c1) Physical braking decel, wet vs dry Imola — VERDICT: wet below dry

Consecutive brake-zone samples 0.03..0.15 s apart inside active braking (`c1b_a_braking.py` method).

| run | n | med | p90 | max |
|---|---|---|---|---|
| wet `watch_20261002T194606Z` | 293 | **21.1** | 33.4 | 51.4 |
| dry `watch_race2` | 381 | 31.1 | 40.9 | 54.2 |
| dry `watch_race2b` | 659 | 32.5 | 41.1 | 56.4 |

- WET/DRY median = **0.664** → wet braking is ~34 % weaker (monsoon). Same direction as the wet corner
  targets, but larger; expected from the wet tyre tables (d).
- **VERDICT (c1): wet physical braking decel is below the dry Imola baseline (medians 21.1 vs 31.8 m/s²).**

## (c2) Corner targets at the same limiting waypoints (match x/z ≤ 2 m) — VERDICT: 75 % of dry

473 wet / 835 dry brake-zone samples with a non-null limiting waypoint found in the logged chain; matched by
waypoint x/z within 2 m (dry 6-wp records; 24 wet samples unmatched, 5 %). 22 matched waypoints:

- ratio wet/dry across matched waypoints: **n=22, med 0.751, p10 0.725, p90 0.779**.
- Range 0.710 (wp 365.0/−1069.1) .. 0.789 (wp 630.7/−1287.1); all 22 below 0.80.
- `corner_speed_scale` 0x4580 is only 0.9754 wet (known), so ~23 % of the slowdown comes from the tyre
  model (TireForceSolver tables, see (d)); this is the prediction recorded in
  `PRIOR_ART_MAP_2026-10-02.md` section 2.
- **VERDICT (c2): wet corner targets are 75.1 % of the dry Imola targets at the same waypoints (median).**

## (c3) Lap pace, wet vs dry Imola (overview 1 Hz) — VERDICT: wet ~20 % slower

Lap duration = first 1 Hz sample of lap n+1 minus first sample of lap n, per driver.

| run | complete laps | med s | p10 | p90 | best_lap med (n) |
|---|---|---|---|---|---|
| wet | 95 | 99.63 | 98.61 | 143.29 | 99.08 (19) |
| dry `watch_race2` | 18 | 99.95 | 92.84 | 103.01 | 89.50 (18) |
| dry `watch_race2b` | 906 | 83.09 | 81.28 | 85.10 | 80.99 (18) |

- Combined DRY median 83.28 s; WET/DRY = **1.196** (best_lap ratio 1.194). Caveat: the dry median mixes the
  SC-influenced start (`watch_race2` ~100 s laps) with the green race (`watch_race2b` ~83 s); the wet race is
  the whole monsoon race and its p90 tail (143 s) is traffic/SC-like bunching.
- **VERDICT (c3): wet lap pace ≈ 119.6 % of dry (median / best-lap consistent within 0.2 %).**

## (d) Tyre tables, wet Imola vs Monza dry, same driver — VERDICT: all model blocks change

Last `cars_static.jsonl` row per driver, 19 drivers present in both runs, values 0x3A7C..0x3D50.
Offsets differing by >0.1 ‰ in ≥ half the drivers = 101; of these 59 are entries the two models read:

| block | changed / entries |
|---|---|
| T3740 = veh+0x3A7C (scalar) | 1/1 (wet/dry med 1.409) |
| T3839/T3840 = 0x3C08 + 0x3C0C..0x3C44 (selector+table) | 14/16 (table ~0.678) |
| T3855/T3856 = 0x3C48 + 0x3C4C..0x3C84 (selector+table) | 14/16 |
| T3871/T3872 = 0x3C88 + 0x3C8C..0x3CC4 (selector+table) | 14/16 |
| T3903/T3904 = 0x3D08 + 0x3D0C..0x3D44 (solver selector+table) | 14/16 |
| T3919/T3921 = 0x3D48, 0x3D50 (limits) | 2/2 (T3921 wet/dry med 1.431) |

- The two unchanged entries per block are the selector/scalar pair tails at the end of each 15-float table
  (entry 14 / adjacent not read); every interpolated entry changes. Direction: force/solver tables are lower
  wet (≈0.63..0.68 of dry), while T3740 and T3921 (aerodynamic/longitudinal terms) are higher (≈1.41).
- 42 further changed offsets (0x3A84, 0x3AAC..0x3ABC, 0x3B2C, 0x3B38, 0x3B6C..0x3B74, 0x3C04,
  0x3CD0..0x3D04, …) are **not read by the TireForce or TireForceSolver model** and remain unnamed tyre/vehicle
  property tables (write-stage material, not needed for D1/D2).
- **VERDICT (d): wet tyres change 59/67 named TireForce/TireForceSolver entries — every selector+table block,
  T3740 and T3919/T3921. The corner-target drop in (c2) is attributable to the solver tables (inference from
  direction and magnitude, not a per-corner decomposition).**

## Limitations / open

- Wet race has no `race_start` and no pit/trouble situations; lap 1 and the tail are race-control-shaped.
- SC/RF mods unconfirmed for the wet race; results are per-car deterministic replays of logged state, so a
  SC period cannot bias (a)/(b) beyond logged inputs, but (c1)/(c3) pace numbers are MOD-INFLUENCED-risk.
- (c2) matching is coordinate-based (dry logs have 6 waypoints only); 24 wet samples unmatched (5 %).
- Registry v1.1 at record time: only `unnamed_4554` needed reinterpretation here; D12 will list v1.2 type fixes.

## Wasted-effort check

- One script run repeated after fixing the lap-transition window (`>5 s` between consecutive 1 Hz samples)
  and a missing counter init; both found by the run itself, ~2 extra minutes, no re-reading of large material.
