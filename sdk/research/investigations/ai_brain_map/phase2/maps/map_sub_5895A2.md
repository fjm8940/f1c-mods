# map_sub_5895A2 — `sub_5895A2`

## Identity

- Address: `0x5895A2`; file `functions/sub_5895A2_005895a2.c`; 42 lines.
- Signature: `double __cdecl sub_5895A2(double a1)`.
- Callers (grep `functions/` for `sub_5895A2(`; 33 call sites): `AerodynamicForceCalc` L23;
  `ComplexMemoryAllocationWithFloatComputation` L83; `CubicHermiteInterpolation` L9; `DynMan_Init` L62, L74;
  `F_1DTableLookupWithLerp` L12; `MainGameTickUpdate` L21; `MemoryAllocation_53ec` L28; `sub_4B9B80` L30, L34;
  `sub_4E51A0` L67; `sub_4F9480` L210, L222; `SuspensionWheelDynamicsSolver` L307, L326, L344, L362;
  `TimerConversionWithSin` L5; `TireBrushModelForceSolver` L349, L369; `TireForceSolver` L62, L102, L125,
  L154, L177; `TireForce` L40, L61, L90; `TireGripLateralForceCalculation` L58, L79, L126, L133, L145.
- Callees (from `offsets_sub_5895A2.csv` CALL rows): `ReturnFPUControlWord` L8, L16, L39; `HIWORD` L9;
  `UtilityFunction_1d2e` L11; `LODWORD` / `HIDWORD` L11; `MathDispatchWithErrorFallback` L22;
  `ConditionalMathWithDAT005e9ad0Flag` L27; `SLODWORD` / `SHIDWORD` L30, L31; `COERCE_UNSIGNED_INT64` L32, L33;
  `FrndMSVCCRTRounding` L38.

## Inputs

None. The function takes a scalar `double` argument and reads no memory offsets (only globals
`dword_5E65A0`, `dword_5E9AD0`-class state).

## Outputs

None. The function returns a `double`; the FPU control word is restored before every return.

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/sub_5895A2_005895a2.c`.

| value | lines | note |
|---|---|---|
| 65535 (`0xFFFF`) | L8, L16, L39 | FPU control-word mask passed to `ReturnFPUControlWord` |
| 32752 (`0x7FF0`) | L9 | high-word exponent mask; `== 32752` means Inf/NaN |
| 2 | L14 | classifier band `v3 <= 2` |
| 3 | L19 | classifier result `v3 == 3` (overflow/unsupported case) |
| 8, 11 | L27, L28 | dispatch codes for `ConditionalMathWithDAT005e9ad0Flag` |
| 11 | L22 | dispatch code for `MathDispatchWithErrorFallback` |
| 1.0 | L32 | `a1 + 1.0` argument in the fallback call |
| 0, 1 | L12 (`v3 > 0`), L14 | classifier early-outs |

The only integer bit-pattern constants are `0xFFFF` (FPU CW mask) and `0x7FF0` (exponent mask). Decoded with
`struct.unpack('<f', struct.pack('<I', N))` they are not meaningful floats; no float field receives an integer
bit pattern here, so no bit-pattern decode table is needed.

## Logic

NOTE: the two mask constants are written in decimal (65535 and 32752) because the cited source lines carry only
the hex tokens, which the checker cannot map back to decimal.

```
result = a1                                                        [L7]
v2 = ReturnFPUControlWord(dword_5E65A0, 65535)                     [L8]
if ((HIWORD(a1) & 32752) == 32752) {                               [L9]
  v3 = UtilityFunction_1d2e(LODWORD(a1), HIDWORD(a1))              [L11]
  if (v3 > 0) {                                                    [L12]
    if (v3 <= 2) { ReturnFPUControlWord(v2, 65535); return a1 }    [L14,L15,L16,L17]
    if (v3 == 3) { result = a1; MathDispatchWithErrorFallback(11, a1, v2); return result }   [L19,L21,L22,L23,L24]
  }
  result = a1                                                      [L26]
  ConditionalMathWithDAT005e9ad0Flag(8, 11, SLODWORD(a1), SHIDWORD(a1),
                                     COERCE(a1 + 1.0), ..., v2)    [L27,L28,L30,L31,L32,L33,L34]
} else {
  FrndMSVCCRTRounding(a1)                                          [L38]
  ReturnFPUControlWord(v2, 65535)                                  [L39]
}
return result                                                      [L41]
```

## Meaning (GUESS)

- GUESS This is an MSVC CRT floating-point rounding helper (`floor`/`_frnd` family): save the FPU control word,
  run `FrndMSVCCRTRounding` (frndint) for normal values, restore the control word, and handle Inf/NaN through a
  classifier plus error dispatch [L7-L41].
- GUESS NaN/Inf detection is the `(HIWORD(a1) & 0x7FF0) == 0x7FF0` exponent test; the classifier
  `UtilityFunction_1d2e` returns 1..2 for values returned unchanged and 3 for the case routed to
  `MathDispatchWithErrorFallback(11, ...)` [L9-L24].
- GUESS Callers are physics/tire math that floors forces, times and interpolants; `TimerConversionWithSin` and
  `MainGameTickUpdate` pass scaled times (`a1 * 400.0`) [callers].
- GUESS The `a1 + 1.0` in the fallback dispatch is the CRT "return x large" overflow path of `_floor`/`_ceil`
  [L32].

## Questions

- Which exact CRT function is it (`floor`, `ceil`, `_frnd`, `_ftol`)? The `+1.0` fallback and the two
  classifier bands should identify it against MSVC CRT source.
- `result = a1` is assigned before `FrndMSVCCRTRounding(a1)` and returned unchanged; the actual rounded value
  lives in `st0` (decompiler artifact). Confirm.
- What are `dword_5E65A0` (initial FPU control word) and `DAT_005e9ad0` (flag used by
  `ConditionalMathWithDAT005e9ad0Flag`)?

## Claude review (dropped ST0, 2026-10-02)

Added by DeepSeek C9 per `REVIEW_11_C8.md` + `phase2/DROPPED_ST0_SCAN.md`; no existing tier is changed.

- Inside this CRT rounding helper (`_frnd`), the `FrndMSVCCRTRounding` call at 0x589628 drops its return in the
  pseudo-code (`result = a1` is the input); the asm stores the rounded return (`fstp qword [ebp-8]`) and compares /
  returns that value. Callers should read the return as the rounded input.
