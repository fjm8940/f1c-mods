# c9_a_retest.py — C9: replay the corrected braking step with the TireForce model (tireforce.py)
# against the next recorded race (REVIEW_11: brain_watch now writes cars_static.jsonl, and
# snapshots carry waypoint k7c/k80 + extra.v3520/v3524).
#
# Expected inputs:
#   %TEMP%\ai_brain_live\<run>\cars_static.jsonl : per car, raw veh 0x3A7C..0x3D50 every 60 s.
#     Accepted record shapes: {"driver"|"name": str, "sim_t"|"t": float, "values"|"raw"|"fields": {key: val}}
#     or the same without a values dict (flat record). key = "0x3A7C" | "3A7C" | 14972 | "tire_T3740" | "T3740".
#     Table fields may be a 15-float list; otherwise the loader collects 15 consecutive offsets.
#   situations/<kind>_<driver>_<t>/snapshots.jsonl : waypoints[].k7c/.k80, extra.v3520/.v3524.
#
# Usage: python c9_a_retest.py [run-name]      (default: newest watch_* with cars_static.jsonl)
import json
import os
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L
import tireforce as TF

FACTOR = 0.915
TABLE_BASES = {3840: 0x3C0C, 3856: 0x3C4C, 3872: 0x3C8C, 3904: 0x3D0C}
SCALARS = {3397: 0x3520, 3398: 0x3524, 3740: 0x3A7C, 3839: 0x3C08, 3855: 0x3C48,
           3871: 0x3C88, 3919: 0x3D48, 3921: 0x3D50, 3903: 0x3D08}


def norm_key(k):
    if isinstance(k, int):
        return k
    s = str(k).strip()
    if s.lower().startswith('0x'):
        return int(s, 16)
    if s.isdigit():
        return int(s)
    if s.startswith('tire_T'):
        return SCALARS.get(int(s[6:]), TABLE_BASES.get(int(s[6:]), -1))
    if s.startswith('T') and s[1:].isdigit():
        return SCALARS.get(int(s[1:]), TABLE_BASES.get(int(s[1:]), -1))
    return -1


def load_static(run_dir):
    p = run_dir / 'cars_static.jsonl'
    if not p.exists():
        p = run_dir / 'cars_static.json'
    if not p.exists():
        return None
    txt = p.read_text(encoding='utf-8').strip()
    recs = []
    one_doc = txt.startswith('[') or (txt.startswith('{') and '\n' not in txt)   # Claude fix: JSONL starts with '{' too
    if one_doc:
        d = json.loads(txt)
        recs = d.values() if isinstance(d, dict) and 'driver' not in d and 'name' not in d else ([d] if isinstance(d, dict) else d)
    else:
        for line in txt.splitlines():
            if line.strip():
                recs.append(json.loads(line))
    out = defaultdict(list)
    for r in recs:
        if not isinstance(r, dict):
            continue
        drv = r.get('driver') or r.get('name')
        t = r.get('sim_t', r.get('t', 0.0))
        vals = r.get('values') or r.get('raw') or r.get('fields')
        if vals is None:
            vals = {k: v for k, v in r.items() if k not in ('driver', 'name', 'sim_t', 't', 'slot', 'tier')}
        flat = {}
        for k, v in vals.items():
            off = norm_key(k)
            if off >= 0:
                flat[off] = v
        if drv:
            out[drv].append((float(t or 0.0), flat))
    return out


REQUIRED_SCALARS = (3397, 3398, 3740, 3839, 3855, 3871, 3919, 3921)
REQUIRED_TABLES = (3840, 3856, 3872)


def build_T(flat, extra):
    """flat: {veh_offset: value}; extra: snapshot['extra']. Returns T dict or None + missing list.
    TireForce needs REQUIRED_SCALARS/TABLES; the TireForceSolver fields 3903/3904 are optional
    (only the corner-speed path uses them)."""
    missing = []
    T = {}
    for n, off in SCALARS.items():
        v = flat.get(off)
        if v is None and off == 0x3520 and extra:
            v = extra.get('v3520')
        if v is None and off == 0x3524 and extra:
            v = extra.get('v3524')
        if v is None:
            if n in REQUIRED_SCALARS:
                missing.append('0x%X' % off)
        else:
            T[n] = float(v)
    for n, base in TABLE_BASES.items():
        if n not in REQUIRED_TABLES:
            continue
        arr = flat.get(base)
        if isinstance(arr, list) and len(arr) >= 15:
            T[n] = [float(x) for x in arr[:15]]
            continue
        vals = [flat.get(base + 4 * i) for i in range(15)]
        if any(v is None for v in vals):
            missing.append('0x%X..+56' % base)
        else:
            T[n] = [float(v) for v in vals]
    return (T if not missing else None), missing


def static_at(stat, drv, t):
    if stat and drv in stat:
        best = None
        for ts, flat in stat[drv]:
            if ts <= t + 1e-6 and (best is None or ts > best[0]):
                best = (ts, flat)
        if best is None:
            best = min(stat[drv], key=lambda x: x[0])
        return best[1]
    return None


def main():
    base = Path(os.environ['TEMP']) / 'ai_brain_live'
    if len(sys.argv) > 1:
        run = sys.argv[1]
    else:
        cands = [d.name for d in base.glob('watch_*') if (d / 'cars_static.jsonl').exists() or (d / 'cars_static.json').exists()]
        run = sorted(cands)[-1] if cands else None
        if run is None:
            print('BLOCKED: no watch_* with cars_static.jsonl under %s.' % base)
            print('Next recorded race must include cars_static.jsonl (RECORDER_REQUESTS A5) and wp k7c/k80.')
            return
    run_dir = base / run
    stat = load_static(run_dir)
    if stat is None:
        print('BLOCKED: %s has no cars_static.jsonl.' % run_dir)
        return
    rows = defaultdict(list)
    missing = defaultdict(int)
    n_steps = 0
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
            drv = s['driver']
            flat = static_at(stat, drv, s['sim_t'] or 0.0)
            T, miss = build_T(flat or {}, s.get('extra') or {})
            if T is None:
                missing[','.join(miss[:2])] += 1
                continue
            for j in range(i1, k):
                wp = wps[j + 1]
                if not all(key in wp for key in ('k112', 'k116', 'k120', 'k7c', 'k80')):
                    missing['waypoint k7c/k80'] += 1
                    continue
                v = prof[j - 1]
                if v <= 1.0:
                    continue
                F = TF.tireforce(v, wp['k112'], wp['k116'], wp['k120'], wp['k7c'], wp['k80'], T)
                pred = wp['seg_len'] / v * dec * F * FACTOR
                meas = prof[j - 1] - prof[j]
                if pred <= 0.05 or meas <= 0.01:
                    continue
                pos = 'first' if j == i1 else ('last' if j == k - 1 else 'mid')
                rows[pos].append(meas / pred)
                rows['all'].append(meas / pred)
                n_steps += 1
    print('== C9 A re-test [%s] ==' % run)
    if not n_steps:
        print(' no evaluable steps; missing:', dict(missing))
        return
    for pos in ('first', 'mid', 'last', 'all'):
        r = sorted(rows[pos])
        if r:
            print(' meas/pred %-5s: n=%d p10=%.3f med=%.3f p90=%.3f; within +-20%%: %.1f%%'
                  % (pos, len(r), r[len(r) // 10], r[len(r) // 2], r[int(0.9 * len(r))],
                     100.0 * sum(1 for x in r if 0.8 <= x <= 1.2) / len(r)))
    if missing:
        print(' skipped:', dict(missing))


if __name__ == '__main__':
    main()
