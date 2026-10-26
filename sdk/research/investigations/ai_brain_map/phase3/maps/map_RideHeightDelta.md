# map_RideHeightDelta - `RideHeightDelta` (GUESS name)

## Identity

- Address: `0x004EF680`; file `functions/RideHeightDelta_004ef680.c`; 11 lines.
- Signature: `int __thiscall sub_4EF680(float *this, int a2)`.
- Callers (read-only corpus cache): `AIVehiclePathPositionTracking`, `CoordinateTransformWithOffset`, `F_3DTransformWithMatrixMultiplication`, `RayTriangleIntersectionWithBarycentricCoordinate`.
- Callees (from `offsets_RideHeightDelta.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x74C | 1868 | float | L6 | `*(float *)(a2 + 8) = this[467];` |
| 0x3688 | 13960 | float | L9 | `*(float *)(a2 + 4) = (this[3490] - this[3522] + this[3428] - this[3460` |
| 0x3708 | 14088 | float | L9 | `*(float *)(a2 + 4) = (this[3490] - this[3522] + this[3428] - this[3460` |
| 0x3590 | 13712 | float | L9 | `*(float *)(a2 + 4) = (this[3490] - this[3522] + this[3428] - this[3460` |
| 0x3610 | 13840 | float | L9 | `*(float *)(a2 + 4) = (this[3490] - this[3522] + this[3428] - this[3460` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x8 | 8 | float | L6 | `*(float *)(a2 + 8) = this[467];` |
| 0x4 | 4 | _DWORD | L7 | `*(_DWORD *)(a2 + 4) = 0;` |
| 0x4 | 4 | float | L9 | `*(float *)(a2 + 4) = (this[3490] - this[3522] + this[3428] - this[3460` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       *(float *)(a2 + 8) = this[467];   [L6]
       *(_DWORD *)(a2 + 4) = 0;   [L7]
       *(_DWORD *)a2 = 0;   [L8]
       *(float *)(a2 + 4) = (this[3490] - this[3522] + this[3428] - this[3460]) * 0.5;   [L9]
```

## Meaning (GUESS)

- GUESS Computes a ride-height delta from four suspension float pairs into the destination; sets y/z to 0.
- GUESS AI relevance: none-low -- physics setup; called by AIVehiclePathPositionTracking among others.

## Questions

- None for this triage map; name and meaning stay GUESS.
