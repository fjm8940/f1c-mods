# map_RacePhasePitEntryTrigger — declared `sub_429EB0`

## Identity

- Address: `0x429EB0`; file `functions/RacePhasePitEntryTrigger_00429eb0.c`; 13 lines.
- Signature: `int __thiscall sub_429EB0(_DWORD *this)`.
- Callers: `AIAbility` (`0x429470`) L188 (`result = RacePhasePitEntryTrigger(this);`); grep `functions/`
  for `RacePhasePitEntryTrigger(` -> `functions/AIAbility_00429470.c` only.
- Callees (from `offsets_RacePhasePitEntryTrigger.csv` CALL rows): `FuelMultiplierValue` (`0x42AD20`),
  `ForcePitExit` (`0x42A110`).
- `this` = F1CVehicle (AIAbility passes its vehicle `this`); `this[N]` byte offset = `N * 4`.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4550 | 17744 | _DWORD | L8 (`this[4436]`) | laneMode / waypoint set selector (doc 54 / FIELD_GLOSSARY) |

Global read, not an offset (not in the CSV): `dword_7C56A8` compared with 5 at L9.

## Outputs

None.

## Constants

| value | lines | note |
|---|---|---|
| 4436 | L8 | `this[N]` index (byte 0x4550) |
| 5 | L9 | `dword_7C56A8` compare |

## Logic

```
result = FuelMultiplierValue(this)                    [L5]
if (result < 1) {                                     [L6]
    result = this[4436]                               [L8]
    if (!result && dword_7C56A8 == 5)                 [L9]
        return ForcePitExit(this)                     [L10]
}
return result                                         [L12]
```

## Meaning (GUESS)

- GUESS Last-step pit trigger called from AIAbility each tick: it only acts when
  `FuelMultiplierValue(this) < 1` [L5,L6].
- GUESS `this[4436]` (0x4550, laneMode / waypoint set selector) non-zero means a pit/lane mode is already
  set, suppressing the trigger [L8,L9].
- GUESS With laneMode == 0 and global `dword_7C56A8 == 5`, it calls `ForcePitExit`; 5 is likely a race
  phase value (compare `dword_7C56A4 != 5` in `ServerSessionGateCheck`) [L9,L10].
- GUESS The function returns its first argument result normally and the `ForcePitExit` result when
  triggered; AIAbility stores that result [L12; caller AIAbility L188].
- GUESS The `result < 1` gate makes this a low-fuel condition: only when the fuel multiplier is below 1
  does the pit-entry force become eligible [L6].

## Questions

- What is `dword_7C56A8`? Same session/race-phase global family as `dword_7C56A4` (ServerSessionGateCheck
  compares it to 5) and `dword_7C56A8 == 5` here.
- What does `FuelMultiplierValue` return exactly (fuel multiplier, laps of fuel, 0..N), and why does
  `< 1` gate the trigger?
- Does `ForcePitExit` (`0x42A110`) mean "force entry into the pit lane" or "force release from the pits"?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
