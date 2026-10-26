"""
sc_audio_test.py — play the PWS_SCAR safety-car clips via commentary injection.

THE PROBLEM: the five safety-car clips exist in PC_RE_MAIN.SDT (indices 137-141)
but NO event in the event table resolves to them, which is why they never play
and why tools/event_catalog.py has no SCAR entries.

THE CHAIN (docs 39, verified live 2026-07-25):
    eid -> event_table[eid].score      (int32 at 0x005BF9A0 + eid*0x14 + 0x10)
        -> word_5BEBB0[score]          (uint16 PWS lookup)
        -> SDT clip index

TableLookupByIndex_004144c0 returns dword_5BF9B0[5*eid], i.e. the score is
RE-READ FROM THE TABLE BY EVENT ID at play time - so writing a score into the
queue entry (as gr21's inject_commentary does) does NOT steer the clip. To play
an orphaned clip we must repoint a CARRIER event's score, inject it, then put the
score back.

Scores confirmed live by walking the lookup table:
    131 -> sdt 137 PWS_SCAR05      134 -> sdt 140 PWS_SCAR03
    132 -> sdt 138 PWS_SCAR01      135 -> sdt 141 PWS_SCAR04
    133 -> sdt 139 PWS_SCAR02      129 -> sdt 135 PWS_RED01 (carrier's own clip)

Carrier = 0x83 (PWS_RED01): proven injectable in mods/grid_reorder/gr18-gr28,
and a red flag will not fire during a safety-car test. Its score is restored
immediately after each injection.

WRITES TO THE RUNNING GAME (including .rdata, via VirtualProtectEx).

Usage:
    python tools/sc_audio_test.py list
    python tools/sc_audio_test.py play <SCAR01|SCAR02|SCAR03|SCAR04|SCAR05|RED01>
    python tools/sc_audio_test.py sequence          # all five, 6 s apart
"""

import ctypes
import re
import struct
import subprocess
import sys
import time

k32 = ctypes.windll.kernel32

CONTROLLER  = 0x005EA618
EVENT_TABLE = 0x005BF9A0
CARRIER_EID = 0x83

# clip name -> score that PWS lookup maps to its SDT index
CLIP_SCORES = {
    "SCAR01": (132, 138),
    "SCAR02": (133, 139),
    "SCAR03": (134, 140),
    "SCAR04": (135, 141),
    "SCAR05": (131, 137),
    "RED01":  (129, 135),
}

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
# VM_READ | VM_WRITE | VM_OPERATION | QUERY_INFORMATION
h = k32.OpenProcess(0x0010 | 0x0020 | 0x0008 | 0x0400, 0, pid)
if not h:
    sys.exit("OpenProcess failed - if the game runs ELEVATED, use an Admin shell")


def raw(a, n):
    b = ctypes.create_string_buffer(n)
    g = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(g))
    return b.raw if ok and g.value == n else None


def i32(a):
    r = raw(a, 4)
    return None if r is None else int.from_bytes(r, "little", signed=True)


def u16(a):
    r = raw(a, 2)
    return None if r is None else int.from_bytes(r, "little")


def wr(a, data):
    """Write, lifting page protection first - the event table is .rdata."""
    old = ctypes.c_ulong()
    k32.VirtualProtectEx(h, ctypes.c_void_p(a), len(data), 0x40, ctypes.byref(old))
    g = ctypes.c_size_t()
    buf = ctypes.create_string_buffer(data, len(data))
    ok = k32.WriteProcessMemory(h, ctypes.c_void_p(a), buf, len(data), ctypes.byref(g))
    k32.VirtualProtectEx(h, ctypes.c_void_p(a), len(data), old, ctypes.byref(old))
    return bool(ok) and g.value == len(data)


def wr32(a, v):
    return wr(a, struct.pack("<i", v))


def wrf(a, v):
    return wr(a, struct.pack("<f", v))


def score_addr(eid):
    return EVENT_TABLE + eid * 0x14 + 0x10


def inject_commentary(eid):
    """Port of inject_commentary() from mods/grid_reorder/gr21.c (in-game proven)."""
    table_pri = i32(EVENT_TABLE + eid * 0x14 + 0x00)
    table_score = i32(score_addr(eid))
    q = CONTROLLER + 0x864
    wr32(q + 0x00, eid)
    wr32(q + 0x04, table_score)
    wr32(q + 0x08, 0)
    wrf(q + 0x0C, -1.0)
    wr32(q + 0x10, -1)
    wr32(q + 0x14, -1)
    wr32(q + 0x18, -1)
    wr32(q + 0x1C, table_pri)
    wrf(CONTROLLER + 0xAF8 + eid * 4, 0.0)   # clear this event's cooldown
    wrf(CONTROLLER + 0xAE8, 0.0)             # clear global cooldown
    wr32(CONTROLLER + 0x135C, 1)             # kick the queue


def play(clip):
    """Repoint the carrier at `clip`, inject, restore. Returns False on a bad write."""
    if clip not in CLIP_SCORES:
        sys.exit(f"unknown clip {clip} - one of {', '.join(CLIP_SCORES)}")
    score, sdt = CLIP_SCORES[clip]
    orig = i32(score_addr(CARRIER_EID))
    if not wr32(score_addr(CARRIER_EID), score):
        print(f"  {clip}: FAILED to write carrier score (.rdata not writable?)")
        return False
    got = i32(score_addr(CARRIER_EID))
    if got != score:
        print(f"  {clip}: score write did not stick (read back {got})")
        wr32(score_addr(CARRIER_EID), orig)
        return False
    inject_commentary(CARRIER_EID)
    time.sleep(0.30)                          # let the tick pick the queue entry up
    wr32(score_addr(CARRIER_EID), orig)       # always restore
    print(f"  {clip}: injected (score {score} -> sdt {sdt}), carrier restored to {orig}")
    return True


def cmd_list():
    print(f"carrier eid 0x{CARRIER_EID:02x}, score field @ 0x{score_addr(CARRIER_EID):08x}")
    print(f"  current carrier score: {i32(score_addr(CARRIER_EID))}")
    print("  clip     score  sdt  lookup-check")
    for name, (score, sdt) in CLIP_SCORES.items():
        live = u16(0x005BEBB0 + score * 2)
        ok = "OK" if live == sdt else f"MISMATCH (live {live})"
        print(f"  {name:8s} {score:5d} {sdt:4d}  {ok}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    c = sys.argv[1]
    if c == "list":
        cmd_list()
    elif c == "play":
        play(sys.argv[2].upper().replace("PWS_", "").replace(".MP2", ""))
    elif c == "sequence":
        for name in ("SCAR01", "SCAR02", "SCAR03", "SCAR05", "SCAR04"):
            play(name)
            time.sleep(6.0)
    else:
        sys.exit(__doc__)
