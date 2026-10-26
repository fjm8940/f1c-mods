# map_UtilityFunction_5750 - `UtilityFunction_5750` (GUESS name)

## Identity

- Address: `0x004A5750`; file `functions/UtilityFunction_5750_004a5750.c`; 25 lines.
- Signature: `int __thiscall sub_4A5750(_DWORD *this, int a2)`.
- Callers (read-only corpus cache): `ModeBasedDispatch`, `PitStopDecisionTireCompound`.
- Callees (from `offsets_UtilityFunction_5750.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x2C | 44 | _DWORD | L11 | `v4 = *(_DWORD *)(result + 44) + 1;` |
| 0x28 | 40 | _DWORD | L13 | `v5 = *(_DWORD *)(result + 40);` |
| 0x35 | 53 | _BYTE | L16 | `if ( *(_BYTE *)(result + 53) )` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x2C | 44 | _DWORD | L12 | `*(_DWORD *)(result + 44) = v4;` |
| 0x2C | 44 | _DWORD | L20 | `*(_DWORD *)(result + 44) = v6;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( a2 == 2 )   [L8]
       result = (int)&this[16 * a2];   [L10]
       v4 = *(_DWORD *)(result + 44) + 1;   [L11]
       *(_DWORD *)(result + 44) = v4;   [L12]
       if ( v4 >= v5 )   [L14]
         if ( *(_BYTE *)(result + 53) )   [L16]
         else   [L18]
           v6 = v5 - 1;   [L19]
         *(_DWORD *)(result + 44) = v6;   [L20]
       if ( !a2 )   [L22]
         return ConditionalDispatch_55e0(this);   [L23]
```

## Meaning (GUESS)

- GUESS Increments the current index in the 16-dword record selected by a2 with wrap; dispatches FindZeroResultIndex (a2 == 2) and ConditionalDispatch_55e0 (a2 == 0).
- GUESS AI relevance: low-medium -- pit/compound state machine mirror of IndexDecrementWithBoundsWrap.

## Questions

- None for this triage map; name and meaning stay GUESS.
