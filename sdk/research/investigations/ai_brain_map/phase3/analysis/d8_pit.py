# d8_pit.py -- D8 live: pit situations in watch_race2b (Imola) + watch_race4fulld (Monza).
#
# Per pit situation: mode (driver_status) sequence, call_to_box, speed_limit, pit plan
# (count/cursor), next_chosen_tires, unnamed_4544, time_of_next_exit 0x4534,
# track_part_from_pit_exit, lap/speed. Aggregates mode coverage, call-to-box edges,
# limiter values, cursor and compound changes. Read-only.
import json
import os
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as L  # noqa: E402

RUNS = ('watch_race2b', 'watch_race4fulld')


def q(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))] if xs else None


def main():
    mode_seen = defaultdict(int)
    cb_edges = []
    limiter = defaultdict(list)
    cursors = []
    tires = defaultdict(int)
    plan_counts = defaultdict(int)
    per_run = defaultdict(list)
    for run in RUNS:
        for x in L.situations(run):
            if x['kind'] != 'pit':
                continue
            snaps = list(L.snaps(run, x['folder']))
            modes = [s['fields'].get('driver_status') for s in snaps]
            for m in set(modes):
                mode_seen[(run, m)] += 1
            # call_to_box edges
            prev = None
            for s in snaps:
                f = s['fields']
                cb = f.get('call_to_box')
                if prev is not None and cb != prev:
                    cb_edges.append((run, s.get('driver'), s['sim_t'], prev, cb,
                                     f.get('driver_status')))
                prev = cb
            # limiter held maxima
            sl = [f['speed_limit'] for f in (s['fields'] for s in snaps)
                  if f.get('speed_limit') is not None and f['speed_limit'] < 1000.0]
            if sl:
                limiter[run] += sl
            # plan/cursor/compound
            c0 = snaps[0]['fields']
            c1 = snaps[-1]['fields']
            cursors.append((run, x.get('driver'), x['start'], c0.get('pit_plan_cursor'),
                            c1.get('pit_plan_cursor'), c0.get('pit_plan_count'),
                            c0.get('next_chosen_tires'), c1.get('next_chosen_tires')))
            for s in snaps:
                f = s['fields']
                if f.get('next_chosen_tires') is not None:
                    tires[(run, f['next_chosen_tires'])] += 1
                if f.get('pit_plan_count') is not None:
                    plan_counts[(run, f['pit_plan_count'])] += 1
            per_run[run].append(dict(
                driver=x.get('driver'), start=x['start'], end=x['end'],
                modes=sorted(set(m for m in modes if m is not None)),
                cb_max=max((s['fields'].get('call_to_box') or 0) for s in snaps),
                lim_med=q(sl, 0.5) if sl else None,
                cursor0=c0.get('pit_plan_cursor'), cursor1=c1.get('pit_plan_cursor'),
                count=c0.get('pit_plan_count'), tires=(c0.get('next_chosen_tires'), c1.get('next_chosen_tires')),
                u4544=sorted(set(s['fields'].get('unnamed_4544') for s in snaps
                                 if s['fields'].get('unnamed_4544') is not None)),
                t2e=[round(s['fields'].get('time_of_next_exit'), 1) for s in snaps[:1]]))
    print('=== D8 pit situations ===')
    for run in RUNS:
        print()
        print('-- %s: %d pit situations' % (run, len(per_run[run])))
        for p in per_run[run][:10]:
            print('  %-22s %7.1f-%7.1f modes=%s cb=%s lim=%s cursor %s->%s count=%s tires %s u4544=%s'
                  % ((p['driver'] or '?')[:22], p['start'], p['end'], p['modes'], p['cb_max'],
                     ('%.2f' % p['lim_med']) if p['lim_med'] else 'n/a',
                     p['cursor0'], p['cursor1'], p['count'], p['tires'], p['u4544']))
        if len(per_run[run]) > 10:
            print('  ... %d more' % (len(per_run[run]) - 10))
    print()
    print('mode coverage per run (situations containing the mode):')
    for k in sorted(mode_seen, key=lambda k: (k[0], -1 if k[1] is None else k[1])):
        print('  %-22s mode %-4s %d situations' % (k[0], k[1], mode_seen[k]))
    print()
    print('call_to_box edges (run, driver, t, from->to, mode):')
    for e in cb_edges[:30]:
        print('  %-20s %-20s t=%7.1f %s->%s mode=%s' % (e[0], (e[1] or '?')[:20], e[2], e[3], e[4], e[5]))
    print('  total call_to_box edges: %d' % len(cb_edges))
    print()
    print('speed_limit held values (m/s / km/h):')
    for run in RUNS:
        v = limiter[run]
        if v:
            print('  %-20s n=%-5d distinct=%s  med=%.2f (%.1f km/h)'
                  % (run, len(v), sorted(set(round(x, 2) for x in v))[:6], q(v, 0.5), q(v, 0.5) * 3.6))
    print()
    print('plan cursor transitions (0 -> N) by driver:')
    for c in cursors:
        if c[3] != c[4]:
            print('  %-20s %-20s %7.1f  cursor %s->%s count=%s tires %s->%s'
                  % (c[0], (c[1] or '?')[:20], c[2], c[3], c[4], c[5], c[6], c[7]))
    print()
    print('next_chosen_tires values seen (samples):')
    for k in sorted(tires, key=lambda k: -tires[k])[:12]:
        print('  %-20s value %-6s %d samples' % (k[0], k[1], tires[k]))
    print('pit_plan_count values seen:')
    for k in sorted(plan_counts, key=lambda k: -plan_counts[k])[:8]:
        print('  %-20s count %-4s %d samples' % (k[0], k[1], plan_counts[k]))


if __name__ == '__main__':
    main()
