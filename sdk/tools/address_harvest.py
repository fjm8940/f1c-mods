#!/usr/bin/env python3
"""Collect every address-like value the project already uses, read-only.

The harvester scans the hand-written SDK headers, sandbox mods, product
sources and SDK review records. It extracts code addresses, data addresses and
vehicle-relative offsets, aggregates them per value, and joins each value with
the name index and the SDK overrides. It assigns no trust tier and writes
nothing unless its caller asks for the output files.
"""
from pathlib import Path
import argparse
import json
import re

SDK_HEADER_EXCLUDES = {
    "f1c_sdk.h",
    "f1c_reviewed_api.hpp",
    "f1c_vehicle_struct.h",
    "f1c_vehicle_struct_ida.h",
    "f1c_trace_schema.h",
}
SKIP_PARTS = {"build", "bin", "obj", "dist", "node_modules", "generated"}
VEHICLE_KEYWORDS = ("veh", "this+", "off_", "f1c_veh")

CODE_LITERAL = re.compile(r"0x00(?:4[0-9A-Fa-f]{5}|5[0-9A-Ba-b][0-9A-Fa-f]{4})(?![0-9A-Fa-f])")
CODE_SYMBOL = re.compile(r"sub_[45][0-9A-Fa-f]{5}(?![0-9A-Fa-f])")
DATA_LITERAL = re.compile(r"0x00(?:5[C-Fc-f][0-9A-Fa-f]{4}|[678][0-9A-Fa-f]{5})(?![0-9A-Fa-f])")
DATA_SYMBOL = re.compile(r"(?:dword_|byte_|word_|flt_|unk_)([0-9A-Fa-f]{6})(?![0-9A-Fa-f])")
OFFSET_LITERAL = re.compile(r"0x[0-9A-Fa-f]{1,4}(?![0-9A-Fa-f])")

DEFINE = re.compile(r"#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)")
ASSIGN = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*=")
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")

KINDS = ("code", "data", "vehicle_offset")
CODE_MAX = 0x5BFFFF
DATA_MIN = 0x5C0000
DATA_MAX = 0x8FFFFF
OFFSET_MAX = 0x48BF


def _source_files(root):
    """Every source path the harvest reads, filtered by the path rules."""
    workspace = root.parent
    sdk = root / "tools" / "sdk"
    candidates = []
    for pattern in ("*.h", "*.hpp", "*.c"):
        candidates.extend(sorted(sdk.glob(pattern)))
    sandbox = root / "mods" / "sdk_sandbox"
    for pattern in ("**/*.c", "**/*.h"):
        candidates.extend(sorted(sandbox.glob(pattern)))
    development = workspace / "development" / "mods"
    for pattern in ("**/*.c", "**/*.h", "**/*.py"):
        candidates.extend(sorted(development.glob(pattern)))
    candidates.extend(sorted(sdk.glob("*REVIEW*.md")))

    files = []
    seen = set()
    for path in candidates:
        if path.parent == sdk and path.name in SDK_HEADER_EXCLUDES:
            continue
        relative = path.relative_to(workspace).as_posix()
        if any(part in SKIP_PARTS for part in relative.split("/")):
            continue
        if relative in seen:
            continue
        seen.add(relative)
        files.append(path)
    return files


def _has_vehicle_keyword(line):
    lowered = line.lower()
    return any(keyword in lowered for keyword in VEHICLE_KEYWORDS)


def _line_hits(line):
    """All (kind, value, matchStart) hits on one line."""
    hits = []
    for match in CODE_LITERAL.finditer(line):
        hits.append(("code", int(match.group(0), 16), match.start()))
    for match in CODE_SYMBOL.finditer(line):
        value = int(match.group(0)[4:], 16)
        if value <= CODE_MAX:
            hits.append(("code", value, match.start()))
    for match in DATA_LITERAL.finditer(line):
        hits.append(("data", int(match.group(0), 16), match.start()))
    for match in DATA_SYMBOL.finditer(line):
        value = int(match.group(1), 16)
        if DATA_MIN <= value <= DATA_MAX:
            hits.append(("data", value, match.start()))
    if _has_vehicle_keyword(line):
        for match in OFFSET_LITERAL.finditer(line):
            value = int(match.group(0), 16)
            if value <= OFFSET_MAX:
                hits.append(("vehicle_offset", value, match.start()))
    return hits


def _candidate_name(line, start):
    """#define NAME, the closest NAME =, or the nearest identifier; else None."""
    match = DEFINE.search(line)
    if match:
        return match.group(1)
    assignments = [item for item in ASSIGN.finditer(line) if item.end() <= start]
    if assignments:
        return assignments[-1].group(1)
    identifiers = [item for item in IDENT.finditer(line) if item.end() <= start]
    if identifiers:
        return identifiers[-1].group(0)
    return None


def _format_value(kind, value):
    if kind == "vehicle_offset":
        return f"0x{value:X}"
    return f"0x{value:08X}"


def _load_stores(root):
    """Return (index, overrides) lookup maps keyed by integer value."""
    index = {"vehicle_fields": {}, "globals": {}}
    overrides = {"vehicle_fields": {}, "globals": {}, "functions": {}}

    index_path = root / "name_index.json"
    if index_path.is_file():
        try:
            data = json.loads(index_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            data = {}
        for key, value in data.get("vehicle_fields", {}).items():
            try:
                index["vehicle_fields"][int(key, 16)] = value
            except (TypeError, ValueError):
                continue
        for key, value in data.get("globals", {}).items():
            try:
                index["globals"][int(key, 16)] = value
            except (TypeError, ValueError):
                continue

    overrides_path = root / "tools" / "sdk" / "f1c_sdk_overrides.json"
    if overrides_path.is_file():
        try:
            data = json.loads(overrides_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            data = {}
        for key, value in data.get("vehicle_fields", {}).items():
            if not isinstance(value, dict):
                continue
            try:
                overrides["vehicle_fields"][int(key, 16)] = {
                    "name": value.get("name"), "trust": value.get("trust")}
            except (TypeError, ValueError):
                continue
        for key, value in data.get("globals", {}).items():
            if not isinstance(value, dict):
                continue
            try:
                overrides["globals"][int(value["addr"])] = {
                    "name": key, "trust": value.get("trust")}
            except (KeyError, TypeError, ValueError):
                continue
        for key, value in data.get("functions", {}).items():
            if not isinstance(value, dict):
                continue
            try:
                overrides["functions"][int(str(value["address"]), 16)] = {
                    "name": key, "trust": value.get("trust")}
            except (KeyError, TypeError, ValueError):
                continue
    return index, overrides


def harvest(root):
    """Read-only collection: return entries, counts and top unknown lists."""
    root = Path(root)
    workspace = root.parent
    sdk_prefix = (root / "tools" / "sdk").relative_to(workspace).as_posix() + "/"
    index, overrides = _load_stores(root)

    aggregates = {}
    for path in _source_files(root):
        relative = path.relative_to(workspace).as_posix()
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
            mtime = path.stat().st_mtime
        except OSError:
            continue
        prefer = 0 if relative.startswith(sdk_prefix) else 1
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if not stripped:
                continue
            for kind, value, start in _line_hits(line):
                key = (kind, value)
                entry = aggregates.get(key)
                if entry is None:
                    entry = {"kind": kind, "value": _format_value(kind, value),
                             "names": set(), "hits": 0, "file_set": set(),
                             "contexts": [], "context_keys": set(),
                             "index_raw": None, "override_raw": None}
                    aggregates[key] = entry
                entry["hits"] += 1
                entry["file_set"].add(relative)
                name = _candidate_name(line, start)
                if name:
                    entry["names"].add(name)
                context_key = (relative, number)
                if context_key not in entry["context_keys"]:
                    entry["context_keys"].add(context_key)
                    entry["contexts"].append(
                        (prefer, -mtime, relative, number, stripped[:160]))

    entries = []
    for (kind, value), entry in aggregates.items():
        if kind == "vehicle_offset":
            entry["index_raw"] = index["vehicle_fields"].get(value)
            entry["override_raw"] = overrides["vehicle_fields"].get(value)
        elif kind == "data":
            entry["index_raw"] = index["globals"].get(value)
            entry["override_raw"] = overrides["globals"].get(value)
        else:
            entry["override_raw"] = overrides["functions"].get(value)
        names = sorted(entry["names"])
        index_out = None
        if entry["index_raw"]:
            index_out = {"name": entry["index_raw"].get("name"),
                         "trust": entry["index_raw"].get("trust"),
                         "store": "name_index"}
        override_out = None
        if entry["override_raw"]:
            override_out = {"name": entry["override_raw"].get("name"),
                            "trust": entry["override_raw"].get("trust")}
        known = [item["name"] for item in (index_out, override_out)
                 if item and item.get("name")]
        lowered = {name.lower() for name in names}
        name_conflict = bool(names) and any(name.lower() not in lowered
                                           for name in known)
        contexts = [{"file": item[2], "line": item[3], "text": item[4]}
                    for item in sorted(entry["contexts"])[:5]]
        entries.append({
            "kind": kind,
            "value": entry["value"],
            "names": names,
            "hits": entry["hits"],
            "files": len(entry["file_set"]),
            "contexts": contexts,
            "index": index_out,
            "overrides": override_out,
            "name_conflict": name_conflict,
        })

    entries.sort(key=lambda item: (KINDS.index(item["kind"]), int(item["value"], 16)))

    counts = {}
    top = {}
    for kind in KINDS:
        subset = [item for item in entries if item["kind"] == kind]
        proven = 0
        known_unproven = 0
        unknown = 0
        name_conflicts = 0
        for item in subset:
            stores = [store for store in (item["index"], item["overrides"]) if store]
            trusts = [store.get("trust") for store in stores]
            if any(trust in ("FACT", "CONFIRMED") for trust in trusts):
                proven += 1
            elif stores:
                known_unproven += 1
            else:
                unknown += 1
            if item["name_conflict"]:
                name_conflicts += 1
        counts[kind] = {"total": len(subset), "proven": proven,
                        "known_unproven": known_unproven, "unknown": unknown,
                        "name_conflicts": name_conflicts}
        unknown_items = [item for item in subset
                         if not item["index"] and not item["overrides"]]
        unknown_items.sort(key=lambda item: (-item["files"], -item["hits"],
                                             int(item["value"], 16)))
        top[kind] = unknown_items[:40]

    return {"entries": entries, "counts": counts, "top": top}


def counts_lines(result):
    lines = []
    for kind in KINDS:
        item = result["counts"][kind]
        lines.append(f"{kind}: total={item['total']} proven={item['proven']} "
                     f"known_unproven={item['known_unproven']} "
                     f"unknown={item['unknown']} "
                     f"name_conflicts={item['name_conflicts']}")
    return lines


def render_summary(result):
    lines = [
        "# Address harvest summary",
        "",
        "Read-only collection of address-like values already used by the",
        "project. No trust tier is assigned here; a reviewer decides tiers.",
        "",
        "## Counts",
        "",
        "| kind | total | in a store (FACT/CONFIRMED) | known, unproven | not in any store | name conflicts |",
        "|---|---|---|---|---|---|",
    ]
    for kind in KINDS:
        item = result["counts"][kind]
        lines.append(f"| {kind} | {item['total']} | {item['proven']} | "
                     f"{item['known_unproven']} | {item['unknown']} | "
                     f"{item['name_conflicts']} |")
    lines += ["", "## Top 40 not-in-any-store entries", ""]
    for kind in KINDS:
        lines.append(f"### {kind}")
        lines.append("")
        top = result["top"][kind]
        if not top:
            lines.append("(none)")
            lines.append("")
            continue
        lines.append("| value | names | files | hits | first context |")
        lines.append("|---|---|---|---|---|")
        for item in top:
            names = ", ".join(item["names"])
            if item["contexts"]:
                context = item["contexts"][0]
                place = f"{context['file']}:{context['line']}"
            else:
                place = ""
            lines.append(f"| {item['value']} | {names} | {item['files']} | "
                         f"{item['hits']} | {place} |")
        lines.append("")
    return "\n".join(lines) + "\n"


def write_output(result, output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=False)
    with (output_dir / "harvest.jsonl").open("w", encoding="utf-8", newline="\n") as handle:
        for entry in result["entries"]:
            handle.write(json.dumps(entry, ensure_ascii=False) + "\n")
    (output_dir / "SUMMARY.md").write_text(render_summary(result), encoding="utf-8",
                                           newline="\n")
    return output_dir


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True,
                        help="new output directory; must not exist")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    output = Path(args.output)
    if not output.is_absolute():
        output = root / output
    if output.exists():
        parser.error(f"output directory already exists: {output}")
    result = harvest(root)
    write_output(result, output)
    if args.json:
        print(json.dumps(result["counts"], indent=2))
    else:
        for line in counts_lines(result):
            print(line)
        print(f"Address harvest written: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
