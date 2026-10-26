# Pit Stop System — Comprehensive Static Analysis

> Generated 2026-07-29 from decompiled `functions/*.c`, `skeleton_facts_all.json`,
> `name_index.json`, `config_schema.json`, `persistence_map.jsonl`, and live
> trace data (`docs/pitstop_full_trace.txt`).
> Trust tiers: FACT (live-poke proven) / CONFIRMED (2+ independent sources) /
> DERIVED (grounded, unproven — verify before building a mod on it) / GUESS.

---

## 1. Architecture Overview

The pit stop system spans ~40 functions across 3 major subsystems:

| Subsystem | Key Function | Role |
|---|---|---|
| **Vehicle pit state machine** | `PitStateUpdateMachine` @ `0x004a6e50` | Per-car state dispatcher, called every tick |
| **Steward (Race Control)** | `Steward_Init` @ `0x004d38f0`, `Steward_Restart` @ `0x004d3cf0` | Track config, pit poly, speed limits |
| **Pit crew visual/animation** | `PitCrewStateMachine` @ `0x004bbfc0`, `PitTimingUpdate` @ `0x004b7a90` | Crew animations, car-proximity detection |

The state machine object is **embedded at veh+0x32CC** (CONFIRMED via two
independent derivations; every offset in the live pit-stop trace lands on a field
this base predicts, while base 0x32C0 would put pittingStatus at 0x3444 which
never moved across 4 traced cycles).

Called from `VehiclePhysicsModeDispatcher` @ `0x004e4930`, itself called from
`MainGameTickUpdate` @ `0x0042c690` — so every car's pit state runs every frame.

```c
// VehiclePhysicsModeDispatcher_004e4930.c
PitStateUpdateMachine(this + 12992, a2);
// where this = veh+0xC, so 0xC + 12992 = 0x32CC
```

---

## 2. The Pitting Status Enum

**Location**: veh+0x3450 (= pit-object offset 0x384), type int32

CONFIRMED end-to-end across 4+ live pit-stop cycles (trace captured 2026-07-28,
150ms poll of full 0x48C0 vehicle struct, small-int-diff filtered):

| Value | Meaning | Paired Transitions |
|---|---|---|
| **0** | On track (normal racing) | — |
| **1** | Pit requested / driving toward pits | — |
| **2** | Entering pit lane | `+0x344C` flips 0→1, `+0x40DC` flips 0→1 |
| **3** | Stopped in pit box (service active) | `+0x345C` (serviceReady latch) flips 0→1 |
| **4** | Exiting box (driving toward pit exit) | — |
| **back to 0** | Crossed pit exit line | `+0x344C` flips 1→0, `+0x40DC` flips 1→0 |

**Code dispatch** in `PitStateUpdateMachine` (`functions/PitStateUpdateMachine_004a6e50.c`):

```c
void __thiscall sub_4A6E50(int this, float *a2) {
    int v3 = *(this + 388);  // = pittingStatus

    if (!v3 || v3 == 4) {
        if (dword_7C5698) return;  // session init gate
        if (v3 == 4) CollisionAvoidanceCheck((_DWORD *)this);
    }

    if (!*(*(_DWORD *)this + 13000) && !VehicleControlEnabled((int *)this)) {
        // AI-controlled or control disabled
        if (*(this + 388) == 2
            && VehicleProximityCollisionDetection((int *)this)
            && !*(*(_DWORD *)this + 13000)) {
            dword_7CE258 = 1;
            StateChangeNotify(this, 4);  // transition to state 4?
        }
        goto LABEL_24;
    }

    switch (*(this + 388)) {
    case 1:
        if (*(float *)&a2 - *(float *)(this + 392) > 150.0)
            StateChangeNotify(this, 0);  // timeout -> reset to 0
        goto LABEL_24;
    case 2:
        if (!VehicleProximityCollisionDetection((int *)this))
            goto LABEL_24;
        *(this + 400) = 0;              // clear serviceReady
        StateChangeNotify(this, 3);      // 2->3
        if (dword_7C54F0[dword_7C54DC] + 18580 == *(this + 4))
            ConditionalIndexSetter(2);
        break;
    case 3:
        if (*(this + 400))              // serviceReady
            PitStopProgress(this, *(float *)&a2);
        goto LABEL_24;
    }
    PitStopReadyCheck((float **)this, a2);  // first tick of state 3

LABEL_24:
    if (*(this + 384) && PositionChangeMonitor(*(_DWORD *)this + 12) + 300 != 1)
        sub_4A5F20((_BYTE *)this);  // AI formation-lap commentary
    if (*(this + 428)) {  // auto-pilot byte
        if (*(this + 388) != 3) {
            *(this + 428) = 0;
            ShortComputation_d410(&dword_5EA510);
        }
    }
}
```

---

## 3. Pit Entry — Driving In

### Player Pit Request
1. Player presses "Request Pit" → UI menu opens (tires, fuel, damage)
2. Confirm → `callToBox` (`veh+0x454C`) set to 1
3. Car must navigate into pit lane

### AI Pit Request
`AIPitStrategyState` @ `0x0041a200` checks:
- `dword_7D0308` (RaceActiveFlag) must be non-zero
- `flt_7CF3C8` (TrackConditionState) must be non-zero
- `dword_7C56A8` (RacePhase) != 8
- AI must be the spectated car OR fires commentary for the spectated car

`RacePhasePitEntryTrigger` @ `0x00429eb0` calls `FuelMultiplierValue`:
```c
// FuelMultiplierValue_0042ad20.c
if (dword_7D0B64)
    return CRTFtol(fuelMass * fuelConsumptionFactor - 0.2);  // laps remaining
else
    return 100;  // fuel multiplier disabled → never pits
```
If `lapsOfFuel < 1`, auto-enters pits.

### Pit Lane Detection
Entering the XPITIN 3D polygon (loaded in `Steward_Init`):
- `veh+0x344C` → 1 (in pit lane)
- `veh+0x40DC` → 1 (in pit lane)
- Speed limiter engages → pit speed = `NormalPitKPH` from `.gdb` file

**Pit speed limit**: Read from `.gdb` key `NormalPitKPH` (53 files, range 60-80 km/h).
Converted in `Steward_Restart`: `value * 0.27778 = m/s`.
Enforced by `sub_4BCCC0` penalty handler with string `"Speeding In Pitlane"`.

---

## 4. Arriving at the Pit Box — Engaging Pit Stop Mode

### Car-to-Stall Proximity
`PitTimingUpdate` @ `0x004b7a90` runs per frame:
- Iterates all cars (count from `dword_5C329C`, FACT root `VehicleCount`)
- Accumulates per-car timer in array at `this+0x3452` stride `4*carIndex`
- When per-car timer > `this+0x3964` threshold → calls `PitStallProximityCheck`

`PitStallProximityCheck` @ `0x004b5da0`:
- Reads `PitStallRingBuffer` via `CircularBufferCheck`
- For each entry: compares car track-distance against stall position
- When `trackDistance - stallPosition < 1.5m` → calls `PitstopActionSet` @ `0x004b6940`

### Transition to Pit Stop Mode
`VehicleProximityCollisionDetection` detects car has come to rest:
→ `PitStateUpdateMachine` case 2: transitions pittingStatus **2→3**
→ `StateChangeNotify(this, 3)` sets new state

---

## 5. Pit Stop Service — Timer, Refueling, Tire Swap

### 5a. Service Initialization (PitStopReadyCheck)

`PitStopReadyCheck` @ `0x004a6200`, called on the first frame of pittingStatus == 3:

1. **Velocity check**: computes `vx² + vy² + vz² > 0.04` → if true, set serviceReady=0 (abort)
2. For player (`obj+0x32C8 == 1`, i.e. slot 0): calls `PitStopDecisionTireCompound` which may also set serviceReady=0
3. For non-race sessions (`dword_7CE048 == 1`): sets serviceReady=0 (forces empty stop)
4. If serviceReady stays 1:
   - Saves `stopStartTime` = `a2` (current sim time → `this+0x390`, veh+0x346C)
   - Calls `PitStopOperationTimer` → returns computed **duration** in seconds
   - Sets `stopEndTime` = `stopStartTime + duration` (at `this+0x3A4`, veh+0x3470)
   - **For player**: sets auto-pilot byte (`this+0x3FC` = 1), triggers 2-second countdown:
     ```c
     // LeftHandPitFlag (0x5EA3EC) selects countdown variant
     if (byte_5EA3EC)
         ShortComputation_d3c0(&dword_5EA510, 10);  // left-handed pits
     else
         ShortComputation_d3c0(&dword_5EA510, 9);   // right-handed pits
     ```
   - Calls `sub_4A3320` to position pit crew 3D transforms at the stop location

### 5b. Duration Computation (PitStopOperationTimer)

`PitStopOperationTimer` @ `0x004a6340`:

Duration = **maximum of all enabled sub-services**, each with random noise.
Returns 0.0 if `PitStopBlockingCondition` == true for player.

**Penalty factor** (= `this+0x63`, offset in object):
```c
penalty = (fuelAtStopStart + 25.0 - simTime) * 0.5;
if (penalty < 0.0) penalty = 0.0;
if (penalty > 4.5) penalty = 4.5;
```
Penalizes late stops relative to the fuel window.

**Noise table**: `flt_8AAE74` at VA `0x008AAE74` (256-entry float array).
Advanced sequentially via `dword_8AAE70`. Each service draws one noise value, doubled:
```c
noise = flt_8AAE74[dword_8AAE70++];
noise = noise + noise;  // doubled
```

**Sub-service times**:

| Service | Formula | Constants |
|---|---|---|
| **Refuel** | `(targetFuel - currentFuel) / 12.0 + noise + penalty + 1.5` | flow rate `flt_59E96C` = 12.0 L/s |
| **Tires** | `noise + penalty + 6.5` (max of 4 corners) | `flt_59E960` = 6.5s |
| **Front Wing** | `penalty + 8.0` | `flt_59E4DC` = 8.0s (shared, 8 readers!) |
| **Rear Wing (broken)** | `penalty + 39.0 + noise` | `flt_5ACB54` = 39.0s |
| **Repair** | `penalty + 8.5 + noise` | hardcoded 8.5 |

**Patc hable constants**:

| VA | Value | Name (Derived) | Readers | Safe to patch? |
|---|---|---|---|---|
| `0x59E96C` | 12.0f | RefuelFlowRate | 2 | Yes — patch both fuel and timer together or gauge disagrees |
| `0x59E960` | 6.5f | TireChangeBaseTime | 2 | Yes |
| `0x5ACB54` | 39.0f | RearWingSetTime | 1 | Yes |
| `0x5ACB58` | 0.08333f | InvRefuelRate | 2 | Yes (keep = 1/flow_rate) |
| `0x5ACB5C` | 4.5f | PenaltyCap | 2 | Yes |
| `0x59E4DC` | 8.0f | WingRepairTime | **8** | **NO — 8 readers across unrelated systems** |

No config file token controls pit service duration or refuel rate. All are hardcoded.

### 5c. PitStopBlockingCondition

`PitStopBlockingCondition` @ `0x0042aee0`:

```c
int __thiscall sub_42AEE0(_DWORD *this) {
    if (dword_7C56A4 != 7)  // non-race session
        return this[4 * this[4472] + 4455] & 8;  // per-stop schedule flag

    // Race session
    if (this[4111] >= dword_8ACF8C)   return BLOCKED;  // past total laps
    if (dword_7C56A8 == 8)            return BLOCKED;  // session ending
    if (this + 14842)                  return BLOCKED;  // callToBox byte
    if (this[*this + 3352] && !this[*this + 3416])     return BLOCKED;  // rear wing broken
    return OK;
}
```

The rear wing condition `(veh+0x3352 != 0 && veh+0x3416 == 0)` is the retirement
trigger. When rear wing is recorded as broken and not intact, the blocking condition
returns true, which in `PitStopOperationTimer` returns `0.0` as the duration. This
causes the car to skip service entirely and retire from the pits.

### 5d. Service Progress (PitStopProgress)

`PitStopProgress` @ `0x004a68b0`, called every tick while pittingStatus == 3 and serviceReady == 1.

**While simTime < stopEndTime** (service still running):

1. **Auto-pilot countdown**: if `this+0x428` (auto-pilot byte) is set and
   `stopEndTime - 2.0 <= simTime`: clears auto-pilot and releases player control
   via `ShortComputation_d410`.

2. **Refuel ramping**: if `FuelRampActive` (`this+0x448`) is set:
   ```c
   fuelNow = (simTime - rampStart) * 12.0 + fuelAtStart;
   if (fuelNow > targetFuel) fuelNow = targetFuel;
   SpeedValueSetterWithGlobalMirror(fuelNow);
   ```

3. **Tire change animation**: if `TyreChangeFlag` (`this+0x452`) is set:
   - At `jobStart + 0.2s` to `jobStart + 0.8s`: front wheels ramp 0→0.096 clamping
   - At `jobStart + 1.4s` to `jobStart + 2.0s`: rear wheels ramp 0→0.096 clamping
   - At `jobEnd` (`this+0x468`): all forces reset to 0.0
   - Performed via `WheelForceParameterSet(frontL, frontR, rearL, rearR)`

**When simTime >= stopEndTime** (service complete):

1. Caps fuel to targetFuel (final refuel tick)
2. If ReasonCode (`+0x4068`) == 9: resets to 0
3. Calls `StateChangeNotify(this, 4)` → transitions pittingStatus **3→4**
4. Records measured duration: `this+0x3A8` (veh+0x3474) = `dbl_8AFEA0 - this+0x390`
5. For AI (`*(this+0x32C8) == 1`):
   - Calls `PostPitCleanup`: increments stop counter, sets anti-repeat cooldown
   - Sends AI command 15: `AICommandMessageQueueDispatcher(veh, 15, 0)` — exit box
6. Resets wheel forces: `WheelForceParameterSet(0.0, 0.0, 0.0, 0.0)`
7. Releases camera: `CameraModeSetter(0)`
8. Plays lollipop-raised sound: `AudioSoundEffectController(veh+0x1045)`

---

## 6. Pit Stop Animations and Crew

### Live trace evidence (from `pitstop_full_trace.txt`)

**Pit entry** (7.09s, pittingStatus 1→2):
```
+0x0B2C=2->1 | +0x0C54=2->1
+0x344C=0->1 | +0x3450=1->2 | +0x40DC=0->1 | +0x443C=0->1
```

**Box stop** (14.18s, pittingStatus 2→3):
```
+0x3450=2->3 | +0x345C=0->1 | +0x3478=0->1 | +0x3488=0->1
```
`+0x345C` = serviceReady latch, `+0x3478` = lollipop one-shot (RECALCULATED!).

**Service complete cluster** (tire-swap tick, various times):
```
+0x125C, +0x3478, +0x3488, +0x348C, +0x3490 — all flip together
```

**Tire compound during stop**: `+0x397C` = 1→4 observed mid-stop.

**Stop counter**: `+0x406C` increments 0→1→2→3 across 3 stops (once per visit).

### Pit Crew Animations

`PitCrewStateMachine` @ `0x004bbfc0`: Listens for animation events with mask
`0x680000`. On crew-animation-enter: sets `this[12]=2`, plays jack/lollipop sound.
On animation-complete: sets `this[12]=0`, plays stow sound.

`PitCrewMeshLoader` @ `0x004a2010`: Loads 3D crew meshes.
`PitCrewAnimationLoader` @ `0x004a2a90`: Loads `.MAS` animation files.
`PitCrewAnimationStringMapper` @ `0x004a24d0`: Maps animation names to files.

`PitServiceSteeringAnimationStep` @ `0x004b9e40`: Rotates steering wheel during
service based on time step. Cases 0-8 cover animation phases (lock-to-lock, slow
return, center).

### Service-complete offset cluster

These offsets all flip simultaneously at the tire-change completion tick:
| Vehicle Offset | Pit-Object Offset | Likely Meaning |
|---|---|---|
| `+0x125C` | — | Tire state flag |
| `+0x3478` | `+0x3AC` | Lollipop one-shot (RECALCULATED) |
| `+0x3488` | `+0x3BC` | Refuel-complete flag |
| `+0x348C` | `+0x3C0` | Fuel job end marker |
| `+0x3490` | `+0x3C4` | Tire job end marker |

---

## 7. Leaving the Pit Box

When `PitStopProgress` detects `simTime >= stopEndTime`:

1. `StateChangeNotify(this, 4)` → pittingStatus **3→4**
2. Auto-pilot byte cleared → player regains control
3. AI: `AICommandMessageQueueDispatcher(veh, 15, 0)` → AI drives out
4. `WheelForceParameterSet(0,0,0,0)` → all wheel clamping released
5. `CameraModeSetter(0)` → camera mode reset

No explicit "pit-box lock" flag exists. The lock is implicit in pittingStatus == 3.
When state transitions to 4, physics resumes and the car is drivable.

**Timing from trace** (1st stop):
- pittingStatus 3→4 at t=24.14s
- Car reaches exit line at t=29.72s → ~5.6s of driving
- Auto-pilot 2-second countdown releases control 2s before stopEndTime

---

## 8. Driving Out the Pits — Crossing Exit Line

**Pit exit** (pittingStatus 4→0, trace t=29.72s):
```
+0x00C4=1->0 | +0x344C=1->0 | +0x3450=4->0 | +0x406C=0->1 | +0x40DC=1->0
```

Key invariants:
- `+0x344C` → 0 (pit lane flag cleared)
- `+0x40DC` → 0 (in-pit-lane flag cleared)
- `+0x406C` increments → pit stop counter (persists across session)
- Speed limiter disengages

**Exit polygon**: `Steward_Init` loads `XPITOUT` scene node property. Crossing this
polygon triggers the limiter release and pittingStatus 4→0 transition.

`ForcePitExit` @ `0x0042a110`:
- In race (SessionType 7): only forces exit if `lapNumber == totalLaps - 1`
  AND `TeammatePartValueLookup` returns 0 or 4
- In non-race: always forces exit
- Calls `StateConditionalAction` + `AIStateFlagSetter(1)`

---

## 9. AI Pit Strategy

### Strategy Initialization
`sub_429960` @ `0x00429960`, called from `AIDriverStructInit` @ `0x00424790`:

Reads `.hdv` file token `"PitStopStrategies"` with format `"%d,%d,%d,%d"`.
- All 125 shipped `.hdv` files have `NumPitstopsSetting=3` (constant!)
- Each has `Pitstop1/2/3Setting=64` (constant target lap)
- `Pitstop1/2/3Range` varies (130-150 laps, far exceeding any race distance)

Sets vehicle fields:
- `nextChosenTires` (+0x458C) → tire compound for each stop
- `fuelConsumptionFactor` (+0x3990)
- Pit schedule: count at `veh+0x342C`, lap numbers at `veh+0x3430 + 8*i`
- Stop lap numbers consumed by writing 0x80000000 when reached

### AI Pit Commentary on Formation Lap
`AIFormationLap` @ `0x0041a3a0`, called from `sub_4A5F20`:

Gated on: SessionType==7, RacePhase==5, lapNumber==0, ChampionshipMode!=5.
Reads `veh+0x342C` (schedule count):
- case 1 → eid 180 ("1 stop strategy")
- case 2 → eid 181 ("2 stop strategy")
- case 3 → eid 182 ("3 stop strategy")

**Known bug**: All 125 `.hdv` files have `NumPitstopsSetting=3` (constant),
and live reads show `veh+0x342C = 3` for every car. So only "3-stop strategy"
commentary ever fires; 1-stop and 2-stop variants are unreachable without editing
`.hdv` files or patching the count.

---

## 10. Steward System — Race Control Configuration

**Steward object**: global at `0x008ACEE0` (CONFIRMED, from `name_index.json`).

### Steward_Setup @ `0x004d36d0`
Registers three subsystems:
1. Vehicle: `aVehicle` → `sub_5599D0`
2. Timing: `aTiming` → `sub_559A00` with `TimerFrameRateThrottle`
3. Pitstop: `aPitstop` → `sub_559A00` with `TrackTemperatureDisplay`

### Steward_Init @ `0x004d38f0`
- Loads `XPITIN` and `XPITOUT` 3D polygons (pit entry/exit boundaries)
- Loads `XFINISH`, `XSECTOR1`, `XSECTOR2` sector boundaries
- All rendered with RenderFlag 2048 (debug/invisible, collision only)
- Loads `aHelicopter` scene node
- Multiplayer vehicle placement per SessionType/ChampionshipMode

### Steward_Restart @ `0x004d3cf0`
- Reads `NormalPitKPH` from `.gdb` file → converts to m/s: `value * 0.27778`
  - Stored at `Steward+0x212` as float (m/s pit speed limit)
  - 53 `.gdb` files: range 60-80 km/h → 16.67-22.22 m/s
- Session type-specific configuration:

| SessionType | Race Time | Race Laps |
|---|---|---|
| 1 (Test Day) | 3600s (60 min) | `dword_7D04A4` |
| 2 (Practice) | 3600s (60 min) | 0x7FFFFFFF |
| 3 (Practice 2) | 2700s (45 min) | 0x7FFFFFFF |
| 4 (Practice 3) | 2700s (45 min) | 0x7FFFFFFF |
| 5 (Qualifying) | 3600s (60 min) | `dword_7D04A4` |
| 6 (Warmup) | 1800s (30 min) | 0x7FFFFFFF |
| 7 (Race) | from `.gdb` | `dword_7D05A0` |

---

## 11. Key Offset Map

### Pit Object (base veh+0x32CC) Field Map

| Object Offset | Vehicle Offset | Type | Name | Role |
|---|---|---|---|---|
| `0x000` | `+0x32CC` | — | pitObjectStart | Base of pit state machine |
| `0x004` | `+0x32D0` | int32 | position | Session position copy |
| `0x088` | `+0x33D4` | int32 | pitScheduleCount | Number of scheduled stops (= 0x342C relative to pitObject) |
| ... | `+0x342C` | int32 | scheduleCount | Same as above (alias) |
| `0x08C` | `+0x33D8` | int32[] | scheduleLaps | Lap numbers for each stop (stride 8) |
| ... | `+0x3430` | int32[] | scheduleLaps | Same (stride 8 from +0x33D8) |
| `0x108` | `+0x3454` | float | fuelAtStopStart | Fuel when service began |
| `0x10C` | `+0x3458` | float | targetFuel | Target fuel after refuel |
| `0x384` | `+0x3450` | int32 | pittingStatus | 0=track, 1=requested, 2=lane, 3=box, 4=exit |
| `0x388` | `+0x3454` | — | — | (padding?) |
| `0x390` | `+0x345C` | byte | serviceReady | 1=car stopped, service can begin |
| `0x398` | `+0x3464` | — | — | — |
| `0x3A0` | `+0x346C` | float | stopStartTime | Sim-time when service began |
| `0x3A4` | `+0x3470` | float | stopEndTime | Sim-time deadline (LATCHED — set once per stop) |
| `0x3A8` | `+0x3474` | float | measuredDuration | Actual elapsed (endTime - startTime) |
| `0x3AC` | `+0x3478` | int32 | lollipopFlag | RECALCULATED — jack/lollipop one-shot |
| `0x3B0` | `+0x347C` | float | fuelAtStart | Refuel baseline (duplicate?) |
| `0x3B4` | `+0x3480` | float | refuelRampStart | Sim-time refueling began |
| `0x3B8` | `+0x3484` | float | targetFuel | Target fuel level |
| `0x3BC` | `+0x3488` | int32 | refuelComplete | 1=refuel job finished |
| `0x3C0` | `+0x348C` | int32 | fuelJobFlag | Refuel active marker |
| `0x3C4` | `+0x3490` | int32 | tyreChangeFlag | 1=tire change active |
| `0x3C8` | `+0x3494` | int32 | wingChangeFlag | 1=wing change active |
| `0x3CC` | `+0x3498` | int32 | repairFlag | 1=repair active |
| `0x3D0` | `+0x349C` | float | tyreJobStart | Tire animation start time |
| `0x3D4` | `+0x34A0` | float | pitTime[0] | Tire job end time (CONFIRMED name) |
| `0x3F4` | `+0x34C0` | float | refuelJobEnd | Refuel job end time |
| `0x3FC` | `+0x34C8` | byte | autoPilotByte | 1=auto-pilot countdown active |
| `0x400` | `+0x34CC` | — | — | — |

### Vehicle-Level Pit Flags

| Offset | Name | Trust | Role |
|---|---|---|---|
| `+0x32C8` | slotLive | CONFIRMED | -1=inactive, 0=player, 1=AI |
| `+0x344C` | inPitLane | DERIVED | 1=car is in pit lane polygon |
| `+0x3450` | pittingStatus | CONFIRMED | 0-4 enum (see Section 2) |
| `+0x397C` | tireChoice | DERIVED | Tire compound writes mid-stop |
| `+0x3990` | fuelConsumptionFactor | CONFIRMED | L per lap |
| `+0x403C` | lapNumber | CONFIRMED | Current lap |
| `+0x4064` | FinishState | FACT | 0=active, 1=classified, 2=retired |
| `+0x4068` | ReasonCode | FACT | Retirement reason (0=none, 9=DSQ, etc.) |
| `+0x406C` | pitStopCounter | DERIVED | Increments once per visit across session |
| `+0x40DC` | inPitLaneAlt | DERIVED | Another in-pit-lane flag |
| `+0x454C` | callToBox | CONFIRMED | 1=AI requests / player confirms pit |
| `+0x458C` | nextChosenTires | CONFIRMED | Tire compound for next stop |
| `+0xCD4` | FrontWingRecordPtr | CONFIRMED | Pointer to front wing damage record |
| `+0xD14` | FrontWingIntactFlag | CONFIRMED | 1=front wing still attached |
| `+0xD18` | RearWingRecordPtr | CONFIRMED | Pointer to rear wing damage record |
| `+0xD58` | RearWingIntactFlag | CONFIRMED | 1=rear wing still attached |

---

## 12. Complete Pit Stop Lifecycle (from live trace)

```
t+0.00s   pittingStatus=0   On track, racing normally
t+7.09s   pittingStatus=1→2 Entering pit lane
          +0x344C=0→1      In pit lane flag
          +0x40DC=0→1      In pit lane flag (alternate)
          +0x0B2C=2→1      (control state change)
          ... driving down pit lane at limiter speed ...
t+14.18s  pittingStatus=2→3 Stopped in pit box
          +0x345C=0→1      serviceReady latch
          +0x3478=0→1      Lollipop raised
          +0x3488=0→1      Refuel/initial flags
          PitStopReadyCheck: computes service list, sets stopEndTime
          PitStopProgress each tick: refuels, waits for deadline
t+22.18s  +0x3478=1→0      Lollipop drops (service nearly done)
t+24.14s  pittingStatus=3→4 Service complete, exiting box
          +0x0188=0→1,+0x0368=0→1,+0x0648=0→1  (physics/damage reactivation)
          ... car accelerates, leaves box ...
t+29.72s  pittingStatus=4→0 Crossed pit exit line
          +0x344C=1→0      In pit lane cleared
          +0x40DC=1→0      In pit lane cleared
          +0x406C=0→1      Pit stop counter increments
          +0x00C4=1→0      Control mode flag
t+30.47s  +0x403C=0→1      Lap 1 begins
          +0x4094=1→2      lapOfRun increments
```

Total service time: **9.96s** (14.18→24.14s, tire+fuel stop).
Pit lane transit: ~5.6s entry driving + ~5.6s exit driving.

---

## 13. Proven Modding Entry Points

### Byte Patches (hardcoded constants)

| What | VA | Current | To change |
|---|---|---|---|
| Refuel flow rate | `0x59E96C` | 12.0 L/s | Slower/faster refuel |
| Tire change base | `0x59E960` | 6.5s | Faster/slower tire swaps |
| Rear wing set time | `0x5ACB54` | 39.0s | Adjust (rarely reached) |
| Inv refuel rate | `0x5ACB58` | 0.08333 | Must match 1/flow_rate |
| Penalty cap | `0x5ACB5C` | 4.5s | Max late-stop penalty |

### Memory Pokes (live, per-car)

| Action | Expression |
|---|---|
| Extend current stop | `*(float*)(veh+0x3470) += seconds` |
| Force AI to pit | `*(int32*)(veh+0x454C) = 1` |
| Read pit status | `pittingStatus = *(int32*)(veh+0x3450)` |
| Detect box entry | watch `+0x3450` for 2→3 transition |
| Detect service complete | watch `+0x3450` for 3→4 transition |
| Detect pit exit | watch `+0x3450` for 4→0 transition |
| Count pit stops | read `*(int32*)(veh+0x406C)` |
| Check in pit lane | `*(int32*)(veh+0x344C) != 0` |
| Read stop time remaining | `*(float*)(veh+0x3470) - dbl_8AFEA0` |

### .gdb Edit

| Key | Effect |
|---|---|
| `NormalPitKPH` | Pit lane speed limit (km/h → m/s × 0.27778) |

### .hdv Edit

| Key | Effect |
|---|---|
| `NumPitstopsSetting` | Number of scheduled stops |
| `Pitstop1/2/3Range` | Lap range for each stop (min, max, target) |
| `Pitstop1/2/3Setting` | Target lap (always 64 in shipped data) |

### DLL Hook Points

| Hook Site | Called By | Purpose |
|---|---|---|
| `PitStateUpdateMachine` @ `0x004a6e50` | `VehiclePhysicsModeDispatcher` → `MainGameTickUpdate` | Catch every state transition |
| `PitStopReadyCheck` @ `0x004a6200` | `PitStateUpdateMachine` | Modify service list before timer computes |
| `PitStopProgress` @ `0x004a68b0` | `PitStateUpdateMachine` | Intercept service completion |
| `PitStopOperationTimer` @ `0x004a6340` | `PitStopReadyCheck` | Modify computed duration |

---

## 14. Function Index

### Core State Machine Chain
| Function | Address | Lines | Role |
|---|---|---|---|
| `PitStateUpdateMachine` | `0x004a6e50` | 60 | Per-car state dispatch, every tick |
| `PitStopReadyCheck` | `0x004a6200` | 50 | Service initialization, velocity check |
| `PitStopProgress` | `0x004a68b0` | 101 | Per-tick service progress + completion |
| `PitStopOperationTimer` | `0x004a6340` | 276 | Duration computation, noise table |

### AI Strategy
| Function | Address | Lines | Role |
|---|---|---|---|
| `sub_429960` | `0x00429960` | — | Init AI pit strategy from .hdv |
| `PitStopDecisionTireCompound` | `0x0042a3d0` | 132 | Per-stop tire compound + retirement decision |
| `RacePhasePitEntryTrigger` | `0x00429eb0` | 13 | Auto-enter pits when fuel low |
| `PitStopBlockingCondition` | `0x0042aee0` | 9 | Rear wing / lap limit / session-end check |
| `AIPitStrategyState` | `0x0041a200` | 23 | AI pit request + commentary |
| `AIFormationLap` | `0x0041a3a0` | — | Formation-lap pit strategy commentary |
| `ForcePitExit` | `0x0042a110` | 24 | Force exit when no more scheduled stops |
| `FuelMultiplierValue` | `0x0042ad20` | 7 | Fuel laps remaining computation |

### Stops and Service
| Function | Address | Lines | Role |
|---|---|---|---|
| `PostPitCleanup` | `0x0042a6f0` | 13 | Post-stop cleanup, counter increment |
| `PitPartStateCopy` | `0x004a5330` | 102 | Copy pit service state to pit object |
| `PitstopStateTransition` | `0x004a5600` | 29 | Pit-object state change handler |
| `PitStopPhaseAdvance` | `0x004c15a0` | 42 | Race control phase advancement |
| `PitStopMemoryCleanup` | `0x004c0b10` | 22 | Free pit memory on session end |
| `PitStopOrderLoader` | `0x004d12c0` | — | Load pit-stop order list |
| `PitStopOrderRank` | `0x004d1bc0` | — | Rank pit-stop order |
| `RepairServiceStateDispatcher` | `0x004ed250` | 51 | Dispatch damage repair by part index |

### Pit Crew and Animation
| Function | Address | Lines | Role |
|---|---|---|---|
| `PitCrewStateMachine` | `0x004bbfc0` | 39 | Listen for crew animation events |
| `PitCrewMeshLoader` | `0x004a2010` | — | Load 3D crew meshes |
| `PitCrewAnimationLoader` | `0x004a2a90` | — | Load .MAS animation files |
| `PitCrewAnimationStringMapper` | `0x004a24d0` | — | Map animation names to files |
| `PitCrewAnimationOffsetUpdate` | `0x0054bf60` | — | Update animation offsets |
| `PitCrewAnimationMASSetup` | `0x004a3910` | — | Setup MAS animation |
| `PITANIMSMASAnimationFileLoader` | `0x004a3220` | — | Load animation file |
| `PitServiceSteeringAnimationStep` | `0x004b9e40` | 58 | Steering wheel animation during service |

### Timing and Proximity
| Function | Address | Lines | Role |
|---|---|---|---|
| `PitTimingUpdate` | `0x004b7a90` | 77 | Per-frame timer, triggers stall check |
| `PitStallProximityCheck` | `0x004b5da0` | 31 | Car vs pit-stall proximity |
| `PitstopActionSet` | `0x004b6940` | — | Trigger pit-stop sequence |
| `PitTimerStateInit` | `0x004d23d0` | — | Initialize pit timer state |
| `PitTimer` | `0x004d2e50` | — | Pit timer (vehicle physics tick) |

### Steward
| Function | Address | Lines | Role |
|---|---|---|---|
| `Steward_Setup` | `0x004d36d0` | 8 | Register subsystems |
| `Steward_Init` | `0x004d38f0` | 145 | Load poly, sector, placement |
| `Steward_Restart` | `0x004d3cf0` | 426 | Load session config, pit speed |
| `Steward_Post` | `0x004d4600` | — | Post-session cleanup |
| `StewardLightGlowNodeFinder` | `0x004d0e20` | — | Find light glow nodes |

### Commentary (pit-related emitters)
| Function | Address | Lines | Role |
|---|---|---|---|
| `PitExit` | `0x0041c0f0` | 132 | Position-based exit commentary (TV, eids 516-535) |
| `PitSelectionStateMachine` | `0x0041b3c0` | 177 | Yellow-flag commentary, sector based |
| `PitRaceStatusMessageDispatcher` | `0x0041be60` | 60 | Race-status messages (eids 506-513) |
| `PitStateSlotActivation` | `0x0041ab30` | 49 | Per-wheel blister commentary (eids 94-104) |

### Fuel (pit-relevant)
| Function | Address | Lines | Role |
|---|---|---|---|
| `FuelMultiplierValue` | `0x0042ad20` | 7 | Fuel laps remaining |
| `FuelWeightLimitCheck` | `0x0042ad50` | — | Check fuel weight limit |
| `FuelWeightComputation` | `0x0049ba50` | 8 | Fuel weight vs consumption check |
| `FuelConsumptionAndWear` | `0x004ed960` | — | Fuel use + tire wear per tick |

---

## 15. Global Offsets

| Address | Name | Trust | Role |
|---|---|---|---|
| `0x0059E80C` | PitlaneTimeThreshold | DERIVED | — |
| `0x0059E96C` | RefuelFlowRate | DERIVED | 12.0 L/s fuel flow |
| `0x0059E960` | TireChangeBaseTime | DERIVED | 6.5s tire swap base |
| `0x0059E4DC` | WingRepairTime | DERIVED | 8.0s (shared, do not patch) |
| `0x005ACB38` | PitWindowScaleFactor | CONFIRMED | Pit window timing scale |
| `0x005ACB3C` | PitDensityThreshold | DERIVED | — |
| `0x005ACB40` | PitStopTimeoutMultiplier | DERIVED | — |
| `0x005ACB48` | Pit_TimerThreshold | DERIVED | — |
| `0x005ACB54` | RearWingSetTime | DERIVED | 39.0s rear wing |
| `0x005ACB58` | InvRefuelRate | DERIVED | 0.08333 (= 1/12) |
| `0x005ACB5C` | PenaltyCap | DERIVED | 4.5s max penalty |
| `0x005ACB60` | PitRepairThreshold | DERIVED | — |
| `0x005EA3EC` | LeftHandPitFlag | DERIVED | Left-handed pit lane |
| `0x008ACEE0` | StewardObject | CONFIRMED | Steward base pointer |
| `0x008AAE74` | NoiseTable | DERIVED | 256-entry float noise table |
| `0x008AAE70` | NoiseIndex | DERIVED | Table index (wraps) |
| `0x007D0B64` | FuelMultiplier | DERIVED | Fuel consumption multiplier |

---

## 16. Original Source Modules

Only two 2002 `.cpp` files were identified via MSVC `__FILE__` strings:

| Module | Functions |
|---|---|
| `pitcrew.cpp` | `PitCrewMeshLoader`, `sub_4A22D0`, `PitCrewAnimationLoader` |
| `steward.cpp` | `StewardLightGlowNodeFinder`, `Steward_Setup`, `Steward_Init`, `Steward_Restart`, `Steward_Post` |

All other pit functions live in unnamed modules.

---

## 17. Pit-Failure Commentary (NOT implemented)

21 clips have no emitter anywhere in the 7,138-function corpus:

| Group | Clips | EIDs |
|---|---|---|
| Player wheel gun failure | PWS_2001_68-73, 76-77 | 56-70 |
| Player fuel hose issue | PWS_2001_79 | — |
| AI jammed fuel hose | PWS_2001_132-136 | 183-188 |
| AI stuck wheel | PWS_2001_132-136 | 183-188 |

All 21 have `blocked: false` and `cooldown: 1.0` in `data/full_event_dump.json`.
None has `FLT_MAX` cooldown. The cheap 4-byte cooldown fix used for PWS_HIT01-06
is NOT available — every clip needs a new DLL-side emitter with a random-failure
trigger condition.

No known trigger offsets exist. `tools/trace_pitstop_full.py` already scans the
full vehicle struct on every poll, so any flag-shaped failure state will surface
automatically if captured during a future traced pit stop.

---

## 18. Live Trace Findings — 2026-07-29 Session

Enhanced trace tool: `tools/trace_pitstop_enhanced.py` — 40ms poll, 50+ named
float fields + full struct dword diff + dual watch on 20 key ints.

### 18a. Multi-Stop Comparison (same session, player car, Monza)

| Metric | Stop #3 | Stop #4 | Stop #5 |
|---|---|---|---|
| **Services** | tire + fuel | tire + fuel + **wing** | tire + fuel + **wing + repair** |
| **serviceReady delay** | 18.0s | 0.13s | 1.6s |
| **Duration** | 8.44s | 12.95s | 13.06s |
| **Refuel (L)** | 101.4→110.0 (8.6L) | 109.1→116.0 (6.9L) | 112.1→123.0 (10.9L) |
| **Refuel rate** | 12.2 L/s | 12.1 L/s | ~12 L/s |
| **tireChoice** | 4→3 | 3→1 | — |
| **+0x3494 wingFlag** | 0 (unchanged) | 0→1 (wing changed) | 1→0 (wing repair) |
| **+0x3498 repairFlag** | 0 | 0 | 0→1 |
| **lollipop up→down** | — | ~11s | 11.08s |
| **lollipop→exit gap** | — | — | 1.96s |

### 18b. Animation Timing

The lollipop flag (`+0x3478`, RECALCULATED) goes 0→1 at service start
(same tick as `+0x345C` serviceReady→1) and 1→0 ~2 seconds BEFORE the
pittingStatus 3→4 transition. Sequence:

```
serviceReady→1 + lollipop→1  (service begins)
  ... 11.08s of service ...
lollipop→1→0                 (lollipop man releases car)
  ... 1.96s ...
pittingStatus 3→4            (box lock disengages)
```

The lollipop drops BEFORE `StateChangeNotify(this, 4)` — matching the real-world
sequence where the lollipop man signals "go" before the car actually moves.

### 18c. serviceReady Delay — Velocity Gate Variability

The `PitStopReadyCheck` velocity check (`vx²+vy²+vz² > 0.04`) causes massive
variability in when service actually begins after the car enters the box:

| Stop | Delay | Likely cause |
|---|---|---|
| #3 | **18.0s** | Car oscillating after braking, velocity flickering above 0.04 |
| #4 | **0.13s** | Clean stop, car settled instantly |
| #5 | **1.6s** | Brief oscillation |

This explains why pit stops "feel" different lengths even with identical service
selections — the pre-service parking time varies wildly. The game doesn't
consider the car "in the box" for service purposes until it's truly stationary.

### 18d. Wing Repair Duration Confirmed

Static code says wing repair adds `penalty + 8.0 + noise`. Comparing tire+fuel
(8.44s) vs tire+fuel+wing (12.95s) = **+4.51s**. Comparing tire+fuel+wing
(12.95s) vs tire+fuel+wing+repair (13.06s) = **+0.11s** (repair adds negligible
extra when wing repair already dominates). The penalty factor and noise account
for the remaining variance.

### 18e. Tire Wear Fields — CORRECTION

The offsets `+0x3924 (FL), +0x382C (FR), +0x3734 (RL), +0x363C (RR)` named
`tirePuncture_*` in the corpus are **puncture state (blown=0, intact=1)**, NOT
continuous tread wear. A burnout does not change these values. The actual
continuous tread wear field is elsewhere in the struct — likely computed in
`VehicleDynamicsForceAccumulatorIntegration_00438130` and stored at a different
offset. Tires are considered "swapped" by the script when the service-complete
cluster (`+0x125C, +0x3478, +0x3488, +0x348C, +0x3490`) flips at the
`PitStopProgress` completion tick — this is when `StateChangeNotify(this, 4)`
fires and all service flags reset.

### 18f. pittingStatus Can Skip State 1

In two of five traced stops, pittingStatus went directly from 0→2 (skipping
state 1 "pit requested"). The pit request was so fast the 150ms poll missed
state 1 entirely. Code in `PitStateUpdateMachine` shows state 1 times out
after 150s if the car doesn't reach the pit lane — but a fast entry makes
state 1 transient at sub-150ms duration.

### 18g. New Offsets Discovered

Damage location tracking cluster (all flip at service-start tick, t=36.04s in stop #5):
- `+0x12B4, +0x12B8`: damage state array entry 0
- `+0x15A0, +0x15A4`: damage state array entry 1
- `+0x188C, +0x1890`: damage state array entry 2
- `+0x33F4, +0x33F8, +0x33FC`: pit/damage structural flags
- `+0x3D6C` (named `carConditions`): flips 1→0 at service start

These appear to track part damage locations and are cleared/updated when repair
service begins. The stride pattern (0x12B4→+0x2EC→0x15A0→+0x2EC→0x188C) suggests
a 748-byte (0x2EC) per-part damage record structure.

### 18h. HUD Damage Indicator Behavior

The in-game steering-wheel damage indicator updates at the **pittingStatus 3→4
transition** (service complete tick), not progressively during service. The
player sees damage→fixed as an instantaneous snap, which matches the trace:
all service flags reset at the single `PitStopProgress` completion frame.
