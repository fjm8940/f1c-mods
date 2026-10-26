# Prior-art map for the AI brain open items (Claude, 2026-10-02)

Owner request: "query the whole F1/ for matters which are open currently; they might have been discovered and
documented before". Method: `rg` over F1/ (md/json/jsonl/txt/c/py; excluded workspace_organization_audit, archive,
_human_review, game copies, third_party, decompiled `functions/`, skeletons, nt2004/ghidra corpora, ai_brain_map
itself) per topic, plus `f1c_mod.py evidence-query findings`. Scan script: `%TEMP%\prior_art_scan.py` (disposable).

**Main result:** `investigations/ai_interaction_role/README.md` (6664 lines, 410 dated sections, almost all
2026-08-18, static Hex-Rays + instruction reads) already answers most of the phase-3 static questions. It was not
linked from doc 54 or the ai_brain_map handoffs, so phase 1/2 re-derived parts of it. Below, `IR Lnnnn` = line of the
section heading in that README. Tier of the IR material: static (DERIVED); live checks are ours.

## 1. Open item -> prior art -> what is still open

| Open item (handoff section 4 / D-task) | Prior art | Still open |
|---|---|---|
| D4 pace 0x453C dynamics | IR L1828, L1875, L2409, L3153, L6396, L5981; `mods/sdk_sandbox/ai_mind_monitor/AI_RCD_AND_PASSING_NOTES.md`. AIAbility runs every **4 s** (deadline 0x4404 = now + 4). Race: 0x4508 == 0 -> step 0.01 down toward 0x44F4 (baseline); else 0.01 up toward 0x44F4 + 0x44D8. If 0x450C != 0 (max opponent race ability seen in traffic) a random gate `(0x44D4 - 0x450C*0.5)*100 vs rand%100` moves up toward 0x44F4+0x44D8 or down toward 0x44F4*0.5. One state sets 0.85*0x44F4. Baseline reset 0x453C = 0x44F4 (LateralForceTorqueCalculation(.,100)). Non-race: AIPushLap ramps +0.003*laps. | Live check only: is it rubber-banding vs the player? Static says it is traffic-driven, not gap-driven. D4 becomes a live verification of this model. |
| D4 sources of 0x4500/0x4504/0x4580 | IR L1548, L2080, L6024, L1640. 0x4500 = .rcd CORNERINGADD, 0x4504 = CORNERINGMULT (direct). 0x4580 = wet factor `max(0.5, 1 - wetness * 0x44C8)` (AIPushLap), 0x44C8 = `.01 * RAINABILITY * flt_5EA3D8`. Full .rcd token table in IR L1548. | Wetness global flt_8AFEE8 identity (live). |
| D7 start reaction | IR L1705, L5059, L5420, L5452, L6079, L2223. Start path in AIMainUpdate bypasses planner, writes outputs directly, clears 0x45FC. Ramp tables 0x5C30E0 / 0x5C3144 = the SAME 26-value ramp 0.0..0.949 (forward/reverse). 0x44B8 = STARTSDRY/STARTSWET blend by wetness + random, `*0.1 + 1.0`. STARTSTALLS 0x44BC: `value*100 vs rand%10000` stall chance; RACEEVENTS (ENGINE/BRAKES/GEARBOX/SUSPENSION/STALL/STALLDNS) scripted failures. | Live launch timing per driver vs STARTSDRY/WET (race_start logs). |
| D5 randomness / mistakes | IR L2393 (0x4448 = .15 + .15*randTable - .1*difficulty at phase change), L1828 (random pace gate), L2223 (stall / race events), L6327 + L5866 (CRASH / COMPLETEDLAPS random incident gates in AICollision 0x4BEF60 -> event code 6). | A complete list of RandLCG uses + whether any random per-corner error exists (none recovered so far). |
| D6 overtake / defend modes | IR L1920, L2097, L2303, L3281, L5041, L3603. AISteering calls Overtaking only for opponent modes {2,7} and {1,8}; accepted -> 0x45FC = candidate, 0x4420 = +3 (mode 2) / -3 (1,7,8). Race gate: candidate lap > own lap, or same-team peer 0x45F8 fallback. Sign = side is NOT proven. | Meaning of modes 4/5/16 (the dominant ones); live context of each mode (D6 as planned). |
| Blue flag / yield 0x45FC | IR L100-150 and L6646. Auto-yield in ProximityScan 0x42A7D0 only when the lapping car is the PLAYER, dist^2 < 625, race, strictly more laps; lateral +-2.5. **Correction to Claude's chat answer 2026-10-02:** when 0x45FC is set, AISteeringCorrection caps throttle 0x457C = AIDifficultyScalar * 0.75 (IR L6593), so the yielding car does lift; it does not brake. | Live: never seen. |
| Modes 0x4510 | IR L1712, L1736, L1747, L2345, L6107-6130. Mode 1: brake 1 / throttle 0, -> 2 when speed < 4 or both slip > 0.9. **Mode 2 = one-tick state**: FieldZeroSetter only writes 0x4510 = 0 and skips that tick's commit (outputs keep last value). Mode 3: route-distance check vs 0x461C, back to 0. Mode 7: 0x4424 = 0, brake 1, throttle 0, -> 0 below 0.2 m/s. Mode 6/7 = reverse recovery. | Doc 54 table says mode 2 = "stopped/handed over" -> should be "one-tick reset after mode 1". AI ticks every 25 ms (IR L5398 / L1933); the 20 Hz watcher can miss one-tick mode 2 entirely, which explains "mode 2 never seen". |
| D8 pit strategy | IR L1640, L3558, L4874, L5965, L6234, L6364, L1490; doc 53. sub_429960 builds the stop schedule; AIAbility (4 s) -> PitStopDecisionTireCompound / RacePhasePitEntryTrigger -> ForcePitExit; tyre-average < 0.9 forces a stop; 0x45F8 = same-team peer, fuel gate for double-stacking. | Link table to brain fields (D8 as planned) - now mostly a citation exercise. |
| D9 traction control / late longitudinal | IR L6593, L4912, L6260. AISteeringCorrection is the final longitudinal authority: steer attenuation, stability/thermal/speed corrections, traffic cap 0.75*difficulty, MaxSpeed 0x4538 clamp. | Live frequency of the slip cut (D9 as planned). |
| D10 writers of 0x4408 / 0x44B4 | IR L1724, L2357 only describe the reader (`/ 0x44B4`). No writer recorded anywhere in F1/. | Still open: D10 capstone scan is the right next step. |
| 0x45F8 (GUESS in registry) | IR L1490: same-TEAM peer vehicle pointer (+0x46B8 Team string match), set by AIDriverStructInit; fuel/pit and Overtaking fallback consumer. | Promote meaning in D2. |
| 0x44CC naming | IR L1548/L1589/L5906: 0x44CC = .rcd PASSING normalized `min(1, 0.9 + 0.001*Passing)`; it is the per-driver factor in the braking projection. AI_RCD_AND_PASSING_NOTES: 18 distinct live values 0.971-1.000. | Registry name `brake_decel_factor` is functionally right but hides the .rcd source; D2 should add source "PASSING". |
| 0x44C4 | IR L1589: RACEABILITY normalized = normal brake-ramp source (0.2*0x44C4 per tick). | Registry meaning fine; add .rcd source in D2. |
| Damage reactions | Vanilla: none found. Mod-side only: `development/mods/mechanical_failures/docs/AI_SYMPTOMS_AND_REACTIONS_SPEC_2026-10-01.md`, `AI_REACTIONS_RUN_2026-10-02.md`. | Vanilla AI damage reaction is unstudied. |
| Write readiness | IR L3694, L4124, L4780, L5580, L5722, L5891 ("writable-contract gate remains open"); mechanical_failures live writes (findings ai-lateral-request-4420-zeroed-each-tick, ai-pulloff-lateral-chain-v4, ai-yield-target-self-pointer-crash). | Unchanged: no general AI write contract; write stage 1 still the plan. |

Other indexes that cover the same ground (lower value, mostly generated): `analysis/semantic_memory_index.json`,
`analysis/recovery_index.json`, `mods/sdk_sandbox/multiplayer_ai_host/generated/memory_recovery_index.json`,
`shadow_source/` architecture packets (LLM reconstructions, below DERIVED).

## 2. New live observation (this session)

Wet factor 0x4580 (`corner_speed_scale`): wet Imola `watch_20261002T194606Z` lap 2 = **0.9754** for all cars;
dry Monza `watch_race4fulld` = **1.0**. 0x44C8 = 0.0307 (wet run), so implied wetness = (1 - 0.9754) / 0.0307
= **0.80**. Matches the static formula `max(0.5, 1 - wetness * 0x44C8)` (IR L6024). Consequence (inference): with
default-scale RAINABILITY the rain cuts the AI corner target only ~2.5 % through this factor; any further wet
slowdown must come from TireForceSolver tyre tables (D11 will show). OBSERVED, one sample per car, one race.

## 3. Actions taken

- DeepSeek handoff phase 3: D4-D8 now start from this map (verify live, do not re-derive).
- Claude handoff section 4 points here.
- Finding `ai-brain-wet-factor-4580-live-2026-10-02` appended.
- Doc 54 mode-2 wording: to fix at the D2 merge (Claude), with IR L1747 as source.
