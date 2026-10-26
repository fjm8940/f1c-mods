"""
struct_block_probe.py - find WHICH REGION of the vehicle struct carries a
behaviour, by copying one car's block over another and bisecting.

WHY THIS EXISTS
---------------
2026-08-03: copying another car's whole vehicle struct over a live car is the
ONLY method found that relocates it. Every direct write to the position triad
fails, because veh+0x1C/0x20/0x24 is itself a mirror that is re-stamped within
30 ms - even on a car that is retired, inactive and hidden. The block copy works
because it carries whatever upstream state that refresh reads from.

Measured the same day, donor Diniz -> target Wurz, identity fields preserved:

    copy [0x0000,0x0E10)   nothing at all, the car kept accelerating
    copy [0x0E10,0x48C0)   RELOCATED: donor position AND donor speed
    copy the whole struct  RELOCATED, and the car drove away from the new spot

So the position lives in the UPPER region. Narrowing that is what this tool is
for.

THE RULE THIS TOOL EXISTS TO ENFORCE
------------------------------------
That session ended with the donor car driving around invisible and the game
crashing on the way out. Both have the same cause - the upper region carries
POINTERS to per-car heap children, so after a copy two cars share one graphics
object and one set of children, and the teardown frees them twice.

The mistake that made it unrecoverable was mine and it was simple: only the
LOWER 0xE10 had been backed up, and that was the region that turned out to do
nothing. The region actually overwritten had no backup at all.

Hence: this tool ALWAYS snapshots the target's full struct before it writes a
single byte, and RESTORES IT AUTOMATICALLY when the observation window ends.
--keep is available but has to be asked for, and it prints a warning telling
you to restore before leaving the session.

USAGE
    python tools/struct_block_probe.py list
    python tools/struct_block_probe.py snapshot <slot> [<slot> ...]
    python tools/struct_block_probe.py copy <donor> <target> <lo> <hi> [secs]
    python tools/struct_block_probe.py copy <donor> <target> --upper [secs]
    python tools/struct_block_probe.py bisect <donor> <target>     (prints a plan)
    python tools/struct_block_probe.py restore <slot>

Offsets are hex, e.g. 0xE10 0x48C0. Backups land in tools/_struct_backups/.

WRITES TO THE RUNNING GAME.
"""

import ctypes
import os
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

VEH_BASE = 0x005EC6D8
STRIDE = 0x48C0
OFF_ACTIVE = 0x32C8
OFF_NAME = 0x4620
OFF_X, OFF_Y, OFF_Z = 0x1C, 0x20, 0x24
OFF_SPEED = 0x4440

# Fields that make a car THAT car. Never copied - a donor's identity landing on
# the target renames it, moves it to the donor's slot, or points it at the
# donor's scene node.
IDENTITY = [
    (0x32C8, 4),    # active flag / slot liveness
    (0x32D0, 4),    # slot index - static, always == the slot's own number
    (0x4620, 32),   # driver name
    (0x489C, 4),    # driver index
    (0x48A0, 4),    # constructor index
    (0x48A4, 4),    # scene node index
    (0x48A8, 4),    # scene sub-index
]

BACKUP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "_struct_backups")
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
h = k32.OpenProcess(0x0010 | 0x0020 | 0x0008 | 0x0400, 0, pid)
if not h:
    sys.exit("OpenProcess failed - if the game runs elevated, use an "
             "Administrator shell")


def rd(addr, n):
    b = ctypes.create_string_buffer(n)
    got = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(addr), b, n, ctypes.byref(got))
    if not ok or got.value != n:
        sys.exit("read failed at 0x%X" % addr)
    return b.raw


def wr(addr, data):
    got = ctypes.c_size_t()
    buf = ctypes.create_string_buffer(data, len(data))
    ok = k32.WriteProcessMemory(h, ctypes.c_void_p(addr), buf, len(data),
                                ctypes.byref(got))
    return bool(ok) and got.value == len(data)


def veh(slot):
    return VEH_BASE + STRIDE * slot


def f32(addr):
    return struct.unpack("<f", rd(addr, 4))[0]


def name_of(slot):
    return rd(veh(slot) + OFF_NAME, 24).split(b"\0")[0].decode("latin-1")


def live(slot):
    return struct.unpack("<i", rd(veh(slot) + OFF_ACTIVE, 4))[0] != -1


def pos_of(slot):
    v = veh(slot)
    return (f32(v + OFF_X), f32(v + OFF_Y), f32(v + OFF_Z), f32(v + OFF_SPEED))


def describe(slot):
    x, y, z, s = pos_of(slot)
    return "slot %d %-26s (%8.1f,%6.1f,%9.1f)  spd=%5.1f" % (
        slot, name_of(slot), x, y, z, s)


def backup_path(slot):
    return os.path.join(BACKUP_DIR, "slot%02d.bin" % slot)


def snapshot(slot):
    os.makedirs(BACKUP_DIR, exist_ok=True)
    data = rd(veh(slot), STRIDE)
    with open(backup_path(slot), "wb") as fh:
        fh.write(data)
    return data


def restore(slot):
    p = backup_path(slot)
    if not os.path.exists(p):
        sys.exit("no backup for slot %d - nothing to restore" % slot)
    with open(p, "rb") as fh:
        data = fh.read()
    if len(data) != STRIDE:
        sys.exit("backup for slot %d is %d bytes, expected %d"
                 % (slot, len(data), STRIDE))
    ok = wr(veh(slot), data)
    print("restored slot %d from %s: %s" % (slot, p, ok))
    return ok


def copy_block(donor, target, lo, hi, secs, keep=False):
    n = hi - lo
    if not (0 <= lo < hi <= STRIDE):
        sys.exit("range must lie inside 0..0x%X" % STRIDE)

    print(describe(donor), " <- donor")
    print(describe(target), " <- target")

    # ALWAYS the whole struct, never just the range being written. The range is
    # what changes today; the whole struct is what has to go back.
    original = snapshot(target)
    print("backed up target's FULL struct -> %s" % backup_path(target))

    src = rd(veh(donor) + lo, n)

    # Punch the identity fields back out of the source before it is written, so
    # they are never even momentarily wrong.
    src = bytearray(src)
    for off, size in IDENTITY:
        if lo <= off and off + size <= hi:
            src[off - lo:off - lo + size] = original[off:off + size]

    if not wr(veh(target) + lo, bytes(src)):
        sys.exit("write failed")
    print("copied [0x%X .. 0x%X) = %d bytes, identity preserved" % (lo, hi, n))

    moved = False
    dx0, _, dz0, _ = pos_of(donor)
    for i in range(int(secs * 4)):
        time.sleep(0.25)
        tx, ty, tz, ts = pos_of(target)
        dx, dy, dz, ds = pos_of(donor)
        near = abs(tx - dx) < 25.0 and abs(tz - dz) < 25.0
        if near:
            moved = True
        print("  +%4dms  target (%8.1f,%6.1f,%9.1f) spd=%5.1f%s"
              % ((i + 1) * 250, tx, ty, tz, ts, "   <-- ON THE DONOR" if near else ""))

    print()
    print("VERDICT: block [0x%X,0x%X) %s the car"
          % (lo, hi, "RELOCATES" if moved else "does NOT relocate"))

    if keep:
        print("!! --keep given: the target is still carrying the donor's block.")
        print("!! Restore it BEFORE leaving the session, or the teardown will")
        print("!! crash on the shared heap children:")
        print("!!     python tools/struct_block_probe.py restore %d" % target)
    else:
        restore(target)
    return moved


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd = sys.argv[1]

    if cmd == "list":
        print("pid %d" % pid)
        for i in range(24):
            try:
                if live(i):
                    print(describe(i))
            except SystemExit:
                break
        return

    if cmd == "snapshot":
        for a in sys.argv[2:]:
            s = int(a)
            snapshot(s)
            print("snapshot slot %d (%s) -> %s" % (s, name_of(s), backup_path(s)))
        return

    if cmd == "restore":
        restore(int(sys.argv[2]))
        return

    if cmd == "bisect":
        print("Known: [0x0,0xE10) does nothing, [0xE10,0x48C0) relocates.")
        lo, hi = 0xE10, 0x48C0
        for step in range(5):
            mid = (lo + hi) // 2 & ~3
            print("  step %d: test [0x%X,0x%X); if it relocates, recurse left, "
                  "else right" % (step + 1, lo, mid))
            lo = mid
        return

    if cmd == "copy":
        donor, target = int(sys.argv[2]), int(sys.argv[3])
        keep = "--keep" in sys.argv
        args = [a for a in sys.argv[4:] if not a.startswith("--")]
        if "--upper" in sys.argv:
            lo, hi = 0xE10, STRIDE
            secs = float(args[0]) if args else 3.0
        else:
            lo, hi = int(args[0], 16), int(args[1], 16)
            secs = float(args[2]) if len(args) > 2 else 3.0
        copy_block(donor, target, lo, hi, secs, keep)
        return

    print("unknown command: %s" % cmd)


if __name__ == "__main__":
    main()
