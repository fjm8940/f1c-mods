# map_PositionChangeMonitor - `PositionChangeMonitor` (GUESS name)

## Identity

- Address: `0x004E9630`; file `functions/PositionChangeMonitor_004e9630.c`; 24 lines.
- Signature: `int __thiscall sub_4E9630(_DWORD *this)`.
- Callers (read-only corpus cache): `CollisionAvoidanceCheck`, `DistanceProjectionCalc`, `ParticleEffectUpdateDispatch`, `PitStateUpdateMachine`, `SessionStateTransition`, `sub_4423B0`, `sub_4D1D80`, `sub_4E96C0`, `sub_4EE2D0`, `sub_4F01B0`.
- Callees (from `offsets_PositionChangeMonitor.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x3A9C | 15004 | _DWORD | L12 | `if ( !(v4 \| v5) \|\| !this[3751] )` |
| 0x3A9C | 15004 | _DWORD | L14 | `v6 = sub_403640(dword_5EA2C0, (float *)this + 3381, this[3751]);` |
| 0x4 | 4 | int | L16 | `v8 = v2[1];` |
| 0x8 | 8 | int | L18 | `v9 = v2[2];` |
| 0x3A9C | 15004 | _DWORD | L23 | `return this[3751];` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x3A9C | 15004 | _DWORD | L17 | `this[3751] = v6;` |
| 0x3AA0 | 15008 | _DWORD | L19 | `this[3752] = v7;` |
| 0x3AA4 | 15012 | _DWORD | L20 | `this[3753] = v8;` |
| 0x3AA8 | 15016 | _DWORD | L21 | `this[3754] = v9;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       v2 = this + 3381;   [L11]
         v6 = sub_403640(dword_5EA2C0, (float *)this + 3381, this[3751]);   [L14]
         v7 = *v2;   [L15]
         this[3751] = v6;   [L17]
         v9 = v2[2];   [L18]
         this[3752] = v7;   [L19]
         this[3753] = v8;   [L20]
         this[3754] = v9;   [L21]
       return this[3751];   [L23]
```

## Meaning (GUESS)

- GUESS Caches the nearest/destination node for the placement pose: sub_403640(tree, this+3381, last node) and stores the last pose vector at this[3752..3754] (pose fields 0x34D4..0x34DC), returning the cached node pointer this[3751].
- GUESS AI relevance: medium -- placement-pose / track-node tracking shared with AI path position.

## Questions

- None for this triage map; name and meaning stay GUESS.
