# map_TeammatePartValueLookup - `TeammatePartValueLookup` (GUESS name)

## Identity

- Address: `0x004A5C40`; file `functions/TeammatePartValueLookup_004a5c40.c`; 22 lines.
- Signature: `int __thiscall sub_4A5C40(int *this)`.
- Callers (read-only corpus cache): `ForcePitExit`.
- Callees (from `offsets_TeammatePartValueLookup.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x48A4 | 18596 | _DWORD | L16 | `if ( *(_DWORD *)(v4 + 18596) == v2 && v4 != v1 )` |
| 0x3450 | 13392 | _DWORD | L21 | `return *(_DWORD *)(v4 + 13392);` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       v1 = *this;   [L8]
       v2 = *(_DWORD *)(*this + 18596);   [L9]
       if ( dword_5C329C <= 0 )   [L11]
         return 0;   [L12]
         v4 = dword_7C54F0[v3];   [L15]
         if ( *(_DWORD *)(v4 + 18596) == v2 && v4 != v1 )   [L16]
       return *(_DWORD *)(v4 + 13392);   [L21]
```

## Meaning (GUESS)

- GUESS Finds the other driver entry with the same pit team index (+18596 = 0x48A4) and returns its +13392 setup value.
- GUESS AI relevance: medium -- team/pit linking used by ForcePitExit.
- GUESS +13392 (0x3450) is a team setup/vehicle value; exact semantics unverified.

## Questions

- None for this triage map; name and meaning stay GUESS.
