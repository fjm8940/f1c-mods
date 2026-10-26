# Investigation brief — the F1 Challenge 99-02 weather system

**Hand this whole file to the investigating model as its task.** It is written
to be self-contained: it names the corpus, the tools, the trust rules, the
prior art to read first, and the exact questions that must come back answered.

---

## 0. Who you are and what the answer is for

You are investigating the weather system of *F1 Challenge 99-02*
(`F1 Challenge Online.exe`, non-ASLR, image base `0x400000`). This is the
owner's own offline copy; the work is hobbyist reverse engineering for a
modding project.

The answer feeds one concrete consumer. A safety-car / caution mod
(`mods/extended_vanilla_features/patches/full_safety_car/`) has just ported
NASCAR Thunder 2004's caution-declaration gates into F1C. NT2004 declares a
caution from exactly two sources: an incident scanner, and **weather** —
`sub_52B1D0` watches rain intensity and calls `sub_4F9D50` ("the steward has
thrown a caution") on the **rising edge across 0.18**. The incident half is
ported and working. The weather half cannot be ported because **nobody has
located F1C's rain-intensity value**.

So the minimum useful result is: *a readable, live-verified number that says
how wet it is right now, and where the transitions happen.* Everything else in
this brief is context that makes that number trustworthy and more useful.

Do not write the mod. Report findings.

---

## 1. Rules you must follow (these are the project's, not suggestions)

1. **Numbers come only from deterministic extraction or live measurement,
   never from inference.** Offsets, addresses, literals and guards are
   extracted or measured. Prose is a lower-trust layer on top.
2. **Label every claim** with a tier: **FACT** (poke-proven live) >
   **CONFIRMED** (two independent sources) > **DERIVED** (grounded, unproven)
   > **GUESS**. Never launder a lower tier into a higher one. A claim you
   cannot ground in a `.c` file, the IDB, a config file, or a live read is a
   GUESS or is left out.
3. **A value holding under a write is NOT proof the game consumes it.** Many
   offsets latch a poke and change nothing. Prove consumption by an observable
   effect, or say plainly that you did not.
4. **Record disproven results too.** "This offset is not wetness" is worth as
   much as finding the one that is, and it is what stops the next session
   re-deriving it.
5. **Never invent a tool.** Everything you need already exists — see §3.
6. Two offsets kill the process instantly on write: `veh+0x10C0` and
   `veh+0x33C8`. Never write them.

---

## 2. Read this prior art BEFORE touching anything

From the corpus root `%USERPROFILE%\Desktop\f1c_ida_decompiled`:

```bash
python tools/ask.py findings weather
python tools/ask.py findings rain
python tools/ask.py findings wetness
python tools/ask.py token Wetness
python tools/ask.py token RainChance
```

`tools/ask.py` joins all eight fact stores in one call and puts a trust tier on
every line. Use it before grepping anything by hand — the stores use three
different key formats and hand-joining them has wasted a session before.

### The strongest lead is inside this repository already

`mods/f1cui_web_remote/web_telemetry.c` is a working web-dashboard mod by this
same project, and **it already reads and displays live weather**. Its header
comments carry the derivation, including a correction of an earlier wrong
reading. Take these as the starting addresses:

| Address | The mod's name for it | What its comments say |
|---|---|---|
| `0x008AFEE8` | `OnLineWetness` | wetness pair, 0..1, consumed as a grip multiplier. **This is what its dashboard shows as the wet percentage** |
| `0x008AFEEC` | `OffLineWetness` | the other half of the pair |
| `0x008AFEF0` | `AmbientTempC` | corroborated: `PitRaceStatusMessageDispatcher` tests `flt_8AFEF0 > 30.0` for a hot-weather line, which pins the address independently of any struct guess |
| `0x008AFEF4` | track temperature | |
| `0x007CF3A4` | `TrackWetnessFactor` | **NOT a wetness percentage.** Read a flat `0.50` in a bone-dry session; `docs/12` calls it a grip factor consumed by `FUN_004C8920`. The mod says explicitly it uses the pair above *instead of* this |

Two things follow, and both are jobs for you:

1. **Verify the four addresses independently.** They were derived by that mod,
   not poke-proven here. `0x8AFEF0` has a static corroboration (the 30.0
   compare); the others do not, as far as the corpus records. Confirm or
   disprove each, and give the tier.
2. **The owner's own observation, 2026-08-03, is the real question:** the web
   dashboard shows *one* rain-intensity percentage, and he does not believe it
   represents the whole track — "wet spots" are not covered. An on-line /
   off-line pair is two *lines*, not two *places*: it says nothing about one
   corner being wet while the rest dries. So:
   **is wetness in F1C a global scalar pair, or is it stored per track
   segment/sector/zone?** This is question A6 below and it is the one that most
   changes what can be built.

   Where to look for a per-place answer: the Steward object at `0x8ACEE0`
   already carries a **29-entry marshalling-zone table** (`+0x108` scale,
   `+0x10C` count, `+0x110` array of `{int32 state; float end}`, rebuilt every
   frame by `sub_4D1730`) — proof that the game already thinks about the lap in
   segments. The track's `.aiw` waypoint/segment data is the other candidate.
   If there is per-segment wetness anywhere, it is most likely hanging off the
   same segment structures.

Known starting points, all already in the corpus:

- `name_index.json` carries **`OnLineWetness`**, **`OffLineWetness`** and
  **`AmbientTempC`** as named symbols. **Their tier and their base object are
  exactly what you must establish** — a name in that file is not proof of a
  live-correct address, and this project has been burned by assuming the base
  (a whole document once printed a player model as `veh+N` when the object was
  a different global entirely).
- `config_schema.json` / `CONFIG_SCHEMA.md` pair 771 config tokens with the
  offset each is parsed into. Weather tokens live in the track `.gdb` and in
  the player `.plr`. Start there, not in the disassembly.
- `data_index.json` gives the real value ranges of every key in the 1,587
  shipped data files — so you can see what range a weather field actually
  takes across all shipped tracks before you ever read memory.
- `strings_index.json` / `strings_modules.json` map every string literal to
  the original 2002 `.cpp` file it was compiled from. If there is a
  `weather.cpp`, that is your function list handed to you.

---

## 3. Tools — use these, do not write new ones

All under corpus root `tools/`. **Never** use anything in the sibling
`F1C_ghidra_project\tools\` — deprecated copies with wrong paths.

| Need | Tool |
|---|---|
| one-shot answer joining all fact stores | `tools/ask.py func\|offset\|global\|token\|findings` |
| read/write live memory | `tools/poke.py list\|read\|watch\|write` |
| snapshot memory, act in game, diff | `tools/snapshot.py` |
| can this offset be poked at all | `persistence_map.jsonl` (all 4,656 vehicle offsets already measured) |
| name an unknown per-driver offset | `tools/match_values.py` (matches live values against the game's own `.rcd`/data files) |
| function dossier: callers, callees, ret-imm | `tools/mod_pipeline/asm_tools.py resolve <addr>` |
| map who touches a behaviour's globals | `tools/mod_pipeline/mod_pipeline.py map <seed>` |
| live hooks / breakpoints | Frida, x64dbg-x32dbg and Cheat Engine MCP servers are already wired up — see `tools/MCP_TOOLS.md` for proven recipes and the hard gotchas |

Two gotchas that have each cost a session:

- **A Frida MCP hook cannot survive a track load.** The bridge tears the script
  down between calls, so hooks detach before the user can start a session. If
  you need to observe something across a load, the pattern that works is an
  IAT hook inside a persistent DLL — `patches/full_safety_car/dll/pace_probe.c`
  has a working `CreateFileA` example (`ftrace_start`).
- **Frida hardware watchpoints crash this process.** Use x32dbg `bphws`
  instead. `Interceptor` is fine.

---

## 4. What is already known from outside the corpus: the F1CUI mod

`%USERPROFILE%\Desktop\F1CUI\` ships `F1CUILive_v0.8.5.1.jar` (and an older
`v0.7.3.2` that additionally logged race data). It is a Java overlay that reads
and writes this game's memory live, and **it already has a working weather
subsystem** — so it is a second, independent source for anything you find, and
in places it is ahead of the corpus.

Unzip the jar and read the classes (they are not obfuscated; `strings` alone
gets you most of it, `javap -p -c` gets the rest):

| Class | What it proves exists |
|---|---|
| `f1p_businessLogic/f1c_weatherRebuildWatcher` | It watches **a weather FILE** on disk (`lastModified` polling), rebuilds it, and carries `trackWetness`, `trackTemperature`, `avgConditions`, `avgTemperature`, `defaultTempPerSession`, and a `TreeMap<Float,Float[]>` named `theBell` — i.e. it models the weather as a **distribution**, not a scalar |
| `f1p_input/f1c_memoryReaderAndWriterEngine` | `checkOnWetStartingRace`, `areWeatherConditionsAtTime0Rainy`, `updateAndTranslateQualyWeatherDynamic`, `updateWeatherTimeFromGDBTrack`, `mustTriggerRainyRaceLogic`, `setLightsOnWeatherConditions`, `updatePlannedPitsInCaseOfStartingRain`, `RainAbility` |
| `f1p_dataContracts/f1c_magicData` + `f1p_resources/f1c_resMagic` | Its own settings: `WeatherVolatilityInSession`, `WeatherVolatilityBetweenSessions`, `WeatherTypicalWeather`, `WeatherWetFactor`, `WeatherTypicalAirTemp`, `WeatherTypicalTrackTemp`, `WeatherCustomEnableMode`, `DrsDisabledWhenRainy` |
| `f1p_dataContracts/f1c_telemetryData` | `hasRaceBeenDeclaredRainy`, `raceStartingAsRainy`, `isAWetCompound` |
| `F1CUILivePlugin.ini` | `F1CStandardWeatherFile = Weather.txt` — the game reads a **`Weather.txt`**, and `updateWeatherTimeFromGDBTrack` says the track `.gdb` feeds it |

**Treat all of that as a lead, not as fact.** F1CUI is a third-party mod; its
field names are its author's, its addresses are unverified here, and it may be
reading a value it believes is wetness without that being what the physics
consumes. Its value is that it tells you **where to look** and **that the
answer is reachable**, which is more than the corpus currently has.

There is also one already-proven anchor in this project's own history: the
**F1CUI anchor model** — a vehicle offset equals `0x4714 − F1CUI_sub`, i.e.
that mod's own subtraction constants map onto the corpus's vehicle-struct
offsets. If F1CUI reads weather off a vehicle, that relation converts its
number into a corpus offset directly.

---

## 5. The questions. Answer these, in this order

### A. The live wetness value — this is the deliverable

1. **What address holds "how wet is it right now", and what is its base
   object?** Global, per-track, per-vehicle, or a child of the Steward
   (`0x8ACEE0`)? Name it, give its type, and give its range.
2. **Is `OnLineWetness` / `OffLineWetness` in `name_index.json` correct, and
   at what tier?** Two values suggest a dry line and a wet off-line — confirm
   or disprove that reading, and say which one a caution should watch.
3. **What is its resting value in the dry, and what does it read in the wet?**
   Live reads, both, with the user confirming what is on screen.
4. **Is it recalculated per frame, or does it latch?** Check
   `persistence_map.jsonl` first if it is a vehicle offset; measure otherwise.
   This decides whether a mod can force weather or must only read it.
5. **What writes it?** The writer function is what a mod would hook if the
   value turns out to be recalculated.

### B. The transitions — what a caution trigger needs

6. **IS WETNESS GLOBAL, OR PER PLACE?** (The owner's question - see the F1CUI
   and web-dashboard section above. Answer this one even if you answer nothing
   else.) One number for the whole circuit, or a value per segment, sector or
   marshalling zone? If it is global, say so plainly and prove it - that is a
   real, useful, closing answer. If it is per place, give the table's base,
   stride, count and index function, exactly as the zone table at
   `0x8ACEE0+0x110` is documented.
7. **Is there a rain-INTENSITY scalar distinct from track wetness?** NT2004
   separates them: intensity is what falls, wetness is what the track holds.
   F1C may or may not - and the on-line/off-line pair is a distinction between
   racing LINES, which is not the same thing as either.
8. **Are there thresholds in the code** — the F1C equivalent of NT2004's
   `0.18` rising-edge test? Find the compare constants and the functions that
   own them.
9. **Does anything in F1C already react to the transition?** Look for the
   wet-tyre decision, the AI's `RainAbility`, the headlight/rain-light logic
   F1CUI's `setLightsOnWeatherConditions` implies, and any commentary event.
   The commentary catalogue in `mods/extended_vanilla_features/data/` has a
   `weather_track` clip category with 10 clips — check whether any of them
   have a live emitter (`data/emitter_index.json` maps eid to emitting
   function; **read it before concluding a clip is unwired**).

### C. The configuration — where weather comes from

10. **Which config tokens define weather, in which file, and what does each
   parse into?** Track `.gdb` and player `.plr` at minimum. Use
   `config_schema.json`; give the parser function and the offset per token.
11. **What is `Weather.txt`?** F1CUI names it and rebuilds it. Find the parser,
    the format, and whether the game re-reads it during a session or only at
    load. If it is re-read live, that is a mod lever with no code at all.
12. **How does a session choose its weather** — fixed from the `.gdb`, rolled
    at load, or evolving during the session? F1CUI's "volatility in session"
    and "volatility between sessions" settings imply it evolves; prove or
    disprove that against the game's own code.

### D. Consumption — does the weather actually do anything

13. **What does wetness change in the physics?** Grip is the obvious one
    (`generic.tbc` / the tyre model), but say what you can prove. If a value
    latches under a poke and nothing on track changes, say so — that is a
    DISPROVEN finding and it is valuable.

---

## 6. How to deliver

1. **Append every result to `findings.jsonl` at the corpus root**, one JSON
   object per line, same schema as the existing rows: `id`, `date`, `tier`,
   `claim`, `evidence`, `source`, and `note`. Include the DISPROVEN ones.
   Give the disproven rows the same care as the positive ones.
2. **Write one report** at
   `mods/extended_vanilla_features/patches/full_safety_car/WEATHER_REPORT.md`,
   structured as: what is FACT, what is CONFIRMED, what is DERIVED, what was
   DISPROVEN, and what is still OPEN. Lead with the answer to question A1.
3. **If, and only if, you land a live-verified rain threshold**, add one
   paragraph at the end saying precisely what a caution trigger should read
   and compare against — address, type, threshold, and whether it needs a
   rising-edge test or a level test. That paragraph is what gets built from.
4. Say plainly what you could not measure and why. An honest OPEN beats a
   confident GUESS; the next session pays for the difference.
