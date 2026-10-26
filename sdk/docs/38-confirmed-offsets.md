# Confirmed Offsets — FACT Tier

Every entry here was verified by reading live game memory and cross-checking
against visible in-game state. Trust tier: **FACT** (same as raw `.c` files).

Do NOT add entries from docs, decompilation, or LLM analysis alone.
Only add entries that were poke-read and matched observable game behavior.

Last updated: 2026-07-20

---

## Globals

| Address | Name | Type | Verified Value | Notes |
|---------|------|------|----------------|-------|
| `0x007C54E8` | PlayerVehiclePtr | ptr | `0x005EC6D8` | Dereferenced, fields match player car |
| `0x007C54F0` | VehiclePtrArray | ptr | — | Array of vehicle pointers, stride 0x48C0 confirmed |
| `0x007C56A4` | SessionType | int32 | 5 in qualifying, 7 in race | **5=qualifying, 7=race (both live-verified 2026-07-27).** 0=none/loading and 1=practice are unverified; the previously documented `2=qual` is WRONG — qualifying reads 5. See findings.jsonl |
| `0x007D0308` | RaceActiveFlag | int32 | 3 on track | Nonzero = on track |
| `0x005C329C` | VehicleCount | int32 | 5, 24 | Matches visible car count |
| `0x007D0410` | ChampionshipMode | int32 | 1 | Observed; meaning unconfirmed |

## Vehicle Struct (relative to vehicle base pointer)

### Identity

| Offset | Name | Type | Verified Value | Notes |
|--------|------|------|----------------|-------|
| `+0x4620` | DriverName | string | `"Matias Driver"` | Matches in-game driver name |
| `+0x4678` | CarName | string | `"2001 Arrows"` | Matches in-game car selection |
| `+0x4714` | VehiclePath | string | `"SEASONDATA\VEHICLES\ARROWS\..."` | Full .VEH path |
| `+0x4794` | SaveFilePath | string | `"SAVE\Matias Driver\tempGarage.svm"` | NOT driver name (was mislabeled) |
| `+0x489C` | DriverID | int32 | 140 | Player's numeric ID |

### Dynamics — CONFIRMED 2026-07-20

| Offset | Name | Type | Verified Value | Method |
|--------|------|------|----------------|--------|
| `+0x4440` | Speed | float (m/s) | 79.07 @ 285 km/h, 60.57 @ 218 km/h | ×3.6 matches speedo exactly |
| `+0x39fc` | CurrentGear | int32 | 5 in 5th, 6 in 6th | Changed with gear shift |
| `+0x00a0` | EngineAngVel | float (rad/s) | 1697 @ 6th/285, 1357 @ 5th/218 | ×60/2π = RPM; changes with gear/speed |
| `+0x0094` | RevLimiter | float (rad/s) | 1815.8 (=17,340 RPM) | Constant across gears — max engine speed |

### Retirement & DNF — CONFIRMED 2026-07-21

Verified by reading all 22 vehicle structs during a race with 6 mid-race
retirements (Trulli, Irvine, Montoya, Manning, Sarrazin, Alonso) visible in
the pit garage, plus 4 post-race DNF classifications (Hakkinen, Frentzen,
Fisichella, Alesi) whose cars were NOT in the garage.

| Offset | Name | Type | Verified Value | Notes |
|--------|------|------|----------------|-------|
| `+0x4064` | FinishState | int32 | 0, 1, 2 | 0=running, 1=finished, 2=DNF, 3=DSQ. Set by `PitCrewStateMachine` (0x4BBFC0) via scoring sub-struct at vehicle+0x4034 |
| `+0x4068` | ReasonCode | int32 | 0, 6, 8 | DNF reason. 1=engine, 2=suspension, 4=brake, 5=gearbox, 6=puncture/flat, 8=electronics, 9=fuel. Set same place as FinishState. 0=no specific mechanical (collision or post-race classification) |
| `+0x4410` | RetiredFlag | byte | 0 or 1 | **THE pit-garage discriminator.** 1=car visually in pit garage with no driver. 0=car on track or classified DNF post-race. Set by `AIStateReset` (0x42AF40). Perfectly separates 6 garage cars (=1) from 16 non-garage cars (=0). Only field in entire 0x48C0 struct that does so |
| `+0x45F4` | ActiveFlag | byte | 0 or 1 | 1=driver active/racing, 0=retired/removed. Cleared by `AIStateReset` simultaneously with RetiredFlag. Garage cars=0, active cars=1. Not unique on its own (player also reads 0) |

**Key distinction:** FinishState=2 (DNF) does NOT mean the car is in the
garage. Post-race DNF classifications (Hakkinen P1, Frentzen, Fisichella,
Alesi) all have FinishState=2 but RetiredFlag=0 — `AIStateReset` never fired
for them because they were still racing when the session ended.

**Retirement cascade (IDA-confirmed call chain + live-observed transitions):**

Phase 1 — Damage detected (car still on track):
```
PitState   (+0x4040) = 2      (request pit for retirement)
AIMode     (+0x3450) = 1      (AI switches to pit-drive mode)
SpeedCap   (+0x453C) ≈ 0.90   (pit lane speed limit applied)
FinishState, ReasonCode, RetiredFlag, ActiveFlag all UNCHANGED
```
Observed live on Coulthard (idx 3) after rear wing loss from contact.
`+0x3450` is `*(vehicle+0) + 0x32CC + 0x184` — the AI state mode inside
`StateChangeNotify` (0x4A5C80). IDA disasm of `AIStateReset` (0x42AF40)
confirms: `ecx = [esi] + 0x32CC` before calling `StateChangeNotify`.

Phase 2 — Car reaches pit garage:
```
PitCrewStateMachine (0x4BBFC0):
  → vehicle+0x4064 = 2          (FinishState = DNF)
  → vehicle+0x4068 = reason     (from event data)
AIStateReset (0x42AF40):
  → StateChangeNotify(0)        (broadcast "stopped" — sets AIMode back to 0)
  → ConditionalGate(vehicle+12)
  → vehicle+0x4410 = 1          (RetiredFlag — car visually in garage)
  → vehicle+0x45F4 = 0          (ActiveFlag — driver removed)
  → SpeedCap reset to 1.0
```

| State | PitState | AIMode | FinishState | RetiredFlag | ActiveFlag | SpeedCap |
|-------|----------|--------|-------------|-------------|------------|----------|
| Racing | 1 | 0 | 0 | 0 | 1 | ~1.08 |
| Driving to pit (retire) | 2 | 1 | 0 | 0 | 1 | ~0.90 |
| In garage (retired) | 2 | 0 | 2 | 1 | 0 | 1.00 |

### Position — Physics vs Render (CONFIRMED 2026-07-22)

**Physics position** (rigid body / collision):

| Offset | Name | Type | Verified Value | Notes |
|--------|------|------|----------------|-------|
| `+0x1c` | PhysX | float | -501, -535 | Changed as car moved |
| `+0x20` | PhysY | float | -5.0 | Height (near track surface) |
| `+0x24` | PhysZ | float | 473, 564 | Changed as car moved. Teleported during retirement (579→321) |

**Render position** (scene graph node — what you see on screen):

| Offset | Name | Type | Source | Notes |
|--------|------|------|--------|-------|
| `+0x34F0` | RenderX | float | IDA: `AIVehiclePathPositionTracking` (0x4EF7C0) | `this=vehicle+0x0C`, field at `this+13524` |
| `+0x34F4` | RenderY | float | IDA | `this+13528` |
| `+0x34F8` | RenderZ | float | IDA | `this+13532` |

Physics and render positions are **independent**. Writing PhysY does NOT move the
visible car. Render position is updated by `AIVehiclePathPositionTracking` which is
called per-frame by `sub_4C6B60` during formation/race. During retirement, NO render
position write occurs — the car disappears because `ActiveFlag=0` stops the scene
graph from updating/rendering the model.

### Retirement Sequence — Frida Trace (CONFIRMED 2026-07-22)

100Hz polling of all AI vehicles during Hakkinen crash at Monza.
Car2 = Hakkinen, hit from behind, lost tire, retired to garage.

| Relative Time | Offset | Field | Change | Notes |
|---------------|--------|-------|--------|-------|
| T+0.0s | +0x4068 | ReasonCode | 0→6 | Accident detected immediately |
| T+4.0s | +0x45F4 | ActiveFlag | 1→0 | Physics/rendering disabled |
| T+10.2s | +0x0024 | PhysZ | 579→321 | Collision body teleported to garage |
| T+10.2s | +0x4064 | FinishState | 0→2 | DNF classification |
| T+10.2s | +0x4410 | RetiredFlag | 0→1 | Car in pit garage |
| T+10.2s | +0x4060 | RacePos | 21→22 | Dropped to last |

**NOT changed during retirement:** SpeedCap (+0x453C), AIMode (+0x3450),
PitState (+0x4040), RenderX/Y/Z (+0x34F0-34F8), QualPos (+0x4058).

**Key insight:** `ActiveFlag=0` is the visual kill switch. No render position
write needed — the scene graph stops updating when ActiveFlag=0. The PhysZ
teleport only moves the collision body to the garage area.

### QualPos Grid Slot — Exploration Results (CONFIRMED 2026-07-22)

Tested by setting QualPos via DLL before restart trigger, Monza, no formation lap.

| QualPos | Result |
|---------|--------|
| 0 | **CRASH** — array underflow (likely `array[QualPos-1]`) |
| 1-48 | Valid grid positions |
| 99 | Safe max (from oph.dll community mod). Car far behind grid, not always invisible on long straights. |
| 100 | No crash. Car position shifted (wraps around track waypoints). |
| 110 | No crash. Cars turned around, middle of track. |
| 122 | Crash (hook was active — may have been hook-related, not QualPos) |
| 198 | Inconsistent: underground once, mid-road once, crash once. Track-dependent waypoint mapping. |
| 500 | Wraps fully back to valid grid position (modulo waypoint count). |

### Scene Node Position — CONFIRMED VISUAL TELEPORT (2026-07-22)

**Scene graph base:** embedded struct at `0x5EA2C0` (NOT a pointer — IDA shows `dword_5EA2C0`
but the call passes the address, not the dereferenced value).

```
nodeTable  = *(0x5EA2C0 + 344)   // this[86] — heap pointer to node array
dim        = *(0x5EA2C0 + 356)   // this[89] — column dimension (observed: 52)
entryAddr  = nodeTable + 28 * (nodeIdx + subIdx * dim)
```

Each 28-byte entry layout:
| Offset | Field | Notes |
|--------|-------|-------|
| +0  | X | float, world-space |
| +4  | Y | float, world-space |
| +8  | Z | float, world-space |
| +12 | rotX | float, Euler angle (radians) |
| +16 | rotY | float, ~1.57 = 90° |
| +20 | rotZ | float |
| +24 | pad  | uint32, observed 0 |

**Vehicle scene node indices** read from vehicle struct:
- `vehicle+0x48A4` = nodeIdx
- `vehicle+0x48A8` = subIdx

**Confirmed visual teleport recipe (2026-07-22, Heidfeld + Raikkonen):**

1. Activate car for ~1s: poll `AF=1, RF=0, FinishState=0, ReasonCode=0` at 5ms — this re-inserts car into the 3D scene graph render pool (which `AIStateReset` removed on retirement).
2. Write scene node XYZ to desired world position.
3. Car visually jumps immediately. Write persists with no polling (nothing overwrites retired car scene nodes once tracker stops).

**Key distinctions:**
- Scene node coords ≈ physics coords for stationary/retired cars (nearly 1:1).
- Active cars (AF=1): `AIVehiclePathPositionTracking` (0x4EF7C0) runs at 840Hz and continuously updates the scene node from physics. Writing scene node for active cars is immediately overwritten.
- Retired cars (AF=0): scene node is frozen. One-shot write teleports the visual model permanently.
- `AIStateReset` removes car from render pool when retiring. Must briefly re-activate (AF=1) to restore render pool membership before scene node writes are visible.

**Complete synchronized teleport recipe (CONFIRMED 2026-07-22, Salo appeared to player's right):**

```python
# 1. Activate for ~1s (re-insert into 3D render pool)
#    Poll at 5ms: AF=1, RF=0, FinishState=0, ReasonCode=0

# 2. After 1s, write BOTH physics and scene node atomically:
vehicle.add(0x1C).writeFloat(TX)      # physics X (collision body)
vehicle.add(0x20).writeFloat(TY)      # physics Y
vehicle.add(0x24).writeFloat(TZ)      # physics Z
entry.writeFloat(TX)                   # scene node X (visual)
entry.add(4).writeFloat(TY)           # scene node Y
entry.add(8).writeFloat(TZ)           # scene node Z
# entry = nodeTable + 28*(nodeIdx + subIdx*dim)
```

Physics may drift ~4m after write (terrain/contact settling) — acceptable for POC.
Scene node holds permanently (nothing overwrites it for AF=0 cars).

**Collision test result:** hitting a teleported retired car updates physics only (Z +10.6m),
scene node unchanged — visual and collision body decouple on impact.

**Tool:** `tools/teleport_car.py` — CLI for listing cars and teleporting by name.

### Scene Graph Visibility — DISPROVEN (2026-07-22)

| Offset | Hypothesis | Result |
|--------|-----------|--------|
| +0x39A0 | Scene visibility flag (ConditionalGate writes 0 here during retirement) | Writing 0 after restart does NOT hide car. Car freezes in "weird position" (flipped/crashed on trackside). SceneRenderUpdate already rebuilt the scene node. |

**Key finding:** `SceneRenderUpdate` (0x4C6D90) rebuilds the car model during
`DynMan_Restart` (0x42C3B0). Any post-rebuild memory write to scene graph flags
is too late — the node is already re-attached and rendered. The only way to
prevent DNF car rendering is to skip the `SceneRenderUpdate` call entirely
(hook approach, untested as of 2026-07-22).

### Restart State Reset — Frida Trace (CONFIRMED 2026-07-22)

During restart (gs=4→5), ALL retirement state wiped simultaneously:
- FinishState → 0
- ReasonCode → 0
- RetiredFlag → 0
- PitState → 2
- SceneFlag0C, SceneByte970, all VehicleRespawnReset fields → zeroed

This happens BEFORE `SceneRenderUpdate` rebuilds car models.
Our DLL writes happen AFTER rebuild — formation code then overwrites at 60fps.

## Vehicle Array

| Fact | Value | Method |
|------|-------|--------|
| Stride | 0x48C0 bytes | DriverIDs at [0]+0x489C, [1]+0x489C with 0x48C0 gap |
| Player slot | [0] in array | PlayerVehiclePtr == VehiclePtrArray[0] |

## Disproven Claims

| Offset/Address | Doc Claim | Reality | Source |
|----------------|-----------|---------|--------|
| `+0x1F00` | SelfPtr (pointer to self) | Reads 0 | Live read 2026-07-20 |
| `+0x4028` | Engine health float | Per-frame control surface input, overwritten by `sub_4B2B40` | IDA + live poke (poke to 0 did nothing) |
| `+0x0DF0,0EE8,0FE0,10D8` | F1CUI tire puncture (FL/FR/RL/RR) | Camera/control doubles — writing 100.0 caused camera shake | Live poke 2026-07-20 |
| `+0x35e8,36e0,37d8,38d0` | Tire temps (Kelvin) | ~300 at 218 km/h, ~301 parked — tracks speed not temperature | Live comparison 2026-07-20 |
| `+0x3984` | Speed candidate | Reads 89 m/s=322 km/h while parked — NOT speed | Live read while stationary |
| `0x007D0C60` | TireWearMultiplier | 1.4e-45 (garbage/near-zero) | Wrong address or disabled |
| `+0x4794` | DriverName | Save file path string | Live read showed `SAVE\...\tempGarage.svm` |
