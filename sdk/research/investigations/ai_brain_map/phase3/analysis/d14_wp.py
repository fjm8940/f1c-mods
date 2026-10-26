# d14_wp.py -- D14: live value ranges for every logged waypoint-record field.
# Runs with 20-waypoint records: watch_race3, watch_race4fullc, watch_race4fulld,
# watch_20261002T194606Z. Prints min/median/p10/p90/max and distinct-count per key.
import os
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as L  # noqa: E402

RUNS = ('watch_race3', 'watch_race4fullc', 'watch_race4fulld', 'watch_20261002T194606Z')
KEYS = ('x', 'y', 'z', 'rx', 'rz', 'dx', 'dz', 'wall_l', 'wall_r', 'line_l', 'line_r', 'dist',
        'seg_len', 'speed_k', 'k112', 'k116', 'k120', 'k7c', 'k80', 'branch_flag', 'branch_val')


def q(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))]


def main():
    vals = defaultdict(list)
    distinct = defaultdict(set)
    n_wp = 0
    n_runs = defaultdict(int)
    for run in RUNS:
        for x in L.situations(run):
            for s in L.snaps(run, x['folder']):
                for wp in (s.get('waypoints') or []):
                    n_wp += 1
                    n_runs[run] += 1
                    for k in KEYS:
                        v = wp.get(k)
                        if v is None:
                            continue
                        vals[k].append(v)
                        if len(distinct[k]) < 20000:
                            distinct[k].add(v)
    print('== D14 waypoint field live ranges (20-wp runs) ==')
    print('waypoint records sampled: %d %s' % (n_wp, dict(n_runs)))
    print('%-12s %9s %9s %9s %9s %9s %8s %9s' % ('key', 'min', 'p10', 'med', 'p90', 'max', 'distinct', 'n'))
    for k in KEYS:
        v = vals[k]
        if not v:
            print('%-12s missing' % k)
            continue
        print('%-12s %9.4f %9.4f %9.4f %9.4f %9.4f %8d %9d'
              % (k, min(v), q(v, 0.1), q(v, 0.5), q(v, 0.9), max(v), len(distinct[k]), len(v)))


if __name__ == '__main__':
    main()
