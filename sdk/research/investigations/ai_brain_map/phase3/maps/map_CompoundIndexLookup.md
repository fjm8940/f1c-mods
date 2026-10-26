# map_CompoundIndexLookup - `CompoundIndexLookup` (GUESS name)

## Identity

- Address: `0x004D7570`; file `functions/CompoundIndexLookup_004d7570.c`; 8 lines.
- Signature: `int __thiscall sub_4D7570(_DWORD *this, int a2)`.
- Callers (read-only corpus cache): `BrakeTemperatureFadeModel`, `NthValidPartLookup`, `PitPartStateCopy`, `PitStopOperationTimer`, `RandomWeightedTierSelection`, `SuspensionAndWheelPhysics`, `TireCompoundPerWheelPhysicsApplication`, `TireNameAccessor`, `TireSuspensionForceComputation`, `VehiclePhysicsInitializer`, `VehicleSetupValueFormatter`, `sub_430750`, `sub_4F18C0`.
- Callees (from `offsets_CompoundIndexLookup.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x10 | 16 | _DWORD | L3 | `if ( a2 >= 0 && a2 < this[4] )` |
| 0x14 | 20 | _DWORD | L4 | `return this[5] + 680 * a2;` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( a2 >= 0 && a2 < this[4] )   [L3]
         return this[5] + 680 * a2;   [L4]
       if ( (unsigned __int8)StringSplitLoop(1, aDDevF119992002_4, 954) )   [L5]
       return 0;   [L7]
```

## Meaning (GUESS)

- GUESS Bounds-checked lookup into the 680-byte compound-entry table (this[5] + 680*index); logs a message on out-of-range.
- GUESS AI relevance: low-medium -- tyre compound data consumed by pit logic.

## Questions

- None for this triage map; name and meaning stay GUESS.
