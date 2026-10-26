# map_ServerSessionGateCheck — declared `sub_42AE60`

## Identity

- Address: `0x42AE60`; file `functions/ServerSessionGateCheck_0042ae60.c`; 11 lines.
- Signature: `char __thiscall sub_42AE60(_DWORD *this)`.
- Callers (grep `functions/` for `ServerSessionGateCheck(`): `Overtaking` (`0x42AF70`) L22
  (`if (ServerSessionGateCheck(this) || !ServerSessionGateCheck(a2))`).
- Callees (from `offsets_ServerSessionGateCheck.csv` CALL rows): none.
- `this` = F1CVehicle (Overtaking passes its own `this` and a target vehicle `a2`).

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x454C | 17740 | _DWORD | L8 (`this[4435] != 1`) | branch id latch (FIELD_GLOSSARY) |
| 0x4094 | 16532 | _DWORD | L8 (`this[4133] == 2`) | ? vehicle state/mode |

Global read, not an offset (not in the CSV): `dword_7C56A4` compared with 5 at L5.

## Outputs

None.

## Constants

| value | lines | note |
|---|---|---|
| 4435, 4133 | L8 | `this[N]` indices (bytes 0x454C / 0x4094) |
| 2 | L8 | state compare for 0x4094 |
| 5 | L5 | global `dword_7C56A4` compare |

## Logic

```
if (dword_7C56A4 != 5) return 0                          [L5,L6]
result = 1                                               [L7]
if (this[4435] != 1 && this[4133] == 2) return 0         [L8,L9]
return result                                            [L10]
```

## Meaning (GUESS)

- GUESS Session/phase gate: false unless global `dword_7C56A4 == 5` (probably race session type/phase)
  [L5,L6].
- GUESS Inside that session, it returns false when the branch id latch 0x454C is not 1 AND the state field
  0x4094 equals 2 [L8,L9]; all other combinations return true [L7,L10].
- GUESS Overtaking calls it on two vehicles (`this`, `a2`) and only proceeds when one side passes and the
  other fails, i.e. a same-session/opposite-session filter [caller L22].
- GUESS The name "ServerSessionGateCheck" comes from the phase-2 target set; it is a session-state gate,
  not networking code in this function.

## Questions

- What is `dword_7C56A4` compared with 5? RacePhasePitEntryTrigger compares `dword_7C56A8` with 5 — two
  different globals, possibly phase pairs.
- What are the semantics of 0x4094 state 2 and of the branch latch 0x454C (glossary: branch id latch,
  compared with wp+276, zeroed on pit release)?
- Why does the combination latch != 1 AND state == 2 return false — what does that select?
- Does Overtaking's second call pass the opponent's vehicle so the gate compares the pair?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
