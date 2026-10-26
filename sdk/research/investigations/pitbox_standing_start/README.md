# Standing start from the pits — the real pit-stop exit, on demand

**Date:** 2026-08-03/04 (deep pass + full live resolution)
**Goal:** keep a car in its own pit box for as long as we want, then deploy it
so it leaves the pits exactly like a real pit stop: slow, on its pit-lane
driving path, under the game's 80 km/h limiter, holding at the exit light if
red, merging at the lane end.
**Method:** reproduce the state a car is in during a genuine pit stop and let
the game's own machinery run the exit. Nothing forged.

Prior art this builds on: `tools/sc_pitbox_trial.py` (placement PROVEN),
`tools/pitbox_hold_release.py` (the stopEnd lever, untested),
`tools/pitbox_route_test.py` (route hand-over, laneMode revert problem),
`investigations/garage_exit_ignition/` (the ignition gate).

**DEFINITIVE CONCLUSION (2026-08-04): the standing-start goal as originally
framed — teleport the car into the box, hold, release — CANNOT produce the
stock clean exit.** The stock exit IS the organic route state (the lane chain
the car builds by driving across the real XPITIN). Every synthetic release
(dS=3, dS=0, handed node, laneMode holds, blocks) ended wobbly or in the
wall. **The correct design: drop the car on the PIT APPROACH (~180 m before
the entry) and call it in ORGANICALLY (0x4590=0 + 0x413C=0) — it does the
whole entry/stop/exit itself.** The pieces below (the switch, the entry set,
the holds) are all still needed — but for the organic flow, not the box
teleport. Full sequence in §9.

---

## 1. Status summary

| Piece | Status |
|---|---|
| The organic-stop switch — `veh+0x344C` (entry latch, M+0x180) | **FACT** — set ONLY by the XPITIN entrance handler (`sub_4A5EA0`), with pit=2 + limiter + machine timestamps |
| Synthesizing the entry set on a placed car → the machine runs the FULL service (2→3→4, stopStart/stopEnd, refuel+pin) | **FACT** — proven on M.Schumacher (2→3 in 0.4 s) and Maylander |
| Natural stop → natural release (go-check dS=4→0) → **perfect exit** | **FACT, driver-confirmed** ("PERFECT EXIT due to the stop having been natural") |
| Hand-rolled exits (dS=3, dS=0+node, laneMode holds) | **FACT: WOBBLY or WALL** — every one |
| The long visible hold — **physics-off `0x32C8=2`** | **FACT** — minutes, zero drift, visible, classified, failure-model-proof |
| The vanilla block — armed retirement (0xD58=0 + cb=1) | **FACT** — holds ~30 s, then the retirement completes |
| The failure model — AI car stopped >~15-20 s → random retire | **FACT** — frozen (act=0) and mode-2 cars exempt |
| Box teleport standing start | **DISPROVEN as a goal** — the stock exit requires the organic route |

---

## 2. The pit-stop machinery, mapped (all code-grounded)

### 2.1 The state machine object

The pit machine is **embedded in the vehicle struct at `veh+0x32CC`**
(`VehiclePhysicsModeDispatcher_004e4930.c`:22,28 calls
`PitStateUpdateMachine(this+12992, ...)`; 0xC+12992 = 0x32CC).
All offsets below are `M = veh+0x32CC`.

| M+ | veh+ | name | role | source |
|---|---|---|---|---|
| 0x184 | **0x3450** | pittingStatus | state enum 0..4 (FACT) | PitStateUpdateMachine switch |
| 0x188 | 0x3454 | state-1 entry time | timeout 150 s → 0 | PitStateUpdateMachine case 1 |
| 0x18C | 0x3458 | state-entry time | written on →1/→2 | StateChangeNotify |
| 0x190 | **0x345C** | serviceReady latch | 1 when car at rest (<0.2 m/s); gates PitStopProgress | PitStopReadyCheck |
| 0x1A0 | 0x346C | stopStartTime | `= SessionTime` when service begins | PitStopReadyCheck:32 |
| 0x1A4 | **0x3470** | stopEndTime | `= stopStart + PitStopOperationTimer()` — **the hold/release deadline** | PitStopReadyCheck:35 |
| 0x1A8 | 0x3474 | measured stop time | written at release | PitStopProgress:70 |
| 0x1B0 | 0x3480 | refuel ramp start | | PitStopProgress fuel ramp |
| 0x1B4 | 0x3484 | refuel ramp slope | | |
| 0x1B8 | 0x3488 | refuel target | | |
| 0x1C0 | 0x3490 | refuel flag | | |
| 0x1C4 | 0x3494 | crew wheel-force hold | `WheelForceParameterSet` ramps — **the box hold brake** | PitStopProgress:26-59 |

### 2.2 The state machine itself (`PitStateUpdateMachine_004a6e50.c`)

```
state = M+0x184 (veh+0x3450)

if (state==0 || state==4):
    if (SessionTickCounter) return          // runs on the physics tick only
    if (state==4) CollisionAvoidanceCheck(M)

if (!veh+0x32C8 && !VehicleControlEnabled(M)):      // "uncontrolled" car
    if (state==2 && proximity-to-box && !0x32C8):
        StateChangeNotify(M, 4)             // drive-through: skip the stop
    goto END

switch (state):
  case 1: if (now - M+0x188 > 150.0) StateChangeNotify(M, 0); break
  case 2: if (!proximity-to-box) break
          M+0x190 = 0; StateChangeNotify(M, 3)      // ARRIVED → box stop
          if (spectated-car) ConditionalIndexSetter(2)
          break
  case 3: if (M+0x190 serviceReady) PitStopProgress(M, now); break
PitStopReadyCheck(M, now)
```

So a car in the box at state 3 with `serviceReady=1` gets `PitStopProgress`
every physics tick, and **the whole box behaviour — including the hold and the
release — is decided inside PitStopProgress by one number: `M+0x1A4` =
`veh+0x3470` stopEndTime.**

### 2.3 PitStopProgress (`PitStopProgress_004a68b0.c`) — the hold AND the release

```
if (now < stopEnd):                          // STILL BEING SERVICED
    ... fuel ramp, crew wheel-force ramps (the brake that pins the car) ...
    return

else:                                        // DEADLINE PASSED → RELEASE
    StateChangeNotify(M, 4)                  // pittingStatus → 4 (exiting)
    M+0x1A8 = SessionTime - M+0x1A0          // measured duration
    if (spectated) sub_4D8760(6,0)           // "pit stop complete" commentary
    if (veh+0x32C8 == 1):                    // AI car only
        PostPitCleanup(veh)                  // 0x4534 timeOfNextExit = 0  ← GO NOW
        AICommandMessageQueueDispatcher(veh, 15, 0)   // THE RELEASE COMMAND
    WheelForceParameterSet(0,0,0,0)          // drop the crew brake
    CameraModeSetter(0)
```

**Hold = `stopEnd` in the future. Release = `stopEnd` in the past.** The game
itself is the one that pins the car (crew brake) and lets it go.

### 2.4 Command 15 (`AICommandMessageQueueDispatcher_00424cd0.c` case 15)

```
if (race && FinishState==0 || non-race && RacePhase!=8):
    if (!RetiredFlag) SessionCleanupFlagClear(veh)   // veh+0x4544=0, ActiveFlag(0x45F4)=1
veh+0x4544      = 0
ParameterValueSetter(18)     // veh+0x4538 maxSpeed = 22.222 (80 km/h limiter)
veh+0x4550 laneMode = 1      // pit-lane waypoint set
veh+0x4548      = 1          // "in the lane" flag, read by AISteering
```

`SessionCleanupFlagClear_0042adb0.c`: `if (!veh+0x4410 RetiredFlag) { veh+0x4544=0; veh+0x45F4 ActiveFlag=1 }`.

### 2.5 The AI side — driverStatus and the go-check (`AIMainUpdate_00424e70.c`)

driverStatus (veh+0x4510) enum: 0=follow, 1=parade, 2=garage, 3=approach,
4/5=pit-entry, 6=pitting, 7=spin (AIMainUpdate_00424e70.md, CONFIRMED).

**Pit entry** is committed by two equivalent paths, both ending in the same
state set — `AIRacingLine_00425920.c:161-181` (LABEL_38, at the pit-in fork of
the racing line) and `AIMainUpdate` when its pit-entry timer
(veh+0x4170, default FLT_MAX per runtime OBSERVED 0x7F7FFFFF) expires:

```
driverStatus = 4 (race) / 5 (other)
callToBox (0x454C)   = 0          ← the request is CONSUMED here
laneMode  (0x4550)   = 1          ← pit-lane waypoint set
throttle/brake reset; 0x4524 = 0; 0x4544 = 0
sub_426430(veh)                   ← waypoint re-acquire against the lane set
```

**While in the box (status 4/5) the AI waits. Every tick it runs the
go-check** (`AIMainUpdate:285-309`):

```
if (SessionTime > veh+0x4534 timeOfNextExit || driverStatus == 4):
    if (sub_4A4110(veh) && ActiveFlag(0x45F4)):
        driverStatus 5 → 3 (approach) / else → 0 (follow)   ← LEAVE
    else veh+0x4534 = SessionTime + 4.0                     ← try again in 4 s
```

`sub_4A4110_004a4110.c` is the lane-clear check: it returns 0 only when a car
with laneMode≠0 is approaching in the lane (lines 46-72); in a race it returns
1 otherwise (line 73-74). **So in a race the AI leaves the box the moment
`0x4534` is in the past and no car is in the lane — that is the game's own
"green light" for an AI pit exit.**

`veh+0x4534 timeOfNextExit` writers: PostPitCleanup (0 in race → go now),
PitStopDecisionTireCompound (`SessionTime+180` when the stop is blocked),
AIAbility, AIDriverStructInit, AIMainUpdate (now+4 on retry).

### 2.6 The route — laneMode is an OUTPUT, the limiter is automatic

`sub_426430_00426430.c` (waypoint acquisition, every AI tick):

- **laneMode (veh+0x4550) is written FROM the waypoint data**, not the other
  way round: on every waypoint advance, `laneMode = waypoint.wpBranchID`
  (line 138). Entering a branch-1 node with laneMode 0 → cmd 14 + laneMode=1
  (lines 126-136); leaving a branch-1 node onto branch 0 → laneMode=0
  (line 129-130). **Hand-writing laneMode is useless — it reverts within a
  tick.** This is why the 8 release attempts that wrote laneMode by hand
  produced a car driving the MAIN path from inside the pit area: straight at
  the wall.
- **The 80 km/h limiter is applied by the route code itself**: speed target =
  `flt_8ACFB4` (= 22.2222 m/s) whenever laneMode==1 (line 145). The
  `0x4538=22.222` write from cmd 15 is a redundant governor; the game releases
  it (cmd 5 → maxSpeed=99999, 0x4544=1) when the car leaves the lane.
- Successor choice: `callToBox (0x454C)` selects the alternate waypoint
  (`+0x114`) when it matches the node's `+0x12C` (lines 118-125) — the pit-in
  fork is chosen by callToBox; once committed, callToBox=0 and the branch-1
  chain carries the car.
- NULL curWaypoint (0x4354) forces a fresh nearest-node query (line 76-83)
  with the laneMode set selector; a STALE non-null waypoint is only re-queried
  past 57 m (line 87) — the garage-wall bug.

### 2.7 The exit line

PittingStatus 4→0 happens on crossing the pit exit line, paired with
0x344C/0x40DC 1→0 (full-trace CONFIRMED). The exit line is the `xpitout`
collision poly (`Steward+0x198`, Steward_Init_004d38f0.c) — the same poly is
the red-light hold point (measured 2026-08-03: cars wait at wp 1134 of 125
with pittingStatus 4, speed 0). Red-light holds are inherent to the game's own
exit; we inherit them by construction.

---

## 3. Why the 8 release attempts failed — each failure now explained

| Symptom (driver-reported) | Root cause (code) | Fix |
|---|---|---|
| Drove into the pit wall | laneMode hand-written → reverted within a tick → main path from pit area (sub_426430:138) | let cmd 15 / the waypoint set the laneMode |
| Fresh pit stop with wheels off | callToBox still 1 → new stop re-armed | driverStatus=4 commit clears callToBox (AIRacingLine:177) |
| Back to the garage mid-exit | garage-exit recipe state (dStat=3 etc.) never matched pit-entry state | use driverStatus=4 (pit-entry), not 3 (approach) |
| Launched off at full throttle | maxSpeed=99999 forced every tick by SC_OUTLAP, fighting the game's limiter | stop writing maxSpeed; the limiter is flt_8ACFB4 via laneMode (sub_426430:145) |
| Sat in the box forever (trial) | **OPEN — primary unknown.** Most likely: go-check needs driverStatus==4 or 0x4534 in the past; trial never logged dStat/0x4534. Also possible: nearest-node query returned 0 from the box (sub_426430:80 → AIMainUpdate:183 returns, no driving) | test matrix in §6 |

---

## 4. THE DEPLOY RECIPE (corrected, game's own machinery)

### 4.1 Placement + HOLD (proven placement + game-own hold)

Timing: the car must take the green with the field (brake-hold rule, FACT);
the start sequence resets a pre-green placement (FACT). Placement happens at
**RacePhase 4** (the start moment), exactly as `pitstart_pump()` does:

```
1. resolve car BY NAME (veh+0x4620) every sample   (hard rule, 2026-08-03)
2. teleport: veh+0x34E0/0x34E4/0x34E8 = PitPos + 2.33 m fwd + 0.227 m up
             (PitPos from aiw_pits.json via veh+0x48A4 AIW team)
   veh+0x34F0 yaw = PitOri; veh+0x58/0x5C/0x60 velocity = 0
3. hand over: callToBox(0x454C)=1, pittingStatus(0x3450)=2,
              laneMode(0x4550)=1, curWaypoint(0x4354)=0
4. the game runs 2→3→4 itself (crew, tyres) — PROVEN, 0 corrective writes
5. HOLD: veh+0x3470 stopEnd = SessionTime + 3600   ← the game's own hold;
         the crew brake (PitStopProgress wheel-force ramps) pins the car
```

### 4.2 DEPLOY (release)

```
1. veh+0x3470 stopEnd  = SessionTime - 0.5   → PitStopProgress release branch:
        state 4, PostPitCleanup → veh+0x4534 = 0 (go NOW), cmd 15
        (laneMode=1, limiter 22.22, 0x4548=1, ActiveFlag=1)
2. veh+0x4510 driverStatus = 4               → the AI's own go-check runs;
        its own pit-entry commit (laneMode=1, callToBox=0) is part of the
        state the game expects
3. veh+0x4534 timeOfNextExit = 0             (idempotent with PostPitCleanup)
4. engine: if RPM < 100 → crank veh+0xA0/0x39A0 = 800 (the ignition gate)
5. safety: 0xD58=1 (no re-retire), 0x4064 FinishState=0, 0x4410=0,
           0x45F4=1, 0x105D=1
DO NOT write: maxSpeed (0x4538), laneMode (0x4550), curWaypoint (0x4354)
— all three are game outputs here.
```

The AI then: go-check clears the lane → driverStatus → 0 → follows the
branch-1 waypoint chain at flt_8ACFB4 (22.22 m/s) → holds at the xpitout
red light if closed → crosses the exit line (pittingStatus 4→0, laneMode→0,
limiter released) → races.

---

## 5. Open questions (ranked)

1. **Why did the trial car sit at state 4 forever?** (see §3 last row). The
   trial never logged driverStatus/0x4534/0x4548. Decisive test: hold a car,
   then deploy with the §4.2 set, and log the full vector at 20 Hz. If it
   still sits, read dStat + 0x4534 + sub_426430's return (nearest-node query
   from the box may return 0 — try handing the nearest branch-1 node instead
   of NULL, i.e. the pitbox_route_test --node idea, but with dStat=4).
2. **Is state-4 hold stable for minutes?** The trial saw 20 s. The stopEnd
   hold (§4.1 step 5) is the designed long-term hold; verify no game override
   re-triggers service.
3. **Red-light behaviour at the box exit**: does the AI stop at xpitout when
   red in this flow (real exits do — inherit by construction, verify once).
4. **Player-car variant**: the go-check (AIMainUpdate) only runs for AI cars
   (0x32C8==1); a player standing start needs a different release (input
   takeover) — out of scope, AI-first.

## 6. Verification plan (live, in this order)

1. **Trace a real AI pit stop** (`tools/trace_pitstop_full.py` pattern + the
   §5.1 vector): capture dStat/0x4534/0x4548/0x4544/laneMode/stopEnd around a
   genuine AI stop to CONFIRM the §2.5 exit sequence in the wild.
2. **Hold test**: `tools/pitbox_standing_start.py hold <track> --secs 120` —
   car must stay in the box with the crew, zero drift, no re-place.
3. **Deploy test**: `... deploy --secs 60` — must exit at 22.2 m/s on the
   lane, hold at the light if red, merge, race.
4. **Full**: `... full` — hold then auto-deploy after N s (standing start).
5. Record every result in `findings.jsonl` (CLAUDE.md rule).

## 7. Files

- `tools/pitbox_standing_start.py` — hold / deploy / state / full (this work)
- `tools/sc_pitbox_trial.py` — the proven placement (keep/pitmode/release)
- `tools/pitbox_hold_release.py` — stopEnd lever prototype (superseded §4)
- `tools/pitbox_route_test.py` — route hand-over experiments
- `patches/full_safety_car/dll/pace_probe.c` — DLL port (pitstart_pump)

## 8. Trust

- §2.2-2.5 control flow, §2.6, §3 root causes: **DERIVED** from the cited
  `functions/*.c` reads (deterministic extraction); the pittingStatus enum,
  driverStatus enum, 0x4534 name, placement, limiter = already FACT/CONFIRMED
  in the corpus.
- §4 recipe: **DERIVED, untested as a whole** — the individual levers are each
  proven (placement FACT; stopEnd DERIVED; cmd 15 DERIVED; go-check DERIVED;
  lane-limiter DERIVED; ignition FACT).
- The "car drives the lane at 22.3 m/s" observation: **OBSERVED live**
  2026-08-03 (Maylander dS6→0, pos -4.5→-25.4, speed 0→22.3→51.9).

---

## 9. THE RESOLUTION (2026-08-04) — the organic flow, fully proven

### 9.1 The organic-stop switch: veh+0x344C (FACT)

```
AIPushLap → ForcePitExit → StateConditionalAction → StateChangeNotify(M,1)  (pit REQUEST)
car crosses XPITIN → StateChangeNotify(M,2)
  → AIStateMachineWithModeDispatch → sub_4A5EA0 (the entrance handler):
      this[384] (0x344C) = 1   ← THE SWITCH, one-shot
      + pit state 2, maxSp 99999→22.222 (the game's own limiter),
        machine timestamps 0x3454/0x3458, the pit radio
PitStateUpdateMachine 2→3 (proximity at the marker) → service → 4 → cmd 15 → release
```

Captured live at 20 Hz (docs/pitstop_switch_trace.txt): the full natural chain
cb→1, lane→1/f44→0 (the fork commit, dS stays 0), then at z=263.02 (the
XPITIN line): 0x344C 0→1 + pit 0→2 + limiter, service (stopStart→stopEnd
~7.3 s), pit 4, wait, natural release (dS 4→0), clean exit.

### 9.2 Synthesizing the entry set works (FACT)

Writing `0x344C=1 + pit=2 + maxSp=22.222 + 0x3454/0x3458=now + lane=1 + cb=1
+ curWP=0` on a PLACED car makes the machinery run the FULL service (2→3→4,
stopStart/stopEnd, refuel+pin). Proven on M.Schumacher (2→3 in 0.4 s) and
Maylander. A dS=5 + tExit-future block is REQUIRED during the placement —
without it the AI drives off dumb before the machine catches him.

### 9.3 The holds (FACT)

| Hold | What | Lifetime |
|---|---|---|
| dS=5 + tExit future | the machine-catch window | until released |
| synthretire (fin=2/ret=1/act=0/PitState=2/hidden) | hidden, physics stopped | indefinite |
| **physics-off 0x32C8=2** | visible, classified, immobile, failure-proof | **indefinite — the long hold** |
| vanilla block (0xD58=0 + cb=1, armed retirement) | held by the game, no per-tick writes | ~30 s, then the retirement completes |

The failure model (CrashPct-based dice, ~15-20 s stopped) hits parked AI cars;
frozen and mode-2 cars are exempt.

### 9.4 The release (FACT)

**Physics on (0x32C8=1) + dS=0 + handed lane node + laneMode=1/0x4548=1 held
+ maxSp=22.222 until the merge — NO crank, NO keeper** (the engine spins up
naturally from the AI throttle; forced cranking only makes starter noise).
But even this is a pale copy — the ONLY driver-confirmed perfect exit came
from the fully organic drive-in.

### 9.5 The design conclusion

```
THE SAFETY CAR MUST NOT BE TELEPORTED INTO THE BOX.
pitstart_drop → the pit APPROACH (180 m before the entry) + the organic call
(0x4590=0 + 0x413C=0) → he drives across the real XPITIN → real stop → held
(physics-off for long waits, or the DLL's leading-car gate) → natural release.
The stock clean exit is the organic route, and only the organic route.
```
