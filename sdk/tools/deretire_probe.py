"""
deretire_probe.py — bring a retired (or garaged) car back onto the track.

MECHANISM: investigations/garage_exit_ignition/README.md

Clearing the retirement flags is NOT enough and never was. A car sitting in its
garage has its ENGINE OFF; the AI happily selects 1st gear, applies throttle and
steers into a dead engine, so the car does not move. Three layers are needed:

  classification  FinishState / RetiredFlag / ActiveFlag   car counts as racing
  state machine   0x4510 driverStatus = 3 (approach)       releases holds
  IGNITION        0xA0 EngineAngVel + 0x39A0 EngineRPM     <-- the actual gate

The engine write is a ONE-SHOT STARTER CRANK, not a drive input: physics takes
the value over within a frame and runs its own numbers. Same shape as the retire
mechanism - arm the trigger, never forge the result.

IT IS A REVERSIBLE FREEZE, NOT A RETIREMENT. `synthretire` does NOT end a
driver's race. The DNF and retired flags are a state the game READS, not a
decision it has already taken and recorded, so setting them stops the car dead
and clearing them puts the SAME car back into the SAME race, still classified
and still able to score.

Proven live 2026-08-03, mid-race: Rubens Barrichello, LEADING at 67.93 m/s,
was frozen (coasted ~88 m, stopped dead, racePos slipped 1 -> 21 as the field
drove past) and then restored - away cleanly, 9 m/s in 1st at t+1.5 s through
75.8 m/s in 5th and 371 m covered by t+9.0 s, driverStatus dropping 3 -> 0 as
the AI took itself back to normal racing.

Four things that go with it:
  * the lost track position is REAL, not a bug - a freeze costs exactly the
    time it lasts, because classification is recomputed from distance;
  * the model must stay HIDDEN while frozen, or the car has no ground
    collision and falls through the world (measured PhysY -13513);
  * veh+0x4440 speed goes STALE while frozen and keeps reporting the speed the
    car had when it stopped - judge motion by POSITION, never by that field;
  * NEVER freeze a car before the green: one that is frozen or hidden at the
    lights never has its brake hold released, and that IS unrecoverable.

PROVEN END TO END on genuinely retired cars in a race, 3/3 (2026-07-25):
Raikkonen, Alesi and Alonso each drove out of the garage, down the pit lane at
pit speed (laneMode 1), exited onto the track at the pit-exit waypoint, braked
for corners and reached 6th gear. Raikkonen gained a place (racePos 20 -> 19).
No pathing writes were needed - the AI finds the pit lane itself.

SCOPE CORRECTION, 2026-08-03 - THERE ARE TWO KINDS OF RETIREMENT AND THIS ONLY
UNDOES ONE OF THEM.

  * a car frozen by `synthretire` (or otherwise left in that flag state) comes
    back every time: Wurz, Hakkinen and Panis, three for three in one session;
  * a car the GAME retired through its own teardown - the wing/pit-call path,
    or a component failure such as veh+0x3D70 expiring - does NOT come back.
    AIControlFlag (veh+0x45F4) is force-cleared EVERY TICK afterwards, whatever
    is restored: measured with RetiredFlag, FinishState, ReasonCode, PitState
    and EngineLifeRemaining all cleared first, on two different cars, with no
    mod running. The code that clears it is NOT yet identified (open item: an
    x64dbg hardware WRITE breakpoint on that address - Frida hardware
    watchpoints crash this process).

So: never let the game retire a car you intend to bring back.

ALSO MISSING FROM THE RECIPE BELOW: veh+0x4040 (PitState) = 2 prevents
AIControlFlag from holding 1, and nothing here clears it. `synthretire` SETS it,
so the reverse operation should clear it - necessary, though not sufficient for
a game-retired car.

LIMBO IS SOLVED (2026-08-03, FACT - see findings `limbo-unlocked-carconditions`):
a car in the ret=0/act=0/fin=0 'limbo' state, or a properly retired car sitting
in its garage (carConditions=2), is RE-RETIRED every ~1 s because
veh+0x3D6C carConditions==2 makes AICollision_004bef60 return 2, which the AI
tick loop turns into event 26 -> PitCrewStateMachine -> AIStateReset
(RetiredFlag=1 + AIControlFlag=0). The unlock that makes the revive stick is
part of the write set below: carConditions=0 plus the wheel-record attach
flags (+0x64/+0x84 of each record at veh+0x834, stride 0x128). Proven live
twice: limbo Maylander left the pits and raced at 140 km/h; an ignition-off
retirement (engine held at 0 for ~14 s, game retires with fin=2 and teleports
to the garage) revived the same way.

CAUSING a retirement with a dead engine (2026-08-03, FACT): hold
EngineAngVel(0xA0) and EngineRPM(0x39A0) at 0.0 for ~10-15 s while the AI
drives - the engine never catches, the car sits still, and the game runs its
engine-failure retirement (ret=1, fin=2, garage teleport). A single zero-write
is NOT enough - the game re-cranks within ~3.5 s.

WRITES TO THE RUNNING GAME.

Usage:
    python tools/deretire_probe.py state       <slot>
    python tools/deretire_probe.py synthretire <slot>
        (forge the retirement end-state instead of arming the game's trigger -
         freezes the car where it stands, no garage teleport)
    python tools/deretire_probe.py deretire <slot> [secs]
    python tools/deretire_probe.py deretire <slot> [secs] --no-engine
        (skips the crank - reproduces the OLD failing behaviour, for contrast)
    python tools/deretire_probe.py deretire <slot> [secs] --lane-only
        (also seeds 0x4550 laneMode=1 - REFUTED as necessary, a garaged car
         already reads laneMode=1; kept only for contrast experiments)
"""

import ctypes
import re
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

VEH_PTRS  = 0x007C54F0
VEH_COUNT = 0x005C329C
NAME_OFF  = 0x4620

RW_FLAG  = 0xD58     # RearWingIntactFlag - disarm or it re-retires at the box
CALLBOX  = 0x454C
PITTING  = 0x3450
FINISH   = 0x4064
RETIRED  = 0x4410
ACTIVE   = 0x45F4
DRVSTAT  = 0x4510    # driverStatus [CONFIRMED] 0=follow 2=garage 3=approach 5=pit-entry
ANGVEL   = 0x00A0    # EngineAngVel [FACT]
RPM      = 0x39A0    # EngineRPM [CONFIRMED]
THROTTLE = 0x357C
BRAKE    = 0x3584
GEAR     = 0x39FC
SPEED    = 0x4440
RACEPOS  = 0x4060
POSX, POSY, POSZ = 0x1C, 0x20, 0x24

# Pathing. sub_426430 passes 0x4550 as the waypoint-set selector to
# SpatialNearestNeighborQueryWithPlaneTest (0 = racing line, 1 = pit lane) and
# then rewrites it every frame from the acquired waypoint's +0x12C, so a seed
# only holds if the game agrees the car really is on that path. 0x4354 is the
# current-waypoint pointer; NULL forces a fresh nearest query (line 76).
LANEMODE = 0x4550
CURWP    = 0x4354

# A genuine retirement clears this, so a revived car drives with an empty
# cockpit (observed on screen). Latching write, FACT tier - see mods/_README.md.
DRIVERVIS = 0x105D

CRANK_RPM = 800.0

PROC = "F1 Challenge Online.exe"


def find_pid():
    out = subprocess.check_output(
        ["tasklist", "/FI", f"IMAGENAME eq {PROC}"], text=True, errors="replace")
    for line in out.splitlines():
        if "F1 Challenge" in line:
            m = re.search(r"\b(\d+)\b", line)
            if m:
                return int(m.group(1))
    return None


pid = find_pid()
if not pid:
    sys.exit("game not running")
h = k32.OpenProcess(0x0010 | 0x0020 | 0x0008 | 0x0400, 0, pid)
if not h:
    sys.exit("OpenProcess failed - the game runs ELEVATED, use an Administrator shell")


def raw(a, n):
    b = ctypes.create_string_buffer(n)
    g = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(g))
    return b.raw if ok and g.value == n else None


def i32(a):
    r = raw(a, 4)
    return None if r is None else int.from_bytes(r, "little", signed=True)


def u8(a):
    r = raw(a, 1)
    return None if r is None else r[0]


def f32(a):
    r = raw(a, 4)
    return None if r is None else struct.unpack("<f", r)[0]


def wr(a, data):
    g = ctypes.c_size_t()
    buf = ctypes.create_string_buffer(data, len(data))
    ok = k32.WriteProcessMemory(h, ctypes.c_void_p(a), buf, len(data), ctypes.byref(g))
    return bool(ok) and g.value == len(data)


def F(x):
    return struct.pack("<f", x)


def veh_of(slot):
    n = i32(VEH_COUNT) or 0
    if not (0 <= slot < n):
        sys.exit(f"slot {slot} out of range (count={n})")
    p = i32(VEH_PTRS + slot * 4)
    if not p:
        sys.exit(f"slot {slot} has a null vehicle pointer")
    return p


def name(v):
    r = raw(v + NAME_OFF, 40)
    return "?" if r is None else r.split(b"\0")[0].decode("ascii", "replace")


def line(v, t=None, d=None):
    s = (f"dStat={i32(v + DRVSTAT)} retired={u8(v + RETIRED)} active={u8(v + ACTIVE)} "
         f"finish={i32(v + FINISH)} RPM={f32(v + RPM):8.1f} thr={f32(v + THROTTLE):5.3f} "
         f"brk={f32(v + BRAKE):5.3f} gear={i32(v + GEAR)} spd={f32(v + SPEED):6.2f} "
         f"racePos={i32(v + RACEPOS)} lane={i32(v + LANEMODE)} wp=0x{i32(v + CURWP) & 0xffffffff:x} "
         f"pos=({f32(v + POSX):7.2f},{f32(v + POSZ):8.2f})")
    if t is not None:
        s = f"t+{t:5.1f} moved={d:7.2f} " + s
    return s


def cmd_state(slot):
    v = veh_of(slot)
    print(f"slot {slot} {name(v)} @ 0x{v:x}")
    print("  " + line(v))


def cmd_synthretire(slot):
    """Retire a car by FORGING the end-state (the ai_pit_retire write set) rather
    than by arming the game's own trigger. Freezes the car where it stands - no
    garage teleport. Documented in mods/_README.md as reversible."""
    v = veh_of(slot)
    print(f"slot {slot} {name(v)} @ 0x{v:x}")
    print("  before: " + line(v))
    for off, data, label in [
        (FINISH,    (2).to_bytes(4, "little"), "FinishState=2 (DNF)"),
        (RETIRED,   bytes([1]),                "RetiredFlag=1"),
        (ACTIVE,    bytes([0]),                "ActiveFlag=0"),
        (0x4040,    (2).to_bytes(4, "little"), "PitState=2"),
        (DRIVERVIS, bytes([0]),                "DriverModelVisible=0"),
    ]:
        print(f"  wrote {label}: {wr(v + off, data)}")
    time.sleep(2.0)
    print("  after : " + line(v))


def cmd_deretire(slot, secs, engine=True, pitlane=False):
    v = veh_of(slot)
    print(f"slot {slot} {name(v)} @ 0x{v:x}")
    print("  before: " + line(v))

    # Order matters: disarm the retire trigger FIRST, revive, hand to the state
    # machine, and only then crank - the engine write needs a running physics
    # step, which ActiveFlag=1 restores.
    steps = [
        (RW_FLAG,  bytes([1]),                 "RWflag=1 (disarm)"),
        (CALLBOX,  (0).to_bytes(4, "little"),  "callToBox=0"),
        (PITTING,  (0).to_bytes(4, "little"),  "pitting=0"),
        (FINISH,   (0).to_bytes(4, "little"),  "FinishState=0"),
        (RETIRED,  bytes([0]),                 "RetiredFlag=0"),
        (ACTIVE,   bytes([1]),                 "ActiveFlag=1"),
        (DRVSTAT,  (3).to_bytes(4, "little"),  "driverStatus=3 (approach)"),
        (DRIVERVIS, bytes([1]),                "DriverModelVisible=1"),
        # PitState 2 CLAMPS AIControlFlag TO 0 (measured 2026-08-03): with it
        # left set, ActiveFlag=1 read back 0 within 250 ms every time.
        # `synthretire` sets it, so the reverse has to clear it.
        (0x4040,   (0).to_bytes(4, "little"),  "PitState=0"),
    ]
    # A car retired by the game's own path already has curWaypoint NULL, which
    # makes sub_426430 do a fresh nearest query. A car left holding a STALE
    # waypoint (e.g. one revived out of the limbo state) skips that query unless
    # it is >3200 away (line 87) and drives at a pre-retirement racing-line node
    # - that is what put M.Schumacher into the garage wall. Nulling is a no-op
    # on a clean subject and the fix on a stale one.
    steps.append((CURWP, (0).to_bytes(4, "little"), "curWaypoint=NULL"))
    # THE LIMBO GATE (2026-08-03, FACT): veh+0x3D6C carConditions==2 (pit
    # mode) makes AICollision_004bef60 return 2, and the AI tick loop
    # (EngineFailureCheck_004bec55) then raises event 26 -> PitCrewStateMachine
    # -> AIStateReset_0042af40 -> RetiredFlag=1 + AIControlFlag=0. A car
    # retired INTO the garage reads carConditions=2 (the retirement sets it),
    # so without this write EVERY revive is re-retired within ~1 s. This is
    # also why the 2026-07-25 'limbo is unrecoverable' verdict was wrong.
    steps.append((0x3D6C, (0).to_bytes(4, "little"),
                  "carConditions=0 (AICollision re-retire gate)"))
    # Wheel-record attach flags: AICollision returns 1 when a wheel record's
    # +0x64 byte is 0 (records at veh+0x834, stride 0x128). A retirement with
    # wheels off / a dead car reads 0; healthy cars read non-zero. Restore the
    # +0x64 and +0x84 flag bytes so the collision check passes.
    for k in range(4):
        wb = 0x834 + k * 0x128
        steps.append((wb + 0x64, bytes([1]), f"wheel{k} attach+0x64=1"))
        steps.append((wb + 0x84, bytes([1]), f"wheel{k} attach+0x84=1"))

    if pitlane in ("both", "lane"):
        steps.append((LANEMODE, (1).to_bytes(4, "little"),
                      "laneMode=1 (pit-lane waypoint set)"))
    if engine:
        steps += [
            (ANGVEL, F(CRANK_RPM), f"EngineAngVel={CRANK_RPM} (crank)"),
            (RPM,    F(CRANK_RPM), f"EngineRPM={CRANK_RPM} (crank)"),
        ]
    for off, data, label in steps:
        print(f"  wrote {label}: {wr(v + off, data)}")
    if not engine:
        print("  --no-engine: skipping ignition (expect the car NOT to move)")

    print("  after : " + line(v))

    x0, z0 = f32(v + POSX), f32(v + POSZ)
    t0 = time.time()
    cranked_again = False
    while time.time() - t0 < secs:
        time.sleep(1.5)
        t = time.time() - t0
        x, z = f32(v + POSX), f32(v + POSZ)
        d = ((x - x0) ** 2 + (z - z0) ** 2) ** 0.5
        print("  " + line(v, t, d))
        # One retry if the crank was swallowed while physics was still resuming.
        if engine and not cranked_again and t > 3.0 and d < 0.5 and f32(v + RPM) < 1.0:
            print("  engine still dead - re-cranking once")
            wr(v + ANGVEL, F(CRANK_RPM))
            wr(v + RPM, F(CRANK_RPM))
            cranked_again = True


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    c = sys.argv[1]
    if c == "state":
        cmd_state(int(sys.argv[2]))
    elif c == "synthretire":
        cmd_synthretire(int(sys.argv[2]))
    elif c == "deretire":
        secs = 40
        if len(sys.argv) > 3 and sys.argv[3].isdigit():
            secs = int(sys.argv[3])
        cmd_deretire(int(sys.argv[2]), secs,
                     engine="--no-engine" not in sys.argv,
                     pitlane=("lane" if "--lane-only" in sys.argv else None))
    else:
        sys.exit(__doc__)
