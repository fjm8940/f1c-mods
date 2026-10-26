# TV Director: circuit camera investigation (2026-10-13)

Status: investigation only, static evidence. Nothing was changed in the DLL, the viewer, `director_state.inc`, `web_telemetry.ini`, `CURRENT_STATE.md`, or the game folder. The game was running; no runtime test, injector, `TELEMETRY_TESTS`, fuzzer or HTTP request was used in this task. Reads of the on-disk EXE were read-only byte reads (PE section walk and byte search).

Tier vocabulary: FACT = raw Hex-Rays line in `functions\` or read-only EXE bytes (cited). DERIVED = inference from FACTs, not proven live. OPEN = not resolved. PROVEN = owner live observation (phase-0 record).

## 1. Question

The director object (pointer at 0x005EA528) keeps mode +0x34 = 3 (SWINGMAN) or 2 (TVCOCKPIT) while the screen shows a circuit (trackside TV) camera after PgDn. Where does the game store:

- (a) which camera system is on screen (director view or circuit system), and
- (b) which circuit camera is selected and which car it follows?

## 2. Live facts (owner, 2026-10-13-18:36)

- L1 PROVEN: modes 0-4 read correctly when switched with the cockpit and onboard keys (phase-0 record, findings `tvd0-live-t1-partial-20261013`).
- L2 PROVEN (`tvd0-circuit-cams-not-in-director-mode-20261013`): PgDn showed circuit cameras. Three reads 00-00-00-00-00-00 gave mode 3, index 0, with sim time advancing 104.2 to 107.4 s.
- L3 Owner, 18:36: from TVCOCKPIT, PgDn showed circuit cameras; the data still said mode 2.
- L4 Owner verdict: the camera data is not trustworthy until investigated.

## 3. Short answer

- (a) On-screen system: the pointer at absolute address **0x005EA518** (manager+0x08). Value equal to the director pointer (0x005EA528) means the director view is on screen. Value equal to the group at 0x005EA52C or 0x005EA530 means a circuit system is on screen. The game's own code uses this same test in four places. FACT for meaning; live read still to do.
- (b) Circuit system: which group is known (FACT). Its entry count is known (DERIVED from FACT). **The selected circuit camera index is not proven** (OPEN; candidate field group+0x14). **The followed car is not found** (OPEN).
- The phase-0 INFERENCE (circuit selection stored inside mode 3, candidates director +0x2C, +0x30, +0x50) is **refuted**. See 4.2 and 4.6.

## 4. Findings

### 4.1 PgDn is a circuit-group action, not a director mode (FACT)

`functions\KeyboardInputActionMapper_0040be70.c`, the in-race key switch:

- case 209 (PgDn): `CameraLockCheck(5) && !GameActionStateMachine(5,1)` then returns "view changed".
- case 201 (PgUp): `GameActionStateMachine(3,1)` under conditions.
- case 199 (Home): `GameActionStateMachine(4,1)`. case 210 (Insert): `GameActionStateMachine(0,1)`.
- case 156 (NumpadEnter): `CameraTargetCurrentTrackedVehicle`. cases 74/78: `CameraTargetRaceLeader`.

DERIVED: the names PgUp/PgDn/Home/Insert for codes 201/209/199/210 come from the standard DirectInput key codes (DIK_PRIOR 0xC9, DIK_NEXT 0xD1, DIK_HOME 0xC7, DIK_INSERT 0xD2), not from the binary. This matches the owner's PgDn observation (L2, L3).

### 4.2 State 5 never writes the director mode (FACT)

`functions\GameActionStateMachine_0040cde0.c`, `case 5`:

- It does not call `ViewCameraModeDispatch`. So the director mode at +0x34 is not written by this path.
- It sets the active-view pointer (this[2] = manager+0x08) to this[8] (manager+0x20), or keeps this[7] under the branch in 4.8.
- It calls the active group's vtable+0x10 (`(*(vtable)+16)`).
- It writes this[3] (manager+0x0C) = 5 after the block.

Cases 0-4 and 7-10 do call `ViewCameraModeDispatch`. Case 3 sets this[2] = this[6] (the director) before it does.

DERIVED: this explains L2 and L3 exactly. PgDn changes the active view, not the director mode.

### 4.3 The active-view pointer is at 0x005EA518 (FACT, live proof pending)

- The manager is a static struct at 0x005EA510, not a pointer. Callers pass its address: `GameActionStateMachine(&dword_5EA510, ...)` in `sub_4DA840_004da840.c:48`, `sub_4B9890_004b9890.c:39`, `RaceFinishHandler_004a0ba0.c:8`, `PitTimer_004d2e50.c:133`.
- Director pointer = manager+0x18 = **0x005EA528** (`CamMan_Init_0040c920.c`, `MainCameraDirectorUpdate_0040ca30.c`; the phase-0 resolver uses the same offset, `DCAM_DIRECTOR_FROM_MANAGER` 0x18).
- Active view = manager+0x08 = **0x005EA518** (`this[2]`):
  - `MainCameraDirectorUpdate_0040ca30.c`: activates `this[2]` (vtable+0x10) after a key action; calls `this[2]` vtable+0x0C every frame; and chooses the car-follow source with `this[2] == this[24]` (director: followed car `dword_7C54E0`+88..96) or else vtable+0x18 of `this[2]`.
  - `CamMan_Init_0040c920.c`: sets `this[8] = this[24]` (director) at init, because `sub_40C140` returns 0 in the raw decompile and the `if (!sub_40C140(...))` branch is always taken.
- The game's own "director is the active view" test is `dword_5EA518 == dword_5EA528` in `ModeBasedDispatch_0049f790.c:9-10`, `VehicleStateUpdate_19d0_004e19d0.c:123`, `AudioSoundEffectController_004e0ed0.c:52` and `sub_4D8450_004d8450.c:151`.

DERIVED: 0x005EA518 is the pointer that decides what the screen shows. Not yet read live.

### 4.4 The two circuit groups (FACT, with EXE bytes)

- manager+0x1C (**0x005EA52C**) is built by `SceneFileLoadByCurrentIndex_0040c240.c` via `StructInitWithVTable` (`sub_40AC00`, vtable **0x0059E678**). Its loader, `StaticCameraSceneLoader_0040b030.c`, reads `[StaticCam]` entries of 160 bytes each (`aStaticcam`).
- manager+0x20 (**0x005EA530**) is built by `ModuleAssetPathResolver_0040c3b0.c` via `StructInitWithVtable` (`sub_40B560`, vtable **0x0059E6AC**). Its loader, `MainView_0040b850.c`, reads `[TrackingCam]` entries of 208 bytes each (`aTrackingcam`).
- Both loaders open the track's `.CAM` file: the suffix literal `aCam` is at VA 0x005BE880 (the `.CAM` string in the EXE .rdata, read-only byte search; also `STATICCAM`, `TRACKINGCAM`, and writer formats `%sStaticCam=%s`, `%sTrackingCam=%s`).
- Object layout (FACT, both loaders, lines 95-97 and 120-122): entries pointer at +0x0C, count at **+0x10**, and 0 at +0x14 after load.
- Vtable bytes read from the on-disk EXE (PE section walk, ImageBase 0x400000, `.rdata` RVA 0x19E000; read-only). The director vtable 0x0059E60C is also listed for comparison:

| Object | vtable | +0x04 | +0x0C (per-frame update) | +0x10 (activate) | +0x18 (target/entry) |
|---|---|---|---|---|---|
| director (manager+0x18) | 0x59E60C | 0x4096D0 | **0x409A00** CameraModeViewportUpdate | 0x409F40 | 0x40A5A0 |
| StaticCam group (manager+0x1C) | 0x59E678 | 0x40B030 StaticCameraSceneLoader | 0x40B260 | 0x40B360 VirtualDispatchChain | 0x40B380 |
| TrackingCam group (manager+0x20) | 0x59E6AC | 0x40B850 MainView | 0x40BAA0 | 0x40BBC0 | 0x40BBE0 |

- Consequence (FACT plus DERIVED): the director's own per-frame update (`CameraModeViewportUpdate`, the code that applies director modes) runs only when the director is the active view. When a group is active, the screen follows that group's update and target methods.
- The vtable identity is a usable check for the reader: the object at the pointer must have vtable 0x59E678 or 0x59E6AC (or 0x59E60C for the director). This is read as memory, with no call.
- Documentation caveat: `docs/12-globals-reference.md` lines 2161 and 3493 give 0x59E6AC and 0x59E678 as "unknown"/"UI object" guesses. Those names are not used here; the loader names above come from the decompile.

### 4.5 Circuit state can be entered without a key press (FACT)

Callers that enter state 5 (group selection) outside the key mapper:

- (5,1): `RaceFinishHandler_004a0ba0.c:8`, `PitTimer_004d2e50.c:133`, `sub_4DA840_004da840.c:48` (conditions on session flags), `ModuleInitializationWithFallback_00434820.c:7`.
- (5,0): `sub_4B9890_004b9890.c:39` (loading phase), `CameraModeChangeRequest_0040c5b0.c:22` (spectated roster differs from the reference roster, in states -1, 1, or 2 with `dword_7CFCC8` = 0), `CameraViewAssignment_0040c650.c:28` (state 3 with flag `dword_7C54E0+13388`, spectated car is not the player's).

Redirect marker: `CameraViewAssignment_0040c650.c` sets manager+0x14 (this[5]) = 3 when it auto-switches, and restores the saved state with `GameActionStateMachine(this[5],0)` when the player's car is viewed again. `CameraLockCheck_0040c720.c` sets this[5] = -1.

DERIVED: the circuit view can appear with no key press (race finish, pit timer, spectating another car), so the reader must not assume that a circuit view came from PgDn.

### 4.6 Director modes 5-8 are not circuit cameras (FACT)

- `GameActionStateMachine` cases 7, 8, 9, 10 call `ViewCameraModeDispatch` with modes 5, 6, 7, 8 (`GameActionStateMachine_0040cde0.c`).
- `CameraModeViewportUpdate_00409a00.c` cases 5-8 use director fields +0x24, +0x28, +0x2C, +0x30 (this[9] to this[12]).
- `ViewCameraModeDispatch_00409440.c` cases 7 and 8 use director +0x2C (this[44]) and +0x30 (this[48]) as the objects for those modes.

So the director+0x2C and +0x30 objects that the phase-0 record suspected belong to director modes 7 and 8. They do not select a circuit camera. The phase-0 INFERENCE is refuted. Modes 5-8 are director-owned; their on-screen meaning is OPEN (no names in the corpus).

### 4.7 The state field at manager+0x0C is not the owner (FACT)

Case 5 writes this[3] = 5 after its count gate. If both group counts (+0x10) are zero, the active view does not change but this[3] is still 5. So manager+0x0C alone cannot say what is on screen. Use manager+0x08 against the three pointers.

### 4.8 Branch on repeat presses (OPEN)

In case 5, Hex-Rays prints `if ( !sub_40D080 || (v10 = this[7]) == 0 || this[2] != v10 )`. `sub_40D080` is an accessor (`return this[36]`, `sub_40D080_0040d080.c`), printed without call parentheses. Read as written, the branch sets the active view to this[8] (TrackingCam) unless this[7] (StaticCam) is non-null and already the active view, in which case it stays on this[7]. The real condition is unresolved. Treated as OPEN.

### 4.9 Circuit camera index (OPEN)

- Candidate: group+0x14 (0 after load). No other write to +0x14 was found in the corpus.
- The methods that would select and apply an entry are not in `functions\`: vtable+0x18 and +0x1C of the StaticCam group (0x40B380, 0x40B3A0) and the TrackingCam group (0x40BBE0, 0x40BC00). `VirtualDispatchChain_0040b360.c` shows the calling pattern: it calls vtable+0x18 twice, then vtable+0x1C on the result. The decompile for these four addresses is missing from the corpus (`f1c_mod inspect` reports them unresolved), so the index field cannot be proven from the corpus.
- The writers and parsers show each entry carries `ActivationLocation` and `ActivationRadius` (`sub_4585E0_004585e0.c` lines 168-169; writers `sub_457670_00457670.c`, `sub_458A60_00458a60.c`). This suggests circuit cameras may switch by proximity to a location rather than by a stored index. DERIVED, not proven.

Verdict: the circuit camera number cannot be stated from static evidence. The reader must not show a number it has not proven.

### 4.10 Followed car (OPEN)

No field that names a followed car was found in the group objects. The car target for a circuit view would come from the group's vtable+0x18 path, which is not decompiled. The director's followed car (`dword_7C54E0`, roster `+0x4894`) is unchanged by the circuit state, but whether a circuit camera follows that car is OPEN.

Target-name strings in the corpus (`CameraTargetNameResolver_0040db00.c`: Player, Original, Last, AnyTrackside, None) are a config-level target mechanism. The literal `AnyTrackside` was not found in the EXE bytes (case-insensitive search), so its storage is OPEN (possibly in `.CAM` data).

### 4.11 Dispatch remap (FACT, already known)

`ViewCameraModeDispatch_00409440.c` remaps requested mode 3 to 2 unless `byte_7CFE88` is set, or `dword_7C54E0+13388` is 0, or `dword_8AB6E4` = 2. The UI must show the read-back mode, not the requested one (phase-0 `camera-swingman-tvcockpit-remap` FACT).

## 5. Proposed fix for `director_state.inc` (read-only, not applied)

Scope: extend the existing read-only capture (`dcam_capture_locked`, game thread, under `g_telemetry_cs`). Reads only; no writes; no calls into game code. Same snapshot, so all fields come from one frame.

### 5.1 Fields to read (all `copy_game`, 4 bytes unless noted)

| Name | Address | Meaning | Tier |
|---|---|---|---|
| `active_view_ptr` | 0x005EA518 (manager+0x08) | pointer deciding the on-screen system | FACT meaning, live pending |
| `director_ptr` | 0x005EA528 (already read) | director | FACT |
| `static_group_ptr` | 0x005EA52C (manager+0x1C) | StaticCam group | FACT |
| `tracking_group_ptr` | 0x005EA530 (manager+0x20) | TrackingCam group | FACT |
| `last_action_state` | 0x005EA51C (manager+0x0C) | last state-machine action, raw | FACT, caveat 4.7 |
| `auto_marker` | 0x005EA524 (manager+0x14) | 3 = automatic change to restore, -1 none | FACT |
| per group: vtable at `*ptr` | group+0x00 | identity check: 0x59E678 / 0x59E6AC / 0x59E60C | FACT |
| per group: count at `*(ptr+0x10)` | group+0x10 | number of circuit cameras in the group (valid if <= 128) | FACT layout, DERIVED meaning |
| per group: raw index at `*(ptr+0x14)` | group+0x14 | candidate index | OPEN, report as raw only |

Plausibility: pointers pass `dcam_plausible` and must be non-zero for a group; the vtable must match exactly; a count outside 0..128 is "unknown".

### 5.2 Classifier (pure function, unit-testable without the game)

```
owner = unknown
if active_view_ptr plausible:
    if active == director_ptr and director vtable == 0x59E60C:      owner = director
    elif active == static_group and vtable == 0x59E678:              owner = circuit_static
    elif active == tracking_group and vtable == 0x59E6AC:            owner = circuit_tracking
    else:                                                            owner = unknown
```

### 5.3 JSON additions (additive; see the contract note below)

- `view_owner`: `director` | `circuit_static` | `circuit_tracking` | `unknown`.
- `view_owner_reason`: short text, e.g. "active view == director (0x5EA528)" or "active view not recognised".
- `active_view_ptr`, `static_group_ptr`, `tracking_group_ptr` (hex or null), `vtable_ok` (bool per group).
- `static_count`, `tracking_count` (integers or null).
- `circuit_index_raw` (integer or null) with `circuit_index_verified: false` until the live test in section 6 passes.
- `last_action_state`, `auto_marker` (raw, labelled "diagnostic").

### 5.4 Labels and what to show

- `view_owner == director`: the existing angle is valid. Show "director view: <angle>". Modes 5-8: show "MODE 5" to "MODE 8 (unnamed)". Never invent names.
- `view_owner == circuit_static` or `circuit_tracking`: `angle` = null. Show "CIRCUIT (static, N cameras)" or "CIRCUIT (tracking, N cameras)", and "camera number unknown" while `circuit_index_verified` is false. The director's mode is shown only as "director stored view: <mode>, not on screen".
- `view_owner == unknown`: `angle` = null. Show "view unknown (pointer not recognised)". Never show a director angle as the on-screen angle.
- The viewer's On-air row must show `view_owner` first; the angle comes second and only for `director`.

The current code sets `angle` from the director mode for every state. That is the trust bug: the angle must be null unless `view_owner == director`.

### 5.5 Contract note

Adding fields changes the `/director/state` contract. `API_VERSION` stays "9" for now (phase-0 doubt 3). The contract bump and the `schemas\` update need an owner or contract pass before a build. Not changed here.

## 6. Live verification (owner, after a build with these fields; reads only)

Run these only after a build with the new fields is approved and installed by the owner. Use the DLL route for reads; do not run injectors or test exes, and do not write to game memory.

1. Start with the director on screen in TVCOCKPIT. `GET http://localhost:8080/director/state` three times, with the sim time advancing. Expect `view_owner` = `director`, `angle` = TVCOCKPIT, `mode` = 2.
2. Press PgDn once. Expect the screen to show a circuit camera and `view_owner` = `circuit_static` or `circuit_tracking`, `angle` = null, `count` > 0, and `mode` still 2. This proves the independence of +0x34 from the screen.
3. Press PgDn repeatedly and write down the on-screen camera each time. Compare with `circuit_index_raw`. If the raw index moves with the screen cycle, it is a candidate for `circuit_index_verified`. If it does not, the index field is wrong; leave it unverified.
4. Press PgUp. Expect `view_owner` = `director` again, with the read-back mode (SWINGMAN may read as 2 because of the remap).
5. Check that `sim_time` advances between samples. Identical samples mean a paused frame, not a stable view.

Record the samples in a dated operations record (time, key pressed, the full JSON line). Do not paste credentials or raw process output.

## 7. Open items

- O1. Circuit camera index: the field and its meaning (candidate group+0x14). Needs the decompile of vtable+0x18 and +0x1C (0x40B380, 0x40B3A0, 0x40BBE0, 0x40BC00), which is not in `functions\`. Next step: a Hex-Rays or IDA export of those four addresses, or the existing `F1 Challenge Online.exe.i64` database, read-only.
- O2. Followed car of a circuit camera: entry field not decoded (`sub_4585E0`, `sub_458A60`, and `vt+0x18`).
- O3. Whether circuit selection is by proximity (ActivationLocation and ActivationRadius) rather than a stored index.
- O4. The `sub_40D080` branch in state 5 (4.8).
- O5. Whether a circuit view follows the same car as the director's followed car (`dword_7C54E0`).
- O6. Where `AnyTrackside` and the other target names are stored.
- O7. Live proof that `0x005EA518` equals the on-screen owner (section 6). All FACT meanings above are static until then.
- O8. Circuit "Static" versus "Tracking" as the owner's own names for the two groups: the names here come from the loader code, not from the game UI.
- O9. Modes 5-8 on-screen meaning (no names in the corpus).

## 8. What was searched

- SDK front door: `python tools\f1c_mod.py start --json`; `evidence-query findings` for "camera" (31 results), "director" (48), "trackside" (1), "TV" (46), "tracking" (11), "static cam" (0); `inspect 0x409440` (ViewCameraModeDispatch); `inspect` for 0x40B380, 0x40B260, 0x40BBE0, 0x40BAA0, 0x40A5A0 (all unresolved: not in the recovery index).
- Raw decompile (`functions\`): references to 0x5EA510, 0x5EA518, 0x5EA528, 0x5EA52C, 0x5EA530, 0x7C54E0, 0x7C54DC; callers of ViewCameraModeDispatch (`GameActionStateMachine`, `GameStateBranchDispatch`, `MenuSlotActionDispatch`); `KeyboardInputActionMapper`; `CameraLockCheck`; `CameraViewAssignment`; `CameraModeChangeRequest`; `MainCameraDirectorUpdate`; `CameraModeViewportUpdate`; `TracksideCameraTargetLookup`; `CameraTargetNameResolver`; `CamMan_Init`, `CamMan_Post`, `CamMan_Restart`; `SceneFileLoadByCurrentIndex`; `ModuleAssetPathResolver`; `StructInitWithVTable`/`StructInitWithVtable`; `StaticCameraSceneLoader`; `MainView`; `sub_4585E0`, `sub_4572B0`, `sub_457670`, `sub_458A60`; `sub_40D080`, `sub_40C140`, `sub_40C520`.
- Read-only EXE bytes: PE section table; vtable slots at 0x59E60C, 0x59E678, 0x59E634, 0x59E6AC; byte search for `.CAM`, `STATICCAM`, `TRACKINGCAM`, `MAINVIEW`, `ActivationLoc`, `AnyTrackside` (not found).
- Prior work: `development\mods\camera_editor` (plugin `camera_editor.c`, `cam_sig.h`, `cam_shm.h`, `origin_20260816\REVIEW_2026-10-04.md`); `docs\12-globals-reference.md` lines 2161, 3493; `STRINGS_REPORT.md`; `shadow_source` notes for the `.CAM` suffix; `findings.jsonl` (`camera-*`, `camera-mode-setter-2026-10-13`, `camera-swingman-tvcockpit-remap-2026-10-13`, `tvd0-*`).

Not found: any decompile of the four group methods; any storage of the followed car for a circuit camera; the owner name for the circuit groups.

## 9. Corrections and limits

- The phase-0 record (`TV_DIRECTOR_PHASE0_RECORD_2026-10-13.md`, "Live T1 follow-up", the "Circuit ... BLIND" bullet) says PgUp/PgDn cycle a group inside mode 3. That INFERENCE is refuted by 4.1, 4.2 and 4.6. Its PROVEN facts (L2, L3) stand. The record was not edited in this task; a correction should be made with a backup.
- `findings.jsonl` (`tvd0-circuit-cams-not-in-director-mode-20261013`) says circuit selection is "stored elsewhere (OPEN)". Now narrowed: the owner pointer is 0x005EA518 (FACT, meaning pending live proof); the index is OPEN.
- Existing `camera-mode-setter-2026-10-13` (DERIVED) says keys 199/201/209/210 "route through GameActionStateMachine". Correct; but 209 does not write the director mode. This file records that.
- No live `/director/state` request was made here. The owner's live facts are used as given.
- `CURRENT_STATE.md` was not edited (per the task).
- The workspace validator was not run: no structural or governance file changed.
- Appended 12 lines to `findings.jsonl` (`tvd-circuitcam-*`, FACT or OPEN only). Backup: `findings.jsonl.BEFORE_20261013_circuitcam`. Lines 1-15 of that file already fail JSON parsing, and 3 mojibake markers were already present in the backup; they were not touched.
- The `.CAM` file contents (`[StaticCam]` and `[TrackingCam]` sections) were not read; only the EXE strings and loader code were checked.
