# H14 - AI theoretical-performance tables and PLR grip/brake usage (static)

Date: 2026-10-15. Agent: Haiku H14. Mode: read-only static review; the only write is this file.
Repo: `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled`. Game not touched.
Tags: FACT = read in corpus code or data; DERIVED = arithmetic or inference from FACT; OPEN = not established.
Paths are repo-relative.

## 0. Summary

1. The five PLR keys (AI Max Load, AI Min Radius, AI Corner Grip Usage, AI Brake Grip Usage, AI Brake Power Usage) are registered in `PlayerOptionsInitializer` (0x004AA270). FACT.
2. No code reader of those five keys was found. Each token string is referenced only by the initializer. FACT.
3. The per-car corner terms used while driving (0x4500, 0x4504, 0x4580, 0x453C) are set in `sub_424080` from AI settings keys (CorneringAdd, CorneringMult, Rainability, RaceAbility), not from PLR. FACT.
4. Per-waypoint tyre-solver inputs (AIW record +0x70, +0x74, +0x78, +0x7C) are constructed to constants (0, 0, (0, -9.81)). No later writer was found in AI or AIW code. FACT for the constructor; DERIVED for "no later writer" (static scan only).
   Consequence (DERIVED): the solver term in AIComputePassScore and AIRacingLine depends on the car only, not on the waypoint or speed. This matches the offline replay (median 0.9993, 100 % within 2 %, Monza; `docs/54-ai-driving-brain-map.md:83`).
5. The load-time construction of the per-car solver tables (this+0x3C40, +0x3C80, +0x3D00, +0x3CFC) was not located. OPEN. This is the main missing piece.
6. Why PLR grip/brake changes did not move lap time: no static path links those values to the corner or braking formulas (section 5). DERIVED. An ordinal (non-name) consumer is not excluded. OPEN.

## 1. PLR registration

Source: `functions/PlayerOptionsInitializer_004aa270.c` L1103-1108. Values decoded from the float bit patterns in the call (FACT).

| PLR key | Line | Default (a3) | Lower (a4) | Upper (a5) |
|---|---|---|---|---|
| AI Additional Fuel | L1103 | 0.95 | 0.1 | 2.0 |
| AI Brake Power Usage | L1104 | 0.98 | 0.1 | 2.0 |
| AI Brake Grip Usage | L1105 | 0.97 | 0.1 | 2.0 |
| AI Corner Grip Usage | L1106 | 0.93 | 0.1 | 2.0 |
| AI Max Load | L1107 | 40000.0 | 0.1 | FLT_MAX |
| AI Min Radius | L1108 | 20.0 | 0.1 | FLT_MAX |

Fraction defaults match `docs/42-ai-realism-and-difficulty-model.md:64`. Token addresses: `skeletons_filled/PlayerOptionsInitializer_004aa270.md:295-303`.
Sample profile: AI Max Load 20000, AI Min Radius 17.3 (`mods/sdk_sandbox/ai_mind_monitor/AI_PLR_PERFORMANCE_TABLE_NOTES.md:12-13`, previously recorded).

Record mechanics:
- `functions/MultiFieldSetterWithStringCopy_004b1d90.c` L1-11: writes this[5] (current, when a7 set), this[6] (= a3), this[7] (= a4), this[8] (= a5). DERIVED mapping: current/default, lower, upper.
- `functions/ShortComputation_1b70_004a1b70.c` L1-8: appends the record to a linked list when global `dword_7CE2E8` is non-zero. That global is set only in `functions/FlagSetterWithGlobalUpdate_004a1da0.c` L1-8, during initialization. FACT.

## 2. Per-car corner and pace fields (AI init)

Source: `functions/sub_424080_00424080.c` (called from AIDriverStructInit, `functions/AIDriverStructInit_00424790.c`). Offsets: float index x 4.

| Field | Offset | Init (FACT) | Keyed source (FACT) | Consumer |
|---|---|---|---|---|
| CorneringAdd | 0x4500 | 1.5 (L63) | key "CorneringAdd", L110-115 | AIComputePassScore |
| CorneringMult | 0x4504 | 0.965 (L64) | key "CorneringMult", L118-123 | AIComputePassScore |
| (pass multiplier) | 0x4580 | written L150 = 1 - flt_8AFEE8 * v30 | v30 = this[4402] * 0.01 * flt_5EA3D8; this[4402] = RainAbilityScaled, key "Rainability" L98-101 | AIComputePassScore |
| (pace) | 0x453C | 1.0 default (`skeletons_filled/AIDriverStructInit_00424790.md:53`) | adjusted by AIAbility (`functions/AIAbility_00429470.c`) | AIComputePassScore, AIRacingLine |
| (ability) | 0x44F4 | (1 - RaceAbility*0.01) * flt_5EA3D8, L152 | key "RaceAbility", L104-108 | AIAbility |

- Skeleton `skeletons_filled/sub_424080_00424080.md:33` calls 0x4580 a "failure lap count". Code L150 contradicts it; code wins.
- Skeleton lines 48-54 say these are recalculated every frame (unverified here).
- The keys come from list `v2`. Its source file was not traced (candidate: `functions/AISettingsFileLoader_004333e0.c`). OPEN. No PLR key string appears in this function. FACT.

So the driven CorneringMult/CorneringAdd do not come from AI Corner Grip Usage. FACT.

## 3. Per-waypoint inputs and the tyre solver

### 3a. AIW record (316 bytes, 79 dwords)

- `functions/sub_402690_00402690.c` L195-212: allocates `316*n + 4` bytes and runs the constructor per record (`functions/FAILTriageUnknownUnidentifiedButIsActuallyConstr_004fad80.c`, body named sub_4FAD80). FACT.
- Constructor sets (FACT, L30-35): this[28] (+0x70) = 0; this[29] (+0x74) = 0; this[30] (+0x78) = 0; this[31] (+0x7C) = -1055066685 = 0xC11CF5C3 = -9.81 (DERIVED decode); this[32] (+0x80) = 0; this[33] (+0x84) = 1.0 (wp_event first value, `docs/52-aiw-navigation-static-architecture.md:208`).
- Parser `sub_402690` does not write +0x70..+0x80 directly (static scan). FACT (scan).

### 3b. Writers after construction

Checked: AIRacingLine, AIComputePassScore, `sub_402690`, grip state machine `functions/MultiPhaseProcessingStateMachine_00404460.c`, phase 0 `functions/sub_4FB260_004fb260.c` (writes +0x94..+0x9C), phase 1 `functions/sub_4FAF60_004faf60.c` (writes +0x3C, +0x50..+0x6C, +0xA4..+0xAC, +0xB8, +0xBC..+0xC8), phase 2 VehicleProximity.
- None writes +0x70..+0x84. The other hits are in unrelated structs (tyre, suspension, camera). DERIVED (static only; a runtime writer is not excluded).

### 3c. Call sites (FACT)

- `functions/AIComputePassScore_0042a780.c` L3-4: `TireForceSolver(a3[29], a3+30, a3[28], 16.0)`; returns `(a2*a1[4417] + a1[4416]) * a1[4448] * a1[4431] * a3[33]`. a1 = vehicle, a3 = waypoint, a2 = scalar input.
- `functions/AIRacingLine_00425920.c` L263: `TireForce(v46, +116, +120, +112)`, the same fields (+0x74, +0x78, +0x70).

### 3d. TireForceSolver (`functions/TireForceSolver_004e8ce0.c`)

Signature L1: `double sub_4E8CE0(float *this, float a2, float *a3, float a4, float a5)`. Mapping from the AI call (DERIVED): this = tyre struct (veh+0xC, `docs/54:83`); a2 = wp+0x74; a3 = (wp+0x78, wp+0x7C); a4 = wp+0x70; a5 = 16.0.

FACT from code:
- Index: `v6 = |a4| * this[3903]` (L60); `idx = floor(v6)`, clamped to 13 (L62-68); `p0 = lerp(this[3904+idx], this[3905+idx])` (L73-74); cap 128 (L80). If idx < 0, `p0 = this[3904]` (L78).
- Window: `p_lo = p0 - 4`, `p_hi = p0 + 4`, floors 0 and 8 (L81-86).
- Residuals (L88-91): `R1(p) = p^2*this[3740] - (p^2*a2 + a3[1])*this[3397]`; `R2(p) = (p^2*a4 - a3[0])*this[3397]`.
- Table branches use threshold this[3855], positive table this[3856..3869], negative table this[3872..3885] (skeleton `skeletons_filled/TireForceSolver_004e8ce0.md` L13-18). Full sign logic: OPEN.
- Bracket expansion: `p_hi += a5` or `p_lo -= a5` on sign failure (L207, L218), clamped [0, 128]. a5 = 16.0 is the bracket step (DERIVED role).
- Return: bisection root in [0, 128], or 0 / 128 at the bounds.

Closed form at default waypoint inputs (a2 = 0, a4 = 0, a3 = (0, -9.81)) (DERIVED):
- idx = 0; p0 = this[3904]; window [p0-4, p0+4].
- R1(p) = p^2*this[3740] + 9.81*this[3397].
- R2(p) = 0 for every p (a4 = a3[0] = 0), so its sign test is degenerate here.
- Result TF0 = one number per car, set by the tables and the two scalars.

### 3e. Closed form of the AI corner target

    TF0      = TireForceSolver(tyre struct; a2=0, a3=(0,-9.81), a4=0, a5=16)   [per car]
    pass(wp) = (TF0 * veh[0x4504] + veh[0x4500]) * veh[0x4580] * veh[0x453C] * wp[0x84]
    corner target 0x43FC ~= pass(wp) * veh[0x4554]     (0x4554 = 1.0 in samples)

The offline replay (`docs/54:83`, `investigations/ai_brain_map/phase3/REVIEW_14_D1.md`) fits this form. DERIVED.
PLR does not appear in this formula.

Braking look-ahead (prior art, `docs/54:88`; `functions/AIRacingLine_00425920.c` L205-349): `v -= seglen(wp+0x3C) / v * 0x44CC * TireForce(v, wp+0x74, wp+0x78, wp+0x70) * 0.915`. 0x44CC = PassingNorm. No PLR input. Constants from prior art, not re-verified.

## 4. Load-time table builders: status

Candidates checked, none confirmed as the writer of this[3856..3917] or of this[3397], this[3740], this[3903], this[3855]:
- `functions/TireModelDataInit_004f34e0.c`: zeroes fields of a different struct; no 0x3C40/0x3D00 write in the printed part.
- `functions/TireGripDataArrayInit_004f6b40.c`: 45 entries of -1/0 init only.
- `functions/TheSvmHdvVehicleFileParser_004f4270.c` (HDV parser, the expected tyre source): not traced.
- `functions/PhysicsStateStructureInitializer_004d68a0.c`, `functions/PhysicsParameterInitialization_004af3a0.c`: write other offsets.
- `functions/AIDatabase_Init_004040a0.c`: AI database setup; calls `sub_402690`; no tyre table. FACT.
- `functions/AIPathPerformanceFunction_004bfa00.c`: AI name and path lists; not a performance table. FACT.
- `functions/AICornerSpeedComparison_00419500.c`: reads `flt_5F0780[4656*v + a2]`, a per-track record table used for timing. Table source OPEN. FACT (code).
- `functions/sub_429960_00429960.c`: tyre choice and fuel. Not a performance table. FACT.

## 5. PLR grip/brake keys: readers and effect

| PLR key | Default | Static readers | Plausible target (name-based only) | Status |
|---|---|---|---|---|
| AI Corner Grip Usage | 0.93 | none | cornering cap in TF or 0x4504 | OPEN, no path |
| AI Brake Grip Usage | 0.97 | none | braking TF in AIRacingLine | OPEN, no path |
| AI Brake Power Usage | 0.98 | none | braking decel scale | OPEN, no path |
| AI Max Load | 40000 | none | load-table upper bound at setup | OPEN, builder not found |
| AI Min Radius | 20 | none | load-table lower radius at setup | OPEN, builder not found |

Evidence: `python tools/ask.py token "<key>"` returns the string literal, the initializer as the only code user, and "no extracted parser handles this key yet" (FACT, all five). No float read of the record offsets was found outside the initializer.
Prior art, not independently proven: `docs/12-globals-reference.md:8195` ("setup/plr") and `docs/42` L64 (physics cap); `investigations/adaptive_ai_difficulty/haiku_H5_corner_cap_2026-10-15.md:151` (consumer not found; agreement, not independent proof).

Why the live null result fits (DERIVED): the driven corner and brake terms use TF0 (tyre tables), CM/CA/rain (AI keys), 0x453C and 0x44CC (AIAbility and PassingNorm), and wp[0x84]. None reads a PLR grip or brake value.

Caveat (OPEN): a consumer that walks the option list by ordinal, or copies `this[5]` into a global, would not show the token string in code. Not excluded; the 27 callers of `StringKeyedLinkedListSearch` (`functions/StringKeyedLinkedListSearch_005679c0.c`) were not exhausted for such a walk.

## 6. Contradictions

- `docs/54:82` calls 0x4500 and 0x4504 "pass score weights"; `sub_424080` sets them as CorneringAdd (1.5) and CorneringMult (0.965). Code names are the better guide.
- Skeleton 0x4580 "failure lap count" vs code L150 (rain-derived). Code wins.
- `docs/12-globals-reference.md:8763` labels 005D0310 "AIMAXLOAD INI key"; the string is the PLR key "AI Max Load". Label is wrong.

## 7. Next probes (static first)

1. Find the writer of this[3856..3917] and the scalars this[3397], this[3740], this[3903], this[3855]. Start with `TheSvmHdvVehicleFileParser_004f4270` and the tyre-struct initializer. Decode the table branch of TireForceSolver to finish the closed form.
2. Decide whether any runtime write to waypoint +0x70..+0x7C exists. A read-only watch on one live waypoint would settle it (owner approval needed).
3. Enumerate the option-list reader (ordinal vs name) to test the caveat in section 5.
4. Trace the source of list `v2` in `sub_424080` (AI settings file) to see where CorneringAdd and CorneringMult vary per track or driver.
5. Decode flt_8AFEE8 and flt_5EA3D8 (used for 0x4580 and 0x44F4).

## 8. Limits and follow-up

- Static only. No process, memory read, or write on the live game.
- Float decodes by hand; -9.81 from 0xC11CF5C3 is DERIVED.
- Workspace validator run: 2 errors unrelated to this task (unexpected top-level `.claude` and `plans` directories). Not changed.
- `findings.jsonl` not appended (read-only scope). Pending: record FACT items in sections 1-3 and OPEN items in sections 4-5 with tier and source, once a corpus write is authorized.
- Wasted-effort check: repeated reads of AIComputePassScore, TireForceSolver and the PLR initializer; broad greps over all functions before filtering by the 316-byte stride. None changed the result.

## 9. Sources

- functions/PlayerOptionsInitializer_004aa270.c L1101-1108; functions/MultiFieldSetterWithStringCopy_004b1d90.c; functions/ShortComputation_1b70_004a1b70.c; functions/FlagSetterWithGlobalUpdate_004a1da0.c
- functions/sub_424080_00424080.c L56-152; skeletons_filled/sub_424080_00424080.md L27-54
- functions/AIComputePassScore_0042a780.c; functions/AIRacingLine_00425920.c L263
- functions/TireForceSolver_004e8ce0.c L1-222; skeletons_filled/TireForceSolver_004e8ce0.md
- functions/FAILTriageUnknownUnidentifiedButIsActuallyConstr_004fad80.c L30-35; functions/sub_402690_00402690.c L195-212, L320-378
- functions/MultiPhaseProcessingStateMachine_00404460.c; functions/sub_4FB260_004fb260.c; functions/sub_4FAF60_004faf60.c
- docs/52-aiw-navigation-static-architecture.md L196-236; docs/54-ai-driving-brain-map.md L82-88; docs/42-ai-realism-and-difficulty-model.md L64
- mods/sdk_sandbox/ai_mind_monitor/AI_PLR_PERFORMANCE_TABLE_NOTES.md L12-13
- investigations/adaptive_ai_difficulty/haiku_H3_aiw_format_laptime_2026-10-15.md section 5; investigations/adaptive_ai_difficulty/haiku_H5_corner_cap_2026-10-15.md L151
