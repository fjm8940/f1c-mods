"""
gen_vehicle_retype_candidates.py — Deterministically pick the functions whose
`this` is a vehicle object, so apply_vehicle_retype_idb.py can retype arg1 to
`F1CVehicle *` without an LLM guessing.

Evidence rule (no prose, no model judgement): count how many DISTINCTIVE vehicle
anchor offsets a function dereferences off `this`. Anchors are offsets that are
large, structurally proven, and unlikely to collide with another class's layout
by chance (identity strings, retirement/pit flags, the slot-live marker).

  >=2 anchors -> candidate (default)
   1 anchor   -> listed but NOT emitted; one shared offset is weak evidence and
                 a wrong retype renders confident, wrong field names.

Hex-Rays prints byte offsets in DECIMAL (`this + 17680`), not hex — matching on
hex is why an earlier pass badly under-counted.

Output: tools/sdk/vehicle_retype_candidates.json  (auditable; edit before applying)
"""
import json
import os
import re

IDA_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUNCS = os.path.join(IDA_ROOT, "functions")
OUT = os.path.join(IDA_ROOT, "tools", "sdk", "vehicle_retype_candidates.json")
MIN_ANCHORS = 2

# Distinctive anchors, all FACT/CONFIRMED in name_index.json:
#   0x32C8 slot-live/wear gate   0x4064 FinishState   0x4068 ReasonCode
#   0x4410 RetiredFlag           0x4440 Speed         0x454C callToBox
#   0x45F4 AIControlFlag         0x4620 DriverName    0x4678 CarName
#   0x4714 VehiclePath (the F1CUI anchor)             0x489C DriverID
ANCHORS = {0x32C8, 0x4064, 0x4068, 0x4410, 0x4440, 0x454C,
           0x45F4, 0x4620, 0x4678, 0x4714, 0x489C}

ni = json.load(open(os.path.join(IDA_ROOT, "name_index.json"), encoding="utf-8"))
known = {int(k, 16) for k in ni["vehicle_fields"]}

pat = re.compile(r"this \+ (\d+)")
addr_pat = re.compile(r"_([0-9a-f]{8})\.c$")

rows = []
for fn in sorted(os.listdir(FUNCS)):
    m = addr_pat.search(fn)
    if not m:
        continue
    txt = open(os.path.join(FUNCS, fn), encoding="utf-8", errors="ignore").read()
    hits = {int(x) for x in pat.findall(txt)}
    if not hits:
        continue
    sig = hits & ANCHORS
    if not sig:
        continue
    rows.append({
        "addr": "0x%s" % m.group(1).upper(),
        "func": fn[:m.start()],
        "file": fn,
        "anchors": len(sig),
        "anchor_offsets": sorted("0x%X" % o for o in sig),
        "named_accesses": len(hits & known),
        "total_this_accesses": len(hits),
    })

rows.sort(key=lambda r: (-r["anchors"], -r["named_accesses"]))
emit = [r for r in rows if r["anchors"] >= MIN_ANCHORS]

json.dump({
    "_meta": {
        "rule": f">={MIN_ANCHORS} distinctive vehicle anchor offsets dereferenced off `this`",
        "anchors": sorted("0x%X" % o for o in ANCHORS),
        "note": "DERIVED-grade inference. Back up the .i64 before applying; "
                "spot-check a couple of decompilations after.",
    },
    "candidates": emit,
    "rejected_single_anchor": [r for r in rows if r["anchors"] < MIN_ANCHORS],
}, open(OUT, "w", encoding="utf-8"), indent=1)

print(f"{len(rows)} functions touch >=1 anchor; {len(emit)} candidates (>={MIN_ANCHORS})")
for r in emit:
    print(f"  {r['addr']}  {r['anchors']} anchors  {r['named_accesses']:3d} named  {r['func']}")
print(f"-> {OUT}")
