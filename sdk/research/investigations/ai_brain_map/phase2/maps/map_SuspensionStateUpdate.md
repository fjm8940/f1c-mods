# map_SuspensionStateUpdate — `sub_4E9600` (`SuspensionStateUpdate`)

## Identity

- Address: `0x4E9600`; file `functions/SuspensionStateUpdate_004e9600.c`; 12 lines.
- Signature: `int __thiscall sub_4E9600(int this)`.
- Callers (grep `functions/` for `SuspensionStateUpdate(`): `AIRacingLinePathFollowing` (`0x4E9CA0`) L99:
  `SuspensionStateUpdate((int)this);` (runs right after `this[3381..3383] = node pos` and
  `EulerToRotationMatrix(this + 3384, this + 3372)` at L96-L98); `SceneObjectLoaderStateMachine` (`0x40F1C0`)
  L34 (`*(_DWORD *)(this + 32)`); `sub_4C6B60` L17 (`this + 3`).
- Callees (from `offsets_SuspensionStateUpdate.csv` CALL rows): `F_3x3MatrixTransform` L6;
  `FieldReadDelegate` L11.

## Inputs

`offsets_SuspensionStateUpdate.csv` contains only CALL rows; the six operands below are pointer arguments that
the extractor did not emit (listed in `unmatched_SuspensionStateUpdate.txt`). The function itself reads no field.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| (arg) `this + 1764` | — | dst pointer | L5, L7 | ? transform output base (byte 1764) |
| (arg) `this + 13524` | — | float * | L8 | ? position (FIELD_GLOSSARY 13524 = position X) |
| (arg) `this + 13536` | — | _DWORD * | L9 | ? orientation (FIELD_GLOSSARY 13536) |
| (arg) `this + 13488` | — | const void * | L10 | ? rotation matrix (FIELD_GLOSSARY 13488) |

## Outputs

None written directly by this function. `F_3x3MatrixTransform` writes the transform at `this + 1764` (argument,
not a CSV `W` row); the function returns `FieldReadDelegate`'s result.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/SuspensionStateUpdate_004e9600.c`.

| value | lines | note |
|---|---|---|
| 1764 | L5, L7 | transform destination base (0x6E4) |
| 13524 | L8 | argument offset (0x34D4) |
| 13536 | L9 | argument offset (0x34E0) |
| 13488 | L10 | argument offset (0x34B0) |
| 0.0, 1.0 | — | none in source |

## Logic

```
v1 = this + 1764                                                              [L5]
F_3x3MatrixTransform(this + 0x6E4, this + 0x34D4, this + 0x34E0, this + 0x34B0)   [L6,L7,L8,L9,L10]
return FieldReadDelegate(v1)                                                  [L11]
```

## Meaning (GUESS)

- GUESS Refreshes a 3x3 transform at `this+0x6E4` from the rotation matrix at `this+0x34B0`, the vector at
  `this+0x34D4` and the orientation dword at `this+0x34E0`, then calls a field accessor/delegate on it
  [L5-L11].
- GUESS `AIRacingLinePathFollowing` calls it immediately after copying the path node position into
  `this[3381..3383]` and rebuilding the rotation matrix at `this+3372`, i.e. it recomputes per-frame
  vehicle/wheel geometry for the new pose [caller L96-L101].
- GUESS The name "SuspensionStateUpdate" comes from TARGETS.md; no suspension force math appears in the body,
  only a transform build plus a delegate read.

## Questions

- The argument offsets 1764/13524/13536/13488 were not extracted as CSV rows (unmatched file lists them as
  argument expressions); confirm they are real struct fields on `this`.
- What does `FieldReadDelegate` read (function table? field accessor)? Is the return used by callers?
- Base ambiguity: `sub_4C6B60` L17 passes `this + 3` while `AIRacingLinePathFollowing` L99 passes its raw
  `this`; on which object do offsets 0x6E4/0x34B0/0x34D4/0x34E0 live?
