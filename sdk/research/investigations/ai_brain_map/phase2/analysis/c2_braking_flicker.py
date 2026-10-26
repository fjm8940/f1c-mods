#!/usr/bin/env python
"""C2 — braking flicker analysis over a brain_recorder / GUI JSONL run.

Read-only: reads snapshots.jsonl and events.jsonl from a run folder (e.g.
%TEMP%\\ai_brain_live\\smoke) and prints:
  1. braking_start / braking_end events for the chosen driver;
  2. a per-tick table for a time window;
  3. state combos (limiting waypoint set/null x brake on/off) for the whole run.

Usage:
  python c2_braking_flicker.py RUN_DIR [--driver NAME] [--from T --to T]
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def _ptr(v):
    if isinstance(v, str):
        return v.lower()
    return "0x%X" % v


def _ticks(run: Path, driver: str, t0: float | None, t1: float | None):
    out = []
    with open(run / "snapshots.jsonl", encoding="utf-8") as fh:
        for line in fh:
            if driver not in line:
                continue
            s = json.loads(line)
            if s.get("driver") != driver:
                continue
            if t0 is not None and s["sim_t"] < t0:
                continue
            if t1 is not None and s["sim_t"] > t1:
                continue
            out.append(s)
    return out


def _combo(run: Path, driver: str):
    counts = {"lim+brake": 0, "lim+no_brake": 0, "null+brake": 0, "null+no_brake": 0}
    flips = 0
    prev = None
    lim_found_but_no_brake = []
    with open(run / "snapshots.jsonl", encoding="utf-8") as fh:
        for line in fh:
            if driver not in line:
                continue
            s = json.loads(line)
            if s.get("driver") != driver:
                continue
            f = s["fields"]
            lim = _ptr(f["limiting_waypoint_ptr"])
            brake = f["cmd_brake"] > 0
            tgt = f["braking_target_speed"]
            if lim != "0x0" and brake:
                counts["lim+brake"] += 1
            elif lim != "0x0" and not brake:
                counts["lim+no_brake"] += 1
                lim_found_but_no_brake.append((s["sim_t"], f["speed"] * 3.6, tgt,
                                               f["cmd_throttle"]))
            elif lim == "0x0" and brake:
                counts["null+brake"] += 1
            else:
                counts["null+no_brake"] += 1
            if prev is not None and prev != brake:
                flips += 1
            prev = brake
    return counts, flips, lim_found_but_no_brake


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("run", type=Path)
    ap.add_argument("--driver", required=True)
    ap.add_argument("--from", dest="t0", type=float, default=None)
    ap.add_argument("--to", dest="t1", type=float, default=None)
    a = ap.parse_args()

    events = []
    with open(a.run / "events.jsonl", encoding="utf-8") as fh:
        for line in fh:
            e = json.loads(line)
            if e.get("driver") == a.driver and e["kind"].startswith("braking"):
                events.append(e)
    print("=== run %s | driver %s | %d braking events ===" % (a.run.name, a.driver, len(events)))
    for e in events:
        print("  %9.3f  %-12s target=%-9s v=%6.1f km/h" % (
            e["sim_t"], e["kind"],
            ("%.1f" % e["target_kmh"]) if e["target_kmh"] is not None else "null",
            e["speed_kmh"]))

    ticks = _ticks(a.run, a.driver, a.t0, a.t1)
    if a.t0 is not None:
        print("=== per-tick window %.3f..%.3f (%d ticks) ===" % (a.t0, a.t1, len(ticks)))
        prev = None
        for s in ticks:
            f = s["fields"]
            dv = "" if prev is None else " dv=%+5.2f" % (f["speed"] - prev)
            print("%9.3f v=%6.1f thr=%4.2f brk=%4.2f tgt=%8.1f lim=%-11s cur=%-11s "
                  "sp0=%6.1f look=%s%s" % (
                      s["sim_t"], f["speed"] * 3.6, f["cmd_throttle"], f["cmd_brake"],
                      f["braking_target_speed"], _ptr(f["limiting_waypoint_ptr"]),
                      _ptr(f["current_waypoint_ptr"]), f["speed_profile"][0],
                      f["steering_lookahead_index"], dv))
            prev = f["speed"]

    counts, flips, odd = _combo(a.run, a.driver)
    total = sum(counts.values())
    print("=== whole-run brake/limiting-waypoint combos (%d samples) ===" % total)
    for k, v in counts.items():
        print("  %-16s %4d  (%.1f%%)" % (k, v, 100.0 * v / total))
    print("  brake on/off transitions: %d" % flips)
    print("  limiting wp set but brake==0: %d samples" % len(odd))
    for row in odd[:6]:
        print("    t=%9.3f v=%6.1f tgt=%8.1f thr=%4.2f" % row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
