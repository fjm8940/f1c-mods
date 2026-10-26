# Pit Stop — Complete Technical Reference

Current live v3 probability, player INI and package audit:
[2026-09-23 release preparation](../verification/ship-preparation-2026-09-23/REPORT.md).
The historical technical reference below predates the r31 candidate.

> All static analysis + live trace + tamper findings. 2026-07-29.

## 1. Function Call Chain

```
MainGameTickUpdate (0x42c690)
  └→ VehiclePhysicsModeDispatcher (0x4e4930)  [EVERY FRAME]
       └→ PitStateUpdateMachine (0x4a6e50)      [ditto, veh+0x32CC]
            ├─ case 2: VehicleProximityCollisionDetection → StateChangeNotify(3)
            ├─ case 3: if(serviceReady) PitStopProgress else PitStopReadyCheck
            └─ sub_4A5F20: AI commentary, red-flag penalty

PIT TIMING (separate):
  PitTimingUpdate (0x4b7a90) → PitStallProximityCheck → PitstopActionSet

SERVICE:
  PitStopReadyCheck (0x4a6200) → PitStopDecisionTireCompound (0x42a3d0)
                                → PitStopOperationTimer (0x4a6340)
                                    → PitStopBlockingCondition (0x42aee0)
                                    → sub_4A35F0 → RangeProportionInitializer (MAS)

  PitStopProgress (0x4a68b0) → PostPitCleanup (0x42a6f0)
                              → StateChangeNotify (0x4a5c80)
```

## 2. pittingStatus Enum

| Value | Meaning | Paired Flags |
|---|---|---|
| 0 | On track | |
| 1 | Pit requested | callToBox=1 |
| 2 | Pit lane | inPitLane=1, maxSpeed=22.22 |
| 3 | In pit box | serviceReady=1 (after velocity < 0.04) |
| 4 | Exiting box | |
| 0 | Exit line crossed | inPitLane=0, stopCounter++, maxSpeed=99999 |

Can skip state 1 (0→2 directly) if pit entry is fast enough.

## 3. Full Offset Reference

### Pit Object (base veh+0x32CC)

| Veh Offset | PitObj Off | Type | Name | Notes |
|---|---|---|---|---|
| +0x32D0 | +0x04 | int32 | menuRow | Selected pit menu row (0-4) |
| +0x3334 | +0x68 | int32 | fuelCapacity? | Car max fuel (150 on Ferrari) |
| +0x3338 | +0x6C | int32 | **fuelTarget** | **Storage for next stop fuel (POKE THIS)** |
| +0x3378 | +0xAC | int32 | **nextChosenTires** | **Storage for next tire compound (POKE THIS)** |
| +0x344C | +0x180 | int32 | inPitLane | 1=XPITIN crossed |
| +0x3450 | +0x184 | int32 | **pittingStatus** | 0-4 enum |
| +0x345C | +0x190 | byte | serviceReady | 1=velocity below 0.04 threshold |
| +0x346C | +0x1A0 | float | stopStartTime | Sim-time service began |
| +0x3470 | +0x1A4 | float | **stopEndTime** | **Box lock deadline (LATCHED, POKE THIS)** |
| +0x3474 | +0x1A8 | float | measuredDuration | Actually elapsed |
| +0x3478 | +0x1AC | int32 | lollipop | 1=up, 0=down (RECALCULATED) |
| +0x347C | +0x1B0 | float | fuelAtStart | Fuel when service began |
| +0x3480 | +0x1B4 | float | refuelRampStart | When fuel actually begins flowing |
| +0x3484 | +0x1B8 | float | targetFuel | Committed at service start |
| +0x348C | +0x1C0 | int32 | fuelJob | 1=refuel queued/active |
| +0x3490 | +0x1C4 | int32 | tyreChg | 1=tire change queued/active |
| +0x3494 | +0x1C8 | int32 | wingChg | 1=wing change queued/active |
| +0x3498 | +0x1CC | int32 | repair | 1=body repair queued/active |
| +0x349C | +0x1D0 | float | tyreJobStart | Tire anim start |
| +0x34A0 | +0x1D4 | float | tyreJobEnd | Tire anim end |
| +0x34C0 | +0x1F4 | float | refuelJobEnd | Refuel job end time |
| +0x34C8 | +0x1FC | byte | autoPilotByte | 1=2s launch countdown |

### Vehicle Flags

| Offset | Type | Name | Notes |
|---|---|---|---|
| +0x00C4 | int32 | isHumanInControl | 1=player, 0=AI/auto |
| +0x0D14 | int32 | fwIntact | Front wing attached |
| +0x0D58 | int32 | rwIntact | Rear wing attached (0=retire) |
| +0x397C | int32 | tireChoice | Current compound index |
| +0x3984 | float | FuelLevel | Current fuel L (RECALCULATED) |
| +0x39FC | int32 | CurrentGear | Gear selection |
| +0x403C | int32 | lapNumber | Current lap |
| +0x406C | int32 | pitStopCounter | Increments at exit |
| +0x40DC | int32 | inPitLaneAlt | Alternative in-pit-lane flag |
| +0x4510 | int32 | driverStatus | 5=grid/moving, 0=garage |
| +0x4538 | float | maxSpeed | 22.22=pits, 99999=track |
| +0x454C | int32 | callToBox | 1=pit confirmed |
| +0x458C | int32 | nextChosenTires | Compound for next stop |

### Pit Menu Readouts (string buffers, 31 bytes each)

| Offset | Row | Format |
|---|---|---|
| +0x3302 | STOP/GO | "N/A" |
| +0x3342 | FUEL | "88/24" (targetLaps/currentLaps?) |
| +0x3382 | TYRES | "Intermediate" |
| +0x33C2 | WING | "42" |
| +0x3402 | DAMAGE | "N/A" |

These are DISPLAY ONLY. Writing to them has no effect on service.
Sync from strategy storage (+0x3338, +0x3378).

### Globals

| Address | Type | Name | Value |
|---|---|---|---|
| 0x008AFEA0 | double | SessionTime | Current sim time |
| 0x007C54E8 | ptr | PlayerVehiclePtr | Vehicle struct for player |
| 0x005EC6D8 | ptr | VehicleArray | Base of vehicle array |
| 0x59E96C | float | RefuelFlowRate | 12.0 L/s |
| 0x59E960 | float | TireChangeTime | 6.5s |
| 0x59E4DC | float | WingRepairTime | 8.0s (DO NOT PATCH: 8 readers) |
| 0x5ACB54 | float | RearWingSetTime | 39.0s |
| 0x5EA3EC | byte | LeftHandPitFlag | 0=R, 1=L |
| 0x008AAE74 | float[256] | NoiseTable | ~0-1 random values |
| 0x008ACEE0 | ptr | StewardObject | Steward system base |

## 4. Animation System

### Crew Actors (from PitCrewAnimationLoader)

Each crew actor is 120 bytes. The crew object has:
- `+0x0C` = actor count
- `+0x10` = actor array pointer

Actor struct:
- `+0x03` = lollipop flag (1 if LMAN/LOLLIPOP)
- **`+100` (0x64) = ASCII name pointer — the actor names itself. 34/34 actors.**
- `+76` (0x4C) = lateral offset (negative = left, positive = right)
- `+84` (0x54) = longitudinal offset (front ≈ −0.3, rear ≈ −3.35 → 3.08 m wheelbase)
- `+112` / `+116` = channel count / channel array (stride 28; `+12`/`+16` = baseStart/baseEnd)

Dump every actor read-only, no Frida and no pit stop: `python tools/dump_crew_struct.py`

**Roster (FACT, live 2026-07-29):** 0 LMAN, 1 LOLLIPOP, 2 JACK_FRONT, 3 TMAN,
4 JACK_REAR, 5 RMAN, 6-11 front **left** (FMAN/GMAN/NMAN/WHEELGUN/WHEELOFF/WHEELON),
12-17 front **right**, 18-23 rear **left**, 24-29 rear **right**, 30 FUEL_RIG
(static, 0 channels), 31 ZMAN, 32 HOSE, 33 NOZZLE.

Roles — each man shares an identical animation window with the object he handles:
GMAN↔WHEELGUN, NMAN↔WHEELON, FMAN↔WHEELOFF, TMAN↔JACK_FRONT, RMAN↔JACK_REAR,
LMAN↔LOLLIPOP, ZMAN↔HOSE+NOZZLE. That shared window is why timing-signature
matching cannot separate them.

`baseStart` groups: 2.3 = front left, 2.5 = front right, 1.0 = both rears.

### Animation Names (from PitCrewAnimationStringMapper, 0x4a24d0)

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

**None of these fire through PitCrewStateMachine** (Frida probe confirmed).
MAS scene-graph controllers drive them independently.

### MAS Files

Located at `SeasonData\Circuits\Pits\{Normal|Left|10Sec}\PITANIMS.MAS`.
CUBEMAS4.10 format, 31-52 .ANM files inside. Binary keyframe+vertex data.

Per-corner animation files decoded:
- NWFL = New Wheel Front Left (279KB)
- NWRL/NWRR/NWRRB = New Wheel Rear L/R (189KB/89KB/81KB)
- WLOF/WLOFC/WLOFD = Wheel Off (5KB/4KB/4KB)
- WLON/WLONC/WLONG = Wheel On (7KB/5KB/3KB)
- WGUN/WGUND = Wheel Gun (9KB/9KB)
- FJAK = Front Jack (120KB)
- RJAK = Rear Jack (7KB)
- NOZL = Fuel Nozzle (4KB)
- HOSE = Fuel Hose (47KB)
- LLMZ = Lollipop Man Zone (137KB)
- LLPOP = Lollipop (4KB)
- MSTR = Master timeline (197KB)

### Animation Trigger Chain

```
PitStopOperationTimer (0x4a6340)
  → sub_4A35F0 (0x4a35f0)
    → RangeProportionInitializer (0x4a1fd0)
      → this[0]=start, this[1]=end (=stopEndTime), this[2]=1/(end-start)
```

The MAS reads the animation range ONCE at service start from
RangeProportionInitializer. After that, the animation controller
auto-advances independently. Poking stopEndTime after service start
does NOT affect animation playback — only the box lock.

## 5. Service Duration

```
duration = MAX(
    refuel: (targetFuel - currentFuel) / 12.0 + noise*2 + penalty + 1.5,
    tires:  noise*2 + penalty + 6.5,          (max of 4 corners)
    wing:   penalty + 8.0,
    repair: penalty + 8.5 + noise
)
penalty = CLAMP((fuelAtStopStart + 25 - simTime) * 0.5, 0, 4.5)
noise   = flt_8AAE74[index++]

If rear wing broken (+0xD58==0): +39.0 instead of +8.0 for wing
Refuel flow: 12.0 L/s (flt_59E96C)
```

## 6. Live Tamper Proofs (2026-07-29)

| Test | Action | Result |
|---|---|---|
| Fuel target poke | Set +0x3338 = 120 | Service used 121L ✓ |
| Tire compound poke | Set +0x3378 = 5 | Game clamped to 4 |
| Freeze pit stop | Push +0x3470 += 5 continuously | Box locked forever ✓ |
| Release pit stop | Set +0x3470 = simTime+2 | Box opens in 2s ✓ |
| MAS file tamper | Zero NWRL (189KB) in PITANIMS.MAS | Game CRASHED on load |
| Menu string tamper | Write "120" to +0x3342 | Overwritten by game, no effect ✗ |
| Frida crew hook | Hook PitCrewStateMachine | ZERO events during pit stops |

## 7. Pit Menu Interaction

```
Player presses ENTER → pit menu overlay (NOT tracked in vehicle struct)
Player adjusts sliders via INC/DEC → writes to string buffers (+0x3302..+0x3402)
Player confirms → callToBox = 1 (+0x454C)
Service starts → PitStopDecisionTireCompound reads strategy from +0x3338/+0x3378
```

The menu string buffers are synced FROM strategy storage. The actual
numeric values live at +0x3338 (fuel) and +0x3378 (tires).

## 8. Tools

| Tool | Path | Purpose |
|---|---|---|
| Crew tracker | EVF tools/watch_pitstop_crew.ps1 | Crew-by-crew animation log |
| Event logger | EVF tools/watch_pitstop_log.ps1 | Timestamped flag changes |
| Live dashboard | EVF tools/watch_pitstop.ps1 | Real-time value display |
| Enhanced trace | EVF tools/trace_pitstop_enhanced.py | Full struct diff to file |
| Freeze script | EVF tools/freeze_pitstop.py | Push stopEndTime, lock box |
| Frida freeze | EVF tools/freeze_pitstop_frida.py | Hook RangeProp, freeze anims |
| Quick inspect | EVF tools/poke_quick.py | Read current state |
| Tire inspector | EVF tools/poke_tires.py | Read tire/damage values |

Run from `mods/extended_vanilla_features/` directory.

## 9. Frida Hooking — Pit Crew Animation Control

### 9a. Critical Gotcha: console.log vs send()

`console.log()` in Frida JavaScript does NOT reach the Python message handler.
Must use `send()`:
```js
// WRONG:
console.log("hello");     // output goes to game process stdout, not Python

// CORRECT:
send({msg: "hello"});     // reaches Python on_msg handler
```

### 9b. 32-bit Stack Arguments

On 32-bit Windows `__thiscall`, Frida's `args[0]` maps to the first stack
argument. For reading float arguments from the stack reliably, use 
`this.context.esp`:
```js
// Read float args from stack ESP+4 and ESP+8:
var start = this.context.esp.add(4).readFloat();
var end   = this.context.esp.add(8).readFloat();
```

### 9c. Per-Actor Animation Control — PROVEN WORKING

**Complete chain to identify and freeze individual crew members:**

```
1. Frida hook on RangeProportionInitializer (0x4a1fd0)
2. Read float args from ESP (start, end times)
3. Read crew object at veh+0x32D4 (pitObject+8)
4. Walk actor array: crew+0x0C=count, crew+0x10=actors ptr
5. Each actor: 120 bytes, +3=byte flag, +112=channel count, +116=channel array
6. Compare current channel pointer (ECX) against each actor's channel array range
7. If channel ∈ [array, array+count*28): this channel belongs to that actor
8. Selectively write inflated end time to ESP+8 for target actors only
```

**Actor Index Map (34 actors, live-verified):**

| Index | flag3 | Crew | Verified |
|---|---|---|---|
| 0-1 | 1 | **LOLLIPOP** | ✅ Frozen: lollipop stops, all others play |
| 2-3 | 0 | Front hoist/jack | Not yet tested |
| 4-5 | 0 | Front hoist/jack | Not yet tested |
| 6-11 | 0 | **FRONT TIRE CREWS** | ✅ Frozen: all play except front tires |
| 12-17 | 0 | Rear tire crews? | PARTIAL — channel check may differ |
| 18-29 | 0 | Wing/repair mechanics | Not yet tested |
| 30-33 | 0 | Cleanup/late crew | Not yet tested |

**Per-actor channel timing (from crew dump):**

| Actor group | ch count | baseStart | baseEnd | Interpretation |
|---|---|---|---|---|
| 0-1 (lollipop) | 2 | 5.0 / 0.0 | 5.0 / 7.9s | Dummy ch + main animation |
| 2-3 (hoist) | 2 | 2.0 / 0.0 | 4.3 / 3.1s | Main + sub animation |
| 4-5 (hoist) | 2 | 1.7 / 0.0 | 10.1 / 3.8s | Master controller? |
| 6-11 (front tires) | 1-2 | 2.3 / 0.0 | 6.7-10.5 / 3.1s | OFF ch at 0.0s, ON ch at 2.3s |
| 12-17 (rear tires) | 1-2 | 2.5 / 0.0 | 6.9-10.5 / 3.1s | OFF ch at 0.0s, ON ch at 2.5s |
| 18-29 (mechanics) | 1-2 | 1.0 / 0.0 | 3.4-10.4 / 2.9s | Wing/repair work |
| 30-33 (cleanup) | 2 | 2.3 / 0.0 | 4.5 / 4.7s | Late-phase work |

Each tire actor has TWO channels: OFF (baseStart 0.0s = wheel removal) and 
ON (baseStart > 2.0s = new wheel attached). This explains why freezing by
baseStart alone produced "only ON played" results — the OFF channel at 0.0s
was misidentified.

### 9d. Freeze Condition Reference

In `freeze_lollipop.py`, change the condition at line ~55:

| Target | Condition |
|---|---|
| Lollipop only | `owner==0` or `owner==1` or `isLolli` |
| Front hoist/jack | `owner>=2 && owner<=5` |
| Front tire crews | `owner>=6 && owner<=11` |
| Rear tire crews | `owner>=12 && owner<=17` |
| All mechanics | `owner>=18` |
| ALL animations | `true` |
| NONE (normal play) | `false` |

### 9e. Proven Working Scripts

| Script | Purpose | Status |
|---|---|---|
| `tools/dump_crew_struct.py` | Read-only actor roster from `actor+100` name strings | **PROVEN** — 34/34 named, no Frida, no pit stop |
| `tools/freeze_crew_byptr.py` | Per-actor freeze by exact channel pointer | **PROVEN** — 34 actors / 44 channels, 0 unmapped |
| `tools/freeze_lollipop.py` | Per-actor freeze by timing signature | **DEPRECATED** — signatures collide; its corner labels are inverted. Use `freeze_crew_byptr.py` |
| `tools/dump_crew_actors.py` | Python dump of crew actor array during service | **PROVEN** — 34 actors with timing |
| `tools/_test_frida2.py` | Frida heartbeat test with `send()` | **PROVEN** — confirms Frida attach works |

### 9f. Known Limitations

- `channel.compare()` works for identifying actor ownership but may miss
  channels at the exact boundary (front-right ON channel was missed when
  range was 6-11)
- Crew must be re-read periodically (not just once) since channel arrays
  may be allocated after first RangeProp call
- `send()` flooding can occur if RangeProp fires thousands of times per
  second from physics system — throttle with counters

## 10. MAS File Modding

Located at `SeasonData\Circuits\Pits\{Normal|Left|10Sec}\PITANIMS.MAS`.
CUBEMAS4.10 format, 31-52 files. Extracted via `mas_dump/`.

**Attempted**: Zeroing NWRL (189KB rear left wheel animation) — **game CRASHED**
on load. The ANM format is binary keyframe+vertex data; zeroing breaks structure.

**Practical**: Hex-edit float timing values within files, or swap same-sized files.
Full reverse-engineering of .ANM format needed for reliable modding.

Animation file naming decoded:
```
NWFL = New Wheel Front Left   WLOF = Wheel Off   WLON = Wheel On  
NWRL = New Wheel Rear Left    WGUN = Wheel Gun    HOSE = Fuel Hose
NWRR = New Wheel Rear Right   FJAK = Front Jack   RJAK = Rear Jack
NOZL = Fuel Nozzle            LLMZ = Lollipop     MSTR = Master
```
