# TV Director phase 0 record (2026-10-13)

Scope: spec `TV_DIRECTOR_SPEC_2026-10-13.md` phase 0 (read-only). No game write, no detour, no car switch, no angle set, no POST route, no install, no deploy.
Stage: implement and verify, offline only. NOT live-tested. `governance\PRODUCT_STATUS.json` not changed (no stage change claimed).
Product: Telemetry 1.1.0 (`web_telemetry`). API version stays `9` (see doubts).

## 0. Starting state and game rule

- `tasklist` showed `F1 Challenge Online.exe` running at the start of the task (checked once, at the start; not re-checked before the build). The task rule for a running game applies: build and offline viewer test only. Not run: `runtime_test.exe`, `TELEMETRY_TESTS=1`, `test_telemetry.py`, fuzz, any browser or live check.
- Workspace validator: `tools\workspace\check-workspace-structure.ps1` did not run. The bash shell gave `The argument 'toolsworkspacecheck-workspace-structure.ps1' to the -File parameter does not exist` (backslashes were eaten by the shell). Not re-run with the right quoting. Pre-existing errors listed in CURRENT_STATE section 12 (`.claude`, `plans`) were not touched.
- Nothing was written into `F1 Challenge V10` or `Documents\F1 Challenge V10`. The on-disk EXE was read only (section 5).

## 1. Backups (cp -pn, suffix `.BEFORE_20261013_tvd0`, made before the first edit)

`web_telemetry.c`, `compatibility_runtime.inc`, `telemetry_viewer.js`, `viewer_test.js`, `web_telemetry.ini`, `web_telemetry.dll.mod.json`, `f1c_mod.json`, `VERIFICATION_MANIFEST.json`, `CURRENT_STATE.md`, and `f1c_ida_decompiled\findings.jsonl`.

Gaps, stated plainly (not hidden):
- `mod.json` was rewritten by `pin_sidecars.py` and had no backup. The only intended change is the `telemetry_viewer.js` sidecar hash (old `37F87A6F...`, new `D8900B17...`). Restore by setting that one value back.
- `generated\*` (five stub files) was rewritten by the build's SDK generator step; no backup. Their content is generic template text, no telemetry field.
- New files, no backup needed: `director_state.inc`, this record.

## 2. What was built

### DLL (`web_telemetry.c`, `compatibility_runtime.inc`, new `director_state.inc`)
- `director_state.inc` (new, included before `storage_v2.inc`):
  - Copied resolver (C-a, C-b, C-c subset) from `development\mods\camera_editor\plugin\cam_sig.h`. APPLY and LOCALCAM patterns not copied (C-h not copied). Refuses a build whose vehicle stride is not `0x48C0` (`cam_sig.h:32`), and also refuses a vehicle array base other than `VEH_BASE`.
  - Read pattern C-e (`camera_editor.c:459-474`): director = `*director_ptr`, plausibility `camera_editor.c:152`, mode `+0x34`, count `+0x38`, index `+0x3C`.
  - Names scanner C-g: `Default.cfg` in `<game>\<SAVEDIR>` (default `SAVE`, `Config.ini` `[COMPONENTS] SAVEDIR`, same rule as `read_save_dir`), `LocalCam` lines in file order, duplicates dropped, fixed four excluded, 31-char cap, 64 max.
  - `dcam_try_resolve()` on the worker thread: every 2 s until resolved or refused, at most 60 attempts. Scans each executable section with guarded `copy_game` chunks. A failed match with fewer than 100 `push ebx; push esi` hits means "not unpacked yet" (retry). 100 or more means "unknown code base" (refused).
  - `dcam_capture_locked()` inside `capture_on_game_thread` (game thread, under `g_telemetry_cs`, TryEnter): reads only. Writes `g_dcam_snap`.
  - `dcam_state_json()` on the HTTP thread. `dcam_host_ok()` Host allowlist. `dcam_settings_read()` for `[Director] Enabled`.
- `compatibility_runtime.inc`: one line in the capture callback (`dcam_capture_locked`), one line in the worker loop (`dcam_try_resolve` while pending).
- `web_telemetry.c`: include; `dcam_settings_read(use)` in `telemetry_settings_load`; route `GET /director/state` after `/health`. No POST route for `/director/*`.

### Route `GET /director/state`
- `[Director] Enabled=0`: 404 JSON error.
- Host header not `localhost`, `127.0.0.1` or `[::1]` (optional `:digits` port): 403 JSON error. Applies to this route only. The other routes still have no Host check (FACT, spec 6.1).
- Otherwise 200 JSON, fields:
  - `api`, `phase` (0), `read_only` (true)
  - `viewer_local` (`http_peer_is_local`, loopback peer)
  - `game_running`: DERIVED. True when a capture is at most 1000 ms old. This DLL runs inside the game process, so "game running" here means "live samples". `sample_age_ms`, `capture_sequence`, `session_generation`, `sim_time_s` (raw session time mirror `0x8AFEA0`)
  - `camera_engine`: `not_tried` | `retrying` | `resolved` | `refused`, plus `camera_engine_reason`
  - `camera_addresses`: resolved addresses (hex), or null
  - `snapshot_valid`
  - `player`: `roster_id` (`veh+0x4894` of the player pointer), `vehicle`
  - `followed`: `roster_id` (`+0x4894`), `roster_index_4898`, `driver_id_489c`, `table_index` (`0x7C54DC`, mirrored), `vehicle`, `driver_name` (`+0x4620`), `car_name` (`+0x4678`)
  - `mode`, `index`, `count`, `angle` (modes 0-3 are the fixed four; mode 4 is the ONBOARD name at `index`; any other mode gives an empty string), or null when no snapshot
  - `names_source` (`Default.cfg` | `missing`), `names` (fixed four, then ONBOARD)
  - `last_applied_seq`: null, `phase_1_fields`: "not implemented in phase 0"
- Phase 1 and later fields (`on_air`, `follow`) are absent by design.

### Viewer (`telemetry_viewer.js`, embedded into the DLL by `.incbin`)
- Top-level tabs in the title bar: "Telemetry" (default, page set `RACE_PAGES` unchanged) and "TV Director".
- TV Director panel: note "Phase 0: read-only. Nothing is sent to the game; this page only reads the game state." On-air rows (car, driver and roster, angle, mode/index/count, camera engine with reason, sample age, sim time, sequence). Names list with the current angle highlighted. Footer on the source of the names.
- Polls `GET /director/state` every 250 ms (about 4 Hz) only while the TV tab is shown. One probe at load: a 404 hides the tab (kill switch).
- Not `viewer_local`: note "Needs the game running on this PC". A failed request: "No answer from the game PC (...). Nothing was sent."
- Telemetry page keys are ignored while the TV tab is shown. In-page UI only, no `confirm`/`alert`/`prompt` in the new code.
- Pure helper `tvdLines` and constant `TVD_FIXED` exported for `viewer_test.js`.

### Config (`web_telemetry.ini`)
- Appended `[Director]`, `Enabled=1` (CRLF, no BOM). Default 1 as requested for phase 0.

### Not changed (on purpose)
- `web_telemetry.dll.mod.json` description still says "Read-only: it never changes the game". Still true for phase 0, so no owner decision (D-TV4) is needed yet.
- `API_VERSION` stays `"9"`. Adding a route normally goes with a contract bump (`schemas/version.json`). Not done here; owner or contract pass needed (doubt 3).
- No storage file changed. No Camera Editor file changed. No file in `development\` changed.

## 3. Addresses and signatures used (source)

Static check on the on-disk EXE, section 5. All values from the copied resolver (`cam_sig.h`, pattern lines 47-53) and the decompiles.

| Item | Address or value | Source |
|---|---|---|
| director_ptr | 0x5EA528 | cam_sig.h:20, DIRECTOR pattern +1 |
| manager | 0x5EA510 | director_ptr - 0x18 (cam_sig.h:21, 33) |
| view_dispatch | 0x409440 | DISPATCH pattern (cam_sig.h:47), read-only here |
| mirror_setup | 0x409890 | MIRROR pattern (cam_sig.h:48), read-only here |
| target_ptr (followed car global) | 0x7C54E0 | SETVEH written pointer that is not the player (cam_sig.h:22, 118-143) |
| player_ptr | 0x7C54E8 | PLAYER pattern +1 (cam_sig.h:23, 52) |
| active_ptrs | 0x7C54F0 | SETVEH +27 (cam_sig.h:24) |
| veh_base | 0x5EC6D8 | SETVEH +12; equals `VEH_BASE` (f1c_sdk.h:9) |
| veh_stride | 0x48C0 | SETVEH +6; equals `VEH_STRIDE` (f1c_sdk.h:10); refused if different |
| roster_count | 0x8AC390 | ROSTER pattern +6 (cam_sig.h:27, 53) |
| table index | 0x7C54DC | existing mirror (`web_telemetry.c` `ADDR_PLAYER_INDEX`), SetPlayerVehicleIndex decompile |
| roster id | veh +0x4894 | CameraViewAssignment decompile line 6 (18580 = 0x4894); spec 4.4 |
| roster index | veh +0x4898 | camera_editor.c:45 (VEH_ROSTER_INDEX), read raw |
| driver id | veh +0x489C | `OFF_DRIVER_ID` (web_telemetry.c:820) |
| driver name | veh +0x4620 | `OFF_DRIVER_NAME` (web_telemetry.c:818) |
| car name | veh +0x4678 | `OFF_CAR_NAME` (web_telemetry.c:819) |
| mode / count / index | director +0x34 / +0x38 / +0x3C | camera_editor.c:38-40 (C-d) |
| fixed angles | NOSECAM, COCKPIT, TVCOCKPIT, SWINGMAN = modes 0-3 | camera_editor.c:36, 459 |
| ONBOARD | mode 4, index into LocalCam order | camera_editor.c:459, 186-243, 349-354 |

Spec corrections found while reading (see section 8): none of the addresses in the spec differ from the code; the spec's roster field is confirmed by the decompile.

## 4. Tests

Run and passed:
- `node viewer_test.js telemetry_viewer.js workbench_ui.js` in the mod folder: `VIEWER PASS 431 checks`, rc 0. 415 before, plus 16 new TV Director checks (fixed angle order; car, driver, angle and raw rows; refused flag; stale-sample label; null-document dashes; source rules: GET only, no POST, no confirm, alert or prompt in the TV block, one fetch to `/director/state`, `cache: 'no-store'`, phase 0 note, 404 hides the tab, 250 ms, both top-level tab labels, page keys off on the TV tab, RACE_PAGES unchanged).
- `python pin_sidecars.py` (rewrote pins: telemetry viewer hash), `python pin_sidecars.py --check`: `pins: OK`.
- `python pin_sidecars.py --payload %USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-tvd0`: pins written, rc 0.
- Build: `TELEMETRY_BUILD_DIR=%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-tvd0 python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry` (run from `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled`, no `TELEMETRY_TESTS`): rc 0, `gcc -Wall -Wextra -Werror` clean, payload gate OK (5 files). The build was run twice (before and after the `dcam_capture_locked` sim-time fix); the second build's hashes are the ones below.
- `python verify_payload.py <payload dir>`: OK.
- `gcc -fsyntax-only -std=c11 -Wall -Wextra -Werror -DTELEMETRY_TEST runtime_test.c` (compile check only, the program was not run): rc 0.
- Static check of the on-disk EXE (Python, read-only, section 5): all six copied patterns match exactly once in `.text` (SETVEH twice), and every address in the table above matches the spec value. Stride check PASS.

Not run, and why:
- `TELEMETRY_TESTS=1` gate and `runtime_test.exe`, `test_telemetry.py`: game running (task rule).
- Fuzz, soak: game running.
- C-level unit test of `dcam_state_json`, `dcam_host_ok`, the names parser and the resolver's refusal path: not written as a program, because building and running a test exe with the game up was out of scope. Prepare as a run when the game is closed (OPEN, section 9).
- Live resolve, live mode and index reads, capture-cost measurement: game running. Owner test (section 6).
- Browser check of the tab: no preview server started (the session's browser pane was not used for this, the spec asks for offline checks). Visual check pending.
- `python -m pytest`: not applicable (`test_telemetry.py` is a harness script, batch 5 record section 6).

## 5. Read-only checks on the shipped EXE

`%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe` (3,141,632 bytes, read by Python, not executed, not written). `.text` VA 0x1000, ImageBase 0x400000. The six patterns (parsed from `director_state.inc`) match: DISPATCH 1, MIRROR 1, DIRECTOR 1, PLAYER 1, ROSTER 1, SETVEH 2. Values as in section 3, all equal to the spec. This is a file check. The live process was not read.

## 6. Hashes (SHA-256, checked by hand)

Build folder `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-tvd0`:

| File | SHA-256 |
|---|---|
| `web_telemetry.dll` | `0D1FF08BE7C489BAC40B2449A40DA7FC27828CECC97F4145601D8676E439484B` |
| `telemetry_viewer.js` | `D8900B1752EA94C3EC38D50598362C37788AC79F77EF3A1B7DA40EDCD7F946E0` |
| `workbench_ui.js` (unchanged) | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` |
| `web_telemetry.ini` | `8ABB63E0DA8ED81FA0E32D2DCF3DA787DA6B2EBF3B6C46AC5F10BFD933324535` |
| `web_telemetry.dll.mod.json` | `C8EBC04B154A0D6F5E934260B2C0E84B2A665F3ED696D38DF1055329C2B0C3D3` |

The DLL in the build root and in `payload\` are byte-identical (same hash). `Build verified:` in the build log names the old `-b4` path (known SDK defect, batch 5 record section 7). Verified by hash instead.

Payload folder: `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-tvd0\payload` (5 files). Staged only, not installed, not promoted, not deployed.

Install files when the owner approves: `web_telemetry.dll`, `telemetry_viewer.js`, `workbench_ui.js`, `web_telemetry.ini`, `web_telemetry.dll.mod.json`.

## 7. Owner live test T1, read-only (from spec section 7, adapted)

Run only in a single-player race with the game closed or in a quiet session, and only if the owner decides to test. Steps:
1. Start the game, install the payload only if the owner approves that install. Open `http://localhost:8080/` (or the port in `web_telemetry.ini`), then the TV Director tab.
2. `GET http://localhost:8080/director/state` in the browser (or the tab). Pass: `camera_engine` = `resolved`, `camera_addresses.director_ptr` = `0x005EA528`, `veh_stride` = `0x000048C0`.
3. Press the native camera key 3 times. Pass: `mode` and `index` change with each press (read the raw values, do not trust the label). The tab updates within about 1 s.
4. Switch cars with the native keys. Pass: `followed.roster_id` and `car_name`/`driver_name` change to the car on screen. Record the `player.roster_id` as well.
5. Check the sim: `sim_time_s` and `capture_sequence` advance between two reads. If identical samples repeat, the sim is paused (memory rule). Do not judge reads on a paused frame.
6. Check `/telemetry` (the existing page) still shows all cars and the race is unaffected (no race-data change from phase 0 by design).
7. Record: each read with capture time, the fact/inference split, and any `refused` reason. Put the result in a dated operations record under the telemetry component (spec 7 T14). Do not put any key or token in it (none is used).

Abort: crash, a race result or timing change, or any write attempted (phase 0 has no write path).

Also check (open items): the `followed.roster_id` for each car against `roster_index_4898` and `driver_id_489c` (OPEN, section 9).

## 8. Decisions and spec points

- Hook: no detour in phase 0. The reads run in the existing broker callback. Spec 13 R1 (Option C) and D-TV1 are still open for phase 1 writes.
- Kill switch: `[Director] Enabled` default 1 (phase 0 request). 0 gives 404 and a hidden tab.
- Host allowlist (spec 6.3): applied to the new route only. Other routes unchanged (FACT: no Host check exists).
- Spec 1.2 / R2 (stale statement about `VIEWER_ACCESS_SPEC` and `SECURITY_REVIEW`): not edited here. Phase 0 needs the loopback peer and the Host allowlist only; the security must-fix list (M1-M7) is for phase 2 network work.
- Spec 13 R3: edits to `web_telemetry.c` and `telemetry_viewer.js` were made by this task only. The task statement says no other agent edits these files now; that was not checked independently.
- D-TV4 (mod.json description): not needed for phase 0 (still read-only).
- D-TV6 (overlay): browser only, as proposed.
- No thresholds (D-TV3, Q4) were used: phase 0 has no driver list.

## 9. Doubts and open items

1. Live behaviour is untested. Resolve in the running game, mode and index changes, follow of native car switches, capture cost (`capture_us` p95 under 500 us, measured in the 10 s log line), and the tab in a real browser are all pending owner tests.
2. Roster field meaning conflict between older findings (0x4894 as slot or team id) and the decompile (0x4894 as the chain argument). Recorded as OPEN in `findings.jsonl` (`tvd0-open-0x4894-meaning-conflict-20261013`). T1 step 4 settles it.
3. `API_VERSION` stays 9 while the route is new. Needs a contract decision (`schemas/version.json`, contract test).
4. The `/telemetry` JSON key that marks the human car is not confirmed in code, so the T1 comparison of player roster id against `/telemetry` is a manual check for now.
5. Host allowlist is for this route only. Remote (`Remote=1`) LAN viewers get 403 on `/director/state` by design (Host is not localhost). The tab then shows "No answer from the game PC" for them.
6. Names: `Default.cfg` is read on the HTTP thread every 5 s (disk, not game memory). Not measured on a slow disk.
7. Default.cfg name order is the one camera_editor uses; ONBOARD index N maps to the Nth non-fixed name. Confirm with the owner's Default.cfg in the live run.
8. A C test of the new functions is not run (game running). Prepare the run for when the game is closed: resolver refusal on a stride change, names parse on a fixture, Host header cases (`localhost:8080`, `127.0.0.1:8080`, `[::1]:8080`, `evil.example`, missing Host, `localhost:abc`), kill switch 0/1.
9. The build regenerates the `generated\` stubs and rewrote `mod.json` (only the pinned hash changed). Both are noted in section 1.
10. Workspace validator not run with the right quoting (section 0).

## 10. Restoration

- Revert the DLL and sidecar changes: copy each `*.BEFORE_20261013_tvd0` over its file (for `mod.json`, set the `telemetry_viewer.js` sha back to `37F87A6F62CC3E387744302ED88876BBB197524FB1B1F444738A40F4B6F6004D`). Then run `pin_sidecars.py --check`.
- `director_state.inc` is new. Without the include in `web_telemetry.c` it is inert. Owner or quarantine removes it (agents never hard-delete, policy section).
- The payload and build folder are in `%TEMP%` only; nothing was installed.

## 11. Documentation gate

- Durable record: this file. Indexed from `CURRENT_STATE.md` (newest entry).
- Findings appended to `f1c_ida_decompiled\findings.jsonl`: `tvd0-camera-sigs-on-disk-exe-20261013` (FACT), `tvd0-roster-field-4894-chain-arg-20261013` (FACT), `tvd0-open-0x4894-meaning-conflict-20261013` (OPEN), `tvd0-capture-callback-director-reads-20261013` (DERIVED), `tvd0-open-live-unverified-20261013` (OPEN), `tvd0-no-hook-needed-phase0-20261013` (DERIVED).
- Fact and inference are split by tier in each entry above. Pending validation: section 9.
- Spec not edited (its section 12 "documentation pending" is still the owner's to approve).

## Fix 1 (live refusal)

Date: 2026-10-13. Scope: the owner's live report that `GET /director/state` answered `camera_engine = refused` (`code recognised as unknown ...`) while Camera Editor was loaded, plus the Recordings wording request. Read-only stays read-only: no game write, no new detour, no POST route, no install, no deploy. Offline only: the game was running, so no `runtime_test.exe`, no `TELEMETRY_TESTS=1`, no fuzz, no live read.

### Cause

- FACT (Camera Editor source): `development\mods\camera_editor\plugin\camera_editor.c` installs two detours in the game process. `apply_hook` at 0x409A00 (`APPLY_STOLEN` 8, line 51, detour line 766). `dispatch_hook` at 0x409440 (`DISPATCH_STOLEN` 12, line 52, detour line 769). `detour()` (lines 712-731) writes `E9 rel32` at +0 and 0x90 over the rest of the stolen bytes.
- FACT (on-disk EXE, read-only PE scan in `%TEMP%\tvd0fix\pe_scan.py`): the phase 0 whole-prologue pattern `A0????????535633DB573AC3` matches once, at 0x409440. Those 12 bytes are exactly the bytes Camera Editor overwrites.
- FACT (Camera Editor log, `F1 Challenge V10\plugins\lightloader\camera_editor_log.txt`, 17:52:25): `resolved: apply 00409A00 switch 00409440 ...` with no `camera switch hook failed` line, so the dispatch detour was installed in that session.
- FACT (offline reproduction, `%TEMP%\tvd0fix\dcam_offline_test.c`, scenario S2): with the 12-byte dispatch detour applied to the on-disk bytes, the old whole-prologue pattern has 0 matches. The other five patterns (MIRROR 0x409890, DIRECTOR 0x49F79F, PLAYER 0x40D825, ROSTER 0x42D2F9, SETVEH 0x42BBD0 and 0x42BC40) lie outside both detour ranges.
- FACT (apply detour is not the cause): scenario S1 (apply detour alone) still resolves, because the read-only resolver never used the apply pattern. The owner's suggestion of 0x409A00 is therefore not the culprit; the dispatch detour is.
- DERIVED (live cause): the live refusal text comes from the `pushes >= 100` branch after the dispatch pattern failed, and the detour is the only difference the static and offline checks found. Not read in the live process. Live proof is the owner check below.
- Also found: the old refusal text said "not this camera code base" for code that is the same build, which hid the failing step. Fixed (see change 4).

### Change (`director_state.inc` only; no other game-facing file changed)

1. The camera switch routine is found by its body anchor `8B4424108BF1BF02000000752183F803751C8B0D` (`DCAM_SIG_DISPATCH_BODY`, unique in .text at 0x40944C, which is routine +12). The routine start is the anchor minus 12 (`DCAM_DISPATCH_PROLOGUE`).
2. `dcam_prologue_ok()` accepts the original prologue (`A0 ????` then `53 56 33 DB 57 3A C3`) or the detour form (`E9 rel32` then seven `90`). Any other prologue is refused with `camera switch routine (prologue is neither the original nor a 5-byte jump)`.
3. `dcam_resolve()` counts every pattern first and fills a diagnostic buffer. `dcam_try_resolve()` logs one line per process, for the first section that fails with code hits: `director: pattern counts in .text (first step failing: ...): dispatch_body=N mirror=N director=N player=N roster=N setveh=N [dispatch_prologue=jump(detoured by another mod)]`.
4. The `pushes >= 100` refusal keeps its prefix `code recognised as unknown (not this camera code base)` and adds `; last failing step: <step>`. `DCAM_REASON_MAX` is 224 (the first build with `-Werror` caught a 160-byte truncation; fixed before the final build).
5. Unchanged on purpose: the vehicle stride refusal (`0x48C0`, `vehicle layout (different code base)`, `refuse_now`), the `pushes >= 100` refusal, read-only access (`copy_game` reads only, no writes, no detour, no POST), the Host allowlist, the kill switch.
6. Chunk boundaries: `dcam_try_resolve()` copies each executable section whole into one heap buffer (the 64 KB `DCAM_SCAN_CHUNK` only splits the copy). A match cannot be cut at a chunk boundary. Tested with a DIRECTOR match straddling 65536 (S-test PASS). No code change needed.
7. The apply pattern is not used (it was not copied in phase 0), so the 0x409A00 detour is irrelevant to the read-only resolver. No change needed.

### Owner UX request (Recordings, `telemetry_viewer.js`)

- Every delete or recycle text now names the Windows Recycle Bin: the three row buttons "Move to Windows Recycle Bin", the dialog title "Move to the Windows Recycle Bin", the dialog text "... to the Windows Recycle Bin? The files go to the Windows Recycle Bin (restore them there).", the job line "Files go to the Windows Recycle Bin; the list updates when it is done.", the archive option "Move the working copy to the Windows Recycle Bin", and the manager note.
- Server notices and job result lines in `storage_api.inc` and `storage_jobs.inc` use the same wording (20 lines changed, comments included; no logic change).
- `viewer_test.js`: 7 new checks (no bare "Recycle Bin" in the three files, the button and dialog labels, the job line, the archive option, three row buttons).

### Tests (offline)

- `node viewer_test.js telemetry_viewer.js workbench_ui.js` (in the mod folder): `VIEWER PASS 438 checks`, rc 0 (phase 0 record: 431; +7 new).
- `python pin_sidecars.py`: pinned the new viewer hash (`D8900B17...` to `5FEDC849...`). `python pin_sidecars.py --check`: `pins: OK`. `python pin_sidecars.py --payload <build dir>`: pins written. `python -I verify_payload.py <build dir>`: `payload gate: OK (5 files)`.
- Build: `TELEMETRY_BUILD_DIR=%USERPROFILE%/AppData/Local/Temp/f1c-web-telemetry-1.1.0-tvd0b python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry` from `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled`: rc 0, `gcc -Wall -Wextra -Werror` clean, payload gate OK. Two earlier attempts failed before the final build (the 160-byte truncation, then the stale viewer pin); both fixed. The build's `Build verified:` line still names the old `-b4` folder (known SDK defect); the outputs were verified by hash here.
- Offline resolver test: `gcc -std=c11 -Wall -Wextra -Werror dcam_offline_test.c` in `%TEMP%\tvd0fix`. It reads the EXE file bytes only, opens no process, and extracts the pure resolver from `director_state.inc` at run time, so it tests the built code. Result: `OFFLINE RESOLVER TEST PASS (0 failing checks)`, 12 checks: S0 clean on-disk bytes resolves to the record's addresses; S1 apply detour resolves; S2 dispatch detour (old pattern 0 matches, new resolves to 0x409440); S3 both detours (live case) resolves; S4 stride 0x48C1 refused `vehicle layout`; S5 unknown prologue refused; S6 jump with a non-NOP fill refused; S7 body changed refused; chunk-boundary match found.
- Not run: `runtime_test.exe`, `TELEMETRY_TESTS=1`, `test_telemetry.py`, fuzz, soak, live resolve, browser check (game running; per task rule).
- Workspace validator (`tools/workspace/check-workspace-structure.ps1`, now run with forward slashes): 2 errors, both pre-existing and unchanged (`.claude` and `plans` top-level directories).

### Hashes (SHA-256, build `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-tvd0b`)

| File | SHA-256 | Before this fix |
|---|---|---|
| `web_telemetry.dll` | `83C501B41C2E52786E16916BE8E74E188748317797083973F71727732ED29195` | `0D1FF08B...` |
| `telemetry_viewer.js` | `5FEDC8499495FC73910EF352F831223900F3127FCFFDD232F7B1C2A338DE2F75` | `D8900B17...` |
| `workbench_ui.js` (unchanged) | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` | same |
| `web_telemetry.ini` (unchanged) | `8ABB63E0DA8ED81FA0E32D2DCF3DA787DA6B2EBF3B6C46AC5F10BFD933324535` | same |
| `web_telemetry.dll.mod.json` | `20ECB0E3684F659BCF052E5A6771155D5592AE4BA14CA249F5788BB0DD56ECE7` | `C8EBC04B...` (viewer pin) |

Payload (staged only, not installed, not promoted, not deployed): `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-tvd0b\payload` (5 files). The DLL in the build root and in `payload\` is byte-identical.

### Restoration

- Backups taken before the first edit this task, suffix `.BEFORE_20261013_tvd0fix`: `director_state.inc`, `telemetry_viewer.js`, `viewer_test.js`, `workbench_ui.js` (not changed), `storage_api.inc`, `storage_jobs.inc`, `web_telemetry.c` (not changed), `mod.json`, `TV_DIRECTOR_PHASE0_RECORD_2026-10-13.md`, `CURRENT_STATE.md`, and `f1c_ida_decompiled\findings.jsonl`. The record and `CURRENT_STATE.md` are also copied to `workspace_organization_audit\TVD0FIX_BEFORE_2026-10-13\`.
- `mod.json` differs from its backup only in the `telemetry_viewer.js` sha256 (`D8900B17...` to `5FEDC849...`).
- `findings.jsonl`: four entries appended (`tvd0fix-*`). Another agent appended one co-op entry after my backup; it was kept. Restore from the backup would remove both, so do not restore the whole file.
- Restore the DLL and sidecars: copy each backup back, then `python pin_sidecars.py --check`.

### Live check (owner, pending; not run)

Only if the owner decides to test, with the game closed or in a quiet session. The payload is installed only if the owner approves.
1. Open `http://localhost:8080/director/state` (or the TV Director tab). Pass: `camera_engine` = `resolved`, `camera_addresses.view_dispatch` = `0x00409440`, `veh_stride` = `0x000048C0`.
2. Log `web_telemetry` (in the game log): the `pattern counts in .text` line. With Camera Editor loaded it should say `dispatch_prologue=jump(detoured by another mod)`.
3. Refusal (if it still refuses): record the `last failing step` text and the pattern-count line, and open a new fix record.
4. Recordings page: every delete control and notice reads "Windows Recycle Bin". Do not delete anything to test.
Abort: any crash, race timing change or write attempt.

### Documentation gate

- Durable record: this section. Indexed from `CURRENT_STATE.md` (newest entry).
- Findings: `tvd0fix-dispatch-prologue-detoured-by-camera-editor-20261013` (FACT), `tvd0fix-resolver-dispatch-by-body-20261013` (FACT), `tvd0fix-live-refusal-cause-20261013` (DERIVED: the live cause is inferred, not read), `tvd0fix-live-resolve-open-20261013` (OPEN).
- Fact and inference are split by tier above. Pending validation: the live check above and the Recordings page visual check.


## Live T1 result (2026-10-13-18:25, owner PC, single-player race)

Installed: `web_telemetry.dll` 62458537..., `telemetry_viewer.js` 5FEDC849... (bundle `F1_JOINT_TEST_2026-10-13`). Camera Editor loaded. Reads: Claude via `curl http://localhost:8080/director/state` (read-only HTTP; no gate, no runtime exe), change log `%TEMP%	vd_t1_poll.log` (polled every 0.25 s); owner pressed keys and sent a screenshot of the tab.

| Step | Result | Evidence (FACT unless marked) |
|---|---|---|
| 2 resolve | PASS | `camera_engine=resolved`, `director_ptr=0x005EA528`, `view_dispatch=0x00409440`, `veh_stride=0x48C0`. The earlier Camera Editor refusal is fixed. |
| 3 camera key | PARTIAL | `mode` changed 2/0/1/2 (TVCOCKPIT/NOSECAM/COCKPIT) within about 1 s, label matched the game view in the screenshot. `index` stayed 0 for every read: the owner used only the cockpit-family keys, so the TV/onboard index is untested. |
| 4 car switch | PASS | Followed car changed Steppe/Ferrari (roster 0) to Coulthard/BAR (roster 14) and back, several times. `player.roster_id=0`. |
| 5 sim advance | PASS | 18:23:46 to 18:23:49: `sim_time_s` 33.343 to 35.859, `capture_sequence` 462 to 499, sample age 31-47 ms. |
| 6 /telemetry unaffected | not checked | |
| capture_us p95 | not checked | log line not read |

Roster fields: for both cars `roster_index_4898` equals `driver_id_489c` (31 for Steppe, 85 for Coulthard); `followed.roster_id` (0x4894) was 0 and 14. FACT for these two cars only. That 0x4894 is the slot index is INFERENCE (fits the 2026-08-02 finding); team id is not ruled out (two cars only).

UI review (owner rule 2, Claude, from the screenshot): developer wording (`roster 0`, `mode 2 index 0 count 17`, `signatures matched (read-only resolver)`); angle buttons look clickable but are read-only; ONBOARD001-017 names say nothing; lots of empty space, no on-air feel. Redesign planned with phase 1, not done.

Still open: TV/onboard camera index (press the TV camera key and cycle onboard cameras), step 6, capture cost, more cars for the 0x4894 meaning.

### Live T1 follow-up (18:26-18:35)

- Onboard: PASS. Mode 4 with index 0..16 tracked ONBOARD001..ONBOARD017 as the owner cycled (some indexes skipped by the 0.25 s poll). `count` 17 matches the names list.
- Swingman: mode 3 SWINGMAN read correctly (owner confirmed swingcam on screen at 18:35:10).
- Circuit (trackside TV) cameras: BLIND. Owner pressed PgDn and the screen showed circuit cameras; three reads 00-00-00-00-00-00 still gave `mode 3 index 0 SWINGMAN` with sim time advancing (104.2 to 107.4 s). FACT: director +0x34/+0x3C do not identify a circuit camera. INFERENCE: PgUp/PgDn cycle a group inside mode 3 (swingman plus circuit cameras) whose selection lives in another field or object (candidates: director +0x2C/+0x30/+0x50, the objects at +0x2C/+0x30 used by modes 7/8 in 0x409440). Must be found before TV Director phase 1 (a TV director without circuit cameras is not useful).
- CORRECTION 2026-10-13 (after TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION_2026-10-13.md, sections 4.1, 4.2, 4.6): the INFERENCE above is REFUTED. PgUp and PgDn do not cycle a group inside mode 3. PgDn (state 5) sets the active view pointer (manager+0x08, 0x005EA518) to a circuit group (0x005EA530 or 0x005EA52C) and does not write the director mode (+0x34). Director +0x2C and +0x30 belong to director modes 7 and 8, not to circuit cameras. The PROVEN facts L2 and L3 stand. The line above is kept unchanged for the record.
- Owner (18:36): from TVCOCKPIT, PgDn shows circuit cameras on screen while the data still says TVCOCKPIT. Owner verdict: the camera data is NOT trustworthy until investigated. Investigation assigned to a Haiku agent: TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION_2026-10-13.md (pending).

## View owner fix (2026-10-13)

Scope: the owner's live T1 follow-up showed circuit (trackside) cameras on screen while `/director/state` still reported TVCOCKPIT or SWINGMAN. Investigation: [TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION_2026-10-13.md](TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION_2026-10-13.md) (section 5, the proposed fix, is implemented here). Still phase 0: read-only, no write, no detour, no POST route, no install, no deploy. Game closed at the start of this task.
Stage: implement and verify, offline only. NOT live-tested. `governance\PRODUCT_STATUS.json` not changed.

### Change

- `director_state.inc`, read in the same capture as the existing reads (game thread, `g_telemetry_cs`, same `copy_game` and plausibility checks):
  - active view pointer = manager+0x08 (0x005EA518); StaticCam group = manager+0x1C (0x005EA52C); TrackingCam group = manager+0x20 (0x005EA530). The manager is the resolved `director_ptr - 0x18`.
  - Identity: the director object vtable must be 0x0059E60C, the StaticCam group 0x0059E678, the TrackingCam group 0x0059E6AC. These come from the on-disk EXE `.rdata` (investigation 4.4, FACT for the bytes; the meaning "view on screen" is FACT from the game's own test, live proof pending).
  - Group entry count at +0x10 is reported as `circuit_count` (a count only). No circuit camera index and no followed car are reported (OPEN, investigation 4.9 and 4.10).
  - Pure classifier `dcam_view_owner`: `director` only when the active pointer equals the director object and its vtable matches; `circuit_static` or `circuit_tracking` when it equals the group object with a matching vtable; otherwise `unknown`.
- JSON, additive:
  - `view_owner` (`director` | `circuit_static` | `circuit_tracking` | `unknown`), `view_owner_reason`.
  - `active_view_value`, `static_group_value`, `tracking_group_value`: the raw pointer values read in this sample (null when not readable).
  - `circuit_count` (owning group only, else null) and `director_mode_raw` (raw director +0x34, null without a sample).
  - `camera_addresses` adds the slot addresses `active_view_ptr` 0x005EA518, `static_group_ptr` 0x005EA52C, `tracking_group_ptr` 0x005EA530 (same convention as `director_ptr`).
  - `mode`, `index`, `count` and `angle` are the on-screen values only when `view_owner` is `director`. For a circuit owner: `mode`, `index`, `count` null and `angle` "CIRCUIT CAMERA". For `unknown`: `angle` null.
- `telemetry_viewer.js` (TV Director code only): the Angle row shows "Circuit camera (trackside)" for a circuit owner, "unknown" for unknown, and the angle for director. The new row "Director stored" shows "mode N (not on screen)" only for a circuit owner. No angle button is highlighted for a circuit or unknown owner. Helpers `tvdCircuit` and `tvdAngleText` are exported for the test. Not touched: `storage_*.inc`, writer code, the Recordings page.
- `viewer_test.js`: 9 new checks in the block "View owner fix (2026-10-13)".
- `web_telemetry.c`, `VERIFICATION_MANIFEST.json`, `mod.json`, `f1c_mod.json`, `web_telemetry.dll.mod.json`: only the viewer pin changed (`pin_sidecars.py`, old `5FEDC849...` to `9C559DC3...`).
- Not changed: resolver, read safety, kill switch, Host allowlist, `API_VERSION` stays "9" (the new fields are additive; no schema in `schemas\` covers `/director/state`; the contract decision stays with the owner, doubt 3).

### Hashes (SHA-256, build `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-viewowner`)

| File | SHA-256 | Before this fix |
|---|---|---|
| `web_telemetry.dll` (build root and `payload\`, byte-identical) | `4A2559159B975C30F56A07B469E9A7A1709E04BC46EA31A8DB39460843E00B8F` | `83C501B4...` (tvd0b, staged) |
| `telemetry_viewer.js` | `9C559DC3957B08ACA4EAD4E6F0C6533F5206822A9D2FE585E6E536DB0BC975F6` | `5FEDC849...` |
| `workbench_ui.js` (unchanged) | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` | same |
| `web_telemetry.ini` (unchanged) | `8ABB63E0DA8ED81FA0E32D2DCF3DA787DA6B2EBF3B6C46AC5F10BFD933324535` | same |
| `web_telemetry.dll.mod.json` | `551D032D3D02F3247AC0F406338B4DDA5D63D8516636C279D3C3E5BA1750C3C0` | `20ECB0E3...` |
| `director_state.inc` (source) | `FA2C6A73E0C07A40489225913A5F6CFBE858D7A6D13A59480139ACC10F5BFCDB` | (backup `.BEFORE_20261013_viewowner`) |
| `viewer_test.js` (source) | `E88B803D055D00846DA4973D862C099EBB0FD652F88B8EE32910AC0E0C0B3E75` | (backup `.BEFORE_20261013_viewowner`) |
| `web_telemetry.c` (source, pinned viewer constant only) | `CEBC3982DAECD1CF0996E2DB33873895439426F11F3FC3AA13CBE8738A5E4C58` | (backup `.BEFORE_20261013_viewowner`) |

Payload (staged only, not installed, not promoted): `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-viewowner\payload` (5 files).

### Tests (offline)

- `node viewer_test.js telemetry_viewer.js workbench_ui.js` (mod folder): `VIEWER PASS 447 checks` (438 before, plus 9 new), rc 0.
- Classifier, C: `view_owner_test.c` compiled with `gcc -std=c11 -Wall -Wextra -Werror` in `%TEMP%\viewowner`. The code under test is extracted from `director_state.inc` at run time (`extract_owner.py`), so the file itself is tested: `VIEW OWNER TEST PASS (14 checks)` (director, static, tracking, unknown on each vtable and pointer mismatch, zero and implausible pointers, the vtable and offset constants).
- Build: `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry` with `TELEMETRY_BUILD_DIR` in `%TEMP%`, `TELEMETRY_TESTS` unset: rc 0, `gcc -Wall -Wextra -Werror` clean, sidecar pins OK, payload gate OK (5 files). A first build failed on a quoting error in the new hex helper (fixed, not a logic change). The `Build verified:` line names the old `-b4` folder (known SDK defect, see the T1 record); outputs verified by hash above.
- `python pin_sidecars.py --check`: `pins: OK`. `python pin_sidecars.py --payload <build dir>`: pins written. `python -I verify_payload.py <build dir>`: OK.
- Not run, and why: `TELEMETRY_TESTS=1` gate (Claude runs the full gate), `runtime_test.exe`, `test_telemetry.py`, fuzz, live reads, browser check. The JSON output of `dcam_state_json` was not executed; it is checked by the build (format placeholders and `-Werror`) only.

### Restoration

- Backups before the first edit, suffix `.BEFORE_20261013_viewowner`: `director_state.inc`, `telemetry_viewer.js`, `viewer_test.js`, `web_telemetry.c`, `VERIFICATION_MANIFEST.json`, `mod.json`, `f1c_mod.json`, `web_telemetry.dll.mod.json`, this record, `CURRENT_STATE.md`, and `f1c_ida_decompiled\findings.jsonl`.
- To revert the code: copy each `.BEFORE_20261013_viewowner` back over its file, then `python pin_sidecars.py --check`. Do not restore `findings.jsonl` as a whole (another agent appended lines after the backup); remove only the line with id `tvd-viewowner-impl-20261013`.
- `generated\*` stubs were regenerated by the build (no backup; generic template text, as in the earlier records).

### Owner live test (pending; not run)

Only with the game closed or in a quiet single-player race, and only if the owner approves the install of the staged payload. Read-only: `GET /director/state` (browser tab or `curl http://localhost:8080/director/state`). Camera Editor loaded as in T1.

1. Start with the native view on TVCOCKPIT. Read three times with sim time advancing. Expect `view_owner` = `director`, `angle` = `TVCOCKPIT`, `director_mode_raw` = 2.
2. Press PgDn once (circuit cameras on screen). Expect `view_owner` = `circuit_static` or `circuit_tracking`, `angle` = `CIRCUIT CAMERA` (not TVCOCKPIT), `mode` null, `director_mode_raw` still 2, `circuit_count` > 0, and the matching `*_group_value` equal to `active_view_value`. The tab shows "Circuit camera (trackside)" with no highlight.
3. Press PgUp or Insert to return. Expect `view_owner` = `director` again, with the on-screen angle and the read-back mode (SWINGMAN may read as 2 because of the dispatch remap, see the T1 record).
4. Check that `sim_time_s` and `capture_sequence` advance between reads. Identical samples mean a paused frame, not a stable view.
5. Record each sample in a dated operations record under the telemetry component: time, key pressed, `view_owner`, `view_owner_reason`, `angle`, raw pointers. No credentials.

Fail signs: `view_owner` = `unknown` after PgDn (the pointer or vtable assumption is wrong; record `view_owner_reason` and the three raw values). `angle` = TVCOCKPIT while the screen shows a circuit camera (the fix did not take effect; check that the owner DLL is the staged hash).

### Open items

- Circuit camera index and followed car: OPEN (investigation 4.9, 4.10; the fix reports only `circuit_count`).
- The three vtables are new hard-coded constants in mod code. `AGENTS.md` forbids new raw addresses in mod code without review. Recorded as an exception for owner or contract review; the slot addresses themselves are relative to the resolved manager.
- `API_VERSION` stays "9" with additive fields (see Change).
- Live proof that 0x005EA518 is the on-screen owner: pending (owner test above).

### Documentation gate

- Durable record: this section. Indexed from `CURRENT_STATE.md` (TV Director entries).
- Findings: `tvd-viewowner-impl-20261013` (OPEN) in `f1c_ida_decompiled\findings.jsonl`.
- Fact and inference: the pointer offsets and vtable bytes are FACT (investigation 4.3-4.4); "on-screen owner" is FACT by the game's own test and OPEN live; the classifier result is tested offline only.
