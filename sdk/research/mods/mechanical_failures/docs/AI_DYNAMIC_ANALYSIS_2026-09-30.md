# AI drivers - dynamic analysis (2026-09-30)

Author: Claude. Game started with `noPlayer` (all 22 cars AI), race, RacePhase 5.
Method: ReadProcessMemory of the whole vehicle block (0x48C0 bytes) for all 22
cars, 5 Hz, 40 s of session time (200 samples). Every changing field was
correlated against speed, rpm, throttle, and lateral/longitudinal acceleration
derived from world position. Scripts + raw capture: `%TEMP%\f1c_ai_dyn\`
(disposable, per retention policy). Read-only, nothing written.
Findings: `ai-accel-inputs-mass-live-2026-09-30` (FACT),
`ai-wheel-block-live-2026-09-30` (DERIVED), plus `noplayer-mode-all-ai-physics-2026-09-30`.

## 1. New, useful for the engine simulation

| What | Where (veh+) | Tier | Use |
|---|---|---|---|
| Lateral acceleration | 0x354C m/s2 (+-3 g) | FACT | oil slosh / fuel surge for AI (E9p); method for the player's R7 |
| Vertical acceleration | 0x3550 m/s2 | FACT | kerb/landing shocks (idea 6) |
| Longitudinal acceleration | 0x3554 m/s2, forward = negative | FACT | slosh, surge, shift shocks |
| Local force vector | 0x3558/5C/60 N | FACT | - |
| Steering / brake input | 0x3580 / 0x3584 | FACT | AI driving-style metric (aggression) for shadow wear |
| Mass | 0x3520 = const + 0.75 x fuel | FACT | - |
| Brake temperature per wheel | wheel +0x5C | DERIVED | brake family (AI brakes timer exists natively) |
| Suspension deflection per wheel | wheel +0x10 | DERIVED | kerb strike detection |
| Tyre load / forces / spin | wheel +0x44/+0x4C, +0x80/+0xD8/+0xE0, +0x20 | DERIVED | wheelspin -> rpm flare, not needed now |

Wheel block: base veh+0x358C, stride 0xF8 (4 wheels, front pair steers).

## 2. Confirmed from earlier work (still true)

rpm 0x39A0, limit 0x39A4, throttle 0x357C, fuel 0x3984, gear 0x39FC (0-7),
engine/brakes/gearbox/suspension timers 0x3D70-0x3D7C tick 1.0 per game second.

## 3. Still unknown (dynamic behaviour seen, meaning not proven)

- veh+0x3C10..0x3D04: four 14-entry tables, cumulative-looking values
  (3000..34000) all falling slowly, faster at higher index. Guess: AI
  strategy / lap-time or fuel projections. Not engine-relevant.
- veh+0x3D10..0x3D50: a 17-value series (110, 61, 45, 37 ... 18) rising slowly;
  name_index calls 0x3D10/0x3D14 OilTemp/WaterTemp, but the shape looks like a
  table, and an earlier finding says they are not the gauge temperatures. OPEN.
- veh+0x1024, 0x1074, 0x3C04, 0x4020: per-car clocks advancing 0.76-1.29 per
  second (not 1.0) - pace/progress estimates? OPEN.
- veh+0x4054 / 0x40A4: 69 falling 0-0.43 per second. OPEN.
- wheel +0x9C and +0xB0: slow per-wheel decline (wear-like); name_index label
  "tirePuncture" on +0xB0 looks wrong. OPEN.

None of these blocks the engine plan.

## 4. What changes in the plan

- E9p (AI engines) gets real g-forces (slosh/surge) and a driving-style input
  (throttle/brake/steer activity) - added to the paper.
- R7 (player lateral g): same correlation script, run in a normal (player)
  session on the player slot and on the 0x7C7428 object. Claude, next live session.

## 5. Second pass (same evening) - events with CTRL+T (FACT)

- CTRL+T: game time x5 -> x17 average (~x30 peak). AI timers tick per GAME second at all speeds.
- 3 natural AI suspension failures: ReasonCode 0 -> 4, car stops, FinishState 0 -> 2 ~60 game s later.
- No AI engine failure in a full race: AI engine timers (3 x LifetimeAvg) outlast the race.
  For AI engine tests the E9p shadow model must use its own lead/onset (timers rarely expire).
- Pit stop: PitState 1 -> 2; refuel may come in two steps.
- Session end: FinishState -> 1, session time -> ~1e9, and the game zeroes every car's torque
  power column (cleanup), then vehicle memory becomes unreadable. Mods must stop on
  session time > 4e6 or FinishState != 0 and must not treat the zeroed table as a failure.
- AI torque table count at veh+0x39BC (int); AI tables end just above the rev limit.
- 20 Hz pass: AI upshift drops rpm ~13 % (155 shifts); AI proximity lists at veh+0x41B4..0x4344
  (stride 0x44, possible dirty-air input); remaining flags are AI behaviour.

## 6. Can the AI drive the player car on full player physics? (Claude, 2026-09-30)

No native way found (DERIVED + live negatives, finding `player-ai-drive-needs-ai-physics-2026-09-30`):
AIMainUpdate drives a car only when veh+0x32C8 == 1 (AI physics). The player car's full physics
takes controller inputs. Live: veh+0xC4 = 0 on track -> car undriven; disqualified car -> flag 0,
never moved. Options left: (a) test the full player sim by driving (current); (b) a mod autopilot
that feeds throttle/brake/steer into the player object from a racing line - large project, not planned.
AI-physics tests (noPlayer + CTRL+T) remain the large-scale path (E9p).
