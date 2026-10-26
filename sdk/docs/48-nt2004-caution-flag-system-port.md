# 48 — The NASCAR caution system and what it would take to port it into F1 Challenge

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`) · sibling binary: NASCAR Thunder 2004 (`NASCAR_Thunder_2004.exe`, same engine lineage, 2003 build)
**Date:** 2026-07-31 · **Author:** corpus session · **Status:** STATIC ONLY

## Abstract (TLDR)

F1 Challenge has flags in name only. It shows blue flags to lapped traffic and
draws a yellow marshal panel when a car is slow in a track sector, but there is
**no race control**: no full-course yellow, no pace car, no restart line, no
field order, and no red flag. The red-flag *physics* (AI driving to an assembly
point and stopping) exists but nothing ever triggers it.

Its sibling game, NASCAR Thunder 2004, was built from the same engine a year
later and ships a **complete, working caution system** — a "steward" object that
scans the track for incidents, throws yellow flags, bunches the field behind a
pace car, holds the field, restarts it on a double-file line, and issues
Stop/Go, pass-through and lap-deduction penalties to anyone who passes under
yellow. It also has a dormant red-flag path.

This paper maps that system (from the NT2004 binary) and lays out, in
dependency order, exactly what a port into F1 Challenge would touch. It is a
static blueprint: nothing here has been poke-tested in a running F1C yet.

A modder's one-line summary: **the caution system is six cooperating machines —
a steward, a command bus, a pace-lap state machine, per-car flag modes, a
field-ordering engine, and a penalty queue — and F1 Challenge already owns two
of the six (a per-car speed lever and the red-flag bunching physics).** The
rest is new code, but the game's own data files even carry the leftover `.aiw`
token the system was designed around.

**Correction vs earlier drafts (2026-07-31): F1 Challenge does NOT have
NT2004's command bus.** F1C's `sub_424CD0` is a *per-car, synchronous* command
queue (capacity 10, event-driven ~15 Hz); NT2004's `sub_4D0D00/4D0D50` is a
*global, async* 208-byte ring. A live port project already exists in this
corpus (`mods/extended_vanilla_features/patches/full_safety_car/`, with a
staged plan at `PLAN.md` and live-proven results: the whole-field speed lever
`sub_42ACD0(veh, N)` holds 21 cars at 30 m/s for ~143 s). Read that project
before building anything on this paper's port surface.

## 1. Introduction

### 1.1 The research question

F1 Challenge 99-02 and NASCAR Thunder 2004 are the same engine. The shared
MSVC `__FILE__` paths baked into both binaries prove it (`steward.cpp`,
`hdvehicle.cpp`, `tire_manager.cpp`, `ai_db.cpp` in both), and 51% of NT2004's
functions trace to an F1C twin. But one game has a full course-yellow/safety-car
system and the other does not. This paper answers: **what exactly is the
system, and which parts could a port reuse?**

### 1.2 Scope

- In: NT2004's steward/flag/caution/pace-car/red-flag machinery, its
  overtaking-and-speed enforcement, and the F1 Challenge surfaces each piece
  would land on.
- Out: commentary clips, HUD artwork, the NASCAR-specific Chase/points layer.

### 1.3 Why it matters

F1 Challenge modders have already *tried* safety cars and red flags twice —
once each — and both attempts hit the same wall: a field cannot be restrained
by poking speed values that the game recomputes every frame. NT2004 contains
the finished version of the machinery those mods were reaching for. Knowing
where it lives, how it enforces, and what F1 Challenge already provides is the
difference between a 200-line poke mod that fails and a port that holds.

## 2. Method: static analysis

Two corpora were examined, both produced by the same extraction tooling (IDA
headless Hex-Rays decompilation, per-function deterministic facts, and a full
string-literal index with xrefs):

1. **NT2004** — `NASCAR_Thunder_2004.exe` (image base 0x400000). The
   caution system's functions were found by their announcement strings
   ("Yellow Flag: …", "Red Flag Detected", "Full Caution", "Illegally Passed
   …", "Lapped Field Under Yellow", "Stay Behind the Pace Car"), then each
   function was read in full and its callers/callees followed.
2. **F1 Challenge** — the F1C corpus, cross-checked for which surfaces already
   exist (command queue, red-flag physics, yellow *display*, infraction list).

All addresses below are NT2004; all tiers are labeled. Nothing is a guess, but
semantic readings of decompiled code are DERIVED until poke-verified in a
running game.

## 3. Method: runtime verification

**PENDING.** The NT2004 side has not been poke-verified in-game (it is the
*reference*). The F1 Challenge side has partial prior runtime evidence — the
red-flag physics assembly-point behavior and the F1C command queue are both
live-proven in this corpus (FACT). Planned experiment for the port: force a
caution trigger in NT2004, watch `dword_75CBB0`/`dword_75CBB4` walk their
phases live, and confirm the pace-line order command and the penalty commands
land on the shared command bus.

## 4. Results

### 4.1 The NT2004 caution system at a glance

Six cooperating layers, all driven from the same per-frame session tick:

| # | Layer | Functions | Role |
|---|-------|-----------|------|
| 1 | **Steward (flags)** | `sub_4F99C0`, `sub_4F9670`, `sub_4F93E0`, `sub_4F9920`, `sub_4F95D0` | detects incidents, throws flag states 0–8 |
| 2 | **Command queue** | `sub_4D0D00` (enqueue) → `sub_4D0D50` (dispatch) | 208-byte ring; flag/pace commands ride as type-4 sub-commands |
| 3 | **Pace-lap state machine** | `sub_4FB270`, `sub_42F950` | 9 substates: catch field → reposition → pits open → countdown → green |
| 4 | **Per-car control** | `sub_4F8D30`, `sub_4F9F50`, `sub_4FA900`, `sub_4DC260` | per-car flag mode (0 green / 1 caution / 2 red / 3 restart) + target speed |
| 5 | **Field object** | `sub_4E29B0`, `sub_4EDDB0`, `sub_4EEAE0` | the pace car = a normal roster vehicle named "pace car", fielded at slot 48 |
| 6 | **Enforcement** | `sub_4DF490`, `sub_4DFF10`, `sub_4DF420`, `sub_4DC690`, `sub_4DC900` | overtaking/speed infractions → penalties + lap deductions |

Key session globals (all extraction-tier):

| Global | Meaning |
|--------|---------|
| `dword_75CBB0` | race phase 0–8 (5=racing, 7=caution/pace-lap, 8=red) |
| `dword_75CBB4` | pace-lap substate 0–8 |
| `dword_75CBA8` | pace-lap phase |
| `dword_7D3C44` | command-queue write pointer |
| `dbl_7DA9D0` | race clock (seconds) — every delay is a `>=` comparison against it |
| `dword_75AAD0` | array of active car pointers |

### 4.2 Flag states and the steward

Flag-state enum (steward field, the `a2` of `sub_4F99C0`):

| val | meaning |
|-----|---------|
| 0 | green — "Green Flag: Resume Normal Racing" |
| 1 | yellow detected, pits closed — "Yellow Flag: Detected (Car #%s), Pits Closed" |
| 2 | yellow, pits closed |
| 3 | pits open to lead-lap vehicles |
| 4/5 | pits open to all |
| 6 | prepare to restart |
| 7 | green pending — "Withholding Green Flag…" / "Green Flag: May Pass On Outside" |
| 8 | **red** — "Red Flag Detected" |

The steward's per-frame brain is `sub_4F9670`, a state machine over those
values: on a trigger it decides **full vs quickie caution** (`sub_4F95D0`
counts cars needing attention across the field; ≥ n+3 → full), then walks
pits-closed → pits-open (only once the field clears the pit entrance,
`sub_4FA660`) → prepare-restart (once enough caution laps elapse) →
green-pending (only once the field is bunched, `sub_4FA1A0`) → green.

**The caution trigger itself is `sub_4F93E0`** — the *incident scanner*. Every
frame it rotates a window over the field and flags a car if:
- it is stopped (speed < 0.1),
- it is damaged (a per-car damage counter > 0), or
- it is off-track: moving yet the signed distance from the racing line
  exceeds 2.0 track units (`sub_51C730`), or its forward-progress scalar is
  tiny (`sub_51C630`).

Weather is a second, independent trigger: when rain intensity crosses 0.18
the weather manager calls the public "throw a caution" API (`sub_52B1D0` →
`sub_4F9D50`).

### 4.3 How a yellow is *enforced* — the two layers

This is the answer to "does the game actually stop cars overtaking under
yellow?" — yes, in two distinct ways:

**Layer A — physical restraint (prevents).** When the steward throws a yellow
it sets `dword_75CBB4` to caution, and `sub_4F9F50` pushes **per-car flag
mode 1** to every car. In parallel the field-ordering engine `sub_4FA900`
rebuilds the running order into a **pace line** and writes a doubly-linked
list — each car's `+0xB0`/`+0xB4` prev/next pointers — via command 0x07.
Every car's target speed becomes the recomputed pace-lap speed
(`sub_4FB270`/`sub_4DC260`, written per frame). The field literally cannot
gain on the line because everyone is target-locked to the same speed and
ordered by position.

**Layer B — detection + penalty (corrects).** `sub_4DF490` runs per frame per
car. It compares the car's waypoint-scalar position (`v41`) against the
pace-target scalar (`v42`). If the car is beyond the pace target it has
*passed*, and:
- a **left-side pass** is detected geometrically: `sub_4DFF10` takes the
  displacement vector between the two cars and dot-products it against the
  track's local right axis; a positive dot means the pass happened on the
  left → "Passed On Left During Caution" → **Stop/Go penalty**,
- a **lapped car** passing the line gets **lap deduction** instead —
  `sub_4DF420` does `car.laps -= gap` and announces "%d Laps Also Deducted",
- at green-pending and the restart, "Illegally Passed Before Pit Entrance /
  Before Finish / Before Green" fire the same penalty pipeline.

The same tracker enforces **speed under yellow**: an accumulator that passes
9.5 s of "driving too slow" (relative to the pace target) earns a penalty, and
"accelerated too fast" past the line earns one too (`sub_4DF490`).

**The penalty pipeline:** `sub_4DC690` builds the message and posts a
command-queue entry (sub-command 0x17, penalty type byte + value byte). The
dispatcher runs `sub_4DC900`, which accumulates into the car's penalty object
(Stop/Go, Pass-Through, Start-At-End-Of-Longest-Line; a second Stop/Go adds
15 seconds). Serving is a pit action — and the pit menu refuses to serve a
Stop/Go under yellow ("Cannot serve stop/go under a yellow flag!", `sub_4C19D0`).

### 4.4 The pace-lap state machine

`sub_4FB270` is the field's per-frame driver during the caution:

| substate | meaning |
|----------|---------|
| 0 | caution called; timer runs, then cars → mode 1 |
| 1 | catching the field; **if the leader gap exceeds 900 units → red flag** (mode 2) |
| 2 | field repositioned into the pace line (teleport + steer) |
| 3 | pits open; player free to pit |
| 4 | countdown to green — 5-4-3-2-1, then go |
| 5 | restart — cars released to race speed |
| 6 | caution active |
| 7 | **red flag** |
| 8 | late restart |

The green release is a single call (`sub_4FAFF0`): set the session's restart
speed, call the per-car speed setter for every car, post the restart command,
"Green Flag". The restart is double-file in the NASCAR sense — the ordering
engine keeps a left/right line, and the flag text says exactly what is allowed:
"May Pass On Outside".

### 4.5 The red flag

NT2004's red flag is **machinery without a live trigger** — the same state F1
Challenge's red-flag physics is in, one level more complete. Flag state 8
(`sub_4F99C0` case 8) sets substate 7 and per-car mode 2; the pace-lap driver
holds the field there. Two automatic paths reach it:

1. leader gap > 900 units during field-catch (`sub_4FB270` substate 1), and
2. rain during the caution window (`sub_4F9670` state 4/5 → `sub_4F9920(8)`
   when the weather flag reads true).

But path 2 is gated on `sub_460A50()`, and that function **is a one-line stub
that returns 0**. So in NT2004, as shipped, the red flag never fires
automatically. The red *display/behaviour* machinery exists; the trigger does
not. F1 Challenge is identical in kind — the waypoint-guidance physics that
drives the field to an assembly point exists and is live-proven, but no
condition ever calls it.

### 4.6 What F1 Challenge already owns (the reuse surface)

| F1C asset | status | tier |
|-----------|--------|------|
| **Per-car command queue** `sub_424CD0` | live-proven, **per-car & synchronous** (capacity 10, event-driven ~15 Hz, cmds 0/1/4/5/14/15). **NOT** NT2004's global async ring — caution commands have no host; only the per-car layer maps | FACT |
| **Whole-field speed lever** `sub_42ACD0(veh, N)` (veh+0x4538 maxSpeed) | live-proven: holds 21 cars at 30 m/s ~143 s, zero overrides | FACT |
| **Red-flag bunching physics** (waypoint guidance → field assembles and stops) | live-proven, never triggered | FACT |
| **Maylander (the real F1 safety-car driver)** already in the roster | used by the existing safety-car mod | FACT |
| **`.aiw` leftover token `ultracarefulyellow`** | shipped in F1C track files, **no parser reads it** — the data hook the yellow AI was designed around | extraction |
| **Yellow flag *display*** — 29 marshal zones, a car slow in a zone turns it yellow | works, cosmetic only, read-only per frame | FACT |
| **Five driving infractions** (Cut Track, Speeding In Pitlane, Ignored Blue Flags, Wrong Way, Lap Will Not Count) | F1C's entire rule book | FACT |
| **Flag Rules race option** | exists, governs blue flags only; no "Black Only" enforcement level | CONFIRMED |

### 4.7 The port, in dependency order

> **Correction (2026-07-31):** F1C has **no session-wide command bus**. NT2004's
> caution commands (`0x15` begin-pace, `0x16` set-flag, `0x26` green-restart)
> cannot be ported as commands — the steward/flag layer must become **DLL-side
> state** on F1C, and only the per-car layer maps onto `sub_424CD0`. The live
> port project `mods/extended_vanilla_features/patches/full_safety_car/`
> (`PLAN.md`, Phases 0–6) is the working plan; this list is the static version.
>
> **Correction 2:** the old "`veh+0x453C`/`0x443C` recalculated per frame" claim
> is **disproven for `0x443C`** — it is a one-shot gap-recompute request, cleared
> on every `AIRacingLine` exit. `veh+0x4538` maxSpeed is not recalc-fragile: the
> whole-field hold is FACT.

1. **Steward flag machine** — port `sub_4F9670` + `sub_4F99C0` + `sub_4F9920`
   + `sub_4F93E0` + `sub_4F95D0` as **DLL-side state** on F1C (no command-bus
   host). This is the decision brain; everything else hangs off it.
2. **Pace-lap substate driver** — `sub_4FB270` + `sub_42F950` + `sub_4FA660`
   + `sub_4FA1A0`. F1C's session-state analogue is still open (locating it is
   `full_safety_car` PLAN Phase 0).
3. **Per-car flag modes + field ordering** — `sub_4F8D30`, `sub_4F9F50`,
   `sub_4FA900`, `sub_4F9040`, the order comparators. The per-car lever =
   `sub_42ACD0(veh, N)`, **gate every call on `dword_7C56A8 == 5`**.
4. **Pace car vehicle** — F1C already has the roster entry (Maylander); needs
   the slot-48 fielding + `PaceCarReleaseDist` track config.
5. **Track config** — `PaceCarReleaseDist` in the `.aiw` (two floats in
   [0,1], default 0.80/0.87); F1C's `.aiw` parser is `sub_402690`.
6. **Enforcement** — port the geometry check + penalty queue, or reuse F1C's
   existing penalty pipeline and add the yellow-specific cases.
7. **Options plumbing** — an `AllowAutoPace`-style session flag + a driving
   aid bit (NT2004: driving-aid bit 11).

**Do not port** the poke-based approach. F1C's earlier safety-car mod capped
the field via `veh+0x4538` and (in v1-v2) appeared to fail because the field
wouldn't hold — but the live 2026-07-31 work proved `veh+0x4538` **does** hold
when written through `sub_42ACD0` on the game thread (the old failure was the
external poke, not the field). The correct path is call `sub_42ACD0` per car,
not write `veh+0x4538` directly and not NT2004's command-queue machinery.

## 5. Discussion

- **Tier honesty:** every semantic reading here is DERIVED until verified
  against a live NT2004 run. The two layers of yellow enforcement (prevent via
  pace-line, correct via geometry check) are both grounded in decompiled code;
  which one a given player perceives is runtime work.
- **`sub_460A50` stub:** the rain→red path is dead in NT2004 as shipped. That
  is a *feature* for a port — it means NT2004's red flag is closer to F1C's
  than either game admits: full behaviour, no trigger.
- **Yellow display vs yellow control:** F1C's existing 29-zone yellow *table*
  is rebuilt from scratch every frame and only *means* "a car is slow here".
  It is a read-only observable. It is not a caution, does not stop anyone, and
  cannot be forced by a poke. The port does not build on it — it replaces the
  meaning of "yellow" entirely.
- **Scale of the port:** the six layers are ~25 NT2004 functions. F1C's live
  port project (`full_safety_car/PLAN.md`) has already proven the two hard
  runtime pieces (the speed lever holds the field; `sub_424CD0` is per-car).
  What remains is DLL-side steward state + per-car speed control — bigger than
  the grid_reorder DLL, but the same shape.

## 6. Conclusion

NT2004 contains a complete course-yellow/pace-car/red-flag system. Overtaking
under yellow is enforced by *prevention* (pace-line speed control) plus
*correction* (a per-frame geometry check that converts passes into Stop/Go
penalties and lap deductions). Red flag is dormant in both games. F1 Challenge
has the per-car speed lever (live-proven), the bunching physics, Maylander and
the orphaned `ultracarefulyellow` `.aiw` token — but **not** a session-wide
command bus, so the steward/pace layers become DLL-side state. The missing
pieces are the steward brain, the pace-lap state machine, and the enforcement
geometry check.

## Appendix A — Corrections to prior notes/tools

- **Superseded claim:** this paper's earlier draft said F1C's command queue
  "is the same bus" as NT2004's. **Wrong** — F1C's `sub_424CD0` is per-car and
  synchronous; NT2004's is a global async ring. Corrected in §4.6/§4.7.
- Earlier safety-car prior art in this corpus ("correct path = hook the
  writers or the AI command queue (cmd 0xe)") is partially overturned by live
  work: cmd 0xe **does** write `veh+0x443C`, but `0x443C` is a one-shot gap
  recompute, not a following mode; the real lever is `sub_42ACD0` (veh+0x4538).
- The note that F1C "has no yellow at all" was **too strong**: F1C has a
  29-zone yellow *display* table (per-zone slow-car flags). It has no caution
  *control*. This paper corrects that wording.

## References

- NASCAR Thunder 2004, `NASCAR_Thunder_2004.exe`, 2003 retail build
  (image base 0x400000); full caution map in the sibling corpus
  (`SAFETY_CAR_MAP.md`).
- F1 Challenge 99-02, `F1 Challenge Online.exe`, 2002 build; this corpus.
- Sibling paper: `investigations/yellow_flag/README.md` (F1C's yellow display
  table). Prior mods: `mods/safety_car/`, `mods/red_flag/`.

---
*Changelog: 2026-07-31 — initial (static only; NT2004 reference side not yet
poke-verified).*
