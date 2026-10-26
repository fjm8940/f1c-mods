# H5 — Why AI CorneringMult (veh+0x4504) stops adding speed above about x1.03

Date: 2026-10-15. Agent: Haiku H5 (static only). Owner question: find the cap on the AI corner target.
Scope: read-only static analysis of the corpus. No game, poke.py, lab.py, memory-mcp or runtime tool was used.
Only this report was written. No findings.jsonl entry was appended (read-only rule); proposed entries are in section 8.
Paths are relative to `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\` unless absolute.

Labels: FACT = seen in code or in a cited live-confirmed record. DERIVED = my inference from FACTs.
OPEN = not resolved by the corpus.

## 0. Verdict (short)

- The AI corner target is `(TireForceSolver(wp)*mult + add) * wet * pace * pathValue`. Lowering the target binds
  (x0.97 gives +0.80 s/lap). Raising it does not bind, so some other limiter takes over above about x1.03-1.06.
- No single static clamp in the corner-target path caps the speed. `veh+0x4538`, `0x4554`, the 75 cap in
  sub_426430 and the 128 cap in the solver are all ruled out for normal racing (section 4).
- Top candidate (DERIVED, not verified): the car's own tyre limit. The planner and the physics-side tyre code read the
  same per-car tyre-table fields, so a target above the grip limit cannot be driven. Confidence about 40 %.
- Second candidate (FACT that the code exists, OPEN whether it binds): the AISteering speed governors that clamp
  `0x4484`, plus the speed ratio at `0x4444` that gates them. The ratio uses mult, so raising mult changes when they fire.
  Confidence about 25 %.
- Braking: CorneringMult reaches braking only through target gates (AIRacingLine L205, L209, L234). The deceleration
  itself is TireForce(v) * 0x44CC, which does not use mult. So brake points can move with mult, but the decel is physics.

## 1. Cold start done

- Read `AGENTS.md`, `README_2026-10-14.md` of this folder, the workspace and governance README heads.
  NOT done: `PRODUCT_STATUS.json`, full `PIPELINE.md`, and the structure validator (read-only task). OPEN for the owner.
- Prior art read: `ai_brain_map/phase3/D1_CORNER_SPEED.md`, `D11_WET.md`, `PACE_MULTIPLIER.md`, `docs/54` 3b,
  `docs/42` 3-4, `docs/48`, `ai_interaction_role/README.md` (2026-08-18), sibling `haiku_H3_*` section 5.

## 2. The corner target chain (FACT)

| Step | Where | What it does |
|---|---|---|
| Score | `functions/AIComputePassScore_0042a780.c` L3-4 | `TireForceSolver(...)`, then `return (a2*a1[4417] + a1[4416]) * a1[4448] * a1[4431] * a3[33]`. Indices: 4417=0x4504 mult, 4416=0x4500 add, 4448=0x4580 wet, 4431=0x453C pace, a3[33]=wp+0x84 path value. |
| asm check | `D1_CORNER_SPEED.md` section D1.1 (capstone) | Same operand order confirmed at 0x42A7A1..0x42A7B9. The Hex-Rays `a2@<st0>` is the solver return value (dropped-st0 issue, finding `hexrays-dropped-st0-ai-brain-2026-10-02`). |
| Solver | `functions/TireForceSolver_004e8ce0.c` L80 (clamp `v8` to 128), L203, L206, L210 (return 128) | Root-finder over the car's own tyre tables (`this` = veh+0xC). Speed bounds 128 are not active at corner speeds. |
| Waypoint target | `functions/AIRacingLine_00425920.c` L205, L209, L234, L259, L263-264, L298-299 | L205 compares score to speed*1.05 (`v26`). L209 uses score*0x4554 (`v27`) as the first target. L234 loop break `v46 >= v50`. L298-299 store the limiting waypoint (0x43F8) and its target (0x43FC). |
| Live confirmation | finding `ai-brain-corner-speed-live-confirmed-2026-10-02` (OBSERVED), `D1_CORNER_SPEED.md` | Monza dry: median meas/pred 0.9993, n=3549; 100 % within +-2 %. Wet Imola (D11a): median 0.9993, n=733. So the formula is live-confirmed on two tracks. |
| Mult default | `functions/AIDriverStructInit_00424790.c` L55-62, parser `docs/12` and `ai_interaction_role/README.md` L1562-1563 | 0x453C=1.0 by default. CORNERINGMULT and CORNERINGADD come from the driver profile through `sub_424080`. |

DERIVED: the planned corner speed is linear in mult. For a fixed track point, target(mult) = mult * S + add, where S is the
solver speed times wet times pace times path value. The owner's x0.97 result (+0.80 s/lap) is consistent with the
planner binding below the physical limit at x0.97.

## 3. Where the target is used, and where it is clamped

### 3a. Planner-side use (AIRacingLine)

- The braking look-ahead (L234-L299) walks waypoints forward. Each step uses decel `v -= len/v * 0x44CC * TireForce(v,...) * 0.915`
  (L263-264; FACT via `docs/54` section 3b, correction 2026-10-02, live-confirmed median 1.000 n=1936, and D11 (b) CONFIRMED n=2042).
- Hex-Rays prints `v32` (the corner target) in that formula at L264. The doc correction says this is a dropped-st0
  artifact and the true factor is the TireForce return. The live replay supports the correction. OPEN only if a new
  disassembly check disagrees.
- The brake/throttle outputs come from L302-L346: `TireGripCoefficient` for brake amount (0x4464, ramp 0.2/tick, L343-346),
  and `BrakeThermalModel` or 1.0 for throttle (L309-339).

### 3b. Mult in the braking decision (answer to the "uncapped braking" question)

- Mult does enter braking, but only through the gates:
  - L205 `v26`: a waypoint whose score is below 1.05*speed sets the brake flag used at L307.
  - L209 `v27`: the first limiting target is score*0x4554 (0x4554 = 1.0 in every sample).
  - L234 loop break `v46 >= v50`: the look-ahead stops earlier or later depending on the target.
- So a higher target can change where the look-ahead starts braking. It does not change the decel constant.
  Brake points are therefore target-gated, not uncapped physics. DERIVED.

### 3c. Clamps on the target (FACT: none on the corner path)

- `0x4554` multiplies the target by 1.0 in all samples (`D1_CORNER_SPEED.md` section D1.3). No clamp.
- Pit and linked-car bypasses (L245, L255): `flt_8ACFB4` (pit speed) and fixed 12.0. These only apply to pit or chained waypoints.
- The solver's 128 bound (see section 2) and the sub_426430 cap of 75 (section 3d) are both above normal corner speeds.

### 3d. sub_426430 (AIWaypointSteering), the affine term and a 75 cap

- `functions/sub_426430_00426430.c` L139-142: `v18 = speed*0x4504 + 0x4500`, capped at 75.0. Same affine term as the planner.
- L144-145: when the state is 1, `v18 = flt_8ACFB4`.
- L146-149: ratio `0x4444 = speed / v18`, capped at 1.0.
- L151-167: band `0x4468` set by speed thresholds 15 / 30 / 65.
- The ratio at 0x4444 is read only by the steering governors at `functions/AISteering_004270a0.c` L696-697 and L737 (test `> 0.5`).
  See section 3f.

### 3e. Speed field 0x4538 (veh+0x4538, "speed_cap_ms" in memory MCP) is NOT on the corner-target path

- Default: `functions/AIDriverStructInit_00424790.c` L62 writes 1232348144 = **999999.0** to 0x4484 (my decode; the
  `.md` says 1,032,957.0f and the header says 999999.0f, so the doc is wrong in one place), and `docs/48` says 0x4538 holds
  a sentinel. Check the value before quoting.
- Writers of 0x4538: `functions/ParameterValueSetter_0042acd0.c` L3-6 writes `flt_8ACFB4` for command 18, otherwise the integer.
  Callers: `AIAbility_00429470.c` L139 (pit path), `AICommandMessageQueueDispatcher_00424cd0.c` L38, and
  `CommandQueueResponseHandler_00429ee0.c` L14, L20 (99999), L26, L37. So 0x4538 is a pit or caution speed cap set by the command queue.
- Readers: `functions/AISteeringCorrection_00426cf0.c` L80 calls `TrackParameterMinimumClamp(0x4538)`, which clamps 0x4484 to
  `[5, 0x4538]` (`functions/TrackParameterMinimumClamp_0042a740.c` L3-6). 0x4484 then sets brake bands at L81-100.
- So 0x4538 caps the brake-trigger speed 0x4484, not the planner target 0x43FC. It binds only if a pit or caution command left it
  low (for example 18 = flt_8ACFB4). DERIVED: it is not the mult cap in normal racing.
- Note: `docs/12-globals-reference.md` L807 calls flt_8ACFB4 "IdleSpeedTarget"; AIRacingLine uses it as pit speed.
  The name is OPEN and does not change the verdict.

### 3f. The governor family on 0x4484 (FACT that the code exists; OPEN whether it binds)

- `functions/AISteering_004270a0.c` has about 14 `TrackParameterMinimumClamp` sites (L589, L597, L603, L700, L741, L758,
  L1066, L1075, L1124, L1183, L1198, L1281, and more). Each sets 0x4484 = min(0x4484, x).
- The arguments are speed-based allowances, for example `(distance field v33[4195] + speed)*0.8` (L587-588),
  `*0.9` (L596), `speed - 5` (L757), `speed - 1.5` (L1123), and `speed*0.8` (L699, L740).
  Here `this+4368` is speed (0x4440, `functions/AIRacingLine_00425920.c` L58 uses 17472 = 0x4440 as current speed).
- The speed-ratio gate at L696-697 and L737 is `0x4444 > 0.5`. It contains mult. A higher mult lowers the ratio
  (speed/(mult*x+add)), so the gate fires less often at higher mult. This is a mult-coupled path that is not a plain cap.
- AISteeringCorrection then uses 0x4484 at L81-100:
  - speed > 0x4484+10 -> full brake (0x4464 = TireGripCoefficient), throttle 0;
  - +3 .. +10 -> half grip brake;
  - +1 .. +3 -> throttle 0;
  - 0 .. +1 -> brake 0.8*BrakeThermalModel.
- DERIVED: if a governor fires often, it limits speed in a way that depends on current speed and geometry, not on
  the planner target. That would give the plateau the owner sees. OPEN: which clamp sites fire at Monza, and whether the
  ratio gate changes the fire rate between x1.00 and x1.10. Needs the v33[4190..4195] driver-record fields decoded.

### 3g. Throttle path clamps (FACT on the code; their binding is OPEN)

- `functions/AISteeringCorrection_00426cf0.c` L22-23: if 0x45FC is set, 0x457C = AIDifficultyScalar*0.75. Note the call
  reads sub_424750 (`functions/AIDifficultyScalar_00424750.c` L3-6: 1.0 at strength >= 95, else `0.75 + 0.01*(s-70)`).
- `functions/AIAbility_00429470.c` L12, L16, L31, L35, L78 write 0x457C as scalar (normal, about 0.97 at strength 92, 0.98 at 93,
  1.0 at 95+) or scalar*0.75 in the RingBuffer==2 path. So the normal cap is 0.97-1.0, not 0.75.
- L131-133: throttle 0x4460 is clamped to 0x457C whenever byte_5C3160 is clear (not at race start).
- L24-28: throttle is scaled by `(1 - 0.7*|0x444C|)` when speed > 20 (steer-dependent corner-exit cut).
- So there are two throttle limiters: a strength-dependent ceiling (about 3 % at strength 92) and a steering cut on
  corner exit (up to 70 % at full lock). Neither depends on mult.
  DERIVED: they limit acceleration out of corners and on straights, which saturates the lap-time gain once speed is no longer planner-limited.

### 3h. Physical limit (the top candidate)

- Corner target speed = `(solver * mult + add) * wet * pace * pathValue`. At mult = 1, add = 0, wet = 1, pace = 1 the target is
  the solver's tyre-limited speed S (`TireForceSolver` uses per-car tables this[3740], this[3397], this[3855..3873]).
- `functions/TireForce_004e9100.c` (sub_4E9100, used as the braking-step TireForce) reads the same table fields
  (this[3397], [3398], [3740], [3855], [3856], [3871], [3872], [3919], [3921]). So the planner and the braking step use one model.
- DERIVED: at mult above roughly 1/0.93 in force or 1/sqrt(0.93)=1.037 in speed, if the solver tables are built with
  the PLR AI Corner Grip Usage factor (0.93), the planned target exceeds the speed the physical tyres can hold. Then extra
  mult only makes the car ask for speed it cannot get. The planned target stops binding and the physical limit takes over.
  This matches the owner's knee near x1.03-1.04. OPEN: the link between the PLR key and the solver tables is not proven
  (see section 5), and the owner's test (grip usage 1.00 gave no effect) argues against it unless the key was not reloaded.
- Physics-side lateral force (`functions/TireGripLateralForceCalculation_004e9300.c`) reads offsets 14960, 15420-15552, 15684
  relative to its own base, which I could not confirm equals veh+0xC. OPEN.

## 4. Candidate caps, ranked

Scale: FACT = code seen; DERIVED = inference; OPEN = not resolved.

| Rank | Candidate | Where | Why it could give a plateau | Evidence | Confidence |
|---|---|---|---|---|---|
| 1 | Car's tyre-limited speed (physics ceiling) | 3h; TireForce_004e9100; TireForceSolver | Planner asks for more than the tyres can hold. Mult stops adding speed once the physical limit binds. | DERIVED, shared tables FACT, PLR link OPEN | Medium-low, about 40 % |
| 2 | Speed governors on 0x4484 with 0x4444 ratio gate | AISteering_004270a0 L589-1281, L696-700, L737-741; AISteeringCorrection L80-133 | Brake and throttle triggers depend on current speed and geometry; the ratio gate is mult-dependent, so fire rate changes with mult. | FACT code, binding OPEN | Low-medium, about 25 % |
| 3 | Throttle ceiling and steer cut | AISteeringCorrection L22-28, L131-133; AIAbility L12-78 | Caps acceleration out of corners and on straights (ceiling 0.97-1.0 at strength 92-95, steer cut up to 70 %). Independent of mult. | FACT code, binding OPEN | Low, about 15 % |
| 4 | PLR AI Corner Grip Usage (0.93) | PlayerOptionsInitializer L1104-1108; docs/42 L64 | If it scales the solver tables, it moves the physics ceiling. | Consumer not found (H3 section 5 also OPEN). Owner test (1.00 gave no effect) argues against. | Low, about 5 % |
| - | veh+0x4538 maxSpeed | ParameterValueSetter L3-6; AISteeringCorrection L80 | Caps 0x4484 only; sentinel 999999 or 99999 in normal racing. | FACT, ruled out for normal racing | Ruled out (check live value only if a pit or caution state was active) |
| - | 0x4554 mult | D1 section D1.3 | Constant 1.0. | FACT | Ruled out |
| - | sub_426430 75.0 cap | sub_426430 L141-142 | 75 m/s is above corner speeds at Monza; only affects the band and the 0x4444 ratio. | FACT | Ruled out for corners |
| - | Solver 128 bound | TireForceSolver L80, L203-210 | Above corner speeds. | FACT | Ruled out |
| - | flt_8ACFB4 pit speed | AIRacingLine L245; sub_426430 L144-145 | Only for pit or pit-state waypoints. | FACT | Ruled out for corners |

## 5. Braking question (answered in 3b)

Braking uses mult only through the target gates. Brake points are not from an uncapped physics limit.

## 6. Data check

- Owner numbers: x1.03 -0.59 s/lap, x1.06 -0.72, x1.10 -0.55, x0.97 +0.80.
  The x1.03 number already shows a gain, so the premise of "no extra speed above 1.03" is not what the table says.
  Non-monotonic results at 1.06 and 1.10 suggest noise of about 0.1-0.2 s/lap. OPEN: number of runs and spread are not given.
- A plateau of about 0.55-0.72 s/lap for mult 1.03-1.10 is consistent with candidates 1-3 (section 4).
  It does not separate them.
- PLR defaults in code (FACT, `PlayerOptionsInitializer_004aa270.c` L1104-1108): brake power 0.98, brake grip 0.97,
  corner grip 0.93 (min 0.1, max 2.0). AI Max Load default = 40000.0 and AI Min Radius default = 20.0 (decode of
  1193033728 and 1101004800). `docs/42` and `AI_PLR_PERFORMANCE_TABLE_NOTES.md` say 20000 and 17.3. The notes may be from an
  older or different PLR file. OPEN: check which values the owner's PLR holds.

## 7. What would settle it (read-only static next steps, then owner-run live tests)

Static (no game needed):
1. Decode the consumer of the PLR floats in `PlayerOptionsInitializer` (the option registry lookup, and any reader of
   the AI Corner Grip Usage value). Check whether the tyre tables that TireForceSolver reads (veh+0x3A7C..0x3D50) are
   built with it. This is the test for candidate 1 and candidate 4.
2. Confirm whether `TireGripLateralForceCalculation` shares the same base as the solver tables (veh+0xC). This is the test
   for the physics ceiling.
3. Decode AISteering driver-record fields v33[4190], [4192], [4195] and find which governor sites write 0x4484 at Monza,
   and whether the 0x4444 gate changes with mult. This is the test for candidate 2.
4. Decode AISteeringCorrection `0x444C` (steer) and confirm the 0x4460 throttle definition (candidate 3).

Live (owner or a separate approved run; not done here):
- Record 0x4484, 0x4444, 0x43FC, 0x4538 and speed at 10 Hz for the same lap at mult 1.00, 1.06 and 1.10. If 0x4484 binds
  (speed reaches 0x4484 and brake bands fire), candidate 2 holds. If 0x43FC > speed for many ticks at 1.06+ and speed
  stays flat, candidate 1 holds. If 0x457C < 1 and throttle sits at 0x457C, candidate 3 holds.
- Repeat each setting at least 3 times to get the spread.

## 8. Proposed findings (NOT appended; read-only rule)

- id `ai-corner-target-clamp-search-2026-10-15`, tier FACT: no static clamp on the corner target inside AIComputePassScore
  or AIRacingLine. Corner target is ungated apart from 0x4554 = 1.0. Evidence: this report sections 2-3c.
- id `ai-speed-governor-0x4484-2026-10-15`, tier FACT (code) / OPEN (binding): about 14 TrackParameterMinimumClamp sites
  in AISteering_004270a0 set 0x4484, consumed by AISteeringCorrection L80-133. Mult enters via the 0x4444 ratio gate (L696, L737).
- id `ai-throttle-ceiling-0x457c-2026-10-15`, tier FACT: throttle ceiling = AIDifficultyScalar (about 0.97-1.0 at strength 92-95, not 0.75 as docs/54 says).
- id `plr-ai-corner-grip-consumer-open-2026-10-15`, tier OPEN: PLR AI Corner Grip Usage default 0.93 has no static consumer found.
- Correction candidates: docs/42 L62-64 "physics cap" is OPEN, not FACT; `docs/12` L807 IdleSpeedTarget name conflicts with pit use of flt_8ACFB4;
  `AIDriverStructInit_00424790.md` line on 0x4538 gives 1,032,957 but the bits decode to 999999.0; AI_PLR notes give 20000 / 17.3 but code default is 40000 / 20.0.

## 9. Limitations

- No live data. No game run. No re-decode of the solver tables. Nothing here is live-verified.
- Hex-Rays st0 drops are handled by citing capstone-verified asm only where D1 already did so.
- Two candidates (1 and 2) are both plausible, and this report does not rank them above a 40 % confidence.
- Documentation gate: this report is the durable record. OPEN: add an index entry in `investigations/adaptive_ai_difficulty/`
  and append section 8 entries to `findings.jsonl` when the owner allows writes.
