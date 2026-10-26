#!/usr/bin/env python3
"""The sole supported front door for F1 Challenge mod-SDK work."""
from pathlib import Path
import argparse
import ast
import base64
import fnmatch
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

import address_harvest
import address_tiering
import source_explorer
from source_explorer import (SourceExplorerError, attach_node_lookup,
                             inspect_function, json_text, render_inspection,
                             render_search, sdk_links, search_sources)
from semantic_memory_linker import (LinkerError, build_index, correlate_event,
                                    front_wing_report, load_signature,
                                    match_capture, render_correlation,
                                    render_fingerprint,
                                    signature_from_fingerprint)
from process_memory_snapshot import (SnapshotError, capture_snapshot,
                                     diff_snapshots)
from variant_resolver import (PEImage as VariantPEImage, ResolverError,
                              bootstrap_f1c_recipes, resolve_recipes)
from runtime_validator import (ValidationError, correlate_live, load_recipe, observe_live, read_live_fields,
                               render_report as render_runtime_report,
                               validate_observation)

ROOT = Path(__file__).resolve().parents[1]
SANDBOX = ROOT / "mods" / "sdk_sandbox"
SDK = ROOT / "tools" / "sdk"
RECOVERY = ROOT / "analysis" / "recovery_index.json"
RUNTIME_RECIPES = ROOT / "analysis" / "runtime_validation_recipes.json"
GAME_DIR = ROOT.parent / "F1 Challenge V10"
DEFAULT_EXE = GAME_DIR / "F1 Challenge Online.exe"
DEFAULT_IDB = GAME_DIR / "F1 Challenge Online.exe.i64"
IDAT = Path(r"C:\Program Files\IDA Professional 9.1\idat.exe")
TARGET_SHA256 = "1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003"
PROTECTED_NAMES = {"pace_probe.dll"}
TOOL_CONTRACT = SDK / "F1C_TOOL_CONTRACT.json"
TOOL_CATALOG = SDK / "F1C_TOOL_CATALOG.json"

class ModError(RuntimeError):
    pass


def load_tool_contract():
    if not TOOL_CONTRACT.is_file():
        raise ModError(f"canonical tool contract not found: {TOOL_CONTRACT}")
    return json.loads(TOOL_CONTRACT.read_text(encoding="utf-8"))


def load_tool_catalog():
    if not TOOL_CATALOG.is_file():
        raise ModError(f"canonical tool catalogue not found: {TOOL_CATALOG}")
    catalogue = json.loads(TOOL_CATALOG.read_text(encoding="utf-8"))
    if catalogue.get("schema") != "f1c.tool_catalog.v1":
        raise ModError("canonical tool catalogue has an unsupported schema")
    return catalogue


def capability_map(catalogue=None):
    catalogue = catalogue or load_tool_catalog()
    return {item["id"]: item for item in catalogue["capabilities"]}


def _matches_any(relative, patterns):
    return any(fnmatch.fnmatch(relative, pattern) for pattern in patterns)


def discover_tool_paths(catalogue=None):
    """Return every executable-like project tool covered by the catalogue."""
    catalogue = catalogue or load_tool_catalog()
    extensions = {item.lower() for item in catalogue["discovery"]["extensions"]}
    excludes = catalogue["discovery"].get("exclude_globs", [])
    paths = set()
    for path in ROOT.iterdir():
        if path.is_file() and path.suffix.lower() in extensions:
            paths.add(path.relative_to(ROOT).as_posix())
    for base_name in ("tools", "mods"):
        base = ROOT / base_name
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in extensions:
                continue
            relative = path.relative_to(ROOT).as_posix()
            if not _matches_any(relative, excludes):
                paths.add(relative)
    return sorted(paths)


def _python_doc_summary(relative):
    path = ROOT / relative
    if path.suffix.lower() != ".py" or not path.is_file():
        return None
    try:
        tree = ast.parse(path.read_text(encoding="utf-8", errors="replace"))
        doc = ast.get_docstring(tree)
    except (OSError, SyntaxError):
        return None
    if not doc:
        return None
    return " ".join(line.strip() for line in doc.splitlines()[:3])[:500]


def classify_tool_implementation(relative, catalogue=None):
    catalogue = catalogue or load_tool_catalog()
    relative = Path(relative).as_posix().lstrip("./")
    for item in catalogue.get("implementation_overrides", []):
        if item["path"] == relative:
            return {"path": relative, **item,
                    "description": _python_doc_summary(relative) or item["reason"]}
    for rule in catalogue.get("implementation_rules", []):
        if fnmatch.fnmatch(relative, rule["glob"]):
            return {"path": relative, **rule,
                    "description": _python_doc_summary(relative) or rule["reason"]}
    raise ModError(f"tool implementation is not classified: {relative}")


def cmd_capabilities(args):
    catalogue = load_tool_catalog()
    if args.all:
        records = [classify_tool_implementation(path, catalogue)
                   for path in discover_tool_paths(catalogue)]
    else:
        records = list(catalogue["capabilities"])
    if args.role:
        records = [item for item in records if item.get("role") == args.role]
    if args.status:
        records = [item for item in records if item.get("status") == args.status]
    report = {
        "schema": "f1c.capability_list.v1",
        "selection_rule": catalogue["selection_rule"],
        "view": "implementations" if args.all else "capabilities",
        "count": len(records),
        "items": records,
    }
    if args.json:
        print(json.dumps(report, indent=2))
        return
    print(catalogue["selection_rule"])
    for item in records:
        identity = item.get("id", item.get("path"))
        summary = item.get("summary", item.get("reason", ""))
        print(f"{identity}: {item.get('role')} / {item.get('status')} — {summary}")
        if item.get("command"):
            print("  ", item["command"])


def cmd_tool_info(args):
    catalogue = load_tool_catalog()
    capabilities = capability_map(catalogue)
    target = args.target.strip()
    if target in capabilities:
        report = {"schema": "f1c.tool_info.v1", "kind": "capability",
                  **capabilities[target]}
    else:
        normalized = Path(target).as_posix().lstrip("./")
        candidates = [item for item in discover_tool_paths(catalogue)
                      if item == normalized or Path(item).name == target]
        candidates += [item["path"] for item in catalogue.get("implementation_overrides", [])
                       if item["path"] == normalized and item["path"] not in candidates]
        candidates = sorted(set(candidates))
        if len(candidates) != 1:
            raise ModError(f"tool must resolve uniquely by capability id, path, or basename: "
                           f"{target} ({len(candidates)} matches)")
        implementation = classify_tool_implementation(candidates[0], catalogue)
        report = {"schema": "f1c.tool_info.v1", "kind": "implementation",
                  **implementation}
        owner = capabilities.get(implementation.get("capability"))
        if owner:
            report["preferred_capability"] = owner
    print(json.dumps(report, indent=2) if args.json else
          "\n".join(f"{key}: {value}" for key, value in report.items()))


def _recommendation_score(capability, query):
    text = query.casefold()
    tokens = set(re.findall(r"[a-z0-9]+", text))
    score = 0
    for keyword in capability.get("keywords", []):
        key = keyword.casefold()
        key_tokens = set(re.findall(r"[a-z0-9]+", key))
        phrase_hit = key in text if len(key_tokens) > 1 else bool(key_tokens & tokens)
        if phrase_hit:
            score += 8 + len(key.split())
        score += 2 * len(tokens & key_tokens)
    haystack = f"{capability['id']} {capability.get('summary', '')}".casefold()
    score += len(tokens & set(re.findall(r"[a-z0-9]+", haystack)))
    if capability.get("status") == "preferred":
        score += 1
    return score


def cmd_recommend(args):
    catalogue = load_tool_catalog()
    ranked = sorted(((_recommendation_score(item, args.task), item)
                     for item in catalogue["capabilities"]),
                    key=lambda pair: (-pair[0], pair[1]["id"]))
    matches = [{"score": score, **item} for score, item in ranked[:args.limit] if score > 0]
    if not matches:
        matches = [{"score": 0, **capability_map(catalogue)["list-tools"]}]
    report = {"schema": "f1c.tool_recommendation.v1", "task": args.task,
              "selection_rule": catalogue["selection_rule"], "matches": matches}
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        for item in matches:
            print(f"{item['id']} [{item['status']}] — {item['summary']}")
            print("  ", item["command"])


def cmd_tool_run(args):
    catalogue = load_tool_catalog()
    runners = {item["id"]: item for item in catalogue.get("runners", [])}
    runner = runners.get(args.tool_id)
    if not runner:
        raise ModError(f"tool is not runnable through the public front door: {args.tool_id}")
    if runner.get("risk_ack_required") and not args.acknowledge_risk:
        raise ModError("this specialist can change game/config state; inspect it with tool-info "
                       "and repeat with --acknowledge-risk")
    path = (ROOT / runner["path"]).resolve()
    if not path.is_file() or not is_within(path, ROOT):
        raise ModError(f"registered tool implementation is missing or outside the repository: {path}")
    tool_args = list(args.tool_args)
    if tool_args[:1] == ["--"]:
        tool_args = tool_args[1:]
    run([sys.executable, path, *tool_args])


def cmd_evidence_query(args):
    as_json = getattr(args, "json", False)
    if as_json and args.kind != "findings":
        raise ModError("--json is supported only for evidence-query findings")
    if args.kind == "stats":
        if args.value:
            raise ModError("evidence-query stats takes no value")
        command = [sys.executable, ROOT / "tools" / "ask.py", "stats"]
    else:
        if not args.value:
            raise ModError(f"evidence-query {args.kind} requires a value")
        command = [sys.executable, ROOT / "tools" / "ask.py", args.kind, args.value]
    if as_json:
        command.append("--json")
    run(command, quiet=True)


def cmd_sdk_query(args):
    catalogue = load_catalogue()
    if args.kind == "stats":
        report = {
            "schema": "f1c.sdk_query.v1",
            "kind": "stats",
            "functions": len(catalogue.get("functions", {})),
            "vehicle_fields": len(catalogue.get("vehicle_fields", {})),
            "legacy_structs": len(catalogue.get("legacy_structs", {})),
            "vtables": len(catalogue.get("vtables", {})),
        }
    else:
        if not args.query:
            raise ModError(f"sdk-query {args.kind} requires a query")
        source_key = "vehicle_fields" if args.kind == "field" else "functions"
        needle = args.query.casefold()
        all_matches = []
        for name, metadata in catalogue.get(source_key, {}).items():
            searchable = f"{name} {json.dumps(metadata, sort_keys=True)}".casefold()
            if needle not in searchable:
                continue
            if args.trust and str(metadata.get("trust", "")).upper() != args.trust:
                continue
            all_matches.append({"name": name, **metadata})
        total = len(all_matches)
        matches = all_matches[:args.limit]
        report = {"schema": "f1c.sdk_query.v1", "kind": args.kind,
                  "query": args.query, "matches": matches, "count": len(matches),
                  "total": total, "returned": len(matches),
                  "truncated": total > len(matches)}
        if not matches:
            raise ModError("no canonical SDK entries matched the query")
    if args.json:
        print(json.dumps(report, indent=2))
        return
    if args.kind == "stats":
        for key, value in report.items():
            if key != "schema":
                print(f"{key}: {value}")
        return
    for item in report["matches"]:
        location = item.get("offset", item.get("address", "-"))
        print(f"{item['name']}: {location} [{item.get('trust', 'UNKNOWN')}]")
    if report.get("truncated"):
        print(f"showing {report['returned']} of {report['total']}")


def classify_project_path(value, contract=None):
    """Classify a repository path without opening or modifying it."""
    contract = contract or load_tool_contract()
    candidate = Path(value)
    if candidate.is_absolute():
        try:
            relative = candidate.resolve().relative_to(ROOT.resolve())
        except ValueError:
            return "external", "Outside this repository; do not treat it as SDK evidence."
    else:
        relative = candidate
    normalized = relative.as_posix().lstrip("./")
    for rule in contract["routing_rules"]:
        prefix = rule["prefix"].rstrip("/")
        if normalized == prefix or normalized.startswith(prefix + "/"):
            return rule["classification"], rule["instruction"]
    return "project", contract["default_instruction"]


BROKER_ABI_HEADER = SDK / "f1c_broker_abi.h"


def header_broker_abi():
    text = BROKER_ABI_HEADER.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"#define\s+F1C_BROKER_ABI_VERSION\s+(\d+)u?\b", text)
    return int(match.group(1)) if match else None


def status_conflicts(checkpoint):
    conflicts = []
    header = header_broker_abi()
    recorded = checkpoint.get("broker_abi_version")
    if header is not None and recorded is not None and int(recorded) != header:
        conflicts.append({
            "field": "broker_abi",
            "authoritative": header,
            "authoritative_source": "tools/sdk/f1c_broker_abi.h",
            "stale_value": recorded,
            "stale_source": "tools/sdk/AUTOMATION_CHECKPOINT.json:broker_abi_version"})
    return conflicts


def cmd_sdk_info(args):
    contract = load_tool_contract()
    tool_catalogue = load_tool_catalog()
    checkpoint_path = SDK / "AUTOMATION_CHECKPOINT.json"
    checkpoint = (json.loads(checkpoint_path.read_text(encoding="utf-8"))
                  if checkpoint_path.is_file() else {})
    catalogue = load_catalogue()
    freshness, recent_entries = checkpoint_freshness(checkpoint)
    facts = {
        "schema": "f1c.sdk_info.v1",
        "front_door": contract["front_door"],
        "sdk_version": contract["sdk_version"],
        "broker_abi": header_broker_abi(),
        "broker_abi_source": "tools/sdk/f1c_broker_abi.h",
        "checkpoint_broker_abi": checkpoint.get("broker_abi_version"),
        "conflicts": status_conflicts(checkpoint),
        "freshness": freshness,
        "recent_checkpoint_entries": recent_entries,
        "catalogue_functions": len(catalogue.get("functions", {})),
        "catalogue_vehicle_fields": len(catalogue.get("vehicle_fields", {})),
        "active_wave": checkpoint.get("active_wave"),
        "state": checkpoint.get("state"),
        "next_objective": checkpoint.get("next_objective", []),
        "contract": str(TOOL_CONTRACT.relative_to(ROOT)),
        "llm_guide": contract["llm_guide"],
        "archive_index": contract["archive_index"],
        "tool_catalog": str(TOOL_CATALOG.relative_to(ROOT)).replace("\\", "/"),
        "tool_workshop": "tools/sdk/TOOL_WORKSHOP.md",
        "preferred_capabilities": sum(
            item.get("status") == "preferred" for item in tool_catalogue["capabilities"]),
        "commands": sorted(build_parser(include_info=True)._subparsers._group_actions[0].choices),
    }
    if args.json:
        print(json.dumps(facts, indent=2))
        return
    print(f"F1C Mod SDK {facts['sdk_version']} | broker ABI {facts['broker_abi']}")
    for conflict in facts["conflicts"]:
        print(f"WARNING: {conflict['field']} {conflict['stale_source']}={conflict['stale_value']} "
              f"but {conflict['authoritative_source']}={conflict['authoritative']}")
    print("Sole supported entry point:", facts["front_door"])
    print("State:", facts["state"] or "unknown")
    print("LLM guide:", facts["llm_guide"])
    print("Tool workshop:", facts["tool_workshop"])
    print("Machine catalogue:", facts["tool_catalog"])
    print("Choose a tool: python tools/f1c_mod.py recommend \"TASK\"")
    print("Use `python tools/f1c_mod.py route PATH` before acting on an unfamiliar file.")


DATED_KEY = re.compile(r"(20\d{2})_?(\d{2})_?(\d{2})$")


def checkpoint_freshness(checkpoint, recent=5):
    dated = []
    for key, value in checkpoint.items():
        match = DATED_KEY.search(key)
        if match and isinstance(value, dict):
            dated.append(("-".join(match.groups()), key, value))
    dated.sort(key=lambda item: (item[0], item[1]), reverse=True)
    updated = str(checkpoint.get("updated_at", ""))[:10] or None
    newest = dated[0][0] if dated else None
    stale = bool(updated and newest and newest > updated)
    result = {
        "updated_at": checkpoint.get("updated_at"),
        "newest_entry_key": dated[0][1] if dated else None,
        "newest_entry_date": newest,
        "dated_entries": len(dated),
        "stale": stale,
        "warning": ("Checkpoint summary fields (state, next_objective, broker_abi_version) "
                    "predate newer dated entries; treat next objectives as possibly stale "
                    "and read recent_checkpoint_entries.") if stale else None,
    }
    entries = []
    for date, key, value in dated[:recent]:
        entries.append({
            "key": key, "date": date,
            "state": str(value.get("state") or value.get("status") or "")[:300],
            "pending": str(value.get("pending") or "")[:300],
            "evidence": str(value.get("evidence") or "")[:200]})
    return result, entries


def cmd_start(args):
    """Return the complete cold-start briefing for an autonomous agent."""
    contract = load_tool_contract()
    checkpoint = json.loads((SDK / "AUTOMATION_CHECKPOINT.json").read_text(encoding="utf-8"))
    freshness, recent_entries = checkpoint_freshness(checkpoint)
    briefing = {
        "schema": "f1c.agent_start.v1",
        "working_directory": str(ROOT),
        "goal": "anything_is_possible_mod_sdk",
        "front_door": contract["front_door"],
        "state": checkpoint.get("state"),
        "active_wave": checkpoint.get("active_wave"),
        "next_objectives_in_priority_order": checkpoint.get("next_objective", []),
        "freshness": freshness,
        "recent_checkpoint_entries": recent_entries,
        "decision_rule": (
            "Execute the user's explicit SDK task; when asked to continue without a task, "
            "take the first unfinished checkpoint objective and pass tests before advancing. "
            "If freshness.stale is true, first compare that objective with "
            "recent_checkpoint_entries and report objectives already completed there "
            "instead of repeating them."),
        "evidence_rules": [
            "Query prior art before re-deriving: evidence-query findings TEXT --json.",
            "RECORDED findings are copied checkpoint summaries, not tier-reviewed; "
            "verify through their evidence link before relying on them.",
            "inspect sdk_links are navigation only; they never raise a trust tier.",
            "Append each new proven, disproven or open result to findings.jsonl with "
            "tier, evidence and source; prose-only results are not discoverable."
        ],
        "workflow": {
            "orient": ["sdk-info --json", "doctor", "route PATH"],
            "tool_selection": ["recommend TASK --json", "capabilities --json", "tool-info TOOL --json"],
            "research": ["search QUERY", "inspect ADDRESS_OR_NAME", "discover MOD_ID"],
            "semantic_evidence": ["evidence-query findings TEXT --json",
                                  "evidence-query KIND VALUE", "sdk-query KIND QUERY --json"],
            "new_mod": ["init MOD_ID", "pipeline MOD_ID", "stage MOD_ID DESTINATION"],
            "runtime_evidence": ["runtime-observe", "runtime-validate", "memory-snapshot", "memory-diff"],
            "variant_support": ["variant-profile", "symbol-recipes-build", "resolve-symbols"],
            "sdk_maintenance": ["sdk-refresh", "sdk-test --suite tooling",
                                "sdk-test --no-regenerate", "sdk-test"],
        },
        "non_negotiable": contract["prohibitions"],
        "definition_of_done": [
            "The requested capability is exposed through tools/f1c_mod.py or the reviewed SDK/broker.",
            "Relevant focused tests and the full python tools/f1c_mod.py sdk-test gate pass.",
            "New proven, disproven or open results are appended to findings.jsonl.",
            "The checkpoint and active guidance describe the new truth.",
            "No evidence, historical artifact, executable, or production mod was deleted or overwritten."
        ],
        "tool_catalog": str(TOOL_CATALOG.relative_to(ROOT)).replace("\\", "/"),
        "tool_workshop": "tools/sdk/TOOL_WORKSHOP.md"
    }
    if args.json:
        print(json.dumps(briefing, indent=2))
        return
    print("F1C autonomous agent bootstrap")
    print("Goal:", briefing["goal"])
    print("State:", briefing["state"])
    print("Front door:", briefing["front_door"])
    if briefing["freshness"]["warning"]:
        print("WARNING:", briefing["freshness"]["warning"])
    print("Next objective:", briefing["next_objectives_in_priority_order"][0])
    print("Rule:", briefing["decision_rule"])


def cmd_route(args):
    classification, instruction = classify_project_path(args.path)
    result = {"path": args.path, "classification": classification,
              "instruction": instruction, "front_door": "python tools/f1c_mod.py"}
    candidate = Path(args.path)
    if candidate.is_absolute():
        try:
            relative = candidate.resolve().relative_to(ROOT.resolve()).as_posix()
        except ValueError:
            relative = None
    else:
        relative = candidate.as_posix().lstrip("./")
    if relative:
        tool_catalogue = load_tool_catalog()
        known_paths = set(discover_tool_paths(tool_catalogue))
        known_paths.update(item["path"] for item in
                           tool_catalogue.get("implementation_overrides", []))
        if relative in known_paths:
            tool_record = classify_tool_implementation(relative, tool_catalogue)
            result["tool_selection"] = tool_record
            result["instruction"] = tool_record["reason"]
            owner = capability_map(tool_catalogue).get(tool_record.get("capability"))
            if owner:
                result["preferred_capability"] = {
                    key: owner[key] for key in
                    ("id", "role", "status", "summary", "command", "effects")
                }
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"{args.path}: {classification}")
        print(result["instruction"])
        if result.get("tool_selection"):
            tool = result["tool_selection"]
            print(f"Tool label: {tool['role']} / {tool['status']}")
        if result.get("preferred_capability"):
            print("Use:", result["preferred_capability"]["command"])


def cmd_inventory(args):
    contract = load_tool_contract()
    counts = {}
    records = []
    for path in ROOT.rglob("*"):
        if not path.is_file() or ".git" in path.parts:
            continue
        relative = path.relative_to(ROOT).as_posix()
        classification, instruction = classify_project_path(relative, contract)
        counts[classification] = counts.get(classification, 0) + 1
        if args.output:
            records.append({"path": relative, "classification": classification,
                            "instruction": instruction})
    report = {"schema": "f1c.file_routing_inventory.v1",
              "front_door": contract["front_door"],
              "total_files": sum(counts.values()),
              "classifications": dict(sorted(counts.items()))}
    if args.output:
        output = Path(args.output)
        if output.exists() and not args.force:
            raise ModError(f"refusing to overwrite inventory: {output}")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps({**report, "files": records}, indent=2), encoding="utf-8")
        report["output"] = str(output)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print("Repository files classified:", report["total_files"])
        for name, count in report["classifications"].items():
            print(f"  {name}: {count}")


def cmd_doctor(args):
    contract = load_tool_contract()
    checkpoint_path = SDK / "AUTOMATION_CHECKPOINT.json"
    checkpoint = (json.loads(checkpoint_path.read_text(encoding="utf-8"))
                  if checkpoint_path.is_file() else {})
    warnings = [
        f"{conflict['field']} {conflict['stale_source']}={conflict['stale_value']} "
        f"but {conflict['authoritative_source']}={conflict['authoritative']}"
        for conflict in status_conflicts(checkpoint)
    ]
    freshness, _recent_entries = checkpoint_freshness(checkpoint)
    if freshness["warning"]:
        warnings.append(freshness["warning"])
    required = [Path(item) for item in contract["required_files"]] + [
        TOOL_CATALOG.relative_to(ROOT),
        Path("tools/sdk/TOOL_WORKSHOP.md"),
    ]
    missing = [str(path) for path in required if not (ROOT / path).is_file()]
    problems = []
    if missing:
        problems.append("missing: " + ", ".join(missing))
    try:
        catalogue = load_catalogue()
        if not catalogue.get("functions") or not catalogue.get("vehicle_fields"):
            problems.append("canonical catalogue has no functions or vehicle fields")
    except (OSError, ValueError, KeyError) as error:
        problems.append(f"catalogue unreadable: {error}")
    try:
        tool_catalogue = load_tool_catalog()
        capability_ids = [item["id"] for item in tool_catalogue["capabilities"]]
        if len(capability_ids) != len(set(capability_ids)):
            problems.append("tool catalogue contains duplicate capability ids")
        runner_ids = {item["id"] for item in tool_catalogue.get("runners", [])}
        missing_capabilities = sorted(runner_ids - set(capability_ids))
        if missing_capabilities:
            problems.append("tool runners lack capabilities: " + ", ".join(missing_capabilities))
        for runner in tool_catalogue.get("runners", []):
            if not (ROOT / runner["path"]).is_file():
                problems.append(f"registered tool runner missing: {runner['path']}")
        for relative in discover_tool_paths(tool_catalogue):
            classify_tool_implementation(relative, tool_catalogue)
    except (OSError, ValueError, KeyError, ModError) as error:
        problems.append(f"tool catalogue unreadable: {error}")
    if Path(args.exe).is_file():
        digest = hashlib.sha256(Path(args.exe).read_bytes()).hexdigest().upper()
        if digest != TARGET_SHA256:
            problems.append("target executable SHA-256 is not the canonical fixed-address target")
    elif args.require_exe:
        problems.append(f"target executable missing: {args.exe}")
    checks_run = ["required files", "SDK catalogue readable",
                  "tool catalogue and runners", "status consistency (warning only)"]
    if Path(args.exe).is_file():
        checks_run.append("target executable SHA-256")
    report = {"schema": "f1c.sdk_doctor.v1", "ok": not problems,
              "problems": problems, "front_door": contract["front_door"],
              "target": "verified" if Path(args.exe).is_file() else "not present (optional)",
              "checks_run": checks_run,
              "checks_not_run": ["full SDK test gate (python tools/f1c_mod.py sdk-test)",
                                 "native/runtime acceptance"],
              "warnings": warnings}
    if args.json:
        print(json.dumps(report, indent=2))
    if problems:
        raise ModError("; ".join(problems))
    if not args.json:
        for warning in warnings:
            print("WARNING:", warning)
        print("Canonical SDK doctor passed.")
        print("Front door:", contract["front_door"])
        print("Target:", report["target"])
        print("Not checked:", "; ".join(report["checks_not_run"]))


def cmd_sdk_refresh(args):
    for helper in ("gen_sdk_from_index.py", "gen_vehicle_struct_header.py",
                   "gen_trace_schema.py", "gen_reviewed_cpp_api.py",
                   "build_recovery_index.py"):
        run([sys.executable, ROOT / "tools" / helper])
    run([sys.executable, SDK / "validate_sdk.py"])
    print("Canonical SDK artifacts refreshed and validated.")


def cmd_safety_car_status(args):
    from safety_car_status import observe, observe_vehicle
    try:
        result=observe()
        if args.vehicle_slot is not None:result['vehicle']=observe_vehicle(args.vehicle_slot)
        print(json_text(result))
    except ValueError as exc:
        raise ModError(str(exc)) from exc

def cmd_address_harvest(args):
    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    if output.exists():
        raise ModError(f"address harvest output already exists: {output}")
    result = address_harvest.harvest(ROOT)
    address_harvest.write_output(result, output)
    if args.json:
        print(json.dumps(result["counts"], indent=2))
    else:
        for line in address_harvest.counts_lines(result):
            print(line)
        print(f"Address harvest written: {output}")

def cmd_address_tier(args):
    harvest_dir = Path(args.harvest)
    if not harvest_dir.is_absolute():
        harvest_dir = ROOT / harvest_dir
    if not (harvest_dir / "harvest.jsonl").is_file():
        raise ModError(f"address tier harvest input not found: {harvest_dir / 'harvest.jsonl'}")
    for name in ("tier_proposals.jsonl", "REVIEW.md"):
        if (harvest_dir / name).exists():
            raise ModError(f"address tier output already exists: {harvest_dir / name}")
    result = address_tiering.propose(ROOT, harvest_dir)
    address_tiering.write_output(result, harvest_dir)
    if args.json:
        print(json.dumps({"counts": result["counts"],
                          "needs_review": result["needs_review"],
                          "total": result["total"]}, indent=2))
    else:
        for line in address_tiering.counts_lines(result):
            print(line)
        print(f"Address tier proposals written: {harvest_dir}")

GENERATED_ARTIFACTS = (
    "tools/sdk/f1c_sdk.json",
    "tools/sdk/f1c_sdk.h",
    "tools/sdk/f1c_vehicle_struct.h",
    "tools/sdk/f1c_vehicle_struct_ida.h",
    "tools/sdk/f1c_trace_schema.h",
    "tools/sdk/f1c_reviewed_api.hpp",
    "analysis/recovery_index.json",
)

TOOLING_TESTS = (
    "test_canonical_front_door.py",
    "test_recovery_index.py",
    "test_source_explorer.py",
    "test_status_consistency.py",
    "test_ask_recorded_findings.py",
    "test_ida_bridge.py",
    "test_ida_external_targets.py",
    "test_address_harvest.py",
    "test_address_tiering.py",
)

def artifact_digest(relative, data):
    if str(relative).endswith(".json"):
        text = data.decode("utf-8", errors="replace")
        text = re.sub(r'"generated":\s*"[^"]*"', '"generated": ""', text)
        data = text.encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def cmd_sdk_test(args):
    test_env = os.environ.copy()
    selected_exe = getattr(args, "exe", None)
    if selected_exe:
        selected_exe = Path(selected_exe).resolve()
        if not selected_exe.is_file():
            raise ModError(f"SDK test executable does not exist: {selected_exe}")
        if hashlib.sha256(selected_exe.read_bytes()).hexdigest().upper() != TARGET_SHA256:
            raise ModError("SDK tests require the exact canonical executable SHA256")
        test_env["F1C_SDK_TEST_EXE"] = str(selected_exe)
    else:
        # An inherited environment variable is not authority to change test inputs.
        test_env.pop("F1C_SDK_TEST_EXE", None)
    no_regenerate = getattr(args, "no_regenerate", False)
    if no_regenerate:
        test_env["F1C_SDK_TEST_NO_REGEN"] = "1"
    else:
        test_env.pop("F1C_SDK_TEST_NO_REGEN", None)
    suite = getattr(args, "suite", "all")
    if suite == "tooling":
        for name in TOOLING_TESTS:
            path = SDK / "tests" / name
            if not path.is_file():
                print(f"skipped (not present): {name}")
                continue
            run([sys.executable, path], env=test_env)
        print("Focused tooling tests passed (not the full SDK gate).")
        return
    if suite == "qualifying-order":
        run([sys.executable, SDK / "tests" / "test_qualifying_order.py"], env=test_env)
        print("Focused qualifying-order tests passed (not the full SDK gate).")
        return
    if suite == "garage-results":
        run([sys.executable, SDK / "tests" / "test_garage_results.py"], env=test_env)
        run([sys.executable, SDK / "tests" / "test_preload.py"], env=test_env)
        print("Focused garage-results and lifecycle tests passed (not the full SDK gate).")
        return
    if no_regenerate:
        run(["cmd", "/c", SDK / "tests" / "run_all.bat"], env=test_env)
        print("Canonical SDK test gate passed.")
        return
    temp_dir = Path(tempfile.mkdtemp())
    before_hashes = {}
    for relative in GENERATED_ARTIFACTS:
        path = ROOT / relative
        if path.is_file():
            data = path.read_bytes()
            target = temp_dir / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            before_hashes[relative] = artifact_digest(relative, data)
    try:
        run(["cmd", "/c", SDK / "tests" / "run_all.bat"], env=test_env)
    finally:
        changed = []
        for relative in GENERATED_ARTIFACTS:
            path = ROOT / relative
            current = (artifact_digest(relative, path.read_bytes())
                       if path.is_file() else None)
            if current != before_hashes.get(relative):
                changed.append(relative)
        if changed:
            stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
            backup_root = ROOT / "analysis" / "sdk_test_regen_before" / stamp
            for relative in changed:
                source = temp_dir / relative
                if source.is_file():
                    target = backup_root / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(source.read_bytes())
            print("Regenerated artifacts changed:", ", ".join(changed))
        else:
            print(f"Regenerated artifacts unchanged ({len(GENERATED_ARTIFACTS)} files).")
        shutil.rmtree(temp_dir, ignore_errors=True)
    print("Canonical SDK test gate passed.")


def cmd_field_viewer(args):
    command=[sys.executable,ROOT / "mods/sdk_sandbox/field_telemetry/field_viewer.py"]
    if args.once:command.append("--once")
    if args.json:command.append("--json")
    run(command)


def cmd_memory_mcp(args):
    from f1c_memory_mcp import main
    main(args)


def cmd_unload(args):
    if os.name != "nt":
        raise ModError("named-event unload is supported only on Windows")
    import ctypes
    import ctypes.wintypes as wintypes
    kernel32 = ctypes.windll.kernel32
    kernel32.OpenEventA.argtypes = [wintypes.DWORD, wintypes.BOOL, ctypes.c_char_p]
    kernel32.OpenEventA.restype = wintypes.HANDLE
    handle = kernel32.OpenEventA(0x0002, False, args.event.encode("ascii"))
    if not handle:
        raise ModError(f"no SDK mod is listening on unload event {args.event!r}")
    try:
        if not kernel32.SetEvent(handle):
            raise ModError(f"SetEvent failed for {args.event!r}")
    finally:
        kernel32.CloseHandle(handle)
    print("Unload requested through named event:", args.event)

def run(command, *, cwd=ROOT, env=None, capture=False, quiet=False):
    if not quiet:
        print("+ [canonical CLI internal operation]", flush=True)
    return subprocess.run([str(item) for item in command], cwd=cwd, env=env,
                          check=True, text=True, capture_output=capture)

def project_path(value):
    candidate = Path(value)
    if not candidate.is_absolute() and len(candidate.parts) == 1:
        candidate = SANDBOX / candidate
    return candidate.resolve()

def manifest_path(project):
    return project / "f1c_mod.json"

def is_within(path, parent):
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False

def load_manifest(project):
    path = manifest_path(project)
    if not path.is_file():
        raise ModError(f"manifest not found: {path}")
    return json.loads(path.read_text(encoding="utf-8"))

def load_catalogue():
    return json.loads((SDK / "f1c_sdk.json").read_text(encoding="utf-8"))

def load_recovery():
    if not RECOVERY.exists():
        run([sys.executable, ROOT / "tools" / "build_recovery_index.py"])
    return json.loads(RECOVERY.read_text(encoding="utf-8"))

def canonical_function_map(catalogue):
    reviewed = {}
    for name, metadata in catalogue["functions"].items():
        if metadata.get("trust") in ("FACT", "CONFIRMED"):
            reviewed[name.lower()] = (name, metadata)
            reviewed[f"0x{int(metadata['address']):08x}"] = (name, metadata)
    return reviewed

def resolve_function(value, reviewed):
    match = reviewed.get(str(value).lower())
    if not match:
        raise ModError(f"function is not a reviewed ABI: {value}")
    return match

def parse_hex_bytes(value):
    compact = re.sub(r"[^0-9a-fA-F]", "", value or "")
    if len(compact) % 2:
        raise ModError(f"invalid expected_bytes: {value!r}")
    return bytes.fromhex(compact)

class PEImage:
    def __init__(self, path):
        self.path = path
        self.data = path.read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        sections = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.image_base = struct.unpack_from("<I", self.data, pe + 52)[0]
        section_table = pe + 24 + optional_size
        self.sections = []
        for index in range(sections):
            offset = section_table + index * 40
            virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
                "<IIII", self.data, offset + 8)
            self.sections.append((virtual_address, virtual_size, raw_pointer, raw_size))

    def read_va(self, address, size):
        rva = address - self.image_base
        for virtual_address, virtual_size, raw_pointer, raw_size in self.sections:
            if virtual_address <= rva and rva + size <= virtual_address + max(virtual_size, raw_size):
                start = raw_pointer + rva - virtual_address
                return self.data[start:start + size]
        raise ModError(f"address is not file-backed: 0x{address:08X}")

def validate_manifest(project, manifest, *, check_conflicts=True, exe=DEFAULT_EXE):
    errors = []
    mods_root = ROOT / "mods"
    if is_within(project, mods_root) and not is_within(project, SANDBOX):
        errors.append("orchestrator refuses projects inside existing mods; use mods/sdk_sandbox")
    if manifest.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    mod_id = manifest.get("id", "")
    if not re.fullmatch(r"[a-z][a-z0-9_]{1,47}", mod_id):
        errors.append("id must match [a-z][a-z0-9_]{1,47}")
    if manifest.get("production") is not False:
        errors.append("sandbox manifest must explicitly set production=false")
    if manifest.get("language", "cpp") not in ("c", "cpp"):
        errors.append("language must be c or cpp")
    if manifest.get("target_sha256") != TARGET_SHA256:
        errors.append("manifest target_sha256 does not match the canonical executable")

    runtime_schema = ("source", "destination")
    for index, item in enumerate(manifest.get("runtime_files", [])):
        if not isinstance(item, dict):
            errors.append(f"runtime_files entry #{index + 1} must be an object")
            continue
        missing = [key for key in runtime_schema if key not in item]
        unknown = sorted(set(item) - set(runtime_schema))
        if missing or unknown:
            detail = f"missing required key {missing[0]!r}" if missing else ""
            if unknown:
                detail += ("; " if detail else "") + f"unexpected keys: {', '.join(unknown)}"
            errors.append(f"runtime_files entry #{index + 1}: {detail}")
            continue
        source = (project / str(item["source"])).resolve()
        destination_name = str(item["destination"])
        if not is_within(source, project) or not source.is_file():
            errors.append(f"runtime_files entry #{index + 1}: source must be an existing project file")
        if Path(destination_name).name != destination_name:
            errors.append(f"runtime_files entry #{index + 1}: destination must be a file name")
        expected_name = f"{manifest.get('id', '')}.ini"
        if destination_name.lower() != expected_name.lower():
            errors.append(f"runtime_files entry #{index + 1}: destination must be {expected_name}")
    for index, item in enumerate(manifest.get("startup_files", [])):
        if not isinstance(item, dict) or set(item) != set(runtime_schema):
            errors.append(f"startup_files entry #{index + 1} must contain only source and destination")
            continue
        source = (project / str(item["source"])).resolve()
        if not is_within(source, project) or source.suffix.lower() != ".dll":
            errors.append(f"startup_files entry #{index + 1}: source must be a project DLL")
        if str(item["destination"]).lower() != "dinput8.dll":
            errors.append(f"startup_files entry #{index + 1}: destination must be dinput8.dll")

    catalogue = load_catalogue()
    recovery = load_recovery()
    known_subsystems = set(recovery.get("subsystems", {}))
    for subsystem in manifest.get("subsystems", []):
        if subsystem not in known_subsystems:
            errors.append(f"unknown recovery subsystem tag: {subsystem}")
    reviewed = canonical_function_map(catalogue)
    field_map = {name.lower(): (name, field)
                 for name, field in catalogue["vehicle_fields"].items()}
    hook_ranges = []
    image = PEImage(exe) if exe.is_file() else None
    # Structural schema check first: a hand-written entry with the wrong keys
    # (e.g. site/name) used to die later as a bare `KeyError: 'function'`.
    # Report the first bad key instead of the downstream crash.
    hook_schema = ("function", "handler", "stolen_length", "expected_bytes")
    write_schema = ("object", "field")
    bad_hooks, bad_writes = set(), set()
    for index, hook in enumerate(manifest.get("hooks", [])):
        if not isinstance(hook, dict):
            errors.append(f"hook entry #{index + 1} must be an object")
            bad_hooks.add(index)
            continue
        missing = [key for key in hook_schema if key not in hook]
        unknown = sorted(set(hook) - set(hook_schema))
        if missing:
            errors.append(f"hook entry #{index + 1}: missing required key "
                          f"{missing[0]!r}" + (f" (unexpected keys: {', '.join(unknown)})" if unknown else ""))
            bad_hooks.add(index)
    for index, write in enumerate(manifest.get("writes", [])):
        if not isinstance(write, dict):
            errors.append(f"write entry #{index + 1} must be an object")
            bad_writes.add(index)
            continue
        missing = [key for key in write_schema if key not in write]
        unknown = sorted(set(write) - set(write_schema))
        if missing:
            errors.append(f"write entry #{index + 1}: missing required key "
                          f"{missing[0]!r}" + (f" (unexpected keys: {', '.join(unknown)})" if unknown else ""))
            bad_writes.add(index)
    for index, hook in enumerate(manifest.get("hooks", [])):
        if index in bad_hooks:
            continue
        try:
            name, metadata = resolve_function(hook.get("function"), reviewed)
            if not re.fullmatch(r"[A-Za-z_]\w*", str(hook.get("handler", ""))):
                raise ModError(f"hook {name}: handler must be a C++ identifier")
            expected = parse_hex_bytes(hook.get("expected_bytes"))
            stolen = int(hook.get("stolen_length", len(expected)))
            if stolen < 5 or stolen > 32 or len(expected) != stolen:
                raise ModError(f"hook {name}: expected_bytes must contain exactly 5..32 stolen bytes")
            address = int(metadata["address"])
            hook_ranges.append((address, address + stolen, name))
            if image and image.read_va(address, stolen) != expected:
                raise ModError(f"hook {name}: expected bytes disagree with the canonical executable")
        except (ModError, TypeError, ValueError) as error:
            errors.append(str(error))
    hook_ranges.sort()
    for left, right in zip(hook_ranges, hook_ranges[1:]):
        if left[1] > right[0]:
            errors.append(f"overlapping hooks: {left[2]} and {right[2]}")

    for index, write in enumerate(manifest.get("writes", [])):
        if index in bad_writes:
            continue
        requested = str(write.get("field", "")).lower()
        match = field_map.get(requested)
        if not match:
            errors.append(f"unknown vehicle write field: {write.get('field')}")
            continue
        name, field = match
        if field.get("trust") not in ("FACT", "CONFIRMED"):
            errors.append(f"write field is not trusted: {name}")
        semantic = field.get("semantic", "state")
        if semantic not in ("command_input", "latched_control"):
            errors.append(f"write to {name} refused: semantic is {semantic}, not a command/control")

    if (check_conflicts and is_within(project, SANDBOX)
            and not bad_hooks and not bad_writes):
        errors.extend(find_cross_project_conflicts(project, manifest, reviewed, field_map))
    if errors:
        raise ModError("manifest validation failed:\n - " + "\n - ".join(errors))
    return {"reviewed": reviewed, "fields": field_map,
            "hooks": hook_ranges, "catalogue": catalogue}

def find_cross_project_conflicts(project, manifest, reviewed, field_map):
    errors = []
    ours_hooks = []
    for hook in manifest.get("hooks", []):
        name, metadata = resolve_function(hook["function"], reviewed)
        start = int(metadata["address"])
        ours_hooks.append((start, start + int(hook.get("stolen_length", 0)), name))
    ours_writes = {str(item.get("field", "")).lower() for item in manifest.get("writes", [])}
    if not SANDBOX.exists():
        return errors
    for other_path in SANDBOX.glob("*/f1c_mod.json"):
        if other_path.resolve() == manifest_path(project).resolve():
            continue
        try:
            other = json.loads(other_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        for hook in other.get("hooks", []):
            try:
                other_name, metadata = resolve_function(hook["function"], reviewed)
                other_length = int(hook.get("stolen_length", 0))
            except (ModError, TypeError, ValueError):
                continue
            other_start = int(metadata["address"])
            other_end = other_start + other_length
            for our_start, our_end, our_name in ours_hooks:
                if our_start < other_end and other_start < our_end:
                    errors.append(f"hook conflict with {other.get('id')}: {our_name} / {other_name}")
        other_writes = {str(item.get("field", "")).lower() for item in other.get("writes", [])}
        for field in sorted(ours_writes & other_writes):
            canonical = field_map.get(field, (field, {}))[0]
            errors.append(f"write ownership conflict with {other.get('id')}: {canonical}")
    return errors

def cmd_init(args):
    project = Path(args.output).resolve() if args.output else project_path(args.id)
    if is_within(project, ROOT / "mods") and not is_within(project, SANDBOX):
        raise ModError("refusing to initialize inside an existing mod tree; use mods/sdk_sandbox")
    run([sys.executable, ROOT / "tools" / "create_f1c_mod.py", args.id,
         "--title", args.title or args.id.replace("_", " ").title(),
         "--output", project, "--language", args.language])
    manifest = {
        "schema_version": 1, "id": args.id,
        "title": args.title or args.id.replace("_", " ").title(),
        "production": False, "sdk_major": 1,
        "language": args.language,
        "target_sha256": TARGET_SHA256,
        "subsystems": [], "seeds": [], "hooks": [], "writes": [],
        "tracing": {"driver": "", "seconds": 180, "hz": 20,
                    "wait_phase": 5, "pit_cycle": False,
                    "semantics": ["command_input", "output", "latched_control"]},
        "regressions": [],
        "build": {"script": "build.bat", "dll": f"build/{args.id}.dll"},
    }
    manifest_path(project).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Manifest:", manifest_path(project))

def cmd_validate(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    try:
        validate_manifest(project, manifest, exe=Path(args.exe))
    except ModError as error:
        if not args.json:
            raise
        print(json.dumps({"valid": False, "id": manifest.get("id"),
                          "error": str(error)}, indent=2))
        raise SystemExit(1)
    if args.json:
        print(json.dumps({"valid": True, "id": manifest.get("id"),
                          "error": None}, indent=2))
        return
    print(f"Manifest valid: {manifest['id']} (no unreviewed hooks, unsafe writes, or sandbox conflicts)")

def emit_output(text, output=None):
    if not output:
        print(text, end="")
        return
    path = Path(output).resolve()
    if path.exists():
        raise ModError(f"refusing to overwrite output: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    print("Wrote:", path)

def emit_replaceable(text, output=None, force=False):
    if not output:
        print(text, end="")
        return
    path = Path(output).resolve()
    if path.exists() and not force:
        raise ModError(f"refusing to overwrite output without --force: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    print("Wrote:", path)

def cmd_search(args):
    recovery = load_recovery()
    results = search_sources(ROOT, recovery, args.query, corpora=args.corpus,
                             regex=args.regex, case_sensitive=args.case_sensitive,
                             mode=args.mode, context=args.context, limit=args.limit)
    text = json_text(results) if args.json else render_search(results, args.query)
    emit_output(text, args.output)
    if not source_explorer.LAST_PREFILTER["used"]:
        print(f"note: full corpus scan ({source_explorer.LAST_PREFILTER['reason']}); expect slower search",
              file=sys.stderr)
    if not results:
        raise ModError("no source matches")

def cmd_inspect(args):
    recovery = load_recovery()
    result = inspect_function(ROOT, recovery, load_catalogue(), args.function,
                              corpora=args.corpus, max_lines=args.max_lines,
                              full=args.full)
    attach_node_lookup(result, recovery, expand=args.expand_graph)
    result["sdk_links"] = sdk_links(ROOT, result["address"])
    text = (json_text(result) if args.json else
            render_inspection(result, max_neighbors=args.max_neighbors))
    emit_output(text, args.output)

def cmd_memory_build(args):
    index = build_index(ROOT)
    output = Path(args.output).resolve() if args.output else ROOT / "analysis" / "semantic_memory_index.json"
    if output.exists() and not args.force:
        raise ModError(f"refusing to overwrite generated index without --force: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json_text(index), encoding="utf-8")
    print("Semantic memory index:", output)
    print("Summary:", json.dumps(index["summary"], sort_keys=True))

def load_memory_index(args):
    value = getattr(args, "index", None)
    if not value:
        return build_index(ROOT)
    path = Path(value).resolve()
    index = json.loads(path.read_text(encoding="utf-8"))
    if index.get("schema_version") != 1 or "functions" not in index:
        raise ModError(f"invalid semantic memory index: {path}")
    return index

def cmd_memory_fingerprint(args):
    index = load_memory_index(args)
    address = None
    if args.json:
        from semantic_memory_linker import resolve_function
        address = resolve_function(index, args.function)
        text = json_text({"address": address, **index["functions"][address]})
    else:
        text = render_fingerprint(index, args.function)
    emit_output(text, args.output)

def cmd_memory_match(args):
    index = load_memory_index(args)
    signature = (load_signature(Path(args.signature).resolve()) if args.signature else
                 signature_from_fingerprint(index, args.function, args.base))
    result = match_capture(ROOT, signature, args.capture, slot=args.slot, limit=args.limit)
    if args.json:
        text = json_text(result)
    else:
        lines = [f"# Runtime layout matches: {signature.get('name', args.function)}", "",
                 f"Capture: `{result['capture']}`", "",
                 f"Matches: {result['total_matches']}", ""]
        for match in result["matches"]:
            lines.append(f"- slot {match['slot']} `{match['target']}` base `{match['base_offset']}` "
                         f"[{match['trust']}] ({match['matched']}/{match['constraints']})")
        text = "\n".join(lines) + "\n"
    emit_output(text, args.output)
    if not result["matches"]:
        raise ModError("signature did not match the capture")

def cmd_memory_correlate(args):
    result = correlate_event(ROOT, load_memory_index(args), args.label,
                             controls=args.control, limit=args.limit)
    text = json_text(result) if args.json else render_correlation(result)
    if args.output and args.force and Path(args.output).resolve().exists():
        path = Path(args.output).resolve()
        path.write_text(text, encoding="utf-8")
        print("Wrote:", path)
    else:
        emit_output(text, args.output)

def cmd_memory_front_wing(args):
    text = front_wing_report(ROOT, load_memory_index(args))
    output = args.output or str(ROOT / "analysis" / "semantic_memory_front_wing.md")
    path = Path(output).resolve()
    if path.exists() and not args.force:
        raise ModError(f"refusing to overwrite pilot report without --force: {path}")
    if path.exists():
        path.write_text(text, encoding="utf-8")
        print("Wrote:", path)
    else:
        emit_output(text, path)

def cmd_memory_snapshot(args):
    manifest_path, manifest = capture_snapshot(ROOT, args.tag, process_name=args.process,
                                               scope=args.scope)
    print("Memory snapshot:", manifest_path)
    print("Summary:", json.dumps(manifest["summary"], sort_keys=True))

def parse_transition(value):
    if value is None:
        return None
    match = re.fullmatch(r"([^:]+):([^:]+)", value)
    if not match:
        raise ModError("transition must be BEFORE:AFTER, for example 0:1")
    return int(match.group(1), 0), int(match.group(2), 0)

def cmd_memory_diff(args):
    result = diff_snapshots(ROOT, args.before, args.after,
                            transition=parse_transition(args.transition),
                            width=args.width, limit=args.limit)
    emit_replaceable(json_text(result), args.output, args.force)

def runtime_paths(tag):
    directory = ROOT / "runtime_memory" / "observations" / tag
    return directory, directory / "observation.json", directory / "report.json"

def cmd_runtime_observe(args):
    if args.seconds <= 0 or args.hz <= 0:
        raise ModError("runtime observation --seconds and --hz must be positive")
    recipe = load_recipe(args.recipes, args.recipe)
    directory, observation_path, report_path = runtime_paths(args.tag)
    if directory.exists():
        raise ModError(f"refusing to overwrite runtime observation: {directory}")
    observation, report = observe_live(
        ROOT, args.recipe, recipe, seconds=args.seconds, hz=args.hz,
        process_name=args.process, mode=args.mode, vehicle_base=args.vehicle_base,
        limit=args.limit,
        progress=lambda elapsed, slots: print(f"  observing: {elapsed:.0f}s, {slots} active slots", flush=True))
    directory.mkdir(parents=True)
    observation_path.write_text(json_text(observation), encoding="utf-8")
    report_path.write_text(json_text(report), encoding="utf-8")
    print(render_runtime_report(report), end="")
    print("Observation:", observation_path)
    print("Report:", report_path)

def cmd_runtime_correlate(args):
    if args.seconds <= 0 or args.hz <= 0 or args.max_events <= 0:
        raise ModError("runtime correlation --seconds, --hz, and --max-events must be positive")
    trigger_recipe = load_recipe(args.recipes, args.trigger)
    additional_trigger_names = args.also_trigger or []
    if len(additional_trigger_names) != len(set(additional_trigger_names)):
        raise ModError("each --also-trigger recipe may be specified only once")
    if args.trigger in additional_trigger_names:
        raise ModError("the primary trigger must not be repeated in --also-trigger")
    additional_triggers = {name: load_recipe(args.recipes, name) for name in additional_trigger_names}
    context_names = args.context or []
    trigger_names = {args.trigger, *additional_trigger_names}
    if trigger_names.intersection(context_names):
        raise ModError("a trigger is captured automatically; do not repeat it in --context")
    context = {name: load_recipe(args.recipes, name) for name in context_names}
    directory, observation_path, _ = runtime_paths(args.tag)
    correlation_path = directory / "correlation.json"
    if directory.exists():
        raise ModError(f"refusing to overwrite runtime correlation: {directory}")
    correlation = correlate_live(
        ROOT, args.trigger, trigger_recipe, context, additional_triggers=additional_triggers,
        seconds=args.seconds, hz=args.hz,
        process_name=args.process, vehicle_base=args.vehicle_base, max_events=args.max_events,
        progress=lambda elapsed, slots: print(f"  correlating: {elapsed:.0f}s, {slots} active slots", flush=True))
    directory.mkdir(parents=True)
    correlation_path.write_text(json_text(correlation), encoding="utf-8")
    print(f"Runtime correlation: triggers={','.join(correlation['triggers'])}, events={len(correlation['events'])}")
    print("No memory was written. Correlation is evidence of co-occurrence, not causation.")
    print("Correlation:", correlation_path)

def cmd_pit_marker_read(args):
    from runtime_validator import read_live_pit_marker
    print(json_text(read_live_pit_marker(args.process)),end="")

def cmd_vehicle_lifecycle_read(args):
    from vehicle_lifecycle_read import read_live
    print(json_text(read_live(args.process)),end="")


def cmd_runtime_read(args):
    names = [args.recipe, *(args.context or [])]
    if len(names) != len(set(names)):
        raise ModError("each runtime-read recipe may be specified only once")
    recipes = {name: load_recipe(args.recipes, name) for name in names}
    snapshot = read_live_fields(ROOT, recipes, process_name=args.process,
                                vehicle_base=args.vehicle_base)
    if args.json:
        print(json_text(snapshot), end="")
        return
    print(f"Runtime snapshot: {len(snapshot['values_by_slot'])} active vehicle slots")
    print("No memory was written. Use --json for machine-readable values.")


def cmd_qualifying_order(args):
    from qualifying_order import run_command
    run_command(ROOT, args)

def cmd_runtime_validate(args):
    recipe = load_recipe(args.recipes, args.recipe)
    path = Path(args.observation)
    if path.is_dir():
        path = path / "observation.json"
    observation = json.loads(path.read_text(encoding="utf-8"))
    report = validate_observation(args.recipe, recipe, observation, args.limit)
    text = json_text(report) if args.json else render_runtime_report(report)
    emit_replaceable(text, args.output, args.force)

def cmd_variant_profile(args):
    profile = VariantPEImage(args.exe).profile()
    result = {"schema_version": 1, "target": profile}
    if args.address is not None:
        if args.length is None:
            raise ModError("variant-profile --address requires --length")
        if args.length < 1 or args.length > 64:
            raise ModError("variant-profile --length must be between 1 and 64")
        address = int(args.address, 0)
        image = PEImage(Path(args.exe))
        data = image.read_va(address, args.length)
        result["code_bytes"] = {
            "address": f"0x{address:08X}",
            "length": len(data),
            "hex": data.hex(" ").upper(),
        }
    elif args.length is not None:
        raise ModError("variant-profile --length requires --address")
    emit_replaceable(json_text(result), args.output, args.force)

def cmd_symbol_recipes_build(args):
    recipes = bootstrap_f1c_recipes(args.exe, ROOT / "tools" / "ida_functions.csv")
    output = args.output or str(ROOT / "analysis" / "symbol_recipes.json")
    emit_replaceable(json_text(recipes), output, args.force)

def cmd_resolve_symbols(args):
    recipe_path = Path(args.recipes).resolve()
    if not recipe_path.is_file():
        raise ModError(f"symbol recipe file not found: {recipe_path}")
    recipes = json.loads(recipe_path.read_text(encoding="utf-8"))
    result = resolve_recipes(args.exe, recipes)
    emit_replaceable(json_text(result), args.output, args.force)
    if result["status"] != "RESOLVED":
        raise ModError("symbol resolution refused: inspect the generated evidence")

def resolve_seed(seed, recovery, reviewed):
    value = str(seed).lower()
    if value in reviewed:
        address = int(reviewed[value][1]["address"])
        return f"0x{address:08X}"
    matches = [key for key, node in recovery["functions"].items()
               if value == key.lower() or value == node["name"].lower() or
               value == node["declared_name"].lower()]
    if len(matches) != 1:
        raise ModError(f"seed must resolve uniquely: {seed} ({len(matches)} matches)")
    return matches[0]

def cmd_discover(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validated = validate_manifest(project, manifest, exe=Path(args.exe))
    recovery = load_recovery()
    seeds = [resolve_seed(seed, recovery, validated["reviewed"])
             for seed in manifest.get("seeds", [])]
    if not seeds:
        raise ModError("manifest has no seeds")
    visited = {}
    frontier = [(seed, 0, None) for seed in seeds]
    while frontier:
        key, depth, parent = frontier.pop(0)
        if key in visited and visited[key]["depth"] <= depth:
            continue
        node = recovery["functions"].get(key)
        if not node:
            continue
        visited[key] = {"depth": depth, "parent": parent, **node}
        if depth < args.depth:
            for target in node["callees"]:
                frontier.append((target, depth + 1, key))
            if args.callers:
                for target in node["callers"]:
                    frontier.append((target, depth + 1, key))
    generated = project / "generated"
    generated.mkdir(exist_ok=True)
    payload = {"schema_version": 1, "seeds": seeds, "depth": args.depth,
               "include_callers": args.callers, "functions": visited}
    (generated / "discovery.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    lines = [f"# Discovery: {manifest['title']}", "", f"Functions: {len(visited)}", ""]
    for key, node in sorted(visited.items(), key=lambda item: (item[1]["depth"], item[0])):
        lines.append(f"- depth {node['depth']} `{key}` `{node['name']}` — "
                     f"owner `{node['object_kind'] or 'unreviewed'}`, "
                     f"{len(node['callers'])} callers/{len(node['callees'])} callees")
    (generated / "discovery.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Discovery: {len(visited)} functions -> {generated / 'discovery.md'}")

def cmd_generate(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validated = validate_manifest(project, manifest, exe=Path(args.exe))
    generated = project / "generated"
    generated.mkdir(exist_ok=True)
    selected = set()
    for seed in manifest.get("seeds", []):
        try:
            selected.add(resolve_function(seed, validated["reviewed"])[0])
        except ModError:
            pass
    for hook in manifest.get("hooks", []):
        selected.add(resolve_function(hook["function"], validated["reviewed"])[0])
    lines = ["/* Generated project bindings. Do not edit. */", "#pragma once",
             '#include "f1c_reviewed_api.hpp"', '#include "f1c_pit.h"', "",
             f"namespace {manifest['id']} {{ namespace game = f1c::reviewed::game_thread;", "",
             "// Selected reviewed APIs:"]
    for name in sorted(selected):
        lines.append(f"using game::{name};")
    lines += ["", "} // namespace", ""]
    (generated / "mod_bindings.hpp").write_text("\n".join(lines), encoding="utf-8")
    hook_plan = []
    for hook in manifest.get("hooks", []):
        name, metadata = resolve_function(hook["function"], validated["reviewed"])
        hook_plan.append({**hook, "function": name,
                          "address": f"0x{int(metadata['address']):08X}",
                          "thread": metadata.get("thread"),
                          "calling_convention": metadata.get("calling_convention")})
    (generated / "hook_plan.json").write_text(
        json.dumps(hook_plan, indent=2) + "\n", encoding="utf-8")
    (generated / "trace_plan.json").write_text(
        json.dumps(manifest.get("tracing", {}), indent=2) + "\n", encoding="utf-8")

    def cpp_type(value):
        return value.replace("F1CVehicle*", "::F1CVehicle*").replace(
            "F1CPitMachine*", "::F1CPitMachine*")

    hook_lines = ["/* Generated guarded hook declarations. Do not edit. */", "#pragma once",
                  '#include "f1c_hook.h"', '#include "f1c_reviewed_api.hpp"', "",
                  "#if defined(_MSC_VER)", "#define F1C_GEN_THISCALL __thiscall",
                  "#define F1C_GEN_STDCALL __stdcall",
                  "#elif defined(__GNUC__) && defined(__i386__)",
                  "#define F1C_GEN_THISCALL __attribute__((thiscall))",
                  "#define F1C_GEN_STDCALL __attribute__((stdcall))", "#else",
                  "#define F1C_GEN_THISCALL", "#define F1C_GEN_STDCALL", "#endif", "",
                  f"namespace {manifest['id']} {{ namespace hooks {{", ""]
    install_names = []
    for hook in hook_plan:
        metadata = validated["catalogue"]["functions"][hook["function"]]
        name = hook["function"]
        handler = hook["handler"]
        ret = cpp_type(metadata.get("return", "void"))
        parameters = metadata.get("parameters", [])
        declarations = ", ".join(f"{cpp_type(item['type'])} {item['name']}" for item in parameters)
        convention = {"thiscall": "F1C_GEN_THISCALL ",
                      "stdcall": "F1C_GEN_STDCALL "}.get(metadata.get("calling_convention"), "")
        expected = parse_hex_bytes(hook["expected_bytes"])
        bytes_text = ", ".join(f"0x{byte:02X}" for byte in expected)
        hook_lines += [
            f"{ret} {convention}{handler}({declarations});",
            f"inline F1CDetour {name}_detour = {{}};",
            f"inline const uint8_t {name}_expected[] = {{{bytes_text}}};",
            f"inline F1CDetourResult install_{name}(F1CHookRegistry *registry)", "{",
            f"    return f1c_hook_registry_install(registry, &{name}_detour,",
            f"        reinterpret_cast<void*>(0x{int(metadata['address']):08X}u),",
            f"        reinterpret_cast<void*>(&{handler}), {name}_expected, sizeof({name}_expected));",
            "}",
            f"inline f1c::reviewed::game_thread::{name}Fn original_{name}()", "{",
            f"    return reinterpret_cast<f1c::reviewed::game_thread::{name}Fn>({name}_detour.trampoline);",
            "}", "",
        ]
        install_names.append(name)
    hook_lines += ["}} // namespace", "", "#undef F1C_GEN_THISCALL",
                   "#undef F1C_GEN_STDCALL", ""]
    (generated / "hooks.hpp").write_text("\n".join(hook_lines), encoding="utf-8")

    write_lines = ["/* Generated reviewed command-field setters. Do not edit. */", "#pragma once",
                   '#include "f1c_vehicle_struct.h"', "",
                   f"namespace {manifest['id']} {{ namespace commands {{", ""]
    for write in manifest.get("writes", []):
        canonical, field = validated["fields"][write["field"].lower()]
        ctype = field["ctype"]
        if ctype not in ("uint8_t", "int32_t", "float"):
            raise ModError(f"no generated setter representation for {canonical}: {ctype}")
        member = canonical
        write_lines += [f"inline void set_{canonical}(::F1CVehicle *vehicle, {ctype} value)", "{",
                        f"    vehicle->{member} = value;", "}", ""]
    write_lines += ["}} // namespace", ""]
    (generated / "commands.hpp").write_text("\n".join(write_lines), encoding="utf-8")
    print(f"Generated bindings/hooks/commands/plans in {generated}")

def cmd_build(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    cmd_generate(args)
    script = (project / manifest["build"]["script"]).resolve()
    dll = (project / manifest["build"]["dll"]).resolve()
    if dll.is_file():
        try:
            with open(dll, "r+b"):
                pass
        except PermissionError:
            raise ModError(
                f"build target is still injected into the running game: {dll}\n"
                "Signal a cooperative unload first — CLIENT DLLs before the broker,\n"
                "which refuses to unload while clients are registered:\n"
                "  python tools/f1c_mod.py unload F1C_ESC_PAUSE_MENU_UNLOAD\n"
                "  python tools/f1c_mod.py unload F1C_SUSFAIL_UNLOAD\n"
                "  python tools/f1c_mod.py unload F1C_PACEPROBE_UNLOAD\n"
                "  python tools/f1c_mod.py unload F1C_FUEL_FIRE_LAB_UNLOAD\n"
                "  python tools/f1c_mod.py unload F1C_GAME_UPDATE_BROKER_UNLOAD\n"
                "A missing event (ERROR: no SDK mod is listening) means that DLL is\n"
                "already out. Never FreeLibrary from outside — self-eject only.")
    environment = os.environ.copy()
    environment["F1C_SDK_DIR"] = str(SDK)
    run(["cmd", "/c", script], cwd=project, env=environment)
    if not dll.is_file() or dll.name.lower() in PROTECTED_NAMES:
        raise ModError(f"expected isolated DLL was not produced: {dll}")
    print("Build verified:", dll)

def cmd_trace(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    trace = manifest.get("tracing", {})
    driver = args.driver or trace.get("driver")
    if not driver:
        raise ModError("trace driver is required in manifest or --driver")
    output = project / "traces" / (args.output or time.strftime("%Y%m%d_%H%M%S.jsonl"))
    if output.exists():
        raise ModError(f"refusing to overwrite trace: {output}")
    command = [sys.executable, ROOT / "tools" / "trace_sdk_vehicle.py",
               "--car", driver, "--secs", str(trace.get("seconds", 180)),
               "--hz", str(trace.get("hz", 20)), "--output", output]
    if trace.get("wait_phase") is not None:
        command += ["--wait-phase", str(trace["wait_phase"])]
    if trace.get("pit_cycle"):
        command.append("--pit-cycle")
    for semantic in trace.get("semantics", []):
        command += ["--semantic", semantic]
    if args.print_only:
        print("Trace plan validated. Run through the canonical command:")
        print(f"python tools/f1c_mod.py trace {args.project}")
    else:
        run(command)

def cmd_compare(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    regressions = manifest.get("regressions", [])
    if args.name:
        regressions = [item for item in regressions if item.get("name") == args.name]
    if not regressions:
        raise ModError("no matching regressions")
    for regression in regressions:
        command = [sys.executable, ROOT / "tools" / "compare_event_traces.py",
                   project / regression["vanilla"], project / regression["rewrite"]]
        for semantic in regression.get("semantics", []):
            command += ["--semantic", semantic]
        for name in regression.get("fields", []):
            command += ["--name", name]
        if regression.get("require_match", True):
            command.append("--require-match")
        run(command)

def cmd_test(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    run(["cmd", "/c", ROOT / "tools" / "sdk" / "tests" / "run_all.bat"])
    cmd_build(args)
    if args.regressions and manifest.get("regressions"):
        compare_args = argparse.Namespace(project=args.project, name=None, exe=args.exe)
        cmd_compare(compare_args)
    print("Project test gate passed:", manifest["id"])

def cmd_stage(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    dll = (project / manifest["build"]["dll"]).resolve()
    if not dll.is_file():
        raise ModError("build the project before staging")
    destination = Path(args.destination).resolve()
    runtime_files = runtime_file_plans(project, manifest, destination)
    startup_files = companion_file_plans(project, manifest, destination, "startup_files")
    if getattr(args, "reuse_startup", False):
        for _source, target, marker in startup_files:
            verify_sdk_owned_file(target, marker)
        startup_files = []
    preflight_runtime_files(runtime_files + startup_files, args.replace_sdk_owned)
    command = [sys.executable, ROOT / "tools" / "deploy_f1c_mod.py", dll, destination]
    if args.replace_sdk_owned:
        command.append("--replace-sdk-owned")
    run(command)
    deploy_runtime_files(runtime_files)
    deploy_runtime_files(startup_files)


def runtime_file_plans(project, manifest, destination):
    """Return validated companion-file source, target, and ownership-marker paths."""
    plans = []
    for item in manifest.get("runtime_files", []):
        source = (project / item["source"]).resolve()
        target = destination / item["destination"]
        marker = destination / (item["destination"] + ".f1c-sdk-owned.json")
        plans.append((source, target, marker))
    return plans


def companion_file_plans(project, manifest, destination, manifest_key):
    plans = []
    for item in manifest.get(manifest_key, []):
        source = (project / item["source"]).resolve()
        if not source.is_file():
            raise ModError(f"build artifact is missing: {source}")
        target = destination / item["destination"]
        marker = destination / (item["destination"] + ".f1c-sdk-owned.json")
        plans.append((source, target, marker))
    return plans


def preflight_runtime_files(plans, replace_sdk_owned):
    """Refuse companion-file collisions before staging any artifact."""
    for _source, target, marker in plans:
        if not target.exists():
            continue
        if not replace_sdk_owned or not marker.is_file():
            raise ModError(f"runtime target exists and is not replace-authorized: {target}")
        try:
            ownership = json.loads(marker.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ModError(f"invalid runtime ownership marker: {marker}") from error
        if ownership.get("target") != target.name:
            raise ModError(f"runtime ownership marker does not match target: {marker}")


def deploy_runtime_files(plans):
    """Stage manifest-declared companion files after collision preflight."""
    for source, target, marker in plans:
        shutil.copy2(source, target)
        digest = hashlib.sha256(target.read_bytes()).hexdigest().upper()
        marker.write_text(json.dumps({
            "schema_version": 1,
            "target": target.name,
            "sha256": digest,
            "source": str(source),
        }, indent=2) + "\n", encoding="utf-8")
        print("Deployed SDK-owned runtime file:", target)
        print("SHA256:", digest)


def verify_sdk_owned_file(target, marker):
    """Verify an existing staged artifact without replacing its locked bytes."""
    if not target.is_file() or not marker.is_file():
        raise ModError(f"reusable SDK-owned artifact is missing: {target}")
    try:
        ownership = json.loads(marker.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ModError(f"invalid ownership marker: {marker}") from error
    digest = hashlib.sha256(target.read_bytes()).hexdigest().upper()
    if ownership.get("target") != target.name or ownership.get("sha256") != digest:
        raise ModError(f"ownership marker or hash does not match: {target}")


def game_process_running(process_name):
    """Return true only when the requested game executable is currently listed."""
    result = subprocess.run(
        ["tasklist", "/FI", f"IMAGENAME eq {process_name}", "/FO", "CSV", "/NH"],
        check=False, text=True, capture_output=True)
    if result.returncode:
        raise ModError("could not inspect the running game process")
    return any(line.lstrip('"').lower().startswith(process_name.lower())
               for line in result.stdout.splitlines())


def game_process_id_matches(process_id, process_name):
    """Fail closed unless one exact PID has the requested executable name."""
    result = subprocess.run(
        ["tasklist", "/FI", f"PID eq {process_id}", "/FO", "CSV", "/NH"],
        check=False, text=True, capture_output=True)
    if result.returncode:
        raise ModError("could not inspect the requested game process")
    prefix = process_name.lower() + '"'
    return any(line.lstrip('"').lower().startswith(prefix)
               for line in result.stdout.splitlines())


def cmd_load(args):
    """Stage one validated sandbox DLL, then request the local game loader to load it."""
    if not args.acknowledge_risk:
        raise ModError("loading a DLL changes the running game; repeat with --acknowledge-risk")
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    source_dll = (project / manifest["build"]["dll"]).resolve()
    if not source_dll.is_file():
        raise ModError("build the project before loading it")
    destination = Path(args.destination).resolve()
    if destination != GAME_DIR.resolve():
        raise ModError(f"refusing to load outside the canonical game directory: {destination}")
    process_name = Path(args.exe).name
    if not game_process_running(process_name):
        raise ModError(f"{process_name} is not running")
    staged_dll = destination / source_dll.name
    loader = destination / "inject.exe"
    if args.pid is not None:
        if args.pid <= 0 or not game_process_id_matches(args.pid, process_name):
            raise ModError(f"PID {args.pid} is not a running {process_name}")
        injector_name = manifest.get("build", {}).get("injector")
        if not injector_name:
            raise ModError("this project does not declare a PID-targeting injector")
        loader = (project / injector_name).resolve()
        loader_arguments = [staged_dll, str(args.pid), process_name]
    else:
        loader_arguments = [staged_dll]
    if not loader.is_file():
        raise ModError(f"canonical game loader is missing: {loader}")
    runtime_files = runtime_file_plans(project, manifest, destination)
    if args.reuse_staged:
        verify_sdk_owned_file(staged_dll, destination / (staged_dll.name + ".f1c-sdk-owned.json"))
        for _source, target, marker in runtime_files:
            verify_sdk_owned_file(target, marker)
    else:
        preflight_runtime_files(runtime_files, args.replace_sdk_owned)
    if args.dry_run:
        print("Load plan validated (no files copied and no process changed):")
        print("  source:", source_dll)
        print("  staged DLL:", staged_dll)
        for source, target, _marker in runtime_files:
            print("  runtime file:", source, "->", target)
        print("  running process:", process_name)
        if args.pid is not None:
            print("  target PID:", args.pid)
        print("  loader:", loader)
        print("  elevated loader:", "yes" if args.elevate else "no")
        print("  reuse staged artifacts:", "yes" if args.reuse_staged else "no")
        return
    if not args.reuse_staged:
        cmd_stage(argparse.Namespace(project=args.project, exe=args.exe,
                                     destination=str(destination),
                                     replace_sdk_owned=args.replace_sdk_owned))
    if not staged_dll.is_file():
        raise ModError(f"staging did not produce the expected DLL: {staged_dll}")
    if args.elevate:
        def ps_literal(value):
            return "'" + str(value).replace("'", "''") + "'"
        inner = ("'' | & " + ps_literal(loader) + " "
                 + " ".join(ps_literal(item) for item in loader_arguments)
                 + "; exit $LASTEXITCODE")
        encoded = base64.b64encode(inner.encode("utf-16le")).decode("ascii")
        elevated_arguments = f"-NoProfile -WindowStyle Hidden -EncodedCommand {encoded}"
        script = ("$process = Start-Process -FilePath powershell.exe"
                  + " -ArgumentList " + ps_literal(elevated_arguments)
                  + " -WorkingDirectory " + ps_literal(destination)
                  + " -Verb RunAs -WindowStyle Hidden -Wait -PassThru;"
                  + " exit $process.ExitCode")
        result = subprocess.run(["powershell", "-NoProfile", "-Command", script],
                                text=True, capture_output=True, check=False)
    else:
        result = subprocess.run([str(loader), *map(str, loader_arguments)], cwd=destination,
                                input="\n", text=True, capture_output=True, check=False)
    if result.returncode:
        detail = (result.stderr or result.stdout or "game loader failed").strip()
        raise ModError(f"game loader failed: {detail}")
    print("DLL load requested for running game:", staged_dll)


def cmd_track_status_gui(args):
    """Launch the external read-only GUI for the Track Status Monitor."""
    script = SANDBOX / "track_status_monitor" / "gui" / "track_status_gui.py"
    if not script.is_file():
        raise ModError(f"track-status GUI is missing: {script}")
    subprocess.Popen([sys.executable, str(script)], cwd=ROOT)
    print("Track Status GUI launched:", script)


def cmd_load_broker(args):
    """Load the reviewed broker dependency required by read-only SDK clients."""
    if not args.acknowledge_risk:
        raise ModError("loading the broker changes the running game; repeat with --acknowledge-risk")
    cmd_doctor(argparse.Namespace(exe=args.exe, require_exe=True, json=False))
    broker = SANDBOX / "game_update_broker" / "build" / "f1c_game_update_broker.dll"
    if not broker.is_file():
        raise ModError("reviewed broker DLL is missing; run the SDK test/build gate first")
    loader = GAME_DIR / "inject.exe"
    if not loader.is_file():
        raise ModError(f"canonical game loader is missing: {loader}")
    process_name = Path(args.exe).name
    if not game_process_running(process_name):
        raise ModError(f"{process_name} is not running")
    if args.dry_run:
        print("Broker load plan validated (no process changed):", broker)
        return
    result = subprocess.run([str(loader), str(broker)], cwd=GAME_DIR,
                            input="\n", text=True, capture_output=True, check=False)
    if result.returncode:
        detail = (result.stderr or result.stdout or "game loader failed").strip()
        raise ModError(f"broker loader failed: {detail}")
    print("Broker load requested for running game:", broker)

def cmd_pipeline(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    if manifest.get("seeds"):
        cmd_discover(argparse.Namespace(project=args.project, exe=args.exe,
                                        depth=args.depth, callers=args.callers))
    cmd_test(argparse.Namespace(project=args.project, exe=args.exe,
                                regressions=args.regressions))
    print("Full local pipeline passed:", manifest["id"])

def cmd_ida_prepare(args):
    project = project_path(args.project)
    manifest = load_manifest(project)
    validate_manifest(project, manifest, exe=Path(args.exe))
    source = Path(args.source_idb).resolve()
    if not source.is_file() or not IDAT.is_file():
        raise ModError("source IDB or IDA executable is missing")
    destination = project / "analysis" / f"{manifest['id']}.i64"
    if destination.exists():
        raise ModError(f"refusing to overwrite project IDB: {destination}")
    destination.parent.mkdir(exist_ok=True)
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    shutil.copy2(source, destination)
    scripts = ["apply_vehicle_struct_idb.py", "apply_vehicle_retype_idb.py",
               "apply_pit_types_idb.py", "apply_reviewed_names_idb.py"]
    for script in scripts:
        log = destination.parent / (Path(script).stem + ".log")
        run([IDAT, "-A", f"-L{log}", f"-S{ROOT / 'tools' / script}", destination])
    after = hashlib.sha256(source.read_bytes()).hexdigest()
    if before != after:
        raise ModError("primary IDB changed during copy preparation")
    print("Prepared isolated project IDB:", destination)

LAB_IDB = ROOT / "analysis" / "sdk_lab_idb" / "F1_Challenge_Online_sdk_lab.i64"
IDA_BRIDGE = ROOT / "tools" / "ida_bridge.py"
IDA_APPLY_SCRIPTS = ("apply_vehicle_struct_idb.py", "apply_vehicle_retype_idb.py",
                     "apply_pit_types_idb.py", "apply_reviewed_names_idb.py")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def default_query_idb():
    """Newest reviewed typed lab IDB, else the SDK lab IDB."""
    typed = sorted(LAB_IDB.parent.glob("F1_Challenge_Online_sdk_lab_typed_*.i64"))
    return typed[-1] if typed else LAB_IDB


def run_ida_bridge(working_db, result_path, bridge_args):
    command = [sys.executable, str(IDA_BRIDGE), "--db", str(working_db),
               "--result", str(result_path), *bridge_args]
    completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
    if not Path(result_path).is_file():
        tail = (completed.stderr or completed.stdout or "").strip()[-800:]
        raise ModError(f"IDA bridge produced no result (exit {completed.returncode}): {tail}")
    result = json.loads(Path(result_path).read_text(encoding="utf-8"))
    if not result.get("ok"):
        raise ModError(f"IDA bridge failed: {result.get('error')}")
    return result


def cmd_ida_query(args):
    source = Path(args.idb).resolve() if args.idb else default_query_idb()
    if not source.is_file():
        raise ModError(f"source IDB missing: {source}")
    before = sha256_file(source)
    # Work on a disposable copy keyed by the source hash; the source is never opened.
    work = Path(tempfile.gettempdir()) / "f1c_ida_query" / before[:16]
    work.mkdir(parents=True, exist_ok=True)
    working_db = work / "db.i64"
    if not working_db.is_file():
        shutil.copy2(source, working_db)
    result_path = work / f"result_{os.getpid()}.json"
    bridge_args = ["query", args.operation, args.address, "--max-lines", str(args.max_lines),
                   "--count", str(args.count), "--size", str(args.size)]
    try:
        result = run_ida_bridge(working_db, result_path, bridge_args)
    finally:
        if result_path.is_file():
            result_path.unlink()
    if sha256_file(source) != before:
        raise ModError("source IDB changed during a read-only query")
    result.pop("ok", None)
    report = {"schema": "f1c.ida_query.v1", "source_idb": str(source),
              "source_sha256": before, "note": "Static IDB evidence; names and types carry "
              "their own trust tiers and are not runtime proof.", **result}
    if args.json:
        print(json.dumps(report, indent=2))
        return
    print(f"# ida-query {args.operation} {report.get('address')}  ({source.name})")
    if "pseudocode" in report:
        print(report["pseudocode"])
        if report["truncated"]:
            print(f"... truncated at {args.max_lines} of {report['line_count']} lines")
    elif "instructions" in report:
        for row in report["instructions"]:
            print(row["address"], row["text"])
    elif "xrefs" in report:
        for row in report["xrefs"]:
            owner = row["from_function"]["name"] if row["from_function"] else "-"
            print(row["from"], "->", row["to"], row["type"], owner)
        if report["truncated"]:
            print(f"showing {report['returned']} of {report['total']}")
    else:
        print(json.dumps({k: v for k, v in report.items() if k not in ("schema", "note")}, indent=2))


def cmd_ida_new_project(args):
    """Create an isolated IDB for a non-game PE target with recorded provenance."""
    project = project_path(args.project)
    if is_within(project, ROOT) and not is_within(project, SANDBOX):
        raise ModError("external-target projects must live under mods/sdk_sandbox "
                       "(or outside the repository for isolated tests)")
    exe = Path(args.exe).resolve()
    if not exe.is_file():
        raise ModError(f"target executable is missing: {exe}")
    with open(exe, "rb") as handle:
        if handle.read(2) != b"MZ":
            raise ModError(f"target is not a PE executable: {exe}")
    if not IDAT.is_file():
        raise ModError(f"IDA batch executable is missing: {IDAT}")
    digest = sha256_file(exe)
    name = args.name or (exe.stem + ".i64")
    if Path(name).name != name or not name.lower().endswith(".i64"):
        raise ModError("--name must be a plain .i64 file name")
    analysis = project / "analysis"
    idb = analysis / name
    provenance_path = analysis / (Path(name).stem + ".provenance.json")
    if idb.exists() or provenance_path.exists():
        raise ModError(f"refusing to overwrite existing analysis: {idb}")
    work = Path(tempfile.mkdtemp(prefix="f1c_ida_new_project_"))
    copied = work / "target.exe"
    shutil.copy2(exe, copied)
    if sha256_file(copied) != digest:
        raise ModError("temporary copy does not match the source digest")
    log = work / "idat_batch.log"
    completed = subprocess.run([str(IDAT), "-A", f"-L{log}", "-B", str(copied)],
                               cwd=work, capture_output=True, text=True)
    produced = copied.parent / (copied.name + ".i64")
    if not produced.is_file():
        tail = ""
        if log.is_file():
            tail = log.read_text(encoding="utf-8", errors="replace").strip()[-500:]
        raise ModError("IDA batch analysis produced no database "
                       f"(exit {completed.returncode}): {tail}")
    analysis.mkdir(parents=True, exist_ok=True)
    shutil.copy2(produced, idb)
    idb_digest = sha256_file(idb)
    provenance = {
        "schema": "f1c.external_idb_provenance.v1",
        "created": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "project": str(project),
        "target_exe": str(exe),
        "target_sha256": digest,
        "target_size": exe.stat().st_size,
        "ida_batch_executable": str(IDAT),
        "idb": str(idb),
        "idb_sha256": idb_digest,
        "note": ("External target analysis. This database is NOT the game IDB; never "
                 "mix its functions, addresses or findings with game evidence."),
    }
    with open(provenance_path, "x", encoding="utf-8", newline="\n") as handle:
        json.dump(provenance, handle, indent=2)
    print("External target IDB prepared:", idb)
    print("target_sha256:", digest)
    print("idb_sha256:", idb_digest)
    print("provenance:", provenance_path)


def cmd_ida_survey(args):
    """One-pass read-only IDB survey written to a new JSON file."""
    source = Path(args.idb).resolve()
    if not source.is_file():
        raise ModError(f"source IDB missing: {source}")
    output = Path(args.output).resolve()
    if output.exists():
        raise ModError(f"refusing to overwrite survey output: {output}")
    before = sha256_file(source)
    work = Path(tempfile.gettempdir()) / "f1c_ida_survey" / before[:16]
    work.mkdir(parents=True, exist_ok=True)
    working_db = work / "db.i64"
    if not working_db.is_file():
        shutil.copy2(source, working_db)
    result_path = work / f"survey_{os.getpid()}.json"
    try:
        result = run_ida_bridge(working_db, result_path,
                                ["survey", "--min-string", str(args.min_string),
                                 "--max-strings", str(args.max_strings),
                                 "--max-functions", str(args.max_functions)])
    finally:
        if result_path.is_file():
            result_path.unlink()
    if sha256_file(source) != before:
        raise ModError("source IDB changed during a read-only survey")
    result.pop("ok", None)
    report = {"schema": "f1c.ida_survey.v1", "source_idb": str(source),
              "source_sha256": before,
              "note": ("Static IDB survey (functions, strings, imports, exports, "
                       "string xrefs, call edges). Names carry their own trust "
                       "tiers; this is not runtime proof."),
              **result}
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "x", encoding="utf-8", newline="\n") as handle:
        json.dump(report, handle, indent=2)
    counts = {key: len(report.get(key, [])) for key in
              ("functions", "imports", "exports", "strings", "string_refs", "call_edges")}
    print("Survey written:", output)
    print(json.dumps(counts, indent=2))


def cmd_ida_export(args):
    source = Path(args.source_idb).resolve()
    output = Path(args.output).resolve()
    save_idb = Path(args.save_idb).resolve() if args.save_idb else None
    if not source.is_file() or not IDAT.is_file():
        raise ModError("source IDB or IDA executable is missing")
    if output.exists():
        raise ModError(f"refusing to write into an existing export directory: {output}")
    if save_idb and save_idb.exists():
        raise ModError(f"refusing to overwrite IDB: {save_idb}")
    before = sha256_file(source)
    work = Path(tempfile.mkdtemp(prefix="f1c_ida_export_"))
    working_db = work / "db.i64"
    shutil.copy2(source, working_db)
    output.mkdir(parents=True)
    logs = output / "_logs"
    logs.mkdir()
    applied = []
    for script in IDA_APPLY_SCRIPTS:
        log = logs / (Path(script).stem + ".log")
        completed = subprocess.run([str(IDAT), "-A", f"-L{log}",
                                    f"-S{ROOT / 'tools' / script}", str(working_db)],
                                   cwd=ROOT, capture_output=True, text=True)
        applied.append({"script": script, "exit_code": completed.returncode,
                        "log": log.relative_to(output).as_posix()})
        if completed.returncode != 0:
            raise ModError(f"{script} failed (exit {completed.returncode}); see {log}")
    typed_sha = sha256_file(working_db)
    result = run_ida_bridge(working_db, work / "export_result.json",
                            ["export", "--output", str(output)])
    if save_idb:
        save_idb.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(working_db, save_idb)
    if sha256_file(source) != before:
        raise ModError("source IDB changed during export")
    manifest = {
        "schema": "f1c.typed_export.v1",
        "created": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "source_idb": str(source), "source_sha256": before,
        "typed_idb_sha256": typed_sha,
        "saved_typed_idb": str(save_idb) if save_idb else None,
        "applied_scripts": applied,
        "functions_written": result["written"],
        "decompile_failures": result["failures"],
        "export_seconds": result["seconds"],
        "trust": ("TYPED_HEXRAYS: machine decompilation of the typed IDB. Types come from "
                  "reviewed SDK layouts; function names mix reviewed SDK names with older "
                  "DERIVED IDB names. The raw functions/ corpus remains preserved evidence."),
    }
    (output / "MANIFEST.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Typed export: {result['written']} functions, "
          f"{len(result['failures'])} decompile failures -> {output}")
    if save_idb:
        print("Typed IDB saved:", save_idb)
    print("Temporary work directory (outside the workspace):", work)


def cmd_camera_editor(args):
    sys.path.insert(0, str(ROOT / "mods" / "sdk_sandbox" / "camera_editor"))
    import cam_editor_gui
    argv = []
    if args.cfg:
        argv += ["--cfg", args.cfg]
    for name in args.exe or []:
        argv += ["--exe", name]
    cam_editor_gui.main(argv)

def build_parser(include_info=True):
    parser = argparse.ArgumentParser(description=__doc__, epilog=(
        "Do not invoke tools/*.py helpers directly; they are private implementation modules."))
    sub = parser.add_subparsers(dest="command", required=True)
    if include_info:
        start = sub.add_parser("start", help="return the complete autonomous-agent cold-start briefing")
        start.add_argument("--json", action="store_true")
        start.set_defaults(func=cmd_start)
        sdk_info = sub.add_parser("sdk-info", help="show the canonical SDK contract and status")
        sdk_info.add_argument("--json", action="store_true")
        sdk_info.set_defaults(func=cmd_sdk_info)
        route = sub.add_parser("route", help="classify any repository path before using it")
        route.add_argument("path")
        route.add_argument("--json", action="store_true")
        route.set_defaults(func=cmd_route)
        inventory = sub.add_parser("inventory", help="classify every repository file without modifying it")
        inventory.add_argument("--json", action="store_true")
        inventory.add_argument("--output", help="optionally write a complete path-by-path sidecar index")
        inventory.add_argument("--force", action="store_true")
        inventory.set_defaults(func=cmd_inventory)
        doctor = sub.add_parser("doctor", help="verify the canonical SDK and optional target")
        doctor.add_argument("--exe", default=str(DEFAULT_EXE))
        doctor.add_argument("--require-exe", action="store_true")
        doctor.add_argument("--json", action="store_true")
        doctor.set_defaults(func=cmd_doctor)
        refresh = sub.add_parser("sdk-refresh", help="regenerate and validate canonical SDK artifacts")
        refresh.set_defaults(func=cmd_sdk_refresh)
        harvest = sub.add_parser("address-harvest",
                                 help="collect every used address into a read-only review list")
        harvest.add_argument("--output", required=True,
                             help="new output directory; must not exist")
        harvest.add_argument("--json", action="store_true")
        harvest.set_defaults(func=cmd_address_harvest)
        tier = sub.add_parser("address-tier",
                              help="propose review-only trust tiers for a harvest list")
        tier.add_argument("--harvest", required=True,
                          help="existing harvest directory containing harvest.jsonl")
        tier.add_argument("--json", action="store_true")
        tier.set_defaults(func=cmd_address_tier)
        viewer = sub.add_parser("field-viewer", help="read-only whole-field telemetry and inversion decisions")
        viewer.add_argument("--once", action="store_true")
        viewer.add_argument("--json", action="store_true")
        viewer.set_defaults(func=cmd_field_viewer)
        memory_mcp = sub.add_parser("memory-mcp", help="read-only local MCP bridge and hidden session reader")
        memory_mcp.add_argument("action", choices=["serve", "reader", "start-reader", "health", "drivers", "positions", "safety_car", "test", "verify"])
        memory_mcp.add_argument("--slot",type=int,help="positions only: include this bounded vehicle slot's full bytes and scene graph")
        memory_mcp.set_defaults(func=cmd_memory_mcp)
        sc_status = sub.add_parser("safety-car-status", help="read existing Safety Car ABI1 diagnostics without writes")
        sc_status.add_argument('--vehicle-slot',type=int,help='also read reviewed lifecycle/surface/restart fields for slot0..103')
        sc_status.set_defaults(func=cmd_safety_car_status)
        sdk_test = sub.add_parser("sdk-test", help="run the complete canonical SDK test gate")
        sdk_test.add_argument("--exe", help="explicit read-only test executable; exact canonical SHA256 required")
        sdk_test.add_argument("--suite", choices=("all", "qualifying-order", "garage-results", "tooling"), default="all",
                              help="default is the full gate; qualifying-order runs only its focused regressions")
        sdk_test.add_argument("--no-regenerate", action="store_true",
                              help="skip regenerating SDK artifacts in the full gate")
        sdk_test.set_defaults(func=cmd_sdk_test)
        unload = sub.add_parser("unload", help="request a cooperative named-event DLL unload")
        unload.add_argument("event")
        unload.set_defaults(func=cmd_unload)
        capabilities = sub.add_parser(
            "capabilities", help="list preferred jobs or every labelled tool implementation")
        capabilities.add_argument("--all", action="store_true",
                                  help="show implementations instead of public capabilities")
        capabilities.add_argument("--role")
        capabilities.add_argument("--status")
        capabilities.add_argument("--json", action="store_true")
        capabilities.set_defaults(func=cmd_capabilities)
        tool_info = sub.add_parser(
            "tool-info", help="explain a capability or preserved tool implementation")
        tool_info.add_argument("target")
        tool_info.add_argument("--json", action="store_true")
        tool_info.set_defaults(func=cmd_tool_info)
        recommend = sub.add_parser(
            "recommend", help="recommend the preferred capability for a natural-language task")
        recommend.add_argument("task")
        recommend.add_argument("--limit", type=int, default=3, choices=range(1, 11))
        recommend.add_argument("--json", action="store_true")
        recommend.set_defaults(func=cmd_recommend)
        tool_run = sub.add_parser(
            "tool-run", help="run an approved specialist backend through the canonical front door")
        tool_run.add_argument("--acknowledge-risk", action="store_true",
                              help="required for registered config/live-write specialists")
        tool_run.add_argument("tool_id")
        tool_run.add_argument("tool_args", nargs=argparse.REMAINDER)
        tool_run.set_defaults(func=cmd_tool_run)
        evidence_query = sub.add_parser(
            "evidence-query", help="query joined functions, offsets, globals, tokens, or findings")
        evidence_query.add_argument(
            "kind", choices=("func", "offset", "global", "token", "findings", "stats"))
        evidence_query.add_argument("value", nargs="?")
        evidence_query.add_argument("--json", action="store_true")
        evidence_query.set_defaults(func=cmd_evidence_query)
        sdk_query = sub.add_parser(
            "sdk-query", help="query canonical SDK fields and functions without opening its JSON")
        sdk_query.add_argument("kind", choices=("field", "function", "stats"))
        sdk_query.add_argument("query", nargs="?")
        sdk_query.add_argument("--trust",
                               choices=("FACT", "CONFIRMED", "OBSERVED", "DERIVED", "LEGACY"))
        sdk_query.add_argument("--limit", type=int, default=25, choices=range(1, 501))
        sdk_query.add_argument("--json", action="store_true")
        sdk_query.set_defaults(func=cmd_sdk_query)
    init = sub.add_parser("init", help="create an isolated manifest-driven SDK mod")
    init.add_argument("id"); init.add_argument("--title"); init.add_argument("--output")
    init.add_argument("--language", choices=("c", "cpp"), default="cpp")
    init.set_defaults(func=cmd_init)
    search = sub.add_parser("search", help="full-text search the source corpora")
    search.add_argument("query")
    search.add_argument("--corpus", action="append",
                        choices=("all", "raw", "typed", "annotated", "facts", "derived", "translated"))
    search.add_argument("--regex", action="store_true")
    search.add_argument("--case-sensitive", action="store_true")
    search.add_argument("--mode", choices=("all", "any"), default="all")
    search.add_argument("--context", type=int, default=1, choices=range(0, 11))
    search.add_argument("--limit", type=int, default=25, choices=range(1, 501))
    search.add_argument("--json", action="store_true")
    search.add_argument("--output")
    search.set_defaults(func=cmd_search)
    inspect = sub.add_parser("inspect", help="show an address-linked source dossier")
    inspect.add_argument("function")
    inspect.add_argument("--corpus", action="append",
                         choices=("all", "raw", "typed", "annotated", "facts", "derived", "translated"))
    inspect.add_argument("--max-lines", type=int, default=240)
    inspect.add_argument("--full", action="store_true")
    inspect.add_argument("--max-neighbors", type=int, default=25)
    inspect.add_argument("--expand-graph", action="store_true")
    inspect.add_argument("--json", action="store_true")
    inspect.add_argument("--output")
    inspect.set_defaults(func=cmd_inspect)
    memory_build = sub.add_parser("memory-build", help="build static/runtime semantic linker index")
    memory_build.add_argument("--output")
    memory_build.add_argument("--force", action="store_true")
    memory_build.set_defaults(func=cmd_memory_build)
    fingerprint = sub.add_parser("memory-fingerprint", help="inspect a static object-layout fingerprint")
    fingerprint.add_argument("function")
    fingerprint.add_argument("--json", action="store_true")
    fingerprint.add_argument("--output")
    fingerprint.add_argument("--index")
    fingerprint.set_defaults(func=cmd_memory_fingerprint)
    memory_match = sub.add_parser("memory-match", help="match a static/signature layout in a runtime capture")
    memory_match.add_argument("function")
    memory_match.add_argument("capture")
    memory_match.add_argument("--signature")
    memory_match.add_argument("--base")
    memory_match.add_argument("--slot", type=int)
    memory_match.add_argument("--limit", type=int, default=25)
    memory_match.add_argument("--json", action="store_true")
    memory_match.add_argument("--output")
    memory_match.add_argument("--index")
    memory_match.set_defaults(func=cmd_memory_match)
    correlate = sub.add_parser("memory-correlate", help="rank runtime fields changed by an event")
    correlate.add_argument("label")
    correlate.add_argument("--control", action="append")
    correlate.add_argument("--limit", type=int, default=50)
    correlate.add_argument("--json", action="store_true")
    correlate.add_argument("--output")
    correlate.add_argument("--force", action="store_true")
    correlate.add_argument("--index")
    correlate.set_defaults(func=cmd_memory_correlate)
    front_wing = sub.add_parser("memory-front-wing", help="generate the front-wing semantic-link pilot")
    front_wing.add_argument("--output")
    front_wing.add_argument("--force", action="store_true")
    front_wing.add_argument("--index")
    front_wing.set_defaults(func=cmd_memory_front_wing)
    memory_snapshot = sub.add_parser("memory-snapshot", help="capture deduplicated read-only process pages")
    memory_snapshot.add_argument("tag")
    memory_snapshot.add_argument("--process", default="F1 Challenge Online.exe")
    memory_snapshot.add_argument("--scope", choices=("module", "process"), default="module")
    memory_snapshot.set_defaults(func=cmd_memory_snapshot)
    memory_diff = sub.add_parser("memory-diff", help="compare memory-map-aware snapshots")
    memory_diff.add_argument("before")
    memory_diff.add_argument("after")
    memory_diff.add_argument("--transition", help="numeric BEFORE:AFTER, e.g. 0:1")
    memory_diff.add_argument("--width", type=int, choices=(1, 2, 4), default=1)
    memory_diff.add_argument("--limit", type=int, default=1000)
    memory_diff.add_argument("--output")
    memory_diff.add_argument("--force", action="store_true")
    memory_diff.set_defaults(func=cmd_memory_diff)
    runtime_observe = sub.add_parser("runtime-observe", help="passively validate or discover a live runtime field")
    runtime_observe.add_argument("recipe")
    runtime_observe.add_argument("tag")
    runtime_observe.add_argument("--mode", choices=("known", "discover"), default="known")
    runtime_observe.add_argument("--seconds", type=float, default=180.0)
    runtime_observe.add_argument("--hz", type=float, default=5.0)
    runtime_observe.add_argument("--process", default="F1 Challenge Online.exe")
    runtime_observe.add_argument("--vehicle-base", help="override relocated vehicle-array address")
    runtime_observe.add_argument("--limit", type=int, default=25)
    runtime_observe.add_argument("--recipes", default=str(RUNTIME_RECIPES))
    runtime_observe.set_defaults(func=cmd_runtime_observe)
    runtime_correlate = sub.add_parser("runtime-correlate", help="passively capture per-driver context at a live field transition")
    runtime_correlate.add_argument("trigger", help="known vehicle recipe whose changes create events")
    runtime_correlate.add_argument("tag")
    runtime_correlate.add_argument("--context", action="append", help="additional known vehicle recipe to capture at each event")
    runtime_correlate.add_argument("--also-trigger", action="append", help="additional known recipe whose changes create events on the same capture clock")
    runtime_correlate.add_argument("--seconds", type=float, default=180.0)
    runtime_correlate.add_argument("--hz", type=float, default=10.0)
    runtime_correlate.add_argument("--process", default="F1 Challenge Online.exe")
    runtime_correlate.add_argument("--vehicle-base", help="override relocated vehicle-array address")
    runtime_correlate.add_argument("--max-events", type=int, default=200)
    runtime_correlate.add_argument("--recipes", default=str(RUNTIME_RECIPES))
    runtime_correlate.set_defaults(func=cmd_runtime_correlate)
    pit_marker_read = sub.add_parser("pit-marker-read",help="read-only exact-target XPITIN marker diagnosis")
    pit_marker_read.add_argument("--process",default="F1 Challenge Online.exe")
    pit_marker_read.set_defaults(func=cmd_pit_marker_read)
    lifecycle_read = sub.add_parser("vehicle-lifecycle-read",help="read-only exact-target retired-car garage confirmation")
    lifecycle_read.add_argument("--process",default="F1 Challenge Online.exe")
    lifecycle_read.set_defaults(func=cmd_vehicle_lifecycle_read)

    runtime_read = sub.add_parser("runtime-read", help="read a one-shot named-field vehicle snapshot")
    runtime_read.add_argument("recipe", help="known vehicle recipe to read")
    runtime_read.add_argument("--context", action="append", help="additional known vehicle recipe to include")
    runtime_read.add_argument("--process", default="F1 Challenge Online.exe")
    runtime_read.add_argument("--vehicle-base", help="override relocated vehicle-array address")
    runtime_read.add_argument("--recipes", default=str(RUNTIME_RECIPES))
    runtime_read.add_argument("--json", action="store_true")
    runtime_read.set_defaults(func=cmd_runtime_read)
    runtime_validate = sub.add_parser("runtime-validate", help="re-score a saved read-only observation")
    runtime_validate.add_argument("recipe")
    runtime_validate.add_argument("observation")
    runtime_validate.add_argument("--limit", type=int, default=25)
    runtime_validate.add_argument("--recipes", default=str(RUNTIME_RECIPES))
    runtime_validate.add_argument("--json", action="store_true")
    runtime_validate.add_argument("--output")
    runtime_validate.add_argument("--force", action="store_true")
    runtime_validate.set_defaults(func=cmd_runtime_validate)
    variant = sub.add_parser("variant-profile", help="profile a PE executable without modifying it")
    variant.add_argument("exe")
    variant.add_argument("--address", help="optionally read bounded file-backed bytes at this VA")
    variant.add_argument("--length", type=int, help="byte count for --address (1..64)")
    variant.add_argument("--output")
    variant.add_argument("--force", action="store_true")
    variant.set_defaults(func=cmd_variant_profile)
    recipes = sub.add_parser("symbol-recipes-build", help="build canonical relocation recipes")
    recipes.add_argument("--exe", default=str(DEFAULT_EXE))
    recipes.add_argument("--output")
    recipes.add_argument("--force", action="store_true")
    recipes.set_defaults(func=cmd_symbol_recipes_build)
    resolver = sub.add_parser("resolve-symbols", help="resolve variant addresses from recipes")
    resolver.add_argument("exe")
    resolver.add_argument("--recipes", default=str(ROOT / "analysis" / "symbol_recipes.json"))
    resolver.add_argument("--output")
    resolver.add_argument("--force", action="store_true")
    resolver.set_defaults(func=cmd_resolve_symbols)
    for name, function in (("validate", cmd_validate), ("generate", cmd_generate),
                           ("build", cmd_build), ("trace", cmd_trace),
                           ("compare", cmd_compare), ("test", cmd_test),
                           ("pipeline", cmd_pipeline),
                           ("stage", cmd_stage), ("ida-prepare", cmd_ida_prepare),
                           ("discover", cmd_discover)):
        help_text = {
            "validate": "validate a sandbox manifest and target assumptions",
            "generate": "generate source from a validated manifest",
            "build": "build a validated sandbox mod",
            "trace": "capture a canonical runtime trace",
            "compare": "compare declared regression traces",
            "test": "run SDK and project test gates",
            "pipeline": "discover, test, and build a complete sandbox project",
            "stage": "stage a built mod with ownership protection",
            "ida-prepare": "prepare an isolated analysis database copy",
            "discover": "build an evidence dossier for declared seeds",
        }[name]
        command = sub.add_parser(name, help=help_text)
        command.add_argument("project")
        command.add_argument("--exe", default=str(DEFAULT_EXE))
        command.set_defaults(func=function)
        if name == "validate":
            command.add_argument("--json", action="store_true",
                                 help="report the first validation error as JSON")
        elif name == "discover":
            command.add_argument("--depth", type=int, default=2, choices=range(0, 6))
            command.add_argument("--callers", action="store_true")
        elif name == "trace":
            command.add_argument("--driver")
            command.add_argument("--output")
            command.add_argument("--print-only", action="store_true")
        elif name == "compare":
            command.add_argument("--name")
        elif name == "test":
            command.add_argument("--regressions", action="store_true")
        elif name == "pipeline":
            command.add_argument("--depth", type=int, default=2, choices=range(0, 6))
            command.add_argument("--callers", action="store_true")
            command.add_argument("--regressions", action="store_true")
        elif name == "stage":
            command.add_argument("destination")
            command.add_argument("--replace-sdk-owned", action="store_true")
            command.add_argument("--reuse-startup", action="store_true",
                                 help="reuse a hash-verified startup proxy locked by running games")
        elif name == "ida-prepare":
            command.add_argument("--source-idb", default=str(DEFAULT_IDB))
    ida_query = sub.add_parser("ida-query", help="read-only IDB query on a disposable copy (idalib)")
    ida_query.add_argument("operation", choices=("info", "decompile", "disasm",
                                                 "xrefs-to", "xrefs-from", "bytes"))
    ida_query.add_argument("address", help="0x4C6E80, 004C6E80 or sub_4C6E80")
    ida_query.add_argument("--idb", help="source IDB (default: newest typed lab IDB)")
    ida_query.add_argument("--max-lines", type=int, default=200)
    ida_query.add_argument("--count", type=int, default=40)
    ida_query.add_argument("--size", type=int, default=64)
    ida_query.add_argument("--json", action="store_true")
    ida_query.set_defaults(func=cmd_ida_query)
    ida_new = sub.add_parser("ida-new-project",
                             help="create an isolated IDB for a non-game PE target (viewer) with provenance")
    ida_new.add_argument("project")
    ida_new.add_argument("--exe", required=True,
                         help="external PE to analyze; the vendor file is only read and copied to the system TEMP directory")
    ida_new.add_argument("--name", help="IDB file name (default: <exe stem>.i64)")
    ida_new.set_defaults(func=cmd_ida_new_project)
    ida_survey = sub.add_parser("ida-survey",
                                help="one-pass read-only IDB survey: functions, strings, imports, exports, call edges")
    ida_survey.add_argument("--idb", required=True)
    ida_survey.add_argument("--output", required=True)
    ida_survey.add_argument("--min-string", type=int, default=4)
    ida_survey.add_argument("--max-strings", type=int, default=20000)
    ida_survey.add_argument("--max-functions", type=int, default=20000)
    ida_survey.set_defaults(func=cmd_ida_survey)
    ida_export = sub.add_parser("ida-export",
                                help="apply reviewed types/names to an IDB copy and export typed pseudocode")
    ida_export.add_argument("--output", required=True, help="new corpus directory; must not exist")
    ida_export.add_argument("--source-idb", default=str(LAB_IDB))
    ida_export.add_argument("--save-idb", help="optional new path for the typed IDB copy")
    ida_export.set_defaults(func=cmd_ida_export)
    load = sub.add_parser("load", help="stage a validated sandbox DLL and load it into the running canonical game")
    load.add_argument("project")
    load.add_argument("--exe", default=str(DEFAULT_EXE))
    load.add_argument("--destination", default=str(GAME_DIR))
    load.add_argument("--replace-sdk-owned", action="store_true")
    load.add_argument("--acknowledge-risk", action="store_true",
                      help="required because this stages a DLL and changes the running game")
    load.add_argument("--dry-run", action="store_true",
                      help="verify the target, process and loader without changing files or process state")
    load.add_argument("--elevate", action="store_true",
                      help="request a visible Windows UAC prompt for an elevated game")
    load.add_argument("--pid", type=int,
                      help="load only into this exact running game process")
    load.add_argument("--reuse-staged", action="store_true",
                      help="reuse hash-verified SDK-owned files already locked by another instance")
    load.set_defaults(func=cmd_load)
    track_status_gui = sub.add_parser("track-status-gui",
                                      help="launch the read-only Track Status Monitor GUI")
    track_status_gui.set_defaults(func=cmd_track_status_gui)
    load_broker = sub.add_parser("load-broker",
                                 help="load the reviewed SDK broker required by sandbox monitor clients")
    load_broker.add_argument("--exe", default=str(DEFAULT_EXE))
    load_broker.add_argument("--acknowledge-risk", action="store_true")
    load_broker.add_argument("--dry-run", action="store_true")
    load_broker.set_defaults(func=cmd_load_broker)
    camera_editor = sub.add_parser(
        "camera-editor",
        help="launch the live camera position editor GUI")
    camera_editor.add_argument("--cfg",
                               help="path to Default.cfg "
                                    "(default: game Save dir)")
    camera_editor.add_argument("--exe", action="append", default=[],
                               help="game process name to attach to")
    camera_editor.set_defaults(func=cmd_camera_editor)
    qualifying = sub.add_parser("qualifying-order", help="save and compare hash-verified qualifying classifications (read-only)")
    qualifying_actions = qualifying.add_subparsers(dest="action", required=True)
    for action in ("capture", "inspect", "compare"):
        operation = qualifying_actions.add_parser(action)
        operation.add_argument("--output", required=True, help="new JSON evidence path; overwrite is refused")
        if action == "capture":
            operation.add_argument("tag", help="new immutable module snapshot tag")
            operation.add_argument("--process", default="F1 Challenge Online.exe")
        elif action == "inspect":
            operation.add_argument("snapshot", help="existing memory-snapshot tag or manifest path")
        else:
            operation.add_argument("before", help="qualifying-order JSON from qualifying")
            operation.add_argument("after", help="qualifying-order JSON after transition")
        operation.set_defaults(func=cmd_qualifying_order)
    disasm=sub.add_parser("exe-disassemble",help="read-only bounded canonical executable ABI review")
    disasm.add_argument("--exe",required=True)
    disasm.add_argument("--address",type=lambda v:int(v,0),required=True)
    disasm.add_argument("--size",type=int,default=128)
    disasm.add_argument("--format",choices=("asm","u32","pointer-matches"),default="asm",
                        help="bounded code/table review or literal pointer matches (not proven code xrefs)")
    disasm.set_defaults(func=cmd_exe_disassemble)
    return parser

def cmd_exe_disassemble(args):
    import capstone
    path=Path(args.exe)
    if hashlib.sha256(path.read_bytes()).hexdigest().upper()!=TARGET_SHA256:
        raise ModError("disassembly review requires the canonical executable hash")
    if not 1<=args.size<=4096:
        raise ModError("size must be 1..4096")
    pe=VariantPEImage(path)
    if not pe.image_base <= args.address < pe.image_base + pe.image_size:
        raise ModError("address must be inside the canonical image")
    output_format=getattr(args,"format","asm")
    if output_format=="pointer-matches":
        needle=struct.pack("<I",args.address)
        matches=[]
        for section in pe.sections:
            start=section["raw_pointer"]
            data=pe.data[start:start+section["raw_size"]]
            offset=data.find(needle)
            while offset>=0:
                matches.append((pe.image_base+section["rva"]+offset,section["name"]))
                if len(matches)>256:
                    raise ModError("more than 256 literal pointer matches; review a narrower target")
                offset=data.find(needle,offset+1)
        print("Literal little-endian pointer matches; instruction/data ownership requires review.")
        for address,section in matches:
            print(f"{address:08X} {section}")
        return
    if output_format=="u32" and args.size%4:
        raise ModError("u32 size must be a multiple of four")
    data=pe.read_va(args.address,args.size)
    if output_format=="u32":
        for offset in range(0,len(data),4):
            print(f"{args.address+offset:08X} {struct.unpack_from('<I',data,offset)[0]:08X}")
        return
    decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    for insn in decoder.disasm(data,args.address):
        print(f"{insn.address:08X} {insn.bytes.hex():24s} {insn.mnemonic} {insn.op_str}")

def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    if Path.cwd().resolve() != ROOT.resolve():
        print("ERROR: run the F1C SDK only from its repository root:", ROOT, file=sys.stderr)
        return 2
    args = build_parser().parse_args()
    try:
        args.func(args)
    except (ModError, SourceExplorerError, LinkerError, SnapshotError, ValidationError,
            ResolverError, KeyError, OSError,
            subprocess.CalledProcessError) as error:
        print("ERROR:", error, file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
