# map_UtilityFunction_1d2e - `UtilityFunction_1d2e` (GUESS name)

## Identity

- Address: `0x00591D2E`; file `functions/UtilityFunction_1d2e_00591d2e.c`; 17 lines.
- Signature: `int __cdecl sub_591D2E(int a1, int a2)`.
- Callers (read-only corpus cache): `FloatClassify`, `sub_5895A2`.
- Callees (from `offsets_UtilityFunction_1d2e.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( a2 == 2146435072 )   [L3]
         if ( !a1 )   [L5]
           return 1;   [L6]
       else if ( a2 == -1048576 && !a1 )   [L8]
         return 2;   [L10]
```

## Meaning (GUESS)

- GUESS Classifies a double as zero/Inf/NaN (returns 0..4) for the CRT math helpers.
- GUESS AI relevance: none -- CRT math helper.

## Questions

- None for this triage map; name and meaning stay GUESS.
