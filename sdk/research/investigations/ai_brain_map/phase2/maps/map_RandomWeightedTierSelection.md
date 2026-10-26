# map_RandomWeightedTierSelection — declared `sub_429FD0`

## Identity

- Address: `0x429FD0`; file `functions/RandomWeightedTierSelection_00429fd0.c`; 63 lines.
- Signature: `int __thiscall sub_429FD0(_DWORD *this)`.
- Callers (grep `functions/` for `RandomWeightedTierSelection(`): `AIMainUpdate` (`0x424E70`) L295;
  `AISteeringRacingLineCorrection` (`0x42B0E0`) L125; `sub_429960` (`0x429960`) L263.
- Callees (from `offsets_RandomWeightedTierSelection.csv` CALL rows): `RandLCGRandomNumberGenerator`
  (`0x5884ED`), `CompoundIndexLookup` (`0x4D7570`), `TireCompoundNameIDMapping` (`0x4A4830`).
- `this` = F1CVehicle; `this[N]` byte offset = `N * 4`.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x458C | 17804 | _DWORD | L12 (`v2 = this[4451]`) | ? fallback tier value |
| 0x3978 | 14712 | _DWORD (pointer) | L54 (`this[3678] + 16` loop count) | ? tire-compound table pointer (count at +16) |

Global read at L10/L15/L28/L34/L36/L38, not an offset (not in the CSV): `flt_8AFEE8` (weight in 0..1).

## Outputs

None. Returns an index (`v5`) into the tire-compound list.

## Constants

| value | lines | note |
|---|---|---|
| 0.15000001 | L10 | first weight threshold |
| 0.2 | L15 | second weight threshold |
| 0.5 | L28 | third weight threshold |
| 0.55000001 | L34 | fourth weight threshold |
| 0.80000001 | L36 | fifth weight threshold |
| 0.85000002 | L38 | sixth weight threshold |
| 6 | L39, L41 | tier value |
| 5 | L45 | tier value |
| 4 | L31, L50 | tier value |
| 2 | L41, L50 | modulo for the coin-flip |
| 0x80000001 | L17 | signed `% 2` mask (decimal 2147483649 unsigned / -2147483647 signed) |
| 0xFFFFFFFE | L20 | signed `% 2` mask (decimal 4294967294 unsigned / -2 signed) |
| -1 | L20 | `% 2` compare |
| 16 | L54 | count field offset in the table header pointer 0x3978 |
| 4451 | L12, L23 | `this[N]` index (byte 0x458C) |
| 3678 | L54 | `this[N]` index (byte 0x3978) |

## Logic

```
if (flt_8AFEE8 < 0.15000001) { v2 = this[4451]; goto LABEL_17 }            [L10,L11,L12,L13]
if (flt_8AFEE8 < 0.2) {                                                    [L15]
    v4 = RandLCGRandomNumberGenerator(this) & 2147483649                   [L17]
    v3 = v4 == 0                                                           [L18]
    if (v4 < 0)                                                            [L19]
        v3 = (((BYTE)v4 - 1) | -2) == -1                                   [L20]
    if (!v3) { v2 = this[4451]; goto LABEL_17 }                            [L21,L22,L23,L24]
    else { v2 = 4; goto LABEL_17 }        /* LABEL_9 */                    [L26,L31]
}
if (flt_8AFEE8 < 0.5) { v2 = 4; goto LABEL_17 }                            [L28,L30,L31]
if (flt_8AFEE8 >= 0.55000001) {                                            [L34]
    if (flt_8AFEE8 >= 0.80000001) {                                        [L36]
        if (flt_8AFEE8 >= 0.85000002) v2 = 6                               [L38,L39]
        else v2 = 6 - (RandLCGRandomNumberGenerator(this) % 2 != 0)        [L41]
    } else v2 = 5                                                          [L45]
} else {
    v2 = (RandLCGRandomNumberGenerator(this) % 2 != 0) + 4                 [L50]
}
LABEL_17:
v5 = 0                                                                     [L53]
for (i = 0; i < *(_DWORD *)(this[3678] + 16); ++i) {                       [L54]
    v7 = *(char **)CompoundIndexLookup(i)                                  [L56]
    if (!v7) v7 = &byte_5EA438                                             [L57,L58]
    if (v2 == TireCompoundNameIDMapping(v7)) v5 = i                        [L59,L60]
}
return v5                                                                  [L62]
```

## Meaning (GUESS)

- GUESS Maps the global weight `flt_8AFEE8` (0..1) to a discrete tire tier 4, 5 or 6 with tuned
  probabilities: <0.15 keeps the current tier 0x458C, 0.15-0.2 is a coin flip between 0x458C and tier 4,
  0.2-0.5 is tier 4, 0.5-0.55 a coin flip between 4 and 5, 0.55-0.8 is tier 5, 0.8-0.85 a coin flip
  between 6 and 5, >=0.85 is tier 6 [L10-L51].
- GUESS The `& 0x80000001` / `| 0xFFFFFFFE` sequence at L17-L20 is the compiler's signed `% 2 != 0` test:
  even random -> keep current tier, odd -> tier 4 [L17-L26].
- GUESS The second stage scans the compound table at `this[3678]` (count at +16), resolves each entry name
  with `CompoundIndexLookup` (null -> empty string `byte_5EA438`) and maps it with
  `TireCompoundNameIDMapping`; the matching list index is returned [L54-L62].
- GUESS If no entry matches, `v5` stays 0 (first compound), so the function degrades gracefully to the
  first compound [L53,L62].
- GUESS The name (phase-2 target) "weighted tier selection" matches the code: weight -> tier id -> list
  index. Callers AIMainUpdate/AISteeringRacingLineCorrection use the index to pick a compound for pit
  strategy [callers].

## Questions

- What is `flt_8AFEE8` (global) and who writes it (weather? tire wear? race randomness)?
- What is tier value 0x458C (`this[4451]`) — current compound index, or a cached tier id?
- What exactly do `CompoundIndexLookup` and `TireCompoundNameIDMapping` do (name lookup -> id), and is the
  return actually a tire compound index?
- Why does the fallback (no match) return index 0 rather than a sentinel?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
