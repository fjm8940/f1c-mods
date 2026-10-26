# 54 — AI driving brain: per-tick pipeline, steering law, modes

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)
**Date:** 2026-10-02 · **Author:** Claude (Opus 5.5) for Matias · **Status:** STATIC (raw Hex-Rays) + steering law OBSERVED live (section 2.1)

Purpose: one map of how an AI car decides throttle, brake, steering and gear, so mods can steer the game's own
brain instead of overriding its outputs. `this` = vehicle base (veh = 0x5EC6D8 + 0x48C0 x slot). Offsets are veh-relative.
Tiers: **FACT** = read directly in raw decompile; **DERIVED** = interpretation; **OPEN** = not known.

## 1. Pipeline (AIMainUpdate 0x424E70, ~40 Hz per AI car)

Order inside one tick (FACT, `functions/AIMainUpdate_00424e70.c`):

1. Heading vector: 0x448C = -sin(yaw 0x34F0), 0x4494 = -cos(yaw).
2. `sub_426430` = **waypoint locator**: speed 0x4440 = |velocity 0x34F8..0x3500|; current waypoint ptr 0x4354
   (re-found by nearest-neighbour search if >sqrt(3200)=56.6 m away); fills 19 look-ahead waypoint ptrs 0x4358..
   (next = wp+272, branch = wp+276 when branch id matches 0x454C); lateral position 0x4428 = -(dot(car-wp pos, wp right));
   look-ahead index 0x4468 by speed (<15 m/s: 0, <30: 1, <65: 2, else 3); anchor point 0x4498/0x44A0 = car position at
   waypoint change. Returns 0 = no waypoint: the whole AI tick is skipped.
3. If mode 0x4510 is 4/5 (pit/garage wait): outputs throttle 0, brake 1, steer 0; return.
4. `OpponentProximityScanTeamRecognition` (traffic), speed limit 0x4484 reset to 1e6.
5. `AIRacingLine 0x425920`: speed profile and pedal candidates throttle 0x4434 / brake 0x4438 (see AI_CONTROL_OUTPUT_NOTES).
6. `AISteering 0x4270A0` (only if on main chain): lateral directive / corridor (prior art: 0x4408, 0x440C, 0x4414/0x4418, 0x4420).
7. `sub_426100` = **steering law** (section 2). Writes steer cmd 0x442C.
8. `AISteeringCorrection 0x426CF0` = **pedal post-processor** (section 3). Name is wrong: it touches no steering.
9. `PhysicsGearShiftLogicWithThresholds 0x426330` = auto gear (prior art).
10. Mode switch on 0x4510 (section 4).
11. Output copy (LABEL_86): steer act 0x3580 = 0x442C, throttle act 0x357C = 0x4434, brake act 0x3584 = 0x4438.

## 2. Steering law `sub_426100` (FACT code, DERIVED meaning)

Pure pursuit with no damping, no feed-forward:

```
wp      = ptr[0x4354 + 4*idx0x4468]          ; 0..3 waypoints ahead (speed dependent)
d       = |car_xz - anchor 0x4498/0x44A0|     ; distance since last waypoint change -> 0x44A4
target  = wp.pos(+148,+156) + offset0x4408 * wp.right(+32,+40) + d * wp.dir(+164,+172)
err     = angle(heading 0x448C/0x4494, target - car), sign by cross product
steer   = err / 0x44B4                        ; 0x44B4 = divisor, likely steering lock in rad (OPEN: value)
mode 6  -> steer = -steer                     ; reversing
steer   = clamp(steer, prev +- 0.1)           ; rate limit 0.1 per tick (~4 per s)
steer   = clamp(steer, -1, 1) -> 0x442C
```

DERIVED consequences:
- The only lateral "knob" is offset 0x4408 (plus the chain the AI follows). Moving 0x4408 moves the steering target. This
  matches prior art `ai-pulloff-lateral-chain-v4-2026-10-02` (AI steers itself to a shifted line).
- The look-ahead distance is short (0-3 waypoints, about 5 m each on Monza, plus d), so a large step in 0x4408 gives a sharp
  turn at low speed. Ramp it.
- Writers (CORRECTION 2026-10-02, capstone scan + Claude check, finding ai-brain-4408-44b4-writers-closed-2026-10-02):
  0x4408 is written only by the AISteering epilogue 0x4293F0 (copy 0x440C; then 0x4420 if byte 0x4548; then 0x4424 if
  non-zero) plus RaceStructInit zeroing; 0x44B4 only by AIDriverStructInit from veh+0x396C. Hex-Rays shows them as
  scaled indices (`this[4354]`), which is why a text search missed them.

### 2.1 Live check (OBSERVED, 2026-10-02, read-only)

Fernando Alonso, slot 8, mid race, game PID 28124, mechanical_failures F67D486D installed (no override active),
20 s at ~40 Hz, 786 samples, 69-324 km/h, mode 0 throughout. Logger: disposable `%TEMP%\mf_dyn\brainlog.py`.

- Recomputing the law above from live memory vs steer cmd 0x442C: correlation 0.989, median abs error 0.017, p90 0.051
  (residual = rate limit + sampling between ticks).
- 0x44B4 = 0.4014 (constant) = about 23 deg, steering lock in rad. So steer = heading error / 23 deg.
- Look-ahead index 0x4468 matched the speed rule in 782/786 samples (4 misses at speed boundaries; idx only updates on
  waypoint change). Seen values 1, 2, 3.
- d 0..6.9 m. Offset 0x4408 -3.45..+2.63 m tracks lateral position 0x4428 (-3.78..+2.70): 0x4408 = racing-line lateral
  position in normal racing (DERIVED).
- Steer act 0x3580 == cmd 0x442C in 786/786 samples. Steer used -0.43..+0.54.

## 3. Pedal post-processor `AISteeringCorrection 0x426CF0` (FACT)

- Steer lift: if |steer| > 0.1 and speed > 20 m/s, throttle *= 1 - 0.7*|steer|.
- Traction control: blends wheel-slip values 0x38C4/0x37CC (weights 0.3/0.7) with last value 0x4464; below 0.9 it pulls
  throttle toward `SuspensionRatioClamp` (name doubtful). Possible brake-on-slip 0x4028 output.
- Speed limit 0x4484 (pit limiter etc.): over by >10 m/s, brake = tyre grip and throttle 0; >3, brake >= grip/2;
  >1, throttle 0; just over, throttle = 0.8 * `BrakeThermalModel` (name doubtful). Only active 2.5 s after flt_8ACF90.
- Throttle cap: race start (byte_5C3160 set) uses 0x44B8; otherwise difficulty cap 0x457C = AIDifficultyScalar * 0.75
  when 0x45FC is set.

## 3b. Corner speed and braking (AIRacingLine 0x425920, both OBSERVED on Monza, Claude 2026-10-02)

- Corner speed per waypoint = `sub_42A780` (misnamed AIComputePassScore):
  `(TireForceSolver(wp) * 0x4504 + 0x4500) * 0x4580 * 0x453C * wp+0x84`. 0x453C = AIAbility dynamic multiplier.
  Exact form (disassembly 0x42A780): solver this = veh+0xC, args (wp+0x74, &wp+0x78, wp+0x70, 16.0); the limiting
  target 0x43FC is additionally `* 0x4554` (1.0 in all samples). Bypasses: wp+0x134 == 1 -> int wp+0x138 / pit speed
  flt_8ACFB4; linked-car block -> 12.0. LIVE-CONFIRMED vs 0x43FC (Monza dry, n=3549 unique ticks, median meas/pred
  0.9993, 100 % within +-2 %, 35 corners; finding ai-brain-corner-speed-live-confirmed-2026-10-02,
  `investigations/ai_brain_map/phase3/REVIEW_14_D1.md`). Second track / wet still open.
- Braking look-ahead: start at speed 0x4440; per waypoint ahead `v -= len(wp+0x3C) / v * 0x44CC * TireForce(v, wp+116,
  wp+120, wp+112) * 0.915` (FACT from disassembly 0x425EEF; CORRECTION 2026-10-02: Hex-Rays hid the TireForce return
  and showed the target speed instead, finding ai-brain-braking-step-tireforce-correction-2026-10-02).
  LIVE-CONFIRMED (Monza, median meas/pred 1.000, n=1936; finding ai-brain-braking-step-live-confirmed-2026-10-02).
  If projected speed is still >= a waypoint's target: brake now (0x4438 = sub_4E88B0 'TireGripCoefficient' = brake
  amount, +0.2/tick ramp, throttle 0). If projection reaches 0 first: throttle (1.0, or sub_4E87E0 'BrakeThermalModel'
  = throttle to hold speed, 0.4/0.5 off-line), ramped by 0.2 * 0x44B8/0x44C4.
- Profile 0x43A4..0x43F0 (20 speeds), limiting waypoint 0x43F8, its target 0x43FC (9999 = none), on-track byte 0x445C.
- Review records: `investigations/ai_brain_map/REVIEW_2_GROUP1.md` (static), `phase2/REVIEW_13_MONZA.md` (braking
  live), `phase3/REVIEW_14_D1.md` (corner speed live).

## 4. Modes 0x4510 (FACT code, DERIVED meaning)

| Mode | Code | Meaning |
|---|---|---|
| 0 | `SteeringAngleController 0x425670` | normal racing + stuck/wrong-way watchdog |
| 1 | `sub_4258C0` | recovery stop: throttle 0, brake 1; to 2 when speed < 4 or both slip values > 0.9 |
| 2 | `FieldZeroSetter` (writes 0x4510 = 0), return | one-tick reset after mode 1: skips that tick's commit, outputs keep their last value (CORRECTION 2026-10-02, `investigations/ai_interaction_role/README.md` 2026-08-18 "mode 2 only resets the mode field"; a 20 Hz log can miss it, AI ticks every 25 ms) |
| 3 | distance check to AIW point indexed by veh+0x48A4 (pit TeamIndex) | approach own pit/team spot; to 0 when distance starts growing. LIVE 2026-10-02: garage exit in practice 5 -> 3 -> 0 (finding ai-brain-mode3-garage-exit-live-2026-10-02) |
| 4/5 | early branch | pit/garage wait, outputs 0/1/0 until released (5 -> 3 at race start) |
| 6 | `0x425670` + steer sign flip | reverse toward current waypoint; to 7 when heading error < 0.5 rad or 4 s timeout |
| 7 | inline | brake to stop; to 0 when speed < 0.2 |

`SteeringAngleController` is misnamed. It steers nothing. It compares travel direction (velocity) with yaw: error > 0.5 rad
at speed > 0.5 m/s adds 5 to counter 0x446C, otherwise -1; counter > 0x4558 -> mode 1 (recovery). Every 4 s it checks
progress against the saved position 0x4470..0x4478 (timer 0x447C) and toggles 0 <-> 6 (reverse attempt) when stuck (flag condition
decompiled as `v12|v13`, exact compare OPEN).

So the native recovery chain is: 0 (wrong way / stuck) -> 1 (stop) -> 2, and 0 -> 6 (reverse) -> 7 (stop) -> 0.

## 5. Native "dead car" coast (FACT code, DERIVED meaning)

When carConditions 0x3D6C == 2 (retired / wheel loss), at the output copy: throttle 0, and brake by lateral
position |0x4428|: <5 m: 0.01; 5..7 m: 0.01 + 0.1*(|lat|-5); >7 m: 0.21. `sub_426430` also drops its corner heading
clamps for this state. DERIVED: the game already has a gentle "roll off and stop away from the line" for retired cars.
Candidate base for the mechanical_failures pull-off instead of brake 1.0 aborts (owner fix c). Not live-tested.

## 6. What this means for mods (DERIVED)

- Full override (post-hook on 0x424E70) = we replace the brain's outputs. Good for tests, not for "smart" behaviour.
- Brain steering = write inputs before the tick: offset 0x4408 (+ corridor 0x4414/0x4418, prior art), speed limit 0x4484
  (makes the AI brake itself smoothly), mode 0x4510 (7 = native stop), carConditions 0x3D6C = 2 (native coast; note LIMBO
  risk, finding `limbo-unlocked-carconditions`).
- Speed limit 0x4484 is reset to 1e6 at step 4 each tick, so a write must happen after step 4 and before step 8 (hook
  `AISteeringCorrection` entry) to stick. DERIVED.

- AISteering epilogue (L1293-L1313, DERIVED): 0x4408 = 0x440C; byte 0x4548 != 0 -> 0x4408 = request 0x4420 directly;
  float 0x4424 != 0 -> 0x4408 = 0x4424 (wins). Candidate clean lateral override for pull-off: set 0x4548 (+0x4420) or
  0x4424 before AISteering runs. Not live-tested; 0x4424 also written by AIRacingLine.

## 6b. AI Brain ABI v1 (2026-10-02)

Read-only field registry for GUIs/tools: `investigations/ai_brain_map/ai_brain_abi_v1.json`
(+ `ABI_README.md`, `test_abi.py`; built by DeepSeek, reviewed by Claude REVIEW_1..5). 55 fields in groups state /
perception / plan_speed / plan_line / decision / watchdog + 5 derived views. GUESS fields must be shown as GUESS.

GUI: `mods/sdk_sandbox/ai_mind_monitor/ai_brain_gui.py` (read-only, uses the registry).

Viewer v2 + LLM recorder: `mods/sdk_sandbox/ai_mind_monitor/` (`brain_core.py`, `brain_recorder.py`). Phase 2
(read-only coverage census, live validation, write-readiness): `tools/sdk/DEEPSEEK_HANDOFF_AI_BRAIN_PHASE2_2026-10-02.md`.
Write roadmap (owner goal, not started): writes only via an in-process hook at the stage named in phase-2
WRITE_READINESS (base: mechanical_failures AiOverride post-hook), one field group at a time, each live write with
owner OK in chat.

ABI v1.1 (2026-10-02, phase 2 C6, REVIEW_8): `investigations/ai_brain_map/ai_brain_abi_v1_1.json`, 140 fields =
100 % of base-resolved vehicle offsets used by the mapped AI functions (6 base-open objects + 51 second-level callees
still unknown). Viewer/recorder load it automatically.

**Decompiler warning (FACT, 2026-10-02):** Hex-Rays drops x87 st0 returns at 14 AI call sites (list:
`investigations/ai_brain_map/phase2/DROPPED_ST0_SCAN.md`). AIRacingLine's AerodynamicForceCalc branches and the braking
step use the callee RETURN value. Check disassembly before trusting a formula next to a float-returning call.

## 7. Open / next

Current handoff (2026-10-02 end): `investigations/ai_brain_map/HANDOFF_2026-10-02_CLAUDE.md`.

Full mapping delegated (owner, 2026-10-02): `tools/sdk/DEEPSEEK_HANDOFF_AI_BRAIN_MAP_2026-10-02.md`, output
`investigations/ai_brain_map/`. Claude reviews at each stop point.


1. DONE 2026-10-02 (section 2.1; Imola, Monaco, Monza). Mode 6 reverse branch seen live (phase-2 Run E).
2. DONE 2026-10-02: writers of 0x4408 and 0x44B4 (section 2).
3. DONE 2026-10-02: corner-speed and braking maths live-confirmed dry and wet (section 3b; phase3/D11_WET.md).
4. DONE 2026-10-02: mode 3 seen live (garage exit in practice). Mode 2 is a one-tick state (section 4).
5. Batch B results (pace dynamics, randomness, overtake modes, start, pit chain, traction, damage, waypoint record,
   write readiness of all 140 fields): `investigations/ai_brain_map/phase3/REVIEW_16_BATCH_B.md`. Registry v1.3.
6. 2026-10-02/03: qualifying checked live (`phase3/QUALI_RESULTS.md`, REVIEW_17): flying-lap car steers around the out-lap car (14/14), corner formula holds in qualifying, carConditions 1 -> pace 0.85 x baseline. Registry v1.4 (4 GUESS left). Write stage 1 spec: `phase3/WRITE_STAGE1_SPEC_DRAFT.md`.
7. Still open: blue-flag yield live, write proofs (write stage 1), R2 traction magnitude, 4 GUESS fields, compound-id mapping / fitted-compound field. Live viewer: `mods/sdk_sandbox/ai_mind_monitor/README.md`.

Sources: `functions/AIMainUpdate_00424e70.c`, `sub_426430_00426430.c`, `sub_426100_00426100.c`,
`AISteeringCorrection_00426cf0.c`, `SteeringAngleController_00425670.c`, `sub_4258C0_004258c0.c`,
`AIRacingLine_00425920.c` lines 120-145. Findings: `ai-brain-*-2026-10-02`.
