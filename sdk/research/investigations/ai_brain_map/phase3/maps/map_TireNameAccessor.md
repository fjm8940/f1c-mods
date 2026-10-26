# map_TireNameAccessor - `TireNameAccessor` (GUESS name)

## Identity

- Address: `0x004F1630`; file `functions/TireNameAccessor_004f1630.c`; 9 lines.
- Signature: `char *__thiscall sub_4F1630(int this)`.
- Callers (read-only corpus cache): `AIPushLap`, `sub_429960`.
- Callees (from `offsets_TireNameAccessor.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x396C | 14700 | _DWORD * | L5 | `result = *(char **)CompoundIndexLookup(*(_DWORD **)(this + 14700), *(_` |
| 0x3970 | 14704 | _DWORD | L5 | `result = *(char **)CompoundIndexLookup(*(_DWORD **)(this + 14700), *(_` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       result = *(char **)CompoundIndexLookup(*(_DWORD **)(this + 14700), *(_DWORD *)(this + 14704));   [L5]
       if ( !result )   [L6]
         return byte_5EA438;   [L7]
       return result;   [L8]
```

## Meaning (GUESS)

- GUESS Returns the compound name string for (this+14700, this+14704) via CompoundIndexLookup, or a static empty string.
- GUESS AI relevance: low-medium -- tyre naming used by AIPushLap and sub_429960.

## Questions

- None for this triage map; name and meaning stay GUESS.
