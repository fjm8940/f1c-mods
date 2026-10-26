# WORKER A — commentary emitter SPEC for category `pit_procedure`

Read the role file in full first — all of it applies (sourcing, tiers, `UNKNOWN`
over guessing, PowerShell not bash, no live memory, no writes to shared files):

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

## Corpus root

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_spec\output\worker_A.jsonl

ONE JSON object. Write it when the analysis is done.

## What you are producing

A **buildable specification** for a single tick-hook state observer that can fire
every clip in your category. Not a survey, not prose. Someone will write C from
this without re-reading the corpus.

These clips have **no emitter at all** — no code anywhere calls their event id.
So you are not looking for an existing emitter. You are answering: *what game
state would have to be watched, and what condition would have to be true, for
each of these lines to make sense?* — and grounding that in state the corpus
already knows about.

## The reliability rule for this task

A spec that invents a threshold is worse than no spec, because someone will
build on it. Therefore:

- Any number you state (threshold, offset, address, constant) MUST have a source:
  a `file:line` in `functions/`, a `name_index.json` entry, a `config_schema.json`
  row, or a `data_index.json` value range. Cite it inline.
- A number you cannot source goes in `needs_live_calibration` with the exact
  measurement someone should take. **That section is a success, not a failure.**
  A spec that says "poll OnLineWetness, threshold UNKNOWN — measure it while rain
  starts" is complete and useful. One that says "threshold 0.3" without a source
  is defective.
- `confidence` is `DERIVED` or `GUESS` only. You cannot self-award FACT/CONFIRMED;
  those need a live poke you are not permitted to perform.

## Method

1. `python tools/ask.py findings <category keyword>` — prior art, including
   DISPROVEN attempts. Quote hits.
2. For each clip, read its description and decide what state it implies.
3. Find that state in the corpus, in this order:
   - `python tools/ask.py global <name-or-hex>` — named globals with trust tiers
   - `python tools/ask.py offset <hex>` — vehicle struct fields
   - `python tools/ask.py token "<config token>"` — a `.plr`/`.hdv` setting that
     parses into the value (`config_schema.json`)
   - `strings_modules.json` — which original `.cpp` the logic came from
4. Find the function that already computes or writes it, and read it. The
   thresholds you need are often literals in that function — the blister
   thresholds (0.7 / 0.4) were recovered exactly this way from `sub_438130`.
5. Pick a hook site: a function that runs every tick and can see this state.
   `python tools/ask.py func <addr>` gives callers. Say why it is suitable.
6. Write the condition for EACH eid as a predicate over the state you named.

## Emission is already solved — do not redesign it

Firing a line, once your condition is true, is a solved call:

```
StructInitWithSentinelValues(buf)
DriverDataTableLookupAndTimeConversion(buf, <eid>, ...)
InsertDriverTableEntry(0x5EA618, buf)
```

Proven working in `mods/grid_reorder/gr21.c` (`inject_commentary()`). Your spec
just needs to say *when* to call it and with which eid.

## Output schema — one object

```json
{"id":"SPEC-pit_procedure","category":"pit_procedure",
 "state_observers":[{"what":"track wetness","global":"OnLineWetness","addr":"0x…","trust":"CONFIRMED","source":"name_index.json"}],
 "computing_functions":[{"addr":"0x…","name":"…","file_line":"functions/X.c:42","what":"writes wetness each tick"}],
 "hook_site":{"addr":"0x…","name":"…","why":"runs per tick, already reads this state","source":"tools/ask.py func 0x…"},
 "conditions":[{"eid":282,"clip":"PWS_2001_218","predicate":"wetness crosses above T_onset","source":"functions/X.c:88","threshold_known":false}],
 "static_constants":[{"name":"T_onset","value":"UNKNOWN","source":"not found in any function"}],
 "needs_live_calibration":["exact value of OnLineWetness when rain begins — watch it across a weather transition"],
 "shared_with_other_categories":["session state guard SessionType==7"],
 "confidence":"DERIVED",
 "evidence":["every claim above, one line each, file:line or command -> output"],
 "open_questions":["…"]}
```

## Session context you need (established live, treat as FACT)

- `SessionType` (`0x007C56A4`) = **5 in qualifying, 7 in a race**. Many emitters
  gate on `== 7`. If your category is race-only, say so in the spec.
- `RaceActiveFlag` (`0x007D0308`) = 3 while on track.
- Vehicle array: base `0x5EC6D8`, stride `0x48C0`, slot live ⇔ int32 at
  `+0x32C8` != -1, driver name ASCII at `+0x4620`.

## Your category: `pit_procedure` — 28 clips with no emitter


| Clip | EIDs | Description |
|------|------|-------------|
| `PWS_2001_132` | 185 | AI driver jammed fuel hose |
| `PWS_2001_68` | 62, 369 | Front Left tire change successful |
| `PWS_2001_69` | 64, 357 | Front Right tire change successful |
| `PWS_2001_70` | 67, 338 | Rear Left tire change successful |
| `PWS_2001_71` | 69, 349 | Rear Right tire change successful |
| `PWS_2001_72` | 63, 358 | Slow Front Left tire change |
| `PWS_2001_73` | 65, 359 | Slow Front Right tire change |
| `PWS_2001_74` | 68, 346 | Slow Rear Left tire change |
| `PWS_2001_75` | 70, 339 | Slow Rear Right tire change |
| `PWS_2001_76` | 61, 372 | Wheel gun failure |
| `PWS_2001_78` | 57, 362 | Refuel successful |
| `PWS_2001_79` | 56, 365 | Fuel hose issue - slow refuel |
| `PWS_AIINCID01` | 233 | AI stalls on the grid |
| `PWS_MECHANWARN0` | 130, 415 | Meatball flag - must pit |
| `PWS_MISC07` | 248 | AI speeding pits - investigation |
| `PWS_MISC30` | 183 | AI has pit stop issue |
| `PWS_MISC54` | 46 | Box! Pit now |
| `PWS_PIT01` | 49, 363 | Pit on this lap |
| `PWS_PIT03` | 50, 367 | Watch for pit lane entrance |
| `PWS_PIT06` | 51, 356 | Limiter On |
| `PWS_PIT08` | 53, 361 | Brake (pit procedure) |
| `PWS_SCAR05` | 428 | Safety car in pit lane (2) |
| `PWS_TEAMPIT01` | 58, 366 | Teammate pits this lap - stay out |
| `PWS_TEAMPIT02` | 59, 368 | Player can now pit |
| `RE_OD_INTER` | 191 | Other driver leaves pits on Inters |
| `RE_OD_SLICKS` | 193, 508 | Other driver leaves pits on Slicks |
| `RE_PD_STALLED` | 112, 405 | Engine stalled - T-car prepared |
| `RE_PD_STALLED_T` | 113, 406 | T-car also stalled - session over |

Cover EVERY clip above in `conditions`, or list it in `open_questions` saying why it cannot be specified.

Print `WORKER A DONE — pit_procedure` when the object is written.
