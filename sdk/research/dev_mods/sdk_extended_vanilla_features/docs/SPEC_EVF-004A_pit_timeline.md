# SPEC_EVF-004A — Pit-stop state timeline & Group A

## 0. Data provenance

| Source | Content |
|--------|---------|
| `PitStopOperationTimer_004a6340.{c,md}` | Pit duration calc: base + repair + fuel + tyre |
| `PitStopReadyCheck_004a6200.{c,md}` | Pit box arrival detection, fires events 9/10 |
| `PitStateUpdateMachine_004a6e50.md` | Pit state machine: 0=idle,1=entering,2=in-pit,3=exiting |
| `PitStopProgress_004a68b0.{c,md}` | Progress tracking, 2s-before-end check, StateChangeNotify(4) on complete |
| `PitStopPhaseAdvance_004c15a0.c` | Game-level pit phases 1→7, phase 7 = race restart |
| `PitSelectionStateMachine_0041b3c0.md` | Commentary emitter: events 133/135/137(flag clear), 131, 123-125 |
| `PitStopBlockingCondition_0042aee0.md` | Blocks pit stop if rear wing record exists |
| `PostPitCleanup_0042a6f0` | Cleanup after pit stop completes |
| `ForcePitExit_0042a110.c` | Force pit exit in race: triggers `StateConditionalAction` + `AIStateFlagSetter(1)` |
| `data/full_event_dump.json` | eids 44-47: not blocked, cooldown 1.0 |
| `persistence_map.jsonl` | veh+0x3450 LATCHED, +0x34A0 LATCHED, +0x403C LATCHED, +0x4040 LATCHED |
| `red_flag/gr45.c:571-577` | `player_in_pit_box()`: `veh+0x3450 == 2` → player in own box (FACT, live) |
| `findings.jsonl` | `DISPROVEN`: veh+0x4040 is NOT pit progress; `FACT`: veh+0x454C callToBox = 1 → AI pit |

---

## 1. Pit-stop state timeline

Every field that changes between pit-lane entry and pit-lane exit, in order.

### Phase 0 — Pre-pit: request / approach

| Field | Offset | Type | Before | After | Tier / Persist | Notes |
|--------|--------|------|--------|-------|----------------|-------|
| pittingStatus (player only) | `veh+0x3450` | int32 | 0 | 1 (requested) | DERIVED / LATCHED | red_flag confirms 2 = in box; enum values 0/1 unconfirmed |
| lapNumber | `veh+0x403C` | int32 | lap L | lap L | CONFIRMED / LATCHED | Does NOT change on entry; the next lap starts after exit |
| pit type flag | `veh+0x4040` | int32 | varies | 2 (regular stop) | DERIVED / LATCHED | PitStopOperationTimer fill: `obj+0x4040 == 2 → ++v8`. Read by 7 functions. Exact enum unknown |
| callToBox (AI) | `veh+0x454C` | int32 | 0 | 1 | CONFIRMED / SKIPPED_PROTECTED | Set by AIAbility. Does NOT change for player |
| pitTime[0] | `veh+0x34A0` | int32 | 0 | ? | DERIVED / LATCHED | Labeled "pit time" in LargeConfigFunctionWithPhysicsConstants |

### Phase 1 — Pit lane

| Field | Offset | Type | Before | After | Tier / Persist |
|--------|--------|------|--------|-------|----------------|
| pittingStatus | `veh+0x3450` | int32 | 1 | 2 (in pit lane) | DERIVED / LATCHED |
| speed | `veh+0x4440` | float | ~80 km/h | ~80 km/h (pit limit) | FACT / VOLATILE |

**Detection: pit-lane entry** → `pittingStatus` transitions 1→2. The red_flag script does NOT detect this — it detects pit-box arrival (value 2). For TRUE pit-lane entry detection, see `RacePhasePitEntryTrigger_00429eb0` and `sub_4BCCC0` pit-lane speed enforcement. The pit-lane zone is track-specific (.aiw file).

### Phase 2 — Pit box arrival

| Field | Offset | Type | Before | After | Tier / Persist |
|--------|--------|------|--------|-------|----------------|
| pittingStatus | `veh+0x3450` | int32 | 2 | 2 (in box) | DERIVED / LATCHED |
| speed | `veh+0x4440` | float | ~80 | ~0 | FACT / VOLATILE |
| PitStateUpdateMachine.state | `pitCrewObj+0x184` | int32 | 1 (entering) | 2 (in-pit) | DERIVED / RECALC* |
| PitStateUpdateMachine.ready | `pitCrewObj+0x190` | int32 | 0 | 1 (set by PitStopReadyCheck) | DERIVED / RECALC* |
| pit start time | `pitCrewObj+0x104` | float* | — | `dbl_8AFEA0` | DERIVED / RECALC* |
| pit total duration | `pitCrewObj+0x105` | float | — | computed by PitStopOperationTimer | DERIVED / RECALC* |

\* RECALC = recomputed every frame by the game's own pit state machine. A DLL poke to these fields is **useless** — hook the reader or detect the transition.

**Detection: pit-box arrival** → `pittingStatus == 2` AND speed < threshold. This is exactly what the red_flag NUMPAD2 detection uses (`player_in_pit_box()` at `gr45.c:575`).

**PitStopReadyCheck** fires session events 9/10 at this moment (NOT commentary eids 44-47 — these are separate session-level events triggered via `dword_5EA510` (SessionManager), not `dword_5EA618` (CommentaryController)). It also begins the pit-stop timer calculation.

**PWS_MISC54 (eid 46) should fire here** — this is the "player enters pit box" sound.

### Phase 3 — Pit stop in progress

| Field | Offset | Type | Before | After | Tier / Persist |
|--------|--------|------|--------|-------|----------------|
| PitStopProgress.elapsed | `pitCrewObj+0x1A4?` | float | 0 | T_total | DERIVED / RECALC* |
| fuel level | unknown | float | fuel_before | fuel_after | UNKNOWN |
| per-wheel tyre age | `veh+0x363C` et al. | float | worn | 1.0 (fresh) | CONFIRMED / — |
| PitStopOperationTimer.tyre_flag | `pitCrewObj+0x12C` | int32 | 0 | 1 | DERIVED / RECALC* |
| PitStopOperationTimer.fuel_flag | `pitCrewObj+0xEC→0xF0` | int32 | before | after (if changed) | DERIVED / RECALC* |

**Detection: pit in progress** → elapsed timer in pit box. All time-based triggers (2s before end, etc.) derive from this timer.

**Fuel detection:** fuel level increasing while `pittingStatus == 2`. Fuel offset unknown — needs discovery. The PitStopOperationTimer compares `this[59]` vs `this[60]` (fuel before/after) and adds `v46 + 8.0` seconds if different.

### Phase 4 — Pit completion / lollipop (~2s before exit)

| Field | Offset | Type | Before | After | Tier / Persist |
|--------|--------|------|--------|-------|----------------|
| PitStopProgress.428 flag | `pitCrewObj+0x428` | byte | 1 | 0 | DERIVED / RECALC* |
| SessionManager event | `dword_5EA510` | — | — | event dispatched | DERIVED / — |

PitStopProgress at `functions/PitStopProgress_004a68b0.c:13-17`:
```c
if ( *(_BYTE *)(this + 428) && *(float *)(this + 420) - 2.0 <= a2 )
{
    *(_BYTE *)(this + 428) = 0;
    ShortComputation_d410((int)&dword_5EA510);
}
```
When elapsed time is within 2.0 seconds of total pit duration, it clears the ready flag and fires a session event.

**PWS_PIT09 (eid 44) should fire here** — the "lollipop turns" moment, second-to-last sound before the car is released.

### Phase 5 — Pit exit

| Field | Offset | Type | Before | After | Tier / Persist |
|--------|--------|------|--------|-------|----------------|
| pittingStatus | `veh+0x3450` | int32 | 2 | 3 or 4 (exiting) | DERIVED / LATCHED |
| PitStateUpdateMachine.state | `pitCrewObj+0x184` | int32 | 2 (in-pit) | 3 (exiting) | DERIVED / RECALC* |
| elapsed pit time | `pitCrewObj+0x424` | float | — | `dbl_8AFEA0 - start_time` | DERIVED / RECALC* |
| PitStopProgress complete | `pitCrewObj+0x420` | float | T_total | — triggers StateChangeNotify(4) | DERIVED / RECALC* |
| camera mode | — | — | 1 (pit) | 0 (normal) | DERIVED / — |

PitStopProgress at `functions/PitStopProgress_004a68b0.c:61-99`: when `a2 >= total_time` → calls `StateChangeNotify(this, 4)`, records elapsed time, resets camera mode, calls `PostPitCleanup`, dispatches `AICommandMessageQueueDispatcher(15)`.

**Detection: pit exit** → `pittingStatus` transitions 2→3 or 2→4. The PitStateUpdateMachine state transitions 2→3 (exiting).

**PWS_PIT10 (eid 45) should fire here** — the "pit stop ends, player exits" sound.

---

## 2. Design question: Can the pit-box timer be extended via a tick hook?

The assignment asks:
> Can the pit-box timer be extended from a tick hook?

**Answer: YES, with caveats.**

The pit duration is computed once by `PitStopOperationTimer` at the moment `PitStopReadyCheck` determines the car is ready (near-stationary, `veh+0x32C8 == 1`). The total duration is stored at `pitCrewObj+0x105` (as end-time in session seconds = `dbl_8AFEA0 + duration`). `PitStopProgress` reads `pitCrewObj+0x420` (total pit duration) and `pitCrewObj+0x1A4` (elapsed time) each tick.

The **pit-crew object** is a separate heap allocation, not the vehicle struct. Its state fields are `RECALCULATED` (recomputed every frame by the game). A poke to the timer value in the pit-crew struct would be overwritten next frame.

**For a tick-hook DLL approach:** rather than extending the game's own timer, the cleanest path is:
1. Arm when `pittingStatus == 2` AND speed < 0.5 m/s (pit box arrival detected)
2. Track elapsed time in our own observer state
3. Fire event at the right time offset (eids 44, 45, etc.)
4. If group C failures need to ADD delay, we inject eids 61/62 BEFORE eid 44, and delay the eid 45/44 sequence by our own timer offset

The game's own pit duration (from `PitStopOperationTimer`) serves as our baseline timing reference, not something we modify. We add observer-side delays for failure events.

**Timer writer (persistence_map.jsonl check):**
```
pitCrewObj+0x1B8: RECALCULATED (every frame)
pitCrewObj+0x1D4: RECALCULATED (every frame)
```
The timer is `RECALCULATED` — external poke useless. Must hook the writer or use a parallel tracker.

---

## 3. Group A row verdicts

### ROW: PWS_MISC54 (SDT 45, eid 46)

- **verdict:** NEEDS_TRACKER
- **emitter:** None. No function in the decompiled corpus references eid 46.
- **blocked:** No. `full_event_dump.json`: cooldown 1.0, blocked=false.
- **trigger:** Player enters pit box. `veh+0x3450 == 2` AND `veh+0x4440` (speed) < 2.0 m/s, first detection of this state (edge-triggered, not level).

```
OFFSETS:
  veh+0x3450 → pittingStatus [DERIVED] [LATCHED] — red_flag/gr45.c confirmed 2 = in box
  veh+0x4440 → speed [FACT] [VOLATILE] — m/s, pit-limit is 80 km/h = 22.22 m/s
  dword_7C54E8 → PLAYER_VEH_PTR [CONFIRMED]

TRIGGER:
  if (pittingStatus == 2) AND (speed < 2.0) AND (!already_fired_46):
      inject eid 46

ARM: pittingStatus transitions 0→1→2 (entering pit lane).
FIRE: pittingStatus == 2 for 500ms continuously (debounce).
LATCH: once per pit stop. Reset when pittingStatus returns to 0 or < 1.
```

- **uncertainty:** 
  - `veh+0x3450` enum values 0/1/3/4 NOT confirmed for the player — red_flag only tested value 2
  - Is `pittingStatus` set back to 0 between pit stops? The red_flag `unretire_pitret()` explicitly clears it to 0
  - The AI full-race trace saw `pittingStatus` ALWAYS 0 across 6653 samples — this field is player-only
  - Need live session: enter pit box, observe `pittingStatus` transitions at `tools/poke.py watch veh+0x3450`

### ROW: PWS_PIT09 (SDT 43, eid 44 / 335)

- **verdict:** NEEDS_TRACKER
- **emitter:** None. eid 44/335 not referenced in the decompiled corpus.
- **blocked:** No. eid 44: cooldown 1.0, priority 20, blocked=false. eid 335: cooldown 1.0, priority 70, blocked=false (same clip, higher priority variant).
- **trigger:** Pit stop near completion — "lollipop turns." ~2 seconds before the pit-box timer expires.

```
OFFSETS:
  veh+0x3450 → pittingStatus [DERIVED] [LATCHED]
  dbl_8AFEA0 → SessionTime [DERIVED] — current session time (pitStopReadyCheck uses this)

TRIGGER:
  Arm when: pittingStatus == 2 AND player_in_pit_box (confirmed by red_flag method)
  Track: elapsed_pit_time = current_session_time - pit_entry_session_time
  Fire when: elapsed_pit_time >= (pit_duration - 2.0)
  where pit_duration is measured live (not computed from PitStopOperationTimer)

LATCH: once per pit stop. Reset when pittingStatus returns to 0.

COOLDOWN: none needed beyond the once-per-stop latch — but eid table has 1.0s cooldown.
```

- **uncertainty:**
  - Pit-stop duration varies by tyre compound, fuel quantity, and repairs. Without the pit-crew object pointer, we cannot read the computed total duration. **Options:**
    a) **LIVE READ REQUEST**: measure pit duration for the player across several stops (no tyres, one tyre, full set, refuel)
    b) **Observe pit-crew timer**: find the player's pit-crew object pointer. Route: `dword_7C54F0[dword_7C54DC] + 0x4894` (= veh_player + 18580) may be the pit index; the pit-crew array base is unknown
  - The 2.0s threshold comes from `PitStopProgress.c:13`: `*(float*)(this + 420) - 2.0 <= a2`. Verified from source.
  - Which eid to inject: 44 (priority 20) or 335 (priority 70)? Priority 70 would preempt lower-priority commentary — the human intent (second-to-last voice) likely wants priority 20 so the final "exit" line (eid 45) can still play after.

### ROW: PWS_PIT10 (SDT 44, eid 45 / 336)

- **verdict:** NEEDS_TRACKER
- **emitter:** None. eid 45/336 not referenced in the decompiled corpus.
- **blocked:** No. eid 45: cooldown 1.0, priority 20, blocked=false. eid 336: priority 70, blocked=false.
- **trigger:** Pit stop completes, player exits the pit box.

```
OFFSETS:
  veh+0x3450 → pittingStatus [DERIVED] [LATCHED] — target: transition 2→3 or 2→4

TRIGGER (Option A — state change):
  Fire on: pittingStatus transitions from 2 → (3 or 4)

TRIGGER (Option B — timer):
  If pittingStatus stays at 2 through the entire stop (enum values may differ from theory):
  Fire when: elapsed_pit_time >= pit_duration (timer-complete approach)

LATCH: once per pit stop. Reset when pittingStatus transitions back through 0.

EDGE: fire on the first frame pittingStatus != 2 after having been 2 for >= pit_duration.
      This avoids false-firing if the game briefly sets pittingStatus to 3 and back.
```

- **uncertainty:**
  - Does `pittingStatus` change to 3 or 4 at exit? The name_index enum claims 3=in box, 4=exiting — but that contradicts the red_flag confirmation that 2=in box. The actual exit value must be observed live.
  - `PitStopProgress.c:69`: `StateChangeNotify(this, 4)` is called on exit — the pit state machine transitions to exit state. If we can observe the pit-crew state, the `pitCrewObj+0x184` transition 2→3 is the authoritative signal.
  - **LIVE READ REQUEST**: watch `veh+0x3450` during an entire pit-stop cycle (entry → box → exit → back on track)

### ROW: PWS_MISC18 (SDT 46, eid 47)

- **verdict:** NEEDS_TRACKER (T5 → T3 after analysis)
- **emitter:** None. eid 47 not referenced in decompiled corpus.
- **blocked:** No. eid 47: cooldown 1.0, priority 55, blocked=false.
- **trigger:** "Fuel for one lap OR scheduled pit stop next lap." Two sub-conditions:

```
SUB-CONDITION A: "fuel for one lap"
  fuel_level: unknown vehicle offset
  fuel_per_lap: unknown — from vehicle physics, likely a function of engine + track
  Fire when: fuel_level <= fuel_required_for_one_lap AND not already in pit lane
  
  UNCERTAINTY: fuel level offset not found in name_index.json. 
  Candidates: search for "fuel" in name_index.json, functions related to fuel consumption
  sub_438130 (VehicleDynamicsForceAccumulatorIntegration) handles tyre physics but not fuel

SUB-CONDITION B: "scheduled pit stop next lap"
  lapNumber: veh+0x403C [CONFIRMED] [LATCHED]
  scheduled_pit_lap: unknown — from pit strategy. The game stores pit strategy in player config
  Fire when: lapNumber == scheduled_pit_lap - 1
  
  UNCERTAINTY: scheduled pit lap location unknown. 
  Candidates:
    - ConfigScheduledStopLaps: function `sub_429960` "Initializes AI pitstop strategy (stops, tires, fuel-adjusted lap targets)"
    - Player pit strategy: stored in the <player>.plr file or game options. The `PitStopDecisionTireCompound_0042a3d0` and `sub_429960` handle AI strategy
    - veh+0x342C = aiPitStrategyIndex [CONFIRMED] — reads 3 on all cars, selects AIPit08/09/10 variant. This is AI-only
    - For the player: pit stop strategy comes from the pre-race menu (Pit Strategy) and is set by the player interactively, not stored in vehicle memory in a directly-readable offset

TRIGGER (combined):
  if (not in pit lane) AND (!already_fired_47_this_lap):
      if (fuel_critical) OR (scheduled_stop_next_lap):
          inject eid 47

LATCH: once per lap (or cooldown 1 lap). 
       Reset when lapNumber changes.
       Do NOT re-arm on the same lap even if fuel crosses threshold mid-lap.

PRIORITY: 55 — medium-high. This is advisory ("box this lap"), it should play over ambient chatter.
```

- **uncertainty:**
  - Fuel level offset — **primary unknown**. The vehicle struct has no named fuel field. Check `config_schema.json` for fuel-related tokens, search `priority_index.md` for fuel functions
  - Player's scheduled pit lap — may NOT be directly readable from vehicle struct. The player sets pit strategy in the pre-race menu; this is typically stored in the `.plr` file or a game config struct, not the per-frame vehicle array. **T5 reclassification warranted** for sub-condition B if no readable offset exists
  - "Fuel for one lap" threshold: varies by track length, engine, driving style. A static threshold (e.g., 3.0 liters) may suffice or may be wildly wrong
  - The human note says this "may already play" — check in a race with critical fuel. But `full_event_dump.json` says eid 47 has cooldown 1.0, not FLT_MAX. If eid 47 appears in `timestamps` at `CONTROLLER+0xAF8` during a race low-fuel situation, it means an emitter exists we haven't found

---

## 4. Pit-crew object pointer: the central unknown

The pit-crew object is a **separate heap allocation**, not part of the 0x48C0 vehicle struct. Its fields (`pitCrewObj+0x184` state, `+0x188` timer, `+0x190` ready flag, `+0x104` start time, `+0x105` duration, `+0x420` total) are all `RECALCULATED` — poking them is useless.

**To find the player's pit-crew object:**

1. `PitStopProgress.c:71-72` references:
   ```c
   if ( *(_DWORD *)(dword_7C54F0[dword_7C54DC] + 18580) == *(_DWORD *)(this + 4)
   ```
   `dword_7C54F0[dword_7C54DC]` = player vehicle base. `+18580` = `veh+0x4894`. This DWORD is compared to `pitCrewObj+4` (= vehicle index stored in the pit-crew). So `veh_player+0x4894` likely contains an index or pointer to the player's pit-crew object.

2. `dword_7C54F0` = VehiclePtrArray [FACT]. `dword_7C54DC` = CurrentPlayerIndex [CONFIRMED].

3. The pit-crew array base is unknown. Candidates: search for the `PitCrewInstanceSetup_004ddad0` function which "Sets up pit crew instance" — it likely allocates or accesses the pit-crew array.

**LIVE READ REQUEST for pit-crew object pointer:**
```
precondition: player in pit box (pittingStatus == 2)
commands:
  python tools/poke.py read <dword_7C54F0[dword_7C54DC] + 0x4894>:int32
  python tools/poke.py read 0x5EC6D8+0x4894:int32
distinguishes: if value is non-zero and increments with successive pit stops → pit-crew index/pointer
```

---

## 5. Unbuildable / deferred

| Row | Issue | Search done |
|-----|-------|-------------|
| PWS_MISC18 (fuel side) | Fuel level offset unknown | `name_index.json`: no "fuel" vehicle offset. `priority_index.md`: no fuel-level reader function found. `config_schema.json` search pending |
| PWS_MISC18 (strategy side) | Player's scheduled pit lap not in vehicle struct | `sub_429960` handles AI pit strategy only. Player strategy from `.plr`/menu, not runtime vehicle memory |
| All rows (timer) | Pit-crew object pointer unknown → cannot read internal timer | `PitStopProgress.c` line 71 references `veh+0x4894` — candidate. `PitCrewInstanceSetup_004ddad0` pending |

---

## 6. Live read requests

### LR-004A-1: pittingStatus phase transitions
```
precondition: race, approaching pit lane
commands:
  python tools/poke.py watch veh+0x3450:int32 100ms  -- player
  Drive through: pit-lane entry → pit box → wait for stop → exit → back on track
distinguishes: 
  - Value 1 at pit-lane entry → enum = 1=none, 2=requested, 3=in-lane, 4=in-box
  - Value 2 at pit-box arrival → enum = 0=none, 1=requested, 2=in-box, 3=exiting (red_flag confirms)
  - Value 3 at exit → confirms the exit transition exists
  - Value stays 2 throughout → red_flag confirmed, exit detection must use timer
```

### LR-004A-2: pit stop durations
```
precondition: race, pit stop
commands:
  In pit box: record dbl_8AFEA0 via python tools/poke.py read 0x8AFEA0:double
  On exit: record dbl_8AFEA0 again
  Repeat for: no tyre change, 1 tyre, full set, refuel
distinguishes: calibrates the "2s before end" window for PWS_PIT09.
  If pit duration with no changes = ~5-8s → 2.0s is ~25-40% of total
  If pit duration with full changes = ~10-15s → 2.0s is ~13-20% of total
```

### LR-004A-3: fuel level offset discovery
```
precondition: race, car stationary, record fuel
commands:
  python tools/poke.py snapshot fuel_before
  Advance one lap (time acceleration or pit exit)
  python tools/poke.py diff fuel_before fuel_after
distinguishes: any offset that decreased by 2-5 per lap → candidate fuel level
```

### LR-004A-4: pit-crew object pointer
```
precondition: player in pit box (any pit stop)
commands:
  python tools/poke.py read 0x5EC6D8+0x4894:int32  (veh_player + 18580)
  python tools/poke.py read 0x5EC6D8+0x4894+0x184:int32  (if pointer, read pit state)
distinguishes: if pit state reads 2 → confirmed pit-crew pointer at veh+0x4894
```

---

## 7. Reviewer notes

- Cite every number: file:line for code constants, `full_event_dump.json` for event params, `persistence_map.jsonl` for persistence class.
- The `pittingStatus` enum contradiction (name_index: 0/1/2/3/4 vs red_flag verified: 2=in-box) is flagged. **Trust the live-verified value; do not trust the name_index enum.**
- `veh+0x4040` was DISPROVEN as pit progress by findings. It is read by PitStopOperationTimer as "pit stop type (2=regular)" — that is a different meaning from "pit progress."
- The `dword_5EA510` SessionManager events (9/10) fired by PitStopReadyCheck are NOT commentary events — they are session-level state notifications. Commentary injection uses `dword_5EA618` CommentaryController.
- All 4 rows are Tier T5 by classification but collapse to T3 after analysis: each needs a tracker (timer + edge-detect), mechanism is understood, and emission path is proven (the retire_commentary DLL already injects commentary on the same tick hook with the same function calls).
