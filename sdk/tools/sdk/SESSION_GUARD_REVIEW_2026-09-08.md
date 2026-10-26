# Read-only native session guard — 2026-09-08

`f1c_session_guard.h` encapsulates Red Flag v76 GameModuleManager offline-state, session type, game state, race phase, pause byte and player-out reads. Native manager pointer is uint32_t; null/overflow and any failed read fail closed. Only measured state zero means offline. Consumers establish exact executable identity and stable game-thread use; the adapter itself performs no game writes. Player-out remains observed, not a universal spectator rejection.

Evidence source: `development/mods/red_flag/trials/2026-09-08/manual-countdown-v76/redflag.input.hardened.c`, mp_state/is_multiplayer/race_is_settled and field macros. Unknown/nonzero is refused; live multiplayer/replay/spectator validation is still pending. Supported executable SHA2561A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003 only.

Canonical SDK gate and primary independent adapter/callback tests pass. The SDK focus test executes an extracted adapter, and other SDK action bindings are source-flow checks; primary runs full extracted loader/game_tick and release/departure authorization tests separately. [Exact candidate, verification, installation, preservation and RF limitations](../../../development/mods/full_safety_car/GUARDRAILS_IMPLEMENTED_2026-09-08.md).
