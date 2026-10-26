# TV Director camera change record, 2026-10-14

Status: implemented and built offline. NOT live-tested. The game was running during this work, so no runtime test, no TELEMETRY_TESTS gate, no fuzz and no soak were run. The file name keeps the owner's requested `KEYPATH` label; the mechanism that finally shipped in this change is a single game call (see section 3).

## 1. Why

- Owner live test 2026-10-14 (reported in chat): TV Director angle changes work. Every Cut to another car was refused by the manager-state guard (manager state was 2 in normal driving). The guard was corrected to the exact game rule (CameraModeChangeRequest 0x40C5B0: state -1 or 1 always, state 2 only while dword 0x7CFCC8 == 0). That correction is now diagnostic only (section 4).
- Owner live test 2026-10-14 (reported in chat): a bare write of 0x7C54DC does NOT move the camera. Numpad Enter does.
- Prior art: `tools/GAME_CONTROL.md` section 4, findings `keyboard-action-queue-background-input` and `numpadenter-returns-camera-to-player-car` (both FACT, live 2026-07-27). Those findings say a non-zero direct write of 0x7C54DC does work. The owner's 2026-10-14 test says it does not. The state of the game in July (manager state, mode) is not recorded, so the two observations are in conflict and are kept as such.

## 2. Design history (superseded, not shipped)

Four designs were tried in turn. Each one was replaced on evidence.

| Step | Design | Why it was replaced |
|---|---|---|
| a | Write the table index i into 0x7C54DC, no key | Owner live test: the camera does not move (2026-10-14). |
| b | Write i-1 into 0x7C54DC, then inject DIK 78 (numpad +) | Decomp contradicts the premise: case 78 of KeyboardInputActionMapper 0x40BE70 calls CameraTargetRaceLeader(0) (0x40D290), which moves to the standings NEIGHBOUR (GetTeamIDByFinishPosition, +-1), not to index+1. The cyclic shift 0x40D2D0 is reached only from case 101 of another command table (research A7, key still OPEN). The decomp file named 0x40D2CA is misaligned (research A4 and A7: 0x40D2CA is padding). |
| c | Arm a standings neighbour of the target (index), press 78 or 74, one retry the other way | Owner live test of a direct write failed; the neighbour method was still a key-queue trick that the owner found unreliable from outside. Direction left as one constant, OPEN. |
| d | FINAL: one call of GameStateDispatchChain 0x40D240 on the game thread (section 3) | Owner decision. Every camera-target path ends in this call. |

Key-queue writes (0x7CB21C / 0x7CB218) are no longer used anywhere in the mod. The 0x42BBD0 setter is not called directly. (It is called inside the chain, see section 3.)

## 3. Final design

**The call.** On the game thread, one call: `GameStateDispatchChain` at 0x40D240, thiscall, ECX = camera manager (`DcamAddrs.manager`), one stack argument = roster id of the target car (callee pops it).
- Numpad Enter (0x40D340) calls it with the roster of the player car: `mov eax,[0x7C54E4]; mov edx,[0x7C54F0+eax*4]; mov eax,[edx+0x4894]; push eax; call 0x40D240` (owner's byte reading; consistent with `functions/CameraTargetCurrentTrackedVehicle_0040d340.c`).
- Roster id is `vehicle + 0x4894` (`DCAM_ROSTER_ID`).

**Byte gate.** The 40 bytes at 0x40D240 must equal the reviewed prologue `56 57 8B 7C 24 0C 8B F1 57 B9 D0 C6 5E 00 E8 7D E9 01 00 8B CE E8 46 F5 FF FF 57 8B CE E8 EE F3 FF FF 8B CE E8 27 FE FF`. Checked against `F1 Challenge Online.exe` at file offset 0xD240 (match). The same bytes are checked in the running game before every call.

**Gates, in order** (`dcam_chain_gate`): [Director] CarSwitch=1; engine resolved; live race session (capture); target live (pool slot in the pool and roster known); camera manager pointer plausible; prologue bytes exact. [Director] Enabled=1 is checked first in `dcam_exec`. Refusal text names the failed gate; nothing is called when a gate fails.

**Target.**
- Cut by driver name: `dcam_find_driver` (unchanged).
- Cut by race position (new): `{"position": N}` with N in 1..104. The game thread reads, for every pool slot, the live race position at vehicle +0x4060 and the active flag at +0x32C8, and picks the single live car at N (`dcam_pick_position`). Zero or more than one car at N is refused. The field is FACT: `sub_40C540` (GetTeamIDByFinishPosition) compares +16480 (= 0x4060) with the finish position, and the telemetry driver list uses `OFF_POSITION` 0x4060.
- Restore: the player car read as numpad Enter reads it: `dword_7C54F0[dword_7C54E4]` (`dcam_player_roster`). Restore then calls the chain once, then restores the saved angle.

**Verify and angle.** After the call a watch runs on the game thread, one step per game frame (the worker posts `dcam_watch_task` when the session time advances; at most one outstanding; wall-clock guard 5 s). Each step reads 0x7C54E0. When it equals the target vehicle: the angle is applied with `dcam_set_angle` and the result is applied (or partial), with `verified_next_frame` armed as before. After 10 frames without the target, the result is refused with the reason "the game did not move the view to that car (after N frames; 0x7C54DC=..., 0x7C54E0=..., manager state ..., 0x7CFCC8=...); nothing else was written". There is no retry.

**Diagnostics** (reads only, never gates): `GET /director/state` adds `view_index_0x7C54DC`, `view_target_0x7C54E0`, `circuit_flag_0x7CFCC8`; `manager_state` was already there. The manager-state rule (`dcam_manager_state_ok`) is kept as a function and is not called by the car path.

**HTTP.** `POST /director/cut` takes `{"position": N, "angle": "..."}` (400 if N is not a whole number 1..104) or `{"driver": "...", "angle": "..."}` as before. Restore and angle bodies are unchanged.

**Viewer (TV tab).** A "Go to position" number box (1..104) with a Go button. Enter in the box submits. The box sends a cut to the driver shown at that position in the live list, with the currently selected angle (`tvdBody` with a position). Hotkeys (1-4, arrows, Enter) do not fire while focus is in an input, textarea, select or editable element (`tvdHotkeyAllowed`). Nothing is sent for a value that is not a whole number 1..104 or when no live row has that position.

**Side effects, stated plainly.** The chain runs SetPlayerVehicleIndex(a2) (the 0x42BBD0 setter), CameraTargetUpdateOnPlayerChange, CameraViewAssignment, VirtualDispatchConditionalAudio, AIOutOfTrackEvent(a2) and sub_4D8760(1,0) (decomp `functions/GameStateDispatchChain_0040d240.c`). Numpad + / - / Enter run the same chain on every press, so a cut here has the native side effects that a human numpad press has. The owner accepted this.

## 4. Files

All edits are in `f1c_ida_decompiled\mods\sdk_sandbox\web_telemetry` unless noted. Every edited file has a `.BEFORE_20261014_tvdkey` backup taken before its first edit in this task.

| File | Change | SHA-256 after edit | Backup |
|---|---|---|---|
| director_state.inc | car change block replaced (chain gate, position pick, standings read, player roster, begin, watch step, watch task, post, exec); phase-2 0x42BBD0 path and key-queue code removed | EDB2826E8AC91258448BA354A2D46AE9AB4398D1CB83C70CAF6BA324C7FD61AD | director_state.inc.BEFORE_20261014_tvdkey |
| compatibility_runtime.inc (LF file) | worker loop: `Sleep(dcam_watch_post_due() ? 2 : 50)` | C3F2795D2869AE96711B1C24FE28C75BB9E0A5E58B148D92F85595B817E8B069 | compatibility_runtime.inc.BEFORE_20261014_tvdkey |
| web_telemetry.c | POST /director/cut parses position (1..104) or driver | EF5D306D48854581E50E0D3C3D1A36080589862531017417BEDC59FD58A87BE3 | web_telemetry.c.BEFORE_20261014_tvdkey |
| runtime_test.c | test_director_phase2 rewritten for the final design | 174F8E104F78F12A6249EC0F458BDCD7C3B4FF9153237BC806A026913F073D3E | runtime_test.c.BEFORE_20261014_tvdkey |
| telemetry_viewer.js (LF) | position box, tvGoPosition, tvdPositionValue, tvdHotkeyAllowed, tvdBody position | EFD00D7002583C709F2083C04893957FE8375088111CE75CDF368526D3D8007D | telemetry_viewer.js.BEFORE_20261014_tvdkey |
| viewer_test.js (LF) | 8 new offline checks | 44D202DDDF8BC19F636EA912640AA9BF5CC52508489C10033183006AED2821F0 | viewer_test.js.BEFORE_20261014_tvdkey |
| VERIFICATION_MANIFEST.json, f1c_mod.json, mod.json, web_telemetry.dll.mod.json | viewer sidecar pin only (pin_sidecars.py) | 1EE2D1BA..., 2420ED12..., 79CCF808..., CEAD80C6... | each `.BEFORE_20261014_tvdkey` |
| CURRENT_STATE.md | index line | (see the file) | CURRENT_STATE.md.BEFORE_20261014_tvdkey |
| f1c_ida_decompiled\findings.jsonl | `tvdkey-*` entries appended | (append only) | findings.jsonl.BEFORE_20261014_tvdkey (root) |

Payload of the plain build (`build.bat`, output in `%TEMP%\f1c-web-telemetry-4.4.0`, not in the workspace):
- web_telemetry.dll 46385C91C6756F3898F396F957461680ACB834F0B99A12211CFB888DC16AEADD
- telemetry_viewer.js EFD00D7002583C709F2083C04893957FE8375088111CE75CDF368526D3D8007D
- workbench_ui.js E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8 (unchanged)
- web_telemetry.ini 0C968FCCC96C78AAFDDA71FCE8E9484DC03DC42D57E2E121506961D2ADB8BB05 (unchanged)
- web_telemetry.dll.mod.json CEAD80C68DBE793626204DA121A13CBE765F9D8AE1D69E8982249402863561A1

Restoration: copy each backup over its file (for example `copy /y director_state.inc.BEFORE_20261014_tvdkey director_state.inc`), then rerun `pin_sidecars.py --check` and `build.bat`. The earlier DLL is not kept as a file; a rebuild from the backed-up sources reproduces the previous state.

## 5. What was run

- `gcc -fsyntax-only -std=c11 -Wall -Wextra -Werror -Wno-unused-function` on runtime_test.c (build.bat flags): exit 0. runtime_test.c includes web_telemetry.c, so this covers director_state.inc and compatibility_runtime.inc.
- Plain `build.bat` (no TELEMETRY_TESTS): exit 0, payload gate OK, sidecar pins OK.
- `node viewer_test.js telemetry_viewer.js workbench_ui.js`: VIEWER PASS 533 checks (was 525).
- `python pin_sidecars.py` then `--check`: pins OK.
- Not run (game running): runtime_test.exe, TELEMETRY_TESTS gate, test_telemetry.py, fuzz, soak.

Offline checks in runtime_test.c, test_director_phase2 (run later, see section 6): chain prologue (exact accepted, each of the 40 one-bit changes refused), player table pick, race position pick (unique; none; two live cars; dead slot ignored; 0 and 105 refused as out of range), body position value (whole numbers; negative, text and absent refused; large capped), the six gate reasons (distinct), the watch decision (continue to 10, give up at 10, reached wins), give-up reason text, plan order, status combination, next-frame verify, result JSON, default gate.

## 6. Commands to run later (only with the game closed)

```
rem in a Windows shell, game closed
set TELEMETRY_TESTS=1
"%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\mods\sdk_sandbox\web_telemetry\build.bat"
rem the TELEMETRY_TESTS block builds %TEMP%\f1c-web-telemetry-4.4.0\runtime_test.exe and runs test_telemetry.py
%TEMP%\f1c-web-telemetry-4.4.0\runtime_test.exe
```
Expect `PASS TV Director camera change offline checks (prologue, player pick, position pick, JSON value, gate, watch, plan, status, verify, JSON)`.

## 7. Live checks pending (owner, game running, [Director] Enabled=1 and CarSwitch=1 in the test build)

1. Cut to a named AI driver. Expect 0x7C54E0 to become that car within a few frames; expect applied or partial with verified_next_frame true.
2. Cut by position N (for example 3). Expect the same as 1 for the car at position 3; out-of-range and "no car" refusals as written.
3. Cut to the player car, then Restore. Expect the chain to set 0x7C54E0 to the player car and the saved angle to read back.
4. Refusal path: force a car that cannot be reached (for example during a replay) and check the refusal reason and that nothing else changed.
5. Log lines `director: camera change frame N: 0x7C54DC=... 0x7C54E0=...` show the per-frame steps. Check that a step happens once per game frame and not in bursts.
6. Side effects: check that no AI car is set out of track and no audio or session event is raised by the chain (see section 3).
7. Viewer: the Go to position box, Enter in the box, hotkeys not firing while the box has focus, and the selected angle kept.

## 8. Open items and risks

- OPEN: whether the chain's side effects are harmless in every session state. The numpad keys run the same chain (owner statement, DERIVED), but a programmatic call at a time of our choosing is not proven equal to a key press.
- OPEN: ECX is the camera manager (owner statement, DERIVED from the mapper and CameraModeChangeRequest frame order). Verify live, or by reading the mapper's `this` in MainCameraDirectorUpdate.
- OPEN: roster id of the player car is read the way numpad Enter reads it. The old roster > 0 check was removed (roster equals the pool slot, research A13, so the player can be 0).
- OPEN: race position ties are refused. Positions are read once, at the command, and the car is not re-checked against the position between that read and the chain call (the call happens in the same broker task, so the gap is one task).
- OPEN: the July finding (non-zero direct write works) and the 2026-10-14 owner test (it does not) conflict. The new design does not depend on the direct write, so the conflict does not block this change; it stays recorded.
- Not a regression but noted: `workspace` validator reports two pre-existing errors (unexpected top-level directories `.claude` and `plans`); they are not from this change.

## 9. Wasted-effort check

- Repeated reads of the decomp and research files during design changes (about four redesigns in one session).
- One avoidable failure: a heredoc quoting error and a backslash path in the validator call.
- Two gcc runs that repeated the same syntax check after small edits.

## 10. Documentation gate

- Record: this file. Index: CURRENT_STATE.md first entry. Findings: `tvdkey-*` lines in f1c_ida_decompiled\findings.jsonl (DERIVED until live-tested).
- Fact versus inference is marked per item above and in the findings. Pending validation is section 7.
