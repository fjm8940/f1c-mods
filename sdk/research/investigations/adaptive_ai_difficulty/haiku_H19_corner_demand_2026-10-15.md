# H19: where corner-specific information enters the AI corner target (2026-10-15)

Owner question: AIW waypoint records (316 bytes) feed AIComputePassScore and AIRacingLine. If nothing writes +0x70..+0x7C after construction, corner speed cannot depend on the corner. Find every writer and every entry point for corner information.

Method: static only. The game was not run or read. No file in the corpus, the mod or the workspace was edited, except the one script named in the task. Paths below are relative to `f1c_ida_decompiled\` unless shown in full. Tags: FACT (read in code or docs with line), DERIVED (inference), OPEN.

## 0. Answers

1. FACT: the constructor sets wp+0x70 = 0, +0x74 = 0, +0x78 = 0, +0x7C = -9.81. No writer of +0x70..+0x7F was found in any function. A few writers hit the record family, but none hits this range (section 1).
2. FACT: a3 in AIComputePassScore is an AIW waypoint record. It is reached by the +0x110/+0x114 link chain, and the same record is read for +0x3C, +0x84 and +0x134 (section 2).
3. DERIVED, which corrects H17: wp+0x70..0x7C are not corner-specific. They are constant per car, so TF0 is one number per car. The only per-waypoint input to the corner target is wp+0x84 (wp_event[0]). Corner geometry enters through the braking projection (seglen +0x3C) and through steering (heading +0xB8, cosine samples +0xBC..+0xC8). It does not enter the corner cap (section 3).
4. FACT: the script is at `development\mods\adaptive_ai_difficulty\tools\aiw_waypoint_dump.py`. It was syntax-checked with `ast.parse` only and not run.

## 1. Writers of wp+0x70..+0x7F (question 1)

### 1a. Writes that are in the record family (FACT)

| Writer | Lines | Offsets written (wp+) | Phase / caller |
|---|---|---|---|
| Constructor `sub_4FAD80` | functions/FAILTriageUnknownUnidentifiedButIsActuallyConstr_004fad80.c L30-35 | 0x70 = 0, 0x74 = 0, 0x78 = 0, 0x7C = -1055066685 (-9.81), 0x80 = 0, 0x84 = 1.0 | allocation loop sub_402690.c L209-L215 |
| AIW parser `sub_402690` | functions/sub_402690_00402690.c L223-L225, L296, L304-L320 | 0x84, 0x94..0x9C (copy), 0x110..0x128 (indices), 0x118, 0x11C, 0x12C, 0x130, 0x134, 0x138, 0x0..0x8 (pos), 0x20..0x28, 0x2C..0x38, 0x58..0x64, 0x44, 0x48 | load |
| Fallback 2 records | functions/sub_402690_00402690.c L332-L365 | count 2, constructor per record | load, when no AIW |
| Link remap `IndexRemappingWithStride` | functions/IndexRemappingWithStride_00403540.c L29, L40, L51 | 0x110, 0x10C, 0x114 | load, sub_402690 L366 |
| Chord vector `sub_4FAEC0` | functions/F_3DPositionDeltaComputation_004faec0.c L20-L28 | 0x0C..0x14 | load, sub_402690 L372 |
| Grip phase 0 `sub_4FB260` | functions/sub_4FB260_004fb260.c L14-L25 | 0x94..0xA0 | per tick, MultiPhase L75 |
| Grip phase 1 `sub_4FAF60` | functions/sub_4FAF60_004faf60.c L26-L28, L40-L47, L58-L64, L68, L73, L84-L96 | 0x3C (seglen, L44), 0x40 (L64), 0x50, 0x54, 0x68, 0x6C, 0xA4..0xAC, 0xB8 (heading), 0xBC..0xC8 (cosines, L93/L95) | per tick, MultiPhase L56 |
| Grip phase 2 `VehicleProximity` | functions/VehicleProximity_004fb2e0.c L54-L121 | 0xB0, 0xB4 | per tick, MultiPhase L37 |
| Per-tick sweep `sub_404250` | functions/SurfaceHeightfieldCollisionCallback_00404250.c L74-L79, L85, L104, L108 | 0x18, 0x114, 0x128 | called from Game_Restart L12 (static, one caller) |
| Cumulative distance `sub_4046B0` | functions/MemoryAllocation_004046b0.c L33, L41, L54 | 0x4C | called from sub_404250 L53 |
| Look-ahead distances `sub_4FB5D0` | functions/FAILNODOCENTRY_004fb5d0.c L27-L31, L39-L42, L55-L68 | 0xCC..0x108 | called from sub_404790 (sub_404250 L54) |

The MultiPhase state machine is driven once per tick from `GamePostMainLoop` (functions/GamePostMainLoop_0042d8a0.c L24). Its blend test is at functions/MultiPhaseProcessingStateMachine_00404460.c L13.

### 1b. Writes that are not AIW records (FACT, excluded)

- functions/CollisionAvoidanceCheck_004a6b80.c L67 `v12[30] = 2`. v12 = this[5], a collision object. Its +0x78 is a state value, not a waypoint field.
- functions/sub_45A8E0_0045a8e0.c L111 and L113-L116. Writes this+192..+335 from a stack buffer. Not in the 112..127 range.
- functions/LapCompletion_004bc4e0.c L61-L63 (this[29..31]). Lap object.
- functions/ArrayOfStructIndexedFieldCopy_00403f70.c L7-L15. Reads a separate 28-byte array at db+0x154. Not the 316-byte records.

### 1c. Search coverage (FACT, with limits)

- Regex over all of `functions/`: writes to `+ 112/116/120/124` or `+ 0x70..0x7C` with any base, and writes to `[28..32]`, with `=`, `+=`, `-=` and the like. Hits in AI, AIW, collision and physics code were read. Hits in camera, audio, render, network and vehicle-physics code were excluded by name.
- Stride-316 and stride-79 loops were enumerated (ObjectReleaseArrayIterator L15, CountedLoop L17, PhysicsNodePropagation, ShortComputation_45f0/4790). None writes this range. The 79-dword loop in sub_402690 L209-L215 is the constructor loop.
- No `memcpy`, `qmemcpy` or struct copy of a 316-byte record was found.
- LIMIT: a write through a computed variable offset (for example `*(p + i*4)` with `i` in a loop) would not match this grep. The 316-stride loops above were read to exclude that case.

Conclusion (1): FACT, no runtime writer of wp+0x70..0x7F was found. The four solver inputs stay at (0, 0, 0, -9.81).

## 2. Is a3 an AIW record? (question 2)

FACT. Evidence:

- functions/AIRacingLine_00425920.c L60-L72. `v2 = SpatialTreeNearestNeighborSearch(...)`. `v5 = v4[69]` or `v4[68]`, that is +0x114 or +0x110, which are the link fields of the AIW record (sub_402690.c L304-L320 and docs/52 L215-L216).
- AIRacingLine L205 `AIComputePassScore(v2)`. L205 also uses `*(v2+272)`, the next pointer +0x110. L209 `AIComputePassScore(v5)` multiplied by `this+17748` (0x4554). L206-L207 `*(v5+308)==1`, that is +0x134, and `*(v5+312)`, that is +0x138; L209 is the non-override branch. L259-L263 TireForce(v5+116, v5+120, v5+112). L264 `v5+60` (+0x3C, segment length). All are AIW record fields.
- The link walk at AIRacingLine L238-L240 (untyped file) steps v5 along +0x110 or +0x114. Each call receives the k-th record ahead on the chain. That is a look-ahead of records, not a separate struct.
- The second solver call on the same record is functions/sub_426430_00426430.c L137-L139 (`TireForceSolver(v17+116, v17+120, v17+112, 16.0)`, v17 = this+17236 = current waypoint). Same inputs.

Call graph: AIComputePassScore has one caller in the corpus (AIRacingLine; `python tools/ask.py func 0x42A780`). Its callees: TireForceSolver only.

## 3. Where corner information enters (question 3, refined H17)

### 3a. The corner cap (FACT code, DERIVED meaning)

    corner target(wp) = (TF0 * CorneringMult + CorneringAdd) * k4580 * pace * wp[+0x84]
    TF0 = TireForceSolver(car; a2 = 0, a3 = (0, -9.81), a4 = 0, step 16)   (constant per car)

Source: functions/AIComputePassScore_0042a780.c L3-L4; the inputs are the constants in section 1. H14 section 3e gives the same closed form.

So the corner cap changes per waypoint only through wp+0x84 (wp_event[0], parser sub_402690.c L296). In the AIW files this is a per-event multiplier and is 1.0 except for rare events (docs/52 L77: Monza records 983 and 984 only). DERIVED: the cap is essentially the same at every corner of a track, whatever its radius. The live replay (`docs/54-ai-driving-brain-map.md` L83, median 0.9993 on Monza) fits a constant TF0 with no speed or radius term, which is consistent with this.

### 3b. Where geometry does enter (FACT writers, FACT readers)

- Braking distance: wp+0x3C segment length. Writer sub_4FAF60 L44-L47 (`sqrt` of the normalized delta to the next record, +0xA4..+0xAC). Reader AIRacingLine L264. Braking projection uses this distance and the cap; no radius term appears in the braking law (H17 section 2c).
- Steering and path: wp+0xB8 heading (sub_4FAF60 L68-L75). Wp+0xBC..+0xC8 cosines of the heading change to the records 2..5 ahead (sub_4FAF60 L84-L96, clamped to 0.0241). Readers: sub_426430_00426430.c L192, L198-L200, L211, L216 (`+188`, `+184`, the steering target). This is the only consumer of the curvature-like samples found. It is a heading and steering input, not the cap.
- Lateral line: wp+0x94..0x9C (sub_4FB260), blended with +0x170 by the phase-0 blend. Downstream readers are DERIVED from docs/52 (table row `+0x94..+0x9C`, "rebuild; nearest queries") and were not traced here.

### 3c. Speed-related reader that uses geometry (OPEN)

- sub_426430 L137-L150. The current waypoint's solver result (L139) feeds a ratio `this+17476 = this+17472 / v18` (L146-L147), where `v18 = v2 * this[17668] + this[17664]`, capped at 75 (L140-L142). `v2` is a position delta (L97), or the lap distance +0x48 (L100), depending on the branch at L98. Fields 17664 and 17668 were not traced. The ratio is read by AISteering at functions/AISteering_004270a0.c L696 and L737 (`> 0.5`). Whether this is a corner-speed governor that sees the heading samples was not established (OPEN).

Conclusion (3): FACT, the corner cap itself (AIComputePassScore and the braking step in AIRacingLine) sees no curvature. Corner-specific speed depends on the braking projection with segment length, on the heading and steering path (+0xB8, +0xBC..+0xC8), and possibly on the OPEN speed ratio of section 3c.

## 4. Script

`%USERPROFILE%\Desktop\F1\development\mods\adaptive_ai_difficulty\tools\aiw_waypoint_dump.py`

- Reads the database base 0x005EA2C0, count at +0x130, base at +0x134, and checks the header dword at base-4 (sub_402690.c L199, L202, L204, L223, L225; docs/52 L155-L156, L224; Game_Init_0042d6b0.c L11).
- Prints index, addr, +0x48 (lap distance) and +0x70, +0x74, +0x78, +0x7C, +0x80, +0x84 as float. Options: `--pid`, `--step` (default 50), `--count` (highest index, default all).
- Read-only: `read_val` only. The `Game` helper opens the process with PROCESS_ALL (tools/poke.py L110-L118), so run from an elevated shell and never with a write call added.
- Not run. Syntax checked with `ast.parse`.
- Expected output if H14 and H19 hold: the columns +0x70, +0x74, +0x78 and +0x7C are 0, 0, 0 and -9.81 on every row. Any row that differs is a runtime writer to chase.
- Caveat: the script reads live memory; live reads need the owner's go-ahead per the workspace rules. Capture time is printed.

## 5. OPEN

1. A live check of the script output (owner run). This tests the no-writer conclusion at runtime on one race.
2. sub_426430 L137-L150 speed ratio: fields 17664, 17668, the L98 branch, and the readers at AISteering L696 and L737.
3. Whether +0xBC..+0xC8 is read by any speed code. Only the steering path was found.
4. Who populates the TireForceSolver table (this+0x3C40..0x3D00 on the car, H14 section 4). Not traced here.
5. Call frequency of Game_Restart (sub_404250 path) per tick or per restart. Not traced.

## 6. Corrections to prior notes

- H17 section 2b calls wp+0x70 "the lookup key for corner grip". It is a constant (0), so the key does not vary per corner. H14 section 3b is right.
- docs/52 table L202, `+0x70, +0x74..+0x80 speed/curve inputs ... AI racing-line look-ahead (DERIVED)`, is not supported by any writer or by a per-corner reader. Mark as stale.
- docs/52 table L213, `+0xBC..+0xC8 short curvature/look-ahead samples ... AI speed planning`. The only consumer found is the steering path (sub_426430 L192-L216). "Speed planning" is OPEN (section 3c).
- Skeleton names: `skeletons_filled/MemoryAllocation_004046b0.md` titles the function "WheelContactHeightSmoothing". The code operates on the AI database (+0x130/+0x134, records of 316 bytes). Name is wrong. Same for `SurfaceHeightfieldCollisionCallback_00404250`, which is the AI database per-restart update.
- name_index.json `0x005EA2C0` is "GarageTableOwner" (trust CONFIRMED). docs/52 L41, L134 and L365 show OBSERVED that it is the AI database structure base. Name index should be corrected by its owner.

## 7. Documentation gate

- Durable record: this report. It is discoverable from `investigations\adaptive_ai_difficulty\`.
- Not done, by scope: the findings in section 1 (writers), section 2 (a3 identity), section 3a (constant TF0) and OPEN items 1-5 were NOT appended to `findings.jsonl`. The owner limited this task to the report and the script. Append them in the next write-enabled step, with tier and source.
- Not done: workspace structure check and `f1c_mod.py evidence-query`. The start command ran (`python tools/f1c_mod.py start --json`); it reported `freshness.stale: true` (newest entry 2026-10-13). Its objectives were compared with recent entries, and the next objective of this task (AIW record writers) was not listed as done there.
- Not verified: live values (none read); the radius and braking numbers (see H17); the writers of 0x3C in the table builder (section 5.4).
- Confidence: writer scan high for the listed functions, medium for the full corpus (section 1c limit). Corner-cap conclusion high (constant inputs read in code). Speed-governor role of +0xBC OPEN.
