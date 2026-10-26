#!/usr/bin/env python3
"""
trace_full_pitstop.py - capture EVERYTHING the game activates during a real
pit stop: what makes a pit-lane crossing a REAL FULL pit stop.

Monitored, per sample (10 Hz), change-logged with the session time:
  * the target car's vehicle struct: the pit machine (veh+0x32CC region),
    pittingStatus, driverStatus, callToBox, laneMode, maxSpeed, the service
    cluster, fuel, tyre choice, the schedule, lap/pos/retire fields
  * the 34 pit-crew actors (veh+0x32D4 -> crew+0x10, stride 120): every byte
    change per actor, with the actor's .ANM name (actor+100)
  * the Steward (0x8ACEE0): the XPITIN/XPITOUT/XFINISH poly handles (+0x194..)
    and the 29 marshalling zones (+0x110, stride 8)
  * the AI state ring block (dword_8AAE64): the +8 state byte StateChangeNotify
    writes
  * the rest of the field: any car entering the pit lane (pit>=2) - lane traffic

Log: docs/pitstop_sequence_trace.txt (append). End: pit 4->0 done AND speed >
45 past the merge, or the timeout.

USAGE
    python tools/trace_full_pitstop.py [--car NAME] [--secs 300]
"""

import ctypes
import ctypes.wintypes as wt
import io
import os
import struct
import subprocess
import sys
import time

EXE = "F1 Challenge Online.exe"
VEH = 0x5EC6D8
STRIDE = 0x48C0
SLOTS = 104
STEWARD = 0x8ACEE0
SESS = 0x8AFEA0
PHASE = 0x7C56A8
AI_STATE_RING = 0x8AAE64
CREW_STRIDE = 120

CAR = "Bernd Maylander"
SECS = 300

LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs",
                   "pitstop_sequence_trace.txt")


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

    def i(self, a):
        r = self.rd(a, 4)
        return None if r is None else struct.unpack("<i", r)[0]

    def f(self, a):
        r = self.rd(a, 4)
        return None if r is None else struct.unpack("<f", r)[0]

    def d(self, a):
        r = self.rd(a, 8)
        return None if r is None else struct.unpack("<d", r)[0]


def veh(i):
    return VEH + STRIDE * i


def find_by_name(m, want):
    for i in range(SLOTS):
        if m.i(veh(i) + 0x32C8) == -1:
            continue
        raw = m.rd(veh(i) + 0x4620, 32)
        if raw and raw.split(b"\0")[0].decode("latin-1", "replace").startswith(want):
            return i
    return -1


# vehicle fields to watch: (offset, label, kind)  kind: i=int32, f=float, b=byte
VWATCH = [
    (0x32CC, "machBase", "p"), (0x3450, "pitState", "i"), (0x345C, "svcReady", "b"),
    (0x346C, "stopStart", "f"), (0x3470, "stopEnd", "f"), (0x3474, "measured", "f"),
    (0x3478, "lollipop", "i"), (0x347C, "fuelAtStart", "f"), (0x3480, "fuelRampStart", "f"),
    (0x3484, "targetFuel", "f"), (0x3488, "refuelNow", "f"), (0x348C, "refuelFlag", "i"),
    (0x3490, "pinFlag", "i"), (0x3494, "tyreFlag", "i"), (0x3498, "wingFlag", "i"),
    (0x349C, "tyreJobStart", "f"), (0x34A0, "tyreJobEnd", "f"),
    (0x4510, "dS", "i"), (0x4534, "tExit", "f"), (0x4538, "maxSp", "f"),
    (0x4544, "f4544", "i"), (0x4548, "f4548", "i"), (0x454C, "callToBox", "i"),
    (0x4550, "laneMode", "i"), (0x4040, "PitState", "i"), (0x403C, "lap", "i"),
    (0x4440, "spd", "f"), (0x397C, "tireChoice", "f"), (0x3338, "fuel", "f"),
    (0x4064, "finish", "i"), (0x4068, "reason", "i"), (0x4410, "retired", "b"),
    (0x45F4, "active", "i"), (0x406C, "pitVisits", "i"),
    (0x4588, "stopsCount", "i"), (0x4590, "stopLap0", "i"),
    (0x34E0, "posX", "f"), (0x34E8, "posZ", "f"),
]


def main():
    global CAR, SECS
    if "--car" in sys.argv:
        CAR = sys.argv[sys.argv.index("--car") + 1]
    if "--secs" in sys.argv:
        SECS = int(sys.argv[sys.argv.index("--secs") + 1])

    m = Mem()
    i = find_by_name(m, CAR)
    if i < 0:
        print("car %r not found" % CAR)
        return 1
    v = veh(i)
    print("car %s slot %d @0x%08X  watching %d s" % (CAR, i, v, SECS))

    out = io.open(LOG, "a", encoding="utf-8")
    def log(s):
        print(s)
        out.write(s + "\n")

    sess0 = m.d(SESS)
    log("==== pitstop full-sequence trace  %s  (slot %d) ===="
        % (time.strftime("%H:%M:%S"), i))

    prev = {}
    prev_crew = {}
    prev_zones = None
    prev_stew = {}
    prev_ring = {}
    prev_act = {}

    t0 = time.time()
    pit_last = 0
    done = False
    while time.time() - t0 < SECS:
        now = time.time() - t0
        sess = m.d(SESS)
        j = find_by_name(m, CAR)
        if j < 0:
            log("  !! car lost at t+%.1f" % now)
            break
        v = veh(j)
        ph = m.i(PHASE)

        # --- vehicle fields ---
        for off, label, kind in VWATCH:
            if kind == "i":
                val = m.i(v + off)
            elif kind == "f":
                val = m.f(v + off)
            elif kind == "b":
                r = m.rd(v + off, 1)
                val = None if r is None else r[0]
            else:
                continue
            if val is None:
                continue
            if off in prev and prev[off] != val:
                log("  T+%5.1f [s%7.1f ph%d] %-13s %r -> %r"
                    % (now, sess, ph, label, prev[off], val))
            prev[off] = val

        # --- pit state machine transitions (event form) ---
        pit = m.i(v + 0x3450)
        if pit != pit_last:
            log("  T+%5.1f [s%7.1f ph%d] >>> PIT STATE %d -> %d"
                % (now, sess, ph, pit_last, pit))
            pit_last = pit

        # --- crew actors ---
        crew = m.i(v + 0x32D4)
        if crew:
            count = m.i(crew + 0x0C) or 0
            arr = m.i(crew + 0x10)
            if arr:
                for a in range(min(count, 34)):
                    abase = arr + a * CREW_STRIDE
                    blk = m.rd(abase, CREW_STRIDE)
                    if not blk:
                        continue
                    if a in prev_crew and prev_crew[a] != blk:
                        diffs = [o for o in range(CREW_STRIDE)
                                 if prev_crew[a][o] != blk[o]]
                        nm = m.rd(abase + 100, 24) or b"?"
                        nm = nm.split(b"\0")[0].decode("latin-1", "replace")
                        # only report the interesting byte ranges
                        sig = ",".join("+%d:%02x->%02x" % (o, prev_crew[a][o], blk[o])
                                       for o in diffs if o < 100 or o >= 104)
                        log("  T+%5.1f [s%7.1f] CREW actor %2d %-8s changed: %s"
                            % (now, sess, a, nm[:8], sig))
                    prev_crew[a] = blk

        # --- steward: polys + zones ---
        for off, label in ((0x194, "xpitin"), (0x198, "xpitout"),
                           (0x19C, "xfinish"), (0x1A0, "xsector1"), (0x1A4, "xsector2")):
            hh = m.i(STEWARD + off)
            if hh is not None and (off not in prev_stew or prev_stew[off] != hh):
                if off in prev_stew:
                    log("  T+%5.1f [s%7.1f] STEWARD %-9s 0x%08X -> 0x%08X"
                        % (now, sess, label, prev_stew[off], hh))
                prev_stew[off] = hh
        zc = m.i(STEWARD + 0x10C) or 0
        if zc:
            zblk = m.rd(STEWARD + 0x110, zc * 8)
            if zblk and prev_zones is not None and zblk != prev_zones:
                for z in range(zc):
                    a = z * 8
                    if prev_zones[a:a+8] != zblk[a:a+8]:
                        st = struct.unpack("<i", zblk[a:a+4])[0]
                        end = struct.unpack("<f", zblk[a+4:a+8])[0]
                        log("  T+%5.1f [s%7.1f] ZONE %2d -> state %d end %.1f"
                            % (now, sess, z, st, end))
            prev_zones = zblk

        # --- AI state ring block ---
        ring = m.i(AI_STATE_RING)
        if ring:
            st8 = m.rd(ring + 8, 1)
            if st8 is not None and (ring not in prev_ring or prev_ring[ring] != st8):
                if ring in prev_ring:
                    log("  T+%5.1f [s%7.1f] AISTATEBLOCK %d -> %d"
                        % (now, sess, prev_ring[ring], st8[0]))
                prev_ring[ring] = st8

        # --- the rest of the field: lane traffic ---
        lane_traffic = []
        for s in range(SLOTS):
            w = veh(s)
            if s == j or m.i(w + 0x32C8) == -1:
                continue
            p = m.i(w + 0x3450)
            if p >= 2:
                nm = m.rd(w + 0x4620, 24) or b"?"
                nm = nm.split(b"\0")[0].decode("latin-1", "replace")
                lane_traffic.append("%s(pit%d)" % (nm[:16], p))
        key = tuple(lane_traffic)
        if key != prev_act:
            if prev_act:
                log("  T+%5.1f [s%7.1f] LANE TRAFFIC: %s"
                    % (now, sess, ", ".join(lane_traffic) if lane_traffic else "(empty)"))
            prev_act = key

        # --- done? pit completed 4->0 and accelerating past the merge ---
        if pit_last == 0 and prev.get(0x3450) == 0 and \
                prev.get(0x4440) or 0 > 45 and m.f(v + 0x34E8) > 800:
            done = True
        if pit_last == 0 and m.i(v + 0x3450) == 0 and \
                (m.f(v + 0x4440) or 0) > 45 and (m.f(v + 0x34E8) or 0) > 800:
            log("==== EXIT COMPLETE at t+%.1f ====" % now)
            done = True

        time.sleep(0.1)

    if not done:
        log("==== trace timeout (%.0f s) ====" % SECS)
    out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
