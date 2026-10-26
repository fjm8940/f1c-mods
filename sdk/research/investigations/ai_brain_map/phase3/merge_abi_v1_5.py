# merge_abi_v1_5.py - ABI v1.4 -> v1.5: value-type fixes only (Claude, REVIEW_18, 2026-10-03).
# Trigger: owner live screenshots of AI Brain viewer v3 showed float bit patterns as ints and an int lap as a
# denormal float. Every change below is read straight from the decompile (tier of the meaning is unchanged).
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
SRC, DST = HERE / 'ai_brain_abi_v1_4.json', HERE / 'ai_brain_abi_v1_5.json'

FIXES = {
    # hex: (new type, units or None = keep, evidence)
    '0x4450': ('float', 'm', 'sub_426430_00426430.c L112 copies waypoint +72 (float lap distance) into it; '
                             'L150 reads it back as *(float *)(this + 17488)'),
    '0x4528': ('float', None, 'sub_426430_00426430.c L190/L215 write 1065353216 / -1082130432 = +1.0f / -1.0f'),
    '0x452C': ('float', None, 'sub_426430_00426430.c L189/L210 write 1065353216 / -1082130432 = +1.0f / -1.0f'),
    '0x441C': ('float', None, 'sub_426430_00426430.c L300 *(float *)(this + 17436) = ratio expression'),
    '0x4590': ('int', 'lap', 'sub_429960_00429960.c L165/L252-254 this[4452] = dword TotalRaceLaps, (int) compare '
                             'and (double)(int) cast; finding f1c-gdb-pitstopstrategies-parsed-into-vehicle-struct'),
}


def main():
    d = json.loads(SRC.read_text(encoding='utf-8'))
    done = []
    for f in d['fields']:
        fx = FIXES.get(f['hex'].upper().replace('0X', '0x'))
        if not fx:
            continue
        old = f['type']
        f['type'] = fx[0]
        if fx[1]:
            f['units'] = fx[1]
        f['source'] = f.get('source', '') + '; REVIEW_18 type fix (%s -> %s): %s' % (old, fx[0], fx[2])
        done.append(f['hex'])
    missing = set(FIXES) - {h.upper().replace('0X', '0x') for h in done}
    assert not missing, missing
    d['version'] = '1.5'
    d['generated_by'] = 'phase3/merge_abi_v1_5.py (Claude, REVIEW_18, 2026-10-03)'
    DST.write_text(json.dumps(d, indent=1, ensure_ascii=False) + '\n', encoding='utf-8', newline='\n')
    print('wrote', DST.name, 'fixed', done)


if __name__ == '__main__':
    main()
