# map_SuspensionVibration - `SuspensionVibration` (GUESS name)

## Identity

- Address: `0x004471F0`; file `functions/SuspensionVibration_004471f0.c`; 23 lines.
- Signature: `void __thiscall sub_4471F0(int this, char a2)`.
- Callers (read-only corpus cache): `Gearbox`, `VehicleClimbAdjust`, `sub_42C440`, `sub_447780`.
- Callees (from `offsets_SuspensionVibration.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x1C5C | 7260 | double | L7 | `v2 = *(double *)(this + 7260) < *(double *)(this + 7300);` |
| 0x1C84 | 7300 | double | L7 | `v2 = *(double *)(this + 7260) < *(double *)(this + 7300);` |
| 0x1C88 | 7304 | _DWORD | L13 | `v3 = *(_DWORD *)(this + 7304);` |
| 0x1C84 | 7300 | _DWORD | L14 | `*(_DWORD *)(this + 7260) = *(_DWORD *)(this + 7300);` |
| 0x1D20 | 7456 | double | L21 | `*(double *)(this + 7440) = v4 * *(double *)(this + 7456) + *(double *)` |
| 0x1D28 | 7464 | double | L21 | `*(double *)(this + 7440) = v4 * *(double *)(this + 7456) + *(double *)` |
| 0x1D18 | 7448 | double | L21 | `*(double *)(this + 7440) = v4 * *(double *)(this + 7456) + *(double *)` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x1C59 | 7257 | _BYTE | L8 | `*(_BYTE *)(this + 7257) = 1;` |
| 0x1C5C | 7260 | _DWORD | L14 | `*(_DWORD *)(this + 7260) = *(_DWORD *)(this + 7300);` |
| 0x1C60 | 7264 | _DWORD | L15 | `*(_DWORD *)(this + 7264) = v3;` |
| 0x1D10 | 7440 | double | L21 | `*(double *)(this + 7440) = v4 * *(double *)(this + 7456) + *(double *)` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       v2 = *(double *)(this + 7260) < *(double *)(this + 7300);   [L7]
       *(_BYTE *)(this + 7257) = 1;   [L8]
         if ( v2 )   [L11]
           v3 = *(_DWORD *)(this + 7304);   [L13]
           *(_DWORD *)(this + 7260) = *(_DWORD *)(this + 7300);   [L14]
           *(_DWORD *)(this + 7264) = v3;   [L15]
         v4 = flt_8AB274[(unsigned __int8)dword_8AAE70++];   [L20]
         *(double *)(this + 7440) = v4 * *(double *)(this + 7456) + *(double *)(this + 7464) + *(double *)(this + 7448);   [L21]
```

## Meaning (GUESS)

- GUESS Suspension vibration state update: latches a time pair and draws from the flt_8AB274 table to set a vibration accumulator.
- GUESS AI relevance: none-low -- physics/audio jitter.

## Questions

- None for this triage map; name and meaning stay GUESS.
