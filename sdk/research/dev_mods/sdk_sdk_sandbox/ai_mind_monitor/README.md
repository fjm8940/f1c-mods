# AI Mind Monitor

## START HERE: AI Brain viewer v3 + SDK (2026-10-02, Claude)

Read-only (OpenProcess 0x0410). Run at the same elevation as the game.

```powershell
& "%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\mods\sdk_sandbox\ai_mind_monitor\ai_brain_gui.bat"
python "%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\mods\sdk_sandbox\ai_mind_monitor\ai_brain_gui.py" --replay "<watch or viewer folder>"
python "%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\mods\sdk_sandbox\ai_mind_monitor\brain_explain.py" --driver Kimi --json
```

- `ui_theme.py` (2026-10-08): shared look of the viewer and the Driver AI Toolkit drawer - display-scale `px()`,
  palette, named fonts, flat widgets; `F1C_UI_SCALE=1.5` forces a scale for layout checks; `--shots` now captures
  through PrintWindow. Record: `development/mods/ai_brain_lab/UI_REDESIGN_V2_DPI_2026-10-08.md`.
- `ai_brain_gui.py` (v3): a fixed GLANCE row (MODE / DOING / WHY / NEXT CORNER / TRAFFIC / LINE), fixed ALERT chips
  (grey when quiet), and 14 lens tabs, each opening with a one-line "read this first": Overview (map, speed plan with
  per-waypoint model targets, pedals), Speed (corner-target build-up factor by factor, pace-multiplier bar with the
  4 s AIAbility countdown, braking look-ahead steps model vs game), Line & steering (lateral chain and which source
  wins, lateral strip, steering law and rate limit), Traffic (5 slots, interaction target with its reason:
  blue flag / lap-ahead / teammate), Modes & safety (state machine, exit conditions, wrong-way counter), Pit &
  strategy, Driver profile (.rcd tokens decoded from stored values), Tick pipeline (11 AIMainUpdate stages and their
  outputs), All fields (all 140 registry v1.2 fields, search, tier filter, write readiness, findings count, full
  detail on select, double-click pins), Watch (pinned sparklines), Timeline (filterable events), Waypoints (20 rows),
  All cars (2 Hz grid + interest score for Follow mode), Record & SDK. Every value is tier-coloured (green OBSERVED,
  yellow DERIVED, orange GUESS). Keys: Space freeze, Left/Right car, F follow, R record, 1-9/0 tabs, ? help.
  Record writes snapshots + events + `explain.jsonl` + `latest.json` (live LLM tap) to `%TEMP%\ai_brain_live\viewer_<stamp>\`.
  `--replay` plays brain_watch situations or viewer recordings (tyre tables from the folder's cars_static.jsonl;
  values logged with older registry types are reinterpreted). `--selftest N` / `--shots DIR` are verification aids.
- `brain_explain.py`: the SDK layer the viewer uses; `explain(snapshot)` returns speed / pace / line / traffic / mode /
  profile / alerts / pipeline blocks, each with its evidence tier. Corner-target and braking-step models are imported
  from `investigations/ai_brain_map/phase3/analysis/tireforcesolver.py` and `phase2/analysis/tireforce.py`.
- `brain_core.py` additions: `Game.globals()` (session, wetness, start state, difficulty, rain scale, pit speed),
  `snapshot(slot, keep_raw=True)`, `Brain.light(slot)`, `slot_of(ptr)`, mode/opponent-mode names from prior art.
  `compact()` drops raw bytes, so brain_watch / brain_recorder output is unchanged apart from the new `globals` block.
- v2 kept as `ai_brain_gui_v2.py`, v1 as `ai_brain_gui_v1.py`. Before-state of the changed files:
  `workspace_organization_audit/AI_BRAIN_GUI_V3_BEFORE_2026-10-02/`.

Verification 2026-10-02 (Claude, live wet Imola practice, game pid 12620, read-only):
- `--selftest` live 40 s / 25 s, every tab and Follow: no errors; replay selftest on watch_20261002T194606Z: no errors.
- Screenshots of all 14 tabs checked by Claude; fixes applied (parked modes 4/5 shown as PARKED not alarm, pace bar
  scale, label overlap, header wrap, flash only discrete changes, pad-step notes, Follow ignores parked cars).
- Live: limiting corner target model / game 0x43FC = 0.9995 (Raikkonen, wet); braking steps game/model 0.92-1.08 in
  the tyre-formula region; flat 1.8 m/s pad steps labelled. Replay: wet Imola median 0.9987 (n 32), dry Monza 0.9995
  (n 50) - small samples; the full wet replay is DeepSeek batch B D11a.
- Global wetness 0x8AFEE8 read 0.800 live = wet factor 1 - 0.8 x 0.0307 = 0.9754 exactly. Session global read 1
  during an owner-confirmed practice (one sample). Practice watcher `watch_20261002T201617Z` caught mode 3 live for the
  first time (garage exit 5 -> 3 -> 0; finding ai-brain-mode3-garage-exit-live-2026-10-02).
- Owner report 2026-10-02: "GUI keeps bouncing" when DOING/WHY change. Fix: glance cards are fixed slots (grid
  uniform weights, fixed 104 px height, overflow clipped, full text on hover), tab summaries fixed at 2 lines, status
  label fixed width. Measured live over 240 ticks while switching all 19 drivers (19 distinct DOING/WHY texts): one card
  geometry, tab area y constant, summary height constant.
- brain_watch change 2026-10-02 (qualifying capture): new situation kind `yield` (interaction target 0x45FC set;
  60 total, 6 per driver), EventDetector also emits `interaction_target` and `call_to_box` changes, and `pit` no
  longer triggers on mode 5 (garage wait; cars park for minutes in practice/qualifying). Practice watch
  `watch_20261002T201617Z`; qualifying `watch_20261002T203309Z` (first minutes) + `watch_20261002T203532Z` (rest).
- Owner knowledge 2026-10-02: QUALIFYINGABILITY only drives simulated session times (FINISH / NEXT SESSION); the
  Driver profile tab says so (finding qualifyingability-simulated-sessions-only-2026-10-02).
- Pending: owner use and acceptance of the v3 presentation (human validation); a braking-zone and a yield case
  viewed live in v3.

This mod was generated from the isolated F1C SDK starter. It is non-production
until its behavior and calling assumptions have been reviewed.

Build from the repository root:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/ai_mind_monitor
```

Before loading, verify the executable:

```powershell
python tools/f1c_mod.py doctor --exe "C:\path\to\F1 Challenge Online.exe" --require-exe
```

## Read-only live dashboard

`ai_mind_monitor_gui.py` is an external Tk GUI. Run it with the same Windows
integrity level as the game. It uses only `python tools/f1c_mod.py` read-only
commands (`tool-run read-standings` and `runtime-read`), never writes to the
game, and reports confirmed position changes. Its selectable driver profile
now shows live speed (game units), waypoint identity, planner throttle/brake
commands, current lateral traffic action, and the validated RCD-derived
scalars. A lateral action is explicitly not labelled a pass until target and
race-progress evidence agree.

```powershell
python mods/sdk_sandbox/ai_mind_monitor/ai_mind_monitor_gui.py
```

For a bounded, read-only event capture that records other fields from the
*same driver* whenever a traffic-side directive changes, use the canonical
front door. Run it at the game's integrity level:

```powershell
python tools/f1c_mod.py runtime-correlate overtakeSideOffset example_tag `
  --recipes mods/sdk_sandbox/ai_mind_monitor/telemetry_recipes.json `
  --context speed --context throttleFactor --context brakeFactor
```

This produces evidence of co-occurrence, not a claim that a driver intended or
completed an overtake.

For a shared-clock commit/outcome study, make `newOvertakeTargetCandidate` the
primary trigger and add `racePosition` as an additional trigger. Both event
types then carry the same capture clock; this is required before attempting a
same-pair commit-to-standing join.

```powershell
python tools/f1c_mod.py runtime-correlate newOvertakeTargetCandidate example_shared_tag `
  --also-trigger racePosition `
  --recipes mods/sdk_sandbox/ai_mind_monitor/telemetry_recipes.json `
  --context lapNumber --context pitStatus --context teammateVehicle `
  --context overtakeSideOffset
```

The DLL refuses incompatible PE builds at runtime. Signal a clean unload with:

```powershell
python tools/f1c_mod.py unload F1C_AI_MIND_MONITOR_UNLOAD
```

Do not rename this DLL over an existing mod. Use
`python tools/f1c_mod.py stage mods/sdk_sandbox/ai_mind_monitor C:\path\to\staging`, which
refuses unowned overwrites.

## Research map

- `AI_PLR_PERFORMANCE_TABLE_NOTES.md` — AI Max Load and AI Min Radius, with
  their evidence and confidence limit.
- `AI_RCD_AND_PASSING_NOTES.md` — RaceAbility, Passing, TrackAggression, and
  their verified pace/traffic paths.
- `AI_CONTROL_OUTPUT_NOTES.md` — the waypoint, traffic, and correction path
  from AI planning to observable throttle and brake commands.
- `AI_CANDIDATE_IDENTITY_NOTES.md` and `AI_MODE_DECISION_NOTES.md` — named
  nearby-car candidates and the candidate-mode state machine.
- `AI_OVERTAKE_CORRELATION_NOTES.md` — commit gates and the evidence required
  before a maneuver can be called a real overtake.

Latest preserved live evidence includes `ai_commit_target_hz90_180_longrace_03/`,
`ai_commit_position_sharedclock_hz90_180_longrace_01/`, and
`ai_commit_position_sharedclock_hz90_300_longrace_02/`; consult
`AI_OVERTAKE_CORRELATION_NOTES.md` for their confidence boundaries.


## AI Brain GUI (2026-10-02)

`ai_brain_gui.py` (launcher `ai_brain_gui.bat`): read-only live view of one AI driver's brain, built on the
AI Brain ABI v1 registry `investigations/ai_brain_map/ai_brain_abi_v1.json` (doc 54). OpenProcess 0x0410 only,
never writes. Shows: car-up track map (walls wp+0x34/0x38, line corridor wp+80/84, waypoints ahead, planned line
0x4408, steering target point from the observed steering law, braking-limit corner with its km/h, nearby cars),
speed plan (projected braking curve 0x43A4.. vs corner target 0x43FC), command vs actuator pedals/steer, a "why"
line, and pause detection from sim time. GUESS fields are labelled GUESS.

Verified 2026-10-02 against a live race (19 cars, Alonso/Klien): reads, map and why line render; screenshot
checked by Claude. Owner live view 2026-10-02 (Jenson Button slot 12, Imola, lap 17, 260 km/h): "looks brilliant"
(presentation accepted; braking-zone behaviour still unverified). Known: per-opponent mode shows 5 for all cars (table index mapping is
GUESS); braking-limit marker not yet seen in a braking zone.


## AI Brain viewer v2 + LLM recorder (2026-10-02)

- `brain_core.py`: shared read-only reader. Reads EVERY field in the ABI registry generically (new registry fields
  show up automatically), decodes the 20-waypoint chain, 5 traffic slots, derived views (steering target + law check,
  braking state, room to walls, line source), plain-language `why`, and an event detector (braking start/end, mode,
  gear, line source, look-ahead, opponent mode).
- `ai_brain_gui.py` (v2, launcher `ai_brain_gui.bat`): status cards DOING / WHY / NEXT CORNER / LINE, car-up map,
  speed plan, controls + steering-law line, tabs Timeline / All brain fields (tier-coloured, changes flash) / Cars near /
  Record for LLM. v1 kept as `ai_brain_gui_v1.py`.
- `brain_recorder.py`: headless multi-driver logger, schema `f1c.ai_brain_log.v1`: header.json, snapshots.jsonl
  (only when sim time advanced), events.jsonl, latest.json (atomic live tap for an LLM), summary.json. Output in
  `%TEMP%\ai_brain_live\` (disposable per retention policy). About 6 KB per snapshot.

Verified live 2026-10-02 (Claude): 19 cars; recorder 8 s Klien+Button = 316 snapshots / 57 events; viewer 8 s record =
64 snapshots / 19 events; screenshot checked. Observed: braking_start/braking_end alternate every 0.1-0.3 s inside one
braking zone (Klien 122 -> 104 km/h) - cause under DeepSeek phase 2 C2. Gear reads 0 during each shift.


## Unattended race watch (2026-10-02)

`brain_watch.py` / `brain_watch.bat`: read-only, all AI cars, whole race, Ctrl+C to stop. Writes events, 1 Hz
overview, latest.json and auto-cut `situations/` (battle, pit, trouble, retire, first 3 brake zones per car; 8 s before
to 4 s after) under the temp folder `ai_brain_live/watch_<stamp>/`. Can be started before the race (rescans cars every
10 s). Tested 2026-10-02 only with the game in menus (0 cars, sim time frozen): starts and stops cleanly; situation
cutting not yet verified live. DeepSeek analyses the output (phase-2 handoff C1b).

First full race 2026-10-02 (62 laps, 19 AI cars, safety car + red flag mods active): watch_race2 (start, full 20 Hz all
cars sim 1.7-95 s) + watch_race2b (sim ~95 s to finish, --no-start, 45 s situation cap), 1.1 GB total in the temp folder.
Fixes found live and applied: latest.json replace lock (skip refresh), start congestion not a battle, 0x4484 is mostly a
traffic speed clamp (not a pit sign; pit = modes 3/4/5 or call_to_box), 45 s situation cap + --no-start, STOP file.
Counts race2b: 11264 braking zones, 192 mode changes, 9 carConditions changes, situations 49 brake / 40 battle / 40 pit
(mode 4 seen) / 20 trouble / 6 retire. Analysis: DeepSeek phase 2 C1b.

Recorder requests P1-P3 (DeepSeek C7) implemented 2026-10-02: waypoint seg_len wp+0x3C, speed_k wp+0x84, corner-target
inputs wp+112/116/120, branch fields wp+308/312; snapshot `extra` = pit team index veh+0x48A4, pit speed global
flt_8ACFB4 (live 22.22 m/s = 80 km/h), slot record index veh+0x41B4+68k (all GUESS); brain_watch `--waypoints N`
(20 = all waypoint records, ~15 KB per snapshot).

Race 3 2026-10-02 (Monaco, ~5 laps, --no-start --waypoints 20): watch_race3, 665 MB; 1260 braking zones, 61 mode
changes, situations 40 battle / 30 brake / 8 trouble / 5 retire / 3 pit; steering look-ahead index 0 seen 2190 times
(Imola rarely). Analysis: DeepSeek.

A5 tire tables (2026-10-02): brain_watch writes `cars_static.jsonl` per car every 60 s = `values` {veh offset hex:
float} for veh 0x3A7C..0x3D50 (+ `ints`), read by phase2/analysis/c9_a_retest.py. Not yet captured in a live race.

Race 4 2026-10-02 (Monza, full lap count, stopped at lap 37): start in watch_race4fullc (lights out sim 47.9, all cars
full rate 8.6-53.6 s), rest in watch_race4fulld (1.5 GB, 40 battle / 40 pit / 36 brake / 9 trouble / 1 retire, 1026
tire-table dumps). watch_race4full / 4fullb superseded (pre-race garage pit artefacts, race restart). Watcher fixes:
pit needs lap >= 1; start window = until 60 s after first car moves, exempt from 45 s cap; origin resets on clock reset.

## Viewer v3 fixes from owner live screenshots (2026-10-03)

- Driver names: map labels and the Traffic tile showed "(2001)" because they used the last word of the name. They now use `nm()`, the full name without the year.
- Tooltips: they now close when the pointer leaves the widget, even if the Leave event is lost.
- Traffic: driver column wider; the -9999 / unset `state` shows "-".
- Line & steering: the strip caption moved to the bottom, so the request/working labels no longer overprint it.
- Next corner tile: "ahead (beyond the 20-waypoint view)" replaces "? ahead".
- Waypoints: "stop" where the game's projected speed is below 0.
- All cars / Follow: new STOPPED alert (racing mode, under 2 m/s, lap >= 1, not retired), +3 interest.
- Speed tab: the pace text wraps.
- Registry v1.5 (5 type fixes, see `investigations/ai_brain_map/ABI_README.md`).
- Replay: recordings without seg_len / speed_k / k112.. / team_record now play. The two v1.0-registry recordings (`smoke`, `viewer_20261002T151813Z`, missing 79 fields) are refused with a plain message.
- Verified: py_compile; replay of `watch_race2b` across Overview / Line / Traffic / Waypoints / All cars, no callback errors; `test_abi.py` OK. Not verified live.
- Before-state: `development/mods/ai_brain_lab/trials/before_viewer_fixes_2026-10-03/`.
