# map_RandomVariationScaler - `RandomVariationScaler` (GUESS name)

## Identity

- Address: `0x004BEA10`; file `functions/RandomVariationScaler_004bea10.c`; 32 lines.
- Signature: `double __fastcall sub_4BEA10(int a1, int a2, int a3, float a4)`.
- Callers (read-only corpus cache): `AIBehaviorInitialization`, `EngineFailureCheck`.
- Callees (from `offsets_RandomVariationScaler.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x360 | 864 | _DWORD | L8 | `if ( *(_DWORD *)(a1 + 864) == 7 )` |
| 0x44C4 | 17604 | int | L10 | `v7 = *(int *)(a3 + 17604);` |
| 0x44C0 | 17600 | _DWORD | L14 | `a1 = *(_DWORD *)(a3 + 17600);` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( *(_DWORD *)(a1 + 864) == 7 )   [L8]
         v7 = *(int *)(a3 + 17604);   [L10]
         a1 = *(_DWORD *)(a3 + 17600);   [L14]
       v8 = (double)(RandLCGRandomNumberGenerator(a1) % 40) * 0.0099999998 - 0.0099999998 + 1.0 - *(float *)&v7;   [L17]
       if ( v8 <= 1.0 )   [L18]
       v5 = (double)(RandLCGRandomNumberGenerator(v4) % 50) * 0.0099999998 + 0.5;   [L28]
       return 0.25 * a4 * (v5 * v8) - 0.25 + a4;   [L31]
```

## Meaning (GUESS)

- GUESS Random AI variation scaler: two RandLCGRandomNumberGenerator draws (% 40 and % 50) build a jitter factor around a4, offset by the per-car ability fields at +17600/+17604 (0x44C0 / 0x44C4 race_ability_norm).
- GUESS AI relevance: high -- called by AIBehaviorInitialization and EngineFailureCheck; primary input to the D5 randomness map.
- GUESS result is a multiplier near a4: 0.25*a4*(v5*v8) - 0.25 + a4, i.e. roughly a4 * [0.75..1.25] shaped by ability.

## Questions

- None for this triage map; name and meaning stay GUESS.
