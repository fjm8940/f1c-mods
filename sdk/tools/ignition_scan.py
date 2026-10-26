"""
ignition_scan.py -- find the IGNITION flag anywhere in the process.

The vehicle struct showed no discrete flip on an ignition press, so the state
lives elsewhere (engine/player-control child object). This snapshots every
writable region and uses a THREE-STATE filter:

    A = engine ON, B = engine OFF, C = engine ON again
    keep bytes where A == C and A != B

Any address that toggles with the key and returns is a candidate. Physics noise
almost never satisfies A == C exactly.

Usage:
    python tools/ignition_scan.py snap A        # engine running
    ... press ignition, engine off ...
    python tools/ignition_scan.py snap B
    ... press ignition, engine on ...
    python tools/ignition_scan.py snap C
    python tools/ignition_scan.py diff

READ-ONLY.
"""

import ctypes
import ctypes.wintypes as wt
import os
import pickle
import struct
import subprocess
import sys

k32 = ctypes.windll.kernel32
PROC = "F1 Challenge Online.exe"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "ignition_scan")

MEM_COMMIT = 0x1000
PAGE_RW = (0x04, 0x40)          # PAGE_READWRITE, PAGE_EXECUTE_READWRITE


class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p),
                ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wt.DWORD),
                ("RegionSize", ctypes.c_size_t),
                ("State", wt.DWORD),
                ("Protect", wt.DWORD),
                ("Type", wt.DWORD)]


def find_pid():
    r = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {PROC}', '/FO', 'CSV', '/NH'],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if 'F1 Challenge' in line:
            return int(line.split(',')[1].strip().strip('"'))
    return None


def regions(h):
    addr = 0
    mbi = MBI()
    out = []
    while addr < 0x7FFF0000:
        if not k32.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi),
                                  ctypes.sizeof(mbi)):
            break
        if mbi.State == MEM_COMMIT and mbi.Protect in PAGE_RW and mbi.RegionSize < 64 * 1024 * 1024:
            out.append((mbi.BaseAddress or 0, mbi.RegionSize))
        addr = (mbi.BaseAddress or 0) + mbi.RegionSize
        if mbi.RegionSize == 0:
            break
    return out


def snap(tag):
    pid = find_pid()
    h = k32.OpenProcess(0x1F0FFF, False, pid)
    n = ctypes.c_size_t()
    data = {}
    total = 0
    for base, size in regions(h):
        buf = ctypes.create_string_buffer(size)
        if k32.ReadProcessMemory(h, ctypes.c_void_p(base), buf, size, ctypes.byref(n)):
            data[base] = buf.raw[:n.value]
            total += n.value
    with open(f"{OUT}_{tag}.pkl", "wb") as f:
        pickle.dump(data, f, protocol=4)
    print(f"snapshot {tag}: {len(data)} regions, {total/1e6:.1f} MB")


def diff():
    snaps = {}
    for t in "ABC":
        with open(f"{OUT}_{t}.pkl", "rb") as f:
            snaps[t] = pickle.load(f)
    A, B, C = snaps["A"], snaps["B"], snaps["C"]
    common = set(A) & set(B) & set(C)
    hits = []
    for base in sorted(common):
        a, b, c = A[base], B[base], C[base]
        ln = min(len(a), len(b), len(c))
        i = 0
        while i < ln:
            if a[i] != b[i] and a[i] == c[i]:
                hits.append((base + i, a[i], b[i]))
                if len(hits) > 20000:
                    print("too many hits, aborting")
                    return
            i += 1
    print(f"{len(hits)} candidate bytes (A==C != B)\n")
    # collapse runs (a float/int shows as up to 4 adjacent bytes)
    runs = []
    for addr, av, bv in hits:
        if runs and addr == runs[-1][-1][0] + 1:
            runs[-1].append((addr, av, bv))
        else:
            runs.append([(addr, av, bv)])
    print(f"{len(runs)} runs\n")
    for r in runs[:120]:
        a0 = r[0][0]
        on = bytes(x[1] for x in r)
        off = bytes(x[2] for x in r)
        extra = ""
        if len(r) == 4:
            extra = (f"  on={struct.unpack('<i', on)[0]}/{struct.unpack('<f', on)[0]:.4g}"
                     f"  off={struct.unpack('<i', off)[0]}/{struct.unpack('<f', off)[0]:.4g}")
        print(f"  0x{a0:08X} len{len(r)}  on={on.hex()} off={off.hex()}{extra}")


def diff_ab(ta="A", tb="B", save=None):
    """Two-state: ignition-off is irreversible, so no C. Keep only boolean-shaped
    flips (1->0 or 0->1) whose 3 neighbouring bytes on both sides are unchanged --
    a lone flag byte, not a float that happened to land on 0/1."""
    with open(f"{OUT}_A.pkl", "rb") as f:
        A = pickle.load(f)
    with open(f"{OUT}_B.pkl", "rb") as f:
        B = pickle.load(f)
    hits = []
    for base in sorted(set(A) & set(B)):
        a, b = A[base], B[base]
        ln = min(len(a), len(b))
        for i in range(3, ln - 3):
            if a[i] == b[i]:
                continue
            if {a[i], b[i]} != {0, 1}:
                continue
            if a[i-3:i] != b[i-3:i] or a[i+1:i+4] != b[i+1:i+4]:
                continue
            hits.append((base + i, a[i], b[i]))
    print(f"{len(hits)} isolated boolean flips ({ta}->{tb})")
    if save:
        with open(save, "wb") as f:
            pickle.dump(hits, f)
        print(f"saved -> {save}")
    for addr, av, bv in hits[:60]:
        print(f"  0x{addr:08X}  on={av} off={bv}")


def intersect(f1, f2):
    with open(f1, "rb") as f:
        h1 = pickle.load(f)
    with open(f2, "rb") as f:
        h2 = pickle.load(f)
    m1 = {a: (x, y) for a, x, y in h1}
    m2 = {a: (x, y) for a, x, y in h2}
    both = [a for a in m1 if a in m2 and m1[a] == m2[a]]
    print(f"pair1={len(m1)}  pair2={len(m2)}  SAME FLIP BOTH TIMES={len(both)}\n")
    for a in sorted(both):
        on, off = m1[a]
        print(f"  0x{a:08X}  on={on} off={off}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    if sys.argv[1] == "snap":
        snap(sys.argv[2])
    elif sys.argv[1] == "diff":
        diff()
    elif sys.argv[1] == "diffab":
        a = sys.argv[2] if len(sys.argv) > 2 else "A"
        b = sys.argv[3] if len(sys.argv) > 3 else "B"
        sv = sys.argv[4] if len(sys.argv) > 4 else None
        diff_ab(a, b, sv)
    elif sys.argv[1] == "intersect":
        intersect(sys.argv[2], sys.argv[3])
    else:
        print(__doc__)
