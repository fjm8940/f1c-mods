# map_StateConditionalAction - `StateConditionalAction` (GUESS name)

## Identity

- Address: `0x004A5E40`; file `functions/StateConditionalAction_004a5e40.c`; 9 lines.
- Signature: `int __thiscall sub_4A5E40(_DWORD *this)`.
- Callers (read-only corpus cache): `ForcePitExit`.
- Callees (from `offsets_StateConditionalAction.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x184 | 388 | _DWORD | L5 | `result = this[97];` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       result = this[97];   [L5]
       if ( !result || result == 4 )   [L6]
         return StateChangeNotify((int)this, 1);   [L7]
```

## Meaning (GUESS)

- GUESS If the state at this[97] is 0 or 4, forwards StateChangeNotify(this, 1).
- GUESS AI relevance: low-medium -- state-machine helper called by ForcePitExit.

## Questions

- None for this triage map; name and meaning stay GUESS.
