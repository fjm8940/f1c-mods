# map_AIVehiclePathPositionTracking — declared `sub_4EF7C0`

## Identity

- Address: `0x4EF7C0`; file `functions/AIVehiclePathPositionTracking_004ef7c0.c`; 56 lines.
- Signature: `_DWORD *__thiscall sub_4EF7C0(int this, float *a2, _DWORD *a3)`.
- Callers (grep `functions/` for `AIVehiclePathPositionTracking(`): `AISteeringRacingLineCorrection`
  (`0x42B0E0`) L255 (`AIVehiclePathPositionTracking(v42, v44);`) - manifest caller; also
  `LoopOverTwoParallelArrays` L32, `SceneRenderUpdate` L17, `SessionStateTransition` L27,
  `sub_4C6B60` L16, `sub_4F1FE0` L142, `VehicleYawRotationRecoveryController` L61/L96.
- Callees (from `offsets_AIVehiclePathPositionTracking.csv` CALL rows): `EulerToRotationMatrix` (L22),
  `RideHeightDelta` (L23), `F_3x3MatrixTransformWithTranslation` (L43), `RigidBodyConstraintSolve` (L53).
- `this` = vehicle; `a3` supplies Euler orientation XYZ copied to 0x34E0/0x34E4/0x34E8; `a2` supplies the
  reference XYZ used to update pose 0x34D4/0x34D8/0x34DC.

## Inputs

CSV access R rows, deduplicated. Matrix columns at 0x34B0..0x34D0 are read as the vehicle orientation.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x34B8 | 13496 | float | L24 | orientation matrix component |
| 0x34B4 | 13492 | float | L25 | orientation matrix component |
| 0x34B0 | 13488 | float | L26 | orientation matrix component |
| 0x34C4 | 13508 | float | L27 | orientation matrix component |
| 0x34C0 | 13504 | float | L28 | orientation matrix component |
| 0x34BC | 13500 | float | L29 | orientation matrix component |
| 0x34D0 | 13520 | float | L30 | orientation matrix component |
| 0x34CC | 13516 | float | L31 | orientation matrix component |
| 0x34C8 | 13512 | float | L32 | orientation matrix component |
| 0x34D4 | 13524 | float | L33 | position X (pose; `FIELD_GLOSSARY.md` 0x34D4) |
| 0x34D8 | 13528 | float | L34 | position Y (pose) |
| 0x34DC | 13532 | float | L35 | position Z (pose) |
| 0x4 | 4 | float | L37 | `a2[1]` (reference Y) |
| 0x8 | 8 | float | L38 | `a2[2]` (reference Z) |
| 0x34D4 | 13524 | float | L39, L47 | position X (pose; read again for the constraint path) |
| 0x34D8 | 13528 | float | L40, L48 | position Y (pose) |
| 0x34DC | 13532 | float | L41, L49 | position Z (pose) |
| 0x34E4 | 13540 | float | L51 | Euler orientation Y (`FIELD_GLOSSARY.md` 0x34E4) |
| 0x34E8 | 13544 | float | L52 | Euler orientation Z (`FIELD_GLOSSARY.md` 0x34E8) |
| 0x32BC | 12988 | _DWORD* | L44 | ? pointer (if null -> `RigidBodyConstraintSolve`) |

`a2[0]` (L36) and `a3[0]` (L19) have no CSV rows (see Questions).

## Outputs

CSV access W rows.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L20 | `v5[1]` = this+13540 (Euler orientation Y) |
| 0x8 | 8 | _DWORD | L21 | `v5[2]` = this+13544 (Euler orientation Z) |
| 0x34D4 | 13524 | float | L39 | position X (pose) updated by delta |
| 0x34D8 | 13528 | float | L40 | position Y (pose) updated by delta |
| 0x34DC | 13532 | float | L41 | position Z (pose) updated by delta |
| 0x3A9C | 15004 | _DWORD | L42 | cached path node pointer = 0 (`FIELD_GLOSSARY.md` 0x3A9C) |

The write `*v5 = *a3` (L19, effective this+13536, Euler X) has no CSV row (see Questions).

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 13536 | L17, L18 | pose/Euler base (`this + 13536`, 0x34E0) |
| 13488 | L22 | matrix destination for `EulerToRotationMatrix` (0x34B0) |
| 13524, 13528, 13532 | L33-L41, L47-L49 | position pose (0x34D4/0x34D8/0x34DC) |
| 13540, 13544 | L51, L52 | passive Euler Y/Z used by the constraint path |
| 12988 | L44 | pointer checked before `RigidBodyConstraintSolve` |
| 15004 | L42 | cached path node pointer reset (`0x3A9C`) |
| 0 | L42 | null pointer written to 0x3A9C |

## Logic

Direct `+ N` forms keep hex names (decimal present in the cited lines); vector components of local doubles
are written `.y`/`.z`.

```
v4 = (float *)(this + 0x34E0); v5 = (_DWORD *)(this + 0x34E0)          [L17,L18]
*v5 = *a3; v5[1] = a3[1]; v5[2] = a3[2]                                [L19,L20,L21]
EulerToRotationMatrix(v5, this + 0x34B0)                               [L22]
RideHeightDelta((float *)this, (int)&v13)                              [L23]
v6    = v14 * this[3374] + v13.y * this[3373] + v13.x * this[3372]     [L24,L25,L26]
v7    = v14 * this[3377] + v13.y * this[3376] + v13.x * this[3375]     [L27,L28,L29]
v14   = v14 * this[3380] + v13.y * this[3379] + v13.x * this[3378]     [L30,L31,L32]
v13.x = v6 + this[3381]                                                [L33]
v8    = v7 + this[3382]                                                [L34]
v9    = v14 + this[3383]                                               [L35]
v13.x = *a2 - v13.x; v13.y = a2[1] - v8; v10 = a2[2] - v9              [L36,L37,L38]
this[3381] = v13.x + this[3381]                                        [L39]
this[3382] = v13.y + this[3382]                                        [L40]
this[3383] = v10 + this[3383]                                          [L41]
*(_DWORD *)(this + 0x3A9C) = 0                                         [L42]
F_3x3MatrixTransformWithTranslation(v11)                               [L43]
result = *(_DWORD **)(this + 0x32BC)                                   [L44]
if (!result) {                                                         [L45]
  v16[0] = this[3381]; v16[1] = this[3382]; v16[2] = this[3383]        [L47,L48,L49]
  v13 = *v4; v14 = *(float *)(this + 0x34E4); v15 = *(float *)(this + 0x34E8)   [L50,L51,L52]
  return RigidBodyConstraintSolve((int)&dbl_7C7428, v16, (int)&v13)    [L53]
}    [L54]
return result                                                          [L55]
```

## Meaning (GUESS)

- GUESS Name says "vehicle path position tracking": it latches a path-node orientation (`a3`) into the
  vehicle Euler block 0x34E0..0x34E8, rebuilds the orientation matrix at 0x34B0, and computes a rotated
  local offset from `RideHeightDelta` [L17-L23].
- GUESS Pose correction: `delta = a2 - (R * local offset + pose)`, then `pose += delta` for
  0x34D4/0x34D8/0x34DC, i.e. it pulls the vehicle pose toward the reference position `a2` while keeping
  the rotated offset [L33-L41].
- GUESS Resets the cached racing-line node 0x3A9C to null (forcing a node re-find) and runs the
  suspension/contact matrix transform pipeline (`F_3x3MatrixTransformWithTranslation`) [L42-L43].
- GUESS If no constraint object exists at 0x32BC, it solves a rigid-body constraint against global
  `dbl_7C7428` using the updated pose and Euler Y/Z; otherwise it returns the existing object pointer
  [L44-L55].
- GUESS `a3` is the target path-node Euler frame and `a2` the current track/reference position; the
  update is a per-tick projection of the car onto the racing line (caller
  `AISteeringRacingLineCorrection` L255 runs next to `VehicleDynamicsStateReset`).

## Questions

- Caller `AISteeringRacingLineCorrection` L255 shows `AIVehiclePathPositionTracking(v42, v44)` (2 args)
  for a 3-arg `__thiscall`; which arg is `this`, and is `a3` omitted there?
- `F_3x3MatrixTransformWithTranslation(v11)` gets `v11` (`float *`, ecx) that is never assigned in this
  decompile - a Hex-Rays artifact; what is the real argument (this + 13488 or the matrix)?
- The writes `*v5 = *a3` (L19, Euler X 0x34E0) and the reads `a2[0]` (L36), `a3[1]`/`a3[2]` are partially
  absent from the CSV; `unmatched_AIVehiclePathPositionTracking.txt` lists only L17/L18/L22. Confirm the
  missing L19-L21 base offsets are all relative to 0x34E0.
- What do `RideHeightDelta`'s three outputs represent (v13.x/y/z double locals), and why is `v14` (the
  second component) the one written at L30-L32?
- Is 0x32BC a constraint-solver object pointer owned by the vehicle, and why is the function's return
  value that pointer (callers ignore it)?
- `dbl_7C7428` is a global constraint/config; what does it hold?
