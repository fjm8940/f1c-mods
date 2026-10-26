# c1b4_monza.py — C1b A-E sweep for the Monza race (watch_race4fullc start + watch_race4fulld rest).
# Read-only over %TEMP% watch folders. The C9 TireForce formula re-test is c9_a_retest.py.
# Prints a short per-run result for A (braking), B (steering), C (battle/opponents),
# D (pit), E (trouble/retire).
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L

RUNS = ('watch_race4fullc', 'watch_race4fulld')
FACTOR = 0.915


def zones(snaps):
    out, cur = [], None
    for s in snaps:
        if L.active(s):
            if cur is None:
                cur = []
                out.append(cur)
            cur.append(s)
        else:
            cur = None
    return out


def compress(seq):
    out = []
    for v in seq:
        if not out or out[-1][0] != v:
            out.append([v, 1])
        else:
            out[-1][1] += 1
    return '>'.join('%s x%d' % (a, b) for a, b in out)


def main():
    for run in RUNS:
        kinds = Counter(x['kind'] for x in L.situations(run))
        print('== %s: situations %s ==' % (run, dict(kinds)))
        # A
        bs = dict(n=0, tgt=0, brk=0, thr=0)
        prof0, pad, decel = [], [], []
        for x in L.situations(run):
            if x['kind'] != 'brake_zone':
                continue
            for z in zones(list(L.snaps(run, x['folder']))):
                if not z:
                    continue
                f0 = z[0]['fields']
                bs['n'] += 1
                bs['tgt'] += f0['braking_target_speed'] < f0['speed']
                bs['brk'] += f0['cmd_brake'] > 0
                bs['thr'] += f0['cmd_throttle'] <= 0.01
                for a, b in zip(z, z[1:]):
                    dt = b['sim_t'] - a['sim_t']
                    dv = a['fields']['speed'] - b['fields']['speed']
                    if 0.03 <= dt <= 0.15 and dv > 0:
                        decel.append(dv / dt)
                for s in z:
                    f = s['fields']
                    prof0.append(abs(f['speed_profile'][0] - f['speed']))
                    ch = L.chain(s)
                    lim = L.hx(f['limiting_waypoint_ptr'])
                    if lim in ch and 1 < ch.index(lim) < 19:
                        k = ch.index(lim)
                        pad.append(abs(f['speed_profile'][k] - (f['speed_profile'][k - 1] - 1.8)))
        print(' A braking_start %d zones: tgt<speed %d, brake>0 %d, throttle0 %d; |prof0-speed| max %.3f; pad err max %.3f; decel med %.1f p90 %.1f'
              % (bs['n'], bs['tgt'], bs['brk'], bs['thr'], max(prof0 or [0]), max(pad or [0]),
                 L.stats(decel)['median'] if decel else -1, L.stats(decel)['p90'] if decel else -1))
        # B
        n = matches = idx_ok = mode6 = 0
        errs, preds, cmds = [], [], []
        for x in L.situations(run):
            if x['kind'] != 'brake_zone':
                continue
            for s in L.snaps(run, x['folder']):
                d = s.get('derived', {}).get('steering_target')
                if not d:
                    continue
                n += 1
                matches += bool(d['law_matches'])
                errs.append(abs(d['predicted_steer'] - d['steer_cmd']))
                preds.append(d['predicted_steer'])
                cmds.append(d['steer_cmd'])
                f = s['fields']
                sp = f['speed']
                want = 0 if sp < 15 else 1 if sp < 30 else 2 if sp < 65 else 3
                idx_ok += f['steering_lookahead_index'] == want
                mode6 += f['driver_status'] == 6
        st = L.stats(errs)
        print(' B steering %d samples: law %.1f%%, med err %.4f p90 %.4f corr %.3f, idx rule %.1f%%, mode6 %d'
              % (n, 100.0 * matches / max(n, 1), st['median'] if st else -1, st['p90'] if st else -1,
                 L.corr(preds, cmds) or -1, 100.0 * idx_ok / max(n, 1), mode6))
        # C
        mono = m = pair = pair_tot = rank_eq = 0
        modes = Counter()
        for x in L.situations(run):
            if x['kind'] != 'battle':
                continue
            for s in L.snaps(run, x['folder']):
                m += 1
                d = s['fields']['opp_slot_distance_sq']
                valid = [v for v in d if v < 999998.0]
                mono += valid == sorted(valid)
                ex = s.get('extra') or {}
                sri = ex.get('slot_record_index') or []
                for o in s.get('opponents', []):
                    k = o['rank']
                    if k < len(sri):
                        pair_tot += 1
                        pair += sri[k] == o.get('team_record')
                        rank_eq += sri[k] == k
                    if o.get('mode') is not None:
                        modes[o['mode']] += 1
        print(' C battle %d samples: nearest-first %.1f%%; slot_record_index==team match %d/%d, ==rank %d; modes %s'
              % (m, 100.0 * mono / max(m, 1), pair, pair_tot, rank_eq, dict(sorted(modes.items()))))
        # D
        real = clamp = 0
        seqs = Counter()
        cc2 = 0
        for x in L.situations(run):
            if x['kind'] != 'pit':
                continue
            modes = []
            r = False
            for s in L.snaps(run, x['folder']):
                f = s['fields']
                modes.append(f['driver_status'])
                r = r or f['driver_status'] in (3, 4, 5) or bool(f['call_to_box'])
                cc2 += f['car_conditions'] == 2
            real += bool(r)
            clamp += (not r)
            seqs[compress([v for v in modes if v != 0] or [0])] += 1
        print(' D pit %d: real %d clamp-only %d; sequences %s; car_conditions==2 samples %d'
              % (sum(seqs.values()), real, clamp, seqs.most_common(3), cc2))
        # E
        for kind in ('trouble', 'retire'):
            nw = rec = rev = stop = c2 = 0
            maxwd = 0
            for x in L.situations(run):
                if x['kind'] != kind:
                    continue
                wd = []
                for s in L.snaps(run, x['folder']):
                    f = s['fields']
                    wd.append(f['watchdog_counter'])
                    rec += f['driver_status'] == 1
                    rev += f['driver_status'] == 6
                    stop += f['driver_status'] == 7
                    c2 += f['car_conditions'] == 2
                nw += any(w > 0 for w in wd)
                maxwd = max(maxwd, max(wd or [0]))
            print(' E %s: situations %d, wd>0 %d max %d, mode1 hits %d, mode6 hits %d, mode7 hits %d, cc2 hits %d'
                  % (kind, sum(1 for x in L.situations(run) if x['kind'] == kind), nw, maxwd, rec, rev, stop, c2))


if __name__ == '__main__':
    main()
