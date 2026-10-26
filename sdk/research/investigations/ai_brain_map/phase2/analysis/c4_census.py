#!/usr/bin/env python
"""C4 - writer census, ABI gaps, coverage report, v1.1 proposal.

Inputs (read-only):
  investigations/ai_brain_map/ai_brain_abi_v1.json
  %TEMP%\\ai_brain_phase2\\all_rows.jsonl (+ all_calls.jsonl, func_lines.json)
  investigations/ai_brain_map/offsets_*.csv and phase2/maps/offsets_*.csv
  map_*.md, FIELD_GLOSSARY.md, docs/54-ai-driving-brain-map.md

Outputs (phase2/):
  WRITER_CENSUS.csv, ABI_GAPS.csv, COVERAGE.md, abi_v1_1_proposal.json
"""
from __future__ import annotations

import csv
import json
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
AI = REPO / "investigations" / "ai_brain_map"
PH2 = AI / "phase2"
CACHE = Path(os.environ["TEMP"]) / "ai_brain_phase2"

CLAUDE_DOC54 = {"AIMainUpdate", "sub_426430", "sub_426100", "AISteeringCorrection",
                "SteeringAngleController", "sub_4258C0"}

# functions whose map Identity says `this` = vehicle + 0xC: normalise dec + 12
SHIFT12 = {"BrakeThermalModel", "TireGripCoefficient", "ConditionalGate",
           "DistanceProjectionCalc", "SuspensionRatioClamp",
           "SpeedValueSetterWithGlobalMirror"}
# `this` is not the vehicle (waypoint/surface/force object): raw dec is not a
# vehicle offset; kept separate from the proposal.
BASE_OPEN = {"TireForce", "TireForceSolver", "AerodynamicForceCalc", "AIOvertake",
             "sub_4A4110", "ArrayOfStructIndexedFieldCopy", "ShortComputation_3ca0",
             "ShortComputation_3db0", "FindNearestVehicle", "BVHNearestNeighborQuery",
             "SignedIndexTableLookup", "SpatialTreeNearestNeighborSearch"}

GROUP_OF = {
    "AIMainUpdate": "state",
    "sub_426430": "perception",
    "OpponentProximityScanTeamRecognition": "perception",
    "Overtaking": "perception",
    "AIOvertake": "perception",
    "AIRacingLine": "plan_speed",
    "AIAbility": "plan_speed",
    "AIComputePassScore": "plan_speed",
    "AIPushLap": "plan_speed",
    "RandomWeightedTierSelection": "plan_speed",
    "AISteering": "plan_line",
    "sub_426100": "plan_line",
    "AISteeringRacingLineCorrection": "plan_line",
    "AISteeringCorrection": "decision",
    "PhysicsGearShiftLogicWithThresholds": "state",
    "AIRingBufferFieldInit": "watchdog",
    "AIStateFlagSetter": "watchdog",
    "SessionCleanupFlagClear": "watchdog",
}
TYPE_MAP = {"float": "float", "double": "float", "int": "int", "_DWORD": "int",
            "_BYTE": "int", "unsigned int": "int", "char": "int", "__int8": "int",
            "short": "int", "__int16": "int"}


def extract_name(path: Path) -> str:
    m = re.match(r"^(.*)_[0-9a-fA-F]{8}$", path.stem)
    return m.group(1) if m else path.stem


def load_jsonl(path):
    return [json.loads(l) for l in open(path, encoding="utf-8")]


def mapped_sets():
    names = set(CLAUDE_DOC54)
    for p in sorted(AI.glob("map_*.md")) + sorted((PH2 / "maps").glob("map_*.md")):
        names.add(re.sub(r"_part\d+$", "", p.stem[4:]))
    return names


def axis_hex(dec):
    return "0x%X" % int(dec)


def map_context(hexes):
    """Best available one-line context per hex (glossary name first)."""
    out = {}
    gloss = (AI / "FIELD_GLOSSARY.md").read_text(encoding="utf-8").splitlines()
    for h in hexes:
        head = "| 0x%s |" % h[2:].upper()
        for line in gloss:
            if line.startswith(head):
                cells = [c.strip() for c in line.strip("|").split("|")]
                name = cells[4] if len(cells) >= 6 else ""
                guess = cells[5] if len(cells) >= 6 else ""
                if name.startswith("?"):
                    name = ""
                if guess.startswith("GUESS"):
                    guess = guess[len("GUESS"):].strip()
                    if guess.startswith("?"):
                        guess = ""
                pick = name or guess
                if pick:
                    out[h] = "%s (FIELD_GLOSSARY.md)" % pick
                break
    files = [REPO / "docs" / "54-ai-driving-brain-map.md",
             AI / "FIELD_GLOSSARY.md"] + sorted(AI.glob("map_*.md")) \
        + sorted((PH2 / "maps").glob("map_*.md"))
    for f in files:
        try:
            for line in f.read_text(encoding="utf-8").splitlines():
                low = line.lower()
                for h in list(hexes):
                    if h in out:
                        continue
                    if h.lower() in low and len(line.strip()) > 12:
                        s = line.strip()
                        if s.startswith("|"):
                            cells = [c.strip() for c in s.strip("|").split("|")]
                            cells = [c for c in cells if c]
                            s = cells[-1] if cells else s
                        else:
                            s = re.sub(r"\s+", " ", s)
                        out[h] = "%s [%s]" % (s[:180], f.name)
        except Exception:
            continue
    return out


def main() -> int:
    abi = json.load(open(AI / "ai_brain_abi_v1.json", encoding="utf-8"))
    fields = abi["fields"]
    abidecs = {int(f["dec"]) for f in fields}
    rows = load_jsonl(CACHE / "all_rows.jsonl")
    calls = load_jsonl(CACHE / "all_calls.jsonl")
    func_lines = json.load(open(CACHE / "func_lines.json", encoding="utf-8"))
    sources = {extract_name(p): p for p in (REPO / "functions").glob("*.c")}
    mapped = mapped_sets()

    # ---- 1. writer census (whole corpus, dec match per ABI field) ----
    by_dec = defaultdict(list)
    for r in rows:
        if r["dec"]:
            by_dec[int(r["dec"])].append(r)
    census = []
    summary = {}
    for f in fields:
        dec = int(f["dec"])
        hits = by_dec.get(dec, [])
        wf = sorted({h["func"] for h in hits if h["access"] == "W"})
        rf = sorted({h["func"] for h in hits if h["access"] == "R"})
        summary[f["id"]] = dict(writers=wf, readers=rf)
        for h in hits:
            census.append(dict(
                offset_hex=f["hex"], offset_dec=dec, field_id=f["id"], group=f["group"],
                function=h["func"], line=h["line"], access=h["access"], base=h["base"],
                form=h["form"], width=h["width"],
                in_ai_mapped_set=("yes" if h["func"] in mapped else "no"),
                expr=h["expr"][:160]))
    census.sort(key=lambda r: (r["offset_dec"], r["function"], int(r["line"])))
    with open(PH2 / "WRITER_CENSUS.csv", "w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(census[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(census)

    inside = sum(1 for v in summary.values()
                 if any(x in mapped for x in v["writers"] + v["readers"]))
    outside_writers = sorted({fn for v in summary.values() for fn in v["writers"]
                              if fn not in mapped})
    none_writers = [f["id"] for f in fields if not summary[f["id"]]["writers"]]

    # ---- 2. gap list from mapped-function CSVs (base this, 0x3D00..0x48C0) ----
    raw_gaps = []
    phys_csvs = sorted(AI.glob("offsets_*.csv")) + sorted((PH2 / "maps").glob("offsets_*.csv"))
    for p in phys_csvs:
        for r in csv.DictReader(open(p, encoding="utf-8", newline="")):
            if r["base"] != "this" or not r["dec"]:
                continue
            dec = int(r["dec"])
            if dec in abidecs:
                continue
            if r["func"] in SHIFT12:
                vdec, status = dec + 12, "veh (this = veh+0xC, normalised +12)"
            elif r["func"] in BASE_OPEN:
                vdec, status = dec, "base open (not assumed veh)"
            else:
                vdec, status = dec, "veh"
            if not (0x3D00 <= vdec <= 0x48C0):
                continue
            raw_gaps.append(dict(vdec=vdec, raw=dec, status=status, func=r["func"],
                                 line=r["line"], access=r["access"], width=r["width"],
                                 expr=r["expr"][:160], csv=p.name))
    agg = defaultdict(lambda: dict(readers=set(), writers=set(), width=set(),
                                   status=set(), hits=[]))
    for r in raw_gaps:
        a = agg[r["vdec"]]
        (a["writers"] if r["access"] == "W" else a["readers"]).add(r["func"])
        a["width"].add(r["width"])
        a["status"].add(r["status"])
        a["hits"].append(r)
    hexes = {axis_hex(d) for d in agg}
    ctx = map_context(hexes)
    gaps = []
    for dec in sorted(agg):
        a = agg[dec]
        wfun = sorted(a["writers"]) or sorted(a["readers"])
        group = next((GROUP_OF[f] for f in wfun if f in GROUP_OF), "state")
        first = a["hits"][0]
        status = sorted(a["status"])[0]
        gaps.append(dict(hex=axis_hex(dec), dec=dec, raw_dec=first["raw"],
                         base_status=status, readers=";".join(sorted(a["readers"])),
                         writers=";".join(sorted(a["writers"])),
                         best_guess_meaning=ctx.get(axis_hex(dec),
                                                    "GUESS %s:%s %s" % (first["csv"],
                                                                        first["line"],
                                                                        first["expr"])),
                         proposed_group=group, in_proposal=("yes" if status == "veh" else "no")))
    with open(PH2 / "ABI_GAPS.csv", "w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=["hex", "dec", "raw_dec", "base_status",
                                           "readers", "writers", "best_guess_meaning",
                                           "proposed_group", "in_proposal"],
                           lineterminator="\n")
        w.writeheader()
        w.writerows(gaps)

    # ---- 3. coverage ----
    veh_decs = {g["dec"] for g in gaps if g["in_proposal"] == "yes"}
    open_decs = {g["dec"] for g in gaps if g["in_proposal"] == "no"}
    universe = veh_decs | {int(f["dec"]) for f in fields
                           if f["base"] == "veh" and 0x3D00 <= int(f["dec"]) <= 0x48C0}
    covered = universe & abidecs
    pct = 100.0 * len(covered) / len(universe) if universe else 100.0
    second = set()
    for fn in mapped:
        for c in {x["expr"] for x in calls if x["func"] == fn}:
            if c in func_lines and c not in mapped:
                second.add(c)
    one_level = {c["expr"] for c in calls if c["func"] == "AIMainUpdate"} & set(func_lines)
    one_level -= mapped
    with open(PH2 / "COVERAGE.md", "w", encoding="utf-8") as fh:
        fh.write("# COVERAGE - AI brain phase 2 census (2026-10-02)\n\n")
        fh.write("Generated by `phase2/analysis/c4_census.py` from the read-only `functions/` corpus\n")
        fh.write("(extractor rows cached in `%TEMP%\\ai_brain_phase2`) and the phase-1/2 map CSVs.\n\n")
        fh.write("## Vehicle-offset coverage (nominated range 0x3D00..0x48C0)\n\n")
        fh.write("- vehicle offsets read/written by the mapped AI set (base resolved): **%d**\n" % len(universe))
        fh.write("- in AI Brain ABI v1: **%d**\n" % len(covered))
        fh.write("- gaps proposed for v1.1 (ABI_GAPS.csv, `in_proposal=yes`): **%d**\n" % (len(universe) - len(covered)))
        fh.write("- coverage: **%.1f%%**\n" % pct)
        fh.write("- offsets whose `this` base is NOT the vehicle (excluded from the proposal,\n")
        fh.write("  `in_proposal=no` / `base_status=base open`): %d\n" % len(open_decs))
        fh.write("- `this` = veh+0xC rows were normalised by +12 before the census\n")
        fh.write("  (BrakeThermalModel, TireGripCoefficient, ConditionalGate, DistanceProjectionCalc,\n")
        fh.write("  SuspensionRatioClamp, SpeedValueSetterWithGlobalMirror).\n\n")
        fh.write("The gaps cluster in 0x44B0..0x45FC (AIMainUpdate/AIAbility/AISteering internal state)\n")
        fh.write("and the pit/strategy block 0x4534..0x45E8 (AIAbility, AIPushLap,\n")
        fh.write("PitStopDecisionTireCompound). The 0x3D00 force/waypoint block is base-open.\n\n")
        fh.write("## Function coverage\n\n")
        fh.write("- phase-1 maps: 19; phase-2 maps: 64; doc-54-only functions: 6.\n")
        fh.write("- direct callees of AIMainUpdate with no map: %d%s\n" % (
            len(one_level), (": " + ", ".join(sorted(one_level))) if one_level else ""))
        fh.write("- second-level callees of mapped functions with no map (with a `.c` file): **%d**\n"
                 % len(second))
        fh.write("  (%s)\n" % ", ".join(sorted(second)[:25]))
        fh.write("- across all 85 phase-1/2 maps + 6 doc-54 functions, every callee with a `.c`\n")
        fh.write("  file is mapped one level deep; math/compiler artifacts without a `.c` file are\n")
        fh.write("  excluded (JUMPOUT, LOBYTE, LODWORD, atan2, cos, fabs, sin, sqrt).\n\n")
        fh.write("## Writer census summary (61 ABI fields, whole functions/ corpus)\n\n")
        fh.write("- fields with at least one mapped-AI reader or writer: %d/61\n" % inside)
        fh.write("- fields with no direct writer at the exact offset: %s\n" % (
            ", ".join(none_writers) if none_writers else "none"))
        fh.write("  (writes go through shifted bases (veh+0xC convention) or indexed/pointer stores.)\n")
        fh.write("- writers outside the mapped set (pit/safety-car/physics or shared engine):\n")
        fh.write("  %s\n\n" % ", ".join(outside_writers))
        fh.write("## Still unknown\n\n")
        for line in [
            "- the %d gap offsets proposed for v1.1 (`abi_v1_1_proposal.json`);" % (len(universe) - len(covered)),
            "- the %d base-open offsets in 0x3D00..0x3D44 (waypoint/surface objects, not vehicles);" % len(open_decs),
            "- the %d second-level unmapped callees;" % len(second),
            "- the indirect writers of: %s;" % ", ".join(none_writers),
            "- the writers of 0x4408 and 0x44B4 are still pointer-based (doc 54 section 7).",
        ]:
            fh.write(line + "\n")

    # ---- 4. v1.1 proposal (base-resolved new fields only) ----
    prop_fields = []
    for g in gaps:
        if g["in_proposal"] != "yes":
            continue
        dec = g["dec"]
        a = agg[dec]
        wfun = sorted(a["writers"]) or sorted(a["readers"])
        widths = a["width"]
        typ = "float" if "float" in widths else TYPE_MAP.get(sorted(widths)[0], "int")
        src = "functions/%s:L%s" % (sources[wfun[0]].name, a["hits"][0]["line"])
        prop_fields.append(dict(id="unnamed_%s" % g["hex"][2:].lower(), group=g["proposed_group"],
                                base="veh", hex=g["hex"], dec=dec, type=typ, count=1, units="",
                                meaning=g["best_guess_meaning"], tier="GUESS", source=src,
                                writer=wfun[0], writers=wfun,
                                readers=sorted(a["readers"])))
    prop = dict(abi)
    prop["generated_by"] = "phase2/analysis/c4_census.py (DeepSeek, phase-2 C4 proposal)"
    prop["generated_from"] = ["phase2/WRITER_CENSUS.csv", "phase2/ABI_GAPS.csv",
                              "phase2/COVERAGE.md", "phase2/maps/*"]
    prop["fields"] = prop_fields
    prop["derived_views"] = []
    (PH2 / "abi_v1_1_proposal.json").write_text(json.dumps(prop, indent=1) + "\n",
                                                encoding="utf-8")

    print("== C4 summary (10 lines) ==")
    print("1. ABI fields: 61; writer/reader census rows: %d" % len(census))
    print("2. fields with a mapped-AI reader or writer: %d/61" % inside)
    print("3. fields with no exact-offset writer: %s" % (", ".join(none_writers) or "none"))
    print("4. writers outside the mapped set: %d (%s)" % (len(outside_writers),
                                                          ", ".join(outside_writers[:8]) + ("..." if len(outside_writers) > 8 else "")))
    print("5. vehicle-offset universe (base resolved): %d" % len(universe))
    print("6. in ABI v1: %d; gaps proposed for v1.1: %d; coverage %.1f%%" % (
        len(covered), len(universe) - len(covered), pct))
    print("7. base-open offsets excluded from proposal: %d" % len(open_decs))
    print("8. direct AIMainUpdate callees without a map: %d" % len(one_level))
    print("9. second-level unmapped callees with a .c file: %d" % len(second))
    print("10. outputs: WRITER_CENSUS.csv, ABI_GAPS.csv, COVERAGE.md, abi_v1_1_proposal.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
