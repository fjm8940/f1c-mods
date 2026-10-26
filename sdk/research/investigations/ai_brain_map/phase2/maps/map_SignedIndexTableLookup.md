# map_SignedIndexTableLookup — declared `sub_4FAE70`

## Identity

- Address: `0x4FAE70`; file `functions/SignedIndexTableLookup_004fae70.c`; 20 lines.
- Signature: `double __thiscall sub_4FAE70(float *this, float a2)`.
- Callers (grep `functions/` for `SignedIndexTableLookup(`): `AISteering` (`0x4270A0`) L681, L686, L722,
  L727, L828, L854.
- Callees (from `offsets_SignedIndexTableLookup.csv` CALL rows): `CRTFtol` (`0x587BDC`).
- `this` = F1CVehicle (or a table owner); `this[N]` byte offset = `N * 4`. The two 8-entry tables selected
  here start at index 59 (byte 0xEC) and index 51 (byte 0xCC).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0xEC | 236 | float | L11 (`return this[v3 + 59]`) | ? non-negative-side table, 8 entries `[0..7]` |
| 0xCC | 204 | float | L18 (`return this[v4 + 51]`) | ? negative-side table, 8 entries `[0..7]` |

## Outputs

None.

## Constants

| value | lines | note |
|---|---|---|
| 59 | L11 | table base index for `v3 >= 0` (byte 0xEC) |
| 51 | L18 | table base index for `v3 < 0` (byte 0xCC) |
| 8 | L9, L16 | clamp threshold (indexes 8+ -> 7) |
| 7 | L10, L17 | clamped index |
| 0.0 | L7 | sign test on the converted value |

## Logic

```
v3 = CRTFtol(a2)                            [L6]
if ((double)v3 >= 0.0) {                    [L7]
    if (v3 >= 8) v3 = 7                     [L9,L10]
    return this[v3 + 59]                    [L11]
} else {
    v4 = -v3                                [L15]
    if (v4 >= 8) v4 = 7                     [L16,L17]
    return this[v4 + 51]                    [L18]
}
```

## Meaning (GUESS)

- GUESS Quantized signed lookup: converts `a2` to an integer with `CRTFtol`, then indexes one of two
  8-entry float tables in `this` — table at index 59 (byte 0xEC) when the value is >= 0, table at index 51
  (byte 0xCC) when negative, using the absolute value [L6-L18].
- GUESS Indexes are saturated at 7, so out-of-range inputs return the last entry of the selected table
  [L9,L10,L16,L17].
- GUESS The asymmetry (two tables, one for each sign) suggests a signed response curve: lateral
  error/grip on one side uses one table, the other side uses the mirrored table [L7,L11,L18].
- GUESS AISteering calls it six times (L681/L686/L722/L727/L828/L854) around lateral/cornering decisions
  [callers].

## Questions

- What are the 8 floats in each table (slip curve? steering response? grip vs angle), and are the
  negative-side entries stored mirrored or independent?
- Why is the table base 59 for positive and 51 for negative (8 entries each, contiguous 51..66)?
- What exactly does `CRTFtol` return for negative inputs (truncation toward zero) — does the `-v3`
  path double-handle negatives?
- Are the tables written by a setup/loader function (who populates 0xCC..0xEC)?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
