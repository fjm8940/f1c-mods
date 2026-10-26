"""
garage_exit_trace.py — trace the game's OWN garage -> track transition.

Purpose: a deretired car sits frozen in its garage even though every state flag
matches a racing car. So we watch practice/warmup AI cars make the same
transition legitimately and find WHAT FLIPS BEFORE THE CAR MOVES.

Two filters make the signal readable:

  1. DISCRETE vs CONTINUOUS. A parked car is still physically simulated, so
     hundreds of words jitter every sample. A state flag changes once or twice.
     We count changes per word and keep only the low-count ones.
  2. CROSS-CAR INTERSECTION. Traced over the whole field, a field that flips
     discretely before EVERY car's exit is a candidate cause; one that flips for
     a single car is noise or that car's own business.

READ-ONLY. Never writes to the game.

Usage:
    python tools/garage_exit_trace.py find
    python tools/garage_exit_trace.py trace <secs> [--hz N] [--max-changes N]
        traces every currently-garaged car at once
"""

import ctypes
import json
import os
import re
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

VEH_PTRS  = 0x007C54F0
VEH_COUNT = 0x005C329C
NAME_OFF  = 0x4620
STRIDE    = 0x48C0

SPEED = 0x4440
POSX, POSY, POSZ = 0x1C, 0x20, 0x24

GARAGE_X_LO, GARAGE_X_HI = 14.0, 20.0
MOVE_THRESHOLD = 1.0        # metres from start = "left the garage"

PROC = "F1 Challenge Online.exe"
HERE = os.path.dirname(os.path.abspath(__file__))


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
h = k32.OpenProcess(0x0010 | 0x0400, 0, pid)   # READ + QUERY only
if not h:
    sys.exit("OpenProcess failed - the game runs ELEVATED, use an Administrator shell")


def raw(a, n):
    b = ctypes.create_string_buffer(n)
    g = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(g))
    return b.raw if ok and g.value == n else None


def i32(a):
    r = raw(a, 4)
    return None if r is None else int.from_bytes(r, "little", signed=True)


def f32(a):
    r = raw(a, 4)
    return None if r is None else struct.unpack("<f", r)[0]


def veh(slot):
    return i32(VEH_PTRS + slot * 4)


def name(v):
    r = raw(v + NAME_OFF, 40)
    return "?" if r is None else r.split(b"\0")[0].decode("ascii", "replace")


def labels():
    out = {}
    p = os.path.join(HERE, "..", "name_index.json")
    try:
        with open(p, encoding="utf-8") as f:
            d = json.load(f)
    except Exception:
        return out
    entries = d if isinstance(d, list) else d.get("vehicle_fields", d.get("entries", []))
    if isinstance(entries, dict):
        entries = list(entries.values())
    for e in entries:
        if not isinstance(e, dict):
            continue
        off, nm = e.get("offset"), e.get("name")
        if off is None or not nm:
            continue
        if isinstance(off, str):
            try:
                off = int(off, 16) if off.lower().startswith("0x") else int(off)
            except ValueError:
                continue
        out[off] = (nm, e.get("tier", "?"))
    return out


def garaged_slots():
    n = i32(VEH_COUNT) or 0
    out = []
    for s in range(n):
        v = veh(s)
        if not v:
            continue
        sp, x = f32(v + SPEED), f32(v + POSX)
        if sp is None or x is None:
            continue
        if GARAGE_X_LO <= x <= GARAGE_X_HI and abs(sp) < 0.5:
            out.append(s)
    return out


def cmd_find():
    n = i32(VEH_COUNT) or 0
    print(f"{'slot':>4} {'driver':<30} {'speed':>8} {'X':>8} {'Z':>9}  where")
    for s in range(n):
        v = veh(s)
        if not v:
            continue
        sp, x, z = f32(v + SPEED), f32(v + POSX), f32(v + POSZ)
        where = "GARAGE" if (GARAGE_X_LO <= x <= GARAGE_X_HI and abs(sp) < 0.5) else ""
        print(f"{s:>4} {name(v):<30} {sp:8.2f} {x:8.2f} {z:9.2f}  {where}")


def fmt(ob, nb):
    oi = int.from_bytes(ob, "little", signed=True)
    ni = int.from_bytes(nb, "little", signed=True)
    of_, nf = struct.unpack("<f", ob)[0], struct.unpack("<f", nb)[0]
    if abs(oi) > 1000 and abs(of_) < 1e6 and abs(nf) < 1e6:
        return f"{of_:.3f}", f"{nf:.3f}"
    return str(oi), str(ni)


def cmd_trace(secs, hz, max_changes):
    slots = garaged_slots()
    if not slots:
        sys.exit("no garaged cars found")
    cars = {}
    for s in slots:
        v = veh(s)
        b = raw(v, STRIDE)
        if b is None:
            continue
        cars[s] = dict(
            veh=v, name=name(v), base=b, prev=b,
            x0=struct.unpack("<f", b[POSX:POSX + 4])[0],
            z0=struct.unpack("<f", b[POSZ:POSZ + 4])[0],
            counts={}, first={}, t_move=None,
        )
    print(f"tracing {len(cars)} garaged cars for {secs}s at {hz}Hz (read-only)")
    for s, c in cars.items():
        print(f"  slot {s:>3} {c['name']}")
    print()

    period = 1.0 / hz
    t0 = time.time()
    nxt = t0
    while time.time() - t0 < secs:
        now = time.time()
        if now < nxt:
            time.sleep(max(0.0, nxt - now))
        nxt += period
        t = time.time() - t0
        for s, c in cars.items():
            cur = raw(c["veh"], STRIDE)
            if cur is None:
                continue
            prev = c["prev"]
            for off in range(0, STRIDE, 4):
                if prev[off:off + 4] != cur[off:off + 4]:
                    c["counts"][off] = c["counts"].get(off, 0) + 1
                    if off not in c["first"]:
                        c["first"][off] = (t, cur[off:off + 4])
            c["prev"] = cur
            if c["t_move"] is None:
                x = struct.unpack("<f", cur[POSX:POSX + 4])[0]
                z = struct.unpack("<f", cur[POSZ:POSZ + 4])[0]
                if ((x - c["x0"]) ** 2 + (z - c["z0"]) ** 2) ** 0.5 > MOVE_THRESHOLD:
                    c["t_move"] = t
                    print(f"  *** slot {s} {c['name']} LEFT GARAGE at t={t:.2f}s")

    lab = labels()
    left = {s: c for s, c in cars.items() if c["t_move"] is not None}
    print(f"\n{len(left)}/{len(cars)} cars left the garage during the trace")
    if not left:
        print("nothing to analyse - run longer or check the session is live")
        return

    # Per car: discrete words that first changed before that car's own exit.
    percar = {}
    for s, c in left.items():
        keep = {}
        for off, (t, nb) in c["first"].items():
            if t < c["t_move"] and c["counts"].get(off, 0) <= max_changes:
                keep[off] = (t, c["base"][off:off + 4], nb)
        percar[s] = keep

    common = set.intersection(*(set(k) for k in percar.values())) if percar else set()

    print(f"\n=== DISCRETE (<= {max_changes} changes) PRE-EXIT FLIPS COMMON TO ALL "
          f"{len(left)} CARS ({len(common)}) ===")
    if not common:
        print("  none - no single field precedes every exit")
    else:
        print(f"{'offset':>8} {'name':<34} {'example from->to':>26}  median t before exit")
        for off in sorted(common):
            nm, tier = lab.get(off, ("", ""))
            nm = f"{nm} [{tier}]" if nm else ""
            deltas = sorted(percar[s][off][0] - left[s]["t_move"] for s in percar)
            med = deltas[len(deltas) // 2]
            s0 = next(iter(percar))
            a, b = fmt(percar[s0][off][1], percar[s0][off][2])
            print(f"  0x{off:04X} {nm:<34} {a:>11} -> {b:<11}  {med:+.2f}s")

    # Per-car detail for the earliest exiter, as a sanity check on the filter.
    s0 = min(left, key=lambda s: left[s]["t_move"])
    k = percar[s0]
    print(f"\n=== slot {s0} {left[s0]['name']} - all discrete pre-exit flips "
          f"({len(k)}), exit at t={left[s0]['t_move']:.2f}s ===")
    print(f"{'offset':>8} {'t':>7} {'n':>4}  {'name':<32} {'from':>11} {'to':>11}")
    for off, (t, ob, nb) in sorted(k.items(), key=lambda kv: kv[1][0]):
        nm, tier = lab.get(off, ("", ""))
        nm = f"{nm} [{tier}]" if nm else ""
        a, b = fmt(ob, nb)
        print(f"  0x{off:04X} {t:7.2f} {left[s0]['counts'][off]:4d}  {nm:<32} {a:>11} {b:>11}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    c = sys.argv[1]
    if c == "find":
        cmd_find()
    elif c == "trace":
        secs = int(sys.argv[2]) if len(sys.argv) > 2 else 120
        hz = 20
        mx = 3
        if "--hz" in sys.argv:
            hz = int(sys.argv[sys.argv.index("--hz") + 1])
        if "--max-changes" in sys.argv:
            mx = int(sys.argv[sys.argv.index("--max-changes") + 1])
        cmd_trace(secs, hz, mx)
    else:
        sys.exit(__doc__)
