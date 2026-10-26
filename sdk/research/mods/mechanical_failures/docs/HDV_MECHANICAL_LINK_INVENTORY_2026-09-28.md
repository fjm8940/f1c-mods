# HDV mechanical-failure link inventory (2026-09-28)

**Question (owner):** the car's `.hdv` (sample: Ferrari 2002_F2002) — does it
contain values we could link to mechanical failures, given the engine.ini
lifetime counters are frozen for the player?

**Answer: yes — the strongest new material is a complete brake wear/fade
model definition, plus a confirmed fuel link and per-car gearbox timing.**
Below, what exists, what is already proven wired, and what needs one
investigation before promising.

## 1. Brakes — full per-wheel wear/failure parameter set (NEW)

Parsed by `PerWheelBrakeSuspensionDataLoader_004f2c60` from each wheel section:

| HDV token (this car) | Meaning |
|---|---|
| `BrakeTorque=3715` | max brake torque (zero wear, optimum temp) |
| `BrakeWearRate=4.726e-011` | metres of disc wear per second at optimum temp |
| `BrakeFailure=(1.43e-002, 5.76e-004)` | **average ± variation of disc thickness at failure** |
| `BrakeOptimumTemp=550` | peak-grip temperature (°C, stored +273.16) |
| `BrakeFadeRange=1100` | temp outside optimum where grip halves |
| `BrakeHeating`, `BrakeCooling=(base, per-velocity)`, `BrakeDuctCooling` | thermal model inputs (duct setting scales cooling) |
| `BrakeDiscRange=(0.021, 0.007, 2)` + `BrakeDiscSetting` | disc thickness setting |
| `BrakeDiscInertia` | rotational inertia of the disc |

- The corpus contains brake thermal/fade/cooling/HUD-named functions
  (`004e87e0`, `00430a50`, `004e5080`, `0049db80`) but the dump names are
  stale/misleading — a consumer of the *wear* fields (thickness reduction →
  `BrakeFailure`) was **not confirmed** in this pass. AI brake failures live
  in the engine.ini timer (`veh+0x3D74`), which is time-based and ignores
  these HDV values.
- Verdict: **candidate for a native player BRAKES family.** Either the game
  already simulates disc wear for the player (find the state + updater), or we
  tick it ourselves from these HDV numbers under our own system. One focused
  investigation decides which. Do not promise before that check.

## 2. Fuel — confirmed live link, per-car tank size

- `FuelRange=(1.0, 1.0, 140)` + `FuelSetting=64` → the physics fuel field
  `0x7C75C4` is seeded as `setting × step + base` = **65 L for this Ferrari**
  (`sub_43C250:421` reads exactly these base/step/count/index slots at
  `+0xCB0/+0xCB8/+0xCC0/+0xCC4`). This also settles the earlier unit question:
  fuel is in **litres**; the starvation flicker (`sub_447B10`) covers the last
  ~0.9 L.
- `FuelTankPos`, `FuelTankMotion` — tank location and slosh model.
- Verdict: our planned fuel-system failure can be per-car configurable
  (tank size), and the whole effect chain (flicker → dead pedal → ReasonCode 9
  → pit refuel recovery) is game-native.

## 3. Gearbox — per-car timing and clutch data

- `UpshiftDelay=0.014`, `DownshiftDelay=0.014`, `DownshiftClutchTime=0.096`,
  `UpshiftClutchTime=0` — real per-car shift timing (candidate seed for the
  parked "delayed shift" feature).
- `ClutchTorque=1014.7`, `ClutchFriction=7.023`, `ClutchInertia`,
  `BaulkTorque=360`, `ClutchWear=0.0` — clutch model inputs; `ClutchWear` is
  likely another unused/frozen token (worth one check, like the brake wear).
- `GearFile=gear_ratios.ini`, `ForwardGears=7`, per-gear settings — already
  used (max gear); ratios available for per-car gearbox flavour.

## 4. Engine — no lifetime data here

- `[ENGINE] Normal=Engine` just points at `engine.ini`. No lifetime keys in
  the HDV (consistent with the earlier finding) — the engine.ini pair remains
  the only lifetime source.

## 5. Supporting data usable for authored effects

- `RadiatorRange/Setting`, `RadiatorDrag/Lift` — cooling vs aero tradeoff; the
  radiator cooling term is already part of the player's engine thermal model.
- `BrakeDuctRange/Setting`, `BrakeDuctDrag/Lift` — brake cooling vs aero (feeds
  the brake model if it runs).
- Suspension springs/dampers/travels, `Mass`, `Inertia` — could scale our
  bend severity or crash impulses per car (authored mapping, clearly ours).

## Recommendation (scope stays basic)

1. **Brakes** — best new family candidate; one investigation (find disc-wear
   state/updater or drive it ourselves from HDV values).
2. **Fuel** — build the already-designed fuel-system fork, per-car tank size
   from `FuelSetting`.
3. **Gearbox delay** — when the delayed-shift investigation happens, seed the
   timing from `Upshift/DownshiftDelay`.
4. Skip variable-intake-style exotica.

## 6. Car-folder deep dive — Ferrari 2002_F2002 (2026-09-28)

Every file in `SeasonData\Vehicles\Ferrari\2002_F2002` reviewed:

| File | Finding |
|---|---|
| `2002_Ferrari.hdv` | Brake wear/fade model, fuel, gearbox timing, suspension — §1–§5 above |
| `engine.ini` (full) | **Stall/starter system** below; `RevLimitRange/Setting/Logic`; `FuelConsumption` coefficient; `LaunchEfficiency`; thermal + lifetime blocks (already known) |
| `2002_ferrari01/02.veh` | Graphics/identity only (HDVehicle link, sounds, head physics) — nothing mechanical |
| `2002_FERRARI01_oph.ini` | Third-party `oph.dll` plugin (helmet/rain settings) — not a failure surface |
| `gear_ratios.ini` (shared, `SeasonData\Vehicles\`) | Global ratio list indexed by garage settings; nothing failure-related |
| `2002_Bridgestone.tbc` | **Tyre wear/temperature model per compound** (see below) |
| `f1sounds.sfx` | Sounds only — but includes `VS_*_ENGINE_STARTER` / `ENGINE_STARTING` cranking samples |
| `TEAM.MAS`, bitmaps/jpg | Graphics archives — nothing |
| `pace_probe_standings_log.txt` | Safety-Car-mod log artifact, not car data |

### 6.1 Missed mechanic: STALL + STARTER (player-facing, native)

- `engine.ini`: `OnboardStarter=0` ("whether vehicle restarts when stalled")
  and `StarterTiming=(1.4, 0.4, 1.5)` (crank average, variance, sound blend).
- `sub_43C250:581` copies `OnboardStarter` into the player physics object
  (`physics+7436`); `VehicleActive_00447160` uses it in the engine-running
  predicate alongside life/fuel/ignition.
- The **starter is a bound player control** (`PlayerOptionsInitializer:1406`
  registers `ControlStarter`, index 89); the start function is `sub_4471F0`
  (sets the ignition byte, lifts rpm) and is also reachable from the pit path
  (`0x4A5E10`).
- The UI has an "Engine stalled" string and the sfx has cranking sounds.
- **Implication:** a recoverable "stall" engine fault (ignition cut that the
  driver must restart with the starter, costing ~1.4–2 s of cranking) is
  fully native — a good third tier between our cutout (80–280 ms flicker) and
  terminal seizure. Per-car config: cars with `OnboardStarter=1` behave
  differently (auto/self restart semantics), so any implementation must
  respect that byte rather than assume.

### 6.2 Tyre wear/temperature model (found, future family)

`2002_Bridgestone.tbc` defines per compound **and per axle**:
`WearRate`, `AIWear`, `Heating=(rolling, friction)`,
`Transfer=(road, static air, moving air)`, `OptimumTemp`, pressure targets.
The vehicle carries per-wheel tread/puncture fields. This is a separate
product family (tyres), not engine/gearbox/suspension — noted, out of current
scope unless the owner wants it.

### 6.3 Smaller knobs confirmed

- `RevLimitRange/Setting/Logic` — real per-car rev limit (garage setting);
  the parked "rev-ceiling drop" idea has a concrete data source.
- `FuelConsumption=2.955e-005` — per-car burn coefficient; controls how fast
  a fuel-system failure empties the tank.
- `OnboardStarter`, `StarterTiming`, `EngineBrakingMap`, `LaunchEfficiency` —
  per-car behavior knobs, all parsed and player-reachable.

## Evidence

- `2002_Ferrari.hdv` (`Documents\F1 Challenge V10\...`) lines 267-311 etc.
- `PerWheelBrakeSuspensionDataLoader_004f2c60.c` (token→field mapping),
  `sub_43C250_0043c250.c:419-433` (fuel seed vs `FuelRange`) and `:581`
  (OnboardStarter → physics+7436),
  `VehicleActive_00447160.c` (stall/starter predicate),
  `PlayerOptionsInitializer_004aa270.c:1406` (ControlStarter binding),
  `EngineSectionParser_004f3c80.c` (engine block layout),
  `2002_Bridgestone.tbc` (tyre wear/temp), `f1sounds.sfx` (starter sounds),
  `CONFIG_SCHEMA.md` rows for the brake/clutch/driveline/starter tokens.
