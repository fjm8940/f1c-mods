# map_CRTFtol — declared `CRTFtol` (`sub_587BDC`)

## Identity

- Address: `0x587BDC`; file `functions/CRTFtol_00587bdc.c`; 4 lines.
- Signature: `__int64 __usercall CRTFtol@<edx:eax>(double a1@<st0>)`.
- Callers (grep `functions/` for `CRTFtol(`): `AIMainUpdate` (`0x424E70`) L112, L114, L203 - manifest
  caller; also `AIRetirementVehicleFailureEvent` L26/L44, `AIWaypointPathCheckerRenderer` L33/L35,
  `AudioMixing` L130-L137, `AudioParameterInterpolation`, `AudioSampleCrossFadeInterpolation`,
  `CameraViewportRenderSetup`, `DriverInputController`, `F_2DTextRenderingWithScissorRectangle`,
  `ScaledTextureRender`, `SignedIndexTableLookup` (`0x4FAE70`) L6 and many other UI/audio/render callers.
- Callees (from `offsets_CRTFtol.csv` CALL rows): none (CSV has only the header row).
- CRT runtime helper: argument arrives in x87 `st0`, result returned in `edx:eax`.

## Inputs

None (only the `double a1` argument in `st0`; no object offsets).

## Outputs

None (return value only: `__int64` in `edx:eax`).

## Constants

None (no numeric literal other than implicit 0 truncation).

## Logic

```
return (__int64)a1                                    [L3]
```

## Meaning (GUESS)

- GUESS CRT `ftol`-style conversion: truncates the x87 double in `st0` to a signed 64-bit integer in
  `edx:eax`; this is the compiler's `_ftol`/`_ftol2` runtime, not game logic [L3].
- GUESS AIMainUpdate uses it to turn floats into table indices: `*((_DWORD *)&unk_5C30E0 - CRTFtol(v18))`
  (L112) and `dword_5C3144[CRTFtol(v18)]` (L114), i.e. array indexing after truncation [callers].
- GUESS Other callers convert time/speed floats to integer milliseconds or pixel units (audio sample
  interpolation, UI rendering, DriverInputController timing) [callers].

## Questions

- Rounding direction is whatever the x87 control word currently specifies (default round-to-nearest) or
  truncation via `fistp` - the C-level cast is truncation, but the CRT helper is usually called only when
  truncation matters; confirm the actual instruction.
- Why is the calling convention `__usercall edx:eax` (custom register pair) rather than standard
  `__stdcall`? Likely a Fastcall/optimized CRT stub; not a game function.
