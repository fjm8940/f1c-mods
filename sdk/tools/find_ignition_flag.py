"""
find_ignition_flag.py — find the ignition state flag by snapshot/diff/poke.

Strategy:
  1. Take full-process baseline scan (engine ON)
  2. Inject comma key via game's key queue
  3. Wait for EngineRPM to hit 0
  4. Take second scan (engine OFF)
  5. Diff: find bytes where A != B (any granularity)
  6. Filter: keep only addresses that look like state flags (0/1, small ints)
  7. For each candidate, poke the ON value back and check if RPM restores
  8. The candidate that revives RPM is the ignition flag

Usage:
  python tools/find_ignition_flag.py scan      # take ON snapshot, toggle, take OFF snapshot, diff
  python tools/find_ignition_flag.py poke      # verify candidates from the last scan
  python tools/find_ignition_flag.py auto      # full automatic: scan + poke-verify
"""

import ctypes
import ctypes.wintypes as wt
import os
import pickle
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32
PROC = "F1 Challenge Online.exe"
VEH_BASE = 0x005EC6D8
STRIDE = 0x48C0
ENGINE_RPM = 0x39A0
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, "..", "ignition_flag_hunt")
os.makedirs(OUT_DIR, exist_ok=True)
SNAP_ON = os.path.join(OUT_DIR, "snap_on.pkl")
SNAP_OFF = os.path.join(OUT_DIR, "snap_off.pkl")
CANDIDATES = os.path.join(OUT_DIR, "candidates.pkl")

MEM_COMMIT = 0x1000
PAGE_RW = (0x04, 0x40)  # PAGE_READWRITE, PAGE_EXECUTE_READWRITE

KEY_QUEUE_COUNT = 0x007CB218
KEY_QUEUE_LIST = 0x007CB21C
DIK_COMMA = 0x33


class MBI(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wt.DWORD),
        ("RegionSize", ctypes.c_size_t),
        ("State", wt.DWORD),
        ("Protect", wt.DWORD),
        ("Type", wt.DWORD),
    ]


def find_pid():
    r = subprocess.run(
        ["tasklist", "/FI", f"IMAGENAME eq {PROC}", "/FO", "CSV", "/NH"],
        capture_output=True, text=True,
    )
    for line in r.stdout.splitlines():
        if "F1 Challenge" in line:
            return int(line.split(",")[1].strip().strip('"'))
    return None


def get_handle():
    pid = find_pid()
    if not pid:
        sys.exit("Game not running")
    h = k32.OpenProcess(0x1F0FFF, False, pid)
    if not h:
        sys.exit("OpenProcess failed (admin?)")
    return h


def readable_regions(h):
    addr = 0
    mbi = MBI()
    regions = []
    while addr < 0x7FFF0000:
        if not k32.VirtualQueryEx(
            h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)
        ):
            break
        if (
            mbi.State == MEM_COMMIT
            and mbi.Protect in PAGE_RW
            and mbi.RegionSize < 64 * 1024 * 1024
        ):
            regions.append((mbi.BaseAddress or 0, mbi.RegionSize))
        addr = (mbi.BaseAddress or 0) + mbi.RegionSize
        if mbi.RegionSize == 0:
            break
    return regions


def read_regions(h, regions):
    n = ctypes.c_size_t()
    data = {}
    total = 0
    for base, size in regions:
        buf = ctypes.create_string_buffer(size)
        if k32.ReadProcessMemory(
            h, ctypes.c_void_p(base), buf, size, ctypes.byref(n)
        ):
            data[base] = buf.raw[: n.value]
            total += n.value
    return data


def read_rpm(h):
    buf = ctypes.create_string_buffer(4)
    n = ctypes.c_size_t()
    k32.ReadProcessMemory(
        h, ctypes.c_void_p(VEH_BASE + ENGINE_RPM), buf, 4, ctypes.byref(n)
    )
    return struct.unpack("<f", buf.raw)[0]


def inject_comma(h):
    """Write DIK_COMMA into the game's pending-key queue."""
    n = ctypes.c_size_t()
    k32.WriteProcessMemory(
        h, ctypes.c_void_p(KEY_QUEUE_LIST), struct.pack("<h", DIK_COMMA), 2, ctypes.byref(n)
    )
    k32.WriteProcessMemory(
        h, ctypes.c_void_p(KEY_QUEUE_COUNT), struct.pack("<i", 1), 4, ctypes.byref(n)
    )


def cmd_scan():
    h = get_handle()
    regions = readable_regions(h)
    print(f"Found {len(regions)} writable regions")

    # ---- baseline: engine ON ----
    print("Taking baseline snapshot (engine ON)...")
    rpm0 = read_rpm(h)
    print(f"  EngineRPM before: {rpm0:.1f}")
    snap_on = read_regions(h, regions)
    total_mb = sum(len(v) for v in snap_on.values()) / 1e6
    print(f"  Snapshot: {len(snap_on)} regions, {total_mb:.1f} MB")
    with open(SNAP_ON, "wb") as f:
        pickle.dump(snap_on, f, protocol=4)

    # ---- toggle ignition ----
    print("Toggling ignition OFF...")
    for i in range(30):  # pulse for ~1.5s
        inject_comma(h)
        time.sleep(0.05)

    # ---- wait for RPM = 0 ----
    print("Waiting for engine RPM to hit 0...")
    timeout = time.time() + 10.0
    while time.time() < timeout:
        rpm = read_rpm(h)
        if rpm <= 10.0:
            print(f"  EngineRPM: {rpm:.1f} — engine off")
            time.sleep(1.0)  # let physics settle
            break
        time.sleep(0.2)
    else:
        print(f"  WARNING: RPM still at {read_rpm(h):.1f} after 10s")

    # ---- snapshot: engine OFF ----
    print("Taking snapshot (engine OFF)...")
    snap_off = read_regions(h, regions)
    with open(SNAP_OFF, "wb") as f:
        pickle.dump(snap_off, f, protocol=4)

    # ---- diff ----
    print("Diffing snapshots...")
    candidates = []
    common = set(snap_on) & set(snap_off)
    for base in sorted(common):
        a, b = snap_on[base], snap_off[base]
        ln = min(len(a), len(b))
        for i in range(ln):
            if a[i] != b[i]:
                addr = base + i
                candidates.append((addr, a[i], b[i]))

    print(f"  Raw byte changes: {len(candidates)}")

    # ---- filter: keep only small-int changes (not physics floats) ----
    # A state flag changes by small amounts (0->1, 1->0, small int shifts)
    # Physics floats change continuously — exclude large deltas
    filtered = []
    for addr, av, bv in candidates:
        delta = abs(av - bv)
        if delta <= 5:  # small change = likely state flag
            filtered.append((addr, av, bv))

    print(f"  After small-delta filter (|delta| <= 5): {len(filtered)}")

    # ---- collapse byte runs into word groups ----
    # Adjacent changed bytes might form a single int/float
    runs = []
    for addr, av, bv in filtered:
        if runs and addr == runs[-1][-1][0] + 1:
            runs[-1].append((addr, av, bv))
        else:
            runs.append([(addr, av, bv)])

    print(f"  After run collapse: {len(runs)} groups")

    # Save candidates as (start_addr, size, (on_bytes, off_bytes))
    final = []
    for r in runs:
        start = r[0][0]
        on_bytes = bytes(x[1] for x in r)
        off_bytes = bytes(x[2] for x in r)
        final.append((start, len(r), on_bytes, off_bytes))

    with open(CANDIDATES, "wb") as f:
        pickle.dump(final, f, protocol=4)

    # Show top candidates (grouped by region)
    print(f"\nTop candidates ({len(final)} total):")
    # Show the most interesting: small byte groups in the 0xD0 and 0x5E/0x5F ranges first
    priority = []
    for start, sz, on_b, off_b in final:
        prio = 0
        if 0x00D00000 <= start <= 0x00D40000:
            prio = 2
        elif 0x005EC000 <= start <= 0x005F3000:
            prio = 1
        priority.append((prio, start, sz, on_b, off_b))
    priority.sort(key=lambda x: (-x[0], x[1]))

    for prio, start, sz, on_b, off_b in priority[:40]:
        on_hex = on_b.hex()
        off_hex = off_b.hex()
        extra = ""
        if sz == 4:
            on_i = struct.unpack("<i", on_b)[0]
            off_i = struct.unpack("<i", off_b)[0]
            on_f = struct.unpack("<f", on_b)[0]
            extra = f"  i:{on_i}->{off_i}  f:{on_f:.4g}"
        elif sz == 1:
            extra = f"  byte:{on_b[0]}->{off_b[0]}"
        print(f"  [{prio}] 0x{start:08X} sz={sz}  {on_hex} -> {off_hex}{extra}")

    k32.CloseHandle(h)
    print(f"\nSaved candidates to {CANDIDATES}")
    print("Next: python tools/find_ignition_flag.py poke")


def cmd_poke():
    """Poke each candidate back to ON value, check if RPM restores."""
    if not os.path.exists(CANDIDATES):
        sys.exit("No candidates file. Run 'scan' first.")
    with open(CANDIDATES, "rb") as f:
        candidates = pickle.load(f)

    h = get_handle()
    n = ctypes.c_size_t()

    print(f"Testing {len(candidates)} candidates...")
    print(f"Current EngineRPM: {read_rpm(h):.1f}")
    print()

    # Sort: test most promising first (byte-sized, 0/1 flips)
    def score(c):
        start, sz, on_b, off_b = c
        s = 0
        if sz == 1:
            s += 10  # byte flags are most likely
        if {on_b[0], off_b[0]} == {0, 1}:
            s += 20  # boolean flip
        if 0x00D03000 <= start <= 0x00D04000:
            s += 5  # in the known ignition cluster
        return -s

    candidates.sort(key=score)

    tested = 0
    for start, sz, on_b, off_b in candidates:
        tested += 1
        if tested > 200:
            print(f"\n  ... stopped after 200 pokes")
            break

        # Write the ON value back
        buf = ctypes.create_string_buffer(on_b)
        k32.WriteProcessMemory(
            h, ctypes.c_void_p(start), buf, sz, ctypes.byref(n)
        )
        time.sleep(0.15)

        rpm = read_rpm(h)
        if rpm > 100.0:
            print(f"  *** HIT *** 0x{start:08X} sz={sz} — RPM={rpm:.1f}!")
            print(f"      ON bytes: {on_b.hex()}  OFF bytes: {off_b.hex()}")
            with open(os.path.join(OUT_DIR, "FOUND.pkl"), "wb") as f:
                pickle.dump((start, sz, on_b, off_b), f)
            break

        # Restore OFF value (undo the poke)
        k32.WriteProcessMemory(
            h, ctypes.c_void_p(start),
            ctypes.create_string_buffer(off_b), sz, ctypes.byref(n)
        )

        if tested % 50 == 0:
            print(f"  ... tested {tested}/{min(len(candidates), 200)}, RPM={rpm:.1f}")

    k32.CloseHandle(h)

    if os.path.exists(os.path.join(OUT_DIR, "FOUND.pkl")):
        with open(os.path.join(OUT_DIR, "FOUND.pkl"), "rb") as f:
            addr, sz, on_b, off_b = pickle.load(f)
        print(f"\nFOUND: ignition flag at 0x{addr:08X}")
        print(f"  ON -> OFF: {on_b.hex()} -> {off_b.hex()}")
        print(f"  To kill engine: write {off_b.hex()} to 0x{addr:08X}")
    else:
        print("\nNo candidate revived RPM within 200 tries.")
        print("The flag may be a float, multi-byte, or in a region not covered.")


def cmd_auto():
    cmd_scan()
    cmd_poke()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    cmd = sys.argv[1]
    if cmd == "scan":
        cmd_scan()
    elif cmd == "poke":
        cmd_poke()
    elif cmd == "auto":
        cmd_auto()
    else:
        print(__doc__)
        sys.exit(1)
