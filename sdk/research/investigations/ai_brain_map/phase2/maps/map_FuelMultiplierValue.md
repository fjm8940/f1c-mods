# map_FuelMultiplierValue — `sub_42AD20` (`FuelMultiplierValue`)

## Identity

- Address: `0x42AD20`; file `functions/FuelMultiplierValue_0042ad20.c`; 7 lines.
- Signature: `int __thiscall sub_42AD20(float *this)`.
- Callers (grep `functions/` for `FuelMultiplierValue(`): `AIAbility` (`0x429470`) L176, L178 (with
  `*((_DWORD *)this + 4478)` = teammate vehicle); `RacePhasePitEntryTrigger` (`0x429EB0`) L5
  (`result = FuelMultiplierValue(this);`).
- Callees (from `offsets_FuelMultiplierValue.csv` CALL rows): `CRTFtol` L4.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x3984 | 14724 | float | L4 | fuelRateFactor (`this[3681]`; inspect, map_AIAbility) |
| 0x3448 | 13384 | float | L4 | ? (`this[3346]`; paired with fuelRateFactor) |

Global read: `dword_7D0B64` (L3, `FuelMult`, doc 12; treated as an int/bool here).

## Outputs

None. The function returns an `int`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/FuelMultiplierValue_0042ad20.c`.

| value | lines | note |
|---|---|---|
| 0.2 | L4 | subtracted before `CRTFtol` |
| 100 | L6 | returned when `FuelMult` is 0 |
| 3681, 3346 | L4 | `this[N]` indices (0x3984, 0x3448) |

## Logic

NOTE: source uses scaled `this[N]` indexing; Logic keeps the source-literal indices and no hex tokens.
Byte offsets are in the table above.

```
if (dword_7D0B64 == 0) return 100                       [L3,L5,L6]
return CRTFtol(this[3681] * this[3346] - 0.2)           [L4]
```

## Meaning (GUESS)

- GUESS With fuel multiplier disabled (`dword_7D0B64 == 0`) it returns `100` as a neutral percentage
  [L3,L6].
- GUESS Otherwise it returns `(fuelRateFactor * this[3346] - 0.2)` converted to integer — a
  percentage/lap margin of available fuel [L4].
- GUESS `AIAbility` L176-L182 treats `< 2` and `0 < v16 < 3` as a low-fuel window that arms a pit request
  via `ForcePitExit` [caller].
- GUESS The 0.2 subtraction is a safety margin (laps of reserve fuel) before conversion [L4].
- GUESS `this[3346]` (0x3448) is a capacity/consumption scale paired with `fuelRateFactor` (0x3984)
  [L4].
- GUESS The `100` path keeps callers' arithmetic (e.g. percentage comparisons) valid when the fuel
  multiplier feature is off [L3,L6].

## Questions

- `dword_7D0B64` is named `FuelMult` and used elsewhere as a float; here it is tested as an integer
  zero/non-zero. Is it a float multiplier or a feature flag? (doc 12 lists it as float `read`.)
- What are the units of the return value: percent, tenths of a lap, or laps × 100? Callers compare it
  with the small integers 2 and 3.
- What is `this[3346]` (0x3448)? `fuelRateFactor` is `this[3681]` (0x3984) per map_AIAbility; the pair's
  product minus 0.2 is unexplained.
- `CRTFtol` is an unmapped CRT float-to-long helper; confirm rounding (truncation) semantics.
- Why is `*((_DWORD *)this + 4478)` passed as the teammate vehicle at AIAbility L178 — does the teammate
  struct have the same 0x3984/0x3448 layout?
