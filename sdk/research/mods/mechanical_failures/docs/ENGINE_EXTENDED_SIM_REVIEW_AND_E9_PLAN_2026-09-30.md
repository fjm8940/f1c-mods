# Extended Engine Simulation - review, realism audit and E9 plan (2026-09-30)

Author: Claude (lead-engineer review). Status: PLAN - owner review, then DeepSeek
batch by batch. Master plan and batch log stay in
`ENGINE_SIM_MODEL_DEEPSEEK_HANDOFF_2026-09-30.md` (section 8); DeepSeek logs E9
batches there, same format.

Evidence tiers: FACT (live or proven), DERIVED (static code read, not live),
INFERENCE (engineering judgement), OPEN (unknown).

## 0. Owner direction (2026-09-30)

1. Keep the failure simulation generic: valid for any generation of car
   (1950-2026). No era-specific technology in logic or text for now.
2. Over-rev: understand how the game does it; our model must not fight it.
3. Keep it fun. Parts may pop now and then; the player must never be forced
   into real F1 engine-management routines (warm-up laps, mileage books,
   mandatory short-shifting all race).
4. Show symptoms with the game's own mechanics (stutter, cut, shake, pitch,
   gauges, lights) wherever possible.
5. Car-specific and plug-and-play: every number comes from the loaded car;
   no edits to cars or other mods.

## 1. E8g + E8h review (Claude, 2026-09-30) - ACCEPTED

- Diffs read against `ENGINE_SIM_BEFORE_2026-09-30_E8g` / `_E8h`. Logic matches
  the specs. E8h `engine_seize()` never touches ENG_RUNNING (FACT basis:
  finding `engine-seized-state-life-zero-ignition-on-2026-09-30`).
- Claude re-ran: model test ALL PASS (54989 checks); `dll\build_merged.bat` PASS;
  GUI unittest OK; panel smoke PASS; diag smoke PASS.
- Installed (game closed): `mechanical_failures.dll` SHA-256
  `93efca579d3f172d3d73d5f44edd0f1a28e3d87c447d0fde21f9ab35860735c4`, dev = installed.
  Previous installed DLL `0fad7fc3...` preserved in
  `workspace_organization_audit/INSTALLED_BEFORE_2026-09-30_E8h/` with MANIFEST.
- Found: build is NOT reproducible (two builds of the same source gave
  `29d77d41...` and `93efca57...`; DeepSeek's own build `61B8A88E...`). Cause
  INFERENCE: PE timestamp. Fix in E9g.
- Realism concerns carried into this plan: bearing temperature ~505 K
  (E9f); DLL radio lines still absolute (E9g); legacy levers still clear
  ignition (E9g).

## 2. Over-rev: how the game does it (DERIVED, static)

Finding `overrev-no-dedicated-code-2026-09-30`.

- The game has no over-rev damage code. Over-rev acts through two mechanisms only:
  1. Life drain `0.5 x (2^((rpm - base)/range) + 2^((oil - obase)/orange)) + stress`
     (FACT, live within 2 %). F2002: base 17223.6 rpm, range 388.7 rpm. At 1.15 x
     18100 = 20815 rpm the drain is ~300 s/s, so a 0.3 s spike costs ~90 s of life.
     It kills the engine only if the remaining life is that small.
  2. Limiter throttle fade (Gearbox 0x443B70): above REV_LO the throttle is capped
     to `(REV_HI - rpm) / (REV_HI - REV_LO)`. The torque curve itself goes
     negative past ~20300 rpm (engine.ini), so rpm above that comes only from the
     wheels (downshift).
- Our model today (`mf_engine_model.h` 4.7): BLOW UP at rev_abs > 1.15 for
  0.3 s, valvetrain damage above 1.02, Miner rod fatigue above 1.04.
  **Conflict**: that blow-up is a mod-authored death the author's clock would
  not produce. This breaks "never less reliable than the author set".
- Our drain EMA skips samples > 20 s/s (E8e), so the model does not see the
  life an over-rev spike took (it sees only the lower ttf afterwards).
- Stage one of a game failure (finding `native-life-zero-rearm-same-call-2026-09-30`):
  when life crosses 0 the same call may re-arm it (1-500 s, smoke bank, stress
  0.15-0.25). The model sees a life JUMP UP, not 0. `culminated` fires only on
  the final, non-re-armed zero. Today a re-arm with no onset can trigger a fresh
  onset on the short re-armed life, and our `bank1_set` can overwrite the game's
  own stage-one smoke. Live behaviour: OPEN (Claude live test L1).

**Design decision (E9a)**: the game owns over-rev death. The model turns
over-rev into **consequences and presentation**: it measures the life each
spike cost, damages valvetrain/rods in proportion (symptoms only), and if the
game's clock later runs out soon after an over-rev, the failure is presented as
"valve hit piston" with the big bang.

## 3. Realism audit of existing sub-models (INFERENCE unless marked)

| Model | Physics basis | Verdict | Action |
|---|---|---|---|
| Timing (onset from game clock) | author's own life law | right authority | keep; handle stage-one re-arm (E9a) |
| Valve return (pneumatic bottle) | ideal gas, choked orifice, float ceiling ~ sqrt(p) | sound physics, but pneumatic = ~1990+ only | generalise to "valve return force" (E9c) |
| Oil pressure / leak / slosh | p ~ rpm x sqrt(viscosity); slosh by oil level | generic across eras (wet and dry sump both starve when low) | keep; drive the game's own oil-pressure gauge (E9d) |
| Bearings | Sommerfeld film + Archard wear + friction heat loop | method right; E8g constants distort temperature (BRG_RISE 300 K -> ~505 K, real plain bearings ~oil + 20-40 K) | E9f: separate displayed temperature from calibrated film factor |
| Cooling (leak, debris, gasket) | pressurised system, Antoine, Arrhenius creep; car-relative warnings | good after E8g. Debris 95 % blockage is more than real (one sidepod ~50 %) but needed because the game's cooling is forgiving | keep; note |
| Rings / blow-by | seal loss -> power, blue smoke, oil loss | good | keep |
| Exhaust crack | Paris law thermal cycles + vibration | good; exhaust gas only ~725 C at full load (real NA engines ~850-950 C), so exhaust-valve temperature never reaches its limit (dead branch) | E9b-lite: gas rise from load AND rpm, ~930 K |
| Fuel pressure / leak / fire | pump capacity vs demand, lean running, fire hazard | good | fuel flow reference from car's FuelConsumption (E9e) |
| Conrods | Miner fatigue, S-N m = 8, endurance 1.04 | good for fun (no damage at/below limiter) | remove 1.15 hard blow-up (E9a) |
| Always-on wear (engine braking, over-rev valvetrain) | small rates | harmless today but could reach terminal without an onset | floor: no terminal without onset (E9b) |

Overall verdict: physically literate lumped model; constants tuned for
gameplay, not calibrated to real engines. Right for the goal. Weak spots:
over-rev authority, stage-one re-arm, bearing temperature, era-specific valve
air, exhaust temperature too low.

## 4. Symptoms: which game mechanics we use

### 4.1 In use (FACT, installed)

Power cap (torque detour, `g_power_mult`); fuel-cut stutter (`vfuel_press`);
valve misfire (`vfuel_valve`); game rev limiter band; smoke banks 1/3 + custom blue
smoke; fireball/flames; backfire pops (generic `Audio\Vehicle\backfire_ext_N.wav`);
own wavs (blow, steam hiss, radio static); HUD strip; team radio eids; extra oil
heat; radiator coefficients (coolant loss / airflow); life = 0 seize with dash on;
retire radio.

### 4.2 Available now, unused

| Mechanic | Address / source | Tier | Symptom it enables |
|---|---|---|---|
| Cockpit OIL PRESSURE gauge | `0x7C919C` kPa, read by sub_49F530 <- SteeringWheelAnimation | DERIVED | needle drops/flickers with our oil fault (big realism win, zero invention) |
| Engine braking map | `0x7C9078` (docs/47) | FACT (earlier work) | worn bearings = more engine braking; seized-ish feel |
| Fuel burn | `FUEL_NOW 0x7C75C4` | FACT | blow-by / rich running burns more |
| Rev band shaping | REV_LIM/LO/HI | FACT | "limiter bounce" rough limiter for valve faults |
| Native stage-one smoke | game re-arm | DERIVED | let the game's own stage one be the smoke prelude |

### 4.3 Needs Claude research first (not DeepSeek)

| Symptom | Lead | Tier |
|---|---|---|
| Rough engine note / pitch wobble | ScaleAudioPitch 0x4F1510, EngineSoundWaveTableGenerator 0x513482 | OPEN |
| Car's own backfire samples (`VS_INSIDE/OUTSIDE_BACKFIRE_1-3` in the car .sfx) instead of generic wavs | find the native backfire trigger | OPEN |
| Dash warning icons | sub_49B9A0 (thermometer), sub_49BA50 (FUEL) | DERIVED |
| Cockpit shake | HeadPhysics (.veh `HeadPhysics=headphysics.ini`), camera code (function names unreliable) | OPEN |
| Force feedback rumble | DirectInput effect path | OPEN |
| Rough idle / stall + native restart | IdleRPMLogic in memory; starter sub_4471F0 (sets rpm to idle) | OPEN |
| Gauge 4 (psi 0-12, TireFrictionCurveEval) meaning | sub_49F530 case 4 | OPEN |
| Lateral g | still missing | OPEN |

## 5. Car data audit (F2002, `SeasonData\Vehicles\Ferrari\2002_F2002`)

Rule kept: the DLL never parses files; values come from memory after the game
loaded them. "Address" = Claude must find it in memory first (research R-tasks).

| File / key | Used today | Proposed use | Needs |
|---|---|---|---|
| engine.ini RevLimitRange/Setting, RevLimitLogic | yes (REV_*) | - | - |
| LifetimeEngineRPM, LifetimeOilTemp, LifetimeAvg/Var | yes (clock law) | - | - |
| OptimumOilTemp | yes | - | - |
| RadiatorCooling, WaterMinimumCooling, EngineSpeedHeat | yes (writes) | - | - |
| **RPMTorque** (torque + engine-braking columns) | no | shape power loss: valve faults cut only above peak torque rpm; bearings raise friction column; car-relative "peak rpm" reference | address R1 |
| **FuelConsumption** | no (measured flow) | exact fuel-flow reference for lean-out and leak | address R2 |
| **EngineInertia** | no | misfire rpm dip size, rhythmic misfire depth | address R2 |
| **IdleRPMLogic** | no | rough idle / stall in pit lane (always auto-restart: fun rule) | address R2 |
| **OilWaterHeatTransfer** (base, per speed) | no | head gasket = coolant/oil mixing -> raise transfer, water follows oil up (native symptom) | address R2 |
| EngineBrakingMapRange/Setting | no (0x7C9078 known) | bearing friction (E9d) | - |
| CombustionHeat, OilMinimumCooling | no | not needed (game already uses them) | - |
| OnboardStarter, StarterTiming | no | stall recovery must work even with OnboardStarter=0 (mod restarts, fun rule) | R2 |
| EngineEmission | game uses it | nothing | - |
| EngineMapRange | inert in game (finding enginemapsetting-is-unimplemented) | nothing | - |
| .hdv RadiatorRange/Setting | yes, through game airflow | - | - |
| .hdv FuelTankMotion | no | fuel pickup surge at low fuel under braking/cornering (generic, every era) | address R3 |
| .hdv UpshiftDelay/DownshiftDelay, ClutchWear | no | gearbox family (hydraulic-style slow shifts); not engine scope | later |
| .veh Engine=, Classes=(year) | no | era profile later; owner says generic now | - |
| .sfx VS_*_BACKFIRE_1-3, VS_*_ON_FIRE | no (generic wavs) | car's own backfire and fire sounds | R4 |
| .sfx playerEngineVolume*, EngineRPMSound bands | no | nothing directly; pitch wobble goes through the audio code (R5) | R5 |
| SeasonData damage.ini [PHYSICAL] Engine | game uses it | nothing | - |

## 6. New design rules (Claude decisions for E9, owner may override)

1. **Game life is the damage currency.** Anything that ends an engine early must
   come from the game's clock. The model may add symptoms and choose how a
   failure looks, never decide that it happens outside the clock (exception:
   the fault developing at its own pace after a natural onset, see rule 2).
2. **Fairness floor.** After an onset, a model terminal can come no earlier
   than `MF_FAIR_FRAC` (0.6) x the time-to-fail predicted at onset. Abuse
   speeds it up only down to that floor. Gentle driving still slows it.
3. **No terminal without an onset.** Always-on layers (engine braking, over-rev
   valvetrain, rod fatigue) can reduce health to `MF_ALWAYS_ON_FLOOR` (0.5) at
   most; they make symptoms, not deaths.
4. **Niggles.** Small non-terminal events that make engines feel alive
   (0-2 per race hour, rolled from the stress share): a 2-5 s misfire burst, a
   sensor warning that clears, a small smoke puff, a radio "keep an eye on it".
   Never touch life; never > 5 % power for > 10 s. Panel toggle, default on.
5. **Never strand the player for a non-fault.** A stall caused by a rough idle
   always auto-restarts within 2-4 s, whatever OnboardStarter says.
6. **Era-neutral text.** HUD/radio strings describe symptoms, not technology.
7. **Symptoms prefer the game's own mechanics** (gauge, limiter, engine braking,
   game smoke) over our own emitters and wavs.

## 7. Batches for DeepSeek

Common rules for every batch (hard rules section 7 of the master plan apply):
- New before-copy folder `workspace_organization_audit/ENGINE_SIM_BEFORE_<date>_<batch>/`
  with MANIFEST (SHA-256) before the first edit, including every test/.py file touched.
- Gates: `tools\run_engine_model_test.bat` ALL PASS, `dll\build_merged.bat` PASS
  (no install), `python -B -m unittest discover -s tools -p "test_mech_failure_test_gui.py"` OK,
  `python -B tools\mech_test_panel.py --smoke-test` PASS.
- Log entry `### <batch> <date>` in master plan section 8: files + hashes, test
  counts, deviations, open questions. Then STOP for review.
- No new game address unless this plan lists it with tier FACT or DERIVED.
- Final chat summary: plain text, no markdown, short lines.

Order: E9a -> E9g -> E9b -> E9f -> E9c -> E9d -> (Claude R-tasks) -> E9e.

### E9a - over-rev ownership and native stage one

Files: `dll/mf_engine_model.h`, `dll/engine_failure.c`, `tools/engine_model_test.c`.

1. Remove the model-authored over-rev terminal: `MF_OVR_HARD` / `MF_OVR_HARD_S`
   no longer set `blown`. Keep the timer as an "over-rev event" detector.
2. Over-rev accounting. New MfInputs field `life_jump_down_s`: life lost in a
   sample the EMA skipped because it exceeded MF_DRAIN_MAX_SPS while
   `rev_abs > MF_OVR_VT_REV` (the caller does not compute it; the model does,
   from life_prev_s). Accumulate `e->overrev_life_s`; count events
   (`e->overrev_events`) when a jump > 1 s of life happens.
   Consequence: `h_vt -= MF_OVR_VT_PER_LIFE x jump_s / max(life_before, 60)`,
   `h_rod_dmg` same scale x 0.5 (new constants, commented, relative).
   Both capped by rule 3 (`MF_ALWAYS_ON_FLOOR` 0.5) unless an onset is active.
3. Presentation: if `culminated` fires within `MF_OVR_BLAME_S` (5 s) of an
   over-rev event, set `e->culm_overrev = 1`; the DLL's `model_clockout_pump`
   then plays the full BLOW UP presentation (`blow_burst_start`, reason
   "over-rev: valve hit piston") instead of the quiet ENGINE FAILURE. No life
   write (it is already 0).
4. Stage-one re-arm. Model detects `life_prev_s < MF_REARM_PREV_S` (5 s) and
   `life_s > life_prev_s + 1` in one sample while on track: `e->native_stage1 = 1`,
   log once. Then:
   - no onset yet: fire the onset NOW (skip warm/confirm), budget = new life,
     fault chosen from the normal roll (native smoke is the prelude);
   - onset active: re-base `life_at_onset` so terminal lines up with the
     re-armed life;
   - DLL: while `native_stage1`, `bank1_set` must not reduce the game's own
     bank density (write only if our value is denser, i.e. more negative).
5. Tests: (a) spike trace 0.3 s at 1.16 with large life: no terminal,
   events = 1, h_vt dented, >= 0.5; (b) same spike with life 50 s: game life
   reaches 0 -> culminated + culm_overrev = 1; (c) life trace 3 s -> 150 s jump:
   native_stage1 = 1, onset fired, terminal within 150 s +/- 10 %;
   (d) existing tests unchanged except those asserting the 1.15 blow-up
   (rewrite to the new rule, list them in the log).

### E9g - cleanups found in review (small)

Files: `dll/engine_failure.c`, `dll/build_merged.bat`.
1. Reproducible build: add `-Wl,--no-insert-timestamp` to the link flags; prove
   two consecutive builds give the same SHA-256 (log both).
2. DLL radio thresholds still absolute (`R_STEAM` water > 110 C,
   `R_WATERPIT`/`I_WATERPIT` > 118 C): switch to the model's water baseline
   (`g_model_eng.water_base_k` + MF_WATER_WARN_DK / MF_WATER_COOL_DK).
3. Legacy levers that still clear ENG_RUNNING for mechanical causes: list every
   remaining `ENG_RUNNING = 0` write; convert mechanical ones to
   `engine_seize()`; leave electrical ones. Log the list.

### E9b - fun and fairness

Files: `dll/mf_engine_model.h`, `dll/engine_failure.c`, `tools/engine_model_test.c`,
panel field only if Claude adds it first (DeepSeek does not touch .py).
1. Fairness floor (rule 2): store `e->ttf_at_onset`; a model terminal before
   `onset_time + MF_FAIR_FRAC x ttf_at_onset` is held (health clamped at 0.02)
   until the floor passes.
2. Always-on floor (rule 3): without onset, h_vt, h_b, h_rod never below
   `MF_ALWAYS_ON_FLOOR`.
3. Niggles (rule 4): `MF_NIGGLE_PER_H` 1.0 mean rate, rolled per second while on
   track and no onset; kinds: misfire burst (vfuel_valve pulse 2-5 s), sensor
   warning (HUD only, clears in 5-15 s, radio "sensor, ignore it" if an eid
   exists - Claude picks eid), smoke puff (bank1 -0.1 for 1 s). New MfOut fields
   `niggle_kind`, `niggle_until_s`. Control-block toggle `niggle_enable`
   (Claude adds the panel checkbox in the same review).
4. Exhaust gas: target `ambient + MF_EXH_RISE_K x load^0.8 x (0.6 + 0.4 x rev)`,
   `MF_EXH_RISE_K` 700 -> 930 K. Re-check exhaust-crack and valve tests;
   exhaust-valve limit now reachable only lean + full load (assert).
5. New test profile CASUAL: race pace, ignores every warning, limiter bounce
   30 % of the time, 20 downshift spikes to 1.08 per 90 s lap. Assert: no
   terminal without onset over 5400 s with infinite life; with an onset, no
   terminal before the fairness floor.

### E9f - bearing temperature realism

Files: `dll/mf_engine_model.h`, `tools/engine_model_test.c`.
- `MF_BRG_RISE_K` back to a physical 35 K for `T_brg` (displayed, logged).
- The film calibration moves to a separate dimensionless factor
  `MF_FILM_TEMP_GAIN` applied inside the viscosity term only, chosen so every
  E8g lubrication target still passes (281/507 s windows, GENTLE survives).
- Assert `T_brg` <= oil + 60 K at race pace, unworn.

### E9c - era-neutral valve return

Files: `dll/mf_engine_model.h`, `dll/engine_failure.c`, `tools/engine_model_test.c`.
- Rename the concept: "valve return force" `R` (1 = healthy) replaces bottle
  gas `m`. Same math (leak -> fatigue/leak rate, ceiling `sqrt(R / R_need)`),
  so all targets hold. Pneumatic top-up in the pits kept behind
  `#define MF_VALVE_PIT_TOPUP 0` (off = generic: nothing to refill).
- Text: `VALVE AIR LOW` -> `LOSING REVS`, `PIT FOR AIR` -> removed when
  top-up is off, `VALVE FLOAT` kept, `SHIFT EARLIER` kept.
- Audit every HUD string in engine_failure.c for technology words; list
  before/after in the log.
- Control-block/panel names unchanged (ABI 13); Claude renames labels.

### E9d - game's own gauges and friction

Files: `dll/engine_failure.c` (+ model outputs).
1. New MfOut `oil_gauge_scale` (1 healthy): during oil-pressure, oil-leak and
   bearing faults = model `p_oil / p_oil_healthy` (flicker included). DLL writes
   `0x7C919C *= oil_gauge_scale` once per tick after the game's physics update
   (the game relaxes it at 4/s, so no restore needed). Address tier DERIVED:
   Claude live-checks L3 before this batch starts.
2. Engine friction (E9 original item): `0x7C9078` scaled by
   `1 + MF_FRICTION_K x (1 - h_b)`; restore on repair and session change.
3. Rhythmic misfire (E9 original item): power pulse synced to crank angle
   integrated from rpm, one cylinder in N (`N` = 10 until R2 gives a car value);
   used by valvetrain, bearings, rings faults above their symptom thresholds.
4. Extra fuel burn (E9 original item): blow-by and rich/lean via FUEL_NOW.

### E9e - car-specific inputs (after Claude research R1-R3)

Only after Claude adds the addresses to the master plan section 2 as FACT.
- MfCar gains `peak_torque_rpm`, `fuel_cons`, `inertia`, `idle_lo/hi`,
  `oil_water_k0/k1`, fallback-relative like the others.
- Valve faults cut power only above `peak_torque_rpm` (top end goes first),
  bearings cut everywhere, rings proportional to load.
- Head gasket raises oil->water transfer (`oil_water_k0 x (1 + 3 x (1 - h_g))`):
  the game's own water gauge climbs with the oil.
- Fuel flow reference = `fuel_cons x rpm x throttle` model instead of session peak.
- Rough idle in pit lane with bearing/valve faults; stall -> auto-restart 2-4 s
  (rule 5).

## 8. Claude-only work (before or alongside)

- GUI (pending from handoff step 3): "no fault" state for oil pressure/valve bars;
  valve ceiling vs limiter rpm; remove inert over-rev checkbox; later niggle toggle.
- Live L1: poke life 1.5 with the model active -> record re-arm, smoke bank,
  model log (feeds E9a test values).
- Live L2: downshift over-rev -> measure life lost per spike vs the law.
- Live L3: confirm 0x7C919C moves the F2002 cockpit gauge (if the cockpit shows one).
- R1 torque table address; R2 FuelConsumption / EngineInertia / IdleRPMLogic /
  OilWaterHeatTransfer / OnboardStarter addresses; R3 FuelTankMotion state;
  R4 native backfire trigger (car's own .sfx samples); R5 engine pitch wobble
  (ScaleAudioPitch 0x4F1510); R6 dash icons detour; R7 lateral g; R8 cockpit shake.

## 9. Owner questions

1. Fairness floor 0.6 of predicted time and niggles ~1 per race hour: OK?
2. Generic valve fault: drop the pit air top-up (default) or keep it as flavour?
3. Over-rev: accept "game decides, model presents"? (recommended)

## 10. Evidence and restoration

- Findings added: `overrev-no-dedicated-code-2026-09-30`,
  `native-oil-pressure-gauge-7c919c-2026-09-30`,
  `native-life-zero-rearm-same-call-2026-09-30` (all DERIVED, static).
- Nothing in this plan is implemented yet. Pending validation: live L1-L3, owner
  retest list (handoff step 4).
- Restore the previous install: copy
  `workspace_organization_audit/INSTALLED_BEFORE_2026-09-30_E8h/mechanical_failures.dll`
  to the game's `plugins\lightloader\` folder.

## 11. Further ideas (owner asked "anything else missed", not yet specified)

Ranked by fun/effort. All generic across eras. INFERENCE unless marked.

1. **Contact-triggered faults.** Impacts below the game's seize impulse
   (damage.ini `[PHYSICAL] Engine`) roll a fault at the moment of contact:
   radiator puncture -> water leak, exhaust knock -> crack, sump hit -> oil leak.
   Uses the game's own impact, feels earned, never random. Needs impact
   impulse address (R9).
2. **Driver "save the engine" mode.** A hotkey/panel toggle: limiter -3 %,
   fault growth x0.6, engineer radio "turning it down". Gives the player a
   choice, not a chore. Uses REV_* only.
3. **Post-session teardown report.** Write a short text report per session:
   part health %, peak temps, over-rev events, niggles, cause of failure.
   Cheap, fun, and a calibration tool for us.
4. **Heat soak.** Standing on the grid / in the pit box with no airflow: the
   game's water/oil rise (check whether the game already does it - OPEN);
   if yes, only add the warning; if not, scale airflow terms down at speed 0.
5. **Dirty air.** Running close behind another car lowers radiator airflow
   (deferred 4.0c; check if the game's airflow term already includes it).
6. **Kerb and landing shocks.** Big vertical g spikes add small bearing/rod
   fatigue and can crack exhausts. Needs vertical g (R7 family).
7. **Low-fuel pickup surge.** Last litres + braking/cornering = short cuts
   (FuelTankMotion, R3). Classic, era-neutral.
8. **Detonation.** Lean + hot head -> piston crown damage (links fuel-pressure
   and cooling faults; piston temperature already modelled).
9. **Crank torsional resonance.** Dwelling in one car-relative rpm band adds
   rod/bearing fatigue; a niggle-sized "vibration" symptom.
10. **Trace-driven calibration.** Record real laps (rpm, load, oil, water,
    speed) from the DLL log and replay them in `engine_model_test.c` instead of
    synthetic profiles. Engineering tool, big confidence gain.
11. **AI parity (visual only).** AI engine failures (AI model, ReasonCodes 1/5/2/4)
    get the same smoke/fire presentation. No change to AI reliability.

## Owner design question (2026-09-30 late): how should failures occur?
Facts (code read, dll 232ce0b9): WHEN = game life clock (.hdv LifetimeAvg/Var, native drain law: F2002 drain doubles
every 389 rpm above 17 224 rpm and every 2.88 C oil above 122.6 C). Onset when time-to-failure < a random 4-15 min lead.
ONE fault per engine, rolled with weights from how the life was used (revs / oil heat / water heat / cycles / g-load
shares), not uniform random. Parts health (bearings, valvetrain, rings, rods, gasket) couples inside that one fault.
Abuse paths that exist today: faster life drain (exponential), always-on valvetrain over-rev damage above 1.02 x limit,
E9a hard death above the torque-zero rpm for 0.3 s.
Inference: flat-out at the F2002 limiter drains ~4.8x -> a 1 h engine lasts ~12-13 min of limiter running, so full-race
abuse does blow it. Not live-measured for a full race.
Candidate additions (not specified yet, owner to choose): bathtub hazard (small early-life random failure chance,
abuse-scaled), a second minor fault / cascade (leak -> bearings), engine quality spread per team via LifetimeVar.

## Claude assessment (2026-09-30 late, owner question: rating, limits, simplicity, unused game parts)
Opinion, not evidence. Rating ~7/10 today: strong structure (game clock = WHEN, usage-weighted fault = HOW, part
health, native levers, reproducible tests); weak on validation (constants from synthetic profiles, few live runs)
and on feel (misfire not felt, no engine-note change, no shake/rumble). Expected ~8/10 after the final run + one
tuning pass from the owner's big test.
Game limits: engine = torque table x throttle x one multiplier; one life clock + stress; no per-cylinder, oil
pressure or bearing physics (all modelled outside, shown through a few levers); engine sound driven by rpm/throttle
only; AI lighter physics (rpm clamped, no thermal); fixed exe addresses; 3 smoke emitters.
Simplicity rule proposed: stop adding hidden physics. A state earns its place only if the player can see, hear or
feel it, or it decides which fault happens. Next effort goes to symptoms and tuning, not new equations.
Unused game parts (leads, all OPEN/DERIVED, section 4.3): engine note pitch wobble (ScaleAudioPitch 0x4F1510), the
car's own backfire samples, dash warning icons (sub_49B9A0 / sub_49BA50), cockpit shake (HeadPhysics), force-feedback
rumble (SuspensionVibration is called from Gearbox and the starter path - meaning unproven), native stall/restart.
New idea: engine mileage carried across a race weekend (practice -> qualifying -> race), saved per car to a file.
