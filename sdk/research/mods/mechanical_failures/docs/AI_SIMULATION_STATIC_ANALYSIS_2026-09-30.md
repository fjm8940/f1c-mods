# What the game simulates for AI drivers - static analysis (2026-09-30)

Author: Claude. Tier: DERIVED (static code read) unless marked FACT.
Finding: `ai-vehicle-simulation-map-2026-09-30` in `f1c_ida_decompiled/findings.jsonl`.
Decompiled `this` in the AI functions = vehicle + 0xC; offsets below are
vehicle offsets (veh+...). Vehicle n base = 0x5EC6D8 + 0x48C0 x n (FACT).

## 1. Split

`VehiclePhysicsModeDispatcher` 0x4E4930 picks per car (veh+0x32C8, FACT: player 0, AI 1):
- player -> full physics on the static object 0x7C7428 (engine stress, thermal, life law);
- AI -> `PhysicsIntegrationStateMachine` 0x4E4E60: a lighter model, sub-stepped
  (more steps at higher speed): suspension, engine torque (0x4E51A0), wheel
  forces, rigid-body + rpm integration (0x4E5AE0).
- AI also gets `FuelConsumptionAndWear` 0x4ED960 once per game second.

## 2. AI simulation, what exists

| System | What the code does | Key fields |
|---|---|---|
| Engine speed | in gear: rpm locked to driven wheels x ratio; CLAMPED to idle..rev limit (no over-rev, no limiter band); neutral: torque / inertia; launch: idle..launch rpm by throttle | rpm 0x39A0 (FACT), limit 0x39A4 (FACT), idle 0x39A8 (FACT), 1/inertia 0x3998, launch 0x399C |
| Torque | coast + throttle x (power - coast) from the car's RPMTorque table (32 rad/s steps, same layout as player) | table ptr 0x39B0, torque 0x39AC, throttle 0x357C |
| Drivetrain | RWD / AWD / FWD split | 0x39C0 |
| Starter | countdown, then rpm = idle | 0x39C4 |
| Fuel | burn = rpm x consumption x throttle x dt; empty -> ReasonCode 9, no throttle | fuel 0x3984 (FACT), consumption 0x3990 |
| Mass | follows fuel | 0x3520 |
| Tyres | wear per wheel (floor 0.75, x tyre-wear setting), temperature blend with track temperature, brush-model refresh every 5.5-7.5 s | per wheel stride 248 bytes |
| Failure timers | 4 countdowns, 1.0 per game second, race phase only | 0x3D70 engine, 0x3D74 brakes, 0x3D78 gearbox, 0x3D7C suspension (FACT) |

Failure timers when they cross 0:
- **Engine (code 1)**: only when rpm >= half the rev limit (otherwise waits).
  Either a staged failure (smoke banks 0x3D54/58/5C, power column of the torque
  table x 0.9, timer re-armed 1-500 s; mode 3 caps 60 s), or power column
  zeroed (engine coasts dead). Same stage-one idea as the player's.
- **Brakes (code 5)**: one axle's brake torque x 1/32.
- **Gearbox (code 2)**: stuck-gear byte 0x39FA (FACT).
- **Suspension (code 4)**: one wheel geometry +0.05.
Timers are seeded Normal(3 x LifetimeAvg, 3 x LifetimeVar) (FACT, earlier finding).

## 3. AI simulation, what does NOT exist

- No oil or water temperature, no radiator, no thermostat.
- No rpm/oil-dependent engine wear: the timer ticks 1 s per game second
  whatever the driver does. AI driving style never changes engine life.
- No engine stress, no rough-running throttle modulation, no over-rev (rpm clamped).
- No engine-braking map (coast column of the table only).

## 4. What this means for the AI shadow engines (paper batch E9p)

- The model's life clock for AI = timer 0x3D70; drain is exactly 1.0 s/s, so
  onset = timer < lead. Works unchanged.
- Load input: real AI throttle 0x357C (better than the speed-based proxy).
- Power loss: best lever is the AI's own torque table power column (the game
  itself scales it x 0.9 on failure). OPEN: per-car or shared between teammates?
  Until Claude checks live (two teammates' table pointers), use the speed cap
  0x4538 fallback.
- Over-rev layers never trigger for AI (clamped rpm): expected, not a bug.
- Thermal faults need the shadow thermal stand-in (no game temperatures).
- Terminal: timer = 0.001 lets the game fire its own engine failure, but the
  game only fires when rpm >= half the limit; a car crawling in the pits waits.
- Large-scale test value: onset timing, fault mix, fairness floor, terminal
  timing, AI power-loss behaviour. Not: thermal realism, driving-style effects.

## 5. Open (Claude live checks)

- L6: AI torque table per vehicle or shared (compare 0x39B0 of teammates).
- L7: readback of 0x357C (throttle 0..1), 0x3990 (consumption) on a running AI car.

## 6. Live results (2026-09-30, game started with `noPlayer`) - FACT

- `noPlayer` puts ALL cars (slot 0 too) on AI physics (veh+0x32C8 = 1); the player static
  object 0x7C7428 stays zero. The player engine/thermal model does not run. AI-only test bed.
- AI throttle 0x357C = 0..1 (FACT now). Engine timer 0x3D70 ticks whole game seconds (3.0 in 3.19 s).
- L6 answered: every car has its own torque table (22 different pointers at 0x39B0) -> the
  torque-table power lever is safe per car; `MF_AI_POWER_TABLE` can default to 1 in E9p.
- 0x3990 fuel coefficient 5.6e-5..5.8e-5 per car; 0x3998 ~17-18 (1/inertia).
- Consequence: today's mod does nothing in noPlayer (it reads the empty player object).
  After E9p, noPlayer + CTRL+T = unattended full-field test of the model.
