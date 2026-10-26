# Multiplayer AI Host — Validation Plan

Status: development prototype.

The multi-instance `noPlayer` launcher is retained research and is outside this
native acceptance gate. A 2026-08-19 live probe reached client Select Team over
LAN but did not confirm a host roster entrant; its isolated `+config` route
crashed in `Lua.dll`. See `LIVE_EVIDENCE_2026-08-21.md`.

## Objective

Allow human clients to join an F1 Challenge multiplayer host whose requested
season AI entrants were created natively inside the host process.

## Non-goals

- Change driver, vehicle, track, or season files.
- Add or remove AI entrants after a race has started.
- Replace F1 Challenge networking or simulate AI on clients.
- Claim compatibility between mismatched mod/season installations.

## Acceptance criteria

1. One regular or dedicated host process creates the multiplayer session and
   constructs the requested AI roster before publishing the session. Starting
   a `-noplayer` process alone does not satisfy this criterion.
2. At least one remote human client joins without a session-content mismatch.
3. A recovered host-to-client entrant mechanism, distinct from the subtype-2
   node-order packet, gives the remote client the same AI count and entrant
   identities as the host.
4. The host creates the requested AI grid without launching any additional F1
   Challenge process or manufacturing a network player slot.
5. AI cars are visible and moving for both host and client for a full race
   weekend; the human client retains normal steering/control.
6. AI collisions, pit stops, retirement/scoring, a client reconnect, and the
   end-of-session result screen do not disconnect or desynchronize either peer.

## Test discipline

Use two non-production game copies with identical executable hash, season,
track, and vehicle content. Preserve the exact launcher script and game
executable hashes in the test record. Do not use QuickRace. Record failures
without overwriting logs or artifacts.

Do not use the custom `+config`/isolated-`SAVEDIR` prototype in this validation
unless a separate repair investigation first produces an independently passing
V10 runtime test. It is not part of the native host design.

For entrant-path recovery, capture a read-only process baseline, Host Rules,
and the garage state using the same executable process; then repeat one
controlled host-session property change to eliminate ordinary UI and loading
allocations. Do not classify the subtype-2 node-order packet as entrant data;
`HOST_ORDER_PACKET_RESEARCH_2026-08-21.md` recovers its narrower purpose.

## Abort conditions

Stop the trial if a client sees AI positions diverge, AI cars freeze/teleport,
the session disconnects, or either installation reports content mismatch. Such
a failure means the next task is network-authority investigation, not a retry
with changed game files.
