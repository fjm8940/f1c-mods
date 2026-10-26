# map_ClampedIndexAssignment - `ClampedIndexAssignment` (GUESS name)

## Identity

- Address: `0x004A5BD0`; file `functions/ClampedIndexAssignment_004a5bd0.c`; 20 lines.
- Signature: `int __thiscall sub_4A5BD0(int this, float a2)`.
- Callers (read-only corpus cache): `PitStopDecisionTireCompound`.
- Callees (from `offsets_ClampedIndexAssignment.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x58 | 88 | double | L6 | `result = CRTFtol((a2 - *(double *)(this + 88)) / *(double *)(this + 96` |
| 0x60 | 96 | double | L6 | `result = CRTFtol((a2 - *(double *)(this + 88)) / *(double *)(this + 96` |
| 0x68 | 104 | _DWORD | L9 | `v4 = *(_DWORD *)(this + 104);` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x6C | 108 | _DWORD | L11 | `*(_DWORD *)(this + 108) = result;` |
| 0x6C | 108 | _DWORD | L13 | `*(_DWORD *)(this + 108) = v4 - 1;` |
| 0x6C | 108 | _DWORD | L17 | `*(_DWORD *)(this + 108) = 0;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       result = CRTFtol((a2 - *(double *)(this + 88)) / *(double *)(this + 96) + 0.5);   [L6]
       if ( result >= 0 )   [L7]
         v4 = *(_DWORD *)(this + 104);   [L9]
         if ( result < v4 )   [L10]
           *(_DWORD *)(this + 108) = result;   [L11]
           *(_DWORD *)(this + 108) = v4 - 1;   [L13]
       else   [L15]
         *(_DWORD *)(this + 108) = 0;   [L17]
```

## Meaning (GUESS)

- GUESS Float-to-index conversion (CRTFtol) with clamp into [0, count-1]; stores at +108; geometry base +88, step +96, count +104 on the callee's object.
- GUESS AI relevance: low-medium -- used by PitStopDecisionTireCompound (compound/lap index selection).

## Questions

- None for this triage map; name and meaning stay GUESS.
