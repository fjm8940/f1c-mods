"""
ignition_trace.py -- find the field the IGNITION keybind flips on the player car.

READ-ONLY. Samples the player vehicle struct (slot 0) at N Hz for <secs> and
reports 4-byte words that changed only a FEW times (discrete state flags), with
the sample index of each flip -- so you can line the flip up with the moment you
pressed the key.

Physics jitter changes hundreds of words every sample; a state flag changes once
or twice. Same discrete-vs-continuous filter as garage_exit_trace.py.

Usage:
    python tools/ignition_trace.py trace 20 [--hz 10] [--max-changes 4]

Procedure: start it, wait ~3 s, press your Ignition keybind ONCE, wait.
"""

import ctypes
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

VEH_BASE = 0x005EC6D8
STRIDE   = 0x48C0
NAME_OFF = 0x4620
PROC     = "F1 Challenge Online.exe"

# never read-then-report noise: the two lethal offsets are never WRITTEN here
# (this tool never writes), but skip the render object hot words anyway.
SCAN_LO, SCAN_HI = 0x0000, 0x48C0


def find_pid():
    r = subprocess.run(
        ['tasklist', '/FI', f'IMAGENAME eq {PROC}', '/FO', 'CSV', '/NH'],
        capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if 'F1 Challenge' in line:
            return int(line.split(',')[1].strip().strip('"'))
    return None


def main():
    secs = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    hz = 10
    mx = 4
    if "--hz" in sys.argv:
        hz = int(sys.argv[sys.argv.index("--hz") + 1])
    if "--max-changes" in sys.argv:
        mx = int(sys.argv[sys.argv.index("--max-changes") + 1])

    pid = find_pid()
    if not pid:
        print("ERROR: game not running")
        return 1
    h = k32.OpenProcess(0x1F0FFF, False, pid)
    if not h:
        print("ERROR: OpenProcess failed (admin?)")
        return 1

    size = SCAN_HI - SCAN_LO
    buf = ctypes.create_string_buffer(size)
    n = ctypes.c_size_t()

    def grab():
        ok = k32.ReadProcessMemory(h, ctypes.c_void_p(VEH_BASE + SCAN_LO),
                                   buf, size, ctypes.byref(n))
        return buf.raw if ok else None

    nbuf = ctypes.create_string_buffer(64)
    k32.ReadProcessMemory(h, ctypes.c_void_p(VEH_BASE + NAME_OFF),
                          nbuf, 64, ctypes.byref(n))
    print(f"player slot 0: {nbuf.raw.split(chr(0).encode())[0].decode('latin-1')}")
    print(f"tracing {secs}s @ {hz} Hz -- press your Ignition keybind ONCE now-ish")

    samples = []
    t_end = time.time() + secs
    period = 1.0 / hz
    while time.time() < t_end:
        s = grab()
        if s:
            samples.append((time.time(), s))
        time.sleep(period)

    if len(samples) < 2:
        print("ERROR: no samples")
        return 1

    t0 = samples[0][0]
    nwords = size // 4
    changes = {}
    for i in range(1, len(samples)):
        a, b = samples[i - 1][1], samples[i][1]
        if a == b:
            continue
        for w in range(nwords):
            o = w * 4
            if a[o:o + 4] != b[o:o + 4]:
                changes.setdefault(w, []).append(i)

    hits = [(w, ix) for w, ix in changes.items() if len(ix) <= mx]
    hits.sort(key=lambda p: (len(p[1]), p[0]))
    print(f"\n{len(changes)} words moved; {len(hits)} discrete (<= {mx} changes)\n")
    print(f"{'offset':>8} {'#chg':>4}  {'at t (s)':<26} values")
    for w, ix in hits:
        off = SCAN_LO + w * 4
        ts = ", ".join(f"{samples[i][0]-t0:.2f}" for i in ix[:4])
        vals = []
        for i in ix[:3]:
            prev = struct.unpack('<I', samples[i - 1][1][w*4:w*4+4])[0]
            cur = struct.unpack('<I', samples[i][1][w*4:w*4+4])[0]
            vals.append(f"{prev:08x}->{cur:08x}")
        print(f"  0x{off:04X} {len(ix):>4}  {ts:<26} {' '.join(vals)}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] != "trace":
        print(__doc__)
        sys.exit(1)
    sys.exit(main())
