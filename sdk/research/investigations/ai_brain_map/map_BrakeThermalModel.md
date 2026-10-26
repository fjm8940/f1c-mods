# map_BrakeThermalModel — declared `sub_4E87E0` (Claude review GUESS name: `AIHoldThrottle`; old name kept)

## Identity

- Address: `0x4E87E0`; file `functions/BrakeThermalModel_004e87e0.c`; 17 lines.
- Signature: `double __thiscall sub_4E87E0(float *this)`.
- Callers: `AIRacingLine` (`0x425920`) L193, L309, L316; `AISteeringCorrection` (`0x426CF0`) L90; grep `functions/`
  for `BrakeThermalModel(` -> `functions/AIRacingLine_00425920.c`, `functions/AISteeringCorrection_00426cf0.c`;
  `inspect` agrees.
- Callees (from `offsets_BrakeThermalModel.csv` CALL rows): none.
- `this` = **vehicle + 0xC** (Claude review item 3; callers pass `this + 12`). `this[N]` byte offsets are
  `index * 4` on that base; the vehicle-relative view below adds 0xC.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x39C8 | 14792 | float | L6 (`this[3698]`) | ? (denominator / reference value) |
| 0x39AC | 14764 | float | L8 (`this[3691]`) | ? (normalizer) |
| 0x3500 | 13568 | float | L10 (`this[3392]`) | ? (v2 base term) |
| 0x3A60 | 14944 | float | L10 (`this[3736]`) | ? (projection vector component) |
| 0x34D0 | 13520 | float | L10 (`this[3380]`) | ? (projection vector component) |
| 0x3A5C | 14940 | float | L10 (`this[3735]`) | ? (projection vector component) |
| 0x34C4 | 13508 | float | L10 (`this[3377]`) | ? (projection vector component) |
| 0x34B8 | 13496 | float | L10 (`this[3374]`) | ? (projection vector component) |
| 0x3A58 | 14936 | float | L10 (`this[3734]`) | ? (projection vector component) |
| 0x3D44 | 15684 | float | L11 (`this[3921]`) | ? (quadratic coefficient) |
| 0x3D40 | 15680 | float | L11 (`this[3920]`) | ? (quadratic coefficient) |
| 0x39A8 | 14760 | float | L11 (`this[3690]`) | ? (subtracted constant term) |

All inputs are read-only. There are no pointer/dereference bases; every access is `this[N]`.

## Vehicle-relative view (`this = veh + 0xC`; Claude review request)

Vehicle-relative byte offset = `index * 4 + 0xC`. Raw hex = offset on `this`.

| this index | this hex | this dec | veh hex | veh dec | name |
|---|---|---|---|---|---|
| 3698 | 0x39C8 | 14792 | 0x39D4 | 14804 | ? (denominator / reference value) |
| 3691 | 0x39AC | 14764 | 0x39B8 | 14776 | ? (normalizer) |
| 3392 | 0x3500 | 13568 | 0x350C | 13580 | ? (v2 base term) |
| 3736 | 0x3A60 | 14944 | 0x3A6C | 14956 | ? (projection vector component) |
| 3380 | 0x34D0 | 13520 | 0x34DC | 13532 | ? (projection vector component) |
| 3735 | 0x3A5C | 14940 | 0x3A68 | 14952 | ? (projection vector component) |
| 3377 | 0x34C4 | 13508 | 0x34D0 | 13520 | ? (projection vector component) |
| 3374 | 0x34B8 | 13496 | 0x34C4 | 13508 | ? (projection vector component) |
| 3734 | 0x3A58 | 14936 | 0x3A64 | 14948 | ? (projection vector component) |
| 3921 | 0x3D44 | 15684 | 0x3D50 | 15696 | ? (quadratic coefficient) |
| 3920 | 0x3D40 | 15680 | 0x3D4C | 15692 | ? (quadratic coefficient) |
| 3690 | 0x39A8 | 14760 | 0x39B4 | 14772 | ? (subtracted constant term) |

## Outputs

None. The function returns a `double`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/BrakeThermalModel_004e87e0.c`.

| value | lines | note |
|---|---|---|
| 3698, 3691, 3690 | L6, L8, L11 | `this[N]` indices |
| 3392, 3736, 3380, 3735, 3377, 3374, 3734 | L10 | `this[N]` indices |
| 3921, 3920 | L11 | `this[N]` indices |
| 0.0 | L6, L7, L8, L9, L12, L13 | early-outs / lower clamp |
| 1.0 | L14, L15 | upper clamp |

## Logic

NOTE: source uses scaled `this[N]` indexing (Hex-Rays prints element index for `float *this`). The Logic keeps the
source-literal index; byte offsets are in the tables above. No hex tokens are used on scaled lines.

```
if (this[3698] == 0.0) return 0.0                       [L6,L7]
if (this[3691] == 0.0) return 0.0                       [L8,L9]
v2 = this[3392] - (this[3736]*this[3380] + this[3735]*this[3377] + this[3374]*this[3734])   [L10]
result = (v2 * this[3921] * v2 * this[3920] / this[3698] - this[3690]) / this[3691]        [L11]
if (result < 0.0) return 0.0                            [L12,L13]
if (result > 1.0) return 1.0                            [L14,L15]
return result                                           [L16]
```

## Meaning (GUESS)

- GUESS Returns a normalized 0..1 command from a quadratic speed term:
  `v2^2 * this[3921] * this[3920] / this[3698] - this[3690]`, divided by `this[3691]`, clamped to [0,1]
  [L10,L11,L12-L15].
- GUESS `v2 = this[3392] - dot(this[3736],this[3735],this[3374] , this[3380],this[3377],this[3734])` looks like a
  forward-speed/airspeed projection: a base speed minus a wind/velocity projection [L10].
- GUESS Callers use the return as a throttle value, not as brake or temperature: AIRacingLine writes it to 0x4434 at
  L193 and L309/L316, and AISteeringCorrection writes `0.8 * return` to 0x4434 at L90 [callers].
- GUESS Early-outs when `this[3698]` or `this[3691]` is zero are divide-by-zero guards; both denominators are latched
  setup parameters [L6-L9].
- GUESS Claude review (below) is the stronger reading: the value is the throttle needed to hold current speed.

## Claude review notes (2026-10-02, DERIVED — do not re-tier)

Source: `REVIEW_2_GROUP1.md` item 3.

- `BrakeThermalModel 0x4E87E0` (`this` = veh+0xC) is NOT thermal and does NOT return brake. It returns the 0..1
  **throttle needed to hold speed** (drag-like v^2 term), clamped, and callers use it as throttle.
- Rename (GUESS name): `AIHoldThrottle`; keep the old `BrakeThermalModel` name in files/callers.

## Questions

- `this[3380]`, `this[3377]`, `this[3374]` have stride 3 floats; is that a vector with stride or three unrelated
  fields? Same for `this[3736]`, `this[3735]`, `this[3734]` (contiguous).
- Fields 0x39C8/0x39A8 are also read by `TireGripCoefficient` (raw 0x39C8/0x39A8); are these shared tire/brake
  parameters?
- Which caller wins in a tick: AIRacingLine writes 0x4434 at step 5, AISteeringCorrection at step 8 (pedal
  post-processor)? Does the 0.8 factor at AISteeringCorrection L90 apply after the AIHoldThrottle value?
