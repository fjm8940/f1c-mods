#!/usr/bin/env python3
"""Compare normalized SDK JSONL field-transition sequences."""
from pathlib import Path
import argparse
import difflib
import json

parser = argparse.ArgumentParser()
parser.add_argument("vanilla", type=Path)
parser.add_argument("rewrite", type=Path)
parser.add_argument("--semantic", action="append")
parser.add_argument("--name", action="append")
parser.add_argument("--require-match", action="store_true")
args = parser.parse_args()

def load(path):
    events = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip(): continue
        event = json.loads(line)
        if event.get("event") != "field_change": continue
        if args.semantic and event.get("semantic") not in args.semantic: continue
        if args.name and event.get("name") not in args.name: continue
        signature = f"{event['name']}:{event.get('before')!r}->{event.get('after')!r}"
        events.append((signature, number, event))
    return events

vanilla = load(args.vanilla)
rewrite = load(args.rewrite)
left = [item[0] for item in vanilla]
right = [item[0] for item in rewrite]
matcher = difflib.SequenceMatcher(a=left, b=right, autojunk=False)
changes = 0
for tag, i1, i2, j1, j2 in matcher.get_opcodes():
    if tag == "equal": continue
    changes += 1
    print(f"{tag}: vanilla[{i1}:{i2}] rewrite[{j1}:{j2}]")
    for signature, line, _ in vanilla[i1:i2][:8]:
        print(f"  - V line {line}: {signature}")
    for signature, line, _ in rewrite[j1:j2][:8]:
        print(f"  + R line {line}: {signature}")
if not changes:
    print(f"Trace transition sequences match exactly ({len(left)} events).")
else:
    ratio = matcher.ratio()
    print(f"{changes} divergence blocks; sequence similarity {ratio:.3f}")
    if args.require_match:
        raise SystemExit(1)
