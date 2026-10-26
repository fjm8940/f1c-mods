# Extended Race Engineer — Phase A worker brief (all batches)

You are one research worker. Read this whole file first. Your batch id and clip list are in your task.

## Paths

- Workspace: `%USERPROFILE%\Desktop\F1` (no git). Mod: `development\mods\extended_race_engineer` (= ERE).
- Your clips: `ERE\data\clip_assignments.json` → `batches.<your batch id>`.
- Corpus: `f1c_ida_decompiled\` (static RE of the game). SDK front door, run from `f1c_ida_decompiled`:
  - `python tools/f1c_mod.py evidence-query findings "TEXT" --json` (prior art, proven/disproven)
  - `python tools/f1c_mod.py inspect ADDRESS_OR_NAME` (function code, callers, SDK links)
  - `functions\*.c` = decompiled code (ground truth text). `findings.jsonl` = prior results.
- EVF (archived earlier attempt — READ ONLY, never edit): `f1c_ida_decompiled\mods\extended_vanilla_features\`
  - `data\full_event_dump.json` (every eid: clip, score, cooldown, blocked), `data\emitter_index.json`
    (which function emits which eid), `data\progress.json`
  - `docs\RESTORATION_PLAN.md`, `docs\EMITTER_SPECS.md`, `docs\VOICE_LINES.md`, `docs\SPEC_EVF-*.md`,
    `docs\TRIAGE_NOTES.txt` (owner's own notes per clip), `dll\README.md` (6 traps)
- Voice map: `f1c_ida_decompiled\docs\56-voice-line-reference-map.md` (eid → clip → what it says).
- Other mods that already speak lines: `development\mods\*` (pitstop_modding, mechanical_failures,
  full_safety_car / native_safety_car, red_flag, extended_damage_commentary, *_commentary) and
  `f1c_ida_decompiled\tools\sdk\f1c_pit_voice.h`.

## Facts you must use (already proven)

- Channel is chosen by eid: **1..290 = race engineer (PWS)**, 292..535 = TV. Most clips have a low
  "twin" eid (PWS) and a high one (TV). Always design for the PWS eid. Never assume an eid from a clip
  name: take it from `full_event_dump.json` / doc 56, and mark it "needs isolation test".
- A line is queued with record init `0x4144D0`, formatter `0x4144F0`, `InsertDriverTableEntry 0x418560`
  on controller `0x5EA618` (see `f1c_pit_voice.h`). AI lines need the subject driver id `veh+0x489C`.
- Cooldown at `EVENT_TABLE + eid*0x14 + 0x0C`; FLT_MAX = blocked. If a native emitter exists and only
  FLT_MAX blocks it, the fix is a cooldown write (proven: HIT01-06, gear lines 80-93).
- Carrier-score trick: a low carrier eid with its score rewritten plays any clip on the PWS channel
  (proven for SCAR lines). Use it for orphans / TV-only clips.
- SessionType 5 = qualifying, 7 = race. Race position = `veh+0x4060` (NOT 0x32D0). Lap distance
  `veh+0x4044`. pittingStatus `veh+0x3450` (0 track, 1 in-lap, 2 lane, 3 box, 4 exit).
- Owner rules: lines describe REAL events only (no invented failures); AI gets the same treatment as
  the player; severe calls only when warranted; no spam (every trigger needs a cooldown/latch).

## Hard rules

1. READ ONLY everywhere except your own output files listed in your task. Never edit EVF, the corpus,
   other mods, `findings.jsonl`, or governance files. Never delete anything.
2. No live game work: do not run poke/frida/inject/memory tools, game exes, builds or test gates.
3. Every number (address, offset, eid, threshold) must come from a cited source (`file:line` or a
   tool output). If you cannot find one, write `UNKNOWN` and what would prove it. Never guess silently.
4. Label every claim's trust: FACT (live-proven) > CONFIRMED (2 sources) > DERIVED (static, unproven)
   > GUESS. Never raise a tier.
5. Never read `api1.txt` or key files. Files are UTF-8 without BOM.
6. If a clip is already played by another mod, say which (file:line) and set technique `owned_by_other_mod`.

## Output (design batches H1-H8)

Write two files:

1. `ERE\data\design\<batch>.json`:
```json
{"schema":"ere.design.v1","batch":"H1_pit","rows":[{
  "clip":"PWS_PIT01","sdt_index":48,"pws_eid":49,"all_eids":[49,363],"says":"Pit on this lap",
  "eid_source":"full_event_dump.json / doc56 line N","eid_isolation_tested":false,
  "already_modded_by":null,"native_emitter":"sub_XXXXXX or null","why_silent":"cooldown_flt_max|gate_never_true|no_emitter|orphan|player_unreachable|unknown",
  "technique":"cooldown_unblock|game_state_emitter|hook_native|carrier_score|owned_by_other_mod|skip",
  "subject":"player|ai|team","trigger":"plain-language predicate with exact state reads",
  "state_reads":[{"what":"pittingStatus","locator":"veh+0x3450","tier":"CONFIRMED","source":"..."}],
  "latch_cooldown":"once per stop / 30 s / ...","calibration_needed":"what constant must be measured live, or null",
  "conflicts":"other lines/mods it could clash with","live_test":"how the owner makes it happen in game",
  "tier":"DERIVED","sources":["file:line", "..."],"open_questions":"..."}]}
```
2. `ERE\docs\design\<batch>.md`: a short human summary — one table row per clip (clip, says, technique,
   trigger in one line, blocker), then "Shared state/hooks this batch needs" and "Open questions".

## Final reply to the coordinator

Max 15 lines: counts per technique, clips you could not design and why, any prior-art contradiction
you found (e.g. EVF says X, code says Y).
