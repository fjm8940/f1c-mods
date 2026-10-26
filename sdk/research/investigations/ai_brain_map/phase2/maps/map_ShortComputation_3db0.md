# map_ShortComputation_3db0 — declared `sub_423DB0`

## Identity

- Address: `0x423DB0`; file `functions/ShortComputation_3db0_00423db0.c`; 42 lines.
- Signature: `void __thiscall sub_423DB0(float *this, float a2, int a3, float a4, float a5, int a6, float a7)`.
  `a3` and `a6` are never used in the body.
- Callers (grep `functions/` for `ShortComputation_3db0(`): `AISteering` (`0x4270A0`) L332
  (`ShortComputation_3db0(v14, this[4387], this[4388], this[4389], this[3384], this[3385], this[3386])`;
  `v14` is the per-opponent slot, `this[4387]/[4389]` = heading -sin/-cos (0x448C/0x4494),
  `this[3384]/[3386]` = Euler orientation X/Z 0x34E0/0x34E8).
- Callees (from `offsets_ShortComputation_3db0.csv` CALL rows): `sqrt` (CRT; rows L28, L30, L38, L40).
- `this` = per-opponent slot whose first dword is a vehicle pointer; `this[3]` = byte 0xC,
  `this[1]` = byte 0x4. The vehicle pointer `veh = *(_DWORD *)this` is the runtime base for 0x34E0/0x34E8.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x34E0 | 13536 | float | L21 (`*(float *)(*(_DWORD *)this + 13536) - a5`) | Euler orientation X (doc 54 / FIELD_GLOSSARY) |
| 0x34E8 | 13544 | float | L22 (`*(float *)(*(_DWORD *)this + 13544) - a7`) | Euler orientation Z (doc 54 / FIELD_GLOSSARY) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0xC | 12 | float | L31 (`this[3] = v14`) | ? slot output 1 (signed projected angle delta) |
| 0x4 | 4 | float | L41 (`this[1] = v20`) | ? slot output 2 (signed perpendicular angle delta) |

## Constants

| value | lines | note |
|---|---|---|
| 13536, 13544 | L21, L22 | vehicle byte offsets (Euler X / Z) |
| 3 | L31 | `this[N]` index (byte 0xC) |
| 1 | L41 | `this[N]` index (byte 0x4) |

## Logic

```
veh = *(_DWORD *)this                                        [L21]
v7 = *(float *)(veh + 0x34E0) - a5                           [L21]
v8 = *(float *)(veh + 0x34E8) - a7                           [L22]
v9 = a4 * v8 + a2 * v7                                       [L23]
v21 = a2 * v9                                                [L24]
v24 = a4 * v9                                                [L25]
v11 = v24 * v24 + v21 * v21                                  [L26]
v14 = (x87 c0|c3 flag) ? -sqrt(v11) : sqrt(v11)              [L27,L28,L29,L30]
this[3] = v14                                                [L31]
v22 = -a4                                                    [L32]
v15 = a2 * v8 - a4 * v7                                      [L33]
v23 = v22 * v15                                              [L34]
v25 = a2 * v15                                               [L35]
v17 = v25 * v25 + v23 * v23                                  [L36]
v20 = (x87 c0|c3 flag) ? sqrt(v17) : -sqrt(v17)              [L37,L38,L39,L40]
this[1] = v20                                                [L41]
```

## Meaning (GUESS)

- GUESS Same decomposition pattern as `ShortComputation_3ca0` but on the Euler angle deltas instead of
  velocity: `v7/v8` = difference between the opponent vehicle's Euler orientation (0x34E0/0x34E8) and the
  caller's values (a5/a7), projected onto the caller's heading vector (a2, a4) [L21-L26].
- GUESS Output 1 (`this[3]`) is the signed magnitude of the projection `(a2*v9, a4*v9)`;
  output 2 (`this[1]`) is the signed magnitude of the perpendicular `(-a4*v15, a2*v15)` where
  `v15 = cross((a2,a4), (v7,v8))` [L23-L41].
- GUESS Caller AISteering compares these to `v14[3]` (L345) and other slot fields to judge the opponent's
  relative orientation while overtaking [caller L332-L345].
- GUESS The x87 `c0|c3` sign branches select the sign from a hidden compare; the exact predicate is lost
  in the decompile [L27-L30, L37-L40].

## Questions

- What is the x87 predicate behind `if (v12 | v13)` / `if (v18 | v19)`? (Same pattern as
  ShortComputation_3ca0.)
- Why are `a3` and `a6` (arguments 3 and 6, `this[4388]` / `this[3385]`) unused? ABI padding or legacy
  signature?
- What are the units of `this[3]` / `this[1]` (radians, degrees, normalized) and who reads them?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
