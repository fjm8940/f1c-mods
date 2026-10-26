# map_FieldZeroSetter — `sub_425660` (`FieldZeroSetter`)

## Identity

- Address: `0x425660`; file `functions/FieldZeroSetter_00425660.c`; 4 lines.
- Signature: `void __thiscall sub_425660(_DWORD *this)`.
- Callers (grep `functions/` for `FieldZeroSetter(`): `AIMainUpdate` (`0x424E70`) L272; the
  `AIMainUpdate_00424e70.md` inventory lists it as `| 0x00425EF0 | FieldZeroSetter | Zero field and return |`
  (the `.md` address is stale; the `.c` call at L272 is authoritative).
- Callees (from `offsets_FieldZeroSetter.csv` CALL rows): none.

## Inputs

None. The function writes a constant; no field is read.

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4510 | 17680 | _DWORD | L3 | driverStatus / mode = 0 (`this[4420]`; FIELD_GLOSSARY) |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/FieldZeroSetter_00425660.c`.

| value | lines | note |
|---|---|---|
| 4420 | L3 | `this[N]` index; byte offset 0x4510 |

## Logic

NOTE: source uses scaled `this[N]` indexing; Logic keeps the source-literal index and no hex token. The
byte offset is in the table above.

```
this[4420] = 0      [L3]
```

## Meaning (GUESS)

- GUESS Zeroes the driver-status/mode field `0x4510`; `AIMainUpdate` uses this as the "racing" mode value
  (4/5 = pit wait, 6 = reverse, 7 = stop variants appear elsewhere) [L3, FIELD_GLOSSARY].
- GUESS The function exists as a named one-liner so the AIMainUpdate mode state machine can "zero field and
  return" through a call, matching the `.md` inventory description [caller, inventory].
- GUESS Called from `AIMainUpdate` L272, i.e. on one specific branch of the per-frame update; the map does
  not establish which branch [caller].

## Questions

- `AIMainUpdate` L272 context (which branch writes mode 0) is not recorded here; check the AIMainUpdate map.
- Why a separate function rather than an inline store? Possibly a compiler-merged epilogue or a function
  that originally contained more fields.
- The `AIMainUpdate_00424e70.md` inventory cites `0x00425EF0`, which is not this function's address
  (`0x425660`); stale address — do not use.
