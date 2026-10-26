# map_sub_4BC060 - `sub_4BC060` (GUESS name)

## Identity

- Address: `0x004BC060`; file `functions/sub_4BC060_004bc060.c`; 27 lines.
- Signature: `int __thiscall sub_4BC060(int this, char a2)`.
- Callers (read-only corpus cache): `PitStopDecisionTireCompound`, `sub_4A0250`, `sub_4F1FE0`.
- Callees (from `offsets_sub_4BC060.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L18 | `result = ProgressBarEventEncode((_DWORD *)v3, 4, 26, 1, *(_DWORD *)(th` |
| 0x34 | 52 | _BYTE | L19 | `*(_BYTE *)(v3 + 8) = *(_BYTE *)(this + 52);` |
| 0x4 | 4 | _DWORD | L23 | `return ProgressBarEventEncode((_DWORD *)v3, 4, 27, 0, *(_DWORD *)(this` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x8 | 8 | _BYTE | L19 | `*(_BYTE *)(v3 + 8) = *(_BYTE *)(this + 52);` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( dword_7C56A4 == 7 )   [L7]
         v3 = dword_8AAE64;   [L9]
         {   [L11]
           dword_8AAE64 += 432;   [L13]
         *(float *)&v5 = dbl_8AFEA0;   [L15]
           result = ProgressBarEventEncode((_DWORD *)v3, 4, 26, 1, *(_DWORD *)(this + 4), v5, 0);   [L18]
           *(_BYTE *)(v3 + 8) = *(_BYTE *)(this + 52);   [L19]
           return ProgressBarEventEncode((_DWORD *)v3, 4, 27, 0, *(_DWORD *)(this + 4), v5, 0);   [L23]
```

## Meaning (GUESS)

- GUESS Emits a progress-bar/session event (ProgressBarEventEncode 4,26/27) when session type is 7 and copies a byte from this+52.
- GUESS AI relevance: low -- UI/session event path reached from pit and race functions.

## Questions

- None for this triage map; name and meaning stay GUESS.
