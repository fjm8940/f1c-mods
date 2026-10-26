# map_SpeedValueSetterWithGlobalMirror — `sub_4F1890` (`SpeedValueSetterWithGlobalMirror`)

## Identity

- Address: `0x4F1890`; file `functions/SpeedValueSetterWithGlobalMirror_004f1890.c`; 10 lines.
- Signature: `int __thiscall sub_4F1890(int this, float a2)`.
- Callers (grep `functions/` for `SpeedValueSetterWithGlobalMirror(`): `AIAbility` (`0x429470`) L135, L169;
  `AISteeringRacingLineCorrection` (`0x42B0E0`) L131; `PitStopProgress` (`0x4A68B0`) L24, L65;
  `PitStopOperationTimer` (`0x4A6340`) L145; `PitStopDecisionTireCompound` (`0x42A3D0`) L87;
  `sub_429960` (`0x429960`) L262.
- `functions/` call sites show one argument (receiver dropped by Hex-Rays); typed variants show the full form
  `SpeedValueSetterWithGlobalMirror((int)(this + 3), value)` in `functions_typed_20260930/AIAbility_00429470.c`
  L135, L169, i.e. callee `this` = caller `this` + 12 bytes.
- Callees (from `offsets_SpeedValueSetterWithGlobalMirror.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x32BC | 12988 | _DWORD | L6 (`*(_DWORD *)(this + 12988)`) | ? owner/driver pointer or gate (truthy test) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x3978 | 14712 | float | L5 (`*(float *)(this + 14712) = a2`) | ? speed/value field (vehicle+14724 if this = veh+12) |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/SpeedValueSetterWithGlobalMirror_004f1890.c`.

| value | lines | note |
|---|---|---|
| 14712 | L5 | `this` offset (0x3978) |
| 12988 | L6 | `this` offset (0x32BC) |
| 0.0, 1.0 | — | no literal 0/1 in source; L7 is a `!result` test |

No integer bit-pattern is stored into a float field: L5 copies the float argument unchanged.

## Logic

```
*(float *)(this + 0x3978) = a2        [L5]
result = *(this + 0x32BC)             [L6]
if (result == 0) dbl_7C75C4 = a2      [L7,L8]
return result                          [L9]
```

## Meaning (GUESS)

- GUESS Writes `a2` into the field at `this+0x3978` and returns the dword at `this+0x32BC` (pointer or flag) [L5-L9].
- GUESS When that owner/gate dword is null, the value is mirrored into global `dbl_7C75C4`; `VehicleSetupInitialization_004e49b0.c`
  L145 writes `dbl_7C75C4` into `this+14712`, so the global is the default used to initialise the field [L6-L8].
- GUESS The value is a speed target/command: `AIAbility` passes the float bits of 1.0 (L135) and a computed value
  (L169); `AISteeringRacingLineCorrection` L127-L131 passes `min(this[4474]*1.5 + this[3681], this[3682])`;
  pit-stop callers pass fuel/stop values.
- GUESS Outside evidence: `name_index.json` says 0x4F1890 writes vehicle+0x3984 (fuel leaf,
  RESTART_FUEL_REVIEW 2026-09-21), matching `this` = vehicle+0xC.

## Questions

- `functions/` call sites appear one-argument (`SpeedValueSetterWithGlobalMirror(v)`); is the dropped receiver
  really caller `this+3` (12 bytes)? Confirm in disassembly.
- What object/field is `this+0x32BC`? Other functions read it as a pointer (`AIVehiclePathPositionTracking` L44)
  and as a small value (`ParticleEffectUpdateDispatch` L83).
- Where else is `dbl_7C75C4` written besides L8?
- The callee returns the gate dword, not `a2`; do callers use the return?
