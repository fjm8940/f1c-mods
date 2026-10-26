# Telemetry 1.1.0 storage v2, batch 4 record (2026-10-13)

Scope: spec `STORAGE_MANAGER_SPEC_2026-10-13.md` section 12, steps 7 and 8 (HTTP `/recordings` routes, API 9, Recordings page, viewer
version), plus the batch 3 carry-over list (META 4.3 subset, U-17 skeleton, S-1 measurement, U-15, U-16). Coordinator instruction:
batch 4 prompt in `HANDOFF_CLAUDE_2026-10-13c.md` and the owner's request of this session (no TV director, viewer access or general
security hardening).
Stage: implement and verify (gated build PASS, b4 pin). Not promoted, not installed, not live-tested. Coordinator review pending.
Product status (`governance\PRODUCT_STATUS.json`): not changed (no stage change is claimed by this record).

## 0. Starting state (read first)

- Batch 3 accepted by Claude (see the Coordinator review in `STORAGE_V2_BATCH3_RECORD_2026-10-13.md`). No batch 4 file existed at start.
- Game closed at start and before every gate (`tasklist | grep -i "f1 challenge"` empty, rc 1), and again before the standalone runs.
- Line endings are NOT uniform in this folder; each file was edited in its own style: `web_telemetry.c` CRLF; `storage_jobs.inc`,
  `storage_writer.inc`, `storage_v2.inc`, `runtime_test.c`, `telemetry_viewer.js`, `viewer_test.js`, `test_telemetry.py`, `CURRENT_STATE.md`,
  `f1c_mod.json`, the schemas LF; `VERIFICATION_MANIFEST.json`, `mod.json`, `web_telemetry.dll.mod.json` CRLF; `API_CONTRACT_2026-10-09.md`
  and `findings.jsonl` mixed (new lines appended in each file's majority style). Checked byte by byte before and after every edit.
  (The batch 3 record's note about `storage_v2.inc` CRLF/LF was not re-checked; `storage_v2.inc` is unchanged in this batch.)

## 1. Facts (verified by command output)

- Gate, final pass (G7, see section 6): the build directory is `%TEMP%\f1c-web-telemetry-1.1.0-b4`; `TELEMETRY_TESTS=1`; command
  `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry` from `f1c_ida_decompiled`. Result lines are in section 6.
- Payload pin: after the sidecar re-pin, `pin_sidecars.py --payload` rc 0. The DLL SHA256 of the final build is
  `152E6AE0A31673F486EF11FBC7E7C1B7872437BEB0550DA840550916975D33A4` (1,271,489 bytes), the value pinned in
  `VERIFICATION_MANIFEST.json` `payload.files["web_telemetry.dll"]`.
- Sidecar pins (`pin_sidecars.py`): `telemetry_viewer.js` SHA256 `A7C038A15A1B2E6696E7AD65ABC6373657595AF2BC38267789C5DC9F1477C342`
  (was 335A0928...). `workbench_ui.js` unchanged (`E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8`).
- Host runtime (`runtime_test.exe` built with the build.bat flags, MinGW gcc, `-Wall -Wextra -Werror`, run in `%TEMP%`) on the final
  sources: rc 0 in every run. Final-source runs: `PASS 5917719 checks`, `PASS 5752098 checks`, `PASS 5456847 checks` (the count varies
  with the U-11 ring consumption, as in batch 3); the batch 4 lines are in the output of each run (section 5).
- S-1 soak (host, 24 cars, 36000 ticks, paced): `0 dropped` in the three final-source runs (producer 28.16 s, 28.19 s, 28.18 s; max
  tick 0.830 ms, 0.813 ms, 0.456 ms) and in four earlier runs of this batch (drops 0 each). Across batch 3 and this batch, 7 runs with
  zero drops; the single batch 2 failure (rt4, see the batch 3 record O-7) was not reproduced. See section 7, D.
- Viewer offline test `node viewer_test.js telemetry_viewer.js workbench_ui.js`: `VIEWER PASS 415 checks` (was 416: the 1.0.1
  `sessionRows` assertions were replaced by one retirement check; 2 batch 4 assertions were added).
- Contract: `schemas/version.json` api_version `9`, schema set SHA256 `7A8DB190CA776AC15BAAEE263C8C1F293A4C54C2B32D710C6572D8EE1B1D4E41`.
- Code facts (grep and line references in the final sources): the 1.0.1 `POST /sessions/delete` route and `GET /sessions` list are
  removed from `web_telemetry.c` (the only remaining references are in the comment that records their removal); no `DeleteFile`,
  `RemoveDirectory`, `_unlink`, `unlink(`, `remove(`, `rmdir(`, `SHDeleteFile` in `storage_v2.inc`, `storage_writer.inc`,
  `storage_jobs.inc`, `storage_api.inc` (U-15 static audit, also a gate line); one `FO_DELETE` and one `fFlags =` in the storage code, both
  with `FOF_ALLOWUNDO`.
- Meta block: a file written by the host test has two `f1c.telemetry.meta/2` blocks (initial and final), raw (not compressed), with
  `"final":false` and `"final":true`; the block walk shows 2 META, 3 DATA, 1 END for a 3-second session.

## 2. Files

Before-state (complete copies and SHA256 list, relative paths kept):
`workspace_organization_audit\TELEMETRY_110_BATCH4_BEFORE_2026-10-13\` (39 entries in `SHA256_BEFORE.txt`).

Created:
- `storage_api.inc` (LF, about 480 lines): recordings snapshot builder (writer thread), `GET /recordings` and `GET /recordings/jobs`
  producers, flat JSON request parser, validation and job submission for load, archive and delete, the `/health` storage object.
  SHA256 `8c4a9dba6eeff1e8abdd75507c58c42c6883ba0d13b18853a26f3bc94dc68443`.
- `schemas/recordings.json`, `schemas/recordings_jobs.json`, `schemas/recordings_accepted.json` (new contract schemas, section 16 of the API
  contract).
- This record.

Edited (final SHA256):
- `web_telemetry.c` (CRLF): `API_VERSION` "9"; `#include "storage_api.inc"`; the 1.0.1 routes replaced by the `/recordings` glue (section 3);
  `/health` gets `"storage"`; sidecar constant re-pinned by `pin_sidecars.py`. `88216bbb8badcf366bc93c618f0a534a29ed8730edbd7288a48fb4759ae658b9`.
- `storage_writer.inc`: META writer (`sw_meta_write`, initial after the channel table, final before END); `sa_snapshot_rebuild()` called at
  the start of `sw_index_write`; `sa_init()` in `sw_start`; forward declarations. `e445cc1f4c9ba32fb2c468415206fdef52a9c3046c90f59386bc3cf515b4e293`.
- `storage_jobs.inc`: the queue becomes multi-item jobs (`sj_submit` with item lists, `sj_run_archive` with optional Recycle Bin move of the
  working copy after a verified archive, `sj_run` delete with per-path moved and not-moved lists, rebuild request after each job); archive
  messages no longer carry a path (M11). `9ed18b6189eac1448ce3e1b8d194b5ccdce2b47e2868ee0f2df31a85f7f1afef`.
- `runtime_test.c`: batch 4 tests (section 5); version pins 8 to 9. `6e80e48b277b332fae7169dd193abf00cf848491d66a4999f103329fb1a361c7`.
- `telemetry_viewer.js` (LF): `PRODUCT_VERSION` '1.1.0'; Recordings page replaces the Sessions & Data Manager (nav name, list, load,
  archive, delete with confirm dialogs, notice, damaged and partial rows); `sessionRows` and its export removed; `reviewUrl` leaves
  `/recordings` unscoped. `a7c038a15a1b2e6696e7ad65abc6373657595af2bc38267789c5dc9f1477c342`.
- `viewer_test.js`: version 1.1.0 assertion; the page-name assertion; `sessionRows` assertions replaced.
  `5056207a2bf52dff3981fb7942daabb0ac2202c169e1213659ecb8f6027fad4c`.
- `test_telemetry.py`: static U-15 audit; removed-route checks (405/404); write guards; remote-peer refusal; `/recordings` and
  `/recordings/jobs` contract validation; legacy review id from disk; api_version 9 assertions. `22b39d522f363e7f9918bc44c822e46ea4e198f9a22c9e0fe9f8a0206e6b8943`.
- `schemas/health.json` (`storage` property, additions only), `schemas/weather.json` (`api_version` const 9), `schemas/version.json`
  (`update_version.py 9`). Hashes: health `7dfa5997d8be8cdeb707144bcfe3b4cc07fd68fdf0ac946c777b06a4927b3bc3`, weather
  `4ffbaf104428f89634e4c071e8b1b17f031c457ac101965cd90eab1a6b2bf2bc`, version `a155280b437e0d34d37c00180ffabb3c0f8c7af389a55aecc4259e90f122d085`.
- `f1c_mod.json` (LF): `build.dll` = `...\f1c-web-telemetry-1.1.0-b4\web_telemetry.dll`; sidecar SHA for the viewer. Only those two lines
  differ from the before-state. `69b593ee6b26413906fb3d16997b1f56239f994ca41f156dbabed78c5a178145`.
- `VERIFICATION_MANIFEST.json`, `mod.json`, `web_telemetry.dll.mod.json` (CRLF): sidecar and payload pins only (`pin_sidecars.py`).
  `c1629dbe2d4848ad9568670b4c530d67777dc2c707b74da07839209c20b7ed20`, `75e0a6823b31d024174904d2c0765c487189d65acf2068ccbc7c8cd2a8db2788`,
  `725c12cd56371914bf67d9fd6c55d542aa8a17e083e06f85445b634345e9234e`.
- `API_CONTRACT_2026-10-09.md`: section 16 (API 9) appended with CRLF.
- `CURRENT_STATE.md`: batch 4 index line added above batch 3 (newest).
- `findings.jsonl`: new lines appended (section 10).

Not edited: `storage_v2.inc`, `compatibility_runtime.inc`, `workbench_ui.js`, `web_telemetry.ini`, `session_archive.inc`, `generated\*`
(rebuilt by the tool with identical bytes), `README.md` (release docs at batch 5), `STORAGE_MANAGER_SPEC_2026-10-13.md` (deviations are
recorded here, not in the spec).

## 3. Routes (API 9)

| Method | Path | Guard | Result |
|---|---|---|---|
| GET | `/recordings` | none (read) | 200 snapshot (section 4) |
| GET | `/recordings/jobs` | none (read) | 200 last eight jobs |
| POST | `/recordings/load` | local peer, `X-Telemetry-Action: load`, `application/json`, 1 to 4096 bytes | 200 selection |
| POST | `/recordings/archive` | local peer, `X-Telemetry-Action: archive`, `application/json`, 1 to 4096 bytes | 202 job |
| POST | `/recordings/delete` | local peer, `X-Telemetry-Action: delete`, `application/json`, 1 to 4096 bytes | 202 job (Recycle Bin only) |
| GET | `/sessions` | removed | now 404 |
| POST | `/sessions/delete` | removed (was the 1.0.1 hard delete) | now 405 |
| POST | `/recordings/import`, `/recordings/data`, `/recordings/laps` and the other spec 8.1 routes | not built | 404 or 405 |

Full status table and bodies: `API_CONTRACT_2026-10-09.md` section 16. Every state-changing route is POST; no new GET changes state.

## 4. Design and deviations (D-n, from the spec)

- D-1 Snapshot, not live reads. `GET /recordings` returns a copy of the snapshot the writer builds in `sa_snapshot_rebuild()`
  (under `sa_cs`), so the HTTP thread never reads the entry table. The snapshot can be up to 5 s old, or until the next index write after a
  job. Validation of every write re-reads the disk on the HTTP thread (folder, files, sizes), so a stale list gives 409 or 404, never a
  wrong write.
- D-2 Body cap 4 KB (M8 of the security review) instead of the spec's 64 KB (8.6). The largest request (8 session ids) is about 700 bytes.
- D-3 Session ids are `<recording>/<session>` in archive bodies, and delete with target session or file needs `recording`. The spec
  examples did not say how a session id is qualified; the folder name is needed to be unambiguous.
- D-4 Load with an empty `recording` unloads (the spec has no unload route). A loaded recording cannot be deleted (409).
- D-5 Snapshot shape: damaged and partial files are listed inside each recording (`damaged`, `partial`), not at the top level as in
  spec 8.3; sessions carry `state`, `archived`, `bytes`, `ticks`, `files`. Not shown yet: session `type_name`, local start and end times,
  `last_good_t` for recovered files, and the legacy 1.0.1 block (no list of 1.0.1 folders on this route; the 1.0.1 review by id still works).
- D-6 Job model: one job holds up to eight archive items or one delete over up to 63 files (plus the recording folder). The queue keeps
  eight jobs; a finished job is replaced by the next submission when the queue is full. A full queue is 503.
- D-7 Recycle Bin pre-check (`sj_recycle_on`, OQ-15, INFERENCE) runs on the HTTP thread for delete and for archive with
  `recycle_working` (503 with the reason, before anything is queued). The job runs it again.
- D-8 Messages and names: no absolute path in any response or job message (M11). Archive messages changed from `archived: <path>` to
  `archived`.
- D-9 META block (4.3) is a subset: schema `f1c.telemetry.meta/2` with `"parts":["session"]`, session name, weekend, track (sanitized and
  raw), season, session type and its name, time zone, ticks, data blocks, dropped ticks, complete flag, and the `final` flag. Drivers, pit,
  incident and lap rows are NOT written (they need the game-side sources that the writer does not receive yet). The reader validates META
  blocks as before; no reader code reads META content yet.
- D-10 Import (7.9) NOT built. Needs the 1.0.1 `traces.bin` to channel conversion and the META driver rows; neither exists in this batch.
  The carry-over asks for META plus a U-17 skeleton if import is too large: META is done (D-9); the U-17 skeleton is done (section 5); the
  import itself is deferred (section 7).
- D-11 Repair chunking (6.5) NOT done. The archive, repair and delete-preparation paths still read whole files (`sj_read_file`). Large
  sessions in a 32-bit process need the chunked path before the owner archives a long session (section 7, A).
- D-12 Load selects only. The review pages for v2 recordings (8.5 and the spec 8.1 read routes) are not built; the console shows the
  selection and the notice.
- D-13 Session delete size check: the size confirmed for a session is the sum of its session files (`.f1t`, `.f1a`, an open `.part`). A
  damaged leftover with the same stem is not in that sum, so the delete is refused with 409 until the list is reloaded and reconfirmed.
- D-14 `/health` `storage.recycle_bin` is the text "checked per delete" (the check runs per request; no drive query on health).
- D-15 `GET /recordings` when storage is off returns `enabled: false` and an empty list; archive and delete return 503 first.

## 5. Tests added or changed

Host (`runtime_test.c`, linked standalone and in the gate's `serve` run, which executes the whole test set first):
- META 4.3 subset: `PASS META 4.3 subset: two f1c.telemetry.meta/2 blocks (initial and final), raw, readable`.
- Request parser: `PASS request parser batch 4: objects, lists and booleans accepted; escapes, trailing data, non-ASCII, long numbers and 9-item lists refused`.
- Snapshot (U-16 sizes): `PASS GET /recordings snapshot batch 4: sizes per session and recording, total 310 B, damaged and partial lists, no absolute path`.
- Load: `PASS POST /recordings/load: loaded, unknown 404, unsafe name 422, wrong type 400`.
- Archive refusals: `PASS POST /recordings/archive: LZMS off 503, no working copy 404, bad ids 422/400, recycle off 503; job refuses an unverifiable working copy and changes nothing`.
- Delete refusals and job: `PASS POST /recordings/delete: loaded 409, stale size 409, unknown 404, missing recording 400, bad target 422, Recycle Bin off 503; recording delete job through the double moves files then folder with FOF_ALLOWUNDO`.
- U-17 skeleton (7.9 not built): `PASS U-17 skeleton (7.9 import not built): the 1.0.1 telemetry_data folder is byte-unchanged by scan, snapshot, load and archive refusal`.
- U-19 (partial): the delete double counts calls; a refused request makes no call (checked in the delete refusal cases); the success case makes one call.

Live HTTP (`test_telemetry.py`, serve harness): removed routes (405, 404); write guards (403 missing header, 415, 413, 400, 422, 404); remote
peer refused (403); `GET /recordings` and `GET /recordings/jobs` validated against their schemas; review of the seeded 1.0.1 archive by id
read from `%TEMP%\...\telemetry_data\`. `PASS batch 4 HTTP ...` and `PASS contract ... api_version 9 pinned`.

Static: `PASS U-15 static delete audit` (gate, section 1).

Node: `VIEWER PASS 415 checks`.

## 6. Gate history (every run; none hidden)

- G1: rc 1 at the payload pin (`verify_payload.py` preflight): `telemetry_viewer.js` manifest pin stale after the sidecar re-pin. Expected.
- `pin_sidecars.py --payload "%TEMP%\f1c-web-telemetry-1.1.0-b4"`: rc 0 (payload files: viewer A7C0..., DLL 152E..., dll.mod.json 725C..., ini, workbench E72B...).
- G2: rc 1 in `test_telemetry.py` line 468: the legacy review id came from `/sessions`, which this batch removes; the harness read the id
  from the wrong folder. Test bug, fixed (id read from disk).
- G3: rc 1, same line, diagnostics added (`DBG review ... 404 unknown or deleted session`): the id picked was the folder `telemetry_data`.
  Test bug, fixed (ids filtered by the 1.0.1 id pattern).
- G4: rc 1, `AssertionError` (no id folder): the 1.0.1 session folders are under `telemetry_data\` (`AR_ROOT_NAME`). Test bug, fixed.
- G5: rc 1 in `contract_validate.py`: `schemas/weather.json` pinned `api_version` const `"8"`. Schema bug, fixed; `update_version.py 9`.
- G6: rc 0, `Build verified: ...\f1c-web-telemetry-1.1.0-b4\web_telemetry.dll`. Full gate PASS (lines listed in section 1).
- G7 (final, after the U-17 skeleton test was added): see the result line below.

G7 result: rc 0. Lines: `PASS payload gate: OK (5 files ...)`, `PASS U-15 static delete audit ...`, `PASS batch 4 HTTP ...`, `PASS contract ... api_version 9 pinned`, `VIEWER PASS 415 checks`, `Build verified: ...1c-web-telemetry-1.1.0-b4\web_telemetry.dll`, `GATE7_RC=0`. Gate PASS on the b4 build with the payload pin in place.

The DLL did not change between G2 and G7 (the test and schema fixes changed no C source; `web_telemetry.c` last written before G2).

## 7. Carry-over from the batch 3 coordinator review

| Item | State |
|---|---|
| A. Repair chunking (6.5) | NOT DONE (D-11). Needed before long archives; the owner test should archive a short session first. |
| B. Import 7.9 + U-17 | Import NOT DONE (D-10): needs the `traces.bin` conversion and META driver rows. U-17 skeleton DONE (section 5). |
| C. META block 4.3 | Subset DONE (D-9): session identity and counters; drivers, pits, incidents, laps not written. |
| D. S-1 flake decision | Not weakened. Measured: 0 drops in 7 runs (section 1). Proposal below. |
| E. U-15 static grep | DONE (gate line). |
| F. U-16 sizes | DONE: snapshot sums in the host test (310 B = 240 + 70); the index keeps per-file and per-recording sizes. |
| G. U-17 (import source byte-unchanged) | Skeleton only (see B). |

S-1 proposal (for the coordinator, not applied): keep the 48-slot ring and the zero-drop assertion. If the batch 2 failure recurs in a
later gate, the proposed change is `SW_RING_CAP` 48 to 96 (about 4.8 s at 20 Hz for 24 cars, memory about 2.8 KB x 96 = 270 KB) with the
drop count printed on every failure, not a weaker assertion. The measured drop count is 0 in 7 runs; no change is justified by the data.

## 8. Open issues and pending validation

  127.x and the custom header). A page that rebinds its domain to 127.0.0.1 is treated as local (SECURITY_REVIEW F-03). Required before any
  public hosting; the security hardening batch was excluded by the owner's instruction for this batch.
- O-2 Chunked IO (6.5, D-11): whole-file reads in archive and repair. Risk: memory in the 32-bit game process for long sessions.
- O-3 Import 7.9 and META drivers/pits/incidents/laps (D-9, D-10).
- O-4 Recycle Bin pre-check (OQ-15) and `SHFileOperationW` on the owner PC (no COM initialisation, O-10 of batch 3) not verified live.
- O-5 LZMS working set in the 32-bit process (R-5, O-9 of batch 3) not measured with a real session.
- O-6 Recordings page: DOM rendering not covered by the offline viewer test; the page code parses (`node --check`) and every server string
  goes through `esc()` or the escaping toast, but no browser run was done in this batch. Browser check pending (owner live test, step 2).
- O-7 Review pages for v2 recordings (D-12) not built; load only selects.
- O-8 Session delete size (D-13): a damaged same-stem leftover blocks a session delete until the list is reloaded.
- O-9 Job results are kept for the last eight jobs only and are not persisted across restarts.
- O-10 S-1 decision: the proposal above, pending coordinator decision.
- O-11 README and release notes (O-13 of batch 3) not updated; batch 5.
- O-12 Spec items not built: 7.9, 8.5, 8.1 read routes for v2, `legacy` block of 8.3, `type_name` in 8.3.
- O-13 Owner live test (11.6) not run; the game must be closed to start, and the owner runs it (no release until it passes).

Owner live test steps for this batch (the owner runs the game; Claude reads logs and files only):
1. Start the game; `GET /health` shows `api_version` 9, `storage.enabled` true, `storage.recycle_bin` "checked per delete".
2. Open the console, page Recordings: the notice text, the folder total, the recordings and sessions of the last weekend are listed.
3. Load one recording: the toast says nothing changed; `GET /recordings` shows `loaded`. Load an empty name: unloaded.
4. Archive one closed short session (keep the working copy): the job list shows `archive-N ok`; the `.f1a` file exists; the working copy
   is unchanged (file size, no move).
5. Archive another with "Move the working copy to the Recycle Bin" OK: the file is in the Recycle Bin and can be restored.
6. Delete one recording: the confirm dialog names it and its size; the files are in the Recycle Bin, not deleted; restore one file.
7. Try to delete the loaded recording: refused (409) until unloaded.
8. Check that no response shows a drive path (`C:\`) anywhere in the page or in `GET /recordings`.
9. Logs checked by Claude: `storage:` lines, job results, no unexpected file operation.

## 9. Documentation gate

- This record exists in the component folder and is indexed from `CURRENT_STATE.md` (batch 4 line, newest).
- Facts (sections 1, 5, 6) are separated from design and deviations (4), carry-over (7), open items (8) and the owner steps.
- Pending validation: coordinator review; the S-1 decision; O-1 (M3) and the security batch; O-2 (chunking) before a long archive; the browser
  check of the Recordings page; the owner live test (section 8).
- Workspace validator run after the documentation change: Errors 2 (`.claude` and `plans` top-level directories, pre-existing, the same two as batch 3). This batch changed no top-level directory and no governance file.

## 10. New findings (`f1c_ida_decompiled/findings.jsonl`)

Appended (tiers VERIFIED, FACT, OPEN; ids `telemetry-batch4-*`): the META subset block; the request parser; the recordings snapshot; the
route removals (1.0.1 hard delete gone); the S-1 measurement (0 drops in 7 runs); the open items O-1, O-2, O-4, O-5 and O-8.

## 11. Coordinator review

(pending)

## Coordinator review (Claude, 2026-10-13, game closed)
- DLL SHA256 re-hashed: 152E6AE0...975D33A4 = report.
- Write routes (web_telemetry.c:8833-8850): POST only, local peer, X-Telemetry-Action = action, JSON content type,
  body 1-4096. PASS. No DeleteFile/RemoveDirectory/unlink in storage_*.inc. PASS.
- Verdict: batch 4 ACCEPTED as dev candidate. Before owner archives a LONG session: build repair/archive chunking (6.5).
  Open: import 7.9, Host allowlist M3 (security batch), real Recycle Bin move only via test doubles. Owner live test
  per section 8 pending.
- Next order: security batch (M1-M6, M9-M11) -> viewer access -> TV Director phase 0 live test -> phase 1.
