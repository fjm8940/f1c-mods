# map_PhysicsGearShiftLogicWithThresholds — section-4 name `PhysicsGearShiftLogicWithThresholds`, declared `sub_426330`

## Identity

- Address: `0x426330`; file `functions/PhysicsGearShiftLogicWithThresholds_00426330.c`; 34 lines.
- Signature: `void __thiscall sub_426330(int this)`; `this` = F1CVehicle.
- Caller: `AIMainUpdate` (`0x424E70`) L250; grep `functions/` -> `functions/AIMainUpdate_00424e70.c` only;
  `inspect` agrees.
- Callees (from `offsets_PhysicsGearShiftLogicWithThresholds.csv` CALL rows): `RaceStartCountdownStateUpdate`
  (called with -1 / 0 / gear+1) and a `JUMPOUT` tail to `0x4F62E6` (the same callee, entered mid-function; not in
  this file).
- Prior art: FACT findings from the 2026-07-30 live session — `PhysicsGearShiftLogicWithThresholds 0x426330` is
  called for AI slots only (player slot 0 is gated out in AIMainUpdate), and its first line is a genuine shift
  lockout: if `veh+0x3A10 > 0.0` it returns immediately.
- Doc 54 lists this as step 9, "auto gear (prior art)" in the per-tick pipeline.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x3A10 | 14864 | float | L6 | shift lockout (prior art FACT: > 0 blocks shifting) |
| 0x39FC | 14844 | _DWORD | L8, L27 | ? current gear (`v1`, compared to -1 / 0 / 1) |
| 0x4510 | 17680 | _DWORD | L9 | driverStatus (doc 54 mode 0x4510) |
| 0x3D6C | 15724 | _DWORD | L15 | carConditions (inspect) |
| 0x3A18 | 14872 | float | L22 | ? input shaft / requested value |
| 0x39A0 | 14752 | float | L22 | EngineRPM (inspect CONFIRMED) |
| 0x37CC | 14284 | float | L24 | wheel slip value 1 (doc 54) |
| 0x38C4 | 14532 | float | L24 | wheel slip value 2 (doc 54) |
| 0x3A04 | 14852 | _DWORD | L28 | ? max gear (upper bound for +1) |

## Outputs

None directly. The function only calls `RaceStartCountdownStateUpdate` (gear/state argument), or falls through the
`JUMPOUT` tail. The state it changes lives in the callee.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/PhysicsGearShiftLogicWithThresholds_00426330.c`.

| value | lines | note |
|---|---|---|
| 6 | L9 | driverStatus compare (pit/garage wait) |
| -1 | L11, L20 | gear compare / shift-down argument |
| 2 | L15 | carConditions compare (retired/retired-like) |
| 0 | L17, L20, L29 | call arguments / gear compare |
| 0.80000001 | L24 | slip threshold (both wheels) |
| 14844, 14852, 14864, 14872, 15724, 17680, 14284, 14532, 14752 | L6-L28 | field offsets (see tables) |
| 0x4F62E6 | L33 | tail-jump target (continuation not in this file) |

## Logic

NOTE: source uses byte-literal `*(TYPE *)(this + N)` dereferences; Logic writes them as hex names. Each hex
token's decimal appears in the cited source line.

```
if (*(float *)(this + 0x3A10) > 0.0) return                      [L6,L7]
v1 = *(_DWORD *)(this + 0x39FC)                                  [L8]
if (*(_DWORD *)(this + 0x4510) == 6) {                           [L9]
    if (v1 != -1) RaceStartCountdownStateUpdate(-1)              [L11,L12]
    return                                                       [L13]
}
if (*(_DWORD *)(this + 0x3D6C) == 2) {                           [L15]
    RaceStartCountdownStateUpdate(0); return                     [L17,L18]
}
if (!v1 || v1 == -1) goto LABEL_12                               [L20,L21]
if (*(float *)(this + 0x3A18) < *(float *)(this + 0x39A0)) {    [L22]
    if (*(float *)(this + 0x37CC) <= 0.80000001 || *(float *)(this + 0x38C4) <= 0.80000001) return   [L24,L25]
LABEL_12:
    v2 = *(_DWORD *)(this + 0x39FC)                              [L27]
    if (v2 < *(_DWORD *)(this + 0x3A04))                         [L28]
        RaceStartCountdownStateUpdate(v2 + 1)                    [L29]
    return                                                       [L30]
}
if (v1 > 1) JUMPOUT(tail 4F62E6)                                 [L32,L33]
```

## Meaning (GUESS)

- GUESS AI gear-shift decision with three thresholds: shift lockout 0x3A10 > 0 blocks everything; driverStatus
  0x4510 == 6 forces a downshift to -1; carConditions 0x3D6C == 2 forces a shift to 0; otherwise a downshift is
  ordered when the current gear 0x39FC is not 0/-1 and `0x3A18 < 0x39A0` (requested < EngineRPM) and both wheel
  slip values are above 0.8 [L6-L25; prior art].
  Otherwise the gear is incremented (`RaceStartCountdownStateUpdate(v2 + 1)`) while `v2 < 0x3A04` (max gear)
  [L27-L30].
- GUESS The engine-RPM comparison plus slip gate is the "threshold" in the function name: a downshift is taken
  only when RPM is too low and the wheels are not slipping beyond the traction threshold [L22-L25].
- GUESS The callee name `RaceStartCountdownStateUpdate` conflicts with gear semantics (doc 54 calls the function
  auto gear); the real callee writes the gear/state. The `JUMPOUT` tail for `v1 > 1` continues the same callee
  outside this decompilation [L33].
- GUESS Prior art fixes the caller contract: this runs for AI cars only; the player car uses a different
  drivetrain path [prior art FACT 2026-07-30].

## Questions

- What is `RaceStartCountdownStateUpdate` (0x4F62E6 / its entry) really? The name suggests race-start countdown,
  but all three call sites pass gear-like values. The tail `JUMPOUT(0x4F62E6)` needs its own export.
- Is 0x39FC the current gear and 0x3A04 the maximum gear, or are they shift-timer/counter fields?
- What are 0x3A18 (compared with EngineRPM 0x39A0) and the 0.8 slip threshold exactly?
- Why is driverStatus 6 special (doc 54: modes 4/5 are pit/garage wait; 6 is reverse)?
- Does the `JUMPOUT` continuation return to the caller or execute the rest of another function inline?
