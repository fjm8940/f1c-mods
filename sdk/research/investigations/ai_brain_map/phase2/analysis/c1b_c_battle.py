# c1b_c_battle.py — Run C from C1b: opponent slots and modes from battle situations.
# Read-only over %TEMP% watch folders. Prints a short per-run result.
#
# Questions (handoff C1 run C):
#  - are opp_slot_vehicle_ptr ranks nearest-first?
#  - does opp_mode index follow slot rank or team record?
#  - which modes appear when side by side / passing?
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L

SENTINEL = 999998.0


def samples(run, kind):
    for x in L.situations(run):
        if x['kind'] == kind:
            for s in L.snaps(run, x['folder']):
                yield x, s


def main():
    for run in L.RUNS:
        mono = n = side = 0
        rank_swap = 0
        drv_change = defaultdict(int)
        team_same = defaultdict(int)
        mode_same = defaultdict(int)
        base_mode_same = base_team_same = base_n = 0
        modes = Counter()
        transitions = Counter()
        prev = None
        near5 = Counter()
        near15 = Counter()
        for x, s in samples(run, 'battle'):
            f = s['fields']
            d = f['opp_slot_distance_sq']
            valid = [v for v in d if v < SENTINEL]
            n += 1
            if valid == sorted(valid):
                mono += 1
            opps = s.get('opponents', [])
            if opps and min(o['dist_m'] for o in opps) < 5:
                side += 1
                for o in opps:
                    if o['dist_m'] < 5:
                        near5[o['mode']] += 1
            for o in opps:
                if o['dist_m'] < 15:
                    near15[o['mode']] += 1
                modes[o['mode']] += 1
            cur = {o['rank']: o['driver'] for o in opps}
            if prev:
                pcur, pf, ps = prev
                if set(cur.values()) == set(pcur.values()) and cur != pcur:
                    rank_swap += 1
                for k in set(cur) & set(pcur):
                    base_n += 1
                    if f['opp_team_id'][k] == pf['opp_team_id'][k]:
                        base_team_same += 1
                    if f['opp_mode'][k] == pf['opp_mode'][k]:
                        base_mode_same += 1
                    if cur[k] != pcur[k]:
                        drv_change[k] += 1
                        if f['opp_team_id'][k] == pf['opp_team_id'][k]:
                            team_same[k] += 1
                        if f['opp_mode'][k] == pf['opp_mode'][k]:
                            mode_same[k] += 1
                for o in opps:
                    pm = None
                    for po in ps.get('opponents', []):
                        if po['driver'] == o['driver']:
                            pm = po
                    if pm and pm['mode'] != o['mode']:
                        transitions[(pm['mode'], o['mode'])] += 1
            prev = (cur, f, s)
        print('== Run C battle [%s] samples=%d side-by-side(<5m)=%d ==' % (run, n, side))
        print(' distance order non-decreasing %d/%d (%.1f%%)' % (mono, n, 100.0 * mono / max(n, 1)))
        dc = sum(drv_change.values())
        print(' rank-order swaps (same driver set): %d; rank driver changes: %d; of those team_id[k] same %d (%.1f%%), mode[k] same %d (%.1f%%)'
              % (rank_swap, dc, sum(team_same.values()), 100.0 * sum(team_same.values()) / max(dc, 1),
                 sum(mode_same.values()), 100.0 * sum(mode_same.values()) / max(dc, 1)))
        print(' baseline consecutive-tick persistence at rank k: team_id[k] same %.1f%%, mode[k] same %.1f%% (n=%d)'
              % (100.0 * base_team_same / max(base_n, 1), 100.0 * base_mode_same / max(base_n, 1), base_n))
        print(' mode value (by rank) counts: %s' % dict(sorted(modes.items())))
        print(' modes <5m: %s ; <15m: %s' % (dict(sorted(near5.items())), dict(sorted(near15.items()))))
        top = transitions.most_common(8)
        print(' per-driver mode transitions (top): %s' % ', '.join('%d>%d x%d' % (a, b, c) for (a, b), c in top))


if __name__ == '__main__':
    main()
