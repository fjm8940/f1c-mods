# TV Director battle signal: AI attack decisions (research, 2026-10-13)

Scope: research only. No code edits, no builds, no game runs. `web_telemetry.c` and `telemetry_viewer.js` were not touched.
Owner decision D-TV3 (`TV_DIRECTOR_SPEC_2026-10-13.md` section 14): a battle = the AI has taken repeated or continuous
attack decisions against the car ahead, with values taken from the AI research (not a fixed gap).

Tier key: FACT = located in code and/or observed live, as stated. INFERENCE = derived from code or research, not
tested live. OPEN = unknown or conflicting; must be resolved before use.

## 1. Answer in one paragraph

The attack decision is stored per car, not per race. Each car's own vehicle struct holds five "team records" (base
`0x42CC`, stride 28). Each record is keyed by the opponent's id, and its mode at `+0x42D4` is the AI's traffic mode toward
that opponent. Modes 1 and 2 (and 7 and 8, which are alongside or side-step) are the attack side; 16 is approach and 5 is
hold. The attacker's accumulator at `+0x42DC` is the "attack patience" that builds while the attacker is close, and the
attacker commits when patience passes a threshold. A yield is a different signature: the yielding car's interaction
target `0x45FC` is set and its lateral request `0x4420` is non-zero. Do not use PassingNorm (`0x44CC`), TrackAggression
(`0x44F8`), or the interaction target alone as an attack flag.

## 2. Per-car fields (offsets are relative to the vehicle base; per-car stride 0x48C0)

| Field | Offset | Tier | Meaning and evidence |
|---|---|---|---|
| Record match flag | `0x42CC` (+0 of record) | FACT (writer OpponentProximityScanTeamRecognition) | 1 when the record matched this car. Records: base `0x42CC`, stride 28, count 5. `investigations/ai_brain_map/ai_brain_abi_v1_5.json` (`opp_team_match_flag`). |
| Opponent id | `0x42D0` (+4) | OBSERVED (4 cars checked) | Equals the opponent's `veh+0x4894`. Records do not follow rank order; mode persists at its index across rank swaps (team id 76 %, mode 95 %). `findings.jsonl` id `ai-brain-opponent-records-by-id-2026-10-02`. |
| Opponent mode | `0x42D4` (+8) | DERIVED meaning, OBSERVED transitions | Enum. 16 = approach (43.9 % of 6745 live transitions), 5 = hold (39.1 %). 1/2 = attempt. 7/8 = alongside or side-step. Mode 4 is unexplained (see OPEN). Writer AISteering only. `ai_brain_abi_v1_5.json` (`opp_mode`); `FIELD_GLOSSARY.md:47`. |
| Per-team state | `0x42D8` (+12) | DERIVED | Session-time timestamp, -1 unset. Not a mode. |
| Attack patience | `0x42DC` (+16) | DERIVED (units OPEN) | Attack "patience". Grows while within (aggression+1)x20 m of the car ahead, scaled by max(1, closing) x aggression per tick (cap 450; 250 with a target). Drains 5 per tick outside range. Follower commits when pass-cost score <= 500 - patience. Code: `AISteering_004270a0.c` L606-629, L950-1051. `findings.jsonl` id `ai-follow-and-lane-change-decision-constants-2026-10-03`. |
| Interaction target | `0x45FC` | FACT (writers Overtaking, ProximityScan, AIMainUpdate) | Yield or traffic pointer on the YIELDING car. Not an overtake flag (S1). Clear-only writes in the lab. |
| Lateral request | `0x4420` | FACT (zeroed each tick unless `0x45FC` set) | Lateral offset request, +/-3 m on the yielder. |
| Same-team peer | `0x45F8` | OBSERVED (one sample) | Pointer to same-team peer. Team-order yield writes it into `0x45FC`. |
| Garage bay rank | `0x48A8` | DERIVED | Team-order rank (higher bay yields). |
| Throttle cap | `0x457C` | CONFIRMED (code); live latch evidence corrected | 0.75 x difficulty while `0x45FC` is set. A side effect of yielding, not intent. |
| PassingNorm | `0x44CC` | CONFIRMED tuning (not intent) | `0.9 + 0.001*Passing`; also braking look-ahead factor. Never an attack flag. `findings.jsonl` S1-passingnorm-trackaggression-latched; f1c-trackaggression-is-not-the-overtaking-knob. |
| TrackAggressionEffective | `0x44F8` | CONFIRMED tuning (not intent) | Sets following gap and pressure scale, not intent. |
| Traffic pass/alongside bits | `0x4578` | DERIVED, not a single flag | Set in modes 1/2 (54 %) and 4 (78 %). Do not use alone. |
| Driver status | `0x4510` | OBSERVED | 0 race; 4/5 pit or garage wait; 2 is a one-tick reset. Use to exclude non-race states. |
| Lap of run / total time | `0x4094` / `0x4098` | DERIVED | Gap and lap context. |

The record set is a block of five entries. The ABI says it is keyed by opponent id, not by proximity slot, but the rule for
which five opponents are tracked is OPEN.

## 3. Attack versus yield versus defend

| Situation | Signature on the car that decided it | Tier |
|---|---|---|
| Attack (own decision) | Attacker's record for the target's id at `0x42D4` is 1, 2, 7 or 8. Attacker's own `0x45FC` is 0 (or points to someone else, which is a yield). Patience `0x42DC` builds. Passes follow (Irvine 16 to 1 at 277 s, pass at 286.3 s; one 248-sample window). | DERIVED / OBSERVED (single case) |
| Yield or side-step (asked to move) | Yielder's `0x45FC` is non-zero and points to the car it yields to. `0x4420` is +/-3 m. Throttle cap `0x457C` 0.75. Blue flag (lap-ahead), qualifying flying lap against an out-lap car (14/14 live), or ProximityScan auto-yield for the player only. | FACT (writers); OBSERVED (qualifying 14/14) |
| Team-order yield | Yielder's `0x45F8` names the teammate and `0x45FC` points to it, with the higher garage bay yielding. Exclude these pairs from battles; record as team. | DERIVED (one sample) |
| Defend | No dedicated defend mode is known. The car ahead did not record the attacker in its records (one live case, Schumacher ahead of Hill). | OPEN |

Rule of thumb: the attack lives in the ATTACKER's struct (`0x42D4`, keyed by the target's id). The yield lives in the
YIELDER's struct (`0x45FC`, `0x4420`). A battle needs the first and must not be confused with the second.

## 4. Update rate and read path

- AI brain tick: 25 ms, so about 40 Hz (`ai_brain_abi_v1_5.json`, `driver_status` note). Mode 2 can be missed at 20 Hz.
- DLL capture target: 20 Hz, duplicates dropped during pause (`API_CONTRACT_2026-10-09.md` section 4). SSE to the viewer: 10 Hz.
- Live transitions were counted from roughly 10 Hz samples (`ai_brain_lab/README.md` section "Irvine vs Alonso"). Flicker shorter
  than one capture is not measured (OPEN).
- Read path (no code change proposed here): `compatibility_runtime.inc` L44-63 copies the full 0x48C0-byte vehicle struct
  for each roster pointer into `mirror_vehicles[i]` on every capture. The pointer table is at `0x7C54F0`
  (`director_state.inc` L37-47). A future field read would be `*(uint32_t*)(mirror_vehicles[i].bytes + 0x42D4)`, and the
  opponent id `mirror_vehicles[i].bytes + 0x42D0` compared to `mirror_vehicles[j].bytes + 0x4894`. The yield target `0x45FC`
  is an absolute game pointer and is compared to the roster pointer table, not to the mirror index.
- Today `web_telemetry.c` does not read `0x42D4`, `0x42D0`, `0x42DC`, `0x45FC`, or `0x4420` (grep: none). The mirror holds them.
- Memory MCP: `tools/f1c_memory_mcp.py` L26-27 already reads `passing_norm` (`0x44CC`), `interaction_target` (`0x45FC`) and
  `lateral_request` (`0x4420`), but not the `0x42D4` family. That is a read-only gap; adding it is a code change and is not
  proposed here.

## 5. Proposed battle rule (INFERENCE, to be tuned live)

Pair: A = attacker, B = the car immediately ahead of A in race order (from the existing `/telemetry` rank).

Per capture sample t (20 Hz):
- `attack(t)` = A's record for B's id has mode in {1, 2, 7, 8}.
- Exclude the sample (do not count it either way) when A's `0x45FC` is non-zero (A is yielding) or A and B are the same team
  (team-order pair). Record team pairs as their own category.

Window: W = 2.25 s, which is 45 samples at 20 Hz. Derived from the AI's own patience drain: cap 450 at 5 per tick,
40 ticks per second, is 90 ticks, or 2.25 s (INFERENCE from DERIVED constants; not validated).

- Battle ON for (A, B) when, in the last W, attack samples are at least 50 % of valid samples and A has at least 2 attack
  episodes (edges into {1, 2, 7, 8}) against the same B. "Repeated" and "continuous" are both covered this way.
- Battle OFF when attack samples are below 10 % of valid samples in the last W (hysteresis, so it does not flicker).
- The 50 % / 10 % / 2-episode thresholds are not from the research. They are proposed defaults and must be tuned live.

Confidence:
- Offsets and writers: medium-high (static plus live consistency checks).
- Mode meaning for 1 vs 2 and 7 vs 8, and the mode 3/4 meaning: low (OPEN).
- Window of 2.25 s: low-medium (derived from constants, never measured).
- Thresholds: low (proposal only).

## 6. OPEN items

1. `0x4570` conflict. The ABI (`unnamed_4570`, from `offsets_AISteering.csv`) says "interaction/avoidance opponent id + 1".
   `web_telemetry.c` L800-806 says `OFF_ELAPSED_TIME 0x4570` is MISNAMED and reads 0 for the player and 1 for every AI car.
   Do not use `0x4570` for battles until resolved.
2. Mode 4 and mode 3 meaning. The lever table lists 1/2/7/8 as attack, but `0x4578` says mode 4 is 78 % attack-like.
3. Patience units. The ABI calls `0x42DC` a "per-team overtake margin accumulator" in m^2. The attack-patience reading is
   points against cap 450. Units are not settled.
4. Which five opponents occupy the records. If the car ahead is not among them, a battle cannot be seen. "No record" does not
   prove no intent.
5. Id-to-slot mapping (`0x42D0` = `0x4894`) checked on 4 cars only. Needs a 22-car check.
6. Defend: whether the car ahead ever records the attacker (one live case only).
7. Flicker: whether modes change faster than 20 Hz capture. Needs a 40 Hz comparison.
8. Pause: any sample must show advancing sim time and TICK. Identical samples are a paused frame (see the corrected
   throttle-cap finding). Read-only; an unchanged value does not prove the AI is idle.

## 7. What must be live-verified first (owner test steps, read-only)

No game run was made here. Use `memory-mcp start-reader` (or the existing telemetry) and read-only captures. Check sim time
advances between samples before counting anything.

1. Id check (22 cars): for each car, confirm each record's `0x42D0` equals some other car's `0x4894`. Pass if 100 % of
   populated records match.
2. Known pass: pick one attack episode with a commit edge (16 or 5 to 1 or 2) and a pass within about 10 s. Confirm the edge
   precedes the position change. Repeat for 3 passes.
3. Yield control: qualifying flying lap against an out-lap car. Expect the flying car's mode attack toward the out-lap car,
   and the out-lap car's `0x45FC` pointing at the flying car with `0x4420` about +/-3 m. Expect 14 of 14 (as before).
4. Team control: teammate ahead on lap 4 or later. Expect `0x45FC` to equal the teammate pointer. Confirm the pair is
   categorised as team, not battle.
5. Defend control: three separate pairs with an attacker behind a car. Check whether the car ahead's records ever list the
   attacker. Expect no (one case so far).
6. Flicker: 20 Hz and 40 Hz captures over the same 60 s. Count mode edges that appear only at 40 Hz.
7. Mode meanings: for modes 3, 4, 7 and 8, record the lateral offset and the gap trend when each is active.
8. Window: measure how long attack patience takes to drain after the attacker leaves range, and check it against 2.25 s.

Pass criteria for the proposed rule: it fires on known passes and does not fire on team yields or qualifying yields, in
at least three races. Until then the rule is a proposal.

## 8. Not done and why

- No `web_telemetry.c` or viewer edits (another agent is editing them).
- No DLL, memory-MCP, or ABI edits (research only).
- No game run or live read was made in this task.
- The TV Director's battle thresholds (spec Q4, D-TV3) are not set here; this note proposes values for the owner to accept or change.

## 9. Sources

- `investigations/ai_brain_map/ai_brain_abi_v1_5.json` (fields `opp_mode`, `opp_team_threshold`, `opp_team_id`,
  `opp_team_match_flag`, `opp_state`, `interaction_target`, `lateral_request`, `throttle_cap_difficulty`,
  `unnamed_4578`, `unnamed_4570`, `unnamed_45f8`, `driver_status`).
- `investigations/ai_brain_map/FIELD_GLOSSARY.md` L45-49 (records), L117 (`0x45FC`).
- `development/mods/ai_brain_lab/README.md` lever table L109-124, Irvine live read L490-496, D6/record note L96-101.
- `development/mods/ai_brain_lab/MANUAL.md` L119 (ATTACKING badge already keyed to modes 1/2), L380-393 (decision constants).
- `findings.jsonl`: `ai-brain-opponent-records-by-id-2026-10-02`, `ai-no-defend-mode-2026-10-03`,
  `ai-follow-and-lane-change-decision-constants-2026-10-03`, `S1-ai-interaction-target-not-overtake`,
  `S1-passingnorm-trackaggression-latched`, `f1c-trackaggression-is-not-the-overtaking-knob`,
  `ai-lateral-request-4420-zeroed-each-tick-2026-10-02`, `ai-native-team-order-yield-overtaking-2026-10-03`,
  `ai-quali-flying-lap-car-steers-around-2026-10-02`, `ai-brain-same-team-peer-45f8-live-2026-10-02`,
  `ai-throttle-cap-457c-latch-evidence-correction-2026-10-03`, `ai-team-order-rank-is-garage-bay-48a8-2026-10-03`.
- `mods/sdk_sandbox/web_telemetry/compatibility_runtime.inc` L44-63; `web_telemetry.c` L148, L800-806;
  `director_state.inc` L37-47; `API_CONTRACT_2026-10-09.md` section 4; `TV_DIRECTOR_SPEC_2026-10-13.md` section 14.
- `tools/f1c_memory_mcp.py` L26-33; `tools/sdk/F1C_MEMORY_MCP.md` (PassingNorm and interaction-target limitation).
