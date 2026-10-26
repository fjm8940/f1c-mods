# NOTIFICATION INVENTORY - mechanical_failures engine family (batch N1)

Date: 2026-10-06. Owner: Matias. Spec + review: Claude.
Prompt: `docs/DEEPSEEK_NOTIFICATION_AUDIT_PROMPT_2026-10-06.txt` (batches N1-N6).
This batch: N1 inventory only. No code change, no build, no install. STOP for
Claude's review ("check DS").

Source pinned (SHA-256):

| file | SHA-256 |
|---|---|
| `dll/engine_failure.c` | `FC3D6B15E5E77B3BF304404FA0EABD7DC4AE9F623C4D22EEF86EDE9B68F818E3` |
| `dll/mf_engine_model.h` | `FBBEF902D7596F24215AA376162191AB64819173B0497DC20CD60CF0E01AB7F7` |
| `dll/mf_msg.h` | `9AEFAA27D644A30B6F488434A76AEDBE12A67C0BAEF6CABEC6039565D878F5E2` |
| `dll/mf_voice.h` | `FD0BC6E67377B93A7D72D92B9D168CD3BF8FD71979A1504D8E6FC053A607969A` |
| `dll/mf_lamps.h` | `1136AC38B861B407F4FC2533F921F08D19A208B548BF458335980D52AE9981AA` |
| `dll/mf_hud.h` | `6C0C33766CFC67DD17AC4778034466123B69F70EFD631118E5720CCCCB75FCAD` |

Compiled-out macros verified:

- `MF_LEGACY_VALVE/OILP/COOLING/FUEL/BLOWBY/EXHAUST/OVERREV = 0` (engine_failure.c:136-142).
- `MF_VALVE_PIT_TOPUP = 0` (engine_failure.c:3754).
- Every `#if MF_LEGACY_*` / `#if MF_VALVE_PIT_TOPUP` legacy branch below is
  therefore NOT in the built DLL and is marked **compiled out**.
- `g_voice_on` = 1 only while the installed SDT carries the 35 appended clips
  and the transaction validated (`voice_install`, engine_failure.c:1550); with
  vanilla SDT every `voice_*` falls back to the legacy eid paths where a
  fallback exists (that is the `!g_voice_on` wording from the prompt).

Column key:

- kind: HUD warn / HUD instr / HUD crit / HUD end / radio / MFV voice / lamp / sound / FX bank.
- verdict: PASS / FAIL (rule) / compiled out / dead code / N/A.
- trigger: copied verbatim from the source line.
- vars class: OBS = observable per R1/R2, INT = internals (forbidden by R1 unless
  section B names the output).

Common gates (repeated per row as codes):

| code | meaning |
|---|---|
| G-S | pump returns on `SESSION_OVER()` |
| G-P | pump returns while `g_pump_frozen` (pause/menus; no writes, no logs) |
| G-M | active only while `g_model_live && g_model_eng.onset && !culminated && !blown && !stopped && fault == <X>` |
| G-L | `hud_show` posts only when `live_drive()` (RACE_PHASE > 4, player on track, session not over) |
| G-W | `instruct`/`critical`/`voice_pre`/`voice_crit`/`voice_instr`/`say_*` return when `wrecked_or_stopped()` |
| G-Q | radio queue: >= 15 s between mod calls (5 s death), 30 s stale drop, no non-death call while wrecked/stopped, non-death refused once the model is terminal |
| G-R8 | `PLR_FAILRATE == 0` / `life_infinite`: model layer off (no onset, no niggle, no no-fault water, no over-rev notice) |
| G-RL | held() rolling detection window `[min_ms, max_ms]` after the condition first holds; reminder cadence 60 s (warn) / 45 s (instr) / 90 s (crit) |

---

## 1. Grep coverage counts (prompt section C N1)

Raw line hits in `engine_failure.c` (rg, lines containing the term): warn_after
24, instruct 46, critical 35, hud_show 31, hud_blink 7, voice_pre 9,
voice_instr 14, voice_crit 9, voice_say 18, voice_arm_line 4, voice_arm_instr 4,
radio_req 17, say_pre 6, say_after 11, play_fx 7, bank1_set 14, lamp_pump 4.
Raw counts include definitions, prototypes and comments; call sites below are
the de-duplicated truth.

Call sites per function (active / compiled out):

| function | active call sites | compiled out |
|---|---|---|
| oil_pump | warn 2642, 2644; instr 2646; voice_instr 2649; crit 2656; voice_crit 2657; say_pre 2626; voice_arm_line 2629 | warn 2534, 2535; instr 2537; say_after 2528, 2545; hud 2555 |
| valve_pump | warn 4115; voice_pre 4117; instr 4119; live 4121; crit 4123; voice_crit 4125 | warn 4005; instr 4006; hud 3990, 4039, 4040, 4077 (topup macro 0) |
| oilp_pump | warn 4238; voice_pre 4241; instr 4243, 4245; voice_instr 4247; crit 4249; voice_crit 4251 | warn 4196; instr 4197 |
| exhaust_pump | warn 4463; instr 4464; voice_pre 4466 | warn 4376; instr 4377; say_after 4374 |
| blowby_pump | warn 4600; instr 4606; voice_pre 4608; crit 4611; voice_crit 4613 | warn 4562; instr 4563; say_after 4564 |
| overrev_pump | hud 4712; voice_say 4715, 4722 | hud 4666 |
| water_heat_pump | warn 4862; instr 4863; voice_say 4870, 4876 | - |
| cooling_pump | warn 5146, 5148, 5169; instr 5149; voice_pre 5153; voice_instr 5155, 5204; say_pre 5159, 5201; crit 5165; voice_crit 5166; radio 5185, 5189; hud 5088, 5103, 5195; voice_say 5089, 5104; voice_arm_instr 5187; play_fx 5213; bank1 5209, 5216 | warn 4959, 4974; instr 4960, 4970; say_after 4964, 4967, 4993; hud 4918, 4930, 4988, 5013; voice_say 4919, 4931; voice_instr 4928; play_fx 5000, 5011; commentary 4984 |
| fuelp_pump | warn 5332; instr 5337; voice_pre 5335; crit 5339; voice_crit 5340 | warn 5274; instr 5275; hud 5287 |
| fuelleak_pump | warn 5493; voice_pre 5496; instr 5518, 5521; voice_crit 5519 | warn 5421; instr 5459, 5461; say_after 5460 |
| model_terminal_pump | hud 6282; voice_say 6276; g_voice_dead 6278 | - |
| model_clockout_pump | hud 6341 | - |
| wear_watch_pump | hud 3165, 3177 | - |
| repair_all | hud 5669; voice_say 5670; bank1 5625 | - |
| stint_clear | hud 5553; voice_say 5554; bank1 5573 | - |
| save_toggle | hud 1111; voice_say 1113 | - |
| niggle_pump | hud_blink 6480; bank1 6483 | - |
| heat_pump / arm_overheat | hud 2795 (arm), 2839 (death); radio 2793/2794 (arm); bank1 2811, 2827, 2834 | - |
| arm_engine_stage_one/terminal/seizure | hud 1828, 1845, 1931 | - |
| arm_engine_instant | hud 1863 (dead code: `__attribute__((unused))`, no live caller since E8g item 6) | - |
| stop_msg_pump | hud_live 1339, 1342 | - |
| voice_pump | radio 1679; voice_say 1688; (instruction slot firing) | - |
| death paths | radio 510, 515 (death_blowup_say), 545, 582 (retired pending) | - |
| model_pump (onset log) | voice_say 9749 (ENG01 defensive) | - |
| impacts | voice_say 9831, 9834 (HIT02/HIT01) | - |
| sounds | blow 3026; exhaust pops 4097, 4487, 5323; steam 5213 | pops 4025, 4380, 5277 |
| AI-only | ai_play_fx_at 7951, 8033 (no player notification) | - |
| lamp_pump | lamp sources 9088-9103 (13 mask bits; 5 drive a lamp) | - |

say_after: all 8 call sites (2528, 2545, 4374, 4564, 4964, 4967, 4993, 5460)
are inside `MF_LEGACY_*` blocks -> compiled out. Active `say_after` = 0.

---

## 2. HUD notifications

### 2.1 HUD warnings (diagnosis tier, white; held() + 60 s reminders)

| # | id | text | call site | trigger (verbatim) | vars class | delay | rate limit | gates | verdict |
|---|---|---|---|---|---|---|---|---|---|
| N01 | W_OILLOW | OIL PRESSURE | engine_failure.c:2642 | `g_model_out.oil_gauge_scale < 0.60` | OBS (gauge ratio p_oil/p_healthy) | 1-3 s held | reminder 60 s while held | G-S G-P G-M G-L | PASS (H3; B allows) |
| N02 | W_OILT | OIL TEMP | :2644 | `g_model_out.warn_cool_oil` (model: `oil > opt + 5 AND (base <= 0 OR oil > base + 8)`, mf_engine_model.h:2690-2692) | OBS (oil temp vs optimum/learned) | 1-3 s | 60 s | G-S G-P G-M G-L | PASS (H4; B allows) |
| N03 | W_VALVE | LOSING REVS | :4115 | `g_model_out.warn_valve_air_low` (= `p < MF_VALVE_P_LOW`, internal return pressure) | INT (sanctioned by section B "existing valve ceiling outputs") | 1-3 s | 60 s | G-S G-P G-M G-L | PASS per B; R1 note: internal but explicitly allocated |
| N04 | W_WATER | WATER TEMP | :5148 (cooling fault) and :4862 (no cooling fault) | cooling: `g_model_out.warn_water_temp`; no fault: `w && g_model_out.warn_water_temp` with `w = ok && ...` | OBS (water >= base+12 K and >= 373.15 K, mf_engine_model.h:2800-2801, 3028) | 1-3 s | 60 s | G-S G-P G-M/G-R8 G-L | PASS (H1, H9; B: >=100 C / fault >= normal+12 K) |
| N05 | W_COOLANTLOW | COOLANT LOW | :5146 | `g_model_out.warn_coolant_low` (model: `v_w_l < 0.20 * start`, mf_engine_model.h:2789) | OBS (coolant level) | 1-3 s | 60 s | G-S G-P G-M G-L | PASS (H1; B allows) |
| N06 | W_RADBLOCK | RADIATOR BLOCKED | :5169 | `g_cool_kind == 1 && g_model_out.warn_water_temp` | OBS (water temp) | 8-15 s | 60 s | G-S G-P G-M G-L | PASS (H10 diagnosis; text names the engineer's diagnosis from the temp trace) |
| N07 | W_EXHTEMP | EXHAUST | :4463 | `g_exh_pops >= 1` (visible flame spits; `g_exh_pops++` at :4489) | OBS (visible effect) | 3-8 s | 60 s | G-S G-P G-M G-L | FAIL (H15, R3/R10): text vague; item 15 wants diagnosis "EXHAUST CRACKED", instruction SHORT SHIFT. Fix in N3 pending Claude's approved wording |
| N08 | W_BLOWBY | ENGINE PROBLEM | :4600 | `k > 0.3` where `k = 1.0 - g_model_eng.ring_s` (:4578) | INT (ring seal only; no sensor) | 45-90 s | 60 s | G-S G-P G-M G-L | FAIL (R1): dash trigger is internal-only. Section B said "report if a condition is internal-only" -> fix needs an OWNER DECISION (no approved observable). See stop list |
| N09 | W_FUEL | FUEL PRESSURE | :5332 | `g_model_eng.fuel_cap < MF_FP_STUMBLE` | OBS (delivery/pump capacity) | 1.5-3 s | 60 s | G-S G-P G-M G-L | PASS (B: delivery output; H? audit thresholds) |
| N10 | W_FUELLEAK | FUEL LEAK | :5493 | `g_model_eng.fuel_lost_l >= FUEL_LEAK_WARN_L` (0.1 L) | OBS (fuel loss) | 20-40 s | 60 s | G-S G-P G-M G-L | PASS (B: existing 0.1 L; L2 fix) |
| N11 | - | OVER-REV | :4712 | `ratio > 1.04f` with `ratio = ENG_RPM_LIVE / lim`; rising edge only (`g_or_in`) | OBS (rpm vs current limit) | edge | none while held (edge re-arms) | G-S G-P G-R8 + grace after band lowering (:4686-4701) | PASS (B; H7) |

### 2.2 HUD instructions (yellow; held() + 45 s reminders, wrecked gate)

| # | id | text | call site | trigger | vars class | delay | rate limit | gates | verdict |
|---|---|---|---|---|---|---|---|---|---|
| N12 | I_OILT | COOL THE OIL | :2646, :4245 | `g_model_out.warn_cool_oil && g_warn_blink[W_OILT].on`; second: `warn_cool_oil && !g_instr_blink[I_OILP].on` | OBS (after oil-temp diagnosis) | 8-14 s | 45 s | G-W besides G-S G-P G-M G-L | PASS |
| N13 | I_VALVE | SHIFT EARLIER | :4119 | `g_warn_blink[W_VALVE].on && ENG_REV_LIMIT > REV_SHIFT` | OBS (rpm/limit) + diagnosis | 6-10 s | 45 s | G-W | PASS (model rewards lower revs: float damage and wear scale with rev) |
| N14 | I_OILP | SHORT SHIFT | :4243 | `g_model_out.warn_shift_earlier && g_warn_blink[W_OILP].on` | INT flag but only after the OBS gauge diagnosis | 6-10 s | 45 s | G-W | PASS with R10 note: trigger is internal wear (< 3 s counter) but requires the gauge diagnosis; at low revs the gauge reads lower (model) while the instruction says shift - Claude to confirm R10 |
| N15 | I_WATER | COOL THE ENGINE | :4863, :5149 | `warn_cool_engine && warning blinks on` | OBS (water >= base+18 K and >= 383.15 K) | 8-14 s | 45 s | G-W | PASS (B) |
| N16 | I_FUEL | SHORT SHIFT | :5337 | `g_model_eng.fuel_cap < MF_FP_CUT_FULL && g_warn_blink[W_FUEL].on` | OBS (delivery) + diagnosis | 6-10 s | 45 s | G-W | PASS (lean cut rides high revs; model rewards lower demand) |
| N17 | I_BLOWBY | EASE OFF | :4606 | `k > 0.3 && g_warn_blink[W_BLOWBY].on` | INT (k) after INT diagnosis | 5-15 s | 45 s | G-W | FAIL via N08 (R1 chain). Model does reward less load (`phys` scales with load) - only the trigger needs an approved observable |
| N18 | I_EXHAUST | WATCH THE REAR | :4464 | `g_warn_blink[W_EXHTEMP].on` | OBS (after visible pops) | 20-40 s | 45 s | G-W | FAIL (H15, R10): nothing the driver can act on; item 15 replaces it with SHORT SHIFT (model rewards lower revs = slower crack growth + less rear heat). Fix in N3 |
| N19 | - | SHIFT NOW | :4121 `hud_live` | `g_model_out.warn_valve_float && ENG_RUNNING && !wrecked_or_stopped()` | OBS (float = rev above ceiling; rpm/limit) | live | off 150 ms after false (MFM_LIVE_OFF_MS) | G-L | PASS (R9 live line; H6) |

### 2.3 HUD criticals (red; held() + 90 s reminders, wrecked gate)

| # | id | text | call site | trigger | vars class | delay | rate limit | gates | verdict |
|---|---|---|---|---|---|---|---|---|---|
| N20 | C_OILLEAK | ENGINE CRITICAL | :2656 | `v < MF_MSG_OIL_CRIT_FRAC * MF_OIL_START_L \|\| g_model_eng.h_b < MF_MSG_BEARING_CRIT` (0.25 / 0.30) | INT + INT | 2-5 s | 90 s | G-S G-P G-M G-W | FAIL (R6, pending evidence): thresholds are margins; terminal at 0. List + `critical` radio (N48) need Claude's confirmation that time-to-terminal at these values is imminent for every severity |
| N21 | I_VALVEPIT | ENGINE CRITICAL | :4123 | `g_model_eng.h_vt < MF_MSG_VALVE_CRIT` (0.30) | INT | 2-5 s | 90 s | same | FAIL (R6, same evidence request) |
| N22 | C_OILP | ENGINE CRITICAL | :4249 | `g_model_eng.h_b < MF_MSG_BEARING_CRIT` (0.30) | INT | 2-5 s | 90 s | same | FAIL (R6, same) |
| N23 | C_BLOWBY | ENGINE CRITICAL | :4611 | `g_model_eng.ring_s < MF_MSG_RING_CRIT` (0.30; stop at 0.15) | INT | 2-5 s | 90 s | same | FAIL (R6, same). H14 ordering: the pre line is now gated on the diagnosis + visible smoke; this critical is not textually gated - verify ordering holds |
| N24 | C_WATER | PIT AND RETIRE | :5165 | `g_cool_kind == 2 && (g_model_eng.v_w_l < MF_WATER_SENSOR_FRAC * MF_WATER_START_L \|\| g_model_eng.h_g < MF_MSG_GASKET_CRIT)` (0.20 / 0.35) | OBS level + INT gasket | 2-5 s | 90 s | same | FAIL (R6, same evidence request; H5 fixed the temp-only onset) |
| N25 | C_FUELP | FUEL STARVATION | :5339 | `g_model_eng.fp_floor_s > 0.0` (pump on its floor; R3i: the engine stops `MF_FP_STARVE_S` later) | INTERNAL floor state but the stop is imminent by construction | 1-3 s | 90 s | same | PASS (R6, comment ties to the stop); note: floor state not on a gauge |

### 2.4 HUD end / status / specials

| # | id | text | call site | trigger | verdict |
|---|---|---|---|---|---|
| N26 | - | ENGINE FAILURE | :6282 (model `engine_stop`), :6341 (clock-out), :3165/:3177 (wear death), :2839 (panel heat, dev) | terminal states; H16 fix sets reason text separately | PASS (death line; R9 death exception). Legacy :2555 compiled out |
| N27 | - | STOP THE CAR | :1339, :1342 `hud_live` | live 3-8 s after a mod death while `VEH_SPEED > 1.0` | PASS (death line) |
| N28 | - | ENGINE SAVE ON/OFF | :1111 | control feedback on a deliberate command | PASS (not a fault notification) |
| N29 | - | RADIATOR CLEARED | :5088 (pit clear), :5103 (model blow-clear), :5553 (stint clear) | clear events; see FAIL below | FAIL (H12, item 12/13): fires with no requirement that W_RADBLOCK (N06) was ever shown; :5553 is unstyled dev wording; no 120 s rate limit across re-arms. Fix in N3 pending approved behaviour |
| N30 | - | ENGINE REPAIRED | :5669 | `g_rep_was && RACE_PHASE > 4` where `g_rep_was` includes `!ENG_RUNNING \|\| ENG_STRESS > 0.0 \|\| ENG_LIFE < 0.0` (:5609-5611) | FAIL (item 13b): repair line can fire with nothing diagnosed to the driver and without any diagnosis line; needs the "only after a diagnosis was shown" gate + 120 s rate limit |
| N31 | - | SENSOR FAULT | :6480 | `k == MF_NIGGLE_SENSOR` (harmless niggle roll, E9b item 3) | FAIL (R4 by the letter: a healthy warm car is not silent; owner-approved feature - keep/decide). On a healthy car, engine running, no onset |
| N32 | - | ENGINE PROBLEM | :1828 (stage-one arm), :1845 (terminal arm) | panel/dev arms; fires at the instant of the arm | FAIL (R5): nothing observable at arm; dev panel only (release plan says manual buttons compiled out; no release macro exists today) |
| N33 | - | ENGINE STOPPED | :1931 (panel SEIZE) | arm instant | FAIL (R5, dev panel) |
| N34 | - | ENGINE BLOWN | :1863 (`arm_engine_instant`) | dead code - no live caller since E8g item 6 | N/A (dead) |
| N35 | - | WATER TEMP (+ radio) | :2795 `hud_show` and :2793/:2794 `radio_req` in `arm_overheat` | fires at the panel OVERHEAT arm instant; the legacy fixed timeline in `heat_pump` then runs (not the model) | FAIL (R5): arm-instant notification with a cold engine (H2 class). Dev panel only |
| N36 | - | VALVE RETURN REFILLED | :3990, :4077 | - | compiled out (`MF_VALVE_PIT_TOPUP == 0`) |

---

## 3. Radio calls (mod queue; MFV voice if installed SDT, else vanilla fallback)

Queue behaviour (mf_msg.h:174-251): one request queue, >= 15 s between calls
(5 s death), 30 s stale drop, nothing but a death call while wrecked/stopped
(`mfr_tick(..., wrecked_or_stopped())`), and `radio_req` drops every non-death
request once the model is terminal (`g_model_eng.culminated/blown/stopped`,
engine_failure.c:1352-1366). Fire moment re-checked by `commentary_say`
(`live_drive()`, :421).

### 3.1 One pre-critical call per fault (`g_radio_pre_done`)

| # | id (voice) | text | call site | trigger | delay | verdict |
|---|---|---|---|---|---|---|
| N37 | MFV_VALVE01 | "Losing valve pressure." | :4117 | `g_warn_blink[W_VALVE].on` (after diagnosis) | 3-8 s | PASS (R1/R3; arm_pair=1 -> N61) |
| N38 | MFV_OILP01 | "Oil pressure dropping." | :4241 | `g_warn_blink[W_OILP].on` | 3-8 s | PASS (R3: gauge < 0.60 is exactly the trigger; H3) |
| N39 | MFV_EXH01 | "Exhaust is cracked." | :4466 | `g_exh_pops >= MF_EXH_FLAG_POPS` (3 visible spits) | 5-20 s | PASS with H15 note: marshal sees flames; the crack diagnosis itself is internal but follows a visible symptom; arm_pair=1 arms N63 which FAILs |
| N40 | MFV_BLOWBY01 | "Engine's losing compression." | :4608 | `g_warn_blink[W_BLOWBY].on && g_model_out.smoke_blue > 0.02 && ENG_RUNNING` | 5-20 s | PASS as the H14 fix (diagnosis first + visible smoke); the diagnosis row N08 still FAILs R1 |
| N41 | MFV_WATER01 | "Water temperature rising." | :5153 | `g_cool_kind == 2 && g_warn_blink[W_WATER].on` | 3-8 s | PASS (R3; arm_pair=1 -> N60) |
| N42 | MFV_FUELP01 | "Fuel pressure dropping." | :5335 | `g_warn_blink[W_FUEL].on` | 3-8 s | PASS (R3) |
| N43 | MFV_FUELLEAK01 | "You've got a fuel leak." | :5496 | `g_warn_blink[W_FUELLEAK].on` | 3-8 s | PASS (R3) |
| N44 | EID_MECHWARN 130 | marshals' flag (vanilla) | :2626 `say_pre` | model oil leak: `loss >= OIL_WARN_SEVERITY && ENG_RUNNING` | 5-20 s | PASS (R2 visible smoke); arms OILLEAK01 at :2629 |
| N45 | eid 77 | "engine overheating" (vanilla) | :2668 `say_pre` only `!g_voice_on` | `OIL_LIMIT_K > 300.0 && OIL_TEMP_K > OIL_LIMIT_K` | 5-15 s | PASS-ish R8 note: the game has its own eid-77 warning for native overheat; using eid 77 here can be perceived as the game warning (owner check) |
| N46 | EID_MECHWARN 130 | marshals' flag | :5159 `say_pre` only `!g_voice_on` | `g_cool_kind == 2 && g_cool_steam_on` | 5-20 s | PASS (visible steam) |
| N47 | eid 77 | vanilla overheat | :5201 `say_pre` only `!g_voice_on` | `WATER_K > model_water_base() + MF_WATER_COOL_DK` | 5-12 s | FAIL (B/R3): no absolute floor here (the model's warn_cool_engine adds >= 110 C; this path fires on +18 K alone). Also duplicated by N-fallback of N41/N60. Fix in N3 |
| N48 | MFV_CRIT01 / MFV_BEAR01 / MFV_COOLANT02 / eid 49 | critical calls | :2657 (CRIT01 fallback 49), :4125 (CRIT01/49), :4251 (BEAR01/49), :4613 (CRIT01/49), :5166 (COOLANT02/49) | same conditions as N20-N24 (`voice_crit` mirrors the HUD critical) | 4-8 s | FAIL (R6, same evidence request as N20-N24) |
| N49 | MFV_CRIT01 / eid 49 | FUEL STARVATION critical | :5340 | `g_model_eng.fp_floor_s > 0.0` | 3-6 s | PASS (R6 by construction) |
| N50 | MFV_FUELLEAK02 / eid 49 | "Fuel leak, box now." | :5519 | `g_fl_advice == 2` (measured laps-left below remaining laps) | 3-8 s | PASS with note: session-terminal (will run dry), not engine-terminal; fuel pit is the action the model rewards |

### 3.2 Death calls

| # | id (voice) | text | call site | trigger | verdict |
|---|---|---|---|---|---|
| N51 | MFV_DEAD01/02 or RETIRE04 119 | "Engine's gone..." / vanilla retire | :510/:515 `death_blowup_say`, :545/:582 pending pump | known-certain death (mod blow-up/seize) or game ReasonCode 1 + stopped detection; session != 7 uses DEAD01/02 | PASS (R9 death exception; H16) |
| N52 | MFV_GASKET01 | "Head gasket's gone." | :6276 | `strstr(g_model_out.reason, "gasket")` on a model `engine_stop` | PASS (death diagnosis; reason text drives it) |
| N53 | MFV_DEAD01/02 | random | :6278 + retire pump | every mod stop | PASS (death) |

### 3.3 One-shot calls

| # | id | text | call site | trigger | rate limit | verdict |
|---|---|---|---|---|---|---|
| N54 | MFV_SAVE01 | "Save the engine." | :1113 | save mode switched on (control feedback) | toggle edge | PASS (N/A) |
| N55 | MFV_CLEAR01 | "Problem's cleared, push now." | :1688 | `g_model_eng.stabilised` (once per onset, `g_voice_stab_seen`) | once per onset | FAIL (item 12): a clear line must only follow a diagnosis line of the same onset; a fault can stabilise with no line ever shown. Also no 120 s rate limit across re-arms. Fix in N3 |
| N56 | MFV_OVERREV01 | "Over-rev, watch the downshifts." | :4715 | `ratio > 1.04` rising edge | 120 s | PASS (B; H7) |
| N57 | MFV_OVERREV02 | "That over-rev hurt the engine." | :4722 | while over 1.04: `h_vt` or `h_rod` fell > 0.002 | 120 s | PASS (R3: text = measurable damage) |
| N58 | MFV_RAD03 | "Radiator's clear now." | :5089 (pit), :5104 (blow-clear), :5554 (stint clear, UNCONDITIONAL - outside the `if` on :5553) | clear events | none | FAIL (H12/H13a, item 12/13): no diagnosis requirement; :5554 can play with no debris fault at all (stint clear); no 120 s rate limit. Fix in N3 |
| N59 | MFV_REPAIR01 | "Engine fixed, go." | :5670 | `g_rep_was && RACE_PHASE > 4` (see N30) | none | FAIL (H13b, item 13b): needs "only if a fault had been diagnosed to the driver" + 120 s rate limit. Fix in N3 |
| N60 | MFV_WATER01/02 (no fault) | "Water temperature rising." / "Water temp high, lift and coast." | :4870, :4876 | `warn_water_temp` / `warn_cool_engine` with `g_warn_blink[W_WATER].on` | 120 s each + once per episode (said1/said2) | PASS (B; H9 fix) |
| N61 | MFV_ENG01 | "We're seeing an engine problem." | :9749 | defensive: `fault < MF_FAULT_VALVE_AIR \|\| fault >= MF_FAULT_COUNT` | once | N/A today (all faults 1..9 map to their own pre lines) |
| N62 | MFV_HIT01 / HIT02 | "Big hit..." / "Damage after that hit..." | :9834 / :9831 | `g_model_out.impact_fired` (impulse or g threshold met) | once per impact | PASS (driver-felt, R2; HIT02 spends the fault's pre-critical slot by design) |

### 3.4 MFV instruction slots (2nd lines; fired by voice_pump / mfi_tick_if)

Arming: `voice_pre(..., arm_pair=1)` or `voice_instr()` or the chain; firing:
`voice_pump` :1667-1683. Eligibility at fire time from `mfv_instruction_eligible`
(mf_voice.h:111-124) except where noted.

| # | line | text | armed at | eligibility guard | verdict |
|---|---|---|---|---|---|
| N63 | MFV_VALVE02 | "Valves are floating, short shift." | :4117 -> :1612 -> :1472 | `valve_float` at fire time | PASS (H6) except the R7 silent-drop FAIL below |
| N64 | MFV_WATER02 | "Water temp high, lift and coast." | :5153 pair, and :5204 direct `voice_instr(R_WATER77)` | `cool_engine` | PASS; note: double arm of the same slot (pair + :5204) - last arm wins |
| N65 | MFV_OILP02/03 | "Oil pressure low, short shift." / "Low oil pressure, keep the revs down." | :4241 pair (alt picked in voice_arm_instr) | `oil_warning` = gauge < 0.60 OR warn_oil_pressure | PASS with N14 R10 note |
| N66 | MFV_RAD02 | "Radiator blocked, find clean air." | :5187 -> voice_arm_instr | `debris` (fault == RAD_DEBRIS && f_rad < 0.999) | PASS (R3/R10: clean air = lower debris clear chance? model: debris clears by airflow q (`pclr` scales with q at speed) - "find clean air" is the right advice; R7 applies) |
| N67 | MFV_EXH02 | "Exhaust problem, keep pushing." | :4466 pair -> mfv_instr_of(EXH01) | default 1 (no guard) | FAIL (H15, R10): contradicts the model (rev^2 crack growth); item 15: do NOT change the SDT, mark OWNER DECISION NEEDED - new recording, stop using it until replaced |
| N68 | MFV_OILLEAK01/02 | "You're leaking oil." / "Oil leak, take it easy." | :2629 (after MECHWARN), chain :1681 | default 1; active/same-fault gate | PASS (R3) with R7 |
| N69 | MFV_OILT01 | "Oil temperature high, back off." | :2649, :4247 | `oil_hot` = warn_cool_oil | PASS |
| N70 | MFV_COOLANT01 | "You're losing coolant." | :5155 | `g_model_eng.v_w_l <= MF_WATER_START_L - 1.0` | PASS (B: coolant level) |
| N71 | MFV_FUELP02 | "Fuel pressure low, keep it smooth." | :5335 pair | default 1 | PASS with R10 note (lean cut is rev/load-driven; smoothness rewarded) |

R7 FAIL (all N63-N71): `mfi_tick_if` (mf_msg.h:281-286) clears the slot
silently when not eligible at the fire time; no hold-until-eligible, no
"VOICE: <id> dropped (<reason>)" log. N3 implements the approved hold (max
120 s) + drop log and msg/voice tests.

---

## 4. Lamps and shift lights

| # | kind | source | mapping | code | verdict |
|---|---|---|---|---|---|
| N72 | lamp engine | held(W_OILT), held(W_WATER), held(W_RADBLOCK) | MF_LAMP_ENGINE (mf_lamps.h:43-56) | lamp_pump :9077-9129 | PASS (R8 temperature-only per owner 2026-10-06). Mask also sets VALVE/OILP/OILLOW/BLOWBY/EXHTEMP/CRITICAL/DEAD/COOLANTLOW bits but the mapping returns 0 for all of them |
| N73 | lamp fuel | held(W_FUEL), held(W_FUELLEAK) | MF_LAMP_FUEL | same | PASS (R8) |
| N74 | shift lights | rpm / CURRENT limit via `mf_shift_mask` steps (0.90/0.93/0.96/0.977) | mf_lamps.h:93-100, shift hook :9040-9042 | 0x49B7A0 detour | PASS (R1: rpm vs current limit) |

Note: the `lamp_pump` comment (:9070-9071) says "+ the engine-dead latch", but
`mf_lamp_bit_for_source` maps MF_LS_DEAD to 0 (only OILT/WATER/RADBLOCK drive
the engine lamp). Comment vs mapping discrepancy; the executed rule matches the
owner's 2026-10-06 temperature-only decision.

History H8 (stuck lamp from a whole-int bool) fixed: states are per-source bits
(`g_w_held`), only sources evaluated in the current worker pass count
(`g_w_pass`), frozen pauses keep the last value.

---

## 5. Sounds

| # | sound | call site | trigger | gates | verdict |
|---|---|---|---|---|---|
| N75 | mf_engine_blow.wav | :3026 `blow_sound` (from blow_burst_start) | terminal blow-up | live_drive | PASS (death) |
| N76 | mf_steam_hiss.wav | :5213 (model cooling, first `smoke_bank1 < 0`) | steam plume write | live_drive via play_fx | PASS as an effect; the plume itself: see N79/H10 |
| N77 | backfire pops | :4097 (valve float over ceiling), :4487 (crack pop), :5323 (lean pop) | engine-visible misfire/flames | live_drive | PASS (R2 visible/audible effect; legacy :4025/:4380/:5277 compiled out) |
| N78 | AI-only blow/hiss | :7951, :8033 `ai_play_fx_at` | AI shadow engines | AI paths | N/A (no player notification) |

## 6. Particle banks (visible effects; R2 allows radio based on them)

| # | write | call site | trigger | verdict |
|---|---|---|---|---|
| N79 | bank1 steam | :5209 `bank1_set(g_model_out.smoke_bank1, "cooling")` | model: water leak steam path (:2752-2760) needs `water > base + 10 K AND >= 373.15 K`; the LOW-COOLANT path (:2782-2792) writes `MF_WATER_LOW_STEAM` whenever `v_w_l < 20%` with NO temperature floor | FAIL (H10 item 10): low-coolant steam can show at 84 C (R3g path). The escape-steam path has the 100 C floor; the low-coolant path does not. Fix in N3 per H10 |
| N80 | bank1 clears | :5043, :5216 (cooling), :6246/:6254 (model stop plume), :6472/:6483 (niggle puff), :5573 (stint), :5625 (repair) | state changes | PASS (FX hygiene) |
| N81 | bank1 legacy heat | :2811, :2827, :2834 | panel OVERHEAT fixed timeline | FAIL R5-adjacent (dev panel; same as N35) |

---

## 7. History items H1-H16 -> current state

| H | subject | current state |
|---|---|---|
| H1 | WATER TEMP at 79-80 C low-coolant | FIXED: N05 is level-only; N04 needs >= 100 C (mf_engine_model.h:2789, 2800) |
| H2 | Steam + COOL THE ENGINE at 44 C | FIXED for the modelled paths (:2800-2803 floors, learned baseline); see H10 for the low-coolant steam remainder |
| H3 | OIL PRESSURE while gauge reads 1.0 | FIXED: N01/N41 gauge-only (:2642, :4238) |
| H4 | Overheat icon for a valve fault at 102 C oil | FIXED: lamp temp-only (N72); oil temp is a temp source but valve no longer lights the lamp |
| H5 | PIT AND RETIRE spammed on +18 K | FIXED: N24 needs level < 20% or gasket < 0.35 |
| H6 | Valves floating line silently dropped | FIXED: N63 eligibility `valve_float` at fire time (but R7 silent-clear remains for all slots) |
| H7 | OVER-REV radio during a bearing failure | FIXED: N56 comes from the always-on over-rev layer only; band-lowering grace :4686-4701 |
| H8 | Engine lamp stuck on | FIXED: N72 per-source bits + `g_w_pass` |
| H9 | Water warnings vs learned normal | FIXED: N04/N15 use baseline + absolute floors |
| H10 | Steam at 84 C low coolant | **NOT FIXED** (N79) - fix in N3 as instructed |
| H11 | REPAIR on hot engine -> EMERGENCY onsets | **NOT FIXED** - N5 batch |
| H12 | Radiator clear without a debris diagnosis | **NOT FIXED** (N29, N58) - fix in N3 |
| H13 | RAD03 x6 / REPAIR01 x6 noise | a) N58 not fixed; b) N59 not fixed; c) 120 s per-line rate limit missing for clear/repair lines (N3) |
| H14 | BLOW-BY critical before diagnosis | pre line now gated on diagnosis + smoke (N40); critical N23 is a 0.30 margin - ordering to verify with R6 evidence |
| H15 | EXHAUST vague + WATCH THE REAR + EXH02 "keep pushing" | **NOT FIXED** (N07, N18, N67) - fix in N3 per item 15 (do not touch SDT; EXH02 stays replaced by an OWNER DECISION item) |
| H16 | Death reason wrong; fuel lost not logged | reason: clock-out -> "engine life expired" (mf_engine_model.h:3293), over-rev blame -> "over-rev: valve hit piston" (:6331 and model :2472) = FIXED as far as source shows; **fuel-lost litres in the 5 s line is MISSING** (no `fuel_lost_l` in any 5 s log; only `oil_l` in "MODEL 5s") - fix in N3 |

---

## 8. FAIL summary and stop-list for Claude

FAIL rows (fix candidates for N3, each needs Claude's approval / a section-B
threshold where indicated):

1. R7 (all MFV instruction slots): silent drop, no hold, no drop log (mf_msg.h:281-286). Pre-approved in the prompt.
2. H10 low-coolant steam below 100 C (N79). Pre-approved.
3. H12/H13a RADIATOR CLEARED + RAD03 diagnosis-less clears (N29, N58), including the unconditional voice at :5554.
4. H13b REPAIR01 / ENGINE REPAIRED only after a showed diagnosis (N30, N59).
5. H13c 120 s per-line rate limit for clear/repair/radiator lines.
6. H15 EXHAUST wording + WATCH THE REAR + EXH02 (N07, N18, N67). EXH02 = OWNER DECISION NEEDED (new recording) - do not touch the SDT.
7. R1 internal-only BLOW-BY dash + instruction (N08, N17): needs an OWNER DECISION - no approved observable/threshold exists in section B.
8. R6 critical margins (N20-N24, N48): need Claude's confirmation that each is tied to an imminent terminal (list: h_b < 0.30, h_vt < 0.30, ring_s < 0.30, h_g < 0.35, coolant < 20%, oil < 25%). Not a code fix unless Claude rules them margins.
9. R5 panel/dev arm HUD+radio (N32, N33, N35, N81): dev-only paths; decide whether they are in scope (release compiles manual panel buttons out per README, but no release macro exists today).
10. R4 niggles SENSOR FAULT (N31): owner-approved feature; flag for the R4 decision.
11. H16 fuel-lost litres missing from the 5 s log line (logging fix, N3).
12. R8 note eid 77 duplicate-owner (N45/N47 fallback only with vanilla SDT).

Not FAILs but watch items: N14/N71 R10 wording against gauge/rev behaviour;
radio_pump lacks an explicit `g_pump_frozen` guard (fire moment nevertheless
checked by `live_drive()`); N47 has no absolute floor.

STOP conditions encountered in N1: none (no code change was needed, no new
thresholds were used, no gate run had to be skipped). Gates were not re-run for
N1 because no code changed; the batch log states this and quotes the prompt
baseline. N2 starts the refactor and will run the full gate list.

Limitations: verdicts are a static read of the pinned source; no live session
was used, no game folder touched, no install. Trigger rows marked R6/R10
depend on model calibration evidence held in the handoff, not on N1
measurement.
