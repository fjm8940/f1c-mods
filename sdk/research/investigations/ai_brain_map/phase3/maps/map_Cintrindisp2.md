# map_Cintrindisp2 - `Cintrindisp2` (GUESS name)

## Identity

- Address: `0x00590640`; file `functions/Cintrindisp2_00590640.c`; 7 lines.
- Signature: `int __usercall sub_590640@<eax>(int a1@<edx>, __int16 a2@<fpstat>, double a3@<st1>, double a4@<st0>)`.
- Callers (read-only corpus cache): `InterruptDispatch`.
- Callees (from `offsets_Cintrindisp2.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       sub_58DAD7(a1, (int)&savedregs, a2, a3, a4);   [L5]
       return sub_59070A();   [L6]
```

## Meaning (GUESS)

- GUESS CRT interrupt-dispatch thunk (sub_58DAD7 then sub_59070A), reached from InterruptDispatch.
- GUESS AI relevance: none -- CRT runtime.

## Questions

- None for this triage map; name and meaning stay GUESS.
