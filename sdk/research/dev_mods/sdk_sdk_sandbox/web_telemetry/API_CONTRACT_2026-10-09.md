# Web Telemetry API contract — api_version 3 (2026-10-09)

Frozen backend contract for the Web Telemetry workbench (rc.10, BACKEND 4
final candidate). The frontend is designed against this document; the
backend must not change a documented shape without bumping the version
(procedure in section 10).

- Machine-readable schemas: `schemas/` (11 endpoint schemas + `version.json`).
- `schema_set_sha256`: `A6DADFA2AAE9EF7A044FBD17632B818B2518C104AD3F27E8E268DD039A0F04B3`
  (pinned in `schemas/version.json`; the contract test recomputes it).
- `/health` reports `"api_version":"3"`; the gate fails when the schema set
  changes without `schemas/update_version.py` and a matching `API_VERSION`
  bump in `web_telemetry.c`.
- All surfaces are read-only. `POST /trace/import/legacy` converts a file
  in memory; it never touches game state.

## 0. Change log

### v2 -> v3 (BACKEND 4 review, 2026-10-09)

1. **Native game-thread capture cost.** `/health` adds
   `capture_cost_us`: `{samples, p50, p95, max, rate_per_s}` measured with QPC
   inside the broker callback (the game-thread path). The same values are
   logged every ~10 s as `capture_us p50=… p95=… max=… n=… rate=…/s`.
   Checklist limit: p95 < 500 us throughout a race with no owner-reported
   frame hitch.
2. **Process memory.** `/health` adds `memory.working_set_kb` for the
   session-flatness check.
3. **Freshness is part of acceptance.** The live-check script records capture
   time (`sample_age_ms`, `sample_sequence`, UTC) and simulation time for
   every poll; identical samples are reported as a paused frame, not a pass.
   (No response-shape change beyond the two fields above.)

### v1 -> v2 (BACKEND 2 review, 2026-10-09)

1. **RPM units made unambiguous.** `/telemetry` per-car `r`/`mr` stay raw
   angular speed in rad/s and are documented as raw; new `rpm`/`max_rpm`
   fields carry the converted display RPM (`x 9.549296585513721`). No field
   named or documented as RPM carries rad/s (schema rule enforced by
   `contract_validate.check_schema_contract`).
2. **Stable lap identity + explicit eviction.** Every completed/imported lap
   carries a `lid` (generation + revision + slot + lap + source). `/trace/data`
   and `/trace/export` accept optional `gen`/`rev` pins and answer an explicit
   `409 lap identity changed`/`404 lap evicted` instead of silently serving a
   different lap; `/trace/status.evicted[]` lists ring evictions. Any
   completed or currently cached imported lap can be kept as the reference
   across crossings (client flow in section 8).
3. **Player steering unavailable with a reason.** `/telemetry` adds
   `steerwhy` (empty when `vf` bit 8 is set); `/trace/status` adds
   `unavailable_why`. Player steering stays unavailable until the live
   steering check passes; AI steering (0x3580 AI input factor) is unchanged.
4. **/trace/compare accepts an imported lap on either side.** `lapA`/`lapB`
   accept the literal token `import`, which uses that side's cached legacy
   import; sides carry their `lid` (imports include the per-side `serial`).
   (Workspace payload staging moved out of the workspace; that changes no
   response shape.)

## 1. Surfaces

| Endpoint | Method | Params | Schema | Limits / notes |
|---|---|---|---|---|
| `/health` | GET | — | `health.json` | build identity, broker ABI, capture state, `api_version`, sidecar integrity (`present/bytes/sha256/expected/matches`), native `capture_cost_us` (`samples/p50/p95/max/rate_per_s`) and `memory.working_set_kb` |
| `/telemetry` | GET | — | `telemetry.json` | full frame; 262,144 B response bound; ~31 KB at 22 cars; per-car `r`/`mr` raw rad/s, `rpm`/`max_rpm` converted, `steerwhy` steering reason |
| `/events` | GET | — | SSE: `data: <telemetry.json>` | 10 Hz; `Access-Control-Allow-Origin: *`; 16-client cap (17th closed) |
| `/track` | GET | — | `track.json` | AI waypoint line, ≤3000 points; 131,072 B bound |
| `/laps` | GET | — | `laps.json` | per-car lap-time history, ≤120 laps |
| `/incidents` | GET | — | `incidents.json` | bounded ring (≤400) |
| `/pits` | GET | — | `pits.json` | bounded pit-event ring |
| `/driver` | GET | `id=0..103` | `driver.json` | 400 on bad id; lap rows + that driver's pits; 131,072 B bound |
| `/trace/status` | GET | — | `trace_status.json` | A/B recorders, open lap, completed laps (each with `lid`), `unavailable[]` + `unavailable_why{}`, `evicted[]`, loss counters; 524,288 B bound |
| `/trace/select` | GET | `which=A\|B`, `car=0..103\|-1`, optional `gen` | `trace_status.json` | internal selection only; `gen` mismatch → 409; bad params → 400 |
| `/trace/data` | GET | `car=A\|B`, `lap=N`, optional `gen`, `rev` | `trace_data.json` (`lid` included) | recorded lap; never recorded → 404 `evicted:false`; evicted by the ring → 404 `evicted:true`; `gen`/`rev` mismatch → 409 `evicted:true` |
| `/trace/export` | GET | `car`, `lap`, `fmt=csv\|json`, optional `gen`, `rev` | CSV (default) or `trace_data.json` | CSV header carries `src=live`; same 404/409 semantics as `/trace/data` |
| `/trace/open` | GET | `car=A\|B` | `trace_data.json` (`open:true`, `lid`) | current partial lap; no open samples → 404 |
| `/trace/compare` | GET | `carA`, `lapA`, `carB`, `lapB` (`N` or `import`), `step=1..50`, optional `gen` | `trace_compare.json` | either side may be the cached import; distance grid ≤1000 points; unknown lap/import → 404; all-hole/unknown distance frame → 404; `gen` mismatch → 409; 524,288 B bound |
| `/trace/import/legacy` | POST | `car=A\|B`; body `.lap`/`.spt`; `Content-Type: application/octet-stream` | `trace_data.json` (`source:legacy`, `lid.src:import`) | body ≤16 MB; sniff `CHECKSUM=` → lap else spt; 415 wrong type; 413 oversized; 400 malformed |
| `/trace/import` | GET | `car=A\|B` | `trace_data.json` | last imported document (same `lid`, serial incremented on replacement); 404 when none |
| `/action` | GET | — | `error.json` | always 403 (read-only refusal; behavior frozen) |
| unknown path | any | — | `error.json` | 404 |
| other methods | — | — | `error.json` | 405 (HEAD: no body) |

## 2. Units and conversions

| Quantity | Live `/telemetry` | Trace/import documents |
|---|---|---|
| Engine RPM | `r`/`mr` raw **rad/s** (documented raw); `rpm`/`max_rpm` converted display RPM = raw × 9.549296585513721 | `rpm` already **display RPM** |
| Speed | `s` m/s | `s` m/s |
| Throttle/brake | `th`/`br` 0..1 | same |
| Steering | `steer` input factor; valid only when `vf` bit 8 (AI); `steerwhy` is the reason when clear | `st`; f bit 16; reason via `/trace/status.unavailable_why.steering` |
| Accelerations | `glat`/`glon` m/s² | `gl`/`go`/`gv` m/s² |
| Yaw | — (heading matrix; derived client-side) | `y` rad; `yr` rad/s (derived) |
| Gear | `g` int (0 = neutral) | `ge` |
| Track position | `mx`/`mz` (coordinatesInTrack), `px`/`pz` (world) | `tx`/`tz` (map frame) |
| Tread | `trd[4]` 0..1 remaining; `vf` bit 16 | `tr[4]`; f bit 1024 |
| Legacy `.lap` velocity | — | mph → m/s (`÷2.2371365`), listed in `conversions[]` |
| Legacy `.lap` accel | — | g → m/s² (`×9.8`), listed |
| Legacy `.lap` yaw | — | degrees → rad, listed |
| Legacy `.spt` distance/speed | — | derived per viewer DM#8 (x/z deltas; mph factor cancels) |

## 3. Validity, nullability and sentinels

- **Never read a value without its validity marker.** Two explicit mechanisms:
  - `/telemetry`: the per-car `vf` bitmask (bit 1 speed, bit 2 throttle,
    bit 4 brake, bit 8 steering, bit 16 tread, bit 32 lateral acceleration,
    bit 64 longitudinal acceleration). A clear
    bit means the value is not data (the field may still carry 0).
  - Trace samples: nullable channels are `null` when missing (`st`, `gl`,
    `go`, `gv`, `y`, `yr`, `rpm`, `ge`, `tx`, `tz`); the original channels
    (`s`, `th`, `br`, `tr`) carry the per-sample `f` bitmask
    (2 speed, 4 throttle, 8 brake, 16 steer, 32/64/128 accel, 256/512 yaw,
    1024 tread, 2048 segment, 4096 rpm, 8192 gear, 16384 track). `f` bit
    2048 marks the first sample after a gap: clients must break the line.
- **Steering (v2 rule).** Player steering input has no proven writer yet, so
  the player's `/telemetry` `vf` bit 8 is always clear and `steerwhy` carries
  `"player steering unavailable: live steering check pending (veh+0x3580
  writer unproven)"`; trace `st` is `null` and
  `/trace/status.unavailable_why.steering` carries the same reason. AI
  steering (0x3580 AI input factor) is valid with `steerwhy` empty unless the
  sample itself is invalid (`"AI steering channel invalid in this sample"`).
  Legacy imports carry the file's own steering channel under the same trace
  rules; they are not a live player capture.
- **Lap identity (v2).** Every completed/imported lap document and status lap
  carries `lid` = `{gen, rev, slot, lap, src}` (`src` = `live`/`import`), plus
  `serial` (≥1, imports only). `gen` is the capture generation, `rev` the
  recorder revision (identity resets bump it), `slot` the vehicle slot,
  `lap` the session lap number. A client keeps a reference by storing `lid`
  and re-requesting the lap with `gen`/`rev`; the backend never silently
  serves a different lap: ring eviction → 404 `evicted:true`, recorder reset
  or session change → 409 `evicted:true`, never recorded → 404
  `evicted:false`. `/trace/status.evicted[]` (bounded, oldest first) lists
  recent ring evictions with their `lid`.
- `unavailable[]` in `/trace/status` names channels never seen for that side;
  `unavailable_why` maps each of those names to a one-line reason.
- Sentinels: `gai/gbi/cw` = -1 (none), `s3` = -1 (split missing), `boxwhy`
  empty (no heuristic guess), `dotdid/flapid` = -1 (unset), `ms` is the game's
  dead 99999 sentinel and is not telemetry. `tid` 0 = unknown team.
- Unknown run metadata is explicit: empty `trk`, 0 `sc1/sc2/lapm` (AIW not
  read — treat as unknown, not equal thirds), 0-length names stay empty.

## 4. Rates and budgets

| Item | Value |
|---|---|
| Capture (game thread) | 20 Hz target (duplicates dropped during pause) |
| SSE transmit | 10 Hz (`SSE_UPDATE_MS=100`) |
| Status polling | 0.5 Hz while Engineering is open |
| Response bounds | `/telemetry` 262,144 B; `/track`, `/driver` 131,072 B; `/trace/*` 524,288 B; compare ≤1000 points; track ≤3000 points |
| SSE clients | 16 (17th refused without disturbing the server) |
| Import body | ≤16 MB, 3 s read deadline; 5 s slow-client header deadline |
| Remote cost | ~310 KB/s per client at 22 cars (~2.5 Mbit/s); Tailscale note in the manifest |

## 5. Counters and loss reporting

`failed_captures` (health), `dropped_laps` / `open_dropped` / `total_samples`
(trace status), `hlost` (per-car dashboard ring), `truncated` (trace document
when the response bound cut the list), `reduction{from,to,stride}` (import
stride-reduction), `breaks`/`break_dist[]` (compare grid holes), CSV `flags`
column, `evicted[]` (ring evictions, v2). Loss is always counted, never
silently dropped.

## 6. SSE events

One event per transmit tick: `data: <telemetry.json>\r\n\r\n`. No catch-up
after reconnect (nearest state only); a client must treat >2.5 s without a
frame as stale. Wildcard CORS is served on `/events` only.

## 7. Errors

`{"error":"..."}` with: 400 bad params/malformed body, 403 `/action` refusal,
404 unknown path/lap/document, 405 wrong method (HEAD no body), 409 stale
`gen`/`rev` on select/data/compare, 413 oversized import, 415 wrong import
content type, 500 only when a response buffer overflows (should not occur;
bounded builders). `/trace/data|export` misses add `"evicted":true|false`
(v2).

## 8. Frontend needs → contract map (BACKEND 2 item 4)

| Need | Surface |
|---|---|
| A/B selection (player or AI) | `/trace/select` + `/trace/status` (slot, names, ai, auto) |
| Live open lap | `/trace/open` (`open:true`, `lid`) |
| Completed laps | `/trace/status.laps[]` (each with `lid`) + `/trace/data` |
| Reference lap (v2) | client picks any `laps[].lid` or an import `lid`; re-request `/trace/data?car=..&lap=..&gen=..&rev=..`; keep polling the lid while driving; eviction appears in `/trace/status.evicted[]` and as 404 `evicted:true`; a new import replaces the previous one under a higher `serial` (explicit, never silent) |
| Freeze snapshot | client-side pin of the fetched documents (no server freeze endpoint) |
| Delta by distance | `/trace/compare.delta[]`; cursor interpolation per DM#14 is client-side (documented formula) |
| Intervals / sectors / statistics | `/driver` lap rows (`t,s1,s2,s3`), `/laps` history, `/telemetry` `sc1/sc2/lapm/secel`; run statistics derived from samples |
| Track line + car positions | `/track.pts[]`, `/telemetry` `mx/mz`, `px/pz`, `dx/dy`; trace `tx/tz` |
| G-G | `/telemetry` `glat/glon`; trace `gl/go/gv` |
| RPM (converted) | `/telemetry` `rpm`/`max_rpm` (converted); raw `r`/`mr` rad/s documented; trace `rpm` |
| Inputs | `/telemetry` `th/br/g/steer` + `vf` + `steerwhy`; trace `th/br/ge/st` (+ status `unavailable_why`) |
| Import / export | POST `/trace/import/legacy`, GET `/trace/import`, `/trace/export` (`fmt=json` is the portable `f1c.trace/1`); compare accepts `lap=import` on either side |
| Run metadata (car/team, driver/control, track, settings/mod identity, fuel/tyres/weather, timing; unknown = unknown) | `/telemetry` names/`a`/`tid`/`trk`/`at`/`trkt`/`onw`/`tw`/`fl`/`tc`/`wfr`…; `/health` `version`+`api_version` for mod identity; unknowns per section 3 |
| Recording/loss/truncation counters | section 5 |
| Capture cost + freshness (v3) | `/health` `capture_cost_us` (p95 < 500 us limit, no frame hitch) and `memory.working_set_kb`; the live-check script records capture time and sim-time advance and treats identical samples as paused |

## 9. Explicit non-goals

No writes to the game; no server-side freeze; no catch-up replay; no
percent-decoding of query values (ASCII literals only); duplicate query keys
resolve to the first occurrence; no keep-alive (Connection: close).

## 10. Change procedure

1. Edit `schemas/*.json` and/or the contract text.
2. Bump `API_VERSION` in `web_telemetry.c` (any shape change) and rebuild.
3. `python schemas/update_version.py <new_version>`.
4. Update this document + the manifest `api_contract` section; run the
   canonical gate (`TELEMETRY_TESTS=1`) and the full SDK gate.
A schema edit without steps 2-3 fails the contract gate.


## 11. rc.13 parser and missing-data clarification (no shape change)

api_version remains 3 and schemas/version.json plus the schema-set digest are
unchanged. Request lines must fit completely and use HTTP/1.0 or HTTP/1.1 with
no extra tokens. Overlong query values, numeric overflow and invalid explicit
A/B or CSV/JSON selectors are refused rather than truncated or silently
selecting the other side. A Content-Length must be digits only, fit the parser,
and occur once; Transfer-Encoding is refused for the bounded import body.
These invalid requests return the existing JSON error shape with status 400.

The established optional compare step behavior remains: missing or malformed
step falls back to 5 m; parsed finite values retain the existing 1..50 m clamp.
Duplicate query keys still resolve first-wins. No percent-decoding was added.

Raw nonfinite channel readings remain finite in JSON but have their validity
bits cleared; clients must honor those bits. Browser interpolation/drawing does
not bridge missing samples or extrapolate a partial lap. Canonical trace gear
is `ge`; completeness flags distinguish full live exports from partial/open or
truncated traces. Valid UTF8 names are preserved; invalid legacy bytes are
escaped using a Windows-1252 compatibility fallback, whose applicability to
other codepages remains unverified. See the rc.12 audit for evidence and limits.


## 12. rc.14 completed live-import timing (no shape change)

A completed live trace's `start_sim` and positive `lap_time` describe the
interpolated line-crossing boundaries; the first/last samples can fall inside
that interval. The workbench uses those boundaries for imported full-lap
elapsed time and fastest ranking when both inputs and their sum are finite
and contain the sample interval (1 ms tolerance for three-decimal timestamp
serialization). Missing or inconsistent metadata falls back to sample-derived
bounds. Partial/open/truncated and legacy imports keep their previous timing
behavior. api_version 3 and all schema bytes remain unchanged. Evidence:
[Luna work report](LUNA_WORK_REPORT_2026-10-09.md) and
[parent review](LUNA_FIX_REVIEW_2026-10-09.md).


## 13. 4.3.0-rc.1: api_version 4 (shape change, additive)

Procedure of section 10 followed: schemas edited, `API_VERSION "4"`,
`schemas/update_version.py 4`. The schema set digest is in `schemas/version.json`.
All changes are additive. Existing clients that ignore unknown fields keep working,
and `f1c.trace/1` exports from rc.14 still import (the document id is unchanged).

- `/telemetry` car rows gain these fields:
  - `vf6`: validity bits. 1 = vertical accel, 2 = yaw, 4 = tyre temp FACT
    (player), 8 = player carcass, 16/32/64/128 = RESEARCH susp/load/slip/force,
    256 = RESEARCH AI tyre temp.
  - `gv`: m/s2.
  - `yaw`: rad.
  - `ttc[4]`: tyre temp in C, FL, FR, RL, RR.
  - `tcc[4]`: player carcass temp in C; -999 otherwise.
  - `rx[16]`: RESEARCH raw candidates in the order susp x4, load x4, slip x4,
    force x4.
- `rx` values have no proven unit or meaning. Clients must not label them as
  physics. Values are meaningful only when the matching `vf6` bit is set.
- `f1c.trace/1` samples gain `tt` (array[4] or null) and `rx` (array[16] or
  null). Validity is carried by sample flag bits:

  | Bit | Value | Meaning |
  |---|---|---|
  | `TRF_TTEMP` | 32768 | tyre temperature |
  | `TRF_RX_SU` | 65536 | RESEARCH susp |
  | `TRF_RX_LD` | 131072 | RESEARCH load |
  | `TRF_RX_SL` | 262144 | RESEARCH slip |
  | `TRF_RX_FO` | 524288 | RESEARCH force |
  | `TRF_TTEMP_RX` | 1048576 | `tt` is the AI RESEARCH candidate, not FACT |

  Importers must accept flags up to 2097151.
- `/trace/compare` channel sets gain `ttemp0`..`ttemp3` and `rx_su0`..`rx_fo3`
  (null where invalid; never bridged).
- `/trace/status` may list `tyre_temp` in `unavailable` with a specific
  `unavailable_why`.
- `/health` gains `viewer_ui`, with the same shape as `workbench_ui`.
- New read-only GET routes, outside the JSON schema set: `/viewer` (page) and
  `/viewer.js` (sidecar `telemetry_viewer.js`, 404 plain text when missing).
- Bounds: trace/compare documents are built in `TRACE_JSON_BYTES` = 1,572,864.
  The measured worst case is trace_data 1,056,123 B and compare 619,798 B. The
  CSV export shape is unchanged.

Evidence: [VIEWER_4.3.0_2026-10-11.md](VIEWER_4.3.0_2026-10-11.md).


## 14. 4.3.0-rc.2 CO-OP additions (api_version 4, unreleased; digest re-stamped)

- `/trace/status` cars gain `remote` (1 = controller mode 2 when recording
  started).
- `/telemetry` `vf6` bit 512 marks a remote car. Its simulated channels are
  cleared, and `s` (speed) is DERIVED from the interpolated position.
- `cr` is the controller mode `veh+0x32C8` (-1, 0 local, 1 AI, 2 remote,
  3 replay), now read every tick.


## Batch 2c note (2026-10-12, rc.3)

- ls1, ls2, bs1, bs2 on the live frame and lap s2 are CUMULATIVE elapsed times from the line (S2 = ls2 - ls1). Unchanged; documented here. s3 on lap rows is the S3 split (lap - s2).
- Lap rows: s2d = the S2 split (s2 - s1), added in rc.2; documented here.
- No new JSON keys in rc.3. The only API behaviour change is GET /trace/select (A/B slot), which the console now calls on A/B change. Default auto A is a running leader, not a stopped or retired car.
- Same slot: the ring is kept. Different slot: the side's ring is cleared.
- api_version stays 4.

## Batch 2g note (2026-10-12, rc.4)

Additive, api_version stays 4. Car objects in /telemetry (and the SSE frame) gain two keys after `pt0`:

- `gar_team` (integer): AIW team index of the car, raw veh+0x48A4 (CONFIRMED field offset; ordering meaning DERIVED).
- `gar_bay` (integer): garage bay 0..2 inside the team, raw veh+0x48A8 (CONFIRMED offset).

Both are required in schemas/telemetry.json `$defs/car` (the rc.4 DLL always emits them). schemas/version.json schema_set_sha256 refreshed with `python schemas/update_version.py 4`. The key name `gb` is not used: it already means metres to the car behind (v4). Viewer order rule: DERIVED, see CONSOLE_BACKLOG "Batch 2g".


## Additive keys since rc.4 under api_version 4 (4.4.0-rc.5 and rc.6, 2026-10-12, Batch 2i)

Supersedes the former "api_version 5" wording. Claude reverted the bump on 2026-10-12: all changes below are additive, so `api_version` stays "4" (web_telemetry.c API_VERSION, test_telemetry.py pins, schemas/update_version.py 4). No key is removed and no key changes meaning.

- /telemetry car objects gain `num` (integer, race number veh+0x4710, -1 = out of range) and `yr` (number, RAW rad/s, DERIVED from heading between ticks; valid only with vf6 bit 2048). Sign: positive `yr` = right turn (owner test FACT, rc.6 viewer labels).
- /telemetry gains top-level `ch_tiers` (object of string to string): evidence tier per channel. Player steering stays OPEN and reports steer 0 with car_valid bit 8 clear.
- `gar_team` and `gar_bay` (rc.4) are DEPRECATED for ordering. The selector sorts by `num` since rc.5 (owner decision: race-number order). The keys still ship; do not use them for order.
- Lateral g sign (owner O3 FACT, rc.6): `glat` < 0 = right turn, glat > 0 = left turn. Viewer labels follow this; the wire value is unchanged.
- Player rx loads: `rx[4..7]` = FL, FR, RL, RR (order FACT); unit newtons DERIVED; player `rx[0..3]` read 0. AI rx front/rear labels stay unverified.
- Engine: `max_rpm` is the rev-limit source (owner test FACT: player rev cut ~17590 rpm, max_rpm 17600). HP, Nm and boost are NOT in the game data (no key).
- 4.4.0-rc.7 (Batch 2j, DERIVED): /telemetry car objects gain `tcn` (string): the compound name for the car's `tc`, from SEASONDATA\Vehicles\<team>\<car>\*.veh (Driver= with a trailing " (year)" ignored, Number=, HDVehicle=) -> the same-folder .hdv TireBrand= -> Vehicles\<brand>.tbc [COMPOUND] Name= in order. Empty string when not resolved (the viewer then shows C<tc>). Tier DERIVED; the index-to-name mapping is not yet checked against a live tyre choice.
- NOT YET IMPLEMENTED (OPEN, no key exists yet): `mods.mechfail` in /health and a `forecast` key from LOG\weather.txt. Documented here so the contract does not promise them.
- schemas/version.json: api_version "4", schema_set_sha256 AAB4CAD783C130D7E2061263E7B62F233587307C740A19A1320A932C12D983BE (refreshed by update_version.py 4 in rc.7 when `tcn` was added to schemas/telemetry.json; previous A6F353589F50BD73DBF2EA02BFA84D49769E73ABBFBE8319F27CE5803D2D0DD4). Gate: test_telemetry.py pins api_version 4.

## Additive keys rc.8 (Batch 2k, 4.4.0-rc.8, 2026-10-12)

api_version stays "4". Both keys are additive on GET /health only (web_telemetry.c:7584). Read-only.

- `mods.mechfail`: `{present, host_verified, activated, families_started}`. Source: read-only probe of the named mapping F1C_MECHFAIL_CTL (web_telemetry.c:7329), magic 0x4D48464D only. present=false when the mapping is absent or the magic differs. Tier: FACT (struct, mechanical_failures.c:76-88), DERIVED (probe design, not yet live-read).
- `forecast`: `{source, tier, written_by, session, ty, available, fresh, file_mtime, session_start, start{...}, steps[{minute, conditions, ambient_temp}], truncated}` or `{source, tier, available:false, reason, ...}`. Source: <game>\LOG\weather.txt, read on the HTTP thread only (web_telemetry.c:7475). Tier: DERIVED. steps capped at 96, file capped at 64 KB. Only `available:true, fresh:true` is shown in the viewer. Schema: schemas/health.json (optional properties).

Not in the API: pit failures from MF (no field exists in MfHostCtl). The blue-flag cue is viewer-only (telemetry_viewer.js:2360) and adds no key.

Schema set after this change: schemas/version.json schema_set_sha256 DD625B7816D650B32B84078A7CE3FD86C8DF88147C66BD0DA1D2FBDAD8595A11.

## /weather (rc.11, 2026-10-12)

api_version stays "4"; the endpoint is additive on GET (read-only, same method allowlist as every GET). It publishes the session weather history ring for the Weather console page.

Shape (schemas/weather.json):

- `api_version`: "4".
- `session_generation`: capture generation of the ring; the ring is cleared when it changes (same rule as the traces).
- `session_type`, `track`: session context.
- `interval_s`: 10.0 — a row is appended when session time has advanced by at least this much, or sooner on a change.
- `capacity`: 2048 rows; `evicted`: how many oldest rows the bounded ring has dropped this session.
- `fields`: units (`st` s, `at`/`trkt` C, `onw`/`offw` 0..1, `tgf` raw).
- `tiers`: `{at CONFIRMED, trkt CONFIRMED, onw CONFIRMED, offw CONFIRMED, tgf DERIVED}` (RESEARCH_WEATHER_2026-10-12 section 1b).
- `samples`: array rows `[st, at, trkt, onw, offw, tgf]`; a missing value (`-900` sentinel or non-finite) is a JSON `null`. No new game memory is read: the values are the same ones /telemetry already publishes.

Cadence rules (web_telemetry.c `weather_update`): append when `st - last >= 10.0`, or when `onw`/`offw` changes by >= 0.01 or `at`/`trkt` by >= 0.1 C; never append while the session clock is unchanged (one paused frame is not a new sample). Output is bounded (`WEATHER_JSON_BYTES`); overflow uses the existing "response capacity exceeded" JSON error path.

Not in the API: rain intensity (native variable OPEN), a weather state enum (none verified), track condition float 0x7CF3C8 (known float, not captured by this server yet). The console shows these as OPEN rows on the Weather page (telemetry_viewer.js `weatherPaneKnowledge`).

Schema set after this change: schemas/version.json schema_set_sha256 6D9A93D667F05C18798D1EAB7B39F27C07DEB401F4DFEDE9954FAEC1BC86C32C.

## /track additive keys (rc.12, 4.4.0-rc.12, 2026-10-12, Batch 2l)

api_version stays "4"; additive on GET. The Weather console page draws the dry
and wet driving lines with the live wetness values.

- `gw`, `gww`: AIW header GrooveWidth / GrooveWidthWet in metres (0 = not read);
  map annotation only, read by the once-per-track AIW scan (sectors_update).
- `dl`, `wl`: dry / wet driving line polylines `[x,z]` per waypoint, computed as
  `wp_pos + wp_path[n] * wp_perp` from the live AI database (FACT file fields,
  DERIVED line; docs/52 section 4.6). Emitted only when every waypoint passes
  sanity (unit perp, offsets <= 100 m) and the array length equals `wc`; a
  truncated array stays valid JSON and the viewer refuses it. `wp_path[0]` is the
  dry line and `wp_path[1]` the wet line per the AIW file comments; one live
  endpoint is verified (docs/52), a full wet-session check remains OPEN.
- schemas/track.json gains the four optional properties; schema set regenerated:
  schemas/version.json schema_set_sha256 36ACDC0326B79B51464293A446F18262D8433A4D1528199C389076FFF3A59248.
  The same batch adds /weather `gdb` (optional): the track GDB Weather{} script
  parsed once per track - per session the start values (Conditions /
  track_wetness / on_path_wetness / off_path_wetness / ambient / tracktemp, raw
  file tokens) and every Minute=/Lap= step with the same token fields; bounded
  to 12 sessions and 32 steps per session with a `truncated` flag. Schema set
  after both additions: schema_set_sha256 83D081E90A6995D01F9105B1649F4A61D246320ADB52D0D0B2B176CFA3CFE182.

## 14. 4.4.0 all-cars traces: api_version 5 (shape change, additive; 2026-10-12)

Procedure of section 10 followed: schemas edited, `API_VERSION "5"`,
`schemas/update_version.py 5`. All changes are additive; clients that ignore
unknown fields keep working, and `f1c.trace/1` exports/imports are unchanged.

Owner rule 2026-10-12: every live car is traced all the time (not only the
selected A/B pair). The recorder keeps one ring per vehicle slot
(VirtualAlloc'd once per live car count, capped at MAX_CARS, ~3.6 MB per car at
TRACE_SAMPLES 3200; the old selected-pair mode remains as the fallback when the
allocation fails). TRACE_SAMPLES is 3200 = 2:40 at the measured ~20 Hz capture
rate (live rate 18.65-19.6 Hz; the old "20 Hz, 2400 = 2 min" comment was stale
and a 2:30 lap could overflow). Six completed laps are still kept per car.

- `/health` (schemas/health.json): new required `trace_mode`: "all" (store
  allocated, every car recorded), "selected" (no session store yet), or
  "selected_fallback" (the allocation failed; the recorder kept the old
  selected-pair mode). New optional `trace_bytes` (store size) and `trace_cars`
  (entries); the allocation is logged once with the same numbers.
- `/trace/status` (schemas/trace_status.json): new required `mode` ("selected"
  | "all"); in all mode `cars[]` lists one entry per vehicle slot (maxItems
  104) and `which` may be "" for cars that are not the selected A/B aliases.
  `samples_max` now reports 3200.
- `/trace/data`, `/trace/export`, `/trace/open` accept `car=<slot>` (0..103)
  alongside `car=A|B`; a numeric slot addresses that car's recorded ring
  directly, so any driver's completed laps and open lap are readable without a
  selection step. In selected-fallback mode a numeric slot resolves only for
  the recorded A/B pair; unknown/unrecorded slots keep the existing 404
  ("lap not recorded" / "no open lap") and invalid values answer 400. The
  identity pins (gen/rev) and the evicted/never-recorded distinction are
  unchanged; `which` in /trace/data may be "" for slot-addressed documents.
- Found and fixed while gating: `build_trace_data_csv_id`'s header format was
  missing its `rev=%u` conversion (13 args, 12 conversions since 4.3.0); the old
  `%c` form swallowed the shift, the %s form surfaced it as a crash in the
  retention fixture. The CSV header now matches the JSON header:
  `# f1c.trace/1 gen=%u rev=%u which=%s slot=%d src=live ...`.
- Viewer: any side (A-F) seeds its already-recorded laps and open lap by slot
  after a pick (GET /trace/status -> /trace/data + /trace/open); the browser
  ring cap follows TRACE_SAMPLES (3200). The /trace/select A/B pin is kept only
  for the fallback mode.

Schema set after API 5: schemas/version.json schema_set_sha256
DD803170D0338DA5DD8AF1F16372295ED1EEFA881BFFEFBC93EED7D4E1B3A271.
Gate: TELEMETRY_TESTS=1 PASS (46,390 checks; runtime cost PERFALL selected
p95 178-216 us vs all-cars p95 181-250 us on 24 cars; no doubling).

## 15. API 6 candidate (2026-10-12; native validation pending)

- `/telemetry` adds required numeric `pel`: the read-only player engine-life global in seconds. Engine Live uses this only for the player; AI uses its vehicle `lfe` countdown. This does not assert AI has the same global.
- A roster pointer change within the same broker generation retains recorded per-slot traces. A generation change clears them. The previous global clear on either event lost existing laps.
- Viewer refreshes trace status on each seed attempt, retries an empty seed, and preserves AI browser history across active/retired flag changes. Numeric slot trace addressing from section 14 is unchanged.
- Session Overview suppresses lapped gaps and blue-flag presentation in non-race sessions. Same-team unanimous tyre mapping is a derived fallback for custom player driver names.
- Mechanical Failures `/health` status establishes the mod's shared mapping presence/activation only. No Mechanical Failures or Pit Stop Failures failure-event feed is claimed.
- Evidence and pending native gate: [CODEX_LIVE_ALLDRIVER_TRACE_2026-10-12.md](CODEX_LIVE_ALLDRIVER_TRACE_2026-10-12.md).

API 6 schema set: `8E1A9C555C3FEDAA8238A1D87F8ED0D30F743842029546BDD5AEB0FBAFA96DDC`.
Viewer tests: 273 checks PASS; build/payload hash gate PASS. Full runtime gate and live reload pending game close.

## 16. API 9: recordings manager (batch 4, 2026-10-13; shape change)

Dated record: [STORAGE_V2_BATCH4_RECORD_2026-10-13.md](STORAGE_V2_BATCH4_RECORD_2026-10-13.md). Schemas: `schemas/recordings.json`
(`f1c.recordings/1`), `schemas/recordings_jobs.json` (`f1c.recordings.jobs/1`), `schemas/recordings_accepted.json` (202 bodies),
`schemas/health.json` (`/health` `storage` object). Contract version 9 (`schemas/version.json`, `api_version` 9).

### 16.1 Changes from API 8

| Change | Route | Notes |
|---|---|---|
| added | `GET /recordings` | Snapshot published by the storage writer with each index write (at most about 5 s old). Relative names only; no absolute path (M11). `loaded` is the current selection (null when none). |
| added | `GET /recordings/jobs` | The last eight archive or delete jobs (`pending`, `ok` or `error`); `moved` and `not_moved` hold names only. |
| added | `POST /recordings/load` | Selects one recording. Changes no file. An empty `recording` unloads. |
| added | `POST /recordings/archive` | Queues a job (202). Each item is a closed session; the verified LZMS copy is written next to it. |
| added | `POST /recordings/delete` | Queues a job (202). Files move to the Recycle Bin only (`SHFileOperationW`, `FOF_ALLOWUNDO`). |
| added | `/health` `storage` | `enabled`, `dir` (always `recordings`), `compression`, `recording`, `ring_drops`, `over_warn`, `recycle_bin`. |
| removed | `GET /sessions` | The 1.0.1 list (it returned an absolute `dir`). Now 404. |
| removed | `POST /sessions/delete` | The 1.0.1 hard delete (`ar_build_delete_json`). Now 405. No hard-delete route exists. |
| unchanged | `GET <endpoint>?session=<1.0.1 id>` | Review of 1.0.1 archives, as in API 8. |
| not built | `POST /recordings/import`, `GET /recordings/data`, `GET /recordings/laps|pits|incidents|trace` | Spec 7.9 and 8.5; answered 404 or 405. |

### 16.2 Guards on every POST route (1.0.1 pattern, spec 8.1)

1. The peer must be the game PC (`http_peer_is_local`); otherwise 403.
2. `X-Telemetry-Action` must equal the action (`load`, `archive` or `delete`); otherwise 403. A foreign page cannot send it without a preflight this server never approves.
3. `Content-Type: application/json`; otherwise 415.
4. Body of 1 to 4096 bytes (M8; spec 8.6 says 64 KB, this batch uses the stricter security-review cap); otherwise 413 or 400.
5. Flat JSON object only: strings without escapes, digits, booleans and lists of at most eight strings. Anything else is 400.

### 16.3 Bodies

- load: `{"recording": "<folder name>"}`. Response 200: `{"loaded": "<name>", "sessions": n, "note": "Loaded: ... Loading does not change any file."}`.
- archive: `{"sessions": ["<recording>/<session>", ...], "recycle_working": false}` (1 to 8 items). Response 202: `{"job": "archive-N", "kind": "archive", "state": "queued", "items": n, "recycle_working": bool}`.
- delete: `{"target": "recording" | "session" | "file", "id": "<name>", "recording": "<folder>" (session and file only), "confirm_bytes": n}`. The size is re-counted on disk; a difference is 409. Response 202: `{"job": "delete-N", "kind": "delete", "state": "queued", "items": n, "bytes": n}`.

### 16.4 Status codes

| Code | Meaning |
|---|---|
| 200 | load done; GET done |
| 202 | job queued (archive, delete); result via `GET /recordings/jobs` |
| 400 | body not a flat JSON object, or a required key missing or of the wrong type |
| 403 | not the game PC, or `X-Telemetry-Action` missing or different |
| 404 | unknown recording, session or file (also unknown 1.0.1 review id) |
| 409 | stale size (`confirm_bytes` differs), recording loaded (delete), session or recording open, session not closed |
| 413 | body over 4096 bytes |
| 415 | `Content-Type` not `application/json` |
| 422 | invalid name (separators, `..`, control or non-ASCII bytes), bad list count, bad target |
| 500 | response too large (never expected: the snapshot is capped at 256 KB) |
| 503 | storage disabled; LZMS not available (archive, spec 9.6 text); Recycle Bin off or unusable for the drive (delete, archive with `recycle_working`); job queue full |

### 16.5 Job result (`GET /recordings/jobs`)

`{"schema": "f1c.recordings.jobs/1", "jobs": [{"job", "kind", "state": "queued|running|done", "result": "pending|ok|error", "message", "items", "recycle_working", "bytes_working", "bytes_archived", "bytes_moved", "moved": [names], "not_moved": [names]}]}`.
A failed archive stops at the first error and leaves its `.partial` and the working copy as they were (spec 7.7).

### 16.6 Notes

- Schema set hash (API 9): `7A8DB190CA776AC15BAAEE263C8C1F293A4C54C2B32D710C6572D8EE1B1D4E41` (`schemas/version.json`).
- Known gap for the owner's live test: no review page for v2 recordings yet (`Load` selects only).
- Security: the M3 Host allowlist (F-03) is not in this batch; the write routes rely on the peer check and the header, as the 1.0.1 delete did. See the batch 4 record, open issues.

## 17. API 10: review index (R1-A, 2026-10-14; additive)

- `GET /recordings` session items gain `"review_id"` (`v2_` + 16 lowercase hex digits, FNV-1a 64 of `folder/stem`). Only closed and recovered sessions have it. The open session never has one.
- `GET /recordings/review?session=<review_id>` (local PC only, 403 otherwise; 400 without `session`; 404 for an unknown or deleted id, or an id that is not `v2_` + 16 hex). Answers `f1c.recordings.review/1` (`schemas/recordings_review.json`): `{"schema","session","state":"none|building|ready|error","laps","cars","lap_len":null,"reason":"SV2_R_*|null","built_ms"}`. `state` `building`: ask again. `state` `none`: a build could not start because another session is being built; ask again.
- Any other route with a `v2_` id answers 404 "review data for this recording is not built yet" until the R1-B adapter (`/laps`, `/trace/*`, `/telemetry`) lands. The 1.0.1 routes never see a `v2_` id.
- Schema changes: `trace_data.json` `samples` cap 2400 to 3200 (`TRACE_SAMPLES`), `source` and `lid.src` accept `recording`; `trace_status.json` gains top-level `source`; `laps.json` gains `source`, `lap_len`, `validated` (optional).
- Schema set hash (API 10): `792C2923C65926A30D86E6EA937940C636A1A4B60A461419B94D25F4C04845E6` (`schemas/version.json`). Section 16.6 is the API 9 hash and stays as history.
- Not in this section: `lap_len` in META (R1-B), the trace adapter (R1-B), and the viewer (R2).

### 17.1 API 10: review adapter for recorded sessions (R1-B, 2026-10-14; additive)

- `GET /laps`, `/trace/status`, `/trace/select`, `/trace/data?car=1000+slot&lap=N` and `/telemetry`, each with `?session=v2_<16 hex>`, answer from the review index (state `ready`). Documents are the 1.0.1 shapes with `"source":"recording"`. `/laps` adds `lap_len` (null) and `validated` (false). `/trace/status` adds top-level `source`.
- Trace samples carry the 19 short keys of `schemas/trace_data.json`. `rx` is always null. `f` carries the bits of the channels the block stores for that car. `count` is the lap's tick count. The sample list stops at 3200 with `"truncated":true`.
- Lap numbers are the raw lap counter. A lap time is the game lap time of the index (EVENT `last_lap_s`). A lap without a game time or a start (the first partial lap) is not listed. `lid` is `{gen 0, rev 0, slot, lap, src "recording"}`.
- `/telemetry` review frame: the live keys with API contract sentinels (`-999` for unavailable wheel arrays, `-1` for unset ids, `0` otherwise). `trk` and `ty` come from the file header. `lapm` is 0 (unknown; lap length is not in v2 META, R1-A D-2; the schema minimum is 0, so the spec value -1 was not used). `c` has one entry per car seen in the file: `id` = slot, `n` = `Car <slot>` (v2 has no roster), `p` = last stored race position, `l` = last lap. The optional `review` object is `{session, stem, ticks, end_t}` (`schemas/telemetry.json`).
- Not recorded (404, `{"error":"not recorded in this recording"}`): `/pits`, `/incidents`, `/weather`, `/track`, `/driver`. The index keeps lap events only, so pit and incident events are not answered yet. `/trace/open`, `/trace/compare` and `/trace/export` answer 404 `{"error":"not available for a recorded session"}`.
- Gates, in order: an unknown or deleted id is 404 `{"error":"unknown or deleted session"}`. A session still recording is 409 `{"error":"session is still recording"}` (its id is kept inside the server only, never listed). A build that has not finished is 503 with `Retry-After: 1` and `{"error":"review index is building"}`; the request starts the build when none runs and never waits for it. A failed build is 404 `{"error":"review index could not be built"}`. Every route is local PC only (403 otherwise).
- A `v2_` id on any other path (the console page `/?session=v2_...`) falls through to the page. The 1.0.1 review guard never sees a `v2_` id.
- Schema set hash (API 10 with R1-B): `0A96E4919E587F0FC75686F642CAB69A6494D540B8DB0BEAA9FBE0EB0C149F27` (`schemas/version.json`). It replaces the R1-A value in section 17 because `schemas/telemetry.json` gained the optional `review` object.
- **1.2.5 (2026-10-18, additive, API stays 10).** Supersedes the listing rule above. A lap is listed when its start was seen and it has the game lap time, or (untimed: out lap, in lap) at least 20 s of sim time. `/trace/status` lap `flags`: 1 timed, 3 untimed (shown like a pit lap); `t` is the game time, else the line-to-line time. `/trace/data` adds `partial` (false timed, true untimed), `lap_time_src` (`game`/`derived`) and `derived_lap_time`; `flags` 1/3. A lap EVENT time equal to the car's previous one is ignored (untimed in lap). `/laps` still lists timed laps only. Schema set hash: `0DB50F859B4A9B76D1ECED15F97DAF9DDDB4F02C9CD2E2E42F7A2BC18BDA6AE6`. Record: `DVOA_FIXES_1.2.5_2026-10-18.md`.
- Not in this section: pit and incident answers from EVENT records, lap length in META (`storage_writer.inc` is not in the R1-B edit list), the viewer (R2).

## 18. API 10: REPLAY and TAPE review routes (2026-10-14; additive, unreleased)

Scope: recorded (v2) sessions only, opened with `session=v2_...`. Specs: `REPLAY_SPEC_2026-10-14.md` section 2 (2.1, 2.2) and
`TAPE_SPEC_2026-10-14.md` section 2 (2.2 to 2.5) with the contract clarification of 2026-10-14 (review `/pits` and
`/incidents` items carry `t`). API stays 10; the schema set hash is in `schemas/version.json`.

- `GET /replay/info?session=` (schema `replay_info.json`, `f1c.replay.info/1`): `t_first`, `t_last` (sim seconds, from the
  DATA blocks), `hz` (measured: ticks / span), `cars[]` (`slot`, `name`, `team`, `num`, `role` = player|remote|ai from
  channel 98, `laps`), `events[]` (lap, pit, incident EVENTs, sorted by `t`; cap 4000 then `events_truncated`), `outline[]`
  (track_x/track_z of one timed lap, at most 600 points). Driver names: the newest `driver` EVENT of the slot, else
  `#<race number>` when channel 97 is known, else `Car <slot>`.
- `GET /replay?session=&t0=&t1=` (schema `replay.json`, `f1c.replay/1`): rows of 12 columns `t, x, z, spd, pos, lap, pit, gear,
  thr, brk, dist, fin` at 5 Hz per car (0.2 s), the last tick of the window always listed, `null` for a channel the file
  does not hold. `t1 - t0` at most 30 s (400 `window too long`), `t0 < t1` (400), both finite (400); clamped to the span.
- `GET /replay/frame?session=&t=` (schema `telemetry.json`, optional `review` and `tape` objects): one live-shaped
  `/telemetry` frame at sim time `t` (clamped to the span). `tape` = `{session, t, t_first, t_last}`. `sample_sequence` is
  the approximate tick index, `sample_age_ms` 0, `session_generation` an FNV-1a hash of the review id. Reads only the
  blocks around `t` (2 s), never the live state.
- `GET /track?session=` (schema `track.json`): `{"wc":N,"pts":[[x,z],...]}`, `wc` = number of outline points.
- `GET /pits?session=` (schema `pits.json`): live item fields plus `t` (sim seconds of the entry; the live document has no time).
  Fields the file does not hold are 0 (`ti`, `to`, `boxdur`, `reqdelay`, `rfw`, `rrw`); `li`, `lo`, `fi`, `fo`, `dmg` are read at
  the entry and the exit.
- `GET /incidents?session=` (schema `incidents.json`): live item fields plus `t`. `tk` = approximate tick count before the
  event (DERIVED); `ty` = `limits` or `contact`; `a` = partner slot (contact) or track-limit count (limits); `b` = 0.
- `GET /driver?session=&id=<slot>[&t=<s>]` (schema `driver.json`): live shape. Laps are the lap EVENTs up to `t` (`t` = game lap
  time; `s1`, `s2` = 0; `s2d`, `s3` = -1; `p`, `f` read at the lap end). Pits as `/pits` without `id`, `n`, `t`. Default `t` = the
  last tick. 400 for a bad `id`, 404 for a car the file does not hold.
- `GET /weather?session=`: 404 `weather is not recorded in this recording`.
- Gates (all review routes): 404 unknown or deleted id, 409 still recording, 503 index building (Retry-After), 400 bad arguments.
- File change (writer, new recordings only): EVENT `{"type":"driver","t","car","name","team","num"}` when a slot is first seen in
  a session and whenever its name, team or number changes. Older files have none (names fall back as above).
- Schema set changes in this section: `pits.json` and `incidents.json` gain the optional item `t`; `telemetry.json` gains the
  optional `tape` object; `replay_info.json` and `replay.json` are new.
- Not in this section: standalone player (game closed), weather recording, `/laps` items (no per-lap `t`: each item is a
  car with a list of game lap times; the contract clarification asked for it only if cheap; not added), a cache of the
  info document (each request reads the file: 46 ms on a 58 MB real file).

## TAPE2 clarification (2026-10-16): `/trace/data` `car` and `team`

- `car` (string, schema `trace_data.json` unchanged): the car/team display name of the traced car.
  Live (`source:"live"`): the game's car name for the slot (web_telemetry.c ~3147). Recording (`source:"recording"`):
  the team name from the review name table, the same `cn` the other review documents use (storage_api.inc ~1088;
  driver EVENT team, else season .veh by race number). `""` when unknown, and always `""` for imported laps.
- `team` (integer): numeric team id. Live: broker team identity. Recording: always `0` (not stored in the file).
  Clients must not treat `team` as a name or `car` as an id.
- Before TAPE2 the recording document sent `car:""`; clients that already accept an empty string need no change.
