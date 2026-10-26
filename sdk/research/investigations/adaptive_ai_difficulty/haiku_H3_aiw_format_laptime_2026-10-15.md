# H3: AIW waypoint format and offline AI lap-time feasibility (2026-10-15)

Owner question: what does an .AIW hold per waypoint, does it carry speed/brake
targets, and can an offline tool estimate an AI lap from AIW + car files?
Method: static only. Read `functions/sub_402690_00402690.c` (parser), the AI
consumers, the shipped Austria 2001 and Albert Park 2001 AIW files, and the
2001 Ferrari car files. The game was not run; no game, runtime or mod file was
touched. Labels: FACT (read in file or code), DERIVED (inferred from FACT),
OPEN (not resolved here).

## 0. Files read (absolute)

- AIW Austria: `%USERPROFILE%\Desktop\F1\F1 Challenge V10\SeasonData\Circuits\Austria\2001_A-1_Ring\2001_A-1_Ring.aiw` (FACT)
- AIW Australia: `%USERPROFILE%\Desktop\F1\F1 Challenge V10\SeasonData\Circuits\Australia\2001_Albert_Park\2001_Albert_Park.AIW` (FACT)
- Car: `...\SeasonData\Vehicles\Ferrari\2001_F2001\2001_Ferrari.hdv`, `engine.ini`; tyre `...\SeasonData\Vehicles\2001_Bridgestone.tbc` (FACT)
- Parser: `f1c_ida_decompiled\functions\sub_402690_00402690.c` (379 lines)
- Consumers: `functions\AIComputePassScore_0042a780.c` (5 lines), `functions\AIRacingLine_00425920.c`, `functions\TireForceSolver_004e8ce0.c`, `functions\F_3DPositionDeltaComputation` = `sub_4FAEC0`, `functions\IndexRemappingWithStride_00403540.c`
- Option setter: `functions\PlayerOptionsInitializer_004aa270.c` L1103-1108 (FACT)

## 1. AIW file layout (FACT)

Plain text, `key=value` lines, `//` comments allowed (Austria L1-L232). Sections:
`[Features]` (L1), `[GRID]` (L7), `[PITS]` (L81), `[Waypoint]` (L192). The parser
is key-driven, so line order within a record does not matter.

Austria 2001 header values (`2001_A-1_Ring.aiw`):

| Key | Example | Parser / note |
|---|---|---|
| pitlanes / startinggrid / pitspots / garagespots | 1 / 104 / 52 / 3 | [Features] L2-5; clamped to minimums in sub_402690 (L~36-44) |
| GridIndex, Pos, Ori | 0, (-5.321,-11.444,-54.841), (-0.010,-3.119,0.000) | [GRID]; grid pose; Pos y ~ -11.4 (height) |
| TeamIndex, PitPos, PitOri, GarPos, GarOri | per-team pit/garage poses | [PITS]; sub_402690 L~130-165 |
| number_waypoints | 1070 | L195 `aNumberWaypoint`; record count |
| lap_length | 4281.66 m | L249 `aLapLength` |
| sector_1_length / sector_2_length | 1225.97 / 3369.11 | L250-251; **sector convention OPEN** (see 6) |
| GrooveWidth / GrooveWidthWet | 8.0 / 4.15 | L253-254 |
| FuelUse | 93446.09 | L252 |
| LeftHandedPits | 0 | L~228 |
| WorstAdjust / MidAdjust / BestAdjust | 0.85 / 1.0 / 1.1 | L235-243 (clamped >=0.5 and <=1.3); AI strength 70/100/120 (per the file comments) |
| RaceQualRatio | 0.988 | L248; "slow AI slightly in race" (file comment) |
| FogDensity, RainyFogPlanes/Color/Darkness, IntermediateFog* | weather look | L255-270 region |
| trackstate, times, Best/Mid/WorstTime, CheatDelta | 499; (98.16,94.84); 0; (0,0,0) | Not read by sub_402690 (no xref in its body). Meaning OPEN |

Albert Park 2001 (`2001_Albert_Park.AIW`): number_waypoints=1170, lap_length=5290.84 m,
1170 `wp_pos` lines (FACT). Austria: 1070 `wp_pos` lines (FACT). The record count
equals `number_waypoints` in both files.

## 2. Per-waypoint record (FACT, parser sub_402690)

One record per `wp_*` block, ending at the `WP_PTRS` line (L320). Austria record 0
is at AIW L218-L229 (`wp_pos` at L218, `WP_PTRS` at L229). Record stride is 316 bytes
(79 DWORDs) (alloc `316*count+4`, remap L15+). Offsets below are `v4[i]` in the
parser times 4 (byte offset in record).

| Byte off | Key (AIW) | Example (Austria rec 0) | Parser line | Consumer / note |
|---|---|---|---|---|
| 0x00-0x08 | wp_pos (x, height, z) | (-2.4833, -10.5696, -189.9698) | L304 | Copied again to 0x94-0x9C (L305-307). y is height (DERIVED: GRID y ~ -11, constant ~0.098 on other tracks) |
| 0x0C-0x14 | (none) | | | Delta to linked waypoint, written by sub_4FAEC0 (F_3DPositionDeltaComputation, L17-40) (DERIVED) |
| 0x18, 0x1C | wp_path (dry, wet lateral) | (-4.1447, -4.4338) | L295 | AIRacingLine reads 0x18 only (see 3); wet use not seen (OPEN) |
| 0x20-0x28 | wp_perp (unit vector, xz) | (0.9990, 0.0014, -0.0447) | L316 | Road direction; |perp| ~ 1 (DERIVED) |
| 0x2C-0x38 | wp_width (roadL, roadR, farL, farR) | (6.200, 9.608, 11.200, 9.608) | L317 | Consumers not traced (OPEN) |
| 0x3C | (none) | | | Segment length read by AIRacingLine L264 (`v5+60`). No setter in parser or remap (OPEN) |
| 0x44 | wp_score[0] | 2 | L314 | Sector label (OPEN, see 6) |
| 0x48 | wp_score[1] | 4136.212 | L314 | Lap distance (m) of this waypoint (FACT: value; DERIVED: meaning) |
| 0x58-0x64 | wp_dwidth (collL, collR, cutL, cutR) | (11.709, 9.608, 0, 0) | L318 | Consumers not traced (OPEN) |
| 0x84 | wp_event[0] | 1.000 | L296 | **Corner speed multiplier** (file comment). Read as `a3[33]` by AIComputePassScore (FACT) |
| 0x110 / 0x10C | (runtime pointers) | | remap L21-25 | Set from WP_PTRS indices (DERIVED) |
| 0x114 | (runtime pointer, 3rd index) | | | Read by AIRacingLine as `v5+276`; setter DERIVED |
| 0x118, 0x11C | wp_galpha (race groove darkness), wp_groove_lat | 0.0; 1.986 | L312-313 | Consumers not traced |
| 0x120, 0x124, 0x128 | wp_ptrs / WP_PTRS (3 used of 4) | (740, 1, 1047) | L320 | Prev index, next index, alt index (DERIVED from remap L21-25). 4th value ignored |
| 0x12C | wp_branchID | 0 (0=main, 1=pit lane, 2+=pit stops) | L294 | FACT (file comment) |
| 0x130 | wp_bitfields | 0 | L319 | Not consumed in this pass (OPEN) |
| 0x134, 0x138 | wp_event[1], wp_event[2] | 0; 981651972 | L301-302 | Special event and data. AIRacingLine reads 0x134/0x138 (`v5+308/312`) for pit bypass |

All 12 `wp_*` keys appear once per record, in every record (FACT).

## 3. Speed, brake and racing-line data

- **Speed or brake target per waypoint: not stored.** The only per-waypoint speed
  field is `wp_event[0]`, a multiplier (1.0 in the samples). FACT.
- **Racing line: stored explicitly as lateral offsets** (`wp_path` dry/wet,
  `wp_groove_lat`, `wp_perp`, `wp_width`). AIRacingLine reads the dry offset
  (`v15+24`, 4 hits). FACT.
- **Brake points: derived at runtime.** AIRacingLine L204-L264 walks forward from
  the current speed. Each step: `v46 -= (len(wp+0x3C)/v46) * this[17612] * target * 0.915`,
  where target = AIComputePassScore(wp) (L259) and this[17612]=0x44CC is the brake
  coefficient. Speed is clamped to 0 below 0.5. FACT (code). Brake trigger
  L204-205 uses `this[17472]*1.05` and the 0x43FC-style target. DERIVED.

## 4. How the AI uses it

- **Corner target (FACT code, `AIComputePassScore` 0x42A780, 5 lines):**
  `ret = (TireForceSolver(...) * a1[4417] + a1[4416]) * a1[4448] * a1[4431] * a3[33]`
  i.e. (grip force * CorneringMult 0x4504 + CorneringAdd 0x4500) * 0x4580 * pace 0x453C * wp_event[0].
  Called from AIRacingLine (L205, 209, 259) and nowhere else in this pass.
- **Tyre force (FACT code, TireForceSolver 0x4E8CE0):** takes |speed|*this[3903],
  interpolates a 14-entry table at this[3904..3917]. The table source is OPEN (see 6).
- **Prior art (in `findings.jsonl`, cited not re-derived):** the corner formula
  was replayed offline on Monza dry (n=3549, median game/model 0.9993, p10 0.9977,
  p90 1.0001) and on Imola wet (n=733, median 0.9993). The tyre tables in those
  replays were logged from game memory, not built from the .tbc. This is the crux.
- **Pace factor (CONFIRMED in findings, not re-verified here):** veh+0x44F4 =
  (1 - RaceAbility/100) * GDS; live 0x453C walks to it. Driver term is per car.
- **Measured sensitivity (MEASURED in findings, one run, Austria 2001):** a live
  x1.03 write to CorneringMult (0x4504) changed AI lap time by about -0.46 s/lap,
  about 0.15 s per 1 %.
- **Simulated-session lap time** (GDB Qualify/Race Laptime, sub_4BE760) is a
  separate path and does not set driven pace (CONFIRMED in findings).

## 5. AI Max Load / AI Min Radius (FACT)

- Keys: `AI Max Load` and `AI Min Radius`, registered in the player options
  initializer, `PlayerOptionsInitializer` L1107-1108. Defaults from the float
  bits: 1193033728 = 0x471C4000 = 40000.0; 1101004800 = 0x41A00000 = 20.0 (DERIVED: decode).
- Saved values in a sample profile: 20000 and 17.3 (`mods\sdk_sandbox\ai_mind_monitor\AI_PLR_PERFORMANCE_TABLE_NOTES.md` L12-13). FACT.
- Notes say they bound the numerical domain of the AI's "theoretical performance
  tables", which feed the racing-line target speed. Table dimensions and the
  consumer chain are NOT recovered (OPEN; the notes say so themselves).
- Not AIW keys. They are `.plr` options. The AIW file has no Max Load or Min Radius entry.
- Adjacent options in the same setter (L1100-1106): AI Power Calibration (0-7),
  AI Additional Fuel, AI Brake Power Usage, AI Brake Grip Usage, AI Corner Grip Usage
  (fractions of "theoretical"). FACT (names); their exact effect is OPEN.

## 6. Car-side inputs (FACT, 2001 Ferrari)

| Input | Where | Value |
|---|---|---|
| Mass (excl. fuel) | 2001_Ferrari.hdv L39 | 601.7 kg |
| Inertia | L40 | (507.4, 578.1, 117.3) |
| CGHeight | L46 | 0.2422 |
| Aero drag | L85 FWDragParams, L101 RWDragParams, L114 BodyDragBase | (0.0147,0.00195,0), (0.0627,0.00733,0), 0.3811 |
| Gears | L249 ForwardGears | 7 |
| Brake torque | L308 (per wheel, at optimum temp) | 3567.5 Nm |
| Tyre brand / compound | L59-60 | 2001_Bridgestone / index 1 |
| Engine torque curve | engine.ini L1-n `RPMTorque=(rpm, c2, c3)` | e.g. (0,-31,-31), (8000,-81.6,284.7). c2 negative = drag or friction; c3 = power (DERIVED, not checked against a comment) |
| Tyre curve | 2001_Bridgestone.tbc L84 `TractiveCurve="Default"`, L69 WetWeather=1 | **Curve values not decoded** (OPEN). DATA_INDEX has no ranges for TractiveCurve |

## 7. Feasibility verdict

**Partial. An offline corner-speed envelope is feasible. An offline lap estimate
that reproduces the game's AI lap is NOT feasible yet.**

Feasible now (FACT inputs exist):
- Track geometry, widths, racing-line offsets, corner multiplier, lap length (AIW).
- Mass, drag, lift, gears, brake torque, engine torque (HDV + engine.ini).
- The corner-speed formula (AIComputePassScore) and the braking step (AIRacingLine).

Missing (OPEN):
1. The TireForceSolver 14-entry grip table: its build from `TractiveCurve` in the .tbc,
   and the role of Max Load / Min Radius in that build.
2. Per-driver factors: CorneringAdd/Mult (0x4500/0x4504), 0x4580 wet factor, pace 0x453C
   (RA, GDS). Live values exist in the .rcd and driver notes, not in the AIW.
3. The segment-length setter for wp+0x3C (computable from wp_pos, so a workaround exists).
4. Origin of wp+0x70-0x78 (TireForceSolver inputs; not parsed, not traced).
5. Sector convention: wp_score sector=2 at lap distance 4136.2 contradicts a
   cumulative sector_2 boundary at 3369.1 (AIW L197-198). Unresolved.
6. Wet path (+0x1C) and width/dwidth consumers: not traced.
7. Traffic, slipstream, pit, fuel and overtake logic: not part of a point-mass estimate.

Expected accuracy (judgment, NOT measured): an envelope-only estimate is likely
to be a few percent off in absolute lap time before calibration. Calibrating
against one logged game lap should bring relative comparisons between cars to
roughly the 0.5 s/lap level. Sensitivity reference: 1 % grip is about 0.15 s/lap
(MEASURED, single run). Confidence in this range: low (OPEN).

Confidence: verdict medium; format facts high (direct reads of parser and
file); accuracy low.

## 8. Recommended next steps (not executed; read-only task)

1. Decode the `TractiveCurve` in 2001_Bridgestone.tbc; compare against the
   game's logged tyre table to close the crux in section 4.
2. Recover the Max Load / Min Radius table builder (follow the
   `MultiFieldSetterWithStringCopy` consumer for `aAiMaxLoad`).
3. Trace the wet path and width consumers (candidates: AISteering 0x4270A0,
   AIAbility 0x429470, AIOvertakeGapCalc 0x47AB35; unverified).
4. Resolve the sector convention with one more AIW (sector_1/2 vs lap distance).

## 9. Documentation gate and state

- Changed files: this report only. No AIW, car file, game file, governance
  file or findings file was modified. `findings.jsonl` was NOT appended, because
  this task is read-only except for this report. Pending: the lead should append
  the FACT and OPEN items from sections 2, 4, 5 and 7 (tiers as stated above).
- Structural validator (`tools/workspace/check-workspace-structure.ps1`) ran
  with 2 pre-existing errors: unexpected top-level directories `.claude` and
  `plans`. Not caused by this task; not changed (structural changes need approval).
- SDK front door `start --json`: `freshness.stale: true` (newest checkpoint 2026-10-13); orientation only.
- Not verified: game runtime behaviour, file hashes, tyre curve values, sector convention, section 7 accuracy.
