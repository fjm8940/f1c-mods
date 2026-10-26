#!/usr/bin/env python
"""C5 - write-readiness table and report (research only, nothing is written).

Inputs (read-only):
  investigations/ai_brain_map/ai_brain_abi_v1.json
  persistence_map.jsonl (repo root; lab probe, slot 1)
  phase2/WRITER_CENSUS.csv (C4)
  tools/lab.py NEVER_WRITE, findings (0x45FC crash, 0x3D6C limbo)

Outputs (phase2/):
  WRITE_READINESS.csv, WRITE_READINESS.md
"""
from __future__ import annotations

import csv
import json
import re
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
AI = REPO / "investigations" / "ai_brain_map"
PH2 = AI / "phase2"

STAGE = {
    "sub_426430": 2, "OpponentProximityScanTeamRecognition": 4, "AIRacingLine": 5,
    "AISteering": 6, "sub_426100": 7, "AISteeringCorrection": 8,
    "PhysicsGearShiftLogicWithThresholds": 9, "SteeringAngleController": 10,
    "sub_4258C0": 10,
}
STAGE_NAME = {
    1: "1 heading/init", 2: "2 waypoint locator", 3: "3 pre-traffic (mode/pushlap/ability)",
    4: "4 opponent scan", 5: "5 AIRacingLine", 6: "6 AISteering", 7: "7 steering law",
    8: "8 AISteeringCorrection", 9: "9 gear logic", 10: "10 mode switch",
    11: "11 output copy",
}

NEVER_WRITE = {
    0x32C8: "LIVE_MARKER (slot liveness; int32 -1 means dead)",
    0x4410: "RetiredFlag", 0x45F4: "ActiveFlag", 0x4064: "FinishState",
    0x454C: "callToBox", 0xD58: "RearWingIntactFlag (arms retirement)",
    0x10C0: "KILLS THE PROCESS (2026-07-27 sweep)",
    0x33C8: "KILLS THE PROCESS (2026-07-27 sweep)",
}
RISK_EXTRA = {
    0x45FC: "yield/interaction target: self-pointer write is the prime crash suspect "
            "(finding ai-yield-target-self-pointer-crash-2026-10-02); hold another car's pointer",
    0x3D6C: "carConditions: value 2 starts the re-retire LIMBO loop "
            "(finding limbo-unlocked-carconditions); write only with the 2026-08-03 unlock set",
    0x4408: "published lateral target: no plain store found; AISteering epilogue last-wins",
    0x44B4: "steering lock divisor: no writer found in the corpus (doc 54 section 7)",
}
HOOK_OVERRIDE = {
    0x4484: "write after the stage-4 reset (AIMainUpdate L244, 1e6) and before AISteeringCorrection (stage 8)",
    0x4420: "AIMainUpdate zeroes it at L242 unless 0x45FC != 0 or carConditions == 2; drive the 0x45FC gate (another car's pointer) or write after L242",
    0x4408: "AISteering epilogue last-wins (0x440C / request 0x4420 / forced 0x4424); set the request before stage 6 or post-hook",
    0x4510: "written by the mode switch (stage 10) and late branches; write before stage 10 or post-hook",
    0x4434: "post-hook 0x424E70 (after the stage-11 output copy) or write after stage 11",
    0x4438: "post-hook 0x424E70 (after the stage-11 output copy) or write after stage 11",
    0x442C: "post-hook 0x424E70 (after the stage-11 output copy) or write after stage 11",
    0x357C: "written by the stage-11 output copy; post-hook only",
    0x3580: "written by the stage-11 output copy; post-hook only",
    0x3584: "written by the stage-11 output copy; post-hook only",
    0x4548: "read by AISteering (stage 6); set before stage 6, post-hook is too late for that tick",
    0x453C: "recalculated by AIAbility (called at AIMainUpdate L216, stage 3); write before L216 or post-hook",
    0x4500: "recalculated by AIAbility/AIPushLap (stage 3); write before stage 3 or post-hook",
    0x4504: "recalculated by AIAbility/AIPushLap (stage 3); write before stage 3 or post-hook",
    0x4580: "recalculated by AIAbility/AIPushLap (stage 3); write before stage 3 or post-hook",
}


def main() -> int:
    abi = json.load(open(AI / "ai_brain_abi_v1.json", encoding="utf-8"))
    persist = {}
    for line in open(REPO / "persistence_map.jsonl", encoding="utf-8"):
        r = json.loads(line)
        persist[int(r["off"], 16)] = r

    census = defaultdict(lambda: dict(writers=set(), readers=set(), lines=defaultdict(list)))
    for r in csv.DictReader(open(PH2 / "WRITER_CENSUS.csv", encoding="utf-8")):
        f = r["field_id"]
        (census[f]["writers"] if r["access"] == "W" else census[f]["readers"]).add(r["function"])
        if r["access"] == "W":
            census[f]["lines"][r["function"]].append(int(r["line"]))

    rows = []
    for f in abi["fields"]:
        dec = int(f["dec"])
        hexv = int(f["hex"], 16)
        c = census[f["id"]]
        p = persist.get(hexv)
        pclass = p["class"] if p else "not probed"
        if pclass in ("SKIPPED_POINTER", "SKIPPED_PROTECTED", "READ_FAILED"):
            pclass += " (probe skipped/failed)"

        in_tick = {}
        outside = []
        for fn in sorted(c["writers"]):
            if fn in STAGE:
                in_tick[fn] = STAGE[fn]
            elif fn == "AIMainUpdate":
                ln = max(c["lines"][fn])
                if ln <= 182:
                    in_tick[fn] = 1
                elif ln <= 242:
                    in_tick[fn] = 3
                elif ln == 243:
                    in_tick[fn] = 4
                elif ln == 244:
                    in_tick[fn] = 4.5
                elif ln == 245:
                    in_tick[fn] = 5
                elif ln == 246:
                    in_tick[fn] = 5.5
                elif ln == 247:
                    in_tick[fn] = 6
                elif ln == 248:
                    in_tick[fn] = 7
                elif ln == 249:
                    in_tick[fn] = 8
                elif ln <= 255:
                    in_tick[fn] = 9
                elif ln <= 269:
                    in_tick[fn] = 10
                else:
                    in_tick[fn] = 11
            else:
                outside.append(fn)
        last = max(in_tick.values()) if in_tick else None
        last_txt = "; ".join("%s=stage %s" % (k, v) for k, v in
                             sorted(in_tick.items(), key=lambda kv: kv[1]))
        if hexv in NEVER_WRITE:
            hook = "DO NOT WRITE (%s)" % NEVER_WRITE[hexv]
        elif hexv in HOOK_OVERRIDE:
            hook = HOOK_OVERRIDE[hexv]
        elif last is None:
            hook = ("outside the AI tick; external write is only meaningful if the "
                    "persistence class is LATCHED, otherwise post-hook 0x424E70")
        else:
            hook = ("write after stage %s (%s), before the next consumer, or post-hook "
                    "0x424E70" % (last, STAGE_NAME.get(int(last), "?")))

        risky = "no"
        notes = []
        if not f["readers"] and not c["readers"]:
            notes.append("no exact-offset reader/writer in the corpus census (indirect/array access)")
        if hexv in NEVER_WRITE:
            risky, _ = "yes", notes.append(NEVER_WRITE[hexv])
        if hexv in RISK_EXTRA:
            risky = "yes"
            notes.append(RISK_EXTRA[hexv])
        if pclass.startswith("RECALCULATED"):
            notes.append("game rewrites it in-tick; a single external write does not stick")
        rows.append(dict(field_id=f["id"], group=f["group"], hex=f["hex"], dec=dec,
                         type=f["type"], persistence_class=pclass,
                         in_tick_writers="; ".join("%s@%s" % (k, v) for k, v in
                                                   sorted(in_tick.items(), key=lambda kv: kv[1])),
                         last_writer_stage=("" if last is None else str(last)),
                         outside_tick_writers="; ".join(sorted(set(outside))),
                         hook_position=hook, risky=risky, risk_notes=" | ".join(notes)))

    with open(PH2 / "WRITE_READINESS.csv", "w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(rows)

    with open(PH2 / "WRITE_READINESS.md", "w", encoding="utf-8") as fh:
        fh.write("# WRITE_READINESS - AI Brain ABI v1 (research only, 2026-10-02)\n\n")
        fh.write("Generated by `phase2/analysis/c5_write_readiness.py`. NOTHING here has been written to\n")
        fh.write("the game. Persistence classes come from `persistence_map.jsonl` (lab probe, slot 1,\n")
        fh.write("one write then next-tick read; `LATCHED` = persisted, `RECALCULATED` = game rewrote it,\n")
        fh.write("`VOLATILE*` = changed during the probe, `SKIPPED_POINTER/PROTECTED/READ_FAILED` = no\n")
        fh.write("probe value). Stage numbers are doc 54 section 1: 1 heading, 2 waypoint locator,\n")
        fh.write("3 pre-traffic (AIAbility/AIPushLap/AIStateFlagSetter), 4 opponent scan, 5 AIRacingLine,\n")
        fh.write("6 AISteering, 7 steering law, 8 AISteeringCorrection, 9 gear logic, 10 mode switch,\n")
        fh.write("11 output copy. Writers per field: `phase2/WRITER_CENSUS.csv`.\n\n")
        fh.write("Rule of thumb: a write sticks only if it happens after the field's last in-tick\n")
        fh.write("writer and survives to the consumer. Fields the game RECALCULATES need a per-tick\n")
        fh.write("hook; LATCHED fields can also be held from outside. A post-hook on AIMainUpdate\n")
        fh.write("0x424E70 (the mechanical_failures base) overrides outputs for one tick.\n\n")
        by_group = defaultdict(list)
        for r in rows:
            by_group[r["group"]].append(r)
        for group in ("state", "perception", "plan_speed", "plan_line", "decision", "watchdog"):
            fh.write("## %s\n\n" % group)
            for r in sorted(by_group[group], key=lambda x: int(x["dec"])):
                tag = "RISK: " if r["risky"] == "yes" else ("NOTE: " if r["risk_notes"] else "")
                fh.write("- **%s** (%s): persistence %s; last in-tick writer %s; outside %s. %s%s\n" % (
                    r["field_id"], r["hex"], r["persistence_class"],
                    ("stage %s" % r["last_writer_stage"]) if r["last_writer_stage"] else "none",
                    (r["outside_tick_writers"] or "none"),
                    r["hook_position"],
                    (tag + r["risk_notes"]) if r["risk_notes"] else ""))
            fh.write("\n")
        fh.write("## Risky fields (write with care / do not write)\n\n")
        for r in rows:
            if r["risky"] == "yes":
                fh.write("- **%s** (%s): %s\n" % (r["field_id"], r["hex"], r["risk_notes"]))
        fh.write("\n## Adjacent risky offsets not in ABI v1 (never write)\n\n")
        for off in (0x45FC, 0x4410, 0x45F4, 0x4064, 0x454C, 0xD58, 0x10C0, 0x33C8, 0x32C8):
            reason = RISK_EXTRA.get(off) or NEVER_WRITE.get(off) or "lab probe risk list"
            fh.write("- 0x%X: %s\n" % (off, reason))
        fh.write("\nSources: `tools/lab.py` NEVER_WRITE; findings `ai-yield-target-self-pointer-crash-2026-10-02`,\n")
        fh.write("`limbo-unlocked-carconditions`; doc 54 sections 3, 4 and 6.\n")

    risky_n = sum(1 for r in rows if r["risky"] == "yes")
    latched = sum(1 for r in rows if r["persistence_class"] == "LATCHED")
    recalced = sum(1 for r in rows if r["persistence_class"].startswith("RECALCULATED"))
    print("== C5 summary ==")
    print("fields: %d; LATCHED: %d; RECALCULATED: %d; risky: %d" % (
        len(rows), latched, recalced, risky_n))
    print("outputs: WRITE_READINESS.csv, WRITE_READINESS.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
