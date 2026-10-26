# Tape playback: Claude review and end-to-end check, 2026-10-14

Product: Telemetry (`web_telemetry`) 1.2.0 (unreleased), API 10. Author: Matias. Reviewer: Claude.
Stage: implement -> verify (offline). **Full gate NOT run** (the game was running the whole session). Not installed,
not live-tested.
Spec: [TAPE_SPEC_2026-10-14.md](TAPE_SPEC_2026-10-14.md). Worker records:
[TAPE_SERVER_RECORD_2026-10-14.md](TAPE_SERVER_RECORD_2026-10-14.md) (T-S, Haiku, includes section 12 follow-up),
[TAPE_VIEWER_RECORD_2026-10-14.md](TAPE_VIEWER_RECORD_2026-10-14.md) (T-V, Haiku),
[REPLAY_SERVER_RECORD_2026-10-14.md](REPLAY_SERVER_RECORD_2026-10-14.md), [REPLAY_VIEWER_RECORD_2026-10-14.md](REPLAY_VIEWER_RECORD_2026-10-14.md).
Tags: FACT = observed in this review (command output, browser, file read). INFERENCE = reasoned, not checked.

## 1. Owner request and decisions
- "A switch: LIVE telemetry (plus this session's data) or PREVIOUS recordings; ALL tabs show the chosen source.
  The whole tool should be a recording playback tool, readable without a running session."
- D1 (owner): phase 1 = game open at the menu serves every recording; phase 2 = standalone player exe (not started).
- D2 (owner): in PAST, live-style pages follow one global tape clock; analysis pages show the whole recording.

## 2. What existed before (FACT, Haiku survey + Claude read)
The LIVE/PAST switch existed (1.2.1 UI). In PAST only analysis pages, Live Gauges and Recordings worked; Race map,
Race Control, Focus dossier, Engine, Chassis, Weather and Replay had no data (404 routes, one static frame, routes
never built). "Open" without a review id led to a dead notice.

## 3. Independent verification (FACT)
- Build from the component sources into `%TEMP%\TAPE_claude_verify\` with the build.bat flags (`-Werror`): DLL and
  runtime_test.exe compile. Suite run from `%TEMP%` (never from the component dir): last run `PASS 6448731 checks`,
  0 FAIL (counts vary between runs: soak tests count by timing). `node viewer_test.js`: `VIEWER PASS 698 checks`.
- End-to-end (never done by the workers): `runtime_test.exe serve` with `RECREV_ROOT=%TEMP%\RECREV_2026-10-14\game`
  (copies of the owner's 7 AUSTRALIA recordings; the game folder was not touched), port 18090, real embedded viewer,
  Browser pane. Session used: CO-OP Race 20-17 (`v2_34c4aad20c99c576`, 2089 s, 24 cars, 530 events).
- Result after the fixes in section 4: Race page (outline, cars moving, tower with Gap/Int/Last/Best/stops,
  Race Control), Car Chassis Live (real pch values, history growing while playing), Braking & Corners and Lap
  Analysis (17 recorded laps per side, A vs B), Focus page and `/driver?t=900` (`p 3, laps 7, pits 1`), transport bar
  on every page, keys from any page. `/replay/info` answers for all 7 sessions.

## 4. Bugs found in review and fixed (FACT)
| # | Bug | Fix | By |
|---|---|---|---|
| 1 | Every float RAW32 channel decoded as its bit pattern in every review route (`ll` = 1120151552; pch 24..63, rx 102..117) | `sa_rv_real` uses the writer's own rule `sw_raw_is_int(id)` / `sv2_f32_from` (`storage_api.inc` ~843) | Claude |
| 2 | Tape frame `session_generation` = hash of id, review trace docs carry `gen 0`: viewer rejected every recorded lap (my spec 2.3 caused it) | frame gen 0 | T-S (follow-up item 7) |
| 3 | Tower Gap/Int/Best empty (`tt`, `bl` not in the file) | derived from lap EVENTs up to t (`tt` = newest lap event t - t_first, `bl` = min last_lap_s) | T-S (items 1-2) |
| 4 | `/replay/info` role of the CO-OP remote car = "ai" (value at session end) | remote if channel 98 was 2 at any tick | T-S (item 4) |
| 5 | Viewer: `wbFeed false` left `wb.sessionGeneration` null, seeding never matched | `Viewer.onFrame` adopts generation, track key and length without pushing samples (`telemetry_viewer.js` ~1022) | Claude |
| 6 | Review `/trace/data` required `car=1000+slot`; the console sends `car=<slot>` like the live route (`http_car_select`): every v2 review seed got 400 (pre-existing since R1-B) | adapter accepts both (`storage_api.inc` `sa_api_review_route` start) | Claude |
| 7 | No side auto-picked when the tape starts before positions exist | one-time pick in review once `pickDefaultSides` finds a running car; user picks kept (`telemetry_viewer.js` end of `Viewer.onFrame`) | Claude |

Before-copies of Claude's edits: `%TEMP%\TAPE_before_2026-10-14\claude\` (`storage_api.inc` before fix 1,
`storage_api.inc.before_carfix` before fix 6, `telemetry_viewer.js` before fixes 5 and 7). Line endings kept (LF).
Test gap: fixes 5, 6 and 7 have no unit test yet (verified in the browser only). OPEN.

## 5. Facts learned
- The viewer and workbench are embedded into the binary at compile time (`.incbin "telemetry_viewer.js"`,
  `web_telemetry.c` ~8383): a viewer change needs a rebuild to be served.
- CO-OP Race 20-17: the host's file records the remote car (slot 1, controller role 2) with lap number 2 above the
  others (lap 6 while the leader is on lap 4 at t=603 s; first lap EVENT at 154 s with `last_lap_s -1`). The tower
  therefore shows the others "+2L". This is the recorded game value, not a playback bug. OPEN: game-side cause.
- Weather is not recorded in v2 (no channel); the page says so.
- Sector times (ls1/ls2/bs1/bs2) are not recorded; S1/S2/S3 columns stay empty in playback. OPEN: writer channel.

## 6. Open items
1. Full gate with the game CLOSED (PowerShell 7): `$env:TELEMETRY_TESTS='1'; .\build.bat` in the component dir; it also
   runs the new `test_telemetry.py` HTTP checks, which ran nowhere yet. Payload pin step if it stops there
   (`python pin_sidecars.py --payload "$env:TEMP\f1c-web-telemetry-4.4.0"`).
2. Owner live check, spec section 7 A1-A4 (game at the menu, PAST -> Recordings -> Open a finished race).
3. Unit tests for fixes 5-7; `/laps` items have no time, so the Race Control fastest-lap line is not tape-filtered.
4. Dossier name fallback "Car N" vs frame "#num" (inconsistent label for old recordings).
5. `?page=<id>` in a review URL is ignored except `replay` (pre-existing rule).
6. Chassis page "driver inputs" panel and tyre temps show 0 in playback (INFERENCE: those fields are not recorded).
7. Workspace hygiene: `telemetry.json` (148 KB) and `__pycache__/` in the component dir are test output from the
   workers' runs (disposable; owner `retention.py purge`, not deleted by an agent).
8. Phase 2 (standalone player, game closed): not started; needs its own spec.
9. Storage soak flake `runtime_test.c` ~3132 (one failure in four worker runs; 0 in Claude's 6 runs). OPEN.

## 7. Restoration
Viewer/server files: worker before-copies in `%TEMP%\TAPE_before_2026-10-14\server|viewer\` and REPLAY copies in
`%TEMP%\REPLAY_before_2026-10-14\`; Claude edits as in section 4. Nothing was deleted. Real recordings untouched.

## 8. Full gate (FACT, 2026-10-14-23:45, game closed)
- Game not running (`tasklist` showed no F1 Challenge process). PowerShell 7: `$env:TELEMETRY_TESTS='1'; .\build.bat`.
- Run 1 stopped at the sidecar pins (expected after viewer edits). `pin_sidecars.py` was then run twice: once by
  mistake as `pin_sidecars.py --help` (the script has no help option; it pinned the sidecars, the intended action) and
  then `--payload "%TEMP%\f1c-web-telemetry-4.4.0"`. Run 2 failed in `test_telemetry.py` line 105 (payload manifest
  still held the old viewer hash, because run 1 never staged the new viewer). Re-pin with `--payload`, run 3:
  **EXIT 0, 45 PASS lines, 0 FAIL**, including `PASS R1-B v2 review ...` (which now also runs the new /weather 404,
  /replay/info, window, 31 s refusal and /replay/frame checks, `test_telemetry.py` 525-554) and
  `PASS served viewer sidecar Node analysis test: VIEWER PASS 698 checks`.
- Pinned hashes: `web_telemetry.dll` FC1B8B3BEC198B79..., `telemetry_viewer.js` 044CA7F3AC5D8C7F...,
  `workbench_ui.js` A3431BCB681D7B32... Files rewritten by the pin script (23:30): `f1c_mod.json`, `mod.json`,
  `web_telemetry.dll.mod.json`, `VERIFICATION_MANIFEST.json`. Payload staged in `%TEMP%\f1c-web-telemetry-4.4.0\payload`.
- Log: `%TEMP%\TAPE_claude_verify\gate3.txt` (disposable). Not installed; owner live check A1-A4 still open.

## 9. Install (FACT, 2026-10-14, Claude at the owner's request, game closed)
- Bundle `%TEMP%\F1_TEST_2026-10-14_TELEMETRY_120_TAPE\` (install.ps1 / restore.ps1 copied from the FINAL3 bundle;
  files = the gated payload, SHA256SUMS match). Installed into `F1 Challenge V10\plugins\lightloader\`:
  DLL FC1B8B3B..., telemetry_viewer.js 044CA7F3..., workbench_ui.js A3431BCB..., dll.mod.json 03086DE3...; all OK.
  `web_telemetry.ini` KEPT (owner config).
- Replaced files backed up in the bundle's `backup_before_install\` (+ restore_map.txt): DLL 89F4AE0D...,
  viewer 9C2CE07B..., workbench A3431BCB..., mod.json 274B8413.... The replaced DLL is NOT the FINAL3 DLL (65A865E9)
  recorded above: another build was installed after FINAL3 (not traced here). Undo: TAPE `restore.ps1` (game closed).
- Not live-tested yet (owner checks A1-A4).
