# map_ForcePitExit — `sub_42A110` (`ForcePitExit`)

## Identity

- Address: `0x42A110`; file `functions/ForcePitExit_0042a110.c`; 24 lines.
- Signature: `int __thiscall sub_42A110(_DWORD *this)`.
- Callers (grep `functions/` for `ForcePitExit(`): `AIAbility` (`0x429470`) L90, L184; `AIPushLap`
  (`0x42A180`) L41, L77, L87; `RacePhasePitEntryTrigger` (`0x429EB0`) L10 (`return ForcePitExit(this);`).
- Callees (from `offsets_ForcePitExit.csv` CALL rows): `TeammatePartValueLookup` L10;
  `StateConditionalAction` L13, L20; `AIStateFlagSetter` L14, L21.
- `this + 3251` arguments at L10/L13/L20 are byte offset `3251 * 4 = 13004 = 0x32CC` (the unmatched file
  records them because they are pointer expressions, not parsed offset rows).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x403C | 16444 | _DWORD | L8 | lapNumber (`this[4111]`; inspect) |
| 0x4064 | 16484 | _DWORD | L11 | ? state (`this[4121]`, compare == 1; map_AIAbility L74) |

Globals read: `dword_7C56A4` (L5, SessionType; phase-1 maps label it RacePhase), `dword_8ACF8C`
(L7, L8, TotalRaceLaps, doc 12).

## Outputs

None. Field writes happen inside `AIStateFlagSetter`/`StateConditionalAction`; this function itself only
returns an `int`.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/ForcePitExit_0042a110.c`.

| value | lines | note |
|---|---|---|
| 7 | L5 | SessionType == 7 (race) |
| 4 | L11 | TeammatePartValueLookup result compare |
| 4111 | L8 | lapNumber index (0x403C) |
| 4121 | L11 | state index (0x4064) |
| 3251 | L10, L13, L20 | `this + 3251` argument base (0x32CC) |

## Logic

NOTE: source mixes scaled `this[N]` indices with the `this + 3251` pointer form; Logic keeps both source
forms and no hex tokens. Byte offsets are in the tables above.

```
if (dword_7C56A4 == 7) {                                    [L5,L6]
  result = dword_8ACF8C - 1                                 [L7]
  if (this[4111] != dword_8ACF8C - 1) {                     [L8,L9]
    result = TeammatePartValueLookup(this + 3251)           [L10]
    if (!result || result == 4 || this[4121] == 1) {        [L11,L12]
      StateConditionalAction(this + 3251)                   [L13]
      return AIStateFlagSetter(1)                           [L14]
    }                                                       [L15]
  }                                                         [L16]
} else {                                                    [L17,L18,L19]
  StateConditionalAction(this + 3251)                       [L20]
  return AIStateFlagSetter(1)                               [L21]
}                                                           [L22]
return result                                               [L23]
```

## Meaning (GUESS)

- GUESS In a race session (`dword_7C56A4 == 7`), forces the pit-exit action only when the car is not on
  the final lap (`this[4111] != TotalRaceLaps - 1`) and the teammate lookup returns none/4 or the state
  flag `this[4121] == 1` [L5-L14].
- GUESS In any non-race session it forces the pit exit unconditionally [L18-L21].
- GUESS When the guards block it, the function returns `TotalRaceLaps - 1`, i.e. the "final lap" value,
  which callers may compare or ignore [L7,L23].
- GUESS `AIStateFlagSetter(1)` sets a state bit that marks the forced pit exit; `StateConditionalAction`
  drives the actual state machine transition on `this + 3251` [L13,L14].
- GUESS Callers use it exactly when they want the AI to leave the pit lane: `RacePhasePitEntryTrigger`
  returns it directly, `AIPushLap` calls it three times, `AIAbility` twice [callers].
- GUESS `TeammatePartValueLookup(this + 3251)` looks up a teammate's pit/part decision; value 4 or 0
  means "no teammate constraint", allowing the exit [L10,L11].

## Questions

- What does `TeammatePartValueLookup` return for each value (0, 4, others)? No map exists.
- What are the fields at `this + 3251` (byte 0x32CC) — is that a sub-object base passed to
  `StateConditionalAction`?
- `dword_7C56A4` is SessionType in doc 12 but "RacePhase" in phase-1 maps; the `== 7` value should be
  cross-checked (race session vs race phase).
- The early return `result = TotalRaceLaps - 1` outside the guard branches leads to returning that value
  with no side effects — is that intentional or dead? Callers (`RacePhasePitEntryTrigger`) return it as
  the function result.
- Does `AIStateFlagSetter` write a field on `this` or a global? (affects the Outputs table; currently
  treated as callee-internal).
