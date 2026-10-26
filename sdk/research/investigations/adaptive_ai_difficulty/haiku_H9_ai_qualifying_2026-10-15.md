# H9 - AI behaviour in qualifying vs race, and the simulated-session lap formula (static)

Date: 2026-10-15. Agent: Haiku 5.5 (static, read-only). Owner question: predict AI qualifying lap
times and rate a player from qualifying. No game process touched. Corpus edits: none (this file is
the only write). Tags: FACT = read at the cited line; DERIVED = my inference from FACT lines;
OPEN = not settled statically. Paths relative to `f1c_ida_decompiled\`.

## 0. Bottom line

- Driven AI in sessions 3-5 has NO race-only pace gate, NO race fuel branch and NO random 4 s pace
  walk. Pace is a 10-lap ramp (+0.003 per lap, cap 0.03) on the 0x453C speed scalar, then flat.
- Simulated (skipped-session) lap times use QualifyingAbility 0x44C0 in sessions 3-5 and
  RaceAbility 0x44C4 in race. Each sim lap is a random draw; the sim "best lap" is the minimum over
  TotalRaceLaps draws. Those numbers are not push laps.
- Driven quali order follows RA (0x44F4 pace); simulated quali order follows QA (0x44C0). A rating
  based on QA would not predict driven AI order.
- Several corpus claims need correction (section 3). The pow direction in the sim is still OPEN.

## 1. Session branches (AI-relevant), race 7 vs qualifying 3-5 (non-race family)

Session ladder: 0 TestDay, 1-4 Practice, 5 Qualifying, 7 Race (FACT per QUALI_STATIC.md header).
Code groups 3/4/5 together in several places (see rows marked *). Session 6 (warm-up) is grouped with
practice 0/1/2 in sub_429960 (L171-200); its identity is not re-verified here.

| # | Area | Race (7) | Qualifying 3-5 | Cite (file:line) | Tier |
|---|---|---|---|---|---|
| 1 | Pace adaptation (0x453C walk) | every frame, +/-0.01 walk, 4 s gate | skipped | AIAbility_00429470.c:36,96,109,117,148,176 | FACT |
| 2 | Pace ramp (non-race only) | skipped | +0.003 x min(laps,10), cap 0x44F4+0x44D8 | AIPushLap_0042a180.c:17-31 | FACT |
| 3 | Physics factor 0.995 * AIDifficulty | applied | skipped (see C4) | AIDriverStructInit_00424790.c:129-130 | FACT |
| 4 * | Sim ability input | 0x44C4 | 0x44C0 | sub_4BE760_004be760.c:33-36; RandomVariationScaler_004bea10.c:8-16 | FACT |
| 5 | Fuel | race branch on FuelMultiplier<2 | qual SVM file (fuel ~21-30 vs 100) | AIAbility_00429470.c:176; findings aad-ai-svm-qual-race-2026-10-15 | FACT / DERIVED |
| 6 | Tyre choice | race-only compound branches | plan-row branch; wet bands shared | PitStopDecisionTireCompound_0042a3d0.c:29,45,54,63,68 | FACT |
| 7 | Run plan (stops) | sim-time plan from race | sessions 3-5: 4 stops w.p. 20/21 (see C3) | sub_429960_00429960.c:212-216 | FACT / DERIVED |
| 8 | Run plan (other) | | sessions 0,1,2,6: 3 or 4 stops 50/50, gaps rand%5+3 | sub_429960_00429960.c:177-178 | FACT |
| 9 | Out/in-lap behaviour re-init | unconditional | only if lap <= StateFlagsLookup(plan row) | AIBehaviorInitialization_004bf0a0.c:83 | FACT / DERIVED |
| 10 | Mode reset after off-track | 4 | 5 | AIMainUpdate_00424e70.c:223; AIRacingLine_00425920.c:170 | FACT |
| 11 | Overtake gate | full race overtake | session 5: ServerSessionGateCheck (lap==2 and !call_to_box) | Overtaking_0042af70.c:18-29; ServerSessionGateCheck_0042ae60.c:5 | FACT |
| 12 | Blue-flag yield | race only | off | OpponentProximityScanTeamRecognition_0042a7d0.c:85 | FACT |
| 13 | Collision / damage / failures | race only | off | AICollision_004bef60.c:12; AIVehicleDamageEvent_0041a170.c:14; EngineFailureCheck_004bec55.c:124,149 | FACT |
| 14 | Sorting key | lap count, then finish time | best lap 16540 (= 0x409C) | AIDriverSorting_004bf390.c:45-52 | FACT |
| 15 | Session end | player inactive and 16484 set | player inactive and lap >= TotalRaceLaps-1 | SessionCheckEndCondition_0046d0d0.c:14-23 | FACT / DERIVED |
| 16 | Setup file | race SVM | `.qual` SVM (3-5 map to 5) | AISettingsFileLoader_004333e0.c:58; AISettingsFileResolver_00433890.c:26 | FACT |
| 17 | Commentary/events | race subset | session 5 only (sector, overtake, position) | AICornerSpeedComparison_00419500.c:10; AIOvertakeDefendStateChange_00419690.c:13; AIPositionChangeEvent_004198b0.c:17 | FACT |
| 18 | Unidentified sub_4BCCC0 | | constants 87.0/1.9 (session 5) vs 37.0/0.8 | sub_4BCCC0_004bccc0.c:272-281 | FACT (role OPEN) |
| 19 | Pit timer | 7 branch | <=6 branch | PitTimer_004d2e50.c:23-53 | FACT |

## 2. Simulated sessions: the lap formula as far as the code allows

Trigger. AIOvertake (sub_4BEB10, AIOvertake_004beb10.c):
- L11: brain+860 = dword_8ACF8C = g_TotalRaceLaps (SDK f1c_sdk.h:6573, "high"). So the sim
  always runs TotalRaceLaps laps, also in qualifying.
- L14: brain+864 = session type.
- L16-31: fill flag a3 = 1 when session clock 852 >= 856 (all remaining laps at once). In race it is
  also set when some car has finished (L18-23).
- L33: AIBehaviorInitialization(brain, a3) when brain+834 == 0.

Step 1, base lap B = brain+844 (sub_4BE760_004be760.c, decoded constants 1142292480 = 600.0):
- (a) L20: default B = 600.0 s.
- (b) L64-71: if track entry exists, B = GDB value. Key: session 3/4/5 -> "Qualify Laptime"
  (off_5D2630 -> 0x5D265C); else "Race Laptime" (off_5D2634 -> 0x5D264C) (L66; strings_index
  005d265c / 005d264c). Value decoded by sub_58850F(DataEntryAccessor(...)) - units not decoded (OPEN).
- (c) L73-76: strength term. a4 = dword_7D01A0 / 120 (AIBehaviorInitialization L35: strength 70-120).
  B <- B * (1 + 0.15*(1 - a4)). Stronger AI -> smaller B.
- (d) Live-data branch, L25-42 and L43-60, L80-89, replaces (b)-(c) when two or more cars have
  state 13000 == 1 and best lap 16540 > 1 s:
  F = fastest 16540; A1 = its ability; T2 = slowest 16540 with T2 <= 1.15F; A2 = its ability.
  B = max(0.97F, F - (T2-F)*(1-A1)/max(A1-A2, 0.01)).
  Ability = 17604 if session 7 else 17600 (L33-36, L53-56).

Step 2, one lap draw per car (RandomVariationScaler 0x4BEA10, called from AIBehaviorInitialization L89):
- A = ability float: 17604 if session 7 else 17600 (L8-16). A is the parser-transformed value:
  0x44C0 = 1 - QualifyingAbility/22; 0x44C4 = 0.8464 - 0.038473*RaceAbility (CONFIRMED live,
  findings rcd-offsets-live-confirmed). Higher A = faster lap (sub_4BE760 L80-85 fits T = T1 + k(A1-A)).
- i = rand%40 (L17); x = clamp(0.99 + 0.01*i - A, 0, 1) (L17-26).
- x' = pow step (L27): decompiled as sub_5881F0(v8) with the return stored back. The operand order
  is OPEN: either x' = x (my "as is" reading) or x' = 1.334^x (RANDOMNESS.md section 3 reading).
- j = rand%50 (L28); y = min(1, 0.5 + 0.01*j) (L28-30).
- lap = B + 0.25*B*x'*y - 0.25 (L31). The literal -0.25 is not multiplied by B; it may be a decompiler
  artifact (OPEN, 0.25 s).

Step 3, bookkeeping (AIBehaviorInitialization_004bf0a0.c):
- Each call gives one lap per car (L89, laps 16444 at L93). Cars stop at laps >= brain+860 (L82),
  or at once when a3 is set (L107-111).
- Best lap 16540 = min over laps (L100-103). Cumulative sim time 16536 (L101).
- v14 = (K - v13)*(lap/K) = lap*(1 - v13/K), K = flt_5EA3FC = g_TrackRefSpeed (L69, L94).
  v13 = veh+0x4044 (16452) when veh+0x4094 (16532) > 1, else 0 (L90-92). Dimension check fails
  (metres vs speed in K); field identity OPEN (C6).
- Clock: brain+852 += v23 at the end (L123); gaps in brain+416.. (L127-133).

Simulation (my Python reproduction of the formula, B = 90 s, N = 53 laps, 4000 trials, both readings;
script in %TEMP%\h9\sim.py, not in the corpus). Lap is seconds; A is the ability input.

| reading | A | per-lap median | per-lap p5 | best-of-53 median | best-of-53 min |
|---|---|---|---|---|---|
| x as is | 1.00 | 92.7 | 89.8 | 89.8 | 89.8 |
| x as is | 0.95 | 93.5 | 90.6 | 90.4 | 90.2 |
| x as is | 0.90 | 94.3 | 91.4 | 91.0 | 90.8 |
| x as is | 0.80 | 96.0 | 92.9 | 92.3 | 91.9 |
| 1.334^x | 1.00 | 107.4 | 102.1 | 101.5 | 101.0 |
| 1.334^x | 0.90 | 107.9 | 102.5 | 101.8 | 101.3 |
| 1.334^x | 0.80 | 108.5 | 102.8 | 102.2 | 101.6 |

Reading (DERIVED): "x as is" gives best sim lap about B - 0.25 at A = 1, losing about 2.5 s by A = 0.8.
1.334^x puts every sim lap about 11-13 % above B, which is hard to square with step 1(d). That favours
"x as is", but it is not proof (C7, O1).

## 3. Corrections to the corpus (no edits made; see section 6)

- C1. findings.jsonl:1573 (gdb-laptime-keys-simulated-only-2026-10-14) says sim time =
  base*(1+0.15*(1-abilityNorm)). Code: the 0.15 term uses a4 = strength/120
  (AIBehaviorInitialization L35-36; sub_4BE760 L74-76). Ability only enters step 1(d) and RandomVariationScaler.
- C2. QUALI_STATIC.md section C says the "slowest active car" is picked first and then the fastest
  within 1.15x. Code: the fastest (min 16540) is picked first (L25-42), then the slowest within 1.15x (L43-60).
- C3. QUALI_STATIC.md section E says "1/21 chance of 4 stops else 3". Code:
  `(rand%21 != 0) + 3` (sub_429960 L212) gives 4 stops with probability 20/21.
- C4. findings.jsonl:1582 (aad-strength-scales-ai-physics) and QUALI_STATIC.md say the 0.995 factor is
  race-only. Code skips it only in sessions 3/4/5 (AIDriverStructInit L129). The SDK header names
  flt_5EA3E8 "AIInitTractionControl" (mods/sdk/f1c_sdk.h:5432) while the factor is used for
  ScaleAudioPitch (L131). Value and name not re-verified here (OPEN).
- C5. dword_8ACF8C is g_TotalRaceLaps (f1c_sdk.h:6573), not a qualifying lap count. The quali end test
  (SessionCheckEndCondition L23) and the sim length (AIOvertake L11) both use race length.
- C6. Lap-distance field conflict: findings.jsonl:151 says veh+0x4048; :304 says veh+0x4044 and
  explicitly NOT 0x4048. The sim reads 0x4044 (16452). Which is right is OPEN.
- C7. RANDOMNESS.md section 3 says the pow base is 1.334 (x-87 return stored back). MSVC _CIpow operand
  order is not verified in the corpus (sub_588212 disassembly is not decodable here). OPEN.

## 4. Verdict: which AI qualifying laps are push laps

- Driven AI laps in sessions 3-5 are NOT stochastic push laps. There is no race-style 4 s pace walk
  and no race fuel branch. The only lap-to-lap shape is the 0x453C ramp over the first 10 laps
  (AIPushLap L24-29). Driven quali laps are therefore steady after lap ~10, plus one out-lap/in-lap
  per planned stop (stops at laps 4/9/14/19 for n = 4, from sub_429960 L216-222; spacing 12/n + 2).
  Caveat: AIPushLap call frequency not re-verified (O7).
- Simulated quali laps (player skipped session) are random draws. They are NOT push laps and should
  not be the rating reference. The sim best lap is the minimum of TotalRaceLaps draws and depends on
  QA (0x44C0), the GDB base and the pow reading (O1).
- Rating advice (DERIVED): compare the player's quali lap against driven AI quali laps (16540 of AI
  cars after lap ~10 in a live session), normalised to the same track and fuel state. Do not compare
  against simulated numbers, and do not use QA alone to predict driven order (driven pace uses RA via
  0x44F4). Mark sim-derived AI times in any dataset.
- Live check (owner): compare AI 16540 in a driven quali vs a skipped quali.

## 5. Open items

- O1. pow operand order in RandomVariationScaler L27 (helper sub_5881F0/sub_588212; caller
  0x4BEA9D-0x4BEAAC per RANDOMNESS.md section 3). Decides the sim scale (table above).
- O2. Literal -0.25 in RandomVariationScaler L31: artifact or real?
- O4. v23 (L96-97) is set from FPU compare flags c0/c3 that the decompile cannot show. Which lap sets
  the reference clock 852 is OPEN.
- O5. Field identity of 16452 (0x4044 vs 0x4048) and the dimension of (K - v13)*(lap/K).
- O6. Value of flt_5EA3FC (TrackRefSpeed) and flt_5EA3E8; decode sub_58850F for the GDB key units.
- O7. Call frequency of AIPushLap and of AIBehaviorInitialization per race tick.
- O8. Session 6 identity (warm-up) and sub_4BCCC0 role (constants 87/37, 1.9/0.8).

## 6. Findings to append (NOT written: this task is read-only except this file)

Per AGENTS.md these should go to f1c_ida_decompiled/findings.jsonl once the owner allows one more write.
Proposed entries (tier as shown):
- {"id":"h9-sim-base-strength-term-2026-10-15","tier":"FACT","claim":"sub_4BE760 fallback base B*(1+0.15*(1-a4)), a4 = dword_7D01A0/120 (AIBehaviorInitialization L35-36), not the ability norm","evidence":"functions/sub_4BE760_004be760.c:73-76; functions/AIBehaviorInitialization_004bf0a0.c:35-36","date":"2026-10-15"}
- {"id":"h9-sim-lap-formula-2026-10-15","tier":"DERIVED","claim":"sim lap = B + 0.25*B*clamp(0.99+0.01*(rand%40)-A,0,1)*min(1,0.5+0.01*(rand%50)) - 0.25; A = 0x44C0 (sessions 3-5) or 0x44C4 (race); best = min over TotalRaceLaps (dword_8ACF8C) draws","evidence":"functions/RandomVariationScaler_004bea10.c:8-31; AIOvertake_004beb10.c:11","date":"2026-10-15"}
- {"id":"h9-sched-stops-quali-inverted-2026-10-15","tier":"FACT","claim":"sessions 3-5 plan: 4 stops w.p. 20/21, 3 w.p. 1/21 (corrects QUALI_STATIC section E)","evidence":"functions/sub_429960_00429960.c:212","date":"2026-10-15"}
- {"id":"h9-open-pow-operand-order-2026-10-15","tier":"OPEN","claim":"Whether RandomVariationScaler computes x^1.334 or 1.334^x; decides sim scale (about +11 % vs about 0 %)","evidence":"functions/RandomVariationScaler_004bea10.c:27; investigations/ai_brain_map/phase3/RANDOMNESS.md section 3","date":"2026-10-15"}

## 7. Limits, cold start, documentation gate

- Cold start partial: AGENTS.md, root README (head), PRODUCT_STATUS.json (head), component
  README_2026-10-14.md read. governance/PIPELINE.md and governance/README.md NOT fully read (gap).
- Structure validator: 2 pre-existing errors (top-level `.claude`, `plans`). Not touched.
- Nothing in corpus, game or governance changed. Scratch script: %TEMP%\h9\sim.py (python -I).
- Pending owner write: index line in investigations/adaptive_ai_difficulty/README_2026-10-14.md.
- Static only; pow, -0.25 and sub_58850F units unresolved.
