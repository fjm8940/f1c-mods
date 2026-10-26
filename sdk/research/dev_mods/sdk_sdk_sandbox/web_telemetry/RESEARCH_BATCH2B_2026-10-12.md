# Web Telemetry Batch 2b research (read-only), 2026-10-12

Scope: research only. No source file was edited (web_telemetry.c and telemetry_viewer.js are being edited by another agent). Live reads were against http://127.0.0.1:8080 (GET only, plus a read-only Browser pane tab). Live state: Silverstone 2002, session_generation 2, player = slot 0 "Matthias Steppe" (P24, stationary, `rt`=1, `ot2`=0), Rubens Barrichello (slot 3, P1, `l`=7, moving at s=83.5, `ot2`=1, `rt`=0). `/health` reports api 4, broker_abi 15, viewer and workbench sha256 match.

Tier key: FACT = code line or live field observed in this batch or a CONFIRMED/FACT finding. DERIVED = reasoning from FACTs. GUESS = not verified.

## A. Yellow flags and the FLAGS row

Evidence
- FACT, game: the steward object at 0x8ACEE0 holds the marshal-zone table. `+0x108` scale, `+0x10C` count, `+0x110` array of {int32 state, float end}, `+0x114` previous-state copy. Source: web_telemetry.c:262-298 (comment block, SOLVED 2026-07-25), defines at web_telemetry.c:333-336.
- FACT, game: the writer is sub_4D1730, run every frame. It clears all zones to 0, then sets a zone to 2 (yellow) for each non-DNF car whose forward speed is below a threshold. So yellow means "a slow car is in this zone now", recomputed each frame. Source: web_telemetry.c:262-298.
- FACT, game: timing sectors are not flags. They come from the track .AIW (path at 0x8AC438). Keys are cumulative distances. Live: sc1 1968.9, sc2 3985.7, lapm 5088.1. Source: web_telemetry.c (AIW block), findings `web-telemetry-sectors-vs-f1cui-20261012` (FACT).
- FACT, live: `/telemetry` `sgn`=26, `sgl`=195.70, `trkl`=5088.1. So 26 zones of 195.7 m, equal to the game's 1/scale. `sg` = 26 zeros, `yc`=0 (all green). Only states 0 and 2 have been observed.
- FACT, live: player `sgi`=5, and `tp2`=18.58 with `s`=0.0 (stationary). Rubens `sgi`=6, `s`=83.5. The white box is the focus car's zone (`fc.sgi`), not a flag.
- FACT, code: viewer draws `sg.length` cells, each `title` says "Marshal zone k of 26". The white box is `fc && fc.sgi === k` (outline), telemetry_viewer.js:3203. The sector bars (SECTORS) are at telemetry_viewer.js:3193-3200.
- FACT, code: the FLAGS header says "YELLOW xN" from `d.yc`, and the strip title says "not timing sectors" (telemetry_viewer.js:3209-3210).
- FACT, code: the server-side per-car zone and "under yellow" logic is web_telemetry.c:3664-3683 (flags_apply_to_car: segment or the one behind it, wrapping). Server zone read is web_telemetry.c:3630-3655.

Answer
- The owner's belief is half right. Game yellows are per marshal zone (here 26), not per timing sector (3), and there is no full-course yellow field in our data. The steward array is the only yellow source we read.
- The white box marks the focus car's zone. It does not move because the default focus is the player, who is stationary in the grid (`s`=0, `tp2` fixed). This is correct behaviour, but it reads as a broken flag.

Recommended display
1. Top row: 3 timing sectors from `sc1`, `sc2`, `lapm` (AIW boundaries; keep the existing bar code).
2. Second row: the 26 marshal zones, yellow where state==2, with a label "YELLOW n of 26 zones" (`yc`). Do not call it full course.
3. Focus-car marker: draw only when the focus car is moving or label it "focus car zone k". Also consider defaulting focus to the leader so the marker moves.
- Fix location: telemetry_viewer.js:3190-3215 (flagHtml), focus source telemetry_viewer.js:3223 and 3450.
- Risk: low (display only). The 26-cell count is per track, so do not hard-code it.
- Tier: FACT for the zone model and sector source. DERIVED for the white-box explanation (live fields match). GUESS: whether the game has a separate full-course track-status flag (not found in this batch).

## B. Tyre compound names

Evidence
- FACT, code: `tyreName(tc)` returns 'C' + tc (telemetry_viewer.js:2294). It is used at telemetry_viewer.js:3078, 3370, 3482, 3490. The value is `c.tc`, which is the live field `tc` (web_telemetry.c:4613-4614 emits `tc`).
- FACT, game: `veh+0x397C` tireChoice is an index into the compound list of the CURRENT TRACK's .tbc file, not a global enum. F1CUILive's `f1c_tbcTires` finds the .tbc linked to the track, parses each `[COMPOUND]` section for `Name="..."` and `WetWeather=0/1`, then selects by index. Source: findings entry topic "Tyre compound labels are per-TRACK, read from the .tbc file - a fixed S/M/H/W/I enum is wrong" (tier CONFIRMED, date 2026-07).
- FACT, findings: pit-stop trace shows `+0x397C` changes 1->4 mid-stop (tier CONFIRMED, pittingStatus entry).
- Correction to the owner's assumption: the names are per track (the .tbc linked to the track), not per car. Per-car .tbc may exist, but the prior art names the track file.
- GUESS: the exact key in the .tbc (`Name=`) and section layout are as described in the finding. Not re-read in this batch (no .tbc file opened).

Answer
- Compound index = `tc` (per car, from `veh+0x397C`). Name = Name key of the Nth `[COMPOUND]` section in the current track's .tbc, filtered by WetWeather.

Recommended fix
- Server: parse the track .tbc (path from the same track as the AIW, `GAME_ADDR_AIW_PATH` block near web_telemetry.c:340-350 area) and expose `tyre_names` once per track in `/telemetry` (a small array, e.g. `"tn":["Soft","Medium","Hard","Wet"]`). Keep the per-car `tc` integer.
- Viewer: `tyreName(tc)` reads `Console.tyreNames[tc]`, falls back to 'C'+tc (telemetry_viewer.js:2294).
- Risk: medium. Needs a bounded file read on track change, and must never copy .tbc contents into records. Track changes must update names.
- Tier: CONFIRMED (prior art) for mechanism; DERIVED for the server change; GUESS for file path until checked.

## C. Race Control content

Inventory of what we capture server-side
- `/incidents` (`ev` list, web_telemetry.c `incident_push`): START, POS, PITSTOP, PIT_IN, PIT_OUT, CONTACT, DAMAGE, FIXEDWING, FRONTWING, REARWING, HALF, LIMITS, RETIRE, BOXSOON (heuristic), CHEQUERED, and YELLOW/GREEN/FLAG transitions from flags_read (web_telemetry.c:3645-3652). Live now: START, POS only.
- `/laps`: per-car `laps` array of lap times. Fastest lap is derivable (min of valid laps). Live: Rubens best 81.748 on Burti's list is lower; the per-car fastest should be computed in the viewer or server.
- `/pits`: `pits` array; live empty (no stops this session).
- `/telemetry` per car: `fs` (finish state), `rt`, `ot2`, `pt` (pit state), `tc`, `dd`, `dt`.
- Viewer `rcMsg` (telemetry_viewer.js:2354-2378) already maps YELLOW, GREEN, FLAG, CHEQUERED, BOXSOON, RC_RETIRE, RC_MODE (VSC, RED).

Missing and where the game stores it (prior art)
- Pit stops and pit failures: pittingStatus `veh+0x3450` enum (0 on track, 1 driving, 2 entering, 3 stopped, 4 exiting) and stop counter candidate `+0x406C` (findings `pittingStatus full enum`, tier CONFIRMED). The mechanical-failure label is in web_telemetry.c:5292 ("label AI mechanical failures"). Pit failures are only meaningful when the mod is loaded.
- Mod presence: `/health` has no mod list (fields: version, api_version, broker_abi, sample_sequence, capture_cost_us, memory, workbench_ui, viewer_ui). Detection options: (a) a mod-reported field via the broker, (b) a presence flag from the mod's own health endpoint, or (c) a config check. GUESS: no current field tells us MF is loaded. Needs a check against the broker ABI.
- Penalties: `veh+0x4070` noOfPenaltiesToProcess is a live counter written on the wrong-way and steward path (findings tier FACT, 2026-07-28). Not exposed to our API yet. Penalty kind string list is in sub_4BCCC0 (FACT, findings).
- Retirements: player `FinishState` `veh+0x4064` and `ReasonCode` `veh+0x4068` (findings tier CONFIRMED). Our `ret`/`fs` fields exist in `/incidents` RETIRE events (web_telemetry.c:1129 emits `fs` and `ret`).
- Collisions: CONTACT/DAMAGE incident types exist; the commentary path for AI collisions is LIVE_UNPROVEN (findings EVF-008_D).
- Fastest laps: derivable from `/laps` (no new game read).

Recommended fix
1. Race Control page: list `/incidents` with types above; add a fastest-lap panel from `/laps`.
2. Penalties: expose `veh+0x4070` per car as `pen` in `/telemetry` after a live read check (read-only).
3. Retirements: show `fs`/`ret` transitions already in `/incidents`.
4. Pit failures: gate on a mod-presence flag; do not show until the presence source is confirmed.
- Risk: low for the display; medium for the new `pen` field (new live read; needs API version bump per contract).
- Tier: FACT for the inventory and game addresses (from findings and code). GUESS for the mod-presence source. DERIVED for the fastest-lap method.

## D. Empty live line charts

Evidence
- FACT, data path: the browser sampler is `CarSide.prototype.pushSample` (workbench_ui.js:270-330), called for every car with `p>0` from workbench_ui.js:637-641. Sample fields: `dist`=`car.tp2` (metres), `speed`=`car.s` when VF.SPEED, throttle/brake/steer under vf bits (workbench_ui.js:75, 288-300). The per-car gate that rejects samples is workbench_ui.js:281-283: `car.p>0`, `car.ot2 !== 0`, `car.pt < 2`, `car.fs === 0`, `!car.rt`, `car.l>0`, `tp2` within `[0, trackLen]`, sample age <= 1000 ms.
- FACT, live gate: player (slot 0) has `rt`=1, `ot2`=0, `fs`=2 in the current frame, so every player sample is rejected. `/trace/status` for slot 0 shows `total_samples`=0 and `open_samples`=0. This matches an empty player lap.
- FACT, live gate: Rubens (slot 3) has `ot2`=1, `rt`=0, `fs`=0, `pt`=0, `tp2`=5052.4 (< 5088.1). So he passes the gate. His samples should be stored.
- FACT, storage: open-lap points are `this.points` (workbench_ui.js:362-366). `displayPoints()` returns `this.points` when no completed lap is shown (workbench_ui.js:422-426). Mode "Latest completed lap (auto)" is the default (telemetry_viewer.js:1061). With no completed lap (`/laps` has laps, but the client sampler started mid-lap, so `startSeen`=false and `complete`=false) the auto list is empty.
- FACT, chart: line drawing is StripChart.draw (telemetry_viewer.js:591-700). Series come from `seriesFor(L)` -> `derive(points)` (telemetry_viewer.js:901-910, derive at 40-80; x = `p.dist` at line 66). A null x or null value skips the point. A `seg` point ends the path (telemetry_viewer.js:677). The x-domain is `extent()` (telemetry_viewer.js:912-924): [0, trackLen] in distance mode, so the domain is correct.
- FACT, server trace: `/trace/status` traces only two cars: A = slot 0 (player) and B = slot 9 (Marcel Lasee). Rubens (slot 3) is not in the server trace. Server selection is web_telemetry.c:1398 (`/trace/select`, web_telemetry.c:7315). `/trace/data` needs `car=A|B&lap=N` (completed laps only, `open_lap` exists in status).
- FACT, Browser pane (fresh tab, not the owner's session): side A label "A(none)", "NO DATA, Waiting for /events", and all four Lap Analysis canvases have 0 opaque pixels. So this pane never received SSE, and its state is not the owner's. Limitation recorded.
- GUESS, root cause not proven: the gate and storage work for Rubens in the live frame, so a blank Rubens chart is not explained by the gate alone. Candidate causes, ranked: (1) side A is bound to a different slot than Rubens (picker shows Rubens, but the trace and A label differ) or to the player (always rejected); (2) the sampler was reset on a generation change, leaving `points` short (`points.length < 2`), so no path is drawn; (3) lines suppressed by `seg` flags after an interrupt. The owner should run one check in their own browser console to decide between (1), (2) and (3): compare `side.slot`, `side.points.length` and the last `seg` value.
- Server open-lap seeding (backlog B1.8): `/trace/data` returns completed laps only, so mid-lap seeding needs the server open lap, which is `open_samples` in `/trace/status`. Confirming server support for an open-lap read is pending.

Recommended minimal fix
1. Bind check: when the viewer sets side A/B, set `side.slot` from the picked car id, and log it in the page notes (telemetry_viewer.js:1062-1070 for the lap-mode options, and the picker bind in workbench_ui.js:580-605).
2. Seed on open (B1.8): on page open, if a car is selected and `/trace/status` shows `open_samples`>0 for that car, fetch `/trace/select?which=A&car=<slot>` then `/trace/data?car=A&lap=<open_lap>` (needs server support) and load into `side.points` before sampling. Entry point: workbench_ui.js:573-610 (history/select) and telemetry_viewer.js:1255-1262 (exportDoc shape, reuse loadTrace at workbench_ui.js:433).
3. Player gate: when `rt`=1 or `ot2`=0 the sampler sets `seg`=true, so the player's line is empty by design. Show a note "player not on track; line paused" rather than an empty chart (workbench_ui.js:281-283).
- Risk: medium. Seeding changes stored trace state; limit to one call per selection and keep the bounded `livePointsPerCar` cap (workbench_ui.js:363).
- Tier: FACT for the gate, storage and endpoints. GUESS for the root cause (needs the owner's browser check). DERIVED for the seeding plan.

## Pending validation
- Owner browser check in a live session: `side.slot`, `side.points.length`, last `seg` for side A (question D).
- Confirm `.tbc` path and `Name=` key on a real track file, without copying its contents (question B).
- Confirm the mod-presence source for pit failures (question C).
- Confirm a full-course yellow or track-status field, if any (question A).

## Restoration
Only this file and findings.jsonl appends were written. No source, data, or game file was changed. The Browser pane tab "seed" was opened for read-only inspection; it can be closed.


## Correction B (Claude review, 2026-10-12)

FACT (file read, game install read-only): .tbc files are per TYRE BRAND, not per track. They live in `SEASONDATA\Vehicles\` (e.g. `2002_Bridgestone.tbc`, `2002_Michelin.tbc`). Each car's .hdv names its file with `TireBrand=<name>` ("references *.tbc file"). `2002_Bridgestone.tbc` [COMPOUND] order: 0 "Hard Compound", 1 "Soft Compound", 2 "Intermediate Compound", ... (each with WetWeather=). So the name lookup is per car: car .hdv TireBrand -> .tbc -> [COMPOUND] index tc. Index-to-tc mapping still DERIVED until checked live against the in-game tyre choice.
