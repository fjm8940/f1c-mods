# 54 — Championship and career static architecture

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)  
**Date:** 2026-08-30  
**Status:** EXHAUSTIVE STATIC PASS WITH PRIOR LIVE RACE-STATE OBSERVATION  
**Scope:** championship/career state, scoring, standings, persistence, season selection, and validation backlog

## Abstract

The championship system is not a thin UI mode. It is a persistent profile subsystem
whose `.bch` file contains lifetime career totals, race-condition/game-option choices,
per-track statistics, and one or more 45,396-byte in-memory season objects. Each season
object holds 103 opponent slots, one player slot, current-season progression fields,
and up to 48 finish/points records per entrant. Post-race scoring is guarded against
duplicate execution and uses a 90% race-distance eligibility test before loading the
position-to-points table.

The structural persistence contract is now bounded. The exact natural transition from
the results screen to the next championship event, malformed-file tolerance, and the
meaning of several mode/status enum values still require runtime validation. **DERIVED**

The exhaustive static follow-up additionally closes the owning menu/session call path,
the entrant initialization semantics, and most lifetime-stat update fields. It also
finds a concrete malformed-save trust boundary: opponent section indices are not
locally range-checked before address calculation. **FACT**

## Evidence policy

- Raw Hex-Rays shown by `python tools/f1c_mod.py inspect` is the primary code evidence.
- Recovery names and filled skeleton prose are navigation aids, not proof.
- `FACT` below means a literal field, call edge, comparison, or string is present in raw
  code. `DERIVED` means the stated role follows from several such facts. `OPEN` means the
  corpus does not settle the claim.

## 1. Subsystem boundaries

The static call graph separates the subsystem into five cooperating surfaces:

1. profile `.bch` path creation and load/save orchestration (`0x004B02B0`,
   `0x004B0720`, `0x004B0770`, `0x004B0930`);
2. career/profile and season serialization (`0x004C03F0`–`0x004C2700`);
3. race-result scoring and ranking (`0x004C0910`, `0x004C0F60`, `0x004C2A80`);
4. championship presentation and constructor aggregation (`0x0041E320`,
   `0x0041F220`, `0x0041F8A0`, `0x00420820`, `0x00420CF0`);
5. game/session setup coupling (`0x0042D5D0`, `0x00463EF0`).

This is a profile-level persistence system coupled to the steward/session code, not a
standalone standings table. **DERIVED**

## 2. Persistent file and replacement lifecycle

`NamePathFormatterWithLengthCheck` (`0x004B02B0`) appends the object/profile path,
strips its existing extension, and appends `.bch`, with an explicit caller-supplied
buffer bound. **FACT**

The load path is:

`PlayerFileInit / profile-name change -> F_5StepInit (0x004B0720) -> open .bch ->
GameOptionsWriter (0x004C1E80) -> career/track/season parsers`. **DERIVED**

Despite several misleading recovery names, raw code shows that `GameOptionsWriter`
parses input. It allocates exactly 45,396 bytes for every `[CAREERSEASON]` section,
initializes the object, calls `CareerSeasonStateFileReader`, and appends successful
objects to a linked list. Parse failure destroys the new object rather than inserting
it. **FACT**

The save/update path passes through `sub_4B0770`. It constructs a temporary pathname,
opens/dispatches the existing structured file, invokes `SettingsFileInit` and the
career writer chain, then calls a delete wrapper for the prior target and a move wrapper
to replace it. This is a temporary-file replacement strategy, but the delete-before-
move ordering leaves an unmeasured interruption window. Return values from the delete
and move wrappers are logged but are not checked by the visible caller. **DERIVED**

No championship-specific checksum, schema-version field, or transactional journal was
found in this pass. Absence from the inspected path is not proof that no generic parser
validation exists. **OPEN**

## 3. File grammar and stored records

The following section and key strings are directly cross-referenced by the reader and
writer paths. **FACT**

### 3.1 Profile/lifetime record

The profile record includes:

- `AbortedSeasons`, `TotalLaps`, `TotalRaces`, and `TotalRacesWith...`;
- `TotalPointsScored`, `TotalChampionships`, `TotalWins`, `TotalPoles`, and
  `TotalLapRecords`;
- average start position, finish position, race distance, and opponent strength;
- mechanical-failure rate;
- race-condition reconnaissance, walkthrough, format, flag-rules, weather, and
  time-scale choices;
- qualifying, race-lap/time/length, opponent-strength, fuel-multiplier,
  tyre-multiplier, and speed-compensation options.

The reader recognizes `[CAREERSEASON]` and `[PLAYERTRACKSTAT]`; the writer emits those
records plus the surrounding career data. Per-track records contain track name, track
file, total laps, and best lap time. **FACT**

### 3.2 Season object layout

`CareerSeasonStateFileReader` receives a 45,396-byte object (`0xB154`). Its stable
layout is: **FACT**

| Offset | Size/count | Meaning |
|---:|---:|---|
| `0x0000` | `103 * 436` | Opponent standings records |
| `0xAF6C` | `436` | Player standings record |
| `0xB144` | 4 | `SeasonStatus` |
| `0xB148` | 4 | `CurrentRace` |
| `0xB14C` | 4 | `RaceSession` |
| `0xB150` | 1 used | `RaceOver` guard |

`103 * 436 == 44,908 (0xAF6C)`, exactly the player-record offset. The writer scans all
103 opponent records and serializes only records whose `Active` byte is non-zero.
**FACT**

### 3.3 Entrant record layout

Each 436-byte player/opponent record has this confirmed serialization surface: **FACT**

| Offset | Field |
|---:|---|
| `0x000` | name/string storage beginning (exact ownership still needs type recovery) |
| `0x018` | `FinishPosition00..47` integer array |
| `0x0D8` | `FinishPoints00..47` integer array |
| `0x198` | `SeasonPoints` |
| `0x19C` | `PointsPosition` |
| `0x1A0` | `PolesTaken` |
| `0x1A4` | `OriginalGridPos` |
| `0x1A8` | `CurrentGridPos` |
| `0x1AC` | `ControlType` |
| `0x1B0` | `Active` byte |

The writer stops the per-race array when it encounters a negative finish-position
sentinel or after 48 entries. The reader rejects indexed finish-position/finish-points
keys whose numeric suffix is 48 or greater. This establishes a hard 48-result capacity
for one saved season record. **FACT**

That protection does **not** extend to opponent section numbers. In
`CareerSeasonStateFileReader`, any token matching the `Opponent` prefix is converted to
an integer and passed to `CareerStandingsStateLoader(this + 436 * index)` with no local
`0..102` test. `SeasonStatus`, `RaceSession`, and `CurrentRace` are likewise assigned
directly from parsed integers, and `RaceOver` is only normalized to a boolean. A
malformed `.bch` can therefore select an entrant address outside the 45,396-byte season
object unless a lower parser layer rejects the token first. No such rejection is
visible in this call path. Treat `.bch` files as untrusted input and never fuzz the
owner's live profile. **FACT** for the missing local checks; **OPEN** for lower-layer
mitigation and practical exploitability.

The object has room for far more opponents than the normal grid. This is capacity, not
evidence that 103 opponents can participate simultaneously. **DERIVED**

Fresh-season population also trusts the configured opponent count in
`GameFilterDataLoader`: it asks `AIOpponentSelection` for that count and advances a
436-byte destination pointer once per selected opponent without a local `<= 103`
clamp. Normal option metadata may enforce the limit upstream, but this constructor does
not. This is a second input-boundary requirement for any mod or profile editor that can
alter opponent-count configuration. **FACT** for the absent local clamp; **OPEN** for
all upstream enforcement.

## 4. Race result to championship state

### 4.1 Scoring an entrant

`SeasonScoring` (`0x004C0910`) is called by
`RaceStandingsRankingsComputation` (`0x004C0F60`) for the player and every active
opponent. In the post-race state (`dword_7D00EC == 7`) it:

- stores the vehicle's finishing-position value into the current result slot;
- defaults awarded points to zero;
- rejects points when vehicle/result state at vehicle `+0x4064` equals `3`;
- requires completed distance at vehicle `+0x403C` to be at least
  `floor(TotalRaceLaps * 0.9)`;
- loads the points value from `SeasonScoringInfo` using finish position minus one;
- writes per-race points and adds them to `SeasonPoints`;
- copies the current result-slot index into the record's current index field.

The 90% threshold therefore gates championship points, while the finishing position is
still recorded before the eligibility test. **FACT**

### 4.2 One-shot result commit and ranking

`RaceStandingsRankingsComputation` exits immediately when `RaceOver` is already
non-zero. Otherwise it scores the player and 103 potential opponents. If
`RaceSession == 7`, it computes each active entrant's `PointsPosition` by counting how
many active entrants have strictly greater `SeasonPoints`; ties therefore receive the
same zero-based rank. It finally sets `RaceOver = 1`. **FACT**

This flag is the visible duplicate-commit barrier. Clearing it would allow the same
result path to run again; safe mod interfaces must not expose it as an unguarded write.
**DERIVED**

### 4.3 Weekend and calendar progression

The recovery name `PitStopPhaseAdvance` at `0x004C15A0` is incorrect for this call
path. Raw code advances the saved `RaceSession` value through `1 -> 2 -> 3 -> 4 -> 5
-> 6 -> 7`, clearing `RaceOver` at every step. On phase 7 it commits standings and
calls `TimerCountdownWithStateTransition` (`0x004C1670`). **FACT**

When the phase-7 commit has set `RaceOver`, `0x004C1670` increments `CurrentRace`,
compares it with `SceneOrderReader()`, sets `SeasonStatus = 3` when the new index reaches
the calendar length, resets `RaceSession = 1`, and clears `RaceOver`. This closes the
previously open static question of which function advances the championship calendar.
The menu/event that invokes the two callers of `0x004C15A0` remains to be verified
live. **FACT/OPEN**

New-season construction at `0x004C0DB0` sets `SeasonStatus = 2`, `CurrentRace = 0`,
initializes the player and all 103 opponent slots, selects opponents from the game
filter, and marks the selected entries active. Fresh object construction at
`0x004C0AB0` instead starts with `SeasonStatus = 0`, `CurrentRace = -1`,
`RaceSession = 1`, and `RaceOver = 0`. **FACT**

`NodeAllocListInsert` (`0x004C2780`) removes the currently selected season when its
status is `2`, then allocates and appends a new status-2 season. This proves status `2`
means the replaceable active/in-progress season in this list workflow; status `3` is
the calendar-exhausted/completed season. Status `0` is the constructor default, while
status `1` remains unused in every inspected direct writer and is still **OPEN**.

The session/menu ownership is now static rather than merely inferred. `GameMode`
(`0x0046D6D0`) routes mode `4` to `SessionSceneInitChain`, which allocates/loads the
season, selects the current calendar entry, and opens `CH_RESUME`. Its session advance
handler (`0x004A0590`) changes global weekend phase `1..6` to the next phase and, only
for mode `4`, advances the saved season phase. A separate post-session presentation
handler (`0x004A0760`) also commits the saved phase, chooses driver/constructor win
presentation, refreshes the next event, and resolves championship messages. **FACT**

Calendar entries come from the active database's `SceneOrder` linked list. The current
race index selects one entry, resolves its season-race section, loads the referenced
track scene, and reads `QualifyLaps`, `QualifyTime`, `RaceLaps`, and `RaceTime`; missing
numeric keys become `-1`. Calendar completion compares the incremented race index with
the `SceneOrder` list count. Thus the saved season does not embed its calendar or
scoring table: both are re-resolved from the currently active database, which makes a
mid-season database change a compatibility hazard. **FACT/DERIVED**

### 4.4 Steward/player aggregate update

`SeasonScoringInfoUpdater` (`0x004C2A80`) is called by `Steward_Post` and another
post-processing caller. It requires the scoring gate to pass and rejects global
`ScoringGate == 2`. Race-specific work requires `SessionType == 7`; final scoring work
requires `ChampionshipMode == 3` or `4`, the player result state not equal to `3`, and
the same 90% distance threshold. It also updates per-track lap totals/best-lap-like
data and lifetime aggregates. **FACT**, with individual aggregate roles still
**DERIVED**.

The entrant's `PolesTaken` field is no longer ambiguous: when global session type is
`5`, `SeasonScoring` copies the vehicle's zero-based grid/result value into
`CurrentGridPos` and increments `PolesTaken` exactly when the vehicle position equals
`1` before subtraction. During session type `7`, it records the finish position and
points as described above. The only explicitly excluded vehicle-result state in both
championship scoring paths is numeric state `3`; static code does not name that state
precisely enough to distinguish DNF from DQ, so that label remains **OPEN**. **FACT**

The lifetime profile offsets updated by the steward paths can now be tied to their
serialized fields. `+0x24` accumulates completed laps; `+0x28` counts processed races;
`+0x30` accumulates eligible championship points; `+0x34` increments when a completed
season's player record passes its visible winner test; `+0x38` increments for a
position-1 race result; and `+0x3C` increments for a qualifying position-1 result.
The running averages at `+0x44`, `+0x48`, `+0x4C`, and `+0x50` are updated from start
position, finish position, race-distance percentage, and opponent-strength inputs,
respectively, with clamps visible in their helpers. The exact meaning of the persisted
`TotalRacesWith...` and lap-record counter still depends on unresolved source strings
or caller semantics. **FACT/DERIVED**

## 5. Driver and constructor standings

`ChampionshipPointsAccumulator` (`0x0041F220`) aggregates indexed scoring categories
for matching names in the shared standings container. Passing `-1` selects a separate
total-points accessor. It is reused by driver classification, constructor standings,
season selection, timing/difference messages, and the function labelled as constructor
clinch detection. **FACT**

`ConstructorStandingsParser` (`0x00420820`) hard-codes 14 constructor names, finds the
top three by accumulated total, detects ties, and conditionally emits presentation
entries. The hard-coded list contains teams from the 1999–2002 data set; modded team
names outside this list cannot participate in this exact constructor-presentation path
without code changes. **FACT/DERIVED**

`GdbSeasonSelector` (`0x00420CF0`) compares the active season path against
`F1_1999.gdb`, `F1_2000.gdb`, `F1_2001.gdb`, and `F1_2002.gdb`, chooses a season ID,
and resolves team/driver presentation data with tie-breaking. **FACT**

The current recovery label `ConstructorChampionshipClinchDetection` at `0x0041F8A0`
must remain provisional. Its raw loop begins with a zero-category prefix, which makes
the filled-skeleton interpretation of a mathematical future clinch suspicious. It is
proven to compare the 14 hard-coded teams across cumulative category prefixes; the
precise message meaning needs call-site and runtime confirmation. **FACT/OPEN**

## 6. Mode and session coupling

`ChampionshipMode` at `0x007D0410` is a legacy name for the broader race-mode selector.
Static menu routing now proves value `4` is championship and value `5` is multiplayer:
`GameMode` sends `1..3` to circuit selection, `4` to `CH_RESUME`, and `5` to the host or
client grid. Value `6` is a special transient mode reset to `1` on return to the main
menu. The user-facing distinctions among values `1`, `2`, and `3` remain **OPEN** even
though they share the single-event circuit-selection path. **FACT**

The function carrying the legacy SDK name `Season_Championship_State_Init`
(`0x00463EF0`) is called by `Game_Setup`. It resets an object state field and, when
`ChampionshipMode == 5`, derives two session/player-count globals from bounded positive
or negative configuration values. Its recovery name overstates what the raw function
alone proves. **FACT**

## 7. Runtime checks performed during this investigation

The supported read-only command `python tools/f1c_mod.py tool-run read-standings` was
run while the user reported the game running. It returned `RaceActiveFlag=0`,
`LapCount=0`, and `VehicleCount=0`, with no live slots. This is a useful negative
result from the normal-integrity terminal, but it was superseded by the elevated
read-only check below. **OBSERVED**

A supported game-window capture was also attempted to distinguish a menu state from a
race state, but the `PrintWindow` backend failed. No screenshot was produced and no
fallback interaction was attempted. **OBSERVED**

Using the owner's previously requested elevated, hidden PowerShell route, the supported
front door then captured a read-only module snapshot from PID 4836 with 1,530 pages,
6,266,880 logical bytes, and zero unreadable pages. Its manifest records the module at
base `0x00400000` and shows that the running executable is under the owner's Documents
tree rather than the governance-declared workspace game path. **OBSERVED**

An elevated `read-standings` call immediately afterward returned
`RaceActiveFlag=3`, `LapCount=53`, and `VehicleCount=22`. Twenty-one AI entries were
already `FIN` or `DNF`; the player entry was still `RUN`, so the race-to-results and
championship calendar commit had not yet occurred. This validates live race-state
availability but does not validate saved championship mutation or the phase-7 advance.
**OBSERVED**

One immediate follow-up read was unchanged, including the player still `RUN`; polling
was stopped because no transition was occurring. **OBSERVED**

No game memory, game file, save, executable, or production artifact was modified.

## 8. Modding implications

The safest useful future APIs are read-oriented:

- enumerate career seasons and their current event/session state;
- read player/opponent standings and 48-event history;
- read lifetime and per-track statistics;
- expose post-race commit notifications after the `RaceOver` edge;
- identify scoring-table provenance and 90%-eligibility outcome;
- resolve the selected 1999–2002 season database.

Any write API needs a transaction boundary around the whole `.bch` structure. Updating
only `SeasonPoints`, `CurrentRace`, or `RaceOver` would leave result arrays, ranks,
lifetime totals, and track statistics mutually inconsistent. **DERIVED**

## 9. Remaining validation backlog

1. Capture a real championship save before race completion, after classification, and
   after advancing to the next event; compare the `.bch` copies without altering them.
2. Map the exact `SeasonStatus`, `RaceSession`, and `ChampionshipMode` enum values.
3. Identify the user-visible/menu events that reach the two callers of the proven
   `RaceSession`/`CurrentRace` transition functions.
4. Verify points, DNF/DQ, fastest-lap/pole, and tie behavior with controlled results.
5. Test whether the 48-result cap constrains long custom championships or only stored
   history.
6. With owner-approved disposable copies, test missing sections, unknown keys,
   truncated lines, invalid opponent indices, duplicate sections, and interrupted temp
   replacement. Never use the owner's only profile/save.
7. Determine whether generic structured-file code provides a version or checksum layer
   outside the inspected championship path.
8. Re-review `0x0041F8A0` from its presentation call site and capture the corresponding
   message before calling it a clinch detector.
9. On disposable copied profiles only, validate the unbounded `OpponentNN` section
   index and raw season/session/race integer trust boundary under a debugger.

## 10. Reproduction commands

All evidence was obtained through the supported front door, principally:

```powershell
python tools/f1c_mod.py search championship --corpus all --json
python tools/f1c_mod.py evidence-query token SeasonStatus
python tools/f1c_mod.py evidence-query token CurrentRace
python tools/f1c_mod.py evidence-query token FinishPosition
python tools/f1c_mod.py inspect 0x004B02B0
python tools/f1c_mod.py inspect 0x004B0720
python tools/f1c_mod.py inspect 0x004B0770
python tools/f1c_mod.py inspect 0x004B0930
python tools/f1c_mod.py inspect 0x004C03F0
python tools/f1c_mod.py inspect 0x004C0690
python tools/f1c_mod.py inspect 0x004C0910
python tools/f1c_mod.py inspect 0x004C0B70
python tools/f1c_mod.py inspect 0x004C0CC0
python tools/f1c_mod.py inspect 0x004C0F60
python tools/f1c_mod.py inspect 0x004C0DB0
python tools/f1c_mod.py inspect 0x004C15A0
python tools/f1c_mod.py inspect 0x004C1670
python tools/f1c_mod.py inspect 0x004C1E80
python tools/f1c_mod.py inspect 0x004C22A0
python tools/f1c_mod.py inspect 0x004C2700
python tools/f1c_mod.py inspect 0x004C2A80
python tools/f1c_mod.py inspect 0x0041F220
python tools/f1c_mod.py inspect 0x0041F8A0
python tools/f1c_mod.py inspect 0x00420820
python tools/f1c_mod.py inspect 0x00420CF0
python tools/f1c_mod.py inspect 0x0046D6D0 --corpus raw --full
python tools/f1c_mod.py inspect 0x004A0590 --corpus raw --full
python tools/f1c_mod.py inspect 0x004A0760 --corpus raw --full
python tools/f1c_mod.py inspect 0x004C2780 --corpus raw --full
python tools/f1c_mod.py search "SeasonStatus|CurrentRace|RaceSession|RaceOver" --regex --corpus raw --json
```

## Wasted-effort check

- Repeated reading: none material.
- Broad builds/tests: none; this was an investigation-only task.
- Avoidable failures/retries: one broad multi-query output was truncated; subsequent
  function inspection was narrowed. The supported `PrintWindow` capture backend also
  failed once and was not retried.
- Polling/idle time: none.

---

*Changelog: 2026-08-30 — initial championship/career architecture and prior live
availability check; exhaustive static follow-up added mode/session ownership,
status semantics, lifetime-stat flows, pole handling, and malformed-save boundaries.*
