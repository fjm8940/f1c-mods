#!/usr/bin/env python3
"""AI difficulty calculator for F1 Challenge 99-02.

Recommends AI Driver Strength (slider 70-120) and AI Realism (aggression, 0..1)
from the player's lap time at a given track, so races stay challenging and
consistent as the player improves.

Model (runtime-verified 2026-07-26, docs/42):
  GDS(s)   = lerp(WorstAdjust -> MidAdjust, (s-70)/30)        s <= 100
             lerp(MidAdjust -> BestAdjust, (s-100)*0.05)      s >  100
  baseline = (1 - RaceAbility/100) * GDS(s)    (veh+0x44F4, front-runner pace)
  laptime  ~ T_ref * (GDS_ref / GDS(s)) ^ e    (e = track exponent, default 1.0)

  aggression a = realism * TrackAggression (rcd default 1.0), clamp >= 0.01
  -> follow gap floor 20-10a m; duel pressure rate ~ a; calm-down range (1+a)*20 m

Calibration: measure the front AI's race lap time at two slider values, feed both
points once ('calibrate'); the track exponent is then exact for that track.
"""
import argparse
import json
import os
import re
import sys

PROJ = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CAL_PATH = os.path.join(PROJ, "tools", "ai_difficulty_calibration.json")

SLIDER_MIN, SLIDER_MAX = 70, 120
DEFAULT_EXPONENT = 1.0

# realism presets mapped to the aggression scalar a (TrackAggression=1.0 drivers)
REALISM_PRESETS = {
    "calm":     0.40,   # follow floor 16 m, slow pressure build — clean gaps
    "balanced": 0.55,   # follow floor 14.5 m
    "spicy":    0.70,   # follow floor 13 m, fast pressure build — frequent duels
    "max":      1.00,   # follow floor 10 m, relentless — expect contact
}


def parse_aiw(path):
    """Extract WorstAdjust/MidAdjust/BestAdjust from a track .AIW file."""
    txt = open(path, encoding="utf-8", errors="replace").read()
    def grab(key, default):
        m = re.search(rf"{key}\s*=\s*\(?\s*([0-9.]+)", txt, re.I)
        return float(m.group(1)) if m else default
    return {
        "worst": grab("WorstAdjust", 0.85),
        "mid":   grab("MidAdjust", 1.00),
        "best":  grab("BestAdjust", 1.10),
    }


def gds(slider, adj):
    """GlobalDifficultyScalar at slider value, given AIW adjustments."""
    if slider <= 100:
        t = (slider - 70) / 30.0
        return adj["worst"] * (1.0 - t) + adj["mid"] * t
    t = (slider - 100) * 0.05
    return adj["mid"] * (1.0 - t) + adj["best"] * t


def slider_for_gds(target, adj):
    """Invert gds(); returns clamped slider and whether clamping happened."""
    lo, hi = gds(SLIDER_MIN, adj), gds(SLIDER_MAX, adj)
    clamped = not (lo <= target <= hi)
    target = min(max(target, lo), hi)
    if target <= adj["mid"]:
        t = (target - adj["worst"]) / (adj["mid"] - adj["worst"])
        s = 70 + 30.0 * t
    else:
        t = (target - adj["mid"]) / (adj["best"] - adj["mid"])
        s = 100 + t / 0.05
    return min(max(round(s), SLIDER_MIN), SLIDER_MAX), clamped


def load_cal():
    if os.path.exists(CAL_PATH):
        return json.load(open(CAL_PATH))
    return {}


def save_cal(cal):
    json.dump(cal, open(CAL_PATH, "w"), indent=1)


def cmd_calibrate(a):
    """Fit track exponent e from two (slider, front-AI laptime) points."""
    adj = parse_aiw(a.aiw)
    g1, g2 = gds(a.s1, adj), gds(a.s2, adj)
    # T2/T1 = (g1/g2)^e  ->  e = ln(T2/T1)/ln(g1/g2)
    import math
    e = math.log(a.t2 / a.t1) / math.log(g1 / g2)
    cal = load_cal()
    cal[os.path.basename(a.aiw).lower()] = {"exponent": round(e, 4),
                                            "points": [[a.s1, a.t1], [a.s2, a.t2]]}
    save_cal(cal)
    print(f"calibrated {os.path.basename(a.aiw)}: exponent e = {e:.4f} "
          f"(slider {a.s1} -> {a.t1:.3f}s, {a.s2} -> {a.t2:.3f}s)")
    print(f"saved -> {CAL_PATH}")


def cmd_calc(a):
    adj = parse_aiw(a.aiw)
    cal = load_cal()
    e = cal.get(os.path.basename(a.aiw).lower(), {}).get("exponent", DEFAULT_EXPONENT)

    # reference: front AI (lowest RaceAbility on grid) at slider 100
    ra = a.ra_front
    g100 = gds(100, adj)  # == adj["mid"] == 1.0 by definition
    # player's lap as equivalent baseline demand:
    #   T_front(s) = T_front(100) * (g100/GDS(s))^e ; want T_front(s*) = T_player
    # We do not know T_front(100) absolutely, but baseline(100) = (1-ra/100)*g100
    # anchors the *relative* scale: a player at lap T_p matches front AI when
    #   GDS(s*) = g100 * (T_ai_ref / T_p) ; T_ai_ref defaults to T_p -> GDS ~ g100.
    # With --ref-lap the caller supplies the observed front-AI lap at --ref-slider.
    if a.ref_lap and a.ref_slider:
        g_ref = gds(a.ref_slider, adj)
        g_target = g_ref * (a.ref_lap / a.player_lap) ** (1.0 / e)
    else:
        # no anchor: assume player lap == front-AI lap at slider 100, and the
        # front AI should run margin_pct % quicker than the player:
        #   T_p/(1+m) = T_p * (g100/G*)^e  ->  G* = g100 * (1+m)^(1/e)
        m = a.margin_pct / 100.0
        g_target = g100 * (1.0 + m) ** (1.0 / e)
    s, clamped = slider_for_gds(g_target, adj)

    # midfield pace spread at chosen slider (RA 1.8..12 typical field)
    g = gds(s, adj)
    b_front = (1 - 1.8 / 100.0) * g
    b_back = (1 - 12.0 / 100.0) * g

    print(f"track: {os.path.basename(a.aiw)}  "
          f"(Worst={adj['worst']} Mid={adj['mid']} Best={adj['best']}, e={e})")
    print(f"player lap: {a.player_lap:.3f}s   target GDS: {g_target:.4f}")
    print(f"--> AI Driver Strength = {s}" + ("  (CLAMPED - out of model range!)" if clamped else ""))
    r = REALISM_PRESETS[a.preset] if a.realism is None else a.realism
    print(f"--> AI Realism = {r:.2f}  ({a.preset} preset)" if a.realism is None
          else f"--> AI Realism = {r:.2f}")
    follow_floor = 20.0 - 10.0 * r
    calm_range = (1.0 + r) * 20.0
    print(f"    aggression a={r:.2f}: follow-gap floor {follow_floor:.1f} m, "
          f"duel pressure range {calm_range:.1f} m, build rate x{r:.2f}")
    print(f"field pace spread at slider {s}: baseline {b_front:.4f} (RA 1.8) .. "
          f"{b_back:.4f} (RA 12) -> ~{(b_front-b_back)/b_front*100:.1f}% laptime spread")
    if clamped:
        print("note: player pace outside AI range - adjust PLR physics caps "
              "(AI Brake Power/Grip Usage, AI Corner Grip Usage) or grid RA instead")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("calc", help="recommend slider + realism")
    c.add_argument("--aiw", required=True, help="path to track .AIW file")
    c.add_argument("--player-lap", type=float, required=True,
                   help="player representative lap time, seconds")
    c.add_argument("--ra-front", type=float, default=1.8,
                   help="RaceAbility of grid's front-runner (default 1.8)")
    c.add_argument("--ref-lap", type=float,
                   help="observed front-AI lap time at --ref-slider (anchors model)")
    c.add_argument("--ref-slider", type=int,
                   help="slider value at which --ref-lap was observed")
    c.add_argument("--margin-pct", type=float, default=0.0,
                   help="front AI should be this %% quicker than player (no-ref mode)")
    c.add_argument("--preset", choices=list(REALISM_PRESETS), default="balanced")
    c.add_argument("--realism", type=float, help="explicit realism 0..1 (overrides preset)")
    c.set_defaults(fn=cmd_calc)

    k = sub.add_parser("calibrate", help="fit track exponent from two points")
    k.add_argument("--aiw", required=True)
    k.add_argument("--s1", type=int, required=True)
    k.add_argument("--t1", type=float, required=True)
    k.add_argument("--s2", type=int, required=True)
    k.add_argument("--t2", type=float, required=True)
    k.set_defaults(fn=cmd_calibrate)

    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
