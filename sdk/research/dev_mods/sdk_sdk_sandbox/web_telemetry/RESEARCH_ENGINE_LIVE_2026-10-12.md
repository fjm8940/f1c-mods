# Engine live statistics research (Batch 2f, read-only) - 2026-10-12

Owner: Matias. Author: Claude research agent (read-only; no source edits; no memory writes or pokes).
Scope: what per-car engine data the Web Telemetry console can show (hp, Nm, rpm/limiter, engine life, oil/water temperature, wear, MF values), with evidence tiers.
Tiers: FACT (in-game or screen proven) > CONFIRMED (two independent sources) > DERIVED (grounded, unproven) > OPEN (question) > GUESS.

Nothing in this document is a verified horsepower or Newton-metre value. No torque curve table was located in this pass.

## 0. Cold-start and sources read

- AGENTS.md (full, 141 lines) read; front door `f1c_mod.py start --json`: state BROKER_DETERMINISTIC_REGRESSIONS_GATED, freshness not stale, last entry `web_telemetry_zzzz_luna_rc14_20261009`.
- Findings queried through `evidence-query findings` (engine, torque, rpm, lifetime, oil, water, temperature, failure, gearbox, 0x3D78). Relevant entries read in full from `f1c_ida_decompiled/findings.jsonl` (line numbers below).
- Source read: `mods/sdk_sandbox/web_telemetry/web_telemetry.c` (offset defines 590-640, JSON emitters 1045-1140, life/oil reads ~4300, UI string 5252, 6151-6199); `telemetry_viewer.js:3430-3451`; `functions/Gearbox_00443b70.c` (lines 160-282); `config_schema.json` parser `EngineSectionParser_004f3c80`; `development/mods/mechanical_failures/dll/mechanical_failures.c` (lines 70-90, 218-230).
- Live read-only: `GET http://127.0.0.1:8080/telemetry`, `/driver?id=0`, `/health` (v4.4.0-rc.1, api 4, broker_abi 15). `f1c-memory` `f1c_health` (read-only).
- Not read: api1.txt, private keys, game install. No game memory was touched by this agent.

## 1. Engine physics in game (torque, power, rpm, limiter, braking, boost)

### 1.1 Torque output location
- FACT (findings.jsonl "player engine output", line 241 and 261): the player engine torque output is the double at `0x7C7430` = player physics object `0x7C7428` + 8. The decompile writes it at `this+8` in `Gearbox_00443b70` (`functions/Gearbox_00443b70.c:173-282`). Here `this` is the static object `0x7C7428` for the player (finding line 258, Frida census: one distinct ecx). It is NOT a vehicle-struct offset.
- For AI cars the same function runs with `this` = veh+0xC (finding line 192, FACT). Torque is then `veh+0xC+8` = veh+0x14. Not verified live.
- CONFIRMED static (Gearbox_00443b70.c:160-282): the torque value is a multiplier chain, not a Newton-metre number. Steps:
  1. Rev-limiter ramp: `v16 = (this[7404]-rpm)/(this[7404]-this[7396])`, clamped at 0, applied as `this[8] = min(this[8], v16)`. rpm = `this+7260`.
  2. Idle/launch ramp via `this+7292`, `this+7300` (idle), `this+7308`.
  3. Stress misfire (finding line 261): `torque *= 1 - v36^2 * stress` where `v36` samples a 256-entry random table `flt_8AAE74` at index `11.5 * rpm/maxRPM * SessionTime`.
  4. Final: `this[8] = RandomPhysicsParameterGenerator(this) * this[8]`, and `RandomPhysicsParameterGenerator` (`sub_447B10`) returns 0 when the engine RUNNING byte `this+7257` is 0 (finding line 242).
- DERIVED: `this+8` is a dimensionless 0..1 torque fraction (clamps at 0 and at min() against previous values). Units in Nm are NOT established. Converting it to Nm needs the engine's peak torque, which is not located (see 1.3).

### 1.2 RPM, rev limit, idle
- FACT/CONFIRMED (findings line 266): engine speed on the player object is `this+7260` in rad/s; max rpm `this+7388`. Live read 2026-07-31: this+7388 = 1874.5 rad/s = 17900 rpm, which equals RevLimitRange base 17400 + RevLimitSetting 5 x step 100 (engine.ini of Williams 2001_FW23). Multiply by 60/(2 pi) for rpm.
- FACT (findings line 192): veh+0xC-based engine object fields: `veh+0x39A0` engine speed, `veh+0x39A4` rev limit (1874.5 in the same car), `veh+0x39A8` idle, `veh+0x39FC` CurrentGear (int32). Frida: the object base is veh+0xC.
- CONFLICT / ARITHMETIC ERROR (OPEN, see O-1): findings line 246 labels `0x7C90C4` as `this+7260`. `0x7C7428 + 7260 = 0x7C9084`, not `0x7C90C4` (`0x7C90C4` = this+7324). Also the same line reads a constant 436.28 at that address. Do not use `0x7C90C4` as rpm. The rpm source for the player is still OPEN; `veh+0x39A0` (web_telemetry OFF_ENGINE_RPM, line 627) is the one used by the live `/telemetry` emitter.
- FACT (findings line 192): veh+0x39A0 is recalculated every frame (write restores within 30 ms). Read-only sampling is fine; do not use it as a control input.

### 1.3 Torque(rpm) curve, power, Nm and HP
- OPEN (O-2): the torque-vs-rpm table was NOT located. `config_schema.json` contains only parser extraction (`_meta` says 36 parsers, 775 fields, 84 suspect), not a curve table. Tokens found in `EngineSectionParser_004f3c80` (offsets relative to the engine object base, which is itself unverified): IdleThrottle 0x69C, IdleRPMLogic 0x68C/0x694, LaunchEfficiency 0x6A4, LaunchRPMLogic 0x6AC/0x6B4, RevLimitLogic 0x6BC, EngineInertia 0x684, FuelConsumption 0x674, FuelEstimate 0x67C, LifetimeEngineRPM 0x1310/0x1318, LifetimeOilTemp 0x1320/0x1328, LifetimeAvg 0x1330, LifetimeVar 0x1338, OptimumOilTemp 0x12C8, CombustionHeat 0x12D0, EngineSpeedHeat 0x12D8, OilMinimumCooling 0x12E0, OilWaterHeatTransfer 0x12E8/0x12F0, WaterMinimumCooling 0x12F8, RadiatorCooling 0x1300/0x1308, StarterTiming 0x135C.
  - Not in this parser: any torque curve token, any boost/turbo token, any engine-brake token. Negative result within this parser only (a `POWER` token at 0x44 belongs to another parser, `sub_565870`, not confirmed to be the engine).
- DERIVED: a live Nm or hp number needs one of: (a) the .eng/.hdv torque curve token located and parsed (not done here); or (b) a measured calibration of `this+8` against a known peak. Neither is proven. HP = Nm x rpm_rad_s / 745.7 only after (a) or (b).
- Engine braking, boost, and mapping: NOT FOUND. The engine parser has no such token. Vanilla boost is probably absent in F1C; this is a GUESS until a full token search of `function sub_565870` tables is done.

### 1.4 Units and transforms (summary)
| Quantity | Address (player object 0x7C7428) | Unit | Tier |
|---|---|---|---|
| engine rpm (candidate) | this+7260 (0x7C9084) | rad/s | OPEN (address conflict) |
| max rpm (rev limit) | this+7388 (0x7C9104) | rad/s | CONFIRMED value, address unverified |
| torque fraction | this+8 (0x7C7430) | 0..1 (DERIVED) | FACT location |
| engine running | this+7257 (0x7C9081) | byte 0/1 | FACT |
| stress (power loss) | this+7596 (0x7C91D4) | 0..0.6 cap | FACT |
| engine life | this+7604 (0x7C91DC) | seconds | FACT |
| banks white/black/flames | 0x7C920C / 0x7C9214 / 0x7C921C | doubles, negative = on | FACT |
| oil temp (HUD) | 0x7C9174 | Kelvin (display -273.16) | FACT |
| water temp (HUD) | 0x7C91B4 | Kelvin | FACT |
| brake disc temp per wheel | base 0x7C7A00 + 1124*i + 0xB8 | Kelvin | FACT (CORRECTED record, findings line 142) |
| tyre temp zones per wheel | base + 1124*i + 0/8/0x10 | Kelvin | FACT |

Transform to rpm: `rpm = rad_s * 60 / (2*pi)` (web_telemetry `LF_RPM_FACTOR`, line 1067 emits both).

### 1.5 Engine-running gate and seizure
- FACT (line 242, 249, 250): `0x7C9081` (this+7257) is the RUNNING/ignition flag. Writing 0 kills torque through `sub_447B10`. Ignition key toggles it (live, 4 presses, 4 flips). Writer of the ignition key path still unlocated (line 250, OPEN).

## 2. Engine life and lifetime

### 2.1 What the game value is
- FACT (findings line 259, 2026-07-31 live): `0x7C91DC` (double, seconds) is the PLAYER engine-life countdown in `EngineStressSimulation_0043ef20` (static object `0x7C7428`). Driving it to zero fires a staged engine failure (ReasonCode 1 at veh+0x4068, re-arm, stress seed, bank roll). Confirmed by two live runs.
- FACT (findings line 240): the countdown `life` is updated as `life -= ((rpmCurve + oilTempCurve)*0.5 + stress) * dt`.
- CONFIRMED (line 262): measured drain rates: about 0.14 s/s at stress 0 (idle), 0.28 at stress 0.14, 0.66 at stress 0.6. Consistent with the formula with stress 1:1. The two curves `rpmCurve`, `oilTempCurve` have not been measured live (game was paused during the attempt). Their likely token sources: `LifetimeEngineRPM` (0x1310/0x1318) and `LifetimeOilTemp` (0x1320/0x1328).
- FACT (web_telemetry.c:6151): engine.ini `LifetimeOilTemp=(123.43,2.90)` halves engine life per 2.9 C above 123.43 C (the comment in our own code; source not re-verified in this pass).
- FACT (findings line 241/259): the life is seeded from `LifetimeAvg + noise*LifetimeVar` for the player too (`Williams 2001_FW23`: LifetimeAvg 9136, LifetimeVar 3027.09). OPEN (line 268): observed seeds 11945 / 11899 / 12500 exceed the stated ceiling 12163. The seed formula is not what the tokens say.
- AI cars: `veh+0x3D70` (engine), `veh+0x3D74` (brakes), `veh+0x3D78` (gearbox), `veh+0x3D7C` (suspension). These are seconds countdowns, seeded in `EngineWearFailureSimulation_004ee150` as `LifetimeAvg*3 + noise*LifetimeVar*3` (finding line 239). They tick only when `veh+0x32C8 == 1` (AI). Player: 0x32C8 == 0 so they NEVER tick (FACT, finding 239 + web_telemetry.c:600-605).
- FACT (finding 267): `flt_8ACF9C` is a race-distance fraction, so the "timescaled" failure-rate divisor is a no-op at 100% distance.

### 2.2 Our "ENGINE LIFE 2.2h (95%)" display
- `telemetry_viewer.js:3430-3451`: `ENGINE LIFE` shows `lifeTxt(fc.lfe, fc.lpe, fc.ll)` ONLY when `fc.wg === 1`; otherwise it prints "n/a - not simulated". `fc.lfe` is the `veh+0x3D70` value (AI countdown), so the viewer is correct to hide it for the player.
- `web_telemetry.c:5252`: the hour format is `sec/3600` with one decimal: 2.2h = 7920 s +/- 180 s.
- `web_telemetry.c:1821-1841` and `4310-4311`: the percent `lpe` is life divided by the MAXIMUM seen for this car in this session (`g_life_max`), not by the seed. So "95%" is relative to the largest value observed since the car was tracked, not a true health fraction. Tier of the percent: DERIVED (UI heuristic).
- Tier of the 2.2h figure: the seconds value is FACT for the AI countdown field on AI cars only (wg==1). The owner's observed "2.2h (95%)" must have come from an AI car with wg==1 (the owner's session is not in this agent's evidence). OPEN: confirm which car.
- Live check today (2026-10-12): `/telemetry` `c[0]` (id 0, "Matthias Steppe", wg 0) shows `lfe` 4922 s, `lpe` 100, `wg` 0, and `rpm` 0 (garage/replay). For wg 0 the value is frozen and must not be displayed as health (the viewer already hides it). OPEN: the player's life source is 0x7C91DC, which the `/telemetry` emitter does not yet read for the player (it reads veh+0x3D70 for all cars). Recommendation below.

## 3. Temperatures (oil and water)

- FACT (findings 145, screenshot 2026-07-28): player oil temp = double at `0x7C9174` and water temp = double at `0x7C91B4`, both Kelvin, both match the in-car steering-wheel gauge (66 C oil / 23 C water). Display = value - 273.16.
- CONFIRMED (finding 146, 2026-07-28): `veh+0x3D10` ("OilTemp") and `veh+0x3D14` ("WaterTemp") do NOT show the displayed oil and water. In the same frame: veh+0x3D10 = 134.76, veh+0x3D14 = 66.09 vs HUD 65.95 / 22.67. Treat veh+0x3D10/0x3D14 as UNVERIFIED for display.
- Live 2026-10-12 (single sample): `/telemetry` top-level `otp` 21.3 and `wtp` 21.0 (HUD, player; replay/garage likely). Car `c[0].ot` = 161.1 and `c[0].wt` = 69.8 (veh+0x3D10/0x3D14 path). The mismatch repeats: per-car values are not the gauge. CONFIRMED (two dates).
- Per-car oil/water for AI cars: only the veh+0x3D10/0x3D14 path exists (no globals found). OPEN (O-4): find an AI-car equivalent or treat AI oil/water as unavailable. The per-car `ot`/`wt` should be labelled "unverified".
- Brake and tyre temps (player only): FACT, base `0x7C7A00`, stride 1124, wheel order FL, FR, RL, RR (findings 142, 145). AI cars: no equivalent found.

## 4. Mechanical failure values (MF + native)

### 4.1 What MF tracks and where
- Player (`veh+0x32C8 == 0`): wear model is the static `EngineStressSimulation_0043ef20` (findings 240-249, 254). Values: life (0x7C91DC), stress/power (0x7C91D4), banks (0x7C920C/14/1C), running (0x7C9081), gear/brake/suspension countdowns are NOT used for the player.
- Player brakes: wear per step from `discTempK^3 * BrakeW...`, failure when thickness <= threshold at +0x444 (finding 256, CONFIRMED). Fade emits eid 74 (finding 257).
- AI (`veh+0x32C8 == 1`): countdown slots `veh+0x3D70..0x3D7C` (engine, brakes, gearbox 0x3D78, suspension), finding 264 CONFIRMED mapping. Gearbox stuck-gear byte `veh+0x39FA` is AI-only (finding 216, FACT).
- Crash damage (not wear): `veh+0x3D6C` carConditions, `veh+0x4068` ReasonCode, damage cluster 0x12B4/0x15A0/0x188C (findings 171-175; DERIVED and "not monotonic with severity"). damage.ini `[PHYSICAL] Engine=10000` is a separate collision seize threshold (finding 243, CONFIRMED).

### 4.2 How MF exposes values (host and families)
- FACT (mechanical_failures.c:76-88): host shared memory name `F1C_MECHFAIL_CTL`, 32-byte struct `MfHostCtl`:
  - `magic` (must be `0x4D48464D` = 'MFHM'), `abi_version`, `struct_size` (32), `host_verified` (1 = Lightloader proof passed), `dev_injector` (1 = dev ini exception), `families_started`, `game_pid`, `activated`.
  - Created with `CreateFileMappingA(INVALID_HANDLE_VALUE, PAGE_READWRITE, ..., "F1C_MECHFAIL_CTL")` in `mf_publish_host` (mechanical_failures.c:219-230).
- FACT: family control blocks: `F1C_ENGFAIL_CTL` (engine_failure.c:11799), `F1C_SUSFAIL_CTL` (engine_failure.c:4977); unload events `F1C_*_UNLOAD` (mechanical_failures.c:70-74). Gear and electronics families use the same pattern (not counted here).
- FACT: MF logs to `plugins\lightloader\mechanical_failures.log` (mechanical_failures.c:144). That is a file, not HTTP.
- FACT: MF has no HTTP endpoint and no `/health` contribution in the code read. The web_telemetry `/health` reports `broker_abi 15` and its own memory, nothing MF-specific.
- FACT: MF's per-family state (engine staged death, seizure, smoke, bank, repair) lives in its own DLL, read over shared memory by the GUI (`tools/mech_failure_test_gui.py`, README line 442).
- FACT: web_telemetry.c contains no reference to `F1C_MECHFAIL_CTL` or any MF DLL symbol. The only MF-related strings are in preserved audit copies and `UI_CORPUS_EVIDENCE_2026-10-08.json`. So web_telemetry does NOT currently read MF.

### 4.3 Detection: is MF loaded?
- DERIVED (design): open the named mapping `F1C_MECHFAIL_CTL` read-only (`OpenFileMappingA(FILE_MAP_READ, ...)`). If it exists and the first dword is `0x4D48464D`, MF host is present. Then read `host_verified`, `activated`, `families_started` (all read-only, no writes).
- NOT PERFORMED here: this agent did not open the mapping (it would touch the game process). Owner live test needed (see section 6).
- Alternative detection: `plugins\lightloader\mechanical_failures.log` exists and its last line is recent. Weaker (stale logs survive).
- Caveat: `dev_injector` = 1 means a dev exception path; release detection should require `host_verified == 1`.

### 4.4 Per-car MF values: what is shown per car and tier
| Value | Per car? | Source | Tier |
|---|---|---|---|
| engine life seconds | AI only (wg 1) | veh+0x3D70 | FACT (AI), player frozen |
| engine life seconds (player) | player only | 0x7C91DC | FACT (live twice) |
| engine stress (power loss) | player only | 0x7C91D4 | FACT |
| smoke banks (3) | player only | 0x7C920C/14/1C | FACT (cosmetic, finding 260) |
| ignition / engine running | player (and AI via 0x7C9081? OPEN) | 0x7C9081 | FACT player |
| gearbox stuck | AI only | veh+0x39FA | FACT |
| brake disc temp / tread | player only | 0x7C7A00 block | FACT |
| damage ReasonCode, carConditions | all | veh+0x4068 / 0x3D6C | FACT/DERIVED |
| MF failure rate setting | global | dword_7CEA5C (.plr key) | FACT (finding 239) |

Risk: the "failure probability" the owner asked about is NOT a stored number. The hazard is a random roll (Weibull, MF README line 19). No per-car risk percentage exists to read. Show life, stress, and temperatures, and label anything else "not stored".

## 5. Recommended page design (Engine live statistics)

### 5.1 Channel list
| Channel | Source | Tier | Unit | Rate | Scope |
|---|---|---|---|---|---|
| rpm | veh+0x39A0 x 60/2pi (live now) | DERIVED (vs 0x7C9084 conflict) | rpm | 2-4 Hz | all cars |
| rev limit | veh+0x39A4 x 60/2pi | DERIVED | rpm | on change | all cars |
| torque fraction | this+8 (0x7C7430) | FACT location, DERIVED scale | 0..1 | 4 Hz | player only |
| torque Nm / hp | none yet | OPEN | Nm / hp | - | hide until O-2 closed |
| engine life (player) | 0x7C91DC | FACT | s, % of max seen | 1 Hz | player only |
| engine life (AI) | veh+0x3D70 when wg==1 | FACT | s | 1 Hz | AI cars |
| stress | 0x7C91D4 | FACT | 0..0.6 | 2 Hz | player only |
| smoke banks | 0x7C920C/14/1C | FACT (cosmetic) | 0..1 | 1 Hz | player only |
| oil temp | 0x7C9174 (-273.16) | FACT | C | 1 Hz | player only |
| water temp | 0x7C91B4 (-273.16) | FACT | C | 1 Hz | player only |
| oil/water per car | veh+0x3D10/0x3D14 | CONFIRMED mismatch | C | 1 Hz | label "unverified" |
| brake disc temp (4) | base+1124i+0xB8 | FACT | C | 2 Hz | player only |
| tyre temps (4x3) | base+1124i+0/8/0x10 | FACT | C | 1 Hz | player only |
| tyre tread | base+1124i+0x48 | FACT | 0..1 | 1 Hz | player only |
| engine running | 0x7C9081 | FACT | 0/1 | on change | player only |
| MF host present | F1C_MECHFAIL_CTL magic/host_verified | DERIVED design | bool | 1/5 s | global |

Rate guidance: the game's sim advances; `/telemetry` `sample_age_ms` must be checked (it was 47 ms today). Do not chart unchanged samples as gameplay (AGENTS rule for live reads).

### 5.2 Per-car versus player-only
- Per car (table, all 24 cars): rpm, life (AI only; player uses 0x7C91DC), damage ReasonCode, carConditions, and a "wg" marker. Rows for player-only channels show "n/a - player only" (not zero).
- Player-only panel: torque fraction, stress, banks, temps from the HUD globals, brake and tyre temperatures.

### 5.3 Charts
- rpm vs time (player and focus car), with rev-limit line.
- Torque fraction vs rpm (scatter, player) once the curve is proven; until then show fraction vs rpm only.
- Engine life trend (player 0x7C91DC; AI veh+0x3D70): show drain rate (s/s) as a derived series, not the raw value.
- Oil and water temperature vs time (player HUD globals), with the `LifetimeOilTemp` threshold line 123.43 C marked as a reference only (source to be re-verified).

### 5.4 MF section
- Show only when `F1C_MECHFAIL_CTL` is present with magic `MFHM` and `host_verified == 1`.
- Contents: families started, activated, dev_injector warning, per-car MF fields from 4.4. No risk percentage.
- Do not write to shared memory from the page (read-only).

## 6. Evidence, open questions, and owner live tests

Confirmed facts used in this record: see findings appended to `f1c_ida_decompiled/findings.jsonl` (keys `engine-live-*`, dated 2026-10-12).

Open questions:
- O-1 rpm address: is the player rpm `this+7260` (0x7C9084) or `veh+0x39A0`? Finding line 246 has an arithmetic error (0x7C90C4 is this+7324). Resolve with a live read-only sample while the car is running and moving (check sim time advances).
- O-2 torque curve: locate the `.eng`/`.hdv` torque-vs-rpm table and its token; calibrate `this+8` against a known peak (Nm). Until then no Nm or hp is shown.
- O-3 boost, engine brake, mapping: negative in the engine parser; full token search pending.
- O-4 AI oil and water: no global found; per-car veh+0x3D10/0x3D14 unverified.
- O-5 engine life seed: observed values exceed `LifetimeAvg + LifetimeVar` ceiling (finding 268).
- O-6 which car produced "ENGINE LIFE 2.2h (95%)": must be wg==1 (AI). Owner to confirm the car and slot.
- O-7 `lifetimeOilTemp` and `LifetimeEngineRPM` curve values: unmeasured (game was paused).
- O-8 MF detection: confirm `F1C_MECHFAIL_CTL` open/read on the owner's install (read-only), and `host_verified` value.
- O-9 f1c_health today: `replay_mode` = 1 and `pause_flag` = 0 while `sim_time` advances (1509.8). Replay may explain rpm 0 on car 0. Check before any live engine conclusion.

Risks:
- Rounding and wrong offsets: several decompiled offsets are misread (findings 191, 245, 258). Always re-derive with the correct base.
- Stale or cosmetic values: smoke banks are cosmetic; the viewer must not label them as damage.
- Player wear countdowns (veh+0x3D70..7C) never tick for the player; showing them would be a false zero.
- Live reads during replay or pause give frozen values.

Owner live tests needed (one session, single race, player car running):
1. Drive a lap; log `this+7260` (candidate rpm) and `veh+0x39A0` (rpm) at 10 Hz for 10 s; confirm which tracks the tacho.
2. Full-throttle pull to the limiter; record `this+7388` and where torque fraction drops (checks the rev-limit ramp).
3. Note the car and slot showing "ENGINE LIFE 2.2h (95%)"; confirm wg == 1.
4. Record `0x7C9174`/`0x7C91B4` against the in-car gauge at two points (already done once; repeat on a second car if possible).
5. With Lightloader running and MF loaded: open `F1C_MECHFAIL_CTL` read-only, report magic, host_verified, activated.

## 7. Next actions (not done here)
- Implement the page after owner approval; no source edits made in this batch (per instruction).
- Close O-1 and O-2 before showing hp or Nm.
- Add the MF detection as read-only probe in the web_telemetry reader (implementation task, separate spec).

Restoration: no source file changed. Files written by this batch: this document, and appended lines in `f1c_ida_decompiled/findings.jsonl`.
