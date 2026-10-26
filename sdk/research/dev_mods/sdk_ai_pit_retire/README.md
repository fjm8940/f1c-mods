# ai_pit_retire

Press **F8** during a race: every AI driver is flagged as pitting **and** retired
(DNF, reason = gearbox). The player car is left untouched.

Per-frame tick DLL. Pure data write — no code patching, no VirtualProtect.

## What it does

On the F8 rising edge, if `SessionType == 7` (race) and `RaceActiveFlag != 0`,
it loops the vehicle array and for each non-player, still-running car:

| Write | Address | Value | Meaning |
|-------|---------|-------|---------|
| pit flag | `0x005F073C + i*0x48C0` | `3` | pitting (writer FUN_004b3400 uses 3) |
| `veh+0x4064` | FinishState | `2` | DNF (0=run 1=fin 2=DNF 3=DSQ) |
| `veh+0x4068` | ReasonCode | `5` | gearbox failure |

Vehicle ptrs from `VehiclePtrArray` (`0x007C54F0`), count from `0x005C329C`,
player skipped via `PlayerVehiclePtr` (`0x007C54E8`). Cars already out
(FinishState != 0) are skipped so events aren't re-posted.

Change the key by editing `#define HOTKEY VK_F8` in `ai_pit_retire.c`.

## Build
```
cd mods/ai_pit_retire
build.bat        # needs MSYS2 mingw32 at C:\msys64\mingw32\bin
```

## Run
1. Start a race and get on track.
2. `inject.exe ai_pit_retire.dll`
3. Press **F8**. Check `ai_pit_retire_log.txt` next to the DLL for the count retired.

## Notes / limits
- Hybrid design: cars are marked pitting + DNF in one shot; they do **not**
  drive a full in-lap first. Scoring/standings show them as retired.
- Non-ASLR binary (base 0x00400000) so absolute VAs are used directly.
- Physics/AI may keep the car object alive visually until the game's own
  retirement cleanup runs off FinishState. Verify in-game.

## SDK
Includes `../sdk/f1c_sdk.h` (`g_VehicleCount`, `g_VehiclePtrArray`,
`g_SessionType`, `g_RaceActiveFlag`). Regenerate with `python tools/gen_sdk.py`.
Inspect live globals with `python tools/poke.py`.

## Session update 2026-07-24 — natural retirement mechanism found + traced live

**The old raw-poke approach (`veh+0x4064`/`0x4068` only) never worked because
`ActiveFlag` (`veh+0x45F4`) is the actual visual kill switch, and it was
never touched.** Full working recipe (IN-GAME CONFIRMED, live-tested
repeatedly on both player-adjacent and AI cars this session):
```
veh+0x4064 FinishState = 2       // DNF
veh+0x4068 ReasonCode  = <any>   // cosmetic, doesn't gate anything
veh+0x4410 RetiredFlag = 1
veh+0x45F4 ActiveFlag  = 0       // <- the real kill switch
veh+0x453C SpeedCap    = 0.35    // optional, slows before the stop
```
See `mods/_README.md`'s reusable-primitives section for the authoritative
copy of this (this file is the detailed trace log behind it).
`retire_driver.py` in this folder implements it plus a scene-node hide
(borrowed from `grid_reorder/gr28.c`'s `hide_dnf_cars`).

**Natural (undisturbed) retirement timing, Frida-style live trace,
confirmed 3x independently (Hakkinen, Alonso, Trulli+Frentzen
simultaneously) via `trace_hakkinen.py`/`trace_all_retirement_cluster.py`:**
```
T+0.0s   ReasonCode 0->6, carConditions 0->2   (collision, same tick)
T+~4-5s  ActiveFlag 1->0                        (visual kill switch)
T+~12-13s FinishState 0->2, RetiredFlag 0->1    (same tick, official classification)
```
Three distinct stages, not instant — a script that forces all 5 fields at
once (as above) works, but skips this natural timing; that's fine for
forcing a result, but if the goal is ever to distinguish "real" vs
"scripted" retirements from telemetry, this timing signature is the tell.

**Command-queue mechanism identified (`AICommandMessageQueueDispatcher_00424cd0.c`,
called by `AIDriverStructInit`):** manages an actual command queue per
vehicle (`this+0x44A8` = buffer of 8-byte `{cmd,param}` pairs, `+0x44AC`
capacity, `+0x44B0` current index). `ActiveFlag` write site found at line 60:
`if (!a2 && this[4121]==2) ActiveFlag=0` — command code 0 processed while
`FinishState`(this+0x4064)==2 clears it. **Contradiction not yet resolved:**
our live trace shows `ActiveFlag` changing BEFORE `FinishState` (~4-5s vs
~12-13s), which is backwards from what that guard implies (`FinishState==2`
must already be true for it to fire). Either this function's `this` isn't
the raw vehicle-struct base we've been assuming (we've hit this exact trap
twice before — `RepairServiceStateDispatcher`, `EngineWearFailureSimulation`),
or `ActiveFlag=0` in the observed natural cascade comes from a different
code path (maybe the `sub_4A46F0` call in case 4/5) than line 60. Don't
assume this is solved — needs a dedicated poke session on `this` vs raw
vehicle base to confirm.

**Field survey around the retirement region (`0x4400-0x4580`), corpus
cross-checked against `skeleton_facts_all.json` (7,138 functions):**

| Offset | Live behavior | Corpus writers | Verdict |
|--------|---------------|-----------------|---------|
| `0x33F4/8/C` | fire once, every collision | (matches prior "hit counter cluster" finding) | confirmed collision hit-counters, not retirement-specific |
| `0x4544` | `1->0` exactly at ActiveFlag-kill tick | `AICommandMessageQueueDispatcher`(init 0, "case 15 reset flag"), `CommandQueueResponseHandler`(init 1) | real, documented — command-queue reset flag |
| `0x4548`/`0x4550` | not traced live yet | same dispatcher, case-15 companions to 0x4544 | worth adding to next trace |
| `0x456C` | one-tick pulse `0->1->0`, same tick as ReasonCode | zero corpus hits | confirmed 3x live — collision-event notification pulse, still unnamed |
| `0x4574` | erratic small-int noise (0,1,4,7,8...) for ~2-4s post-crash | `AISteering` (RW) | steering-recovery internal, NOT a status field — corrects earlier live-only guess |
| `0x4510` | oscillates 0/1(/2) for ~3s post-crash | `FieldZeroSetter`, `SteeringAngleController`, `AIDriverStructInit`, `AIMainUpdate` | steering-state, NOT `driverStatus` as first guessed — car is just spinning |
| `0x4468` | clean countdown `3->2->1->0`, ~1s per step, 3x reproducible | `AIRacingLine`, `AISteering`, two unnamed subs | AI racing-line replan counter after being knocked off line, NOT a collision-severity counter as first guessed |
| `0x4434`/`0x4420` | noisy floats | `AIRacingLine`/`AISteeringCorrection`/`AIAbility`, `OpponentProximityScanTeamRecognition`/`AISteering` | steering/proximity internals, not retirement |
| `0x4470/4/8` | erratic floats | `SteeringAngleController`, `CameraViewportFrustumInitialization` | physics/camera jitter, confirmed noise |
| `0x4484`/`0x4434` | change EVERY frame for EVERY car | `AIDriverStructInit` etc (broad) | pure per-frame telemetry noise, exclude from any future trace |
| `0x4404` | steady `+4.0` per ~4s tick, identical value across different cars at the same tick | `RaceStructInit`, `AIDriverStructInit`, `CameraViewportFrustumInitialization`, `AIMainUpdate` | ambient shared timer (session/race time), not per-car, not retirement |
| `0x447C` | mirrors `0x4404` exactly (same value, same tick) | **zero corpus hits** | despite being "unanalyzed," empirically just a second read of the same clock as 0x4404 — not worth naming separately |
| `0x4534` | `+4.0` per ~4s tick after crash, similar shape to 0x4404 | `PitStopDecisionTireCompound`, `PostPitCleanup`(init 0), `AIDriverStructInit`, `AIAbility` | pit-related timer, plausibly counts toward the eventual pit-classification of the retirement |
| `0x457C` | **`0.97 -> 0.7275`, byte-identical transition, 3/3 cars, exact ActiveFlag-kill tick** | **zero corpus hits** | genuinely new, real, reproducible finding — likely an AI pace/effort multiplier distinct from `SpeedCap` (0x453C). Best lead for further work. |
| `0x450C` | real per-car float, 0.54-0.65 band, fluctuates for a few seconds post-crash | **zero corpus hits** | genuinely unresolved — plausible grip/performance-damage multiplier feeding the steering chaos above, unconfirmed |

**Open items, in priority order:**
1. ~~Resolve the `this`-pointer question~~ **DONE 2026-07-24, mostly.**
   Frida-hooked `AICommandMessageQueueDispatcher`/`CommandQueueResponseHandler`/
   `sub_4A46F0` live during a real Raikkonen crash
   (`tools/frida_trace_ai_commands.js`, broad mode — logs all 21 cars,
   dedup'd on a2-change per car). Confirmed sequence:
   ```
   a2=2, a2=3          RC=0  Active=1  f4544=1   (normal)
   a2=1                RC=6  Active=1  f4544=0   (crash: RC pulses to 6)
   a2=0  <- THE ONE     RC=0  Active=0  f4544=1   (ActiveFlag clears HERE)
   a2=1                RC=0  Active=1  f4544=1   (back to normal)
   ```
   `this` IS the raw vehicle base (confirmed — offsets line up exactly)
   and `ActiveFlag` clears precisely at command code `a2=0`, matching the
   source's bottom dedup-queue block (`if (!a2 && this[4121]==2)
   ActiveFlag=0`), not case 4/5's `sub_4A46F0` as suspected.
   **Anomaly, NOT resolved:** that line requires `this[4121]==2`
   (`FinishState==2`) to also be true, but live `FS=0` at that exact
   moment — contradicts the decompiled guard. Also `ReasonCode` pulses to
   `6` for one tick then resets to `0` *before* the `ActiveFlag` clear,
   suggesting it's a one-shot signal consumed by whatever queues the
   `a2=0` command, not a persisted value the retirement logic reads
   directly. Don't force a clean resolution here — could be decompiler
   index math or a hook-timing subtlety; flag honestly, revisit only if
   it starts mattering for a mod.
2. ~~Confirm `0x457C`'s meaning~~ **DONE 2026-07-24, FACT-tier.** Poked
   `veh+0x457C` to `0.5` on a healthy (non-crashing) AI car (Coulthard,
   slot 3) mid-race — car visibly drove slower, confirmed by direct
   observation. Independently corroborated by `trace_all_retirement_cluster.py`
   catching the exact `0.97<->0.5` flips matching our poke sessions. This is
   a real AI pace multiplier, not a byproduct. Added to SDK as
   `VEHF_AIPaceMultiplier` (`tools/sdk/f1c_sdk.h`). Tool:
   `poke_pace_and_grip.py <slot> pace <value> [seconds]`.
3. ~~Confirm/name `0x450C`~~ **Tested 2026-07-24, negative result.** Poked
   `veh+0x450C` to `0.15` then `5.0` (both well outside the observed
   0.54-0.65 crash-window band) on a healthy car (Coulthard, slot 3),
   15s each, twice. Zero visible effect either time — no handling change,
   no grip loss. Notably the game never contested the write either
   (`before=0.0` both times, unlike `0x457C` which the game actively
   recalculates every frame) — meaning nothing reads/uses this field while
   the car is healthy. Best remaining theory: it's only meaningful *during*
   active damage processing (the transient 0.54-0.65 values only ever seen
   mid-crash), not a persistent grip multiplier — would need a poke *during*
   an actual crash window to test further, not on a healthy car. Deprioritized.
4. Trace `0x4548`/`0x4550` (the other two `AICommandMessageQueueDispatcher`
   case-15 fields) — now included in `trace_all_retirement_cluster.py`'s
   `FIELDS` dict, will show up on the next crash automatically.

## Follow-up 7 — real RetiredFlag/ActiveFlag writer found via static review (no live tools needed)

Went back to `skeleton_facts_all.json` looking for actual writers (RW, not
just R) of `RetiredFlag` (0x4410) — Frida had only ever shown us *readers*
of that field, never the write site. Found it.

**`AIStateReset_0042af40.c`** — literally named "ResetAIStateToRetired" in
its own skeleton fill. Unconditionally, no guards at all:
```c
this[RetiredFlag] = 1;
this[ActiveFlag]  = 0;
```
Both fields, same call, every time. This resolves the old ActiveFlag-vs-
FinishState timing question a different way than expected: `ActiveFlag`
clearing early (via `AICommandMessageQueueDispatcher`'s `a2=0` path, found
via Frida in Follow-up 6) and `RetiredFlag` clearing later (via this
function) are two DIFFERENT writers on two different timelines — this
function just redundantly re-sets `ActiveFlag=0` (already 0 by then, so
our diff-based traces never saw it "change" again).

**Called by `PitCrewStateMachine_004bbfc0.c`** — this is NOT lap-count/pit
logic, it's a **message-dispatch handler**:
```c
if ( (*(int*)a2 & 0xFC0000) == 0x680000 )      // matches incoming event type
    if ( !this[12] )                            // not already processed
    { this[12] = 2; return AIStateReset(*this); }
```
Best read: a severe-collision event message gets posted somewhere (matching
type tag `0x680000`), and this is the handler that commits the retirement
in response, guarded so it only fires once per car (`this[12]` state
marker). This is very likely THE actual "crash bad enough → retire"
trigger for the terminal-damage cascade we've been tracing all session.

**Second, independent trigger path also found — `PitStopBlockingCondition_0042aee0.c`:**
```c
return this[lapNumber] >= dword_8ACF8C   // current lap >= total race laps
    || RacePhase == 8
    || retirementPending (veh+0x39FA)     // nonzero = force it
    || (pit-data-ready && !gate-byte);
```
When this returns true AND `SessionType==7`, `PitStopDecisionTireCompound_0042a3d0.c`
sets `RetiredFlag=1` (separately from `AIStateReset`). This reconciles the
old deleted investigation's negative result on `retirementPending` — it's
a real trigger, just one of four OR'd conditions, and wasn't the one firing
in those specific trials.

**RESOLVED 2026-07-24 — found via hardware data breakpoint (Cheat Engine
MCP bridge), not static analysis.** Static grepping across the whole
corpus (skeleton extraction AND raw `.c` text search) genuinely could not
find this write site because of a `this`-pointer offset trap (see below) —
this needed a live hardware watchpoint on `veh+0x4064` to catch the exact
instruction.

**The writer is `PitCrewStateMachine_004bbfc0` itself** — the same
function documented above as the caller of `AIStateReset`. It's not a
separate mystery function; it's ONE function that commits the entire
retirement in one shot:
```c
if ( (*(int*)a2 & 0xFC0000) == 0x680000 )   // incoming message type match
    if ( !this[12] ) {                       // this[12] == FinishState, not-yet-set guard
        this[12] = 2;                        // FinishState = 2 (DNF) -- the write, confirmed live
        AudioSoundEffectController(v6);
        return AIStateReset(*this);           // RetiredFlag=1, ActiveFlag=0
    }
```
Exact disassembly at the write site (confirmed via hardware watchpoint on
a live Hakkinen crash, hit at `EIP=0x004BBFEA` immediately after):
```
004BBFE3:  C7 46 30 02000000   mov [esi+0x30], 2
004BBFEA:  75 07               jne 004BBFF3
```
**Why static analysis missed it:** `PitCrewStateMachine`'s `this` pointer
is `veh+0x4034`, NOT raw vehicle base — same offset-pointer trap hit
earlier this session with `RepairServiceStateDispatcher` and
`EngineWearFailureSimulation`. Its own `this[12]` (the decompiled source
declares `this` as `_BYTE**`, so index 12 scales by 4 = byte offset
`+0x30`) lands on `(veh+0x4034)+0x30 = veh+0x4064` = `FinishState`
exactly. Confirmed live: `ESI=0x005F988C` at the hit, `+0x30 =
0x005F98BC` = the exact watched address.

**Net conclusion for the whole investigation:** there is no separate,
staggered multi-function retirement pipeline — `PitCrewStateMachine` sets
`FinishState`, calls `AIStateReset` (which sets `RetiredFlag`+`ActiveFlag`)
all in one call, triggered by a single incoming collision-event message
(type tag `0x680000`). The multi-second stagger observed live all session
(`ReasonCode` → ~4-8s → `ActiveFlag` → ~7-9s more → `FinishState`+`RetiredFlag`)
is NOT multiple writers firing on different timelines — it's however long
the message-dispatch system takes to actually queue and deliver that
`0x680000` event after the initial collision. Don't re-open this as a
"why is there a delay" question expecting multiple writer functions;
there's one writer, the delay is upstream of it in message dispatch.

**Tooling note for future sessions:** the Cheat Engine MCP bridge
(`cheatengine-mcp-bridge`, registered as MCP server `cheatengine`) is what
actually cracked this after Frida and an x32dbg MCP bridge both hit
dead ends. `set_data_breakpoint` (hardware, exact address, no page-guard
noise) + `get_breakpoint_hits` (returns full register state + disassembled
instruction at the hit) is the correct tool for "what writes this address"
questions going forward — much cleaner than Interceptor-hooking a guessed
function or software page-guarding a hot page.

## Follow-up 8 — direct field-write retirement confirmed working but leaves car stranded; found the SCRIPTED/historical failure mechanism as the likely real fix

**Direct field write, live-tested via Cheat Engine on a healthy AI car
(Frentzen, slot 9) mid-race:** writing `FinishState=2`, `RetiredFlag=1`,
`ActiveFlag=0` together does stop the car and classifies it DNF — but the
car is stranded wherever it stopped on track, never drives to the pits.
Matches the known, previously-documented limitation (`red_flag_v2`'s "DNF
car visibility problem") — forcing the flags skips whatever process
normally drives the car in first.

**Attempted: call the real trigger function (`PitCrewStateMachine_004bbfc0`)
directly via Cheat Engine's `execute_method`**, constructing a fake event
message matching its type-tag guard (`(*(int*)a2 & 0xFC0000) == 0x680000`).
Call reported success but produced no field changes, and a logging
breakpoint placed at the function's own entry point never fired even
though the call apparently completed — most likely because the injected
call runs on a separate thread the debugger's hardware breakpoint doesn't
cover, not proof the call landed at the wrong address. Real diagnosis
would need single-stepping the injected call specifically; not pursued
further given time already spent. **Lesson for next time:** verify an
injected call's landing with something thread-independent (e.g. a memory
write breakpoint on a field the call is known to touch) rather than an
execution breakpoint, if hardware breakpoints don't reliably apply to
CE's injection thread.

**Real lead, from `.gdb` track files (`SeasonData/Circuits/<Country>/<Track>/<Track>.gdb`):**
each track file has a `RaceEvents` block with a scripted, historically-accurate
DNF schedule:
```
RaceEvents
{
    Gaston Mazzacane   { 1 = DNS }
    Tarso Marques      { 3 = Gearbox }
    Mika Hakkinen      { 25 = Suspension }
    Jarno Trulli       { 38 = Engine }
    Juan Pablo Montoya { 40 = Engine }
    Jenson Button      { 52 = Gearbox }
}
```
(driver name → lap number → failure type). Traced the full live pipeline
that consumes this, end to end:

1. **`sub_424080`** (called by `AIDriverStructInit` at driver setup) parses
   the `.gdb` RaceEvents section for this driver (via
   `StringKeyedLinkedListSearch` keyed by name) into a 5-slot per-driver
   schedule array at `veh+0x4144` (2 dwords/slot: lap number, failure-type
   code 0-6).
2. **`DriverSetupParamLookupByID_0042aeb0`** — given a lap number, searches
   that 5-slot array, returns the paired failure-type code or `-1` if this
   lap has no scheduled event.
3. **`AICollision_004bef60`** (called every frame by `EngineFailureCheck`)
   is the single caller of step 2:
   ```c
   if ( dword_7CEA5C == 3 )     // EngineWearMode == 3
       return DriverSetupParamLookupByID((_DWORD*)a2, *(_DWORD*)(a2+16444)) <= -1 ? 0 : 2;
       //                                              a2+16444 = veh+0x403C = lapNumber
   ```
   When `EngineWearMode==3` and the current lap matches a scheduled entry,
   returns severity `2` — presumably committed into an actual failure by
   `EngineFailureCheck` from there (not yet traced further down).

**Why this never fired organically all session:** live-checked
`dword_7CEA5C` (`EngineWearMode`, global at `0x7CEA5C`) — the current
session has it at `2`, not `3`. The scripted-schedule path is dead code
under the current game config. Mode `2` is (per `EngineWearFailureSimulation`,
documented earlier this project) the pure noise-based random-wear path;
`3` is presumably "scripted/season mode" or similar, not yet confirmed.

**Executed 2026-07-24 — mixed results, real find, real dead end.**

Set `dword_7CEA5C=3` globally and wrote a schedule-array entry
(`veh+0x4144=5, veh+0x4148=1`) for Raikkonen (slot 7, healthy, lap 5 at
the time) matching his then-current lap. No effect — `FinishState`/
`RetiredFlag`/`ActiveFlag` all unchanged even after several seconds
(during which his lap advanced past 5, so the window for that scheduled
entry to match had passed — the check may be edge-triggered on the lap
transition instant rather than continuously true for the whole lap; not
confirmed).

**Traced the full commit logic in `EngineFailureCheck_004bec55.c` while
investigating** (lines ~28-107): confirmed `AICollision(veh, 1.0)`
returning nonzero leads to constructing a message via
`ProgressBarEventEncode(msg, 4, 26, 1, veh+18580, sessionTime, 0)` and
calling `PitCrewStateMachine((BYTE**)(veh+16436), msg)` — i.e.
`veh+0x4034` confirmed again as the `this` pointer (16436 decimal =
0x4034), matching Follow-up 6's Frida-derived hardware-breakpoint finding
exactly. Decoded `ProgressBarEventEncode`'s bit-packing formula precisely:
```
result = a5 + ((a4 + ((a3 + ((SessionType + 8*(a7+4*a2)) << 6)) << 10)) << 8)
```
With the real call's args (a2=4, a3=26, a4=1, a7=0, SessionType=7) this
computes to exactly `0x87680100`, and `0x87680100 & 0xFC0000 == 0x680000`
— confirms `PitCrewStateMachine`'s type-tag guard precisely. Also found
`this[107]` gets written by `ProgressBarEventEncode`, meaning the real
message object is a much larger struct (400+ bytes) than the naive
16-byte buffer used in Follow-up 8's first injection attempt.

**Retried the direct `PitCrewStateMachine` call (Cheat Engine
`execute_method`) with the corrected 512-byte buffer and the exact
`0x87680100` encoding.** Still zero effect — `RetiredFlag`/`ActiveFlag`
unchanged immediately after a reported-successful call, on a target
confirmed healthy (`FinishState==0`, guard should pass). Two increasingly-
correct attempts producing identical silent no-ops is a real signal, not
bad luck — most likely a genuine limitation in the CE bridge's thiscall
injection for this specific pattern (register clobbering or some other
trampoline issue we can't diagnose without visibility into its generated
code). **Not resolved. Don't keep re-trying this exact approach without
new tooling** (e.g. real single-step tracing of the injected call itself,
not just before/after memory snapshots).

**Also worth checking in a future session, not yet done:** the in-game
"Mechanical Failures" menu option (mentioned by corpus owner) almost
certainly writes `dword_7CEA5C` through the game's own proper
initialization path — likely seeds companion state (e.g. the per-car wear
accumulator array `EngineFailureCheck` reads via `*a4`/`a4[104]`, gated
before `AICollision` is even called) that a mid-race raw write to
`dword_7CEA5C` skips. Toggling it via the actual menu before a race starts,
rather than forcing the global mid-race, might be the missing piece for
the scripted `.gdb` RaceEvents path specifically.

**Practical resolution used going forward:** `mechanical_failure.py` in
this folder — the proven direct-field-write recipe (`FinishState=2`,
`ReasonCode`, `RetiredFlag=1`, `ActiveFlag=0`, `SpeedCap=0.35`). Meets the
actual objective (driver suffers a failure and retires) at the cost of
the car not physically driving to the pits — a known, documented,
unsolved cosmetic gap, not a blocker.

## Follow-up 9 — the "hide the DNF car" problem is solved in principle, but the fix crashed the game. Session stopped here.

**Root cause of why the raw scene-node Y-teleport (from Follow-up 8's last
attempt) didn't visually hide Frentzen, found by actually reading
`grid_reorder/gr28.c` in full instead of reusing one function out of
context:** `hide_dnf_cars()` only works as ONE STEP of gr28's full
sequence — it writes `Y=-1000` into the scene node specifically in the
~500ms window right after a red-flag restart (`GAME_STATE==5`), and
critically, gr28 also installs a call-redirect hook on the
`SceneRenderUpdate` call site (`0x0042C409`, inside `DynMan_Restart`) that
makes the game **skip rebuilding that car's model** during the restart.
Without both the restart AND the hook, whatever normally re-syncs a still-
"active-ish" car's render position from physics each frame just
overwrites a one-shot Y write on the very next frame — exactly what we
observed (render Y held, physics Y did not, car stayed visible).

**Correct approach identified: don't replicate this by hand, use the
already-built, IN-GAME CONFIRMED `gr28.dll` directly.** Its full flow
(`snapshot_positions` detects existing DNF cars via `FinishState!=0 ||
RetiredFlag!=0` → red-flag restart → `re_retire_dnf` → `hide_dnf_cars`
with the hook installed) is built for exactly this scenario — force a car
DNF first (`mechanical_failure.py`), then trigger gr28's NUMPAD1 sequence
to have the *real*, already-working mechanism hide it properly.

**Housekeeping found along the way:** `mods/grid_reorder/inject.bat`
pointed at `gr36.dll`, not `gr28.dll` as `mods/_README.md` documents as
current — and `gr36.dll` has no corresponding `.c` source anywhere in
this folder (compiled binary only, source presumably lost in an earlier
cleanup). Also, `mods/red-flag-dll/` (the injector `gr28.c`'s own header
comment references) is gone — reused `mods/ai_pit_retire/inject.exe`
instead, which works identically (generic injector) but prints a stale
hardcoded "press M" message from whatever mod it was originally written
for; ignore that message, `gr28.dll`'s real hotkey is NUMPAD1 per its own
source.

**Result: injected `gr28.dll` successfully, but the game crashed** shortly
after (process confirmed gone). Not diagnosed further — session stopped
here at the corpus owner's direction. **Do not blindly re-inject gr28.dll
next session without first checking why it crashed this time** — possible
causes worth checking first, in rough priority order:
1. `gr28.c`'s own header notes a prior crash-causing bug in `gr27`
   ("stale-hook crash") that gr28 claims to fix — but only for one
   specific case (double-install guard); there may be other stale-hook
   paths, especially since this session's game state was already unusual
   (multiple manual DNF pokes via Cheat Engine on Frentzen, Raikkonen's
   `EngineWearMode` forced to 3, a stray schedule-array write on
   Raikkonen, a leftover 512-byte allocated buffer from Follow-up 8's
   injection attempts) — gr28 was never tested against a game state this
   heavily hand-modified before.
2. The `EngineWearMode` global (`dword_7CEA5C`) was left at `3` (forced
   earlier this session, never restored to the original `2`) — if gr28 or
   the restart path reads this and doesn't expect `3`, that's an unvetted
   interaction.
3. `DriverSetupParamLookupByID`-adjacent state on Raikkonen (schedule slot
   `veh+0x4144=5, +0x4148=1`) was left in from the earlier scripted-
   failure test — stale/inconsistent with his actual current lap by now.

**Session ends here per corpus owner's direction.** Next session should
start clean (fresh game launch, don't assume any of this session's forced
global/per-car state survived or is safe) before retrying `gr28.dll`.

**Correction to the field-survey table above:** `0x4510` and `0x4534` are
NOT unnamed/unanalyzed — checking `tools/sdk/f1c_sdk.h` (which the corpus
cross-check above should have done and didn't) shows both already
CONFIRMED-tier named: `VEHF_driverStatus` (0x4510) and
`VEHF_timeOfNextExit` (0x4534, explains the pit-related writers found:
`PostPitCleanup`/`PitStopDecisionTireCompound`). The "steering noise, not
status" dismissal of 0x4510 was premature — a real driver-status field
legitimately fluctuating while the car is spinning/recovering is not the
same thing as meaningless noise. Also found: `maxSpeed` is SDK-confirmed at
**0x4538**, a different, adjacent dword from the `0x453C` "SpeedCap" used
throughout `grid_reorder`/`red_flag_v2` (both real, both confirmed, just
don't conflate them). **Lesson: check `tools/sdk/f1c_sdk.h` for existing
names BEFORE cross-referencing `skeleton_facts_all.json` writer functions —
it's the faster and more authoritative source for vehicle-relative fields.**
