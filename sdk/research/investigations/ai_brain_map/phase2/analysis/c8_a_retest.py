# c8_a_retest.py — C8.3: re-run the braking-loss formula with the corrected step
#   v -= seg_len/v * 0x44CC * TireForce(v, wp+116, wp+120, wp+112) * 0.915
# on the existing logs. TireForce's vehicle tables are NOT logged (see RECORDER_REQUESTS
# A5), so the true meas/pred cannot be computed; this script reports what the logs can
# still show: the TireForce value implied by the profile per step and its v^2 shape.
import math
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L

FACTOR = 0.915
RUNS = ('watch_race3',)


def steps(run):
    for x in L.situations(run):
        if x['kind'] != 'brake_zone':
            continue
        for s in L.snaps(run, x['folder']):
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
            dec = f['brake_decel_factor']
            for j in range(i1, k):
                seg = wps[j + 1]['seg_len']
                v0, v1 = prof[j - 1], prof[j]
                if seg <= 0.1 or v0 <= 5 or v1 >= v0:
                    continue
                F = (v0 - v1) * v0 / (seg * dec * FACTOR)
                pos = 'first' if j == i1 else ('last' if j == k - 1 else 'mid')
                yield s['driver'], pos, v0, F, wps[j + 1]


def fit_quad(pts):
    # least squares F = A*v^2 + B (2x2 normal equations)
    n = len(pts)
    if n < 10:
        return None
    sx2 = sum(v * v for v, _ in pts)
    sx4 = sum(v ** 4 for v, _ in pts)
    sy = sum(F for _, F in pts)
    sxy = sum(v * v * F for v, F in pts)
    det = n * sx4 - sx2 * sx2
    if abs(det) < 1e-9:
        return None
    B = (sx4 * sy - sx2 * sxy) / det
    A = (n * sxy - sx2 * sy) / det
    my = sy / n
    ss_tot = sum((F - my) ** 2 for _, F in pts)
    ss_res = sum((F - (A * v * v + B)) ** 2 for v, F in pts)
    r2 = 1 - ss_res / ss_tot if ss_tot > 1e-9 else 0.0
    return A, B, r2


def main():
    for run in RUNS:
        by_pos = defaultdict(list)
        by_drv = defaultdict(list)
        n = 0
        for drv, pos, v0, F, wp in steps(run):
            by_pos[pos].append(F)
            by_drv[drv].append((v0, F))
            n += 1
        print('== C8 A re-test [%s] steps with implied TireForce: %d ==' % (run, n))
        for pos in ('first', 'mid', 'last'):
            v = sorted(by_pos[pos])
            if v:
                print(' implied TireForce %-5s: n=%d p10=%.2f med=%.2f p90=%.2f' %
                      (pos, len(v), v[len(v) // 10], v[len(v) // 2], v[int(0.9 * len(v))]))
        r2s, As = [], []
        for drv, pts in by_drv.items():
            r = fit_quad(pts)
            if r:
                r2s.append(r[2])
                As.append(r[0])
        if r2s:
            r2s.sort()
            As.sort()
            print(' per-driver fit F ~ A*v^2 + B: drivers=%d median R2=%.3f p10=%.3f p90=%.3f; median A=%.3f'
                  % (len(r2s), r2s[len(r2s) // 2], r2s[len(r2s) // 10], r2s[int(0.9 * len(r2s))], As[len(As) // 2]))
        print(' meas/pred with tireforce.py: NOT COMPUTABLE - vehicle tire tables (veh+0x3520..0x3D50)')
        print(' and wp+124/wp+128 are not logged; listed in RECORDER_REQUESTS A5. race2b skipped (old schema).')


if __name__ == '__main__':
    main()
