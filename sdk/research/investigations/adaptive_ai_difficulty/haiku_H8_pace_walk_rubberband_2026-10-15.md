# H8: AI pace walk (veh+0x453C) and rubber-banding, static audit

Date: 2026-10-15. Agent: Haiku H8 (read-only static review, no game process touched).
Question: does native F1C adapt AI pace during a session (rubber-banding, catch-up, gap or position to the player)?
Tiers: FACT = read in decompiled source or prior verified corpus; DERIVED = inferred from FACT; OPEN = not resolved.

## 1. Verdict

- **Native rubber-banding: NO** (FACT for the pace walk; DERIVED for the wider AI). The 453C walk reads no player gap, race position, laps remaining, damage, tyre wear, fuel, mistakes or blue flags. Live evidence agrees: `investigations/ai_brain_map/phase3/PACE_MULTIPLIER.md` section 4 gives r about 0.1 for gap and 0.18 for position (Monza), and concludes "no rubber-band" (findings.jsonl:772, OBSERVED).
- **Partial, indirect player dependence (DERIVED):** (a) the traffic accumulator 0x4508 counts any roster car near the AI, and the player is a roster car; (b) the random gate 0x450C takes the max of the 0x44C4 ability norms of cars the AI ran Overtaking() against. Both change the pace target only by traffic, not by player identity.
- **Player-specific code exists but is lateral, not speed (FACT):** when the player is within 25 m and a lap ahead, the AI sets its lateral target 0x4360 and a player pointer 0x4479 (OpponentProximityScanTeamRecognition_0042a7d0.c:83-96).
- **Correction to the premise:** the walk is NOT per frame. AIAbility runs once every 4 s of session time (AIMainUpdate_00424e70.c:213-216, FACT). The "+/-0.01 per frame" claim in CLAUDE.md:171-172 and findings.jsonl:28 (`0x453C-recalc`, DERIVED) is wrong on cadence. The 1 Hz replay found a 4.1 s median interval (PACE_MULTIPLIER.md section 2).

## 2. The writer: AIAbility (0x429470)

Source: `functions/AIAbility_00429470.c`. Only race (dword_7C56A4 == 7) reaches the walk. Other sessions use AIPushLap (section 6).

Walk, per call (FACT):

| Step | Lines | Rule |
|---|---|---|
| Traffic gate | L38, L46 | `this[4418]` (0x4508) == 0 selects branch 1; otherwise branch 2. The test is binary: only zero or nonzero matters. |
| Branch 1 (no traffic) | L40-42 | target = max(cur - 0.01, 0x44F4). Decays 0.01 per call toward baseline. If baseline is above cur - 0.01, it snaps up to baseline in one call. |
| Branch 2 (traffic) | L46-49 | target = min(0x44F4 + 0x44D8, cur + 0.01). Ramps up 0.01 per call to the ceiling. |
| Random gate | L52-69 | only if 0x450C (this[4419]) != 0. Gate `(0x44D4 - 0x450C*0.5)*100 >= RandLCG % 100` (L54). Up: min(ceiling, v3 + 0.01) (L56-60). Down: max(0x44F4*0.5, v3 - 0.01) (L63-66). Uses LCG directly, not RandomVariationScaler. |
| Accumulator reset | L70-71 | 0x4508 and 0x450C cleared each call. |
| Car-condition override | L80-93 | if carConditions 0x3D6C (this[3931]) == 1 and this[3681] > 0.001 and this[4096] <= 0, pace = 0x44F4 * 0.85 (L92). Replaces the 0.85 "disqualification" in the header comment (AIAbility_00429470.c:1); the code keys on carConditions. |
| Final-lap / pit gates | L96-103, L109-115 | last lap and speed >= 5 skip the pit branch; ConditionalGate and pit-exit logic. No pace write. |
| Fuel | L176-185 | ForcePitExit only (no pace write). |

Cadence: `AIMainUpdate_00424e70.c:213-216`: if session time 0x4480 > deadline 0x4404, deadline = now + 4.0 and AIAbility runs. (FACT)

Ceiling and baseline (FACT, from `functions/sub_424080_00424080.c`):
- 0x44F4 (baseline) = (1 - RA/100) * GDS, L151-152. Writers: this function only (L60 default 1.0, L152).
- 0x44D8 (push) = (1 - 0x44F4) * 0.67, L153. Depends on GDS, so it depends on the slider and track, not only on RA. Monza fit: 0.0268 + 0.006432*RA.
- 0x44D4 = (22 - 0.67*RA)/22 (L154), slider-independent.
- 0x44C4 (ability norm, read by Overtaking) = (22 - RA)/22 (L155-156), times (slider/100)^2 if slider < 100 (L157-158).
- L159 calls LateralForceTorqueCalculation(this, 100): 0x453C = 1.0 * 0x44F4 (baseline reset).

## 3. Inputs to the walk target and the gates

| Input | Field | Source | Per car or player | Changes during race? | Mod must |
|---|---|---|---|---|---|
| Baseline | 0x44F4 | RA and GDS (sub_424080 L151-152) | per car, init only | no (init-time) | hold, or change deliberately (see section 7) |
| Push ceiling | 0x44D8 | GDS and RA (L153) | per car, init only | no | hold |
| Slider | dword_7D01A0 | player file slider (AIDifficultyScalar_00424750.c:3-6) | global | no | hold |
| GDS | flt_5EA3D8 | slider and track anchors | global, init | no | hold |
| Traffic | 0x4508 | AISteering increments (section 4) | per car; includes any roster car in the window | yes, every frame | account for: player near AI feeds it |
| Random gate | 0x450C | max of own and opponent 0x44C4 in Overtaking (Overtaking_0042af70.c:13-17) | per car; includes opponents the AI compared | yes | account for: player's 0x44C4 can feed it |
| RNG | RandLCG global | AIAbility L54 | global sequence | yes | note: shared sequence, any mod call shifts rolls |
| Car condition | 0x3D6C (carConditions) | not traced to writer | per car | yes | OPEN (writer not traced) |
| Unknown gate | 0x3984 (this[3681]) | not traced | per car | yes | OPEN |
| Wetness | flt_8AFEE8 | global | global | per track | hold (feeds 0x4580, not the walk) |

Checked and NOT inputs to the walk (grep of AIAbility L26-190 and the callers listed):
- Race position, gap to leader, laps remaining (except final-lap gate at L96-99): no read in the walk.
- Fuel: only ForcePitExit (L176-185).
- Tyre wear: physics only (TireForceSolver in AIComputePassScore, corner speed), not the walk.
- TrackAggression 0x44F8 (this[4414]): not read in AIAbility. Used only in AISteering follow-gap (map_AISteering_part3.md L22, L58, L68; AISteering_004270a0.c:608).
- Blue flags: HUD only (HUDFlagRendererMain_004d82d0.c:1); no AI pace read found.
- RandomVariationScaler 0x4BEA10: called only from EngineFailureCheck_004bec55.c:37-38 (mechanical-failure threshold) and AIBehaviorInitialization_004bf0a0.c:89. Not in the walk.
- Session time: cadence only (4 s).

## 4. Traffic accumulator 0x4508 (who writes it)

All writes are increments by 1.0 (FACT), inside AISteering_004270a0.c (per frame; the AISteering loop runs over the 5 ranked slots, L441-449):
- L634-639: opponent longitudinal gap v33[4192] in (-3.5, 0), not "v142". Also sets this[17785]=1.
- L668-672: opponent gap in [0, 4).
- L1135, L1204, L1248: later branches keyed on v86 (AI state 1, 2, 7, 8). Conditions not fully traced (OPEN).

Because AIAbility tests only zero vs nonzero, the traffic channel means: "a ranked opponent was within the window at least once in the last 4 s". Magnitude does not matter.

Who is ranked (FACT/DERIVED): OpponentProximityScanTeamRecognition_0042a7d0.c:67-70 loops over every entry in dword_7C54F0 (count dword_5C329C), skipping self (L70). Ranking keeps the 5 nearest (L99-107). The player is a roster entry (dword_7C54E4 is the player index, findings.jsonl:1361, FACT), so the player can be ranked (DERIVED). Possible exclusion: the 0x4550 filter at L77 (`v9 < dword_5EA2CC`, OPEN; docs/12 names 5EA2CC as an overtake-distance threshold).

## 5. Random gate 0x450C

Written only by Overtaking_0042af70.c:13-17 (FACT): `this[4419] = max(this[4419], a2[4401])`, where a2 is the opponent and 4401 is 0x44C4. Cleared by AIAbility (L71). Overtaking is called from AISteering only in states v86 1, 2, 7, 8 (AISteering_004270a0.c:1202-1251, FACT). So 0x450C = max ability norm among opponents compared this 4 s window (DERIVED). If the player is one of those opponents, the player's own RA norm enters the gate. This is a rating dependence, not a player-gap dependence.

## 6. Other pace writers (checked)

- AIDriverStructInit_00424790.c:55: this[4431] = 1.0 (unconditional when called). Caller chain: SceneRenderUpdate_004c6d90.c:19. Callers of SceneRenderUpdate(0) in the AI path:
  - AICommandMessageQueueDispatcher_00424cd0.c:34 (command 15, pit exit; race branch when this[16484] != 0; non-race when dword_7C56A8 == 8).
  - PitStopDecisionTireCompound_0042a3d0.c:66 (when PitStopBlockingCondition or non-race).
  - sub_4A00A0_004a00a0.c:40, sub_4A0250_004a0250.c:44 (not traced).
  - Result: a pit event can reset 0x453C to 1.0 (DERIVED, not seen as a live cluster in PACE_MULTIPLIER.md; the 2 external 1.0 events there were stopped cars, OPEN).
- LateralForceTorqueCalculation (AIDriverRandomSelector_0042ae20.c:11 when this[17916] == a2; sub_424080 L159): baseline reset. AIDriverRandomSelector is called from sub_4C6E80_004c6e80.c:14 on session/restart paths (DynMan_Post_0042c9f0.c:8, ProcessDriverEntries_004b8da0.c:123,135), not per frame (DERIVED).
- AIPushLap_0042a180.c:17-31: non-race only (dword_7C56A4 != 7). Pace += min(laps * 0.003, ceiling). Laps = this[4111]. Not race-relevant.
- AIPushLap L33-37: wetness -> 0x4580 corner scale (per car rain ability times global wetness).
- sub_42ACD0 (ParameterValueSetter_0042acd0.c:1-6): NOT a continuous governor. It writes 0x4538 (max speed) from a discrete command (18 = pit speed flt_8ACFB4; otherwise a2). Callers: AICommandMessageQueueDispatcher_00424cd0.c:38, CommandQueueResponseHandler_00429ee0.c:14-37 (values 18 and 99999), AIAbility L139, AISteeringRacingLineCorrection_0042b0e0.c:139. No gap or player input (FACT).
- AISteering per-opponent speed/gap target v156 (AISteering_004270a0.c:500-507) uses the opponent's relative speed and gap. It is a per-frame follow and braking channel, NOT a pace multiplier. It applies to any ranked car including the player (DERIVED). Its effect on throttle/brake output was not traced (OPEN).

## 7. What a mod must hold or account for

Adaptive AI (per-lap or per-gap) would fight the native walk only in these ways:
1. Writing 0x453C directly is overwritten within 4 s (AIAbility), and reset to 1.0 or base by SceneRenderUpdate paths (section 6). Do not use 0x453C as the control knob. Consistent with CLAUDE.md note and findings.jsonl:396 (DISPROVEN, poke does not hold).
2. Writing 0x44F4 (baseline) is the stable lever: its only writers are sub_424080 (init, FACT). Branch 1 snaps up to a higher baseline in <= 4 s and decays down 0.01 per 4 s. Branch 2 ramps up 0.01 per 4 s to 0x44F4 + 0x44D8 (so a raised baseline also raises the push ceiling only if 0x44D8 is also changed or derived from GDS).
3. Keep the traffic channel in mind: AI in traffic gains up to +0.0266 (Monza; PACE_MULTIPLIER.md section 4). A player who stays close to the AI drives its pace up, which is a native effect your mod must not mistake for rubber-banding.
4. Keep RNG effects in mind: any extra RandLCG call shifts the random gate sequence.
5. Keep these constant or re-derive them: slider, GDS, RA values (0x44C4, 0x44D4), wetness, and the 0x4538 command path (pit speed).
6. The 0.85 car-condition branch (0x3D6C == 1) overrides the walk. A mod that leaves carConditions set will suppress its pace target.

## 8. Open items

- OPEN: writer and meaning of 0x3D6C (carConditions) and 0x3984 (this[3681]) not traced.
- OPEN: whether roster car exclusion at 0x4550 (OpponentProximityScanTeamRecognition L77) removes the player from the traffic set.
- OPEN: frequency of SceneRenderUpdate(0) on pit events and whether they reset 0x453C live.
- OPEN: AISteering L1135, L1204, L1248 conditions for traffic increments.
- OPEN: output path of v156 to throttle and brake (player-proximity speed effect).
- OPEN: live confirmation of the per-frame absence of the walk on a player-adjacent AI (requires a live session; not run, read-only task).

## 9. Records and checks

- Workspace validator (`tools\workspace\check-workspace-structure.ps1`): 0 warnings; 2 pre-existing errors ("unexpected top-level directory" for `.claude` and `plans`). Not caused by this task; not changed here.
- Not appended to `findings.jsonl`: the task was read-only except this report. The findings below are pending owner authorisation:
  - `0x453C` cadence is 4 s, not per frame (correct findings.jsonl:28; FACT).
  - Native pace has no gap, position or player rubber-banding (OBSERVED live plus FACT static).
  - AIAbility header "0.85 on disqualification" should read "carConditions 0x3D6C == 1".
- Live game not touched. No files other than this report written.
