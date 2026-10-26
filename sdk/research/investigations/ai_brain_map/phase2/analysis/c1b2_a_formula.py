# c1b2_a_formula.py — Run A re-test on watch_race3 (Monaco) with seg_len (wp+0x3C) now logged.
# Read-only over %TEMP% watch folders. Prints a short result.
#
# Tests the documented formula loss = seg_len/profile[j-1] * 0x44CC * target * 0.915:
#  1. last pre-limiter step, target = logged 0x43FC (exact within-tick target)
#  2. implied factor per step position (target-like vs force-like)
#  3. hypothesis: the multiplication factor is the TireForce value derived from the logged
#     corner-speed constants (target = (F*coeff + offset) * scale * pace * speed_k)
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L

RUN = 'watch_race3'
FACTOR = 0.915


def main():
    last = []
    by_pos = defaultdict(list)
    tfs_test = []
    n_sit = n_active = 0
    for x in L.situations(RUN):
        if x['kind'] != 'brake_zone':
            continue
        n_sit += 1
        for s in L.snaps(RUN, x['folder']):
            if not L.active(s):
                continue
            f = s['fields']
            ch = L.chain(s)
            wps = s['waypoints']
            if len(wps) < 20:
                continue
            lim = L.hx(f['limiting_waypoint_ptr'])
            if lim not in ch:
                continue
            k = ch.index(lim)
            prof = f['speed_profile']
            i1 = next((j for j in range(1, 20) if prof[j] < f['speed'] - 1e-3), 20)
            if not (0 < i1 < k and k <= 19):
                continue
            n_active += 1
            dec = f['brake_decel_factor']
            for j in range(i1, k):
                seg = wps[j + 1]['seg_len']
                v0, v1 = prof[j - 1], prof[j]
                if seg <= 0.1 or v0 <= 0.5 or v1 >= v0:
                    continue
                fac = (v0 - v1) * v0 / (seg * dec * FACTOR)
                pos = 'first' if j == i1 else ('last' if j == k - 1 else 'mid')
                by_pos[pos].append(fac)
                if j == k - 1:
                    tgt = f['braking_target_speed']
                    pred = seg / v0 * dec * tgt * FACTOR
                    if pred > 0.05:
                        last.append((v0 - v1) / pred)
                    c1 = f['corner_speed_force_coeff']
                    c0 = f['corner_speed_force_offset']
                    c2 = f['corner_speed_scale']
                    c3 = f['ai_pace_multiplier']
                    sk = wps[k]['speed_k']
                    if c1 and c2 * c3 * sk and tgt < 9000:
                        tfs = ((tgt / (c2 * c3 * sk)) - c0) / c1
                        pred2 = seg / v0 * dec * FACTOR * tfs
                        if pred2 > 0.05:
                            tfs_test.append((v0 - v1) / pred2)
    def st(v):
        v = sorted(v)
        return (len(v), v[len(v) // 10], v[len(v) // 2], v[int(0.9 * len(v))]) if v else (0, 0, 0, 0)
    print('== Run A re-test [%s] %d brake_zone situations, %d active samples, 20-wp records ==' % (RUN, n_sit, n_active))
    if last:
        n, p10, med, p90 = st(last)
        print('last pre-limiter step, documented formula (seg_len, 0x43FC target): n=%d p10=%.3f med=%.3f p90=%.3f' % (n, p10, med, p90))
    for pos in ('first', 'mid', 'last'):
        n, p10, med, p90 = st(by_pos[pos])
        if n:
            print(' implied factor %-5s: n=%d p10=%.2f med=%.2f p90=%.2f (m/s)' % (pos, n, p10, med, p90))
    if tfs_test:
        n, p10, med, p90 = st(tfs_test)
        print(' last step vs TireForce-derived factor: n=%d p10=%.2f med=%.2f p90=%.2f' % (n, p10, med, p90))


if __name__ == '__main__':
    main()
