# map_AIDelegateCall - `AIDelegateCall` (GUESS name)

## Identity

- Address: `0x0042ADD0`; file `functions/AIDelegateCall_0042add0.c`; 9 lines.
- Signature: `int __thiscall sub_42ADD0(int this)`.
- Callers (read-only corpus cache): `FindNearestVehicle`, `sub_4A4110`.
- Callees (from `offsets_AIDelegateCall.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4354 | 17236 | _DWORD | L6 | `*(_DWORD *)(this + 17236),` |
| 0x4550 | 17744 | _DWORD | L7 | `*(_DWORD *)(this + 17744),` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       return SpatialTreeNearestNeighborSearch(   [L3]
                dword_5EA2C0,   [L4]
                (float *)(this + 13536),   [L5]
                *(_DWORD *)(this + 17236),   [L6]
                *(_DWORD *)(this + 17744),   [L7]
                0);   [L8]
```

## Meaning (GUESS)

- GUESS Thin AI wrapper around the spatial-tree nearest-neighbour query: SpatialTreeNearestNeighborSearch(tree, vehicle position at +13536 = 0x34E0, *(this+17236), *(this+17744), 0).
- GUESS AI relevance: medium -- same query family the waypoint locator uses; called by FindNearestVehicle and sub_4A4110.
- The corpus name AIDelegateCall is a naming-pass guess; the function only forwards to the spatial tree.

## Questions

- None for this triage map; name and meaning stay GUESS.
