"""
safety_car.py — NUMPAD1 safety car for F1 Challenge 99-02.

WHAT IT DOES
    On launch (race session), retires Bernd Maylander so he starts the race
    parked in his garage and out of the classification.
    NUMPAD1 (1st press)  DEPLOY  - revive Maylander, he drives out of the pit
                                   lane under his own AI; field capped to
                                   150 km/h with positions frozen.
    NUMPAD1 (2nd press)  RECALL  - Maylander is called to the box and retires
                                   again via the game's own path; cap released.

    Commentary fires in this order:
        SCAR01  on deploy
        SCAR02  when Maylander clears the pit limiter (on track)
        SCAR03  on recall
        SCAR05  shortly before he reaches the box
        SCAR04  when he is back in the pits

MECHANISMS (all proven, see the READMEs cited)
    Retire / revive  investigations/garage_exit_ignition/README.md
        A garaged car has its ENGINE OFF - the gate is ignition, not the
        retirement flags. Revival = clear classification + driverStatus=3 +
        one-shot crank at 0xA0/0x39A0. Null 0x4354 or a stale waypoint drives
        the car into the garage wall.
    Recall           mods/ai_wing_retire (force_wing_retire.py)
        veh+0xD58=0 (arm) + callToBox=1, then hand off: the game drives him in
        and garages him itself. Arm the trigger, never forge the result.
    Speed cap        mods/grid_reorder/gr28.c  (in-game confirmed)
        veh+0x4538 maxSpeed, ORIGINALS SAVED AND RESTORED.
        UNITS ARE m/s: every car reads 22.22 in the garage = the 80 km/h pit
        limit / 3.6. So 150 km/h = 41.67, NOT 150.
    Position lock    mods/grid_reorder/gr27.c  (in-game confirmed)
        snapshot veh+0x4060 racePos, rewrite every tick.
    Commentary       tools/sc_audio_test.py
        The five PWS_SCAR clips exist in PC_RE_MAIN.SDT but no event resolves
        to them. Repoint a carrier event's score, inject, restore.

WRITES TO THE RUNNING GAME. Run it with the game already in a race.

Usage:
    python mods/safety_car/safety_car.py            # arm and poll NUMPAD1
    python mods/safety_car/safety_car.py --no-retire   # skip the start retire
    python mods/safety_car/safety_car.py status
"""

import ctypes
import re
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32
user32 = ctypes.windll.user32

# ---- globals -------------------------------------------------------------
VEH_PTRS     = 0x007C54F0
VEH_COUNT    = 0x005C329C
PLAYER_VEH   = 0x007C54E8
SESSION_TYPE = 0x007C56A4
CONTROLLER   = 0x005EA618
EVENT_TABLE  = 0x005BF9A0
PWS_LOOKUP   = 0x005BEBB0

# ---- vehicle offsets -----------------------------------------------------
NAME      = 0x4620
RW_FLAG   = 0x0D58   # RearWingIntactFlag - 0 arms the retire at the box
ANGVEL    = 0x00A0   # EngineAngVel [FACT]
DRIVERVIS = 0x105D   # DriverModelVisible [FACT]
PITTING   = 0x3450
RPM       = 0x39A0   # EngineRPM
PITSTATE  = 0x4040
QUALPOS   = 0x4058   # 200 = excluded from the grid (gr29 apply_qualpos)
RACEPOS   = 0x4060
FINISH    = 0x4064
CURLAP    = 0x403C
CURWP     = 0x4354   # CurrentWaypointPtr [FACT] - null it to force reacquire
RETIRED   = 0x4410
DRVSTAT   = 0x4510   # driverStatus: 3 = approach
MAXSPEED  = 0x4538   # m/s
SPEED     = 0x4440   # m/s
CALLBOX   = 0x454C
ACTIVE    = 0x45F4

SC_NAME      = "Bernd Maylander"
CAP_KMH      = 150.0
CAP_MS       = CAP_KMH / 3.6          # 41.67 m/s
PIT_LIMIT_MS = 22.22                  # garage/pit reading, = 80 km/h
ON_TRACK_MS  = 25.0                   # clear of the limiter => he is out
CRANK_RPM    = 800.0
SESSION_RACE = 7

CARRIER_EID = 0x83
CLIP_SCORES = {"SCAR01": 132, "SCAR02": 133, "SCAR03": 134,
               "SCAR04": 135, "SCAR05": 131}

VK_NUMPAD1 = 0x61
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
    sys.exit("OpenProcess failed - if the game runs ELEVATED, use an Admin shell")


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
    """Write, lifting page protection - the event table lives in .rdata."""
    old = ctypes.c_ulong()
    k32.VirtualProtectEx(h, ctypes.c_void_p(a), len(data), 0x40, ctypes.byref(old))
    g = ctypes.c_size_t()
    buf = ctypes.create_string_buffer(data, len(data))
    ok = k32.WriteProcessMemory(h, ctypes.c_void_p(a), buf, len(data), ctypes.byref(g))
    k32.VirtualProtectEx(h, ctypes.c_void_p(a), len(data), old, ctypes.byref(old))
    return bool(ok) and g.value == len(data)


def wr32(a, v):
    return wr(a, struct.pack("<i", v))


def wrf(a, v):
    return wr(a, struct.pack("<f", v))


def wr8(a, v):
    return wr(a, bytes([v]))


# ---- commentary ----------------------------------------------------------
def play(clip):
    """Repoint the carrier event at an orphaned clip, inject, restore."""
    score = CLIP_SCORES[clip]
    sa = EVENT_TABLE + CARRIER_EID * 0x14 + 0x10
    orig = i32(sa)
    if not wr32(sa, score):
        log(f"  !! {clip}: carrier score write failed")
        return
    pri = i32(EVENT_TABLE + CARRIER_EID * 0x14)
    q = CONTROLLER + 0x864
    wr32(q + 0x00, CARRIER_EID)
    wr32(q + 0x04, score)
    wr32(q + 0x08, 0)
    wrf(q + 0x0C, -1.0)
    wr32(q + 0x10, -1)
    wr32(q + 0x14, -1)
    wr32(q + 0x18, -1)
    wr32(q + 0x1C, pri)
    wrf(CONTROLLER + 0xAF8 + CARRIER_EID * 4, 0.0)
    wrf(CONTROLLER + 0xAE8, 0.0)
    wr32(CONTROLLER + 0x135C, 1)
    time.sleep(0.30)
    wr32(sa, orig)
    log(f"  audio: {clip}")


# ---- vehicles ------------------------------------------------------------
def vehicles():
    n = i32(VEH_COUNT) or 0
    out = []
    for s in range(n):
        v = i32(VEH_PTRS + s * 4)
        if v:
            out.append((s, v))
    return out


def name_of(v):
    r = raw(v + NAME, 40)
    return "?" if r is None else r.split(b"\0")[0].decode("latin-1", "replace")


def find_sc():
    """Resolve Maylander BY NAME - enabling him shifts every other slot."""
    for s, v in vehicles():
        if SC_NAME.lower() in name_of(v).lower():
            return s, v
    return None, None


def log(msg):
    line = f"[{time.strftime('%H:%M:%S')}] {msg}"
    print(line, flush=True)
    with open("safety_car_log.txt", "a", encoding="utf-8") as f:
        f.write(line + "\n")


# ---- retire / revive -----------------------------------------------------
def exclude_from_grid(v):
    """Keep him OFF the starting grid. Same pair gr29's apply_qualpos() uses for
    pre-existing DNFs: QualPos 200 (out of range -> waypoint wraparound, the car
    is not placed in the grid slots) plus CurrentLap 0. This must land BEFORE the
    game forms the grid; applied after the start it does nothing."""
    wr32(v + QUALPOS, 200)
    wr32(v + CURLAP, 0)


def retire_to_garage(v):
    """Forge the retirement end-state. He is ALREADY in his garage pre-race, so
    freezing him in place parks him exactly where a real retirement would, and
    the synthetic case is the one proven easiest to revive."""
    wr32(v + FINISH, 2)
    wr8(v + RETIRED, 1)
    wr8(v + ACTIVE, 0)
    wr32(v + PITSTATE, 2)
    wr8(v + DRIVERVIS, 0)


def revive(v):
    """The garage-exit recipe. Order matters: disarm, revive, hand to the state
    machine, and only then crank - the engine write needs a running physics step,
    which ActiveFlag=1 restores."""
    wr8(v + RW_FLAG, 1)          # disarm, else he re-retires at the box
    wr32(v + CALLBOX, 0)
    wr32(v + PITTING, 0)
    wr32(v + FINISH, 0)
    wr8(v + RETIRED, 0)
    wr8(v + ACTIVE, 1)
    wr32(v + DRVSTAT, 3)         # approach - NOT 0
    wr8(v + DRIVERVIS, 1)        # else he drives with an empty cockpit
    wr32(v + CURWP, 0)           # force a fresh waypoint query (the wall fix)
    wrf(v + ANGVEL, CRANK_RPM)   # one-shot starter crank
    wrf(v + RPM, CRANK_RPM)


def send_to_box(v):
    """Arm the game's own retire trigger and hand off - it drives him in and
    garages him itself. Do NOT forge the end-state here."""
    wr8(v + RW_FLAG, 0)
    wr32(v + CALLBOX, 1)


# ---- field control -------------------------------------------------------
UNCAPPED = 99999.0   # the natural maxSpeed of a racing car (live-read)


class FieldControl:
    """Keyed BY SLOT, not by raw vehicle pointer. A session change reallocates the
    vehicle structs, and a tick that keeps writing through stale pointers is
    writing into freed game memory - that happened on the first live run."""

    def __init__(self):
        self.saved_speed = {}    # slot -> original maxSpeed
        self.locked_pos = {}     # slot -> racePos
        self.ptr = {}            # slot -> vehicle ptr AT ENGAGE TIME
        self.active = False

    def engage(self, exclude_slots):
        player = i32(PLAYER_VEH)
        self.saved_speed.clear()
        self.locked_pos.clear()
        self.ptr.clear()
        for s, v in vehicles():
            if v == player or s in exclude_slots:
                continue
            if u8(v + ACTIVE) != 1:
                continue
            orig = f32(v + MAXSPEED)
            # Never memorise a pit-limiter value as the "original" - restoring it
            # later would leave the car permanently limited to 80 km/h.
            if orig is None or orig < CAP_MS * 1.5:
                orig = UNCAPPED
            self.saved_speed[s] = orig
            self.locked_pos[s] = i32(v + RACEPOS)
            self.ptr[s] = v
        self.active = True
        log(f"  cap {CAP_KMH:.0f} km/h ({CAP_MS:.2f} m/s) + position lock on "
            f"{len(self.saved_speed)} cars (player free)")

    def _live(self, s):
        """Re-resolve the slot and refuse to write if the struct moved."""
        v = i32(VEH_PTRS + s * 4)
        return v if v and v == self.ptr.get(s) else None

    def tick(self):
        if not self.active:
            return
        for s, pos in self.locked_pos.items():
            v = self._live(s)
            if v is None:
                continue
            wrf(v + MAXSPEED, CAP_MS)
            wr32(v + RACEPOS, pos)

    def release(self):
        if not self.active:
            return
        n = 0
        for s, orig in self.saved_speed.items():
            v = self._live(s)
            if v is None:
                v = i32(VEH_PTRS + s * 4)     # struct moved: restore anyway
            if v:
                wrf(v + MAXSPEED, orig)
                n += 1
        self.active = False
        log(f"  cap released, {n} cars restored")


# ---- states --------------------------------------------------------------
IDLE, OUT_LAP, ON_TRACK, RECALLING, RETURNING = range(5)


def cmd_release():
    """Panic button: un-cap every car. Use if a controller was killed mid-deploy."""
    n = 0
    for s, v in vehicles():
        ms = f32(v + MAXSPEED)
        if ms is not None and ms < UNCAPPED * 0.5:
            wrf(v + MAXSPEED, UNCAPPED)
            n += 1
    print(f"restored {n} car(s) to maxSpeed {UNCAPPED}")


def cmd_prep(use_qualpos=False):
    """Run this in the PRE-RACE session (garage), before the grid is formed.

    QualPos=200 is OPT-IN (--qualpos). Live 2026-07-25 it did keep him off the
    grid (P1 slot empty) but it also blanked the pre-race grid listing entirely
    and left him parked at the end of the grid. Retiring him alone may park him
    in his garage without either side effect - that is the cheaper thing to try
    first."""
    s, v = find_sc()
    if v is None:
        sys.exit(f"'{SC_NAME}' not found")
    sess = i32(SESSION_TYPE)
    print(f"slot {s} {name_of(v)} @ 0x{v:x}  session={sess}")
    print(f"  before: qualPos={i32(v+QUALPOS)} curLap={i32(v+CURLAP)} "
          f"retired={u8(v+RETIRED)} active={u8(v+ACTIVE)} finish={i32(v+FINISH)}")
    if sess == SESSION_RACE:
        print("  WARNING: already in the race session - the grid is likely built,")
        print("           QualPos may no longer keep him off it.")
    if use_qualpos:
        exclude_from_grid(v)
        print("  QualPos=200 applied (--qualpos)")
    retire_to_garage(v)
    print(f"  after : qualPos={i32(v+QUALPOS)} curLap={i32(v+CURLAP)} "
          f"retired={u8(v+RETIRED)} active={u8(v+ACTIVE)} finish={i32(v+FINISH)}")
    print("  now start the race and check whether he is on the grid.")


def cmd_status():
    s, v = find_sc()
    if v is None:
        print(f"'{SC_NAME}' NOT FOUND in a {i32(VEH_COUNT)}-car field")
        return
    print(f"session={i32(SESSION_TYPE)} (race={SESSION_RACE})")
    print(f"slot {s} {name_of(v)} @ 0x{v:x}")
    print(f"  retired={u8(v+RETIRED)} active={u8(v+ACTIVE)} finish={i32(v+FINISH)} "
          f"dStat={i32(v+DRVSTAT)} spd={f32(v+SPEED):.2f} m/s "
          f"pos=({f32(v+0x1C):.2f},{f32(v+0x24):.2f})")


def main(do_retire=True):
    slot, sc = find_sc()
    if sc is None:
        sys.exit(f"'{SC_NAME}' not found - enable him in the driver roster first")
    log(f"safety car = slot {slot} '{name_of(sc)}' @ 0x{sc:x}")

    sess = i32(SESSION_TYPE)
    if sess != SESSION_RACE:
        log(f"WARNING: session is {sess}, not a race ({SESSION_RACE}) - "
            f"deploying anyway is untested")

    if do_retire:
        retire_to_garage(sc)
        log("Maylander retired to his garage for the start")

    field = FieldControl()
    state = IDLE
    t_mark = 0.0
    prev_key = False
    log("armed. NUMPAD1 = deploy, NUMPAD1 again = recall. Ctrl-C to quit.")

    try:
        while True:
            key = bool(user32.GetAsyncKeyState(VK_NUMPAD1) & 0x8000)
            pressed = key and not prev_key      # rising edge only
            prev_key = key

            # A session change reallocates every vehicle struct. Bail out rather
            # than keep writing through pointers captured in the old session.
            now_sess = i32(SESSION_TYPE)
            now_sc = i32(VEH_PTRS + slot * 4)
            if now_sess != sess or now_sc != sc:
                if state != IDLE:
                    log("session changed mid-sequence - releasing field, back to IDLE")
                    field.release()
                    state = IDLE
                sess = now_sess
                slot, sc = find_sc()
                if sc is None:
                    log("safety car vanished after session change - waiting")
                    time.sleep(1.0)
                    continue
                log(f"session now {sess}, safety car re-resolved: slot {slot} @ 0x{sc:x}")

            if pressed and state == IDLE:
                if sess != SESSION_RACE:
                    log(f"REFUSED: session is {sess}, not a race ({SESSION_RACE}). "
                        f"Deploying outside a race caps cars that are still in the "
                        f"garage and the revive has no race to run in.")
                    time.sleep(0.3)
                    continue
                log("DEPLOY")
                revive(sc)
                field.engage(exclude_slots={slot})
                play("SCAR01")
                state, t_mark = OUT_LAP, time.time()

            elif pressed and state in (OUT_LAP, ON_TRACK):
                log("RECALL")
                play("SCAR03")
                state, t_mark = RECALLING, time.time()

            if state == OUT_LAP:
                # He is on track once he is clear of the pit limiter.
                if f32(sc + SPEED) > ON_TRACK_MS:
                    play("SCAR02")
                    state = ON_TRACK
                elif time.time() - t_mark > 45:
                    # ABORT, do not pretend he is out. Leaving the cap on while
                    # the safety car is stuck strands the whole field at 150.
                    log(f"  !! ABORT: never moved in 45 s "
                        f"(retired={u8(sc+RETIRED)} active={u8(sc+ACTIVE)} "
                        f"dStat={i32(sc+DRVSTAT)} RPM={f32(sc+RPM):.0f}) "
                        f"- releasing field, back to IDLE")
                    field.release()
                    state = IDLE

            elif state == RECALLING and time.time() - t_mark > 4.0:
                play("SCAR05")
                send_to_box(sc)
                state, t_mark = RETURNING, time.time()

            elif state == RETURNING:
                # The game retires him itself once he reaches the box.
                if u8(sc + RETIRED) == 1 or u8(sc + ACTIVE) == 0:
                    play("SCAR04")
                    field.release()
                    log("safety car in. Green flag.")
                    state = IDLE
                elif time.time() - t_mark > 120:
                    log("  !! never reached the box - releasing the field anyway")
                    field.release()
                    state = IDLE

            field.tick()
            time.sleep(0.05)

    except KeyboardInterrupt:
        log("interrupted - restoring field")
        field.release()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "status":
        cmd_status()
    elif len(sys.argv) > 1 and sys.argv[1] == "prep":
        cmd_prep(use_qualpos="--qualpos" in sys.argv)
    elif len(sys.argv) > 1 and sys.argv[1] == "release":
        cmd_release()
    else:
        main(do_retire="--no-retire" not in sys.argv)
