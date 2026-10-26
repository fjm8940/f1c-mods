"""
gen_sdk_from_index.py — Build the modding SDK from the VERIFIED name index.

Unlike F1C_ghidra_project/tools/gen_sdk.py (which compiles docs/12+13 —
Ghidra-era, mixed trust), this reads only crosschecked sources:
  name_index.json           trust-tiered globals + vehicle fields
  proven vehicle array      base 0x5EC6D8, stride 0x48C0, 104 slots
                            (BuildActiveVehiclePointerArray 0042bc80)

Emits the canonical SDK to tools/sdk/:
  f1c_sdk.json  — machine-readable; drop-in for poke.py / gen_hook.py / Frida
  f1c_sdk.h     — C accessor macros (exe is non-ASLR, image base 0x400000)

The historical mods/sdk export is retained as a lead database.  Its functions,
vtables and non-vehicle structures are copied into the canonical JSON with a
LEGACY trust tag; reviewed records in tools/sdk/f1c_sdk_overrides.json replace
those leads.  This prevents the two SDK generations from silently diverging.

Every entry carries its trust tier. FACT/CONFIRMED = safe to build on.
DERIVED = lead; verify with a live poke before shipping a mod on it.
"""
import os
import json
import time

IDA_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(IDA_ROOT, "tools", "sdk")
os.makedirs(OUT_DIR, exist_ok=True)

VEH_BASE, VEH_STRIDE, VEH_SLOTS, VEH_END = 0x5EC6D8, 0x48C0, 104, 0x7C54D8
VEH_PTR_ARRAY, SLOT_ACTIVE_OFF = 0x7C54F0, 0x32C8

ni = json.load(open(os.path.join(IDA_ROOT, "name_index.json"), encoding="utf-8"))
OVERRIDE_PATH = os.path.join(OUT_DIR, "f1c_sdk_overrides.json")
LEGACY_PATH = os.path.join(IDA_ROOT, "mods", "sdk", "f1c_sdk.json")
overrides = json.load(open(OVERRIDE_PATH, encoding="utf-8"))
legacy = json.load(open(LEGACY_PATH, encoding="utf-8")) if os.path.exists(LEGACY_PATH) else {}


def san(name):
    s = "".join(c if c.isalnum() or c == "_" else "_" for c in name.strip())
    return s if s and not s[0].isdigit() else "_" + s


sdk = {
    "_meta": {
        "generated": time.strftime("%Y-%m-%d %H:%M"),
        "source": "name_index.json + reviewed f1c_sdk_overrides.json + legacy mods/sdk leads",
        "image_base": "0x400000 (non-ASLR)",
        "schema_version": 2,
        "inputs": ["name_index.json", "tools/sdk/f1c_sdk_overrides.json",
                   "mods/sdk/f1c_sdk.json (LEGACY leads only)"],
    },
    "vehicle_array": {
        "base": hex(VEH_BASE), "stride": hex(VEH_STRIDE), "slots": VEH_SLOTS,
        "end": hex(VEH_END), "ptr_array": hex(VEH_PTR_ARRAY),
        "slot_active_field": hex(SLOT_ACTIVE_OFF),
        "slot_active_rule": "int32 at +0x32C8 != -1",
        "player_slot": 0, "trust": "FACT",
        "evidence": "BuildActiveVehiclePointerArray 0042bc80; 0x5F0DEC = slot0+0x4714 (F1CUI anchor)",
    },
    "vehicle_fields": {}, "globals": {},
}

# poke.py-compatible schemas: globals {name: {addr:int, type:str, desc:str}},
# structs.VehicleStruct {field: {offset:int, type, desc}}
# name_index "type" (poke.py vocabulary) -> C type for the generated header
C_TYPE = {"float": "float", "double": "double", "int": "int32_t",
          "int32": "int32_t", "uint32": "uint32_t", "int16": "int16_t",
          "uint16": "uint16_t", "int8": "int8_t", "uint8": "uint8_t",
          "byte": "uint8_t", "ptr": "void*", "string": "char", "embedded": "embedded"}
sdk["structs"] = {"VehicleStruct": {}}
merged_fields = {hex(int(k, 16)): dict(v)
                 for k, v in ni.get("vehicle_fields", {}).items()}
for off_hex, override in overrides.get("vehicle_fields", {}).items():
    key = hex(int(off_hex, 16))
    base = dict(merged_fields.get(key, {}))
    base.update(override)
    merged_fields[key] = base

for off_hex, e in sorted(merged_fields.items(), key=lambda kv: int(kv[0], 16)):
    nm = san(e["name"])
    ent = {"offset": int(off_hex, 16), "type": e.get("type", "float"),
           "desc": f"[{e.get('trust')}]" + (f" enum:{e['enum']}" if e.get("enum") else "")}
    sdk["vehicle_fields"][nm] = {"offset": off_hex, "trust": e.get("trust"),
                                 "ctype": C_TYPE.get(ent["type"], "float"),
                                 "enum": e.get("enum"), "sources": e.get("sources")}
    for key in ("semantic", "array", "note"):
        if e.get(key) is not None:
            sdk["vehicle_fields"][nm][key] = e[key]
    sdk["structs"]["VehicleStruct"][nm] = ent

for addr, e in sorted(ni.get("globals", {}).items(), key=lambda kv: int(kv[0], 16)):
    nm = san(e["name"])
    if nm.startswith(("_", "sub_")) or len(nm) < 3:
        continue
    key, n = nm, 2
    while key in sdk["globals"]:
        key = f"{nm}_{n}"; n += 1
    sdk["globals"][key] = {"addr": int(addr, 16), "type": "int",
                           "trust": e.get("trust"),
                           "desc": f"[{e.get('trust')}]"}
# poke.py's veh command needs PlayerVehiclePtr; static array makes it constant-
# derivable but the game also keeps the live pointer array:
sdk["globals"].setdefault("PlayerVehiclePtr",
    {"addr": VEH_PTR_ARRAY, "type": "ptr", "trust": "FACT",
     "desc": "[FACT] dword_7C54F0[0] = first active vehicle ptr"})

# Preserve the old broad export as explicitly unverified leads. Reviewed ABI
# Explicitly typed global evidence overrides the historical untyped names.
for name, entry in overrides.get("globals", {}).items():
    sdk["globals"][name] = dict(entry)

# Preserve the old broad export as explicitly unverified leads. Reviewed ABI
# records replace matching names and are the only functions emitted to C/C++.
sdk["functions"] = {
    name: {"address": addr, "trust": "LEGACY", "prototype": None}
    for name, addr in legacy.get("functions", {}).items()
}
for name, entry in overrides.get("functions", {}).items():
    sdk["functions"][name] = dict(entry)
    sdk["functions"][name]["address"] = int(entry["address"], 16)
sdk["vtables"] = {
    address: {"address": int(address, 16), "slots": slots, "trust": "LEGACY"}
    for address, slots in legacy.get("vtables", {}).items()
}
sdk["legacy_structs"] = legacy.get("structs", {})

json.dump(sdk, open(os.path.join(OUT_DIR, "f1c_sdk.json"), "w", encoding="utf-8"), indent=1)

H = ["// f1c_sdk.h — canonical SDK generated by gen_sdk_from_index.py. DO NOT EDIT.",
     "// Trust tags: FACT=poke-proven CONFIRMED=2-source DERIVED=lead(verify first)",
     "// Reviewed overrides supersede stale widths/names from the historical SDK.",
     "#pragma once", "#include <stddef.h>", "#include <stdint.h>", "",
     "#ifndef F1C_CUSTOM_VEH_ACCESS",
     f"#define VEH_BASE       0x{VEH_BASE:06X}u",
     f"#define VEH_STRIDE     0x{VEH_STRIDE:04X}u",
     f"#define VEH_SLOTS      {VEH_SLOTS}",
     f"#define VEH_PTR_ARRAY  ((void**)0x{VEH_PTR_ARRAY:06X})",
     "#define VEH(slot)      ((uint8_t*)VEH_BASE + (size_t)(slot)*VEH_STRIDE)",
     "#define VEH_PLAYER     VEH(0)",
     f"#define VEH_ACTIVE(s)  (*(int32_t*)(VEH(s)+0x{SLOT_ACTIVE_OFF:04X}) != -1)  // FACT",
     "#endif // F1C_CUSTOM_VEH_ACCESS",
     "", "// ---- vehicle struct fields (offset accessors on any VEH(slot)) ----"]
for nm, e in sdk["vehicle_fields"].items():
    H.append(f"#define F1C_OFF_{nm}  0x{int(e['offset'], 16):04X}u  // {e['trust']}"
             + (f" {e.get('semantic')}" if e.get("semantic") else ""))
    if e["ctype"] == "embedded":  # sub-object living inside the vehicle struct
        H.append(f"#define VEHF_{nm}(v)  ((uint8_t*)(v)+{e['offset']})  // {e['trust']} (embedded)")
    else:
        H.append(f"#define VEHF_{nm}(v)  (*({e['ctype']}*)((uint8_t*)(v)+{e['offset']}))  // {e['trust']}")
H.append("")
H.append("// ---- globals (absolute, non-ASLR). Cast per real type when known ----")
for nm, e in sdk["globals"].items():
    if e["trust"] in ("FACT", "CONFIRMED"):
        H.append(f"#define G_{nm}  ((void*)0x{e['addr']:08X})  // {e['trust']}")
H.append("// DERIVED globals omitted from header (see f1c_sdk.json; verify before use)")
H.append("")
H.append("// ---- reviewed function addresses (ABI metadata is in f1c_sdk.json) ----")
for nm, e in sorted(sdk["functions"].items()):
    if e.get("trust") in ("FACT", "CONFIRMED"):
        H.append(f"#define F1C_FN_{san(nm)}  0x{e['address']:08X}u  // {e['trust']} {e.get('calling_convention', '')}")
open(os.path.join(OUT_DIR, "f1c_sdk.h"), "w", encoding="utf-8").write("\n".join(H) + "\n")

nf = sum(1 for e in sdk["vehicle_fields"].values() if e["trust"] in ("FACT", "CONFIRMED"))
ng = sum(1 for e in sdk["globals"].values() if e["trust"] in ("FACT", "CONFIRMED"))
na = sum(1 for e in sdk["functions"].values() if e.get("trust") in ("FACT", "CONFIRMED"))
print(f"sdk: {len(sdk['vehicle_fields'])} vehicle fields ({nf} FACT/CONFIRMED), "
      f"{len(sdk['globals'])} globals ({ng} FACT/CONFIRMED), "
      f"{len(sdk['functions'])} functions ({na} reviewed ABI) -> tools/sdk/")
