#!/usr/bin/env python3
"""
pitbox_standing_start.py - hold a car in its pit box as long as we want, then
deploy it through the GAME'S OWN pit exit: slow, on the pit-lane path, under
the 80 km/h limiter, exactly like a real pit stop release.

THE MECHANISM (investigations/pitbox_standing_start/README.md)
--------------------------------------------------------------
The pit machine lives at veh+0x32CC. PitStopProgress (0x4A68B0) decides
everything with one number: veh+0x3470 stopEndTime.

    hold    = stopEnd in the future  -> the crew brake pins the car (servicing)
    release = stopEnd in the past    -> state 4 + PostPitCleanup (0x4534=0,
              go NOW) + cmd 15 (laneMode=1, limiter 22.22, 0x4548=1)

The AI then leaves by its own go-check (AIMainUpdate: driverStatus 4/5 ->
sub_4A4110 lane-clear -> driverStatus 0 -> follows the branch-1 waypoints at
flt_8ACFB4 = 22.22 m/s -> holds at the xpitout red light if closed -> merges).

DO NOT write maxSpeed / laneMode / curWaypoint on deploy - all three are game
outputs here (laneMode is derived from the waypoint branch every tick; writing
it by hand is what drove a car into the pit wall).

PLACEMENT (proven, sc_pitbox_trial.py): the car must take the green with the
field; placement happens at RacePhase 4, then the hand-over
(callToBox=1, pittingStatus=2, laneMode=1, curWaypoint=0) makes the game run
a real stop on it with zero corrective writes.

USAGE
    python tools/pitbox_standing_start.py state [--car NAME] [--track KEY]
    python tools/pitbox_standing_start.py hold  [--car NAME] --track KEY [--secs N]
    python tools/pitbox_standing_start.py deploy [--car NAME] [--secs N]
    python tools/pitbox_standing_start.py full  [--car NAME] --track KEY [--hold N] [--secs N]

    hold   place + hand over + stopEnd = now + 3600  (keep in box indefinitely)
    deploy release with the game's own machinery (stopEnd past, dStat=4) + watch
    full   hold, wait N s, deploy, watch the exit to the merge

WRITES TO THE RUNNING GAME.
"""

import ctypes
import ctypes.wintypes as wt
import io
import json
import math
import os
import struct
import subprocess
import sys
import time

EXE        = "F1 Challenge Online.exe"
VEH        = 0x5EC6D8
STRIDE     = 0x48C0
SLOTS      = 104
SESSION_TYPE = 0x7C56A4
RACE_PHASE = 0x7C56A8
SESSION_TIME = 0x8AFEA0   # double, seconds - frozen while the game is paused

OFF_ACTIVE  = 0x32C8
OFF_BODY    = 0x34E0      # authoritative position (not the 0x1C mirror)
OFF_YAW     = 0x34F0
OFF_VEL     = 0x58
OFF_NAME    = 0x4620
OFF_PITTEAM = 0x48A4
OFF_PITTING = 0x3450
OFF_SVCREADY= 0x345C
OFF_STOPSTART = 0x346C
OFF_STOPEND   = 0x3470
OFF_DRVSTAT = 0x4510
OFF_TIMEEXIT = 0x4534     # timeOfNextExit - 0 in race = the go-check fires now
OFF_MAXSPEED= 0x4538
OFF_LANEMODE= 0x4550
OFF_CALLBOX = 0x454C
OFF_F4544   = 0x4544
OFF_F4548   = 0x4548
OFF_CURWP   = 0x4354
OFF_SPEED   = 0x4440
OFF_RPM     = 0x39A0
OFF_ANGVEL  = 0x00A0
OFF_RWFLAG  = 0x0D58
OFF_FINISH  = 0x4064
OFF_RETIRED = 0x4410
OFF_ACTIVE2 = 0x45F4
OFF_DRIVERVIS = 0x105D

BOX_FORWARD_M = 2.33
BOX_UP_M      = 0.227
PIT_LIMIT     = 22.2222

PITS_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                         "aiw_pits.json")


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
        return buf.raw if ok and got.value == n else None

    def wr(self, addr, data):
        got = ctypes.c_size_t()
        return bool(self.k.WriteProcessMemory(
            wt.HANDLE(self.h), ctypes.c_void_p(addr), data, len(data),
            ctypes.byref(got)))

    def f(self, a):   return struct.unpack("<f", self.rd(a, 4))[0]
    def i(self, a):   return struct.unpack("<i", self.rd(a, 4))[0]
    def u(self, a):   return struct.unpack("<I", self.rd(a, 4))[0]
    def wf(self, a, v): self.wr(a, struct.pack("<f", v))
    def wi(self, a, v): self.wr(a, struct.pack("<i", v))
    def wu(self, a, v): self.wr(a, struct.pack("<I", v))
    def wb(self, a, v): self.wr(a, bytes([v]))
    def sess(self):   return struct.unpack("<d", self.rd(SESSION_TIME, 8))[0]


def veh(i):
    return VEH + STRIDE * i


def find_by_name(m, want):
    """Resolve the slot BY NAME every sample - the array reindexes between
    sessions and a hardcoded slot once wrote a parked-pit state onto Eddie
    Irvine (2026-08-03)."""
    for i in range(SLOTS):
        if m.i(veh(i) + OFF_ACTIVE) == -1:
            continue
        raw = m.rd(veh(i) + OFF_NAME, 32)
        if raw and raw.split(b"\0")[0].decode("latin-1", "replace").startswith(want):
            return i
    return -1


def box_target(m, track, slot):
    d = json.load(io.open(PITS_JSON, encoding="utf-8"))
    entry = d.get(track) or d[[k for k in d if k.lower() == track.lower()][0]]
    team = m.i(veh(slot) + OFF_PITTEAM)
    t = entry["teams"][str(team)]
    px, py, pz = t["pit_pos"]
    yaw = t["pit_ori"][1]
    return (px + math.sin(yaw) * BOX_FORWARD_M, py + BOX_UP_M,
            pz + math.cos(yaw) * BOX_FORWARD_M), yaw, team


def show(m, slot, tag=""):
    v = veh(slot)
    p = struct.unpack("<3f", m.rd(v + OFF_BODY, 12))
    print("  %-9s (%9.3f,%7.3f,%9.3f) spd %6.2f  pit %d svc %d dS %d lane %d "
          "cb %d  maxSp %6.0f  stopEnd %7.1f tExit %7.1f now %7.1f "
          "ret %d act %d fin %d rw %d"
          % (tag, p[0], p[1], p[2], m.f(v + OFF_SPEED),
             m.i(v + OFF_PITTING), m.i(v + OFF_SVCREADY),
             m.i(v + OFF_DRVSTAT), m.i(v + OFF_LANEMODE),
             m.i(v + OFF_CALLBOX), m.f(v + OFF_MAXSPEED),
             m.f(v + OFF_STOPEND), m.f(v + OFF_TIMEEXIT), m.sess(),
             m.rd(v + OFF_RETIRED, 1)[0], m.i(v + OFF_ACTIVE2),
             m.i(v + OFF_FINISH), m.rd(v + OFF_RWFLAG, 1)[0]))
    return p


def paused(m, window=0.4):
    a = m.sess()
    time.sleep(window)
    b = m.sess()
    return (b - a) < 1e-6


def place(m, slot, pos, yaw, hold_secs=3600.0):
    """Put the car in the box and pin it with the GAME'S OWN service hold.

    pit=3 + svc=1 + stopEnd future: PitStopProgress runs its service branch
    every tick while SessionTime < stopEnd, and the crew wheel-force ramps
    (the service brake) pin the car. tExit future blocks the AI go-check
    (AIMainUpdate:  now > 0x4534 || dS==4  -> release). pit=2 must NOT be
    used mid-race: the game's 2->3 transition needs the car stationary at
    the box AND the AI inert; a mid-race un-freeze leaves dS=0 (follow) with
    tExit=0, which drove the car straight off (2026-08-03, first live run).
    """
    v = veh(slot)
    now = m.sess()
    m.wr(v + OFF_BODY, struct.pack("<3f", *pos))
    m.wr(v + OFF_VEL, struct.pack("<3f", 0.0, 0.0, 0.0))
    m.wf(v + OFF_YAW, yaw)
    m.wb(v + OFF_RWFLAG, 1)
    m.wb(v + OFF_RETIRED, 0)
    m.wb(v + OFF_ACTIVE2, 1)
    m.wb(v + OFF_DRIVERVIS, 1)
    m.wi(v + OFF_FINISH, 0)
    m.wi(v + OFF_CALLBOX, 1)          # a stop is requested - why he is here
    m.wi(v + OFF_PITTING, 3)          # stopped in the box - the crew pin
    m.wb(v + OFF_SVCREADY, 1)         # gates PitStopProgress in state 3
    m.wi(v + OFF_LANEMODE, 1)
    m.wu(v + OFF_CURWP, 0)            # fresh nearest-node query
    m.wf(v + OFF_STOPSTART, now)
    m.wf(v + OFF_STOPEND, now + hold_secs)   # <- THE HOLD (crew brake pins)
    m.wf(v + OFF_TIMEEXIT, now + hold_secs)  # <- blocks the AI go-check
    print("  placed in the box (%.3f, %.3f, %.3f) yaw %.3f  pit 3  "
          "stopEnd now+%.0f s  tExit now+%.0f s"
          % (pos[0], pos[1], pos[2], yaw, hold_secs, hold_secs))


def deploy(m, slot):
    """The game's own release. stopEnd in the past makes PitStopProgress fire
    state 4 + PostPitCleanup (0x4534=0, go NOW) + cmd 15 (laneMode=1, limiter,
    0x4548=1). driverStatus=4 puts the AI in pit-entry mode so its own
    go-check drives it out. Nothing is forged: maxSpeed/laneMode/curWaypoint
    are left to the game."""
    v = veh(slot)
    now = m.sess()
    m.wf(v + OFF_STOPEND, now - 0.5)
    m.wi(v + OFF_DRVSTAT, 4)          # pit-entry/exit state (NOT 0, NOT 3)
    m.wf(v + OFF_TIMEEXIT, 0.0)       # go NOW (idempotent w/ PostPitCleanup)
    m.wi(v + OFF_CALLBOX, 0)          # stop asking - consumed at commit
    if (m.f(v + OFF_RPM) or 0.0) < 100.0:
        m.wf(v + OFF_ANGVEL, 800.0)   # the ignition gate - the AI steers a
        m.wf(v + OFF_RPM, 800.0)      # dead engine otherwise
        print("  cranked the engine")
    print("  deployed: stopEnd past, driverStatus 4, timeOfNextExit 0")


def watch(m, name, secs, p0=None, want_exit=False):
    t0 = time.time()
    merged = False
    while time.time() - t0 < secs:
        j = find_by_name(m, name)
        if j < 0:
            print("  lost him")
            return False
        p = show(m, j, "+%4.1fs" % (time.time() - t0))
        if p0 is not None:
            d = math.hypot(p[0] - p0[0], p[2] - p0[2])
            print("           moved %6.1f m" % d)
        if want_exit and m.i(veh(j) + OFF_PITTING) == 0 and \
                m.i(veh(j) + OFF_LANEMODE) == 0 and \
                m.f(veh(j) + OFF_SPEED) > 30.0:
            print("  >>> EXITED the pit lane, racing speed")
            merged = True
            break
        time.sleep(1.0)
    return merged


def main():
    raw = sys.argv[1:]
    args = [a for a in raw if not a.startswith("--")]
    def opt(name, default):
        return raw[raw.index(name) + 1] if name in raw else default
    name = opt("--car", "Bernd Maylander")
    track = opt("--track", None)
    secs = int(opt("--secs", "40"))
    hold = int(opt("--hold", "15"))
    if not args:
        print(__doc__)
        return 1
    mode = args[0]

    m = Mem()
    if paused(m):
        print("!! THE GAME IS PAUSED - SessionTime is frozen, results would")
        print("   be meaningless. Unpause and run again.")
        return 1
    slot = find_by_name(m, name)
    if slot < 0:
        print("car %r not in the field" % name)
        return 1
    v0 = veh(slot)
    if m.rd(v0 + OFF_RETIRED, 1)[0] == 0 and m.i(v0 + OFF_ACTIVE2) == 0 \
            and m.i(v0 + OFF_FINISH) == 0 and m.f(v0 + OFF_SPEED) < 0.5:
        print("!! car %r is in LIMBO (ret 0 / act 0 / fin 0) - the game "
              "deactivated it and this state is unrecoverable (corpus FACT)." % name)
        print("   It CANNOT be revived this session; start a fresh race.")
        return 1
    print("car %r: slot %d  SessionType %d RacePhase %d SessionTime %.1f"
          % (name, slot, m.i(SESSION_TYPE), m.i(RACE_PHASE), m.sess()))

    if mode == "state":
        show(m, slot, "now")
        return 0

    if track is None:
        print("hold/full need --track (aiw_pits.json key, e.g. 1999_Monza)")
        return 1
    pos, yaw, team = box_target(m, track, slot)
    print("AIW team %d -> PitPos + (%.2f fwd, %.2f up) = (%.3f, %.3f, %.3f)"
          % (team, BOX_FORWARD_M, BOX_UP_M, pos[0], pos[1], pos[2]))

    if mode == "hold":
        place(m, slot, pos, yaw)
        watch(m, name, secs, p0=pos)
        return 0

    if mode == "deploy":
        deploy(m, slot)
        p0 = struct.unpack("<3f", m.rd(veh(slot) + OFF_BODY, 12))
        watch(m, name, secs, p0=p0, want_exit=True)
        return 0

    if mode == "full":
        place(m, slot, pos, yaw)
        print("\n  holding %d s (standing start), then deploying" % hold)
        time.sleep(hold)
        deploy(m, slot)
        p0 = struct.unpack("<3f", m.rd(veh(slot) + OFF_BODY, 12))
        watch(m, name, secs, p0=p0, want_exit=True)
        return 0

    print("unknown mode %r" % mode)
    return 1


if __name__ == "__main__":
    sys.exit(main())
