# map_AIClearStateFlags — declared `sub_42AE00`

## Identity

- Address: `0x42AE00`; file `functions/AIClearStateFlags_0042ae00.c`; 8 lines.
- Signature: `int __thiscall sub_42AE00(int this)`.
- Callers (grep `functions/` for `AIClearStateFlags(`): `AIAbility` (`0x429470`) L147; `AIMainUpdate`
  (`0x424E70`) L222; `AIRacingLine` (`0x425920`) L169; `AIDriverStructInit` (`0x424790`) L52;
  `CameraViewportFrustumInitialization` (`0x424B20`) L32; `RaceStructInit` (`0x423E80`) L7;
  `SteeringAngleController` (`0x425670`) L71. Manifest callers (AIAbility, AIMainUpdate, AIRacingLine,
  SteeringAngleController) confirmed; three extra callers found.
- Callees (from `offsets_AIClearStateFlags.csv` CALL rows): `memset` (L6).
- `this` = vehicle/AI object passed directly by callers. The memset destination `this + 17236` is vehicle
  offset `0x4354` (current waypoint ptr, `FIELD_GLOSSARY.md`), so the clear covers 0x4354..0x43A3.

## Inputs

None (no offset read; only `this` and the constants).

## Outputs

None in the CSV (the extraction has only the `memset` CALL row; the destination/width lines were not
parsed - see Questions). Functionally it zeroes 80 bytes at `this + 17236`.

## Constants

| value | lines | note |
|---|---|---|
| 17236 | L6 | memset destination (`this + 17236`, vehicle 0x4354) |
| 80 | L6 | memset size (`0x50u`) |

## Logic

```
result = 0                                            [L5]
memset(this + 17236, 0, 80)                           [L6]
return result                                         [L7]
```

## Meaning (GUESS)

- GUESS Clears 80 bytes (20 dwords) of vehicle waypoint state starting at 0x4354: current waypoint ptr
  plus the 19 look-ahead waypoint pointers at 0x4358..0x43A0 [L6].
- GUESS Used to reinitialize the racing-line locator on pit release / session reset - callers include
  `RaceStructInit`, `AIDriverStructInit`, `CameraViewportFrustumInitialization` and
  `SteeringAngleController` (grep), i.e. both construction and per-race reset paths.
- GUESS Always returns 0; return value is a status placeholder only [L5,L7].

## Questions

- The CSV extractor did not produce a W row for the memset; `unmatched_AIClearStateFlags.txt` L6 has the
  raw line `memset((void *)(this + 17236), 0, 0x50u);`. Destination 0x4354 and size 80 are therefore
  carried in Constants/Logic only, not in Outputs.
- Is the cleared block exactly current waypoint ptr (0x4354) + 19 look-ahead pointers (0x4358..0x43A0)?
  `FIELD_GLOSSARY.md` names 0x4358 as "first of the 19 look-ahead waypoint pointers"; 0x50 bytes matches.
- Callers in construction paths (`RaceStructInit`, `AIDriverStructInit`) may pass a partially built object;
  does zeroing the whole block there match the runtime-reset use in AIAbility/AIMainUpdate?
