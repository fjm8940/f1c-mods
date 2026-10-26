# map_FloatFunctionPassThrough — `sub_5894A0` (`FloatFunctionPassThrough`)

## Identity

- Address: `0x5894A0`; file `functions/FloatFunctionPassThrough_005894a0.c`; 8 lines.
- Signature: `int __usercall sub_5894A0@<eax>(unsigned __int64 a1@<st0>)`.
- Callers (grep `functions/` for `FloatFunctionPassThrough(`): `CrossProductNormalize` (`0x56C540`) L18;
  `QuaternionNormalizeAndDecompose` (`0x50DAB2`) L31; `QuaternionSphericalInterpolation` (`0x53B5B0`) L32;
  `RotationVectorToQuaternion` (`0x50E055`) L16; `sub_426100` (`0x426100`) L34; `sub_4FAF60` (`0x4FAF60`)
  L93; `sub_567580` (`0x567580`) L41. All pass no visible arguments (x87 `st0` input).
- Callees (from `offsets_FloatFunctionPassThrough.csv` CALL rows): `FloatExponentExtractWithNaNGuard` L6
  (plus `HIDWORD` macro at L6); `sub_5894BD` L7.

## Inputs

None at the field level. The only input is the x87 register argument `a1` (`st0`, 64-bit) and its high
dword `HIDWORD(a1)` at L6.

## Outputs

None. The function returns an `int` (`eax`); the decompiler locals `v2`/`v3` are never assigned in the
listing (see Questions).

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/FloatFunctionPassThrough_005894a0.c`.

| value | lines | note |
|---|---|---|
| 0, 4 | L3, L4 | stack-frame local offsets only (`[esp+0h]`, `[esp+4h]`), not data offsets |

## Logic

```
FloatExponentExtractWithNaNGuard(a1, HIDWORD(a1))       [L6]
return sub_5894BD(v2, v3)                               [L7]
```

## Meaning (GUESS)

- GUESS An x87 real-number helper: takes a 64-bit value on `st0`, extracts/guards the exponent via
  `FloatExponentExtractWithNaNGuard`, then tail-calls the actual operation `sub_5894BD` [L6,L7].
- GUESS Callers are vector/quaternion normalisation routines (`CrossProductNormalize`,
  `QuaternionNormalizeAndDecompose`, `RotationVectorToQuaternion`); the helper likely implements a
  `sqrt`/`fabs`/float-conversion primitive of the compiler's CRT [callers].
- GUESS Because callers show no arguments, the operand is passed on the x87 stack; `v2`/`v3` are the
  call-saved stack slots that the first callee leaves for the second (decompiler artifact) [L6,L7].
- GUESS The two-stage structure (guard then worker) is a NaN/inf exponent guard around a standard
  floating operation [L6].

## Questions

- `v2`/`v3` are used at L7 without assignment in the Hex-Rays output; confirm in the disassembly that
  `FloatExponentExtractWithNaNGuard` writes those two stack slots.
- What are `FloatExponentExtractWithNaNGuard` and `sub_5894BD` exactly (addresses, semantics)? Neither has
  a phase-2 map; `sub_5894BD` is at `0x5894BD`, 0x1D bytes after this function.
- `HIDWORD` is a macro, not a function — the CSV CALL row for it is an extraction artifact.
- Which float operation does this implement (e.g. `_CIpow`, `_ftol`, NaN canonicalisation)? No constant or
  branch in this stub identifies it.
