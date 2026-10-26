# Build batch (Haiku A-D, reviewed by Claude) — adaptive_ai_difficulty — 2026-10-15

. Development tools only; nothing run against the game; real ini untouched by these tools.
Verification = unit tests with fake memory / temp files only. No live validation yet.

| Agent | Files (tools/ unless noted) | Tests | Claude review |
|---|---|---|---|
| A rating logger | `aad_rating_logger.py`, `test_aad_rating_logger.py` | 15 OK | Fixed: summary lacked the apply contract names; added `n_player_clean`, `player_median_clean`, `ci_low`, `ci_high` (old names kept). Note: `ai_top5_median_clean` = median of the 5 fastest clean AI laps in the window, not 5 cars. |
| B model + apply | `aad_model.py`, `aad_apply.py`, `test_aad_model.py`, `docs/lever_model.json` | 27 OK | Fixed: unreachable target returned 1.20; now returns the cap point 1 + max_gain_s/abs(S) (1.099 at Austria) because more strength past the car limit only adds side effects. Test updated. Spot checks: gap +2.0 s -> 0.934, gap -1.0 s -> 1.033. |
| C in-race controller | `aad_live_controller.py`, `test_aad_live_controller.py` | 20 OK | Rules match owner: budget 0.3 s (factor 0.0099, 0.009 on the 3-decimal grid), step <= 1/3 budget, every 3 clean laps, dead band 0.2 s, lock after first third. AI window = AI laps with the same lap numbers as the player's last 3 (ASSUMPTION). `--write` on the real ini is not blocked (owner's call). |
| D UI spec | `docs/LIGHTLOADER_UI_SPEC_DRAFT_2026-10-15.md` | read-only | Citations checked: manager source `release_2.3.25_prep_2026-10-13/source_freeze` (ManagerVersion 2.3.26 at LightloaderManager.cs:62), schema `ManagerSchema.cs:51` (Bool/Number defs), ini lookup `ManagerUi.cs:1375-1383`. Correction to D: use a hybrid — existing schema for Enabled/strength, hand-coded part only for the rating view. |

Sign conventions (B): gap = player median - AI median (positive = player slower, AI must slow);
`target_gap +0.5` = AI 0.5 s/lap faster than the player; S negative (s/lap per +1.00 factor).

Open before any live use:
- Lever model has Austria only (MEASURED); Monza pending V3; every other track (e.g. Indianapolis) uses the
  DERIVED fallback (sensitivity scaled by lap time).
- Rating logger: player off-track/cut signals 0x40E8/0x4104 not live-checked in this build; first lap of a
  session never counts as clean.
- Summary must record the AllAIFactor the laps were driven at (apply assumes the ini's current value).
- Per-track rating store (file beside the DLL) not built; UI depends on it.
- Agents skipped AGENTS.md cold start by brief design; this file is the durable record.

## Balance changes (Claude, owner-approved 2026-10-15: "we need to find proper balance")
Trigger: Albert Park 1997 CO-OP race data (`Documents\F1 Challenge V10\RaceData\MP_Albert Park, 1997_20261014_171747-...json`,
33 %, 19 laps). Matching the AI FIELD median would set factor ~1.07 and drop Matias P7 -> P18 and Tester A P1 -> P9
(uniform-shift replay, DERIVED, fallback sensitivity). Matias's real P7 came from a last-lap brake failure (lap 19 =
133.7 s, -38.6 s); without it humans were 1-2 with the best AI ~5 s behind. Tester A started P24: his traffic laps
(~97.7 s) vs clear-air laps (~93.8 s) made the median underrate him.
1. `aad_rating_logger.py`: player lap is not clean when within 1.0 s (TRAFFIC_GAP_S) of the car ahead for more than
   25 % (TRAFFIC_FRAC) of the lap's samples (reason "traffic"). Summary adds front-runner fields
   `ai_top5_median_clean`, `gap_top5_median`, `ci_top5_low/high`. Test fixture distance made realistic
   (time x speed). Tests 17 OK.
2. `aad_apply.py`: `--reference top5|field`, default top5 (match the AI front-runners, not the field). Tests 28 OK.
3. Replay of the race under the new rules (traffic proxy = within 1 s of the car ahead at both line crossings, since
   the file has no in-lap gaps): Tester A 13 clean laps, gap to AI top 5 -0.01 s -> factor 1.000; Matias 7 clean laps
   (8 traffic laps dropped), gap -0.77 s -> 1.019. Earlier whole-race estimate for Tester A: -0.6 s -> 1.015.
   Inference: at Albert Park both humans need only a small AI bump (~1.00-1.02). Per-lap estimates are noisy
   (7-13 laps, fuel burn, crude traffic proxy); real calibration needs the live logger. Not validated in game.
Open: AI laps in the reference are not traffic-filtered; CO-OP is refused by the DLL session guard (host rule pending).

## Owner design decisions 2026-10-16 (night, not yet implemented)
- D-SLIDER-BASELINE: keep the player's own game AI-strength slider (e.g. 93) as the starting point. The mod factor
  multiplies the native value that already includes the slider, so factor 1.00 = the player's familiar level. The
  per-track rating drifts the factor from 1.00 (EMA per session) toward the player's real level. The mod never
  changes the slider (the slider also scales the player car: finding aad-stored-strength-player-and-ai-live). Each
  rating is stored with the slider value it was driven at; a slider change restarts that track's drift from 1.00.
  No-data fallback = factor 1.00 at the player's slider. UI shows e.g. "Your AI: 93 (+2 % at A1 Ring)".
- D-PRACTICE: practice assessment of 3-5 clean laps vs AI front-runners sets the level for qualifying and race;
  locks at qualifying; skipped practice -> fallback ladder with an on-screen "ESTIMATED" message.
- D-REFERENCE: default target = AI front-runners (top 5), traffic laps excluded (implemented 2026-10-15).

## Pace estimates asked by owner 2026-10-16 (analysis only)
- GDB 'Race Laptime' vs measured native AI field median (6 tracks, 2001, 50 %): ratio 0.958-1.029; Silverstone 2002
  race files 0.984. Use GDB x ~1.0 (+-3 %) as the AI race-pace prior for unmeasured tracks (DERIVED).
- Team offsets at equal RA (2001 cars, runs 10-11): Williams/Ferrari about -1.4 %, BAR -1.0 %, Benetton +0.6 %,
  Minardi +1.8 % vs field median.
- Estimates: 2002 Ferrari Silverstone race lap 1:22.9 (MEASURED, 10 race files); 1999 Benetton Barcelona ~1:26.7
  (DERIVED, GDB 86.425); 2000 BAR Spa ~1:58.5 (DERIVED, GDB 120.154 may include the wet 2000 race).
- A-1 Ring 2002 simulated quali floor 71.263 s confirmed (finding aad-sim-quali-floor-confirmed-a1ring2002-2026-10-16);
  driven AI quali estimate ~1:08.0 (DERIVED, untested).
- 2026-10-16: model tests now read a frozen fixture `tools/test_lever_model_fixture.json` (night1 state), so new lever measurements in `docs/lever_model.json` no longer break test numbers. 28 OK.

## Realistic mode built (Claude, 2026-10-16; DESIGN_DECISIONS DD-01/DD-03)
- `tools/aad_rating_logger.py`: reads every car's name (veh+0x4678, SDK F1C_OFF_CarName, FACT); AI cars with the
  player's car name are the teammates. Per lap: `player_car`, `teammate_clean`, `gap_to_teammate`; summary:
  `n_teammate_laps`, `teammate_median_clean`, `gap_teammate_median`, `ci_teammate_low/high`. Tests 19 OK (2 new).
- `tools/aad_apply.py`: `--style realistic` (DEFAULT, reference = AI teammate, target `--target-teammate-pct`
  -0.3 = player 0.3 % faster than the teammate) or `--style competitive` (AI top 5, `--target-gap` s);
  `--reference` overrides; refuses Realistic with < 5 teammate laps. `--mode season-lock` = alias of the lock mode
  `championship` (not the Competitive style). Tests 30 OK (2 new; older numeric tests pinned to `--reference top5`).
- Smoke (dry run, BAR-like summary: -1.38 s vs teammate, -0.48 s vs top 5): Realistic 1.046, Competitive 1.020.
- DERIVED: the teammate shares the track's AIW strength curve, so Realistic mode cancels the per-track AIW
  differences (DD-09) automatically for the rating; DD-09 still matters for the fallback ladder (no teammate data).
- Not yet: `tools/aad_live_controller.py` still nudges on the AI field median (should use the teammate in Realistic
  mode); car name read not live-checked (first logger run); teammate absent when the player drives a car that has no
  AI twin (custom grids) -> Realistic refuses, falls back to Competitive by user choice.
- 2026-10-16: `tools/aad_live_controller.py` now has `--style realistic` (DEFAULT: in-race gap = player minus AI
  teammate in the same car, minus -0.3 % of the teammate lap; holds when no clean teammate lap is in the window) and
  `--style competitive` (5 fastest clean AI laps of the window); `--reference teammate|top5|field` overrides. Car names
  read live from veh+0x4678 per snapshot. Budget, step, dead band, lock rules unchanged. Tests 23 OK (3 new; the 20 old
  tests pinned to the original field reference). Not live-tested.

## Per-track rating store built (Claude, 2026-10-16; DD-04/05/08/13/15)
- `tools/aad_ratings.py` (new): JSON store `adaptive_ai_difficulty_ratings.json` beside the DLL (schema 1, documented
  in the module docstring): per track `slider`, `style`, `factor`, `n_races`, `n_clean_laps`, `gap_teammate_pct`,
  `gap_top5_pct`, `player_car`, `history` (max 20). Atomic save (temp file + replace) so the UI can read any time.
  `lookup()` = fallback ladder: track rating at this slider -> overall skill (median over >= 2 rated tracks) at half
  strength -> 1.00 (native at the player's slider). A slider change restarts that track's drift and keeps the history
  (DD-15 still OPEN: converting through the slider curve). CLI `show` / `lookup`. Tests 9 OK.
- `tools/aad_rating_logger.py`: summary now carries `slider` (dword 0x7D01A0). Tests 19 OK.
- `tools/aad_apply.py`: `--store PATH` records each adaptive session on `--write`; `--mode store` (no `--rating`)
  sets the pre-race factor from the ladder using the track's lever-model sensitivity; the live store beside the DLL is
  refused without `--allow-live`. Tests 32 OK (2 new: record then store mode restores the factor; store mode needs
  --store/--slider). Smoke on temp files: BAR-like session -> STORED factor 1.023 (EMA of 1.046), pre-race lookup returns
  it, an unrated track returns 1.00 (only one rated track), live store refused.
- Not yet: track key read from the game (the caller passes --track); the Lightloader UI reading the store; first live run.
