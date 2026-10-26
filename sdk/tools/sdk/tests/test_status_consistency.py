#!/usr/bin/env python3
"""Focused tests for sdk-info ABI authority and doctor coverage fields."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
CLI = [sys.executable, str(ROOT / "tools" / "f1c_mod.py")]
sys.path.insert(0, str(ROOT / "tools"))
import f1c_mod

header_text = (ROOT / "tools" / "sdk" / "f1c_broker_abi.h").read_text(encoding="utf-8")
header_match = re.search(r"#define\s+F1C_BROKER_ABI_VERSION\s+(\d+)u?\b", header_text)
header_abi = int(header_match.group(1))
checkpoint_path = ROOT / "tools" / "sdk" / "AUTOMATION_CHECKPOINT.json"
checkpoint = json.loads(checkpoint_path.read_text(encoding="utf-8"))
checkpoint_abi = checkpoint.get("broker_abi_version")
checkpoint_hash_before = hashlib.sha256(checkpoint_path.read_bytes()).hexdigest()

info = subprocess.run([*CLI, "sdk-info", "--json"], cwd=ROOT, capture_output=True, text=True)
if info.returncode != 0:
    raise SystemExit(f"sdk-info --json failed: {info.stderr}")
info_json = json.loads(info.stdout)
if info_json["broker_abi"] != header_abi:
    raise SystemExit(f"broker_abi {info_json['broker_abi']} != header {header_abi}")
if info_json["checkpoint_broker_abi"] != checkpoint_abi:
    raise SystemExit("checkpoint_broker_abi does not match the checkpoint file")
conflicts = info_json["conflicts"]
if header_abi != checkpoint_abi:
    if not any(item.get("field") == "broker_abi" for item in conflicts):
        raise SystemExit("expected broker_abi conflict is missing")
else:
    if conflicts != []:
        raise SystemExit(f"unexpected conflicts: {conflicts}")

doctor = subprocess.run([*CLI, "doctor", "--json"], cwd=ROOT, capture_output=True, text=True)
if doctor.returncode not in (0, 1):
    raise SystemExit(f"doctor --json unexpected exit {doctor.returncode}: {doctor.stderr}")
doctor_json = json.loads(doctor.stdout)
for key in ("checks_run", "checks_not_run", "warnings"):
    if key not in doctor_json:
        raise SystemExit(f"doctor report missing key: {key}")

start = subprocess.run([*CLI, "start", "--json"], cwd=ROOT, capture_output=True, text=True)
if start.returncode != 0:
    raise SystemExit(f"start --json failed: {start.stderr}")
start_json = json.loads(start.stdout)
freshness = start_json["freshness"]
for key in ("updated_at", "newest_entry_key", "newest_entry_date", "dated_entries",
            "stale", "warning"):
    if key not in freshness:
        raise SystemExit(f"freshness missing key: {key}")
entries = start_json["recent_checkpoint_entries"]
if len(entries) > 5:
    raise SystemExit(f"recent_checkpoint_entries returned {len(entries)} items")
entry_dates = [item["date"] for item in entries]
if entry_dates != sorted(entry_dates, reverse=True):
    raise SystemExit(f"recent checkpoint entries not sorted by date descending: {entry_dates}")

dated_key = re.compile(r"(20\d{2})_?(\d{2})_?(\d{2})$")
dated_dates = []
for key, value in checkpoint.items():
    match = dated_key.search(key)
    if match and isinstance(value, dict):
        dated_dates.append("-".join(match.groups()))
newest = max(dated_dates) if dated_dates else None
if freshness["newest_entry_date"] != newest:
    raise SystemExit(
        f"freshness newest_entry_date {freshness['newest_entry_date']} != independent {newest}")
updated = str(checkpoint.get("updated_at", ""))[:10] or None
expected_stale = bool(updated and newest and newest > updated)
if freshness["stale"] != expected_stale:
    raise SystemExit(f"freshness stale {freshness['stale']} != expected {expected_stale}")

checkpoint_hash_after = hashlib.sha256(checkpoint_path.read_bytes()).hexdigest()
if checkpoint_hash_after != checkpoint_hash_before:
    raise SystemExit("CLI calls modified AUTOMATION_CHECKPOINT.json")

query = subprocess.run([*CLI, "sdk-query", "function", "Driver", "--limit", "1", "--json"],
                       cwd=ROOT, capture_output=True, text=True)
if query.returncode != 0:
    raise SystemExit(f"sdk-query failed: {query.stderr}")
query_json = json.loads(query.stdout)
if query_json["returned"] != 1:
    raise SystemExit(f"sdk-query returned {query_json['returned']}, expected 1")
if query_json["total"] < 1:
    raise SystemExit("sdk-query total is not at least 1")
if query_json["truncated"] != (query_json["total"] > 1):
    raise SystemExit("sdk-query truncated does not match total/returned")
if query_json["count"] != query_json["returned"]:
    raise SystemExit("sdk-query count != returned")

json_a = b'{"schema": "f1c.sdk_info.v1", "generated": "2026-09-30 12:00", "value": 1}'
json_b = b'{"schema": "f1c.sdk_info.v1", "generated": "2026-09-30 12:01", "value": 1}'
json_c = b'{"schema": "f1c.sdk_info.v1", "generated": "2026-09-30 12:00", "value": 2}'
digest = f1c_mod.artifact_digest
if digest("tools/sdk/f1c_sdk.json", json_a) != digest("tools/sdk/f1c_sdk.json", json_b):
    raise SystemExit("artifact_digest changed on a timestamp-only difference")
if digest("tools/sdk/f1c_sdk.json", json_a) == digest("tools/sdk/f1c_sdk.json", json_c):
    raise SystemExit("artifact_digest ignored a real JSON difference")
header_a = b"#define F1C_BROKER_ABI_VERSION 15u\n"
if digest("tools/sdk/f1c_sdk.h", header_a) != digest("tools/sdk/f1c_sdk.h", header_a):
    raise SystemExit("artifact_digest is not stable for identical .h bytes")
replaced = digest("x.json", b'\xff\xfe{"generated": "a"}')
if not (isinstance(replaced, str) and len(replaced) == 64
        and all(character in "0123456789abcdef" for character in replaced)):
    raise SystemExit(
        f"artifact_digest did not return a 64-character hex string on invalid UTF-8: {replaced!r}")

print("Status consistency passed: sdk-info ABI authority, conflicts, doctor coverage, and artifact digest verified.")
