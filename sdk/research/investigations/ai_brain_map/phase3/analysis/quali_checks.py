# quali_checks.py - Claude, 2026-10-02. Read-only checks of the wet Imola QUALIFYING watch logs against the
# static predictions (investigations/ai_interaction_role/README.md 2026-08-18; doc 54; phase3 PACE_MULTIPLIER.md):
#  A. interaction target in session 5: Overtaking writes it only when THIS car fails ServerSessionGateCheck
#     (lapOfRun 0x4094 == 2 and not called to box = flying lap) and the candidate passes it (lapOfRun != 2 or boxed).
#     ProximityScan's auto-yield is race-only, so no other writer is expected.
#  B. pace 0x453C outside the race: AIAbility race logic is skipped; AIPushLap ramps it UP only (+0.003 x laps).
#  C. lapOfRun / mode sequences (garage exit 5 -> 3 -> 0, entry 0 -> 5).
# Usage: python quali_checks.py [folder ...]   (default: the two qualifying folders of 2026-10-02)
import bisect
import collections
import json
import os
import sys
from pathlib import Path

BASE = Path(os.environ['TEMP']) / 'ai_brain_live'
RUNS = sys.argv[1:] or ['watch_20261002T203309Z', 'watch_20261002T203532Z']
VEH_BASE, STRIDE = 0x5EC6D8, 0x48C0


def slot_of(p):
    if isinstance(p, str):
        p = int(p, 16)
    if p and VEH_BASE <= p < VEH_BASE + 104 * STRIDE and (p - VEH_BASE) % STRIDE == 0:
        return (p - VEH_BASE) // STRIDE
    return None


def load(run):
    d = BASE / run
    ov = collections.defaultdict(list)          # slot -> [(t, fields)]
    names = {}
    for line in open(d / 'overview.jsonl', encoding='utf-8'):
        try:
            r = json.loads(line)
        except ValueError:
            continue
        ov[r['slot']].append((r['sim_t'] or 0.0, r['fields']))
        names[r['slot']] = r['driver']
    ev = []
    for line in open(d / 'events.jsonl', encoding='utf-8'):
        try:
            ev.append(json.loads(line))
        except ValueError:
            pass
    return ov, names, ev


def at(series, t):
    ts = [x for x, _ in series]
    i = bisect.bisect_left(ts, t)
    cand = [j for j in (i - 1, i) if 0 <= j < len(series)]
    j = min(cand, key=lambda j: abs(series[j][0] - t)) if cand else None
    return series[j][1] if j is not None else None


def gate_pass(f):
    """ServerSessionGateCheck: pass iff callToBox == 1 or lapOfRun != 2."""
    return bool(f.get('call_to_box')) or f.get('lap_of_run') != 2


def main():
    for run in RUNS:
        ov, names, ev = load(run)
        sess = collections.Counter(e.get('globals', {}).get('session') for e in [])
        print('== %s: %d cars, %d events' % (run, len(ov), len(ev)))
        # ---- A. interaction targets
        rows = []
        for e in ev:
            if e.get('kind') != 'interaction_target' or not e.get('new'):
                continue
            ws = next((s for s, n in names.items() if n == e['driver']), None)
            ts = slot_of(e['new'])
            if ws is None:
                continue
            fw = at(ov[ws], e['sim_t'])
            ft = at(ov.get(ts, []), e['sim_t']) if ts is not None else None
            if not fw:
                continue
            ok = (not gate_pass(fw)) and (ft is not None and gate_pass(ft))
            rows.append((e['sim_t'], e['driver'], names.get(ts, 'slot %s' % ts), fw.get('lap_of_run'),
                         fw.get('call_to_box'), ft.get('lap_of_run') if ft else None,
                         ft.get('call_to_box') if ft else None, fw.get('lateral_request'),
                         fw.get('throttle_cap_difficulty'), ok))
        print(' A. interaction-target sets: %d; match quali gate (writer flying, target not): %d' % (
            len(rows), sum(r[-1] for r in rows)))
        for r in rows[:30]:
            print('   t=%7.1f %-20s -> %-20s writer lapOfRun %s box %s | target lapOfRun %s box %s | req %s cap %s %s'
                  % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], 'OK' if r[9] else 'MISMATCH'))
        # ---- B. pace
        up = down = flat = 0
        downs = []
        for s, ser in ov.items():
            prev = None
            for t, f in ser:
                p = f.get('ai_pace_multiplier')
                if prev is not None and p is not None:
                    if p > prev + 1e-6:
                        up += 1
                    elif p < prev - 1e-6:
                        down += 1
                        downs.append((names[s], round(t, 1), prev, p, f.get('driver_status'), f.get('car_conditions')))
                    else:
                        flat += 1
                prev = p
        print(' B. pace 1 Hz steps: up %d, down %d, flat %d' % (up, down, flat))
        for d in downs[:10]:
            print('   DOWN %s t=%s %.4f -> %.4f mode %s cond %s' % d)
        # ---- C. lapOfRun and modes
        lor = collections.Counter()
        for s, ser in ov.items():
            for t, f in ser:
                lor[(f.get('lap_of_run'), f.get('driver_status'))] += 1
        print(' C. (lapOfRun, mode) samples:', dict(sorted(lor.items(), key=lambda kv: -kv[1])[:12]))
        tr = collections.Counter((e['old'], e['new']) for e in ev if e.get('kind') == 'driver_status')
        print('    mode transitions:', dict(tr))
        cb = collections.Counter((e['old'], e['new']) for e in ev if e.get('kind') == 'call_to_box')
        print('    call_to_box transitions:', dict(cb))
        cc = [(e['driver'], round(e['sim_t'], 1), e['old'], e['new']) for e in ev if e.get('kind') == 'car_conditions']
        print('    car_conditions changes:', cc[:5])


if __name__ == '__main__':
    main()
