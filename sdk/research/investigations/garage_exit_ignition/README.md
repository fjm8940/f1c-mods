# Garage exit / de-retirement — the gate is IGNITION

**Date:** 2026-07-25
**Status:** **SOLVED — 3/3 in-race, cars drive out of the garage and rejoin the
race.** The pathing "problem" was a STALE WAYPOINT POINTER on one test subject,
not a missing path. See "Pathing SOLVED" below. The `0x4510` + ignition recipe
needed no additions.
**Tools:** `tools/garage_exit_trace.py` (read-only), `tools/deretire_probe.py` (writes)

## The question

Can a retired driver be brought back? Prior art (`mods/red_flag/gr43.c`
`unretire_pitret()`) clears the retirement flags, but only ever immediately
before a grid restart, which repositions every car. Nobody had tried it
standalone, mid-race.

## What happened

Alonso (slot 21) was retired via `tools/force_wing_retire.py arm 21` — the
game's own path, garage placement included (PhysX 17.09). Then the gr43 write
set was applied:

```
0xD58 RWflag=1, 0x454C callToBox=0, 0x3450 pitting=0,
0x4064 FinishState=0, 0x4410 RetiredFlag=0, 0x45F4 ActiveFlag=1
```

**The flags held.** 40 s, nothing re-retired him, `racePos=17` intact. **The car
did not move.** `moved=0.00`, speed 0, waypoint frozen at node 9/2.

A field-wide read then killed the obvious hypotheses: Alonso's `0x32C8`
(AIEnableFlag), `0x4510` (driverStatus) and `0xD0` all *already matched* the
cars that were racing. No state flag distinguished him.

## Tracing the game's own transition

Rather than teleport the car (forging state again), we watched AI cars leave
their garages legitimately in a warmup session and recorded what flips *before*
first motion.

Naive diffing is useless here: a parked car is still physically simulated, so
~406 words jitter every sample. Two filters made it readable, and both are
implemented in `tools/garage_exit_trace.py`:

1. **discrete vs continuous** — count changes per 4-byte word; physics noise
   changes every sample, a state flag changes once or twice. Keep low counts.
2. **cross-car intersection** — trace the whole garaged field at once; a field
   that flips before *every* exit is a candidate cause.

406 offsets collapsed to **17**, nearly all in one tick 3.75 s before the car
rolled. The named one:

| offset | flip | name |
|---|---|---|
| `0x4510` | **5 -> 3** | driverStatus [CONFIRMED] |

Per `skeletons_filled/AIMainUpdate_00424e70.md`, the switch is
`0=follow, 1=parade, 2=garage, 3=approach, 4/5=pit-entry, 6=pitting, 7=spin`.

Field-wide read, 22/22 cars, zero exceptions:

| state | `0x4510` | `0x4488` | `0x4578` | speed |
|---|---|---|---|---|
| in garage | 5 | 1 | 1 | 0.0 |
| on track | 0 | 0 | 0 | 20-91 |

So a garaged car is `driverStatus=5`. Deretired Alonso read `0` — the **on-track**
signature — while parked in a garage. We had forged the destination state, so the
AI believed the car was already out racing and never ran an exit.

## driverStatus is causal — but not sufficient

Poking `0x4510 = 3` on a healthy garaged car (Hakkinen, slot 2) made the game run
the rest of the sequence **by itself**: `0x4488 1->0`, `0x4578 1->0`,
`0x4314 0->500.00`, then `0x4510 3->0`. The full trace signature, reproduced from
one write.

**The car still did not move.** 40 s, speed 0.00, position unchanged.

Since Hakkinen was never retired, this proved the wall was never about
retirement at all.

## The actual gate

Direct observation from the user: *Hakkinen was turning his steering wheel.* The
AI was driving — it had a path and a racing line. So the missing piece was
longitudinal, not lateral. Comparing powertrain fields:

```
Hakkinen (frozen):  dStat=0 throttle=0.400 brake=1.000 gear=1 RPM=0.0  angVel=0.00   speed=0.0
M.Schumacher (out): dStat=0 throttle=0.970 brake=0.000 gear=7 RPM=1792 angVel=1791.98 speed=90.6
garaged cars:       dStat=5 throttle=0.000 brake=1.000 gear=0 RPM=0.0  angVel=0.00   speed=0.0
```

The AI had selected **1st gear** and applied **40% throttle** into an engine with
**`EngineRPM = 0` and `EngineAngVel = 0`**. The engine was never started.

`brake=1.000` is a red herring — Heidfeld was doing 77.9 with brake at 1.000.

Two writes fixed it:

```
0xA0   EngineAngVel = 800.0   (float)
0x39A0 EngineRPM    = 800.0   (float)
```

```
t+ 1.5  moved=  1.51  RPM= 431.9  brk=0.000  gear=1  spd= 2.29
t+ 4.5  moved= 38.62  RPM=1010.6  thr=0.366  gear=1  spd=22.20
t+13.5  moved=245.48  RPM=1493.5  thr=0.970  gear=2  spd=37.92
t+21.0  moved=729.09  RPM=1762.0  thr=0.970  gear=6  spd=82.46
```

The car released its own brake, pulled away, upshifted 1->2->3->5->6 and was on
the racing line at 82 km/h, 729 m later. The RPM did not stay at 800 — physics
took it over immediately (431 -> 679 -> 1010). **This is a starter crank, not a
drive input.** One shot, then hands off. Same shape as the retire mechanism:
arm the trigger, never forge the result.

## Why every previous attempt stalled

`mods/ai_pit_retire` writing the end-state, gr43's `unretire_pitret()`, and the
Alonso poke above all produced the same thing: a car the AI was genuinely trying
to drive, with a dead engine. The classification layer and the state machine were
never the problem.

## Recipe (DERIVED)

```
veh+0xD58  RearWingIntactFlag = 1     disarm, else it re-retires at the box
veh+0x454C callToBox          = 0
veh+0x3450 pitting            = 0
veh+0x4064 FinishState        = 0
veh+0x4410 RetiredFlag        = 0
veh+0x45F4 ActiveFlag         = 1
veh+0x4510 driverStatus       = 3     approach - NOT 0
veh+0x105D DriverModelVisible = 1     else the car races with an empty cockpit
veh+0x4354 curWaypoint        = 0     force a fresh nearest-waypoint query
veh+0xA0   EngineAngVel       = 800   one-shot crank
veh+0x39A0 EngineRPM          = 800
```

**This is the complete, in-race-proven set** (3/3 cars, 2026-07-25). No pathing
fields need seeding.

## Trust

- `0x4510` driverStatus — **CONFIRMED** already; this session adds poke-proof
  that writing `3` drives the whole exit sequence.
- `0xA0` EngineAngVel (FACT), `0x39A0` EngineRPM (CONFIRMED) — names pre-existing;
  their role as the garage-exit gate is **DERIVED, poke-proven on a healthy car**.
- `0x4488`, `0x4578`, `0x4314`, `0x434C`, `0x42F8`, `0x4330`, `0x39F8`, `0x1060`,
  `0x1068`, `0x058C/0x05AC` triads — **GUESS**. Observed flipping in the exit
  sequence; only one natural exit (Frentzen) was captured, and the game sets them
  itself once `0x4510` is written, so none were independently tested.
- `0x4534` (341.856 -> 345.857, 7.75 s before exit, earliest flip) — **GUESS**,
  possibly a scheduled out-time. Not investigated.

## Race test — M. Schumacher, slot 1 (2026-07-25)

Retired mid-race via `force_wing_retire.py arm 1`; game placed him in his garage
(16.92, 302.74). Then the recipe above, verbatim:

```
t+ 1.5  moved=  2.77  RPM=600.6  thr=0.400  brk=0.000  gear=1  spd=3.57  X=14.15
t+ 3.0  moved= 10.87  RPM=600.6  thr=0.400  brk=0.000  gear=1  spd=7.08  X= 6.05
t+ 4.5  moved= 16.16  ...                                      spd=0.26  X= 0.76
t+15.0  retired=1 active=0 finish=2  — teleported back to 16.92
```

**Revival works on a genuinely retired car in a race.** Engine lit, brake
released itself, 1st gear, 40% throttle, 16 m under its own power. Everything
observed in warmup reproduced.

**Pathing does not.** It drove X 16.92 -> 0.76, straight into the wall in front
of the garage, stalled against it, and at t+15 the game retired it AGAIN. The
re-retirement is itself confirmation the car was fully back under normal rules,
including crash-damage retirement — a live car that crashed, not a zombie.

Why Hakkinen managed 729 m and Schumacher 16: **session context**. In warmup,
cars are meant to leave the garage, so a pit-exit path exists, and Hakkinen
started from the true garage state `dStat=5` and let the game walk itself
`5 -> 3`. Schumacher was forced straight to `3` from `0`, skipping the origin
state — and `dStat` decayed back to `0` within 3 s. He got a racing line he
could not reach instead of a pit-lane path.

## FAILED: writing every field a healthy car uses

Hypothesis: diff the retired car against healthy cars and set every differing
flag-like field. Diff of retired Schumacher vs 18 cars doing >20, keeping only
offsets where ALL healthy cars agree and both sides are small ints — 14 hits:

| offset | retired | healthy | note |
|---|---|---|---|
| `0x4064` | 2 | 0 | FinishState [CONFIRMED] |
| `0x4410` | 1 | 0 | RetiredFlag [CONFIRMED] |
| `0x45F4` | 0 | 1 | ActiveFlag [CONFIRMED] |
| `0x105C` | 1 | 257 | bytes `01 00` vs `01 01` -> DriverModelVisible (`0x105D`) = 0 |
| `0x39F8` | 257 | 256 | the flag seen flipping in the exit trace |
| `0x00C4` | 1 | 0 | GUESS |
| `0x048C` | 2 | 0 | GUESS |
| `0x0498` | 1 | 0 | GUESS |
| `0x3338` | 139 | 79 | GUESS |
| `0x4114` | 0 | -1 | GUESS |
| `0x445C` | 0 | 1 | GUESS |
| `0x4590` | 0 | 53 | GUESS |
| `0x459C` | 8 | 0 | GUESS |
| `0x45E0` | 1 | 0 | GUESS |

Note `0x4510` driverStatus and `0x32C8` AIEnableFlag are ABSENT — retired and
healthy both read 0.

Writing all 14 (plus disarm + crank, `dStat` left at the healthy value 0) was a
**regression**: `ActiveFlag` went `1 -> 0` within 1.5 s, engine died, brake back
to 1.000, gear 0, car never moved. The game actively DEACTIVATED it, leaving
`retired=0, finish=0, active=0` — neither racing nor properly retired.

| attempt | dStat | extra fields | result |
|---|---|---|---|
| recipe | 3 | none | engine lit, drove 16 m, crashed, re-retired |
| shotgun | 0 | all 14 | deactivated in 1.5 s, never moved |

One of those 14 writes triggers deactivation; which one is untested. The lesson
is the same one that froze Alonso: copying a healthy car's state onto a garaged
car describes a car that is ALREADY OUT RACING. More writes is the wrong axis —
the previous run had already proven the AI drives fine once the engine is lit.

## REFUTED: borrowing the warmup origin state (`driverStatus = 5`)

Single-variable test against the known-good recipe: `dStat = 5` (garage) plus the
garage triad `0x4488 = 1`, `0x4578 = 1`, then crank and hand off. The hope was
that the game would walk itself `5 -> 3` as Hakkinen did in warmup.

`dStat` **held** at 5 (it did not decay to 0 the way 3 does), but `ActiveFlag`
went `1 -> 0` within 1.5 s, the engine died, and the car never moved (51 s).

Reading: in a RACE, "car sitting in garage" is not a legal running state, so the
game switches it off. Warmup tolerates `dStat = 5` because that is where cars
legitimately live before going out. **The warmup origin state cannot be
borrowed.**

Full scoreboard on a genuinely retired car in a race:

| dStat | extras | ActiveFlag | result |
|---|---|---|---|
| 0 (Alonso, no crank) | — | held 1 | alive, frozen, engine off |
| **3** | — | **held 1** | **engine lit, drove 16 m**, hit wall, re-retired |
| 0 | all 14 healthy fields | -> 0 in 1.5 s | deactivated |
| 5 | garage triad | -> 0 in 1.5 s | deactivated |

`dStat = 3` is the only state that survives in a race. It stays the known-good.

## Pathing SOLVED — it was a stale waypoint pointer (2026-07-25)

The recipe was already complete. Applied unchanged to three genuinely retired
cars (Raikkonen 7, Alesi 18, Alonso 21), **all three drove out of the garage,
down the pit lane, and rejoined the race:**

```
t+ 1.5  moved=  2.81  dStat=3  thr=0.400  gear=1  spd= 3.69  lane=1
t+ 3.0  moved= 16.35  dStat=0  thr=0.728  gear=1  spd=15.75  lane=1
t+12.0  moved=440.39           thr=0.000  gear=5  spd=70.08  lane=1
t+13.5  moved=550.29                      gear=5  spd=76.12  lane=1 -> 0   PIT EXIT
t+30.0  moved=1286.07                     gear=6  spd=79.63  racePos 20 -> 19
```

They braked for corners, self-shifted to 6th, and Raikkonen **gained a place**.
Alesi and Alonso crossed from pit lane to track at the identical waypoint
(`0x36798a40`) — the pit-exit node.

### Why M.Schumacher hit the wall

The one variable that differed, visible in the probe's `before:` line:

| car | `0x4354` curWaypoint at revive | result |
|---|---|---|
| Raikkonen / Alesi / Alonso | `0x0` | drove the pit lane out |
| M.Schumacher | `0x36788218` (stale) | 16 m into the wall |

`sub_426430` line 76: a NULL waypoint forces a fresh nearest-neighbour query.
Line 87 only re-queries a non-null one when the car is more than `3200` (squared,
≈57 m) from it. Schumacher was holding a racing-line waypoint from *before* his
retirement, close enough to pass that test — so the AI never re-queried and drove
straight at a node on the other side of the garage wall. Alonso's original
"waypoint frozen at node 9/2" was the same thing.

`deretire_probe.py` now always writes `0x4354 = 0`: a no-op on a car the game
retired itself, the fix on any car holding a stale pointer.

**Directly confirmed on a stale-waypoint garaged car.** Frentzen was revived,
then retired again by the game's own trigger. That second retirement left him in
the garage with the SCHUMACHER signature rather than the clean one — `wp` stale
(`0x367895d8`), `lane=0` — because a revived car carries its pointer into the
retirement. Reviving him with `0x4354 = 0` in the recipe:

```
t+ 1.5  X=14.43  spd= 3.32     out of the garage
t+ 4.5  X=-0.00  Z= 404.21     on the pit-lane corridor
t+21.0  X=-33.32 Z=1044.25  gear=5  spd=74.48
t+30.0  moved=905.74  racing
```

Same start signature as the car that hit the wall, one added write, opposite
outcome. This is the cleanest evidence in the investigation that the stale
pointer WAS the pathing bug.

### What the waypoint fields actually are (from `sub_426430_00426430.c`)

- `0x4550` — **NOT** a lap counter. It is the waypoint-set selector passed as the
  3rd arg to `SpatialNearestNeighborQueryWithPlaneTest` (line 78/89), and line 138
  **rewrites it every frame** from the acquired waypoint's `+0x12C`.
  **Do NOT read it as "the car is in the pit lane".** Frentzen drove the entire
  pit-lane corridor with `0x4550 == 0`, while Raikkonen drove the same corridor
  reading 1 until Z~984. It selects which node set the query searches; it does not
  reliably label the corridor the car is physically on. Seeding it is a no-op
  (see REFUTED below). Trust the car's X/Z, not this field.
- `0x454C` callToBox selects the chain branch (line 120): each waypoint has a
  normal successor at `+0x110` and an alternate at `+0x114`, and the alternate is
  taken only when `callToBox == *(alt+0x12C)`.
- `0x4534` is `timeOfNextExit` **[CONFIRMED]** per `AIMainUpdate_00424e70.md`, not
  the GUESS this file previously recorded.

### Synthetic retirement revives too — and more easily

A car retired by FORGING the end-state (`FinishState=2 / RetiredFlag=1 /
ActiveFlag=0 / PitState=2`, the `ai_pit_retire` set — `deretire_probe.py
synthretire <slot>`) is never teleported, so it freezes mid-track. Reviving it
needs no route at all: Frentzen restarted on the racing line where he stopped,
ran a full lap, reached 7th gear at 88 km/h and settled back at his original
`racePos=8`. Its stale `0x4354` is harmless because the car never moved, so the
old waypoint still describes where it is.

| | genuine (game's trigger) | synthetic (forged end-state) |
|---|---|---|
| where the car sits | garage, teleported | frozen in place |
| route needed | garage -> pit lane -> pit exit | none |
| `0x4354` | `0x0`, or stale if previously revived | stale but still VALID |
| result | rejoins via the pit lane | rejoins in place |

Untested combination: a car with REAL damage that is then synthetically frozen —
`RWflag=1` disarms the rear-wing trigger, but other damage state is untouched.

### REFUTED: seeding `laneMode = 1`

Hypothesis was that the car needed to be pushed onto the pit-lane waypoint set.
**A garaged/retired car already reads `laneMode = 1`** — field read, 3/3 retired
cars — so the write is a no-op. Kept behind `--lane-only` for contrast only.

### The limbo state is SOLVED (2026-08-03) — the re-retire gate was carConditions

The "unrecoverable" verdict above was wrong. Limbo is not a latched state; the
game is ACTIVELY re-retiring the car every cycle:

```
veh+0x3D6C carConditions == 2 (pit mode)          <- the latched poison
  -> AICollision_004bef60 returns 2
  -> EngineFailureCheck_004bec55 (the AI tick loop)
  -> event 26 -> PitCrewStateMachine_004bbfc0
  -> AIStateReset_0042af40 -> RetiredFlag=1 + AIControlFlag=0   (re-retire)
```

Both live limbo cars observed (Maylander, M.Schumacher) read `carConditions=2`;
every healthy car reads 0. A retirement INTO the garage sets carConditions=2,
so a car retired by the game's own path and then revived WITHOUT clearing it
gets re-retired within ~1 s — which is exactly what the 2026-07-25 attempts
saw (re-retire first, restore QualPos, whatever: the gate was never cleared).

**The unlock (proven live 2026-08-03, Maylander limbo -> left the pits ->
racing at 140 km/h; also revived a fresh ignition-off retirement the same way):**

```
1. veh+0x3D6C carConditions = 0                       <- THE fix
2. wheel-record attach flags: for k in 0..3: veh+0x834+k*0x128: +0x64=1, +0x84=1
   (AICollision returns 1 when a record's +0x64 byte is 0)
3. the normal deretire set (ret=0, fin=0, act=1, dS=3, RWflag=1, dm=1,
   curWP=0, tExit=0, 0x4548=0, maxSp=99999, laneMode=1, 0x4040=0) + crank
```

`tools/deretire_probe.py deretire <slot>` now includes all three layers.

**Causing a retirement with a dead engine (the reverse direction, 2026-08-03):**
hold `0xA0` EngineAngVel + `0x39A0` EngineRPM at 0.0 for ~10-15 s while the AI
drives — the engine never catches, the car sits still, and the game runs its
engine-failure retirement (ret=1, fin=2, garage teleport, carConditions=2).
A single zero-write is NOT enough: the game re-cranks within ~3.5 s.

**Still true from the old verdict:** reviving OUTSIDE a race session
(SessionType != 7) deactivates the car; the paused-game trap (frozen
SessionTime mimics deactivation) still applies.

**Method note:** two of the runs above were made while the game was PAUSED, which
freezes the physics step and mimics deactivation exactly (values hold forever).
Confirm the game is running before reading anything into a frozen car.

## Open

1. Which of the 14 fields causes deactivation — bisect, if it is ever worth it.
2. Whether lap count / race position / timing survive the round trip. `racePos`
   is preserved and updates live afterwards (Raikkonen 20 -> 19 by overtaking).
3. What starts the engine on the game's own path — never located. We bypassed it.
4. Only ONE natural garage exit was captured. Re-run `garage_exit_trace.py trace`
   across a full field for a stronger core set.
5. `0x105D` DriverModelVisible must be restored to 1 or the car races with an
   empty cockpit (observed on screen). Now in the recipe.
