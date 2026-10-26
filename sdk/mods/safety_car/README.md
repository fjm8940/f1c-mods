# safety_car — NUMPAD1 safety car

> ## SUPERSEDED 2026-07-31 — read `mods/extended_vanilla_features/patches/full_safety_car/PACE_PROBE.md`
>
> A working safety car now exists there, driver-verified end to end. This folder
> is kept for its research; **do not build on `sc1.c` without reading the
> corrections below.**
>
> | this folder said | what was proven live |
> |---|---|
> | PLACEMENT is an open question | **Answered: retirement does NOT prevent grid placement.** Tested with the DLL loaded before the field existed — retired, AI off, driver hidden, planned position last — and the game still placed him in grid box 23. Retirement controls state, never placement. |
> | `QualPos=200` keeps him off the grid, cost is a blanked grid listing | **It is an out-of-bounds write.** Placement is `sub_4D10E0` → `sub_403CD0(gridTable, plannedPos - 1, ...)`, which indexes `v4[7 * a2]` unchecked — 200 reads 1393 floats past the grid array. The "blanked listing" was reading garbage. **Do not use it.** |
> | the field cap (`CAP_MS 41.67`) is a poke on `maxSpeed` | Superseded: call the game's own governor `sub_42ACD0(veh, N)` and re-arm it, because the game occasionally writes `99999` back. |
> | `revive()` sets `driverStatus = 3` (approach) | A car that never started is in **grid-hold with brakes pinned at 1.0** and cannot be revived at all — `deretire_probe.py`'s full write set left it stationary. The recipe works on a **garaged** car, not one that never started. Never retire a car before the green flag. |
> | the score-swap audio trick (`CARRIER_EID 0x83` + `SCORE_SCAR*`) | **Correct, and now proven live** — with carrier eid 132. It generalises to any TV-only or orphaned clip. |
>
> The genuinely reusable parts of `sc1.c`: resolving Maylander **by name**, the
> garage-exit write set, `send_to_box`, and the 5 s delay between slowing the
> field and releasing him (he was wiped out without it).

**Status:** DLL BUILT, NOT YET INJECTION-TESTED (2026-07-25). Every underlying
mechanism is separately proven; the assembled sequence has not completed a race.

| file | what | status |
|---|---|---|
| `sc1.c` / `sc1.dll` | **the mod.** Enforces Maylander's retirement every tick, NUMPAD1 deploy/recall | compiles clean, never injected |
| `inject.bat` | injects `sc1.dll` via `../ai_pit_retire/inject.exe` | untested |
| `safety_car.py` | external prototype, same logic. Useful for probing (`status`, `prep`, `release`) | ran live, failed, defects fixed |

## Why the DLL exists

`safety_car.py` had to land the retirement in the window between the driver
structs existing and the grid being placed — an external process cannot hit that
reliably, and missing it puts a retired car physically on pole. The DLL instead
**enforces the retirement every tick**, so grid formation, restarts and session
changes all see a retired car and the timing stops mattering.

`NUMPAD3` toggles `QualPos=200` enforcement at runtime, because which placement
behaviour is correct is still an open question (see PLACEMENT below).

## PLACEMENT — the open question

Enforcing "retired" controls his STATE, not where the game parks him. A retired
car can still be placed on the grid: on the first live run he sat on **pole**,
retired and inactive, blocking the slot.

| approach | keeps him off the grid? | side effect |
|---|---|---|
| retired only (default) | **UNTESTED** | none known |
| retired + `QualPos=200` (NUMPAD3) | YES — P1 slot empty, parked at his garage | blanks the pre-race grid listing entirely |
| let him start, then arm the genuine retire trigger | not tried | he runs part of a lap first; but the game garages him itself, which is the placement we know is revivable |

Try the default first; if he appears on the grid, hit NUMPAD3 before the session
loads and accept the UI cost.

## Use

```
mods\safety_car\inject.bat
```

Then in the race: NUMPAD1 deploys, NUMPAD1 again recalls. Log: `sc1_log.txt`.

The Python prototype is still useful for probing:

```bash
python mods/safety_car/safety_car.py status     # live safety-car state
python mods/safety_car/safety_car.py release    # panic: un-cap the whole field
```

Start it with the game already in a **race** (SessionType 7). It retires
Bernd Maylander into his garage immediately, then polls NUMPAD1:

| press | what happens | audio |
|---|---|---|
| 1st | Maylander revived, drives out of the pit lane under his own AI. Field capped to 150 km/h, positions frozen, player untouched. | `SCAR01` on deploy, `SCAR02` when he clears the pit limiter |
| 2nd | Called to the box, retires again via the game's own path. Cap released. | `SCAR03` on recall, `SCAR05` before the box, `SCAR04` when he is in |

`status` prints the safety car's live state. `--no-retire` skips the start-of-race
retirement. Ctrl-C always restores the field.

## Why it is built this way

**Maylander is resolved BY NAME, never by slot index.** Enabling him made it a
23-car grid and shifted every other driver up one slot (he is slot 1; M.Schumacher
moved 1 -> 2). Any hardcoded index would silently drive the wrong car.

**`0x4538` maxSpeed is in m/s, not km/h.** Every car reads `22.22` sitting in the
garage, which is the 80 km/h pit limit / 3.6. So the cap is written as `41.67`.
Writing `150` would be a ~540 km/h cap, i.e. no cap at all.

**Retire at the start is SYNTHETIC (forged end-state), recall is GENUINE (armed
trigger).** These are deliberately different:
- At the start he is already sitting in his garage, so freezing him in place puts
  him exactly where a real retirement would, and the synthetic case is the one
  proven easiest to revive.
- On recall he is out on track, so forging the end-state would freeze him on the
  racing line. Arming `0xD58=0` + `callToBox=1` makes the game drive him in and
  garage him itself. Arm the trigger, never forge the result.

**The revive is the full garage-exit recipe**, including `0x4354 = 0`. Skipping
that is what drove M.Schumacher into the garage wall — a stale waypoint is only
re-queried if the car is >57 m from it. See
`investigations/garage_exit_ignition/README.md`.

**Commentary uses a carrier event.** The five `PWS_SCAR` clips exist in
`PC_RE_MAIN.SDT` (indices 137-141) but **no event in the table resolves to them**,
which is why they never play in the stock game and why `event_catalog.py` has no
SCAR entries. `TableLookupByIndex_004144c0` re-reads the score from the event
table by event ID (`dword_5BF9B0[5*eid]`), so writing a score into the queue entry
does not steer the clip. The script repoints carrier event `0x83`'s score, injects,
and restores it. Scores verified live against the PWS lookup:

| clip | score | SDT idx |
|---|---|---|
| PWS_SCAR01 | 132 | 138 |
| PWS_SCAR02 | 133 | 139 |
| PWS_SCAR03 | 134 | 140 |
| PWS_SCAR04 | 135 | 141 |
| PWS_SCAR05 | 131 | 137 |

Carrier `0x83` is PWS_RED01, proven injectable in `mods/grid_reorder/gr18-gr28`.
Both a RED01 control and a repointed SCAR01 were **confirmed audible in-game
2026-07-25**. Prototype: `tools/sc_audio_test.py`.

## First live run, 2026-07-25 — what it taught us

The run FAILED, and the failure was instructive. Sequence: `prep` was applied in
the pre-race, then NUMPAD1 was pressed **while still in session 6 (warmup)**.

1. **`QualPos=200` DOES exclude him from the grid** — the P1 slot was empty and he
   was placed at the end of the grid, immobilised. It also **blanked the pre-race
   grid listing entirely**, so it is now opt-in (`prep --qualpos`).
2. **Deploying outside a race session poisons the car permanently.** The revive
   wrote `retired=0 / finish=0 / active=1`; the game zeroed `ActiveFlag`; that
   combination IS the limbo state. Maylander was unrecoverable for the rest of the
   session.
3. **Limbo is confirmed terminal, and more strongly than before.** Three separate
   recoveries were tried on the same car and all failed within 1.5 s:
   direct revive; re-retire to a clean `retired=1 active=0 finish=2` and then
   revive; and restoring a valid `QualPos` (23) first. Re-establishing the
   canonical retired signature is NOT enough - something else is latched.
4. **The speed cap outlived the failure.** The 45 s timeout moved the state machine
   to ON_TRACK and kept the cap, so 21 cars sat at 150 km/h after the safety car
   had failed to move, and the cap survived the session change.

Fixes applied after that run:

- **Deploy is refused unless `SessionType == 7`.** This alone would have prevented
  the whole failure.
- **`FieldControl` is keyed by SLOT, not by raw vehicle pointer**, and re-resolves
  each tick. A session change reallocates the vehicle structs; the old build kept
  writing through stale pointers into freed game memory every 50 ms.
- **Session-change detection** releases the field and returns to IDLE.
- **The deploy timeout now ABORTS** - releases the cap and logs the car's state -
  instead of pretending the safety car got out.
- **Saved `maxSpeed` is sanity-checked.** Snapshotting a car sitting in the garage
  would memorise the pit limiter (22.22) as its "original" and restore it to a
  permanent 80 km/h cap. Anything below `CAP_MS * 1.5` is treated as `99999`.
- **`release` panic command** un-caps the whole field if a controller is killed
  mid-deploy.

## Untested / known risks

1. **The whole sequence in a race.** Built and syntax-checked against a session-6
   garage state only.
2. **The recall.** `send_to_box` relies on Maylander accepting `callToBox` from
   wherever he is on track. Proven for ordinary AI cars; not yet for a revived one.
3. **Re-deploy in the same race.** After a recall he is garaged with a stale
   waypoint — the Schumacher signature. The revive nulls `0x4354`, which is
   exactly the fix, and that path was confirmed on Frentzen, but not twice in one
   session.
4. **Position lock vs. the pit lane.** `racePos` is frozen for the whole field
   while the safety car is out, including for anyone who pits.
5. `SCAR02` fires on a speed threshold (>25 m/s), so it is track-independent but
   will not fire if he stalls in the pit lane. There is a 45 s fallback.

## Next step

A DLL port (`gen_hook.py` + the `grid_reorder` tick-DLL skeleton) if you want it
running without an external Python process. The playbook's rung order says prove
the writes live first — that is what this script is for.
