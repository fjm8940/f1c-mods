# Component lifetime counters — FOUND and poke-proven, 2026-07-25

The corpus listed "the lifetime counter itself is still not located" as the last
open item of the engine-wear model. It is located, and it is four counters, not
one. Arming the engine one on a healthy AI produces a real, game-driven
retirement.

## The counters

| Offset | Component | Fires ReasonCode | Healthy sample (seconds) |
|---|---|---|---|
| `veh+0x3D70` | **Engine** | 1 (Engine) | 20063 / 28294 / 29899 |
| `veh+0x3D74` | Brakes | 5 (Brakes) | 30880 |
| `veh+0x3D78` | Gearbox | 2 (Gearbox) | 20605 |
| `veh+0x3D7C` | Suspension | 4 (Suspension) | 40928 |

Units are seconds remaining. A car whose engine had just blown read **−1.0** in
the engine slot while three healthy cars read 20 000–30 000.

## Where they come from

`FuelConsumptionAndWear_004ed960`:

```c
if ( sub_4D21D0() )
{
  for ( i = 0; i < 4; ++i )
  {
    if ( this[i + 3929] >= 0.0 )
    {
      v23 = this[i + 3929] - a2;      // a2 = delta time
      this[i + 3929] = v23;
      if ( v23 < 0.0 )
      {
        switch ( i )
        {
          case 0: ... *(_DWORD *)(*(_DWORD *)this + 16488) = 1;   // Engine
          case 1: ... = 5;   // Brakes
          case 2: ... = 2;   // Gearbox
          case 3: ... = 4;   // Suspension
```

**The offset trap:** `this` is **`veh+0xC`**, not the vehicle base. The giveaway
is `*(_DWORD *)this + 16488` resolving to `veh+0x4068` (ReasonCode), which only
works if `*(veh+0xC) == veh` — and it does; the vehicle has several self-pointers
(`+0x0`, `+0xC`, `+0x14`, `+0x6F0`, `+0x32CC`, `+0x4034`). So
`this[3929]` = `veh + 0xC + 3929*4` = **`veh+0x3D70`**, not `veh+0x3D64`.
Reading the naive offset gives 0.28/1.30/0.00 for every car — plausible-looking
garbage that would have been easy to publish.

ReasonCode values match `FailureCategoryNameLookup_004bb730` (1 Engine,
2 Gearbox, 4 Suspension, 5 Brakes).

## Poke proof — arming the engine on a healthy AI

Target: Stephane Sarrazin, slot 18, P13, running normally at 79.5 units/s,
`ReasonCode 0`, engine life 20062.98 s.

```
write veh+0x3D70 = 0.5
```

| t | observed |
|---|---|
| +0.2 s | `ReasonCode 1`, life −0.50, rpm falling 1577 → 1064, angvel tracking rpm down |
| +1 s | rpm 0, angvel 0, speed 0 — car stopped on track |
| +5 s | **the game itself** set `FinishState 2` and `RetiredFlag 1` |

Nothing was forged: one countdown was shortened and every consequence —
failure code, engine wind-down, classification as a retirement — was produced by
the game's own code. This is the "arm the trigger, never forge the end state"
pattern the corpus prescribes, and it is the correct replacement for
`mods/engine_blowup`, which writes `FinishState=2` + `ReasonCode=1` directly and
has never been proven in a race.

**User-observed on screen:** the failing car emitted **white smoke**. Reported
separately: a player engine damaged by sustained over-revving emitted **black
smoke** beforehand. So the two are visually distinct — black smoke while the
engine is being abused and losing life, white smoke at the moment of failure.
(Visual correlation from direct observation, not traced to code.)

## What does NOT work

**Poking OilTemp (`veh+0x3D10`) is useless as a kill switch.** It was held at
200 °C on a healthy AI for 24 seconds: the write persisted (it is an
accumulator, not recomputed per frame), but `ReasonCode` stayed 0 and the engine
kept running at 1572 rpm. Oil temp is an *input to the wear rate* and a readout;
it does not itself end the engine. The countdown above is what ends it.

This also means the `engine.ini` model (`LifetimeOilTemp`, `LifetimeEngineRPM`)
must feed the *rate* at which these counters are decremented — consistent with
`a2` being a scaled delta rather than raw wall time, since healthy cars show
20 000–40 000 s remaining against a `LifetimeAvg` of 7919–10365 s.

## How to use it

- **Blow a specific driver's engine:** write a small positive float (0.5) to
  `veh+0x3D70`. Do not write 0 or a negative value — the loop only acts on the
  transition `>= 0` → `< 0`, so a value already negative is skipped entirely.
- **Other failures:** same trick on `+0x3D74` (brakes), `+0x3D78` (gearbox),
  `+0x3D7C` (suspension).
- **Make a car bulletproof:** write a huge value. Untested.
- **Read remaining life for a wear gauge:** these are exactly the numbers a
  "engine health" display needs, and `mods/f1cui_web_remote` can now show real
  remaining life instead of inferring from oil temp.

## Drain rate — MEASURED

Sampled 12 running cars against the session clock over 10 s: **every counter, on
every car, drained at exactly 1.000/s**, across rpm 1204-1841 and oil 103-105 C.
So the units ARE wall-clock seconds and a "remaining laps" estimate
(life / lap time) is valid.

A second sample measured 1.108/s — again *identical across cars*, so the
variation is global (the decrement follows a physics/frame time base that does
not track the session clock exactly), not per-car wear.

**No evidence yet that the engine.ini wear model accelerates this counter.** A
car whose oil temp was pinned at 200 C for 8 s drained at 1.108/s — precisely the
same as an untouched control car at 104.5 C:

```
TARGET  oil held at 200.0 -> ENG drain 1.108/s
CONTROL oil 104.5         -> ENG drain 1.108/s
```

That is consistent with two different explanations and the evidence does not yet
separate them:

1. `0x3D10` is a display copy, and the wear model reads its own internal oil
   temperature — so poking it can never accelerate anything. (Supported by the
   24 s / 200 C test that failed to kill the engine at all.)
2. The multiplier only applies above `LifetimeOilTemp` base (123.43 for Arrows),
   and every car sampled sat at ~104 C, i.e. multiplier = 1 either way.

**To settle it:** measure the drain on a car whose *genuine* oil temp is above
its base — e.g. during sustained doughnuts, which is exactly how the player's own
engine reached 130.7 C. If the drain jumps above the global baseline, the model
scales this counter and (1) is wrong; if it stays flat, the counter is a plain
clock and the engine.ini terms act somewhere else entirely.


## MAJOR CORRECTION — the countdown is AI-ONLY (same session, later)

`MainGameTickUpdate_0042c690` runs the wear pass only for cars that pass a gate:

```c
if ( !(dword_7C5698 % 400) )
  for ( k = 0; k < VehicleCount; ++k )
    if ( *(_DWORD *)(dword_7C54F0[k] + 13000) == 1 )   // veh+0x32C8
      FuelConsumptionAndWear(1.0);                      // literal 1.0 decrement
```

**Live: every AI car reads `veh+0x32C8 == 1`; the HUMAN reads 0.** Consequences,
all observed directly:

- The player's four countdowns never tick. Watched across a full doughnut
  session: engine life sat at **23245.8 unchanged**, drain 0.000/s, while AI cars
  around him drained at ~1.0/s.
- The player's `OilTemp` (0x3D10) is likewise frozen — it held 130.73 (a value
  left over from the *previous* race) for minutes while the engine was being
  revved to the limiter and the game displayed a high-temperature warning on the
  HUD. So for a human car that field is stale garbage, not telemetry.
- At the moment of failure the counter jumped **straight from 23245.8 to exactly
  -1.000**, with `ReasonCode 1`. It was written by the failure, not drained into
  it. That also explains the earlier puzzle of counters landing on exactly 0.0 or
  -1.0 rather than a fraction.

**So the player's engine failed by an OVERHEAT path that is not this model.** The
in-game high-temp warning appeared first, then the engine died. Where that path
reads its temperature from is unknown — it is NOT `0x3D10`, which was frozen
throughout.

### What this means for earlier claims in this file

- "`LifetimeAvg`/`LifetimeVar` seed the four counters as `Avg*3 + noise*(Var*3)`"
  — **still true**, and re-confirmed on a fresh restart (player reseeded to
  23245.8 against Arrows `Avg*3` = 23757, noise -0.07).
- "Arming `veh+0x3D70` produces a real retirement" — **still true**, proven on an
  AI car (Sarrazin), which is exactly the case where the gate is 1.
- The wear model as a whole describes **AI reliability only**. Nothing here
  governs when a human's engine blows.

### Still open

- What the overheat path reads and writes. A struct-wide float diff over 6 s of
  revving found only per-car time accumulators (`+0x78C`, `+0x1024`, `+0x34B8`,
  `+0x413C`, `+0x4480`, all climbing ~1.0/s and tracking the session clock), no
  rising temperature. The real temperature is likely in the physics object rather
  than the vehicle struct.
- What writes `-1.0` into the counter on failure. A 5 ms poller, or a hardware
  write breakpoint on `0x5EC6D8+0x3D70` (slot 0 is static memory), would catch it.

## Still open

- Whether `LifetimeOilTemp` / `LifetimeEngineRPM` modulate this counter at all —
  see the measurement above. Until that is answered, do not claim a burn-rate
  multiplier as fact.
- `sub_4D21D0()` gates the whole loop — presumably "wear enabled / session is
  live". Not decoded.
