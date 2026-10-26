# WORKER E — commentary emitter SPEC for category `weather_track`

Read the role file in full first — all of it applies (sourcing, tiers, `UNKNOWN`
over guessing, PowerShell not bash, no live memory, no writes to shared files):

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

## Corpus root

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_spec\output\worker_E.jsonl

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
{"id":"SPEC-weather_track","category":"weather_track",
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

## Your category: `weather_track` — 11 clips with no emitter


| Clip | EIDs | Description |
|------|------|-------------|
| `PWS_2001_218` | 282 | Overcast conditions - possible rain |
| `PWS_2001_220` | 283 | Rain starts - short shower |
| `PWS_2001_221` | 280 | Rain starts - monsoon likely |
| `PWS_2001_222` | 281 | Rain eases off |
| `PWS_2001_310` | 286 | Fastest AI is on Slicks |
| `PWS_2001_311` | 285 | Fastest AI is on Inters |
| `PWS_2001_312` | 287 | Fastest AI is on Wets |
| `PWS_OIL01` | 127, 420 | Oil on track Sector 1 |
| `PWS_OIL02` | 128, 417 | Oil on track Sector 2 |
| `PWS_OIL03` | 129, 418 | Oil on track Sector 3 |
| `RE_OD_FASTEST_M` | 290 | Other driver fastest - on Monsoon tires |

Cover EVERY clip above in `conditions`, or list it in `open_questions` saying why it cannot be specified.

Print `WORKER E DONE — weather_track` when the object is written.
