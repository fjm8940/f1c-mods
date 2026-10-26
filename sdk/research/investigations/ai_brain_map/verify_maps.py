#!/usr/bin/env python
"""B4 verifier for `map_*.md`.

1. In every `## Logic` line that carries `[Lnn]` citations, each `0x…` token's
   decimal value must appear as a standalone number in at least one cited
   source line of the function's `.c` file.  A hex token on a line without a
   citation is a mismatch.
2. Every `0x…` token in `## Inputs` / `## Outputs` must exist (decimal) in the
   matching `offsets_<Name>.csv`.

Exit code 0 only when there are no mismatches.  The checker must not be
weakened to pass.
"""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
FUNCTIONS = REPO / "functions"

HEX_RE = re.compile(r"0x[0-9A-Fa-f]+")
REF_RE = re.compile(r"\[((?:L\d+)(?:\s*,\s*L\d+)*)\]")
ROW_RE = re.compile(r"^\|\s*(0x[0-9A-Fa-f]+)\s*\|")


def source_index() -> dict[str, Path]:
    index: dict[str, Path] = {}
    for path in FUNCTIONS.glob("*.c"):
        stem = path.stem
        m = re.match(r"^(.*)_[0-9a-fA-F]{8}$", stem)
        index[m.group(1) if m else stem] = path
    return index


def sections(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for part in re.split(r"^## ", text, flags=re.M)[1:]:
        title, _, body = part.partition("\n")
        out[title.strip()] = body
    return out


def map_name(path: Path) -> str:
    name = path.stem
    assert name.startswith("map_"), path
    return re.sub(r"_part\d+$", "", name[4:])


def has_number(line: str, dec: int) -> bool:
    return re.search(r"(?<!\d)%d(?!\d)" % dec, line) is not None


def check_map(path: Path, sources: dict[str, Path], mismatches: list[str]) -> tuple[int, int, int]:
    name = map_name(path)
    src = sources.get(name)
    if src is None:
        mismatches.append(f"{path.name} | - | function | no functions/*.c for {name}")
        return 0, 0, 0
    src_lines = src.read_text(encoding="utf-8").splitlines()
    secs = sections(path.read_text(encoding="utf-8"))

    logic_checked = 0
    hex_checked = 0
    logic = secs.get("Logic", "")
    for offset, line in enumerate(logic.splitlines(), start=1):
        hexes = HEX_RE.findall(line)
        if not hexes:
            continue
        logic_checked += 1
        refs = []
        for ref in REF_RE.findall(line):
            refs.extend(int(n) for n in re.findall(r"L(\d+)", ref))
        if not refs:
            mismatches.append(f"{path.name} | logic L{offset} | {hexes[0]} | no [Lnn] citation")
            continue
        for hexv in hexes:
            hex_checked += 1
            dec = int(hexv, 16)
            hit = None
            for ref in refs:
                if 1 <= ref <= len(src_lines) and has_number(src_lines[ref - 1], dec):
                    hit = ref
                    break
            if hit is None:
                cited = ", ".join("L%d" % r for r in refs)
                mismatches.append(
                    f"{path.name} | logic L{offset} | {hexv} ({dec}) | cited {cited}: "
                    f"{src_lines[refs[0]-1].strip() if 1 <= refs[0] <= len(src_lines) else '?'}")

    csv_path = HERE / ("offsets_%s.csv" % name)
    if not csv_path.is_file():
        mismatches.append(f"{path.name} | - | table | missing {csv_path.name}")
        return logic_checked, hex_checked, 0
    with open(csv_path, encoding="utf-8", newline="") as fh:
        decs = {row["dec"] for row in csv.DictReader(fh) if row["dec"]}
    table_checked = 0
    for sec_name in ("Inputs", "Outputs"):
        for hexv in HEX_RE.findall(secs.get(sec_name, "")):
            table_checked += 1
            if str(int(hexv, 16)) not in decs:
                mismatches.append(
                    f"{path.name} | {sec_name} | {hexv} | missing in {csv_path.name}")
    return logic_checked, hex_checked, table_checked


def main() -> int:
    sources = source_index()
    maps = sorted(HERE.glob("map_*.md"))
    mismatches: list[str] = []
    total_logic = total_hex = total_tables = 0
    for path in maps:
        lc, hc, tc = check_map(path, sources, mismatches)
        total_logic += lc
        total_hex += hc
        total_tables += tc
    print("maps checked:      %d" % len(maps))
    print("logic lines w/hex: %d" % total_logic)
    print("hex tokens checked: %d" % total_hex)
    print("table offsets checked: %d" % total_tables)
    if mismatches:
        print("mismatches: %d" % len(mismatches))
        for m in mismatches:
            print("  " + m)
        return 1
    print("mismatches: 0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
