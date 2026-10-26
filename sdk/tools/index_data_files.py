"""index_data_files.py — index the game's SHIPPED data files (the other half of F1C).

WHY: F1C is half data-driven. 2,300+ .hdv/.aiw/.rcd/.tbc/.gdb/.plr/.veh files ship
with the game and the corpus had ZERO coverage of them. Two concrete payoffs:

 1. Join with config_schema.json (token -> struct offset, recovered from the
    parsers) gives token -> offset -> REAL VALUES: units, ranges, which fields
    anyone ever varies, which are dead.
 2. A free offset prover. Put a distinctive value in a .hdv field, load the game,
    read the struct: if it lands at the predicted offset that is DERIVED -> FACT
    with no debugger and no disassembly. Repeatable, cheap, no live-poke session.

NOTHING IS COPIED into the corpus - this indexes values and aggregates only, so
the corpus stays lean and no game asset is duplicated.

Outputs: data_index.json   per (ext, section, key): file/occurrence counts,
                           numeric min/max/mean, distinct values (capped),
                           samples, Range/Setting garage-adjustable flag,
                           and the parser + struct offsets when known
         DATA_INDEX.md      readable summary + the three join verdicts:
                           matched / schema-token-never-in-files / file-key-with-no-parser

Usage: python tools/index_data_files.py [--root "<game install>"]
"""
import argparse
import io
import json
import os
import re
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_GAME = os.path.join(ROOT, "..", "F1 Challenge V10")
EXTS = ("hdv", "aiw", "rcd", "tbc", "gdb", "plr", "veh")

SECTION_RE = re.compile(r"^\s*\[([^\]]+)\]\s*$")
KV_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_ ]*?)\s*=\s*(.*)$")
NUM_RE = re.compile(r"^-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?$")

MAX_DISTINCT = 40      # keep the index small; counts stay exact past the cap
MAX_SAMPLES = 5
MAX_EXAMPLE_FILES = 3


def strip_comment(s):
    """ISI files use // comments, including trailing ones after a value."""
    i = s.find("//")
    return (s[:i] if i >= 0 else s).strip()


def parse_value(v):
    """-> (kind, numbers). kind: num | tuple | str | empty"""
    v = v.strip()
    if not v:
        return "empty", []
    if v.startswith("(") and v.endswith(")"):
        parts = [p.strip() for p in v[1:-1].split(",")]
        nums = [float(p) for p in parts if NUM_RE.match(p)]
        return "tuple", nums
    if NUM_RE.match(v):
        return "num", [float(v)]
    return "str", []


class Agg(object):
    __slots__ = ("n_occ", "files", "distinct", "n_distinct_seen", "lo", "hi",
                 "total", "n_num", "kinds", "examples")

    def __init__(self):
        self.n_occ = 0
        self.files = set()
        self.distinct = set()
        self.n_distinct_seen = 0
        self.lo = None
        self.hi = None
        self.total = 0.0
        self.n_num = 0
        self.kinds = collections.Counter()
        self.examples = []

    def add(self, raw, rel):
        self.n_occ += 1
        self.files.add(rel)
        if len(self.distinct) < MAX_DISTINCT:
            if raw not in self.distinct:
                self.distinct.add(raw)
                self.n_distinct_seen += 1
        kind, nums = parse_value(raw)
        self.kinds[kind] += 1
        for n in nums:
            self.lo = n if self.lo is None else min(self.lo, n)
            self.hi = n if self.hi is None else max(self.hi, n)
            self.total += n
            self.n_num += 1
        if len(self.examples) < MAX_SAMPLES and raw not in self.examples:
            self.examples.append(raw)

    def dump(self):
        d = {
            "occurrences": self.n_occ,
            "files": len(self.files),
            "kinds": dict(self.kinds),
            "samples": self.examples,
            "distinct_capped": len(self.distinct) >= MAX_DISTINCT,
            "distinct_seen": len(self.distinct),
            "example_files": sorted(self.files)[:MAX_EXAMPLE_FILES],
        }
        if self.n_num:
            d["min"] = self.lo
            d["max"] = self.hi
            d["mean"] = round(self.total / self.n_num, 6)
            d["constant"] = (self.lo == self.hi)
        return d


def scan(game_root):
    """(ext, section, key) -> Agg, walking every shipped data file."""
    data = collections.defaultdict(Agg)
    counts = collections.Counter()
    for dirpath, _dirs, files in os.walk(game_root):
        for fn in files:
            ext = fn.rsplit(".", 1)[-1].lower() if "." in fn else ""
            if ext not in EXTS:
                continue
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, game_root).replace("\\", "/")
            counts[ext] += 1
            section = "(top)"
            try:
                with io.open(full, encoding="latin-1", errors="replace") as f:
                    for line in f:
                        line = strip_comment(line)
                        if not line:
                            continue
                        m = SECTION_RE.match(line)
                        if m:
                            section = "[%s]" % m.group(1).strip().upper()
                            continue
                        m = KV_RE.match(line)
                        if not m:
                            continue
                        key = m.group(1).strip()
                        data[(ext, section, key)].add(m.group(2).strip(), rel)
            except (IOError, OSError):
                continue
    return data, counts


def load_schema():
    """token(lower) -> [{parser, section, offsets, setter}] from the parsers."""
    p = os.path.join(ROOT, "config_schema.json")
    if not os.path.exists(p):
        return {}
    with io.open(p, encoding="utf-8") as f:
        d = json.load(f)
    out = collections.defaultdict(list)
    for parser, pd in d.get("parsers", {}).items():
        for sec, items in pd["sections"].items():
            for e in items:
                out[e["token"].lower()].append({
                    "parser": parser, "parser_section": sec,
                    "offsets": e["offsets"], "setter": e["setter"]})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=DEFAULT_GAME)
    args = ap.parse_args()

    data, counts = scan(args.root)
    schema = load_schema()

    # A Key with a KeyRange/KeySetting sibling is player-adjustable in the
    # garage - that is the difference between a value a mod must patch and one
    # the player can already change.
    keys_by_ext = collections.defaultdict(set)
    for (ext, _sec, key) in data:
        keys_by_ext[ext].add(key.lower())

    index = {}
    matched = unmatched_file_keys = 0
    for (ext, sec, key), agg in sorted(data.items()):
        rec = agg.dump()
        kl = key.lower()
        if kl in schema:
            rec["parsed_by"] = schema[kl]
            matched += 1
        else:
            unmatched_file_keys += 1
        base = re.sub(r"(range|setting)$", "", kl)
        if base != kl:
            rec["garage_adjustable_part"] = True
        elif (kl + "range") in keys_by_ext[ext] and (kl + "setting") in keys_by_ext[ext]:
            rec["garage_adjustable"] = True
        index.setdefault(ext, {}).setdefault(sec, {})[key] = rec

    seen_tokens = set(k.lower() for (_e, _s, k) in data)
    schema_only = sorted(t for t in schema if t not in seen_tokens)

    out = {
        "_meta": {
            "tier": "extraction",
            "game_root": args.root,
            "files_scanned": dict(counts),
            "n_keys": len(data),
            "n_keys_matched_to_parser": matched,
            "n_keys_no_parser": unmatched_file_keys,
            "n_schema_tokens_absent_from_files": len(schema_only),
            "note": ("values indexed, no game asset copied; a key with no parser "
                     "means the parser was not among the 13 extracted OR the game "
                     "ignores the key - not proof of either"),
        },
        "schema_tokens_absent_from_files": schema_only,
        "index": index,
    }
    with io.open(os.path.join(ROOT, "data_index.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, ensure_ascii=False, sort_keys=True)

    L = ["# DATA_INDEX - the shipped data files, indexed", "",
         "Generated by `tools/index_data_files.py`. **Extraction tier**: counts and "
         "values read straight out of the files that ship with the game. No asset is "
         "copied into the corpus.", "",
         "| ext | files |", "|---|---|"]
    for e, n in sorted(counts.items()):
        L.append("| `.%s` | %d |" % (e, n))
    L += ["", "| metric | value |", "|---|---|",
          "| distinct (ext, section, key) | %d |" % len(data),
          "| keys matched to an extracted parser | %d |" % matched,
          "| keys with no known parser | %d |" % unmatched_file_keys,
          "| parser tokens never seen in any file | %d |" % len(schema_only), ""]

    L += ["## Keys joined to a struct offset", "",
          "Token, the offset its parser writes, and what the shipped files actually "
          "put there. **This is the free offset prover**: set one of these to a "
          "distinctive value, load, and read the struct at the offset.", "",
          "| ext | section | key | offsets | files | min | max | constant |",
          "|---|---|---|---|---|---|---|---|"]
    rows = 0
    for ext, secs in sorted(index.items()):
        for sec, keys in sorted(secs.items()):
            for key, rec in sorted(keys.items()):
                if "parsed_by" not in rec:
                    continue
                offs = ", ".join(rec["parsed_by"][0]["offsets"])
                L.append("| .%s | %s | `%s` | %s | %d | %s | %s | %s |" %
                         (ext, sec, key, offs, rec["files"],
                          rec.get("min", ""), rec.get("max", ""),
                          "yes" if rec.get("constant") else ""))
                rows += 1
    L += ["", "%d joined keys." % rows, ""]

    # Keys the files use heavily but no extracted parser handles = the parsers
    # still to be recovered, ranked by how much the shipped data leans on them.
    unmatched = []
    for ext, secs in index.items():
        for sec, keys in secs.items():
            for key, rec in keys.items():
                if "parsed_by" not in rec:
                    unmatched.append((rec["files"], ext, sec, key, rec))
    unmatched.sort(reverse=True)
    L += ["## Most-used keys with no parser yet (the next parsers to recover)", "",
          "| ext | section | key | files | samples |", "|---|---|---|---|---|"]
    for nfiles, ext, sec, key, rec in unmatched[:60]:
        L.append("| .%s | %s | `%s` | %d | %s |" %
                 (ext, sec, key, nfiles,
                  ", ".join(str(s)[:18] for s in rec["samples"][:3])))
    L += ["",
          "## Parser tokens never present in any shipped file", "",
          "The parser handles them, no file uses them: engine defaults, or features "
          "cut before release. Safe places to look for unused behaviour.", "",
          "```", ", ".join(schema_only) or "(none)", "```", ""]

    with io.open(os.path.join(ROOT, "DATA_INDEX.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(L))

    print("[data] %d files, %d distinct keys, %d joined to a parser offset, "
          "%d parser tokens unused in files -> data_index.json / DATA_INDEX.md"
          % (sum(counts.values()), len(data), matched, len(schema_only)))


if __name__ == "__main__":
    main()
