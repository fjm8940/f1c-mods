# Adaptive AI Difficulty - dll (v0.2.0-dev)

Development build, hook only, no rating yet. Nothing here is copied into the game folder, production, or any other mod. Install and test steps below are for the owner to run.

Spec: [../docs/HOOK_SPEC_2026-10-15.md](../docs/HOOK_SPEC_2026-10-15.md). Mod standard: [../../MOD_STANDARD.md](../../MOD_STANDARD.md). Template: `ai_brain_lab\dll`.

## What v0.1.0-dev does
- Retargets the 5-byte `call sub_4F1510` at `0x424AA6` (in AIDriverStructInit) to a naked stub. Same length, no trampoline.
- Stub: ESI = vehicle base. Slot = (veh - 0x5EC6D8) / 0x48C0. Slot 0, the player car, human cars (veh+0x32C8 != 1) and any pointer mismatch pass the native float through unchanged. AI cars get `native x factor[slot]`. Tail-jumps to `0x4F1510` with ECX intact, so the callee's `ret 4` is unchanged.
- Factors default to 1.0. Read from the ini at load and polled every 2 s. Clamped to 0.80..1.20.
- Live apply: an ini change posts a game-thread task through the shared broker (`f1c_broker.h`, post). The task re-calls `sub_4F1510(P, N_k * factor)` for each AI car, where N_k is the last native value the stub saw for that slot.
- Restore: at unload, the task re-calls `sub_4F1510(P, N_k)` for each car whose live divisor still equals our own write. The 5 original bytes then go back to `0x424AA6`. If the restore does not finish in 3 s, the hook stays and the module is pinned.
- Session guard: writes happen only when the session state is 0 (offline). Multiplayer, CO-OP, and replay are read-only and pass through.
- Pace baseline `0x44F4` is not written (spec section 5, D-PACE owner decision pending).

## Refusal paths (each writes nothing)
| Code | Condition |
|---|---|
| AAD-H | Lightloader host proof fails (magic, ABI, product id, exclusive capability, generation, PID, seal, proof) |
| (no patch) | `Enabled` is not `1` in the ini (default `0`, vanilla) |
| AAD-P2 | Bytes at `0x424AA6` are not `e8 65 ca 0c 00` (foreign hook) |
| AAD-P1 | 37-byte block at `0x424A90`, 10-byte prologue at `0x424790`, or 16-byte callee at `0x4F1510` mismatch, or the call does not target `0x4F1510` |
| AAD-P3 | Stub outside rel32 range of `0x424AA6` |

Per-call pass-through reasons are counted in the stub and logged by the worker: `range`, `slot0`, `ptr_mismatch`, `player`, `not_ai`, `bad_native`, `not_offline`.

## Build
Requires the 32-bit MinGW toolchain at `C:\msys64\mingw32\bin` (gcc and windres). Same as `ai_brain_lab\dll\build.bat`.

```
cmd /c development\mods\adaptive_ai_difficulty\dll\build.bat
```
Flags: `-O2 -Wall -Wextra -Werror -std=gnu11`, `-shared -static-libgcc -Wl,--no-insert-timestamp` (reproducible PE). Intermediate objects go to `%TEMP%\adaptive_ai_difficulty_build`.

Last build (2026-10-15), output:
```
Built %USERPROFILE%\Desktop\F1\development\mods\adaptive_ai_difficulty\dll\adaptive_ai_difficulty.dll
```
No warnings. One fix was needed during the build (a `snprintf` truncation warning in `ada_path`, replaced with a length-checked copy).

## Files and hashes (sha256)
| File | sha256 |
|---|---|
| `adaptive_ai_difficulty.dll` | `89e2f7ce295a9de5c4cbe6ce98467cebacacf7e13538faa4d08c5e8fab94c0ff` |
| `adaptive_ai_difficulty.c` | `ecfee46569bf773da491424f131767e70311032bf8dad56dfc7a36c768bfaf02` |
| `adaptive_ai_difficulty.ini` | `8002ec08b10012185fbe47ec52bd0a85d5aad5032092a9648081f1a1496faaed` |
| `adaptive_ai_difficulty.meta.json` | `c76727cf26c2f3bc9cc8ea013e0e0b8da16366bd408e603b85905a259ff4f9e2` |
| `build.bat` | `ada9790d2ca97e59a189f198f8e3b04eedbe1a2c01120794ad186e2de736560b` |

Stub disassembly (checked with objdump): `push ecx; push esi; push 0xc(%esp); push esi; call _ada_strength_hook; add esp,8; fstps 0xc(%esp); pop esi; pop ecx; mov eax,0x4f1510; jmp *eax`. Export `rfe_1d3b7a91` present.

## ini keys (`adaptive_ai_difficulty.ini`, beside the DLL)
- `[Adaptive]`
- `Enabled` = `0` or `1`. Read once at load. `0` (default) means no patch, vanilla behaviour. Changing it needs a restart.
- `AllAIFactor` = factor for every AI slot without its own key. Default `1.000`.
- `Slot<N>` = factor for slot N (1..29). Overrides `AllAIFactor`. `Slot0` is ignored.
- Values are clamped to 0.80..1.20. A non-number falls back to `AllAIFactor` and is logged.
- Factors are polled every 2 s and applied live.

## Log
`adaptive_ai_difficulty_log.txt` beside the DLL. Written by the worker and at activation only (capped at 4000 lines). Lines: START, INI, COMPAT/REFUSED, HOOK installed, ACTIVE, FACTOR, APPLY, RESTORE, STUB status, SESSION GUARD, BROKER, UNLOAD.

## How to test (owner runs these; the agent did not run the game)
1. Static: confirm the 37-byte block at `0x424A90` of the test exe matches the spec section 8.1 bytes (the DLL does this at load).
2. Clean test install only (not the live game). Copy `adaptive_ai_difficulty.dll`, `.ini` and the `.meta.json` into the plugin folder, with the ini set to `Enabled=1`.
3. Acceptance A3: `Enabled=1`, all factors 1.000. Lap times and the log must match a run without the DLL.
4. Acceptance A5: set `Slot5=1.100` mid-race. The log should show APPLY for slot 5 within about 2 s. The car's B value should change.
5. Acceptance A7: the player car is unchanged. The log should show `player` or `slot0` pass-throughs for it.
6. Acceptance A8: set factors back to 1.000. Check RESTORE lines. On unload the call site must read `e8 65 ca 0c 00`.
7. Refusal R-2: patch a copy of the exe so `0x424AA6` differs. The log must show `REFUSED AAD-P2` and no hook.
8. Three race loads in one process; check the log and startup ms.

## Open items and deviations (not implemented or not verified)
- **Spec 9.1 (x87 depth at the call site) CLOSED 2026-10-15:** capstone decode shows depth 0 at `0x424AA6` (finding `aad-hook-x87-depth-zero-2026-10-15`, `../docs/HOOK_VALIDATION_PLAN_2026-10-15.md` V0).
- **Spec 9.3 (sim thread identity, broker custom-task path) is OPEN.** Live apply uses `f1c_broker_post` with the game-thread callback. The spec says confirm this before batch C.
- **Live apply path differs from the template.** The request said to use the template's per-frame hook, which is the AIMainUpdate call at `0x42C748`. Spec section 4 forbids that anchor, so the broker is used instead.
- **Ini key names differ from the spec.** The spec proposes `StrengthMult_<k>`; this build uses `AllAIFactor` and `Slot<N>`, as requested.
- **Clamp range differs from the spec.** The spec proposes 0.5..1.5 (OPEN). This build clamps to 0.80..1.20, as requested.
- **Enabled default.** The spec says `Enabled=0` by default. The shipped ini has `Enabled=0`. The owner must set it to 1 to test.
- **Pace 0x44F4 not written** (spec section 5 default `p = 1.0`).
- **Restore depends on the broker.** If the game never runs a frame, restore cannot complete and the module stays pinned.
- **Session generation** (spec R9) is only handled by the offline/not-offline transitions. A stale N_k could be applied before the first re-init of a new race. Low risk, not tested.
- **Not implemented, dev-only exceptions:** usage report (`f1c_usage.h`), crash capture, and release metadata/DRM. Logs are local only.
- **Not done:** no runtime or unit test ran (per instruction, no game run). Refusal and pass-through paths are untested.
- **Records outside this folder are pending** (read-only scope): the index line in `../README.md`, `findings.jsonl` entries (spec section 11), and `ADDRESS_INVENTORY.md`.

## v0.1.1-dev (2026-10-16): per-process ini override for parallel tests
- New: environment variable `AAD_INI` = absolute path of an ini used INSTEAD of the shared
  `adaptive_ai_difficulty.ini` (Enabled and factors). Read once at start; ignored unless it names an existing file.
  Logged as `INI: per-process override AAD_INI=<path>`. Without the variable the DLL behaves exactly like 0.1.0-dev.
- START line now carries the process id (`START ... pid <n>`), so lines from parallel instances can be told apart.
- Purpose: `tools/aad_night_launcher.py --plan factor93` gives each of 8 parallel games its own factor; the shared ini
  is never touched in that mode, so games already running are unaffected.
- Build: no warnings under -Werror; two builds give the same sha256 (reproducible). Installed into
  `F1 Challenge V10\plugins\lightloader\` 2026-10-16 (byte-equal to the build). The 0.1.0-dev DLL (495d7eb0...) and
  its meta are kept in `%TEMP%\aad_installed_0.1.0_backup\`; the pre-change sources in `%TEMP%\aad_dll_before_0.1.1\`.
- Verified: build, strings, import of GetEnvironmentVariableA, export `rfe_1d3b7a91`. NOT yet verified live: the
  override path (first factor93 batch is the test; the launcher refuses a game whose log lacks the override line).

## v0.2.0-dev (2026-10-16): log-only rating engine (WIP phase P1)
Spec `../docs/P1_RATING_PORT_SPEC_2026-10-16.md`; record `../docs/P1_BUILD_RECORD_2026-10-16.md`.
- New sources (compiled with `-msse2 -mfpmath=sse -ffp-contract=off` for bit-exact doubles): `aad_json.c/.h` (JSON DOM),
  `aad_rating.c/.h` (port of `tools/aad_rating_logger.py` RatingEngine), `aad_store.c/.h` (port of `tools/aad_ratings.py`),
  `aad_trace.c/.h` (sample trace). Test harness `test/aad_replay_harness.c` + `test/build_harness.bat` (exe in %TEMP%).
- New ini keys (read once at load): `RatingLog` (1 = rate, default 0), `RatingTrace` (1 = `%TEMP%\aad_trace_<utc>_pid<n>.txt`),
  `RatingIntervalMs` (500, 100..2000). `Enabled=0` + `RatingLog=1` = no patch, no broker, worker reads only, entry returns TRUE.
- Worker samples only while the session guard reads offline. Track key = stem of the AIW path at `0x008AC438`.
- Output beside the DLL: `adaptive_ai_difficulty_rating.jsonl` (lap + summary records, Python field names),
  `adaptive_ai_difficulty_ratings.json` (schema 1 store; race sessions with >= 1 clean lap; factor_new = factor used,
  i.e. nothing decided), RATING lines in the log incl. the factor the ladder WOULD use (not applied).
- Summary at session change, unload, or best effort at process exit (skipped if the worker was awake).
- Build 2026-10-16: clean under -Werror, reproducible, sha256 `671d984d85b163e32965c2aad604bf4c978bfe66b502f0d03ef1fae02edb9c66`.
  Superseded 0.1.1-dev DLL kept in `build_prev_89e2f7ce/`. Not verified in game yet (owner drive).

## v0.3.0-dev (2026-10-16): weekend decision + session conclusion (WIP phase P2), decisions logged only
Spec `../docs/P2_APPLY_SPEC_2026-10-16.md`; record `../docs/P2_BUILD_RECORD_2026-10-16.md`.
- New sources (rating flags): `aad_decide.c/.h` (port of `tools/aad_decide.py`: weekend start via the ladder, lock at
  qualifying from practice, race update into the store; 6 lever tracks by AIW-stem alias, other tracks S = -0.307 x lap),
  `aad_conclusion.c/.h` (plain-language session report + one-line "AI LEVEL NEXT RACE"). Harness modes `decide`,
  `conclusion`, `levertable`; test `tools/test_aad_decide_c.py`.
- New ini key `Adaptive` (read once, default 0). The decision sets every AI slot factor only with `Enabled=1` and
  `Adaptive=1`; otherwise logged as `DECISION ...: would apply ... NOT applied`.
- Worker weekend state: new weekend = track changed or a non-race session after a race; `WEEKEND start` decision at the
  first offline sample; practice summaries kept (most teammate laps); lock once on entering qualifying (5) or race (7);
  race summary -> `aad_race_update` (store record, replaces the P1 log-only record). Every summary writes
  `adaptive_ai_difficulty_last_session.txt` beside the DLL + a `CONCLUSION:` log line. `SESSION type a -> b` logged.
- Build 2026-10-16: clean under -Werror, reproducible, sha256 `597d4aaab90a7ecd9ff973c4c124b9320ba371d0aba46bf0a9793a5892b81b5a`.
  Superseded 0.2.0-dev kept in `build_prev_671d984d/`. Installed (Enabled=0, Adaptive=0). Not verified in game yet.

## v0.3.1-dev (2026-10-16): ProxyDriver test mode (dev only)
Owner test (2026-10-16): an AI driver "Human Driver" (2002 roster, `SeasonData\Drivers\2002\HDriver.rcd`, owner-made,
RaceAbility 0.0, Passing 100, Prost) plays an over-fast "human"; the field must adapt to him.
- New ini key `ProxyDriver` (read once; empty = off). The worker finds the AI car whose driver name (veh+0x4620, SDK reader
  recipe) equals it (case-insensitive, trimmed) and remaps each sample before the rating engine: proxy -> slot 0, non-AI,
  no player-only fields; the real player car -> the proxy's slot number, non-AI (not rated, not a teammate; still traffic).
  Rating engine, decision code and the Python oracle are unchanged (C/Python parity untouched).
- The proxy slot is never scaled: stub and apply loop use factor 1.0 for it; any earlier write is restored to native.
- Log: `INI: ProxyDriver=...`, `PROXY: rating AI driver ... (slot N, car ...)` / `PROXY: ... not in this session`.
- Build 2026-10-16: clean under -Werror, reproducible, sha256 `4ff57971f22761ea87a109fd42a360177173b1d7cbc79b211195e95809053002`.
  Superseded 0.3.0-dev in `build_prev_597d4aaa/`. Not run in game yet. Unit tests not re-run: the harness does not
  compile `adaptive_ai_difficulty.c` (no code it tests changed).

## v0.3.2-dev (2026-10-16): live monitor file + session record (owner request)
Spec `../docs/LIVE_MONITOR_SPEC_2026-10-16.md` (sections 1 and 5).
- Worker writes `adaptive_ai_difficulty_live.json` beside the DLL once per second (atomic tmp + MoveFileEx), schema
  `aad.live.v1`: state (off / watching / active), when, session, rated driver (player or test proxy), AI level + source +
  why, estimated AI seconds per lap (`aad_effect`), all laps of the session with the AI level at each lap, stats,
  balance verdict (+-0.3 s around the Realistic target; PROPOSAL thresholds) with positions, next-race level, each AI
  car's native strength and the mod's change, last 15 notable log events.
- Every session end with rated laps appends one line to `adaptive_ai_difficulty_sessions.jsonl` (`aad.session.v1`:
  same body without cars/events, plus outcome). This is the planned WIP upload payload (DD-17). Nothing is sent.
- Player name never written (DD-22); `rated.name` only for the proxy. Strings Latin-1 -> UTF-8.
- Proxy is now found by driver name alone (live 2026-10-16: the AI-control byte veh+0x45F4 reads 0 on the grid, so
  0.3.1 found the proxy only after the start).
- Fix: `why` in `ada_rating_summary` initialised (was read uninitialised for a race without clean laps in the new record).
- Verified offline: test exe including the DLL source (`%TEMP%\aad_p21\livetest\t.c`) wrote live / record / empty
  bodies; all three parse as JSON (quotes, backslash, Latin-1 car names). Build clean under -Werror, reproducible,
  sha256 `31ba8b634c22ba46a59e0359c93531ceaf3cb98aed470ed9000848c9e3b716cd`. Installed 2026-10-16 (game closed) with the
  proxy TEST ini (unchanged `414257757f93...`); meta `64adb8bb...`. 0.3.1 kept in `build_prev_4ff57971/` and
  `%TEMP%\aad_installed_0.3.1_backup_20261016\`. NOT verified live yet.

## v0.3.3-dev (2026-10-16): best-laps fallback (DD-28) + realistic AI strength (DD-29)
- Race end: when the clean-lap step refuses, `aad_race_update_laps` compares the best 3 laps with the teammate's best 3
  (traffic allowed; pit / first lap / cut / time jump excluded), ESTIMATED, smoothing 0.5. Races with laps but no clean
  lap are now stored. Oracle `tools/aad_decide.py` (`best_laps_gap`, `race_update(..., laps)`); harness `race_update`
  op takes optional `laps`; parity test generates random lap lists (all reasons, None teammate laps, ties).
- Live file / session record `balance` gains `slider`, `realistic_slider`, `realistic_slider_edge`, basis text: slider
  where the native AI would put the driver at the target (measured A-1 Ring 2001 slider ladder, scaled by lap; estimate).
- Tests 2026-10-16 (game closed): replay_c 10, decide_c 8, decide 30 (4 new incl. race-1 numbers: gap -0.49 s -> 1.006),
  ratings 9, rating_logger 19, model 32, live_controller 24, import_racedata 25 - all OK. Writer test exe: realistic
  slider 94 for gap -0.80 s at slider 93. Build reproducible, sha256 `b752c9b2f650f35b75d1e752e7792b904bbb4348eed274d2e0876701d56b531b`;
  installed (game closed) with the proxy TEST ini; meta `a3234d10...`. 0.3.2 kept in `build_prev_31ba8b63/`. NOT verified live.

## v0.3.4-dev (2026-10-16): DD-31 fastest-half rule (traffic gate removed from the decision)
- Race decision `aad_race_update_laps(..., have_laps=1)`: every real lap counts, fastest half of the driver's counted
  laps vs the teammate's fastest half; >= 5 laps rated, 3-4 ESTIMATED, fewer refused. Replaces the DD-28 best-3 patch.
  Oracle `tools/aad_decide.py` (`lap_counts`, `fastest_half_gap`, `laps_step`, `race_update(laps=...)`).
- Live file / record: lap `used` (the rule), `session_stats.used/gap_fastest_half/need_used/min_used`, balance on the
  same rule. Monitor "Used?" follows `used` (traffic laps show "Yes (close behind another car)").
- Tests 2026-10-16 (game closed): replay_c 10, decide_c 8 (random lap lists up to 24 laps incl. empty lists),
  decide 31 (5 new), ratings 9, rating_logger 19, model 32, live_controller 24, import 25, monitor 144 - all OK.
  Replay of the 3 proxy races: Prost -0.3 % (3 laps, ESTIMATED), Arrows (toolkit from lap 4) +0.4 %, Arrows (toolkit
  from start) +2.7 %. Live race C was stored by 0.3.3 as 1.032 (DD-28 rule); kept (new rule 1.027).
- Build reproducible, sha256 `1483ac50c9669e067559b2014ec49b95806ce7295d6c796864b78462190b4f2e`; installed (game closed),
  proxy TEST ini unchanged; meta `2894482c...`. 0.3.3 in `build_prev_b752c9b2/`. NOT verified live.
- Live 0.3.3 race C (2026-10-16-15:56): proxy found on the grid; live file + session records written; race stored
  2002_A-1_Ring factor 1.000 -> 1.032. Known: teammate position 0 on the grid (AI-control byte, same cause as the proxy).

## v0.3.5-dev (2026-10-16): teammate position by car name + monitor wording
- `ada_positions` finds the teammate by car name (not the AI-control byte, 0 on the grid); in proxy mode the real
  player car (it carries the proxy's slot number) is skipped. Writer test: position 2 / teammate 1 as expected.
- Monitor (`tools/aad_monitor`): "Used?" never shows "close behind another car" / yellow / off-track as a reason
  (DD-31: they do not block a lap); an uncounted lap without an excluding reason reads "no teammate lap to compare
  yet". Pre-DD-31 files keep the old wording. Monitor tests 146 OK.
- Live race 2 attempt (0.3.4, 16:00-16:04): weekend start APPLY 1.032 (tier track) to all 20 AI, proxy slot 5 never
  applied (0 APPLY lines), proxy led lap 1 then 3rd - toolkit was NOT applied (owner), race aborted; store kept 1.032
  (1 counted lap < 3, n_races 2).
- Tests: replay_c 10, decide_c 8, decide 31 OK. Build reproducible, sha256
  `60008a0321681c8ae46da77acbf041d9ce7ab8b896941b8d98e913addfc48e1c`; installed (game closed); meta `7ccbdbef...`.
  0.3.4 in `build_prev_1483ac50/`.

## v0.4.0-dev (2026-10-16): P3 in-race adjustment (DD-32)
Spec `../docs/P3_INRACE_SPEC_2026-10-16.md`. Oracle + C port + harness op `nudge` + parity tests by a Haiku agent,
reviewed by Claude; DLL wiring by Claude.
- `ada_nudge_tick` after every rated-driver lap in a race (session 7, level locked): on each new counted-lap count calls
  `aad_nudge_step`; points 3/5 with budget 1.5 s/lap when the race level is not a firm track rating (calibration,
  DD-24), points 3/6 with 0.3 s/lap at a rated track (DD-06); dead band 0.2 s; applied through `ada_apply_decision`
  ("in-race", every AI slot, proxy never); `NUDGE` log lines; locked after the last point.
- Lap ring stores the level really in force (`ada_factor_in_force`); race-end rating converts teammate laps to the
  final level (lever model), live/record gap uses the same conversion.
- Monitor: source "in-race" = "adjusted during this race from your laps".
- Verified offline: DLL-level test exe feeding race-2 laps: 1.032 -> 1.039 at 3 counted laps, -> 1.046 at 6, locked;
  pit lap not counted. Oracle simulation on saved races: race 2 -> 1.046 in race, 1.056 next race; race C
  (calibration) 1.000 -> 0.990 at 3 laps (stuck behind teammate) -> 1.000 at 5, next race 1.027. Weakness: the first
  check right after the start can misjudge a bunched field.
- Tests: replay_c 10, decide_c 8, decide 46, ratings 9, logger 19, model 32, live_controller 24, import 25, monitor
  146 - all OK. Build reproducible, sha256 `1f891617f8f8807d9b296cc4a257363e6a984e495b3a525ec9423951f3ef54a4`.
  NOT installed yet (game running), NOT verified live.

## v0.4.1-dev (2026-10-16): teammate-skill correction (DD-33)
Spec `../docs/TEAMMATE_SKILL_SPEC_2026-10-16.md`. Oracle + C + parity by Haiku (53 / 8 / 10 OK, no mismatch); DLL by
Claude: RaceAbility per slot from veh+0x44C4 each sample (unknown = NAN, also at startup), `ada_lap_skill` attaches the
teammate's RA and the median RA of the other AI cars to every lap; lap fills pass `tm_ra`/`ref_ra`; live/record laps
carry `teammate_ra`, `ref_ra`. DLL test exe: Arrows teammate 3.35 vs field median 2.60 attached, nudges as before.
Live 0.3.5 race 2 result (before this build): 17/21 laps counted, gap -0.89 s (race C -1.43 s), next race 1.047.
Build reproducible, sha256 `8a8b5ce2cd7e691fffc17751b0dae74da3f61f1bc24ef60e0b363a24c72c0e02`; installed (game
closed); meta `7a4967fd...`. 0.4.0 in `build_prev_1f891617/`. NOT verified live. DD-26 (comparable teams) not built.

## v0.4.2-dev (2026-10-16): DD-26 comparable-teams fallback
Spec `../docs/DD26_COMPARABLE_TEAMS_SPEC_2026-10-16.md`. Python oracles by Claude; C ports, harness ops and parity tests
by Haiku agents, reviewed by Claude; DLL wiring and monitor balance wording by Claude.
- Rating engine (`aad_rating.c` = `aad_rating_logger.py`): per-team AI pace (% vs field median, teams with >= 3 clean
  laps); every lap record gains `comparable_clean` / `comparable_source` (weekend / history / field) /
  `comparable_teams` = the 2 teams nearest the rated car in AI pace with a lap in the same window, their laps scaled
  to the rated car's pace; table order: this session, this weekend (merged session summaries), past weekends (store),
  else the whole AI field. Summary key `team_pace`. Weekend table reset like the DLL weekend rule.
- Decision (`aad_decide.c` = `aad_decide.py`): `reference_mode` / `reference_gap` - teammate while it has >= 3 counted
  laps, else the comparable reference (always ESTIMATED); used by the race-end rating, the in-race nudge (count by the
  reference in use) and the practice lock (`aad_lock_level` gets the kept practice laps when the summary step refuses).
- Store (`aad_store.c` = `aad_ratings.py`): top-level `team_pace[track][car] = {pct, n, utc}` (latest value per team),
  written after every session summary with team pace (`TEAM PACE` log line); weekend start passes it to the engine.
- Live file / session record: laps `comparable`, `comparable_source`, `comparable_teams`; `used` = counted by the
  reference in use; `session_stats.reference` / `reference_teams`; `balance.reference`, basis text names it. Monitor
  names the comparable teams and marks ESTIMATED.
- Tests 2026-10-16 (game closed): decide 74, decide_c 8 (~12,600 random ops incl. comparable laps), ratings 15,
  rating_logger 39, replay_c 12 (synthetic races: teammate never clean, practice prefix, ties, history; all three
  sources reached), model 32, live_controller 24, import 25, monitor 178 - all OK.
- Real traces, C harness vs Python: identical comparable fields on pid28060, pid18576, pid2920, pid24972 (only the
  known pre-existing first-summary `track` before any T line differs, same with the pre-DD-26 harness). pid17600 /
  pid15980 cannot be read by the C trace reader (slot 64 proxy sentinel, pre-existing; findings
  `aad-trace-reader-rejects-proxy-sentinel-slot-2026-10-16`).
- Fallback working (DLL test exe feeding the A-1 Ring 2001 practice trace pid28060 through the DLL's own code, teammate
  in the garage): before = practice refused, lock "practice skipped" 1.000; now = practice kept "6 counted laps vs
  field", lock 1.032 ESTIMATED, 9 teams stored; second weekend with that store = "vs history" (Jaguar / Sauber /
  Minardi / Jordan), lock 1.066 ESTIMATED; live file + monitor show "compared with 2001 Jaguar and 2001 Sauber ...
  ESTIMATED". Nudge test: teammate retired after lap 2 -> comparable laps, 1.032 -> 1.039 (3) -> 1.046 (6), locked.
- Accuracy (where both exist, 3 proxy races): comparable minus teammate per lap median -0.24..+0.25 s, mean abs
  0.67-0.90 s (finding `aad-comparable-teams-vs-teammate-accuracy-2026-10-16`).
- Limits: a rated track keeps the 0.3 s nudge budget even when the reference is comparable; at a race start the
  reference can switch from comparable to teammate once the teammate has 3 counted laps; comparable drivers' skill not
  corrected (DD-33 is teammate only); car names with ',' would split in the live team list.
- Build reproducible, sha256 `39c85590a7c70fc44b0d35b4e0219ae64cee97d240a546a3bc9a518a074dbb45`; installed (game
  closed), proxy TEST ini unchanged (`414257757f93...`); meta `93042e92...`. 0.4.1 in `build_prev_8a8b5ce2/` and
  `%TEMP%\aad_installed_0.4.1_backup_20261016\`. NOT verified live.

## v0.4.3-dev (2026-10-16): RaceAbility read at any AI slider (fix from the parent session)
- Bug: `g_live_ra` inverted veh+0x44C4 with `0.8464 - 0.038473 x RA`, the slider-92 case only. Game code
  `f1c_ida_decompiled/functions/sub_424080_00424080.c` lines 155-158: value = `(22 - RA) * 0.045454547`, multiplied by
  `(slider * 0.01)^2` when the AI slider `dword_7D01A0` < 100. Live 2026-10-16 at slider 93: Raikkonen RCD 3.35 read as
  2.94. Fix in `ada_read_sample`: `sl2 = (slider/100)^2` (1.0 at >= 100), `RA = 22 * (1 - v / sl2)` for `0 < v <= sl2`.
  Affects the DD-33 teammate-skill correction (teammate RA and field median RA); 0.4.1/0.4.2 laps at slider != 92 carry
  slightly wrong RA values (median and teammate shift together, so the RA difference was off by the same scale).
- Hand check: slider 93, RA 3.35 -> v 0.73320 -> 3.350. Tests decide 74, decide_c 8, replay_c 12, ratings 15,
  rating_logger 39 OK (no tested code changed). Build reproducible, sha256
  `0803ddde678eef088e73cb05fb3126a2efae91f185c686f02ebf1937f2bb5c32`, meta `431788b6...`. 0.4.2 binary (installed)
  kept in `build_prev_39c85590/` and `%TEMP%\aad_installed_0.4.2_backup_20261016\`. Installed 2026-10-16 (game closed), ini unchanged; NOT verified live. Pre-fix source `adaptive_ai_difficulty.c.BEFORE_20261016_slider`.
