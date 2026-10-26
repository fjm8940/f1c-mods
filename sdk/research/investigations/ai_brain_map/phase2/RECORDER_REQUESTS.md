# RECORDER_REQUESTS — fields the C1b A/C/D analyses missed (C7, 2026-10-02)

Created from `REVIEW_9_C1B.md` (C7). Purpose: close the OPEN items left by the C1b
unattended watch analyses on the next recorded race. This file is a request list
only: no recorder/script was edited here (Claude owns `brain_core.py`,
`brain_watch.py`, `ai_brain_gui.py`). All reads stay read-only
(`OpenProcess 0x0410`), no game writes. Recording needs the owner's race, as
before.

Already accepted from the review: `seg_len (wp+0x3C)` and `speed_k (wp+0x84)` are
now in brain_core's waypoint record for the **first 6** chain waypoints per
snapshot. The requests below are what is still missing.

## A — braking projection formula (phase2/LIVE_RESULTS.md Run A, OPEN)

The recurrence `profile[j] = profile[j-1] - seg_len/ profile[j-1] * 0x44CC *
v_corner(wp_j) * 0.915` cannot be checked beyond the limiting waypoint step
because only the limiting waypoint's target `0x43FC` is logged and only 6
waypoints carry geometry.

| # | Prio | Field | Address | Why |
|---|---|---|---|---|
| A1 | P1 | per-waypoint corner target `v_corner`, or the raw inputs to `AIComputePassScore`: `TireForceSolver` args (3 floats) | `wp+112`, `wp+116`, `wp+120` (0x70/0x74/0x78) per logged waypoint | With `0x4500/0x4504/0x4580/0x453C/0x4554` (already logged) plus `speed_k (wp+0x84)`, the target for every chain waypoint can be recomputed and the full 20-slot projection replayed tick by tick |
| A2 | P1 | waypoint record for all 20 chain entries (or an opt-in `--waypoints N` focused mode) | chain 0..19 | The first-6 cap leaves profile slots 6..19 without `seg_len`/`speed_k`/target; the deep slots are where the projection shape matters most |
| A3 | P1 | per-car waypoint mode fields `wp+308`, `wp+312` | `wp+0x134`, `wp+0x138` per logged waypoint | These select the two branches that bypass `AIComputePassScore` (`flt_8ACFB4` pit speed; fixed `12.0` on a blocked branch); without them a projection mismatch cannot be attributed |
| A4 | P2 | global pit speed `flt_8ACFB4` | 0x8ACFB4 (float) | The `wp+308==1` branch target; one read per snapshot is enough (or confirm it equals the 22.2 m/s pit limiter seen in Run D) |

Not needed for A: speed_profile (logged), 0x43F8/0x43FC (logged), 0x44CC (logged),
cmd_throttle/cmd_brake (logged), 0x44B8/0x44C4 (logged as caps).

### A5 — TireForce / TireForceSolver vehicle tables (C8, P1)

The corrected braking step needs `TireForce(v, wp+116, wp+120, wp+112)` (finding
`ai-brain-braking-step-tireforce-correction-2026-10-02`), and its model
(`analysis/tireforce.py`) reads per-car tables from `veh+0xC`. None of them are logged.

| # | Prio | Fields | Addresses (vehicle-relative) | Why |
|---|---|---|---|---|
| A5 | P1 | tire model scalars | `0x3520`, `0x3524`, `0x3A7C`, `0x3C08`, `0x3C48`, `0x3C88`, `0x3D48`, `0x3D50` (floats) | TireForce 0x4E9100: `this[3397/3398/3740/3839/3855/3871/3919/3921]` |
| A5 | P1 | 3 lookup tables, 15 floats each | `0x3C0C..0x3C44`, `0x3C4C..0x3C84`, `0x3C8C..0x3CC4` | interpolation tables (`this[3840]`, `[3856]`, `[3872]` + next row) |
| A5 | P1 | TireForceSolver selector + table | `0x3D08` (float), `0x3D0C..0x3D44` (15 floats) | TireForceSolver 0x4E8CE0 `this[3903]`, `[3904..3918]` (corner-speed path) |
| A5 | P1 | waypoint floats | `wp+0x7C`, `wp+0x80` (wp+124, wp+128) | TireForce `a4[1]`, `a4[2]` (logged waypoint records need two more floats) |

These are latched tire-model values: one read per car per run (e.g. into the snapshot
`extra` for the first tick) is enough; no 20 Hz logging needed.

Status 2026-10-02 (REVIEW_11): implemented by Claude - `cars_static.jsonl` (raw veh
0x3A7C..0x3D50 per car every 60 s), snapshots carry `wp+0x7C`/`0x80` as `k7c`/`k80`
and `extra.v3520`/`v3524`. Re-test script `analysis/c9_a_retest.py` is ready; not yet
live-tested (game in menus).

## C — opponent slots / mode pairing (phase2/LIVE_RESULTS.md Run C)

The rank-paired mode display is an approximation: the native mode belongs to the
28-byte team record selected by the candidate slot's `+0x40` team index. Claude
fixed brain_core to pair by team id (`veh+0x4894 == opp_team_id[k]`), live-checked
on 4 cars; the field below makes that check routine and per-tick.

| # | Prio | Field | Address | Why |
|---|---|---|---|---|
| C1 | P1 | candidate slot team index, 5 dwords | `veh+0x41B4 + 0x44*k` (k=0..4) | The actual selector for the team record (`0x42CC + 0x1C*idx`, mode at +8); lets the GUI/logs pair each candidate with its own record instead of assuming rank == record |
| C2 | P2 | candidate entry metrics, 5 x 3 floats | `veh+0x4178 + 0x44*k` (distance/progress-like), `veh+0x417C + 0x44*k` (lateral-offset-like), `veh+0x4190 + 0x44*k` (closing/relative-motion-like) | The mode state machine's branch inputs (`AI_MODE_DECISION_NOTES.md`); joins battle mode transitions (1/2/7/8 attempts) to the geometry that caused them |

Not needed for C: `opp_slot_vehicle_ptr`/`opp_slot_distance_sq` (logged),
`opp_team_match_flag`/`opp_team_id`/`opp_team_threshold` (logged),
`opp_mode`/`opp_state` (logged), `0x45FC`/`0x4420` (logged).

## D — pit (phase2/LIVE_RESULTS.md Run D)

| # | Prio | Field | Address | Why |
|---|---|---|---|---|
| D1 | P2 | pit team index | `veh+0x48A4` (dword) | Mode 3 compares distance to the AIW point indexed by this field; explains why mode 3/5 never appear and what mode 4 waits for |
| D2 | P3 | identify a reliable pit-lane / track-part field | candidate `0x444C` (`track_part_from_pit_exit`) | It currently reads implausible raw ints (e.g. 1155388367); with it the 80 km/h limiter (0x4484 = 22.2 m/s) could be separated from traffic clamps without relying on the value. Identify first, do not guess the offset |

Not needed for D: `driver_status`, `call_to_box`, `car_conditions`, `speed_limit`,
`lap_number`, `pit_plan_count`, `pit_plan_cursor` (all logged).

## Sampling

| # | Prio | Request | Why |
|---|---|---|---|
| S1 | P2 | optional `--hz 40` for short focused runs | brain_watch polls 20 Hz while `AIMainUpdate` runs ~40 Hz, so every second AI tick is invisible; removes aliasing from mode transitions and the projection replay. Disk budget: short runs only |

## Method for the re-test (next recorded race)

1. Record a normal race with the fields above (owner starts brain_watch).
2. Re-run `phase2/analysis/c1b_a_braking.py` logic with `seg_len` + recomputed
   `v_corner` per waypoint; expect the recurrence to hold slot by slot, including
   the pre-limiter step, or a proven branch (`wp+308`/`12.0`) to explain the tail.
3. Re-run the Run C record pairing with C1 and confirm slot+0x40 == the matched
   record index for every candidate sample; then update `LIVE_RESULTS.md`.

No `findings.jsonl`, ABI JSON, `.c`, doc 54, map or CSV edits from this list;
Claude decides what enters the recorder.

## Status 2026-10-02 (checked in watch_race3, Monaco)

| # | Request | Status in watch_race3 | Evidence |
|---|---|---|---|
| A1 | per-wp corner-target inputs k112/k116/k120 | DONE | present on all 38386 waypoint records |
| A2 | all 20 chain waypoints (or `--waypoints N`) | DONE | 20 records per snapshot (27 empty = no chain) |
| A3 | branch fields wp+308/+312 | DONE as `branch_flag`/`branch_val` (ints, matches raw C `(double)*(int*)(v5+312)`) | all records |
| A4 | global pit speed `flt_8ACFB4` | DONE as `extra.pit_speed_global` = 16.667 m/s at Monaco (80 km/h 22.2 at Imola: track-specific) | all snapshots |
| C1 | slot team index `veh+0x41B4+0x44k` | DONE as `extra.slot_record_index`; matches team-id pairing 68165/68169 (99.99%), rank only 25.5% | battle snapshots |
| C2 | candidate metrics `slot+0x04/+0x08/+0x1C` | NOT LOGGED | - |
| D1 | `veh+0x48A4` pit team index | DONE as `extra.pit_team_index` | all snapshots |
| D2 | pit-lane/track-part field (0x444C) | OPEN (raw ints still implausible) | `track_part_from_pit_exit` |
| S1 | optional `--hz 40` | NOT USED (run at 20 Hz) | header hz=20.0 |

Formula re-test outcome (watch_race3, `analysis/c1b2_a_formula.py`): still not
reproduced with `seg_len` (last step meas/pred med 0.060). See
`LIVE_RESULTS.md` C1b2 Run A; a static re-read of AIRacingLine L233-L272 is the
next step, not more recorder fields.
