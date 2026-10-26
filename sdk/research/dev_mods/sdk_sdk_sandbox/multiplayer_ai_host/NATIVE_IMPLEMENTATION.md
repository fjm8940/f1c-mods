# Native Multiplayer AI Grid — Implementation Boundary

Status: investigation/design. This document defines the requested native
implementation; it is not evidence that the DLL currently adds a driver.

Live correction recorded 2026-08-21: the separate multi-instance launcher
probe reached client Select Team over LAN, but did not confirm a host roster
entry. Its per-client `+config` isolation route crashed in `Lua.dll` and is
unsupported on the tested V10 installation. None of that launcher behavior is
a native entrant implementation. See `LIVE_EVIDENCE_2026-08-21.md`.

## Objective

On the multiplayer host, add season AI entrants as native game vehicles so
human clients see the same field. The feature must use one F1 Challenge host
process only. It must not launch, emulate, or require one F1 Challenge process
per AI driver.

## Verified static evidence

- `AIDriverCountSetup` builds standard AI entrants from the selected content.
  It writes the authoritative active vehicle count and rebuilds the active
  vehicle pointer array.
- `DynMan_Init` calls `AIDriverCountSetup`, rebuilds the active array, then
  runs the game's vehicle-scene loading loop for every active entry.
- `ProcessDriverEntries` is the native incremental entry path. It can load an
  entrant, rebuild the active array, load the vehicle scene, and apply an
  entrant state transition; it can also remove an entry.
- `NetCommPlayerSlotSync` is different: it converts real network player-slot
  records into loaded vehicle entries and removes them when those connections
  leave. AI entrants must not be fabricated as player slots.
- An earlier hypothesis treated network subtype 2 as a 418-byte general session
  descriptor. Producer/consumer recovery on 2026-08-21 disproved that role: the
  variable payload is a session-type byte, a count, and ordered multiplayer
  node IDs used for qualification/grid placement. It is not an AI entrant
  channel. See `HOST_ORDER_PACKET_RESEARCH_2026-08-21.md`.

## First deliverable: pre-session host grid

1. Detect a host multiplayer setup state using a runtime-validated session
   state, never an assumed raw address.
2. Intercept the native load-time entrant build on the game thread.
3. Ask the game to populate the requested season AI entries using the native
   entrant/vehicle initialization route.
4. Use only a separately recovered entrant synchronization path, or prove that
   stock clients independently construct the same native AI field from shared
   session configuration. The subtype-2 node-order packet is excluded.
5. Refuse to run if the executable variant, host state, function ABI, or
   selected count cannot be validated.

The next recovery task is to find the actual client-visible non-player entrant
construction path. Static work should trace backward from native host AI setup
and `MultiplayerVehiclePlacement`, looking for vehicle-count, vehicle-file, or
entrant-state transfer. It must not synthesize a client connection or insert
fake node IDs into the recovered order packet.

## Runtime evidence status

The first read-only module snapshot attempt found an existing game process but
was refused by Windows because the game runs at a higher integrity level than
the current terminal. No game memory was read or changed. Runtime mapping can
resume only when the observer and the host process run at the same integrity
level; the capture remains read-only until the descriptor layout is proven.

A subsequent elevated read-only observer captured the matching F1 Challenge
Online executable (SHA-256 `1A70E5ED...FDF3A003`) in three states on
2026-08-19: a baseline, Multiplayer Host Rules, and the resulting practice pit
garage. The Rules-to-garage delta changed 9,106 existing pages and allocated
30,117 pages. This proves a material in-process session transition, but it is
not yet enough to assign an AI-count field: the next evidence must vary one
native host-session property while holding the rest of the UI state stable.

## Later deliverable: live add/remove

Live changes are deliberately a separate phase. They require proving that the
host serializes an AI-created entrant to every current client, that late join
receives it, and that removal repairs standings, scoring, pits, and spectator
state. The existing dynamic entry path is evidence of a candidate mechanism,
not proof that it is network-safe.

## Explicit non-goals

- No extra no-player client processes.
- No reliance on the unsupported per-client `+config`/`SAVEDIR` experiment.
- No fake network identity or synthetic remote player slot.
- No direct fixed-address call or write.
- No mutation after race start until the pre-session grid is verified in a
  guarded full weekend with a remote client.

## Validation gates

1. Static recovery supplies a reviewed ABI and broker-owned game-thread hook
   for the chosen native entry point.
2. A host lobby trace validates the host state and its active vehicle list.
3. A host plus one remote client sees an identical pre-session AI list.
4. A full weekend validates AI motion, collisions, pit stops, classification,
   disconnect/reconnect, and late join without desynchronization.
