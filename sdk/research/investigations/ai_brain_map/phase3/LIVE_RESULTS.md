# LIVE_RESULTS — phase 3 short summaries


## D1 — corner-speed target vs 0x43FC (Monza watch_race4fullc + watch_race4fulld) — PASS

- Static: `AIComputePassScore 0x42A780`, ecx = vehicle base, `TireForceSolver` this = veh+0xC, args
  `(wp+0x74, wp+0x78 ptr, wp+0x70, 16.0)`; exactly 4 direct callers, all AIRacingLine; no indirect pointers.
- Bypasses: `wp+0x134 == 1` -> `fild wp+0x138` (limiting-wp target) or `flt_8ACFB4` (projection loop);
  linked-car block -> fixed 12.0; chain test uses `speed * 1.05`.
- Model `phase3/analysis/tireforcesolver.py` (table index selector multiplies A, not B; tables
  veh 0x3A7C..0x3D50 + snapshot `extra.v3520`); replay `phase3/analysis/d1_corner_speed.py`.
- Replay: n=4242, meas/pred median **0.9993**, p10 0.9976, p90 1.0001, **100% within +-2%**;
  all 18 drivers 0.9987..0.9996; no speed bin or car dependence.
- `0x4554` = 1.0 in every sample (logged as raw int bits); doc-54 formula == code formula.
- Excluded: 683 branch samples; 291 ticks with limiting wp beyond the 20 logged chain entries;
  85343 ticks with null limiting waypoint (9999). SC/RF not marked in the logs; formula is a
  deterministic function of logged state, nothing was filtered on SC/RF.
- Verdict PASS, no constant tuning. Detail: `phase3/D1_CORNER_SPEED.md`.

## D2 — ABI v1.2 tier proposal (no ABI edit; Claude decides)

- `phase3/abi_v1_2_tier_proposal.json` + `phase3/TIER_REFRESH.md`: 22 entries = 7 OBSERVED raises,
  14 meaning corrections (tier unchanged), 1 recorder observation.
- OBSERVED raises: `unnamed_4480` (r=1.000 vs sim_t), `unnamed_4894`/`opp_team_id`/`opp_team_match_flag`
  (3-track record pairing), `command_queue_array`/`command_queue_capacity` (CONFIRMED queue finding + live),
  `unnamed_4544` (CONFIRMED cmd-0xe finding + live 0/1).
- Meaning corrections include `unnamed_4454` = cumulative distance-like (r=0.95 vs speed integral),
  `unnamed_444c` = enum {2,4,5}, `unnamed_4528/452c` = +-1.0 sign scratch, `unnamed_4578` = bitfield.
- `command_queue_write_index` reads NaN live; recorder request R1 (`phase3/RECORDER_REQUESTS.md`).
- `test_abi.py` re-run: still passes (ABI untouched).

## D3.1 — the 51 second-level callees mapped (terse triage maps)

- `phase3/analysis/d3_callees.py` recomputed the list (51 with `.c` files), generated
  `phase3/maps/offsets_*.csv` + `unmatched_*.txt`; `d3_make_maps.py` wrote 51 maps.
- AI-relevant ones read in full and mapped with purpose/relevance: AIDriverSorting, AIDelegateCall,
  ClampedIndexAssignment, CommandQueueResponseHandler, DriverSetupParamLookupByID, PitStopBlockingCondition,
  PositionChangeMonitor, RandomVariationScaler (D5 input), SpeedDeltaComputation, StateChangeNotify,
  StateFlagsLookup, sub_4BE760 (AI pace anchor candidate), TeammatePartValueLookup, TireCompoundNameIDMapping.
- Pure math/CRT/render/network/physics helpers got 3-line maps (purpose, AI relevance none/low, callers).
- `phase3/verify_maps_phase3.py` (copy of the phase-2 verifier): 51 maps checked, 417 table offsets,
  0 mismatches, exit 0.

## D3.2 — 57 GUESS fields statistics (Monza 1 Hz, n=62510)

  Stats: min/med/max, distinct, integer-like, per-car changes, correlation vs brake/pit/battle/lap/time/distance.
- Highlights: `unnamed_4480` r=1.000 vs sim_t; `unnamed_4454` r=0.95 vs cumulative distance;
  `unnamed_455c` battle r=0.45, `unnamed_4570/74/78/79` battle r=0.17..0.28 (D6 material);
  `command_queue_capacity` constant 10; `unnamed_4400/4364/441c/45f8` pointer-like; `unnamed_444c` {2,4,5}.
- All findings feed the D2 proposal above.

## BATCH B live summaries (2026-10-02, offline replays)

## D11a — wet Imola (watch_20261002T194606Z)
- D1 corner target: PASS n=733 med 0.9993 p10 0.9985 p90 0.9999 100 % within ±2 % (second track + wet).
- Braking step (tireforce.py): n=2042 med 1.000, 74.1 % within ±20 % (last-step spread as on Monza).
- Wet vs dry Imola: braking decel med 21.1 vs 31.8 m/s^2 (0.664); corner targets 75.1 % of dry at
  22 matched waypoints; lap pace 119.6 % (best_lap 119.4 %).
- Tyre tables: 59/67 named TireForce/Solver entries change (all 5 selector+table blocks, T3740,
  T3919/T3921; force tables ~0.68 wet, T3740/T3921 ~1.41). Detail `D11_WET.md`.

## D4 — pace 0x453C (Monza + wet)
- Monza 5467 events: 5465 explained (two-write model), 4.1 s cadence; 2 outliers = stopped-car 1.0
  (MOD-suspect). Wet 1399/1399 explained. No rubber-banding (gap-leader r=0.10/0.05); traffic-driven
  (pace <15 m 0.9869 vs >100 m 0.9603 on Monza). Detail `PACE_MULTIPLIER.md`.

## D6 — opponent modes (6 race folders)
- 6745 transitions: 16 43.9 %, 5 39.1 %, 2 6.3 %, 1 5.2 %, 7/8 2.3 %, 4/3 0.9 %. 5<->16 churn
  (dt 0.2-0.3 s); attack entries 16->2/1; 2->7 / 1->8 side conversions. 0x4578/79 fire in 1/2 (54 %)
  and 4 (78 %). 0x45FC set at 0/6745 transitions. Detail `OVERTAKE_DEFEND.md`.

## D7 — starts (Imola + Monza race_start)
- Field launches together: time to >1 m/s after first mover med 0.10 s p90 0.15 s; only Klien +3.55 s
  (known stuck/reverse incident). Throttle applied 8.4-9.8 s before movement (countdown ramp
  0..0.949, phases 1-3 s). cap 0x44B8 constant per driver 1.000..1.192. Detail `START_REACTION.md`.

## D8 — pits (40 + 40 situations)
- Modes 0/4 only (3/5 never). call_to_box rises in mode 0, clears in mode 4 (67 edges). Limiter
  22.22 (Imola) / 27.78 m/s (Monza). Cursor 0->1 per stop; plan count 2/3; next_chosen_tires = 3 in
  all dry samples. Doc 53 Q2: 0x458C is an int compound id (`2.8e-45` = int 5). Detail `PIT_LINK.md`.

## D9 — traction control (Monza)
- Trigger (slip < 0.9 & speed > 4) 5.44 % of 90559 samples, gears 1-2 (20.5/15.8 %); brake_on_slip
  never fired (0/90559); throttle med 0.430 vs 0.428 non-trigger. Cut magnitude needs R2. Detail
  `TRACTION_CONTROL.md`.

## BATCH C live summaries (2026-10-02, offline static)

## E1 — qualifying session gates (static)
- 139 reads of 0x7C56A4 in the .text corpus, 0 writes; AI brain: race-only AIAbility pace block,
  non-race AIPushLap +0.003*laps ramp, reset mode 4 race / 5 non-race, qualifying plan rows in
  sub_429960 cases 3/4/5, Overtaking session-5 ServerSessionGateCheck (lap 2 + call_to_box).
- QUALIFYINGABILITY 0x44C0 is CONSUMED (sub_4BE760, RandomVariationScaler non-race), contradicting the
  IR prior-art "parser only"; parser writes (0.5 default, (22-QA)/22) confirmed.
- 10 testable predictions for the qualifying log in `QUALI_STATIC.md` section G. No live work done.

## E2 — practice (wet Imola) offline analysis
- Garage exit 5->3->0 (mode 3 5-6 s drive from the box at 63-180 m from pit exit), entry 0->5, no
  mode 4. Out-laps 103.7-107.7 s vs flying 94.5-97.6 s (+8..13 s, 10 cars).
- AIPushLap non-race ramp CONFIRMED 16/16 (`min(prev+0.003*min(lap,10), base+push)`); park entry
  resets 0x453C to 1.0 (explains D4's stopped-car outliers); wet base/push 0.9923/0.0052 for all
  cars (dry Monza 0.9603/0.0266). Pit limiter 22.22 respected (max +0.63 m/s).
- interaction_target 0x45FC nonzero 14x (2 backmarkers, laps 9-11); unnamed_4554 = 1.0.
  Detail `D11_PRACTICE.md`.

## E3 — GUESS static settling (offline)
- 36 v1.3 GUESS fields read statically; proposal: 32 -> DERIVED, 4 stay GUESS (0x4000, 0x4040,
  0x4364, 0x441C). Highlights: 0x413C = AIPushLap command deadline (now+2.0); 0x4170 = re-plan
  deadline; 0x4454 = 0x4450 + 0x4458*seg; 0x4570/74 = opponent id + 1; 0x4490 = heading Y;
  0x4590 = pit stop-lap array; 0x4554 corner multiplier. Detail `GUESS_V13_SETTLED.md`,
  machine-readable `abi_v1_4_proposal.json` (validated, hex-checked).

## E4 — write stage 1 spec (offline, no code)
- Chain: 0x4420 zeroed at 0x4253D5 unless 0x45FC / carConditions 2 / fuel-rate 0x3984<=1e-3;
  epilogue 0x4293FE -> 0x42940C -> 0x42942B sets 0x4408 = 0x440C, then request (if 0x4548), then
  forced 0x4424 last. Recommended hook: 5-byte detour at the AISteering call 0x425403 (after
  stage 5, before the epilogue); post-hook alone is not deterministic for 0x4424 (AIRacingLine
  rewrites it at stage 5).
- Ramp <= 2 m/s; steer law +-0.1/tick, look-ahead 0-3 wp. Never 0x45FC self pointer (crash finding).
- 5 owner-gated test steps. Detail `WRITE_STAGE1_SPEC_DRAFT.md`.

## E5 — branch_flag (wp+0x134/0x138) anomaly
- Type: int32 (consumer 0x425DA8/0x425E81 cmp ==1, 0x425DB1 fild; parser sub_402690 L296-303 writes
  (int)wp_event / wp_wpse). brain_core int32 decode correct; no type change.
- Odd values are unwritten slots on real waypoints without wp_event/wp_wpse (Monza mostly 0; Monaco
  float-like content tracking +0x48 within ~111 m). Consumer inert except ==1. Recommended:
  `branch_event = (flag==1)` filter. Detail `BRANCH_FLAG_CHECK.md`.
