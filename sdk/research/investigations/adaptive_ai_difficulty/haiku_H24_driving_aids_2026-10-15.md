# H24 - Player driving-aid storage (static, read-only) - 2026-10-15

Scope: where driving-aid settings live (PLR keys, options init, runtime globals, per-vehicle copy) and whether they change mid-session. Static only; the running game was not touched. Tier: FACT = read in code/file; DERIVED = inferred from code shape; OPEN = not closed statically.

## 1. Answer summary

- FACT: aids are read from the PLR `[DRIVING AIDS]` section and related sections by the options initializer `sub_4AA270` (`PlayerOptionsInitializer_004aa270.c`). It writes default values and value ranges into an options object at `this` (= `a1`), not into a fixed global.
- FACT: the live aid state the game cycles is 11 separate DWORD globals at `0x007CE6AC..0x007CE8A4` (not `byte_7CE498`). They are cycled by `AIShiftPattern` (`sub_42B8D0`) from the in-car key dispatcher `sub_42C440`.
- FACT: `AIShiftPattern` ends by calling `BrakeBiasSetup(&dbl_7C7428)` (`AIShiftPattern_0042b8d0.c:111`), which copies those 11 globals into the block at `0x7C7428+0x72..0x7C` (`BrakeBiasSetup_00438cc0.c:5-15`). That block is a global struct, not a per-vehicle struct, and this is the only copy found.
- OPEN: how PLR values reach the 11 globals at load time. No writer of the globals exists in the corpus (`ask.py global`: WRITERS 0). A UI-object path (`this[199] = &dword_7CE...`) exists but is unproven as a load path.
- Mid-session: YES, FACT. Keys mapped in `sub_42C440` cycle the aid globals during play. Each change calls `BrakeBiasSetup`, and the HUD/UI reads the same globals (`FormatStringBatchDisplay_0049e170`, `UIObjectConstructorWithDefaults_00471b20`).

## 2. Premise corrections (important)

1. PLR `Traction Control="1.75000"` (`Matias.PLR:125`) is in `[ Sound Options ]` and is a volume multiplier. The TC aid is NOT in the PLR. There is no `Traction Control` key in `[ DRIVING AIDS ]`. Options-initializer `a1+3712` (`PlayerOptionsInitializer_004aa270.c:502-506`) is the sound value. The schema row `Traction Control | 0xE80` (`CONFIG_SCHEMA.md:142`) is the same sound key. Do not read PLR "Traction Control" as the TC aid.
2. `byte_7CE498` is named `AutoLoadConfigFile` [DERIVED] (`ask.py global 0x7ce498`, `docs/12-globals-reference.md:34`), with 47 readers and no writer in the corpus. It is not proven to be the options object. Nothing static ties it to aids.
3. `dbl_7C7428` is `PlayerPhysicsObject` [FACT] per `ask.py`, but `docs/12-globals-reference.md:3240` calls `0x7C7428` `RaceTimeElapsed`. Conflict, unresolved. Treat the block at `0x7C749A..0x7C74A4` as "global block near PlayerPhysicsObject", not a vehicle.
4. The initializer `this` comes from `UIControlWindowInitialization` (`UIControlWindowInitialization_004a70e0.c:1011`), so the "options object" is a UI window instance. Heap address; no fixed global.

## 3. Aid table

Value offsets are relative to the options object `a1` in `PlayerOptionsInitializer_004aa270.c`. Defaults and ranges are the values the initializer writes; PLR value is what the file currently holds.

| Aid | PLR key (Matias.PLR line) | Options offset (init line) | Runtime global (offset in BrakeBias block) | Type / range | Tier | Source |
|---|---|---|---|---|---|---|
| Steering Help (mode) | `Steering Help` (`:24`) | value a1+568 (`:106`) | not in 11-byte block | int 0..3 (Functionality) | FACT (offset), OPEN (global) | init 95-106 |
| Steering Help Functionality | `Steering Help Functionality` (`:25`) | a1+568 region, min 576, max 580 (`:101-106`) | none | int 0..3 (PLR=1) | FACT | init 101-106 |
| Throttle Control | `Throttle Control` (`:26`) | a1+604 (`:112`), max 616 | none found | int 0..2 (PLR=2) | FACT | init 108-112 |
| Brake Help | `Brake Help` (`:27`) | a1+640 (`:118`), max 652 | none found | int 0..2 (PLR=0) | FACT | init 114-118 |
| Traction Control (TC aid) | NOT IN PLR (sound key at `:125` is a different thing) | none found | `0x007CE6F4` `dword_7CE6F4` (cycle index 0 `'B'`), block +114 | int 0..2 (cycle max 2), level 0..2 | FACT (global/index), OPEN (PLR-to-global load) | `sub_42C440:49-50`, `BrakeBiasSetup:5` |
| Antilock Brakes (ABS) | `Antilock Brakes` (`:28`) | value a1+676 (`:124`), min 684, max 688 | `0x007CE73C` `dword_7CE73C` (index 1 `'C'`), block +115 | int 0..2 (PLR=2) | FACT | init 120-124; `sub_42C440:52` |
| Stability Control | `Stability Control` (`:33`) | a1+856 (`:154`), max 868 | `0x007CE7F0` `dword_7CE7F0` (index 2 `'>'`), block +116 | int 0..2 (PLR=2) | FACT | init 150-154; `sub_42C440:35` |
| Auto shifting (auto-shift) | `Auto Shift Override Time` (`:41`) is timing; the on/off PLR key is NOT identified | none identified | `0x007CE8A4` `dword_7CE8A4` (index 3 `'A'`, 3 states), block +117 | int 0..2 | OPEN (PLR key) | `sub_42C440:46`, `BrakeBiasSetup:15` |
| Clutch assistance (auto clutch) | `Auto Clutch` (`:36`) | a1+956 (`:171`), min 960, max 964 | `0x007CE850` `dword_7CE850` (index 4 `'W'`), block +118 | int 0..1 (PLR=1) | FACT (options), DERIVED (global link) | init 170-174; `sub_42C440:60` |
| Invulnerability | `Invulnerability` (`:30`) | a1+748 (`:136`), max 760 | `0x007CE784` `dword_7CE784` (index 5 `'@'`), block +119 | int 0..1 (PLR=1) | FACT | init 132-136; `sub_42C440:41` |
| Opposite Lock | `Opposite Lock` (`:32`) | a1+820 (`:148`), max 832 | `0x007CE7CC` `dword_7CE7CC` (index 6 `'<'`), block +120 | int 0..1 (PLR=1) | FACT | init 144-148; `sub_42C440:27` |
| Steering assist (steering help toggle, in-car) | `Steering Help` (`:24`) | a1+568 (see above) | `0x007CE6AC` `dword_7CE6AC` (index 7 `';'`), block +121 | int 0..2 | DERIVED (key map to PLR) | `sub_42C440:24`; `ObjectConstructor_1a00` |
| Braking point assist | none found in PLR | none | `0x007CE718` `dword_7CE718` (index 8 `'='`), block +122 | int 0..2 | OPEN (PLR key) | `sub_42C440:32` |
| Spin Recovery | `Spin Recovery` (`:29`) | a1+712 (`:130`), max 724 | `0x007CE760` `dword_7CE760` (index 9 `'?'`), block +123 | int 0..2 (PLR=0) | FACT | init 126-130; `sub_42C440:38` |
| Pit lane assist (Autopit / pit assist) | `Autopit` (`:31`) | a1+784 (`:142`), max 796 | `0x007CE7A8` `dword_7CE7A8` (index 10 `'D'`), block +124 | int 0..1 (PLR=0) | FACT (options), DERIVED (global link) | init 138-142; `sub_42C440:55` |
| No AI Control | `No AI Control` (`:34`) | a1+896 (`:157`), max 904 | none found | int 0..1 (PLR=0) | FACT | init 156-158 |
| Auto Lift | `Auto Lift` (`:37`) | not extracted | none | int | OPEN | `PlayerOptionsInitializer:176` |
| Auto Blip | `Auto Blip` (`:38`) | not extracted | none | int | OPEN | `PlayerOptionsInitializer:183` |
| Auto Reverse | `Auto Reverse` (`:42`) | not extracted | none | int (PLR=1) | OPEN | PLR only |
| Speed Sensitive Steering | `Speed Sensitive Steering` (`:43`) | not extracted | none | float (PLR=0.0) | OPEN | PLR only |
| Mechanical failures (7 series) | `[ Mechanical Failures ]` `CURNT/PRACT/QUICK/GPRIX/CHAMP/MULTI/RPLAY Failure Rate` (`:53-60`, all =2) | value at a1+1476+36*i, i=0..6 (`:264-282`) | none | int 0..3 (PLR=2 all) | FACT (options), OPEN (global) | init 264-282, `sub_4AA1E0` |
| Damage multiplier | `Damage Multiplier` (`:223`) | not extracted | none | int (PLR=100) | FACT (key), OPEN (offset) | init 1091 `MultiFieldSetterWithStrings` |
| Multiplayer aid mask | `Multi Allowed Aids` (`:339`) = -513 | not extracted | mask in object `this[2733]` (`BitmaskStateTest` `sub_4B15D0`) | bitmask; bit 9 (value 512) clear -> index 9 (Spin Recovery) disallowed when `this[2014]==5` | DERIVED | `BitmaskStateTest_004b15d0.c`; `BrakeBiasSetup:20-21`; OPEN for PLR-to-mask link |

Block mapping inside `0x7C7428` (BrakeBiasSetup, `BrakeBiasSetup_00438cc0.c:5-15`): +114 TC, +115 ABS, +116 stability, +117 auto-shift, +118 clutch assist, +119 invuln, +120 opposite lock, +121 steering assist, +122 braking point, +123 spin recovery, +124 pit lane assist. +128 = 2-bit flag from `byte_7CE88C`/`byte_7CE874` (FACT, not an aid by name).

## 4. Mid-session changes

- FACT: `sub_42C440_0042c440.c` walks a key queue `word_7CB21C` (count `dword_7CB218`) and calls `AIShiftPattern` for each command code (`;` `<` `=` `>` `?` `@` `A` `B` `C` `D` `W`, lines 24-60). Each call increments the global and wraps at max.
- FACT: `AIShiftPattern_0042b8d0.c:15` gates on `BitmaskStateTest(a1)`. When disallowed, it shows a "Disallowed" message (`:17-29`) instead of changing the value.
- FACT: after the cycle it calls `BrakeBiasSetup(&dbl_7C7428)` (`:111`), so the copied block refreshes on every toggle.
- Where it shows: `FormatStringBatchDisplay_0049e170` and `UIObjectConstructorWithDefaults_00471b20` (reader list of the globals). Text labels: `TractionControl`, `AntiLockBrakes`, `SteeringAssist`, `OppositeLockAs`, `StabilityAssis`, `SpinRecovery`, `Invulnerabilit`, `AutoShifting`, `ClutchAssistan`, `PitLaneAssista`, `BrakingPointAs` (strings in `sub_42C440`/`AIShiftPattern`).
- OPEN: whether the in-car key queue is fed by the HUD/menu only, and whether AI cars read these globals (the vehicle-side reader of `0x7C749A..` was not found).

## 5. Live-read recipe (for a mod, no writes)

- Read globals `0x007CE6F4` (TC), `0x007CE73C` (ABS), `0x007CE7F0` (stability), `0x007CE8A4` (auto-shift), `0x007CE850` (clutch assist), `0x007CE784` (invuln), `0x007CE7CC` (opposite lock), `0x007CE6AC` (steering assist), `0x007CE718` (braking point), `0x007CE760` (spin), `0x007CE7A8` (pit lane) as 4-byte ints (dword). Capture time and session-time freshness per the F1C MCP guide.
- Cross-check the block `0x7C7428+0x72..0x7C` after a toggle; it should match the globals within one frame if `BrakeBiasSetup` ran.
- Do NOT read PLR `Traction Control` as the TC aid. Read TC from the global above.
- Mechanical failures and damage multiplier: read from the options object, which is not a stable global. Use PLR values or a session-scoped hook. OPEN.
- Skill-rating use: record the 11 global values at session start and on every change (log the `AIShiftPattern` call). Driving-aid state is per-session; the PLR is only the saved default.

## 6. Open items (static cannot close)

1. How the PLR `[DRIVING AIDS]` values load into the 11 `dword_7CE*` globals at startup (no writer in corpus; check `PlayerFileInit_004b0540.c` and `UIObjectConstructor` paths).
2. Which PLR key maps to auto-shift (`dword_7CE8A4`) and braking point (`dword_7CE718`). Candidate: `Auto Clutch`/`Auto Lift`/`Auto Blip` group; unproven.
3. Whether `Multi Allowed Aids` (-513) writes `this[2733]` in the race object; whether that object is the same as `0x7C7428`.
4. The meaning of `dbl_7C7428` (PlayerPhysicsObject vs RaceTimeElapsed); conflict in corpus.
5. Live check (owner-approved only): read the 11 globals and the block at `0x7C749A` in a session where a toggle is pressed. Not done here (game not touched).

## 7. Corpus housekeeping

- Recommend the conflict in `docs/12-globals-reference.md:3240` and `ask.py` label for `0x7C7428` be reconciled. Not edited here (read-only task).
- Recommend `CONFIG_SCHEMA.md:142` note that `Traction Control` in `[ Sound Options ]` is a volume, not the TC aid. Not edited here.
- No findings appended to `findings.jsonl` in this read-only pass. Items 1-5 above are candidates for a dated checkpoint.

## 8. Files read

- `Matias.PLR` lines 23-60, 117-130, 216-232, 331-370 (values only).
- `functions/PlayerOptionsInitializer_004aa270.c`, `functions/sub_42C440_0042c440.c`, `functions/AIShiftPattern_0042b8d0.c`, `functions/BrakeBiasSetup_00438cc0.c`, `functions/BitmaskStateTest_004b15d0.c`, `functions/UIControlWindowInitialization_004a70e0.c`.
- `CONFIG_SCHEMA.md` (driving-aid rows), `docs/12-globals-reference.md`, `ask.py token/global/findings` output.
