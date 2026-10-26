# map_sub_4F18C0 - `sub_4F18C0` (GUESS name)

## Identity

- Address: `0x004F18C0`; file `functions/sub_4F18C0_004f18c0.c`; 55 lines.
- Signature: `void __thiscall sub_4F18C0(int this, unsigned int a2, int a3, char a4)`.
- Callers (read-only corpus cache): `LoopWithConditionalSubCall`.
- Callees (from `offsets_sub_4F18C0.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x396C | 14700 | _DWORD * | L15 | `v5 = *(_DWORD **)(this + 14700);` |
| 0xB0 | 176 | double | L30 | `*(float *)(248 * a2 + this + 13880) = v9[22] / v10;` |
| 0x20 | 32 | double | L31 | `*(float *)(248 * a2 + this + 13840) = (50.0 / v9[4] * v9[5] + v9[4]) *` |
| 0x28 | 40 | double | L31 | `*(float *)(248 * a2 + this + 13840) = (50.0 / v9[4] * v9[5] + v9[4]) *` |
| 0x100 | 256 | double | L32 | `v11 = v9[32] * 0.5;` |
| 0x8 | 8 | double | L33 | `*(float *)(248 * a2 + this + 13856) = (v9[1] + *v9) * v11;` |
| 0x18 | 24 | double | L34 | `v12 = v9[3] + v9[2];` |
| 0x10 | 16 | double | L34 | `v12 = v9[3] + v9[2];` |
| 0x110 | 272 | double | L39 | `*(float *)(248 * a2 + this + 13876) = v9[34];` |
| 0x3BF4 | 15348 | float | L40 | `*(float *)(248 * a2 + this + 13852) = v13 * *(float *)(this + 15348);` |
| 0x108 | 264 | double | L41 | `if ( v9[33] <= 0.0 )` |
| 0x108 | 264 | double | L44 | `*(float *)(248 * a2 + this + 13844) = 1.0 / v9[33];` |
| 0xF8 | 248 | double | L45 | `v15 = v9[31];` |
| 0xF0 | 240 | double | L46 | `v14 = v9[30];` |
| 0x32BC | 12988 | _DWORD | L50 | `if ( !*(_DWORD *)(this + 12988) )` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x3970 | 14704 | _DWORD | L16 | `*(_DWORD *)(this + 14704) = a3;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
     void __thiscall sub_4F18C0(int this, unsigned int a2, int a3, char a4)   [L1]
```

## Meaning (GUESS)

- GUESS Vehicle sub-update dispatched with an index, object pointer and flag; called from LoopWithConditionalSubCall and other vehicle/pit update paths.
- GUESS AI relevance: low -- not on the AI decision path.

## Questions

- None for this triage map; name and meaning stay GUESS.
