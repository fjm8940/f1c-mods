# d1_corner_speed.py -- D1.3: replay the corner-speed target formula vs logged 0x43FC.
#
# Claim under test (doc 54 section 3b, DERIVED; AIComputePassScore 0x42A780):
#   braking_target_speed(0x43FC) for the limiting waypoint ==
#   (TireForceSolver(wp) * 0x4504 + 0x4500) * 0x4580 * 0x453C * wp+0x84
# and AIComputePassScore's caller multiplies that by 0x4554 (AIRacingLine 0x425DBC), logged as
# unnamed_4554; both variants are reported (they are identical if 0x4554 == 1.0).
#
# Data: %TEMP%\ai_brain_live\watch_race4fullc + watch_race4fulld (Monza; cars_static.jsonl present).
# Read-only. Excludes: null limiting waypoint, target == 9999, branch_flag/branch_val != 0, and
# snapshots without a matching logged waypoint among the 20 or without a cars_static row.
# No SC/RF marker exists in the brain logs; both Monza runs had the owner's SC/RF mods active.
# The formula is a per-car function of logged state, so SC periods cannot bias it beyond the
# inputs already logged; this is recorded as a limitation, not used to filter.
import bisect
import json
import os
import struct
import sys
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib  # noqa: E402  (phase-2 read-only helper: situations(), snaps(), run_dir())
from tireforcesolver import tireforcesolver  # noqa: E402

RUNS = ('watch_race4fullc', 'watch_race4fulld')
TBL_3856 = 0x3C4C
TBL_3872 = 0x3C8C
TBL_3904 = 0x3D0C


def load_cars_static(run):
    """slot -> (sorted sim_t list, values list)"""
    p = c1b_lib.run_dir(run) / 'cars_static.jsonl'
    by_slot = {}
    with open(p, encoding='utf-8') as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            r = json.loads(line)
            by_slot.setdefault(r['slot'], []).append((r['sim_t'], r['values']))
    for slot in by_slot:
        by_slot[slot].sort(key=lambda t: t[0])
    return by_slot


def latest_row(rows, sim_t):
    ts = [t for t, _ in rows]
    i = bisect.bisect_right(ts, sim_t) - 1
    return rows[i][1] if i >= 0 else None


def table(values, base, n=15):
    return [values['0x%X' % (base + 4 * k)] for k in range(n)]


def as_float(v):
    """Some GUESS registry fields are logged as raw int bits (brain_core leaves unknown type raw)."""
    if isinstance(v, int):
        return struct.unpack('<f', struct.pack('<I', v & 0xffffffff))[0]
    return v


def collect():
    rows = []
    skip = dict(no_lim=0, target_9999=0, branch=0, wp_missing=0, no_static=0, no_v3520=0, empty_wp=0)
    for run in RUNS:
        static = load_cars_static(run)
        for idx in c1b_lib.situations(run):
            for s in c1b_lib.snaps(run, idx['folder']):
                f = s['fields']
                lim = f.get('limiting_waypoint_ptr')
                bt = f.get('braking_target_speed')
                if not lim or lim == '0x0':
                    skip['no_lim'] += 1
                    continue
                if bt is None or abs(bt - 9999.0) < 1e-3:
                    skip['target_9999'] += 1
                    continue
                wps = s.get('waypoints') or []
                if not wps:
                    skip['empty_wp'] += 1
                    continue
                wp = next((w for w in wps if w['ptr'] == lim), None)
                if wp is None:
                    skip['wp_missing'] += 1
                    continue
                if wp.get('branch_flag') != 0 or wp.get('branch_val') != 0:
                    skip['branch'] += 1
                    continue
                cs = static.get(s['slot'])
                if not cs:
                    skip['no_static'] += 1
                    continue
                vals = latest_row(cs, s['sim_t'])
                if vals is None:
                    skip['no_static'] += 1
                    continue
                v3520 = (s.get('extra') or {}).get('v3520')
                if v3520 is None:
                    skip['no_v3520'] += 1
                    continue
                v3520 = as_float(v3520)
                T = {
                    3397: v3520,
                    3740: vals['0x3A7C'],
                    3855: vals['0x3C48'], 3856: table(vals, TBL_3856),
                    3871: vals['0x3C88'], 3872: table(vals, TBL_3872),
                    3903: vals['0x3D08'], 3904: table(vals, TBL_3904),
                }
                tfs = tireforcesolver(wp['k112'], wp['k116'], wp['k120'], wp['k7c'], T)
                base = ((tfs * f['corner_speed_force_coeff'] + f['corner_speed_force_offset'])
                        * f['corner_speed_scale'] * f['ai_pace_multiplier'] * wp['speed_k'])
                full = base * as_float(f.get('unnamed_4554', 1.0))
                rows.append(dict(
                    run=run, sim_t=s['sim_t'], slot=s['slot'], driver=s.get('driver'),
                    measured=bt, tfs=tfs, pred=base, pred_full=full,
                    ratio=bt / full if full else None,
                    ratio_nomult=bt / base if base else None,
                    wp_ptr=wp['ptr'], k112=wp['k112'], k116=wp['k116'], k120=wp['k120'],
                    k7c=wp['k7c'], speed_k=wp['speed_k'],
                    coeff=f['corner_speed_force_coeff'], offset=f['corner_speed_force_offset'],
                    scale=f['corner_speed_scale'], pace=f['ai_pace_multiplier'],
                    mult4554=as_float(f.get('unnamed_4554')) if f.get('unnamed_4554') is not None else None,
                    v3520=v3520, t3903=vals['0x3D08'],
                ))
    return rows, skip


def quant(xs, q):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(q * len(xs)))]


def summarize(rows, label):
    ratios = [r['ratio'] for r in rows if r['ratio'] is not None]
    if not ratios:
        print('%s: n=0' % label)
        return
    inside2 = sum(1 for x in ratios if abs(x - 1.0) <= 0.02) / len(ratios)
    print('%s: n=%d  ratio med=%.4f  p10=%.4f  p90=%.4f  min=%.4f  max=%.4f  within +-2%%: %.1f%%'
          % (label, len(ratios), quant(ratios, 0.5), quant(ratios, 0.1), quant(ratios, 0.9),
             min(ratios), max(ratios), 100.0 * inside2))


def main():
    rows, skip = collect()
    print('D1 corner-speed target replay (Monza watch_race4fullc + watch_race4fulld)')
    print('exclusions: %s' % skip)
    print('samples: %d' % len(rows))
    if not rows:
        return
    mults = sorted({round(r['mult4554'], 6) for r in rows if r['mult4554'] is not None})
    print('0x4554 values seen: %s' % mults[:8])
    summarize(rows, 'ALL (pred * 0x4554)')
    summarize(rows, 'ALL (pred, doc 54 form)')
    for run in RUNS:
        summarize([r for r in rows if r['run'] == run], run)
    print()
    print('by driver (median ratio, n):')
    drivers = {}
    for r in rows:
        drivers.setdefault(r['driver'], []).append(r['ratio'])
    for d in sorted(drivers, key=lambda k: quant(drivers[k], 0.5) if drivers[k] else 0, reverse=True):
        xs = [x for x in drivers[d] if x is not None]
        if xs:
            print('  %-24s med=%.4f n=%d' % (d, quant(xs, 0.5), len(xs)))
    print()
    print('ratio vs predicted target (bins, m/s):')
    for lo, hi in ((0, 20), (20, 40), (40, 60), (60, 90), (90, 130)):
        xs = [r['ratio'] for r in rows if lo <= r['pred_full'] < hi and r['ratio'] is not None]
        if xs:
            print('  pred %3d-%-3d n=%-4d med=%.4f p10=%.4f p90=%.4f'
                  % (lo, hi, len(xs), quant(xs, 0.5), quant(xs, 0.1), quant(xs, 0.9)))
    print()
    print('ratio vs measured target bins (m/s):')
    for lo, hi in ((0, 20), (20, 40), (40, 60), (60, 90), (90, 140)):
        xs = [r['ratio'] for r in rows if lo <= r['measured'] < hi and r['ratio'] is not None]
        if xs:
            print('  meas %3d-%-3d n=%-4d med=%.4f p10=%.4f p90=%.4f'
                  % (lo, hi, len(xs), quant(xs, 0.5), quant(xs, 0.1), quant(xs, 0.9)))
    print()
    print('10 worst |ratio-1|:')
    worst = sorted((r for r in rows if r['ratio'] is not None), key=lambda r: abs(r['ratio'] - 1.0), reverse=True)[:10]
    for r in worst:
        print('  %s sim=%.1f slot=%d %-20s meas=%.3f pred=%.3f ratio=%.4f tfs=%.3f '
              'k112=%.5f k116=%.7f k120=%.5f k7c=%.3f speed_k=%.3f pace=%.4f'
              % (r['run'][-1], r['sim_t'], r['slot'], (r['driver'] or '?')[:20], r['measured'], r['pred_full'],
                 r['ratio'], r['tfs'], r['k112'], r['k116'], r['k120'], r['k7c'], r['speed_k'], r['pace']))


if __name__ == '__main__':
    main()
