#!/usr/bin/env python3
"""Summarise a web_telemetry race trace: tyre wear and component-life drain per car.

Reads the newest (or a named) web_telemetry_race_*.jsonl written by
mods/f1cui_web_remote and reports, per car, how fast tyres and component
lifetimes actually fell over the race.

    python3 tools/wear_trace_report.py [file.jsonl] [--laps]

Key caveat baked into the output: the component countdown only ticks for cars
with wg == 1. The human driver reads wg == 0, so their life/oil figures are
frozen and are reported as "not simulated" rather than as zero wear.
"""
import json
import sys
import glob
import os

MOD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "mods", "f1cui_web_remote")


def load(path):
    wear, laps, pits, meta = [], [], [], {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except json.JSONDecodeError:
                continue          # a truncated final line is expected if the game crashed
            t = r.get("t")
            if t == "wear":
                wear.append(r)
            elif t == "lap":
                laps.append(r)
            elif t == "pitstop":
                pits.append(r)
            elif t == "session_start":
                meta = r
    return wear, laps, pits, meta


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_laps = "--laps" in sys.argv

    if args:
        path = args[0]
    else:
        files = sorted(glob.glob(os.path.join(MOD_DIR, "web_telemetry_race_*.jsonl")),
                       key=os.path.getmtime)
        if not files:
            sys.exit("no web_telemetry_race_*.jsonl found in %s" % MOD_DIR)
        path = files[-1]

    wear, laps, pits, meta = load(path)
    print("file      : %s" % os.path.basename(path))
    print("session   : type %s, %s laps scheduled, %s cars"
          % (meta.get("ty", "?"), meta.get("tl", "?"), meta.get("vc", "?")))
    print("samples   : %d wear, %d lap records, %d pit stops" % (len(wear), len(laps), len(pits)))
    if not wear:
        sys.exit("no wear samples yet — is a race running?")

    span = max(r["st"] for r in wear) - min(r["st"] for r in wear)
    print("covering  : %.1f s of session time\n" % span)

    by_car = {}
    for r in wear:
        by_car.setdefault(r["id"], []).append(r)

    rows = []
    for cid, rs in by_car.items():
        rs.sort(key=lambda r: r["st"])
        a, b = rs[0], rs[-1]
        dt = b["st"] - a["st"]
        if dt <= 0:
            continue
        d_laps = b["lap"] - a["lap"]

        tyre_a = sum(a["tyre"].values()) / 4.0
        tyre_b = sum(b["tyre"].values()) / 4.0
        # wear expressed as per-mille of tread lost per lap
        tyre_per_lap = ((tyre_a - tyre_b) * 1000.0 / d_laps) if d_laps > 0 else None

        eng_drain = (a["life"]["eng"] - b["life"]["eng"]) / dt
        simulated = b.get("wg") == 1

        rows.append(dict(
            id=cid, name=b["n"][:22], pos=b["pos"], lap=b["lap"], dlap=d_laps,
            wg=b.get("wg"), simulated=simulated,
            eng=b["life"]["eng"], eng_drain=eng_drain,
            brk=b["life"]["brk"], gbx=b["life"]["gbx"], sus=b["life"]["sus"],
            tyre_lost=(tyre_a - tyre_b) * 1000.0, tyre_per_lap=tyre_per_lap,
            oil=b["eng"]["oil"], fuel=b["fuel"], comp=b["comp"],
            fs=b["fs"], ret=b["ret"], rc=b["rc"], dead=b["eng"]["dead"],
        ))

    rows.sort(key=lambda r: (r["pos"] if r["pos"] > 0 else 999))

    print("%-3s %-22s %4s %5s %10s %9s %9s %8s %7s  %s" %
          ("P", "driver", "lap", "tyre", "tyre/lap", "englife", "eng/s", "oil", "fuel", "status"))
    print("-" * 108)
    for r in rows:
        if r["simulated"]:
            eng = "%9.0f" % r["eng"]
            drain = "%9.3f" % r["eng_drain"]
        else:
            eng = "%9s" % "frozen"
            drain = "%9s" % "n/a"
        tpl = ("%10.3f" % r["tyre_per_lap"]) if r["tyre_per_lap"] is not None else "%10s" % "-"
        status = []
        if not r["simulated"]:
            status.append("NOT SIMULATED (wg=%s)" % r["wg"])
        if r["ret"]:
            status.append("RETIRED")
        if r["fs"] == 2:
            status.append("DNF")
        if r["dead"]:
            status.append("ENGINE DEAD")
        if r["rc"]:
            status.append("rc=%d" % r["rc"])
        print("%-3d %-22s %4d %5.2f %s %s %s %8.1f %7.1f  %s" %
              (r["pos"], r["name"], r["lap"], r["tyre_lost"], tpl, eng, drain,
               r["oil"], r["fuel"], ", ".join(status)))

    sim = [r for r in rows if r["simulated"]]
    if sim:
        drains = [r["eng_drain"] for r in sim]
        print("\nengine-life drain across %d simulated cars: min %.3f  max %.3f  mean %.3f per second"
              % (len(sim), min(drains), max(drains), sum(drains) / len(drains)))
        tpl = [r["tyre_per_lap"] for r in sim if r["tyre_per_lap"] is not None]
        if tpl:
            print("tyre wear per lap (per-mille of tread, mean of 4 corners): min %.3f  max %.3f  mean %.3f"
                  % (min(tpl), max(tpl), sum(tpl) / len(tpl)))
        print("\nA flat drain near 1.0/s means the countdown is a plain clock and neither")
        print("revs nor oil temperature accelerate it. Anything above that is the wear")
        print("model biting — which is the open question this trace exists to answer.")

    if pits:
        print("\npit stops:")
        for p in pits:
            bits = ["lap %d" % p["lap_in"], "%.1fs" % p["dur"]]
            if p["tire_in"] != p["tire_out"]:
                bits.append("tyre %d->%d" % (p["tire_in"], p["tire_out"]))
            if p["fuel_out"] > p["fuel_in"]:
                bits.append("fuel +%.0fL" % (p["fuel_out"] - p["fuel_in"]))
            if p.get("fixed_fw"):
                bits.append("front wing")
            if p.get("fixed_rw"):
                bits.append("rear wing")
            print("  %-22s %s" % (p["n"][:22], ", ".join(bits)))

    if show_laps and laps:
        print("\nlap records: %d (use jq for detail)" % len(laps))


if __name__ == "__main__":
    main()
