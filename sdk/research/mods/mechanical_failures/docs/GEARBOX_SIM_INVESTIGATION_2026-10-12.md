# Gearbox simulation - native investigation (2026-10-12, Claude)

Owner request (chat): give the GEARBOX family the same treatment as ENGINE - realistic wear simulation, the
native lifetime value decides WHEN, wear from gearbox use, scalable to the player, more gearbox issues for AI.
Pipeline stage: investigate -> specify. Spec: [GEARBOX_SIM_SPEC_2026-10-12.md](GEARBOX_SIM_SPEC_2026-10-12.md).
Sources: raw Hex-Rays files in `f1c_ida_decompiled/functions/`, SDK `inspect`, `exe-disassemble` of
`F1 Challenge V10\F1 Challenge Online.exe`, prior records (LIFETIME_SEEDS_INVESTIGATION_2026-09-27,
GEARBOX_FAILURES_TAXONOMY_AND_PLAN_2026-09-27, gear_failure.c header). No live session in this pass.

## Facts (code read; tier FACT unless marked)

1. **The gearbox life counter.** `EngineWearFailureSimulation_004ee150` seeds four floats per car,
   `veh+0x3D70/74/78/7C` = engine / brakes / gearbox / suspension, each `Normal(3 x LifetimeAvg, 3 x LifetimeVar)`
   from the car's `engine.ini`, divided by `flt_8ACF9C` in race + Failure Rate mode 2, only for Failure Rate 1-2
   (0 and 3 leave FLT_MAX). The gearbox slot is `veh+0x3D78`. No per-gearbox token exists in any car file
   (LIFETIME_SEEDS record): the gearbox budget is the engine pair with its own noise draw.
2. **Only AI ticks it, flat.** `FuelConsumptionAndWear_004ed960` lines 112-126: after green, every counter >= 0
   drops 1.0 per game second, gated to AI by `veh+0x32C8` (player 0). No speed, load, shift or heat term.
   Player copy: seeded, frozen (live 2026-09-27: bit-identical over 246 s of driving).
3. **What vanilla does at expiry (case 2).** ReasonCode `veh+0x4068 = 2` (if none); then only if
   `0.5 x rev limit (veh+0x39A4) <= rpm (veh+0x39A0)`: stuck byte `veh+0x39FA = 1`, drive state
   `veh+0x3D6C = 1` (if 0), commentary `sub_41AC50(veh, 2)` = eid 212-217 "<driver> is stuck in 1st..6th gear"
   by the current gear `veh+0x39FC` (211 default); else the counter is set to 0 and fires again next tick.
   (`FuelConsumptionAndWear_004ed960.c` 240-255, `sub_41AC50_0041ac50.c`.)
4. **Stuck byte consumers.** `0x4EF930` (AI SetGear, `this = veh+0xC`) returns at once while `veh+0x39FA != 0`.
   `PitStopDecisionTireCompound_0042a3d0` schedules a pit stop (race) and `PitStopBlockingCondition_0042aee0`
   blocks the exit -> the stuck AI pits and stays (vanilla gearbox retirement). Cleared only at session init
   (`SuspensionAndWheelPhysics_004e5f60` line 639).
5. **Drive state 2** (`veh+0x3D6C`, set by the engine/brake/suspension cases): `0x426330` shifts to neutral,
   `AIMainUpdate` runs the broken-car path, `AICollision` returns 2. Gearbox case 2 never sets 2 natively.
6. **AI shift path.** `AIMainUpdate 0x424E70` -> `PhysicsGearShiftLogicWithThresholds 0x426330` (returns while
   shift timer `veh+0x3A10 > 0`; upshift when rpm > `veh+0x3A18`; state 2 -> neutral) -> `0x4EF930(veh+0xC, gear)`.
   `0x4EF930`: gear 0 = neutral at once; else (timer <= 0) sound, timer = up/down time (`veh+0x3A08/0x3A0C`),
   target `veh+0x3A14 = gear`, current `veh+0x39FC = 0` until the timer expires. Callers also AIMainUpdate,
   SteeringAngleController, PitStopReadyCheck (SDK inspect).
7. **`0x4EF930` prologue** (exe-disassemble): `53 | 56 | 8B F1 | 8A 86 EE 39 00 00` = push ebx, push esi,
   mov esi,ecx, mov al,[esi+0x39EE]: 10 position-independent bytes, site 16-byte aligned. The shared 8-byte
   atomic patcher (`mf_patch8.h`) allows at most 8 stolen bytes, so this site needs a 10-byte variant.
8. **Player gear path** (already proven, gear_failure.c): SetGear `0x447E00`, `this = 0x7C7428`, target gear
   `0x7C931C`, filter rewrites the requested gear (stuck / lost / false neutral / no drive DRIVER-CONFIRMED
   2026-07-30). Inputs the player model can read: rpm `veh+0x39A0`, rev limit `veh+0x39A4`, torque/throttle
   `0x7C7430` (double, engine model input), speed `veh+0x4440`, ForwardGears `veh+0x3A04`, ambient `0x8AFEF0`.
9. **AI inputs** (engine_failure.c AI section, FACT there): rpm `0x39A0`, limit `0x39A4`, throttle `0x357C`,
   speed `0x4440`, gear `0x39FC`, top `0x3A04`, shift timer `0x3A10`, target `0x3A14` (this pass, item 6).

## Inferences (not proven live)

- Vanilla per-race gearbox failure probability equals the engine's for AI (same distribution); with the
  engine family's Q4b numbers the vanilla AI average is about 2.8 % per car per full-distance race.
- Forcing gear 0 on an AI at speed is unsafe: `0x426330` re-selects gear 1 from neutral at any speed. The AI
  false-neutral symptom is therefore a refused shift, not a neutral.
- The gear ratio table `veh+0x39DC` is structural only (RETRACTED as the physics source 2026-07-30); the model
  learns rpm per m/s per gear online instead.

## Open (needs a live session)

- `0x4EF930` detour behaviour on AI (refuse / substitute) and that refusing never stalls the AI pipeline.
- Native counter hold on `veh+0x3D78` (the engine family does the same on `0x3D70`, live since 1.0.0).
- Real shift rate and throttle duty per lap (calibration of MF_GBX_SHIFT_S and the load norm).
