# P3 spec (2026-10-16) — in-race adjustment ("nudge")

Owner 2026-10-16 (live proxy test, Human Driver 2nd in an Arrows, teammate 10th, race 2 at AI +3.2 %): "go ahead"
with the in-race adjustment proposed in chat: check after counted laps; at a track without a firm rating correct up to
~1-1.5 s/lap within the first 5 counted laps then lock (DD-24); at a rated track only small nudges <= 0.3 s/lap (DD-06).
Prior art: `tools/aad_live_controller.py` NudgeController (budget, dead band 0.2 s, max step, lock) - same ideas, new
lap rule (DD-31). Python is the reference; C must match exactly (same rounding: `round(x, 3)` = `aad_round3`).

## 1. Laps carry the AI level
Lap records used by P3 and by the race-end update may carry `"level"` = AI factor in force when the lap ended
(the DLL lap ring already has it). AI cars (the teammate) are scaled, the rated driver is not, so a teammate lap at
level L is converted to level F with the lever model:
`tm_at_F = tm + effect(F, S, cap) - effect(L, S, cap)` (effect = `aad_model.effect`, negative = AI faster;
S, cap = `aad_model.sensitivity(MODEL, model_key(track), ai_lap)` with ai_lap = the lap's own teammate time).
A lap without `level`, or with level == F, is used unchanged.

## 2. Oracle changes (`tools/aad_decide.py`)
- `fastest_half_gap(laps, track=None, level=None)`: unchanged when `level is None`. Otherwise every counted lap's
  `teammate_clean` is first converted to `level` (section 1) and then rounded with `round(x, 3)`; the rest as today.
- `race_update(store, track, race, factor_used, when, laps=None)`: laps path calls
  `laps_step(laps, track, factor_used)`, and `laps_step` passes `level=current` to `fastest_half_gap`. (Laps without
  `level` behave exactly as today, so existing tests keep passing.)
- New constants: `NUDGE_DEAD_BAND_S = 0.2`, `NUDGE_BUDGET_CAL_S = 1.5`, `NUDGE_BUDGET_RATED_S = 0.3`,
  `NUDGE_POINTS_CAL = (3, 5)`, `NUDGE_POINTS_RATED = (3, 6)` (decide when the counted-lap count first reaches a
  point; lock after the last point).
- New `nudge_step(laps, track, current, start, calibration)` -> `(new_factor, why, decided, locked)`, pure:
  1. `n` = number of counted laps (`lap_counts`). `points` = CAL or RATED tuple by `calibration`.
     If `n` is not in `points`: return `(current, "waiting", False, n >= points[-1])`.
  2. `gap, ai_lap, n_used = fastest_half_gap(laps, track, level=current)`; gap None -> `(current, why, True,
     n >= points[-1])`.
  3. `target = TARGET_PCT / 100 * ai_lap`; `err = gap - target`.
     If `abs(err) < NUDGE_DEAD_BAND_S`: hold (`decided=True`).
  4. `S, cap, _ = m.sensitivity(MODEL, model_key(track), ai_lap)`;
     `budget_s = NUDGE_BUDGET_CAL_S if calibration else NUDGE_BUDGET_RATED_S`; `budget_f = budget_s / abs(S)`;
     `step_max = budget_f / (len(points))`.
     `step = max(-step_max, min(step_max, err / S))` (driver faster: err < 0, S < 0 -> step > 0 = AI faster).
     `lo = max(0.80, start - budget_f)`; `hi = min(1.20, start + budget_f)` (the same
     clamp constants, 0.80 / 1.20).
     `new = round(min(max(current + step, lo), hi), 3)`.
  5. Return `(new, why, True, n >= points[-1])`; `why` names gap, target, err, step, budget (text need not match C).
- `calibration` (caller) = the race's locked decision tier is not "track" or it is estimated (unrated / ESTIMATED).

## 3. C port (`dll/aad_decide.c/.h`)
`AadLapRef` gains `double level` (NAN = none). `aad_fastest_half_gap(laps, n, track, level, &gap, &ai_lap)` (track
NULL / level NAN = today's behaviour). `aad_race_update_laps` passes `factor_used` as level. New
`int aad_nudge_step(const AadLapRef *laps, int n, const char *track, double current, double start, int calibration,
double *new_factor, int *locked, char *why, size_t why_n)` returns `decided` (0/1). Same constants (#define).
Harness (`dll/test/aad_replay_harness.c`, mode `decide`): `race_update` laps accept `"level"`; new op
`{"op":"nudge","laps":[...],"track","current","start","calibration"}` -> `{"factor","decided","locked"}`.
Parity test (`tools/test_aad_decide_c.py`): random nudge ops (0..12 laps, levels 0.95..1.10 incl. absent, both
calibration values, all 6 alias tracks + unknown, current/start near the clamp edges), plus race_update with levels.
Unit tests (`tools/test_aad_decide.py`): conversion math by hand, dead band, step limit, budget limit, lock at the
last point, waiting between points, calibration vs rated budgets, laps without level unchanged.

## 4. DLL integration (Claude)
In a race (session 7) with Adaptive active and the level locked for the session: after every lap record of the rated
driver, call `aad_nudge_step` on this race's lap ring (with levels) once per new counted-lap count; `decided` with a
new factor -> apply it to every AI slot except the proxy (same path as a decision; source "in-race"), log
`NUDGE ...`, record it in the live file (`level.source = "in-race"`, events). After `locked` -> no further nudges this
race. Race end: `aad_race_update_laps` with the lap levels and `factor_used` = the factor in force at the end.
Watching mode (Adaptive=0): nudges computed and logged as "would", nothing applied.

## 5. Not in scope
Practice lock on the new lap rule (P2.1), teammate outlier check (H3), race length based lock (race laps not read).
