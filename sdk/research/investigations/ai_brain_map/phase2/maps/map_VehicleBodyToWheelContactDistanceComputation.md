# map_VehicleBodyToWheelContactDistanceComputation — `sub_4E9A40`

## Identity

- Address: `0x4E9A40`; file `functions/VehicleBodyToWheelContactDistanceComputation_004e9a40.c`; 55 lines.
- Signature: `double __thiscall sub_4E9A40(float *this, float *a2)`.
- Callers (grep `functions/` for `VehicleBodyToWheelContactDistanceComputation(`):
  `AIRacingLinePathFollowing` (`0x4E9CA0`) L101 (`(this, 0)`); `ParticleEffectUpdateDispatch` (`0x4EB430`)
  L125 (`((float *)this, 0)`); `SwitchBasedStateHandler` (`0x40F380`) L40 (`(v7)`), L48 (`(0)`).
- Callees (from `offsets_VehicleBodyToWheelContactDistanceComputation.csv` CALL rows):
  `NetworkPacketDataWriter` L47; `sqrt` L51.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x3B68 | 15208 | float | L18 (`this[3802]`) | ? body point Z |
| 0x3B60 | 15200 | float | L19 (`this[3800]`) | ? body point X |
| 0x3B64 | 15204 | float | L20 (`this[3801]`) | ? body point Y |
| 0x4 | 4 | float | L24 (`a2[1]`) | optional delta Y (only when a2 != 0) |
| 0x8 | 8 | float | L25 (`a2[2]`) | optional delta Z |
| 0x34D4 | 13524 | float | L31 (`this[3381]`) | position X (FIELD_GLOSSARY) |
| 0x34D8 | 13528 | float | L32 (`this[3382]`) | position Y |
| 0x34DC | 13532 | float | L35 (`this[3383]`) | position Z |
| 0x34C8 | 13512 | float | L36 (`this[3378]`) | rotation matrix element |
| 0x34BC | 13500 | float | L36 (`this[3375]`) | rotation matrix element |
| 0x34B0 | 13488 | float | L36 (`this[3372]`) | rotation matrix element |
| 0x34CC | 13516 | float | L37 (`this[3379]`) | rotation matrix element |
| 0x34C0 | 13504 | float | L37 (`this[3376]`) | rotation matrix element |
| 0x34B4 | 13492 | float | L37 (`this[3373]`) | rotation matrix element |
| 0x34D0 | 13520 | float | L38 (`this[3380]`) | rotation matrix element |
| 0x34C4 | 13508 | float | L38 (`this[3377]`) | rotation matrix element |
| 0x34B8 | 13496 | float | L38 (`this[3374]`) | rotation matrix element |
| 0x72C | 1836 | float | L39, L44 (`this[459]`) | reference vector X |
| 0x730 | 1840 | float | L40, L45 (`this[460]`) | reference vector Y |
| 0x734 | 1844 | float | L41, L46 (`this[461]`) | reference vector Z |
| 0x3B6C | 15212 | float | L52 (`this[3803]`) | minimum-distance floor |

`v7` base is `this + 3759` (float index, unmatched L34); each loop step reads `v7[-1]`, `v7[0]`, `v7[1]` and
advances by 3 floats (13 iterations).

## Outputs

None in `this` (no CSV `W` rows). The function fills the 6-float stack buffer `v16` and passes it to
`NetworkPacketDataWriter(v16, v6++)` for each of the 13 iterations (L42-L50), and returns a `double`.

## Constants

Grouped by value; every numeric literal except 0/1 in
`functions/VehicleBodyToWheelContactDistanceComputation_004e9a40.c`.

| value | lines | note |
|---|---|---|
| 3802, 3800, 3801 | L18, L19, L20 | body point indices (0x3B68, 0x3B60, 0x3B64) |
| 0.025 | L23, L24, L25 | a2 scaling |
| 3759 | L34 | wheel/contact array base (float index) |
| 3381, 3382, 3383 | L31, L32, L35 | position indices (0x34D4-0x34DC) |
| 3378, 3375, 3372 | L36 | matrix column (0x34C8, 0x34BC, 0x34B0) |
| 3379, 3376, 3373 | L37 | matrix column (0x34CC, 0x34C0, 0x34B4) |
| 3380, 3377, 3374 | L38 | matrix column (0x34D0, 0x34C4, 0x34B8) |
| 459, 460, 461 | L39-L41, L44-L46 | reference vector (0x72C-0x734) |
| 13 | L50 | loop count |
| 3803 | L52, L53 | floor index (0x3B6C) |
| 0.0, 1.0 | — | none in source |

No integer bit-pattern is written into a float field.

## Logic

```
v14 = this[3802]; v10 = this[3800]; v12 = this[3801]                      [L18,L19,L20]
if (a2) { v3 = a2[0]*0.025 + v10; v12 = a2[1]*0.025 + v12; v14 = a2[2]*0.025 + v14 }   [L21,L22,L23,L24,L25]
else v3 = v10                                                             [L29]
v4 = v3 - this[3381]; v5 = v12 - this[3382]; v8 = v14 - this[3383]        [L31,L32,L35]
v11 = v8*this[3378] + v5*this[3375] + v4*this[3372]                       [L36]
v13 = v8*this[3379] + v5*this[3376] + v4*this[3373]                       [L37]
v15 = v8*this[3380] + v5*this[3377] + v4*this[3374]                       [L38]
v16[0] = v11 - this[459]; v16[1] = v13 - this[460]; v16[2] = v15 - this[461]   [L39,L40,L41]
v6 = 0; v7 = this + 3759                                                  [L33,L34]
do {
  v16[3] = *(v7-1) - this[459]; v16[4] = *v7 - this[460]; v16[5] = v7[1] - this[461]   [L44,L45,L46]
  NetworkPacketDataWriter(v16, v6++)                                      [L47,L48]
  v7 += 3                                                                 [L48]
} while (v6 < 13)                                                         [L50]
result = sqrt(v15*v15 + v13*v13 + v11*v11)                                [L51]
if (this[3803] > result) return this[3803]                                [L52,L53]
return result                                                             [L54]
```

## Meaning (GUESS)

- GUESS Computes the body-frame delta of a body point (`this[3800..3802]`, optionally offset by `a2 * 0.025`),
  rotates it with the 9-float matrix `this[3372..3380]`, subtracts reference vector `this[459..461]`, then emits
  13 contact samples from the array at `this+3759` floats (stride 3) minus the same reference [L18-L50].
- GUESS Returns the larger of the transformed body vector length and the floor `this[3803]` - a minimum
  separation/distance estimate over body and 13 contact points [L51-L54].
- GUESS The 13-entry array looks like wheel/suspension contact geometry (13 points) and the reference vector
  could be a track/ground anchor [L44-L46].
- GUESS `AIRacingLinePathFollowing` calls it per path checkpoint right after `SuspensionStateUpdate` (L99) and
  before `F_3x3MatrixTransformWithTranslation` (L102), so this is a per-pose geometry update [callers].

## Questions

- What are `a2`'s units and source? It is scaled by 0.025 and only read when non-null.
- What do the 13 samples represent (wheels, aero, suspension points)? `NetworkPacketDataWriter` is an odd name
  for a distance routine - does it serialize the contacts instead?
- `v7 = this + 3759` is unmatched in the CSV (float-indexed base at 0x3ABC); confirm the 13x3 layout.
- Why do two `SwitchBasedStateHandler` call sites show a wrong argument count (`(v7)` and `(0)`)?
