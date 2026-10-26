#!/usr/bin/env python3
"""Typed comparison of two 0x48C0 vehicle snapshots using canonical fields."""
from pathlib import Path
import argparse
import json
import struct

ROOT = Path(__file__).resolve().parents[1]
catalogue = json.loads((ROOT / "tools" / "sdk" / "f1c_sdk.json").read_text(encoding="utf-8"))
parser = argparse.ArgumentParser()
parser.add_argument("before", type=Path)
parser.add_argument("after", type=Path)
parser.add_argument("--all-trust", action="store_true", help="include DERIVED/GUESS fields")
parser.add_argument("--semantic", help="filter, e.g. output or command_input")
parser.add_argument("--json", action="store_true")
args = parser.parse_args()

before = args.before.read_bytes()
after = args.after.read_bytes()
if len(before) != 0x48C0 or len(after) != 0x48C0:
    parser.error("both inputs must be complete 0x48C0 vehicle snapshots")

def decode(data, offset, ctype):
    if ctype == "uint8_t": return data[offset]
    if ctype == "int32_t": return struct.unpack_from("<i", data, offset)[0]
    if ctype == "float": return struct.unpack_from("<f", data, offset)[0]
    if ctype == "void*": return f"0x{struct.unpack_from('<I', data, offset)[0]:08X}"
    if ctype == "char":
        return data[offset:data.find(b"\0", offset, min(offset + 128, len(data)))].decode(
            "latin1", errors="replace")
    return None

changes = []
for name, field in catalogue["vehicle_fields"].items():
    if not args.all_trust and field.get("trust") not in ("FACT", "CONFIRMED"):
        continue
    if args.semantic and field.get("semantic", "state") != args.semantic:
        continue
    offset = int(field["offset"], 16)
    old = decode(before, offset, field.get("ctype"))
    new = decode(after, offset, field.get("ctype"))
    if old is None or old == new:
        continue
    changes.append({
        "offset": f"0x{offset:04X}", "name": name,
        "type": field.get("ctype"), "trust": field.get("trust"),
        "semantic": field.get("semantic", "state"),
        "before": old, "after": new,
    })

if args.json:
    print(json.dumps(changes, indent=2))
else:
    for change in changes:
        print(f"{change['offset']} {change['name']} [{change['trust']}/{change['semantic']}]: "
              f"{change['before']} -> {change['after']}")
    print(f"{len(changes)} trusted named field changes")

