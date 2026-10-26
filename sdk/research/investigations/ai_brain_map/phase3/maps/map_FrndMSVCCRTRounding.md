# map_FrndMSVCCRTRounding - `FrndMSVCCRTRounding` (GUESS name)

## Identity

- Address: `0x00591CF3`; file `functions/FrndMSVCCRTRounding_00591cf3.c`; 8 lines.
- Signature: `double __cdecl sub_591CF3(double a1)`.
- Callers (read-only corpus cache): `DoubleIntegerAndDivisibilityClassification`, `sub_5895A2`.
- Callees (from `offsets_FrndMSVCCRTRounding.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       _ST7 = a1;   [L5]
       __asm { frndint }   [L6]
```

## Meaning (GUESS)

- GUESS MSVC CRT _frnd primitive: FRNDINT wrapper used by sub_5895A2.
- GUESS AI relevance: none directly -- but this rounding step drives every AI table index (DROPPED_ST0_SCAN).

## Questions

- None for this triage map; name and meaning stay GUESS.
