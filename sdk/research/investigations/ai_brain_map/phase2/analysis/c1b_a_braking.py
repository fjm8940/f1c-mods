# c1b_a_braking.py — Run A from C1b: braking look-ahead and the projection formula.
# Read-only over %TEMP% watch folders. Prints a short per-run result.
#
# Checks (handoff C1 run A):
#  1. At braking start: braking_target_speed < speed, cmd_brake > 0, cmd_throttle == 0.
#  2. Profile 0x43A4..: profile[0] == speed, descending, trailing zeros, pad step 1.8.
#  3. Plan recurrence: profile[k-1] vs profile[k-2] - len/profile[k-2]*0x44CC*target*0.915
#     for the segment ending at the limiting waypoint (k = chain index of lim,
#     target = braking_target_speed, len = logged waypoint chord; wp+0x3C is not logged).
#  4. Physical braking: measured per-tick deceleration in the zone vs the formula-implied
#     deceleration 0x44CC*target*0.915.
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L

FACTOR = 0.915
PAD = 1.8


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


def main():
    for run in L.RUNS:
        situ = [x for x in L.situations(run) if x['kind'] == 'brake_zone']
        bs = dict(n=0, tgt_lt=0, brk=0, thr0=0)
        prof0, desc, zeros, n = [], 0, 0, 0
        plan, pad_err, decel = [], [], []
        zones_n = 0
        for x in situ:
            for z in zones(list(L.snaps(run, x['folder']))):
                if not z:
                    continue
                zones_n += 1
                z0 = z[0]
                for s in z:
                    f = s['fields']
                    prof = f['speed_profile']
                    n += 1
                    prof0.append(abs(prof[0] - f['speed']))
                    desc += all(prof[i + 1] <= prof[i] + 1e-4 for i in range(19))
                    zeros += prof[-1] <= 0.01
                    ch = L.chain(s)
                    lim = L.hx(f['limiting_waypoint_ptr'])
                    if lim not in ch:
                        continue
                    k = ch.index(lim)
                    if k < 2 or k > 5:
                        continue
                    ln = L.wp_dist(s, k - 1, k)
                    if ln is None or prof[k - 2] <= 0.5:
                        continue
                    pred = ln / prof[k - 2] * f['brake_decel_factor'] * f['braking_target_speed'] * FACTOR
                    meas = prof[k - 2] - prof[k - 1]
                    if pred > 0.05:
                        plan.append((meas, pred, f['brake_decel_factor'] * f['braking_target_speed'] * FACTOR))
                    if k < 19:
                        pad_err.append(abs(prof[k] - (prof[k - 1] - PAD)))
                f0 = z0['fields']
                bs['n'] += 1
                bs['tgt_lt'] += f0['braking_target_speed'] < f0['speed']
                bs['brk'] += f0['cmd_brake'] > 0
                bs['thr0'] += f0['cmd_throttle'] <= 0.01
                # physical decel: strongest consecutive pair inside the zone, > 0.03 s apart
                for a, b in zip(z, z[1:]):
                    dt = b['sim_t'] - a['sim_t']
                    dv = a['fields']['speed'] - b['fields']['speed']
                    if 0.03 <= dt <= 0.15 and dv > 0:
                        decel.append(dv / dt)
        print('== Run A/B braking [%s] brake_zone %d situations, %d zones ==' % (run, len(situ), zones_n))
        if bs['n']:
            print(' braking_start tgt<speed %d/%d, cmd_brake>0 %d/%d, cmd_throttle==0 %d/%d'
                  % (bs['tgt_lt'], bs['n'], bs['brk'], bs['n'], bs['thr0'], bs['n']))
        if prof0:
            s0 = L.stats(prof0)
            print(' |profile[0]-speed|: med=%.3f p90=%.3f max=%.3f m/s (n=%d)' % (s0['median'], s0['p90'], s0['max'], s0['n']))
        print(' profile descending %d/%d, trailing zeros %d/%d, pad err med=%.3f max=%.3f (n=%d)'
              % (desc, n, zeros, n, L.stats(pad_err)['median'] if pad_err else -1,
                 L.stats(pad_err)['max'] if pad_err else -1, len(pad_err)))
        if plan:
            ratios = sorted(m / p for m, p, _ in plan)
            print(' last pre-limiter step meas/pred: n=%d p10=%.2f med=%.2f p90=%.2f; frac<0.3=%.2f'
                  % (len(ratios), ratios[len(ratios) // 10], ratios[len(ratios) // 2],
                     ratios[int(0.9 * len(ratios))], sum(1 for v in ratios if v < 0.3) / len(ratios)))
            errs = L.stats([abs(m - p) for m, p, _ in plan])
            im = L.stats([i for _, _, i in plan])
            print('   abs(meas-pred): med=%.2f p90=%.2f m/s; formula-implied decel med=%.1f m/s^2'
                  % (errs['median'], errs['p90'], im['median']))
        if decel:
            ds = L.stats(decel)
            print(' physical decel in zones: n=%d med=%.1f p90=%.1f max=%.1f m/s^2' % (ds['n'], ds['median'], ds['p90'], ds['max']))
        else:
            print(' physical decel in zones: none measured')


if __name__ == '__main__':
    main()
