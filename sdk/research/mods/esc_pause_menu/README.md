**2026-10-11 RETIRE TO PITS crash (open):** Lightloader 2.3.4, crash in d3d8.dll right after `native garage route armed`, before PLAYER_OUT consumed; an earlier retire the same session passed. No dump; cause unproven. [Evidence and pending](verification/retire-crash-d3d8-2026-10-11/REPORT.md).

**Free v1.0.2 published, 2026-09-23:** [Download](https://mods.palloset.com/esc_pause_menu). The protected client preserves Disqualified when a disqualified player selects Retire to Pits; the shared broker opens RACE RESULTS from the garage on reviewed layout-compatible game versions, including the owner-tested 99-02 variant. The exact pair was tested through official Lightloader in an isolated copy. ZIP SHA-256: `2F5858BBA8996A68E5495EB49720281313B4E5CDFE98482CBC873A768885F658`. [Promotion, public verification and restoration](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_2.md). Earlier candidate and v1.0.1 entries are historical.

**2026-09-23 compatible-variant repair candidate:** v1.0.2-rc.1 replaces the broker's metadata-only refusal of the 99-02 garage results hook with checked code-layout preflight. The protected ESC client is unchanged; the new broker and client are staged in an isolated 99-02 copy. Static variant checks, focused native fixtures, and the full SDK gate pass. The in-game results and P-key regression checks remain pending; public v1.0.1 is unchanged. [Repair, evidence, and test path](verification/variant-garage-2026-09-23/REPORT.md).

**2026-09-23 99-02 Race Results finding:** The same published DLLs load in both tested installations. The 99-02 executable is accepted as a compatible variant, but the shared broker refuses the canonical-only Leave Circuit results hook; the native action therefore skips the results screen. The later hook-byte checks have not been evaluated for that variant. [Comparison, evidence, and next validation](verification/variant-results-2026-09-23/REPORT.md).

**Free v1.0.1 published, 2026-09-23:** [Download](https://mods.palloset.com/esc_pause_menu). This release corrects `install.txt`: launch the game, use Up/Down and Space, and report bugs at https://mods.palloset.com/report/. The two DLLs are byte-identical to v1.0.0. ZIP SHA-256: `3FE7483DFED414E284AE5AAF874834876E911D2BBD81E6FDD743473231DFBF5B`. [Publication and restoration](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_1.md). The v1.0.0 entry below is historical.

**Free v1.0.0 published, 2026-09-23:** [Download](https://mods.palloset.com/esc_pause_menu). Install the two DLLs through official Lightloader 1.1.0 using the included `install.txt`; there is no EXE installer. The client accepts only Lightloader's exclusive host proof, and the broker retains its game-hook safety check. ZIP SHA-256: `453ECB3A176F8D0A9907736D3C112C7BBF24384A2BD290443302F82170B0889C`. The protected client is unsigned. [Publication record](../../../operations/release_management/esc_pause_menu/PUBLICATION_1_0_0.md). Earlier release-preparation entries below are historical.

**Free release preparation, 2026-09-23:** rc.2 is installed for exact protected-hash gameplay validation. It uses Lightloader as the ESC client's only loader gate, without an executable name/hash check; the unchanged broker retains its owner-approved memory-layout safety guard. Build, SDK,  static, Defender, and offline loader checks pass. Gameplay, signing decision, production promotion, and VPS publication remain pending. The planned package contains two DLLs and no installer. [Release candidate and restoration](releases/v1.0.0-rc.2/RELEASE_CANDIDATE.md).

**Owner accepted, 2026-09-23:** Matias reports the installed P-key fix works and considers ESC Pause Menu finished. Broker `9D85FD4D…` and client `84FA9C5C…` are the installed development pair; public release has not been performed. [Acceptance, evidence, and restoration](verification/p-key-2026-09-23/REPORT.md).

**Owner gameplay pass, 2026-09-23:** installed r5 broker `C1FA9203…`; owner reports the requested retirement/garage/Leave Circuit/results flow now works properly. Native completion corroborated in logs, including maximized monitor. This supersedes pending r5 statements below; broader modes and mechanical-failure path remain separate gates. [Pass record](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md).

**Current r5 trial installed (2026-09-23):** r4 resumed driving because native ESC cleared UI-active. Broker `C1FA9203…` retains the UI-active/frame context and race data through results; ACCEPT exits. Focused/native UI-setter regression and full SDK pass; one-click populated results gameplay pending. [Evidence and rollback](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md). Earlier summaries below are historical.

**Current r4 trial installed (2026-09-23):** r3 opened empty Race Results because exit cleanup preceded the native countdown. Broker `1D22A793…` retains the session on the results page until native ACCEPT exits. Build/focused/full SDK gate pass; populated results and ACCEPT gameplay pending. [Evidence and rollback](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md). Earlier candidate summaries below are historical.

**Current r3 trial installed (2026-09-23):** broker `D7E50F67…` maps the on-screen Leave Circuit icon to the native ESC exit after UI processing, covering normal and maximized monitors. Build/full SDK gate pass; gameplay pending. Earlier trial failures below remain historical. [Evidence and restoration](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md).

**Latest owner test, 2026-09-23: FAILED.** Broker 76B0F99A intercepts normal Leave Circuit clicks without finalizing; MONITOR MAXIMIZED bypasses the new route. Earlier pending/pass statements below are historical. Review of older mods found retained ESC-results fixes but no confirmed complete garage-icon pass. See the latest section of the [investigation record](../pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md).

# ESC Pause Menu

**2026-09-23 trial installed:** the actual on-screen Leave Circuit icon now
schedules native race finalization for a retired player. Original OSC restored;
build and full SDK gate pass. Gameplay pending.
[Current evidence and restoration](verification/leave-icon-2026-09-23/REPORT.md).

Replaces the vanilla ESC behavior (abandon race → RACE RESULTS) with an
in-race pause menu, modeled on NASCAR Thunder 2004's same-engine RestartGate.

## Behavior

- **ESC during a race** enters the same native pause state as **P** (the game's
  pause byte `0x7C56AC`; physics stops, SessionTime freezes, and the native
  "Paused" overlay shows) and draws a menu
  on the game's own HUD message strip:

  ```
  PAUSE MENU
  > CONTINUE
    RETIRE TO PITS
    EXIT RACE
  UP/DOWN SELECT  SPACE CONFIRM
  ```

- **Qualifying / practice** get the same menu with `RETURN TO PITS` instead of
  `RETIRE TO PITS` (no SessionType flip needed there).
- **Up / Down** move the cursor (wraps). **Space** confirms.
- **ESC while the menu is open** = CONTINUE (resumes the race), matching
  NT2004.
- Menu options:
  - `CONTINUE` — resume.
  - `RETIRE TO PITS` / `RETURN TO PITS` — close the menu, then the player
    retires by the game's own path. During an in-race handoff, the client
    temporarily presents SessionType 5 while the game consumes `PLAYER_OUT`,
    then immediately restores SessionType 7. Live testing established that
    this is the route which leaves the player in the pit-garage spectator view.
    After that handoff, clicking the on-screen **X / LEAVE CIRCUIT** icon
    schedules the native results-grid countdown. Gameplay must still verify
    that the race finalizes and **ACCEPT** returns to the main menu.
  - `EXIT RACE` — exactly what vanilla ESC did (`dword_7C569C = 6` +
    `MainMenuTransition`), landing on RACE RESULTS.
- ESC passes through to vanilla behavior while the game is in replay mode
  (`dword_8AB6E4 == 2`) and outside sessions (menus).

## Architecture

- **Broker (ABI 10, `F1C_BROKER_CAP_PAUSE_MENU`)**: owns all game-thread work.
  Its `RestartGate` hook gained a PRE-section that scans the key queue before
  the original can abandon the race, manages the menu state machine, freezes/
  unfreezes the sim, renders the menu, swallows the key queue, executes the
  EXIT leaf, and emits `F1C_BROKER_EVENT_PAUSE_MENU`. While open, it also
  consumes the native P pause action while the menu owns pause. The former
  `0x7CC4F4` audio-gate classification was disproved by the native toggle
  branch; ESC sound behavior needs renewed gameplay validation.
  Pure state machine + labels live in `broker_core.h` (regression-tested).
- **This mod**: a thin broker client. It discovers and arms the capability,
  polls events, keeps its persistent menu arm asserted, and runs retirement
  only after a locally observed OPEN event. This prevents the pit-failure
  client's programmatic retirement command from starting a second worker.
  LightLoader scans alphabetically, so the client waits up to 10 seconds when
  the broker is not yet present; incompatible brokers still fail immediately.
- The broker's ABI-5 RestartGate hook already owned the ESC site; this mod
  never duplicates it.

## Build

```powershell
cd %USERPROFILE%\Desktop\F1\f1c_ida_decompiled
python tools/f1c_mod.py validate %USERPROFILE%\Desktop\F1\development\mods\esc_pause_menu
python tools/f1c_mod.py build %USERPROFILE%\Desktop\F1\development\mods\esc_pause_menu
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

The three-option repair builds clean and passes the broker regression suite and
full canonical SDK gate. Earlier live validation proved menu discovery and
rendering, but also found that sound continued under ESC and the no-flip
retirement route went straight to results. Both paths were corrected and are
pending a new guarded race test. Exercise Continue, Retire/Return to Pits, and
Exit through a full weekend via `f1cm_guard.asi` before promotion. The retired
garage **X → simulated remainder → RACE RESULTS** path also needs live confirmation.

- [BR2256 timer investigation (2026-10-13)](BR2256_TIMER_INVESTIGATION_2026-10-13.md): confirmed first-arm uptime bug; strongest explanation for reported Esc pass-through, player uptime confirmation pending. ESC 1.1.2 telemetry incorrectly labels itself 1.1.1.

- ESC Pause Menu 1.1.4 shipped 2026-10-13: first activation after long Windows uptime fixed; usage version corrected. [Exact hashes, verification, restoration and pending gameplay](releases/v1.1.4-rc.1/RELEASE_RECORD.md).
