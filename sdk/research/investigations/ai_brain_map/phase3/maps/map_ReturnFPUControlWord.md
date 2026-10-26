# map_ReturnFPUControlWord - `ReturnFPUControlWord` (GUESS name)

## Identity

- Address: `0x00591E66`; file `functions/ReturnFPUControlWord_00591e66.c`; 4 lines.
- Signature: `int __fastcall sub_591E66(__int16 a1)`.
- Callers (read-only corpus cache): `ConditionalMathWithDAT005e9ad0Flag`, `FloatParsingWithErrorHandling`, `MathDispatchWithErrorFallback`, `sub_5895A2`, `sub_596175`.
- Callees (from `offsets_ReturnFPUControlWord.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       return a1;   [L3]
```

## Meaning (GUESS)

- GUESS Identity function returning the FPU control-word argument (compiler/CRT helper).
- GUESS AI relevance: none -- CRT.

## Questions

- None for this triage map; name and meaning stay GUESS.
