#!/usr/bin/env python3
"""Offline verifier for the one executable build supported by the SDK."""
from pathlib import Path
import argparse
import hashlib
import struct
import sys

EXPECTED = {
    "machine": 0x14C,
    "timestamp": 0x3E9C24F6,
    "entry_rva": 0x4D8000,
    "image_base": 0x400000,
    "image_size": 0x5FA000,
    "checksum": 0x306F7B,
    "file_size": 3141632,
    "sha256": "1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003",
}

parser = argparse.ArgumentParser()
parser.add_argument("exe", type=Path)
args = parser.parse_args()
data = args.exe.read_bytes()
pe = struct.unpack_from("<I", data, 0x3C)[0]
actual = {
    "machine": struct.unpack_from("<H", data, pe + 4)[0],
    "timestamp": struct.unpack_from("<I", data, pe + 8)[0],
    "entry_rva": struct.unpack_from("<I", data, pe + 40)[0],
    "image_base": struct.unpack_from("<I", data, pe + 52)[0],
    "image_size": struct.unpack_from("<I", data, pe + 80)[0],
    "checksum": struct.unpack_from("<I", data, pe + 88)[0],
    "file_size": len(data),
    "sha256": hashlib.sha256(data).hexdigest().upper(),
}
bad = False
for key, expected in EXPECTED.items():
    value = actual[key]
    ok = value == expected
    shown = hex(value) if isinstance(value, int) else value
    print(("OK   " if ok else "FAIL ") + f"{key}: {shown}")
    bad |= not ok
sys.exit(1 if bad else 0)
