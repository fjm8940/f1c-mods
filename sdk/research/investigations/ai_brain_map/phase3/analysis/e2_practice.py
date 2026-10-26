# e2_practice.py -- E2: practice-session analysis (watch_20261002T201617Z, wet Imola, finished).
#
# Read-only. Reuses phase2/analysis/c1b_lib.py (import only, no edit). The watcher ran into the
# following qualifying session (sim_t resets to 0 at line 17594, practice sim_t 946..1886 s), so the
# practice segment is cut there.
# Reports:
#  1. garage exit/entry driver_status (0x4510) sequences: run durations, speeds, pit position
#     (track_part_from_pit_exit 0x4450, logged as int bits of a float).
#  2. lap times (lap_number 0x403C increments, 1 Hz) split out-lap / garage lap / flying lap, and
#     ai_pace_multiplier (0x453C) vs the AIPushLap non-race ramp model (target 0x44F4+0x44D8,
#     step +0.003*min(lap,10)); park (mode 5) reset to 1.0 noted against D4's Monza outliers.
#  3. pit-lane speed clamp: speed_limit 0x4484 non-sentinel samples and speed vs the 22.22 limit.
#  4. interaction_target 0x45FC nonzero samples.
#  5. unnamed_4554 raw values (v1.2 log: float 1.0).
import json
import statistics
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as lib  # noqa: E402

RUN = 'watch_20261002T201617Z'
F = ('speed', 'driver_status', 'lap_number', 'lap_distance', 'ai_pace_multiplier',
     'ability_baseline_scalar', 'race_ability_push_scalar', 'speed_limit', 'call_to_box',
     'interaction_target', 'best_lap', 'track_part_from_pit_exit', 'on_track', 'lateral_position',
     'unnamed_4554', 'car_conditions')


def fb(v):
    return struct.unpack('<f', struct.pack('<I', int(v)))[0] if v is not None else None


def pct(xs, q):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(q * len(xs)))] if xs else None


def runs_of(rows, key):
    out = []
    cur = None
    start = None
    for r in rows:
        v = r[key]
        if v != cur:
            if cur is not None:
                out.append((cur, start, r['sim_t'] - start))
            cur = v
            start = r['sim_t']
    if cur is not None and rows:
        out.append((cur, start, rows[-1]['sim_t'] - start))
    return out


def main():
    cars = defaultdict(list)
    n = 0
    prev_t = None
    cut = 0
    for s in lib.overview(RUN):
        n += 1
        if prev_t is not None and s['sim_t'] < prev_t - 1:
            cut = n
            break
        prev_t = s['sim_t']
        f = s['fields']
        row = {k: f.get(k) for k in F}
        row['sim_t'] = s['sim_t']
        row['slot'] = s.get('slot')
        cars[s['driver']].append(row)
    total = n
    print('== practice segment: %d samples total, cut at %d (sim_t reset into qualifying) =='
          % (total, cut))
    print('== %d drivers ==' % len(cars))

    print()
    print('== 1. driver_status (0x4510) runs per car ==')
    for drv, rows in sorted(cars.items()):
        rr = runs_of(rows, 'driver_status')
        if all(st == 0 for st, _, _ in rr):
            continue
        parts = ['%s@%.0fs(%.0fs)' % (st, t0, dur) for st, t0, dur in rr]
        print('%-22s %s' % (drv, ' '.join(parts)))

    print()
    print('== 1b. mode-3/5 garage position (track_part 0x4450 bits->float, lap_distance) ==')
    for drv in sorted(cars):
        rows = [r for r in cars[drv] if r['driver_status'] in (3, 5)]
        if not rows:
            continue
        tp = sorted({round(fb(r['track_part_from_pit_exit']), 1) for r in rows
                     if r['track_part_from_pit_exit'] is not None})
        ld = [r['lap_distance'] for r in rows if r['lap_distance'] is not None]
        sp = [r['speed'] for r in rows]
        print('%-22s n=%4d parts=%s lap_dist med %.0f | speed med %.1f max %.1f' %
              (drv, len(rows), tp, statistics.median(ld) if ld else -1,
               statistics.median(sp) if sp else -1, max(sp) if sp else -1))

    print()
    print('== 2. lap times (lap_number increments) and pm (0x453C) ==')
    for drv, rows in sorted(cars.items()):
        laps = []
        cur = rows[0]['lap_number']
        t0 = rows[0]['sim_t']
        garage = False
        for r in rows:
            if r['driver_status'] in (3, 5):
                garage = True
            if r['lap_number'] != cur:
                laps.append((cur, r['sim_t'] - t0, garage))
                cur = r['lap_number']
                t0 = r['sim_t']
                garage = False
        fly = [t for _, t, g in laps if not g]
        gl = [t for _, t, g in laps if g]
        pm = [r['ai_pace_multiplier'] for r in rows if r['ai_pace_multiplier'] is not None]
        base = rows[0]['ability_baseline_scalar']
        push = rows[0]['race_ability_push_scalar']
        print('%-22s laps=%d flying med %.1fs (min %.1f max %.1f) garage-laps=%d | pm %.4f..%.4f base %.4f push %.4f' %
              (drv, len(laps), statistics.median(fly) if fly else -1,
               min(fly) if fly else -1, max(fly) if fly else -1, len(gl),
               min(pm) if pm else -1, max(pm) if pm else -1, base, push))

    print()
    print('== 2b. pm change events vs AIPushLap non-race ramp model ==')
    ok = bad = 0
    for drv, rows in sorted(cars.items()):
        target = rows[0]['ability_baseline_scalar'] + rows[0]['race_ability_push_scalar']
        prev = None
        for r in rows:
            v = r['ai_pace_multiplier']
            if v is not None and (prev is None or v != prev):
                if prev is not None:
                    lap = int(r['lap_number'] or 0)
                    step = min(lap, 10) * 0.003
                    if prev < target:
                        pred = min(prev + step, target)
                    else:
                        pred = target
                    match = abs(v - pred) < 1e-4
                    note = 'PARK' if r['driver_status'] == 5 else ('ok' if match else 'MISMATCH')
                    if note == 'ok':
                        ok += 1
                    else:
                        bad += 1
                    print('%-22s t=%.0f lap=%d st=%s pm %.4f -> %.4f (pred %.4f) %s' %
                          (drv, r['sim_t'], lap, r['driver_status'], prev, v, pred, note))
                prev = v
    print('ramp model: ok=%d mismatches/park=%d' % (ok, bad))

    print()
    print('== 3. speed_limit 0x4484 (non-sentinel) and speed ==')
    for drv, rows in sorted(cars.items()):
        lim = [(r['speed_limit'], r['speed'], r['driver_status']) for r in rows
               if r['speed_limit'] is not None and 0 < r['speed_limit'] < 1000]
        if not lim:
            print('%-22s no real limit samples' % drv)
            continue
        vals = sorted({round(x, 2) for x, _, _ in lim})
        pit = [(sl, sp, st) for sl, sp, st in lim if sl <= 23.0]
        over = sum(1 for sl, sp, _ in pit if sp > sl + 0.5)
        mx = max(sp for _, sp, _ in lim)
        print('%-22s n=%4d values=%s | <=23: n=%d max_speed %.2f over_limit %.2f=%d'
              % (drv, len(lim), vals[:8], len(pit), max([s for _, s, _ in pit], default=-1),
                 max([s - l for l, s, _ in pit], default=-1), over))

    print()
    print('== 4. interaction_target 0x45FC nonzero samples (practice) ==')
    it = 0
    for drv, rows in sorted(cars.items()):
        hits = [r for r in rows if r['interaction_target'] not in (0, 0.0, None)]
        if hits:
            it += len(hits)
            print('%-22s n=%d t=%.0f..%.0f values=%s speeds=%s laps=%s statuses=%s' %
                  (drv, len(hits), hits[0]['sim_t'], hits[-1]['sim_t'],
                   sorted({r['interaction_target'] for r in hits}),
                   sorted({round(r['speed'], 1) for r in hits}),
                   sorted({r['lap_number'] for r in hits}),
                   sorted({r['driver_status'] for r in hits})))
    print('total nonzero interaction_target practice samples:', it)

    print()
    print('== 5. unnamed_4554 (0x4554) raw values ==')
    vals = defaultdict(int)
    for drv, rows in cars.items():
        for r in rows:
            vals[r['unnamed_4554']] += 1
    print('distinct:', dict(list(vals.items())[:6]), 'n_distinct', len(vals))


if __name__ == '__main__':
    main()
