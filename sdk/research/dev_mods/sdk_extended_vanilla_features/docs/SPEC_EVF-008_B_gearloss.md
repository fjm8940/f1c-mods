# SPEC_EVF-008_B — AI gear-loss / stuck-gear family (14 rows)

**Source:** ASSIGNMENT_EVF-008.md Group B
**Date:** 2026-07-28
**Emitter:** `sub_41AC50_0041ac50` (NOT `RaceCommentary_0041d630` as the assignment header states — that function is a position/leader-commentary emitter, not a gear-fault emitter. The actual gear emitter is `sub_41AC50`, confirmed by `ask.py func 0x0041ac50` and `emitter_index.json` dynamic candidates.)

## Emitter identity

| Property | Value |
|---|---|
| Function | `sub_41AC50_0041ac50` |
| Priority | score 10, `on_frame` (FACT-tier deterministic per-tick marker) |
| Called from | `EngineStressSimulation_0043ef20` (line 282: a2=0), `FuelConsumptionAndWear_004ed960` (lines 249: a2=2, line 273: a2=1/3) |
| `this` identity | vehicle struct (accesses +0x39FC=CurrentGear, +0x4064=FinishState, +0x489C=DriverID) |

## Emitter guard conditions

```
dword_7D0308 != 0          // RaceActiveFlag
flt_7CF3C8 != 0.0          // commentary enabled
dword_7C56A8 != 8          // RacePhase != post-race
dword_7C56A4 == 7          // SessionType == race only
a1 != player               // (!dword_7C54E8 || a1 != player) → AI only
a1[4121] == 0              // FinishState at veh+0x4064 must be 0 (not finished/retired)
```

All guards are satisfiable during a race. The function IS live and called per-frame.

## eid dispatch logic

```c
if (a2 == 0):
    v2 = 208                 // "smoke coming from engine" (AI) — eid 208, sdt 221
else if (a2 == 2):
    switch (CurrentGear):    // veh+0x39FC
        case 0,1 → 212       // stuck in 1st gear → sdt 225
        case 2   → 213       // stuck in 2nd gear → sdt 226
        case 3   → 214       // stuck in 3rd gear → sdt 227
        case 4   → 215       // stuck in 4th gear → sdt 228
        case 5   → 216       // stuck in 5th gear → sdt 229
        case 6   → 217       // stuck in 6th gear → sdt 230
        default → 211        // RE_OD_STUCK_7 → sdt 224
else if (a2 == 3):
    v2 = 220                 // not in Group B's eid range
else:
    return                   // a2=1 is a NOOP
```

## Per-clip analysis

### A. "lost 1st-6th gear" (eids 201-206, sdt 214-219) + "periodically slowing down" (eid 207, sdt 220)

**Verdict: NO_KNOWN_WRITER**

These 7 eids do not appear in `sub_41AC50`'s dispatch table. They are absent from both `emitter_index.json` `literal` and `dynamic` sections. A grep of all 7,138 functions for literal eid values 201-206 found zero `DriverDataTableLookupAndTimeConversion` call sites. The SDT name `PWS_AIMECHPROB0` collides with the stuck-gear clips (eids 212-217 share the same truncated SDT name `PWS_AIMECHPROB1` — see `DEEPSEEK_INSTRUCTIONS.md` §3), but mechanically these are distinct clips with distinct eids.

The human heard these clips play during isolation-injection testing, confirming the audio assets exist and the queue pipeline works. But no code path in the decompiled corpus dispatches eids 201-207.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| lost 1st gear (AI) | 214 | 201 | NO_KNOWN_WRITER |
| lost 2nd gear (AI) | 215 | 202 | NO_KNOWN_WRITER |
| lost 3rd gear (AI) | 216 | 203 | NO_KNOWN_WRITER |
| lost 4th gear (AI) | 217 | 204 | NO_KNOWN_WRITER |
| lost 5th gear (AI) | 218 | 205 | NO_KNOWN_WRITER |
| lost 6th gear (AI) | 219 | 206 | NO_KNOWN_WRITER |
| periodically slowing down (AI) | 220 | 207 | NO_KNOWN_WRITER |

### B. "smoke coming from engine" (eid 208, sdt 221)

**Verdict: LIVE_UNPROVEN_PREDICATE**

Trigger: `EngineStressSimulation` line 282 calls `sub_41AC50(vehicle, 0)` when engine durability (`this+7604`) hits 0 AND the random failure event logic fires (lines 126-282). Sets ReasonCode=1 at `veh+0x4068` (FACT: `EngineStressSimulation` line 139 writes 1 to `veh+0x4068` via `*(v19+52) = 1`).

Predicate in plain English: **AI vehicle's engine durability countdown expires AND the random noise value selects engine-failure type (as opposed to gearbox/other).** This requires an AI's engine to actually fail — a genuine rare event during a race.

Evidence:
- `EngineStressSimulation_0043ef20.c` lines 126-282: full failure event logic
- `sub_41AC50` line 282: `sub_41AC50(vehicle, 0)` — unambiguously wired
- Cooldown: cd=15.0, not blocked (CONFIRMED: live event-table read)

Uncertainty: how often does `this+7604` actually reach 0 for an AI car? If AI durability is protected (e.g., scaled by difficulty), this may never fire in practice. The live measurement of `EngineLifeRemaining` (at `veh+0x3D70`, a DIFFERENT timer in `FuelConsumptionAndWear`) showed 20000-41000s starting values draining at 1.000/s — not directly applicable here because `EngineStressSimulation` uses its OWN timer at `this+7604` (`EngineStressSimulation`'s `this` is not the vehicle — it's an engine-subsystem object with `this+7936` pointing to the vehicle).

### C. "stuck in 1st-6th gear" (eids 212-217, sdt 225-230) + "other driver stuck in 7th" (eid 211, sdt 224)

**Verdict: LIVE_UNPROVEN_PREDICATE**

Trigger: `FuelConsumptionAndWear` case 2 (lines 240-255):
1. Function must be called with `sub_4D21D0()` returning nonzero (RacePhase > 4)
2. Timer at `this[2+3929]` (veh+0x3D68) must count down and expire (go negative)
3. `this[3686] * 0.5 <= this[3685]` must be true (engine durability condition: `veh+0x3998 * 0.5 <= veh+0x3994`, both unnamed engine-wear floats)
4. `*((DWORD*)this + 3928) == 0` (veh+0x3D60 must be 0 — first-fire latch)

**Why the predicate is effectively unsatisfiable:** The timer at `veh+0x3D68` is (like its siblings at `veh+0x3D64/0x3D6C/0x3D70`) initialized by `EngineWearFailureSimulation_004ee150` to a random value in the range ~20000-41000 seconds (the FACT live measurement of EngineLifeRemaining at +0x3D70). It decrements at exactly 1.000/s via `FuelConsumptionAndWear` line 118: `this[i+3929] -= a2` where a2 = delta time. In a 53-lap race (~65 min = 3900 s), the lowest observed remaining value was 10400 s (FACT, `EngineLifeRemaining` note). The timer does NOT reach zero within any normal race distance.

Evidence:
- `FuelConsumptionAndWear_004ed960.c` lines 112-255: full case-2 trigger chain
- FACT live measurement of `veh+0x3D70` (EngineLifeRemaining) draining at 1.000/s from ~20000-41000s, never reaching zero in a 53-lap race
- `EngineWearFailureSimulation_004ee150.c` lines 13-35: initializes all four timer slots per frame when `dword_7CEA5C > 0 && <= 2`
- Cooldown: eids 212-217 cd=15.0, eid 211 cd=15.0, all blocked=false

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| stuck in 1st gear (AI) | 225 | 212 | LIVE_UNPROVEN_PREDICATE |
| stuck in 2nd gear (AI) | 226 | 213 | LIVE_UNPROVEN_PREDICATE |
| stuck in 3rd gear (AI) | 227 | 214 | LIVE_UNPROVEN_PREDICATE |
| stuck in 4th gear (AI) | 228 | 215 | LIVE_UNPROVEN_PREDICATE |
| stuck in 5th gear (AI) | 229 | 216 | LIVE_UNPROVEN_PREDICATE |
| stuck in 6th gear (AI) | 230 | 217 | LIVE_UNPROVEN_PREDICATE |
| other driver stuck in 7th | 224 | 211 | LIVE_UNPROVEN_PREDICATE |

## uncertainty (mandatory, per group)

1. **Reinitialization race in `EngineWearFailureSimulation`**: lines 13-16 write `FLT_MAX` to ALL FOUR timer slots BEFORE the gate check. If `dword_7CEA5C` is 0 (wear mode disabled), all timers = FLT_MAX permanently — components never fail. If `dword_7CEA5C` is 1 or 2, the loop provides new random values every frame. Call order relative to `FuelConsumptionAndWear` is unverified — if `EngineWearFailureSimulation` runs AFTER `FuelConsumptionAndWear` in the tick loop, the decrement from `FuelConsumptionAndWear` is immediately overwritten by fresh random values, making the counters never actually deplete. The live measurement of EngineLifeRemaining draining at 1.000/s suggests the call order is correct (decrement-then-reset → net erosion), but this is unverified for the specific `veh+0x3D68` slot.

2. **The 7 "lost gear" eids (201-207) may be emitted through a dynamic lookup table** (like `DriverNameIDLookup`) that emitter_index.json cannot statically resolve. Possibility: same SDT name collision masks a separate emitter at a different function. The fact that `sub_41AC50` handles EVERY other gear-related eid (208, 211-217, 220) but skips 201-207 is suspicious — these may be deliberate unused alternates, or they may have an emitter in an unanalyzed function.

3. **No live counter measurement for `veh+0x3D68` specifically** — the live FACT covers `veh+0x3D70` (slot 3, engine/oil). Slot 2 (gear) at `veh+0x3D68` is assumed to behave identically (same writer `EngineWearFailureSimulation`, same decrement loop in `FuelConsumptionAndWear`), but this is DERIVED, not FACT.

4. **`EngineStressSimulation`'s timer at `this+7604`** has no live measurement — we know it exists from the decompiled code, but whether AI cars ever reach the failure threshold under normal gameplay is unverified.
