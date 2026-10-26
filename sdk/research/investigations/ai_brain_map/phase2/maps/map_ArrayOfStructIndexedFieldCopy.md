# map_ArrayOfStructIndexedFieldCopy — declared `sub_403F70`

## Identity

- Address: `0x403F70`; file `functions/ArrayOfStructIndexedFieldCopy_00403f70.c`; 17 lines.
- Signature: `_DWORD *__thiscall sub_403F70(_DWORD *this, int a2, _DWORD *a3, _DWORD *a4)`.
- Callers (grep `functions/` for `ArrayOfStructIndexedFieldCopy(`): `AIMainUpdate` (`0x424E70`) L262
  (`ArrayOfStructIndexedFieldCopy(dword_5EA2C0, *(_DWORD *)(*(_DWORD *)this + 18596), v39, v40);`) -
  manifest caller; also `AIDriverStructInit` L119, `ConditionalDelegateDispatch` L5, `sub_4C6F10`
  L62/L96, `VehicleProximityCollisionDetection` L33. All callers pass the global `dword_5EA2C0` as `this`.
- Callees (from `offsets_ArrayOfStructIndexedFieldCopy.csv` CALL rows): none.
- `this` = global table object `dword_5EA2C0`; `this[85]` (0x154) = record-array base; record stride 28
  bytes; `a2` = record index; `a3`/`a4` = output 3-dword vectors.

## Inputs

CSV access R rows, deduplicated.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x154 | 340 | _DWORD | L7 | record-array base pointer (`this[85]`) |
| 0x4 | 4 | _DWORD | L9 | record[0] vector component 2 (`v4[1]`) |
| 0x8 | 8 | _DWORD | L10 | record[0] vector component 3 (`v4[2]`) |
| 0x4 | 4 | _DWORD | L14 | record[1] vector component 2 (`v5[1]`, base `+12`) |
| 0x8 | 8 | _DWORD | L15 | record[1] vector component 3 (`v5[2]`, base `+12`) |

Record[0] component 1 (`*v4` at L8) and record[1] component 1 (`*v5` at L12) have no CSV rows.

## Outputs

CSV access W rows.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L9 | `a3[1]` (output vector 1 component 2) |
| 0x8 | 8 | _DWORD | L10 | `a3[2]` (output vector 1 component 3) |
| 0x4 | 4 | _DWORD | L14 | `a4[1]` (output vector 2 component 2) |
| 0x8 | 8 | _DWORD | L15 | `a4[2]` (output vector 2 component 3) |

First components `*a3` (L8) and `*a4` (L12) have no CSV rows (see Questions).

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 28 | L7, L11 | record stride in bytes |
| 85 | L7, L11 | `this` dword index (0x154) holding the array base |
| 340 | L7, L11 | byte offset 0x154 (`85 * 4`) |
| 12 | L11 | second vector offset inside the 28-byte record |

## Logic

```
v4 = (_DWORD *)(28 * a2 + this[85])          [L7]
*a3 = *v4; a3[1] = v4[1]; a3[2] = v4[2]      [L8,L9,L10]
v5 = (_DWORD *)(28 * a2 + this[85] + 12)     [L11]
*a4 = *v5; a4[1] = v5[1]; a4[2] = v5[2]      [L12,L13,L14,L15]
return a4                                    [L12,L16]
```

## Meaning (GUESS)

- GUESS Copies two 3-dword vectors out of record `a2` of a 28-byte-stride array at `this[85]`: vector 1
  from record offset 0 into `a3`, vector 2 from record offset 12 into `a4` [L7-L15].
- GUESS The 28-byte record layout is two 12-byte vectors plus 4 bytes of padding or a fourth scalar; the
  caller passes `*(_DWORD *)(*(_DWORD *)this + 18596)` as the index, i.e. records are selected by a
  vehicle field at +0x48A4 (probably a node/segment id) [callers].
- GUESS `dword_5EA2C0` is a shared racing-line/path point table; this is a getter split into forward and
  tangent vectors used by collision and proximity scans [callers: AIMainUpdate, VehicleProximityCollisionDetection].
- GUESS Returns `a4` unchanged (the function's `result` is just the out pointer) [L12,L16].

## Questions

- The extractor has no rows for `*a3 = *v4` (L8) and `*a4 = *v5` (L12); effective component-1 offsets are
  0 on both `a3`/`a4` and on the record base. `unmatched_ArrayOfStructIndexedFieldCopy.txt` lists no lines
  at all - the misses are zero-offset forms.
- What is field `+0x48A4` (18596) on the vehicle used as `a2` by AIMainUpdate/AIDriverStructInit - a path
  node index?
- What are the two vectors semantically (position + tangent/normal?), and are 28 bytes really
  two vec3 + 4, or a vec3 + a scalar + a vec3?
- Is `dword_5EA2C0` the same table used by `SpatialTreeNearestNeighborSearch`/BVH functions?
