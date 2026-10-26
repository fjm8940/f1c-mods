# Telemetry 1.2.0: Replay page for past recordings (SPEC r1, 2026-10-14)

Author: Matias. Spec by Claude (coordinator/reviewer); implementation by two Haiku workers (H-S server, H-V viewer).
Stage: specify -> implement. Product version stays 1.2.0 (unreleased); API stays 10 (unreleased, new route only).
Owner request (2026-10-14): "make it so that I can watch recordings afterwards".
Builds on: RECORDING_REVIEW_SPEC_2026-10-13.md (R1/R2 done = lap charts only; this is its R3 "timeline"),
R1B_RECORD_2026-10-14.md (review adapter), RECREV_REAL_RECORDINGS_2026-10-14.md (real-file check, H-1).

## 0. Summary
- A new console page **Replay** (group Data) plays a closed recording like a race broadcast: track map with every
  car moving, timing tower, play/pause, speed 1x-16x, scrub bar with lap and event marks, follow-a-car panel.
- Server: one new read route `GET /replay?session=<v2 id>&t0=<s>&t1=<s>` (window of car states) plus
  `GET /replay/info?session=<v2 id>` (time span, cars, events, track outline). Viewer fetches 30 s windows ahead
  and interpolates locally at 60 fps. No per-frame HTTP.
- Writer: new EVENT `driver` so new recordings carry driver names; old recordings show `#<race number>` or `Car N`.

## 1. Facts used (FACT, source checked 2026-10-14 by Claude)
- F1 Every DATA tick stores per car (`storage_writer.inc:201-283`): 1 lap_dist, 2 speed m/s, 3 throttle, 4 brake,
  16 gear, 17 track_x, 18 track_z, 68 position, 69 lap_number, 70 pitting, 79 fuel, 80/81 gap ahead/behind m,
  82/83 gap ids, 86 last_lap s, 97 race_num, 98 controller_role, 122 finish_state, 9 yaw. Per-tick sim time
  (`SV2_FLAG_HAS_SIM`, `storage_v2.inc:28`, sim_ms deltas `:531`).
- F2 Block index exists with sim span per block (`storage_v2.inc:1197-1215`) and an overlap helper
  ("Blocks whose sim span overlaps [t0, t1]", `storage_v2.inc:1575`). Lap rows with start/end sim (`:1213-1215`).
- F3 EVENT blocks: lap, pit, incident(limits/contact) as raw JSON with sim time (`storage_writer.inc:874-935`).
- F4 Names are NOT stored (`car_telemetry_t.name` / `car_name`, `web_telemetry.c:866-867`, never written).
- F5 Review adapter entry: `sa_api_review_route` (`storage_api.inc:1165`), gates `sa_rv_id_check` (404/409) and
  `sa_rv_ready` (503 + Retry-After while index builds). Route list `sa_rv_is_api_route` (`:1121`).
- F6 HTTP is one thread; response buffer `g_http_big` 2 MiB. Work per request must stay bounded.

## 2. Server (H-S): files `storage_api.inc`, `storage_v2.inc` (index walk only), `storage_writer.inc`,
`web_telemetry.c` (dispatcher lines only), `runtime_test.c`, `schemas/replay.json`, `schemas/replay_info.json`,
`schemas/version.json` (via `python -I schemas/update_version.py 10`), `contract_validate.py`, `test_telemetry.py`,
`API_CONTRACT_2026-10-09.md` (append section 18).

### 2.1 `GET /replay/info?session=<id>` (same gates as the review routes: 404/409/503)
```json
{"schema":"f1c.replay.info/1","session":"v2_...","stem":"...","track":"...","session_type":7,
 "t_first":12.345,"t_last":5432.1,"hz":20,
 "cars":[{"slot":0,"name":"M. Schumacher","team":"Ferrari","num":1,"role":"player|ai|remote","laps":58}],
 "events":[{"t":123.4,"type":"lap|pit|incident","car":0,"lap":2,"last_lap_s":91.234,"kind":"contact","with":3,"state":1}],
 "outline":[[x,z],...]}
```
- `name`: newest `driver` EVENT for the slot; else `"#"+num` when channel 97 > 0; else `"Car "+slot`. `team` "" when unknown.
- `role`: channel 98 (controller_role) as in the live API: 0 player (local), 2 remote, else ai. Use the same mapping
  the live `/telemetry` uses (find it in `web_telemetry.c`, cite the line in the record).
- `laps`: highest channel 69 seen.
- `events`: every EVENT block except `driver`, sorted by t, cap 4000 (then `"events_truncated":true`).
- `outline`: track_x/track_z of ONE complete timed lap (prefer the player's fastest lap, else any car's first full
  lap from the lap index), decimated to at most 600 points (keep a point when >= 8 m from the last kept one).
  Empty array when no complete lap exists.
- Cache the info document in the review slot after the first build (it reads the whole file once; the index build
  already walks all blocks, so collect cars/events/names there if that is simpler: allowed in the `storage_v2.inc`
  index walk). Must not block the HTTP thread for > 2 s on a 1-hour race; measure on the largest real file in
  `%TEMP%\RECREV_2026-10-14` and record the time.

### 2.2 `GET /replay?session=<id>&t0=<s>&t1=<s>`
- `t1 - t0` <= 30 s (else 400 "window too long"), t0 < t1, both finite. Clamp to [t_first, t_last].
- Output decimated to 5 Hz (one tick every >= 0.2 s of sim time per car; always include the last tick in the window).
- Compact columns to stay small (target < 400 KB for 24 cars x 30 s):
```json
{"schema":"f1c.replay/1","session":"v2_...","t0":100.0,"t1":130.0,
 "cols":["t","x","z","spd","pos","lap","pit","gear","thr","brk","dist","fin"],
 "cars":[{"slot":0,"rows":[[100.0,12.3,-45.6,71.2,1,3,0,5,0.98,0.0,1234.5,0],...]}]}
```
  Numbers: t 3 decimals, x/z/spd/dist 1 decimal, thr/brk 2 decimals, others integers. A missing channel = null.
- Reads only blocks overlapping [t0, t1] (F2 helper). Allocation per request, freed before return.

### 2.3 Writer: `driver` EVENT (new recordings only)
- `{"type":"driver","t":<sim>,"car":<slot>,"name":"...","team":"...","num":<race_num>}` written when a slot is first
  seen in a session and whenever its name changes. Names are copied on the capture side under the existing lock that
  `sw_fill_sample` runs under (`g_telemetry_cs`) into a fixed per-slot table inside the ring event or a side table;
  the writer thread formats the JSON. Respect the lock order telemetry -> storage. No allocation on the capture side.
  JSON-escape names (reuse `sw_json_body`). Team: `car_name` field (`web_telemetry.c:867`) if that holds the team/car
  text; state in the record what it holds.
- The B2 index and existing review routes must ignore unknown EVENT types (check; fix if not).

### 2.4 Tests (H-S, all must run; paste the PASS lines in the record)
- `runtime_test.c`: synthetic recording with 3 cars, 2 laps, a pit and a driver EVENT -> `/replay/info` names, events,
  outline non-empty; `/replay` window rows at 5 Hz, columns, clamp, 400 for 31 s window, 404/409/503 gates.
  Old-format file without driver EVENT -> `#num` / `Car N` names.
- Schema validation of both docs with `contract_validate.py`.
- Real files: serve mode on `%TEMP%\RECREV_2026-10-14` (RECREV_ROOT harness from H-1) -> info + 3 windows per
  session OK; record sizes and timings.
- Full gate with the game closed (`tasklist | grep -i "F1 Challenge"` empty; Lightloader.exe is not the game):
  PowerShell 7: `$env:TELEMETRY_TESTS='1'; .\build.bat` in the component dir; if it stops at the payload pin, run
  `python pin_sidecars.py --payload "$env:TEMP\f1c-web-telemetry-4.4.0"` and rerun. EXIT 0 required.

## 3. Viewer (H-V): files `telemetry_viewer.js`, `viewer_test.js`, `viewer_review/mock_feed.js` only
### 3.1 Page
- Page id `replay`, name "Replay", group Data, listed right after Recordings. In LIVE mode the page shows:
  "Replay plays a finished recording. Open one on the Recordings page." with a button "Open Recordings".
- In review (`?session=v2_...`) the Replay page is the page that opens by default (owner wants to watch), other pages
  keep working.
- Recordings page: each session card with a review_id gets a primary button "Watch" (opens `/?session=ID&page=replay`)
  next to the existing load/open action.
### 3.2 Layout (match the existing console style: same colours, fonts, panels as the Race / Session Overview page;
use page-styled controls, never native `<select>`)
- Left 70 %: track map canvas. Outline from `/replay/info` (thin line), cars as dots with race number inside
  (colour by team if known, else a fixed palette by slot), player car larger with a ring, remote (CO-OP) car with a
  second ring colour, pitting cars dimmed with "PIT". Click a dot = follow that car.
- Right 30 %: timing tower at the current time: position, name, lap, gap to car ahead (`gap ahead m / speed`, shown
  as +s.s; "PIT" when pitting; "FIN" when finish_state set). Click row = follow.
- Bottom bar: play/pause button, speed buttons 1x 2x 4x 8x 16x, time "Lap L/N  mm:ss.s / mm:ss", scrub bar with
  lap-start ticks (leader laps) and event marks (pit = yellow, contact = red, limits = grey); hover a mark shows its
  text; click a mark seeks 3 s before it.
- Follow panel (under the tower): name, speed km/h, gear, throttle and brake bars, lap, last lap time (from lap events).
- Keys (only on this page): Space play/pause, Left/Right -5/+5 s, Shift+Left/Right -30/+30 s, Up/Down speed.
### 3.3 Data and playback
- On open: GET `/replay/info` (handle 503 with the existing "Preparing the recording" wait). Start at t_first, paused.
- Buffer windows of 30 s: keep current and next; fetch next when within 10 s of the window end; on seek fetch the
  window containing the target. Never more than 2 requests in flight.
- Interpolate x, z, dist linearly between rows; position, lap, pit, gear take the previous row. A gap > 2 s in a car's
  rows = car hidden in that span (not interpolated).
- Animation with requestAnimationFrame; stop when the page is hidden or another page is active.
- Errors: show the server message in the page; never a blank page.
### 3.4 Tests (H-V)
- `viewer_test.js`: interpolation, gap rule, window prefetch decision, tower ordering and gap text, name fallback,
  LIVE-mode note, Watch button href, keys. Run `node viewer_test.js` (paste the PASS count).
- `viewer_review/mock_feed.js`: serve synthetic `/replay/info` and `/replay` so `viewer_review/index.html` shows the
  page offline.

## 4. Contract between H-S and H-V
Section 2.1 and 2.2 JSON shapes are fixed. Neither worker changes them without Claude.

## 5. Rules (both workers)
- Read `%USERPROFILE%\Desktop\F1\AGENTS.md`. No deletes (no `rm`, not even in `/tmp`: in Git Bash `/tmp` is the
  Windows TEMP and an earlier worker deleted its own before-copies that way). Before-copies of every edited file to
  `%TEMP%\REPLAY_before_2026-10-14\` first. Keep each file's line endings (check bytes with Python before and after).
- Never touch `F1 Challenge V10\Telemetry\recordings` (read-only; copies only in `%TEMP%`).
- Edit only your listed files. Write your record: H-S `REPLAY_SERVER_RECORD_2026-10-14.md`, H-V
  `REPLAY_VIEWER_RECORD_2026-10-14.md` (changed file:line, tests with output lines, deviations, open items,
  FACT/INFERENCE tags). Do not edit CURRENT_STATE.md (Claude indexes).

## 6. Not in this batch
- Weather, TV Director, sound, pit-stop detail pages in replay; replay of an open (still recording) session;
  driver names for recordings made before this build (not stored); LAN viewing changes.

## 7. Acceptance (owner live, after Claude review and gate)
- A1 After a session ends, Recordings -> Watch opens Replay; pressing Play moves all cars around the map.
- A2 Tower order matches the cars on the map; scrub and speed buttons work; clicking a car follows it.
- A3 Race of 2026-10-14 20-54 (Australia 1997) plays from start to finish without errors.
