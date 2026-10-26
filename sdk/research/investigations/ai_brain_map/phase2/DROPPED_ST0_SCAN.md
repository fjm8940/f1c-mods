# DROPPED_ST0_SCAN — C8.4: mapped AI functions with x87 returns dropped by Hex-Rays (2026-10-02)

Method: `analysis/c8_sto_scan.py` disassembles every function address in the phase-1 +
phase-2 map set (78 maps) with capstone, bounded by the next known function start from
`functions/*.c`; for each direct `call` to a corpus function whose prototype returns
`float`/`double`/`long double`, it checks whether an x87 instruction that consumes ST0
(arithmetic/store) follows before the next call/jump/ret, and whether the caller's
Hex-Rays text shows the call as a standalone statement. Hits were then manually
classified against the disassembly. Read-only; no scripts of Claude were modified.

## Confirmed dropped-return sites

| Function (map) | call | callee | asm evidence | what the pseudo-code says | wrong variable |
|---|---|---|---|---|---|
| AIRacingLine | 0x425ED9 | `TireForce` 0x4E9100 (double) | `fmul st(1)` at 0x425EFC (st0 = TireForce return) | `AIRacingLine_00425920.c` L263 standalone call; L264 multiplies by `v32` | the third factor in L264 is the TireForce return, not `v32` (the target; finding `ai-brain-braking-step-tireforce-correction-2026-10-02`, REVIEW_10) |
| AIComputePassScore | 0x42A79C | `TireForceSolver` 0x4E8CE0 (double) | `fmul [esi+0x4504]` immediately after the call | C models the return as a phantom parameter `double a2@<st0>` and returns `(a2*...)` | none functionally wrong, but the signature is misleading: `a2` is the return of the call, not an input |
| AIRacingLine | 0x425A19 | `AerodynamicForceCalc` 0x4E8B80 (double) | `fcomp [0x59e440]` (0.0) directly after the call | L79 standalone; L80 tests `v8` (an argument, 0.4*dist) | the `v8 > 0.0` condition is the call return |
| AIRacingLine | 0x425C8C | `AerodynamicForceCalc` 0x4E8B80 (double) | `fcomp [0x59e440]` directly after the call | L189 standalone; L190 tests `v23*0.3` (an argument) | the `v23*0.3 <= 0.0` condition is the call return |
| TireForce | 0x4E916B | `sub_5895A2` `_frnd` 0x5895A2 (double) | `fstp dword [esp+0x24]` stores the return | `sub_5895A2(v12); v31 = v12;` then `(int)v31` indexes the table | `v31` should be the rounded return, not the raw `v12` |
| TireForce | 0x4E91BE | `_frnd` | same | same (`v33 = v17` raw) | same class |
| TireForce | 0x4E924D | `_frnd` | `fstp dword [esp+0x20]` stores the return | same (`v29 = v20` raw) | same class |
| TireForceSolver | 0x4E8D02/0x4E8DF7/0x4E8E63/0x4E8F11/0x4E8F7D | `_frnd` | store of the return after each call | `sub_5895A2(v6); v59 = v6;` etc. | the interpolated index uses the raw value in the pseudo-code |
| AerodynamicForceCalc | 0x4E8BC4 | `_frnd` | store of the return | `sub_5895A2(v6); ...` | same class |
| sub_5895A2 (CRT helper) | 0x589628 | `FrndMSVCCRTRounding` 0x591CF3 (double) | `fstp qword [ebp-8]` then reload/compare | `FrndMSVCCRTRounding(a1); ... return result;` with `result = a1` | the `_frnd` result path returns the rounded value, not the input |

## Checked and NOT a problem (scanner false positives)

- `OpponentProximityScanTeamRecognition` 0x42A966 -> `F_2DVectorDirectionProjection`:
  the C uses the return in `>= -5.5`; the scanner only looked for `= callee(`.
- `AIRacingLinePathFollowing` 0x4E9EFE -> `VehicleBodyToWheelContactDistanceComputation`:
  asm does `fstp st(0)` immediately, i.e. the return is deliberately discarded; the
  standalone C statement is correct.

## Consequences

- The `_frnd` class is systematic: every consumer of `sub_5895A2` computes its table
  index from the raw value in the pseudo-code, while the game uses the rounded value.
  The phase-2 maps for `TireForce`, `TireForceSolver` and `AerodynamicForceCalc` inherit
  this and should carry a correction note; the "return unused in every call site"
  statement in `map_TireForce.md` Questions is disproved (the return is stored after
  every call).
- The two AIRacingLine `AerodynamicForceCalc` conditions (L80, L190) change the
  approach-block interpretation: the branch depends on the aero force, not on the
  distance-derived argument.
- No ABI field offsets change; only the meaning of the affected expressions.
