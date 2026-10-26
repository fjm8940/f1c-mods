# map_CoordinateTransformWithOffset — declared `sub_4EF6C0`

## Identity

- Address: `0x4EF6C0`; file `functions/CoordinateTransformWithOffset_004ef6c0.c`; 37 lines.
- Signature: `int __thiscall sub_4EF6C0(float *this, float *a2, int a3, float *a4)`.
- Callers (grep `functions/` for `CoordinateTransformWithOffset(`): `AIRacingLinePathFollowing`
  (`0x4E9CA0`) L58 (`CoordinateTransformWithOffset(&v30, v32, &v28);`) - manifest caller; also
  `VehicleMainPhysicsUpdate` (`0x4441F0`) L861 (`CoordinateTransformWithOffset(v329, v335, v325);`) and
  L1310 (`CoordinateTransformWithOffset(&v300, v308, &v285);`).
- Callees (from `offsets_CoordinateTransformWithOffset.csv` CALL rows): `EulerToRotationMatrix` (L17),
  `RideHeightDelta` (L18), `LODWORD` (L31, pseudo).
- `this` = vehicle (position components at `this[3381..3383]` = 0x34D4/0x34D8/0x34DC); `a2` = reference
  position; `a3` = Euler input passed to `EulerToRotationMatrix`; `a4` = output delta vector.

## Inputs

CSV access R rows, deduplicated.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x34D4 | 13524 | float | L22, L25 | position X (`this[3381]`; `FIELD_GLOSSARY.md` 0x34D4) |
| 0x34D8 | 13528 | float | L23, L26 | position Y (`this[3382]`) |
| 0x34DC | 13532 | float | L24, L27 | position Z (`this[3383]`) |
| 0x4 | 4 | float | L26 | `a2[1]` (reference Y) |
| 0x8 | 8 | float | L27 | `a2[2]` (reference Z) |

`a2[0]` (`*a2` at L25) has no CSV row. `a3` is consumed by `EulerToRotationMatrix` without an offset row.

## Outputs

CSV access W rows.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4 | 4 | float | L33 | `a4[1]` (delta Y) |
| 0x8 | 8 | float | L35 | `a4[2]` (delta Z) |

`*a4` (delta X, L29) has no CSV row (see Questions).

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 3381, 3382, 3383 | L22-L27 | `this` float indices (position 0x34D4/0x34D8/0x34DC) |
| 4, 8 | L26, L27, L33, L35 | `a2`/`a4` vector component offsets |

## Logic

Scaled `this[idx]` forms keep the source index (no hex token); `a2`/`a4` forms keep decimal offsets.

```
EulerToRotationMatrix(a3, v17)                                        [L17]
RideHeightDelta(this, (int)&v13)                                      [L18]
v5      = v17[2]*v15 + v17[1]*v14 + v17[0]*v13                        [L19]
v6      = v17[5]*v15 + v17[4]*v14 + v17[3]*v13                        [L20]
v15     = v17[8]*v15 + v17[7]*v14 + v17[6]*v13                        [L21]
v13     = v5 + this[3381]                                             [L22]
v7      = v6 + this[3382]                                             [L23]
v8      = v15 + this[3383]                                            [L24]
v9      = this[3381] + *a2                                            [L25]
v10     = this[3382] + a2[1]                                          [L26]
v16     = this[3383] + a2[2]                                          [L27]
v13     = v9 - v13; *a4 = v13                                         [L28,L29]
v14     = v10 - v7; result = LODWORD(v14); a4[1] = v14                [L30,L31,L32,L33]
v15     = v16 - v8; a4[2] = v15                                       [L34,L35]
return result                                                         [L36]
```

## Meaning (GUESS)

- GUESS Transforms a local offset (from `RideHeightDelta`, components v13/v14/v15) by an Euler-derived
  rotation matrix `v17` built from `a3`, then adds the vehicle position to get an intermediate point
  `(v13, v7, v8)` [L17-L24].
- GUESS Computes the vector from that intermediate point to `vehicle position + a2`, i.e.
  `a4 = (pos + a2) - (R * offset + pos) = a2 - R * offset`, and returns the Y component as an int bit
  pattern [L25-L35].
- GUESS "Coordinate transform with offset": converts a reference point `a2` into vehicle-local space by
  subtracting the rotated offset, used by path-following and by `VehicleMainPhysicsUpdate` (suspension /
  contact positions) [callers].
- GUESS The `LODWORD(v14)` return is a Hex-Rays artifact (returns the float Y bits as int);
  the real ABI return is likely void or float [L31,L36].

## Questions

- Callers pass 3 arguments (`&v30, v32, &v28`; `v329, v335, v325`) for a 4-arg `__thiscall`; in
  `VehicleMainPhysicsUpdate` the arguments look like `(this, a2, a4)` with `a3` supplied elsewhere. Which
  argument is the Euler input `a3`? `AIRacingLinePathFollowing` passes `v32` as the second arg, which may be
  the Euler source, not `a2`.
- What is `a3`'s type: an int pointer to Euler angles or an offset? `EulerToRotationMatrix(a3, v17)` is
  called without a visible dereference.
- The extractor has no rows for `*a4 = v13` (L29) and `*a2` (L25); confirm the zero-offset writes.
- `v16` is only used for Z; is the third component naming (`v16` vs `v15`) a decompiler artifact?
