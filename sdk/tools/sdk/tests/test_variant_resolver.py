#!/usr/bin/env python3
"""Synthetic relocation and snapshot-diff tests; never executes a variant."""
from pathlib import Path
import hashlib
import json
import os
import struct
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))

from process_memory_snapshot import diff_snapshots
from variant_resolver import PEImage, bootstrap_f1c_recipes, resolve_recipes

EXE = Path(os.environ.get("F1C_SDK_TEST_EXE", str(ROOT.parent / "F1 Challenge V10" / "F1 Challenge Online.exe")))
recipes = bootstrap_f1c_recipes(EXE, ROOT / "tools" / "ida_functions.csv")
canonical = resolve_recipes(EXE, recipes)
if canonical["status"] != "RESOLVED":
    raise SystemExit("canonical executable did not resolve")
if canonical["target"]["dynamic_base"]:
    raise SystemExit("canonical target unexpectedly advertises ASLR")

pe = PEImage(EXE)
new_rvas = {"BuildActiveVehiclePointerArray": 0x84CFC,
            "PitStopDecisionTireCompound": 0x850FC}
new_values = {"VehicleArray": 0x005ED6D8, "VehicleStride": 0x48D0,
              "VehicleLivenessOffset": 0x32D8, "FrontWingRecordPtr": 0xCE4,
              "FrontWingIntactFlag": 0xD24}

def relocated_variant(values):
    data = bytearray(pe.data)
    for function_name, function in recipes["functions"].items():
        old_va = int(function["canonical_va"], 16)
        old_offset = pe.rva_to_offset(old_va - pe.image_base)
        body = bytearray(data[old_offset:old_offset + function["size"]])
        for symbol_name, symbol in recipes["symbols"].items():
            if symbol["function"] != function_name:
                continue
            for context in symbol["contexts"]:
                struct.pack_into("<I", body, context["canonical_function_offset"],
                                 values[symbol_name])
        new_offset = pe.rva_to_offset(new_rvas[function_name])
        data[new_offset:new_offset + len(body)] = body
        data[old_offset:old_offset + len(body)] = b"\xCC" * len(body)
    return data

with tempfile.TemporaryDirectory(prefix="f1c_variant_resolver_") as temporary:
    temporary = Path(temporary)
    variant_path = temporary / "synthetic_relocated.exe"
    variant_path.write_bytes(relocated_variant(new_values))
    resolved = resolve_recipes(variant_path, recipes)
    if resolved["status"] != "RESOLVED" or resolved["confidence"] < 0.7:
        raise SystemExit("synthetically relocated functions/operands did not resolve")
    expected_functions = {name: f"0x{pe.image_base + rva:08X}" for name, rva in new_rvas.items()}
    actual_functions = {name: item["address"] for name, item in resolved["functions"].items()}
    if actual_functions != expected_functions:
        raise SystemExit(f"relocated function mismatch: {actual_functions}")
    actual_values = {name: item["numeric_value"] for name, item in resolved["symbols"].items()}
    if actual_values != new_values:
        raise SystemExit(f"relocated operand mismatch: {actual_values}")

    inconsistent = dict(new_values)
    inconsistent["FrontWingIntactFlag"] = 0xD28
    refused_path = temporary / "synthetic_inconsistent.exe"
    refused_path.write_bytes(relocated_variant(inconsistent))
    refused = resolve_recipes(refused_path, recipes)
    if refused["status"] != "REFUSED" or all(item["passed"] for item in refused["invariants"]):
        raise SystemExit("inconsistent field relationship was not refused")

    # Memory pages match by module RVA even when snapshot absolute bases differ.
    store = temporary / "page_store"; store.mkdir()
    before_bytes = bytes(0x1000)
    after_data = bytearray(before_bytes); after_data[0x123] = 1
    after_bytes = bytes(after_data)
    after_hash = hashlib.sha256(after_bytes).hexdigest().upper()
    (store / f"{after_hash}.zlib").write_bytes(zlib.compress(after_bytes, 9))
    before_hash = hashlib.sha256(before_bytes).hexdigest().upper()
    manifests = []
    for name, base, digest, zero in (("before", 0x400000, before_hash, True),
                                     ("after", 0x500000, after_hash, False)):
        directory = temporary / name; directory.mkdir()
        manifest = {
            "schema_version": 1, "pid": 1, "store": str(store),
            "main_module": {"sha256": "SAME"},
            "pages": [{"address": f"0x{base:08X}", "size": 0x1000,
                       "sha256": digest, "zero": zero,
                       "module": "fixture.exe", "rva": "0x00000000"}],
        }
        path = directory / "manifest.json"
        path.write_text(json.dumps(manifest), encoding="utf-8")
        manifests.append(path)
    difference = diff_snapshots(temporary, manifests[0], manifests[1],
                                transition=(0, 1), width=1)
    if difference["summary"]["changed_pages"] != 1:
        raise SystemExit("module-RVA page comparison failed across relocated bases")
    candidates = difference["transition_candidates"]
    if len(candidates) != 1 or candidates[0].get("module_rva") != "0x00000123":
        raise SystemExit(f"0->1 transition candidate mismatch: {candidates}")

print("Variant resolver passed: canonical, relocated operands/functions, invariant refusal, RVA snapshot diff.")
