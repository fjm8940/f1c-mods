# CO-OP as an offline race instead of a multiplayer session — feasibility, 2026-10-09

Status: investigation only. No code, game, or install changed.

## Question (owner)

Could remote human players be added to a normal offline race (RACE / TEST DAY /
CHAMPIONSHIP) as a new CO-OP mode, instead of adding AI to a multiplayer
session, to avoid changing multiplayer functionality?

## Evidence

- FACT (decompile `functions/GameMode_0046d6d0.c`): front-end mode is
  `dword_7D0410`; cases 1-3 go to circuit select, 4 = championship resume,
  5 = multiplayer (client path `mpclientgrid` + `NetworkEntrantSetupWithIDCheck`).
- DERIVED (findings `ai-brain-mp-guard-session-state-3fc4-2026-10-03`):
  network session state `GameModuleManager+0x3FC4` is 0 offline; the game
  broadcasts only at 1/2 (host) and runs client sync only at 3. Only 13
  decompiled functions read it (slot sync, collision broadcast, session
  synchronizer, entrant setup, server sync).
- CONFIRMED (`ai-brain-vehicle-mode-32c8-local-vs-network-2026-10-03`): a
  remote human car is vehicle mode 2 (network car), whose state arrives only
  through the NetComm packet path; `AIMainUpdate` skips it.
- FACT (`trials/2026-09-20-22bot-mp-grid/TRIAL_RECORD.md`): current mod already
  runs 22 host-owned AI + 2 humans live on two PCs.

## Conclusion

- Offline race + remote humans: not practical. With state 0 no network code
  runs, so a remote car would have no transport, join/handshake, content
  check, state replication, collision broadcast, grid or session sync. The mod
  would have to rebuild what NetComm already does — far more change, not less.
- Premise inverted: the existing mod changes little multiplayer code. Vanilla
  netcode handles humans; the mod only adds AI entrants on the host.
- Recommended CO-OP form: a presentation layer over native multiplayer
  (mode 5). A "CO-OP" launch (Lightloader / F1CM Lobby button) starts the host
  with preset rules and the AI configurator INI, and clients with `+connect`.
- A sixth in-game menu entry is possible in principle (mode switch has a
  default branch) but menu resources/dispatch for a new mode are unrecovered;
  it would still have to enter mode 5 underneath. Lower value than a launcher button.

## Assumptions / not verified

- Single-player-only features (pause, time acceleration, championship saves)
  are likely unavailable in a multiplayer-backed CO-OP; not checked here.
- Championship-style CO-OP (points across races) would need its own season
  tracking; not investigated.

## Next

Owner decision: launcher-button CO-OP vs in-game menu entry. Before either,
live-validate build `5e12af15...` (MinHumans + callout fix).
