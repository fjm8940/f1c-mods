# merge_abi_v1_3.py - Claude, 2026-10-02 (REVIEW_16, batch B). Builds ai_brain_abi_v1_3.json from v1.2 +
# the 14 "promote-only" fields of phase3/GUESS_V12_NEXT.md (D15) + meaning updates from D4/D6/D7/D8/D10/D13.
# Evidence per entry is the batch-B report named in 'src'. Read-only on inputs; writes only the new registry.
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / 'ai_brain_abi_v1_2.json'
OUT = ROOT / 'ai_brain_abi_v1_3.json'

# id -> (new tier or None = keep, meaning, src)
UPDATES = {
    'unnamed_4404': ('OBSERVED', 'Next AIAbility deadline: AIMainUpdate writes session time + 4.0 before each AIAbility call '
                     '(4 s cadence; replay 91-94 % in-window).', 'phase3/PACE_MULTIPLIER.md (D4)'),
    'unnamed_4508': ('OBSERVED', 'Traffic accumulator incremented by AISteering; AIAbility steps the pace multiplier up when '
                     'non-zero, down when zero, then clears it (pace model explains 5465/5467 Monza, 1399/1399 wet).',
                     'phase3/PACE_MULTIPLIER.md (D4)'),
    'unnamed_450c': ('OBSERVED', 'Max race_ability_norm of traffic candidates seen by Overtaking; input of the AIAbility random '
                     'pace gate ((push norm - this*0.5)*100 >= rand%100 -> step up, else down); cleared by AIAbility.',
                     'phase3/PACE_MULTIPLIER.md (D4), OVERTAKE_DEFEND.md (D6)'),
    'unnamed_45f8': ('OBSERVED', 'Same-team peer vehicle pointer (AIDriverStructInit links the other active car with the same '
                     'Team string); AIAbility fuel/pit gate and Overtaking same-team fallback. Live: Massa -> Villeneuve.',
                     'REVIEW_15; finding ai-brain-same-team-peer-45f8-live-2026-10-02'),
    'unnamed_416c': ('DERIVED', 'Per-lap engine-life modulus budget used by AIMainUpdate (scripted engine-life slots).',
                     'phase3/DAMAGE_REACTION.md (D13)'),
    'unnamed_4488': ('DERIVED', 'Garage signature flag: 1 for a garaged car, 0 on track (finding garage-vs-track-signature).',
                     'phase3/GUESS_V12_NEXT.md (D15)'),
    'unnamed_44fc': ('DERIVED', 'Per-driver random variant chosen at profile load (sub_424080 rand%3); read by AISteering.',
                     'phase3/RANDOMNESS.md (D5)'),
    'unnamed_4564': ('DERIVED', 'Next start-throttle toggle time during the race-start branch: now + 1 + rand%100 * 0.02.',
                     'phase3/START_REACTION.md (D7)'),
    'unnamed_4568': ('DERIVED', 'Start-ramp direction flag: selects forward or reverse reading of the 26-step launch ramp '
                     '(0.0..0.949).', 'phase3/START_REACTION.md (D7)'),
    'unnamed_4578': ('DERIVED', 'Traffic pass/alongside state bits (bit0/bit8): set in attack modes 1/2 (54 %) and 4 (78 %); '
                     'not a single flag.', 'phase3/OVERTAKE_DEFEND.md (D6)'),
    'unnamed_4579': ('DERIVED', 'Byte of the pass/alongside state word (see unnamed_4578); fires in attack modes.',
                     'phase3/OVERTAKE_DEFEND.md (D6)'),
    'unnamed_4584': ('DERIVED', 'Pit-release flag: set by PitStopDecisionTireCompound, read by the post-pit cleanup.',
                     'phase3/PIT_LINK.md (D8)'),
    'unnamed_4594': ('DERIVED', 'Per-stop speed slot written by PitStopDecisionTireCompound.', 'phase3/PIT_LINK.md (D8)'),
    'unnamed_45a0': ('DERIVED', 'Next-stop lap target slots of the pit schedule.', 'phase3/PIT_LINK.md (D8)'),
    'unnamed_4454': (None, 'Cumulative distance-like accumulator of the waypoint locator (r = 0.95 vs the speed integral).',
                     'phase3/GUESS_FIELDS.md (D3.2)'),
    'opp_mode': (None, 'Per-opponent traffic mode in the 5 team records. Live (6745 transitions): 16 and 5 = approach / '
                 'hold (43.9 / 39.1 %); 1/2/7/8 = attempt / alongside; Overtaking runs for {2,7} and {1,8} with lateral '
                 '+3 m (mode 2) or -3 m (1,7,8). Side sign not proven.', 'phase3/OVERTAKE_DEFEND.md (D6)'),
    'published_lateral_target': (None, 'Final lateral offset the steering law follows (live-confirmed). Written only by the '
                                 'AISteering epilogue: copy of the working target, then the request if override_enable, then '
                                 'the forced value if non-zero (last wins); zeroed by RaceStructInit.',
                                 'phase3/POINTER_WRITERS.md (D10), Claude disassembly check REVIEW_16'),
    'steering_lock_divisor': (None, 'Steering-lock divisor; steer = heading error / this. Written once by AIDriverStructInit '
                              'from a per-car physics value (live constant 0.4014 rad, about 23 degrees); no per-tick writer.',
                              'phase3/POINTER_WRITERS.md (D10)'),
    'car_conditions': (None, 'Vehicle condition status; value 2 = retired/pit mode: the car coasts dead (no AI retire '
                       'decision of its own) and the game keeps re-retiring it (limbo) until cleared.',
                       'phase3/DAMAGE_REACTION.md (D13)'),
}


def main():
    data = json.loads(SRC.read_text(encoding='utf-8'))
    by_id = {f['id']: f for f in data['fields']}
    for fid, (tier, meaning, src) in UPDATES.items():
        f = by_id[fid]
        if tier:
            f['tier'] = tier
        f['meaning'] = meaning
        f['source'] = '%s; %s (Claude REVIEW_16)' % (f['source'], src)
    for fid, w in (('published_lateral_target', ['AISteering', 'RaceStructInit']),
                   ('steering_lock_divisor', ['AIDriverStructInit'])):
        by_id[fid]['writers'] = w
        by_id[fid]['writer'] = w[0]
    data['version'] = '1.3'
    data['generated_by'] = 'phase3/merge_abi_v1_3.py (Claude, REVIEW_16, 2026-10-02)'
    data['generated_from'] = [SRC.name, 'phase3 batch B reports (D4-D15)']
    OUT.write_text(json.dumps(data, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    tiers = {}
    for f in data['fields']:
        tiers[f['tier']] = tiers.get(f['tier'], 0) + 1
    print('updated', len(UPDATES), 'tiers', tiers)


if __name__ == '__main__':
    main()
