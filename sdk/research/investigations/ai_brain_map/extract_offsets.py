#!/usr/bin/env python
"""B1 extractor: vehicle-relative offset accesses and calls in Hex-Rays C.

Output CSV columns: func,line,dec,hex,access,width,base,form,expr

- ``func`` is the file stem without the trailing ``_XXXXXXXX`` address
  (``AIRacingLine_00425920.c`` -> ``AIRacingLine``).
- ``base`` names the identifier (or dereference expression) before ``+``:
  ``this``, ``a2``, ``v10``, ``*(_DWORD*)v2``, ...  B5 treats only
  ``base == this`` as a vehicle field.
- ``form``:
    direct   ``*(TYPE *)(base + N)``
    indexed  ``*(TYPE *)(base + K * expr + N)`` or address-only
             ``base + K * expr + N`` for vehicle bases ``this``/``a2``
    array    ``ptr[index]`` for a pointer declaration or a pointer-typed
             ``this`` parameter (byte offset = index * element size;
             ``var + N`` indexes use the constant part ``N * size``)
    deref2   ``*(TYPE *)(*(CAST *)name + N)``
    arith    ``*((TYPE *)base + N)`` (pointer-arithmetic deref; byte offset N)
    call     callee(...) row; dec/hex/width/base empty, expr = callee
- ``access`` is ``W`` when the dereference/array element is the left side of
  ``=``, ``+=``, ``-=``, ``*=``, ``/=``, ``%=``, ``++`` or ``--``; else ``R``.
- Lines that look offset-bearing (``this +``, a tracked pointer index, or a
  double deref with ``+ N``) but produce no row are listed in
  ``unmatched_<func>.txt`` when ``--unmatched-dir`` is given.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

WORD = r"[A-Za-z_]\w*"
TYPE = WORD + r"(?:\s+" + WORD + r")*(?:\s*\*)*"
DEREF_HEAD = r"\*\(\s*(?P<type>" + TYPE + r")\s*\*\s*\)"

DEREF_D2_RE = re.compile(
    DEREF_HEAD
    + r"\s*\(\s*(?P<base>\*\([^()]*\)\s*" + WORD + r")\s*\+\s*(?P<off>\d+)\s*\)"
)
DEREF_IDX_RE = re.compile(
    DEREF_HEAD
    + r"\s*\(\s*(?P<base>" + WORD + r")\s*\+\s*(?P<scale>\d+)\s*\*\s*"
    r"(?P<idx>(?:[^()]|\([^()]*\))+?)\s*\+\s*(?P<off>\d+)\s*\)"
)
DEREF_DIRECT_RE = re.compile(
    DEREF_HEAD + r"\s*\(\s*(?P<base>" + WORD + r")\s*\+\s*(?P<off>\d+)\s*\)"
)
PTR_ARITH_RE = re.compile(
    r"\*\s*\(\s*\(\s*(?P<type>" + TYPE + r")\s*\*+\s*\)\s*(?P<base>" + WORD
    + r")\s*\+\s*(?P<off>\d+)\s*\)"
)
ADDR_IDX_RE = re.compile(
    r"\b(?P<base>this|a2)\s*\+\s*(?P<scale>\d+)\s*\*\s*"
    r"(?P<idx>(?:[^()]|\([^()]*\))+?)\s*\+\s*(?P<off>\d+)"
)

PTR_DECL_RE = re.compile(
    r"^\s*(?P<type>float|double|int|_DWORD|unsigned int|char|_BYTE|short|long|"
    r"__int8|__int16)\s*\*+\s*(?P<name>[A-Za-z_]\w*)\s*;"
)
TYPE_SIZE = {
    "float": 4, "double": 8, "int": 4, "_DWORD": 4, "unsigned int": 4,
    "char": 1, "_BYTE": 1, "short": 2, "long": 4, "__int8": 1, "__int16": 2,
}

CALL_RE = re.compile(r"(?<![.\w])(?P<name>[A-Za-z_]\w*)\s*\(")
KEYWORDS = {
    "if", "else", "while", "do", "for", "switch", "case", "default",
    "return", "break", "continue", "goto", "sizeof",
    "void", "int", "char", "short", "long", "float", "double",
    "unsigned", "signed", "const", "static", "struct", "union", "enum",
    "typedef", "extern",
}

WRITE_RE = re.compile(r"\s*(==|!=|<=|>=|\+=|-=|\*=|/=|%=|\+\+|--|=)")
WRITE_OPS = {"+=", "-=", "*=", "/=", "%=", "++", "--", "="}

UNMATCHED_THIS_RE = re.compile(r"\bthis\s*\+")
UNMATCHED_D2_RE = re.compile(r"\*\([^()]*\)\s*" + WORD + r"\s*\+")

FIELDS = ["func", "line", "dec", "hex", "access", "width", "base", "form", "expr"]


def func_name(path: Path) -> str:
    stem = path.stem
    m = re.match(r"^(.*)_[0-9a-fA-F]{8}$", stem)
    return m.group(1) if m else stem


def _norm_base(text: str) -> str:
    return re.sub(r"\s+", "", text)


def _type_size(type_text: str) -> int:
    base = type_text.replace("*", "").strip()
    return TYPE_SIZE.get(base, 4)


def _array_dec(idx: str, size: int) -> str:
    idx = idx.strip()
    if idx.isdigit():
        return str(int(idx) * size)
    m = re.fullmatch(r"(.+?)\s*([+\-])\s*(\d+)", idx)
    if m:
        n = int(m.group(3))
        return str(n * size if m.group(2) == "+" else -n * size)
    m = re.fullmatch(r"(\d+)\s*([+\-])\s*(.+)", idx)
    if m:
        n = int(m.group(1))
        return str(n * size if m.group(2) == "+" else -n * size)
    return ""


def _access_at(line: str, end: int) -> str:
    m = WRITE_RE.match(line, end)
    if m and m.group(1) in WRITE_OPS:
        return "W"
    return "R"


SIG_PTR_RE = re.compile(
    r"\b(?P<type>float|double|int|_DWORD|unsigned int|char|_BYTE|short|long|"
    r"__int8|__int16)\s*\*+\s*(?P<name>this|a1|a2|a3|a4|a5|a6)\b"
)


def _pointer_decls(lines: list[str], sig_index: int) -> dict[str, tuple[str, int]]:
    ptrs: dict[str, tuple[str, int]] = {}
    for line in lines:
        m = PTR_DECL_RE.match(line)
        if m:
            ptrs[m.group("name")] = (
                m.group("type"), TYPE_SIZE.get(m.group("type"), 4))
    if sig_index >= 0:
        for m in SIG_PTR_RE.finditer(lines[sig_index]):
            ptrs[m.group("name")] = (
                m.group("type"), TYPE_SIZE.get(m.group("type"), 4))
    return ptrs


def _signature_index(lines: list[str]) -> int:
    for i, line in enumerate(lines[:-1]):
        s = line.strip()
        if (
            line[:1] not in ("", " ", "\t")
            and s.endswith(")")
            and ";" not in s
            and lines[i + 1].strip() == "{"
        ):
            return i
    return -1


def _starts_inside(events: list[tuple[int, int, dict[str, str]]], start: int) -> bool:
    return any(ev_start <= start < ev_end for ev_start, ev_end, _ in events)


def _row(name, lineno, stripped, access, width, base, form, dec):
    dec_s = str(dec) if dec != "" else ""
    return {
        "func": name, "line": str(lineno), "dec": dec_s,
        "hex": f"0x{int(dec_s):X}" if dec_s else "",
        "access": access, "width": width, "base": base, "form": form,
        "expr": stripped,
    }


def extract_file(path: Path, unmatched_path: Path | None = None) -> list[dict[str, str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    name = func_name(path)
    sig_index = _signature_index(lines)
    ptrs = _pointer_decls(lines, sig_index)
    array_res = {
        var: re.compile(r"\b" + re.escape(var) + r"\s*\[\s*(?P<idx>[^\[\]]{1,80}?)\s*\]")
        for var in ptrs
    }
    rows: list[dict[str, str]] = []
    unmatched: list[str] = []

    for lineno, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or stripped.startswith("//"):
            continue
        events: list[tuple[int, int, dict[str, str]]] = []

        for m in DEREF_D2_RE.finditer(line):
            events.append((m.start(), m.end(), _row(
                name, lineno, stripped, _access_at(line, m.end()), m.group("type"),
                _norm_base(m.group("base")), "deref2", m.group("off"))))
        for m in DEREF_IDX_RE.finditer(line):
            events.append((m.start(), m.end(), _row(
                name, lineno, stripped, _access_at(line, m.end()), m.group("type"),
                m.group("base"), "indexed", m.group("off"))))
        for m in DEREF_DIRECT_RE.finditer(line):
            events.append((m.start(), m.end(), _row(
                name, lineno, stripped, _access_at(line, m.end()), m.group("type"),
                m.group("base"), "direct", m.group("off"))))
        for m in PTR_ARITH_RE.finditer(line):
            if _starts_inside(events, m.start()):
                continue
            events.append((m.start(), m.end(), _row(
                name, lineno, stripped, _access_at(line, m.end()), m.group("type"),
                m.group("base"), "arith", int(m.group("off")) * _type_size(m.group("type")))))
        for var, (elem_type, size) in ptrs.items():
            for m in array_res[var].finditer(line):
                if _starts_inside(events, m.start()):
                    continue
                dec = _array_dec(m.group("idx"), size)
                events.append((m.start(), m.end(), _row(
                    name, lineno, stripped, _access_at(line, m.end()),
                    elem_type, var, "array", dec)))
        for m in ADDR_IDX_RE.finditer(line):
            if _starts_inside(events, m.start()):
                continue
            events.append((m.start(), m.end(), _row(
                name, lineno, stripped, _access_at(line, m.end()), "",
                m.group("base"), "indexed", m.group("off"))))

        idx0 = lineno - 1
        if idx0 != sig_index:
            for m in CALL_RE.finditer(line):
                callee = m.group("name")
                if callee in KEYWORDS or callee == name:
                    continue
                call_row = _row(
                    name, lineno, stripped, "CALL", "", "", "call", "")
                call_row["expr"] = callee
                events.append((m.start(), m.end(), call_row))

        events.sort(key=lambda item: (item[0], item[1]))
        rows.extend(row for _, _, row in events)

        has_form = any(row["form"] != "call" for _, _, row in events)
        looks_offset = (
            UNMATCHED_THIS_RE.search(line)
            or UNMATCHED_D2_RE.search(line)
            or any(array_res[var].search(line) for var in ptrs)
        )
        if looks_offset and not has_form:
            unmatched.append(f"L{lineno}: {stripped}")

    if unmatched_path is not None:
        text = "# %s: offset-like lines with no extracted row (%d)\n" % (name, len(unmatched))
        if unmatched:
            text += "\n".join(unmatched) + "\n"
        unmatched_path.write_text(text, encoding="utf-8")

    return rows


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--unmatched-dir", type=Path, default=None)
    args = parser.parse_args(argv)
    writer = csv.DictWriter(sys.stdout, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader()
    for src in args.files:
        unmatched = None
        if args.unmatched_dir is not None:
            unmatched = args.unmatched_dir / ("unmatched_%s.txt" % func_name(src))
        for row in extract_file(src, unmatched):
            writer.writerow(row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
