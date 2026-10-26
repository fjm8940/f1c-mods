# DD-26 spec (2026-10-16) — comparable-teams fallback when the teammate has no laps

Owner decision DD-26 (2026-10-16 live drive, A-1 Ring 2001 practice, Prost teammate stayed in the garage, 0 laps):
when the AI teammate has no laps, compare the rated driver with the drivers of the **2 teams nearest the driver's
car in measured AI pace** instead of refusing; the teammate stays first choice; the result is ESTIMATED. Pace order:
this weekend first, then past races at this track (the store must start keeping per-team AI pace), none known ->
whole AI field, ESTIMATED.

Style as P3 / teammate-skill specs: the Python oracle is the reference, C must match exactly (same rounding
`round(x, 3)` = `aad_round3`, same evaluation order of float expressions), parity through
`dll/test/aad_replay_harness.c`. Laps / summaries without the new keys behave exactly as today.

## 1. Terms
- **team** = AI car name (veh+0x4678, `car` field); the two cars of one team share it (FACT, used by DD-01 since P1).
- **team pace (pct)** = `round(100 * (team_median - field_median) / field_median, 3)`: median of the team's clean AI
  laps minus the median of all clean AI laps of the same session, in % of the field median (negative = faster).
  Only teams with >= `TEAM_MIN_LAPS = 3` clean laps get a value. The hook scales every AI car by the same factor, so
  the pct order does not depend on the AI level or the slider (ASSUMPTION, consistent with Run 14 "same seconds per
  factor").
- **pace table** = `{car: pct}`. Three sources, best first:
  1. `weekend` — this session's own laps so far (engine-internal), else the weekend table (merge of the earlier
     sessions' summaries this weekend);
  2. `history` — the store's per-track table (past weekends), set by the caller;
  3. `field` — no usable table: the whole AI field.
  A table is **usable** when it contains the rated car AND at least one other car.

## 2. Rating engine (`tools/aad_rating_logger.py` RatingEngine; C `dll/aad_rating.c`)
Constants: `TEAM_MIN_LAPS = 3`, `COMPARABLE_TEAMS = 2`.

State (new):
- `team_laps` {car: [clean AI lap times]} and `session_ai_clean` [all clean AI lap times] — reset with the session.
- `weekend` {car: pct} — NOT reset per session (see weekend rule below).
- `history` {car: pct} — set by the caller only (`engine.history = {...}`; C `aad_engine_set_history`).

AI crossing (feed, AI loop): when the lap is clean, append its time to `session_ai_clean`, and when `car` is a
non-empty name also to `team_laps[car]`.

`own_pace()` = `{car: round(100 * (median(team_laps[car]) - F) / F, 3)}` for every car with >= TEAM_MIN_LAPS laps,
`F = median(session_ai_clean)`; empty when `session_ai_clean` is empty or F <= 0.

Pace table for a lap: the first usable of `own_pace()` (source "weekend"), `weekend` ("weekend"), `history`
("history"); none usable -> "field". The rated car = `player_car`; no player_car -> "field".

Per player lap (in `_player_lap`, after the teammate value) three new record keys, appended after
`gap_to_teammate` in this order:
- `comparable_clean`: number or None;
- `comparable_source`: "weekend" / "history" / "field" / None;
- `comparable_teams`: list of car names (nearest first), `[]` when none.

Rule, with `wc` = the clean AI laps of the same window as the teammate value and `pc` = player_car:
1. Table usable (`tbl`, `pp = tbl[pc]`): candidates = the distinct cars of `wc` with a non-empty name, `!= pc`,
   present in `tbl`; sorted by `(abs(tbl[c] - pp), c)` (C: `fabs`, `strcmp` on the Latin-1 bytes); first
   COMPARABLE_TEAMS. If there is at least one: every lap `a` of `wc` whose car is a chosen team is converted to the
   rated car's pace `adj = a.lap_time * (100.0 + pp) / (100.0 + tbl[a.car])` (exactly this order:
   multiply, then divide), `comparable_clean = round(median(adj), 3)`, `comparable_source` = the table's source,
   `comparable_teams` = the chosen cars.
2. Otherwise (no usable table, or no chosen team has a lap in the window): if `ai_median_clean` is not None ->
   `comparable_clean = ai_median_clean` (already rounded), source "field", teams `[]`.
3. No clean AI lap in the window -> None, None, `[]`.

Summary: new key `team_pace` (after `gap_ci_seed`) = `{car: {"pct": p, "n": k}}` for the cars of `own_pace()` at
session end, keys sorted by car name (C: strcmp), `{}` when none. C `AadSummary` gains
`int n_team_pace; AadTeamPace team_pace[AAD_MAX_TEAMS]` (`AAD_MAX_TEAMS 32`, extra cars dropped in sorted order;
`typedef struct { char car[AAD_CAR_NAME_LEN]; double pct; int n; } AadTeamPace;`). `aad_summary_to_json` /
`aad_summary_from_json` carry it (missing key = 0 teams).

Weekend rule (feed, at a session change): after the old session's summary is emitted, merge its `team_pace` into
`weekend` (`merge_pace`: every car in the summary replaces its old value; other cars kept). Then after `_reset`,
when the previous session existed and (the engine track changed, or the previous session was a race (7) and the new
one is not) -> `weekend = {}` (the DLL's new-weekend rule). `close()` does not merge.

C API: `void aad_engine_set_history(AadEngine *e, const AadTeamPace *t, int n);` (copies car + pct; n <= 0 clears).

## 3. Store (`tools/aad_ratings.py`; C `dll/aad_store.c`)
Top-level key `"team_pace": {track: {car: {"pct": p, "n": k, "utc": when}}}` (schema stays 1; old stores have no
key = empty). `record_team_pace(store, track, team_pace, when)`: for every car of the summary's `team_pace`, replace
that car's entry; others kept; does nothing when `team_pace` is empty or track is None. `team_pace(store, track)` ->
`{car: pct}` (empty when none). C: `int aad_store_record_team_pace(AjNode *store, const char *track,
const AadSummary *s, const char *when)` and `int aad_store_team_pace(const AjNode *store, const char *track,
AadTeamPace *out, int max)` (returns count, keys in stored order). The slider is not part of the key (section 1).

## 4. Decision layer (`tools/aad_decide.py`; C `dll/aad_decide.c`)
Lap records may carry `comparable_clean` and `comparable_source`.
- `lap_counts_cmp(r)`: as `lap_counts` but needs `comparable_clean` (not `teammate_clean`).
- Comparable time at a level: `comparable_at_level(r, track, level)` = the P3 level conversion applied to
  `comparable_clean` (the comparable cars are scaled AI like the teammate); **no** teammate-skill correction (DD-33
  is about one teammate's RaceAbility).
- `reference_mode(laps)` -> ("teammate", n) when the teammate-counted laps >= LAPS_MIN or the comparable-counted laps
  < LAPS_MIN; else ("comparable", n_cmp).
- `reference_gap(laps, track=None, level=None)` -> `(gap, ai_lap, n_used, ref)`: teammate mode = exactly
  `fastest_half_gap` with `ref = "teammate"`; comparable mode = the same fastest-half arithmetic on the
  `lap_counts_cmp` laps with `comparable_at_level`, `ref` = the weakest source among the used laps (order
  weekend < history < field; a lap without a known source counts as "field").
- `laps_step`: uses `reference_gap`; `estimated = n_used < LAPS_RATED or ref != "teammate"`; `why` names the
  reference ("comparable teams (weekend)" / "comparable teams (past races)" / "whole AI field").
- `nudge_step`: `n` and the gap come from `reference_mode` / `reference_gap` (teammate laps only = unchanged).
- `lock_level(store, track, slider, practice, factor_in_practice, practice_laps=None)`: as today; when the summary
  step gives no factor (refused or practice None) and `practice_laps` is given -> `laps_step(practice_laps, track,
  factor_in_practice, alpha=1.0)`; accepted -> tier "practice", `estimated` = laps_step's flag, smoothed 50/50 with
  a track rating as today, why "from your practice laps: " + why.
- C: `AadLapRef` gains `double comparable; int cmp_src;` (`cmp_src` 0 = none — zero-initialised is safe —,
  1 weekend, 2 history, 3 field; `comparable` used only when `cmp_src` >= 1 and not NAN). New
  `int aad_lap_counts_cmp(const AadLapRef*)`, `int aad_reference_gap(laps, n, track, level, &gap, &ai_lap, &ref)`
  (ref 0 = teammate, else the weakest cmp_src; returns n_used), `int aad_reference_count(laps, n, &ref)` (the
  `reference_mode` count). `aad_lock_level` gains `const AadLapRef *laps, int n_laps, int have_laps`.
  Harness lap objects accept `comparable_clean`, `comparable_source`; op `lock` accepts `laps`.

## 5. DLL (`dll/adaptive_ai_difficulty.c`, Claude)
- Weekend start: `aad_engine_set_history(engine, store team_pace(track))`.
- Every session summary with `n_team_pace > 0`: `aad_store_record_team_pace` + save (log `TEAM PACE`).
- Lap ring: `comparable`, `cmp_src`, teams text; live / record laps gain `comparable`, `comparable_source`,
  `comparable_teams`; `used` = counted by the reference in use; session_stats/balance use `aad_reference_gap` and
  name the reference (`reference`: "teammate" / "comparable: weekend|history|field").
- Practice kept for the lock = the practice session with the most reference-counted laps (summary + its laps);
  `aad_lock_level` gets those laps. Nudge counts with `aad_reference_count`.

## 6. Monitor (`tools/aad_monitor`)
Reference line: "Compared with the AI teammate in the same car." or, for a comparable reference, "Your teammate has
no laps: compared with <team A> and <team B> (nearest in AI pace, this weekend / past races) - ESTIMATED." /
"... compared with the whole AI field - ESTIMATED." Lap table: teammate column shows the comparable time marked
"(cmp)" when the teammate time is missing; Used? "Yes (vs comparable teams, ESTIMATED)". Old files unchanged.

## 7. Tests
- Python unit (`test_aad_rating_logger.py`): own pace, nearest-2 choice incl. ties by name, conversion arithmetic,
  weekend merge + reset rule, history fallback, field fallback, None without AI laps, summary `team_pace`.
- Parity (`test_aad_replay_c.py`): the synthetic races get sessions without teammate laps (teammate car pits for
  the whole session / retires early) and a history table (harness `replay` optional 4th arg = history JSON
  `{car: pct}`; Python `aad_trace.replay(path, close_utc, history)`); records must be identical.
- `test_aad_decide.py` / `test_aad_decide_c.py`: reference_mode / reference_gap / laps_step / nudge / lock with
  comparable laps (random sources incl. absent, mixed teammate/comparable lap lists); old lap lists unchanged.
- `test_aad_ratings.py`: record_team_pace replace/keep, empty no-op, team_pace lookup; C store op parity.
- Replay of saved traces (A-1 Ring practice pid28060, proxy races pid15980 / pid18576): comparable reference
  present when the teammate is missing; where both exist, the comparable gap vs the teammate gap = accuracy
  evidence (recorded in the build record).

## 8. Not in scope / limits
Teammate outlier check (H3); DD-27 qualifying-to-race step; a session-weighted store average (latest value per team
wins); AI driver skill of the comparable teams (DD-33 corrects only the teammate). At a race start the reference can
switch from comparable to teammate once the teammate has 3 counted laps (the nudge only runs on a new count).
