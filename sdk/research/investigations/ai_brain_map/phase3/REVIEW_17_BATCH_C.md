# Claude review 17 — phase 3 batch C (E1-E5) + registry v1.4 (2026-10-02/03)

Verdict: **PASS with one override. Registry v1.4 built** (`ai_brain_abi_v1_4.json`, `phase3/merge_abi_v1_4.py`):
27 OBSERVED / 109 DERIVED / 4 GUESS (v1.3: 27 / 77 / 36). brain_core and viewer v3 use v1.4.

## Checks

- verify_maps (3 phases), test_abi (now also v1.4: same 140 fields and offsets), test_extract_offsets: exit 0.
- E4 hook site (capstone, Claude): `0x4253F3 mov eax,[esi+0x4550]; cmp eax,[0x5EA2CC]; jge 0x425408;
  0x425401 mov ecx,esi; 0x425403 call 0x4270A0 (AISteering)`; then 0x42540A steering law, 0x425411
  AISteeringCorrection, 0x425418 gear. The spec's hook point and its guard are correct.
- E3 override: `unnamed_4170` is the scheduled STALL race-event time (AIMainUpdate L80-L89 kind 4 branch, L218
  compare with session time, L234 FLT_MAX), not a "re-plan deadline". Other 35 entries accepted as DERIVED / GUESS.

## E1 qualifying predictions vs the live qualifying log (phase3/QUALI_RESULTS.md + Claude checks)

| # | prediction | result |
|---|---|---|
| 1 | no 4 s race pace pattern; ramp + clamp only | CONFIRMED (61 up, 13 clamp, 1 carConditions; no traffic steps) |
| 2 | mode 4 absent; mode 5 on off/recovery | half: mode 4 absent CONFIRMED; mode 5 = garage parking (0 -> 5 x30), recovery uses 6/7 |
| 3 | at most one box call per car | DISPROVEN: 38 call_to_box 0 -> 1 edges for 19 cars (runs end with a box call); pit_plan_count 4 constant |
| 4 | wet compound 4/5/6 in wet qualifying | NOT TESTED as stated: next_chosen_tires (next-stop choice) = 3 in all 21 774 samples; owner confirms the AI ran proper wet tyres, so id 3 is unmapped, not 'dry' (correction 2026-10-03) |
| 5 | best_lap only decreases | CONFIRMED (0 increases, 19 drivers) |
| 6 | Overtaking attempt modes rare; +-3 m only outside the flying-lap window | first half DISPROVEN (5->2 17, 2->7 22, 16->1 12 of 407); second half inverted - all 14 targets were set by cars INSIDE the window (lapOfRun 2, not boxed), as the static gate says |
| 7 | no ProximityScan auto-yield; any yield = Overtaking path | CONFIRMED (14/14 Overtaking session-5 gate) |
| 8 | QUALIFYINGABILITY only in simulated sessions | settled earlier (finding qualifyingability-simulated-sessions-only-2026-10-02) |
| 9 | qualifying settings file variant | not testable from brain logs |
| 10 | corner formula session-independent | CONFIRMED: qualifying brake zones n 402, game/model median 0.9998 (p10 0.9988, p90 1.0189) |

## Other accepted results

- E2 practice: garage 5 -> 3 -> 0, out lap ~106 s vs flying ~95 s, AIPushLap ramp 16/16, parking resets 0x453C to
  1.0, pit limiter 22.22 m/s held.
- E5: wp+0x134/0x138 are int32; non-1 values are inert unwritten slots. brain_core now adds `branch_event`
  (flag == 1); brain_explain already tests == 1.
- E4 write stage 1 spec draft: accepted as the basis for implementation (forced 0x4424 path first, call-site
  detour at 0x425403 keeping the 0x4550 guard, ramp <= 2 m/s, never 0x45FC self, 5 owner-gated steps). Before
  implementation: owner approval of the plan; build in `development/mods/` (dev DLL), not in the corpus.

## Open

- 4 GUESS left: 0x4000, 0x4040, 0x4364, 0x441C (static identity unresolved).
- Compound id mapping for 0x458C and the fitted-compound field (owner: AI ran proper wet tyres) - static follow-up.
