# map_SessionCleanupFlagClear — declared `sub_42ADB0`

## Identity

- Address: `0x42ADB0`; file `functions/SessionCleanupFlagClear_0042adb0.c`; 12 lines.
- Signature: `char __thiscall sub_42ADB0(int this)`.
- Callers (grep `functions/` for `SessionCleanupFlagClear(`): `AICommandMessageQueueDispatcher`
  (`0x424CD0`) L30; `AIMainUpdate` (`0x424E70`) L63; `CommandQueueResponseHandler` (`0x429EE0`) L7;
  `SessionStateTransition` (`0x4EF5A0`) L41; `VehicleProximityCollisionDetection` (`0x4A5FA0`) L51.
- Callees (from `offsets_SessionCleanupFlagClear.csv` CALL rows): none.
- `this` = F1CVehicle / session object; direct byte-offset accesses.

## Inputs

| hex | dec | type | first line read | name |
|---|---|---|---|---|
| 0x4410 | 17424 | _BYTE | L5 (`result = *(_BYTE *)(this + 17424)`) | ? state byte (returned; gates the cleanup) |

## Outputs

| hex | dec | type | first line written | name |
|---|---|---|---|---|
| 0x4424 | 17444 | _DWORD | L8 (`= 0`) | hard lateral override = 0 (FIELD_GLOSSARY) |
| 0x45F4 | 17908 | _BYTE | L9 (`= 1`) | ActiveFlag = 1 (FIELD_GLOSSARY) |

## Constants

| value | lines | note |
|---|---|---|
| 17424, 17444, 17908 | L5-L9 | direct `this` byte offsets |

## Logic

```
result = *(_BYTE *)(this + 0x4410)                     [L5]
if (!result) {                                         [L6]
    *(_DWORD *)(this + 0x4424) = 0                     [L8]
    *(_BYTE *)(this + 0x45F4) = 1                      [L9]
}
return result                                          [L11]
```

## Meaning (GUESS)

- GUESS If state byte 0x4410 is zero, clear the hard lateral override 0x4424 and set ActiveFlag 0x45F4 to
  1; the original 0x4410 value is always returned [L5-L11].
- GUESS Called on session cleanup / command-queue handling: releasing the AI's forced line override and
  re-activating the vehicle [FIELD_GLOSSARY; callers].
- GUESS Returning 0x4410 lets the caller tell whether this was a first cleanup (0) or a repeat (non-zero)
  [L5,L11].
- GUESS Names of 0x4424 / 0x45F4 are from FIELD_GLOSSARY; 0x45F4's writer list there does not include
  `SessionCleanupFlagClear`, so this map adds a writer [FIELD_GLOSSARY].

## Questions

- What is state byte 0x4410 (cleanup-done flag? release-requested?)?
- Why does the cleanup set ActiveFlag 0x45F4 = 1 rather than clear it? Is the function "activate after
  session bridge" rather than "cleanup"?
- Should 0x45F4's FIELD_GLOSSARY writer list be updated with SessionCleanupFlagClear?
- unmatched_ list is empty: no offset-like lines were dropped by the extractor.
