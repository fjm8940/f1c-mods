# map_SentinelSlotFill - `SentinelSlotFill` (GUESS name)

## Identity

- Address: `0x004BD760`; file `functions/SentinelSlotFill_004bd760.c`; 14 lines.
- Signature: `int __thiscall sub_4BD760(_DWORD *this)`.
- Callers (read-only corpus cache): `PostPitCleanup`.
- Callees (from `offsets_SentinelSlotFill.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x8 | 8 | _DWORD | L12 | `this[result + 56] = this[2];` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0xE0 | 224 | _DWORD | L12 | `this[result + 56] = this[2];` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       for ( i = this + 56; *i != -1; ++i )   [L7]
       {   [L8]
           return result;   [L10]
       }   [L11]
       this[result + 56] = this[2];   [L12]
```

## Meaning (GUESS)

- GUESS Finds the first free slot (sentinel -1) in the 10-slot array at this+56 and fills it with this[2]; returns the count.
- GUESS AI relevance: low -- called by PostPitCleanup.

## Questions

- None for this triage map; name and meaning stay GUESS.
