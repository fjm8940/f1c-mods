# H4 research: suppressing "riding with X" during TV Director (2026-10-14)

Author: Matias. Type: read-only static research. No code edits, no game run, no memory writes.
Evidence tiers: FACT = read in decompile text, recorded finding, or cited bytes; DERIVED = inferred from FACTs; OPEN = needs live test or owner decision.
Sources: `functions/*.c` (decompile text), `docs/12-globals-reference.md`, `docs/56` (voice map), `findings.jsonl`, `mods/sdk_sandbox/web_telemetry/TV_DIRECTOR_PHASE2_RESEARCH_2026-10-14.md` (A2, A4).

## 0. Bottom line

- The candidate in the brief is NOT the gate. `VirtualDispatchConditionalAudio` 0x40D090 does not play any audio. It calls a virtual method on `this[2]` and zeroes two fields of the global at `dword_8AFE88` (+420, +424). FACT.
- The "riding with" line is queued by `AIOutOfTrackEvent` 0x41A0C0, the 5th call in the chain. It inserts one entry (eid 264) into the driver table `flt_5EA618` when the target is not the player car and two globals are non-zero. FACT (decompile).
- Gate: `dword_7D0308 != 0 && flt_7CF3C8 != 0.0`. These two globals are read by about 40 commentary functions, so a global change is not narrow.
- Recommended: option (a), a save/zero/restore of `dword_7D0308` only around our own chain call. Live test required before release.

## 1. Call path from 0x40D240 to the line

Chain `GameStateDispatchChain` 0x40D240 (`functions/GameStateDispatchChain_0040d240.c`, thiscall, ECX = camera manager 0x5EA510, arg = target roster id):

| # | Call site (bytes, FACT from SDK finding) | Callee | Convention | Role |
|---|---|---|---|---|
| 1 | 0x40D24E | `SetPlayerVehicleIndex` 0x42BBD0 | stdcall | writes `dword_7C54E0` and `dword_7C54DC` (`functions/SetPlayerVehicleIndex_0042bbd0.c`) |
| 2 | 0x40D255 | `CameraTargetUpdateOnPlayerChange` 0x40C7A0 | thiscall | calls `AudioSoundEffectController`, sets `this[14]` (`functions/CameraTargetUpdateOnPlayerChange_0040c7a0.c`) |
| 3 | 0x40D25D | `CameraViewAssignment` 0x40C650 | thiscall | camera view; may call `ConditionalModeDispatchWithGlobalGate` 0x40C610 and `GameActionStateMachine` 0x40CDE0 |
| 4 | 0x40D264 | `VirtualDispatchConditionalAudio` 0x40D090 | thiscall | NOT audio: vtable+16 on `this[2]`, zero `dword_8AFE88+420/+424` |
| 5 | 0x40D26F | `AIOutOfTrackEvent` 0x41A0C0 | stdcall, 1 arg (a2) | THE LINE. Inserts eid 264 (see section 2) |
| 6 | 0x40D27D | `sub_4D8760(1,0)` 0x4D8760 | thiscall, ECX = 0x8AD298 | sets target via `VTableDispatchWithFieldClear` when allowed |

Callers of the chain (FACT, SDK links): `CameraTargetRaceLeader` 0x40D290 (numpad +/-), `VehicleSelectionCyclicShift` 0x40D2CA, `CameraTargetCurrentTrackedVehicle` 0x40D340 (numpad Enter), `sub_4A0250`, `sub_4DA840`. Our TV Director call is a seventh path. So the player's own numpad switches run the same line, which matches the owner's report.

Only one caller of `AIOutOfTrackEvent` exists in `functions/` (grep): the chain. FACT.

## 2. The gate, with code excerpts

`functions/AIOutOfTrackEvent_0041a0c0.c` (verbatim):

```c
void __stdcall sub_41A0C0(int a1)
{
  float v1[8];
  if ( dword_7D0308 && flt_7CF3C8 != 0.0 )
  {
    BooleanOROfTwoChecks(flt_5EA618, 264);
    if ( a1 != *(_DWORD *)(dword_7C54F0[dword_7C54E4] + 18580) )
    {
      StructInitWithSentinelValues(v1);
      DriverDataTableLookupAndTimeConversion(v1, 264, dword_5F0F74[4656 * a1], -1, -1, -1, -2.0, -2.0);
      InsertDriverTableEntry(flt_5EA618, v1);
    }
  }
}
```

Facts from the excerpt:
- Two gates: `dword_7D0308` (int, docs/12 "RaceActiveFlag", docs/56 calls it "commentary flag") and `flt_7CF3C8` (float, docs/12 "TrackConditionState", docs/56 "level").
- The line is not played in the call. It is inserted into the queue `flt_5EA618` (`InsertDriverTableEntry` 0x418560). Who consumes the queue and plays it is OPEN (phase-2 A10).
- Player-car test: `a1 != roster(dword_7C54F0[dword_7C54E4])`. Note `dword_7C54E4` is the CurrentVehicleIndex (docs/12); `SetPlayerVehicleIndex` in step 1 writes 0x7C54E0/0x7C54DC, not 0x7C54E4. Whether step 1 changes the player comparison in this function is OPEN (not needed for the suppression choice).
- Live state (phase-2 A4, `web_telemetry.c:1006`): raw 0x7D0308 reads 3 on track, never 1. Writing 1 back would be wrong; restore the saved value.

`functions/VirtualDispatchConditionalAudio_0040d090.c` (verbatim, for the record):

```c
int __thiscall sub_40D090(_DWORD *this)
{
  v1 = this[2];
  if ( v1 ) result = (*(int (__thiscall **)(int))(*(_DWORD *)v1 + 16))(v1);
  v3 = dword_8AFE88;
  if ( dword_8AFE88 ) { result = 0; *(_DWORD *)(dword_8AFE88 + 424) = 0; *(_DWORD *)(v3 + 420) = 0; }
  return result;
}
```

This does not gate audio. `dword_8AFE88` is labelled "CollisionGeometryHandle" by docs/12 (DERIVED). The vtable+16 target is unnamed; OPEN. Skipping this step is NOT needed for suppression.

Other readers of the gate on the chain path (grep of `functions/` for `7d0308` / `7cf3c8`): none of steps 1, 2, 3 (or their direct callees shown), 4, or 6 read the two globals directly. Indirect reads through vtable calls (step 4 vtable+16, step 6 `VTableDispatchWithFieldClear`) are not traced. OPEN.

## 3. Ranked suppression options

### (a) RECOMMENDED: save, zero, call, restore `dword_7D0308` around our own call

Mechanism: on the game thread, in the same broker task as our chain call: read `old = dword_7D0308`; write 0; call 0x40D240; write `old` back. The gate in step 5 then fails, so no entry is inserted.

- Does not change the player's own switches: the window contains only our call. Numpad paths do not run inside it.
- Other commentary: nothing else runs inside the window except the chain and its callees (static list above). Lines queued before the window still play. Nothing is queued by the window, so there is nothing to flush later. The game thread is single, so no other code sees 0 during the window.
- Restore must be exact (value 3 in race, not 1). Restore even if the chain path returns early.
- Risk: indirect reads through the unnamed vtable calls (step 4 and step 6) are not traced. Live check: a switch to an AI car with the window must produce no line, and a switch with the window off must still produce it.
- Alternative: zero `flt_7CF3C8` (float) instead. Same window logic and same read sites. Prefer `dword_7D0308` because docs/56 names it the commentary flag, but the docs/12 name "RaceActiveFlag" conflicts. The name must be settled before implementation (OPEN).

### (b) Skip the call through a mod-side wrapper

Mechanism: do not call 0x40D240. Reproduce steps 1, 2, 3 and 6 from the mod, leaving out steps 4 and 5.

- Cost: the ABI must be exact for each step (stdcall for step 1; thiscall for 2, 3, 6 with the ECX values above). A mistake here moves the camera wrong or corrupts state.
- Side effects: skipping step 4 drops the vtable+16 call and the `dword_8AFE88 +420/+424` zeroing. Effect unknown (OPEN). Step 4 is not called by any other path except `sub_4A00A0` (`functions/sub_4A00A0`), so its loss is not proven harmless.
- Also replicates the chain's ordering, so a future chain change in the game would be missed.
- Rank: second. Use only if (a) is rejected for the flag-window reason or fails live.

### (c) Byte patch (least preferred)

Mechanism: NOP the call to `AIOutOfTrackEvent` in the chain (site 0x40D26F).

- Affects ALL car switches, including the owner's numpad switches. That may be acceptable, but the owner asked for TV Director only.
- Trap: `AIOutOfTrackEvent` is stdcall with one pushed argument. NOPing only the `call` leaves the argument on the stack. The push must be removed with it, or the stack is corrupted. Check the bytes before any patch (not done here; the bytes in the SDK finding are from a prior read).

## 4. Side-effect summary

| Option | Silences other commentary? | Affects player's own camera change? | Main risk |
|---|---|---|---|
| (a) flag window | No, only lines inside our call (none queued) | No (window covers our call only) | Indirect flag reads via vtables; name conflict on 0x7D0308 |
| (b) wrapper | No | No | ABI mistakes; loss of step 4 side effects |
| (c) byte patch | Only the AI-switch line, for every switch | Yes (line removed for numpad too) | Stack imbalance; protected code |

## 5. Open items

- OPEN: the spoken text of eid 264. docs/56 row 264 says `PWS_MISC16.mp2`, "Switch camera to AI driver", source `browser`, cooldown 0, not blocked. It is not from EVF-008. The owner hears "riding with X". The owner's clip-to-text mapping does not match this row. Do not assume eid 264 is the "riding with" line until heard in isolation (docs/56 rule: never guess an eid from a clip name).
- OPEN: what consumes the queue `flt_5EA618` and whether it plays the line (phase-2 A10).
- OPEN: the meaning of unnamed vtable calls in steps 4 and 6.
- OPEN: naming conflict on `dword_7D0308` (docs/12 "RaceActiveFlag" vs docs/56 "commentary flag").
- OPEN: the step-1 effect on the player comparison in `AIOutOfTrackEvent`.

## 6. Validation plan (for implementation, not run here)

1. Live, owner-run: with the window in place, switch to 3 AI cars and to the player car; expect no "riding with" for the AI switches while the window is in place, and the line still plays for numpad switches.
2. Check the camera ends on the target car and the angle is unchanged (the camera work of steps 2, 3 and 6 still runs).
3. Confirm the value of `dword_7D0308` after restore equals the value before (read back in a log).
4. Confirm no change in race data (positions, timing) by the existing TV Director T2 checks.

## 7. Questions for the owner

- Which line do you hear as "riding with X": is it eid 264 (`PWS_MISC16.mp2`) or another eid? Isolate it once in a test before we commit to the gate.
- Do you accept the flag window (option a) as the default?

## 8. Files read

`functions/GameStateDispatchChain_0040d240.c`, `VirtualDispatchConditionalAudio_0040d090.c`, `AIOutOfTrackEvent_0041a0c0.c`, `CameraViewAssignment_0040c650.c`, `CameraTargetUpdateOnPlayerChange_0040c7a0.c`, `SetPlayerVehicleIndex_0042bbd0.c`, `sub_4D8760_004d8760.c`, `BooleanOROfTwoChecks_00418a20.c`, `InsertDriverTableEntry_00418560.c`, `DriverDataTableLookupAndTimeConversion_004144f0.c`, `StructInitWithSentinelValues_004144d0.c`, `ConditionalModeDispatchWithGlobalGate_0040c610.c`, `AudioSoundEffectController_004e0ed0.c`; `docs/56-voice-line-reference-map.md`; `tools/GAME_CONTROL.md` (sections 2 to 4); `docs/12-globals-reference.md` (grep); `findings.jsonl` (SDK queries and last lines). Game EXE not run; no bytes re-read in this pass beyond the SDK finding.
