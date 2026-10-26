# map_AnimationStateSetter - `AnimationStateSetter` (GUESS name)

## Identity

- Address: `0x004BE9E0`; file `functions/AnimationStateSetter_004be9e0.c`; 11 lines.
- Signature: `int __thiscall sub_4BE9E0(int this, char a2, char a3, int a4, int a5)`.
- Callers (read-only corpus cache): `AIBehaviorInitialization`.
- Callees (from `offsets_AnimationStateSetter.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x340 | 832 | _BYTE | L5 | `*(_BYTE *)(this + 832) = a2;` |
| 0x341 | 833 | _BYTE | L7 | `*(_BYTE *)(this + 833) = a3;` |
| 0x344 | 836 | _DWORD | L8 | `*(_DWORD *)(this + 836) = a4;` |
| 0x350 | 848 | _DWORD | L9 | `*(_DWORD *)(this + 848) = a5;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       *(_BYTE *)(this + 832) = a2;   [L5]
       result = a4;   [L6]
       *(_BYTE *)(this + 833) = a3;   [L7]
       *(_DWORD *)(this + 836) = a4;   [L8]
       *(_DWORD *)(this + 848) = a5;   [L9]
```

## Meaning (GUESS)

- GUESS Sets four animation/visual state fields on an actor (bytes +832/+833, dwords +836/+848).
- GUESS AI relevance: low -- called only by AIBehaviorInitialization.

## Questions

- None for this triage map; name and meaning stay GUESS.
