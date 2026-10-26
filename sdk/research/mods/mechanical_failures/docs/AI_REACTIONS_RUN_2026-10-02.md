# AI symptoms + AI driver reactions - ADDENDUM 14 (B1-B9) run record (2026-10-02)

Status: IMPLEMENTED + OFFLINE VERIFIED, NOT INSTALLED (dev dll `6CCBA4A2`). B8 calibration gates
partially met - owner decision needed (section 6). Pipeline stage: implement/verify.
Spec: `docs/AI_SYMPTOMS_AND_REACTIONS_SPEC_2026-10-01.md` (Claude, 2026-10-01 g).
Work order: `docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt` ADDENDUM 14 (supersedes ADDENDUM 13 where they conflict).

Start state: installed dll `D7F23A4E`; sources `engine_failure.c CD68E58C`, `mf_engine_model.h E8433FBA`,
`engine_model_test.c 355F91B3`. Owner direction: every player symptom happens to the AI too (incl. BLUE
smoke), AI drivers react (rev-cap triple just under the perceived fault limit), AI damage physics live
(valve float guard removed), mistakes can blow engines; fairness gate D1 = A (AI mean engine
retirements/race = vanilla +-10 %, 20-40 % of faults nursed to the flag replaces q6 exactness on the AI side).

Deviation (recorded per `governance/PIPELINE.md`): B1-B7 were implemented in one continuous edit
session; only ONE before-copy (`workspace_organization_audit/ENGINE_SIM_BEFORE_2026-10-02_B1/`, run-start
sources) was preserved. Intermediate per-batch before-states were not separately captured, so the per-item
hashes below identify the end state, not each edit step.

## 1. Per-item result (files = end-state SHA-256 first 8)

| Item | Result | Evidence |
|---|---|---|
| B1 header | DONE - new pure header `dll/mf_ai_react.h` (`3B90005C`): AiTraits/AiSymptom/AiReaction, `mf_ai_present`, `mf_ai_react`, level masks/mixes (spec 4.x), perception (3.2), mistakes (3.3), notice (3.4), caps (5), restore bookkeeping. No game access. | compiles `-Wall -Wextra -Werror`; B7 tests |
| B2 model | DONE - AI valve-float damage guard removed; live AI physics behind `MfInputs.ai_reaction` (fairness override off) + `MfInputs.ai_nursed` (Q7a cap/hard terminal and the culmination block skipped for a nursed slot). `mf_engine_model.h` `E5E74966`; test `39D6E627`. | model test ALL PASS 67577 (`test_ai_float_damage_live`, `test_ai_nursed_b6`) |
| B3 levers | DONE - rev-cap TRIPLE writer (limit `0x39A4` + upshift `0x3A18` + downshift array `0x3A1C..0x3A38`, idle-adjacent slots unscaled), power shape + fuel-pressure hard top cut + misfire pulse windows in the ONE table writer, bank owner, flame spit, maxSpeed terminal limp, PassingNorm `0x44CC` / TrackAggression `0x44F8`; every write ownership-saved and restored (levers snapshot `MfAiLeverSave`, `ai_levers_restore` on fault end/retire/session/unload). `engine_failure.c` `078742A4`. | build x2; static gate (section 4) |
| B4 blue + sound | DONE - `PBurst.slot`; `emit_bluesmoke_now(n, rate, slot)` uses `VEH_BASE + slot x 0x48C0 + 0x34E0/E4/E8` on the game thread (0x447B10 drain); AI blue accumulator (budget <= 2x player rate) for oil leak / blow-by; 3D sounds via `mf_audio_play_pos` at the AI car (steam hiss, game backfire pops, blow bang). | build; harness |
| B5 reaction wiring | DONE - traits rolled once per slot (RaceAbility `0x44C4` / Crash `0x44D0`, normalised, N(0,0.15)), notice/escalation/mistakes per step, logs `AI REACT:`, `AI MISTAKE:`, `AI PIT:` (R3 pit is logged only - callToBox/repair is an open item). Battle gap and race-fraction context pass none/1.0 (fields unproven). | build; B7 determinism/rate tests |
| B6 endings | DONE - model terminal = Option A (power column 0 + idle `0x39A8` 0 + ReasonCode `0x4068` 1, no vanilla smoke), fault banks per spec 4 (fireball ONLY for blow-up/rear break), `AI END:` log; R4 retire = the same Option A without fire. Shared torque table: power zero is skipped, logged. | build; live lever evidence in plan section 8 |
| B7 tests | DONE - `tools/ai_react_test.c` (`7F52D978`) + `tools/run_ai_react_test.bat`: symptoms per fault, restore flags, determinism, escalation monotonic/no sub-R1, caps bounded, mistake rate within +-20 % of lambda over 8e6 s, every fault x level reachable (masks exact). ALL PASS 40. | gate |
| B8 sweep | DONE CODE, CALIBRATION GATES NOT ALL MET - `sweep.c b8` (`368CE6E6`) + `b8_cal.py` (`A768F439`). Tables in section 3. | 37 s per 22 engines x 200 seeds |
| B9 gates | DONE except live ones - section 4. | - |

## 2. Start/end hashes

- Installed at start: `D7F23A4E` (untouched; no install in this run).
- Dev dll after the run: `6CCBA4A2` (build x2 identical).
- Sources: `engine_failure.c 078742A4`, `mf_engine_model.h E5E74966`, `mf_ai_react.h 3B90005C`,
  `engine_model_test.c 39D6E627`, `ai_react_test.c 7F52D978`, `sweep.c 368CE6E6`, `b8_cal.py A768F439`.

## 3. B8 field table (22 engines, T = 5400 s, 200 seeds/car, realized vanilla reference)

Run A (first build, Q7a skip at R1+), scratch `%TEMP%/mf_b8` (disposable):

| Gate | Result |
|---|---|
| season 1997 | ours 0.0600 vs realized-van 0.0700 (-14.3 %) FAIL |
| season 2005 | ours 0.4400 vs realized-van 0.5550 (-20.7 %) FAIL |
| per-team | 0 fails (2SE) PASS |
| 2005 Spearman | 0.947 PASS |
| nurse share | 24.8 % (target 20-40 %) PASS |
| missed vanilla / ours-not-vanilla | 20.8 % / 1.0 % |

Run B (shipped defaults: Q7a skip only at R2+; an R1 nurse keeps the exact vanilla WHEN):

| Gate | Result |
|---|---|
| season 1997 | ours 0.0650 vs 0.0700 (-7.1 %) PASS |
| season 2005 | ours 0.4650 vs 0.5550 (-16.2 %) FAIL |
| per-team | 0 fails PASS |
| 2005 Spearman | 0.960 PASS |
| nurse share | 20.3 % PASS |
| missed / ours-not-vanilla | 16.0 % / 0.9 % |
| RaceAbility terciles (nursed) | 16.7 % / 19.1 % / 24.0 % - better drivers save more |

Control experiment (spec-authorized lever): `MF_AIR_MISTAKE_RATE` x5 (1/600 -> 1/120) in a scratch copy
only: terms 106 -> 105, seasons unchanged (-14.3 % / -16.2 %), nurse 21.1 %. Conclusion: mistakes do NOT
convert nursed saves into retirements, so the spec's named calibration knobs (base_mistake, margins, R4
weights) cannot restore the mean once nursing can save engines. The structural lever is the fraction of
faults allowed to hand the clock to the physics (the Q7a skip): R1+ = -29 % total, R2+ = -12 % total.
Root cause: at rate 1.0 every onset is on a car whose vanilla timer is already inside the lead window, so
there are no extra cars to fail; a nurse can only remove failures.

## 4. B9 gates

| Gate | Result |
|---|---|
| Model test (`run_engine_model_test.bat`) | ALL PASS 67577 |
| Msg test (`run_msg_test.bat`) | ALL PASS 57 |
| AI reaction test (`run_ai_react_test.bat`) | ALL PASS 40 |
| Normal car sweep (player rows) | out.csv SHA-256 `67E70EC2` = byte-identical to the stored `%TEMP%/mf_sweep_T3` baseline (145 cars, all profiles) |
| grade_test | ALL PASS 306 |
| GUI unittest | Ran 20 tests OK |
| Panel smoke | PASS (pages incl. AiPage) |
| Workspace validator | PASS 0 errors / 0 warnings |
| Build x2 | `6CCBA4A2` = `6CCBA4A2` |
| q6 variant 1 exactness | RETIRED for the AI side by owner D1 = A; the player rows are unchanged and B8 reports the new gate |
| AI pump <= 6 ms | NOT MEASURED (no game in this run) - live check |
| Static rev-cap triple check | `AI_OFF_REVLIM`/`AI_OFF_REV_UPSHIFT`/`AI_OFF_DOWNSHIFT` are written and restored in `ai_rev_apply`/`ai_rev_restore` only (no single-field writer) |

## 5. Open items (spec section 10 + this run)

- R3 pit call is logged only (`AI PIT:`); `callToBox` + pit clear/repair + `PitStopEndTime` extend are
  UNPROVEN and deliberately not wired.
- Battle gap / race-fraction reaction context need proven fields (currently none / 1.0).
- AI blue smoke and sounds are spawned only while the player physics detour exists (0x447B10); a
  spectate/no-player session is unverified for AI effects.
- Exhaust rear-suspension break (4.9 R4) has no AI lever; the layer treats it as an ending symptom only.
- R3q-style re-assert of banks after the game zeroes them is player-only; AI burns stop when the game
  zeroes the banks.
- Driver name read from `veh+0x4620` is best-effort (printable, bounded); logged as `?` on failure.
- AI pump timing, live lever behaviour and the owner live checklist below are pending.

## 6. Owner decisions needed

1. B8 mean: the AI field now retires ~12 % fewer engines than vanilla (2005 -16.2 %) because R2 nursing
   genuinely saves cars. Options: (a) accept (fewer AI failures, D1 target missed); (b) raise the AI onset
   lead (an AI-only model constant - more time for a nursed fault to still kill the car); (c) restrict the
   Q7a clock hand-over to a narrower state. The spec's named knobs cannot close the gap (evidence in 3).
2. Death radio (HANDOFF 2026-10-01 g action 3) is still open; the game's own "he's blown the engine"
   plays for Option A endings.

## 7. Restoration information

- Before-copy: `workspace_organization_audit/ENGINE_SIM_BEFORE_2026-10-02_B1/` (`engine_failure.c CD68E58C`,
  `mf_engine_model.h E8433FBA`, `engine_model_test.c 355F91B3`). New file `mf_ai_react.h` has no before state.
- Rollback = restore those three files (and delete the new header/tool/test files, owner decision) and
  rebuild; the installed game copy `D7F23A4E` was never touched.
- Per-run sweep outputs are in `%TEMP%/mf_b8` (disposable).

## 8. Owner live checklist (spec section 9; run on the next installed dev build)

1. Valve air AI: shifts earlier, rare float misses, occasional blow-up after a mistake.
2. Oil leak + blow-by AI: BLUE smoke growing.
3. Water leak AI: white steam growing, white end, no fire.
4. Fuel pressure AI: stutters, top cut, early shifts, no gear hunting.
5. Exhaust AI: flame spits on lift + pops.
6. Nursing AI lets you by (R2), retirement via garage (R4 Option A).
7. Field race: retirements per race feel like vanilla; check `AI REACT:`, `AI MISTAKE:`, `AI END:` lines
   and the AI pump ms in the log.

## 9. Wasted-effort check

- Repeated reading: the AI pump section and the model terminal paths were read once per edit area; no
  large material was re-read needlessly.
- Broad builds: full model test re-run after every model edit (required); the sweep was compiled many
  times in the scratch dir (needed for each B8 calibration iteration).
- Failed/retried commands: three compile errors fixed in one cycle (AI_VEH_STRIDE ordering, MfOut field
  names, mf_clampd typo) - could have been avoided by checking the header order first.
- Polling/idle: the PowerShell `>` redirection produced a UTF-16 file that cost one hash comparison cycle
  (stored baseline uses ASCII `cmd` redirection); noted for future sweep hashes.

## 10. Claude review (2026-10-02)

- Hashes re-checked (SHA-256 first 8): mf_ai_react.h 3B90005C, engine_failure.c 078742A4, dev dll 6CCBA4A2 = report.
- Gates re-run by Claude: ai_react_test ALL PASS 40, model test ALL PASS 67577, msg test ALL PASS 57.
- Code read: `ai_rev_apply` / `ai_rev_restore` write the triple together, scale from the snapshot (not the capped
  live value), idle-adjacent downshift slots unscaled, ownership-checked restore (drops if the game moved the limit).
  Option A ending saves the power column before zeroing, skips shared tables. AI blue = pq_push with slot, <= 2 bursts
  per pump. No writes to throttle 0x357C / brake 0x3584 / steering / pace / position found. Pump frozen on pause.
- B8 root cause accepted (FACT in the sweep: mistake rate x5 left the count unchanged). Note: the original reaction plan
  (AI_FAILURE_REACTION_PLAN section 1) already foresaw the fix: raise the AI ONSET RATE so that
  faults started x (1 - saved share) = vanilla. Recommended option (d) for the owner: extra AI onsets on cars the
  vanilla clock would NOT kill, rate factor ~ 1 / (1 - nursed share) (~1.15-1.2), WHO weighted by grade/q
  (mean-preserving), calibrated in the B8 sweep to +-10 %. Options a-c in section 6 do not restore the count with
  realistic behaviour ((b) only makes saves harder, (c) removes the nursing benefit).
- Not verified: AI pump ms, all live lever behaviour (no game run) - owner live checklist section 8.

### 10.1 Claude review - hopeless-limp gap (2026-10-02, code read mf_ai_react.h 3B90005C)
FACT (code): no safeguard against a car limping a whole race far behind.
- `mf_ai_level_mask`: R4 never allowed for valve air (spec 4.1 said so - Claude spec error), fuel pressure, radiator
  debris, fuel leak; exhaust only after the rear break. `race_left_frac` is ignored (`(void)race_left_frac`) and the
  caller feeds 1.0 (race-fraction context not wired, section 5).
- Levels are re-picked only on a clear worsening; a plateaued fault (valve floor, stabilised leak) never re-decides.
- R3 pit is log-only, so pit choices behave as R2 limp.
Consequence: a lap-10 valve fault can produce a car nursing at 85 % rev cap for the rest of the race.
Owner direction (2026-10-02): realistic = a driver with a developing fault rarely tries to finish; continuing is
for survivable faults with something to gain (M. Schumacher Brazil 2000: leading, late, managed oil pressure).
Fix goes into ADDENDUM 15 (continue-or-retire decision re-evaluated every lap; see the chat proposal).
