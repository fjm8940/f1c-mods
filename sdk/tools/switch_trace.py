#!/usr/bin/env python3
"""switch_trace.py - 20 Hz capture of the ORGANIC-STOP SWITCH candidates during
a natural pit stop: 0x344C (entry latch), 0x32C8 (AI mode 2/3 gate), the ring
block state, the machine fields, callToBox, the schedule. Log:
docs/pitstop_switch_trace.txt. Run until the stop completes (pit 4 -> 0 out)."""
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
RING = 0x8AAE64
LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs",
                   "pitstop_switch_trace.txt")
SECS = 420

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
out = subprocess.check_output(["tasklist", "/FI", "IMAGENAME eq " + EXE],
                              text=True, errors="replace")
pid = None
for line in out.splitlines():
    if EXE.lower() in line.lower():
        for tok in line.split():
            if tok.isdigit():
                pid = int(tok)
                break
h = k32.OpenProcess(0x1F0FFF, False, pid)


def rd(a, n):
    buf = (ctypes.c_char * n)()
    got = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(wt.HANDLE(h), ctypes.c_void_p(a), buf, n,
                               ctypes.byref(got))
    return buf.raw if ok and got.value == n else None


def i32(a):
    r = rd(a, 4)
    return None if r is None else struct.unpack("<i", r)[0]


def f32(a):
    r = rd(a, 4)
    return None if r is None else struct.unpack("<f", r)[0]


def dbl(a):
    r = rd(a, 8)
    return None if r is None else struct.unpack("<d", r)[0]


def find(name):
    for i in range(SLOTS):
        v = VEH + STRIDE * i
        if i32(v + 0x32C8) == -1:
            continue
        raw = rd(v + 0x4620, 32)
        if raw and raw.split(b"\0")[0].decode("latin-1", "replace").startswith(name):
            return i
    return -1


f = io.open(LOG, "w", encoding="utf-8")
f.write("==== organic-switch trace %s ====\n" % time.strftime("%H:%M:%S"))
t0 = time.time()
prev = {}
prev_ring = {}
done = False
while time.time() - t0 < SECS:
    j = find("Bernd Maylander")
    if j < 0:
        break
    v = VEH + STRIDE * j
    sess = dbl(0x8AFEA0)
    vals = {
        "x32C8": i32(v + 0x32C8), "x344C": i32(v + 0x344C),
        "pit": i32(v + 0x3450), "svc": rd(v + 0x345C, 1)[0],
        "x3454": f32(v + 0x3454), "x3458": f32(v + 0x3458),
        "stopS": f32(v + 0x346C), "stopE": f32(v + 0x3470),
        "dS": i32(v + 0x4510), "cb": i32(v + 0x454C),
        "lane": i32(v + 0x4550), "maxSp": f32(v + 0x4538),
        "f44": i32(v + 0x4544), "f48": i32(v + 0x4548),
        "stops": i32(v + 0x4588), "stopLap": i32(v + 0x4590),
        "visits": i32(v + 0x406C), "carC": i32(v + 0x3D6C),
        "spd": f32(v + 0x4440), "z": f32(v + 0x34E8),
    }
    ring = i32(RING)
    if ring:
        vals["ring8"] = rd(ring + 8, 1)[0]
        vals["ring97"] = i32(ring + 0x184)
    changed = {k: vals[k] for k in vals if k not in prev or prev[k] != vals[k]}
    if changed:
        line = "T+%6.2f s%7.1f %s" % (
            time.time() - t0, sess,
            " ".join("%s=%s" % (k, round(vals[k], 2) if isinstance(vals[k], float)
                                else vals[k]) for k, v in changed.items()))
        print(line)
        f.write(line + "\n")
        f.flush()
    prev = vals
    if vals.get("pit") == 0 and prev.get("pit") == 4 and vals.get("spd", 0) > 30:
        done = True
    if done:
        break
    time.sleep(0.05)
f.write("==== end ====\n")
f.close()
