"""
force_wing_retire.py — retire a healthy AI car by ARMING THE GAME'S OWN TRIGGER
instead of writing a retirement end-state.

Basis (see investigations/part_detach_commentary/README.md):
    PitStopBlockingCondition_0042aee0 damage term:
        *(veh+0xD18) && !*(byte*)(veh+0xD58)
    When true at the pit box, PitStopDecisionTireCompound_0042a3d0 writes
    RetiredFlag(0x4410)=1 + ActiveFlag(0x45F4)=0 itself.

So we only set:
    veh+0xD58 = 0     arm the rear-wing record   (ptr at 0xD18 is always non-null)
    veh+0x454C = 1    callToBox

...and let the game drive the car in and retire it on its own path. This is the
difference from mods/ai_pit_retire, which wrote the end-state directly and left
the car stuck at the pit box instead of its garage.

WRITES TO THE RUNNING GAME. Reversible: --revert restores flag=1, callToBox=0.

Usage:
    python tools/force_wing_retire.py list
    python tools/force_wing_retire.py check  <slot>
    python tools/force_wing_retire.py arm    <slot>          # the two writes
    python tools/force_wing_retire.py arm    <slot> --hold 90   # arm, then watch
    python tools/force_wing_retire.py revert <slot>
    python tools/force_wing_retire.py watch  <slot> [secs]
"""

import ctypes
import re
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

VEH_PTRS  = 0x007C54F0
VEH_COUNT = 0x005C329C
NAME_OFF  = 0x4620

RW_PTR   = 0xD18    # RearWingRecordPtr
RW_FLAG  = 0xD58    # RearWingIntactFlag  (1 = intact, 0 = armed)
FW_PTR   = 0xCD4
FW_FLAG  = 0xD14
CALLBOX  = 0x454C
PITTING  = 0x3450
FINISH   = 0x4064
RETIRED  = 0x4410
ACTIVE   = 0x45F4
REASON   = 0x4068
CARCOND  = 0x3D6C
POSX, POSY, POSZ = 0x1C, 0x20, 0x24

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
# PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | QUERY
h = k32.OpenProcess(0x0010 | 0x0020 | 0x0008 | 0x0400, 0, pid)
if not h:
    sys.exit("OpenProcess failed — the game runs ELEVATED, use an Administrator shell")


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


def f32(a):
    import struct
    r = raw(a, 4)
    return None if r is None else struct.unpack("<f", r)[0]


def wr(a, data):
    g = ctypes.c_size_t()
    buf = ctypes.create_string_buffer(data, len(data))
    ok = k32.WriteProcessMemory(h, ctypes.c_void_p(a), buf, len(data), ctypes.byref(g))
    return bool(ok) and g.value == len(data)


def cstr(a, n=40):
    r = raw(a, n)
    return "?" if r is None else r.split(b"\0")[0].decode("ascii", "replace")


def cars():
    n = i32(VEH_COUNT) or 0
    out = []
    for i in range(min(n, 104)):
        v = u32(VEH_PTRS + i * 4)
        if v:
            out.append((i, v, cstr(v + NAME_OFF)))
    return out


def veh_of(slot):
    for i, v, nm in cars():
        if i == slot:
            return v, nm
    sys.exit(f"slot {slot} not live")


def state(v):
    return dict(
        FWptr=u32(v + FW_PTR), FWflag=u8(v + FW_FLAG),
        RWptr=u32(v + RW_PTR), RWflag=u8(v + RW_FLAG),
        callBox=i32(v + CALLBOX), pitting=i32(v + PITTING),
        finish=i32(v + FINISH), retired=i32(v + RETIRED),
        active=i32(v + ACTIVE), reason=i32(v + REASON),
        carCond=i32(v + CARCOND),
        pos=(f32(v + POSX), f32(v + POSY), f32(v + POSZ)),
    )


def fmt(s):
    p = s["pos"]
    return (f"RWflag={s['RWflag']} FWflag={s['FWflag']} callBox={s['callBox']} "
            f"pitting={s['pitting']} finish={s['finish']} retired={s['retired']} "
            f"active={s['active']} reason={s['reason']} carCond={s['carCond']} "
            f"pos=({p[0]:.2f},{p[1]:.2f},{p[2]:.2f})")


def cmd_list():
    for i, v, nm in cars():
        s = state(v)
        tag = " RETIRED" if s["retired"] else ""
        print(f"  slot {i:2d}  {nm:<28s} RWflag={s['RWflag']} "
              f"callBox={s['callBox']} pitting={s['pitting']}{tag}")


def cmd_check(slot):
    v, nm = veh_of(slot)
    print(f"slot {slot} {nm} @ {v:#x}")
    print("  " + fmt(state(v)))


def cmd_arm(slot, hold=0.0):
    v, nm = veh_of(slot)
    s = state(v)
    print(f"slot {slot} {nm} @ {v:#x}")
    print("  before: " + fmt(s))
    if s["retired"]:
        sys.exit("  already retired — pick a healthy car")
    if not s["RWptr"]:
        sys.exit("  RWptr is NULL — the retire term cannot be satisfied")

    ok1 = wr(v + RW_FLAG, b"\x00")          # arm rear-wing record
    ok2 = wr(v + CALLBOX, (1).to_bytes(4, "little"))   # callToBox = 1
    print(f"  wrote RWflag=0 ({ok1}), callToBox=1 ({ok2})")
    time.sleep(0.6)
    after = state(v)
    print("  after : " + fmt(after))
    if after["RWflag"] != 0:
        print("  !! RWflag did NOT hold — recalculated by the game. "
              "Poking is not the right technique; hook the writer instead.")
    if hold:
        cmd_watch(slot, hold)


def cmd_arm_all(skip_player=True):
    """Arm every live, non-retired car. Player (slot 0) skipped by default."""
    done, skipped = [], []
    for i, v, nm in cars():
        if skip_player and i == 0:
            skipped.append((i, nm, "player"))
            continue
        s = state(v)
        if s["retired"]:
            skipped.append((i, nm, "already retired"))
            continue
        if not s["RWptr"]:
            skipped.append((i, nm, "RWptr NULL"))
            continue
        wr(v + RW_FLAG, b"\x00")
        wr(v + CALLBOX, (1).to_bytes(4, "little"))
        done.append((i, nm))
    time.sleep(0.6)
    print(f"armed {len(done)} cars:")
    for i, nm in done:
        v = dict((a, b) for a, b, _ in [(x, y, z) for x, y, z in cars()]).get(i)
        print(f"  slot {i:2d}  {nm}")
    if skipped:
        print(f"skipped {len(skipped)}:")
        for i, nm, why in skipped:
            print(f"  slot {i:2d}  {nm:<28s} ({why})")
    # verify the arm held
    bad = []
    for i, v, nm in cars():
        if any(i == d[0] for d in done) and u8(v + RW_FLAG) != 0:
            bad.append((i, nm))
    if bad:
        print("!! RWflag did NOT hold on:", ", ".join(f"{i} {nm}" for i, nm in bad))
    else:
        print("all armed flags held")


def cmd_revert_all():
    n = 0
    for i, v, nm in cars():
        if i == 0:
            continue
        if i32(v + RETIRED):
            continue
        wr(v + RW_FLAG, b"\x01")
        wr(v + CALLBOX, (0).to_bytes(4, "little"))
        n += 1
    print(f"reverted {n} non-retired cars")


def cmd_status():
    n_ret = 0
    for i, v, nm in cars():
        s = state(v)
        if s["retired"]:
            n_ret += 1
        p = s["pos"]
        where = "GARAGE" if 16.5 < p[0] < 17.5 else ("BOX" if 9.5 < p[0] < 11.0 else "")
        print(f"  slot {i:2d} {nm:<28s} RWflag={s['RWflag']} callBox={s['callBox']} "
              f"pitting={s['pitting']} retired={s['retired']} "
              f"pos=({p[0]:7.2f},{p[1]:6.2f},{p[2]:8.2f}) {where}")
    print(f"retired: {n_ret}/{len(cars())}")


def cmd_revert(slot):
    v, nm = veh_of(slot)
    wr(v + RW_FLAG, b"\x01")
    wr(v + CALLBOX, (0).to_bytes(4, "little"))
    time.sleep(0.3)
    print(f"slot {slot} {nm} reverted")
    print("  " + fmt(state(v)))


def cmd_watch(slot, secs=90.0):
    v, nm = veh_of(slot)
    prev = None
    t0 = time.time()
    print(f"watching slot {slot} {nm} for {secs:.0f}s")
    while time.time() - t0 < secs:
        s = state(v)
        key = {k: s[k] for k in
               ("RWflag", "FWflag", "callBox", "pitting", "finish",
                "retired", "active", "reason", "carCond")}
        if key != prev:
            print(f"[{time.time() - t0:6.2f}] " + fmt(s))
            prev = key
        time.sleep(0.1)
    print("final: " + fmt(state(v)))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == "list":
        cmd_list()
    elif cmd == "check":
        cmd_check(int(sys.argv[2]))
    elif cmd == "arm":
        hold = 0.0
        if "--hold" in sys.argv:
            hold = float(sys.argv[sys.argv.index("--hold") + 1])
        cmd_arm(int(sys.argv[2]), hold)
    elif cmd == "arm-all":
        cmd_arm_all()
    elif cmd == "revert-all":
        cmd_revert_all()
    elif cmd == "status":
        cmd_status()
    elif cmd == "revert":
        cmd_revert(int(sys.argv[2]))
    elif cmd == "watch":
        cmd_watch(int(sys.argv[2]),
                  float(sys.argv[3]) if len(sys.argv) > 3 else 90.0)
    else:
        sys.exit(__doc__)
