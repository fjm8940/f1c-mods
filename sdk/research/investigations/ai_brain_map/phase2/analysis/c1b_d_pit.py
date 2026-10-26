# c1b_d_pit.py — Run D from C1b: pit situations, mode 0x4510 sequence, speed limit, car_conditions.
# Read-only over %TEMP% watch folders. Prints a short per-run result.
#
# NOTES.md (owner): in watch_race2 the old watcher cut pit_* on speed clamp 0x4484 held >=3 s
# (mostly a traffic clamp); fixed code in watch_race2b cuts on driver_status 3/4/5 or call_to_box.
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L


def compress(seq):
    out = []
    for v in seq:
        if not out or out[-1][0] != v:
            out.append([v, 1])
        else:
            out[-1][1] += 1
    return '>'.join('%s x%d' % (a, b) for a, b in out)


def main():
    for run in L.RUNS:
        pit = [x for x in L.situations(run) if x['kind'] == 'pit']
        real, clamp_only, empty = 0, 0, 0
        mode_seqs = {}
        rows = []
        for x in pit:
            seq = []
            real_s = False
            for s in L.snaps(run, x['folder']):
                f = s['fields']
                seq.append(s)
                if f['driver_status'] in (3, 4, 5) or f['call_to_box']:
                    real_s = True
            if not seq:
                empty += 1
                continue
            real += bool(real_s)
            clamp_only += (not real_s)
            modes = [s['fields']['driver_status'] for s in seq]
            key = compress(modes)
            mode_seqs[key] = mode_seqs.get(key, 0) + 1
            if real_s:
                cc = [s['fields']['car_conditions'] for s in seq]
                sl = [s['fields']['speed_limit'] for s in seq]
                rows.append(dict(folder=x['folder'], driver=x['driver'], t0=round(x['start'], 1),
                                 dur=round(x['end'] - x['start'], 1),
                                 modes=compress([m for m in modes if m != 0] or [0]),
                                 cc=compress(cc), sl_min=round(min(sl), 1), sl_max=round(max(sl), 1),
                                 c2b=sum(1 for s in seq if s['fields']['call_to_box'])))
        print('== Run D pit [%s] situations=%d real=%d speed-clamp-only=%d empty=%d ==' % (run, len(pit), real, clamp_only, empty))
        print(' mode sequences (driver_status, zeros skipped): %s' % sorted(mode_seqs.items(), key=lambda kv: -kv[1])[:5])
        for r in rows[:10]:
            print('  %s %s dur=%.1fs modes=%s cc[%s] sl=%.1f..%.1f call_to_box=%d'
                  % (r['folder'], r['driver'], r['dur'], r['modes'], r['cc'], r['sl_min'], r['sl_max'], r['c2b']))
        if len(rows) > 10:
            print('  ... %d more real pit situations' % (len(rows) - 10))
        # driver_status history from events, tail of the run
        ds = [e for e in L.events(run) if e.get('kind') == 'driver_status']
        tail = ds[-12:]
        print(' driver_status events total=%d, last %d:' % (len(ds), len(tail)))
        for e in tail:
            print('   sim %8.1f %-22s %s -> %s (%.0f km/h)' % (e.get('sim_t', -1), e.get('driver', '?'),
                                                               e.get('old'), e.get('new'), e.get('speed_kmh', -1)))
        mode_counts = {}
        for e in ds:
            mode_counts[e.get('new')] = mode_counts.get(e.get('new'), 0) + 1
        print(' driver_status new-value counts: %s' % dict(sorted(mode_counts.items(), key=lambda kv: str(kv[0]))))


if __name__ == '__main__':
    main()
