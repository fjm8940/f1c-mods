# WORKER L — extended_vanilla_features / evf_verify

You are worker L of 20. Your items are listed below; no other worker has
them. Read the role file **in full** before starting:

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

## Corpus root (run all tools from here)

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — write to this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_verify\output\worker_L.jsonl

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
| 1 | CI-012 | 2001_79 | 56 | Fuel hose issue — slow refuel | none | Pit Stop Procedure (16 clips — emitters never fire, PitSelectionStateMachine skips them) |
| 2 | CI-032 | 2001_14 | 30 | AI Driver ahead, on Flying Lap | none | Qualifying Detail (10 clips — run counter not hooked) |
| 3 | CI-052 | MechProb15 | 77 | Engine overheating | LITERAL sub_41B190_0041b190.c:7 (0x0041b190) | Player Mechanical — Other (4 clips) |
| 4 | CI-072 | AIBlue03 | 259 | Player passes AI Driver | LITERAL AIDriverStateTransition_00419120.c:39 (0x00419120) | Blue Flag / Lapping Chain (5 clips) |
| 5 | CI-092 | AIPit11 | 195 | AI Driver pits to serve stop-go penalty | none | Pit Strategy / AI Pit / Misc (15 clips) |
| 6 | CI-112 | RE_OD_PUNCTURE | 210 | Other driver has a puncture | none | AI Mechanical (18 clips — emitter exists for wing/wheel but not gear) |
| 7 | CI-132 | Pit14 | 55 | Pit this lap for Stop-Go penalty | LITERAL AISessionEventAudio_0041a8c0.c:10 (0x0041a8c0) | Remaining ALIVE Clips Quick-Reference |
| 8 | CI-152 | 2001_21 | 22 | AI Driver pits, completed all qualifying laps | CANDIDATE AISessionPhaseChange_0041a2b0.c:36 via v5 | Remaining ALIVE Clips Quick-Reference |
| 9 | CI-172 | Tyres01 | 105 | Light blistering — Player | LITERAL AISessionPhaseCounter_0041aa10.c:45 (0x0041aa10) | Remaining ALIVE Clips Quick-Reference |
| 10 | CI-193 | AIPit10 | 182 | AI Driver has 3 stop strategy" (scripted wrong) | CANDIDATE AIFormationLap_0041a3a0.c:24 via v3 | Remaining ALIVE Clips Quick-Reference |
| 11 | BL-02 | MechProb21 | 81 | Player lost 2nd gear | none | Player Gearbox Failure — Lost Gears (6 clips) |
| 12 | BL-22 | Retire02 | 117 | Player generic retirement (variant 1) | LITERAL sub_41B250_0041b250.c:23 (0x0041b250) | Player Retirement + Stall (4 clips) |

When the last item is written, print `WORKER L DONE — 12 items`.
