# Mechanical Failures — Player Gearbox, Engine and Suspension Faults

This folder holds **three independent DLLs**:

| DLL | Covers | Control | Status |
|---|---|---|---|
| `dll/gear_failure.dll` | gearbox: stuck / lost gears / neutral | GUI over shared memory | shipped, driver-confirmed |
| `dll/engine_failure.dll` | engine: staged wear death, seizure, smoke/fire emitters | numpad + number-key hotkeys, in-game HUD | shipped, driver-confirmed |
| `dll/suspension_failure.dll` | suspension: staged wheel-bend damage via the game's own input | Num7 + GUI tab, in-game HUD | shipped, live-proven 2026-08-08 |

They share no hook site and can be injected together. The rest of this file
documents the gearbox DLL; the engine one is summarised in
[§ Engine failures](#engine-failures-engine_failuredll) below and mapped in full
in [`docs/ENGINE_FAILURE.md`](../../docs/ENGINE_FAILURE.md); the suspension one
in [§ Suspension failures](#suspension-failures-suspension_failuredll).

---

## Gearbox failures (`gear_failure.dll`)

Gives the **player** a jammed or stripped gearbox. Two failure modes, one hook.

| Mode | Behaviour | Real-world analogue |
|------|-----------|---------------------|
| **stuck** | No gear change at all. The car is locked in whatever gear it was in. | Selector fork jammed |
| **lost gear(s)** | Any number of gears can be gone at once. Selecting one either **skips past** it to the next working gear, or **drops to neutral** (recoverable — select another gear). | Stripped dog teeth / worn dog rings |
| **neutral** | Terminal: nothing engages at all, the engine free-revs. Optional recovery timer. | Gearbox destroyed |
| **both** | stuck + lost | |

Lost gears are chosen with a tick per gear, so "lost 2nd and 3rd" gives a box
that jumps 1↔4. If *every* gear is lost the walk falls through to neutral.

## Files

```
dll/gear_failure.c      the mod (detour on the player SetGear)
dll/gear_failure.dll    build output
mechanical_failure_gui.bat  launcher (elevates itself)
../../tools/mechanical_failure_gui.py   manual trialing GUI
```

## Use

```bash
python tools/mechanical_failure_gui.py
```

1. Start the game, get in the car
2. **Inject DLL**
3. Pick a mode and parameters
4. **ARM FAILURE** while driving — `Stuck gear = 0` locks whatever gear you're in
5. **CLEAR** to release, **Unload DLL** to remove the hook and self-eject

Needs admin (`OpenProcess`, injection). Hot-swappable: `Unload DLL` restores the
original bytes and frees the module, so you can rebuild and re-inject without
restarting the game.

## How it works

`PositionLapStateUpdate` at **0x447E00** is the player's `SetGear` — misnamed in
the corpus; it is the player counterpart of the AI's `sub_4EF930`.

```
__thiscall, this = 0x7C7428 (static player physics object)
requested gear   = [esp+4]
called from VehicleMainPhysicsUpdate 0x4441F0

this+0x1EF0 = 0x7C9318   current gear (zeroed while shifting)
this+0x1EF4 = 0x7C931C   target gear
this+0x1EF8 = 0x7C9320   shift direction (+1 up / −1 down)
```

Its first line is `if (newGear == this->[0x1EF4]) return;` — the game's own
no-op path. The mod reuses it:

```c
stuck   : if (want != locked) want = locked;   /* function returns, nothing happens */
lost    : if (broken_mask & (1 << want))       /* walk to the next working gear,   */
              want = next_working(want, cur);  /* or 0 if there is none            */
neutral : want = 0;                            /* the game's own arg==0 branch     */
```

Passing `0` works because the setter has an explicit `arg == 0` branch that sets
the target to neutral — we're using the game's own path, not forcing a value.

**Every filter branch must gate on `engaged`, not `enabled`.** `CLEAR` clears
`engaged`; a branch that checks only `enabled` keeps running after the driver has
switched the fault off. That shipped once and left the car unable to upshift out
of 1st with no way to release it.

A 5-byte `jmp` replaces the prologue (`8B 44 24 04 | 56` — a whole-instruction
boundary), vectors through a stub that saves state, calls the filter, then runs
the stolen bytes and resumes at 0x447E05. The hook refuses to install if the
prologue bytes don't match what was reversed.

Because this intercepts the **decision**, nothing races the game. One
interception per shift attempt.

## Why not a memory poke (read this before "simplifying" it)

Every offset the original polling script wrote is downstream of the decision and
does nothing. All live-tested 2026-07-30, all in `findings.jsonl`:

| Offset | What it really is | Effect of writing it |
|---|---|---|
| `veh+0x3A10` | genuine shift lockout — but gates `sub_426330`, which the **player never enters** | none |
| `veh+0x39FC` | display mirror, rewritten ~400 Hz from `0x7C9318` by `0x4E4C4F` | HUD moves, car shifts normally |
| `veh+0x39DC` | genuine gear ratios from `.hdv [DRIVELINE]`, but read **once at session load** | held perfectly, changed nothing |
| `0x7C9318` | the gear itself, but written by the setter every frame | **stuck the throttle at full** and glitched the gears |

A 50 ms poll loses every frame; a 94 kHz tight loop held `veh+0x39FC` 99.9% of
the time and still didn't change the car. Persistence is not causation — three
separate offsets held under write and did nothing.

## No vanilla equivalent exists for the player

The vanilla mechanism is real but **AI-only**:

- `FuelConsumptionAndWear_004ed960` case 2 sets `ReasonCode = 2` (Gearbox) and
  raises a byte at `veh+0x39FA`
- `sub_4EF930` tests that byte at `0x4EF934` and skips all shift logic
- but the player never enters `sub_4EF930` — the flag was set and held on the
  player car for 12 s while the driver shifted freely 1→2→1→3

`.PLR` `Mechanical Failures` and `Mechanical failure rate (0-none, 1-normal,
2-timescaled, 3-season)` feed that same AI-only path. Failure categories
(`FailureCategoryNameLookup_004bb730`): 1 Engine, **2 Gearbox**, 3/7 Clutch,
4 Suspension, 5 Brakes, 6 Accident, 8 Electronics, 9 Fuel.

## Verification

Both modes driver-confirmed in-car, 2026-07-30:

- **stuck** — 3867 setter calls, 3862 shift attempts blocked, gear never moved.
  *"locked perfectly at 1."* No throttle or handling side effects.
- **skip** — broken gear 2: 22 redirects, sequence `1,3,1,3,1,3…`, 2nd never
  engaged, a normal `3→4` in the middle proving other gears are untouched.
  *"sounds like a gearbox glitch on each shift."*

## Build

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -o gear_failure.dll gear_failure.c -lkernel32
```

## Commentary — 14 cut clips restored

The player's own gearbox lines are `PWS_MECHPROB20`–`33`. **All 14 ship with
cooldown `FLT_MAX`, i.e. unplayable in vanilla** — same cut-content signature as
PWS_HIT01-06. The DLL clears the cooldown at load (idempotent), and the same
change is registered as patch `gearbox-commentary-unblock` (14 sites).

| eid | sdt | line |
|-----|-----|------|
| 80–86 | 79–85 | Lost 1st … 7th gear |
| 87–93 | 86–92 | Stuck in 1st … 7th gear |

`EID_LOST(g) = 79 + g`, `EID_STUCK(g) = 86 + g`.

Heard in-game 2026-07-30: eid 88 *"stuck in 2nd"*, 89 *"stuck in 3rd"*,
84 *"lost 5th"*. Each had `EVENT_TS = -1.0` beforehand — never played, ever.

### Two eid traps, both live-tested

- **eids 201–217 are the OTHER-DRIVER lines.** The previous README mapped
  stuck/lost to these. Injecting 213 plays *"[driver] is stuck in second gear"* —
  third-person, about another car. That family (`PWS_AIMECHPROB*`, siblings
  `RE_OD_STUCK_7`/`RE_OD_LOST_7`) is correct only for AI.
- **eids 340–383 are the TV channel.** They are unblocked duplicates of these
  very clips and look like a shortcut, but they sit above eid 291 and the channel
  is chosen purely by eid range (1-290 PWS / 292-535 TV). They resolve against
  `PC_TV_MAIN.SDT`. Same trap that cost two sessions on PWS_QUAL28/29/30.

### Injection bugs this replaces

The old script never spoke, for three compounding reasons — all fixed in
`pump_commentary()`:

1. It bailed whenever `CONTROLLER+0xAE4` (`isPlaying`) was set. That flag reads 1
   during ordinary driving and is already logged as false-positive-prone.
2. It confirmed success by reading back the queue head, which drains within one
   poll — so clips that played were reported failed.
3. It set its `fired` flag even when injection returned false, so one busy tick
   killed the line for the rest of the race.

Now: ignore `isPlaying`, retry every 500 ms for **20 s**, and confirm by
`EVENT_TS(eid) > 0`.

Two refinements learned the hard way:

- **Detect with `> 0`, not "changed from a captured value."** The injection
  itself writes `EVENT_TS = 0`, so a retry rewrites the very field the check
  depends on. Only the game writes a session time there, so any positive value
  means it spoke. The old check reported `GAVE UP` on a clip whose `EVENT_TS`
  read 5471.146 afterwards — it had played.
- **20 s, not 8 s.** While racing the queue is busy with other commentary
  (eids 123/114/222 were observed competing), and an injected clip can be
  displaced for seconds. After the fix the same clip logged
  `PLAYED after 31ms`.

## Realism model

**A stuck gear is binary; a lost gear is gradual.** Two different phase tables:

| | phase 1 | phase 2 | phase 3 |
|---|---|---|---|
| stuck | 20% of shifts refused | **100%** | 100% |
| lost | 25% | 60% | 100% |

A jammed selector either holds or it doesn't — rolling a dice on every shift
forever reads as random glitching, not a failure. Worn dog rings genuinely do
fail intermittently first, so the lost-gear ramp stays gradual. Default
escalation is 10 s per phase.

**A stuck fault may only ever refuse a change, never cause one.** It substitutes
the *current* gear, which triggers the setter's own `newGear == current → return`
path. Substituting the gear captured at arm time was a real bug: once one shift
got through during the warning phase the value was stale, so the setter actively
engaged it and yanked the car down a gear.

**A lost gear costs time.** The first grab spits into neutral; only a second
attempt within 1.5 s forces the next working gear. Without that, `1→3` was as
quick as `1→2` and the "failure" was strictly an advantage.

**Refused shifts make a noise.** The stuck path returns before both of the
setter's sound calls, so a refusal was completely silent and read as a dropped
input. The mod fires the down/neutral sound `0x4F89A0` on every refusal.

### Radio timing

The engineer speaks **only once the box has actually failed**, then after a
2.5 s delay, naming the gear at that moment.

Announcing on ARM told the driver about a fault before it had happened.
Announcing on the first refusal was also wrong — a warning-phase miss is not the
failure, and it named *"stuck in 5th"* while the box actually jammed in 6th ten
seconds later. Symptom → HUD → real failure → radio.

### HUD

Uses the game's own message strip (the one that shows `RED FLAG`), mechanism
from `mods/red_flag/gr29.c`:

```
inner = *(u32*)0x7CD204     8 slots, stride 0x5C
text  = slot + 0x3C         ASCII, max 31 chars
timer = slot + 0x90         float; write 1.5 and re-arm each tick
```

Take a slot whose timer is `<= 0`. The line fades on its own, so nothing needs
cleaning up if the DLL is ejected mid-race. Messages are short and CAPS:
`GEARBOX FAILURE`, `GEARBOX FAILING`, `GEARBOX CRITICAL`, `NO DRIVE`.

Two dead ends, both checked first: `TimerDisplayRender` (0x4D9DD0) is
grid/classification only and never runs in-race, and calling
`UITextRenderWithScale` (0x4D7E40) directly from a `DrawDebugTextOverlay` hook
drew 3285 frames with nothing visible.

## Not done yet

- **11 of the 14 clips are reachable but not individually ear-checked** — only
  3 were heard. They are `WIP` in `progress.json` with a note; use the GUI's
  *Test line* button, then flip to DONE.
- **Automatic triggering.** Currently manual via the GUI. Lap/time/random
  triggers still to come.
- `mechanical_failures.py` is the superseded poll-based script, kept only as
  the record of the disproved approach and marked DEPRECATED in its own header.

---

## Engine failures (`engine_failure.dll`)

Full map: [`docs/ENGINE_FAILURE.md`](../../docs/ENGINE_FAILURE.md). Summary only
here.

The `.PLR` "Mechanical failure rate" timer array (`veh+0x3D70/74/78/7C`) is
**AI-only** — the player never enters `FuelConsumptionAndWear_004ed960`
(Interceptor: 105 calls, 21 distinct `this`, zero for slot 0). The player's own
engine failure is `EngineStressSimulation_0043ef20`, `this = 0x7C7428`.

| address | field | role |
|---|---|---|
| `0x7C91DC` | life, double | ticks down; at ≤0 sets ReasonCode 1, rolls an emitter, re-arms. **No commentary** — see below |
| `0x7C91D4` | stress, double | **the power loss** — multiplies torque by `(1 − v36²·stress)`, cap 0.6 |
| `0x7C920C` | bank 1 | WHITE SMOKE |
| `0x7C9214` | bank 2 | BLACK SMOKE |
| `0x7C921C` | bank 3 | FLAMES |
| `0x7C9081` | running byte | **the ignition flag** — 0 stops the engine, 1 restarts it |

Two things that are easy to get wrong:

- **Power and smoke are separate channels.** Stress drives the misfire; the
  banks only drive particles. A stage-one failure smokes while driving fine.
- **Healthy banks are `0.0`, not `1.0`.** A positive bank is used directly as an
  emission rate, so "repairing" to 1.0 leaves the car smoking permanently.

### Independent re-validation, 2026-07-31

Every claim above was re-tested from scratch against a live session, because
corpus root `docs/46-player-specific-mechanical-failures.md` had been written
with a different (wrong) offset base and contradicted this folder on four
points. Results, all driver-confirmed:

| Test | Result |
|---|---|
| `Interceptor` on `0x43EF20`, 5 s | 2001 calls, **one** distinct `ecx` = `0x7C7428`. `this` is the static object, confirmed independently of the 2026-07-30 run |
| `0x7C91DC = 1.5` | drained 1.5 → 0.08 in 5 s, then life 120.78 / stress 0.1415 / bank1 **−0.3611** / ReasonCode 0→1. Driver: white smoke, slower on straights |
| **`0x7C91D4 = 0.0`, banks untouched** | Driver: *"Smoke still there, going faster now"* — **the A/B that proves stress is power and banks are cosmetic.** Stress stayed latched at 0.0 for 10 s |
| `0x7C91D4 = 0.6` | life drain 0.635/s vs 0.28/s at stress 0.1415 vs 0.14/s idle — **stress feeds back into life drain ~1:1** |
| `Interceptor` on `0x41AC50` across the whole failure | **0 calls.** Driver: no radio line |

Three corrections this produced:

1. **`EngineStressSimulation` never calls `sub_41AC50`.** The earlier "it calls
   the emitter itself, so the radio line needs no unblocking" is DISPROVEN. A
   player engine-failure radio line needs a DLL emitter.
2. **The life drain is ~0.14–0.64/s, not the folded `(2.0+2.0)*0.5`.** Doc 46's
   "≈2–2.5/s" is wrong the same way.
3. **`flt_8AAE74` is a 256-entry uniform-random table in [0,1]** and
   `dbl_8AFEA0` is `SessionTime`, so the misfire index is
   `11.5·(rpm/maxRPM)·SessionTime` — flutter frequency scales with revs
   (~11.5 Hz near the limiter). Mean torque loss `0.345·stress`, peak
   `0.997·stress`.

Also fixed: **`tools/poke.py` could not write doubles at all** (`'f' not in fmt`
routed `'<d'` through `int()`), which made a real write look like a poke that did
nothing. Every field in this table is a double. Fixed at corpus root.

### Control

The GUI (`tools/mechanical_failure_gui.py`, launched by `mechanical_failure_gui.bat`)
now has two tabs — **Gearbox** (shared block `F1C_GEARFAIL_CTL`) and **Engine**
(shared block `F1C_ENGFAIL_CTL`). The Engine tab writes the same intents as the
hotkeys below, edge-triggered through the shared block, and shows a live
life/stress/torque/rpm readout. Hotkeys still work while the GUI is connected.

| Key | Action |
|---|---|
| `Num3` / `Num4` / `Num5` | engine stage-one (smoke) / terminal (ramp to cap) / instant (stress 0.6) |
| `Num6` | **seizure** — binary stop, the ignition byte |
| `Num7` | **disabled** — suspension, removed after it crashed the game |
| `Num8` / `Num9` / `Num0` | state dump / telemetry HUD / repair all |
| `1` / `2` / `3` / `4` | toggle white smoke / black smoke / fire / all off |

`Num1` and `Num2` are deliberately unbound — they are red-flag scripts baked into
the exe. The top-row digits are the stock camera binds and are not swallowed, so
they also change camera unless cleared in the `.PLR`.

HUD messages use the game's own message strip (`0x7CD204`), re-armed each tick.
Self-eject: `python tools/dll_unload_signal.py F1C_ENGFAIL_UNLOAD`.

### Build

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o engine_failure.dll engine_failure.c -lkernel32
```

### Cutout and oil leak (`C` / `O`, 2026-08-07)

Two mod-authored glitches, no vanilla equivalent, both GUI-tracked (Engine
tab, `Glitches` section):

- **Cutout (`C`)** — reuses the FACT ignition byte `ENG_RUNNING` (0x7C9081),
  but flicks it off for a short random burst (80-280ms) on a random interval
  (0.8-3s) instead of holding it off. Stutters, doesn't kill the engine.
- **Oil leak (`O`)** — a pressure gauge modelled entirely in the DLL (F1C has
  no live oil-pressure telemetry, only a durability/thermal bracket unrelated
  to any trigger). Drains 100→0 over 60s: blue smoke density rises the whole
  way (reuses bluesmoke.dll's proven `ParticleTrailEmitter` path, duplicated
  here so the DLL stays self-contained), `ENG_STRESS` also climbs past 50%
  pressure, real seizure at 0%. Fires `PWS_MECHANWARN0` (eid **130**, not
  131 - see `findings.jsonl` `pws-mechanwarn0-eid130-confirmed-not-131`) once
  at the 50% mark, isolation-tested and driver-confirmed correct.
- **Engine-death commentary** — `PWS_RETIRE04` (eid **119**, not 120 - see
  `findings.jsonl` `pws-retire04-eid119-confirmed-not-120`) fires once on any
  `ReasonCode==1` death (oil-leak seizure, `Num6` manual seizure, or the
  vanilla life system's own death) once the car has actually coasted to a
  stop (`retire_commentary_pump()`, ports the proven wait-for-stop state
  machine from `dll/retire_commentary.c` rather than firing on the
  `ReasonCode` transition alone - that naive approach is that file's own
  documented v3 failure, "announced a retirement at 88 km/h with a drivable
  car").

### Not done yet (engine)

- **Suspension is FIXED — see [§ Suspension failures](#suspension-failures-suspension_failuredll) above.** The 2026-07-30 removal note below is history: writing the per-wheel bend fields directly did nothing and crashed; the proper hunt found the control input is the game's own call `SteeringGeometryUpdate(0x7C7428, wheel, severity)`, now driven from `suspension_failure.dll` (live-proven 2026-08-08).
- **Blue smoke is cut content, and the driver genuinely does not exist (not
  just unwired) — checked 2026-08-07.** `ParticleCollisionEffect` (`0x4CE930`)
  is three copy-pasted hardcoded blocks keyed on `this[8]/[9]/[10]`, no loop,
  no id parameter — compiled for exactly WHITE/BLACK/FLAMES, no 4th slot to
  claim (confirmed no spare struct field exists either: `this+7676` is reused
  by the very next statement in the caller). Restoring it for real needs
  either a binary patch to that function plus new struct storage (high blast
  radius, not attempted), or bypass the vehicle's 3-slot system: add
  `Reaction=bluesmoke Tex=smokewhite.tga Color=0x80aaccff ...` to
  `terrain.ini` `[LOOSE]` (no blue texture ships — commonmaps.mas has only
  SMOKEWHITE/SMOKEBLACK/SMOKEMARBLES/SMOKEWHITELOW/FIRE, tint reused white via
  `Color=`), then call `ParticleTrailEmitter` (`0x4CBC50`) directly from the
  DLL's own tick with the resulting id==2 descriptor pointer, found by walking
  the global reaction table at `0x8ACEA0` (`SpecialFX_Init` owner, array base
  `*(0x8ACEA0+20)`, count `*(0x8ACEA0+16)`, stride 108, id at `entry+4`) —
  DERIVED, not live-verified; see `findings.jsonl`
  `bluesmoke-no-vanilla-driver-emitter-hardcoded-3slot` and
  `bluesmoke-reaction-table-global-and-direct-emit-path`. Oil temperature
  (`OptimumOilTemp`/`LifetimeOilTemp`) is a real but **entirely unconnected**
  system — nothing in code reads it to drive any emitter, so using it as a
  trigger would be a pure mod-authored choice, not a restoration.
- **Not registered in `build_pack.py`'s `FEATURES`** — it restores no commentary
  clips, so the clip-oriented pack schema does not fit it. Revisit if the pack
  ever ships non-clip features.
- `0x7C90C4` is **not** engine RPM despite the label used while building this;
  recorded OPEN in `findings.jsonl`.

## Related

- `findings.jsonl` — 20 entries from the 2026-07-30 gearbox session (incl. one
  RETRACTED and two CORRECTION rows), plus 15 from the engine/ignition session
- `docs/ENGINE_FAILURE.md` — the full engine map
- **corpus root** `docs/IGNITION_INVESTIGATION.md` — superseded; ignition is
  `0x7C9081` (a SOLVED header was added there pointing here)
- `docs/SUSPENSION_DAMAGE.md` (this mod) — carries a read-vs-write warning header
- `functions/PositionLapStateUpdate_00447e00.c`, `functions/sub_4EF930_004ef930.c`
- `functions/FuelConsumptionAndWear_004ed960.c` — the AI wear/failure system
- `functions/EngineStressSimulation_0043ef20.c` — the player equivalent

---

## Suspension failures (`suspension_failure.dll`)

Shipped 2026-08-08, live-proven end to end. Gives the **player** a staged
suspension failure by driving the game's **own** bend input — the exact call
the game makes on a wheel impact — instead of poking the bend fields (which
is DISPROVEN: did nothing, then crashed, Num7 removed 2026-07-30).

### The input the game itself writes

```
CollisionSparkParticleEffects_004ec1a0 line 515:
    if (v89 > this+3965)                                  /* impulse test       */
        if (v89 <= this+3966 || random < this+3967)
            if (this[3247] == 0)                          /* veh+0x32C8 role 0  */
                SteeringGeometryUpdate(&dbl_7C7428, wheel, v108);   /* PLAYER ONLY */
            else RepairServiceStateDispatcher(wheel);      /* full wheel damage  */
```

`SteeringGeometryUpdate_00442060` (the misnamed "LifetimeClampUpperBound"
owner — the ±10° clamp is the bend clamp, see `docs/SUSPENSION_DAMAGE.md`) is
the **only writer** of the player's per-wheel bend state:

| field | role |
|---|---|
| `0x7C7428 + 1124*w + 1105` | bent flag (BYTE1 of dword +276; +1104 is the contact flag) |
| `+1116` / `+1124` | bend-angle doubles, clamped to **exactly ±10°** (0.1745329251994329) |
| `+1132` | accumulator; `> 0.015` → `veh+0x4068 ReasonCode = 4` (Suspension), guarded by `if (!ReasonCode)` |
| `veh+0x4068` | ReasonCode 4 |

It is **player-only by construction** (role gate `this[3247] == 0`), which is
why AI suspension damage uses the separate `veh+0x3D7C` timer.

### How the DLL uses it

- **Detour** on `VehicleMainPhysicsUpdate_004441f0` (+0, 6-byte push-imm/ret
  because the prologue has no 5-byte whole-instruction boundary; 12 stolen
  bytes, resume at +0x0C). That is the player's per-frame physics tick (calls
  Gearbox_00443b70 and PositionLapStateUpdate_00447e00).
- While armed, the detour calls `SteeringGeometryUpdate(0x7C7428, wheel, sev)`
  at ~20 Hz — **on the game thread**, the same context the game uses one frame
  deeper. A worker-thread call is exactly the crash class of 2026-07-30.
- **Stages** = applications × severity; the game's own clamp and trip do the
  rest:

  | stage | HUD | applications × severity | resulting bend |
  |---|---|---|---|
  | 1 FAILING | `SUSP FAILING` | 2 × 0.30 | ~1-3° per corner |
  | 2 FAILURE | `SUSP FAILURE` | 3 × 0.60 | ~5-7° (retuned 2026-08-08; was 8 × 0.60 = straight to the clamp) |
  | 3 COLLAPSED | `SUSP COLLAPSED` | 30 × 1.00 | at the ±10° clamp |

- **Radio**: `PWS_MECHPROB16` "Damaged suspension - avoid kerbs" (eid **75**,
  score 73, cooldown 15, unblocked — not the TV twins 345/348) once per arm,
  confirmed by `EVENT_TS > 0` with the 20 s retry pump.
- **Repair**: zero angles/accumulators/flags, clear ReasonCode — restore
  proven clean live.

### Live proof (2026-08-08, qualifying, stage 1 on RL+RR)

| wheel | bend1 | bent flag | ReasonCode | radio |
|---|---|---|---|---|
| RL | **−0.0377 rad (−2.16°)** | 1 | 0→4 on application | eid 75 **PLAYED** |
| RR | **+0.0496 rad (+2.84°)** | 1 | (drifted back to 0 later — the game re-arms it; treat as trigger, not state) | |

Same configuration as the organic crash of 2026-07-29 (RL −1.927°, RR
+3.323°). Repair restored all fields to 0.

### Driver-confirmed handling effect (2026-08-08, test day)

The bend is **real geometry, not dead telemetry** — the open question is
closed. Driver reports:

- **Stage 1** (RL −1.36°, RR +2.85°): mild, drivable, car slightly unsettled.
- **Stage 2** (original 8 × 0.60): both rears slammed to the **±10° clamp**
  (RL −0.174533, RR +0.174533) — *"The car tries to turn left"*, *"took me
  off road, nearly impossible to drive"*. This proved the mechanism beyond
  doubt and sent the stage back for a retune.
- **Stage 2 retuned** (3 × 0.60): RL −6.57°, RR +5.80° — mid-range, strong
  pull but driveable. The escalation now reads FAILING → FAILURE →
  COLLAPSED instead of a cliff.

Each 0.60 application adds up to ~3° per axis in a random direction, so the
application count is the severity knob; the game's own ±10° clamp bounds it.

### Control

- `Num7` cycles FAILING → FAILURE → COLLAPSED → off.
- `Num8` cycles which wheels get bent: **REAR → FRONT → ALL** (added
  2026-08-08; publishes the choice straight back into the ctl block so the
  worker's change-detect cannot re-adopt the stale GUI value — the bug that
  first made the selector snap back to REAR). NOTE: `Num8` collides with
  `engine_failure.dll`'s state-dump hotkey when both DLLs are injected —
  use the GUI for one of the two in that case.
- GUI: **Suspension** tab in `tools/mechanical_failure_gui.py`, shared block
  `F1C_SUSFAIL_CTL` (stage buttons, wheel checkboxes, per-corner bend/accum
  live readout, REPAIR, dump, inject/unload). Wheel mask ownership: the DLL
  owns it; a GUI checkbox set is change-detected and adopted.
- Self-eject: `python tools/dll_unload_signal.py F1C_SUSFAIL_UNLOAD`.

### Build

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o suspension_failure.dll suspension_failure.c -lkernel32
```

### Traps already settled

- `VehicleContactForceAccumulation_004ebc60` is **event-driven** (contact
  only), not per-frame — the first detour there never fired on demand. The
  per-frame player site is `VehicleMainPhysicsUpdate_004441f0`.
- The bent flag is at **+1105**, not +1104 (that byte is the contact flag) —
  reading +1104 while the corner is bent reports 0.
- Do NOT write the bend fields directly (2026-07-30 crash). Call the function.

### Not done yet

- Stage 3 (terminal, all four corners at the clamp) driver-tested only
  implicitly (stage-2-at-clamp proved that severity); the front-wheel
  selector (`Num8` → FRONT/ALL) is built and the fix is in, but a front-only
  bend has not been driven yet.
- **Artifact hygiene (status 2026-08-14):** `suspension_failure_v5.c/.dll`
  (2026-08-14, includes the auto-reset fix) is the current build, and the
  canonical `suspension_failure.dll` here is STILL the pre-retune 13:21
  build. Do NOT rebuild over the canonical name while old copies are mapped
  (a double-injection leaves the file locked). After the next game restart:
  rebuild v5 over the canonical name, then retire the `_v2`/`_v3`/`_v4`
  artifacts. The canonical SOURCE going forward is the sandbox port
  `mods/sdk_sandbox/suspension_failure/` (buildable via
  `python tools/f1c_mod.py build mods/sdk_sandbox/suspension_failure`,
  auto-reset included) — promote work there, keep this tree as evidence.

## Related

- `findings.jsonl` — 20 entries from the 2026-07-30 gearbox session (incl. one
  RETRACTED and two CORRECTION rows), plus 15 from the engine/ignition session
- `docs/ENGINE_FAILURE.md` — the full engine map
- **corpus root** `docs/IGNITION_INVESTIGATION.md` — superseded; ignition is
  `0x7C9081` (a SOLVED header was added there pointing here)
- `docs/SUSPENSION_DAMAGE.md` (this mod) — carries a read-vs-write warning header
- `functions/PositionLapStateUpdate_00447e00.c`, `functions/sub_4EF930_004ef930.c`
- `functions/FuelConsumptionAndWear_004ed960.c` — the AI wear/failure system
- `functions/EngineStressSimulation_0043ef20.c` — the player equivalent
