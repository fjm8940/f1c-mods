# map_sub_4FB6D0 - `sub_4FB6D0` (GUESS name)

## Identity

- Address: `0x004FB6D0`; file `functions/sub_4FB6D0_004fb6d0.c`; 56 lines.
- Signature: `double __thiscall sub_4FB6D0(void *this, float *a2)`.
- Callers (read-only corpus cache): `sub_403640`.
- Callees (from `offsets_sub_4FB6D0.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4 | 4 | float | L24 | `v16 = a2[1] - *((float *)this + 1);` |
| 0x8 | 8 | float | L25 | `v2 = a2[2] - *((float *)this + 2);` |
| 0x28 | 40 | float | L27 | `v3 = v2 * *((float *)this + 10) + v16 * *((float *)this + 9) + v15 * *` |
| 0x24 | 36 | float | L27 | `v3 = v2 * *((float *)this + 10) + v16 * *((float *)this + 9) + v15 * *` |
| 0x20 | 32 | float | L27 | `v3 = v2 * *((float *)this + 10) + v16 * *((float *)this + 9) + v15 * *` |
| 0x5C | 92 | float | L29 | `result = v3 - *((float *)this + 23);` |
| 0x58 | 88 | float | L31 | `result = -(v3 + *((float *)this + 22));` |
| 0x110 | 272 | _DWORD | L32 | `v5 = (float *)*((_DWORD *)this + 68);` |
| 0x4 | 4 | float | L34 | `v11 = v5[1] - *((float *)this + 1);` |
| 0x8 | 8 | float | L35 | `v13 = v5[2] - *((float *)this + 2);` |
| 0x10C | 268 | _DWORD | L42 | `v7 = (float *)*((_DWORD *)this + 67);` |
| 0x4 | 4 | float | L44 | `v12 = v7[1] - *((float *)this + 1);` |
| 0x8 | 8 | float | L45 | `v14 = v7[2] - *((float *)this + 2);` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       v15 = *a2 - *(float *)this;   [L23]
       v3 = v2 * *((float *)this + 10) + v16 * *((float *)this + 9) + v15 * *((float *)this + 8);   [L27]
       if ( v3 >= 0.0 )   [L28]
         result = v3 - *((float *)this + 23);   [L29]
         result = -(v3 + *((float *)this + 22));   [L31]
       v5 = (float *)*((_DWORD *)this + 68);   [L32]
       v9 = *v5 - *(float *)this;   [L33]
       v18 = v13 * (1.0 / v6) * v17 + v11 * (1.0 / v6) * v16 + v9 * (1.0 / v6) * v15 - v6;   [L39]
       if ( result < v18 )   [L40]
       v19 = v14 * (1.0 / v8) * v17 + v12 * (1.0 / v8) * v16 + v10 * (1.0 / v8) * v15 - v8;   [L49]
       if ( result < v19 )   [L50]
       v20 = fabs(v16) - 3.0;   [L52]
       if ( result < v20 )   [L53]
```

## Meaning (GUESS)

- GUESS Signed distance from a point to two segment endpoints/bounding planes (sqrt/normal math), with a 10.0 fallback for degenerate segments.
- GUESS AI relevance: none-low -- spatial query helper used by sub_403640.

## Questions

- None for this triage map; name and meaning stay GUESS.
