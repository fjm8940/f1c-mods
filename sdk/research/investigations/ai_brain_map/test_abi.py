#!/usr/bin/env python
"""B8/C6 acceptance test: validate the AI Brain ABI registries.

- `ai_brain_abi_v1.json` against offsets_ALL.csv and doc 54 (B8 checks);
- `ai_brain_abi_v1_1.json` (C6) additionally against the phase-2 map CSVs and
  must contain every v1 field unchanged plus at least one new field, with
  `"version": "1.1"`.

Asserts (both files): every hex appears in the allowed CSVs/doc 54, ids are
unique, groups/bases/tiers are valid, position fields keep their meaning, and
the REVIEW_4 reader fix lists AIComputePassScore.
"""

from __future__ import annotations

import csv
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
ABI = HERE / "ai_brain_abi_v1.json"
ABI11 = HERE / "ai_brain_abi_v1_1.json"
CSV = HERE / "offsets_ALL.csv"
PH2 = HERE / "phase2" / "maps"
DOC54 = REPO / "docs" / "54-ai-driving-brain-map.md"

GROUPS = ["state", "perception", "plan_speed", "plan_line", "decision",
          "watchdog", "derived_views"]
BASES = {"veh", "wp", "opp_slot"}
HEX_RE = re.compile(r"0x[0-9A-Fa-f]+")
VIEW_READER_FIX = {"0x4500", "0x4504", "0x4580", "0x453C"}


def allowed_hexes(include_phase2: bool = False) -> set[str]:
    allowed: set[str] = set()
    with CSV.open(encoding="utf-8", newline="") as fh:
        for row in csv.DictReader(fh):
            if row["hex"]:
                allowed.add(row["hex"].upper())
    for token in HEX_RE.findall(DOC54.read_text(encoding="utf-8")):
        allowed.add(token.upper())
    if include_phase2:
        for path in sorted(PH2.glob("offsets_*.csv")):
            with path.open(encoding="utf-8", newline="") as fh:
                for row in csv.DictReader(fh):
                    if row["hex"]:
                        allowed.add(row["hex"].upper())
    return allowed


def check_abi(path: Path, version: str | None = None,
              include_phase2: bool = False) -> tuple[dict, int]:
    data = json.loads(path.read_text(encoding="utf-8"))
    assert data["schema"] == "f1c.ai_brain_abi.v1", "schema mismatch in %s" % path.name
    if version is not None:
        assert data.get("version") == version, "version mismatch in %s" % path.name
    allowed = allowed_hexes(include_phase2)

    fields = data["fields"]
    views = data["derived_views"]
    assert fields, "no fields"
    assert views, "no derived views"

    ids = [f["id"] for f in fields]
    ids += [v["id"] for v in views]
    assert len(ids) == len(set(ids)), "duplicate ids"

    for field in fields:
        for key in ("id", "group", "base", "hex", "dec", "type", "count",
                    "units", "meaning", "tier", "source", "writer",
                    "writers", "readers"):
            assert key in field, "field %s missing %s" % (field.get("id"), key)
        assert field["group"] in GROUPS, "bad group %s" % field["group"]
        assert field["group"] != "derived_views", "field in derived_views group"
        assert field["base"] in BASES, "bad base %s" % field["base"]
        assert field["tier"] in {"OBSERVED", "DERIVED", "GUESS"}, "bad tier"
        assert field["count"] >= 1
        if field["count"] > 1:
            assert "stride" in field and field["stride"] > 0, "array without stride"

    for view in views:
        assert view["group"] == "derived_views", "view group"
        for key in ("id", "formula", "inputs", "source", "tier"):
            assert key in view, "view %s missing %s" % (view.get("id"), key)

    text = json.dumps(data)
    for token in HEX_RE.findall(text):
        assert token.upper() in allowed, (
            "%s: hex %s not in offsets_ALL.csv/phase-2 CSVs or doc 54"
            % (path.name, token))

    by_id = {f["id"]: f for f in fields}
    assert by_id["position_x"]["hex"].lower() == "0x34e0", (
        "position_x must be the live world position 0x34E0")
    assert by_id["placement_pose_x"]["hex"].lower() == "0x34d4", (
        "placement_pose_x must be the path-walk pose 0x34D4")

    for field in fields:
        if field["hex"].upper().replace("0X", "0x") in VIEW_READER_FIX:
            assert "AIComputePassScore" in field["readers"], (
                "REVIEW_4 reader fix missing for %s" % field["hex"])

    return data, len(HEX_RE.findall(text))


def main() -> int:
    v1, n1 = check_abi(ABI)
    print("OK: %d fields, %d derived views, %d hex tokens checked"
          % (len(v1["fields"]), len(v1["derived_views"]), n1))

    if ABI11.exists():
        v11, n11 = check_abi(ABI11, version="1.1", include_phase2=True)
        before = {f["id"]: f for f in v1["fields"]}
        after = {f["id"]: f for f in v11["fields"]}
        for fid, field in before.items():
            assert fid in after, "v1.1 lost field %s" % fid
            assert after[fid]["hex"].lower() == field["hex"].lower(), (
                "v1.1 changed %s" % fid)
            assert after[fid]["dec"] == field["dec"], "v1.1 changed %s dec" % fid
        assert len(v11["fields"]) > len(v1["fields"]), "v1.1 adds no fields"
        print("OK v1.1: %d fields (+%d), %d derived views, %d hex tokens checked"
              % (len(v11["fields"]), len(v11["fields"]) - len(v1["fields"]),
                 len(v11["derived_views"]), n11))

    abi12 = HERE / "ai_brain_abi_v1_2.json"
    if ABI11.exists() and abi12.exists():
        v12, n12 = check_abi(abi12, version="1.2", include_phase2=True)
        before = {f["id"]: f for f in v11["fields"]}
        after = {f["id"]: f for f in v12["fields"]}
        assert set(before) == set(after), "v1.2 must keep exactly the v1.1 field set"
        for fid, field in before.items():
            assert after[fid]["hex"].lower() == field["hex"].lower(), "v1.2 changed %s" % fid
        print("OK v1.2: %d fields, %d hex tokens checked" % (len(v12["fields"]), n12))

    abi13 = HERE / "ai_brain_abi_v1_3.json"
    if abi12.exists() and abi13.exists():
        v13, n13 = check_abi(abi13, version="1.3", include_phase2=True)
        before = {f["id"]: f for f in v12["fields"]}
        after = {f["id"]: f for f in v13["fields"]}
        assert set(before) == set(after), "v1.3 must keep exactly the v1.2 field set"
        for fid, field in before.items():
            assert after[fid]["hex"].lower() == field["hex"].lower(), "v1.3 changed %s" % fid
        print("OK v1.3: %d fields, %d hex tokens checked" % (len(v13["fields"]), n13))

    abi14 = HERE / "ai_brain_abi_v1_4.json"
    if abi13.exists() and abi14.exists():
        v14, n14 = check_abi(abi14, version="1.4", include_phase2=True)
        before = {f["id"]: f for f in v13["fields"]}
        after = {f["id"]: f for f in v14["fields"]}
        assert set(before) == set(after), "v1.4 must keep exactly the v1.3 field set"
        for fid, field in before.items():
            assert after[fid]["hex"].lower() == field["hex"].lower(), "v1.4 changed %s" % fid
        print("OK v1.4: %d fields, %d hex tokens checked" % (len(v14["fields"]), n14))

    # v1.5 (REVIEW_18, 2026-10-03): value-type fixes only, read from the decompile (phase3/merge_abi_v1_5.py)
    abi15 = HERE / "ai_brain_abi_v1_5.json"
    if abi14.exists() and abi15.exists():
        v15, n15 = check_abi(abi15, version="1.5", include_phase2=True)
        before = {f["id"]: f for f in v14["fields"]}
        after = {f["id"]: f for f in v15["fields"]}
        assert set(before) == set(after), "v1.5 must keep exactly the v1.4 field set"
        retyped = {"0x441c": "float", "0x4450": "float", "0x4528": "float", "0x452c": "float", "0x4590": "int"}
        for fid, field in before.items():
            hx = field["hex"].lower()
            assert after[fid]["hex"].lower() == hx, "v1.5 changed %s" % fid
            assert after[fid]["tier"] == field["tier"], "v1.5 changed the tier of %s" % fid
            want = retyped.get(hx, field["type"])
            assert after[fid]["type"] == want, "v1.5 %s type %s, expected %s" % (fid, after[fid]["type"], want)
        print("OK v1.5: %d fields, %d retyped, %d hex tokens checked" % (len(v15["fields"]), len(retyped), n15))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print("FAIL: %s" % exc)
        raise SystemExit(1)
