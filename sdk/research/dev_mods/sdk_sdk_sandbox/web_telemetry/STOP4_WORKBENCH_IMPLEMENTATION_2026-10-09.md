# STOP 4 implementation — legacy workbench integrated into Web Telemetry 4.2.0-rc.5 (2026-10-09)

Status: **STOP 4 READY**. Base: accepted rc.4 (configurable port + native
crossing fix, both preserved). Candidate DLL
`%TEMP%\f1c-web-telemetry-4.2.0-rc.5\web_telemetry.dll` SHA256
`696E83CF5F9ADA0C73AAE077A5A1E8FAAB35932D73EF1435CD2E5A9F43A46D41`
(two consecutive builds identical). NOT installed; the running DLL is still
rc.2. Assignment: `DEEPSEEK_LEGACY_TELEMETRY_ASSIGNMENT_2026-10-09.md`;
design: `legacy_dossier/STOP3_LEGACY_TELEMETRY_2026-10-09.md` (STOP 3R,
accepted); gates: `legacy_dossier/STOP4_TEST_PLAN_2026-10-09.md`;
owner additions this pass: reuse parallel C's formulas
(`legacy_dossier/pcode_semantics/PCODE_SEMANTICS_2026-10-09.md`) and the
mph velocity unit. All work is offline/synthetic; no game was started, no
file installed, nothing deleted.

## 1. What changed

### 1.1 Decoder integration (import path)

- `legacy_formats.h` / `legacy_formats.c` / `legacy_formats_test.c` moved from
  `legacy_dossier/c/` to the component root (hash-equal moves, same volume);
  `legacy_dossier/c/README.md` retained as the preparation record with a
  pointer. The decoder is now compiled into both the DLL and the test harness
  by `build.bat`.
- New memory entry points `lf_decode_lap_mem` / `lf_decode_spt_mem` (private
  copy; the caller keeps ownership of the POST body).
- `POST /trace/import/legacy?car=A|B` (bounded body ≤ 16 MB, `Content-Length`
  required, 400 on malformed/incomplete/oversized, 405 elsewhere): sniff
  (`CHECKSUM=` → .lap, else .spt), decode, convert to an `f1c.trace/1`
  document, cache it per side; `GET /trace/import?car=A|B` serves the cached
  document. Conversion runs on the HTTP thread; no game state is touched.
- Imported documents carry `"source":"legacy"`, `"format"`, `"partial"`,
  `"file":{"bytes"}`, `"conversions":[…]`, and (when >2400 samples) an explicit
  `"reduction"` with stride; stride-reduced samples carry `TRF_SEGMENT` so the
  UI never draws a false continuous line.

### 1.2 Trace additions (recorder + surfaces)

- `trace_sample_t` 64 → 80 B: `rpm` (display RPM), `gear`, `tx/tz`
  (coordinatesInTrack). New validity bits `TRF_RPM/TRF_GEAR/TRF_TRACK`; the
  recorder fills them from `c->rpm` × `LF_RPM_FACTOR`, `c->gear`,
  `c->track_x/z` on the game-thread capture path (no new native reads).
- `/trace/data`, `/trace/export` (CSV columns appended: `rpm,gear,tx,tz`),
  `/trace/compare` (new `rpm` grid channel) and `/trace/status`
  (`unavailable[]` now names rpm/gear/track when unseen) all carry the new
  channels; data/open documents gain `"source":"live"`, `"open"` and a
  `"truncated":true` marker when the response bound cut the sample list.
- New `GET /trace/open?car=A|B`: serves the open lap as it is driven
  (bounded by TRACE_SAMPLES; 404 when no open samples), so the workbench draws
  partial laps immediately.

### 1.3 RPM label corrections (owner condition)

- Page gauges (Pit Wall + focus): `fc.r`/`fc.mr` are converted with
  `RPMF = 9.549296585513721` before any 'RPM' label; the engine-angvel
  cross-check and the Full Debug `RPM` dump convert too.
- Race logs: `"eng"` now writes `"rpm"` as converted display RPM and keeps the
  raw value as `"rpm_rad_s"` (schema note below). The live `/telemetry` keys
  `r`/`mr` remain raw rad/s for compatibility; the ledger and UI convert.

### 1.4 Workbench UI served by the product

- `workbench_ui.js` (the accepted draft renderer, moved to the component root)
  is served read-only at `/workbench.js` from beside the DLL (bounded ≤1 MB,
  cached once; 404 with capture unaffected when missing). `GET /workbench`
  serves the product page that mounts it on the live SSE feed, `/track` and
  the new import route; the dashboard tab bar links to it.
- UI behaviors kept from the accepted design: A/B player-or-AI sides with
  colors/labels, Chassis Overview (track/driving line, speed vs distance,
  G-G scatter, RPM vs distance), Inputs view, live partial-lap drawing with
  segment breaks, LIVE/PARTIAL/COMPLETED/FROZEN states, freeze/resume while
  capture continues, crossing keeps history+reference, mid-lap selection
  reset, linked cursor, zoom/pan/reset, missing-data reasons.
- Deployment note: the private package must include `workbench_ui.js` beside
  the DLL; without it `/workbench.js` 404s (documented in
  `PRIVATE_TEST_README.md`).

### 1.5 Parallel C formulas reused (owner addition)

From `PCODE_SEMANTICS_2026-10-09.md`:

- **Velocity is mph** (DM#8 constant 2.2371365): `.lap` import converts
  `velocity / 2.2371365` → m/s. Numeric check on the real reference lap:
  sample 600 reads 176.993 mph = 79.1 m/s. This corrects the STOP 3R
  conversion table line "velocity → m/s"; recorded in `"conversions"` in the
  imported document, never silent.
- **Distance (a)**: the `.spt` import derives distance and speed with the
  viewer's own DM#8 formula (cumulative `sqrt(dx²+dz²)` over x/z, y unused;
  `dt` floor 1e-7; `dt > 0.01` else hold; the mph factor cancels to m/s).
  Synthetic parity fixture: 3-4-5 triangle → 5.0 m, 50.0 m/s.
- **Delta (c)**: `workbench_ui.js` `timeAtDistance()` implements DM#14 exactly
  (scan while distance ≤ target; `f = (target−d[k])/(d[i]−d[k])` when
  `|Δd| > 0.0001`, else 1; `t[k] + f·(t[i]−t[k])`); the cursor readout shows
  Δ(tB−tA) at the linked distance. Node tests cover linear, degenerate and
  two-lap cases.
- **Tyre/damper (f)**: the FileReading#0 formulas (temp −273, crown/camber,
  ride-height averages in mm, damper velocity per Δt) are cited in the
  imported document's `"formulas"` field; the `f1c.trace/1` model carries no
  temperature/damper channels, so nothing is re-derived — the `.lap` channels
  are already the viewer's own derived output.
- **Acceleration is g** (correction found while wiring the formulas; parallel
  C open item 7): the `.lap` lat/long/vert accel channels are g, converted
  `×9.8` to the trace's m/s². Numeric check on the real lap: max lateral
  3.32 g, at rest vertical ≈1.0 (m/s² would be ≥9.8 at rest). Recorded in
  `"conversions"`.
- **Yaw**: `.lap` yaw is degrees → radians (`×π/180`, recorded); `.spt` yaw is
  already radians (as stored).

### 1.6 Version and metadata

rc.4 → rc.5 per MOD_STANDARD section 5 (bytes changed): `TELEMETRY_VERSION`,
page footer, `build.bat` (build dir + decoder sources + sidecar copy),
`f1c_mod.json`, `mod.json`, `web_telemetry.dll.mod.json`; tests assert rc.5.
No port or crossing behavior was changed (verified by the retained rc.4
gate results, all still passing).

## 2. Verification (all gates run 2026-10-09, this machine)

| Gate | Result |
|---|---|
| G1 C decoder suite (component root) | **70 checks, 0 failures** (real torn spt + real lap + synthetic/hostile + RPM) |
| G2 Python parity suite | pass (unchanged; both suites assert the same real-file facts) |
| G3 Node draft suite (`legacy_dossier/stop4_draft/rpm_test.js`) | **21 passed, 0 failed** (adds DM#14 cases) |
| Canonical build `TELEMETRY_TESTS=1` | runtime **41,536 checks PASS** (423 new: trace channels, open lap, import, refusals); HTTP/Node gates PASS including workbench page/sidecar, open-lap serving, import POST/cache/refusals, served-sidecar Node RPM/DM#14 test; port fallback/disabled/ini gates still PASS |
| G5 budgets | `/trace/*` bounded at 524,288 B (truncation marker added); compare ≤1000 pts; 80 B/sample → A/B ≤2.7 MiB; import body ≤16 MB; SSE remote cost unchanged (31,016 B frame ×10 Hz ≈310 KB/s ≈2.48 Mbit/s per client, 16-client cap) |
| G7 full SDK gate | **PASS** — all canonical SDK tests passed; regenerated artifacts unchanged (7 files) |
| Workspace validator | same 2 pre-existing errors (`.claude`, `plans`), 0 warnings |
| Reproducibility | two consecutive builds → identical DLL SHA256 (above) |

Live/native acceptance (G6 browser-in-game, owner install, native crossing
retest on rc.5) is **pending owner action** — STOP 5. Browser screenshots of
the synthetic page remain part of STOP 5's acceptance pack; the offline page,
HTTP and Node gates passed here.

## 3. Changed files and hashes (after-state)

| File | SHA256 |
|---|---|
| `web_telemetry.c` | `8F6092B2C673AFCFAEBFB407953909960B1E2C3A34B8C5CC0353033CECD21175` |
| `legacy_formats.h` (moved from `legacy_dossier/c/`) | `5F18B7A4DA2A54544E292E4B4ACCF99F03245246F9F6BEDF429B26F38E5C721B` |
| `legacy_formats.c` (moved; mem variants added) | `F4CDF9D25EA0BA9B20639B3996A3F83DE5C10D3FA6B383B77D747777169AD60F` |
| `legacy_formats_test.c` (moved, unchanged bytes) | `193AC3F5F2FD7718CD618BFE24DA76CE7EE3FBA9ED1C58115C4F64148FE89E3C` |
| `workbench_ui.js` (moved from `stop4_draft/workbench.js`; product patches) | `4588D5831118BDA6BB3ED7ED6B087D442249559970EB1ECE844D13665408A3C5` |
| `runtime_test.c` | `D401BAA2807B5DCD85A3C9A265C043CB9C45E559E101BE1AF6AF2D0E51D76D61` |
| `test_telemetry.py` | `37A6A011B56C73B5C91FCC324C58E9F8BD2F6ADA270A9EB715055AB3004EFEEA` |
| `build.bat` | `EECDF0852A7174E8E687530D328B8B000C116AEF7643225AEE6E54625B6B6675` |
| `f1c_mod.json` | `C0399DA68E1A416C30B74B6C9403C9C23E97580AC7144984607DEC3E5B388F25` |
| `mod.json` | `B973CD9AC0BC521DF05EE5BE299F5D16911DD78E408921163BE035F21790E522` |
| `web_telemetry.dll.mod.json` | `427195DC1278C2301C2D0DACCEAB621069CA8363DCBAF0C7E0D2489B8EBCBBD7` |
| candidate DLL | `696E83CF5F9ADA0C73AAE077A5A1E8FAAB35932D73EF1435CD2E5A9F43A46D41` |

Before-states of every edited existing file:
`workspace_organization_audit/WEB_TELEMETRY_STOP4_BEFORE_2026-10-09/` (+ this
record's changed-file map). Nothing was deleted; the four moved files were
verified hash-equal after the move.

## 4. Notes and pending items

- Race-log schema: `"eng"` now has `"rpm"` = converted display RPM and
  `"rpm_rad_s"` = the raw value (previously `"rpm"` held the raw rad/s).
  Offline consumers of old logs should read the new key for display RPM.
- Player steering (`P+0x000`/`P+0x040`) stays OPEN with the next-race
  read-only check (STOP 3R section 8); rev-limit band cross-check still
  pending next race.
- STOP 5 (not started): owner closes the game, installs the exact rc.5
  candidate through the guarded route, native full-weekend acceptance of the
  crossing fix + workbench/import live behavior, browser screenshots,
  manifest/checkpoint finalization, packaging note for `workbench_ui.js`.

## 5. Wasted-effort check

- One assertion error and one refused-fixture error in the new tests were
  caught by running the suite before finalizing (compare `rpm` array starts
  with nulls before the first sample distance; a truncated lap that still
  decodes complete samples is not a refusal case).
- The rc.4 page-footer version string was missed on the first version bump and
  caught by the HTTP gate — the gate did its job rather than a manual scan.
- The exact 1803.7 rad/s product was already corrected in the prep pass; no
  re-derivation was needed here.
- `web_telemetry.c` was extended by targeted edits after the earlier mapping;
  no full re-reads.

## 6. Documentation gate

This record is indexed from `README.md`; the test plan, decoder README, draft
README and manifest are linked; facts (test counts, hashes, gate results),
design decisions (conversions, caching, sidecar) and pending owner gates are
separated; the parallel C formula reuse is cited by section. Paired findings
entries and the SDK checkpoint carry the durable summary; UTF-8 edited lines
were re-read; no credentials or vendor bytes entered the documentation.

STOP 4 READY - check DS
