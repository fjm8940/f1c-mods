"""B1 acceptance test for extract_offsets.py (plain asserts, run with python)."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from extract_offsets import extract_file  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
FIXTURE = REPO / "functions" / "sub_426100_00426100.c"
RACING_LINE = REPO / "functions" / "AIRacingLine_00425920.c"

rows = extract_file(FIXTURE)


def has(dec: int, access: str) -> bool:
    return any(
        row["dec"] == str(dec) and row["access"] == access for row in rows
    )


def find(dec: int) -> list[dict[str, str]]:
    return [row for row in rows if row["dec"] == str(dec)]


assert rows, "no rows extracted from fixture"
assert has(17452, "W"), "missing W for dec 17452"
assert has(17452, "R"), "missing R for dec 17452"
assert has(17416, "R"), "missing R for dec 17416"
assert has(17588, "R"), "missing R for dec 17588"
assert has(17560, "R"), "missing R for dec 17560"
assert has(17568, "R"), "missing R for dec 17568"
assert has(17572, "W"), "missing W for dec 17572"

for row in find(17452):
    assert row["hex"] == "0x442C", f"bad hex for 17452: {row['hex']}"

assert any(row["base"] == "this" for row in find(17452)), "missing base=this row"

non_this = [row for row in rows if row["base"] != "this" and row["form"] == "deref2"]
assert non_this, "missing deref2 non-this base row"
assert any("v2" in row["base"] for row in non_this), "deref2 base should name v2"

indexed = [row for row in rows if row["line"] == "14" and row["form"] == "indexed"]
assert indexed, "missing indexed row on sub_426100 line 14 (4 * ...)"
assert indexed[0]["dec"] == "17236", f"indexed base offset: {indexed[0]['dec']}"
assert indexed[0]["base"] == "this", f"indexed base: {indexed[0]['base']}"

calls = {row["expr"] for row in rows if row["form"] == "call"}
assert "FloatFunctionPassThrough" in calls, "missing call row for FloatFunctionPassThrough"
assert all(row["hex"] == "" for row in rows if row["form"] == "call")

tire = extract_file(REPO / "functions" / "TireForce_004e9100.c")
tire_3740 = [row for row in tire if row["line"] == "33" and row["base"] == "this"]
assert any(row["dec"] == "14960" and row["form"] == "array" for row in tire_3740), \
    "missing this[3740] -> 0x3A70 array row"
assert any(row["dec"] == "4" and row["base"] == "a4" and row["form"] == "array"
           for row in tire), "missing a4[1] pointer-param array row"
assert any(row["line"] == "54" and row["base"] == "this" and row["dec"] == "15424"
           for row in tire), "missing this[v13 + 3856] variable-index array row"

named = extract_file(RACING_LINE)
assert named, "no rows extracted from AIRacingLine"
assert all(row["func"] == "AIRacingLine" for row in named), "mixed func naming"
assert not any(row["func"].startswith("sub_4") for row in named), "sub_ name leaked"

with tempfile.TemporaryDirectory() as tmp:
    unmatched = Path(tmp) / "unmatched_sub_426100.txt"
    extract_file(FIXTURE, unmatched)
    assert unmatched.is_file(), "unmatched file not written"

print("OK")
