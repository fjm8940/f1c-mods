# Native engine issues the player can experience (2026-09-27)

**Question (owner):** investigate what engine issues the player can already
experience in the game as shipped; owner prior knowledge: (1) overheating due
to radiator or too high RPM, (2) engine blowup as per `engine.ini` values.

**Answer: both are real, with one nuance each.** (1) Overheating is real, but
"radiator" means the radiator *setting/airflow* — F1C has no radiator-damage
hole; big collisions kill the engine through a separate impulse-seizure path.
(2) The `engine.ini` lifetime blowup is real and, **in this installation,
already active in normal races** because the 2002 season files carry
Randomizer-reduced lifetimes and the profile runs timescaled mode 2.

## 1. Overheating (CONFIRMED, live-observed)

The player runs a full thermal model in `EngineStressSimulation_0043ef20`
(player-only; AI runs the four wear countdowns instead):

- **Heat in:** combustion (`CombustionHeat`, per litre burned) and engine
  speed (`EngineSpeedHeat`); oil temperature lives at `this+7500` (the dash
  "ot" channel).
- **Heat out:** ambient transfer, oil→water transfer, water cooling, and
  **`RadiatorCooling` scaled by velocity and the setup's `RadiatorSetting`**
  (default 3; the radiator also affects drag/lift — closing it costs cooling).
- **Kill chain:** oil temp above `LifetimeOilTemp` base → lifetime burn
  accelerates (each unit of the halving range ≈ doubles the burn), life
  expires → ReasonCode 1, staged smoke/bank roll/death.
- **Live proof:** `DOUGHNUT_BLOWUP_2026-07-25.md` — sustained doughnuts (max
  heat input, zero airflow) killed a 2001 Arrows engine at **oil 130.7 °C**
  (base 123.43), engine stopped, ReasonCode 1, zero contact, zero damage.
  Water temp stayed low (65 °C) — oil is the early-warning channel.
- **"Due to radiator":** the game models radiator *cooling* (setting +
  airflow), not radiator damage. `damage.ini [PHYSICAL]` has no radiator
  field; the existing damage keys are Engine/Aero/Wing/Wheel/Part.

## 2. `engine.ini` lifetime blowup (CONFIRMED — and hot in this install)

- Player life at `0x7C91DC` = `Normal(LifetimeAvg, LifetimeVar)` ×1 (AI uses
  the same token pair ×3 for four subsystem timers; player gets only this one).
- Drain: rpm curve (`LifetimeEngineRPM`) + oil-temp curve (`LifetimeOilTemp`)
  + stress, per tick. Rev limits sit **above** the RPM base on every car, so
  racing near the limiter consumes budget faster (halving per range unit).
- **This installation's 2002 season:** `LifetimeAvg` 2806–4232 s (mean 3370)
  — Randomizer-reduced to 27–41 % of clean originals (mass-retirement
  diagnosis 2026-09-15); +25 % applied to `Custom\2002\V1` on 2026-09-16
  (still 34–51 % of clean), failure mode 2 (timescaled). Native engine deaths
  inside a race are therefore plausible — consistent with the owner's "too
  many retirements" report. The active tree and the Custom V1 tree are no
  longer identical; which tree a given season loads matters.
  Evidence: `operations\ENGINE_LIFETIME_2002_25PCT_APPLY_2026-09-16.md`,
  `f1c_ida_decompiled\investigations\engine_lifetime\2002_V1_AUDIT_2026-09-16.md`.
- Stock 2000/2001 values (~8–10k s) would NOT expire inside a race; the
  finding that "stock races never see wear deaths" applies to those seasons,
  not to this reduced 2002 set.

## 3. Collision seizure (separate native path)

`damage.ini [PHYSICAL] Engine=10000.0` — a big-enough impulse (≈150 km/h
head-on) seizes the engine instantly. Unrelated to overheating or lifetime.

## 4. Not player-accessible

- The four AI wear countdowns (`veh+0x3D70/74/78/7C` = engine/brakes/gearbox/
  suspension) — live-proven dead for the player.
- The lap-completion component roll (`EngineFailureCheck`) belongs to the
  wear-timer path documented as AI-side; player relevance is unverified and
  must not be relied on.
- Over-rev has no instant-damage model; the rev limiter simply cuts ignition.
  High RPM matters only through the lifetime-burn term.

## 5. Other engine-stop triggers found (second pass, 2026-09-27)

- **Out-of-fuel stop — ReasonCode 9 (Fuel).** Two coupled parts,
  **mechanism pinned 2026-09-27 second pass**:
  1. **Drain** (`EngineStressSimulation`): consumption = `this+428 × torque ×
     |rpm|` scaled by `this+436`, per tick; fuel lives at physics-object
     `this+412` (= `0x7C75C4`, double) seeded in `sub_43C250:421` from the
     garage **`FuelRange`** selection (base `a2+0xCB0`, step `+0xCB8`, count
     `+0xCC0`, index `+0xCC4`). At ≤0 it clamps to 0 and writes ReasonCode 9
     if none is set. Consumption is load-proportional: thrash it and the tank
     empties faster.
  2. **The throttle cut** (`sub_447B10`, misnamed "RandomPhysicsParameter-
     Generator"; it is the engine OUTPUT MULTIPLIER — `Gearbox_00443b70`
     ends with `torque = sub_447B10(this) × torque`):
     - ignition off (`this+7257` = `0x7C9081`) → `0.0`
     - fuel ≥ 0.9 → `1.0` (full authority)
     - fuel ≤ 0.0 → `0.0` (dead pedal)
     - otherwise `mult = clamp01(noise × 1.5 + (fuel + 0.1))` — a
       **stochastic multiplier whose floor falls with the tank**:
       ≈ `[0.6 .. 1.0]` at 0.5 units, ≈ `[0.15 .. 1.0]` just above zero,
       hard `0` at empty. This flicker IS the "lagging/cutting, then the
       throttle stops responding".
  Engine-running predicate (`sub_447160`) = `life > 0 && fuel > 0 &&
  ignition`. Recovery: refuelling (`PitStopProgress`) clears code 9; the
  game's starter (`sub_4471F0`) re-arms the ignition byte and lifts rpm.
  Caveat: the internal unit of `this+412` (litres vs fraction) is not pinned;
  the garage `FuelRange` block and the gauge copy (`veh+0x3984`) suggest
  litres, in which case the flicker covers the last ~0.9 L. Live-verifiable
  by reading `0x7C75C4:double` while the gauge drains.
- **Ignition switch.** `sub_42C440` (player input/assist handler) toggles the
  ignition byte when input flag `dword_7CC6D8` fires — a player-accessible
  ignition off/on. The exact key binding is unlabelled in the corpus.
- **Unresolved helper.** `sub_4EFAC0` ("ConditionalGate") sets the player's
  ignition byte to 0 (its AI branch clears a different field). No decoded
  caller exists in the corpus, so its trigger is unknown — likely
  race-end/teardown logic. Do not rely on it.
- **Not found:** no other writer of the player life field (`0x7C91DC`:
  `sub_43C250` seeds it, `EngineStressSimulation` drains/blows up, our DLL
  writes it); water temperature only modulates cooling (no kill); no stall
  logic found.

## 6. Implications for our mod

1. **Double jeopardy risk:** if the 2002 reduced-lifetime setup is what the
   owner races, the game can already kill engines by itself. Adding our 15 %
   engine roll on top overshoots the intended frequency. Options, owner's
   call:
   - leave native as-is (accept stacking),
   - or **normalise the native wear risk** (restore lifetimes and/or keep the
     player seed high) so OUR system is the only engine-failure source —
     consistent with the 2026-09-27 decision that probabilities are ours.
2. **Overheat stays authentic:** it is a native, drivable, manageable kill
   (airflow/rev/setting). Do not duplicate it in our weights; it can coexist
   as the "skill" failure.
3. Any lifetime file changes are game-config changes: separate owner approval
   and preservation required (the 2026-09-16 batch is the precedent).

Evidence: `EngineStressSimulation_0043ef20.c` lines 60–131 (thermal block and
life drain), `sub_43C250_0043c250.c:531-540` (player seed) and `:419-433`
(fuel-field seed), `Gearbox_00443b70.c:161-168` (output multiplier call site
and fuel gate), `RandomPhysicsParameterGenerator_00447b10.c` (the multiplier),
`findings.jsonl` FACT row "Engine RUNNING byte 0x7C9081 gates torque — the
binary seizure lever" (sub_447B10 fuel-starvation ramp; sub_447160 running
predicate; sub_4471F0 starter; sub_4472A8 kill), `VehicleActive_00447160.c`,
`PitStopProgress_004a68b0.c` lines 66–67, `sub_42C440_0042c440.c` lines 75–91
(ignition toggle), `ConditionalGate_004efac0.c` (unresolved), doughnut and
audit docs above, `damage.ini` current values.
