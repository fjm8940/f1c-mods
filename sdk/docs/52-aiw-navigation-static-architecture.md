# 52 — AIW/navigation: what the track database owns and who consumes it

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)  
**Date:** 2026-08-30 · **Author:** OpenAI Codex · **Status:** STATIC + READ-ONLY RUNTIME BASELINE

## Abstract (TLDR)

- The file selected by the game as its **AI Database File** is a shared track-topology database, not just a racing-line input. **DERIVED**
- It supplies the main and alternate waypoint chains, track widths, grid slots, pit boxes, garage slots, lap/sector distances, fuel-use metadata, and several visual-weather values. **DERIVED**
- AI cars use the waypoint graph every update to reacquire their location, choose the main/pit chain, build a look-ahead window, estimate corner speed, stay inside available width, and plan overtakes. **DERIVED**
- The player-side physics and camera code also query the same waypoint graph. Grid/garage placement and steward lap-distance setup use the same database object as well. A malformed or experimental AIW can therefore cause effects well outside AI pace. **DERIVED**
- Each waypoint occupies 316 bytes in memory. Three file indices are converted after parsing into previous, next, and alternate/branch pointers. Derived positions, directions, lengths, widths, headings, and curvature samples are then calculated. **DERIVED**
- Grid indices are accepted only from 0 through 103, and pit-team indices only from 0 through 51, even when a file asks for larger allocation counts. Garage indexing lacks equivalent visible validation in the parser. **DERIVED**
- The safest future mod interface is read-only inspection first. Directly changing a waypoint pointer, count, or array address is high risk because the parser reallocates the arrays and multiple systems retain waypoint pointers. **DERIVED**
- Same-day, read-only elevated captures validated the fixed database base, active header, 316-byte record stride, sampled and boundary topology, every authored Monza grid/pit/garage placement, 22 live vehicle waypoint owners, both non-default waypoint event records, one complete natural main-to-pit-to-main routing cycle, and seven direct passages through event records 983–984. Cross-track field meaning, malformed-file behavior, lifecycle relocation, natural weather response, the exact event-node consumer effect, and mutation persistence remain pending runtime verification. **OBSERVED where stated below**

## 1. Introduction

This paper asks: **what does F1 Challenge's AIW/navigation system contain, when is it built, and which game systems depend on it?** The scope is the static chain from scene discovery and AIW filename selection through parsing, graph construction, derived-data passes, consumers, and teardown. It includes AI driving, grid/pit/garage placement, player-side path queries, cameras, steward logic, and debug rendering where those systems directly touch the same database.

The paper does not claim the semantics of every AIW value and does not define a public SDK ABI. Its architecture was first recovered statically; a later read-only live race supplied the bounded validation described in section 3. It also separates four things that older function names blur together: the scene catalogue, the selected `.AIW` path, the parsed AI database object, and the terrain/HAT collision loader.

## 2. Method: static analysis

The analysis traced string references, exact callers/callees, object offsets, allocation sizes, index guards, and the raw decompiled bodies around the following boundaries:

- scene catalogue discovery and selection;
- construction and registration of `.SCN` and `.AIW` paths;
- opening and parsing the `AI Database File`;
- allocation, initialization, link remapping, derived-data rebuild, and cleanup;
- direct database consumers and the major second-order AI consumers.

The primary evidence was the raw decompilation of the analyzed `F1 Challenge Online.exe`, queried through the component's supported static-analysis interface. Existing annotations and legacy names were used only as navigation aids. Technical conclusions remain **DERIVED** unless a paragraph or table explicitly labels the result **OBSERVED**.

No private credentials, game-installation files, production tree, or live memory were changed. The runtime work used only read-only process access through the supported component interface.

## 3. Method: runtime verification

The first attempt was blocked by an integrity-level mismatch. With explicit owner authorization, the supported commands were then launched by hidden elevated PowerShell while the reported full-weekend race was running. The successful work remained read-only: module/process snapshots; bounded named-field reads of the fixed header, sampled waypoint records, complete authored placements, and two event records; one vehicle-array snapshot; and passive rebuild/branch monitors. No input or memory write was sent to the game. **OBSERVED**

The live module was `F1 Challenge Online.exe`, PID 4836, loaded at `0x00400000` from `%USERPROFILE%\Documents\F1 Challenge V10`. The executable is version `1, 0, 2, 6`, size 3,141,632 bytes, SHA-256 `1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003`. The global AI database structure is directly based at `0x005EA2C0`; it is not a heap pointer stored at that address. Only its owned arrays are indirect. **OBSERVED**

The active header read returned the following slot-zero values. The vehicle-field reader was deliberately rebased onto the global structure, so its additional stride-based rows are adapter artefacts and have no AIW meaning; only row/slot zero is evidence here. **OBSERVED**

| Field | Live value |
|---|---:|
| waypoint count / pointer | `1204` / `0x1784438C` |
| lap / sector 1 / sector 2 length | `5781.2568` / `1716.8676` / `3605.1504` |
| `FuelUse` scalar | `120976.6953` (units/scale still unresolved) |
| grid / pit / garage pointers | `0x1330B528` / `0x04BA8008` / `0x175906B0` |
| pit lanes / grid capacity / pit capacity / garages per pit | `1` / `104` / `52` / `3` |
| rebuild phase / last grip blend / cursor | `0` / `1.0` / `-1` |
| `LeftHandedPits` | `0` |

For waypoint records 0 through 9, all 30 stored previous, next, and alternate pointers exactly matched `waypoint_base + parsed_index * 316`; every `-1` alternate index mapped to a null pointer. Record 0, for example, had next index 1 and pointer `0x178444C8`, previous index 942 and pointer `0x1788CE54`, and alternate index 1168 and pointer `0x1789E54C`. Its base position was `(-18.0961, 0.4028, 506.3146)`, active position `(-23.1075, 0.4033, 506.2154)`, segment length `5.1792`, and branch ID/bitfield were both zero. This directly validates the 316-byte stride and index-to-pointer remap on the requested ten-record window. **OBSERVED**

The initial placement pass sampled record zero at each declared base. A later complete authored-record pass read all 24 grid, 12 pit, and 36 garage entries. Every position/orientation matched the candidate file within decimal-text-to-float32 rounding; all final words were zero. Address progression and content agreement validate the live 28-byte stride for those authored entries. Garage flattening is `TeamIndex + GarageOrdinal * PitspotsCapacity`, yielding authored flat ranges 0–11, 52–63, and 104–115. Sampled unauthored capacity edges—including grid 103, pit 51, and garage flat 155—were readable and all-zero. This distinguishes authored content from allocated minimum capacity without claiming that every intermediate record was sampled. **OBSERVED + DERIVED indexing**

The persistent inline path buffer at header `+0x18` contained `SEASONDATA\CIRCUITS\Italy\2002_Monza\2002_Monza.AIW`. Combined with the process directory, this resolves to an external game asset of 392,785 bytes, SHA-256 `4A6928A14B1878AA479FD14D238EEC9CF9EDAF4256F10FC4E7F16891700E8703`. Its count, header scalars, record-0 values, ten-record links, and eight boundary samples agree with live memory. The opened file handle itself was not captured, so identity with that exact on-disk copy remains strongly supported rather than directly proven. **OBSERVED + external-asset comparison**

Static lifecycle review explains why no handle can be inspected during the running race: initialization opens the selected AIW into a local handle, passes it to the parser, and closes it before initialization returns. There is no persistent AIW handle field in the live database. Exact opened-handle identity would require observing the brief initialization window; it cannot be recovered later from a steady running session. **DERIVED**

On this asset, branch distribution is: ID 0 at records 0–942 and 1103–1167 (1,008 records), ID 1 at 943–1102 (160 records), IDs 2–25 at one record each (1168–1191), and IDs 106–117 at one record each (1192–1203). All eight live boundary probes exactly matched the candidate file's position, branch ID, bitfield, source indices, and remapped pointers. IDs 2–25 correlate with authored `GridIndex` 0–23 as `2 + GridIndex`; IDs 106–117 correlate with `TeamIndex` 0–11 as `2 + StartingGridCapacity + TeamIndex`. Their X/Z coordinates match the placement entries exactly and Y is consistently about `+0.05`. These IDs are selectors embedded in one pointer graph, not a universal taxonomy of disjoint physical routes. **OBSERVED for this asset; broader meaning remains DERIVED**

At waypoint 0, the live active position matched `base_position + wp_path[0] * wp_perp` with Euclidean residual `2.41e-6`; using `wp_path[1]` left residual `6.62e-2`. The stored rebuild blend was `1.0`. This proves the Path0 endpoint for the captured state only. Although the inspected asset comments call Path0 dry and Path1 wet, the full weather-response ordering and interpolation direction remain pending a natural grip/weather transition. **OBSERVED + external comment; no causal claim**

A 20-second, 10 Hz passive correlation window observed no change in rebuild blend, phase, or cursor; final slot-zero state remained `1.0 / 0 / -1`. A later header read also retained all four array pointers and count 1204. This establishes short same-session stability, not restart or post/setup lifetime. **OBSERVED**

A vehicle-side read found 22 active vehicles: one human-role slot and 21 AI-role slots. Every current-waypoint pointer was aligned to the 316-byte array stride and inside the 1,204-record allocation; every previous-waypoint pointer was also valid. During a subsequent 120-second, 5 Hz passive monitor, AI lap counters advanced from approximately 30–31 to 32–33, while all path selectors and call-to-box fields remained zero and emitted no transition event. This proves ordinary live vehicle-to-waypoint ownership and movement through the array, but it is only negative evidence for pit/branch transitions during that interval. **OBSERVED**

A delegated follow-up repeated the supported custom-recipe monitor for 40.125 seconds at 5 Hz: 200 samples, 22 readable slots, zero selector changes, and zero call-to-box changes. A separate pointer-triggered capture reached its 120-event cap on ordinary stride-aligned AI waypoint movement, with selector and call-to-box still zero. Thus movement through the graph is directly observable, but no natural pit request, branch switch, or passage through records 983–984 occurred in either bounded window. **OBSERVED negative result**

A later 900.438-second, 2 Hz passive monitor captured a complete natural pit-routing episode for AI-role slot 18 on lap 17. `CallToBox` rose on main-branch record 115; `PathSelector` changed 0→1 on pit-branch record 945; `CallToBox` cleared while still on pit-branch record 1009; and `PathSelector` returned 1→0 on main-branch record 1103. All four pointers were exactly aligned to the live base `0x1784438C` at the 316-byte stride, and the capture had 1,799 samples with zero unreadable reads. This directly validates the active asset's natural main→pit→main branch-boundary interpretation and shows that the pit request precedes branch entry and clears before branch exit. Records 983 and 984 lie between the captured pit records 945 and 1009, but the separate high-frequency pointer stream reached its retention cap before this episode; their passage and visible `(1,18)` effect remain bracketed rather than directly sampled. **OBSERVED natural transition + bounded limitation**

A subsequent high-capacity 1 Hz route capture retained 23,064 events without reaching its 35,000-event cap and observed selector entry at aligned records 943–955, plus record 867 for three slots. It still did not sample records 983 or 984. The first delegated summary contained hexadecimal/index transcription errors; its additive arithmetic correction is authoritative and confirms that all raw selector-entry pointers were aligned. After the user restarted the race, a 20 Hz capture retained 104,111 events and directly observed seven AI cars traverse record 983 followed by 984 with selector 1 and call-to-box 1. A separate 5 Hz service trace captured the full AI state sequence and 12 matched service intervals, and the fresh-race follow-up proved physical tread refresh and front-wing restoration during one natural stop. The service system is documented separately in `docs/53-ai-pit-stop-procedure-runtime-map.md`. **OBSERVED + reviewed correction**

The parser/event ambiguity was also narrowed. `wp_event` stores its first value as a float at waypoint `+0x84`, then converts its second and third values to integers at `+0x134/+0x138`; `wp_wpse` is the two-integer alternate syntax for those final two words. `AIComputePassScore` multiplies its normal result by `+0x84`. `AIRacingLine` treats code A `1` as an override path using code B instead of its normal computed look-ahead/pass-score threshold. Monza records 983 and 984 are the only non-default event records, both `(1.000,1,18)` on branch 1, and live memory matched those values exactly. Their visible effect was not isolated. **DERIVED consumer behavior + OBSERVED values**

For this Monza asset, `wp_bitfields=1` occurs on exactly records 1168–1203: the 24 grid-selector and 12 pit-team-selector pseudo-nodes. All 1,168 ordinary branch-0/1 driving records use zero. This is strong asset-local correlation with placement-selector nodes, but no direct primary-code consumer was recovered and it must not yet be treated as a universal flag definition. **OBSERVED asset distribution + unresolved semantics**

The shipped-data index broadens only the storage facts: 199 indexed files contain `Number Waypoint` (reported range 0–4384), 198 contain `wp_branchID` (0–213) and `wp_bitfields` (only 0/1), 179 contain `wp_event`, and 11 contain `wp_wpse`. The current public front door does not expose the per-file identities or complete histograms behind these aggregate ranges, so it cannot identify the zero-count or extreme event-value outliers as malformed assets. An inherited prior finding reports pit-lane entrance ordering valid across 209 AIWs, but its per-file manifest was not exposed and was not independently reproduced in this pass. **Aggregate corpus evidence + explicitly inherited finding**

The AI difficulty global read `92`. With live `WorstAdjust=0.85` and `MidAdjust=1.0`, the static `<=100` interpolation predicts `0.960000012`; the live header held `0.960000038` (absolute error `2.63e-8`). This validates the recovered difficulty-interpolation path for this initialization. **OBSERVED + DERIVED formula**

Transient opened-handle capture, exhaustive unauthored-capacity coverage, natural weather transition, and post/setup relocation remain pending. A coordinated feasibility audit found no owner-approved disposable game/track target and no supported front-door observer for the short-lived initialization handle. The handle is already closed in the running race, so capture requires a future initialization window after those prerequisites are approved and implemented. Any future work must remain read-only until a separately specified disposable-track mutation test is approved, and must abort on unexpected file writes, malformed counts, or effects outside the development installation.

## 4. Results

### 4.1 The system has four distinct layers

| Layer | Static anchor | What it actually does | Evidence |
|---|---:|---|---|
| Scene catalogue | `0x004C52C0` | Enumerates `.SCN` files, allocates 396 bytes per scene record, parses each file, and sorts the catalogue by `SeasonRaces` order. | DERIVED |
| Scene selection | `0x004C5690` | Reduces an input path to a basename and selects the matching 396-byte scene record. | DERIVED |
| Path publication | `0x004C54F0` | Copies the selected scene base path, constructs `<base>.SCN` and `<base>.AIW`, and registers those strings. It does not parse AIW content. | DERIVED |
| AI database | `0x004040A0` → `0x00402690` | Opens the configured `AI Database File`, parses track/navigation data into one global database object, and computes difficulty interpolation. | DERIVED |

The scene record stride is 396 bytes. The selected record contains the scene path beginning at `+0x04`, a scene basename/key beginning at `+0x104` (260 decimal), and a parsed/sorted value at `+0x188` (392 decimal). The exact meaning of the last value is not yet runtime-confirmed. **DERIVED**

`SceneDataParser` is therefore a misleading legacy name: it selects an already parsed scene record. The function named `AIW` is also misleading: it publishes paths. The actual AIW/track-database parser remains unnamed at `0x00402690`. **DERIVED**

The HAT/terrain loader at `0x0044C0E0` is a separate subsystem. It loads or generates height/collision data and must not be treated as the AIW parser. **DERIVED**

### 4.2 Load and lifetime sequence

The statically visible lifecycle is:

```text
game enter
  -> enumerate/parse .SCN catalogue
  -> select scene record
  -> publish .SCN / track-directory / .AIW paths
  -> Game_Init
       -> initialize global AI database defaults
       -> resolve "AI Database File"
       -> open file
       -> release prior waypoint array and clear its pointer/count
       -> release and replace grid/pit/garage arrays
       -> parse sections and waypoint records
       -> remap file indices to pointers
       -> compute first derived waypoint vectors
       -> initialize steward and SpecialFX consumers
  -> Game_Restart
       -> rebuild per-waypoint derived data in the existing allocation
  -> game/update loop
       -> refresh derived geometry in batches when grip changes
       -> AI, physics, camera, placement, steward, and render consumers query data
  -> Game_Post / GameExit
       -> free grid, pit, garage, and waypoint allocations
```

`Game_Init` calls the database initializer before the steward and SpecialFX initializers. Every AIW reparse invalidates and replaces all four owned array bases before parsing new records. `Game_Restart` performs a larger graph/contact rebuild **in place** and does not itself free or relocate the arrays. `Game_Post` frees all four allocations, clears the four pointers and waypoint count, and resets selected waypoint metadata; it does not visibly clear the feature/capacity counts, aggregate fields, or rebuild fields. Stale capacity metadata can therefore coexist with null pointers after post, so readers must gate on pointer validity as well as counts. **DERIVED**

The global database structure begins at `0x005EA2C0` (`dword_5EA2C0` in the decompiler). Existing notes call it `SetupScenePtr`; the direct parser, layout, consumers, and live header show that “global track AI/navigation database” is the narrower and safer description. Its array fields are pointers, but the global address itself is the structure base. **OBSERVED + DERIVED**

The resolved AI-database path is an inline 256-byte C-string buffer at `+0x18..+0x117`. It remains intact after file close, parser execution, and `Game_Post` in the visible static code, and is refreshed by the next `AIDatabase_Init`. Consumers must stop at the first NUL: the ordinary lookup path zero-pads the full buffer, but the fallback path only guarantees the first 200 bytes. **DERIVED; post-close persistence OBSERVED during the race**

### 4.3 Database header and arrays

| Offset | Size/shape | Static role | Notes | Evidence |
|---:|---|---|---|---|
| `+0x000` | float | `GrooveWidth` input | Also used during derived geometry. Direction/units pending. | DERIVED |
| `+0x004` | float | `GrooveWidthWet` input | Direction/units pending. | DERIVED |
| `+0x008` | integer | first aggregate boundary | Set to `1`. Live value: `1`. | OBSERVED + DERIVED |
| `+0x00C` | integer | pit-lane aggregate boundary | `Pitlanes + 1`. Live value: `2`. | OBSERVED + DERIVED |
| `+0x010` | integer | grid aggregate boundary | `Pitlanes + 1 + Startinggrid`. Live value: `106`. | OBSERVED + DERIVED |
| `+0x014` | integer | pit aggregate boundary | `Pitlanes + 1 + Startinggrid + Pitspots`; garages do not participate. Live value: `158`. | OBSERVED + DERIVED |
| `+0x018..+0x117` | 256-byte C string | resolved AI-database path | Inline-owned and persistent after file close; only the NUL-terminated prefix is meaningful. | OBSERVED + DERIVED |
| `+0x118` | float | difficulty-interpolated adjustment | Computed from global difficulty and the three parsed adjustment inputs; live value `0.960000038` at difficulty 92. | OBSERVED + DERIVED |
| `+0x11C` | float | `WorstAdjust` | Clamped to at least `0.5`. | DERIVED |
| `+0x120` | float | `MidAdjust` | No local clamp found. | DERIVED |
| `+0x124` | float | `BestAdjust` | Clamped to at most `1.3`. | DERIVED |
| `+0x128` | float | `RaceQualRatio` | Scale pending runtime verification. | DERIVED |
| `+0x12C` | byte | `LeftHandedPits` | Rewritten on every parse. | DERIVED |
| `+0x130` | 32-bit count | waypoint count | Defaults to two fallback records if no count is parsed. | DERIVED |
| `+0x134` | pointer | waypoint array | 316-byte elements; allocation stores its count four bytes before the returned pointer. | DERIVED |
| `+0x13C` | float | lap length | Parsed from `lap_length`. | DERIVED |
| `+0x140` | float | sector 1 length | Parsed from `sector_1_length`. | DERIVED |
| `+0x144` | float | sector 2 length | Parsed from `sector_2_length`. | DERIVED |
| `+0x148` | float | auxiliary track scalar | Initialized to `-1.0`; exact meaning pending. | DERIVED |
| `+0x14C` | float | fuel-use value | Parsed from `FuelUse`. | DERIVED |
| `+0x150` | pointer | starting-grid array | 28-byte position/orientation records. | DERIVED |
| `+0x154` | pointer | pit-slot array | 28-byte position/orientation records. | DERIVED |
| `+0x158` | pointer | garage-slot array | 28-byte records; flat index is `TeamIndex + GarageOrdinal * PitspotsCapacity`. The Monza authored ranges are 0–11, 52–63, and 104–115. | OBSERVED + DERIVED |
| `+0x15C` | count | pit-lane count | Also participates in aggregate range calculation. | DERIVED |
| `+0x160` | count | starting-grid allocation count | Raised to at least 104. | DERIVED |
| `+0x164` | count | pit-slot allocation count | Raised to at least 52. | DERIVED |
| `+0x168` | count | garage-slots per pit/team | Raised to at least 3. | DERIVED |
| `+0x16C` | phase | incremental rebuild phase | Cycles through three derived-data passes. | DERIVED |
| `+0x170` | float | last grip-blend value | Rebuild starts when it changes by more than about `0.05`. | DERIVED |
| `+0x174` | index | incremental rebuild cursor | Processes at most 20 waypoints per invocation. | DERIVED |

The grid, pit, and garage records are each 28 bytes: a three-float position at `+0x00`, a three-float orientation at `+0x0C`, plus a final four bytes whose meaning is not established here. **DERIVED**

### 4.4 Parsed sections and tokens

The parser recognizes the following groups. Capitalization below follows recovered strings; spelling variants should be verified against an actual development-track file before editing.

| Group | Tokens/records | Destination | Evidence |
|---|---|---|---|
| `[Features]` | `Pitlanes`, `Startinggrid`, `Pitspots`, `Garagespots` | counts and allocation sizes | DERIVED |
| `[GRID]` | `GridIndex`, position, orientation | 28-byte grid records | DERIVED |
| `[PITS]` | `TeamIndex`, `PitPos`, `PitOri`, `GarPos`, `GarOri` | pit and garage records | DERIVED |
| Header/global | `Number Waypoint`, `LeftHandedPits`, `WorstAdjust`, `MidAdjust`, `BestAdjust`, `RaceQualRatio`, `lap_length`, sector lengths, `FuelUse`, groove widths | database header | DERIVED |
| Weather/visual | intermediate/rain fog planes, colors, density, and rainy darkness | global visual parameters, not the waypoint record | DERIVED |
| Per waypoint | `wp_branchID`, `wp_path`, `wp_event`, `wp_wpse`, `wp_pos`, `wp_galpha`, `wp_groove_lat`, `wp_score`, `wp_perp`, `wp_width`, `wp_dwidth`, `wp_bitfields`, `wp_ptrs` | current 316-byte waypoint | DERIVED |

`wp_ptrs` is the waypoint-record commit/advance token in the parser: after it succeeds, the parser advances to the next 316-byte record and only then checks the declared count. This means record ordering is part of the effective file grammar and malformed excess records are not safely bounded before their field writes. **DERIVED hazard**

### 4.5 Waypoint record layout

The following layout includes only fields with direct parser or consumer evidence. Unknown gaps are intentionally left unnamed.

| Offset | Static role | Produced/consumed by | Evidence |
|---:|---|---|---|
| `+0x00..+0x08` | base `wp_pos` vector | parser; renderer; geometry queries | DERIVED |
| `+0x0C..+0x14` | segment delta/direction vector | post-parse pass; proximity/geometry | DERIVED |
| `+0x18,+0x1C` | two `wp_path` scalars | parser; wet/dry position blend | DERIVED |
| `+0x20..+0x28` | `wp_perp` vector | parser; derived lateral position and steering | DERIVED |
| `+0x2C..+0x38` | four `wp_width` values | parser; usable-width derivation | DERIVED |
| `+0x3C` | segment length | derived pass; AI speed look-ahead | DERIVED |
| `+0x40` | alternate/branch distance | derived pass | DERIVED |
| `+0x44,+0x48` | score components | parser; path/speed scoring | DERIVED |
| `+0x50,+0x54` | derived left/right usable widths | rebuild; AI steering/overtaking | DERIVED |
| `+0x58..+0x64` | four `wp_dwidth` values | parser; downstream width calculations | DERIVED |
| `+0x68,+0x6C` | derived offset widths | rebuild | DERIVED |
| `+0x70,+0x74..+0x80` | speed/curve inputs and samples | AI racing-line look-ahead | DERIVED |
| `+0x84` | first `wp_event` value; normal pass-score multiplier | parser; `AIComputePassScore` | DERIVED; live default/event value `1.0` OBSERVED |
| `+0x94..+0x9C` | active/blended waypoint position | rebuild; nearest queries | DERIVED |
| `+0xA0` | lateral/groove adjustment | rebuild | DERIVED |
| `+0xA4..+0xAC` | normalized forward vector | rebuild; nearest and steering tests | DERIVED |
| `+0xB8` | heading angle | rebuild; AI look-ahead | DERIVED |
| `+0xBC..+0xC8` | short curvature/look-ahead samples | rebuild; AI speed planning | DERIVED |
| `+0x10C` | previous pointer | remapped from index at `+0x120` | DERIVED |
| `+0x110` | next pointer | remapped from index at `+0x124` | DERIVED |
| `+0x114` | alternate/branch pointer | remapped from index at `+0x128` | DERIVED |
| `+0x118` | `wp_galpha` | parser; path-checker color/visibility | DERIVED |
| `+0x11C` | `wp_groove_lat` | parser | DERIVED |
| `+0x120..+0x128` | three file indices from `wp_ptrs` | parser; link-remap pass | DERIVED |
| `+0x12C` | `wp_branchID` / path selector | parser; nearest-query filtering; main/pit switching | DERIVED |
| `+0x130` | `wp_bitfields` | parser; direct consumer and general flag semantics pending | DERIVED; Monza placement-selector correlation OBSERVED |
| `+0x134,+0x138` | second/third `wp_event` values converted to integers, or two integer `wp_wpse` values | parser; `AIRacingLine` event override | DERIVED; Monza `(1,18)` values OBSERVED |

The loader allocates `316 * count + 4` bytes, constructs each record, and returns a pointer four bytes past the allocation base. The preceding word stores the element count for destruction. **DERIVED**

After all records are read, invalid link indices (`-1` or at/above count) are replaced with null pointers and reset to `-1`. Valid indices become pointers into the same allocation. The first derived pass then calculates the segment delta from the next record, or from the previous record when no next record exists. **DERIVED**

### 4.6 Derived-data pipeline

The parsed values are not the final values used by all consumers. Three important transformations follow:

1. **Link remap:** file indices become previous/next/alternate pointers. **DERIVED**
2. **Load/restart rebuild:** blended positions, normalized forward vectors, segment lengths, headings, usable widths, curvature samples, and some alternate-path relationships are calculated in the existing allocation. **DERIVED**
3. **Incremental grip rebuild:** `GamePostMainLoop` calls the state machine every tick. It starts when the current blend differs from `+0x170` by more than approximately `0.05`, or while cursor `+0x174` is not `-1`. Start sets phase/cursor to `0`, captures the blend, and processes at most 20 waypoints per tick through phase 0 (active positions), phase 1 (direction/length/width/heading/curvature), and phase 2 (relationship/proximity data). Completion restores phase `0` and cursor `-1`. **DERIVED**

The active waypoint position is statically equivalent to:

```text
blend = 1 - average(trackGripA, trackGripB)
lateral = lerp(wp_path_A, wp_path_B, blend)
active_position = wp_pos + lateral * wp_perp
```

The captured endpoint selected `wp_path_A` with residual `2.41e-6`, while `wp_path_B` was `6.62e-2` away. This verifies one endpoint but does not establish the full weather-response ordering or prove that stored `+0x170` is numerically identical to the local `blend` expression in every state. **OBSERVED + DERIVED**

### 4.7 Navigation query primitives

Three query styles form the core navigation API inside the executable:

| Function | Behavior | Evidence |
|---:|---|---|
| `0x004038E0` | Full linear nearest-waypoint search, optionally filtered by `branchID`; may advance to the next pointer after a plane-side test. | DERIVED |
| `0x00403AA0` | Local walk from a prior waypoint using previous/next pointers, with branch filtering and full-search fallback. | DERIVED |
| `0x00403640` | Nearest active path search with vertical weighting and an additional path-distance test; considers path IDs 0 and 1. | DERIVED |

These functions explain why pointer continuity and valid branch IDs are structural invariants rather than cosmetic metadata. **DERIVED**

### 4.8 AI-driving consumers

The per-frame AI entry `AIMainUpdate` calls the waypoint-steering update, racing-line planner, steering/overtaking controller, steering correction, and gearbox logic. **DERIVED**

The waypoint-steering update (`0x00426430`) performs the following:

- reacquires a nearest waypoint when no current pointer exists or the car is too far from it;
- performs a local nearest search from the previous result;
- fills a 20-pointer look-ahead window by following next or alternate links;
- switches the active branch/path selector when moving between path types;
- derives speed class, lateral bounds, local heading limits, and steering geometry from waypoint fields. **DERIVED**

`AIRacingLine` follows next/alternate pointers, uses waypoint score/speed/curvature fields, and fills a 20-value forward speed plan. `AISteering` consumes the look-ahead widths and paths for lane choice, opponent avoidance, and overtaking. `AISteeringRacingLineCorrection` also uses starting-grid points for a special recovery/start correction path. **DERIVED**

Changing topology can therefore affect not only which line the AI follows, but also its braking horizon, overtaking space, pit-path transition, recovery behavior, and apparent corner speed. **DERIVED**

### 4.9 Non-AI consumers

Direct references to the global database establish these additional consumers:

| Consumer family | Static use | Evidence |
|---|---|---|
| Grid and garage placement | Reads grid/pit/garage position-orientation arrays; assigns a team/pit index and one of up to three garage choices. | DERIVED |
| Vehicle physics | Queries the nearest active path for position/progress/recovery calculations and reads placement records. | DERIVED |
| Camera and replay/selection paths | Queries nearest path and interpolated grid coordinates for camera positioning and track previews. | DERIVED |
| Steward/session | Uses nearest waypoint distance, lap length, and scene timing polygons to build lap-distance bins and sector/finish handling. | DERIVED |
| Collision/proximity helpers | Rebuilds waypoint relationships and uses graph geometry for local path/proximity calculations. This is separate from HAT terrain collision. | DERIVED |
| SpecialFX/debug | Reads grid points and calls the waypoint path checker renderer; debug rendering follows the next-pointer ring and colors segments from `wp_galpha`. | DERIVED |

The path-checker renderer stops and logs an incomplete-path condition if a null next pointer is encountered, and aborts traversal after 12,000 links. This is a diagnostic traversal cap, not a proven maximum waypoint count. **DERIVED**

### 4.10 Hard limits and malformed-input hazards

| Constraint | Static result | Evidence |
|---|---|---|
| Grid index | Only indices `< 104` are written. Allocation count is raised to at least 104, but larger configured counts do not raise the parser's accepted index ceiling. | DERIVED |
| Pit/team index | Only indices `< 52` are written for pit position/orientation. Allocation count is raised to at least 52, but larger configured counts do not raise the parser's accepted index ceiling. | DERIVED |
| Garage count | Allocation count is raised to at least 3. No equivalent local validation of team index or garage ordinal is visible before the flattened address calculation. | DERIVED |
| Waypoint count | No explicit upper bound or multiplication-overflow guard is visible before `316 * count + 4` allocation. | DERIVED |
| Waypoint links | `-1` and indices at/above count are nulled/reset. Other negative indices are not visibly rejected and form pointers before the allocation. | DERIVED hazard |
| Parse order/count guard | `Number Waypoint` must establish the array before waypoint fields are meaningful. The count guard is reached only at `wp_ptrs`, after current-record fields have written and the cursor can advance one record past the allocation; excess textual records are not proven safe. | DERIVED hazard |
| Allocation failure | The visible path does not robustly recover from every nonzero-count allocation failure. | DERIVED |
| Fallback | With no parsed waypoint count, the game creates two synthetic records and seeds a minimal linked/branch state. The fallback's gameplay usability is unknown. | DERIVED |

These findings describe visible code paths, not a security assessment of arbitrary hostile files. Normal game assets are assumed to be trusted and well formed.

### 4.11 Ownership and mutation hazards

The database owns four reallocatable blocks: one waypoint array containing all path/branch metadata, grid records, pit records, and garage records. Reparse frees and replaces all four; post/exit cleanup frees them and clears their pointers, while some capacity metadata remains stale. Restart rebuilds in place. Vehicle AI state caches a current waypoint and a 20-entry look-ahead array; other subsystems also keep waypoint pointers transiently. A sequential steady-race process-snapshot comparison found 6,016 changed pages, 17 added, 40 removed, and no width-4 transition candidate; the capture launches were reported 52 seconds apart and their manifest timestamps are 28 seconds apart. A later long control compared snapshots around roughly 15 minutes of ordinary racing: 10,196 pages changed, 538 were added, 13 removed, and no transition candidate was found. During the same work, a 600.031-second AIW monitor collected 2,990 samples with zero rebuild/blend events and retained phase 0, cursor -1, and blend 1.0; pre/post selected path, count, and all four allocation pointers were byte-identical. A separate 900.156-second lap monitor recorded 199 ordinary monotonic +1 lap changes across 22 active slots, confirming continued racing rather than a session transition. These controls demonstrate ordinary process churn and stable AIW state, but no weather rebuild or session transition occurred and therefore no causal lifecycle relocation was tested. **DERIVED ownership + OBSERVED long bounded negative result**

Consequences for future tooling:

- Reading the header and copying record values is comparatively low risk once the database pointer and counts are validated. **DERIVED**
- Changing scalar fields in place may be overwritten by parse, restart, or the incremental grip rebuild. **DERIVED**
- Changing source values without rebuilding their dependent derived fields can create internally inconsistent geometry. **DERIVED**
- Changing counts, array pointers, or link pointers in place is high risk because consumers assume contiguous 316-byte records and valid graph links. **DERIVED**
- A file-level edit loaded through the normal parser is safer than an arbitrary live pointer edit, but malformed-file guards are incomplete. **DERIVED**

### 4.12 Recommended future SDK surface

No SDK ABI change is justified by static evidence alone. The useful staged surface is:

1. **Read-only database snapshot:** validated header, counts, selected path, and copied placement records. **DERIVED recommendation**
2. **Read-only waypoint snapshot:** base/active position, widths, branch ID, bitfields, link indices reconstructed from pointers, and selected derived values. **DERIVED recommendation**
3. **Consistency checker:** count/allocation sanity, pointer-in-array checks, closed-chain checks per branch, grid/pit/garage index checks, and zero-vector warnings. **DERIVED recommendation**
4. **Offline AIW validator/editor:** preserve unknown tokens and ordering; make changes on a development copy; reload through the normal game path. **DERIVED recommendation**
5. **Only after runtime proof:** bounded scalar mutation with explicit rebuild/restart semantics. Pointer/count mutation should remain unsupported unless the ownership lifecycle is independently verified. **DERIVED recommendation**

## 5. Discussion

This static map is complete for the recovered loader, primary database layout, graph construction, lifecycle, and direct consumer categories. It is not a semantic dictionary for every unknown byte in the 316-byte waypoint record. Several older names are actively misleading, and some decompiler register recovery around the garage record syntax is ambiguous. Those ambiguities are preserved rather than filled with conventions from related games.

The strongest architectural conclusion is the shared ownership boundary: the global database joins file selection, placement, AI navigation, player/path recovery, camera behavior, steward distance, and diagnostics. That makes it a high-value research target and a poor candidate for casual live pointer editing.

The static analysis also cannot prove:

- which configured path string is finally opened for every single-player and multiplayer transition;
- the exact units/signs of widths, offsets, score, `gAlpha`, groove lateral position, and path selectors;
- whether branch IDs 0 and 1 always mean main track and pit lane on every asset;
- when every cached consumer pointer is discarded across track reloads;
- whether malformed garage entries cause a controlled failure or memory corruption;
- which visible/gameplay effects result from each field.

### Runtime verification backlog

1. **Lifecycle resolved, transient capture optional:** the resolved inline path identifies Monza; the AIW handle is local to initialization and closed immediately after parse, so no persistent handle exists in the running race. Capturing the transient handle would require observing a future natural initialization window.
2. **Strongly supported:** live header/counts and sampled records agree with the candidate resolved asset; exact opened-handle identity was not captured before the handle closed.
3. **Complete for one live race:** all three links on ten consecutive records matched the 316-byte index remap without writing memory.
4. **Asset-local mapping complete, visual causality pending:** boundary records and placement-selector ranges are mapped for Monza; visible route tracing is still required before generalizing.
5. **One endpoint complete:** Path0 generated the captured active position; a natural wet/grip transition is still required to establish full ordering and response.
6. **Complete for authored Monza content and sampled capacity edges:** all 24 grid, 12 pit, and 36 garage records match live memory; grid 103, pit 51, and garage flat 155 were sampled as allocated zero-filled records. Authored high-index boundary behavior still requires a disposable test asset.
7. **Static lifecycle complete, 30-minute steady-state/post-running control captured, transition pending:** reparse replaces arrays, restart rebuilds in place, and post frees arrays while leaving some counts stale; the 1,800-second rebuild and pointer monitors produced zero events even after lap increments stopped and cars returned toward the pits. Observe a true post/setup, track-load, or reparse transition to validate addresses live.
8. **Natural pit routing, event passage, and service sequence complete; exact event effect pending:** the three `wp_event` storage fields and the `AIRacingLine` code-1 path are mapped, a natural slot-18 cycle directly crossed main record 115 → pit record 945 → pit record 1009 → main record 1103, and later traces captured repeated selector entries, seven direct 983→984 passages, and the five-state service procedure. Correlate the racing-line/pass-score/look-ahead consumer output during another direct passage to isolate code B `18`.
9. **Malformed-input review advanced:** document and test the late record-count guard and negative-link-index hazard only against a separately approved disposable development asset.
10. Only then test a reversible file-level scalar change, beginning with a non-topological field.

## 6. Conclusion

F1 Challenge's AIW system is the game's central track-navigation and placement database. Static analysis establishes its loader, 316-byte waypoint graph, placement arrays, derived-data passes, hard index ceilings, lifecycle, event-score path, and broad consumer set; the Monza baseline directly validates the global header, selected path, sampled topology, pointer remap, complete authored placements, live vehicle ownership, one blend endpoint, event records, initialization difficulty formula, and a natural main→pit→main branch cycle. The most valuable immediate engineering outcome remains a read-only inspector and validator. A live editor should wait until post/setup pointer lifetime, cross-track branch/bitfield semantics, field units, exhaustive capacity behavior, and weather-driven rebuild behavior are runtime-proven.

## Appendix A — Corrections to prior notes/tools

| Existing label/claim | Correction | Evidence |
|---|---|---|
| `AIW` at `0x004C54F0` implies an AIW parser. | It selects the current 396-byte scene record and publishes `.SCN`/`.AIW` paths; it never reads waypoint records. | DERIVED |
| `SceneDataParser` at `0x004C5690` implies scene-file parsing. | It strips path/extension and selects an already parsed scene record by basename. | DERIVED |
| `SceneTrackFileLoader` at `0x0044C0E0` can be grouped with AIW loading. | It is the separate HAT/terrain/collision-data loader. | DERIVED |
| `dword_5EA2C0` described generically as `SetupScenePtr` or as a pointer variable. | Direct initialization, layout, consumers, and the live header show that `0x005EA2C0` is the base of the global track AI/navigation database structure itself; array fields inside it are pointers. | OBSERVED + DERIVED |
| `IndexRemappingWithStride` is a generic stride helper. | It converts three per-waypoint file indices into previous, next, and alternate/branch pointers and sanitizes invalid indices. | DERIVED |
| `sub_4FAF60` legacy name suggests opponent positioning. | In this context it rebuilds per-waypoint direction, length, width, heading, and curvature-derived fields. | DERIVED |
| `sub_4FB2E0` legacy name suggests vehicle collision-avoidance radar. | In this context it computes geometric relationships between linked waypoint segments. | DERIVED |
| Treating `wp_event` and `wp_wpse` as an undifferentiated pair at `+0x134/+0x138`. | `wp_event` has a first float at `+0x84` plus two float-to-int codes at `+0x134/+0x138`; `wp_wpse` is the two-integer alternate syntax for the code words. | DERIVED parser + consumer trace; live values OBSERVED |
| Assuming `wp_wpse` always overrides `wp_event`. | Parsing is line-by-line; both target `+0x134/+0x138`, so the last applicable key encountered wins. | DERIVED |
| Describing waypoint count/link validation as generally safe. | The count guard is late at `wp_ptrs`, and negative link indices other than `-1` are not visibly rejected. | DERIVED hazard |

These are documentation corrections only. No reviewed name, SDK header, generated catalogue, executable, or production artifact was changed.

## Appendix B — Investigation and verification record

- **Changed state:** added this paper and one discoverability row to `docs/README.md`, then appended the successful read-only runtime baseline and its additive observation recipes/artifacts. Existing papers 50 and 51 and all unrelated dirty-worktree changes were preserved. No source, generated SDK file, executable, production artifact, game installation, or game runtime state was changed.
- **Static verification:** the loader, parser, lifecycle, allocation, link-remap, nearest-query, placement, AI, physics, camera, steward, and renderer claims were checked against raw decompiled function bodies and caller/callee metadata through the supported component interface.
- **Documentation gate:** the paper exists, is classified as a research reference, is indexed through the normal `docs/README.md` entry point, labels its evidence as DERIVED, and names pending runtime validation.
- **Formatting check:** the maintained index and all AIW files changed by this investigation passed scoped whitespace checks; the paper contains the required status, runtime-pending, correction, references, and changelog sections. A full-worktree `git diff --check` remains blocked by pre-existing trailing whitespace in unrelated generated SDK files (`tools/sdk/f1c_sdk.*` and vehicle-struct headers); those files were not changed or reformatted by this research.
- **Tooling correction:** the first inspection command was issued from the workspace root, where the component tool does not exist. It failed without changing state; all subsequent static queries used the canonical component working directory.
- **Runtime tooling result:** the first normal-integrity live baseline was blocked by `OpenProcess access denied`, and the independent `PrintWindow` attempt failed. Two early elevated launch forms also failed harmlessly: one from quoting and one because `-Verb RunAs` cannot share the selected `Start-Process` redirection parameter set. The encoded-command form then succeeded with hidden elevated PowerShell. During the event-record extension, one elevated read also failed harmlessly because a numeric PID was passed where the adapter requires an executable name; it produced no evidence and was rerun with `F1 Challenge Online.exe`. A module capture (`runtime_memory/aiw_navigation_20260830_elevated_baseline/manifest.json`) and process capture (`runtime_memory/aiw_navigation_20260830_elevated_process/manifest.json`) completed with zero unreadable pages. The constructor-default memory signature did not match the loaded object because its mutable waypoint-count default was no longer present; exact-base named reads then produced the valid live header and record evidence above.
- **Runtime evidence:** `runtime_memory/observations/aiw_navigation_20260830_live_header_read_v2.json` is the primary header read; `live_path_read`, `live_header_scalars`, `live_waypoint0_blend_read`, `live_header_late_read`, and `live_difficulty_slider` supply the later path, scalar, blend, stability, and difficulty checks. `live_waypoint0_read` through `live_waypoint9_read` contain the ten-record window; eight `*_boundary_read` artifacts cover indices 942, 943, 1102, 1103, 1168, 1191, 1192, and 1203. Full placement raw reads are under `aiw_navigation_20260830_full_placements/` and `aiw_navigation_20260830_capacity_boundaries/`, with conclusions in `aiw_navigation_20260830_full_placement_validation.json`. `aiw_navigation_20260830_live_vehicle_waypoints.json` and the `aiw_navigation_20260830_vehicle_branch_transitions/` correlation are summarized by `aiw_navigation_20260830_vehicle_waypoint_summary.json`. The live event reads for records 983–984 and their parser/consumer/asset correlation are summarized by `aiw_navigation_20260830_waypoint_event_summary.json`. The cross-track aggregate scan, corrected malformed-input audit, 40-second focused branch monitor, and sequential lifecycle control are consolidated in `aiw_navigation_20260830_remaining_investigations_summary.json`; the corrected delegated pit report is under `investigations/ai_organic_pit_trace/captures/live_pit_event_20260830_01a052c1/`. The 900-second natural pit cycle, 600-second AIW/weather control, 900-second lap/session control, and long process diff are consolidated in `aiw_navigation_20260830_long_transition_summary.json`; raw correlations sit under `pit_transition_20260830_1525_*` and `weather_session_transition_20260830T*`. Five additive recipe files document offsets and encodings. `aiw_navigation_20260830_artifact_metadata.json` records process/asset hashes, target bases/indices, adapter limitations, and meaningful-slot rules. The 20-second rebuild-stability record is under `aiw_navigation_20260830_rebuild_stability/`.

- **Initialization/disposable feasibility:** `investigations/AIW_INITIALIZATION_WINDOW_FEASIBILITY_2026-08-30.md` records the read-only capability audit and a fail-closed future execution specification. It found neither an owner-approved disposable target nor a supported initialization-window observer, so malformed/high-index tests and transient handle capture were not attempted.

- **Pit-procedure continuation:** `docs/53-ai-pit-stop-procedure-runtime-map.md` and `runtime_memory/observations/pit_procedure_20260830_dynamic_followup_summary.json` consolidate the corrected high-capacity route capture, natural AI service sequence, fuel-target correlation, exceptions, and 30-minute lifecycle control.
- **Independent review:** three delegated Terra reviews independently checked branch topology, all runtime arithmetic/artifact integrity, and static ownership/lifecycle. The runtime reviewer parsed all 33 then-existing JSON artifacts and recomputed 54 links without error, but correctly narrowed placement, dry/wet, path-handle, adapter, and identity claims. The branch reviewer matched every field in eight boundary samples and corrected the asset-local selector ranges. The lifecycle reviewer distinguished reparse replacement, in-place restart rebuild, post cleanup with stale counts, exact aggregate formulas, and inline path lifetime. Their corrections are incorporated above; no reviewer edited files or live state.
- **Coordinated remaining-work review:** three additional Terra agents covered cross-track aggregates, a focused live pit/event monitor, and lifecycle/weather/handle evidence; a fourth independently audited their claims. The audit rejected automatic `wp_wpse` precedence, a false waypoint-count safety claim, unsafe treatment of negative link indices, overgeneralized selector taxonomy, and an incorrect grid OOB boundary. It also caught a live agent applying topology from a different Monza variant; that report now begins with an explicit correction and uses the verified active `2002_Monza` ranges and approved custom recipe. Only the corrected claims are integrated here.
- **Workspace validation:** the post-documentation structure check returned the same nine pre-existing errors observed at cold start: two absent game-installation directories, their dependent missing product paths, the dependent missing randomizer production path, and four source-like Python files already present under two bug-report-site production versions. It reported zero warnings. This research did not create or alter those conditions.
- **Restoration:** the work is additive. No restoration action was needed. Any owner-requested reversal must preserve the paper and the complete prior index state under the workspace's human-review/audit rules before changing the maintained index.

## References

- `F1 Challenge Online.exe`, decompiled static corpus analyzed in this workspace and fingerprinted running executable under the external game installation.
- External active-asset candidate `%USERPROFILE%\Documents\F1 Challenge V10\SEASONDATA\CIRCUITS\Italy\2002_Monza\2002_Monza.AIW`, used read-only for file-to-memory comparison; not treated as canonical SDK evidence.
- `50-camera-system-static-architecture.md` — camera ownership and render boundary.
- `51-static-analysis-system-priorities.md` — system-priority review that selected AIW/navigation for the next deep investigation.
- Preserved runtime artifacts under `runtime_memory/` and their interpretation sidecar; live claims are explicitly marked OBSERVED and bounded to the captured session.

---
*Changelog: 2026-08-30 — initial comprehensive static map; integrity-level failure preserved; successful elevated read-only Monza baseline added; global/header/path/topology/placements/vehicle ownership/blend/difficulty/lifecycle/event claims reviewed and corrected; cross-track aggregate and malformed-input audits added; a complete natural main→pit→main cycle, repeated pit entries, seven direct event-node passages, natural AI service and physical repair outcomes, a 30-minute lifecycle negative control, and post-restart same-address comparison recorded; initialization/disposable feasibility specified; remaining weather/session transitions, exact event-node consumer effect, and exhaustive capacity validation identified.*
