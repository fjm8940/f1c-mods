# d11_wet.py -- D11a: wet Imola replays (read-only).
#
# Data: %TEMP%\ai_brain_live\watch_20261002T194606Z (WET Imola, 6 laps, monsoon, 19 AI cars,
# 20 waypoints, cars_static present, registry v1.1 at record time -> unnamed_4554 and
# command_queue_write_index are logged as raw int bits / NaN; read with v1.2 types).
# Dry same-track baseline: watch_race2 + watch_race2b (Imola; old 6-waypoint records, no cars_static).
# Dry tyre-table baseline: watch_race4fullc + watch_race4fulld (Monza, cars_static present).
#
# Parts (handoff BATCH B item 1):
#  (a) D1 corner-speed replay on the wet folder (same PASS rule as D1: median +-2 %, p10..p90 +-5 %).
#  (b) braking-step replay with phase2/analysis/tireforce.py, c9_a_retest method (median meas/pred,
#      % within +-20 %).
#  (c) wet vs dry Imola: physical braking decel, corner targets at the same limiting waypoints
#      (matched by waypoint x/z within 2 m), lap pace.
#  (d) cars_static wet vs Monza dry for the same driver: which TireForce / TireForceSolver
#      table entries change with wet tyres.
#
# Reuses c1b_lib (import, unmodified), phase2 tireforce.py, phase3 tireforcesolver.py. No writes.
import bisect
import json
import math
import os
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase3' / 'analysis'))
import c1b_lib as L                                  # noqa: E402
import tireforce as TF                               # noqa: E402
from tireforcesolver import tireforcesolver          # noqa: E402

WET = 'watch_20261002T194606Z'
DRY_IMOLA = ('watch_race2', 'watch_race2b')
MONZA = ('watch_race4fullc', 'watch_race4fulld')
FACTOR = 0.915
TBL_3856 = 0x3C4C
TBL_3872 = 0x3C8C
TBL_3904 = 0x3D0C
# TireForce (phase2 model) fields: n -> veh offset
TF_SCALARS = {3397: 0x3520, 3398: 0x3524, 3740: 0x3A7C, 3839: 0x3C08, 3855: 0x3C48,
              3871: 0x3C88, 3919: 0x3D48, 3921: 0x3D50}
TF_TABLES = {3840: 0x3C0C, 3856: 0x3C4C, 3872: 0x3C8C}


def as_float(v):
    """v1.1 registries logged some float fields as raw int bits (brain_core unknown type)."""
    if isinstance(v, int):
        return struct.unpack('<f', struct.pack('<I', v & 0xffffffff))[0]
    return v


def quant(xs, q):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(q * len(xs)))]


def load_cars_static(run):
    p = L.run_dir(run) / 'cars_static.jsonl'
    by_slot = defaultdict(list)
    by_driver = defaultdict(list)
    if not p.exists():
        return by_slot, by_driver
    with open(p, encoding='utf-8') as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            r = json.loads(line)
            by_slot[r['slot']].append((r['sim_t'], r['values']))
            by_driver[r.get('driver')].append((r['sim_t'], r['values']))
    for d in (by_slot, by_driver):
        for k in d:
            d[k].sort(key=lambda t: t[0])
    return by_slot, by_driver


def latest_row(rows, sim_t):
    ts = [t for t, _ in rows]
    i = bisect.bisect_right(ts, sim_t) - 1
    return rows[i][1] if i >= 0 else None


def table15(values, base):
    return [values['0x%X' % (base + 4 * k)] for k in range(15)]


def collect_rows(run):
    """Part (a): D1 replay rows (same exclusions as d1_corner_speed.py)."""
    static, _ = load_cars_static(run)
    rows = []
    skip = dict(no_lim=0, target_9999=0, branch=0, wp_missing=0, no_static=0,
                no_v3520=0, empty_wp=0)
    for idx in L.situations(run):
        for s in L.snaps(run, idx['folder']):
            f = s['fields']
            lim = f.get('limiting_waypoint_ptr')
            bt = f.get('braking_target_speed')
            if not lim or lim == '0x0':
                skip['no_lim'] += 1
                continue
            if bt is None or abs(bt - 9999.0) < 1e-3:
                skip['target_9999'] += 1
                continue
            wps = s.get('waypoints') or []
            if not wps:
                skip['empty_wp'] += 1
                continue
            wp = next((w for w in wps if w['ptr'] == lim), None)
            if wp is None:
                skip['wp_missing'] += 1
                continue
            if wp.get('branch_flag') != 0 or wp.get('branch_val') != 0:
                skip['branch'] += 1
                continue
            cs = static.get(s['slot'])
            if not cs:
                skip['no_static'] += 1
                continue
            vals = latest_row(cs, s['sim_t'])
            if vals is None:
                skip['no_static'] += 1
                continue
            v3520 = (s.get('extra') or {}).get('v3520')
            if v3520 is None:
                skip['no_v3520'] += 1
                continue
            T = {
                3397: as_float(v3520),
                3740: vals['0x3A7C'],
                3855: vals['0x3C48'], 3856: table15(vals, TBL_3856),
                3871: vals['0x3C88'], 3872: table15(vals, TBL_3872),
                3903: vals['0x3D08'], 3904: table15(vals, TBL_3904),
            }
            tfs = tireforcesolver(wp['k112'], wp['k116'], wp['k120'], wp['k7c'], T)
            base = ((tfs * f['corner_speed_force_coeff'] + f['corner_speed_force_offset'])
                    * f['corner_speed_scale'] * f['ai_pace_multiplier'] * wp['speed_k'])
            mult = as_float(f.get('unnamed_4554')) if f.get('unnamed_4554') is not None else 1.0
            full = base * mult
            rows.append(dict(run=run, sim_t=s['sim_t'], slot=s['slot'], driver=s.get('driver'),
                             measured=bt, tfs=tfs, pred=full, ratio=bt / full if full else None,
                             ratio_nomult=bt / base if base else None, wp_ptr=wp['ptr'],
                             k112=wp['k112'], k116=wp['k116'], k120=wp['k120'], k7c=wp['k7c'],
                             speed_k=wp['speed_k'], scale=f['corner_speed_scale'],
                             pace=f['ai_pace_multiplier'], mult4554=mult, v3520=as_float(v3520)))
    return rows, skip


def part_a():
    print('== D11a(a) D1 corner-speed replay on WET Imola [%s] ==' % WET)
    rows, skip = collect_rows(WET)
    print('exclusions: %s' % skip)
    ratios = [r['ratio'] for r in rows if r['ratio'] is not None]
    if not ratios:
        print('VERDICT: n=0 -> BLOCKED')
        return
    inside2 = sum(1 for x in ratios if abs(x - 1.0) <= 0.02) / len(ratios)
    p10, med, p90 = quant(ratios, 0.1), quant(ratios, 0.5), quant(ratios, 0.9)
    print('n=%d ratio med=%.4f p10=%.4f p90=%.4f min=%.4f max=%.4f within +-2%%=%.1f%%'
          % (len(ratios), med, p10, p90, min(ratios), max(ratios), 100.0 * inside2))
    mults = sorted({round(r['mult4554'], 6) for r in rows})
    print('0x4554 as float bits: %s' % mults[:5])
    bydrv = defaultdict(list)
    for r in rows:
        bydrv[r['driver']].append(r['ratio'])
    print('by driver (median, n):')
    for d in sorted(bydrv, key=lambda k: quant(bydrv[k], 0.5)):
        print('  %-24s med=%.4f n=%d' % (d, quant(bydrv[d], 0.5), len(bydrv[d])))
    print('by predicted-target bin (m/s):')
    for lo, hi in ((0, 20), (20, 40), (40, 60), (60, 90), (90, 130)):
        xs = [r['ratio'] for r in rows if lo <= r['pred'] < hi]
        if xs:
            print('  %3d-%-3d n=%-4d med=%.4f p10=%.4f p90=%.4f'
                  % (lo, hi, len(xs), quant(xs, 0.5), quant(xs, 0.1), quant(xs, 0.9)))
    worst = sorted(rows, key=lambda r: abs(r['ratio'] - 1.0), reverse=True)[:10]
    print('10 worst |ratio-1|:')
    for r in worst:
        print('  sim=%.1f slot=%2d %-20s meas=%.3f pred=%.3f ratio=%.4f tfs=%.3f k112=%.5f '
              'k116=%.2e k120=%.2e k7c=%.3f speed_k=%.3f pace=%.4f scale=%.4f'
              % (r['sim_t'], r['slot'], (r['driver'] or '?')[:20], r['measured'], r['pred'],
                 r['ratio'], r['tfs'], r['k112'], r['k116'], r['k120'], r['k7c'],
                 r['speed_k'], r['pace'], r['scale']))
    verdict = 'PASS' if abs(med - 1.0) <= 0.02 and p10 >= 0.95 and p90 <= 1.05 else 'FAIL'
    print('VERDICT (a): %s (rule: |med-1|<=2%% and p10>=0.95 and p90<=1.05)' % verdict)


def build_T(flat, extra):
    """TireForce T dict from a cars_static values row + snapshot extra (c9 method)."""
    missing = []
    T = {}
    for n, off in TF_SCALARS.items():
        v = flat.get('0x%X' % off)
        if v is None and extra:
            if off == 0x3520:
                v = extra.get('v3520')
            elif off == 0x3524:
                v = extra.get('v3524')
        if v is None:
            missing.append('0x%X' % off)
        else:
            T[n] = float(v)
    for n, base in TF_TABLES.items():
        arr = flat.get('0x%X' % base)
        if isinstance(arr, list) and len(arr) >= 15:
            T[n] = [float(x) for x in arr[:15]]
            continue
        vals = [flat.get('0x%X' % (base + 4 * i)) for i in range(15)]
        if any(v is None for v in vals):
            missing.append('0x%X..+56' % base)
        else:
            T[n] = [float(v) for v in vals]
    return (T if not missing else None), missing


def part_b():
    print()
    print('== D11a(b) braking-step replay (tireforce.py, c9 method) on WET Imola ==')
    run = WET
    _, by_drv = load_cars_static(run)
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
            wps = s['waypoints']
            if len(wps) < 20:
                continue
            lim = L.hx(f['limiting_waypoint_ptr'])
            ch = L.chain(s)
            if lim not in ch:
                continue
            k = ch.index(lim)
            prof = f['speed_profile']
            i1 = next((j for j in range(1, 20) if prof[j] < f['speed'] - 1e-3), 20)
            if not (0 < i1 < k and k <= 19):
                continue
            drv = s['driver']
            rowsd = by_drv.get(drv)
            flat = latest_row(rowsd, s['sim_t'] or 0.0) if rowsd else None
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
                pred = wp['seg_len'] / v * f['brake_decel_factor'] * F * FACTOR
                meas = prof[j - 1] - prof[j]
                if pred <= 0.05 or meas <= 0.01:
                    continue
                pos = 'first' if j == i1 else ('last' if j == k - 1 else 'mid')
                rows[pos].append(meas / pred)
                rows['all'].append(meas / pred)
                n_steps += 1
    if not n_steps:
        print(' no evaluable steps; missing: %s' % dict(missing))
        print('VERDICT (b): BLOCKED')
        return
    for pos in ('first', 'mid', 'last', 'all'):
        r = sorted(rows[pos])
        if r:
            print(' meas/pred %-5s n=%-4d p10=%.3f med=%.3f p90=%.3f within +-20%%=%.1f%%'
                  % (pos, len(r), r[len(r) // 10], r[len(r) // 2], r[int(0.9 * len(r))],
                     100.0 * sum(1 for x in r if 0.8 <= x <= 1.2) / len(r)))
    print(' skipped: %s' % dict(missing))
    allr = sorted(rows['all'])
    med = allr[len(allr) // 2]
    share = sum(1 for x in allr if 0.8 <= x <= 1.2) / len(allr)
    verdict = 'CONFIRMED' if abs(med - 1.0) <= 0.05 and share >= 0.7 else 'NOT CONFIRMED'
    print('VERDICT (b): %s (median meas/pred %.3f, %.1f%% within +-20%%)' % (verdict, med, 100 * share))


def zones(snaps):
    out, cur = [], None
    for s in snaps:
        if L.active(s):
            if cur is None:
                cur = []
                out.append(cur)
            cur.append(s)
        else:
            cur = None
    return out


def part_c_decel():
    print()
    print('== D11a(c1) physical braking decel: wet vs dry Imola ==')
    data = {}
    for run in (WET,) + DRY_IMOLA:
        decel = []
        for x in L.situations(run):
            if x['kind'] != 'brake_zone':
                continue
            for z in zones(list(L.snaps(run, x['folder']))):
                for a, b in zip(z, z[1:]):
                    dt = b['sim_t'] - a['sim_t']
                    dv = a['fields']['speed'] - b['fields']['speed']
                    if 0.03 <= dt <= 0.15 and dv > 0:
                        decel.append(dv / dt)
        data[run] = decel
        if decel:
            s = sorted(decel)
            print('  %-26s n=%-5d med=%.1f p90=%.1f max=%.1f m/s^2'
                  % (run, len(s), s[len(s) // 2], s[int(0.9 * len(s))], s[-1]))
    wet = sorted(data[WET])
    dry = sorted(data[DRY_IMOLA[0]] + data[DRY_IMOLA[1]])
    if wet and dry:
        print('  WET med %.2f vs DRY med %.2f -> wet/dry = %.3f'
              % (wet[len(wet) // 2], dry[len(dry) // 2], wet[len(wet) // 2] / dry[len(dry) // 2]))
        print('VERDICT (c1): wet braking decel is %s the dry Imola baseline (medians)'
              % ('below' if wet[len(wet) // 2] < dry[len(dry) // 2] else 'NOT below'))


def corner_target_samples(run, max_wp=20):
    out = []
    for x in L.situations(run):
        if x['kind'] != 'brake_zone':
            continue
        for s in L.snaps(run, x['folder']):
            f = s['fields']
            lim = f.get('limiting_waypoint_ptr')
            tgt = f.get('braking_target_speed')
            if not lim or lim == '0x0' or tgt is None or abs(tgt - 9999.0) < 1e-3:
                continue
            wps = s.get('waypoints') or []
            wp = next((w for w in wps[:max_wp] if w['ptr'] == lim), None)
            if wp is None:
                continue
            out.append(dict(x=wp['x'], z=wp['z'], target=tgt, speed=f['speed'],
                            driver=s.get('driver'), sim_t=s['sim_t'],
                            scale=f.get('corner_speed_scale')))
    return out


def part_c_targets():
    print()
    print('== D11a(c2) corner targets at the same limiting waypoints (match x/z <= 2 m) ==')
    wet = corner_target_samples(WET)
    dry = []
    for run in DRY_IMOLA:
        dry += corner_target_samples(run)
    print('  wet samples: %d, dry samples: %d' % (len(wet), len(dry)))
    if not wet or not dry:
        print('  VERDICT (c2): no samples -> BLOCKED')
        return
    # bucket dry waypoints by rounded position
    dry_by_pos = defaultdict(list)
    for d in dry:
        dry_by_pos[(round(d['x'] / 2.0), round(d['z'] / 2.0))].append(d)
    pairs = defaultdict(lambda: ([], []))
    unmatched = 0
    for w in wet:
        best = None
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                for d in dry_by_pos.get((round(w['x'] / 2.0) + dx, round(w['z'] / 2.0) + dz), []):
                    dist = math.hypot(w['x'] - d['x'], w['z'] - d['z'])
                    if dist <= 2.0 and (best is None or dist < best[0]):
                        best = (dist, d)
        if best is None:
            unmatched += 1
            continue
        key = (round(best[1]['x'], 1), round(best[1]['z'], 1))
        pairs[key][0].append(w['target'])
        pairs[key][1].append(best[1]['target'])
    ratios = []
    print('  matched waypoints: %d (wet unmatched samples: %d)' % (len(pairs), unmatched))
    for key in sorted(pairs):
        wt, dt = pairs[key]
        wm, dm = quant(wt, 0.5), quant(dt, 0.5)
        ratio = wm / dm if dm else None
        if ratio is not None:
            ratios.append(ratio)
        print('   wp(%8.1f,%8.1f) wet med=%7.3f (n=%3d) dry med=%7.3f (n=%3d) ratio=%.3f'
              % (key[0], key[1], wm, len(wt), dm, len(dt), ratio if ratio is not None else -1))
    if ratios:
        print('  ratio wet/dry across matched waypoints: n=%d med=%.3f p10=%.3f p90=%.3f'
              % (len(ratios), quant(ratios, 0.5), quant(ratios, 0.1), quant(ratios, 0.9)))
        print('VERDICT (c2): wet corner targets %.1f%% of dry at matched waypoints (median)'
              % (100 * quant(ratios, 0.5)))


def lap_stats(run):
    """1 Hz overview: lap durations = first-sample time of lap n+1 minus first-sample time of n."""
    laps = defaultdict(list)
    best = {}
    prev_ln = {}
    first_t = {}
    for r in L.overview(run):
        f = r['fields']
        drv = r.get('driver')
        ln = f.get('lap_number')
        bl = f.get('best_lap')
        if bl is not None and bl > 0:
            best[drv] = bl
        if drv is None or ln is None:
            continue
        if drv not in prev_ln:
            prev_ln[drv] = ln
            first_t[drv] = r['sim_t']
            continue
        if ln != prev_ln[drv]:
            if ln == prev_ln[drv] + 1:
                dur = r['sim_t'] - first_t[drv]
                if 5.0 < dur < 300.0:
                    laps[ln].append(dur)
            prev_ln[drv] = ln
            first_t[drv] = r['sim_t']
    all_laps = [x for v in laps.values() for x in v]
    return all_laps, sorted(best.values())


def part_c_laps():
    print()
    print('== D11a(c3) lap pace: wet vs dry Imola (overview 1 Hz) ==')
    for run in (WET,) + DRY_IMOLA:
        all_laps, best = lap_stats(run)
        if all_laps:
            s = sorted(all_laps)
            print('  %-26s laps n=%-4d med=%6.2f s p10=%6.2f p90=%6.2f | best_lap med=%s (n=%d)'
                  % (run, len(s), s[len(s) // 2], s[int(0.1 * len(s))], s[int(0.9 * len(s))],
                     ('%.2f' % quant(best, 0.5)) if best else 'n/a', len(best)))
    wet_laps, wet_best = lap_stats(WET)
    dry_laps = []
    dry_best = []
    for run in DRY_IMOLA:
        a, b = lap_stats(run)
        dry_laps += a
        dry_best += b
    if wet_laps and dry_laps:
        wm = quant(wet_laps, 0.5)
        dm = quant(dry_laps, 0.5)
        print('  lap-time median WET %.2f s vs DRY %.2f s -> wet/dry = %.3f'
              % (wm, dm, wm / dm))
        if wet_best and dry_best:
            print('  best_lap median WET %.2f s vs DRY %.2f s -> wet/dry = %.3f'
                  % (quant(wet_best, 0.5), quant(dry_best, 0.5),
                     quant(wet_best, 0.5) / quant(dry_best, 0.5)))
        print('VERDICT (c3): wet lap pace is %.1f%% of dry (median lap duration)'
              % (100.0 * wm / dm))
    else:
        print('VERDICT (c3): no complete lap transitions -> BLOCKED')


def part_d():
    print()
    print('== D11a(d) tyre tables: wet Imola vs Monza dry, same driver ==')
    _, wet_drv = load_cars_static(WET)
    monza = {}
    for run in MONZA:
        _, md = load_cars_static(run)
        for drv, rows in md.items():
            monza.setdefault(drv, []).extend(rows)
    common = sorted(set(wet_drv) & set(monza))
    print('  same-driver set: %s' % common)
    if not common:
        print('  VERDICT (d): no common driver -> BLOCKED')
        return
    diff_count = defaultdict(int)
    rel = defaultdict(list)
    ratio = defaultdict(list)
    n_drv = 0
    for drv in common:
        wv = wet_drv[drv][-1][1]
        mv = sorted(monza[drv])[-1][1]
        for k in wv:
            if k not in mv:
                continue
            a, b = wv[k], mv[k]
            if isinstance(a, list) or isinstance(b, list):
                continue
            if a is None or b is None:
                continue
            d = abs(a - b)
            if d > max(1e-4, 1e-4 * max(abs(a), abs(b))):
                diff_count[k] += 1
            if b not in (0.0,):
                rel[k].append(abs(a - b) / abs(b))
                ratio[k].append(a / b)
        n_drv += 1
    print('  drivers compared: %d' % n_drv)
    changed = sorted((k for k, c in diff_count.items() if c >= max(1, n_drv // 2)),
                     key=lambda k: -sum(rel.get(k, [0])))
    print('  offsets differing (>=half of drivers): %d' % len(changed))
    names = {}
    for n, off in ((3740, 0x3A7C), (3839, 0x3C08), (3855, 0x3C48), (3871, 0x3C88),
                   (3903, 0x3D08), (3919, 0x3D48), (3921, 0x3D50)):
        names[off] = 'T%d' % n
    for n, base in ((3840, 0x3C0C), (3856, 0x3C4C), (3872, 0x3C8C), (3904, 0x3D0C)):
        for i in range(15):
            names[base + 4 * i] = 'T%d[%d]' % (n, i)
    for k in changed[:40]:
        rr = quant(ratio[k], 0.5) if ratio.get(k) else 0.0
        print('   %-8s %-10s drivers %2d/%d  wet/dry med %.4f'
              % (names.get(int(k, 16), '?'), k, diff_count[k], n_drv, rr))
    if len(changed) > 40:
        print('   ... %d more (all not read by the two models)' % (len(changed) - 40))
    unnamed = [k for k in changed if int(k, 16) not in names]
    print('  changed offsets not read by the TireForce/TireForceSolver models: %d' % len(unnamed))
    # classify by TireForce/TireForceSolver block
    blocks = {'T3740 scalar': [0x3A7C],
              'T3855/T3856 selector+tbl': [0x3C48] + [0x3C4C + 4 * i for i in range(15)],
              'T3871/T3872 selector+tbl': [0x3C88] + [0x3C8C + 4 * i for i in range(15)],
              'T3839/T3840 selector+tbl': [0x3C08] + [0x3C0C + 4 * i for i in range(15)],
              'T3903/T3904 solver sel+tbl': [0x3D08] + [0x3D0C + 4 * i for i in range(15)],
              'T3919/T3921 limits': [0x3D48, 0x3D50]}
    print('  changed-by-block (>=half drivers):')
    for name, offs in blocks.items():
        c = sum(1 for off in offs if diff_count.get('0x%X' % off, 0) >= max(1, n_drv // 2))
        print('   %-30s %d/%d entries' % (name, c, len(offs)))
    named_changed = [k for k in changed if int(k, 16) in names]
    print('VERDICT (d): wet tyres change %d/%d named TireForce/TireForceSolver entries '
          '(every selector+table block, T3740, T3919/T3921); %d further changed offsets '
          'are outside the two models' % (len(named_changed), len(names), len(unnamed)))


def main():
    print('D11a wet replays -- %s' % WET)
    part_a()
    part_b()
    part_c_decel()
    part_c_targets()
    part_c_laps()
    part_d()


if __name__ == '__main__':
    main()
