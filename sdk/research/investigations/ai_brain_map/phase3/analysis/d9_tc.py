# d9_tc.py -- D9: traction-control / slip-cut activity in the Monza logs.
#
# Static (AISteeringCorrection 0x426CF0):
#   slipA = this[3633] (0x38C4), slipB = this[3571] (0x37CC); blend 0.3/0.7 (larger gets 0.7),
#   smoothed = (blend + slip_blend_last 0x4464) * 0.5; store back to 0x4464.
#   if smoothed < 0.9 and (speed 0x4440 > 4.0 or this+17516 != 0):
#       v8 = clamp(2 - 2.2222*smoothed, 0, 1)      # cut depth, 1.0 at smoothed <= 0.45
#       throttle 0x4434 = (SuspensionRatioClamp - throttle)*v8 + throttle
#       if (x87 cc set) and byte 0x39F9: brake_on_slip 0x4028 = clamp(4*(throttle_before-after), 0, 1)
#   The pre-correction throttle and SuspensionRatioClamp return are NOT logged, so the log can
#   measure the trigger frequency and the resulting throttle, not the exact cut magnitude.
#
# Data: watch_race4fullc + watch_race4fulld, all situations at 20 Hz (fields slip_blend_last,
# brake_on_slip, cmd_throttle, act_throttle, speed, gear, cmd_steer). Read-only.
import os
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as L  # noqa: E402

RUNS = ('watch_race4fullc', 'watch_race4fulld')


def q(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))] if xs else None


def main():
    n = trig = 0
    by_bin = defaultdict(lambda: [0, 0])
    by_gear = defaultdict(lambda: [0, 0])
    thr_trig, thr_free = [], []
    bos = 0
    bos_gear = defaultdict(int)
    steer_lift = 0
    cut_low = 0
    for run in RUNS:
        for x in L.situations(run):
            for s in L.snaps(run, x['folder']):
                f = s['fields']
                slip = f.get('slip_blend_last')
                sp = f.get('speed')
                if slip is None or sp is None:
                    continue
                n += 1
                fired = slip < 0.9 and sp > 4.0
                if fired:
                    trig += 1
                b = int(min(sp // 10, 9)) * 10
                by_bin[b][0] += 1
                by_bin[b][1] += 1 if fired else 0
                g = f.get('gear')
                if isinstance(g, int):
                    by_gear[g][0] += 1
                    by_gear[g][1] += 1 if fired else 0
                th = f.get('cmd_throttle')
                if th is not None:
                    (thr_trig if fired else thr_free).append(th)
                    if fired and th <= 0.01:
                        cut_low += 1
                bv = f.get('brake_on_slip')
                if bv:
                    bos += 1
                    if isinstance(g, int):
                        bos_gear[g] += 1
                st = f.get('cmd_steer')
                if st is not None and abs(st) > 0.1 and sp > 20.0:
                    steer_lift += 1
    print('== D9 traction control (Monza, 20 Hz situations) ==')
    print('samples=%d  trigger (smoothed slip<0.9 & speed>4)=%d (%.2f%%)' % (n, trig, 100.0 * trig / max(1, n)))
    print('  brake_on_slip!=0: %d (%.3f%%)  gears=%s' % (bos, 100.0 * bos / max(1, n), dict(bos_gear)))
    print('  cmd_throttle<=0.01 while trigger: %d (%.1f%% of trigger)' % (cut_low, 100.0 * cut_low / max(1, trig)))
    print('  steer-lift condition (|steer|>0.1 & speed>20): %d (%.1f%%)' % (steer_lift, 100.0 * steer_lift / max(1, n)))
    print('  trigger share by speed bin:')
    for b in sorted(by_bin):
        t, f = by_bin[b]
        print('    %3d+ m/s n=%-6d trigger %5.1f%%' % (b, t, 100.0 * f / t))
    print('  trigger share by gear:')
    for g in sorted(by_gear):
        t, f = by_gear[g]
        if t > 50:
            print('    gear %3d n=%-6d trigger %5.1f%%' % (g, t, 100.0 * f / t))
    if thr_trig and thr_free:
        print('  cmd_throttle: trigger med=%.3f p10=%.3f p90=%.3f | non-trigger med=%.3f p10=%.3f p90=%.3f'
              % (q(thr_trig, 0.5), q(thr_trig, 0.1), q(thr_trig, 0.9),
                 q(thr_free, 0.5), q(thr_free, 0.1), q(thr_free, 0.9)))
        print('  trigger share among low-throttle samples: ')
        for lo, hi in ((0, 0.01), (0.01, 0.2), (0.2, 0.5), (0.5, 0.8), (0.8, 1.01)):
            xs = [1 for t in thr_trig if lo <= t < hi]
            ys = [1 for t in thr_free if lo <= t < hi]
            print('    throttle %4.2f-%4.2f: trigger %d / total %d' % (lo, hi, len(xs), len(xs) + len(ys)))


if __name__ == '__main__':
    main()
