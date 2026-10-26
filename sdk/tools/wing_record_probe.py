"""
wing_record_probe.py — read the two candidate wing records that
PitStopBlockingCondition_0042aee0 tests, across every live car.

Static basis (DERIVED, functions/PitStopDecisionTireCompound_0042a3d0.c +
functions/PitStopBlockingCondition_0042aee0.c):

    record A: veh+0xCD4 ptr, veh+0xD14 byte    (tested by StateTransition only)
    record B: veh+0xD18 ptr, veh+0xD58 byte    (tested by BOTH StateTransition
                                                and PitStopBlockingCondition,
                                                i.e. the one that can retire)

    retire term:  *(veh+0xD18) != 0  &&  !*(byte*)(veh+0xD58)

Hypothesis under test: A = front wing, B = rear wing. If true, a car that lost
its FRONT wing shows A "armed" and B clear; a car that lost its REAR wing shows
B armed and therefore retires at the box.

Read-only.
"""

import ctypes
import re
import struct
import subprocess
import sys

k32 = ctypes.windll.kernel32

VEH_PTRS   = 0x007C54F0
VEH_COUNT  = 0x005C329C
NAME_OFF   = 0x4620

REC = [("A", 0xCD4, 0xD14), ("B", 0xD18, 0xD58)]
EXTRA = [(0x3D6C, "carCond"), (0x4068, "reason"), (0x4064, "finish"),
         (0x4410, "retired"), (0x45F4, "active"), (0x3450, "pitting"),
         (0x454C, "callBox")]

# the stride-0x44 continuation seen in the runtime_delta paths
MORE = [0xD5C, 0xDA0, 0xDE4, 0xE28]


def pid():
    out = subprocess.check_output(
        ["tasklist", "/FI", "IMAGENAME eq F1 Challenge Online.exe"],
        text=True, errors="replace")
    for line in out.splitlines():
        if "F1 Challenge" in line:
            m = re.search(r"\b(\d+)\b", line)
            if m:
                return int(m.group(1))
    return None


p = pid()
if not p:
    sys.exit("game not running")
h = k32.OpenProcess(0x0010, 0, p)


def raw(a, n):
    b = ctypes.create_string_buffer(n)
    g = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(g))
    return b.raw if ok and g.value == n else None


def u32(a):
    r = raw(a, 4)
    return None if r is None else int.from_bytes(r, "little")


def i32(a):
    r = raw(a, 4)
    return None if r is None else int.from_bytes(r, "little", signed=True)


def u8(a):
    r = raw(a, 1)
    return None if r is None else r[0]


def cstr(a, n=40):
    r = raw(a, n)
    return "?" if r is None else r.split(b"\0")[0].decode("ascii", "replace")


n = i32(VEH_COUNT) or 0
hdr = (f"{'slot':>4s} {'driver':<26s} "
       + " ".join(f"{lbl}ptr    {lbl}flg" for lbl, _, _ in REC) + "  "
       + " ".join(f"{nm:>7s}" for _, nm in EXTRA) + "   RETIRE-TERM")
print(hdr)
print("-" * len(hdr))

for i in range(min(n, 104)):
    veh = u32(VEH_PTRS + i * 4)
    if not veh:
        continue
    name = cstr(veh + NAME_OFF)
    cells = []
    term = False
    for lbl, po, fo in REC:
        pv = u32(veh + po)
        fv = u8(veh + fo)
        cells.append(f"{pv:08x} {fv!s:>5s}")
        if lbl == "B" and pv and not fv:
            term = True
    ex = " ".join(f"{i32(veh + o)!s:>7s}" for o, _ in EXTRA)
    print(f"{i:4d} {name:<26s} " + " ".join(cells) + "  " + ex
          + ("   *** RETIRE ***" if term else ""))

print("\nstride-0x44 continuation (ptr at each), non-zero only:")
for i in range(min(n, 104)):
    veh = u32(VEH_PTRS + i * 4)
    if not veh:
        continue
    vals = [(hex(o), u32(veh + o)) for o in MORE]
    nz = [f"{o}={v:08x}" for o, v in vals if v]
    if nz:
        print(f"  slot {i:2d} {cstr(veh + NAME_OFF):<26s} " + " ".join(nz))
