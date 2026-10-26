# map_DistanceProjectionCalc — `sub_4F0050` (`DistanceProjectionCalc`)

## Identity

- Address: `0x4F0050`; file `functions/DistanceProjectionCalc_004f0050.c`; 47 lines.
- Signature: `double __thiscall sub_4F0050(int this)`.
- Callers (grep `functions/` for `DistanceProjectionCalc(`): `AISteeringRacingLineCorrection` (`0x42B0E0`) L62
  (`this + 3`), L63 (`dword_7C54F0[v3] + 12`); `PitSelectionStateMachine` (`0x41B3C0`) L23
  (`dword_7C54E8 + 12`); `ShortComputation_bad0` (`0x49BAD0`) L5 (`dword_7C54E0 + 12`); `sub_41AD80`
  (`0x41AD80`) L28 (`a2 + 12`), L29 (`a3 + 12`); `sub_4BCCC0` (`0x4BCCC0`) L78, L306; `sub_4D1730`
  (`0x4D1730`) L58 (`v5 + 12`).
- Callees (from `offsets_DistanceProjectionCalc.csv` CALL rows): `PositionChangeMonitor` L15; `sqrt` L23.
- Callers pass `vehicle + 12`, so `this + 13524/13528/13532` = vehicle `0x34E0/0x34E4/0x34E8`
  (Euler orientation X/Y/Z per `FIELD_GLOSSARY.md`).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x110 | 272 | float * | L16 | ? (`v2` field: pointer `v3`) |
| 0x10C | 268 | float * | L17 | ? (`v2` field: pointer `v4`) |
| 0x4 | 4 | float | L19 | ? (`v3[1]`/`v4[1]`, vector component 2) |
| 0x8 | 8 | float | L20 | ? (`v3[2]`/`v4[2]`, vector component 3) |
| 0x48 | 72 | float | L22 | ? (`v2` base distance) |
| 0x8 | 8 | float | L26 | ? (`v2` reference component 3) |
| 0x34DC | 13532 | float | L26 | ? (`this` component 3; veh 13544 per FIELD_GLOSSARY) |
| 0x4 | 4 | float | L27 | ? (`v2` reference component 2) |
| 0x34D8 | 13528 | float | L27 | ? (`this` component 2; veh 13540 per FIELD_GLOSSARY) |
| 0x34D4 | 13524 | float | L28 | ? (`this` component 1; veh 13536 per FIELD_GLOSSARY) |

`*(float *)v2` (offset 0) is also read at L28 but is not represented in `offsets_DistanceProjectionCalc.csv`
(zero offset); the unmatched file is empty.

## Outputs

None. The function returns a `double`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/DistanceProjectionCalc_004f0050.c`.

| value | lines | note |
|---|---|---|
| 272, 268, 72 | L16, L17, L22, L29 | byte offsets on `v2` |
| 2 | L20 | array index (`v3[2]`/`v4[2]`) |
| 13532, 13528, 13524 | L26, L27, L28 | byte offsets on `this` |
| 8, 4 | L26, L27 | byte offsets on `v2` |
| 0.0 | L21, L31, L43 | zero-length early-out / sign test / lower clamp |
| 1.0 | L23 | inverse-norm numerator |
| `flt_5EA3FC` | L33, L35, L36, L37, L42 | symbolic cap (doc 12 `TrackRefSpeed`, configurable) |

## Logic

NOTE: source is a mix of scaled `this[N]`-free direct byte offsets; hex offsets below are cited on source
lines that contain their decimal value. Vector elements use the source array form.

```
v2 = PositionChangeMonitor(this)                                            [L15]
v3 = *(float **)(v2 + 0x110); v4 = *(float **)(v2 + 0x10C)                  [L16,L17]
v8 = *v3 - *v4; v9 = v3[1] - v4[1]; v10 = v3[2] - v4[2]                    [L18,L19,L20]
if (v8 == 0.0 && v9 == 0.0 && v10 == 0.0) return *(float *)(v2 + 0x48)      [L21,L22]
v6 = 1.0 / sqrt(v10 * v10 + v9 * v9 + v8 * v8)                              [L23]
v11 = v8 * v6; v12 = v9 * v6                                                [L24,L25]
v7 = v6 * v10 * (*(float *)(this + 0x34DC) - *(float *)(v2 + 0x8))          [L26]
   + v12 * (*(float *)(this + 0x34D8) - *(float *)(v2 + 0x4))               [L27]
   + v11 * (*(float *)(this + 0x34D4) - *(float *)v2)                       [L28]
   + *(float *)(v2 + 0x48)                                                  [L29]
if (v7 >= 0.0) {                                                            [L31]
  if (v7 > flt_5EA3FC[0]) {                                                 [L33]
    result = v7 - flt_5EA3FC[0]                                             [L35]
    if (result > flt_5EA3FC[0]) return flt_5EA3FC[0]                        [L36,L37]
  } else {                                                                  [L34]
    result = v7                                                             [L30]
  }                                                                         [L38]
} else {                                                                    [L40]
  result = v7 + flt_5EA3FC[0]                                               [L42]
  if (result < 0.0) return 0.0                                              [L43,L44]
}                                                                           [L45]
return result                                                               [L46]
```

## Meaning (GUESS)

- GUESS Computes a signed distance/progress projection: direction = normalised `v3 - v4`, dot with the
  vector from the `v2` reference point to the `this` point, plus `v2` base distance `*(v2+72)` [L16-L29].
- GUESS The result is wrapped into `[0, flt_5EA3FC]` with at most one correction step: subtract one cap
  when over, add one cap when under, clamp to 0 [L31-L44].
- GUESS Callers subtract two calls (`DistanceProjectionCalc(other) - DistanceProjectionCalc(self)`) and
  then re-wrap by `flt_5EA3FC` themselves, so the value is a track-progress scalar in metres
  (AISteeringRacingLineCorrection L62-L73).
- GUESS `flt_5EA3FC` behaves as a track length/loop size here, not a speed; doc 12 calls it
  `TrackRefSpeed` (configurable) [L33,L42].
- GUESS `v2 + 272` / `v2 + 268` are the current and previous pose/path-node pointers used to build the
  movement direction [L16,L17].
- GUESS All 6 call sites pass `vehicle + 12` (or a vehicle pointer at `+12`), so the `this+13524..13532`
  reads are the vehicle Euler orientation at `0x34E0..0x34E8` (FIELD_GLOSSARY) [L26-L28].

## Questions

- `*(float *)v2` (offset 0) is read at L28 but absent from the CSV; confirm the zero-offset reference point.
- `PositionChangeMonitor` is an unknown callee (no map) — what does it return, and what lives at
  `+0/+4/+8/+72/+268/+272` of its result?
- `v2 + 272` and `v2 + 268` are pointers to float arrays used as `*p`, `p[1]`, `p[2]`; are these 2 points
  of one path segment (current/previous node)?
- Why does the wrap use `flt_5EA3FC` (doc 12 `TrackRefSpeed`, m/s) as a length-like modulus? AISteering
  RacingLineCorrection re-wraps with the same constant at L65-L73, so it may be a shared lap/progress
  scale, not a speed.
- The `this` pointer is `veh + 12`; field `0x34E0..0x34E8` is named "Euler orientation X/Y/Z" in
  `FIELD_GLOSSARY`, yet this function treats it as a position-like vector.
