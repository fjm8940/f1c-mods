# map_ConditionalMathWithDAT005e9ad0Flag - `ConditionalMathWithDAT005e9ad0Flag` (GUESS name)

## Identity

- Address: `0x0059168F`; file `functions/ConditionalMathWithDAT005e9ad0Flag_0059168f.c`; 34 lines.
- Signature: `double __cdecl sub_59168F(DWORD dwExceptionCode, int a2, double a3, double a4, int a5)`.
- Callers (read-only corpus cache): `sub_5895A2`.
- Callees (from `offsets_ConditionalMathWithDAT005e9ad0Flag.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( !sub_5919DA(dwExceptionCode, &a4, a5) )   [L8]
         sub_591727((ULONG_PTR)Arguments, (int)&a5, dwExceptionCode, a2, (int)&a3, (int)&a4);   [L11]
       v5 = BitFlagDecoderViaBitTests(dwExceptionCode);   [L13]
       if ( dword_5E9AD0 || !v5 )   [L14]
         SetErrorCodeBasedOnInputRange(v5);   [L16]
         result = a3;   [L22]
         FloatParsingWithErrorHandling(   [L23]
```

## Meaning (GUESS)

- GUESS FPU-exception math dispatch helper used by sub_5895A2 (_frnd): handles the exception code and returns the operand depending on the global flag dword_5E9AD0.
- GUESS AI relevance: none -- CRT/FPU helper inside the rounding primitive.

## Questions

- None for this triage map; name and meaning stay GUESS.
