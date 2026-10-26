# H20 - Re-application of the sub_4F1510 strength call: triggers, player coverage, caches, mod options (2026-10-15)

Agent: Haiku H20 (static only, read-only). Only this file was written. Scratch scripts in `%TEMP%\h20` (not in the corpus). No `findings.jsonl` append (read-only scope); proposed entries in section 9. No game process, memory tool or live read was used.

Labels: FACT = code or asm shown and cited. DERIVED = reasoned from FACTs. OPEN = not resolved here. Paths are relative to `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\` (the corpus root) unless absolute.

Asm source: `%USERPROFILE%\Documents\VM\F1 Challenge V10_clean\F1 Challenge Online.exe` (clean test copy, file offset = VA - 0x400000). The bytes at file offset 0x5B20F match the decompiled WinMain store `mov [0x7C56B0], ...`, so this binary is the decompiled build. Disassembly by capstone 5.0.7.

Cold start: AGENTS.md (system context), `README.md` (head), `governance\PRODUCT_STATUS.json` (head), H7 report read in full. `governance\PIPELINE.md` and full `governance\README.md` were not read in this pass (partial cold start, noted). Validator `tools\workspace\check-workspace-structure.ps1` run: 2 errors (`.claude`, `plans`), same as H7; nothing changed. Stage: investigation only.

## 0. Verdict

- FACT: AIDriverStructInit (0x424790) has exactly one caller, SceneRenderUpdate (0x4C6D90), at `SceneRenderUpdate_004c6d90.c:19`. SceneRenderUpdate has 7 callers (callgraph.json). Re-application is event-driven, not per frame.
- FACT: sub_4F1510 has exactly one caller, AIDriverStructInit (`AIDriverStructInit_00424790.c:128-131`; asm 0x424AA6). AIDifficultyInterpolation (0x424AC0) has the same single caller.
- FACT: AIDriverStructInit has no human/AI test before the scale call. The player therefore gets the same value as AI cars: `curve(dword_7D01A0)`, times 0.995 outside session types 3/4/5. DERIVED from code; live confirmation OPEN.
- FACT: every SceneRenderUpdate first re-runs LargeConfig (`SceneRenderUpdate_004c6d90.c:7`), which runs SuspensionAndWheelPhysics, which sets B = P+15348 = 1.0 (`SuspensionAndWheelPhysics_004e5f60.c:293`). The game's strength is therefore absolute per event (baseline x slider), and a DLL-only ratio on a car is wiped at that car's next SceneRenderUpdate.
- FACT: two session-side routes call SceneRenderUpdate(0) with ECX = `dword_7C54E8` = PlayerVehiclePtr (`tools/sdk/f1c_sdk.h:283`, FACT). So the player's car is re-initialized by those routes (section 2, R2/R3).
- Recommendation (section 6): hook the single call site 0x424AA6 and substitute a per-car value; optionally add one immediate sub_4F1510 call for a mid-race change.

## 1. Corrections to H7

1. "Tyre change path re-runs sub_4F1510": not correct. `LoopWithConditionalSubCall_004f1a60.c:5-8` calls sub_4F18C0 four times and the solver, never sub_4F1510. It is per-frame, called from AIMainUpdate, sub_429960, AISteeringRacingLineCorrection, PitStopOperationTimer and SuspensionAndWheelPhysics (callgraph.json). It consumes B; it is not a re-init event.
2. H7 section 6 listed the AI command and pit decision as generic triggers. Both are AI-only and gated (section 2, R4/R5).
3. H7 did not identify that SuspensionAndWheelPhysics resets B = 1.0 at every SceneRenderUpdate before the game's own scale (`SuspensionAndWheelPhysics_004e5f60.c:293`, reached via `SceneRenderUpdate_004c6d90.c:7`). This makes each re-init absolute (section 5).
4. H7 "driver swap": no dedicated route found (OPEN).

## 2. Question 1 - every route to AIDriverStructInit (0x424790)

Entry path (FACT). GameMainEntry (`GameMainEntry_0045b200.c:19-28`) loops: asm 0x45B2A1 calls Game_Restart (0x42D7A0); asm 0x45B2B4 calls GamePostMainLoop (0x42D8A0); asm 0x45B2BB `je 0x45B2A1` re-enters Game_Restart when GamePostMainLoop returns 0. Game_Restart calls DynMan_Restart (`Game_Restart_0042d7a0.c:15`), which calls SceneRenderUpdate(1) for every car (`DynMan_Restart_0042c3b0.c:12-19`).

GamePostMainLoop returns 0 only at `GamePostMainLoop_0042d8a0.c:49-50` (SessionStateCheck == 1 after a frame) or `:52-53` (RestartGate returned 4). It returns 1 at `:56`, which sends the loop through Game_Post, Game_Setup and Game_Init (asm 0x45B2C2-0x45B2D3). Game_Init calls AIDatabase_Init (FACT, callgraph).

| # | Route to SceneRenderUpdate (arg; ECX) | Gate (file:line) | Trigger | Normal race, no player action? |
|---|---|---|---|---|
| R1 | DynMan_Restart -> SRU(1); all cars | `Game_Restart_0042d7a0.c:15`; entered when GamePostMainLoop returns 0 | Session restart: RestartGate returns 4, or SessionStateCheck == 1 | OPEN. RestartGate 4 needs byte_7CE25D (`RestartGate_004a0fd0.c:11-24`), set only by GlobalFlagSetter (`GlobalFlagSetter_004a0da0.c:3`). Its callers: `SessionSynchronizer_0045f950.c:110` (MP session-type change, `:104-110`), `sub_4A0250_004a0250.c:34`, `sub_4D4580_004d4580.c:113-114` (steward states 5-8; trigger OPEN). |
| R2 | sub_4A0250 -> SRU(0); ECX = PlayerVehiclePtr (asm 0x4A0358 `mov ecx,[0x7c54e8]`) | `sub_4A0250_004a0250.c:41-44` (ScoringGateCheck = `!dword_7D2E44 && SessionStateCheck != 1`, `ScoringGateCheck_0042bc10.c:3`) | Callers: MainMenuTransition, SessionSynchronizer (`:108`), ConditionalInitGuard (via RestartGate `:59-62`) | Player-facing, see below. |
| R2a | ConditionalInitGuard -> sub_4A0250 | `ConditionalInitGuard_004a0cf0.c:6-9` (runs if byte_7CE254 == 0; clears dword_7CE258) | RestartGate calls it whenever dword_7CE258 == 1 (`RestartGate_004a0fd0.c:53-62`) | Yes for the chain, once the flag is set. |
| R2b | Writers of dword_7CE258 = 1 (the flag that starts R2a) | `PitStateUpdateMachine_004a6e50.c:13-19` (human car, veh+13000 == 0, !VehicleControlEnabled, pit state 2 with pit-box proximity); `RestartGate_004a0fd0.c:51, 56, 81`; `sub_4F1FE0_004f1fe0.c:147-148` (per-frame, under `!this[3247]`, from VehicleYawRotationRecoveryController) | Human car out of control (VehicleControlEnabled false: FinishState 1 or 3, or a zero health field, `VehicleControlEnabled_004a6db0.c:9-21`) while in pit proximity; or the sub_4F1FE0 recovery branch | OPEN. The human-car branch is FACT for code; the exact frequency in a normal race is not established. R2b-sub_4F1FE0 is OPEN. |
| R3 | sub_4A00A0 -> SRU(0); ECX = PlayerVehiclePtr (asm 0x4A01C5) | `sub_4A00A0_004a00a0.c:37-40` (ScoringGateCheck) | sub_485B90 (`RestartGate:41`, `Options_Setup`, `AutoJoinRetryLoop`) | Session setup/teardown; not normal mid-race (DERIVED). |
| R4 | PitStopDecisionTireCompound -> SRU(0); ECX = this (the AI car, `[esi]`, asm 0x42A52D) | `PitStopDecisionTireCompound_0042a3d0.c:63-66` (PitStopBlockingCondition OR session != 7). Called only from PitStopReadyCheck (`sub_4A6200.c:21-24`, gated veh+13000 == 1, AI-only) and from AIAbility (`AIAbility_00429470.c:115-123`, not in race) | AI car pit-ready (PitStateUpdateMachine states 2/3 -> PitStopReadyCheck, `sub_4A6E50.c:41, 48`) | Plausible yes for AI pit approach. PitStopBlockingCondition in race (`PitStopBlockingCondition_0042aee0.c:3-8`) is true when lap >= total, dword_7C56A8 == 8, veh+14842, or veh+3352 set with veh+3416 clear. The meaning of 3352/3416 is OPEN. If a normal pit request sets 3352, every AI pit request re-inits that car. |
| R5 | AICommand 15 -> SRU(0) for the car | `PitStopProgress_004a68b0.c:76-79` (veh+13000 == 1, AI-only; pit-exit branch from `:61`), then `AICommandMessageQueueDispatcher_00424cd0.c:23-34`: in race it renders only if veh+0x4064 FinishState != 0 | AI pit-exit completion | Not during a normal racing pit (DERIVED: FinishState is 0 while racing; FinishState at 0x4064 per ask.py/docs38). Yes for AI cars that have finished or retired. |
| R6 | ProcessDriverEntries -> SRU(1) | `ProcessDriverEntries_004b8da0.c:120-128`, via `ConditionalMultiPathDispatcher_004ba180.c:12-13, 31-33` (state 2 and a changed float) | Driver entry load | No for a single-player race (DERIVED). MP joins OPEN. |
| R7 | NetCommPlayerSlotSync -> SRU(1) | `NetCommPlayerSlotSync_0045fc50.c:101-104`; caller SessionScoringUpdate | Network slot message | Network-only (FACT, call chain). |
| R8 | AIDriverStructInit's own AICommand(0,0) (`AIDriverStructInit_00424790.c:99`) | Not a render: only case 15 renders | n/a | No. |

Not triggers (FACT): TrackTemperatureDisplay sends commands 4 and 5 (no render). sub_426430 sends 14 (`sub_426430_00426430.c:134`, no render). TimerFrameRateThrottle sends `v4+1` where v4 = FlagPropertyGetter(2048) (`TimerFrameRateThrottle_004d3740.c:8-9`); if that gives 14, case 15 can fire (OPEN). Replay and ESC-to-pits: no route traced (OPEN).

Summary for Q1 (DERIVED): inside a normal single-player race with no player action, the re-init routes that can fire are the AI pit routes R4 (possibly, depending on pit flags) and R5 (only for finished or retired AI cars). Routes that re-init the player's car (R2/R2a/R2b) need the human-car out-of-control state or the sub_4F1FE0 branch. Session restarts (R1) and setup (R3, R6, R7) are session-level.

## 3. Question 2 - does the scale reach the player, and what value?

- FACT: between the entry of AIDriverStructInit and the call at 0x424AA6 there is no human/AI test. The only branch tests are `LOBYTE(a2)` (`AIDriverStructInit_00424790.c:82`), the name compare at `:111` (not a human test), and the session type at `:129`.
- FACT: candidate human-control fields exist (SDK): veh+0xC4 isHumanInControl (`tools/sdk/f1c_sdk.h:31-32`), veh+0x45F4 AIControlFlag (`f1c_sdk.h:243-244`). Neither is read in AIDriverStructInit (readers of 0x45F4 in the corpus are AI functions).
- FACT, asm: 0x424A90 `fld [0x5ea3e8]` / 0x424A96 `fmul` (the 0.995 factor, skipped only for session types 3/4/5); 0x424AA3 `lea ecx,[esi+0xc]`; 0x424AA6 `call 0x4f1510`. The call is unconditional. ESI = vehicle base.
- FACT: the value is `AIDifficultyInterpolation()`, which reads only the global `dword_7D01A0` (`AIDifficultyInterpolation_00424ac0.c:1-10`). It takes no car argument. So the player gets the same value as every AI car: curve(slider), times 0.995 outside session types 3/4/5 (`AIDriverStructInit_00424790.c:129-130`).
- FACT (SDK, live note): veh+13000 (= 0x32C8, WearTickGate) reads 1 on all AI cars and 0 on the human (`ask.py offset 0x32C8`; `tools/sdk/f1c_sdk.h:63-64`; LIVE 2026-07-25, single session, investigations/engine_lifetime). It is the AI/human marker the pit gates use (`PitStopReadyCheck:21`, `PitStopProgress:76`, `PitStateUpdateMachine:13`). AIDriverStructInit does not use it.
- FACT: the LargeConfig gate on P+12988 (= veh+13000) accepts 0 to 3 (`LargeConfigFunctionWithPhysicsConstants_004e45b0.c:21-31`), so the physics rebuild (B = 1.0 and the baseline re-derivation) runs for the human too.
- DERIVED: the player's per-wheel terms (P+13852+248k), torque pairs (P+14756), gear block (P+14800) and fuel-consumption factor (P+14724) all change with the slider on every re-init, because the scale call is unconditional and sub_4F1510's bit flags (byte_7D01C4 = 7) apply to all cars.
- OPEN: live confirmation (H7 test 6). Note: "slot == 0" is not a human test. Use veh+13000 == 0 or PlayerVehiclePtr (dword_7C54E8).

## 4. Question 3 - other readers of P+15348 and the scaled fields; caches

- P+15348 = veh+0x3C00 = B. Writers: `SuspensionAndWheelPhysics_004e5f60.c:293` (= 1.0, straight-line code at the start of each SceneRenderUpdate rebuild); `PhysicsParameterInitialization_004af3a0.c:183, 343, 503` (0, 0.2f, 0.5f then FloatClamp; caller TrackEnvironmentSelect via TextFileReader, session-level); `ScaleAudioPitch_004f1510.c:68`. Readers: `sub_4F18C0_004f18c0.c:40` (W+0 = W+4 * B), `TireBrushModelForceSolver_004e8010.c:304` (1/B, each solver call), `ScaleAudioPitch_004f1510.c:17`.
- FACT: no static caches in files touching the scaled offsets (grep for `static` returns nothing relevant). The scaled fields are re-derived in SuspensionAndWheelPhysics on every SceneRenderUpdate: fuel factor `:562`, P+14744 `:591`, pair-table allocation `:622-623`, P+14840 `:643`. Per-call readers (TireBrushModelForceSolver P+13840/13868 at `:102, :267-269`; TireGripLateral P+14744/14756/14800/14840) read live values.
- DERIVED: no init-only cache keeps a stale value after a DLL call. The risk is re-derivation at the next event, which is H7 section 6.
- Fuel factor P+14724 (= veh+0x3990): P-frame readers found are only the writer (`:562`) and ScaleAudioPitch (`:66`). OPEN whether other P-frame aliases exist.
- Do not conflate with FuelLevel (veh+0x3984 = veh-frame 14724): `FuelWeightComputation_0049ba50.c:7` (dword_7C54E0 is the vehicle base), `AIMainUpdate_00424e70.c:241`, `sub_426430_00426430.c:196, 234, 257, 301`. This confirms H7's CONFLICT note.
- AI "ability" and steering use the slider live, not B: AIDifficultyScalar (`AIDifficultyScalar_00424750.c:1-6`; AIAbility `:31-35, 76`; sub_424080). A physics-only call does not change them.
- Anchors and RaceQualRatio are per-track data, not code constants (prior art, DERIVED/CONFIRMED, see section 4a). flt_5EA3DC/E0/E4 = AI database object 0x5EA2C0 +0x11C/+0x120/+0x124 (AIW WorstAdjust/MidAdjust/BestAdjust), and flt_5EA3E8 = +0x128 (AIW RaceQualRatio; code default 0.995 written by `FunctionCallWrapper_00401080.c:21`). All four are parsed by sub_402690 inside AIDatabase_Init (`AIDatabase_Init_004040a0.c:29-31, 53-63`), called from Game_Init (`Game_Init_0042d6b0.c`), which runs only on the full-restart path (asm 0x45B293, 0x45B2D3).
- DynMan_Restart calls AISettingsFileLoader each restart (`DynMan_Restart_0042c3b0.c:11`). Its writes into the strength path were not traced (OPEN).

## 4a. Prior-art cross-check (ask.py / f1c_mod evidence-query, read-only)

- `aad-strength-scales-ai-physics-2026-10-15` (DERIVED, H1/H2): "Only caller is AIDriverStructInit (session init)" is now corrected. It has a mid-race re-init through R4/R5 (AI pit) and R2 (player, human pit-proximity or sub_4F1FE0 branch) (section 2). Its open item "mid-session re-call of sub_4F1510 ... untested" stays untested; section 6 gives the design.
- `aad-live-strength-call-spec-2026-10-15` (DERIVED, H7): its open items "whether the player car gets scaled" (section 3: yes by code) and "how often AIDriverStructInit re-runs mid-race" (section 2: AI pit routes; human-car routes; session routes) are answered here by code, not yet live.
- `aad-aiw-adjust-is-strength-curve` (CONFIRMED): consistent with section 4 (anchors feed both pace and physics strength).
- `aad-racequalratio-and-quali-compression` (DERIVED): flt_5EA3E8 = RaceQualRatio per track; consistent with the correction in section 4.
- `0x44CC-second-writer` (CONFIRMED): AIDriverStructInit rewrites veh+0x44CC (PassingNorm) on every re-init, so a re-init also resets PassingNorm (FACT by prior art). Side effect for any re-init route.
- `S1-per-car-restart-4c6d90` (CONFIRMED): SceneRenderUpdate is the per-vehicle restart with integer 1 (or 0); consistent with section 2.
- `f1c-sub424cd0-is-event-driven-not-per-frame` (CONFIRMED): natural dispatcher traffic during racing is command 1 (timer), which does not render. Only command 15 renders (section 2, R5), so the render route is rare, consistent with this.
- Open item in `aad-aiw-adjust-is-strength-curve` ("whether the physics baseline phys+15348 starts at 1.0"): answered in part by FACT, `SuspensionAndWheelPhysics_004e5f60.c:293` sets B = 1.0 at every SceneRenderUpdate before the scale (section 5).

## 5. Order of operations inside one re-init (DERIVED from FACT order)

SceneRenderUpdate(this, a2) for a car: `:7` LargeConfig(this+3) -> SuspensionAndWheelPhysics (B = 1.0 at `:293`; fuel factor, pair table, gear block re-derived; LoopWithConditionalSubCall rebuilds W+0 = W+4 * B) -> `:19` AIDriverStructInit -> `:131` sub_4F1510(P, v19) (W+0 *= v19; B = v19). After the event: state = baseline x v19, B = v19. A DLL call afterwards with target T gives baseline x T exactly (composition, H7 section 4, DERIVED). Any DLL change made before the next event is overwritten at that event.

## 6. Question 4 - recommended mod approach

| Option | Pros (evidence) | Cons (evidence) |
|---|---|---|
| (a) Re-call sub_4F1510 after each re-init | Uses the game's own scaler; the composition is exact (H7 section 4). | Needs an event or per-frame hook: SceneRenderUpdate has 7 callers (FACT), so an exit hook on SRU covers them all. A per-frame poll of B (P+15348) reads the game's value for up to one frame after an event. |
| (b) Hook AIDifficultyInterpolation (0x424AC0) per car | Its only caller is AIDriverStructInit (FACT). | It takes no car argument (`AIDifficultyInterpolation_00424ac0.c:1-10`), so the hook must read the caller's ESI. Fragile. Still only fires at re-init, so it cannot change the car mid-race. |
| (c) Patch flt_5EA3DC/E0/E4/E8 before session init | Simple, no runtime hooks. | Global, not per car: the same value reaches the player and every AI car. The three anchors also set the track's AI pace curve GDS (`flt_5EA3D8`), so pace changes too (prior art, CONFIRMED: aad-aiw-adjust-is-strength-curve). The values come from the track's AIW file and are reloaded at Game_Init (FACT, section 4), so a patch is lost on the next full restart unless re-applied after parsing. Cannot change a car mid-race without a re-init (FACT, section 5). Not recommended. |
| Preferred: hook the single call site 0x424AA6 | Covers every route R1 to R5 (all pass AIDriverStructInit), including the player, with no polling. ESI = vehicle base, so slot = (ESI - 0x5EC6D8) / 0x48C0 (stride from `TimerFrameRateThrottle_004d3740.c:9`, 4656 DWORDs). Exact composition. | Still needs a per-car policy (player or AI only). Full re-init is a full physics rebuild (FACT), so frequent re-init may be visible (OPEN). Thread of the call: OPEN (main sim thread assumed, H7 section 7). Also the one place where a per-car value is needed: the hook can read the car from ESI and skip the human car if the owner wants AI-only (section 3). |

Additional points:
- Keep byte_7D01C4 = 7. Changing it affects all cars and the game's own re-init. The bit flags also scale torque (bit 1), gear ratios (bit 2) and fuel factor (bit 4), so a "strength" change through sub_4F1510 is a bundle, not only grip (FACT, `ScaleAudioPitch_004f1510.c:28-67`).
- For a mid-race change, call sub_4F1510(P, target) once on the current state, then let the call-site hook keep it through later re-inits (DERIVED, section 5).
- Owner decisions: (i) AI-only or also the player (veh+13000 == 0 or PlayerVehiclePtr); (ii) whether the player is re-initialized by R2 (it is, by code).

## 7. Open items

1. Live: does the human car re-init through R2/R2b (log B at P+15348 after pit-proximity with a damaged or retired human car)?
2. Live: FinishState (0x4064) value during a normal AI pit (R5 gate).
3. Live: what sets veh+3352 and veh+3416 (R4 gate).
4. Live: sub_4F1FE0 branch (`:147-148`), which field `this[3247]` is, and when it fires.
5. Replay and ESC-to-pits routes: not traced.
6. TimerFrameRateThrottle FlagPropertyGetter(2048) value (command 15 possibility).
7. Sim thread for the DLL call (H7 OPEN 5).
8. Whether PhysicsParameterInitialization (B = 0, 0.2f, 0.5f then FloatClamp) can leave B = 0 at session start (sub_4F1510 divides by it at `ScaleAudioPitch_004f1510.c:17`). Check FloatClamp bounds (`FloatClamp_004646c0.c`).

## 8. Live validation (not run; clean test install only, memory-MCP freshness rules)

Use H7 section 8 tests 1-7, plus: (i) log B and the veh+13000 flag per car across a pit approach; (ii) log dword_7CE258 and the R2 call after a human-car pit-proximity event; (iii) confirm no human exemption by checking B on the player after R1.

## 9. Proposed findings.jsonl entries (not appended)

- FACT: sub_4F1510 has one caller, AIDriverStructInit (asm 0x424AA6); AIDifficultyInterpolation has one caller (same function).
- FACT: SuspensionAndWheelPhysics:293 sets B = 1.0 on every SceneRenderUpdate before AIDriverStructInit:131 (order in SceneRenderUpdate:7 -> :19).
- FACT: sub_4A0250:44 and sub_4A00A0:40 call SceneRenderUpdate(0) with ECX = dword_7C54E8 = PlayerVehiclePtr (asm 0x4A0358, 0x4A01C5).
- FACT: AICommand case 15 renders only when FinishState (veh+0x4064) != 0 in race (AICommandMessageQueueDispatcher:23-34).
- FACT: LoopWithConditionalSubCall (sub_4F1A60) never calls sub_4F1510; it is a per-frame B consumer (correction to H7).
- DERIVED: no human exemption on the sub_4F1510 scale path; the player gets curve(dword_7D01A0) x 0.995.
- OPEN: R2b sub_4F1FE0 field and trigger; PitStopBlockingCondition inputs (veh+3352/3416).

## 10. Documentation gate

- This file is the durable record. Recommend a one-line entry in the adaptive_ai_difficulty investigation index (not done: read-only scope).
- Facts and inferences are labelled; pending validation is section 7-8.
- Validator: 2 pre-existing errors (`.claude`, `plans`), unchanged.
- Changed: this file only. No code, game, settings, findings.jsonl or governance file was changed.
