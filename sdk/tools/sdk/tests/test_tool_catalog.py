#!/usr/bin/env python3
"""Enforce complete, unambiguous LLM-facing tool selection."""
from pathlib import Path
import json
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools/f1c_mod.py")]
CATALOG_PATH = ROOT / "tools/sdk/F1C_TOOL_CATALOG.json"


def call(*args):
    return json.loads(subprocess.check_output(
        [*CLI, *args, "--json"], cwd=ROOT, text=True))


def fail(message):
    raise AssertionError(message)


def main():
    catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    if catalog.get("schema") != "f1c.tool_catalog.v1":
        fail("tool catalogue schema changed without a migration")

    capabilities = catalog["capabilities"]
    ids = [item["id"] for item in capabilities]
    if len(ids) != len(set(ids)):
        fail("capability ids are not unique")
    for item in capabilities:
        if not item["command"].startswith("python tools/f1c_mod.py"):
            fail(f"capability bypasses the front door: {item['id']}")
        for required in ("role", "status", "summary", "effects", "keywords"):
            if not item.get(required):
                fail(f"capability lacks {required}: {item['id']}")

    all_tools = call("capabilities", "--all")
    if all_tools["count"] < 100:
        fail("tool discovery unexpectedly lost most implementations")
    for item in all_tools["items"]:
        if not item.get("role") or not item.get("status") or not item.get("reason"):
            fail(f"tool is not fully labelled: {item.get('path')}")

    runner_ids = {item["id"] for item in catalog.get("runners", [])}
    if not runner_ids <= set(ids):
        fail("a runnable specialist lacks a public capability")
    for runner in catalog.get("runners", []):
        if not (ROOT / runner["path"]).is_file():
            fail(f"registered runner is missing: {runner['path']}")
        if set(runner["effects"]) & {"config-write", "live-write", "binary-write",
                                     "credential-write", "process-control"}:
            if not runner.get("risk_ack_required"):
                fail(f"state-changing runner lacks acknowledgement gate: {runner['id']}")

    recommendation = call(
        "recommend", "identify an unknown vehicle offset and check prior failed experiments")
    if recommendation["matches"][0]["id"] != "query-evidence":
        fail("semantic evidence task did not select the joined evidence query")

    old_input = call("tool-info", "tools/inject_key.py")
    if (old_input.get("status") != "superseded" or
            old_input.get("preferred_capability", {}).get("id") != "send-game-key"):
        fail("keyboard-tool precedence is ambiguous")
    routed_input = call("route", "tools/inject_key.py")
    if (routed_input.get("tool_selection", {}).get("status") != "superseded" or
            routed_input.get("preferred_capability", {}).get("id") != "send-game-key"):
        fail("path routing does not surface the workshop label and replacement")

    sdk_stats = call("sdk-query", "stats")
    if sdk_stats["functions"] < 6000 or sdk_stats["vehicle_fields"] < 90:
        fail("SDK query does not expose the canonical catalogue")

    print(f"Tool catalogue passed: {len(capabilities)} capabilities, "
          f"{all_tools['count']} labelled implementations.")


if __name__ == "__main__":
    main()
