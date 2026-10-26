# map_SpeedDeltaComputation - `SpeedDeltaComputation` (GUESS name)

## Identity

- Address: `0x00426CC0`; file `functions/SpeedDeltaComputation_00426cc0.c`; 13 lines.
- Signature: `double __thiscall sub_426CC0(_DWORD *this, int a2)`.
- Callers (read-only corpus cache): `FindNearestVehicle`, `sub_4A4110`.
- Callees (from `offsets_SpeedDeltaComputation.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4354 | 17236 | _DWORD | L9 | `v3 = this[4309];` |
| 0x4C | 76 | float | L11 | `return *(float *)(a2 + 76) - *(float *)(v3 + 76);` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       result = -1.0;   [L6]
       if ( !a2 )   [L7]
       v3 = this[4309];   [L9]
       if ( v3 )   [L10]
         return *(float *)(a2 + 76) - *(float *)(v3 + 76);   [L11]
```

## Meaning (GUESS)

- GUESS Returns (a2+76) - (this[4309]+76), or -1; this[4309] is the pointer field 0x4354 (current waypoint), so this compares a float at +0x4C of the current waypoint/segment object with the same field of a target object.
- GUESS AI relevance: medium -- called by FindNearestVehicle and sub_4A4110 (traffic-aware speed comparison).
- GUESS the +0x4C float is a speed-like quantity; both operands are objects of the same kind.

## Questions

- None for this triage map; name and meaning stay GUESS.
