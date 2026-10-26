# c1b2_b_steering.py — Run B on the second track (watch_race3, Monaco), brake_zone situations.
# Read-only over %TEMP% watch folders. Prints a short result.
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L

RUN = 'watch_race3'
BANDS = ((0, 15, 0), (15, 30, 1), (30, 65, 2), (65, 1e9, 3))


def main():
    situ = [x for x in L.situations(RUN) if x['kind'] == 'brake_zone']
    n = matches = 0
    errs, preds, cmds, locks = [], [], [], []
    idx_ok = 0
    by_band = {b[2]: [0, 0] for b in BANDS}
    mode6 = 0
    for x in situ:
        for s in L.snaps(RUN, x['folder']):
            d = s.get('derived', {}).get('steering_target')
            if not d:
                continue
            n += 1
            matches += bool(d['law_matches'])
            errs.append(abs(d['predicted_steer'] - d['steer_cmd']))
            preds.append(d['predicted_steer'])
            cmds.append(d['steer_cmd'])
            f = s['fields']
            locks.append(f['steering_lock_divisor'])
            want = next(b[2] for b in BANDS if b[0] <= f['speed'] < b[1])
            ok = f['steering_lookahead_index'] == want
            idx_ok += ok
            by_band[want][0] += ok
            by_band[want][1] += 1
            mode6 += f['driver_status'] == 6
    st = L.stats(errs)
    ls = L.stats(locks)
    print('== Run B steering Monaco [%s] brake_zone %d situations, samples=%d ==' % (RUN, len(situ), n))
    print(' law_matches %d/%d (%.1f%%), mode6 samples %d' % (matches, n, 100.0 * matches / n if n else 0, mode6))
    if st:
        print(' abs(pred-cmd): med=%.4f p90=%.4f max=%.4f; corr=%.4f' % (st['median'], st['p90'], st['max'], L.corr(preds, cmds) or -1))
    if ls:
        print(' lock divisor min=%.4f med=%.4f max=%.4f' % (ls['min'], ls['median'], ls['max']))
    print(' look-ahead idx vs speed rule: %d/%d (%.1f%%); per band %s'
          % (idx_ok, n, 100.0 * idx_ok / n if n else 0,
             ', '.join('idx%d %d/%d' % (b, v[0], v[1]) for b, v in sorted(by_band.items()))))


if __name__ == '__main__':
    main()
