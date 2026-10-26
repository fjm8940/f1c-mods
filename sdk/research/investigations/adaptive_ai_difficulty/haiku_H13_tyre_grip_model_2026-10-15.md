# H13 - Tyre grip model used by the AI corner target (static, read-only)

Date: 2026-10-15. Scope: static decompile reading and .tbc value reads only. The running game was not touched. The only file written is this report. No findings.jsonl append was made (the owner's read-only instruction overrides the AGENTS.md append rule for this task; see section 8).

Tiers: FACT = read in code or data (file:line). DERIVED = arithmetic or inference from FACTs. OPEN = not established.

## 0. Bottom line

- FACT: the AI "tyreForce" is the return value of TireForceSolver (sub_4E8CE0). It is a live per-call solve, not a table lookup. It returns a SPEED (capped at 128), not a force.
- DERIVED: AIComputePassScore (sub_42A780) computes corner target = (S_tyre*mult + add) * wet * pace * path, where S_tyre is a tyre-limited speed. This matches H5 (haiku_H5_corner_cap_2026-10-15.md).
- FACT: the force model is per-wheel. Grip = G_k * mu(L), with linear load sensitivity mu(L) = 1 + k*min(L, 13300). Peak force sits on a friction ellipse sqrt(mu^2 - a^2)/mu.

## 1. Chain of calls

| Step | Function | Evidence |
|---|---|---|
| Pass/corner score | AIComputePassScore sub_42A780 | functions/AIComputePassScore_0042a780.c:3-4: `TireForceSolver(a3[29], a3+30, a3[28], 16.0); return (a2*a1[4417]+a1[4416])*a1[4448]*a1[4431]*a3[33];` |
| Tyre speed solver | TireForceSolver sub_4E8CE0 | functions/TireForceSolver_004e8ce0.c:1 (double return) |
| Speed table + load sweep | TireBrushModelForceSolver sub_4E8010 | functions/TireBrushModelForceSolver_004e8010.c:104-110 (writes this+15612 = 14*flt_7D029C) |
| Per-wheel data from tyre file | sub_4F18C0 | functions/sub_4F18C0_004f18c0.c:26-47 |
| Re-scale of per-wheel grip | sub_4F1510 (file name ScaleAudioPitch_004f1510.c is a mis-label) | functions/ScaleAudioPitch_004f1510.c:17-18, 68 |
| Friction-ellipse force (braking, per H5; lateral twin) | TireForce sub_4E9100 / sub_4E9300 | functions/TireForce_004e9100.c:34,80-87,102,111; functions/TireGripLateralForceCalculation_004e9300.c:142,163,167 |

## 2. Task 1 - every .tbc key, parser, struct offset

### 2a. File-level parser

- FACT: sub_4D7150 (functions/TireBrandFileParser_004d7150.c) reads [SLIPCURVE] and [COMPOUND] sections. It counts them: slip curves at 24-byte stride (this[2]), compounds at 680-byte stride (this[4]).
- FACT: [SLIPCURVE] goes to sub_4D5F70 (Name, Step, Data). [COMPOUND] goes to sub_4D6B00 (functions/sub_4D6B00_004d6b00.c): 680-byte record, front block +24, rear block +352 (328 bytes each).

### 2b. Compound record keys

Offsets are relative to the front block (+24) or rear block (+352). All numeric fields are doubles.

| Key in .tbc | Parser | Front offset | Notes |
|---|---|---|---|
| Name / WetWeather / Front: / Rear: | sub_5654A0 / sub_565760 / sub_587C20 | n/a / record+20 (byte) / n/a | Name not stored; WetWeather = value != 0; Front/Rear select block base |
| DryLatLong (2) | sub_565AA0 | +0 lat, +8 long | dry grip |
| WetLatLong (2) | sub_565AA0 | +16, +24 | wet grip |
| Radius / RadiusRPM / Width | sub_565AA0 | +32 / +40 (times 9.549296585513721) / +48 | m, per-RPM, m |
| SpringBase / SpringkPa / Damper | sub_565AA0 | +56 / +64 / +72 | |
| LoadSens (3) | sub_565AA0 | +80, +88, +96 | (initial slope, final grip mult, final load) |
| LatPeak (3) | sub_565AA0 | +104, +112, +120 | lateral peak slip vs load |
| LongPeak (3) | sub_565AA0 | +128, +136, +144 | longitudinal peak slip vs load |
| CamberLatLong (3) | sub_565AA0 | +152, +160, +168 | +152 times pi/180 |
| RollingResistance | sub_565AA0 | +176 | |
| Heating (2) / Transfer (3) / WearRate | sub_565AA0 | +184,+192 / +200,+208,+216 / +224 | heat; heat transfer; wear |
| Softness | sub_565AA0 | +232 | "for AI use" per file comment |
| AISens (2) | sub_565AA0 | +240, +248 | (AI final grip mult, AI final load) |
| AIGripMult | sub_565AA0 | +256 | |
| AIPeakSlip | sub_565AA0 | +264 | |
| AIWear | sub_565AA0 | +272 | |
| OptimumTemp | sub_565AA0 | +280 | C |
| OptimumPressureBase | sub_565AA0 | +288 | |
| OptimumPressureMult | sub_565AA0 | +296 | |
| LatCurve / BrakingCurve / TractiveCurve | sub_5654A0 | n/a | string names only |

Record-level derived fields (FACT, tail of sub_4D6B00):
- record+8 = mean of the four dry grip doubles: (+24 + +32 + +352 + +360) * 0.25.
- record+12 = same for wet.
- record+16 = wear rating = CRTFtol(22.5 - (rear Softness + front Softness)*0.5*15), clamped to [7,15]. Softness is at record+584 and record+256.

## 3. Task 2 - brush model and helpers

### 3a. Inputs read by TireBrushModelForceSolver (sub_4E8010)

Per-wheel block is 248 bytes (62 floats). Slots read through pointer v4 = phys+13868 (FACT from pointer arithmetic, lines 102-136):

| Slot | Address | Meaning |
|---|---|---|
| v6[-1] | phys+13864 | load slope k = (AISens_X - 1)/AISens_Y (DERIVED, sub_4D5EA0 via sub_4F18C0 L47) |
| v6[0] | phys+13868 | load threshold = AISens_Y (13300 in 2001 files; DERIVED) |
| v6[18] | phys+13940 | per-wheel load L_k (writer OPEN), line 125 |
| v6[-66] after v6+=62 | phys+13852+248k | scaled AI grip G_k, line 133 |

Other inputs:
- phys+15612 = 14*flt_7D029C (line 110). TireForceSolver reads it as this[3903].
- phys+16420 = global scale on the summed force (line 136). Value OPEN.
- phys+15348 = grip scale s. FACT: sub_4F1510 writes it (L68), sub_4F18C0 multiplies by it (L40). Source OPEN.
- phys+14960 (C_L) and phys+13588 (mass m): lines 345-353.

### 3b. Outputs

- 15-entry table at phys+15616 (floats 3904..3918), written at line 423. It is a speed-vs-curvature table: the tyre-limited speed for 15 curvature samples, step (1/14)/flt_7D029C (line 96). TireForceSolver reads it as the speed guess (sub_4E8CE0 L60-90).
- Per-axis tables at phys+15424, +15488, +15552 (line 299 onward). Scaled by 1/s with factors 0.92, 0.84, 0.97 (the mapping of each factor is OPEN).

### 3c. Peak-grip formula vs vertical load (DERIVED, lines 116-136)

For sample n = 0..14, t_n = n*flt_7D0278/14 (line 116):

    v9_k = t_n * L_k                          (line 125)
    v10_k = min(v9_k, L_t)                    (lines 126-129; L_t = 13300 in 2001 files)
    v11_k = v10_k * k_load                    (line 130)
    F_sum += (v11_k + 1) * G_k * v9_k         (line 133)
    F_out(t_n) = F_sum * s_global             (line 136)

So mu_k(L) = 1 + k_load*min(L, L_t) is the load-sensitivity term. Vertical load for the centripetal balance is (line 345):

    N(v) = C_L * v^2 + g * m,  g = 9.8100004 (literal)

Balance (lines 345-413): Fcap(v) = max(T_lat(C_lat*N), T_long(C_long*N)), where C_lat = phys+15420 and C_long = phys+15484 are both set (lines 106-109) to 1/(flt_7D0278/14). The solver finds v with m*v^2*kappa = Fcap(v), by bracket halving and secant (lines 340-413), and stores v_max(kappa_n) at line 423.

### 3d. Helpers

- sub_5895A2 (functions/sub_5895A2_005895a2.c): rounding helper before an int cast (mode OPEN).
- sub_4D6AE0 VehicleSuspensionForceHelper (functions/VehicleSuspensionForceHelper_004d6ae0.c:1-3): returns (OptimumTemp+273.16)*P*0.002778858*SpringkPa + SpringBase, with P = 190.0 (sub_4F18C0 L26). Feeds only phys+13880 = RollingResistance / that value (L26-30). A stiffness ratio, not grip.
- sub_4D5EA0 DivisionWithThreshold (functions/DivisionWithThreshold_004d5ea0.c:1-10): slope = (a2-1)/a3, threshold = a3. sub_447BB0 and sub_4E88B0 are brake-side, off the AI path.

## 4. Task 3 - which value the AI uses as tyreForce

- FACT: AIComputePassScore (functions/AIComputePassScore_0042a780.c:3-4) calls `TireForceSolver(a3[29], (int)(a3+30), a3[28], 16.0)`. The return is in st0. The decompiler labels the st0 input as a2, and the return expression uses a2. Taken literally this is inconsistent, because the call overwrites st0. The consistent reading is that a2 is the solver return and the input label is an artifact. H5 reached the same conclusion (haiku_H5_corner_cap_2026-10-15.md, line 54). Confidence DERIVED, about 85 percent. A disassembly check of 0x42A780 would make it FACT (O5).
- FACT: the solver's `this` is a1 (the ecx argument), the same physics object. Argument types match sub_4E8CE0's signature `(float *this, float a2, float *a3, float a4, float a5)` (TireForceSolver_004e8ce0.c:1).
- DERIVED: TireForceSolver is a live root-finder that returns a speed in m/s, in [0,128]. Bracket starts at table(|a4|*this[3903]) +/- 4 (lines 60-90), steps by a5 = 16 (lines 203-210). Returns 128 with no crossing (lines 203, 206, 210), 0 if the lower bound fails (line 217), otherwise linear interpolation (lines 227-229).
- Therefore S_tyre is a speed. The name "tyreForce" is a misnomer. sub_4E9100 (TireForce) is the braking-side force function, not this path.

## 5. Task 4 - closed form, each term tagged

Per wheel k (0..3), AI path:

1. Base grip. FACT code, DERIVED arithmetic:
   G_k = s * (Lat_k + Long_k) * AIGripMult_k / 2
   Lat_k, Long_k = DryLatLong of the front (k=0,1) or rear (k=2,3) block. s = phys+15348 (source OPEN).
   Example, 2001 Bridgestone Hard (2001_Bridgestone.tbc:68-125): front (1.9076+2.0960)*1.080/2 = 2.16194*s; rear (1.9702+2.1813)*1.150/2 = 2.38711*s.

2. Load sensitivity. FACT inputs, DERIVED form:
   mu(L) = 1 + k_load*min(L, L_t), k_load = (AISens_X - 1)/AISens_Y, L_t = AISens_Y.
   2001 files: AISens = (0.395, 13300) in all compounds (2001_Bridgestone.tbc:91, 119, 151, 179, 211, 239). mu(0) = 1, mu(6650) = 0.6975, mu(>=13300) = 0.395.

3. Sweep force. DERIVED from lines 116-136:
   F_sum(t) = s_global * sum_k [ t*L_k * G_k * mu_k(t*L_k) ], t = n*flt_7D0278/14, n = 0..14.
   L_k = phys+13940+248k (writer OPEN). s_global = phys+16420 (value OPEN).

4. Vertical load. DERIVED, line 345: N(v) = C_L*v^2 + g*m, g = 9.8100004, m = phys+13588, C_L = phys+14960.

5. Peak grip cap. DERIVED, lines 345-413: Fcap(v) = max(T_lat(C_lat*N), T_long(C_long*N)). T_* are 14-bin linear tables at phys+15424/+15488; C_lat = phys+15420 = 14/flt_7D0278 and C_long = phys+15484 (FACT code, flt value OPEN).

6. Tyre-limited speed for curvature kappa. DERIVED: m*v^2*kappa = Fcap(v), solved for v. Stored at phys+15616 for 15 kappa samples; read by TireForceSolver as the speed guess.

7. AI tyre limit. FACT for the call, DERIVED for meaning: S_tyre = TireForceSolver(phys, a2 = a3[29], a3 = a3+30, a4 = a3[28], a5 = 16), in [0,128].

8. Friction ellipse for force. FACT, TireForce sub_4E9100 lines 34, 80-111:
   F_long = min( T(s_slip) * this[3398] * sqrt(mu^2 - a^2)/mu , this[3919] ) + this[3921]*this[3398]*v^2 + a4[2]
   mu = interp(table 3856/3872 at |v^2*C - ...|*this[3855 or 3871]); a = |v^2*a5 - a4[0]|*this[3397]; T = interp(table 3840 at slip*this[3839]); this[3398] = scale (OPEN).
   Lateral analogue, sub_4E9300 lines 142-167: F_lat = min(T_lat*this[13592]*sqrt(mu^2 - a^2)/mu, cap) - this[15684]*this[13592]*v^2 - a4[2].

Not in the closed form (OPEN): temperature (OptimumTemp only feeds the spring-ratio helper), pressure (OptimumPressure* not read on the traced path), camber (converted to radians, consumer not traced), wear (AIWear stored at phys+13876, L39; no reader traced).

## 6. OPEN items

- O1: values of flt_7D0278, flt_7D029C, phys+16420, dbl_7C7428 (not read).
- O2: writers of phys+15348 (grip scale s) and callers of sub_4F1510 with their a2.
- O3: writer of phys+13940+248k (per-wheel load L_k).
- O4: meaning of a3[28..31], the waypoint/vehicle block feeding TireForceSolver.
- O5: disassembly of 0x42A780 to confirm the st0 return (section 4).
- O6: physics-path consumers of LoadSens, LatPeak/LongPeak, CamberLatLong, Heating, Transfer.
- O7: whether temperature (sub_4C77D0) and wear feed grip.
- O8: sub_5895A2 rounding mode. O9: which table takes the 0.92/0.84/0.97 factors (brush lines 299-320).

## 7. 2001 compound values (Bridgestone; values only from SeasonData/Vehicles/2001_Bridgestone.tbc, section line in brackets)

| Compound | Axle | DryLatLong | WetLatLong | Softness | AIGripMult | AIPeakSlip | AIWear | OptimumTemp | G_k per unit s (DERIVED) |
|---|---|---|---|---|---|---|---|---|---|
| Hard [68] | F | 1.9076, 2.0960 | 1.0214, 1.1223 | 0.50 | 1.080 | 0.095 | 1.334e-8 | 114.0 | 2.16194 |
| Hard | R | 1.9702, 2.1813 | 1.0550, 1.1680 | 0.50 | 1.150 | 0.094 | 8.683e-9 | 114.0 | 2.38711 |
| Soft [128] | F | 1.9305, 2.1211 | 1.0475, 1.1509 | 0.60 | 1.080 | 0.095 | 1.604e-8 | 112.0 | 2.18786 |
| Soft | R | 1.9938, 2.2075 | 1.0818, 1.1978 | 0.60 | 1.150 | 0.094 | 1.048e-8 | 112.0 | 2.41575 |
| Intermediate [188] | F | 1.8376, 2.0191 | 1.5084, 1.6574 | 0.75 | 1.080 | 0.095 | 2.008e-8 | 107.0 | 2.08262 |
| Intermediate | R | 1.8978, 2.1012 | 1.5579, 1.7248 | 0.75 | 1.150 | 0.094 | 1.321e-8 | 107.0 | 2.29943 |
| Wet [248] | F | 1.7228, 1.8929 | 1.7069, 1.8755 | 0.90 | 1.080 | 0.095 | 2.426e-8 | 104.0 | 1.95248 |
| Wet | R | 1.7792, 1.9699 | 1.7629, 1.9518 | 0.90 | 1.150 | 0.094 | 1.602e-8 | 104.0 | 2.15573 |
| Monsoon [308] | F | 1.6182, 1.7780 | 1.7938, 1.9709 | 1.00 | 1.080 | 0.095 | 2.699e-8 | 102.0 | 1.83395 |
| Monsoon | R | 1.6712, 1.8503 | 1.8525, 2.0511 | 1.00 | 1.150 | 0.094 | 1.782e-8 | 102.0 | 2.02486 |

Common to all compounds: LoadSens (-4.09e-5, 0.395, 13300); AISens (0.395, 13300); WetWeather=1; Radius 0.325; RollingResistance 1435 (F) / 1610 (R). The last column is G_k divided by s, from rounded inputs.

## 8. Documentation gate and limits

- Durable record: this file, not yet indexed in the adaptive_ai_difficulty README (follow-up). No findings.jsonl append (read-only instruction). Pending validation: O5, O1/O2 values, and a live AIComputePassScore replay (not run).
- Limitations: field names DERIVED from parser keys, not symbols; global values not read; no live game state. Restoration: only this report was written.
