# map_PositionUpdateWithTelemetryRecording - `PositionUpdateWithTelemetryRecording` (GUESS name)

## Identity

- Address: `0x004EF4F0`; file `functions/PositionUpdateWithTelemetryRecording_004ef4f0.c`; 20 lines.
- Signature: `void __thiscall sub_4EF4F0(int this)`.
- Callers (read-only corpus cache): `Gearbox`, `RaceStartCountdownStateUpdate`.
- Callees (from `offsets_PositionUpdateWithTelemetryRecording.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4014 | 16404 | float | L5 | `if ( dbl_8AFEA0 > *(float *)(this + 16404) + 4.5 )` |
| 0x4 | 4 | _DWORD | L18 | `ProgressBarEventEncode(v1, 2, 0, 0, *(_DWORD *)(this + 4), *(_DWORD *)` |
| 0x34AC | 13484 | _DWORD | L18 | `ProgressBarEventEncode(v1, 2, 0, 0, *(_DWORD *)(this + 4), *(_DWORD *)` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4014 | 16404 | float | L7 | `*(float *)(this + 16404) = dbl_8AFEA0;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( dbl_8AFEA0 > *(float *)(this + 16404) + 4.5 )   [L5]
       {   [L6]
         *(float *)(this + 16404) = dbl_8AFEA0;   [L7]
         ProgressBarEventEncode(v1, 2, 0, 0, *(_DWORD *)(this + 4), *(_DWORD *)(this + 13484), 0);   [L18]
```

## Meaning (GUESS)

- GUESS Emits a telemetry/progress event (ProgressBarEventEncode type 2) at most every 4.5 s and stamps this+16404.
- GUESS AI relevance: low-medium -- telemetry recording called by Gearbox and RaceStartCountdownStateUpdate.

## Questions

- None for this triage map; name and meaning stay GUESS.
