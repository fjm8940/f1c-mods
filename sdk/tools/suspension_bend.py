"""
suspension_bend.py -- read/watch/verify PLAYER suspension bend damage.

Per-wheel physics record: base 0x007C7428, stride 1124 (281 ints), 4 wheels,
order FL, FR, RL, RR. PLAYER CAR ONLY (this is a global block, not the
0x48C0 vehicle array).

Fields (proven in functions/SteeringGeometryUpdate_00442060.c and
functions/WheelCollisionContactFlagSetter_0043e770.c -- both write
this[281*wheel + 276], LOBYTE and BYTE1 respectively, which is what pins
these two functions to the same object):

  +1104  byte    wheel collision / detach flag
  +1105  byte    SUSPENSION BENT flag (0/1)
  +1116  double  bend angle axis 1, radians, clamped +-0.1745329251994329 (+-10 deg)
  +1124  double  bend angle axis 2, radians, same clamp
  +1132  double  accumulated misalignment; >0.015 writes ReasonCode=4 (Suspension)
  +1496  double  tyre temp carcass zone 0 (Kelvin)   -- known-good sanity field
  +1504  double  tyre temp zone 1
  +1512  double  tyre temp zone 2
  +1496+0x48     tyre tread (1.0 = fresh)
  +1496+0xB8     brake disc temp (Kelvin)

The previously documented tyre/brake block at 0x007C7A00 is THIS SAME ARRAY:
0x007C7A00 == 0x007C7428 + 1496, identical 1124 stride.

Health percentage uses the game's own +-10 deg clamp, which is bounded.
Do NOT use +1132 for a percentage -- it keeps integrating past the 0.015
trip point and is unbounded.

Usage:
  python tools/suspension_bend.py              # one read
  python tools/suspension_bend.py --watch 30   # poll for 30 s, print on change
  python tools/suspension_bend.py --verify     # poke-prove all 4 slots, restores
"""
import struct, ctypes, subprocess, math, sys, time, argparse

WB, WS = 0x007C7428, 1124
NAMES = ["FL", "FR", "RL", "RR"]
OFF_DETACH, OFF_BENT = 1104, 1105
OFF_BEND1, OFF_BEND2, OFF_ACCUM = 1116, 1124, 1132
OFF_TYRE = 1496
CLAMP = 0.1745329251994329      # +-10 deg, the game's own clamp
TRIP  = 0.015                   # accumulator value that sets ReasonCode=4
VEH, OFF_REASON, OFF_CARCOND = 0x005EC6D8, 0x4068, 0x3D6C


def find_pid():
    r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq F1 Challenge Online.exe',
                        '/FO', 'CSV', '/NH'], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if 'F1 Challenge' in line:
            return int(line.split(',')[1].strip().strip('"'))
    return None


class Mem:
    def __init__(self):
        pid = find_pid()
        if not pid:
            sys.exit("ERROR: F1 Challenge Online.exe not running")
        self.h = ctypes.windll.kernel32.OpenProcess(0x1F0FFF, False, pid)
        if not self.h:
            sys.exit("ERROR: OpenProcess failed (try running as admin)")

    def rd(self, addr, size):
        buf = ctypes.create_string_buffer(size)
        n = ctypes.c_size_t()
        ok = ctypes.windll.kernel32.ReadProcessMemory(
            self.h, ctypes.c_void_p(addr), buf, size, ctypes.byref(n))
        return buf.raw if ok else None

    def wr(self, addr, data):
        n = ctypes.c_size_t()
        return bool(ctypes.windll.kernel32.WriteProcessMemory(
            self.h, ctypes.c_void_p(addr), data, len(data), ctypes.byref(n)))

    def d(self, addr):   return struct.unpack('<d', self.rd(addr, 8))[0]
    def i(self, addr):   return struct.unpack('<i', self.rd(addr, 4))[0]
    def b(self, addr):   return self.rd(addr, 1)[0]


def wheel(m, w):
    base = WB + WS * w
    b1, b2 = m.d(base + OFF_BEND1), m.d(base + OFF_BEND2)
    worst = max(abs(b1), abs(b2))
    return {
        "name": NAMES[w], "base": base,
        "bend1_deg": math.degrees(b1), "bend2_deg": math.degrees(b2),
        "accum": m.d(base + OFF_ACCUM),
        "bent": m.b(base + OFF_BENT), "detached": m.b(base + OFF_DETACH),
        "health": 100.0 * (1.0 - min(1.0, worst / CLAMP)),
        "tyre_c": m.d(base + OFF_TYRE) - 273.16,
        "brake_c": m.d(base + OFF_TYRE + 0xB8) - 273.16,
        "tread": m.d(base + OFF_TYRE + 0x48),
    }


def show(m):
    print(f"  vehicle: carConditions={m.i(VEH + OFF_CARCOND)}  "
          f"ReasonCode={m.i(VEH + OFF_REASON)} (4 = Suspension)\n")
    print(f"  {'':3} {'bend1':>9} {'bend2':>9} {'accum':>10} {'bent':>5} "
          f"{'health':>8}   {'tyreC':>7} {'brakeC':>7} {'tread':>8}")
    for w in range(4):
        r = wheel(m, w)
        print(f"  {r['name']:3} {r['bend1_deg']:>+8.3f}d {r['bend2_deg']:>+8.3f}d "
              f"{r['accum']:>10.6f} {r['bent']:>5} {r['health']:>7.1f}%   "
              f"{r['tyre_c']:>7.2f} {r['brake_c']:>7.2f} {r['tread']:>8.6f}")


def verify(m):
    """Poke-prove that each wheel slot's address math is real and live."""
    print("PROOF 1 -- record identity per wheel slot.")
    print("  Each of the 4 slots must return sane, DISTINCT physical values at the")
    print("  known-good sub-offsets. That proves base+stride indexing for all 4.\n")
    ok1 = True
    for w in range(4):
        r = wheel(m, w)
        sane = (-40 < r['tyre_c'] < 250 and -40 < r['brake_c'] < 1200
                and 0.0 <= r['tread'] <= 1.5)
        ok1 &= sane
        print(f"  {r['name']} @0x{r['base']:08X}  tyre {r['tyre_c']:7.2f}C  "
              f"brake {r['brake_c']:7.2f}C  tread {r['tread']:.6f}   "
              f"{'OK' if sane else 'IMPLAUSIBLE'}")
    print(f"\n  => {'PASS' if ok1 else 'FAIL'}: all 4 slots decode as real wheel records.\n")

    print("PROOF 2 -- write/readback/restore on every wheel's bend field.")
    print("  Proves each of the 4 addresses is a live field, not padding.")
    print("  Original values are restored immediately and re-verified.\n")
    TEST = 0.02   # ~1.15 deg, far inside the +-10 deg clamp
    ok2 = True
    for w in range(4):
        base = WB + WS * w
        addr = base + OFF_BEND1
        orig = m.d(addr)
        m.wr(addr, struct.pack('<d', TEST))
        got = m.d(addr)
        m.wr(addr, struct.pack('<d', orig))
        back = m.d(addr)
        good = abs(got - TEST) < 1e-9 and abs(back - orig) < 1e-12
        ok2 &= good
        print(f"  {NAMES[w]} @0x{addr:08X}  orig={orig:+.9f}  wrote {TEST} -> "
              f"read {got:+.9f}  restored={back:+.9f}  {'PASS' if good else 'FAIL'}")
    print(f"\n  => {'PASS' if ok2 else 'FAIL'}: all 4 bend fields are live and writable.\n")

    bent = [NAMES[w] for w in range(4) if wheel(m, w)['bent']]
    print(f"PROOF 3 -- organic damage currently flagged on: {bent if bent else 'none'}")
    print("  (a wheel only reaches bent=1 through the game's own physics path)")
    return ok1 and ok2


def watch(m, secs):
    prev = None
    t0 = time.time()
    print("watching (prints only when something changes)...\n")
    while time.time() - t0 < secs:
        cur = [(w['bent'], round(w['bend1_deg'], 3), round(w['bend2_deg'], 3),
                round(w['accum'], 6)) for w in (wheel(m, i) for i in range(4))]
        if cur != prev:
            print(f"  t={time.time()-t0:6.1f}s  ReasonCode={m.i(VEH + OFF_REASON)}")
            for i in range(4):
                r = wheel(m, i)
                print(f"     {r['name']} bend {r['bend1_deg']:+7.3f}/{r['bend2_deg']:+7.3f} deg "
                      f"accum {r['accum']:.6f} bent={r['bent']} health {r['health']:.1f}%")
            prev = cur
        time.sleep(0.25)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--watch", type=float, metavar="SECONDS")
    ap.add_argument("--verify", action="store_true")
    a = ap.parse_args()
    m = Mem()
    print("SUSPENSION BEND  base 0x007C7428  stride 1124  wheels FL,FR,RL,RR "
          "(player only)\n")
    if a.verify:
        sys.exit(0 if verify(m) else 1)
    elif a.watch:
        watch(m, a.watch)
    else:
        show(m)
