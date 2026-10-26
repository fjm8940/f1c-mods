# Parallel worker rules — EVF-003 through EVF-007

Several workers are running **at the same time** on this folder. These rules are
what keep them from destroying each other's output. They outrank convenience and
they outrank your own judgement about what would be tidier.

## 1. File ownership — write ONLY your own files

| Assignment | May create/modify | Domain |
|---|---|---|
| EVF-003 | `docs/OBSERVER_TABLE.md`, `docs/SPEC_EVF-003_*.md` | the 97 T3 rows, observer collapse |
| EVF-004 | `docs/SPEC_EVF-004_*.md` | pit-stop procedure & pit failures |
| EVF-005 | `docs/SPEC_EVF-005_*.md` | mechanical failure, damage, oil, stall |
| EVF-006 | `docs/SPEC_EVF-006_*.md` | AI behaviour, pace, incidents, team orders |
| EVF-007 | `docs/SPEC_EVF-007_*.md` | penalty & stewarding **mechanism discovery** |

Nobody else's spec file. Nobody else's observer table. If you believe another
worker's row belongs in your group, **say so in your uncertainty section** and
leave their file alone.

## 2. Never touch these (all workers)

- `data/progress.json` — status changes go through `tools/voice_browser.py`, human-driven
- `docs/PROGRESS.md`, `docs/WORK_CATEGORIES.md`, `docs/RESTORATION_PLAN.md` — generated
- `patches/manifest.json` — register via `tools/patch.py add`, never hand-edit
- the exe, any `.dll`, any live-memory **write**

## 3. `findings.jsonl` is append-only and shared

It is at the corpus root. Multiple workers appending concurrently can interleave
and corrupt a line. So: **do not append to it directly.** Write your findings to
`batch/evf_pending/<your-assignment-id>.jsonl`, one JSON object per line, same
schema. A human merges them. Every entry: tier <= DERIVED, `needs_review: true`.

## 4. The live game is a single resource

One process, one debugger attachment, one race state. You cannot all read it at
once, and a session in qualifying cannot answer a race question.

- **Read-only always.** No `poke.py write`, no `patch.py apply`, no injection.
- Before you run any live read, check `docs/LIVE_LOCK.md`. If it names another
  assignment, you do not have the game — emit a `LIVE READ REQUEST` block and
  move on to work that doesn't need it. If it says `FREE`, put your assignment id
  in it, do your reads, set it back to `FREE`.
- `LIVE READ REQUEST` blocks are the normal path, not the fallback:

```
LIVE READ REQUEST
  precondition: <exact session state, e.g. "wet race, AI pitting">
  commands:     <exact poke.py lines>
  distinguishes: <what each possible value would mean — state this BEFORE seeing it>
```

The last line is mandatory. A prediction written after the read is worthless.

## 5. Do not re-derive settled facts

| Thing | Value | Tier |
|---|---|---|
| `SessionType` `0x7C56A4` | 5 = qualifying, 7 = race | FACT, live |
| `TotalRaceLaps` `0x8ACF8C` | post-reduction, truncated | FACT, live |
| `lapNumber` `veh+0x403C` | 0 on grid pre-start; per-car | CONFIRMED, LATCHED |
| `RaceActiveFlag` `0x7D0308` | 3 while on track in a race | FACT |
| `flt_7CF3C8` | commentary gate, must be != 0.0 | CONFIRMED |
| Injection audio | plays **PWS (race engineer)**, not TV | RESOLVED |
| `PWS_RETIRE01/02/04` | handled by `dll/retire_commentary_v5.dll`, no spec needed | DONE |
| `RE_PD_DQ` | fires in vanilla gameplay, no code | DONE |

`docs/38-confirmed-offsets.md` is **wrong** about qualifying. Ignore its enum.
Do not confuse `0x7C56A4` (SessionType) with `0x7C56A8` (RacePhase).

`data/full_event_dump.json` was regenerated 2026-07-28 (26 -> 16 truly-blocked).
If your reasoning rests on an older copy, redo it.

## 6. Constraints from the working DLL — read `dll/README.md`, mandatory

Every predicate any of you specs will run inside that engine. Its six traps are
your constraints. In particular:

- **Never fire on a transition that coincides with session teardown.**
  `FinishState -> 2` injects fine and is never audible. Find an earlier signal and
  state the window in seconds.
- The commentary queue is a **single contested slot**. If your line must beat the
  game's own chatter, say so explicitly — the priority lever at `qhead+0x1C` is
  **unverified**.
- A **false positive is worse than silence** under a once-per-race latch: it eats
  the real event. Prefer a `cancel_condition` over an eager trigger.
- Re-validate arm conditions at fire time. Latched state goes stale.
- `lapNumber` going backwards is the **only** reliable race-restart signal.
  `SessionType` and `RaceActiveFlag` do not change on a restart.

## 7. One DLL, one hook site

Six of ten EVF-002 categories independently chose `MainGameTickUpdate`. Assume
everything you spec runs in **one shared per-frame observer engine**. Do not
propose a second tick redirect on the same call site; do not propose your own DLL.
Spec the predicate, not the plumbing.

## 8. Output contract (all assignments)

Per group, in your own spec file:

- verdict per clip: `WIRED | NEEDS_TRACKER | BLOCKED | PRIOR_ART | UNKNOWN`
- every number cited to a store, a `functions/*.c` file:line, or a live read you ran
- **non-empty uncertainty** section — mandatory
- unbuildable rows listed separately with the search you already did.
  **Never invent an offset to fill a cell.**
- reclassification is welcome, with evidence (a T5 that collapses to T1 is a win)
