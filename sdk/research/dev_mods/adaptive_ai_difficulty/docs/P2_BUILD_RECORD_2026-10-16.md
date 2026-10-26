# P2 build record (2026-10-16) — DLL 0.3.0-dev: weekend decision + session conclusion, decisions logged only

Spec: [P2_APPLY_SPEC_2026-10-16.md](P2_APPLY_SPEC_2026-10-16.md). Phase P2 of
[WIP_RELEASE_SPEC_2026-10-16.md](WIP_RELEASE_SPEC_2026-10-16.md). Status: **built, tested, installed; STOP for ONE combined
P1 + P2 owner drive.**

## Who did what
- Haiku agents: `tools/aad_decide.py` + `tools/test_aad_decide.py` (oracle, record
  [P2_DECIDE_ORACLE_RECORD_2026-10-16.md](P2_DECIDE_ORACLE_RECORD_2026-10-16.md)), `dll/aad_decide.c`,
  `dll/aad_conclusion.c`, harness modes `decide` / `conclusion` / `levertable`, `tools/test_aad_decide_c.py`.
- Claude: headers, weekend state machine in `dll/adaptive_ai_difficulty.c` (version 0.3.0-dev, ini key `Adaptive`),
  `build.bat`, ini, meta, review edits (oracle drops `*_wet` tracks like C; conclusion prints no "+" before "slower";
  overall line in words), this record. Before-state of edited files: `*.BEFORE_20261016_p2`.
- Session 2 (after a usage-limit cut): first build failed under `-Werror` (`signed_pct_text` in `aad_conclusion.c` unused
  after the overall-line review edit) -> function deleted; nothing else changed.

## Verification (2026-10-16, game not running)
- `dll\build.bat`: clean under `-Wall -Wextra -Werror`; two builds same SHA-256
  `597d4aaab90a7ecd9ff973c4c124b9320ba371d0aba46bf0a9793a5892b81b5a` (reproducible). Imports KERNEL32 + msvcrt only;
  export `rfe_1d3b7a91`. Harness `dll\test\build_harness.bat`: clean.
- Tests (`python -I -B tools\<name>.py`, all OK): `test_aad_replay_c` 10 (P1 C vs Python, unchanged), `test_aad_decide_c` 8
  (C decide vs oracle incl. random ops, lever table == lever_model.json, conclusion phrases / no "nan" / every reason
  wording), `test_aad_decide` 26, `test_aad_ratings` 9, `test_aad_rating_logger` 19, `test_aad_model` 32,
  `test_aad_live_controller` 24, `test_aad_import_racedata` 25.
- Store-missing path read in source: `aad_store_load` returns an empty store on ENOENT, so the first weekend still gets a
  decision (tier native).
- NOT verified: anything in game — session type sequence, weekend start/lock timing, practice summary capture, conclusion
  file, `DECISION` lines. That is the owner drive.

## Facts vs assumptions
- FACT (live, findings.jsonl): SessionType `0x7C56A4` 7 = race, 5 = qualifying.
- ASSUMPTION: every other non-zero value in an offline weekend is practice / warm-up. The DLL logs each change
  (`SESSION type a -> b`) so the drive confirms or refutes it.
- KNOWN: `clamped_high` is unreachable on the 6 lever tracks (the max-gain cap trips first) — accepted.
- Installed state cannot change the AI: `Enabled=0`, `Adaptive=0` -> every decision is logged as `would apply ... NOT applied`.

## Installed (2026-10-16, game closed, by Claude per handoff step 4)
`F1 Challenge V10\plugins\lightloader\`: DLL `597d4aaa...` (above), ini `4b4a7e1e34dbf977c59aa106d777e132b5c0f45081324622637c9d48473a9d46`
(installed 0.2.0 ini + the two `Adaptive` comment lines + `Adaptive=0`; owner values unchanged: `Enabled=0`, `AllAIFactor=1.000`,
`RatingLog=1`, `RatingTrace=1`, `RatingIntervalMs=500`), meta `8adf134e2e57ebb554eda957639d8d6916b218afce1cc4bbc80af0ce5e6b78f4`.
Byte-equal to `dll\`. Before-state (0.2.0-dev: DLL `671d984d...`, ini `e957d7c9...`, meta `e185a75f...`):
`%TEMP%\aad_installed_0.2.0_backup_20261016\` and `dll\build_prev_671d984d\`. Restore = copy those three files back.

## Owner drive (combined P1 + P2) — one offline weekend (practice, qualifying, race), then a second race
P1 checks: [P1_BUILD_RECORD_2026-10-16.md](P1_BUILD_RECORD_2026-10-16.md) section "Owner drive". P2 additions in
`adaptive_ai_difficulty_log.txt`:
1. `RATING: engine started ...; Adaptive=0 -> decisions are logged only`.
2. `SESSION type a -> b` for each session — note the practice / warm-up values (confirms the ASSUMPTION above).
3. `WEEKEND start <track>, slider N` + `DECISION weekend start: would apply ...` at the first offline sample.
4. `PRACTICE kept for the qualifying lock` after practice; `DECISION locked from practice` (or `practice skipped`) on
   entering qualifying.
5. After the race: `RATING store: <track> recorded, factor used 1.000 -> next ...`, `CONCLUSION: AI LEVEL NEXT RACE ...`.
6. `adaptive_ai_difficulty_last_session.txt` beside the DLL: readable, no "nan", reasons in plain words.

## Workspace validator (2026-10-16, after these edits)
2 errors, both pre-existing and outside this task: unexpected top-level directories `.claude` (dated 2026-10-16) and
`plans` (2026-10-10). Not touched (top-level moves need owner approval).

## Owner drive in progress (2026-10-16, pid 28060, log + rating.jsonl read live, read-only)
- FACT: SessionType 1 = practice (22 cars, sim time advancing ~ AND the menu (type 1 while browsing tracks).
- BUG (harmless while log-only): menu track browsing logs `WEEKEND start` for each browsed track (1997_Jerez, Hockenheim,
  MONACO, A-1_Ring within 12 s). Fix after the drive: start a weekend only once the player car has moved / laps exist.
- Practice A-1 Ring, 2001 Prost, slider 93: laps 2-5 + 7 clean (70.86-71.27 s, median 71.10); AI median 74.2 -> 72.0 s.
  Teammate (slot 15, Prost) 0 laps (garage) -> every `teammate_clean` null -> practice lock will refuse and fall back.
  Owner decision DD-26 (comparable-team fallback) logged in DESIGN_DECISIONS.md.

## Owner drive result + ProxyDriver test install (2026-10-16)
- Drive: practice summary 5/8 clean, median 71.103, gap top5 -1.880, teammate n 0 (teammate stayed in the garage while
  the owner ran). Session types seen: 1 (menu + practice), 2 (~1 s), 5 (qualifying, 1 lap), 6 (~1 s), 7 (race). Race
  exited with 0 laps -> `race not stored`; session report file written and readable (no "nan"). Owner then chose a proxy
  test instead of driving the race; holes review `HOLES_REVIEW_2026-10-16.md`, DD-26 / DD-27 logged.
- Installed 0.3.1-dev (DLL `4ff57971...`, meta `9a5e2c14...`) with TEST ini `414257757f93...`: `Enabled=1`, `Adaptive=1`,
  `ProxyDriver=Human Driver`, `AllAIFactor=1.000`, `RatingLog=1`, `RatingTrace=1`. Effect: the hook is installed; the
  AI is scaled by the weekend decision (1.000 = native until a race at that track is rated); Human Driver never scaled.
  Before-state (0.3.0, Enabled=0 Adaptive=0): `%TEMP%\aad_installed_0.3.0_backup_20261016\` and `dll\build_prev_597d4aaa\`.
- Test plan (option A, two races, same 2002 track): race 1 at native -> race summary rates Human Driver vs the other
  Prost -> store -> back in the menu a new weekend starts with the track rating -> race 2 at the new level.
  Expect only half the computed step in race 2 (smoothing alpha 0.5); max change capped (0.041736 x lap per step,
  clamp 0.80..1.20). Needs >= 5 clean proxy laps AND >= 5 teammate laps in the same windows, CI width <= 1.0 s;
  traffic laps (> 25 % of samples within 1 s of a car ahead) do not count -> 15 laps advised.
- 2002 tracks have no lever measurement: fallback sensitivity S = -0.307 x lap (DD-25).
- After the test: restore `Enabled=0`, `Adaptive=0`, no ProxyDriver (or the 0.3.0 backup).
