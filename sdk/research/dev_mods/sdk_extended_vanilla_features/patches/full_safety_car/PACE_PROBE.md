# pace_probe — the working caution engine

Reference for `dll/pace_probe.c`, built and driver-verified **2026-07-31**.

> **Read `HANDOFF_2026-08-01b.md` alongside this.** Two things below are no
> longer true of the current build: **this DLL no longer writes a car's position
> or velocity anywhere** (the garage-pose teleport is compiled out — DISPROVEN,
> it crashed the game), and the **in-lap hide is off**, because a car lifted out
> of the active array is simulated but *not driven*. The commentary section is
> current: all five SCAR scores are isolation-tested.

This is the Phase 3 + Phase 5 instrument from `PLAN.md`, and it turned into the
first working caution in F1 Challenge: the whole field slows to a commanded pace
speed, forms a single queue in classification order, holds station without
overtaking, and releases cleanly. There is still **no flag state, no pace car
vehicle and no restart procedure** — those are Phases 2, 4 and 6.

Everything below is either measured live or cites the function it came from.

---

## What it does

| capability | how | status |
|---|---|---|
| Hold a pace speed across the field | `sub_42ACD0(veh, N)` per car, re-armed at 20 Hz | proven, 21 cars, zero game overrides |
| Assemble the field into a queue | per-car slot `k × gap` behind the leader | proven, 2.9 km → 20 m queue in ~80 s |
| Keep the queue in race order | slot index from race position `veh+0x4060` | proven live after the offset fix |
| Prevent overtaking | approach law + the AI's own aggression fields | built, driver-verified |
| Wave-around ("lapped cars may pass") | uncap the lapped car until it catches the queue | built, driver-verified |
| Release everything | restore each car's own captured values | proven, repeatedly |

## The offsets it uses

Every one is CONFIRMED or FACT tier. **Three were corrected during this work and
the wrong ones are named here so nobody re-derives them.**

| offset | field | note |
|---|---|---|
| `+0x4538` | `maxSpeed` — the governor | written via `sub_42ACD0`, never by hand |
| `+0x4440` | actual speed | FACT, `name_index` |
| `+0x4044` | **lap distance** | **NOT `+0x4048`** — that one is player-only, reads −1.00 on every AI car |
| `+0x403C` | lap number | used to detect a lapped car |
| `+0x4060` | **live race position** | **NOT `+0x32D0`** — that is the static slot index |
| `+0x3450` | `pittingStatus` | **pit-owned means ≥ 2**, not ≠ 0 — 1 is the in-lap, still on track |
| `+0x32C8` | live flag | ≠ −1 |
| `+0x4620` | driver name | cars identify themselves; no hardcoded grid |
| `+0x44CC` | `PassingNorm` | 0.9 + 0.001 × `.rcd Passing`, read by `AIRacingLine` |
| `+0x44F8` | `TrackAggressionEffective` | read every frame by `AISteering` |

Globals: `dword_7C56A8` race phase (must be 5), `dword_7C56A4` session type,
`flt_8ACFB4` = **22.2222 m/s = 80 km/h = the pit speed limit**.

## The three control laws

### 1. The pace hold

`sub_42ACD0(veh, N)` writes `veh+0x4538`. Re-armed every 50 ms, because the game
reaches the same setter through its own command queue and will occasionally
release a car — measured: 4 real overrides on one car in one run, each corrected
within 50 ms. A one-shot write would have lost that car.

Commanded speed ramps down over 6 s on engage. Commanding the target directly
decelerated the field at ~15 m/s², i.e. the whole grid braking at once.

**`a2 == 18` is a trap**: the setter substitutes `flt_8ACFB4`, the pit limiter.
A commanded 18 m/s silently becomes 22.2. Guarded in two places.

### 2. The slot controller (queue assembly)

```
k    = number of governed, moving cars with a better race position
err  = (metres behind leader) − k × target_gap
cap  = pace + 0.25 × err,  clamped to [0.55 × pace, pace + 35]
```

Ordering by race position is **required**, not optional. An earlier version had
each car chase whatever was physically nearest ahead; that bunched the field
tightly and in arbitrary order, with lapped cars leading. This is exactly what
NT2004's `sub_4FA900` field-ordering exists for.

A car physically *ahead* of the leader (a lapped car) is held at 35% of pace so
the leader reels it in — the crude stand-in for a wave-around when the policy
forbids overtaking.

### 3. The approach law (no passing, and brake early)

```
allowed = (lowest speed of the car ahead in the last 0.5 s)
        + 0.22 × (gap − target_gap)
```

Applied last, so it overrides the slot controller: a car may want to be further
up the queue but may never get there *through* the car in front.

- At the target gap it may do exactly what the car ahead is doing ⇒ **cannot pass**.
- Closer ⇒ commanded slower ⇒ falls back.
- Further ⇒ closing allowance proportional to the room ⇒ **decelerates continuously**
  from 400 m out instead of arriving hot.

Two refinements that matter:

- **Abreast cars count as ahead** (±7 m longitudinally). The gap is measured
  along the lap, so a car alongside reads as ~0 m and was previously skipped as
  "that's us", which let a half-completed pass finish.
- **The lowest recent speed**, not the current one. A car that spins sheds speed
  in far less than one poll interval, and at 30 m/s a 50 ms lag is 1.5 m.

Exempt: cars on a wave-around lap, and lapped cars being deliberately held back.
Both are meant to be passed.

### The AI's own willingness to pass

Physics alone leaves the AI *trying*. Two CONFIRMED per-car fields, both baked
at session init and never recomputed, so a live write holds and the restore is
exact:

- `+0x44F8` → `0.01` (the game's own clamp floor). `AISteering` uses it for the
  desired follow gap `×(2−a)`, the side-by-side window `4−a`, and the
  per-opponent pressure accumulator `+= a × closing`. This is the PLR
  "AI Realism" knob's only effect.
- `+0x44CC` → `0.9` (equivalent to `.rcd Passing` = 0).

## The lapped-car policy

`Num/` toggles between two real procedures:

- **NOT ALLOWED** (default) — lapped cars stay in the queue in classification
  order. This is F1 Challenge's own 1999–2002 rule: no unlapping under the
  safety car.
- **ALLOWED** — the modern wave-around. Each lapped car is uncapped to full race
  speed, gets its real aggression back, passes the queue, and is put back under
  pace control once it reaches the back. Applied continuously, so a car that
  goes a lap down mid-caution is released too.

State machine per car: `NONE → RUNNING → DONE`. The "passed the leader" step is
observed explicitly, otherwise a lapped car already behind the tail would
declare itself finished without moving.

---

# The safety car

Added 2026-07-31, driver-verified end to end. F1C ships a complete FIA safety
car — `SeasonData/Vehicles/FIA/1999_SafetyCar/Safgpxa.veh`, `DRIVER="Bernd
Maylander"`, Mercedes livery, its own `safety.hdv` — which joins the grid as a
normal entrant when its `Classes` line matches the season.

## How it is kept out of the race — the NT2004 port

NT2004's pace car is invisible to the race because it lives in a **separate
vehicle list** at slot 48, past a full 48-car field (`sub_4E29B0` →
`sub_4EDDB0(i + 48)`). F1C has one array, so the port is:

> lift the car's pointer out of the **active-vehicle array at `0x7C54F0`**,
> shift the rest down, park the pointer in the freed last entry, and
> **decrement `VehicleCount` at `0x5C329C`**.

`VehicleCount` is FACT-tier with **7 writers — all session setup** — and **90
readers**, so one decrement removes the car from every field-wide system at
once. Live result: **gone from the standings and the spectate cycle**, game
stable.

**Two things this does NOT do**, both learned the hard way:

- **It does not move the car.** Rendering follows *placement*, not the field
  list, so a car hidden after grid placement stays parked in its grid box,
  visible and unreachable. It must reach its garage *first*.
- **It does not survive a session change.** Holding a decremented count across a
  restart **crashed the game on the loading screen** — the loader rebuilds the
  array on top of the stale modification.

Three guards resulted, and any mod touching `VehicleCount` needs all three:

1. Never touch the array or count outside green-flag racing (`0x7C56A8 == 5`).
2. Validate the whole array first — every in-field entry must be a slot-aligned
   pointer into the vehicle array — because the game is mid-rewrite during a load.
3. On restore, if the entry parked past the end is no longer ours, the session
   was rebuilt: drop the state and **write nothing**.

The DLL also restores the count on self-eject. A decremented count left behind
would cost the game an entry permanently.

## Why not just retire him

Because **retirement controls state, never placement.** Proven with the DLL
loaded before the field existed — retired flag set, AI control off, driver model
hidden, planned position last — and the game *still* placed him in grid box 23,
directly behind the player.

The other route, `sc1.c`'s `plannedPositionAtStart = 200`, is an out-of-bounds
write in disguise: placement is `sub_4D10E0` → `sub_403CD0(gridTable,
plannedPos - 1, ...)`, and `sub_403CD0` indexes `v4[7 * a2]` with **no bounds
check**, so 200 reads 1393 floats past the end of the track's grid array. Every
legal index is a real grid box (`.aiw [FEATURES] startinggrid`, 24–104 across 198
tracks), so there is no in-bounds "off the grid" position. Do not reuse it.

## The deployment chain

Every step is the game's own code, armed rather than simulated.

| phase | what happens |
|---|---|
| race start | starts normally from his grid slot — **never touched before green** |
| green | DLL arms `veh+0xD58 = 0` + `callToBox = 1`; he drives himself to the pit lane |
| garaged | at `pittingStatus = 4` he is lifted out of the field |
| **Numpad 0** | restored to the field, revived from the garage, drives out of the pit lane |
| out lap | uncapped until clear of the limiter (>25 m/s), then **SCAR02** |
| catch-up | runs **unrestricted** until he is in front of the race leader (NT2004 substate 1) |
| pacing | becomes the queue leader; the whole field forms up behind him |
| **Numpad 0** | **SCAR03** ("getting in at the end of this lap"), sent to the box by `callToBox`; **SCAR05** when he actually reaches the pit lane; **SCAR04** 3.5 s later as the restart arms; green flag |

Live state trace of the garage phase: `pit=0` at 82.1 m/s → `pit=2` at 22.3 m/s
(the game's own 80 km/h limiter) → `pit=3` stopped → `pit=4` garaged, with
`VehicleCount` 23 → 22 at that instant.

**The catch-up phase is not optional.** He leaves the pit lane wherever the pit
exit is — P19 of 23 in the live run — so without it he paces at 30 m/s in
mid-pack while the real leaders are held at the fallback crawl waiting for a car
that is not chasing them. NT2004 has the same phase as pace-lap substate 1,
"catching the field".

**Never retire a car before the green flag.** One doing so is left in grid-hold
with its brakes pinned at 1.0, and the proven de-retirement recipe cannot
recover it — `deretire_probe.py` wrote its full set and the car did not move at
all. That recipe recovers a car in its *garage* with the engine off, which is a
different state from one that never started.

## The voice lines

The four safety-car clips exist in `PC_RE_MAIN.SDT` but have **no PWS event**:
the low-eid table runs eid 131 (score 129, RED01), eid 132 (score 130,
SLOWCAR01), then jumps to eid 133 (score 136, YELLOW04) — skipping exactly the
four safety-car scores. Only eids 423–428 reach them, and those are above 291 so
they play from the **TV** table; isolation-testing eid 423 produced an unrelated
position line, *"are second"*.

The way in: **the channel comes from the eid range, the clip comes from the
score**, and the two are independent. So a low eid is borrowed as a carrier and
its score is temporarily rewritten:

```c
sc_say(score):  VirtualProtect(EVENT_TABLE + 132*0x14 + 0x10)
                write score  ->  inject carrier eid 132 via the game's own
                FN_STRUCT_INIT / FN_DRIVER_LOOKUP / FN_INSERT_ENTRY
                hold 300 ms  ->  restore
```

| score | clip | used for |
|---|---|---|
| 131 | SCAR05 | "safety car is entering the pits now" |
| 132 | SCAR01 | "safety car has been deployed and is exiting the pits now" |
| 133 | SCAR02 | "safety car is on track" |
| 134 | SCAR03 | "safety car getting in at the end of this lap, get ready" |
| 135 | SCAR04 | "safety car is in" |

**ALL FIVE ISOLATION-TESTED 2026-08-01**, one at a time, driver listening, with
`tools/sc_say_test.py`. Score 135 was the orphan this document previously
flagged as inferred — it is real, and the lines above are what the driver heard
verbatim.

**Trap:** a line fired while the previous one is still playing comes out as the
CARRIER's own clip (score 130, SLOWCAR01, "watch out, there's a slow car
ahead"), because the game builds the queue entry after `sc_say` has already
restored the original score. Leave a few seconds of silence between tests, and
treat `isPlaying=1` as "this result is void" for carrier-score tests.

`sc1.c` wrote this technique down in 2026-07-25 with carrier 131 but was never
injection-tested; this is its first live confirmation. **It generalises**: any
TV-only or orphaned clip can be played on the race-engineer channel this way,
with no exe patch.

## Controls — two keys and an on-screen menu

Rewritten 2026-08-01. There used to be seventeen global hotkeys. That is
unusable at speed: every one fires instantly, several irreversibly, and nothing
on screen told you what state you were in.

```
Num0   open / close the MENU
Num0   deploy the safety car; once he is out, bring him in and restart
```

Inside the menu: **Up/Down** move, **Left/Right** change the value, **Enter**
acts on the row, **Num0** closes.

| row | what it does |
|---|---|
| SAFETY CAR | deploy / bring in — the same thing `Numpad 0` does |
| PACE HOLD | engage or release the speed hold on its own |
| PACE SPEED · TARGET GAP · CAUTION LAPS | the three numbers worth tuning |
| LAPPED CARS | HOLD (the 1999–2002 rule) or MAY PASS (wave-around) |
| BUNCHING · AUTO CAUTION · YELLOW FLAG | on/off |
| PLAYER | is the player's own car paced with the field |
| SLOWDOWN RAMP · SC PARKING · SC HIDING · TELEMETRY | on/off |
| DUMP FIELD · PANIC RELEASE | actions |

The menu is drawn on the game's **own message strip** — the `RED FLAG` one,
mechanism from `mods/red_flag/gr29.c`: 8 slots at `*(u32*)0x7CD204`, stride
`0x5C`, ASCII at `+0x3C` capped at 31 characters, countdown float at `+0x90`.
One slot per line, re-armed every tick so it cannot fade, giving a status line
plus seven scrolling rows. Slot order is assumed to be draw order; the `>`
cursor keeps the menu usable if that turns out to be wrong.

No new text surface was invented. Two were tried by earlier sessions and both
failed: `TimerDisplayRender` (0x4D9DD0) is grid/classification only and is
called zero times in race, and `UITextRenderWithScale` (0x4D7E40) called from a
`DrawDebugTextOverlay` hook drew 3285 frames with nothing visible.

`Num+`/`Num−` stay unused — the game's spectate-view binds. **`Num2` stays
unbound**: it belongs to the red-flag DLL, a separate deliberate injection
pulled from the mod pack on 2026-08-01.

**`Num1` IS bound since 2026-08-02**: it lifts the safety car out of the
pre-race classification, and it is REFUSED outside `RACE_PHASE 2`. Pressing it
during the lights re-hides him and leaves him brake-locked in his grid box for
ever — see the addendum in `HANDOFF_2026-08-01c.md`, which also explains why
that screen cannot be handled automatically.

**Pace default is 56 m/s (201.6 km/h) since 2026-08-02**, a VSC-style cap, with
the track-shaped pace switched off. Older text in this document that assumes
30 m/s describes the earlier default, not the current one.

Build and load:

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o pace_probe.dll pace_probe.c -lkernel32
```

```bash
python tools/dll_unload_signal.py F1C_PACEPROBE_UNLOAD
```

The DLL **releases the field before it ejects** — otherwise every car would keep
its commanded speed with nothing left to restore it.

## Safety rules honoured

- Gated on `dword_7C56A8 == 5`; auto-releases if the phase leaves 5.
- Never calls `sub_424CD0`, so `CommandQueueResponseHandler_00429ee0`'s
  race-phase branches never run. `sub_42ACD0` is a leaf float store, safe from
  the worker thread; the "game thread only" rule is specific to the dispatcher.
- Never governs a car in the pit lane — `maxSpeed` is the game's pit limiter
  there, and fighting it would send AI speeding down their own pit lane.
- Restores each car's **own** captured values, never a hardcoded default: the
  field is not uniform (one car was found at `999999.0` while 20 read `99999.0`).
- Refuses to capture a sub-1000 `maxSpeed` as a restore value — that is pit-limit
  residue on a car that just left the lane, and restoring it would pin the car
  at 80 km/h for the rest of the race.

## Measurement traps found the hard way

Each of these produced a confident, wrong reading before it was caught.

1. **An override counter must compare against what you last wrote**, never
   against what you currently want. Comparing against the target reported 1155
   game overrides when the true count was zero — all of them its own ramp steps.
2. **A "position unchanged" result is worthless if the field cannot change.**
   `+0x32D0` is the slot index, so `pos-changed = 0` for 149 s proved nothing.
3. **Never treat an unmeasurable input as zero.** Reading lap distance from the
   player-only offset made every gap `(−1) − (−1) = 0`, so the controller
   concluded the field was nose-to-tail and dragged everyone to the floor speed.
4. **Do not reject large inputs in a system whose whole job is large inputs.** A
   400 m gap cutoff left 2 of 20 cars with a "valid" gap on a 5.8 km lap.
5. **Parked cars are not competitors.** They held queue slots and dominated the
   error statistics while every moving car was on target.
6. **`SessionTime` (`0x8AFEA0`) is not a reliable duration.** It ran 149.3 s
   inside 47.8 s of wall clock, in bursts. Still valid for detecting a pause.

---

# The caution lifecycle — added 2026-08-01, BUILT NOT YET DRIVEN

Everything in this section compiles clean and **has not been run in the game.**
Treat every claim here as a design statement until the driver confirms it.

The arc is NT2004's — watch, decide, neutralise, gather, run, warn, inspect,
release — with NASCAR's rules replaced by F1's 1999–2002 ones.

| state | what it means |
|---|---|
| `CT_OFF` | green-flag racing; the watcher may run |
| `CT_DEPLOY` | called: yellow flags out, field slowing, car on its way out |
| `CT_PACE` | the safety car has the field; queue forming, caution laps counting |
| `CT_IN` | "safety car in this lap" — wave-around window shut |
| `CT_RESTART` | car in the lane; the leader controls; no passing until the line |

## The yellow flag is real, and it was already solved

The earlier note that F1C has "no yellow-flag concept" was wrong. F1C has
**29 marshalling zones** on the Steward at `0x8ACEE0` — solved 2026-07-25 in
`investigations/yellow_flag/`, and this session traced the rest of the chain.

```
sub_4D1730   every frame: clear all 29 zones, then for each car whose forward
             velocity is below its segment's threshold, set that car's zone = 2
   │         then diff against the previous copy and, on any change,
   ▼
sub_41B3C0   THE FLAG EVENT DISPATCHER (misnamed PitSelectionStateMachine)
             a2 = 0 cleared · 2 raised · 3 red flag
             raise  -> eid 134/136/138  PWS_YELLOW01/02/03
             clear  -> eid 133/135/137  PWS_YELLOW04/05/06, and 126 GREEN01
```

Two consequences, pulling opposite ways:

- **The table cannot be poked.** It is rebuilt from scratch every frame.
- **The table is therefore a clean incident detector**, computed by the game,
  and it *stays* clean while this DLL runs a caution.

So the table is **read, never written** — that is the watcher — and the flag
*display* is forced separately.

### Forcing the display

The renderer asks one argument-less boolean, `ShortComputation_bad0`
(`0x0049BAD0`): *is the player's zone, or the one behind it, yellow*. All three
callers are display. Its entry is stack-balanced and it takes no arguments, so
`mov al,1 / ret` is a legal early return.

`0x0049BAD0` is 16-byte aligned and the fourth byte is preserved, so the swap is
**one aligned 32-bit store** — the render thread can never execute a torn
instruction. `0x54E0A151` ↔ `0x54C301B0`. The DLL verifies the original dword
before writing, refuses permanently on mismatch, and restores it on green, on
release, and on self-eject.

The yellow *lines* are deliberately **not** injected. The game announces its own
zone yellows through the chain above; injecting would double them.

## The restart — the one phase that is not NT2004's

NT2004 counts 5-4-3-2-1 and releases the whole field at one instant
(`sub_4FA8E0` → `sub_4FAEF0` → `sub_4FAFF0`). F1 does not, so that is not what
was built.

1. Safety car reaches `pittingStatus >= 2`. **SCAR05** plays ("entering the
   pits now") and **SCAR04** ("safety car is in") is deferred 3.5 s behind it,
   because `sc_say()` holds the carrier score for 300 ms and two lines fired in
   one tick clobber each other.
2. **The pace cap is lifted from every car at once** — but the approach law
   stays armed, so nobody can pass, and the train accelerates in order behind
   whatever the leader chooses to do.
3. Each car is **fully released the moment its lap number increments** — the
   start/finish line, which is where F1 allows passing again.
4. When the **leader** crosses: yellow down, **PWS_GREEN01** (eid 126, native,
   no carrier needed), HUD `GREEN FLAG`.
5. A 60 s timeout frees whatever is left, so a wrecked car cannot strand the
   caution.

The whole restart needed no new lever. Lifting the base cap and leaving the
approach law armed *is* the F1 restart.

## The trigger: NT2004's gates, ported 2026-08-03

The automatic declaration used to be one condition — the game's zone table
yellow for 3 s. NT2004 wraps its own scanner in five more, and each one exists
to stop a caution that would be *wrong* rather than merely unlucky. All five
are now here, and every one of them is on screen while it is being evaluated.

| Gate | NT2004 | Here |
|---|---|---|
| in a race | `dword_75CBB0 == 7` in `sub_4F9040` | `phase_racing()` |
| automatic declarations allowed | `dword_767310 == 2`, i.e. `FlagRules=All` | the GUI tickbox |
| nothing already running | steward state | `g_ct == CT_OFF && SC_IDLE` |
| **not the final lap** | leader lap `<` race laps (`steward+144`) | leader lap `<` `TotalRaceLaps` (`0x8ACF8C`, FACT) |
| **post-restart grace** | 10 s window inside `sub_4F93E0` | same, from the last green, GUI-tunable |
| **whole-field mask** | every car carries byte `+0x5188` | the field's own median is above walking pace |
| incident | the scanner's stopped / damaged / off-pace predicates | the game's zone table **or** cars stopped/off the pace |
| persistence | — | the incident must hold, default 3 s, GUI-tunable |

**`dword_75CBB0` is the SESSION-TYPE LADDER, not a race phase.** `sub_4B99F0`
walks it 1→2→3→4→5→6→7 one step at a time and race is 7; the caution state
lives entirely in `dword_75CBB4` and the steward's own field. `DESIGN.md` said
otherwise until 2026-08-03 — see `findings.jsonl`
`nt2004-75cbb0-is-session-ladder-not-caution-phase`.

**Severity is ported, the NASCAR names are not.** `sub_4F95D0` counts the cars
needing attention and returns a full caution above a threshold, else a
"quickie", differing only in caution-lap count. F1 has one procedure, so it
shows up here purely as the length: at or above *Cars for a FULL one* the
caution runs the full lap count, below it the short one.

**Manual deployment is never gated by any of this.** `Home` and the GUI's
deploy button call `sc_deploy()` directly, and a manual caution always gets the
full length. The gates only decide whether the DLL may declare one by itself.

### Watching the watcher

The whole evaluation is published into the control block every tick — the
gates as a bitfield, the counts, the persistence timer, the field median, the
slowest car by name, and a 0-100 closeness figure. Two views read it:

```
python tools/safety_car_gui.py       "Caution trigger watch" panel, right side
python tools/sc_trigger_watch.py     the same numbers, animated, in a terminal
```

Both are read-only and can be started or stopped mid-race. This exists because
the previous trigger was a hidden number: when it did not fire, the only way to
find out why was to rebuild with more logging and drive again.

## The rulebook we are modelling, and where we knowingly differ

The game is 1999-2002, so the procedure being replicated is the one of that
era — not the modern one. Several rules people remember as "the safety car
rules" postdate this game by five to twenty years, and copying them in would be
wrong. Checked against the sport's own history 2026-08-03.

| Rule | Era | Us |
|---|---|---|
| **Pit lane stays OPEN under the safety car** | pre-2007. F1 closed the pit lane only in **2007-2008** — no entry until the field had formed up and "pit lane open" was given, ten-second stop/go otherwise — and dropped it again for 2009 in favour of the minimum-lap-time delta | **Open. Always was, and it stays that way.** Nothing here closes it, refuses an entry or penalises one. `pit_watch()` publishes the count so this is a number on the panel, not a claim |
| A car on its **in-lap** is still racing under the caution | all eras | `pittingStatus == 1` stays governed at pace. Only from 2, in the lane under the game's own 80 km/h limiter, do we let go entirely |
| **Lapped cars do not unlap themselves** | pre-2010. Explicitly forbidden 2010-2011, permitted from **2012** | Wave-around **default OFF**, which is the era-correct setting. The switch exists for people who want the modern race |
| No overtaking under the safety car | all eras | The approach law makes a pass arithmetically impossible rather than penalising one after the fact |
| Cars **between the safety car and the leader may pass him** until the leader is behind him (his green lights) | all eras | **Deviation, see below** |
| "Safety car in this lap" — lights off, field stays in formation, racing resumes at the line | all eras | `CT_IN` then `CT_RESTART`; the whole field is freed in one instant when the LEADER crosses the line |
| Red light at the **pit exit** while the train is on the start/finish straight | modern | Not modelled. No lever found for it |
| Minimum-time delta enforced per car | 2009+ | Not modelled, and it should not be — it is a decade past this game |

**The one deviation that matters.** Real procedure lets cars caught between the
safety car and the leader pass him and rejoin the back of the queue, which is
what his green lights mean. Here they are instead held at a fraction of pace
until the leader reels them in. That is deliberate and it is not free: the
first version that let a car near him run free produced a driver report,
*"Hakkinen tried to overtake the safety car and got hit"*, because a stale
hold-back flag left him passable. Changing it means re-opening the law that
fixed that. Logged as OPEN rather than quietly ignored.

## Caution laps and the inspection gate

Laps are counted from the leader, and only once the safety car actually has the
field — laps run while he is still catching it are not caution laps. When the
count is reached the recall waits for the queue to be **FORMED**, not merely for
the laps to run out. That is NT2004's withholding-green inspection
(`sub_4FA1A0`) in F1 clothing: a restart from a strung-out field is exactly what
it exists to prevent.

## One more fix that only the restart exposed

`nearest_obstacle()` now skips cars with `pittingStatus >= 2`. A car in the pit
lane is not on the racing line, and at the restart it is the safety car peeling
in **directly in front of the leader** — capping the leader to a car on the
80 km/h limiter would have stalled the green.

## New controls

```
PgUp/PgDn  caution laps ±1 (default 2)
Left       AUTO-CAUTION toggle - declares a caution by itself once every gate
           in the table above is open. OFF by default.
Del        force the yellow flag on/off by hand (flag test, no caution)
```

The gate settings themselves are in the GUI's **Rules** box: short-caution
length, how many involved cars make it a full one, the restart grace, how long
an incident must hold, and whether the final-lap gate is honoured at all.

## Not done

- Penalties for passing under the safety car. F1C has its own penalty emitter
  (`sub_41A720`, wired for player and AI); nothing hands it a drive-through yet.
- Safety-car lights, and a trackside waving-yellow beyond the flag strip.
- `AllowAutoPace`-style config, `.aiw PaceCarReleaseDist`, driving-aid bit.
- ~~SCAR04's score 135 is inferred~~ **DONE 2026-08-01: all five scores
  isolation-tested, and SCAR03/SCAR04 were swapped in the call sites. See the
  score table above.**
- The catch-up phase is still uninstrumented.
## Pit stops under caution — kept open, deliberately

**Project decision, 2026-07-31.** Pitting under a safety car is normal and is a
real strategic decision, so the pit lane is **not** closed. Closing it was
considered and rejected.

The failure that looked like a pit-lane problem was not one. A damaged car got
called to the box and drove its entire in-lap at full racing speed, passing the
whole paced field — driver report: *"TesterA ... disregarded all safety car pacing
rules and just went 100% into racing mode and overtook everyone. When he got out
of pits he waited for his position."*

That was `pit_owned()` testing `pittingStatus != 0`. Status 1 is the in-lap, with
the car still out on the circuit, so the probe was handing it back to the game a
whole lap early. With the test at `>= 2` the sequence is correct end to end:

1. Called to the box — stays capped at pace speed, no overtaking, all the way to
   the pit entry.
2. In the lane (`>= 2`) — handed to the game, which applies its own 80 km/h
   limiter through `maxSpeed`.
3. Back on track — picked up by the mid-hold re-arm and slotted into the queue
   at its position, which the driver confirmed happens by itself.
- Wave-around cars pass the queue on the racing line — nothing routes them
  around the outside.
