#!/usr/bin/env python3
"""
pitbox_hold_release.py - hold a car in its pit box with the GAME'S OWN
machinery, then let the game run its own exit.

WHY THIS AND NOT A FREEZE
-------------------------
The synthretire freeze holds a stationary car, but it is not a pit stop and the
game will not release it. Worse, applied to a car doing 45 m/s it does not even
hold: measured 2026-08-03, he drifted 28 m out of the box.

The exit sequence is now sourced rather than guessed. PitStopProgress_004A68B0:

    if (SessionTime < *(float*)(this+420))      // veh+0x3470 stopEndTime
        ... still being serviced ...
    else {
        StateChangeNotify(this, 4);                       // pit state 4
        ...
        AICommandMessageQueueDispatcher(*(int**)this, 15, 0);
    }

and command 15, handled inside sub_424CD0 itself:

    SessionCleanupFlagClear(this)   // re-init the AI's route state
    this[4433] = 0                  // veh+0x4544
    ParameterValueSetter(18)        // maxSpeed = 22.222, the pit limiter
    this[4436] = 1                  // veh+0x4550 laneMode = the PIT LANE set
    *((BYTE*)this+17736) = 1        // veh+0x4548

So laneMode is an OUTPUT of the release, not an input - which is why writing it
by hand reverted within a tick and the car then drove the MAIN path from inside
the pit area, straight at the wall.

Therefore: to hold him, push stopEndTime into the future. To release him, put
stopEndTime in the past and let the game's own code fire command 15. Nothing is
forged either way.

USAGE
    python tools/pitbox_hold_release.py hold <track_key> [--secs N]
    python tools/pitbox_hold_release.py release [--secs N]
    python tools/pitbox_hold_release.py state

WRITES TO THE RUNNING GAME.
"""

import math
import struct
import sys
import time

sys.path.insert(0, __file__.rsplit("\\", 1)[0])
from pitbox_route_test import (Mem, veh, find_by_name, aiw_records,   # noqa: E402
                               SC_NAME, OFF_BODY, OFF_YAW, OFF_SPEED,
                               OFF_PITTING, OFF_LANEMODE, OFF_CURWP,
                               OFF_PITTEAM, OFF_RETIRED, OFF_ACTIVE2,
                               OFF_FINISH, OFF_RWFLAG, OFF_CALLBOX,
                               OFF_DRIVERVIS, OFF_RPM, OFF_ANGVEL,
                               SESSION_TIME)

OFF_STOPSTART = 0x346C
OFF_STOPEND   = 0x3470
OFF_MAXSPEED  = 0x4538
BOX_FORWARD_M = 2.33
BOX_UP_M      = 0.227


def sess(m):
    return struct.unpack("<d", m.rd(SESSION_TIME, 8))[0]


def box_target(track, team):
    import json, io, os
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                     "aiw_pits.json")
    d = json.load(io.open(p, encoding="utf-8"))
    entry = d.get(track) or d[[k for k in d if k.lower() == track.lower()][0]]
    t = entry["teams"][str(team)]
    px, py, pz = t["pit_pos"]
    yaw = t["pit_ori"][1]
    return (px + math.sin(yaw) * BOX_FORWARD_M, py + BOX_UP_M,
            pz + math.cos(yaw) * BOX_FORWARD_M, yaw)


def show(m, i, t0, p0):
    v = veh(i)
    p = struct.unpack("<3f", m.rd(v + OFF_BODY, 12))
    print("  +%4.1fs (%9.2f,%7.2f,%9.2f) spd %6.2f  pit %d lane %d "
          "maxSpd %8.1f  stopEnd %8.2f  now %8.2f  moved %6.1f m"
          % (time.time() - t0, p[0], p[1], p[2], m.f(v + OFF_SPEED),
             m.i(v + OFF_PITTING), m.i(v + OFF_LANEMODE),
             m.f(v + OFF_MAXSPEED), m.f(v + OFF_STOPEND), sess(m),
             math.hypot(p[0] - p0[0], p[2] - p0[2])))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    secs = 20
    if "--secs" in sys.argv:
        secs = int(sys.argv[sys.argv.index("--secs") + 1])
    if not args:
        print(__doc__)
        return 1
    mode = args[0]

    m = Mem()
    i = find_by_name(m, SC_NAME)
    if i < 0:
        print("safety car not in the field")
        return 1
    v = veh(i)
    print("safety car: slot %d   SessionTime %.2f" % (i, sess(m)))

    if mode == "state":
        p = struct.unpack("<3f", m.rd(v + OFF_BODY, 12))
        print("pos (%.2f, %.2f, %.2f) spd %.2f pit %d lane %d maxSpeed %.1f"
              % (p[0], p[1], p[2], m.f(v + OFF_SPEED), m.i(v + OFF_PITTING),
                 m.i(v + OFF_LANEMODE), m.f(v + OFF_MAXSPEED)))
        print("stopStart %.2f  stopEnd %.2f  now %.2f"
              % (m.f(v + OFF_STOPSTART), m.f(v + OFF_STOPEND), sess(m)))
        return 0

    if mode == "hold":
        if len(args) < 2:
            print("hold needs a track key")
            return 1
        team = m.i(v + OFF_PITTEAM)
        x, y, z, yaw = box_target(args[1], team)
        now = sess(m)
        # Put him in the box, stopped, and tell the pit machinery it is
        # servicing him until further notice. The machinery holds the car -
        # that is its job - so nothing here has to fight the physics.
        m.wr(v + OFF_BODY, struct.pack("<3f", x, y, z))
        m.wf(v + OFF_YAW, yaw)
        m.wr(v + 0x58, struct.pack("<3f", 0.0, 0.0, 0.0))
        m.wb(v + OFF_RETIRED, 0)
        m.wb(v + OFF_ACTIVE2, 1)
        m.wb(v + OFF_DRIVERVIS, 1)
        m.wi(v + OFF_FINISH, 0)
        m.wb(v + OFF_RWFLAG, 1)
        m.wi(v + OFF_CALLBOX, 0)
        m.wi(v + OFF_PITTING, 3)          # being serviced
        m.wf(v + OFF_STOPSTART, now)
        m.wf(v + OFF_STOPEND, now + 3600.0)
        m.wf(v + OFF_MAXSPEED, 22.2222)
        print("held: pit 3, stopEnd = now + 3600 s, in the box (%.2f, %.2f, %.2f)"
              % (x, y, z))
    elif mode == "release":
        # THE GAME'S OWN RELEASE. Putting the deadline in the past makes
        # PitStopProgress take its else-branch on the next tick: pit state 4,
        # then AICommandMessageQueueDispatcher(veh, 15, 0), which re-inits the
        # route, applies the limiter and selects the pit-lane waypoint set.
        m.wf(v + OFF_STOPEND, sess(m) - 0.5)
        m.wf(v + OFF_ANGVEL, 1500.0)
        m.wf(v + OFF_RPM, 1500.0)
        print("released: stopEnd set into the past - the game should issue "
              "command 15 itself")
    else:
        print("unknown mode %r" % mode)
        return 1

    p0 = struct.unpack("<3f", m.rd(v + OFF_BODY, 12))
    t0 = time.time()
    print("\nwatching %d s" % secs)
    while time.time() - t0 < secs:
        j = find_by_name(m, SC_NAME)
        if j < 0:
            print("  lost him")
            break
        show(m, j, t0, p0)
        time.sleep(1.0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
