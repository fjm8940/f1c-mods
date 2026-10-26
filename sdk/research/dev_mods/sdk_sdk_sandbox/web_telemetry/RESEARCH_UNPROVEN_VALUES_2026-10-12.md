# Web Telemetry: unproven-value research (Batch 2e), 2026-10-12

Status: read-only research for Claude review. No source edits (web_telemetry.c and telemetry_viewer.js are being edited by another agent), no game writes, no memory pokes, no install, no name_index.json tier edits. Weather and track-condition values are excluded (covered by the weather research agent).

Owner: Matias. Component: `mods/sdk_sandbox/web_telemetry`. Tiers: FACT / CONFIRMED / DERIVED / GUESS (plus OPEN and HEURISTIC where the code already uses them). Proposed tiers below are recommendations, not changes.

## 0. Evidence used and limits

Live, read-only (`http://127.0.0.1:8080`, build 4.4.0-rc.1, session generation 2, track 2002_Silverstone, sample age about 31 ms):
- `GET /telemetry`: 24 cars (player slot 0; 23 AI). Snapshot taken 2026-10-12 about 12:10.
- `GET /trace/status`: per-channel `unavailable` and `unavailable_why`.
- `GET /trace/data?car=B&lap=11`: 1573 recorded samples for AI car slot 9 (Marcel Lasee, 2002 Jordan), lap time 84.3 s. Fields used: `s` speed, `th`, `br`, `gl`, `go`, `gv`, `yr`, `y`, `st`, `rx`, `tt`.
- `GET /trace/data?car=B&lap=4,5,6,9,10`: returned `{"error":"lap not recorded"}` at 12:13, although `/trace/status` listed those laps at 12:10. Not investigated further (see section 6).

Offline:
- `web_telemetry.c`: capture code (`fill_v6_channels`, about line 3700), JSON writer (about lines 4530-4680), `car_telemetry_t` comments (about 855-885), RESEARCH block (about 557-575).
- `telemetry_viewer.js` (lines 13-17, 1295, 1672, 1795-1809, 2013-2034, 3416, 3579) and `workbench_ui.js` (lines 82-83, 819).
- `CHANNEL_LEDGER_2026-10-09.md` sections 1.1-1.3 and 7.
- `findings.jsonl`, `name_index.json` (`vehicle_fields`), `persistence_map.jsonl`, `data_index.json`.
- `f1c_mod.py start --json` (state BROKER_DETERMINISTIC_REGRESSIONS_GATED; freshness not stale), `evidence-query findings`, and `inspect` (inspect could not resolve the bare offsets 0x3580/0x358C by address; used name_index and persistence_map instead).

Scratch scripts (correlation and physics checks) live in `%TEMP%\w2e_scripts`, not in the workspace. Raw captures are in `%TEMP%` only, per the lean-workspace rule.

Not done, with reason:
- f1c-memory MCP reads: not invoked. No owner-run live window; the proofs below do not need them.
- Car `Mass` lookup in `.hdv` data for the static-weight calibration: not done; it is the input to owner test O1.
- `value_matches.json`: no entries found for the offsets in this batch.
- `inspect` by address: the recovery index has no entries for bare vehicle-field offsets.

Limits that cap every conclusion below: one AI car, one full lap (1573 samples), plus one 23-car instantaneous snapshot. Correlations are within-lap time series; they establish relationships and plausibility, not units or absolute names.

## 1. Inventory: values labelled unproven, unavailable, RESEARCH, DERIVED or heuristic

| # | Value | JSON key(s) | Offset / source | Tier as shown today | Where shown |
|---|---|---|---|---|---|
| 1 | Player steering input | `steer`, `steerwhy`, `vf` bit 8 clear | veh+0x3580 (no player writer found; persistence VOLATILE_BASELINE) | OPEN, "unproven for player" | viewer 1990, 3579; `/trace/status` `unavailable_why.steering`; `steerwhy` |
| 2 | AI steering input factor | `steer` (vf bit 8) | veh+0x3580 (`AIMainUpdate`) | FACT (AI) per ledger; sign "not independently proven" | viewer live and compare panels |
| 3 | Yaw rate | `yr` (trace only) | derived from heading matrix | DERIVED (method) | viewer 1672 "Yaw rate" (DERIVED); `/trace/status` yaw_rate |
| 4 | Heading angle | `yaw` (`/telemetry`), `y` (trace) | veh+0x34BC/34C4/34D4/34DC rotation matrix | FACT (matrix); angle sign convention unproven (C about 6379, console turn bucket) | `/telemetry` `yaw`; console |
| 5 | Vertical acceleration | `gv` | veh+0x3550 | FACT (AI), DERIVED (player) | `/telemetry` `gv`, trace `gv` |
| 6 | Suspension deflection candidate | `rx[0..3]` | AI wheel block +0x10 | RESEARCH | viewer 2027, 2043 |
| 7 | Vertical load candidate | `rx[4..7]` | AI wheel block +0x44; player W+796 (unit unknown) | RESEARCH | same |
| 8 | Slip-angle-like candidate | `rx[8..11]` | AI wheel block +0x38 | RESEARCH | same |
| 9 | Force parameter candidate | `rx[12..15]` | AI wheel block +0xD8 | RESEARCH | same |
| 10 | AI tyre temperature candidate | `ttc` (AI), trace `tt` | AI wheel block +0x50 read as Kelvin | RESEARCH; `unavailable_why.tyre_temp` says "read no plausible Kelvin value" | viewer 1795-1809 |
| 11 | Wheel order FL,FR,RL,RR (player thermal, brake temp) | `btmp`, `ttmp`, `ttc` (player) | 0x7C7A00 thermal block | DERIVED from HUD draw order (viewer 3416); block itself FACT | viewer brake and tyre panels |
| 12 | AI wheel-block index map (3,2,1,0 for FL,FR,RL,RR) | all AI `rx` arrays | C `wb_index` in `fill_v6_channels` | CONFIRMED by F1CUI labels only (ledger section 7) | the same panels |
| 13 | Raw engine int | `u4574` | veh+0x4574 | name_index GUESS, "DISPROVEN as seize flag"; shown raw | `/telemetry` `u4574` (0 on all 24 cars) |
| 14 | Pit watch | `boxsoon`, `boxwhy` | derived heuristic | GUESS by design (viewer 2386) | keep as is |
| 15 | Damage attribution | `cw` | proximity heuristic | HEURISTIC (C about 4650) | keep as is |
| 16 | Yaw-sign turn bucket | (console only) | depends on #4 sign | "unproven" (C about 6379) | console |

Derived UI panels that are labelled DERIVED but are not unproven physics (no change proposed): Delta vs A, Combined G, braking zones, wear per lap, mini-sector winner, physics-consistency checks.

## 2. Per-value evidence and proposed tiers

Each block gives: evidence obtained in this batch, proposed tier, what proof is still missing, and the cheapest next test.

### 2.1 Yaw rate `yr` (row 3): promote candidate

- Evidence (lap 11, n=1572 with `yr` present; 1 sample null): `yr` against `gl / s` (lateral accel over speed, native FACT lateral accel, right-positive): slope -1.021, correlation -0.971. Magnitude matches a_lat / v. Against the heading derivative `dy/dt` (heading from the matrix): correlation +0.951. Speed range in lap 22.5-86.2 m/s.
- Reading: `yr` is the heading derivative (consistent with the heading matrix) and its magnitude matches lateral accel over speed. Heading increases on left turns (gl negative).
- Proposed tier: DERIVED now; recommend CONFIRMED (two native sources, the heading matrix and the lateral-accel block, agree in magnitude and sign). Claude decides.
- Still missing: absolute naming of left and right in the game world.
- Cheapest test: owner drives one known left-hand and one known right-hand corner (Silverstone has both). Check the sign of `yr` and `gl`. Read-only, no poke.

### 2.2 AI steering sign (row 2): sign relation now evidenced

- Evidence (lap 11, n=1529 non-zero): `st` range -0.300 to +0.361. Correlation of `st` with `yr` +0.919, with `gl` -0.816. Positive steer input goes with positive yaw rate (heading increasing, left turn in the heading frame).
- Proposed tier: DERIVED for the sign relation. The magnitude stays an input factor, not degrees, per the ledger (FACT AI). Absolute left/right naming still needs the owner test in 2.1.
- Player steering (row 1): unchanged, OPEN. The live `/telemetry` reports `steer` 0.0 with `vf` bit 8 clear, and the code labels it invalid. No new evidence. The legacy writer evidence (player source P+0x000 filtered, P+0x040 unfiltered, legacy dossier STOP 3) is not live-verified. Cheapest test: owner drives left and right while a read-only capture of P+0x000 runs (owner-run; not run here).

### 2.3 Load candidate `rx[4..7]` (row 7): stays RESEARCH; left-right pairing supported

- Evidence, lap 11:
  - Summed load against speed squared: correlation +0.965. Mean summed load 15,319. Relative to a 600 kg static weight (5886 N) that is 2.6 times. Plausible as newtons with downforce; impossible as kgf. Twelve samples have a load of zero or less on some wheel (excluded from ratios).
  - Left-right pairing with C labels (left = FL+RL = rx4+rx6, right = FR+RR = rx5+rx7) against lateral accel `gl`: correlation +0.995. Outer-wheel loading under a right-positive lateral accel predicts this sign.
- Evidence, live snapshot (23 AI cars): left-right pairing is not separately checked there.
- Proposed tier: DERIVED ("load-like, scales with downforce, left-right pairing consistent"). Unit still unproven (N is plausible, not shown). Not promotable to FACT.
- Missing: unit (N or other), and an independent static-weight match.
- Cheapest test (owner O1): stand still on grid, engine running, no inputs, 10 s. Summed load should equal car plus fuel mass times 9.81 if the unit is N. The mass is in the car `.hdv` (lookup not yet done).

### 2.4 Front-rear labelling of AI load and suspension arrays (rows 6, 7, 12): SUSPECT, reverses C label

- Evidence, lap 11 (n=1573): with C labels (rear = RL+RR = rx6+rx7, front = FL+FR = rx4+rx5), rear-minus-front load against throttle is -0.757. Against brake it is +0.535, and against forward accel `go` it is +0.592.
  - Physics: throttle moves load rearward (rear up), brake moves it forward (front up). Both observed signs are the reverse of that, so the C front and rear labels for these arrays appear swapped.
  - The same reversal appears across 23 AI cars at one instant: rear-minus-front against throttle -0.623, against brake +0.745.
  - Per-wheel check: `rx4` (labelled FL) rises with throttle (+0.51, brake -0.17). `rx5` (FR) rises with throttle (+0.55). `rx6` (RL) and `rx7` (RR) show weak throttle dependence (+0.14, +0.27). Front and rear groups do not match their labels.
  - Mean load front (C labels) 9,170, rear 6,149: the labelled front carries more load on average, which is also the reverse of a normal rear-biased load split.
- Interpretation (inference, DERIVED): the AI block-index mapping `wb_index = {3,2,1,0}` assigns rear and front the wrong way round for these arrays, or the array is not vertical load. Left-right is not affected. Suspension (`rx[0..3]`) uses the same index mapping, so it is probably affected too.
- Proposed tier: DERIVED (against the current label). Downgrade the ledger's "CONFIRMED by labels" for front and rear only.
- Missing: decisive owner test, or static proof of the writer order.
- Cheapest test (O2, decisive, uses existing viewer checklist): one hard stop from about 60 m/s on a straight. The front pair should rise under brake. Compare with the labelled front pair. Also use the same run for the player `btmp` wheel order (2.7). Read-only capture, no code change needed.

### 2.5 Suspension deflection candidate `rx[0..3]` (row 6): stays RESEARCH, plausibility improved

- Evidence, lap 11: range 0.040 to 0.123 (mean about 0.068). Correlation with same-wheel load 0.87 to 0.93. Correlation with speed 0.72 to 0.89 (confounded by downforce). In metres, 4 to 12 cm is a plausible compression range; unit not shown.
- Proposed tier: DERIVED ("compression-like, follows load, metres plausible"). Not a named physics channel. Front-rear naming inherits 2.4.
- Cheapest test (O1): standstill static value. A loaded car should show a non-zero static compression. Also compare the front and rear static values once O2 settles the labels.

### 2.6 Slip-angle-like candidate `rx[8..11]` (row 8): stays RESEARCH, correlation only

- Evidence: correlation with lateral accel per wheel 0.64 to 0.98 (positive). Values about +/-0.08 (radian-scale plausible). For some samples the four wheels are almost identical, so this may be a car-level term rather than per-wheel slip.
- Proposed tier: RESEARCH with a DERIVED lateral correlation note. Not promotable.
- Cheapest test (owner O5): steady-state constant-radius corner (a skidpad-like lap section). Per-wheel slip should differ between front and rear and relate to steer and yaw rate.

### 2.7 Player wheel order (rows 11): stays DERIVED; owner test

- Evidence: player `btmp` reads 24.6 on all four wheels at rest (ambient-consistent). No order information in a stationary capture. The thermal block itself is FACT (ledger 0x7C7A00), and temperatures are converted from Kelvin with a sanity clamp in C.
- Proposed tier: DERIVED (unchanged). Order to be decided by the owner test.
- Cheapest test: O2 (the same hard stop). Also the viewer's own checklist item "One hard stop from top speed" (`BrakeTemp Max jumps, FL/FR hotter than RL/RR`, console about line 6447). That checklist is the existing owner-run test.

### 2.8 Force parameter candidate `rx[12..15]` (row 9): suspect, looks wrong as a force

- Evidence, lap 11: summed force correlates with `go` 0.076, `th` -0.094, `br` 0.064. A tyre longitudinal-force sum should track throttle and brake. It does not. Max |force| over load is 2.19, and any wheel's |force| exceeds its load in 44 percent of samples. Per-wheel force against same-wheel load ranges from -0.58 to +0.47 (sign-flipping, no stable relation). Mean forces are negative on three of four wheels.
- Proposed tier: OPEN, suspect. Recommend relabelling as "unclassified raw parameter" and dropping the word "force" from UI copy until a writer or consumer proof exists. Do not promote.
- Cheapest test: static read of the consumers of `WheelForceParameterSet_004f1ba0` output (read-only), plus O2 (brake and throttle sign test).

### 2.9 AI tyre temperature candidate `ttc` / trace `tt` (row 10): suspect, never valid in captures

- Evidence (FACT observation): trace `tt` is null in 1573 of 1573 samples of lap 11. `/telemetry` `ttc` is `[-999,-999,-999,-999]` for all 23 AI cars at the snapshot. `/trace/status` reports `tyre_temp` unavailable for slot B with the reason "read no plausible Kelvin value". The C gate accepts only 200-700 K (`fill_v6_channels`, about line 3730).
- Interpretation (inference): in this build, wheel+0x50 does not hold a Kelvin temperature at these times, or the flag gate is never met. The tier-RESEARCH temperature series is effectively empty.
- Proposed tier: observation FACT; claim that it is a tyre temperature is OPEN and should be dropped from user-facing copy until a writer proof exists.
- Cheapest test: read-only raw value of wheel+0x50 across AI cars, if the owner game is running (f1c-memory read-only), or static writers of +0x50. No owner action.

### 2.10 Unknown raw int `u4574` (row 13): relabel as raw

- Evidence: `name_index` GUESS, "DISPROVEN as seize flag" (live 2026-07-25 per name_index). The C string at about line 5170 says the same. Live `/telemetry` value is 0 on all 24 cars now.
- Proposed tier: GUESS (unchanged). Recommend labelling raw and not presenting it as a status flag.
- Test: none needed for promotion. Keep it raw.

## 3. Ranking

### 3.1 Promotable now, with the evidence above (Claude decides)
1. Yaw rate `yr`: DERIVED to CONFIRMED candidate (2.1). Two native sources agree in magnitude and sign.
2. AI steering sign relation: DERIVED sign relation (2.2). Magnitude stays an input factor.
3. Left-right pairing of AI load arrays (C labels FL+RL left): DERIVED, supported (2.3).
4. Load and suspension arrays: stay RESEARCH, but add "DERIVED: scales with downforce, follows load, plausible unit" (2.3, 2.5). Not FACT.

### 3.2 Need an owner live test
- O1 standstill calibration (2.3, 2.5): engine on, no inputs, 10 s on grid. Summed load against mass times 9.81. Static suspension values.
- O2 hard stop from about 60 m/s on a straight (2.4, 2.7): decisive for the front-rear label of the AI arrays and the player brake-temperature wheel order. Also matches the viewer's existing checklist.
- O3 known left and known right corner (2.1, 2.2): absolute left-right naming for `yr`, `gl`, `st`, heading sign.
- O4 player steering read-only capture while steering left and right (2.2). Player steering stays OPEN until this.
- O5 steady constant-radius corner (2.6): slip-like candidate.
- Force candidate (2.8) needs no owner test if the static consumer read and O2 agree.

### 3.3 Look wrong, remove or relabel (no code edit made here)
- AI tyre temperature `ttc` / `tt` (2.9): never valid in captures. Hide the series, or label it "no valid value", until a writer proof exists. Current copy already says "no plausible Kelvin".
- Force candidate `rx[12..15]` (2.8): relabel as "unclassified raw parameter". Drop "force" wording.
- Front and rear naming of AI `rx` arrays (2.4): the reversal suggests the C `wb_index` front and rear labels need correcting after O2. Do not edit until the owner test. Note the other agent is editing `web_telemetry.c`.
- `u4574` (2.10): raw GUESS; relabel and do not present as a status flag.
- Console yaw-sign turn bucket (row 16): keep the wording relative until O3.

## 4. Finding records appended

Appended to `findings.jsonl` (existing format, one object per line): `web-telemetry-b2e-yaw-rate-vs-lat-accel-20261012`, `web-telemetry-b2e-ai-steer-sign-20261012`, `web-telemetry-b2e-rx-load-leftright-20261012`, `web-telemetry-b2e-rx-susp-load-correlation-20261012`, `web-telemetry-b2e-rx-frontrear-label-suspect-20261012`, `web-telemetry-b2e-rx-force-candidate-suspect-20261012`, `web-telemetry-b2e-ai-tyre-temp-never-valid-20261012`. No `name_index.json` tier was edited.

## 5. Pending validation

- Every proposed tier here is a recommendation for Claude review.
- O1-O5 need the owner's live session. None was run from this agent.
- Units of the load and suspension candidates remain unproven.
- Absolute left-right naming remains unproven.

## 6. Limitations and open questions

- Single AI car, single lap (1573 samples), and one 23-car instantaneous snapshot. The front-rear reversal is consistent across both, but both are from one session.
- `/trace/status` listed laps 4, 5, 6, 9 and 10 for car B at 12:10, but `/trace/data` returned "lap not recorded" for all five at 12:13. Possible ring or generation inconsistency (`/trace/status` and `/trace/data` disagree). Not investigated; worth a separate read-only check.
- The `inspect` front door could not resolve bare vehicle-field offsets by address. The name_index entries were read directly.
- Weather and track-condition values are excluded by instruction.


## Claude review note (2026-10-12): trace anomaly

FACT (live /trace/status ~12:30): side B still slot 9, but rev rose to 7 (was 3 earlier), total_samples dropped 13996 -> 2444 and completed laps list is empty while open_lap=17. So the server trace for B was reset, which explains "lap not recorded" at 12:13. DERIVED cause: a /trace/select (or auto re-bind) call re-selected the side and cleared its laps, possibly an agent probing the live server. Follow-up for rc.3: re-selecting the same slot must not wipe recorded laps; test it.


## O2 result (owner test, Claude capture, 2026-10-12)

FACT: player btmp indices 0/1 = front, 2/3 = rear (hard stop 296->0 km/h: peaks 923/914 vs 652/644 C). Left/right not settled. AI rx front/rear not settled by a player test; next step = compare AI front/rear rx arrays in AI braking zones from /trace/data (read-only), no owner action needed. Finding web-telemetry-player-btmp-front-first-o2-20261012.


## O1/O3 result (rc.5, owner tests, Claude capture, 2026-10-12)

- Player standstill loads rx4..7 = 1413 / 1301 / 1772 / 1868 (sum 6354, about 648 kg if newtons). Rear pair heavier: indices 6,7 = rear (FACT); unit N DERIVED.
- Player turns: glat +16 loads 5,7; glat -15 loads 4,6. Clockwise track majority and owner left-then-right order give glat < 0 = right turn, so 4,6 = left (DERIVED). Player order = FL,FR,RL,RR, matching C labels. yr positive in a right turn.
- Player rx0..3 read 0 (no suspension candidate for the player).
- Finding web-telemetry-player-loads-glat-sign-o3-o1-20261012.

- Full-lap check (same day): Copse right -39 m/s2, Maggotts left +39, Becketts right -33, Chapel left +37, Priory/Brooklands left +31/+24, Luffield right -24. Left/right now FACT: glat < 0 = right turn, player loads FL,FR,RL,RR. Finding web-telemetry-glat-negative-right-turn-lap-20261012.
