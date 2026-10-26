#!/usr/bin/env python3
"""
pitbox_route_test.py - can a car PLACED in its pit box be given the game's own
route out of it?

THE QUESTION
------------
Starting the safety car parked in his box works; RELEASING him from there never
has. Six attempts failed, and the measurement that closed them was a byte diff
against a vanilla parked car: the difference is not in the vehicle struct. The
conclusion drawn at the time was that the route state lives in an unmapped AI
heap child.

That conclusion was too pessimistic. `curWaypoint` (veh+0x4354) is a POINTER to
a runtime waypoint node, and those nodes are one contiguous array in .AIW record
order:

    node = base + index * 316          (stride measured from 23 live cars,
                                        every pointer delta a multiple of 316)

    wp 1212 @ 0x28887C9C  pos (10.497, 0.403, 506.505)  lapdist 5700.915
    .AIW    wp 1212       pos [10.497, 0.4027, 506.505] score  5700.915

and the .AIW gives a node PER PIT BOX: branch 106 + AIW team index, i.e. the
node a car that drove in would be holding. So the route can be handed over
rather than forged.

WHAT THIS WRITES (slot resolved BY DRIVER NAME, every sample)
-------------------------------------------------------------
    un-freeze     retired 0, FinishState 0, active 1, RWflag 1, callToBox 0,
                  driver model visible, driverStatus 3
    pit state     pittingStatus 4, laneMode 1      (laneMode reverts within a
                                                    tick while pitting is 0)
    THE ROUTE     curWaypoint = the car's own pit-box node
    engine        EngineAngVel + EngineRPM = 1500  (the revive needs the crank;
                                                    without it the AI steers a
                                                    dead engine)

then watches position for N seconds and says whether he moved.

The base address is derived live, never hardcoded: it is taken from another
car's curWaypoint and that car's own .AIW index, found by matching the node's
lap distance against the file.

USAGE
    python tools/pitbox_route_test.py <track_key> [--secs 20] [--dry]
    python tools/pitbox_route_test.py 1999_Monza

WRITES TO THE RUNNING GAME.
"""

import ctypes
import ctypes.wintypes as wt
import io
import math
import os
import re
import struct
import subprocess
import sys
import time

EXE     = "F1 Challenge Online.exe"
VEH     = 0x5EC6D8
STRIDE  = 0x48C0
SLOTS   = 104
NODE_STRIDE = 316

OFF_ACTIVE   = 0x32C8
OFF_PITTING  = 0x3450
OFF_BODY     = 0x34E0
OFF_YAW      = 0x34F0
OFF_RPM      = 0x39A0
OFF_ANGVEL   = 0x00A0
OFF_RWFLAG   = 0x0D58
OFF_CURWP    = 0x4354
OFF_FINISH   = 0x4064
OFF_RETIRED  = 0x4410
OFF_ACTIVE2  = 0x45F4
OFF_DRVSTAT  = 0x4510
OFF_CALLBOX  = 0x454C
OFF_LANEMODE = 0x4550
OFF_SPEED    = 0x4440
OFF_NAME     = 0x4620
OFF_PITTEAM  = 0x48A4
OFF_DRIVERVIS = 0x105D
OFF_PITSTATE = 0x4040
SESSION_TIME = 0x8AFEA0

GAMEDIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "..",
    "F1 Challenge V10", "SeasonData", "Circuits")
SC_NAME = "Bernd Maylander"


def aiw_records(track_key):
    path = None
    for root, _d, files in os.walk(GAMEDIR):
        for f in files:
            if f.lower() == track_key.lower() + ".aiw":
                path = os.path.join(root, f)
    if path is None:
        return None
    txt = io.open(path, encoding="latin-1", errors="replace").read()

    def nums(s):
        out = []
        for tok in s.split(","):
            try:
                out.append(float(tok))
            except ValueError:
                pass
        return out

    recs, cur = [], None
    for line in txt.splitlines():
        line = line.split("//")[0].strip()
        m = re.match(r"(wp_\w+)=\((.*)\)", line)
        if not m:
            continue
        k, v = m.group(1), m.group(2)
        if k == "wp_pos":
            if cur is not None:
                recs.append(cur)
            cur = {}
        if cur is None:
            continue
        cur[k] = nums(v)
    if cur is not None:
        recs.append(cur)
    return recs


class Mem(object):
    def __init__(self):
        out = subprocess.check_output(
            ["tasklist", "/FI", "IMAGENAME eq " + EXE], text=True, errors="replace")
        pid = None
        for line in out.splitlines():
            if EXE.lower() in line.lower():
                for tok in line.split():
                    if tok.isdigit():
                        pid = int(tok)
                        break
        if pid is None:
            raise SystemExit("game is not running")
        self.k = ctypes.WinDLL("kernel32", use_last_error=True)
        self.h = self.k.OpenProcess(0x1F0FFF, False, pid)
        if not self.h:
            raise SystemExit("cannot open the process")

    def rd(self, addr, n):
        buf = (ctypes.c_char * n)()
        got = ctypes.c_size_t()
        ok = self.k.ReadProcessMemory(wt.HANDLE(self.h), ctypes.c_void_p(addr),
                                      buf, n, ctypes.byref(got))
        return buf.raw if ok else None

    def wr(self, addr, data):
        got = ctypes.c_size_t()
        return bool(self.k.WriteProcessMemory(
            wt.HANDLE(self.h), ctypes.c_void_p(addr), data, len(data),
            ctypes.byref(got)))

    def f(self, a):
        return struct.unpack("<f", self.rd(a, 4))[0]

    def i(self, a):
        return struct.unpack("<i", self.rd(a, 4))[0]

    def u(self, a):
        return struct.unpack("<I", self.rd(a, 4))[0]

    def wf(self, a, v):
        self.wr(a, struct.pack("<f", v))

    def wi(self, a, v):
        self.wr(a, struct.pack("<i", v))

    def wu(self, a, v):
        self.wr(a, struct.pack("<I", v))

    def wb(self, a, v):
        self.wr(a, bytes([v]))


def veh(i):
    return VEH + STRIDE * i


def find_by_name(m, want):
    """Resolve the slot BY NAME every time - the array reindexes between
    sessions, and a hardcoded slot once wrote a parked-pit state onto Eddie
    Irvine."""
    for i in range(SLOTS):
        if m.i(veh(i) + OFF_ACTIVE) == -1:
            continue
        raw = m.rd(veh(i) + OFF_NAME, 32)
        if raw and raw.split(b"\0")[0].decode("latin-1", "replace").startswith(want):
            return i
    return -1


def node_base(m, recs):
    """Derive the waypoint array base from a live car, never hardcoded.

    Any car's curWaypoint points at a node; the node's lap distance identifies
    which .AIW record it is; base = ptr - index * stride. Two cars must agree
    or the answer is refused."""
    votes = {}
    for i in range(SLOTS):
        if m.i(veh(i) + OFF_ACTIVE) == -1:
            continue
        ptr = m.u(veh(i) + OFF_CURWP)
        if not (0x00400000 < ptr < 0x7FFFFFFF):
            continue
        raw = m.rd(ptr, 0x4C)
        if not raw:
            continue
        px, py, pz = struct.unpack("<3f", raw[0:12])
        dist = struct.unpack("<f", raw[0x48:0x4C])[0]
        best, bestd = None, 1e9
        for idx, r in enumerate(recs):
            p = r.get("wp_pos")
            if not p:
                continue
            d = (p[0] - px) ** 2 + (p[2] - pz) ** 2
            if d < bestd:
                bestd, best = d, idx
        if best is None or bestd > 0.01:
            continue
        score = recs[best].get("wp_score", [0, 0])
        if len(score) > 1 and abs(score[1] - dist) > 0.5:
            continue
        base = ptr - best * NODE_STRIDE
        votes[base] = votes.get(base, 0) + 1
    if not votes:
        return None
    base, n = max(votes.items(), key=lambda kv: kv[1])
    return base if n >= 2 else None


def box_node_index(recs, team):
    want = 106 + team
    for idx, r in enumerate(recs):
        if int(r.get("wp_branchID", [0])[0]) == want:
            return idx
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    secs = 20
    if "--secs" in sys.argv:
        secs = int(sys.argv[sys.argv.index("--secs") + 1])
    dry = "--dry" in sys.argv
    if not args:
        print(__doc__)
        return 1
    track = args[0]

    recs = aiw_records(track)
    if recs is None:
        print("no .aiw for %r under %s" % (track, GAMEDIR))
        return 1

    m = Mem()
    i = find_by_name(m, SC_NAME)
    if i < 0:
        print("safety car not found in the field")
        return 1
    team = m.i(veh(i) + OFF_PITTEAM)
    print("safety car: slot %d   AIW team %d" % (i, team))

    base = node_base(m, recs)
    if base is None:
        print("could not derive the waypoint array base from live cars")
        return 1
    idx = box_node_index(recs, team)
    if idx is None:
        print("no pit-box waypoint (branch %d) in this .aiw" % (106 + team))
        return 1
    # --node overrides which waypoint he is handed.
    #
    # Handed his BOX node, the game's own next target was wp 1134 - 12 m ahead
    # and 9.5 m ACROSS, because the box sits about 8 m off the lane centre. He
    # cut at it, overshot past the lane into the far wall, reversed and then
    # ran the lane properly (driver, 2026-08-03). The lane nodes level with the
    # box are a gentler merge, so they are worth comparing directly.
    if "--node" in sys.argv:
        idx = int(sys.argv[sys.argv.index("--node") + 1])
        print("node OVERRIDE: using waypoint %d" % idx)
    node = base + idx * NODE_STRIDE
    raw = m.rd(node, 0x4C)
    npos = struct.unpack("<3f", raw[0:12])
    ndist = struct.unpack("<f", raw[0x48:0x4C])[0]
    print("waypoint base 0x%08X  stride %d" % (base, NODE_STRIDE))
    print("his box node: wp %d @0x%08X  pos(%.3f, %.3f, %.3f)  lapdist %.3f"
          % (idx, node, npos[0], npos[1], npos[2], ndist))

    t0 = struct.unpack("<d", m.rd(SESSION_TIME, 8))[0]
    time.sleep(0.4)
    t1 = struct.unpack("<d", m.rd(SESSION_TIME, 8))[0]
    print("SessionTime %.2f -> %.2f  (%s)"
          % (t0, t1, "running" if t1 > t0 else "PAUSED - results mean nothing"))

    p0 = struct.unpack("<3f", m.rd(veh(i) + OFF_BODY, 12))
    print("before: pos(%.2f, %.2f, %.2f) pit %d lane %d curWP 0x%08X spd %.2f"
          % (p0[0], p0[1], p0[2], m.i(veh(i) + OFF_PITTING),
             m.i(veh(i) + OFF_LANEMODE), m.u(veh(i) + OFF_CURWP),
             m.f(veh(i) + OFF_SPEED)))

    if dry:
        print("--dry: nothing written")
        return 0

    v = veh(i)
    m.wb(v + OFF_RWFLAG, 1)            # or he re-retires
    m.wi(v + OFF_CALLBOX, 0)
    m.wi(v + OFF_FINISH, 0)
    m.wb(v + OFF_RETIRED, 0)
    m.wb(v + OFF_ACTIVE2, 1)
    m.wb(v + OFF_DRIVERVIS, 1)         # else an empty cockpit
    m.wi(v + OFF_PITSTATE, 2)
    m.wi(v + OFF_PITTING, 4)           # parked in the box
    m.wi(v + OFF_LANEMODE, 1)          # the pit-lane waypoint set
    m.wi(v + OFF_DRVSTAT, 3)           # approach, not 0
    m.wu(v + OFF_CURWP, node)          # <- THE ROUTE, the game's own node
    m.wf(v + OFF_ANGVEL, 1500.0)       # the crank
    m.wf(v + OFF_RPM, 1500.0)
    print("written: curWaypoint = his box node, pit 4, lane 1, engine cranked")

    hold_lane = "--hold-lane" in sys.argv
    limiter = "--limiter" in sys.argv
    if hold_lane:
        print("holding laneMode at 1 for the whole run")
    if limiter:
        print("holding maxSpeed at 22.222 (the game's own pit limiter)")

    print("\nwatching %d s" % secs)
    start = time.time()
    while time.time() - start < secs:
        j = find_by_name(m, SC_NAME)
        if j < 0:
            print("  lost him"); break
        v = veh(j)
        # RE-ASSERT THE WAYPOINT SET, NOT THE ROUTE.
        #
        # Handing the box/lane node works - the pointer advances by itself -
        # but veh+0x4550 fell back to 0 within one second and the game then
        # walked him along the MAIN path from inside the pit area, i.e.
        # straight at the wall (driver, 2026-08-03). The route is only as good
        # as the set it is read against, so the selector is what needs holding.
        if hold_lane:
            m.wi(v + OFF_LANEMODE, 1)
        if limiter:
            m.wf(v + 0x4538, 22.2222)
        p = struct.unpack("<3f", m.rd(v + OFF_BODY, 12))
        print("  +%4.1fs (%9.2f,%7.2f,%9.2f) spd %6.2f  pit %d lane %d "
              "curWP 0x%08X  moved %6.1f m"
              % (time.time() - start, p[0], p[1], p[2], m.f(v + OFF_SPEED),
                 m.i(v + OFF_PITTING), m.i(v + OFF_LANEMODE),
                 m.u(v + OFF_CURWP),
                 math.hypot(p[0] - p0[0], p[2] - p0[2])))
        time.sleep(1.0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
