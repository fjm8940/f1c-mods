# map_SceneRenderUpdate - `SceneRenderUpdate` (GUESS name)

## Identity

- Address: `0x004C6D90`; file `functions/SceneRenderUpdate_004c6d90.c`; 25 lines.
- Signature: `void __thiscall sub_4C6D90(int *this, float a2)`.
- Callers (read-only corpus cache): `AICommandMessageQueueDispatcher`, `DynMan_Restart`, `NetCommPlayerSlotSync`, `PitStopDecisionTireCompound`, `ProcessDriverEntries`, `sub_4A00A0`, `sub_4A0250`.
- Callees (from `offsets_SceneRenderUpdate.csv` CALL rows): none (CSV has only CALL rows of other forms or none).
- This is a terse phase-3 triage map; the name is a GUESS until reviewed.

## Inputs

None.

## Outputs

| hex | dec | type | line | expr |
|---|---|---|---|---|
| 0x48B4 | 18612 | int | L21 | `this[4653] = -8388609;` |

## Constants

Every numeric literal except 0/1 in the quoted source lines below.

## Logic

```
       VehicleRespawnReset(SLOBYTE(a2));   [L6]
       LargeConfigFunctionWithPhysicsConstants(this + 3);   [L7]
       DelegationWrapperToFUN0040ed10(dword_5EA560, (int)(this + 3));   [L8]
         sub_4BB810(this + 4109);   [L10]
       SingleFunctionDispatch_10e0(this, v4, v3);   [L11]
       ConditionalDelegateDispatch(this, v4, v3);   [L12]
       RenderCallWithGuard(this, v4, v3);   [L13]
         AIVehiclePathPositionTracking(v4, v3);   [L17]
       AIDriverStructInit(this, a2);   [L19]
       sub_4C6B60(this, 1);   [L20]
         CollisionWrapperWithLock(this + 3);   [L24]
```

## Meaning (GUESS)

- GUESS Per-frame vehicle/scene update orchestrator (respawn, physics config, AI driver struct init, render) called from the command dispatcher, pit decision, DynMan and network code.
- GUESS AI relevance: medium -- AIDriverStructInit and AIVehiclePathPositionTracking run inside it.

## Questions

- None for this triage map; name and meaning stay GUESS.
