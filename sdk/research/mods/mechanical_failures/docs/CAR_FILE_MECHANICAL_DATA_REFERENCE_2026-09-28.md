# Car-file mechanical data reference (player) — 2026-09-28

**Purpose:** one consolidated source for every car-file value relevant to
player mechanical failures: where it lives, whether the game wires it, and how
our mod can use it. Complements (does not replace) the records in
`LIFETIME_SEEDS_INVESTIGATION_2026-09-27.md`,
`LIFETIME_SEED_CORRELATION_RACE_2026-09-28.md`,
`HDV_MECHANICAL_LINK_INVENTORY_2026-09-28.md`,
`PLAYER_SUBSYSTEM_FAILURES_DESIGN_2026-09-27.md`.

## 1. File map — what to read for what

| File | Contains (mechanical) |
|---|---|
| `engine.ini` | Lifetime pair; thermal block; rev limit; fuel burn; **stall/starter flags** |
| `*.hdv` | Per-wheel **brake wear/fade/failure**; suspension springs/dampers/travel; mass/inertia; **fuel range/tank**; gearbox clutch + shift timing; radiator/brake ducts (cooling vs aero) |
| `*.tbc` | Per-compound, per-axle **tyre wear + temperature** model |
| `gear_ratios.ini` (shared) | Global ratio list indexed by garage settings |
| `*.veh` | Links only (hdv, sounds, graphics) — no mechanical data |
| `*.sfx` | Sounds, incl. starter cranking (`VS_*_ENGINE_STARTER`) |
| `SeasonData\damage.ini` (global) | Collision thresholds incl. engine seizure, wheel bend/detach, wing/part detach |
| `*.rcd` (driver, AI) | `StartStalls`, ability fields — AI-only |

## 2. Proven wiring (what the game actually does)

| Source value | Consumer | Player status |
|---|---|---|
| `engine.ini LifetimeAvg/Var` | AI: 4 counters (`veh+0x3D70/74/78/7C` = engine/brakes/gearbox/suspension) = `Normal(3A,3V)/raceDivisor`, tick 1.0/s, AI-only gate | AI: live. **Player: seeded but FROZEN** (gate `veh+0x32C8=0`); values match the ACTIVE tree live-verified |
| same pair | Player engine life `0x7C91DC` = `Normal(A,V)/raceDivisor` (modes 2–3); drain scaled by RPM/oil-temp curves + stress | **Live** (0.14–0.66/s measured) |
| `LifetimeEngineRPM/OilTemp` | Player-life drain curves only (not the AI counters) | Live |
| `RevLimitRange/Setting/Logic` | Garage rev limiter | Live (fields exist per car) |
| `FuelConsumption` | Burn coefficient feeding the physics fuel field | Live |
| `hdv FuelRange/FuelSetting` | Seeds physics fuel (`0x7C75C4`, litres) — Ferrari 2002 = 65 L | **Live** (starvation flicker `sub_447B10`, code 9, pit refuel clears) |
| `hdv FuelTankPos/Motion` | Tank CG + slosh | Live (physics) |
| `hdv` per-wheel brake block | `BrakeWearRate`, `BrakeFailure=(avg,var)`, `BrakeOptimumTemp`, `BrakeFadeRange`, `BrakeHeating/Cooling`, `BrakeDuctCooling` — parsed by `PerWheelBrakeSuspensionDataLoader` | **Wear consumer unverified** → investigation |
| `hdv` driveline block | `ClutchTorque/Friction/Inertia/Wear`, `BaulkTorque`, `Upshift/DownshiftDelay`, `DownshiftClutchTime`, gear file | Parsed; shift timings live (feel); `ClutchWear` unused? |
| `hdv` suspension per wheel | Springs/dampers/travels/bump stops | Live (physics) |
| `engine.ini OnboardStarter/StarterTiming` | Stall/restart: physics+7436 flag; starter control (bound, index 89); `sub_4471F0` cranks; "Engine stalled" UI + cranking sounds | **Live** (player can stall + crank) |
| `*.tbc` WearRate/Heating/OptimumTemp/… | Tyre wear + temps per compound (vehicle tread/puncture fields) | Live (tyres = separate family, future) |
| `damage.ini Engine=` | Collision impulse → instant engine seizure | Live |
| `damage.ini WheelBend/WheelDetach/PartN` | Collision wheel/part damage | Live |

## 3. Status legend per value

- **Live** — game uses it for the player today.
- **Frozen** — value exists on the player (seeded) but the game never consumes
  it (the four AI counters).
- **Unused/unverified** — parsed but no confirmed consumer (brake wear fields,
  `ClutchWear`).
- **Usable** — our mod can read it for parameters/weights.

## 4. How our mod uses this (player-only plan)

**Master clock stays ours** (already built): per-family chance, one roll per
session, type weights, onset/guard rules, never auto-retire, our presentation
(HUD/radio/clunk/stages).

**Flavor layer (proposed, cheap).** Read the player's frozen per-session
counters (they are car-scaled draws) and/or `LifetimeAvg` at race start and
use them as weights inside our system — fragile engine → engine faults more
likely; low gearbox counter → gearbox flavor. Keeps timing authority with us;
gives per-car character without unfreezing anything.
**IMPLEMENTED 2026-09-28 (dev, off by default):** all three DLLs scale the
auto-roll chance by `clamp(fieldMean / playerCounter, 0.50, 2.00)` per family
using the frozen counters — `CAR_FLAVOR_WEIGHTING_2026-09-28.md`.

**Usage layer (proposed, phase 2).** Weight escalation by driving: shift count
(gearbox), time above oil-temp/RPM curves (engine), kerb strikes (suspension),
heavy braking (brakes family, if built). All from fields/hooks we already
have or are one investigation away.

**Not doing:** unfreezing the AI timers as our clock — the seeds are calibrated
so they rarely expire inside a race, give no presentation, and model no
gearbox behavior. No variable-intake-style subsystems.

## 5. Open investigations (ordered)

1. Brake wear simulation — find disc-thickness state/updater or drive it
   ourselves (would unlock a native player BRAKES family).
2. Stall tier for the engine family — confirm `OnboardStarter` semantics
   (self-restart vs manual crank) per car, then implement stall as a
   recoverable fault (one ignition write + starter crank time).
3. Rev-limit live fields — if writable, the "rev-ceiling drop" failure has a
   data source.
4. Tyre family decision (out of current scope; data ready if wanted).
