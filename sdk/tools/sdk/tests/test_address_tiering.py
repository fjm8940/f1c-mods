#!/usr/bin/env python3
"""address-tier regression: rules R0-R6, names, review flags, refusal (fixture-based)."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools" / "f1c_mod.py")]
sys.path.insert(0, str(ROOT / "tools"))
import address_tiering  # noqa: E402


def fail(message):
    raise SystemExit(message)


def find(records, value):
    return [record for record in records if record["value"] == value]


def require(records, value, rule, tier):
    matches = find(records, value)
    if len(matches) != 1:
        fail(f"expected one record for {value}, found {len(matches)}")
    record = matches[0]
    if record["rule_id"] != rule:
        fail(f"{value}: expected rule {rule}, got {record['rule_id']}")
    if record["proposed_tier"] != tier:
        fail(f"{value}: expected tier {tier}, got {record['proposed_tier']}")
    return record


def build_fixture():
    workspace = Path(tempfile.mkdtemp())
    sdk_root = workspace / "f1c_ida_decompiled"
    sdk = sdk_root / "tools" / "sdk"
    tests = sdk / "tests"
    functions = sdk_root / "functions"
    analysis = sdk_root / "analysis"
    harvest_dir = workspace / "tier_input"
    for path in (sdk, tests, functions, analysis, harvest_dir):
        path.mkdir(parents=True)

    (analysis / "recovery_index.json").write_text(json.dumps({
        "functions": {"0x004C6E40": {"name": "FixtureNativeClear"},
                      "0x004C6E80": {"name": "FixtureDerivedFn"}},
    }), encoding="utf-8")
    (sdk_root / "findings.jsonl").write_text(
        "// fixture findings\n"
        + json.dumps({"id": "fixture-r4", "tier": "CONFIRMED",
                      "claim": "Pit slot offset 0x4040 is confirmed",
                      "evidence": "fixture", "source": "fixture"}) + "\n"
        + json.dumps({"id": "fixture-unproven", "tier": "DERIVED",
                      "claim": "0x4040 was seen once",
                      "evidence": "fixture", "source": "fixture"}) + "\n",
        encoding="utf-8")
    (sdk / "fixture_fingerprinted.hpp").write_text(
        "// fixture fingerprint header\n"
        "F1C_FIXTURE_NATIVE = 0x004C6E40;\n",
        encoding="utf-8")
    (tests / "test_fixture_fingerprinted.py").write_text(
        "print('fixture_fingerprinted')\n", encoding="utf-8")
    (sdk / "fixture_plain.h").write_text(
        "#define OFF_PIT_STATE 0x4500\n"
        "#define F1C_FIXTURE_GLOBAL 0x007C54F0\n"
        "#define OFF_PIT_SOMETHING 0x4040\n",
        encoding="utf-8")
    (functions / "fixture_corpus.c").write_text(
        "x = dword_7C54F0;\ny = dword_7C0000;\n", encoding="utf-8")

    entries = [
        {"kind": "vehicle_offset", "value": "0x1000", "names": ["KnownOffset"],
         "hits": 2, "files": 1,
         "contexts": [{"file": "f1c_ida_decompiled/tools/sdk/fixture_plain.h",
                       "line": 1, "text": "#define OFF_PIT_STATE 0x4500"}],
         "index": {"name": "KnownOffset", "trust": "CONFIRMED", "store": "name_index"},
         "overrides": None, "name_conflict": False},
        {"kind": "data", "value": "0x00800000", "names": ["ProvenGlobal"],
         "hits": 1, "files": 1, "contexts": [],
         "index": {"name": "ProvenGlobal", "trust": "FACT", "store": "name_index"},
         "overrides": None, "name_conflict": False},
        {"kind": "code", "value": "0x005BF000", "names": ["PossiblyData"],
         "hits": 3, "files": 2, "contexts": [],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "code", "value": "0x004C6E40", "names": ["F1C_FIXTURE_NATIVE"],
         "hits": 1, "files": 1,
         "contexts": [{"file": "f1c_ida_decompiled/tools/sdk/fixture_fingerprinted.hpp",
                       "line": 2, "text": "F1C_FIXTURE_NATIVE = 0x004C6E40;"}],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "data", "value": "0x007C54F0", "names": ["F1C_FIXTURE_GLOBAL"],
         "hits": 1, "files": 1,
         "contexts": [{"file": "f1c_ida_decompiled/tools/sdk/fixture_plain.h",
                       "line": 2, "text": "#define F1C_FIXTURE_GLOBAL 0x007C54F0"}],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "vehicle_offset", "value": "0x4040", "names": ["OFF_PIT_SOMETHING"],
         "hits": 1, "files": 1,
         "contexts": [{"file": "f1c_ida_decompiled/tools/sdk/fixture_plain.h",
                       "line": 3, "text": "#define OFF_PIT_SOMETHING 0x4040"}],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "vehicle_offset", "value": "0x3000", "names": ["OFF_MULTI"],
         "hits": 2, "files": 2,
         "contexts": [{"file": "development/mods/red_flag/fixture.c",
                       "line": 1, "text": "#define OFF_MULTI 0x3000"},
                      {"file": "development/mods/skidmod/fixture.c",
                       "line": 2, "text": "OFF_MULTI = 0x3000;"}],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "code", "value": "0x004C6E80", "names": ["FixtureDerivedFn"],
         "hits": 1, "files": 1, "contexts": [],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "data", "value": "0x007C0000", "names": [],
         "hits": 1, "files": 1, "contexts": [],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "vehicle_offset", "value": "0x4500", "names": ["OFF_PIT_STATE"],
         "hits": 1, "files": 1,
         "contexts": [{"file": "f1c_ida_decompiled/tools/sdk/fixture_plain.h",
                       "line": 1, "text": "#define OFF_PIT_STATE 0x4500"}],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "code", "value": "0x00401234", "names": ["mystery"],
         "hits": 1, "files": 1,
         "contexts": [{"file": "f1c_ida_decompiled/tools/sdk/f1c_sdk.h",
                       "line": 9, "text": "generated reference 0x00401234"}],
         "index": None, "overrides": None, "name_conflict": False},
        {"kind": "vehicle_offset", "value": "0x2000", "names": ["LocalName"],
         "hits": 1, "files": 1, "contexts": [],
         "index": {"name": "DifferentIndexName", "trust": "DERIVED", "store": "name_index"},
         "overrides": None, "name_conflict": True},
    ]
    with (harvest_dir / "harvest.jsonl").open("w", encoding="utf-8", newline="\n") as handle:
        for entry in entries:
            handle.write(json.dumps(entry) + "\n")
    return workspace, sdk_root, harvest_dir


def verify_rules(records):
    kept = require(records, "0x1000", "R0", "CONFIRMED")
    if kept["needs_review"]:
        fail("R0 entry must not need review without a conflict")
    fact = require(records, "0x00800000", "R0", "FACT")
    if fact["existing"]["trust"] != "FACT" or fact["proposed_name"] != "ProvenGlobal":
        fail(f"R0 keep damaged the FACT entry: {fact['existing']}")
    r1 = require(records, "0x005BF000", "R1", "GUESS")
    if r1["reclassify"] != "data?":
        fail(f"R1 must reclassify as data?: {r1['reclassify']}")
    r2 = require(records, "0x004C6E40", "R2", "CONFIRMED_PROPOSED")
    if r2["evidence"]["function_start"] is not True:
        fail("R2 entry must have function_start true")
    if r2["proposed_name"] != "FixtureNative":
        fail(f"R2 name normalization wrong: {r2['proposed_name']}")
    r3 = require(records, "0x007C54F0", "R3", "CONFIRMED_PROPOSED")
    if r3["evidence"]["raw_corpus"] < 1 or r3["proposed_name"] != "FixtureGlobal":
        fail(f"R3 evidence/name wrong: {r3['evidence']} {r3['proposed_name']}")
    r4 = require(records, "0x4040", "R4", "CONFIRMED_PROPOSED")
    if r4["proposed_name"] != "PitSomething":
        fail(f"R4 name normalization wrong: {r4['proposed_name']}")
    r5 = require(records, "0x3000", "R5", "DERIVED")
    if r5["evidence"]["products"] != ["red_flag", "skidmod"]:
        fail(f"R5 products wrong: {r5['evidence']['products']}")
    if r5["needs_review"]:
        fail("R5 with a name and no conflict must not need review")
    require(records, "0x004C6E80", "R5", "DERIVED")
    r5_null = require(records, "0x007C0000", "R5", "DERIVED")
    if r5_null["proposed_name"] is not None or not r5_null["needs_review"]:
        fail("R5 with a null name must need review")
    name_test = require(records, "0x4500", "R5", "DERIVED")
    if name_test["proposed_name"] != "PitState" or name_test["alt_names"] != ["OFF_PIT_STATE"]:
        fail(f"name normalisation wrong: {name_test['proposed_name']} {name_test['alt_names']}")
    r6 = require(records, "0x00401234", "R6", "GUESS")
    if r6["evidence"]["sdk_headers"]:
        fail("the generated header exclusion did not work")
    r6 = require(records, "0x2000", "R6", "GUESS")
    if not r6["needs_review"] or r6["proposed_name"] != "DifferentIndexName":
        fail(f"conflict handling wrong: {r6['needs_review']} {r6['proposed_name']}")
    for record in records:
        if record["proposed_tier"] == "FACT" and record["rule_id"] != "R0":
            fail(f"FACT must never be proposed by a rule: {record['value']} {record['rule_id']}")
    for rule in ("R1", "R2", "R3", "R4"):
        for record in records:
            if record["rule_id"] == rule and not record["needs_review"]:
                fail(f"{rule} entry {record['value']} must need review")


def main():
    workspace, sdk_root, harvest_dir = build_fixture()
    try:
        result = address_tiering.propose(sdk_root, harvest_dir)
        verify_rules(result["records"])
        if result["needs_review"] < 1 or result["total"] != 12:
            fail(f"counts wrong: needs_review={result['needs_review']} total={result['total']}")

        first = subprocess.run([*CLI, "address-tier", "--harvest", str(harvest_dir), "--json"],
                               cwd=ROOT, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
        if first.returncode != 0:
            fail(f"address-tier failed: {first.stderr.strip()}")
        summary = json.loads(first.stdout)
        if set(summary) != {"counts", "needs_review", "total"}:
            fail(f"address-tier --json keys wrong: {sorted(summary)}")
        if not (harvest_dir / "tier_proposals.jsonl").is_file() or not (harvest_dir / "REVIEW.md").is_file():
            fail("address-tier did not write tier_proposals.jsonl and REVIEW.md")
        review = (harvest_dir / "REVIEW.md").read_text(encoding="utf-8")
        if "CONFIRMED_PROPOSED" not in review or "Name conflicts (1)" not in review:
            fail("REVIEW.md is missing its required tables")
        second = subprocess.run([*CLI, "address-tier", "--harvest", str(harvest_dir)],
                                cwd=ROOT, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        if second.returncode == 0:
            fail("a second address-tier with the same --harvest must be refused")
        if "already exists" not in second.stderr:
            fail(f"refusal message unexpected: {second.stderr.strip()}")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    print("Address tiering passed: rules R0-R6, no proposed FACT, name normalisation, "
          "review flags, and output refusal verified.")


if __name__ == "__main__":
    main()
