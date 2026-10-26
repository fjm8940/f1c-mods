#!/usr/bin/env python3
"""AI form jitter for F1 Challenge 99-02 - makes AI race results vary.

Problem: RaceAbility (rcd) is baked deterministically at session init, so the
same driver (lowest RA) wins the AI pecking order every time. In-game noise
(duel rolls, sim noise) is far too small to overturn 1-point RA gaps.

Solution (uses the game's own pipeline - docs/41: rcd is read fresh each
session): before launching the game, jitter each driver's RaceAbility and
QualifyingAbility inside their .rcd, with momentum so form comes in streaks.

Model:
  form[d]   = rho * form[d] + sigma * N(0,1)        (EMA momentum, rho=0.5)
  RA_eff    = clamp(RA_base + form[d] + event, 0.2, 20)
  QA_eff    = clamp(QA_base + sigma_q * N(0,1), 0.2, 25)   (no momentum ->
              qualifying order diverges from race order, like real life)
  event     = with prob 0.06, an extra +/-3.5 "brilliant/awful weekend"

Sizing guide (verified scale: baseline = (1 - RA/100) x GDS):
  sigma 1.0 -> order shuffles among drivers ~1 RA apart (top 3-4)
  sigma 2.0 -> top 5-6 rotate wins; midfield swaps weekly
  sigma 3.0+ -> hierarchy dissolves; backmarkers still stay back (10+ gap)

Originals are backed up on first run; 'restore' puts them back byte-exact.
The game reads rcd at session init, so run this BETWEEN races (game can stay
open - values apply to the NEXT session load).
"""
import argparse
import json
import os
import random
import re
import sys

STATE_FILE = "ai_form_state.json"
BACKUP_FILE = "ai_form_backup.json"

RHO = 0.5          # form momentum (0 = no memory, 0.9 = season-long streaks)
SIGMA = 2.0        # weekly form noise, RA points
SIGMA_Q = 2.0      # independent qualifying noise
EVENT_P = 0.06     # chance of a brilliant/awful weekend
EVENT_SIZE = 3.5
RA_MIN, RA_MAX = 0.2, 20.0
QA_MIN, QA_MAX = 0.2, 25.0

KEY_RE = {
    "RaceAbility": re.compile(r"^(\s*RaceAbility\s*=\s*)([\-0-9.]+)(.*)$", re.I | re.M),
    "QualifyingAbility": re.compile(r"^(\s*QualifyingAbility\s*=\s*)([\-0-9.]+)(.*)$", re.I | re.M),
}


def find_rcds(drivers_dir):
    for root, _, files in os.walk(drivers_dir):
        for f in files:
            if f.lower().endswith(".rcd"):
                yield os.path.join(root, f)


def load_json(path):
    return json.load(open(path, encoding="utf-8")) if os.path.exists(path) else {}


def cmd_jitter(a):
    drivers_dir = a.drivers_dir
    rcds = sorted(find_rcds(drivers_dir))
    if not rcds:
        sys.exit(f"no .rcd under {drivers_dir}")

    backup = load_json(BACKUP_FILE)
    state = load_json(STATE_FILE)
    first_run = not backup

    print(f"{'driver':<28} {'RA base':>8} {'form':>6} {'RA_eff':>7} {'QA_eff':>7}")
    print("-" * 62)
    for path in rcds:
        raw = open(path, encoding="utf-8", errors="replace").read()
        name = os.path.splitext(os.path.basename(path))[0]

        if first_run:
            backup[path] = raw  # byte-exact originals

        m_ra = KEY_RE["RaceAbility"].search(raw)
        m_qa = KEY_RE["QualifyingAbility"].search(raw)
        if not m_ra:
            continue
        ra_base = float(m_ra.group(2))
        qa_base = float(m_qa.group(2)) if m_qa else None

        form = RHO * state.get(name, 0.0) + random.gauss(0.0, SIGMA)
        event = 0.0
        if random.random() < EVENT_P:
            event = random.choice([-1, 1]) * EVENT_SIZE
        state[name] = round(form, 3)

        ra_eff = min(max(ra_base + form + event, RA_MIN), RA_MAX)
        out = KEY_RE["RaceAbility"].sub(
            lambda m: f"{m.group(1)}{ra_eff:.2f}{m.group(3)}", raw, count=1)

        qa_eff_s = ""
        if qa_base is not None:
            qa_eff = min(max(qa_base + random.gauss(0.0, SIGMA_Q), QA_MIN), QA_MAX)
            out = KEY_RE["QualifyingAbility"].sub(
                lambda m: f"{m.group(1)}{qa_eff:.2f}{m.group(3)}", out, count=1)
            qa_eff_s = f"{qa_eff:7.2f}"

        flag = " *" if event else ""
        print(f"{name:<28} {ra_base:8.2f} {form:+6.2f} {ra_eff:7.2f} {qa_eff_s:>7}{flag}")
        if not a.dry_run:
            open(path, "w", encoding="utf-8").write(out)

    if not a.dry_run:
        json.dump(backup, open(BACKUP_FILE, "w", encoding="utf-8"))
        json.dump(state, open(STATE_FILE, "w", encoding="utf-8"), indent=1)
        print(f"\napplied. originals backup: {BACKUP_FILE}  form state: {STATE_FILE}")
        if first_run:
            print("(first run - backup captured BEFORE any edits)")
    else:
        print("\n(dry run - no files written)")


def cmd_restore(a):
    backup = load_json(BACKUP_FILE)
    if not backup:
        sys.exit(f"no backup at {BACKUP_FILE}")
    for path, raw in backup.items():
        open(path, "w", encoding="utf-8").write(raw)
    print(f"restored {len(backup)} rcd files byte-exact from {BACKUP_FILE}")
    if os.path.exists(STATE_FILE) and a.reset_form:
        os.remove(STATE_FILE)
        print("form state reset")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    j = sub.add_parser("jitter", help="apply one race-weekend of form jitter")
    j.add_argument("--drivers-dir", required=True,
                   help=r"e.g. <install>\SeasonData\Drivers\2001")
    j.add_argument("--sigma", type=float, default=SIGMA, help="weekly RA noise (default 2.0)")
    j.add_argument("--dry-run", action="store_true")
    j.set_defaults(fn=lambda a: (globals().__setitem__('SIGMA', a.sigma), cmd_jitter(a))[1])

    r = sub.add_parser("restore", help="restore all rcd files from backup")
    r.add_argument("--reset-form", action="store_true")
    r.set_defaults(fn=cmd_restore)

    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
