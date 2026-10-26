# H6: AI strength curve, slider 70-120 (static, read-only) - 2026-10-15

Scope: static reverse engineering from the decompiled corpus only. No game process, poke.py, lab.py, memory-mcp or runtime tools were used. Only this report was written. No findings.jsonl entry was appended (owner asked for read-only except this file); see section 8.

Labels: FACT = code or cited record shows it. DERIVED = reasoned from FACT. OPEN = not established. "Live" = recorded in an existing docs/ or findings entry, not re-verified here.

## 1. Bottom line

- Two separate slider paths exist. (a) Physics: AIDriverStructInit passes `AIDifficultyInterpolation()` (x0.995 outside sessions 3/4/5) into sub_4F1510. (b) Pace: GlobalDifficultyScalar (GDS, flt_5EA3D8) is the same piecewise-linear curve with track anchors, stored in the AI database object.
- GDS is NOT recomputed from the slider per frame or per UI change. It is written once, by AIDatabase_Init, which is called once from Game_Init (DERIVED, section 3).
- The three anchors (flt_5EA3DC/E0/E4) are the AI database fields +0x11C/+0x120/+0x124. Their values come from the AI database file parsed by sub_402690 (FACT for the field map). Their per-track dependence is OPEN (conflict between notes, section 4).
- The live anchors recorded for Monza are 0.85 / 1.00 / 1.10 (live, docs/42 section 4.1; docs/43). The defaults before parse are 0.5 / 1.0 / 1.3 (FACT, AIDatabase_Init_004040a0.c:29-31).

## 2. Slider curve formulas (FACT)

Source: functions/AIDifficultyInterpolation_00424ac0.c:5-9 (s = dword_7D01A0, L = flt_5EA3DC, M = flt_5EA3E0, H = flt_5EA3E4):

- s <= 100: `L + (M - L) * (s - 70) * (1/30)`  (constant 0.033333335 = 1/30)
- s > 100:  `M + (H - M) * (s - 100) * 0.05`  (constant 0.050000001)

AIDatabase_Init_004040a0.c:53-63 has the same shape with the database fields (this[71]=L, this[72]=M, this[73]=H) and writes the result to this[70] (line 63). Same math, so GDS(s) = AIDifficultyInterpolation(s) when the anchors match.

Other slider-dependent functions (FACT, formulas as in source):

| Function | Formula | Use | Source |
|---|---|---|---|
| AIDifficultyScalar sub_424750 | s >= 95 ? 1.0 : 0.75 + (s-70)*0.04*0.25 | AI ability and steering (per call) | functions/AIDifficultyScalar_00424750.c:3-6 |
| sub_424080 veh+0x44C4 damp | s < 100 ? (s/100)^2 * (22-RA)/22 : (22-RA)/22 | Race ability norm | functions/sub_424080_00424080.c:157-158 |
| sub_424080 veh+0x44F4 | (1 - RA/100) * GDS | Baseline pace scalar | sub_424080_00424080.c:151 (GDS read), :149-152 writes 0x44F4 (this[4413]) |
| sub_424080 wet 0x4580 | 1 - flt_8AFEE8 * RainAbility*0.01*GDS | Wet penalty | sub_424080_00424080.c:148, 150 |
| AIBehaviorInitialization line 35 | s * 0.0083333338 (= s/120) | Passed to sub_4BE760 (simulated lap time path per findings "gdb-laptime-keys-simulated-only-2026-10-14") | functions/AIBehaviorInitialization_004bf0a0.c:35-36 |
| AIRingBufferFieldInit sub_424C80 | this[4370] = flt_8AAE74[i]*0.15 + 0.15 - s*0.001 | Field this+0x4568; the constant term source is a table (OPEN) | functions/AIRingBufferFieldInit_00424c80.c:6-8 |
| sub_4C29F0 | v4 = (float)s; ArithmeticComparison_2dc0(v4) when dword_7D0990 > 0 and session == 7 | Per tick, race only | functions/sub_4C29F0_004c29f0.c:12-16 |
| AIDriverStructInit physics input | v19 = AIDifficultyInterpolation(); if session not in {3,4,5}: v19 *= flt_5EA3E8 (0.995); ScaleAudioPitch(v19) | Per-car init | functions/AIDriverStructInit_00424790.c:127-131 (note: skeleton line numbers differ, use the .c) |

## 3. The three anchors and GDS: who writes them (FACT unless marked)

1. Address map. Object base is dword_5EA2C0 (the AI database). Game_Init passes it: functions/Game_Init_0042d6b0.c:11 `AIDatabase_Init((float *)dword_5EA2C0)`. This is the only caller of sub_4040A0 (H1 cites `ask.py func 0x4040a0`; DERIVED that object is inline at 0x5EA2C0, because FunctionCallWrapper_00401080.c:7-21 writes dword_5EA2C0[0..5] as an array).
2. Offsets. Using float index x 4 = bytes:
   - this[70] = +0x118 = 0x5EA2C0 + 0x118 = **0x5EA3D8** = GDS (written at AIDatabase_Init_004040a0.c:63).
   - this[71] = +0x11C = **0x5EA3DC** = WorstAdjust / L. Written by sub_402690_00402690.c:235 (`sub_565870("WorstAdjust", this + 284)`).
   - this[72] = +0x120 = **0x5EA3E0** = MidAdjust / M. sub_402690_00402690.c:240.
   - this[73] = +0x124 = **0x5EA3E4** = BestAdjust / H. sub_402690_00402690.c:243.
   - this[74] = +0x128 = RaceQualRatio, written at sub_402690_00402690.c:248; not used by the curve.
   The equality of the offsets with the live-recorded anchor addresses is FACT for the address arithmetic; that the object is at 0x5EA2C0 is DERIVED from the call and the array writes.
3. Defaults before parse (FACT): AIDatabase_Init_004040a0.c:29-31: this[71]=0.5, this[72]=1.0, this[73]=1.3.
4. Clamps (FACT, sub_402690): WorstAdjust raised to at least 0.5 (lines 236-238); BestAdjust capped at 1.3 (lines 244-245). MidAdjust has no clamp.
5. Writers of the globals by absolute address: none in the corpus (grep of functions/ for 5EA3DC/E0/E4 gives only reads in AIDifficultyInterpolation). Writers of 0x5EA3D8 by absolute address: none (grep gives only reads at sub_424080_00424080.c:148,151). So any write goes through the object pointer (DERIVED).
6. GDS formula inside AIDatabase_Init (FACT, lines 52-63): for s > 100 `(1 - r*0.05)*M + r*0.05*H`, for s <= 100 `(1 - r*0.033333)*L + r*0.033333*M`, with r = s-100 or s-70.

Live check recorded earlier (live, docs/41 runbook lines 41-47; docs/42 section 4.1): GDS = 0.96 at slider 92 with Monza anchors 0.85/1.0. Formula: 0.85 + 0.15*22/30 = 0.96 (DERIVED, exact). This matches the GDS path, not the physics path, since the physics path has the 0.995 factor only in race.

## 4. Anchor values and their scope (OPEN where noted)

- Monza, live-recorded (live, docs/42:75-76; docs/43:83): WorstAdjust 0.85, MidAdjust 1.0, BestAdjust 1.1. Used for the table below as the "Monza" set.
- Defaults before the parse (FACT): 0.5 / 1.0 / 1.3. Used as the "default" set below. This is what the curve is if the AI database file gives no anchors.
- CONFLICT: name_index.json:43173-43208 and docs/12 notes say the anchors are read "from the loaded track AIW". H1 (haiku_H1_aiw_adjust_2026-10-15.md, section 1 and open item 2) says the file opened in AIDatabase_Init is not proven to be per track, and Game_Init is the only caller. OPEN: whether the anchors vary per track. If Game_Init is the only caller, the anchors are fixed per boot and per file (DERIVED), which contradicts a per-track claim unless the file name changes at boot.
- The file opened is aAiDatabaseFile (AIDatabase_Init_004040a0.c:39), with path from dword_7CE608 or byte_5EA438 (lines 40-41). The file name source is PlayerOptionsInitializer_004aa270.c:88 (a1+348). Whether that is the track file: OPEN.

## 5. Table, slider 70 to 120

Columns:
- GDS (Monza anchors) = flt_5EA3D8 under Monza anchors. Label: DERIVED from FACT formula + live anchors (FACT for 92 = 0.96, live).
- Physics a2 race = 0.995 x GDS-curve. Passed to sub_4F1510 (ScaleAudioPitch). Session 7 and practice (1) take the 0.995 factor. Label: FACT for the 0.995 gate and the formula; DERIVED for the numbers.
- Physics a2 qualifying = curve without 0.995 (session 5 is in {3,4,5}). Label: FACT for the gate, DERIVED for the numbers. Session 5 = qualifying is FACT (docs/38-confirmed-offsets.md:19; findings "session-global-7c56a4-values-live-2026-10-02" OBSERVED).
- 0x44C4 factor = (s/100)^2 for s < 100, else 1.0; the full value is this times (22-RA)/22. Label: FACT for the slider part (sub_424080:157-158). Independent of anchors and of track.
- GDS default set (anchors 0.5/1.0/1.3), for comparison. Label: DERIVED.

| Slider | GDS Monza (DERIVED) | Physics race a2 Monza (DERIVED) | Physics qual a2 Monza (DERIVED) | GDS default set (DERIVED) | Race a2 default set (DERIVED) | 0x44C4 slider factor (FACT) | Sim-lap input s/120 (FACT) | AIDifficultyScalar (FACT) |
|---|---|---|---|---|---|---|---|---|
| 70  | 0.850000 | 0.845750 | 0.850000 | 0.500000 | 0.497500 | 0.490000 | 0.583333 | 0.7500 |
| 75  | 0.875000 | 0.870625 | 0.875000 | 0.583333 | 0.580417 | 0.562500 | 0.625000 | 0.8000 |
| 80  | 0.900000 | 0.895500 | 0.900000 | 0.666667 | 0.663333 | 0.640000 | 0.666667 | 0.8500 |
| 85  | 0.925000 | 0.920375 | 0.925000 | 0.750000 | 0.746250 | 0.722500 | 0.708333 | 0.9000 |
| 90  | 0.950000 | 0.945250 | 0.950000 | 0.833333 | 0.829167 | 0.810000 | 0.750000 | 0.9500 |
| 92  | 0.960000 | 0.955200 | 0.960000 | 0.866667 | 0.862333 | 0.846400 | 0.766667 | 0.9700 |
| 95  | 0.975000 | 0.970125 | 0.975000 | 0.916667 | 0.912083 | 0.902500 | 0.791667 | 1.0000 |
| 100 | 1.000000 | 0.995000 | 1.000000 | 1.000000 | 0.995000 | 1.000000 | 0.833333 | 1.0000 |
| 105 | 1.025000 | 1.019875 | 1.025000 | 1.075000 | 1.069625 | 1.000000 | 0.875000 | 1.0000 |
| 110 | 1.050000 | 1.044750 | 1.050000 | 1.150000 | 1.144250 | 1.000000 | 0.916667 | 1.0000 |
| 115 | 1.075000 | 1.069625 | 1.075000 | 1.225000 | 1.218875 | 1.000000 | 0.958333 | 1.0000 |
| 120 | 1.100000 | 1.094500 | 1.100000 | 1.300000 | 1.293500 | 1.000000 | 1.000000 | 1.0000 |

Notes on the table:
- The qual a2 column equals GDS only because the same curve is used; the 0.995 gate is the only difference (FACT).
- Physics a2 is then divided by the car baseline stored at phys+15348 (`this[15348]`) in sub_4F1510 (ScaleAudioPitch_004f1510.c:17, v14 = a2 / this[15348]). The baseline value is OPEN (H2 section 5 and 7.1). So the true multiplier v14 is OPEN; the table gives only a2.
- The 0x44C4 column is the slider part only. Full 0x44C4 = slider part x (22 - RA)/22. Example: RA = 10 gives (12/22) = 0.545454 times the slider part. Label FACT for the formula.
- Live clamp: docs/41 (runbook) reports 0x44C4 clamps at 1.0 for an extreme RA (-82). Not in sub_424080 lines 157-158 (no clamp there) and not located elsewhere in corpus. OPEN where the clamp lives.
- AIDifficultyScalar column is the per-call function value. It is not cached (FACT, read at call time at AIAbility sites H2 section 3a).

## 6. Where the slider is read from, and when

Read from (FACT):
- Storage: options object at byte_7CE498; current value at dword_7D01A0 (= 0x7CE498 + 0x1CF4 + 0x14 offsets per H2 section 2; the options base name is FACT, the exact field arithmetic is DERIVED from H2's table).
- Registration: functions/PlayerOptionsInitializer_004aa270.c:1101 `MultiFieldSetterWithStrings(aAiDriverStreng, 75, 70, 120, 0, 1)`: key "AI Driver Strength" (string at 0x5D0528, strings_index.json:11235), default 75, min 70, max 120.
- Setter: functions/MultiFieldSetterWithStrings_004b1ac0.c:5-9 stores default/min/max and sets current when a7 is set.
- PLR file path: functions/sub_4B01B0_004b01b0.c:7 appends ".PLR" to the player name. The reader that copies the file value into the current field is OPEN (not found in the corpus; ShortComputation_1b70 / sub_4A1B70 is a linked-list append, not a parser).
- UI presets: functions/DrivingModeParameterSetup_004adf10.c:65 switch on mode (mode index unk_7D0158 = 0x7D0158, set from sub_4732D0 at functions/sub_4732D0_004732d0.c:4-6 with dword_7D0410). Mode 1 writes 75 (line 112), mode 2 writes 85 (line 162), both clamped to the record min/max (lines 113, 163). Mode 0 writes nothing (line 66-67). Mode 3-5: not analysed here (H2 says OPEN). Effect: selecting modes 1 or 2 replaces the PLR value (DERIVED from H2 section 4 and this read).

When (FACT / DERIVED):
- Options are initialised by UIControlWindowInitialization (PlayerOptionsInitializer caller, skeletons_filled/PlayerOptionsInitializer_004aa270.md:8). Startup timing relative to Game_Init is OPEN.
- GDS (0x5EA3D8) is computed once in Game_Init -> AIDatabase_Init (sub_42D6B0 line 11 -> sub_4040A0 line 63). Only caller (H1, ask.py). DERIVED: GDS uses the slider value present at Game_Init. The live reading 0.96 at slider 92 (live) means the slider was already 92 at that moment, so the PLR value is loaded before Game_Init. Changing the slider later (UI or preset 1/2) does NOT update GDS in the static code (DERIVED). Live confirmation of that is OPEN and not claimed.
- The curve function AIDifficultyInterpolation is called at AIDriverStructInit (per car init, FACT, H2 section 6), and dword_7D01A0 is read live by sub_424080 (per driver parse), AIDifficultyScalar (per call), AIBehaviorInitialization (sim lap time), and sub_4C29F0 (per race tick). The per-tick reads mean an in-race slider change affects sub_4C29F0 and AIDifficultyScalar but not GDS nor the physics a2 (which is computed at car init, DERIVED).

## 7. Session types used in the gate (FACT)

- 7 = race (docs/38:19; findings 2026-07-27/28 live).
- 5 = qualifying (docs/38:19; findings 2026-07-27 live).
- 1 = practice (OBSERVED 2026-10-02, findings "session-global-7c56a4-values-live-2026-10-02").
- 3 and 4: meaning OPEN. They are exempt from the 0.995 factor in AIDriverStructInit_00424790.c:129 together with 5.

## 8. Open items and follow-ups

1. Per-track anchors: confirm whether the AI database file (aAiDatabaseFile, AIDatabase_Init_004040a0.c:39) is a track file, and whether Game_Init reruns on track change (H1 open item 2). Owner-visible test: read the three anchors live on two tracks. Not done here (no runtime).
2. Baseline this[15348] (phys+15348) at car init, to get v14 = a2 / baseline: see H2 section 7.1 (OPEN).
3. The 0x44C4 clamp at 1.0 (live, docs/41): find the clamp in static code or accept as live-only.
4. Slider changes after Game_Init: static says GDS is frozen (DERIVED). Needs a static check that no other path calls AIDatabase_Init or writes this+0x118, and a live A/B if the owner wants it confirmed.
5. Sessions 3 and 4 meaning (OPEN).
6. Stale docs: docs/42 and docs/43 state the anchors are "track .AIW"; the code path is the AI database file. Correction is for the owner to approve (not edited here).
7. findings.jsonl: candidate entries not appended (read-only scope). Suggested: (a) GDS = this[70] of the AI database object at 0x5EA2C0, written once in Game_Init, tier DERIVED; (b) anchors 0x5EA3DC/E0/E4 = this+284/288/292, tier FACT for the field map via sub_402690; (c) 0x44C4 slider factor is (s/100)^2 only for s < 100, tier FACT.

## 9. Files used

- functions/AIDifficultyInterpolation_00424ac0.c:5-9
- functions/AIDatabase_Init_004040a0.c:29-31, 39-41, 50, 53-63
- functions/sub_402690_00402690.c:235-248
- functions/Game_Init_0042d6b0.c:11
- functions/FunctionCallWrapper_00401080.c:7-21
- functions/sub_424080_00424080.c:148-158
- functions/AIDifficultyScalar_00424750.c:3-6
- functions/AIDriverStructInit_00424790.c:127-131
- functions/ScaleAudioPitch_004f1510.c:17 (v14 = a2 / this[15348], per H2)
- functions/AIBehaviorInitialization_004bf0a0.c:35
- functions/AIRingBufferFieldInit_00424c80.c:6-8
- functions/sub_4C29F0_004c29f0.c:12-16
- functions/PlayerOptionsInitializer_004aa270.c:88, 1101-1102, 1119
- functions/MultiFieldSetterWithStrings_004b1ac0.c:5-9
- functions/sub_4B01B0_004b01b0.c:7
- functions/DrivingModeParameterSetup_004adf10.c:65, 112-113, 162-163
- functions/sub_4732D0_004732d0.c:4-6
- investigations/adaptive_ai_difficulty/haiku_H1_aiw_adjust_2026-10-15.md, haiku_H2_ai_calibration_2026-10-15.md
- docs/41-runtime-verification-runbook.md:41-58 (live, not re-run); docs/42-ai-realism-and-difficulty-model.md:70-93; docs/43-ai-difficulty-calculator.md:83-97; docs/38-confirmed-offsets.md:19

## 10. Limits

- Static only. Live values are cited from existing records and not re-verified.
- Table values are arithmetic on the FACT formulas. Anchors are the Monza live set and the code defaults, not a measured track set.
- Governance cold-start (README, governance/PIPELINE.md, PRODUCT_STATUS.json, workspace validator) was not run; the owner scoped this task to read-only static analysis plus this one report.
