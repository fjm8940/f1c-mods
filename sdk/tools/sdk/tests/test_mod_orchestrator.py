#!/usr/bin/env python3
"""End-to-end and fail-closed tests for tools/f1c_mod.py."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
ORCHESTRATOR = ROOT / "tools" / "f1c_mod.py"

def invoke(*arguments, ok=True):
    result = subprocess.run([sys.executable, str(ORCHESTRATOR), *map(str, arguments)],
                            cwd=ROOT, text=True, capture_output=True)
    if ok and result.returncode:
        raise SystemExit(result.stdout + result.stderr)
    if not ok and result.returncode == 0:
        raise SystemExit("command unexpectedly succeeded: " + " ".join(map(str, arguments)))
    return result

# Source exploration reads function bodies directly, not just the reduced index.
source_hit = invoke("search", "memset((void *)this, 0, 0x18u);",
                    "--corpus", "raw", "--limit", "3")
if "functions/AABBCollisionBodyInitializer_00448e90.c" not in source_hit.stdout:
    raise SystemExit("raw full-body source search did not return the expected function")
source_json = json.loads(invoke("search", "memset((void *)this, 0, 0x18u);",
                                "--corpus", "raw", "--limit", "1", "--json").stdout)
if source_json[0]["address"] != "0x00448E90" or source_json[0]["sources"][0]["corpus"] != "raw":
    raise SystemExit("machine-readable raw source result lost address/provenance")
profile_with_bytes = json.loads(invoke(
    "variant-profile", Path(os.environ.get("F1C_SDK_TEST_EXE", str(ROOT.parent / "F1 Challenge V10" / "F1 Challenge Online.exe"))),
    "--address", "0x0042C690", "--length", "6").stdout)
if profile_with_bytes.get("code_bytes") != {
        "address": "0x0042C690", "length": 6, "hex": "55 8B EC 83 E4 F8"}:
    raise SystemExit("bounded variant code-byte evidence changed unexpectedly")
inspection = invoke("inspect", "0x00448E90", "--max-lines", "15")
for expected in ("RAW_HEXRAYS", "memset((void *)this", "DERIVED_PROSE", "LEGACY"):
    if expected not in inspection.stdout:
        raise SystemExit(f"source inspection missing provenance/context: {expected}")
translated = invoke("search", "front-wing", "--corpus", "translated", "--limit", "1")
if "DERIVED_PROSE" not in translated.stdout:
    raise SystemExit("translated alias did not preserve DERIVED provenance")
invoke("search", "[", "--regex", ok=False)

with tempfile.TemporaryDirectory(prefix="f1c_mod_orchestrator_") as temporary:
    semantic_index = Path(temporary) / "semantic_memory_index.json"
    invoke("memory-build", "--output", semantic_index)
    semantic = json.loads(semantic_index.read_text(encoding="utf-8"))
    if semantic["summary"]["fingerprinted_functions"] < 4000:
        raise SystemExit("semantic linker omitted most static fingerprints")
    if semantic["summary"]["derived_identity_proposals"] > 100:
        raise SystemExit("identity propagation escaped its one-hop reviewed boundary")
    fingerprint = json.loads(invoke("memory-fingerprint", "AABBCollisionBodyInitializer",
                                    "--index", semantic_index, "--json").stdout)
    explicit_types = {field.get("write_type") for group in fingerprint["bases"]
                      for field in group["fields"] if field.get("width_evidence")}
    if explicit_types != {"uint8", "uint16", "uint32"}:
        raise SystemExit("raw-width evidence was not preserved in the fingerprint")

    # A synthetic runtime capture verifies exact multi-field matching without a game write.
    memory = bytearray(0x80)
    base = 0x20
    memory[base + 24] = 1
    memory[base + 25] = 0
    memory[base + 26:base + 28] = (0xFFFF).to_bytes(2, "little")
    memory[base + 28:base + 32] = (0).to_bytes(4, "little")
    capture = Path(temporary) / "layout_capture.json"
    capture.write_text(json.dumps({"struct": "test", "live": [1], "slots": {
        "1": {"flat": memory.hex(), "children": {}}
    }}), encoding="utf-8")
    matched = json.loads(invoke("memory-match", "AABBCollisionBodyInitializer", capture,
                                "--index", semantic_index, "--slot", "1", "--json").stdout)
    if matched["total_matches"] != 1 or matched["matches"][0]["base_offset"] != "0x20":
        raise SystemExit("multi-field runtime layout match was not exact")
    correlation = json.loads(invoke("memory-correlate", "t2_slot6", "--control", "restart_check",
                                    "--limit", "50", "--index", semantic_index, "--json").stdout)
    correlated_paths = {item["path"] for item in correlation["ranked_paths"]}
    if "+0x0D14" not in correlated_paths or "+0x0CD4->+0x0020" not in correlated_paths:
        raise SystemExit("front-wing flat/child event paths were not correlated")
    pilot = Path(temporary) / "front_wing.md"
    invoke("memory-front-wing", "--index", semantic_index, "--output", pilot)
    if "No direct static writer was extracted" not in pilot.read_text(encoding="utf-8"):
        raise SystemExit("front-wing pilot did not preserve the known evidence gap")

    project = Path(temporary) / "large_mod_probe"
    invoke("init", "large_mod_probe", "--title", "Large Mod Probe", "--output", project)
    manifest_path = project / "f1c_mod.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["subsystems"] = ["pit", "ai", "vehicle"]
    manifest["seeds"] = ["RequestPitEntry", "PitStateUpdateMachine"]
    manifest["writes"] = [{"object": "F1CVehicle", "field": "CallToBox"}]
    manifest["tracing"]["driver"] = "Bernd Maylander"
    manifest["tracing"]["pit_cycle"] = True
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    invoke("validate", project)
    invoke("discover", project, "--depth", "1", "--callers")
    invoke("generate", project)
    invoke("build", project)
    invoke("trace", project, "--print-only")
    for required in (project / "build" / "large_mod_probe.dll",
                     project / "generated" / "discovery.json",
                     project / "generated" / "mod_bindings.hpp",
                     project / "generated" / "hook_plan.json"):
        if not required.exists():
            raise SystemExit(f"orchestrator artifact missing: {required}")

    # Exact reviewed hook bytes pass; duplicate/overlapping hook ownership fails.
    hook = {"function": "RequestPitEntry", "handler": "probe_hook",
            "stolen_length": 7, "expected_bytes": "83 3D A4 56 7C 00 07"}
    manifest["hooks"] = [hook]
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    invoke("validate", project)
    invoke("generate", project)
    generated_smoke = project / "generated_smoke.cpp"
    generated_smoke.write_text(
        '#include "generated/mod_bindings.hpp"\n'
        '#include "generated/hooks.hpp"\n'
        '#include "generated/commands.hpp"\n'
        'int generated_smoke() { return 0; }\n', encoding="utf-8")
    compiler = Path(r"C:\msys64\mingw32\bin\g++.exe")
    subprocess.run([str(compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "tools" / "sdk"), "-I", str(project),
                    "-c", str(generated_smoke), "-o", str(project / "generated_smoke.o")],
                   cwd=ROOT, check=True)
    manifest["hooks"] = [hook, dict(hook)]
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    invoke("validate", project, ok=False)

    # Unknown ABI and output-state writes are rejected independently.
    manifest["hooks"] = [{"function": "PitCrewAnimationLoader",
                           "handler": "bad", "stolen_length": 5,
                           "expected_bytes": "00 00 00 00 00"}]
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    invoke("validate", project, ok=False)
    manifest["hooks"] = []
    manifest["writes"] = [{"object": "F1CVehicle", "field": "PitStatus"}]
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    invoke("validate", project, ok=False)

    # Regression comparison is a gate, not merely a pretty diff.
    trace_dir = project / "traces"
    trace_dir.mkdir(exist_ok=True)
    event = {"event": "field_change", "name": "PitStatus",
             "semantic": "output", "before": 2, "after": 3}
    (trace_dir / "vanilla.jsonl").write_text(json.dumps(event) + "\n", encoding="utf-8")
    (trace_dir / "rewrite.jsonl").write_text(json.dumps(event) + "\n", encoding="utf-8")
    manifest["writes"] = []
    manifest["regressions"] = [{"name": "pit", "vanilla": "traces/vanilla.jsonl",
                                  "rewrite": "traces/rewrite.jsonl",
                                  "fields": ["PitStatus"], "require_match": True}]
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    invoke("compare", project, "--name", "pit")
    different = {**event, "after": 4}
    (trace_dir / "rewrite.jsonl").write_text(json.dumps(different) + "\n", encoding="utf-8")
    invoke("compare", project, "--name", "pit", ok=False)

# Existing mod trees are never valid init destinations.
protected = ROOT / "mods" / "orchestrator_must_not_create"
if protected.exists():
    raise SystemExit(f"test precondition violated: {protected} exists")
invoke("init", "orchestrator_must_not_create", "--output", protected, ok=False)
if protected.exists():
    raise SystemExit("orchestrator created a project inside the existing mod tree")

print("Mod orchestrator passed: raw search/inspect, semantic memory linking, init/discover/generate/build/trace-plan and all fail-closed gates.")
