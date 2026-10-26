# c1b2_fields.py — check RECORDER_REQUESTS P1-P3 fields in watch_race3 (C7 follow-up).
# Read-only over %TEMP% watch folders. Prints a short status.
import json
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import c1b_lib as L

RUN = 'watch_race3'


def main():
    base = L.run_dir(RUN)
    h = json.load(open(base / 'header.json', encoding='utf-8'))
    wp_counts = Counter()
    extra_keys = Counter()
    pit_speeds = Counter()
    branch_ok = 0
    wp_n = 0
    pair_match = pair_total = rank_eq = no_rec = 0
    tpfe = Counter()
    for x in L.situations(RUN):
        for s in L.snaps(RUN, x['folder']):
            wp = s.get('waypoints') or []
            wp_counts[len(wp)] += 1
            if wp:
                wp_n += 1
                branch_ok += all(k in wp[0] for k in ('seg_len', 'speed_k', 'k112', 'k116', 'k120', 'branch_flag', 'branch_val'))
            ex = s.get('extra') or {}
            extra_keys.update(ex.keys())
            if ex.get('pit_speed_global') is not None:
                pit_speeds[round(ex['pit_speed_global'], 3)] += 1
            if x['kind'] == 'battle' and ex.get('slot_record_index'):
                sri = ex['slot_record_index']
                for o in s.get('opponents', []):
                    k = o['rank']
                    if k >= len(sri):
                        continue
                    pair_total += 1
                    if sri[k] == o.get('team_record'):
                        pair_match += 1
                    if sri[k] == k:
                        rank_eq += 1
                    if sri[k] == -1:
                        no_rec += 1
            tpfe[s['fields'].get('track_part_from_pit_exit')] += 1
    print('== RECORDER_REQUESTS field check [%s] header=%s abi=%s hz=%s ==' % (RUN, h.get('source'), h.get('abi'), h.get('hz')))
    print(' A1 k112/k116/k120 present on every sampled waypoint: %s (n=%d)' % (bool(wp_n and branch_ok), wp_n))
    print(' A2 waypoint records per snapshot: %s' % dict(wp_counts))
    print(' A3 branch_flag/branch_val present (raw +308/+312 ints): %s' % bool(wp_n and branch_ok))
    print(' A4 extra.pit_speed_global values: %s' % dict(pit_speeds))
    print(' C1 extra.slot_record_index present: %s; record selector == team-id match %d/%d, == rank %d, -1 %d'
          % (bool(extra_keys.get('slot_record_index')), pair_match, pair_total, rank_eq, no_rec))
    print(' C2 candidate metrics +0x04/+0x08/+0x1C: %s' % ('present' if any(k.startswith('cand') for k in extra_keys) else 'NOT logged'))
    print(' D1 extra.pit_team_index present: %s' % bool(extra_keys.get('pit_team_index')))
    print(' D2 pit-lane/track-part field 0x444C values (unresolved): %s' % dict(tpfe.most_common(3)))
    print(' S1 sampling: hz=%s (40 Hz not used)' % h.get('hz'))


if __name__ == '__main__':
    main()
