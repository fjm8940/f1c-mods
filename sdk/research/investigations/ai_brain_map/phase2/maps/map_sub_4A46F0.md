# map_sub_4A46F0 — `sub_4A46F0`

## Identity

- Address: `0x4A46F0`; file `functions/sub_4A46F0_004a46f0.c`; 13 lines.
- Signature: `int __thiscall sub_4A46F0(_DWORD *this, int a2, char a3, int a4)`.
- Callers (grep `functions/` for `sub_4A46F0(`): `AICommandMessageQueueDispatcher` (`0x424CD0`) L13, L17;
  `AISpeedDeltaSetter` (`0x42ACA0`) L9; `sub_426430` (`0x426430`) L130, L135.
- Calling convention: `functions/` call sites render literals (`sub_4A46F0(this, 1, 1)` etc.), but the typed
  variants show the hidden receiver and real arguments:
  `sub_4A46F0(dword_7CE2F0, (int)this, 1, 1)` (`functions_typed_20260930/AICommandMessageQueueDispatcher_00424cd0.c`
  L13, L17; `functions_typed_20260930/sub_426430_00426430.c` L132, L137). So `this` = global AI vehicle table
  `dword_7CE2F0`, `a2` = a vehicle pointer. Prior art (`investigations/ai_interaction_role/README.md` L1630-1639)
  documents the raw-signature conflict; do not semantically name the callers' literals from this alone.
- Callees (from `offsets_sub_4A46F0.csv` CALL rows): none.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4 | 4 | _DWORD | L7 (`*(_DWORD *)(a2 + 4)`) | ? vehicle slot index (field at vehicle+4) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| (dynamic) `this[v5]` | — | _DWORD | L9, L11 | AI vehicle table slot = `a2` (enable) or 0 (disable); index `v5` comes from `a2+4` |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/sub_4A46F0_004a46f0.c`.

| value | lines | note |
|---|---|---|
| 4 | L7 | `a2` field offset (0x4) |
| 0 | L11 | cleared slot value |
| 0, 1 | L8 (`if (a3)`) | boolean enable flag |

No integer bit-pattern is written into a float field.

## Logic

```
result = a2                                  [L6]
v5 = *(a2 + 0x4)                             [L7]
if (a3) this[v5] = a2  else this[v5] = 0     [L8,L9,L11]
return result                                [L12]
```

## Meaning (GUESS)

- GUESS Registry setter: `a2` is a vehicle pointer, `*(a2+0x4)` is its slot index, and `this` is the global AI
  vehicle table `dword_7CE2F0`. When `a3` is true the vehicle pointer is stored at `this[slot]`, otherwise the
  slot is zeroed [L6-L12].
- GUESS `a4` is unused in the recovered body (candidate parameter or dropped receiver artifact).
- GUESS Consumers use it to register/unregister vehicles: command 4 (`AICommandMessageQueueDispatcher` L13) adds,
  command 5 (L17) removes, `sub_426430` toggles on route-state transitions (L130 vs L135).
- GUESS Prior art warns the raw call forms `(this, 1, 1)` / `(this, 0, 0)` are decompiler argument-loss; the
  typed forms are the reliable ones, so this is not a named pit/lane operation.

## Questions

- What is `a4`? It is never read in this body.
- Is vehicle+0x4 really the table slot index (cross-check with `dword_5C329C` counts)?
- Which events must unregister a vehicle (crash, retirement, session end) and who is `dword_7CE2F0`?
- Confirm the hidden receiver with disassembly before trusting any caller literal.
