#!/usr/bin/env python3
from pathlib import Path
import json

root = Path(__file__).resolve().parents[3]
index = json.loads((root / "analysis" / "recovery_index.json").read_text(encoding="utf-8"))
functions = index["functions"]
progress = functions["0x004A68B0"]
if progress["object_kind"] != "F1CPitMachine":
    raise SystemExit("PitStopProgress lost its reviewed pit-machine owner")
if progress["object_kind"] == "F1CVehicle":
    raise SystemExit("PitStopProgress regressed to the false vehicle type")
if "0x00424CD0" not in progress["callees"]:
    raise SystemExit("PitStopProgress -> AI command edge missing")
for address in ("0x004A5C40", "0x004A5C80", "0x004A5D10", "0x004A5E40",
                "0x004A5EA0", "0x004A5F20", "0x004A5FA0", "0x004A6340"):
    if functions[address]["object_kind"] != "F1CPitMachine":
        raise SystemExit(f"core pit owner missing at {address}")
for address in ("0x0042A110", "0x0042A6F0", "0x0042AC70"):
    if functions[address]["object_kind"] != "F1CVehicle":
        raise SystemExit(f"vehicle-side pit owner missing at {address}")
if index["summary"]["functions"] < 7000 or index["summary"]["call_edges"] < 10000:
    raise SystemExit("recovery index is unexpectedly incomplete")
print("Recovery index passed: pit owner, critical edge, and corpus coverage verified.")
