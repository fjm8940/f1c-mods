# merge_abi_v1_4.py - Claude, 2026-10-02 (REVIEW_17, batch C). Builds ai_brain_abi_v1_4.json from v1.3 +
# DeepSeek E3 proposal phase3/abi_v1_4_proposal.json (36 static reads; 32 GUESS -> DERIVED) with Claude overrides.
# Read-only on inputs; writes only the new registry.
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / 'ai_brain_abi_v1_3.json'
PROP = HERE / 'abi_v1_4_proposal.json'
OUT = ROOT / 'ai_brain_abi_v1_4.json'

# Claude overrides (REVIEW_17): id -> (tier, meaning, source)
OVERRIDE = {
    'unnamed_4170': ('DERIVED', 'Scheduled STALL race-event time: AIMainUpdate L89 writes session time + 10 + (rand & 0x14) '
                     'when a RACEEVENTS STALL entry has lap key 0; L218 compares it with session time; FLT_MAX = disarmed. '
                     '(DeepSeek E3 "re-plan deadline" rejected; IR 2026-08-18 gave this write an offset 8 bytes too low.)',
                     'functions/AIMainUpdate_00424e70.c L80-L89, L218, L234 (Claude REVIEW_17)'),
}


def main():
    data = json.loads(SRC.read_text(encoding='utf-8'))
    prop = json.loads(PROP.read_text(encoding='utf-8'))
    by_id = {f['id']: f for f in data['fields']}
    n = 0
    for e in prop['entries']:
        f = by_id.get(e['id'])
        if f is None or f['tier'] != 'GUESS':
            continue
        note = (e.get('note') or '').replace('Meaning refined: ', '').replace('Meaning correction: ', '')
        if e['id'] in OVERRIDE:
            tier, meaning, src = OVERRIDE[e['id']]
        else:
            tier, meaning, src = e['new_tier'], note or f['meaning'], 'phase3/GUESS_V13_SETTLED.md (DeepSeek E3)'
        f['tier'] = tier
        f['meaning'] = meaning
        f['source'] = '%s; %s (REVIEW_17)' % (f['source'], src)
        n += 1
    data['version'] = '1.4'
    data['generated_by'] = 'phase3/merge_abi_v1_4.py (Claude, REVIEW_17, 2026-10-02)'
    data['generated_from'] = [SRC.name, 'phase3/abi_v1_4_proposal.json']
    OUT.write_text(json.dumps(data, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    tiers = {}
    for f in data['fields']:
        tiers[f['tier']] = tiers.get(f['tier'], 0) + 1
    print('updated', n, 'tiers', tiers)


if __name__ == '__main__':
    main()
