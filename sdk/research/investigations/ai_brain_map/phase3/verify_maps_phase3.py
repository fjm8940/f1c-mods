#!/usr/bin/env python
"""Phase-3 wrapper for the phase-1 B4 checker over phase3/maps/.

Imports the unchanged phase-1 `verify_maps` module, points its `HERE` at
`phase3/maps/` and runs the same checks (logic hex <-> cited source line,
Inputs/Outputs hex present in offsets_<Name>.csv). The phase-1 and phase-2 maps
are unaffected and can still be checked with their own scripts.

Exit code 0 only when there are no mismatches.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(REPO / "investigations" / "ai_brain_map"))
import verify_maps  # noqa: E402

MAPS = HERE / "maps"


def main() -> int:
    verify_maps.HERE = MAPS
    sources = verify_maps.source_index()
    maps = sorted(MAPS.glob("map_*.md"))
    mismatches: list[str] = []
    total_logic = total_hex = total_tables = 0
    for path in maps:
        lc, hc, tc = verify_maps.check_map(path, sources, mismatches)
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
