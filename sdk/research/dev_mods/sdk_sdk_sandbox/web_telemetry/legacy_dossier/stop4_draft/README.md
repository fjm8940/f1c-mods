# STOP 4 draft — workbench UI/JS (NOT WIRED)

Date: 2026-10-09. Status: STOP 4 preparation draft. Not served by
`web_telemetry.c`; not installed; no product behavior changed.

The owner condition for STOP 4 is that `web_telemetry.c` must not be edited
until parallel port task A has landed and been reviewed. This directory holds
the parts of STOP 4 that do not touch it: the draft workbench UI/JS, its
synthetic feed and its tests. The C decoder port lives in `../c/`; the test
plan in `../STOP4_TEST_PLAN_2026-10-09.md`; the preparation record in
`../STOP4_PREP_2026-10-09.md`.

## Files

| File | Purpose |
|---|---|
| `../../workbench_ui.js` | **Moved to the component root in STOP 4** as the product renderer served at `/workbench.js`. Model + Canvas renderer: A/B sides (player or any AI), Chassis Overview (track/driving line, speed vs distance, G-G scatter, RPM vs distance), Inputs view, live partial-lap drawing, LIVE/PARTIAL/COMPLETED/FROZEN states, freeze/resume, lap-crossing history + reference, mid-lap selection, linked distance cursor with the viewer's DM#14 delta interpolation, zoom/pan/reset, missing-data reasons. Consumes the existing `/telemetry` SSE frame shape, `f1c.trace/1` exports, `/trace/open` and `/trace/import`. |
| `mock_data.js` | SYNTHETIC feed in the `/telemetry` frame shape (22 cars, one capture pause, a lap crossing per car). No game memory is read. |
| `workbench.html` | Offline review page (synthetic feed; loads `../../workbench_ui.js`). The product page is served at `/workbench` by the DLL. |
| `parity_test.js` | Node tests for the rc.11 old-viewer parity renderer: 23 embedded layouts and Chassis Overview geometry, channel map with unavailable reasons, filter, dtime, DM#14 lookup, lap full/fastest rules, import lap entry, note dedup, ticks ([record](../../FRONTEND_PARITY_2026-10-09.md)). |
| `rpm_test.js` | Node tests: RPM conversion and label guard, validity gating, segment breaks, crossing rotation, ring bounds, freeze/resume, selection reset, no-double-conversion trace import, DM#14 interpolation, recorded budgets. |

## Review

```powershell
# tests (Node.js)
node mods/sdk_sandbox/web_telemetry/legacy_dossier/stop4_draft/rpm_test.js
node mods/sdk_sandbox/web_telemetry/legacy_dossier/stop4_draft/parity_test.js

# visual review (open in a browser; synthetic by default)
start mods/sdk_sandbox/web_telemetry/legacy_dossier/stop4_draft/workbench.html
```

## RPM rule (review condition, implemented and tested)

The live `/telemetry` field `c.r` (and `c.mr`) is engine angular speed in
rad/s (finding `engine-rpm-live-scale-20261009`). `radPerSecToRpm()` multiplies
by 9.549296585513721 and every RPM label goes through `formatRpm()` /
`formatRpmValue()`; a raw rad/s value is never labeled RPM. A converted trace
channel (`rpm` in a future `f1c.trace/1`) is stored by the recorder already in
RPM, so `loadTrace()` must not convert it again (asserted in `rpm_test.js`).

## Budget note recorded here (review condition)

Measured `/telemetry` frame 2026-10-09: 31,016 B for 22 cars at 10 Hz SSE
≈ 310 KB/s ≈ 2.5 Mbit/s per client; 16-client cap ≈ 4.96 MB/s aggregate worst
case — fine on loopback/LAN, notable for remote Tailscale viewers. A/B trace
payloads stay bounded by the existing 524,288 B `/trace/*` responses and the
≤1000-point compare grid; the draft asserts those constants in its test.

## What STOP 4 delivered (rc.5, integrated 2026-10-09)

1. `workbench_ui.js` served by the product at `/workbench.js` and mounted by
   the `/workbench` page on the live SSE feed (single product; no second
   server). The offline review page here still loads the same file.
2. Trace record additions (`rpm` converted at capture, `gear`, `tx/tz`,
   64→80 B) with `/trace/data`, `/trace/export`, `/trace/compare` and the new
   `/trace/open` partial-lap route.
3. `POST /trace/import/legacy` wired to the C decoder (`legacy_formats.c` in
   the component root); the workbench Import button posts `.lap`/`.spt` files.
4. All gates run: C 70/70, Node 21/21, canonical build 41,536 checks +
   HTTP/Node gates, SDK gate PASS; candidate `696E83CF…`. Native/browser
   acceptance is STOP 5.

The `?live=` parameter in `workbench.html` remains a convenience for review;
the product page uses same-origin endpoints.
