#!/usr/bin/env python3
"""Join static object layouts to runtime captures without promoting guesses."""
from pathlib import Path
from collections import defaultdict
import hashlib
import json
import re
import struct


class LinkerError(RuntimeError):
    pass


def load_json(path, default=None):
    if not path.exists():
        return default
    return json.loads(path.read_text(encoding="utf-8", errors="replace"))


def norm_address(value):
    text = str(value).lower().replace("0x", "")
    return f"0x{int(text, 16):08X}"


def norm_offset(value):
    return int(str(value), 0)


def load_inputs(root):
    persistence = {}
    persistence_path = root / "persistence_map.jsonl"
    if persistence_path.exists():
        for line in persistence_path.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.strip() and not line.lstrip().startswith("//"):
                item = json.loads(line)
                persistence[int(item["off"], 16)] = item
    return {
        "facts": load_json(root / "skeleton_facts_all.json", {}),
        "recovery": load_json(root / "analysis" / "recovery_index.json", {}),
        "names": load_json(root / "name_index.json", {}),
        "runtime": load_json(root / "runtime_facts.json", {}),
        "strings": load_json(root / "strings_index.json", {}),
        "schema": load_json(root / "config_schema.json", {}),
        "persistence": persistence,
    }


def _raw_path(root, node):
    source = node.get("source")
    return root / source if source else None


def _same_this_edges(root, recovery):
    edges = []
    functions = recovery.get("functions", {})
    for caller, node in functions.items():
        path = _raw_path(root, node)
        if not path or not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for callee in node.get("callees", []):
            target = functions.get(callee, {})
            aliases = {target.get("name", ""), target.get("declared_name", "")}
            aliases.discard("")
            if any(re.search(rf"\b{re.escape(alias)}\s*\(\s*(?:\([^)]*\)\s*)?this\b", text)
                   for alias in aliases):
                edges.append((caller, callee))
    return edges


def propagate_identities(root, recovery):
    identities = {}
    for address, node in recovery.get("functions", {}).items():
        if node.get("object_kind"):
            identities[address] = {
                "kind": node["object_kind"], "trust": node.get("object_trust", "CONFIRMED"),
                "confidence": 1.0, "evidence": ["reviewed object-method anchor"],
            }
    edges = _same_this_edges(root, recovery)
    # Deliberately one-hop and forward-only. Reverse propagation through a generic
    # helper (free/copy/format) launders one caller's type into every other caller.
    anchors = dict(identities)
    proposals = defaultdict(list)
    for caller, callee in edges:
        if caller in anchors and callee not in anchors:
            source = anchors[caller]
            proposals[callee].append((source["kind"], 0.72,
                                      f"reviewed {caller} forwards unchanged this"))
    for address, candidates in proposals.items():
        kinds = {item[0] for item in candidates}
        if len(kinds) != 1:
            continue
        identities[address] = {
            "kind": candidates[0][0], "trust": "DERIVED", "confidence": 0.72,
            "evidence": sorted({item[2] for item in candidates}),
        }
    return identities, edges


def _schema_for_address(schema, address):
    suffix = address.lower().replace("0x", "")
    output = []
    for parser, metadata in schema.get("parsers", {}).items():
        if not parser.lower().endswith(suffix):
            continue
        for section, fields in metadata.get("sections", {}).items():
            for field in fields:
                output.append({"section": section, **field})
    return output


def _vehicle_name(names, offset):
    fields = names.get("vehicle_fields", {})
    for spelling in (f"0x{offset:X}", f"0x{offset:x}"):
        if spelling in fields:
            return fields[spelling]
    return None


def _explicit_raw_writes(path):
    """Extract only writes whose width, offset and literal are explicit in Hex-Rays C."""
    if not path or not path.exists():
        return {}
    text = path.read_text(encoding="utf-8", errors="ignore")
    pattern = re.compile(
        r"\*\(_(BYTE|WORD|DWORD)\s*\*\)\(\s*this\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\s*\)"
        r"\s*=\s*(-?(?:0x[0-9A-Fa-f]+|\d+))\s*;")
    kinds = {"BYTE": "uint8", "WORD": "uint16", "DWORD": "uint32"}
    output = {}
    for width, offset, literal in pattern.findall(text):
        output[("this", int(offset, 0))] = {
            "write_type": kinds[width], "raw_init_literal": int(literal, 0),
            "width_evidence": "explicit Hex-Rays store",
        }
    return output


def build_index(root):
    data = load_inputs(root)
    recovery = data["recovery"]
    identities, edges = propagate_identities(root, recovery)
    runtime_fields = {int(item["off"], 16): item
                      for item in data["runtime"].get("vehicle", {}).get("fields", [])}
    functions = {}
    for raw_address, fact in data["facts"].items():
        address = norm_address(raw_address)
        config = _schema_for_address(data["schema"], address)
        if not fact.get("fields") and not config:
            continue
        groups = defaultdict(list)
        for field in fact["fields"]:
            item = dict(field)
            offset = int(item["off_hex"], 16)
            item["offset"] = offset
            groups[item.get("base", "unknown")].append(item)
        node = recovery.get("functions", {}).get(address, {})
        identity = identities.get(address)
        explicit_writes = _explicit_raw_writes(_raw_path(root, node))
        group_output = []
        for base, fields in sorted(groups.items()):
            fields.sort(key=lambda item: item["offset"])
            normalized = []
            for field in fields:
                entry = {key: value for key, value in field.items() if key != "offset"}
                offset = field["offset"]
                if (base, offset) in explicit_writes:
                    entry.update(explicit_writes[(base, offset)])
                if identity and identity["kind"] == "F1CVehicle" and base == "this":
                    name = _vehicle_name(data["names"], offset)
                    if name:
                        entry["semantic_name"] = name.get("name")
                        entry["semantic_trust"] = name.get("trust")
                    if offset in runtime_fields:
                        entry["runtime"] = runtime_fields[offset]
                    if offset in data["persistence"]:
                        entry["persistence"] = data["persistence"][offset].get("class")
                normalized.append(entry)
            signature_material = ";".join(
                f"{item['off_hex']}:{item.get('rw')}:{item.get('init_literal', '-') }"
                for item in normalized)
            group_output.append({
                "base": base, "field_count": len(normalized),
                "span": f"0x{fields[-1]['offset'] - fields[0]['offset']:X}",
                "constant_count": sum("init_literal" in item for item in normalized),
                "fingerprint": hashlib.sha256(signature_material.encode()).hexdigest()[:16],
                "fields": normalized,
            })
        string_rows = data["strings"].get("by_function", {}).get(raw_address.lower(), [])
        functions[address] = {
            "name": node.get("name") or fact.get("name"),
            "declared_name": node.get("declared_name") or fact.get("name"),
            "fact_name": fact.get("name"), "source": node.get("source"),
            "identity": identity,
            "bases": group_output,
            "strings": [row.get("s") for row in string_rows],
            "config_fields": config,
            "callers": node.get("callers", []), "callees": node.get("callees", []),
        }
    summary = {
        "fingerprinted_functions": len(functions),
        "confirmed_identities": sum(item.get("trust") in ("FACT", "CONFIRMED")
                                    for item in identities.values()),
        "derived_identity_proposals": sum(item.get("trust") == "DERIVED"
                                          for item in identities.values()),
        "same_this_edges": len(edges),
        "config_fields": sum(len(item["config_fields"]) for item in functions.values()),
    }
    return {
        "schema_version": 1,
        "policy": "DERIVED identity propagation is a review lead and is never auto-promoted or written to the game.",
        "summary": summary, "functions": functions,
    }


def resolve_function(index, value):
    text = str(value).strip()
    if re.fullmatch(r"(?:0x)?[0-9a-fA-F]{8}", text):
        address = norm_address(text)
        if address in index["functions"]:
            return address
        raise LinkerError(f"no fingerprinted function at {address}")
    needle = text.casefold()
    hits = [address for address, item in index["functions"].items()
            if needle in {(item.get("name") or "").casefold(),
                          (item.get("declared_name") or "").casefold(),
                          (item.get("fact_name") or "").casefold()}]
    if len(hits) != 1:
        raise LinkerError(f"function must resolve uniquely: {value} ({len(hits)} matches)")
    return hits[0]


def render_fingerprint(index, value):
    address = resolve_function(index, value)
    item = index["functions"][address]
    identity = item.get("identity") or {}
    lines = [f"# Memory fingerprint: {item['name']} {address}", "",
             f"Identity: `{identity.get('kind', 'unknown')}` / `{identity.get('trust', 'UNKNOWN')}` "
             f"(confidence {identity.get('confidence', 0):.2f})", "",
             "DERIVED identity is a review lead, not a type promotion.", ""]
    if identity.get("evidence"):
        lines += ["Evidence:"] + [f"- {value}" for value in identity["evidence"]] + [""]
    for group in item["bases"]:
        lines += [f"## Base `{group['base']}`", "",
                  f"{group['field_count']} fields · span {group['span']} · "
                  f"{group['constant_count']} constants · fingerprint `{group['fingerprint']}`", "",
                  "| Offset | R/W | Constant | Semantic | Runtime | Persistence |",
                  "|---|---|---|---|---|---|"]
        for field in group["fields"]:
            constant = field.get("init_float", field.get("init_literal", ""))
            runtime = field.get("runtime", {}).get("kind", "")
            semantic = field.get("semantic_name", "")
            if field.get("semantic_trust"):
                semantic += f" [{field['semantic_trust']}]"
            lines.append(f"| {field['off_hex']} | {field.get('rw', '')} | {constant} | "
                         f"{semantic} | {runtime} | {field.get('persistence', '')} |")
        lines.append("")
    if item["config_fields"]:
        lines += ["## Configuration tokens", ""]
        for field in item["config_fields"]:
            lines.append(f"- `{field.get('token')}` → {', '.join(field.get('offsets', []))} "
                         f"via `{field.get('setter')}` ({field.get('assoc')})")
        lines.append("")
    return "\n".join(lines)


def _constraint_bytes(constraint):
    kind = constraint.get("type", "uint32")
    value = constraint.get("value")
    if kind == "hex":
        return bytes.fromhex(str(value).replace(" ", ""))
    if kind == "float":
        return struct.pack("<f", float(value))
    if kind == "uint8":
        return struct.pack("<B", int(value) & 0xFF)
    if kind == "uint16":
        return struct.pack("<H", int(value) & 0xFFFF)
    if kind == "int32":
        return struct.pack("<i", int(value))
    return struct.pack("<I", int(value) & 0xFFFFFFFF)


def signature_from_fingerprint(index, value, base=None):
    address = resolve_function(index, value)
    groups = index["functions"][address]["bases"]
    if base is not None:
        groups = [group for group in groups if group["base"] == base]
    candidates = []
    for group in groups:
        constraints = []
        for field in group["fields"]:
            if "raw_init_literal" in field and field.get("write_type"):
                constraints.append({"offset": field["off_hex"], "type": field["write_type"],
                                    "value": field["raw_init_literal"],
                                    "evidence": field["width_evidence"]})
        if len(constraints) >= 2:
            candidates.append((len(constraints), group["base"], constraints))
    if not candidates:
        raise LinkerError("fingerprint has fewer than two extracted constants; provide a signature JSON")
    _, chosen_base, constraints = max(candidates)
    return {"name": index["functions"][address]["name"], "base": chosen_base,
            "trust": "EXTRACTED", "constraints": constraints}


def load_signature(path):
    signature = load_json(path)
    if not signature or len(signature.get("constraints", [])) < 2:
        raise LinkerError("signature needs at least two constraints")
    if not any(any(byte != 0 for byte in _constraint_bytes(item))
               for item in signature["constraints"]):
        raise LinkerError("all-zero signatures are too ambiguous")
    return signature


def _capture_path(root, value):
    path = Path(value)
    if not path.is_absolute() and len(path.parts) == 1:
        path = root / "runtime_captures" / f"{value}.json"
    return path.resolve()


def match_capture(root, signature, capture_value, slot=None, limit=25):
    capture_path = _capture_path(root, capture_value)
    capture = load_json(capture_path)
    if not capture:
        raise LinkerError(f"runtime capture not found: {capture_path}")
    constraints = []
    for item in signature["constraints"]:
        constraints.append((norm_offset(item["offset"]), _constraint_bytes(item), item))
    max_end = max(offset + len(expected) for offset, expected, _ in constraints)
    targets = []
    for slot_name, snapshot in capture.get("slots", {}).items():
        if slot is not None and int(slot_name) != slot:
            continue
        targets.append((int(slot_name), "flat", bytes.fromhex(snapshot["flat"])))
        for child, content in snapshot.get("children", {}).items():
            targets.append((int(slot_name), f"child {child}", bytes.fromhex(content)))
    results = []
    for slot_number, target, content in targets:
        for base in range(0, max(0, len(content) - max_end + 1), 4):
            matches = []
            for offset, expected, original in constraints:
                if content[base + offset:base + offset + len(expected)] == expected:
                    matches.append(original)
            if len(matches) == len(constraints):
                results.append({
                    "slot": slot_number, "target": target, "base_offset": f"0x{base:X}",
                    "matched": len(matches), "constraints": len(constraints),
                    "trust": "OBSERVED_CANDIDATE",
                })
    return {"signature": signature, "capture": str(capture_path),
            "matches": results[:limit], "total_matches": len(results)}


def correlate_event(root, index, label, controls=None, limit=50):
    data = load_inputs(root)
    deltas = data["runtime"].get("vehicle", {}).get("behavior_deltas", {})
    if label not in deltas:
        raise LinkerError(f"unknown behavior delta: {label}")
    controls = controls or []
    unknown = [name for name in controls if name not in deltas]
    if unknown:
        raise LinkerError("unknown control delta(s): " + ", ".join(unknown))
    control_paths = {item["path"] for name in controls
                     for item in deltas[name].get("moved_paths", [])}
    samples = {item["path"]: item for item in deltas[label].get("samples", [])}
    vehicle_functions = []
    for address, function in index["functions"].items():
        identity = function.get("identity") or {}
        if identity.get("kind") != "F1CVehicle":
            continue
        for group in function["bases"]:
            if group["base"] != "this":
                continue
            for field in group["fields"]:
                vehicle_functions.append((int(field["off_hex"], 16), address,
                                          function["name"], field.get("rw", ""), identity))
    ranked = []
    for moved in deltas[label].get("moved_paths", []):
        path = moved["path"]
        flat = re.fullmatch(r"\+0x([0-9A-Fa-f]{4})", path)
        child = re.fullmatch(r"\+0x([0-9A-Fa-f]{4})->\+0x([0-9A-Fa-f]{4})", path)
        score = min(int(moved.get("cars", 1)), 10)
        evidence = [f"moved in {moved.get('cars', 1)} examined car(s)"]
        if path in control_paths:
            score -= 100
            evidence.append("also moved in a control capture")
        name = None
        static = []
        persistence = None
        if flat:
            offset = int(flat.group(1), 16)
            named = _vehicle_name(data["names"], offset)
            if named:
                name = named.get("name")
                score += {"FACT": 35, "CONFIRMED": 28, "DERIVED": 12, "GUESS": 0}.get(
                    named.get("trust"), 0)
                evidence.append(f"{named.get('trust')} vehicle field")
            if offset in data["persistence"]:
                persistence = data["persistence"][offset].get("class")
            for field_offset, address, function_name, rw, identity in vehicle_functions:
                if field_offset == offset:
                    static.append({"address": address, "function": function_name,
                                   "rw": rw, "identity_trust": identity.get("trust")})
            if any("W" in item["rw"] for item in static):
                score += 12
            elif static:
                score += 4
            if any(item["identity_trust"] in ("FACT", "CONFIRMED") for item in static):
                score += 5
        elif child:
            parent = int(child.group(1), 16)
            named = _vehicle_name(data["names"], parent)
            if named:
                name = f"{named.get('name')} child +0x{int(child.group(2), 16):X}"
                score += 25
                evidence.append(f"child of {named.get('trust')} vehicle pointer")
        ranked.append({
            "path": path, "name": name, "score": score,
            "in_controls": path in control_paths, "persistence": persistence,
            "static_accessors": static[:12], "sample": samples.get(path),
            "evidence": evidence,
        })
    ranked.sort(key=lambda item: (-item["score"], item["path"]))
    return {"label": label, "controls": controls, "trust": "OBSERVED_RANKING",
            "ranked_paths": ranked[:limit], "total_paths": len(ranked)}


def render_correlation(result):
    lines = [f"# Runtime correlation: {result['label']}", "",
             f"Controls: `{', '.join(result['controls']) or 'none'}`", "",
             "Ranking is OBSERVED evidence prioritization, not automatic naming.", "",
             "| Score | Path | Current name | Control | Persistence | Static accessors |",
             "|---:|---|---|---|---|---:|"]
    for item in result["ranked_paths"]:
        lines.append(f"| {item['score']} | `{item['path']}` | {item['name'] or ''} | "
                     f"{'yes' if item['in_controls'] else 'no'} | {item['persistence'] or ''} | "
                     f"{len(item['static_accessors'])} |")
    lines += ["", f"Ranked {len(result['ranked_paths'])} of {result['total_paths']} changed paths.", ""]
    return "\n".join(lines)


def front_wing_report(root, index):
    data = load_inputs(root)
    offsets = (0xCD4, 0xD14)
    lines = ["# Semantic-memory pilot: front-wing detachment", "",
             "This report joins static and runtime evidence. It does not claim that a detachment writer has been found.", "",
             "## Anchors", ""]
    for offset in offsets:
        name = _vehicle_name(data["names"], offset) or {}
        runtime = next((item for item in data["runtime"].get("vehicle", {}).get("fields", [])
                        if int(item["off"], 16) == offset), {})
        child = next((item for item in data["runtime"].get("vehicle", {}).get("real_children", [])
                      if int(item["off"], 16) == offset), None)
        lines.append(f"- `veh+0x{offset:X}` `{name.get('name', 'unnamed')}` [{name.get('trust', 'UNKNOWN')}]; "
                     f"runtime kind `{runtime.get('kind', 'unknown')}`")
        if child:
            lines.append(f"  - OBSERVED child shape: `{child.get('kind')}`, "
                         f"{child.get('floats', 0)} float-like / {child.get('ptrs', 0)} pointer-like words")
    config_hits = []
    for address, function in index["functions"].items():
        for field in function.get("config_fields", []):
            if "frontwing" in field.get("token", "").replace(" ", "").casefold():
                config_hits.append((address, function["name"], field))
    lines += ["", "## Static configuration bridge", ""]
    for address, name, field in config_hits:
        lines.append(f"- `{field.get('token')}` → parser object {', '.join(field.get('offsets', []))} "
                     f"in `{name}` `{address}` ({field.get('assoc')})")
    readers, writers = [], []
    for address, function in index["functions"].items():
        for group in function["bases"]:
            for field in group["fields"]:
                if int(field["off_hex"], 16) not in offsets:
                    continue
                row = (address, function["name"], group["base"], field.get("rw", ""),
                       (function.get("identity") or {}).get("trust", "UNKNOWN"))
                if "R" in field.get("rw", ""):
                    readers.append(row)
                if "W" in field.get("rw", ""):
                    writers.append(row)
    lines += ["", "## Static accessors", "",
              f"Readers: {len(readers)}; writers: {len(writers)}.", ""]
    for address, name, base, rw, trust in readers[:30]:
        lines.append(f"- `{address}` `{name}` base `{base}` `{rw}` identity `{trust}`")
    if not writers:
        lines += ["", "No direct static writer was extracted. This strongly suggests an indirect call, nested record write, "
                  "or resource-operation boundary is the next target; it does not prove that no writer exists."]
    labels = []
    for label, delta in data["runtime"].get("vehicle", {}).get("behavior_deltas", {}).items():
        paths = {item["path"] for item in delta.get("moved_paths", [])}
        relevant = [path for path in ("+0x0CD4", "+0x0D14") if path in paths]
        if relevant:
            labels.append((label, relevant))
    lines += ["", "## Runtime event links", ""]
    for label, paths in labels:
        lines.append(f"- `{label}` moved {', '.join(f'`{path}`' for path in paths)} [OBSERVED]")
    lines += ["", "## Next discriminating experiment", "",
              "Capture the same car immediately before and after a natural front-wing loss, with an ordinary driving "
              "control over the same duration. Correlate the two captures, then hook only the highest-ranked static "
              "accessors. The goal is to observe the game-owned call that changes the wing record—not to forge the flag.", ""]
    return "\n".join(lines)
