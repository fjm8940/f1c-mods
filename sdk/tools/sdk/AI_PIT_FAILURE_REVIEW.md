# AI pit-failure native adaptation — 2026-09-21

Exact target 1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003.
Caller must be the broker game thread with freshly validated vehicle identity.
Only AI role 1 in SERVICE, unfinished and not retired, is admitted.

Original player pit retirement changes global player-out/session/restart flags;
these must not be copied for AI. Original AI failures were delay/voice only.

f1c_ai_engine_stop.h is reused unchanged. ConditionalGate 004EFAC0 takes
vehicle+12, thiscall, and its AI branch clears vehicle EngineRPM at 39A0.
New f1c_ai_pit_restart calls VehicleClimbAdjust 004EFA40 with vehicle+12,
argument 1. Raw code and exact executable disassembly show that branch raises
RPM to the native idle at vehicle+39A8 without consuming the RNG used by
argument 0. Player branch is never admitted. Entry fingerprint is checked.

f1c_ai_pit_retire reuses the existing f1c_pit.h organic garage predicate:
the rear-wing intact flag is held false only across the synchronous native
PitStopDecisionTireCompound 0042A3D0 call, then restored. The same native
function is called by PitStopReadyCheck and tests the rear-wing record. Its
terminal branch calls SceneRenderUpdate(0), sets RetiredFlag/ActiveFlag and
queues sub_4BC060's native retirement event. All classification/placement is
performed by native code. No player/session global, direct DNF flag or
coordinate writes are made. Entry fingerprint, back-pointer, wing record,
strategy index 0..3, race session and READY are checked before the temporary
predicate. This is a terminal decision only, never a normal service timer reroll.

Evidence obtained through canonical inspect/exe-disassemble commands for
ConditionalGate, VehicleClimbAdjust, PitStopDecisionTireCompound,
PitStopBlockingCondition, PitStopReadyCheck, SceneRenderUpdate, AIDriverStructInit,
PostPitCleanup and sub_4BC060. Decompiler hidden-this arguments were checked
against existing SDK object contracts; these are exact-target trial helpers,
not a portable broker capability promise.

Pit mod integration tests cover mocked native stop/restart, terminal diagnosis,
single retirement invocation and selection. Native validation, including engine
silence and correct garage geometry, is separately recorded in
../../../development/mods/pitstop_modding/verification/ai-mechanics-2026-09-21/REPORT.md.
Never call this a confirmed garage placement from request acceptance alone.
The lifecycle reader must observe terminal status, inactive AI, DNF and the
allocated garage location. Human visual/audio confirmation remains separate.
