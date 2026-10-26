#!/usr/bin/env python3
"""Focused tests for RECORDED checkpoint entries in evidence-query findings."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools" / "f1c_mod.py")]
FINDINGS = ROOT / "findings.jsonl"

before = hashlib.sha256(FINDINGS.read_bytes()).hexdigest()

result = subprocess.run([*CLI, "evidence-query", "findings", "replay"],
                        cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
if result.returncode != 0:
    raise SystemExit(f"evidence-query findings replay failed: {result.stderr}")
if "[RECORDED]" not in result.stdout:
    raise SystemExit("recorded replay findings missing")
if "AUTOMATION_CHECKPOINT.json#" not in result.stdout:
    raise SystemExit("recorded source link missing")

result = subprocess.run([*CLI, "evidence-query", "findings", "QuickRace"],
                        cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
if result.returncode != 0:
    raise SystemExit(f"evidence-query findings QuickRace failed: {result.stderr}")
if not any(line.startswith(("[FACT]", "[CONFIRMED]", "[DISPROVEN]"))
           for line in result.stdout.splitlines()):
    raise SystemExit("curated findings no longer appear")

json_result = subprocess.run([*CLI, "evidence-query", "findings", "replay", "--json"],
                             cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
if json_result.returncode != 0:
    raise SystemExit(f"evidence-query findings replay --json failed: {json_result.stderr}")
payload = json.loads(json_result.stdout)
if payload["schema"] != "f1c.findings_query.v1":
    raise SystemExit(f"unexpected findings JSON schema: {payload['schema']}")
if payload["recorded"] < 1:
    raise SystemExit("recorded count is zero in JSON findings")
if payload["reviewed"] + payload["recorded"] != payload["total"]:
    raise SystemExit("reviewed + recorded != total")
if payload["total"] != len(payload["findings"]):
    raise SystemExit("total != len(findings)")
if payload["legend"].get("RECORDED") != ("copied from the automation checkpoint; "
                                         "not tier-reviewed; verify via its evidence link."):
    raise SystemExit("recorded legend missing from JSON findings")

rejected = subprocess.run([*CLI, "evidence-query", "offset", "0x4620", "--json"],
                          cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
if rejected.returncode == 0:
    raise SystemExit("--json must be refused for evidence-query offset")

for extra in (["offset", "0x4620"], ["stats"]):
    banner = subprocess.run([*CLI, "evidence-query", *extra], cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace")
    if banner.returncode != 0:
        raise SystemExit(f"evidence-query {' '.join(extra)} failed: {banner.stderr}")
    if "+ [canonical CLI internal operation]" in banner.stdout:
        raise SystemExit(f"internal banner leaked into evidence-query {' '.join(extra)} stdout")

after = hashlib.sha256(FINDINGS.read_bytes()).hexdigest()
if after != before:
    raise SystemExit("findings.jsonl was modified")

print("Ask recorded findings passed: checkpoint entries shown as RECORDED, JSON mode verified, findings.jsonl untouched.")
