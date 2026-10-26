# map_AIDifficultyScalar — declared `sub_424750`

## Identity

- Address: `0x424750`; file `functions/AIDifficultyScalar_00424750.c`; 7 lines.
- Signature: `double sub_424750()` (free function - no `this` parameter).
- Callers (grep `functions/` for `AIDifficultyScalar(`): `AIAbility` (`0x429470`) L31, L35, L76;
  `AISteeringCorrection` (`0x426CF0`) L23 (`*(float *)(this + 17788) = AIDifficultyScalar() * 0.75;`);
  `sub_424080` (`0x424080`) L176 (`*((float *)this + 4447) = AIDifficultyScalar();`). Manifest callers
  (AIAbility, AISteeringCorrection) confirmed.
- Callees (from `offsets_AIDifficultyScalar.csv` CALL rows): none (CSV has only the header row).
- No `this`; input is the global `dword_7D01A0` (same global read by `AIRingBufferFieldInit` L8 and
  `AIBehaviorInitialization` L35).

## Inputs

None (no object offsets; only global `dword_7D01A0`).

## Outputs

None.

## Constants

Every numeric literal except 0/1.

| value | lines | note |
|---|---|---|
| 95 | L3 | upper breakpoint: `dword_7D01A0 >= 95` returns 1.0 |
| 70 | L6 | lower breakpoint subtracted from `dword_7D01A0` |
| 0.039999999 | L6 | slope per unit (`0.04`) |
| 0.25 | L6 | quarter scale (combined slope 0.01 per unit) |
| 0.75 | L6 | base offset / minimum scalar |

## Logic

```
if (dword_7D01A0 >= 95) return 1.0                                   [L3,L4]
return (double)(dword_7D01A0 - 70) * 0.039999999 * 0.25 + 0.75       [L6]
```

## Meaning (GUESS)

- GUESS Converts the global AI strength `dword_7D01A0` into a 0.75..1.0 difficulty scalar: value 70
  gives 0.75, 95 or more gives 1.0, slope 0.01 per strength point [L3-L6].
- GUESS Used as the per-driver ability/pace cap: `AIAbility` multiplies it by 0.75 into 0x457C and stores
  it directly into 0x4447; `AISteeringCorrection` writes `*0.75` into 0x457C (`difficulty cap`,
  `FIELD_GLOSSARY.md` 0x457C) [callers].
- GUESS No clamp below 70: strengths under 70 produce values below 0.75 (negative for very low values);
  the 95 cutoff is the only hard clamp [L3,L6].

## Questions

- What writes global `dword_7D01A0`, and what is its valid range? Values below 70 yield scalars below
  0.75 (and below 0.0 under 70 - 0.75/0.01 = negative region), apparently unintended.
- Is `dword_7D01A0` really "AI difficulty strength", or a skill/opponent-level index shared with
  `AIRingBufferFieldInit` (which scales `0.0099999998 * 0.1`) and `AIBehaviorInitialization` (`1/120`)?
