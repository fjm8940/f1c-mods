#!/usr/bin/env python3
"""Mod: Cutting the track = instant Disqualification.

Redirects both cut-track penalty calls in the steward routine FUN_004bccc0
to the disqualify handler FUN_004bc0e0:

  0x004bd0aa  6A 0A            -> 90 90            NOP the 'push 10' (stopgo seconds)
  0x004bd0b3  E8 A8ECFFFF      -> E8 28F0FFFF      stopgo call  -> DQ
  0x004bd0c1  E8 9AEBFFFF      -> E8 1AF0FFFF      warning call -> DQ

Only these two call sites (the cut-track branch) are touched; pit-speeding,
blue-flag and wrong-way penalties are unaffected.

Writes a MODDED COPY; never modifies the original. Verifies original bytes first.
"""
import os
import shutil
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "..", "..", "..", ".."))
SRC = os.path.join(ROOT, "..", "F1 Challenge V10", "F1 Challenge Online.exe")
IMAGE_BASE = 0x00400000

# (VA, expected_original_hex, new_hex, note)
EDITS = [
    (0x004bd0aa, "6a0a",       "9090",       "NOP push 0xA (stopgo seconds)"),
    (0x004bd0b3, "e8a8ecffff", "e828f0ffff", "stopgo call -> DQ"),
    (0x004bd0c1, "e89aebffff", "e81af0ffff", "warning call -> DQ"),
]


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = struct.unpack_from("<H", data, pe + 20)[0]
    sstart = pe + 24 + opt
    out = []
    for i in range(nsec):
        off = sstart + i * 40
        va = struct.unpack_from("<I", data, off + 12)[0]
        vs = struct.unpack_from("<I", data, off + 8)[0]
        pr = struct.unpack_from("<I", data, off + 20)[0]
        out.append((va, vs, pr))
    return out


def va_to_fo(secs, va):
    rva = va - IMAGE_BASE
    for v, vs, pr in secs:
        if v <= rva < v + vs:
            return pr + (rva - v)
    return None


def main():
    if not os.path.exists(SRC):
        print(f"ERROR: source exe not found: {SRC}")
        sys.exit(1)

    data = bytearray(open(SRC, "rb").read())
    secs = sections(data)

    # Verify + stage edits
    staged = []
    for va, orig_hex, new_hex, note in EDITS:
        fo = va_to_fo(secs, va)
        if fo is None:
            print(f"ERROR: VA {va:#010x} not mapped")
            sys.exit(1)
        orig = bytes.fromhex(orig_hex)
        actual = bytes(data[fo:fo + len(orig)])
        if actual != orig:
            print(f"ERROR: byte mismatch at {va:#010x} (fo {fo:#x})")
            print(f"       expected {orig_hex}, found {actual.hex()}")
            print("       Aborting — wrong exe version or already patched.")
            sys.exit(1)
        staged.append((fo, orig, bytes.fromhex(new_hex), va, note))

    # Apply to copy
    dst = os.path.join(os.path.dirname(SRC), "F1 Challenge Online (CutTrackDQ).exe")
    for fo, orig, new, va, note in staged:
        data[fo:fo + len(new)] = new
        print(f"  patched {va:#010x}: {orig.hex()} -> {new.hex()}  ({note})")

    open(dst, "wb").write(data)
    print(f"\nWrote modded exe: {dst}")
    print("Original untouched. To use: launch the (CutTrackDQ) exe.")
    print("Rollback: delete the modded copy, or re-run to regenerate from original.")


if __name__ == "__main__":
    main()
