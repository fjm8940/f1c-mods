#!/usr/bin/env python3
"""Generate an orientation report for one recovered subsystem tag."""
from pathlib import Path
import argparse
import json

ROOT = Path(__file__).resolve().parents[1]
index = json.loads((ROOT / "analysis" / "recovery_index.json").read_text(encoding="utf-8"))
parser = argparse.ArgumentParser()
parser.add_argument("subsystem", choices=sorted(index["subsystems"]))
args = parser.parse_args()
functions = index["functions"]
keys = index["subsystems"][args.subsystem]
nodes = [functions[key] for key in keys]
reviewed = [node for node in nodes if node["object_kind"]]
ranked = sorted(nodes, key=lambda node: -(len(node["callers"]) + len(node["callees"])))

lines = [
    f"# Recovered {args.subsystem.title()} Subsystem",
    "",
    "Generated orientation report. Names and graph edges are static-analysis",
    "leads unless a row explicitly carries a reviewed object owner.",
    "",
    f"- Tagged functions: {len(nodes)}",
    f"- Reviewed owner methods: {len(reviewed)}",
    "",
    "## Reviewed object methods", "",
]
for node in reviewed:
    lines.append(f"- `{node['address']}` `{node['name']}` — `{node['object_kind']}*` ({node['object_trust']})")
if not reviewed:
    lines.append("- None yet.")
lines += ["", "## Highest-connectivity functions", ""]
for node in ranked[:40]:
    lines.append(
        f"- `{node['address']}` `{node['name']}` — "
        f"{len(node['callers'])} callers, {len(node['callees'])} callees; "
        f"owner `{node['object_kind'] or 'unreviewed'}`")
lines += ["", "## Review policy", "",
          "Use `tools/query_recovery_index.py <name> --depth 2` to inspect a",
          "candidate. Promote object ownership only after caller/constructor or",
          "direct-layout evidence; never apply DERIVED same-`this` proposals automatically.", ""]
out = ROOT / "analysis" / "subsystems" / f"{args.subsystem}.md"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("\n".join(lines), encoding="utf-8", newline="\n")
print(f"subsystem report: {len(nodes)} functions -> {out.relative_to(ROOT)}")

