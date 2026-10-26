# map_StateTransition - `StateTransition` (GUESS name)

## Identity

- Address: `0x004A56B0`; file `functions/StateTransition_004a56b0.c`; 24 lines.
- Signature: `int __thiscall sub_4A56B0(_DWORD *this, char a2)`.
- Callers (read-only corpus cache): `ActorMotionPostProcessingUpdate`, `CollisionSparkParticleEffects`, `PitPartStateCopy`, `PitStopDecisionTireCompound`, `RepairServiceStateDispatcher`.
- Callees (from `offsets_StateTransition.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x128 | 296 | _DWORD | L9 | `if ( this[74] != 2 )` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x128 | 296 | _DWORD | L11 | `this[74] = 2;` |
| 0x12C | 300 | _DWORD | L13 | `this[75] = 1;` |
| 0x130 | 304 | _DWORD | L14 | `this[76] = 1;` |
| 0x128 | 296 | _DWORD | L19 | `this[74] = 1;` |
| 0x12C | 300 | _DWORD | L20 | `this[75] = 0;` |
| 0x130 | 304 | _DWORD | L21 | `this[76] = 0;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( a2 )   [L6]
         result = 2;   [L8]
           this[74] = 2;   [L11]
           result = 1;   [L12]
           this[75] = 1;   [L13]
           this[76] = 1;   [L14]
         this[74] = 1;   [L19]
         this[75] = 0;   [L20]
         this[76] = 0;   [L21]
```

## Meaning (GUESS)

- GUESS Sets the three state words this[74..76] to transition values (2/1/1 or 1/0/0) based on a2.
- GUESS AI relevance: low -- called by pit/repair/collision update functions.

## Questions

- None for this triage map; name and meaning stay GUESS.
