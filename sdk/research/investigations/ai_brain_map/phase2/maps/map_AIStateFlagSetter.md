# map_AIStateFlagSetter — declared `sub_42AC70`

## Identity

- Address: `0x42AC70`; file `functions/AIStateFlagSetter_0042ac70.c`; 9 lines.
- Signature: `unsigned int __thiscall sub_42AC70(_DWORD *this, unsigned int a2)`.
- Callers (grep `functions/` for `AIStateFlagSetter(`): `AIMainUpdate` (`0x424E70`) L189
  (`AIStateFlagSetter(1);`) - manifest caller; `ForcePitExit` (`0x42A110`) L14 and L21
  (`return AIStateFlagSetter(1);`); `WrapPlayerCallback` (`0x42AC90`) L3
  (`return AIStateFlagSetter(this, dword_5EA2CC + this[1]);`).
- Callees (from `offsets_AIStateFlagSetter.csv` CALL rows): none.
- `this` = vehicle/AI object; `a2` = value latched only when 0 or 1.

## Inputs

No object offset read. Only the argument `a2` (compared with 1 at L6).

## Outputs

CSV access W rows.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x454C | 17740 | _DWORD | L7 | branch id latch (`this[4435]`; `FIELD_GLOSSARY.md` 0x454C) |

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 4435 | L7 | `this` dword index (0x454C) |

## Logic

```
result = a2                                       [L5]
if (a2 <= 1) this[4435] = a2                      [L6,L7]
return result                                     [L8]
```

## Meaning (GUESS)

- GUESS Guarded setter for the branch id latch 0x454C: only values 0 and 1 are stored; any larger value
  leaves the field unchanged but is still returned [L5-L8].
- GUESS ForcePitExit and AIMainUpdate use it to latch branch id 1 (pit release), while
  `WrapPlayerCallback` feeds `dword_5EA2CC + this[1]` - so non-0/1 player callback values are filtered out
  of 0x454C [callers].
- GUESS 0x454C is matched against waypoint fields and zeroed on pit release elsewhere
  (`FIELD_GLOSSARY.md` 0x454C; `map_AIAbility.md` L74), hence the 0/1 domain.

## Questions

- Callers `AIMainUpdate` L189 and `ForcePitExit` L14/L21 call `AIStateFlagSetter(1)` with one argument for
  a 2-arg `__thiscall`; which `this` is used there? Only `WrapPlayerCallback` L3 passes an explicit
  `this`.
- What does `dword_5EA2CC` hold, and why is `this[1]` added to it before the 0/1 clamp?
- Is 0x454C really only 0/1 at runtime, or does `AIRacingLine`/`sub_426430` write other values directly
  (bypassing this setter)?
