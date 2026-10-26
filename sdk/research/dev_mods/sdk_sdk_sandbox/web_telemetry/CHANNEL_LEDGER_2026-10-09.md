# Web Telemetry: player/AI channel ledger and performance budget

Date: 2026-10-09. Status: specification for the 4.2.0 engineering-comparison
candidate. Planning plus investigation only where noted; no game attachment,
installation or publication. Companion plan:
`DEEPSEEK_AI_COMPARISON_PLAN_2026-10-09.md`. This ledger is the bounded
specification that plan phase 0 requires.

Method: canonical SDK front door (`f1c_mod.py start/evidence-query`), reviewed
`findings.jsonl` entries, decompiled `functions/*` for writer/reader evidence,
`tools/sdk/f1c_vehicle_struct.h` / `name_index.json` for field naming, the
mechanical-failures live documents, and the rc.2 source. No new live capture was
possible (no game process running during this work); every row states whether
its evidence is static, previously live, or unverified.

Tier vocabulary (kept from the corpus): **FACT** = decompile/machine code or
direct live proof; **CONFIRMED** = two independent sources (per
`f1c_vehicle_struct.h` legend); **LIVE** = previously live-verified, cited;
**DERIVED** = arithmetic/role inference from facts; **OPEN** = meaning not
proven.

Vehicle record pool: `VEH_BASE 0x5EC6D8`, stride `0x48C0`, 104 slots
(`tools/sdk/f1c_sdk.h`). The rc.2 capture already copies every full record into
owned mirrors on the game thread, so every row marked "mirror" below needs no
new live read. Player physics object: `P = 0x7C7428` (FACT). Player wheel
record: `W(w) = P + 652 + 1124*w`, wheel order FL,FR,RL,RR screen-confirmed for
the 0x7C7A00 thermal block (`Wheel-thermal block screen-confirmed` FACT).

## 1. Channel ledger

### 1.1 Channels available for BOTH player and AI

| Channel | Source | Type / unit | Frame, sign | Writer / reader evidence | Tier | Cadence | Missing value |
|---|---|---|---|---|---|---|---|
| Speed | veh `+0x4440` (mirror) | float, m/s | car forward | `F1CVehicle.Speed FACT`; broker `speed`; live in every session record | FACT | per capture | 0 + invalid flag |
| Throttle factor | veh `+0x357C` (mirror) | float 0..1 | pedal | `F1CVehicle.ThrottleFactor CONFIRMED`; broker `throttle_factor` reads it for every car; rc.2 pedal bars live for player | CONFIRMED | per capture | 0 + invalid |
| Brake factor | veh `+0x3584` (mirror) | float 0..1 | pedal | `F1CVehicle.BrakeFactor CONFIRMED`; broker `brake_factor` | CONFIRMED | per capture | 0 + invalid |
| Gear | veh `+0x39FC` (mirror) | int32 0..7 | - | `CurrentGear FACT` | FACT | per capture | 0 + invalid |
| Lap number | veh `+0x403C` | int32 | - | `lapNumber CONFIRMED` | CONFIRMED | per capture | invalid |
| Lap distance | veh `+0x4044` | float, metres | 0 at start line, resets | FACT `f1c-lapdist-4044-not-4048`, live 2026-07-31 all-car speed/distance sweep; rc.1 native failure 2026-10-09 | FACT | per capture | invalid; +0x4048 player-only claim must not be generalized; do not use 0x6CC |
| Lateral acceleration | veh `+0x354C` (mirror) | float, m/s2 | car local, right positive (corr +0.99 with position-derived lat) | `ai-accel-inputs-mass-live-2026-09-30` FACT; player block same fields, live p99 3.4 g (`player-live-telemetry-f2002-2026-09-30`) | FACT | per capture | 0 + invalid |
| Longitudinal acceleration | veh `+0x3554` (mirror) | float, m/s2 | car local, forward NEGATIVE (slope -1.02 vs d(speed)/dt) | same two findings; player p99 3.8 g | FACT | per capture | 0 + invalid; display converts to forward-positive |
| Vertical acceleration | veh `+0x3550` (mirror) | float, m/s2 | car local | AI FACT (`ai-accel-inputs-mass-live`); player same vector block but the player finding cited only lat/long | FACT (AI) / DERIVED (player) | per capture | 0 + invalid |
| Heading / yaw | veh `+0x34BC..0x34E0` matrix | 3x3 yaw rotation matrix | `+0x34BC cos, +0x34C4 sin, +0x34D4 -sin, +0x34DC cos` | `f1c-car-heading-is-a-rotation-matrix-not-the-yaw-field` FACT (live write/read-back proof) | FACT | per capture | yaw invalid if cos/sin fail norm check |
| Yaw rate | derived | float, rad/s | wrapped | Derived from yaw + simulation-time difference; no native channel proven | DERIVED (method) | per capture | invalid across gaps/pause/replay |
| Tyre tread, player | `P` wheel block `W+916` via 0x7C7A00 mirror | float, tread 1.0 new, 0.75 destroyed | per wheel FL,FR,RL,RR | `tyre-thermal-wear-grip-model-2026-10-06` FACT; rc.2 already displays it | FACT | per capture | -999 sentinel / unavailable |
| Tyre tread, AI | veh wheel block `+0xB0` = `0x363C/0x3734/0x382C/0x3924` (RR,RL,FR,FL per F1CUI names) | float, tread 1.0 new, floor 0.75 | per wheel | **NEW static proof:** `FuelConsumptionAndWear_004ed960` line 82-92 decrements `this[62k+3468]` (veh+0xC+13872 = wheel+0xB0) with floor 0.75 and `dword_7D0C60` tyre-wear option; live movement observed on AI cars 2026-07-25 (`OFF_TIRE_WEAR_*` comment). Wheel identity from F1CUI field names (CONFIRMED offset, labels third-party) | CONFIRMED | per capture | 0 + invalid; never copy player global values |

### 1.2 Channels available for AI only

| Channel | Source | Type / unit | Frame, sign | Evidence | Tier | Notes |
|---|---|---|---|---|---|---|
| Steering input factor | veh `+0x3580` (mirror) | float, input factor (observed +-0.28) | left/right, sign not independently proven | `ai-accel-inputs-mass-live` FACT; `AIMainUpdate` writes it every AI tick (`ai-brain-pipeline-and-modes`, `ai-mode-4-native-park`, `ai-forced-pulloff-trials`) | FACT (AI) | Scale is an input factor, not degrees; no steering-angle conversion contract exists. Player writer NOT found (see 1.3). |

### 1.3 Channels investigated and NOT available

| Channel | Candidate | Why it is not exposed | Evidence |
|---|---|---|---|
| Player steering input | veh `+0x3580` | Every writer found in the decompiled corpus is AI or replay: `AIMainUpdate` (AI), `VehiclePhysicsTimeStep` (replay/network state 2 only). No player input writer was located; the canonical header leaves `0x3580` unnamed. Reading it for the player would be a provisional offset masquerading as data. | rg over `functions/*` for writers of 13696; `F1CVehicle` header gap_3580; `SetupCarLightingAndRender` reads it for the replay/network packet. Revisit with a live `noPlayer`-vs-player comparison or a wider decompile. |
| Suspension compression (per wheel) | AI wheel `+0x10` anchored at `0x358C` | Dynamic correlation only ("0.036-0.105, rises with speed"); no writer/reader traced. The wheel array's true base is `veh+0x358C` stride `0xF8` (static: `SuspensionAndWheelPhysics_004e5f60`, `PhysicsIntegrationStateMachine_004e4e60` stride-62-float math). Zero reference, sign and units unproven. Player path not proven at all. | `ai-wheel-block-live` DERIVED; this ledger's static pass. |
| Tyre vertical load (per wheel) | AI `+0x44/+0x4C/+0x50` | `+0x4C` is a per-wheel torque CAP written by `sub_4E51A0` (`v7 = torque_coeff*throttle + offset`), not a load. `+0x44/+0x50` are positive load-ish factors used by the AI tread-wear equation, but units, filtering and axle mapping are unproven. Player `W+796` is used as "tyre load" inside the optimum-pressure formula (FACT use) but has no established unit or writer. | `sub_4E51A0_004e51a0.c`; `FuelConsumptionAndWear_004ed960.c`; `NATIVE_SIMULATION_MAP_2026-10-06.md` section 11.2. |
| Tyre forces (per wheel) | AI `+0x80/+0xD8/+0xE0` | `+0xD8` is written per wheel by `WheelForceParameterSet_004f1ba0` (a force/torque parameter for AI cars; the player goes through `ForceTorqueApplicationToRigidBodyWithInertiaTens` instead) - role not final-output proven. `+0x80/+0xE0` remain correlation-only. No player persistence found. | `WheelForceParameterSet_004f1ba0.c`; `ai-wheel-block-live` DERIVED. |
| Wheel spin / per-wheel steer angle | AI `+0x20/+0x28/+0xC0` | Correlation-only (spin -1.00 with speed is suggestive but not a semantic proof); wheel identity unproven. | `ai-wheel-block-live` DERIVED. |
| Brake temperature (AI per wheel) | `+0x5C` | **Corrected:** `SuspensionSpringDampingRateReset_004e4f40` writes `wheel+0x5C` with a spring/damper value, not a temperature. The dynamic "576-1870, falls at speed, probably C" guess conflicts with the static writer. Do not expose. | `SuspensionSpringDampingRateReset_004e4f40.c` lines 49-51. |
| Compound-parameter fields | AI `+0x9C`, `+0x84..0xB8` | `sub_4F18C0` writes these from the compound record (`DivisionWithThreshold(v9[30],v9[31])` etc.) when a compound is applied; they are not a wear signal. The dynamic "slow decline" observation is not a proven semantic. | `sub_4F18C0_004f18c0.c`; `TireCompoundPerWheelPhysicsApplication` dispatch at line 50-54. |

Corrected wheel-block map (static, this pass): base `veh+0x358C`, stride
`0xF8`; wheel `+0x48` applied drive torque (zeroed then set per driven wheel),
`+0x4C` torque cap, `+0x5C` spring/damper rate, `+0xB0` tread (AI wear),
`+0xB4` wear-rate coefficient, `+0x60` suspension geometry offset (failure
mode 4 writes +0.05). The `ai-wheel-block-live-2026-09-30` role list retains
its DERIVED tier and is superseded for `+0x4C`, `+0x5C` and `+0x9C`.

## 2. Trace record and validity model (specification)

One versioned trace record per (session generation, vehicle slot, driver id,
lap number). Sample fields: simulation timestamp, capture monotonic timestamp,
lap distance, elapsed lap time, speed, throttle, brake, steering (AI only),
lateral/longitudinal/vertical acceleration, yaw, yaw rate, four tread values,
and a per-sample validity bitfield. Native observed samples are recorded; no
interpolated values are stored. Interpolation happens only when serving the
comparison grid.

Validity rules:
- Sample accepted only while session active, `on_track`, `lap_dist` in
  `(0, lap_len]`, lap number matches the open lap, simulation time advancing.
- Segment breaks (never interpolated across): simulation-time gap > 1 s, lap
  change, `on_track` false, pit status > 0, `lap_dist` step < -2 m (reverse) or
  > 50 m (teleport/reset), session generation change.
- Yaw valid only when the rotation matrix passes a norm check; yaw rate valid
  only within one segment with `0.001 s <= dt <= 1 s` and wrapped continuity.
- Per-channel flags: speed/throttle/brake/accel require finite values within
  physical bounds; steering requires AI control; tread requires 0..1.
- A lap is `complete` only after two witnessed continuous start-line crossings
  (adjacent lap numbers, simulation gap <= 1 s, combined wrap distance <= 50 m).
  First partial, missed crossings, rewinds and sample overflow are discarded;
  the final open lap is not offered for comparison. Pause duplicates are skipped.
  Recording interruptions set sample flag 2048 so even short gaps stay holes.
- Lap time delta is computed at equal distance as `elapsed_B - elapsed_A`
  (positive = B slower) only between valid segments on the same track/layout;
  mismatched or incomplete references are rejected.

Retained data per selected car: 6 completed laps plus one open lap, each <= 2400 samples (about 2 minutes of
green running per lap at 20 Hz). Older laps are dropped with an explicit
counter. Exports are versioned JSON and CSV, bounded, serialized off the game
thread.

## 3. Performance budget (concrete, measured in the synthetic harness)

| Item | Budget | Rationale |
|---|---|---|
| Capture callback added work (2 traced cars) | p95 < 500 us, max < 2000 us | Broker update budget is 2000 us (`F1C_BROKER_UPDATE_TIME_BUDGET_US`); the callback already copies ~1.9 MB of mirrors. Trace sampling only reads owned mirrors. |
| Worker per-tick added work (trace update) | < 100 us typical | Two cars x ~60 B copy + O(1) logic at capture rate. |
| Trace static memory | <= 2.2 MiB total | rc.2 explicitly reserves an additional open-lap slot: 2 x 7 x 2400 x 64 B = 2,150,400 B plus bounded metadata. This avoids exposing overwritten historical laps; measured capture+read budget is retained. |
| Trace HTTP serialization | bounded by 524288-byte static buffer per trace response, off game thread | Same bounded pattern as the existing JSON endpoints (the trace buffer is larger because one lap of samples is serialized); refuse overflow instead of truncating. |
| Comparison grid | requested 1..50 m step, <= 1000 points | Both C and browser expand step to lap_length / 999 when needed; the grid covers the full track rather than truncating it. Response uses the 524288-byte bound. |
| Capture rate | 20 Hz for the selected pair (proposed), 10 Hz SSE whole-field context unchanged | 20 Hz is a design target to be confirmed by the measured callback cost; it is not an existing measured capability and does not resolve solver transients. |
| Frame-time impact | human validation gate; synthetic proxy = callback budget | Native FPS measurement remains a pending human/native gate. |

Abort conditions (from the plan, kept): missing layout proof, unreadable or
ambiguous ownership, unresolved data type, invalid session identity,
unexplained stale samples, or budget overrun stop the affected channel; proven
basic channels stay usable when an advanced capability fails.

## 4. Evidence trail

- `findings.jsonl`: `ai-accel-inputs-mass-live-2026-09-30`,
  `player-live-telemetry-f2002-2026-09-30`,
  `ai-wheel-block-live-2026-09-30`, `noplayer-mode-all-ai-physics-2026-09-30`,
  `f1c-car-heading-is-a-rotation-matrix-not-the-yaw-field`,
  `tyre-thermal-wear-grip-model-2026-10-06`,
  `brake-model-seeding-and-mode-gate-2026-10-06`, plus this task's appended
  entries.
- Decompiled writers/readers cited inline above; files under
  `f1c_ida_decompiled/functions/`.
- Live documents: `AI_DYNAMIC_ANALYSIS_2026-09-30.md`,
  `NATIVE_SIMULATION_MAP_2026-10-06.md`, `HANDOFF_2026-09-30b.md`.
- Reviewed ABI: `tools/sdk/f1c_broker_abi.h`, `f1c_vehicle_struct.h`,
  `name_index.json`.

## 5. What 4.2.0 implements against this ledger

1. Distance-aligned player/AI and saved-run comparison for every channel in
   sections 1.1 and 1.2 (steering AI-only, clearly marked unavailable for the
   player).
2. Unavailable advanced channels (section 1.3) are named, reasoned and shown as
   unavailable in the UI - never as zeros.
3. Recording, replay, exports, validity indicators, run metadata and the
   baseline-vs-modified comparison workflow (repeated-lap statistics with
   exclusions).
4. Dashboard corrections C02, C06, C07, C08, C09, C10 and C15 from
   `UI_CORPUS_REVIEW_2026-10-08.md` as they intersect the comparison data path.

rc.2 corrections and exact verification: [Repair record](REPAIRS_2026-10-09.md).
The `ai` export field describes AI control during the recording, not physics mode.
Identity/control changes reset recording revision; terminal control-bit clearing does not relabel historical laps.
The installed broker may return the empty FNV hash 2166136261: telemetry normalizes it to unknown and uses a nonempty exact car/team-name fallback for teammate matching.

## 6. Legacy dossier mapping (STOP 3, 2026-10-09)

The legacy recorder (game-written `telemetry.spt`, viewer-written
`reference_*.lap`) is now fully decoded; the mapping of its channels onto this
ledger, the RPM scale result (`engine-rpm-live-scale-20261009`: `+0x39A0` is
rad/s player and AI, RPM = × 9.549296585513721), the measured decode budgets
and the workbench/port design are in
[the STOP 3 record](legacy_dossier/STOP3_LEGACY_TELEMETRY_2026-10-09.md) §4-§7
and [format specs](legacy_dossier/FORMAT_SPECS_f1_telemetry_2026-10-09.md).
One new native candidate from the legacy writer evidence: the player steering
source (`P+0x000` filtered, `P+0x040` unfiltered) recorded by the game; not yet
live-verified, so section 1.3 stays unchanged until that proof exists.


## 7. 4.3.0-rc.1 additions (2026-10-11)

This section implements the ledger and does not change any tier. Wheel order
is FL, FR, RL, RR everywhere. The AI wheel-block array index for that order is
3, 2, 1, 0 (F1CUI tread labels; the order is CONFIRMED by labels, not
independently proven).

| Channel | Source | Exposure | Tier |
|---|---|---|---|
| Player tyre surface temperature | Thermal mirror `0x7C7A00`: mean of W+844/852/860, Kelvin to C | `/telemetry ttc`, trace `tt`, viewer Tyres | FACT |
| Player carcass temperature | W+868 (mirror +0x18) | `/telemetry tcc`, viewer Tyres | FACT |
| Vertical accel, yaw (per car, live) | veh+0x3550, heading matrix | `/telemetry gv`, `yaw` (were trace-only) | FACT (player vertical: DERIVED, per 1.1) |
| AI tyre temperature candidate | wheel+0x50, Kelvin; initialised to ambient + 273.16 in `SuspensionAndWheelPhysics_004e5f60.c:477` | `ttc` with `vf6` 256; trace `tt` with `TRF_TTEMP_RX` | RESEARCH (static DERIVED: initial value only) |
| Suspension deflection candidate | wheel+0x10 | `rx[0..3]` | RESEARCH (correlation only, 1.3) |
| Vertical load candidate | AI wheel+0x44; player W+796 (new mirror `0x7C79D0`, 3,380 B) | `rx[4..7]` | RESEARCH (unit unknown; W+796 is the load term of the optimum-pressure formula) |
| Slip-angle-like candidate | wheel+0x38 | `rx[8..11]` | RESEARCH |
| Per-wheel force parameter | wheel+0xD8 (`WheelForceParameterSet_004f1ba0`) | `rx[12..15]` | RESEARCH (not proven to be solver output) |

**Promotion rule.** A RESEARCH field becomes a named physics channel only after
all three of these:
1. A guarded live capture with sim time advancing.
2. The physical checks the viewer's research view computes:
   - load: left/right opposite with lateral g, front/rear opposite with
     longitudinal g, sum near the static weight plus downforce;
   - deflection: follows load and rises with speed;
   - force/load: below a plausible μ.
3. A `findings.jsonl` entry with tier and evidence.

Until then the viewer keeps these fields behind the research toggle, unitless.
