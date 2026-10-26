#!/usr/bin/env python3
"""Direct, provenance-aware access to the F1C decompiler corpora."""
from pathlib import Path
import fnmatch
import json
import os
import re
import shutil
import subprocess


def _newest_typed_corpus():
    """Newest `ida-export` corpus directory name, or None before the first export."""
    candidates = sorted(path.name for path in Path(__file__).resolve().parents[1].glob("functions_typed_*")
                        if path.is_dir() and (path / "MANIFEST.json").is_file())
    return candidates[-1] if candidates else None


CORPORA = {
    "raw": ("functions", "*.c", "RAW_HEXRAYS"),
    "annotated": ("functions_annotated", "*.c", "ANNOTATED_MIXED"),
    "facts": ("skeletons", "*.md", "FACTS_WITH_PLACEHOLDERS"),
    "derived": ("skeletons_filled", "*.md", "DERIVED_PROSE"),
}
if _newest_typed_corpus():
    # Typed Hex-Rays export of the reviewed lab IDB (see its MANIFEST.json).
    CORPORA = {"raw": CORPORA["raw"],
               "typed": (_newest_typed_corpus(), "*.c", "TYPED_HEXRAYS"),
               **{key: value for key, value in CORPORA.items() if key != "raw"}}
ADDRESS_SUFFIX = re.compile(r"_([0-9a-fA-F]{8})\.(?:c|md)$")


class SourceExplorerError(RuntimeError):
    pass


def selected_corpora(values):
    values = values or ["all"]
    if "all" in values:
        return list(CORPORA)
    return list(dict.fromkeys("derived" if value == "translated" else value
                              for value in values))


def source_records(root, corpora=None):
    records = []
    for corpus in selected_corpora(corpora):
        directory, pattern, trust = CORPORA[corpus]
        for path in sorted((root / directory).glob(pattern)):
            match = ADDRESS_SUFFIX.search(path.name)
            if not match:
                continue
            records.append({
                "corpus": corpus,
                "trust": trust,
                "address": f"0x{int(match.group(1), 16):08X}",
                "path": path,
            })
    return records


def _plain_terms(query, case_sensitive):
    terms = [item for item in re.split(r"\s+", query.strip()) if item]
    if not terms:
        raise SourceExplorerError("search query cannot be empty")
    return terms if case_sensitive else [item.casefold() for item in terms]


def _matching_lines(lines, terms=None, regex=None, case_sensitive=False):
    matches = []
    for number, line in enumerate(lines, 1):
        candidate = line if case_sensitive else line.casefold()
        if regex:
            hit = regex.search(line)
            strength = 1 if hit else 0
        else:
            strength = sum(term in candidate for term in terms)
            hit = strength > 0
        if hit:
            matches.append((number, strength))
    return [number for number, _ in sorted(matches, key=lambda item: (-item[1], item[0]))]


def _snippets(lines, line_numbers, context, maximum=3):
    output = []
    for number in line_numbers[:maximum]:
        first = max(1, number - context)
        last = min(len(lines), number + context)
        output.append({
            "line": number,
            "first_line": first,
            "last_line": last,
            "text": "\n".join(f"{index:>6}: {lines[index - 1]}"
                               for index in range(first, last + 1)),
        })
    return output


# Diagnostic state of the most recent search_sources call in this process.
# Single-threaded CLI use only; read it immediately after the call it describes.
LAST_PREFILTER = {"used": False, "reason": None}


def _rg_skip_reason(terms):
    if os.environ.get("F1C_SEARCH_NO_RG") == "1":
        return "disabled by F1C_SEARCH_NO_RG"
    if not shutil.which("rg"):
        return "rg not found on PATH"
    if not terms:
        return "empty search term"
    if not all(term.isascii() for term in terms):
        return "non-ASCII search term"
    return None


def _rg_candidates(root, corpora, terms, mode, case_sensitive):
    """Relative-path set of files that may match, or None to scan everything."""
    if _rg_skip_reason(terms) is not None:
        return None
    exe = shutil.which("rg")
    command = [exe, "--files-with-matches", "--fixed-strings", "--no-ignore",
               "--hidden", "--no-messages", "--no-config", "--text"]
    if not case_sensitive:
        command.append("--ignore-case")
    chosen = [max(terms, key=len)] if mode == "all" else list(terms)
    for term in chosen:
        command += ["-e", term]
    directories = []
    for corpus in selected_corpora(corpora):
        directory, pattern, _ = CORPORA[corpus]
        command += ["--glob", pattern]
        if (Path(root) / directory).is_dir():
            directories.append(directory)
    if not directories:
        return set()
    command += ["--"] + sorted(set(directories))
    process = subprocess.run(command, cwd=root, capture_output=True, text=True,
                             encoding="utf-8", errors="replace")
    if process.returncode == 1:
        return set()
    if process.returncode != 0:
        return None
    return {os.path.normcase(os.path.normpath(line.strip()))
            for line in process.stdout.splitlines() if line.strip()}


def search_sources(root, recovery, query, *, corpora=None, regex=False,
                   case_sensitive=False, mode="all", context=1, limit=25):
    flags = 0 if case_sensitive else re.IGNORECASE
    compiled = None
    terms = None
    if regex:
        try:
            compiled = re.compile(query, flags)
        except re.error as error:
            raise SourceExplorerError(f"invalid regular expression: {error}") from error
    else:
        terms = _plain_terms(query, case_sensitive)

    if compiled:
        candidates = None
        prefilter_reason = "regex mode"
    else:
        candidates = _rg_candidates(root, corpora, terms, mode, case_sensitive)
        prefilter_reason = _rg_skip_reason(terms) or "rg exited with an error"
    LAST_PREFILTER["used"] = candidates is not None
    LAST_PREFILTER["reason"] = None if candidates is not None else prefilter_reason
    records = source_records(root, corpora)
    if candidates is not None:
        records = [r for r in records
                   if os.path.normcase(os.path.normpath(str(r["path"].relative_to(root)))) in candidates]

    grouped = {}
    for record in records:
        text = record["path"].read_text(encoding="utf-8", errors="ignore")
        comparable = text if case_sensitive else text.casefold()
        if compiled:
            occurrences = len(compiled.findall(text))
            qualifies = occurrences > 0
        else:
            counts = [comparable.count(term) for term in terms]
            qualifies = all(counts) if mode == "all" else any(counts)
            occurrences = sum(counts)
        if not qualifies:
            continue
        address = record["address"]
        node = recovery.get("functions", {}).get(address, {})
        name_text = " ".join((record["path"].stem, node.get("name", ""),
                              node.get("declared_name", "")))
        name_comparable = name_text if case_sensitive else name_text.casefold()
        name_hits = (1 if compiled and compiled.search(name_text) else 0) if compiled else sum(
            term in name_comparable for term in terms)
        lines = text.splitlines()
        line_numbers = _matching_lines(lines, terms=terms, regex=compiled,
                                       case_sensitive=case_sensitive)
        item = grouped.setdefault(address, {
            "address": address,
            "name": node.get("name", record["path"].stem.rsplit("_", 1)[0]),
            "declared_name": node.get("declared_name"),
            "score": 0,
            "sources": [],
            "callers": len(node.get("callers", [])),
            "callees": len(node.get("callees", [])),
        })
        item["score"] += name_hits * 100 + min(occurrences, 50)
        item["sources"].append({
            "corpus": record["corpus"],
            "trust": record["trust"],
            "path": str(record["path"].relative_to(root)).replace("\\", "/"),
            "occurrences": occurrences,
            "snippets": _snippets(lines, line_numbers, context),
        })
    return sorted(grouped.values(), key=lambda item: (-item["score"], item["address"]))[:limit]


def resolve_address(recovery, value):
    functions = recovery.get("functions", {})
    raw = str(value).strip()
    match = re.fullmatch(r"(?:0[xX]|sub_)?([0-9a-fA-F]{1,8})", raw)
    key = None
    if match:
        key = f"0x{int(match.group(1), 16):08X}"
        if key in functions:
            return key
    needle = raw.casefold()
    matches = [k for k, node in functions.items()
               if needle in (node.get("name", "").casefold(),
                             node.get("declared_name", "").casefold())]
    if len(matches) == 1:
        return matches[0]
    detail = f"address {key} not in recovery index" if key else "not an address"
    raise SourceExplorerError(
        f"function must resolve uniquely by exact name or address: {value} "
        f"({detail}; {len(matches)} name matches)")


def inspect_function(root, recovery, catalogue, value, *, corpora=None,
                     max_lines=240, full=False):
    address = resolve_address(recovery, value)
    node = recovery["functions"][address]
    reviewed = None
    for name, metadata in catalogue.get("functions", {}).items():
        if int(metadata.get("address", -1)) == int(address, 16):
            reviewed = {"name": name, **metadata}
            break
    records = []
    for corpus in selected_corpora(corpora):
        directory, pattern, trust = CORPORA[corpus]
        extension = pattern[1:]
        for path in sorted((root / directory).glob(f"*_{address[2:].lower()}{extension}")):
            match = ADDRESS_SUFFIX.search(path.name)
            if not match:
                continue
            if f"0x{int(match.group(1), 16):08X}" != address:
                continue
            records.append({
                "corpus": corpus,
                "trust": trust,
                "address": address,
                "path": path,
            })
    sources = []
    for record in records:
        text = record["path"].read_text(encoding="utf-8", errors="ignore")
        lines = text.splitlines()
        truncated = not full and len(lines) > max_lines
        shown = lines if full else lines[:max_lines]
        sources.append({
            "corpus": record["corpus"], "trust": record["trust"],
            "path": str(record["path"].relative_to(root)).replace("\\", "/"),
            "line_count": len(lines), "truncated": truncated,
            "text": "\n".join(shown),
        })
    return {"address": address, "node": node, "reviewed": reviewed, "sources": sources,
            "neighbor_counts": {"callers": len(node.get("callers", [])),
                                "callees": len(node.get("callees", []))}}


def render_search(results, query):
    lines = [f'# Source search: "{query}"', "",
             "Raw and extracted evidence are shown separately; DERIVED prose is not ABI evidence.", ""]
    if not results:
        lines.append("No matches.")
        return "\n".join(lines) + "\n"
    for result in results:
        lines.append(f"## {result['address']} {result['name']}")
        lines.append(f"Score {result['score']} · {result['callers']} callers / {result['callees']} callees")
        lines.append("")
        for source in result["sources"]:
            lines.append(f"- `{source['corpus']}` `{source['trust']}` "
                         f"[{source['path']}] ({source['occurrences']} matches)")
            for snippet in source["snippets"]:
                lines += ["", "```text", snippet["text"], "```"]
        lines.append("")
    return "\n".join(lines)


def render_inspection(result, max_neighbors=25):
    node = result["node"]
    lines = [f"# {result['address']} {node['name']}", "",
             "The raw Hex-Rays file is canonical code evidence. Annotated and DERIVED sections are aids, not proof.", "",
             f"- Declared name: `{node.get('declared_name') or '-'}`",
             f"- Owner: `{node.get('object_kind') or 'unreviewed'}` ({node.get('object_trust') or 'UNKNOWN'})",
             f"- SDK ABI: `{result['reviewed']['name']}` ({result['reviewed'].get('trust')})" if result["reviewed"] else "- SDK ABI: `not reviewed`",
             f"- Object offsets: `{', '.join(node.get('this_offsets', [])) or '-'}`", ""]
    links = result.get("sdk_links")
    if links:
        lines += ["## SDK links (navigation only; trust unchanged)", "",
                  links["note"], ""]
        for category in ("helpers", "tests", "docs"):
            entries = links.get(category, [])
            if not entries:
                continue
            lines.append(f"- {category.title()} ({links['totals'].get(category, len(entries))} total):")
            for hit in entries:
                lines.append(f"  - `{hit['path']}`:{hit['line']} `{hit['text']}`")
        lines.append("")
    for relation in ("callers", "callees"):
        addresses = node.get(relation, [])
        entries = []
        for address in addresses[:max_neighbors]:
            related = result.get("node_lookup", {}).get(address)
            entries.append(f"`{address}` `{related['name'] if related else '?'}`")
        text = ", ".join(entries) if entries else "none"
        if len(addresses) > max_neighbors:
            text += f" (+{len(addresses) - max_neighbors} more)"
        lines.append(f"- {relation.title()}: " + text)
    lines.append("")
    if not result["sources"]:
        lines.append("No selected source files exist for this address.")
    for source in result["sources"]:
        language = "c" if source["path"].endswith(".c") else "text"
        lines += [f"## {source['corpus']} — {source['trust']}", "",
                  f"Source: `{source['path']}` ({source['line_count']} lines)", "",
                  f"```{language}", source["text"]]
        if source["truncated"]:
            lines.append(f"... truncated after {len(source['text'].splitlines())} lines; use --full")
        lines += ["```", ""]
    return "\n".join(lines)


def attach_node_lookup(result, recovery, expand=False):
    addresses = result["node"].get("callers", []) + result["node"].get("callees", [])
    result["node_lookup"] = {}
    for key in addresses:
        node = recovery["functions"].get(key)
        if node is None:
            result["node_lookup"][key] = None
        elif expand:
            result["node_lookup"][key] = node
        else:
            result["node_lookup"][key] = {"name": node.get("name"),
                                          "declared_name": node.get("declared_name")}
    return result


SDK_LINK_EXCLUDED = {"f1c_sdk.h", "f1c_reviewed_api.hpp", "f1c_vehicle_struct.h",
                     "f1c_vehicle_struct_ida.h", "f1c_trace_schema.h"}

SDK_DOC_EXCLUDED_PATTERNS = ("*_REVIEW_*.md", "*_REVIEW.md", "*PLAN*.md",
                             "*HANDOFF*.md", "*ASSESSMENT*.md",
                             "LLM_TOOLING_IMPLEMENTATION_LOG.md",
                             "TEXT_CORRUPTION_REPORT_*.md")


def sdk_links(root, address):
    """Navigation-only links from SDK helpers/tests/docs to this address."""
    value = int(address, 16)
    patterns = [re.compile(rf"\b0x0*{value:X}u?\b", re.IGNORECASE),
                re.compile(rf"\bsub_0*{value:X}\b", re.IGNORECASE),
                re.compile(rf"\b{value:08X}\b", re.IGNORECASE)]

    def read_lines(path):
        try:
            return path.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError:
            return []

    def first_address_hit(path):
        for number, line in enumerate(read_lines(path), 1):
            if any(pattern.search(line) for pattern in patterns):
                return number, line
        return None

    def hit_record(path, number, line):
        return {"path": str(path.relative_to(root)).replace("\\", "/"),
                "line": number, "text": line.strip()[:160]}

    sdk_dir = root / "tools" / "sdk"
    helper_hits = []
    helper_stems = set()
    for path in sorted(sdk_dir.iterdir()):
        if not path.is_file() or path.suffix.lower() not in (".h", ".hpp", ".c"):
            continue
        if path.name in SDK_LINK_EXCLUDED:
            continue
        found = first_address_hit(path)
        if found:
            helper_hits.append(hit_record(path, *found))
            helper_stems.add(path.stem)

    test_hits = []
    test_dir = sdk_dir / "tests"
    if test_dir.is_dir():
        for path in sorted(test_dir.iterdir()):
            if not path.is_file() or path.suffix.lower() not in (".py", ".c", ".bat"):
                continue
            if path.name == "test_source_explorer.py":
                continue
            found = first_address_hit(path)
            if found is None and helper_stems:
                for number, line in enumerate(read_lines(path), 1):
                    if any(stem in line for stem in helper_stems):
                        found = (number, line)
                        break
            if found:
                test_hits.append(hit_record(path, *found))

    doc_hits = []
    for path in sorted(sdk_dir.glob("*.md")):
        if any(fnmatch.fnmatch(path.name, pattern)
               for pattern in SDK_DOC_EXCLUDED_PATTERNS):
            continue
        found = first_address_hit(path)
        if found:
            doc_hits.append(hit_record(path, *found))

    return {"note": "Navigation links only; trust tiers unchanged.",
            "docs_excluded_patterns": list(SDK_DOC_EXCLUDED_PATTERNS),
            "helpers": helper_hits[:10], "tests": test_hits[:10], "docs": doc_hits[:10],
            "totals": {"helpers": len(helper_hits), "tests": len(test_hits),
                       "docs": len(doc_hits)}}


def json_text(value):
    return json.dumps(value, indent=2, ensure_ascii=False) + "\n"
