# map_F_3DTransformWithMatrixMultiplication — `sub_4EFB20` (`F_3DTransformWithMatrixMultiplication`)

## Identity

- Address: `0x4EFB20`; file `functions/F_3DTransformWithMatrixMultiplication_004efb20.c`; 26 lines.
- Signature: `int __thiscall sub_4EFB20(float *this, float *a2, _DWORD *a3)`.
- Callers (grep `functions/` for `F_3DTransformWithMatrixMultiplication(`): `AIRacingLinePathFollowing`
  (`0x4E9CA0`) L34; `AIRacingLine` (`0x425920`) L73, L118; `AISteeringRacingLineCorrection` (`0x42B0E0`)
  L149; `DistanceComparison` (`0x4EFFD0`) L10; `PitStopReadyCheck` (`0x4A6200`) L38; `SessionStateTransition`
  (`0x4EF5A0`) L26; `sub_4BC9B0` (`0x4BC9B0`) L38; `sub_4F0D20` (`0x4F0D20`) L15;
  `VehicleMainPhysicsUpdate` (`0x4441F0`) L839; `VehicleProximityCollisionDetection` (`0x4A5FA0`) L35;
  `VehicleYawRotationRecoveryController` (`0x4F1C40`) L60, L95.
- Callees (from `offsets_F_3DTransformWithMatrixMultiplication.csv` CALL rows): `RideHeightDelta` L11.
- Decompiler artifact: `v3` is read (L12) without any visible assignment; presumably `RideHeightDelta`
  leaves the `this` pointer in `v3` (ecx). See Questions.

## Inputs

Base `v3` (assumed `this`).

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x34B8 | 13496 | float | L12 | rotation matrix element 3 |
| 0x34B4 | 13492 | float | L13 | rotation matrix element 2 |
| 0x4 | 4 | _DWORD | L22 | orientation Y |
| 0x8 | 8 | _DWORD | L24 | orientation Z |

All other reads in this function use `v3 - N` back-offsets after the pointer
bump at L14 and are absent from `offsets_F_3DTransformWithMatrixMultiplication.csv`;
they are listed in the vehicle-relative view section below (verified against the
cited source lines, not invented).

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4 | 4 | float | L16, L19 | `a2[1]` transformed point Y |
| 0x8 | 8 | float | L17, L20 | `a2[2]` transformed point Z |
| 0x4 | 4 | _DWORD | L23 | `a3[1]` orientation Y |
| 0x8 | 8 | _DWORD | L24 | `a3[2]` orientation Z |

`*a2` (L15) and `*a3` (L21, copied from `*(_DWORD *)v3`) are written but are absent from the CSV
(zero offsets).

## Vehicle-relative view (derived from the `v3 += 13536` bump; NOT in offsets CSV)

`v3` is bumped to `this + 13536` (0x34E0) at L14; the `v3 - N` reads then touch
vehicle 0x34B0..0x34DC. This section is outside the CSV-backed tables above.

| veh hex | veh dec | source form | type | lines | name |
|---|---|---|---|---|---|
| 0x34B0 | 13488 | v3 - 48 | float | L15 | rotation matrix element 1 |
| 0x34BC | 13500 | v3 - 36 | float | L16 | rotation matrix element 4 |
| 0x34C0 | 13504 | v3 - 32 | float | L16 | rotation matrix element 5 |
| 0x34C4 | 13508 | v3 - 28 | float | L16 | rotation matrix element 6 |
| 0x34C8 | 13512 | v3 - 24 | float | L17 | rotation matrix element 7 |
| 0x34CC | 13516 | v3 - 20 | float | L17 | rotation matrix element 8 |
| 0x34D0 | 13520 | v3 - 16 | float | L17 | rotation matrix element 9 |
| 0x34D4 | 13524 | v3 - 12 | float | L18 | translation X |
| 0x34D8 | 13528 | v3 - 8 | float | L19 | translation Y |
| 0x34DC | 13532 | v3 - 4 | float | L20 | translation Z |

## Constants

Grouped by value; every numeric literal except 0/1 in
`functions/F_3DTransformWithMatrixMultiplication_004efb20.c`.

| value | lines | note |
|---|---|---|
| 13496, 13492 | L12, L13 | matrix byte offsets before pointer bump |
| 13536 | L14 | pointer bump to orientation block (veh 0x34E0) |
| 48, 36, 32, 28, 24, 20, 16, 12, 8, 4 | L15-L24 | back-offsets from bumped `v3` |
| 0x4, 0x8 | L22, L24 | `v3` orientation dword offsets |

## Logic

NOTE: hex offsets below are cited on source lines that contain their decimal value.

```
RideHeightDelta(this, (int)&v7)                                             [L11]
v4 = v9 * *(float *)(v3 + 0x34B8)                                           [L12]
v5 = v8 * *(float *)(v3 + 0x34B4)                                           [L13]
v3 += 0x34E0                                                                [L14]
*a2 = v4 + v5 + v7 * *(float *)(v3 - 0x30)                                  [L15]
a2[1] = v9 * *(float *)(v3 - 0x1C) + v7 * *(float *)(v3 - 0x24) + v8 * *(float *)(v3 - 0x20)   [L16]
a2[2] = v9 * *(float *)(v3 - 0x10) + v7 * *(float *)(v3 - 0x18) + v8 * *(float *)(v3 - 0x14)   [L17]
*a2 = *a2 + *(float *)(v3 - 0xC)                                            [L18]
a2[1] = *(float *)(v3 - 0x8) + a2[1]                                        [L19]
a2[2] = *(float *)(v3 - 0x4) + a2[2]                                        [L20]
*a3 = *(_DWORD *)v3                                                         [L21]
result = *(_DWORD *)(v3 + 0x4); a3[1] = result                              [L22,L23]
a3[2] = *(_DWORD *)(v3 + 0x8)                                               [L24]
return result                                                               [L25]
```

## Meaning (GUESS)

- GUESS Transforms the point `(v7, v8, v9)` (filled by `RideHeightDelta` at L11) by the 3x3 rotation
  matrix at `0x34B0..0x34D0`, adds translation `0x34D4..0x34DC`, and writes the world point to `a2`
  [L12-L20].
- GUESS Then copies three dwords from `0x34E0..0x34E8` (Euler orientation X/Y/Z per `FIELD_GLOSSARY`)
  into `a3`, so the caller gets pose origin plus orientation in one call [L21-L24].
- GUESS The return value is the orientation Y dword (`*(v3+4)`), used by callers as a chained accessor
  result [L22,L23,L25].
- GUESS `RideHeightDelta` supplies a local point (ride-height-adjusted point), so this is "transform a
  local vehicle point to world space" [L11-L15].
- GUESS `F_3x3MatrixTransformWithTranslation` (`0x4E9BD0`) is the specialised sibling that transforms
  `0x3B54..0x3B5C` into `0x3B60..0x3B68` without the `a3` copy [cross-map].
- GUESS `PitStopReadyCheck` L38 calls with only two arguments (`this`, `&v7`), so `a3` may be null in some
  paths — the write at L21-L24 would then be caller-side garbage; verify [caller].

## Questions

- Where does `v3` get its value? The Hex-Rays output has no assignment; assume `RideHeightDelta` returns
  `this` in ecx, but this must be confirmed in the disassembly.
- `PitStopReadyCheck` L38 passes only 2 args; is `a3` an optional out-parameter (and can it be null)?
- `RideHeightDelta` is an unmapped callee — what does it compute into `v7/v8/v9` (local point? ride height
  delta?) and does it clobber ecx/`v3`?
- Field block `0x34B0..0x34D0` is assumed to be a row-major rotation matrix; confirm against
  `EulerToRotationMatrix` output layout and `FIELD_GLOSSARY` (which does not name 0x34B0..0x34D0).
- CSV gap: the negative `v3 - N` reads (L15-L20) are missing from
  `offsets_F_3DTransformWithMatrixMultiplication.csv` while the unmatched file is empty; the extractor
  apparently skips `base - N` forms. Confirm the extractor rule and whether other maps share this gap.
