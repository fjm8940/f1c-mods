# STOP 3 record — corrections, player/AI channel ledger, import + LIVE workbench design and budgets (STOP 3R revision)

Date: 2026-10-09. Status: STOP 3R READY. Assignment:
`DEEPSEEK_LEGACY_TELEMETRY_ASSIGNMENT_2026-10-09.md`. Reviews being answered:
`legacy_dossier/STOP2R_CLAUDE_REVIEW_2026-10-09.md` (ACCEPTED WITH
CORRECTIONS — sections 1-3) and `legacy_dossier/STOP3_CLAUDE_REVIEW_2026-10-09.md`
(NOT ACCEPTED — sections 4-7 revised in this 3R pass; the STOP 3 review items
1-5 are answered under the section titles below). Authority: root `AGENTS.md`,
`development/mods/MOD_STANDARD.md` (cited below), `f1c_ida_decompiled/AGENTS.md`,
SDK front door. Viewer and game files were used read-only in place; no vendor
file was copied into the workspace (the one %TEMP% copy is the crash-truncated
.spt test case, hash recorded). No viewer launch, no installs, no game writes.
No live race was needed for STOP 3/3R: writer evidence plus the review's live
growth figures and the retained live HTTP captures suffice; a live race will be
requested when STOP 4/5 native acceptance or the section 8 steering check needs
one. The 3R pass is documentation-only; no code, DLL or settings file changed.

## 1. Review item 1 — telemetry.spt writer is the GAME (corrected)

Front-door location (SDK corpus, `functions/*.c`):

| Function | Role |
|---|---|
| `TelemetryDataDump` 0x004D4A80 | opens the file; writes u32 0 + LCG seed (0x04), constant f32 0.05 (0x08), four identity strings, 57 descriptors, 17 session hash words, then calls `FileReadWithXORDecode` |
| `FileReadWithXORDecode` 0x004D5AF0 | writes u32 size + XOR-encoded file bytes, keys 0x5C/0xB4/0x12 cycling (writer-side proof of the embedded block) |
| `DebugLogInit` 0x004D5960 + `sub_447F30` 0x00447F30 | per-call player record: type 0x00 + 57 × float32 from `P = 0x7C7428` |
| `sub_4D59B0` 0x004D59B0 | environment record: type 0x04, packed flags u32 + 5 × float32 (last two +273.16 K) |
| `EncodedDataWriterWithXOR` 0x004D5900 | event record: type byte + one dword XOR the last player elapsed-time bits |
| `MainGameTickUpdate` 0x0042C690 (lines 88-94) | cadence: player record every `dword_8AD23C` frames of the 400-frame counter; environment record at counter 0 (1 Hz) |

Record layout and per-field source offsets: `FORMAT_SPECS_f1_telemetry_2026-10-09.md`
§1 (57 channels: 18 base + 36 wheel + 3 tail; writer-proven offsets). The five
wheel offsets previously guessed in the in-progress draft (0x405/0x406/0x409/
0x40D/0x3FE) were WRONG and are corrected to the writer's reads:
L/C/R temps `W+0x164/+0x16C/+0x174`, pressure `W+0x18C`, wear `W+0x1AC`,
corner weight `W+0x134`.

Attribution correction: during the STOP 2R window the file was being written
by the GAME (`F1 Challenge Online.exe` PID 34576, live race, no viewer
process), not the viewer. Stride vs growth: measured 2,312 B/s over the
closed 4.5 MB stream (19,447 × 229 B at 0.1 s + 1,945 × 25 B at 1 Hz);
expected 2,315 B/s. The review's `growth × 0.05 s` gives ≈ 120 B and does not
match; the effective cadence is 0.1 s, so the correct cross-check is
`growth × 0.1 s ≈ 231 B ≈ 229 B + env share`. The stored 0.05 field is a
writer constant, not the cadence.

Decode hygiene: the file was decoded only while closed (game exited); the
crash-truncated copy lives at
(SHA256 `AA04E86E3C64311AC0D4AF93003E6AB82320162AAA5AB0B9F7FF6FE823B308E2`,
4,505,600 B) and is used only as the torn-input regression case. Header
bytes 0x00-0x07 finalization on a graceful close remains OPEN.

## 2. Review item 2 — .lap obfuscation implemented, remaining sections decoded

`tools/legacy_formats.py::decode_lap` now implements the byte-5 shift +
reversal for the metadata (B1) and weather (B2) blocks and decodes the whole
file against `reference_2002_hockenheim.lap`
(SHA256 `6AA795B5FC05E3ADB8BAB5F4FE8350824AA095E4F3FA0A8434CC48E2CADE27AF`):

- B1 (len 113): `DRIVER=Matthias Steppe TRACK=2002_Hockenheim DATE=09-10-2026
  12.43 TEAM= VEH=2002 BAR TER=unknown VERSION=2003.14`.
- Fixed 73-byte field block; first float = lap time 133.419 s, matching
  `LAPTIME=2.13.419` in B2 (cross-check in the decoder).
- B2 (len 172): LAPTIME, SECTOR1/2, AIR_TEMP=20,1 TRK_TEMP=21,3 RAIN=0,0
  TRK_WET=0,0 `AIDS=[ST2OP1SC2SP1AB2…]` OFFTRACK=FAILED UPLOAD_ALLOWED=NO.
- Channel table: marker 1, count 86 (not 113 — the STOP 2R "count" was the B1
  length), length-prefixed names (`time, x, y, z, yaw, … rear wheel speed
  differential`).
- Samples: 1,385 × 344 B (86 × float32), 0.1 s cadence, t = 0.0025 → 138.4025 s,
  decoded per channel name (e.g. sample 600: engine rpm 16,246.9, throttle
  99.998, gear 6, distance 2,924.29 m).
- Trailer (9,381 B): byte-7 shift + reversal text block at 0x74E5A decodes to
  the lap identity plus end-of-lap weather (`AIR_TEMP=20,2 TRK_TEMP=23,1`).
  The remaining trailer bytes are recorded as OPEN (offset/length in the JSON).

## 3. Review item 3 — RPM scale (no re-investigation)

Finding `engine-rpm-live-scale-20261009` (OBSERVED): `F1CVehicle+0x39A0` is
engine angular speed in rad/s for player and AI; `RPM = value ×
9.549296585513721`. It is cited in the channel ledger below; nothing was
re-investigated.

## 4. Player/AI channel ledger — legacy recorder ↔ native evidence

The native ledger (units, ownership, validity, budgets) stays
`../CHANNEL_LEDGER_2026-10-09.md`; this section adds the mapping the legacy
dossier contributes, for the import path and for future native verification.
The legacy recorder is **player-only**; AI rows cite the native ledger only.

**Correction (STOP 3 review item 2, 2026-10-09).** The original STOP 3 line
"positions not in the native ledger as exposed channels / not exposed" was
WRONG for x/z. Car position is exposed natively for **all** cars through three
independent, already-served paths (all read-only, no new live read path):

- `F1CVehicle.WorldPositionX/Y/Z` at `veh+0x34E0/+0x34E4/+0x34E8` — FACT
  (`tools/sdk/f1c_vehicle_struct.h` lines 69-71). The reviewed broker copies
  them per slot into the vehicle snapshot `world_x/world_z`
  (`mods/sdk_sandbox/game_update_broker/game_update_broker.c` lines 389-391;
  ABI fields `tools/sdk/f1c_broker_abi.h` lines 161-163), and
  `memory-mcp drivers` returns `world_x/world_z` per slot for every car
  (`tools/f1c_memory_mcp.py`; live 2026-10-09, per the review).
- `F1CVehicle.PhysX/PhysZ` at `veh+0x1C/+0x24` — FACT, served per car as
  `px/pz` by `/telemetry` (`web_telemetry.c` lines 3136-3137, 3423, 3373).
- `coordinatesInTrack_x/y` at `veh+0x744/+0x74C` — CONFIRMED, served per car
  as `mx/mz` by `/telemetry` (`web_telemetry.c` lines 424-425, 3138-3139).
  `/track` already serves the AI waypoint racing line (count at AI DB `+0x130`,
  array at `+0x134`, stride 316, ≤3000 points; `web_telemetry.c` lines
  253-258, 1671-1692), and the existing track maps draw every car with
  heading and identity.

Stays explicit: **y** is also exposed (`WorldPositionY +0x34E4`, `PhysY
+0x20`, FACT) but is not used by the current UI; **yaw is not a native
scalar** — heading is the rotation matrix at `0x34BC..0x34DC` (FACT), the
angle is a consumer-side DERIVED conversion, and yaw rate is DERIVED between
samples with the existing segment rules (never bridged across gaps).

| Legacy channel group (spt / lap) | Legacy source | Native player | Native AI | Tier |
|---|---|---|---|---|
| elapsed time, lap distance | spt #0; lap `time`, `distance` | sim time; `veh+0x4044` | same | FACT |
| position x/z | spt #1-3 | FACT native for ALL cars: `WorldPositionX/Z veh+0x34E0/+0x34E8` (broker snapshot `world_x/world_z`, memory-mcp `drivers`); plus `PhysX/Z veh+0x1C/+0x24` and `coordinatesInTrack veh+0x744/+0x74C` (CONFIRMED), both served per car by `/telemetry` (`px/pz`, `mx/mz`) | same | FACT |
| position y, heading/yaw | spt #1-4 | y: `WorldPositionY +0x34E4` / `PhysY +0x20`, FACT exposed, unused by the UI; yaw: rotation matrix `0x34BC..0x34DC` (FACT) → angle DERIVED, rate DERIVED | same | FACT (y, matrix) / DERIVED (angle, rate) |
| lat/vert/long accel | spt #5-7 | `veh+0x354C/+0x3550/+0x3554` | same | FACT |
| engine rpm | spt #8 | +0x39A0 × 9.549296585513721 | same | OBSERVED (`engine-rpm-live-scale-20261009`) |
| clutch rpm | spt #9 | not exposed | not exposed | OPEN |
| throttle / brake | spt #10-13 | `+0x357C/+0x3584` | same | CONFIRMED |
| steering | spt #14-15 | native player writer NOT found; the game recorder writes `P+0x000` (filtered) and `P+0x040` (unfiltered) — a new candidate source | `veh+0x3580` (input factor) | DERIVED (player, native unproven) / FACT (AI) |
| clutch input | spt #16-17 | not exposed | not exposed | OPEN |
| gear | spt #54 | `+0x39FC` | same | FACT |
| water/oil temperature | spt #55-56 | not exposed | not exposed | OPEN |
| per-wheel spin, suspension, ride height | spt #18-53 | tyre tread via 0x7C7A00 mirror; others unproven | tread only | FACT (tread) / OPEN |
| tyre L/C/R temp, pressure, wear, corner weight | spt #18-53 | tread via mirror; temps/pressure/wear in the player block (legacy evidence) | tread only | FACT (legacy) / DERIVED (native player block) |
| velocity | lap `velocity` | `veh+0x4440` (speed) | same | FACT |
| rain, track dampness, air/track temp | spt env record; lap weather | spt env record = packed flags + 5 floats (producer evidence) | n/a | FACT (file) / OPEN (native exposure) |
| aids1-4, damper velocity ×4, tyre crown/camber/avg temps, rear wheel speed differential | lap only | viewer-side derived/recorded channels; native equivalents unproven | n/a | OPEN |

RPM-scale result to carry into the workbench: display RPM = raw ×
9.549296585513721 for both player and AI; never label the raw rad/s as RPM.

## 5. Budgets (review item 3)

### 5.1 Import/decode budgets (measured 2026-10-09, this machine)

| Item | Budget / measured |
|---|---|
| decode_spt 4,505,600 B (19,447 player records) | 0.48 s, peak process memory 9.0 MB |
| decode_lap 487,843 B (1,385 × 344 B) | 0.13 s |
| One imported lap in float32 | ≈ 0.42 MB samples + names/metadata |
| Workbench import bounds (design) | file ≤ 64 MB, channels ≤ 512, samples ≤ 2,000,000, trailer ≤ 1 MB; refuse overflow with an explicit error; truncation counters reported |
| Threading | all decoding on the worker/HTTP side (file I/O), never the game callback (MOD_STANDARD R2) |
| Response | imports reuse the existing 524,288-byte bounded trace response; a legacy import converts into the trace model first, then serves like any trace |

### 5.2 Live workbench budgets (review item 3; measured where stated, otherwise design)

| Item | Budget | Status / rationale |
|---|---|---|
| Cars captured | Trace recorders: 2 selected cars (`TRACE_CARS=2`). Whole-field stream/mirrors: every active car — 22 in a full race; slot pool `VEH_SLOTS=104` × `0x48C0` B ≈ 1.85 MB static mirrors (existing rc.3 behavior) | MEASURED/existing (`web_telemetry.c` 1104, 59, `MAX_CARS` 104 at 623; `VEH_SLOTS` `tools/sdk/f1c_sdk.h:11`; live payload shows 22 cars) |
| Capture rate | 20 Hz (worker posts one game-thread capture per ~50 ms loop; duplicates during pause are dropped, not stored) | DESIGN target; rc.3 synthetic callback p95 267 us, max 286 us (MEASURED synthetic); native FPS gate open |
| Transmit rate | 10 Hz SSE (`SSE_UPDATE_MS=100`), full `/telemetry` frame per tick; `/trace/status` poll 0.5 Hz while Engineering is open | MEASURED/existing (610, 193-198; 4494) |
| Redraw rate | Dashboard repaints per SSE frame (10 Hz). Live partial traces/new overview: draw throttled ≤ 10 Hz; completed-lap charts redraw on user action | DESIGN for the new views; 10 Hz is the existing transport ceiling |
| Ring sizes | Trace: 6 completed + 1 open lap × 2400 samples per car (`TRACE_LAPS=6`, `TRACE_LAP_SLOTS=7`, `TRACE_SAMPLES=2400`); per-car dashboard lap-ring 120 laps (`MAX_LAP_HISTORY`, overflow counted in `hlost`); client live driving-line ring `TRK_MAX=500` points; SSE client cap 16 (`MAX_SSE_CLIENTS`) | MEASURED/existing (1104-1111, 1017, 4018, 612) |
| Memory per car/lap | Now: 2400 × 64 B = 153,600 B per lap per car; 7 slots = 1,075,200 B per car; 2 cars = 2,150,400 B (≈2.05 MiB). With the design additions (rpm, gear, track x/z: +16 B → 80 B sample): 192,000 B per lap, 2,688,000 B for A/B (≈2.56 MiB), metadata excluded; budget ceiling ≤ 2.7 MiB | MEASURED now (1147, 1183); DESIGN for 80 B samples (§6.3) |
| Legacy import memory | One imported lap ≈ 0.42 MB float32 (measured, §5.1) | MEASURED import path |
| SSE payload | Measured live 2026-10-09: `/telemetry` = 31,016 B for 22 cars (retained capture `telemetry_sample1.json`, rc2 window). At 10 Hz ≈ 310 KB/s (~2.5 Mbit/s) per client; 16 clients ≈ 4.96 MB/s aggregate worst case — fine for loopback/LAN, to be noted for remote Tailscale viewers. Static broadcast buffer 262,160 B (`sse_broadcast`) | MEASURED capture, arithmetic budget |
| Response bounds | Per-request static buffers: `/telemetry` 262,144 B, `/track` 131,072 B, `/trace/*` 524,288 B, `/driver` 131,072 B; trace exports measured A 218,453 B / B 328,121 B for real laps; compare grid 1..50 m step, ≤1000 points | MEASURED/existing (5490-5565, 1722-1723, 5656-5658) |
| Client bounds | Nearest-state only: no catch-up replay after reconnect; `TRANSPORT_STALE` banner after 2.5 s without frames (C08); EventSource re-created 2 s after error; page state cleared on session generation change | MEASURED/existing (5343-5354) |
| Pause / reconnect | Repeated pause captures are not stored as samples; resume starts a new segment (`TRF_SEGMENT` bit 2048, lap flag `TRL_BREAKS`); charts break lines at gaps and never interpolate across them; capture older than 1 s clears the field to WAITING/STALE — no fake zeros beyond explicit validity flags | MEASURED/existing (1293, 1126-1133, 186-190) |

## 6. Workbench design — legacy import plus the LIVE player/AI workbench (review item 1)

The owner's core requirement is **live graphs for the player AND any AI**, not
only legacy import; import stays as one additional source. Design follows
MOD_STANDARD: read-only product, no game writes, game calls on the game thread
only (R2), settings beside the DLL with safe defaults (R10), all-or-nothing
checks (R5), logging from the worker. Reuse, don't fork: the 4.2.0 Engineering
tab, `/trace/*`, `/track`, `/events` SSE and the versioned trace model are the
target; nothing here starts a second product.

### 6.1 Existing building blocks this design maps onto (verified in rc.3 `web_telemetry.c`)

| Block | Existing behavior | Where |
|---|---|---|
| A/B selection | `/trace/select?which=A\|B&car=slot&gen=…`; defaults A = player, B = teammate by broker team hash else first AI; manual selection disables auto-follow; changing a car resets that side's recorder (`revision++`) with a visible message | 5569-5604, 1205-1267, 4500-4510 |
| Trace model | 2 cars; 6 completed + 1 open lap; 2400 samples/lap; 64 B/sample; per-sample validity bits; segment breaks; complete only after two witnessed crossings (rc.3 counter-before-wrap deferral) | 1104-1184, 1275-1470 |
| Completed-lap serving | `/trace/status` (open lap, lap list, `unavailable[]`, `channels_seen`, `dropped_laps`), `/trace/data` + `/trace/export` (`f1c.trace/1` JSON/CSV), `/trace/compare` (distance grid 1..50 m, ≤1000 pts) | 1878-1980, 1722-1750 |
| Track line + car map | `/track` = AI waypoint line (≤3000 pts); `/telemetry` carries per-car `px/pz` (world), `mx/mz` (track coords), `dx/dy` heading; maps `mC`/`mCPw` draw all cars with zoom/pan/click-to-focus | 1671-1692, 3373, 4145-4225 |
| Live stream | `/events` SSE pushes the full `/telemetry` frame at 10 Hz; page states LIVE / STALE DATA / TRANSPORT STALE / WAITING FOR GAME | 5553-5559, 193-198, 5338-5354 |
| Engineering live snapshots (rc.3) | validity-gated numeric text for speed, throttle, brake, AI steering, lat/long accel on the 2 s status poll; progress %, sample count, completed count | 4527-4539 |
| Gauges | focused-car speed and RPM dials at 10 Hz | 4102-4138, 4370-4372 |

### 6.2 Workbench items — mapped to existing surfaces, with what is new

| Item (review item 1) | Existing | New for STOP 4/5 |
|---|---|---|
| A/B car selection: player or any AI, colors, labels | Engineering A/B selectors over all cars, A blue / B dashed orange, default player/teammate rule, AI-control label | The same selection drives the live overview; explicit mid-lap notice; no history implied for cars that were not recorded (rule kept) |
| Default Chassis Overview | Components exist separately: map + `/track` line; speed row; RPM exists per car live (`c.r`); G-G data already streamed (`c.glat/c.glon`, validity-gated) | A default composite layout (track/driving line, speed vs distance, lat-vs-long G-G scatter, RPM vs distance) opening on the Engineering tab; **the G-G scatter chart is new** (today only dial gauges exist on the live page; the only G-G code in the component is the unverified `live_reference/` draft, not product) |
| Track/driving line | Map with AI line (white) and all cars as dots; zoom/pan | A/B driven-line polylines for live and completed laps (needs track x/z in the trace record, §6.3); start-line marker; AI line kept as backdrop |
| Speed vs distance | Completed-lap rows in the Engineering canvas | Live open-lap series drawn as the car moves (throttled ≤10 Hz), same scale as completed laps |
| G-G scatter | Data only (live 10 Hz per car; trace channels `glat/glon`) | Scatter chart for A and B from open/completed/imported data with the linked cursor; axis in m/s² and g |
| RPM vs distance | RPM per car live; RPM for AI and player is `veh+0x39A0` rad/s × 9.549296585513721 (finding `engine-rpm-live-scale-20261009`) | RPM (and gear, `veh+0x39FC`) added as trace channels so RPM-vs-distance works for open, completed, compared and exported laps; always plotted as converted RPM, never raw rad/s |
| Inputs view | Throttle/brake rows; steering AI-only with validity; gear per car live (`c.g`) | A dedicated Inputs layout (throttle, brake, gear, steering where proven); gear trace channel; player steering stays visibly unavailable until the §8 check passes |
| Live partial-lap drawing | Recorder keeps the open lap (`open_lap`, `open_samples` in status); rc.3 shows numeric snapshots only | Serve the open lap's samples (bounded, §5.2) and draw them immediately with the existing segment-break rules; usefulness never waits for two completed laps |
| LIVE / partial / completed / frozen states | Page states; complete-lap flags (`TRL_COMPLETE`, pit, breaks); open lap counters | Per-side state model with badges: LIVE (open lap advancing), PARTIAL (open lap with holes or <2 witnessed crossings), COMPLETED (ring laps), FROZEN (pinned snapshot for analysis) |
| Freeze / resume without stopping capture | — | NEW: freeze pins the displayed data (client binding + bounded server snapshot for export fidelity); capture and SSE keep running; resume rebinds to the newest data; recording is never paused |
| Lap crossing keeps history + reference | Ring rotates only when the 7th slot lands; rc.3 crossing rule keeps interpolation and two-boundary completeness | Automatic reference = previous completed lap of the same car; a frozen reference survives crossings; open lap never overwrites still-visible history |
| Mid-lap selection | `/trace/select` accepted any time; recorder reset is visible | Same mechanics with explicit copy; discard counts reported; no fabricated history for previously unrecorded cars |
| Linked cursor across charts + track | Engineering cursor readout + two-click interval + time delta | One shared distance cursor across the overview charts and the track dot (A and B at equal lap distance); the existing interval selection and delta stay |
| Zoom / pan / reset | Distance-axis wheel zoom + double-click reset (no drag pan); map has zoom/pan/reset | Drag pan and optional chart↔map linked zoom; reset to full lap/track; keys kept consistent with the map |
| Missing-data reasons | `unavailable[]` per channel; `null` samples; segment break distances; validity bits | Per-hole reason text (off-track, pit, pause, reverse, teleport, sample overflow, not recorded) driven by the existing break causes in `../CHANNEL_LEDGER_2026-10-09.md` §2 |

### 6.3 Trace record additions (design)

To draw the Chassis Overview from recorded data, the trace sample gains `rpm`
(f32), `gear` (i32) and track position `tx/tz` (2 × f32 from
`coordinatesInTrack`, the map's own frame): 64 → 80 B/sample, two new
validity bits plus a position bit, additive optional fields in `f1c.trace/1`
exports (existing readers ignore unknown fields; the STOP 4 reader validates
the new bits when set; a `f1c.trace/2` bump is only needed if a breaking
change appears). Memory consequence in §5.2. Positions and RPM are already
mirrored on the game thread, so no new live read path and no game-thread work.

### 6.4 States, crossing, freeze, pause (design rules)

- Crossing: rc.3 rule kept verbatim — a +1 counter change in the final 50 m
  with continuous time and advancing distance is deferred until the physical
  wrap; larger jumps, missing boundaries, backwards distance and overflows stay
  refused; first partial lap is not offered as a completed lap.
- Freeze: pins the currently displayed laps/series (server snapshot under the
  same lock, bounded by the existing ring); live capture, SSE and the recorder
  continue; resume returns to the newest completed/open data. Freeze never
  mutates recorder state or exports anything not actually recorded.
- Pause/reconnect: repeated pause captures are skipped, resume opens a new
  segment, holes are never interpolated, and the client shows
  WAITING/STALE/TRANSPORT STALE instead of stale values dressed as live.

### 6.5 Legacy import (one more source, unchanged scope)

- `POST /trace/import/legacy` with a bounded body, dispatching to a C port of
  the proven decoders (`decode_lap` first, `decode_spt` second) with the same
  strict bounds/refusals as `tools/legacy_formats.py`; results convert into the
  existing trace record (identity, units, validity) and participate in export.
- Channel conversion table (import): `time` → elapsed s; `distance` → m;
  `velocity` → m/s (label kept km/h); pedals 0..100 → 0..1; `engine rpm`
  already RPM; `gear` float → int; yaw degrees → radians only if required
  (recorded as a conversion, never silently).
- UI: `Import legacy file…` on the Engineering tab; the imported lap is marked
  `legacy`/`partial` (first sample starts at distance 172.15 m in the observed
  file), draws where data exists, and is never presented as a native live lap.
- Tests: synthetic round trip (`build_synthetic_lap` fixture), hostile and
  truncated inputs, the real reference lap hash gate, the torn spt copy,
  conversion-table checks, bounded-import refusals — plus the live-workbench
  tests in §6.6.

### 6.6 Tests to add in STOP 4 for the live workbench

Partial-lap serving bounds; open-lap drawing with segment holes; crossing
fixture (early counter then wrap) reused for state badges; freeze/resume leaves
capture counters advancing; mid-lap selection revision reset; linked-cursor
distance math shared by A and B; RPM conversion label (raw rad/s must never be
labeled RPM); gear mapping; G-G point count/bounds; reconnect shows no
catch-up samples and no lines across gaps.

## 7. Port 8080 conflict — owned by the parallel port task (review item 4)

Observed (kept as evidence): Web Telemetry's HTTP failed with
`bind port 8080 failed: 10048` while `llama-server.exe` (PID 14736, local LLM)
held TCP 8080; the dashboard was unreachable while llama-server ran.

Ownership: the fix (ini `Port=`, default 8080, 8081-8085 fallback, `/health`
port reporting, visible bind failure, tests, candidate hash) is the scope of
the parallel task `../DEEPSEEK_PARALLEL_A_PORT_2026-10-09.md` and is **removed
from this dossier**. STOP 4 excludes all port work; this task does not edit
`web_telemetry.c` until the owner confirms task A has landed; if it lands
first, STOP 4 only integrates and verifies, with no duplicate implementation.
Finding `web-telemetry-port-8080-conflict-20261009` remains the evidence
record.

## 8. Open items

- spt header bytes 0x00-0x07 final state on graceful close (truncated sample).
- Which game setting feeds `dword_7CE9D0` → `dword_8AD23C` divisor (observed 40).
- spt event records (5 B) present but zero in the observed stream; payload
  semantics written but untested on a real event.
- .lap checksum algorithm; field-block semantics for the four unproven floats
  and 0x1C/0x20/0x24 values; trailer remainder (9,381 B − decoded block).
- Player steering native exposure (`P+0x000` filtered / `P+0x040` unfiltered
  candidate from the game recorder's own reads) stays **OPEN**. Exact live
  read-only check for the next owner race (review item 5):
  1. Launch through the guarded full-weekend route (never QuickRace); read
     only, no writes. `P = 0x7C7428` (FACT); sample `P+0x000`, `P+0x040` and,
     for cross-reference, the player's `veh+0x3580` at ≥ 10 Hz.
  2. Drive three holds at ≥ 40 km/h: 5 s wheels straight, 5 s full left, 5 s
     full right; include one pause/resume cycle.
  3. Record finite range, sign, and difference between filtered and unfiltered;
     compare the shape against the viewer-recorded .lap steering channel
     (spt #14-15, legacy proof) from the same corner.
  4. Pass (promote the row with two independent sources) only if values are
     finite, track the input with a consistent sign/scale, and are not a
     constant/denormal pattern through pause; otherwise keep OPEN and record
     the negative result in `findings.jsonl`.

## 9. Verification

- `tools/test_legacy_formats.py`: 56/56 PASS (real files in place + synthetic
  round trip + synthetic refusals + torn spt TEMP copy).
- `tools/test_vb6_pcode.py`: 30/30 PASS (unchanged parser).
- `tools/legacy_formats.py --json` regenerates `FORMATS_f1_telemetry_2026-10-09.json`.
- Workspace validator: same 2 pre-existing errors (`.claude`, `plans`), 0 warnings.
- All vendor files opened read-only; no vendor copies anywhere except the
  hashed %TEMP% torn .spt case; the game was closed during decoding.

STOP 3R revision verification (documentation-only pass):

- Sections 4-7, the open-items steering check and this verification block were
  rewritten; `../README.md` re-linked. No code, DLL, settings, game or viewer
  file was touched; no build or test run was needed for a text revision.
- The only new measurement used is the retained live capture
  `%TEMP%\f1c-web-telemetry-live-20261009\telemetry_sample1.json`
  (31,016 B, 22 cars, rc2 acceptance window 2026-10-09) for the §5.2 SSE
  payload line; arithmetic only, no new game session.
- Workspace validator re-run after the edit: same 2 pre-existing root errors
  (`.claude`, `plans`), 0 warnings.
- Edited sections re-read for UTF-8/mojibake and missing spaces before digits;
  before/after hashes in §10 and the paired findings entry.

## 10. Changed files and hashes

| File | SHA256 |
|---|---|
| `tools/legacy_formats.py` | `DAEB8203D65EC684E57E624582F306FAC8A0FBE1348FF7B4B8C8EB41BC435D03` |
| `tools/test_legacy_formats.py` | `7B7B7A6461034A254931C6A655B1705EBCD5A8A848A33524FDD17C513F65B42C` |
| `FORMATS_f1_telemetry_2026-10-09.json` | `3017CDF2966370F6C846FF0A19BCBD19417F6AA74D541393D025DBB9675F3531` |
| `FORMAT_SPECS_f1_telemetry_2026-10-09.md` | `BADF065551D6B2F2AEBCE41A0024DBDF96B243C2A225A592985AA3A21C8348F8` |
| `STOP2R_LEGACY_TELEMETRY_2026-10-09.md` (corrected in place) | `0F331102C6F5E72D7A5A08A7C85B33EC6EE4D94A1C9C095740E8813F1BA56AD9` |

Before-states: `workspace_organization_audit/LEGACY_TELEMETRY_STOP3_BEFORE_2026-10-09/`
(preserved before edits). No file was deleted or moved.

STOP 3R revision (2026-10-09): this record was revised in place (sections 4-7,
open items, verification, changed files) and `../README.md` was relinked; no
other file in this component changed (the findings append and the SDK
checkpoint update are recorded in §12). Before-state of both files:
`workspace_organization_audit/LEGACY_TELEMETRY_STOP3R_BEFORE_2026-10-09/` —
this record `47839CE4D4E1DAFFB786E16747F4DFE03D3970E97A7DB26794F35B6085DF5485`,
README `49E6815386B6EC2F68BCF6455D74E79B667291EC610A6F9C3A5179A638FD902E`
(SHA256). The after-state hashes are recorded in the paired findings entry
`web-telemetry-stop3r-live-workbench-20261009` so this file does not have to
hash itself. Nothing was deleted or moved; the earlier before-state directory
remains untouched.

## 11. Wasted-effort check

- One wrong hypothesis (the five wheel offsets in the in-progress draft) was
  caught by re-reading the writer and corrected before shipping; the before
  state is preserved.
- One probe script walked a mis-parsed block and was abandoned when the
  offsets diverged instead of being patched with guesses.
- The .lap sample-count scan was validated against the exact `0.0025 + 0.1k`
  time sequence instead of a heuristic stride mod.
- 3R pass: two code-line citations in the first draft of §5.2 were wrong
  (`web_telemetry.c` line 22 for slots, and a stale compare-grid line); both
  were re-checked against the source before the record was finalized.

## 12. Documentation gate

This record is indexed from `../README.md`; the specs, machine data, tests and
findings are linked; facts, method-level evidence and open items are separated;
viewer evidence stays tagged with the viewer SHA256; UTF-8 re-read performed.
The 3R pass answers every STOP 3 review item: item 1 (live player/AI workbench
mapping) in §6, item 2 (native position exposure, offsets cited) in §4, item 3
(live budgets) in §5.2, item 4 (port moved to the parallel task A) in §7, item
5 (steering kept OPEN with the exact live check) in §8. The paired findings
entry and the SDK checkpoint entry `legacy_telemetry_stop3_20261009` carry the
revision.

STOP 3R READY - check DS
