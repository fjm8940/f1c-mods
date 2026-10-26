#!/usr/bin/env python3
"""Propose trust tiers for a harvested address list by fixed rules, review-only.

Reads DIR/harvest.jsonl and writes DIR/tier_proposals.jsonl and DIR/REVIEW.md.
Every record is a proposal: this tool never decides, never edits the name
index, the SDK overrides or findings.jsonl, and never emits FACT as a new
proposal (FACT appears only as an R0 already-proven keep).
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess

SDK_HEADER_EXCLUDES = {
    "f1c_sdk.h",
    "f1c_reviewed_api.hpp",
    "f1c_vehicle_struct.h",
    "f1c_vehicle_struct_ida.h",
    "f1c_trace_schema.h",
}
FINGERPRINT_MARKERS = ("fingerprint", "expected_bytes", "entry bytes", "f1c_expect")
RAW_PREFIXES = ("dword_", "byte_", "word_", "flt_", "dbl_", "off_", "unk_")
NAME_PREFIXES = ("OFF_", "F1C_", "FN_", "A_")
NAME_SUFFIX = "_ADDRESS"
KINDS = ("code", "data", "vehicle_offset")
TIERS = ("FACT", "CONFIRMED", "CONFIRMED_PROPOSED", "DERIVED", "GUESS")
EXISTING_PROVEN = ("FACT", "CONFIRMED")
REVIEW_RULES = ("R1", "R2", "R3", "R4")
CODE_DATA_RANGE_MIN = 0x5B0000
TOP_DERIVED = 30


def _format_hex8(value):
    try:
        return f"0x{int(value, 16):08X}"
    except (TypeError, ValueError):
        return str(value)


def _load_harvest(harvest_dir):
    path = Path(harvest_dir) / "harvest.jsonl"
    if not path.is_file():
        raise FileNotFoundError(f"harvest input not found: {path}")
    entries = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        record = json.loads(stripped)
        if isinstance(record, dict):
            entries.append(record)
    return entries


def _sdk_prefix(root):
    return (Path(root) / "tools" / "sdk").relative_to(Path(root).parent).as_posix() + "/"


def _is_sdk_header(path, sdk_prefix):
    if not path.startswith(sdk_prefix):
        return False
    if Path(path).suffix.lower() not in (".h", ".hpp"):
        return False
    return Path(path).name not in SDK_HEADER_EXCLUDES


def _read_workspace_text(root, relative):
    for base in (Path(root).parent, Path(root)):
        candidate = base / relative
        if candidate.is_file():
            try:
                return candidate.read_text(encoding="utf-8", errors="replace")
            except OSError:
                return None
    return None


def _sdk_headers(entry, sdk_prefix):
    headers = []
    seen = set()
    for context in entry.get("contexts", []):
        path = context.get("file", "")
        if _is_sdk_header(path, sdk_prefix) and path not in seen:
            seen.add(path)
            headers.append(path)
    return headers


def _fingerprinted(root, sdk_headers):
    for path in sdk_headers:
        text = _read_workspace_text(root, path)
        if not text:
            continue
        lowered = text.lower()
        if any(marker in lowered for marker in FINGERPRINT_MARKERS):
            return True
    return False


def _matched_tests(root, sdk_headers):
    tests_dir = Path(root) / "tools" / "sdk" / "tests"
    if not sdk_headers or not tests_dir.is_dir():
        return []
    stems = {Path(path).stem for path in sdk_headers}
    workspace = Path(root).parent
    matched = []
    for path in sorted(tests_dir.iterdir()):
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if any(stem in text for stem in stems):
            matched.append(path.relative_to(workspace).as_posix())
    return matched


def _function_keys(root):
    path = Path(root) / "analysis" / "recovery_index.json"
    keys = set()
    if not path.is_file():
        return keys
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return keys
    for key in data.get("functions", {}):
        try:
            keys.add(int(str(key), 16))
        except (TypeError, ValueError):
            continue
    return keys


def _raw_corpus_pattern(digits):
    return "(?:" + "|".join(RAW_PREFIXES) + ")" + digits + "(?![0-9A-Fa-f])"


def _rg_corpus_count(rg, functions, digits):
    try:
        result = subprocess.run([rg, "-l", "--no-messages", _raw_corpus_pattern(digits),
                                 str(functions)],
                                capture_output=True, text=True, encoding="utf-8",
                                errors="replace")
    except OSError:
        return None
    if result.returncode not in (0, 1):
        return None
    return len([line for line in result.stdout.splitlines() if line.strip()])


def _python_corpus_counts(functions, digit_values):
    counts = {digits: 0 for digits in digit_values}
    symbol = re.compile("(?:" + "|".join(RAW_PREFIXES) + r")([0-9A-F]{6})(?![0-9A-Fa-f])")
    for path in functions.rglob("*.c"):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for digits in set(symbol.findall(text)) & set(counts):
            counts[digits] += 1
    return counts


def _raw_corpus_counts(root, digit_values):
    functions = Path(root) / "functions"
    digit_values = {digits for digits in digit_values if digits}
    if not digit_values or not functions.is_dir():
        return {digits: 0 for digits in digit_values}
    counts = {}
    rg = shutil.which("rg")
    if rg:
        for digits in sorted(digit_values):
            counts[digits] = _rg_corpus_count(rg, functions, digits)
        missing = {digits for digits, count in counts.items() if count is None}
        if missing:
            counts.update(_python_corpus_counts(functions, missing))
    else:
        counts.update(_python_corpus_counts(functions, digit_values))
    return counts


def _load_findings(root):
    path = Path(root) / "findings.jsonl"
    records = []
    if not path.is_file():
        return records
    text = path.read_text(encoding="utf-8", errors="replace")
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        try:
            record = json.loads(stripped)
        except ValueError:
            continue
        if not isinstance(record, dict):
            continue
        records.append((record.get("id") or f"line-{number}",
                        record.get("tier"), stripped))
    return records


def _value_pattern(value, cache):
    pattern = cache.get(value)
    if pattern is not None:
        return pattern
    forms = {value.lower(), _format_hex8(value).lower()}
    body = "|".join(re.escape(form) for form in sorted(forms))
    pattern = re.compile(r"(?<![0-9a-f])(?:" + body + r")(?![0-9a-f])", re.IGNORECASE)
    cache[value] = pattern
    return pattern


def _finding_matches(findings, value, cache):
    pattern = _value_pattern(value, cache)
    return [{"id": identifier, "tier": tier}
            for identifier, tier, text in findings if pattern.search(text)]


def _product_for_path(path):
    parts = path.split("/")
    for index in range(len(parts) - 2):
        pair = (parts[index], parts[index + 1])
        if pair == ("development", "mods"):
            return parts[index + 2]
        if pair == ("mods", "sdk_sandbox"):
            return parts[index + 2]
    return "sdk"


def _products(entry):
    return sorted({_product_for_path(context.get("file", ""))
                   for context in entry.get("contexts", [])})


def _existing(entry):
    overrides = entry.get("overrides")
    if overrides:
        return {"name": overrides.get("name"), "trust": overrides.get("trust"),
                "store": "f1c_sdk_overrides"}
    index = entry.get("index")
    if index:
        return {"name": index.get("name"), "trust": index.get("trust"),
                "store": index.get("store") or "name_index"}
    return {"name": None, "trust": None, "store": None}


def _kept_tier(entry, existing):
    if existing["trust"] in EXISTING_PROVEN:
        return existing["trust"]
    for store in ("index", "overrides"):
        candidate = entry.get(store)
        if candidate and candidate.get("trust") in EXISTING_PROVEN:
            return candidate["trust"]
    return None


def _strip_name(name):
    stripped = name
    for prefix in NAME_PREFIXES:
        if stripped.startswith(prefix):
            stripped = stripped[len(prefix):]
            break
    if stripped.endswith(NAME_SUFFIX):
        stripped = stripped[:-len(NAME_SUFFIX)]
    return stripped


def _camel_case(name):
    parts = []
    for part in name.split("_"):
        if not part:
            continue
        parts.append(part.capitalize() if part.isupper() else part[0].upper() + part[1:])
    return "".join(parts)


def _name_from_sdk_contexts(entry, sdk_headers):
    names = entry.get("names", [])
    if not names:
        return None
    escaped = "|".join(re.escape(name) for name in names)
    pattern = re.compile(r"(?<![A-Za-z0-9_])(" + escaped + r")(?![A-Za-z0-9_])")
    for context in entry.get("contexts", []):
        if context.get("file") not in sdk_headers:
            continue
        match = pattern.search(context.get("text", ""))
        if match:
            return match.group(1)
    return None


def _most_frequent_name(entry):
    names = entry.get("names", [])
    if not names:
        return None
    texts = [context.get("text", "") for context in entry.get("contexts", [])]
    counts = {}
    for name in names:
        pattern = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])")
        counts[name] = sum(len(pattern.findall(text)) for text in texts)
    ranked = sorted(counts.items(), key=lambda item: (-item[1], item[0]))
    return ranked[0][0]


def _proposed_name(entry, existing, sdk_headers):
    if existing["name"]:
        return existing["name"]
    header_name = _name_from_sdk_contexts(entry, sdk_headers)
    if header_name:
        normalized = _camel_case(_strip_name(header_name))
        if normalized:
            return normalized
    return _most_frequent_name(entry)


def _evidence(entry, root, sdk_prefix, function_keys, findings, raw_counts, cache):
    kind = entry.get("kind")
    value = entry.get("value", "")
    sdk_headers = _sdk_headers(entry, sdk_prefix)
    function_start = None
    if kind == "code":
        try:
            function_start = int(value, 16) in function_keys
        except (TypeError, ValueError):
            function_start = False
    digits = None
    if kind == "data":
        try:
            digits = f"{int(value, 16):06X}"
        except (TypeError, ValueError):
            digits = None
    raw_corpus = raw_counts.get(digits, 0) if digits else 0
    return {
        "sdk_headers": sdk_headers,
        "fingerprinted_header": _fingerprinted(root, sdk_headers),
        "tests": _matched_tests(root, sdk_headers),
        "function_start": function_start,
        "raw_corpus": raw_corpus,
        "findings": _finding_matches(findings, value, cache),
        "products": _products(entry),
    }


def _propose_entry(entry, root, sdk_prefix, function_keys, findings, raw_counts, cache):
    kind = entry.get("kind")
    value = entry.get("value", "")
    evidence = _evidence(entry, root, sdk_prefix, function_keys, findings, raw_counts, cache)
    existing = _existing(entry)
    kept = _kept_tier(entry, existing)
    in_code_range = False
    if kind == "code" and not evidence["function_start"]:
        try:
            in_code_range = int(value, 16) >= CODE_DATA_RANGE_MIN
        except (TypeError, ValueError):
            in_code_range = False
    finding_proven = any(item["tier"] in EXISTING_PROVEN for item in evidence["findings"])
    reclassify = None
    if kept:
        rule_id = "R0"
        proposed_tier = kept
    elif in_code_range:
        rule_id = "R1"
        proposed_tier = "GUESS"
        reclassify = "data?"
    elif (kind == "code" and evidence["function_start"]
          and evidence["fingerprinted_header"] and evidence["tests"]):
        rule_id = "R2"
        proposed_tier = "CONFIRMED_PROPOSED"
    elif kind == "data" and evidence["sdk_headers"] and evidence["raw_corpus"] >= 1:
        rule_id = "R3"
        proposed_tier = "CONFIRMED_PROPOSED"
    elif kind == "vehicle_offset" and evidence["sdk_headers"] and finding_proven:
        rule_id = "R4"
        proposed_tier = "CONFIRMED_PROPOSED"
    elif (evidence["sdk_headers"] or len(evidence["products"]) >= 2
          or (kind == "code" and evidence["function_start"])
          or (kind == "data" and evidence["raw_corpus"] >= 1)):
        rule_id = "R5"
        proposed_tier = "DERIVED"
    else:
        rule_id = "R6"
        proposed_tier = "GUESS"
    proposed_name = _proposed_name(entry, existing, evidence["sdk_headers"])
    alt_names = [name for name in entry.get("names", []) if name != proposed_name]
    needs_review = (rule_id in REVIEW_RULES or bool(entry.get("name_conflict"))
                    or (rule_id == "R5" and not proposed_name))
    return {
        "kind": kind,
        "value": value,
        "proposed_tier": proposed_tier,
        "rule_id": rule_id,
        "proposed_name": proposed_name,
        "alt_names": alt_names,
        "existing": existing,
        "evidence": evidence,
        "needs_review": bool(needs_review),
        "reclassify": reclassify,
    }


def propose(root, harvest_dir):
    """Read-only rule evaluation; returns proposals, counts and review totals."""
    root = Path(root)
    entries = _load_harvest(harvest_dir)
    sdk_prefix = _sdk_prefix(root)
    function_keys = _function_keys(root)
    findings = _load_findings(root)
    cache = {}
    digits = set()
    for entry in entries:
        if entry.get("kind") == "data":
            try:
                digits.add(f"{int(entry.get('value', ''), 16):06X}")
            except (TypeError, ValueError):
                continue
    raw_counts = _raw_corpus_counts(root, digits)
    records = [_propose_entry(entry, root, sdk_prefix, function_keys, findings,
                              raw_counts, cache)
               for entry in entries]
    contexts = {}
    for entry in entries:
        context_list = entry.get("contexts") or []
        if context_list:
            context = context_list[0]
            contexts[entry.get("value", "")] = "{}:{} {}".format(
                context.get("file", ""), context.get("line", ""),
                context.get("text", "")[:120])
    conflicts = []
    for entry, record in zip(entries, records):
        if entry.get("name_conflict"):
            conflicts.append({
                "kind": entry.get("kind"),
                "value": entry.get("value"),
                "proposed_name": record["proposed_name"],
                "existing": record["existing"],
                "names": entry.get("names", []),
            })
    needs_review = sum(1 for record in records if record["needs_review"])
    return {"records": records, "counts": _counts(records),
            "needs_review": needs_review, "total": len(records),
            "contexts": contexts, "conflicts": conflicts}


def _counts(records):
    counts = {tier: {kind: 0 for kind in KINDS} for tier in TIERS}
    for record in records:
        tier = record["proposed_tier"]
        if tier not in counts:
            counts[tier] = {kind: 0 for kind in KINDS}
        counts[tier][record["kind"]] += 1
    return counts


def counts_lines(result):
    lines = []
    for tier in TIERS:
        row = result["counts"].get(tier, {})
        lines.append("{}: code={} data={} vehicle_offset={} (total={})".format(
            tier, row.get("code", 0), row.get("data", 0),
            row.get("vehicle_offset", 0), sum(row.get(kind, 0) for kind in KINDS)))
    lines.append(f"needs_review: {result['needs_review']} of {result['total']}")
    return lines


def _cell(text):
    return str(text).replace("|", "\\|").replace("\n", " ")


def _evidence_summary(evidence):
    return ("hdrs={} fp={} tests={} func={} corpus={} findings={} products={}".format(
        len(evidence["sdk_headers"]), int(bool(evidence["fingerprinted_header"])),
        len(evidence["tests"]),
        "-" if evidence["function_start"] is None else int(bool(evidence["function_start"])),
        evidence["raw_corpus"], len(evidence["findings"]), len(evidence["products"])))


def _row(columns):
    return "| " + " | ".join(_cell(column) for column in columns) + " |"


def render_review(result):
    contexts = result.get("contexts", {})
    lines = [
        "# Address tier proposals (review only)",
        "",
        "Fixed-rule proposals from a harvest. Nothing here is applied: a reviewer",
        "decides and merges. FACT appears only as an R0 already-proven keep.",
        "",
        "## Counts",
        "",
        "| tier | code | data | vehicle_offset | total |",
        "|---|---|---|---|---|",
    ]
    for tier in TIERS:
        row = result["counts"].get(tier, {})
        lines.append(f"| {tier} | {row.get('code', 0)} | {row.get('data', 0)} | "
                     f"{row.get('vehicle_offset', 0)} | "
                     f"{sum(row.get(kind, 0) for kind in KINDS)} |")
    lines.append(f"| needs_review |  |  |  | {result['needs_review']} |")
    confirmed = [record for record in result["records"]
                 if record["proposed_tier"] == "CONFIRMED_PROPOSED"]
    lines += ["", f"## CONFIRMED_PROPOSED ({len(confirmed)})", ""]
    lines += ["| kind | value | name | rule | evidence | context |",
              "|---|---|---|---|---|---|"]
    for record in confirmed:
        lines.append(_row([record["kind"], record["value"], record["proposed_name"] or "",
                           record["rule_id"], _evidence_summary(record["evidence"]),
                           contexts.get(record["value"], "")]))
    r1 = [record for record in result["records"] if record["rule_id"] == "R1"]
    lines += ["", f"## R1 reclassifications (data?) ({len(r1)})", ""]
    lines += ["| kind | value | name | rule | evidence |",
              "|---|---|---|---|---|"]
    for record in r1:
        lines.append(_row([record["kind"], record["value"], record["proposed_name"] or "",
                           record["rule_id"], _evidence_summary(record["evidence"])]))
    conflicts = result.get("conflicts", [])
    lines += ["", f"## Name conflicts ({len(conflicts)})", ""]
    lines += ["| kind | value | proposed | existing | harvested names |",
              "|---|---|---|---|---|"]
    for conflict in conflicts:
        existing = conflict["existing"]
        store = f"{existing.get('name') or 'null'} ({existing.get('store') or 'none'})"
        names = conflict.get("names", [])
        shown = ", ".join(names[:12])
        if len(names) > 12:
            shown += f" (+{len(names) - 12} more)"
        lines.append(_row([conflict["kind"], conflict["value"],
                           conflict["proposed_name"] or "", store, shown]))
    derived = [record for record in result["records"]
               if record["rule_id"] == "R5" and record["proposed_tier"] == "DERIVED"]
    derived.sort(key=lambda record: (-len(record["evidence"]["products"]), record["value"]))
    lines += ["", f"## Top {TOP_DERIVED} R5 DERIVED by product count (of {len(derived)})", ""]
    lines += ["| kind | value | name | products | evidence |",
              "|---|---|---|---|---|"]
    for record in derived[:TOP_DERIVED]:
        lines.append(_row([record["kind"], record["value"], record["proposed_name"] or "",
                           ", ".join(record["evidence"]["products"]),
                           _evidence_summary(record["evidence"])]))
    return "\n".join(lines) + "\n"


def write_output(result, harvest_dir):
    harvest_dir = Path(harvest_dir)
    proposals = harvest_dir / "tier_proposals.jsonl"
    review = harvest_dir / "REVIEW.md"
    for path in (proposals, review):
        if path.exists():
            raise FileExistsError(f"address tier output already exists: {path}")
    with proposals.open("w", encoding="utf-8", newline="\n") as handle:
        for record in result["records"]:
            handle.write(json.dumps(record, ensure_ascii=False) + "\n")
    review.write_text(render_review(result), encoding="utf-8", newline="\n")
    return harvest_dir


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--harvest", required=True,
                        help="existing harvest directory containing harvest.jsonl")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    harvest_dir = Path(args.harvest)
    if not harvest_dir.is_absolute():
        harvest_dir = root / harvest_dir
    if not (harvest_dir / "harvest.jsonl").is_file():
        parser.error(f"harvest input not found: {harvest_dir / 'harvest.jsonl'}")
    for name in ("tier_proposals.jsonl", "REVIEW.md"):
        if (harvest_dir / name).exists():
            parser.error(f"address tier output already exists: {harvest_dir / name}")
    result = propose(root, harvest_dir)
    write_output(result, harvest_dir)
    if args.json:
        print(json.dumps({"counts": result["counts"],
                          "needs_review": result["needs_review"],
                          "total": result["total"]}, indent=2))
    else:
        for line in counts_lines(result):
            print(line)
        print(f"Address tier proposals written: {harvest_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
