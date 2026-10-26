#!/usr/bin/env python3
"""Query callers, callees, offsets, ownership and subsystem tags."""
from pathlib import Path
import argparse
import json

ROOT = Path(__file__).resolve().parents[1]
index = json.loads((ROOT / "analysis" / "recovery_index.json").read_text(encoding="utf-8"))
functions = index["functions"]
parser = argparse.ArgumentParser()
parser.add_argument("query", help="function name fragment or 0x-address")
parser.add_argument("--depth", type=int, default=1, choices=range(0, 4))
args = parser.parse_args()

needle = args.query.lower()
matches = [key for key, node in functions.items()
           if needle in key.lower() or needle in node["name"].lower() or
           needle in node["declared_name"].lower()]
if not matches:
    raise SystemExit("No matching function")

shown = set()
def show(key, depth, prefix=""):
    if (key, prefix) in shown:
        return
    shown.add((key, prefix))
    node = functions[key]
    owner = node["object_kind"] or "unknown owner"
    print(f"{prefix}{key} {node['name']} [{owner}] offsets={','.join(node['this_offsets']) or '-'}")
    if depth:
        for callee in node["callees"]:
            show(callee, depth - 1, prefix + "  -> ")

for key in matches:
    show(key, args.depth)
    callers = functions[key]["callers"]
    if callers:
        print("  callers:", ", ".join(f"{item} {functions[item]['name']}" for item in callers))

