# Telemetry 1.1.0, batch B2 record (index core, lap index, offline tests), 2026-10-14

Product: Telemetry (`web_telemetry`), candidate 1.1.0. Pipeline stage: **implement, offline verified, gate PASS**.
Spec: `TELEMETRY_LIVE_REVIEW_SPEC_2026-10-13.md` section 3.2 (data path), 3.5 (threads), 5 (B2), 6.3 (risks), 10.2 (backups).
Predecessor: `TELEMETRY_B1_RECORD_2026-10-13.md` (format 2.1, sim per tick, EVENT blocks).
Not installed. Not live-tested. No game folder, no deploy, no VPS, no production file touched.

**Scope decision (read first).** B2 in the spec is larger than one safe batch. This batch delivers the index
core and the lap index with offline tests (B2-1, B2-2, B2-4). The review worker, the HTTP routes, the open-`.part`
reader and the LIVE history layer are **deferred** (section 1). The viewer is not changed, so the LIVE / PAST design
of `TELEMETRY_GUI_REDESIGN_2026-10-14.md` and the TV Director phase 2 UI are untouched (no conflict found; section 4).

Tags: FACT = read in a file named, or observed in a run in this session. INFERENCE = reasoned, needs a test.
ESTIMATE = arithmetic or synthetic measurement. OPEN = needs a test, an owner decision or a live run.

## 1. Spec B2 items: done or deferred

| Spec B2 item | Status | Where / reason |
|---|---|---|
| Block index: one record per DATA block (file offset, seq, ticks, sim span) | **DONE** (offline) | `storage_v2.inc` `sv2_index_build`, `sv2_idx_block_t` |
| Lap index: game lap events first, sim-time derived fallback | **DONE** (offline) | `storage_v2.inc` `sv2_idx_lap_put`, `sv2_idx_apply_event`, `sv2_idx_apply_data`, `sv2_idx_lap_time` |
| Any car, any lap from the index; reader touches only that lap's blocks | **DONE** for the pure reader (instrumented `blocks_read`) | `sv2_idx_span`, `sv2_idx_read_block`. The HTTP route that uses it is deferred |
| `GET /recordings/review`, `/recordings/laps`, `/trace/data` for any car, `/telemetry?session=&t=` | **DEFERRED** | No route added. Reason: spec R-V1 says review decode runs on a worker and the HTTP thread never waits. The worker and the session-id resolver are not built. Adding a route now would decode on the HTTP thread, which the spec forbids |
| Incremental index of the open `.part` (LIVE layer 2) | **DEFERRED** | Needs the writer-reader stress test (spec B2-3, R5). Not built |
| Review worker thread (R-V1, queue of 4, `503 busy`) | **DEFERRED** | Spec B2-5. Not built |
| Tests B2-1 (lap times), B2-2 (sim lap across a pause), B2-4 (any car/lap, only its blocks) | **PASS offline** | `tests/storage_b2_index_test.c`, section 3 |
| Tests B2-3 and B2-5 | **NOT RUN** (not built) | section 1 deferrals |
| Owner live B2 (two laps of two cars within 0.1 s of the game; untraced car in Lap Analysis) | **NOT DONE** | Needs the routes and the viewer reader (B3/B4). Nothing for the owner to run yet |

Not changed in this batch: the HTTP API (`API_VERSION` stays **9**; owner decision on API 10 stays pending because no
route was added), `telemetry_viewer.js`, `workbench_ui.js`, `web_telemetry.ini`, `storage_writer.inc`, `storage_jobs.inc`,
`web_telemetry.c`, `director_state.inc`, `runtime_test.c`.

## 2. Files, backups and hashes

Every existing file edited has a `.BEFORE_20261014_b2` copy in the same folder (section 6). New files: none in the workspace
except the test below and this record. No file was hard-deleted.

| File | Change | SHA-256 (after) |
|---|---|---|
| `storage_v2.inc` | Appended "2.1 B2 INDEX CORE" section (about 290 lines, LF, UTF-8, no BOM): frame helper, lap-row merge, DATA and EVENT appliers, `sv2_index_build`, `sv2_idx_read_block`, `sv2_idx_span`, lookup and lap-time helpers. The walk shares the framing rules of `sv2_scan`. One comment that contained `/*` was reworded (compile fix). Nothing above the appended section changed | `18986DA8C2F88BA8E83E50E9CE69355337B9E26EBDCB056E369289FB445F8D7C` |
| `tests/storage_b2_index_test.c` | NEW. Offline test, 126 checks (section 3) | `34DD35E9F252AF571685093ED74C6C5447B6DC88856BA1B7A4CB6BAA434C62C2` |
| `VERIFICATION_MANIFEST.json` | Only the `payload` section re-pinned to the gate build (`pin_sidecars.py --payload`). Checked: no other section changed | `964E7BBECE9B4A3D23B19D4656694F3FDE6A4D5C69D0CAC50CCE1263D6D4E6FD` |
| `CURRENT_STATE.md` | One index line added at the top of the list (LF, unchanged line endings) | `C11C6FA72D9FE0C80383C85E786BDBD9ED4915E45BDCD29E21CE0F02A95FD226` (backup before the edit: `F849CDD7F8D73D4DD9F5C719DACB42214B2D8AC8C0C1F1D2DFB8C23C19D0864E`) |
| `TELEMETRY_B2_RECORD_2026-10-14.md` | NEW (this file) | n/a |

Payload (staged in `%TEMP%\f1c-web-telemetry-4.4.0\payload`, gate OK, 5 files):

| File | SHA-256 |
|---|---|
| `web_telemetry.dll` (B2 build) | `04F923D420B29E091CE13251D82AA3CE1A08DEAF4B89F51AE3076A78B7D958BC` (**identical to the pre-B2 staged DLL** of the TV Director phase 2 record) |
| `telemetry_viewer.js` | `0C8F2606650BD72B26027E8921950E1D0CF879A5CBB893244A35CF1235A97B7B` (unchanged) |
| `workbench_ui.js` | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` (unchanged) |
| `web_telemetry.ini` | `A7691C6E7C22F25E26A041B3E804153797DE0BD71E7986C28107763825F54F05` (unchanged) |
| `web_telemetry.dll.mod.json` | `40BC4635D464A5346527DB7C57A5C89F03F885E85EB5E744F688A87D68CF17B6` (unchanged) |

FACT: the DLL hash did not change. INFERENCE: the new index functions have no caller yet, so the optimiser drops them
from the DLL. This is why the binary is unchanged. Runtime behaviour is therefore identical to 1.1.0 B1 (per hash).
When the routes land, the DLL will change and the payload re-pin will be needed again.

## 3. Tests and checks

| Check | Command / place | Result |
|---|---|---|
| B2 index test | `gcc -O1 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -o %TEMP%\b2work\storage_b2_index_test.exe tests\storage_b2_index_test.c`, then run | **PASS: 126 checks, 0 failed**, exit 0 |
| Viewer suite (spec rule, always) | `node viewer_test.js telemetry_viewer.js workbench_ui.js` | **VIEWER PASS 525 checks** (same as before; viewer not edited) |
| Sidecar pins | `python pin_sidecars.py --check` | `pins: OK` |
| Plain DLL compile (no TELEMETRY_TESTS) | `build.bat` through a `.cmd` wrapper in `%TEMP%` calling the absolute path (same as B1, relative call fails) | **BUILD_RC=0**; payload gate OK (5 files); DLL in `%TEMP%` |
| Full gate (`TELEMETRY_TESTS=1`) | Game check first: `tasklist` shows no `F1 Challenge Online.exe` (only `Lightloader.exe`). PowerShell `.\build.bat` with the log in `%TEMP%\b2_gate.txt` | **Run 1 FAILED** (expected, documented): `test_telemetry.py:105` manifest payload pin for `telemetry_viewer.js` was stale (E138C234..., from the GUI gate). **Run 2 after re-pin: EXIT 0**, 36 PASS lines, 0 FAIL, including runtime_test, fuzz (40 x 2000, 0 crashes), soak (flat RSS), SSE, HTTP audits, `VIEWER PASS 525 checks` |
| Workspace validator | `tools\workspace\check-workspace-structure.ps1` | 2 errors, both pre-existing and not caused by B2: top-level `.claude` and `plans`. Not changed (structural, needs Matias's approval) |

Test coverage in `tests/storage_b2_index_test.c` (synthetic 60 s session, 20 Hz, 3 cars, 1200 ticks, 60 DATA blocks,
16,704 bytes; sim frozen for 2 s at ticks 300..339 while the wall clock keeps running):

- B2-1 game laps: car 0 laps 1, 2, 3 have game times 12.0, 6.0, 20.0 s; matched within 1 ms. Derived lap 2 start
  12.000 s, end 18.000 s (1 ms). FACT in this test; the writer's real output is not yet measured (section 5).
- B2-1 partial lap: car 1 lap 1 (first sighting, no event) gives no lap time (`sv2_idx_lap_time` returns 0). Correct:
  a lap that began before the recording is not invented.
- B2-2 across the pause: car 0 lap 2 sim-based 6.000 s equals the game time (6.0 s); the wall-clock difference (8.0 s)
  is rejected by the check. Car 1 lap 2 (no events, derived only): 18.000 s sim versus 20.0 s wall.
- Event order: EVENT before the DATA block of its tick (writer order) and EVENT after it both give the same 5 lap rows
  with equal fields.
- B2-4: `sv2_idx_span` for car 0 lap 2 `[12, 18] s` and car 1 lap 2 `[8, 26] s` returns the same blocks as a brute-force
  overlap of tick times; `sv2_idx_read_block` reads exactly that many blocks (`blocks_read`), fewer than 60; decoded values
  match the truth at every tick.
- Reader agreement: block count and event count equal `sv2_scan` on the same buffers.
- Negative cases, reason codes only: CRC flip in DATA block 5 (`SV2_R_BLOCK_CRC`, index stops with 5 blocks);
  truncated before END (`complete = 0`, no error, nothing invented); truncated inside a block (error); block array full
  (`SV2_IDX_R_CAPACITY`, 10 blocks kept); malformed lap EVENT (`SV2_R_BLOCK_DECODE`, same as `sv2_scan`); out-of-range
  block index refused with no read counted.

Measured size (ESTIMATE, synthetic): 60 blocks = 1,920 B of block index in memory; 5 lap rows = 200 B. For 2 h at 22 cars
the spec estimate is about 230 KB block index and 104 KB lap index (section 3.2 of the spec). Not measured on a live file.

## 4. Conflicts with the redesign (checked)

- LIVE / PAST switch, PAST notice on pages (`TELEMETRY_GUI_REDESIGN_2026-10-14.md` D1), TV Director phase 2 UI: no
  change in this batch. The viewer still has no storage v2 reader, so PAST still shows the notice. The index core
  is the basis of that reader (B3/B4), not a change to the design.
- The spec's "LIVE history" layer (section 2.3) and the spec's lap view need the routes and the worker first. Deferred.
- No conflict found that required keeping the old design over a B2 item. Nothing was undone.

## 5. Open items and risks

- OPEN, B2-3 and B2-5 (worker, `.part` reader, 503, `/health` under a slow read): not built. Next batch.
- OPEN, routes and API version: when `/recordings/laps`, `/recordings/review` and `/trace/data?session=` land, the
  API version question (owner decision, spec 6 / API 10) must be answered first. Not changed here.
- OPEN, real-writer check: the lap tests use synthetic files built with the same encoder. The writer's real events
  (`sw_events_from_sample`, `last_lap_s` = channel 86) have not been compared with a live lap yet. INFERENCE from B1
  record: `last_lap` is the completed lap time. Owner live B2 needs the routes first.
- OPEN, gate coverage: `tests/storage_b2_index_test.c` is not run by `build.bat` (the same as the B1 codec test). It is a
  manual offline check run from `%TEMP%`. Adding it to the gate would change `build.bat` (LF file): left for a later
  batch so that the gate stays as it is.
- RISK, `EVENT` ordering with a pause: the writer writes a lap EVENT before the DATA block of the same sample. The merge
  rule (match by car, lap and end within 2 ms) covers both orders (test). A 2 ms tolerance is INFERENCE from the
  writer's 1 ms rounding.
- RISK, lap numbers that repeat inside one file (a session type change inside one file cannot happen by design, B1 Q1).
  The index would merge them by (car, lap, end); not seen in tests.

## 6. Backups and restoration

Backups (same folder, suffix `.BEFORE_20261014_b2`, taken before the first edit of each file):
`storage_v2.inc`, `CURRENT_STATE.md`, `VERIFICATION_MANIFEST.json`.

Restoration (undo B2 exactly):
1. Copy `storage_v2.inc.BEFORE_20261014_b2` over `storage_v2.inc` (removes the appended section). Hash must be `5BECCD19...`.
2. Copy `VERIFICATION_MANIFEST.json.BEFORE_20261014_b2` over `VERIFICATION_MANIFEST.json`. Hash must be `481197D8...`.
   Then nothing else needs re-pinning (the viewer and the other sidecars are unchanged).
3. Copy `CURRENT_STATE.md.BEFORE_20261014_b2` over `CURRENT_STATE.md` (removes the index line; no later line exists yet).
4. Move `tests/storage_b2_index_test.c` out of the component to a quarantine folder (never hard-delete; the file is new).
5. Rebuild and rerun the gate (not needed for a file-only restore of identical bytes; the DLL hash stays `04F923D4...`).

## 7. Process notes (PIPELINE.md wasted-effort check)

- Compile errors: a `/*` inside a block comment (fixed by rewording) and two unused variables in the test (fixed). Test
  wiring: the second index used a zero-capacity struct, and the malformed-event case never wrote its event (fixed).
  Each was found by the first test run, not by reading.
- `build.bat` could not be called by its relative path from `cmd` (LF file); the wrapper with the absolute path worked.
- The first full gate failed only on the manifest payload pin (documented expected failure). One re-pin fixed it.
- The validator's two errors were checked against the record of 2026-10-13/14 and are unchanged.

## 8. Documentation gate

- Durable record: this file. Indexed: one line at the top of `CURRENT_STATE.md`.
- Fact versus inference: tagged in sections 1 to 5.
- Pending validation: section 5 (B2-3, B2-5, routes, real-writer lap check, owner live B2).
- No secrets, no `api1.txt`, no private keys, no raw game output copied. The gate log stayed in `%TEMP%`.
- `governance\PRODUCT_STATUS.json` not changed: the stage stays as it was; this batch does not change a product stage.
