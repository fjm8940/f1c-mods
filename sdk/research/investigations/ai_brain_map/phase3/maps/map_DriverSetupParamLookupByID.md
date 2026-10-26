# map_DriverSetupParamLookupByID - `DriverSetupParamLookupByID` (GUESS name)

## Identity

- Address: `0x0042AEB0`; file `functions/DriverSetupParamLookupByID_0042aeb0.c`; 13 lines.
- Signature: `int __thiscall sub_42AEB0(_DWORD *this, int a2)`.
- Callers (read-only corpus cache): `AICollision`.
- Callees (from `offsets_DriverSetupParamLookupByID.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4148 | 16712 | _DWORD | L12 | `return this[2 * v2 + 4178];` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       v2 = 0;   [L6]
       for ( i = this + 4177; *i != a2; i += 2 )   [L7]
         if ( ++v2 >= 5 )   [L9]
           return -1;   [L10]
       return this[2 * v2 + 4178];   [L12]
```

## Meaning (GUESS)

- GUESS Linear search over five (id, value) 2-dword pairs starting at this[4177]; returns the paired value or -1.
- GUESS AI relevance: low-medium -- driver setup parameter lookup, called by AICollision.

## Questions

- None for this triage map; name and meaning stay GUESS.
