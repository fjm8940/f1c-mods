# Rename inventory: "Multiplayer AI Mode" to "Multiplayer Overhaul" (2026-10-13)

Status: READ-ONLY inventory. No product file was edited, moved or renamed. The only file created is this record.
Scope owner: Matias. Rename execution: later, not part of this record.

## 1. Decision being inventoried

- Public display name changes: `Multiplayer AI Mode` (also `Multiplayer AI Mod`, `MULTIPLAYER AI`) becomes `Multiplayer Overhaul`.
- Internal id stays `coop_mode`. The dev folder `development\mods\coop_mode`, DLL `coop_mode.dll`, ini `coop_mode.ini`, catalog id, telemetry key and update pipeline keep the old id.
- The historical id `multiplayer_ai_mod` is not the target. See risk R6.

## 2. Method (verified facts vs inference)

Verified by search and file reads:
- Searched the terms `Multiplayer AI Mode`, `Multiplayer AI Mod`, `multiplayer_ai_mod`, `MULTIPLAYER AI`, `coop_mode`.
- Excluded `*.BEFORE_*`, `archive\`, `workspace_organization_audit\`, `_human_review\`, `F1 Challenge V10*` game folders, `node_modules`.
- Newest release folder by mtime: `development\lightloader\v2.0.0-hub-dev.1\release_2.3.24_catalog_warning_2026-10-13` (also `release_catalog_redflag_hold_2026-10-13` exists, dated the same day).
- Hit counts (non-BEFORE files): Lightloader newest release 5 files; `development\mods\coop_mode` 161 files (most under `releases\v0.9.x`, `docs\`, `dll\build\`); `publishing\` 2 files; `operations\` 21 files; `production\` 38 files (list only).
- Verified in code: Lightloader looks up catalog entries by `id` (`LightloaderManager.cs:1877-1886`, `CatalogEntry(catalog, id)`). Usage uploads are whitelisted by `mod` id (`LightloaderManager.cs:797`). The display name is only a label (`LightloaderManager.cs:1136` sets `mod.Name` from the table, used for display).
- Verified: no name-based comparison of mod names anywhere in the Lightloader source or the DLL (grep for `== "Multiplayer` returned none).
- Verified: `BugReport.cs:274` and `FullBundle.cs:199` write `====` lines from Windows event logs, not from the mod log. They do not parse the mod's log header.

Inference (not yet tested):
- Changing the display name is safe for update matching and telemetry, as long as ids and file names stay. This must be proven by the test list in section 5.

Validator result (run as cold start): `tools/workspace/check-workspace-structure.ps1` reports 2 errors that predate this task: unexpected top-level directories `.claude` and `plans`. Not caused by this record; not fixed here (read-only task).

## 3. Inventory table

Classes: **A** = public display string, change at rename. **B** = internal id/path/file name, keep as `coop_mode`. **C** = historical record, leave unchanged.

Paths are relative to `%USERPROFILE%\Desktop\F1`. `LL` = `development\lightloader\v2.0.0-hub-dev.1\release_2.3.24_catalog_warning_2026-10-13`. Note: this folder is a frozen release record (classified C for its own content); any rename must land in a new Lightloader release folder, not in this one.

| path:line | text | class | note |
|---|---|---|---|
| `governance\PRODUCT_STATUS.json:378` | `"publicName": "Multiplayer AI Mode"` | A | Machine-readable public name. Update under the status evidence rule. |
| `governance\PRODUCT_STATUS.json:394` | `Rename to multiplayer_ai_mod needs structural spec.` | A (decision) | Blocker wording contradicts the owner's new plan (keep id). Supersede explicitly. Do not act on it. |
| `governance\PRODUCT_STATUS.json:377,382,383` | `"id": "coop_mode"`, `developmentPath`, `productionPath` | B | Keep. |
| `governance\PRODUCT_STATUS.json:33,34,302,318,371,388,389...` | version history and evidence lines naming coop 0.9.x | C | Historical status. Leave. |
| `LL\source_freeze\LightloaderManager.cs:152` | `{ "coop_mode.dll", new[] { "coop_mode", "Multiplayer AI Mode", "0" } }` | A (name) / B (id, file) | Same tuple: id `coop_mode` and file name keep; the second item is the display label. Change only the label. |
| `LL\source_freeze\LightloaderManager.cs:797-800` | `"coop_mode", // 2.3.1: Multiplayer AI Mode 0.9.1 (coop_usage.h)` | B (id); comment C | Usage whitelist key. Keep id. Comment is history. |
| `LL\source_freeze\ManagerSchema.cs:149` | `{ "coop_mode.ini", "Multiplayer AI Mode" }` | A (label) / B (key) | Manager settings label. Key `coop_mode.ini` stays. |
| `LL\source_freeze\ManagerUi.cs:3250` | `Schema.FileNames.TryGetValue(Path.GetFileName(f), ...)` | B (logic) | Shows the label from row above. No edit needed here. |
| `LL\source_freeze\ManagerSchema.cs:161` | `{ "coop_mode", "Work in progress: race online ..." }` | B / A-neutral | Description has no old name. Keep unless the owner wants new card text. |
| `LL\source_freeze\SelfTest.cs:905,907,908` | JSON and checks with `"coop_mode"` | B | Test fixtures use the id. Keep. |
| `LL\source_freeze\SelfTest.cs:1302-1311` | comment `Multiplayer AI Mode 0.9.1 summary`; file `coop_mode-20261010T...json`; assertions on `"mod":"coop_mode"` | comment C; rest B | Keep the id and filename pattern. Comment is history. |
| `LL\shipping\catalog.json:1` | `"id": "coop_mode", "name": "Multiplayer AI Mode", "version": "0.9.6"` | A (name) / B (id) | Catalog card title. Changing it changes the catalog file hash; the release registry must be updated with it. Id keeps lookup working. |
| `LL\shipping\catalog.json:1` (other mods' `changes`/notes) | e.g. `Needs Multiplayer AI Mode 0.9.3 or newer`, `Every PC needs ... Multiplayer AI Mode ...` | A (current notes) / C (history entries) | Current notes must read the new name when the catalog is republished. Dated history entries stay. Roughly 11 occurrences total in this file; split per entry before editing. |
| `LL\shipping\catalog.before.json` | same content as before the hold | C | Before-state record. Leave. |
| `development\mods\coop_mode\dll\coop_mode.cpp:43` | `#define MOD_NAME "Multiplayer AI Mode"` | A | Used in log lines at `coop_mode.cpp:5965,5970,6076,6081,6091` (log text only). Changing it changes the DLL bytes and its SHA. |
| `development\mods\coop_mode\dll\coop_mode.cpp:42` | `#define MOD_ID "coop_mode"` | B | Keep. |
| `development\mods\coop_mode\dll\coop_mode.meta.json:1` | `"name":"Multiplayer AI Mode"` | A | Meta name for the mod. Keep `"id":"coop_mode"` on the same line. |
| `development\mods\coop_mode\dll\coop_mode.ini:1` | `; coop_mode.ini - Multiplayer AI Mode (CO-OP). Audited ...` | A (comment) | Header comment shown in the settings file. Ini filename stays. |
| `development\mods\coop_mode\dll\coop_mode_README.txt:1` | `Multiplayer AI Mode 0.9.4 (WORK IN PROGRESS)` | A | Also stale: says 0.9.4, meta says 0.9.6. Fix in the same edit. |
| `development\mods\coop_mode\dll\coop_mode_README.txt:11` | `install Multiplayer AI Mode from the Lightloader` | A | In-game instructions. |
| `development\mods\coop_mode\dll\coop_usage.h:1` | `per-session usage summary for Multiplayer AI Mode (0.9.1)` | A (comment) | Header comment only. |
| `development\mods\coop_mode\dll\coop_usage.h:25` | `#define COOPU_MOD_ID "coop_mode"` | B | Telemetry key. Keep. |
| `development\mods\coop_mode\dll\coop_dinput8_proxy.cpp:11,18,36,45` | `coop_mode.ini`, `coop_mode.dll` | B | Load path. Keep. |
| `development\mods\coop_mode\dll\f1c_mod.json:3,109,115-116` | `"id": "coop_mode"`, `build/coop_mode.dll`, `coop_mode.ini` | B | Keep. |
| `development\mods\coop_mode\dll\mod.json:3,7` | `"id": "coop_mode"`, `build/coop_mode.dll` | B | Keep. |
| `development\mods\coop_mode\dll\coop_mode_meta.rc:2`, `build.bat:2,15,20,23,32` | `coop_mode.meta.json`, `coop_mode.dll`, `coop_mode_build` | B | Build artifact names. Keep. |
| `development\mods\coop_mode\dll\coop_mf_bridge.h:6,12` | named block `coop_mode` | B | ABI with MF mod. Keep. |
| `development\mods\coop_mode\dll\coop_mode_README.txt:73,82` | `coop_mode_log.txt`, `coop_mode_log.old.txt` | B | Log names. Keep. |
| `development\mods\coop_mode\dll\coop_mode.cpp:285` and related `coop_mode_is_selected` | function name | B | Code symbol, not user-facing. Keep. |
| `development\mods\coop_mode\README.md:1,52` | heading `CO-OP mode (coop_mode)`; path to `release_coop_0.9.5_2026-10-11` | A (heading) / B (path) | Owned by another agent. Not edited here. Heading uses "CO-OP", so may need new name too. |
| `development\mods\coop_mode\docs\USER_GUIDE.md` | no matches | - | Nothing to change. |
| `development\mods\coop_mode\tests\test_usage.py:125,147` | `d["mod"] == "coop_mode"` | B | Telemetry test. Keep. |
| `development\mods\coop_mode\tests\test_usage.py:1,27` | docstring and FIXTURE `==== Multiplayer AI Mode 0.9.0 start` | C (fixture) / A (docstring) | Fixture is a sample log header. Leave as sample, or update docstring only. |
| `development\mods\coop_mode\releases\v0.9.0 ... v0.9.6\**` (about 100 files) | name in package READMEs, SHA sums, src copies | C | Shipped/source snapshots with their own hashes. Do not edit. |
| `development\mods\coop_mode\docs\*.md` (about 45 files) | investigation and handoff text | C | Historical records. Do not mass-edit (AGENTS.md). |
| `publishing\website\privacy\mod-downloads.html:35` | `Multiplayer AI Mode 0.9.1 (from Lightloader 2.3.1): ...` | A | Public privacy text. Needs privacy review and redeploy. Data description stays accurate under the new name. |
| `operations\README.md:136` | link label `Multiplayer AI Mode 0.9.5 crash reports (2026-10-11)` | A (label) | Change label only; link target is history. |
| `operations\release_management\changelogs\lightloader.json:142,149,164,171,173` (and `mechanical_failures.json:15,22`, `pitstop_failures.json`, `esc_pause_menu.json:6`) | release-note sentences | C | Shipped changelog history. Keep. New entries use new name. |
| `operations\release_management\changelogs\*.BEFORE_*` | before-states | C | Leave. |
| `operations\vps\COOP_095_CRASH_REPORTS_2026-10-11.md`, `VPS_COOP_SERVER_FITNESS_2026-10-11.md`, `BR_20261012_*_INVESTIGATION.md`, `analytics\*` (about 6 files) | incident and analysis text | C | Historical. Keep. |
| `operations\vps\telemetry_v10_2026-10-10\deployed_artifact\telemetry_service.py:37,56` | comments `Multiplayer AI Mode 0.9.0/0.9.1`; list `"coop_mode"` | comment C; list B | Deployed artifact. The `"coop_mode"` list is the telemetry whitelist. Keep. |
| `operations\vps\telemetry_v10_2026-10-10\deployed_artifact\mod-downloads.html:35` | same privacy text as publishing | C (deployed copy) | Live copy is `publishing\website\...`. Changing this needs a redeploy, so leave it. |
| `operations\vps\telemetry_v10_2026-10-10\README.md`, `deploy_v10.sh`, `test_v10_usage.py`, `sample_coop_mode_summary.json`; `telemetry_v9_2026-10-10\*` | id `coop_mode`, version notes | B / C | Keep ids. |
| `production\RELEASE_REGISTRY.md` (3 lines) | release history lines | C | Keep. |
| `production\mods\coop_mode\v0.9.0 ... v0.9.6\*` (about 24 files: README, install, ini, SHA sums) | shipped text | C | Shipped packages. Their hashes are kept. Do not edit. |
| `production\lightloader\v2.3.0\*`, `v2.3.1\*`, `v2.3.2\catalog.json` | catalog and install text | C | Shipped. Do not edit. |

Counts (table rows, counted by the first class letter in the class column):
- Class A: 15 rows. Several share a line with a B id that must be kept (for example `LightloaderManager.cs:152`, `catalog.json:1`).
- Class B: 16 rows.
- Class C: 13 rows, plus 2 mixed rows (comment C / list B) and 1 row with no class (`-`, no matches).
- File-level hit counts from search (not all rows listed): dev mod 161 files (mostly `releases\` and `docs\`, class C); `production\` 38 files (class C, list only); `operations\` 21 files (class C, with B id lists in telemetry); `publishing\` 2 files (1 A, 1 C); Lightloader newest release 5 files (catalog and source freeze, A/B).

## 4. Proposed change order (for the later rename, not executed now)

0. Preserve before-state of `governance\PRODUCT_STATUS.json` under `workspace_organization_audit\<phase>_BEFORE_<date>\` (AGENTS.md). Then supersede blocker line 394 with the owner's decision. Update `publicName` only.
1. Decide version and release: the rename changes the DLL bytes (`MOD_NAME`) and the catalog name, so it is a new coop release (for example 0.9.7) with new DLL hash, new zip hash and new catalog hash. A rename-only build is still a release. Do not reuse `release_2.3.24_catalog_warning_2026-10-13`.
2. Dev DLL: `dll\coop_mode.cpp:43`, `dll\coop_mode.meta.json:1`, `dll\coop_mode.ini:1` header comment, `dll\coop_mode_README.txt:1,11` (fix stale 0.9.4 at the same time), `dll\coop_usage.h:1` comment. Rebuild into `%TEMP%` (build.bat already does this).
3. Lightloader new release folder (not the frozen one): `LightloaderManager.cs:152` label, `ManagerSchema.cs:149` label, catalog `name` field, then regenerate `catalog.json` and its SHA. Update the release registry (`production\RELEASE_REGISTRY.md` gets a new line, old lines stay).
4. Mod README (`development\mods\coop_mode\README.md`) and `docs\USER_GUIDE.md` (no hits): owned by another agent; coordinate. Not in this change.
5. Public text after owner approval: `publishing\website\privacy\mod-downloads.html:35` (privacy review), `operations\README.md:136` label, release notes.
6. Deploy catalog and website only with the owner's approval (VPS deploy is owner-run by default per workspace memory).
7. Leave all class C items untouched. No mass search-and-replace.

## 5. Risks and checks

- **R1 Update matching.** Catalog lookup and opt-in use the id (`LightloaderManager.cs:1877-1886`, `ManagerForm.OptIn(CatalogEntry(..., "coop_mode"))` in `SelfTest.cs:907`). Display name is a label only. Safe if the id stays `coop_mode`. Check: run Lightloader self-test (existing `SelfTest.cs` checks on `coop_mode`) after the rename.
- **R2 Ids and file names must not change.** Renaming `coop_mode.dll`, `coop_mode.ini`, the id, the `coop_mode-*.json` usage filename pattern (`UsageNameRx`, `LightloaderManager.cs:799`), or the log file names breaks loading, opt-in and telemetry. The `multiplayer_ai_mod` idea in governance line 394 would do this. Do not do it.
- **R3 Telemetry.** Usage JSON carries `"mod":"coop_mode"`, whitelisted by `UsageMods` and by `telemetry_service.py` (lines 37, 56 list). A display-name change does not change these. Check: `development\mods\coop_mode\tests\test_usage.py` (asserts `mod == coop_mode`) and `test_address_inventory.py`.
- **R4 Hash coupling.** Changing `MOD_NAME` changes DLL bytes; the catalog `sha256` and package SHA must be regenerated together. A mismatch makes the install fail. Old and new catalogs would show different names during the transition. This is cosmetic, not functional.
- **R5 Privacy statement.** `publishing\website\privacy\mod-downloads.html:35` describes what data the mod sends. Renaming the mod does not change the data. Still needs owner/privacy review before redeploy.
- **R6 Governance conflict.** `PRODUCT_STATUS.json:394` says rename the id to `multiplayer_ai_mod`. The owner's current plan keeps `coop_mode`. Treat line 394 as superseded only after the owner confirms in writing.
- **R7 Log text.** `MOD_NAME` appears in the mod's own log header lines (`coop_mode.cpp:5965`). Any reader of `coop_mode_log.txt` that matches the header would break. Searched `operations\` and the Lightloader source: no parser of this header found (`BugReport.cs`/`FullBundle.cs` are Windows event logs). Still verify with a log replay after the change.
- **R8 Stale text.** `dll\coop_mode_README.txt` says 0.9.4 while meta says 0.9.6. Fix in the same edit, not as a separate change.
- **R9 Mixed names in the wild.** Installed copies of 0.9.x will keep "Multiplayer AI Mode" in their own README and logs. Historical records stay C.
- **R10 Other mods' notes.** Several other mods' current notes say `Needs Multiplayer AI Mode 0.9.x`. These must read the new name in the same catalog publication, or players see the old name next to the new card.

## 6. Open items for the owner

- Confirm the display name exactly: `Multiplayer Overhaul` (with or without "CO-OP").
- Confirm the id stays `coop_mode` (this record assumes yes).
- Confirm whether the rename ships as its own release or with the next functional change.
- Decide whether `publishing\website\privacy\mod-downloads.html` is updated now or with the release.
- The validator errors (`.claude`, `plans` top-level dirs) are pre-existing and need an owner decision; not part of this rename.

## 7. Restoration and state

- Changed files: this file only (`development\mods\coop_mode\docs\RENAME_INVENTORY_MULTIPLAYER_OVERHAUL_2026-10-13.md`).
- No product, governance, catalog, release or publishing file was edited, moved or renamed.
- Restoration: delete this file. No other state changed.
- Pending validation: the checks in section 5 have not been run; they are recommendations for the rename task.

## 8. Correction (2026-10-13, after a late background search)

The first pass under-scoped the search. It covered only the frozen release folder and the `production\`, `operations\` and `publishing\` paths, and it missed the live development Lightloader source. This section adds the missed items. Sections 3 to 5 still hold for the items listed there, except where this section says otherwise.

**Correction to section 4, step 3.** The live source is `development\lightloader\v2.0.0-hub-dev.1\manager_cs\`, not the frozen `release_2.3.24_catalog_warning_2026-10-13\source_freeze\`. The frozen folder is a release record (class C). Rename work goes into `manager_cs\` and into a new release folder.

| path:line | text | class | note |
|---|---|---|---|
| `development\lightloader\v2.0.0-hub-dev.1\manager_cs\ManagerSchema.cs:87` | `Bool(Coop, "Mod", "Enabled", "Multiplayer AI Mode", "Off = the mod does nothing.")` | A | Settings UI label for the coop switch. Missed in section 3. Visible to players in the manager. Keys `coop_mode.ini` and `Mod/Enabled` stay B. |
| `development\lightloader\v2.0.0-hub-dev.1\manager_cs\ManagerSchema.cs:196` | `{ "coop_mode.ini", "Multiplayer AI Mode" }` | A (label) / B (key) | Manager file label, as in the frozen copy at line 149. |
| `development\lightloader\v2.0.0-hub-dev.1\manager_cs\ManagerSchema.cs:86` | comment `Multiplayer AI Mode 0.9.5: [Net] ...` | C (comment) | Code comment, history. Optional to update. |
| `development\lightloader\v2.0.0-hub-dev.1\manager_cs\LightloaderManager.cs:144` | `{ "coop_mode.dll", new[] { "coop_mode", "Multiplayer AI Mode", "0" } }` | A (label) / B (id, file) | Same tuple as the frozen copy at line 152. Change only the label. |
| `development\lightloader\v2.0.0-hub-dev.1\manager_cs\LightloaderManager.cs:792` | `"coop_mode", // 2.3.1: Multiplayer AI Mode 0.9.1` | B (id); comment C | Usage whitelist key. |
| `development\lightloader\v2.0.0-hub-dev.1\manager_cs\SelfTest.cs:684,1173` | comments naming the mod | C (comment) | Ids in the same block are B. |
| `development\lightloader\v2.0.0-hub-dev.1\manager_cs\SelfTest.cs:686-740,1174-1182` | `coop_mode.ini`, `"coop_mode"` ids | B | Keep. |
| `development\lightloader\v2.0.0-hub-dev.1\ops\telemetry_v13_20261011\telemetry_service.py:37,56` and `ops\telemetry_v14_20261012\telemetry_service.py:37,56` | comments naming the mod; list `"coop_mode"` | comment C; list B | v14 is the newest telemetry service. The `"coop_mode"` list is the whitelist. Keep the list. |
| `development\lightloader\README.md:25,27` | `first user Multiplayer AI Mode 0.9.0` | C | Lightloader history line. Keep. |
| `development\lightloader\v2.0.0-hub-dev.1\REPLAY_NAMING_SPEC_2026-10-13.md:19,132` | `owned by coop_mode` | B | Id reference in a new spec. Keep. |
| `development\mods\README.md:15` | `owner name multiplayer_ai_mod; rename pending a structural-move spec` | A (decision text) | Same stale rename idea as PRODUCT_STATUS line 394. Supersede with the owner's decision. Do not act on it. |
| `development\mods\HANDOFF_USAGE_V2_2026-10-11.md:37` | `Shipped 2026-10-11: ... Multiplayer AI Mode 0.9.4` | C | Handoff history. Keep. |
| `development\mods\camera_editor\releases\1.0.6\deploy\public\catalog.json` (9 occurrences) | catalog change-note text | C (history) / A (any current note) | Shipped catalog notes. Not in the owner's named paths. Keep. |

**Other gaps.** The background search output lists about 305 files. Most are under `development\lightloader\...\release_*` and `development\mods\coop_mode\releases\` or `dll\build\`. These are historical (class C) and were not reviewed line by line. The `development\lightloader\v2.0.0-hub-dev.1\release_*\public\catalog.json` files and `catalog.before.json` files are historical catalog snapshots (class C). The newest is `release_2.3.24`, which is in section 3.

**Updated counts.** Adding the rows above, class A grows by 2 (`ManagerSchema.cs:87` and `:196` from manager_cs; `LightloaderManager.cs:144` is a label row like its frozen twin), and the `development\mods\README.md:15` decision row is also A. Class B and class C grow by 6 and 7 rows respectively. The exact counts in section 3 are not updated; treat the table above as the authoritative addition.

**Pending validation** (unchanged): no test in section 5 was run.
