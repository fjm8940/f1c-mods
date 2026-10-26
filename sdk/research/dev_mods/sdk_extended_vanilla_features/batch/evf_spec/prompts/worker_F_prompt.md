# WORKER F — commentary emitter SPEC for category `flags_incident`

Read the role file in full first — all of it applies (sourcing, tiers, `UNKNOWN`
over guessing, PowerShell not bash, no live memory, no writes to shared files):

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

## Corpus root

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_spec\output\worker_F.jsonl

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
{"id":"SPEC-flags_incident","category":"flags_incident",
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

## Your category: `flags_incident` — 8 clips with no emitter


| Clip | EIDs | Description |
|------|------|-------------|
| `PWS_AIBLUE01` | 258 | Player approaches AI driver (blue flag) |
| `PWS_AIBLUE02` | 257 | AI driver under blue flag - be ready |
| `PWS_AIINCID03` | 228 | AI went off track |
| `PWS_AIINCID04` | 232 | AI spun off sector X |
| `PWS_AIINCID06` | 227 | Car off track just ahead |
| `PWS_AIINCID07` | 231 | Car spun off just ahead |
| `PWS_OFFROAD01` | 116, 409 | Player went off track (1) |
| `PWS_SCAR02` | 425 | Safety car on track |

Cover EVERY clip above in `conditions`, or list it in `open_questions` saying why it cannot be specified.

Print `WORKER F DONE — flags_incident` when the object is written.
