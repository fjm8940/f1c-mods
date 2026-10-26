# CO-OP: Pit Stop Failures sync, ESC menu without pause, SC/Red Flag off (2026-10-10/11)

Owner decisions (2026-10-10): sync Pit Stop Failures; Safety Car and Red Flag disabled in multiplayer for now;
ESC pause menu offered in multiplayer without pausing. Definition: DEFINITION_ALL_MODS_COOP_SYNC_2026-10-10.md.
Stage: implement + offline verify. NOT live-tested. Local test builds.

## Safety Car / Red Flag
No change. Both already refuse in multiplayer (Red Flag `is_multiplayer` fail-closed; Safety Car offline/race/field
guard). Evidence: COMPAT_AUDIT_SHIPPED_MODS_2026-10-10.md.

## ESC menu without pause
- Broker (f1c_ida_decompiled/mods/sdk_sandbox/game_update_broker/game_update_broker.c): `pause_menu_arm` accepts
  2 = F1C_BROKER_PAUSE_MENU_ARM_NO_FREEZE (new define in tools/sdk/f1c_broker_abi.h; ABI version unchanged, older
  clients pass 0/1). In that mode every pause-byte write goes through `menu_pause_set` (skipped) and sound is not
  suspended; menu drawing and key handling unchanged. Before-state: preserved/2026-10-10_pause_no_freeze/.
  Broker regressions PASS. Reference broker SHA-256 7D1F7A94E881868A9684CC03265767DB05496D45419FAEA3760B6A7B10A2175D
  (was pinned E0D4F68B...; Build-Test.ps1 pin NOT changed).
- Loader test build (embedded broker): dinput8.dll 92142ddb2c692252caa5d658986813d13bacedfc023b61648a0ae9290fff1533,
  built with a TEMP copy of Build-Test.ps1 that skips the loader/manager version check (loader still reports 2.2.6).
- ESC Pause Menu 1.1.2-rc.1 (development/mods/esc_pause_menu/releases/v1.1.2-rc.1, source also in the mod root;
  before-state *.BEFORE_20261010_mpmenu): mp_state != 0 arms with 2 when `[Multiplayer] MenuWithoutPause=1`
  (default 1); broker refusing 2 -> vanilla ESC in MP. DLL 71cecc349369e6e36f04b77ff279434f738f8d12f4cba4c7d114124951dde9b4.
- Risk: RETIRE TO PITS / EXIT in a network session use the same routes as single player; not tested online.

## Pit Stop Failures sync
- Pit Stop Failures 1.1.2-rc.1 (pitstop_modding/release_candidates/pitstop_failures-v1.1.2-rc.1, copy of 1.1.1 + changes):
  new `pf3_session_commit_given` (src/pit_failure_session_v3.c); live_client.cpp opens Local\COOP_PIT_BRIDGE_V1.
  Car driven on this PC (controller role 0/1) commits its own roll and sends the plan (parent, resolution, corner,
  fire, durations; reliable). A car driven elsewhere (role 2) waits up to 2 s for that plan, commits it unchanged,
  else shows a clean stop. Retirement stays with the driving PC (role 2 never retires locally).
  Build + live_tests PASS (60,000 plans). DLL 66db640d99ccf7380d428dfb951dc56211debfa772399ce77c3fb75e690a194f.
- coop_mode: MF bridge code generalised to channels (MF + Pit, wire tag PFEV1, shared peers/resends);
  INI `[CoOp] PitSync=1`. Before-state dll/coop_mode.cpp.BEFORE_20261010_pitsync. Tests PASS (ABI test now also
  checks the pitstop header copy). DLL a83ba7f43db8d9310fef8567589112b0b5818b53edc06f2f4671df205aae9db7.
- Limits: receiving PC rebuilds the plan from its own INI weights; a fault type disabled there falls back to a clean
  stop. Bot pit-failure retirement on the client is not synced (only MF retirements are).

## Installed on owner PC (game closed), backups in %TEMP%\esc_mp_install_backup
dinput8.dll (test loader), esc_pause_menu.dll + ini `[Multiplayer]`, pitstop_failures.dll 1.1.2-rc.1, coop_mode.dll,
coop_mode.ini PitSync=1. Tester A's PC needs: dinput8.dll + esc_pause_menu.dll (menu), pitstop_failures.dll +
coop_mode.dll (pit sync).
Note: launching through the Lightloader manager may reinstall its own dinput8.dll payload (2.2.6) and undo the
no-pause broker; check `f1c_game_update_broker_log.txt` / ESC log line `armed: multiplayer, no pause`.

## Pending live
1. CO-OP race: ESC opens the menu, game keeps running on both PCs; CONTINUE closes it.
2. A bot pit failure on the host appears identically on the client (pitstop_failures.log `CO-OP plan received`).
3. Tester A's own pit failure appears on the host the same way.

## Sync scope check (2026-10-11, owner question "is everything synced incl. engineer voice lines")
Live host-PC log evidence: pitstop_failures.log `Tester A: CO-OP plan received clean` and
`... received fuel_rig/temporary_no_flow` (pit plans from the client arrive). mechanical_failures.log
`COOP: events tx 560 rx 0` (host sends; nothing received - consistent with no client-side MF fault, not proven).
By design NOT synced: MF HUD/radio/engineer voice (owner-only), MF player oil trail/steam emitters outside the burst
queue, client-side retirement after a bot pit failure. A fault type disabled in the receiver's INI falls back to a clean stop.
Client-PC logs not inspected; real-time latency not measured.

## Shipped 2026-10-11: MF 1.0.6 + PSF 1.1.4 (owner "fix em all and ship")
Record: development/lightloader/v2.0.0-hub-dev.1/release_mf_1.0.6_2026-10-11/README.md. PSF 1.1.2 never showed
another PC's plan (role-2 cars dropped before the plan code) - fixed; client bot pit retire now marked on the copy;
MF shows other players' blow-up/retire on the HUD. Engineer voice stays per-driver by design.
