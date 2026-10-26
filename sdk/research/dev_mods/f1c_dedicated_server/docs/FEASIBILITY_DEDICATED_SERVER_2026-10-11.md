# Dedicated server feasibility (multiplayer / CO-OP), 2026-10-11

Status: investigation only. No code, game, install or production changed.
Pipeline stage: investigate (product stage stays `define`).

## Question (owner)
Is a dedicated server feasible for multiplayer / CO-OP? NASCAR Thunder 2004 (NT2004) has one; use it as reference.

## Evidence

### NT2004 reference (static, `research/reverse_engineering/nt2004_ida_decompile/strings_index.json`)
- FACT: `0x465920` (NetComm.cpp) builds `"%s +dedicated"` plus ` +gamename "` / ` +password "` and relaunches
  the game's own exe ("cannot launch dedicated server" errors). The dedicated server is the same game process, not a
  separate binary.
- FACT: `0x527B50` (vidman.cpp) forces a windowed video mode for the dedicated server. It still opens D3D. Not headless.
- FACT: `0x468AD0` reads `DedicatedServer.ini`; `0x469050` writes it (`[TRACKS]` rotation, RaceLength,
  DamageMultiplier, AllowAutoPit/Shift/Clutch/Stability/Invulnerable/DupVehicles...).
- FACT: `0x467E40` / `0x468080`: player voting ("advance to next session/event", "Vote passed", "Cannot advance
  session past race"). UI buttons `APP_Button_Dedicated_CallVote_*`, `APP_Button_Multiplayer_HostDedicated`.
- DERIVED: NT2004 dedicated = native NetComm host with no local driver + config file + track rotation + votes.
  Same netcode, same limits as a normal host.

### F1C (`F1 Challenge Online.exe` strings + `functions/NetCommParseCommandLine_00463160.c`)
- FACT: `NetCommParseCommandLine` 0x463160 checks `dedicated`, but only as `if (!dedicated) { +host / +connect }`.
  No dedicated flag is stored. `+dedicated` on F1C therefore just disables `+host`/`+connect`.
- FACT: remnants present: strings `+dedicated`, `+gamename "`, `+maxplayers ` at file offsets 0x1C64A8/0x1C6474/
  0x1C6350 (same set NT2004's relaunch builder uses); PLR options `Dedicated Target Rate` (128, 32..1024) and
  `Server Session End` ("session is advanced ...", default 300 s) in `PlayerOptionsInitializer_004aa270.c:1267-1270`;
  GameSpy heartbeat/query strings.
- FACT: absent in F1C: `DedicatedServer.ini`, vote strings, `HostDedicated` button.
- OPEN: whether F1C still contains the relaunch builder function referencing 0x5C64A8 (not in recovery index).
- Correction: `f1c_ida_decompiled/mods/sdk_sandbox/multiplayer_ai_host/Start-F1CDedicatedAIHost.ps1` comment says
  `+dedicated` "starts the server code path". On F1C it does not; it suppresses host/connect. That wrapper should
  not be relied on (it is already labelled "not a proven headless server" in its README).

### Own server (prior evidence, see README)
- PASS spike: offline mode-2 car driven by own data via native apply 0x4C73F0; player collides with it;
  live internet UDP link ~55 Hz (coop_mode logs 2026-10-09/10).

## Options

A. **Native dedicated host (NT2004 style) on F1C.** A game process hosts with `-noplayer`, no local driver,
   auto session advance (native `Server Session End` exists), DLL adds INI rotation/votes.
   - Feasible: yes, moderate effort; F1C keeps most server options.
   - Limits: still native NetComm (slot+1 grid, send rate, port forwarding on the host, AI only on host).
     Needs a full game + D3D window, so a Linux/headless VPS cannot run it; a Windows box/VM with GPU or WARP is
     needed (unverified). It only fixes "host must also play", not netcode quality.

B. **Own lightweight server (this project's plan).** Server runs no physics: lobby/join, clock, session flow,
   grid, timing, flags, results, relays car state. Each player runs offline race; DLL injects remote cars (mode 2).
   - Feasible: yes, core transport proven. Server is small (Rust/C++), runs on the existing Linux VPS.
   - Hard/unproven parts: (1) collision channel so both cars react; (2) aligning offline session clock/start
     across PCs; (3) grid and starting order inside an offline weekend; (4) pit/re-entry and retire state;
     (5) AI cars: none server-side, so CO-OP-with-AI needs one client as AI authority or no AI.

## Recommendation
- Multiplayer RACE (humans): option B. Next unknown to retire first is the collision channel, then clock/start.
- CO-OP with AI: keep native MP (coop_mode). Option A only if owner wants an always-on host box; low priority.
- Do not build on F1C `+dedicated`; it does nothing useful natively.

## Not verified / pending
- No live test this task. Headless or WARP run of F1C not tested.
- Owner decisions still open in README: server language, server location, spike code migration.
