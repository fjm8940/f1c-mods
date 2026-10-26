# map_ArithmeticComparison_a140 - `ArithmeticComparison_a140` (GUESS name)

## Identity

- Address: `0x004BA140`; file `functions/ArithmeticComparison_a140_004ba140.c`; 8 lines.
- Signature: `int __thiscall sub_4BA140(_DWORD *this, char a2)`.
- Callers (read-only corpus cache): `AIBehaviorInitialization`.
- Callees (from `offsets_ArithmeticComparison_a140.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x8 | 8 | _DWORD | L5 | `result = this[2];` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0xF78 | 3960 | _BYTE | L6 | `*(_BYTE *)(result + 3960) = a2;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       result = this[2];   [L5]
       *(_BYTE *)(result + 3960) = a2;   [L6]
       return result;   [L7]
```

## Meaning (GUESS)

- GUESS Sets byte +3960 on the object referenced by this[2].
- GUESS AI relevance: low -- AI behavior init helper.

## Questions

- None for this triage map; name and meaning stay GUESS.
