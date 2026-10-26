# H7 - Live AI strength via sub_4F1510 (static spec), 2026-10-15

Agent: Haiku H7 (static only). Owner question: can a DLL change one AI car's physical strength mid-race by calling the game's own scaling function (0x4F1510, called "ScaleAudioPitch" by the SDK label)?
Scope: read-only corpus + read-only disassembly of `F1 Challenge V10\F1 (1).exe` via capstone (scripts in %TEMP%, not in the corpus). No game process, poke, memory MCP or live tool was used. Only this file was written. No `findings.jsonl` append (read-only rule); proposed entries are in section 9.

Labels: FACT = code/bytes show it (cited). DERIVED = reasoned from FACTs. OPEN = not resolved. CONFLICT = sources disagree.

## 0. Verdict

- Call target is real and self-contained: `sub_4F1510(P, a2)`, `__thiscall`, ECX = P, one float argument, callee-cleans (`ret 4`). FACT.
- P is NOT a pointer field. P = vehicle base + 0xC, an embedded sub-object. FACT (asm `lea ecx,[esi+0xc]`).
- Round trip is exact: call with `a2 = B*m`, then with the original `B`, restores the state (composition proof, section 4). DERIVED from the code; float rounding only.
- BLOCKER for a naive mid-race call: the game re-runs the same routine with the slider value at several event points (driver restart, driver entry, pit decision, tyre change path, AI command). A DLL-set value can be overwritten at the next such event. OPEN frequency; see section 6.
- Side effects: bit 4 of `byte_7D01C4` (default 7) scales fuel consumption factor; bits 1/2 scale torque and gear tables. FACT.

## 1. Cold start

- Read: `AGENTS.md`, `README.md`, `governance\README.md` (head), adaptive-AI README and sibling H2/H5 reports. Not read: `PIPELINE.md`, `PRODUCT_STATUS.json` (OPEN for owner).
- Validator: `tools\workspace\check-workspace-structure.ps1`: 2 pre-existing errors (`.claude`, `plans`), same as H2. Not changed here.
- Stage: investigation only (README_2026-10-14). No stage change proposed.

## 2. Identity of `this` (question 1)

| Fact | Evidence |
|---|---|
| AIDriverStructInit's `this` (ECX on entry) is stored in ESI | asm `0x424798: mov esi, ecx` |
| Its call to the scaler passes `esi+0xC` in ECX | asm `0x424aa3: lea ecx,[esi+0xc]`, `0x424aa6: call 0x4f1510`, no caller cleanup |
| AIDriverStructInit's `this` is the vehicle base | fields it writes are the vehicle's own: RetiredFlag at +0x4410 (docs/38 line 56, "0x48C0 struct"), `*(this+4)` vehicle ID, `this+0x48A4` nodeIdx (docs/38 line 180). `docs/54` line 7 says `this` = vehicle base |
| Vehicle stride 0x48C0, base array 0x5EC6D8 | `tools/sdk/f1c_sdk.h:9-13` (`VEH_BASE`, `VEH(slot)`), `:280` `G_VehicleArrayBase`, `:284` `G_VehiclePtrArray` 0x7C54F0 |
| The pointer array holds vehicle bases | `functions/AIDriverStructInit_00424790.c` line ~111 compares `dword_7C54F0[k]` with `this`; `functions/DynMan_Restart_0042c3b0.c` sets `dword_7C54E0 = dword_7C54F0[dword_7C54E4]` (player/focus vehicle) |
| Other scaler-style calls use the same +0xC convention | `functions/SceneRenderUpdate_004c6d90.c`: `LargeConfigFunctionWithPhysicsConstants(this + 3)` (int* arithmetic = +0xC), asm call `0x4c6dad` |

So:

- **P(slot k) = VehiclePtrArray[k] + 0xC = 0x5EC6D8 + k*0x48C0 + 0xC** (the two forms agree if the array is in slot order; verify at runtime, section 8).
- There is no pointer field at 0x48C0 for P. "0x48C0" is the vehicle stride, not a slot inside the struct. FACT.
- The player car has the same F1CVehicle type and stride. Same P recipe. FACT (same array, `dword_7C54E0` is an element of it).
- Whether the player is one of the cars that gets AIDriverStructInit: DERIVED YES, OPEN in fact. DynMan_Restart (`functions/DynMan_Restart_0042c3b0.c:14-16`) loops `SceneRenderUpdate(1)` over all `dword_5C329C` cars, and AIDriverStructInit has no human-control test (grep: only guards at lines 82, 95, 105, 129; no read of +0xC4 `isHumanInControl`). So the player car is probably scaled too. Owner should confirm before any "AI only" claim.

## 3. Field map of sub_4F1510 (question 2)

Frame note (CONFLICT resolved): the SDK header (`tools/sdk/f1c_vehicle_struct.h`, `f1c_sdk.h`) labels offsets against the vehicle base. The decompiled offsets in sub_4F1510 are relative to P. Real vehicle offset = P-offset + 0xC. Example: the header calls veh+0x3984 `FuelLevel` (CONFIRMED), but sub_4F1510's `P+14724` is veh+0x3990, which the header calls `fuelConsumptionFactor` (CONFIRMED). Using the header's names unshifted gives the wrong field.

Cross-check that proves the frame (FACT): sub_4F18C0 writes `1.0f` at P+13872 (`functions/sub_4F18C0_004f18c0.c:35`, value 1065353216). P+13872 = veh+0x363C = `tirePuncture_RR` (header line 81, CONFIRMED). Stride 248 gives RL 0x3734, FR 0x382C, FL 0x3924, matching header lines 83/85/87. Wheel index k: 0=RR, 1=RL, 2=FR, 3=FL (DERIVED from the header labels plus the stride).

| P-offset | Vehicle offset | Name (best) | Status | Evidence |
|---|---|---|---|---|
| +13852 + 248k (k=0..3) | 0x3628 + 0xF8k | Per-wheel term W+0 = strength-scaled. W+0 = W+4 * B (B = P+15348). sub_4F1510 multiplies W+0 by v14 | FACT for the write; name OPEN ("grip/load" unproven) | `sub_4F18C0.c:38-40`; `ScaleAudioPitch_004f1510.c:17-25` |
| +13856 (W+4) | veh+0x362C | Raw table value: 0.5 * v9[32] * (v9[0]+v9[1]) (not rescaled by sub_4F1510) | FACT write; name OPEN | `sub_4F18C0.c:33, 38` |
| +13840 (W-12) | | 0.98*(50/v9[4]*v9[5]+v9[4]); read per wheel by the solver at P+13840, +14088, +14336, +14584 | FACT read | `TireBrushModelForceSolver_004e8010.c:267-269` |
| +13872 (W+20) | veh+0x363C | tirePuncture (set 1.0 at init) | CONFIRMED (header) + FACT write | `sub_4F18C0.c:35`; header line 81 |
| +14756 | veh+0x39B0 | Heap pointer to (x,y) float pairs, 8-byte stride, count at +14768. Read by lateral-force code with linear interpolation at index int(v22/32), where v22 = (speed/radius) * gear ratio. Best name: engine torque/power curve | Pointer FACT (allocated `SuspensionAndWheelPhysics_004e5f60.c:622-623`); reader FACT (`TireGripLateralForceCalculation_004e9300.c:130`); meaning DERIVED | `ScaleAudioPitch` bit 1 (lines 28-36) |
| +14768 | veh+0x39BC | Count of the pair table (2 - floor(-14744/32)) | FACT write `:622` | |
| +14800 (block; index 9 at +918 floats) | veh+0x39DC | Per-gear ratio table (indices 1..8 consecutive, index 9 after a 918-float jump). Read as `v22 = a2/radius * ratio` | FACT reader `TireGripLateral:107`; name DERIVED ("gear ratios") | `ScaleAudioPitch` bit 2 (lines 38-61) |
| +14840 | veh+0x3A04 | Count (last index) of the ratio block | FACT readers `TireGripLateral:102`, `SuspensionAndWheel:643, 668` | |
| +14744 | veh+0x39A4 | Upper bound on the lateral-grip index (`v22 < P+14744` test). Matches header `EngineMaxRPM` (veh+0x39A4, CONFIRMED, header line 96) after the +0xC shift | FACT reader `TireGripLateral:114`; name DERIVED from the frame shift | |
| +14724 | veh+0x3990 | fuelConsumptionFactor (header CONFIRMED label) = dword_7D0B64 * a3[1652] * flt_7D01E8 (AI Additional Fuel Mult). Scaled by v14 in bit 4 | FACT writer `SuspensionAndWheelPhysics_004e5f60.c:562`; header line 93 | `ScaleAudioPitch` bit 4 (lines 63-67) |
| +15348 | veh+0x3C00 | Strength baseline B: scale reference, stored a2 of the last call. sub_4F1510 writes a2 at its end (line 68) | FACT. Writers: `SuspensionAndWheelPhysics_004e5f60.c:293` (1.0), `PhysicsParameterInitialization_004af3a0.c:183, 343, 503` (0, 0.2, 0.5 then FloatClamp), sub_4F1510 line 68. Readers: `sub_4F18C0.c:40`, `TireBrushModelForceSolver_004e8010.c:304` (1/B) | Must be finite and > 0 |

Conflicts found (not resolved here):

- CONFLICT: `tools/sdk/f1c_sdk.h:8143` labels veh+0x39A4 as `VEHICLE_JitterBuffer0` (void*), and `:8121` labels veh+0x361C as a `float**` PitchScale. Both are read as pointers in the SDK, but sub_4F1510 uses the P-frame, where P+14744 = veh+0x39A4 is a float bound and P+13852 = veh+0x3628 is inline floats. The SDK names for these two offsets are unreliable. Do not reuse them.
- CONFLICT: `skeletons_filled/ScaleAudioPitch_004f1510.md` names veh-frame offsets (0x3984 pitch scale, 0x39A4 EngineMaxRPM, 0x39B0 engine RPM) and calls the routine "pitch". Replaced by section 3. The skeleton's "per-frame" call frequency for AIDriverStructInit is wrong (section 6).
- CONFLICT: H2 says `this+14724` is the "fuel-weight scalar". Correct under P-frame (fuel consumption factor). The veh-frame readers `AIMainUpdate_00424e70.c:241`, `sub_426430:196,234,257,301`, `FuelWeightLimitCheck_0042ad50.c:4` read veh+0x3984 = FuelLevel, a different field. Do not conflate.

## 4. Math of the call (the recipe's basis)

Let v14 = a2 / B (B = P+15348 before the call). Per call:

- W+0 (k=0..3): x -> x * v14.
- Bit 1 (byte_7D01C4 & 1): each pair (x,y) in the 14756 table -> (x, (x+y)*v14 - x). First element unchanged.
- Bit 2 (& 2): each ratio in the 14800 block -> ratio / v14.
- Bit 4 (& 4): fuelConsumptionFactor (P+14724) -> x * v14.
- B := a2. Then `TireBrushModelForceSolver(P, 0)` (line 69) recomputes forces.

Composition (FACT algebra, checked by hand):
- Bit 1 map f_r(x,y) = (x, (x+y)r - x); f_{r2}(f_{r1}(x,y)) = f_{r1 r2}, so calls compose multiplicatively.
- Restore: call with a2 = B_before (the exact restore value).
- Per-wheel W+0 = W+4 * B stays consistent (sub_4F18C0 re-derives from the same B).

## 5. Are these values read from init-only caches? (question 3)

Readers of each field (all FACT unless marked):

- Solver (`TireBrushModelForceSolver_004e8010.c`): reads per-wheel P+13840 (lines 267-269), P+13868 (line 102) and 1/B (line 304). Recomputed at the end of each sub_4F1510 call. Live.
- Lateral grip (`TireGripLateralForceCalculation_004e9300.c`): reads the 14756 table, 14800 block, 14840 count per evaluation. Live.
- AI planner (`AIRacingLine_00425920.c`): no static or cached arrays (grep). Live.
- sub_4F18C0 via `LoopWithConditionalSubCall` (`sub_4F1A60`, lines 5-8, tyre-change callers): rebuilds W+0 from W+4*B. Consistent with a changed B.

Init-derived values that will NOT follow a mid-race call (FACT, these are the cache risks):

- `sub_424080_00424080.c:50, 108, 151-154`: per-driver strength-derived values (this+4401 from strength via `sub_58850F`, and this+4405 from it). Called from inside AIDriverStructInit at asm `0x42493a`, so re-derived on each re-application, but from the slider, not from the DLL's value. Not updated by a physics-only call.
- The AI "ability" path reads the slider live (AIDifficultyScalar 0x424750, `AIAbility_00429470.c:31-35, 76`, steering `AISteeringCorrection_00426cf0.c:23`). A physics call does not change these. So a physics-only change is not the whole "strength".
- The 0x44F4 pace (RA x GDS) is set at setup (docs/41, H1). Not touched by sub_4F1510. Not changed by this recipe.
- Tyre compound change re-runs sub_4F1A60 -> sub_4F18C0 x4 -> solver (`LoopWithConditionalSubCall_004f1a60.c:5-8`). That rebuilds W+0 from the current B, which is consistent after our call.

Verdict on caches: no cache found that would silently keep the OLD physics after a call. The risk is the opposite: the game re-applies the slider (section 6), and the AI ability/pace paths follow the slider, not the DLL value.

## 6. Re-application by the game (the main risk)

sub_4C6D90 (SceneRenderUpdate) calls AIDriverStructInit (asm `0x4c6e38`), which calls sub_4F1510(P, v19) with v19 from the slider (`AIDriverStructInit_00424790.c:127-131`). A byte scan of the exe for E8 call instructions finds 7 direct call sites of 0x4C6D90 (H2 reported 8; the difference is OPEN, possibly a non-E8 form):

| Caller (corpus file) | Asm site | Trigger (from the caller code) |
|---|---|---|
| AICommandMessageQueueDispatcher_00424cd0.c:34 | 0x424d6d | AI command queue message (gate not fully traced) |
| PitStopDecisionTireCompound_0042a3d0.c:66 | 0x42a536 | Pit decision (gated: `PitStopBlockingCondition || dword_7C56A4 != 7`) |
| DynMan_Restart_0042c3b0.c:16 | 0x42c409 | Restart: loop over all cars with SceneRenderUpdate(1) |
| NetCommPlayerSlotSync_0045fc50.c:104 | 0x45fe4e | Network slot sync (per car) |
| sub_4A00A0_004a00a0.c:40 | 0x4a01cd | Session setup |
| sub_4A0250_004a0250.c:44 | 0x4a0360 | Session setup |
| ProcessDriverEntries_004b8da0.c:128 | 0x4b8f74 | Driver entry (per car) |

So AIDriverStructInit is EVENT-driven, not a per-frame tick. The skeleton's "Call frequency: per-frame tick" is contradicted by these callers (CONFLICT; treat the skeleton label as wrong). Frequency of pit decisions and AI-command messages during a race: OPEN.

Effect of a re-application (DERIVED): the game sets B := v19 (slider). Net arrays = orig * (B_mine/B_orig) * (v19/B_mine) = orig * v19/B_orig, i.e., the game's own slider state. The DLL's change is undone at that event.

Options (owner decision): (1) poll B each frame and re-call when B != target; (2) hook the tail call at `0x424aa6` and substitute a2 for the target slot (cleanest, needs owner approval); (3) change slider `dword_7D01A0` (not per car; also moves AI ability and steering).

## 7. Call recipe (exact)

Preconditions:

- 32-bit process (the game is 32-bit PE; calling convention is x86 __thiscall).
- Slot k is a live car: `N = dword_5C329C`, 0 <= k < N. Read `veh = dword_7C54F0[k]` (or `0x5EC6D8 + k*0x48C0` after checking they agree).
- `P = veh + 0xC`.
- `B = *(float*)(P + 15348)` is finite and > 0. Store B0 = B before the first change (this is the restore value).
- Call from the sim/main thread at a frame boundary, not inside AI or physics code. (Threading: OPEN; the game's event callers are on the same path, so main thread is the default assumption.)
- `byte_7D01C4` (AI Power Calibration bitfield, default 7) is read by the routine itself; do not change it in the DLL unless intended.

Call (C, MSVC 32-bit):

    typedef void (__thiscall *ScaleFn)(void *self, float a2);
    ScaleFn scale = (ScaleFn)0x004F1510;
    scale((void*)P, a2_new);    // ECX=P, one float on stack, callee cleans 4 bytes

a2 choices:

- Ratio-only: a2_new = B0 * m. Set m = curve(S_new) / curve(S_old) if you want the game's curve (curve from `AIDifficultyInterpolation_00424ac0.c:5-9`; x0.995 applied unless `dword_7C56A4` is 3, 4 or 5, `AIDriverStructInit_00424790.c:129-130`).
- Exact game-equivalent for strength S: a2 = curve(S) * 0.995 (or without 0.995 for session types 3/4/5). Requires the track constants of `AIDifficultyInterpolation` (not reproduced here).

Restore: `scale((void*)P, B0);` Exact by section 4. Float rounding only.

Side effects of every call (FACT):

- Bit 1 torque table, bit 2 gear ratios, bit 4 fuel consumption factor, and the per-wheel W+0 all change.
- `TireBrushModelForceSolver(P,0)` runs inside (line 69). Forces on that car change at the next physics step.

Do NOT call without a valid B (division by zero, `ScaleAudioPitch_004f1510.c:17`).

## 8. Test plan (static-to-live, not run here)

Run only on the clean test install (`%USERPROFILE%\Documents\VM\F1 Challenge V10_clean`, Q19) with memory-MCP freshness rules. Never on the live game as part of this spec.

1. Frame check: for each slot k, `P = veh + 0xC`. Read `P+13872` for k=0..3 (wheel tirePuncture): expect 1.0 (no puncture). Read `veh+0x363C`, `0x3734`, `0x382C`, `0x3924`: same values. Confirms the P-frame.
2. Baseline check: `P+15348` equals the AI slider curve x 0.995 at init for the car's strength (or 1.0 before AIDriverStructInit ran). Expect a match within 1e-4 for AI cars.
3. Fuel canary: after a call with a2 = 2*B0 (bits 1,2,4 on), `veh+0x3990` (fuelConsumptionFactor) changes by ratio 2 (or 1/2 for bit 2 ratios); `veh+0x3984` (FuelLevel) does NOT change. Any FuelLevel change refutes the frame.
4. Round trip: hash P+13852 (4 floats x 4 wheels), the 14756 pair table (count P+14768), the 14800 block (count P+14840). Call with 1.5*B0, then B0. Compare hashes. Expect equality within float tolerance (relative 1e-5). Exact bit equality is not guaranteed.
5. Reversion: after a DLL change, trigger a pit decision for that car or a driver restart. Log B. Expect B to return to the slider value (section 6). If B stays, the re-application is not reached in that path.
6. Player check: confirm whether the player car (`dword_7C54E0`) gets B = slider after a restart. Settles the open question in section 2.
7. Behaviour: lap times via memory-MCP with the pause/freshness check (identical samples = paused).

## 9. Proposed findings.jsonl entries (not appended; read-only task)

- FACT: sub_4F1510 `this` = vehicle base + 0xC (asm 0x424aa3); P-frame offsets = vehicle offset - 0xC; sub_4F18C0 writes 1.0 at P+13872 = tirePuncture_RR veh+0x363C.
- DERIVED: P+14724 = veh+0x3990 fuelConsumptionFactor, not FuelLevel (veh+0x3984); H2's "fuel-weight scalar" is the P-frame name.
- FACT: AIDriverStructInit is event-driven (7 E8 call sites, section 6); skeleton "per-frame" label is wrong.
- DERIVED: game re-applies the slider via sub_4F1510 at those events, so a DLL-only mid-race call is reverted.
- FACT (algebra): bit-1 map composes as f_{r1 r2}; restore by calling with the original B is exact.
- OPEN: player car included in AIDriverStructInit (no human gate found).

## 10. Open items

1. Confirm the frame (test 1) and fuel side effect (test 3) on the clean test install.
2. Trace the pit-decision gate (`PitStopBlockingCondition`) for re-application frequency.
3. Confirm the player car is in `dword_5C329C` and gets AIDriverStructInit.
4. Name W+4 (13856) and per-wheel semantics (grip vs load) from compound fields v9[0..5], v9[30..34].
5. Confirm the sim thread that runs the AI/physics step.
6. Owner decision on the re-application strategy (section 6).

Documentation gate: this file is the durable record. Not added to a README index (read-only scope). Pending validation: tests 1-6. No code, game or settings changed.
