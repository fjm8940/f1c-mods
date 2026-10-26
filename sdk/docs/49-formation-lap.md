# 49 — The formation lap: how the PLR setting becomes a parade lap, a grid, and a green flag

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)
**Date:** 2026-08-02 · **Author:** corpus session · **Status:** STATIC + PARTIAL RUNTIME EVIDENCE (prior sessions)

## Abstract (TLDR)

- "Formation Lap" in the player file (`.plr`, `[ RACE CONDITIONS ]`) is a per-session-mode
  on/off switch (0 or 1), one copy each for CURNT / PRACT / QUICK / GPRIX / CHAMP / MULTI / RPLAY.
  The stock game ships every copy at **0 (off)**.
- When switched on, the game releases the whole field onto the track during the
  pre-race presentation phase (RacePhase 2, "grid ready"). Every car is driven by the
  game itself along the AI path — the player included — in grid order, single file,
  at parade pace. This is the formation lap.
- The lap is not a timed racing lap. It ends when the phase timer expires and the
  game's own state decides between two endings: **green flag** (lights out, race) or a
  silent "cooldown" transition that skips the green moment entirely.
- Which ending happens is controlled by a single memory flag (SessionInitFlags,
  `0x7CEE88`). It is the least-understood link in the whole chain: the only code that
  writes it clears it, and live traces show sessions starting both ways. See §5.
- The "Recon" settings are a separate but related family: reconnaissance laps before
  the session (pits open/closed windows, on-screen timer). The recon-timer setting
  also controls whether the game prints "Begin Formation Lap" on screen at the green
  flag — with it off, the formation happens silently.
- Modders should know: car positions during the formation are owned by the game's
  per-frame AI path tracking — external writes lose at 60 fps. The presentation can be
  skipped by forcing the phase deadline, but the green-flag gate decides whether you
  land in a race or a cooldown.

## 1. Introduction

**Research question:** what does the `Formation Lap` setting in the player file
actually do in-game, through what mechanism, and where does it interact with the
reconnaissance settings next to it?

**Scope:** in — the `.plr` keys, their parse path, the session/race state machines
that consume them, the per-car behavior during the lap, the HUD/audio, and the known
failure modes. Out — multiplayer grid ordering details beyond the formation release,
and the AI pace model (`41-raceability-mechanism.md`, `42-ai-realism-and-difficulty-model.md`).

**Motivation:** the setting is shipped everywhere at 0 and community lore about it is
thin. Two prior mod projects (red-flag restart, grid reorder, safety car) all had to
work around the formation/presentation machinery without a written map of it. This
paper is that map.

## 2. Method: static analysis

Full Hex-Rays decompilation of the game binary (~7,100 functions) plus a string
literal index. The player-options parser was found by its own key strings
(`"Formation Lap"`, `"Recon Pit Open"`, `"AI Follow Grid"`, …); every function that
references those keys or the session-flag globals was read in full. The race-phase
machine was already partially traced in sibling papers (`41-racephase-skip.md` is a
sibling research note in this corpus) and was re-read against the raw decompile.

Data sources in the game: the player profile `.plr` file, section `[ RACE CONDITIONS ]`.

Evidence tiers: code-read results are DERIVED unless marked CONFIRMED (two
independent sources) or FACT (observed live in a running game).

## 3. Method: runtime verification

**PENDING for the formation-specific chain.** The static path has not been driven
end-to-end in a live session with the setting on. Designed experiment (2–3 lines
when it runs):

1. Set `GPRIX Formation Lap="1"` in the player file, start a grand-prix race.
2. Watch, at 10 Hz: `RacePhase` (`0x7C56A8`), `GameState` (`0x7C569C`), the green gate
   `0x7CEE88`, the FormationLap byte `0x7CEBC4`, the phase deadline `0x7C5750`.
3. Record which ending fires (phase 3 green vs phase 4 cooldown) and whether
   `0x7CEE88` ever goes nonzero — that is the paper's central open question.

**Partial runtime evidence already on record from prior sessions (cited inline):**
RacePhase timing traces (docs/15 state machines, docs/41 live test 2026-07-22),
red-flag restart flows (GameState 4→5→6→7, mods/red_flag_v2), and the safety-car
project's grid-hold observations (brakes pinned at 1.0 before lights out).

## 4. Results

### 4.1 The player-file keys and their defaults (FACT — file + decompile)

The player-options builder parses `[ RACE CONDITIONS ]`. Two families:

**Per-mode family** — seven copies, one per session mode. The mode prefix
(`CURNT`, `PRACT`, `QUICK`, `GPRIX`, `CHAMP`, `MULTI`, `RPLAY`) is generated at
parse time; each entry holds default / current / min / max:

| Key (×7 modes) | Default | Min | Max |
|---|---|---|---|
| `Reconnaissance` | 0 | 0 | 12 (laps) |
| `Grid Walkthrough` | 1 | 0 | 1 (MULTI ships 0) |
| **`Formation Lap`** | **0** | **0** | **1** |
| `Flag Rules` | 1 | 0 | 1 |
| `Weather` | 0 | 0 | 5 |
| `TimeScaledWeather` | 1 | 0 | 1 |
| `Failure Rate` | 2 | 0 | 3 |

**Scalar family** — one copy each:

| Key | Default | Min | Max |
|---|---|---|---|
| `AI Follow Grid` | 1 | 0 | 1 |
| `Recon Pit Open` | 300 s | 30 | 900 |
| `Recon Pit Closed` | 150 s | 30 | 900 |
| `Recon Timer` | 1 | 0 | 1 |
| `Race Timer` | 0 | 0 | — |

### 4.2 Where the values land (DERIVED, code-confirmed)

The six scalar settings are represented as "session-flag" objects; each object holds
a **pointer to a global**, and the object's methods write through that pointer. That
indirection is why a static search for writers finds none — the write is invisible.

| Flag slot | Global | Setting |
|---|---|---|
| 1 | `0x7CEB64` | Recon Pit Open |
| 2 | `0x7CEB7C` | Recon Pit Closed |
| 3 | `0x7CEB94` | Recon Timer (pit-timer visible) |
| 4 | `0x7CEBAC` | (likely AI Follow Grid) |
| 5 | **`0x7CEBC4`** | **Formation Lap** (byte) |
| 6 | `0x7CEBDC` | Pit Window |

Related globals written at session setup: `0x7CEC30` / `0x7CEC54` (pit open/closed
seconds), `0x7CEC78` (recon-timer visible, gates the "Begin Formation Lap" message),
`0x7CEC90` / `0x7CED8C` (restart-path flags), and **`0x7CEE88` (SessionInitFlags,
the green-light gate)**.

### 4.3 The race-phase machine (DERIVED; phase timing CONFIRMED live)

`RacePhase` lives at `0x7C56A8` inside the game-state struct at `0x7C569C`.

| Value | Name | What happens |
|---|---|---|
| 0 | PRE_START | init, brief |
| 1 | countdown | start-lights countdown; **never observed live** |
| 2 | **GRID_READY** | **the formation lap + grid presentation happen here** (10–30 s observed) |
| 3 | GREEN_FLAG | lights out, jump-start check, race |
| 4 | pit window / cooldown | pit-open tracking; also the "skipped green" landing |
| 5 | FINISHING | post-leader finishing |
| 8 | DONE | session complete |

The phase machine's GRID_READY handler (pit-timer state machine, `0x4D2E50`,
case 2) — pseudocode:

```
enter: latch flag set, camera/state call, once
hold:  while (SessionTime < phase_deadline) do nothing
exit:  if (SessionInitFlags != 0) → GREEN FLAG (phase 3)
       else                      → COOLDOWN (phase 4, FLT_MAX deadlines set)
```

A per-frame steward dispatch (`0x4D4580`) runs the same gate during the phase:
`SessionInitFlags` set → drives the start-lights progress bar; clear → enqueues a
generic "advance phase" command ~3 s later.

The phase setter (`0x42D140`) is the only writer of RacePhase and prints the
**"Begin Formation Lap"** message when entering phase 3 — but **only when the recon
timer flag (`0x7CEC78`) is set**. Formation is silent with `Recon Timer=0`.

### 4.4 The game-state machine: releasing the field (DERIVED; GameState values FACT)

`GameState` (`0x7C569C`; live-confirmed 4=restart-in-progress, 5=formation/grid,
7=race running). The transition machine (`0x42CF10`):

| Transition | Action |
|---|---|
| → 2/3/4, or 4→5 | session setup |
| **5→6** | `if (FormationLap != 0 || RaceMode == 4)` → per-car release call — **this is the formation release** |
| 6→7 | per-car race-start call |
| → 7 (mp) | grid ordering + per-car grid-slot assignment |

Note the code-level quirk: **`RaceMode == 4` forces the formation release regardless
of the player setting.**

The session-start gate additionally runs the full session loop and marks the first
car "started" when `SessionType==5` and FormationLap is set — a branch whose exact
intent is unresolved (see §5).

### 4.5 What the cars do (DERIVED; per-frame overwrite FACT)

During GRID_READY (and on every phase transition into 0/2/4), each car runs a
per-vehicle tick (`0x4C6B60`) that includes **AI path-position tracking**
(`0x4EF7C0`): every car is placed on the AI racing path each frame — the field is
"on rails". The red-flag mod measured this loop overwriting car positions at 60 fps,
winning every race against external writes (FACT, live 2026-07). Before lights-out
the game holds the grid with brakes pinned (FACT, safety-car project: `brake == 1.0`,
RPM 0 is the grid-hold signature).

`AI Follow Grid` (default 1, per the setting's own comment) makes the AI drive in
grid order, single file, during recon/formation instead of racing positions.

Audio: a formation/race-start emitter (`0x41A3A0`, race-only) speaks the AI pit-
strategy lines — "1/2/3 stop strategy" — keyed off a per-car strategy count. A live
finding shows that count reading 3 for every car, which is the known "the third line
always wins" scripting artifact.

### 4.6 The recon connection

Recon laps are the pre-session window; formation is the pre-race lap. Shared
machinery: the pit open/closed windows (seconds), the on-screen pit-closure countdown
("Time until pit closes:" — shown only when the recon-timer flag is set), and the
same flag that shows the countdown also gates the "Begin Formation Lap" message.

## 5. Discussion

**Central open question — who sets the green-light gate?** `0x7CEE88` decides green
flag vs cooldown, yet the only static writer clears it at session load. Two live
traces of the same build disagree: one session went phase 2→3 (green, +15.4 s), one
went 2→4 (cooldown, gate read 0 on every sample). The writer is either outside the
decompiled corpus (vtable/pointer-alias write) or gated by a session-type/flag
combination not yet hit. Until a write-watch answers it, any mod that forces a green
flag must also hold this byte nonzero and verify.

**Other loose ends:** RacePhase 1 has never been observed (possibly dead code for an
unwired start-lights sequence); the "Begin Formation Lap" message only shows with the
recon timer on; `RaceMode==4` forces the release; the `SessionType==5 && FormationLap`
session-loop branch needs a live read of which session types actually reach it
(SessionType 5 is live-confirmed = qualifying).

**Threats to validity:** everything in §4.1–4.5 except the cited live observations is
DERIVED — read from code, not poked. The phase names come from live traces but the
formation-specific chain (setting on → parade lap → which ending) has not been
driven end-to-end with the setting enabled.

## 6. Conclusion

The formation lap is real machinery, well-built, and shipped switched off. It lives
in the pre-race presentation phase, is driven entirely by the game (AI path
tracking), and ends at a two-way gate whose "green" branch depends on a memory flag
the code never visibly sets. Turning the setting on in the player file is the player-
facing knob; the honest modding takeaway is that the formation lap itself works, but
the green-light gate is the fragile link — verify it before building a mod on it, and
use the recon-timer flag if you want the on-screen "Begin Formation Lap" message.

## Appendix A — Corrections to prior notes/tools

| Where | Old claim | Corrected claim |
|---|---|---|
| ghidra corpus `docs/06` session-flag table | slot 4 = FormationLap, slot 5 = PitWindow | slot **5** = FormationLap (`0x7CEBC4`); slot 4 = `0x7CEBAC` (likely AI Follow Grid) |
| ghidra corpus `docs/07` phase machine | phase 2 = "formation lap", phase 3 = "formation lap/grid" | runtime names per docs/15: phase 2 = GRID_READY (formation happens inside it), phase 3 = GREEN_FLAG |
| ghidra corpus `docs/07` | phase 4 = "green flag / race running" | phase 4 also serves as the "skipped green" cooldown landing when `0x7CEE88 == 0` |
| IDA corpus name_index | `0x7C569C` labeled "SessionType5 (5=server)" | live-confirmed GameState: 5 = formation/grid phase (red-flag mod traces) |

## References

- Game binary: `F1 Challenge Online.exe` (x86 PE32, non-ASLR, image base 0x400000)
- Sibling papers in this folder: `41-raceability-mechanism.md`, `42-ai-realism-and-difficulty-model.md`, `48-nt2004-caution-flag-system-port.md`
- Research note: `41-racephase-skip.md` (RacePhase 2 deadline/gate, live test 2026-07-22)
- Mod prior art: `mods/red_flag_v2/`, `mods/grid_reorder/`, `mods/extended_vanilla_features/patches/full_safety_car/` (grid-hold, per-frame position overwrite)
- F1CUI community offsets (`docs/26-f1cui-community-offsets.md`): RacePhase `0x7C56A8` confirmed as the presentation gate the community tool keys off

---
*Changelog: 2026-08-02 — initial (static + prior-session runtime evidence).*
