# LIVE_RESULTS — phase 2 short summaries

<= 15 lines per run, per handoff. Raw JSONL stays in `%TEMP%\ai_brain_live\`.

## Run smoke (recorder) — Christian Klien + Jenson Button, 8 s @ 20 Hz

- Folder: `%TEMP%\ai_brain_live\smoke`, schema `f1c.ai_brain_log.v1`, game PID 28124, 316 snapshots, 57 events.
- Track unknown (header has no track field); race session, dry, both cars mode 0, conditions 0.
- Klien: accelerating 129.7 -> 266.2 km/h, no braking event; throttle caps 0.75/1.0; look-ahead index 2 -> 3.
- Button: braking 293.5 -> 102 km/h across two zones. Flicker at 256.3 -> 246.7 km/h: limiting waypoint switches 0x19A2DB5C/0x19A2DC98, then null for 10 ticks (0.5 s) with brake 0 and throttle 0.68, then rebinds 0x19A2F058.
- State combos (Button): lim+brake 42, lim+no-brake 0, null+brake 7 (all speed-limit 0x4484 braking at 15..25 m/s), null+no-brake 109.
- Detail: `phase2/C2_BRAKING_FLICKER.md`.

## Run viewer_20261002T151813Z (GUI) — Christian Klien, ~3.2 s

- Folder: `%TEMP%\ai_brain_live\viewer_20261002T151813Z`, source `ai_brain_gui.py`, 64 snapshots, 19 events.
- Klien braking 172.8 -> 98.4 km/h into a corner (target 103.6 km/h shown as 28.8 m/s).
- Flicker 122.4 -> 101.1 km/h: 8 braking events in ~1 s; limiting waypoint 0x19A331C4 set on 4 ticks, null on the ticks between; cmd_brake 0.20/0.40 when set, 0 when null.
- lim+brake 8, lim+no-brake 0, null+brake 0, null+no-brake 56; brake transitions 9.
- Conclusion (Claude review 6 correction): brake ticks == limiting-waypoint ticks; the null ticks are the planner choosing throttle, i.e. the native bang-bang brake/throttle decision around the target, not a field-reset artefact.

## C1b Run A — braking look-ahead + projection formula (watch_race2 + watch_race2b, brake_zone)
- Folder data: `%TEMP%\ai_brain_live\watch_race2` + `watch_race2b`, 46 + 49 brake_zone situations, 193 + 266 zones, 19 AI cars, one track (header has no track field; race2/2b are start/restart of the same race). Script: `analysis/c1b_a_braking.py`.
- Braking start: `braking_target_speed < speed` 459/459, `cmd_brake > 0` 459/459, `cmd_throttle == 0` 454/459. PASS.
- Profile: `profile[0] == speed` exactly in all 1500 braking samples; non-increasing 1500/1500; pad step exactly `profile[k] = profile[k-1] - 1.8` (n=821, err 0.000). PASS.
- Formula `len/profile[k-2]*0x44CC*target*0.915` vs the last pre-limiter step: measured loss is only p10..p90 0.00..0.12 of the prediction (median 0.06, n=821, 100% < 0.3) with len = logged waypoint chord (5-6 m) and target = `braking_target_speed`. The quantity at `wp+0x3C` (60) is NOT logged, so the map's `wp_len(wp+60)` reading is unconfirmed; a 5-6 m chord cannot reproduce the observed ~0.1-0.6 m/s tail step. OPEN, needs wp+60 / per-waypoint targets logged.
- Physical decel during zones: median 31.1 (race2) / 32.5 (race2b) m/s^2, p90 ~41, max 56; formula-implied decel `0x44CC*target*0.915` median 41.3 / 47.4 m/s^2 (same order, formula ~1.3x higher).
- No waypoint crossing ever coincided with the previously limiting waypoint (5682 changes, 0 hits): the planner clears 0x43F8 before the car reaches it, so formula-vs-measured per-segment loss is not directly observable in these logs.

## C1b Run B — steering law (brake_zone, all cars)
- `derived.steering_target.law_matches`: watch_race2 11585/11606 (99.8%), watch_race2b 11031/11045 (99.9%). Script: `analysis/c1b_b_steering.py`.
- abs(pred_cmd - cmd_steer): median 0.0033 / 0.0026, p90 0.0175 / 0.0144, max 0.458 / 0.279; correlation 0.9919 / 0.9945.
- `steering_lock_divisor` = 0.4014 constant (min=med=max). Look-ahead index vs speed rule 99.0% / 98.8% (misses at band boundaries). No mode-6 samples in brake zones.

## C1b Run C — opponent slots + modes (battle situations)
- `opp_slot_distance_sq` non-decreasing by rank: 10376/10376 (race2) and 11606/11606 (race2b) -> nearest-first CONFIRMED. Script: `analysis/c1b_c_battle.py`.
- At 1081 rank-driver changes (race2b): `opp_team_id[k]` unchanged 75.6%, `opp_mode[k]` unchanged 95.3% (baseline 99.2% / 98.6%). race2: 458 changes, 58.3% / 88.0% (baseline 99.6% / 97.4%). The value at index k tracks the persistent 28-byte team record, not the rank-k driver; brain_core's rank pairing is an approximation (slot+0x40 team index is not logged).
- Modes by rank (race2b): 5 x39890, 16 x7840, 2 x1776, 1 x1243, 8 x568, 7 x434, 4 x87; top transitions 16>5, 5>16, 5>2, 2>5, 16>2, 5>1, 1>8. Side-by-side <5 m: 2/7/16/4/5 (race2), 5/7/16/2 (race2b).

## C1b Run D — pit situations
- watch_race2 (old watcher): 2/2 pit situations are speed-clamp-only (no mode 3/4/5, no call_to_box) — confirms the NOTES warning. Script: `analysis/c1b_d_pit.py`.
- watch_race2b (fixed watcher): 40/40 cut on call_to_box or mode 4. Sequences `0>4>0` (e.g. Sato sim 759-872, Coulthard 767-857, Monteiro 781-872); held `speed_limit` 22.2 m/s (= 80 km/h pit limiter) on pit-lane passes, 15.8-20.2 m/s traffic clamps elsewhere. `car_conditions` stays 0 through the pit stop.
- Modes 3 and 5 never appear anywhere in either run; mode 2 appears (18 events). Post-race pit-in: Raikkonen 0->4 at sim 5102.3, 4->0 at 5111.7; Sato/Heidfeld to sim 5311 (passing the finish then boxing).

## C1b Run E — trouble / retire
- trouble: watchdog rises to 45-50 then mode 1 recovery stop (15/17 with wd>0; 1.1-6.2 s, entered at 283-301 km/h). Script: `analysis/c1b_e_trouble.py`.
- Klien start incident (race2, sim 15-27): 0->6 (5 km/h), 6->7, 7->0, 0->6 with watchdog 0 — the stuck/reverse branch, not the watchdog; race2b keeps him in mode 6 with `car_conditions` 2 (limbo/retire).
- retire: `car_conditions` 0->2; 3/6 with watchdog>0, mode 1 before 2 for Alonso/Fisichella/Massa; Trulli 0->2 with mode 0 (silent retire). Situation coverage: all five kinds occurred; missing sub-cases: modes 3/5, mode-2 handover (only 25 samples), a second track.

## C1b2 Run A — braking-loss formula re-test, Monaco, seg_len logged (watch_race3)
- Data: `%TEMP%\ai_brain_live\watch_race3`, 30 brake_zone situations, 479 active samples, 20-waypoint records, `seg_len` (wp+0x3C), `speed_k`, k112/116/120, branch fields; script `analysis/c1b2_a_formula.py`. `seg_len` ~5.0-5.6 m, matches the waypoint chord.
- Documented formula `loss = seg_len/v * 0x44CC * target * 0.915` with target = live 0x43FC: last pre-limiter step measured/pred p10=0.022, med=0.060, p90=0.143 (n=472) — UNCHANGED from the chord test. The formula is NOT the generator of the live profile; the log-side length proxy was not the problem.
- Implied factor (loss*v/(seg_len*0x44CC*0.915)) by step position: first 22.2, mid 28.4, last 1.64 m/s (p10 0.67 / p90 5.9). The last step behaves as if seg_len were ~0.33 m at both Imola and Monaco, regardless of the logged 25-41 m/s target.
- GUESS from the shape: the true factor vanishes as the projection reaches the corner target (constant-decel model disproven; a force-like factor fits the "ease into target" tail). Needs a static re-read of AIRacingLine L233-L272 with x87/ST0 tracking; the decompiled `v32` attribution at L264 is the prime suspect.
- Physical decel unchanged: median ~31-33 m/s^2 vs formula-implied 41-47.

## C1b2 Run B — steering law on track 2 (Monaco, watch_race3)
- 8297 brake_zone samples: law_matches 8001/8297 (96.4%), abs(pred-cmd) med=0.0053 p90=0.0278 max=1.147, corr=0.819; lock 0.4014 constant; look-ahead index rule 98.0%. Script: `analysis/c1b2_b_steering.py`.
- Misses concentrate at low speed: 216/543 (39.8%) below 15 m/s vs 22/5694 (0.4%) at 30-65 m/s and 0/460 above 65; 124 samples clamp |steer| >= 0.99. The law holds at racing speed; Monaco's slow hairpin exceeds the pure-pursuit prediction (rate limit/clipping), which is why the aggregate rate is below Imola's 99.8%.

## C1b2 — RECORDER_REQUESTS P1-P3 field check (watch_race3, script `analysis/c1b2_fields.py`)
- A1 k112/k116/k120 on every waypoint (38386 waypoint records): PRESENT. A2 20 waypoint records per snapshot (27 empty snapshots = no chain, pit/stopped): PRESENT. A3 `branch_flag`/`branch_val` (+308/+312 ints): PRESENT. A4 `extra.pit_speed_global` = 16.667 m/s (60 km/h Monaco, track-specific): PRESENT.
- C1 `extra.slot_record_index`: PRESENT, and it equals the team-id-matched record index in 68165/68169 samples (99.99%) while only 25.5% equal the rank — Claude's team-record pairing fix is live-validated; the old rank pairing was wrong ~74% of the time.
- C2 candidate metrics +0x04/+0x08/+0x1C: NOT logged. D1 `extra.pit_team_index` (0x48A4): PRESENT. D2 pit-lane/0x444C: still unresolved raw ints. S1: 20 Hz, no 40 Hz run.

## C8 — static re-derivation + dropped-ST0 scan (2026-10-02)

- C8.1 TireForce 0x4E9100 confirmed from capstone: the AIRacingLine step at 0x425ED9 uses the dropped TireForce return (`fmul st(1)` at 0x425EFC), exactly as REVIEW_10 found. Inside TireForce the three `sub_5895A2` (MSVC `_frnd`) calls ALSO have their returns dropped by Hex-Rays: the rounded value is stored and used for the table index, while the pseudo-code uses the raw value. Same pattern in TireForceSolver (5 calls) and AerodynamicForceCalc (1).
- C8.2 `analysis/tireforce.py` implements the asm-faithful model (this = veh+0xC, tables + wp+124/128) with `MISSING` = 16 field groups; self-test runs. It cannot be evaluated on the recorded logs: see RECORDER_REQUESTS A5.
- C8.3 re-run on watch_race3 (`analysis/c8_a_retest.py`): implied TireForce per position first med 22.2 / mid 28.4 / last 1.64; per-driver fit F ~ A*v^2+B median R^2=0.48 (16 drivers). Direct meas/pred with tireforce.py: NOT COMPUTABLE (vehicle tire tables not logged); race2b skipped (old schema).
- C8.4 dropped-ST0 scan (`analysis/c8_sto_scan.py`, report `DROPPED_ST0_SCAN.md`): 14 confirmed call sites across AIRacingLine, AIComputePassScore, TireForce, TireForceSolver, AerodynamicForceCalc and the CRT `_frnd`; 2 scanner false positives (return used in a `>=` expression; return deliberately discarded with `fstp st(0)`). New finding: the two AIRacingLine `AerodynamicForceCalc` conditions (L80, L190) test the call return, not the `v8`/`v23*0.3` arguments.

## C9 — dropped-ST0 map notes + next-race re-test (2026-10-02)

- Added an additive "Claude review (dropped ST0)" section to the six affected maps (no tier changes): `map_AIRacingLine`, `map_AIComputePassScore`, `map_TireForce`, `map_TireForceSolver`, `map_AerodynamicForceCalc` (phase 1) and `phase2/maps/map_sub_5895A2`; before/after sha256 recorded in `DS_LOG.md`. False-positive maps untouched.
- `analysis/tireforce.py` table model refined: each lookup is one 15-float array interpolated between adjacent entries; `MISSING` = 15 groups.
- `analysis/c9_a_retest.py` ready: replays every braking step with the TireForce model and prints meas/pred overall/by position (+-20% share) once a race has `cars_static.jsonl` and waypoint `k7c`/`k80`. Fail-closed verified: run against `watch_race3` and auto-discovery both report BLOCKED (no `cars_static.jsonl` yet).

## C9 A re-test — Monza (watch_race4fulld, cars_static.jsonl + k7c/k80) — FORMULA CONFIRMED

- `analysis/c9_a_retest.py watch_race4fulld` (Claude's JSONL parser fix, REVIEW_12): corrected step `v -= seg_len/v * 0x44CC * TireForce(v, wp+116, wp+120, wp+112) * 0.915` replayed with `analysis/tireforce.py` fed from `cars_static.jsonl`.
- meas/pred: all n=1936 median **1.000**, 85.4% within +-20%; first n=514 med 1.000 80.5%; mid n=1083 med **1.001**, 93.4%; last n=339 med 1.000 67.3%.
- Last-step spread is ratio noise on tiny absolute losses (median measured loss 0.18 m/s, mean abs err 0.18 m/s); the low/high outliers are not branch_flag==1 or target==12.0 cases.
- This closes the C1b Run A OPEN item: the REVIEW_10 TireForce correction plus the A5 static fields reproduce the live projection.

## C1b4 — Monza A-E (watch_race4fullc start + watch_race4fulld rest; script c1b4_monza.py)

- c (start, sim 8.6-53.6, 19 race_start situations): A 24 zones all checks pass (24/24 tgt<speed, brake>0, throttle 0), decel med 29.1; B steering 99.9% (804 samples, corr 0.972, idx rule 97.5%); C 5373 battle samples nearest-first 100%, slot_record_index==team match 16800/16800; D none; E 1 trouble (mode 6 x164, no watchdog).
- d (rest, sim ~65-3358, 126 situations): A 218 zones, tgt<speed 218/218, brake>0 218/218, throttle 0 206/218, decel med 26.4 p90 38.3; B steering 99.4% (9316 samples, corr 0.974, idx rule 98.3%); C nearest-first 100%, slot_record_index==team match 63191/63191 (==rank only 27.5%); D 40 real pit situations (call_to_box), mode-4 waits, `pit_speed_global` 27.78 m/s = 100 km/h (Monza), car_conditions==2 never in a pit; E trouble 9 (6 with watchdog, max 50, mode 1 x122, mode 6 x5555), retire 1 (mode 6, car_conditions 2).
- Coverage: c has no pit/retire, d has no race_start; all five kinds occur across the race. NOTES: SC/RF mods may be active - no whole-field-bunched period is claimed as vanilla.
