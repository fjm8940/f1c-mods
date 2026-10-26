# AI engine symptoms + AI driver reactions - specification (Claude, 2026-10-01 g)

Status: SPEC for DeepSeek (ADDENDUM 14 of `DEEPSEEK_AI_ENGINE_J_PROMPT.txt`). Pipeline stage: specify.
Product: mechanical_failures (merged DLL). Start binary: dll D7F23A4E (engine_failure.c CD68E58C,
mf_engine_model.h E8433FBA). Owner: Matias.

Read first: `AI_FAULT_PRESENTATION_PLAN_2026-10-01.md` sections 8-10b (live lever facts),
`AI_FAILURE_REACTION_PLAN_2026-10-01.md` sections 2, 3, 11-13 (levers, traits, owner decisions, A0 trials).

## 0. Owner direction (2026-10-01 g, verbatim intent)

1. Every effect/symptom a fault gives the PLAYER also happens to the AI car (power, revs, smoke incl. BLUE for
   oil faults, flames, sounds, ending).
2. AI drivers REACT properly to the symptom. Example: valve float - the AI already knows how hard it can rev /
   how much throttle it can use without floating the valves, so it drives just under the limit.
3. Mistakes happen sometimes: an AI can misjudge, over-rev, push too long, and blow the engine.
4. Rough idle: dropped (player rarely hears AI idling).
5. Gearbox: out of scope (vanilla AI stuck-gear already exists; gearbox sim is a separate future product).

### 0.1 What this supersedes (decision record)

| Old rule | Where | New rule |
|---|---|---|
| "Reaction-dependent physics (valve float, oil pickup, overheat, ...) player-only; AI keeps power loss, not damage" | `mf_engine_model.h` valve case `if (!in->ai_mode)`, memory project-ai-engine-fairness-rule, ADDENDUM 13 "no reaction damage" | AI runs the SAME damage physics as the player. The AI reaction layer keeps AI inputs (rpm, load) inside safe margins; damage then only comes from margin choice + mistakes |
| ADDENDUM 13 "presentation only" | DS prompt | presentation + reaction + physics |
| q6 variant 1 "exact vanilla" gate | ADDENDUM 13 A7 | replaced by the fairness CALIBRATION gate (section 7) - owner D1 = A (mean +-10 %) |
| AI rev cap = 0x39A4 + 0x3A18 | plan section 8 | + downshift array 0x3A1C..0x3A38 (section 2, DERIVED) |

## 1. Architecture (one screen)

```
model (per AI slot, existing shadow MfEngine)  -> MfOut (same struct as the player)
        |                                             |
        |                         mf_ai_present(): MfOut -> AiSymptom {power shape, rev cap, misfire,
        |                                         banks, blue, flames, sounds, ending}     (pure C)
        |                         mf_ai_react():  AiSymptom + traits + race context -> AiReaction
        |                                         {margin, rev_cap, power trim, passing, gap, pit, retire,
        |                                          mistake events}                          (pure C)
        v                                             v
AI pump (worker)  ---- writes levers (ownership-checked, saved/restored) ---- game thread queue (blue, sound)
```

- Both new functions are pure C in a new header `dll/mf_ai_react.h` (no game access), fully unit-testable.
- Reaction changes the real AI rpm/load the model reads next step -> wear really slows (same equations).
- Deterministic per seed: RNG = per-slot `MfRng` seeded from session seed + slot (existing pattern).

## 2. Lever table (what the DLL may write for an AI car)

Tier: FACT = live-proven 2026-10-01 (plan sections 8, 10, 10b, reaction plan section 12).

| Lever | Field | Tier | Rules |
|---|---|---|---|
| Power vs rpm | torque table ptr veh+0x39B0, count 0x39BC, pairs {coast, power}; scale column [1] per row | FACT (x0.85/0.70/0.50 top 279/258/221 vs 304 km/h) | save the whole table at first write; shape per row (section 3.1); shared-table check (pointer unique in field) else no lever |
| Hard top cut | power column rows with rpm >= X set 0 | DERIVED (live test used 20 ms polling; row-zeroing is the static equivalent) | prefer row-zeroing over polling; always pair with the rev-cap triple |
| Rev cap triple | limit veh+0x39A4, upshift veh+0x3A18 = 0.975 x cap, downshift array veh+0x3A1C..0x3A38 (8 floats) scaled by cap/limit | 0x39A4+0x3A18 FACT; downshift array DERIVED (live: 53 gear changes / 90 s = normal shifting; before, without it: fast gear hunting, owner-seen). Entries <= 1.5 x idle are NOT scaled (unused gear slots) | write all three together, restore all three; never write one alone |
| Misfire | power column 0 for 40-120 ms bursts | FACT (owner: stutter visible) | game-thread or <= 20 ms cadence; rewrite column values only (never swap the table pointer) |
| Smoke banks | veh+0x3D54/58/5C | FACT; bank1 graded -0.1/-0.3/-0.6 white (owner: grows), bank1 + grey, bank3 0.5 black, 1.0 fire | persist until rewritten -> mod owns and restores |
| Flame spit | bank3 = 1.0 for 0.3 s on lift (throttle veh+0x357C < 0.1), >= 1 s apart | FACT (owner: real flames) | read throttle only, never write it |
| Blue smoke | SpecialFX id 2 via particle queue with slot field, pos veh+0x34E0/E4/E8 | DERIVED design (plan section 9) | game thread only (0x447B10 drain) |
| 3D sounds | mf_spatial at AI pos (pops, hiss, bang) | DERIVED design | camera-range gate |
| Engine roughness sound | rpm veh+0x39A0 x (1 +- a) every 10 ms, or limiter flutter (0x39A4 random 85-100 % / 80 ms) | DERIVED (owner: "sounds very weird"; which of the two not separated) | optional effect; ownership: 0x39A0 is physics output - jitter only while the effect runs; flutter writes the triple |
| Speed cap | maxSpeed veh+0x4538 m/s | FACT holds; owner earlier: "artificial" for nursing | ONLY for terminal limp (section 4.10); game writes this field too (22.2 seen) - restore only if ours |
| Engine clock | veh+0x3D70 | FACT extend holds | mod owns while a fault is active (existing) |
| Ending (Option A) | power column 0 + idle 0x39A8 0 + ReasonCode 0x4068 = 1 | FACT (clean FinishState 2, results "Engine") | existing A5 path |
| Pit call | callToBox veh+0x454C | FACT alone does not retire | R3 only |
| Overtaking | PassingNorm veh+0x44CC | per reaction plan section 2 (live tier there) | restore on end |
| Following gap | TrackAggression veh+0x44F8 | CONFIRMED | restore on end |
| FORBIDDEN | throttle 0x357C / brake 0x3584 / steering / pace 0x453C / positions | rewritten every 25 ms or DISPROVEN | never write |

Consequence: the AI's "throttle discipline" is expressed as rev cap + power trim + top cut (the AI's own throttle
cannot be written). "Shift earlier" = lower the rev-cap triple (the AI shifts by its own thresholds).

## 3. Shared building blocks

### 3.1 Power shape (same as the player hook)
Per table row r with rpm w_r: `mult_r = clamp(1 - loss_all - loss_top * s(w_r) - loss_load * 1.0, MF_POWER_MIN, 1)`
with `s = smoothstep(peak_tq_rads, limit, w_r)` (identical to the player hook; AI throttle is ~1.0 on straights,
so loss_load uses throttle 1.0 - inference, documented). Rewrite only when any mult_r moves > 0.01. Misfire
pulses multiply on top.

### 3.2 Rev cap (the "known limit")
`true_limit` = what the fault physically allows (valve: `out->rev_ceiling x revlimit`; fuel top cut: rpm where
`loss_top x s >= 0.5`; oil/bearings: none). The driver does NOT know it exactly:
`perceived = true_limit x (1 + N(0, sigma_p))`, sigma_p = 0.02 (high sensitivity) .. 0.05 (low), re-rolled at each
escalation. `cap = perceived x (1 - margin)`, margin per reaction (section 5). If cap > true_limit the car floats
occasionally = honest mistake (no special code).

### 3.3 Mistakes (the "sometimes they blow it")
Per-slot Poisson events while a fault is active, rate `lambda = base_mistake x (1 + 2 x crash_trait) x context`:
- base_mistake = 1 / 600 s;
- context x2 when within 1.0 s of a car ahead/behind (battle), x1.5 last 10 % of race, x0.5 when nursing R2.
Mistake kinds (weighted): M1 missed upshift = rev cap lifted to the full original limit for 1-3 s (40 %),
M2 "push lap" = margin 0 for 1 lap (40 %), M3 late notice = reaction delayed by a second notice delay (20 %).
Every mistake logged `AI MISTAKE: slot N <driver> M1 1.8 s fault valve`.

### 3.4 Notice delay (from reaction plan 3.2, kept)
`delay = base(fault) x sens_factor x LogNormal(0, 0.4)`; sens_factor 0.5 (RaceAbility top) .. 2.0. Before notice:
reaction R0 (symptoms still shown - the CAR shows them, the driver has not reacted).

### 3.5 Traits (reaction plan 3.1, kept): sensitivity, mechanical sympathy, racer, team order; .rcd-based + N(0,0.15).

## 4. Fault by fault

Columns: PLAYER today (model/DLL) -> AI SYMPTOM (levers) -> DRIVER CUE -> REACTION (after notice) -> MISTAKE ->
ENDING. Numbers are starting values; DS keeps them as named constants `MF_AIR_*` in mf_ai_react.h.

### 4.1 Valve return air (MF_FAULT_VALVE_AIR)
- Player: loss_top above peak rpm; rev_ceiling falls (sqrt p, floor by severity); above ceiling = valve float:
  misfire (vfuel_valve 0.5) + h_vt damage (quadratic in over-rev) -> "valve hits piston" blow-up.
- AI symptom: power shape loss_top; above ceiling the float misfire = misfire pulses (section 2) while rpm >
  true_limit; engine roughness sound optional while floating.
- Cue: revs stop climbing, misses at the top. Notice base 8 s.
- Reaction: R1 rev cap triple at `perceived x (1 - 0.02..0.04)` (sympathy trait: smooth drivers 0.04); follows the
  falling ceiling every escalation (ceiling drop > 3 %). Passing unchanged at R1. If ceiling < 0.80: R2 (margin 0.05,
  PassingNorm low). Retire never chosen while ceiling > MF_VALVE_CEIL_MIN floor (car can limp home - realistic).
- Remove the `if (!in->ai_mode)` guard on h_vt: AI float damage is live, accrued only when the AI actually floats
  (perception error, M1/M2 mistakes, pre-notice).
- Mistake: M1 over-rev into float = big h_vt hit -> possible blow-up (the owner's "they might just blow it").
- Ending: blow-up = bank3 1.0 fire + black 0.5 + bang sound + Option A.

### 4.2 Oil pressure (MF_FAULT_OIL_PRESSURE)
- Player: pump_eta falls -> p_oil low at low rpm and under slosh (g_comb); film < crit -> bearing wear (h_b),
  loss_all; gauge flicker; warn_oil_pressure; blue smoke scaled by oil lost (R2a); "bearing seizure" blow-up.
- AI symptom: loss_all power shape; blue smoke ONLY if oil is being lost (pressure fault alone: none - matches the
  player); when h_b < 0.3 roughness sound + occasional misfire pulse (bearing knock, inference).
- Cue: warning light (film < crit). Notice base 4 s (light is obvious).
- Reaction: film rises with rpm (p_oil ~ rev below 0.55) but bearing load rises too - the safe zone is mid-rpm:
  R1 rev cap triple at 0.90; R2 (h_b < 0.5) cap 0.85 + PassingNorm low + longer gap (less slosh in battles is not
  modelled - gap is presentation). R4 retire weight high when h_b < 0.25 and laps left > 15 % (era: stop to save the
  engine), else limp to the flag.
- Mistake: M2 push lap -> faster h_b drop.
- Ending: bearing seizure = blow-up (fire + black + bang) per the player reason; or R4 retire (Option A without fire,
  results "Engine").

### 4.3 Oil leak (MF_FAULT_OIL_LEAK)
- Player: oil_a grows with rev^2, v_oil drops, cascade below 60 % sump -> bearing wear; blue smoke by oil lost;
  oil heat; "bearing seizure".
- AI symptom: BLUE smoke density = player formula (oil lost fraction) via the AI blue queue; loss_all from bearings
  after cascade; oil heat as the model already computes.
- Cue: smoke in mirrors / team sees it (blue visible) + oil temp. Notice base 30 s, x0.5 once blue density > 0.3.
- Reaction: leak grows with rev^2 -> R1 cap 0.92 (real gain in the model); cascade active -> R2 cap 0.88. R4 weight
  rises when sump < 30 % and laps left > 20 %. No pit fix (owner: no oil top-ups).
- Mistake: M2.
- Ending: seizure = blow-up; or engine stop (no fire) when sump empty and h_b 0 - use the model's reason.

### 4.4 Water leak (MF_FAULT_WATER_LEAK)
- Player: pressurised leak, steam bank1 -0.1..-0.6 while escaping and hot; low coolant -> oil heat, gasket wear;
  "head gasket" stop with white smoke -0.6.
- AI symptom: bank1 white = the model's smoke_bank1 (FACT graded); gasket end = white -0.6 + stop (no fire).
- Cue: water temp / steam. Notice base 40 s, x0.5 when smoke_bank1 <= -0.3.
- Reaction: loss rises with (1 + rev) and pressure -> R1 cap 0.93 + TrackAggression low (bigger gap, clean air -
  presentation, plus the model's cooling uses speed; inference); R2 when coolant < sensor fraction: cap 0.88,
  PassingNorm low. R4 when cascade gasket h_g < 0.4 and laps left > 15 %.
- Ending: head gasket = engine stop, white smoke, no fire, Option A.

### 4.5 Radiator debris (MF_FAULT_RAD_DEBRIS)
- Player: airflow scale f_rad, per-second clear chance scales with dynamic pressure q; warn_pit_debris.
- AI symptom: none visible (correct: debris not visible); temps rise (model).
- Cue: water temp fast. Notice base 15 s.
- Reaction: clear chance scales with q (speed) -> NOT a rev cap. R1 = TrackAggression low (drop out of the
  slipstream, inference: cleaner air); R3 pit-clear weight 40 % when warn_pit_debris and laps left > 10 %: callToBox,
  repair time +3..8 s (PitStopEndTime extend UNPROVEN - if not proven, the stop is normal length and the fault is
  cleared at pit exit).
- Ending: via the cooling cascade -> gasket (4.4 ending).

### 4.6 Fuel pressure (MF_FAULT_FUEL_PRESSURE)
- Player: fuel_cap falls; stumbles (1.5 s dips, rolled per second, scaled by load) below MF_FP_STUMBLE; top-end cut
  loss_top below MF_FP_CUT_FULL; native lean cut below MF_FP_HEAVY; lean rings wear; "fuel starvation" stop.
- AI symptom: stumble = misfire pulses for 1.5 s at depth fp_stumble_d; top cut = power shape + hard top cut rows
  when cut > 0.5 (FACT top-cut lever); starvation stop = Option A, no smoke.
- Cue: stutter on full throttle. Notice base 4 s.
- Reaction: lean wear rises with demand (rpm x throttle) -> R1 cap at the rpm where demand = fuel_cap (true_limit,
  perceived per 3.2) - shifting early lowers demand; R2 below MF_FP_HEAVY: cap 0.85.
- Ending: fuel starvation = coast, silent, no smoke.

### 4.7 Fuel leak (MF_FAULT_FUEL_LEAK)
- Player: fuel_lost_l grows (E4 removes fuel); no visible symptom until dry.
- AI symptom: AI fuel removed the same way IF the AI fuel field is proven (OPEN - A0 item); else no symptom (owner
  default 2026-10-01: no AI symptom).
- Cue: team sees fuel (1-3 laps). Reaction: R3 splash if laps left > fuel left; else economy R1 (cap 0.92).
- Ending: runs dry = coast, silent.

### 4.8 Blow-by / rings (MF_FAULT_BLOWBY)
- Player: ring_s falls with piston temp and load; crankcase oil loss; blue smoke 1 - ring_s; loss_load; "blow-by" stop
  with MF_RING_BLUE.
- AI symptom: BLUE smoke = 1 - ring_s via the AI blue queue (owner: oil faults = blue); loss_load power shape.
- Cue: smoke + power fade. Notice base 5 s.
- Reaction: ring wear ~ load x piston temp -> R1 cap 0.92 (lower load/temp in the model); R2 when ring_s < 0.5:
  cap 0.88, PassingNorm low. R4 weight when power < 0.70 and laps left > 15 %.
- Ending: blow-by stop = heavy blue, no fire, Option A.

### 4.9 Exhaust crack (MF_FAULT_EXHAUST)
- Player: Paris-law crack growth on temperature swings + vibration (rev^2); pops (pop_chance_s); rear heat ->
  break_rear_suspension; "EXHAUST TEMP" messages after first pop.
- AI symptom: flame spits (bank3 1.0 for 0.3 s on lift, FACT) at pop_chance_s; pop sound 3D; rear suspension break:
  no AI suspension lever - OPEN (log only, treat as retire R4 with ReasonCode of suspension if proven, else engine).
- Cue: noise/pops. Notice base 20 s.
- Reaction: growth ~ swings + rev^2 -> R1 cap 0.94 (vibration term), mostly R0/R1 (realistic: drivers carried on).
- Ending: rear break -> R4 retire (OPEN lever) or keep running.

### 4.10 Cross-cutting
- Cascades (oil leak -> bearings, cooling -> gasket): the reaction follows the CURRENT dominant symptom.
- Niggles (harmless misfire, sensor, puff): AI shows misfire pulses / puff bank1 -0.1 for the niggle time; driver
  reaction R0 (they ignore) except 10 % R1 cap 0.97 for 1 lap (worried driver).
- Over-rev (E9a culm_overrev, any engine): the AI downshift array keeps over-revs rare; M1 mistakes can over-rev.
- Impact faults (E9h): same per-fault rows.
- Terminal limp (cap < 0.70 or power < 0.55 and the driver keeps going): maxSpeed 0x4538 may be used as the final
  "crawling back" cap - only here.
- Pre-notice: symptoms shown, no reaction (R0).

## 5. Reaction levels (numbers per level, per fault overrides in section 4)

| Level | Margin under perceived limit | Rev cap if no fault limit | PassingNorm | TrackAggression | Notes |
|---|---|---|---|---|---|
| R0 ignore | 0 | none | as file | as file | pre-notice or racer |
| R1 nurse | 0.02-0.04 | 0.92-0.94 | as file | x0.8 | default |
| R2 limp/defend | 0.05 | 0.85-0.88 | x0.3 | x0.5 | lets faster cars by |
| R3 pit | as R1 until the box | - | - | - | debris / fuel splash only |
| R4 retire | - | - | - | - | Option A ending without fire (results "Engine"); never on the last lap |

Choice = weighted dice (reaction plan 3.3 / 4.1 mixes + section 5 context modifiers, kept). Escalation only on a
clear worsening (severity stage, ceiling -3 %, h_b/ring_s/coolant step of 0.1); never de-escalate below R1 once noticed.
Restore every lever on: fault cleared, retirement done, session end, DLL unload, pause = freeze (no writes).

## 6. Presentation details

- Smoke owner: one function decides the bank values from (symptom, ending) each step; never two writers.
- Blue: density 0..1 -> burst rate exactly like the player's `emit_bluesmoke_burst` scaling; AI bursts capped
  (budget: all AI blue <= 2x the player rate total) to protect frame time.
- Sounds: pops on flame spits, hiss with steam, bang on blow-up, all mf_spatial at veh+0x34E0, camera range only.
- Panel AI page: per slot `fault / sev / noticed / level R0-R4 / cap % / margin / mistakes`.
- Log lines: `AI REACT`, `AI MISTAKE`, `AI END` with driver name.
- Radio/commentary: owner decision pending (handoff g action 3); default vanilla "he's blown the engine".

## 7. Fairness and calibration (owner decision D1 = A, 2026-10-01 g)

Default: fault ONSET for AI stays vanilla-calibrated (WHEN = vanilla timer, mean-preserving WHO). The OUTCOME
(nursed home / retired / blown) comes from physics + reactions + mistakes. Calibrate `base_mistake`, margins and R4
weights in the field sweep so mean AI engine retirements per race = vanilla +- 10 %, and 20-40 % of AI faults are
nursed to the flag. Report per season folder (1997, 2005 at least) and by RaceAbility tercile (better drivers save
more). DECIDED (owner, option A): this +-10 % mean gate REPLACES q6 variant 1 exactness for the AI side; player gates unchanged.
Rejected: B free physics (no count target), C other tolerance.

## 8. DeepSeek work order (ADDENDUM 14, items B1-B9)

B1 HEADER: `dll/mf_ai_react.h` - AiTraits, AiSymptom, AiReaction structs; `mf_ai_present(const MfOut*, const MfEngine*,
   const MfCar*) -> AiSymptom` (section 4 per fault); `mf_ai_react(...)` (sections 3, 5); constants named, commented with
   the section number. No game access.
B2 MODEL: remove the AI guard on valve float damage (4.1); keep everything else in the model untouched.
B3 LEVERS: rev-cap triple writer (0x39A4, 0x3A18, 0x3A1C..0x3A38 with the idle rule), power shape per row, top-cut rows,
   misfire pulses, banks owner, flame spit, maxSpeed terminal limp, PassingNorm/TrackAggression; all ownership-checked,
   saved at first write, restored per section 5. Shared-table check kept.
B4 BLUE + SOUND: particle queue slot field (plan section 9), AI blue for oil leak / blow-by / oil cascade; mf_spatial at
   AI pos. Game thread only.
B5 MISTAKES + PERCEPTION: section 3.2/3.3, per-slot RNG, logged.
B6 ENDINGS: per fault per section 4 (fire only for blow-ups), via existing Option A path.
B7 TESTS: new `tools/ai_react_test.c` - every fault x level reachable, escalation monotonic, no de-escalation below R1,
   restore completeness (all levers back to the snapshot), determinism per seed, mistake rate within +-20 % of lambda
   over 10^6 s, rev-cap triple always written together (static check: no single-field writer).
B8 SWEEP: extend car_sweep with the reaction layer; report section 7 numbers. Do not change the player gates.
B9 GATES: model test, msg test, ai_react_test, sweep report, grade_test, GUI unittest, panel smoke, validator, build x2,
   AI pump <= 6 ms; batch log entry per item with hashes; owner live checklist (section 9).

Hard rules (ADDENDUM 13 kept): pause = no writes; never write throttle/brake/steering/pace/positions; SDK front door for
every new address; append findings; prior-art grep before deriving; UTF-8 no BOM; use the Edit tool for C (heredoc
escape trap).

## 9. Owner live checklist (after DS)

1. Valve air AI: shifts earlier, rare float misses, occasional blow-up after a mistake.
2. Oil leak + blow-by AI: BLUE smoke growing.
3. Water leak AI: white steam growing, white end, no fire.
4. Fuel pressure AI: stutters, top cut, early shifts, no gear hunting.
5. Exhaust AI: flame spits on lift + pops.
6. Nursing AI lets you by (R2), retirement via garage (R4).
7. Field race: retirements per race feel like vanilla.

## 10. Open items / evidence status

- Downshift array 0x3A1C..0x3A38: DERIVED (cross-car ratios + one live run, 53 shifts / 90 s). Owner visual verdict for
  run 2 pending; per-gear index mapping (which entry = which gear) unproven.
- Running hot (exp 9) owner verdict pending; jitter vs flutter (exp 10) not separated.
- AI fuel field (4.7), AI rear suspension lever (4.9), PitStopEndTime extend (4.5): OPEN.
- D1 fairness gate: DECIDED A (mean AI engine retirements/race = vanilla +-10 %, 20-40 % nursed home).
- Player rpm read 0x39A0 static at 417 while parked with FinishState 3: player rpm field / state unclear (log only).
