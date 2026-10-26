# map_IndexDecrementWithBoundsWrap - `IndexDecrementWithBoundsWrap` (GUESS name)

## Identity

- Address: `0x004A57A0`; file `functions/IndexDecrementWithBoundsWrap_004a57a0.c`; 23 lines.
- Signature: `int __thiscall sub_4A57A0(_DWORD *this, int a2)`.
- Callers (read-only corpus cache): `ModeBasedDispatch`, `PitStopDecisionTireCompound`.
- Callees (from `offsets_IndexDecrementWithBoundsWrap.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x2C | 44 | _DWORD | L10 | `v4 = *(_DWORD *)(result + 44) - 1;` |
| 0x35 | 53 | _BYTE | L14 | `if ( *(_BYTE *)(result + 53) )` |
| 0x28 | 40 | _DWORD | L15 | `v5 = *(_DWORD *)(result + 40) - 1;` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x2C | 44 | _DWORD | L11 | `*(_DWORD *)(result + 44) = v4;` |
| 0x2C | 44 | _DWORD | L18 | `*(_DWORD *)(result + 44) = v5;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( a2 == 2 )   [L7]
         FindZeroResultIndex(this);   [L8]
       v4 = *(_DWORD *)(result + 44) - 1;   [L10]
       *(_DWORD *)(result + 44) = v4;   [L11]
       if ( v4 < 0 )   [L12]
         if ( *(_BYTE *)(result + 53) )   [L14]
         else   [L16]
           v5 = 0;   [L17]
         *(_DWORD *)(result + 44) = v5;   [L18]
       if ( !a2 )   [L20]
```

## Meaning (GUESS)

- GUESS Decrements the current index in the 16-dword record selected by a2, wrapping per the record's wrap flag; dispatches FindZeroResultIndex (a2 == 2) and ConditionalDispatch_55e0 (a2 == 0).
- GUESS AI relevance: low-medium -- pit/compound state machine mirror of UtilityFunction_5750.

## Questions

- None for this triage map; name and meaning stay GUESS.
