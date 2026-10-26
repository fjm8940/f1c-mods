# map_VehicleClimbAdjust — `sub_4EFA40` (`VehicleClimbAdjust`)

## Identity

- Address: `0x4EFA40`; file `functions/VehicleClimbAdjust_004efa40.c`; 22 lines.
- Signature: `void __thiscall sub_4EFA40(int this, char a2)`.
- Callers (grep `functions/` for `VehicleClimbAdjust(`): `AIMainUpdate` (`0x424E70`) L236, L297 (`(0)`);
  `AISteeringRacingLineCorrection` (`0x42B0E0`) L132 (`(0)`); `sub_4A00A0` L52 (`(0)`); `sub_4C6B60` L37
  (`return VehicleClimbAdjust(1);`, only when `dword_7C56A4 == 7` and `dword_7C56A8` is 3 or 4).
- Callees (from `offsets_VehicleClimbAdjust.csv` CALL rows): `SuspensionVibration` L20.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x32BC | 12988 | _DWORD | L5 (`*(_DWORD *)(this + 12988)`) | ? owner/driver pointer (truthy gate) |
| 0x3994 | 14740 | float | L9 (`*(float *)(this + 14740)`) | ? height/climb offset (clamped upward) |
| 0x399C | 14748 | float | L9 (`*(float *)(this + 14748)`) | ? minimum height reference |
| 0x39C0 | 14784 | float | L15 (`*(float *)(this + 14784)`) | ? noise scale |
| 0x39C4 | 14788 | float | L15 (`*(float *)(this + 14788)`) | ? noise offset |
| 0x39BC | 14780 | float | L15 (`*(float *)(this + 14780)`) | ? noise base term |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x3994 | 14740 | _DWORD (float bits) | L10 (`*(_DWORD *)(this + 14740) = *(_DWORD *)(this + 14748)`) | ? height/climb offset raised to reference |
| 0x39B8 | 14776 | float | L15 (`*(float *)(this + 14776) = ...`) | ? suspension vibration value |

Globals: L14 increments `dword_8AAE70`; L20 falls back to `SuspensionVibration(&dbl_7C7428, a2)`.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/VehicleClimbAdjust_004efa40.c`.

| value | lines | note |
|---|---|---|
| 12988 | L5 | `this` offset (0x32BC) |
| 14740 | L9, L10 | `this` offset (0x3994) |
| 14748 | L9, L10 | `this` offset (0x399C) |
| 14776 | L15 | `this` offset (0x39B8) |
| 14784 | L15 | `this` offset (0x39C0) |
| 14788 | L15 | `this` offset (0x39C4) |
| 14780 | L15 | `this` offset (0x39BC) |
| 0.0, 1.0 | — | none in source (a2 is a char flag; L10 copies float bits with `_DWORD` store) |

No integer bit-pattern is decoded: L10 copies the bits of the same float field.

## Logic

```
if (*(this + 0x32BC) != 0) {                                    [L5]
  if (a2) {
    if (*(float *)(this + 0x3994) < *(float *)(this + 0x399C))  [L9]
      *(this + 0x3994) = *(this + 0x399C)                       [L10]
  } else {
    v2 = flt_8AB274[(unsigned __int8)dword_8AAE70++]            [L14]
    *(float *)(this + 0x39B8) = v2 * *(float *)(this + 0x39C0)  [L15]
                              + *(float *)(this + 0x39C4) + *(float *)(this + 0x39BC)   [L15]
  }
} else {
  SuspensionVibration(&dbl_7C7428, a2)                          [L20]
}
```

## Meaning (GUESS)

- GUESS With a valid owner (`this+0x32BC` non-null): when `a2` is set, raises the height/climb field 0x3994 to
  at least the reference 0x399C; otherwise computes a vibration value into 0x39B8 from the lookup table
  `flt_8AB274` indexed by the incrementing global `dword_8AAE70`, scaled by 0x39C0 and offset by 0x39C4/0x39BC
  [L5-L15].
- GUESS Without an owner it forwards to the global/default suspension vibration state `&dbl_7C7428` - the same
  global used by `VehicleDynamicsStateReset` for its physics fallback [L20].
- GUESS Callers: `AIMainUpdate` calls the `a2=0` vibration variant on pit-entry setup (L236) and on pit release
  (L297); `sub_4C6B60` calls the `a2=1` variant only in a race (`dword_7C56A4 == 7`) with session state 3/4.
- GUESS The 0x39B8..0x39C4 fields overlap the region `BrakeThermalModel`/`SuspensionRatioClamp` read on the
  veh+0xC base (0x39B4/0x39B8), so this is a shared suspension/ride-height block [Questions].

## Questions

- Base: is `this` the vehicle base or the veh+0xC base used by `BrakeThermalModel`? If veh+0xC, all offsets shift
  by 0xC and 0x39B8 here equals `BrakeThermalModel`'s `this[3690]`.
- What is the lookup table `flt_8AB274` and who increments/wraps `dword_8AAE70`?
- Is `a2` only "raise" (1) / "vibrate" (0), or a mode enum?
- How does `SuspensionVibration(&dbl_7C7428, a2)` update the global default, and when is that default copied
  into a vehicle?
