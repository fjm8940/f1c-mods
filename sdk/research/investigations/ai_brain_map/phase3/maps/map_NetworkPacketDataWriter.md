# map_NetworkPacketDataWriter - `NetworkPacketDataWriter` (GUESS name)

## Identity

- Address: `0x0054FA00`; file `functions/NetworkPacketDataWriter_0054fa00.c`; 21 lines.
- Signature: `int __thiscall sub_54FA00(_DWORD *this, _DWORD *a2, int a3)`.
- Callers (read-only corpus cache): `VehicleBodyToWheelContactDistanceComputation`, `VehicleOrientationMatrixConstruction`, `VehiclePhysicsVelocityAngularVelocityIntegrator`.
- Callees (from `offsets_NetworkPacketDataWriter.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x1E0 | 480 | _DWORD | L7 | `v3 = this[120];` |
| 0x8 | 8 | _DWORD | L8 | `v4 = *(_DWORD *)(v3 + 8);` |
| 0x4 | 4 | _DWORD | L9 | `if ( v4 && a3 < *(_DWORD *)(v3 + 4) )` |
| 0x4 | 4 | _DWORD | L13 | `v5[2] = a2[1];` |
| 0x8 | 8 | _DWORD | L14 | `v5[3] = a2[2];` |
| 0xC | 12 | _DWORD | L15 | `v5[7] = a2[3];` |
| 0x10 | 16 | _DWORD | L16 | `v5[8] = a2[4];` |
| 0x14 | 20 | _DWORD | L17 | `v5[9] = a2[5];` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L12 | `v5[1] = *a2;` |
| 0x8 | 8 | _DWORD | L13 | `v5[2] = a2[1];` |
| 0xC | 12 | _DWORD | L14 | `v5[3] = a2[2];` |
| 0x1C | 28 | _DWORD | L15 | `v5[7] = a2[3];` |
| 0x20 | 32 | _DWORD | L16 | `v5[8] = a2[4];` |
| 0x24 | 36 | _DWORD | L17 | `v5[9] = a2[5];` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( v4 && a3 < *(_DWORD *)(v3 + 4) )   [L9]
         v5 = (_DWORD *)(v4 + 116 * a3);   [L11]
         v5[1] = *a2;   [L12]
         v5[2] = a2[1];   [L13]
         v5[3] = a2[2];   [L14]
         v5[7] = a2[3];   [L15]
         v5[8] = a2[4];   [L16]
         v5[9] = a2[5];   [L17]
         *v5 |= 8u;   [L18]
```

## Meaning (GUESS)

- GUESS Writes six dwords (two 3-vectors) into the 116-byte network slot a3 and sets flag bit 3.
- GUESS AI relevance: none-low -- network replication of an object transform.

## Questions

- None for this triage map; name and meaning stay GUESS.
