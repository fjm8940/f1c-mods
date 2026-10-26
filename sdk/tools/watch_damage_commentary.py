"""
watch_damage_commentary.py — correlate part-detach damage with the commentary
event that actually fires.

Polls every live vehicle's damage fields AND the commentary controller's
per-event timestamp array. When a timestamp slot moves, that event just played
— that is the decisive signal for "which voice line fired", far more reliable
than catching the transient queue head.

Chain being verified (see functions/sub_41AFD0_0041afd0.c):
    WheelControllerInputDecoder / CollisionSparkParticleEffects
      -> RepairServiceStateDispatcher_004ed250(veh, partIdx)
      -> sub_41AFD0_0041afd0(veh, partIdx)   -> event id
      -> InsertDriverTableEntry_00418560(0x5EA618, buf)

Usage:
    python tools/watch_damage_commentary.py                # watch everything
    python tools/watch_damage_commentary.py --slot 3       # damage: one slot only
    python tools/watch_damage_commentary.py --hz 30 --secs 180

Read-only. Nothing is written to the game.
"""

import argparse
import ctypes
import json
import os
import re
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

# --- FACT: static addresses (non-ASLR, image base 0x400000) ---
VEH_PTRS       = 0x007C54F0   # array of live vehicle pointers
VEH_BASE       = 0x005EC6D8   # slot 0
VEH_STRIDE     = 0x48C0
VEH_COUNT_ADDR = 0x005C329C
RACE_ACTIVE    = 0x007D0308
SESSION_TYPE   = 0x007C56A4

CONTROLLER  = 0x005EA618      # commentary controller (from commentary_inject.py)
Q_HEAD      = CONTROLLER + 0x864
TS_ARRAY    = CONTROLLER + 0xAF8
CM_ENABLED  = CONTROLLER + 0xAF0
IS_PLAYING  = CONTROLLER + 0xAE4
PB_TS       = CONTROLLER + 0xAE8
EVENT_TABLE = 0x005BF9A0
TS_SLOTS    = 520             # covers every id seen in the corpus (max 514)

# --- vehicle fields we care about for part detach ---
# offset, label, ctype
DMG_FIELDS = [
    (0x3D6C, "carConditions", "i"),   # CONFIRMED, poke-proven severity 0/1/2
    (0x4064, "FinishState",   "i"),   # FACT 0=run 1=fin 2=DNF 3=DSQ
    (0x4068, "ReasonCode",    "i"),   # FACT; set to 6 only on wheel loss (cases 0-3)
    (0x4410, "RetiredFlag",   "i"),   # FACT
    (0x32C8, "liveFlag",      "i"),   # slot live <=> != -1
]

FS_MAP = {0: "RUN", 1: "FIN", 2: "DNF", 3: "DSQ"}

# --- event ids emitted by sub_41AFD0, DERIVED part mapping ---
PART_EVENTS = {
    106: "player wheel (partIdx 0)",
    107: "player wheel (partIdx 1)",
    108: "player wing A (partIdx 4, front?)",
    109: "player wheel (partIdx 2)",
    110: "player wheel (partIdx 3)",
    111: "player wing B (partIdx 5, rear?)",
    220: "PWS_AIHIT08 - EMITTED BY NOBODY (dead clip)",
    221: "AI wheel loss (partIdx 0-3)",
    222: "AI wing A (partIdx 4, front?)",
    223: "AI wing B (partIdx 5, rear?)",
}

CLIP_MAP_PATH = os.path.join(
    os.path.dirname(__file__), "..", "..",
    "research", "reverse_engineering",
    "F1C_ghidra_project", "event_clip_map.json")


def load_clip_map():
    try:
        with open(CLIP_MAP_PATH) as f:
            return {int(k): v for k, v in json.load(f).items()}
    except Exception:
        return {}


CLIP_MAP = load_clip_map()


def describe_event(eid):
    bits = [f"0x{eid:04x} ({eid})"]
    clip = CLIP_MAP.get(eid)
    if clip:
        bits.append(clip)
    note = PART_EVENTS.get(eid)
    if note:
        bits.append(f"<< {note}")
    return "  ".join(bits)


def find_pid():
    out = subprocess.check_output(
        ["tasklist", "/FI", "IMAGENAME eq F1 Challenge Online.exe"],
        text=True, errors="replace")
    for line in out.splitlines():
        if "F1 Challenge" in line:
            m = re.search(r"\b(\d+)\b", line)
            if m:
                return int(m.group(1))
    return None


class Mem:
    def __init__(self, pid):
        self.h = k32.OpenProcess(0x0010, 0, pid)   # PROCESS_VM_READ
        if not self.h:
            raise SystemExit("OpenProcess failed (try running as admin)")

    def raw(self, addr, size):
        buf = ctypes.create_string_buffer(size)
        n = ctypes.c_size_t()
        ok = k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, size,
                                   ctypes.byref(n))
        return buf.raw if ok and n.value == size else None

    def i32(self, addr):
        r = self.raw(addr, 4)
        return None if r is None else int.from_bytes(r, "little", signed=True)

    def u32(self, addr):
        r = self.raw(addr, 4)
        return None if r is None else int.from_bytes(r, "little")

    def f32(self, addr):
        import struct
        r = self.raw(addr, 4)
        return None if r is None else struct.unpack("<f", r)[0]

    def floats(self, addr, count):
        import struct
        r = self.raw(addr, count * 4)
        return None if r is None else list(struct.unpack("<%df" % count, r))

    def cstr(self, addr, size=40):
        r = self.raw(addr, size)
        if r is None:
            return "?"
        return r.split(b"\0")[0].decode("ascii", errors="replace").strip()


def vehicle_ptrs(m, limit=104):
    count = m.i32(VEH_COUNT_ADDR) or 0
    out = {}
    for i in range(min(count, limit)):
        p = m.u32(VEH_PTRS + i * 4)
        if p:
            out[i] = p
    return out


def read_damage(m, veh):
    d = {}
    for off, label, _ in DMG_FIELDS:
        d[label] = m.i32(veh + off)
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hz", type=float, default=20.0)
    ap.add_argument("--secs", type=float, default=600.0)
    ap.add_argument("--slot", type=int, default=None,
                    help="restrict damage reporting to one slot (commentary "
                         "events are always reported for all cars)")
    args = ap.parse_args()

    pid = find_pid()
    if not pid:
        raise SystemExit("Game not running")
    m = Mem(pid)

    print(f"pid={pid}  RaceActive={m.i32(RACE_ACTIVE)}  "
          f"SessionType={m.i32(SESSION_TYPE)}  "
          f"CmEnabled={m.i32(CM_ENABLED)}")
    if not m.i32(CM_ENABLED):
        print("  WARNING: commentary disabled in Options > Audio — "
              "timestamp array will not move")
    print(f"  clip map: {len(CLIP_MAP)} entries")

    cars = vehicle_ptrs(m)
    names = {i: m.cstr(p + 0x4620) for i, p in cars.items()}
    print(f"  {len(cars)} cars\n")

    prev_dmg = {i: read_damage(m, p) for i, p in cars.items()}
    prev_ts = m.floats(TS_ARRAY, TS_SLOTS) or [0.0] * TS_SLOTS
    prev_q = m.i32(Q_HEAD)

    t0 = time.time()
    period = 1.0 / args.hz
    print("watching — cause the damage now (Ctrl-C to stop)\n")

    try:
        while time.time() - t0 < args.secs:
            now = time.time() - t0

            # --- commentary: which event timestamp moved ---
            ts = m.floats(TS_ARRAY, TS_SLOTS)
            if ts:
                for eid, (a, b) in enumerate(zip(prev_ts, ts)):
                    # NaN != NaN is always true; unused slots hold NaN/FLT_MAX
                    if a != a and b != b:
                        continue
                    if a != b:
                        print(f"[{now:7.2f}] EVENT  {describe_event(eid)}"
                              f"   ts {a:.3f} -> {b:.3f}")
                prev_ts = ts

            q = m.i32(Q_HEAD)
            if q is not None and q != prev_q:
                print(f"[{now:7.2f}] QUEUE  head {prev_q} -> {q}"
                      f"  {describe_event(q) if 0 <= q < TS_SLOTS else ''}")
                prev_q = q

            # --- damage fields ---
            for i, p in cars.items():
                if args.slot is not None and i != args.slot:
                    continue
                cur = read_damage(m, p)
                old = prev_dmg.get(i, {})
                for label, v in cur.items():
                    ov = old.get(label)
                    if v != ov:
                        extra = ""
                        if label == "FinishState":
                            extra = f"  ({FS_MAP.get(v, v)})"
                        print(f"[{now:7.2f}] DMG    slot {i:2d} "
                              f"{names.get(i, '?'):<28s} "
                              f"{label:<14s} {ov} -> {v}{extra}")
                prev_dmg[i] = cur

            time.sleep(period)
    except KeyboardInterrupt:
        pass

    print("\n--- final state ---")
    for i, p in sorted(cars.items()):
        if args.slot is not None and i != args.slot:
            continue
        d = read_damage(m, p)
        print(f"slot {i:2d} {names.get(i, '?'):<28s} "
              + "  ".join(f"{k}={v}" for k, v in d.items()))


if __name__ == "__main__":
    main()
