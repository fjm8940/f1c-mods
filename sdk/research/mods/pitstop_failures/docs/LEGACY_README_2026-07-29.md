# Pit Stop Modding — Complete Reference

> Everything discovered about pit stops: static analysis, live traces, memory offsets,
> animation triggers, tamper techniques, and live tools.
> 2026-07-29

## Quick Start

```powershell
# Who is every pit crew member? (read-only, no Frida, no pit stop needed)
python tools\dump_crew_struct.py

# Freeze individual crew members by actor index (exact, fail-safe box hold):
python tools\freeze_crew_byptr.py 6 7 8

# Launch the crew animation tracker:
.\tools\watch_pitstop_crew.ps1

# Live dashboard:
.\tools\watch_pitstop.ps1
```

**Crew roster (FACT, 2026-07-29):** 34 actors, each naming itself at `actor+100`.
0 LMAN, 1 LOLLIPOP, 2 JACK_FRONT, 3 TMAN, 4 JACK_REAR, 5 RMAN,
**6-11 front left**, **12-17 front right**, **18-23 rear left**, **24-29 rear right**
(each corner = FMAN, GMAN, NMAN, WHEELGUN, WHEELOFF, WHEELON),
30 FUEL_RIG (static), 31 ZMAN, 32 HOSE, 33 NOZZLE.
Full roster + roles: `docs/HANDOFF_ANIMATION_MAPPING.md`.

---

## Documents

| File | Content |
|---|---|
| `DOCS.md` | Full technical reference (static code, function chain, all offsets) |
| `docs/PIT_STOP_FIRING_ORDER.md` (EVF) | Trigger chain, tamper cheat sheet, animation tracking |
| `docs/PIT_STOP_COMPREHENSIVE_ANALYSIS.md` (EVF) | 18-section static analysis, every function + offset |
| `docs/pitstop_enhanced_trace.txt` (EVF) | 642KB raw trace, 5+ live stops |
| `docs/HANDOFF_2026-07-28.md` (EVF) | Session context + prior art |

---

## Tools (all in EVF `tools/`)

| Tool | Purpose |
|---|---|
| `watch_pitstop_crew.ps1` | **Crew-by-crew animation tracker** (lollipop, tires, fuel, wing, repair) |
| `watch_pitstop_log.ps1` | **Event logger** — prints every flag change with timestamps |
| `watch_pitstop.ps1` | **Live dashboard** — table view of all pit values |
| `trace_pitstop_enhanced.py` | **Full-struct diff trace** — 40ms poll, 50+ fields, saves to docs/ |
| `freeze_pitstop.py` | **Box freezer** — pushes stopEndTime forward, car locked in box |
| `freeze_pitstop_frida.py` | **Frida freezer** — hooks RangeProp to freeze animations + box |
| `probe_pitcrew_anim.py` | **Frida animation probe** — confirmed zero PitCrewStateMachine events |
| `poke_quick.py` / `poke_tires.py` | Quick state inspectors |

### Frida Notes

- **MUST use `send()` not `console.log()`** — `console.log` doesn't reach Python handler
- 32-bit stack args: use `this.context.esp.add(N).readFloat()` not `args[N].readFloat()`
- `PitStopProgress` (0x4a68b0) onLeave hooks work and fire every frame in box
- `RangeProportionInitializer` (0x4a1fd0) is the per-animation range setter — needs ESP arg reading

---

## Proven Tamper Points

### Memory Pokes (real-time, no restart)

| Goal | Expression |
|---|---|
| Force AI to pit | `*(int32*)(veh+0x454C) = 1` |
| Change refuel target | `*(int32*)(veh+0x3338) = liters` |
| Change tire compound | `*(int32*)(veh+0x3378) = compound_index` |
| **Freeze pit stop** | Push `*(float*)(veh+0x3470) += N` while pittingStatus==3 |
| **Release pit stop** | Set `*(float*)(veh+0x3470) = sessionTime + 2.0` |
| Force tire change | `*(int32*)(veh+0x3490) = 1` |
| Force wing repair | `*(int32*)(veh+0x3494) = 1` |
| Force body repair | `*(int32*)(veh+0x3498) = 1` |
| Read pit state | `*(int32*)(veh+0x3450)` → 0=track, 1=req, 2=lane, 3=box, 4=exit |
| Detect service running | pittingStatus==3 AND serviceReady==1 |
| Detect box exit | pittingStatus 3→4 |

### Byte Patches (requires restart)

| What | VA | Current |
|---|---|---|
| Refuel flow rate | `0x59E96C` | 12.0 L/s |
| Tire change time | `0x59E960` | 6.5s |
| Rear wing set time | `0x5ACB54` | 39.0s |

### MAS File Modding (requires restart)

| File | Location |
|---|---|
| `PITANIMS.MAS` (normal pits) | `SeasonData\Circuits\Pits\Normal\` |
| `PITANIMS.MAS` (left-handed) | `SeasonData\Circuits\Pits\Left\` |
| `PITANIMS.MAS` (10 second) | `SeasonData\Circuits\Pits\10Sec\` |

52 .ANM files inside: wheel-off (WLOF), wheel-on (WLON), wheel-gun (WGUN),
front jack (FJAK), rear jack (RJAK), fuel nozzle (NOZL), fuel hose (HOSE),
lollipop (LLMZ/LLPOP), master timeline (MSTR).

---

## Key Offset Map

### Vehicle Struct (base 0x5EC6D8 + slot*0x48C0)

| Offset | Type | Name | Role |
|---|---|---|---|
| `+0x3338` | int32 | fuelTarget | Target fuel for next stop (L) — **POKE THIS** |
| `+0x3378` | int32 | nextChosenTires | Compound index — **POKE THIS** |
| `+0x344C` | int32 | inPitLane | 1=entered XPITIN polygon |
| `+0x3450` | int32 | pittingStatus | 0=track, 1=req, 2=lane, 3=box, 4=exit |
| `+0x345C` | byte | serviceReady | 1=car stopped, service queued |
| `+0x346C` | float | stopStartTime | Sim-time when service began |
| `+0x3470` | float | stopEndTime | **CONTROLS BOX LOCK** — sim-time deadline |
| `+0x3474` | float | measuredDuration | Actual elapsed service time |
| `+0x3478` | int32 | lollipop | 1=up/jacked, 0=down/released — RECALCULATED |
| `+0x347C` | float | fuelAtStart | Fuel level when service began |
| `+0x3480` | float | refuelRampStart | Sim-time when refuel actually starts |
| `+0x3484` | float | targetFuel | Fuel target (committed at service start) |
| `+0x348C` | int32 | fuelJob | 1=refuel queued/active |
| `+0x3490` | int32 | tyreChg | 1=tire change queued/active |
| `+0x3494` | int32 | wingChg | 1=wing change queued/active |
| `+0x3498` | int32 | repair | 1=repair queued/active |
| `+0x349C` | float | tyreJobStart | Tire animation start time |
| `+0x34A0` | float | tyreJobEnd | Tire animation end time |
| `+0x397C` | int32 | tireChoice | Current compound (writes mid-stop) |
| `+0x3984` | float | FuelLevel | Current fuel (L) — RECALCULATED |
| `+0x406C` | int32 | pitStopCounter | Increments at each pit exit |
| `+0x454C` | int32 | callToBox | 1=pit confirmed, driving to box |
| `+0x4538` | float | maxSpeed | 22.2 in pit lane, 99999 on track |

### Pit Menu String Buffers (DISPLAY ONLY)

| Offset | Row | Format Example |
|---|---|---|
| `+0x3302` | STOP/GO | `"N/A"` |
| `+0x3342` | FUEL | `"88/24"` |
| `+0x3382` | TYRES | `"Intermediate"` |
| `+0x33C2` | WING | `"42"` |
| `+0x3402` | DAMAGE | `"N/A"` |

---

## Service Duration Formula

```
duration = MAX(
    refuel: (targetFuel - currentFuel) / 12.0 + noise*2 + penalty + 1.5,
    tires:  noise*2 + penalty + 6.5,
    wing:   penalty + 8.0,
    repair: penalty + 8.5 + noise
)
penalty = CLAMP((fuelAtStart + 25 - simTime) * 0.5, 0, 4.5)
noise   = flt_8AAE74[index++]

If rear wing broken: +39.0 instead of +8.0
Refuel flow: 12.0 L/s (constant at 0x59E96C)
```

---

## Animation Timing (from PitStopProgress)

| Event | Time | Signal |
|---|---|---|
| Service start | stopStart | all flags set, lollipop=1 |
| Front tire guns | tyreJobStart + 0.2s | wheel force clamp front |
| Rear tire guns | tyreJobStart + 1.4s | wheel force clamp rear |
| Refuel flowing | rampStart (stopStart + ~6s) | fuel level rising |
| Lollipop drops | stopEnd - 2s | lollipop=0 |
| Box opens | stopEnd | pittingStatus 3→4 |

### What CANNOT Be Tracked From Memory

`PitCrewStateMachine` receives ZERO events during pit stops (Frida-confirmed), and
the 3D mesh swaps are MAS scene-graph controllers that tick independently — so
none of it shows up in the vehicle struct.

**Superseded in part (2026-07-29):** the crew IS reachable, just not through the
vehicle struct. `veh+0x32D4` → crew object → 34 named actors, each with animation
channels that can be read and individually frozen. Per-corner wheel-off/wheel-on,
both jacks, and the fuel hose are all named actors. See
`docs/HANDOFF_ANIMATION_MAPPING.md`.

---

## Live Tamper Proof (2026-07-29)

```
POKED: +0x3338 = 120L (was 87)
RESULT: Service used 121L refuel target — CONFIRMED WORKING

POKED: +0x3378 = 5 (monsoon)
RESULT: Game clamped to 4 (compound 5 out of range for track)

FREEZE: Continuously *+0x3470 += 5 while ps==3
RESULT: Box locked forever, lollipop drops visually but car can't move
        Lollipop animation NOT synced (MAS reads original deadline at service start)
        To sync: need Frida hook on sub_4A35F0
```

---

## Original Source Modules

| Module | Functions |
|---|---|
| `pitcrew.cpp` | PitCrewMeshLoader, sub_4A22D0, PitCrewAnimationLoader |
| `steward.cpp` | StewardLightGlowNodeFinder, Steward_Setup, Steward_Init, Steward_Restart |

---

## Dependencies on EVF Folder

All tools and docs reference paths within `mods/extended_vanilla_features/`.
Run tools from that folder's `tools/` directory.
External dependencies: `tools/poke.py` at corpus root, game running for live reads.
