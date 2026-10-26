# Friend race data (Tester A) for adaptive_ai_difficulty — 2026-10-16

Source: `%USERPROFILE%\Desktop\RaceData\RaceData` (read-only; sent by the owner's friend). Analysis by Haiku G
(scripts `%TEMP%\aad_friend\`), reviewed and written up by Claude (the subagent could not write this file).
Outputs beside this file: `files.csv` (137 rows, class per file), `ratings.json` (per-track gaps).
Tiers: MEASURED = computed from the exports; DERIVED = with a stated assumption. Nothing in the game, ini or DLL touched.

## Summary
- 67 real races driven by the friend (2001/2002 single player, one 1997) on 16 circuits; 1298 clean laps.
- He beats the AI front-runners (top-5 clean-lap reference) in 56 of 67 races: pooled -0.83 s/lap (-1.0 %) vs AI top 5,
  -1.64 s vs the AI field. Qualifying: median 0.92 s off pole (AI field median 2.38 s).
- Very consistent: lap spread (IQR) 0.95 s vs AI 2.09 s, tighter in 62 of 67 races.
- Track spread is real: from -3.7 % (Nurburgring, much faster than the AI) to +1.1 % (Magny-Cours, slower than the AI).
  A1-Ring is strong (-1.5 %); Indianapolis is only one race (-0.1 %), so "weak at Indianapolis" is not testable yet.
- Results lag pace: 37 of 67 races end before full distance (retired or quit); 12 wins / 13 podiums in 32 verifiable finishes.
- The AI slider he races at (owner: 93) is not stored in any export; the mod factors below are relative to his own slider.
- Data quality: 76 of 105 JSON files are malformed (legacy exporter, see Q8); salvage is needed to use them.

## Per track: his gap and the mod factor it implies (DERIVED)
Gap = his clean-lap median minus AI top-5 reference (negative = he is faster). Factor = 1 + (-gap %) / 30.7, using the
measured relative sensitivity (-0.307 per unit factor, 6 dry tracks, lever_model.json); factor is relative to the AI level
he raced at (his slider). Target = AI front-runners match his pace.

| Circuit | Races | Clean laps | Gap to AI top 5 s (%) | Gap to AI field s | Quali gap to pole s | Mod factor |
|---|---|---|---|---|---|---|
| Nurburgring | 7 | 112 | -3.77 (-3.7 %) | -4.48 | 0.00 | 1.121 (above the measured 1.10 range) |
| Spa | 8 | 112 | -1.76 (-1.6 %) | -2.91 | 1.49 | 1.052 |
| Sepang | 3 | 63 | -1.56 (-1.5 %) | -2.39 | 0.65 | 1.049 |
| Barcelona | 5 | 125 | -1.22 (-1.4 %) | -2.15 | 1.08 | 1.046 |
| A1-Ring | 10 | 268 | -1.08 (-1.5 %) | -1.86 | 0.00 | 1.049 |
| Silverstone | 4 | 66 | -0.78 (-0.9 %) | -1.94 | 4.41 | 1.029 |
| Monaco | 4 | 74 | -0.73 (-0.9 %) | -1.52 | 0.85 | 1.029 |
| Hungaroring | 3 | 85 | -0.50 (-0.6 %) | -1.04 | 0.69 | 1.020 |
| Interlagos | 6 | 103 | -0.47 (-0.6 %) | -1.09 | 1.93 | 1.020 |
| Monza | 1 | 13 | -0.42 (-0.5 %) | -1.54 | 2.22 | 1.016 |
| Montreal | 5 | 84 | -0.36 (-0.4 %) | -1.53 | 0.19 | 1.013 |
| Albert Park | 4 | 54 | -0.18 (-0.2 %) | -1.01 | 0.00 | 1.007 |
| Indianapolis | 1 | 25 | -0.05 (-0.1 %) | -1.34 | 2.15 | 1.003 |
| Imola | 3 | 65 | +0.40 (+0.5 %) | -0.33 | 0.96 | 0.984 |
| Suzuka | 1 | 16 | +0.42 (+0.4 %) | -0.72 | 2.04 | 0.987 |
| Magny-Cours | 2 | 33 | +0.93 (+1.1 %) | -0.16 | 1.16 | 0.964 |
| All | 67 | 1298 | -0.83 (-1.0 %) | -1.64 | 0.92 | 1.033 |

Pole gap 0.00 = he took pole (median). Qualifying in these exports may be simulated when skipped (floor formula in findings).

## Q1 Inventory (MEASURED)
105 JSON: friend real races 67; other-human real races 2 (MP files, friend not driving); short/partial 25; zero-lap test 11.
32 CSV: 14 rounds of a CHAMP "F1 2000 Season" (R4-R17) and 18 single-race result tables. Seasons: 2002 (42 races),
2001 (24), 1997 (1); race length 18-39 laps. `recovery\`: 3031 JSONL, 6.8 GB = Safety Car / coordination event logs
(race_data_collector checkpoints), not lap data.

## Q4 Consistency and learning (MEASURED)
Friend IQR 0.95 s vs AI 2.09 s. Last third vs first third of a race: -0.90 s (fuel burn-off, not learning). Repeat races of
one circuit: improved at Nurburgring, Spa, Barcelona; worse at Monaco, Montreal; flat at A1-Ring.

## Q5 Outcomes (MEASURED, partly unreliable)
Incomplete 37/67 (agrees with native DNF where present, 15/15). Places gained grid to finish +3.7 (n=32). Hits: 31 as the
driver, 54 as the victim (blame unknown). Pit-stop counts unreliable.

## Q6 Settings
No slider / difficulty field in any export; `driverRatings.nativeRaceNorm` is the per-driver RCD value, constant. The owner's
"93" is unverifiable from the data. Note (Claude): the importer's "factor 1.0" means mod factor 1.0 (no mod), which is
compatible with any slider; it is not a conflict with 93.

## Q7 Championship CSVs (DERIVED, points 10-6-4-3-2-1)
CHAMP 2000 (14 rounds): about 10 points, 0 wins; best lap faster than teammate Villeneuve in 5 of 13 rounds. Single-race CSVs:
about 42 points, 3 wins (teammate matching unreliable).

## Q8 Data quality (MEASURED, cause checked by Claude)
- 76 of 105 JSON fail to parse: the legacy exporter (`exporterVersion` "1.0", no `producer` field, files 2026-07-12 to
  2026-10-03) writes `"serviceMessages": [` and then the `"positionHistory"` key inside that array. All 29 files from
  Race Data Collector 0.2.5 parse; the owner's own 279 files (producer 0.2.6) parse. Haiku salvaged the 76 by section repair
  (ASSUMPTION A2: faithful); `observedAt` missing in salvaged files was filled from `cumulativeFL` (A3), max outlier 2922 s.
- MP files: controllerRole 2 marks 23-24 drivers as human; the human rule needs playerConfig + isPlayer.
- Track names in metadata sometimes disagree with the scene; circuit taken from `sessionInfo.trackName` (A8).

## Limitations
Traffic proxy from line crossings only; fuel load not modelled; AI reference = top-5 laps in a window, not five cars; his slider
unknown, so factors are relative to whatever level he raced at; tracks with 1-2 races (Monza, Indianapolis, Suzuka,
Magny-Cours) are indicative only; Nurburgring's factor 1.12 is beyond the measured hook range (0.85-1.10).

## What this means for the mod
- The per-track rating idea is confirmed on real data: his needed factor ranges 0.96-1.12 across tracks at one slider.
- One global factor (1.033) would leave him ~3 % too fast at Nurburgring and ~2 % too slow at Magny-Cours.
- Follow-ups: (1) importer: salvage legacy exports, use playerConfig/isPlayer for humans; (2) Race Data Collector: add the
  AI slider (dword_7D01A0) and GDS to metadata so ratings know the level they were driven at; (3) measure the hook above
  1.10 if players like him need ~1.12.

## Why some tracks look easy or hard: AIW strength curves (Claude, 2026-10-16)
Owner: the friend's slider went 90 -> 92 -> 95 -> 93 over time; same track files as the owner. GDB files hold only the
simulation lap targets, pit plans and scripted failures; the per-track AI strength is in each track's AIW
(`WorstAdjust`/`MidAdjust`/`BestAdjust` = slider 70/100/120 points, `RaceQualRatio`). At slider 93 a standard track
(0.85/1.00, RQR 0.985) gives AI strength 0.9505; deviations (AI strength vs standard, predicted lap effect with the
Austria slider-ladder slope 0.88 lap-% per strength-%):

| Track | Worst/Mid/Best, RQR | AI strength vs standard | Predicted gap shift | Friend gap | Residual (his "true" edge) |
|---|---|---|---|---|---|
| Nurburgring | 0.81/0.96/1.06, 0.985 | -4.15 % | -3.65 % | -3.7 % | -0.05 % |
| Imola | 0.89/1.023/1.19-1.69, 0.985 | +2.79 % | +2.46 % | +0.5 % | -1.96 % |
| Albert Park | 0.88/1.01/1.085, 0.985 | +1.52 % | +1.34 % | -0.2 % | -1.54 % |
| Monaco | 0.8585/1.01/1.111, 0.988 | +1.31 % | +1.15 % | -0.9 % | -2.05 % |
| Sepang, Suzuka | 0.8415/0.99/1.089, 0.985 | -1.00 % | -0.88 % | -1.5 / +0.4 % | -0.62 / +1.28 % |
| Spa | 0.842/0.997/1.097, 0.988 | -0.13 % | -0.11 % | -1.6 % | -1.49 % |
| A1-Ring | 0.85/1.00/1.10, 0.988 | +0.30 % | +0.27 % | -1.5 % | -1.77 % |
| all others | 0.85/1.00/1.10, 0.985 | 0 | 0 | -1.4..+1.1 % | same as gap |

- MEASURED/DERIVED: weighted by races, friend gap vs AIW strength deviation r = +0.82, slope +0.58 (expected +0.88). The
  Nurburgring "talent" is entirely the AIW (MidAdjust 0.96: AI ~4 % weaker at every slider); Imola, Albert Park and
  Monaco hide a real edge of ~1.5-2 % because their AI is 1.3-2.8 % stronger. After the correction his edge is a fairly
  uniform -0.5..-2 % on most tracks; Magny-Cours (+1.1 %, 2 races) and Suzuka (+1.3 %, 1 race) stay unexplained (few races).
- Imola 2002 BestAdjust 1.69 (2001: 1.19) looks like a data error in the track file (affects sliders above 100 only).
- Limits: slider history 90/92/95/93 not dated per file, the slope is from one track (Austria), traffic/fuel not modelled.

## Replay: the mod over his 67 races (DERIVED)
Model: per-track rating, start factor 1.00 at his slider, EMA 0.5 per race toward target 1 + (-gap %)/30.7 (front-runner
target), linear lever. Mean absolute gap to the AI front-runners per race: 1.27 % without the mod, 0.44 % with it (67 races).
Converged factors (last race): Nurburgring 1.119, Spa 1.051, A1-Ring 1.048, Barcelona 1.043, Monaco/Silverstone 1.026,
Interlagos 1.019, Montreal 1.012, Albert Park 1.006, Imola 0.988, Magny-Cours 0.982 (2 races, still 0.55 % off).
Tracks with 1-3 races keep a visible error (convergence halves the error per race). Slider changes: the current design
restarts a track's drift after a slider change; converting the rating through the measured slider curve would keep it.

## Car matters (Claude, 2026-10-16)
BAR 2001: 27 races, -1.67 % vs AI teammate, -0.60 % vs AI top 5, 8 wins / 12 podiums. Williams 2000: 14 races, -0.96 % /
-1.13 %, 5 wins. Williams BMW 2002: ~34 races, -1.1 % / -1.0..-1.25 %. Benetton: 3 races, ~0. The mod's front-runner target
ignores the car; proposed: a Realistic mode that rates the player against the AI teammate (same car) and targets a small
edge (e.g. -0.3 %), plus the current Competitive mode (front-runners). Owner decision pending.

## Realistic-mode replay (Claude, 2026-10-16, DERIVED)
Target: friend 0.3 % faster than his AI teammate. Factor = 1 + (-(gap_tm %) - 0.3) / 30.7: BAR 2001 (-1.67 %) -> 1.045;
Williams 2000 (-0.96 %) -> 1.021; Williams BMW 2002 (-1.1 %) -> 1.026. The BAR then runs at BAR pace among AI that are
~4.5 % stronger, so podiums need exceptional races and wins are rare; in the Williams he can still fight for wins.
The teammate shares the track's AIW curve, so the per-track AIW bias is removed from the rating automatically.
