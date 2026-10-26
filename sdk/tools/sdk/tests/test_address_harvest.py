#!/usr/bin/env python3
"""address-harvest regression: extraction, aggregation, refusal (fixture-based)."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools" / "f1c_mod.py")]
sys.path.insert(0, str(ROOT / "tools"))
import address_harvest  # noqa: E402


def fail(message):
    raise SystemExit(message)


def find(entries, kind, value):
    return [entry for entry in entries
            if entry["kind"] == kind and entry["value"] == value]


def build_fixture(workspace):
    sdk_root = workspace / "f1c_ida_decompiled"
    sdk = sdk_root / "tools" / "sdk"
    sdk.mkdir(parents=True)
    product = workspace / "development" / "mods"
    product.mkdir(parents=True)

    (sdk_root / "name_index.json").write_text(json.dumps({
        "vehicle_fields": {"0x4620": {"name": "FixtureDifferentName", "trust": "FACT"}},
    }), encoding="utf-8")
    (sdk / "f1c_sdk_overrides.json").write_text(json.dumps({
        "functions": {"FixtureNativeClear": {"address": "0x004C6E80", "trust": "CONFIRMED"}},
    }), encoding="utf-8")
    (sdk / "f1c_fixture_header.h").write_text(
        "#define OFF_NAME 0x4620 /* veh driver name */\n"
        "#define NAME_CLEAR 0x004C6E80\n"
        "    sub_4C6E80();\n",
        encoding="utf-8")
    # The build directory must be skipped by the path rules.
    build = sdk_root / "mods" / "sdk_sandbox" / "build"
    build.mkdir(parents=True)
    (build / "skip_me.c").write_text("uintptr_t skipped = 0x004ABCDE;\n", encoding="utf-8")
    (product / "fixture_product.c").write_text(
        "x = 0x004C6E80;\n"
        "#define OTHER_VALUE 0x4620 // unrelated\n"
        "// dword_7C54F0 observed\n",
        encoding="utf-8")
    return sdk_root


def main():
    workspace = Path(tempfile.mkdtemp())
    cli_parent = Path(tempfile.mkdtemp())
    try:
        sdk_root = build_fixture(workspace)
        result = address_harvest.harvest(sdk_root)
        entries = result["entries"]

        offsets = find(entries, "vehicle_offset", "0x4620")
        if len(offsets) != 1:
            fail(f"expected one vehicle_offset 0x4620, found {len(offsets)}")
        offset = offsets[0]
        if "OFF_NAME" not in offset["names"]:
            fail(f"vehicle_offset 0x4620 names lack OFF_NAME: {offset['names']}")
        if offset["hits"] != 1 or offset["files"] != 1:
            fail(f"non-keyword 0x4620 line was harvested: hits={offset['hits']} "
                 f"files={offset['files']}")
        if not offset["name_conflict"]:
            fail("vehicle_offset 0x4620 must conflict with the differing fixture index name")
        if not offset["index"] or offset["index"].get("name") != "FixtureDifferentName":
            fail(f"vehicle_offset 0x4620 index join wrong: {offset['index']}")

        codes = find(entries, "code", "0x004C6E80")
        if len(codes) != 1:
            fail(f"0x004C6E80 and sub_4C6E80 did not merge: {len(codes)} entries")
        code = codes[0]
        if code["hits"] != 3 or code["files"] != 2:
            fail(f"code 0x004C6E80 aggregation wrong: hits={code['hits']} "
                 f"files={code['files']}")
        if not code["overrides"] or code["overrides"].get("name") != "FixtureNativeClear":
            fail(f"code 0x004C6E80 override join wrong: {code['overrides']}")

        data = find(entries, "data", "0x007C54F0")
        if len(data) != 1:
            fail("dword_7C54F0 did not become data 0x007C54F0")

        if find(entries, "code", "0x004ABCDE"):
            fail("a path under /build/ was not skipped")

        # CLI: one run writes both files, a second run with the same output is refused.
        output = cli_parent / "harvest_out"
        first = subprocess.run([*CLI, "address-harvest", "--output", str(output), "--json"],
                               cwd=ROOT, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
        if first.returncode != 0:
            fail(f"address-harvest failed: {first.stderr.strip()}")
        counts = json.loads(first.stdout)
        if set(counts) != {"code", "data", "vehicle_offset"}:
            fail(f"counts keys wrong: {sorted(counts)}")
        if not (output / "harvest.jsonl").is_file() or not (output / "SUMMARY.md").is_file():
            fail("address-harvest did not write harvest.jsonl and SUMMARY.md")
        second = subprocess.run([*CLI, "address-harvest", "--output", str(output)],
                                cwd=ROOT, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        if second.returncode == 0:
            fail("a second address-harvest with the same --output must be refused")
        if "already exists" not in second.stderr:
            fail(f"refusal message unexpected: {second.stderr.strip()}")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)
        shutil.rmtree(cli_parent, ignore_errors=True)

    print("Address harvest passed: extraction, aggregation, index/override join, "
          "conflict, build skip, and output refusal verified.")


if __name__ == "__main__":
    main()
