#!/usr/bin/env python3
"""AI live form for F1 Challenge 99-02 - change an AI driver's pace MID-SESSION.

Mechanism (POKE-PROVEN 2026-07-26, docs/43 sec3.8): veh+0x44F4 (baseline pace
scalar) is baked at session init but READ every frame by AIAbility as the pace
target; it is never rewritten during the session. Writing it mid-race sticks,
and the live pace factor (veh+0x453C) walks to the new value at the game's own
+-0.01/frame rate - the form change looks completely natural (verified in-race:
Salo's pace factor walked 0.88128 -> 0.85128 over ~8 s after a -0.03 write).

Uses:
  - give a driver a bad day (factor 0.97-0.99 = +1..3 s/lap at Monza scale)
  - give a midfield driver the drive of his life (factor 1.005-1.02)
  - fade a leader over a stint (tire trouble narrative)

Factor is relative to the driver's ORIGINAL session baseline (captured on first
touch into ai_live_form_session.json), so repeated calls never compound.
'reset' restores originals. Same-session only - originals are invalid after a
game/session reload (the tool re-captures on first touch after 'clear').

Pace-factor scale (docs/42): 1.0% baseline ~= 1% lap time ~= 0.9 s at Monza.
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from poke import Game, find_pid  # noqa: E402

VEH_PTRS = 0x7C54F0
VEH_COUNT = 0x5C329C
OFF_NAME = 0x4620
OFF_BASELINE = 0x44F4
OFF_LIVE = 0x453C
OFF_FINISH = 0x4064
PLAYER_SLOT = 0
FACTOR_MIN, FACTOR_MAX = 0.85, 1.08

STATE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "ai_live_form_session.json")


def read_cstr(g, addr, n=40):
    raw = g.read(addr, n) or b""
    return raw.split(b"\0")[0].decode("ascii", errors="replace").strip()


def cars(g):
    cnt = g.read_val(VEH_COUNT, "int32") or 0
    out = []
    for i in range(min(cnt, 22)):
        veh = g.read_val(VEH_PTRS + i * 4, "ptr")
        if not veh:
            continue
        out.append({
            "slot": i, "veh": veh,
            "name": read_cstr(g, veh + OFF_NAME),
            "fs": g.read_val(veh + OFF_FINISH, "int32"),
            "baseline": g.read_val(veh + OFF_BASELINE, "float"),
            "live": g.read_val(veh + OFF_LIVE, "float"),
        })
    return out


def load_state():
    if os.path.exists(STATE_PATH):
        return json.load(open(STATE_PATH))
    return {}


def save_state(st):
    json.dump(st, open(STATE_PATH, "w"), indent=1)


def pick(c, token):
    if token.isdigit():
        for x in c:
            if x["slot"] == int(token):
                return x
    t = token.lower()
    for x in c:
        if t in x["name"].lower():
            return x
    sys.exit(f"no AI car matching '{token}'")


def apply_factor(g, st, car, factor, quiet=False):
    if car["slot"] == PLAYER_SLOT:
        sys.exit("refusing: slot 0 is the player")
    key = f'{car["slot"]}:{car["name"]}'
    if key not in st:
        st[key] = {"original": car["baseline"]}
        save_state(st)
    orig = st[key]["original"]
    new = orig * factor
    g.write_val(car["veh"] + OFF_BASELINE, "float", new)
    if not quiet:
        print(f'{car["name"]:<22} baseline {orig:.5f} -> {new:.5f} '
              f'(x{factor:.3f}); pace factor will walk there in seconds')


def cmd_list(g, a):
    st = load_state()
    print(f"{'slot':>4} {'FS':>3} {'baseline':>9} {'live':>8}  {'mod':>6}  Driver")
    print("-" * 60)
    for x in cars(g):
        key = f'{x["slot"]}:{x["name"]}'
        mod = ""
        if key in st:
            mod = f'x{x["baseline"]/st[key]["original"]:.3f}'
        print(f'{x["slot"]:>4} {x["fs"]:>3} {x["baseline"]:>9.5f} '
              f'{x["live"]:>8.5f}  {mod:>6}  {x["name"]}' +
                  ("  (player)" if x["slot"] == PLAYER_SLOT else ""))


def cmd_set(g, a):
    factor = a.factor
    if not (FACTOR_MIN <= factor <= FACTOR_MAX):
        sys.exit(f"factor out of safety range [{FACTOR_MIN}, {FACTOR_MAX}]")
    if factor > 1.03:
        print("warning: >+3% is faster than any natural in-game step", file=sys.stderr)
    st = load_state()
    apply_factor(g, st, pick(cars(g), a.driver), factor)


def cmd_fade(g, a):
    if not (FACTOR_MIN <= a.factor <= FACTOR_MAX):
        sys.exit("factor out of safety range")
    st = load_state()
    car = pick(cars(g), a.driver)
    key = f'{car["slot"]}:{car["name"]}'
    if key not in st:
        st[key] = {"original": car["baseline"]}
        save_state(st)
    orig = st[key]["original"]
    start = car["baseline"]
    steps = max(1, int(a.minutes * 60 / a.step))
    print(f'fading {car["name"]} x{start/orig:.3f} -> x{a.factor:.3f} '
          f'over {a.minutes:.1f} min ({steps} steps)')
    try:
        for i in range(1, steps + 1):
            f_now = (start / orig) + (a.factor - start / orig) * i / steps
            apply_factor(g, st, car, f_now, quiet=True)
            if i % max(1, steps // 5) == 0 or i == steps:
                print(f'  {i * 100 // steps:3d}%  baseline = {orig * f_now:.5f}')
            time.sleep(a.step)
    except KeyboardInterrupt:
        print("\ninterrupted - partial fade left in place ('reset' to undo)")


def cmd_reset(g, a):
    st = load_state()
    if not st:
        print("nothing to reset")
        return
    cs = cars(g)
    for x in cs:
        key = f'{x["slot"]}:{x["name"]}'
        if key in st and (a.driver is None or a.driver.lower() in x["name"].lower()):
            g.write_val(x["veh"] + OFF_BASELINE, "float", st[key]["original"])
            print(f'{x["name"]:<22} restored to {st[key]["original"]:.5f}')
            if a.driver is not None:
                del st[key]
    if a.driver is None:
        st = {}
    save_state(st)


def cmd_clear(g, a):
    if os.path.exists(STATE_PATH):
        os.remove(STATE_PATH)
    print("session state cleared (next touch re-captures originals)")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list").set_defaults(fn=cmd_list)

    s = sub.add_parser("set", help="instant form change (pace walks over seconds)")
    s.add_argument("driver", help="slot number or name substring")
    s.add_argument("factor", type=float, help="x baseline (0.97 = bad day, 1.01 = heroics)")
    s.set_defaults(fn=cmd_set)

    f = sub.add_parser("fade", help="ramp form over minutes")
    f.add_argument("driver")
    f.add_argument("factor", type=float)
    f.add_argument("--minutes", type=float, default=5.0)
    f.add_argument("--step", type=float, default=5.0, help="seconds between nudges")
    f.set_defaults(fn=cmd_fade)

    r = sub.add_parser("reset", help="restore original baselines")
    r.add_argument("driver", nargs="?", default=None)
    r.set_defaults(fn=cmd_reset)

    sub.add_parser("clear", help="forget captured originals").set_defaults(fn=cmd_clear)

    a = p.parse_args()
    if not find_pid():
        sys.exit("game not running")
    g = Game()
    a.fn(g, a)


if __name__ == "__main__":
    main()
