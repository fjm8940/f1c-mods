# Investigation: static race-order logic (AIDriverSorting_004BF390)

Date: 2026-08-18. Read-only static investigation of `f1c_ida_decompiled`. No files edited,
no game/process access, no api1.txt, no private keys.

## Commands run (all via `python tools/f1c_mod.py`, read-only)

- `start --json` (mandated bootstrap), `route PATH`
- `search AIDriverSorting`, `inspect AIDriverSorting`, `inspect ConditionalFieldSetter`,
  `inspect sub_4BEB10`
- `search 16536`, `search 16480`, `search 0x4060`, `search 16532`, `search 4BEC55`
- Raw sources read: `AIDriverSorting_004bf390.c`, `ConditionalFieldSetter_004bc2f0.c`,
  `AITickUpdate_004bf4f0.c`, `EngineFailureCheck_004bec55.c`,
  `AIBehaviorInitialization_004bf0a0.c`, `sub_49BBB0_0049bbb0.c`, `sub_4BFF70_004bff70.c`,
  `sub_4C01A0_004c01a0.c`, `sub_4C0850_004c0850.c`, `sub_4B3400_004b3400.c`,
  `PitCrewStateMachine_004bbfc0.c`, `AIOvertake_004beb10.c`, `sub_46BF10_0046bf10.c`,
  `sub_46DC60_0046dc60.c`, `DispatchChainWithVTableCalls_0046da20.c`,
  `AIDriverStateTransition_00419120.c`
- Grep (read-only) over `functions/` for offset writers: `16436`, `16480) =`, `16444) =`,
  `16484) =`, `16536) =`, `4BEC55`

## 1. What the offsets are

### +0x4060 (16480) — position/slot

- 1-based race position. Written ONLY via `ConditionalFieldSetter(x + 0x4034, val)`, which
  writes `(x + 0x4034) + 0x2C = x + 0x4060` (`ConditionalFieldSetter_004bc2f0.c:7-10`).
- Read by ~20 functions as position: HUD `" %d/%d"` (`sub_49C6D0`), grid display 1–25
  (`sub_4D9690`, `TimerCountdown`), standings rebuild (`sub_46BA20`), scoring
  (`SeasonScoring`), pit-exit gap (`PitExit`), commentary (`RaceCommentary`,
  `PerDriverRaceCommentary`), AI steering reference (`AISteeringRacingLineCorrection`).

### +0x4034 (16436) — shared per-driver state sub-object

- `+0x0` = pointer to driver state array, `+0x2C` = state scalar (the position value).
- Same sub-object used by pit subsystem (`PitCrewStateMachine`, `PitTimer*`,
  `PitCloseTeamReadyCheck`), fuel/wear (`FuelConsumptionAndWear`), net sync
  (`NetCommPlayerSlotSync`, `MultiplayerVehiclePlacement`), and the `sub_4BC2x0` sibling
  family (`4BC240/4BC270/4BC2D0/4BC2F0/4BC320/4BC340/4BC9B0`).

### +0x4098 (16536) — total race time (float)

3 writers, 2 readers. All 3 writers are the AIDriverSorting callers, all write immediately
before sorting:

- `EngineFailureCheck:115` — `raceTime(a3+0x354) - v11` (v11 = remaining time in lap →
  exact finish-line crossing time), clamped >= 0.
- `AITickUpdate:34` — `this[213] + v7` (current race time + frame delta).
- `AIBehaviorInitialization:101` — `this+0x354 + v14` (race time + remaining-lap estimate).
- Reader `sub_49BBB0:11` — `dbl_8AFEA0 - car+0x4098` = time since last crossing →
  formatted as `" %.2d:%06.3f"` lap/total-time HUD string.
- Reader `AIDriverSorting:49` — the session-7 tie-break.

Semantic: total race time of the car (skeleton label `totalTimeOnFinishLine [CONFIRMED]`
is consistent). Smaller = better. NOT a penalty, pit, or sector value.

### +0x4094 (16532) — race-state

- `AIRacingLinePathFollowing:100` sets it to 1 (on-track). HUD gates on `== 2`
  (`sub_49BBB0:29`, `sub_49BE50:15`, `sub_4BCCC0:332`). Not checked by any
  AIDriverSorting caller.

### +0x4064 (16484) — vehicle state (corpus: `FinishState [FACT]`)

- Values observed: 0, 1 (racing/processed), 2 and 3 (excluded from sorting).
- Only state→1 writes appear in the five sorting-related functions; state 2/3 written
  elsewhere (unresolved).

## 2. Exact conditions in AIDriverSorting_004BF390

- Entry gate: `dword_5C329C (VehicleCount) >= 1` (lines 14, 18). Scans slots
  v13 = 1..VehicleCount over `dword_7C54F0[]` for the car at each slot.
- Session branch (lines 42–52), `v11 = dword_7C56A4 (SessionType)`:
  - **== 7**: other (slot-car) stays ahead iff
    `otherLap(+0x403C) >= selfLap && (otherLap != selfLap || self+0x4098 >= other+0x4098)`.
    Therefore self is promoted iff `selfLap > otherLap`, or
    `selfLap == otherLap && self+0x4098 < other+0x4098`.
  - **else**: other stays ahead iff
    `self+0x409C (bestLap) >= other+0x409C && other+0x409C >= 0.0`
    (qualy/practice: best-lap ordering).
- On promotion (lines 56–61): `v10=1`; if adjacent
  (`self+0x4060 - 1 == other slot`) abort scan; then
  `ConditionalFieldSetter(self+0x4034, otherSlot)` and
  `ConditionalFieldSetter(other+0x4034, otherSlot+1)`.
- Post-swap shuffle (lines 63–68): every subsequent car found gets
  `ConditionalFieldSetter(car+0x4034, ownSlot+1)`; scan terminates when a car's
  **post-write** slot equals `v12` (self's original slot). Net effect: correct insertion —
  self takes the passed car's slot, passed car + intermediates shift +1, cars behind
  self's old slot untouched.
- **At most one position gained per call** (one swap per invocation).

## 3. Concrete race-session-7 paths to AIDriverSorting

Callgraph note: `EngineFailureCheck`/`AITickUpdate` show `callers: []` because
`AIOvertake_004BEB10` **jumps into** the region
(`JUMPOUT(0x4BEC10/0x4BEEF2/0x4BEF10)`); the decompiler split one large function.

1. **Grid** (pre-race): `sub_4BFF70` → `ConditionalFieldSetter` directly (no
   AIDriverSorting). Gates: `dword_7D0410` in 1..5 (session mode); player slot
   `dword_7D0110` vs `dword_7D011C`; grid built by sorting times
   (`sub_4C0260`/`sub_4C02D0`) or player slot + fill rest.
2. **Race-start / first processing** (once):
   `sub_46DA20 DispatchChainWithVTableCalls` → `sub_46BF10` (gate `dword_7C56A4 > 0`;
   session 7 → time budget 1e9, others `flt_8ACF94 - flt_8ACF90`) →
   `AIOvertake_004BEB10` (gate `this+0x342` flag; `this+0x35C = dword_8ACF8C
   TotalRaceLaps`, `this+0x360 = dword_7C56A4`) →
   `AIBehaviorInitialization_004BF0A0` → **AIDriverSorting** (per car: slot != -1,
   `lap < TotalRaceLaps`, `dword_7C56A4 == 7 || lap <= StateFlagsLookup`, state != 1,
   state not 2/3; lap++ from 0→1, +0x4098/+0x409C/+0x40B8 written, state := 1).
3. **Race ongoing** (every frame, session 7): same chain →
   `EngineFailureCheck_004BEC55` (entry gate `a1 || a2 <= StateFlagsLookup`; per car:
   state == 1, not 2/3, `*a4 < a4[104]` lap-completion event; lap++, +0x4098 = crossing
   time, bestLap update, +0x40B8, then **AIDriverSorting**; recursion continues only
   while `lap < TotalRaceLaps` (line 147); finish handling lines 120–126). Tail calls
   `AITickUpdate` (line 158) because session 7 forces `v15 = 0` (lines 124–125) →
   `AITickUpdate_004BF4F0` (per car: state not 2/3, per-vehicle progress > 0; lap++,
   +0x4098 = current time + delta, +0x40B8, state := 1 unless 3, then **AIDriverSorting**).
4. **Post-race / results**: `sub_4C01A0` (rebuild standings by sorted +0x4710 values →
   `ConditionalFieldSetter`), `sub_4C0850` (results; position = `this[106]+1`) —
   ConditionalFieldSetter, not AIDriverSorting.

Side effect of every position write: `ConditionalFieldSetter` →
`AIDriverStateTransition_00419120` (gates: `dword_7D0308` race-active, `flt_7CF3C8`
track, `dword_7C56A8 != 8` phase, value actually changed). In session 7 it can insert a
**RaceCommentary driver-table entry** (events 120/198/259) on position gain — commentary
feed, not physics.

## 4. Verified gates on ranking updates

| Gate | Location | Effect |
|---|---|---|
| SessionType == 7 | AIDriverSorting:43 | lap+totalTime ordering vs bestLap ordering |
| VehicleCount >= 1 | AIDriverSorting:18 | no sort with no cars |
| vehicle state +0x4064 == 1 (not 2/3) | EngineFailureCheck:31–34, AITickUpdate:25, AIBehaviorInitialization:84–87 | pit/retired/unprocessed cars not sorted (caller-level, not inside AIDriverSorting) |
| lap < TotalRaceLaps (`a3+0x35C` = `dword_8ACF8C`) | EngineFailureCheck:147, AIBehaviorInitialization:82 | finished cars stop being sorted; on finish state := 1 |
| race-time budget `this+0x354 < this+0x358` (or threshold <= 0) | AIOvertake:10–24 | whole lap/sort complex skipped once accumulated time >= threshold |
| SessionType > 0 | sub_46BF10:11 | AI loop only in active session |
| `dword_7D0308 && flt_7CF3C8 != 0 && dword_7C56A8 != 8` | AIDriverStateTransition:12 | position-change notifications (not the sort itself) |

No pit-lane, checkpoint, or grid-position gate exists inside AIDriverSorting or its
callers; "pit exclusion" is indirect via state 2/3 (whether pitting cars actually enter
state 2 is NOT verified — see open questions).

## 5. Verdicts on the current conclusions

1. **"vehicle +0x4060 is live racePosition"** — **CONFIRMED** (mechanism corrected):
   canonical position read by ~20 consumers; mutated at runtime by AIDriverSorting, grid
   setup, net slot sync, standings rebuild, results. It is the `+0x2C` state scalar of the
   sub-object at +0x4034; each change fires `AIDriverStateTransition`.
2. **"AIDriverSorting updates racePosition through ConditionalFieldSetter"** —
   **CONFIRMED** (indirectly: `ConditionalFieldSetter(a1+0x4034, v)` writes `a1+0x4060`
   and fires the transition callback). Correction: the update is an *insertion* (self
   takes passed car's slot, passed car gets own slot+1, intermediates +1), not a value
   exchange, and at most one position per call.
3. **"Session 7 promotes when higher lap count, or equal laps with smaller +0x4098"** —
   **CONFIRMED exactly** (AIDriverSorting:44–49; +0x4098 = total race time, smaller =
   less time = ahead).
4. **"Open question: what is +0x4098"** — **RESOLVED (strong evidence)**: total race
   time (crossing-time at lap completion, refreshed every frame, displayed by HUD). Same
   evidence base as the skeleton's `totalTimeOnFinishLine [CONFIRMED]`.
5. **"Explicit/implicit exclusions"** — **CONFIRMED**: explicit vehicle-state (2/3)
   exclusion at callers; race-length gate (lap < TotalRaceLaps); session-type branch;
   race-time-budget gate; no grid/checkpoint/pit-lane gate inside the sort.

## 6. Verified facts vs inferences vs open questions

**Verified static facts**: all offsets above; all writer/reader sets; sort conditions;
call chains; shuffle mechanics; grid/results writers; state-transition side effects; gate
tables.

**Reasonable inferences**: state 2/3 = pitting/retired (excluded by all callers, corpus
labels `FinishState [FACT]` — labeling of *which* state means pit is inference); +0x4098
exact semantic label; `dword_7C56A4` values (7 = race is proven by code branching; corpus
prose saying "7 = qualifying" in some skeletons is contradicted by the code and should be
re-reviewed); the `JUMPOUT` reassembly of the AIOvertake→EngineFailureCheck single
function (strong — addresses straddle 0x4BEC10/0x4BEC55).

**Still open**: who writes state 2/3 into +0x4064 (no writer found in these functions);
the exact `StateFlagsLookup` gate and `dword_7C56A8` phase semantics at
EngineFailureCheck entry; what sets the `this+0x358` time threshold and its exact meaning;
`dword_7D0410`/`dword_7D0110` grid-slot semantics beyond what sub_4BFF70 shows;
`AITickUpdate`'s per-vehicle progress array (`this+0x400+4i`) — why its `> 0.0` gate
semantics yield lap++ every frame in session 7.

**On-track overtakes**: no code path updates +0x4060 from checkpoint/collision/racing-line
data. Position changes are event-driven (grid, race start, lap completion, per-frame tick,
net, results). Calling a position change an on-track overtake is NOT supported — at most,
`AIDriverStateTransition`/commentary may *narrate* a gain.

## 2026-08-18 correction — raw executable confirms the scheduler reassembly

Read-only disassembly of `F1 Challenge Online.exe` at `0x004BEB00..0x004BEF30`
converts the former jumpout-based reassembly from inference to verified fact.
`AIOvertake_004BEB10` begins at `0x004BEB10`; after initializing the iterator,
it executes a direct `jmp 0x004BEC10` at `0x004BEC03`. The purported
`EngineFailureCheck_004BEC55` entry is simply a fall-through session gate in
that same function's per-vehicle loop. Its tail calls `AITickUpdate_004BF4F0`
at `0x004BEF12` before returning.

Thus the recovered scheduler chain in section 3 is verified at the raw-byte
level: `DispatchChainWithVTableCalls_0046DA20 -> sub_46BF10 ->
AIOvertake_004BEB10/004BEC55 -> AITickUpdate_004BF4F0`. Remove the former
“outer phase unresolved” qualification for this split function only; it does
not establish any writable SDK surface.

## 7. Recommended next static step

Trace writers of `+0x4064` (state) and `+0x4094` (race-state) across the whole corpus
(pit entry/exit, retirement, finish) to pin down state 2/3 semantics and confirm the pit
exclusion claim; then verify the `sub_4B3400` message-dispatch (`case 4/7`
`LapCompletion` path) and `PitExit` (`sub_41C0F0`) interplay with the sort, since both
touch position-adjacent state and are the most likely places where pit/grid behavior
feeds or bypasses AIDriverSorting.

## 2026-08-18 correction — direct state-writer scan narrows, but does not close, state 2/3 provenance

Read-only full-text disassembly of the canonical executable was filtered for every
direct `+0x4064` operand.  The direct stores recovered by that scan write the
literal value `1` (including `AIOvertake`/`AITickUpdate` paths); no direct
immediate store of `2` or `3` was present.  This is a bounded negative result:
state `2`/`3` can still originate in a structure copy, indirect/virtual writer,
or unrecovered code path.  Their producer and exact semantics therefore remain
open, while the already-proved sort exclusion for their observed values remains
unchanged.

## 2026-08-18 correction — race-state is not a direct sorting input

The same full-text direct-operand audit for `V+0x4094` recovered one direct
writer: `AIRacingLinePathFollowing_004E9CA0` reaches raw store
`0x004E9EF8` and writes literal `1` through its verified `P[0] == V`
back-reference. `AIDriverSorting_004BF390` contains no read of `V+0x4094`;
its comparison uses position, lap count, and total race time instead.

This establishes that the recovered race-state write is not a direct sort
input. It neither assigns meaning to state `2` nor excludes an indirect
race-state effect through some other routine.

## 2026-08-18 — Lap-completion and pit-exit dispatch do not directly mutate race order

`sub_4B3400` message class 4, subtype 8, dispatches `LapCompletion` for the
selected vehicle; it has no literal `V+0x4060` write or direct
`AIDriverSorting_004BF390` call. `PitExit_0041C0F0` likewise only reads the
local driver's `V+0x4060` position to select a commentary record before
emitting a driver-table entry. It does not write the position, call the sorter,
or alter the race-order state.

Therefore these two routes can feed later ranking indirectly through their
respective lifecycle/event systems, but they are not direct race-order writers.
The verified ranking mutation remains the `AIDriverSorting` conditional
adjacent-position exchange.
