# 46 — Player-Specific Mechanical Failures: which engine.ini / .plr knobs actually affect the human car

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`, image base 0x400000, non-ASLR)
**Date:** 2026-07-31 · **Author:** corpus session · **Status:** STATIC (IDB-verified), **partially CORRECTED by live testing the same day — read §0 first**

## 0. Errata (live session 2026-07-31)

This paper was written from static analysis and prior-art pokes. It was then
runtime-validated. Four claims did not survive. The two-model thesis and the
AI/player split did.

| § | Claim as written | Verdict |
|---|---|---|
| 2, 4.4, 5, 6 | player model fields are `veh+0x19C` / `veh+0x1D4C` / `veh+0x1DB4` / `veh+0x1DE4…` | **WRONG BASE.** `EngineStressSimulation`'s `this` is the static global **`0x7C7428`**, not `veh+0xC`. Frida: 2001 calls in 5 s, **one** distinct `ecx`. Control: `veh+0x1DB4` sat at 0.0 and never moved while `0x7C91DC` ticked. The offsets are right *relative to `0x7C7428`* — `0x7C7428+0x1DB4 = 0x7C91DC` |
| 4.3 | slot 1 = gearbox, slot 3 = tyre damage | **WRONG.** Slot 1 = **brakes** (RC 5), slot 3 = **suspension** (RC 4). Slot 2 = stuck gear, as written |
| 4.4 | component timer drains at "≈2–2.5/s" | **WRONG.** 0.14/s idle, 0.28/s at stress 0.1415, 0.635/s at stress 0.6 |
| 4.4 | player gets no commentary *because `sub_41AC50` rejects the player* | **RIGHT CONCLUSION, WRONG REASON.** `sub_41AC50` is entered **0 times** during a player engine failure — never called. The internal guard never gets the chance to reject anything |
| 4.1 | mode 2 "timescaled" = "÷ total race laps", both models | **HALF RIGHT.** The player really is scaled — not exempt — but the divisor is `fullRaceLaps / actualRaceLaps`, not the lap count. Measured 1.0 at 53 laps/100 % and **2.03846 = 53/26 exactly** at 26 laps/50 %; player life fell 12500.2 → 6994.59 across the two. `TotalRaceLaps` is the other global `0x8ACF8C`. Mode 2 thus equals mode 1 at full distance and only bites below it |

Not in this paper at all, and the thing a modder actually wants: **stress is the
power channel, the emitter banks are cosmetic**, proven by an A/B where zeroing
stress alone restored full power with the smoke still running. See
`mods/extended_vanilla_features/docs/ENGINE_FAILURE.md`.

Corrected map, all live-verified:

| address | field |
|---|---|
| `0x7C91DC` | engine life, double, seconds |
| `0x7C91D4` | stress, double, cap 0.6 — **the power channel** |
| `0x7C920C` / `0x7C9214` / `0x7C921C` | emitter banks: white smoke / black smoke / flames. Healthy = **0.0**, fire negative |
| `0x7C9081` | ignition / engine-running byte |
| `veh+0x4068` | ReasonCode (unchanged — this one really is vehicle-relative) |

Evidence: `findings.jsonl` rows `stresssim-this-is-static-global`,
`player-engine-life-0x7c91dc-fires`, `stress-is-power-banks-are-cosmetic`,
`torque-cut-formula-quantified`, `stress-feeds-back-into-life-drain`,
`player-engine-failure-fires-no-commentary`, `ai-wear-slot-components-corrected`.

---

## Abstract (TLDR)

- The game runs **two completely separate mechanical-failure models**: one for the **AI cars** and one for the **player's car**. They are driven by the same config tokens but through different code paths, and the player's model is much weaker.
- The **AI model** is a set of four wall-clock countdowns (seeded from `LifetimeAvg`/`LifetimeVar`) that drain 1 s per real second and fire "random" component failures (engine power loss, gearbox, stuck gear, tyre damage). These countdowns **never tick for the human car** — the field is frozen for the player.
- The **player model** is the engine-thermal/stress simulation: coolant and oil temperature, RPM-based engine durability, and a component-failure timer also seeded from `LifetimeAvg`/`LifetimeVar`. It only runs through the full-physics step, which only the player's car gets. In a normal race it never fires (the timers are seeded in the tens of thousands of seconds); the player's realistic engine killer is **overheating**, which is a separate thermal path.
- The `.plr` **Failure Rate** setting (0–3 per game mode: CURNT/PRACT/QUICK/GPRIX/CHAMP/MULTI/RPLAY) is a single global switch (`EngineWearMode`) that gates **both** models: 0 = no mechanical failures, 1 = normal (raw lifetime), 2 = timescaled (lifetime ÷ race length), 3 = season (AI: scripted race events only; player: timescaled).
- `FuelConsumption`, `FuelEstimate`, `EngineInertia`, `LaunchEfficiency`, the four engine-map settings and the oil/coolant thermal tokens all feed the same physics/thermal block, but only the **thermal tokens** and the **lifetime tokens** feed the failure logic itself. The rest are performance/lap-time knobs.
- Result for a modder: to give the **player** visible mechanical failures, the AI countdown approach does not work (player never ticks those fields). The workable levers are the player's stress/thermal model fields, a code hook on the player physics step, or the season-event scripted-failure table.

## 1. Introduction

Research question: which of the engine.ini `[ENGINE]` tokens and which `.plr` `[MECHANICAL FAILURES]` settings actually control **mechanical failures for the player's own car**, and through what code path? The game ships seven per-session "Failure Rate" settings and a large engine config block; community lore treats them as a single system, but the binary splits the implementation into an AI-only countdown model and a player-only stress/thermal model. This paper maps the tokens, the two models, and the player/AI split so a modder knows exactly which knob to turn — and which approach will not work.

Scope: static (decompiled) analysis of the engine/failure pipeline plus live poke evidence from prior corpus sessions. Runtime experiments on the player model are marked PENDING where not yet performed.

## 2. Method: static analysis

Tool chain: IDA + Hex-Rays decompilation of `F1 Challenge Online.exe`, cross-referenced through the corpus fact tables (`skeleton_facts_all.json`, `strings_index.json`, `config_schema.json`, `name_index.json`). Functions cited by address/name. All offsets in the vehicle physics object are absolute offsets in the vehicle struct (`veh+N`); the decompiler's `this` in the wear functions is `veh+0xC`, so a decompiled index like `this[3929]` resolves to `veh+0x3D70` (proven by live poke in prior art).

Config flow examined:

- `EngineSectionParser` (0x004F3C80, source file `vehload.cpp`) — reads every `[ENGINE]` token of a vehicle `.hdv` and stores it in the vehicle engine block.
- `EngineMapSetupFieldDefinitions` (0x004F3C10) — reads the four map tokens + `RevLimitSetting`.
- `sub_43C250` (0x0043C250, `hdvehicle.cpp`) — vehicle physics setup: copies the parsed engine block into the per-car physics engine object (seeds lifetime, thermal constants, component-failure timer).
- `EngineStressSimulation` (0x0043EF20) — per-frame engine durability + coolant/oil thermal + component failure.
- `EngineWearFailureSimulation` (0x004EE150) — per-car seeding of four AI countdown slots.
- `FuelConsumptionAndWear` (0x004ED960) — per-AI-car fuel burn, tyre wear, countdown drain and failure dispatch.
- `VehiclePhysicsModeDispatcher` (0x004E4930) / `VehiclePhysicsIntegrationStep` (0x00441650) — the player-vs-AI physics split.
- `sub_424080` (0x00424080) — AI driver init incl. pre-race scripted failures and season race events.
- `sub_41AC50` (0x0041AC50) — mechanical-failure commentary emitter (AI-only).
- `PlayerOptionsInitializer` (0x004AA270) / `sub_4AA1E0` (0x004AA1E0) — the seven `.plr` Failure Rate settings.
- `sub_4F1FE0` (0x004F1FE0) — retirement handling shared by player and AI.

## 3. Method: runtime verification

Prior corpus sessions (2026-07-25 and 2026-07-29) already poked parts of this system live:

- `veh+0x3D70` (EngineLifeRemaining) on a healthy **AI** car read 20,000–41,000 s and drained at exactly 1.000 s/s for all 21 simulated cars over 65 minutes. Writing 0.5 into that slot fired ReasonCode=1 within 0.2 s, wound the engine down to 0 rpm, and 5 s later the game itself set FinishState=2 + RetiredFlag=1 — a genuine failure, no forged state. (FACT)
- The same session proved the four countdown slots and the oil readout are **never updated for the human car**: a player engine blew up with its countdown frozen at 23,245.8 and oil stuck at the previous race's value; the player died by the overheat path, not the countdown model. (FACT)
- A second investigation (2026-07-29) confirmed the player's `veh+0x4068` ReasonCode is written by suspension/steering damage paths (ReasonCode=4) and that the tyre-puncture fields, wing flags and countdown slots stayed frozen in control reads.

Planned/not yet run: freeze a player's component-failure timer (`veh+0x1DB4`) down to seconds and observe the player's engine fail — the static path predicts ReasonCode=1 + physical failure, but this specific player-car experiment is PENDING.

## 4. Results

### 4.1 The `.plr` Failure Rate settings → a single mode switch

Each `.plr` has a `[MECHANICAL FAILURES]` section with seven keys, one per game mode:

```
CURNT PRACT QUICK GPRIX CHAMP MULTI RPLAY
```

Each value is 0–3: **0 = none, 1 = normal, 2 = timescaled, 3 = season**. The seven settings are built as UI records by `PlayerOptionsInitializer` at its struct +0x5C8, stride 36 bytes (current value, min 0, max 3, default 2, token string built as `"<MODE> Failure Rate"` by `sub_4AA1E0`). At session start the current mode's record is applied to the single global **`EngineWearMode` = `dword_7CEA5C`** (DERIVED name; value semantics observed: mode gating of both failure models). The exact writer of `0x7CEA5C` is runtime-only (no static writer; OPEN — see Discussion). `dword_7CEA5C` is read by both failure models and by the AI init (`sub_424080`), the vehicle physics setup (`sub_43C250`) and the pre-race scripted-failure code.

Mode semantics confirmed across readers:

| Mode | AI countdown model (`4EE150`) | Player stress model (`sub_43C250` seeding) | Pre-race scripted failures (`sub_424080`) |
|---|---|---|---|
| 0 none | slots stay FLT_MAX → never fire | component timer = near-infinite sentinel → never fires | disabled |
| 1 normal | lifetime × 3 ± noise (seconds) | lifetime ± noise (seconds) | random start-damage (90 % code 5 / 10 % code 4) |
| 2 timescaled | ÷ total race laps | ÷ total race laps; tyre-wear inits × laps | random start-damage |
| 3 season | slots FLT_MAX (random off) | ÷ total race laps | season race events (component + lap per car) |

### 4.2 Engine.ini tokens → what each controls

All parsed by `EngineSectionParser` (offsets relative to its object, which the offset cross-analysis shows coincides with the vehicle engine block — `config_schema.json` extraction). `sub_43C250` copies the thermal/lifetime block into the per-car physics object.

| Token | Parser offset | Becomes / consumed by | Failure relevance |
|---|---|---|---|
| `FuelConsumption` | 0x674 | fuel burn rate in `4ED960` (AI) and the physics fuel model | indirect: running out → ReasonCode 9 |
| `FuelEstimate` | 0x67C | fuel estimate (HUD/strategy) | none |
| `EngineInertia` | 0x684 | engine rotational inertia (physics) | none |
| `LaunchEfficiency` | 0x6A4 | launch control (physics) | none |
| `RevLimitSetting` (+`RevLimitRange`) | via `4F3C10` → engine-map block | max rpm; higher limit → higher sustained rpm | **indirect**: rpm drives durability drain in the stress model |
| `EngineMapSetting` / `EngineMapRange` | via `4F3C10` → +0x15B0 block | engine power map index | **INERT — see `docs/47`.** Not "performance only": `sub_43C250` never resolves this block, and the shipped engine.ini says `(unimplemented)`. Changing it does nothing |
| `EngineBrakingMapSetting` / `EngineBrakingMapRange` | via `4F3C10` → +0x15F0 block | engine-braking map | **LIVE — see `docs/47`.** Resolves to `0x7C9078`; zeroing it roughly **doubles** coasting deceleration (driver-in-the-loop A/B). A real driving knob, not "none" |
| `OptimumOilTemp` | 0x12C8 (+273.16 K) | → physics +0x1D54 = max coolant/oil threshold | **yes** — the overheat ceiling of the player model |
| `CombustionHeat` | 0x12D0 | → physics +0x1D5C (thermal dissipation) | **yes** — heat-out rate |
| `EngineSpeedHeat` | 0x12D8 | → physics +0x1D64 (rpm→heat conversion) | **yes** — how fast revs heat the engine |
| `OilMinimumCooling` | 0x12E0 | → physics +0x1D6C (ambient heat transfer) | **yes** |
| `RadiatorCooling`, `WaterMinimum`, `OilWaterHeatTransfer` | 0x12E8 / 0x12F0 / 0x12F8 | → physics +0x1D7C / +0x1D84 / +0x1D94 | **yes** — remaining thermal block |
| `LifetimeEngineRPM` | 0x1310 / 0x1318 (rad/s) | rpm range at which lifetime is measured | **yes** — the rpm bracket for durability |
| `LifetimeOilTemp` | 0x1320 / 0x1328 (K) | oil-temp bracket for durability | **yes** |
| `LifetimeAvg` | 0x1330 | seeds the component-failure timer (both models) | **yes** — primary failure knob |
| `LifetimeVar` | 0x1338 | noise spread on that timer | **yes** |
| `StarterTiming` | 0x135C / 0x1364 / 0x136C | starter motor behaviour | none |

### 4.3 Model A — the AI countdown model (does NOT run for the player)

`EngineWearFailureSimulation` (0x004EE150, called from the suspension/wheel physics setup) seeds four countdown slots. The decompiled `this` is `veh+0xC`, so decompiled slots `this[3929..3932]` resolve to `veh+0x3D70 … +0x3D7C` (FACT, live-verified):

- mode 1: `LifetimeAvg×3 + noise×LifetimeVar×3` (seconds); mode 2: divided by total race laps; modes 0/3: left at FLT_MAX (no random failures).

`FuelConsumptionAndWear` (0x004ED960), called from `MainGameTickUpdate` once per game-second **only for cars whose `veh+0x32C8 == 1`** (0x32C8 = WearTickGate, CONFIRMED — AI cars read 1, the human driver reads 0), and gated on `RacePhase > 4` (`sub_4D21D0`):

1. Depletes fuel (`veh+0x3984`); at zero writes ReasonCode **9**.
2. Drains the four slots by 1.0/s; on expiry dispatches (live-poke confirmed):
   - slot 0 (`veh+0x3D70`, named EngineLifeRemaining): ReasonCode **1** — engine power-loss; the three power factors `veh+0x3D54/58/5C` are re-rolled to 1.0−noise×0.3 (0.7–1.0); gated on rpm ≥ 50 % of max.
   - slot 1 (`veh+0x3D74`): ReasonCode **5** — gearbox; factors `veh+0x3A3C/0x3A50` scaled by 0.03125; gated on gearbox health.
   - slot 2 (`veh+0x3D78`): ReasonCode **2** — stuck gear; byte `veh+0x39FA` set; fires MechProb commentary (below).
   - slot 3 (`veh+0x3D7C`): ReasonCode **4** — random tyre damage (+0.05 on one wheel's wear); fires MechProb commentary.
3. Mechanical-failure commentary (`sub_41AC50`, 0x0041AC50) is **AI-only**: it returns early when the car is the player's (`dword_7C54E8 != a1` guard), and only in a race session at race phase. EIDs 208 (generic), 211–217 (per current gear), 220 (random wear).

Live measurement: with stock config the slots sit at 20,000–41,000 s and drain 1.000 s/s — **no random failure can ever fire within a race** (53-lap race low: 10,400 s). Real AI failures come from the scripted pre-race assignments instead (section 4.5).

### 4.4 Model B — the player stress model (only the player runs it)

`VehiclePhysicsModeDispatcher` (0x004E4930) splits physics by the same gate:

- `veh+0x32C8 == 0` (player): `VehiclePhysicsIntegrationStep` → per-frame `EngineStressSimulation` (0x0043EF20).
- `veh+0x32C8 == 1` (AI): simplified `PhysicsIntegrationStateMachine` — no stress simulation.

`EngineStressSimulation` per frame:

1. **Durability** (`veh+0x19C`): drained by `damageScale × rpm_input × |rpm| × dissipation × dt`; at 0 writes ReasonCode **9**. Seeded at vehicle setup from a three-point curve interpolation in the vehicle data block (source offset 0xCB0 block; the exact config token feeding it is not yet identified — distinct from `LifetimeAvg`, which only seeds the component timer below).
2. **Coolant temperature** (`veh+0x1D4C`): heated by rpm (`EngineSpeedHeat`) and combustion (`CombustionHeat`), cooled by ambient transfer (`OilMinimumCooling`, `RadiatorCooling`), threshold `OptimumOilTemp`; when above threshold, thermal stress accumulates.
3. **Oil temperature** (`veh+0x1D8C`): similar, ceiling 373.16 K (100 °C); above it, extra stress.
4. **Component-failure timer** (`veh+0x1DB4`): seeded in `sub_43C250` as `LifetimeAvg + noise×LifetimeVar`, gated by EngineWearMode (0 → near-infinite sentinel; modes 2/3 → ÷ laps). The stress sim drains it at ≈2–2.5/s; at 0 it writes ReasonCode **1** and rolls one of three component states `veh+0x1DE4/1DEC/1DF4` to `1.0−noise×0.3`, then fires the MechProb commentary path (which is AI-only, so the player gets the physical effect and the ReasonCode but no commentary).

Practical consequence (FACT from live observation): with stock `LifetimeAvg`/`LifetimeVar` the player's component timer is tens of thousands of seconds — it will not fire in a race. The player's real engine-failure risk is **overheating** (coolant/oil stress), which the prior live session confirmed: a player engine blew up with the AI countdown frozen and the overheat/thermal path as the cause.

### 4.5 Scripted pre-race failures (`sub_424080`, AI driver init)

For every AI car at race start:

- Modes 1/2: random 10 % chance of starting with a pre-assigned failure (code 5, or 4 in 10 % of cases).
- Mode 3 (season): reads the season's race-event list; per event assigns a component (0 engine, 1 brakes, 2 gearbox, 3 suspension, 4 stall, 5 stall-DNS) and a **lap number** (`value ÷ total race laps`) stored at `veh+0x4148` (pair: type, lap). This is the only path that can realistically produce AI mechanical failures inside a race.

### 4.6 Player retirement path (shared, but gated)

`sub_4F1FE0` (0x004F1FE0) handles retirement for both. Player branch (FinishState==0) has three gates before the dwell timer can trip a retire: the global `dword_7CE814` (when non-zero the player branch returns early), a four-pit-stop completion check, and `RacePhase > 4`. The dwell threshold (`flt_7D0A8C`) is runtime-initialised, so it cannot be byte-patched (FACT). Prior corpus work proved the player retire-commentary clips are structurally unreachable by gameplay alone and were restored via a DLL hook.

## 5. Discussion

- **The single most important modding fact**: `veh+0x3D70..0x3D7C` countdowns are AI-only. Poking them on the player does nothing visible (frozen). The player's failure levers are `veh+0x19C` (durability), `veh+0x1DB4` (component timer, seeded from `LifetimeAvg`/`LifetimeVar`), the thermal block `veh+0x1D4C..0x1D94`, and the `veh+0x4148` scripted-failure table.
- **EngineWearMode writer unknown (OPEN)**: `dword_7CEA5C` has no static writer in the corpus (runtime-set). The `.plr` per-mode records are the source; the copy to the global is a runtime-only write not yet located. This does not change the mechanics, only the hook point for a "force mode" mod.
- **`LaunchEfficiency` offset discrepancy**: `config_schema.json` (extraction) says 0x6A4; one prose fill says 0x6BC. The extraction wins (tier: extraction); treat 0x6BC as a stale prose guess.
- **Player thermal model not yet poked as a failure trigger**: the overheat path was confirmed as a killer, but no experiment has deliberately driven the player's `veh+0x1DB4` to zero to confirm the exact player-visible failure. PENDING.
- The two models being split per gate also means **spectating** (which switches which car is "the player") moves the stress simulation onto the watched AI car — worth knowing when designing telemetry mods.
- `veh+0x4068` (ReasonCode) is shared: engine/lifetime/overheat writes land there, but it is never written by pit retirements (prior art), and wheels/wings write their own codes via separate paths (ReasonCode 6 accident, 4 suspension).

## 6. Conclusion

For the player's car: **mechanical failures exist, but only through the engine stress/thermal model** (durability `veh+0x19C`, component timer `veh+0x1DB4`, coolant/oil thermal block), gated by the `.plr` Failure Rate mode via `EngineWearMode`. The visible AI countdown slots are dead to the player. `LifetimeAvg`/`LifetimeVar` and the thermal tokens (`OptimumOilTemp`, `CombustionHeat`, `EngineSpeedHeat`, `OilMinimumCooling`) are the real player-side knobs; the engine-map, inertia, launch and fuel tokens are performance-only. With stock values nothing fires inside a race except scripted season events (AI) and overheating (player). A mod that wants visible player failures should target `veh+0x19C` / `veh+0x1DB4` seeding or the stress-sim hook, not the AI countdown fields.

## Appendix A — Corrections to prior notes/tools

- `FuelConsumptionAndWear_004ed960` fill labels `case 0/1/2/3` as "engine/gearbox/puncture/random wear". The dispatch switch actually writes ReasonCodes **1 / 5 / 2 / 4** for slots 0–3; the fill's ordering is cosmetic, the codes were confirmed live (0.5 on slot → ReasonCode 1).
- The corpus `EngineWearMode` name is "0/2/3" in one source; the `.plr` value range is 0–3 and mode 1 is observed in the AI seeding. Name updated to 0–3 semantics.
- A prior fill's `LaunchEfficiency → 0x6BC` conflicts with extraction `0x6A4`; extraction retained.

## Appendix B — engine.ini tuning notes, worked on the Williams 2001_FW23

Added 2026-07-31 from the live session. This is the practical "what do I edit" layer;
the mechanism it rests on is in §4 and `docs/47`.

### B.1 Torque curve

`RPMTorque=(rpm, coastTorque, powerTorque)`, 44 rows. Peak power torque **375.0 Nm at
15 500 rpm**, easing to 366.1 at 17 000 and 356.4 at 17 500, then collapsing through zero
around 20 000 (a modelled valve-float cliff). Coast torque runs −30.3 to −314.4.

The car's rev limit is `17400 + 5×100 = 17 900 rpm` — **past peak torque**. The top
limiter settings buy revs and lose torque.

### B.2 The lifetime tokens are a mean and a SIGMA

`LifetimeAvg=9136`, `LifetimeVar=3027.09`. `Var` is a **standard deviation**, not a
half-range: the seed is `Normal(Avg, Var)` and the generator was runtime-proven to be a
unit normal (20 000 samples, mean 0.0106, sd 1.0023 — see
`findings.jsonl: randomnoise-4b3ce0-normal-RUNTIME-PROVEN`). The community rule of thumb
"max ≈ avg + 3·var" is fair; about 0.2 % of draws exceed it.

Six observed player draws sat at z = −0.44, +0.91, +0.93, +1.11, +1.69, −0.05.

### B.3 The two wear curves, and an honest caveat

| token | value | documented meaning |
|---|---|---|
| `LifetimeEngineRPM` | `(17038.4, 384.6)` | base engine speed, and the range over which lifetime **halves** |
| `LifetimeOilTemp` | `(125.11, 2.93)` | same, for oil temperature |

Two things follow *if* the halving reading is right:

- **`RevLimitSetting` is an engine-life knob, not just a power knob.** Setting 0 (17 400)
  sits 361 rpm over the 17 038.4 base; setting 10 (18 400) sits 1362 over. That is roughly
  a **6× difference in wear rate** between the extremes.
- **Overheating is a cliff, not a slope.** `OptimumOilTemp` is 107.2 °C but the lifetime
  base is 125.11 °C and it halves every 2.93 °C. Oil temperature contributes almost
  nothing to wear until you are ~18 °C over optimum, then it goes vertical.

**CAVEAT — the halving model does not reproduce the measured drain.** Observed drains were
0.14/s idle, 0.28/s under load at stress 0.1415, 0.635/s at stress 0.6. Plugging 17 015 rpm
into the halving form predicts far more; plugging a realistic lap-average rpm predicts far
less. The exact curve function is **unverified** — do not tune against these two rows and
expect predictable timing until someone measures drain at held, known rpm.

### B.4 What to edit if you want failures to actually happen

**Not the `.PLR` failure rate.** With `LifetimeAvg` at 9136 s against a full race of
~4700–5400 s, seeded life exceeds race duration by ~2.5× **at every race distance**,
because mode 2 divides by `fullLaps/actualLaps` and so preserves the ratio (§4.1 errata).
No vanilla failure-rate setting blows an engine inside one race. That is period-correct:
2001 engines were built to last more than one weekend.

The lever is **`LifetimeAvg` in the car's `engine.ini`**. Set it near the race duration
you actually run, and remember `LifetimeVar` is a σ, so `Avg` ≈ race length gives roughly
a coin-flip per race with a long tail both ways.

### B.5 Two tokens the §4.2 table omits

- `EngineEmission=(0.0, 0.53, 0.0)` — where flames and smoke are emitted, relative to the
  rear axle. This is the placement of the three emitter banks (`0x7C920C/14/1C`).
- `OnboardStarter=0` — this car does **not** restart when stalled. Directly relevant to the
  ignition byte `0x7C9081`.

### B.6 Not verified

`BrakeWearRate` / `BrakeFailure` driving brake failure by disc thickness is consistent with
the config vocabulary and with brake wear being timescaled alongside the engine
(`findings.jsonl: brake-wear-timescaled-CONFIRMED`), but **what drives the brake wear rate
was never traced**. Treat "torque and temperature" as plausible and unproven.

## References

- Binary: `F1 Challenge Online.exe`, F1 Challenge 99–02, non-ASLR, image base 0x400000.
- Sibling papers: `docs/41-raceability-mechanism.md`, `docs/42-ai-realism-and-difficulty-model.md`, `docs/38-confirmed-offsets.md`.
- Corpus prior-art: `findings.jsonl` (engine_lifetime pokes 2026-07-25, steering/wheel damage 2026-07-29), `name_index.json` entries `WearTickGate` (veh+0x32C8, CONFIRMED) and `EngineLifeRemaining` (veh+0x3D70, FACT).
- Tier legend: FACT (live-poke proven) > CONFIRMED (2 independent sources) > DERIVED (grounded, unproven) > GUESS.

---
*Changelog: 2026-07-31 — initial (static + live-corroborated; one planned player-car poke pending).*
