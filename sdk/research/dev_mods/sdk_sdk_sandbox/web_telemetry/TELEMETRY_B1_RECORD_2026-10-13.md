# Telemetry 1.1.0, batch B1 record (writer additions and format 2.1), 2026-10-13

Product: Telemetry (`web_telemetry`), candidate 1.1.0. Pipeline stage: **implement, offline verify only**.
Spec: `TELEMETRY_LIVE_REVIEW_SPEC_2026-10-13.md` section 5 (B1), section 11 (owner answers Q1-Q7).
Status: B1 code written and built offline (DLL in `%TEMP%`). **Not installed. Not live-tested. The full gate
(`TELEMETRY_TESTS=1` with runtime_test.exe) was NOT run: the game was running.** No game file, no game folder,
no install, no deploy was touched.

Tags: FACT = read in the file named or observed in this session. INFERENCE = reasoned, needs a test.
ESTIMATE = arithmetic or synthetic measurement, not live. OPEN = needs a test or an owner decision.

## 1. Acceptance criteria, per criterion

| Criterion (spec 5, B1) | Result | Evidence |
|---|---|---|
| B1-1 round trip of new channels: RAW32 exact, Q16 within bound | PASS offline for the codec (stored values round trip exactly; Q16 quantizer itself unchanged) | `tests/storage_b1_codec_test.c`: round trips with 1, 3, 4, 5 cars, 1 to 20 ticks, all-constant blocks |
| B1-1 writer maps the game fields into the new channels | NOT VERIFIED by execution. Code written (`sw_car_from`), DLL compiles with -Werror | Gate item G2 (runtime_test `t3_b1_format21` and the existing writer tests) |
| B1-2 sim per tick: monotonic, pause detected, block t_first = first tick sim | PASS offline (round trip of sim_ms; a 10-tick sim pause shows 9 zero steps). Writer path NOT run | codec test; `t3_b1_format21` (gate G2) |
| B1-3 events valid JSON, at most 512 B, ordered by sim time | PASS offline for the shape check (valid, brackets, quotes, 512 B cap, control bytes). Writer-generated events NOT run | codec test; gate G2 checks 5 events |
| B1-4 corrupt new blocks: reason codes only, no crash | PASS offline: unknown block type (reason 3), CRC flip (4), truncation, bad JSON (5), oversize event (5) | codec test |
| B1-5 old v2 files still read | **SUPERSEDED by owner Q3.** Pre-2.1 files (minor 0) are now refused with `SV2_R_VERSION` (7). PASS offline for the refusal | codec test; header test in `runtime_test.c` `t2_u5_header_table` |
| B1-6 size per car-minute on a 10-minute 22-car file within 1.5 x estimate | NOT MEASURED LIVE. Synthetic raw (uncompressed, random values): 71,311 B per car-minute (ESTIMATE, not representative, random walks) | printed `SIZES` line of the codec test. Gate G5 |
| B1-7 capture cost p95 under 500 us | NOT MEASURED (game closed to build; live only) | Gate G5 |
| Owner live B1 (10-minute session, one 60 s pause) | NOT DONE | Gate G5 |

Overall: B1 is **not done** in the spec sense. The format and the offline codec are done. The writer path, the live
size and cost, and the live pause test are pending Claude's gate and an owner session.

## 2. What changed (files and backups)

Every edited file has a `.BEFORE_20261013_b1` copy in the same folder (restoration, section 7).

| File | Change | Backup |
|---|---|---|
| `storage_v2.inc` | Format 2.1: header minor 1 (`SV2_MINOR`), flag `SV2_FLAG_HAS_SIM` (bit 9), reader refuses minor != 1 (`SV2_R_VERSION` 7), DATA payload carries sim per tick and per-car constant-channel masks, EVENT block type 5 (`SV2_BLOCK_EVENT`, cap 512 B), `sv2_event_ok`, `last_good_t` from sim time, catalog ids 64..124 (61 channels, 125 total) | `storage_v2.inc.BEFORE_20261013_b1` |
| `storage_writer.inc` | `SW_NCH` 128; car mask 128 bits (`mask_lo`/`mask_hi`); `sw_car_from` writes ids 64..124; DATA flush writes sim_ms; `sw_write_event`; `sw_events_from_sample` (lap, pit, track-limit and contact events, writer thread); `session_start` event at open and `session_end` event at close; header flag HAS_SIM set at open; table buffer 8 KB; `channels_declared` = catalog count | `storage_writer.inc.BEFORE_20261013_b1` |
| `runtime_test.c` | Random DATA helper sets sim_ms; round trip compares sim_ms; file builder sets HAS_SIM; header minor 0/2 refusal test; channel count check uses catalog count; new writer test `t3_b1_format21` (lap at tick 60, pit at 100, contact at 120, 20-tick pause at 150-169; expects 5 events and the last sim time) | `runtime_test.c.BEFORE_20261013_b1` |
| `storage_jobs.inc` | NOT EDITED. Backup made before the batch. Its decode pool now follows `SW_NCH` (see section 6) | `storage_jobs.inc.BEFORE_20261013_b1` (identical, same SHA256) |
| `tests/storage_b1_codec_test.c` | NEW. Pure offline codec test (358 checks) | none (new file) |
| `CURRENT_STATE.md` | One index entry added as newest, above the spec entry | `CURRENT_STATE.md.BEFORE_20261013_b1` |
| `web_telemetry.c`, `director_state.inc`, `telemetry_viewer.js`, `workbench_ui.js` | NOT EDITED (TV Director scope kept out) | n/a |

No sidecar changed, so no sidecar pin was edited. `pin_sidecars.py --check` = pins OK.
No schema, API contract, README or VERIFICATION_MANIFEST.json change in B1 (see open items).

## 3. Format 2.1 layout (as written)

File = header (192 B, magic `F1CTEL02`, **major 2, minor 1**, CRC) + channel table + blocks.

- Header flags: `SV2_FLAG_HAS_SIM` (bit 9) must be set, otherwise the reader refuses with `SV2_R_HEADER`.
  A minor other than 1 is refused with `SV2_R_VERSION` (7). Pre-B1 files (minor 0) therefore show as refused.
- Channel table: 125 entries (ids 0..124). Ids 64..124 are new (catalog in `storage_v2.inc`, comments give the
  inventory row T-number). Q16 ranges are ESTIMATE (clamped and counted). RAW32 integers are exact; RAW32 floats
  are f32 bit patterns (no range claimed).
- DATA block (type 1): `n_ticks`, `n_cars`, zigzag `dt_ms[]` (wall clock, unchanged), zigzag **`sim_ms[]`** (new,
  ms relative to tick 0, sim_ms[0] = 0), then per car: slot, scope, first_tick, count, `mask_lo`, `mask_hi`,
  **`cmask_lo`, `cmask_hi`** (new: channels constant over the block, written once; the encoder derives them, the
  decoder checks the subset rule).
- EVENT block (type 5, new): raw JSON object, at most 512 B, printable ASCII; `t_first` = sim time of the event.
  The writer flushes the open DATA block first, so events stay in sim order.
- Event types written in B1: `session_start` (sim, session_type, track), `lap` (lap, prev_lap, last_lap_s),
  `pit` (state, prev_state), `incident` with kind `limits` (count) or `contact` (with), `session_end` (sim, reason).
- META (type 2), TRACK (type 6, NOT implemented), GAP (4), END (3) unchanged.

## 4. Writer behaviour (what the code does, not yet executed)

- Capture side (under `g_telemetry_cs`): `sw_car_from` copies about 61 more fields per on-track car into the ring
  slot. The ring copy is still per sample, not per channel write to disk. Event detection is NOT on the capture
  side: it runs on the writer thread in `sw_apply`, comparing each car with its previous sample.
- Writer thread: `sw_block_add` stores sim per tick; `sw_flush_block` writes `sim_ms`; events go to
  `sw_write_event` (flush, then EVENT block).
- Derived events are comparisons only. `lap` uses the game field `last_lap` at the change (INFERENCE: this is the
  completed lap time; verify live). `contact` uses the existing heuristic `contact_with` (labelled heuristic in
  web_telemetry.c, not a game flag).

## 5. Tests run and results (all offline, game running, nothing touched)

| Test | How | Result |
|---|---|---|
| Syntax and types of the DLL | `build.bat` run through a wrapper in `%TEMP%` (no TELEMETRY_TESTS), `gcc -Wall -Wextra -Werror` | PASS, RC 0, DLL built |
| Sidecar pins | `python pin_sidecars.py --check` | pins OK |
| Payload staging gate | part of build.bat (`verify_payload.py`, `verify_sidecar.py`) | OK, 5 files |
| Codec test | `gcc -std=c11 -Wall -Wextra -Werror` of `tests/storage_b1_codec_test.c`, run from `%TEMP%` | 358 checks, 0 failed |
| runtime_test.c compiles | `gcc -fsyntax-only -Werror` (NOT built into an exe, NOT run) | syntax OK |
| `build.bat` with TELEMETRY_TESTS=1 | not run (game running) | NOT RUN |
| runtime_test.exe | not built, not run (game running) | NOT RUN |
| test_telemetry.py, audit_packaging_test.py | not run | NOT RUN |
| viewer_test.js | not run (viewer unchanged) | not run, not affected |

Synthetic size line from the codec test (ESTIMATE, uncompressed, random walks, so an upper-bound-like figure
for changing channels, not representative): 71,311 B per car-minute raw.

## 6. Risks and facts found in this batch

- FACT: `build.bat` has LF-only line endings. Calling it as `cmd /c build.bat` from a relative path fails with
  "not recognized". It works when called through a `.cmd` wrapper with the absolute path and
  `TELEMETRY_BUILD_DIR` set (the wrapper used here is in `%TEMP%\b1work`). Not changed in the component.
- FACT: `tools/workspace/check-workspace-structure.ps1` reports 2 pre-existing errors (top-level `.claude` and
  `plans` folders). Not caused by B1. Not fixed: needs Matias's approval (AGENTS structural guardrail).
- FACT: the TV Director work changed `director_state.inc` (mtime 18:49:52 and 18:50:41) and `web_telemetry.c` while
  this batch ran. The DLL in section 10 was built at 18:51:08 against the versions listed there (RC 0). An earlier
  DLL (built 18:48:51, before those writes) is superseded and not kept. Claude must re-check the merge is complete
  and rebuild before the gate (spec R13). B1 did not edit either file.
- FACT: `f1c_mod.json`, `mod.json`, `web_telemetry.dll.mod.json` and `generated/` (commands.hpp, hooks.hpp,
  hook_plan.json, mod_bindings.hpp, trace_plan.json) were rewritten at 18:50:30-35 by a process outside this batch.
  This batch did not run any generate or build command that writes them (`pin_sidecars.py --check` writes nothing;
  the workspace validator has no write calls). Their sidecar pins still match (pins OK). Not reverted. Owner or
  TV Director agent to confirm.
- INFERENCE: `last_lap` is the completed lap time at the lap change. OPEN until a live lap event matches the game.
- INFERENCE: capture cost rises (about 61 more fields and a 128-slot per-car array in the ring). Not measured (AR-3,
  B1-7). Abort condition A1 applies if p95 is above 500 us.
- ESTIMATE, risk A4: const masks are the only CHG-like saving in B1. Changing channels (wear, temperatures,
  research) are not compressed to CHG. A 22-car live file may exceed 1.5 x the spec estimate. Measure in G5.
- FACT: the jobs decode pool is malloc'd per job and follows the channel count: 104 x 128 x 255 x 4 B = 13.6 MB per job
  (was 6.6 MB). Transient. Measure in B5.
- FACT: a 1 MiB raw DATA payload is the reader limit. Worst case here (104 cars, all channels changing) could exceed
  it. Realistic 24-car block is far below (the codec test checks 24 cars with all channels against the limit).
  Writing such a block would set `io_failed`. Not handled beyond that in B1.
- Owner Q3 is a deliberate change from spec B1-5 and abort A3: pre-B1 files are refused, not read. Matias removes
  his batch 4 and 5 recordings himself. The plain refusal message is UI work (B2 to B4). Until then the Recordings
  page treats them as closed or damaged. OPEN.

## 7. Restoration

To undo B1 exactly: copy each `.BEFORE_20261013_b1` file back over its source (`storage_v2.inc`,
`storage_writer.inc`, `runtime_test.c`, `CURRENT_STATE.md`), delete nothing else, delete
`tests/storage_b1_codec_test.c` only through the quarantine rule (it is new; move it, never hard-delete), then
rebuild (no sidecar pins change). The `storage_jobs.inc.BEFORE_20261013_b1` copy is identical to the current file.

## 8. Not done in B1 (open items, with the spec reference)

- TRACK block (type 6, spec B1, T6): not written.
- META `drivers[]` (T27) and the META schema `f1c.telemetry.meta/3`: not written. Team id (`team_id`) not stored (a
  32-bit hash does not fit the float value path); the sample copy would need a RAW32 integer path.
- Remaining channels: player chassis 39 of 79 (T16); brake temps as C per wheel player-only (T9); carcass zones (T10);
  blister (T14); engine life (T15); validity as a flags word on channel 19 (T19, T20); world pairs px/pz (T34,
  OPEN). Carcass temps stored per wheel only (`tcarc`).
- Events not written: `weather` (T8), `flag` (T7), `damage` (T28), `wear` (T31), `dotd` (T33), `classification`
  (T36; `finish_state` is stored as a channel, not an event).
- Owner Q7 says everything telemetry can log, including health and diagnostic data (T39) and TV Director cuts (T40),
  is saved. Not done: T39 is not telemetry data in the spec table, T40 belongs to the TV Director agent. OPEN.
- Changing-value CHG lists (spec B1 "CHG records"): only the constant-channel masks are done. Per-tick CHG lists
  are not.
- API contract section 17 and the spec's `schemas/` update: not done.
- Owner live tests B1 and gates G2 to G5 below.

## 9. Gate list for Claude (game CLOSED; run in this order)

- G1. Re-run `tests/storage_b1_codec_test.c` at the final hashes (offline, allowed with the game running).
- G2. Full build: `build.bat` with `TELEMETRY_TESTS=1` into `%TEMP%`, through an absolute-path wrapper (see section 6).
  It builds `runtime_test.exe` (-Werror) and runs `audit_packaging_test.py`, `runtime_test.exe` and
  `test_telemetry.py`. In `runtime_test.exe`, the B1 items are `t2_u5_header_table` (minor refusal), the DATA round
  trip with sim_ms and the `t3_b1_format21` writer test (5 events, last sim 12.35 s, a 20-tick pause). Also check that
  every older writer test still passes with the new channel count and `last_good_t`, which now comes from sim time
  instead of the wall-clock span (any test that compares it with a dt sum needs review).
- G3. `python tools/f1c_mod.py sdk-test` (full gate) after G2, game closed. Claude decides whether it regenerates
  anything.
- G4. Payload pin only if this build is to be packaged later: `pin_sidecars.py --payload <TEMP build dir>` and
  `verify_payload.py`. Not done in B1 (no release intent yet).
- G5. Owner live session (game closed to start, owner runs the game): a 10-minute 22-car session with one 60 s pause.
  Measure: file sizes per car-minute with XPRESS (B1-6, limit 1.5 x section 6.1 estimate); capture cost p95 under
  500 us (B1-7, AR-3); ticks during the pause (A6: a paused game must still write ticks with equal sim_ms, which the
  reader shows as zero steps); lap events against the in-game lap times (INFERENCE above); pit and contact events
  appear; the file reads with the 2.1 reader; a minor 0 recording from batches 4 and 5 is refused with a plain reason.
- G6. Hashes check after any further edit (SHA256 in section 10) and `pin_sidecars.py --check`.

## 10. Hashes (SHA256, at the time of this record)

| Item | SHA256 |
|---|---|
| `web_telemetry.dll` (B1, `%TEMP%\f1c-web-telemetry-1.1.0-b1\`), 1,318,713 B (built 18:51:08) | DD5DD85CA4F03D70290131A5C5E4BB3F1512F221C918ADBBFF7E97DE6EBA1F9B |
| `storage_v2.inc` | 5BECCD193B1176C8048C972D2766A24C96DCB425F1A101ED7E1E3EAF89DFE0C6 |
| `storage_writer.inc` | A7CEC1A8A7B9593F9DA1EB6684F79CAF0A44FD070377A93217B86A9D57912C33 |
| `runtime_test.c` | 97801C158D3D71C15DB41D387081463C3A787513C51691811954CF806599AD98 |
| `tests/storage_b1_codec_test.c` | CD1A619C83CA394CDFB071C7E3D92F41C85533D882EDA54F40DCC448E8FACEE7 |
| `storage_b1_codec_test.exe` (TEMP, not shipped) | 8A99006D90036B88158DEE003240AE305921887A0241E45D8EA07BB31063878F |
| `storage_jobs.inc` (unchanged; equals its backup) | 6421A00DDDA4640EBA85CFC290CB5CDECC8AE3C325286C70CC1A3908FCEF978A |
| `web_telemetry.c` (not edited by B1; hash at the 18:51:08 build, changed by the TV Director work) | CEBC3982DAECD1CF0996E2DB33873895439426F11F3FC3AA13CBE8738A5E4C58 |
| `director_state.inc` (not edited; hash at the 18:51:08 build) | FA2C6A73E0C07A40489225913A5F6CFBE858D7A6D13A59480139ACC10F5BFCDB |
| `workbench_ui.js` (sidecar, unchanged) | E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8 |
| `telemetry_viewer.js` (sidecar, unchanged) | 5FEDC8499495FC73910EF352F831223900F3127FCFFDD232F7B1C2A338DE2F75 |
| `VERIFICATION_MANIFEST.json` (not edited) | 198E85C034A43DDE8B43ED35EC63C062F61663610B6D374FE9F803ADA55E7FC4 |

Pins: `pin_sidecars.py --check` = pins OK (B1 build, sidecars unchanged).

## 11. Process notes (wasted effort, PIPELINE.md)

- Two shell heredoc mistakes (nested heredoc, a truncated script file) cost two retries. Scripts were then written
  with the file tool. Build via `cmd /c build.bat` failed twice before the absolute-path wrapper worked.
- One test defect found by the first run (a truncated escaped-quote literal, length 11 instead of 12). Test fixed;
  the code was right.
- One code defect found on review and fixed before the build: `sv2_scan` overwrote a header reason with
  `SV2_R_HEADER`, which would hide `SV2_R_VERSION`.

## 12. Documentation gate

- Record exists: this file. Indexed as newest in `CURRENT_STATE.md` (backup kept).
- Fact and inference are separated (tags above). Pending validation is named (sections 1, 6, 8, 9).
- Abort conditions in the spec (section 10.3): A3 superseded by owner Q3 (documented); A1, A4, A6 pending live
  measurements (G5). Owner answers Q2 (research for all cars: implemented as channels 102..117, always on) and Q7
  (partly done, section 8).

## Claude gate run (2026-10-13 evening, game closed) — NOT PASSING
Full `build.bat` with `TELEMETRY_TESTS=1` (wrapper `%TEMP%\telgate.cmd`, log `%TEMP%\telgate_final.log`), tree = B1 + view-owner fix, DLL `BBB3EDC72BE44871DFCE1E7EADEE7F489EDDB252AC4C494ADBAA6ED829AE32E5`.
Fixed during the run (stale test expectations, not product code; backups `*.BEFORE_20261013_gatefinal`):
- `VERIFICATION_MANIFEST.json` payload DLL pin 4A255915 (view-owner-only build) -> BBB3EDC7 (merged build).
- `runtime_test.c` `CHECK(q16 == 21)` -> 44 (B1 added Q16 channels 64-124).
- `runtime_test.c` table buffers 2048 -> 8192 bytes (B1 catalog no longer fits 2048; the writer already uses 8192).
Still failing: `runtime_test.c:2495` `sv2_scan(...) == SV2_R_BLOCK_HEADER` (unknown if stale expectation or real B1 bug). Gate stopped here by owner token budget. Next: diagnose line 2495, continue the gate; do not ship BBB3EDC7 before PASS.

## Claude gate run 2 (2026-10-13 late, game closed) — PASS

Game check: `tasklist` found no F1 process (the first `findstr "F1 Challenge"` form was invalid: the unquoted space made findstr open a file named `Challenge`; re-checked with a proper filter). Gate: `%TEMP%\telgate.cmd` (`TELEMETRY_TESTS=1`, full `build.bat`), log `%TEMP%\telgate_final.log`, final run `EXIT=0`, 0 FAIL lines, 36 PASS lines.

Changes (all stale test expectations, `runtime_test.c` only; backup `runtime_test.c.BEFORE_20261013_gate2` = state before this run's first edit). Product code (`storage_v2.inc`, `storage_writer.inc`, `storage_jobs.inc`) unchanged. No real B1 bug found.
- `runtime_test.c` ~2486 `t2_u6_block_framing`: added `h.flags = SV2_FLAG_HAS_SIM`. Stale: 2.1 requires per-tick sim flag, scan stopped at SV2_R_HEADER before framing (was line 2495 failure).
- `runtime_test.c` ~3121-3124 `t3_b1_format21`: pace one `Sleep(1)` per block. Stale timing: 240-tick burst overflows SW_RING_CAP=48; drops counted and GAP-reconciled (39 kept + 200 dropped = 239), product behaves to spec.
- `runtime_test.c` ~3128-3130 `t3_b1_format21`: expectation 240 -> `ticks - 1`. Stale: tick 0 only primes `sw_sm_step` (no `have_last`, no open), same as `t3_slow_disk`/`t3_s1_soak`.
- `runtime_test.c` ~3387-3391 `t3`/`test_storage_jobs` startup cut: `info[1]` -> `info[3]`, `CHECK(nb >= 4)`. Stale: 2.1 files are META, EVENT (session_start), then DATA; `info[1]` cut before any DATA block (repair said "no valid data block").
- `runtime_test.c` ~3414 U-18 startup order: poll for end state (`fin` present AND `.part` gone). Test race: repair renames repair->fin before `.part`->`.damaged` (storage_jobs.inc:248-249).
- `runtime_test.c` ~3449 channel round trip: `mask_hi == 0` -> `mask_hi != 0`. Stale: B1 adds channel ids 64..124 (sw_car_from).

Final results:
- Telemetry gate: EXIT=0, 0 FAIL, 36 PASS lines. Log `%TEMP%\telgate_final.log`.
- Codec test `tests/storage_b1_codec_test.c` (gcc -std=c11 -Wall -Wextra -Werror, built and run in `%TEMP%\codec_b1`): 358 checks, 0 failed, rc 0.
- `python tools/f1c_mod.py sdk-test`: "Canonical SDK test gate passed", SDK_RC=0. Log `%TEMP%\sdktest_gate2.log`.
- DLL SHA256 (unchanged, product code not edited): `BBB3EDC72BE44871DFCE1E7EADEE7F489EDDB252AC4C494ADBAA6ED829AE32E5`. Matches the `VERIFICATION_MANIFEST.json` payload pin, so no pin update was needed.
- Payload staging: `%TEMP%\f1c-web-telemetry-4.4.0\payload\` (5 files; sidecar hashes unchanged: workbench_ui.js `E72BDD77...`, telemetry_viewer.js `9C559DC3...`).

Not done by this gate (owner decisions remain): no install to the game, no deploy, no VPS publish. Live and human acceptance items in the record are still pending. Debug copies (diagnostic `runtime_test.c` with prints) lived only in `%TEMP%\gate2dbg` and were never placed in the workspace.

## 2026-10-13 (evening): Recordings page fixes (1.1.2 dev, NOT live-tested)

Files changed (backups: `<file>.BEFORE_20261013_recpage`):
- `telemetry_viewer.js`: confirm text now "You can restore it from there."; "Last job" line is a short summary (no file list); table rows left/right aligned; session order Practice 1-4, Qualifying, Warm-up, Race; after a Delete job reports ok the deleted rows are hidden and the list re-polls every 1.5 s while a job runs; a 404 now refreshes the list instead of asking for a reload.
- `web_telemetry.c`: only the sidecar hash pin constant (via `pin_sidecars.py`).
- `VERIFICATION_MANIFEST.json`, `f1c_mod.json`, `mod.json`, `web_telemetry.dll.mod.json`: pins only.

Root cause of "deleted group still listed" (fact from code reading, not live-proven): `GET /recordings` serves a snapshot built by the writer thread. The writer rescans after a job (`sw_rebuild_req`), but `storage_writer.inc` only rescans when `!sw_wr.have`, i.e. skipped while a session is being recorded. On the owner PC the snapshot stayed stale, while the 404 came from the live disk check (`sa_folder_exists`). The page now hides the deleted keys on its own.

Not done: server-side fix for the stale snapshot while recording (writer must drop removed folders even with an open session). Archive: checkbox selection and `/recordings/archive` checked in code; left as is.

Build: `build.bat` OK to `%TEMP%\f1c-web-telemetry-4.4.0` (payload gate OK). Game folder not touched; no test exe run. Pending: owner live check of delete, 404, order and layout; owner-run deploy.


## 2026-10-13 (late): Stale recordings list fix (server side, dev source, NOT live-tested)

Backups: `storage_writer.inc.BEFORE_20261013_staleList`, `runtime_test.c.BEFORE_20261013_staleList`, `TELEMETRY_B1_RECORD_2026-10-13.md.BEFORE_20261013_staleList`.

- Cause (fact, from code reading): `GET /recordings` serves the snapshot built by the writer on every index write. The writer rescanned the folders after a delete or archive job only when `!sw_wr.have` (no session open), so while recording the removed groups stayed listed and a second delete got 404 from the live disk check.
- Why a plain rescan was not enough: the scan resets `sw_nentries`, and `sw_wr.entry` indexes that table while a session is open. A naive rescan would corrupt the open session's entry.
- Change (`storage_writer.inc`): new `sw_rebuild_entries()` (before `sw_writer_main`). It rescans, then re-pins the open session's `<stem>.f1t.part` entry as `SW_ST_OPEN` and sets `sw_wr.entry` to its new index. The writer loop now consumes `sw_rebuild_req` on every pass (no `!sw_wr.have` condition), between writes. `sw_rebuild_req` comment updated.
- Capture path: unchanged. The sample path (`sw_apply` SWE_SAMPLE, `sw_block_add`, ring push) is not touched. The rescan runs only on the writer thread, once per completed job (directory enumeration plus one header read per `.f1t`), never per sample. A rescan delays writer drains only; ring drops are already counted and GAP-reconciled. Recording writes and file handles are not changed.
- Test (`runtime_test.c`, `t3_u12_rebuild_keeps_open`, called from `test_storage_writer`): deletes a closed folder, rebuilds with an open session, checks the removed folder is gone, the open entry is OPEN and `sw_wr.entry == 0`, the session stays open, and a second rebuild is idempotent. **Written, NOT run** (owner rule: no test exes while the game runs). Syntax-checked only (`gcc -fsyntax-only`, rc 0).
- Build: `build.bat` (no `TELEMETRY_TESTS`) OK to `%TEMP%1c-web-telemetry-4.4.0`; payload gate OK. DLL SHA256 `C99706B7A8567BAF1E67D1A7BED95B4BBF6D5E5E85A260B29DABC18304838032`. Sidecar JS not edited: `pin_sidecars.py --check` = pins OK, no re-pin needed.
- Stale pin (not changed): `VERIFICATION_MANIFEST.json` still pins the previous DLL `BBB3EDC7...`. The payload gate does not check it. Update it only after the telemetry gate passes on this DLL.
- Pending: (1) telemetry gate (`TELEMETRY_TESTS=1`) with the game closed, to run `t3_u12`; (2) owner live check while recording: delete a group, confirm it leaves `/recordings` at once, second delete no longer 404s, the open session stays listed; (3) archive while recording; (4) owner-run deploy.
