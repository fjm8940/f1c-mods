# d3_callees.py -- D3.1: recompute the 51 second-level unmapped callees (phase-2 COVERAGE logic)
# from the read-only corpus cache, print a triage table, and generate phase3/maps/offsets_*.csv
# and unmatched_*.txt for each function. Read-only over functions/.
import csv
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(r'%USERPROFILE%\Desktop\F1\f1c_ida_decompiled')
AI = ROOT / 'investigations' / 'ai_brain_map'
PH2 = AI / 'phase2'
MAPS_OUT = AI / 'phase3' / 'maps'
CACHE = Path(os.environ['TEMP']) / 'ai_brain_phase2'
FUNCTIONS = ROOT / 'functions'
sys.path.insert(0, str(AI))
import extract_offsets  # noqa: E402

CLAUDE_DOC54 = {'AIMainUpdate', 'sub_426430', 'sub_426100', 'AISteeringCorrection',
                'SteeringAngleController', 'sub_4258C0'}


def strip_parts(stem):
    return re.sub(r'_part\d+$', '', stem)


def sources():
    out = {}
    for p in FUNCTIONS.glob('*.c'):
        m = re.match(r'^(.*)_([0-9a-fA-F]{8})$', p.stem)
        out[m.group(1) if m else p.stem] = (int(m.group(2), 16) if m else None, p)
    return out


def main():
    calls = [json.loads(l) for l in open(CACHE / 'all_calls.jsonl', encoding='utf-8') if l.strip()]
    func_lines = json.load(open(CACHE / 'func_lines.json', encoding='utf-8'))
    mapped = set(CLAUDE_DOC54)
    for p in AI.glob('map_*.md'):
        mapped.add(strip_parts(p.stem[4:]))
    for p in (PH2 / 'maps').glob('map_*.md'):
        mapped.add(strip_parts(p.stem[4:]))
    second = sorted({c['expr'] for c in calls
                     if c['func'] in mapped and c['expr'] in func_lines and c['expr'] not in mapped})
    src = sources()
    targets = [n for n in second if n in src]
    print('second-level unmapped callees (corpus): %d' % len(second))
    print('with .c file: %d' % len(targets))
    callers = {}
    for c in calls:
        if c['expr'] in targets:
            callers.setdefault(c['expr'], set()).add(c['func'])
    csv_calls = {}
    rows = list(csv.DictReader(open(AI / 'offsets_ALL.csv', encoding='utf-8', newline='')))
    for r in rows:
        if r['access'] == 'CALL' and r['func'] in targets:
            csv_calls.setdefault(r['func'], set()).add(r['expr'])

    MAPS_OUT.mkdir(parents=True, exist_ok=True)
    info = {}
    for name in targets:
        addr, path = src[name]
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
        try:
            rows_out = extract_offsets.extract_file(path, MAPS_OUT / ('unmatched_%s.txt' % name))
        except Exception as exc:  # keep going; report the failure
            rows_out = []
            print('EXTRACT FAIL %s: %s' % (name, exc))
        for row in rows_out:
            if row['dec'].startswith('-'):
                row['hex'] = ''
        with open(MAPS_OUT / ('offsets_%s.csv' % name), 'w', encoding='utf-8', newline='') as fh:
            w = csv.DictWriter(fh, fieldnames=extract_offsets.FIELDS, lineterminator='\n')
            w.writeheader()
            w.writerows(rows_out)
        info[name] = dict(addr=addr, lines=len(lines), sig=(lines[0].strip() if lines else ''),
                          callers=sorted(callers.get(name, ())), calls=sorted(csv_calls.get(name, ())),
                          rows=len(rows_out))
        print('%-42s 0x%08X L%-4d callers=%-2d calls=%-2d csvrows=%-3d | %s'
              % (name, addr, len(lines), len(info[name]['callers']), len(info[name]['calls']),
                 len(rows_out), (lines[0].strip()[:70] if lines else '')))
        print('    callers: %s' % ', '.join(info[name]['callers'][:12]))
        if info[name]['calls']:
            print('    calls:   %s' % ', '.join(info[name]['calls'][:12]))
    json.dump(info, open(CACHE / 'd3_callees_info.json', 'w', encoding='utf-8'), indent=1)
    print('wrote %d offset CSVs + unmatched files to phase3/maps/' % len(targets))
    print('json: %s' % (CACHE / 'd3_callees_info.json'))


if __name__ == '__main__':
    main()
