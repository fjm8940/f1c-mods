# map_LoopWithConditionalSubCall — `sub_4F1A60` (`LoopWithConditionalSubCall`)

## Identity

- Address: `0x4F1A60`; file `functions/LoopWithConditionalSubCall_004f1a60.c`; 9 lines.
- Signature: `void __thiscall sub_4F1A60(void *this, int a2, char a3)`.
- Callers (grep `functions/` for `LoopWithConditionalSubCall(`): `AIMainUpdate` (`0x424E70`) L296;
  `AISteeringRacingLineCorrection` (`0x42B0E0`) L126; `PitStopOperationTimer` (`0x4A6340`) L194
  (`LoopWithConditionalSubCall(v28, 1);`); `sub_429960` (`0x429960`) L264; `SuspensionAndWheelPhysics`
  (`0x4E5F60`) L425 (`LoopWithConditionalSubCall(*(_DWORD *)(a2 + 3012), 0);`).
- Callees (from `offsets_LoopWithConditionalSubCall.csv` CALL rows): `sub_4F18C0` L6;
  `TireBrushModelForceSolver` L8.

## Inputs

None at the field level. Inputs are the arguments `this`, `a2`, `a3`; the loop variable `i` is local.

## Outputs

None directly. Any state written by `sub_4F18C0` / `TireBrushModelForceSolver` is callee-internal.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/LoopWithConditionalSubCall_004f1a60.c`.

| value | lines | note |
|---|---|---|
| 4 | L5 | loop bound (4 iterations) |

## Logic

```
for (i = 0; i < 4; ++i) sub_4F18C0((int)this, i, a2, 0)     [L5,L6]
if (a3) TireBrushModelForceSolver((int)this, 0)             [L7,L8]
```

## Meaning (GUESS)

- GUESS Calls `sub_4F18C0(this, i, a2, 0)` once per wheel/index `i = 0..3`, so four wheels with the same
  `a2` payload and a zero last argument [L5,L6].
- GUESS If `a3` is set, follows with the tire brush force solver for the axle/wheel index 0 [L8].
- GUESS `a2` is a force/torque input shared by all four wheel calls; callers pass 0 or 1
  (`SuspensionAndWheelPhysics` L425 passes 0; others pass 1) [callers].
- GUESS Used from suspension/tire physics paths and from `AIMainUpdate`/`AISteeringRacingLineCorrection`
  when AI hits something (collision response) [callers].
- GUESS The final `0` argument of `sub_4F18C0` and `TireBrushModelForceSolver` may be a wheel/axle id or a
  "first pass" flag [L6,L8].

## Questions

- `sub_4F18C0` is unmapped: no name, no contract (does it write forces per wheel?).
- `TireBrushModelForceSolver` receives `((int)this, 0)` — is the `0` the same selector as in
  `LoopWithConditionalSubCall(..., 0)` at `SuspensionAndWheelPhysics` L425?
- `a2` semantics: callers pass 0 or 1; is it a mode (rebuild/solve) or a delta-time flag?
- The function name is descriptive only; the `if (a3)` gate's purpose (e.g. skipping the solver during
  AI updates) should be confirmed from callers.
