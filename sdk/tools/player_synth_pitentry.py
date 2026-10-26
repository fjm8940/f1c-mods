"""
player_synth_pitentry.py - OPEN ITEM 0 (HANDOFF_2026-08-03.md 5c) on the PLAYER.

THE QUESTION
------------
Crossing the pit-entry line changes EXACTLY SIX vehicle-struct fields (traced
twice, 20 Hz, whole struct). Everything else that frame is continuous physics.
If we write those six onto a car that has NOT crossed the line - here, the
player, teleported straight into his own pit box - does the game then treat him
as having ENTERED THE PITS and run the rest of the sequence itself?

The six fields (5c):
    veh+0x3450  pittingStatus  = 2
    veh+0x4538  maxSpeed       = 22.222        (limiter, applied at the line)
    veh+0x344C  <new>          = 1             (one-shot pit-entry flag)
    veh+0x3454  <new>          = SessionTime   (float)
    veh+0x3458  <new>          = SessionTime   (float)
    veh+0x1068  <new>          |= 0x100        (bitfield gains bit 8)

ACKNOWLEDGEMENT we watch for (the game doing the rest, 5c):
    pittingStatus advances 2 -> 3 -> 4 by itself
    callToBox (0x454C) 1 -> 0
    AIControl (0x45F4) 1 -> 0
    driverStatus (0x4510) -> 4
    stopEndTime (0x3470) written to a FUTURE SessionTime
    fuelTarget changes

RULES BAKED IN (both cost a wrong result on 2026-08-03):
  * resolve the player EVERY sample, never a cached slot. Slot 0 is the player
    (active flag 0), but we re-check the active flag each read anyway.
  * NEVER judge a result while SessionTime (0x8AFEA0, double) is frozen. A paused
    game reads as a perfect motionless hold. sim_time() gates INTERPRETATION,
    never the START of a capture - the scenario is armed while paused.

WRITES TO THE RUNNING GAME: position + velocity + yaw, then the six 5c fields.
Restores position/velocity/pit fields on request with `restore`.

USAGE
    python tools/player_synth_pitentry.py state              read-only
    python tools/player_synth_pitentry.py fire [--track T]   teleport + 6 fields + watch
    python tools/player_synth_pitentry.py fire --nomove      6 fields where he stands
    python tools/player_synth_pitentry.py restore            put position/pit back

Options:
    --track NAME   track key in aiw_pits.json (default 1999_Monza)
    --secs N       observe seconds after firing (default 25)
"""

import ctypes
import json
import math
import os
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

VEH_BASE = 0x005EC6D8
STRIDE = 0x48C0
VEH_COUNT = 0x005C329C
SESSION_TYPE = 0x007C56A4
RACE_PHASE = 0x007C56A8
SESSION_TIME = 0x008AFEA0  # double, seconds - FROZEN WHILE PAUSED

OFF_ACTIVE = 0x32C8        # -1 inactive, 0 player, 1 AI
OFF_NAME = 0x4620
OFF_BODY = 0x34E0          # X, Y, Z - the authoritative position
OFF_YAW = 0x34F0
OFF_VEL = 0x58             # velocity triad
OFF_SPEED = 0x4440         # goes STALE while frozen - judge by position
OFF_TEAMIDX = 0x48A4       # AIW [PITS] TeamIndex
OFF_BAYIDX = 0x48A8

# The six 5c pit-line-entry fields
OFF_PITTING = 0x3450
OFF_MAXSPEED = 0x4538
OFF_ENTRY1 = 0x344C        # one-shot entry flag
OFF_TS1 = 0x3454           # entry timestamp
OFF_TS2 = 0x3458           # entry timestamp (prev)
OFF_BITS = 0x1068          # bitfield, bit 8 set at the line

# Acknowledgement fields
OFF_CALLBOX = 0x454C
OFF_AICTRL = 0x45F4
OFF_DRVSTAT = 0x4510
OFF_STOPEND = 0x3470       # stopEndTime
OFF_FUELTGT = 0x3338       # fuel target

MAXSPEED_LIMITER = 22.222
FORWARD_M = 2.33           # metres ahead of the PitPos marker (measured)
UP_M = 0.227

PITS_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                         "aiw_pits.json")
PROC = "F1 Challenge Online.exe"


def find_pid():
    out = subprocess.check_output(["tasklist", "/FI", "IMAGENAME eq " + PROC],
                                  text=True, errors="replace")
    for line in out.splitlines():
        if "F1 Challenge" in line:
            for tok in line.split():
                if tok.isdigit():
                    return int(tok)
    return None


pid = find_pid()
if not pid:
    sys.exit("game not running")
h = k32.OpenProcess(0x1F0FFF, 0, pid)
if not h:
    sys.exit("OpenProcess failed - try an Administrator shell")


def rd(a, n):
    b = ctypes.create_string_buffer(n)
    g = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(g))
    return b.raw if ok and g.value == n else None


def wr(a, data):
    g = ctypes.c_size_t()
    b = ctypes.create_string_buffer(data, len(data))
    ok = k32.WriteProcessMemory(h, ctypes.c_void_p(a), b, len(data),
                                ctypes.byref(g))
    return bool(ok) and g.value == len(data)


def i32(a):
    r = rd(a, 4)
    return None if r is None else struct.unpack("<i", r)[0]


def f32(a):
    r = rd(a, 4)
    return None if r is None else struct.unpack("<f", r)[0]


def u32(a):
    r = rd(a, 4)
    return None if r is None else struct.unpack("<I", r)[0]


def veh(s):
    return VEH_BASE + STRIDE * s


def sim_time():
    r = rd(SESSION_TIME, 8)
    return None if r is None else struct.unpack("<d", r)[0]


def paused(window=0.4):
    a = sim_time()
    time.sleep(window)
    b = sim_time()
    if a is None or b is None:
        return False
    return (b - a) < 1e-6


def find_player():
    """The player, every call. Slot 0 is the player (active flag 0); re-verify."""
    if i32(veh(0) + OFF_ACTIVE) == 0:
        return 0
    n = i32(VEH_COUNT) or 0
    for s in range(max(n, 24)):
        if i32(veh(s) + OFF_ACTIVE) == 0:
            return s
    return None


def load_pits(track):
    with open(PITS_JSON, encoding="utf-8") as fh:
        allpits = json.load(fh)
    if track in allpits:
        return allpits[track]
    matches = [k for k in allpits if track.lower() in k.lower()]
    if len(matches) == 1:
        return allpits[matches[0]]
    sys.exit("track %r not found in aiw_pits.json (%d candidates: %s)"
             % (track, len(matches), matches[:6]))


def box_target(slot, pits):
    team = i32(veh(slot) + OFF_TEAMIDX)
    t = pits["teams"].get(str(team))
    if not t or "pit_pos" not in t:
        sys.exit("no PitPos for AIW team %s on this track" % team)
    px, py, pz = t["pit_pos"]
    yaw = t["pit_ori"][1]
    fx, fz = math.sin(yaw), math.cos(yaw)
    return (px + fx * FORWARD_M, py + UP_M, pz + fz * FORWARD_M), yaw, team


def snap(slot):
    v = veh(slot)
    x, y, z = struct.unpack("<3f", rd(v + OFF_BODY, 12))
    return dict(
        slot=slot, active=i32(v + OFF_ACTIVE),
        pos=(x, y, z), yaw=f32(v + OFF_YAW), spd=f32(v + OFF_SPEED),
        pit=i32(v + OFF_PITTING), maxspd=f32(v + OFF_MAXSPEED),
        entry1=i32(v + OFF_ENTRY1), ts1=f32(v + OFF_TS1), ts2=f32(v + OFF_TS2),
        bits=u32(v + OFF_BITS), callbox=i32(v + OFF_CALLBOX),
        aictrl=i32(v + OFF_AICTRL), drvstat=i32(v + OFF_DRVSTAT),
        stopend=f32(v + OFF_STOPEND), fueltgt=f32(v + OFF_FUELTGT),
        team=i32(v + OFF_TEAMIDX), bay=i32(v + OFF_BAYIDX),
        st=sim_time(),
    )


def line(s, tag=""):
    print("  %-8s pit %s max %8.2f  e1 %s ts1 %8.2f ts2 %8.2f  bits 0x%X  "
          "callBox %s AICtrl %s drv %s stopEnd %8.2f  (%.1f,%.1f,%.1f)"
          % (tag, s["pit"], s["maxspd"], s["entry1"], s["ts1"], s["ts2"],
             s["bits"], s["callbox"], s["aictrl"], s["drvstat"], s["stopend"],
             s["pos"][0], s["pos"][1], s["pos"][2]))


def write_six(slot, now_t):
    """The 5c write set. now_t = current SessionTime."""
    v = veh(slot)
    ok = True
    ok &= wr(v + OFF_PITTING, struct.pack("<i", 2))
    ok &= wr(v + OFF_MAXSPEED, struct.pack("<f", MAXSPEED_LIMITER))
    ok &= wr(v + OFF_ENTRY1, struct.pack("<i", 1))
    ok &= wr(v + OFF_TS1, struct.pack("<f", now_t))
    ok &= wr(v + OFF_TS2, struct.pack("<f", now_t))
    bits = u32(v + OFF_BITS) or 0
    ok &= wr(v + OFF_BITS, struct.pack("<I", bits | 0x100))
    return ok


def do_fire(slot, pits, secs, move):
    v = veh(slot)
    before = snap(slot)
    print("\n  BEFORE:")
    line(before, "pre")

    now_t = sim_time()
    if now_t is None:
        sys.exit("could not read SessionTime")

    if move:
        pos, yaw, team = box_target(slot, pits)
        print("\n  AIW team %d -> PitPos %s"
              % (team, pits["teams"][str(team)]["pit_pos"]))
        print("  teleport target (+%.2fm fwd, +%.2fm up) = (%.3f,%.3f,%.3f)"
              % (FORWARD_M, UP_M, *pos))
        wr(v + OFF_BODY, struct.pack("<3f", *pos))
        wr(v + OFF_VEL, struct.pack("<3f", 0.0, 0.0, 0.0))
        wr(v + OFF_YAW, struct.pack("<f", yaw))
    else:
        print("\n  (--nomove: writing the six fields where he stands)")

    ok = write_six(slot, now_t)
    print("  wrote six 5c fields at SessionTime %.3f : %s" % (now_t, ok))
    line(snap(slot), "armed")

    print("\n  observing %d s - does the game advance the stop itself?" % secs)
    t0 = time.time()
    was_paused_warned = False
    while time.time() - t0 < secs:
        # resolve the player fresh every sample
        s_slot = find_player()
        if s_slot is None:
            print("  player vanished from the array")
            break
        s = snap(s_slot)
        frozen = paused()
        line(s, "+%4.1fs" % (time.time() - t0))
        if frozen and not was_paused_warned:
            print("       !! SessionTime FROZEN (paused) - readings below are "
                  "not a real result until you unpause")
            was_paused_warned = True
        elif not frozen:
            was_paused_warned = False
        time.sleep(1.0)

    after = snap(find_player() or slot)
    print("\n  VERDICT:")
    adv = after["pit"] != 2
    print("  pittingStatus 2 -> %s   %s" % (after["pit"],
          "ADVANCED (game accepted the entry)" if adv else "stuck at 2 (not accepted)"))
    print("  callToBox %s->%s  AIControl %s->%s  driverStatus %s->%s"
          % (before["callbox"], after["callbox"], before["aictrl"],
             after["aictrl"], before["drvstat"], after["drvstat"]))
    print("  stopEndTime %.2f -> %.2f  (now=%.2f, future=%s)"
          % (before["stopend"], after["stopend"], after["st"] or -1,
             (after["stopend"] or -1) > (after["st"] or 0)))


def do_restore(slot):
    v = veh(slot)
    print("  clearing the pit fields (position left where it is)")
    wr(v + OFF_PITTING, struct.pack("<i", 0))
    wr(v + OFF_MAXSPEED, struct.pack("<f", 99999.0))
    wr(v + OFF_ENTRY1, struct.pack("<i", 0))
    bits = u32(v + OFF_BITS) or 0
    wr(v + OFF_BITS, struct.pack("<I", bits & ~0x100))
    wr(v + OFF_CALLBOX, struct.pack("<i", 0))
    line(snap(slot), "post")


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    cmd = args[0]
    secs = 25
    if "--secs" in args:
        secs = int(args[args.index("--secs") + 1])
    track = "1999_Monza"
    if "--track" in args:
        track = args[args.index("--track") + 1]
    move = "--nomove" not in args

    slot = find_player()
    if slot is None:
        sys.exit("player not found (no slot with active flag 0) - not in a session?")
    print("player: slot %d  (SessionType %d, RacePhase %d, VehicleCount %d, "
          "SessionTime %s)"
          % (slot, i32(SESSION_TYPE), i32(RACE_PHASE), i32(VEH_COUNT),
             sim_time()))
    nm = rd(veh(slot) + OFF_NAME, 24)
    if nm:
        print("  name: %r" % nm.split(b"\x00")[0].decode("latin1", "replace"))

    if cmd == "state":
        line(snap(slot), "now")
        print("  AIW team %d bay %d" % (i32(veh(slot) + OFF_TEAMIDX),
                                        i32(veh(slot) + OFF_BAYIDX)))
        return
    if cmd == "restore":
        do_restore(slot)
        return
    if cmd == "tp":
        # PHASE 1: teleport into the box ONLY. No pit fields. Watch what he does.
        pits = load_pits(track)
        pos, yaw, team = box_target(slot, pits)
        v = veh(slot)
        print("\n  BEFORE:")
        line(snap(slot), "pre")
        print("\n  AIW team %d -> PitPos %s"
              % (team, pits["teams"][str(team)]["pit_pos"]))
        print("  teleport target (+%.2fm fwd, +%.2fm up) = (%.3f,%.3f,%.3f)"
              % (FORWARD_M, UP_M, *pos))
        wr(v + OFF_BODY, struct.pack("<3f", *pos))
        wr(v + OFF_VEL, struct.pack("<3f", 0.0, 0.0, 0.0))
        wr(v + OFF_YAW, struct.pack("<f", yaw))
        line(snap(slot), "placed")
        print("\n  observing %d s (teleport only, no pit fields):" % secs)
        t0 = time.time()
        warned = False
        while time.time() - t0 < secs:
            ps = find_player() or slot
            s = snap(ps)
            frozen = paused()
            d = math.dist(s["pos"], pos)
            print("  +%4.1fs (%8.2f,%6.2f,%9.2f) spd %6.2f pit %d  %6.1f m from box"
                  % (time.time() - t0, s["pos"][0], s["pos"][1], s["pos"][2],
                     s["spd"], s["pit"], d))
            if frozen and not warned:
                print("       !! PAUSED - not a real result until unpaused")
                warned = True
            elif not frozen:
                warned = False
            time.sleep(1.0)
        return
    if cmd == "fire":
        pits = load_pits(track) if move else None
        if "--wait5" in args:
            print("\n  waiting for RacePhase 5 (racing) and the sim to run ...")
            while True:
                ph = i32(RACE_PHASE)
                if ph == 5 and not paused(0.2):
                    print("  RacePhase 5, sim running - firing")
                    break
                time.sleep(0.1)
            slot = find_player() or slot
        do_fire(slot, pits, secs, move)
        return
    sys.exit("unknown command %r" % cmd)


if __name__ == "__main__":
    main()
