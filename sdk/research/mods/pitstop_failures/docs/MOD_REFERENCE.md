# pit_commentary.dll — Complete Reference

> Built 2026-07-29. Merges pit exit commentary + pit-stop failures into one DLL.
> Source: `patches/pitstop_modding/dll/pit_commentary.c`

---

## 1. What it does

Single polling DLL (100ms, no code hooks) that watches the player's `pittingStatus`
(veh+0x3450) and injects commentary events through the game's own
`InsertDriverTableEntry` pipeline. Also monitors AI vehicles for failures.

### 1.1 Pit Exit Commentary (unconditional, edge-triggered)

| Transition | EID | Clip | Audio |
|------------|-----|------|-------|
| 0/1 → 2 | 51 | PWS_PIT06 | "Limiter On" — entering pit lane |
| → 3 | 53 | PWS_PIT08 | "Brake" — entering pit box |
| 3 → 4 | 45 | PWS_PIT10 | "Go" — box exit |
| 4 → 0 | 43 | PWS_PIT12 | "Limiter Off" — exiting pit lane |

State 1 may be skipped (0→2 directly) — handled by `(prev==0||prev==1) && ps==2`.

### 1.2 Pit Failures (player, slot 0)

All failures are EITHER auto-rolled (random) OR queued via GUI (see §6).

#### Wheel Gun Failure (per-corner, 3-clip sequence)

| Phase | EID (FL/FR/RL/RR) | Audio | Timing |
|-------|-------------------|-------|--------|
| Problem | 63/65/68/70 | "Problem with [corner]" (PWS_2001_72-75) | T+0 (7-10s after tyreJobStart) |
| Jammed | 66 | "Wheel jammed" (PWS_2001_77) | T+4s |
| Resolved | 62/64/67/69 | "[Corner] clear" (PWS_2001_68-71) | T+22s (3s before release) |

Extends `stopEndTime` by +25s. Shows HUD: "WHEEL GUN FAILURE (XX)" → "WHEEL GUN FIXED (XX)".

Corner picked randomly each time. tyreJobStart detection at veh+0x349C, gated on `>= stopStartTime` (veh+0x346C).

#### Other Player Failures (auto-roll or queued)

| Prob | EID | Clip | Delay | Notes |
|------|-----|------|-------|-------|
| 15% | 61 | PWS_2001_76 Wheel gun failure | +15s | Generic (superseded by per-corner when queued) |
| 8% | 56 | PWS_2001_79 Fuel hose issue | varies | Deferred — 3 variants (see §7) |
| 3% | 54 | PWS_2001_80 Engine stall | +5s | Immediate on serviceReady |
| 2% | 66 | PWS_2001_77 Wheel jammed | +12s | Severe, can lead to DNF |
| 12%/corner | 63/65/68/70 | Slow tire per corner | +2.5s each | Multiple corners can stack |

Slow tire and wheel gun both use eids 63/65/68/70 — slow tire fires immediately on serviceReady, wheel gun fires deferred with the 3-clip sequence.

#### AI Failures (slots 1-103)

| Prob | EID | Clip | Delay |
|------|-----|------|-------|
| 3% | 184 | PWS_2001_131 AI engine stalled in pits | +6s |
| 6% | 185 | PWS_2001_132 AI fuel hose jammed | +4s |
| 4%/corner | 186-189 | PWS_2001_133-136 AI wheel stuck per corner | +5s |
| 8% | 183 | PWS_MISC30 AI generic pit issue | +3s |

---

## 2. Key Offsets

### 2.1 Globals (FACT, non-ASLR exe)

| Address | Type | Name | Role |
|---------|------|------|------|
| `0x005EC6D8` | ptr | VEH_BASE | Vehicle array, slot 0 = player |
| `0x48C0` | — | VEH_STRIDE | Per-slot stride |
| `0x005EA618` | ptr | CONTROLLER | Commentary controller |
| `0x005BF9A0` | ptr | EVENT_TABLE | Event cooldown/score/priority table |
| `0x007D0308` | int32 | RACE_ACTIVE | 3 = on track |
| `0x008AFEA0` | double | SIMTIME | Simulation clock |
| `0x007CD204` | ptr | HUD_OUTER_PTR | HUD overlay slot array inner ptr |

### 2.2 Vehicle Struct (per-slot, base VEH_BASE + slot*VEH_STRIDE)

| Offset | Type | Name | Role |
|--------|------|------|------|
| `+0x32C8` | int32 | active | -1=inactive, 0=player, 1=AI |
| `+0x3450` | int32 | pittingStatus | 0=track, 1=req, 2=lane, 3=box, 4=exit |
| `+0x345C` | int32 | serviceReady | 1=car stationary, service queued |
| `+0x346C` | float | stopStartTime | Sim-time service began |
| `+0x3470` | float | stopEndTime | Box-lock deadline (LATCHED, POKE THIS) |
| `+0x3478` | int32 | lollipop | 1=up, 0=down (RECALCULATED) |
| `+0x347C` | float | fuelAtStart | Fuel when service began |
| `+0x3480` | float | refuelRampStart | When fuel starts flowing |
| `+0x3484` | float | targetFuel | Committed fuel target |
| `+0x3490` | int32 | tyreChg | 1=tire change active (OUTPUT) |
| `+0x3494` | int32 | wingChg | 1=wing change active (OUTPUT) |
| `+0x3498` | int32 | repair | 1=repair active (OUTPUT) |
| `+0x349C` | float | tyreJobStart | Tire animation start sim-time |
| `+0x34A0` | float | tyreJobEnd | Tire animation end sim-time |
| `+0x34A4` | int32 | QUEUE_EID | GUI writes queued eid here |
| `+0x34A8` | float | QUEUE_DELAY | GUI writes queued delay here |
| `+0x3984` | float | FuelLevel | Current fuel (RECALCULATED) |
| `+0x454C` | int32 | callToBox | 1=pit confirmed |

### 2.3 Commentary Controller (CONTROLLER + offset)

| Offset | Type | Role |
|--------|------|------|
| `+0x864` | struct[0x14] | Queue head entry |
| `+0xAF0` | int32 | COMMENTARY_ON flag |
| `+0xAF8 + eid*4` | float | Per-event last-played timestamp |
| `+0xAE8` | float | Playback timestamp (set to 0 to kick) |
| `+0x135C` | int32 | Starter timing signal |

### 2.4 HUD Overlay

| Address | Role |
|---------|------|
| `0x7CD204` → inner ptr | Slot array base |
| `slot*0x5C + 0x3C` | Text buffer (31 chars) |
| `slot*0x5C + 0x90` | Timer float (seconds to display) |

8 slots, 0x5C stride. Write text + set timer to show. Timer decrements automatically.

---

## 3. Injection Pipeline

The DLL uses the proven `InsertDriverTableEntry` path (same as `pit_exit_commentary.c`):

```c
FN_STRUCT_INIT(buf);                                    // 0x004144D0
FN_DRIVER_LOOKUP(buf, eid, -1, -1, -1, -1, -2.0, -2.0); // 0x004144F0
FN_INSERT_ENTRY((void*)CONTROLLER, buf);                 // 0x00418560
```

Cooldown bypass: `EVENT_TS(eid) = 0.0f` before injection (same as `commentary_inject.py`).

Timer extension: `stopEndTime = simTime + HOLD_LEAD + delay` (same as `freeze_pitstop.py`).
HOLD_LEAD = 3.0 seconds — fail-safe if this DLL stops writing, the car releases within 3s.

---

## 4. State Machine

### 4.1 Per-Slot State (SLOTS = 104)

```
g_prev_pitting[s]  — previous pittingStatus for edge detection
g_failure_eid[s]   — latched failure eid for this stop (-1 = none)
g_failure_delay[s] — latched delay in seconds
```

### 4.2 Wheel Gun State (player only)

```
g_wheelgun_armed       — 1 = deferred, waiting for tyreJobStart
g_wheelgun_corner      — 0=FL 1=FR 2=RL 3=RR
g_wheelgun_fire_at     — sim_time to fire "problem" clip
g_wheelgun_jammed_at   — sim_time to fire "wheel jammed" clip (+4s)
g_wheelgun_resolved_at — sim_time to fire "clear" clip (+22s)
```

State flow:
```
Box entry → armed=1, pick corner
  ↓
serviceReady=1 + tyreJobStart >= stopStartTime
  ↓
set fire_at = now + 7-10s, armed=0
  ↓
sim_time >= fire_at → fire slow clip + HUD, set jammed_at + resolved_at
  ↓
sim_time >= jammed_at → fire "wheel jammed"
  ↓
sim_time >= resolved_at → fire "clear" + HUD
```

### 4.3 Fuel Failure State (player only)

```
g_fuel_variant         — FUEL_VAR_NONE(0), SLOW(1), NOFUEL(2), CUT(3)
g_fuel_orig_ramp       — original refuelRampStart (captured before manipulation)
g_fuel_srv_start       — sim_time when service became ready
g_fuel_cut_since       — sim_time when cut started (variant C)
g_fuel_last_level      — previous poll fuel level
g_fuel_fired           — 1 once audio has been injected
g_fuel_delay           — penalty delay for this variant
```

---

## 5. GUI (pit_failure_gui.py)

### 5.1 Launch

```
cd mods/extended_vanilla_features
python tools/pit_failure_gui.py
```

### 5.2 Features

- **Real-time pit state**: 16 fields (pittingStatus, stopEndTime, fuel, service flags, etc.)
- **Manual injection buttons**: 8 failure types with per-corner wheel gun variants
- **Custom EID entry**: Inject any eid with custom delay
- **Timer extend**: Poke stopEndTime directly while in box
- **Queue system**: Click button outside pits → queued via veh+0x34A4 → fires on next pit entry
- **Force mode**: Checkbox to inject audio immediately anywhere (timer not extended)
- **Pause toggle**: Checkbox to disable DLL auto-roll (named event)
- **Log panel**: Shows edge transitions, service events, injections, tails DLL log

### 5.3 Injection Modes

| Mode | How | When fires | Timer extends |
|------|-----|------------|---------------|
| Queue | Click button (not in box) | Next pit entry | Yes |
| In Box | Click button while pittingStatus=3 + serviceReady=1 | Immediately | Yes |
| Force | Check FORCE, click anywhere | Immediately | No |

---

## 6. Queue System

GUI writes to veh+0x34A4 (eid, int32) and veh+0x34A8 (delay, float).
DLL reads these on the `pittingStatus → 3` edge, clears them, and uses the queued values
instead of auto-rolling. Write cleared after read to prevent replay on next stop.

Queue must be re-written by GUI before each pit stop.

---

## 7. Fuel Manipulation (Variants A/B/C)

Game formula: `FuelLevel = fuelAtStart + (simTime - refuelRampStart) * 12.0`

DLL manipulates `refuelRampStart` (veh+0x3480) to change apparent flow:

### Variant A — SLOW_PUMP
Push ramp forward each poll: `fake_ramp = now - elapsed * 0.333333f`
→ apparent rate drops to 4 L/s (instead of 12 L/s)
Detected when elapsed > RAMP_DELAY + DETECT_SLOW (9s after service start)
Audio: eid 56 (PWS_2001_79), delay +3s

### Variant B — NO_FUEL
Push ramp to `simTime + 999999.0f` (never starts)
Detected when elapsed > DETECT_NONE (2s after expected ramp)
Audio: eid 56, delay +5s

### Variant C — INTERMITTENT
Let fuel flow normally until 3L delivered, then cut (push ramp to +inf)
Detected 2s after cut
Audio: eid 56, delay +2s

### Cancellation
If box exit occurs before detection, variant is cleared silently.
If variant A detects normal rate (>= 4 L/s), failure is cancelled.

---

## 8. HUD Text Overlay

Uses the same slot system as `red_flag/gr41.c`:
- 8 slots at inner_ptr + slot*0x5C
- Write ASCII text at +0x3C
- Set timer float at +0x90 (seconds to display)
- Timer decrements automatically, text clears when timer ≤ 0

Messages:
| Event | Text |
|-------|------|
| Wheel gun problem | WHEEL GUN FAILURE (FL/FR/RL/RR) |
| Wheel gun resolved | WHEEL GUN FIXED (FL/FR/RL/RR) |

---

## 9. Build, Inject, Unload

### Build
```
cd patches/pitstop_modding/dll
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o pit_commentary.dll pit_commentary.c -lkernel32
```

### Inject
```
echo . | %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\red_flag\inject.exe pit_commentary.dll
```

### Unload (self-eject, safe)
```
cd mods/extended_vanilla_features
python tools/dll_unload_signal.py pit_commentary
```

The DLL watches named event `F1C_EVF_UNLOAD_pit_commentary`. When signalled,
it breaks its poll loop and calls `FreeLibraryAndExitThread` — the thread
unloads itself, no remote-thread crash risk.

### Pause auto-roll
```
python tools/dll_unload_signal.py pit_commentary   # to unload
```
Pause via GUI checkbox or named event `F1C_EVF_PAUSE_pit_failure_commentary`.

### Log
`pit_commentary_log.txt` in the game directory.

---

## 10. Files

| File | Role |
|------|------|
| `dll/pit_commentary.c` | Merged DLL source (exit commentary + failures) |
| `dll/pit_commentary.dll` | Compiled 32-bit DLL |
| `tools/pit_failure_gui.py` | Tkinter debug GUI |
| `tools/dll_unload_signal.py` | Safe DLL ejector |
| `pit_commentary_log.txt` | Runtime log (game directory) |

---

## 11. Known Limitations

1. **Lollipop desync**: Extending `stopEndTime` holds the car but the MAS lollipop
   animation reads its original deadline at service start. The lollipop drops
   visually before the car is actually released. Fix requires hooking
   `RangeProportionInitializer` (0x4a1fd0) like `freeze_crew_byptr.py` does.

2. **Queue single-slot**: The commentary controller has one queue slot. Three
   clips injected simultaneously (attempted with priority system) collide.
   Current workaround: space clips 4s apart.

3. **Manual priority injection**: Writing boosted priority directly to the queue
   head broke audio entirely (2026-07-29). Reverted to standard
   `InsertDriverTableEntry` path. Priority boost not yet working.

4. **Fuel RECALCULATED**: `FuelLevel` is recalculated every frame. Direct pokes
   don't persist. Instead, manipulate `refuelRampStart` to control apparent rate.

5. **tyreJobStart equals stopStartTime**: These values are identical at box
   entry — detection gate must use `>=` not `>`.

6. **Pit exit commentary state skip**: pittingStatus may skip state 1 (0→2
   directly). Limiter On trigger must handle both 0→2 and 1→2 edges.

7. **AI auto-roll shares pause**: The pause event disables BOTH player and AI
   auto-roll. No separate toggle.

---

## 12. Prior Art & Findings

| Finding | Source |
|---------|--------|
| Crew mapping SOLVED: `actor+100` ASCII name, 34/34 | dump_crew_struct.py |
| pittingStatus full enum: 0=track 1=req 2=lane 3=box 4=exit | 4+ live pit cycles |
| stopEndTime LATCHED, poke works | persistence_map.jsonl + live tests |
| refuelRampStart manipulation proven for rate control | PitStopOperationTimer analysis |
| tyreJobStart detection needs `>=` not `>` | Live debug logs |
| veh+0x33F8 repair trigger path — NOT reliable | Code path may skip |
| HUD overlay system: 8 slots at 0x7CD204→inner, stride 0x5C | red_flag/gr41.c |
| fuelAtStart + 25 - simTime penalty: 0-4.5s | PitStopOperationTimer |
| refuel rate constant 0x59E96C = 12.0 L/s | PitStopOperationTimer |
| wing repair: 8.0s (FW) / 39.0s (RW) via damage flags | PitStopOperationTimer |
| driverStatus offset: veh+0x4510, 0=active 5=garaged | web_telemetry.c |
| EngineRPM: veh+0x39A0, EngineAngVel: veh+0xA0 | web_telemetry.c |
| Ignition kill: RPM+AngVel=0, but VOLATILE — physics recalculates | web_telemetry.c |
| Tyre crew log: TYRE CREW: tyre_start= srv= sim= | Live debug |

---

## 13. Wheel Gun WAV Sound Effects

Plays on EVERY pit stop when tyre crew begins work (tyreJobStart detected).
Randomly selects one of three WAV files:

```
%USERPROFILE%\Desktop\F1 Challenge V10\Audio\Vehicle\wheelguns_1.wav
%USERPROFILE%\Desktop\F1 Challenge V10\Audio\Vehicle\wheelguns_2.wav
%USERPROFILE%\Desktop\F1 Challenge V10\Audio\Vehicle\wheelguns_3.wav
```

Loaded via `PlaySoundA` from `winmm.dll` (dynamic load, no link dependency).
Plays 2 seconds after tyreJobStart is detected (gated by `g_wg_sfx_at` timer).
SND_FILENAME | SND_ASYNC flags — non-blocking.

---

## 14. Limiter Off False-Positive Fix

**Bug:** pittingStatus resets to 0 when game is paused (ESC/menu), triggering
"Limiter Off" (eid 43) incorrectly. Same class of bug as retire_commentary v1.

**Fix:** `g_last_go_time` tracks when "Go" (3→4) last fired. Limiter Off (4→0)
only fires if Go was within the last 60 seconds.

---

## 15. Tyre Crew Activity Debug Log

Every pit stop logs tyre crew activation (independent of failure system):

```
TYRE CREW: tyre_start=XXXX.X srv=XXXX.X sim=XXXX.X
```

Uses `static float last_tyre_log` to avoid spam — logs only when tyre_start value changes.
This runs regardless of whether a failure is active.

Also logs wheel gun WAV playback:
```
SFX: wheel gun WAV played
```

---

## 16. Wheel Jammed DNF System (INCOMPLETE)

### Design
3-clip sequence → "and that's the end" (eid 117, PWS_RETIRE02) → car exits pit lane → terminal flags + ignition kill.

### State Machine
```
g_dnf_pending  — set at resolved time, triggers DNF on pit exit (4→0)
g_dnf_active   — set after DNF executed, for tracking
```

### Terminal Flags Set (on 4→0 edge)
- veh+0x3D6C (carConditions) = 2
- veh+0x4068 (ReasonCode) = 6 (wheel loss)
- veh+0x4064 (FinishState) = 2 (retired)
- veh+0x4510 (driverStatus) = 5 (garaged)
- veh+0x4488 = 1
- veh+0x4578 = 1

### SOLVED 2026-07-29 — player retirement is two writes, no hook needed

```c
*(int *)(VEH_BASE + 0x4064) = 2;    /* FinishState = DNF        */
*(int *)0x007CE258          = 1;    /* player out -> teardown   */
```

`RestartGate_004a0fd0` consumes `dword_7CE258` on its next tick and calls
`ConditionalInitGuard()` -> `sub_4A0250()`, which tears the session down and
returns to the menu. This is the game's own path: `sub_4F1FE0`'s player-retire
branch ends with `if (!this[3247]) dword_7CE258 = 1;`.

Live-proven: the flag read back 0 within 0.5 s (RestartGate consumed it) and the
retirement + menu return happened on screen. `dword_7C569C` stays 5 throughout,
so the visible teardown comes from `sub_4A0250()`, NOT the `7C569C = 6` arm
(that arm needs `dword_7CE264 == 1`).

**Neither `CreateRemoteThread` nor a `sub_4F1FE0` call is required.** Options A
and B previously listed here were never needed. Implemented in
`dll/pit_commentary.c` as `retire_player()`.

### DISPROVEN — do not retry the ignition route

The Ignition keybind cannot be replicated. All of the following were tested live
on 2026-07-29 and failed:

| Attempt | Result |
|---|---|
| `PostMessage` / `keybd_event` of the ignition key | no effect (was already dead code) |
| Write scancode 51 into the game's key queue (`word_7CB21C` + `dword_7CB218`) | accepted, engine unaffected |
| Write 0x80 into the DirectInput keyboard buffer at index 51 | DirectInput refreshes the buffer from the device before the game reads it |
| Poke `veh+0x39A0` engine speed = 0 | RECALCULATED — restores within one 30 ms frame (audible RPM dip, then recovery) |
| Poke `veh+0x3984` to 0 | that offset is FUEL, not an ignition gate; it is rewritten every frame |

The binding itself is real and confirmed — `Save/<name>/<name>.PLR` line
`Control - Ignition="(0, 51)"` = device 0 (keyboard), scancode 51 (`,`) — but
there is no reachable ignition state to write. Use `retire_player()` instead.

---

### SOLVED 2026-07-30 — retirement to PITS (not menu) via SessionType flip

`sub_4A0250()` reads `dword_7C56A4` (SessionType). When ==7 (race), the race
block fires `LookupDispatch("POST RACE MONITOR")` → main menu. Flipping to 5
before writing `PLAYER_OUT=1` skips the race block, routing `sub_4A0250()` to
the garage/pits screen.

```c
/* DO NOT COPY THIS - it is the 2026-07-30 version and it is a coin flip.
   See the CORRECTION immediately below. */
static void retire_player_to_pits(void) {
    int saved = SESSION_TYPE;
    if (saved == 7) SESSION_TYPE = 5;   // skip race block in sub_4A0250
    retire_player();                     // FinishState=2 + PLAYER_OUT=1
    Sleep(500);                          // let RestartGate consume flag
    if (saved == 7) SESSION_TYPE = saved;
    hide_driver_model();                // veh+0x105D = 0, after teardown
}
```

Proven live 2026-07-30 — **but only because the race below happened not to be
lost that day. See the correction.**

### CORRECTION 2026-08-02 — the flip races SessionSynchronizer, and always did

The snippet above worked for weeks and then started restarting the race.
Nothing had changed in it. Driver reports, in order: *"why is the pit failure
script restarting the race, it should just throw me into the pit garage … It
worked before"*, then *"threw me back into the grid causing an effective race
restart. When I was in the pit box, I noticed that the race was restarted."*

`SessionSynchronizer_0045f950` is a session-type **change detector**:

```c
v13 = *v26;                                        // authoritative session type
if ( dword_7C56A4 != v13 && v13 < 8 )              // our flip IS this mismatch
{
    if (!byte_7CE254) sub_4A0250(&unk_7CE24C);     // teardown
    MultiplayerVehiclePlacement(&dword_7C569C, v13);   // RE-PLACES EVERY VEHICLE
    GlobalFlagSetter(&unk_7CE24D);                 // == byte_7CE25D = 1
}
```

- `GlobalFlagSetter_004a0da0` is one statement: `byte_7CE25D = 1`.
- `RestartGate_004a0fd0`'s own header: *"byte_7CE25D -> return 4 (mini-restart
  with cleanup)"*.
- `MultiplayerVehiclePlacement` is the "setting vehicle places for RACE"
  function — that is the player being put back on the grid.

Live log, 2026-08-02: `gate ran after 31ms (GameState 5 -> 4)` — exactly
`RestartGate`'s mini-restart return, at the instant of the retirement.

**The hold length only changes the odds** that the synchronizer ticks inside
the window. 500 ms was a coin flip; two intermediate fixes that lengthened the
hold (to 5 s, then 900 ms) made it *worse*, and the 5 s version also broke the
session outright — SessionType 5 is qualifying, and a retirement in qualifying
ends the session, so the race did not continue. Do not try to fix this with
timing.

**Working version (driver-verified 2026-08-02), in `dll/pit_commentary.c`:**

1. Clear `byte_7CE25D` and `byte_7CE25E` before starting — anything already
   pending would fire the moment the gate runs.
2. Flip, call `retire_player()`, then poll at 5 ms **pinning both restart
   flags at 0** and re-asserting the flip. Even if the synchronizer fires,
   `RestartGate` finds nothing to consume: no mini-restart, no re-grid.
3. Wait on `PLAYER_OUT (dword_7CE258) 1 -> 0`, capped at 900 ms. That is the
   exact consumption signal — **`ConditionalInitGuard_004a0cf0` clears it**
   (`result = sub_4A0250(); dword_7CE258 = 0;`). `RestartGate` itself never
   clears it, which is what makes a reading of `RestartGate` alone conclude,
   wrongly, that the flag is never cleared.
4. Restore SessionType **immediately** — every extra millisecond at 5 is
   another chance to be caught.
5. Keep eating restart flags for a 2.5 s tail: the synchronizer can fire on
   either edge of the flip, and `RestartGate` only consumes on its next tick.

The DLL logs `swallowed N mini-restart …` or `no restart was requested`, so a
future run says whether the guard is doing work or the flip slipped past
unnoticed.

`findings.jsonl`: `f1c-sessiontype-flip-trips-sessionsynchronizer-regrid`
(CONFIRMED), `f1c-retire-to-pits-restart-guard-verified` (FACT). Driver model hide (`veh+0x105D`, byte) must run AFTER
teardown — `PitCrewStateMachine_004bbfc0` resets it during `sub_4A0250`.

### SOLVED 2026-07-30 — wheel gun SFX timing from live observation

Crew animation windows (baseStart/baseEnd from channel+12/+16) do not directly
map to audible timing — values are proportion-based, not second offsets. Gun
SFX timing determined by live observation (`tools/trace_pitstop_manual.py`):

| Corner | OFF burst (srv_start+) | ON burst (srv_start+) |
|--------|----------------------|----------------------|
| RL, RR | +6.0s | +9.5s |
| FL     | +7.3s | +10.8s |
| FR     | +7.5s | +11.0s |

Bursts staggered per corner: rears first, fronts ~1.3s later. ON ~3.5s after
OFF. Reference is `srv_start` (veh+0x346C), not `tyre_start` (0x349C) — the
latter fires ~5s before crew animation begins. Two bursts per corner (OFF =
nut removal, ON = tightening), 8-voice `waveOut` pool prevents clipping.

### Tools added 2026-07-30

| Tool | Purpose |
|------|---------|
| `tools/trace_pitstop_manual.py` | Manual pit stop timing with hotkeys (NUMPAD0/7/8/9). Records crew start, wheel off, gun timing relative to box entry. |
| `tools/pit_failure_gui.py` (updated) | Crew Animation tab — full-size Treeview showing all 34 actors with animation windows, green active highlight, real-time elapsed. Reads all channels per actor. |

### GUI updates 2026-07-30

- Tabbed interface: "Pit Control" + "Crew Animations"
- Crew tab: 34 actors, scrollable, live green highlight for active animations
- Shows channel count, start/end times, role extraction from actor names
- Pit exit commentary suppressed when `g_dnf_active` set (prevents spurious "Go")

### DLL changes 2026-07-30

- `retire_player_to_pits()` — SessionType flip for garage/pits path
- `hide_driver_model()` — called after teardown, byte write to veh+0x105D
- Gun SFX: hardcoded timing from live observation, no more crew channel reads
- Gun SFX gate: `srv_start > 0` (was `tyre_start >= srv_start && tyre_start > 0`)
- ON burst added: second gun burst per corner matching tightening phase

### FUEL SPILL variant (FUEL_VAR_SPILL = 4)

New fuel failure type. Fuel flows at normal rate (12 L/s) — no rate manipulation.
At resolution time, target is reduced by `fuel_spill_pct` of the **remaining
delta** (target - currentFuel), not the absolute target. Audio fires once at
ramp detection, HUD shows at resolution only.

| Setting | Default | Description |
|---------|---------|-------------|
| `[duration] fuel_spill_{min,mode,max}` | 3.0/5.0/10.0 | Delay until spill detected |
| `[fuel] spill_loss_pct` | 0.30 | Fraction of needed fuel lost |

EID: 9005 (pseudo). Randomly picked as 1/4 of fuel failures. GUI button in
Tyre & Fuel group.
