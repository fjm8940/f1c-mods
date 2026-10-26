# Web Telemetry community feedback research C1, C2, C3 (2026-10-14)

Author: Matias. Type: read-only root-cause research. No source, build, game or
runtime change was made. Only this file was written.

Evidence tiers: **FACT** = read in the code or data at the snapshot below.
**DERIVED** = arithmetic or code-path reasoning from FACT items.
**HYPOTHESIS** = plausible, not measured. Each one names the test that settles it.

## 0. Snapshot and caveats

| File | sha256 (at read) | Note |
|---|---|---|
| `telemetry_viewer.js` | `560ff6acfab5eccd7c19304eafd8c1bed04764dd73348f41ea5723c4a7aff96e` (final, mtime 17:51:42, 6506 lines) | **Changed twice during this read** (17:49:04 at 6462 lines, 17:51:42 at 6506 lines). Another writer is active in this component. Viewer line numbers below are for the final hash. Re-verify before editing; cite by function name if the file moves again. |
| `web_telemetry.c` | `4be30d77ba3b7217d8d525243b23f61c105c8fb7ddb061ce4d2e1b329a81479c` | mtime 09:04, unchanged during read |
| `workbench_ui.js` | `d26972d3db89e8676b0017274867819a674f6775407a3cc7d2ff1b17485cd961` | mtime 08:57, unchanged during read |
| `compatibility_runtime.inc`, `session_archive.inc`, `storage_v2.inc` | not hashed | read only |

- The feedback says "Telemetry 1.0.0". `mod.json` is **1.2.0** (`mod.json:8`), and the
  console is **4.4.0** (`telemetry_viewer.js:29`). Lap-time and export code changed after
  1.0.0 (1.0.1 latch note, `web_telemetry.c:1341-1351`). The findings apply to 1.2.0.
- Validator run at cold start: 2 errors, both unrelated and pre-existing (`.claude` and
  `plans` are unexpected top-level directories). Not touched.
- `api1.txt` and private keys were not read.

## 1. Live path map (what the user sees)

- `/` console is the only product UI (`web_telemetry.c:8261-8277`). It mounts
  `F1CViewer.mountConsole` from `telemetry_viewer.js`. `/viewer`, `/workbench` and
  `/classic` are compiled out (`#if 0` at `web_telemetry.c:6079`, `:8176`, `:8233`, `:9251`).
- Live frames: `/events` is SSE. `compatibility_runtime.inc:255-258` calls
  `json_telemetry` and `sse_broadcast` every `SSE_UPDATE_MS` = **100 ms** (`web_telemetry.c:856`).
  So live frames are **10 Hz**.
- `/telemetry` is a one-shot copy of the same JSON (`web_telemetry.c:9216-9220`).
- The DLL trace (`trace_sample_car_at`) runs in the worker loop at about **20 Hz**
  (`web_telemetry.c:1459-1460`, `Sleep(50)`).
- The console seeds completed laps from the server trace (`telemetry_viewer.js`
  `seedSideTrace`, line 4501, uses `/trace/data`). Laps completed while the page is
  open are built in the browser from the 10 Hz frames (`workbench_ui.js:272-402`).
  **One export can mix a 20 Hz DLL lap and a 10 Hz browser lap.**

---

## C1. Exported lap time differs from the posted lap time (about a tenth)

### C1.1 Facts: the three lap times in the system

| Quantity | Where computed | Formula / source | Evidence |
|---|---|---|---|
| **Game official lap time** (what the timing tower shows) | game field `last_lap` at veh+0x404C | read `c->last_lap = RF(veh, OFF_LAST_LAP)` `web_telemetry.c:5254`, `OFF_LAST_LAP 0x404C` `:773`; shown as `ll` in JSON `web_telemetry.c:5962`; tower "Last" column `tsec(c.ll)` `telemetry_viewer.js:4848` | FACT |
| **Official per-lap history** (`/laps`) | `g_lap_history[car][n] = last_lap`, `-1` when unknown | `lap_history_store` `web_telemetry.c:1355-1371`; `build_laps_json` `:2703-2725` | FACT |
| **DLL trace lap time** (`/trace/data`, `/trace/status`) | distance-wrap crossing difference | `web_telemetry.c:1866` `wrap_ds = lap_len - prev_dist + lap_dist`; `:1872` `crossing_time = prev_sim + wrap_dt*(lap_len-prev_dist)/wrap_ds`; `:1909` `lap_time = crossing_time - lap_start_sim`; `:1933` next lap start = `crossing_time`; `:1938` fallback start = first sample time | FACT |
| **Browser lap time** (viewer, shown and exported) | same crossing formula on frame samples | `workbench_ui.js:380-402`: `crossingTime = prev.t + (sample.t-prev.t)*(trackLen-prev.dist)/(trackLen-prev.dist+sample.dist)` (`:381-382`); `start: startSeen ? lapStart : points[0].t` (`:386`); `end: crossingTime` (`:387`); `lapStart = crossingTime` (`:401`) | FACT |
| **Exported `lap_time`** | `shown.end - shown.start` | `telemetry_viewer.js:1281` in `exportDoc` (`:1250`). The game's time is never read. | FACT |

Sample times in the browser come from frame field `st`: `workbench_ui.js:274` `var t = j.st`.
The frame prints it as **`"st":%.1f`** at `web_telemetry.c:5879` (`json_telemetry` at `:5846`).
The DLL trace uses the float session time at full precision (`:2135`, `trace_emit_sample_json`
prints `"t":%.3f` at `:3099`).

### C1.2 Root cause (ranked)

**RC1 (strongest, FACT + DERIVED): every browser sample time is rounded to 0.1 s.**
- `st` has one decimal (`web_telemetry.c:5879`). Browser crossings interpolate between
  two such timestamps (`workbench_ui.js:381-382`).
- Each timestamp can be wrong by up to 0.05 s. The interpolated crossing is a weighted
  average of two timestamps, so its error is at most 0.05 s. A lap time is the difference
  of two crossings, so its error is at most **0.10 s** (RMS about 0.04 s).
- That matches "off by a tenth or so". It applies only to laps the browser builds
  (completed while the page is open, or the open lap), which is the common case.

**RC2 (FACT): the export uses the browser's derived time, not the game's official time.**
`exportDoc` writes `lap_time: shown.end - shown.start` (`telemetry_viewer.js:1281`). The
tower shows `c.ll` (`telemetry_viewer.js:4848`). The export never reads it, so the two numbers can differ even
when both are correct.

**RC3 (FACT): two timing sources in one export.** Seeded laps take the DLL `lap_time`
(`workbench_ui.js:513-521`, which overrides `start`/`end` from `start_sim + lap_time` when
the doc is complete). Browser laps use RC1 timing. Nothing in the exported doc says which.

**RC4 (FACT): partial laps are exported with a time.** For `!startSeen`, `start = points[0].t`
(`workbench_ui.js:386`), so the "lap" starts at the first sample, not the line. `exportDoc`
writes `lap_time` regardless and only sets `flags` from `complete` (`telemetry_viewer.js:1283`).
The first sample must be within 150 m of the line for a *full* lap (`lapIsFull`,
`workbench_ui.js:762-768`), but the export does not check `lapIsFull`.

**RC5 (HYPOTHESIS, not measured): game boundary vs trace boundary.** The game raises the
lap counter, then writes `last_lap` a few frames later (1.0.1 latch comment,
`web_telemetry.c:1341-1351`). The trace closes the lap at distance wrap, not at the counter
change (`web_telemetry.c:1853-1855`: at Hockenheim the counter flips at 6775.659 m on a
6784.1 m lap, so about 8.4 m earlier). DERIVED: for steady pace this offset cancels between
consecutive laps, so it should not add a constant 0.1 s. It matters only if the game's
timer starts or stops away from the same track point. Test: see C1.5.

**Not a cause (DERIVED):** linear interpolation of distance between samples. With a
10 m/s^2 change in acceleration at 80 m/s and 100 ms spacing, the position error is about
0.0125 m, which is about 0.2 ms of time. The crossing interpolation is already implemented
on both sides (`web_telemetry.c:1872`, `workbench_ui.js:381`). The fix is about timestamp
precision and the choice of source, not adding interpolation.

**Known to the project already:** `FRONTEND_PARITY_2026-10-09.md:26` says browser lap times
"come from 10 Hz samples" with "±0.1 s", and the Laps tab keeps the official times. The
export does not do that.

### C1.3 Proposed fix (keep traces as they are)

1. **Export uses the game's official time when it is available (viewer only, no DLL rebuild).**
   - In the browser, when a lap closes (`completeLap`, `workbench_ui.js:380`), keep it
     pending until the frame's `c.ll` changes, or 8 s pass (same rule as
     `web_telemetry.c:1352-1353`, `LAP_FRESH_S 1.5`, `LAP_WAIT_S 8`). Store the value
     as `official_time`. Use `null` if it never arrives.
   - `exportDoc` (`telemetry_viewer.js:1281`): `lap_time = official_time ?? derived`, and add
     `lap_time_src: "game" | "derived"` and `derived_lap_time`. Set `lap_time: null` when
     `complete` is false (RC4).
2. **Timestamp precision (DLL, needs a rebuild and the owner's gate).** Change
   `web_telemetry.c:5879` from `"st":%.1f` to `"st":%.3f`. This cuts RC1 from +/-0.05 s to
   +/-0.0005 s per timestamp. It does not touch the trace (the trace already uses floats).
   Check that no other reader of `st` assumes one decimal (`schemas/telemetry.json` types it
   as number, so this should be safe).
3. **Do not mix silently.** The export carries `lap_time_src` per lap (RC3).

Do **not** change the crossing formula. It is already correct to first order.

### C1.4 Field rename note

`exportDoc` writes `driver: side.label` (`telemetry_viewer.js:1280`). The label is
`name · car · role` (`workbench_ui.js:225-228`), so the driver field is not a driver name.
Use the roster entry (`n`, `cn`, `tid`) instead.

### C1.5 Tests to add or run (none run now)

- `viewer_test.js` (after line 113, the existing `exportDoc` test):
  - `exportDoc` uses `official_time` when set and sets `lap_time_src: "game"`.
  - `lap_time` is `null` for a partial lap (`complete:false`).
  - Quantized-timestamp fixture: build samples with `t` rounded to 0.1 s and assert the
    derived lap time is within 0.10 s of the true value. This pins RC1.
- `test_telemetry.py`:
  - Live `/events` or `/telemetry` `st` has at least 3 decimals (regex `"st":\d+\.\d{3}`)
    once the DLL change lands.
  - For N completed laps, `|trace lap_time - /laps official| <= 0.10 s` (with the lap join
    in C2.4). This is the measurement that settles RC5 and the size of the remaining error.
- `tests/`: a unit check of `sv2_idx_lap_time` (`storage_v2.inc:1453`) for "game time
  preferred" already exists (`tests/storage_b2_index_test.c:230`). It is the precedent for
  using game time, and the recording index already prefers it (`storage_v2.inc:1283`).

---

## C2. Saved sessions do not keep all laps; export only gives Driver A and B

### C2.1 What the Export button exports now

- Button: `telemetry_viewer.js:3224-3227` in the console toolbar (title "Download the
  displayed laps as f1c.trace/1 JSON"). Handler `exportShown` at `:1289-1300`.
- Iterates `letters()` = all active sides (`:901-903`, `wb.allSides()`). For each side it
  takes `points(L)` = `displayPoints()` (`:905-908`, `workbench_ui.js:431-435`), which is the
  **single displayed lap** (`shown.samples`, or the open lap).
- Output: one download per side, `f1c_trace_<L>_<name>.json` (`:1296`). Format is the
  `f1c.trace/1` JSON (`exportDoc`, `:1250-1286`).
- Doc fields (`:1278-1285`): `schema, source, open, gen, rev, which, slot, driver(label),
  car:'', ai, team:0, lap, lap_time, start_sim, lap_len, count, flags, channels_seen,
  track:'', lid, exported_by, samples[]`.
- Sample keys (`:1256-1276`): `t, d, s, th, br, st, gl, go, gv, y, yr:null, rpm, ge, tx, tz,
  tr[4], tt[4]|null, rx|null, f`.

So "Driver A and B" is the cause: exports only cover the sides on screen, and each side only
holds the lap shown at that moment.

### C2.2 Server data for every car

- **All cars are recorded.** In all-cars mode, `trace_update` calls `trace_sample_car_at`
  for every live slot (`web_telemetry.c:2131-2137`, comment "record EVERY live car slot from
  session start"). Mode is set when the store is allocated (`:1634`, `g_trace_mode`
  default `SELECTED` at `:1604`).
- **Live memory keeps only 6 completed laps per car.** `TRACE_LAPS 6`, `TRACE_LAP_SLOTS 7`
  (`web_telemetry.c:1455-1456`). Older laps are evicted and listed in `evicted`
  (`trace_evict_push`, `:1805-1812`).
- **Archive keeps every completed lap (when the archive is active).** `ar_on_trace_lap`
  is called on each completed lap (`web_telemetry.c:1915`; `session_archive.inc:199-225`).
  The archive is switched on at `session_archive.inc:263` (the condition was not traced).
- **Laps are lost before the archive:**
  - **Laps longer than 160 s are never recorded.** `TRACE_SAMPLES 3200` (`:1466`) at 20 Hz
    is 160 s. At the cap `dropped_samples++` (`:1940`), and a lap with `dropped_samples` is
    discarded at close (`:1895` requires `!t->dropped_samples`; discard at `:1918`).
    Pit-stop, safety-car and wet laps are the likely victims. FACT in the code; the
    frequency is HYPOTHESIS until counted.
  - **Lap is dropped if the start crossing was not seen** (`:1895`: `start_seen` required).
    Grid and first-lap starts after attach are affected.
  - **Archive queue overflow is silent.** `AR_PENDING_LAPS 256` (`session_archive.inc:35`,
    `:205`). Counted in `g_ar_total_dropped` and shown only in the archive status
    (`session_archive.inc:982-984`, field `dropped`), not in the viewer.
  - **Retention deletes whole sessions.** `MaxStorageMB` default 3000 (`web_telemetry.c:3837`).
    An old session can go entirely, not lap by lap.
- **The viewer's lap labels do not match the server's lap numbers.** The browser labels a
  completed lap `openLap + 1` (`workbench_ui.js:384`, "label 1-based"). The DLL trace uses
  the raw game counter `lap_number` (`web_telemetry.c:1856` `sample_lap = c->lap_number`;
  `/trace/data` looks up `lap` against it, `trace_find_lap`). So an exported `lap` (and
  `lid.lap`, `telemetry_viewer.js:1284`) cannot be used to re-fetch the lap. This is the
  open item in `CONSOLE_BACKLOG_2026-10-12.md` ("Bug 2 ... Not changed (OPEN)"). FACT.

### C2.3 Routes and response shapes (all verified in code)

Live (no `session` query):

| Route | Handler | Shape |
|---|---|---|
| `GET /trace/status` | route `web_telemetry.c:9339`, builder `build_trace_status_json` `:2947` | `cars[]`: `{rev, which, slot, used, auto, n, cn, ai, remote, team, dropped_laps, open, open_lap, open_samples, open_dropped, channels_seen, total_samples, laps:[{lap, t, samples, flags, start, lid:{gen,rev,slot,lap,src:"live"}}], unavailable:[...], unavailable_why:{...}}` (`:2972-2990`, lap row `:2998-3007`). `lap` is the raw counter, `t` is lap time, `flags` is `TRL_*` bits (`:1506-1508`: 1 complete, 2 pitted, 4 breaks). |
| `GET /trace/data?car=<A\|B\|slot>&lap=<n>[&fmt=json\|csv][&gen=&rev=]` | route `web_telemetry.c:9418-9475`; JSON `build_trace_data_json_id` `:3114`; CSV `build_trace_data_csv_id` `:3566` | JSON: `{schema:"f1c.trace/1", source:"live", open:false, gen, rev, which, slot, driver, car, ai, team, lap, lap_time, start_sim, lap_len, count, flags, channels_seen, track, lid, samples:[...]}` (`:3140-3150`). CSV: `# f1c.trace/1 ...` comment line then header `sim_t,dist_m,speed_mps,throttle,brake,steer,glat_mps2,glon_mps2,gvert_mps2,yaw_rad,yaw_rate_rps,tread_fl,tread_fr,tread_rl,tread_rr,flags,rpm,gear,tx,tz` (`:3606-3607`). **CSV has no lap column per row.** Errors: 400 bad args (`:9444`), 404 "lap not recorded" or "lap evicted" (`evicted:true`), 409 session or identity changed (`:9458-9470`). |
| `GET /trace/export?car=..&lap=..` | same handler as `/trace/data` (`:9418`); CSV by default (`:9420`), `fmt=json` allowed | CSV or JSON, one lap per request. |
| `GET /laps` | route `:9290`, builder `build_laps_json` `:2703` | `{gen, cars:[{id, p, n, laps:[seconds, ...]}]}`. **Times only, no lap numbers.** Index `j` is the j-th counted lap; `-1` = unknown (`lap_history_store`, `:1355-1371`). |
| `GET /telemetry` | `:9216` | Frame JSON. Car fields: `id, p, n` (driver, `:5978` region), `cn` (car/team string, `:5978`), `tid` (team hash, `:6008`), `ll` (last lap, `:5962`), `bl` (best). |

Session archive (add `?session=<id>`; the route is chosen at `web_telemetry.c:9139-9146`; a
bad or unknown id returns 404):

| Route | Handler | Shape and limits |
|---|---|---|
| `GET /trace/status?session=<id>` | `http_archive_route` `:8809`, `ar_build_trace_status_json` `session_archive.inc:869` | All archived laps. `"mode":"archive"`, `laps_max:100000`, `lap_len`, `cars[]` with `laps[]` and `lid.src:"archive"`. |
| `GET /trace/data?session=<id>&car=<numeric slot>&lap=<n>` | `:8841-8849`, `ar_build_trace_data_json` `session_archive.inc:932` | **Numeric slot only.** Letters `car=A`/`car=B` return `-2` (`:8844-8846`, `which >= 1000` required). |
| `GET /trace/export?session=<id>` | `:8850-8852` | Not supported (`len=-3`). Use `/trace/data` and convert. |
| `GET /laps?session=<id>` | `ar_build_laps_json` `session_archive.inc:691` | `{gen, archive, cars:[{id,p,n,laps:[t...]}]}`. Times only, same join rule as live `/laps`. |

Live fallback: a live request with a numeric slot for a lap the 6-lap ring no longer holds
is served from the current archive (`web_telemetry.c:9455`, `ar_live_fallback_trace`
`session_archive.inc:965`). So `/trace/data?car=<slot>&lap=<n>` already returns old laps
while recording, and the viewer does not use it.

### C2.4 Join rules (needed by the export)

- Take lap numbers from `/trace/status` (`laps[].lap`) and fetch with the same number. Do
  not use the viewer's `lap` label (C2.2).
- Take official times from `/laps` by order: the k-th completed lap in the trace list is
  `laps[k]` in `/laps`. Check each pair: `|t_trace - official| <= 0.25 s`. If the check
  fails, mark the lap `lap_time_src: "derived"` and do not substitute. (INFERENCE: `/laps`
  has one entry per counted lap, and the trace list can be shorter after eviction. Verify
  with the test in C1.5 before relying on it.)
- Archive: `laps[].lid.lap` and `row.lap` are the same raw counter (`ar_lap_rec_t`,
  `session_archive.inc:46-59`). Use them directly.

### C2.5 Proposed "Export laps" dialog (spec, no visual design)

Inputs:
- **Source:** Live, or a session from the recordings list (`?session=<id>`).
- **Cars:** multi-select of cars from `/trace/status` `cars[]` (label `n`, slot `slot`).
  Also select by team: team label from `cn` with a leading year removed (`cn` is
  `"1995 McLaren"` in the corpus, `findings.jsonl` id `veh-4678-car-4714-veh-path-live-2026-10-01`).
  Group by `team` hash (`tid`) when labels collide.
- **Laps:** all completed, or best lap (min `t` over laps where `flags & 1` is set).
  Default: exclude laps with `flags & 4` (breaks) and `flags & 2` (pitted); show toggles.
- **Format:**
  - JSON: one `f1c.trace/1` document per lap, same shape as the viewer export. Can be
    re-imported with the existing Import (`telemetry_viewer.js` `importFile`, `:1215-1245`).
  - CSV (ML): one row per sample. Columns, in order: `session, car_slot, driver, team, lap,
    lap_time, lap_time_src, sim_t, dist_m, speed_mps, throttle, brake, steer, glat_mps2,
    glon_mps2, gvert_mps2, yaw_rad, yaw_rate_rps, rpm, gear, tx, tz, tread_fl, tread_fr,
    tread_rl, tread_rr, flags`. Missing channels are empty (the CSV writer already leaves
    them empty, `web_telemetry.c:3619-3642`). Units as emitted; `throttle`/`brake` units
    are not verified here (see C2.7).
  - JSON Lines variant: one lap document per line, for large runs.
- **Output:** one file, named `f1c_laps_<source>_<driver-or-team>_<YYYYMMDD-HHMM>.<ext>`.

Implementation (client-side, existing routes only):
1. `GET /trace/status` (or `?session=`) to build the lap list.
2. For each selected lap, `GET /trace/data?car=<slot>&lap=<n>` (JSON) or `&fmt=csv`. In
   archive mode, use the numeric slot only (C2.3).
3. **Fetch one request at a time.** The DLL shares one 2 MB buffer for HTTP
   (`web_telemetry.c:9449`, "HTTP is single-threaded"). Parallel requests will overwrite each
   other. A lap can be up to about 1.5 MB (`:3233`).
4. Add the lap-level fields (`lap_time`, `lap_time_src`, team, driver) from C2.4.
5. A lap that returns 404 or 409 is listed in the dialog as "not recorded / evicted" with
   the reason. It is never dropped silently.

### C2.6 Gaps to close (DLL side, not part of the viewer-only change)

- Laps over 160 s: raise `TRACE_SAMPLES` or stream samples to the archive without the
  in-memory cap (`web_telemetry.c:1466`, `:1940`, `:1895`). Needs a rebuild and the gate.
- Count and surface dropped laps per car (`dropped_laps` already exists in the status row,
  `:2976`); show them in the dialog. Archive queue drops (`session_archive.inc:205`) should be
  shown per session, not only in `/health`.
- Start-crossing rule (`:1895`): decide whether attach-mid-lap laps are kept as `TRL_BREAKS`
  instead of discarded.

### C2.7 Pending validation

- Actual frequency of >160 s laps and of archive drops in a real race: not counted here.
- `throttle`, `brake` units in the JSON and CSV (the viewer treats them as percent; the
  writer uses `%.2f`). Check one live lap before the ML schema is frozen.
- Whether `archive` is on by default (`session_archive.inc:263` condition not traced).

---

## C3. Team column or indicator next to driver names

### C3.1 Which field carries the team

| Field | Where | Meaning | Evidence |
|---|---|---|---|
| `n` | `web_telemetry.c:5210-5216` (`OFF_DRIVER_NAME 0x4620`, `:818`) | driver name, for example "Mika Hakkinen (1995)" | FACT |
| `cn` | `web_telemetry.c:5216-5220` (`OFF_CAR_NAME 0x4678`, `:819`), emitted at `:5978` | "car/team name" in the source comment, but the corpus shows **"1995 McLaren"** (season prefix plus constructor) | FACT, `findings.jsonl` `veh-4678-car-4714-veh-path-live-2026-10-01`; `tools/sdk/f1c_vehicle_struct.h:199` |
| **team short name** | game veh+**0x46B8** ("McLaren") | plain team name | FACT (corpus). **The DLL never reads it** (no reference to 0x46B8 in `web_telemetry.c`). |
| `tid` | `web_telemetry.c:6008` (team hash, 0 = unknown; `trace_team_id` `:1591`) | broker ABI-8 FNV team hash, for matching teammates | FACT |
| `team` (trace/status, `/laps` absent) | `web_telemetry.c:2972`, `:3153` | same hash as `tid` in the trace | FACT |
| `gar_team` | `web_telemetry.c:5967` | garage index, **not** a team name | FACT |

Consequences:
- The only human-readable team-like string on the wire is `cn`, and it has a year prefix.
  `cleanName` strips only a **suffix** "(YYYY)" (`telemetry_viewer.js:2419`), so `"1995 McLaren"`
  is shown with its year. Needs a prefix strip (`/^\d{4}\s+/`) for display.
- The clean team name at +0x46B8 is not exposed. Exposing it is a DLL change (new read and a
  new JSON key, for example `tn`). It needs the gate and a schema entry in `schemas/telemetry.json`.
- `/laps` has no `cn` or `tid`. A table that uses `/laps` must join on `id` with the frame roster.

### C3.2 Dead code to ignore

- `telemetry_viewer.js` is not the only place with a TEAM label. The classic page
  (`web_telemetry.c:7693`, "TEAM: '+me.cn") is inside the compiled-out classic block
  (`#if 0` at `:6079`, `#endif` at `:8002`). It is not shipped.

### C3.3 Tables that show driver names in the live console

Pages are from `RACE_PAGES` and `ANALYSIS_PAGES` (`telemetry_viewer.js:2376-2379`), mounted
by `renderView`. The page ids are in brackets.

| # | Table / element | Page id | File:line | Current driver cell | Team column location |
|---|---|---|---|---|---|
| 1 | Timing Tower (race) | `race` (`view_race` `:5110`) | `telemetry_viewer.js:4805` (`towerPane`), columns `:4809-4810` (`full=false`), row `:4847-4848` | `cleanName(c.n)` + tags | After `Driver` (`:4809-4810` list, `:4847` cell). |
| 2 | Timing Tower (full) | `timing` (`view_timing` `:5122`) | same `towerPane`, `:4809-4810` (`full=true`) | same | After `Driver`. |
| 3 | Race Pace table | `timing` | `pacePane` `:5074`, header `:5087`, cell uses `code3(c.n)` (`:2415` helper) | 3-letter code | Optional: team after code, or no column (width). |
| 4 | Director drivers list | top tab "Director" (`tvRenderDrivers` `:3688`) | header `:3690`, cells `:3696` (`d.name`); data `tvdDrivers` `:3391-3403` (`name: c.n` at `:3397`) | `d.name` | Add `team` to the `tvdDrivers` object (from `c.cn`), then a header and cell. |
| 5 | Session Overview: fastest lap and pit stop rows | `race` (`sessionPane` `:5008`) | fastest `:5058`, stop rows `esc(r.n)` in the same pane | text, not a table | Optional text, not a column. |
| 6 | Driver Focus header | `focus` (`view_focus` `:5227`) | `:5266` (`cleanName(fc.n)` + `cn`) | already shows `cn` | Header already has the car/team string; a dedicated "Team" label would use the same stripped value. |
| 7 | Driver Compare (A vs B) and Lap analysis | `compare` (`Viewer.view_compare` `:1887`), `overview` (`:1598`) | side header row `:1931` (`L · side.label`), chassis `:1839`, lap-strip tag `:4326` | `side.label` (name · car · role) | `label` already has `cn` (`workbench_ui.js:225-228`). A Team cell would be `cn` with the year stripped. |
| 8 | Side dropdown options | all pages (A/B selectors) | `telemetry_viewer.js:1068` | `esc(c.n) + tag` | Optional: append team to option text. Not a table. |
| 9 | Race Control messages | `race`, `timing` (`rcPane` `:5047`, `rcMsg` `:2955`) | `e.n` in text | text | No table; no column. |

Checked and not a target: the Recordings page (`view_sessions` `:5786`) lists sessions by
name and has no per-driver table.

### C3.4 Proposed change

1. **Viewer only (no DLL rebuild):** derive `team = cn.replace(/^\d{4}\s+/, '')`. Add a
   `teamOf(c)` helper next to `cleanName` (`telemetry_viewer.js:2419`). Add a Team column to
   towerPane (full and short), the Director list, and the compare header.
2. **DLL (needs gate):** expose the team name from veh+0x46B8 as a new key (for example
   `tn`) in `json_telemetry` (`web_telemetry.c:5978` area), and in `build_trace_status_json`.
   Then the viewer uses `tn` and falls back to the stripped `cn`. Add the key to
   `schemas/telemetry.json`. The 0x46B8 read is FACT in the corpus, but not yet proven in
   this DLL, so the owner should confirm it on a live slot first.
3. Show `tid` only as a hidden grouping key for teammates. The only teammate logic in the
   tree is in the compiled-out classic page (`web_telemetry.c:7686-7690`, "C15: prefer the reviewed
   broker team identity"). The live console has no teammate logic yet, so do not rely on it.

### C3.5 Tests to add

- `viewer_test.js`: `teamOf({cn:'1995 McLaren'}) === 'McLaren'`; `teamOf({cn:'Car'}) === 'Car'`;
  Tower rows contain the team cell in the header order; Director list objects carry `team`.
- `test_telemetry.py`: `/telemetry` car objects include `cn`; once added, `tn` equals the
  corpus value for a live slot (owner check).

---

## 5. Pending validation (not run in this task)

1. Measure RC1 and RC5: export `lap_time` against `/laps` official for N laps in a live race
   (game running, owner-run). Record the mean and max difference and the fraction within
   0.10 s before and after `st %.3f`.
2. Confirm the `0x46B8` team string on a live slot before adding a DLL read.
3. Count laps over 160 s and archive drops in one real race (`/health` archive `dropped`,
   `session_archive.inc:984`; `dropped_laps` in `/trace/status`).
4. Confirm `throttle`, `brake` units in JSON and CSV.
5. Confirm the `/laps` index-to-trace join (C2.4) on one session.

No live or gameplay check was run. All statements are from code and stored data.

## 6. Actionable follow-ups (for a future spec, not started)

- Viewer: official-time capture and export fields (C1.3.1), partial-lap null (C1.3.1),
  team display (C3.4.1), "Export laps" dialog (C2.5).
- DLL (gated): `st` precision (C1.3.2), `tn` team name (C3.4.2), 160 s lap cap and drop
  surfacing (C2.6).
- Documentation: `FRONTEND_PARITY_2026-10-09.md:26` says 10 Hz "±0.1 s". This is now
  explained (C1.2 RC1). Update it when the fix lands.
- `CONSOLE_BACKLOG_2026-10-12.md` Bug 2 (lap label off by one) is still OPEN and blocks
  C2.4. Fix it before the export relies on lap numbers.

## 7. Documentation gate

- This record exists in the component directory. It is **not yet indexed** in the component
  `README.md` or `CURRENT_STATE.md`. Those files were out of scope (read-only task), so add
  an index line when editing is allowed.
- Facts and inferences are separated by tier (section headers and the tier labels).
- Pending validation is in section 5.
- No credentials, `api1.txt`, private keys or raw sensitive output were copied here.
- `governance\PRODUCT_STATUS.json` was not touched, and no stage change is implied.
