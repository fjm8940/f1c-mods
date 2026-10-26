# map_TLSValueRetrievalWithInit - `TLSValueRetrievalWithInit` (GUESS name)

## Identity

- Address: `0x0058E0C7`; file `functions/TLSValueRetrievalWithInit_0058e0c7.c`; 28 lines.
- Signature: `_DWORD *sub_58E0C7()`.
- Callers (read-only corpus cache): `FontGlyphRender`, `RandLCGRandomNumberGenerator`, `SEHExceptionHandlerTrampolineSetup`, `SEHWrapperAroundCRTFunctionCall`, `SEHWrapperDispatchingToHandlerFunction`, `TLSFieldSetter`, `TLSOffsetGetter`, `TLSOffsetGetter_afd8`, `sub_5897CA`, `sub_592787`, `sub_592924`, `sub_5934E1`, `sub_597388`.
- Callees (from `offsets_TLSValueRetrievalWithInit.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L18 | `Value[1] = -1;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       LastError = GetLastError();   [L8]
       Value = TlsGetValue(dwTlsIndex);   [L9]
       if ( !Value )   [L10]
         v2 = sub_58AB58(1, 116);   [L12]
         if ( v2 && TlsSetValue(dwTlsIndex, v2) )   [L14]
           TwoFieldInitializer_e0b4((int)Value);   [L16]
           Value[1] = -1;   [L18]
           *Value = CurrentThreadId;   [L19]
       SetLastError(LastError);   [L26]
       return Value;   [L27]
```

## Meaning (GUESS)

- GUESS CRT TLS get-or-create for the per-thread error struct (TlsGetValue/TlsSetValue with fallback).
- GUESS AI relevance: none -- CRT runtime.

## Questions

- None for this triage map; name and meaning stay GUESS.
