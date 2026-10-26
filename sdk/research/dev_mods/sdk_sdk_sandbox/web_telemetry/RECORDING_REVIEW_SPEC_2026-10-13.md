# Telemetry 1.1.0: review of a loaded recording in the existing pages (SPEC, DRAFT r1)

Date: 2026-10-13. Product: Telemetry (`web_telemetry`), candidate 1.1.0 (API 9, not installed).
Pipeline stage: **specify**. This document is a spec only. No code, no build, no gate, no live probe.
Owner request: after **Load** on the Recordings page the owner must be able to watch the loaded
recording in the existing telemetry pages. Today Load only selects (batch 4 record D-12, O-7).

Coordination: another agent is editing `web_telemetry.c` and `telemetry_viewer.js` (TV Director phase 0,
`TV_DIRECTOR_PHASE0_RECORD_2026-10-13.md`). This spec changes neither file. Implementation must start
after that work is merged, or be rebased onto it (section 10).

Tags: `FACT file:line` = read in the source named (line checked 2026-10-13). `INFERENCE` = reasoned, to
verify. `ESTIMATE` = arithmetic from stated assumptions. `OPEN` = needs an owner decision or a test.
`DECISION` = owner-approved (`STORAGE_MANAGER_SPEC_2026-10-13.md` section 0.2). `PROPOSAL` = this spec.

---

## 0. Summary (read first)

- Smallest path: **server-side adapter**. The browser keeps its 1.0.1 review mode (`?session=<id>` on the
  same endpoints). The DLL decodes the v2 file and answers with the same JSON shapes the pages already use.
  No browser decoding (NG-9). No new viewer page.
- Phase 1 is **lap-based review** (what 1.0.1 review already is: laps, traces, A/B compare, dynamics).
  It is not a frame-by-frame replay. A timeline scrub is phase 2.
- Two facts shape the design. (a) v2 files do not store lap numbers, lap times, lap length, driver names,
  pits, incidents or weather. Lap times must be derived from the lap distance channel, and `lap_len` must be
  added to META. (b) v2 session stems contain spaces and can be longer than the 47-character id the server and
  viewer accept, so v2 review uses an opaque id `v2_<16 hex>`.
- Three batches (section 10): R1 server read side, R2 viewer wiring, R3 phase 2 (driver names, events,
  timeline, M3 Host allowlist).

---

## 1. Objective, non-goals, acceptance

### 1.1 Objective
Make a loaded or selected closed v2 recording reviewable in the existing console pages, read-only, with a
clear REVIEW state and a one-click return to LIVE.

### 1.2 Non-goals (phase 1)
- NG-R1 No change to capture, the writer ring, the capture rate (about 20 Hz) or any game memory write.
- NG-R2 No file write, rename, move or delete on the review path (review is read-only; `Load` stays a selection).
- NG-R3 No browser-side decompression (NG-9). No new viewer page or new chart library.
- NG-R4 No open (still recording) session in phase 1 (OPEN Q3 may change this).
- NG-R5 No 1.0.1 change. The 1.0.1 `?session=` review keeps its behaviour byte for byte.
- NG-R6 No frame-by-frame timeline or scrub in phase 1 (phase 2, section 10).
- NG-R7 No driver names, pit or incident pages, weather or research channels in phase 1 (not stored; phase 2).
- NG-R8 No archive, repair or import changes (O-2 whole-file reads there stay as they are; separate task).

### 1.3 Acceptance criteria (phase 1, measurable)
- AR-1 Lap list: for a synthetic v2 file with known lap times, every timed lap appears once, lap time equals the
  `tick_ms` difference of the two line crossings within 1 ms, and short laps (under 30 samples, `web_telemetry.c:1466`)
  are dropped. Owner live check: lap times equal the game's lap times within 0.1 s for at least one session.
- AR-2 Read-only: SHA-256 of every file under `Telemetry\` is unchanged after a full review session (test + static grep:
  no `WriteFile`, `CreateFile` with write access, `MoveFile`, `SHFileOperation` or `Delete*` in the review code).
- AR-3 Capture isolation: during review of a 1-hour 24-car file, `/health` rate stays at or above 19.0 Hz and
  `capture_cost_us` p95 stays below 500 us (BACKEND4 limit, `STORAGE_MANAGER_SPEC_2026-10-13.md` AC-4).
- AR-4 Memory: review adds at most 8 MB working set at peak for a 1-hour 24-car file (ESTIMATE, to measure);
  no single `ReadFile` reads more than 1 MiB + 40 bytes; no response exceeds 2 MiB (`TRACE_JSON_BYTES`, `web_telemetry.c:3229`).
- AR-5 Responsiveness: `/telemetry` (live) answers within 50 ms while a review index is building (test with a slow
  read hook). Index for a 1-hour file is ready within 3 s (ESTIMATE, to measure).
- AR-6 Refusals: review of an open or unknown session returns 409 or 404 with the text in section 8.4. Ids with
  separators, `..` or spaces never reach the file system (they fail the opaque-id lookup).
- AR-7 Corrupt input: truncated or mutated v2 file gives a JSON error with the reader reason code (`SV2_R_*`,
  `storage_v2.inc:42-48`) and no crash (fuzz, section 9).
- AR-8 Legacy: the 1.0.1 `?session=<1.0.1 id>` answers are unchanged (existing `test_telemetry.py` review cases stay green).
- AR-9 UI: in review the live pill reads `REVIEW`, the banner reads `REVIEW: <name>` with a `Back to LIVE` link,
  and `/` returns to LIVE with the live feed running again.

---

## 2. Facts (verified in source, 2026-10-13)

### 2.1 What Load does today
- F-LOAD-1 `POST /recordings/load` only stores the folder name in `sj_loaded` (`storage_api.inc:529`,
  `storage_jobs.inc:592`). It changes no file. Empty name unloads (`storage_api.inc:509-512`).
- F-LOAD-2 Viewer: `loadRecording` posts and shows a toast (`telemetry_viewer.js:4759-4763`); click handler at `:4848`.
  Nothing opens a page. Batch 4 record D-12 and O-7 say the same.
- F-LOAD-3 Spec 7.6 step 3 promised viewer pages reading `/recordings/*`. Spec 8.5 and the 8.1 read routes
  (`/recordings/data`, `/laps`, `/pits`, `/incidents`, `/trace`) are not built (API contract section 16.1, "not built").

### 2.2 Existing review mode (1.0.1) that v2 can reuse
- F-REV-1 Browser: `this.review` is set from the page URL (`telemetry_viewer.js:2930`; the `/` page script reads
  `?session=` at `web_telemetry.c:8213`). The live feed is not connected in review.
- F-REV-2 Browser: `reviewUrl()` appends `session=<id>` to every console `fetchJson` except `/sessions*` and `/recordings*`
  (`telemetry_viewer.js:3436-3438`, `:3897-3900`).
- F-REV-3 Server: any non-POST request with `session=` is checked first: `ar_id_valid` and `ar_session_exists`
  (1.0.1 folder only) else 404 `unknown or deleted session` (`web_telemetry.c:8864-8868`). A v2 id therefore gets 404 today.
  INFERENCE for implementation: any new console GET called through `fetchJson` will also get `session=` and 404
  unless it is excluded or it is a review route.
- F-REV-4 Server review dispatcher `http_archive_route` (`web_telemetry.c:8714-8765`) answers: `/telemetry` (frame.json),
  `/weather`, `/track`, `/laps`, `/pits`, `/incidents`, `/trace/status`, `/trace/select` (status, no stored selection),
  `/driver?id=`, `/trace/data?car=1000+slot&lap=N`. `/trace/open`, `/trace/compare`, `/trace/export` answer 404 (`:8763`).
- F-REV-5 1.0.1 review is a **snapshot**, not a timeline: `frame.json` is the latest timing frame written every 30 s
  and at close (`session_archive.inc:11`, `ar_tick_locked` `:300-310`).
- F-REV-6 Console review banner: `REVIEW` word and `Saved session <title>, back to live` link (`telemetry_viewer.js:3550-3556`).
  Review title is `reviewTitle(id)` (`:3898-3905`, for 1.0.1 ids only).
- F-REV-7 Existing 1.0.1 JSON shapes (to reproduce for v2):
  - `/laps` = `{"gen","archive","cars":[{"id","p","n","laps":[t,...]}]}` (`session_archive.inc:691-713`).
  - `/trace/status` = `{"gen","archive","lap_len","laps_max","samples_max","mode":"archive","cars":[{"rev","which","slot","used","auto","n","cn","ai","remote","team","laps":[{"lap","t","samples","flags","start","lid"}],...}]}` (`session_archive.inc:869-912`).
  - `/trace/data` = `{"schema":"f1c.trace/1","source":"archive",...,"lap","lap_time","start_sim","lap_len","count","flags","samples":[...]}` (`session_archive.inc:932-963`).

### 2.3 What a v2 file holds (and what it does not)
- F-V2-1 Per tick and per car the writer stores channels by id (`storage_v2.inc:164-232`; writer `storage_writer.inc:201-262`).
  Relevant for review: `tick_ms` (0), `dist` (1) = `c->lap_dist` (`storage_writer.inc:208`), `speed` 2, `throttle` 3,
  `brake` 4, `steer` 5 (AI only), `glat` 6, `glon` 7, `gvert` 8 (player only in practice, `storage_writer.inc:219`),
  `yaw` 9, `yaw_rate` 10, `tread_*` 11-14, `rpm` 15, `gear` 16, `tx` 17, `tz` 18, `ttemp_*` 20-23 (player),
  `pch_*` 24-63 (player, RAW32).
- F-V2-2 Not stored: flags (`storage_writer.inc:238`, comment: TRF exists only for the two trace cars), lap number,
  lap time, lap length, driver and car names, team, position, pits, incidents, weather, track document.
- F-V2-3 Header (192 bytes, `STORAGE_MANAGER_SPEC_2026-10-13.md` 4.1): track (64 B), session_name (60 B), session_type,
  start time, flags, channel table offset, block sizes. **No `lap_len`.**
- F-V2-4 META subset (batch 4 D-9, `STORAGE_V2_BATCH4_RECORD_2026-10-13.md:125-128`): session name, weekend, track
  (sanitized and raw), season, session type and name, time zone, ticks, data blocks, dropped ticks, complete, final.
  Drivers, pit, incident and lap rows are **not** written. `lap_len` is not in META either.
- F-V2-5 Block sizes: `block_ticks_max` 20, `block_ms_max` 1000 (header fields at 24 and 28; spec 4.1). 1 hour at 20 Hz
  is 72,000 ticks, about 3,600 DATA blocks (`storage_v2.inc:13`, `:11` for channel and car limits).
- F-V2-6 The reader is whole-file: `sv2_scan(buf, len, ...)` (`storage_v2.inc:706`) and `sj_read_file` reads the whole file
  (`storage_jobs.inc:90`). Batch 4 D-11 / O-2 state the same. Review needs a block iterator (section 5.4).
- F-V2-7 Stem format (batch 5 decision A): `COUNTRY YEAR - Mode - Session - YYYY-MM-DD HH-MM`, for example
  `BRAZIL 1995 - Championship - Race - 2026-10-13 16-14` (spaces, about 55 characters) (`storage_v2.inc:986-1000`,
  batch 5 record `:52-66`). Session ids allowed by the server: `ar_id_valid` (no spaces, max 47; `session_archive.inc:135-143`,
  `AR_ID_LEN` 48 at `:33`); the viewer regex `[A-Za-z0-9_-]{8,47}` (`telemetry_viewer.js:2930`, `web_telemetry.c:8213`).
  **Conflict: v2 stems cannot be review ids as they are.** Filesystem names may still contain spaces (`sa_name_ok`,
  `storage_api.inc:441-448`, allows up to 119 characters).

### 2.4 Lap and trace facts from the 1.0.1 live builder (basis for the v2 derivation)
- F-LAP-1 `lap_dist` is metres along the lap, corrected for lap number (`web_telemetry.c:888`, `:928`).
- F-LAP-2 Thresholds: `TRACE_SIM_GAP_S` 1.0 s (`:1463`), `TRACE_TELEPORT_M` 50 m (`:1465`), `TRACE_REVERSE_M` 2 m,
  `TRACE_MIN_LAP_SAMPLES` 30 (`:1466`), `TRACE_SAMPLES` 3200 (`:1460`). Line crossing by `dist` wrap (`:1855-1866`).
- F-LAP-3 Validity in 1.0.1 uses `TRF_*` flags per sample (`:1475-1485`). v2 has no flags, so pit and out laps cannot be
  rejected the 1.0.1 way (F-V2-2). OPEN Q7.
- F-LAP-4 `g_telemetry.lap_len` is known from track load (`web_telemetry.c:1831`, `:1855`, `session_archive.inc:306`).
  INFERENCE: available at session start to the writer (to verify in R1).

### 2.5 Threads, sizes and the HTTP model
- F-THR-1 HTTP is one accept loop: `while (g_http_running) { accept; http_handle(client); }` (`web_telemetry.c:9347-9353`).
  Requests are served one at a time on `http_thread`. A slow review request blocks live `/telemetry` for everyone.
- F-THR-2 Response buffer: `g_http_big` 2 MiB static (`web_telemetry.c:3229`, `:8000`).
- F-THR-3 Writer ring 48 slots (`storage_writer.inc:16`); jobs thread is low priority (`storage_jobs.inc`, batch 3 record).
- F-THR-4 Lock order `g_telemetry_cs` -> `g_storage_cs` (`STORAGE_MANAGER_SPEC` 6.1). Review must not take `g_telemetry_cs`.
  GET routes are readable from LAN and Tailscale; writes stay local.
- F-SEC-2 Host check: only `/director/state` checks Host (`dcam_host_ok`, `director_state.inc:448-460`; used at
  `web_telemetry.c:8955`, added by the TV Director phase 0 work). All other routes have no Host check (SECURITY_REVIEW
  F-03; M3 in `SECURITY_REVIEW_2026-10-13.md:42`).
- F-SEC-3 Write guards on recordings POST (`web_telemetry.c:8836-8858`): peer local, `X-Telemetry-Action`, JSON, 4 KB.

---

## 3. Options considered

| Option | What | Verdict |
|---|---|---|
| A. Server adapter (recommended) | Reuse `?session=` review. New v2 branch in the review dispatcher answers the 1.0.1 shapes from the v2 file, through a lap index built on the jobs thread. | Smallest change. Viewer code stays the same except banner and ladder. |
| B. New `/recordings/*` read routes and a new viewer | Spec 8.1 route family (`/recordings/laps`, `/trace`, `/data`) with new shapes and a new page. | More code and two parallel review paths. Rejected for phase 1; `/recordings/data` stays for phase 2 scrub. |
| C. Browser decodes `.f1t` | Client-side XPRESS/varint decode. | Rejected: NG-9 (no browser decompression); XPRESS is not available in the browser. |

---

## 4. Phase 1 scope

### 4.1 Pages (existing console pages, `telemetry_viewer.js:2285-2287`)

| Page (id, label) | Phase 1 in review | Uses | Note |
|---|---|---|---|
| `overview` Lap Analysis | WORKS | `/laps`, `/trace/status`, `/trace/data` | Lap table and per-lap traces. The main review page. |
| `compare` Driver Compare (A vs B) | WORKS | `/trace/select`, `/trace/data` | Selection is client-side in review (as 1.0.1). Slots show as numbers until R3. |
| `dynamics` Vehicle Dynamics | WORKS (partial) | `/trace/data` (speed, glat, glon, yaw, rpm) | Needs the channel subset in 5.6. |
| `braking` Braking and Corners | WORKS (partial) | `/trace/data` (speed, brake, dist) | As above. |
| `tyres` Tyres | PARTIAL | `/trace/data` (tread, player ttemp) | No carcass temperature or pressure. Shows "not recorded" for the rest. |
| `timing` Timing and Race Pace | PARTIAL | `/laps` | Lap times only; no positions. |
| `race` Session Overview | PARTIAL | `/telemetry` (synthesized, section 5.5) | Track, session type, lap length. No classification. |
| `focus` Driver Focus | NOT IN PHASE 1 | `/driver` | Needs names (R3). Show "not recorded in 1.1.0 recordings". |
| `live` Live Gauges | HIDDEN in review | live only | Already live-only in 1.0.1 terms. |
| `research` Chassis Research | NOT IN PHASE 1 | `/trace/data` pch | RESEARCH channels are not stored (NG-8); pch is R3. |
| Pits, incidents, weather, track | NOT IN PHASE 1 | `/pits`, `/incidents`, `/weather`, `/track` | Answer a stated "not recorded" document (5.3). |

### 4.2 Why these pages
Lap times, lap traces and A/B compare are the core review of a session. They need only lap distance, speed,
throttle, brake, steer and time, which are stored (F-V2-1). Names, pits and incidents need new writer data (R3).

---

## 5. Derivation and API shapes

### 5.1 Opaque review id
- PROPOSAL: `review_id = "v2_" + lowercase hex of FNV-1a 64 of the folder + "/" + session stem` (16 hex), built by the
  writer when it publishes the snapshot (`sa_snapshot_rebuild`, `storage_api.inc:187`). Example `v2_9f3c1ad07b2e4c18`.
  19 characters, matches `[A-Za-z0-9_-]{8,47}` (viewer `:2930`, server `:8213`). No change to either regex.
- The server resolves `session=` only through the published snapshot (`sa_pub`, under `sa_cs`), never by building a path
  from the request. An id not in the snapshot is 404.
- 1.0.1 ids (`20261012-213454_s5_g12`) keep their branch. A `v2_` prefix routes to the v2 adapter.
- `GET /recordings` gains `review_id` per session (additive key, API 10). The Recordings page uses it for "Review".

### 5.2 Lap index (built once per session, on the jobs thread)
- Input: the v2 file, read block by block (5.4). Output per car slot: list of laps `{lap, t, start_ms, end_ms, samples, first_block, last_block}`.
- Line crossing: sample k is a crossing if `dist[k-1] >= lap_len - 50` and `dist[k] <= 50` (1.0.1 teleport band,
  `TRACE_TELEPORT_M` 50, `web_telemetry.c:1465`). Lap time = `tick_ms[k] - tick_ms[k_prev_cross]`.
- Segment breaks (not laps): a `tick_ms` gap larger than 1.0 s (`TRACE_SIM_GAP_S`), a backwards `dist` step larger
  than 2 m that is not a crossing, a GAP block, and a missing channel for that car.
- Laps shorter than 30 samples are dropped (`TRACE_MIN_LAP_SAMPLES`). The first partial lap (session start to first
  crossing) is not timed.
- `lap_len`: read from META (section 5.7). If META has none (files written before R1), the review shows
  `lap_len: unknown` and the lap list is withheld with the text in 8.4. No guessing from data in phase 1 (OPEN Q4).
- Validity: v2 has no flags, so every timed lap has `flags: null` and a note `not validated (no per-sample flags)`.
  Pit and out laps are **not** removed (OPEN Q7).

### 5.3 Endpoints in review (`?session=<review_id>`, existing routes, phase 1)

| Route | Phase 1 answer | Shape |
|---|---|---|
| `GET /laps` | Built from the lap index | 1.0.1 shape (F-REV-7) plus additive `"source":"recording"`, `"lap_len"`, `"validated":false` |
| `GET /trace/status` (and `/trace/select`) | Built from the lap index | 1.0.1 shape, `"mode":"archive"` kept (viewer compatibility, to verify), `"source":"recording"` |
| `GET /trace/data?car=1000+slot&lap=N` | Samples of one lap, decoded from the lap's blocks | `f1c.trace/1` with `"source":"recording"`, samples `{t, dist, speed, throttle, brake, steer, glat, glon, yaw, rpm, gear, tread_fl..rr}` (keys absent when not stored) |
| `GET /telemetry` | One synthesized frame (5.5) | Live frame shape, sentinels per API contract section 3 |
| `GET /weather`, `/track`, `/pits`, `/incidents`, `/driver` | HTTP 404 with `{"error":"not recorded in 1.1.0 recordings","phase":"R3"}` | error JSON |
| `POST` anything | 405 | - |
| `GET /trace/open`, `/trace/compare`, `/trace/export` | 404 as in 1.0.1 | error JSON |
| `GET /recordings/review?session=<review_id>` (NEW, read) | State of the index | `f1c.recordings.review/1`: `{"schema","session":"<review_id>","state":"none|building|ready|error","laps":n,"cars":n,"lap_len":m|null,"reason":"<SV2_R_*|null>","built_ms":n}` |

- `GET /recordings/review` while building: 200 with `state:"building"`; the viewer polls it (no request blocks).
- A request for lap or trace data while the index is `building` returns 503 with `{"error":"review index is building"}` and
  `Retry-After: 1`. It never waits on the jobs thread.
- All responses under 2 MiB. Trace data above 2 MiB is refused with 413 (cannot happen with 3,200 samples per lap, F-LAP-2).

### 5.4 Block iterator (new, R1)
- Read the header and channel table (about 2 KB) with one read. Read the block index on demand: a block header is 40 bytes;
  store `{file_offset, seq, type, first_tick, ticks, stored_len}` (about 24 bytes per block; about 86 KB per 1-hour file).
- Read one block at a time: `ReadFile` of `stored_len` bytes at the offset (at most 1 MiB + 40 B, `SV2_MAX_PAYLOAD`,
  `storage_v2.inc:14`). Decode with the existing `sv2_data_decode` (`storage_v2.inc:531`) and the existing decompression
  step. The scan uses `sv2_scan`'s validation rules (4.10 of the spec) without the whole-file buffer.
- A trace request reads only the blocks of that lap (found through the lap index), not the whole file.
- Reader failures keep their reason codes (`SV2_R_*`). A corrupt block stops the index at the last good block and sets `state:"error"` or
  `"ready"` with `reason`, per spec 4.10 (valid prefix).

### 5.5 Synthesized `/telemetry` (phase 1)
- One frame at the session's last committed tick: `trk` (header track), `ty` (session type), `lapm` (lap length in metres
  from META, else -1), `c` as an empty list, plus `"review":{"session","stem","ticks","end_t"}`.
- Missing values use the API contract sentinels (section 3 of `API_CONTRACT_2026-10-09.md`). INFERENCE: the console
  renders with an empty `c` and shows "no classification"; verify in R2 (OPEN Q8).

### 5.6 Channels per page
Dynamics needs `speed, glat, glon, yaw, rpm, gear`. Braking needs `speed, brake, dist`. Tyres needs `tread_*`
and player `ttemp_*`. Traces always include `dist` and `t`. The adapter returns only the channels the request needs
(`ch=` query, optional; default: all stored channels of that scope).

### 5.7 META addition (R1, writer change)
- Add to META (`f1c.telemetry.meta/2`, additive key, no file-format version change): `"lap_len_m"` (float, set at session
  start from `g_telemetry.lap_len` and rewritten in the final META), and `"laps_count"` (for the list header). Reader ignores unknown keys.
- INFERENCE: `g_telemetry.lap_len` is set before the first session sample. Verify in R1 with the synthetic writer and
  on the live run; if not, use the final META value only and show the lap list after close.

---

## 6. Memory and size limits

| Item | Limit | Basis |
|---|---|---|
| Review index in memory | at most 8 MB total, LRU of 2 sessions | ESTIMATE: 1-hour file: lap rows about 40 KB, block index about 86 KB |
| One block read | 1 MiB + 40 B | `SV2_MAX_PAYLOAD` (`storage_v2.inc:14`) |
| Decode scratch per block | about 0.6 MB worst case (24 cars, 64 channels, 20 ticks) | ESTIMATE |
| One HTTP response | 2 MiB | `TRACE_JSON_BYTES` (`web_telemetry.c:3229`) |
| Trace samples per lap | 3,200 | `TRACE_SAMPLES` (`web_telemetry.c:1460`) |
| Session size | up to about 50 MB per hour (working); 300 MB for a 6-hour weekend | `STORAGE_MANAGER_SPEC` section 5 (ESTIMATE) |
| Whole-file reads on review path | none | AR-4; batch 4 D-11 remains for archive and repair only |

Long sessions: the index is built by reading block headers and DATA payloads once (linear, about 1 MB/s at minimum
on a slow disk: ESTIMATE, measure). Trace reads are seeks to the lap's first block, so a 6-hour file costs the same per lap as a 1-hour one.

---

## 7. Threading

| Work | Thread | Locks | Rule |
|---|---|---|---|
| Capture, ring copy | `telemetry_worker` / game callback | `g_telemetry_cs` | unchanged; no review code |
| Snapshot publish (review_id, sizes) | storage writer | `sa_cs` (short) | unchanged pattern |
| Lap index build (block scan) | `storage_jobs` (low priority) | `rv_cs` for the result only | file I/O here only |
| Review route answers (lap list, trace, telemetry, recordings/review) | `http_thread` | `rv_cs` (copy out) | bounded work: index copy and one lap decode; no scan of the whole file |
| Load (selection) | `http_thread` | `sj_cs` | unchanged |

- Rule R-T1: no review code takes `g_telemetry_cs`. Lock order stays `g_telemetry_cs` -> `g_storage_cs` -> `rv_cs`.
- Rule R-T2: a review request never scans a file. Index build is a job; a request during build gets 503 or `building`.
- Rule R-T3: one lap decode per request (at most about 3,200 samples). Measure p95; if above 150 ms, move trace decode to the jobs thread with polling.
- Rule R-T4: `storage_jobs` calls `CoInitializeEx` only if it uses shell calls (unchanged).

---

## 8. Live and review modes in the UI

### 8.1 States
- LIVE: `/` with no `session=`. Live feed on. Load/Unload work as today.
- REVIEW(id): `/?session=<review_id>`. Live feed off for this page (as 1.0.1). Capture and the writer keep running.
- Switch LIVE -> REVIEW: "Review" button on a closed session in the Recordings page (new, R2), or a session in the
  session ladder (8.2). Both navigate (page load), as 1.0.1 does.
- Switch REVIEW -> LIVE: "Back to LIVE" link (existing anchor `telemetry_viewer.js:3556` points to `/`).
- Load and Unload do not change the view mode. Loading a recording only sets the Recordings selection (as now).

### 8.2 Banner and session ladder
- Live pill word: `REVIEW` (existing `telemetry_viewer.js:3551`), class `review` (`:3552`).
- Banner text, exact: `REVIEW: <display name>` then `Back to LIVE`. Display name = the replay-style stem
  (`COUNTRY YEAR - Mode - Session - date time`), HTML-escaped with `esc()`. Replaces "Saved session" for v2 ids only (1.0.1 keeps its text).
- Session ladder (spec 9.2): tabs for the closed sessions of the loaded recording, in time order. Clicking one navigates to its REVIEW.

### 8.3 Recordings page additions (R2)
- Per closed session row: button `Review` (disabled with the reason text for open sessions and recovered sessions without a closed END).
- Notice text kept (spec 9.4). No new confirm dialogs (review is read-only).

### 8.4 Exact texts (proposal, English, for the owner to approve)
- Open session: `This session is still recording. Review is available after it closes.` (409)
- Unknown id: `This session is no longer on disk. Reload the list.` (404)
- Index building: `Preparing the review. This takes a few seconds for long sessions.` (page shows, polls `/recordings/review`)
- Index error: `This recording could not be read (reason <code>). Nothing was changed.` (`reason` from `SV2_R_*`)
- Lap length unknown: `Lap length is not recorded in this file. Lap times are not shown.`
- Not recorded page: `Not recorded in 1.1.0 recordings yet.`
- Not validated laps note: `Pit and out laps are not removed in recordings (no per-sample flags).`

---

## 9. Security

- S-R1 Read-only review: no POST route, no `X-Telemetry-Action`, no write in the review path (AR-2).
- S-R2 Peer policy: phase 1 review answers **local peers only** (`http_peer_is_local`, `web_telemetry.c:8697`), even when
  `Remote=1`. This is stricter than `/telemetry` and avoids exposing recordings on the LAN before M3. OPEN Q5 (owner may
  allow LAN reads after M3).
- S-R3 Host allowlist: phase 1 uses the same Host check as the existing review routes, which is none. The `dcam_host_ok`
  helper (`director_state.inc:448`) is the M3 candidate; using it here needs the TV Director agent's agreement because
  that file is theirs. Required before any LAN exposure (SECURITY_REVIEW M3, F-03). Not claimed as done.
- S-R4 Id handling: `session=` is looked up in the published snapshot only (5.1). No path is built from the request. No
  separator, `..` or drive letter can reach the file system. Responses contain no absolute path (M11).
- S-R5 Output escaping: all names via `esc_json` (server) and `esc()` (browser).
- S-R6 Bounded work: one request reads at most one lap or one block; response 2 MiB max.
- S-R7 Corrupt files: reason codes only, no internal dumps.

---

## 10. Implementation batches

### R1: server read side (DLL, 2-3 files, offline gate)
Files (after the TV Director work is merged): `storage_v2.inc` (block iterator, index), `storage_jobs.inc` (index job),
`storage_writer.inc` (META lap_len, review_id), `storage_api.inc` (`GET /recordings/review`, review_id in snapshot, id resolver),
`web_telemetry.c` (dispatcher branch for `v2_` ids in the `session=` block at `:8864` and `http_archive_route` `:8714`;
`API_VERSION` "10" at `:56`), `schemas/` (`f1c.recordings.review/1`, version 10), `API_CONTRACT` section 17.
Done when: section 11 offline tests T1-T7 pass; `contract_validate.py` contract 10 passes; existing gates pass.

### R2: viewer wiring (telemetry_viewer.js, offline test + one browser run)
Files: `telemetry_viewer.js` (Review button, v2 banner text, session ladder, not-recorded pages, polling of
`/recordings/review`), `viewer_test.js` (new cases). Done when: viewer_test PASS (current 431 plus new), browser check on the
synthetic feed (`viewer_review/`) of Lap Analysis, Compare and the banner, and `pin_sidecars.py` run.

### R3 (phase 2, separate spec when R1-R2 are accepted)
- Driver names and car names into META (capture-side copy at session start, `g_telemetry.cars[i].name`, as `ar_note_driver` does, `session_archive.inc:170`).
- Driver Focus (`/driver?id=`), pit and incident rows as META events or an event channel.
- Timeline: `GET /telemetry?session=<id>&t=<s>` (frame at time t from the block index) and `GET /recordings/data` (spec 8.5, paged), with a scrub bar.
- Chassis research page from pch channels; track map from `tx`/`tz`.
- M3 Host allowlist on all routes (security batch) and then optional LAN review (OPEN Q5).

---

## 11. Tests

### 11.1 Offline (game closed; host build in `%TEMP%`, `-Wall -Wextra -Werror`; the existing harness `runtime_test.c` / `test_telemetry.py` pattern)
- T1 Lap index on synthetic files with known laps: count, times within 1 ms, short-lap drop, first partial lap not timed.
- T2 Segment edge cases: exact-zero crossing, reverse step of 1 m (not a break) and of 5 m (break), tick gap > 1 s, GAP block
  inside a lap, missing channel for one car, AI car (steer AI only).
- T3 Shapes: `/laps`, `/trace/status`, `/trace/data`, `/recordings/review`, `/telemetry` validate against the new and existing schemas (`contract_validate.py`).
- T4 Read path: instrument the reader; assert no `ReadFile` > 1 MiB+40 B, no whole-file buffer, one lap decode per trace request.
  1-hour synthetic 24-car file (about 72,000 ticks): index ready within the AR-5 limit on the host, peak working set delta at most 8 MB.
- T5 Fuzz: truncation at every block boundary and 1,000,000 mutations of three reference files (reuse AC-9 harness): no crash, reason codes only (AR-7).
- T7 Threading: (a) live `/telemetry` answers within 50 ms while the index builds with a 50 ms-per-block read hook; (b) static check: no review function references `g_telemetry_cs` or capture functions; (c) hash of every file under `Telemetry\` unchanged after a review pass (AR-2).
- T8 Regression: `test_telemetry.py` 1.0.1 review cases, `viewer_test.js` (431 now), `pin_sidecars.py` and payload gate.
- T9 Viewer (R2): `reviewUrl` for `v2_` ids, banner text, Review button state per session state, `not recorded` pages, no polling in review except `/recordings/review`.

### 11.2 Owner live list (after the R1 and R2 gates PASS; game closed to start, owner runs the game)
- L1 Play or use a short closed session, exit the game normally (no `.part` left: batch 5 T3 check).
- L2 Recordings page: Load a recording, click Review on a closed session. Banner reads `REVIEW: <name>`; live pill reads REVIEW.
- L3 Lap Analysis lists the timed laps; lap times match the game's lap times within 0.1 s for two laps.
- L4 Driver Compare: select two slots with laps; traces overlay by distance.
- L5 Vehicle Dynamics and Braking show traces for one lap.
- L6 Back to LIVE: live feed resumes; `/health` rate stays at or above 19.0 Hz during review (owner reads it).
- L7 Open session review refused with the section 8.4 text.
- L8 A 1.0.1 legacy session still reviews with `?session=` (regression).
- L9 Long session (at least 1 hour, 24 cars): lap list appears within a few seconds; per-lap trace within about 1 second; no stall of live `/telemetry` (the live console keeps updating in a second window).
- L10 `%TEMP%` and `Telemetry\` hashes unchanged after review (owner runs the Explorer check or the script).

---

## 12. Risks

| Risk | Effect | Mitigation |
|---|---|---|
| R-1 `lap_len` not available at session start | No lap list | Final META value; list after close; check in R1 |
| R-2 Pit and out laps not removed (no flags) | Lap times inflated for pit laps | Show laps with a note; OPEN Q7 |
| R-3 Lap crossing on AI cars (lapped cars, `lap_dist` corrected) | Wrong lap counts for lapped cars | T2 AI cases; owner check on a lapped car |
| R-4 Review id change breaks the 1.0.1 id rule | Old reviews fail | Separate branch on prefix; AR-8 regression |
| R-5 Console `reviewUrl` adds `session=` to every fetch (F-REV-2) | New GET routes 404 in review | Review route list in server; exclude new non-review GETs |
| R-6 Long disk reads on the HTTP thread | Live feed stalls | Index on jobs thread; R-T2, R-T3 |
| R-7 Writer change to META | Old readers | Additive keys; reader ignores unknown keys; schema version note |
| R-8 File edited while reviewed (delete, archive) | 404 mid-review | Page shows "no longer on disk"; delete of a reviewed session is OPEN Q10 |
| R-9 TV Director edits to `web_telemetry.c` and `telemetry_viewer.js` | Merge conflicts | Start R1 after their merge; agree the `dcam_host_ok` reuse |
| R-10 32-bit memory in the game process | Crash risk | 8 MB cap; measure; fuzz |
| R-11 Name length and spaces | Broken links | Opaque `v2_` ids (5.1) |
| R-12 Viewer shows empty classification | Looks broken | Clear "not recorded" text (8.4); OPEN Q8 |

---

## 13. Open questions for the owner

- Q1 What does "watch" mean for phase 1? PROPOSAL: lap-based review (laps, traces, A/B compare), like 1.0.1 review.
  Frame-by-frame playback is phase 2 (R3). Confirm, or ask for playback first (it adds about one batch).
- Q2 May review ids be opaque (`v2_` + 16 hex) instead of the readable file name? PROPOSAL: yes; the readable name is shown in the banner.
- Q3 Review of an open (still recording) session: refuse (PROPOSAL, phase 1) or read the committed blocks only?
- Q4 OK to add `lap_len_m` to META (additive writer change, R1)? Without it no lap list is shown for files recorded before R1.
- Q5 LAN review: local peers only in phase 1 (PROPOSAL), or allow Remote=1 readers now (needs M3 first)?
- Q6 Pit, incident, weather and driver pages: show "Not recorded in 1.1.0 recordings yet" (PROPOSAL) or hide the tabs?
- Q7 Pit and out laps: show all timed laps with the note (PROPOSAL), or hide laps with a slow-speed run (heuristic, not measured)?
- Q8 Session Overview without classification: accept the empty page in phase 1?
- Q9 Load: keep it as selection only (PROPOSAL, as now), with Review as the separate button? Or should Load open the review too?
- Q10 Deleting a session that is open in a review page: refuse (needs a server-side review lease) or let the page show "no longer on disk" (PROPOSAL)?
- Q11 Phase 2 priority: driver names first (needed for Driver Focus and readable compare) or timeline first?

---

## 14. Facts, inferences and open items (summary)

- FACT: sections 2.1-2.5 (file:line given). Load selects only (F-LOAD-1..3). 1.0.1 review is a snapshot served through `?session=` (F-REV-1..7). v2 holds per-tick channels but no lap, name, pit, incident or lap_len data (F-V2-1..4). HTTP is serial (F-THR-1). v2 stems conflict with the id rules (F-V2-7).
- INFERENCE (verify in R1/R2): `g_telemetry.lap_len` known at session start (F-LAP-4, 5.7); the console renders with an empty classification (5.5); `mode:"archive"` kept for the viewer (5.3); `reviewUrl` side effects on new routes (R-5).
- ESTIMATE (measure): index build time, memory peak, decode scratch, per-lap decode time (sections 6, AR-4, AR-5).
- OPEN: Q1-Q11 (section 13); coordination with the TV Director work (section 10, R-9).

### 14.1 Documentation gate
- This spec exists in the component folder and is indexed by one line in `CURRENT_STATE.md` (newest entry; before-state kept as `CURRENT_STATE.md.BEFORE_20261013_reviewspec`).
- No code, build, gate, live run or install was done. Pending validation: owner decisions Q1-Q11, the TV Director merge, the R1 and R2 gates and the owner live list (11.2).
- Workspace validator (`tools\workspace\check-workspace-structure.ps1`) reported 2 pre-existing errors (top-level `.claude` and `plans`); no structural change made here.
- No `PRODUCT_STATUS.json` change (stage stays as is; this is a spec).
- Findings: no game-behaviour finding; nothing appended to `f1c_ida_decompiled/findings.jsonl`.

## Owner decisions (2026-10-13)
- Q1: lap-based review first (any lap of any car, all recorded channels over the lap, hover values, A/B compare); full playback later.
- Owner scope rule: EVERYTHING the telemetry can log today must be reviewable from a loaded recording. Data not stored in
  v2 recordings yet (pits, incidents, weather, lap numbers/times, names, ...) must be ADDED to the recording format, not
  left as "Not recorded" -> R1/R2 plan must include the writer additions.
- Q3 open sessions: refused until closed. Q4: add lap_len to META (old recordings: no lap list). Q7: pit/out laps shown and
  marked. Q10: delete of a session under review refused. Q5: review local-only until security M3.
- Q2 (opaque review ids): accepted as proposed.

## Owner product model (2026-10-13) - supersedes the "laps first, playback later" split of Q1
Two modes, both first-class:
1. LIVE: what is happening now, plus everything that already happened earlier in the CURRENT session (scroll back laps,
   traces, timing, events of this session while it runs).
2. REVIEW: a loaded recording can be watched retroactively in full - EVERYTHING that happened (timeline playback of the
   whole session: positions, gaps, map, gauges, events, pits, incidents, weather, every car), plus lap analysis/compare.
Rule from 18:00 still holds: all data telemetry can log must be stored in recordings so REVIEW can show it.
