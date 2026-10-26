# Vanilla Features Restoration — bring back the cut content

**Layout:** `tools/` (all scripts), `data/` (generated JSON catalogs),
`docs/` (TODO/PROGRESS/DUMP_RESULTS — this README stays at root). Run scripts
from anywhere; each resolves its data/docs via `__file__`, not cwd.

**Status:** RESTORATION STARTED — first 6 clips restored and heard in-game
2026-07-27.

316 race engineer clips confirmed in PC_RE_MAIN.SDT. 456 playable events in the
event table. 23 orphaned clips (no event points to them). 26 blocked events
(cooldown=FLT_MAX, deliberately disabled).

**The effort split is now measured, not estimated** — see
`docs/RESTORATION_PLAN.md` (regenerate: `python tools/evf_classify.py --write`):

| | Clips | |
|---|--:|---|
| DONE | 6 | restored, heard in-game (player wing/wheel loss) |
| SHIPPED | 76 | already audible in vanilla |
| patched, unproven | 3 | cooldown unblocked, awaiting a live retirement |
| needs only the right situation | 87 | emitter exists, nothing blocks it |
| needs a new emitter | 109 | no code emits the eid — `docs/EMITTER_SPECS.md` |
| orphaned | 23 | no event resolves to the clip |

**90 clips need no new code; 132 do.** Of the emitterless 109, static analysis
has produced buildable specs for all 10 categories (`docs/EMITTER_SPECS.md`):
**68 conditions are directly hookable, 78 need a constant measured in-game.**

The older "219 wired clips that never trigger" figure overstated the emitterless
count — it predates `data/emitter_index.json`, the deterministic scan of every
`DriverDataTableLookupAndTimeConversion` call site.

See `DUMP_RESULTS.md` for the original dump numbers and `full_event_dump.json`
for every event's priority/cooldown/score/blocked/sdt_index.

## The loop: listen → triage → restore → prove

`progress.json` is the **master progress file** — one row per SDT clip, one
status each. `PROGRESS.md` is its generated human view (never hand-edit it).

```
SILENT --(you hear it in voice_browser.py and know it never plays)--> WIP
WIP    --(a mod fires it in-game, verified live)-------------------> DONE
```

Open `voice_browser.py`, preview clips, and press `→` on any clip on the left
you recognise as one the game never actually says. That single keypress writes
the status, its event IDs, and its blocked/orphaned flags into `progress.json`
and regenerates `PROGRESS.md`. The right-hand panel then *is* the work queue,
and it is the same list `TODO.md` is organised around.

A human status is never overwritten by a re-dump. Re-running the dump refreshes
only the derived fields (eids, blocked, orphaned) — your triage survives.

**What the auto-classifier does and does not claim:** SHIPPED means the blog
(j21csblog, 2023-01-21 — the author actually played the game and bolded what
they heard) bolded it **and** it is still reachable by a non-blocked event; an
earlier version of this classifier used "reachable by an event" alone, which
mismarked ~200 wired-but-silent clips as heard. That earlier bug is exactly the
"I've never heard this in-game" report that prompted the fix — see
`DUMP_RESULTS.md`'s 2026-07-27 correction note for the numbers. Current split:
57 SHIPPED, 247 in the work queue (untriaged SILENT, since nobody has moved
anything to WIP/DONE yet). Each row records its `auto_reason` so the
classification can't launder a guess into evidence — read it before trusting a
clip is genuinely heard.

## What's already here (working tools)

| File | What it does |
|------|-------------|
| `progress.py` | The master progress store. `python progress.py` prints current counts |
| `voice_browser.py` | GUI — preview every SDT clip, move clips between SHIPPED and the work queue, writes `progress.json` |
| `sdt_tool.py` | Parses `.SDT` files, lists all clips by index + name |
| `commentary_inject.py` | Injects events into the game's commentary queue via `WriteProcessMemory` |
| `commentary_binder.py` | GUI — 2 tabs: browse events + bind NUMPAD keys, browse SDT clips + audio preview (ffplay) |
| `play_commentary.py` | CLI — loads bindings from JSON, listens for NUMPAD hotkeys |
| `event_catalog.py` | Loads the 482 event→clip mappings from `event_clip_map.json` |
| `dump_full_catalog.py` | Regenerates the mapping from live game memory + the SDT files |
| `crossref_blog.py` | Cross-references the mapping against the 2023 blog clip list |

### Generated data

```
event_clip_map.json     482 eid -> clip name        (dump_full_catalog.py)
full_event_dump.json    all 520 events, full fields (dump_full_catalog.py)
orphaned_clips.json     23 clips no event reaches   (dump_full_catalog.py)
progress.json           per-clip status, human+auto (progress.py / voice_browser.py)
PROGRESS.md             generated view of the above (progress.py)
event_bindings.json     NUMPAD bindings             (commentary_binder.py GUI)

GAME_DIR                commentary_binder.py expects F1 Challenge V10/ at
                        mods/../../F1 Challenge V10 (the game install dir)
```

**Known data wart:** `used_voices.json` says `total_used: 65` but its `clips`
list holds 285 entries — the header counts blog-confirmed-heard clips, the list
holds every alive clip. Do not read the two as the same number. `progress.json`
supersedes it; treat `used_voices.json` as the 2023 blog snapshot only.

The SDT files it reads:
```
Audio/Commentary/Pws/PC_RE_MAIN.SDT    # race engineer PWS clips
Audio/Commentary/Pws/PC_RE_STITCH.SDT  # PWS stitch transitions
Audio/Commentary/Tv/PC_TV_MAIN.SDT     # TV commentary
Audio/Commentary/Tv/PC_TV_STITCH.SDT   # TV stitch transitions
```

## How the chain works (FACT, verified live)

```
Emitter function → DriverDataTableLookupAndTimeConversion(eid, ...)
  → InsertDriverTableEntry(controller, buf)
  → controller 0x005EA618
      +0x864  queue entry (32 bytes)
      +0xAE4  isPlaying
      +0xAE8  global cooldown timestamp
      +0xAF0  enabled flag
      +0xAF8  per-event timestamp[~514]
      +0x135C kick (write 1 to process)
  → TableLookupByIndex_004144c0 re-reads score by eid FROM THE TABLE
  → EVENT_TABLE 0x005BF9A0, stride 0x14
      +0x00  priority (int32)
      +0x0C  cooldown (float)
      +0x10  score (int32)   ← controls which clip plays
  → word_5BEBB0[score]       ← uint16 PWS lookup table
  → SDT clip index in PC_RE_MAIN.SDT
```

**Key constraint:** the score is re-read from the table at play time. To play an
orphaned clip through a carrier event, you must repoint the carrier's score field
in the event table, inject, then restore. Proven in `tools/sc_audio_test.py` +
`mods/grid_reorder/gr21.c`.

`commentary_inject.py` instead injects the event's OWN ID — it works for events
that already have a valid score mapping but are blocked by cooldown, session guard,
or queue contention. It bypasses the cooldown+timer gates by clearing the
per-event timestamp and kicking the queue.

## Cut feature inventory (all of them — commentary is just the loudest)

### TIER 1: Commentary — all the unused voice lines

Source: j21csblog 2023-01-21 (316 lines catalogued). Status tracked per clip in
the event catalog. The SDT tools already show which clips have event IDs and which
are orphaned.

#### Orphaned (no event resolves to these clips)
- `PWS_SCAR01-05` — safety car commentary (5 clips in SDT indices 137-141, scores 131-135)
- `PWS_AIHIT08` — AI suspension damage (event 220, zero callers in 7,138 functions)

#### Silenced by missing emitter
- Weather commentary (2001_218-222): rain transitions — no weather→commentary bridge
- Pit stop procedure (2001_68-80): tire changes, refuel, stall — PitSelectionStateMachine skips these
- Session phase (2001_163-165): 25/50/75% race distance — no distance-tracker emitter
- Penalty investigations (MISC_02-09, MISC_47-48): jumps straight to verdict
- Player mechanical issues (MechProb06-33, MechWarn0): fatigue writes damage silently
- Player retire (Retire01-04): no retire-commentary bridge for player
- Blue flag approach chain (AIBlue01-03, AILappin01/05): partial coverage only
- AI incident chain (AIIncid01-08, AIOffTrack0): sub_41A8C0 switch covers 4/11 types
- AI penalty commentary (events 252-256): sub_41A720 only fires for player
- Per-tire blistering: generic Tyres01/02 fires but per-corner variants never
- Qualifying lap-type (2001_13-15, 2001_17-20): run counter not hooked to commentary

#### Scripting bugs
- `Qual02/03` fastest-sector fires on slow sectors too — no time comparison
- `AIPit10` "3-stop strategy" fires on 1-stoppers — strategy counting wrong

### TIER 2: Game features — commentary reveals what's cut

| Feature | What exists | What's missing | Priority |
|---------|------------|----------------|----------|
| **Red flag** | Full physics (AI stop, repositioning), `PWS_RED01` clip, entry/exit penalties, network layer | Original setter (0x4a5840) never recognized as function by IDA; no accident trigger | HIGH |
| **Safety car** | 5 SCAR clips, speed cap `veh+0x4538`, AI cmd queue 0xe, waypoint guidance | No SC vehicle type, no deployment loop, no deployment condition | MEDIUM |
| **Formation lap** | Config tokens (`Formation Lap`, `Recon Pit Open`, `AI Follow Grid`), `Begin Formation Lap` string | No session state-machine transition; all .plr files set to `0` | HIGH |
| **Multiple practices** | SessionType enum 0-7 (P1-P4 exist), localization strings `Practice 1/2/3/4`, config tokens `Run Practice2/3/4` | Only P1→Qualify→Warmup→Race chain implemented | MEDIUM |
| **Test day** | SessionType 0, `"Test Day"` string | No setter, no menu entry | LOW |
| **Mechanical failures** | 4-level rate system (0=none to 3=season), full ReasonCode enum (1-9), AI part-detach path | Player failure detection never fires; non-wing/wheel AI failures blocked | HIGH |
| **Night lighting** | Full sunset/sunrise system, `nightlightXXX` model toggles, `Night Lighting` config 0.0-1.0 | Every track ships with `Night Lighting=1.0` (disabled) | LOW |
| **Broadcast overlay** | `Broadcast Overlay` config, separate TV-style HUD, `CommentaryTVPWSPathBuilder` | Unclear what UI elements are controlled | LOW |
| **Yellow flags** | 29 marshalling zones, ∼199m each, per-frame recalculation | Only green(0) and yellow(2) observed out of 62 possible states | MEDIUM |
| **Replay AI bug** | Save/load works | AI drives simplified after replay — 3 fix attempts failed | BUG |

### Features NOT in the code (negative findings)
- T-Car / spare car — zero string literals, zero config tokens. Only audio clip implies it.
- Meatball flag as game concept — only audio clip `MechWarn0`, no code.
- Pit crew lollipop — only `Pitcrew Push` (out-of-fuel) and `Pitcrew Detail` (LOD).
- TV studio/presenter — Broadcast Overlay config only, no studio references.

## Implementation plan

### Phase 0: Generate the event→clip mapping

Write `dump_full_catalog.py` (or equivalent) that:
1. Reads game memory live: event table at 0x5BF9A0, PWS lookup at 0x5BEBB0
2. Reads all 4 SDT files from disk, maps SDT index → clip name
3. Produces `event_clip_map.json`:
   ```json
   {"0x01": "PWS_2001_04", "0x02": "PWS_2001_05", ...}
   ```
4. Marks each event: `alive` (emitter exists), `dead` (no emitter), `orphaned` (clip exists but no event maps to it)
5. Reports which 200+ clips from the blog post have no event mapping

### Phase 1: Play all existing events — confirm wiring

Using `commentary_binder.py` + `play_commentary.py`:
- Test every event ID in the catalog in-game
- Confirm audio plays correctly for all 153 mapped events
- List any that fail (wrong clip, silence, crash)
- Build confidence that queue injection works for every wired event

### Phase 2: Wire orphaned clips to carrier events

For each un-mapped clip:
1. Assign a score in the PWS lookup table (repurpose unused slots or extend the table)
2. Pick a carrier event ID that never fires (event 220, or synthetic IDs >520)
3. Build a mapping: `clip_name → carrier_eid → score`
4. Prove each plays via injection

### Phase 3: Restore commentary emitters

For each category of silenced clips, write the emitter or fix the guard:
- **Tier A (simple patches):** AI penalty events, AI incident switch extension,
  blue flag approach chain — fix one guard or extend a switch
- **Tier B (new emitters):** weather commentary, session phase, pit procedure,
  per-tire blistering — tick-hook DLL polling game state
- **Tier C (bug fixes):** Qual02/03 time comparison, AIPit10 strategy counter

Proven DLL skeleton: copy `mods/grid_reorder/gr28.c`, replace tick logic.

### Phase 4: Restore game features (beyond commentary)

This is the real mod. Each feature needs per-feature investigation, prioritized
by impact and difficulty:

1. **Red flag setter** (HIGH) — hook accident detection + write red flag globals.
   Most infrastructure exists (`mods/red_flag_v2/`).
2. **Formation lap** (HIGH) — needs session state-machine patch. Config exists,
   AI grid-following path exists.
3. **Mechanical failures** (HIGH) — hook fatigue/wear to fire player commentary.
   AI path already works for wing/wheel; extend to engine/gearbox/electronics.
4. **Multiple practice sessions** (MEDIUM) — UI session-advance patch.
5. **Safety car deployment** (MEDIUM) — `mods/safety_car/` DLL built but untested.
6. **Night lighting** (LOW) — one config flip per track.

## Prior art in the corpus

| What | Where |
|------|-------|
| Carrier event injection (proven) | `tools/sc_audio_test.py` |
| `inject_commentary()` (in-game proven) | `mods/grid_reorder/gr21.c` |
| Tick-hook DLL skeleton | `mods/grid_reorder/gr28.c` |
| HUD text overlay primitive | `mods/_README.md` line 105, `mods/grid_reorder/gr21.c` |
| AI speed cap `veh+0x4538` | `mods/grid_reorder/gr28.c` |
| Garage exit / de-retirement recipe | `mods/_README.md` lines 26-92 |
| Red flag write-set (confirmed) | `mods/red_flag_v2/` |
| Part-detach commentary emitter | `mods/ai_wing_retire/` investigation |
| Event ID mapping (106-111, 220-223) | `investigations/part_detach_commentary/README.md` |
| Event table structure (confirmed) | `investigations/part_detach_commentary/`, `mods/safety_car/README.md` |

## Score discovery procedure

For clip→event wiring when `event_clip_map.json` isn't available:

1. `sdt_tool.py list PC_RE_MAIN.SDT` → clip index per name
2. Read `word_5BEBB0[score]` from live game → reverse map `sdt_index → [scores]`
3. Read event table `dword_5BF9B0[5*eid]` → map `eid → score`
4. Clip with no event hitting it → orphaned, needs carrier assignment
5. Event with score=0 → dead event, can be repurposed as carrier
