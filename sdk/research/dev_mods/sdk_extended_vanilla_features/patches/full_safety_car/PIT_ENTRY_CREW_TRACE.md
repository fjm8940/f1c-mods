# Pit-lane entry detection & crew activation — static trace (2026-08-03)

How F1C detects a car crossing the pit-lane entrance, and where the pit crew
comes from. Static, grounded in `functions/*.c`; the one live-only gap is
flagged.

---

## 1. The entrance is an invisible collision trigger, NOT a coordinate test

Every track's `.SCN` ships a poly `xpitin`
(`Render=False CollTarget=True MeshFile=xpitin.MTS Response=VEHICLE,PITSTOP`).
`Steward_Init` (0x4D38F0) resolves it, with four siblings, into the Steward
object at `0x8ACEE0`:

```
+0x194 XPITIN   +0x198 XPITOUT   +0x19C XFINISH   +0x1A0 XSECTOR1   +0x1A4 XSECTOR2
```

Because the same five polys ship on every circuit, pit-entry behaviour is
identical everywhere. (Prior art: finding
`f1c-pit-exit-red-light-is-scn-collision-trigger`.)

## 2. The collision routes by zone code

When the car overlaps a pit trigger poly, the physics/collision engine calls
`AIStateMachineWithModeDispatch` (0x4A5D10) with the poly as `a2`. It reads the
poly's zone code into `this[97]`:

```
this[97] == 2  ->  sub_4A5EA0   (pit-lane ENTRANCE)
this[97] == 0  ->  sub_4A5F20   (pit-lane EXIT)
```

gated on the car's pit-state being 2 or 3 (`v4[3250]`).

## 3. The entrance handler commits the pit — `sub_4A5EA0` (0x4A5EA0)

Once-only, guarded by the entry flag `veh+0x344C` (`this[384]`):

```c
if (!this[384]) {
    StateChangeNotify(this, 2);          // pittingStatus veh+0x3450  0 -> 2
    this[384] = 1;                        // entry flag  veh+0x344C  0 -> 1
    AISessionPhaseChange(dword_5EB978, veh);  // pit-entry RADIO line (not crew)
    sub_4BC240(veh + 16436);              // session/graphics helper (not crew)
    // dword_8ACFC8 == 2 -> "Entering pits under [red/yellow]" penalty check
}
```

`StateChangeNotify` (0x4A5C80) is the field writer: on the 0/1 -> 2 edge it
stamps `veh+0x3454` and `veh+0x3458` (`this+392`/`this+396`) with `SessionTime`
(`dbl_8AFEA0`) and pushes a `ProgressBarEventEncode(6,...)` event.

**This exactly reproduces five of the six §5c pit-line-entry fields**
(pittingStatus 2, entry flag 1, the two timestamps; the limiter `maxSpeed`
22.222 and the `0x1068` bit 8 are written on the same edge elsewhere in the
physics step). So the six fields are the *symptom* of this handler firing — which
is why writing them by hand does not reproduce the entry (finding
`f1c-synth-pit-entry-by-teleport-rejected`): the game re-runs its own trigger
logic and overwrites them.

`AISessionPhaseChange` (0x41A2B0) is COMMENTARY only — it emits driver-table
eids 21/22 (session 5) or 178/179 (race) = the pit-entry radio call. Not crew.

## 4. The state machine then drives the stop — `PitStateUpdateMachine` (0x4A6E50)

Runs per frame on the pit-state object `veh+0x32CC` (dispatched by
`VehiclePhysicsModeDispatcher` 0x4E4930 on the player/AI discriminator). Field
map on that object: `+388` pittingStatus (0x3450), `+392` entryTime (0x3454),
`+396` (0x3458), `+400` serviceReady latch (0x345C), `+384` entry flag (0x344C).

```
state 2 (in lane): when VehicleProximityCollisionDetection(0x4A5FA0) is true
                   -> serviceReady = 0; StateChangeNotify(3)  (stopped in box)
                   -> PitStopReadyCheck (0x4A6200) writes stopEndTime veh+0x3470
state 3 (in box):  PitStopProgress (0x4A68B0) counts down to stopEndTime
```

`VehicleProximityCollisionDetection` (0x4A5FA0) is "am I stopped in MY box":
it compares the car's position to the box marker `dword_5EA2C0[ veh+0x48A4 ]`
(the AIW `[PITS]` TeamIndex), requiring distance < 5 m, longitudinal alignment
and low speed. So the box is found from `veh+0x48A4`, matching the FACT already
in the corpus.

## 5. The crew

The crew is **instantiated at track/vehicle load**, from the `.SCN`, not at the
entrance:

```
VehgenScnFileLoader (0x4C6C40)
  -> VehicleGraphicsSceneLoader (0x4DFCE0)
     -> sub_4E2A60 (0x4E2A60)
        -> PitCrewInstanceSetup (0x4DDAD0)   // 34 actors, hung at veh+0x32D4
```

(Also reached via `VehicleSceneLoadAndPositionTires` 0x4681B0 → `ZeroInitWrapper`
0x4DDF70 → PitCrewInstanceSetup.) The 34 actors sit idle by the box for the
whole race. Layout (verified live, prior art): `veh+0x32D4`→crew, `crew+0x0C`
actorCount, `crew+0x10` actorArray stride 120, `actor+112/+116`
channelCount/array stride 28, `channel+12/+16` (baseStart, baseEnd).

What the player perceives as the crew "spawning" is the crew being made
**visible + animating** — driven by the crew animation state machine
(`PitCrewStateMachine` 0x4BBFC0, `PitCrewAnimation*` 0x4A2A90/0x4A3910,
`RangeProportionInitializer` 0x4A1FD0), which reads the associated car's pit
state and runs the `.ANM` animations, positioned at the box via `veh+0x48A4`.
These are **vtable-driven** (indirect calls), which is why static xref finds no
direct callers.

## OPEN (live-only): which pit-state edge makes the crew visible

Static gets the trigger chain but cannot settle whether the crew flips from idle
to visible/animating on **state 2 (entrance)** or **state 3 (in box)**. The
driver reports seeing the crew at the entrance, which points at state 2 / the
`0x344C` entry flag, but confirming it needs a live hook on the crew update
(Frida on 0x4A1FD0, reading the associated car's `veh+0x3450`, as the
2026-07-29 crew-animation work did) OR a poll of the crew actor array against
pittingStatus through a real stop. NOTE: the Frida MCP bridge unloads scripts
between calls, so use a standalone Frida script or the poll route.

---

## Function index

| Addr | Name | Role in the chain |
|---|---|---|
| 0x4D38F0 | Steward_Init | resolves XPITIN/OUT/FINISH/SECTOR polys into 0x8ACEE0 |
| 0x4A5D10 | AIStateMachineWithModeDispatch | routes pit-trigger collision by zone code |
| 0x4A5EA0 | sub_4A5EA0 | ENTRANCE handler: state->2, entry flag, radio |
| 0x4A5F20 | sub_4A5F20 | EXIT handler: state->0, pit-count++, "exiting pits" |
| 0x4A5C80 | StateChangeNotify | writes pittingStatus + entry timestamps on edges |
| 0x4A6E50 | PitStateUpdateMachine | per-frame pit state advance (obj veh+0x32CC) |
| 0x4A5FA0 | VehicleProximityCollisionDetection | "stopped in my box" (keyed veh+0x48A4) |
| 0x4A6200 | PitStopReadyCheck | writes stopEndTime veh+0x3470 |
| 0x4A68B0 | PitStopProgress | counts down the stop |
| 0x41A2B0 | AISessionPhaseChange | pit-entry radio commentary (eids 21/22/178/179) |
| 0x4DDAD0 | PitCrewInstanceSetup | builds the 34 crew actors at load |
| 0x4DFCE0 | VehicleGraphicsSceneLoader | load-time crew scene setup caller |
| 0x4A1FD0 | RangeProportionInitializer | per-frame crew animation proportion (vtable) |
| 0x4BBFC0 | PitCrewStateMachine | crew animation state (vtable) |
