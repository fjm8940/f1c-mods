# SPEC: Multiplayer Overhaul rename, id migration and CO-OP mode persistence (2026-10-13)

Status: SPEC ONLY. No code, ini, catalog, governance or operations file was edited.
The only created file is this one. Owner test, owner GO and release are pending.
Standard: `development\mods\MOD_STANDARD.md` (product id is snake_case and permanent, sections 1, 2, 4, 5, 7, 8).
Prior records: `docs\RENAME_INVENTORY_MULTIPLAYER_OVERHAUL_2026-10-13.md` (read-only inventory, see the
"Scope" note in section 1 below) and `docs\INVESTIGATION_2026-10-09.md` (menu spinner and mode enum).
Evidence tags: **FACT** = read in a file or decompile (path and address given). **INFERENCE** = reasoned,
not tested. **OPEN** = needs a live or owner check.

Note on the inventory path: the rename inventory is at
`development\mods\coop_mode\docs\RENAME_INVENTORY_MULTIPLAYER_OVERHAUL_2026-10-13.md` (not the root `docs\`).

---

## Part 1: Internal id standardization

### 1.1 Facts about the current id

- **FACT** The catalog id is `coop_mode`. It appears in the Lightloader `KnownMods` table
  (`LightloaderManager.cs:152`), the usage whitelist `UsageMods` (`LightloaderManager.cs:797`), the telemetry
  lists (`operations\vps\telemetry_v10_2026-10-10\deployed_artifact\telemetry_service.py:37,56`), the mod meta
  (`dll\coop_mode.meta.json`), `dll\mod.json`, `dll\f1c_mod.json`, `dll\coop_usage.h:25` (`COOPU_MOD_ID`) and
  `governance\PRODUCT_STATUS.json:377`.
- **FACT** Other catalog ids are the public name in snake_case: `mechanical_failures`, `red_flag`,
  `safety_car`, `pitstop_failures`, `esc_pause_menu`, `extended_damage_commentary`, `driver_ai_toolkit`,
  `controller_hub`, `web_telemetry`, `shared_broker`. `coop_mode` is the odd one out: it is a mode name, not a
  product name.
- **FACT** Lightloader keys everything by id: catalog lookup (`LightloaderManager.cs:1877-1886`), opt-in
  (`ManagerForm.OptIn(CatalogEntry(..., "coop_mode"))` in `SelfTest.cs:907`), and the usage filename pattern
  `^[a-z0-9_]{2,60}-...\.json$` (`LightloaderManager.cs:799`). The display name is a label only.
- **FACT** The opt-in flag is the catalog key `optIn` (`ManagerUi.cs:2551`). It hides work-in-progress entries
  from install-all. It is not a per-player saved choice.
- **FACT** Disabling uses `LightloaderDisabled.ini` (`LightloaderManager.cs:96`). The ini for mods is
  `coop_mode.ini` beside the DLL (`dll\coop_dinput8_proxy.cpp:11,18,36,45`).
- **FACT** The id regex is `^[a-z0-9_]{2,60}$` (`LightloaderManager.cs:154`). `multiplayer_overhaul` passes it.
- **FACT** The governance blocker `governance\PRODUCT_STATUS.json:394` says
  "Rename to multiplayer_ai_mod needs structural spec." That id is not the owner's target. It conflicts with
  the rename inventory (R2, R6).

### 1.2 Candidate ids

| id | Pros | Cons |
|---|---|---|
| `multiplayer_overhaul` | Matches the owner's public name in the standard snake_case form. Matches the pattern of all other catalog ids. No "AI" word to drift again. Forward-compatible if the mod grows beyond AI. | Id change: needs the migration in 1.3. Long-ish, but within the 60-char limit. |
| `coop` | Short, and the owner floated it. | "coop" is a feature (the CO-OP menu entry), not the product. It collides with the in-menu CO-OP label and with `coop_usage.h` naming. Would still need the same migration. |
| `coop_mode` (keep) | Zero migration. Every ini, log, telemetry row and status line stays valid. | The public name and the id disagree permanently. Standard section 2 says the id is the product id. Keeping a mode name as the id is what the standard was written to prevent. |

**Recommendation: `multiplayer_overhaul`.** It is the only option where the id, the public name and the
standard agree. The migration cost is one-time and bounded (1.3). If the owner rejects the migration cost,
fall back to keeping `coop_mode` and changing only the display name (rename inventory, class A). Do not
choose `coop`.

### 1.3 Migration design (proposed, not executed)

Every step is non-destructive. Nothing is deleted. Superseded material moves to quarantine.

1. **Governance first.** Copy the before-state of `governance\PRODUCT_STATUS.json` to
   `workspace_organization_audit\<phase>_BEFORE_2026-10-13\`. Then:
   - set `publicName` to `Multiplayer Overhaul`;
   - supersede line 394 (`Rename to multiplayer_ai_mod ...`) with a dated line: "superseded 2026-10-13 by owner
     decision: id `multiplayer_overhaul`, see this spec";
   - change `id` only at the release that ships the new id (step 4), under the evidence-change rule.
2. **Mod build (dev).** In `development\mods\coop_mode\dll`:
   - new DLL name `multiplayer_overhaul.dll`; ini `multiplayer_overhaul.ini`; log `multiplayer_overhaul_log.txt`;
   - `MOD_ID` and `COOPU_MOD_ID` become `multiplayer_overhaul`; `MOD_NAME` becomes `Multiplayer Overhaul`;
   - `mod.json`, `f1c_mod.json`, meta id and the proxy load names follow;
   - keep the old file names as a compatibility read path only for the ini (step 3), never as a second load.
   The source folder stays `development\mods\coop_mode` in this release (renaming a top-level or mod folder
   needs separate approval per `AGENTS.md`). Record that as a known inconsistency. Folder rename is a separate
   later decision.
3. **Ini migration (in the new mod).** On first start, if `multiplayer_overhaul.ini` is missing and
   `coop_mode.ini` exists beside the DLL, copy it to `multiplayer_overhaul.ini` (same keys and values, verbatim).
   Rename the old file to `coop_mode.ini.migrated-2026-10-13` in the same folder. This is a rename, not a
   delete. Log one line with the result. If the copy fails, leave the old file in place and run with defaults.
4. **Lightloader release (new folder, not the frozen 2.3.24 folder).** In a new release under
   `development\lightloader\v2.0.0-hub-dev.1\release_<what>_2026-10-xx\`:
   - `KnownMods`: `coop_mode.dll` maps to id `multiplayer_overhaul` with the new name (so an existing install is
     recognized under the new id, which carries the installed state over);
   - `UsageMods`: add `multiplayer_overhaul`; keep `coop_mode` for 0.9.x reports already on disk (no upload
     break);
   - the catalog entry for `coop_mode` is kept, marked superseded (see 1.4), and the new entry is added with
     `optIn: true` like today.
   Lightloader must not delete the old DLL. The update step moves `plugins\lightloader\coop_mode.dll` to
   `plugins\lightloader\_superseded_coop_mode_2026-10-13\` after the new DLL loads. It must run only with the
   game closed (file in use otherwise). Failure leaves the old DLL in place and logs it (quarantine-safe, matches
   `AGENTS.md`).
5. **Opt-in carry-over.** The player's intent is the installed state, not a separate flag. Because step 4 maps
   the old file to the new id, the manager sees the installed DLL under `multiplayer_overhaul` and shows it as
   installed. The opt-in test (`SelfTest.cs:907`) changes its id to `multiplayer_overhaul` in the same release.
   **OPEN:** confirm that the manager stores no separate per-id opt-in record on disk (only the catalog flag was
   found in source). If one exists, migrate it with the same copy-and-keep rule.
6. **Telemetry and usage continuity.** The usage filename pattern accepts any `[a-z0-9_]{2,60}` id, so both
   names pass. Telemetry (`telemetry_service.py` lists at lines 37 and 56) accepts both `coop_mode` and
   `multiplayer_overhaul` for at least one full release cycle. Old `coop_mode` rows stay as history. Add a
   `legacy_id` note in the server whitelist comment, not a rewrite of stored rows. Analytics queries group by
   `mod`, so report both ids together until the old id stops arriving. The install id and salt do not change.
7. **Catalog.** Republish the catalog with the new entry. The old `coop_mode` entry remains downloadable so
   installed 0.9.x users and rollback still work. Other mods' current notes that say "Needs Multiplayer AI Mode"
   change to the new name in the same publication (rename inventory R10). Dated history stays. The release
   registry gets a new line (`production\RELEASE_REGISTRY.md`), no old line is removed.
8. **Public text.** Privacy page paragraph (`publishing\website\privacy\mod-downloads.html:35`) and the operations
   label (`operations\README.md:136`) use the new name, after privacy review (rename inventory R5).
9. **Verification gates.** Lightloader self-test with the new id; a fresh-install test; an upgrade test from a
   0.9.6 install (ini copied, old DLL moved to quarantine, mod loads, opt-in shown); a usage-report test with
   both ids in `tests\test_usage.py`; the exact release DLL hash in game (owner test, `MOD_STANDARD` section 6).
10. **Rollback.** Restore the old catalog entry as current, move the superseded DLL back from
    `_superseded_coop_mode_2026-10-13\`, restore `coop_mode.ini.migrated-2026-10-13` to `coop_mode.ini`. Nothing
    is deleted, so every step is reversible.

Pending owner decisions: id choice (1.2), whether the folder rename waits, and privacy text timing.

---

## Part 2: CO-OP keeps its place across launches

### 2.1 Facts: how the mode is stored today

- **FACT** The game mode global is `dword_7D0410` (`0x007D0410`). Values: 0 CURRENT, 1 TEST DAY, 2 QUICKRACE,
  3 RACE, 4 CHAMPIONSHIP, 5 MULTIPLAYER, 6 DRIVING SCHOOL, 7 REPLAY PLAYBACK
  (`docs\INVESTIGATION_2026-10-09.md` section 1; `f1c_ida_decompiled/findings.jsonl` `game-mode-enum-7d0410-20261009`).
- **FACT** The spinner gizmo `APP_SpinBox_GameOpt_GameModeSelect` has vtable `0x005A2400`. Slot 40 `0x472D20`
  rebuilds the list, slot 43 `0x472FB0` syncs each frame, slot 46 `0x472E30` changes the item. Its selection is
  kept in `this+0x320` (slot 29, `0x473040`), memory only (`INVESTIGATION_2026-10-09.md` section 2).
- **FACT** The menu-return path `sub_404F30` (`functions\MainMenuTransition_00404f30.c`) copies a field of the
  transition record into `dword_7D0410` and clamps it to `[dword_7D0418, dword_7D041C]`. The record is in
  memory, not read from a file in this function.
- **FACT** The other writers of `dword_7D0410` found in `functions\*.c`: `AutoJoinRetryLoop_004637a0.c` (sets 5),
  `RaceModeSetup_00404890.c` (sets 2), `sub_463BC0_00463bc0.c` (sets 5, the MP path), `sub_4A0490_004a0490.c`
  (case 6 sets 1). None of them calls a file read or write.
- **FACT** The PLR reader `PlayerOptionsInitializer_004aa270` reads `ProfileVehicle`, `AutoLift`,
  `TelemetryF1Tel`, `ReconTimer` (`docs\12-globals-reference.md` lines 8183-8186). No mode key was found in the
  PLR path. The string `GameMode` (`DAT_005c5bfc`) is used in the network status text (`SceneTextureApply_0045cf50.c`
  writes "GameMode" from a network entry), not as a saved setting. The generic `Mode` field parser
  (`ConfigIniParser_00421cd0.c`, `DAT_005c2afc`) was not traced to a menu key.
- **FACT** The mod's CO-OP flag is `static volatile LONG g_coop` in `dll\coop_mode.cpp` (memory only).
  `coop_mode_is_selected()` returns `coop_sanitize(*game_mode_ptr(), g_coop)`. The spinner index 8 ("CO-OP")
  exists only in the mod (`dll\coop_menu_logic.h:15`, `COOP_SPINNER_INDEX = 8`).
- **FACT** `dll\coop_mode.ini` has no key that stores the menu selection. Sections: `[Mod]`, `[Prototype]`,
  `[CoOp]`, `[Net]`, `[Log]`. The header says "Only keys listed here are read."

### 2.2 Why CO-OP is lost on relaunch

- **FACT** CO-OP is mode 5 plus the mod flag. The flag is zero at process start. The game mode restored at the
  menu is 5 (MULTIPLAYER), so the spinner shows MULTIPLAYER, not CO-OP.
- **FACT** Nothing writes the flag to disk. Every write to `g_coop` is in `spinner_change_hook` and sync.
- **OPEN** Which value `dword_7D0410` holds at the first menu of a new process (the default). Not traced.
  If it is not 5 (for example 1 TEST DAY), the mod must set mode 5 at menu load, which is more than a flag
  restore.

### 2.3 Proposed persistence mechanism

Store the selection in the mod's own ini and restore it when the menu is built. Vanilla is not changed.

1. **Save.** In `spinner_change_hook`, in the `handled` branch, after `InterlockedExchange(&g_coop, coop)`, write
   `[Menu] LastSelected=coop` or `LastSelected=multiplayer` to `multiplayer_overhaul.ini`. Use an atomic write
   (write `*.tmp` beside it, then `MoveFileEx` with replace). Write only when the value changes. Do this on the
   game thread, the menu runs there. Only the mod's own ini is touched.
2. **Restore.** Read `[Menu] LastSelected` once at mod start (same as other `[...]` keys with
   `GetPrivateProfileStringA`). In the first `spinner_sync_hook` call of a menu visit (a one-shot flag reset when
   the menu is left):
   - if `LastSelected=coop` and `*game_mode_ptr() == 5` and `g_coop == 0`, set `g_coop = 1` and call the rebuild
     hook so the label shows CO-OP;
   - if `LastSelected=multiplayer`, do nothing (vanilla).
   If the mode is not 5 at that point, do not force it (log `COOP-M01 restore skipped: mode %d`). This keeps the
   restore from overriding a vanilla choice that the player made in a different way.
3. **Respect the switches.** `[Mod] Enabled=0` disables both save and restore. Restore never runs in an online
   session: it runs only at the main menu, before a race is created.
4. **Log.** One line per save and per restore, with the value, not the path.
5. **Do not touch** the connection-type spinner (vtable `0x005A3F00`, `coop_mode.cpp:5348`), the MP host pages or
   the `MainMenuTransition` write. The restore reads the spinner's own state through the sync hook.

Alternative considered: save the whole `dword_7D0410` value. Rejected: mode 5 alone does not say CO-OP, and
restoring other modes would fight vanilla.

### 2.4 Risks and checks

- **R-P1 (OPEN).** `MainMenuTransition` may overwrite `dword_7D0410` after the restore. The sync hook runs each
  frame, so the restore must be re-checked on each visit, not only at load. Test: CO-OP, quit to desktop, relaunch,
  open the main menu, leave to the race screen and back.
- **R-P2 (OPEN).** A new process may start with `dword_7D0410` not equal to 5. Then the restore is skipped and
  CO-OP is still lost. Needs the default-value trace (2.2) before the fix is final.
- **R-P3.** Two copies (the mod and Lightloader) must not both write `LastSelected`. Only the mod writes it.
- **R-P4.** Renaming the ini (Part 1) and adding `[Menu]` must be in one release. Otherwise the key is written to
  the old file and the migration copy misses it.
- **R-P5.** The MP client: a joiner who opens the menu with CO-OP saved must not be armed. The existing rule
  (bots arm only when CO-OP is picked, `coop_mode_is_selected`) must still hold, so the restore must not change the
  client's armed state. Test with two PCs (pending owner test).

### 2.5 Pending validation (no game run in this spec)

1. Live: pick CO-OP, close the game, relaunch, confirm CO-OP is shown and armed. Confirm that a MULTIPLAYER choice
   relaunches as MULTIPLAYER.
2. Live: record the value of `dword_7D0410` at the first menu of a fresh process (default, R-P2). Use the SDK
   front door (`python tools\f1c_mod.py start --json`, then `evidence-query findings`) before any new read.
3. Record the new findings in `f1c_ida_decompiled\findings.jsonl` (the "default mode at launch" result, and
   "no PLR mode key"), with tier and source, as `AGENTS.md` requires.

---

## Documentation gate

- Durable record: this file.
- Indexing: not yet added to `development\mods\coop_mode\README.md` (owned by another agent, not edited here).
  Add an index line when that agent is done.
- Governance: `governance\PRODUCT_STATUS.json` not edited. It changes only at the id release (1.3 step 1).
- Fact/inference separation: section headers and tags above.
- Validator: not re-run for this task (spec only, no structural change). The cold-start validator reported two
  pre-existing errors (`.claude`, `plans` top-level dirs) in the rename inventory. Not caused by this spec.
- Restoration: delete this file. No other state changed.


---

## Implementation record (2026-10-13, Haiku agent)

Status: implemented in source and built to %TEMP% only. Not deployed, not promoted, not run in the game.
Before-state copies: `*.BEFORE_20261013_multiovh` beside each edited file, and the audit set in
`workspace_organization_audit\multiplayer_overhaul_BEFORE_20261013\` (PRODUCT_STATUS.json, development mods README.md, this spec).

### Changed files
- `development\mods\coop_mode\dll\coop_mode.cpp`: MOD_ID `multiplayer_overhaul`, MOD_NAME `Multiplayer Overhaul`; ini/log names follow MOD_ID; legacy ini migration (`coop_migrate_legacy_ini`, COOP-M01/M02); `[Menu] LastSelected` save (`coop_save_menu_choice`, temp file then replace, COOP-M03) and restore (spinner sync hook, once per process, max 600 frames, mode 5 and network_state 0 only).
- `dlluild.bat`: output `multiplayer_overhaul.dll`, default OUT `%TEMP%\multiplayer_overhaul_build`. Note: the file has LF-only line endings and fails under cmd.exe; it was not used. The same compile lines were run directly with the MinGW g++ and windres (follow-up).
- `dll\coop_dinput8_proxy.cpp`: loads `multiplayer_overhaul.dll` (falls back to `coop_mode.dll`); Enabled read from `multiplayer_overhaul.ini`, falling back to `coop_mode.ini`.
- `dll\mod.json`, `dll1c_mod.json`, `dll\coop_mode.meta.json`: id, name, dll path, ini destination. `coop_mode.ini` header comment. `coop_mode_README.txt`: version line fixed to 0.9.7-dev1 (MOD_VERSION; the old 0.9.4 line was stale), install line renamed.
- `lightloader2.0.0-hub-dev.1\manager_cs\LightloaderManager.cs`: KnownMods labels, new key `multiplayer_overhaul.dll` (id stays `coop_mode`), UsageMods accepts `multiplayer_overhaul` as well as `coop_mode`, `Core.MigrateCoopIni` and `Core.SupersedeCoopDll`.
- `ManagerSchema.cs`: Coop file `multiplayer_overhaul.ini`, labels. `SelfTest.cs`: file names, new migration test block. `ManagerUi.cs`: migration on startup and on folder change.
- `governance\PRODUCT_STATUS.json`: publicName `Multiplayer Overhaul`; old blocker line superseded by a dated decision line. Stage and id unchanged.
- `development\mods\README.md`: line 15 superseded with the owner decision.
- `f1c_ida_decompiledindings.jsonl`: 3 rows appended (coop-menu-mode-restore-globals-20261013 FACT, coop-menu-mode-at-process-start-20261013 OPEN, coop-menu-lastselected-persist-20261013 INFERENCE).

### Mode-persist correction (verified)
- The owner's "saved globals" wording is only partly right. sub_404F30 restores `dword_7D0410` from an in-memory transition record, clamped to bounds 0x7D0418/0x7D041C. No file I/O. Bounds are set in-process. (FACT, findings row 1.)
- The first-process default is still OPEN. CO-OP is lost because `g_coop` is memory-only.

### Hashes (build outputs in %TEMP%, not in the workspace)
- multiplayer_overhaul.dll: fe3e823237a482c9b0a0751cabd73f65854fc85fa86782b12dae633992536551
- dinput8.dll (proxy): 62c82e6ed2d93486e5e714e9b6e047a183d90be0e3c0ec93120729a7c5040895
- LightloaderManager.exe: 49429c84ca61fea29e29c40b7cd3c5c3a54da9b0f8e8d76e2df6ad5224a531c1

### Tests
- coop_mode pytest (`development\mods\coop_mode	ests`): 5 passed.
- Manager `--self-test`: 226 PASS, 1 FAIL. The failure is "embedded loader is the broker-built-in build": the pinned hash in SelfTest.cs:288 (D32050E4...) does not match `payload\dinput8.dll` (3C7590C7...). Neither file was changed here. Pre-existing, not fixed.
- New manager migration checks (copy, `.migrated` keep, idempotent, DLL to `_superseded`): PASS.
- Workspace validator: only the 2 pre-existing errors (`.claude`, `plans`).

### Deviations from the spec
- Restore runs once per process, not on every menu visit (R-P1 still open).
- `[Menu]` save uses a copy-then-replace, not a pure atomic write. Rename is not done for the DLL folder.
- Lightloader catalog id stays `coop_mode` (catalog not republished). KnownMods maps `multiplayer_overhaul.dll` to that id.
- `COOPU_MOD_ID` stays `coop_mode` (server whitelist is not changed here).
- coop_mode.meta.json version still 0.9.6 (DLL is 0.9.7-dev1). Owner's 1.0.0 release not done.
- Folder `development\mods\coop_mode` not renamed (needs approval).

### Server whitelist proposal (not applied; operations\ is live)
In `operationsps	elemetry_v10_2026-10-10\deployed_artifact	elemetry_service.py` and the newer telemetry_service (v14), add `multiplayer_overhaul` to the lists at lines 37 and 56 next to `coop_mode`. Keep `coop_mode`. Owner or ops to deploy.

### Pending validation (live, owner)
1. Pick CO-OP, quit, relaunch: CO-OP shown and armed. Pick MULTIPLAYER, relaunch: MULTIPLAYER shown.
2. Record `dword_7D0410` at the first menu of a fresh process (R-P2).
3. Joiner with CO-OP saved in the ini must not arm (R-P5, two PCs).
4. Migration from an installed 0.9.x (coop_mode.ini to multiplayer_overhaul.ini, old DLL to `_superseded_coop_mode_2026-10-13`).

### Restoration
Restore each `*.BEFORE_20261013_multiovh` over its file (and the audit copies). Nothing is deleted. To undo the ini migration, rename `multiplayer_overhaul.ini` to a `.reverted` name and rename `coop_mode.ini.migrated-2026-10-13` back to `coop_mode.ini`. Superseded DLLs are in `_superseded_coop_mode_2026-10-13\`.

### Documentation gate
- Durable record: this section. Findings: 3 rows in f1c_ida_decompiled/findings.jsonl.
- Not done: index line in `development\mods\coop_mode\README.md` (owned by another agent); README still names coop_mode_log.txt and the old MOD name in places. Pending.

### Wasted-effort check
Repeated reads of the same large decompile output; first build failed on a LF-only batch file and a NUL char in a source literal (fixed); one JSON edit failed on escapes (fixed, file unchanged).

### Review note (Claude, 2026-10-13)
- build.bat converted LF -> CRLF so it runs under cmd (before-state dll/build.bat.BEFORE_20261013_crlf). Not re-run.
- Version stays 0.9.7-dev1 in dev; release packaging sets 1.0.0 (owner) in MOD_VERSION and coop_mode.meta.json; meta description "WORK IN PROGRESS" to be rewritten at release.
- Mode persistence: vanilla restore source of MULTIPLAYER across relaunch still unproven (agent: in-memory record only); live test decides.
