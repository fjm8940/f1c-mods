# GUESS_V12_NEXT — D15: one settling action per v1.2 GUESS field (BATCH B item 12)

Date: 2026-10-02. Author: DeepSeek V4.1. No tier changes here (Claude merges later). "static read" =
read the named function/line in `functions/*.c`; "live log" = one watcher capture with the named
field; "promote" = evidence already exists in phase3, only the merge is missing. Fields in registry
order.

- `placement_pose_x` (0x34D4): static read of `AIRacingLinePathFollowing` L90-L110 write sources plus the docs/52 path-blend header — names the blend (base+path[0]*perp) or disproves the pose meaning.
- `placement_pose_y` (0x34D8): same read as 0x34D4; live log of the path blend during a weather/grip transition would fix the interpolation.
- `placement_pose_z` (0x34DC): same as 0x34D4 (one read closes all three).
- `opp_slot_vehicle_ptr` (0x4174): live log of the slot record's team index (`slot+0x40`) next to the pointer; pair with `extra.slot_record_index` to prove the slot↔vehicle pairing directly.
- `opp_slot_distance_sq` (0x41A8): static re-read of `OpponentProximityScanTeamRecognition` 0x42A7D0 distance math; live check is a one-line squared-vs-planar ratio against the logged `opponents[].pos`.
- `opp_team_threshold` (0x42DC): static read of the reset-to-500 site and the compare in 0x42A7D0; live log of a threshold crossing (~500) during a battle settles it.
- `opp_state` (0x42D8): static re-read of the `Overtaking`/`AISteering` state compare (candidate record) with the D6 transition table; a live per-opponent state log replaces the rank pairing.
- `unnamed_4000` (0x4000): static read of the writer feeding the `this[4096] <= 0.0` pit-request gate in `AIAbility` L81 (command/pit paths) — names the request flag.
- `unnamed_4040` (0x4040): static read of the `*((_DWORD*)this+4112)` uses (AIMainUpdate race-end branch, AIAbility L97) — names the race-end/limbo state.
- `unnamed_413c` (0x413C): static read of the `sub_426430` corner-clamp branch reader — the FLT_MAX set/compare names the "no clamp" sentinel.
- `unnamed_416c` (0x416C): static read of `AIMainUpdate` L196-L206 — D13 shows it is the per-lap engine-life modulus budget; promote at merge.
- `unnamed_4170` (0x4170): static read of `AIMainUpdate` L89 writer + L208 consumer (`this+16752` compare) — names the start-phase timer.
- `outside_corridor_flag` (0x4140): static read of `AISteering` epilogue 0x429457 write and its readers; live log during a corridor clamp settles the trigger.
- `unnamed_4364` (0x4364): live identity check — compare the value against the logged waypoint chain pointers (it is in the same heap block); static read of `sub_426430` L~120.
- `unnamed_43f4` (0x43F4): static read of the `sub_426430` write expression; live log vs speed/segment length to name the quantity.
- `unnamed_4400` (0x4400): live identity check against the chain pointers (one capture already logs `current_waypoint_ptr`) — settles which node it caches; promote or correct.
- `unnamed_4404` (0x4404): settled by D4 (deadline = session time + 4, 4 s cadence, replay 91-94 % in-window) — promote at merge, no further work.
- `unnamed_441c` (0x441C): static read of the `sub_426430`/`AISteering` use; live address-class check (heap node vs waypoint) names it.
- `unnamed_4444` (0x4444): static read of the `sub_426430` write; live correlation with lateral position/radius.
- `time_margin` (0x4448): static read of the `AIRingBufferFieldInit` expression and its `AIRacingLine` phase-change consumer — names the margin; live log of its 40 changes vs lap phase confirms.
- `unnamed_444c` (0x444C): static read of the AIMainUpdate branch that writes {2,4,5} — names the grid/session state enum.
- `unnamed_4454` (0x4454): static read of the `sub_426430` accumulator; the D3.2 r=0.95 vs distance integral is enough to promote to "cumulative distance-like".
- `unnamed_4458` (0x4458): static read of the `sub_426430` counter write; live distinct-value timeline names it.
- `unnamed_4460` (0x4460): static read of the `AISteeringRacingLineCorrection` write and its consumer — names the correction scratch.
- `unnamed_4488` (0x4488): live capture in a garage/session-start phase confirms the 1/0 garage signature (finding garage-vs-track-signature) — promote.
- `unnamed_4490` (0x4490): static read of the `AISteering` read site; live correlation with mode/line.
- `command_queue_write_index` (0x44B0): type fixed in v1.2 (int); a watcher restart logs real values — one race capture with -1/indices settles activity.
- `unnamed_44fc` (0x44FC): static writer already found (D5: `sub_424080` L62 `rand%3`); promote to "per-driver random variant"; a live log cannot distinguish the variant without behavior correlation.
- `unnamed_4508` (0x4508): settled by D4 (traffic accumulator, cleared by AIAbility; pace model 5465/5467 explained) — promote.
- `unnamed_450c` (0x450C): settled by D4/D6 (max candidate race_ability_norm; random-gate input) — promote.
- `unnamed_4524` (0x4524): static read of the AIMainUpdate mode-branch write — names the reset state.
- `unnamed_4528` (0x4528): static read of the `sub_426430` write sites; live sign vs lateral direction names the side convention.
- `unnamed_452c` (0x452C): same read as 0x4528 (one read closes both).
- `unnamed_4554` (0x4554): static store scan for 0x4554 (D10-style) finds its writer — settles whether it is an init constant 1.0 (D1 live: always 1.0).
- `unnamed_455c` (0x455C): D6 live shown (2 in attack modes, 3 in approach; not a pass Boolean); static read of the `AISteering` write sites promotes the counter meaning.
- `unnamed_4560` (0x4560): static read of the AIMainUpdate write (`dbl_8AFEA0+...` pattern) — names the session-time stamp.
- `unnamed_4564` (0x4564): settled by D7 (`this+17764` = next start-throttle toggle time, `db + 1 + rand%100*0.02`) — promote.
- `unnamed_4568` (0x4568): settled by D7 (`this+17768` = start-ramp direction flag) — promote.
- `unnamed_456c` (0x456C): static read of the `AISteering` write/read pair — names the byte flag.
- `unnamed_4570` (0x4570): static read of the `AISteering` write sites; D6 shows it is rare (15 % in mode 4) — a mode-correlated live log names it.
- `unnamed_4574` (0x4574): same static read; the 0/1 with battle r=0.24 suggests an alongside flag — live log vs mode groups.
- `unnamed_4578` (0x4578): D6 shows bit0/bit8 set in attack modes 1/2 (54 %) and 4 (78 %); static read of the writer promotes "pass/alongside state".
- `unnamed_4579` (0x4579): same writer as 0x4578 (one read closes both).
- `unnamed_4584` (0x4584): settled by D8 (`PitStopDecisionTireCompound` sets it, `PostPitCleanup` reads it) — promote to pit-release flag.
- `unnamed_4590` (0x4590): static read of the `sub_429960` slot indexing (`this[4*i+4452]` lap estimates) — names the per-stop lap slot.
- `unnamed_4594` (0x4594): settled by D8 (`PitStopDecisionTireCompound` L83 writes the per-stop speed) — promote.
- `unnamed_45a0` (0x45A0): settled by D8 (`this[4*v2+4456]` = next-stop lap target) — promote.
- `unnamed_45e8` (0x45E8): static read of `sub_429960` L254 (`this[4474] = (lap+2)*scale`) and the `AISteeringRacingLineCorrection` reader — names the strategy scale.
- `unnamed_45f8` (0x45F8): settled by REVIEW_15 live (Massa -> Villeneuve same-team peer) — promote.
- `unnamed_461c` (0x461C): static read of the AIMainUpdate write (constant 999999 in D3.2) — names the "no target" sentinel.

## Notes

- 50 fields; 14 already have settling evidence in phase3 (promote-only): 0x4404, 0x416C, 0x4488,
  0x44FC, 0x4508, 0x450C, 0x4564, 0x4568, 0x4578/79, 0x4584, 0x4594, 0x45A0, 0x45F8, 0x4454.
- The remaining 36 need one static reader/writer read each (most already located by D4-D14) or one
  targeted live log; none needs a new recorder field except the already-listed R2.
