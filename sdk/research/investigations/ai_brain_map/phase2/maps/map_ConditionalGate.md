# map_ConditionalGate — declared `sub_4EFAC0`

## Identity

- Address: `0x4EFAC0`; file `functions/ConditionalGate_004efac0.c`; 11 lines.
- Signature: `int __thiscall sub_4EFAC0(_DWORD *this)`.
- Callers (grep `functions/` for `ConditionalGate(`): `AIAbility` (`0x429470`) L111
  (`ConditionalGate(this + 3);`); `AIMainUpdate` (`0x424E70`) L79 and L88 (`ConditionalGate(this + 12);`);
  `AISteeringRacingLineCorrection` (`0x42B0E0`) L258 (`ConditionalGate(this + 3);`) - all three manifest
  callers; also `AIStateReset` L6, `PitStopDecisionTireCompound` L47/L56, `sub_4A0250` L43
  (`ConditionalGate(dword_7C54E8 + 12);`).
- Callees (from `offsets_ConditionalGate.csv` CALL rows): none.
- `this` = vehicle + 12 bytes: callers pass `this + 3` (dword index) or `+ 12` (bytes) or
  `dword_7C54E8 + 12`. Therefore `this[3247]` (12988) = vehicle `0x32C8` (13000) and `this[3685]` (14740) =
  vehicle `0x39A0` (14752) - the same shifted-base pattern as `map_BrakeThermalModel.md`.

## Inputs

CSV access R rows.

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x32BC | 12988 | _DWORD | L5 | `this[3247]`; effective vehicle 13000 = vehicle flag (AISteering part2 L368; AIPushLap L15) |

## Outputs

CSV access W rows.

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x3994 | 14740 | _DWORD | L7 | `this[3685] = 0`; effective vehicle 14752 = EngineRPM (inspect CONFIRMED; `map_AISteeringRacingLineCorrection.md` L32, `map_PhysicsGearShiftLogicWithThresholds.md` L26) |

Also writes the global `byte_7C9081 = 0` (L9); globals have no offset row.

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 3247 | L5 | `this` dword index (input, effective vehicle 13000) |
| 3685 | L7 | `this` dword index (output, effective vehicle 14752) |

## Logic

```
result = this[3247]                     [L5]
if (result) this[3685] = 0              [L6,L7]
else byte_7C9081 = 0                    [L9]
return result                           [L10]
```

## Meaning (GUESS)

- GUESS Vehicle-flag-conditional gate on the shifted base: when the vehicle flag at effective 0x32C8 is
  set, it zeroes the effective 0x39A0 dword - named EngineRPM (inspect) in two phase-1 maps [L5-L7].
- GUESS When the flag is clear it clears the global byte `byte_7C9081` instead; that global is not named
  anywhere in the phase-1 maps [L9].
- GUESS Callers wrap it in race-phase/session checks: `AISteeringRacingLineCorrection` L257 only calls it
  when `dword_7C56A8 == 1`, `AIAbility` L109 only when `dword_7C56A4 == 7` [callers].
- GUESS Despite the EngineRPM target, the "zero the RPM" write may actually be resetting a different
  0x39A0 float in this context (the shifted base makes the field identity uncertain) - flagged below.

## Questions

- Confirm the shifted-base mapping: `this` = vehicle + 12, so `this[3247]` is the vehicle flag 0x32C8 and
  `this[3685]` is vehicle 0x39A0. If so, why would this function zero EngineRPM when flag 0x32C8 is set?
- `byte_7C9081` is unnamed; what does it gate?
- The manifest name "ConditionalGate" is generic; is the real behavior "engine cut / stall reset"?
- `sub_4A0250` L43 passes `dword_7C54E8 + 12` (a global vehicle pointer?) while AIAbility passes
  `this + 3`; both resolve to +12 bytes only if the bases are values, not dword pointers.
