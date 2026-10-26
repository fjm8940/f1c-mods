#!/usr/bin/env python
"""D12 - write-readiness table for ALL 140 AI Brain ABI v1.2 fields (research only).

Same method/columns as phase2/analysis/c5_write_readiness.py (new file, old one untouched):
inputs = ai_brain_abi_v1_2.json, persistence_map.jsonl (lab probe, slot 1),
phase2/WRITER_CENSUS.csv; NEVER_WRITE/RISK_EXTRA/HOOK_OVERRIDE tables copied from c5.
Adds `prior_art_verdict` citing the IR writable-contract sections
(investigations/ai_interaction_role/README.md L3694, L4124, L4780, L5580, L5722, L5891)
and the mechanical_failures live-write findings.

Outputs (phase3/): WRITE_READINESS_V12.csv, WRITE_READINESS_V12.md.
"""
from __future__ import annotations

import csv
import json
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent          # phase3/analysis
PH3 = HERE.parent                                # phase3
REPO = HERE.parents[3]                           # f1c_ida_decompiled
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
    0x4408: "published lateral target: D10 closes the writer question - AISteering epilogue last-wins "
            "(0x440C copy / request 0x4420 / forced 0x4424); set the request before stage 6",
    0x44B4: "steering lock divisor: D10 - init-only value from veh+0x396C (AIDriverStructInit); no per-tick writer",
}
HOOK_OVERRIDE = {
    0x4484: "write after the stage-4 reset (AIMainUpdate L244, 1e6) and before AISteeringCorrection (stage 8)",
    0x4420: "AIMainUpdate zeroes it at L242 unless 0x45FC != 0 or carConditions == 2; drive the 0x45FC gate "
            "(another car's pointer) or write after L242",
    0x4408: "AISteering epilogue last-wins (0x440C / request 0x4420 / forced 0x4424); set the request before stage 6",
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
IR_GENERAL = ("IR 3694/4124/4780/5580/5722/5891: no general writable AI contract proved; ownership, "
              "phase, value range and replay/network arbitration stay open (observation-only)")
PRIOR_ART = {
    0x357C: "IR 5597-5634: phase-local physics workspace after handoff; overwritten in-mode; NOT a writable channel",
    0x3580: "IR 5597-5634: handed to wheel-steering input in mode 1; phase-local; NOT a writable channel",
    0x3584: "IR 5597-5634: phase-local physics workspace; NOT a writable channel",
    0x45FC: "finding ai-yield-target-self-pointer-crash-2026-10-02: self/pointer write crashes; hold another car's pointer; " + IR_GENERAL,
    0x4420: "finding ai-lateral-request-4420-zeroed-each-tick-2026-10-02: zeroed each tick unless 0x45FC/carConditions==2; "
            "write after stage 4; " + IR_GENERAL,
    0x4408: "finding ai-pulloff-lateral-chain-v4-2026-10-02: the 0x4548/0x4420/0x4424 chain steers the AI "
            "(not a hard override of the steering law); " + IR_GENERAL,
    0x4510: "mode 7 = native stop, mode 4/5 = park (doc 54); the mode switch owns it last (stage 10); " + IR_GENERAL,
    0x4484: "speed governor (traffic/pit clamp), reset to 1e6 at stage 4; short writes give smooth braking (doc 54); " + IR_GENERAL,
    0x453C: "D4: verified 4 s AIAbility model (traffic-driven); write before AIAbility or post-hook; " + IR_GENERAL,
    0x4500: "D4: .rcd CORNERINGADD, sub_424080 (parse) + AIAbility/AIPushLap (stage 3); " + IR_GENERAL,
    0x4504: "D4: .rcd CORNERINGMULT, sub_424080 (parse) + AIAbility/AIPushLap (stage 3); " + IR_GENERAL,
    0x4580: "D4: wet factor from rain_ability_scaled + global wetness; recomputed at parse/stage 3; " + IR_GENERAL,
    0x44B4: "D10: AIDriverStructInit only (value from veh+0x396C); constant per session; " + IR_GENERAL,
    0x3D6C: "carConditions 2 = limbo (finding limbo-unlocked-carconditions); 1 = car-problem branch (AIAbility 0.85*baseline); " + IR_GENERAL,
    0x457C: "difficulty cap = AIDifficultyScalar*0.75, written by AIAbility/AISteeringCorrection; " + IR_GENERAL,
    0x446C: "watchdog counter: +5/tick opposing yaw, threshold 0x4558 triggers mode 1; not an input; " + IR_GENERAL,
}
PERSIST_TEXT = {
    "LATCHED": "value persisted after the one-shot probe write",
    "RECALCULATED": "game rewrote it within a tick",
    "VOLATILE_BASELINE": "changed during the probe on its own",
    "VOLATILE": "changed during the probe on its own",
}


def main() -> int:
    abi = json.load(open(AI / "ai_brain_abi_v1_2.json", encoding="utf-8"))
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
        if hexv in NEVER_WRITE:
            hook = "DO NOT WRITE (%s)" % NEVER_WRITE[hexv]
        elif hexv in HOOK_OVERRIDE:
            hook = HOOK_OVERRIDE[hexv]
        elif hexv in PRIOR_ART:
            hook = HOOK_OVERRIDE.get(hexv, "route through the stage that wrote it last, or post-hook 0x424E70")
        elif last is None:
            hook = ("outside the AI tick; external write is only meaningful if the persistence class is "
                    "LATCHED, otherwise post-hook 0x424E70")
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
        if hexv in PRIOR_ART:
            pa = PRIOR_ART[hexv]
        else:
            pa = IR_GENERAL if (pclass.startswith(("LATCHED", "VOLATILE")) or last is not None) else "not covered by a prior-art write claim"
        rows.append(dict(field_id=f["id"], group=f["group"], hex=f["hex"], dec=dec,
                         type=f["type"], persistence_class=pclass,
                         in_tick_writers="; ".join("%s@%s" % (k, v) for k, v in sorted(in_tick.items(), key=lambda kv: kv[1])),
                         last_writer_stage=("" if last is None else str(last)),
                         outside_tick_writers="; ".join(sorted(set(outside))),
                         hook_position=hook, risky=risky, risk_notes=" | ".join(notes),
                         prior_art_verdict=pa))

    cols = ["field_id", "group", "hex", "dec", "type", "persistence_class", "in_tick_writers",
            "last_writer_stage", "outside_tick_writers", "hook_position", "risky", "risk_notes",
            "prior_art_verdict"]
    with open(PH3 / "WRITE_READINESS_V12.csv", "w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=cols, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)

    with open(PH3 / "WRITE_READINESS_V12.md", "w", encoding="utf-8") as fh:
        fh.write("# WRITE_READINESS_V12 - AI Brain ABI v1.2, all 140 fields (research only, 2026-10-02)\n\n")
        fh.write("Generated by `phase3/analysis/d12_write_readiness.py` (same method as the phase-2 C5 script;\n")
        fh.write("the v1.2 registry adds 79 fields, so this table is complete). NOTHING here has been written\n")
        fh.write("to the game. Persistence classes: `persistence_map.jsonl` lab probe (slot 1, one write,\n")
        fh.write("next-tick read). `prior_art_verdict` cites the IR writable-contract sections\n")
        fh.write("(`investigations/ai_interaction_role/README.md` L3694/L4124/L4780/L5580/L5722/L5891) and the\n")
        fh.write("mechanical_failures live-write findings. Hook stages: doc 54 section 1.\n\n")
        classes = defaultdict(int)
        for r in rows:
            classes[r["persistence_class"].split(" ")[0]] += 1
        fh.write("## Summary\n\n")
        for k in sorted(classes, key=lambda k: -classes[k]):
            fh.write("- %s: %d\n" % (k, classes[k]))
        fh.write("Total fields: %d; risky (`DO NOT WRITE`/crash list): %d\n\n"
                 % (len(rows), sum(1 for r in rows if r["risky"] == "yes")))
        by_group = defaultdict(list)
        for r in rows:
            by_group[r["group"]].append(r)
        for group in sorted(by_group):
            fh.write("## %s\n\n" % group)
            for r in sorted(by_group[group], key=lambda x: int(x["dec"])):
                tag = " **RISK**: " if r["risky"] == "yes" else " "
                fh.write("- **%s** (%s): persistence %s.%s%s | last in-tick writer %s | outside %s | %s\n"
                         % (r["field_id"], r["hex"], r["persistence_class"], tag,
                            (" " + r["risk_notes"]) if r["risk_notes"] else "",
                            ("stage %s" % r["last_writer_stage"]) if r["last_writer_stage"] else "none",
                            r["outside_tick_writers"] or "none",
                            r["hook_position"]))
                fh.write("  - prior art: %s\n" % r["prior_art_verdict"])
            fh.write("\n")

    latched = sum(1 for r in rows if r["persistence_class"] == "LATCHED")
    recalced = sum(1 for r in rows if r["persistence_class"].startswith("RECALCULATED"))
    print("== D12 summary ==")
    print("fields: %d; LATCHED: %d; RECALCULATED: %d; risky: %d"
          % (len(rows), latched, recalced, sum(1 for r in rows if r["risky"] == "yes")))
    print("outputs: WRITE_READINESS_V12.csv, WRITE_READINESS_V12.md")
    print()
    print("top candidates for a first write test (LATCHED or clean hook, low risk, visible effect):")
    visible = {0x4484, 0x4510, 0x4538, 0x453C, 0x4548, 0x457C, 0x4408, 0x4420, 0x4424,
               0x4500, 0x4504, 0x4580, 0x44CC, 0x44C4, 0x45F8, 0x4060, 0x403C, 0x4450,
               0x4588, 0x45E0, 0x458C, 0x4534, 0x4184}
    def score(r):
        s = 0
        pc = r["persistence_class"].split(" ")[0]
        s += {"LATCHED": 3, "VOLATILE_BASELINE": 1, "RECALCULATED": 0}.get(pc, -2)
        if int(r["hex"], 16) in visible:
            s += 2
        if r["last_writer_stage"]:
            s += 1
        if r["risky"] == "yes":
            s -= 10
        return s
    ranked = sorted(rows, key=score, reverse=True)
    for r in ranked[:20]:
        print("  %-26s %s %-22s %-16s score=%d  %s"
              % (r["field_id"], r["hex"], r["persistence_class"][:22],
                 ("last stage %s" % r["last_writer_stage"]) if r["last_writer_stage"] else "no in-tick writer",
                 score(r), r["hook_position"][:60]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
