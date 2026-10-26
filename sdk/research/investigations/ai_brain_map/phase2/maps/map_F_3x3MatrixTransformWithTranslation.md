# map_F_3x3MatrixTransformWithTranslation — `sub_4E9BD0` (`F_3x3MatrixTransformWithTranslation`)

## Identity

- Address: `0x4E9BD0`; file `functions/F_3x3MatrixTransformWithTranslation_004e9bd0.c`; 9 lines.
- Signature: `void __thiscall sub_4E9BD0(float *this)`.
- Callers (grep `functions/` for `F_3x3MatrixTransformWithTranslation(`): `AIRacingLinePathFollowing`
  (`0x4E9CA0`) L102; `AIVehiclePathPositionTracking` (`0x4EF7C0`) L43; `ParticleEffectUpdateDispatch`
  (`0x4EB430`) L128; `SuspensionAndWheelPhysics` (`0x4E5F60`) L851.
- Callees (from `offsets_F_3x3MatrixTransformWithTranslation.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x3B54 | 15188 | float | L3, L4, L5 | transform source X (`this[3797]`) |
| 0x3B58 | 15192 | float | L3, L4, L5 | transform source Y (`this[3798]`) |
| 0x3B5C | 15196 | float | L3, L4, L5 | transform source Z (`this[3799]`) |
| 0x34B0 | 13488 | float | L3 | rotation row 0 col 0 (`this[3372]`) |
| 0x34B4 | 13492 | float | L3 | rotation row 0 col 1 (`this[3373]`) |
| 0x34B8 | 13496 | float | L3 | rotation row 0 col 2 (`this[3374]`) |
| 0x34BC | 13500 | float | L4 | rotation row 1 col 0 (`this[3375]`) |
| 0x34C0 | 13504 | float | L4 | rotation row 1 col 1 (`this[3376]`) |
| 0x34C4 | 13508 | float | L4 | rotation row 1 col 2 (`this[3377]`) |
| 0x34C8 | 13512 | float | L5 | rotation row 2 col 0 (`this[3378]`) |
| 0x34CC | 13516 | float | L5 | rotation row 2 col 1 (`this[3379]`) |
| 0x34D0 | 13520 | float | L5 | rotation row 2 col 2 (`this[3380]`) |
| 0x34D4 | 13524 | float | L6 | translation X (`this[3381]`; FIELD_GLOSSARY position X) |
| 0x34D8 | 13528 | float | L7 | translation Y (`this[3382]`; FIELD_GLOSSARY position Y) |
| 0x34DC | 13532 | float | L8 | translation Z (`this[3383]`; FIELD_GLOSSARY position Z) |

No zero-offset or unscaled reads; the CSV contains rows for every source access.

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x3B60 | 15200 | float | L3, L6 | transformed point X (`this[3800]`; read-modify-write at L6) |
| 0x3B64 | 15204 | float | L4, L7 | transformed point Y (`this[3801]`; read-modify-write at L7) |
| 0x3B68 | 15208 | float | L5, L8 | transformed point Z (`this[3802]`; read-modify-write at L8) |

## Constants

Grouped by value; every numeric literal except 0/1 in
`functions/F_3x3MatrixTransformWithTranslation_004e9bd0.c`.

| value | lines | note |
|---|---|---|
| 3372, 3373, 3374 | L3 | rotation row 0 indices |
| 3375, 3376, 3377 | L4 | rotation row 1 indices |
| 3378, 3379, 3380 | L5 | rotation row 2 indices |
| 3381, 3382, 3383 | L6, L7, L8 | translation indices |
| 3797, 3798, 3799 | L3, L4, L5 | source vector indices |
| 3800, 3801, 3802 | L3-L8 | output vector indices |

## Logic

NOTE: source uses scaled `this[N]` indexing; Logic keeps the source-literal index and no hex tokens. Byte
offsets are in the tables above.

```
this[3800] = this[3374] * this[3799] + this[3373] * this[3798] + this[3797] * this[3372]   [L3]
this[3801] = this[3377] * this[3799] + this[3376] * this[3798] + this[3797] * this[3375]   [L4]
this[3802] = this[3380] * this[3799] + this[3379] * this[3798] + this[3797] * this[3378]   [L5]
this[3800] = this[3381] + this[3800]                                                       [L6]
this[3801] = this[3382] + this[3801]                                                       [L7]
this[3802] = this[3383] + this[3802]                                                       [L8]
```

## Meaning (GUESS)

- GUESS Computes `out = M * v + t` where `v` = `(this[3797], this[3798], this[3799])` at
  `0x3B54..0x3B5C`, `M` = 3x3 matrix at `0x34B0..0x34D0` (`this[3372..3380]`), `t` = `0x34D4..0x34DC`
  (`this[3381..3383]`), and writes the result in place at `0x3B60..0x3B68` (`this[3800..3802])`
  [L3-L8].
- GUESS This is the vehicle-local-to-world point transform used by path following and suspension
  (`AIRacingLinePathFollowing` L102, `SuspensionAndWheelPhysics` L851) [callers].
- GUESS The same matrix `0x34B0..0x34D0` is used by `F_3DTransformWithMatrixMultiplication` (`0x4EFB20`),
  which additionally copies `0x34E0..0x34E8` into its third argument; this function is the shorter variant
  that leaves the transformed point in the vehicle struct itself [cross-map].
- GUESS Translation `0x34D4..0x34DC` is the pose position (FIELD_GLOSSARY position X/Y/Z, written by
  `AIRacingLinePathFollowing`) [FIELD_GLOSSARY].
- GUESS The source vector `0x3B54..0x3B5C` is a local point (wheel contact / path sample) staged by the
  caller immediately before this call [callers].

## Questions

- Is the matrix at `0x34B0` row- or column-major? The expressions match `row . v` for a row-major
  matrix only if the `this[3372..3380]` are rows; confirm with `EulerToRotationMatrix` output layout.
- `0x3B54..0x3B5C` has no name in `FIELD_GLOSSARY`; who writes it before each call?
- `ParticleEffectUpdateDispatch` L128 and `AIVehiclePathPositionTracking` L43 call this on different `this`
  pointers (particles?) — does the "vehicle" naming hold for those?
- No guard for zero-length or identity matrix; callers are assumed to initialise `0x34B0..0x34D0`.
