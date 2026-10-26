# map_StateChangeNotify - `StateChangeNotify` (GUESS name)

## Identity

- Address: `0x004A5C80`; file `functions/StateChangeNotify_004a5c80.c`; 24 lines.
- Signature: `int __thiscall sub_4A5C80(int this, int a2)`.
- Callers (read-only corpus cache): `AICommandMessageQueueDispatcher`, `AIStateReset`, `PitStateUpdateMachine`, `PitStopProgress`, `SessionTransitionDispatch`, `StateConditionalAction`, `VehicleProximityCollisionDetection`, `sub_4A5EA0`, `sub_4A5F20`, `sub_4A6D50`, `sub_4F1FE0`.
- Callees (from `offsets_StateChangeNotify.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x184 | 388 | _DWORD | L7 | `result = *(_DWORD *)(this + 388);` |
| 0x4 | 4 | _DWORD | L19 | `ProgressBarEventEncode(6, 1, 1, *(_DWORD *)(this + 4), LODWORD(v4), 0)` |

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x18C | 396 | float | L11 | `*(float *)(this + 396) = dbl_8AFEA0;` |
| 0x8 | 8 | _BYTE | L20 | `*(_BYTE *)(v3 + 8) = a2;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       result = *(_DWORD *)(this + 388);   [L7]
       if ( a2 != result )   [L8]
         if ( a2 == 1 || result != 1 && a2 == 2 )   [L10]
           *(float *)(this + 396) = dbl_8AFEA0;   [L11]
         v3 = dword_8AAE64;   [L12]
         if ( dword_83EE60 < 1023 )   [L13]
           ++dword_83EE60;   [L15]
         v4 = dbl_8AFEA0;   [L18]
         ProgressBarEventEncode(6, 1, 1, *(_DWORD *)(this + 4), LODWORD(v4), 0);   [L19]
         *(_BYTE *)(v3 + 8) = a2;   [L20]
         return AIStateMachineWithModeDispatch(v3);   [L21]
```

## Meaning (GUESS)

- GUESS AI state-change notification: on a change of the state word at +388 it raises ProgressBarEventEncode(6,...), writes the new state into the ring-buffer entry at v3+8 and calls AIStateMachineWithModeDispatch.
- GUESS AI relevance: medium-high -- state-machine notify used by AICommandMessageQueueDispatcher and ten other update functions.

## Questions

- None for this triage map; name and meaning stay GUESS.
