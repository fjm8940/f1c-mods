# GUESS_FIELDS - D3.2 statistics for the 57 GUESS fields in ABI v1.1

Data: `%TEMP%\ai_brain_live\watch_race4fullc` + `watch_race4fulld` `overview.jsonl`
(Monza, 1 Hz, all cars). Array fields use element 0 (nearest opponent). Correlation is
Pearson of the raw value against: brake (`derived.braking.active`), pit
(`driver_status` 3/4/5), battle (any opponent < 15 m), lap (`lap_number`), time
(`sim_t`), dist (integral of `speed` over each car's samples). Tiers feed D2;
"GUESS unchanged" means the logs neither prove nor contradict the documented meaning
strongly enough to raise it.

| field | hex | n | min | med | max | distinct | int | changes | strongest corr | data vs GUESS meaning | proposed |
|---|---|---|---|---|---|---|---|---|---|---|---|
| placement_pose_x | 0x34D4 | 62510 | -1 | -0.0197 | 0.9998 | 14756 | no | 57005 | lap r=0.18 | unit-range floats -1..+1, 14756 distinct, changes every tick, weak lap corr | GUESS: values look direction-like, not a metric pose; meaning should be re-read before any raise |
| placement_pose_y | 0x34D8 | 62510 | -0.8465 | -0.0044 | 0.2469 | 2519 | no | 56957 | lap r=0.31 | -0.85..+0.25, 2519 distinct, weak lap corr 0.31 | GUESS: direction-like component (same as x/z) |
| placement_pose_z | 0x34DC | 62510 | -1 | -0.1428 | 1 | 12887 | no | 51881 | pit r=-0.10 | -1..+1, 12887 distinct, no state correlation | GUESS: direction-like component; no tier raise |
| opp_slot_vehicle_ptr | 0x4174 | 62510 | 0 | 6.398e+06 | 6.565e+06 | 21 | yes | 7456 | battle r=0.09 | 21 pointer values, constant per run/slot; battle r=0.09 (addresses not meaningful) | GUESS: consistent with per-slot opponent pointer; no tier raise |
| opp_slot_distance_sq | 0x41A8 | 62510 | 1 | 2.204e+04 | 1e+06 | 52427 | no | 54726 | battle r=-0.18 | 1..1e6 (1e6 sentinel), battle r=-0.18; nearest-first order already confirmed on 3 tracks | GUESS: ordering evidence is OBSERVED-level but the squared-distance unit is not calibrated; no tier raise |
| opp_team_match_flag | 0x42CC | 62510 | 0 | 1 | 1 | 2 | yes | 2582 | brake r=-0.06 | 0/1 on every car; team-record pairing matched 99.99 % (Monaco) and 100 % (Monza) | OBSERVED: match flag confirmed by the 3-track record pairing |
| opp_team_id | 0x42D0 | 62510 | 0 | 11 | 4.295e+09 | 21 | yes | 4614 | battle r=-0.06 | 0..19 plus 0xFFFFFFFF empty-record sentinel, constant per car | OBSERVED: the id used by the validated record pairing (Imola/Monaco/Monza) |
| opp_team_threshold | 0x42DC | 62510 | 0 | 0 | 500 | 1225 | no | 2323 | sim r=-0.27 | 0..500, 1225 distinct, changes during the race, sim r=-0.27 | GUESS unchanged; per-tick reset behaviour not contradicted |
| opp_state | 0x42D8 | 62510 | -9999 | -9999 | 3358 | 3600 | no | 4488 | sim r=-0.30 | -9999 sentinel..3357, 4488 changes, sim r=-0.30; battle context is D6 work | GUESS unchanged; meaning needs D6 |
| unnamed_4000 | 0x4000 | 62510 | 0 | 0 | 0 | 1 | yes | 0 | -  | constant 0 on all cars | GUESS unchanged (threshold read; no data) |
| unnamed_4040 | 0x4040 | 62510 | 0 | 1 | 2 | 3 | yes | 1866 | pit r=-0.10 | {0,1,2} | GUESS unchanged |
| unnamed_413c | 0x413C | 62510 | 49.9 | 3.403e+38 | 3.403e+38 | 639 | no | 1278 | battle r=-0.07 | 49.9..FLT_MAX (FLT_MAX median), 639 distinct | GUESS unchanged; FLT_MAX branch matches the note |
| unnamed_416c | 0x416C | 62510 | 0 | 0 | 0 | 1 | yes | 0 | -  | constant 0 | GUESS unchanged |
| unnamed_4170 | 0x4170 | 62510 | 3.403e+38 | 3.403e+38 | 3.403e+38 | 1 | yes | 0 | -  | constant FLT_MAX | GUESS unchanged |
| outside_corridor_flag | 0x4140 | 62510 | 0 | 0 | 1 | 2 | yes | 10687 | cumdist r=-0.30 | 0/1, 10687 changes | GUESS unchanged |
| unnamed_4364 | 0x4364 | 62510 | 0 | 8.337e+08 | 8.339e+08 | 1175 | yes | 53844 | sim r=0.01 | pointer-like 0x31B0.... waypoint addresses, 1175 distinct, changes with the waypoint chain | GUESS: meaning correction - waypoint-node pointer scratch (not a generic read) |
| unnamed_43f4 | 0x43F4 | 62510 | 0.3984 | 5.003 | 40.74 | 24744 | no | 53896 | sim r=0.18 | 0.40..40.7 float, 24744 distinct | GUESS unchanged |
| unnamed_4400 | 0x4400 | 62510 | 8.339e+08 | 8.339e+08 | 8.339e+08 | 10 | yes | 0 | cumdist r=0.13 | pointer-like waypoint address, constant per car (0 changes), 10 distinct | GUESS: meaning correction - cached waypoint pointer, constant per run |
| unnamed_4404 | 0x4404 | 62510 | -1 | 1539 | 3362 | 8363 | no | 14480 | cumdist r=0.86 | 0..3361, median 1539, r=0.86 vs cumulative distance / 0.84 vs sim_t | GUESS unchanged: time-like stamp but not exactly sim_t |
| unnamed_441c | 0x441C | 62510 | 0 | 3.152e+09 | 3.344e+09 | 56835 | yes | 56825 | pit r=-0.09 | pointer-like 0xBC0.... addresses, 56835 distinct | GUESS: meaning correction - pointer scratch (heap node) |
| unnamed_4444 | 0x4444 | 62510 | 0 | 0.9138 | 1 | 6016 | no | 33799 | cumdist r=0.38 | 0..1 float, 6016 distinct, lap r=0.38 | GUESS unchanged |
| time_margin | 0x4448 | 62510 | 0.0556 | 0.1051 | 0.1976 | 58 | no | 40 | lap r=0.12 | 0.056..0.198 s, 58 distinct, only 40 changes | GUESS unchanged; plausible stable margin |
| unnamed_444c | 0x444C | 62510 | 2 | 5 | 5 | 3 | yes | 38 | sim r=0.18 | small integer enum {2,4,5}, 38 changes | GUESS: meaning correction - small state enum, not a float |
| unnamed_4454 | 0x4454 | 62510 | -5633 | 8.91e+04 | 2.249e+05 | 29635 | no | 53828 | cumdist r=0.95 | 89101..224900, 29635 distinct, r=0.95 vs cumulative speed integral | GUESS: meaning correction - cumulative distance-like quantity, not a scratch float |
| unnamed_4458 | 0x4458 | 62510 | 0 | 22 | 4.295e+09 | 41 | yes | 652 | cumdist r=-0.48 | 0..0xFFFFFFFF, 41 distinct, lap r=-0.48 | GUESS unchanged (counter/bitfield) |
| unnamed_4460 | 0x4460 | 62510 | -1 | -1 | -1 | 1 | yes | 0 | -  | constant -1 | GUESS unchanged |
| unnamed_4480 | 0x4480 | 62510 | 8.632 | 1687 | 3358 | 5150 | no | 62472 | sim r=1.00 | r=1.000 vs sim_t over 62510 samples (median 1687.2 s) | OBSERVED: session-time stamp confirmed (Monza, 2 runs; one track) |
| unnamed_4488 | 0x4488 | 62510 | 0 | 0 | 1 | 2 | yes | 27 | battle r=-0.06 | 0/1 | GUESS unchanged |
| unnamed_4490 | 0x4490 | 62510 | 0 | 0 | 0 | 1 | yes | 0 | -  | constant 0 | GUESS unchanged |
| command_queue_array | 0x44A8 | 62510 | 7.231e+08 | 7.231e+08 | 7.231e+08 | 19 | yes | 0 | lap r=0.14 | per-car constant pointer (19 values for 19 cars, 0 changes) | OBSERVED: finding f1c-command-queue-is-per-car-not-global-ring (CONFIRMED) + live per-car pointer |
| command_queue_capacity | 0x44AC | 62510 | 10 | 10 | 10 | 1 | yes | 0 | -  | constant 10 on every car | OBSERVED: finding f1c-command-queue-is-per-car-not-global-ring (CONFIRMED) + live 10 |
| command_queue_write_index | 0x44B0 | 0 | - | - | - | - | - | - | - | all 62510 samples NaN in both runs | GUESS unchanged: not usable; recorder request added (0x44B0) |
| unnamed_44fc | 0x44FC | 62510 | 0 | 1 | 2 | 3 | yes | 0 | battle r=0.01 | {0,1,2} | GUESS unchanged |
| unnamed_4508 | 0x4508 | 62510 | 0 | 0 | 7595 | 986 | yes | 16259 | lap r=-0.31 | 0..7595, 986 distinct, lap r=-0.31 | GUESS unchanged |
| unnamed_450c | 0x450C | 62510 | 0 | 0 | 0.8636 | 2 | no | 2843 | cumdist r=-0.39 | {0, 0.8636}, 2843 changes, lap r=-0.39 | GUESS unchanged (two-state value) |
| unnamed_4524 | 0x4524 | 62510 | 0 | 0 | 204.4 | 163 | no | 205 | battle r=0.07 | 0..204.4, 163 distinct | GUESS unchanged |
| unnamed_4528 | 0x4528 | 62510 | 1.065e+09 | 1.065e+09 | 3.213e+09 | 2 | yes | 9171 | cumdist r=0.10 | raw float bits: +1.0 / -1.0 only | GUESS: meaning correction - sign scratch (+-1.0 float) |
| unnamed_452c | 0x452C | 62510 | 1.065e+09 | 1.065e+09 | 3.213e+09 | 2 | yes | 4990 | brake r=0.14 | raw float bits: +1.0 / -1.0 only | GUESS: meaning correction - sign scratch (+-1.0 float) |
| unnamed_4544 | 0x4544 | 62510 | 0 | 1 | 1 | 2 | yes | 1742 | cumdist r=0.35 | 0/1, 1742 changes; finding f1c-cmd-0xe-writes-veh-443c (CONFIRMED) gives the writers | OBSERVED: command-response flag (cmd 5 -> 1, cmd 0/14 -> 0) |
| unnamed_4554 | 0x4554 | 62510 | 1.065e+09 | 1.065e+09 | 1.065e+09 | 1 | yes | 0 | -  | constant 1.0 float (logged raw bits 1065353216) in all samples | GUESS: meaning correction - constant 1.0 multiplier in the corner-target chain (D1) |
| unnamed_455c | 0x455C | 62510 | 0 | 0 | 6 | 7 | yes | 9095 | battle r=0.45 | 0..6 integers, battle r=0.45 | GUESS: meaning correction - battle/steering state counter; D6 material |
| unnamed_4560 | 0x4560 | 62510 | 0 | 42.06 | 42.45 | 30 | no | 46 | brake r=0.05 | 42.06..42.45, 30 distinct, no state correlation | GUESS unchanged (timer-like, not session time) |
| unnamed_4564 | 0x4564 | 62510 | 0 | 43.56 | 44.91 | 45 | no | 46 | battle r=0.05 | 43.6..44.9, 45 distinct | GUESS unchanged |
| unnamed_4568 | 0x4568 | 62510 | 0 | 0 | 1 | 2 | yes | 27 | lap r=0.07 | 0/1, 27 changes | GUESS unchanged |
| unnamed_456c | 0x456C | 62510 | 0 | 0 | 1 | 2 | yes | 1793 | battle r=0.05 | 0/1 | GUESS unchanged |
| unnamed_4570 | 0x4570 | 62510 | 0 | 0 | 21 | 8 | yes | 505 | battle r=0.17 | {0..6, 20, 21}, battle r=0.17 | GUESS unchanged; D6 material |
| unnamed_4574 | 0x4574 | 62510 | 0 | 0 | 1 | 2 | yes | 428 | battle r=0.24 | 0/1, battle r=0.24 | GUESS unchanged; D6 material |
| unnamed_4578 | 0x4578 | 62510 | 0 | 0 | 257 | 4 | yes | 2802 | battle r=0.28 | {0, 1, 256, 257}, battle r=0.28 | GUESS: meaning correction - bitfield (bit0 and bit8), not a single flag |
| unnamed_4579 | 0x4579 | 62510 | 0 | 0 | 1 | 2 | yes | 2784 | battle r=0.28 | 0/1, battle r=0.28 | GUESS unchanged; D6 material |
| unnamed_4584 | 0x4584 | 62510 | 0 | 0 | 1 | 2 | yes | 3 | lap r=0.14 | 0/1, only 3 changes across the race, pit r=0.08 | GUESS unchanged; rare toggle is consistent with a pit flag but not proof |
| unnamed_4590 | 0x4590 | 62510 | 0 | 0 | 0 | 1 | yes | 0 | -  | constant 0 | GUESS unchanged (not used in this session) |
| unnamed_4594 | 0x4594 | 62510 | 45.25 | 70.95 | 87.46 | 34 | no | 17 | lap r=0.16 | 45.2..87.5, 34 distinct, 17 changes | GUESS unchanged (consistent with per-compound speed) |
| unnamed_45a0 | 0x45A0 | 62510 | 13 | 18 | 20 | 6 | yes | 0 | lap r=0.08 | {13, 14, 17, 18, 19, 20} | GUESS unchanged |
| unnamed_45e8 | 0x45E8 | 62510 | 7.452 | 7.58 | 7.674 | 10 | no | 0 | cumdist r=0.12 | 7.45..7.67, 10 distinct, constant per car | GUESS unchanged |
| unnamed_45f8 | 0x45F8 | 62510 | 6.211e+06 | 6.379e+06 | 6.565e+06 | 19 | yes | 0 | battle r=0.06 | pointer-like 0x5F...., 19 distinct, per car | GUESS: meaning correction - per-car pointer scratch |
| unnamed_461c | 0x461C | 62510 | 1e+06 | 1e+06 | 1e+06 | 1 | yes | 0 | -  | constant 999999 | GUESS unchanged |
| unnamed_4894 | 0x4894 | 62510 | 1 | 10 | 19 | 19 | yes | 0 | battle r=0.04 | 1..19 identity id, constant per car; the key matched against team records 99.99 %/100 % on 3 tracks | OBSERVED: driver/team identity key (meaning refined) |

Notes: `changes` counts per-car sample-to-sample value changes across both runs; a value
with 0 changes is constant in every sampled car. Pointer-like fields (`command_queue_array`,
`opp_slot_vehicle_ptr`) are compared as integers and correlated as raw addresses, so their
correlations are not meaningful. `unnamed_4554` is logged as raw float bits (D1).
