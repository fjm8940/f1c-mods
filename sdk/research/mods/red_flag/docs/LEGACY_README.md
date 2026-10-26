# red_flag — builds and hotkeys

| build | hotkeys | status |
|---|---|---|
| `gr36.dll` | NUMPAD1 red flag | what `inject.bat` currently injects |
| `gr30.c` / `gr30.dll` | NUMPAD1 red flag + auto-fire on 3-car DNF pileup in 8s | previous source build |
| `gr31.c` / `gr31.dll` | NUMPAD1 timer restart, NUMPAD2 full red flag, NUMPAD3 pit-retire all AI | injection-tested 2026-07-24, all three hotkeys reported working. **Bug found in that test: cars going terminal during the red-flag window reappeared on the grid.** NOTE: `gr31.c` was later edited IN PLACE with the gr32 fix before being forked, and `gr31.dll` was rebuilt from it — so neither is a usable pre-fix reference. Use gr32. |
| `gr32`–`gr38` | iterations on the late-DNF sweep | all superseded; see the version history in `gr39.c`'s header for what each one broke |
| `gr39`–`gr42` | iterations converging the late-DNF sweep | superseded; each one's failure is recorded in `gr44.c`'s header |
| **`gr43.c` / `gr43.dll`** | **KNOWN-GOOD.** duration-based sweep + revalidate-on-recovery | reverted to 2026-07-25 after gr44 was implicated in a crash. `COUNTDOWN_SEC` is **10** (debug value) in this build. |
| **`gr45.c` / `gr45.dll`** | **gr43 with `COUNTDOWN_SEC` 10 -> 30, and NOTHING else** | Deliberately isolates the countdown change from gr44's restart hold, so the two can be tested separately. **NOT injection-tested.** |
| **`gr44.c` → `redflag.dll`** | **THE CURRENT SOURCE. Edit this file, nothing else.** | See "Status 2026-08-02" below. `redflag.c` is GENERATED from it by `make_release.py`; editing `redflag.c` loses your work on the next regen. The 2026-07-25 "REVERTED — DO NOT USE" verdict on this file is **withdrawn**: the crash it referred to was never attributed (the sequence completed cleanly and `dnf_settled()` never engaged), and the file has since been driven repeatedly. |

### Status 2026-08-02 — current

**Build chain.** `gr44.c` is the only file to edit. Then:

```bash
python make_release.py          # gr44.c -> redflag.c (logging compiled out)
bash build_release.sh redflag.dll
```

**Hot-swap, no game restart** (added 2026-08-02). The DLL ejects itself on a
named event; never eject from outside — a `CreateRemoteThread(FreeLibrary)`
crashed the game (findings.jsonl DISPROVEN 2026-07-28).

```bash
python ../extended_vanilla_features/tools/dll_unload_signal.py redflag
```

`eject_teardown()` reverses **everything this DLL does outside its own module**:
governed cars, pit arms, sunk DNF cars, commentary flag, flag icon, both
call-site trampolines, and the two EXE sprite bytes at `0x4D8338` / `0x4DBAE3`
(verify with a read-back of `0x41`). Then it waits 300 ms so a call already
inside a trampoline can finish. The trampoline pages are deliberately NOT
freed — unreachable once the call sites are restored, and freeing them is the
one thing that could still be jumped into.

**Keys.** NUMPAD1 = countdown restart · NUMPAD2 = field to pits, restart when
the player reaches his BOX · NUMPAD3 = pit-retire all AI (SHIFT reverts).

#### What changed on 2026-08-02

| Area | Before | Now |
|---|---|---|
| No-overtaking | `0x453C` speed-cap multiplier + per-tick rewrite of race position `0x4060` | **Both deleted — neither did anything to the cars.** `0x453C` is recalculated every frame by AIAbility; `0x4060` is the standings number the game recomputes from lap + lap distance, so the write only froze the timing screen. Replaced with the accepted `pace_probe.c` law through the game's own governor `sub_42ACD0(veh,N)` → `veh+0x4538`. |
| NUMPAD2 restart point | `pittingStatus == 2` | `>= 3`. **2 is the pit LANE; 3 is the BOX** (`PIT_STOP_FIRING_ORDER.md`, five live stops). The old test fired at pit entry. Fallback: `>= 2` and stopped under 0.5 m/s for 1.2 s, since a red-flag stop may never arm the crew. |
| Session end mid-sequence | `phase` never reset | Watchdog aborts on a dead session or a changed field identity. **One defect behind three tester reports**: persistent red flag in later races, dead NUMPAD2, and a stranded countdown firing `RESTART_FLAG` in an unrelated session. |
| Multiplayer | every client could press NUMPAD1, desyncing everyone | All hotkeys refused when `*(*(0x7CD25C)+16324) > 0`. Offline value measured 0; **fail-open**, so a wrong detector degrades to the old behaviour rather than locking you out offline. |
| PWS_RED01 | raw queue-head write, then the mod muted itself 50 ms later | Proven emitter path `FN_STRUCT_INIT` → `FN_DRIVER_LOOKUP` → `FN_INSERT_ENTRY 0x418560`, fired only into a free channel, confirmed by `EVENT_TS`. Suppression is held off until the line has been said. |
| Beeps | 7 `Beep()` calls incl. a per-second countdown tone | all removed |

**The no-overtake law** (ported from `patches/full_safety_car/dll/pace_probe.c`,
the 2026-08-01 version the driver accepted):

```
queue leader    -> the pace speed (30 m/s, ramped over 6 s)
every other car -> max(current, 0.5 s min) speed of the car PHYSICALLY ahead
                   + 0.22 * (gap - 20 m)
```

No term is independent of the car in front, so at the target gap a pass is
arithmetically impossible. Three guards, each a live regression before it
existed: an abreast car is an obstacle only to the one worse off in the
standings (mutual caps once deadlocked eleven cars at `maxSpeed 5`); the base
is `max(current, rolling min)`, never the sagging minimum alone; and floors of
60% of the car ahead and 45% of the pace. Cars self-release at
`pittingStatus >= 2`, where the game's own pit limiter owns the same field.

**Still open** (tester feedback 2026-07-27, triaged not built): VSC-only key,
`.ini` tunables (`COUNTDOWN_SEC` 30/15/10), penalties and DQ surviving the
restart, practice/qualifying support, and whether the replay survives a red
flag (nobody has checked).

### Status 2026-07-25

Confirmed in-game:

- Pre-existing DNF cars parked in their pit boxes, `QualPos=200`, hidden.
- Late-DNF sweep: cars wrecked DURING the countdown stay out; healthy cars that
  blip `act=0` are not falsely binned (gr43 duration + revalidate).
- **Grid reorder** (2+ laps, `reorder=YES(race)`).
- **NUMPAD2** full red flag.

`COUNTDOWN_SEC` is back to **30**. The loaded-banner string is hardcoded
separately and may not match if you change it.

### gr44 restart hold

`dnf_settled()` blocks `*RESTART_FLAG = 1` for up to `DNF_SETTLE_MAX_MS` (2s)
while any swept car still reads `fin=0 && ret=0`, then restarts anyway and logs
it. Applied to both the NUMPAD1 countdown path and the NUMPAD2 pit-box path.

Reason: placement at restart is decided by `FinishState`/`RetiredFlag`, so
firing while a wreck is still unclassified hands it a grid slot. gr43 avoided
this only by luck of timing — the 4s kill-confirm plus the rest of the countdown
happened to be enough. This makes the assumption explicit and bounded.

Give-up line to watch for:

```
DNF SETTLE: gave up after Nms with N car(s) still unclassified - they may be
gridded. Restarting anyway.
```

**gr44 is NOT yet injection-tested.**

### The model that actually explains DNF handling

Two INDEPENDENT things, repeatedly conflated during the gr32-gr43 debugging:

| what | decided by | effect |
|---|---|---|
| **Is the car placed at all?** | `FinishState`/`RetiredFlag` **at the instant of restart** | DNF => parked in its pit box / garage, no grid slot. Not DNF => gets a grid slot. |
| **Where in the grid order?** | `QualPos` | Only applies to cars that ARE being gridded. |

`QualPos=200` is **purely grid-numbering hygiene**: without it a DNF car keeps
its original qualifying number and the start classification hands it that slot
again. Setting 200 forces it to the tail of the ordering so the active cars take
1..N cleanly. **It has nothing to do with putting a car in the pits.**

So every "car at the end of the grid" failure had the same real cause: the game
did not consider the car DNF when the restart fired, so it gridded it — and 200
merely decided where in the order it landed. A `QualPos=198` in a log is the
game numbering a car it considers a RUNNER.

### Two signals, different latency — the root of every bug in gr32-gr43

| signal | latency | authority |
|---|---|---|
| `ActiveFlag` (`0x45F4`) — physics/visual kill | immediate, deterministic | none, it is a symptom |
| `FinishState`/`RetiredFlag` — bookkeeping | seconds, **variable** | decides placement |

Rule: **use the fast signal to decide WHEN to look and whether the kill is real
(persistence); let the slow signal be authoritative for what the game does.
Never assert the slow one yourself.** Same lesson as the pit-retire work — arm
the trigger, never forge the end state.

Failure modes, one per build, all from mishandling that gap:

- gr32-34 asserted the end state -> pre-empted the game's cleanup, wreck froze.
- gr35 classified early but deferred `QualPos` -> game numbered it itself (198).
- gr37 waited only for the game's verdict -> too strict, fresh wrecks missed.
- gr41 trusted ONE `act=0` sample -> false-positived a healthy car (Heidfeld).
- gr42 undid a sweep whenever the game had not confirmed YET -> raced a
  variable-latency classifier and put a real wreck (Hakkinen) back on the grid.
- gr43 commits on PERSISTENCE (`act=0` for `KILL_CONFIRM_MS`=4s) and only undoes
  cars that actually RECOVERED -> decoupled from the classifier's latency.

**Latent race still present in gr43:** nothing enforces that swept cars have
reached `fin`/`ret` before `*RESTART_FLAG = 1`. It works because the 4s confirm
plus the remaining countdown gives the game time, but a car wrecked very late
could still be gridded. Fix would be to hold the restart up to ~2s while any
swept car still reads `fin=0 && ret=0`.

### Hard-won facts about DNF handling (do not re-derive)

- **A crashed car is NEVER teleported to a garage by the game.** That only
  happens on the pit-retirement path (NUMPAD2/NUMPAD3). Wrecks are removed by
  `re_retire_dnf()` setting `ActiveFlag=0` — which **ends the physics step** —
  followed by `hide_dnf_cars()` moving scene node + physics Y to `-1000`.
  Physics off FIRST, then the teleport sticks.
- **Forcing the retirement end-state during the countdown pre-empts the game's
  own cleanup** and strands the wreck on track (gr32–gr34).
- **Deferring `QualPos=200` to the end of the countdown lets the game grid the
  car itself** at `QualPos=198`, a value this mod never writes (gr35). Write it
  at classification time, as `gr17_CONFIRMED` does.
- **Classifying only on `FinishState`/`RetiredFlag` is too strict** — a fresh
  wreck is physics-dead long before the game classifies it (gr37).
- `QualPos=198` in a log means the GAME placed the car, not this mod.

Inject with `inject_gr32.bat` (`inject.bat` is stale — it points at
`mods\red-flag-dll\inject.exe`, which does not exist in this corpus; the working
injector is `mods\ai_pit_retire\inject.exe`).

## gr32 — late-DNF sweep

`snapshot_positions()` classifies DNF-vs-active **once**, at trigger time. A car
wrecked *during* the 30s countdown therefore stayed in `g_active` and was handed
a grid slot, so it reappeared on the restart grid instead of staying out.
Reported in-game 2026-07-24.

`sweep_late_dnf()` now runs every tick in both wait windows (phase 1 countdown
and the phase 10 NUMPAD2 wait). It walks `g_active`, and any car that has since
gone terminal is:

- appended to `g_dnf_ptrs` (so `apply_qualpos()` gives it `QualPos=200` and
  `re_retire_dnf()` pins it in phase 3),
- pinned immediately (`FinishState=2`, `RetiredFlag=1`, `ActiveFlag=0`) so it
  cannot un-DNF before the restart,
- compacted out of `g_active` / `g_active_rp` / `g_locked_racepos` together, so
  `enforce_positions()` keeps working on aligned indices.

Terminal = `ActiveFlag -> 0` (the same signal the auto-detect uses), or an
explicit `FinishState`/`RetiredFlag` already set by the game.

Two exclusions, both deliberate:

- **the player** — never removed from his own restart grid, however wrecked.
- **cars armed by NUMPAD2** (`g_pitret_ptrs`) — they are retired *on purpose*
  and get un-retired before the restart; sweeping them into `g_dnf_ptrs` would
  strand them out of the race they are supposed to rejoin.

## AUTO RED FLAG IS DISABLED (since gr34)

`#define AUTO_REDFLAG_ENABLED 0`. The code is preserved, just not called.

It had **never once fired** before gr33: `veh_killed()` misread `ActiveFlag` as a
float, so `detect_seed()` seeded every car as already-killed and there was never
a `false->true` edge. gr33 fixed the read, the feature came alive for the first
time, and immediately mis-fired — a grid RESTART transiently drops several cars
to `ActiveFlag=0`, which is indistinguishable from a 3-car pileup inside the 8s
window. A red flag auto-triggered right after the previous one finished and left
AI stuck on the grid.

Re-enabling needs a detector that ignores restart transients: gate on
`GAME_STATE`/`RACE_PHASE` being settled racing, require the kill to persist for
N consecutive polls (`kill_duration()` in gr43 already does this), and arm the
cooldown on MANUAL triggers too — only `check_auto_redflag()` sets one today,
which is why NUMPAD1 did not protect itself.

## gr31 hotkeys

| key | what |
|---|---|
| **NUMPAD1** | Timer-based restart — 30s countdown, speed cap, no-overtaking lock, then grid restart. Unchanged from gr30; still auto-fires on a 3-car DNF pileup within 8s. |
| **NUMPAD2** | **Full red flag.** Snapshots the grid, sends every AI to the pits via the retire trigger, then **restarts automatically the moment the player drives into his own pit box**. SHIFT+NUMPAD2 aborts and puts the field back. 5-minute timeout auto-aborts if the player never pits. |
| **NUMPAD3** | Pit-retire every AI car and leave it there (no restart). SHIFT+NUMPAD3 reverts arms that have not fired yet. |

### How NUMPAD2 works

1. `snapshot_positions()` runs **first**, while the whole field is still active —
   so the restart grid is the running order at the moment of the red flag.
2. `pit_retire_all()` arms every AI car. They drive to their boxes and the game
   retires them (garage placement included).
3. Phase 10 waits on `player_in_pit_box()` — the player's `pitting` (+0x3450)
   reaching `2`, the same "in the box" state verified on AI cars.
4. On arrival: `unretire_pitret()` clears the retirement on exactly the cars we
   retired, then `apply_qualpos()` + `adjust_laps()` + `RESTART_FLAG = 1`.

Two deliberate design points:

- **The cars we retire never enter `g_dnf_ptrs`**, because the snapshot happens
  before they retire. So `re_retire_dnf()` in phase 3 leaves them alone and they
  come back on the grid. Only genuinely pre-existing DNFs stay retired.
- **We clear the retirement flags ourselves rather than trusting the restart to
  revive them.** The restart does appear to reactivate DNF cars (that is why
  `re_retire_dnf()` and the anti-reactivation hooks exist at all), but that has
  never been verified for a whole field, and being wrong means restarting alone.

## NUMPAD3 — pit-retire (new in gr31)

Retires AI through **the game's own retirement path** rather than forging a
retirement end-state. Two writes per car, then hand off:

```
veh+0xD58  = 0   (byte)   RearWingIntactFlag -> armed
veh+0x454C = 1   (int32)  callToBox
```

The car drives itself in; at the pit box `PitStopBlockingCondition_0042aee0`'s
damage term `*(veh+0xD18) && !*(byte*)(veh+0xD58)` goes true, and
`PitStopDecisionTireCompound_0042a3d0` retires it — `RetiredFlag=1` +
`ActiveFlag=0` in one tick, **plus the garage teleport**.

Player is exempt (compared against `PLAYER_VEH_PTR`), as are cars already
retired and any car whose `0xD18` record pointer is NULL.

### Why this is better than what the mod already does

`re_retire_dnf()` forges `RetiredFlag=1 / ActiveFlag=0` directly, which strands
cars at the pit box (PhysX ~10.25) instead of the garage (PhysX ~17.0) — and
that is precisely why `hide_dnf_cars()` has to shove them to `Y = -1000`.
Cars retired via NUMPAD3 need neither hack.

`re_retire_dnf()` is deliberately **left as-is**: it re-pins cars that were
already DNF *before* the red flag, during the restart. Those cars cannot drive
to a pit box, so the pit path does not apply to them.

### Behaviour to expect

Retirements **cascade** — the flag is only a trigger, and each car retires when
it reaches its own box. On lap 1, with the field near pit entry, the whole grid
goes within ~60 s (`pitting 0 -> 2`, skipping state 1). Mid-lap it takes a lap
or two. `reason` stays 0 and `carConditions` stays 0: a clean retirement with no
damage cosmetics, since nothing was actually detached.

SHIFT+NUMPAD3 reverts arms that have not fired yet. Once the game has retired a
car the write is spent — the game resets the flag to 1 itself.

### Evidence

Mechanism, live traces and the failed-hypothesis retraction:
`investigations/part_detach_commentary/README.md`.
Offsets are CONFIRMED in `name_index.json`.
Standalone equivalent (no DLL): `tools/force_wing_retire.py`
(`arm <slot>`, `arm-all`, `status`, `revert-all`).

In-game confirmed 2026-07-24 via the standalone tool: 21/22 cars retired, every
one placed at PhysX 16.91-17.15 with per-team Z spacing, player untouched.

**Not yet verified for gr31.dll specifically:** the DLL compiles and the writes
are identical to the proven tool, but it has not been injected and driven in a
race. Treat NUMPAD3-in-DLL as untested until it is.
