# H1 research: telemetry channels present for the player but missing for AI cars

- Date: 2026-10-14. Author: Matias. Task type: read-only static research (task H1).
- Scope: `mods\sdk_sandbox\web_telemetry`. No code edited, no files moved or deleted, game not started, no .exe run, api1.txt and keys not read.
- Vehicle array: base 0x5EC6D8, stride 0x48C0, slot 0 = player (FACT, from `f1c_ida_decompiled\CLAUDE.md` and `tools/ask.py`).
- Result in one line: the mod already reads AI wheel spin and AI brake temperature. AI wheel speed is published as `aws` in JSON, but the engine/gearbox page draws only the player block. AI tyre temperature, AI clutch input and any AI tyre temperature aggregate have no valid source in the evidence. Clutch input for AI is OPEN. The only AI tyre-temperature candidate is DISPROVEN.

## Key finding: player chassis data lives in one static object

`VehiclePhysicsModeDispatcher` 0x4E4930 switches on veh+0x32C8 (`functions\VehiclePhysicsModeDispatcher_004e4930.c`):
- mode 0 (player): `VehiclePhysicsIntegrationStep(&0x7C7428)`, one static full-physics object (P).
- mode 1 (AI): `PhysicsIntegrationStateMachine` 0x4E4E60, per-car float wheel blocks at veh+0x358C, stride 0xF8.

Source: `f1c_ida_decompiled\findings.jsonl` `full-physics-player-only-dispatch-20261013` (FACT). Every CT player channel (tyre temps, pressure, camber, grip, disc wear) lives only in P.

## Evidence commands and excerpts

```
python tools/f1c_mod.py start --json
  "state": "BROKER_DETERMINISTIC_REGRESSIONS_GATED"
  "stale": true; warning: checkpoint summary predates newer dated entries

python tools/f1c_mod.py evidence-query findings "wheel speed" --json
  total 1: player-tyre-deflection-camber-w804-w788-20261013 DERIVED "W+188 wheel spin"

python tools/f1c_mod.py evidence-query findings "clutch" --json
  total 4: FACT veh+0x39FA stuck-gear byte is AI-ONLY (player never enters sub_4EF930)

python tools/f1c_mod.py evidence-query findings "tyre temp" --json
  total 13: CONFIRMED player tyre temps at 0x7C7A00 stride 1124, PLAYER CAR ONLY (+0x00/08/10 carcass zones, Kelvin)
  CORRECTED row: earlier base 0x7C7AB8 was wrong; correct base 0x7C7A00
  AI row: "AI tyre temperature ... never valid in the captures checked"

python tools/f1c_mod.py evidence-query findings "tire temp" --json
  total 0

python tools/f1c_mod.py evidence-query findings "wheel spin" --json
  total 4: ai-wheel-block-live-2026-09-30 DERIVED "+0x20 wheel spin" (correlation only)
  ai-wheel-block-roles-static-20261013 DERIVED "+0x30 wheel spin"

python tools/f1c_mod.py evidence-query findings "wheelspin" --json
  total 2: CONFIRMED wheel order FL,FR,RL,RR in TelemetryDataDump_004d4a80.c:172-190

python tools/f1c_mod.py evidence-query findings "player-only" --json
  total 9: RETRACTED sub_4BCCC0 player-only inference (sub_41A720 is the AI-capable path)

python tools/f1c_mod.py evidence-query findings "full physics" --json
  total 1: FACT full-physics-player-only-dispatch-20261013

python tools/f1c_mod.py inspect 0x4E4930
  Callees: 0x441650 VehiclePhysicsIntegrationStep, 0x4E4E60 PhysicsIntegrationStateMachine

python tools/f1c_mod.py inspect 0x4C77D0   (TireTemperatureSimStep = sub_4C77D0)
  Caller: 0x4B3BA0 IterateVehicles. Body: timer on this[4654], SetupCarLightingAndRender, sub_4EF2E0.
  No tyre-temperature write. The name is misleading.

python tools/ask.py offset 0x1E38        (VEHICLE struct +0x1E38, not the mod's player-object field)
  runtime distinct=1 kind=static sample 0x00000000 [OBSERVED]
  readers include 0x443B70 Gearbox (AI shift logic)

python tools/ask.py offset 0x35BC        (AI wheel 0 +0x30)
  writers 004e5f60 SuspensionAndWheelPhysics; runtime distinct=21 kind=float sample 0x413F78E8 (~11.95)

python tools/ask.py offset 0x35DC        (AI wheel 0 +0x50, tyre temp candidate)
  runtime distinct=4 kind=float sample 0x3F7FA718 (~0.999, NOT Kelvin)
  writers 004e5f60 SuspensionAndWheelPhysics, 004ed810 WheelTireForceUpdate

python tools/ask.py findings "clutch"    -> 4 hits, none about AI clutch input
python tools/ask.py findings "AI clutch" -> 0 hits (evidence-query)

grep "tire_manager|TireTemp|WheelSpeed|clutch" functions\*.c docs\*.md tools\*.md
  functions\*.c: TireTemperatureSimStep (IterateVehicles_004b3ba0.c:9); clutch names in gearbox/config only
  docs: only DAT_ global rows (12-globals-reference.md 105, 748, 6143, 8699); no tire_manager symbol
```

## Channel table

| # | Channel | Mod read path (file:line) | Player source | AI available? | Offset / function | Tier | Evidence |
|---|---|---|---|---|---|---|---|
| 1 | Wheel speed (engine/gearbox page) | Player: `web_telemetry.c:4554` pch_spin from PW(w,188); JSON `ws` at `:5827`. AI: `:4654-4657` aws[w] from wheel block +0x30; JSON `:6059`. Viewer: player row `telemetry_viewer.js:5181`; AI block `:5153-5156` | P+652+1124w+188 (W+188) | YES, as `aws` in JSON. Engine page draws player only | AI wheel block veh+0x358C+0xF8*w+0x30 (`AI_WB_SPIN`, `:567`) | DERIVED (AI). Offset conflict: +0x20 in `ai-wheel-block-live-2026-09-30` vs +0x30 in `ai-wheel-block-roles-static-20261013`. Mod uses +0x30 | `ask.py offset 0x35BC`: writer SuspensionAndWheelPhysics, sample ~11.95 (one sample). Ledger row 129 "Wheel spin AI +0x30 DERIVED". Ledger 185: live corr 0.996 with speed (09-30 capture) |
| 2 | Clutch (driver input) | `web_telemetry.c:4558-4564` raw_off[3]=0x20, used_off[3]=0x18 into pch_in_raw/used[3] | P+0x18 (used), P+0x20 (raw) | OPEN. No AI clutch input field found. Gearbox 0x443B70 touches veh+0x1E38 and veh+0x1C5C (shift logic, not an input) | Player only. Mod pch_clutch_rpm = P+0x1E38 (`:4570`), not veh+0x1E38 | Player FACT (ledger line 370). AI OPEN | `ask.py offset 0x1E38` is vehicle struct, distinct=1 sample 0. Different base; not evidence for the mod field. AI clutch findings: 0 |
| 3 | Tyre temps, every field (inner/middle/outer per wheel) | Player: `web_telemetry.c:5179-5181` tire_temp[w][z] from block 0x7C7A00 +0x00/08/10 (`WH_OFF_TIRE_TEMP`, `:495-497`), temp_sanity(K - 273.16). AI candidate: `:672` WB_OFF_TEMP_K=0x50, read `:4638-4641` | P-block 0x7C7A00, stride 1124, Kelvin | NO valid AI source. +0x50 candidate disproven in captures | AI candidate veh+0x358C+0xF8*w+0x50 (initialised at `SuspensionAndWheelPhysics_004e5f60.c:477`) | Player: CONFIRMED (2 sources), FACT live per ledger. AI: DISPROVEN for the +0x50 candidate. AI absence DERIVED | Ledger 185: candidate initialised ambient+273.16, initial value only. Findings: "/telemetry ttc is -999 on all 23 AI cars". `ask.py offset 0x35DC` sample ~0.999 (not Kelvin). Sweep `ai-no-tyre-temp-pressure-disc-wear-sweep-20261013`: no AI tyre-temperature code except brake temp +0x5C |
| 4 | Front tyre, rear tyre, averaged tyre temps | Mod emits per-wheel ttemp[4] = mean of 3 player zones at `web_telemetry.c:4620-4626`, JSON `:6054`. No front, rear or average field in `web_telemetry.c` (grep tfront/trear/tavg = 0) | Player only. Average = mean of zones | NO (no AI source) | n/a for AI. Player zones at 0x7C7A00 | Field absence FACT (grep). Average formula DERIVED (code). AI as channel 3 | `web_telemetry.c:4626` `c->ttemp[w] = sum / (float)TIRE_TEMP_ZONES;` |
| 5 | Wheel spin | Player `web_telemetry.c:4554` (W+188). AI `:4657` (+0x30) | P+652+1124w+188 | YES (`aws`), DERIVED, offset conflict open | veh+0x358C+0xF8*w+0x30 | DERIVED (AI). Player TC wheelspin live FACT per `player-damage-aero-pit-bias-tc-live-20261013` | As row 1 |

## Assumptions, corrections and conflicts

- The question said "AI has no wheel speed". Corrected: the mod DOES read AI wheel spin (`aws`) from the AI wheel block. What is missing is the engine/gearbox page render for AI (`telemetry_viewer.js:5181` uses `p.ws` only). AI spin offset conflicts across records (+0x20 live 09-30 vs +0x30 static 10-13). The mod uses +0x30.
- `TireTemperatureSimStep` (0x4C77D0) is a misnamed timer/lighting routine, not a tyre-temperature model. Do not use this name as a tyre-temp lead.
- `ask.py offset 0x1E38` and `ask.py offset 0x35DC` are vehicle-struct lookups. The mod's clutch field is P+0x1E38 on the player object. Do not merge the two.
- Ledger line 36 (player tyre temp = mean of W+844/852/860) disagrees with the code and the CONFIRMED findings (zones at +0x00/08/10 of block 0x7C7A00). Not resolved here. The code is used in this record.
- The `ask.py offset 0x35DC` (~0.999) and `0x35BC` (~11.95) samples are single runtime samples. They are not proof of role.
- AI clutch: no evidence found. Tier OPEN. A live test is needed.

## Pending validation (not done in this task)

1. Live check of AI wheel spin offset (+0x20 vs +0x30) with an AI-car capture while braking and accelerating.
2. Live check of the player zone order (inner/middle/outer = zones 0/1/2) under braking.
3. Locate an AI clutch-equivalent input in Gearbox 0x443B70 or its callers, if one exists. Static search only.
4. Correct the misleading `TireTemperatureSimStep` label in name_index.json. No edit made here.

## Changed files

- Created: this file only.
- Appended: `f1c_ida_decompiled\findings.jsonl` (5 lines, ids prefix `h1-aichan-`).
- Not edited: component README, CURRENT_STATE.md or any index. The record is discoverable by file name and the five findings lines. No README index entry was added, since that is outside the requested output.
