# d4_pace.py -- D4 (BATCH B item 2): live-log verification of the AIAbility 0x453C model.
#
# Static model (PRIOR_ART_MAP: IR L1828/L1875/L2409/L5981 + map_AIAbility.md L38-L71;
# code re-read for this script):
#   AIAbility 0x429470 is called by AIMainUpdate only when session time 0x4480 > deadline 0x4404,
#   then 0x4404 = now + 4.0 (4 s cadence). When RingBufferAccessWithWrap(0x4044) is 0/1 and
#   RacePhase 7 (race), the pace field 0x453C = this[4431] is updated in up to TWO writes:
#     (1) s4508 = 0x4508 == 0 -> v3 = max(pace - 0.01, 0x44F4)
#         s4508 != 0          -> v3 = min(0x44F4 + 0x44D8, pace + 0.01)
#     (2) if s450c = 0x450C != 0 (random gate):
#           ((0x44D4 - s450c*0.5) * 100 >= rand()%100)
#             -> v4 = min(0x44F4 + 0x44D8, v3 + 0.01)
#           else -> v4 = max(0x44F4 * 0.5, v3 - 0.01)
#     then 0x4508 = 0, 0x450C = 0.
#   carConditions == 1 branch: pace = 0x44F4 * 0.85 (skips the pace logic).
#   Outside AIAbility: LateralForceTorqueCalculation writes pace = a2 * 0x44F4 * 0.01
#   (called with a2 = 100 -> 0x44F4, the baseline reset), AIPushLap writes
#   pace = laps * 0.003 + pace (non-race), AIDriverStructInit initialises 1.0.
#   Because the 1 Hz overview samples are coarser than the 4 s cadence, a single observed
#   step can be one write (s450c == 0 throughout) or both writes (s450c != 0 right before the
#   call), so the reachable-value test below enumerates the one-write and two-write results.
#   Accumulators 0x4508/0x450C are cleared by AIAbility; a sample showing s450c != 0 proves the
#   coming step used the random gate, a sample showing 0 cannot prove the gate was off.
#
# Fields: 0x453C ai_pace_multiplier, 0x44F4 ability_baseline_scalar, 0x44D8 race_ability_push_scalar,
# 0x44D4 race_ability_norm_push (gate threshold), 0x4508/0x450C unnamed_4508/unnamed_450c,
# 0x4404 unnamed_4404 (next deadline), 0x3D6C car_conditions.
#
# Data: overview.jsonl 1 Hz, all AI cars. Monza watch_race4fullc + watch_race4fulld; wet Imola
# watch_20261002T194606Z. Slot 0 (player) is NOT logged by the watcher, so the handoff's
# "gap to slot 0" cannot be computed; nearest-opponent planar distance (opponents[0].dist_m)
# is used as the traffic-gap proxy. SC/RF mods were active in the Monza runs: no SC/RF marker
# exists in the logs; pace values written by mods (if any) would appear as model-unexplained.
#
# Read-only. Prints stats + the unexplained-step list.
import os
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as L  # noqa: E402

RUNS = {
    'monza': ('watch_race4fullc', 'watch_race4fulld'),
    'wet_imola': ('watch_20261002T194606Z',),
}
TOL = 3e-4


def q(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))]


def load(runs):
    cars = defaultdict(list)
    for run in runs:
        for r in L.overview(run):
            f = r['fields']
            opp = (r.get('opponents') or [{}])
            nearest = opp[0].get('dist_m') if opp and isinstance(opp[0], dict) else None
            cars[(run, r['slot'], r.get('driver'))].append(dict(
                t=r['sim_t'], lap=f.get('lap_number'), ld=f.get('lap_distance'),
                speed=f.get('speed'), pace=f.get('ai_pace_multiplier'),
                base=f.get('ability_baseline_scalar'), push=f.get('race_ability_push_scalar'),
                pnorm=f.get('race_ability_norm_push'), s08=f.get('unnamed_4508'),
                s0c=f.get('unnamed_450c'), dl=f.get('unnamed_4404'),
                cond=f.get('car_conditions'), pos=f.get('race_position'),
                nearest=nearest))
    for k in cars:
        cars[k].sort(key=lambda s: s['t'])
    return cars


def track_len(runs):
    cands = []
    for run in runs:
        prev = {}
        for r in L.overview(run):
            f = r['fields']
            drv = r.get('driver')
            ln, ld, sp = f.get('lap_number'), f.get('lap_distance'), f.get('speed')
            if ln is None or ld is None:
                continue
            if drv in prev:
                pln, pld, pt, psp = prev[drv]
                if ln == pln + 1 and pld is not None:
                    cands.append(pld + (psp or 0.0) * (r['sim_t'] - pt))
            prev[drv] = (ln, ld, r['sim_t'], sp)
    return q(cands, 0.5) if cands else None


def candidates(p, base, push, cond, lap, cond_next=None):
    """Reachable pace values for one AIAbility event (one or two writes + known externals)."""
    c = {}
    if (cond == 1 or cond_next == 1) and base is not None:
        c['cond85: 0.85*baseline'] = base * 0.85
    if base is None:
        return c
    c['reset: baseline'] = base
    d1 = max(p - 0.01, base)
    c['1w down toward baseline'] = d1
    u1 = min(base + push, p + 0.01) if push is not None else None
    if u1 is not None:
        c['1w up toward baseline+push'] = u1
        c['2w up then down(0.5b)'] = max(base * 0.5, u1 - 0.01)
        c['2w up then up'] = min(base + push, u1 + 0.01)
    c['2w down then down(0.5b)'] = max(base * 0.5, d1 - 0.01)
    if push is not None:
        c['2w down then up'] = min(base + push, d1 + 0.01)
    if lap is not None:
        c['pushlap +0.003*lap'] = p + 0.003 * lap
    return c


def match(n, cands):
    hits = [name for name, v in cands.items() if abs(n - v) <= TOL]
    return hits


def main():
    print('D4 pace multiplier 0x453C live verification -- %s' % os.path.basename(__file__))
    for label, runs in RUNS.items():
        tl = track_len(runs)
        cars = load(runs)
        print()
        print('=== %s (track_len est %.0f m) ===' % (label, tl or -1))
        total = 0
        hist = defaultdict(int)
        unexplained = []
        per_car = defaultdict(lambda: dict(n=0, ch=0, paces=[], ts=[]))
        for key, ss in sorted(cars.items()):
            run, slot, drv = key
            pc = per_car[(label, slot, drv)]
            for s in ss:
                pc['n'] += 1
                if s['pace'] is not None:
                    pc['paces'].append(s['pace'])
            prevs = None
            for s in ss:
                if prevs is not None and prevs['pace'] is not None and s['pace'] is not None \
                        and abs(s['pace'] - prevs['pace']) > 1e-9:
                    total += 1
                    pc['ch'] += 1
                    pc['ts'].append(s['t'])
                    cands = candidates(prevs['pace'], prevs['base'], prevs['push'],
                                       prevs['cond'], prevs['lap'], s['cond'])
                    hits = match(s['pace'], cands)
                    step = s['pace'] - prevs['pace']
                    if hits:
                        key_h = hits[0]
                        if len(hits) > 1:
                            key_h = min(hits, key=lambda h: abs(cands[h] - s['pace']))
                        hist[key_h] += 1
                    else:
                        hist['UNEXPLAINED'] += 1
                        unexplained.append((label, run, slot, drv, dict(prevs), s, dict(cands)))
                prevs = s
        print('cars=%d  pace-change events=%d  unexplained=%d (%.2f%%)'
              % (len(cars), total, hist.get('UNEXPLAINED', 0),
                 100.0 * hist.get('UNEXPLAINED', 0) / max(1, total)))
        for k in sorted(hist, key=lambda k: -hist[k]):
            print('  %-34s %5d  (%.1f%%)' % (k, hist[k], 100.0 * hist[k] / max(1, total)))
        # per-car cadence and range
        print('per-car: pace min/med/max, changes, median interval between changes (s)')
        allpace = []
        for key in sorted(per_car):
            pc = per_car[key]
            if not pc['paces']:
                continue
            paces = sorted(pc['paces'])
            allpace += paces
            ts = sorted(set(round(t, 2) for t in pc['ts']))
            dts = [b - a for a, b in zip(ts, ts[1:]) if 0.5 < b - a < 20]
            print('  %-30s %.4f / %.4f / %.4f  ch=%-4d med dt=%s'
                  % (('%s s%d' % (key[2] or '?', key[1]))[:30], paces[0], q(paces, 0.5), paces[-1],
                     pc['ch'], ('%.1f' % q(dts, 0.5)) if dts else 'n/a'))
        if allpace:
            aps = sorted(allpace)
            print('  ALL cars: min %.4f med %.4f max %.4f n=%d' % (aps[0], q(aps, 0.5), aps[-1], len(aps)))
        steps = []
        for key, ss in cars.items():
            for p, s in zip(ss, ss[1:]):
                if p['pace'] is not None and s['pace'] is not None and abs(s['pace'] - p['pace']) > 1e-9:
                    steps.append(abs(s['pace'] - p['pace']))
        if steps:
            print('  |step|: n=%d med=%.5f p90=%.5f max=%.5f; >0.0201: %d; >0.0101: %d'
                  % (len(steps), q(steps, 0.5), q(steps, 0.9), max(steps),
                     sum(1 for x in steps if x > 0.0201), sum(1 for x in steps if x > 0.0101)))
        # deadline cadence
        dl_ok = dl_n = 0
        for key, ss in cars.items():
            for s in ss:
                if s['dl'] is not None:
                    dl_n += 1
                    if 0.0 <= s['dl'] - s['t'] <= 4.05:
                        dl_ok += 1
        print('  deadline 0x4404: %d/%d samples with 0 <= deadline-sim_t <= 4.05' % (dl_ok, dl_n))
        # rubber-band / correlation
        by_t = defaultdict(list)
        for key, ss in cars.items():
            for s in ss:
                if tl and s['lap'] is not None and s['ld'] is not None:
                    s['prog'] = s['lap'] * tl + s['ld']
                by_t[round(s['t'] * 2) / 2].append(s)
        pairs_gap, pairs_near, pairs_pos, pairs_lap = [], [], [], []
        for key, ss in cars.items():
            for s in ss:
                if s.get('prog') is None or s['pace'] is None:
                    continue
                same = by_t.get(round(s['t'] * 2) / 2, [])
                lead = max((x.get('prog', -1e18) for x in same), default=None)
                if lead is not None and lead > -1e17:
                    pairs_gap.append((lead - s['prog'], s['pace']))
                if s['nearest'] is not None:
                    pairs_near.append((s['nearest'], s['pace']))
                if s['pos'] is not None:
                    pairs_pos.append((s['pos'], s['pace']))
                if s['lap'] is not None:
                    pairs_lap.append((s['lap'], s['pace']))
        print('  rubber-band / correlation with 0x453C (all AI samples):')
        for name, pairs in (('gap-to-leader (m)', pairs_gap), ('nearest-opponent distance (m)', pairs_near),
                            ('race position', pairs_pos), ('lap number', pairs_lap)):
            r = L.corr([a for a, _ in pairs], [b for _, b in pairs])
            print('    %-28s r=%s (n=%d)' % (name, ('%.3f' % r) if r is not None else 'n/a', len(pairs)))
        print('    [slot-0 gap NOT logged: player is not in overview.jsonl; nearest-opponent proxy used]')
        for fld in ('s08', 's0c'):
            on = [s['pace'] for key, ss in cars.items() for s in ss if s['pace'] is not None and s[fld] not in (None, 0.0)]
            off = [s['pace'] for key, ss in cars.items() for s in ss if s['pace'] is not None and s[fld] == 0.0]
            if on and off:
                print('    pace when %s!=0: med %.4f (n=%d) vs ==0: med %.4f (n=%d)'
                      % (fld, q(on, 0.5), len(on), q(off, 0.5), len(off)))
        near = [p for d, p in pairs_near if d < 15]
        free = [p for d, p in pairs_near if d > 100]
        if near and free:
            print('    pace in traffic (<15 m): med %.4f (n=%d) vs free air (>100 m): med %.4f (n=%d)'
                  % (q(near, 0.5), len(near), q(free, 0.5), len(free)))
        # unexplained list
        print('  unexplained events (cap 30 of %d):' % len(unexplained))
        for label_r, run, slot, drv, p, s, cands in unexplained[:30]:
            print('    %s s%-2d %-22s t=%7.1f pace %.4f->%.4f base %.4f push %.4f '
                  's4508 %.0f s450c %.3f cond %s lap %s pnorm %.3f nearest %s'
                  % (run[-1], slot, (drv or '?')[:22], s['t'], p['pace'], s['pace'], p['base'],
                     p['push'], p['s08'] or 0, p['s0c'] or 0, s['cond'], s['lap'], s['pnorm'],
                     ('%.0f' % s['nearest']) if s['nearest'] is not None else 'n/a'))
        if len(unexplained) > 30:
            print('    ... %d more' % (len(unexplained) - 30))
        # clamp-jump context
        big = [(key, p, s) for key, ss in cars.items() for p, s in zip(ss, ss[1:])
               if p['pace'] is not None and s['pace'] is not None and abs(s['pace'] - p['pace']) > 0.0201]
        big_ctx = defaultdict(int)
        for key, p, s in big:
            v = s['pace']
            if s['base'] is not None and abs(v - s['base'] * 0.85) <= TOL:
                big_ctx['cond85 (0.85*baseline)'] += 1
            elif s['base'] is not None and abs(v - s['base']) <= TOL:
                big_ctx['baseline reset (LateralForceTorqueCalculation)'] += 1
            elif abs(v - 1.0) <= TOL:
                big_ctx['1.0 (AIDriverStructInit / pre-race)'] += 1
            else:
                big_ctx['other'] += 1
        print('  |step| > 0.0201 (%d): %s' % (len(big), dict(big_ctx)))


if __name__ == '__main__':
    main()
