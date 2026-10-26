# map_IndexedStructDataRead - `IndexedStructDataRead` (GUESS name)

## Identity

- Address: `0x00403FC0`; file `functions/IndexedStructDataRead_00403fc0.c`; 17 lines.
- Signature: `int __thiscall sub_403FC0(_DWORD *this, int a2, int a3, _DWORD *a4, _DWORD *a5)`.
- Callers (read-only corpus cache): `RenderCallWithGuard`, `sub_4C6F10`.
- Callees (from `offsets_IndexedStructDataRead.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x158 | 344 | _DWORD | L7 | `v5 = (_DWORD *)(this[86] + 28 * (a2 + a3 * this[89]));` |
| 0x164 | 356 | _DWORD | L7 | `v5 = (_DWORD *)(this[86] + 28 * (a2 + a3 * this[89]));` |
| 0x4 | 4 | _DWORD | L9 | `a4[1] = v5[1];` |
| 0x8 | 8 | _DWORD | L10 | `a4[2] = v5[2];` |
| 0x164 | 356 | _DWORD | L11 | `v6 = (_DWORD *)(28 * (a2 + a3 * this[89]) + this[86] + 12);` |
| 0x158 | 344 | _DWORD | L11 | `v6 = (_DWORD *)(28 * (a2 + a3 * this[89]) + this[86] + 12);` |
| 0x4 | 4 | _DWORD | L13 | `result = v6[1];` |
| 0x8 | 8 | _DWORD | L15 | `a5[2] = v6[2];` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L9 | `a4[1] = v5[1];` |
| 0x8 | 8 | _DWORD | L10 | `a4[2] = v5[2];` |
| 0x4 | 4 | _DWORD | L14 | `a5[1] = result;` |
| 0x8 | 8 | _DWORD | L15 | `a5[2] = v6[2];` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       v5 = (_DWORD *)(this[86] + 28 * (a2 + a3 * this[89]));   [L7]
       *a4 = *v5;   [L8]
       a4[1] = v5[1];   [L9]
       a4[2] = v5[2];   [L10]
       v6 = (_DWORD *)(28 * (a2 + a3 * this[89]) + this[86] + 12);   [L11]
       *a5 = *v6;   [L12]
       result = v6[1];   [L13]
       a5[1] = result;   [L14]
       a5[2] = v6[2];   [L15]
```

## Meaning (GUESS)

- GUESS Reads two 3-dword vectors from a 28-byte-stride array (base this[86], columns this[89]) into caller buffers.
- GUESS AI relevance: none-low -- render/placement data (28-byte stride like team records but a different base object).

## Questions

- None for this triage map; name and meaning stay GUESS.
