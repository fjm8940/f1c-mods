# H2: AI calibration PLR options (static, read-only) - 2026-10-15

Scope: static reverse engineering only. No game process, poke.py, lab.py or memory-mcp was used.
Evidence labels: FACT = code or bytes show it; DERIVED = reasoned from FACT; OPEN = not established.
Binary used for disassembly: `F1 Challenge V10\F1 (1).exe` (same strings at the same addresses as `F1 Challenge Online.exe`). Dumps are in %TEMP% (`full_asm.txt`, `init_asm.txt`), not in the corpus. Addresses below are from those dumps and the decompiled `.c` files.

## 1. Next action / bottom line

- The four options live in one options object at global `byte_7CE498`. Each has a 36-byte-style record with current value at +0x14, default +0x18, min +0x1C, max +0x20 (FACT, setter below).
- Power, gearing and fuel are one bitfield consumer, `ScaleAudioPitch` (sub_4F1510), called once per AI car at init. It scales vehicle arrays by `v14 = a2 / this[15348]`, where `a2` is the slider curve times 0.995.
- Strength 100 is NOT neutral unless the stored baseline happens to equal the scaled curve value (DERIVED, section 5).
- Autocalibrate has no reader found (OPEN).
- Confidence: high on storage and reader locations (FACT). Medium on semantic labels of the rescaled arrays. OPEN on the baseline value and on the Autocalibrate consumer.

## 2. Storage: where each option lives (FACT unless noted)

Options object: `byte_7CE498`. Passed as `this` to the UI init `UIControlWindowInitialization` (functions/UIControlWindowInitialization_004a70e0.c:1011, called from functions/sub_4A6FE0_004a6fe0.c:3 with `&byte_7CE498`). Asm confirms the global: `mov ecx,0x7ce498` at 0x4732db and `call 0x4adf10` at 0x4732f0.

Initializer (functions/PlayerOptionsInitializer_004aa270.c:1101-1116) writes each option through a setter whose `this` is `esi+N` (esi = a1 = ecx at entry, asm 0x4aa282 `mov esi,ecx`):

| Option (name string addr) | Record base (esi+N) | Current (abs) | Default | Min | Max | Setter call (asm) |
|---|---|---|---|---|---|---|
| AI Driver Strength (005d0528) | +0x1CF4 | 0x7D01A0 = `dword_7D01A0` | 75 | 70 | 120 | 0x4ac5d0, lea 0x4ac5ca |
| AI Power Calibration (005d04b4) | +0x1D18 | 0x7D01C4 = `byte_7D01C4` | 7 | 0 | 7 | 0x4ac5eb, lea 0x4ac5e5 |
| AI Additional Fuel Mult (005d0454) | +0x1D3C | 0x7D01E8 = `flt_7D01E8` | 0.95 | 0.1 | 2.0 | 0x4ac610, lea 0x4ac60a |
| Autocalibrate AI Mode (005d01d4) | +0x1F10 | 0x7D03BC | 0 | 0 | 3 | 0x4ac786, lea 0x4ac780 |
| AI Realism (context) | +0x1CD0 | 0x7D017C = `flt_7D017C` | 0.5 | 0 | 1 | lea at 0x4ac7cb (DERIVED: base) |

- Strings verified from the exe bytes at 0x5D04B4, 0x5D0454, 0x5D01D4, 0x5D0528 (static file read).
- Float constants in the table are from the setter arguments (asm pushes: 0x3F733333 = 0.95, 0x3DCCCCCD = 0.1, 0x40000000 = 2.0). Note the minimum is 0.1, not 0.05.
- Setter layout (FACT): `functions/MultiFieldSetterWithStrings_004b1ac0.c:5-9` writes `this[6]=default, this[7]=min, this[8]=max`, and `this[5]=default` when a7 is set. `functions/MultiFieldSetterWithStringCopy_004b1d90.c:5-9` is the same. So current = +0x14, default = +0x18, min = +0x1C, max = +0x20.
- Value clamp helper (FACT): `functions/ArithmeticComparison_20d0_004b20d0.c:4-17` (= sub_4B20D0) sets `this[5]` then clamps to `[this[7], this[8]]`.

Stale-doc notes (FACT):
- `docs/12-globals-reference.md:5919` gives DAT_005d04b4 as "AiPowerCalibra"; the bytes are "AI Power Calibration".
- `docs/12-globals-reference.md:6205` gives DAT_005d01d4 as "AutocalibrateA ... analog axes". The bytes are "Autocalibrate AI Mode". The analog-axis reading is not supported by this string.
- `docs/12-globals-reference.md:5352` gives DAT_005d0528 as "AIDriverStrength"; the bytes are "AI Driver Strength".
- PLR sample values for Autocalibrate are 0 and 1 (`data_index.json`), but the initializer allows 0..3.

## 3. Readers and how each option is used

### 3a. AI Driver Strength (dword_7D01A0)
- Slider curve (FACT): `functions/AIDifficultyInterpolation_00424ac0.c:5-9`. Low = flt_5EA3DC, Mid = flt_5EA3E0, High = flt_5EA3E4.
  - s <= 100: `Low + (s-70)/30 * (Mid-Low)`
  - s > 100: `Mid + (High-Mid) * (s-100) * 0.05`
- Diff scalar (FACT): `functions/AIDifficultyScalar_00424750.c:3-6`: s >= 95 gives 1.0, else `0.75 + (s-70)*0.01`. Used for AI ability (`functions/AIAbility_00429470.c:31-35`, `:76`) and steering correction (`functions/AISteeringCorrection_00426cf0.c:23`, x0.75).
- Per-driver setup (FACT): `functions/sub_424080_00424080.c:157-158`: for s < 100, `this[4401] = (s/100)^2 * (22-RA)/22`. Line 151 uses GDS flt_5EA3D8 (`docs/41`, `docs/43`).
- Other readers (FACT, grep): `AIBehaviorInitialization_004bf0a0.c:35`, `AIDatabase_Init_004040a0.c:53-60`, `AIRingBufferFieldInit_00424c80.c:8`, `sub_4C29F0_004c29f0.c:15`.
- Applied at car init, not per frame: the consumer below runs from `AIDriverStructInit` (functions/AIDriverStructInit_00424790.c:128-131).

### 3b. AI Power Calibration (byte_7D01C4): the bitfield
- Only reader of byte_7D01C4 (FACT): `functions/ScaleAudioPitch_004f1510.c:28` (bit 1), `:38` (bit 2), `:63` (bit 4). Asm: `test BYTE PTR ds:0x7d01c4` at 0x4f1548, 0x4f1593, 0x4f15e7.
- Only caller (FACT): `functions/AIDriverStructInit_00424790.c:131` `return ScaleAudioPitch(v19);`. Asm call at 0x424aa6 with `lea ecx,[esi+0xc]`, so `this` = AI struct + 12 bytes (FACT for the offset; the vehicle identity of that object is DERIVED).
- Input `v19` (FACT): `functions/AIDriverStructInit_00424790.c:128-130`: `v19 = AIDifficultyInterpolation()`, then `v19 *= flt_5EA3E8` unless `dword_7C56A4` is 3, 4 or 5. flt_5EA3E8 = 0.995 (FACT, set at `functions/FunctionCallWrapper_00401080.c:21`).

Body of `sub_4F1510(this, a2)` (FACT, `ScaleAudioPitch_004f1510.c`):
- Line 17: `v14 = a2 / this[15348]`.
- Lines 12-23 (ungated, runs for any bit value): four floats at `this+13852 + 248*k` (k = 0..3) are multiplied by v14.
- Bit 1 (lines 28-36): `sub_5881F0(v14)`, then a loop over a float-pair block at `this+14756` (count `this+14768`), updating `v9[1] = (v8 + v9[1]) * v14 - *v9`.
- Bit 2 (lines 38-61): block at `this+14800` (count `this+14840`). Entries are multiplied by `1/v14`. Index 9 uses stride 918 floats (`v11 += 918`).
- Bit 4 (lines 63-67): `this+14724 *= v14`.
- Line 68: `this[15348] = a2` (stored as the new baseline).

Owner labels (DERIVED): bit 1 = power, bit 2 = gearing (inverse scaling of a ratio-like block), bit 4 = fuel. Field identities for the blocks are OPEN. `this+14724` is the fuel-weight scalar (see 3c). `sub_5881F0` tail-calls `sub_588212_00588212.c` (not analysed).

### 3c. AI Additional Fuel Mult (flt_7D01E8)
- Only reader found (FACT): `functions/SuspensionAndWheelPhysics_004e5f60.c:562`: `this+14724 = dword_7D0B64 * a3[1652] * flt_7D01E8`. Asm: `fmul DWORD PTR ds:0x7d01e8` at 0x4e6930.
- Run at vehicle init, once per car init (DERIVED from the function's place in init).
- Default 0.95 therefore scales `this+14724` by 0.95 (DERIVED).
- `this+14724` consumers (FACT): `functions/FuelWeightLimitCheck_0042ad50.c:4` and `functions/FuelWeightComputation_0049ba50.c:7` (fuel-weight limit). Also thresholds at `AIMainUpdate_00424e70.c:241` and `sub_426430_00426430.c:196,234` (> 0.001).
- `dword_7D0B64` is named `FuelMultiplierValue` by its function `functions/FuelMultiplierValue_0042ad20.c:3` (DERIVED).
- OPEN: whether this applies to the player car too. The caller of `SuspensionAndWheelPhysics` was not traced for an AI-only guard.

### 3d. Autocalibrate AI Mode (0x7D03BC)
- No reader found (FACT, negative): no decompiled `dword_7D03BC`, `flt_7D03BC` or `7d03bc` hit (grep over `functions/`). Full static disassembly (`full_asm.txt`, 550,744 lines) has no `0x7d03bc` absolute reference, and no `+0x1f24` / `+0x1f10`-style displacement read that is clearly on the options object.
- The string is not used at runtime (only one literal, in the initializer, per `ask.py token`).
- Status: OPEN. The consumer may read it through a path not decompiled. The "AI Mode" string suggests it is not analog-axis calibration (see stale-doc note in section 2).

## 4. Preset writes that overwrite the PLR values (FACT, mode-dependent)

- `functions/sub_4732D0_004732d0.c:8`: `DrivingModeParameterSetup((int)byte_7CE498)`. Mode index is `unk_7D0158` (= 0x7CE498 + 0x1CC0, offset 7360 in `DrivingModeParameterSetup_004adf10.c:65`).
- Mode 1 (`:112-116`): AI Driver current (+7432 = 0x7D01A0) := 75, clamped to [7440, 7444]. AI Realism current (+7396) := 0.25 (1048576000).
- Mode 2 (`:162-166`): AI Driver current := 85. AI Realism := 0.25.
- Mode 3 (`:169-198`): `ArithmeticComparison_20d0(95)` on an unidentified record (OPEN which field). Modes 4 and 5 call the same helper with other values (OPEN).
- DERIVED: selecting modes 1 or 2 in the UI replaces the PLR AI Driver Strength (for example 92) with 75 or 85. Which mode is the default, and whether the PLR value is re-read afterwards, is OPEN.

## 5. Is power active at strength 100?

Formula (DERIVED from sections 3a-3c):
- `v19(s) = curve(s) * 0.995` (no 0.995 for session types 3, 4, 5). Track constants from the AI database (`docs/12-globals-reference.md:6692` and `:6977`).
- `v14 = v19 / this[15348]_before`.
- At s = 100, `curve(100) = Mid`, so `v19 = 0.995 * Mid`.

Consequences:
- The scale is relative to the stored baseline `this[15348]`, not to 1.0. The baseline is written by two paths: `SuspensionAndWheelPhysics_004e5f60.c:293` (1.0) and `PhysicsParameterInitialization_004af3a0.c:183, 343, 503` (0, 0.2, 0.5, then FloatClamp). Which path runs last per car is OPEN.
- If the baseline is 1.0 and Mid = 1.0 (the Monza anchor in `docs/42-ai-realism-and-difficulty-model.md` section 4.1, not re-verified here): at s = 100, `v14 = 0.995`. Bit 1 array x0.995, bit 4 fuel x0.995, bit 2 ratios x1.005. At s = 92, `curve = 0.96` and `v14 = 0.9552`. So the power adjustment changes AI power even at 100 (DERIVED, conditional on baseline).
- The ungated block (`this+13852`, lines 12-23) is scaled by v14 even when the bitfield is 0 (FACT). So bits only gate groups b, c, d.
- Arrays are scaled in place. Repeated calls multiply the ratios. Unless the arrays are re-initialised per car, the net factor is `v19 / baseline_initial` (DERIVED). Reset behaviour is OPEN.
- Neutral (v14 = 1) therefore requires `v19 == baseline`, which is not guaranteed at 100 and depends on track Mid and the baseline path.

## 6. Timing (when it applies)

- Per car, at init: `AIDriverStructInit` is called from `functions/SceneRenderUpdate_004c6d90.c:19`. The exe has 8 call sites to `0x4c6d90` (asm lines 48431, 54484, 56840, 121067, 213082 and others). Not per frame (FACT).
- `ScaleAudioPitch` (line 131) is outside the `if (LOBYTE(a2))` guard (`AIDriverStructInit_00424790.c:82-122`). So every car that passes through `AIDriverStructInit` runs it. Whether the player car is one of them is OPEN (not traced).
- The PLR values are read by the initializer at startup. UI preset selection (section 4) can overwrite them later.

## 7. Open items and follow-ups

1. Resolve the baseline `this[15348]` at car init (check `PhysicsParameterInitialization_004af3a0.c` branch conditions and call order vs `AIDriverStructInit`).
2. Name the blocks at `this+14756` (bit 1) and `this+14800` (bit 2), and the 4-entry block at `this+13852`.
3. Find the Autocalibrate consumer (try the full-binary scan for `7ce498` plus offsets 0x1F20-0x1F28, and any `lea` of the record base).
4. Confirm whether `SuspensionAndWheelPhysics` init runs for the player car (Additional Fuel scope).
5. Identify modes 3-5 targets for `ArithmeticComparison_20d0` (implicit ECX).
6. Correct `docs/12-globals-reference.md` lines 5352, 5919 and 6205 (string names). Doc 42 section 3 says "AI Power Calibration ... 1+2+4", which matches the bit tests (consistent, FACT).
7. Append to `findings.jsonl` (not done: the request said read-only except this one report file). Candidate entries: storage map (section 2), bitfield consumer (3b), Autocalibrate no-reader (3d, OPEN).

## 8. Workspace notes

- `tools/workspace/check-workspace-structure.ps1` run: 2 pre-existing errors (unexpected top-level dirs `.claude` and `plans`). Not caused by this task, not changed here.
- Temp files created in %TEMP% only: `pe_check.py`, `flt.py`, `init_asm.txt`, `full_asm.txt`. Nothing in the workspace was changed except this report.
- Method notes: decompiler hides `this` (ECX) at setter call sites, so the record bases come from disassembly (FACT). Offsets 0x1CF4 and others also appear in vehicle code, so the absolute addresses (0x7D01xx) are used to separate the options object from vehicle reads.
