# Player engine failure — complete map

Session 2026-07-30. Everything here is either static-proven from the decompiled
source or live-proven in-game with the driver reporting what they saw. Tier is
marked per claim. Tool: `patches/mechanical_failures/dll/engine_failure.c`.

---

## 1. The `.PLR` line, and why it does not reach the player

`Mechanical failure rate (0-none, 1-normal, 2-timescaled, 3-season)` parses into
`dword_7CEA5C` (config_schema offset 0x6AC). Companion key `Mechanical Failures`
→ 0x5A4.

That global arms a four-slot countdown array in
`EngineWearFailureSimulation_004ee150`, at `engine[3929..3932]`:

| offset | component | ReasonCode on expiry |
|---|---|---|
| `veh+0x3D70` | engine | 1 |
| `veh+0x3D74` | brakes | 5 |
| `veh+0x3D78` | gearbox | 2 |
| `veh+0x3D7C` | suspension | 4 |

Each is seeded `LifetimeAvg*3 + noise*LifetimeVar*3` from `.hdv [ENGINE]`
(config_schema 0x1330 / 0x1338), divided by `flt_8ACF9C` when rate == 2 in a
race.

> **CORRECTION 2026-07-31 — `flt_8ACF9C` is `fullRaceLaps / actualRaceLaps`, a
> divisor ≥ 1. It is not a lap count.** Measured on two grids:
>
> | race | `0x8ACF9C` | player life `0x7C91DC` | AI slots `veh+0x3D70` |
> |---|---|---|---|
> | 53 laps, 100 % | **1.0** (= 53/53) | 12500.2 | 18170.7 / 34808.7 / 23129.6 |
> | 26 laps, 50 % | **2.03846** (= 53/26 exactly) | 6994.59 | 17043.9 / 9874.21 / 9756.41 |
>
> `TotalRaceLaps` is the *other* global `0x8ACF8C` (CONFIRMED), which read 53
> then 26 in the same commands. `name_index` had `0x8ACF9C` as
> "TotalRaceLaps_2"; it is now **`RaceLengthWearDivisor`** at FACT tier.
>
> **Both models are scaled — the player is NOT exempt.** This also settles the
> open question in root `docs/46`: that paper was directionally right that the
> player is timescaled, and wrong only about what the divisor is.
>
> **Mode 2 is therefore identical to mode 1 at full distance** (53/53 = 1) and
> only differs at reduced distance, where it shortens life in proportion to the
> race so that `life / raceDuration` stays constant. It does **not** make
> failures likely: `LifetimeAvg` 9136 s against a full race of ~4700–5400 s
> leaves seeded life ~2.5× race duration at every distance. No vanilla failure-
> rate setting produces a wear-timer failure inside one race.
>
> *Beware the earlier note in this file's history claiming the divisor was a
> distance fraction and mode 2 a universal no-op — that came from seeing only
> the 100 % case, where the ratio happens to be 1.0.*

> **`noise` IS A STANDARD NORMAL, so `LifetimeVar` is a σ — not a half-range.**
> `RandomNoiseValueGenerator_004b3ce0` popcounts a random 32-bit word
> (`Binomial(32, ½)`, mean 16, sd `sqrt(8)`) and multiplies by
> **`0.35355338 = 1/sqrt(8)`**, normalising to unit sd.
>
> **RUNTIME-PROVEN**, 20 000 direct calls via
> `NativeFunction(ptr(0x4B3CE0), 'double', ['pointer'], 'thiscall')`:
> **mean 0.0106, sd 1.0023**, clean symmetric bell, min −3.851 max +3.862.
> 0.19 % of draws fell outside ±3 σ (normal theory: 0.27 %), so the community
> "max ≈ avg + 3·var" is a good rule of thumb but **not** a clamp; the
> algorithmic bound is near ±5.66 σ. Call the function directly rather than
> hooking it — the return is an x87 `st0` double that Frida's `onLeave` retval
> does not expose on ia32. Note it mutates the PRNG state.
>
> **Player and AI are seeded differently and gated on different modes:**
>
> | | seed | divided in modes |
> |---|---|---|
> | player `0x7C91DC` (`sub_43C250:531`) | `Normal(LifetimeAvg, LifetimeVar)` — **no ×3** | **2 and 3**, race only |
> | AI `veh+0x3D70…` (`0x4EE150:23`) | `Normal(3·LifetimeAvg, 3·LifetimeVar)` | **1 and 2**, race only |
>
> A negative player draw is clamped to `LifetimeAvg`. **In mode 3 the player is
> still timescaled while the AI slots stay `FLT_MAX`** and the AI take the
> scripted `RACEEVENTS` path instead — an asymmetry worth knowing. The AI seed
> is 3× larger, but the AI countdown drains at a flat 1.0/s against the player's
> 0.14–0.66/s, so **the player's effective life is longer, not shorter**. Do not
> compare the two seeds directly.
>
> Same block also does `*(double*)(this + 1736 + 1124*n) *= flt_8ACF9C` for four
> wheels. **CONFIRMED live** — the four fields read `8.60037e-20` at 100 % and
> `1.75315e-19` at 50 %, a ratio of **2.03846**, the divisor to six figures.
> They scale **up** for a shorter race, so the quantity is a per-wheel
> **consumption rate**, not a remaining allowance: **brake wear is timescaled
> alongside engine life.** The exact identity is still unpinned — the base value
> is not the `.hdv` `BrakeWearRate` of `4.797e-011`, so it is a derived internal
> rate. All four wheels read identically, matching the FW23's four identical
> corner sections; a car with asymmetric front/rear rates would be a good
> cross-check and has not been tried. `FuelConsumptionAndWear_004ed960` ticks them and fires on cross-to-negative.
Rate == 0 instead writes `DBL_MAX` and zeroes the brake accumulators; rate == 3
takes a separate path in `sub_424080` that reads the season `RACEEVENTS` list and
matches scripted per-race failures by name — a shipped feature that appears
unused.

**FACT — this whole array is AI-only.** Frida Interceptor on `0x4ED960`, 5 s
sample: 105 calls, 21 distinct `this`, all `0x5EC6E4 + 0x48C0*n` with `n >= 1`.
Zero calls for slot 0. Writing `veh+0x3D70 = 3.0` on the player latched for 12 s
and did nothing. Same shape as the AI-only stuck-gear byte `veh+0x39FA`.

`persistence_map.jsonl` classes all four `SKIPPED_POINTER`, so they were never
swept. Their live values on an AI car decode as floats ≈ 24800 / 32647 / 23765 /
29467 s, which is what identified them in the first place.

---

## 2. The player's own engine failure — `EngineStressSimulation_0043ef20`

`this = 0x7C7428`, the static player physics object (the same `this` as the
player's `SetGear` in `gear_failure.c`).

| address | field | meaning |
|---|---|---|
| `0x7C91DC` | `this+7604` | engine life, double, seconds, ticks down |
| `0x7C91D4` | `this+7596` | stress rate, double, hard cap 0.6 |
| `0x7C920C` | `this+7652` | emitter bank 1 — WHITE SMOKE |
| `0x7C9214` | `this+7660` | emitter bank 2 — BLACK SMOKE |
| `0x7C921C` | `this+7668` | emitter bank 3 — FLAMES |
| `0x7C9081` | `this+7257` | engine RUNNING / ignition flag, byte |
| `0x7C7430` | `this+8` | engine torque, double |

Each bank's `+4` half is the high dword of its double.

**Life decrement:** `life -= ((rpmCurve + oilTempCurve)*0.5 + stress) * dt`. The
two curves are the `.hdv` `LifetimeEngineRPM` (0x1310/0x1318) and
`LifetimeOilTemp` (0x1320/0x1328) tokens; Hex-Rays constant-folds both calls to
`sub_5881F0(2.0)`, which is why the decompile reads `(2.0+2.0)*0.5`.

> **CORRECTION 2026-07-31 — do not read `(2.0+2.0)*0.5` as the runtime rate.**
> The folded constant implies 2.0/s. Measured live it is an order of magnitude
> slower and stress-dependent: **~0.14/s idle at stress 0, 0.28/s under load at
> stress 0.1415, 0.635/s at stress 0.6** (three windows, 0.5 s sampling). The
> stress term feeds back roughly 1:1 — a stressed engine dies faster — so a
> failure genuinely accelerates once it starts. The evaluated values of the two
> curves are still unmeasured; the attempt was made but the game was paused
> (SessionTime frozen). `docs/46`'s "≈2–2.5/s" comes from the same folded
> constant and is wrong for the same reason.

**On `life <= 0`:** writes ReasonCode 1 to `vehicle+16488` (`veh+0x4068`), rolls
one emitter bank, and re-arms life for a staged death.

> **CORRECTION 2026-07-31 — the earlier claim that it "calls `sub_41AC50`
> itself, so the race-engineer line needs no cooldown unblocking" is
> DISPROVEN.** A Frida `Interceptor` held on `0x41AC50` across an entire live
> failure — write, expiry, ReasonCode 0→1, re-arm, 12 s of stress and smoke —
> logged **0 calls**. The driver independently confirmed no radio line. Static
> corroborates the negative twice over: `sub_41AC50:15` guards with
> `(!dword_7C54E8 || dword_7C54E8 != a1)`, which excludes the player car even
> if it were called. **A player engine-failure radio line needs a DLL emitter**,
> same as PWS_RETIRE02.

**Below zero the failure keeps developing:**

- `life < -10.1` — halves banks 1 and 2, clears bank 3 (partial recovery)
- `life < -15.4` — stress ramps `+0.002*dt` up to the 0.6 cap

**Live-proven progression (2026-07-30):**

| write | result |
|---|---|
| `life = 1.5` | ReasonCode 0→1, life re-armed 168.06, stress 0→0.111, bank1 −0.3248. Driver: *"engine is blowing white smoke, still running normally"* |
| `life = -16.0` | stress ramped 0.111 → 0.148 → 0.427 → **0.600 cap** unaided; torque fell to 0.9 |
| second `life = 1.5` | rolled bank1 to **+0.719**, zeroed stress and life |

Fresh-session life draws observed: 10198, 11652, 6911 — the random
`LifetimeAvg + noise*LifetimeVar`, as the code predicts.

---

## 3. Power and smoke are separate channels

This is the single most useful thing in this document, and it explains why stage
one reads as "smoke but the car is fine".

**Power — stress cuts torque.** End of the torque update in `Gearbox_00443b70`:

```c
if (this+7596 > 0.0) {
    v33 = 11.5 / this[7388] * dbl_8AFEA0 * this[7260];
    v36 = interpolated sample of noise table flt_8AAE74 at v33;
    this[8] = (1.0 - v36*stress*v36) * this[8];      // this+8 = engine torque
}
this[8] = sub_447B10(this) * this[8];                // running/fuel gate
```

`v36` is a noise sample **phased by engine speed**, so the loss is a stuttering,
rev-dependent misfire — not a flat power cut.

**Quantified 2026-07-31.** `flt_8AAE74` was dumped live: a **256-entry table of
uniform random floats in [0, 1]** (min 0.0004, max 0.9987, mean 0.5027,
mean-of-squares 0.3446). `dbl_8AFEA0` is **SessionTime**. So the index is

```
idx = 11.5 * (rpm / maxRPM) * SessionTime        (wraps, u8 -> 256 entries)
```

which sweeps at **11.5·(rpm/maxRPM) entries per second** — the flutter frequency
scales with engine speed, ~11.5 Hz near the limiter, and the table wraps every
~22 s at full revs. Because `v36` is uniform and then **squared**, the multiplier
distribution is skewed toward small cuts with occasional deep ones:

| | mean loss | worst-case loss |
|---|---|---|
| formula | `0.345 × stress` | `0.997 × stress` |
| stage one, stress 0.1415 | 4.9 % | 14 % |
| stress cap 0.6 | 21 % | 60 % |

The old "at stage one the cut is a few percent, which is why the driver felt
nothing" holds on average, but understates the peaks. **2026-07-31 live: the
driver DID feel stage one** — reported "car is slower on straights" at stress
0.1415, then "going faster now" the moment stress alone was zeroed with the
banks left dirty. That A/B is the direct proof that stress is the power channel
and the banks are cosmetic.

**Smoke — banks drive the emitter.** `TireSlipForceCalculation_004472b0`
(misnamed; it is the vehicle effects pass) skips the emitter **only** when all
three banks are exactly 0.0. Otherwise:

```c
v19 = clamp(-1.0/this[7388] * rpm * torque, -1.0, 0.0);
per bank:  (bank <= 0) ? v19 * bank : bank      ->  a2[38] / a2[43] / a2[48]
ParticleCollisionEffect(...)
```

A **positive** bank is used directly as the emission rate, so it renders even at a
standstill. A **negative** bank is scaled by engine load, so it only shows under
power — which is why the first white-smoke run needed driving.

> **Trap for any repair routine: healthy is 0.0, not 1.0.** Writing 1.0 to
> "restore" a bank leaves the car smoking permanently. This bug was written and
> caught in this session.

---

## 4. Bank → emitter mapping (isolation-proven)

Emitter ids come from `MaterialShaderReactionEffectParser_004ca480`:

```
0 DUST   1 WHITESMOKE   2 BLUESMOKE   3 BLACKSMOKE   4 FLAMES
5 SPRAY  6 SKID   7 SOFTSKID   8 WETSKID   9 GROOVE   10 WETGROOVE
```

| bank | address | emitter |
|---|---|---|
| 1 | `0x7C920C` | WHITE SMOKE |
| 2 | `0x7C9214` | BLACK SMOKE |
| 3 | `0x7C921C` | FLAMES |

**FACT.** Each armed alone at `+0.85` with the other two at exactly 0.0, on a
healthy car, driver reporting what rendered: bank3 → *"I can see only fire now"*;
bank2 → *"Black smoke only"*; bank1 → white smoke.

So the three "damage banks" are **not severity stages** — they are three
independent visual channels, and the failure roll picks which one to light.

### Blue smoke is cut content

`SeasonData/Circuits/terrain.ini` defines the shipped emitters:
`whitesmoke`, `blacksmoke`, `flames`, `dust`, `skid`, `softskid`, `wetskid`,
`spray`, `groove`, `wetgroove`, `rain`, `spark`.

There is **no `Reaction=bluesmoke` line anywhere in the install** — every
`.ini`/`.gen`/`.veh`/`.hdv`/`.tbc` was searched. The exe parses the token and
assigns it id 2, but nothing defines the particle and no bank maps to it.

To restore it you would need both halves: a `Reaction=bluesmoke Tex=... Max=...`
line in `terrain.ini`, **and** something to drive it, since no engine bank
targets id 2. In real F1 blue smoke means burning oil, and the engine model does
carry an oil-temperature curve (`LifetimeOilTemp`) — so an oil-related trigger is
the obvious intent, but that is a GUESS, not evidence.

---

## 5. Binary seizure, and the ignition flag

`0x7C9081` (`this+7257`) gates torque through the game's own multiplier:

```c
sub_447B10:  if (!this[7257]) return 0.0;   // then a fuel-starvation ramp
Gearbox_00443b70 ends with:  this[8] = sub_447B10(this) * this[8];
```

Clearing it is an instant binary stop — no wear, no smoke, no staging. This is
the equivalent of the `damage.ini` collision outcome, on demand.

Related helpers: `sub_4472A8` is the game's one-line kill; `sub_4471F0` is the
starter (sets the byte **and** lifts `this+7260` to `this+7300` if below);
`sub_447160` is the "engine running" predicate (`life>0 && fuel>0 && this[7257]`).

**FACT — this is the ignition flag.** Frida polled the byte at 10 Hz for 8 s
while the driver pressed the bound ignition key: transitions 1→0 @0.2 s,
0→1 @2.2 s, 1→0 @3.7 s, 0→1 @7.1 s. Four presses, four flips, no drift. The
driver independently reported the DLL's seizure key *"does EXACTLY the same as
the ignition key"*.

This **supersedes** `docs/IGNITION_INVESTIGATION.md`, which tested `veh+0x39A0`
(recalculated), `veh+0x3984` (fuel, not a gate) and the key-queue / DirectInput
buffers (overwritten before read). Right concept, wrong offsets.

**Still open:** the keybind does **not** reach the byte via `sub_4472A8` or
`sub_4471F0` — both were hooked during the same window and neither was called.
The real writer is inline elsewhere and is unlocated. Use an x32dbg `bphws` on
`0x7C9081`; a Frida hardware watchpoint crashes this process.

---

## 6. How this differs from `damage.ini`

F1C has **no `EngineSeizure` token** — that is the rFactor/GTR name and it
appears nowhere in `strings_index.json`. The F1C equivalent is
`SeasonData/damage.ini [PHYSICAL] Engine=10000.0` *"Impulse to seize engine"*,
parsed by `VehicleDamageModel_004f24f0` via `sub_565870` into offset `0xC`
(`0x0` RadiusAdd, `0x4` RadiusMult, `0x8` RadiusMax, `0xC` Engine, `0x10`
AeroDiv, `0x14` AeroMin, `0x7C` WheelBend, `0x80` WheelDetach).

| | `damage.ini Engine=` | life/stress model |
|---|---|---|
| trigger | collision impulse | wear countdown |
| source | one crash | `.hdv LifetimeAvg`/`LifetimeVar` |
| outcome | instant binary seizure | staged: smoke → misfire → stop |
| progression | none | bank rolls, stress ramp, recovery at −10.1 |

Editing `damage.ini` does not affect the wear path or any offset in the
`0x7C7428` object.

---

## 7. Brakes — no mod lever needed

`PhysicsCollisionDamageDNFDetection_00438690` decrements `this+1068` (brake disc
thickness) by `this[1028]^3 * this[1084] * load * dt`, and on crossing zero walks
the active-car list and sets ReasonCode 5 on the first car whose `+13000` is
clear. `this+1092` is the floor used for the ratio once thickness is at or below
it.

This path reaches the player, which matches the observation that brakes fail
naturally from setups with too-low brake duct values. No mod is required.
*(Static only — not separately poke-tested.)*

---

## 8. Corrections this session produced

| symbol | current name | what it actually is |
|---|---|---|
| `0x7C91DC` | `DoubleCarOrientationThreshold` | player engine LIFE |
| `sub_447B10` | `RandomPhysicsParameterGenerator` | engine output multiplier |
| `sub_4472A8` | `SetByteOffset7257` | engine kill |
| `sub_4471F0` | `SuspensionVibration` | engine starter |
| `sub_004472b0` | `TireSlipForceCalculation` | vehicle effects pass (damage emitters) |

**Unresolved:** `0x7C90C4` (`this+7260`) was labelled engine RPM while building
the DLL. It read exactly `436.2834437795265` in every sample across a session
that included real driving — constant to 16 digits, so the label is wrong or the
read is of a stale copy. Nothing in the failure work depends on it. Re-derive
live before using it, and do not propagate the RPM label.

---

## 9. What is still unknown

1. **The ignition keybind's actual writer** (§5). Needs an x32dbg `bphws`.
2. **`0x7C90C4` identity** (§8).
3. **Blue smoke's intended trigger** (§4) — no bank targets id 2 and no particle
   is defined. Oil-related is a guess.
4. **ReasonCodes 3/7 Clutch, 6 Accident, 8 Electronics** — the category table
   `FailureCategoryNameLookup_004bb730` lists nine, but the wear function only
   produces 1, 2, 4, 5, 9. The rest are emitted somewhere unmapped.
5. **Season scripted failures** (§1) — rate 3 reads a `RACEEVENTS` list and
   matches per-race failures by name (`STALL` seen in the strcmp chain). Never
   exercised.
6. **Whether stage-one is escapable.** The game halves banks below −10.1, so a
   partial recovery path exists, but nobody has driven one out.
