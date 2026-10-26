# ESC Pause Menu

Replaces the vanilla ESC behavior (abandon race → RACE RESULTS) with an
in-race pause menu, modeled on NASCAR Thunder 2004's same-engine RestartGate.

## Behavior

- **ESC during a race** freezes the sim (game's own pause byte `0x7C56AC`;
  physics stops, SessionTime freezes, "Paused" overlay shows) and draws a menu
  on the game's own HUD message strip:

  ```
  PAUSE MENU
  > CONTINUE
    RETIRE TO PITS
    EXIT RACE
    REPLAY
  UP/DOWN SELECT  ENTER CONFIRM
  ```

- **Qualifying / practice** get the same menu with `RETURN TO PITS` instead of
  `RETIRE TO PITS` (no SessionType flip needed there).
- **Up / Down** move the cursor (wraps). **Enter** confirms.
- **ESC while the menu is open** = CONTINUE (resumes the race), matching
  NT2004.
- Menu options:
  - `CONTINUE` — resume.
  - `RETIRE TO PITS` / `RETURN TO PITS` — close the menu, then the player
    retires by the game's own path (the proven `retire_player_to_pits()`
    sequence: SessionType 7→5 flip for races, retirement writes, restart-flag
    pinning) and lands in the pit garage view, same as the wheel-stuck
    mechanical-failure retirement.
  - `EXIT RACE` — exactly what vanilla ESC did (`dword_7C569C = 6` +
    `MainMenuTransition`), landing on RACE RESULTS.
  - `REPLAY` — injects DIK_R (0x13) into the game's key queue, same as
    pressing R.
- ESC passes through to vanilla behavior while the game is in replay mode
  (`dword_8AB6E4 == 2`) and outside sessions (menus).

## Architecture

- **Broker (ABI 10, `F1C_BROKER_CAP_PAUSE_MENU`)**: owns all game-thread work.
  Its `RestartGate` hook gained a PRE-section that scans the key queue before
  the original can abandon the race, manages the menu state machine, freezes/
  unfreezes the sim, renders the menu, swallows the key queue, executes the
  EXIT leaf, injects the replay key, and emits `F1C_BROKER_EVENT_PAUSE_MENU`.
  Pure state machine + labels live in `broker_core.h` (regression-tested).
- **This mod**: a thin broker client. It discovers and arms the capability,
  polls events, and runs the retirement sequence on its own thread (the one
  thing that must not block the game thread).
- The broker's ABI-5 RestartGate hook already owned the ESC site; this mod
  never duplicates it.

## Build

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/esc_pause_menu
python tools/f1c_mod.py validate mods/sdk_sandbox/esc_pause_menu
```

Requires the ABI-10 broker DLL (`mods/sdk_sandbox/game_update_broker/build/
f1c_game_update_broker.dll`) loaded in the game process; discovery fails
closed otherwise (see `tools/sdk/f1c_broker.h`).

## Configuration (`esc_pause_menu.ini`, read once at load)

- `[Mod] Enabled` — master switch (default 1).
- `[Retire] HoldMaxMs` — retirement wait cap (default 900, proven value).
- `[Retire] GuardMs` — restart-flag pin tail (default 2500, proven value).

## Unload

```powershell
python tools/f1c_mod.py unload F1C_ESC_PAUSE_MENU_UNLOAD
```

## Known friction (2026-08-14)

- **Stale-event replay on re-inject** — closed in the broker: a fresh client
  used to poll the event ring from cursor 0 and replay retained history
  (observed as a `RETIRE command received` log line 828 ms after load). The
  broker now floors every poll at a per-client registration watermark
  (`BrokerClient.event_floor`, set in `F1CBrokerRegister`), so a re-injected
  client only sees events emitted after it registered.
- **OPEN: "Return to pits" ~500 ms latency — instrumented, not bisected.**
  Mod-side timings (`[tickms]` stamps): event received → thread start →
  snapshot ok → writes all 0 ms apart; gate consumed PLAYER_OUT 15 ms later.
  Remaining candidates: (a) SPACE → broker command pickup (broker log has
  `pause: pending command picked up` / `pause: executing command` /
  `pause: execute entered`), or (b) the game's own post-gate teardown
  (sub_4A0250 garage path), which the mod does not control. To bisect: run
  one live session with broker log and client log both timestamped and press
  SPACE once; compare the pickup timestamp against the felt delay.

## Validation status

Compiles clean, broker regressions + compatibility matrix + full SDK test gate
pass. **Not yet live-validated in a guarded weekend** — the REPLAY option in
particular depends on the game's own R-key handler being key-queue driven
(confirmed for ESC; R needs a live check). All four options must be exercised
through a full race weekend via `f1cm_guard.asi` before promotion.
