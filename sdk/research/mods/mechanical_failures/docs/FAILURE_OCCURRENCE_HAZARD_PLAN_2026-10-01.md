# Failure occurrence: countdown -> wear-driven hazard (PLAN), 2026-10-01

Owner: Matias. Author: Claude. Status: DRAFT, owner discussion 2026-10-01. Stage: define/specify.
DeepSeek executes only the mechanical phases (P0-P2) via `DEEPSEEK_AI_ENGINE_J_PROMPT.txt` ADDENDUM 2.
P3+ need owner decisions after P0-P2 data.

## 1. Problem (FACT, code + live)
- WHEN = the game life clock. Player: ENG_LIFE drained by `2^((rpm-base)/range)` (+ oil term). AI: veh+0x3D70
  rolled once `Normal(3xAvg, 3xVar)/race factor`, ticks 1 s/s (live z-fit 1997 and 2005).
- Onset (`mf_engine_model.h` ~1372): fires when `life / drain_ema < lead_s` for 10 s (lead_s rolled once,
  240-900 s); emergency onset for short horizons; early random failures `MF_EARLY_PER_H` 0.02/h (x drain, x 1/q^2).
- Fault type = `mf_roll_fault` weights from the CURRENT usage snapshot (rev, oil, water, g, cycles) at onset.
- Development after onset (leaks, wear, temperatures, fairness floor) = physical model. This part is good; keep it.

Weaknesses:
1. Fate rolled before the start: once the clock is rolled, failure timing is a pure function of revs; no chance
   enters during the race (except the 0.02/h early roll).
2. One scalar for the whole engine: fault type is picked by a snapshot at onset, not earned by the history.
3. AI clock is constant 1 s/s - AI driving cannot matter to WHEN.
4. Nursing only after onset; before onset only revs below the base matter.
5. No memory across sessions (2005 two-race engine rule not representable).
6. AI terminal visuals: every AI terminal = the game's vanilla engine failure (black smoke) whatever the fault.
   Live 2026-10-01 (2005, dll 69a5ef1a, law mode): Klien fuel-leak onset t=2461, power lever 730 s, terminal
   t=3191 by the old law clock -> vanilla black-smoke blow-up. A fuel leak should not look like that.

## 2. Target design (opinion, Claude)
- Keep the game life as the WEAR BUDGET and calibration anchor (modder Avg/Var keep their mean meaning).
- Damage history: split every second of life consumption across component accumulators by the stressor mix of
  that second (rev -> valvetrain, oil -> bearings/oil system, water -> gasket/cooling, cycles -> fuel/exhaust,
  g -> oil pickup/radiator). Sum of accumulators = life consumed (exact bookkeeping).
- Onset = per-tick chance: with wear fraction `x = consumed / budget`, onset fires in dx with Weibull hazard
  `h(x) = (beta/eta) (x/eta)^(beta-1)`; at x >= 1 - lead*drain/budget the current countdown onset still fires
  (backstop: no engine outlives the vanilla clock without an onset). Early failures stay as today.
- Fault type at onset = drawn from the ACCUMULATED component shares (history), not the snapshot.
- Development, fairness floor, warnings, symptoms: unchanged.
- AI: same code, fed by AI rpm/oil/water (needs J2b/J3); WHEN budget = game timer. AI never reacts (owner rule).
- Calibration rule: expected failures per race per car within +-10 % of the current system (sweep), unless the
  owner chooses an era target.

## 3. Phases
| Phase | Who | What | Behaviour change |
|---|---|---|---|
| P0 | DeepSeek | Sweep metric: failures per race per car (player + AI) for the current system and for vanilla | none |
| P1 | DeepSeek | Component damage accumulators, logged + published | none |
| P2 | DeepSeek | Hazard onset in SHADOW (logs "would fire"), beta/eta table from the sweep | none |
| P3 | Owner decides, DeepSeek | Option "Onset model: countdown / hazard" (default countdown), fault from history | yes, opt-in |
| P4 | Claude A0 live + plan | AI per-fault visuals (fuel leak = fire/vapour or stop, not black smoke) | yes |
| P5 | Owner decides | Engine carry-over across sessions / race weekends (era rule) | yes |

## 4. Risks / open
- Var loses its exact meaning; only the mean is guaranteed (owner accepts or not at P3).
- Randomness felt more -> mitigated by warnings before every non-sudden failure (existing development phase).
- AI hazard depends on AI thermal inputs: P3 for AI waits for J2b + J3.
- Pending validation: P0-P2 numbers (DeepSeek), owner feel test at P3.

## 5. Owner decision 2026-10-01: BUILD IT (engine first)
Owner: "variance built in, working with vehicle-specific values, plug and play, realistic engine component wear as
intended from the first place." Scope now = ENGINE only. Gearbox, hydraulics (?), electronics, suspension and
brakes follow later on the same framework (section 7).

## 6. P3 design (Claude) - executed via DeepSeek ADDENDUM 3
Plug and play = every number comes from the car files the game already loads (engine.ini Lifetime* via H5/J3, the
game's own timer roll); no per-mod tables.
- AI WEAR RATE (new): consumption = game drain law `mf_game_drain(car, rpm, oil)` with the car's own J3 values,
  divided by the FIELD mean of the same law (EMA over all running AI, tau 600 s), clamped 0.25-4.0; 1.0 until the
  field mean is valid (60 s). Field-mean normalisation keeps the vanilla failure COUNT (average car = 1 s/s) while
  a car driven harder / hotter than the field wears faster. Shadow life `ai_life` starts at timer/divisor and drains
  by that rate; this is the AI budget and clock for the hazard and the countdown backstop.
- GAME TIMER OWNERSHIP (AI): the game timer must not kill a car our clock keeps alive: while the slot is live keep
  veh+0x3D70 >= ai_life + 600 s (write only when below; A0 trial: engine timer extend holds, finding
  ai-engine-timer-extend-holds-live). Terminal = our model writes 0.001 (as today). H1c generation rules apply.
- ONSET = hazard (P2 function, x = consumed/budget) OR countdown backstop (unchanged), whichever first; fault type
  = draw from dmg shares (history) for both. Early random + impact paths unchanged.
- Player: same hazard; budget = ENG_LIFE at model start; consumption = the game's own drain (already).
- Variance: vehicle Avg/Var through the game's roll + hazard spread + usage. Calibration target: per-car expected
  engine failures per race within +-10 % of vanilla Phi((T f - 3Avg)/(3Var)) for AI at field-average usage
  (sweep, 1997 + 2005), player within +-10 % of the current model.
- Option `onset_model` (panel later): 1 = hazard (DEFAULT), 0 = countdown only (old behaviour, for A/B).

## 7. Framework for the other systems (later, owner-listed)
Same pattern per system: budget from the car file (or the game's own timer where one exists), consumption from
that system's stressors, damage shares per component, hazard onset, physical development, fair warning.
| System | Vanilla budget source (known) | Status |
|---|---|---|
| Gearbox | AI timer veh+0x3D74 (doc 46), stuck gear 0x3D78 | gear_failure.c exists (player); AI side not built |
| Electronics | none known | electronics_failure.c exists (timer model) |
| Suspension | none known (no AI timer) | suspension_failure.c exists (load breaks) |
| Brakes | none known | not built |
| Hydraulics | not in vanilla as far as known - research first | not built |
Order and scope: owner decides after engine P3 live test.
