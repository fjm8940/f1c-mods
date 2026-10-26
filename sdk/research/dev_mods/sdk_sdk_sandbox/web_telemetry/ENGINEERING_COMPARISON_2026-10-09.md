# Web Telemetry 4.2.0-rc.1: player/AI engineering comparison

Date: 2026-10-09. Status: Development record.; offline
verification passed, native gameplay and human presentation acceptance pending.
Canonical work: `mods/sdk_sandbox/web_telemetry` inside `f1c_ida_decompiled`.
Target unchanged: Lightloader manager 2.2.7 (2.2.6 loader payload), protected
host ABI v2 / Hub 2, broker ABI 15.

This record closes the implementation of
`DEEPSEEK_AI_COMPARISON_PLAN_2026-10-09.md` phases 0-2 plus the phase-4
workflow, against the bounded specification in
`CHANNEL_LEDGER_2026-10-09.md`. The ledger holds the evidence tier, units,
signs, ownership and missing-value rule for every channel; this record reports
what was built, what was verified, and what remains open.

## What shipped

1. **Distance-indexed trace recording** (`g_trace`, 4.2.0): the game-thread
   capture already copies full 0x48C0 vehicle records into owned mirrors; the
   worker interprets them into per-lap traces for two selected cars (player +
   rival). Samples store native observed values only: session time, lap
   distance, speed, throttle, brake, steering (AI only), lateral/longitudinal/
   vertical acceleration, yaw, derived yaw rate, four tread values, and a
   per-channel validity bitfield. Six laps per car x 2400 samples; older laps
   drop with counters. Segment breaks (sim gap > 1 s, reverse travel > 2 m,
   teleport > 50 m, pit, off-track, lap reset, session change) are never
   bridged.
2. **Endpoints (read-only):** `/trace/status`, `/trace/select` (internal
   selection state only), `/trace/data` (JSON) and `/trace/export` (CSV),
   `/trace/compare` (distance-aligned grid with per-channel holes and
   `elapsed_B - elapsed_A`). Unknown laps answer 404; malformed requests 400.
3. **Engineering tab:** A/B car+lap selectors, per-channel charts (speed,
   throttle, brake, steering, lateral/longitudinal g, yaw rate, minimum
   tread), wheel-zoom, cursor readout with time delta, two-click interval with
   entry/min/exit speed, brake and full-throttle points, sector buttons, reset
   zoom, and condition/run metadata with mismatch warnings. File export/import
   (`f1c.trace/1`) supports cross-session baseline-vs-modified comparisons;
   the client `cmpGrid` mirrors the C builder and is exercised by an offline
   Node test.
4. **Run metadata and statistics:** session/track/weather/AI difficulty, car
   and driver identity, lap time, physics mode per side; per-run lap list,
   clean-lap count, median and spread, exclusions (pit/breaks). Missing mod or
   setup identity is stated as not visible to the observer, never guessed.
5. **Dashboard corrections** from `UI_CORPUS_REVIEW_2026-10-08.md`:
   - C02: the misleading partial-sector "LIVE Sx" delta is replaced by
     `SECTOR Sx ELAPSED` plus `LAST Sx vs PB` (completed sectors only); the
     distance-aligned delta lives in the Engineering tab.
   - C06: lap history ring raised 60 -> 120 with published `lhlim`/`hlost`
     truncation counters; the JSONL journal remains the durable record.
   - C07: per-car `vf` validity bits (speed/throttle/brake/steering/tread/
     acceleration) and per-sample trace flags; unavailable differs from zero.
   - C08: independent client watchdog ages the last accepted SSE frame; status
     line shows frame age and transport age separately.
   - C09: `/laps`, `/incidents`, `/pits`, `/driver` and the trace endpoints
     carry the session generation; async replies are rejected across a
     generation change and dependent caches clear on it.
   - C10: the driver dossier drawer exists, opens from the single selection
     path (rows/map/team clicks now call `loadDriver`), and closes cleanly.
   - C15: teammate matching prefers the broker ABI-8 team hash (`tid`) fetched
     through the reviewed broker snapshot API, falls back to a non-empty name
     match, and never matches empty strings.

## Channel availability delivered (evidence in the ledger)

Available for both player and AI: speed, throttle, brake, gear, lap number,
lap distance, lateral/longitudinal acceleration, vertical acceleration (AI
FACT, player same block), yaw (derived from the FACT rotation matrix), yaw
rate (derived), tyre tread (player from the thermal mirror, AI from the wheel
block `+0xB0` proven by `FuelConsumptionAndWear`).

AI only: steering input factor at `+0x3580` (FACT).

Explicitly unavailable, with reasons shown in the UI/status: player steering
(no proven player-side writer), per-wheel suspension compression, tyre loads,
tyre forces, wheel spin/steer angles, AI per-wheel brake temperature and
compound-parameter fields. The static pass corrected three previously DERIVED
labels (`+0x4C` is a torque cap, `+0x5C` is a spring/damper rate, `+0x9C` is
written from compound data) and those candidates stay out of production reads.

## Verification

- `TELEMETRY_TESTS=1 python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry`:
  **PASS**. 41,053 runtime assertions (trace ring, lap completion and +1-only
  closure, crafted segment break flagged, player-steering-unavailable and
  AI-steering-valid, yaw/yaw-rate derivation, tread normalization, compare
  grid holes and positive delta for the slower synthetic lap, serializer
  canaries, host/layout refusal, mirror isolation) plus synthetic HTTP
  (non-loopback health/dashboard/SSE/write refusal; trace status/data/csv/
  compare/select; 400/404 refusals; fragmented request; system-only x86
  imports) and the Node `cmpGrid` interpolation/break/yaw-wrap test.
- `python tools/f1c_mod.py sdk-test`: **full gate PASS**, all canonical SDK
  tests passed.
- Performance (synthetic proxy, O1 test build): repeated runs measured
  capture+read+traces p50 214-258 us, p95 261-338 us, max 298-531 us,
  including the 1.9 MB mirror copy. Native FPS impact remains unmeasured.
- Workspace validator: same two pre-existing errors (`.claude`, `plans`),
  zero warnings; the owner authorized this task's scoped exception and no new
  errors were introduced.

Artifact SHA-256:
`22DEB680422DE43E2986647F4C3EA9C0158D28B7A004EDCBF8C8708A9BEB67C7`.
The build is reproducible: `build.bat` now links with
`-Wl,--no-insert-timestamp`, and two consecutive builds produced this identical
hash. Source and SDK input hashes: `VERIFICATION_MANIFEST.json`.

## Preservation and restoration

Before-state of every edited file is preserved under
`workspace_organization_audit/WEB_TELEMETRY_ENGINEERING_BEFORE_2026-10-09/f1c_ida_decompiled/mods/sdk_sandbox/web_telemetry/`.
Nothing was deleted, moved or renamed. Restore by copying those files back
with the game closed. No game file, production artifact, loader or broker
binary was touched; the broker is used only through its reviewed discovery,
registration, task-post and snapshot APIs. HTTP stays read-only; `/action`
still refuses; no POST is accepted.

## Pending gates (cannot be closed by this task)

1. Native guarded full-weekend run (`f1cm_guard.asi`, never QuickRace) with a
   human player and AI cars: confirm trace samples advance, laps record at
   real crossings, the Engineering tab renders live data, and measure the real
   frame-time delta against the recorded budget.
2. Live verification of the player steering candidate and the per-wheel
   advanced channels (suspension/loads/forces) with the owner driving.
3. Human presentation acceptance of the Engineering view and the corrected
   sector/fuel/team displays.
4. Cross-PC Tailscale acceptance (unchanged from rc.2).

## Wasted-effort check

- Repeated reads: orientation targeted; two large source reads were reused for
  several edits. The decompiled-function search for the player steering writer
  covered ~30 files before the conclusion; the result is recorded as a
  negative finding so it is not repeated.
- Broad builds: focused build+suite run per source change; one full SDK gate
  after the focused suite passed (plus one earlier partial invocation cut
  short by console truncation and rerun).
- Retries: three compile/test failures were real defects caught by the gates
  (a C string continuation, a TRF/TGF bit-namespace mix-up, a delta placement)
  and were fixed at the source; no speculative retry loops.
- Polling: none; the native game was not running and no live polling was
  possible or attempted.
