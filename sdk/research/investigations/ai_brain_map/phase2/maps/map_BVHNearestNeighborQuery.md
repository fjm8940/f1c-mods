# map_BVHNearestNeighborQuery — declared `sub_4039C0`

## Identity

- Address: `0x4039C0`; file `functions/BVHNearestNeighborQuery_004039c0.c`; 54 lines.
- Signature: `int __stdcall sub_4039C0(float *a1, int a2)` (`__stdcall` - no `this`).
- Callers (grep `functions/` for `BVHNearestNeighborQuery(`): `AISteering` (`0x4270A0`) L323
  (`v15 = BVHNearestNeighborQuery((float *)(*(_DWORD *)v14 + 13536), *((_DWORD *)this + 4309));`) -
  manifest caller; only occurrence.
- Callees (from `offsets_BVHNearestNeighborQuery.csv` CALL rows): none.
- `a1` = query position (3 floats; query is the opponent vehicle Euler? caller passes `vehicle + 13536`);
  `a2`/`result` = node pointer whose fields +148/152/156 are node positions, +272 a linked child, +60 a
  per-hop float.

## Inputs

CSV access R rows, deduplicated.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x94 | 148 | float | L24 | node position X |
| 0x98 | 152 | float | L25 | node position Y |
| 0x9C | 156 | float | L26 | node position Z |
| 0x110 | 272 | _DWORD | L28 | child/link pointer (null ends the walk) |
| 0x3C | 60 | float | L32 | accumulated cost per hop |
| 0x4 | 4 | float | L25 | `a1[1]` (query Y) |
| 0x8 | 8 | float | L26 | `a1[2]` (query Z) |

`*a1` (query X, used at L24/L41) has no CSV row. The same +148/152/156/+272 fields are re-read through
`v10` at L39-L41 and L46-L47.

## Outputs

None. The function only reads and returns a node pointer.

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 148, 152, 156 | L24-L26, L39-L41 | node position offsets |
| 272 | L28, L36, L46, L47 | child pointer offset |
| 60 | L32 | per-hop cost added to `v16` |
| 200.0 | L34 | travel-cost budget: stop when `v16 > 200.0` |

## Logic

```
result = a2                                                 [L18]
if (a2 == 0) return 0                                       [L19,L20,L21,L22]
v6 = a1[0] - *(a2 + 148); v7 = a1[1] - *(a2 + 152); v8 = a1[2] - *(a2 + 156)   [L24,L25,L26]
v15 = v8*v8 + v7*v7 + v6*v6                                 [L27]
if (*(a2 + 272)) {                                          [L28]
  while (1) {                                               [L30]
    v9 = v16 + *(v4 + 60); v16 = v9                         [L32,L33]
    if (v9 > 200.0) break                                   [L34,L35]
    v10 = *(v4 + 272); if (!v10) break                      [L36,L37,L38]
    v11 = a1[1] - *(v10 + 152)                              [L39]
    v12 = a1[2] - *(v10 + 156)                              [L40]
    v13 = a1[0] - *(v10 + 148)                              [L41]
    v14 = v12*v12 + v11*v11 + v13*v13                       [L42]
    if (v14 >= v15) break                                   [L43,L44]
    v15 = v14; v4 = *(v4 + 272)                             [L45,L46]
    if (!*(v10 + 272)) return v10                           [L47,L48]
  }    [L49]
  return v4                                                 [L51]
}    [L52]
return result                                               [L53]
```

## Meaning (GUESS)

- GUESS Greedy nearest-neighbor descent along a singly linked node chain: computes the squared distance
  from query `a1` to the current node's position (+148/152/156) and repeatedly advances to the child at
  +272 while the child is strictly closer [L24-L48].
- GUESS A per-hop cost at node +60 is accumulated into `v16`; the walk aborts once the budget exceeds
  200.0, preventing unbounded traversal (name "BVH" per the manifest; structure is a chain, not a binary
  tree) [L32-L35].
- GUESS Returns the best node pointer found (or the input `a2` if it has no child); callers use the result
  as a spatial index (AISteering L323 passes the opponent vehicle's Euler block 0x34E0 and a per-opponent
  index) [L46-L53].
- GUESS Distance is carried in the low double `v15`; `v14 >= v15` stops on ties, so first-found wins on
  equal distance [L43,L44].

## Questions

- `a2` is tested only for null; the caller's second argument `*((_DWORD *)this + 4309)` (AISteering
  offset 17236 = 0x4354, current waypoint ptr per `FIELD_GLOSSARY.md`) is the start node. Is this linked
  list the racing-line node chain rather than a BVH?
- What is node field +60 (per-hop cost) and why is the cap exactly 200.0? Is it a distance in metres or a
  node-hop budget?
- `*a1` (query X) has no CSV row; confirm the query is `vehicle + 13536` (0x34E0 Euler orientation block)
  rather than a position vector - the caller passes the Euler base.
- The returned pointer may be `v4`, `v10`, or the input `a2` depending on the path; is a null return
  consumed safely by AISteering L323?
