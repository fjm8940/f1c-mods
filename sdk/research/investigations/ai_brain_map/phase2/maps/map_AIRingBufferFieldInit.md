# map_AIRingBufferFieldInit — declared `sub_424C80`

## Identity

- Address: `0x424C80`; file `functions/AIRingBufferFieldInit_00424c80.c`; 10 lines.
- Signature: `int __thiscall sub_424C80(float *this)`.
- Callers (grep `functions/` for `AIRingBufferFieldInit(`): `AIMainUpdate` (`0x424E70`) L59
  (`AIRingBufferFieldInit((float *)this);`) - manifest caller; `CameraViewportFrustumInitialization`
  (`0x424B20`) L57.
- Callees (from `offsets_AIRingBufferFieldInit.csv` CALL rows): none.
- `this` = vehicle (`float *`); write is `this[4370]` (0x4448). Globals: `dword_8AAE70` (ring index,
  post-incremented), `flt_8AAE74` (shared value table), `dword_7D01A0` (AI difficulty/strength, also used
  by `AIDifficultyScalar` and `AIBehaviorInitialization`).

## Inputs

None (no object offset read; only globals `dword_8AAE70`, `flt_8AAE74`, `dword_7D01A0`).

## Outputs

CSV access W rows.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4448 | 17480 | float | L8 | ? time margin (`this[4370]`; `FIELD_GLOSSARY.md` 0x4448) |

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 4370 | L8 | `this` float index (0x4448) |
| 0.15000001 | L7, L8 | sample scale and base addend |
| 0.0099999998 | L8 | difficulty scale (1/100) |
| 0.1 | L8 | final difficulty multiplier (net 1/1000 per strength point) |

## Logic

```
result = dword_8AAE70 + 1                                              [L6]
v2 = flt_8AAE74[(unsigned __int8)dword_8AAE70++] * 0.15000001         [L7]
this[4370] = v2 + 0.15000001 - (double)dword_7D01A0 * 0.0099999998 * 0.1   [L8]
return result                                                          [L9]
```

## Meaning (GUESS)

- GUESS Per-tick update of the 0x4448 time margin: takes the next sample from the shared random/table ring
  `flt_8AAE74` (scaled by 0.15, offset by 0.15) and subtracts `difficulty * 0.001`, making the margin
  smaller for stronger AI [L6-L8].
- GUESS `dword_8AAE70` is a free-running index shared with `AICollision` (which also post-increments it);
  both use the `(unsigned __int8)` wrap as the table index [L7].
- GUESS The return value `dword_8AAE70 + 1` (pre-increment value) is the caller's ring position, e.g. for
  writing the same sample into a parallel ring buffer in `AIMainUpdate` L59 [L6,L9].

## Questions

- What is the dimension of `flt_8AAE74` and who fills it? `AICollision` and this function both consume
  from the same post-incremented index - is concurrent use on the same tick intended?
- `FIELD_GLOSSARY.md` calls 0x4448 a "time margin used by the correction path"; is the 0.15 sample range
  in seconds, and why does difficulty subtract only 0.001 per point?
- Why return the index rather than the written value? The caller in `AIMainUpdate` may mirror it into
  another ring buffer.
