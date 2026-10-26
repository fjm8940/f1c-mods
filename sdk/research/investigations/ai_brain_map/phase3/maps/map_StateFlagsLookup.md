# map_StateFlagsLookup - `StateFlagsLookup` (GUESS name)

## Identity

- Address: `0x0042AE90`; file `functions/StateFlagsLookup_0042ae90.c`; 4 lines.
- Signature: `int __thiscall sub_42AE90(_DWORD *this)`.
- Callers (read-only corpus cache): `AIBehaviorInitialization`, `EngineFailureCheck`, `sub_4A4110`.
- Callees (from `offsets_StateFlagsLookup.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4588 | 17800 | _DWORD | L3 | `return this[4 * (this[4450] - 1 < 0 ? 1113 : this[4450] - 1 + 1113)];` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       return this[4 * (this[4450] - 1 < 0 ? 1113 : this[4450] - 1 + 1113)];   [L3]
```

## Meaning (GUESS)

- GUESS Returns the per-plan flags dword this[4*(pit_plan_count-1) + 1113]; this[4450] is 0x4588 (pit_plan_count) and selects the 16-byte plan row.
- GUESS AI relevance: high -- pit-plan state flags; callers AIBehaviorInitialization, EngineFailureCheck, sub_4A4110.

## Questions

- None for this triage map; name and meaning stay GUESS.
