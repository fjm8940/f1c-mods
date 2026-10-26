# map_RingBufferAccessWithWrap — declared `sub_4D1CF0`

## Identity

- Address: `0x4D1CF0`; file `functions/RingBufferAccessWithWrap_004d1cf0.c`; 18 lines.
- Signature: `int __thiscall sub_4D1CF0(int this, float a2)`.
- Callers (grep `functions/` for `RingBufferAccessWithWrap(`): `AIAbility` (`0x429470`) L28, L33 (twice);
  `ShortComputation_bad0` (`0x49BAD0`) L6.
- Callees (from `offsets_RingBufferAccessWithWrap.csv` CALL rows): `CRTFtol` (`0x587BDC`).
- `this` = ring-buffer owner object; accesses are direct `this + N` byte offsets.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x108 | 264 | float | L6 (`a2 * *(float *)(this + 264)`) | ? scale factor converting a2 to a slot count |
| 0x10C | 268 | _DWORD | L8 (`v3 += *(_DWORD *)(this + 268)`) | ? buffer length (slot count) |
| 0x110 | 272 | _DWORD | L13 (`*(_DWORD *)(this + 272) + 8 * v3`) | ? buffer base pointer (8-byte entries) |

## Outputs

None.

## Constants

| value | lines | note |
|---|---|---|
| 264, 268, 272 | L6-L17 | direct `this` byte offsets |
| 8 | L13, L16, L17 | ring-buffer entry stride (bytes) |

## Logic

```
v3 = CRTFtol(a2 * *(float *)(this + 0x108)) - 1                            [L6]
if (v3 < 0) v3 += *(_DWORD *)(this + 0x10C)                                [L7,L8]
v4 = *(_DWORD *)(this + 0x10C) - 1                                         [L9]
if (v3 < 0) { v3 = 0; return *(_DWORD *)(*(_DWORD *)(this + 0x110) + 8 * v3) }   [L10,L11,L12,L13]
if (v3 <= v4) return *(_DWORD *)(*(_DWORD *)(this + 0x110) + 8 * v3)        [L15,L16]
return *(_DWORD *)(*(_DWORD *)(this + 0x110) + 8 * v4)                      [L17]
```

## Meaning (GUESS)

- GUESS Converts a float `a2` (time or distance) to an integer slot with `CRTFtol(a2 * 0x108)`, subtracts
  1 and wraps a negative result by adding the length 0x10C [L6-L8].
- GUESS Then clamps the slot to `[0, length - 1]` and returns the first dword of the 8-byte entry at
  `base 0x110 + 8 * slot` [L9-L17].
- GUESS Because the index is shifted by one before wrapping, `a2` selects the previous sample (a history
  ring), which matches AIAbility's use of the result as a discrete state (0/1/2) [caller AIAbility
  L28/L33; L6].
- GUESS The entry layout is 8 bytes (2 dwords); only the first dword is returned [L13,L16,L17].

## Questions

- What is field 0x108 (sample rate / slots per unit of a2) and the unit of a2?
- What do the 8-byte entries contain (two dwords), and why return only the first one?
- Why subtract 1 before the wrap (previous sample vs off-by-one against 1-based count)?
- What exactly does `CRTFtol` do (float -> int truncation vs rounding)?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
