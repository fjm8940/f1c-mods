# P1 build record (2026-10-16) — DLL 0.2.0-dev: C rating engine + store, log-only

Spec: [P1_RATING_PORT_SPEC_2026-10-16.md](P1_RATING_PORT_SPEC_2026-10-16.md). Phase P1 of
[WIP_RELEASE_SPEC_2026-10-16.md](WIP_RELEASE_SPEC_2026-10-16.md). Status: **built, tested, installed; STOP for the owner drive.**

## Who did what
Owner asked mid-task (2026-10-16): "task haiku agents to do majority of the work". Claude wrote the spec and the four headers
(`dll/aad_json.h`, `aad_rating.h`, `aad_store.h`, `aad_trace.h`), the DLL integration (`adaptive_ai_difficulty.c`,
`build.bat`, ini, meta) and this record; five Haiku agents ran in parallel:
| Haiku | Files | Their own check |
|---|---|---|
| 1 | `dll/aad_json.c` | Python json parity on samples, 15 invalid inputs rejected, depth 64 |
| 2 | `dll/aad_rating.c` | round3 sweep 158,013 values = Python; MT seed 0 = Python; 14,000-sample replay identical |
| 3 | `dll/aad_store.c` | compile only (others not ready) |
| 4 | `dll/aad_trace.c`, `dll/test/aad_replay_harness.c`, `dll/test/build_harness.bat` | trace round trip bit-exact; record [P1_HAIKU4_TRACE_HARNESS_RECORD_2026-10-16.md](P1_HAIKU4_TRACE_HARNESS_RECORD_2026-10-16.md) |
| 5 | `tools/aad_trace.py`, `tools/test_aad_replay_c.py` | 10 tests OK |
Claude review: store `record/overall/lookup` read line by line against `aad_ratings.py` (match); `aj_set` frees the replaced
value (store relies on it, confirmed); engine bounds (slots outside 0..63 dropped, OOM drops data); test coverage checked.

## Verification (Claude re-ran everything, 2026-10-16, game not running)
- `dll\build.bat`: clean under `-Wall -Wextra -Werror`; two builds same SHA-256 (reproducible). Export `rfe_1d3b7a91`, imports
  KERNEL32 + msvcrt only. Harness `dll\test\build_harness.bat`: clean, exe in `%TEMP%\aad_replay_harness\`.
- `python -I -B tools\test_aad_replay_c.py`: **10 tests OK** (13.6 s). C vs Python oracle, exact equality after JSON load:
  16 logger scenarios (every reason incl. frozen, out-of-range, lap below 2, session change, pause, jump); 30 seeded 22-car
  float32-quantized races = 411 player laps (90 clean; reasons: traffic 151, off_track 86, cut 59, yellow 58, pit 52,
  partial 50, jump 45; 245 laps with a teammate reference; 29 summaries with teammate CI); ~90 store ops + lookups/overall
  and the saved file; 2000 double formats; 5000 round3 incl. exact ties; 200 MT + 200 randbelow.
- Old Python tests still pass: rating_logger, ratings, model (32), live_controller, import_racedata.
- NOT verified: anything in game. The DLL read path (`ada_read_sample`), the AIW track key, session/summary timing, the
  process-exit summary and the store file written beside the DLL are untested live — that is the owner drive.

## Facts vs inferences
- FACT (prior art, not re-run): `0x008AC438` = loaded track .AIW path (findings.jsonl line 150, live 2026-07-28 / 2026-08-03).
  Track key = AIW stem, e.g. `2001_A-1_Ring`. Lever-model keys (`austria_2001`) need an alias table (P2).
- INFERENCE: summary at game exit relies on `DLL_PROCESS_DETACH` (plugins are never unloaded); skipped if the worker was awake
  at that moment. If the race-end -> menu flow does not change session type 0x7C56A4, the summary only arrives at exit.
- Spec amendment (Haiku 2): `aad_round3` uses Dekker only below 2^40, `%.3f` between 2^40 and 2^52, identity above; lap data
  never reaches those ranges.

## Installed (2026-10-16, game closed)
`F1 Challenge V10\plugins\lightloader\`: DLL `671d984d85b163e32965c2aad604bf4c978bfe66b502f0d03ef1fae02edb9c66`, ini
`e957d7c9...` (`Enabled=0`, `AllAIFactor=1.000`, `RatingLog=1`, `RatingTrace=1`, `RatingIntervalMs=500`), meta `e185a75f...`.
Effect: AI is vanilla (nothing patched); the DLL only reads the race and logs. Before-state: `%TEMP%\aad_installed_0.1.1_backup_20261016\`
and `dll\build_prev_89e2f7ce\` (0.1.1-dev DLL). Restore = copy those three files back.

## Owner drive (P1 STOP) — what to check after 2 offline races
1. Log `adaptive_ai_difficulty_log.txt`: `RATING: log-only mode`, `RATING trace: ...`, `RATING lap:` lines, `RATING summary`,
   `RATING store: <track> recorded`, `would use factor ... NOT applied`.
2. `adaptive_ai_difficulty_rating.jsonl` + `adaptive_ai_difficulty_ratings.json` beside the DLL; track key correct.
3. Trace `%TEMP%\aad_trace_<utc>_pid<n>.txt` -> replay through both (`python tools\aad_trace.py replay TRACE` vs the harness)
   must match; keep a short trace excerpt as a fixture (retention: not the whole file).
