# map_VehicleDynamicsStateReset — `sub_4F2320` (`VehicleDynamicsStateReset`)

## Identity

- Address: `0x4F2320`; file `functions/VehicleDynamicsStateReset_004f2320.c`; 27 lines.
- Signature: `_DWORD *__thiscall sub_4F2320(_DWORD *this)`.
- Callers (grep `functions/` for `VehicleDynamicsStateReset(`): `AISteeringRacingLineCorrection` (`0x42B0E0`)
  L254 (`VehicleDynamicsStateReset(this + 3);`); `VehicleYawRotationRecoveryController` (`0x4F1C40`) L54, L90
  (`VehicleDynamicsStateReset(this);`).
- Callees (from `offsets_VehicleDynamicsStateReset.csv` CALL rows): `PhysicsStateZeroingResetForSuspension` L25.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x32BC | 12988 | _DWORD | L24 (`this[3247]`) | ? owner/driver pointer; zero triggers the global reset |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x34EC | 13548 | _DWORD | L6 (`this[3387] = 0`) | ? first float of the 9-float dynamics block |
| 0x34F0 | 13552 | _DWORD | L7 | ? yaw (doc 54 names yaw at 0x34F0) |
| 0x34F4 | 13556 | _DWORD | L8 | ? dynamics field |
| 0x34F8 | 13560 | _DWORD | L9 | ? velocity X (FIELD_GLOSSARY) |
| 0x34FC | 13564 | _DWORD | L10 | ? velocity Y |
| 0x3500 | 13568 | _DWORD | L11 | ? velocity Z |
| 0x3504 | 13572 | _DWORD | L12 | ? angular/aux field |
| 0x3508 | 13576 | _DWORD | L13 | ? angular/aux field |
| 0x350C | 13580 | _DWORD | L14 | ? angular/aux field |

The `do` loop also zeroes 4 records: `*result = 0`, stride 62 dwords, starting at `this + 3436`
(unmatched L15; dword index 3436 -> byte 13744, not in the CSV).

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/VehicleDynamicsStateReset_004f2320.c`.

| value | lines | note |
|---|---|---|
| 3387-3395 | L6-L14 | `this[N]` indices (0x34EC-0x350C) |
| 3436 | L15 | first record index (byte 0x35B0) |
| 62 | L20 | record stride in dwords (248 bytes) |
| 4 | L16, L23 | record count/loop counter |
| 3247 | L24 | `this[N]` index (0x32BC) |
| 0 | L6-L14, L19 | all stores are zero |

No integer bit-pattern decode needed: every store writes integer 0 into a `_DWORD` slot.

## Logic

```
this[3387] = this[3388] = ... = this[3395] = 0        [L6,L7,L8,L9,L10,L11,L12,L13,L14]
result = this + 3436; v2 = 4                          [L15,L16]
do { *result = 0; result += 62; --v2 } while (v2)     [L19,L20,L21,L23]
if (this[3247] == 0)
    return PhysicsStateZeroingResetForSuspension(&dbl_7C7428)   [L24,L25]
return result                                         [L26]
```

## Meaning (GUESS)

- GUESS Clears a 9-float vehicle dynamics block (0x34EC-0x350C): yaw 0x34F0 and velocity 0x34F8-0x3500 per
  doc 54/FIELD_GLOSSARY, plus three unknown fields at each end [L6-L14].
- GUESS Also zeroes four 248-byte records starting at byte 0x35B0, stride 62 dwords - the same 62-dword stride
  seen for per-opponent blocks in `AISteering`/`VehicleYawRotationRecoveryController` (`v11 += 62` at L82)
  [L15-L23].
- GUESS When the owner pointer 0x32BC is null it additionally zeroes the global/default suspension physics state
  `dbl_7C7428` (same global used by `VehicleClimbAdjust` L20 as fallback) [L24,L25].
- GUESS Called by the yaw-rotation recovery controller before realigning/teleporting the car (L54, L90) and by
  the racing-line correction before a reset (L254).

## Questions

- Field 0x32BC (this[3247]) is also tested by `SpeedValueSetterWithGlobalMirror`, `VehicleClimbAdjust` and other
  functions; what object is it (driver pointer? ownership flag?), and why is the global reset taken only when it
  is null?
- Do the 4 x 248-byte records at 0x35B0 belong to per-vehicle state or per-opponent slots?
- Which semantic covers the 9 floats: roll/pitch stripped (0x34EC-0x34F4) + yaw + velocity + angular rates?
- Return value is `result` after the loop when 0x32BC is non-null - is that pointer consumed?
