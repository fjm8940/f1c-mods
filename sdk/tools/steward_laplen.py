#!/usr/bin/env python3
"""
steward_laplen.py - is the lap length readable from the game the moment a track
loads, on ANY track?

WHY
---
The safety-car deploy holds him in the garage until the leader is within N
metres of the start line, which needs the lap length. Learning it by watching
lap distances is wrong on lap 1 (the running maximum only ever equals the
leader's own distance, so the gate reads "0 m from the line" and releases him
at once), and learning it from a line crossing costs a whole lap.

The Steward already has it. Its marshalling-zone table - the one the yellow-flag
work decoded - carries:

    0x8ACEE0 + 0x108   float  scale       distance * scale = segment index
    0x8ACEE0 + 0x10C   int    segments    29 on Monza

so segments / scale is the lap length, in memory, before anyone has driven.
Monza: 29 / 0.005048069 = 5744.7702 against the AIW's own 5744.77.

This tool checks that on whatever track is loaded, against aiw_pits.json - which
is extracted from the shipped .AIW files and is the ground truth here.

READ-ONLY. Reads the game's memory, writes nothing.

USAGE
    python tools/steward_laplen.py [track_key]

`track_key` is a key of aiw_pits.json, e.g. 1999_Monza. Omitted, the value is
just printed with no comparison.
"""

import ctypes
import ctypes.wintypes as wt
import io
import json
import os
import struct
import subprocess
import sys

STEWARD    = 0x008ACEE0
OFF_SCALE  = 0x108
OFF_COUNT  = 0x10C
EXE        = "F1 Challenge Online.exe"
AIW_JSON   = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                          "aiw_pits.json")


def pid_of(name):
    out = subprocess.check_output(["tasklist", "/FI", "IMAGENAME eq " + name],
                                  text=True, errors="replace")
    for line in out.splitlines():
        if name.lower() in line.lower():
            for tok in line.split():
                if tok.isdigit():
                    return int(tok)
    return None


def main():
    pid = pid_of(EXE)
    if not pid:
        print("game is not running")
        return 1

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    h = k32.OpenProcess(0x1F0FFF, False, pid)
    if not h:
        print("cannot open the process")
        return 1

    def rd(addr, n):
        buf = (ctypes.c_char * n)()
        got = ctypes.c_size_t()
        ok = k32.ReadProcessMemory(wt.HANDLE(h), ctypes.c_void_p(addr), buf, n,
                                   ctypes.byref(got))
        return buf.raw if ok else None

    raw = rd(STEWARD + OFF_SCALE, 8)
    if raw is None:
        print("could not read the Steward - is a track loaded?")
        return 1

    scale = struct.unpack("<f", raw[0:4])[0]
    count = struct.unpack("<i", raw[4:8])[0]
    if scale <= 0.0 or count <= 0:
        print("Steward table not populated yet (scale %r, segments %r) - "
              "no track loaded" % (scale, count))
        return 1

    lap = count / scale
    print("scale     %.9f" % scale)
    print("segments  %d   (%.1f m each)" % (count, 1.0 / scale))
    print("lap length from the game = %.2f m" % lap)

    if len(sys.argv) > 1:
        key = sys.argv[1]
        with io.open(AIW_JSON, encoding="utf-8") as fh:
            allpits = json.load(fh)
        entry = allpits.get(key)
        if entry is None:
            hit = [k for k in allpits if k.lower() == key.lower()]
            entry = allpits[hit[0]] if hit else None
        if entry is None:
            print("no aiw_pits.json row for %r" % key)
            return 1
        truth = entry["meta"].get("lap_length")
        print("lap length from the .AIW  = %.2f m" % truth)
        print("difference = %.2f m" % (lap - truth))
    return 0


if __name__ == "__main__":
    sys.exit(main())
