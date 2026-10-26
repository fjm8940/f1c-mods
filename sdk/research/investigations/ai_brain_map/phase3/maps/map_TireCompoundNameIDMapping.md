# map_TireCompoundNameIDMapping - `TireCompoundNameIDMapping` (GUESS name)

## Identity

- Address: `0x004A4830`; file `functions/TireCompoundNameIDMapping_004a4830.c`; 12 lines.
- Signature: `int __thiscall sub_4A4830(void *this)`.
- Callers (read-only corpus cache): `AIPushLap`, `GuardedCallWithCheck`, `RandomWeightedTierSelection`, `sub_429960`, `sub_4F18C0`.
- Callees (from `offsets_TireCompoundNameIDMapping.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( !sub_588410(this, aHardCompound) )   [L3]
       if ( !sub_588410(this, aSoftCompound) )   [L5]
       if ( !sub_588410(this, aIntermediateCo) )   [L7]
       if ( sub_588410(this, aWetCompound) )   [L9]
         return sub_588410(this, aMonsoonCompoun) != 0 ? 1 : 6;   [L10]
       return 5;   [L11]
```

## Meaning (GUESS)

- GUESS Maps a tyre compound string to its numeric id (hard 2, soft 3, intermediate 4, wet 5, monsoon 6, default 1).
- GUESS AI relevance: low-medium -- tyre compound mapping used by pit strategy callers.

## Questions

- None for this triage map; name and meaning stay GUESS.
