# e5_branch_flag.py -- E5: explain the odd wp+0x134/0x138 records in the Monza 20-waypoint chains.
#
# Read-only. Reads situation snapshots of the Monza runs, builds the 20-entry chain from
# fields.current_waypoint_ptr + fields.lookahead_waypoints, joins to the logged waypoints and reports
# every record with branch_flag != 0 or branch_val != 0: value reinterpretations (int/float bits),
# chain neighbours and ptr deltas, position, and how often the same ptr appears (stationary pseudo
# node vs moving path node). Raw output -> %TEMP%.
import json
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
sys.path.insert(0, str(ROOT / 'investigations' / 'ai_brain_map' / 'phase2' / 'analysis'))
import c1b_lib as L  # noqa: E402

RUNS = ('watch_race4fullc', 'watch_race4fulld', 'watch_race3')


def fb(v):
    try:
        return struct.unpack('<f', struct.pack('<I', int(v) & 0xFFFFFFFF))[0]
    except Exception:
        return None


def main():
    odd = defaultdict(list)
    flag_counts = defaultdict(int)
    val_counts = defaultdict(int)
    seen_ptr = defaultdict(int)
    n_snap = 0
    for run in RUNS:
        for x in L.situations(run):
            for s in L.snaps(run, x['folder']):
                wps = s.get('waypoints') or []
                if not wps:
                    continue
                n_snap += 1
                by_ptr = {L.hx(w['ptr']): i for i, w in enumerate(wps) if w.get('ptr') is not None}
                chain = [L.hx(s['fields']['current_waypoint_ptr'])]
                chain += [L.hx(p) for p in s['fields'].get('lookahead_waypoints', [])]
                for ci, ptr in enumerate(chain):
                    i = by_ptr.get(ptr)
                    if i is None:
                        continue
                    w = wps[i]
                    bf = w.get('branch_flag')
                    bv = w.get('branch_val')
                    if bf is None:
                        continue
                    flag_counts[bf] += 1
                    val_counts[bv] += 1
                    if bf == 0 and bv == 0:
                        continue
                    rec = dict(run=run, driver=s['driver'], sim_t=s['sim_t'], chain_idx=ci,
                               ptr=ptr, bf=bf, bv=bv, x=w.get('x'), z=w.get('z'),
                               seg=w.get('seg_len'), sk=w.get('speed_k'),
                               prev=chain[ci - 1] if ci else None,
                               nxt=chain[ci + 1] if ci + 1 < len(chain) else None)
                    odd[(run, bf, bv)].append(rec)
                    seen_ptr[(run, ptr)] += 1
    print('== snapshots with chains: %d ==' % n_snap)
    print('branch_flag counts:', dict(sorted(flag_counts.items(), key=lambda kv: -kv[1])[:12]))
    print('branch_val  counts:', dict(sorted(val_counts.items(), key=lambda kv: -kv[1])[:12]))

    print()
    print('== (flag,val) groups ==')
    for (run, bf, bv), recs in sorted(odd.items(), key=lambda kv: -len(kv[1])):
        ptrs = sorted({r['ptr'] for r in recs})
        xs = [r['x'] for r in recs if r['x'] is not None]
        zs = [r['z'] for r in recs if r['z'] is not None]
        fvals = sorted({fb(r['bf']) for r in recs})
        print('%-20s flag=%-12d val=%-12d n=%5d distinct_ptr=%4d x %.1f..%.1f z %.1f..%.1f float(flag)~%s'
              % (run, bf, bv, len(recs), len(ptrs), min(xs), max(xs), min(zs), max(zs),
                 [round(v, 3) for v in fvals[:3]]))
        for r in recs[:4]:
            dprev = (r['ptr'] - r['prev']) if r['prev'] is not None else None
            dnxt = (r['nxt'] - r['ptr']) if r['nxt'] is not None else None
            print('   %-20s t=%.0f ci=%2d ptr=%#010x prev_d=%s next_d=%s x=%.2f z=%.2f seg=%.2f sk=%.3f'
                  % (r['driver'], r['sim_t'], r['chain_idx'], r['ptr'], dprev, dnxt,
                     r['x'] or 0, r['z'] or 0, r['seg'] or 0, r['sk'] or 0))

    print()
    print('== stride check: are odd ptrs 316-aligned to a common base? ==')
    ptrs = sorted({r['ptr'] for (run, bf, bv), recs in odd.items() for r in recs})
    if ptrs:
        base = ptrs[0]
        for p in ptrs[:30]:
            d = p - base
            print('ptr %#010x  delta_from_first=%d  mod316=%d' % (p, d, d % 316))
    print()
    print('== do odd ptrs recur across snapshots (stationary) ? ==')
    recur = sorted(((k, v) for k, v in seen_ptr.items() if v > 1), key=lambda kv: -kv[1])[:20]
    for (run, ptr), n in recur:
        print('%-20s ptr=%#010x n_snapshots=%d' % (run, ptr, n))


if __name__ == '__main__':
    main()
