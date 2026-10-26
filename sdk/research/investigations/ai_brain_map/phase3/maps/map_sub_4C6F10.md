# map_sub_4C6F10 - `sub_4C6F10` (GUESS name)

## Identity

- Address: `0x004C6F10`; file `functions/sub_4C6F10_004c6f10.c`; 130 lines.
- Signature: `void __thiscall sub_4C6F10(_DWORD *this)`.
- Callers (read-only corpus cache): `ConditionalDelegateDispatch`, `MultiplayerVehiclePlacement`, `RenderCallWithGuard`.
- Callees (from `offsets_sub_4C6F10.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4894 | 18580 | _DWORD | L34 | `this[4649] = this[4645] % 52;` |
| 0x4710 | 18192 | _DWORD | L48 | `v4 = this[4548];` |
| 0x48A4 | 18596 | _DWORD | L62 | `ArrayOfStructIndexedFieldCopy(dword_5EA2C0, this[4649], v28, v27);` |
| 0x48A4 | 18596 | _DWORD | L72 | `if ( (_DWORD *)v9 != this && *(_DWORD *)(v9 + 18596) == this[4649] && ` |
| 0x48A8 | 18600 | _DWORD | L72 | `if ( (_DWORD *)v9 != this && *(_DWORD *)(v9 + 18596) == this[4649] && ` |
| 0x48A4 | 18596 | _DWORD | L96 | `ArrayOfStructIndexedFieldCopy(dword_5EA2C0, this[4649], v28, v27);` |
| 0x48A4 | 18596 | _DWORD | L101 | `IndexedStructDataRead(dword_5EA2C0, this[4649], k, v30, v29);` |
| 0x48A4 | 18596 | _DWORD | L109 | `if ( (_DWORD *)v16 != this && *(_DWORD *)(v16 + 18596) == this[4649] &` |
| 0x48A8 | 18600 | _DWORD | L109 | `if ( (_DWORD *)v16 != this && *(_DWORD *)(v16 + 18596) == this[4649] &` |
| 0x48A4 | 18596 | _DWORD | L119 | `v17 = this[4649] + 1;` |
| 0x48A4 | 18596 | _DWORD | L120 | `v18 = this[4649] - 51 < 0;` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x48A4 | 18596 | _DWORD | L34 | `this[4649] = this[4645] % 52;` |
| 0x48A4 | 18596 | _DWORD | L42 | `this[4649] = v2;` |
| 0x48A8 | 18600 | _DWORD | L68 | `this[4650] = v7;` |
| 0x48A4 | 18596 | _DWORD | L93 | `this[4649] = 0;` |
| 0x48A8 | 18600 | _DWORD | L105 | `this[4650] = v14;` |
| 0x48A4 | 18596 | _DWORD | L121 | `this[4649] = v17;` |
| 0x48A4 | 18596 | _DWORD | L127 | `this[4649] = -1;` |
| 0x48A8 | 18600 | _DWORD | L128 | `this[4650] = -1;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
     void __thiscall sub_4C6F10(_DWORD *this)   [L1]
```

## Meaning (GUESS)

- GUESS Vehicle scene/render update chain (130 lines) called from the render path and multiplayer placement.
- GUESS AI relevance: none-low -- render.

## Questions

- None for this triage map; name and meaning stay GUESS.
