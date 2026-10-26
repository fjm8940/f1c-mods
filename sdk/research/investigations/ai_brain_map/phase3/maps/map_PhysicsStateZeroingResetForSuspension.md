# map_PhysicsStateZeroingResetForSuspension - `PhysicsStateZeroingResetForSuspension` (GUESS name)

## Identity

- Address: `0x0043E360`; file `functions/PhysicsStateZeroingResetForSuspension_0043e360.c`; 60 lines.
- Signature: `_DWORD *__thiscall sub_43E360(_DWORD *this)`.
- Callers (read-only corpus cache): `VehicleDynamicsStateReset`.
- Callees (from `offsets_PhysicsStateZeroingResetForSuspension.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x15AC | 5548 | _DWORD | L10 | `v2 = this[1387];` |
| 0x15B0 | 5552 | _DWORD | L18 | `qmemcpy((void *)(v6 + this[1388] + 100), v7, 0x..u);` |
| 0x15AC | 5548 | _DWORD | L23 | `while ( v3 < this[1387] );` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L30 | `result[1] = 0;` |
| 0x1F04 | 7940 | _DWORD | L35 | `this[1985] = 0;` |
| 0x1F08 | 7944 | _DWORD | L36 | `this[1986] = 0;` |
| 0x1F0C | 7948 | _DWORD | L37 | `this[1987] = 0;` |
| 0x1F10 | 7952 | _DWORD | L38 | `this[1988] = 0;` |
| 0x1F14 | 7956 | _DWORD | L39 | `this[1989] = 0;` |
| 0x1F18 | 7960 | _DWORD | L40 | `this[1990] = 0;` |
| 0x1F1C | 7964 | _DWORD | L41 | `this[1991] = 0;` |
| 0x1F20 | 7968 | _DWORD | L42 | `this[1992] = 0;` |
| 0x1F24 | 7972 | _DWORD | L43 | `this[1993] = 0;` |
| 0x1F28 | 7976 | _DWORD | L44 | `this[1994] = 0;` |
| 0x1F2C | 7980 | _DWORD | L45 | `this[1995] = 0;` |
| 0x1F30 | 7984 | _DWORD | L46 | `this[1996] = 0;` |
| 0xEC | 236 | _DWORD | L47 | `this[59] = 0;` |
| 0xF0 | 240 | _DWORD | L48 | `this[60] = 0;` |
| 0xF4 | 244 | _DWORD | L49 | `this[61] = 0;` |
| 0xF8 | 248 | _DWORD | L50 | `this[62] = 0;` |
| 0xFC | 252 | _DWORD | L51 | `this[63] = 0;` |
| 0x100 | 256 | _DWORD | L52 | `this[64] = 0;` |
| 0x104 | 260 | _DWORD | L53 | `this[65] = 0;` |
| 0x108 | 264 | _DWORD | L54 | `this[66] = 0;` |
| 0x10C | 268 | _DWORD | L55 | `this[67] = 0;` |
| 0x110 | 272 | _DWORD | L56 | `this[68] = 0;` |
| 0x114 | 276 | _DWORD | L57 | `this[69] = 0;` |
| 0x118 | 280 | _DWORD | L58 | `this[70] = 0;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       memset(v7, 0, sizeof(v7));   [L12]
           Copy6DwordsToStructCall(v7);   [L19]
           v6 += 580;   [L21]
       result = this + 210;   [L25]
         *result = 0;   [L29]
       this[1985] = 0;   [L35]
       this[59] = 0;   [L47]
```

## Meaning (GUESS)

- GUESS Zeroes the suspension/wheel state arrays (580-byte strides) and a block of engine/damage words.
- GUESS AI relevance: none-low -- physics reset; the state it clears is visible to AI only through physics.

## Questions

- None for this triage map; name and meaning stay GUESS.
