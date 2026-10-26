"""
import_crossbinary_names.py — Import class::method names from the CONFIRMED
F1C<->NASCAR match table (cross_binary_final.csv, ISI engine identity) onto our
sub_XXXXXX functions.

Safety: a name is only proposed if
  1. the match confidence is HIGH or BYTE_EXACT, and
  2. our function exists in the corpus, and
  3. the current IDA name is generic (sub_/nullsub_/unknown) OR agrees.
Never overwrites an existing meaningful name — conflicts are reported instead.

Output: tools/crossbinary_names.json
  {addr8: {proposed, class, method, confidence, current_name, fill_label}}
Trust tier for imported names: CONFIRMED (independent-binary source), but the
tier is only claimed for HIGH/BYTE_EXACT rows.
"""
import os
import csv
import re
import json

IDA_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV = os.path.join(IDA_ROOT, "tools", "cross_binary_final.csv")

facts = json.load(open(os.path.join(IDA_ROOT, "skeleton_facts_all.json"), encoding="utf-8"))
FILLED = os.path.join(IDA_ROOT, "skeletons_filled")

out, conflicts, skipped = {}, [], 0
for r in csv.DictReader(open(CSV, encoding="utf-8")):
    name = (r["name"] or "").strip()
    if not name or name.endswith("::?") or "?" in name:
        continue
    if r["confidence"] not in ("HIGH", "BYTE_EXACT"):
        skipped += 1
        continue
    addr8 = r["f1c_addr"].lower().replace("0x", "").rjust(8, "0")
    f = facts.get(addr8)
    if not f:
        continue
    cur = f["name"]
    generic = bool(re.match(r'^(sub|nullsub|loc|j)_', cur))
    lbl = None
    p = os.path.join(FILLED, f"{cur}_{addr8}.md")
    if os.path.exists(p):
        m = re.search(r'^## \S+ — (.+)$',
                      open(p, encoding="utf-8", errors="replace").read(), re.M)
        lbl = m.group(1).strip() if m else None
    entry = {"proposed": name, "class": r["class"], "method": r["method"],
             "confidence": r["confidence"], "current_name": cur,
             "fill_label": lbl, "trust": "CONFIRMED"}
    if generic:
        out[addr8] = entry
    else:
        conflicts.append((addr8, cur, name))

json.dump(out, open(os.path.join(IDA_ROOT, "tools", "crossbinary_names.json"),
                    "w", encoding="utf-8"), indent=1)
print(f"proposed names for {len(out)} generic-named functions -> tools/crossbinary_names.json")
print(f"skipped low-confidence: {skipped}")
print(f"named-vs-named (kept ours, review manually): {len(conflicts)}")
for a, c, n in conflicts[:10]:
    print(f"  {a}: ours '{c}' vs cross-binary '{n}'")
