# Web Telemetry 4.3.0-rc.1: unified telemetry viewer and physics channels

Date: 2026-10-10 to 2026-10-11. Implementer: Claude (owner-approved plan
`plans/check-the-whole-web-atomic-newell.md`). Pipeline: **verify -> human
validate**. Private candidate, NOT installed; installed state not inspected or
changed. No game process was running, so no live capture was possible.

## Why

The owner asked for one premium, professional telemetry viewer that brings
in the acceleration and braking channels, built as a single package. A
community request (Tester F, 2026-10-06) asked for:

- speed, inputs, and lateral/longitudinal acceleration;
- yaw, tyre loads and tyre-force solver output, to see why a setup lets the AI
  corner faster;
- suspension compression and tyre wear, to tune the AI tyre-wear rate;
- AI-versus-player comparison, both as teammate comparison and to see how
  modified physics changes the AI.

rc.14 had two separate comparison UIs: the dashboard Engineering tab (JavaScript
inside C strings) and the `/workbench` legacy-layout sidecar.

## What changed

| Area | Change | Files |
|---|---|---|
| Viewer | New sidecar `telemetry_viewer.js` (`F1CViewer`) served at `/viewer` + `/viewer.js`. It reuses the `Stop4Workbench` recorder, lap model, imports and DM#14 interpolation unchanged. | `telemetry_viewer.js`, `web_telemetry.c` (`g_viewer_html`, `sidecar_load`, routes) |
| Views | Overview; Vehicle dynamics (G-G diagram plus per-sector grip envelope); Braking & corners (auto zones, brake point, peak decel, minimum/apex, throttle-on, exit, time delta); Tyres (wheel cards, temperature/tread traces, wear per lap with laps-left estimate); Compare (head-to-head figures, sectors, 20 mini-sectors, "who is faster where" map, not-like-for-like warnings); Live gauges; Chassis research; Legacy layouts (the 23 old-viewer layouts, unchanged) | `telemetry_viewer.js` |
| Interaction | Shared distance cursor across every chart, the map and G-G; wheel zoom, drag pan, double-click reset; distance or lap-time x axis; per-side lap picker defaulting to "latest completed lap (auto)"; freeze; import (`f1c.trace/1`, `.lap`, `.spt`); export; keys 1-8 / D / R / A / B / Esc | `telemetry_viewer.js` |
| Design | Dark motorsport theme from design tokens: A cyan `#38bdf8`, B orange `#fb923c`, tabular mono numerals. HiDPI canvas with base and overlay layers, so cursor moves never redraw data. Evidence-tier badge on every channel; responsive single-column layout below 980 px | `telemetry_viewer.js` (CSS block) |
| Dashboard | New "Telemetry Viewer" button (`#tbTV`) beside Workbench. Engineering-tab code unchanged, so the tests' byte-stable slices still hold | `web_telemetry.c` |
| Capture | Per-car v6 block computed after `player_slot_update()` (`fill_v6_channels`): vertical accel, yaw, tyre temp, player carcass, and RESEARCH candidates. New 3,380-byte mirror `ADDR_WHEEL_LOAD` (`0x7C79D0` = `W(0)+796`) | `web_telemetry.c`, `mirror_regions.json` |
| Traces | `trace_sample_t` grows from 80 to 160 bytes: `ttemp[4]` and `rx[16]`; TRF bits 15-20; grid/compare emit `ttemp0-3` and `rx_su/ld/sl/fo0-3`; `/trace/status` adds `tyre_temp` with a specific reason. Trace/compare buffers grow to `TRACE_JSON_BYTES` 1,572,864 | `web_telemetry.c` |
| Contract | `api_version` 4; schemas updated for `telemetry` (car `vf6`/`gv`/`yaw`/`ttc`/`tcc`/`rx`), `trace_data` (`tt`/`rx`), `trace_compare` (20 new series) and `health` (`viewer_ui`). Document id stays `f1c.trace/1`: fields are additive and old exports still import | `schemas/*`, `API_CONTRACT_2026-10-09.md` section 13 |
| Packaging | The payload now has 5 files (adds `telemetry_viewer.js`). `pin_sidecars.py` keeps all pins in sync: C constants, manifest and three metadata files, plus `--payload` and `--check` modes. `verify_sidecar.py` checks both sidecars and the compiled pins | `pin_sidecars.py`, `verify_sidecar.py`, `verify_payload.py`, `build.bat` |
| Tests | New `viewer_test.js` (41 checks). Runtime setup now seeds player thermal values and AI wheel-block values, with assertions on `tt`/`rx`. The worst-case size test uses the production buffer and asserts no truncation | `viewer_test.js`, `runtime_test.c`, `test_telemetry.py`, `robustness_backend3.py` |
| Review harness | Offline review page with a SYNTHETIC physics feed (closed track, grip- and brake-limited speed profile) | `viewer_review/index.html`, `viewer_review/mock_feed.js` |

## Channel coverage against the request

| Requested | Status in 4.3.0-rc.1 | Tier |
|---|---|---|
| Speed, throttle, brake, gear, RPM | Charted, compared, exported | FACT / CONFIRMED |
| Steering | AI only: input factor, not degrees. Player writer unproven | FACT (AI) |
| Lateral / longitudinal / vertical acceleration | Charted as g; long G shown accel-positive (native sign is forward-negative); G-G envelope | FACT |
| Yaw, yaw rate | Yaw FACT (rotation matrix); yaw rate DERIVED by the viewer and the trace | FACT / DERIVED |
| Braking analysis | Zones, brake point, peak decel, apex, throttle-on, exit, time delta per zone | DERIVED from FACT |
| Tyre wear | Tread per wheel (player FACT, AI CONFIRMED); wear per lap and laps-left estimate | FACT / CONFIRMED + DERIVED |
| Tyre temperature | Player: mean of three surface zones (FACT) plus carcass (FACT W+868). AI: wheel+0x50 candidate | FACT / RESEARCH |
| Tyre loads | Candidate only: AI wheel+0x44, player W+796 | RESEARCH |
| Tyre-force solver output | Candidate only: wheel+0xD8 force parameter | RESEARCH |
| Suspension compression | Candidate only: wheel+0x10 | RESEARCH |
| Slip | Candidate only: wheel+0x38 | RESEARCH |
| AI vs player / physics A/B | Any two cars, or imports, on A/B; head-to-head, sectors, mini-sectors, corners, G-G envelope; warnings when car, compound or fuel differ | DERIVED |

RESEARCH channels are recorded but hidden until the user turns them on. They
are always shown unitless with a banner, and never summarized as physics. The
research view adds Pearson correlations against FACT lateral g, longitudinal
g and speed. These encode the checks a real load or deflection channel must
pass.

## New static finding (not live)

`SuspensionAndWheelPhysics_004e5f60.c:477` initialises wheel+0x50 to
`flt_8AFEF0 + 273.16` at session setup (ambient temperature in Kelvin). That
makes +0x50 a tyre-temperature candidate, not the "load-like" role guessed on
2026-09-30. Tier: DERIVED (initial value only; the per-tick writer is not
traced). Recorded as `ai-wheel-block-0x50-temperature-candidate-20261011` in
`findings.jsonl`. The other candidates keep their CHANNEL_LEDGER 1.3 status.

## Verification (offline)

- Canonical `TELEMETRY_TESTS=1` build: PASS.
  - Runtime assertions: 42,213.
  - Contract: api_version 4 pinned, schemas validate every live document.
  - Served `/viewer` and `/viewer.js` (byte-equal to the staged sidecar);
    dashboard links the viewer; every `/telemetry` car carries the v6 fields.
  - Node tests: viewer 41/41, workbench RPM/DM#14, 22/22 audit model groups,
    served bootstrap lifecycle.
  - Robustness: 17 malformed HTTP refusals, SSE cap, soak (20 x 2 h, flat RSS),
    fuzz (80,000 iterations, 0 crashes).
- Worst-case sizes, now measured, not clipped: telemetry 141,300; trace_data
  1,056,123; compare 619,798 bytes.
  - Correction during this work: the first passing run reported trace_data =
    524,287. That was the old 512 KB test buffer truncating the output, not a
    measurement. Fixed by sizing the test to the production buffer and asserting
    no `truncated:true`.
  - Measuring also showed that 1 MB was too small, which is why
    `TRACE_JSON_BYTES` is now 1.5 MB.
- Reproducible build: two builds give DLL
  `3D967E4244D61FD8BC961EAAD76684DB564730E22856271BBE0BAF2218DB1CEB`;
  `pin_sidecars.py --check` OK.
- Synthetic capture cost: capture+read p95 248 us; callback-only p95 202.9 us.
  This is not a native measurement.
- Browser:
  - All 8 views rendered on the synthetic feed with zero console errors.
  - DLL-served `/viewer` through `runtime_test serve`: both sidecars match their
    pins, 104-car roster, player `ttc` 86.8 C with `vf6=47`, AI research loads
    present.
  - Synthetic data proves implementation behavior only.

## Decisions and deviations from the plan

- The CSS ships inside `telemetry_viewer.js` (one sidecar, one pin), not as a
  separate `.css` file.
- The trace document id stays `f1c.trace/1`, with the new fields additive. A
  `/2` id would have broken existing exports and the Engineering-tab importer
  for no gain. The contract shape change is versioned by `api_version` 4.
- `/workbench` stays the legacy page, because its tests drive the page script
  itself. The viewer embeds the same legacy layouts under "Legacy layouts".
- CSV export is unchanged. The new columns are only in the JSON trace.

## Pending (human / native)

1. Install the exact payload (5 files from
   `%TEMP%\f1c-web-telemetry-4.3.0-rc.1\payload`) with the existing
   install/restore map, extended with `telemetry_viewer.js`. This needs
   Matias's authorization.
2. One guarded race weekend: existing L1-L15 checklist plus the viewer items in
   `PRIVATE_TEST_README.md`. Check the native capture p95 with the extra mirror.
3. RESEARCH proof session (research probe protocol in `PRIVATE_TEST_README.md`).
   Promote a field only with live evidence in `findings.jsonl` and
   CHANNEL_LEDGER.
4. Visual and presentation acceptance on a real desktop browser and the second
   PC (Tailscale), plus long-session memory.

## Restoration

Before-state of every edited operational document and source is under
`workspace_organization_audit/webtel_430_BEFORE_2026-10-10/`. rc.14 source can
be restored from there. The rc.14 payload in `%TEMP%` was overwritten only in the
staging directory of the new version name (`4.3.0-rc.1`); the
`f1c-web-telemetry-4.2.0-rc.14` staging directory is untouched.

## CO-OP support (4.3.0-rc.2, 2026-10-11)

Owner request: "make it work with the co-op mode". Before-state of every
edited file is under `workspace_organization_audit/webtel_430_coop_BEFORE_2026-10-11/`.

**Evidence used**
- Static (FACT, `development/mods/coop_mode/docs/INVESTIGATION_NET_SYNC_2026-10-09.md`):
  controller mode `veh+0x32C8` is -1 inactive, 0 local, 1 AI, 2 remote, 3
  replay. Mode 2 runs **no physics** on this PC. Its pose, velocity, wheels and
  rpm come from network records. On a CO-OP client every bot is mode 2
  (host-driven).
- Live (read-only f1c-memory, CO-OP race on the host PC, 2026-10-11,
  sim time 213.9 -> 239.2 s):
  - slot 0, the host, was role 0 and ai_control 0 (retired);
  - 23 bots in slots 32-54 were role 1;
  - retired bot slot 48 read **ai_control 0 while in role 1**.

  So "ai_control 0" is not a safe local-player test. No mode-2 car was present
  in that read, so the remote-car path is static evidence plus synthetic tests.

**What changed**

| Area | Change |
|---|---|
| Player detection | `player_slot_update()` first takes the car with controller mode 0 and ai_control 0, then falls back to the old rule. On a client, your car is no longer confused with the host's car. |
| Remote cars (mode 2) | `remote_gate()` clears throttle, brake, steering, accelerations, tread, tyre temperatures and all RESEARCH bits. Yaw, position, rpm and lap fields stay. Speed is DERIVED from the interpolated position (EMA, ≥ 50 ms steps), because the raw speed field is not written for network copies; the host copy read 0 on 2026-10-10. New `vf6` bit 512. The steering reason reads "remote car (CO-OP)". |
| Traces | A remote car records only the gated speed plus pose, yaw and rpm. `/trace/status` adds `remote` and per-channel reasons. A slot that switches between a bot and a remote human resets its trace (coop `release_bot`). |
| `cr` field | Read from the vehicle mirror every tick. Previously it came only from the broker every 2 s. Same field. |
| Viewer | Defaults are A = you (mode 0) and B = friend (remote human) when present, otherwise an AI. Car lists mark *(you)*, *(friend, remote)* and *(AI, remote copy)*. The header shows a CO-OP badge (host/client, remote counts). A remote banner on Overview and Compare explains the missing channels and the workflow: friend exports a lap, you import it. A remote human never takes your tyre block. KPI deltas no longer treat missing values as 0 (bug found on the CO-OP synthetic feed). |
| Contract | `trace_status.cars[].remote`; `vf6` bit 512 documented; api_version stays 4, because the v4 contract is unreleased. The schema digest was re-stamped. |

**Verification**
- Gate PASS:
  - 42,222 runtime assertions. These include `test_coop`: client layout with
    the host car remote in slot 0 and you in slot 5, plus a retired ai-0 bot.
    It checks the player is slot 5, remote channels are cleared, the derived
    speed is 50 m/s, and the local tyre block stays FACT.
  - Viewer 45 checks.
  - Soak, fuzz and sizes unchanged.
- Synthetic browser review: a friend car on side B shows the remote label,
  banner, CO-OP badge, "not available" tyres, and "—" for missing KPIs.
  0 console errors.
- DLL `198D9F6CEF1C67F2A048B65B5337D1F8FFDE7F52A9B56DF6531A4A9EABAB5819`
  (reproducible).
- Limitation: three gate runs timed out on HTTP while the CO-OP game ran on the
  same PC. The same code then passed, and the endpoints answered in ~13 ms in
  isolation. The cause is not proven; it is recorded as load-related.

**Pending (live CO-OP)**
- C1: on the host, with a friend connected, the friend shows *(friend, remote)*
  and the derived speed matches the friend's in-game speed within a few km/h.
- C2: on the client, A = you, the bots are labelled *AI (remote copy)*, and your
  tyre temperatures are FACT on your car only.
- C3: each PC exports a lap and imports the other's, giving a full-channel
  comparison.
- C4: native capture p95 < 500 us with 20+ cars.
