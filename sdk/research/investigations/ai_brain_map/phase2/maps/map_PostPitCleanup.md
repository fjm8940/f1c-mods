# map_PostPitCleanup — `sub_42A6F0` (`PostPitCleanup`)

## Identity

- Address: `0x42A6F0`; file `functions/PostPitCleanup_0042a6f0.c`; 13 lines.
- Signature: `char __thiscall sub_42A6F0(int this)`.
- Callers (grep `functions/` for `PostPitCleanup(`): `AIAbility` (`0x429470`) L137; `PitStopProgress`
  (`0x4A68B0`) L78 (`PostPitCleanup(*(_DWORD *)this);`); `PitStopDecisionTireCompound` (`0x42A3D0`) L88.
- Callees (from `offsets_PostPitCleanup.csv` CALL rows): `SentinelSlotFill` L5.
- Unmatched offset-like line: L5 (`SentinelSlotFill(this + 16436);` = byte offset 0x4034).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4584 | 17796 | _BYTE | L6 | ? pit flag (`*(_BYTE *)(this + 17796)`; also written by `PitStopDecisionTireCompound`) |
| 0x45E0 | 17888 | _DWORD | L8 | ? pit compound index (`*(_DWORD *)(this + 17888)`; read-modify-write `++`) |

Global read: `dword_7C56A4` (L9, SessionType, `== 7` race).

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x45E0 | 17888 | _DWORD | L8 | ? pit compound index incremented when 0x4584 is zero |
| 0x4534 | 17716 | _DWORD | L10 | ? time stamp = 0 (map_AIAbility L58) |
| 0x447C | 17532 | float | L11 | stuck timer = `dbl_8AFEA0 + 4.0` (FIELD_GLOSSARY) |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/PostPitCleanup_0042a6f0.c`.

| value | lines | note |
|---|---|---|
| 7 | L9 | SessionType compare |
| 4.0 | L11 | added to `dbl_8AFEA0` (session time) |
| 16436, 17796, 17888, 17716, 17532 | L5-L11 | byte offsets on `this` |

`dbl_8AFEA0` (L11) is doc 12 `SessionTime` (current session elapsed time).

## Logic

NOTE: hex offsets below are cited on source lines that contain their decimal value.

```
SentinelSlotFill(this + 0x4034)                          [L5]
result = *(_BYTE *)(this + 0x4584)                       [L6]
if (!result) ++*(_DWORD *)(this + 0x45E0)                [L7,L8]
if (dword_7C56A4 == 7) *(_DWORD *)(this + 0x4534) = 0    [L9,L10]
*(float *)(this + 0x447C) = dbl_8AFEA0 + 4.0             [L11]
return result                                            [L12]
```

## Meaning (GUESS)

- GUESS Post-pit-stop reset: fills a sentinel slot at `this + 0x4034`, then bumps the pit/compound counter
  `0x45E0` only when the `0x4584` flag is clear (i.e. not already handled) [L5-L8].
- GUESS In a race session (`SessionType == 7`) it clears the timestamp `0x4534` (the same field
  `PitStopDecisionTireCompound` L67 stamps with `SessionTime + 180.0`) [L9,L10].
- GUESS It (re)arms the stuck watchdog `0x447C` to `SessionTime + 4.0`, exactly the same value written by
  `AIMainUpdate` L235/L307 (FIELD_GLOSSARY: stuck watchdog timer ~4 s) [L11].
- GUESS The returned `char` is the pre-increment value of the `0x4584` flag, so callers know whether the
  cleanup path incremented the compound counter [L6,L12].
- GUESS Called at the end of `PitStopDecisionTireCompound` when the stop was not blocked (`!v16`), and
  from `AIAbility` pit-completion (`LABEL_57`) and `PitStopProgress` [callers].
- GUESS `SentinelSlotFill(this + 0x4034)` probably marks a pit-progress/sentinel slot as consumed; the
  name comes from the phase-2 label [L5].

## Questions

- What is `this + 0x4034` (16436), the argument to `SentinelSlotFill`? Not in `FIELD_GLOSSARY`; who reads it?
- Is the increment at L8 a "number of pit stops made" counter (`0x45E0` = pit compound index in
  map_AIAbility)? The global `PitStopDecisionTireCompound` also writes/reads `this[4472]` = `0x45E0`.
- `0x4534` is described as "? time stamp written before pit servicing" (FIELD_GLOSSARY) and as a
  `dbl_8AFEA0 + 120/180` target in map_AIAbility; here it is reset to 0. Confirm the semantics.
- Does `SentinelSlotFill` write state, and is the `0x4034` slot documented anywhere?
- Why is the counter increment conditional on the `0x4584` flag being clear rather than always?
