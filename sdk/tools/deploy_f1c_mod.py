#!/usr/bin/env python3
"""Deploy a generated SDK DLL without overwriting unowned files."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import sys

parser = argparse.ArgumentParser()
parser.add_argument("dll", type=Path)
parser.add_argument("destination", type=Path, help="existing destination directory")
parser.add_argument("--replace-sdk-owned", action="store_true")
args = parser.parse_args()
source = args.dll.resolve()
destination = args.destination.resolve()
if not source.is_file() or source.suffix.lower() != ".dll":
    parser.error("dll must name an existing .dll")
if not destination.is_dir():
    parser.error("destination must be an existing directory")
if source.name.lower() == "pace_probe.dll":
    print("REFUSED: production pace_probe.dll is a protected name", file=sys.stderr)
    sys.exit(2)

target = destination / source.name
marker = destination / (source.name + ".f1c-sdk-owned.json")
if target.exists():
    if not args.replace_sdk_owned or not marker.is_file():
        print(f"REFUSED: target exists and is not replace-authorized: {target}", file=sys.stderr)
        sys.exit(2)
    ownership = json.loads(marker.read_text(encoding="utf-8"))
    if ownership.get("target") != source.name:
        print("REFUSED: ownership marker does not match target", file=sys.stderr)
        sys.exit(2)

shutil.copy2(source, target)
digest = hashlib.sha256(target.read_bytes()).hexdigest().upper()
marker.write_text(json.dumps({
    "schema_version": 1,
    "target": source.name,
    "sha256": digest,
    "source": str(source),
}, indent=2) + "\n", encoding="utf-8")
print(f"Deployed SDK-owned DLL: {target}")
print(f"SHA256: {digest}")

