# c1b_e_trouble.py — Run E from C1b: spin/stuck watchdog and recovery modes from trouble/retire.
# Read-only over %TEMP% watch folders. Prints a short per-run result.
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L


def compress(seq):
    out = []
    for v in seq:
        if not out or out[-1][0] != v:
            out.append([v, 1])
        else:
            out[-1][1] += 1
    return '>'.join('%s x%d' % (a, b) for a, b in out)


def main():
    for run in L.RUNS:
        for kind in ('trouble', 'retire'):
            situ = [x for x in L.situations(run) if x['kind'] == kind]
            n_wd = n_rec = n_rev = n_stop = n_cc2 = 0
            max_wd = max_lim = 0
            rows = []
            for x in situ:
                seq = list(L.snaps(run, x['folder']))
                if not seq:
                    continue
                modes = [s['fields']['driver_status'] for s in seq]
                wd = [s['fields']['watchdog_counter'] for s in seq]
                lim = [s['fields']['watchdog_limit'] for s in seq]
                cc = [s['fields']['car_conditions'] for s in seq]
                sp = [s['fields']['speed'] for s in seq]
                n_wd += any(w > 0 for w in wd)
                n_rec += any(m == 1 for m in modes)
                n_rev += any(m == 6 for m in modes)
                n_stop += any(m == 7 for m in modes)
                n_cc2 += any(c == 2 for c in cc)
                max_wd = max(max_wd, max(wd))
                max_lim = max(max_lim, max(lim))
                rows.append((x['folder'], round(x['end'] - x['start'], 1),
                             compress([m for m in modes if m != 0] or [0]),
                             max(wd), max(lim), compress(cc) if len(set(cc)) > 1 else str(cc[0]),
                             round(min(sp) * 3.6), round(max(sp) * 3.6)))
            print('== Run E %s [%s] situations=%d ==' % (kind, run, len(situ)))
            print(' with watchdog>0: %d, mode1: %d, mode6: %d, mode7: %d, car_conditions==2: %d; max watchdog=%d limit=%d'
                  % (n_wd, n_rec, n_rev, n_stop, n_cc2, max_wd, max_lim))
            for r in rows[:8]:
                print('  %s dur=%.1fs modes=%s wd_max=%d lim=%d cc=%s speed %.0f-%.0f km/h' % r)


if __name__ == '__main__':
    main()
