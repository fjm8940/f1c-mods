# TIER_REFRESH — D2 proposal v1.2 (one line per change)

Proposal file: `phase3/abi_v1_2_tier_proposal.json` (22 entries). Claude decides; no ABI file was edited.
Evidence: D1 report, D3.2 `GUESS_FIELDS.md`, `phase2/LIVE_RESULTS.md`, findings — tracks noted per entry.

- OBSERVED unnamed_4480 (0x4480) — session-time stamp; r=1.000 vs sim_t, n=62510, Monza (2 runs).
- OBSERVED unnamed_4894 (0x4894) — driver/team identity key; 3-track record pairing + id 1..19 constant per car.
- OBSERVED opp_team_id (0x42D0) — team-record id (pairing key); 3 tracks, 0xFFFFFFFF empty sentinel.
- OBSERVED opp_team_match_flag (0x42CC) — match flag 0/1; 3-track pairing evidence.
- OBSERVED command_queue_array (0x44A8) — per-car queue buffer pointer; CONFIRMED finding + live per-car constants.
- OBSERVED command_queue_capacity (0x44AC) — capacity 10; CONFIRMED finding + live constant.
- OBSERVED unnamed_4544 (0x4544) — command-response flag (cmd 5 -> 1, cmd 0/14 -> 0); CONFIRMED finding + live 0/1.
- MEANING unnamed_4364 — waypoint-node pointer scratch (was "read").
- MEANING unnamed_4400 — cached waypoint pointer, constant per car.
- MEANING unnamed_441c — pointer scratch (heap node).
- MEANING unnamed_444c — small enum {2,4,5}.
- MEANING unnamed_4454 — cumulative distance-like (r=0.95 vs speed integral).
- MEANING unnamed_4528 — sign scratch (+-1.0 float bits).
- MEANING unnamed_452c — sign scratch (+-1.0 float bits).
- MEANING unnamed_4554 — constant 1.0 multiplier in the corner-target chain (D1).
- MEANING unnamed_455c — battle/steering state counter 0..6 (D6 material).
- MEANING unnamed_4578 — bitfield bit0/bit8, not a single flag.
- MEANING unnamed_45f8 — per-car pointer scratch.
- MEANING placement_pose_x/y/z — direction-like components, not a metric pose (re-read writer before raising).
- OBSERVE command_queue_write_index (0x44B0) — live log reads NaN; recorder request R1, no tier change.
