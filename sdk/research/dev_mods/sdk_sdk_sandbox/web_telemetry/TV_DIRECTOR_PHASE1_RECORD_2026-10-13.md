# TV Director phase 1 record (2026-10-13)

Scope: spec `TV_DIRECTOR_SPEC_2026-10-13.md` phase 1 (3.2 a-d and f, routes 6.2 and 6.3, live tests T1-T7 to run by the owner), with owner decisions D-TV1 (broker, no detour), D-TV2 (car switch only after live T2), D-TV4 (mod.json description), D-TV6 (browser only), and the task rules: `[Director] Enabled=0` by default, writes only through broker game-thread tasks, battle flag not in phase 1.
Stage: implemented and built offline. NOT live-tested. `governance\PRODUCT_STATUS.json` not changed (no stage change claimed).
Product: Telemetry 1.2.0 (`web_telemetry`). API version stays `"9"` (see doubts).
Game rule: the game was running for the whole task. Nothing was installed, nothing was run except the build (`tools/f1c_mod.py build`, output in `%TEMP%`) and `node --check` (syntax only). `runtime_test.exe`, `TELEMETRY_TESTS`, `viewer_test.js` and all other test programs were NOT run. `F1 Challenge V10` was not read or written.

## 1. What was built

### Game-side commands (`director_state.inc`, included from `web_telemetry.c`)
- Section "TV Director phase 1: commands" at `director_state.inc:~290-660`:
  - `dcam_json_string` (382): small body parser, printable ASCII, no escapes.
  - `dcam_angle_key` (412): `NOSECAM|COCKPIT|TVCOCKPIT|SWINGMAN` = modes 0-3; `ONBOARD:<name>` = mode 4 and the index of `<name>` in the names list (LocalCam order).
  - `dcam_cmd_submit` (426), `dcam_cmd_abandon` (~441), `dcam_cmd_wait` (460): one pending slot; the HTTP thread waits up to 1000 ms for the result.
  - `dcam_find_driver` (485): live pool slot (+0x32C8 is not -1) whose driver name at +0x4620 equals the request; roster id at +0x4894.
  - `dcam_set_angle` (506): fixed modes through the game's dispatch (0x409440, thiscall, `immediate=1`); ONBOARD through dispatch to mode 4, then index (+0x3C), manager+0x2D cleared, mirror setup (0x409890). Range check against the game's count (+0x38). Read-back right after the write. Mismatch = `redirected`.
  - `dcam_chain_switch` (548): car switch through GameStateDispatchChain 0x40D240 (thiscall, ECX = manager). Blocked by `DCAM_CHAIN_VERIFIED 0` (line 339). Read-back of the followed pointer 0x7C54E0.
  - `dcam_exec` (562): runs one command. Refuses when Enabled=0, the engine is not resolved, the director pointer is not plausible, the driver is not live. CUT is refused with no write while the chain is blocked. RESTORE moves the saved angle and reports `partial`.
  - `dcam_cmd_task` (604): broker task body. Runs on the game thread, logs the thread id (`director: command N seq S on game thread T: status (reason)`), publishes the result under `g_telemetry_cs`.
  - `dcam_cmd_json` (640): POST response object.
- Saved mode for restore (spec 9, R4): `dcam_capture_locked` (655), block at `director_state.inc:688`. First valid director sample of each game session (`capture_generation`), mode and index only.
- Config: `g_dcam_enabled` default 0 (`director_state.inc:253`). Settings log text updated in `dcam_settings_read`.
- `dcam_state_json` (GET /director/state): `"phase":1`, new `"write_enabled"`, `"last_applied_seq"` (applied or partial only), `"last_result"` (the last command with status, reason, thread id, read-back), `"saved"` (saved mode and index), `"on_air":{"car_name","scope":"own game"}`. Reads stay on when Enabled=0 (the route no longer answers 404 for that reason).

### HTTP (`web_telemetry.c`)
- `director_post_route` at `web_telemetry.c:8773`: POST `/director/cut`, `/director/angle`, `/director/restore`. Guards in order: Enabled=1 (else 404), Host allowlist `dcam_host_ok` (else 403), loopback peer `http_peer_is_local` (else 403), `X-Telemetry-Action: director` (else 403), `Content-Type: application/json` (else 415), body 1 to 4096 bytes (else 413/400). Then parse `driver` (cut) and `angle` (cut and angle), reject unknown names (400), submit, post `dcam_cmd_task` through `telemetry_broker.post`, wait up to 1 s, respond 200 with the result object. Busy = 409. Broker unavailable = refused, nothing written.
- Dispatch sits before the generic POST 405 branch (`web_telemetry.c:8969`; the first splice was after it and would have been unreachable; fixed before the build).
- GET `/director/state` block at `web_telemetry.c:~9047`: the Enabled 404 branch was removed; Host allowlist kept.
- `TELEMETRY_VERSION` and `PRODUCT_LABEL` bumped to 1.2.0 (`web_telemetry.c:49, 52`).

### Viewer (`telemetry_viewer.js`, TV block `:3157-3520`, exports at `:5326`)
- Driver list from `GET /telemetry` (about 1 Hz): position, lap, metres to car ahead and behind (`ga/gb`, `gai/gbi`), pit lane (pitting enum 2 or more), leader gap and battle as `—` placeholders. Helper `tvdDrivers` (`:3223`).
- On-air strip (`tvdLines`), angle list (names from `/director/state`, click or keys 1-4), Cut (driver row plus angle), Angle on this car, Restore. Results shown with status, reason, read-back and game thread id.
- Hotkeys (`tvKey`, `:3472`): Up/Down move the driver row, 1-4 pick a fixed angle, Enter cuts. Ignored when a text field, select or contentEditable has focus. Enter on a focused button is left to the browser (one click).
- Buttons disabled unless the game PC answers `write_enabled` and `viewer_local`.
- POST: one place only (`tvSend`, `:3452`), header `X-Telemetry-Action: director`, path `/director/<kind>`. No confirm, alert or prompt.
- Pure helpers exported for tests: `tvdAngleKey`, `tvdPitText`, `tvdDrivers`, `tvdKeyAction`, `tvdBody`, `tvdResultText`, `TVD_CSS`.

### Config and manifests
- `web_telemetry.ini`: `[Director] Enabled=0` (was 1; comment rewritten).
- `web_telemetry.dll.mod.json`: version 1.2.0; description changed per D-TV4 (no longer says "Read-only: it never changes the game"); sidecar pins updated by `pin_sidecars.py` (viewer SHA-256 `0C2CB08E...9F0183`, pin check OK).
- `mod.json` (root): version 1.2.0; pins updated the same way.

### Tests (added, compiled, NOT run)
- `runtime_test.c:3852` `test_director_phase1()` (called from `main` at `:3925`): body parser, angle keys, default-off gate, car-switch block constant, one-slot queue, result JSON, Host allowlist. Offline, no game memory. `gcc -fsyntax-only -Wall -Wextra -Werror` passes.
- `viewer_test.js`: phase 0 source rules repointed to the phase 1 block (`:788-800`, `li` check `:~857`), new block "TV Director phase 1" (`:~857` onwards): driver list rules, key mapping, body and result text, one POST only, no confirm/alert/prompt, shipped default `Enabled=0`. `node --check` passes. Not run.

## 2. Build and hashes

Build: `TELEMETRY_BUILD_DIR=%TEMP%\f1c-web-telemetry-1.2.0-tvd1 python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry`, rc 0, `gcc -Wall -Wextra -Werror` clean (after two fixes: format-truncation in the JSON writers, now `_snprintf` with an explicit terminator), sidecar pins OK, payload gate OK (5 files). The build log's `Build verified:` line names the old `-b4` folder (known SDK defect, see phase 0 record); outputs checked by hash here.

| File | SHA-256 |
|---|---|
| `web_telemetry.dll` (build root and `payload\`, identical) | `551D70A61961471AC5A51FF35C889C710142CADBD5521F5B7C0A67924388983E` |
| `telemetry_viewer.js` | `0C2CB08EA726D02639AF7F461FD973674C58E5469B8FE2D0F5DA74DB9C9F0183` |
| `web_telemetry.ini` (payload) | `9C6046D3CCB1D2DF8045DE766FC5F19A70D28FCFAE245F530CCC303EA7893397` |
| `web_telemetry.dll.mod.json` (payload) | `1E8DE5FEE49FC77589487F0DCFCE190C09C1A40FC29324C26F98A49DD9E7814C` |
| `workbench_ui.js` (unchanged) | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` |
| `web_telemetry.c` (source) | `739CAC4A0477BCD698B8DE89A416AE0B4A3A342A96B86F3A255E381AE2066C36` |
| `director_state.inc` (source) | `DC2D7D41A014971EAD06201F04D12F85BE36DCAA637F2FCDE731F609AAEFBD43` |
| `mod.json` (source) | `DEE50979AAC21DE1EC7C15777F0567D55E9C7B456E384B92C2936BE355EFD1AD` |

Payload folder `%TEMP%\f1c-web-telemetry-1.2.0-tvd1\payload` (staged only, not installed, not promoted, not deployed).

## 3. Blocked or not done (stopped on purpose)

1. CAR SWITCH (spec 4.4, D-TV2). The chain code exists (`dcam_chain_switch`, `dcam_exec`) but is behind `DCAM_CHAIN_VERIFIED 0`. Reason: the prologue bytes of GameStateDispatchChain 0x40D240 are not checked anywhere, and the corpus has no byte dump. Reading the game folder to get them was not allowed in this task. Effect now: every CUT is refused with no write; RESTORE moves the angle only and reports `partial`. The owner or the next agent must (a) supply the bytes from a non-game copy, (b) add a prologue check in the resolver, (c) run T2 (AIOutOfTrackEvent side effect, 4.4 step 5) and only then set the flag to 1.
2. Leader gap: `/telemetry` has no time gap to the leader. The column shows `—` (`tvdRenderDrivers`, title text).
3. Battle flag: `—` placeholder (owner: not in phase 1).
4. Read-back timing: read immediately after the write, not on the next frame as spec 4.5 says. T4 records which one the game needs.
5. Driver name match: `dcam_find_driver` compares the request with the game name at +0x4620. The viewer sends the `n` field of `/telemetry`. That these two are the same string is INFERENCE (not checked live). T1 or T4 must confirm.
6. Camera Editor detour: the dispatch call goes through 0x409440, which Camera Editor detours when loaded. The call should reach the original code through its trampoline, but this is untested (T8).
7. HTTP accept loop: a POST may block the single accept loop for up to 1 s while the game thread takes the command (normally one frame). Spec 6.2 allows 1 s (OPEN). Recorded as a risk.
8. Not done: follow switch (phase 2), on-air across clients (phase 2), queue and Take (phase 3), auto-director (phase 4), DIR1 network channel and HMAC (phase 2, needs SECURITY_REVIEW M3 and M7), Host check on the other routes (spec 6.1, not changed).
9. Not done: `API_VERSION` stays `"9"` while routes and fields were added. Needs an owner or contract decision (phase 0 doubt 3).
10. Not done: no change to `VIEWER_ACCESS_SPEC` or `SECURITY_REVIEW` (phase 2).

## 4. Owner live tests to run (spec 7, T2-T7; T1 was run in phase 0)

Run only with the game closed or in a quiet single-player race, and only with the staged payload installed by the owner. No gate exe while the race is live. Check `sim_time_s` and `capture_sequence` advance between reads.

- T2 Car-switch side effects. BLOCKED: cannot run until item 3.1 is done. Pass: positions, times and classification unchanged; no new driver-table entry for B.
- T3 Refused switch. BLOCKED with T2. Pass: `last_result.status` = `refused` with the gate reason.
- T4 Angle set. Enabled=1. Send `/director/angle` with NOSECAM, COCKPIT, TVCOCKPIT, SWINGMAN on the player car. Read back each (`last_result.readback`, and `GET /director/state` mode and index on the next frame). Record the SWINGMAN to TVCOCKPIT remap and whether the read-back is immediate or next frame.
- T5 ONBOARD write. Camera Editor NOT loaded. `/director/angle` with `ONBOARD:<name>` for index 0 and count-1. Pass: correct view; out-of-range index refused (use a name not in the list: 400).
- T6 Restore. After T4 and T5: `/director/restore`. Pass: saved mode back (car part is blocked; expect `partial`).
- T7 Threading. 50 rapid POSTs to `/director/angle` with `curl` from `%TEMP%`. Pass: no crash; each result shows `thread_id` = the game thread id from the log (`director: command ... on game thread T`); final state = last command. 409 answers are expected under load and are a pass if the state is consistent.
- Also: T8 Camera Editor loaded, cut to a disabled ONBOARD view, expect `redirected`. T11 Host header `evil.example` gives 403 (`curl -H "Host: evil.example"`). T12 remote peer: out of scope here (LAN is not enabled). Record each result with capture time and fact or inference split, in a dated operations record under the telemetry component.

## 5. FACT, DERIVED, untested

- FACT (source): routes, guards, names, angle mapping, offsets are the ones in the phase 0 record and spec 4.5, 6.2, 6.3; the driver fields `c.p`, `c.l`, `c.n`, `c.ga`, `c.gb`, `c.gai`, `c.gbi`, `c.pt` are in `web_telemetry.c` car JSON (around line 6003 and the car format string).
- FACT (build): the DLL compiles and links with `-Werror`; the viewer parses (`node --check`).
- DERIVED: the dispatch and mirror calls behave as in camera_editor.c (same thiscall signatures). Not run.
- DERIVED: `write_enabled` gate and loopback guard are as in the code; not exercised.
- UNTESTED (no live run): every write, every read-back, the broker post and the thread id, the Camera Editor interaction, the viewer tab, the hotkeys, the driver list.

## 6. Doubts and open items

1. Blocked car switch (section 3.1). The biggest gap: CUT cannot move to another driver yet.
2. The 404-to-200 change for GET /director/state when Enabled=0 is from the task ("phase-0 reads stay"). The phase 0 record and the TV tab note said the reverse. The tab stays visible with writes off and says so.
3. `API_VERSION` stays "9" (see section 3.9).
4. The `[Director]` kill switch also removes the POST routes (404), which is the spec 9 kill switch.
5. Restore uses the saved mode from the first valid sample of the session, not an owner setting (spec 10 Q10, R4).
6. Warning: the browser pane was not used and no preview server was started (game-served page, no dev server in this folder). Visual check pending.

## 7. Restoration

- Backups before the first edit, suffix `.BEFORE_20261013_tvd1`: `web_telemetry.c`, `director_state.inc`, `telemetry_viewer.js`, `viewer_test.js`, `runtime_test.c`, `web_telemetry.ini`, `mod.json`, `web_telemetry.dll.mod.json`, `f1c_mod.json`, `VERIFICATION_MANIFEST.json`, `CURRENT_STATE.md`, and `f1c_ida_decompiled\findings.jsonl` (the findings file is large and shared; do not restore it as a whole, remove only the `tvd1-*` lines).
- To revert the code: copy each `.BEFORE_20261013_tvd1` back over its file, then `python pin_sidecars.py --check`.
- New file: this record (no backup needed).

## 8. Documentation gate

- Durable record: this file. Indexed from `CURRENT_STATE.md` (TV Director phase 1 entry).
- Findings: `tvd1-*` lines appended to `f1c_ida_decompiled\findings.jsonl` (FACT: build and routes; DERIVED: dispatch and mirror use; OPEN: car switch, read-back timing, name match).
- Fact and inference are split in sections 3 and 5. Pending validation: owner tests T4-T8 and T11 (section 4); T2-T3 blocked.

## 9. Claude review and full gate (2026-10-14 session, game closed)

Review of the TVD1 diff (web_telemetry.c, director_state.inc): no blockers. Angle path ok for owner live T4-T8, T11 with
`Enabled=1`. Car switch correctly refused (`DCAM_CHAIN_VERIFIED 0`). FACT (SDK inspect 0x40D240, raw Hex-Rays):
GameStateDispatchChain calls `SetPlayerVehicleIndex(a2)`, `CameraTargetUpdateOnPlayerChange`, `CameraViewAssignment`,
`VirtualDispatchConditionalAudio`, `AIOutOfTrackEvent(a2)`, `sub_4D8760(1,0)`. So unblocking the car switch needs a
side-effect check of SetPlayerVehicleIndex and AIOutOfTrackEvent, not only prologue bytes. Notes (not blockers): the
POST handler holds the HTTP thread up to 1 s; when the game is paused and broker tasks do not run, each command waits 1 s
then answers refused; `dcam_json_string` matches the key with strstr (localhost-only input).

The full gate (`TELEMETRY_TESTS=1 build.bat`) had never passed after TVD1/recpage. Fixes (tests and pins, plus viewer version
and comment text; no change to engine logic):
- `VERIFICATION_MANIFEST.json` payload hashes re-pinned to staged bytes (were recpage-era).
- Version 1.1.0 -> 1.2.0: `runtime_test.c:160,2046`; `test_telemetry.py:137,192,195,197,207,217,464`; `viewer_test.js:758`;
  `telemetry_viewer.js:32` PRODUCT_VERSION (TVD1 bumped the DLL but not the viewer).
- `runtime_test.c` t3_u12 rebuild test: the request flag is now taken before `sw_rebuild_entries()`, as in
  `storage_writer.inc:1256` (the test had the order reversed; this was the old failure).
- `viewer_test.js` TVD1 block: key '3' is TVCOCKPIT, test now uses '4' for SWINGMAN; missing `src` defined; write_enabled
  check inverted (`< 0` -> `> 0`). Delete dialog test updated to the B1 wording "You can restore it from there.".
- `telemetry_viewer.js:5002` comment says "Windows Recycle Bin" (recycle wording test).
- `python pin_sidecars.py` re-pinned the viewer.

Result: exit 0. VIEWER PASS 466, contract api_version 9, soak 20 workers flat RSS, fuzz 80,000 iterations 0 crashes,
perf p95 191 us (selected) / 188 us (all cars). Payload (in %TEMP%\f1c-web-telemetry-4.4.0\payload):
`web_telemetry.dll` BFBDC73F9C3368E653A11482B100D7D5A97CA649515CA71DDC0B103CBA031B95,
`telemetry_viewer.js` 2EF14FF67ADF668D0FAC51FC5B733308E5393BAF78A3D75FBFEC6A4358175804. The DLL hash 551D70A6 in
section 2 is superseded. This build is NOT in RC2 and not installed.

Owner race log check (10-min race, 22 cars, web_telemetry_log.txt): 472 samples, capture p95 127-281 us (limit 500),
single-frame max 3127 us once. PASS.

Backups: suffix `.BEFORE_20261014_ver120` (runtime_test.c, test_telemetry.py, viewer_test.js, telemetry_viewer.js,
web_telemetry.c, web_telemetry.dll.mod.json, f1c_mod.json, mod.json, VERIFICATION_MANIFEST.json), `.BEFORE_20261014_gaterepin`
(manifest), `.BEFORE_20261014_review` (this file).
