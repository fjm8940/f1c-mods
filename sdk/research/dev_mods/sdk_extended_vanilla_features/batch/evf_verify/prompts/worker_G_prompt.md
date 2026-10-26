# WORKER G — extended_vanilla_features / evf_verify

You are worker G of 20. Your items are listed below; no other worker has
them. Read the role file **in full** before starting:

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

## Corpus root (run all tools from here)

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — write to this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_verify\output\worker_G.jsonl

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
| 1 | CI-007 | 2001_74 | 68 | Slow tire change — Rear Left | none | Pit Stop Procedure (16 clips — emitters never fire, PitSelectionStateMachine skips them) |
| 2 | CI-027 | 2001_221 | 280 | Rain starts, probably will continue (monsoon) | none | Weather Commentary (6 clips — no weather→commentary bridge) |
| 3 | CI-047 | MechProb06 | 78 | Puncture — Front Left | none | Player Mechanical — Punctures (4 clips) |
| 4 | CI-067 | AIIncid07 | 231 | Spun off just ahead | none | AI Incidents (7 clips — sub_41A8C0 switch covers only 4 of 11 types) |
| 5 | CI-087 | MISC_48 | 245 | Cutting track investigation — AI | none | Penalty System — AI (7 clips) |
| 6 | CI-107 | AIHit07 | 221 | AI lost wheel | LITERAL sub_41AFD0_0041afd0.c:42 (0x0041afd0) | AI Mechanical (18 clips — emitter exists for wing/wheel but not gear) |
| 7 | CI-127 | Pit02 | 52 | Pit crew is ready | CANDIDATE AIPitStrategyState_0041a200.c:15 via v3 | Remaining ALIVE Clips Quick-Reference |
| 8 | CI-147 | Scar05 | 428 | Safety car in pit lane (alt) | none | Remaining ALIVE Clips Quick-Reference |
| 9 | CI-167 | AIWin02 | 260 | AI finishes at X position | LITERAL PerDriverRaceCommentary_00419c50.c:164 (0x00419c50) | Remaining ALIVE Clips Quick-Reference |
| 10 | CI-187 | Qual28 | 12 | Very few drivers on track | none | Remaining ALIVE Clips Quick-Reference |
| 11 | CI-209 | 2001_311 | 285 | Fastest AI on Inters | none | Remaining ALIVE Clips Quick-Reference |
| 12 | BL-17 | Hit03 | 106 | Player lost wheel — Front Left | LITERAL sub_41AFD0_0041afd0.c:13 (0x0041afd0) | Player Wing/Wheel Loss (6 clips — part detach commentary) |

When the last item is written, print `WORKER G DONE — 12 items`.
