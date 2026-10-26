# map_AerodynamicForceCalc — section-4 name `AerodynamicForceCalc`, declared `sub_4E8B80`

## Identity

- Address: `0x4E8B80`; file `functions/AerodynamicForceCalc_004e8b80.c`; 62 lines.
- Signature: `double __thiscall sub_4E8B80(float *this, float a2, float a3, float a4)`.
- Callers: `AIRacingLine` (`0x425920`); grep `functions/` for `AerodynamicForceCalc(` ->
  `functions/AIRacingLine_00425920.c` L79/L189; `inspect` agrees.
- Callees (from `offsets_AerodynamicForceCalc.csv` CALL rows): `sub_5895A2` (raw name), `sqrt`.
- `this` is a `float *` vehicle parameter; field accesses are `this[N]` (element size 4).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x3514 | 13588 | float | L21 (this[3397]) | ? |
| 0x3518 | 13592 | float | L41 (this[3398]) | ? |
| 0x3A70 | 14960 | float | L21 (this[3740]) | ? |
| 0x3BFC | 15356 | float | L21 (this[3839]) | ? |
| 0x3C00 | 15360 | float | L34 (this[3840]) | ? lookup table base |
| 0x3C04 | 15364 | float | L35 (this[3841]) | ? lookup table next row |
| 0x3D3C | 15676 | float | L42 (this[3919]) | ? cap |
| 0x3D44 | 15684 | float | L44 (this[3921]) | ? final-term coefficient |

## Outputs

None. The function returns a `double`; no `this` field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/AerodynamicForceCalc_004e8b80.c`.

| value | lines | note |
|---|---|---|
| 13 | L31, L32 | table index clamp |
| 14 | L29 | table size |
| 3397 | L21 | `this[3397]` |
| 3398 | L41, L44 | `this[3398]` |
| 3740 | L21 | `this[3740]` |
| 3839 | L21 | `this[3839]` |
| 3840, 3841 | L34, L35, L39 | lookup table |
| 3919 | L42, L43 | cap |
| 3921 | L44 | coefficient |
| 0.0 | L21, L45, L49, L52, L60 | comparisons |
| 0.5 | L47 | negative half factor |
| 4.0 | L48 | quadratic term |
| 9.8100004 | L21 | gravity constant |
| 1.0 | L54 | return threshold (written as 1.0) |

## Logic

NOTE: source uses scaled `this[N]` indexing (Hex-Rays prints element index for `float *this`). The Logic keeps the
source-literal index; byte offsets are in the tables above. No hex tokens are used on scaled lines.

```
v4 = a2 * a2; v19 = v4; v14 = v4                                 [L18,L19,L20]
v6 = (v4*this[3740] - (v14*0.0 - 9.8100004)*this[3397]) * this[3839]   [L21]
v18 = v6; sub_5895A2(v6); v16 = v6; v7 = (int)v16; v17 = v7      [L22,L23,L24,L25,L26]
if (v7 >= 0)                                                     [L27]
    if (v7 >= 14) v7 = 13                                        [L29,L31]
    v9 = this[v7 + 3840]                                         [L34]
    v8 = v9 + (this[v7 + 3841] - v9) * (v18 - v17)               [L35]
else
    v8 = this[3840]                                              [L39]
v10 = v8 * this[3398]                                            [L41]
if (v10 > this[3919]) v10 = this[3919]                           [L42,L43]
v15 = this[3921]*this[3398]*v14 + v10                            [L44]
if (v15 > 0.0)                                                   [L45]
    v12 = v15 * -0.5                                             [L47]
    v13 = v19 - -a3 * v12 * 4.0                                  [L48]
    if (v13 > 0.0)                                               [L49]
        return a2 - (sqrt(v13) - a2) / (v12 + v12) * v15 * a4    [L50]
    else
        return 0.0                                               [L52]
else if (a2 >= 1.0)                                              [L54]
    return a2                                                    [L56]
else
    return 0.0                                                   [L60]
```

## Meaning (GUESS)

- GUESS Computes an aerodynamic force as a function of speed input `a2` and calibration fields
  this[3740]/this[3397]/this[3839], with a table lookup over this[3840]/this[3841] [L21-L44].
- GUESS `a3` and `a4` are a distance/deformation scale and a blend factor in the quadratic root formula
  `a2 - (sqrt(v13)-a2)/(2*v12)*v15*a4` [L48-L50].
- GUESS `v15` is a drag/force magnitude; if non-positive it returns `a2` (>=1.0) or 0.0, i.e. no retardation [L45-L60].
- GUESS The `- 9.8100004` term is gravity compensation in the load term [L21].
- GUESS Same lookup/interpolation pattern as `TireForce`/`TireForceSolver` (this[3839..3841]) — a shared
  aero-force table family [L21-L43].

## Questions

- `a3`, `a4` semantics unknown; caller `AIRacingLine` passes `v45` (distance * 0.4) and a constant 0.75/0.69999999.
- `sub_5895A2` unknown (same helper as TireForce/TireForceSolver).
- Field names all `?`; `this[3740]`, `this[3397]`, `this[3839]` also appear in TireForce — shared car parameter block.
- Return semantics: does `a2` represent a speed that is reduced, or a force ratio? Needs caller context review.

## Claude review (dropped ST0, 2026-10-02)

Added by DeepSeek C9 per `REVIEW_11_C8.md` + `phase2/DROPPED_ST0_SCAN.md`; no existing tier is changed.

- The `sub_5895A2` (`_frnd`) call at 0x4E8BC4 drops its return in the pseudo-code; the asm stores the rounded value and
  uses it as the table index.
- The return is used by callers: AIRacingLine 0x425A19/0x425C8C compares it with 0.0 in a branch, so the Questions
  bullet on return semantics can be closed as "force-valued return consumed by the caller" (review action, not a re-tier).
