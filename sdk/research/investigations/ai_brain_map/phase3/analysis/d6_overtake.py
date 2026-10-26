# d6_overtake.py -- D6 live: opponent mode transitions in battle situations (all 6 race folders).
#
# For every opp_mode transition inside battle situations: from/to mode, nearest-opponent context
# (distance, its mode/state), car speed/cmd, lateral fields (0x4420 request, 0x440C working,
# 0x4408 published), interaction_target 0x45FC presence, closing speed, and the outcome
# (did the battle car pass its nearest rival within 10 s, judged from overview race_position).
# Outputs an aggregate table per mode and a transition matrix. Read-only.
import json
import os
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as L  # noqa: E402

RUNS = ('watch_race2', 'watch_race2b', 'watch_race3', 'watch_race4fullc',
        'watch_race4fulld', 'watch_20261002T194606Z')


def q(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))] if xs else None


def pos_timeline(run):
    """driver -> sorted [(t, race_position)] from overview 1 Hz."""
    out = defaultdict(list)
    for r in L.overview(run):
        p = r['fields'].get('race_position')
        if p is not None:
            out[r.get('driver')].append((r['sim_t'], p))
    for d in out:
        out[d].sort()
    return out


def nearest_at(tl, t):
    best = None
    for ts, p in tl:
        if abs(ts - t) < (abs(best[0] - t) if best else 1e9):
            best = (ts, p)
    return best


def main():
    events = []
    for run in RUNS:
        tl = pos_timeline(run)
        for x in L.situations(run):
            if x['kind'] != 'battle':
                continue
            snaps = list(L.snaps(run, x['folder']))
            prev = None
            for s in snaps:
                f = s['fields']
                mlist = f.get('opp_mode')
                if not isinstance(mlist, list):
                    mlist = [mlist]
                ml = [m if not isinstance(m, (list, dict)) else None for m in mlist]
                opps = s.get('opponents') or []
                best = min((o for o in opps if o.get('dist_m') is not None),
                           key=lambda o: o['dist_m'], default=None)
                if prev is not None:
                    for k in range(min(len(ml), len(prev['ml']))):
                        if ml[k] == prev['ml'][k]:
                            continue
                        n = next((o for o in opps if o.get('rank') == k), None)
                        dist = n.get('dist_m') if n else (best.get('dist_m') if best else None)
                        closing = None
                        if n and prev['opp_drv'] == n.get('driver') and prev['dist'] is not None and dist is not None:
                            dts = s['sim_t'] - prev['t']
                            if dts > 1e-6:
                                closing = (prev['dist'] - dist) / dts
                        dt_change = None
                        if k < len(prev['pt']) and prev['pt'][k] is not None:
                            dt_change = s['sim_t'] - prev['pt'][k]
                        et = dict(run=run, driver=s.get('driver'), slot=s.get('slot'), t=s['sim_t'],
                                  idx=k, frm=prev['ml'][k], to=ml[k], dist=dist,
                                  dt=dt_change,
                                  opp=n.get('driver') if n else (best.get('driver') if best else None),
                                  opp_mode=n.get('mode') if n else None,
                                  opp_state=n.get('state') if n else None,
                                  f455c=f.get('unnamed_455c'), f4570=f.get('unnamed_4570'),
                                  f4574=f.get('unnamed_4574'), f4578=f.get('unnamed_4578'),
                                  f4579=f.get('unnamed_4579'),
                                  speed=f.get('speed'), brake=f.get('cmd_brake'),
                                  throttle=f.get('cmd_throttle'),
                                  lat_req=f.get('lateral_request'),
                                  working=f.get('working_lateral_target'),
                                  published=f.get('published_lateral_target'),
                                  interaction=bool(f.get('interaction_target')),
                                  closing=closing, lap=f.get('lap_number'))
                        # outcome: position swap within 10 s vs the nearest rival
                        oppdrv = et['opp']
                        if oppdrv in tl and s.get('driver') in tl:
                            a0 = nearest_at(tl[s['driver']], s['sim_t'])
                            b0 = nearest_at(tl[oppdrv], s['sim_t'])
                            a1 = nearest_at(tl[s['driver']], s['sim_t'] + 10.0)
                            b1 = nearest_at(tl[oppdrv], s['sim_t'] + 10.0)
                            if a0 and b0 and a1 and b1:
                                was_behind = a0[1] > b0[1]
                                now_ahead = a1[1] < b1[1]
                                et['outcome'] = ('pass' if was_behind and now_ahead else
                                                 'lost' if (not was_behind) and (not now_ahead) else 'none')
                        events.append(et)
                pt = prev['pt'] if prev else [None] * len(ml)
                if prev is not None:
                    for k in range(min(len(ml), len(prev['ml']))):
                        if ml[k] != prev['ml'][k]:
                            pt[k] = s['sim_t']
                prev = dict(ml=ml, t=s['sim_t'], pt=pt,
                            dist=(best.get('dist_m') if best else None),
                            opp_drv=(best.get('driver') if best else None))
    print('=== D6 battle opp_mode transitions (6 race folders) ===')
    print('total transitions: %d' % len(events))
    by = defaultdict(list)
    for e in events:
        by[e['to']].append(e)
    print('per mode (to): count | share | med dist m | med |lat_req| | interaction%% | closing med | outcomes pass/lost/none')
    for m in sorted(by, key=lambda k: -len(by[k])):
        es = by[m]
        d = [e['dist'] for e in es if e['dist'] is not None]
        lat = [abs(e['lat_req']) for e in es if e['lat_req'] is not None]
        cl = [e['closing'] for e in es if e['closing'] is not None]
        oc = defaultdict(int)
        for e in es:
            oc[e.get('outcome', 'n/a')] += 1
        print('  %-4s n=%-6d %5.1f%% | med %6.2f | med %5.2f | %5.1f%% | %+6.2f | %s'
              % (m, len(es), 100.0 * len(es) / len(events),
                 q(d, 0.5) if d else -1, q(lat, 0.5) if lat else -1,
                 100.0 * sum(1 for e in es if e['interaction']) / len(es),
                 q(cl, 0.5) if cl else 0.0,
                 {k: oc[k] for k in ('pass', 'lost', 'none', 'n/a') if oc.get(k)}))
    tm = defaultdict(int)
    for e in events:
        tm[(e['frm'], e['to'])] += 1
    print()
    print('top transitions (from -> to): count')
    for (a, b), c in sorted(tm.items(), key=lambda kv: -kv[1])[:20]:
        print('  %3s -> %-3s %d' % (a, b, c))
    print()
    print('per-mode transition interval + battle fields (0x455C/0x4570/0x4574/0x4578/0x4579):')
    print('  mode n     dt med(s)  455c med  nz%%: 455c 4570 4574 4578 4579')
    for m in sorted(by, key=lambda k: -len(by[k])):
        es = by[m]
        dts = [e['dt'] for e in es if e['dt'] is not None]
        f455c = [e['f455c'] for e in es if e['f455c'] is not None]
        nz = {nm: (100.0 * sum(1 for e in es if e[key] not in (None, 0)) / len(es))
              for nm, key in (('455c', 'f455c'), ('4570', 'f4570'), ('4574', 'f4574'),
                              ('4578', 'f4578'), ('4579', 'f4579'))}
        print('  %-4s %-5d %-10s %-9s %4.0f %4.0f %4.0f %4.0f %4.0f'
              % (m, len(es), ('%.2f' % q(dts, 0.5)) if dts else 'n/a',
                 q(f455c, 0.5) if f455c else 'n/a',
                 nz['455c'], nz['4570'], nz['4574'], nz['4578'], nz['4579']))
    print()
    print('mode 5/16/4 detail: median speed, brake, throttle; interaction; lap')
    for m in (4, 5, 16):
        es = by.get(m, [])
        if not es:
            continue
        sp = [e['speed'] for e in es if e['speed'] is not None]
        br = [e['brake'] for e in es if e['brake'] is not None]
        th = [e['throttle'] for e in es if e['throttle'] is not None]
        print('  mode %-3d n=%-5d speed med %.1f  brake med %.2f  throttle med %.2f  interaction %d/%d'
              % (m, len(es), q(sp, 0.5) or -1, q(br, 0.5) or -1, q(th, 0.5) or -1,
                 sum(1 for e in es if e['interaction']), len(es)))
    print()
    print('transition samples that set interaction_target (cap 15):')
    n = 0
    for e in events:
        if e['interaction']:
            n += 1
            if n <= 15:
                print('  %-12s t=%7.1f %3s->%-3s dist=%s opp=%-18s lat_req=%s work=%s pub=%s outcome=%s'
                      % (e['run'][:12], e['t'], e['frm'], e['to'],
                         ('%.1f' % e['dist']) if e['dist'] is not None else '?',
                         (e['opp'] or '?')[:18], e['lat_req'], e['working'], e['published'],
                         e.get('outcome', 'n/a')))
    print('  interaction transitions total: %d / %d' % (n, len(events)))
    # sign distribution of lateral request per mode
    print()
    print('lateral_request sign by mode (neg/zero/pos):')
    for m in sorted(by):
        neg = sum(1 for e in by[m] if e['lat_req'] is not None and e['lat_req'] < -0.01)
        pos = sum(1 for e in by[m] if e['lat_req'] is not None and e['lat_req'] > 0.01)
        zer = sum(1 for e in by[m] if e['lat_req'] is not None and abs(e['lat_req']) <= 0.01)
        print('  %-4s neg=%-5d zero=%-5d pos=%-5d' % (m, neg, zer, pos))


if __name__ == '__main__':
    main()
