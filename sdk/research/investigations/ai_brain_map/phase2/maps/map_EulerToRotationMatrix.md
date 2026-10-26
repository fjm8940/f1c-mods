# map_EulerToRotationMatrix — `sub_566E40` (`EulerToRotationMatrix`)

## Identity

- Address: `0x566E40`; file `functions/EulerToRotationMatrix_00566e40.c`; 29 lines.
- Signature: `void __fastcall sub_566E40(float *a1, float *a2)`.
- Callers (grep `functions/` for `EulerToRotationMatrix(`): `AIRacingLinePathFollowing` (`0x4E9CA0`) L91
  (`this + 3384`, `this + 3372`); `AIVehiclePathPositionTracking` (`0x4EF7C0`) L22 (`v5`, `this + 13488`);
  `AISteeringRacingLineCorrection` (`0x42B0E0`) L229; `AudioParameterInterpolation` (`0x4FF680`) L69;
  `CameraModeViewportUpdate` (`0x409A00`) L104; `Camera_004a3b50` (`0x4A3B50`) L128;
  `ConditionalAudioParamSetup` (`0x4DFA60`) L15, L27; `CoordinateTransformWithOffset` (`0x4EF6C0`) L17;
  `sub_4A3320` (`0x4A3320`) L10; `sub_4BC9B0` (`0x4BC9B0`) L34; `sub_4D0110` (`0x4D0110`) L252;
  `VehicleMainPhysicsUpdate` (`0x4441F0`) L843; `VehiclePhysicsTimeStep` (`0x4C73F0`) L49;
  `VehicleProximityCollisionDetection` (`0x4A5FA0`) L42, L45; `VehicleVisibilityCulling` (`0x4B3DC0`) L57;
  `VehicleYawRotationRecoveryController` (`0x4F1C40`) L59 (`this + 13536`, `this + 13488`).
- Callees (from `offsets_EulerToRotationMatrix.csv` CALL rows): `sin` L12, L14, L16; `cos` L13, L15, L17.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4 | 4 | float | L14 | ? (`a1[1]`; 2nd Euler angle) |
| 0x8 | 8 | float | L16 | ? (`a1[2]`; 3rd Euler angle) |

`*a1` (offset 0) is also read at L12 but is not represented in `offsets_EulerToRotationMatrix.csv`
(zero offset); the unmatched file is empty.

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4 | 4 | float | L21 | ? (`a2[1]`; matrix element 2) |
| 0x8 | 8 | float | L22 | ? (`a2[2]`; matrix element 3) |
| 0xC | 12 | float | L23 | ? (`a2[3]`; matrix element 4) |
| 0x10 | 16 | float | L24 | ? (`a2[4]`; matrix element 5) |
| 0x14 | 20 | float | L25 | ? (`a2[5]`; matrix element 6) |
| 0x18 | 24 | float | L26 | ? (`a2[6]`; matrix element 7) |
| 0x1C | 28 | float | L27 | ? (`a2[7]`; matrix element 8) |
| 0x20 | 32 | float | L28 | ? (`a2[8]`; matrix element 9) |

`*a2` (offset 0, matrix element 1) is also written at L20 but is not represented in
`offsets_EulerToRotationMatrix.csv` (zero offset).

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/EulerToRotationMatrix_00566e40.c`.

| value | lines | note |
|---|---|---|
| 1, 2 | L14, L15, L16, L17 | `a1[N]` index |
| 1..8 | L21-L28 | `a2[N]` index |
| -1 | L25 | negation of `sin(a1[0])` |

## Logic

NOTE: source uses scaled `a1[N]`/`a2[N]` indexing; Logic keeps the source-literal index and no hex tokens.
Byte offsets are in the tables above.

```
v2 = sin(*a1); v3 = cos(*a1)                            [L12,L13]
v4 = sin(a1[1]); v7 = cos(a1[1])                        [L14,L15]
v5 = sin(a1[2]); v6 = cos(a1[2])                        [L16,L17]
v9 = v6 * v7; v8 = v5 * v7                              [L18,L19]
*a2 = v4 * v2 * v5 + v9                                 [L20]
a2[1] = v4 * v2 * v6 - v8                               [L21]
a2[2] = v4 * v3                                         [L22]
a2[3] = v5 * v3                                         [L23]
a2[4] = v6 * v3                                         [L24]
a2[5] = -v2                                             [L25]
a2[6] = v8 * v2 - v6 * v4                               [L26]
a2[7] = v5 * v4 + v9 * v2                               [L27]
a2[8] = v7 * v3                                         [L28]
```

## Meaning (GUESS)

- GUESS Builds a 3x3 rotation matrix from three angles: `v2/v3` from `*a1`, `v4/v7` from `a1[1]`, `v5/v6`
  from `a1[2]` [L12-L17].
- GUESS `a2[5] = -sin(a1[0])` and the sparsity of `a2[2]`, `a2[3]`, `a2[4]`, `a2[8]` suggests an
  Euler convention where the first angle is applied last (or the matrix is stored column-major) [L22-L28].
- GUESS Callers copy the 9 floats into vehicle/pose blocks (`this + 13488` = rotation at `0x34B0`,
  `VehicleYawRotationRecoveryController` L59) and path-follow pose blocks (`this + 3372`) [callers].
- GUESS Interchangeable with the generic matrix at 0x34B0 used by `F_3x3MatrixTransformWithTranslation` and
  `F_3DTransformWithMatrixMultiplication`; this is the common "build rotation from Euler" helper [callers].
- GUESS Angles are radians (no degree conversion inside) [L12-L17].

## Questions

- `*a1` read (L12) and `*a2` write (L20) are missing from the CSV (zero offsets); confirm both.
- Which Euler order/convention (XYZ vs ZYX) and is the output row- or column-major? The sparse pattern
  (`a2[2]=v4*v3`, `a2[3]=v5*v3`, `a2[4]=v6*v3`, `a2[5]=-v2`) does not match a plain Rz*Ry*Rx product, so
  the order is non-standard and worth a dedicated test.
- `AIRacingLinePathFollowing` calls with `this + 3384` (input) and `this + 3372` (output): are inputs at
  `0x34E0..0x34E8` (Euler orientation) and outputs at `0x34B0..0x34D0` (rotation matrix)?
- `AIVehiclePathPositionTracking` L22 passes `this + 13488` as output but `v5` as input — where is that
  input filled from?
