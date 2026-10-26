#!/usr/bin/env python3
"""F1C Mod Investigation Pipeline.

Turns "I want to understand and change behavior X" into a repeatable flow:

  SEED  -> a behavior spec (anchor globals / seed funcs / strings)   [seeds/*.json]
  MAP   -> static dossier: who touches the anchors, player vs AI path [map subcommand]
  WATCH -> Frida agent that logs the anchors live during a race       [watch subcommand]
  PATCH -> patch record + Frida live-override + rollback              [patch subcommand]

Data sources (all pre-existing in the repo):
  tools/ida_data_xrefs.csv        func->global->read/write (17k edges)
  tools/cross_binary_final.csv    confirmed NASCAR/ISI identity
  knowledge_graph/callers.json    inverse call graph
  analysis_pipeline/analyzed/*.c  raw decompiled source (ground truth)
  docs/12-globals-reference.md    global labels (GUESS tier)

Usage:
  python mod_pipeline.py map    seeds/ai_drying.json
  python mod_pipeline.py watch  seeds/ai_drying.json
  python mod_pipeline.py patch  --addr 0x0059e450 --type float --value 0.06 --name wetness_scale
"""
import argparse
import csv
import glob
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TOOLS = os.path.join(ROOT, "tools")
# IDA corpus: raw Hex-Rays .c live in functions/<Name>_<8hex>.c (not ghidra's analyzed/)
ANALYZED = os.path.join(ROOT, "functions")
KG = os.path.join(ROOT, "knowledge_graph")
EXE = os.path.join(ROOT, "..", "F1 Challenge V10", "F1 Challenge Online.exe")
IMAGE_BASE = 0x00400000

sys.path.insert(0, HERE)
import subsystems  # noqa: E402


# ---------------------------------------------------------------- data loaders
def norm_addr(a):
    """Normalize any hex address string/int to '0x' + 8 lowercase hex."""
    if isinstance(a, int):
        v = a
    else:
        v = int(a, 16)
    return f"0x{v:08x}"


def load_data_xrefs():
    """global_addr -> list of {func, access}."""
    path = os.path.join(TOOLS, "ida_data_xrefs.csv")
    by_global = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            g = norm_addr(row["global_addr"])
            by_global.setdefault(g, []).append(
                {"func": norm_addr(row["func_addr"]),
                 "name": row["global_name"],
                 "access": row["access"]}
            )
    return by_global


def load_cross_binary():
    """f1c_addr -> row dict."""
    path = os.path.join(TOOLS, "cross_binary_final.csv")
    out = {}
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            out[norm_addr(row["f1c_addr"])] = row
    return out


def load_callers():
    path = os.path.join(KG, "callers.json")
    if not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8", errors="replace") as f:
        data = json.load(f)
    # normalize keys
    out = {}
    for k, v in data.items():
        key = k if k.startswith("0x") else norm_addr(int(k, 16)) if re.match(r"[0-9a-fA-F]+$", k) else k
        out[key] = v
    return out


def raw_c_path(func_addr):
    v = int(func_addr, 16)
    matches = glob.glob(os.path.join(ANALYZED, f"*_{v:08x}.c"))
    return matches[0] if matches else None


def extract_source_lines(func_addr, hex_tokens, context=0):
    """Return source lines in func that mention any of hex_tokens (e.g. '008afee8')."""
    path = raw_c_path(func_addr)
    if not path:
        return None, []
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    hits = []
    toks = [t.lower() for t in hex_tokens]
    for i, line in enumerate(lines):
        low = line.lower()
        if any(t in low for t in toks):
            hits.append((i + 1, line.rstrip()))
    return path, hits


# ------------------------------------------------------------------ MAP stage
def cmd_map(args):
    spec = json.load(open(args.seed, encoding="utf-8"))
    name = spec["name"]
    globals_spec = spec.get("globals", [])          # [{addr,label}]
    seed_funcs = [norm_addr(a) for a in spec.get("seeds", [])]
    player_ranges = spec.get("player_ranges", [])   # optional explicit [[start,end],...]
    ai_ranges = spec.get("ai_ranges", [])

    outdir = os.path.join(HERE, "investigations", name)
    os.makedirs(outdir, exist_ok=True)

    xrefs = load_data_xrefs()
    xbin = load_cross_binary()
    callers = load_callers()

    # Collect anchor globals + their hex tokens for source grep
    anchor_addrs = [norm_addr(g["addr"]) for g in globals_spec]
    # IDA Hex-Rays names globals with minimal-width hex (flt_8AFEE8, dword_59E450),
    # not Ghidra's zero-padded DAT_008afee8 — strip leading zeros so the source
    # grep in extract_source_lines() matches. (case handled downstream via .lower())
    hex_tokens = [a[2:].lstrip("0") or "0" for a in anchor_addrs]

    # Gather consumers
    consumers = {}  # func_addr -> {globals_touched:[(global,access)], ...}
    for g in globals_spec:
        gaddr = norm_addr(g["addr"])
        for ref in xrefs.get(gaddr, []):
            fa = ref["func"]
            rec = consumers.setdefault(fa, {"touched": [], "src_hits": []})
            rec["touched"].append((gaddr, g.get("label", ref["name"]), ref["access"]))

    # Also include explicit seed functions even if no xref
    for fa in seed_funcs:
        consumers.setdefault(fa, {"touched": [], "src_hits": []})

    # Enrich each consumer
    rows = []
    for fa, rec in consumers.items():
        va = int(fa, 16)
        xrow = xbin.get(fa)
        cls = subsystems.classify(va, xrow)

        # explicit range override
        path_class = cls["path_class"]
        for s, e in player_ranges:
            if s <= va < e:
                path_class = "player_physics"
        for s, e in ai_ranges:
            if s <= va < e:
                path_class = "ai"

        # source lines touching anchors
        srcpath, hits = extract_source_lines(fa, hex_tokens)
        rec["src_hits"] = hits

        caller_list = callers.get(fa, [])
        writes = [t for t in rec["touched"] if t[2] == "write"]
        reads = [t for t in rec["touched"] if t[2] == "read"]

        rows.append({
            "func": fa,
            "size_hit_lines": len(hits),
            "subsystem": cls["subsystem"],
            "path_class": path_class,
            "identity": cls["identity"],
            "id_source": cls["source"],
            "nascar03": xrow.get("nascar03_addr", "") if xrow else "",
            "reads": ";".join(f"{t[1]}" for t in reads),
            "writes": ";".join(f"{t[1]}" for t in writes),
            "n_callers": len(caller_list),
            "has_source": bool(srcpath),
        })

    rows.sort(key=lambda r: (r["path_class"], r["func"]))

    # Write consumers.csv
    csv_path = os.path.join(outdir, "consumers.csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else
                           ["func", "path_class"])
        w.writeheader()
        w.writerows(rows)

    # Write dossier.md
    _write_dossier(outdir, spec, rows, consumers, hex_tokens)
    print(f"[map] {name}: {len(rows)} consumers -> {outdir}")
    print(f"      dossier.md + consumers.csv")

    # path-class breakdown
    from collections import Counter
    c = Counter(r["path_class"] for r in rows)
    for k, v in sorted(c.items(), key=lambda x: -x[1]):
        print(f"      {k}: {v}")


def _write_dossier(outdir, spec, rows, consumers, hex_tokens):
    name = spec["name"]
    L = []
    L.append(f"# Mod Investigation: {name}")
    L.append("")
    L.append(spec.get("description", ""))
    L.append("")
    L.append("## Trust tiers")
    L.append("- **FACT** — raw decompiled `.c` source lines (shown inline)")
    L.append("- **GUESS** — docs/12 global labels, docs/13 offsets (DS-worker inferred)")
    L.append("- **LEAD** — docs/31 call chains, semantic names (may be hallucinated)")
    L.append("- **CONFIRMED** — ISI class from NASCAR cross-binary match")
    L.append("")

    L.append("## Anchor globals")
    L.append("")
    L.append("| Address | Label | Consumers |")
    L.append("|---------|-------|-----------|")
    xrefs = load_data_xrefs()
    for g in spec.get("globals", []):
        ga = norm_addr(g["addr"])
        n = len(xrefs.get(ga, []))
        L.append(f"| {ga} | {g.get('label','')} | {n} |")
    L.append("")

    # Split by path class
    from collections import defaultdict
    by_path = defaultdict(list)
    for r in rows:
        by_path[r["path_class"]].append(r)

    order = ["ai", "player_physics", "shared_physics", "game", "render",
             "audio", "net", "ui", "engine_lib", "unknown"]
    L.append("## Consumers by path")
    L.append("")
    for pc in order:
        if pc not in by_path:
            continue
        L.append(f"### {pc} ({len(by_path[pc])})")
        L.append("")
        L.append("| Func | Identity | Src | R/W | Callers |")
        L.append("|------|----------|-----|-----|---------|")
        for r in sorted(by_path[pc], key=lambda x: x["func"]):
            ident = r["identity"] or "—"
            if r["id_source"] == "confirmed_class":
                ident = f"**{ident}** (CONFIRMED)"
            rw = []
            if r["reads"]:
                rw.append(f"R:{r['reads']}")
            if r["writes"]:
                rw.append(f"W:{r['writes']}")
            L.append(f"| {r['func']} | {ident} | {'yes' if r['has_source'] else 'no'} "
                     f"| {' '.join(rw)} | {r['n_callers']} |")
        L.append("")

    # Inline FACT source for the highest-value path classes
    L.append("## Source evidence (FACT)")
    L.append("")
    focus = ["ai", "player_physics", "shared_physics"]
    for pc in focus:
        for r in sorted(by_path.get(pc, []), key=lambda x: x["func"]):
            hits = consumers.get(r["func"], {}).get("src_hits", [])
            if not hits:
                continue
            ident = r["identity"] or r["func"]
            L.append(f"#### {r['func']} — {ident} [{pc}]")
            L.append("```c")
            for ln, txt in hits:
                L.append(f"{ln}: {txt}")
            L.append("```")
            L.append("")

    open(os.path.join(outdir, "dossier.md"), "w", encoding="utf-8").write("\n".join(L))


# ----------------------------------------------------------------- WATCH stage
FRIDA_WATCH_TMPL = """// Auto-generated Frida watch agent for investigation: {name}
// Poll {hz}Hz. Emits CSV rows: time,{headers}
// Run: frida -p <pid> -l {name}_watch.js   (or via tools/trace runner)
'use strict';
const IMAGE_BASE = ptr('0x400000');
const GLOBALS = {globals_js};
const STRUCTS = {structs_js};
const HZ = {hz};

function readVal(addr, type) {{
  const p = ptr(addr);
  try {{
    if (type === 'float')  return p.readFloat().toFixed(5);
    if (type === 'double') return p.readDouble().toFixed(5);
    if (type === 'int')    return p.readS32();
    if (type === 'byte')   return p.readU8();
    if (type === 'ptr')    return p.readPointer();
  }} catch (e) {{ return 'ERR'; }}
  return '?';
}}

const headers = GLOBALS.map(g => g.label).concat(
  STRUCTS.map(s => s.label));
send('CSV_HEADER:time,' + headers.join(','));

let t0 = Date.now();
setInterval(function () {{
  const t = ((Date.now() - t0) / 1000).toFixed(2);
  let vals = GLOBALS.map(g => readVal(g.addr, g.type));
  for (const s of STRUCTS) {{
    try {{
      const base = ptr(s.base_global).readPointer();
      const obj  = base.add(s.index * s.stride);
      vals.push(readVal(obj.add(s.offset), s.type));
    }} catch (e) {{ vals.push('ERR'); }}
  }}
  send('CSV:' + t + ',' + vals.join(','));
}}, 1000 / HZ);
"""


def cmd_watch(args):
    spec = json.load(open(args.seed, encoding="utf-8"))
    name = spec["name"]
    outdir = os.path.join(HERE, "investigations", name)
    os.makedirs(outdir, exist_ok=True)

    globals_js = json.dumps(
        [{"addr": norm_addr(g["addr"]), "label": g.get("label", norm_addr(g["addr"])),
          "type": g.get("type", "float")} for g in spec.get("globals", [])],
        indent=2)
    structs_js = json.dumps(spec.get("struct_watches", []), indent=2)
    headers = ",".join(g.get("label", "g") for g in spec.get("globals", []))

    js = FRIDA_WATCH_TMPL.format(
        name=name, hz=spec.get("watch_hz", 10),
        globals_js=globals_js, structs_js=structs_js, headers=headers)
    js_path = os.path.join(outdir, f"{name}_watch.js")
    open(js_path, "w", encoding="utf-8").write(js)
    print(f"[watch] wrote {js_path}")
    print(f"        globals: {len(spec.get('globals', []))}, "
          f"struct watches: {len(spec.get('struct_watches', []))}")
    print(f"        run against a live race, capture send('CSV:...') to a file")


# ----------------------------------------------------------------- PATCH stage
class PEMapper:
    """Minimal PE section mapper: VA -> file offset, read original bytes."""
    def __init__(self, exe_path):
        self.data = open(exe_path, "rb").read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.data, pe + 6)[0]
        opt = struct.unpack_from("<H", self.data, pe + 20)[0]
        sstart = pe + 24 + opt
        self.sections = []
        for i in range(nsec):
            off = sstart + i * 40
            vaddr = struct.unpack_from("<I", self.data, off + 12)[0]
            vsize = struct.unpack_from("<I", self.data, off + 8)[0]
            praw = struct.unpack_from("<I", self.data, off + 20)[0]
            self.sections.append((vaddr, vsize, praw))

    def va_to_file(self, va):
        rva = va - IMAGE_BASE
        for vaddr, vsize, praw in self.sections:
            if vaddr <= rva < vaddr + vsize:
                return praw + (rva - vaddr)
        return None

    def read(self, va, n):
        fo = self.va_to_file(va)
        if fo is None:
            return None
        return self.data[fo:fo + n]


def cmd_patch(args):
    addr = int(args.addr, 16)
    mapper = PEMapper(EXE) if os.path.exists(EXE) else None

    # Build new bytes
    if args.type == "float":
        new_bytes = struct.pack("<f", float(args.value))
        size = 4
    elif args.type == "double":
        new_bytes = struct.pack("<d", float(args.value))
        size = 8
    elif args.type == "int":
        new_bytes = struct.pack("<i", int(args.value))
        size = 4
    elif args.type == "bytes":
        new_bytes = bytes.fromhex(args.value.replace(" ", ""))
        size = len(new_bytes)
    else:
        print(f"unknown type {args.type}")
        return

    orig = mapper.read(addr, size) if mapper else None
    orig_hex = orig.hex() if orig else "UNKNOWN (exe not found)"
    orig_val = ""
    if orig and args.type == "float":
        orig_val = f" ({struct.unpack('<f', orig)[0]})"
    elif orig and args.type == "double":
        orig_val = f" ({struct.unpack('<d', orig)[0]})"
    elif orig and args.type == "int":
        orig_val = f" ({struct.unpack('<i', orig)[0]})"

    pname = args.name or f"patch_{addr:08x}"
    outdir = os.path.join(HERE, "patches")
    os.makedirs(outdir, exist_ok=True)

    record = {
        "name": pname,
        "va": norm_addr(addr),
        "file_offset": f"0x{mapper.va_to_file(addr):x}" if mapper and mapper.va_to_file(addr) else None,
        "type": args.type,
        "size": size,
        "original_bytes": orig_hex,
        "new_bytes": new_bytes.hex(),
        "new_value": args.value,
    }
    json.dump(record, open(os.path.join(outdir, f"{pname}.json"), "w"), indent=2)

    # Frida live-override
    frida = f"""// Live override: {pname}  ({norm_addr(addr)})  {args.type}={args.value}
'use strict';
const P = ptr('{norm_addr(addr)}');
const ORIG = '{orig_hex}';
Memory.protect(P, {size}, 'rwx');
"""
    if args.type == "float":
        frida += f"P.writeFloat({float(args.value)});\n"
    elif args.type == "double":
        frida += f"P.writeDouble({float(args.value)});\n"
    elif args.type == "int":
        frida += f"P.writeS32({int(args.value)});\n"
    else:
        frida += f"P.writeByteArray([{','.join('0x'+new_bytes.hex()[i:i+2] for i in range(0,len(new_bytes.hex()),2))}]);\n"
    frida += f"console.log('[patch] {pname} applied; original was ' + ORIG);\n"
    open(os.path.join(outdir, f"{pname}.js"), "w", encoding="utf-8").write(frida)

    print(f"[patch] {pname}")
    print(f"        VA {norm_addr(addr)}  file_offset {record['file_offset']}")
    print(f"        original: {orig_hex}{orig_val}")
    print(f"        new:      {new_bytes.hex()} ({args.value})")
    print(f"        -> patches/{pname}.json + {pname}.js")
    print(f"        (static exe patch: write new_bytes at file_offset; keep original for rollback)")


# ------------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description="F1C mod investigation pipeline")
    sub = ap.add_subparsers(dest="cmd", required=True)

    m = sub.add_parser("map", help="static dossier from a seed spec")
    m.add_argument("seed")
    m.set_defaults(func=cmd_map)

    w = sub.add_parser("watch", help="generate Frida watch agent")
    w.add_argument("seed")
    w.set_defaults(func=cmd_watch)

    p = sub.add_parser("patch", help="generate patch record + Frida override")
    p.add_argument("--addr", required=True)
    p.add_argument("--type", required=True, choices=["float", "double", "int", "bytes"])
    p.add_argument("--value", required=True)
    p.add_argument("--name")
    p.set_defaults(func=cmd_patch)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
