# map_F_2DVectorDirectionProjection — `sub_423AD0` (`F_2DVectorDirectionProjection`)

## Identity

- Address: `0x423AD0`; file `functions/F_2DVectorDirectionProjection_00423ad0.c`; 10 lines.
- Signature: `double __fastcall sub_423AD0(float *a1, float *a2)`.
- Callers (grep `functions/` for `F_2DVectorDirectionProjection(`): `OpponentProximityScanTeamRecognition`
  (`0x42A7D0`) L100 (`(float *)this + 4387`, `v42`).
- Callees (from `offsets_F_2DVectorDirectionProjection.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x8 | 8 | float | L5 | ? (`a1[2]`; vector 1 component 2) |
| 0x8 | 8 | float | L9 | ? (`a2[2]`; vector 2 component 2) |

`*a1` (offset 0) is read at L5 and `*a2` (offset 0) is read at L9, but neither is represented in
`offsets_F_2DVectorDirectionProjection.csv` (zero offsets); the unmatched file is empty.

## Outputs

None. The function returns a `double`; no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/F_2DVectorDirectionProjection_00423ad0.c`.

| value | lines | note |
|---|---|---|
| 2 | L5, L9 | `a1[2]`/`a2[2]` index |
| 0.0 | L6 | zero-length early-out |

## Logic

NOTE: source uses scaled `a1[N]`/`a2[N]` indexing; Logic keeps the source-literal index and no hex tokens.
Byte offsets are in the table above.

```
v2 = *a1 * *a1 + a1[2] * a1[2]              [L5]
if (v2 == 0.0) return 0.0                   [L6,L7]
return (a2[2] * a1[2] + *a1 * *a2) / v2     [L9]
```

## Meaning (GUESS)

- GUESS Scalar projection of `a2` onto `a1` in the XZ plane: `dot(a1,a2) / |a1|^2`, returning the fraction
  of `a1` along which `a2` lies [L5,L9].
- GUESS Because the result is normalized by `|a1|^2` rather than `|a1|`, the caller can compare it
  against a distance-like threshold (`>= -5.5` at OpponentProximityScanTeamRecognition L100) to mean
  "opponent is ahead / beside, not far behind" [caller].
- GUESS `a1` is the own-car heading vector: the caller passes `(float *)this + 4387` = byte offset 17548 =
  `0x448C` (heading `-sin(yaw)`, `FIELD_GLOSSARY`), so `a1[2]` = 17556 = `0x4494` (heading `-cos(yaw)`)
  [caller, FIELD_GLOSSARY].
- GUESS `a2` is the opponent offset `(dx, dz)` built at caller L72-L74 from `v7 + 13536` / `v7 + 13544`
  (Euler orientation fields) [caller].
- GUESS Zero guard covers the degenerate "heading is zero" case only; `|a1|^2` is assumed non-zero for a
  unit heading [L6,L7].

## Questions

- Offsets 0 of both vectors are read but absent from the CSV; confirm `a1[0]`/`a2[0]` are used and not an
  extraction miss (the unmatched file is empty).
- Is the return unitless (projection ratio) or metres? The caller compares it to `-5.5` directly.
- The zero-length branch returns `0.0`, which is also a valid projection value — no sentinel distinguishes
  "degenerate input" from a legitimate zero. Does the caller care?
- The function name was assigned before this map; the dot-product form is confirmed, but the 2D plane
  (XZ) should be confirmed against the caller's `(dx, dz)` ordering.
