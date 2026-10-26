# WORKER R — extended_vanilla_features / evf_verify

You are worker R of 20. Your items are listed below; no other worker has
them. Read the role file **in full** before starting:

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

## Corpus root (run all tools from here)

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — write to this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_verify\output\worker_R.jsonl

One JSON object per line, one line per item, in the order listed. Append after
every item; never buffer the whole batch. The exact field schema is in the role
file's "Output format" section.

## Your shell is PowerShell 7, not bash

`grep`, `head`, `tail`, `awk` **do not exist**. Use:

    rg -n "pattern" --glob "*.c" functions/ | Select-Object -First 20

Do not pipe to `head`. Do not use `grep`. `rg` is available; the `--glob` form
above works, bare `functions/*.c` does not.

## The emitter search is ALREADY DONE — do not repeat it

`mods/extended_vanilla_features/data/emitter_index.json` is a deterministic scan
of every call to `DriverDataTableLookupAndTimeConversion` in all 7,138
decompiled functions. Read it; do not re-derive it with `rg`.

- `literal["<eid>"]` — call sites passing your eid as a constant. If your eid is
  here, the emitter EXISTS: use the file/line/func_addr given.
- `dynamic[]` — call sites where the eid arrives in a variable, each with
  `candidate_eids` (every literal assigned to that variable in that function).
  If your eid appears in some site's `candidate_eids`, that function is a
  **candidate** emitter — open the `.c` and confirm your eid's assignment is
  really reachable at that call.
- Your eid in neither ⇒ `why_silent: no_emitter` is supported.

**Never conclude `no_emitter` from your own `rg` search.** Most call sites pass a
variable, so a literal-only regex misses them. The index is the only sound
source. It is also why `TimeBasedMessageChain` emits eid 277 despite no literal
`, 277,` anywhere.

## Per item, do exactly this

1. `python tools/ask.py findings <clip>` and again on a keyword from the
   description. Quote any hit verbatim into `prior_art`. If a DISPROVEN or
   CONFIRMED finding already answers the item, record it and move on.
2. Look up the event in `mods/extended_vanilla_features/data/full_event_dump.json`
   under `events.<eid>`: read `score`, `sdt_index`, `clip_name`, `blocked`,
   `cooldown`. Compare `clip_name` to the row's clip **by prefix** — the field is
   truncated to 15 chars for some entries.
3. Look your eid up in `emitter_index.json` per the section above. If an emitter
   or candidate exists, open that `.c`, confirm the call is reachable for your
   eid, then run `python tools/ask.py func <func_addr>` for its caller count and
   guards. Those guards are the interesting answer: they are why a wired clip
   stays silent.
4. Decide `verdict` and `why_silent`. Every non-UNKNOWN field needs a line in
   `evidence`. Cite the index as
   `data/emitter_index.json literal.<eid> -> <file>:<line>` or
   `data/emitter_index.json dynamic <file>:<line> candidate_eids contains <eid>`.

Do not write to any file other than your output file. Do not touch live game
memory. Do not apply patches.

## Your 11 items


`emitter` column is precomputed from `emitter_index.json`. `none` means the eid appears at no call site, literal or candidate — `no_emitter` is then the supported answer and you need not search for one.

| # | id | clip | eid | engineer says | emitter | group |
|--:|----|------|----:|---------------|---------|-------|
| 1 | CI-018 | 2001_164 | 271 | 50% race distance complete | none | Session Phase (8 clips — no distance-tracker emitter) |
| 2 | CI-038 | 2001_22 | 27 | Rain expected, make qualifying run ASAP | none | Qualifying Detail (10 clips — run counter not hooked) |
| 3 | CI-058 | RE_PD_FR_BLIST | 97 | Light blistering — Front Right | LITERAL AISessionPhaseCounter_0041aa10.c:31 (0x0041aa10) | Per-Tire Blistering (8 clips) |
| 4 | CI-078 | MISC_05 | 143 | Blocking blue under investigation | none | Penalty System — Self (8 clips, penalty emitters exist but rare) |
| 5 | CI-098 | MISC_27 | 267 | AI maintains position | none | Pit Strategy / AI Pit / Misc (15 clips) |
| 6 | CI-118 | 2001_159 | 275 | Session ends | LITERAL DisplayMessage0x113_0041a6b0.c:10 (0x0041a6b0) | Session / Event Clips (5 clips) |
| 7 | CI-138 | Yellow02 | 136 | Yellow flag deployed Sector 2 | LITERAL AISessionModeTransition_004193a0.c:29 (0x004193a0) | Remaining ALIVE Clips Quick-Reference |
| 8 | CI-158 | AIFastLap01 | 262 | AI sets fastest lap of race | none | Remaining ALIVE Clips Quick-Reference |
| 9 | CI-178 | Qual07 | 3 | Pole sitter AI beats own provisional pole | none | Remaining ALIVE Clips Quick-Reference |
| 10 | CI-199 | RE_OD_EXIT_RED | 36 | Other driver under investigation (exiting under red) | none | Remaining ALIVE Clips Quick-Reference |
| 11 | BL-08 | MechProb27 | 87 | Player stuck in 2nd gear | none | Player Gearbox Failure — Stuck Gears (8 clips) |

When the last item is written, print `WORKER R DONE — 11 items`.
