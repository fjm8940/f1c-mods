# Multiplayer AI prototype v1 — guarded live-test record

Date: 2026-08-21  
Status: **built, statically verified, not live-validated**  
Distribution status: **sandbox only**

## Objective

Prove one useful vertical slice:

1. a human host and a human client join the same normal multiplayer session;
2. the mod adds one AI vehicle during each process's normal track-load pass;
3. stock multiplayer slot synchronization does not delete that AI;
4. the AI appears and drives for both humans.

This prototype intentionally installs the same DLL and configuration on every
participant. Untouched-client compatibility is not part of this gate.

## Mechanism

- `DynMan_Init` brackets the game's track-load field construction.
- On the first `VehgenScnFileLoader` call inside that bracket, the DLL appends
  one inactive vehicle slot using reviewed roster/identity/state functions,
  marks it as AI, increments `VehicleCount`, and rebuilds the active array.
- The game's own remaining vehicle loop loads its mesh and physics.
- Around `NetCommPlayerSlotSync`, the DLL temporarily parks only the slot whose
  roster identity it owns. Stock cleanup therefore ignores it. The DLL restores
  the AI role and active array before returning to the game tick.
- If the slot's roster identity changes, protection is disabled rather than
  overwriting a human entrant.

No custom vehicle-state replication exists yet. Every participant simulates the
same AI locally. The live test decides whether this is sufficiently stable or
whether host correction packets are required next.

## Safety gates

- `EnableDistributedAI=0` is the shipped sandbox default.
- The exact canonical executable fingerprint and exact whole-instruction hook
  bytes must match or the DLL refuses to install.
- A missing vehicle path, full field, non-inactive target slot, changed roster
  identity, or hook conflict fails closed.
- The DLL does not modify the executable or either game installation.
- Join all human players before loading the track. Late joining is outside v1.
- Use a full guarded race weekend; never QuickRace.

## Test preparation

For a deliberate throwaway test only, use identical files on both processes:

```ini
[Mod]
Enabled=1

[Prototype]
EnableDistributedAI=1
VehiclePath=SEASONDATA\VEHICLES\BAR\2001_BAR003\2001_bar09.veh
```

The selected `.VEH` must exist in every participant's installation. Both
processes must use the same season, track, game data, DLL, and INI.

## Required evidence

Capture separately for host and client:

- `multiplayer_ai_host_log.txt` containing `prepared distributed AI`;
- the same AI driver name and slot on both processes;
- pre-race grid/standings showing both humans and the AI;
- at least two minutes of green-flag running with the AI moving on both screens;
- standings position/lap/distance snapshots at 30-second intervals;
- any disconnect, slot-change, crash, or visible divergence.

## Pass and abort conditions

Pass v1 only if both humans see and can race the AI for two continuous minutes
without a crash, human-slot replacement, repeated join/leave spam, or obvious
AI position divergence.

Abort immediately if a human vehicle is renamed/replaced, the field count grows
more than one, the AI is meshed at the origin, either process crashes, or the
two views place the AI in materially different locations. Preserve all logs and
captures. A divergence result advances the design to host correction; it does
not invalidate load-time AI construction.
