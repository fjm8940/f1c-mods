# 47 — The three engine.ini "map" tokens: which ones the game actually reads

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`, image base 0x400000, non-ASLR)
**Date:** 2026-07-31 · **Status:** STATIC (IDB-verified) + **LIVE A/B PROVEN** (driver in the loop)
**Subject car:** Williams 2001_FW23 (`SEASONDATA\VEHICLES\WILLIAMS\2001_FW23`)

## Abstract (TLDR)

- `EngineMapSetupFieldDefinitions` (0x004F3C10) parses **three** setup blocks from a
  vehicle's `engine.ini`: `RevLimit*`, `EngineMap*`, `EngineBrakingMap*`.
- **`RevLimitSetting` is live.** It resolves into the static player physics object at
  `this+7388`. Verified live to the digit: `RevLimitRange=(17400.0, 100.0, 11)` with
  `RevLimitSetting=5` → 17 900 rpm → **1874.48 rad/s** read from memory.
- **`EngineMapSetting` is DEAD.** Parsed, stored, and never resolved into the physics
  object. The shipped `engine.ini` says so itself — `(unimplemented)` — and the code
  agrees. Changing it does nothing.
- **`EngineBrakingMapSetting` is LIVE and it matters.** It resolves to
  `this+7248 = 0x7C9078`, and **zeroing it roughly doubles coasting deceleration**,
  proven by driver-in-the-loop A/B with the value re-armed 20×/s so it could not revert.
- A **clamp is suspected** but untested: stock and 10× stock produce indistinguishable
  coast curves while 0.0 differs from both by ~2×.
- Corrects `docs/46`, which listed both map tokens as "none (performance)". One is not a
  performance knob at all; the other is a real driving knob.

## 1. Method

Static: Hex-Rays decompilation, cross-referenced through `strings_index.json` and the
`functions/` corpus. Runtime: `tools/poke.py` for reads, Frida `Interceptor` /
`NativeFunction` for everything else, with the human driver reporting car behaviour.

All "this+N" offsets below are byte offsets into the **static player physics object at
`0x7C7428`** — *not* the vehicle struct. That object identity is FACT-tier: an
`Interceptor` on `EngineStressSimulation` (0x43EF20) logged 2001 calls in 5 s with
exactly **one** distinct `ecx`, `0x7c7428`.

## 2. Static: three blocks, one parser

```c
int __stdcall sub_4F3C10(int a1, char *a2)
{
  CRTMathHelper_e1f0(a2 + 5488, a1, aRevlimitrange,    aRevlimitsettin, 0.1047197551196598);
  CRTMathHelper_e1f0(a2 + 5552, a1, aEnginemaprange,   aEnginemapsetti, 1.0);
  return CRTMathHelper_e1f0(a2 + 5616, a1, aEnginebrakingm_0, aEnginebrakingm, 1.0);
}
```

Three 64-byte setup blocks holding `(base, step, count, setting)`. The RevLimit scale
`0.1047197551196598` is `2π/60` — rpm to rad/s.

`sub_43C250` is the function that copies the parsed engine block into the physics object.
It resolves exactly **two** of the three:

| line | block | resolved to |
|---|---|---|
| 487 | `a2+5488` RevLimit | `setting*step + base` → maxRPM |
| 471 → 480 | `a2+5616` EngineBrakingMap | `(setting*step + base) * 0.09549296585513721` → `this+7248` |
| — | `a2+5552` **EngineMap** | **never read** |

## 3. Runtime results

### 3.1 RevLimit — control, confirms the chain works

`RevLimitRange=(17400.0, 100.0, 11)`, `RevLimitSetting=5` → 17 400 + 5×100 = 17 900 rpm.
Live `this+7388` = **1874.48 rad/s** = 17 900 rpm. Exact. The engine.ini demonstrably
reaches this object.

### 3.2 EngineMap — inert

Predicted resolved value for `EngineMapRange=(0, 1, 5)`, `EngineMapSetting=2` is `2.0`.
A live scan of the entire physics object (0…8200, doubles and floats) found only one
`2.0`, at `this+7076`, and it is the head of an unrelated descending table
(`2.0 / 1.9375 / 1.875`, step −1/16). No resolved engine-map value exists in the object.

### 3.3 EngineBrakingMap — live, and verified to seven figures

`EngineBrakingMapRange=(0.0, 0.000125, 6)`, `EngineBrakingMapSetting=2`:

```
predicted  2 * 0.000125 * 0.09549296585513721 = 2.3873241463784304e-05
live read  0x7C9078                           = 2.387324e-05
```

The constant `0.09549296585513721` is `60/(2π)/100`, so **stored value × engine speed in
rad/s = applied-throttle fraction**, which reproduces the engine.ini comment's own worked
example: `2 * 0.000125 * 16000 rpm = 4 %` applied throttle at zero input throttle.

### 3.4 The A/B that proves it is consumed

Storing a value is not the same as reading it. Two coast runs, driver fully off throttle,
in gear, hands off, with the value **re-armed every 50 ms** so it could not revert:

| 188 → 100 km/h coasting | time |
|---|---|
| `0x7C9078` = **0.0** | **≈ 3.7 s** |
| `0x7C9078` = **2.387e-4** (10×) | **≈ 7.0 s** |

Roughly **2× the deceleration with the map zeroed** — the expected direction, since the
map sets a minimum applied throttle at zero driver input, and removing it removes the
throttle floor that was opposing engine braking.

Compared over a matched *speed band* rather than matched clock time, because the two runs
started from different speeds. In the 0.0 run engine speed floored at 4009 rpm (idle) from
t = 8 s and the car entered a much gentler idle coast, so only the pre-idle portion is
comparable.

### 3.5 Persistence

`0x7C9078` is **LATCHED**: 0 overwrites in 360 polls across 18 s of live driving.

## 4. Open: a suspected clamp

Stock (`2.387e-5`, nominally a 4 % floor) and 10× (`2.387e-4`, nominally 42 %) gave
**indistinguishable** coast curves, while 0.0 differed from both by ~2×. A 42 % throttle
floor should not decelerate like a 4 % one.

Most likely the consumer clamps to the legal range maximum: `EngineBrakingMapRange`'s
count of 6 caps at setting 6 → `0.00075` → stored `7.16e-5`, and the 10× write of
`2.387e-4` is far beyond it. **Untested.** The clean test is an armed run at exactly
`7.16e-5` plus a stock run from a matched ~190 km/h start.

## 5. Corrections to prior corpus documents

- `docs/46` §4.2 listed `EngineMapSetting`/`Range` as "none (performance)". It is **inert**,
  not merely performance-only.
- `docs/46` §4.2 listed `EngineBrakingMapSetting`/`Range` as "none (performance)". It is a
  **real driving knob** with a measured ~2× effect on coasting deceleration.

## 6. Incidental findings worth reusing

- **Player road speed is a signed float at `veh+0x60`** (`0x5EC738` for slot 0), m/s,
  with mirrors at `veh+0x3500` and `veh+0x4440`. The physics object has no speed field.
  `name_index.json` had no usable speed entry before this.
- **Writes made while the game is PAUSED are suspected not to survive the unpause** (OPEN,
  one observation). A silent revert looks exactly like a RECALCULATED offset and nearly
  produced a false negative here. Set up live experiments with the game running.
- **Frida MCP scripts must stay under ~20 s.** A 75 s `Thread.sleep` loop exceeds the tool
  timeout *and then blocks the session* for its full duration; `tools/poke.py` keeps working
  meanwhile.

## References

- `functions/EngineMapSetupFieldDefinitions_004f3c10.c`, `functions/sub_43C250_0043c250.c`
- `findings.jsonl`: `enginemapsetting-is-unimplemented`,
  `enginebrakingmap-resolves-to-0x7c9078`, `enginebrakingmap-consumed-live-proven`,
  `0x7c9078-is-latched`, `pause-wipes-pokes-suspected`, `player-speed-field-veh-0x60`,
  `frida-mcp-long-sleep-blocks-session`
- Sibling: `docs/46-player-specific-mechanical-failures.md`,
  `mods/extended_vanilla_features/docs/ENGINE_FAILURE.md`
- Tier legend: FACT (live-poke proven) > CONFIRMED (2 independent sources) > DERIVED
  (grounded, unproven) > GUESS

---
*Changelog: 2026-07-31 — initial, static + live A/B with driver in the loop.*
