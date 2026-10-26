#!/usr/bin/env python3
"""Focused tests for tools/source_explorer.py address resolution."""
from pathlib import Path
import json
import os
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
import source_explorer

recovery = json.loads((ROOT / "analysis" / "recovery_index.json").read_text(encoding="utf-8"))

EXPECTED_ADDRESS = "0x004C6E80"
for value in ("0x4C6E80", "0X4c6e80", "4C6E80", "004C6E80", "0x004C6E80", "sub_4C6E80"):
    resolved = source_explorer.resolve_address(recovery, value)
    if resolved != EXPECTED_ADDRESS:
        raise SystemExit(
            f"resolve_address({value!r}) -> {resolved}, expected {EXPECTED_ADDRESS}")

resolved = source_explorer.resolve_address(recovery, "SetDriverVisibility")
if resolved != "0x004C7AA0":
    raise SystemExit(
        f"resolve_address('SetDriverVisibility') -> {resolved}, expected 0x004C7AA0")

for value in ("0xZZ", "0x12345678", ""):
    try:
        source_explorer.resolve_address(recovery, value)
    except source_explorer.SourceExplorerError:
        pass
    else:
        raise SystemExit(f"resolve_address({value!r}) should have raised SourceExplorerError")

def check_search_equivalence():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        for directory in ("functions", "functions_annotated", "skeletons", "skeletons_filled"):
            (root / directory).mkdir()
        (root / ".gitignore").write_text("functions/\n", encoding="utf-8")
        files = {
            "functions/Alpha_0040aaaa.c": "SetDriverVisibility handles driver input as DRIVER\npit lane entry\n",
            "functions/Beta_0040aaab.c": "PIT LANE manager\nDriver seat\n",
            "functions/Gamma_0040aaac.c": "driver only in lowercase here\n",
            "functions/Eta_0040aab0.c": "no keyword present\n",
            "functions_annotated/Delta_0040aaad.c": "annotated DRIVER copy\n",
            "functions_annotated/Kappa_0040aab3.c": "unrelated annotated text\n",
            "skeletons/Epsilon_0040aaae.md": "driver facts skeleton\n",
            "skeletons_filled/Zeta_0040aaaf.md": "pit lane derived prose\n",
        }
        for relative, text in files.items():
            (root / relative).write_text(text, encoding="utf-8")
        recovery = {"functions": {}}
        queries = [
            {"query": "driver"},
            {"query": "driver pit", "mode": "all"},
            {"query": "driver absenttoken", "mode": "any"},
            {"query": "DRIVER", "case_sensitive": True},
            {"query": "zzzznomatchzzzz"},
            {"query": "déjà"},
            {"query": "Driver|Pit", "regex": True},
        ]
        saved = os.environ.get("F1C_SEARCH_NO_RG")
        try:
            for case in queries:
                os.environ["F1C_SEARCH_NO_RG"] = "1"
                baseline = source_explorer.search_sources(root, recovery, **case)
                os.environ.pop("F1C_SEARCH_NO_RG", None)
                prefilt = source_explorer.search_sources(root, recovery, **case)
                if baseline != prefilt:
                    raise SystemExit(
                        f"search prefilter changed results for {case}: "
                        f"{baseline} != {prefilt}")
        finally:
            if saved is None:
                os.environ.pop("F1C_SEARCH_NO_RG", None)
            else:
                os.environ["F1C_SEARCH_NO_RG"] = saved


def check_two_corpus_order():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        for directory in ("functions", "functions_annotated", "skeletons", "skeletons_filled"):
            (root / directory).mkdir()
        files = {
            "functions/Alpha_0040aaaa.c": "driver raw one\n",
            "functions/Beta_0040aaab.c": "driver raw two\n",
            "functions/Gamma_0040aaac.c": "driver raw three\n",
            "functions_annotated/Delta_0040aaad.c": "driver annotated four\n",
            "skeletons/Alpha_0040aaaa.md": "driver facts one\n",
            "skeletons/Beta_0040aaab.md": "driver facts two\n",
            "skeletons/Delta_0040aaad.md": "driver facts three\n",
            "skeletons_filled/Epsilon_0040aaae.md": "driver derived four\n",
        }
        for relative, text in files.items():
            (root / relative).write_text(text, encoding="utf-8")
        recovery = {"functions": {}}
        for corpora in (["raw", "facts"], ["facts", "raw"]):
            saved = os.environ.get("F1C_SEARCH_NO_RG")
            try:
                os.environ["F1C_SEARCH_NO_RG"] = "1"
                baseline = source_explorer.search_sources(root, recovery, "driver",
                                                          corpora=corpora)
                os.environ.pop("F1C_SEARCH_NO_RG", None)
                prefilt = source_explorer.search_sources(root, recovery, "driver",
                                                         corpora=corpora)
            finally:
                if saved is None:
                    os.environ.pop("F1C_SEARCH_NO_RG", None)
                else:
                    os.environ["F1C_SEARCH_NO_RG"] = saved
            if baseline != prefilt:
                raise SystemExit(f"two-corpus search changed results for {corpora}")
            order = {item["address"]: [source["corpus"] for source in item["sources"]]
                     for item in prefilt}
            if order.get("0x0040AAAA") != corpora:
                raise SystemExit(
                    f"record order {order.get('0x0040AAAA')} != corpus order {corpora}")


def check_inspection_links():
    catalogue = json.loads((ROOT / "tools" / "sdk" / "f1c_sdk.json").read_text(encoding="utf-8"))
    result = source_explorer.inspect_function(ROOT, recovery, catalogue, "0x004C6E80")
    trust_before = result["reviewed"]["trust"] if result["reviewed"] else None
    links = source_explorer.sdk_links(ROOT, "0x004C6E80")
    helper_paths = [hit["path"] for hit in links["helpers"]]
    if not any(path.endswith("f1c_vehicle_native_clear.h") for path in helper_paths):
        raise SystemExit(f"SDK helper link missing: {helper_paths}")
    test_paths = [hit["path"] for hit in links["tests"]]
    if not any(path.endswith("test_vehicle_native_clear.py") for path in test_paths):
        raise SystemExit(f"SDK test link missing: {test_paths}")
    if any(path.endswith("test_source_explorer.py") for path in test_paths):
        raise SystemExit(f"the explorer test linked itself: {test_paths}")
    doc_paths = [hit["path"] for hit in links["docs"]]
    for banned in ("HANDOFF", "PLAN", "ASSESSMENT"):
        if any(banned in path for path in doc_paths):
            raise SystemExit(f"excluded planning document linked: {doc_paths}")
    if links.get("docs_excluded_patterns") != list(source_explorer.SDK_DOC_EXCLUDED_PATTERNS):
        raise SystemExit("docs_excluded_patterns missing or wrong")
    trust_after = result["reviewed"]["trust"] if result["reviewed"] else None
    if trust_after != trust_before:
        raise SystemExit("sdk_links changed the reviewed trust")

    bounded = source_explorer.attach_node_lookup(result, recovery)
    for address, node in bounded["node_lookup"].items():
        if node is not None and set(node) != {"name", "declared_name"}:
            raise SystemExit(f"node lookup not bounded for {address}: {sorted(node)}")

    rendered = source_explorer.render_inspection(result, max_neighbors=1)
    if "(+" not in rendered:
        raise SystemExit("render_inspection did not bound the neighbour list")


def check_prefilter_notice():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        for directory in ("functions", "functions_annotated", "skeletons", "skeletons_filled"):
            (root / directory).mkdir()
        (root / "functions" / "Alpha_0040aaaa.c").write_text(
            "SetDriverVisibility handles driver input\n", encoding="utf-8")
        recovery = {"functions": {}}
        saved = os.environ.get("F1C_SEARCH_NO_RG")
        try:
            os.environ.pop("F1C_SEARCH_NO_RG", None)
            source_explorer.search_sources(root, recovery, "driver")
            if source_explorer.LAST_PREFILTER != {"used": True, "reason": None}:
                raise SystemExit(f"plain search prefilter state wrong: {source_explorer.LAST_PREFILTER}")
            os.environ["F1C_SEARCH_NO_RG"] = "1"
            source_explorer.search_sources(root, recovery, "driver")
            if source_explorer.LAST_PREFILTER != {
                    "used": False, "reason": "disabled by F1C_SEARCH_NO_RG"}:
                raise SystemExit(f"disabled prefilter state wrong: {source_explorer.LAST_PREFILTER}")
            os.environ.pop("F1C_SEARCH_NO_RG", None)
            source_explorer.search_sources(root, recovery, "Driver|Pit", regex=True)
            if source_explorer.LAST_PREFILTER != {"used": False, "reason": "regex mode"}:
                raise SystemExit(f"regex prefilter state wrong: {source_explorer.LAST_PREFILTER}")
            source_explorer.search_sources(root, recovery, "déjà")
            if source_explorer.LAST_PREFILTER != {"used": False, "reason": "non-ASCII search term"}:
                raise SystemExit(f"non-ASCII prefilter state wrong: {source_explorer.LAST_PREFILTER}")
        finally:
            if saved is None:
                os.environ.pop("F1C_SEARCH_NO_RG", None)
            else:
                os.environ["F1C_SEARCH_NO_RG"] = saved


check_search_equivalence()
check_two_corpus_order()
check_inspection_links()
check_prefilter_notice()

print("Source explorer passed: address/name resolution and refusal verified.")
