# d7_start.py -- D7: start reaction static tables + live race_start analysis.
#
# Static (AIMainUpdate 0x424E70 L96-L131): during the start phase the AI alternates the
# 0x4434 throttle candidate through the 26-entry ramp at 0x5C30E0 (0.0 .. 0.949); the read is
# `dword_5C3144[CRTFtol(v17 * -25.0)]`, i.e. dword_5C3144[-25..0] = the SAME 26 values read
# backwards (v17 = min(session_time - last_toggle, 1.0)); the branch flag this+17768 picks the
# direction. The next toggle time is `db + 1.0 + rand%100*0.02` or `db + rand%100*0.01` (0-2 s),
# so a driver's launch is a random sequence of 1-2 s ramp up/down phases.
# Live: race_start situations of watch_race2 (Imola) and watch_race4fullc (Monza):
# per driver time from the first car moving (>1 m/s) to own cmd_throttle>0 and own speed>1 m/s,
# and the 0x44B8 throttle_cap_race_start profile.
import json
import os
import struct
import sys
from collections import defaultdict
from pathlib import Path

import pefile

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
EXE = Path(r'%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as L  # noqa: E402

RUNS = ('watch_race2', 'watch_race4fullc')


def q(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))] if xs else None


def static_tables():
    pe = pefile.PE(str(EXE), fast_load=True)
    data = EXE.read_bytes()

    def off(va):
        return pe.get_offset_from_rva(va - pe.OPTIONAL_HEADER.ImageBase)

    def f32(va):
        return struct.unpack('<f', data[off(va):off(va) + 4])[0]
    ramp = [round(f32(0x5C30E0 + 4 * i), 4) for i in range(26)]
    print('== static: 26-entry start throttle ramp at 0x5C30E0 (read both directions) ==')
    print('  %s' % ramp)
    print('  monotone=%s  min=%.4f max=%.4f' %
          (all(ramp[i] <= ramp[i + 1] for i in range(25)), min(ramp), max(ramp)))
    return ramp


def live(run):
    series = defaultdict(list)
    for x in L.situations(run):
        if x['kind'] != 'race_start':
            continue
        for s in L.snaps(run, x['folder']):
            f = s['fields']
            series[s.get('driver') or s.get('slot')].append((
                s['sim_t'], f.get('speed'), f.get('cmd_throttle'), f.get('act_throttle'),
                f.get('throttle_cap_race_start'), f.get('cmd_brake'), f.get('lap_number')))
    for k in series:
        series[k].sort(key=lambda t: t[0])
    speeds = [v[1] for sl in series.values() for v in sl if v[1] is not None]
    t0 = None
    for sl in series.values():
        for v in sl:
            if v[1] is not None and v[1] > 1.0:
                t0 = v[0] if t0 is None else min(t0, v[0])
    print()
    print('== live [%s] race_start: %d drivers, first car >1 m/s at sim %.2f ==' % (run, len(series), t0 or -1))
    if t0 is None:
        return
    rows = []
    for drv, sl in series.items():
        t_thr = next((v[0] for v in sl if (v[2] or 0) > 0.01), None)
        t_act = next((v[0] for v in sl if (v[3] or 0) > 0.01), None)
        t_spd = next((v[0] for v in sl if (v[1] or 0) > 1.0), None)
        caps = [v[4] for v in sl if v[4] is not None]
        cap0 = next((v[4] for v in sl if v[4] is not None), None)
        cap2 = next((v[4] for v in sl if v[4] is not None and t0 and v[0] >= t0 + 2.0), None)
        rows.append(dict(drv=drv, thr=(t_thr - t0) if t_thr else None,
                         act=(t_act - t0) if t_act else None,
                         spd=(t_spd - t0) if t_spd else None,
                         cap0=cap0, capmin=min(caps) if caps else None,
                         capmax=max(caps) if caps else None, cap2=cap2, n=len(sl)))
    rows.sort(key=lambda r: (r['spd'] if r['spd'] is not None else 999))
    print('  driver                    thr-t0  act-t0  spd>1-t0  cap first/min/max/at+2s')
    for r in rows:
        print('  %-24s %7s %7s %9s  %6s / %6s / %6s / %6s'
              % ((r['drv'] or '?')[:24],
                 ('%.2f' % r['thr']) if r['thr'] is not None else 'n/a',
                 ('%.2f' % r['act']) if r['act'] is not None else 'n/a',
                 ('%.2f' % r['spd']) if r['spd'] is not None else 'n/a',
                 r['cap0'], r['capmin'], r['capmax'], r['cap2']))
    sp = [r['spd'] for r in rows if r['spd'] is not None]
    thr = [r['thr'] for r in rows if r['thr'] is not None]
    caps = [r['cap0'] for r in rows if r['cap0'] is not None]
    if sp:
        print('  reaction (time to >1 m/s): n=%d min=%.2f med=%.2f p90=%.2f max=%.2f'
              % (len(sp), min(sp), q(sp, 0.5), q(sp, 0.9), max(sp)))
    if thr:
        print('  throttle onset:            n=%d min=%.2f med=%.2f p90=%.2f max=%.2f'
              % (len(thr), min(thr), q(thr, 0.5), q(thr, 0.9), max(thr)))
    if caps:
        print('  throttle_cap_race_start at first sample: n=%d min=%.3f med=%.3f p90=%.3f max=%.3f'
              % (len(caps), min(caps), q(caps, 0.5), q(caps, 0.9), max(caps)))
    # 0-6 s cap profile after t0 (1 s bins, all drivers)
    prof = defaultdict(list)
    for sl in series.values():
        for v in sl:
            if v[4] is not None and t0 - 1.0 <= v[0] <= t0 + 8.0:
                prof[int(v[0] - t0)].append(v[4])
    print('  cap profile (bin s: median): %s'
          % {k: round(q(v, 0.5), 3) for k, v in sorted(prof.items())})


def main():
    static_tables()
    for run in RUNS:
        live(run)


if __name__ == '__main__':
    main()
