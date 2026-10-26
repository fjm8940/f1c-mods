# Lifetime seeds — does the player get gearbox/suspension/brakes/engine values? (2026-09-27)

**Question (owner):** investigate whether the game gives `LifetimeAvg` /
`LifetimeVar` values to the player for gearbox / suspension / engine / brakes
etc., as it does with AI drivers.

**Answer: there is exactly ONE lifetime pair per car and it is the ENGINE's.**
AI and player cars both get four counters seeded from it; only the AI's tick.
**Live correction 2026-09-27 (evening):** an earlier version of this record
said the player gets no gearbox/suspension/brake counters at all. That is
wrong — the player's vehicle holds seeded values in all four slots too; they
are simply **frozen** (never tick). Measured data in §3.

## 1. The data source: one pair per car, engine.ini only

- Tokens: `LifetimeEngineRPM`, `LifetimeOilTemp`, `LifetimeAvg`, `LifetimeVar`
  in the car's `engine.ini` `[ENGINE]` section (sample:
  `SeasonData\Vehicles\Arrows\1995_FA16\engine.ini` — `LifetimeAvg=10324`,
  `LifetimeVar=3517`).
- 284 files in the live game install contain `LifetimeAvg`; all of them are
  `engine.ini` files.
- **Zero** hits for any per-subsystem token
  (`LifetimeGearbox` / `LifetimeSuspension` / `LifetimeBrake` /
  `GearboxLifetime` / …) across all `.ini` / `.hdv` / `.veh` files.
- Parsed into the engine config block at `+0x1330` (Avg) / `+0x1338` (Var)
  (`CONFIG_SCHEMA.md`; `sub_565AA0`). `RandomNoiseValueGenerator_004b3ce0` is
  a standard normal, so `Var` is a **sigma** (runtime-proven, 20k samples).

## 2. AI drivers: four subsystem counters, all from the engine pair

`EngineWearFailureSimulation_004ee150.c`:

- Writes four slots on the vehicle: `veh+0x3D70/74/78/7C` = **engine /
  brakes / gearbox / suspension** (ReasonCodes 1 / 5 / 2 / 4).
- Each = `Normal(3 × LifetimeAvg, 3 × LifetimeVar)` — **four independent
  draws from the same engine values** (lines 22–36). Divided by
  `flt_8ACF9C` when `SessionType == 7 && mode == 2`; mode gate 1–2; modes 0/3
  leave the slots at FLT_MAX.
- Ticked at 1.0/s by `FuelConsumptionAndWear_004ed960`, which runs only for
  cars with `veh+0x32C8 == 1` (WearTickGate, CONFIRMED: AI = 1, player = 0).
- **Ticking is pure clock time — no usage conditions** (code read 2026-09-28,
  `FuelConsumptionAndWear_004ed960.c:112-126`): gate = `RacePhase > 4` (green);
  then each counter decrements 1.0 per game-second with no speed, distance,
  throttle, pit or finish checks. An AI car parked for the whole race still
  accrues wear and can fire a failure; counters freeze once negative, and a
  ReasonCode is written only if none is set. Pre-green (grid, `RacePhase 4`)
  nothing ticks — observed live as zero drain on the paused grid.

So the AI's "gearbox lifetime" and "suspension lifetime" are not gearbox or
suspension data at all — they are the engine's `LifetimeAvg/Var` with an
independent noise draw per slot.

**No other counters derive from the pair** (sweep 2026-09-28): the only
consumers of `LifetimeAvg/Var` in the whole codebase are this seeder and the
player's engine-life seed in `sub_43C250`. The player's life *drain rate* is
additionally modulated by the `LifetimeEngineRPM` / `LifetimeOilTemp` curve
tokens (plus stress); the AI counters ignore those curves (flat 1.0/s).
Lookalike math elsewhere (`int × Var + Avg`) reads a different config block
(wheel/suspension range settings — same offsets, different struct).

## 3. Player: one live consumer (engine); the other three are seeded but frozen

`sub_43C250_0043c250.c` (player vehicle initialisation; `this` = the static
player object `0x7C7428`, `HIDWORD(a2)` = engine config block):

- Line 531: `life = Normal(LifetimeAvg, LifetimeVar)` — **no ×3**.
- Line 532: writes it to `this+7604` = `0x7C91DC` (the player engine-life
  countdown; the same field docs/46 called "component timer veh+0x1DB4" —
  `0x1DB4` hex = 7604 decimal, one field, two names).
- Line 533–534: a non-positive draw is clamped to `LifetimeAvg`.
- Lines 537–540: divided by `flt_8ACF9C` in modes 2 **and 3**, race session
  only. Mode 0 writes the −1 / +FLT_MAX sentinel (line 554).
- No other lifetime-seeded field exists in the player initializer. The
  per-wheel block (`this+1736`, stride 1124) is only scaled by the race
  divisor, not seeded from Avg/Var.

The four AI-style slots are **present on the player's vehicle as well** —
`sub_4EE150` has no role gate of its own, and the live read below shows the
values — but they never tick for the player. The 2026-07-30 FACT covered the
ticker only (zero player entries in `FuelConsumptionAndWear`; poke latches
with no failure).

**Live correction 2026-09-27 evening** (Frida reads, game PID 29588, warmup,
failure mode 2, `RaceActiveFlag` 3; player vehicle `0x5EC6D8`, AI slot 1
`0x5F0F98`):

| Slot | read 1 | read 2 (+43 s) | delta |
|---|---|---|---|
| player engine `+0x3D70` | 15190.233 | 15190.233 | **0.0** |
| player brakes `+0x3D74` | 15341.913 | 15341.913 | **0.0** |
| player gearbox `+0x3D78` | 12203.521 | 12203.521 | **0.0** |
| player suspension `+0x3D7C` | 11935.637 | 11935.637 | **0.0** |
| AI1 engine `+0x3D70` | 2022.375 | 1979.375 | −43.0 (1.0/s) |

- Gates: player `veh+0x32C8 = 0`, AI = 1.
- All four AI slots drained in lockstep at exactly 1.0/s; all four player
  slots were bit-identical 43 s apart.
- **Replicated under real driving:** over the next 203 s (fuel 35.5 → 22.6 L,
  player engine life 4284.2 → 4226.2 — the car was genuinely driven), the
  player's four values stayed bit-identical while AI1 drained exactly −204.0
  on every slot. Nothing in-session touches the player's counters.
- The player's magnitudes fit `Normal(3×Avg, 3×Var)` for the tuned 2002 cars
  (3 × 3806..5290 = 11.4k..15.9k, undivided in a non-race session): the
  player's four counters are game-seeded, per-session, car-scaled — **and
  inert**. The tick gate excludes the player, not the seeder.
- Writer attribution is inference: the formula matches the seeder, but no
  seeding call for slot 0 was intercepted in this pass.
  (The apparent second player "component timer" in docs/46 is the same
  `0x7C91DC` field — docs/46 must not be cited for a second timer.)

## 4. Summary table

| Subsystem | Car file token pair | AI counter | Player counter |
|---|---|---|---|
| Engine | `[ENGINE] LifetimeAvg/Var` | `veh+0x3D70` = Normal(3A,3V), ticks 1.0/s | `0x7C91DC` = Normal(A,V) — live |
| Brakes | *(none)* | `veh+0x3D74` = Normal(3A,3V), ticks | `veh+0x3D74` — seeded, frozen |
| Gearbox | *(none)* | `veh+0x3D78` = Normal(3A,3V), ticks | `veh+0x3D78` — seeded, frozen |
| Suspension | *(none)* | `veh+0x3D7C` = Normal(3A,3V), ticks | `veh+0x3D7C` — seeded, frozen |

Player's other mechanical paths are unrelated to lifetime seeds: engine
stress/overheat (`EngineStressSimulation_0043ef20`) and collision damage
(`damage.ini`). The frozen gearbox/suspension/brake counters are game-seeded
data the game never consumes for the player — nothing in the game turns them
into a failure.

## 5. Implication for Mechanical Failures

- A per-car reliability scaling for our auto trigger can only honestly use
  the **engine** lifetime values: read `[ENGINE] LifetimeAvg/Var` from
  `engine.ini`, or simpler at runtime, read the already-seeded player life at
  `0x7C91DC` (it is `Normal(Avg,Var)`, idempotent per session).
- Any per-subsystem lifetime numbers (e.g. "this car has a fragile gearbox")
  would be **mod-authored data**, not a restoration of game data, and must be
  labelled that way. A natural design: derive our chance scaling from the
  engine's `LifetimeAvg` (lower = more fragile = higher chance) plus our own
  per-car/team modifiers if wanted.
- Do not promise "uses the game's gearbox lifetime" anywhere: the game seeds
  a number per subsystem but never consumes it for the player (frozen seed).
  Optionally the mod could consume those frozen seeds itself one day, but
  that would be our mechanism, not the game's.

## Evidence / repro

- `EngineWearFailureSimulation_004ee150.c` lines 11–38 (four slots, same pair).
- `sub_43C250_0043c250.c` lines 531–565 (player single seed, modes, sentinel).
- `findings.jsonl`: `ai-only-mechanical-failure-timers` (FACT),
  `player-vs-ai-lifetime-seeding-exact` (CONFIRMED),
  `randomnoise-4b3ce0-normal-RUNTIME-PROVEN` (FACT),
  `player-engine-life-0x7c91dc-fires` (FACT).
- Install scan: 284 `engine.ini` files with `LifetimeAvg`; 0 per-subsystem
  lifetime tokens in `.ini`/`.hdv`/`.veh`.
- Live reads 2026-09-27 evening via Frida on PID 29588 (player vs AI slot
  comparison, §3). The earlier pass had no live session (game closed) and
  relied on prior recorded proofs.

State: read-only investigation; no code, files or game state changed.
