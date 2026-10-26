# Engine destroyed by doughnuts — observed 2026-07-25, with a replication recipe

A live, deliberately-caused engine failure: sustained doughnuts (full throttle,
high revs, near-zero road speed), engine died. This is the cleanest engine-wear
sample the corpus has, because the cause was known in advance.

## What the car read at the moment of death

Player, slot 0, **2001 Arrows A22**:

| Field | Offset | Value | Reading |
|---|---|---|---|
| EngineRPM | `+0x39A0` | **0.0** | not turning |
| EngineAngVel | `+0x00A0` | **0.0** | crank stopped — engine genuinely dead, not just stalled-looking |
| **OilTemp** | `+0x3D10` | **130.73 °C** | **7.30 °C above this engine's lifetime base** |
| WaterTemp | `+0x3D14` | 65.41 °C | still low — see "why oil, not water" |
| ReasonCode | `+0x4068` | **1 = Engine** | the game's own verdict |
| carConditions | `+0x3D6C` | 0 | no bodywork damage |
| hitsInRace | `+0x12B4` | 0 | **no contact at all** — purely mechanical |
| FinishState / Retired | `+0x4064` / `+0x4410` | 0 / 0 | not yet classified out |
| Fuel | `+0x3984` | 61.46 L | not fuel starvation |
| Throttle | `+0x357C` | 1.00 | still flooring it, nothing happening |

Note `FinishState 0`, `Retired 0` with `ReasonCode 1`: the game sets the failure
reason **while the car is still nominally running**, which corroborates the
earlier Hakkinen observation. Zero hits and zero damage rule out every collision
path — this was the wear model, nothing else.

## The mechanism, from the car's own engine.ini

`SeasonData/Vehicles/Arrows/2001_A22/engine.ini`:

```ini
OptimumOilTemp=105.6                // degrees C at which engine operates optimally
OilMinimumCooling=9.017e-004        // heat dissipated WITHOUT radiator
RadiatorCooling=(2.849e-004, 1.003e-004)   // cooling rate WITH VELOCITY (base, per setting)
LifetimeEngineRPM=(15742.0,355.3)   // base engine speed, range where lifetime is HALVED
LifetimeOilTemp=(123.43,2.90)       // base oil temp,  range where lifetime is HALVED
LifetimeAvg=7919                    // average lifetime, seconds
LifetimeVar=2618.55                 // random variance
```

The decisive line is `RadiatorCooling`, which **scales with velocity**. A
doughnut is the worst case the model allows: maximum heat input at maximum revs
with essentially zero airflow, so only `OilMinimumCooling` (9.017e-4) is
removing heat. Oil temperature climbs unopposed.

Lifetime then burns super-linearly. At the observed 130.73 °C:

```
(130.73 - 123.43) / 2.90 = 2.52 halvings
2^2.52 = 5.7x
```

**INFERENCE, NOT MEASUREMENT — see the correction below.** Taken at face value
the formula says the engine was consuming its life ~5.7 times faster than
normal, on top of whatever the RPM term was adding. With `LifetimeAvg=7919 s`, an effective
~1390 s of engine life is enough to destroy it — and revs contribute their own
halvings via `LifetimeEngineRPM` (every 355.3 rpm over 15742).

For reference, healthy AI cars in the same session read oil ~84 or ~100 °C
depending on manufacturer. 130.7 is far outside normal running.

### Why oil spiked but water did not

`OilWaterHeatTransfer=(5.263e-003, 8.896e-005)` moves heat from oil to water
slowly, and `WaterMinimumCooling` is likewise small without airflow. Oil is
where the heat appears first, so **oil temp is the early-warning channel** —
water temp lags and would not have warned you in time.

## The thresholds are per-manufacturer — do not hardcode one number

Read straight from the 2001 engine files:

| Team | OptimumOilTemp | LifetimeOilTemp (base, halving) | LifetimeEngineRPM | LifetimeAvg |
|---|---|---|---|---|
| Ferrari | 106.9 | 124.79, 2.93 | 16853.2, 380.4 | 10365 |
| McLaren | 105.5 | 123.35, 2.90 | 16575.4, 374.1 | 9239 |
| Williams | 107.2 | 125.11, 2.93 | 17038.4, 384.6 | 9136 |
| Jordan | 103.5 | 121.26, 2.85 | 16668.0, 376.2 | 8151 |
| **Arrows** | **105.6** | **123.43, 2.90** | **15742.0, 355.3** | **7919** |

The Arrows has the **shortest** average lifetime and the **lowest** RPM base of
the five, i.e. the player was driving the most fragile engine on the grid. A
Ferrari would have tolerated the same abuse noticeably longer (base 124.79 and
LifetimeAvg 10365).

## Replication recipe

1. Start a race in a car whose `LifetimeAvg` is low — Arrows 2001 is the
   worst-case of those sampled.
2. Hold the car near-stationary at full throttle: doughnuts, burnouts, or simply
   revving against the brakes. Airflow is what matters, so **zero road speed is
   the point**, not wheelspin.
3. Watch `OilTemp` (`veh+0x3D10`). It is on the dashboard as `ot`. Above the
   engine's `LifetimeOilTemp` base, every `2.90 °C` doubles the burn rate.
4. Sustain. Death signature is unambiguous and appears in one tick:
   `EngineRPM == 0` **and** `EngineAngVel == 0`, with `ReasonCode` flipping to
   `1` while `FinishState` is still `0`.

Expect it to be faster at higher revs: the RPM term halves lifetime every
355.3 rpm above 15742, and it multiplies with the temperature term.

To make it happen *sooner* for testing, raise the ambient temperature — the
physics reads air temp from `0x8AFEF0` (see `name_index.json`), which feeds the
radiator model.

## What the dashboard already shows

`mods/f1cui_web_remote` publishes `ot` (oil), `wt` (water), `eav`
(EngineAngVel), `ed` (engine-dead) and `rc` (reason code), and colours oil temp
against the 123 °C threshold. It reported this failure correctly: `dt:5`
(ENGINE STOP), `ed:1`, `rc:1`, `ot:130.7`, `eav:0.0`.

**Known imprecision:** that 123 °C colour threshold is hardcoded, but the table
above shows the real base ranges from 121.26 (Jordan) to 125.11 (Williams). It
is close enough to be useful and wrong enough to mislead by a couple of degrees
on some cars. Fixing it properly means keying the threshold off the car/team
name (`cn`), which the DLL already has.

## CORRECTION 2026-07-25 (later the same session)

The 5.7x figure above is arithmetic from `engine.ini`, **not** an observed burn
rate, and the component life counters have since been located
(`LIFETIME_COUNTER_FOUND_2026-07-25.md`). Measured against those counters:

- healthy cars drain at a flat ~1.0/s regardless of rpm, and
- pinning a car's `OilTemp` field at 200 C changed its drain **not at all**.

So the causal chain "high oil temp -> faster life burn -> engine dies" is still
UNPROVEN as stated. What remains solid is the observation itself: sustained
doughnuts, oil at 130.7 C, engine dead with `ReasonCode 1`, no contact and no
damage. The mechanism connecting the two has not been demonstrated.

## Still open

- **SOLVED, see `LIFETIME_COUNTER_FOUND_2026-07-25.md`** — the four component
  life counters are at `veh+0x3D70` (engine), `+0x3D74` (brakes), `+0x3D78`
  (gearbox), `+0x3D7C` (suspension). This section previously said they were not
  located; that is out of date.
- What remains genuinely open is whether the `engine.ini` temperature and RPM
  terms modulate the drain of those counters. Measured so far: a flat ~1.0/s for
  every car, unaffected by pinning `OilTemp` at 200 C. Needs a car with a
  genuinely elevated internal oil temp to settle.
