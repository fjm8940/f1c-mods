"""extract_config_schema.py — recover the game's config-file schemas from its own parsers.

THE POINT: F1 Challenge is half data-driven (.hdv .aiw .rcd .tbc .gdb .plr .ini).
The parsers for those files live in the exe, and they are written as one call per
field:

    sub_565AA0(aMass,     this + 64, 0, 0, 0);          // "Mass"     -> +0x40
    sub_565AA0(aInertia,  this + 72, this + 80, this + 88, 0);  // vec3 -> +0x48..

So the token string and the struct offset it lands in sit in the same call. That
makes field naming a text join, not a guess: the game is naming its own fields,
and the shipped .hdv files are the second, independent source. Code + shipped data
= CONFIRMED tier, no LLM in the loop.

Inputs : functions/*.c  (Hex-Rays output, ground truth text)
         strings_index.json  (IDA symbol `aMass` -> literal "Mass")
Outputs: config_schema.json  — {parser_func: {section: [{token, offsets, setter}]}}
         CONFIG_SCHEMA.md    — readable table per parser

SCOPE / HONESTY: the offsets recovered are into whatever object that parser's
`this` points at — for the .hdv parser that is the vehicle DATA object, which is
NOT proven to be the 0x48C0 race-vehicle struct at 0x5EC6D8. Linking the two is a
separate, separately evidenced step. Nothing here is written to name_index.json
automatically.

Usage: python tools/extract_config_schema.py
"""
import io
import json
import os
import re
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUNCS = os.path.join(ROOT, "functions")
IDX = os.path.join(ROOT, "strings_index.json")

# A call whose FIRST argument is a string symbol: sub_565AA0(aMass, this + 64, ...)
CALL_RE = re.compile(r"\b(\w+)\s*\(\s*(a[A-Za-z0-9_]+)\s*([,)])")
# Destination offsets in the remaining arguments: this + 64 / a1 + 5192.
# Only `this`/`a1` count — a local temp is not a struct field.
DEST_RE = re.compile(r"\b(?:this|a1)\s*\+\s*(\d+)\b")

# The second parser dialect, e.g. the .rcd driver-ability reader sub_424080:
#     v18 = StringKeyedLinkedListSearch(v2, (int)aPassing);
#     if ( v18 ) { ...; *((float *)this + 4403) = sub_58850F(v17); }
# The token lookup and the store are separate statements, and the destination is
# a TYPED INDEX, not a byte offset - so `+ 4403` on a float pointer means byte
# 4403*4 = 0x44CC. Missing this dialect hid every .rcd/.aiw ability field.
IDX_RE = re.compile(r"\*\(\(\s*(_BYTE|_WORD|_DWORD|float|double|int|char|"
                    r"unsigned __int8|__int16)\s*\*\)\s*(?:this|a1)\s*\+\s*(\d+)\s*\)")
ELEM_SIZE = {"_BYTE": 1, "char": 1, "unsigned __int8": 1, "_WORD": 2, "__int16": 2,
             "_DWORD": 4, "float": 4, "int": 4, "double": 8}
# How far after a token lookup to look for its store, in characters. The pattern
# is always lookup -> if(found) -> store, so this stays inside one if-block.
LOOKAHEAD = 320
SECTION_RE = re.compile(r"^\[[A-Za-z0-9_ ]+\]$")


def load_syms():
    with io.open(IDX, encoding="utf-8") as f:
        d = json.load(f)
    return {v["sym"]: v["s"] for v in d["strings"].values() if v.get("sym")}


def balanced_call(text, start):
    """Return the argument text of the call whose '(' is at/after start."""
    i = text.index("(", start)
    depth = 0
    for j in range(i, len(text)):
        c = text[j]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return text[i + 1:j]
    return ""


def open_paren_before(text, pos, limit=600):
    """Index of the '(' opening the call that encloses pos, or -1."""
    depth = 0
    i = pos - 1
    stop = max(-1, pos - limit)
    while i > stop:
        ch = text[i]
        if ch == ")":
            depth += 1
        elif ch == "(":
            if depth == 0:
                return i
            depth -= 1
        elif ch == ";":
            return -1
        i -= 1
    return -1


def enclosing_call(text, pos):
    """Name of the function whose argument list contains pos, or None."""
    i = open_paren_before(text, pos)
    if i < 0:
        return None
    j = i - 1
    while j >= 0 and text[j].isspace():
        j -= 1
    k = j
    while k >= 0 and (text[k].isalnum() or text[k] == "_"):
        k -= 1
    name = text[k + 1:j + 1]
    return name or None


def call_args(text, pos):
    """Argument text of the call enclosing pos."""
    i = open_paren_before(text, pos)
    if i < 0:
        return ""
    return balanced_call(text, i)


def scan_function(path, syms):
    with io.open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()

    # Two kinds of event, ordered by position in the decompiled text. Hex-Rays
    # preserves the source order of the parser's if/else-if chain, so the section
    # a field belongs to is simply the last [SECTION] literal seen above it.
    # The section test is done on ANY occurrence of the symbol, not just a first
    # argument: the compare reads `sub_587C20(v72, aGeneral)`, token second.
    events = []
    for m in re.finditer(r"\b(a[A-Za-z0-9_]+)\b", text):
        literal = syms.get(m.group(1))
        if literal and SECTION_RE.match(literal.strip()):
            events.append((m.start(), "sec", literal.strip()))

    sym_hits = [m for m in re.finditer(r"\b(a[A-Za-z0-9_]+)\b", text)
                if syms.get(m.group(1))]
    for hi, m in enumerate(sym_hits):
        sym = m.group(1)
        literal = syms.get(sym)
        if literal is None or SECTION_RE.match(literal.strip()):
            continue
        # The token is not always the first argument - the .rcd reader writes
        # StringKeyedLinkedListSearch(v2, (int)aPassing) - so find the enclosing
        # call by walking back to its unmatched '(' and taking the name before it.
        setter = enclosing_call(text, m.start())
        if setter is None:
            continue
        offsets = [int(x) for x in DEST_RE.findall(call_args(text, m.start()))]
        assoc = "inline"       # token and destination in the same call
        if not offsets:
            # Dialect 2: the store follows the lookup. Association is by
            # proximity, so it is recorded as a weaker kind of evidence.
            # The window MUST stop at the next token lookup: StartsDry/StartsWet
            # store into locals, and an unbounded window silently attributed the
            # NEXT token's store to them - a wrong offset that looked authoritative.
            end = m.end() + LOOKAHEAD
            if hi + 1 < len(sym_hits):
                end = min(end, sym_hits[hi + 1].start())
            window = text[m.end():end]
            # Either dialect can appear in the body - `*((float *)this + 4399)`
            # or a raw `this + 17632` (strcpy destinations). Take whichever comes
            # FIRST, else a string field is attributed to a later float store.
            typed = IDX_RE.search(window)
            plain = DEST_RE.search(window)
            if typed and (not plain or typed.start() <= plain.start()):
                offsets = [int(typed.group(2)) * ELEM_SIZE.get(typed.group(1), 4)]
                assoc = "lookahead"
            elif plain:
                offsets = [int(plain.group(1))]
                assoc = "lookahead"
        if not offsets:
            continue
        events.append((m.start(), "field", {
            "token": literal,
            "sym": sym,
            "setter": setter,
            "assoc": assoc,
            "offsets": ["0x%X" % o for o in offsets],
        }))

    entries = []
    section = None
    sections_seen = set()
    for _, kind, payload in sorted(events, key=lambda e: e[0]):
        if kind == "sec":
            section = payload
            sections_seen.add(payload)
        else:
            payload["section"] = section
            entries.append(payload)

    return entries, len(sections_seen)


def main():
    syms = load_syms()
    schema = {}
    total = 0

    for fn in sorted(os.listdir(FUNCS)):
        if not fn.endswith(".c"):
            continue
        entries, n_sections = scan_function(os.path.join(FUNCS, fn), syms)
        if not entries:
            continue
        # A real config parser assigns many tokens; one stray match is noise.
        if len(entries) < 3:
            continue
        name = fn[:-2]
        # Proximity association can attribute one store to two tokens. A shared
        # offset is not automatically wrong (a string field and its default
        # legitimately share one), but it IS the failure signature, so every
        # colliding lookahead row is flagged rather than silently shipped.
        off_count = collections.Counter(tuple(e["offsets"]) for e in entries)
        for e in entries:
            if off_count[tuple(e["offsets"])] > 1 and e["assoc"] == "lookahead":
                e["suspect"] = "duplicate_offset"

        by_section = collections.OrderedDict()
        for e in entries:
            by_section.setdefault(e["section"] or "(no section)", []).append(e)
        schema[name] = {"n_sections": n_sections, "n_fields": len(entries),
                        "n_suspect": sum(1 for e in entries if e.get("suspect")),
                        "sections": by_section}
        total += len(entries)

    out = {
        "_meta": {
            "tier": "extraction",
            "n_parsers": len(schema),
            "n_fields": total,
            "n_suspect": sum(d["n_suspect"] for d in schema.values()),
            "assoc_note": ("inline = token and destination offset in the SAME call "
                           "(strong). lookahead = the store follows the token lookup "
                           "and was matched by proximity (weaker); rows whose offset "
                           "collides with another in the same parser are flagged "
                           "suspect and must be read in the .c before use."),
            "note": ("offsets are relative to that parser's own `this`; linking a "
                     "parser object to the 0x48C0 race-vehicle struct is a separate, "
                     "separately evidenced step"),
        },
        "parsers": schema,
    }
    with io.open(os.path.join(ROOT, "config_schema.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)

    lines = ["# CONFIG_SCHEMA — config-file field names recovered from the parsers",
             "",
             "Generated by `tools/extract_config_schema.py`. **Extraction tier**: each "
             "row is a literal token string and the `this + N` offset it is written to, "
             "read out of the same call in `functions/*.c`.",
             "",
             "Offsets are relative to **that parser's own object**. They are NOT proven "
             "to be offsets into the 0x48C0 race-vehicle struct; that link is a separate "
             "step and is not assumed anywhere here.",
             "",
             "| parser | sections | fields | suspect |", "|---|---|---|---|"]
    for name, d in sorted(schema.items(), key=lambda kv: -kv[1]["n_fields"]):
        lines.append("| `%s` | %d | %d | %d |" % (name, len(d["sections"]),
                                                     d["n_fields"], d["n_suspect"]))
    lines.append("")
    for name, d in sorted(schema.items(), key=lambda kv: -kv[1]["n_fields"]):
        lines.append("## %s" % name)
        lines.append("")
        for sec, items in d["sections"].items():
            lines.append("### %s" % sec)
            lines.append("")
            lines.append("| token | offsets | setter | assoc |")
            lines.append("|---|---|---|---|")
            for e in items:
                lines.append("| `%s` | %s | `%s` | %s%s |" %
                             (e["token"], ", ".join(e["offsets"]), e["setter"],
                              e["assoc"],
                              " SUSPECT" if e.get("suspect") else ""))
            lines.append("")
    with io.open(os.path.join(ROOT, "CONFIG_SCHEMA.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print("[schema] %d parsers, %d named fields -> config_schema.json / CONFIG_SCHEMA.md"
          % (len(schema), total))


if __name__ == "__main__":
    main()
