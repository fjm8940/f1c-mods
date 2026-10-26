# Full Safety Car — project

**Status: WORKING SAFETY CAR, driver-verified 2026-07-31.** Press one key and
the race engineer calls it, the field slows and forms a queue in race order
without overtaking, and Bernd Maylander drives out of the pit lane, catches the
field and paces it — then goes back in and the race resumes. Built in
`dll/pace_probe.c`, documented in **`PACE_PROBE.md`**.

Phases 1, 3, 4 and 5 PASSED. What is missing is the caution *decision* (what
declares a caution automatically) and the *restart* procedure — caution laps,
"prepare to restart", the bunching check and the countdown to green (Phase 6).
This folder is the design
for a *real* safety car in F1 Challenge 99-02: full-field caution, pace car
vehicle, field bunching and restart — ported from **NASCAR Thunder 2004**, which
ships a complete implementation on the same engine lineage (2002 vs 2003 MSVC
build, 51% of NT2004 functions crossmatch F1C's).

| file | what |
|---|---|
| **`PACE_PROBE.md`** | **START HERE** - the working caution engine: control laws, offsets, traps, controls |
| `FONT_MAS_ICONS.md` | how to add a custom HUD icon (e.g. an SC badge): CUBEMAS4.10 format, `tools/font_mas_tool.py`, paint recipe |
| `HANDOFF_2026-07-31.md` | cold-start state as of the Phase 1/5 live work |
| `README.md` | this cover: goal, what exists already, what is missing, plan |
| `DESIGN.md` | the NT2004 reference implementation, mapped function-by-function (extraction-tier facts) |
| `PLAN.md` | the staged F1C port: build order, live-verification gates, RESULT blocks per phase |
| `dll/` | `pace_probe.c` — the built caution engine, plus the scaffold for the remaining modules |

## What exists in F1C already

- **Yellow flags** — the game raises a yellow on incidents and has flag-rule
  options, but there is **no caution logic, no field bunching, no pace car**.
  Verified: zero "yellow/pace/caution" strings in F1C's `strings_index.json`.
- **Red-flag physics, unwired** (`mods/red-flag.md`): `DAT_008acfd4/c8/d8` +
  waypoint-guidance make AI drive to an assembly point and stop. Never connected
  to a trigger. The closest native primitive to NT2004's field-bunching.
- **An AI command queue, but a per-car one** (verified 2026-07-31,
  `AICommandMessageQueueDispatcher_00424cd0`): `sub_424CD0(veh, cmd, param)`,
  buffer `veh+0x44A8` / capacity `+0x44AC` / index `+0x44B0`, flat 8-byte
  `{cmd, param}` entries, **synchronous** — it appends, drains, and resets in the
  one call. Commands `0/1/4/5/14/15`. **cmd `0xe` (14) sets `veh+0x443C = 1`** —
  the offset the legacy fake-SC mod poked and could not hold, so this is its
  legitimate writer.
  **It is NOT NT2004's mechanism.** NT2004 rides its flag/pace commands on a
  *global* async 208-byte ring (`sub_4D0D00`/`sub_4D0D50`, `dword_7D3C44`). F1C
  has no session-wide bus at all — see `DESIGN.md` §3. The per-car layer of the
  port maps onto `sub_424CD0`; the steward/flag layer has no host and must be new
  DLL state.
- **Maylander** — F1C's safety-car driver is a real roster entry, proven in the
  legacy `mods/safety_car/`.

## Legacy vs this project

`mods/safety_car/` (corpus root) is the **fake** safety car: retire Maylander,
revive on demand, cap the field by poking `veh+0x4538 maxSpeed`. It has no
caution state, no bunching, no ordering, and the poke cannot hold
(`veh+0x453C/0x443C` recalc every frame — the note there already says the
correct path is *"hook the writers or the AI command queue (cmd 0xe)"*). That
is exactly the machinery this project ports from NT2004.

## The system to port (summary — full map in `DESIGN.md`)

NT2004's caution is five cooperating layers, all driven from the per-frame
session tick:

1. **Steward / flags** — `sub_4F99C0` (flag states 0-8), `sub_4F9670` (caution
   decision machine, full-vs-quickie), `sub_4F93E0` (incident scanner),
   `sub_4F9920` (posts the flag via the command queue).
2. **Command queue** — enqueue `sub_4D0D00` → dispatcher `sub_4D0D50`; flag and
   pace commands are type-4 sub-commands (0x07 order, 0x15 begin-pace, 0x16
   set-flag, 0x26 green-restart).
3. **Pace-lap state machine** — `sub_4FB270` (substates 0-8: catch field →
   reposition → pits open → countdown → release), `sub_42F950` (phase setter),
   `sub_4FA1A0` (field-bunched check), `sub_4FA660` (pits-open check).
4. **Per-car control** — `sub_4F8D30` (per-car flag mode), `sub_4FA900` (field
   ordering), `sub_4DC260` (pace speed), computed pace speed per frame.
5. **Pace car vehicle** — a roster entry named `"pace car"` fielded at grid slot
   48 (`sub_4E29B0` + `sub_42E180`), plus the `.aiw` track token
   `PaceCarReleaseDist`.

Plus: manual-pace player control (`sub_4FB930`/`sub_4FA450`), yellow infraction
penalties (`sub_4DF490`), "stay behind the pace car" HUD (`sub_4DCEB0`),
rain-triggered caution (`sub_52B1D0`), `AllowAutoPace` / driving-aid bit 11.

## Port plan at a glance

See `PLAN.md` for the staged detail. Order of operations:

1. **Locate F1C equivalents** of the NT2004 integration points (command queue
   dispatcher, steward object, race-phase global, dynman field setup, `.aiw`
   parser, driver roster).
2. **Stage A — flag machine on the existing queue** (probe-first): reuse the
   queue; prove a flag command round-trips like NT2004's 0x16.
3. **Stage B — field bunching** via the red-flag waypoint-guidance primitive
   retargeted (the cheap win — F1C already has "AI drives to a point and stops").
4. **Stage C — pace car vehicle** from Maylander + a roster/name check.
5. **Stage D — per-car pace control + ordering** (the largest new code).
6. **Stage E — player control, penalties, HUD, config.**

Every stage has a live gate (game running, `tools/poke.py` / Frida / the DLL
scaffold in `dll/`) before the next stage is started. Nothing here is a
manifest byte-patch — this is per-frame logic and ships as DLLs like
`patches/mechanical_failures/`.

## Context a cold-start LLM needs

- **F1C corpus** (static analysis, ground truth): `%USERPROFILE%\Desktop\f1c_ida_decompiled` —
  read its `CLAUDE.md`; everything goes through `tools/ask.py`, `tools/poke.py`,
  `functions/*.c`, `findings.jsonl`. Do NOT re-derive an offset that corpus already
  answers.
- **NT2004 reference corpus**: `%USERPROFILE%\Desktop\nt2004_ida_decompile` — `functions/*.c`
  hold the ground-truth text behind every address in `DESIGN.md`.
- **Project conventions**: this folder (`extended_vanilla_features/CLAUDE.md`) — DLL rules,
  self-eject pattern, eid-isolation-before-wiring, findings logging. The `mechanical_failures`
  patch folder is the structural precedent for a multi-DLL feature.
- **Prior art to not repeat**: `mods/safety_car/` (fake SC, poke-based cap that cannot
  hold), `mods/red-flag.md` (unwired red-flag physics, the bunching primitive to reuse).

## Resolved

- **F1C command queue** = `sub_424CD0` (`AICommandMessageQueueDispatcher_00424cd0`),
  per-car and synchronous, commands `0/1/4/5/14/15`. See above and `DESIGN.md` §3.
- **F1C `.aiw` parser** = `sub_402690` (`PaceCarReleaseDist` token absent — add there).
- **NT2004 race-phase setter** = `sub_42F6C0(&dword_75CBA8, phase)`; the five
  `0x75CBA8..B8` "globals" are one session-state struct (`DESIGN.md` §2a).

## Open questions (resolve before building)

- F1C's race-phase global (NT2004 `dword_75CBB0 == 7` = in caution) — find the
  F1C session-state *object*, not five separate addresses.
- F1C's steward object / per-frame session tick hook site.
- Where the session-wide flag state lives, given F1C has no global command bus.
- Whether NT2004's `sub_4F93E0` scanner is the only first-caution trigger.
- Exact runtime meaning of NT2004 `dbl_7DA9D0` (race clock) and
  `dword_75CBAC` bit0 (manual pace) — verified live before porting the gates.
