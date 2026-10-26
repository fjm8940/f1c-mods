# Pit Stop — Complete Firing Order & Tamper Map

> Holistic reference: every function, every offset, every transition, what can be poked.
> Built from static decompile + 6 live traced stops + Frida probe.
> Last updated: 2026-07-29

---

## 1. Trigger Chain (who calls who, in order)

```
MainGameTickUpdate (0x0042c690)
  └→ VehiclePhysicsModeDispatcher (0x004e4930)     [EVERY FRAME, every car]
       └→ PitStateUpdateMachine (0x004a6e50)         [ditto, veh+0x32CC = pit object]

            ┌─ pittingStatus=0,4 & session init gate → CollisionAvoidanceCheck
            ├─ pittingStatus=1  → timeout check (150s) → reset to 0
            ├─ pittingStatus=2  → VehicleProximityCollisionDetection
            │                     → StateChangeNotify(this, 3)  ⚡ 2→3
            ├─ pittingStatus=3  → PitStopProgress (if serviceReady=1)
            │                     → PitStopReadyCheck (first frame, sets serviceReady)
            └─ sub_4A5F20          → AI formation-lap commentary
                                    → Exiting-pits-under-red penalty

PIT ENTRY DETECTION (separate system):
  PitTimingUpdate (0x004b7a90)                     [EVERY FRAME, all cars]
    └→ PitStallProximityCheck (0x004b5da0)          [per-car timer threshold]
         └→ PitstopActionSet (0x004b6940)           [car <1.5m from stall]
              └→ triggers pit-stop physics event
```

---

## 2. Full Timeline — Annotated from Live Trace

```
         TRIGGER SYSTEM                            VEHICLE STATE CHANGES
         ═════════════                             ════════════════════
                                                   
t-??s    [Player presses "Request Pit" key]
           ↓ UI menu → confirm tires/fuel/repair
           ↓ callToBox (+0x454C) = 1

t+0.00s  (already in pit lane approach)            pittingStatus=0
                                                   callToBox=1

t+N.Ns   XPITIN polygon crossed
         → speed limiter engages                    +0x344C 0→1  (inPitLane)
         → maxSpeed = NormalPitKPH (m/s)            +0x40DC 0→1  (inPitLane alt)
         → pittingStatus 0→1 (or 0→2 if fast)       maxSpeed 99999→22.22

t+N.Ns   PiTStateUpdateMachine case 1→2
         → pittingStatus 1→2

t+XXs    PitTimingUpdate → PitStallProximityCheck
         car reaches stall < 1.5m
         → PitstopActionSet triggers stop sequence

t+YYs    VehicleProximityCollisionDetection=true
         → PitStateUpdateMachine case 2:
           serviceReady cleared to 0                +0x345C 1→0
           StateChangeNotify(this, 3)               +0x3450 2→3  ⚡ BOX STOP

t+YY+δs  PitStopReadyCheck runs (every frame
         while pittingStatus=3, serviceReady=0):
           velocity check: v²<0.04?
           → YES: serviceReady=1                    +0x345C 0→1  ⚡ SERVICE BEGINS
                   PitStopDecisionTireCompound
                   PitStopOperationTimer → duration
                   stopEndTime written               +0x3470 = simTime + duration
                   stopStartTime written             +0x346C = simTime
                   lollipop raised                   +0x3478 0→1
                   service flags set:
                     fuelJob                          +0x348C = 1
                     tyreChange (if tires)            +0x3490 = 1
                     wingChange (if wing)             +0x3494 = 1
                     repair (if damage)               +0x3498 = 1
                   tyreJobStart/End written           +0x349C, +0x34A0
                   refuelRampStart written            +0x3480
                   fuelAtStart written                +0x347C
                   targetFuel written                 +0x3484
                   auto-pilot byte set (player)       +0x34C8 = 1

t+YY+Ns  PitStopProgress runs every frame:
           refuel: fuel += (simTime-rampStart)*12.0   +0x3984 FuelLevel rising
           tire anim: wheel force params
           auto-pilot countdown at stopEnd-2s

t+YY+Ds  simTime >= stopEndTime
         → PitStopProgress completion branch:
           lollipop drops                              +0x3478 1→0
           StateChangeNotify(this, 4)                  +0x3450 3→4  ⚡ EXIT BOX
           measuredDuration recorded                   +0x3474 = stopEnd-stopStart
           PostPitCleanup (AI only)
           AICommandMessageQueueDispatcher(veh,15,0)
           CameraModeSetter(0)
           AudioSoundEffectCtrl (lollipop sound)
           HUD damage indicator updates
           service flags reset (stay at current val)

t+ZZs    XPITOUT polygon crossed
         → speed limiter disengages                   +0x344C 1→0
         → pittingStatus 4→0                          +0x40DC 1→0
         → stopCounter increments                     maxSpeed 22.22→99999
                                                      +0x406C += 1
```

---

## 3. Function Call Chain — Full Detail

### 3a. Per-Frame Dispatch

| Function | Address | Called By | Lines | What it does |
|---|---|---|---|---|
| `MainGameTickUpdate` | 0x42c690 | Game loop | — | Master tick |
| `VehiclePhysicsModeDispatcher` | 0x4e4930 | MainGameTickUpdate | — | Dispatches on +0x32C8 (player/AI), calls PitStateUpdateMachine |
| `PitStateUpdateMachine` | 0x4a6e50 | VehiclePhysicsModeDispatcher | 60 | State switch on pittingStatus |
| `PitTimingUpdate` | 0x4b7a90 | (separate timer) | 77 | Per-car timer, triggers stall check |
| `PitStallProximityCheck` | 0x4b5da0 | PitTimingUpdate | 31 | Car distance vs stall position |
| `PitstopActionSet` | 0x4b6940 | PitStallProximityCheck | — | Triggers stop sequence |

### 3b. Service Initialization (called ONCE per stop, first frame of state 3 with serviceReady=1)

| Function | Address | Called By | Lines | What it does |
|---|---|---|---|---|
| `PitStopReadyCheck` | 0x4a6200 | PitStateUpdateMachine | 50 | Velocity gate, calls Decision & Timer |
| `PitStopDecisionTireCompound` | 0x4a2a3d0 | PitStopReadyCheck | 132 | Chooses compound, checks retirement |
| `PitStopOperationTimer` | 0x4a6340 | PitStopReadyCheck | 276 | Computes duration, sets service flags |
| `PitStopBlockingCondition` | 0x4a2aee0 | PitStopOperationTimer | 9 | Blocks service for rear-wing retirement |

### 3c. Service Progress (called EVERY FRAME while pittingStatus=3, serviceReady=1)

| Function | Address | Called By | Lines | What it does |
|---|---|---|---|---|
| `PitStopProgress` | 0x4a68b0 | PitStateUpdateMachine | 101 | Refuels, checks stopEndTime, fires completion |
| `PostPitCleanup` | 0x4a2a6f0 | PitStopProgress (on complete) | 13 | Counter++, anti-repeat cooldown |
| `StateChangeNotify` | 0x4a5c80 | PitStateUpdateMachine / PitStopProgress | — | Transitions pittingStatus |
| `PitPartStateCopy` | 0x4a5330 | (separate init) | 102 | Copies part state to pit object |
| `PitstopStateTransition` | 0x4a5600 | PitPartStateCopy | 29 | Pit-object state change handler |
| `RepairServiceStateDispatcher` | 0x4ed250 | (damage system) | 51 | Dispatch repair by part idx (0-3=wheels, 4=FW, 5=RW) |

### 3d. Crew / Animation (session-init, NOT per-stop)

| Function | Address | Lines | Role |
|---|---|---|---|
| `PitCrewAnimationLoader` | 0x4a2a90 | 245 | Loads .MAS files, sets up per-wheel mesh swap |
| `PitCrewMeshLoader` | 0x4a2010 | 58 | Loads 3D crew meshes |
| `PitCrewAnimationMASSetup` | 0x4a3910 | 73 | Main crew animation setup from PITANIMS.MAS |
| `PitCrewAnimationStringMapper` | 0x4a24d0 | 103 | Animation name → ID (0-23) |
| `PitCrewStateMachine` | 0x4bbfc0 | 39 | Crew event handler (mask 0x680000) — **NOT used during pit stops** |
| `PitServiceSteeringAnimationStep` | 0x4b9e40 | 58 | Steering wheel anim during service |

### 3e. Animation Name → ID Map (from `PitCrewAnimationStringMapper`)

| ID | Name | ID | Name |
|---|---|---|---|
| 0 | FrontJackUp | 12 | RR_Wheel2 |
| 1 | FrontJackDown | 13 | 10secLollipopDown |
| 2 | RearJackUp | 14 | 10secLollipopUp |
| 3 | RearJackDown | 15 | FmanFleft |
| 4 | LollipopDown | 16 | Nman |
| 5 | LollipopUp | 17 | Rman |
| 6 | FuelIn | 18 | Rman2 |
| 7 | FuelOut | 19 | Sman |
| 8 | FL_Wheel | 20 | Sman2 |
| 9 | FR_Wheel | 21 | Sman3 |
| 10 | RL_Wheel | 22 | Sman4 |
| 11 | RR_Wheel | 23 | StarterRelease |

Frida probe confirmed: **none of these fire through PitCrewStateMachine during a pit stop.** The MAS animations are driven by the scene-graph animation controllers set up at session load, not by runtime event dispatch.

---

## 4. Every Offset — What, When, Can You Poke It?

### 4a. Pit Object State (base veh+0x32CC → vehicle offset shown)

| Vehicle Offset | Type | Name | Set When | Poke-able? |
|---|---|---|---|---|
| `+0x342C` | int32 | scheduleCount | Session init (.hdv) | **LATCHED** — poke holds |
| `+0x344C` | int32 | inPitLane | XPITIN polygon crossed | Read-only (scene query) |
| `+0x3450` | int32 | **pittingStatus** | StateChangeNotify calls | **LATCHED** — poke sets state, game responds |
| `+0x345C` | byte | **serviceReady** | PitStopReadyCheck velocity gate | **LATCHED** — poke=1 forces service start |
| `+0x346C` | float | stopStartTime | PitStopReadyCheck | **LATCHED** — but overwritten next stop |
| `+0x3470` | float | **stopEndTime** | PitStopReadyCheck | **LATCHED** — **poke += seconds to extend stop** |
| `+0x3474` | float | measuredDuration | PitStopProgress completion | Read-only (records history) |
| `+0x3478` | int32 | lollipopFlag | PitStopReadyCheck / PitStopProgress | **RECALCULATED** — poke useless |
| `+0x347C` | float | fuelAtStart | PitStopReadyCheck | **LATCHED** — poke to fake start fuel |
| `+0x3480` | float | refuelRampStart | PitStopReadyCheck | **LATCHED** — poke to start refuel early |
| `+0x3484` | float | targetFuel | PitStopReadyCheck (from UI) | **LATCHED** — poke to change refuel target |
| `+0x348C` | int32 | fuelJobFlag | PitStopOperationTimer | **LATCHED** — poke=1 enables refuel |
| `+0x3490` | int32 | **tyreChangeFlag** | PitStopOperationTimer | **LATCHED** — poke=1 forces tire change time |
| `+0x3494` | int32 | **wingChangeFlag** | PitStopOperationTimer | **LATCHED** — poke=1 forces wing repair time |
| `+0x3498` | int32 | repairFlag | PitStopOperationTimer | **LATCHED** — poke=1 forces repair time |
| `+0x349C` | float | tyreJobStart | PitStopReadyCheck | **LATCHED** — poke to shift tire anim |
| `+0x34A0` | float | tyreJobEnd | PitStopReadyCheck | **LATCHED** — poke to shift tire anim end |
| `+0x34C0` | float | refuelJobEnd | PitStopReadyCheck | **LATCHED** — poke to shift refuel end |
| `+0x34C8` | byte | autoPilotByte | PitStopReadyCheck | **LATCHED** — poke=1 triggers 2s launch |

### 4b. Vehicle-Level (base 0x5EC6D8 + slot*0x48C0)

| Offset | Type | Name | Role | Poke-able? |
|---|---|---|---|---|
| `+0x454C` | int32 | **callToBox** | 1=request pit | **LATCHED** — poke forces pit request |
| `+0x397C` | int32 | tireChoice | Compound ID, writes mid-stop | **LATCHED** — poke changes compound |
| `+0x458C` | int32 | nextChosenTires | Compound for next stop | **LATCHED** — poke to pre-select |
| `+0x3990` | float | fuelConsumption | L/lap, from .hdv | **LATCHED** |
| `+0x3984` | float | **FuelLevel** | Current fuel (L) | **RECALCULATED** — poke useless |
| `+0x406C` | int32 | pitStopCounter | Increments at exit | **LATCHED** — poke to fake count |
| `+0x4064` | int32 | FinishState | 0=active, 2=retired | **LATCHED** |
| `+0x4068` | int32 | ReasonCode | Retirement reason | **LATCHED** |
| `+0xD58` | int32 | RearWingIntact | 0=broken → retire at pit | **LATCHED** — poke=0 forces wing retirement |
| `+0x4538` | float | maxSpeed | Pit limiter m/s | **RECALCULATED** — reads 22.22 in pits |
| `+0x4048` | float | trackDistance | Metres from start line | **RECALCULATED** |
| `+0x403C` | int32 | lapNumber | Current lap | **LATCHED** — poke changes lap |

### 4c. Globals

| Address | Type | Name | Value | Poke-able? |
|---|---|---|---|---|
| `0x59E96C` | float | RefuelFlowRate | 12.0 L/s | **Byte-patch only** |
| `0x59E960` | float | TireChangeTime | 6.5s | **Byte-patch only** |
| `0x59E4DC` | float | WingRepairTime | 8.0s | **DO NOT PATCH** (8 readers) |
| `0x5ACB54` | float | RearWingSetTime | 39.0s | **Byte-patch only** |
| `0x5ACB5C` | float | PenaltyCap | 4.5s | **Byte-patch only** |
| `0x8AAE74` | float[256] | NoiseTable | Random ~0-1 | **Byte-patch only** |
| `0x5EA3EC` | byte | LeftHandPitFlag | 0=R, 1=L | **LATCHED** — poke to flip crew side |

---

## 5. Poke Cheat Sheet — What You Can Do Without Writing Code

| Goal | Action |
|---|---|
| **Extend current stop** | `*(float*)(veh+0x3470) += N` |
| **Force service to start immediately** | `*(byte*)(veh+0x345C) = 1` |
| **Skip velocity gate** | Set +0x345C=1 while in state 3 |
| **Force AI to pit** | `*(int32*)(veh+0x454C) = 1` |
| **Force retirement at next stop** | `*(int32*)(veh+0xD58) = 0` then request pit |
| **Force tire change (even if not needed)** | `*(int32*)(veh+0x3490) = 1` |
| **Force wing repair time** | `*(int32*)(veh+0x3494) = 1` |
| **Change refuel target mid-stop** | `*(float*)(veh+0x3484) = newTarget` |
| **Read pit state** | `*(int32*)(veh+0x3450)` → 0/1/2/3/4 |
| **Read stop time remaining** | `*(float*)(veh+0x3470) - sessionTime` |
| **Count pit stops this session** | `*(int32*)(veh+0x406C)` |
| **Detect "in pit lane"** | `*(int32*)(veh+0x344C) != 0` |
| **Detect "service running"** | `pittingStatus==3 && serviceReady==1` |
| **Detect "about to exit box"** | `pittingStatus==3 && stopEndTime - simTime < 2.0` |
| **Change pit speed limit** | Edit `.gdb` key `NormalPitKPH` |
| **Change refuel rate** | Byte-patch `0x59E96C` (12.0→X) AND `0x5ACB58` (0.08333→1/X) |
| **Change tire swap time** | Byte-patch `0x59E960` (6.5→X) |

---

## 6. What CANNOT Be Poked (needs code hook)

| What | Why |
|---|---|
| **Fuel consumption rate** | Computed from vehicle physics integration |
| **AI pit strategy schedule** | Set once at session init from .hdv |
| **Pit crew animations** | Driven by MAS scene-graph controllers, not state flags |
| **Tire wear (continuous)** | Recalculated every physics tick |
| **Speed limiter engage/disengage** | Triggered by scene polygon queries |
| **Pit stall assignment** | Set at session init, not per-vehicle |
| **Pit failure events (random)** | No emitter exists — 21 clips need DLL-side emitters |
| **Pit-in/pit-out commentary** | No live emitter for player — needs DLL (PIT10/PIT12 done in `pit_exit_commentary.dll`) |

---

## 7. Service Duration Formula

```
duration = MAX(
    refuel: (targetFuel - currentFuel) / 12.0 + noise*2 + penalty + 1.5,
    tires:  noise*2 + penalty + 6.5,
    wing:   penalty + 8.0,
    repair: penalty + 8.5 + noise
)

penalty = CLAMP((fuelAtStopStart + 25 - simTime) * 0.5, 0, 4.5)
noise   = flt_8AAE74[index++]

If rear wing set (broken): +39.0 instead of +8.0
```

---

## 8. Pit Menu System — String Buffers vs Strategy Storage

The pit menu (ENTER key) displays 5 rows: STOP/GO, FUEL, TYRES, WING, DAMAGE.
It is rendered by `sub_49F1C0` (called from `DrawYellowTextAtFixedScreenPosition`).

### 8a. Display Strings (DISPLAY-ONLY, overwritten by game)

Each row has a 31-byte ASCII string buffer at `pitObject + 54 + row*64`:

| Row | Label | Vehicle Offset | String Buffer |
|---|---|---|---|
| 0 | STOP/GO | `+0x3302` | `"N/A"` or status text |
| 1 | FUEL | `+0x3342` | `"88/24"` (targetL / something) |
| 2 | TYRES | `+0x3382` | `"Intermediate"` or compound name |
| 3 | WING | `+0x33C2` | `"42"` or status code |
| 4 | DAMAGE | `+0x3402` | `"N/A"` or status text |

**IMPORTANT**: Writing to these buffers has NO effect on the pit service.
The game syncs them FROM the strategy storage. Any tamper is overwritten
when the pit menu initializes or the player cycles values.

### 8b. Strategy Storage (ACTUAL values, POKE THESE)

The real numeric values that `PitStopDecisionTireCompound` reads at service
start are stored at fixed offsets in the pit object:

| Vehicle Offset | Pit Object Offset | Type | Name | Role |
|---|---|---|---|---|
| `+0x3338` | `+0x6C` | int32 | **fuelTarget** | Target fuel for next stop (L) |
| `+0x3334` | `+0x68` | int32 | fuelCapacity? | Max fuel (150 on Ferrari) |
| `+0x3378` | `+0xAC` | int32 | **nextChosenTires** | Tire compound index for next stop |
| `+0x3484` | `+0x1B8` | float | targetFuel | WRITTEN at service start from fuelTarget |

### 8c. Live Tamper Proof (2026-07-29)

```
POKED: +0x3338 = 120  (was 87)
POKED: +0x3378 = 5    (was 3)

RESULT: Service used targetFuel=121L (tamper CONFIRMED)
        tyreChoice=4 (compound 5 invalid, game clamped to 4)
        fuel went 87→121 (+34L at 12 L/s = ~2.8s refuel)
```

Fuel target tamper: **CONFIRMED WORKING**. Tire compound: game validates
index against track's .tbc file; 5 was out of range for this track.

### 8d. Pit Menu Row Selection

The selected row index is at `veh+0x32D0` (pitObject[1]). The INC/DEC
wraps on rows where `pitObject + 40 + row*64 ≤ 1` (skipped rows).

---

## 9. Animation Tracking — What CAN and CANNOT Be Detected

### 9a. Detectable from Vehicle Struct (lollipop, service flags)

| Event | Signal | Offset | Trust |
|---|---|---|---|
| **Service START** | serviceReady 0→1 | `+0x345C` | CONFIRMED |
| **Lollipop UP** (jack raised) | lollipop 0→1 | `+0x3478` | CONFIRMED |
| **Fuel hose connected** | fuelJob 0→1 | `+0x348C` | CONFIRMED |
| **Tire guns active** | tyreChg 0→1 | `+0x3490` | CONFIRMED |
| **Wing change active** | wingChg 0→1 | `+0x3494` | CONFIRMED |
| **Repair active** | repair 0→1 | `+0x3498` | CONFIRMED |
| **Refuel flowing** | FuelLevel rising >0.03/frame | `+0x3984` | CONFIRMED |
| **Lollipop DOWN** (release!) | lollipop 1→0 | `+0x3478` | CONFIRMED |
| **Service COMPLETE** | pittingStatus 3→4 | `+0x3450` | CONFIRMED |
| **Tire swap DONE** | service complete + tyreChg was 1 | `+0x3490` | CONFIRMED |
| **Wing repair DONE** | service complete + wingChg was 1 | `+0x3494` | CONFIRMED |
| **Repair DONE** | service complete + repair was 1 | `+0x3498` | CONFIRMED |
| **Front wing fixed** | fwIntact changes | `+0xD14` | CONFIRMED |
| **Rear wing fixed** | rwIntact changes | `+0xD58` | CONFIRMED |
| **Pit lane entry** | inPitLane 0→1, maxSpeed→limit | `+0x344C`, `+0x4538` | CONFIRMED |
| **Pit lane exit** | inPitLane 1→0, maxSpeed→∞ | `+0x344C`, `+0x4538` | CONFIRMED |
| **Menu row selected** | pitObject[1] changes | `+0x32D0` | CONFIRMED |
| **Menu value changed** | string buffer at row offset changes | `+0x3302+row*64` | CONFIRMED |
| **Pit confirmed** | callToBox 0→1 | `+0x454C` | CONFIRMED |

### 9b. NOT Detectable (3D Scene Graph Only)

These animations are MAS scene-graph controllers set up at session load by
`PitCrewAnimationLoader` / `PitCrewAnimationMASSetup`. They tick on the
GPU/scene timeline, not in the vehicle struct:

- Individual wheel-off/wheel-on mesh swaps (per corner)
- Front/rear jack up/down movement
- Fuel hose insertion/removal animation
- Crew member walk-in/walk-out
- Tire gun spin animation
- Steering wheel lock animation during service
- The exact frame tire meshes are swapped

The Frida probe on `PitCrewStateMachine` (0x4bbfc0) confirmed: **zero
animation events fire through that function during a pit stop.** The
crew state machine handles session-init events only.

### 9c. What "Tire Change Complete" Means Mechanically

From the game's perspective, tires are "changed" at the **service complete
tick** (pittingStatus 3→4). At this moment:
1. `PitStopProgress` detects `simTime >= stopEndTime`
2. `StateChangeNotify(this, 4)` fires
3. Service flags persist (tyreChg, fuelJob etc. stay at 1)
4. `measuredDuration` records actual elapsed time
5. The car is drivable

The tire mesh swap in the 3D scene happens BEFORE this tick (during the
service window). The exact timing of mesh swaps is in the MAS animation
timeline, which we cannot read from memory.

---

## 10. Live Monitor Scripts

| Script | Purpose |
|---|---|
| `tools/watch_pitstop_log.ps1` | Real-time event logger, 40ms poll, prints on any value change |
| `tools/watch_pitstop.ps1` | Live dashboard table |
| `tools/trace_pitstop_enhanced.py` | Full-struct diff trace for offline analysis |
| `tools/probe_pitcrew_anim.py` | Frida hook for crew animation events (confirmed: none fire) |
| `tools/poke_quick.py` | Quick state inspection |

### Monitor Usage

```powershell
# Launch event logger (recommended):
.\tools\watch_pitstop_log.ps1
```

Logged events include: PIT-ENTRY, BOX-STOP, SERVICE START (with per-job
breakdown), LOLLIPOP-UP/DOWN, REFUEL-FLOW, SERVICE COMPLETE (per-job),
FWING-FIXED, RWING-FIXED, PIT-EXIT, PIT-MENU value changes, PIT-CONFIRM,
DRIVER-STATUS, CONTROL-MODE changes.
