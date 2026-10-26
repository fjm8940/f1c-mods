"""
regen_knowledge_graph.py — Rebuild this corpus's knowledge_graph/*.json
from the IDA corpus (skeleton_facts_all.json + fills), replacing the old
Ghidra-derived graph (13-20% name error rate) with IDB-proven edges.

Rewrites (originals backed up as *.pre_ida_bak once):
  call_graph.json    {addr: [{callee, description}]}
  callers.json       {addr: [{caller, caller_name, description}]}
  globals_usage.json {DAT_xxxxxxxx: {name, module, functions:[{function,
                      function_name, access}]}}
  functions.json     {addr: {address, name, summary, ...}}

Descriptions/summaries come from skeletons_filled/ (DERIVED trust — labeled).
Run from anywhere; paths are absolute.
"""
import os
import re
import json
import shutil

IDA_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Detached: knowledge graph now lives IN this corpus (mod_pipeline/asm_tools read it here)
KG_DIR = os.path.join(IDA_ROOT, "knowledge_graph")

facts = json.load(open(os.path.join(IDA_ROOT, "skeleton_facts_all.json"), encoding="utf-8"))
cg = json.load(open(os.path.join(IDA_ROOT, "callgraph.json"), encoding="utf-8"))
ni = json.load(open(os.path.join(IDA_ROOT, "name_index.json"), encoding="utf-8"))
FILLED = os.path.join(IDA_ROOT, "skeletons_filled")


def fill_meta(name, addr):
    p = os.path.join(FILLED, f"{name}_{addr}.md")
    if not os.path.exists(p):
        return None, None
    t = open(p, encoding="utf-8", errors="replace").read()
    m = re.search(r'^## \S+ — (.+)$', t, re.M)
    s = re.search(r'\*\*Summary:\*\* (.+)$', t, re.M)
    return (m.group(1).strip() if m else None,
            s.group(1).strip() if s else None)


def a10(addr8):          # '0042d5d0' -> '0x0042d5d0' (old graph key style)
    return "0x" + addr8


labels = {}   # addr8 -> short label for edge descriptions
summaries = {}
for addr8, f in facts.items():
    lbl, summ = fill_meta(f["name"], addr8)
    labels[addr8] = lbl or (f["name"] if not f["name"].startswith("sub_") else None)
    summaries[addr8] = summ

call_graph = {}
callers = {}
for addr8, f in facts.items():
    edges = []
    for c in f["callees"]:
        ca = c["addr"]
        edges.append({"callee": a10(ca), "description": labels.get(ca)})
        callers.setdefault(a10(ca), []).append(
            {"caller": a10(addr8),
             "caller_name": facts[addr8]["name"],
             "description": labels.get(addr8)})
    if edges:
        call_graph[a10(addr8)] = edges

g_names = {k.lower(): v for k, v in ni.get("globals", {}).items()}
globals_usage = {}
for addr8, f in facts.items():
    for g in f["globals"]:
        key = "DAT_" + g["addr"].lstrip("0").rjust(8, "0")
        e = globals_usage.setdefault(key, {"name": None, "module": None,
                                           "trust": None, "functions": []})
        idx = g_names.get("0x" + g["addr"].lower()) or g_names.get("0x00" + g["addr"].lower()[-6:])
        if idx and not e["name"]:
            e["name"], e["trust"] = idx["name"], idx.get("trust")
        if not e["name"] and g["name"] and not g["name"].startswith(("dword_", "byte_", "unk_", "flt_", "word_", "dbl_")):
            e["name"] = g["name"]
        e["functions"].append({"function": a10(addr8),
                               "function_name": facts[addr8]["name"],
                               "access": {"R": "read", "W": "write",
                                          "RW": "read_write"}.get(g["rw"], g["rw"])})

functions = {}
for addr8, f in facts.items():
    functions[a10(addr8)] = {
        "address": a10(addr8),
        "name": f["name"],
        "label": labels.get(addr8),
        "summary": summaries.get(addr8),
        "on_frame_path": cg.get(addr8, {}).get("on_frame_path", False),
        "n_facts": len(f["fields"]) + len(f["globals"]) + len(f["guards"]) + len(f["callees"]),
        "source": "ida_corpus (skeleton_facts_all + skeletons_filled, DERIVED prose)",
    }

for fname, data in [("call_graph.json", call_graph), ("callers.json", callers),
                    ("globals_usage.json", globals_usage), ("functions.json", functions)]:
    p = os.path.join(KG_DIR, fname)
    bak = p + ".pre_ida_bak"
    if os.path.exists(p) and not os.path.exists(bak):
        shutil.copy(p, bak)
    json.dump(data, open(p, "w", encoding="utf-8"), indent=1)
    print(f"{fname}: {len(data)} entries (old backed up: {os.path.basename(bak)})")
