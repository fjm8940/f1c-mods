# map_FuelWeightLimitCheck — `sub_42AD50` (`FuelWeightLimitCheck`)

## Identity

- Address: `0x42AD50`; file `functions/FuelWeightLimitCheck_0042ad50.c`; 5 lines.
- Signature: `BOOL __thiscall sub_42AD50(int this)`.
- Callers (grep `functions/` for `FuelWeightLimitCheck(`): `AIAbility` (`0x429470`) L181
  (`!FuelWeightLimitCheck(*((_DWORD *)this + 4478))`), L182 (`!FuelWeightLimitCheck(this)`).
- Callees (from `offsets_FuelWeightLimitCheck.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x403C | 16444 | _DWORD | L4 | lapNumber (`*(int *)(this + 16444)`; inspect) |
| 0x4040 | 16448 | int | L4 | ? (`*(int *)(this + 16448)`; map_AIAbility L97) |
| 0x3984 | 14724 | float | L4 | fuelRateFactor (`*(float *)(this + 14724)`; inspect) |
| 0x3448 | 13384 | float | L4 | ? (`*(float *)(this + 13384)`; paired with fuelRateFactor) |

Globals read: `dword_7D0B64` (L3, `FuelMult`), `dword_8ACF8C` (L4, TotalRaceLaps, doc 12).

## Outputs

None. The function returns a `BOOL`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/FuelWeightLimitCheck_0042ad50.c`.

| value | lines | note |
|---|---|---|
| 0.33333334 | L4 | one-third scale on `this + 16448` |
| 0.34999999 | L4 | subtraction from the fuel-rate product |
| 16444, 16448, 14724, 13384 | L4 | byte offsets on `this` |

## Logic

NOTE: hex offsets below are cited on source line L4, which contains all four decimal byte offsets.

```
return dword_7D0B64 == 0 || (double)(dword_8ACF8C - *(int *)(this + 0x403C)) - (double)*(int *)(this + 0x4040) * 0.33333334 < *(float *)(this + 0x3984) * *(float *)(this + 0x3448) - 0.34999999     [L3,L4]
```

## Meaning (GUESS)

- GUESS Returns TRUE when fuel is (or may be) insufficient: with `FuelMult == 0` it always returns TRUE
  (feature disabled), otherwise the "remaining fuel" estimate is compared against the fuel-rate product
  [L3,L4].
- GUESS `dword_8ACF8C - this[16444]` is laps remaining (TotalRaceLaps − lapNumber) and
  `this[16448] * (1/3)` subtracts a pit-stop/partial-lap penalty [L4].
- GUESS `this[14724] * this[13384] - 0.35` is the required fuel for the remaining distance with a 0.35
  reserve margin [L4, map_AIAbility].
- GUESS `AIAbility` L181-L182 requires **both** the teammate and the own car to report "insufficient"
  before calling `ForcePitExit`, i.e. the check gates a fuel-driven pit request [caller].
- GUESS The 0.33333334 (1/3) factor converts some integer count (stops? laps of an out-lap?) into laps
  [L4].
- GUESS The comparison direction (`<`) means "remaining fuel estimate below the requirement" = limit
  reached [L4].

## Questions

- What is `this + 16448` (0x4040)? The × 1/3 scale is unexplained; map_AIAbility L97 only records it as
  an unnamed dword.
- Is "remaining fuel" really `laps_remaining - count/3`, or are the units mixed (laps vs fuel units)?
- Why does `FuelMult == 0` return TRUE? Downstream `!FuelWeightLimitCheck(...)` then blocks the pit
  trigger when the feature is off — confirm the intended default.
- `this[13384]` (0x3448) is also used by `FuelMultiplierValue` L4; is it the same quantity in both
  formulas? (both maps leave it unnamed.)
- `AIAbility` calls this on two different `this` pointers (teammate via index 4478, own car) — is the
  struct layout identical for both?
