# H18 - Grip table writers, TireForceSolver units and demand inputs (static, read-only)

Date: 2026-10-15. Scope: static reading of decompiled code, the corpus docs and the prior replay model. The running game was not touched. The only file written is this report. No findings.jsonl append was made (the owner's read-only instruction overrides that rule for this task; see section 8).

Tiers: FACT = read in code or data (file:line). DERIVED = arithmetic or inference from FACTs. OPEN = not established. "Model" = `ai_brain_map/phase3/analysis/tireforcesolver.py`, which was written from a capstone disassembly of 0x4E8CE0 and replays logged AI corner speeds (see section 6).

## 0. Bottom line

- FACT: this[3872..3886] is written only by TireBrushModelForceSolver sub_4E8010, at the store `*(++v85 - 17) = v34` (functions/TireBrushModelForceSolver_004e8010.c:240). It is 15 samples (3872..3886), not 14. TireForceSolver reads up to index 14 (3886) via j+1 (functions/TireForceSolver_004e8ce0.c:136-141, clamp at 13).
- FACT: sub_4E8010 is called from 8 sites, several of them on_frame-tagged (`python tools/ask.py func 0x4E8010`). The table is therefore rebuilt during play, not only at session init. Its inputs change per frame or per event.
- DERIVED: the table is a force capacity F_cap(N) in newtons, indexed by total normal load N. It is not a speed table. The speed table is a different array at this[3904..3918] (functions/TireBrushModelForceSolver_004e8010.c:334-423).
- FACT: TireForceSolver returns a speed (m/s, capped at 128). Its root is where lateral demand m*v^2*kappa equals the capacity at N(v). Units are consistent with the caller: S*CorneringMult + CorneringAdd is a speed.
- FACT: the braking projection uses TireForce (sub_4E9100), not TireForceSolver. TireForce returns an acceleration because it multiplies by this[3398] = phys+13592 = 1/m (functions/SuspensionAndWheelPhysics_004e5f60.c:567-568). So the two callers use different units by design.
- DERIVED: wp+0x70 (a4) is the corner curvature kappa. wp+0x74 (a2) multiplies v^2 in the normal load, so it is a vertical curvature. wp+0x78/0x7C (a3[0], a3[1]) are the lateral and normal gravity components. The recipe's "(0,0,(0,-9.81))" is only true for kappa = 0 (see section 5).

## 1. Layout of the per-car tables (FACT, from the brush solver stores)

The brush solver fills four 15-sample families in one loop over sample i = 0..14 (functions/TireBrushModelForceSolver_004e8010.c:121-240). Each family is a sum over the four wheels with a sign pattern. Stores:

| Address (this[N]) | Byte offset (this) | Store line | Sign pattern across wheels k=0..3 (variable) | Read by |
|---|---|---|---|---|
| 3840+i | 15360+4i | :236 (`*(v85-48)=v15`) | pitch: +,+,-,- (`v83`) | not in TireForceSolver |
| 3856+i | 15424+4i | :238 (`*(v85-32)=v27`) | lateral mirror: -,+,-,+ (`v81`) | TireForceSolver, TireForce, TireGripLateral |
| 3872+i | 15488+4i | :240 | lateral: +,-,+,- (`v81`) | TireForceSolver, TireForce, TireGripLateral |
| 3888+i | 15552+4i | :237 (`*v85=v20`) | drive split (depends on this[14772], :160) | TireGripLateral (:156,161) |
| 3904+i | 15616+4i | :423 (`*v61++=v73`) | speed for kappa_i | TireForceSolver seed (:60-90) |

- FACT: the wheel-sign expressions are at :144, :177-183, :201-202, :221-224.
- FACT: this[3903] = 14*flt_7D029C (:110), and the seed index is |kappa| * this[3903] (TireForceSolver :60).
- FACT: C_lat = this[3855] (phys+15420) and C_long = this[3871] (phys+15484) are both set to 14/flt_7D0278 (:105-108). The table index is therefore N * 14/flt_7D0278, so sample i corresponds to load N_i = i*flt_7D0278/14 (DERIVED from :121 `t = i*flt*0.0714`).
- FACT: this[3887] (phys+15548) = 14/flt_7D0278 (:109), just past the 15-sample table. It is read by TireGripLateral (functions/TireGripLateralForceCalculation_004e9300.c:143).

## 2. Table closed form (DERIVED from the stores, each term tagged)

Sample i, total normal load N_i = t_i = i*Phi/14, where Phi = flt_7D0278 (runtime global, value OPEN, section 6).

1. Wheel load shares: L_k = fraction, sum 1. FACT writer: functions/SuspensionAndWheelPhysics_004e5f60.c:368-383 stores `v267..v270` into phys+13940+248k. Those values are (1-v211)(1-v26), (1-v211)v26, (1-v26)v211, v211 v26 (lines 375-377), which sum to 1. DERIVED: L_k are static weight shares from two ratios (v26, v211, from a2+3064/3076 and a2+3120/3128/3140). This closes H13's writer question O3.

2. Grip per wheel: G_k = s * (Lat_k + Long_k) * AIGripMult_k / 2 (FACT: functions/sub_4F18C0_004f18c0.c:33-40). s = phys+15348 (grip scale) and this[16420] is a second global scale (point 4). Note s enters twice: G_k contains s, and a per-frame multiplier is applied on top.

3. Load sensitivity: mu_k(L) = 1 + kload*min(L, Lt). FACT: kload = DivisionWithThreshold(AISens) and Lt = AISens_Y (sub_4F18C0 :47 and :49 path; functions/DivisionWithThreshold_004d5ea0.c). Values from H13 section 7 (2001 files: kload = -0.605/13300, Lt = 13300).

4. Sweep sample with transfer shift: for wheel k, effective load Leff_k = t_i*L_k + sigma_k * D_i, with sigma = (+,-,+,-) for the 3872 family and (-,+,-,+) for the 3856 family. FACT: :145-148 and :200-208. D_i is the lateral transfer v81 = F_sum(t_i)*s_g * v90 * 0.5 (FACT :140). F_sum(t_i) = sum_k G_k * (1+kload*min(Leff_k,Lt)) * Leff_k (FACT :125-156; :156 `v15 += (v19+1)*G_k*v17`).

5. Family value: T_B[i] = sum_k G_k * (1 + kload*min(Leff_k, Lt)) * Leff_k with the 3872 signs, and T_A[i] the same with the 3856 signs. Both are forces in N (DERIVED: units of t*L_k are N).

6. Speed table: for curvature kappa_j = j/(14*flt_7D029C), j = 0..14 (FACT :96 `v96 = j*v87*0.0714`), solve m*v^2*kappa_j = max(T_B(C*N(v)), T_A(C*N(v))) (FACT :337-413, with v68 = max, :380-390), where N(v) = C_L*v^2 + g*m and g = 9.8100004 (FACT :62, :62 expression, a2=0 and a3[1]=-9.81 flat). Stored at 3904+j (:423).

7. Global scale: F_sum is multiplied by this[16420] (FACT :136 `v12 = v8*this[16420]`). Writers: sub_4E5F60 per-wheel block (functions/SuspensionAndWheelPhysics_004e5f60.c:390-400, float) and VehiclePhysicsTimeStep (functions/VehiclePhysicsTimeStep_004c73f0.c:176, a BYTE write `*(BYTE*)(this+16420) = flags & 1`). Conflict: the same 4-byte field is written as a float and as a byte. OPEN (section 6).

8. Mass and downforce: m = this[3397] = phys+13588 = a3[64] + 0.75*this[14712] (FACT: SuspensionAndWheelPhysics_004e5f60.c:565-567; VehicleSetupInitialization_004e49b0.c:147 also writes 13588). C_L = this[3740] = phys+14960 (FACT writer SuspensionAndWheelPhysics_004e5f60.c:799; AccumulatorClamp_004ed0f0.c:16-21 and ActorMotionPostProcessingUpdate_004ed030.c:14-16 also write 14960 from 14972/14976 - OPEN which one is the live value). The 4096 factor in v54 (:287) is used only for the static per-wheel block (:291-300) and is not in the speed loop (:345-350). DERIVED: a units inconsistency, not resolved.

## 3. Who writes this[3872..3886] and when (FACT, call sites)

| Writer | Where | When (per tag / context) |
|---|---|---|
| TireBrushModelForceSolver sub_4E8010 | :240 | every call (see 3b) |
| PhysicsParameterInitialization sub_4AF3A0 | :199 (`*(this+15492)=1065353216`), :359 (`=1058642330` = 0.6), :361, :201-202, :361-362 | session/tyre-parameter init |

3a. Who calls sub_4E8010 (`ask.py func 0x4E8010`, FACT callers):
- sub_4F18C0 (per-wheel setup, :49 when a4 flag set): tyre compound setup.
- ScaleAudioPitch sub_4F1510 (functions/ScaleAudioPitch_004f1510.c:68-69): sets s (phys+15348) = a2, then calls brush with a2 = 0. This is the strength path.
- CollisionWrapperWithLock sub_4E4910 (functions/CollisionWrapperWithLock_004e4910.c:3-5): calls brush with a2 = 1, then 0. The a2 = 1 branch divides by s (sub_4E8010 `v57 = 1/this[15348]` in the a2 branch).
- ActorMotionPostProcessingUpdate sub_4ED030, FuelConsumptionAndWear sub_4ED960, AccumulatorClamp sub_4ED0F0, RepairServiceStateDispatcher sub_4ED250, LoopWithConditionalSubCall sub_4F1A60, LargeConfigFunctionWithPhysicsConstants sub_4E45B0.
- The ask.py priority tag for sub_4E8010 is on_frame. The guard condition for each of the six non-init callers was not traced (OPEN).

3b. When: DERIVED. The table is recomputed in the same call graph as per-frame physics (fuel, actor motion, accumulator clamp) plus event paths (collision, repair). Its inputs L_k (per frame, point 1 of section 2), this[16420] (per frame), C_L and m (per frame) change every step. So "static at load" is false. Only G_k changes at a tyre-compound setup (sub_4F18C0) or by the ScaleAudioPitch ratio.

3c. Other writers of the same addresses, which can alias the table (FACT):
- PhysicsParameterInitialization writes a value/min/max triple every 36 bytes: values at 15348, 15384, 15420, 15456, 15492, 15528, 15564, 15600, 15636, bounds at +8 and +12. Two triples land inside the table: value 15492 = T_B[1] with bounds 15500/15504 = T_B[3]/[4], and value 15528 = T_B[10] with bounds 15536/15540 = T_B[12]/[13] (functions/PhysicsParameterInitialization_004af3a0.c:191-215, 343-372, 503-515). The brush solver then overwrites those cells on each call. So the clamp reads the stale table values, and the parameter value is lost. OPEN: whether anything reads 15492/15528 as a parameter.
- Brush also writes parameter cells: 15420 (C_lat) and 15356 and 15484 (:107-109). Those are triple values/bounds of other parameters.

## 4. Question 2: units of TireForceSolver and TireForce

4a. TireForceSolver returns a SPEED (FACT, structure of the solve). Inside it, A = v^2*C_L - (v^2*a2 + a3[1])*m is a normal force in N, and B = (v^2*a4 - a3[0])*m is a lateral force in N (TireForceSolver_004e8ce0.c:95-96). The table lookups return N. The bracket moves in speed steps of a5 = 16 m/s (:207, :218), and the root is returned by linear interpolation of the speed (:227-229). Output range [0, 128] (:203, :206, :217; the 128 cap is FACT, 0x59E6FC = 128.0 per the model header). So S is in m/s.

4b. AIComputePassScore (functions/AIComputePassScore_0042a780.c:3-4): return = (S*a1[4417] + a1[4416]) * a1[4448] * a1[4431] * a3[33]. Offsets: 4417*4 = 0x4504 CorneringMult, 4416*4 = 0x4500 CorneringAdd, 4448*4 = 0x4580 wet, 4431*4 = 0x453C pace, a3[33] = wp+0x84. So the corner target is a speed in m/s when CorneringAdd is in m/s. Units: S (m/s) x dimensionless mult + add (m/s) = m/s. FACT as an expression; DERIVED that CorneringAdd is m/s. The corpus records the solver's st0 return as the phantom a2 (FACT per docs/54 and ai_brain_map/phase2 REVIEW_11_C8, confirmed by capstone at 0x42A780). The question's "a2@<st0>" is a Hex-Rays artifact for the solver's return.

4c. Replay (FACT, prior work): the Monza dry replay with the model in section 6 matched the logged 0x43FC corner target with median ratio 0.9993, 100% within +-2% over 3549 ticks (`ai_brain_map/phase3/REVIEW_14_D1.md`, cited in `docs/54`). This supports the TireForceSolver speed reading.

4d. Braking: TireForce sub_4E9100 (functions/TireForce_004e9100.c). Its return = min(T(slip)*this[3398]*sqrt(mu^2-a^2)/mu, this[3919]) + this[3921]*this[3398]*v^2 + a4[2] (:34-111). Here T is the table at 3840 (slip) and 3856/3872 (speed-related), in N. this[3398] = phys+13592 = 1/m (FACT writer, SuspensionAndWheelPhysics_004e5f60.c:567-568), so the return is an ACCELERATION in m/s^2 (DERIVED from the multiplication).
- FACT in the decompile: AIRacingLine_00425920.c:263-264 uses `v32` in `v46 = v46 - seglen/v46 * this[17612] * v32 * 0.915`, and calls TireForce with its return discarded. v32 is AIComputePassScore or flt_8ACFB4 (:233-259).
- FACT (corpus, not re-checked by me): the st0 scan (docs/54 section 3b and ai_brain_map/phase2 REVIEW_11_C8) reports a capstone check at 0x425ED9 showing the braking factor is TireForce's st0 return. So the decompile's v32 is the artifact, and the factor is TireForce.
- DERIVED: dimensions with that reading: Delta v = (seglen/v) * Passing * a * 0.915, where seglen is m, a is m/s^2 and v is m/s. The units agree. Under the decompile's reading (v32 = a speed) the dimensions would not agree, so the st0 reading is the consistent one.

4e. Summary of units: TireForceSolver -> m/s (speed). TireForce -> m/s^2 (acceleration). Passing 0x44CC (this[17612]) and 0.915 are dimensionless. The two are not interchangeable and the question's "same units" premise is false.

## 5. Question 3: demand inputs a2 / a3[0] / a3[1] / a4

Call shape: TireForceSolver(this, a2 = wp+0x74, a3 = &wp+0x78, a4 = wp+0x70, a5 = 16). FACT: functions/AIComputePassScore_0042a780.c:3; the same pattern in functions/PhysicsNodePropagationWithDistanceFalloff_00401c90.c:66, functions/SweptSuspensionCollisionDetection_00401910.c:75-103, functions/sub_426430_00426430.c:139. In the model, a3[0] = wp+0x78 and a3[1] = wp+0x7C (tireforcesolver.py header).

| Symbol | Waypoint offset | Where it enters (FACT) | Meaning (DERIVED) |
|---|---|---|---|
| a4 | wp+0x70 | seed `|a4|*this[3903]` (:60); B = (v^2*a4 - a3[0])*m (:96) | curvature kappa (1/m). Multiplies v^2, so lateral accel = v^2*kappa |
| a2 | wp+0x74 | A = v^2*C_L - (v^2*a2 + a3[1])*m (:95) | vertical curvature (1/m). Crest reduces normal load |
| a3[0] | wp+0x78 | B only (:96) | lateral gravity (banking), m/s^2 |
| a3[1] | wp+0x7C | A only (:95) | normal gravity component, m/s^2, = -9.81 on flat ground |

- DERIVED: with a2 = 0, a3 = (0, -9.81): A = C_L v^2 + 9.81 m, the normal load. B = m*v^2*kappa, the lateral demand. This is exactly the H13 closed form.
- The table is indexed by A (normal load) and the sign of B picks the table and the sign of the added term (model: branch `B >= 0` uses T_B with F = v - B, else T_A with F = v + B).
- OPEN: the sign convention of a3[0] and a2 relative to the physical bank and crest, and where wp+0x70..0x7C are written. The corpus says "not parsed from the AIW (writer OPEN, likely load-time AI tables)" (`docs/54`). The replay in 4c used logged waypoint values, so they are non-zero in the logs.

## 6. Limits, values and what would close them

- O1 (values): flt_7D0278 (named SpeedScalingDenominator, docs/12-globals-reference.md:668), flt_7D029C (SuspensionRefSpeed, docs/12:707, with a name conflict), flt_7D020C, flt_7D0230, flt_7D0254 are NOT in the static image. Static read of F1.exe (`%USERPROFILE%/Desktop/F1/F1.exe`, PE image base 0x400000): these addresses (RVA 0x3D0278 and so on) fall past the raw data of .data (raw ends at file offset 0x1EB000, file size 0x2A4000), so they are zero-filled at load and set at runtime. FACT. The runtime value is OPEN. A live read would be needed and is not allowed by this task. The read was static only, and no game process was touched.
- O2: writers of phys+15348 (s): PhysicsParameterInitialization (:183, :343, :503 = 0, 0.2, 0.5, then FloatClamp), SuspensionAndWheelPhysics (:293 = 1.0), ScaleAudioPitch (:68 = a2). Which session branch applies is OPEN.
- O3: CLOSED by section 2 point 1 (L_k, SuspensionAndWheelPhysics_004e5f60.c:368-383).
- O4: wp+0x70..0x7C writers: OPEN (section 5).
- O5: disassembly of 0x42A780 and 0x425920 at the branch flags. Hex-Rays drops the FPU flags at TireForceSolver :98, :150, :202, :214, so which table is used per call is taken from the model (sign of B), not from the decompile. This is DERIVED from the model; a capstone check is the verification step.
- O6: table-sign convention: T_B (3872 family) is the lateral capacity for the wheel shift sigma = (+,-,+,-). Its physical side (which wheels are the outside wheels) is OPEN.
- O7: whether temperature or wear enter the table: not on the traced path (OptimumTemp, AIWear). Unchanged from H13.
- O8: the 4096 factor (:287) versus the unscaled C_L in the speed loop (:345-350): inconsistent units, OPEN.
- O9: the 0.92/0.84/0.97 and flt_7D0230/7D0254 factors (:310-322): which table each applies to, OPEN, plus the normalisation branch a2 = 1 (CollisionWrapperWithLock).
- O10: vehicle+0xC identity: the model header says this = veh+0xC is confirmed by `lea ecx,[esi+0xC]` in AIComputePassScore (ai_brain_map/phase3/analysis/tireforcesolver.py header), which matches the question. Not re-checked by me.
- O11: C_L writers (14960 from 14972/14976 in AccumulatorClamp and ActorMotion): which is live, OPEN.

## 7. Corrections to earlier notes

- Question premise "14 entries at [3872..3885]": the table is 15 samples at [3872..3886], with 14 segments. TireForceSolver clamps j to 13 and reads j+1 = 14 (:136-141).
- Question premise "AIRacingLine braking uses TireForce via the solver": the decompile shows v32 (AIComputePassScore) and TireForce's return is not visibly consumed. The corpus st0 check says the factor is TireForce. Both are cited in section 4d. The braking uses TireForce (sub_4E9100), not TireForceSolver.
- LAPTIME_RECIPE line "wp+0x70..0x7C built as (0,0,(0,-9.81))": not consistent with a4 = curvature. If all four were as stated, kappa would be 0 and the solver would never reduce speed for curvature. Treat as UNVERIFIED until the writer is found (O4).
- LAPTIME_RECIPE "grip = G_k x (1 + k x min(load,13300))": confirmed (section 2, points 2-3).
- H13 "L_k writer OPEN": closed (section 2, point 1).
- H13 "per-axis tables at 15424, 15488, 15552 (line 299 onward)": the stores are at :236-240 and the reads at :299-385. Line 299 is only the base pointer `v56 = this+15424`.

## 8. Documentation gate

- Durable record: this file. Not yet indexed in `investigations/adaptive_ai_difficulty/README_2026-10-14.md` (follow-up).
- No findings.jsonl append (the read-only instruction for this task overrides the AGENTS.md append rule). Follow-up: append FACT items: brush table layout (:236-240), L_k writer (SuspensionAndWheelPhysics:368-383), 1/m at 13592 (:567-568), flt globals zero-filled in image.
- Pending validation: O5 (capstone of the branch flags), O1 (runtime values of flt_7D0278/flt_7D029C via a permitted offline or replay source), O2, O4, O8.
- Limits: field names are DERIVED from code shape, not symbols. The per-caller guard conditions for sub_4E8010 were not traced. The logged cars_static.jsonl used by the replay was not found in the workspace (`find` returned nothing), so the logged table values were not read. No live game state was read.
- Restoration: only this report was written. No corpus or game file was modified. The F1.exe was opened read-only for the static float check.

## 9. Derivation check for section 2 point 4

The shift for wheel k is sigma_k * D_i where D_i = v81 = F_sum(t_i)*s_g*v90*0.5 (FACT :140; v90 is set from the fabs of per-wheel position differences at phys+13708..14452, sub_4E8010 line 'v90 = 1/(fabs(...)*0.5)'). If v90 is 1/m, D_i has units of N/m, not N. OPEN: whether a CG-height factor is hidden in those fields. Until resolved, treat the lateral transfer magnitude as unverified.
