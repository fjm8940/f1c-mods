# H1: Who reads AIW [WAYPOINT] MidAdjust / BestAdjust / RaceQualRatio (static, 2026-10-15)

Scope: static corpus only (functions/, docs/, findings.jsonl, ask.py). No game, poke, lab, or memory-mcp tool was run.
Workspace validator (`tools/workspace/check-workspace-structure.ps1`) ran read-only: 0 warnings, 2 pre-existing errors (`.claude` and `plans` top-level dirs). Not caused by this task; not changed.

Labels: FACT = code shows it directly. DERIVED = inference from code. OPEN = not resolved statically. CONFLICT = corpus sources disagree.

## Answer in brief

1. The object is the AI database at global `dword_5EA2C0`. FACT (functions/Game_Init_0042d6b0.c:11).
2. AIW values land in that object at offsets +0x11C (WorstAdjust), +0x120 (MidAdjust), +0x124 (BestAdjust), +0x128 (RaceQualRatio). FACT (functions/sub_402690_00402690.c:240-248).
3. AIDatabase_Init computes a derived scalar at +0x118 from the AI difficulty slider and these three values. FACT (functions/AIDatabase_Init_004040a0.c:53-63).
4. The only static reader of the three AIW values is that init-time formula. No code reads +0x120/+0x124/+0x128 from the database at runtime. OPEN (search scope in section 5).
5. Runtime AI difficulty code (AIDifficultyScalar, AIDifficultyInterpolation) reads the slider global `dword_7D01A0` plus separate globals `flt_5EA3DC/E0/E4`. It does not read the database object. FACT.
6. Qualifying vs race: the only session gate on the difficulty interpolation is `SessionType not in {3,4,5}` at AIDriverStructInit line 129. Session 5 = qualifying and 7 = race are FACT from live reads recorded in findings (see section 4). DERIVED: the 0.995 factor is skipped in qualifying and applied in race.
7. RaceQualRatio: no reader found anywhere in functions/. It is stored and never read. DERIVED, scope-limited.

Confidence: high for the writer and storage facts; medium for "no runtime reader of the AIW values"; low for how the difficulty value reaches actual AI pace.

## 1. The AIDatabase object

- Game_Init calls `AIDatabase_Init((float *)dword_5EA2C0)`. FACT (functions/Game_Init_0042d6b0.c:11). Only caller of AIDatabase_Init (0x4040A0). FACT (`ask.py func 0x4040a0`, callers: Game_Init 0x42d6b0).
- The same global is passed to the waypoint and spatial-tree helpers (`SpatialNearestNeighborQueryWithPlaneTest(dword_5EA2C0, ...)` in sub_426430_00426430.c:78; `sub_403640(dword_5EA2C0, ...)` in Camera_004a3b50.c:62; `sub_403CD0(dword_5EA2C0, ...)` in TrackSplineExtrapolation_004349a0.c:71). FACT. This matches docs/52-aiw-navigation-static-architecture.md section for the database header (lines 140-153).
- CONFLICT: `ask.py global 0x5ea2c0` names it `GarageTableOwner` and cites a garage-table finding (findings.jsonl, VEHICLE_LIFECYCLE_REVIEW_2026-09-08). The code shows it is the AIW database (Game_Init:11). Treat the garage name as a stale label. Not resolved here.
- Object layout (float index = byte offset / 4), per docs/52 lines 140-153 and AIDatabase_Init:
  - +0x118 (float index 70): difficulty-interpolated scalar. FACT (AIDatabase_Init:63 writes `this[70]`).
  - +0x11C (71): WorstAdjust, clamped at least 0.5. FACT (sub_402690:240-241 and AIDatabase_Init:29 default 0.5).
  - +0x120 (72): MidAdjust. FACT (sub_402690:240, default AIDatabase_Init:30 = 1.0).
  - +0x124 (73): BestAdjust, clamped at most 1.3. FACT (sub_402690:243-245; default AIDatabase_Init:31 = 1.3).
  - +0x128 (74): RaceQualRatio. FACT (sub_402690:248 stores the value at `this+296`). Default: none set in AIDatabase_Init (no write to this[74]).

## 2. Writers and parser

- sub_402690 parses the key names with `sub_565870` (float) and writes into the object `this`. FACT. Key strings: `MidAdjust` 0x5BE5C4, `BestAdjust` 0x5BE5B8, `RaceQualRatio` 0x5BE5A8 (skeletons_filled/sub_402690_00402690.md:81-83).
- Clamps: WorstAdjust `< 0.5 -> 0.5` (sub_402690:236-238); BestAdjust `> 1.3 -> 1.3` (sub_402690:244-245). FACT. MidAdjust and RaceQualRatio have no clamp. FACT.
- Parser is called once: AIDatabase_Init:50 `sub_402690((int)this, v5)` when the database file opens. FACT.
- Derived scalar: AIDatabase_Init:53-63.
  - Slider `dword_7D01A0 > 100`: `this[70] = (1 - r*0.05)*Mid + r*0.05*Best`, with r = slider-100. FACT.
  - Else: `this[70] = (1 - r/30)*Worst + (r/30)*Mid`, with r = slider-70. FACT.
  - DERIVED: this is an interpolation from Worst (slider 70) to Mid (100) to Best (120). docs/52 line 83 reports one live check: slider 92, Worst 0.85, Mid 1.0 gives 0.96 (absolute error 2.6e-8). OBSERVED per docs/52.
- Timing: the value is computed once at Game_Init from the slider at that moment. DERIVED. Game_Restart does not call AIDatabase_Init (only Game_Init does). FACT by callers list.

## 3. Readers

### 3a. Readers of the database object offsets +0x118..+0x128

- Static grep over functions/ for `[70]..[74]` on a float array and for `+280..+296` / `+0x118..+0x128` (excluding the parser and init files) returns no reader that is provably the AIDatabase object.
- Hits that look similar are on other structs: AIWaypointPathCheckerRenderer_004024f0.c:33-35 (waypoint node, `+280` = wp_galpha-related), SurfaceHeightfieldCollisionCallback_00404250.c:79-115 (writes `v12+296` for a waypoint record, not the database).
- Accessors that receive the database pointer (sub_403640, sub_403CD0, ArrayOfStructIndexedFieldCopy, IndexedStructDataRead, PhysicsNodePropagationWithDistanceFalloff, ShortComputation_45f0 family) show no read of +0x118..+0x128 in their bodies (sub_403640 read only partially, lines 1-70 reviewed). DERIVED.
- OPEN: the database pointer can also be cached in another struct and read through that alias. Not searched exhaustively.

### 3b. Readers of the RaceQualRatio value specifically

- `RaceQualRatio` appears in functions/ only in the parser (sub_402690:248) and its string. FACT.
- No function reads database offset +0x128. DERIVED (scope as in 3a).
- docs/52 line 153 already says "Scale pending runtime verification" for +0x128. Consistent.

### 3c. Readers of the derived scalar +0x118

- No static reader found (scope as in 3a). OPEN.
- Live-only link to AI pace is not established by code. See section 6.

## 4. How the difficulty value enters AI speed (code path that exists)

### 4a. Slider and the runtime difficulty functions

- `dword_7D01A0` = AIDifficultySlider (readers: AIDatabase_Init, sub_424080, AIDifficultyScalar, AIDifficultyInterpolation, AIRingBufferFieldInit, AIBehaviorInitialization, sub_4C29F0). FACT (`ask.py global 0x7d01a0`).
- `AIDifficultyScalar` (0x424750):
  - slider >= 95 returns 1.0; else `0.75 + (slider-70)*0.01`. FACT (functions/AIDifficultyScalar_00424750.c:3-6).
  - Callers: sub_424080 (line 176, stored into driver `this[4447]`), AISteeringCorrection (0x426CF0), AIAbility (0x429470). FACT (`ask.py func 0x424750`).
  - AIAbility applies x0.75 on the cool-down/yellow state (AIAbility_00429470.c:31, 35, 76-78). FACT. Context: findings `[CONFIRMED]` yellow-segment cap, and `[FACT]` cool-down = 0.75 cap (findings.jsonl, development/mods/coop_mode log).
- `AIDifficultyInterpolation` (0x424AC0), the same piecewise shape as AIDatabase_Init:53-63 but using globals instead of the database:
  - slider > 100: `E0 + (E4-E0)*(slider-100)*0.05`. FACT (AIDifficultyInterpolation_00424ac0.c:5-7).
  - else: `DC + (E0-DC)*(slider-70)/30`. FACT (:9).
  - Globals: `flt_5EA3DC` (DifficultySlopeLow), `flt_5EA3E0` (DifficultyMidValue), `flt_5EA3E4` (RaceAbility_HighRange). Their names say "AIW WorstAdjust / MidAdjust / BestAdjust". CONFLICT: names are from `ask.py global` and docs/12 lines 6692, 6977. Static writers: none found. AIDatabase_Init does not write these addresses (it writes `this[71..73]`, i.e. the database object). So the link from the AIW values to these globals is NOT shown by code. OPEN.
  - Single caller: AIDriverStructInit (0x424790), line 128. FACT.
- AIDriverStructInit 128-131:
  - `v19 = AIDifficultyInterpolation();`
  - `if (dword_7C56A4 != 3 && != 4 && != 5) v19 = flt_5EA3E8 * v19;` (flt_5EA3E8 = 0.995 per FunctionCallWrapper_00401080.c:21). FACT.
  - `return ScaleAudioPitch(v19);`. The name ScaleAudioPitch is a decompiler label that looks wrong for this use. CONFLICT/OPEN: what consumes this return value is not traced here (H3/H4 may cover it).

### 4b. Session gate (qualifying vs race)

- SessionType global `dword_7C56A4` (FACT, `ask.py global 0x7c56a4`).
- Enum values from live reads recorded in findings.jsonl (FACT, entry dated 2026-07-28): 7 = race, 5 = qualifying.
- AIDriverStructInit:129: the 0.995 factor is skipped when session is 3, 4 or 5 (so qualifying = 5 is skipped), applied otherwise (race = 7 included). DERIVED.
- sub_424080:177: `if (dword_7C56A4 == 7 && dword_7CEA5C > 0)` gates a race-only branch that sets driver fields `this[4177]/[4178]` (values 0/4/5) from random draws. Not a speed term. FACT for the gate; the meaning of those fields is OPEN.
- AIBehaviorInitialization:40 `if (dword_7C56A4 != 7)` and :83 `dword_7C56A4 == 7 ||` are other session branches. Not related to the three AIW values. FACT.
- Net: none of the three AIW values is read with a session condition in the code found. RaceQualRatio, which by its name would be a race/qualifying ratio, has no reader at all. DERIVED.

### 4c. Pace scalar (where the value would have to act)

- Findings `ai-pace-formula` (DERIVED, not runtime verified) and `0x44F4-pace-scalar` (FACT, poke-proven): veh+0x44F4 = (1 - RaceAbility/100) * flt_5EA3D8. FACT of code at sub_424080:148-151 (reads `flt_5EA3D8`).
- `flt_5EA3D8` = "GlobalDifficultyScalar" in docs/12. Static writers: none found; readers: sub_424080 only. Its value must come from .data or a runtime write. OPEN.
- The README `investigations/adaptive_ai_difficulty/README_2026-10-14.md` says GDS is "interpolated from track .AIW WorstAdjust/MidAdjust/BestAdjust". This is NOT shown by code: the code that uses GDS is `flt_5EA3D8`, not database +0x118. CONFLICT with that README. The live observation (slider 92 -> 0.96 in docs/52) matches database +0x118, not necessarily GDS.

## 5. Search scope and limits

- functions/: all .c files grepped for `dword_5EA2C0`, `dword_7D01A0`, `flt_5EA3D8/DC/E0/E4/E8`, `dword_7C56A4`, `[70..74]`, `+280..+296`, `+0x118..+0x128`, `RaceQual`, `times`.
- Not done: exhaustive dataflow from every reader of dword_5EA2C0 (32 + 17 readers per `ask.py global`); the 17 truncated readers were not listed. This is the main open gap for 3a.
- Not done: the consumer of AIDriverStructInit's return value (4a).

## 6. Lead from coordinator: the `times=` header key

- `.aiw [WAYPOINT] times` appears in 198 shipped files (DATA_INDEX.md; range -0.5914..huge). `multilinetimes` in 6 files (`ask.py token times`). FACT (shipped-data index, not re-read here).
- sub_402690 does not contain a string literal for `times` (grep of the function and its skeleton found none). FACT. So the parser does not read it. DERIVED: unknown keys are ignored.
- No function in functions/ reads a `times` AIW key. The only `aTimes` is a season-data lookup in sub_47D800 (skeletons_filled/sub_47D800_0047d800.md:20), a different key. FACT.
- Therefore `times` is NOT consumed alongside MidAdjust/BestAdjust/RaceQualRatio in the code found. OPEN: whether a different parser (not sub_402690) consumes it is not searched.
- Live AI lap ~71 s (coordinator) is not checked here.

## 7. Open items and follow-up

1. OPEN: what reads database +0x118/+0x120/+0x128 through a cached pointer. Next step: trace readers of dword_5EA2C0 (the 17 truncated) and check whether any stores the pointer into a driver struct.
2. OPEN: whether AIDatabase_Init opens a per-track .aiw or a fixed file (sub_587DB0/sub_587DD0 path build, AIDatabase_Init:43-50). Only Game_Init calls it, so a per-track value would be stale after a track change unless another path reloads. Matters for any "AIW value drives AI" claim.
3. CONFLICT: names of flt_5EA3DC/E0/E4 as AIW copies vs no static writer. Next step: find the .data initial values or a runtime writer before trusting the name.
4. CONFLICT: GDS in the README versus flt_5EA3D8 in the code. Next step: correct the README wording or find the writer of flt_5EA3D8.
5. CONFLICT: `GarageTableOwner` label on 0x5EA2C0 versus AIW database use. Next step: correct the label in name_index/globals docs.
6. OPEN: what consumes AIDriverStructInit's return (`ScaleAudioPitch` label suspicious).
7. OPEN: RaceQualRatio has no reader. If the game ignores it, mods changing it do nothing. Next step: runtime test is needed; not done (no runtime tools per brief).

## 8. Files consulted

- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\sub_402690_00402690.c (lines 236-248, 320, 358-360)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\AIDatabase_Init_004040a0.c (lines 29-31, 43-63)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\Game_Init_0042d6b0.c (line 11)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\AIDifficultyScalar_00424750.c
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\AIDifficultyInterpolation_00424ac0.c
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\AIDriverStructInit_00424790.c (lines 128-131)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\sub_424080_00424080.c (lines 148-158, 176-177)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\AIBehaviorInitialization_004bf0a0.c (lines 35, 40, 83)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\functions\FunctionCallWrapper_00401080.c (lines 16-21)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\docs\52-aiw-navigation-static-architecture.md (lines 83, 140-153)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\docs\12-globals-reference.md (lines 6692, 6977)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\findings.jsonl (ai-pace-formula, 0x44F4-pace-scalar, SessionType entries)
- %USERPROFILE%\Desktop\F1\f1c_ida_decompiled\investigations\adaptive_ai_difficulty\README_2026-10-14.md

## 9. Documentation gate

- Record: this file, in the component's investigation folder.
- Not added to findings.jsonl in this pass. AGENTS.md asks for new results to be appended there, but the brief limited writes to this one report file; the owner should decide. Pending validation: items 1-7 in section 7.
- SDK front door `python tools/f1c_mod.py start --json` was run (read-only). Its checkpoint says stale: true; next objectives from it were not used.
- No maintained file was changed. No game, runtime, or preserved file was modified.
