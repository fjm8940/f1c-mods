# map_FieldReadDelegate - `FieldReadDelegate` (GUESS name)

## Identity

- Address: `0x004E16C0`; file `functions/FieldReadDelegate_004e16c0.c`; 10 lines.
- Signature: `int __thiscall sub_4E16C0(int this)`.
- Callers (read-only corpus cache): `SuspensionStateUpdate`, `VehicleTelemetryPositionAccumulator`.
- Callees (from `offsets_FieldReadDelegate.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x8 | 8 | _DWORD | L4 | `*(_DWORD *)(this + 8),` |
| 0xC | 12 | _DWORD | L5 | `*(_DWORD *)(this + 12),` |
| 0x10 | 16 | _DWORD | L6 | `*(_DWORD *)(this + 16),` |
| 0x14 | 20 | float | L7 | `*(float *)(this + 20),` |
| 0x18 | 24 | float | L8 | `*(float *)(this + 24),` |
| 0x1C | 28 | float | L9 | `*(float *)(this + 28));` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       return sub_54B4D0(   [L3]
                *(_DWORD *)(this + 8),   [L4]
                *(_DWORD *)(this + 12),   [L5]
                *(_DWORD *)(this + 16),   [L6]
                *(float *)(this + 20),   [L7]
                *(float *)(this + 24),   [L8]
                *(float *)(this + 28));   [L9]
```

## Meaning (GUESS)

- GUESS Delegates to sub_54B4D0 with six fields read from this+8, +12, +16, +20, +24, +28.
- GUESS AI relevance: none-low -- scene-node field push (called by suspension and telemetry updaters).

## Questions

- None for this triage map; name and meaning stay GUESS.
