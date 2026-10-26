#!/usr/bin/env python3
"""Build an auditable call/offset/object index from decompiled function files."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
FUNCTIONS = ROOT / "functions"
OUT = ROOT / "analysis" / "recovery_index.json"
reviewed_path = ROOT / "tools" / "sdk" / "vehicle_retype_reviewed.json"
pit_reviewed_path = ROOT / "tools" / "sdk" / "pit_retype_reviewed.json"

file_pattern = re.compile(r"^(?P<label>.+)_(?P<address>[0-9a-fA-F]{8})\.c$")
call_pattern = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
offset_patterns = (
    re.compile(r"\bthis\s*\+\s*(\d+)"),
    re.compile(r"\bthis\s*\[\s*(\d+)\s*\]"),
)
declaration_pattern = re.compile(
    r"^[^\n;{}]*?(?:__thiscall|__cdecl|__stdcall)?\s+([A-Za-z_]\w*)\s*\([^;]*\)\s*\{",
    re.MULTILINE)
ignored_calls = {"if", "for", "while", "switch", "return", "sizeof", "LODWORD",
                 "HIDWORD", "BYTE1", "BYTE2", "BYTE3", "WORD1", "__PAIR64__"}

nodes = {}
aliases = {}
texts = {}
for path in sorted(FUNCTIONS.glob("*.c")):
    match = file_pattern.match(path.name)
    if not match:
        continue
    label = match.group("label")
    address = int(match.group("address"), 16)
    text = path.read_text(encoding="utf-8", errors="ignore")
    declaration = declaration_pattern.search(text)
    declared_name = declaration.group(1) if declaration else label
    key = f"0x{address:08X}"
    nodes[key] = {
        "address": key,
        "name": label,
        "declared_name": declared_name,
        "source": str(path.relative_to(ROOT)).replace("\\", "/"),
        "callees": [], "callers": [], "this_offsets": [],
        "object_kind": None, "object_trust": None, "object_evidence": [],
    }
    texts[key] = text
    for alias in {label, declared_name}:
        aliases.setdefault(alias, []).append(key)

unique_alias = {name: keys[0] for name, keys in aliases.items() if len(keys) == 1}
for key, node in nodes.items():
    text = texts[key]
    callees = set()
    for called in call_pattern.findall(text):
        target = unique_alias.get(called)
        if target and target != key and called not in ignored_calls:
            callees.add(target)
    node["callees"] = sorted(callees)

    offsets = {int(value) for value in offset_patterns[0].findall(text)}
    offsets.update(int(value) * 4 for value in offset_patterns[1].findall(text))
    node["this_offsets"] = [f"0x{value:X}" for value in sorted(offsets)]

for caller, node in nodes.items():
    for callee in node["callees"]:
        nodes[callee]["callers"].append(caller)
for node in nodes.values():
    node["callers"].sort()

if reviewed_path.exists():
    reviewed = json.loads(reviewed_path.read_text(encoding="utf-8"))
    for entry in reviewed.get("approved", []):
        node = nodes.get(entry["addr"].upper().replace("0X", "0x"))
        if node:
            node["object_kind"] = "F1CVehicle"
            node["object_trust"] = "CONFIRMED"
            node["object_evidence"].append(entry["evidence"])
if pit_reviewed_path.exists():
    reviewed = json.loads(pit_reviewed_path.read_text(encoding="utf-8"))
    for entry in reviewed.get("approved", []):
        address = entry["addr"].upper().replace("0X", "0x")
        if address in nodes:
            nodes[address]["object_kind"] = "F1CPitMachine"
            nodes[address]["object_trust"] = "CONFIRMED"
            nodes[address]["object_evidence"].append(entry["evidence"])

# Keep same-this propagation as explicit review leads; never promote it to a type.
proposals = []
for caller, node in nodes.items():
    if not node["object_kind"]:
        continue
    text = texts[caller]
    for callee in node["callees"]:
        target = nodes[callee]
        if target["object_kind"]:
            continue
        aliases_for_target = {target["name"], target["declared_name"]}
        if any(re.search(rf"\b{re.escape(alias)}\s*\(\s*(?:\([^)]*\)\s*)?this\b", text)
               for alias in aliases_for_target):
            proposals.append({
                "caller": caller, "callee": callee,
                "proposed_object_kind": node["object_kind"],
                "trust": "DERIVED",
                "reason": "reviewed owner method appears to forward unchanged `this`",
            })

subsystems = {}
keywords = {
    "pit": ("pit", "garage", "tire", "tyre", "fuel"),
    "ai": ("ai", "racingline", "waypoint", "steer"),
    "vehicle": ("vehicle", "physics", "collision", "engine"),
    "session": ("session", "race", "qual", "grid"),
    "render": ("render", "camera", "scene", "hud"),
    "audio": ("audio", "sound", "commentary"),
}
for key, node in nodes.items():
    lower = node["name"].lower()
    tags = [group for group, words in keywords.items() if any(word in lower for word in words)]
    if node["object_kind"] == "F1CPitMachine" and "pit" not in tags:
        tags.append("pit")
    node["subsystems"] = tags
    for tag in tags:
        subsystems.setdefault(tag, []).append(key)

OUT.parent.mkdir(parents=True, exist_ok=True)
payload = {
    "schema_version": 1,
    "source": "functions/*.c",
    "policy": "Object proposals are DERIVED review leads and are never auto-applied to IDA.",
    "summary": {
        "functions": len(nodes),
        "call_edges": sum(len(node["callees"]) for node in nodes.values()),
        "reviewed_object_methods": sum(bool(node["object_kind"]) for node in nodes.values()),
        "same_this_proposals": len(proposals),
    },
    "subsystems": {key: sorted(value) for key, value in subsystems.items()},
    "ownership_proposals": proposals,
    "functions": nodes,
}
OUT.write_text(json.dumps(payload, indent=1) + "\n", encoding="utf-8")
print("recovery index:", payload["summary"], "->", OUT.relative_to(ROOT))
