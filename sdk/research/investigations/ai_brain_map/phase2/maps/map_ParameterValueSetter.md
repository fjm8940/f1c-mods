# map_ParameterValueSetter — `sub_42ACD0` (`ParameterValueSetter`)

## Identity

- Address: `0x42ACD0`; file `functions/ParameterValueSetter_0042acd0.c`; 7 lines.
- Signature: `void __thiscall sub_42ACD0(float *this, int a2)`.
- Callers (grep `functions/` for `ParameterValueSetter(`): `AICommandMessageQueueDispatcher`
  (`0x424CD0`) L38 (`ParameterValueSetter(18)`); `AIAbility` (`0x429470`) L139 (`ParameterValueSetter(18)`);
  `CommandQueueResponseHandler` (`0x429EE0`) L14, L26 (`ParameterValueSetter(18)`), L20, L37
  (`ParameterValueSetter(99999)`).
- Callees (from `offsets_ParameterValueSetter.csv` CALL rows): none.

## Inputs

None at the field level except the `a2` argument; the branch tests `a2 == 18`.

Global read: `flt_8ACFB4` (L4, doc 12 `IdleSpeedTarget`; `~22.2222` per a mod comment in
`mods/sdk_sandbox/game_update_broker_r2_control_20260929/game_update_broker.c` L65).

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4538 | 17720 | float | L4, L6 | maxSpeed / speed governor (`this[4430]`; FIELD_GLOSSARY maxSpeed, doc 12 also `IdleSpeedTarget`) |

## Constants

Grouped by value; every numeric literal except 0/1 in `functions/ParameterValueSetter_0042acd0.c`.

| value | lines | note |
|---|---|---|
| 18 | L3 | sentinel selector: reset to `flt_8ACFB4` |
| 4430 | L4, L6 | `this[N]` index; byte offset 0x4538 |

Caller-side constants (not in this file): `18` (L14/L26/L38/L139), `99999` (L20/L37).

## Logic

NOTE: source uses scaled `this[N]` indexing; Logic keeps the source-literal index and no hex token. The
byte offset is in the table above.

```
if (a2 == 18) this[4430] = flt_8ACFB4     [L3,L4]
else this[4430] = (float)a2               [L6]
```

## Meaning (GUESS)

- GUESS Writes the speed governor/target at `0x4538`: selector `18` resets it to the `flt_8ACFB4` idle
  target, any other value is stored as `(float)a2` [L3-L6].
- GUESS `18` is a command code in the AI command-message queue; `AICommandMessageQueueDispatcher` L38 and
  `CommandQueueResponseHandler` L14/L26 send it, while `99999` (L20/L37) sets an effectively unlimited
  speed (the "clear limit" value) [callers].
- GUESS `AIAbility` L139 calls `ParameterValueSetter(18)` as part of pit-stop completion
  (`LABEL_57`: `PostPitCleanup`, `this[4433] = 0.0`, `ParameterValueSetter(18)`, ...), i.e. restore idle
  speed after servicing [caller].
- GUESS `0x4538` is the same field that pace mods write (FIELD_GLOSSARY: "maxSpeed (inspect CONFIRMED;
  pace mod writes here)") [FIELD_GLOSSARY].
- GUESS The two names (`IdleSpeedTarget` in doc 12 vs `maxSpeed` in FIELD_GLOSSARY) describe the same
  governor field used both as a floor (idle) and a cap (command 99999) [docs].

## Questions

- Naming conflict: doc 12 lists `dword/flt_8ACFB4 = IdleSpeedTarget` "stored at vehicle offsets 0x114e and
  0x4538"; `FIELD_GLOSSARY` calls `0x4538` "maxSpeed". Which role is primary?
- Is `99999` a hard-coded "no limit" sentinel? No named constant exists.
- Does a2 have units (km/h, m/s, percent) before the float cast?
- `CommandQueueResponseHandler` mixes `18` and `99999` across branches; the queue command semantics should
  be confirmed from that function's map.
