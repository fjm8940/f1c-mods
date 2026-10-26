#!/usr/bin/env python
"""C4/C5 helper: one extraction pass over the whole functions/ corpus.

Writes (outside the workspace, per retention policy):
  %TEMP%\\ai_brain_phase2\\all_rows.jsonl   one row per extracted access
  %TEMP%\\ai_brain_phase2\\all_calls.jsonl  one row per CALL
  %TEMP%\\ai_brain_phase2\\func_lines.json  function -> source line count

Read-only over functions/; no workspace files written.
"""
from __future__ import annotations

import json
import os
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
sys.path.insert(0, str(REPO / "investigations" / "ai_brain_map"))
import extract_offsets  # noqa: E402

OUT = Path(os.environ["TEMP"]) / "ai_brain_phase2"


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    files = sorted((REPO / "functions").glob("*.c"))
    rows_fh = open(OUT / "all_rows.jsonl", "w", encoding="utf-8")
    calls_fh = open(OUT / "all_calls.jsonl", "w", encoding="utf-8")
    lines = {}
    t0 = time.time()
    nrows = ncalls = 0
    for i, path in enumerate(files, 1):
        name = extract_offsets.func_name(path)
        lines[name] = len(path.read_text(encoding="utf-8").splitlines())
        for row in extract_offsets.extract_file(path):
            row["func"] = name
            if row["access"] == "CALL":
                calls_fh.write(json.dumps(row, separators=(",", ":")) + "\n")
                ncalls += 1
            else:
                rows_fh.write(json.dumps(row, separators=(",", ":")) + "\n")
                nrows += 1
        if i % 1000 == 0:
            print("  %d/%d files, %d rows, %.0fs" % (i, len(files), nrows, time.time() - t0),
                  flush=True)
    rows_fh.close()
    calls_fh.close()
    (OUT / "func_lines.json").write_text(json.dumps(lines), encoding="utf-8")
    print("done: %d files, %d offset rows, %d calls, %.0fs" % (
        len(files), nrows, ncalls, time.time() - t0))
    print("cache:", OUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
