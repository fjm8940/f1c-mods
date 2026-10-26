# map_SpatialNearestNeighborQueryWithPlaneTest — declared `sub_4038E0`

## Identity

- Address: `0x4038E0`; file `functions/SpatialNearestNeighborQueryWithPlaneTest_004038e0.c`; 44 lines.
- Signature: `int __thiscall sub_4038E0(_DWORD *this, float *a2, int a3)`.
- Callers (grep `functions/` for `SpatialNearestNeighborQueryWithPlaneTest(`): `MemoryAllocation_004046b0`
  (`0x4046B0`) L49; `Steward_Init` (`0x4D38F0`) L34; `sub_426430` (`0x426430`) L78, L89;
  `SurfaceHeightfieldCollisionCallback` (`0x404250`) L67, L93; plus five internal calls from
  `SpatialTreeNearestNeighborSearch` (`0x403AA0`) L31, L62, L83, L95, L100.
- Callees (from `offsets_SpatialNearestNeighborQueryWithPlaneTest.csv` CALL rows): none.
- `this` = spatial container (`this[76]` count, `this[77]` array); nodes are 316 bytes; `a2` = query point
  (float[3]); `a3` = node id filter (-1 accepts any).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x130 | 304 | _DWORD | L13 (`v3 = this[76]`) | ? node count |
| 0x134 | 308 | _DWORD | L18 (`v5 = this[77]`) | ? node array base pointer |
| 0x4 | 4 | float | L22 (`a2[1]`) | query point Y |
| 0x8 | 8 | float | L23 (`a2[2]`) | query point Z |
| 0x94 | 148 | float | L24 (`v5 + 148`) | node position X |
| 0x98 | 152 | float | L22 (`v5 + 152`) | node position Y |
| 0x9C | 156 | float | L23 (`v5 + 156`) | node position Z |
| 0x12C | 300 | _DWORD | L26 (`v5 + 300`) | ? node id/type filter field |
| 0xAC | 172 | float | L36 (`v4 + 172`) | ? plane term B (multiplied by Z delta) |
| 0xA4 | 164 | float | L36 (`v4 + 164`) | ? plane term A (multiplied by X delta) |
| 0x110 | 272 | _DWORD | L38 (`v4 + 272`) | ? linked node / replacement pointer |

Query X is read as `*a2` (no offset row in the CSV; extractor matched only `a2[1]`/`a2[2]`).

## Outputs

None (returns a node pointer or the linked pointer).

## Constants

| value | lines | note |
|---|---|---|
| 9.9999999e36 | L15 | initial best squared distance (near FLT_MAX) |
| 316 | L31 | node stride in bytes (`v5 += 316`) |
| 76, 77 | L13, L18 | `this[N]` indices (bytes 0x130 / 0x134) |
| 148, 152, 156 | L22-L24 | node position X/Y/Z offsets |
| 300 | L26 | node id/type offset |
| 164, 172 | L36 | plane terms |
| 272 | L38 | linked pointer offset |
| -1 | L26 | `a3` sentinel: accept any node id |

## Logic

```
v3 = this[76]                                                              [L13]
v4 = 0                                                                     [L14]
v12 = 9.9999999e36                                                         [L15]
if (v3 <= 0) return v4                                                     [L16,L17]
v5 = this[77]                                                              [L18]
v6 = v3                                                                    [L19]
do {
    v7 = a2[1] - *(float *)(v5 + 0x98)                                     [L22]
    v8 = a2[2] - *(float *)(v5 + 0x9C)                                     [L23]
    v9 = *a2 - *(float *)(v5 + 0x94)                                       [L24]
    v10 = v8 * v8 + v7 * v7 + v9 * v9                                      [L25]
    if (v10 < v12 && (a3 == -1 || a3 == *(_DWORD *)(v5 + 0x12C))) {        [L26]
        v12 = v10; v4 = v5                                                 [L28,L29]
    }
    v5 += 316                                                              [L31]
    --v6                                                                   [L32]
} while (v6)                                                               [L34]
if (v4                                                                    [L35]
    && *(float *)(v4 + 0xAC) * (*(float *)(v4 + 0x9C) - a2[2])            [L36]
       + *(float *)(v4 + 0xA4) * (*(float *)(v4 + 0x94) - *a2) < 0.0)      [L36]
    return *(_DWORD *)(v4 + 0x110)                                         [L38]
else
    return v4                                                              [L42]
```

## Meaning (GUESS)

- GUESS Linear nearest-neighbour scan over `this[76]` nodes (stride 316 bytes) starting at `this[77]`,
  using squared XZ/Y distance to the query point `a2`; the best node wins [L13-L34].
- GUESS `a3` filters by node field +0x12C (byte 300): -1 accepts any node, otherwise the field must equal
  `a3` [L26].
- GUESS After the scan, if the winner's plane test — `term(+0xAC) * (z_node - z_query) +
  term(+0xA4) * (x_node - x_query)` — is negative, it returns the node's linked pointer at +0x110 instead
  of the node itself [L35-L38].
- GUESS This is the leaf/brute-force fallback of the spatial tree: `SpatialTreeNearestNeighborSearch`
  calls it when chains end or no entry node exists [SpatialTreeNearestNeighborSearch L31,L62,L83,L95,L100].
- GUESS doc 54 / FIELD_GLOSSARY context: used by the waypoint locator `sub_426430` and by Steward and
  surface-heightfield callbacks, i.e. generic 3D point queries [callers].
- GUESS Returning the +0x110 pointer when the plane test is negative flips the result to the node on the
  other side of a splitting plane [L36,L38].

## Questions

- What is the node struct (stride 316 = 0x13C)? Position at +148/+152/+156, id at +300, linked pointers at
  +268/+272/+276, plane terms at +164/+172.
- Why is `a2[0]` read as `*a2` but not extracted to an offset row (unmatched list is empty)? Is query X at
  byte 0?
- What do the two plane terms at +164/+172 represent (normal components in X/Z? direction vector?).
- Initial sentinel 9.9999999e36 vs FLT_MAX: is the comparison exact enough on all compilers?
- Is `this[76]` really a count and `this[77]` the array (adjacent fields, often `this[76] + ...`)?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
