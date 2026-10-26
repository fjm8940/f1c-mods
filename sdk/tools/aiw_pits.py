"""
aiw_pits.py - extract every track's PIT BOX and GARAGE coordinates from the
shipped .AIW files, plus the pit-lane waypoint path.

WHY
---
The safety car needs somewhere to wait that is not a grid box, and a car can
now be put there: veh+0x34E0/0x34E4/0x34E8 is the authoritative position and a
12-byte write teleports a live car (FACT, 2026-08-03). What was missing was the
DESTINATION, per track, without guessing - which is exactly what the .AIW
already carries:

    [PITS]   TeamIndex=N
             PitPos=(x,y,z)  PitOri=(x,y,z)      the pit box on the lane
             GarPos=(i,x,y,z) GarOri=(i,x,y,z)   three garage bays behind it

    [Waypoint]  wp_branchID   0 = main path, 1 = PIT LANE, 2+ = per-slot stalls
                wp_score      (sector, LAP DISTANCE)

Both decodings are the file's own: the branch comment is written in the AIW,
and wp_score's second field was confirmed against a live car (Monza pit lane
runs 5271.7 -> 190.6 on a 5744.77 m lap, wrapping the start/finish line).

WHICH TEAM IS THE SAFETY CAR? That is a RUNTIME question, not a file one - his
constructor index is veh+0x48A0 and it depends on the season's entrant list.
Read it once in a race, then index this table with it. The tool prints every
team so the answer is a lookup, not a re-run.

USAGE
    python tools/aiw_pits.py <track.aiw>              one track, full detail
    python tools/aiw_pits.py --all [gamedir]          every track -> JSON
    python tools/aiw_pits.py --all --team 11          every track, one team

Default game dir is the real install, NOT the stale SeasonData copy on the
Desktop - that copy has no .AIW files in it at all.

READ-ONLY. Touches no game process and no game file.
"""

import io
import json
import os
import re
import sys

DEFAULT_GAMEDIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "..",
    "F1 Challenge V10", "SeasonData", "Circuits")
OUT_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                        "aiw_pits.json")

NUM = r"[-+0-9.eE]+"


def _floats(s):
    out = []
    for tok in s.split(","):
        tok = tok.strip()
        try:
            out.append(float(tok))
        except ValueError:
            pass
    return out


def parse_aiw(path):
    """Return {'pits': {teamIndex: {...}}, 'lane': [...], 'meta': {...}}."""
    txt = io.open(path, encoding="latin-1", errors="replace").read()

    meta = {}
    for key in ("lap_length", "number_waypoints", "pitlanes", "pitspots",
                "garagespots", "startinggrid", "LeftHandedPits"):
        m = re.search(key + r"\s*=\s*(" + NUM + ")", txt)
        if m:
            meta[key] = float(m.group(1))

    # ---- [PITS] -----------------------------------------------------------
    pits = {}
    team = None
    in_pits = False
    for line in txt.splitlines():
        line = line.split("//")[0].strip()
        if line.startswith("["):
            in_pits = line.upper().startswith("[PITS")
            continue
        if not in_pits or "=" not in line:
            continue
        k, _, v = line.partition("=")
        k = k.strip()
        v = v.strip().strip("()")
        if k == "TeamIndex":
            team = int(float(v))
            pits.setdefault(team, {"garage": {}})
        elif team is None:
            continue
        elif k == "PitPos":
            pits[team]["pit_pos"] = _floats(v)
        elif k == "PitOri":
            pits[team]["pit_ori"] = _floats(v)
        elif k in ("GarPos", "GarOri"):
            f = _floats(v)
            if len(f) >= 4:
                bay = int(f[0])
                slot = pits[team]["garage"].setdefault(bay, {})
                slot["pos" if k == "GarPos" else "ori"] = f[1:4]

    # ---- [Waypoint] : the pit-lane branch ---------------------------------
    recs = []
    cur = None
    for line in txt.splitlines():
        line = line.split("//")[0].strip()
        m = re.match(r"(wp_\w+)=\((.*)\)", line)
        if not m:
            continue
        k, v = m.group(1), m.group(2)
        if k == "wp_pos":
            if cur is not None:
                recs.append(cur)
            cur = {}
        if cur is None:
            continue
        cur[k] = _floats(v)
    if cur is not None:
        recs.append(cur)

    lane = []
    for i, r in enumerate(recs):
        br = r.get("wp_branchID", [0])
        if br and int(br[0]) == 1:
            score = r.get("wp_score", [0, 0])
            lane.append({
                "wp": i,
                "pos": r.get("wp_pos"),
                "perp": r.get("wp_perp"),
                "lap_dist": score[1] if len(score) > 1 else None,
            })

    main = []
    for i, r in enumerate(recs):
        br = r.get("wp_branchID", [0])
        if (not br) or int(br[0]) == 0:
            score = r.get("wp_score", [0, 0])
            main.append({
                "wp": i,
                "pos": r.get("wp_pos"),
                "lap_dist": score[1] if len(score) > 1 else None,
            })

    lap_len = meta.get("lap_length") or 0.0
    approach = pit_approach(main, lane, lap_len)

    return {"meta": meta, "pits": pits, "lane": lane, "main": main,
            "approach": approach}


# How far BEFORE the pit-lane entrance the safety car is dropped. He must
# arrive on the racing line with enough road left to have his own AI steer him
# into the lane and CROSS THE REAL XPITIN TRIGGER - forging the entry state
# instead is DISPROVEN (findings f1c-synth-pit-entry-by-teleport-rejected).
APPROACH_BACK_M = 180.0


def pit_approach(main, lane, lap_len):
    """The main-path point ~APPROACH_BACK_M before the pit lane starts.

    Returns {'wp','pos','yaw','lap_dist','back_m'} or None. `yaw` follows the
    game's own convention - forward is (sin yaw, cos yaw) - so it is the
    bearing to the NEXT main waypoint, which is what a car dropped there needs
    in order to keep driving rather than to be pointing at the wall.
    """
    import math

    if not lane or not main or not lap_len:
        return None
    entry_d = lane[0].get("lap_dist")
    if entry_d is None:
        return None

    want = (entry_d - APPROACH_BACK_M) % lap_len

    best = None
    for k, w in enumerate(main):
        d = w.get("lap_dist")
        if d is None or not w.get("pos"):
            continue
        gap = abs(((d - want + lap_len / 2.0) % lap_len) - lap_len / 2.0)
        if best is None or gap < best[0]:
            best = (gap, k)
    if best is None:
        return None

    k = best[1]
    p = main[k]["pos"]
    here = main[k]["lap_dist"]

    # WHICH WAY IS FORWARD COMES FROM LAP DISTANCE, NOT FROM FILE ORDER.
    #
    # This used to take the next record in the file and point at it. File order
    # matches driving order on most tracks and does not on all of them: Monaco
    # put the car on the approach point FACING BACKWARDS (driver, 2026-08-03).
    # wp_score's lap distance always increases along the driving direction, so
    # the neighbour a little further round the lap is the reliable "ahead", and
    # taking one ~20 m away rather than the immediate next one keeps the
    # bearing out of the noise of closely spaced waypoints.
    AHEAD_M = 20.0
    ahead = None
    best_gap = None
    for w in main:
        d = w.get("lap_dist")
        if d is None or not w.get("pos"):
            continue
        fwd = (d - here) % lap_len          # 0 .. lap_len, forward only
        if fwd < AHEAD_M:
            continue
        if best_gap is None or fwd < best_gap:
            best_gap = fwd
            ahead = w["pos"]
    if ahead is None:
        ahead = main[(k + 1) % len(main)]["pos"] or p

    # THE GAME'S YAW IS THE PATH BEARING PLUS PI.
    #
    # Measured 2026-08-03 against three cars the game had just placed on the
    # Monaco grid, each compared with the AIW bearing at its own position:
    #
    #     car yaw 2.793   bearing -0.380   difference 3.173
    #     car yaw 2.979   bearing -0.186   difference 3.165
    #     car yaw 2.921   bearing -0.207   difference 3.128
    #
    # i.e. forward is (-sin yaw, -cos yaw). Without the offset the safety car
    # is placed FACING BACKWARDS - driver, Monaco: "it is literally facing the
    # wrong way". Monza got away with it only because the AI turned him round
    # before the pit entrance.
    yaw = math.atan2(ahead[0] - p[0], ahead[2] - p[2]) + math.pi
    if yaw > math.pi:
        yaw -= 2.0 * math.pi
    return {
        "wp": main[k]["wp"],
        "pos": p,
        "yaw": yaw,
        "lap_dist": main[k]["lap_dist"],
        "back_m": ((entry_d - main[k]["lap_dist"]) % lap_len),
    }


def show(path, only_team=None):
    d = parse_aiw(path)
    print("=" * 78)
    print(os.path.basename(path))
    m = d["meta"]
    print("  lap %.1f m   waypoints %d   pitspots %d   garagespots %d   "
          "left-handed pits %d"
          % (m.get("lap_length", 0), m.get("number_waypoints", 0),
             m.get("pitspots", 0), m.get("garagespots", 0),
             m.get("LeftHandedPits", 0)))

    lane = d["lane"]
    if lane:
        print("  pit lane: %d waypoints, wp %d..%d, lap distance %.1f -> %.1f"
              % (len(lane), lane[0]["wp"], lane[-1]["wp"],
                 lane[0]["lap_dist"] or 0, lane[-1]["lap_dist"] or 0))
        e = lane[-1]
        print("  lane merge (pit exit end): (%.3f, %.3f, %.3f)" % tuple(e["pos"]))
    ap = d.get("approach")
    if ap:
        print("  pit APPROACH: wp %d (%9.3f,%7.3f,%9.3f) yaw %.3f, "
              "%.1f m before the lane at lap distance %.1f"
              % (ap["wp"], ap["pos"][0], ap["pos"][1], ap["pos"][2],
                 ap["yaw"], ap["back_m"], ap["lap_dist"] or 0))

    teams = sorted(d["pits"])
    if only_team is not None:
        teams = [t for t in teams if t == only_team]
    for t in teams:
        p = d["pits"][t]
        pp = p.get("pit_pos")
        po = p.get("pit_ori")
        if pp:
            print("  team %-3d  PIT BOX (%9.3f,%7.3f,%9.3f)  ori (%.3f,%.3f,%.3f)"
                  % (t, pp[0], pp[1], pp[2], *(po or [0, 0, 0])))
        for bay in sorted(p["garage"]):
            g = p["garage"][bay]
            if "pos" in g:
                print("            garage %d (%9.3f,%7.3f,%9.3f)"
                      % (bay, *g["pos"]))


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return

    only_team = None
    if "--team" in args:
        i = args.index("--team")
        only_team = int(args[i + 1])
        del args[i:i + 2]

    if args and args[0] == "--all":
        gamedir = args[1] if len(args) > 1 else DEFAULT_GAMEDIR
        found = []
        for root, _dirs, files in os.walk(gamedir):
            for f in files:
                if f.lower().endswith(".aiw"):
                    found.append(os.path.join(root, f))
        found.sort()
        print("%d .aiw files under %s\n" % (len(found), gamedir))

        out = {}
        bad = []
        for p in found:
            try:
                d = parse_aiw(p)
            except Exception as e:      # a corrupt or unusual track must not
                bad.append((p, str(e)))  # stop the other 209
                continue
            key = os.path.splitext(os.path.basename(p))[0]
            out[key] = {
                "path": os.path.relpath(p, gamedir),
                "meta": d["meta"],
                "teams": {str(t): d["pits"][t] for t in sorted(d["pits"])},
                "lane_first": d["lane"][0] if d["lane"] else None,
                "lane_last": d["lane"][-1] if d["lane"] else None,
                "lane_count": len(d["lane"]),
                "approach": d["approach"],
            }
            if only_team is not None:
                show(p, only_team)

        with open(OUT_JSON, "w", encoding="utf-8") as fh:
            json.dump(out, fh, indent=1)
        print("\nwrote %s  (%d tracks)" % (os.path.abspath(OUT_JSON), len(out)))

        n_pits = sum(1 for v in out.values() if v["teams"])
        n_lane = sum(1 for v in out.values() if v["lane_count"])
        print("tracks with a [PITS] table: %d   with a pit-lane branch: %d"
              % (n_pits, n_lane))
        teamcounts = {}
        for v in out.values():
            teamcounts[len(v["teams"])] = teamcounts.get(len(v["teams"]), 0) + 1
        print("teams per track:", dict(sorted(teamcounts.items())))
        if bad:
            print("\n%d file(s) failed to parse:" % len(bad))
            for p, e in bad[:5]:
                print("   %s: %s" % (os.path.basename(p), e))
        return

    show(args[0], only_team)


if __name__ == "__main__":
    main()
