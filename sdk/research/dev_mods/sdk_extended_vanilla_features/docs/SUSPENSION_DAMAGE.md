# Suspension / wheel-bend damage — located, decoded, live-proven

> **RESOLVED 2026-08-08 — the control input is found and live-proven.**
>
> The WARNING below stands for **direct field writes** (DISPROVEN 2026-07-30:
> did nothing, then crashed). But the *input* the game writes is now known,
> and driving it works end to end:
>
> **`SteeringGeometryUpdate_00442060(0x7C7428, wheel, severity)`** — the exact
> call the game makes at `CollisionSparkParticleEffects_004ec1a0:515` on a
> wheel impact, gated there to the player (role `this[3247] == 0`). It sets
> the bent flag (+1105), both bend angles (+1116/+1124, clamped ±10°), the
> accumulator (+1132) and ReasonCode=4, all with the game's own arithmetic.
>
> Built as `patches/mechanical_failures/dll/suspension_failure.dll`: a detour
> on `VehicleMainPhysicsUpdate_004441f0` (per-frame, game thread) calls that
> input ~20 Hz while armed. Live 2026-08-08: stage 1 produced RL −0.0377 rad
> (−2.16°), RR +0.0496 rad (+2.84°), both rear bent flags 1, ReasonCode 0→4,
> radio eid 75 "Damaged suspension - avoid kerbs" PLAYED, repair restores all
> fields. Full details in the mechanical_failures README.
>
> The "does not reconcile" point below was arithmetic confusion: `v4+480` in
> Hex-Rays' `int*` rendering is the accumulator at record+1132 (the live
> measurement was right); record +{1105, 1116, 1124, 1132} all reconcile.

> **WARNING added 2026-07-30 — these offsets are READ-proven, not WRITE-proven.**
>
> Everything below was validated by *reading* these fields during real crash
> damage. That evidence stands, and the fields are good telemetry.
>
> **They are not control inputs.** Writing them to arm damage on demand was
> tried on 2026-07-30: setting the accumulator past the 0.015 trip did nothing
> (ReasonCode never became 4, even with ReasonCode cleared to 0 first), and a
> follow-up that also wrote the bend angles and the bent flag **crashed the
> game**.
>
> Two reasons, both checkable in the source:
> 1. The 0.015 trip only raises ReasonCode, and is guarded by `if (!ReasonCode)`
>    — dead whenever another failure already set a code.
> 2. `SteeringGeometryUpdate_00442060` indexes `this[281*a2 + N]` with N up to
>    282. 281 dwords = 1124 bytes = the stride assumed here, so `+279..+282`
>    dwords run *past* the end of the record. The stride/offset model below
>    does not reconcile with the function's own arithmetic. The accumulator it
>    actually tests is `*(double*)(v4 + 480)` on a sub-object.
>
> To arm suspension damage, find what `v4` is first (x32dbg `bphws` during a
> real bend — a Frida hardware watchpoint crashes this process), then write the
> input the game itself writes. See `findings.jsonl` (2 rows, 2026-07-30).

**Date:** 2026-07-29 · **Tier:** FACT (static-grounded + live poke-proven)
**Tool:** `tools/suspension_bend.py` (corpus root)

The question this answers: *where does the game store suspension damage for the
player, and can it be shown as a percentage?* Answer: yes — there is a real,
continuous, per-wheel bend angle with the game's own bounded clamp.

---

## 1. Memory layout

**Base `0x007C7428`, stride `1124` (281 ints), 4 wheels, order `FL, FR, RL, RR`.**

This is a **global block — PLAYER CAR ONLY**. It is *not* in the `0x48C0`
vehicle array, so AI cars' suspension bend is not here.

| offset | type | meaning |
|---|---|---|
| +1104 | byte | wheel collision / detach flag |
| **+1105** | byte | **suspension bent flag** (0 / 1) |
| **+1116** | double | **bend angle axis 1**, radians, clamped ±0.1745329251994329 (**±10°**) |
| **+1124** | double | **bend angle axis 2**, radians, same clamp |
| **+1132** | double | accumulated misalignment; **> 0.015 → ReasonCode = 4 (Suspension)** |
| +1496 | double | tyre temp carcass zone 0 (Kelvin) |
| +1504 / +1512 | double | tyre temp zones 1 / 2 |
| +1496+0x48 | double | tyre tread (1.0 = fresh) |
| +1496+0xB8 | double | brake disc temp (Kelvin) |

Per-wheel record base addresses:

| wheel | record base | bend1 address |
|---|---|---|
| FL | `0x007C7428` | `0x007C7884` |
| FR | `0x007C788C` | `0x007C7CE8` |
| RL | `0x007C7CF0` | `0x007C814C` |
| RR | `0x007C8154` | `0x007C85B0` |

### The tyre/brake block was this same array all along

The previously documented per-wheel tyre/brake block at `0x007C7A00` is **not a
separate structure**: `0x007C7A00 == 0x007C7428 + 1496`, identical 1124 stride.
It is a later field group of these same records. Anything already written
against `0x7C7A00` stays valid; this just extends the record backwards.

---

## 2. Static proof

`functions/SteeringGeometryUpdate_00442060.c`:

```c
v4 = (int)&this[281 * a2 + 163];          // a2 = wheel index, stride 281 ints = 1124
BYTE1(this[281 * a2 + 276]) = 1;          // the "bent" flag byte (+1105)

v28 = a3 * 0.08726646259971647;           // 5 deg in radians, per-event increment
v5  = flt_8AAE74[(unsigned __int8)dword_8AAE70] * 3.141592653589793;   // pseudo-random direction
v6  = sin(v5) * v28;

if ( !a2 || a2 == 3 )                     // wheels 0 and 3 add, 1 and 2 subtract
  this[281*a2 + 279] += v6;               // axis 1  (+1116)
else
  this[281*a2 + 279] -= v6;
... clamp to +-0.1745329251994329 ...
this[281*a2 + 281] += cos(v5) * v28;      // axis 2  (+1124)
... same clamp ...

// ---- the trip, lines 128-136 ----
v18 = v28 + *(double *)(v4 + 480);        // v4+480 == base + 1132
*(double *)(v4 + 480) = v18;
if ( v18 > 0.015 )
{
    v19 = v3[1984];                       // = vehicle base
    if ( !*(_DWORD *)(v19 + 16488) )      // 16488 = 0x4068 = ReasonCode
        *(_DWORD *)(v19 + 16436 + 52) = 4;   // ReasonCode = 4 (Suspension)
}
```

**Object identity** is pinned because `WheelCollisionContactFlagSetter_0043e770`
writes `LOBYTE(this[281*a2 + 276])` on the same base while
`SteeringGeometryUpdate_00442060` writes `BYTE1(...)` of that same dword — two
independent functions, same object, same stride.

**Address math:** `v4 + 480 = 0x7C7428 + 1124*w + 163*4 + 480 = +1132`.

### The clamp is literally ±10°

The clamp writes are dword pairs forming an exact double:
`this[...+279] = 1256670343` (`0x4AE47C87`, low) and
`this[...+280] = 1069963032` (`0x3FC65718`, high) → `0x3FC657184AE47C87`
= `0.17453292519943295` rad = **exactly 10°**. Negative branch uses
`-1077520616` = `0xBFC65718` → −10°.

> **Correction to `docs/12-globals-reference.md`:** that file describes the
> `0x3fc65718` value in `FUN_00442060` as a *"LifetimeClampUpperBound … 930-byte
> vehicle lifetime/damage model"* sentinel. It is not a lifetime bound — it is
> the ±10° suspension bend clamp.

### How the damage gets applied at all

`functions/RepairServiceStateDispatcher_004ed250.c` is the damage-apply/part
dispatcher. Its `this` is **veh+0xC** (not the vehicle base); `*this` is the
vehicle base. Proven by four simultaneous cross-checks against already-known
corpus offsets:

| expression | resolves to | known as |
|---|---|---|
| `this + 818` (int*) | veh+0xC+3272 = **0xCD4** | front wing ptr |
| `this + 835` | veh+0xC+3340 = **0xD18** | rear wing ptr |
| `this[3928]` | veh+0xC+15712 = **0x3D6C** | carConditions |
| `*this + 16488` | **0x4068** | ReasonCode |

partIdx map: **0–3 wheels, 4 front wing, 5 rear wing, 6–13 bodywork.**
Wheel path sets `carConditions = 2`, `ReasonCode = 6`, calls
`SubslotCleanup(veh+0x6F0, wheel)` (stride 296) and
`WheelCollisionContactFlagSetter(0x7C7428, wheel)`.

**Wheel order FL, FR, RL, RR** is confirmed statically by
`TelemetryDataDump_004d4a80.c`, whose `k = 0..3` column-header loop emits
`aFl, aFr, aRl, aRr` over this same indexing. (The corpus previously carried
this order as DERIVED / not proven.)

---

## 3. Live proof

### Organic damage — captured 2026-07-29, driver reported "swerving to the left"

| wheel | bend1 | bend2 | accum | bent flag |
|---|---|---|---|---|
| FL | +0.000° | +0.000° | 0.000000 | 0 |
| FR | +0.000° | +0.000° | 0.000000 | 0 |
| **RL** | **−1.927°** | **+1.503°** | **0.044623** | **1** |
| **RR** | **+3.323°** | **+1.072°** | **0.061743** | **1** |

Concurrently `veh+0x4068` (ReasonCode) read **4 = Suspension**, exactly as the
static path predicts once the accumulator passes 0.015 — both rear accumulators
were past it. The >5° RL/RR toe mismatch is the leftward pull the driver felt.

**Controls in the same read** (rules out other damage sources): all four tyre
puncture fields 1.0 (intact), both wing flags 1 (intact), all four wheel-detach
flags (+1104) 0, and `veh+0x3D70..0x3D7C` frozen across two samples 4 s apart.

### Address/liveness proof — all four corners

`python tools/suspension_bend.py --verify`:

```
PROOF 1 -- record identity per wheel slot
  FL @0x007C7428  tyre  50.53C  brake  39.86C  tread 0.999927   OK
  FR @0x007C788C  tyre  50.69C  brake  37.25C  tread 0.999895   OK
  RL @0x007C7CF0  tyre  52.70C  brake  44.73C  tread 0.999867   OK
  RR @0x007C8154  tyre  52.44C  brake  39.71C  tread 0.999875   OK
  => PASS: all 4 slots decode as real wheel records

PROOF 2 -- write / readback / restore on every wheel's bend field
  FL @0x007C7884  wrote 0.02 -> read +0.020000000  restored +0.000000000  PASS
  FR @0x007C7CE8  wrote 0.02 -> read +0.020000000  restored +0.000000000  PASS
  RL @0x007C814C  wrote 0.02 -> read +0.020000000  restored +0.000000000  PASS
  RR @0x007C85B0  wrote 0.02 -> read +0.020000000  restored +0.000000000  PASS
  => PASS: all 4 bend fields are live and writable
```

### Coverage status — be precise about what is and isn't proven

| corner | address math + liveness | organic in-game bend observed |
|---|---|---|
| FL | **PASS** | not yet |
| FR | **PASS** | not yet |
| RL | **PASS** | **YES** (−1.927° / +1.503°) |
| RR | **PASS** | **YES** (+3.323° / +1.072°) |

All four slots are proven to be real, live, correctly-indexed wheel records.
Organic (game-generated) bend has so far only been *witnessed* on the rears,
because that is where the driver's actual crash landed. Closing the front
half requires damaging a front corner and re-reading — the code path is
index-generic (`281 * a2`), so there is no mechanism by which fronts would
behave differently, but that is an argument, not an observation.

---

## 4. Using it as a health percentage

Use the bend angles against the game's own ±10° clamp — bounded and physical:

```c
#define SUSP_CLAMP 0.1745329251994329   /* the game's own +-10 deg clamp */

double a1 = *(double*)(0x007C7428 + 1124*w + 1116);
double a2 = *(double*)(0x007C7428 + 1124*w + 1124);
double worst  = fmax(fabs(a1), fabs(a2));
double health = 100.0 * (1.0 - fmin(1.0, worst / SUSP_CLAMP));
```

**Do not use +1132 for a percentage.** It keeps integrating past the 0.015 trip
point and is unbounded (observed 0.0446 and 0.0617). It is a good binary
"has this corner been damaged yet?" signal only.

The `+1105` byte is the cheapest boolean if all you want is a warning light.

---

## 5. What was ruled out along the way

| candidate | verdict |
|---|---|
| `veh+0x3D7C` `SuspensionLifeRemaining` | **Dead for the player.** AI-only wear countdown, gated on `veh+0x32C8 == 1` (0 for the human). Verified frozen across samples 4 s apart. `web_telemetry.c`'s `lifepct_sus` normalises it against its own session max, so it reads ~100% for the player forever. |
| `veh+0x3D6C` `carConditions` | Real and player-working, but coarse discrete tiers: 0 none / 1 wing+bodywork / 2 wheel loss. Not a percentage. |
| `veh+0x4068` `ReasonCode` | A *classification label*, and it does **not reset live** — observed stuck at 4 from a previous incident. Use as a trigger, never as current state. |
| `VehicleDamageModel_004f24f0` | Just the `damage.ini` parser (loads `WheelBend`/`WheelDetach`/etc. into a parser object). No runtime state. |
| vertex deformation list, `veh+0x40C` stride 0x20 | Real, and populated by crashes — records of `{type, severity 0..1, 1.0, x, y, z}` whose x/y/z span the car body, matching `damage.ini`'s `[VERTEX]` / `Radius*` sphere model. But it is **body** deformation, not per-wheel suspension, and is not a per-corner health value. |
| damage cluster `0x12B4` / `0x15A0` / `0x188C` | Non-monotonic with severity (read 8 after a catastrophic slam, 11 after a light graze) and all three always move together — a shared counter, not per-part health. |

`SeasonData/Vehicles/damage.ini` itself has **no suspension section**. The
relevant knobs are `[PHYSICAL]` `WheelBend=1500.0` (impulse to bend),
`WheelDetach=4500.0`, `WheelRandom=0.65` — thresholds only, not runtime state.

---

## 6. Tool

```bash
python tools/suspension_bend.py            # one-shot read of all 4 corners
python tools/suspension_bend.py --watch 60 # poll 60 s, print only on change
python tools/suspension_bend.py --verify   # the two proofs above (restores writes)
```

## 7. Open items

- Witness organic bend on a **front** corner to close the coverage table.
- Promote the FACT-tier addresses into `name_index.json`, then rerun
  `tools/gen_sdk_from_index.py` + `tools/gen_vehicle_struct_header.py`
  (these are globals, not vehicle-struct offsets — they belong in the globals
  section).
- Replace `web_telemetry.c`'s `lifepct_sus` (currently the dead `0x3D7C`) with
  the bend-based percentage above.
- Correct `docs/12-globals-reference.md`'s `LifetimeClampUpperBound` entry.
