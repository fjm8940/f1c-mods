# merge_abi_v1_2.py - Claude, 2026-10-02 (REVIEW_15). Builds ai_brain_abi_v1_2.json from v1.1 +
# DeepSeek's D2 proposal (phase3/abi_v1_2_tier_proposal.json, reviewed) + Claude's prior-art corrections
# (phase3/PRIOR_ART_MAP_2026-10-02.md, IR = investigations/ai_interaction_role/README.md line numbers).
# Read-only on every input; writes only the new registry file.
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / 'ai_brain_abi_v1_1.json'
PROP = HERE / 'abi_v1_2_tier_proposal.json'
OUT = ROOT / 'ai_brain_abi_v1_2.json'

# DeepSeek entries rejected or replaced by Claude (reason in REVIEW_15_STOP2.md)
REJECT = {'unnamed_45f8'}

IR = 'investigations/ai_interaction_role/README.md'
# Claude corrections: id -> dict of overrides (meaning/type/tier/source appended)
CLAUDE = {
    'unnamed_45f8': dict(type='_DWORD', units='pointer', meaning=(
        'Same-team peer vehicle pointer (AIDriverStructInit links the last other active car with the same Team '
        'string); AIAbility uses it for a race fuel/pit gate and Overtaking for its same-team fallback path.'),
        src=IR + ':1490,2097'),
    'command_queue_write_index': dict(type='int', meaning=(
        'Per-car command queue write index (int; -1 = empty). Logged as float before v1.2, which showed -1 as NaN '
        '(phase3 RECORDER_REQUESTS R1).'), src='phase3/RECORDER_REQUESTS.md R1'),
    'unnamed_4554': dict(type='float', meaning=(
        'Multiplier on the limiting-waypoint corner target in AIRacingLine; 1.0 in every Monza sample (D1). '
        'Logged as int bits before v1.2.'), src='phase3/D1_CORNER_SPEED.md'),
    'brake_decel_factor': dict(meaning=(
        'Per-driver factor in the braking look-ahead step; it is the .rcd PASSING value normalized as '
        'min(1, 0.9 + 0.001*Passing) by the profile parser sub_424080 (live 0.971-1.000).'),
        src=IR + ':1548,1589; mods/sdk_sandbox/ai_mind_monitor/AI_RCD_AND_PASSING_NOTES.md'),
    'race_ability_norm': dict(meaning=(
        '.rcd RACEABILITY normalized (22 - R)/22; AIRacingLine uses 0.2 * this as the normal brake-demand ramp step.'),
        src=IR + ':1589,2080'),
    'corner_speed_force_offset': dict(meaning='.rcd CORNERINGADD (direct). Offset term of the per-waypoint corner target.',
                                      src=IR + ':1548'),
    'corner_speed_force_coeff': dict(meaning='.rcd CORNERINGMULT (direct). Multiplier of the tyre-force term of the corner target.',
                                     src=IR + ':1548'),
    'corner_speed_scale': dict(tier='OBSERVED', meaning=(
        'Wet factor = max(0.5, 1 - wetness * rain_ability_scaled), recomputed by AIPushLap; multiplies the corner '
        'target. Live: 1.0 dry Monza, 0.9754 wet Imola (implied wetness 0.80).'),
        src=IR + ':6024; finding ai-brain-wet-factor-4580-live-2026-10-02'),
    'ai_pace_multiplier': dict(meaning=(
        'Pace multiplier in the corner target. AIAbility (every 4 s, not per tick) steps it by 0.01 toward the '
        'baseline ability_baseline_scalar when no traffic was processed, else toward baseline + race_ability_push_scalar; '
        'a random gate against the max opponent ability seen in traffic can move it either way. Reset to the baseline by '
        'LateralForceTorqueCalculation(.,100).'), src=IR + ':1828,1875,2409,6396'),
    'unnamed_4508': dict(meaning='Traffic-processing counter incremented by AISteering; AIAbility uses it for the pace step and clears it.',
                         src=IR + ':1828'),
    'unnamed_450c': dict(meaning='Max race_ability_norm of traffic candidates seen by Overtaking; AIAbility uses it in the random pace gate and clears it.',
                         src=IR + ':1828'),
    'unnamed_4404': dict(meaning='Next AIAbility deadline: AIMainUpdate writes session time + 4.0 before each AIAbility call.',
                         src=IR + ':1875'),
    'time_margin': dict(meaning=(
        'Phase-change random factor written by AIRingBufferFieldInit: 0.15 + 0.15*randomTable - 0.1*difficulty '
        '(not a ring buffer, consumers not recovered).'), src=IR + ':2393'),
    'throttle_cap_race_start': dict(meaning=(
        'Start ramp factor: .rcd STARTSDRY/STARTSWET blended by wetness plus bounded random, value*0.1 + 1.0; '
        'used as the longitudinal ramp and throttle cap while the start-state byte is set.'), src=IR + ':1548,5059'),
    'driver_status': dict(meaning=(
        'AI mode: 0 race, 1 recovery stop (brake 1, throttle 0; to 2 below 4 m/s or both slips > 0.9), '
        '2 one-tick reset (writes 0, skips that tick commit, outputs keep last value), 3 route-distance check then 0, '
        '4/5 pit/garage wait (park), 6 reverse toward waypoint, 7 stop (brake 1) then 0 below 0.2 m/s. '
        'AI ticks every 25 ms, so a 20 Hz log can miss mode 2.'), src=IR + ':1712,1736,1747,2345,6130'),
    'interaction_target': dict(meaning=(
        'Interaction/yield target pointer. Writers: Overtaking (candidate a lap ahead, or same-team peer) and the '
        'ProximityScan auto-yield (only when the lapping car is the player, dist^2 < 625). While set, the lateral '
        'request survives the per-tick reset and AISteeringCorrection caps throttle at difficulty * 0.75. '
        'Not an overtake flag; never write a self pointer (crash finding).'), src=IR + ':100,6593,6646'),
    'throttle_cap_difficulty': dict(meaning='Throttle cap = AIDifficultyScalar * 0.75, set while interaction_target is non-null (yield/traffic lift).',
                                    src=IR + ':6593'),
}


def main():
    data = json.loads(SRC.read_text(encoding='utf-8'))
    prop = json.loads(PROP.read_text(encoding='utf-8'))
    by_id = {f['id']: f for f in data['fields']}
    applied, rejected = [], []
    for e in prop['entries']:
        f = by_id.get(e['id'])
        if f is None or e['id'] in REJECT:
            rejected.append(e['id'])
            continue
        if e.get('new_tier') in ('OBSERVED', 'DERIVED', 'GUESS'):
            f['tier'] = e['new_tier']
        if e.get('old_tier') == e.get('new_tier') and e.get('note'):
            note = e['note'].replace('Meaning correction candidate: ', '').replace('Meaning correction: ', '')
            f['meaning'] = '%s v1.2 live note (Monza, D3.2): %s' % (f['meaning'], note)
        f['source'] = '%s; phase3/abi_v1_2_tier_proposal.json (DeepSeek D2, REVIEW_15)' % f['source']
        applied.append(e['id'])
    for fid, o in CLAUDE.items():
        f = by_id[fid]
        for k in ('type', 'units', 'meaning', 'tier'):
            if k in o:
                f[k] = o[k]
        f['source'] = '%s; %s (Claude REVIEW_15)' % (f['source'], o['src'])
    data['version'] = '1.2'
    data['generated_by'] = 'phase3/merge_abi_v1_2.py (Claude, REVIEW_15, 2026-10-02)'
    data['generated_from'] = [str(SRC.name), 'phase3/abi_v1_2_tier_proposal.json', 'phase3/PRIOR_ART_MAP_2026-10-02.md']
    OUT.write_text(json.dumps(data, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    tiers = {}
    for f in data['fields']:
        tiers[f['tier']] = tiers.get(f['tier'], 0) + 1
    print('applied DS:', len(applied), 'rejected DS:', rejected, 'claude:', len(CLAUDE), 'tiers:', tiers)


if __name__ == '__main__':
    main()
