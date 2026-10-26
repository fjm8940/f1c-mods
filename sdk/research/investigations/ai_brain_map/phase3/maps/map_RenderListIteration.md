# map_RenderListIteration - `RenderListIteration` (GUESS name)

## Identity

- Address: `0x0054F710`; file `functions/RenderListIteration_0054f710.c`; 38 lines.
- Signature: `int __thiscall sub_54F710(_DWORD *this, int a2)`.
- Callers (read-only corpus cache): `UIFocusChange`.
- Callees (from `offsets_RenderListIteration.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x1E0 | 480 | _DWORD | L8 | `v3 = this[120];` |
| 0x8 | 8 | _DWORD * | L10 | `v5 = *(_DWORD **)(v3 + 8);` |
| 0x4 | 4 | int | L12 | `if ( *(int *)(v3 + 4) <= 0 )` |
| 0x1E0 | 480 | _DWORD | L36 | `while ( v4 < *(_DWORD *)(this[120] + 4) );` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       v3 = this[120];   [L8]
       v4 = 0;   [L9]
       v5 = *(_DWORD **)(v3 + 8);   [L10]
       if ( *(int *)(v3 + 4) <= 0 )   [L12]
         if ( *v5 == 2 )   [L16]
           (*(void (__cdecl **)(int, _DWORD *, _DWORD *, _DWORD *))(*(_DWORD *)dword_8B8F24 + 4))(   [L22]
           (*(void (__cdecl **)(int, _DWORD *, _DWORD *, _DWORD *))(*(_DWORD *)dword_8B8F24 + 4))(   [L27]
         v5 += 29;   [L33]
         ++v4;   [L34]
```

## Meaning (GUESS)

- GUESS Iterates the render list (this[120]) and calls the registered vtable on non-type-2 entries; counts skipped entries.
- GUESS AI relevance: none -- render.

## Questions

- None for this triage map; name and meaning stay GUESS.
