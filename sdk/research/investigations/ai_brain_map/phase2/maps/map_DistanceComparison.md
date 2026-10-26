# map_DistanceComparison — declared `sub_4EFFD0`

## Identity

- Address: `0x4EFFD0`; file `functions/DistanceComparison_004effd0.c`; 13 lines.
- Signature: `bool __thiscall sub_4EFFD0(float *this)`.
- Callers (grep `functions/` for `DistanceComparison(`): `AISteeringRacingLineCorrection` (`0x42B0E0`)
  L110 (`if ((unsigned __int8)DistanceComparison(this + 3))`) - manifest caller; also
  `LoopOverTwoParallelArrays` L16 (`v8[v1 + 12] = DistanceComparison(v3 - 4647);`),
  `sub_4F1FE0` L109 (`!DistanceComparison((float *)this)`),
  `VelocityThresholdCheck` L10 (`return v2 >= 2 && v2 <= 4 && (!(v4 | v5) || !DistanceComparison((float *)this));`).
- Callees (from `offsets_DistanceComparison.csv` CALL rows): `F_3DTransformWithMatrixMultiplication`
  (L10), `RenderCallWithGuard` (L11).
- `this` = vehicle/object (callers pass the vehicle, `this + 3` where `this` is int, or a shifted pointer
  `v3 - 4647`).

## Inputs

None in the CSV. All state consumed goes through the two callees (`this` is forwarded, and
`*(_DWORD **)this` is passed to `RenderCallWithGuard`).

## Outputs

None. Returns a bool (via the x87 compare flags; see Logic).

## Constants

None (no numeric literal except 0/1).

## Logic

```
F_3DTransformWithMatrixMultiplication(this, &v6, v8)          [L10]
RenderCallWithGuard(*(_DWORD **)this, &v7, v9)                [L11]
return (v3 | v4) != 0                                         [L12]
```

`v3` (`c0`) and `v4` (`c2`) are x87 compare-flag locals; the decompiler lost the actual comparison, so the
return value is a flag artifact.

## Meaning (GUESS)

- GUESS Compares two distances/positions: transforms `this` into locals `v6`/`v8` via
  `F_3DTransformWithMatrixMultiplication` and the object pointed to by `*this` into `v7`/`v9` via
  `RenderCallWithGuard`, then returns the result of the floating-point comparison captured in the x87
  flags [L10-L12].
- GUESS Callers use the bool as a proximity/overlap predicate: `AISteeringRacingLineCorrection` L110 gates
  on it, `VelocityThresholdCheck` L10 returns true only when `DistanceComparison` is false in the 2..4
  speed band, and `LoopOverTwoParallelArrays` L16 stores it into an array [callers].
- GUESS `RenderCallWithGuard` receiving `*(_DWORD **)this` suggests the second operand is another object
  pointer stored at `this[0]` (e.g. a paired car/waypoint), so the comparison is between transformed
  positions of two objects [L11].
- GUESS The manifest name "DistanceComparison" is a phase-1 GUESS; the true predicate (distance vs
  threshold, along-track order, or plane side) is not recoverable from this decompile [L12].

## Questions

- `v3`/`v4` are undefined in the decompile (x87 `c0`/`c2` flags); the real comparison operands are lost.
  Which two values are compared, and with what epsilon/threshold?
- Why does a distance comparison call `RenderCallWithGuard` (a render-guarded call) with `*(_DWORD **)this`?
  Is the phase-2 name wrong, or is `RenderCallWithGuard` here a misnamed sibling helper?
- Caller `AISteeringRacingLineCorrection` L110 passes `this + 3` (vehicle + 12 bytes) while `sub_4F1FE0`
  and `VelocityThresholdCheck` pass the vehicle directly; are these the same object type?
- `LoopOverTwoParallelArrays` L16 passes `v3 - 4647` (a dword-scaled pointer subtraction); which object
  base is intended?
