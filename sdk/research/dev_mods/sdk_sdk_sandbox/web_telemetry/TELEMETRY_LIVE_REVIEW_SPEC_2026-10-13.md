# Telemetry 1.1.0: LIVE and REVIEW modes, full recording inventory (SPEC, DRAFT r1)

Date: 2026-10-13. Product: Telemetry (`web_telemetry`), candidate 1.1.0 (API 9 today, not installed).
Pipeline stage: **specify**. Spec only: no code, no build, no gate, no live probe, no install.
Coordination: another agent is editing `director_state.inc`, `telemetry_viewer.js` and `web_telemetry.c`
(TV Director). This spec changes none of them. B1 starts after that merge (section 5).

Owner model (2026-10-13, "Owner product model" in `RECORDING_REVIEW_SPEC_2026-10-13.md`):
1. **LIVE** = now, plus everything earlier in the current session.
2. **REVIEW** = a loaded recording watched retroactively in full: timeline playback of the whole session
   (positions, gaps, track map, gauges, events, pits, incidents, weather, every car) plus lap analysis and compare.
3. Rule (18:00, still holds): all data telemetry can log must be stored in recordings.

Relation to the earlier spec: `RECORDING_REVIEW_SPEC_2026-10-13.md` (r1, "laps first") is NOT rewritten. Its R1
writer work and server adapter stay the base of B1-B2 here. Section 8 lists every point where this spec corrects
or supersedes it.

Tags: `FACT file:line` = read in the source named (checked 2026-10-13). `INFERENCE` = reasoned, to verify.
`ESTIMATE` = arithmetic or assumption, to measure. `OPEN` = needs an owner decision or a test.
`DECISION` = owner words (quoted). `PROPOSAL` = this spec. `CORRECTION` = changes the earlier spec.

---

## 0. Summary (read first)

- **The gap is in the recording.** A v2 file stores, per car per tick, 17 channels for AI cars and up to about
  60 for the player (section 1). It does not store game sim time per tick, positions, lap numbers, lap times,
  names, teams, tyre compounds, pit, incident or classification events, weather history, track geometry, sector
  and flag state, damage, most of the player chassis, or the research channels.
- **The 1.0.1 writers that held these items are off in 1.1.0.** `ar_init` is called only by the test harness
  (`runtime_test.c:1957`, `:2118`). Every `ar_*` producer returns at its first check when `g_ar_root` is empty
  (`session_archive.inc:176-182`, `:184-186`, `:230-232`, `:239-241`). The race log uses the same journal
  (`web_telemetry.c:1121-1130`). So lap, pit, incident, wear and classification events are written nowhere in 1.1.0 today.
- **Time is the main design issue.** `tick_ms` is wall clock (`GetTickCount()`, `compatibility_runtime.inc:237`,
  `storage_writer.inc:267`). Session sim time is stored only once per DATA block (`t_first`, `storage_writer.inc:706`).
  The writer emits a sample on every tick while a session is open (`storage_writer.inc:181`), so a paused game
  writes frames with a moving wall clock (INFERENCE, verify). Lap times and the review clock must use sim time, which
  needs a per-tick channel (section 1, row T1). **CORRECTION:** AR-1 of the earlier spec (lap time = `tick_ms`
  difference) is wrong across a pause and is not the game's lap time.
- **LIVE history today is small.** It lives in RAM rings (120 laps per car, 400 incidents, 128 pits, 2,048 weather rows,
  2 trace cars with 6 laps each) and in the browser tab. One owner log already logged 666 incidents in a session
  (`compatibility_runtime.inc:218`), so the 400-entry ring is too small for a long race. Recommendation (section 2): LIVE history
  reads the open `.part` through the same index code as REVIEW (committed blocks, lag up to 1 s), with the RAM rings as
  the instant path for the last minutes.
- **REVIEW is one clock: session sim time.** One index per recording, built on a low-priority job. A scrub is a binary
  search to one 1-second DATA block, which already decodes alone (keyframe at each block start,
  `STORAGE_MANAGER_SPEC_2026-10-13.md` 4.4). The existing pages keep their code: the server answers
  `/telemetry?session=<id>&t=<sim>` with the live frame key names for stored channels, and declares the rest as missing.
- **Batches** (section 5): B1 writer additions and format (sim per tick, slow-channel and event blocks, META drivers,
  track snapshot); B2 index and lap analysis for every car; B3 timeline core (play, pause, speed, scrub, jump to lap or
  event); B4 remaining pages and event lists; B5 hardening (memory, long sessions, Host and LAN). Each batch is gated
  offline and then owner-tested live.
- **Size, 2 h race, 22 cars, recommended set:** about 70 to 105 MB working (ESTIMATE, section 6). The new channels and
  events add about 3 to 8 MB of that; the rest is the existing channels and the player chassis.
- **Owner questions:** 7 (section 7), each with a recommendation.

---

## 1. Inventory: what telemetry can log today, and what a recording holds

### 1.1 Rule used for "stored"
A value counts as stored only when one of these is true:
- (a) it is a channel in the catalog (`storage_v2.inc:164-232`) **and** `sw_car_from` writes it
  (`storage_writer.inc:201-262`), or
- (b) it is a field of META (`sw_meta_format`, `storage_writer.inc:758-771`; batch 4 subset, D-9 in
  `STORAGE_V2_BATCH4_RECORD_2026-10-13.md:125-134`).

Everything else is **not stored**. Sources of "can log": the live frame `json_telemetry` (`web_telemetry.c:5828-6000`,
per-car keys `:5942-5990`), the 1.0.1 hooks (`session_archive.inc`), the race log (`web_telemetry.c:1121-1290`), the
in-memory rings and the route table (`http_archive_route` `:8714-8765`, live routes `:8936-9200`).

Sizes below are per car per minute at 20 Hz (1,200 ticks), ESTIMATE from the ranges in `STORAGE_MANAGER_SPEC` 4.5 and 5:
a changing Q16 value costs about 1.5 B per tick raw and compresses about 2x (XPRESS); a constant value costs about 0.
"CHG" means change-only: a record is written only when the value changes.

### 1.2 Inventory table

| # | Item telemetry logs | Source (FACT file:line) | In v2 recording today | Add in B1 (kind, id) | Size per car-min (ESTIMATE) |
|---|---|---|---|---|---|
| T1 | Session sim time `st` | frame `:5859-5882`; writer `storage_writer.inc:268` | **Block start only** (`t_first`, `:706`, `:811`). Not per tick | DATA-level `sim_ms` deltas, header flag (B1 format) | ~0 (shared, not per car) |
| T2 | Wall-clock tick | `storage_writer.inc:267`; `compatibility_runtime.inc:237` | Yes (channel 0, `tick_ms`) | none | in base |
| T3 | Phase `ph`, race active `ac`, raw active `acr`, session type `ty` | `:5859-5860` | Type only, in META at open (`sw_meta_format`) | CHG session channel `phase`, `race_active` | ~0 |
| T4 | Total laps `tl`, vehicle count `vc`, championship `cm`, race mode `rm`, player index `pi`/slot `ps`, AI difficulty `ai`, time threshold `rt` | `:5860-5865` | Track name only (META `track`, `track_raw`) | META fields; CHG for `vc`, `ps` | ~0 |
| T5 | Lap length `lapm`, sector cuts `sc1`/`sc2` | `:5877-5878`; `g_telemetry.lap_len` `web_telemetry.c:4419` | No (earlier spec 5.7 proposed `lap_len_m` only) | META `lap_len_m`, `sc1`, `sc2` (open and final) | 0 |
| T6 | Track geometry (waypoints, up to 3,000 x/z points) | `build_track_json` `web_telemetry.c:2640-2655`; `MAX_WAYPOINTS` `:412` | No (`/track` reads game memory live) | TRACK block, once per session, <= 24 KB | 0 (per session) |
| T7 | Sector flag strip `sg`, segment count `sgn`, `sgl`, yellow count `yc`, flags | `:5865-5872`; `flags_reset` `web_telemetry.c:4823` | No | EVENT `flag` on change | ~0.1 |
| T8 | Weather: ambient `at`, track temp `trkt`, wetness `onw`/`offw`/`tw`, grip `tcf`/`tgf`, condition | `:5860-5866`; weather ring `web_telemetry.c:2339-2393` (2,048 rows, 10 s, `:2159`) | No (RAM ring only) | EVENT `weather` every 10 s or on change | ~0.03 |
| T9 | Player brake temps `btmp[4]` | `:5870-5874` | No | Q16 `btemp_fl..rr`, player only | 2-3 (compressed) |
| T10 | Player carcass temps `ttmp[4][3]` (3 zones per tyre) | `:5867`; `tire_temp[4][3]` | Only the single per-wheel value (`ttemp` ids 20-23, `storage_writer.inc:240`); zones **not stored** | CHG `ctemp_<wheel>_<zone>`, deadband 0.1 C, player only | 1-3 |
| T11 | Player tyre temp per wheel `ttemp` | `storage_writer.inc:240` | Yes (ids 20-23) | none | in base |
| T12 | Tread, normalized (ids 11-14) | `storage_writer.inc:223-236` | Yes (AI from tire wear, clamped) | none | in base |
| T13 | Raw tyre wear `wfl..wrr` (1.0 = fresh) | frame `:5958` (`wfl`..`wrr`) | **Not stored raw**: AI wear is normalized and clamped (`storage_writer.inc:228-232`) | Q16 `wear_fl..rr`, 20 Hz | 4-7 |
| T14 | Blister `bls[4]` | `:5874` | No | CHG per wheel | ~0 |
| T15 | Player oil/water temp `otp`/`wtp`, engine life `pel`, per-car `ot`/`wt`/`ot2`/`wt2` | `:5872-5873`; per-car keys `:5962` | No | CHG (deadband 0.2 C) | 0.5-1 |
| T16 | Player chassis `pch` (about 79 values, `json_pch` `:5796-5826`) | writer 24-63 (`storage_writer.inc:243-258`) | **40 of about 79.** Not stored: `pch_press_set`, `pch_opt_c`, `pch_roll`, `pch_disc_fail_mm`, `pch_bent[4]`, `pch_detach[4]`, `pch_eng_wear`, `pch_clutch_rpm`, `pch_oil_press`, `pch_bias`, `pch_drag`, `pch_lift`, `pch_drag_used`, `pch_in_raw[4]`, `pch_in_used[4]` | ids 64-127 (reserved in `STORAGE_MANAGER_SPEC` 4.5) for the missing values; ranges per OQ-5 | 20-30 (player only, compressed) |
| T17 | Lap distance `dist` | `storage_writer.inc:208` | Yes (channel 1) | none | in base |
| T18 | Speed, throttle, brake, steer (AI), glat, glon, gvert, yaw, yaw_rate, rpm, gear, tx, tz | `storage_writer.inc:208-237` | Yes. Note: gvert is written for **all** cars with validity bit 32 (`:218`) | none | in base (20-34 total) |
| T19 | Validity bits `vf` (speed, throttle, brake, steer, tread, accel) | `:5982-5989` (v5 comment, `vf`) | **No**. Channel 19 (`flags`) is not written (`storage_writer.inc:238-239`) | RAW32 `validity` per car, CHG | ~0.1 |
| T20 | Trace flags TRF (21 bits: sample validity, interpolation, pit, out of range) | `web_telemetry.c:1470-1497` | No (`flags` not written) | channel 19 `flags` for all cars, CHG-compatible | 0.5-1 |
| T21 | Gaps ahead/behind `ga`, `gb` (m), car ids `gai`, `gbi` | `:5977-5978` (`compute_v4_features`) | No | Q16 `gap_ahead`, `gap_behind`; RAW32 ids | 1.5-2 |
| T22 | Live sector delta `lsd`, `lsn`, `secel`, `lspb`, `lspn`, `bs1`/`bs2`, `ls1`/`ls2` | `:5978-5990` | No | Q16 `sec_delta` at 20 Hz; the rest from lap events | 1-2 |
| T23 | Lap number `l` (`:5942`); lap and sector time keys `p1`, `p2`, `lp`, `lps`, `lpe`, `lpb`, `lpg` (`:5954-5955`, meaning INFERENCE, verify) | `:5942-5955`; lap rows `web_telemetry.c:1313` (120 per car); race log `:1182` | No: lap number and time are not stored; the 1.0.1 trace held `lap_time` (`session_archive.inc:215`) | RAW32 `lap` CHG; EVENT `lap` at completion (game time, sectors, position, validity) | CHG ~0; events ~0.3 KB per lap |
| T24 | Position `p` | `:5942`; `c->position` | No | RAW32 `pos` CHG | ~0.1 |
| T25 | Pit state and box call `cb`, `boxsoon`, `boxwhy`, pit events | `:5949`; `g_pits` `web_telemetry.c:4119-4147` (128); race log `:4210`; `ar_on_pit` `session_archive.inc:230` | No | RAW32 `pit_state` CHG; EVENT `pit` | CHG ~0.1 + events |
| T26 | Tyre compound and choice `tc`, name `etcn` (`tyre_name_for`) | `:5927` (tyre_name_for call) | No | RAW32 `tyre` CHG; name table in META | ~0 |
| T27 | Driver name `n`, car name `cn`, team `tid` / `gar_team` / `gar_bay`, controller role `cr` (CO-OP), AI flag, slot | `:5942-5965`; roster `race_log_open` `web_telemetry.c:1132-1150` | No (META has no drivers, `storage_writer.inc:758-771`) | META `drivers[]` at open and on change | ~0 (per driver 100 B) |
| T28 | Damage `dt`, `fw`, `rw`, `dm`, `dc`, `dpr`, `ed`, `eav`, `u4574`; heuristic attribution `cw` | `:5949-5961`, `:5981` | No | CHG bitfield `damage`; EVENT `damage` | CHG ~0.1 |
| T29 | Track limits `tl` per car, penalties `pn`, `OFF_TRACK_LIMITS` | `web_telemetry.c:792`, `:5274`; `incident_push` `:2597` | No | CHG `limits` per car; EVENT `incident` | ~0.1 |
| T30 | Incidents (LIMITS, PENALTY_TL, POS, contact) | `incident_t` ring 400 (`web_telemetry.c:2469-2475`); `ar_on_incident` `session_archive.inc:239`; race log `:1205` | No | EVENT `incident` (tick, sim, type, car a/b, lap, name) | per event ~0.2 KB |
| T31 | Wear events | race log `web_telemetry.c:1251` | No | EVENT `wear` | rare |
| T32 | Fuel: per-car fraction `fc`, rolling use `favg`, autonomy `faut`; player fuel (pch 62) | `:5952`, `:5981` | Player fuel only (pch 62 is in the stored 40) | Q16 `fuel_frac` 1 Hz per car | ~0.2 |
| T33 | Fastest lap `flapt`/`flapid`, Driver of the Day `dotdid`/`dotdsc` | `:5881-5882`; `compute_v4_features` | No | derived in review from lap events; EVENT `dotd` on change | ~0 |
| T34 | World position `px`, `pz`, `mx`, `mz`, `dx`, `dy` | `:5948` | `tx`, `tz` stored (channels 17-18, `storage_writer.inc:212`). The other pairs are not stored | **OPEN**: check whether `px/pz` equal `tx/tz`; if not, add | 0-3 |
| T35 | Research candidates: AI tyre temp, load, force, suspension, slip (`rx[16]`, 4.3.0 "RESEARCH") | `trace_sample_t` `web_telemetry.c:1521-1522`; research toggle `telemetry_viewer.js:2014-2020` | No | RESEARCH group ids 80-111, tier tag in META; OPEN Q2 | all cars: about 28 (about 40-50 MB per 2 h); trace cars only: 0.6 |
| T36 | Session events: start (cars), end (classification), heartbeat (weather, phase) | `web_telemetry.c:1148`, `:1171`, `:1275` (race log) | No (race log off) | EVENT `session_start`, `classification`; heartbeat covered by T3, T8 | per session |
| T37 | Session boundaries (open, close, reason) | `storage_writer.inc:9-13`, `:145-186` | Yes, as file open/close and END block (reason in writer state only) | keep; store the close reason in END or META | 0 |
| T38 | Trace laps of 2 cars, 6 completed each, 3,200 samples per lap, `sim_t` per sample | `TRACE_CARS` `:1450`, `TRACE_LAPS` `:1451`, `TRACE_SAMPLES` `:1462`, `trace_sample_t` `:1507-1523` | Covered by T1, T17-T18 for all cars (more than 1.0.1 trace) | none beyond T1 | 0 |
| T39 | Health and diagnostics (`/health`, `capture_cost_us`, rate) | `web_telemetry.c:8942` | No | not telemetry data: not stored (decision of this spec, OPEN Q7 scope) | 0 |
| T40 | TV Director state (`/director/state`) | `director_state.inc` (other agent) | No | not telemetry data today; OPEN Q7 (store cuts as EVENT) | 0 |

Gaps of most importance (summary): T1 (sim time), T13 (raw wear), T16 (40 of 79 chassis values), T19-T20 (validity and
flags), T23-T26 (laps, positions, pits, compounds), T27 (names), T30 (incidents), T36 (classification).

### 1.3 Storage facts that bound the design
- Per-tick data is in `DATA` blocks of at most 20 ticks or 1,000 ms, keyframe at each block start, channel-major per car
  (`STORAGE_MANAGER_SPEC` 4.3-4.4; `storage_v2.inc:13`, `:16`). A 1-second block decodes alone.
- Channel ids 0-63 are used; 64-127 are reserved (`STORAGE_MANAGER_SPEC` 4.5). Car count limit 104 (`storage_v2.inc:12`).
- Payload limit 1 MiB + 40 B (`storage_v2.inc:14`).
- Capture cadence: worker `Sleep(50)`, about 20 Hz (`web_telemetry.c:1455-1457`). SSE push every 100 ms, about 10 Hz
  (`web_telemetry.c:852`, `compatibility_runtime.inc:258`).
- The writer ring holds 48 slots (`storage_writer.inc:16`); capture only copies (`storage_writer.inc:265-275`).
- Writer file: `fopen(part, "wb")` (`storage_writer.inc:919`), block-by-block `fflush` (`STORAGE_MANAGER_SPEC` 6.3).

---

## 2. LIVE mode: "current session history"

### 2.1 What each page shows in LIVE today, and how deep the history is
Pages from `telemetry_viewer.js:2285-2287` and `:2954-2963`. "Source" is FACT (route or socket); "depth" is FACT from the cap named.

| Page (id) | Live source | History depth today | Limit (FACT) |
|---|---|---|---|
| Session Overview (`race`) | SSE `/events` (`web_telemetry.c:8217`), `/track`, weather | Weather 2,048 rows (about 5.7 h at 10 s) | `WEATHER_RING` `:2158`, cadence `:2159` |
| Timing and Race Pace (`timing`) | `/laps` (`telemetry_viewer.js:3503`, polled every 5 s `:3477`) | 120 laps per car | `MAX_LAP_HISTORY` `:1291`, `g_lap_rows` `:1313` |
| Driver Focus (`focus`) | `/driver?id=` (`:3514`, polled every 3 s `:3478`) | Driver document, no history | `ar_build_driver_json` is archive-side (`session_archive.inc:778`) |
| Lap Analysis (`overview`), Vehicle Dynamics, Braking, Tyres, Compare (A vs B) | `/trace/status`, `/trace/data` | **2 trace cars only, 6 completed laps each** (plus 1 open), 3,200 samples per lap | `TRACE_CARS` `:1450`, `TRACE_LAPS` `:1451`, `TRACE_SAMPLES` `:1462`. Worst case about 7.2 MB (2 x 7 x 3,200 x 160 B, ESTIMATE from `trace_sample_t`) |
| Live Gauges (`live`) | frame only | None (live by design) | n/a |
| Chassis Research (`research`) | trace `rx[16]` | 2 trace cars, 6 laps | as above |
| Engine Live (`engine`), Car Chassis Live (`chassis`) | frame; `pchHist` in the tab | Browser tab memory only | lost on reload; `pchPush` `telemetry_viewer.js` (`onFrame` `:3452`) |
| Incidents (list, not a page) | `/incidents` every 3 s (`:3476`) | 400 incidents | `MAX_INCIDENTS` `web_telemetry.c:2469` |
| Pits (list) | `/pits` (`:3507`) | 128 pit events | `MAX_PIT_EVENTS` `:4119` |
| Recordings (`sessions`) | `/recordings`, `/recordings/jobs` | Files on disk | n/a |
| Legacy layouts (23 entries, `WB.LAYOUTS`) | frame | Frame only | `telemetry_viewer.js:2965` |

### 2.2 Limits of today's LIVE history (FACT unless marked)
- **Caps:** the numbers above. A 2 h race with 22 cars can exceed the incident ring (see the 666-incident owner log,
  `compatibility_runtime.inc:218`). Lap rows (120 per car) are enough for a race. Trace history covers only two cars.
- **Two session notions.** `session_history_clear` (`web_telemetry.c:4818-4830`) clears incidents, laps, flags and pits on a
  broker session change and on "leader lap went backwards" (`:4878`, `:4894`). The writer closes a session on a type
  change, a sim-time drop, results (sim at or above 1e8), or 30 s stale (`storage_writer.inc:9-13`, `:157-186`).
  These boundaries differ. **OPEN Q1.**
- **Browser memory.** The tab keeps the history (`WB.LIMITS` is set only in review, `telemetry_viewer.js:2931`). A reload
  loses it. The server RAM rings survive a reload but not a DLL restart.
- **Recording off.** If the writer is off (`sw_enabled` 0, `compatibility_runtime.inc:335`, `storage_writer.inc:1135-1145`),
  there is no file and no history beyond the rings.
- **Pauses.** Wall clock keeps running in LIVE; any "minutes ago" or lap time computed from wall clock is wrong across
  a pause (see T1 and section 0).

### 2.3 LIVE after B2 (PROPOSAL)
- **Two layers.**
  1. RAM tail: the last 60 s of frames (all cars, 10 Hz, the SSE frame already built) kept in a ring, for the instant
     "back 30 s" controls. Bounded: 60 s x 10 Hz x about 4 KB = about 2.4 MB (ESTIMATE).
  2. Committed file history: the open `.part` read through the same block index as REVIEW (section 3), extended as new
     blocks commit. One DATA block is about 1 s, so the lag is at most about 1 s (`STORAGE_MANAGER_SPEC` 4.3).
- **Reads are read-only.** A reader opens the `.part` with shared read access and takes a snapshot of the committed block
  count under a short lock (`sa_cs`-style, never `g_telemetry_cs`). The writer is not paused.
  INFERENCE: the CRT `fopen` used by the writer (`storage_writer.inc:919`) allows a concurrent read on Windows. Verify in
  B2 with a writer-reader stress test.
- **Definition of "current session"** = the writer's open session (one file). The old hard-coded boundary in
  `session_history_clear` is replaced by it, subject to Q1.
- **If recording is off:** the page shows a banner "Recording is off: history is kept in memory only (last 60 s and the
  rings)". PROPOSAL text.
- **Crash:** after a crash the `.part` is repaired at next start (`recovered`). LIVE does not show it; REVIEW does.

---

## 3. REVIEW mode

### 3.1 One timeline and clock
- Clock `review_t` = session sim time, seconds, f64. Sim time is what the game uses; wall clock is not used.
- Controls: play, pause, speed 0.25x, 0.5x, 1x, 2x, 4x, 8x; scrub; jump to next or previous lap of a selected car; jump to
  next or previous event (incident, pit, flag, classification, session start, session end).
- A paused game (sim unchanged) keeps `review_t` still. Samples with the same sim time are written but shown once; the
  timeline shows the pause as a band (T1 makes this visible).

### 3.2 Data path (server)
- **Index (per recording, built once, in memory):**
  - (a) block index: one record per DATA block `{file_offset, seq, t_first, t_last, ticks, stored_len, raw_len}`. About 32 B
    per block; 7,200 blocks for 2 h = about 230 KB (ESTIMATE).
  - (b) lap index: `{car, lap, start_sim, end_sim, game_time or null, source: event | derived, flags}`. About 80 B per lap;
    1,300 laps for 22 cars x 2 h = about 104 KB.
  - (c) event index: `{t_sim, type, car, offset}` (about 64 B per event).
  - (d) track geometry (TRACK block) and META (drivers, lap length, cuts, channel manifest).
  - Cache: LRU of 2 sessions; at most 2 decoded blocks per session (at most 2 MiB).
- **Frame at t:** binary search of `t_first` in the block index, read one block (at most 1 MiB + 40 B), decode its 20 ticks
  for all cars, select the tick with `sim` nearest to `t`. A seek costs one block read plus one decode.
- **Window for playback:** `frames(t0, span, hz)` decodes consecutive blocks; limits: at most 30 blocks (30 s of sim) per
  request and at most 1 MiB output. Playback fetches the next window ahead of the clock.
- **Lap analysis:** from the lap index. Game lap time when a lap event exists (`source: event`); derived from sim-time
  line crossings only when the event is missing (`source: derived`). Pit and out laps are shown with a mark from
  validity and pit state (T19, T25), not removed.
- **Traces:** `trace(car, lap)` reads only that lap's blocks (found via the lap index). At most 3,200 samples per lap.
- **Events:** from the event index, paged (at most 500 per response).
- **Track:** TRACK block, once per session; if absent (older files), the page says "Not recorded in this recording".

### 3.3 Frame shape (keeps the pages)
- A review frame reuses the live frame key names for every stored channel (for example `c[].id`, `p`, `l`, `s`, `th`,
  `br`, `g`, `rpm`, `wfl`, `glat`, `glon`). The viewer code then needs no new key names for stored data.
- A key that is not stored is **absent**, not zero. META lists `channels_missing` so the page can say "not recorded in this
  recording" per panel. Sentinels follow `API_CONTRACT_2026-10-09.md` section 3.
- `session_generation` is set to a per-file value so the viewer's generation check (`telemetry_viewer.js:3444-3450`)
  resets its histories on a new recording, not on each seek. Seeks reset the histories explicitly (section 4).

### 3.4 Routes (PROPOSAL, API 10, all GET, local peer only in phase, see section 6)
| Route | Answer | Notes |
|---|---|---|
| `GET /recordings/review?session=<id>` | `f1c.recordings.review/1`: state (none, building, ready, error), duration (sim), cars, laps, `channels_missing`, `reason` | Already in the earlier spec (5.3). Extended with duration and missing list |
| `GET /telemetry?session=<id>&t=<sim>` | one review frame at sim time t | The existing `session=` branch (`web_telemetry.c:8865`) gets a v2 branch in B2. Replaces the snapshot of the earlier spec |
| `GET /recordings/frames?session=<id>&t=<sim>&span=<s>&hz=<n>` | array of review frames | Bounded: span x hz at most 240 frames and 1 MiB |
| `GET /recordings/events?session=<id>&from=<sim>&to=<sim>&type=<t>&limit=<n>` | event rows | Limit 500 |
| `GET /recordings/laps?session=<id>` | all laps of all cars | About 130 KB for 2 h, 22 cars (ESTIMATE) |
| `GET /trace/data?session=<id>&car=<slot>&lap=<n>` | samples of one lap | Existing shape `f1c.trace/1`, with `"source":"recording"` |
| `GET /recordings/track?session=<id>` | geometry, at most 256 KB | 404 with `not recorded` for older files |
| `GET /recordings/meta?session=<id>` | drivers, track, lap length, cuts, channel manifest, schema | |

Notes:
- `reviewUrl` (`telemetry_viewer.js:3897-3900`) does not add `session=` to `/recordings*`. Callers of the new
  `/recordings/*` routes must add `session=` themselves (R-5 of the earlier spec).
- Phase "in review" never answers POST. The write guards (`web_telemetry.c:8836-8858`) stay as they are.

### 3.5 Threading, memory and capture isolation
- **Facts:** all GET requests are served one at a time on `http_thread` (`web_telemetry.c:9347-9353`, F-THR-1 of the
  earlier spec). SSE clients are fed by the capture thread (`compatibility_runtime.inc:258`, `sse_broadcast`
  `web_telemetry.c:8047`), so a slow review request does **not** stall the SSE feed, but it does stall GET `/telemetry`
  and `/health`. **CORRECTION** to the earlier spec's F-THR-1 reading (it stated a stall of live `/telemetry` only).
- **Rules:**
  - R-V1 Review decode runs on a dedicated low-priority review worker (B2), with a queue of at most 4 requests. The HTTP
    thread copies results only, and answers `503 building` or `busy` instead of waiting.
  - R-V2 No review code takes `g_telemetry_cs`. Lock order: `g_telemetry_cs` -> `g_storage_cs` -> `rv_cs` (unchanged rule R-T1).
  - R-V3 No request reads a whole file. Index build reads block headers and payloads once (linear).
  - R-V4 A single response is at most 2 MiB (`TRACE_JSON_BYTES`, `web_telemetry.c:3229`).
- **Memory caps:** index at most 8 MB per session (ESTIMATE, section 6), 2 sessions cached, at most 2 decoded blocks
  (at most 2 MiB), decode scratch at most about 1.1 MB (104 cars x 128 channels x 20 ticks x 4 B, upper bound).
- **Budgets to measure (not claimed):** index for 2 h in at most 3 s; seek p95 at most 100 ms; capture cost p95 under
  500 us while review runs (AR-3 of the earlier spec).

### 3.6 Lap-time and sim-time facts used by REVIEW
- FACT: the writer keys samples on wall clock (`tick_ms`) and stores sim time once per block (section 1, T1).
- INFERENCE: while paused, the game still yields captures and the writer still writes ticks (`storage_writer.inc:181`).
  The weather code already expects "paused frames" (`web_telemetry.c:2379`: "paused frame: never append"). Verify in B1
  with a 60 s pause: count the ticks in the file against the sim advance.
- Consequence: lap times for the lap list come from game lap events (T23) or from sim-time crossings, never from `tick_ms`.

---

## 4. UI: LIVE and REVIEW

- **One switch, two states.** Top bar: `LIVE` or `REVIEW: <display name>` with a `Back to LIVE` link. The pill word and class
  already exist (`telemetry_viewer.js:3550-3556`: `REVIEW`, `review`, the `Saved session ... back to live` text). For v2
  recordings the banner text replaces "Saved session" (earlier spec 8.2). Display name = replay-style stem
  (`COUNTRY YEAR - Mode - Session - date time`), escaped with `esc()`.
- **Same pages in both modes.** Each page reads a clock source: LIVE = SSE frame plus the LIVE history index; REVIEW =
  the review clock (3.1). No page has a separate review copy.
- **Timeline bar (REVIEW only):** play and pause, speed buttons, scrub bar with markers for laps (all cars, thin) and
  events (incident, pit, flag, classification), a sim-time readout (h:mm:ss) and the session type name, previous and next
  lap for the selected car, and an event list (paged).
- **Car selection in REVIEW:** follow leader, follow player, or A and B from the Compare page. Compare keeps working as in LIVE.
- **Live Gauges:** in REVIEW it shows the gauges of the review frame at the clock. PROPOSAL, owner says "gauges" in the product
  model. Not live-only in REVIEW.
- **Seek and speed changes** reset the viewer's per-session histories (`pchHist`, incidents list, pace list) and then refill
  from the frames at the new clock. No history is stitched across a seek.
- **"Not recorded" text:** `Not recorded in this recording.` on a panel whose channels are in `channels_missing`. For
  recordings made before B1 the panel says `Not recorded in recordings made before 1.1.0 B1.`
- **Pre-B1 recordings (no sim per tick, no events):** REVIEW works at 1-second resolution (block times). Banner
  `Reduced review: this recording was made before the 1.1.0 storage update (1 s resolution, no events).` Lap list is
  withheld when lap length is missing (earlier spec 8.4 text kept).
- **Open recordings:** not reviewable until closed (earlier spec Q3, owner decided). LIVE covers them.
- **Delete under review:** refused (owner, 18:00). The Recordings page keeps the earlier spec's notice.

---

## 5. Batch plan

Each batch ships alone (gated and installed only after owner approval). Test lists: offline (game closed, host build in
`%TEMP%`, `-Wall -Wextra -Werror`, the existing `runtime_test.c` and `test_telemetry.py` pattern) and owner live (game
closed to start, owner runs the game).

### B1: writer additions and format (writer and reader, no page change)
Scope:
- Per-tick sim time (`sim_ms`, DATA-level delta list, header flag bit `has_sim`) (T1).
- New per-car channels, all in ids 64-127 unless noted: raw wear (T13), carcass temps CHG (T10), brake temps (T9),
  oil/water/engine life (T15), validity CHG (T19), flags for all cars (T20, channel 19), gaps and their ids (T21), sector delta
  (T22), lap number CHG (T23), position CHG (T24), pit state CHG (T25), tyre compound CHG (T26), damage CHG (T28), limits
  CHG (T29), fuel fraction 1 Hz (T32), remaining player chassis (T16), world pairs if OPEN T34 says so.
- EVENT block (type 5, JSON, size cap 512 B per event): `lap`, `pit`, `incident`, `wear`, `flag`, `weather`, `damage`,
  `dotd`, `session_start`, `classification` (T23, T25, T30, T31, T7, T8, T28, T33, T36).
- META: `drivers[]` (T27), `lap_len_m`, `sc1`, `sc2` (T4, T5), `channels` manifest, close reason (T37). META max 16 KB stays,
  drivers go in a separate META part if larger (`parts` array already exists, `storage_writer.inc:765`).
- TRACK block (type 6), once per session at open, <= 24 KB (T6).
- CHG records: implemented as a per-channel change list inside the DATA block, or as a CHG block (type 7). Decision in B1 (OPEN Q4 default: CHG list inside DATA).
- Header: minor version 2.1 with the `has_sim` flag. Reader refuses an unknown **required** flag, ignores unknown optional blocks.
- Writer load: no new locks; new data copied in the capture copy (the ring copy stays O(1) per tick, `storage_writer.inc:265-275`).

Done when: tests B1-1 to B1-7 pass and the existing gate passes.
- B1-1 round trip of every new channel: RAW32 exact; Q16 within the bound of `STORAGE_MANAGER_SPEC` 4.5.
- B1-2 sim time per tick: monotonic; a pause span (sim unchanged) is detected; block `t_first` equals the first tick's sim time.
- B1-3 events: valid JSON, each at most 512 B, ordered by sim time.
- B1-4 corrupt new blocks (truncation, CRC, unknown required flag): reason codes only, no crash.
- B1-5 old v2 files (batch 4 and batch 5 recordings) still read, with `channels_missing` set.
- B1-6 size per car-minute on a 10-minute synthetic 22-car file against section 6.1 (fail above 1.5 x estimate).
- B1-7 capture cost p95 under 500 us with the added channels (AR-3 of the earlier spec).
Owner live B1: a 10-minute session with one 60 s pause; the file holds sim per tick and events; owner reads the file sizes.

### B2: index, lap analysis, LIVE history reads
Scope: block index; lap index (game events first, derived fallback); `GET /recordings/review`, `/recordings/laps`, `/trace/data`
for any car (not only two); `/telemetry?session=&t=` at block resolution; incremental index for the open `.part` (LIVE layer 2,
section 2.3); review worker thread (R-V1).

Done when: tests B2-1 to B2-5 pass.
- B2-1 lap index: game-event laps match synthetic laps within 1 ms; derived laps match within 0.5 s.
- B2-2 sim-based lap time equals the game lap time across a pause in the synthetic file.
- B2-3 open `.part` read while written (writer 20 Hz, reader 10 Hz, 10 minutes): no torn read; committed count monotonic.
- B2-4 any car, any lap from the index; the reader touches only that lap's blocks (instrumented reader).
- B2-5 review worker busy returns 503; GET `/health` answers within 50 ms while a 50 ms-per-block read hook runs.
Owner live B2: lap times for two laps of two cars within 0.1 s of the game; LIVE history of a car that is not traced appears in Lap Analysis.

### B3: timeline core
Scope: review clock; `GET /recordings/frames`; viewer clock wrapper and seek reset; play, pause, speed, scrub, lap jump and event
jump; the Race, Timing, Lap Analysis, Dynamics, Braking, Compare and Live Gauges pages read the clock; LIVE layer 1 (RAM tail).

Done when: tests B3-1 to B3-4 pass. B3-1 viewer clock tests in `viewer_test.js` (seek reset, speed, pause). B3-2 frame at t equals writer input bit-exact on the synthetic file. B3-3 seek p95 under 100 ms on a 1-hour file. B3-4 playback at 4 Hz for 10 minutes with memory growth under 8 MB.
Owner live B3: L-R1 play a race at 4x, pause, scrub, jump to a lap; L-R2 positions and gaps match the game's timing screen at three points.

### B4: remaining pages and event lists
Scope: Driver Focus (names from META), Pits and Incidents lists (events), Weather page (weather events), Track Map (TRACK block),
Session Overview classification (event), Chassis Research (full player chassis, research group if Q2 says so), Engine page.

Done when: tests B4-1 to B4-3 pass. B4-1 each page on the synthetic file shows data or a "Not recorded" text, never a blank panel. B4-2 event lists paged. B4-3 pre-B1 files show the reduced-review text.
Owner live B4: L-R3 incidents and pits in REVIEW equal the LIVE lists for the same session.

### B5: hardening
Scope: 6 h session memory and time; fuzz on the new block types (AC-9 harness, 1,000,000 mutations); soak 2 h x 22 cars; M3
Host allowlist on review routes (earlier spec S-R3, `dcam_host_ok` agreed with the TV Director owner); LAN review only if Q6 says
so; research toggle behaviour (Q2).

---

Done when: tests B5-1 to B5-4 pass. B5-1 fuzz of 1,000,000 mutations on the new block types: no crash, reason codes only. B5-2 soak of 2 h with 22 cars: index under 3 s, memory under 8 MB. B5-3 6-hour memory run. B5-4 M3 Host matrix on the review routes.
Owner live B5: L-R4 a 2 h race reviewed while live runs in a second window; live rate stays at or above 19 Hz (`/health`).

## 6. Risks, sizes and budgets

### 6.1 Sizes (ESTIMATE; to measure in B1)
Basis: 20 Hz, 22 cars present for the whole race, 2 h = 7,200 s. Working format (XPRESS, about 1.6 to 2.2x), as in
`STORAGE_MANAGER_SPEC` 5.

| Part | Per car-min | 22 cars, 2 h | Notes |
|---|---|---|---|
| Current v2 channels (T17-T18, ids 1-18, 20-23 player) | 20-34 KB | 53-88 MB | `STORAGE_MANAGER_SPEC` 5 (1.2-2.0 MB per car-hour) |
| Player chassis, 40 values (T16, existing) | (player only) | +7 to +9 MB per player hour | `STORAGE_MANAGER_SPEC` 5 |
| `sim_ms` (T1), DATA-level | shared | about 0.1 MB | one value per tick, constant step |
| Added player chassis, 39 values (T16) | player only | +2 to +3 MB | ESTIMATE, B1 measures |
| Added per-car CHG and slow channels (T9-T15, T19-T29, T32) | 1 to 3 KB | 2.6 to 7.9 MB | CHG keeps this small |
| Added per-car 20 Hz channels (T13, T21, T22, validity, flags) | 3 to 6 KB | 8 to 16 MB | Biggest new cost. Option: CHG for validity |
| EVENT blocks (laps about 1,300 x 300 B, incidents about 500 x 200 B, pits, weather 720 x 40 B) | - | about 0.6 MB | ESTIMATE |
| TRACK block (once) | - | <= 24 KB | 3,000 points x 8 B |
| RESEARCH group, all cars (T35, only if Q2 says all cars) | 28 KB | about 40 to 50 MB | Large: recommend trace cars and player only (about 1 MB) |

Total, recommended set (no research for all cars): about 70 to 105 MB working. About 3 to 8 MB of it is new.
Owner threshold: the 1 GiB warning is reached after about 3 to 6 full weekends (`STORAGE_MANAGER_SPEC` 5, warn only).

### 6.2 Memory (ESTIMATE)
- Block index for 2 h: about 230 KB. Lap index: about 104 KB. Event index: about 64 KB. Total index: well under 1 MB.
- Decode cache: at most 2 blocks (at most 2 MiB). Frames in flight: at most 240 frames (at most 1 MiB).
- Cap 8 MB per session including everything. Measured in B2 and B5.

### 6.3 Risks
| Risk | Effect | Mitigation |
|---|---|---|
| R1 Paused game writes repeated ticks | Lap times and timeline wrong if wall clock is used | Sim time per tick (T1); lap times from events (T23); B1 pause test |
| R2 New per-tick channels raise writer cost | Capture cost p95 above 500 us | Copy-only capture; CHG where possible; measure in B1 (AR-3) |
| R3 Viewer assumes a monotonic live feed | Stale histories after a seek, wrong incident and pace lists | Seek reset (4); regression tests in `viewer_test.js` (T9) |
| R4 HTTP thread serial | GET `/telemetry` and `/health` stalled by review | Review worker, 503 on busy (R-V1); SSE not affected |
| R5 Reading the `.part` while written | Torn read | Committed block count snapshot; stress test B2 (INFERENCE on share mode) |
| R6 Two session boundaries | LIVE history cut at other places than the file | Q1: one rule |
| R7 Format bump | Old readers reject the file | 1.1.0 is not shipped; owner test recordings must still read (T-B1-5); required-flag rule |
| R8 CO-OP remote cars (controller mode 2) lose simulated channels | Holes in the data, shown as "remote" | Validity CHG (T19) carries the reason; page shows it |
| R9 Cars off track (`on_track`) | No sample for that car at that tick | Presence per block (`count`, `first_tick`, `storage_v2`) already encodes it; page says "not on track" |
| R10 Research channels size | Recording grows by 40 to 50 MB per 2 h | Q2 default: trace cars and player only |
| R11 Track geometry changes between sessions | Map of the wrong track | TRACK snapshot per session (Q5 default) |
| R12 Pit and out laps not removed when flags are missing | Lap times inflated | Validity and pit channels (T19, T25); shown with a mark |
| R13 TV Director edits to `web_telemetry.c` and `telemetry_viewer.js` | Merge conflicts | B1 starts after their merge (earlier spec R-9) |
| R14 Security: review routes readable from LAN when `Remote=1` | Data exposure | Local peer only until M3 (S-R2, earlier spec; Q6) |

---

## 7. Open questions for the owner (7), with a recommendation each

- **Q1 One session boundary for LIVE history and review.** Recommend: the writer's session (type change, sim drop, results,
  30 s stale). Replace the broker-change clear in `session_history_clear` for history purposes, keep it for the live
  incident and lap rings. Impact: LIVE "earlier in session" matches the recording exactly.
- **Q2 Research channels (T35).** Recommend: trace cars and player always (about 1 MB per 2 h); all cars only when the research
  switch is on (default off, about 40 to 50 MB per 2 h). Owner decides the switch behaviour.
- **Q3 Recordings made before B1.** Recommend: reduced review (1 s resolution, no events, lap list only with lap length), with the
  banner in section 4. Alternative: hide them from REVIEW.
- **Q4 Format.** Recommend: header minor version 2.1 with `has_sim`, required-flag rule, CHG list inside DATA blocks. Alternative: a
  separate CHG block type (type 7).
- **Q5 Track geometry and playback rates.** Recommend: TRACK snapshot per session (<= 24 KB); playback frames at 4 Hz for all cars,
  20 Hz only for the traces of selected cars.
- **Q6 Lap times and pit laps.** Recommend: game lap time from the lap event when present, sim-time derived only when missing (marked);
  pit and out laps shown with a mark, not removed (owner decision 18:00 kept). LAN review stays local-only until M3.
- **Q7 Scope of "all data telemetry can log".** Recommend: include incidents, pits, classification, flags, weather, damage and
  limits (all in T-rows above). Health and diagnostics (T39) are not telemetry data and are not stored. TV Director cuts (T40) are
  not stored in B1; store them as EVENT `camera` in B4 if the owner wants the broadcast choice replayed.

---

## 8. Corrections and supersessions of `RECORDING_REVIEW_SPEC_2026-10-13.md`

- **CORRECTION, AR-1 (lap rule):** "lap time equals the `tick_ms` difference" is wrong across a pause and is not the game's lap
  time. Use game lap events (T23) and sim-time crossings. The owner live check (lap times within 0.1 s of the game) stays.
- **CORRECTION, F-V2-1 (gvert):** the earlier spec says gvert is "player only in practice" (`storage_writer.inc:219`). The code writes
  gvert for any car with validity bit 32 (`storage_writer.inc:218`). The earlier 4.7 proposal (AI gvert excluded) and the code
  disagree. B1 decides; recommend keeping the code behaviour, since AI validity is already recorded.
- **CORRECTION, F-THR-1 (threads):** the earlier spec says a slow review request blocks live `/telemetry`. That is true for GET
  `/telemetry` and `/health`. SSE `/events` is pushed by the capture thread (`compatibility_runtime.inc:258`) and is not blocked.
- **SUPERSEDED, Q1 phase split ("laps first, playback later"):** replaced by the owner product model (REVIEW is the full timeline).
  R1 and R2 of the earlier spec remain valid as the lap base of B2; the timeline (earlier R3) moves to B3 and B4 here.
- **FACT missing from the earlier spec:** the 1.0.1 archive and the race log are off in 1.1.0 (section 0), so no event is written today.
- **FACT missing from the earlier spec:** per-tick sim time is not stored (section 1, T1). Its F-V2-3 and F-V2-4 lists are otherwise right.
- **Batch 4 META gap:** the 4.3 promise (drivers, pits, incidents, laps in META) was not kept in batch 4 (D-9). These are B1 items here.
- **Unchanged:** the opaque `v2_` review id (earlier Q2), the Load behaviour (selection only), the read-only rule (AR-2), the
  local-peer rule (S-R2) and the capture isolation rule (AR-3).

---

## 9. Facts, inferences, estimates and open items; documentation gate

- **FACT:** sections 1, 2.1, 2.2, 3.5, 3.6 cite file:line. Key facts: the 1.0.1 archive is off (`session_archive.inc:176-182`,
  `runtime_test.c:1957` only); the writer is wall-clock keyed (`storage_writer.inc:267`); sim time is per block
  (`storage_writer.inc:706`); META has no drivers, pits, incidents or laps (`storage_writer.inc:758-771`); flags are not written
  (`storage_writer.inc:238-239`); 40 of about 79 chassis values are written (`storage_writer.inc:243-258`); track geometry is live
  only (`web_telemetry.c:2640-2655`).
- **INFERENCE (verify):** paused game writes repeated ticks (B1 pause test); the `.part` can be read by another thread (B2 stress
  test); `px/pz` equal `tx/tz` or not (T34); a review worker can answer within the budgets (B2 measure).
- **ESTIMATE (measure):** all sizes in 6.1; index times and memory (6.2); seek p95; capture cost with new channels.
- **OPEN:** Q1 to Q7; OQ-5 (chassis value ranges, `STORAGE_MANAGER_SPEC` 4.5); M3 Host allowlist; the TV Director merge.

### 9.1 Documentation gate
- This spec is in the component folder. It is indexed by one line in `CURRENT_STATE.md` (newest entry). Before-state:
  `CURRENT_STATE.md.BEFORE_20261013_livereview` (copied with `cp -p`, identical at backup time).
- No code, build, gate, live run, install or PRODUCT_STATUS change. Stage for Telemetry stays as in
  `governance/PRODUCT_STATUS.json` (1.0.1 shipped, stage observe). 1.1.0 is in specify (earlier spec 14.1).
- Workspace validator (`tools/workspace/check-workspace-structure.ps1`) ran at the start: 2 pre-existing errors (top-level
  `.claude` and `plans`). Not changed here.
- `f1c_ida_decompiled/findings.jsonl`: nothing appended. This is a specification with no game-behaviour finding; the
  storage findings already exist as batch 5 ids.
- Pending validation: owner Q1 to Q7; the TV Director merge; B1 to B5 gates; the owner live list per batch (B1: pause test,
  sim per tick in an owner session; B2: lap times within 0.1 s for two laps; B3: seek and playback; B4: pits and incidents
  equal to the LIVE lists; B5: 2 h x 22 cars soak).

## 10. Specify exit evidence (governance/PIPELINE.md) and wasted-effort check

### 10.1 Affected paths (by batch; no edit made by this spec)
- B1: `storage_v2.inc` (catalog ids 64-127, DATA `sim_ms`, block types 5-7, reader flags), `storage_writer.inc` (`sw_car_from`
  `:201-262`, `sw_meta_format` `:758-771`, sample copy `:265-275`, TRACK block), `web_telemetry.c` (the `sv2` channel
  sources only), `schemas/` (new `f1c.telemetry.meta/3` and event schemas), `API_CONTRACT_2026-10-09.md` section 17 (doc),
  `runtime_test.c` and `test_telemetry.py` (tests), `pin_sidecars.py` run after any sidecar change.
- B2: `storage_api.inc` (`GET /recordings/review`, `/recordings/laps`, id resolver), `storage_jobs.inc` (index job, review
  worker), `web_telemetry.c` (routes at `:8711-8765`, `session=` branch `:8865`), `storage_v2.inc` (block iterator).
- B3-B4: `telemetry_viewer.js` (clock wrapper, seek reset, banner, timeline bar, not-recorded text), `viewer_test.js`.
- B5: `web_telemetry.c` (Host check on review routes, shared helper `dcam_host_ok` with the TV Director owner).
- Not touched by any batch: `F1 Challenge V10` and the clean test install; `production\`; the shipped 1.0.1 payload.
- Coordination: `director_state.inc`, `telemetry_viewer.js` and `web_telemetry.c` are being edited by the TV Director work.
  B1 starts only after that merge (section 5).

### 10.2 Preservation and restoration
- Before any batch edits a file, copy it to `<name>.BEFORE_20261013_<batch>` in the same folder (the pattern used by batches
  1 to 6). Those copies are kept (retention policy: source and records are kept).
- Build output goes to `%TEMP%\f1c-web-telemetry-1.1.0-<batch>\payload` only (never into the workspace).
- Restoration of a batch: copy the `.BEFORE_` file back over the source, re-run the gate, re-pin sidecars. The 1.0.1
  payload in `production\mods\web_telemetry\v1.0.1` is untouched by every batch.

### 10.3 Abort conditions (stop the batch and report to the owner)
- A1 Capture cost p95 above 500 us after B1 with the new channels: revert the new channels in that batch, keep the spec.
- A2 Any review code path takes `g_telemetry_cs`, or any file under `Telemetry\` changes hash during a review pass (AR-2).
- A3 An older v2 file (batch 4 or 5 recording) fails to read (B1-5).
- A4 Measured size above 1.5 x the section 6.1 estimate for the recommended set: stop and re-plan the CHG and 20 Hz split.
- A5 Owner answers Q1 or Q4 differently from the recommendation: re-plan before B1.
- A6 A pause test (B1) shows the writer does not write paused ticks: remove the INFERENCE in 3.6 and keep the sim-time channel
  (it is still needed for lap times and the review clock).

### 10.4 Wasted-effort check (this task, required by PIPELINE.md)
- Repeated reading of large material: yes. `json_telemetry`, the writer and the spec were read in more than one pass, and the
  line numbers were corrected in one verification pass (about 20 citations were wrong on the first draft and were fixed).
- Unnecessarily broad builds or tests: none (no build, no gate run).
- Failed or retried commands: three. (a) the validator path lost its backslashes in Bash, rerun with forward slashes;
  (b) `python3` is not on the bash PATH, rerun with the workspace interpreter path; (c) one replacement count was 2, not 3,
  rerun after correction. A better route would have been to grep all citations once before drafting.
- Polling or idle waiting: none.

## 10. Claude UI review (2026-10-13) - binding for B2-B4
Owner bar: a layman understands every screen; it still feels like a real F1 race engineer's tool.
- Mode banner: big, colour-coded: green `LIVE` / amber `REVIEW` + plain sub-line ("Watching a saved session: <name>").
  `Back to LIVE` is a button, not a link.
- Timeline like a broadcast replay bar: play/pause, 0.5x/1x/2x/4x/8x, scrub with a lap ruler (L1, L2 ...) and event
  icons (pit = tyre icon, incident = warning triangle, flag = flag colour, fastest lap = purple dot). Hover = one-line
  plain text ("Lap 12: Schumacher pits, softs -> hards").
- Engineer language WITH plain help: keep terms (delta, sector, stint, compound, gap, interval) but every header has a
  "?" tooltip in one plain sentence. Units on every number (s, km/h, °C, %, L). Timing colours: purple = overall best,
  green = personal best, yellow = slower.
- Names, never ids: driver + team colour chip everywhere; car number optional. No internal ids, file names or "_2"
  suffixes in the UI.
- Empty states say what to do: "Not recorded in this recording" -> add why/when: "Recorded from version 1.1.0 B1 on."
- Pre-B1 banner in plain words: "Older recording: less detail (1 sample per second, no pit/incident events)."
- Safe defaults and in-page dialogs only (batch 6 pattern); destructive actions name the Windows Recycle Bin.
Claude reviews each batch's screens live (browser, real data) before owner install.

## 11. Owner answers (2026-10-13)
- Q1: yes - every session (P1..P4, Q, Warmup, Race, Test Day) is separate and individual; LIVE history and the saved recording use the same session boundary.
- Q2: research channels for ALL cars, always ("after the race you realize you want it and it was not recorded").
  Accept the size cost (+40-50 MB per 2 h x 22 cars, estimate); B5 must measure it.
- Q3: pre-B1 recordings are not needed: NO reduced-review support for them; the owner removes them himself (Recordings
  page -> Windows Recycle Bin). Agents do not delete them. The reader may refuse pre-2.1 files with a plain message.
- Q4: format 2.1 with has_sim: yes. Q5: track snapshot per session, 4 Hz all / 20 Hz selected: yes.
- Q6: game lap times, pit laps marked, review local-only until M3: yes.
- Q7: EVERYTHING telemetry can log is saved (incl. health/diagnostic data and TV Director cuts when they exist).
