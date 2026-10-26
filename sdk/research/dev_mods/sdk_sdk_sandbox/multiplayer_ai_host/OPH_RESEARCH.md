# OPH `noPlayer` static investigation

Status: read-only static analysis of the user-supplied `oph.dll` and
`oph.ini`, 2026-08-19. This is evidence about an existing third-party helper;
it is not part of this mod and must not be copied as a fixed-address patch.

## Finding

`noPlayer=true` does **not** add AI entrants to a multiplayer session. OPH
patches the existing local network player's loading path in the F1 Challenge
process. The separate `Mutex` setting disables the one-instance guard. Together
those settings explain the commonly used "many game instances" workaround;
they do not implement a native host-side bot grid.

## Exact setting behavior

### `Mutex=true`

OPH generates three random uppercase letters and uses them to replace the
game's fixed `_F1 Challenge 99-02_APP_MUTEX_` string with a unique mutex name
in the current process. F1 Challenge therefore no longer sees another copy's
mutex and permits another game process to start. This affects only the
single-instance guard; it has no relationship to AI, season entrants, or the
network session.

### `noPlayer=true`

OPH changes two bytes in its supported F1 Challenge executable build:

1. In the local-player branch of `NetCommPlayerSlotSync`, it changes a
   Boolean argument passed to `SessionStateTransition` for the already loaded
   local player vehicle.
2. It enables the game's scoring-gate flag used by that route.

The target transition performs additional state/path bookkeeping for that
vehicle. It does not allocate a vehicle, select a driver, increment the
active vehicle count, or serialize a new entrant to connected clients.

## Evidence

- The DLL identifies itself as **OPH / Online play helper**, version 1.0.5.9,
  company **EvilClip**. Its only exports are `ip`, `port`, and `start`.
- `oph.ini` loads `[GameSettings] noPlayer` into an internal boolean.
- When true, the DLL uses `WriteProcessMemory` on the current F1 Challenge
  process twice. It changes the Boolean argument in the local branch of
  `NetCommPlayerSlotSync`, which invokes `SessionStateTransition` for an
  already existing player-slot vehicle, and toggles the scoring gate used by
  that route.
- The patched `SessionStateTransition` branch performs state/path bookkeeping
  for that existing vehicle. It does not create an entrant, select a season
  driver, increment the active-vehicle count, rebuild the active array, or run
  the vehicle/scene loader.
- By contrast, native entrant creation is visible in `ProcessDriverEntries`:
  it assigns the entry state, increments the active vehicle count, rebuilds
  the active array, and invokes the vehicle and scene loaders. OPH never
  reaches that path in its `noPlayer` implementation.

## Deeper control-flow check

- The exported `start` function performs OPH setup and launches its helper
  thread. That thread parses `noPlayer`; the setting has six static references
  total: its configuration read, UI/configuration text selection, a startup
  gate, the patch block, and a later pointer-selection branch. There is no
  second hidden `noPlayer` routine.
- The patch block is the only `noPlayer`-controlled process-write block. It
  changes an immediate Boolean argument in the local `NetCommPlayerSlotSync`
  branch, then toggles the scoring gate. A failure in either write produces
  OPH's `Failed start the noPlayer` message.
- A binary-wide sweep found no references to the game's known active-vehicle
  counter, native AI builder, incremental entrant loader, or AI-driver
  selector. It has one read of the base vehicle-array address and one
  selection between current-vehicle pointers; those support OPH's cosmetic
  and telemetry helpers, not grid construction.

## Implication for this project

OPH is useful corroboration that F1 Challenge can make an already connected
slot behave without human input. It is **not** a basis for the requested
single-process multiplayer AI grid. The native implementation must instead
populate real AI vehicles before the host publishes its session descriptor,
then prove that clients receive the same entrant list.

## Safety boundary

The original OPH writes are version-specific and bypass the SDK's validation
and game-thread safeguards. They must not be enabled, reproduced, or adapted
into this project. Any future implementation uses only a reviewed,
version-validated SDK hook and a native entrant-building route.
