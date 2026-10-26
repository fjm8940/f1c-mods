# TV Director phase 2 research (2026-10-14)

Scope: research and phase 2 specification only. No code, build, test program, game run, or game-folder write was done. The only file created is this record. `findings.jsonl` received appended lines only. No other file under `web_telemetry` was edited. `CURRENT_STATE.md` was not edited (follow-up, section 9).
Inputs: `TV_DIRECTOR_PHASE1_RECORD_2026-10-13.md` (sections 3, 4, 5, 9), `TV_DIRECTOR_SPEC_2026-10-13.md` (sections 4.4, 4.5, 7, 13, 14), `director_state.inc`, `TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION_2026-10-13.md`, `TV_DIRECTOR_BATTLE_SIGNAL_2026-10-13.md`.
Product: Telemetry (`web_telemetry`) 1.2.0 as recorded in phase 1. Pipeline stage: investigate (research) and specify. Not implemented.

Tags: FACT = read in source, in decompile text, in raw bytes of the game EXE (cited with file offset), or in a recorded finding. DERIVED = inferred from FACTs, not proven live. OPEN = unknown, needs a test or an owner decision.

Binary used: the canonical EXE `F1 Challenge V10\F1 Challenge Online.exe` (SHA-256 starts `1A70E5ED`, the build the SDK and the camera signatures use). It was opened read-only with Python `rb` for byte reads and PE section parsing. The research copy `research\game_versions\f1_challenge_online_version_08082026\F1 Challenge Online.exe` (`19A338BD`) was also read: the bytes of every function cited below are identical in both. File offset = VA - 0x400000 for `.text` and `.rdata` (both have RVA = raw offset in this EXE).

Two SDK corrections found while doing this (FACT, recorded in findings):
- `VehicleSelectionCyclicShift` does not start at 0x40D2CA. The bytes at 0x40D2CA are padding and a call tail. The real entry is 0x40D2D0 (`8A 54 24 04 84 D2 A1 DC 54 7C 00 74`). The SDK's `__userpurge(edx)` signature does not match: the byte argument is read from `[esp+4]`.
- The SDK caller list for `GameStateDispatchChain` and `CyclicShift` is incomplete (`callers: []` for 0x40D2CA). See item A6.

---

## A. Car switch ("Cut to driver X")

### A1. What SetPlayerVehicleIndex 0x42BBD0 writes (FACT)

- Signature: `int __stdcall sub_42BBD0(int a1)`. Source: `functions/SetPlayerVehicleIndex_0042bbd0.c:1-15`.
- It writes the VIEWED car, not the player's car:
  - `dword_7C54E0 = &unk_5EC6D8 + 18624*a1` (viewed/followed car pointer; 18624 = 0x48C0 = vehicle stride).
  - Searches the pool table `dword_7C54F0[0..103]` for that pointer. If found, `dword_7C54DC = index`. If not found, returns 104 and 0x7C54DC is left alone.
- Canonical bytes (file 0x2BBD0): `8B 4C 24 04 69 C9 C0 48 00 00 81 C1 D8 C6 5E 00 | 89 0D E0 54 7C 00` (the last store is `mov [0x7C54E0], ecx`).
- Its twin 0x42BC40 (`functions/SetPlayerVehicleIndex_bc40_0042bc40.c`) has the same 16 leading bytes and writes the PLAYER car: `89 0D E8 54 7C 00` = `mov [0x7C54E8], ecx`, and then `dword_7C54E4` (player index). The twin is therefore told apart only by its last store. A prologue check must include the store bytes (see A9).
- `BuildActiveVehiclePointerArray` 0x42BC80 (`functions/BuildActiveVehiclePointerArray_0042bc80.c:54-61`) recomputes `dword_7C54DC` from `dword_7C54E0` and `dword_7C54E4` from `dword_7C54E8`. It reads the pointers; it does not choose the car.
- Live evidence already recorded: `tvd0-live-t1-partial-20261013` (PROVEN, phase 0): the followed car follows a native car switch (roster 0 to roster 14).

Summary: 0x7C54E0 + 0x7C54DC = the car the camera follows (viewed). 0x7C54E8 + 0x7C54E4 = the player's car. The name "SetPlayerVehicleIndex" is misleading.

### A2. The game's chain 0x40D240 (FACT, bytes and call sites)

Decompile: `functions/GameStateDispatchChain_0040d240.c:1-9`. Canonical bytes, with each call decoded from the bytes (thiscall, ECX = manager 0x5EA510 for the caller that uses it):

| Step | Call site | Target | Convention in bytes | Effect (FACT from the target's decompile) |
|---|---|---|---|---|
| 0 | 0x40D240 prologue | `56 57 8B 7C 24 0C 8B F1` | push esi; push edi; edi = a2 (roster id); esi = ECX (manager) | |
| 1 | 0x40D24E | 0x42BBD0 SetPlayerVehicleIndex | `push edi; call` (stdcall) | viewed pointer 0x7C54E0 and index 0x7C54DC (A1) |
| 2 | 0x40D255 | 0x40C7A0 CameraTargetUpdateOnPlayerChange | thiscall, ECX = esi | if `this[14]` (manager+0x38) differs from the viewed roster: audio calls, then `this[14] = viewed roster` |
| 3 | 0x40D25D | 0x40C650 CameraViewAssignment | thiscall, push edi | see A5 |
| 4 | 0x40D264 | 0x40D090 VirtualDispatchConditionalAudio | thiscall | calls `this[2]` (the active view) vtable+0x10; clears audio fields at dword_8AFE88 +420/+424 |
| 5 | 0x40D26F | 0x41A0C0 AIOutOfTrackEvent | stdcall, push edi | see A4 |
| 6 | 0x40D27D | 0x4D8760 (sub_4D8760) | `push 0; push 1; mov ecx, 0x008AD298; call` | writes the object at 0x008AD298, NOT the manager (A3) |

FACT: step 6 uses `ECX = 0x008AD298` (bytes `B9 98 D2 8A 00` at 0x40D278, `E8` at 0x40D27D). The phase 1 spec (section 4.4, step 6) says the gated setter writes "roster id"; that is wrong. See A3.

### A3. sub_4D8760 with ECX = 0x008AD298 (FACT, with one correction)

- Decompile: `functions/sub_4D8760_004d8760.c:1-39`. `int __thiscall sub_4D8760(this, a2, a3)`.
- Called from the chain as `sub_4D8760(1, 0)`: `this = 0x008AD298`, `a2 = 1`, `a3 = 0`. The object 0x008AD298 is also used by `sub_4DA840` (`sub_4D8760(&dword_8AD298, 17, 0)`, `functions/sub_4DA840_004da840.c`).
- Gate (all must pass): `this[a2+4] != 0` (that is `0x008AD2AC`), and `(dword_7CF638 || Return0Stub_9280(a2))`, and (`*this == -1` or `IDToValueMapper(*this) < IDToValueMapper(a2)` or `*this == a2`), and `!ConditionalFlagReturn(&dword_5EA43C)`.
- Effect when the gate passes: `this[55] = 0.5f` (a3 = 0 gives 1056964608 = 0x3F000000), `*this = a2` (= the LITERAL 1, not a roster id), then `VTableDispatchWithFieldClear(this[a2+4])`.
- Correction to spec 4.4 step 6: the chain never writes a roster id here. It writes 1 into 0x008AD298 and runs a vtable dispatch on 0x008AD2AC. What that object means is OPEN.

### A4. AIOutOfTrackEvent 0x41A0C0 (FACT, with the live gate)

- Decompile: `functions/AIOutOfTrackEvent_0041a0c0.c:4-13`. Bytes: `A1 08 03 7D 00 | 83 EC 20 | 85 C0 | 56 | 74 ...` then `D9 05 C8 F3 7C 00` (fld flt_7CF3C8).
- Gate: `dword_7D0308 != 0 && flt_7CF3C8 != 0.0`.
- Inside the gate: `BooleanOROfTwoChecks(flt_5EA618, 264)`. If `a1 != roster of dword_7C54F0[dword_7C54E4]` (the PLAYER car), then `DriverDataTableLookupAndTimeConversion(v1, 264, dword_5F0F74[4656*a1], -1,-1,-1,-2,-2)` and `InsertDriverTableEntry(flt_5EA618, v1)`.
- Live state of the gate: `web_telemetry.c:1006` says raw 0x7D0308 "reads 3 on track (docs/38), never 1", and `:5129` makes `race_active = raw != 0`. So during a race the gate is open, and a cut to any non-player car inserts one event-264 entry into the driver table `flt_5EA618`. The same table is written by `AICornerSpeedComparison`, `AIDriverStateTransition`, `AIFormationLap` (grep of `functions/`).
- Consequence (FACT + DERIVED): the game does this on every manual switch to an AI car (numpad). The owner rule "T2: race data unchanged" (D-TV2) cannot be met for AI targets as written. What the 264 table feeds is OPEN (section A10).

### A5. CameraViewAssignment 0x40C650 (FACT)

- Decompile: `functions/CameraViewAssignment_0040c650.c:6-17`; raw bytes `A1 E4 54 7C 00 53 8B 5C 24 08 56 8B F1 ...`.
- If the target is the player (`a2 == roster of table[7C54E4]`): if `this[5] != -1`, call `GameActionStateMachine(this[5], 0)` and set `this[5] = -1`.
- Else if `this[3] <= 2`: `ConditionalModeDispatchWithGlobalGate(this)`; else `this[5] = -1`.
- If `this[3] == 3` and the target car's byte at +13388 is set: `GameActionStateMachine(2, 0)` (player) or `GameActionStateMachine(5, 0)` (non-player, i.e. CIRCUIT group selection, `TV_DIRECTOR_CIRCUIT_CAM_INVESTIGATION` 4.5), and `this[5] = 3`.
- The byte at +13388 of the followed car is also tested by `ViewCameraModeDispatch` (SWINGMAN remap) and by the key mapper case 201. Its meaning is OPEN (recorded as "camera availability" only by usage).

### A6. Who calls what (FACT, direct-call scan of the canonical EXE)

Scan: every E8/E9 rel32 in `.text` whose target equals the function, plus absolute dword references. Method: `pe_scan` (Python, read only).

| Function | Direct callers (site) |
|---|---|
| Chain 0x40D240 | 0x40D2C1 (CameraTargetRaceLeader), 0x40D306 (CyclicShift), 0x40D353 (CameraTargetCurrentTrackedVehicle), 0x4A03CC (sub_4A0250, session end path), 0x4DA90A (sub_4DA840, post-race path) |
| SetPlayerVehicleIndex 0x42BBD0 | 0x40D24E (chain), 0x40DFDB, 0x42C215, 0x42C22B (preload 0x42C1A0), 0x45FED4, 0x46028F (NetComm), 0x4B99A1, 0x4BA3C4 |
| Twin 0x42BC40 | 0x42C21D, 0x42C234, 0x45FDBC, 0x4BA3D6 |
| AIOutOfTrackEvent 0x41A0C0 | 0x40D26F (chain only) |
| sub_4D8760 0x4D8760 | 0x40D27D (chain) and 12 others |
| CyclicShift entry 0x40D2D0 | 0x46F679 only (inside wrapper 0x46F672, see A7) |
| CameraTargetCurrentTrackedVehicle 0x40D340 | 0x40BF82 (key mapper case 156) |
| CameraTargetRaceLeader 0x40D290 | 0x40BF6E, 0x40BF79 (key mapper cases 74, 78), 0x40C109 (RandomStateMachineWithTimerAndTrigger) |

Note: the SDK says `callers: []` for `VehicleSelectionCyclicShift`. The direct-call scan finds one caller, so the SDK's list is incomplete (see the finding `tvd2-cyclicshift-entry-0x40d2d0-only-caller-0x46f679-20261014`).

### A7. The game's own next-car command (FACT)

- Wrapper 0x46F672: `6A 01` (push 1), `B9 10 A5 5E 00` (mov ecx, 0x5EA510 = manager), `E8` call 0x40D2D0 (CyclicShift), `8B C6 5E C3`. So the game's "next car" calls CyclicShift with a1 = 1 (next) on the manager.
- The wrapper is case 101 of a 15-entry jump table at 0x46F794 (table entries 0x46F65D, 0x46F672, 0x46F790, 0x46F682, 0x46F69C, 0x46F6C6, 0x46F6E0, 0x46F6F0, 0x46F71A, 0x46F734, 0x46F744, 0x46F754, 0x46F764, 0x46F774, 0x46F784 for command codes 100..114).
- Dispatcher 0x46F640: `8B 89 E4 00 00 00` (mov ecx, [ecx+0xE4]: the command code is a field of its object), `lea eax,[ecx-0x64]`, range check 0x0E, `jmp [eax*4+0x46F794]`. The table base is referenced from code at 0x46F659. The dispatcher's address is referenced from a data slot at VA 0x5A001C (in `.rdata`; the owning class is not resolved).
- Case 100 toggles byte 0x7CD8BB. Cases 103 to 114 call other objects (0x8AB6EC). Only case 101 reaches CyclicShift. No direct caller passes a1 = 0 (previous car).
- So: the game's "previous car" path is not present in the static EXE (FACT: no call with push 0 to 0x40D2D0). OPEN which key sends code 101.

### A8. The game's own "return to player" and "relative" paths (FACT)

- CameraTargetCurrentTrackedVehicle 0x40D340: `GameStateDispatchChain(this, roster of dword_7C54F0[dword_7C54E4])`. The argument is the PLAYER car (7C54E4 = player index), so numpad Enter (key 156) means "back to my car". The name is misleading. Live: `numpadenter-returns-camera-to-player-car` (FACT, 2026-07-27).
- CameraTargetRaceLeader 0x40D290 (byte arg `[esp+4]`): `GameStateDispatchChain(this, GetTeamIDByFinishPosition(roster of dword_7C54F0[dword_7C54DC], 2*(a2!=0)-1))`. Numpad +/- (keys 74, 78) step through the finish order RELATIVE TO THE VIEWED CAR, not to the race leader. What the returned value is (team id vs roster id) is OPEN (`GetTeamIDByFinishPosition` not read).

### A9. Prologue bytes to verify at runtime (FACT, canonical EXE, identical in research copy)

Read from the file offsets shown (VA - 0x400000). Use these as the runtime pattern (hex, space separated, `??` = wildcard if needed):

| Function (VA) | File offset | Bytes to match (first N) | Notes |
|---|---|---|---|
| 0x42BBD0 SetPlayerVehicleIndex | 0x2BBD0 | `8B 4C 24 04 69 C9 C0 48 00 00 81 C1 D8 C6 5E 00 89 0D E0 54 7C 00` (22 bytes) | 22 bytes needed: the first 16 match the twin 0x42BC40; the store (`E0`) tells them apart. Reject if the store is `E8`. |
| 0x42BC40 twin (do NOT call for car switch) | 0x2BC40 | same, store byte `E8` | Player setter. |
| 0x40D240 GameStateDispatchChain | 0xD240 | `56 57 8B 7C 24 0C 8B F1 57 B9 D0 C6 5E 00 E8` (15 bytes) | Then the 5 call targets above (A2) can be checked by rel32. |
| 0x41A0C0 AIOutOfTrackEvent | 0x1A0C0 | `A1 08 03 7D 00 83 EC 20 85 C0 56 74` (12 bytes) | |
| 0x4D8760 sub_4D8760 | 0xD8760 | `51 A1 38 F6 7C 00 53 56 57 8B 7C 24 14` (13 bytes) | |
| 0x40D2D0 VehicleSelectionCyclicShift | 0xD2D0 | `8A 54 24 04 84 D2 A1 DC 54 7C 00 74` (12 bytes) | NOT 0x40D2CA. |
| 0x40D340 CameraTargetCurrentTrackedVehicle | 0xD340 | `A1 E4 54 7C 00 8B 14 85 F0 54 7C 00` (12 bytes) | |
| 0x40D290 CameraTargetRaceLeader | 0xD290 | `8A 54 24 04 33 C0 84 D2 0F 95 C0 56` (12 bytes) | |
| 0x40C650 CameraViewAssignment | 0xC650 | `A1 E4 54 7C 00 53 8B 5C 24 08 56 8B F1` (13 bytes) | |
| 0x40C7A0 CameraTargetUpdateOnPlayerChange | 0xC7A0 | `57 8B F9 8B 0D DC 54 7C 00 8B 14 8D` (12 bytes) | |
| 0x40D090 VirtualDispatchConditionalAudio | 0xD090 | `8B 49 08 85 C9 74 05 8B 01 FF 50 10` (12 bytes) | |

Note on the camera signature finding (`camera-code-signatures-all-builds-2026-10-04`): it lists SetPlayerVehicleIndex among the stable addresses, but does NOT list the chain 0x40D240, AIOutOfTrackEvent 0x41A0C0, or sub_4D8760. Those three must be byte-checked at runtime in the game process before any call (DERIVED: they may differ in other builds; the three above were identical in both local copies only).

### A10. Who else reads the viewed car (FACT, with side effects that matter for "race data unchanged")

Counts from `functions/`: 58 functions read 0x7C54E0; 31 read 0x7C54DC. Relevant readers:

- `PitStateUpdateMachine` 0x4A6E50 line 35: case 2, `if viewed roster == this car roster: ConditionalIndexSetter(2)`.
- `PitStopProgress` 0x4A68B0 line 71: `if viewed roster == this car and this+16484 != 1: sub_4D8760(6, 0)`. The ECX of that call is not shown in the decompile (OPEN). So switching the view onto a car that is in the pit sequence can trigger the 0x4D8760 setter with a different object.
- `NetCommPlayerSlotSync` 0x45FC50 lines 255-269: on unload it rewrites 0x7C54DC and 0x7C54E0 to another car (controller == -1 search).
- `DisqualificationCheck` 0x49BAB0: reads 0x7C54E0 +16484 (== 3).
- `CameraModeChangeRequest` 0x40C5B0 (see A11), `MainCameraDirectorUpdate`, `CameraModeViewportUpdate`, and the audio/particle/visibility readers.

So a viewed-car change is not display-only. The owner's "race data unchanged" test must cover these readers, not only positions and times.

Open: what `flt_5EA618` (the driver table that receives event 264) feeds, and whether a 264 entry changes results (OPEN, the same question the spec calls T2).

### A11. Per-frame effects after a retarget (FACT from code, DERIVED consequence)

- Frame order (FACT): `GamePostMainLoop` 0x42D8A0 calls `MainGameTickUpdate` 0x42C690 (line 27 of its decompile), then later calls `MainCameraDirectorUpdate` 0x40CA30 (when `v2` is 0). The broker drains its task queue at the top of `MainGameTickUpdate` (see D1).
- `MainCameraDirectorUpdate` (FACT, `functions/` raw): every frame it calls `CameraTargetUpdateOnPlayerChange`, then `KeyboardInputActionMapper` (if `this+46` is 0), then `CameraModeChangeRequest`, then the active view's per-frame update (vtable+0x0C = 0x409A00 for the director).
- `CameraModeChangeRequest` 0x40C5B0 (FACT): if the viewed roster != the player roster and the manager state `this[3]` is -1 or 1, or is 2 with `dword_7CFCC8 == 0`, it sets `this[5] = state` and calls `GameActionStateMachine(5, 0)`. State 5 is circuit group selection.
- DERIVED: a retarget to an AI car while the manager state is -1, 1 (or 2 with the flag clear) will move the view to a CIRCUIT camera in the same or the next frame, with no key press. This matches the circuit investigation (4.5). It is a candidate explanation of the "PgDn circuit" observation, not proven. Which manager states occur in director view is OPEN: the first live test must log `manager+0x0C` before and after the cut.

### A12. Recommended car-switch mechanism

Options considered:

| Option | What it calls | Side effects (from A2 to A11) | Live precedent |
|---|---|---|---|
| A. Full chain, one call per cut | 0x40D240 (manager, roster) | viewed pointer and index; audio; CameraViewAssignment state branch (A5); event-264 insert for AI targets during a race (A4); write to 0x008AD298 (A3); active-view vtable+0x10 | FACT live: numpad path (return to player, phase 0 T1). Same call for AI targets is unproven live for the race-data test. |
| B. Viewed-car setter only | 0x42BBD0 (roster) | viewed pointer and index only. No event-264, no 0x008AD298 write, no CameraViewAssignment. Per-frame CameraTargetUpdateOnPlayerChange still runs next frame (A11). | FACT in shipped code: CO-OP `retarget_view_off` (`development\mods\coop_mode\dll\coop_mode.cpp:2533-2560`, 2026-10-11 crash fix) and the native unload path (NetComm 0x45FC50, A6) call 0x42BBD0 alone on unload. Live for a living switch: UNPROVEN. |
| C. Cycle with CyclicShift (command 101) N times | 0x40D2D0 per step | the full chain per step (A-option A effects for every car passed), plus `sub_4F0D20` per car per step | FACT key path, but many more side effects |

Recommendation: **Option B first** (the setter the game itself calls on unload), gated by owner live test T2b (section 7). Reasons: it leaves out the one step the owner's rule is about (event-264 insert, A4) and the 0x008AD298 write (A3); CO-OP already calls it in shipped code; it is one call per cut, unlike C.

Fallback: **Option A** only with an explicit owner decision (new D-TV item), because the insert is the game's own behaviour on a manual switch and the owner's "race data unchanged" test cannot pass for AI targets as the rule is written.

Do not use: option C (N chain calls), or writing 0x7C54DC alone (numpad finding: the camera does not move; the index is only an input to the game's targeting).

Guards common to B and A (to be implemented, see section 6): race session active, roster target live (`+0x32C8 != -1`), slot consistent (A13), manager state not in the refused set (A11, set OPEN until logged), and the prologue bytes (A9) verified. Read back `0x7C54E0` immediately, then verify the view owner and followed car on the next capture (D4).

Risks (must be on the owner's test list): A4 (only for option A); A10 pit-logic readers (pit-lane targets); A11 circuit push for states -1, 1, 2; the camera may not visibly move in director view under B (unproven); NetComm may re-target on its own when slots change (A10).

### A13. Roster id versus slot index (DERIVED)

- `SetPlayerVehicleIndex` computes the car as `0x5EC6D8 + 18624*a1`, so its argument is the pool SLOT. The chain and the CyclicShift pass `*(vehicle + 0x4894)` (roster id) into that same argument (`functions/CyclicShift`, chain step 1).
- DERIVED: roster id == slot index for every live car. The phase 1 code (`dcam_find_driver`) takes the roster from +0x4894 and passes it to the chain. Runtime check to add: `(veh - 0x5EC6D8) % 0x48C0 == 0` and `roster == (veh - 0x5EC6D8) / 0x48C0`. If false, refuse with reason "roster is not the slot index; nothing was written".

### A14. Existing phase 1 code versus this analysis

- `dcam_chain_switch` (`director_state.inc`, CUT) calls the chain with (manager, roster): correct arguments per A2. Blocked by `DCAM_CHAIN_VERIFIED 0`.
- Phase 1 RESTORE after a car switch: the player car is the roster of `0x7C54E8` (player pointer), not the viewed pointer. Use the player pointer (A8).
- The phase 1 claim "the chain has six steps and step 6 writes roster id" is wrong (A3).

---

## B. Circuit (trackside) cameras as cut targets

### B1. What is known (FACT)

- The three vtable constants are already in the code: `director_state.inc:224-226` (`DCAM_VT_DIRECTOR 0x0059E60C`, `DCAM_VT_STATIC 0x0059E678`, `DCAM_VT_TRACKING 0x0059E6AC`).
- They are verified in the canonical EXE:
  - Object stores (`mov [ecx], imm32`) at file/VA 0x4084B9 and 0x40852A (director), 0x40AC09 and 0x40AC4A (static group), 0x40B569 and 0x40B5AA (tracking group). Scan of absolute dwords.
  - Vtable slot values read from `.rdata` file offsets 0x19E60C, 0x19E678, 0x19E6AC (the constant is the vtable address; its slots are functions):
    - director: +0x04 0x4096D0, +0x0C 0x409A00 (CameraModeViewportUpdate), +0x10 0x409F40, +0x18 0x40A5A0.
    - static: +0x04 0x40B030, +0x0C 0x40B260, +0x10 0x40B360, +0x18 0x40B380.
    - tracking: +0x04 0x40B850, +0x0C 0x40BAA0, +0x10 0x40BBC0, +0x18 0x40BBE0.
  - These match the circuit investigation table (section 4.4).
- The manager fields `+0x08` (active view), `+0x1C` (static group), `+0x20` (tracking group), `+0x10` (group count) are from the same investigation and are used by `dcam_capture_locked` already.

### B2. What is missing (OPEN)

- The circuit camera index. Candidate `group+0x14` (0 after load). The methods that would select it are vtable+0x18 and +0x1C of the groups (0x40B380, 0x40B3A0, 0x40BBE0, 0x40BC00). They are not in `functions\` (the SDK `inspect` fails for them). The index cannot be written or read with confidence.
- The followed car of a circuit camera. The group objects have no field that names a car. The entry format (`ActivationLocation`, `ActivationRadius`) suggests proximity selection, not an index (OPEN O3).
- How a circuit selection is written. The only FACT path is `GameActionStateMachine` state 5 (`case 5` writes `this[2]` and `this[3]`, not the index).
- The `sub_40D080` branch in state 5 (circuit investigation 4.8, OPEN).

### B3. Consequence for phase 2

- A cut to a circuit camera needs a write that is not known. Recommendation: circuit cut stays OUT of phase 2. Phase 2 shows the on-screen owner only (read-only): `view_owner` = director | circuit_static | circuit_tracking | unknown, and `angle` = null unless `view_owner == director` (the circuit investigation 5.4 trust fix, which is still required: the current code sets `angle` from the director mode in every state).
- Circuit cut becomes a candidate only after the owner's live test C1 (section 7) proves a stable write for an index (O1) and a followed-car field (O2).

---

## C. Leader time gap and battle for the TV driver list

### C1. Time gap to leader (FACT, existing helper; DERIVED use)

- Data: `c.tt` = `total_time` = veh+0x4098 (`web_telemetry.c:788` `OFF_TOTAL_TIME_SF`, read at `:5237`, written in the car JSON at `:6005`). `c.l` = lap number (veh+0x403C). `c.p` = position (veh+0x4060). `c.id` = roster id. `c.rt`/`c.fs` = retired/finish state.
- Meaning of 0x4098: race clock written at each line crossing for driven cars (finding `driven-cars-line-clock-4098`, FACT). Position 0x4060 changes only at line crossings (finding `race-rank-line-crossing-only`, CONFIRMED).
- Existing helper, live-verified per its comment (2026-07-25, frida): `telemetry_viewer.js:2788` `gapTo(c, ref, raceSession)`. It returns 'LEADER', '+NL' (lap-aware), '-' for DNF/retired or missing data, and '+s.sss' from `c.tt - ref.tt`. The same logic exists inside the C string at `web_telemetry.c:6675-6690` (`gm`/`gapStr`, legacy workbench).
- The TV driver list does not use it: `telemetry_viewer.js:3402` is `TVD_DASH` in the "Leader gap" cell. Column order (confirmed `:3392-3404`): Pos, Driver, Lap, Ahead, Behind, Leader gap (3402), Pit, Battle (3404). `tvdDrivers` (`:3223-3232`) does not copy `tt`.
- DERIVED: the leader-gap cell = `gapTo(car, leaderCar, true)` where leader = the car with `p == 1`. Add `tt`, `rt`, `fs` to the `tvdDrivers` output row. The gap is the gap AT THE LAST LINE CROSSING of each car, not "now": label it so (OPEN: owner check against the in-game timing screen, T-C1).
- AI cars: DERIVED that AI cars in race write 0x4098 too (they are "driven"; only AI-sim lap paths write 0x40B8 instead, per the same finding). Verify in test T-C1.

### C2. Battle (DERIVED research; not for phase 2 without the owner's id test)

- Per attacker, five team records in the car's own struct, base 0x42CC, stride 28: `+0` match flag (0x42CC), `+4` opponent id (0x42D0, equals the opponent's +0x4894), `+8` mode (0x42D4), `+12` per-team timestamp (0x42D8), `+16` attack patience (0x42DC). Source: `TV_DIRECTOR_BATTLE_SIGNAL_2026-10-13.md` sections 2 and 5 (the offsets are from the ABI and the writers recorded there; checked against that record, not re-decompiled here).
- The read path needs no new game reads: `compatibility_runtime.inc` copies the full vehicle struct of every car on each capture (FACT, `copy_game(mirror_vehicles[i].bytes, address, VEH_STRIDE)` at about line 45-57). The battle rule would run in the DLL on the mirror plus a short history.
- OPEN (blocking): (1) the 22-car id check (battle file test 1; not run); (2) the car ahead may not be among the five records, so "no record" does not prove no battle; (3) meaning of modes 3, 4, 7, 8; (4) patience units; (5) window and thresholds (proposals only, D-TV3).
- Recommendation: battle stays "—" in phase 2 (reason text: "battle needs the record id check; not verified"). The DLL work for it is phase 2b after the owner's id test passes.

---

## D. Read-back timing (phase 1 OPEN item 4)

### D1. Where the write runs in the frame (FACT)

- The broker's per-frame hook is installed on `MainGameTickUpdate` 0x42C690 (`game_update_broker.c:37` `MAIN_UPDATE_ADDRESS`, hook at `:2633`). `broker_update_hook` calls `broker_drain` first, then the original (`game_update_broker.c` around 1459-1470).
- `broker_drain` runs tasks in a `while (executed < F1C_BROKER_UPDATE_BUDGET)` loop until the queue is empty or the time budget is reached (`game_update_broker.c` 1253-1300). A task posted from inside a task is picked up by the same drain if the budget allows. So "post and run next frame" is not automatic.
- Both the director command task (`dcam_cmd_task`) and the telemetry capture (`capture_on_game_thread`, `compatibility_runtime.inc:27`, posted by a worker loop with a pending latch, `:202`) run in this drain.

### D2. Order relative to the director update (FACT)

- In the same frame: the drain (command write, immediate read-back) runs at the top of `MainGameTickUpdate`. Then `MainCameraDirectorUpdate` runs later in `GamePostMainLoop` (A11). In that update: the keyboard mapper, `CameraModeChangeRequest` and the director's per-frame viewport update may change the mode, index, or view.
- So the phase 1 read-back (taken inside the command task, `dcam_set_angle` and `dcam_chain_switch`) is taken BEFORE the same frame's director update. It can report "applied" and be changed one step later.

### D3. Camera Editor redirect (FACT)

- Camera Editor's per-frame apply hook (detour on 0x409A00 in the Camera Editor design; `tvd0fix` finding) redirects disabled ONBOARD views every frame. A redirect is visible only after that frame's update, i.e. after the immediate read-back.

### D4. Recommended read-back (DERIVED; live test T4)

- The capture task of the NEXT frame runs before that frame's director update, so it sees the state left by this frame's update. That is one full update after the write. Use it as the verify step:
  1. The command task writes and takes the immediate read-back (as now). Store `verify_pending = seq`, `expect_mode`, `expect_index`, `expect_viewed = veh`.
  2. In `dcam_capture_locked` (every capture), if `verify_pending` is set and the sample is valid: compare mode, index, viewed pointer. Publish `verified: true` if equal, or `status: "redirected"` with the read values. Clear `verify_pending`. Record the capture sequence number as `verify_capture_seq`.
  3. The HTTP handler waits up to 300 ms for `verify_pending` to clear (inside the existing 1 s wait budget); if not cleared, answer `status: "applied"` with `verified: false` and reason "verify not yet taken". Do not turn that into "applied".
- Phase 1 behaviour is unchanged when Enabled=0 (no writes, no verify).

---

## E. Phase 2 specification (mechanical; implement only after the owner's gates in section 7)

Scope: car switch (option B), angle after car, RESTORE of the car, circuit read-only owner, leader gap column, verify read-back. Out of scope: circuit cut (B3), battle values (C2), follow switch and DIR channel (phase 2 of the earlier spec, separate record), queue/Take (phase 3).

### E1. Constants and gates (`director_state.inc`)

- `DCAM_SETVIEW_ADDR 0x0042BBD0u` (stdcall, one int32 argument).
- `DCAM_SETVIEW_VERIFIED 0` (compile-time) and `g_dcam_carswitch_enabled` (ini `[Director] CarSwitch=0`, default 0). Both must be 1 for CAR writes. Kill switch: `[Director] Enabled=0` still removes all routes (phase 1 rule).
- `DCAM_SETVIEW_BYTES[22]` = the A9 pattern for 0x42BBD0. The resolver rejects if not equal; reason "viewed-car setter bytes not as verified; nothing was written".
- `DCAM_VERIFY_WAIT_MS 300u`.
- Refused set for manager state (`this[3]`): REFUSE while `-1`, `1`, or `2` until the owner's live test T2b logs the states in director view (A11). Reason: "manager state would hand the view to a circuit camera (CameraModeChangeRequest); nothing was written". Keep the set in one constant so the owner can change it.

### E2. Pure helpers (testable offline)

- `dcam_setview_bytes_ok(const uint8_t *p)`: compares 22 bytes with `DCAM_SETVIEW_BYTES`. Must return 1 for the 0x42BBD0 bytes and 0 for the 0x42BC40 bytes (store `E8`).
- `dcam_slot_of(uint32_t veh, uint32_t *slot)`: returns 1 when `veh` is in the pool and `(veh - VEH_BASE) % VEH_STRIDE == 0`, sets slot. Used with the roster check (A13).
- `dcam_plan_car_and_angle(kind, have_car, have_angle)`: returns the ordered step list: CAR then ANGLE for CUT; ANGLE only for ANGLE; CAR(player) then saved angle for RESTORE. Order is part of the test.
- `dcam_view_owner(...)`: already exists (phase 0/1). Keep. Add the rule: `angle` = null unless owner == DIRECTOR.
- `dcam_status_from_verify(immediate, verify_state)`: returns "applied" only when `verified` is true; "applied_unverified" when not yet; "redirected" when the verify differs.

### E3. Command flow (game thread, one broker task per request)

For CUT `{"driver": "<name>", "angle": "<key>"}`:
1. Refuse if: Enabled=0; CarSwitch=0; engine not resolved; prologue bytes not verified; director pointer not plausible; driver not live (`dcam_find_driver`); slot check fails (A13); manager state in refused set (E1); session not active (`stats.session_active` and the capture sample valid).
2. If the target is already the viewed car: skip the CAR step; go to ANGLE.
3. Write CAR: `((int(__stdcall*)(int))0x42BBD0)(slot)`. Read back `0x7C54E0` (must equal veh) and `0x7C54DC` (must equal the table index of veh). If not equal: status "refused", reason "viewed car did not change after the setter; the angle was not written". (No angle write after a failed car step.)
4. Write ANGLE through the existing `dcam_set_angle` (dispatch 0x409440). Order matters: the SWINGMAN remap reads the viewed car's +13388 byte (A10/A5).
5. Set `verify_pending` (D4). Status is final only after verify.

For RESTORE: the same as step 3 with the PLAYER roster (`0x7C54E8` pointer, slot from A13), then the saved angle via `dcam_set_angle`, then verify.

For ANGLE: unchanged from phase 1, plus verify.

### E4. Result object (additive, `API_VERSION` stays "9" until the contract pass)

`{"status": "applied" | "applied_unverified" | "redirected" | "refused" | "partial", "reason": "...", "readback": {"mode", "index", "viewed_slot", "viewed_roster"}, "verified": bool, "verify_capture_seq": int|null, "thread_id": int}`.
- `partial` only for RESTORE when the car step is skipped for a reason other than a refusal.
- Add to `GET /director/state`: `viewed_slot`, `viewed_roster`, `player_roster`, `view_owner`, `view_owner_reason`, `manager_state` (diagnostic). The `angle` field is null unless `view_owner == director`.

### E5. Viewer (`telemetry_viewer.js`)

- Leader gap cell (line 3402): `gapTo(car, leader, true)` (helper at 2788). Add `tt`, `rt`, `fs` in `tvdDrivers` (3223-3232). Leader = car with `p == 1`; if none, '-'.
- Battle cell (3404): stays `TVD_DASH` with title "battle needs the record id check (not verified)".
- Cut body (`tvdBody`): unchanged shape: `{"driver","angle"}`. Result text (`tvdResultText`): show `applied_unverified` as "sent, not yet verified". Show `redirected` with the read-back.
- Circuit owner: when `view_owner` is circuit_*, the on-air strip shows "CIRCUIT (static|tracking, N cameras)" and "camera number unknown"; no angle name.
- Buttons: Cut disabled unless `write_enabled` and `viewer_local` and `carswitch_enabled` (new field).

### E6. Tests to add (not run by this record)

`runtime_test.c` (new function `test_director_phase2()`, called from `main` next to `test_director_phase1()` at about line 3925):
- `dcam_setview_bytes_ok`: accepts the 0x42BBD0 bytes; rejects the 0x42BC40 bytes; rejects a one-byte change anywhere in 0..21.
- `dcam_slot_of`: accepts `VEH_BASE + 18624*k` for k in [0, 103]; rejects VEH_BASE+1, below base, and beyond the pool.
- `dcam_plan_car_and_angle`: CUT order is [CAR, ANGLE]; ANGLE only; RESTORE uses the player car; no step when the target is already viewed.
- `dcam_status_from_verify`: "applied" only with verified=1.
- Refusal reasons: each reason string is non-empty and under `DCAM_RESULT_REASON`.
- Circuit owner: `dcam_view_owner` with a circuit pointer returns CIRCUIT_STATIC/TRACKING (existing) and the JSON angle is null (new function returns the angle string only for DIRECTOR).
- Build: `gcc -fsyntax-only -Wall -Wextra -Werror` on the telemetry sources (phase 1 method).

`viewer_test.js` (new block "TV Director phase 2"):
- Leader gap column uses the gapTo rules: LEADER for p==1, '+NL' for lapped, '-' for DNF/retired/missing `tt`, '+s.sss' otherwise (assert with two fixture cars 0.5 s apart).
- Battle cell is the placeholder text, not a number.
- CUT POST: one request, header `X-Telemetry-Action: director`, path `/director/cut`, body `driver` and `angle` only; no confirm/alert/prompt (same as phase 1).
- `applied_unverified` text and `redirected` text.
- Angle is null in the on-air strip when `view_owner` is circuit_* (fixture).
- Cut disabled when `carswitch_enabled` is false.

### E7. Abort and restore

- Abort on: any write outside the game thread; a failed bytes check; a car step that changes 0x7C54E0 to something other than the target; any race result or timing change seen in the owner's test (section 7).
- Restore: RESTORE command (player car, saved angle). Kill switch: `[Director] CarSwitch=0` (no CAR writes) and `Enabled=0` (no writes at all).

---

## F. Answers to the owner's questions (short)

- Recommended car-switch mechanism: option B (call 0x42BBD0 with the target slot, once per cut, after the bytes check), then angle, then next-frame verify. Option A (full chain) only with an explicit owner decision on the event-264 insert. Not option C.
- Risks: event-264 insert for AI targets (option A); pit-logic readers of the viewed car (A10); manager states -1/1/2 hand the view to circuit cameras (A11); the camera may not move under option B in director view (unproven); NetComm rewrites the viewed car on slot changes.
- Still unknown: what the driver table `flt_5EA618` feeds (264 event); what `0x008AD298` is; what the manager states are in director view; circuit index and followed car (O1, O2); `GetTeamIDByFinishPosition` return meaning; the key that sends command 101; the gate functions `Return0Stub_9280` and `ConditionalFlagReturn`.

---

## 7. Owner live tests (run by the owner; the game must be quiet; check `sim_time` and capture sequence advance between reads)

Rules unchanged from phase 1: no gate or test program while a race is live; before counting a read, check sim time or tick is advancing (memory rule). Payloads only from `%TEMP%` staging. Record each result as a dated operations record under the telemetry component.

- T2b (car switch, option B, the gate for E): single-player race, Enabled=1, CarSwitch=1 (test build only). Cut to one AI car in director view. Pass: camera follows the target on screen; `viewed_roster` equals the target; positions, times and classification unchanged for all cars over 60 s versus a recording without the cut; `manager_state` logged before and after; no circuit camera appears. Fail: any change in race data, or a circuit view. Log the state values to decide the E1 refused set.
- T2c (baseline for option A, only if the owner wants A): the same cut done by the numpad path (key 74/78 or 156) and by the endpoint, then compare the event-264 entries. If the numpad path gives the same entries, the event is native and the rule must change (D-TV2 wording).
- T2d (pit-lane target): cut to a car in the pit lane while it is in its stop. Pass: no change to the stop; `sub_4D8760(6,0)` side effect logged as the only difference (or none).
- T4' (next-frame read-back): cut to the same target twice and to two angles. Pass: `verified: true` on all; a deliberate redirect (Camera Editor, disabled ONBOARD view, T8) reports `redirected` in the verify, not in the immediate read.
- T5' (order): CUT with car then angle. Pass: angle read-back matches; the SWINGMAN remap uses the target's +13388 byte as expected (record the remap).
- T-C1 (gap): three cars in the same race, values from `gapTo` compared to the in-game timing screen at the same sim time (within one line crossing). Pass: difference below one line crossing; label "at last line" visible.
- T-C2 (battle id check, blocks C2): for 22 cars, every populated record `+4` equals some car's `+0x4894` (battle file test 1).
- T-C3 (circuit owner, read-only): steps 1 to 5 of the circuit investigation section 6. Pass: owner classification follows the screen; angle null on circuit.
- T1 (phase 0) stays as passed. T3 (refused switch) to be run with the E1 refused states.

## 8. Documentation gate

- Durable record: this file (the one allowed new file).
- Findings: `tvd2-*` lines appended to `f1c_ida_decompiled\findings.jsonl` (FACT, DERIVED, OPEN). The file was 1497 lines before this task; the appended lines are the last ones. Restoration of the findings file: keep the first 1497 lines (no backup file was created because the owner asked for one new file only; the append is the only change).
- Fact and inference split: sections A to D and each finding tag.
- Pending validation: T2b, T2c, T2d, T4', T5', T-C1, T-C2, T-C3 (section 7). Nothing in this record was run live.
- Corrections recorded here (not edited in the phase records): spec 4.4 step 6 (roster id is wrong; literal 1 on 0x008AD298); SDK address of `VehicleSelectionCyclicShift` (0x40D2CA is not the entry; 0x40D2D0 is); SDK `callers: []` for it; `SetPlayerVehicleIndex` name (it sets the viewed car); `CameraTargetCurrentTrackedVehicle` name (it returns to the player's car).
- Workspace validator (`tools\workspace\check-workspace-structure.ps1`) was not run: no structural or governance file changed and this record is a component research file.

## 9. Follow-ups (not done now: other agent is editing the viewer and the DLL)

1. Add a link line to this record in `CURRENT_STATE.md` (TV Director phase 2 research) after the other agent finishes.
2. Owner decision D-TV7: option B versus option A for the car switch (section A12), and the wording of D-TV2 (the event-264 insert is native to a manual switch).
3. Owner decision D-TV8: circuit cut stays out of phase 2 (section B3).
4. Correct `TV_DIRECTOR_SPEC_2026-10-13.md` section 4.4 step 6 and `TV_DIRECTOR_PHASE1_RECORD_2026-10-13.md` section 3.1 (roster id claim; blocked-chain wording) with a backup, when editing is allowed.
5. SDK owner: fix `VehicleSelectionCyclicShift` address (0x40D2D0), add its callers (wrapper 0x46F672, dispatcher 0x46F640), and add `SetPlayerVehicleIndex` to the names list as "set viewed car".
6. Identify the owner of the vtable at VA 0x5A001C (dispatcher 0x46F640 reference) and the sender of key code 101.
7. Decompile `GetTeamIDByFinishPosition`, `ConditionalFlagReturn`, `Return0Stub_9280`, `ConditionalModeDispatchWithGlobalGate`, and the two circuit group methods (0x40B380, 0x40B3A0, 0x40BBE0, 0x40BC00) for the circuit index (O1).
8. Audit the readers of 0x7C54DC (31 functions) and 0x7C54E0 (58) for write side effects before any car-switch build.
9. Run the 22-car battle id check (T-C2) before any battle value is shown.
