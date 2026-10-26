# Engine lifetime / wear model — and the fields that drive it

Applied 2002 Custom V1 +25% lifetime change and correction to the original-baseline interpretation: [operations record (2026-09-16)](../../../operations/ENGINE_LIFETIME_2002_25PCT_APPLY_2026-09-16.md). The active game tree was not changed.

Current 2002 reliability target and proposed trial: [historical results, four local captures, and exact candidate values (2026-09-16)](2002_RELIABILITY_TARGET_2026-09-16.md). No game files were changed by that investigation.

Current 2002 season file audit: [2002 V1 engine-lifetime audit (2026-09-16)](2002_V1_AUDIT_2026-09-16.md). This older 2026-07-25 investigation is preserved as historical evidence; the newer report points to the corrected player/AI wear interpretation.

**Date:** 2026-07-25
**Trust:** offsets DERIVED (time-series drift, strong discriminator, not poke-proven).
The *model* itself is FACT — it is declared in the game's own data files.

## Correction to prior corpus belief

An earlier conclusion in this session — that F1 Challenge does **not** simulate engine
wear, and that AI engine failures are purely a random draw — was **wrong**. It was
reached from the memory-field lists alone (`name_index.json` had only `EngineRPM` /
`EngineMaxRPM` / `EngineAngVel`, F1CUI the same) without consulting the game's data
files. The owner pointed to `engine.ini`, which settles it.

## The model (from `SeasonData/Vehicles/<team>/<year>_<car>/engine.ini`)

```ini
LifetimeEngineRPM=(15742.0,355.3)   // base engine speed for lifetime, range where lifetime is HALVED
LifetimeOilTemp=(123.43,2.90)       // base oil temp for lifetime, range where lifetime is HALVED
LifetimeAvg=8924                    // average lifetime in SECONDS
LifetimeVar=2947.89                 // lifetime random variance
EngineEmission=(0.0, 0.53, 0.0)     // where flames and smoke are emitted (rel. to rear axle)
OptimumOilTemp=105.6                // degrees C at which the engine operates optimally
CombustionHeat=32.0                 // degrees C added per litre of fuel burned
EngineSpeedHeat=7.732e-004          // heat added linearly with engine speed
OilMinimumCooling=9.017e-004
OilWaterHeatTransfer=(5.263e-003,8.896e-005)
WaterMinimumCooling=3.653e-003
RadiatorCooling=(2.849e-004, 1.003e-004)   // cooling with velocity (base, per setting)
```

Read plainly: every engine gets a **lifetime in seconds**, drawn around
`LifetimeAvg` ± `LifetimeVar` (~8924 ± 2948 s). That budget is consumed **faster** the
further engine speed sits above `LifetimeEngineRPM` base and the further oil temp sits
above `LifetimeOilTemp` base — each halving remaining life per unit of "range". Heat
itself is simulated from fuel burned and engine speed, offset by radiator cooling that
scales with velocity and radiator setting.

`EngineEmission` is the smoke/flame origin — i.e. **the visible smoke from a failing
engine is this model's output**, which is how this line of investigation started.

Note: these values are per-manufacturer. Each team's `engine.ini` differs, so there is
no single "normal" oil temp across the grid (see below).

## Relationship to the AI failure roll

This does **not** contradict `EngineFailureCheck_004bec55` (documented separately).
That function runs at lap completion and, on a non-collision failure, rolls a random
float to choose *which component* is blamed:

| Roll | Reason | Chance |
|---|---|---|
| < 0.30 | Engine | 30% |
| 0.30–0.45 | Gearbox | 15% |
| 0.45–0.60 | Clutch | 15% |
| 0.60–0.75 | Suspension | 15% |
| 0.75–0.89 | Brakes | 14% |
| 0.89–0.91 | Clutch (7) | 2% |
| 0.91–0.93 | Electronics | 2% |
| 0.93–0.96 | Fuel | 3% |
| >= 0.96 | none | 4% |

The two coexist: the lifetime model is a physics-driven wear budget; the roll is a
classification/selection step. The earlier error was treating the roll as the *whole*
story.

## Fields located in memory

Method: sample all cars every 2 s for ~28 s of green-flag running via a **persistent
external poller** (`tools/engine_temp_poll.js` + `tools/mod_pipeline/watch_run.py`),
then compare drift.

**The discriminator that made this work:** cars whose engines were dead (Schumacher and
Hakkinen, both `rpm 0` and `EngineAngVel 0`) held these values at delta **exactly
0.000**, while every running car drifted. Static-vs-drifting cleanly separates live
physics state from setup constants.

| Offset | Name | Behaviour over 28 s | Reading |
|---|---|---|---|
| `+0x3D10` | `OilTemp` | +0.03 … +0.08 running, **0.000** dead | oil temperature |
| `+0x3D14` | `WaterTemp` | +0.008 … +0.025 running, **0.000** dead | water temperature |
| `+0x3984` | `FuelLevel` | **−0.56 … −0.96** running, 0.000 dead | litres remaining |
| `+0x424C` | — | ±50 randomly | **rejected**, noise |
| `+0x3D0C` | — | constant 255.5 on all 22 cars | likely a limit/config constant |

### The ~84 vs ~100 oil-temp anomaly — resolved

Oil temp clustered into two groups, and at first this looked like it might be damage:
TesterA and Raikkonen read ~84 while doing 268 and 288 km/h, the same value as the two
dead engines.

It is **not** health. It is **different engine manufacturers** — each team's
`engine.ini` has its own `OptimumOilTemp` (Arrows 2001 = 105.6). Montoya, Raikkonen and
TesterA simply run cooler engines. The dead cars are distinguished not by *value* but by
being **frozen**, which is exactly why the drift test was necessary and a single
snapshot would have been misleading.

Consequence for any UI: there is **no fixed "normal" oil-temp band** across the grid.
Colour-code against the `LifetimeOilTemp` threshold (123.43) rather than a fixed
midpoint.

## Method note — a trap worth recording

Sampling with a **busy-wait inside a Frida script** (`while (Date.now()-t0 < 4000) {}`)
returned **byte-identical** readings for every field including RPM, which made a running
game look paused. A separate positional check proved the game was running normally
(a car moved 139.8 units in 1.5 s).

**Do not time-sample from inside a single injected script.** Use a persistent external
poller. This is the same class of mistake as post-hoc polling of transient flags: the
measurement apparatus, not the game, produced the null result.

## Open

- **The lifetime counter itself is NOT located.** `LifetimeAvg` is in seconds (~8924),
  so a per-car float should exist counting engine-seconds up or remaining-life down.
  Nothing in the scanned ranges fitted that magnitude and trend. Finding it would let a
  pit wall display *actual engine life remaining* — the real prize.
  Suggested approach: full-struct dumps several minutes apart under sustained green-flag
  running (no crashes, no pit stops), looking for a monotonic float in the 10^3–10^4
  range, cross-checked against a car at higher sustained RPM consuming it faster.
- None of `0x3D10` / `0x3D14` / `0x3984` are poke-proven. Writing a high oil temp and
  observing an engine failure would raise these to FACT and simultaneously confirm the
  lifetime mechanism end to end.
- Whether the visible smoke is triggered off a lifetime threshold or off oil temp
  directly is untested.
