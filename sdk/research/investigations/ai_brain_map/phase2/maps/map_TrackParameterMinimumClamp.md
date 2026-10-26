# map_TrackParameterMinimumClamp — `sub_42A740` (`TrackParameterMinimumClamp`)

## Identity

- Address: `0x42A740`; file `functions/TrackParameterMinimumClamp_0042a740.c`; 7 lines.
- Signature: `void __thiscall sub_42A740(float *this, float a2)`.
- Callers (grep `functions/` for `TrackParameterMinimumClamp(`): `AISteeringCorrection` (`0x426CF0`) L80
  (passes `*(float *)(this + 17720)` = maxSpeed 0x4538); `AISteering` (`0x4270A0`) L580, L589, L597, L603,
  L700, L741, L758, L1066, L1075, L1124, L1178, L1183, L1198, L1207, L1250, L1281;
  `AISteeringRacingLineCorrection` (`0x42B0E0`) L80, L84 (15.0), L99 (49.0), L157, L188, L192 (35.0).
- Callees (from `offsets_TrackParameterMinimumClamp.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4484 | 17540 | float | L3 (`this[4385] > (double)a2`) | speed limit (FIELD_GLOSSARY; reset to 1e6 each tick by AIMainUpdate) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4484 | 17540 | float | L4 (`this[4385] = a2`) | speed limit, written twice (L4 and L6) |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/TrackParameterMinimumClamp_0042a740.c`.

| value | lines | note |
|---|---|---|
| 4385 | L3, L4, L5, L6 | `this[N]` index (0x4484) |
| 5.0 | L5, L6 | hard lower bound |
| 0.0, 1.0 | — | none in source (comparisons use `a2`) |

## Logic

NOTE: source uses scaled `this[N]` indexing; the Logic keeps the source-literal index (no hex tokens).

```
if (this[4385] > a2) this[4385] = a2        [L3,L4]
if (this[4385] < 5.0) this[4385] = 5.0      [L5,L6]
```

## Meaning (GUESS)

- GUESS Lowers the speed-limit field 0x4484 to at most the requested value `a2`, but never below 5.0 m/s
  [L3-L6].
- GUESS `this` = vehicle base: caller `AISteeringCorrection` passes vehicle offset 0x4538 (`maxSpeed`,
  FIELD_GLOSSARY "pit/speed governor target"); `AISteeringRacingLineCorrection` passes 15.0/49.0/35.0,
  consistent with pit-lane and race-phase speed caps.
- GUESS The lower floor of 5.0 stops a caller (or a mod) from installing a zero limit that would freeze the AI.
- GUESS `AIMainUpdate` resets 0x4484 to 1e6 each tick (FIELD_GLOSSARY), so this function only ever narrows it
  inside a tick.

## Questions

- Units of `a2` (m/s?) and origin of the exact 5.0 floor.
- Why is `a2` (not the stored value) the comparison key in L3 while the stored field is clamped again in L5?
- Does any caller rely on the lower clamp or only on the upper?
