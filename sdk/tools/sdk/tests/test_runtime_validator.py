#!/usr/bin/env python3
"""Deterministic tests for passive behavioral field validation."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CLI = ROOT / "tools" / "f1c_mod.py"


def candidate(offset, *, changes, allowed, monotonic, comparisons=100,
              unchanged=94, values=104, in_range=104, slots=4, distinct=3):
    return {"offset": offset, "values": values, "in_range": in_range,
            "comparisons": comparisons, "unchanged": unchanged,
            "changes": changes, "allowed_changes": allowed,
            "monotonic_changes": monotonic, "slots": slots,
            "distinct_values": distinct, "sample_values": [0, 1, 2],
            "transitions": []}


with tempfile.TemporaryDirectory(prefix="f1c_runtime_validator_") as temporary:
    observation = {
        "schema_version": 1, "recipe": "lapNumber",
        "metadata": {"mode": "discover", "samples": 26},
        "candidate_stats": [
            candidate("0x403C", changes=6, allowed=6, monotonic=6),
            candidate("0x4010", changes=6, allowed=0, monotonic=3,
                      unchanged=94, in_range=60),
            candidate("0x4040", changes=0, allowed=0, monotonic=0,
                      unchanged=100, distinct=1),
        ]}
    path = Path(temporary) / "observation.json"
    path.write_text(json.dumps(observation), encoding="utf-8")
    result = subprocess.run([sys.executable, str(CLI), "runtime-validate", "lapNumber",
                             str(path), "--json"], cwd=ROOT, text=True,
                            capture_output=True, check=True)
    report = json.loads(result.stdout)
    known = report["known_offset_result"]
    if known["offset"] != "0x403C" or known["verdict"] != "STRONGLY_SUPPORTED":
        raise SystemExit(f"known lap field was not strongly supported: {known}")
    if report["ranked_candidates"][0]["offset"] != "0x403C":
        raise SystemExit("behavioral discovery did not rank the lap-like field first")
    static = next(item for item in report["ranked_candidates"] if item["offset"] == "0x4040")
    if static["verdict"] != "INCONCLUSIVE_NO_NATURAL_TRANSITION":
        raise SystemExit("a static low integer was incorrectly treated as validated")
    if report["read_only"] is not True or "does not authorize" not in report["policy"]:
        raise SystemExit("read-only/write-safety policy was lost")

    engine_observation = {
        "schema_version": 1, "recipe": "EngineAngularSpeed",
        "metadata": {"mode": "known", "samples": 12},
        "candidate_stats": [
            candidate("0x39A0", changes=30, allowed=0, monotonic=30,
                      comparisons=33, unchanged=3, values=36, in_range=36,
                      slots=6, distinct=8),
        ]}
    engine_path = Path(temporary) / "engine_observation.json"
    engine_path.write_text(json.dumps(engine_observation), encoding="utf-8")
    engine_result = subprocess.run([sys.executable, str(CLI), "runtime-validate",
                                    "EngineAngularSpeed", str(engine_path), "--json"],
                                   cwd=ROOT, text=True, capture_output=True, check=True)
    engine_report = json.loads(engine_result.stdout)
    engine_known = engine_report["known_offset_result"]
    if engine_known["offset"] != "0x39A0" or engine_known["verdict"] != "SUPPORTED":
        raise SystemExit(f"engine angular speed recipe did not validate: {engine_known}")
    if not any("SpeedMultiplier" in item for item in engine_report["static_evidence"]):
        raise SystemExit("engine recipe lost its SpeedMultiplier static evidence")
    if not any("does not authorize writing" in item for item in engine_report["limitations"]):
        raise SystemExit("engine recipe lost its read-only limitation")

print("Runtime validator passed: known-field validation, candidate ranking, inconclusive gating, and the EngineAngularSpeed recipe.")
