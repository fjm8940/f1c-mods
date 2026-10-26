# Race history import (2026-10-16)

Status: **first import, small sample, NOT a tuning basis** ().

Tool: `tools\aad_import_racedata.py` (stdlib only). Tests: `tools\test_aad_import_racedata.py`
(25 tests, all pass). Input: the game's `RaceData` folder, 257 Race Data Collector exports
(schema `f1c.race-data/2.0`), read only. No game file, ini or game folder was touched.

Outputs in this folder: `ratings.json` (per human, per track, overall, and per real race),
`files.csv` (one row per file), this README.

## Counts per class

| Class | Files |
|---|---|
| real_race | 3 (MP 2, SINGLE 1) |
| player_retired_test | 246 (SINGLE 243, MP 3) |
| short_or_partial | 8 |
| error | 0 |
| **Total** | **257** |

## Tracks with real races

- Albert Park, 1997 (MP, 19 laps, 2026-10-14)
- Silverstone, 2002 (MP, 19 laps, 2026-10-12)
- Sepang, 2002 (SINGLE, 28 laps, 2026-09-15)

## Per-human summary (overall = median of track values; lower = faster than AI)

| Human | Races | Tracks | Clean laps | Gap to AI top-5 (s) | Gap to AI field (s) | Last race |
|---|---|---|---|---|---|---|
| Tester A | 2 | 2 | 31 | -0.81 | -2.40 | 2026-10-14 |
| Matias Driver | 2 | 2 | 22 | -0.89 | -2.95 | 2026-10-14 |

Per race (gap to AI top-5 / field, median over clean laps):

| Race | Human | Grid to finish | Clean laps | Median clean lap (s) | Top-5 gap | Field gap |
|---|---|---|---|---|---|---|
| Albert Park, 1997 | Tester A | 24 to 1 | 13 | 94.835 | -0.410 | -2.531 |
| Albert Park, 1997 | Matias Driver | 23 to 7 | 7 | 94.708 | -1.196 | -3.722 |
| Silverstone, 2002 | Matias Driver | 18 to 1 | 15 | 83.400 | -0.575 | -2.173 |
| Sepang, 2002 | Tester A | 6 to 19 | 18 | 100.541 | -1.210 | -2.276 |

Tester A has no lap data in the Silverstone file (listed as a human, no laps).

## Method

- Human = name equal to `playerConfig.nameOfPlayer` OR `telemetrySnapshot.controllerRole` 2.
  All other drivers are AI.
- Names: the ` (1997)`-style suffix is stripped everywhere (AI names carry it in some sections).
- Clean lap: lap >= 2; not a pit lap (pit lap L excludes L and L+1); 50 s < lapTime < 200 s;
  lapTime < 1.05 x the driver median (median of laps 2+ within the time bounds, pit laps out);
  not in traffic.
- Traffic proxy: another car crossed the line 0 to 1.0 s before this car at BOTH the lap start
  (previous crossing) and the lap end.
- Gap per clean human lap: lapTime minus the median of the 5 fastest AI clean laps ending in
  (lap start, lap end + 30 s]; field gap uses the median of all such AI laps.
- Per race: median over the human's clean laps of each gap. Per track: unweighted median of
  race values. Overall: median of track values.
- Classes: real_race = a human completed >= 50 % of scheduled laps AND >= 5 clean laps;
  player_retired_test = primary human < 2 laps or 0 clean laps; else short_or_partial.
- Native AI: all these races ran at native AI (factor 1.0). This is stated in the brief, not
  read from the files.

## Limitations

- Traffic proxy only: no in-lap gap data in the files, so a car just behind another car can be
  flagged as clean when it is not, and vice versa.
- Fuel load and tyre state are not corrected for (the files hold fuel fields, not used here).
- No practice or qualifying pace data is used.
- SINGLE and MP races are pooled; the MP sample is 2 races.
- Sample is 3 races and 2 humans. Gaps are descriptive only, not a rating for tuning.
- Human names are not merged: `Matias` (137 retired files) and `Matias Driver` are separate keys.
  `Matthias Steppe` also appears as a local player in other sessions.
- Only one file has no identifiable human (Monza, 2002, 2026-09-30); classified short_or_partial.

## Assumptions (not in the brief)

1. "Within 1.0 s after another car" read as 0 <= (own crossing - other crossing) <= 1.0 s
   (one-sided), not an absolute gap.
2. Driver median for the 1.05x rule uses laps 2+ within 50-200 s, excluding pit laps, before the
   traffic filter.
3. A lap has a start only when the previous lap number is lap-1; otherwise it is not clean.
4. Laps completed = gameResults.lapsCompleted, falling back to the highest lap in lapChart.
5. Each file's primary human = most laps completed. real_race applies if any human qualifies.
6. Human with telemetry but no laps is listed under humans but has no race entry.
7. controllerRole 0 is treated as AI (brief: "all others").
8. Per-race human entries and ratings only include humans with >= 1 clean lap.
9. Top-5 median uses fewer cars when the window holds fewer than 5 AI clean laps.
10. Retirement check runs after real_race; a human with >= 2 laps and 0 clean laps is retired.
11. Unweighted medians for track and overall values (brief allowed weighted medians).
12. Error rows record the exception type only (no paths).

## Reproduce

    python -I -B tools\aad_import_racedata.py --input "<RaceData folder>" --output "docs\race_history_import_2026-10-16"

`-I` ignores `PYTHONDONTWRITEBYTECODE`, so pass `-B` as well to avoid writing `.pyc` files.

## Pending / follow-up

- Human validation: owner check that the three races and the human names are right.
- Name merging (`Matias` vs `Matias Driver`) needs an owner decision.
- Sample size is too small for any AI tuning; more races needed before use.
- Not updated (outside this brief): the mod `README.md` and `docs/HANDOFF_NEXT_2026-10-15.md`
  do not mention this importer yet.
- Stray bytecode: a `tools\__pycache__\aad_import_racedata.cpython-312.pyc` was written by the
  first test run (before the `-B` fix). Left in place: the brief says delete nothing. Owner to
  remove it.
