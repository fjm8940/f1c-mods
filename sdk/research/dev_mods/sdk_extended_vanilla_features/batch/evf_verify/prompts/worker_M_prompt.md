# WORKER M — extended_vanilla_features / evf_verify

You are worker M of 20. Your items are listed below; no other worker has
them. Read the role file **in full** before starting:

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

## Corpus root (run all tools from here)

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — write to this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_verify\output\worker_M.jsonl

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

## Your 12 items


`emitter` column is precomputed from `emitter_index.json`. `none` means the eid appears at no call site, literal or candidate — `no_emitter` is then the supported answer and you need not search for one.

| # | id | clip | eid | engineer says | emitter | group |
|--:|----|------|----:|---------------|---------|-------|
| 1 | CI-013 | 2001_80 | 54 | Stalled engine, we'll get you refired | none | Pit Stop Procedure (16 clips — emitters never fire, PitSelectionStateMachine skips them) |
| 2 | CI-033 | 2001_15 | 31 | AI Driver ahead, on In Lap | none | Qualifying Detail (10 clips — run counter not hooked) |
| 3 | CI-053 | MechProb16 | 75 | Damaged suspension, avoid kerbs | none | Player Mechanical — Other (4 clips) |
| 4 | CI-073 | AILappin01 | 122 | AI Driver approaches player (blue flag) | none | Blue Flag / Lapping Chain (5 clips) |
| 5 | CI-093 | AIPit13 | 190 | AI Driver rejoins track at X position | none | Pit Strategy / AI Pit / Misc (15 clips) |
| 6 | CI-113 | RE_OD_INTER | 191 | Other driver leaves pits, using Inters | none | AI Tire Choice (4 clips) |
| 7 | CI-133 | 2001_124 | 159 | Player wins WDC with X rounds to go | LITERAL PerDriverRaceCommentary_00419c50.c:149 (0x00419c50) | Remaining ALIVE Clips Quick-Reference |
| 8 | CI-153 | 2001_96 | 60 | Player rejoins track at X position after pit stop | none | Remaining ALIVE Clips Quick-Reference |
| 9 | CI-173 | Tyres02 | 104 | Severe blistering — Player | LITERAL PitStateSlotActivation_0041ab30.c:45 (0x0041ab30) | Remaining ALIVE Clips Quick-Reference |
| 10 | CI-194 | RE_PD_LAP_DISALLOWED | 39 | Player fastest lap disallowed (exiting under red) | none | Remaining ALIVE Clips Quick-Reference |
| 11 | BL-03 | MechProb22 | 82 | Player lost 3rd gear | none | Player Gearbox Failure — Lost Gears (6 clips) |
| 12 | BL-23 | Retire04 | 119 | Player blown engine | LITERAL sub_41B250_0041b250.c:15 (0x0041b250) | Player Retirement + Stall (4 clips) |

When the last item is written, print `WORKER M DONE — 12 items`.
