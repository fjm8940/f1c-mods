# QUALI_STATIC -- E1: session-global dword_7C56A4 AI branch census

Date: 2026-10-02. Author: DeepSeek V4.1 (BATCH C, offline). Registry: `ai_brain_abi_v1_3.json`.
Task: `tools/sdk/DEEPSEEK_HANDOFF_AI_BRAIN_PHASE3_2026-10-02.md` BATCH C item 1 (E1).

Method: `phase3/analysis/e1_session_gates.py` -- capstone (detail mode) disassembly of the game exe,
one slice per `functions/*.c` start (7128 of 7132 corpus functions are inside .text). A whole-.text
linear sweep with `skipdata` desyncs and misses real hits (e.g. ServerSessionGateCheck 0x42AE60), so the
per-function slice is the primary pass. Command:
for QUALIFYINGABILITY 0x44C0).

Session ladder (prior art, OBSERVED/FACT): 0 = TestDay, 1-4 = Practice1-4, 5 = QUALIFYING,
7 = RACE (`test-day-session-type-0`, `session-global-7c56a4-values-live-2026-10-02`). The global is
READ-ONLY in all 139 corpus hits (0 writes).

## A. AI brain / AI-layer readers of dword_7C56A4

`cmp` value = the compared literal (capstone immediate or the Hex-Rays `.c` line for register compares).
"non-race" = what changes for practice/quali (sessions 1-4/5) vs race (7).

| Function | Site(s) | Compare | Race (7) | Non-race (practice/quali) | Tier |
|---|---|---|---|---|---|
| AIMainUpdate | 0x425326, 0x4255BE | == 7 | mode reset after limbo -> 4 | -> 5 (garage/pit wait) | DERIVED |
| AIMainUpdate | 0x4255BE region L287 | != 7 | boxed wait keeps flag | clears `+0x45F4` byte when past plan | DERIVED |
| AIAbility | 0x4294FD..0x4298FB (6) | == 7 | full pace adaptation + race pit/end logic | skips all of it (see D) | DERIVED |
| AIRacingLine | 0x425D0A | == 7 | reset after off-track -> 4 | -> 5 | DERIVED |
| AIPushLap | 0x42A19B | != 7 | no ramp | +0.003 * min(laps,10) ramp of 0x453C (see C) | DERIVED |
| AIAbility race-only block | L96-186 | == 7 (6 sites) | end-of-race gate, ConditionalGate, cmd 6, non-race pit path skipped | -- | DERIVED |
| AIDriverStructInit | 0x424A7C | != 3/4/5 | v19 *= flt_5EA3E8 | sessions 3/4/5 skip the scale (audio pitch init) | GUESS |
| sub_424080 (.rcd parser) | 0x4244FC | == 7 (+CEA5C) | race-only HP/damage branch at L177 | -- | DERIVED |
| AICollision | 0x4BEF60 | == 7 | race only (incident codes) | disabled | DERIVED |
| AIBehaviorInitialization | 0x4BF1B1, 0x4BF340 | == 7 | always re-init | non-race gated by `lap <= StateFlagsLookup(veh)` (L83) | DERIVED |
| AICommandMessageQueueDispatcher | 0x424D30 | == 7 | race queue path | -- | DERIVED |
| AIRacingLine / drivers | see above | | | | |
| CommandQueueResponseHandler | 0x429F03 | != 7 | pair-array search only when not race | non-race command response allowed | DERIVED |
| ForcePitExit | 0x42A110 | == 7 | race behavior | -- | DERIVED |
| PostPitCleanup | 0x42A70E | == 7 | race branch | -- | DERIVED |
| PitStopDecisionTireCompound | 0x42A425 + 3 more | == 7 | race lap/end-of-race branch | non-race branch uses plan-slot lap (L35-43); `PitStopBlockingCondition` path | DERIVED |
| PitStopBlockingCondition | 0x42AEE0 | != 7 | race blockers (lap>=total, SC, etc.) | non-race: plan row flags `& 8` (L4) | DERIVED |
| OpponentProximityScanTeamRecognition | 0x42A8D7 | == 7 | blue-flag auto-yield (dist^2<625, lapping car = player) | disabled | DERIVED |
| Overtaking | 0x42AFB8 | == 7 / == 5 | full race overtake logic | session-5 ServerSessionGateCheck branch (B) | DERIVED |
| sub_429960 (pit plan) | 0x429A2A + switch | == 7 / switch | parses PitStopStrategy + race schedule | cases 3/4/5: evenly spaced plan; 0/1/2/6: random plan (E) | DERIVED |
| ServerSessionGateCheck | 0x42AE60 | == 5 | returns 0 (not qualifying) | returns 1 except lap_of_run==2 && !call_to_box | DERIVED |
| AIDriverSorting | 0x4BF393 | stored + used | sort key = lap/race position | sort key = best_lap 0x409C (L52) | DERIVED |
| sub_4A4110 (AIMainUpdate callee) | 0x4A41B7, 0x4A46A1 | == 7 / 3-4-5 | returns 1 immediately (L73) | field-fraction divisor 1/11 for sessions 3/4/5, else 1/14 (L92-95) | GUESS |
| AISettingsFileLoader | 0x4334F7 -> IterationCountMap 0x432E50 | map 3/4/5 -> 5 | sessions 0/1/2/6/7 -> race SVM | sessions 3/4/5 -> `aQual` SVM file (L58) | DERIVED |
| AISettingsFileResolver | 0x433905 | same | same | same | DERIVED |

AI-layer event/commentary functions (not the driving brain, but session-gated): AICornerSpeedComparison
(0x419542, ==5, best-sector commentary eids 13/14/19/20), AIOvertakeDefendStateChange (0x4196D2, ==5,
eids 10/16/3/33/4/5), AIPositionChangeEvent (0x419909, ==5, eids 17/18/7/8/9), AISessionPhaseChange
(0x41A311/0x41A380, ==5 eids 21/22 else 178/179), AIDriverStateTransition (0x41918A, ==7),
AIRetirementVehicleFailureEvent (0x419A15), AIVehicleDamageEvent (0x41A1A7, ==7), AIFormationLap
(0x41A3F8, practice/qual only via `>=0 && <=6 && !=7`), AIOvertake (0x4BEB3C.., stores the global into
brain+0x360 = 864).

Non-AI readers (89 remaining hits): UI/audio/network/race-control/pit-stop/standings/camera/render
functions. No write to 0x7C56A4 exists in the corpus (scan: 0 writes), so the global is set by the
session loader only.

## B. Overtaking session-5 gate (required coverage)

- `ServerSessionGateCheck` 0x42AE60, confirmed at instruction level:
  `0x42AE60 cmp [0x7c56a4],5`; `0x42AE69 mov edx,[ecx+0x454C]` (call_to_box); `0x42AE78 cmp [ecx+0x4094],2`
  (lap_of_run). Returns 1 when session==5 and NOT (call_to_box != 1 AND lap_of_run == 2), else 0.
- `Overtaking` 0x42AFB8-0x42B021: `if (session != 7) { if (session == 5) { if (ServerSessionGateCheck(this)
  || !ServerSessionGateCheck(candidate)) return 0; } else { if (candidate.best_lap >= own.best_lap) return 0; }
  goto LABEL_26; }` (L18-29). In qualifying, an overtake candidate is accepted only when the own gate is 0
  (lap 2 and not called to box) and the candidate's gate is 1; in practice (other non-race) the candidate's
  best lap must be better than the actor's. `v4` accepted -> `this[4479]` interaction_target set and
  lateral_request `this[4360]` = +-3.0 (0x4420) when not already in a mode-2 state (L56-65).

## C. Practice/qualifying best-lap pace anchor (required coverage)

- `sub_4BE760` (called once per AIBehaviorInitialization 0x4BF0A0 L36, with `this+864` = stored session
  type): scans all vehicles' best_lap 0x409C, picks the slowest active car (best_lap > 1s) and the fastest
  car within 1.15x of it, and selects a per-car ability term: race (864==7) uses 0x44C4 race_ability_norm,
  non-race uses **0x44C0 qualifying_ability_norm** (L33-36, L53-56). Output: pace anchor `this+844`
  (0x34C) = interpolated best-lap anchor, floored at 0.97x the slowest; fallback = season table + blend.
  This is the practice/quali best-lap pace anchor.
- `AIDriverSorting` 0x4BF390: non-race ordering compares best_lap 0x409C (`>=` plus `>= 0.0`, L52); race
  ordering compares lap count and finish-time (L45-50).
- `AIBehaviorInitialization` 0x4BF0A0: L83 race bypasses, non-race requires `lap <= StateFlagsLookup(veh)`
  (0x42AE90 returns per-pit-plan row flags); L102-103 writes best_lap from the paced value.
- `AICornerSpeedComparison` 0x419500: sector-time commentary only when session==5; compares the driver's
  sector time against `flt_5F0780` best-sector table (personal best) -> eids 13/14 (new best) or 19/20.

## D. AIPushLap non-race pace ramp and AIAbility race-only logic (required coverage)

- `AIPushLap` 0x42A180 L15-32: only when AI active (`*(*this+13000)==1`). Race (`==7`) skips the block
  entirely. Non-race: `v4 = 0x44F4 (ability_baseline_scalar) + 0x44D8 (race_ability_push_scalar)`; clamped
  to at most `this[4431] + min(laps,10)*0.003`, then `0x453C = v4`. So in practice/quali 0x453C ratchets
  UP toward baseline+push by +0.003 per plan-lap (max 10), once per AIBehaviorInitialization, and never
  gets the 4 s two-write AIAbility pattern.
- `AIAbility` 0x429470: the entire pace-adaptation block (L36-72) and the end-of-race/pit logic
  (L96-186) run only when `dword_7C56A4 == 7` (six `cmp [0x7c56a4], 7` sites, register ebp=7). Non-race
  AIAbility only: AIDifficultyScalar / 0.75 cap writes to `this[4447]` (0x457C throttle_cap_difficulty),
  the mode-1 recovery branch (L83), and the pit compound decision reached through the pit-state machine.
  Consequence: in qualifying 0x453C stays at the AIPushLap anchor (or parse-time value), no 4 s steps.

## E. Qualifying out-lap / in-lap paths (required coverage)

- Non-race reset mode is 5, not 4: AIMainUpdate L223-226 and AIRacingLine L170-173 set driver_status
  0x4510 = 4 (race) / 5 (non-race) when the car leaves the track/recover-watchdog fires; D8 confirmed
  modes 0/4 in race logs, and the practice log recorded mode 3 (garage exit, finding) then 5->... .
- `sub_429960` (pit plan) non-race switch: cases 0/1/2/6 -> random plan `Rand%2+3` stops, first stop
  3..7 laps out; cases 3/4/5 -> `1/21` chance of 4 stops else 3, evenly spaced (12/laps + 2). The
  qualifying plan row is then consumed by PitStopDecisionTireCompound (non-race branch L35-43 uses
  `4*cursor+4456` = next plan lap) and PitStopBlockingCondition (non-race = plan row flags `& 8`).
- `AIBehaviorInitialization` L83: non-race re-init only when `lap <= StateFlagsLookup(veh)`, i.e. the
  out-lap behavior init is gated by the pit-plan row; race re-inits unconditionally.
- `sub_4A4110` (AIMainUpdate L291, mode 4/5 wait): race returns 1 immediately (L73); sessions 3/4/5 use
  divisor 1/11, sessions 0/1/2/6 use 1/14 for the `1 - remaining_cars*factor` term; the fire decision
  at L267 in non-race is `rand % floor(...)`. (GUESS: session-scaled field/pit timing term.)
- `AISettingsFileLoader`/`AISettingsFileResolver` (IterationCountMap 0x432E50: <3 -> 7, 3..5 -> 5,
  >5 -> 7): AI SVM setup files. Sessions 3/4/5 load the `aQual` variant; races and practice 1/2 load
  the race variant. This is the one place where practice 3/4 behaves like qualifying.

## F. QUALIFYINGABILITY 0x44C0 consumer search (required coverage) -- prior-art correction

Capstone, displacement 0x44C0 on any base (all 7128 .text functions):

| Address | Function | Access | Instruction |
|---|---|---|---|
| 0x4240D5 | sub_424080 (.rcd parser) | write | `mov dword [esi+0x44c0], 0x3f000000` (default 0.5f) |
| 0x4241E6 | sub_424080 | write | `fstp dword [esi+0x44c0]` = `(22 - QualifyingAbility) / 22` |
| 0x4BE7F7 | sub_4BE760 | READ | `mov ecx, [ecx+0x44c0]` (non-race pace anchor, C) |
| 0x4BE892 | sub_4BE760 | READ | `fld dword [ecx+0x44c0]` |
| 0x4BEA1E | RandomVariationScaler 0x4BEA10 | READ | `mov ecx, [eax+0x44c0]` (non-race; race uses 0x44C4) |

Contradiction with prior art: `investigations/ai_interaction_role/README.md` (via
`phase3/PRIOR_ART_MAP_2026-10-02.md` row "0x44C0") says no consumer beyond the parser. At instruction
level there ARE three non-race consumers (`sub_4BE760` x2, `RandomVariationScaler` x1), both in the AI
pace-anchor / random-variation chain reached from `AIBehaviorInitialization` (0x4BF0A0). The parser
writes are consistent with finding `rcd-offsets-live-confirmed` (0x44C0 = 1 - QualifyingAbility/22,
default 0.5). So QualifyingAbility does affect non-race AI pace/random variation; RaceAbility
(0x44C4) replaces it in races. No other reader of 0x44C0 exists in the corpus.

## G. Testable predictions for the qualifying log (for Claude's watcher)

The upcoming/recorded qualifying folders are `watch_20261002T203309Z` and `watch_20261002T203532Z`
(the latter still growing; not touched by this task). Fields are those in `header.json` (v1.2 types at
record time). Check with `overview.jsonl`, `events.jsonl`, `situations/index.jsonl`, `cars_static.jsonl`.

1. `ai_pace_multiplier` 0x453C: NO 4 s AIAbility two-write pattern (D4's race signature). Expect
   per-car piecewise motion in steps of at most +0.003 per AIBehaviorInitialization, clamped by
   `ability_baseline_scalar + race_ability_push_scalar`; expect a much lower event count than Monza's
   5467 and no 4.1 s cadence. Check: per-car 0x453C deltas and inter-write intervals; compare against
   a D4-model replay with the race branch disabled.
2. `driver_status` 0x4510: mode 5 (non-race reset) should appear when a car goes off/recovery-watches;
   mode 4 (race pit wait) should be rare/absent except race-control-like cases. Check `event_driver_status`
   events and situations; D8 saw 0/4 only in race logs.
3. `call_to_box` 0x454C: plan still exists in qualifying (sub_429960 case 5); expect at most one
   in-lap/out-lap box call per car if a stop is scheduled, with `pit_plan_count` 3-4 and evenly spaced
   plan laps (12/plan_count + 2). Check `sit_pit` folders, `call_to_box` edges, `pit_plan_cursor`.
4. `next_chosen_tires` 0x458C / wet logic: AIPushLap wet compound selection is session-independent
   (`flt_8AFEE8` bands, L44-84); in wet qualifying expect 4/5/6 instead of dry 3. Check cars_static +
   pit situations if the session is wet.
5. `best_lap` 0x409C: should be written down monotonically per car during qualifying (AIBehaviorInitialization
   L102-103) and be the AIDriverSorting key. Check per-car min over time; look for the field feeding a
   `+16540`-ordered change.
6. Overtaking gate: `opp_mode` transitions 16->2/1 and 5->2/1 should be rare/absent except in the
   lap-2-without-call-to-box window (ServerSessionGateCheck logic); `lateral_request` 0x4420 = +-3
   from Overtaking is expected only outside that window. Check `event_opp_mode` distribution vs the D6
   race table (16 43.9%, 5 39.1%).
7. `interaction_target` 0x45FC / `yield` situations: the blue-flag auto-yield path is race-only
   (OpponentProximityScanTeamRecognition 0x42A8D7 `==7`), so `yield` situations should NOT fire in
   qualifying; if they do, it is the Overtaking candidate path (0x42B01D-0x42B03C), not blue-flag.
8. QUALIFYINGABILITY: 0x44C0 is not logged, so test indirectly via `cars_static.jsonl` driver ids and
   shipped .rcd values (`value_matches.json`): the non-race pace spread (per-car laps / pace anchor)
   should order by QualifyingAbility, not RaceAbility, and RandomVariationScaler's non-race branch
   should consume 0x44C0. Static prediction only; the field is a candidate for a later watcher request.
9. `AISettingsFileLoader` variant: sessions 3/4/5 use `aQual` SVM; no brain log field exposes it, so
   check only via file mtime/hash in a controlled session if at all.
10. `event_braking_start/end`, corner-speed targets (0x43FC): D1's corner-speed formula is
    session-independent and already PASS on Monza wet/dry; expect the same 0x43FC relationship in
    qualifying (regression check, not a qualifying-specific effect).

## Limits / open

- The 4 functions whose starts are outside .text (4 of 7132) are not scanned; all AI functions are.
- Function classification (AI vs not) is by map membership/name; the non-AI list is not itemized here
  (see the raw log for all 139 hits).
- `sub_4BE760` and `RandomVariationScaler` meanings remain GUESS except the 0x44C0/0x44C4 reads proven
  at instruction level here.
- The `yield` situation kind is expected from the new watcher; no qualifying folder checked in this task
  (E1 is static; Claude runs the checks).
