# map_CommandQueueResponseHandler - `CommandQueueResponseHandler` (GUESS name)

## Identity

- Address: `0x00429EE0`; file `functions/CommandQueueResponseHandler_00429ee0.c`; 41 lines.
- Signature: `int __thiscall sub_429EE0(_DWORD *this)`.
- Callers (read-only corpus cache): `AICommandMessageQueueDispatcher`.
- Callees (from `offsets_CommandQueueResponseHandler.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x4544 | 17732 | _DWORD | L8 | `this[4433] = 0;` |
| 0x4544 | 17732 | _DWORD | L13 | `this[4433] = 0;` |
| 0x4510 | 17680 | _DWORD | L15 | `this[4420] = 5;` |
| 0x4544 | 17732 | _DWORD | L21 | `this[4433] = 1;` |
| 0x443C | 17468 | _DWORD | L25 | `this[4367] = 1;` |
| 0x4544 | 17732 | _DWORD | L31 | `this[4433] = 0;` |
| 0x443C | 17468 | _DWORD | L32 | `this[4367] = 1;` |
| 0x4544 | 17732 | _DWORD | L38 | `this[4433] = 1;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       if ( dword_7C56A8 == 3 )   [L5]
         SessionCleanupFlagClear(this);   [L7]
         this[4433] = 0;   [L8]
       if ( dword_7C56A4 != 7 && PairArrayLinearSearch(this, 0) )   [L11]
         this[4433] = 0;   [L13]
         ParameterValueSetter(18);   [L14]
         this[4420] = 5;   [L15]
       if ( PairArrayLinearSearch(this, 5) )   [L18]
         ParameterValueSetter(99999);   [L20]
         this[4367] = 1;   [L25]
       result = PairArrayLinearSearch(this, 14);   [L28]
         this[4433] = 0;   [L31]
       if ( dword_7C56A8 == 4 )   [L34]
         result = ParameterValueSetter(99999);   [L37]
```

## Meaning (GUESS)

- GUESS Applies AI command-queue responses: PairArrayLinearSearch for status codes 0/4/5/14, sets queue/pit flags (this[4433], this[4367]), writes mode 5 via this[4420] and calls AISpeedDeltaSetter / ParameterValueSetter.
- GUESS AI relevance: high -- response side of the per-car AI command queue; called by AICommandMessageQueueDispatcher.
- this[4420] is driver_status 0x4510 (value 5 = garage wait per doc 54); this[4433] is 0x4544 (pit command flag).

## Questions

- None for this triage map; name and meaning stay GUESS.
