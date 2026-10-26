# Bulk-work instructions — DeepSeek V4 Pro

You are doing the **volume** work on this project. A stronger model (Opus) audits
your output afterwards. That review is cheap only if you make your reasoning
checkable — so the single most valuable thing you produce is not code, it is a
**paper trail that shows where each number came from**.

Read first, in this order:
1. corpus root `CLAUDE.md` — the trust model and what every data store contains
2. this folder's `CLAUDE.md` — the local loop and the "STOP, the tool exists" list
3. `docs/WORK_CATEGORIES.md` — your queue, bucketed by effort
4. `docs/TRIAGE_NOTES.txt` — the human's own intent per clip, verbatim

---

## The one rule that outranks everything

**Numbers come only from extraction or live observation. Never from you.**

Offsets, addresses, thresholds, byte values, enum values, event IDs: read them
out of a file or out of live memory with a tool, and cite which. If you find
yourself typing a hex number you did not just read from somewhere, stop — that
is the failure mode this whole corpus is built to prevent.

Tiers: `FACT` (poke-proven live) > `CONFIRMED` (two independent sources) >
`DERIVED` (grounded but unproven) > `GUESS`. You may write `DERIVED` and
`GUESS` and `OPEN` and `DISPROVEN` freely. **You may not write `FACT` or
`CONFIRMED`** — those require evidence a reviewer must see. Propose them
instead: `"proposed_tier": "CONFIRMED", "needs_review": true`.

---

## Before starting any queue row

Run these three, every time, no exceptions:

```bash
python tools/ask.py findings <keyword>
python tools/ask.py offset <hex>        # for each offset you intend to touch
grep '"off": "0x____"' persistence_map.jsonl
```

**Read the findings output in full. Never pipe it through `head`.** Results are
not ordered by relevance, and a later entry routinely supersedes an earlier one —
a question recorded as "still unobserved" in one entry is often closed as FACT
two entries down. A reviewer on this project already truncated a 7-result
`findings SessionType` search to the first two, missed the entry that closed the
question, and filed a false-positive review against correct work. Same rule for
`ask.py offset` and `ask.py global`.

Reason: `findings.jsonl` already contains **DISPROVEN** entries for this exact
commentary system. Someone already poked the wrong tyre offsets and proved they
do nothing. Re-deriving a settled answer has cost a full session more than once.
`persistence_map.jsonl` tells you whether a poke can even hold (`LATCHED`) or is
overwritten every frame (`RECALCULATED` — poke is useless, hook the writer).

If prior art answers the row: record that, close the row, move on. That is a
win, not a shortcut.

---

## Never write these offsets

`veh+0x10C0` and `veh+0x33C8` kill the process instantly — the write succeeds
and the next read fails. Both are in `tools/lab.py` NEVER_WRITE. Do not "test"
them. Do not sweep them.

---

## Per-tier recipe

### T1 (reuse existing mechanism, 8 rows)
The note names an existing script that already detects the condition. Your job
is wiring, not discovery. Read that script, extract the actual detection
predicate (not your paraphrase of it), and write the wiring plan. Do not
redesign the detection.

### T2 (simple new condition, 14 rows)
One check against data that is already exposed. Find the field via
`tools/ask.py offset` or `config_schema.json`/`data_index.json`. Confirm it is
`LATCHED` before proposing a poke-based approach. State the exact comparison.

### T3 (needs a tracker, 95 rows — the bulk of your work)
Mechanism is understood; it needs state across frames (timer, counter, flag).
For each row produce a spec, not code:
- which field(s) are read, with offset + tier + how you verified them
- the exact trigger predicate, in terms of those fields
- what state must persist between frames, and where it lives
- which event ID to inject, from `data/event_clip_map.json` (never invented)
- the cooldown/priority from `data/full_event_dump.json` for that eid
Batch these. Many T3 rows share one tracker (all the gear-loss lines, all the
penalty lines) — say so explicitly, because it collapses ~10 rows into 1 hook.

### T4 / T5 (35 rows)
**Do not attempt these.** Write the open question sharply, list which corpus
store you already checked and what it did *not* contain, and escalate. A
well-framed unknown is your deliverable here.

---

## What you may and may not modify

| Action | Allowed |
|---|---|
| Append to `findings.jsonl` (tier ≤ DERIVED) | yes |
| Write new analysis docs in `docs/` | yes |
| Change clip status via `tools/voice_browser.py` or `progress.set()` | yes |
| Add a note to a clip's line in `docs/TRIAGE_NOTES.txt` | yes — then `python tools/triage.py sync` |
| Hand-edit `docs/TODO.md` | **no — generated from TRIAGE_NOTES.txt, your edit is lost on the next run** |
| Hand-edit `docs/PROGRESS.md` or `docs/WORK_CATEGORIES.md` | **no — generated** |
| Hand-edit anything under `patches/<feature>/` | **no — generated by `tools/build_pack.py` from `manifest.json`** |
| Edit fact tables, `skeleton_facts_all.json`, `name_index.json` tiers | **no** |
| `python tools/patch.py add ...` (records a patch, does not touch exe) | yes |
| `python tools/patch.py apply ...` (writes to the exe) | **no — ask** |
| Write to live game memory | **only when explicitly asked this session** |
| Anything under `F1C_ghidra_project/tools/` | **no — deprecated copies** |

Regenerate, don't hand-write:
```bash
cd mods/extended_vanilla_features/tools
python -c "import progress; progress.load().save()"   # PROGRESS.md
python categorize_work.py                              # WORK_CATEGORIES.md
```

---

## Output contract (this is what gets reviewed)

Per row you touch, hand back:
```
CLIP / SDT index / EIDs
verdict:      one of  WIRED | NEEDS_TRACKER | BLOCKED | PRIOR_ART | UNKNOWN
evidence:     file:line or tool command, for every number quoted
offsets:      offset -> name -> tier -> persistence class
trigger:      the predicate, in terms of named fields
uncertainty:  what you are NOT sure about, named explicitly
```
The `uncertainty` line is mandatory and must not be empty. An empty one reads as
overconfidence and the row gets sent back.

---

## Stop and escalate, don't guess

- A name in `name_index.json` conflicts with what the code actually reads
- Two stores disagree (e.g. an enum documented one way, observed another)
- A field is `RECALCULATED` and the note assumes a poke will hold
- You need a live process, a debugger, or a new address resolved
- The row is T4/T5
- You are about to write the same hex number twice from memory rather than
  re-reading it

---

## Caveats specific to this project — read before you trip on one

**1. A name is a label, not proof of who reads it.** `name_index.json` has four
offsets named `tirePuncture_FL/FR/RL/RR`. They are real, they are `CONFIRMED`,
and they are **not** what the blister-commentary code reads — that lives on a
different per-wheel object at `this[0x394]`. Poking the well-named field did
nothing for a whole session. Always find the *reader* of a field before
building on the field's name.

**2. Attaching a debugger perturbs a physics game.** Starting the CE/x64dbg
debugger pauses and steps the process; the game can register that hitch as a
physics event (a tyre going black, a spin). If something changes in-game right
after you attach, **do not assert you changed nothing** — you attached a
debugger, that is a change. Read the relevant memory and report what it says
rather than defending a claim.

**3. Truncated SDT names collide.** SDT clip names are cut to ~15 chars, so
`PWS_AIMECHPROB0` covers several distinct lines (lost 1st gear, lost 2nd, stuck
in 1st, stuck in 2nd…). `data/progress.json` is keyed by clip name, so those
distinct features are currently **merged into one row** with a combined note.
Nine triage lines collapsed into two rows this way. Do not treat one row as one
feature without checking `sdt_index` against `data/full_event_dump.json`.

**4. The triage file's leading number is an SDT index, not an event ID.** They
overlap numerically and both look plausible. Cross-check against a line whose
identity is unambiguous before bulk-processing anything keyed on those numbers.

**5. `PROGRESS.md` / `WORK_CATEGORIES.md` are generated and will silently eat
your edits.** Also: `python progress.py` prints counts but does **not** save —
running it and seeing the right numbers does not mean the markdown regenerated.

**6. `SHIPPED` means vanilla-only — and triage-sourced statuses are still
provisional.** `SHIPPED` is defined as *heard in normal vanilla gameplay, no
mod*. The first triage merge got two rows wrong by treating any "EXISTS" line as
SHIPPED; both are now **fixed** (sdt 135 `"EXISTS VIA ANOTHER MOD"` → `DONE`,
sdt 162 `"EXISTS, double check"` → `WIP`), and `apply_triage_notes.classify()`
now guards both cases. The general lesson stands: the `note` field carries the
human's original words and **outranks the status** — read it before trusting a
row, especially any SHIPPED row whose note hedges.

**7. `categorize_work.py` resolves "same as above" by iteration order** (sorted
by `sdt_index`), while the human wrote "above" meaning *file order*. Those
mostly coincide — they are not guaranteed to. A row whose whole note is "same as
above" inherits its tier from a neighbour and may be in the wrong bucket.

**8. `tools/patch.py`'s manifest is resolved, not hardcoded — check which one is
live.** Order: `$F1C_PATCH_MANIFEST` → `mods/extended_vanilla_features/patches/
manifest.json` → `mods/patches/manifest.json`. `patch.py status` prints the
resolved path on its second line; read it before trusting that a patch is or
isn't registered. If you need an isolated manifest for another mod, set the env
var — do not hardcode a new path (a stale hardcoded path is exactly how one
mod's patches previously ended up in another mod's folder).

**9. Patch VAs were read from a modded install.** The exe is
`F1 Challenge V10\F1 Challenge Online.exe`. Byte patches recorded against it
will read as `FOREIGN` on a different build — which `patch.py` correctly
refuses, so heed that refusal instead of forcing it.

**10. Console encoding lies.** This is a Windows shell; em-dashes and box
characters in these UTF-8 docs render as garbage in command output. The files
are fine. **Never rewrite a file to "fix" characters you only saw garbled in a
terminal** — you will corrupt working content.

**11. No locking on `data/progress.json`.** Do not run two writers at once
(e.g. `voice_browser.py` open while a script calls `progress.save()`);
last write wins and silently discards the other.

**12z. `data/full_event_dump.json` is a SNAPSHOT of live memory and goes stale
the moment a byte patch is applied.** It once claimed 26 blocked events when only
16 were blocked live; the 10 phantom-blocked ones had already been fixed and
shipped. Two specs consequently recommended re-applying patches that were already
`[applied]`. **Never** conclude "blocked by a FLT_MAX cooldown" from that file
alone. Confirm both ways first:
```bash
python tools/patch.py status          # [applied] means the fix already shipped
python tools/poke.py read 0x005BF9A0+<eid>*0x14+0x0C:float   # live cooldown
```
`registered in manifest.json` ≠ `applied to the exe`. If a row's blocker turns
out to be already fixed, the verdict is `PRIOR_ART`, not a patch recommendation.

**12a. Session/distance globals — settled, cite these, do not re-derive.**
- `SessionType` `0x7C56A4`: **5 = qualifying, 7 = race** (both FACT, live-proven).
  Ignore the enum in `docs/38-confirmed-offsets.md` — it is wrong about
  qualifying. Do not confuse `0x7C56A4` with `0x7C56A8` (RacePhase); both can
  read 5 at different times, meaning different things.
- `TotalRaceLaps` `0x8ACF8C`: **post-reduction, truncated** (FACT — a 53-lap race
  at 50% distance reads 26, not 27). Use it directly, apply no scaling.
- `lapNumber` `veh+0x403C`: 0 on the grid pre-start; CONFIRMED, LATCHED.

**12b. A correct number you cannot cite still fails review.** Two specs on this
project reached right conclusions from evidence that did not support them (a race
guard value guessed correctly; a race-distance claim argued from a *qualifying*
code branch). Both were then only settled by a live read. Cite the store or the
observation, or mark the line as an assumption in `uncertainty` — an uncited
number is unreviewable even when it happens to be right.

**12. `flt_7CF3C8 != 0.0` and `dword_7D0308` gate the commentary emitters.**
Commentary can be enabled in the audio options and still not fire because the
session/race-active guard is unset. A silent clip is not proof of a broken
trigger — check the guards before concluding anything about the emitter.
