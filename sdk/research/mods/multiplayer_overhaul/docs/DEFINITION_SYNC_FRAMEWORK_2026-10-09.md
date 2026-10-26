# CO-OP sync framework — definition (2026-10-09, Claude; owner request, awaiting decisions)

Owner: "build a modern synchronization framework to the whole multiplayer matter, completely modernize
the multiplayer experience." Pipeline stage: **define**. Supersedes nothing; `SPEC_NETCODE_2026-10-09.md`
becomes its car-state channel (stage S2 below).

## Objective
One host-authoritative state layer ("CO-OP Sync") on top of the native session, so every PC shows the
same race: same cars, positions, grid, clock, timing, flags and results, smooth at 60 Hz.

## Principles
- Host is the single authority for shared state; clients send only their own inputs/car state.
- Native game keeps: menu + CO-OP entry, session creation/join, team select, physics of locally
  driven cars, rendering, sound. We replace *what is synchronised and how*, not the game.
- One transport (UDP, versioned binary messages, sequence numbers, reliable + unreliable channels),
  one clock (host time with client offset), one state model (snapshots + events).
- Every channel has a native fallback and a log line with rates/errors; failures degrade, not crash.

## Channels (each = own stage, own test)
| Stage | Channel | Replaces / fixes today |
|---|---|---|
| S0 | Transport + clock + diagnostics window (ping, loss, jitter, rates) | port 34500 ad-hoc UDP/TCP; no visibility |
| S1 | Session state: roster/manifest, grid order, session type/phase, start timer | manifest, grid races (P1 bug), hold/timer hacks |
| S2 | Car state 60 Hz + interpolation/extrapolation | native 52-byte records, lag/rubberband |
| S3 | Timing & scoring: laps, sectors, positions, gaps, pit stops, penalties | per-PC divergent timing |
| S4 | Race control: flags, Safety Car, retire/DNF, finish, results | per-PC flags/SC, bot retire path OPEN |
| S5 | Content + join: content sync, late join, reconnect, spectate | download flow, no late join |
| S6 | Internet: relay server (VPS) / NAT traversal, encryption | LAN/Tailscale only |

## Non-goals (for now)
Rewriting physics; replacing the native menu/session creation; anti-cheat; > 24 cars.

## Acceptance (whole framework)
LAN and internet race, host + 1..3 clients: identical grid/order/timing on all PCs; no visible
stutter at speed; 5 % loss still smooth; client drop/rejoin does not break the race; diagnostics show
rates; owner verdict "feels like a modern online racer".

## Decisions needed from owner
1. Scope: CO-OP only, or all F1C multiplayer (incl. plain MP without bots)?
2. Internet play target now (relay on VPS) or LAN/Tailscale first?
3. Build split: Claude spec+review, DeepSeek implements (CO-OP pattern) — or Claude implements?
4. Order: recommended S0 -> S1 -> S2 (fixes today's grid + lag first), then S3..S6.

## Owner direction (2026-10-09, later): own dedicated server, not bound to vanilla MP netcode
"Where people join and all the connection matters are handled that way... set up a dedicated server and not be
bound with the vanilla game multiplayer netcode, but instead run our own."

### Candidate architecture "Offline race + own server" (feasibility OPEN)
- Every player runs a native **single-player race weekend** (same season/track/settings, set by the server).
- Mod adds remote cars as vehicle slots in **controller mode 2** (remote); their state comes from our
  transport, applied through the native remote/replay apply path (`0x4F03E0` -> `0x4C73F0`), not NetComm.
- Bots run only on the host PC (or a chosen authority), shown as mode-2 cars elsewhere.
- **Dedicated server** (own program, VPS or a PC): lobby, join codes, NAT/relay, clock, session flow
  (practice/quali/race transitions, start timer), grid, timing & scoring, flags/SC, results, reconnect.
- Native F1C MP (NetComm) not used at all.

### Known risks (from evidence)
- DERIVED finding `coop-offline-race-infeasible-20261009`: offline has no NetComm transport, replication,
  collision broadcast or session sync; remote cars are fed only by NetComm. Our transport would replace all of
  that ourselves — large but not ruled out.
- Offline pauses in the garage / time warp: session clock must be server-driven.
- Collisions between local car and mode-2 cars offline: unknown.
- Mode-2 apply reads NetComm buffer 0x5F0708 via 0x463D50 — must be fed or detoured.

### Proposed first step: SPIKE (throw-away, owner yes needed)
Single-player race, one extra slot in mode 2 driven by a UDP stream recorded from a real lap (or a second PC).
Pass = car drives smoothly, visible, collides, no crash. Fail -> fall back to "native session + own channels".
