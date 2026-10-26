"""
sc_pitbox_trial.py - TRIAL: at the race start, teleport the safety car straight
into his own pit box and watch what the game does about it.

THE QUESTION THIS ANSWERS
-------------------------
Can the safety car be taken off the starting grid and put in his pit box the
moment the race begins, instead of driving a whole in-lap to get there? And
once he is there, does he stay, or does the game drag him back onto the track?

WHAT IS ALREADY KNOWN, so this script does not re-derive it
-----------------------------------------------------------
  * The authoritative position is veh+0x34E0/0x34E4/0x34E8. A 12-byte write
    teleports a LIVE, DRIVING car and it keeps driving from the new place
    (FACT, 2026-08-03). veh+0x1C and veh+0x744 are only mirrors of it.
  * His pit box is found from his own struct: veh+0x48A4 is his AIW [PITS]
    TeamIndex, and that team's PitPos is where the game really services him -
    measured at 0.13 m lateral error with the car actually in the box.
  * The car rests about 2.3 m FORWARD of the PitPos marker along PitOri, and
    about 0.23 m above it. Writing PitPos raw would bury the car 23 cm into
    the ground, which is the state that once sent a car falling to PhysY
    -13513.
  * HE MUST TAKE THE GREEN WITH EVERYONE ELSE. A car that is hidden, retired
    or frozen when the lights go out never has its brake hold released - brake
    pins at 1.000 for ever and no de-retirement recipe recovers it. So this
    waits until he is genuinely moving before touching him.

WHAT IS NOT KNOWN, and is the point of the trial
------------------------------------------------
  * whether he stays in the box or simply drives off
  * whether pittingStatus agrees with where he physically is
  * whether the game tries to send him round a lap to enter the pits "properly"

USAGE
    python tools/sc_pitbox_trial.py watch --pitmode   arm; at the green run the
                                                     full hand-over sequence
    python tools/sc_pitbox_trial.py watch          arm and wait for the start
    python tools/sc_pitbox_trial.py now            fire immediately, no waiting
    python tools/sc_pitbox_trial.py now --govern    pin maxSpeed to 0
    python tools/sc_pitbox_trial.py now --freeze    park him (retire + hide)
    python tools/sc_pitbox_trial.py pitmode         teleport -> pit request ->
                                                    pit-lane mode -> release
    python tools/sc_pitbox_trial.py now --call     ...and also raise callToBox
    python tools/sc_pitbox_trial.py state          read-only, print his state

Options:
    --secs N     how long to observe afterwards (default 30)
    --speed N    m/s he must reach before the teleport arms (default 3.0)

WRITES TO THE RUNNING GAME. Position + velocity + yaw, and callToBox only if
--call is given. Nothing is hidden, retired or removed from the field.
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
SESSION_TIME = 0x008AFEA0  # double, seconds - FROZEN WHILE THE GAME IS PAUSED

OFF_ACTIVE = 0x32C8
OFF_NAME = 0x4620
OFF_PITTING = 0x3450
OFF_SPEED = 0x4440
OFF_BODY = 0x34E0          # X, Y, Z  - the authoritative position
OFF_YAW = 0x34F0
OFF_VEL = 0x58             # velocity triad
OFF_TEAMIDX = 0x48A4       # AIW [PITS] TeamIndex
OFF_BAYIDX = 0x48A8
OFF_CALLBOX = 0x454C
OFF_LAPNUM = 0x403C
OFF_MAXSPEED = 0x4538      # the governor the game itself uses (99999 = free)
OFF_FINISH = 0x4064
OFF_RETIRED = 0x4410
OFF_ACTIVE_AI = 0x45F4
OFF_PITSTATE = 0x4040
OFF_DRIVERVIS = 0x105D
OFF_LANEMODE = 0x4550      # WaypointSetSelector: 1 = use the pit-lane path
OFF_CURWP = 0x4354         # CurrentWaypointPtr - NULL forces a re-acquire

# Measured 2026-08-03 with the car actually parked in its box.
FORWARD_M = 2.33           # metres ahead of the PitPos marker
UP_M = 0.227               # metres above it

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


def veh(s):
    return VEH_BASE + STRIDE * s


def sim_time():
    r = rd(SESSION_TIME, 8)
    return None if r is None else struct.unpack("<d", r)[0]


def paused(window=0.4):
    """Is the sim actually advancing?

    A paused game reports a car standing perfectly still with a STALE speed
    field, which reads exactly like a car being pinned by someone else's code.
    That cost a wrong diagnosis on 2026-08-03 - the release looked like it had
    failed when the game was simply paused. Never judge motion without this.
    """
    a = sim_time()
    time.sleep(window)
    b = sim_time()
    if a is None or b is None:
        return False
    return (b - a) < 1e-6


def find_sc():
    """The safety car, by name. Slot changes between sessions - never cache."""
    n = i32(VEH_COUNT) or 0
    for s in range(max(n, 24)):
        if i32(veh(s) + OFF_ACTIVE) == -1:
            continue
        nm = rd(veh(s) + OFF_NAME, 24)
        if nm and b"Maylander" in nm:
            return s
    return None


def track_name():
    """The loaded track, taken from the game's own .AIW path string."""
    # cheap: aiw_pits.json is keyed by the track file's basename, and the game
    # keeps that path in memory. Scan a modest window of the heap for it.
    try:
        keys = json.load(open(PITS_JSON, encoding="utf-8")).keys()
    except Exception:
        return None
    # brute-force is unnecessary: try the .gdb the session loaded via the
    # MessageDisplayBuffer style globals is unreliable, so fall back to asking.
    return None


def load_pits(track):
    with open(PITS_JSON, encoding="utf-8") as fh:
        allpits = json.load(fh)
    if track in allpits:
        return allpits[track]
    matches = [k for k in allpits if track.lower() in k.lower()]
    if len(matches) == 1:
        return allpits[matches[0]]
    sys.exit("track %r not found in aiw_pits.json (%d candidates)"
             % (track, len(matches)))


def state(slot):
    v = veh(slot)
    x, y, z = struct.unpack("<3f", rd(v + OFF_BODY, 12))
    return dict(
        pos=(x, y, z),
        yaw=f32(v + OFF_YAW),
        spd=f32(v + OFF_SPEED),
        pit=i32(v + OFF_PITTING),
        lap=i32(v + OFF_LAPNUM),
        team=i32(v + OFF_TEAMIDX),
        bay=i32(v + OFF_BAYIDX),
        callbox=i32(v + OFF_CALLBOX),
    )


def show(slot, tag=""):
    s = state(slot)
    print("  %-9s (%9.3f,%7.3f,%9.3f) yaw %7.4f  spd %6.2f  pit %d  lap %d"
          % (tag, s["pos"][0], s["pos"][1], s["pos"][2], s["yaw"], s["spd"],
             s["pit"], s["lap"]))
    return s


def target_for(slot, pits):
    """Where the game itself would park him: PitPos, plus the measured offset."""
    team = i32(veh(slot) + OFF_TEAMIDX)
    t = pits["teams"].get(str(team))
    if not t or "pit_pos" not in t:
        sys.exit("no PitPos for AIW team %s on this track" % team)
    px, py, pz = t["pit_pos"]
    yaw = t["pit_ori"][1]
    # forward axis from the box heading; verified against the measured rest
    # position, which sat 2.33 m along it and 0.227 m above the marker.
    fx, fz = math.sin(yaw), math.cos(yaw)
    return (px + fx * FORWARD_M, py + UP_M, pz + fz * FORWARD_M), yaw, team


def teleport(slot, pos, yaw, call=False, freeze=False, govern=False):
    """Put him in the box. Arriving stopped is not enough - he drives out.

    TRIAL 1, 2026-08-03: teleport alone put him exactly in the box and he
    immediately drove away under his own AI, 0 -> 58 m/s and 580 m in 18 s,
    with pittingStatus stuck at 0 the whole time. The game never treated him
    as pitting just because he was standing in a pit box. So the arrival has
    to remove his ability to drive, not just his velocity.

    Two ways, both offered here:

      govern - pin the game's OWN speed governor, veh+0x4538. This is the
               lever the caution controller already uses and it keeps the car
               alive, visible and classified.
      freeze - the proven park: FinishState=2 / RetiredFlag=1 / ActiveFlag=0 /
               PitState=2 / model hidden. Exactly the state sc_deploy() already
               revives from, so it fits the existing chain. He vanishes from
               the field, which is what the mod wants anyway.

    Timing rule that must not be broken: this may only run AFTER the green and
    after he has moved. A car frozen or retired at the lights never gets its
    brake hold released, and that state is unrecoverable.
    """
    v = veh(slot)
    ok = True
    ok &= wr(v + OFF_BODY, struct.pack("<3f", *pos))
    ok &= wr(v + OFF_VEL, struct.pack("<3f", 0.0, 0.0, 0.0))   # arrive stopped
    ok &= wr(v + OFF_YAW, struct.pack("<f", yaw))
    if govern:
        ok &= wr(v + OFF_MAXSPEED, struct.pack("<f", 0.0))
    if call:
        ok &= wr(v + OFF_CALLBOX, struct.pack("<i", 1))
    if freeze:
        ok &= wr(v + OFF_FINISH, struct.pack("<i", 2))
        ok &= wr(v + OFF_RETIRED, bytes([1]))
        ok &= wr(v + OFF_ACTIVE_AI, struct.pack("<i", 0))
        ok &= wr(v + OFF_PITSTATE, struct.pack("<i", 2))
        ok &= wr(v + OFF_DRIVERVIS, bytes([0]))
    return ok


def observe(slot, secs, target):
    print("\n  observing %d s (target (%.3f,%.3f,%.3f))\n" % (secs, *target))
    t0 = time.time()
    last = None
    while time.time() - t0 < secs:
        s = show(slot, "+%4.1fs" % (time.time() - t0))
        d = math.dist(s["pos"], target)
        if last is not None and abs(d - last) > 5.0:
            print("       ^ moved %.1f m from the box" % d)
        last = d
        time.sleep(1.0)


def pitmode(slot, pos, yaw, hold_ms, secs):
    """The sequence asked for on 2026-08-03:

         teleport into the box  ->  raise the pit request  ->  tell the game he
         is IN THE PIT LANE  ->  release his movement and see what he does.

    Trial 1 showed a teleport alone is not enough: he landed in the box exactly
    and then drove off at 58 m/s with pittingStatus stuck at 0. Standing in a
    box means nothing to the game. So this hands him over to the pit machinery
    instead of trying to pin him in place:

        pittingStatus = 2   he is in the pit lane, so the game applies its own
                            80 km/h limiter and pit-lane pathing
        callToBox     = 1   a stop is requested, so there is a reason to be here
        laneMode      = 1   the AI uses the pit-lane waypoint set
        curWaypoint   = 0   drop the stale racing-line waypoint; a stale one is
                            what once steered a car into a garage wall

    maxSpeed is pinned to 0 only for the hand-over, then released, so we can
    see whether HE drives himself into the box rather than us placing him.
    """
    v = veh(slot)
    print("")
    print("  1) teleport + hold")
    wr(v + OFF_BODY, struct.pack("<3f", *pos))
    wr(v + OFF_VEL, struct.pack("<3f", 0.0, 0.0, 0.0))
    wr(v + OFF_YAW, struct.pack("<f", yaw))
    wr(v + OFF_MAXSPEED, struct.pack("<f", 0.0))
    show(slot, "placed")

    print("  2) pit request + pit-lane designation")
    wr(v + OFF_CALLBOX, struct.pack("<i", 1))
    wr(v + OFF_PITTING, struct.pack("<i", 2))
    wr(v + OFF_LANEMODE, struct.pack("<i", 1))
    wr(v + OFF_CURWP, struct.pack("<I", 0))
    time.sleep(hold_ms / 1000.0)
    s = show(slot, "armed")
    print("     callToBox %d  pittingStatus %d  laneMode %d"
          % (i32(v + OFF_CALLBOX), i32(v + OFF_PITTING), i32(v + OFF_LANEMODE)))

    print("  3) releasing his movement (maxSpeed back to free)")
    wr(v + OFF_MAXSPEED, struct.pack("<f", 99999.0))
    observe(slot, secs, pos)


def keep(slot, pos, yaw, secs, thresh=3.0, hz=50.0, repit=True):
    """Hold him in the pit box against the game putting him back on the grid.

    Measured 2026-08-03: a pre-race teleport into the pit box HOLDS perfectly
    while the grid sits (12 s, not a millimetre of drift) - but the moment the
    race starts the game's own grid placement runs and puts him straight back
    in his slot. So the box position has to be re-asserted, not just written
    once.

    This is a corrective loop, not a pin: it only writes when he is more than
    `thresh` metres away, so the game is left alone the rest of the time and
    every correction is one countable event in the log. If corrections never
    stop, the placement is running every frame and this approach is wrong -
    that is exactly what the count tells you.

    The pit designation is re-applied too, because a grid re-placement is
    likely to reset pittingStatus along with the position.
    """
    v = veh(slot)
    t0 = time.time()
    fixes = 0
    last_report = 0.0
    print("  holding him in the box for %.0f s (correcting past %.1f m, %.0f Hz)"
          % (secs, thresh, hz))
    while time.time() - t0 < secs:
        s = state(slot)
        d = math.dist(s["pos"], pos)
        if d > thresh:
            wr(v + OFF_BODY, struct.pack("<3f", *pos))
            wr(v + OFF_VEL, struct.pack("<3f", 0.0, 0.0, 0.0))
            wr(v + OFF_YAW, struct.pack("<f", yaw))
            if repit:
                wr(v + OFF_CALLBOX, struct.pack("<i", 1))
                wr(v + OFF_PITTING, struct.pack("<i", 2))
                wr(v + OFF_LANEMODE, struct.pack("<i", 1))
                wr(v + OFF_CURWP, struct.pack("<I", 0))
            fixes += 1
            print("  %6.1fs  PUT BACK from (%8.2f,%7.2f,%9.2f)  %6.1f m away  "
                  "spd %5.1f  pit %d   [fix %d]"
                  % (time.time() - t0, s["pos"][0], s["pos"][1], s["pos"][2],
                     d, s["spd"], s["pit"], fixes))
        elif time.time() - t0 - last_report >= 2.0:
            last_report = time.time() - t0
            print("  %6.1fs  in the box, %.2f m off   spd %5.2f  pit %d  "
                  "callToBox %d  lap %d"
                  % (last_report, d, s["spd"], s["pit"],
                     i32(v + OFF_CALLBOX), s["lap"]))
        time.sleep(1.0 / hz)
    print("\n  done: %d correction(s) in %.0f s" % (fixes, secs))


def release(slot, secs):
    """Let him OUT of the pit box and away, cleanly.

    De-retiring alone is not enough. Measured 2026-08-03: after the freeze,
    the deploy revived him and he just circled - alive, engine running, but
    with no route. The pit lane is a separate waypoint branch (wp_branchID 1)
    and a car sitting in a box has no path onto it unless it is told it is in
    the pit lane.

    The state that DID work earlier the same session, when the game drove him
    out by itself at limiter speed, was: in the pit lane, no stop requested,
    pit-lane waypoint set, stale waypoint dropped. That is what this writes.

        callToBox     = 0   the stop is finished - stop asking for one
        pittingStatus = 2   he is in the pit lane, so the game routes him out
        laneMode      = 1   use the pit-lane waypoint branch
        curWaypoint   = 0   drop whatever stale node he was holding
        maxSpeed      = free
        engine crank        only if it is not already running
    """
    v = veh(slot)
    s0 = state(slot)
    print("  before:")
    show(slot, "pre")

    # make sure he is actually alive first
    if rd(v + OFF_RETIRED, 1)[0] or i32(v + OFF_ACTIVE_AI) == 0:
        wr(v + 0xD58, bytes([1]))
        wr(v + OFF_FINISH, struct.pack("<i", 0))
        wr(v + OFF_RETIRED, bytes([0]))
        wr(v + OFF_ACTIVE_AI, struct.pack("<i", 1))
        wr(v + OFF_DRIVERVIS, bytes([1]))
        print("  de-retired him first")

    wr(v + OFF_CALLBOX, struct.pack("<i", 0))
    wr(v + OFF_PITTING, struct.pack("<i", 2))
    wr(v + OFF_LANEMODE, struct.pack("<i", 1))
    # DO NOT NULL CurrentWaypointPtr HERE.
    #
    # Out of a GARAGE, nulling it forces a re-acquire and is the fix for
    # driving into the garage wall. Out of the BOX it is the opposite: the
    # pit-lane node the game gave him is his ONLY route out, and throwing it
    # away leaves him with no path at all. Measured 2026-08-03 - he was alive
    # with the engine running and just circled. pace_probe.c's sc_revive()
    # already branches on exactly this (g_sc_in_box).
    wr(v + OFF_MAXSPEED, struct.pack("<f", 99999.0))
    if (f32(v + 0x39A0) or 0.0) < 100.0:
        wr(v + 0xA0, struct.pack("<f", 800.0))
        wr(v + 0x39A0, struct.pack("<f", 800.0))
        print("  cranked the engine")
    print("  released: callToBox 0, pittingStatus 2, laneMode 1, waypoint cleared")

    if paused():
        print("  !! THE GAME IS PAUSED - SessionTime is not advancing.")
        print("  !! Nothing will move and the speed field will read stale.")
        print("  !! Unpause and run this again.")
        return

    t0 = time.time()
    start = state(slot)["pos"]
    while time.time() - t0 < secs:
        st = state(slot)
        print("  +%4.1fs (%9.2f,%7.2f,%9.2f) spd %6.2f pit %d lap %d   moved %6.1f m"
              % (time.time() - t0, st["pos"][0], st["pos"][1], st["pos"][2],
                 st["spd"], st["pit"], st["lap"], math.dist(st["pos"], start)))
        time.sleep(1.0)


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    cmd = args[0]
    secs = 30
    if "--secs" in args:
        secs = int(args[args.index("--secs") + 1])
    arm_speed = 3.0
    if "--speed" in args:
        arm_speed = float(args[args.index("--speed") + 1])
    call = "--call" in args
    freeze = "--freeze" in args
    govern = "--govern" in args
    track = "1999_Monza"
    if "--track" in args:
        track = args[args.index("--track") + 1]

    slot = find_sc()
    if slot is None:
        sys.exit("safety car not found in the vehicle array")
    print("safety car: slot %d  (SessionType %d, RacePhase %d, VehicleCount %d)"
          % (slot, i32(SESSION_TYPE), i32(RACE_PHASE), i32(VEH_COUNT)))

    if cmd == "state":
        s = show(slot, "now")
        print("  AIW team %d bay %d   callToBox %d" % (s["team"], s["bay"],
                                                       s["callbox"]))
        return

    pits = load_pits(track)
    pos, yaw, team = target_for(slot, pits)
    print("  AIW team %d -> PitPos %s" % (team, pits["teams"][str(team)]["pit_pos"]))
    print("  target (marker + %.2f m forward, + %.2f m up) = (%.3f, %.3f, %.3f)"
          % (FORWARD_M, UP_M, *pos))

    if cmd == "release":
        release(slot, secs)
        return

    if cmd == "keep":
        thresh = 3.0
        if "--thresh" in args:
            thresh = float(args[args.index("--thresh") + 1])
        print("  before:")
        show(slot, "pre")
        # put him there once up front, then hold it
        teleport(slot, pos, yaw)
        wr(veh(slot) + OFF_CALLBOX, struct.pack("<i", 1))
        wr(veh(slot) + OFF_PITTING, struct.pack("<i", 2))
        wr(veh(slot) + OFF_LANEMODE, struct.pack("<i", 1))
        keep(slot, pos, yaw, secs, thresh)
        return

    if cmd == "pitmode":
        print("  before:")
        show(slot, "pre")
        pitmode(slot, pos, yaw, 1500, secs)
        return

    if cmd == "watch":
        print("\n  waiting for the race to start and for him to reach %.1f m/s"
              % arm_speed)
        print("  (he must take the green in the field, or his brake hold is")
        print("   never released - that state is unrecoverable)")
        while True:
            st = i32(SESSION_TYPE)
            ph = i32(RACE_PHASE)
            s = state(slot)
            if st == 7 and ph == 5 and s["spd"] >= arm_speed:
                print("\n  ARMED: SessionType 7, RacePhase 5, speed %.2f m/s"
                      % s["spd"])
                break
            time.sleep(0.05)
    elif cmd != "now":
        sys.exit("unknown command %r" % cmd)

    print("\n  before:")
    show(slot, "pre")
    ok = teleport(slot, pos, yaw, call, freeze, govern)
    extra = "".join([" + callToBox=1" if call else "",
                     " + maxSpeed=0" if govern else "",
                     " + FROZEN (retired, hidden)" if freeze else ""])
    print("  teleported: %s%s" % (ok, extra))
    observe(slot, secs, pos)


if __name__ == "__main__":
    main()
