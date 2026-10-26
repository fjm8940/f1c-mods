# Frontend to old-viewer parity — rc.11 candidate (2026-10-09)

Owner request: the handoff task "frontend to old-viewer parity"
([CLAUDE_HANDOFF_2026-10-09.md](CLAUDE_HANDOFF_2026-10-09.md)). Implemented by Claude.
Pipeline stage: implement → verify. **Built, offline-verified, INSTALLED 2026-10-09 (owner-authorized).**
Human validation is still pending (live look against the f1_telemetry.exe
screenshots).

## Scope and non-goals

- In scope: `workbench_ui.js` sidecar only, plus the SHA pin and version bump
  the sidecar contract requires.
- No capture, endpoint, schema or contract change. `api_version` stays 3.
  Read-only. No game writes.
- Not done (deferred): server-recorded laps (`/trace/status` + `/trace/data`)
  in the lap strip. The strip shows laps this page recorded since it opened,
  plus imports. Also deferred: the plan-view bitmap, the multiplayer spectator
  and the live video panel.

## What changed (`workbench_ui.js`, rc.5 → rc.11)

| Handoff item | Implementation |
|---|---|
| 1. Full-screen 2x2 Chassis Overview | New layout engine. The page fills the window height, and every graph is positioned by its INI normalized rect. The default layout `overview.ini` gives: track map at top left (x/z, fitted, aspect kept, `NOGRID NOXAXIS NOYAXIS`), G-G at bottom left (`lat accel` vs `long accel`, `DOTS NOLINES`, symmetric equal axes in g, 1-2-5 grid, labelled axes), velocity at top right (km/h) and engine RPM at bottom right. Graphs have grids, y tick labels, x tick labels in metres, and a unit on the last tick. Canvases are sized by devicePixelRatio. |
| 2. Layout dropdown, 23 layouts | `gen_workbench_layouts.py` parses every `Telemetry/Layouts/*.ini` with the p-code grammar (LAYOUT_GRAMMAR_AND_AXIS_2026-10-09.md) and embeds the result between `LAYOUTS:BEGIN/END`. That is 23 layouts and 108 graphs, in viewer order: numbered titles first, the two viewer-state files last. `--check` reports a stale block. Each legacy channel maps to a live/import source through `channelInfo()`. A channel with no source draws an "Unavailable" panel with a plain reason, and the reason is also listed under the graphs. Flags are honoured: NOGRID, NOXAXIS, NOYAXIS, DOTS, NOLINES, MINORGRID, WIDELINES. `TITLE=` is honoured, and `filter[ch,N]` is a moving average over N samples that resets at gaps. |
| 3. Lap strip | One row per side. It shows the open lap (live/frozen), completed laps with times (newest first) and imported laps. The fastest full lap is purple. Partial laps (started more than 150 m from the line, or with a gap) are labelled `partial` and are never fastest. Click a lap to show it; click ⚑ to make it the reference, which draws as a dashed grey overlay on series and map and is used by dtime. Lap times come from 10 Hz samples and are marked `≈` (±0.1 s); the Laps tab keeps the official times. |
| 4. Inputs as separate traces | Comes from the `chassis_inputs.ini` layout: throttle, brake, steering, gear and clutch each get their own graph. Steering shows the backend's `steerwhy` reason for the player. Clutch is unavailable. |
| 5. Cursor / zoom / freeze / delta | One linked cursor across every graph: a vertical line with value dots, a ring on the G-G plot and a white dot on the map. Hovering the map also sets the cursor. All series graphs share one x zoom: wheel zooms, drag pans, double-click or **Reset zoom** resets. The map has its own wheel/drag/double-click. Freeze A/B is kept. The readout shows `Δ t2−t1 (B−A)` using DM#14 interpolation, with each lap timed from the first distance both laps share. The old version subtracted absolute session times. |
| 6. Notes | Notes are de-duplicated and counted (`(×N)`) and written in plain language. Per-sample codes stay machine-readable in `sample.missing` for tests. An empty B is explained ("pick a car in the B menu, or import a lap file"). Unavailable channels in the current layout are listed once each. |

Other changes:
- The x axis toggle is **distance (m)**, the default, or **lap time (s)**,
  and it persists in localStorage. The legacy `time` x channel is drawn
  against lap distance, matching the owner's screenshot (the grammar record,
  section 2, says distance is reconstructed by the viewer).
- Imports go into B, or into A when A is empty. The legacy POST now sends
  `Content-Type: application/octet-stream`.

## Channel source map (fact vs. inference)

| Legacy channel | Source | Tier |
|---|---|---|
| velocity, engine rpm, gear, throttle, brake, lat/long accel, x/z | live `/telemetry` (validity bits) and trace/import | contract v3 |
| steering | AI live; the player's is unavailable and shows the backend reason | contract v3 |
| vert accel, yaw, tyre wear | trace/import only (`gv`, `y`, `tr`). Live panels say so | contract v3 |
| dtime | client computation against the reference lap (DM#14 formula) | DERIVED (viewer parity of the reference choice is unverified) |
| clutch, tyre temps, wheelspin / rear wheel speed differential, ride height, dampers, suspension, air/track temp, rain, dampness | none. Unavailable, with a reason | — |
| plan view (`BMP=planview.jpg`) | not served. Unavailable | — |

Inference to confirm with the owner's screenshots:
- **Map orientation**: +z is drawn up.
- **G-G axis assignment**: lateral on x, as in the INI.
- **Units**: velocity in km/h and G in g (÷9.8, the same factor the import uses).
- The old viewer's G-G range is the asymmetric -6..6 / -2..4. The handoff
  asked for symmetric axes, so the workbench uses symmetric axes.

## Release mechanics (rc.10 → rc.11)

Version strings were bumped in `web_telemetry.c` (`TELEMETRY_VERSION`, header
note), `build.bat`, `mod.json`, `f1c_mod.json`, `web_telemetry.dll.mod.json`,
`runtime_test.c` and `test_telemetry.py`. `WORKBENCH_UI_EXPECTED_SHA256` was
updated, and `VERIFICATION_MANIFEST.json` was refreshed (version, payload dir,
dll/payload/sidecar/source hashes, rc.11 note).

The manifest's `live_check.py` hash was stale: it predated the earlier
RPM-tolerance fix. It is now synced.

The served-sidecar Node test in `test_telemetry.py` now also asserts the 23
layouts, Chassis Overview first, and an unavailable clutch.

| Artifact | SHA256 |
|---|---|
| workbench_ui.js | `7353B8EBDEFA2CD7494E91BDAF25844BF1C3C425B93E5668D3FD4EA9666C1E6D` (95,635 B) |
| web_telemetry.dll | `465DB2E93291F6A90C142609CEAA5D5D9B1E8AFA1CE44D3FAB8804BC397A517F` |
| web_telemetry.dll.mod.json | `8C712EDB80F07EF4D12E57753CC797BFF1FB2E100563AFE3AD8060431074F099` |
| web_telemetry.ini | `26F66E10…8001` (unchanged) |

Payload: `%TEMP%\f1c-web-telemetry-4.2.0-rc.11\payload`.

## Verification (2026-10-09)

- `node legacy_dossier/stop4_draft/rpm_test.js`: 21 passed, 0 failed. One
  assertion was updated for the plain-language selection note.
- `node legacy_dossier/stop4_draft/parity_test.js` (new): 17 passed, 0 failed.
  It covers layouts, the overview geometry, TITLE/flag parsing, the channel
  map, unavailable reasons, filter, import-only channels, dtime, DM#14
  lookup equality, lap full/fastest rules, the import lap entry,
  freeze-with-shown-lap, note dedup/plain text and ticks.
- `TELEMETRY_TESTS=1 build.bat` passed. Results: sidecar byte-exact, payload
  gate OK, 41,631 runtime checks, HTTP/contract (api_version 3)/endpoint
  audit, served workbench Node test, port fallback, missing sidecar, SSE,
  robustness, soak and fuzz (0 crashes). Two consecutive builds gave the same
  DLL hash.
- Synthetic page `legacy_dossier/stop4_draft/workbench.html` (served on
  127.0.0.1:8765 through `.claude/launch.json` `workbench-synthetic`) was
  checked visually at 1600x900:
  - Chassis Overview 2x2, Driver Inputs (separate traces, clutch
    unavailable), Weather (4 unavailable panels) and Time Diff
    (needs-reference message) all render.
  - The linked cursor shows on every graph and on the map.
  - The Δ readout appears, and the reference overlay draws dashed.
  - It also renders correctly under the product page's `#app` CSS.
- The synthetic feed reports many "gaps". This comes from the mock itself:
  its distance is computed as `speed × t`, which runs backwards when speed
  falls. It is not a renderer defect, and live data was not affected.
- **Not verified:** live game data and a side-by-side check against the
  old-viewer screenshots, which have not been received yet.

## Install / restore (not executed)

Installing needs the game closed and the owner's go-ahead. Copy the 4
payload files over `plugins\lightloader\`. To restore rc.10, use the backup
named in `workspace_organization_audit\WEB_TELEMETRY_BACKEND4_INSTALL\INSTALL_RESTORATION.json`
(the current installed rc.10 files) and the before-state below.

Before-states: `workspace_organization_audit\WEB_TELEMETRY_FRONTEND_PARITY_BEFORE_2026-10-09\`
(workbench_ui.js, web_telemetry.c, manifests, build/test files, README,
handoff; SHA256SUMS.txt).

## Pending / follow-up

1. Owner: send the old-viewer screenshots. Tune the map orientation, G-G
   axes/range, colors and the velocity unit (km/h vs mph) to match.
2. Owner: install rc.11 (game closed). Then do a live look at
   `/workbench` over a full lap: check completed-lap times against the Laps
   tab, the reference pick, dtime, and B = an on-track AI.
3. Optional: list server-recorded laps (`/trace/status`) in the strip so laps
   recorded before the page opened appear.

## Install (2026-10-09)

Owner authorized in chat (game closed; no game process running, verified). rc.11 payload copied to `Documents\F1 Challenge V10\plugins\lightloader\`; all 4 files byte-equal to the payload (DLL `465DB2E9…`, sidecar `7353B8EB…`, ini unchanged, mod.json `8C712EDB…`). rc.10 backup + payload copy: `Documents\F1 Challenge V10\WebTelemetryBackups\20261009_rc10_before_rc11\` (`rc10\`, `rc11_payload\`). Map: `workspace_organization_audit\WEB_TELEMETRY_FRONTEND_PARITY_INSTALL\INSTALL_RESTORATION.json`.
Pending: start game, open `/health` (expect version 4.2.0-rc.11, workbench_ui matches:true), then live look at `/workbench`.

## rc.12 — history kept across driver changes (2026-10-09)

Owner report: two laps driven, A/B driver changed, collected telemetry lost. Cause (FACT, code read): `Workbench.select()` reset the side's history by design (rc.5 rule). Server recorder had no completed lap either: `/trace/status` A `dropped_laps:1`, `laps:[]` (out-lap from pits never starts at the line, so it is discarded; lap 2 still open). Lost laps not recoverable.
Fix: page records every car from page open (`Workbench.hist`, per-car bounds unchanged); select only switches the shown history; reference lap survives; imports get their own side (`importInto`) and never overwrite a car. Also fixed: empty side had `slot === null` and `null >= 0` is true in JS.
Verify: parity_test 20/20 (3 new history tests), rpm_test 21/21, synthetic switch A->AI->A keeps trace, full gate PASS (41,631 checks). rc.12 sidecar `AB160E1D…`, DLL `42037A9D…`, payload `%TEMP%\f1c-web-telemetry-4.2.0-rc.12\payload`. NOT installed (game must be closed). Before-state: `workspace_organization_audit/WEB_TELEMETRY_KEEP_HISTORY_BEFORE_2026-10-09/`.
