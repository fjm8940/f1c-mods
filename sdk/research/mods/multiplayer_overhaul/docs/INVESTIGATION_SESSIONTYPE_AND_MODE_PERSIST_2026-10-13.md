# Investigation: SessionType enumeration and multiplayer mode persistence (2026-10-13)

Status: READ-ONLY investigation. No code, ini, DLL, Lightloader, governance or game file was edited.
The game was not launched and no runtime or telemetry test was run (another session needs the game closed).
Scope: static evidence from `f1c_ida_decompiled` and the exe bytes (read-only), plus existing live rows in
`f1c_ida_decompiled/findings.jsonl`. Findings appended: 13 rows, ids listed in section 7.

Evidence tags:
- **PROVEN**: read in a decompiled file, exe bytes or disassembly, or an existing live FACT row (path or address given).
- **INFERRED**: reasoned from PROVEN facts, not run in game.
- **OPEN**: needs a live or further static check.

Exe used: `F1 Challenge V10\F1 Challenge Online.exe` (read-only PE parse and `exe-disassemble`).
Addresses are VA (image base 0x400000). Corpus paths are under `f1c_ida_decompiled\functions\`.

---

## Q1. Warm-up SessionType value and the full enumeration

### 1.1 Answer

| SessionType (0x007C56A4) | Name | Evidence |
|---|---|---|
| 0 | TestDay | PROVEN (static `aTestDay`; live `test-day-session-type-0`) |
| 1 | Practice1 | PROVEN (static; live P1 read 1) |
| 2 | Practice2 | PROVEN (static; live P2 read 2) |
| 3 | Practice3 | PROVEN (static; live P3 read 3) |
| 4 | Practice4 | PROVEN (static; live P4 read 4) |
| 5 | Qualify | PROVEN (static; live qualifying read 5) |
| **6** | **Warmup** | **PROVEN** (static case 6 -> `aWarmup`; live warm-up read 6) |
| 7 | Race | PROVEN (static; live race read 7) |
| other | Unknown | PROVEN (static default `aUnknown`) |

Hot Lap is **not** a SessionType value. There is no hot-lap session value in the table.

### 1.2 Static evidence (PROVEN)

- `functions\SessionTypeNameLookup_0042ce30.c` (`sub_42CE30(int a1, char a2)`): switch on `a1`.
  - Arm `a2 != 0`: case 0 `aTestDay`, 1-4 `aPractice1`..`aPractice4`, 5-7 jump to the lower arm labels.
  - Arm `a2 == 0`: case 0 `aTestday`, 1-4 `aPractice1_0`..`aPractice4_0`, case 5 `aQualify`, case 6 `aWarmup`, case 7 `aRace_0`, default `aUnknown`.
- Label literals located by byte search in the exe (file offset = VA - 0x400000 - section delta; matches the
  string addresses in `docs\12-globals-reference.md`, e.g. `DAT_005C3510` = TestDay, `DAT_005C3504` = Practice1):
  `Warmup` file 0x1C34D0, `Qualify` 0x1C321C, `Race` 0x1BE5A8, `TestDay` 0x1C3510, `Practice1..4` 0x1C3504..0x1C34E0.
- Callers of the name lookup (for display only, not gating): `Steward_Restart_004d3cf0.c:109`,
  `sub_4FE350_004fe350.c:57-84`, `WeatherConditionsFileWriter_004fe820.c:19`. `WeatherConditionsFileWriter`
  writes the weekend weather timeline `LOG\weather.txt` with one block per session 0..7 (`web_telemetry` finding
  `game-writes-log-weather-txt-timeline-20261012`, CONFIRMED).

### 1.3 Live evidence (existing FACT rows, not re-run today)

- `session-type-ladder-1-7-live-20261013` (FACT): owner scrolled the weekend P1, P2, P3, P4, Qualifying,
  Warm-up, Race; `session_type` read 1, 2, 3, 4, 5, 6, 7 through the f1c-memory reader, 2026-10-13-.
  This is the only live read of Warm-up = 6 and it is FACT in the store.
- `test-day-session-type-0` (FACT, 2026-08-07): Test Day reads 0.
- `session-switch-p1-to-p2-no-1e9-20261013` (FACT): switching P1 -> P2 changed 1 -> 2 with no 1e9 sim-time jump.

### 1.4 Practice 1..4: distinct values or one value plus a counter? (PROVEN)

- Practice 1-4 are **four distinct values (1, 2, 3, 4)**. The static table has four separate cases and the
  live ladder read 1, 2, 3, 4 in order.
- **There is no separate practice counter.** The practice index is the SessionType value itself.
- The statement "Practice 1-4 all share value 1" (`replay-warmup-session-value-open-20261013`, OPEN) is
  **DISPROVEN** by the table and the ladder. A new row `practice-1-4-distinct-values-no-counter-20261013` records it.
- The weekend session set is configured in the player file, section `[ Race Conditions ]`, keys `Run Practice1`,
  `Run Practice2`, `Run Practice3`, `Run Practice4`, `Run Qualify`, `Run Warmup`. The reader is
  `functions\PlayerOptionsInitializer_004aa270.c` lines 282-312 (`aRunPractice1`..`aRunWarmup`). Key names were
  read from `Save\Matias\Matias.PLR`; values were not read. (PROVEN: keys and reader. INFERRED: those flags select
  which sessions appear in the weekend.)

### 1.5 Native recorder arm check (PROVEN)

- `functions\sub_4BA460_004ba460.c` lines 16-18: `if ((dword_7D0038 || dword_7D005C) && dword_7C56A4 == 7)`.
- Disassembly (read-only `exe-disassemble`, exe above):
  - `0x4BA46E` `call 0x461DB0` (SessionStateCheckWrapper on `0x7CD25C`).
  - `0x4BA477..0x4BA487` tests of `[0x7D0038]` then `[0x7D005C]`.
  - **`0x4BA489` `833da4567c0007` = `cmp dword ptr [0x7c56a4], 7`**, `0x4BA490` `jne 0x4BA49B` (not armed),
    `0x4BA492` `mov [ebp], 1` (armed).
- Consequence (DERIVED from the above): only SessionType 7 arms the native recorder. Warm-up (6), Practice (1-4),
  Qualifying (5) and TestDay (0) do not write a native replay.

### 1.6 Hot Lap (PROVEN for the writer, INFERRED for its gate)

- Hot-lap file writer: `functions\sub_4B7DE0_004b7de0.c` (`aHotLap`, `aSavingHotlap`, `sub_566940(path, type, 0, 1)`).
  The visible code gates on `dword_7C54E8 + 16540` (> 0), not on SessionType. The PLR option `RecordHotlaps`
  (0x5D05BC) is the owner-side switch (`replay-no-all-sessions-option-2026-10-13`).
- INFERRED: hot-lap files come from a separate writer and do not use the arm gate. The owner's report that
  qualifying overwrote a replay is still unreproduced (`replay-practice-qual-no-file-2026-10-13`).

### 1.7 Where SessionType gets its value (PROVEN start, OPEN advance)

- The global is in the BSS (see 2.2), so it is 0 at process start.
- The only direct assignment found in `functions\*.c` is `sub_42CE00` (`functions\Thunk_0042ce00.c` line 8):
  `dword_7C56A4 = 1`, with `dword_7C569C = 1`, `dword_7C56A0 = 3`, `dword_7C56A8 = 0`, `dword_7C56B8 = -1`.
- The live advance P1 -> P2 -> ... -> Race (1 -> 2 -> ... -> 7) is **proven live** but its **writer is OPEN**: no direct
  assignment to `0x7C56A4` exists in the decompiled functions corpus, so the write is indirect (pointer, block copy
  or struct field).

### 1.8 Replay naming impact (INFERRED from 1.1-1.5)

`development\lightloader\v2.0.0-hub-dev.1\replay_naming_dev_2026-10-13\replay_naming.c` `rn_session_token()`
currently maps 7 -> Race, 5 -> Qualifying, 1 -> Practice, else `S<n>`. So warm-up prints `S6`. With the FACT above
the mapping can be exact (section 5.1). The naming spec `REPLAY_NAMING_SPEC_2026-10-13.md` sections 19.3 and 19.7
still list warm-up and practice values as OPEN; they are now answered. That spec was not edited here.

---

## Q2. Where the game persists the selected multiplayer mode across relaunch

### 2.1 Answer

**PROVEN: the native game does not persist MULTIPLAYER (mode 5) across relaunch.** The mode lives in memory in
`dword_7D0410`. The only native persisted multiplayer state is connection settings in the player file.

### 2.2 The mode global and its start value (PROVEN)

- `dword_7D0410` = `0x007D0410` game mode (0 CURRENT, 1 TEST DAY, 2 QUICKRACE, 3 RACE, 4 CHAMPIONSHIP,
  5 MULTIPLAYER, 6 DRIVING SCHOOL, 7 REPLAY PLAYBACK; `game-mode-enum-7d0410-20261009`, FACT).
- Clamp bounds `dword_7D0418` = `0x7D0418`, `dword_7D041C` = `0x7D041C`.
- PE parse of `F1 Challenge Online.exe`: `.data` VA 0x5BE000 with raw size 0x2D000. RVA 0x3D0410 (= 0x7D0410)
  lies beyond the raw data, so it is **zero-initialized (BSS)**. The bounds and `0x7C56A4` are in the same
  region. **At process start the mode is 0 (CURRENT)** until a native writer runs.

### 2.3 All writers of dword_7D0410 (PROVEN, all in memory)

| Writer | Address / file | Value | File I/O |
|---|---|---|---|
| `sub_404F30` MainMenuTransition | `functions\MainMenuTransition_00404f30.c` lines 9-24 | copies record field `[21]` (record `[20]` and `record[8]==1`), clamped to `[0x7D0418, 0x7D041C]` | none |
| `AutoJoinRetryLoop_004637a0` | `functions\AutoJoinRetryLoop_004637a0.c` 27-35 | 5 (bounded) | none |
| `RaceModeSetup_00404890` | `functions\RaceModeSetup_00404890.c` 9-17 | 2 | none |
| `sub_463BC0_00463bc0` | `functions\sub_463BC0_00463bc0.c` 8-16 | 5 (MP path) | none |
| `sub_4A0490_004a0490` | `functions\sub_4A0490_004a0490.c` 38-46 | 1 (case 6) | none |

The "saved globals" idea is only partly right: `sub_404F30` restores the mode from an **in-process transition
record**, not from a file (`coop-menu-mode-restore-globals-20261013`, FACT).

### 2.4 Where the native game does persist multiplayer (PROVEN)

- The per-profile file `Save\<name>\<name>.PLR`, section `[ Multiplayer Options ]`, has keys
  `Net Connection Type` (literal at exe 0x1CFB98) and `Multiplayer Enum Type` (literal at exe 0x1CFB68, LAN/Internet).
  Reader: `functions\PlayerOptionsInitializer_004aa270.c` lines 1247-1249
  (`SettingsPathBuilder(a1+10648, aMultiplayerOpt)`, `MultiFieldSetterWithStrings(aNetConnectionT, ...)`,
  `MultiFieldSetterWithStrings(aMultiplayerEnu, 1, 0, 1, a0Lan1Internet, 1)`).
- The same `[ Multiplayer Options ]` section also holds `Closed MP Qualify Sessions`, `Server Session End Timeout`,
  `Gamelist AutoRefresh (LAN)`, `Throttle Upstream Override`, `Net Data Rate`, `Net Flush Threshold`, and others.
- The owner profile `[ Game Options ]`, `[ Multiplayer Options ]` and `[ Options ]` were scanned for key names
  containing mode, menu, last or select: **no game-mode or spinner key was found**. Per-mode race settings
  use the prefixes `CURNT`, `PRACT`, `QUICK`, `GPRIX`, `CHAMP`, `MULTI`, `RPLAY` (for example `MULTI Race Laps`).
- `Config.ini` (game root) has sections `COMPONENTS`, `INSTALL`, `AUTOLAUNCH`, `VERSION` only. No multiplayer mode key.
- Registry: not searched in this pass (OPEN, low priority, the code writers above are all in memory).

INFERRED: the seven per-mode prefixes map to modes 0, 1, 2, 3, 4, 5, 7 (mode 6 DRIVING SCHOOL has no prefix). This is
a naming inference only and does not persist the mode.

### 2.5 What the mod assumes (PROVEN from source)

- `development\mods\coop_mode\dll\coop_mode.cpp`:
  - line 267: `ADDR_GAME_MODE = 0x007D0410`.
  - lines 297-301: `[Menu] LastSelected` state and `g_menu_restore_frames` (wait for mode 5, max 600 frames).
  - lines 324-340: `coop_save_menu_choice()` writes `[Menu] LastSelected` with `WritePrivateProfileStringA` to a temp
    file and replaces the target (the mod's own ini only).
  - lines 356-378: restore loop. It re-arms CO-OP (`g_coop = 1`) only when `*game_mode_ptr() == 5 && network_state() == 0`.
    **It never writes the mode.** On a skip it logs `menu: CO-OP restore skipped: mode %ld, network %ld`.
  - lines 391-395: spinner-change hook writes `*game_mode_ptr() = mode` and saves `LastSelected`.
  - line 827: plain MP detection `g_plain_mp && *game_mode_ptr() == 5 && !coop_mode_is_selected()`.
- The spec `docs\SPEC_RENAME_AND_MODE_PERSIST_2026-10-13.md` section 2.2 lists the default of `dword_7D0410` at the first
  menu as OPEN (R-P2). Section 2.2 is now partly answered (2.6 below).

### 2.6 Why CO-OP is lost on relaunch (PROVEN + DERIVED)

- `g_coop` is memory-only (PROVEN, spec 2.1).
- At process start `dword_7D0410 = 0` (PROVEN, 2.2). The restore waits for `== 5`. Nothing in the native corpus
  writes 5 on a cold start before the menu (the 5-writers are `AutoJoinRetryLoop`, `sub_463BC0`, which are join/MP
  paths, and the transition record). So **on a fresh process the restore is expected to skip after 600 frames**
  (DERIVED, live test needed).
- Caveat for any fix (INFERRED from 2.3): `sub_404F30` clamps the mode to `[dword_7D0418, dword_7D041C]`. Both bounds are
  0 at start (BSS). A mode 5 written before the bounds are set would be clamped to 0 on the next menu transition.
  The bounds are set in-process by `RaceModeSetup`, `AutoJoinRetryLoop`, `sub_463BC0` and `sub_4A0490` (2.3).

---

## 3. Results against the question

| Question | Result | Tier |
|---|---|---|
| Warm-up SessionType value | **6** | PROVEN (static case 6 + live ladder 2026-10-13) |
| Full enumeration | 0 TestDay, 1-4 Practice1-4, 5 Qualify, 6 Warmup, 7 Race, other Unknown | PROVEN |
| Hot Lap session value | none (hot lap is a separate writer) | PROVEN (no session value), INFERRED (gate) |
| Practice 1-4 distinct or counter | distinct values 1-4, no counter | PROVEN |
| Where the practice counter lives | does not exist; the value is the index | PROVEN |
| Native recorder arm | `cmp [0x7C56A4],7` at 0x4BA489, race only | PROVEN |
| Who advances SessionType in the weekend | not identified statically | OPEN |
| Native persistence of MULTIPLAYER mode | none; mode 5 is memory-only | PROVEN |
| What the native game persists for multiplayer | PLR `[ Multiplayer Options ]` connection settings only | PROVEN |
| Mode value at first menu of a fresh process | BSS 0 at start; first writer not traced | PROVEN (start), OPEN (first menu) |
| Mod restore works on a cold start | expected to skip (no native writer of 5 before the menu) | DERIVED, live test needed |

---

## 4. Ruled out (negative results, kept for future agents)

- Registry as the mode store: not searched; the writers in 2.3 are all in-process, so a registry store would need a
  writer that is not in the corpus (low likelihood, not proven absent).
- `Config.ini` mode key: none (sections listed in 2.4).
- PLR `[ Game Options ]` / `[ Options ]` mode key: none found by key-name scan of the owner profile.
- `sub_404F30` as a file reader: no file I/O in the function (`MainMenuTransition_00404f30.c`).
- "Practice share value 1": disproven (1.4).
- "Hot Lap" as a SessionType value: no such value in the table.
- Recorder arm for practice, qualifying or warm-up via an option: no option flag bypasses the `== 7` compare
  (`replay-no-all-sessions-option-2026-10-13`); a patch is required (not proposed here).

---

## 5. Recommended code changes (NOT implemented)

### 5.1 Replay naming (`development\lightloader\v2.0.0-hub-dev.1\replay_naming_dev_2026-10-13\replay_naming.c`)

1. `rn_session_token()`: add `0 -> "TestDay"`, `2/3/4 -> "Practice2/3/4"`, `1 -> "Practice1"` (currently `Practice`
   for 1 only), `6 -> "Warmup"`. Keep `S<n>` only for unknown values.
2. Note: only 7 arms the native writer, so these tokens matter only for the Lightloader rename path (Option B) if
   non-race recording is ever enabled (requires the arm patch in 1.5, which is a separate risk decision).
3. Update `REPLAY_NAMING_SPEC_2026-10-13.md` sections 19.3 and 19.7 to cite the FACT rows above. Before editing, copy the
   before-state to `workspace_organization_audit\<phase>_BEFORE_2026-10-13\` (AGENTS rule). Not done here.

### 5.2 CO-OP restore (`development\mods\coop_mode\dll\coop_mode.cpp`, lines 367-378)

1. Make the restore able to act on a cold start. Options, in order of preference (owner decision needed):
   a. When `[Menu] LastSelected=coop` and the menu is at the spinner, set the mode to 5 through the same path the
      spinner-change hook uses (lines 391-395), after checking the bounds `0x7D0418 <= 5 <= 0x7D041C`. If the bounds
      are not set yet, wait for them rather than writing 5 (see the clamp caveat, 2.6).
   b. Or keep the wait-for-5 loop but log the mode at the first sync (diagnostic only), then decide after the live run.
2. Add one log line at the first spinner sync: `menu: first sync mode=%ld bounds=%ld..%ld network=%ld`. This answers
   the OPEN "first menu value" without a behaviour change.
3. Keep the mod's own ini as the only store. Do not write `Matias.PLR`; the native `[ Multiplayer Options ]` section is
   the game's file and holds connection settings the game reads.
4. Do not reuse `Net Connection Type` for the CO-OP flag; it is LAN/Internet, a different axis.
5. Safety: keep the rule that the restore never arms a joiner (spec R-P5). Keep `network_state() == 0`.

### 5.3 Session-type helpers in the mod (if any)

- Any mod code that gates on `SessionType == 7` (race) for warm-up or practice must treat 6 as Warmup, not race.
  No such code was reviewed in this pass (OPEN, not checked).

---

## 6. Pending validation (owner or live, not run here)

1. Fresh process: read `0x7D0410` at the first main menu and after each spinner change (expected 0 at first, OPEN).
   Use the f1c-memory reader or the SDK read-only path, not a write.
2. Live: pick CO-OP, quit, relaunch, confirm CO-OP is shown and armed. Pick MULTIPLAYER, relaunch, confirm MULTIPLAYER.
3. Static: xref search for the advancing writer of `0x7C56A4` (indirect write), to close the weekend-sequencing question.
4. Live (owner): replay naming on a warm-up session should produce no native file (arm gate), so no rename is expected.

---

## 7. Findings appended to `f1c_ida_decompiled\findings.jsonl` (2026-10-13)

13 rows, append-only, one write. Ids: `sessiontype-enum-static-table-20261013` (FACT),
`warmup-sessiontype-6-proven-20261013` (FACT), `practice-1-4-distinct-values-no-counter-20261013` (FACT, supersedes the
"share value 1" claim), `replay-arm-gate-disasm-verified-20261013` (FACT), `warmup-no-native-replay-arm-20261013`
(DERIVED), `sessiontype-start-zero-init-writer-20261013` (FACT), `weekend-run-flags-plr-race-conditions-20261013` (FACT),
`mode-global-bss-zero-at-start-20261013` (FACT), `mode-persist-native-plr-connection-only-20261013` (FACT),
`mode-writers-no-file-io-20261013` (FACT), `coop-restore-cannot-set-mode5-20261013` (DERIVED),
`mode-at-first-menu-open-20261013` (OPEN), `weekend-sequence-advance-writer-open-20261013` (OPEN).

The file has 15 pre-existing `//` header comment lines (not JSON). Those were not changed. All 13 new rows parse.

## 8. Documentation gate and limitations

- Durable record: this file. Discoverable from the coop_mode spec (`SPEC_RENAME_AND_MODE_PERSIST_2026-10-13.md`) only
  after a pointer is added; the coop_mode README index was NOT edited (another agent owns it). Pending.
- Governance: `governance\PRODUCT_STATUS.json` not edited (no stage change).
- Workspace validator: `tools\workspace\check-workspace-structure.ps1` reports 2 pre-existing errors (top-level `.claude`
  and `plans`). Not caused by this task.
- Fact/inference separation: section tags above.
- Restoration: this file and the 13 appended findings rows can be removed by restoring `findings.jsonl` from its
  `.BEFORE_` copy (none was made for this append; remove the last 13 lines to revert).
- Limitation: live rows used here (ladder, P1->P2, test day) come from the existing store and were not re-observed today.
