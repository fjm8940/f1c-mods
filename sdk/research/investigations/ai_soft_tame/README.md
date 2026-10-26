# AI Soft Tame — park a driver without retiring him

Live-proven 2026-08-04 (Monza 1999, test-day session). Two holds + one teleport
recipe. All offsets vehicle-struct (base 0x5EC6D8, stride 0x48C0, resolve the
car by name at +0x4620 on every sample).

## Recipe 1 — synthretire WITHOUT the retired flag (frozen, not retired)

Freezes the car solid (position bit-stable), engine/state untouched by us,
**RetiredFlag stays 0**, model visible, reversible.

```
veh+0x4064 (FinishState)   = 2
veh+0x45F4 (AIControlFlag) = 0
veh+0x4040 (PitState)      = 2
veh+0x4410 (RetiredFlag)   = 0   <- explicitly NOT retired
```

- The game stops driving the car because FinishState=2 marks it finished;
  the "AIControl=0 on a live car -> game retires it" reaction is suppressed by
  the FinishState, so ret=0 holds indefinitely (measured 19 s, zero drift).
- The car coasts to a halt first (from 48 m/s it travelled ~4 m) then freezes.
- Speed field veh+0x4440 goes STALE (frozen at the apply value) - judge motion
  by position (veh+0x34E0), never by speed.
- Release: restore 0x4064=0, 0x45F4=1, 0x4040=0 and the AI resumes driving.

## Recipe 2 — armed physics-off (0x32C8=2)

Alternative hold, keeps the AI's machinery running but physics off.

```
write veh+0x32C8 = 2  at 20 Hz (re-assert - a one-shot write REVERTS)
```

- Held 30 s flat: position bit-stable, RPM frozen ALIVE at idle (engine runs),
  no retirement (mode-2 is exempt from the stopped-car retire model).
- VehiclePhysicsModeDispatcher mode-2 path runs only the dt store - no physics
  integration, no pit machine, and AIMainUpdate returns early for mode != 1.
- FLAW: a collision (player hits the car) makes it jitter - no physics to
  resolve the impact. Recipe 1 does not have this flaw (physics is on, the car
  is simply not driven).
- Release: stop arming, or write 1.

## Recipe 3 — teleport the FROZEN car (Recipe 1 state)

A frozen (synthretire) car accepts a body-position write and keeps it - the
freeze means nothing re-asserts the position. This is the exception to the
"stationary car is pinned" rule.

```
veh+0x34E0/0x34E4/0x34E8 = target XYZ   (authoritative body position)
veh+0x58/0x5C/0x60       = 0            (zero the stale velocity)
```

LIVE 2026-08-04: Zonta (frozen via Recipe 1, RetiredFlag=0) teleported into
the pit lane (lane node -7.80, -1.87, 79.43), held indefinitely, not retired.

## Hazard (do not repeat)

`veh+0x4354` (curWaypoint) raw write on the PLAYER slot = instant process
crash (the field reads as a qword pair with +0x4358; a 4-byte write mixes the
pair into a bad pointer). AI cars accept node hands (wp1135 proven) - the
player slot does not.

## Dead end (for the record)

Forcing pedals from outside does NOT work via the vehicle-struct floats:
veh+0x357C (throttle) / 0x3580 / 0x3584 (BrakeFactor) are AI-DECISION MIRRORS,
not the physics-consumed inputs. Armed throttle=0 + brake=1.0 at 200 Hz: the
car accelerated anyway (56->76 m/s). The physics reads pedals from a packed
input object (a2 in VehiclePhysicsTimeStep 0x4C73F0: bits at a2+52/+56/+59);
the player's input object = 0x2AB000; the AI cars' input objects are not yet
mapped. Finding them = breakpoint on the per-car physics path (sub_4EE2D0 /
PerVehicleCameraUpdateIterator) and grab the input pointer.

## Release from the hold (live 2026-08-04)

stopEnd(0x3470) past + tExit(0x4534) = 0 -> pit 3->4 -> the game's own cmd-15
lane exit -> clean merge (Barrichello, Monza, trace-clean: lane-curve X
tracking, 1->21 m/s through the lane, then racing). Full recipe:
freeze -> teleport to own box (AIW PitPos + PitOri yaw) -> unfreeze + hold set
(pit=3, svc=1, stopEnd future, tExit future, limiter, laneMode=1, cb=1, e1=1,
bit8) -> hold -> release. NOTE: the crew pin (zero drift) is state-dependent -
Hakkinen bit-stable 60 s, Barrichello crept 1.5 m/s. See HANDOFF_2026-08-04b.
