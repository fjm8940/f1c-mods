# TV Director phase 2 record (2026-10-14)

Scope: implementation of `TV_DIRECTOR_PHASE2_RESEARCH_2026-10-14.md` section E, with the owner decisions given in the task. Stage: implemented in source and built offline. NOT live-tested. `governance\PRODUCT_STATUS.json` not changed.

Files changed (only these two, plus the findings file):
- `director_state.inc` (phase 2 code). Backup: `director_state.inc.BEFORE_20261014_tvd2`.
- `runtime_test.c` (offline tests, CRLF kept). Backup: `runtime_test.c.BEFORE_20261014_tvd2`.
- `f1c_ida_decompiled\findings.jsonl` (7 lines appended, ids `tvd2impl-*`). Backup: `findings.jsonl.BEFORE_20261014_tvd2` (1519 lines before; 1526 after).
- Not changed: `web_telemetry.c`, `telemetry_viewer.js`, `viewer_test.js`, `web_telemetry.ini`, `mod.json`, `web_telemetry.dll.mod.json`, `f1c_mod.json`, `CURRENT_STATE.md`. `pin_sidecars.py` and `build.bat` were not run.

## 1. What changed

### Car switch (option B, research A12)
- One stdcall call to SetPlayerVehicleIndex 0x42BBD0 with the pool slot (`dcam_car_step`). Slot = (veh - 0x5EC6D8) / 0x48C0; the roster id must equal the slot (`dcam_car_slot_ok`, research A13).
- Before the call, the 22 bytes at 0x42BBD0 must match `DCAM_SETVIEW_BYTES` exactly (`dcam_setview_bytes_ok`). The twin 0x42BC40 differs only by its store byte (E8 instead of E0), so it is refused.
- Read-back after the call: the followed pointer 0x7C54E0 must equal the target, and 0x7C54DC must equal the car's position in the active table 0x7C54F0. Otherwise the angle is not written and the command is refused.
- Then the angle, through the existing `dcam_set_angle`, in the same task.
- Not called: 0x40D240 (chain), AIOutOfTrackEvent 0x41A0C0, 0x008AD298. `DCAM_CHAIN_VERIFIED`, `DCAM_CHAIN_ADDR`, `DcamChainFn` and `dcam_chain_switch` were removed.

### Gate (`dcam_car_gate`, research E3 step 1)
Order: CarSwitch on; engine resolved; live race session (`g_dcam_snap.session_live`, set from `compatibility_runtime.inc:77` `valid && session_active`); byte check; driver live; roster equals slot; manager state (this[3], +0x0C) not -1, 1 or 2. Each failure returns its own reason, and nothing is written. When the target car is already on screen, no car gate runs and only the angle is written.

### CarSwitch flag
`[Director] CarSwitch` is parsed in `dcam_settings_read`. Default 0. The key is not in the shipped `web_telemetry.ini` (not edited), so the default applies. `Enabled=0` still disables every write.

### Restore
Car = the player pointer (0x7C54E8, resolved), with the same gate and car step, then the saved angle. If CarSwitch=0 and the car differs, the angle alone is restored and the status is `partial`. Status `applied` when both steps read back. `dcam_plan_steps` gives the order: CAR (unless on screen), then ANGLE.

### Status
`dcam_combine_status`: `applied` when the angle reads back as requested; `partial` when the car step ran and the angle did not; otherwise the angle's own status.

### Next-frame verify
- After a command with status applied or partial, `dcam_arm` sets `verify_state = pending` and stores the expected mode, index, and car.
- In `dcam_capture_locked`, the first valid sample after that compares the mode, index and followed car (`dcam_verify_match`). The result is `true` or `false`, and the next-frame sample is stored (`next_frame`).
- `GET /director/state` `last_result` and the POST response carry `verified_next_frame`: `"pending"`, `true`, `false`, or `null` (no write).
- The status stays `applied` when the verify is false. Only `verified_next_frame` changes, per the task.

### Result and state fields
- `readback` now has `viewed_slot`. `next_frame` is `{mode, index, viewed}` or null.
- `GET /director/state` adds `carswitch_enabled`, `viewed_slot` (null unless a car is followed) and `manager_state` (diagnostic, null when unreadable).
- `phase` stays `1` (API version stays `"9"`; the contract pass is still open).
- `angle` for circuit owners stays `"CIRCUIT CAMERA"`, not null as research E4 proposes, because `telemetry_viewer.js` and `viewer_test.js:810` depend on that string. See follow-up 4.

## 2. Tests and checks run

| Check | Command or place | Result |
|---|---|---|
| Syntax, DLL sources, build.bat flags (`-std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsyntax-only`, same include paths) | `web_telemetry.c`, `legacy_formats.c`, `sha256.c`, `f1c_broker.c` | all pass |
| runtime_test build (build.bat test line, output to `%TEMP%\tvd2_rt\runtime_test.exe`) | gcc mingw32 | rc 0 |
| runtime_test run (game not running; `tasklist` grep empty) | `%TEMP%\tvd2_rt\runtime_test.exe` | rc 0, 7761764 checks, 26 PASS lines |
| Phase 1 offline block (updated: `DCAM_CHAIN_VERIFIED` assert replaced by `g_dcam_carswitch_enabled == 0`) | `test_director_phase1` | PASS |
| Phase 2 offline block (new) | `test_director_phase2` | PASS |

Phase 2 block covers: exact-bytes accept; twin refusal; each of the 22 single-bit changes refused; slot and roster rules; circuit-state refusal set (-1, 1, 2); the seven gate reasons (each non-empty, shorter than the field, pairwise distinct); plan order for CUT, RESTORE, ANGLE; status combination; verify match and token; arming; finish; result JSON with `verified_next_frame` (pending, true, false, null) and `viewed_slot`; the default gate reason.

PERF in the run (synthetic, not the game): capture+read p95 354 us, budget 500 us.

Not tested offline: the game-memory paths (`dcam_car_step`, `dcam_gate_for_car`, `dcam_exec`, the capture verify block) and `dcam_state_json`. These need the game and the owner tests in section 4.

## 3. FACT / DERIVED / OPEN

FACT (in source or run):
- 0x42BBD0 byte pattern and twin difference (research A9, file offsets).
- Code paths, gate order and reasons, plan order, status rules, JSON shape: in `director_state.inc`.
- Build and syntax pass; runtime_test rc 0, 7761764 checks.

DERIVED (research, not live):
- The roster id equals the pool slot (A13).
- Manager states -1, 1, 2 hand the view to a circuit camera (A11). The refused set is a constant so the owner can change it after T2b.
- The camera may not visibly move under option B in director view.

OPEN (needs the game):
- Whether the setter moves the view in director view, and whether any race data changes (T2b).
- Whether the verify is too early when the capture runs in the same broker drain as the command (T4'). The verify takes the first capture after the write, as the task said. If the capture runs in the same drain, the sample is taken before the director update.
- Behaviour when the car is not in the active table (the setter returns 104): only inferred from A1.
- What the manager states are in director view.
- Event-264 insert and 0x008AD298 are not triggered by this path, but this is not proven live.

## 4. Owner live tests

Research section 7 lists these (run by the owner, game quiet, sim time advancing, one dated operations record each):
- T2b car switch, option B, one AI car, Enabled=1 and CarSwitch=1 (test build only). Pass: camera follows, `viewed_roster` equals the target, race data unchanged for 60 s, manager state logged, no circuit view.
- T2c numpad baseline (only if the owner wants option A).
- T2d pit-lane target.
- T4' next-frame read-back: cut to the same target twice and to two angles; a Camera Editor redirect gives `redirected` in the verify.
- T5' order of car then angle; SWINGMAN remap.
- T-C1 gap to leader (`gapTo`) against the in-game timing screen.
- T-C2 22-car battle id check (blocks the battle column).
- T-C3 circuit owner, read-only.
- T1 and T3 from section 7 (refused switch with the refused states).

The task also named "T1-T11". Research section 7 has no T6 to T11; phase 1 record section 4 has T4-T8 and T11 (angle, restore, threading, Camera Editor, Host header). Those are not repeated here. Owner decides whether the phase 1 list still applies.

## 5. Follow-ups for the viewer (not done; the viewer file is being edited by another agent)
1. Leader gap column (`TVD_DASH` cell in `telemetry_viewer.js`, about line 3402): use the existing `gapTo(car, leader, true)` (about line 2788). Leader = car with p == 1. Add `tt`, `rt`, `fs` to `tvdDrivers`. Label the value as "at last line crossing". Verify with T-C1.
2. Cut UI wording: show `verified_next_frame` `"pending"` as "sent, not yet verified"; `true` as "verified next frame"; `false` as "not as requested next frame" with the next_frame readback; keep status text as it is.
3. Cut button enabled only when `write_enabled`, `viewer_local` and `carswitch_enabled` are all true.
4. Circuit owner: research E4 says angle null; the code keeps `"CIRCUIT CAMERA"`. Viewer should show "CIRCUIT (static|tracking)" with "camera number unknown" and no angle name either way.
5. `viewer_test.js` needs phase 2 cases (gap rules, pending and verified text, carswitch_enabled gating). Not touched here.

## 6. Restoration
- Restore the two code files: copy `director_state.inc.BEFORE_20261014_tvd2` over `director_state.inc`, and `runtime_test.c.BEFORE_20261014_tvd2` over `runtime_test.c`. Then rebuild runtime_test if needed.
- Findings: remove the 7 lines whose id starts with `tvd2impl-` (last 7 lines of `findings.jsonl`). Do not restore the whole file; other agents appended lines since the backup.
- Phase 1 behaviour with CarSwitch=0 (default) is the same: every car write is refused, the angle and restore paths are unchanged.
- No game folder was read or written. No build output in the workspace (`%TEMP%` only).

## 7. Documentation gate
- Durable record: this file.
- Findings: 7 `tvd2impl-*` lines appended (FACT, DERIVED, OPEN).
- Fact and inference split: section 3.
- Pending validation: section 4. Nothing was run live.
- Not indexed in `CURRENT_STATE.md` (not edited; owner or viewer agent to add a link).
- Pending: `pin_sidecars.py` and `build.bat` not run (task rule); the viewer pins will change when the viewer is rebuilt.
