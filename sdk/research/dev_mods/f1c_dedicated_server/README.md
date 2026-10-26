# F1C Dedicated Server (`f1c_dedicated_server`)

Status: **define** (created 2026-10-10 on owner request). Not a release.
Owner: "the CO-OP should be multiplayer_ai_mod, and this RACE mode server should be a completely different
dedicated server project."

## Objective
Online racing for F1 Challenge V10 without the native multiplayer netcode: every player runs a native
single-player race weekend; a game-side DLL adds the other players' cars (controller mode 2) and exchanges car
state, collisions and session state with **our own dedicated server** (lobby, join, clock, sessions, grid,
timing, flags, results).

## Separation
- CO-OP (native MP + host bots) stays its own product in `development/mods/coop_mode` (owner wants it named
  `multiplayer_ai_mod`; rename pending a structural-move spec). It keeps shipping on native MP.
- This project shares no build with CO-OP. Reusable CO-OP parts (content sync, start timer, bot add sequence) are
  copied in when needed, not linked.

## Evidence so far (proof of concept, built inside coop_mode as a spike)
- Offline mode-2 car driven by our own data through native apply `0x4C73F0` — PASS (spike, 2026-10-09).
- Player collides with that car — PASS (car itself unaffected; no collision channel yet).
- Live two-PC link over the internet, UDP, ~55 packets/s each way — PASS for visibility (2026-10-10).
- Records: `../coop_mode/docs/IMPLEMENTATION_LOG_2026-10-09.md` (SPIKE/LINK sections),
  `../coop_mode/docs/IMPLEMENTATION_LOG_2026-10-10.md`, `../coop_mode/docs/DEFINITION_SYNC_FRAMEWORK_2026-10-09.md`,
  findings `offline-mode2-car-own-data-20261009`, `mp-remote-car-pipeline-20261009`.
- Reference: NASCAR Thunder 2004 built-in dedicated mode (`+dedicated`, DedicatedServer.ini, autojoin, server
  list, session voting) — feature reference only, it is native NetComm.

- Feasibility review 2026-10-11 (native `+dedicated` vs own server, NT2004 comparison):
  `docs/FEASIBILITY_DEDICATED_SERVER_2026-10-11.md`.

## Non-goals (for now)
Physics rewrite; replacing the native menu; anti-cheat; more than 24 cars.

## Acceptance (first playable)
Two to four players over the internet: join through the server, same track/session clock, smooth cars
(interpolated), collisions felt by both cars, shared grid and timing, results; re-entering from pits works.

## Owner decisions
- 2026-10-11: players host their own dedicated servers (like NT2004). Server ships as a standalone
  program anyone can run (Windows + Linux); our VPS runs only a master server list. Pending owner
  confirmation: Rust, master list on VPS, config-file format.

## Open decisions
1. Server language (recommended Rust; or C++).
2. Where the server runs (VPS vs a player's PC).
3. Migration: move the spike/link code out of coop_mode into `dll/` here (structural move spec).

## Next steps
1. Owner decisions above.
2. Specify: transport + clock + collision channel + interpolation; server MVP (lobby/join, session flow).
3. Move spike/link code here; set `[Spike]`/`[Link]` off in coop_mode.
