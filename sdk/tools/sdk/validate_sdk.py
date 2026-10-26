#!/usr/bin/env python3
"""Validate that reviewed SDK evidence, generated C, and IDA layout agree."""
from pathlib import Path
import json
import re
import sys

SDK = Path(__file__).resolve().parent
ROOT = SDK.parent.parent

canonical = json.loads((SDK / "f1c_sdk.json").read_text(encoding="utf-8"))
overrides = json.loads((SDK / "f1c_sdk_overrides.json").read_text(encoding="utf-8"))
header = (SDK / "f1c_sdk.h").read_text(encoding="utf-8")
vehicle_header = (SDK / "f1c_vehicle_struct.h").read_text(encoding="utf-8")
pit_header = (SDK / "f1c_pit_struct.h").read_text(encoding="utf-8")
target_header = (SDK / "f1c_target.h").read_text(encoding="utf-8")
version_header = (SDK / "f1c_sdk_version.h").read_text(encoding="utf-8")
trace_schema = (SDK / "f1c_trace_schema.h").read_text(encoding="utf-8")
reviewed_api = (SDK / "f1c_reviewed_api.hpp").read_text(encoding="utf-8")

errors = []

if canonical.get("_meta", {}).get("schema_version") != 2:
    errors.append("canonical JSON is not schema version 2")

by_offset = {int(v["offset"], 16): (name, v)
             for name, v in canonical.get("vehicle_fields", {}).items()}
for raw_offset, expected in overrides.get("vehicle_fields", {}).items():
    offset = int(raw_offset, 16)
    actual_pair = by_offset.get(offset)
    if not actual_pair:
        errors.append(f"missing reviewed field at {raw_offset}")
        continue
    name, actual = actual_pair
    if name != expected["name"]:
        errors.append(f"{raw_offset}: name {name!r} != {expected['name']!r}")
    if actual.get("ctype") != {
        "float": "float", "double": "double", "int32": "int32_t",
        "uint8": "uint8_t", "ptr": "void*"
    }.get(expected["type"], expected["type"]):
        errors.append(f"{raw_offset}: generated type disagrees with override")
    macro = f"F1C_OFF_{expected['name']}"
    if not re.search(rf"^#define {re.escape(macro)}\s+0x{offset:04X}u\b",
                     header, re.MULTILINE):
        errors.append(f"generated C header missing {macro}")

reviewed = {n: f for n, f in canonical.get("functions", {}).items()
            if f.get("trust") in ("FACT", "CONFIRMED")}
for name, function in reviewed.items():
    for key in ("address", "calling_convention", "thread", "source", "abi_evidence"):
        if not function.get(key):
            errors.append(f"reviewed function {name} lacks {key}")
    if not (ROOT / function["source"]).exists():
        errors.append(f"reviewed function {name} source does not exist")

if len(canonical.get("functions", {})) < 6000:
    errors.append("historical function lead catalogue was not merged")
if len(canonical.get("legacy_structs", {})) < 100:
    errors.append("historical structure lead catalogue was not merged")
if "uint8_t RearWingIntactFlag;  // 0x0D58 FACT" not in vehicle_header:
    errors.append("vehicle layout lost byte-wide RearWingIntactFlag")
if "int32_t PitStatus;  // 0x3450 FACT" not in vehicle_header:
    errors.append("vehicle layout lost typed PitStatus")
if "struct F1CPitMachine" not in pit_header or "gap_008[0x178]" not in pit_header:
    errors.append("pit-machine reviewed layout is missing")
if '#define F1C_SDK_VERSION_STRING "1.0.0"' not in version_header:
    errors.append("SDK public version is missing or unexpected")
if "F1C_TARGET_SHA256" not in target_header or "F1C_TARGET_IMAGE_SIZE" not in target_header:
    errors.append("executable identity gate is incomplete")
trace_count = re.search(r"#define F1C_TRACE_FIELD_COUNT (\d+)", trace_schema)
if not trace_count or int(trace_count.group(1)) < 80:
    errors.append("trusted runtime trace schema is unexpectedly small")
for name in reviewed:
    if f"inline " not in reviewed_api or name not in reviewed_api:
        errors.append(f"reviewed C++ API missing {name}")
for required in (
        SDK / "f1c_runtime.h", SDK / "f1c_hook.h", SDK / "f1c_trace.h",
        SDK / "templates" / "native_mod" / "mod.c.in",
        ROOT / "tools" / "create_f1c_mod.py",
        ROOT / "tools" / "deploy_f1c_mod.py"):
    if not required.exists():
        errors.append(f"new-mod workflow file missing: {required}")

if errors:
    print("SDK validation FAILED:")
    for error in errors:
        print(" -", error)
    sys.exit(1)

print("SDK validation passed: "
      f"{len(by_offset)} vehicle fields, {len(reviewed)} reviewed ABIs, "
      f"{len(canonical['functions']) - len(reviewed)} legacy function leads.")
