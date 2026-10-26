# H4: AI setups and fuel, qualifying vs race (2001 season, Austria and Nurburgring)

Date: 2026-10-15. Scope: static, read-only. No game process, poke, lab, or memory tool was touched.
Labels: FACT = directly read in a file or decompile line. DERIVED = inferred from FACTs. OPEN = not established.

## Answer (short)

- FACT: The AI setup file is picked per session type. Qualifying (SessionType 3-5) uses `<track>.qual.svm`. Other sessions use `<track>.race.svm`. The lookup goes through the car's vehicle directory, then `<track>.svm`, then `ai.svm`, then the track gdb `SettingsAI` default.
- FACT: In every qual/race pair checked (2001 Australia; 2002 Austria and Great Britain for 6 teams), the two files differ only in fuel, pit-stop count, tyre compound, and at most one body-aero flag. Wings, gearing, brakes, ride height and suspension are identical.
- FACT: There are no active 2001 Austria or 2001 Nurburgring qual/race files in any 2001 car directory. Only `.race.svm.bak` copies exist for Austria and Europe.
- DERIVED: For the 2001 Austria and Nurburgring races, AI cars fall back to the gdb `SettingsAI` file (`Save\2001 Austria.svm`, `Save\2001 Europe.svm`). Race fuel is about 44% (Austria) and 87% (Nurburgring), with 0 and 3 pit stops. A qualifying session would use the same file, because the qual-specific gdb key is not identified.
- Confidence: High for the lookup order and for qual-vs-race being a fuel/pit-only difference. Medium for the 2001 fallback values. Low for AI fuel-mass effects (see OPEN).

## 1. Which setup files exist

### 1a. 2001 car directories (what the 2001 AI would load)

- FACT: 2001 car directories are `SeasonData\Vehicles\<Team>\2001_<car>\`. There are 11 of them (Arrows A22, BAR 003, Benetton B201, Ferrari F2001, Jaguar R2, Jordan EJ11, McLaren MP4-16, Minardi PS01, Prost AP04, Sauber C20, Williams FW23).
- FACT: All 11 2001 car directories contain `Australia.qual.svm` and `Australia.race.svm`. Each also contains `.race.svm.bak` files for Austria, Europe, Brazil, Canada, France and Malaysia. No `.svm` for Austria or Europe is active. No `ai.svm` exists anywhere under `SeasonData\Vehicles`.
- DERIVED: The 2001 AI for Austria and Nurburgring therefore falls to the gdb default in all sessions.

### 1b. Owner's custom files (active, dated)

- FACT: `SeasonData\Vehicles\<Team>\2002_<car>\Austria.{qual,race}.svm` (Arrows, BAR, Jaguar, Jordan, McLaren, Minardi, Prost, Renault, Sauber, Toyota), modified 2026-09-27.
- FACT: `SeasonData\Vehicles\<Team>\2002_<car>\Great Britain.{qual,race}.svm` (Arrows, BAR, Ferrari, Jaguar, Jordan, McLaren, Minardi, Prost, Renault, Sauber, Toyota), modified 2026-10-10.
- FACT: `Save\Matias Driver\Settings\Great Britain\Great Britain.{qual,race}.svm`, modified 2026-10-10. These are the owner's player-side copies.
- FACT: 208 non-backup `.qual.svm`/`.race.svm` files exist in the whole tree. None exist for 2001 Austria or 2001 Europe.
- FACT: Custom pack `Custom\2001\Modit\V1_V10\Randomizer\TKD-F1C-99-02_SetupPack_v3\AI setups\` has `Austria.svm` and `2001 Europe.svm` (not qual/race split, not in any game vehicle dir). Also `Austria\TKD-Austria [Monsoon].svm` (a wet/alt variant) and `Europe\TKD-Nurburgring.svm`, `TKD-Nurburgring V3.svm`.
- OPEN: Whether the owner copies the SetupPack `AI setups` files into vehicle directories. Nothing in the tree shows that copy for 2001.

### 1c. Track defaults (game "game.gdb" equivalent)

- FACT: `SeasonData\Circuits\Austria\2001_A-1_Ring\2001_A-1_Ring.gdb`: `RaceLaps = 71` (line 4), `TrackName = A-1 RING` (line 6), `Length = 2.684 m./4.319 km.` (line 11), `SettingsFolder = Austria` (line 160), `SettingsCopy = 2001 Austria.svm` (lines 161-162), `SettingsAI = 2001 Austria.svm` (line 163). Qualify Laptime 69.769 (line 164), Race Laptime 73.814 (line 165).
- FACT: `SeasonData\Circuits\Europe\2001_Nurburgring\2001_Nurburgring.gdb`: `RaceLaps = 67` (line 4), `TrackName = EUROPE, NURBURGRING` (line 6), `Length = 2.832 m./4.556 km.` (line 11), `SettingsFolder = Europe` (line 142), `SettingsCopy = Grip.svm` and `2001 Europe.svm` (lines 143-144), `SettingsAI = 2001 Europe.svm` (line 145). Qualify Laptime 75.490 (line 146), Race Laptime 79.852 (line 147).
- FACT: `SeasonData\F1_2001.gdb` defines the season: `QualifyLaps = 12`, `RaceLaps = 50` (lines 24-30), `QualifyDuration = 60`, `RaceTime = 120`. Scene order has `2001_A-1_Ring` and `2001_Nurburgring`.
- FACT: `Save\2001 Austria.svm` (the SettingsAI default) has FuelSetting 44, FWSetting 47, RWSetting 32, Gear1 6, Gear7 60, FinalDrive 2, BrakePressure 10, RearBrake 50, NumPitstops 0, TireCompound 1, RevLimit 10.
- FACT: `Save\2001 Europe.svm` (the SettingsAI default) has FuelSetting 87, FWSetting 48, RWSetting 47, Gear1 7, Gear7 65, FinalDrive 1, BrakePressure 8, RearBrake 42, NumPitstops 3, Pitstop1 57, TireCompound 1, RevLimit 10.
- FACT: The owner's player files `Save\Matias Driver\Settings\Austria\Austria.svm` (FuelSetting 84, RW 47, NumPitstops 3) and `...\Europe\2001 Europe.svm` (FuelSetting 87, pit 3) are not the AI default and are not used by the AI lookup.

## 2. Qual vs race values

### 2a. Same-car, same-track pairs (FACT, full-key diff of every section)

| File pair | Keys differing (qual -> race) |
|---|---|
| 2001 Ferrari, Australia | BodyAero.BrakeDuct 0 -> 1; Fuel 30 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| Owner Save, Great Britain | Fuel 25 -> 100; Pitstops 0 -> 1 |
| 2002 Arrows, Austria | BodyAero.Radiator 0 -> 2; Fuel 26 -> 100; Pitstops 0 -> 2 |
| 2002 Arrows, Great Britain | Fuel 26 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| 2002 BAR, Austria | Fuel 23 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| 2002 BAR, Great Britain | Fuel 26 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| 2002 Jordan, Austria | Fuel 23 -> 100; Pitstops 0 -> 2 |
| 2002 Jordan, Great Britain | Fuel 27 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| 2002 McLaren, Austria | Fuel 23 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| 2002 McLaren, Great Britain | Fuel 28 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| 2002 Ferrari, Great Britain | Fuel 26 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |
| 2002 Sauber, Austria | Fuel 21 -> 100; Pitstops 0 -> 2 |
| 2002 Sauber, Great Britain | Fuel 26 -> 100; Pitstops 0 -> 1; Tyre 1 -> 0 |

- FACT: No wing, gear, final-drive, ride-height, brake-pressure, brake-bias or suspension key differs between qual and race in any pair above. Only the fuel, pit, and tyre keys (and one radiator/brake-duct flag) change.

### 2b. Cross-team values, race files, 2002 Austria and Great Britain (FACT)

| Team | Austria FW / RW | Austria qual fuel | Austria race pits | GB FW / RW | GB qual fuel | GB race pits |
|---|---|---|---|---|---|---|
| Arrows | 50 / 38 | 26 | 2 | 43 / 32 | 26 | 1 |
| BAR | 50 / 39 | 23 | 1 | 43 / 31 | 26 | 1 |
| Jordan | 50 / 41 | 23 | 2 | 45 / 37 | 27 | 1 |
| McLaren | 45 / 35 | 23 | 1 | 44 / 36 | 28 | 1 |
| Sauber | 45 / 36 | 21 | 2 | 46 / 36 | 26 | 1 |
| Ferrari | n/a | n/a | n/a | 42 / 33 | 26 | 1 |

- FACT: Across the same six teams, Gear1 = 9 and FinalDrive = 1 in every file. Gear7 = 62 (Austria) or 64 (GB). Brake pressure = 10 (Austria) or 9 (GB). Rear brake = 40-41. RevLimit = 10. Downforce, Balance and Gearing are 0.5 in every file.
- FACT: Race fuel is 100 in every race file. Qual fuel is 21-30 (about a quarter tank).
- DERIVED: Front-wing and rear-wing spread across teams is about 7-8 units (FW 42-50, RW 31-41). That is the only setup variable that varies much by team in these files. The gear and fuel values are effectively uniform.
- DERIVED: Qual and race therefore differ in fuel load and pit count, not in aero or gearing. Fuel is the strongest lap-time effect (more fuel = slower laps in race), and the 100% race load is not on the qual lap.

### 2c. Rain track

- FACT: Austria and Hungary have `TKD-*[Monsoon].svm` variants in the SetupPack. San Marino has `TKD-Imola (wet) v3.svm`. No 2001 or 2002 qual/race split exists for Spa (Belgium) or Interlagos (Brazil) in a vehicle directory. Silverstone has only the owner's GB pair.
- OPEN: Whether the Monsoon/wet variants are ever loaded by AI. No code path in the corpus names them.

## 3. Code: where the lookup happens

- FACT: `functions\DynMan_Restart_0042c3b0.c:11` calls `AISettingsFileLoader(&dword_7C73E8)` once per restart, before `SceneRenderUpdate` (line 16).
- FACT: `functions\AISettingsFileLoader_004333e0.c`:
  - line 29: only slots with `*(v2+13000) != 0` are processed (DERIVED: AI-eligible slot flag).
  - line 31: `byte_7D03E0` gates the vehicle-specific branch. The globals doc (`docs\12-globals-reference.md:830`) names it AISettingsMode (0 = simple, nonzero = advanced). The option text in `PlayerOptionsInitializer_004aa270.c:1117` (`sub_4B1C20(aVehicleSpecifi ...)`) is the "Vehicle Specific AI Setups" toggle (DERIVED: option writes this byte).
  - line 35: copies the vehicle file path from `v2+18196`. Lines 40-50 strip the filename and append `\`. So v14 = vehicle directory.
  - line 54-57: `SettingsFolderResolver(dword_7CE5EC)` gives the track folder (`Austria` or `Europe`), appended to the vehicle directory.
  - line 58-62: `IterationCountMap(dword_7C56A4) == 5` selects `qual`, else `race`. Then `.svm` is appended. First probe: `<vehdir><track>.<qual|race>.svm` (line 63).
  - line 70-75: second probe `<vehdir><track>.svm`.
  - line 79-84: third probe `<vehdir>ai.svm`.
  - line 86-94: any hit sets `v2+18324` (the setup path, 0x4794 per corpus finding) and clears `v2+18579`.
  - line 99-100: no hit falls back to `sub_433890`'s resolved path (the gdb `SettingsAI` file) and clears the same flag.
- FACT: `functions\SettingsFolderResolver_004326a0.c:5-7`: if the season race section is found, the `off_5C3FA4[0]` key value is returned (the SettingsFolder value, e.g. `Austria`). Otherwise the input name is returned.
- FACT: `functions\IterationCountMap_00432e50.c:5-10`: input < 3 returns 7; input 3 to 5 returns 5; input > 5 returns 7. So SessionType 3, 4 and 5 use the qual file, and 0, 1, 2, 6, 7 and above use the race file.
- FACT: `docs\38-confirmed-offsets.md:19`: SessionType 5 = qualifying, 7 = race (live-verified 2026-07-27).
- DERIVED: Practice (1) and warmup use `.race.svm`. Only the qualifying sessions use `.qual.svm`.
- FACT: `functions\AISettingsFileResolver_00433890.c:14-46` resolves the fallback. It reads the season race section, takes the SettingsCopy/SettingsAI value (keys `off_5C3FA8`, `off_5C3FB0`), overrides with `off_5C3FAC` when session == 5, prepends `SettingsDirectory` (`dword_5EC440`), and returns the path.
- OPEN: The exact key strings for `off_5C3FA8`, `off_5C3FB0`, `off_5C3FAC`. The globals index names them only as "VTable_Base", "Off_5C3FB0_Sound", "UnknownDataPtr_5C3FAC" (those names are wrong or unreliable). So the key used for the qual-session fallback is not identified. The gdb only has `SettingsAI`.
- FACT: The corpus memory note (`findings.jsonl`, 2026-10-10 entry) matches this path: "vehicle dir <track>.<qual|race>.svm, <track>.svm, ai.svm, else game default", and CO-OP bots added after load have an empty setup path, so they race on `.hdv` defaults.

### 3b. AI fuel option and fuel load

- FACT: `functions\PlayerOptionsInitializer_004aa270.c:1103` registers "AI Additional Fuel Mult" with three float literals: 0x3F733333 (about 0.95), 0x3DCCCCCD (about 0.1), 0x40000000 (2.0). So default 0.95, min 0.1, max 2.0 (DERIVED from literal decode).
- FACT: The owner's `.plr` value 0.95 matches the shipped default (`data_index.json:19631`, 6 files carry 0.95).
- FACT: No function in the corpus reads the "AI Additional Fuel Mult" value. `ask.py token "AI Additional Fuel"` returns only the setter. `ask.py token "qual.svm"` returns nothing.
- FACT: Fuel for each car comes from `FuelSetting` in its setup file. `functions\TheSvmHdvVehicleFileParser_004f4270.c:248` maps FuelSetting through `FuelRange` (`CRTMathHelper_e1f0`). So race fuel for an AI car = the FuelSetting in the loaded `.svm` (race files: 100).
- FACT: `functions\FuelMultiplierValue_0042ad20.c:3-6` returns `ftol(this[3681]*this[3346] - 0.2)` when `dword_7D0B64` (FuelMult) is set, else 100. `FuelWeightLimitCheck_0042ad50.c` and `FuelWeightComputation_0049ba50.c` use it. These are pit-decision helpers, not a fuel-load multiplier.
- FACT: `functions\AIAbility_00429470.c:176-182`: in race (SessionType 7), an AI car whose FuelMultiplierValue < 2 may be forced into pit exit when the remaining-fuel test fails. This reads FuelMult, not the AI option.
- FACT: `RaceLaps` = 71 (Austria) and 67 (Nurburgring) are in the track gdb. No code path in the corpus multiplies laps by the AI fuel option.
- Answer to the sub-question: NO evidence that AI race fuel = race length x AI Additional Fuel Mult. Race fuel for AI is the FuelSetting of the loaded `.svm` (100 in the race files found). Whether the 0.95 option scales the loaded FuelSetting is OPEN, because the reader is not identified.

## 4. Derived 2001 picture (what the game would load now)

- DERIVED: 2001 Austria, all sessions, AI cars: `SettingsAI` = `Save\2001 Austria.svm` (fuel 44, FW 47, RW 32, pit 0). Race and qual share this file, so AI qual fuel = race fuel = 44 unless a qual key overrides it (OPEN).
- DERIVED: 2001 Nurburgring, all sessions, AI cars: `Save\2001 Europe.svm` (fuel 87, FW 48, RW 47, pit 3). The same caveat applies.
- DERIVED: If the owner's SetupPack `AI setups\Austria.svm` (fuel 18, FW 49, RW 39, gear1 9, gear7 62) or `AI setups\2001 Europe.svm` (fuel 21, FW 49, RW 35, gear1 5, gear7 63) were copied into the 2001 car directories as `Austria.race.svm` or `ai.svm`, the AI would use those instead. No copy is present.

## 5. Which setup values most plausibly change lap time

- FACT-based ranking (from the value spread above):
  1. Fuel (FuelSetting): spread 18-100 in the files here. Largest single lap-time effect.
  2. Front and rear wing: FW 42-50, RW 31-47. Moderate effect (downforce vs drag). The 2001 Europe default has RW 47 vs Austria RW 32.
  3. Gearing (Gear1, Gear7, FinalDrive): Gear7 60-65 and FD 1-2 across the files. Top-speed effect at Austria/Nurburgring (long straights).
  4. Pit stops (NumPitstops, Pitstop1): race-only. Affects race time, not qualifying.
  5. Brake pressure and rear brake bias: 8-10 and 40-50. Small effect.
  6. Tyre compound and body-aero flags: 0/1, radiator 0-2. Small.
- DERIVED: In qualifying, the fuel difference (21-30 in the 2002 qual files) is the only variable that changes between qual and race files. The owner's qual files already run light fuel; the AI qual file in the 2001 gdb default (44 or 87) is not light, which would make AI qual laps slower than the owner's qual setups. This is the most important open risk.

## 6. Verification performed

- Full INI key diff (every section, excluding Notes) of 13 qual/race pairs, using a read-only Python script outside the workspace (`%TEMP%`-style scratch in `/tmp`, not kept).
- File listings with modification times across the whole game tree (`find -printf "%TY-%Tm-%Td"`).
- Decompile reads of `AISettingsFileLoader`, `AISettingsFileResolver`, `SettingsFolderResolver`, `IterationCountMap`, `DynMan_Restart`, `PlayerOptionsInitializer` (lines 1103, 1117), `FuelMultiplierValue`, `FuelWeightLimitCheck`, `FuelWeightComputation`, `AIAbility` (lines 170-185), `TheSvmHdvVehicleFileParser` (lines 236-256).
- Corpus queries: `ask.py token`, `ask.py global` for 0x5C3FA8, 0x5C3FB0, 0x5C3FAC, 0x5EC440, 0x7CE5EC.
- Not verified: no game run, no live read, no poke. Nothing was written to the game tree.

## 7. Negative and unchanged results

- No `ai.svm` file exists anywhere under `SeasonData\Vehicles` (FACT).
- No active `.qual.svm` or `.race.svm` for 2001 Austria or 2001 Europe (FACT).
- No corpus function reads "AI Additional Fuel Mult" (FACT).
- No corpus string matches "qual.svm" (FACT). The qual/race choice is made by the IterationCountMap call, not by a literal string.

## 8. OPEN items for follow-up

1. Identify the exact key strings behind `off_5C3FA8`, `off_5C3FB0` and `off_5C3FAC` in `AISettingsFileResolver` (needs a dump of the string table around 0x5C3FA8 or a per-key read), to confirm whether a qual-specific SettingsAI key exists.
2. Identify the reader of the "AI Additional Fuel Mult" value (the registry that `MultiFieldSetterWithStringCopy` writes to), and whether it scales FuelSetting or only pit-decision thresholds.
3. Confirm whether the owner copies SetupPack `AI setups` files into 2001 car directories, and under what names.
4. Confirm whether `byte_7D03E0` is set from the "Vehicle Specific AI Setups" option (the option text is at `PlayerOptionsInitializer_004aa270.c:1117`; the write is not shown).
5. Confirm which vehicle directory a 2001 season car uses (`2001_*` vs `2002_*`). The owner's custom AI files for Austria and GB are in `2002_*` directories, not `2001_*`.
6. Live check (not done here): with a 2001 Austria race, compare the AI fuel load to FuelSetting 44 and to 44 x 0.95. This needs an owner-approved live session.

## Records

- Report: this file.
- Not done, by scope: the AGENTS.md SDK front-door step (`python tools/f1c_mod.py start --json` and `evidence-query`) was not run, because the task said read-only and that front door may write a checkpoint. Its findings query, and the append of these results to `findings.jsonl` (tier, evidence, source), remain pending owner approval. Candidate entries: qual/race differ only in fuel/pits/tyre (FACT); AI lookup order (FACT); AI fuel option has no identified reader (OPEN).
- Related: `docs\38-confirmed-offsets.md:19` (SessionType), `findings.jsonl` 2026-10-10 entry (CO-OP empty setup path).
- Not modified: no game file, no corpus source, no `findings.jsonl` (per the read-only scope).
