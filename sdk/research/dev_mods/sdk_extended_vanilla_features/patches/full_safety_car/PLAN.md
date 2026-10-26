# Full Safety Car — F1C port plan

Status: **DRAFT.** Integration points below are listed as *to locate*, not
located. Nothing in this plan is a manifest byte-patch — the whole feature is
per-frame logic and ships as DLLs (see `patches/mechanical_failures/` for the
established pattern: `dll/*.c`, tick-hook, self-eject).

## Phase 0 — locate the F1C integration points (static, then live)

Map these NT2004 anchors onto F1C before writing any DLL. Each needs a
corpus-grounding (`.c` file + function) and then a live poke/watch gate.

| NT2004 anchor | role | F1C target to find |
|---|---|---|
| `sub_4D0D00`/`sub_4D0D50` | command-queue enqueue/dispatcher (208-byte ring, `dword_7D3C44`) | **RESOLVED — `sub_424CD0` (`AICommandMessageQueueDispatcher_00424cd0`)**, but per-car + synchronous, NOT a global ring. Buffer `veh+0x44A8`, capacity `+0x44AC`, index `+0x44B0`; commands `0/1/4/5/14/15`. **F1C has no session-wide bus** — only §7's per-car layer ports onto it (`DESIGN.md` §3) |
| `dword_75CBB0` | race phase global (5=racing, 7=caution), setter `sub_42F6C0` | F1C session-phase field. NT2004's is `this[2]` of one session-state struct at `0x75CBA8` — look for F1C's equivalent **object**, not five loose globals (live: watch during warmup→race→yellow) |
| `sub_4F99C0` + `this+0x118` | steward flag state + setter | F1C steward object + any existing flag-state field (F1C raises yellows already — find the writer) |
| `sub_4F93E0` | incident scanner (rotating car window) | F1C per-frame session tick (candidate: `MainGameTickUpdate`, used by several EVF emitters) |
| `sub_4FA900`/`sub_4F8D30` | field ordering + per-car flag mode | F1C per-car fields for flag mode / pace target (probe: which veh+N holds a yellow-flag reaction state) |
| `sub_4E29B0`+`sub_42E180` | pace car roster slot 48 | F1C dynman field setup + driver roster (Maylander already exists; find name-field offset + slot assigner) |
| `sub_402C00` line 274 | `.aiw` `PaceCarReleaseDist` parser | F1C `.aiw` parser = **`sub_402690`** (AIDatabase_Init; tokens `wp_ptrs`, `lap_length`, `sector_1_length` match NT2004's `sub_402C00` set; `PaceCarReleaseDist` absent — add it here) |
| `sub_4CE720` | aid bit 11 = auto pace lap | F1C session aid-mask site (F1C has auto-pit aid — auto-pace would sit adjacent) |

## Phase 1 — prove the pipe: per-car command round-trip

**Rescoped 2026-07-31.** The original goal ("prove an NT2004-style *flag* command
can ride the F1C queue") is void: F1C's queue is per-car and carries no
session-wide commands. Nothing to round-trip a flag on. New goal: prove
**per-car command injection** works and holds where a poke did not.

### RESULT — Phase 1 executed live 2026-07-31, gates PASSED, premise CHANGED

Run live via Frida on a stable race (`dword_7C56A8 == 5`, SessionTime advancing
1:1 with wall clock), target = last-place AI car, player untouched.

- **Injection works.** `sub_424CD0(veh, 14, 0)` as `__thiscall` sets
  `veh+0x443C = 1` **synchronously** — read inside the same callback right after
  the call returns, `pre: 0 → after: 1`, 3 of 3 reproductions. Queue slot 0 holds
  `14`, index self-resets to `-1`. Live queue struct: heap buffer at `+0x44A8`,
  **capacity 10**, idle index `-1`.
- **Persistence gate: PASSED.** `0x443C` is not overwritten per frame. It held
  **4734 ms across 190 `AIRacingLine` calls** before being consumed. Root
  `CLAUDE.md`'s "`0x443C` recalc every frame" is disproven for this offset (that
  claim's only evidence, `AIAbility` ±0.01/frame, concerns `0x453C`).
- **Behaviour gate: PASSED, but the lever is not what this plan assumed.**
  `0x443C` is a **one-shot recompute request**, not a following mode. Sole
  consumer is `AIRacingLine_00425920:71`, which computes the 2D gap to the object
  at `veh+0x4400`, feeds `AerodynamicForceCalc`, stores the distance at
  `veh+0x4524`, and **clears the flag on every exit path**. Caught in the act:
  `0x4524` went `0.00 → 151.64` at the moment the flag cleared. Driver also
  reported the target car braking on the straight (one sighting, DERIVED).
- **`oph.c`'s name `SafetyCarFollow` is wrong** and should not be reused.

**Consequence: cmd `0xe` is not a speed governor and holds nothing.** It cannot
pin a car behind another. This does not break Phase 1 — the pipe works — but it
removes the lever Phase 5 was expected to use. Phase 5 must find a sustained
per-car speed/target field of its own, or hook the writer, before it is planned
in detail.

**BUILD RULE (hazard, static-proven).** `CommandQueueResponseHandler_00429ee0`
runs race-phase branches on *every* invocation regardless of the command sent
(`dword_7C56A8 == 3` → `SessionCleanupFlagClear` + `WrapPlayerCallback`;
`== 4` → `AISpeedDeltaSetter(0)`, `ParameterValueSetter(99999)`). Since
`sub_424CD0` always calls it, **every call site must gate on
`dword_7C56A8 == 5` first.** A race ended unexpectedly during out-of-band
calling in the same session — causation not established, but the hazard is real
in the decompiled source and costs nothing to guard.

**Method notes for whoever repeats this:**
- Call `sub_424CD0`; do **not** hand-write the queue array as `oph.c:340-369`
  does. The function fires only ~15 Hz and event-driven (181 natural calls in
  12 s, all cmd `1`, slots 1-2 only), so a hand-written entry waits an
  unpredictable time to drain.
- Invoke it from inside an `Interceptor` callback on a hot function so it runs on
  the **game thread**, not Frida's.
- **Verify the game is actually unpaused** by checking `SessionTime` (`0x8AFEA0`,
  double) advances. Three latch measurements in this session were discarded as
  contaminated — one overlapped a pause, two straddled a race restart. A hot
  function such as `0x43EF20` keeps ticking through a pause and will not warn you.

## Phase 2 — caution state and steward flags

### RESULT — BUILT 2026-08-01, NOT YET DRIVEN. The premise was wrong.

**F1C does have flags.** This plan and `DESIGN.md` §12 both said it has "no
yellow/pace/caution strings at all" and that the flag would have to be a DLL
copy. That is false for yellows: F1C carries **29 marshalling zones** on the
Steward (`0x8ACEE0 +0x110`), rebuilt every frame by `sub_4D1730` from a real
minimum-speed test, diffed against its own previous copy, and announced through
`sub_41B3C0` — which is the game's **flag event dispatcher**, misnamed
`PitSelectionStateMachine`, with `a2` = 0 cleared / 2 raised / 3 red. It was
solved on 2026-07-25 in `investigations/yellow_flag/` and this project did not
look before concluding it was absent.

Built as a result:

- **The watcher is a read of the game's own verdict**, not a reimplementation
  of NT2004's `sub_4F93E0` incident scanner. Auto-declaration on a zone yellow
  held 3 s, OFF by default.
- **The flag display is forced** with one aligned 32-bit store at
  `ShortComputation_bad0` (`0x0049BAD0`), verified before writing and restored
  on green, on release, and on eject.
- Caution laps counted from the leader, with the recall gated on the queue
  being FORMED — the F1 form of NT2004's withholding-green inspection.

Still open: penalties, safety-car lights, and the config plumbing.

### Original note, 2026-07-31 F1C has no caution concept (zero yellow/pace/caution
strings), so the state is DLL-side. Built so far in `dll/pace_probe.c`:

- A two-phase caution state — `FORMING` while the field closes up, `FORMED` once
  four fifths of the running cars are on their slot (wave-around cars excluded
  so they cannot drag it backwards).
- An on-screen banner through the game's own message strip: `SAFETY CAR`,
  `SAFETY CAR - CLOSE UP`, `SAFETY CAR - STAY IN LINE`, `GREEN FLAG`.

**Not** built: closing the pits — rejected, see Phase 3's note. Still open: the
real yellow flag (F1C raises yellows on incidents; the writer is unlocated), the
caution *decision* (what declares a caution automatically), and the session-state
object that NT2004 keeps at `0x75CBA8`.

### NT2004 reference for the rest

1. Port `sub_4F99C0` (flag setter) + `sub_4F9920` (queue post) semantics onto
   the F1C steward; reuse F1C's existing yellow-raise writer so the flag is
   real, not a copy.
2. Port `sub_4F9670` (caution decision machine) as a per-frame call from the
   F1C session tick. Full-vs-quickie (`sub_4F95D0`) optional first pass —
   quickie-only is enough to test.
3. Gate: forced flag → yellow shown, pits closed for all cars, field speed
   drops (measure via existing `tools/race_monitor.py` / poke watchers).

## Phase 3 — field bunching

### RESULT — PASSED live 2026-07-31. Built as `dll/pace_probe.c`.

**The red-flag waypoint route was not needed.** Bunching falls out of the same
per-car speed lever as the pace hold: give each car its own cap and the field
assembles itself. Full design in `PACE_PROBE.md`.

Measured: a field strung out over **2.9 km assembled into a ~20 m queue in about
80 s**, ending with every moving car on its assigned slot, all holding the
commanded pace, zero game overrides. Driver: *"Grouped properly now, lapped
drivers following dutifully."*

The control law, applied per car every 50 ms:

```
k    = number of governed, moving cars with a better race position
err  = (metres behind leader) − k × target_gap
cap  = pace + 0.25 × err,  clamped to [0.55 × pace, pace + 35]
```

**Ordering by race position is required.** The first working version had each
car chase whatever was physically nearest ahead; it bunched the field tightly
and in arbitrary order, with a lapped car leading. NT2004's `sub_4FA900`
field-ordering + `sub_4F8B20` comparator exist for exactly this reason, and an
earlier note in this project suggesting they might be unnecessary is retracted.

1. ~~Wire the existing red-flag waypoint-guidance to a caution state~~ — not
   needed; the speed lever alone bunches the field.
2. ~~Add the pace line: leader + field sorted~~ — **done**, slot-based, in race
   order. The leader is currently the race leader; a pace *car* is Phase 4.
3. ~~Gate: field visibly bunches behind a leader and holds order~~ — **PASSED**,
   log-measured and driver-confirmed.

**Beyond the original scope, also built and verified:**

- **No overtaking.** An approach law caps every car at the lowest recent speed of
  the car ahead plus a room-proportional closing allowance, so a pass is
  impossible rather than discouraged, and cars brake from 400 m out instead of
  arriving hot. Backed by the AI's own fields — `veh+0x44F8`
  TrackAggressionEffective → 0.01 and `veh+0x44CC` PassingNorm → 0.9 — so the AI
  stops *trying*, not just failing.
- **Wave-around**, both procedures, toggled live: lapped cars either stay in the
  queue (F1C's own 1999-2002 rule) or are released to full race speed until they
  catch the back of the queue (the modern rule).

**Pit stops stay allowed under caution** — project decision 2026-07-31, closing
the pits was considered and rejected: pitting under a safety car is normal and
strategically real. The failure that looked like a pit problem was the in-lap
bug (`pittingStatus == 1` is still on track), now fixed; see `PACE_PROBE.md`.

## Phase 4 — pace car vehicle

### RESULT — WORKING live 2026-07-31. Full detail in `PACE_PROBE.md`.

F1C already ships the car: `SeasonData/Vehicles/FIA/1999_SafetyCar/Safgpxa.veh`,
`DRIVER="Bernd Maylander"`, Mercedes livery. It joins the grid as a normal
entrant when its `Classes` line matches the season, so no roster work was needed
— only a way to keep it out of the race until called.

**NT2004's arrangement ported.** Its pace car is invisible because it lives in a
separate vehicle list at slot 48 (`sub_4E29B0` → `sub_4EDDB0(i + 48)`). F1C's
equivalent is the active-vehicle array at `0x7C54F0` walked to `VehicleCount`
`0x5C329C` — lift the pointer out, decrement the count, and the car is gone from
all 90 readers at once. Live: **gone from standings and spectate, game stable.**

**The full chain, all of it the game's own code:** starts normally → at green the
DLL arms `veh+0xD58 = 0` + `callToBox = 1` and he drives himself to the pit lane
at the game's 80 km/h limiter → garaged → lifted out of the field → **Home**
restores him, revives him, and he drives out of the pit lane → catch-up →
paces the field. With the four safety-car voice lines.

1. ~~Find F1C's dynman field setup and driver roster~~ — not needed, he is a
   normal entrant resolved BY NAME.
2. ~~Do not copy slot 48~~ — honoured; the F1C equivalent is the array + count.
3. ~~Port `sub_4E29B0` placement + `sub_4EDDB0` slot assignment~~ — replaced by
   the array/count port, which is the same idea in F1C's data structures.
4. ~~Gate: pace car appears and leads the pace line~~ — **PASSED.**

**Three traps recorded** (`findings.jsonl`, and `PACE_PROBE.md` in full):
retirement does not prevent grid placement; `plannedPositionAtStart = 200` is an
out-of-bounds write; and a decremented `VehicleCount` held across a session
change crashes the loader.

## Phase 4 — original plan

1. Find F1C's dynman field setup (`sub_42E180` analog) and driver roster
   (name field, stride).
2. Add Maylander as the pace car. **Do not copy slot 48** — that is NASCAR's
   field size (NT2004 parks the pace car one past a full 48-car grid); F1C's
   grid is ~22-26 of 104 slots, so the slot must be derived from F1C's own field
   count. **Do not copy the `"pace car"` name check either** — that string does
   not exist in F1C (0 hits in `strings_index.json`). F1C's driver is Bernd
   Maylander, resolved from the `.rcd` roster; `mods/safety_car/README.md:66`
   already established "resolved BY NAME, never by slot index" and that walk is
   the one to reuse.
3. Port `sub_4E29B0` placement + `sub_4EDDB0` slot assignment (5 slot fields).
4. Gate: pace car appears on the grid at the derived slot and leads the pace line.

## Phase 5 — per-car pace control

### STEP 0 — RESOLVED LIVE 2026-07-31: the lever exists

Phase 1 proved cmd `0xe` / `veh+0x443C` is a one-shot request that cannot hold a
car at a commanded speed, so this phase needed its own sustained lever. **Found
and proven:**

```c
void __thiscall sub_42ACD0(float *this, int a2)   // ParameterValueSetter_0042acd0
{
  if ( a2 == 18 ) this[4430] = flt_8ACFB4;        // 4430*4 = 17720 = veh+0x4538 maxSpeed
  else            this[4430] = (float)a2;
}
```

Live on a racing AI car (phase 5 verified, SessionTime verified advancing):
`maxSpeed 99999 → 30` on call, **held 30 for all 8 s sampled**, **actual speed
clamped to exactly 30.0 m/s** on 7 of 8 one-second samples (car had been doing
24.5 m/s). `sub_42ACD0(veh, 99999)` restores cleanly.

This is the game's own slow-down lever — `CommandQueueResponseHandler_00429ee0`
calls `ParameterValueSetter(18)` on cmds 0 and 4 and `(99999)` on cmd 5.
`a2 == 18` is special-cased to `flt_8ACFB4` (`IdleSpeedTarget`, DERIVED).

Note `persistence_map.jsonl` marks `0x4538` `SKIPPED_POINTER` — never classified,
because the default `99999.0` is bit pattern `0x47C34F80` and the sweep took it
for a pointer. No `maxSpeed` persistence verdict existed before this test.

**The Phase 5 abort criterion is cleared.** Remaining steps below are now
buildable.

### STEP 1 — RESULT: whole-field hold + release, run live 2026-07-31. GATE PASSED.

Built `dll/pace_probe.c` (self-ejecting, hotkey-driven, `dword_7C56A8 == 5`
gated) and ran it on a live 22-car race. It calls `sub_42ACD0` per car directly
and re-arms at 20 Hz. **Both halves of the Phase 5 gate — "all cars hold pace
speed and order" — are met.**

| measurement | result |
|---|---|
| cars governed at once | **21** (player excluded, pit-lane cars excluded) |
| speed hold at cmd 30 m/s | **lo 29.9 / mean 30.0-30.1 / hi 30.2-30.3** for ~143 s |
| cars at-or-below command | **20/20 moving cars, every one-second sample** |
| **game overrides in steady state** | **ZERO** — the game never re-wrote `maxSpeed` |
| **race positions changed** | **ZERO**, every sample, whole hold |
| release | all 21 restored in one pass, field resumed racing |

Driver, same run: *cars stayed in line, no overtakes seen, deceleration
realistic and gradual.*

**Order preservation comes free from the speed cap.** No pace-line, following
or ordering logic was present. This bears on Phase 3 and on step 1 below:
NT2004's `sub_4FA900` ordering + `sub_4F8B20` comparator may not be needed for
*order*. Two caveats — every car was capped at the same speed so none had the
pace to pass, and this is order-*preservation*, not gap-closing. **Bunching the
field up behind a leader is still unproven.**

Three lever properties learned that any later module must respect:

1. **The clamp is exact at 30 m/s but has a floor.** Commanded 5 m/s, the field
   settled at 6.1-7.7. Something below ~7 m/s is the AI throttle controller's
   own minimum. Pace speeds well above it are safe; a "crawl" is not available
   this way.
2. **`maxSpeed` is SHARED with the pit limiter** — `flt_8ACFB4` = 22.2222 m/s =
   80 km/h, and the game sets it itself on cars in the pit lane (observed live
   on two cars, probe idle). Never govern a car with `pittingStatus != 0`, and
   never command `18` (it is substituted for `flt_8ACFB4`). The name
   `IdleSpeedTarget` is a misnomer; it is **PitSpeedLimit**.
3. **A ramp is needed.** Commanding the target directly decelerated the field at
   ~15 m/s² — the whole grid on the brakes at once. `pace_probe.c` ramps the
   command down over 6 s, which is what the driver described as realistic.

### STEP 2 — the queue, the no-passing rule and the wave-around

All built in the same DLL and driver-verified the same day. See Phase 3's RESULT
block and `PACE_PROBE.md`; the per-car target speed NT2004 computes in
`sub_4FB270` is this project's slot controller plus approach law.

Remaining:

1. ~~Port `sub_4FB270` substate machine (0-8) with per-car target speed~~ —
   **done 2026-08-01** as the five-state caution lifecycle `CT_OFF / CT_DEPLOY /
   CT_PACE / CT_IN / CT_RESTART` (pick-up, close-up, wave-around window,
   one-to-go). Built, not yet driven.
2. ~~Port the green release (`sub_4FAFF0` semantics)~~ — **deliberately NOT
   ported.** `sub_4FAFF0` releases the whole field at one instant after a
   5-4-3-2-1 countdown; that is NASCAR. The F1 restart is the leader's: cap
   lifted from everyone at once with the approach law still armed, then each car
   freed as it crosses the start/finish line, green flag and PWS_GREEN01 when
   the leader crosses. Built 2026-08-01, not yet driven.
3. ~~Gate: during caution all cars hold pace speed and order; on release the
   field resumes racing speed.~~ **PASSED, see the RESULT block above.**

## Phase 6 — player control, penalties, HUD, config

1. Manual pace: `sub_4FB930` substates + "Press SPACE To Skip Pace Lap" + 20 s
   auto-takeover (`sub_4BA1A0`).
2. Enforcement: `sub_4DF490` yellow infractions (too slow / illegal pass /
   lapped-field deduction) — port only what F1C's penalty system doesn't
   already cover.
3. HUD: "Under Caution"/"Stay Behind the Pace Car" via F1C's message strip
   (the `RED FLAG` HUD mechanism, proven in `mods/red_flag/gr29.c`).
4. Config: `AllowAutoPace` + aid bit 11 + `.aiw` `PaceCarReleaseDist`.

## Stage gates / verify loop

Every stage above ends with the same loop (mirror the EVF methodology):

1. Static grounding first — port a function only after its F1C target is named
   in a `.c` file and cross-checked with `tools/ask.py`.
2. Isolation first — for anything that fires an eid / audio clip, inject that
   eid alone and confirm before wiring (EVF rule, `commentary_inject.py`).
3. Live before build — `tools/poke.py watch` the target fields during a
   forced caution; only then write the DLL.
4. Record results in `findings.jsonl` (`tools/ask.py findings`) — including
   DISPROVEN attempts. The previous fake-SC mod already documented that poking
   `veh+0x453C/0x443C` is useless; don't re-derive it.

## Abort criteria

Stated up front so a failed gate ends the project instead of drifting:

- ~~**Phase 1 gate fails**~~ — **ran 2026-07-31, PASSED** (see the RESULT block in
  Phase 1). The pipe works and the write holds. But `0x443C` turned out to be a
  one-shot request rather than a mode, so the *lever* Phase 5 wanted does not
  exist yet. New gate inherited by Phase 5 below.
- ~~**Phase 5 finds no sustained per-car speed/target field**~~ — **cleared
  2026-07-31**: `sub_42ACD0(veh, N)` holds a commanded speed, proven live (see
  Phase 5 STEP 0), then proven across the whole 21-car field with zero game
  overrides and zero position changes (STEP 1). **Field ordering is no longer a
  Phase 5 risk either** — order held for free. The remaining Phase 5 risk is
  *bunching* (closing the gaps behind a leader), which is Phase 3's problem.
- ~~**Phase 3 gate fails**~~ — **PASSED 2026-07-31**, and the premise was wrong:
  the red-flag waypoint guidance was never needed. The per-car speed lever
  bunches the field on its own.
- ~~**Phase 5 exceeds scope**~~ — Phase 5's speed control is **done and
  verified**. What remains of it (substate sequencing, green release) depends on
  the Phase 2 caution state, not on new speed work.

Remaining risk sits in Phases 2 and 4, which are now the critical path: the
caution *state* (with the flag and closed pits) and the pace car *vehicle*.

## Open questions to resolve in Phase 0

- ~~F1C queue dispatcher~~ — resolved, `sub_424CD0`; commands `0/1/4/5/14/15`,
  no session-wide sub-commands, so NT2004's `0x15/0x16/0x26` have no counterpart
  and must become DLL-side state.
- F1C race-phase field identity and its 7 = caution semantics (NT2004) — find the
  session-state **object** (NT2004's is one struct at `0x75CBA8`, setter
  `sub_42F6C0`).
- F1C steward's existing yellow-raise writer (for the real flag, not a copy).
- F1C `.aiw` parser is `sub_402690` (anchored); whether `PaceCarReleaseDist` needs the
  same [0,1] clamp as NT2004 (`sub_402C00` resets to 0.80/0.87 when out of range).
- NT2004 `dbl_7DA9D0` (race clock) and `dword_75CBAC` bit0 exact runtime
  semantics — confirm live in NT2004 under the same MCP tooling before trusting
  the gates they feed.
- Whether NT2004's `sub_4F93E0` scanner is the only first-caution trigger, or
  whether wreck/debris detection elsewhere also calls `sub_4F9D50`.
