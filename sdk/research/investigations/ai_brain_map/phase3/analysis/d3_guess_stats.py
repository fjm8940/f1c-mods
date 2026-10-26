# d3_guess_stats.py -- D3.2: value statistics for the 57 GUESS fields in ABI v1.1
# from the Monza logs (watch_race4fullc + watch_race4fulld overview.jsonl, 1 Hz, all cars).
# Read-only. Writes only phase3/GUESS_FIELDS.md (+ a stats JSON under %TEMP%/f1c_analysis).
import json
import math
import os
from pathlib import Path

AI = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled') / 'investigations' / 'ai_brain_map'
CACHE = Path(os.environ['TEMP']) / 'f1c_analysis'
RUNS = ('watch_race4fullc', 'watch_race4fulld')

# assessment: id -> (data reading, proposed meaning + tier). Written after the first stats pass.
ASSESS = {
 'placement_pose_x': ('unit-range floats -1..+1, 14756 distinct, changes every tick, weak lap corr',
                      'GUESS: values look direction-like, not a metric pose; meaning should be re-read before any raise'),
 'placement_pose_y': ('-0.85..+0.25, 2519 distinct, weak lap corr 0.31',
                      'GUESS: direction-like component (same as x/z)'),
 'placement_pose_z': ('-1..+1, 12887 distinct, no state correlation',
                      'GUESS: direction-like component; no tier raise'),
 'opp_slot_vehicle_ptr': ('21 pointer values, constant per run/slot; battle r=0.09 (addresses not meaningful)',
                          'GUESS: consistent with per-slot opponent pointer; no tier raise'),
 'opp_slot_distance_sq': ('1..1e6 (1e6 sentinel), battle r=-0.18; nearest-first order already confirmed on 3 tracks',
                          'GUESS: ordering evidence is OBSERVED-level but the squared-distance unit is not calibrated; no tier raise'),
 'opp_team_match_flag': ('0/1 on every car; team-record pairing matched 99.99 % (Monaco) and 100 % (Monza)',
                         'OBSERVED: match flag confirmed by the 3-track record pairing'),
 'opp_team_id': ('0..19 plus 0xFFFFFFFF empty-record sentinel, constant per car',
                 'OBSERVED: the id used by the validated record pairing (Imola/Monaco/Monza)'),
 'opp_team_threshold': ('0..500, 1225 distinct, changes during the race, sim r=-0.27',
                        'GUESS unchanged; per-tick reset behaviour not contradicted'),
 'opp_state': ('-9999 sentinel..3357, 4488 changes, sim r=-0.30; battle context is D6 work',
               'GUESS unchanged; meaning needs D6'),
 'unnamed_4000': ('constant 0 on all cars', 'GUESS unchanged (threshold read; no data)'),
 'unnamed_4040': ('{0,1,2}', 'GUESS unchanged'),
 'unnamed_413c': ('49.9..FLT_MAX (FLT_MAX median), 639 distinct', 'GUESS unchanged; FLT_MAX branch matches the note'),
 'unnamed_416c': ('constant 0', 'GUESS unchanged'),
 'unnamed_4170': ('constant FLT_MAX', 'GUESS unchanged'),
 'outside_corridor_flag': ('0/1, 10687 changes', 'GUESS unchanged'),
 'unnamed_4364': ('pointer-like 0x31B0.... waypoint addresses, 1175 distinct, changes with the waypoint chain',
                  'GUESS: meaning correction - waypoint-node pointer scratch (not a generic read)'),
 'unnamed_43f4': ('0.40..40.7 float, 24744 distinct', 'GUESS unchanged'),
 'unnamed_4400': ('pointer-like waypoint address, constant per car (0 changes), 10 distinct',
                  'GUESS: meaning correction - cached waypoint pointer, constant per run'),
 'unnamed_4404': ('0..3361, median 1539, r=0.86 vs cumulative distance / 0.84 vs sim_t',
                  'GUESS unchanged: time-like stamp but not exactly sim_t'),
 'unnamed_441c': ('pointer-like 0xBC0.... addresses, 56835 distinct', 'GUESS: meaning correction - pointer scratch (heap node)'),
 'unnamed_4444': ('0..1 float, 6016 distinct, lap r=0.38', 'GUESS unchanged'),
 'time_margin': ('0.056..0.198 s, 58 distinct, only 40 changes', 'GUESS unchanged; plausible stable margin'),
 'unnamed_444c': ('small integer enum {2,4,5}, 38 changes', 'GUESS: meaning correction - small state enum, not a float'),
 'unnamed_4454': ('89101..224900, 29635 distinct, r=0.95 vs cumulative speed integral',
                  'GUESS: meaning correction - cumulative distance-like quantity, not a scratch float'),
 'unnamed_4458': ('0..0xFFFFFFFF, 41 distinct, lap r=-0.48', 'GUESS unchanged (counter/bitfield)'),
 'unnamed_4460': ('constant -1', 'GUESS unchanged'),
 'unnamed_4480': ('r=1.000 vs sim_t over 62510 samples (median 1687.2 s)',
                  'OBSERVED: session-time stamp confirmed (Monza, 2 runs; one track)'),
 'unnamed_4488': ('0/1', 'GUESS unchanged'),
 'unnamed_4490': ('constant 0', 'GUESS unchanged'),
 'command_queue_array': ('per-car constant pointer (19 values for 19 cars, 0 changes)',
                         'OBSERVED: finding f1c-command-queue-is-per-car-not-global-ring (CONFIRMED) + live per-car pointer'),
 'command_queue_capacity': ('constant 10 on every car', 'OBSERVED: finding f1c-command-queue-is-per-car-not-global-ring (CONFIRMED) + live 10'),
 'command_queue_write_index': ('all 62510 samples NaN in both runs', 'GUESS unchanged: not usable; recorder request added (0x44B0)'),
 'unnamed_44fc': ('{0,1,2}', 'GUESS unchanged'),
 'unnamed_4508': ('0..7595, 986 distinct, lap r=-0.31', 'GUESS unchanged'),
 'unnamed_450c': ('{0, 0.8636}, 2843 changes, lap r=-0.39', 'GUESS unchanged (two-state value)'),
 'unnamed_4524': ('0..204.4, 163 distinct', 'GUESS unchanged'),
 'unnamed_4528': ('raw float bits: +1.0 / -1.0 only', 'GUESS: meaning correction - sign scratch (+-1.0 float)'),
 'unnamed_452c': ('raw float bits: +1.0 / -1.0 only', 'GUESS: meaning correction - sign scratch (+-1.0 float)'),
 'unnamed_4544': ('0/1, 1742 changes; finding f1c-cmd-0xe-writes-veh-443c (CONFIRMED) gives the writers',
                  'OBSERVED: command-response flag (cmd 5 -> 1, cmd 0/14 -> 0)'),
 'unnamed_4554': ('constant 1.0 float (logged raw bits 1065353216) in all samples',
                  'GUESS: meaning correction - constant 1.0 multiplier in the corner-target chain (D1)'),
 'unnamed_455c': ('0..6 integers, battle r=0.45', 'GUESS: meaning correction - battle/steering state counter; D6 material'),
 'unnamed_4560': ('42.06..42.45, 30 distinct, no state correlation', 'GUESS unchanged (timer-like, not session time)'),
 'unnamed_4564': ('43.6..44.9, 45 distinct', 'GUESS unchanged'),
 'unnamed_4568': ('0/1, 27 changes', 'GUESS unchanged'),
 'unnamed_456c': ('0/1', 'GUESS unchanged'),
 'unnamed_4570': ('{0..6, 20, 21}, battle r=0.17', 'GUESS unchanged; D6 material'),
 'unnamed_4574': ('0/1, battle r=0.24', 'GUESS unchanged; D6 material'),
 'unnamed_4578': ('{0, 1, 256, 257}, battle r=0.28', 'GUESS: meaning correction - bitfield (bit0 and bit8), not a single flag'),
 'unnamed_4579': ('0/1, battle r=0.28', 'GUESS unchanged; D6 material'),
 'unnamed_4584': ('0/1, only 3 changes across the race, pit r=0.08',
                  'GUESS unchanged; rare toggle is consistent with a pit flag but not proof'),
 'unnamed_4590': ('constant 0', 'GUESS unchanged (not used in this session)'),
 'unnamed_4594': ('45.2..87.5, 34 distinct, 17 changes', 'GUESS unchanged (consistent with per-compound speed)'),
 'unnamed_45a0': ('{13, 14, 17, 18, 19, 20}', 'GUESS unchanged'),
 'unnamed_45e8': ('7.45..7.67, 10 distinct, constant per car', 'GUESS unchanged'),
 'unnamed_45f8': ('pointer-like 0x5F...., 19 distinct, per car', 'GUESS: meaning correction - per-car pointer scratch'),
 'unnamed_461c': ('constant 999999', 'GUESS unchanged'),
 'unnamed_4894': ('1..19 identity id, constant per car; the key matched against team records 99.99 %/100 % on 3 tracks',
                  'OBSERVED: driver/team identity key (meaning refined)'),
}


def load():
    recs = []
    for run in RUNS:
        p = Path(os.environ['TEMP']) / 'ai_brain_live' / run / 'overview.jsonl'
        with open(p, encoding='utf-8') as fh:
            for line in fh:
                if line.strip():
                    recs.append((run, json.loads(line)))
    return recs


def build_aux(recs):
    """Per-record state vector aligned with recs: brake/pit/battle/lap/sim/cumdist."""
    aux = []
    cum = {}
    last_t = {}
    for run, r in recs:
        fl = r.get('fields') or {}
        k = (run, r.get('slot'))
        t = r.get('sim_t') or 0.0
        d = cum.get(k, 0.0)
        dt = t - last_t.get(k, t)
        if 0.0 < dt < 5.0:
            d += (fl.get('speed') or 0.0) * dt
        cum[k] = d
        last_t[k] = t
        st = r.get('derived', {}).get('braking', {}).get('active')
        opp = r.get('opponents') or []
        aux.append(dict(
            brake=1.0 if st else 0.0,
            pit=1.0 if fl.get('driver_status') in (3, 4, 5) else 0.0,
            battle=1.0 if opp and min(o.get('dist_m', 1e9) for o in opp) < 15.0 else 0.0,
            lap=float(fl.get('lap_number') or 0),
            sim=float(t),
            cumdist=d,
        ))
    return aux


def q(xs, p):
    return xs[min(len(xs) - 1, int(p * len(xs)))]


def pearson(xs, ys):
    n = len(xs)
    if n < 10:
        return None
    mx = sum(xs) / n
    my = sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    if sxx <= 0 or syy <= 0:
        return None
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    return sxy / math.sqrt(sxx * syy)


def field_value(r, fid):
    fl = r.get('fields') or {}
    if fid not in fl:
        return None
    v = fl[fid]
    if isinstance(v, list):
        v = v[0] if v else None
    if isinstance(v, str):
        try:
            return int(v, 16)
        except ValueError:
            return None
    if isinstance(v, float) and not math.isfinite(v):
        return None
    return v


def main():
    abi = json.load(open(AI / 'ai_brain_abi_v1_1.json', encoding='utf-8'))
    guess = [f for f in abi['fields'] if f['tier'] == 'GUESS']
    recs = load()
    aux = build_aux(recs)
    stats = {}
    for f in guess:
        fid = f['id']
        xs2, ys2, last, chg = [], {k: [] for k in aux[0]}, {}, 0
        for i, (run, r) in enumerate(recs):
            v = field_value(r, fid)
            if v is None:
                continue
            if isinstance(v, float) and not math.isfinite(v):
                continue
            xs2.append(float(v))
            for k in aux[i]:
                ys2[k].append(aux[i][k])
            kk = (run, r.get('slot'))
            if kk in last and last[kk] != v:
                chg += 1
            last[kk] = v
        if not xs2:
            stats[fid] = dict(n=0, hex=f['hex'], base=f['base'], meaning=f['meaning'])
            continue
        xs = sorted(xs2)
        fin = [v for v in xs2 if math.isfinite(v)]
        intlike = all(abs(v - round(v)) < 1e-6 for v in fin)
        distinct = len({round(v, 6) for v in fin})
        corr = {k: pearson(xs2, ys2[k]) for k in ys2}
        corr = {k: v for k, v in corr.items() if v is not None}
        best = max(corr, key=lambda k: abs(corr[k])) if corr else None
        stats[fid] = dict(
            hex=f['hex'], base=f['base'], meaning=f['meaning'], n=len(xs2),
            min=xs[0], med=q(xs, 0.5), max=xs[-1], distinct=distinct, intlike=intlike,
            changes=chg, corr={k: round(v, 3) for k, v in corr.items()},
            best=best, best_r=round(corr[best], 3) if best else None,
        )

    out = []
    out.append('# GUESS_FIELDS - D3.2 statistics for the 57 GUESS fields in ABI v1.1\n')
    out.append('Data: `%TEMP%\\ai_brain_live\\watch_race4fullc` + `watch_race4fulld` `overview.jsonl`')
    out.append('(Monza, 1 Hz, all cars). Array fields use element 0 (nearest opponent). Correlation is')
    out.append('Pearson of the raw value against: brake (`derived.braking.active`), pit')
    out.append('(`driver_status` 3/4/5), battle (any opponent < 15 m), lap (`lap_number`), time')
    out.append('(`sim_t`), dist (integral of `speed` over each car\'s samples). Tiers feed D2;')
    out.append('"GUESS unchanged" means the logs neither prove nor contradict the documented meaning')
    out.append('strongly enough to raise it.\n')
    out.append('| field | hex | n | min | med | max | distinct | int | changes | strongest corr | data vs GUESS meaning | proposed |')
    out.append('|---|---|---|---|---|---|---|---|---|---|---|---|')
    for f in guess:
        fid = f['id']
        s = stats.get(fid, dict(n=0))
        read, proposal = ASSESS.get(fid, ('stats collected; assessment pending', 'GUESS unchanged'))
        if not s.get('n'):
            out.append('| %s | %s | 0 | - | - | - | - | - | - | - | %s | %s |' % (fid, f['hex'], read, proposal))
            continue
        out.append('| %s | %s | %d | %.4g | %.4g | %.4g | %d | %s | %d | %s %s | %s | %s |' % (
            fid, f['hex'], s['n'], s['min'], s['med'], s['max'], s['distinct'],
            'yes' if s['intlike'] else 'no', s['changes'],
            s['best'] or '-', ('r=%.2f' % s['best_r']) if s['best_r'] is not None else '', read, proposal))
    out.append('')
    out.append('Notes: `changes` counts per-car sample-to-sample value changes across both runs; a value')
    out.append('with 0 changes is constant in every sampled car. Pointer-like fields (`command_queue_array`,')
    out.append('`opp_slot_vehicle_ptr`) are compared as integers and correlated as raw addresses, so their')
    out.append('correlations are not meaningful. `unnamed_4554` is logged as raw float bits (D1).')
    (AI / 'phase3' / 'GUESS_FIELDS.md').write_text('\n'.join(out) + '\n', encoding='utf-8')
    json.dump(stats, open(CACHE / 'd3_guess_stats.json', 'w', encoding='utf-8'), indent=1)
    print('wrote GUESS_FIELDS.md; stats: %s' % (CACHE / 'd3_guess_stats.json'))


if __name__ == '__main__':
    main()
