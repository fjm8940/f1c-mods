# Web Telemetry: weather and track conditions research (Batch 2d)

Date: 2026-10-12. Owner: Matias. Mode: READ-ONLY research. No source edited
(web_telemetry.c and telemetry_viewer.js untouched). No game memory writes,
pokes or hooks. Game install not touched. Only this file and appended lines in
`f1c_ida_decompiled/findings.jsonl` were written.

Tiers: FACT (live proof or code plus live proof) > CONFIRMED (two independent
static sources or decompiled code plus name) > DERIVED (grounded, unproven) >
OBSERVED (live, single state) > GUESS. OPEN = not found. Citations are
file:line or function address.

## Bottom line

- Our server already publishes the weather fields the game exposes at known
  addresses: air temp, track temp, on-line and off-line wetness, and a raw
  track grip factor. Live values now (Silverstone, dry): air 20.0 C, track
  29.3 C, wetness 0.000, grip factor 0.500.
- F1CUI does NOT have a live wetness read from game memory. Its "weather" is a
  generator: it WRITES the game's weather file (per-minute conditions, wetness,
  temperatures) and reads back only air and track temperature from memory.
  F1CUI cannot be the source for live wetness, and its forecast is its own
  invention, not a game forecast.
- Two labels in our code are wrong. Per-car `wfl/wfr/wrl/wrr` are TYRE WEAR
  (1.0 fresh), not grip. The `WL` condition table (Dry/Damp/Wet/Rain/Storm) does
  not match the game's own vocabulary (dry, overcast, rain, monsoon).
- No per-track drying model, dry line, rubber state, or per-wheel on/off-track
  value was found. The racing groove appears only as particle emitters.
- No game forecast timeline was found in memory. Do not show a forecast as game
  data yet.

## 1. Weather model

### 1a. States

- FACT (names): the game's own radio vocabulary has the weather states and
  transitions `ForecastNoRain060`, `ForecastRainEnd061`, `ForecastRainKeepOn062`,
  `ForecastRainStart059`, `OvercastToRain051`, `RainToOvercast052`,
  `RainToMonsoon053`, `MonsoonToRain054`. Source:
  `mods/f1cui_web_remote/f1p_dataContracts/f1c_RadioCommData.class` (constant
  pool, `isAboutWeather`). So the states are dry, overcast, rain, monsoon.
- CONFIRMED: `f1c_magicData` fields `DrsDisabledWhenRainy`,
  `WeatherVolatilityInSession`, `WeatherVolatilityBetweenSessions`,
  `WeatherTypicalWeather`, `WeatherWetFactor`, `WeatherTypicalAirTemp`,
  `WeatherTypicalTrackTemp` exist (F1CUI). Their numeric defaults are not in the
  bytecode (set at runtime) -> OPEN.
- OUR table: `web_telemetry.c:5314` `WL={0:Dry,1:Damp,2:Wet,3:Rain,4:Storm}`.
  The comment at `:5315` says the condition is now derived from on-line
  wetness. The labels `Damp` and `Storm` are not in the game's vocabulary. The
  mapping from wetness value to label is not verified -> DERIVED mismatch, fix
  the labels or the thresholds before showing them as game states.
- Native rain intensity: the NT2004 finding (existing, DERIVED) says
  `sub_52B1D0` fires a caution when rain intensity crosses 0.18 upward
  (`functions/sub_52B1D0_0052b1d0.c` line 189). The variable holding rain
  intensity is NOT named -> OPEN. docs/12 (Ghidra era, lower trust) guesses
  `flt_7CD160` = rain intensity and `dword_7CD164` = grip/drying factor (GUESS,
  unverified).

### 1b. Where current weather lives (addresses)

| Value | Address | Units | Tier | Our code | F1CUI |
|---|---|---|---|---|---|
| Ambient (air) temp | 0x008AFEF0 | C (float) | CONFIRMED (ask.py global `AmbientTempC`), live 16.4/29.6 per comment | `ambient_temp`, `at` (`web_telemetry.c:349-370, 4234`) | reads `base+4914928` = 0x8AFEF0 (javap `f1c_memoryReaderEngine` ~line 178759), base 0x400000 implied (DERIVED) |
| Track temp | 0x008AFEF4 | C (float) | CONFIRMED (same comment, PitRace `flt>30` test) | `track_temp`, `trkt` | reads `base+4914932` = 0x8AFEF4 |
| On-line wetness | 0x008AFEE8 | 0..1 | CONFIRMED (ask.py `OnLineWetness`) | `on_wetness`, `onw`, `tw` (`:4532-4536`) | NOT read |
| Off-line wetness | 0x008AFEEC | 0..1 | CONFIRMED (ask.py `OffLineWetness`) | `off_wetness`, `offw` | NOT read |
| Track grip factor (raw) | 0x007CF3A4 | float | DERIVED (ask.py `TrackWetnessFactor`); comment says constant 0.50 when dry | `tcf`, `tgf`, `track_grip_factor` (`:496`, `:902`) | NOT read |
| Track condition float | 0x007CF3C8 | float (0..1) | CONFIRMED type (`web_telemetry.c:486-489`) | not sent | not read |

Conflict to resolve: docs/12 (`docs/12-globals-reference.md:192, 718`) names
0x8AFEE8 `TrackGripBase` and 0x8AFEEC `TrackGripComparison`. Our code and
ask.py name them wetness. The physics consumers (`web_telemetry.c:349-370`:
`1.0-(a+b)*0.5` in VehicleMainPhysicsUpdate and friends) support wetness.
Conflict OPEN; keep the wetness name, and cite the consumers.

Weather-file keys (F1CUI writer, see section 2): `wfConditions`,
`wfOnPathWetness`, `wfOffPathWetness`, `wfAmbientTemp`, `wfTrackTemp`,
`wfMinute`, plus session blocks `wfTestDay`, `wfPractice1-4`, `wfQualify`,
`wfWarmup`, `wfRace`. The literal key strings come from a runtime macro file
(`f1c_resMacro`), not from bytecode, so the exact on-disk spelling is OPEN.

### 1c. How it changes over the session

- In-session change of the wetness fields is driven by the game (the
  rain-intensity threshold in `sub_52B1D0` above). The timeline or forecast
  source is OPEN. The per-track `.gdb` files (below) hold typical values, not a
  timeline.
- DERIVED per-track baseline: circuit `.gdb` files in SeasonData carry
  `OnPathWetness`, `OffPathWetness`, `AmbientTemp`, `TrackTemp` keys
  (`data_index.json`: OffPathWetness samples 60/80; AmbientTemp min 69.8 max
  106.5). Units of AmbientTemp in the `.gdb` are UNCLEAR (the 69.8-106.5 range
  is not obviously Celsius) -> OPEN. Do not use until units are proven.
- Config tokens (`config_schema.json`, lookahead-associated, not
  offset-proven): `RACECOND_weather`, `RACECOND_timescaled_weather`,
  `TrackTemp`, `AmbientTemp`, `RainAbility`, `WetWeather` (tyre .tbc),
  `GrooveWidthWet` ([PITS], inline), `AIDryGrip`/`AIWetGrip` ([LOOSE]),
  `CLOUDRNY`/`CLOUDSNY`. These are file settings, not live state.

### 1d. Comparison with our server (sent today)

- Sent and correct: `at`, `trkt`, `onw`, `offw`, `tw` (= on-line wetness,
  `web_telemetry.c:4532`), `tcf`/`tgf` (raw, labelled as grip factor).
- Not sent: rain intensity (native variable OPEN), weather state enum (no
  verified enum in memory), forecast (OPEN), per-track typical values (OPEN
  units).
- Mislabelled: the `WL` table (see 1a) and per-car `wfl..wrr` (see section 3).

## 2. F1CUI reference: what it reads and how it labels

Source: `mods/f1cui_web_remote/f1p_*/` decompiled classes (javap -c -p
-constants). Full dump of all `f1p_*` classes: 224,230 lines (kept in %TEMP%,
not in the workspace). The 7z archive (`f1cui_web_remote.7z`, 210,565 bytes)
was listed and extracted into a new %TEMP% directory
`%USERPROFILE%\AppData\Local\Temp\f1cui_w2d_iQFf` (3 files: Start_Telemetry.bat,
inject_simple.exe, web_telemetry.dll). The archive holds no weather source. All
F1CUI evidence below comes from the loose `.class` files.

- FACT (code): F1CUI writes a custom WEATHER file.
  `f1p_businessLogic/f1c_weatherRebuildWatcher` `writeFileRebuilt`,
  `writeBellToFile`, `buildSingleSessionBell`, `copyFileToOriginal` ("WEATHER:
  Replaced original weather file with the custom one"). It builds a per-session
  bell curve (TreeMap of minute to conditions) and writes it, replacing the
  game's weather file. The writer also keys on `calcAirTemp` and `theBell`.
- CONFIRMED (memory reads): F1CUI reads ONLY two weather values from game
  memory: `base + 4914928` (air temp, 0x8AFEF0) and `base + 4914932` (track
  temp, 0x8AFEF4), in `f1p_input/f1c_memoryReaderEngine` at the
  `bpFreqRaceWeatherInfo + bpFreqQLapWeatherInfo` cadence (javap lines
  ~178759-178767, `setActualAirTemp`, `setActualTrackTemp`). They also store
  start-of-session temperatures before the session starts. Units: Celsius by
  comparison with our `at`/`trkt` (same addresses, same values at Silverstone).
- CONFIRMED (no wetness read): the full `f1p_*` dump has no reference to
  4914920 (0x8AFEE8 offset), 4914924 (0x8AFEEC), or 3998628 (0x7CF3A4 offset).
  F1CUI does not read wetness, grip factor, or track state from game memory.
  DISPROVEN premise: "F1CUI reads live wetness from memory."
- F1CUI "actual weather" (`f1p_dataContracts/f1c_sessionInfoData`
  `updateActualWeatherConditions`, `getWeatherConditionsAtTime`) is the
  interpolation of its OWN bell (`weatherTimeMap`, floorEntry/ceilingEntry), not
  a game value. Its "forecast" messages come from that bell too.
- Labels F1CUI uses: `actualWeatherConditions`, `numberOfRainyMoments`,
  `dryThreshold`, `rainThreshold`, `rainyDeclarationRaceMinLimit`,
  `hasRaceBeenDeclaredRainy`, `startWeatherConditions`, `actualAirTemp`,
  `actualTrackTemp`, `startAirTemp`, `startTrackTemp`, `dryDefault`,
  `rainDefault`. These are F1CUI's own state labels, not game labels.
- Tyre side (CONFIRMED names): `f1c_telemetryData` `noOfDryTires`,
  `isAWetCompound`, `raceStartingAsRainy`; `f1c_tbcTires` `wetTire`.
  `WetWeather` in `.tbc` is a pair of floats (e.g. 1.0040, 1.1031) in 360
  occurrences (DERIVED, per-compound wet multiplier). This is the "wet tyre
  need" source for per-car effects, but the live per-car tyre compound was not
  checked.

Verdict on F1CUI: it is a weather GENERATOR with a 2-value memory reader. It
does not have live wetness, rain, dry line, or grip. Do not model our Weather
page on its wetness handling. Its air and track temperature reads match ours.

## 3. Track conditions and per-car effects

### 3a. Per-track wetness, drying, dry line, rubber, grip

- Dry line / racing groove: no state variable found. Emitter IDs only (FACT,
  existing finding, `groove` query): `9 GROOVE`, `10 WETGROOVE`, `8 WETSKID`, from
  `MaterialShaderReactionEffectParser_004ca480`. These are visuals.
  `GrooveWidthWetField` `0x005BE558` (DERIVED) and config `GrooveWidthWet`
  (`[PITS]`, inline) exist as settings, not live state.
- Rubber build-up: no field found (`rubber` query: only AI pace multiplier).
- Per-track drying model: none found. OPEN.
- Grip multiplier: `WetGripMultiplier` `0x007D020C` (docs12, DERIVED, 0 writers,
  1 reader) and `DryGripMultiplier` `0x005AD1B4`, `TireDryGripMult2`
  `0x005AD1B0` (DERIVED). `ContactGripMultiplier` `0x005AD230` (DERIVED). These are
  constants, not live track values.
- `EnvironmentFlags` `0x007CB710` (docs12 "bit0 = wet track", DERIVED,
  competing name `Env flag`, no writers found) -> OPEN; candidate for a live
  wet-track bit, not verified.
- Surface globals (DERIVED, ask.py): `SurfaceKey` 0x005CC678,
  `SurfaceTextureData` 0x005CC9C8, `TrackSurface_Intermediate` 0x005BE500,
  `ContactSurfaceFriction` 0x0059EA50, `TireTerrainDoubleThreshold` 0x0059EB20.
  None is a per-wheel or per-segment surface value.
- Grass / gravel / kerb / on-track per-segment: no named field. Gravel-trap
  geometry is known only from an AI pull-off finding (FACT,
  `development/mods/mechanical_failures/docs/AI_PULLOFF_RUN_2026-10-02.md` s.13,
  AIW wall widths are not physical barriers). `terrain.ini`
  (`SeasonData/Circuits/terrain.ini`, parsed by FUN_00460a10) is a candidate
  per-track surface table -> OPEN, not read.

### 3b. Per-car effects

- Wet factor per car, AI (OBSERVED, existing): `veh+0x4580` = `max(0.5, 1 -
  wetness * veh+0x44C8)` in `AIPushLap` (0x42A180), with `veh+0x44C8` =
  RainAbility. Wet Imola lap 2: 0.9754 matched, with implied wetness 0.80 for
  all cars at `veh+0x44C8` 0.0307 (existing finding, `PRIOR_ART_MAP_2026-10-02.md`).
  This is an AI corner-speed scale, not the physics grip the player feels.
- Player grip the car sees: NO live field found. The tyre-force solver
  (`TireForceSolver`) is internal. OPEN.
- Per-wheel surface under the car: NO field found. OPEN.
- On/off track (player): `veh+0x3450` `pittingStatus` (CONFIRMED end-to-end
  for the player, existing finding 2026-07-28): 0 on track, 1 driving,
  2 entering pit lane, 3 stopped in box, 4 exiting box. It is the on-track vs
  pit signal; `+0x32C8` active is NOT on-track (existing finding).
  CONTRADICTION (existing, `name_index` note): for AI cars the field read 0 in
  all 6653 samples across 20 AI cars that pitted -> applies to the player only,
  or unproven for AI -> OPEN for AI.
- Tyre wear (player): `wfl/wfr/wrl/wrr` in our JSON (`web_telemetry.c:4633`),
  1.0 fresh, from `(double at +0x48 per wheel - 0.75)*4.0` (existing wear bar,
  DERIVED), struct base 0x007C7A00 stride 1124 (CONFIRMED). Our label "tire wear"
  is right in the code comment; the risk is the viewer calling it grip.
- Tyre temps (player, CONFIRMED static, not yet live-verified per existing
  finding): `btmp`, `ttmp` (Kelvin in game, we publish Celsius).

## 4. Recommendations

### 4a. Channels to add (tier, unit, source, rate)

| Channel | Source | Tier | Unit | Rate | Action |
|---|---|---|---|---|---|
| `wx.air_c` | 0x8AFEF0 | CONFIRMED | C | per tick | already `at`; label "air" |
| `wx.track_c` | 0x8AFEF4 | CONFIRMED | C | per tick | already `trkt`; label "track" |
| `wx.wet_on`, `wx.wet_off` | 0x8AFEE8, 0x8AFEEC | CONFIRMED | 0..1 | per tick | already `onw`, `offw`; show as % |
| `wx.wet` | = on-line wetness | CONFIRMED | 0..1 | per tick | already `tw` |
| `wx.grip_raw` | 0x7CF3A4 | DERIVED | float | per tick | already `tgf`; label "grip factor (raw), not wetness" |
| `wx.cond` | derived from wetness | DERIVED | enum | per tick | replace `WL` labels with dry/overcast/rain/monsoon only after thresholds verified (OPEN) |
| `wx.rain_intensity` | unnamed native | OPEN | 0..1 | - | do not add until the variable is found (sub_52B1D0 rising-edge 0.18 gives a clue) |
| `wx.forecast` | none found | OPEN | - | - | do not add; show "not available" |
| `car.wet_factor` | veh+0x4580 | OBSERVED (AI) | 0.5..1 | per tick | optional, AI only |
| `car.rain_ability` | veh+0x44C8 | CONFIRMED-ish (rcd parser, existing) | pct | static | optional |
| `car.pit_state` | veh+0x3450 | CONFIRMED (player) | enum 0..4 | per tick | on/off track signal for the player only |
| `car.tyre_wear` | wfl..wrr | CONFIRMED | 0..1 | per tick | rename label; not grip |
| `track.typical_wet/temp` | circuit .gdb | DERIVED (units unclear) | - | static | show as reference only |

### 4b. Weather page layout

- Current: air, track, wetness (on, off) as %, grip factor labelled raw, and a
  state label only after the threshold check.
- Forecast: "not available from game memory" until a timeline is found. Do not
  copy F1CUI's bell; it is F1CUI's invention and it rewrites a game file.
- Track map overlay: NO per-segment wet, dry line, or rubber data exists. Show
  only per-car on-track or pit state, and only the player's (validated) state.
  A wet-zone overlay is not possible from current data.
- Per-car: wet tyre need (from `WetWeather` compound value and compound) needs a
  live check before display. Per-car grip and per-wheel surface: not available.

## 5. Evidence and verification performed

- Live `/telemetry` (read-only), two samples 3 s apart, sample_sequence
  27055 -> 27114, sim time `st` 1443.8 -> 1446.9, `tk` advanced. Values: `ty` 7,
  `ph` 5, `at` 20.0, `trkt` 29.3, `tcf` 0.527, `tgf` 0.500, `tw`/`onw`/`offw`
  0.000, `trk` 2002_Silverstone. Dry session, live and advancing. This is not
  wet-track evidence.
- F1CUI javap batch dump of `f1p_*` classes (no game memory touched).
- ask.py global/offset/func/findings, evidence-query findings, config_schema scan,
  data_index scan, docs12 grep.
- Not done: f1c-memory MCP (deferred tool, not loaded; the live endpoint covered
  the same values). No live wet-track sample exists in the endpoint. Not checked:
  the game's weather-file path and contents on disk, which would give the
  forecast source. Not checked: per-car live grip.

## 6. Risks

- Mislabelled values: `wfl` as grip, `WL` labels, `tgf` as wetness. Each would
  mislead the owner's analysis. Fix labels first.
- docs12 names conflict with our wetness names at 0x8AFEE8/EC. Keep our names,
  record the conflict.
- F1CUI's file-writing approach replaces the game weather file. Do not copy it;
  it changes game behaviour and is outside a read-only telemetry console.
- Reading new unverified addresses (rain intensity, EnvironmentFlags) risks wrong
  values. Verify live (read-only) before publishing.
- AI `pittingStatus` contradiction: do not gate an on-track feature on it for AI.

## 7. Open questions

1. Where is the rain intensity variable and the weather state enum? (sub_52B1D0
   callers and reads.)
2. Where is the game's forecast or weather timeline (file path, read at session
   start or live)? F1CUI's `getWeatherFilePath` / `F1CStandardWeatherFile` is the
   lead.
3. Units of `AmbientTemp` and `TrackTemp` in the circuit `.gdb` files.
4. Is `0x007CB710` (EnvironmentFlags) a live wet-track bit?
5. Does `pittingStatus` (`+0x3450`) apply to AI cars? (Contradicted once.)
6. Per-car live grip and the tyre compound in use (for `WetWeather` need).
7. Per-track surface table (`terrain.ini`, FUN_00460a10): grass, gravel, kerb.
8. Conflict: docs12 `TrackGripBase`/`TrackGripComparison` vs our wetness names.
9. Thresholds behind `WL` and F1CUI's `dryThreshold`/`rainThreshold` values.

## 8. Changed files

- Created: this file.
- Appended: `f1c_ida_decompiled/findings.jsonl` (one line per result below).
- Temp only (not workspace): `%TEMP%\f1cui_w2d_iQFf` (archive extract, 3 files),
  `/tmp` dumps. Nothing deleted.


## Claude review addition (2026-10-12): forecast source found

FACT (read-only file read): `<game>\LOG\weather.txt` exists (11,940 bytes, modified 2026-10-11, i.e. before today's session). It is a per-session weather timeline: blocks TestDay, Practice1-4, Qualify, Warmup, Race; each has start Conditions / OnPathWetness / OffPathWetness / AmbientTemp / TrackTemp, then Minute=<t> {Conditions, AmbientTemp} steps (171 steps total). This matches the WEATHER file format F1CUI writes (f1c_weatherRebuildWatcher).

DERIVED: this file is the forecast source the console can show (read-only, parse once per session; mtime check). OPEN: whether the game reads this exact path or F1CUI only logs here; meaning/scale of Conditions (0-100?) and OnPathWetness=4.00 vs memory 0..1; whether it is rewritten per session (mtime older than today's race = maybe stale). Verify mtime/contents change at next session start before trusting it.


Update (Claude, 2026-10-12, after owner opened a new session): LOG/weather.txt NOT rewritten (still 11,940 bytes, mtime 2026-10-11; no other weather file in the game dir newer than today). Live: air 20.0, track 20.0, wetness 0/0. So the file is not refreshed per session by the game in this setup; DERIVED: it is written only when F1CUI's weather rebuild runs (or by an earlier F1CUI session). Do not use it as the forecast source unless its mtime matches the current session.


## Owner questions addendum (Claude, 2026-10-12 evening)

Owner asked: (1) where the occasional wet areas and wet lines with less grip come
from, (2) add a track map to the Weather tab showing wet areas and the on/off
track dry/wet distinction, (3) how F1CUI forecasts rain and condition changes,
(4) whether track temperature is the same everywhere. Answers below; findings
lines appended (`web-telemetry-*` and `f1cui-weather-system-*`, all 2026-10-12).

### A. Wet line and wet areas (game files)

- FACT: every AIW waypoint carries `wp_path=(dry, wet)` documented as "dry line
  lateral offset, wet line lateral offset" plus `wp_width=(road left, road
  right, far left, far right)`, `wp_groove_lat`, `wp_galpha` (groove darkness).
  Track header: `GrooveWidth=8.200`, `GrooveWidthWet=4.3125` for Silverstone.
  So the wet line is a per-waypoint lateral offset of the driving line (AI
  moves to it in the wet), and the wet groove is about half the dry groove
  width. `sub_402690` parses both groove widths into the AIW structure.
- CONFIRMED: spatial wetness at a car is a lateral profile - `sub_4EE2D0`
  computes the lateral distance from the path plane and calls
  `F_1DTableLookupWithLerp(distance)` to blend on-line/off-line wetness;
  other consumers use the plain average `(on+off)*0.5` (`0x401C90`,
  `0x4F0430`) and per-wheel wear interpolates by on-line wetness
  (`0x4ED960`). There is no per-segment wetness store anywhere: "wet areas"
  are this groove-distance profile plus per-material grip.
- FACT: per-surface dry/wet grip is `SEASONDATA\Circuits\terrain.ini`
  `[FEEDBACK]` groups: road 1.00/0.80, rumble (kerbs) 0.95/0.60, grass
  0.50/0.45, gravel 0.52/0.50; walls/fences listed separately. `[TRACKVARS]`
  can be overridden per track from its GDB; 2002_Silverstone has no override.
  The file comments: dry/wet coefficients are part of the grip equation, tyre
  dry/wet components are the other part. Relative wet loss: kerb -37%,
  road -20%, grass -10%, gravel -4%.
- FACT: the track GDB `Weather {}` block is a per-session script with
  Conditions + TrackWetness (Dry/Damp/Wet/Soaked) + `Minute=` steps (Silverstone
  Practice2: Drizzle at min 48, Rain at 56; Qualify: Damp then Dry at 56).

### B. F1CUI weather mechanics (javap bytecode facts)

- Condition scale (internal floats): DRY=0, BEST=10, CLEAR=20, NICE=30,
  FINE=40, unrecognized=50, OVERCAST=65, DRIZZLE=75, RAIN=82, MONSOON=100.
  Thresholds: overcast=61.5, rain=72, monsoon=88.
- It builds `weatherTimeMap` (seconds -> conditions) from TWO sources:
  `updateWeatherTimeFromGDBTrack()` parses the track .gdb `Weather {}` block;
  `updateWeatherTimeMapDynamic()` re-reads the game weather file
  (`getWeatherFilePath`). `getWeatherConditionsAtTime` linear-interpolates.
- Forecast: `isItGoingToRainAfterXMinutes(X)` looks up conditions at
  elapsed+X*60 and fires the radio `ForecastRainStart059` only when value >
  61.5 AND `value - rand + random(0, 2*rand) > 73`, with
  `rand = X * RandomWeatherFactor`; rain end mirrors it (< 71). So "rain is
  about to come" is a lookahead on its own timeline, gated by a random factor -
  it never measures rain to predict it.
- Generation (f1c_weatherRebuildWatcher): `buildSingleSessionBell` randomizes
  the session from `WeatherTypicalWeather` / `WeatherTypicalAirTemp` with a
  time-of-day temp curve, `WeatherWetFactor` bias
  (`|W-30|*(W-30)/120`, 30=neutral), between-session volatility
  (`WeatherVolatilityBetweenSessions/15`, `/2` as `nextGaussian` walk over 15
  groups), `WeatherVolatilityInSession` for intra-session steps; `putInBell`
  clamps 0..100 and derives track temp from the typical air/track delta; air
  temp clamped to typical +/-15. `writeBellToFile`/`writeFileRebuilt` then
  REPLACE the game weather file with the bell (`copyFileToOriginal`).
- Conclusion: F1CUI "knows" the future because it either reads the authored
  timeline (GDB/weather file) or generated it itself; its radio/forecast
  displays that timeline, not live measurements.

### C. Track temperature uniformity

- CONFIRMED: one global float `flt_8AFEF4` (C). The tyre force integration reads
  it once per vehicle call together with air temp; the only other consumers are
  the weather serializer and the damage path. No per-sector/per-waypoint/per
  material temperature exists in AIW, terrain.ini or GDB (GDB TrackTemp is one
  value per session; the weather timeline has one TrackTemp per minute).
  Track temperature is uniform across the track in this engine; spatial grip
  variation comes only from materials and the wetness profile.

### D. What the Weather tab can honestly show (implementation basis)

- The track map (outline from the existing `/track` data) with:
  - the racing line as a band using `GrooveWidth` (dry) / `GrooveWidthWet`
    (wet) and `wp_path` offsets per waypoint, tinted on-line wetness vs
    off-line wetness (colours keyed to `onw`/`offw`);
  - the wet-line offset polyline from `wp_path[1]`;
  - the on/off track dry-wet distinction as data: road vs kerb vs grass vs
    gravel dry/wet grip (terrain.ini) plus the live on-line/off-line wetness %;
- Tiers: AIW/terrain.ini/GDB values FACT (file), the drawn grip loss and band
  geometry DERIVED (model; watched offline against the live values).
- Not available: per-segment wetness, puddle map, spatial track temperature.
  Do not invent them.

### E. Third-party compendium cross-check (owner-supplied, 2026-10-12)

Source: J21C's Corner, The F1 Challenge 99-02 Data Compendium Revision 2 (2023-02-02). Third-party modding research; claims to verify in-game, not first-party evidence. Finding: f1c-gdb-weather-compendium-crosscheck-20261012.

- GDB weather model it documents: Conditions 0-100 (clouds up to 50, rain above; dampness target = (Conditions-50)*2); words Best/Nice/Clear=0, Overcast=50, Drizzle=55, Rain=75, Monsoon=95; TrackWetness 0-100 sets the whole track, On/OffPathWetness override per line; words Dry=0, Damp=25, Wet=50, Soaked=90, Monsoon=100; scheduled steps accept Minute= and Lap=; temperature words Cold 8 / Cool 14 / Medium 20 / Warm 26 / Hot 33 C.
- Local checks (FACT): our 2002_Silverstone.gdb uses exactly those words; Lap= steps exist in many installed GDBs (2001_Interlagos Lap=34/44/56 with Conditions+AmbientTemp); TrackWetness whole-track + on/off-line override is consistent with the CONFIRMED 0x8AFEE8/0x8AFEEC pair, and the single track temperature 0x8AFEF4 matches one whole-track value.
- Divergence worth keeping: F1CUI maps the same words to its OWN numbers (DRY 0, BEST 10, CLEAR 20, NICE 30, FINE 40, OVERCAST 65, DRIZZLE 75, RAIN 82, MONSOON 100; thresholds 61.5/72/88), which do not match the compendium's game values. Do not present F1CUI condition numbers as game states.
- Still to verify: the (Conditions-50)*2 dampness target and the minute/lap schedule behaviour live; the compendium's revision 1 (RPM lights, tyre wetness curves) is referenced but was not fetched.

## F. rc.14 corrections and the "wet spots" answer (Claude, 2026-10-12 evening)

Owner input: (1) the Weather GUI was broken, (2) Web Telemetry must not rely on
F1CUI (F1CUI was only a reference for how data is fetched), (3) the track has
wetter spots with heavier spray; DeepSeek claimed there are no "more or less
wet" spots. Findings appended: `wetness-lateral-profile-per-wheel-table-20261012`,
`game-writes-log-weather-txt-timeline-20261012`, `ai-dry-drives-wp-path0-live-20261012`.

### F1. Wet spots exist per wheel (CONFIRMED static, not live-poked)

- The weather object 0x8AFEC0 carries a 16-float lateral wetness profile at
  0x8AFF24..0x8AFF60. RangeInterpolationTable 0x4FD6F0 fills it as
  `on + f x (off - on)` with f = 0.05, 0.03, 0.01, 0.00, 0.01, 0.06, 0.14,
  0.25, 0.37, 0.50, 0.63, 0.75, 0.86, 0.94, 0.99, 1.00 (0.25 m steps).
  F_1DTableLookupWithLerp 0x4FD7E0 reads it at `distance x 4`; 3.75 m or more
  returns full off-line wetness. Disassembly at 0x4EE316: `mov ecx, 0x8afec0`.
- Distance = lateral offset of the point from the nearest waypoint's
  wp_path[1] line: `|perp . pos - (perp . wp_pos + wp[+0x1C])|`
  (PositionChangeMonitor 0x4E9630 -> nearest-waypoint walk sub_403640).
  Memory +0x1C = the AIW's second wp_path value (file comment "wet line";
  /track `wl`; checked live: wl offset 3.52 m = file -3.5220).
- Consumers: per WHEEL in sub_4423B0 (wheel+0x270, vehicle+0x4FC for wheel 0,
  stride 1124) -> tyre friction 0x436870 (surface grip `dry - wet x loss` and
  the tyre grip terms both drop with it) and tyre cooling 0x438130
  (`1 + flt_8AFEE4 + wheel wetness`); per CAR in sub_4EE2D0 -> spray and water
  particle intensity. When on == off the profile is skipped.
- Shape: driest 0.75 m either side of the line (two tyre tracks), the centre 5%
  of the gap wetter, half way at 2.25 m, full off-line from 3.75 m.
- Answer to the owner: there is no per-corner puddle store (DeepSeek is right
  about that), but the claim "no more or less wet spots" is WRONG in effect.
  Grip, tyre cooling and spray vary across the track with each wheel's
  distance from that line, so wherever a driver's path departs from wp_path[1]
  (corner entries, kerbs, overtaking lines, places where the AIW line differs
  from the driven line) the car is on wetter tarmac with heavier spray. Plus
  per-surface wet grip (terrain.ini: kerb -37%, road -20%).
- To promote to FACT: in a wet session with on != off, read 0x8AFF24..0x8AFF60
  and the player's wheel+0x270 while driving on and off the line.

### F2. LOG\weather.txt is game-written (CONFIRMED: static + live)

- Game_Init 0x42D6B0 -> sub_4FCAA0 -> sub_4FD8B0(0, 432000) builds the weekend
  timeline; WeatherConditionsFileWriter 0x4FE820 writes `%sweather.txt` with the
  8 SessionTypeNameLookup blocks. sub_4FCEE0 evaluates the timeline once per
  second at session clock x time scale (this[0], default 1.0).
- Live: game start 16:36:36, file written 16:36:50; Practice1 start values
  16.88 / 23.24 C = live 16.9 / 23.3 C (the GDB says 19 / 22).
- CORRECTIONS to earlier sections: section 2 and the Claude review addition
  said F1CUI writes this file and that the game keeps no forecast in memory.
  Both are wrong. The "not rewritten per session" observation is correct and
  expected: the game writes it once per weekend load. The rc.8-rc.13 server
  gate "file newer than session start" therefore never passed; rc.14 accepts
  the file when it was written during this game run (mtime >= process start).
- Web Telemetry does not read, use or need F1CUI for weather; F1CUI content
  is removed from the Weather page.

### F3. Live observation

- Dry race, Silverstone, 80 samples of 16 running AI cars: median 0.30 m from
  wp_path[0], 1.59 m from wp_path[1] (OBSERVED; matches docs/52 dry -> Path0).
  Wet-session line choice remains OPEN.

### F4. Game condition classes, CURRENT and PROJECTED (rc.15, CONFIRMED static + live)

- ThresholdBasedReturn 0x4FEAA0 on Conditions/100: < 0.6165 Dry, < 0.70
  Overcast, < 0.88 Raining, else Monsoon. CURRENT CONDITIONS (sub_4FF130) =
  class of the timeline at the session clock; PROJECTED (sub_4FF1D0) = class of
  (min + max)/2 over the next 50 min (90 when dword_7C56A4 == 7) plus a float
  offset at 0x8AFEC0+0x104 (not read, OPEN).
- Live 2026-10-12 Practice1: timeline 59.96 -> screen "Dry" (matches);
  window 59.96-72.04 -> 66 Overcast without the offset, screen "Raining".
- GDB words in game code: Conditions Best 3 / Nice 30 / Clear 60 / Overcast 65 /
  Drizzle 73 / Rain 85 / Monsoon 97; wetness Dry 0 / Damp 25 / Wet 50 /
  Soaked 75 / Monsoon 100; temps Cold 7.5 / Cool 13.75 / Medium 20 / Warm 26.25 /
  Hot 32.5 (0x4FE0D0, 0x4FE1E0, 0x4FE2B0). CORRECTION to section E: the
  compendium values and its "rain above 50 / dampness target (C-50)x2" rule do
  not match the code; F1CUI's 61.5/72/88 thresholds were an approximation of
  the game's 61.65/70/88.
- The timeline (and LOG\weather.txt) exists only when the race option
  Changing Weather = "Selected Season" (dword_7CF080 == 0).

