# map_AIComputePassScore — declared `sub_42A780` (Claude review: waypoint corner-speed limit, not a pass score)

## Identity

- Address: `0x42A780`; file `functions/AIComputePassScore_0042a780.c`; 5 lines.
- Signature: `double __userpurge sub_42A780(float *a1@<ecx>, double a2@<st0>, float *a3)`.
- Callers: `AIRacingLine` (`0x425920`) L205, L209, L259; grep `functions/` for `AIComputePassScore(` ->
  `functions/AIRacingLine_00425920.c` only; `inspect` agrees.
- Callees (from `offsets_AIComputePassScore.csv` CALL rows): `TireForceSolver`.
- Argument roles (Claude review item 1): `a1` = vehicle, `a3` = waypoint. The first explicit C argument shown at the
  call sites is the waypoint; `a1`/`a2` arrive implicitly (ecx / st0, `__userpurge`).
- This map is a REVIEW_2_GROUP1 addition to the group-1 callees; it does not re-tier anything.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4504 | 17668 | float | L4 (`a1[4417]`, base `a1`) | ? (force-to-speed coefficient) |
| 0x4500 | 17664 | float | L4 (`a1[4416]`, base `a1`) | ? (force-to-speed offset) |
| 0x4580 | 17792 | float | L4 (`a1[4448]`, base `a1`) | ? (scale) |
| 0x453C | 17724 | float | L4 (`a1[4431]`, base `a1`) | AIAbility dynamic multiplier (prior art) |
| 0x84 | 132 | float | L4 (`a3[33]`, base `a3`) | ? (per-waypoint factor) |
| 0x74 | 116 | float | L3 (`a3[29]`, base `a3`) | ? (TireForceSolver slip/load input) |
| 0x70 | 112 | float | L3 (`a3[28]`, base `a3`) | ? (TireForceSolver slip/load input) |

`a1` fields are byte offsets on the vehicle; `a3` fields are byte offsets on the waypoint. `(a3 + 30)` L3 is an address
argument (waypoint block at +120 bytes), not a scalar field.

## Outputs

None directly. The function returns a `double` (corner-speed value); no field is written.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/AIComputePassScore_0042a780.c`.

| value | lines | note |
|---|---|---|
| 16.0 | L3 | TireForceSolver force cap argument |
| 29, 30, 28 | L3 | waypoint float indices / address offset |
| 33 | L4 | waypoint field index |
| 4417, 4416, 4448, 4431 | L4 | vehicle float indices |

## Logic

NOTE: source uses scaled `a1[N]`/`a3[N]` indexing (Hex-Rays prints element index for `float *`). The Logic keeps the
source-literal index; byte offsets are in the tables above. No hex tokens are used on scaled lines.

```
TFS = TireForceSolver(wp[29], wp+30 (address), wp[28], 16.0)   [L3]
return (TFS * veh[4417] + veh[4416]) * veh[4448] * veh[4431] * wp[33]   [L4]
```

## Meaning (GUESS)

- GUESS Computes one scalar from a tire force and vehicle/waypoint coefficients; the value is consumed by
  `AIRacingLine` as a per-waypoint speed target [L3,L4].
- GUESS The force base comes from `TireForceSolver` over waypoint fields (wp[29], wp[28], pointer wp+30) with force
  cap 16.0, the same waypoint block AIRacingLine passes to `TireForce` at its L263 [L3].
- GUESS vehicle[4417], vehicle[4416] form a linear force-to-speed term; vehicle[4448] and waypoint[33] scale it
  [L4].
- GUESS vehicle[4431] (0x453C) is the AIAbility dynamic pace multiplier (prior art), so the result tracks per-tick
  AI pace [L4; caller L209].
- GUESS Caller use: compared against 1.05 x current speed (0x4440) at L205 to test whether the waypoint chain is
  followable, and stored as throttle/score context at L209/L259 [caller lines].

## Claude review notes (2026-10-02, DERIVED — do not re-tier)

Source: `REVIEW_2_GROUP1.md` item 1, from raw source.

- This function is NOT a pass score. It is the **waypoint corner-speed limit** (m/s):
  `v = (TireForceSolver(wp) * veh[0x4504] + veh[0x4500]) * veh[0x4580] * veh[0x453C] * wp[+0x84]`.
- Float-index mapping: `a1[4417]` = 0x4504, `a1[4416]` = 0x4500, `a1[4448]` = 0x4580, `a1[4431]` = 0x453C,
  `a3[33]` = wp+0x84.
- Same shape as `sub_426430`'s speed ratio (0x4504/0x4500, cap 75). 0x453C = AIAbility dynamic multiplier (prior art).

## Questions

- `__userpurge` argument order: which caller instructions load ecx with the vehicle and st0 with the force return;
  the decompiled call sites show only the waypoint argument.
- Fields 0x4500, 0x4504, 0x4580 and wp+0x84 are unnamed; 0x4500/0x4504 also appear in `sub_426430`.
- Are wp+0x70/wp+0x74 the same waypoint fields passed to `TireForce` in AIRacingLine L263 (`*(v5+112)`,
  `*(v5+116)`)?

## Claude review (dropped ST0, 2026-10-02)

Added by DeepSeek C9 per `REVIEW_11_C8.md` + `phase2/DROPPED_ST0_SCAN.md`; no existing tier is changed.

- The return of `TireForceSolver` at 0x42A79C is consumed immediately (`fmul [esi+0x4504]`); the `double a2@<st0>`
  parameter shown by Hex-Rays is that return, not a caller input. The arithmetic is otherwise as written.
