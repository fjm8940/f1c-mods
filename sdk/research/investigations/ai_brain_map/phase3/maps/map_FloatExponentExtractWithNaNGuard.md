# map_FloatExponentExtractWithNaNGuard - `FloatExponentExtractWithNaNGuard` (GUESS name)

## Identity

- Address: `0x0058DD28`; file `functions/FloatExponentExtractWithNaNGuard_0058dd28.c`; 9 lines.
- Signature: `int __cdecl sub_58DD28(int a1, int a2)`.
- Callers (read-only corpus cache): `FloatConvertWrapper`, `FloatFunctionPassThrough`.
- Callees (from `offsets_FloatExponentExtractWithNaNGuard.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
         return a2;   [L7]
```

## Meaning (GUESS)

- GUESS Returns the double exponent bits (a2 & 0x7FF00000) unless the NaN/Inf pattern is present.
- GUESS AI relevance: none -- CRT math helper.

## Questions

- None for this triage map; name and meaning stay GUESS.
