# T1 research: PgDn circuit (trackside) camera and how to enter it for a target car (2026-10-14)

Scope: read-only static analysis. The game `F1 Challenge Online.exe` was running; it was NOT attached, poked, or read live.
Sources read: the on-disk EXE bytes (`F1 Challenge V10\F1 Challenge Online.exe`, PE section walk, file offset = VA - 0x400000 for .text), functions\*.c decompiles, the track file `SeasonData\Circuits\Australia\2001_Albert_Park\2001_Albert_Park.cam` (plain text), director_state.inc, and the prior records in section 7.
Not done: live tests, writes, `api1.txt`, keys. Workspace cold start: SDK front door and findings queried; governance files and `check-workspace-structure.ps1` were NOT run in this task (limitation).

Tier legend: FACT = read from bytes, decompile or file text; DERIVED = inference from FACT; OPEN = not settled by static evidence.

## 0. Answers in short

1. PgDn is DIK 209 (0xD1, DIK_NEXT) in `KeyboardInputActionMapper` 0x40BE70. Its case 209 calls `CameraLockCheck(5)` then `GameActionStateMachine(5,1)`. State 5 = circuit (trackside) group. It does NOT write the director mode.
2. State 5 makes the TrackingCam group (manager+0x20) the active view (unless StaticCam is already active), activates it, and sets state 5. The trackside camera shown is then chosen per frame from the followed car pointer at 0x7C54E0 (car+0x744 position) and proximity to each entry's ActivationLocation/Radius. No per-car index is needed.
3. Target car: the existing `GameStateDispatchChain` 0x40D240 (roster id = veh+0x4894) sets 0x7C54E0 to the car. Then, in the same game-thread task, replay case 209 exactly: `CameraLockCheck(5)` then `GameActionStateMachine(5,1)`. Both are thiscall with ECX = manager 0x5EA510.
4. Open items before the owner test: the meaning of car+0x744 and of the byte 0x5EA534, and the nearest-entry helper 0x40B630 (section 6).

## 1. PgDn handler (FACT unless marked)

- Input: the in-race keyboard action queue. `word_7CB21C[]` holds int16 DIK codes, `dword_7CB218` = pending count. `KeyboardInputActionMapper` (0x40BE70, thiscall, ECX = manager 0x5EA510) drains it. It runs only when `*(this+36) == 0` (manager+0x24, the byte read by the getter at 0x40D080).
- Caller: `MainCameraDirectorUpdate` 0x40CA30 calls the mapper each frame when `this+46 == 0` (else `RandomStateMachineWithTimerAndTrigger`).
- Case table in the mapper (decompile functions\KeyboardInputActionMapper_0040be70.c):

| DIK | Key | Action |
|---|---|---|
| 74 / 78 | numpad - / + | `CameraTargetRaceLeader` 0x40D290 (standings neighbour, per record camjump-numpadplus) |
| 156 | numpad Enter | `CameraTargetCurrentTrackedVehicle` 0x40D340 = `GameStateDispatchChain(roster of player)` |
| 199 | Home | `CameraLockCheck(4)` then `GameActionStateMachine(4,1)` (ONBOARD group state) |
| 201 | PgUp | `GameActionStateMachine(3,1)` under conditions |
| **209** | **PgDn** | **`CameraLockCheck(5)` then `GameActionStateMachine(5,1)` (circuit group)** |
| 210 | Insert | `GameActionStateMachine(0,1)` under conditions |

- VK_NEXT 0x22 to DIK 0xD1: the standard DirectInput value (DERIVED from the usual dinput table). The VK-to-DIK translator in the input driver was not located (OPEN); it is not needed for the proposal.

Call path, PgDn (FACT):

```
MainCameraDirectorUpdate 0x40CA30 (ECX=manager)
  -> KeyboardInputActionMapper 0x40BE70 (ECX=manager), queue word_7CB21C = 209
     -> CameraLockCheck 0x40C720 (ECX=manager, push 5)        returns 1 for a2=5 (only a2=3 can return 0)
     -> GameActionStateMachine 0x40CDE0 (ECX=manager, push 1, push 5)
          jump table 0x40D050 [5] -> 0x40CF30 (case 5)
```

Case 5 (FACT, disassembly 0x40CF30..0x40CF7D):
- `ecx = this[8]` (manager+0x20 = TrackingCam group). If its count (+0x10) > 0, or the StaticCam group (this[7], manager+0x1C) count > 0, take the block; otherwise skip the view change.
- `this[2]` (manager+0x08, active view) = this[8] (TrackingCam), unless `this[7] != 0 && this[2] == this[7]` (StaticCam already active, keep it). The `sub_40D080` test in the decompile is a constant address (`mov edx,0x40d080; test edx,edx`), so its branch is always taken. This resolves record O4.
- `call [active_view vtable + 0x10]` (activate). For TrackingCam this is 0x40BBC0 (table from the investigation record, 4.4).
- `AudioSoundEffectController(0)` (0x4E0ED0 on 0x7C54E0+0x6F0), then `this[3]` (manager+0x0C) = 5. Returns 0.
- Case 5 never calls `ViewCameraModeDispatch` 0x409440 (director modes 0-4 use it), so director +0x34 stays put (matches the live PROVEN tvd0 result).

State numbers (FACT from the state machine switch): 0, 1, 2 = director view calls through ViewCameraModeDispatch (0..2); 3 = SWINGMAN (remapped in 0x409440); 4 = ONBOARD index; 5 = circuit group; 7-10 = director modes 5-8 (not circuit, record 4.6).

## 2. How the circuit camera is chosen (FACT unless marked)

Groups (manager struct at 0x5EA510; FACT from record 4.3-4.4, re-checked on disk):
- StaticCam group: manager+0x1C = 0x5EA52C, vtable 0x59E678, entry stride 160 (slot 6 0x40B380 uses `lea eax,[edx+edx*4]; shl eax,5`), section `[StaticCam]`.
- TrackingCam group: manager+0x20 = 0x5EA530, vtable 0x59E6AC, entry stride 208 (0xD0), section `[TrackingCam]`. Loader MainView 0x40B850 calls the same entry parser sub_4585E0 (keys include ActivationLocation, ActivationRadius, TrackingRate, MovementRate).
- Object layout: entries pointer +0x0C, count +0x10, selected index +0x14.

Vtable slots actually read in the EXE (bytes, not decompile):
- TrackingCam slot 6 (+0x18) = 0x40BBE0: `mov edx,[ecx+0x14]; xor eax,eax; test edx,edx; jl ret0; cmp edx,[ecx+0x10]; jge ret0; mov eax,[ecx+0x0C]; imul edx,edx,0xD0; add eax,edx`. It returns `&entries[index]` (0 if out of range). This is the "current entry" getter; the index is +0x14. This supports the record's O1 candidate.
- TrackingCam slot 7 (+0x1C) = 0x40BC00: loops over the entries, compares the entry name pointer (entry+4) with a string argument via `sub_588410` (string compare), and returns the matching entry. So slot 7 is a NAME lookup, not a car lookup. StaticCam slot 7 (0x40B3A0) is the same.
- StaticCam slot 6 = 0x40B380 (same pattern, stride 160).

Per-frame update, TrackingCam (vtable +0x0C = 0x40BAA0), `this` = group. The proximity selection (FACT for the reads, DERIVED for the meaning):
```
idx   = group[+0x14];  entry = entries + idx*208
car   = *(0x7C54E0)                      ; followed car pointer (global)
pos   = car + 0x744 (three floats)       ; followed-car position block
if byte[0x5EA534] == 0:                  ; flag meaning OPEN
    if entry != 0 and (0x4579C0(...) vs entry[+0x8C] test says still inside): keep
    else: best = 0x40B630(group, pos, ...) (nearest entry, min of distance minus entry[+0x8C]);
          if best != idx: group[+0x14] = best; call vtable[+0x10]  (activate)
then 0x40B710 (camera transform, not decoded)
```
- entry+0x8C is the ActivationRadius field (DERIVED: the .cam keys in section 3, and the nearest-entry loop 0x40B630 reads `entry+140` = 0x8C as the radius).
- 0x4579C0 is named "EmptyStub" by the decompiler; its real role is OPEN (it returns a float used in the inside test).
- So the followed car for a trackside view is the GLOBAL 0x7C54E0, not a group field. Record O2 ("the followed car is not found") is resolved to this: the car is 0x7C54E0, and both the TrackingCam update (0x40BAA0) and the StaticCam update (0x40B260) read `[0x7c54e0]` (FACT bytes).

Track data (FACT, file text): `SeasonData\Circuits\Australia\2001_Albert_Park\2001_Albert_Park.cam` has 15 `TrackingCam=` and 6 `StaticCam=` sections. Example:
```
TrackingCam=TRACKING006
{ Position=(-1137.12, -1.91, 469.93) ... Fov=(13,45) ... LODMultiplier=(30) ...
  ActivationLocation=(-1064.575439, -1.087027, 424.426178)
  ActivationRadius=(159.000000)  TrackingRate=(0.000000)  MovementRate=(0.000000) }
```
STATIC001 has ActivationLocation = 3.4e38 (never proximity-activated; DERIVED: manual cycle only). The meaning of the manual path is OPEN.

Name lookup (FACT code, meaning DERIVED): `CameraTargetNameResolver` 0x40DB00 maps a target string to a car or a camera: "Player" (manager), "Original", "Last", "AnyTrackside" (returns 0), "None" (calls `TracksideCameraTargetLookup` 0x40C890, which calls slot 7 of director, StaticCam and TrackingCam with the name). Its caller is not established (OPEN). It is not needed for the proposal.

## 3. Mode / state summary for our use

- To show trackside cams for car X: make X the followed car (0x7C54E0 = X), then set state 5.
- Restore to the director: `GameActionStateMachine(saved_state, 1)` with the matching `CameraLockCheck`, as the key table does. The saved state must be read BEFORE the change (R9).

## 4. Proposed implementation (not written; for owner approval)

Thread: game thread only, inside a broker game-thread task (same pattern as `dcam_chain_begin` and `dcam_cmd_task` in director_state.inc). Never from the worker or HTTP thread. Never inside the mapper or the camera update.

Gates (all must pass; otherwise refuse with a reason and write nothing):
- `[Director] Enabled=1` and `[Director] CarSwitch=1` (existing), plus a new switch `[Director] CircuitCam=1` (test build only, default 0).
- Race session live; roster target inside the vehicle pool (`dcam_in_pool`).
- Manager resolved (`DCAM_RESOLVED`); manager+0x24 byte == 0 (input unlocked; FACT, the mapper checks it).
- Byte gates, read with copy_game from the running game:
  - 0x40D240 first 40 bytes (existing gate).
  - 0x40CDE0 first 22 bytes: `53 55 8B 6C 24 0C 56 57 33 FF 32 DB 83 FD 0A 8B F1 B8 01 00 00 00 0F 87`.
  - 0x40C720 first 22 bytes: `8B 15 E4 54 7C 00 8B 14 95 F0 54 7C 00 56 8B 35 DC 54 7C 00 8B 34 B5 F0`.
  - 0x40BBE0 first 10 bytes: `8B 51 14 33 C0 85 D2 7C 12 3B`.
- TrackingCam count (manager+0x20, then +0x10) > 0 and entries pointer non-null (no state 5 with an empty group; R2).
- Byte [0x5EA534] == 0, or log its value and refuse while it is nonzero (meaning OPEN, see R3).

Calls, in this order, in one task (game thread):
1. Save the current state: `saved = manager[+0x0C]`, `saved_view = manager[+0x08]` (for restore).
2. Car: `GameStateDispatchChain` at 0x40D240: ECX = manager (0x5EA510), push roster (veh+0x4894). This is the existing `dcam_chain_call_once`. It sets 0x7C54E0 = X, 0x7C54DC, audio, and runs CameraViewAssignment (FACT chain, record camera-retarget-chain).
3. `CameraLockCheck` at 0x40C720: ECX = manager, push 5. Returns 1. Side effect: if the rosters differ it sets manager+0x14 = -1, so keep `saved` from step 1.
4. `GameActionStateMachine` at 0x40CDE0: ECX = manager, push 1 (a3, char), push 5 (a2). Returns 0 on success. Sets the active view to TrackingCam, activates it, state 5.
Order matters: step 2 can itself write a director state (CameraViewAssignment and ConditionalModeDispatchWithGlobalGate can use states 0-2), so step 4 must come AFTER step 2 in the same task.

Verify (watch, one step per game frame, like `dcam_watch_step`; timeout 10 frames; no retry):
- `manager[+0x08] == 0x5EA530` (or 0x5EA52C if the game chose StaticCam), `manager[+0x0C] == 5`, and `*(0x7C54E0) == X`.
- `TrackingCam[+0x14]` in [0, count) and changing as X moves. This is the proof that the trackside cam follows.
- Report `view_owner` (existing classifier) = circuit_tracking. Angle stays null.
- On timeout, refuse with a reason. Log `saved`, `manager_state`, `0x7C54DC`, `0x7C54E0`, `0x5EA534`, and `+0x14`.

Fallback (not recommended): inject DIK 209 into `word_7CB21C` / `dword_7CB218` (queue route, GAME_CONTROL.md section 4). It writes game memory, the count is not exact, and it does not choose a car, so step 2 is needed anyway.

## 5. Risks (tier per claim)

| ID | Risk | Tier |
|---|---|---|
| R1 | Target car outside all TrackingCam radii: the nearest entry is still chosen, so the view can be far from the car. | DERIVED (0x40B630 nearest search) |
| R2 | Count 0 with state 5: no view change, state 5 with the director still on screen. Guard count > 0. | FACT (case 5 code) |
| R3 | Byte 0x5EA534 gates the TrackingCam update; its meaning is unknown. | FACT read location, OPEN meaning |
| R4 | Step 2 (GameStateDispatchChain) can change state 3 or 1/2 via CameraViewAssignment; order matters (step 4 last). | FACT code, DERIVED order |
| R5 | `CameraLockCheck(5)` clears manager+0x14 when rosters differ; save the state first. | FACT (0x40C720 writes +0x14 = -1) |
| R6 | Step 2 side effects: AIOutOfTrackEvent (event 264 insert for AI cars), SetPlayerVehicleIndex, commentary "riding with" (suppressed by MuteCarChangeCommentary today; mechanism OPEN). | FACT code, OPEN mechanism |
| R7 | Race finish, pit timer and others call state 5 on their own (record 4.5), so the circuit view can change without our call. The watch must not treat that as our failure. | FACT |
| R8 | Car+0x744 is read as position by both group updates. Its exact meaning (vs director car+88) is OPEN; a wrong field gives wrong framing, not a crash. | DERIVED / OPEN |
| R9 | Restore needs the saved state read BEFORE steps 2 and 3. | FACT |
| R10 | Byte patterns come from the on-disk EXE; the running build must be byte-checked (existing check method). | FACT (method); needs live check |

## 6. Open items (O-numbers from the record 4.x series)

- O1 RESOLVED (FACT by bytes): circuit index = group+0x14, read by slot 6 0x40BBE0; written by the TrackingCam update 0x40BAA0.
- O2 RESOLVED (FACT by bytes): followed car = global 0x7C54E0, position block +0x744 (meaning DERIVED, not proven).
- O3 PARTLY RESOLVED: selection is by proximity (ActivationLocation + Radius) with a nearest fallback (0x40B630). The exact radius test is OPEN (0x4579C0 meaning).
- O4 RESOLVED (FACT bytes): the `sub_40D080` branch is `mov edx,0x40d080; test edx,edx`, always taken.
- O5 RESOLVED: yes, the circuit view follows 0x7C54E0, the same car as the director's followed car.
- OPEN: 0x5EA534 flag; 0x40B630 distance formula; 0x4579C0 result; 0x40BBC0 (activate) body; car+0x744 vs car+88 meaning; the caller of `CameraTargetNameResolver` 0x40DB00; the VK-to-DIK path.

## 7. Prior art read

- TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION_2026-10-13.md sections 4.1-4.9 (PgDn route, state 5, groups, vtables, O1-O8).
- TV_DIRECTOR_KEYPATH_RECORD_2026-10-14.md (GameStateDispatchChain call, byte gate, watch, diagnostics).
- TV_DIRECTOR_PHASE0_RECORD_2026-10-13.md and TV_DIRECTOR_PHASE2_RESEARCH_2026-10-14.md section B (circuit cut stays out of phase 2 until O1/O2 are proven; this task proves O1/O2 statically, not live).
- findings.jsonl: tvd-circuitcam-pgdn-route-20261013, tvd0-circuit-cams-not-in-director-mode-20261013, tvd-circuitcam-groups-5EA52C-5EA530-20261013, camjump-direct-call-gamestatedispatchchain-20261014, camjump-direct-call-live-proven-20261014, camera-retarget-chain-six-steps-2026-10-13, tvd0-roster-field-4894-chain-arg-20261013.
- tools/GAME_CONTROL.md section 4 (queue injection, numpad keys).

## 8. Pending validation (owner live test; the static check ran with the game running, so nothing was tested live; do not run now)

1. Byte gates above match the running game (read-only check at the next launch).
2. Owner: TVCOCKPIT, PgDn once: `view_owner` = circuit_tracking, `+0x14` shows a valid index, `0x5EA534` value logged.
3. Build test (CircuitCam=1, test build only): one AI car; expect state 5, `0x7C54E0` = car, TrackingCam `+0x14` changing as the car moves; race data unchanged for 60 s.
4. Restore: back to the saved director state with no car change.
5. Commentary check (R6).

## 9. Changed files

This task changed no code. Written: this record. Appended: findings.jsonl (t1-circuitcam-* lines).
