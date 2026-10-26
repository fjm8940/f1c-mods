# map_ReplayAIStateCheck — declared `sub_4F1C20`

## Identity

- Address: `0x4F1C20`; file `functions/ReplayAIStateCheck_004f1c20.c`; 7 lines.
- Signature: `BOOL __thiscall sub_4F1C20(_DWORD *this)`.
- Callers (grep `functions/` for `ReplayAIStateCheck(`): `Overtaking` (`0x42AF70`) L11
  (`ReplayAIStateCheck(this + 3)`).
- Callees (from `offsets_ReplayAIStateCheck.csv` CALL rows): none.
- `this` = object passed by Overtaking; all reads are `this[N]` (stride 4) or `v1 + N` byte offsets.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x3A9C | 15004 | _DWORD | L5 (`v1 = this[3751]`) | cached path node pointer (FIELD_GLOSSARY) |
| 0x12C | 300 | _DWORD | L6 (`v1 + 300`) | ? node state/mode field (checked == 1) |

## Outputs

None.

## Constants

| value | lines | note |
|---|---|---|
| 3751 | L5 | `this[N]` index (byte 0x3A9C) |
| 300 | L6 | node field byte offset (0x12C) |

## Logic

```
v1 = this[3751]                                     [L5]
return v1 && *(_DWORD *)(v1 + 0x12C) == 1           [L6]
```

## Meaning (GUESS)

- GUESS Returns true only when the cached path node pointer at `this[3751]` (byte 0x3A9C) is non-null and
  that node's +0x12C state field equals 1 [L5,L6].
- GUESS FIELD_GLOSSARY names byte 0x3A9C the cached racing-line/path node pointer, so this is "has a path
  node and it is in state 1" [FIELD_GLOSSARY; L5].
- GUESS The +0x12C field is the same node state/id field used by the Spatial* node structures
  (0x12C == node id compared in `SpatialNearestNeighborQueryWithPlaneTest`), so state 1 may mean
  "racing line node" vs other node types [L6].
- GUESS The phase-2 name "ReplayAIStateCheck" is not evidenced by this code; Overtaking uses the result to
  gate an interaction branch [caller L11].

## Questions

- Overtaking calls `ReplayAIStateCheck(this + 3)` while the callee indexes `this[3751]` — are the caller's
  object and the vehicle/global object layout aligned at +12 bytes, or is the caller `this` something else?
- What is node state 1 at +0x12C (racing line? active waypoint?) and what are other values?
- Does anything besides `AIRacingLinePathFollowing` write 0x3A9C (FIELD_GLOSSARY lists it as writer)?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
