# map_SuspensionRatioClamp — `sub_4E8780` (`SuspensionRatioClamp`)

## Identity

- Address: `0x4E8780`; file `functions/SuspensionRatioClamp_004e8780.c`; 13 lines.
- Signature: `double __thiscall sub_4E8780(float *this)`.
- Callers (grep `functions/` for `SuspensionRatioClamp(`): `AISteeringCorrection` (`0x426CF0`) L47:
  `v7 = SuspensionRatioClamp(this + 12);` — the caller passes `this + 12` bytes, so callee `this` = vehicle + 0xC
  in that path.
- Callees (from `offsets_SuspensionRatioClamp.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x39AC | 14764 | float | L5 (`this[3691] == 0.0`) | ? denominator/normalizer (shared with BrakeThermalModel) |
| 0x39A8 | 14760 | float | L7 (`this[3690] / this[3691]`) | ? subtracted constant term (shared with BrakeThermalModel) |

## Outputs

None. The function returns a `double`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/SuspensionRatioClamp_004e8780.c`.

| value | lines | note |
|---|---|---|
| 3691 | L5, L7 | `this[N]` index (0x39AC) |
| 3690 | L7 | `this[N]` index (0x39A8) |
| 0.0 | L5, L7(implicit), L8, L9 | divide guard / lower clamp |
| 1.0 | L10, L11 | upper clamp |

## Logic

NOTE: source uses scaled `this[N]` indexing; the Logic keeps the source-literal index. No hex tokens are used on
scaled lines; byte offsets are in the tables.

```
if (this[3691] == 0.0) return 0.0        [L5,L6]
result = -(this[3690] / this[3691])      [L7]
if (result < 0.0) return 0.0             [L8,L9]
if (result > 1.0) return 1.0             [L10,L11]
return result                             [L12]
```

## Meaning (GUESS)

- GUESS Returns `clamp(-(this[3690]/this[3691]), 0, 1)` with a divide-by-zero guard [L5-L12].
- GUESS The same two fields are read by `BrakeThermalModel` (`this[3690]`, `this[3691]`; phase-1 map), whose
  expression is `(v2^2 * this[3921] * this[3920] / this[3698] - this[3690]) / this[3691]`; setting the quadratic
  term to zero gives exactly `-this[3690]/this[3691]`, so this function is the zero-speed baseline of that ratio
  (same `this` = veh+0xC base).
- GUESS It is a normalized grip/slip ratio used by the traction-control path: caller `AISteeringCorrection` L47 is
  the pedal post-processor (doc 54 section 3), which pulls throttle toward this value below 0.9 slip.
- GUESS The zero-denominator early-out means vehicle setup parameters are not yet loaded.

## Questions

- If `this` = veh+0xC, the fields are vehicle+0x39B4/0x39B8; is that consistent with BrakeThermalModel's
  vehicle-relative view?
- What physically are 0x39A8/0x39AC (setup latched parameters)?
- Does the caller use the result directly as a 0..1 target or scale it?
