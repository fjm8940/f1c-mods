"""ask.py — one question, one answer, every layer joined. The corpus's front door.

WHY THIS EXISTS: the facts live in eight stores with three different key formats
(addr8 in skeleton_facts_all, Name_addr.c filenames, Name_addr.md fills,
offset-paths in runtime_facts, hex-with-0x in name_index). Every session used to
re-join those by hand with ls/grep before doing any actual work. This collapses
that to one call, and prints a TRUST TIER on every line so a DERIVED guess can
never be mistaken for a poke-proven FACT.

    python tools/ask.py func    <addr|name>     full dossier on a function
    python tools/ask.py offset  <hex>           what is at vehicle struct +N
    python tools/ask.py global  <hex|name>      what is at a global address
    python tools/ask.py token   <text>          config-file token / string search
    python tools/ask.py findings <text>         has this been tried / disproven?
    python tools/ask.py stats                   corpus inventory

Read-only. Never writes to the corpus. Adding evidence stays a deliberate,
cited act (CLAUDE.md hard rule 1).
"""
import io
import json
import os
import re
import sys
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAXLIST = 25          # keep answers token-bounded; counts are always reported


def load(rel, default=None):
    p = os.path.join(ROOT, rel)
    if not os.path.exists(p):
        return default
    with io.open(p, encoding="utf-8", errors="replace") as f:
        return json.load(f)


def load_jsonl(rel):
    p = os.path.join(ROOT, rel)
    if not os.path.exists(p):
        return []
    out = []
    with io.open(p, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("//"):
                out.append(json.loads(line))
    return out


def checkpoint_records():
    """Dated automation-checkpoint entries as RECORDED, not tier-reviewed."""
    p = os.path.join(ROOT, "tools", "sdk", "AUTOMATION_CHECKPOINT.json")
    if not os.path.exists(p):
        return []
    try:
        with io.open(p, encoding="utf-8", errors="replace") as f:
            data = json.load(f)
    except (ValueError, OSError):
        return []
    out = []
    for key, v in data.items():
        m = re.search(r"(20\d{2})_?(\d{2})_?(\d{2})$", key)
        if not m or not isinstance(v, dict):
            continue
        out.append({
            "id": "checkpoint:" + key,
            "tier": "RECORDED",
            "claim": str(v.get("state") or v.get("status") or ""),
            "evidence": str(v.get("evidence") or ""),
            "note": ("pending: " + str(v["pending"])) if v.get("pending") else "",
            "source": "tools/sdk/AUTOMATION_CHECKPOINT.json#" + key,
            "date": "-".join(m.groups()),
            "origin": "checkpoint",
        })
    return out


def norm_addr(s):
    """Any address spelling -> 8-hex-digit lowercase, or None."""
    if not s:
        return None
    t = s.strip().lower().replace("0x", "")
    if re.fullmatch(r"[0-9a-f]{1,8}", t):
        return t.rjust(8, "0")
    return None


def norm_off(s):
    t = s.strip().lower().replace("0x", "")
    if re.fullmatch(r"[0-9a-f]{1,5}", t):
        return int(t, 16)
    return None


def more(n):
    return "  ... %d more" % n if n > 0 else ""


def cap(seq, n=MAXLIST):
    seq = list(seq)
    return seq[:n], max(0, len(seq) - n)


# --------------------------------------------------------------------------
class Corpus(object):
    def __init__(self):
        self.facts = load("skeleton_facts_all.json", {})
        self.names = load("name_index.json", {}) or {}
        self.callers = load("knowledge_graph/callers.json", {}) or {}
        self.priority = load("priority_index.json", []) or []
        self.runtime = load("runtime_facts.json", {}) or {}
        self.strings = load("strings_index.json", {}) or {}
        self.modules = load("strings_modules.json", {}) or {}
        self.schema = load("config_schema.json", {}) or {}
        self.data_index = load("data_index.json", {}) or {}
        reviewed = load_jsonl("findings.jsonl")
        recorded = checkpoint_records()
        self.reviewed_findings_count = len(reviewed)
        self.recorded_findings_count = len(recorded)
        self.findings = reviewed + recorded

        self.prio_by_addr = {p["addr"]: p for p in self.priority}
        self.name_to_addr = {}
        for a, v in self.facts.items():
            self.name_to_addr.setdefault((v.get("name") or "").lower(), a)

        # function addr -> source .cpp module, from the MSVC __FILE__ strings
        self.mod_of = {}
        for src, d in self.modules.items():
            for addr in d.get("functions", {}):
                self.mod_of[addr.lower()] = src

    # -- lookups ----------------------------------------------------------
    def resolve_func(self, q):
        a = norm_addr(q)
        if a and a in self.facts:
            return a
        ql = q.lower()
        if ql in self.name_to_addr:
            return self.name_to_addr[ql]
        hits = [a2 for n, a2 in self.name_to_addr.items() if ql in n]
        if len(hits) == 1:
            return hits[0]
        if hits:
            print("ambiguous, %d matches:" % len(hits))
            for a2 in hits[:MAXLIST]:
                print("  %s  %s" % (a2, self.facts[a2]["name"]))
            sys.exit(1)
        return None

    def src_path(self, addr):
        name = self.facts[addr]["name"]
        for d, ext in (("functions", ".c"), ("skeletons_filled", ".md")):
            p = os.path.join(ROOT, d, "%s_%s%s" % (name, addr, ext))
            if os.path.exists(p):
                yield os.path.join(d, "%s_%s%s" % (name, addr, ext))

    def veh_field(self, off):
        """name_index vehicle field entry for an offset, tier included."""
        vf = self.names.get("vehicle_fields", {})
        for key in ("0x%X" % off, "0x%x" % off, hex(off)):
            if key in vf:
                return vf[key]
        return None

    def global_entry(self, addr8):
        g = self.names.get("globals", {})
        for key in ("0x%08X" % int(addr8, 16), "0x%X" % int(addr8, 16),
                    "0x%08x" % int(addr8, 16)):
            if key in g:
                return g[key]
        return None

    def find_findings(self, *needles):
        out = []
        for f in self.findings:
            blob = json.dumps(f, ensure_ascii=False).lower()
            if all(n.lower() in blob for n in needles if n):
                out.append(f)
        return out


def print_findings(fs, label="findings"):
    if not fs:
        return
    print("\n%s (%d) - prior art, read before re-deriving:" % (label, len(fs)))
    shown, rest = cap(fs, 10)
    if any(f.get("tier") == "RECORDED" for f in shown):
        print("RECORDED = copied from the automation checkpoint; not tier-reviewed; verify via its evidence link.")
    for f in shown:
        print("  [%s] %s" % (f.get("tier", "?"), f.get("claim", "")))
        if f.get("evidence"):
            print("        evidence: %s" % f["evidence"])
        if f.get("source"):
            print("        source:   %s" % f["source"])
    print(more(rest))


# --------------------------------------------------------------------------
def cmd_func(c, q):
    addr = c.resolve_func(q)
    if not addr:
        print("no function matches %r" % q)
        return 1
    f = c.facts[addr]
    print("=== %s  @0x%s ===" % (f["name"], addr))

    if addr in c.mod_of:
        print("module:   %s   [extraction: MSVC __FILE__ string]" % c.mod_of[addr])
    p = c.prio_by_addr.get(addr)
    if p:
        print("priority: score %s%s" % (p.get("score"),
                                        "  on_frame" if p.get("on_frame") else ""))
        if p.get("named_fields"):
            nf, rest = cap(p["named_fields"], 12)
            print("known vehicle fields touched: %s%s" % (", ".join(nf),
                                                          more(rest)))

    for rel in c.src_path(addr):
        print("source:   %s" % rel)

    callers = c.callers.get("0x" + addr, [])
    if callers:
        shown, rest = cap(callers, 12)
        print("\ncallers (%d):" % len(callers))
        for x in shown:
            print("  %s  %s" % (x["caller"], x.get("caller_name") or ""))
        print(more(rest))
    else:
        print("\ncallers: none recorded (entry point, vtable-only, or unanalysed)")

    if f["callees"]:
        shown, rest = cap(f["callees"], 12)
        print("\ncallees (%d):" % len(f["callees"]))
        for x in shown:
            print("  %s  %s" % (x["addr"], x["name"]))
        print(more(rest))

    if f["globals"]:
        shown, rest = cap(f["globals"], 15)
        print("\nglobals touched (%d):" % len(f["globals"]))
        for g in shown:
            e = c.global_entry(g["addr"])
            tier = "  [%s: %s]" % (e["trust"], e["name"]) if e else ""
            print("  %s %-3s %s%s" % (g["addr"], g["rw"], g["name"], tier))
        print(more(rest))

    if f["fields"]:
        shown, rest = cap(f["fields"], 20)
        print("\nstruct fields touched (%d) - offsets are relative to THIS "
              "function's `this`:" % len(f["fields"]))
        for x in shown:
            vf = c.veh_field(int(x["off_hex"], 16))
            tier = "  [%s: %s]" % (vf["trust"], vf["name"]) if vf else ""
            print("  %-8s %-3s base=%s%s" % (x["off_hex"], x["rw"], x["base"], tier))
        print(more(rest))

    if f["guards"]:
        shown, rest = cap(f["guards"], 12)
        print("\nguards (%d):" % len(f["guards"]))
        for g in shown:
            print("  %s %s %s" % (g["off_hex"], g["op"], g.get("value_hex", g.get("value"))))
        print(more(rest))

    sf = c.strings.get("by_function", {}).get(addr, [])
    if sf:
        shown, rest = cap(sf, 15)
        print("\nstrings referenced (%d):" % len(sf))
        for s in shown:
            print("  %r" % s["s"][:90])
        print(more(rest))

    for pname, d in c.schema.get("parsers", {}).items():
        if pname.endswith(addr):
            print("\nCONFIG PARSER: %d fields in %d sections — see CONFIG_SCHEMA.md"
                  % (d["n_fields"], len(d["sections"])))

    print_findings(c.find_findings(addr) + c.find_findings(f["name"]))
    return 0


def cmd_offset(c, q):
    off = norm_off(q)
    if off is None:
        print("not an offset: %r" % q)
        return 1
    print("=== vehicle struct +0x%X ===" % off)

    dis = c.names.get("_meta", {}).get("disproven_offsets", [])
    if ("0x%X" % off) in [d.upper() for d in dis]:
        print("!! LISTED AS DISPROVEN in name_index._meta — do not build on it")

    vf = c.veh_field(off)
    if vf:
        print("name:     %s   [%s]" % (vf["name"], vf["trust"]))
        if vf.get("type"):
            print("type:     %s" % vf["type"])
        if vf.get("sources"):
            print("sources:  %s" % ", ".join(vf["sources"]))
        if vf.get("note"):
            print("note:     %s" % vf["note"])
    else:
        print("name:     (unnamed in name_index)")

    veh = c.runtime.get("vehicle", {})
    for fld in veh.get("fields", []):
        if int(fld["off"], 16) == off:
            extra = " ".join("%s=%s" % (k, v) for k, v in sorted(fld.items())
                             if k not in ("off",))
            print("runtime:  %s   [OBSERVED]" % extra)
            break
    for ch in veh.get("real_children", []):
        if int(ch["off"], 16) == off:
            print("runtime:  heap child %s   [OBSERVED]" % json.dumps(ch))

    moved = []
    for label, d in veh.get("behavior_deltas", {}).items():
        for mp in d.get("moved_paths", []):
            if mp["path"].startswith("+0x%04X" % off):
                moved.append("%s (%s cars)" % (label, mp["cars"]))
    if moved:
        print("moved during: %s   [OBSERVED]" % ", ".join(moved))

    readers, writers = [], []
    for addr, f in c.facts.items():
        for x in f["fields"]:
            if int(x["off_hex"], 16) != off:
                continue
            entry = (addr, f["name"], x["base"],
                     bool(c.prio_by_addr.get(addr, {}).get("named_fields")))
            if "W" in x["rw"]:
                writers.append(entry)
            if "R" in x["rw"]:
                readers.append(entry)

    def dump(lst, label):
        if not lst:
            print("\n%s: none found" % label)
            return
        # A function whose known vehicle fields are already named is far more
        # likely to have a vehicle `this`; flagged, not assumed.
        lst.sort(key=lambda e: (not e[3], e[1]))
        shown, rest = cap(lst, 15)
        print("\n%s (%d):" % (label, len(lst)))
        for a, n, base, vehlike in shown:
            print("  %s %-45s base=%-6s%s" % (a, n[:45], base,
                                              "  <- vehicle-like" if vehlike else ""))
        print(more(rest))

    dump(writers, "WRITERS")
    dump(readers, "readers")
    print("\ncaveat: `this` identity is per function. A hit only means that "
          "function touches +0x%X of ITS OWN struct." % off)

    print_findings(c.find_findings("0x%X" % off))
    return 0


def cmd_global(c, q):
    a = norm_addr(q)
    if a is None:
        ql = q.lower()
        hits = [(k, v) for k, v in c.names.get("globals", {}).items()
                if ql in (v.get("name") or "").lower()]
        if not hits:
            print("no global name matches %r" % q)
            return 1
        shown, rest = cap(hits, 20)
        for k, v in shown:
            print("  %s  %-40s [%s]" % (k, v["name"], v["trust"]))
        print(more(rest))
        return 0

    print("=== global 0x%s ===" % a)
    e = c.global_entry(a)
    if e:
        print("name:     %s   [%s]" % (e["name"], e["trust"]))
        if e.get("sources"):
            print("sources:  %s" % ", ".join(e["sources"]))
        if e.get("conflicts"):
            print("conflicts: %s   <- competing names, unresolved" % e["conflicts"])
    else:
        print("name:     (not in name_index)")

    readers, writers = [], []
    for addr, f in c.facts.items():
        for g in f["globals"]:
            if g["addr"].lower() != a:
                continue
            if "W" in g["rw"]:
                writers.append((addr, f["name"]))
            if "R" in g["rw"]:
                readers.append((addr, f["name"]))
    for lst, label in ((writers, "WRITERS"), (readers, "readers")):
        shown, rest = cap(lst, 15)
        print("\n%s (%d):" % (label, len(lst)))
        for addr, n in shown:
            print("  %s  %s" % (addr, n))
        print(more(rest))

    print_findings(c.find_findings(a))
    return 0


def cmd_token(c, q):
    ql = q.lower()
    print("=== token/string search: %r ===" % q)

    hits = [(ea, v) for ea, v in c.strings.get("strings", {}).items()
            if ql in v["s"].lower()]
    shown, rest = cap(hits, 20)
    print("\nstring literals (%d):" % len(hits))
    for ea, v in shown:
        users = []
        for x in v.get("code_xrefs", []):
            users.append(x.get("func_name") or x["func"])
        for x in v.get("data_xrefs", []):
            users += [r.get("func_name") or r["func"] for r in x.get("readers", [])]
        u = ", ".join(sorted(set(users))[:4]) or "(no xref)"
        print("  %s %-45r %s" % (ea, v["s"][:45], u))
    print(more(rest))

    print("\nconfig-schema matches:")
    n = 0
    for pname, d in c.schema.get("parsers", {}).items():
        for sec, items in d["sections"].items():
            for e in items:
                if ql in e["token"].lower():
                    print("  %-24s %-12s %-22s offsets %s  setter %s"
                          % (pname.rsplit("_", 1)[0], sec, e["token"],
                             ", ".join(e["offsets"]), e["setter"]))
                    n += 1
    if not n:
        print("  none")
    else:
        print("  (offsets are relative to that parser's object, NOT the "
              "0x48C0 race struct)")

    # What the SHIPPED files actually put in this key: the values the game runs
    # on, and the handle for proving an offset without a debugger.
    hits = []
    for ext, secs in c.data_index.get("index", {}).items():
        for sec, keys in secs.items():
            for key, rec in keys.items():
                if ql in key.lower():
                    hits.append((ext, sec, key, rec))
    if hits:
        print("\nshipped data files (%d key(s)):" % len(hits))
        shown, rest = cap(hits, 12)
        for ext, sec, key, rec in shown:
            rng = ""
            if "min" in rec:
                rng = "  range %s..%s%s" % (rec["min"], rec["max"],
                                            "  CONSTANT" if rec.get("constant") else "")
            adj = "  garage-adjustable" if rec.get("garage_adjustable") else ""
            print("  .%-4s %-14s %-24s %d files%s%s"
                  % (ext, sec, key, rec["files"], rng, adj))
            print("        samples: %s" % ", ".join(str(s)[:28] for s in rec["samples"][:4]))
            if "parsed_by" not in rec:
                print("        no extracted parser handles this key yet")
        print(more(rest))

    print_findings(c.find_findings(q))
    return 0


def cmd_findings(c, q, as_json=False):
    fs = c.find_findings(q) if q else c.findings
    if as_json:
        recorded = sum(1 for f in fs if f.get("tier") == "RECORDED")
        report = {
            "schema": "f1c.findings_query.v1",
            "query": q,
            "total": len(fs),
            "reviewed": len(fs) - recorded,
            "recorded": recorded,
            "legend": {"RECORDED": "copied from the automation checkpoint; not tier-reviewed; verify via its evidence link."},
            "findings": fs,
        }
        print(json.dumps(report))
        return 0
    if not fs:
        print("no findings match %r (findings.jsonl has %d curated, checkpoint has %d recorded)"
              % (q, c.reviewed_findings_count, c.recorded_findings_count))
        return 0
    print("=== %d finding(s) ===" % len(fs))
    if any(f.get("tier") == "RECORDED" for f in fs):
        print("RECORDED = copied from the automation checkpoint; not tier-reviewed; verify via its evidence link.")
    for f in fs:
        print("\n[%s] %s" % (f.get("tier", "?"), f.get("claim", "")))
        for k in ("evidence", "repro", "source", "supersedes", "note"):
            if f.get(k):
                print("  %-10s %s" % (k + ":", f[k]))
    return 0


def cmd_stats(c):
    print("=== corpus inventory ===")
    print("functions with facts : %d" % len(c.facts))
    print("fills                : %d" % len(os.listdir(os.path.join(ROOT, "skeletons_filled"))))
    g = c.names.get("globals", {})
    vf = c.names.get("vehicle_fields", {})
    for label, d in (("globals", g), ("vehicle_fields", vf)):
        t = collections.Counter(v.get("trust") for v in d.values())
        print("%-20s : %d  %s" % (label, len(d),
                                  "  ".join("%s=%d" % kv for kv in t.most_common())))
    print("strings              : %d (%d functions)"
          % (len(c.strings.get("strings", {})),
             len(c.strings.get("by_function", {}))))
    print("source modules       : %d" % len(c.modules))
    print("config parsers       : %d (%d fields)"
          % (len(c.schema.get("parsers", {})),
             c.schema.get("_meta", {}).get("n_fields", 0)))
    print("findings             : %d" % len(c.findings))
    veh = c.runtime.get("vehicle", {})
    print("runtime vehicle      : %d fields, %d heap children, %d behavior deltas"
          % (len(veh.get("fields", [])), len(veh.get("real_children", [])),
             len(veh.get("behavior_deltas", {}))))
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cmd = sys.argv[1].lower()
    arguments = list(sys.argv[2:])
    as_json = "--json" in arguments
    while "--json" in arguments:
        arguments.remove("--json")
    arg = " ".join(arguments).strip()
    c = Corpus()
    if cmd == "func":
        return cmd_func(c, arg)
    if cmd == "offset":
        return cmd_offset(c, arg)
    if cmd == "global":
        return cmd_global(c, arg)
    if cmd in ("token", "string"):
        return cmd_token(c, arg)
    if cmd == "findings":
        return cmd_findings(c, arg, as_json=as_json)
    if cmd == "stats":
        return cmd_stats(c)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
