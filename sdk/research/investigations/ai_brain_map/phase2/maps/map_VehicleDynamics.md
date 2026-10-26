# map_VehicleDynamics — `sub_423B10` (`VehicleDynamics`)

## Identity

- Address: `0x423B10`; file `functions/VehicleDynamics_00423b10.c`; 54 lines.
- Signature: `void __thiscall sub_423B10(float *this, float *a2, int a3, char a4)`.
- Callers (grep `functions/` for `VehicleDynamics(`): `AISteering` (`0x4270A0`) L340
  (`VehicleDynamics(v18, (float *)v15, 0, 1);`) and L341
  (`VehicleDynamics(v19, *(float **)&this[4 * *((_DWORD *)this + 4378) + 17236], 0, 0);`). `v18`/`v19` are
  uninitialised `ecx` register locals in the decompile (see Questions); the intended receiver is not recovered.
- `a3` is declared but never used in the body.
- Callees (from `offsets_VehicleDynamics.csv` CALL rows): `sqrt` L30, L32, L49, L51.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x34E0 | 13536 | float | L24 (`*(_DWORD *)this + 13536`) | ? (glossary: Euler orientation X) |
| 0x94 | 148 | float | L24 (`a2[37]`) | waypoint position X (doc 54 wp+148) |
| 0xA4 | 164 | float | L24 (`a2[41]`) | waypoint direction X (doc 54 wp+164) |
| 0x34E8 | 13544 | float | L25 (`*(_DWORD *)this + 13544`) | ? (glossary: Euler orientation Z) |
| 0x9C | 156 | float | L25 (`a2[39]`) | waypoint position Z (doc 54 wp+156) |
| 0xAC | 172 | float | L25 (`a2[43]`) | waypoint direction Z (doc 54 wp+172) |
| 0x34F8 | 13560 | float | L38 (`v4 + 13560`) | ? velocity X (FIELD_GLOSSARY) |
| 0x3500 | 13568 | float | L38 (`v4 + 13568`) | ? velocity Z |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x24 | 36 | float | L33 (`this[9] = v10`) | ? scratch result 1 (written again L52) |
| 0x30 | 48 | float | L36 (`this[12] = 0.0`), L38 | ? scratch result 2 (only written when a4) |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/VehicleDynamics_00423b10.c`.

| value | lines | note |
|---|---|---|
| 13536 | L24, L44 | dereferenced base offset (0x34E0) |
| 13544 | L25, L43 | dereferenced base offset (0x34E8) |
| 13560 | L38 | dereferenced base offset (0x34F8) |
| 13568 | L38 | dereferenced base offset (0x3500) |
| 37, 39, 41, 43 | L24-L27, L34, L38, L42-L46 | a2 waypoint indices (0x94, 0x9C, 0xA4, 0xAC) |
| 9, 12 | L33, L36, L38, L52 | `this` array indices (0x24, 0x30) |
| 0.0 | L35, L36 | zero denominator guard |
| 1.0 | — | none in source |

No integer bit-pattern is written into a float field (`this[9]`/`this[12]` receive doubles).

## Logic

```
if (a4) {
  v4 = *this                                                                    [L23]
  v5 = (*(v4 + 0x34E0) - a2[37])*a2[41] + (*(v4 + 0x34E8) - a2[39])*a2[43]      [L24,L25]
  v18 = a2[41]*v5; v21 = a2[43]*v5                                              [L26,L27]
  v9 = v21*v21 + v18*v18                                                        [L28]
  if (v7 | v8) v10 = -sqrt(v9) else v10 = sqrt(v9)                              [L29,L30,L31,L32]
  this[9] = v10                                                                 [L33]
  v11 = a2[43]*a2[43] + a2[41]*a2[41]                                           [L34]
  if (v11 == 0.0) this[12] = 0.0                                                [L35,L36]
  else this[12] = (*(v4 + 0x34F8)*a2[41] + *(v4 + 0x3500)*a2[43]) / v11         [L38]
} else {
  v19 = -a2[43]                                                                 [L42]
  v12 = (*(*this + 0x34E8) - a2[39])*a2[41] - (*(*this + 0x34E0) - a2[37])*a2[43]   [L43,L44]
  v20 = v19*v12; v22 = a2[41]*v12                                               [L45,L46]
  v16 = v22*v22 + v20*v20                                                       [L47]
  if (v14 | v15) v17 = sqrt(v16) else v17 = -sqrt(v16)                          [L48,L49,L50,L51]
  this[9] = v17                                                                 [L52]
}
```

## Meaning (GUESS)

- GUESS `this` is a scratch object whose `[0]` is a vehicle pointer; it projects the vehicle against a waypoint
  `a2` and stores two signed scalars in `this[9]` (0x24) and `this[12]` (0x30) [L23-L52].
- GUESS a4=1 branch: dot product of (vehicle component - waypoint position) with the waypoint direction, then
  squared magnitude of the direction-scaled vector - a projection/angle-like pair; this[12] divides the
  velocity dot direction by |direction|^2 (closing speed ratio) [L24-L38].
- GUESS a4=0 branch does the cross-product variant (2D cross of the same deltas), so the two branches are
  forward vs reverse/lateral evaluations; `AISteering` calls both, for the current waypoint (a4=1) and for the
  selected opponent/path waypoint (a4=0) [callers L340-L341].
- GUESS Waypoint fields: +0x94/+0x9C = position (x,z), +0xA4/+0xAC = direction (x,z), matching doc 54
  (`wp.pos(+148,+156)`, `wp.dir(+164,+172)`).
- GUESS If the glossary is right that 0x34E0/0x34E8 are Euler angles and 0x34F8/0x3500 velocity, the first
  branch mixes angle against position - see Questions; otherwise these are vehicle world position X/Z.

## Questions

- `this` receiver unresolved: `AISteering` L340/L341 passes `v18`/`v19`, which are decompiler register locals.
  What object has `[0]` = vehicle pointer and scratch slots at 0x24/0x30?
- Are `veh+0x34E0`/`veh+0x34E8` position or Euler angles? `AISteering` L309-L314 passes `veh+13536` to
  `SpatialTreeNearestNeighborSearch` as a point, while FIELD_GLOSSARY names 0x34E0/0x34E8 "Euler orientation".
- The sign conditions decompile as uninitialised flags (`v7 | v8`, `v14 | v15`); the real compare is lost.
- `a3` is unused - leftover parameter or a dropped branch?
