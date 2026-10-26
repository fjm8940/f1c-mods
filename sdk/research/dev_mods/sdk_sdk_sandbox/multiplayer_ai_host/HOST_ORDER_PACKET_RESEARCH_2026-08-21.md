# Host order packet recovery — 2026-08-21

Status: static raw-code evidence. Function names remain unreviewed SDK leads;
addresses identify the tested executable only and are not an implementation
contract.

## Correction

The network subtype-2 payload previously described as a 418-byte session
descriptor is not an AI-roster or entrant descriptor. Static producer and
consumer recovery show that it carries the host's ordered multiplayer node-ID
list for qualifying/grid placement.

This closes one candidate synchronization route. It does not show how native AI
entrants are serialized to clients.

## Producer

`SessionUIElementRenderer_0045f2b0.c` constructs network subtype 2:

- When `dword_7C56A4 == 7`, it writes the session-type byte first.
- It reads the entry count from the network object's offset `0x3FD9`.
- It copies `count * 4` bytes from offset `0x3FDA`.
- It encodes a payload length of `count * 4 + 2` and sends it through
  `NetworkSendDispatch`.

`ChampionshipStandingsReOrdering_00462a10.c` builds the ordered values. It
sorts active player records and writes their node IDs into the array ending at
offset `0x3FDA`, then stores the final count at `0x3FD9`.

`NetworkUIUpdate_00462848.c` calls the producer after its guarded update path.
The recovered call graph does not yet expose a conventional caller for this
usercall function, so its complete scheduling/ownership ABI remains unreviewed.

## Consumer

`NetCommSessionMessageDispatcher_0045d890.c` handles subtype 2 only while the
network object is in state 3. It copies the payload region into the same object
area beginning at `0x3FD8` and performs the related state update.

`sub_462E20_00462e20.c`, called by `MultiplayerVehiclePlacement`, consumes the
count and ordered ID array. It compares each received ID with the local
`nodemap`, assigns `plannedPositionAtStart`, and falls back to last position if
the local node ID is absent. Its retained debug strings explicitly describe
the comparison as `NODEIDs` and the array as server qualification order.

## Consequence for native multiplayer AI

- The packet synchronizes ordering for player-shaped network nodes already in
  the session.
- It does not carry driver, team, vehicle-file, AI-controller, or native entrant
  records.
- Adding AI node IDs to this array would fabricate ordering references without
  creating vehicles and would repeat the synthetic-player-slot mistake.
- The native investigation must find a different host-to-client path that
  establishes non-player entrants, or prove that stock clients independently
  construct an identical AI field from shared session configuration.

## Next evidence target

Trace backward from native host AI creation and `MultiplayerVehiclePlacement`
to find where clients acquire the active non-player vehicle list. Candidate
evidence must correlate vehicle count, vehicle-file identity, or entrant state;
the subtype-2 node-order packet is now excluded.
