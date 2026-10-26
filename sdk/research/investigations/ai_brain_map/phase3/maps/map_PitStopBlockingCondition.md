# map_PitStopBlockingCondition - `PitStopBlockingCondition` (GUESS name)

## Identity

- Address: `0x0042AEE0`; file `functions/PitStopBlockingCondition_0042aee0.c`; 9 lines.
- Signature: `int __thiscall sub_42AEE0(_DWORD *this)`.
- Callers (read-only corpus cache): `PitStopDecisionTireCompound`, `PitStopOperationTimer`.
- Callees (from `offsets_PitStopBlockingCondition.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x45E0 | 17888 | _DWORD | L4 | `return this[4 * this[4472] + 4455] & 8;` |
| 0x403C | 16444 | _DWORD | L5 | `return this[4111] >= dword_8ACF8C` |
| 0x39FA | 14842 | _BYTE | L7 | `\|\| *((_BYTE *)this + 14842)` |

## Outputs

None.

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( dword_7C56A4 != 7 )   [L3]
         return this[4 * this[4472] + 4455] & 8;   [L4]
       return this[4111] >= dword_8ACF8C   [L5]
           || dword_7C56A8 == 8   [L6]
           || *((_BYTE *)this + 14842)   [L7]
           || *(_DWORD *)(*this + 3352) && !*(_BYTE *)(*this + 3416);   [L8]
```

## Meaning (GUESS)

- GUESS Pit-stop blocking test: outside session type 7 returns bit 3 of the active plan-state dword this[4*this[4472] + 4455] (this[4472] = pit_plan_cursor 0x45E0); in type 7 checks race-end/garage conditions.
- GUESS AI relevance: high -- pit decision/entry gate; callers PitStopDecisionTireCompound and PitStopOperationTimer.
- GUESS bit 3 of the 16-byte pit-plan row marks the active slot as blocked; the type-7 branch covers race end / garage / flags.

## Questions

- None for this triage map; name and meaning stay GUESS.
