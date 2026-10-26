# Driver AI Toolkit game plug-in: embedded metadata so Lightloader shows the version (2026-10-14, Claude)

 until the owner approves shipping (see "Ship steps"). Nothing was published, uploaded, deployed,
installed into the game folder, or sent to the VPS.

## 0. Incident during staging (2026-10-14) and repair

- Cause: a `tar` extraction in the staging step ran with the workspace root as working directory. It wrote the
  AI Brain Lab tree into `F1\`: 20 top-level items (`app`, `dll`, `presets_builtin`, `run.bat`, `DriverAIToolkit.bat`,
  `MANUAL.md`, handoff and backup `.md` files), 30 files into `tools\`, 14 into `trials\`, and overwrote the root
  `README.md` with the AI Brain Lab README. The validator then failed with 5 unexpected-top-level errors.
- Scope of damage, verified: every stray item was byte-identical to its `development\mods\ai_brain_lab` source before
  any move (dry run, 0 problems). Original root `tools\` entries (README.md, drm, integrations, reverse_engineering,
  workspace) and `trials\ilspy-r111` were untouched.
- Repair (same volume, nothing deleted): the 64 stray items were moved to
  `_human_review\20261014_abl_meta_tar_incident\preserved\` with `PRE_MOVE_MANIFEST.json` (hashes) and
  `RESTORATION_MAP.md`. Each move was verified by tree hash. The root listing now matches the original set.
- Root `README.md`: the original 2026-09-30 file (11,620 B, with the Q21 retention section) was overwritten and
  no complete copy exists (a workspace-wide text search found none). It was restored from the verified pre-Q21
  before-copy `workspace_organization_audit\RETENTION_BEFORE_2026-09-30\README.md` (SHA-256 `7f5ce891...`).
  **OWNER ACTION:** re-apply the 2026-09-30 Q21 README change, or accept the pre-Q21 version. `AGENTS.md` (Q21) is
  still authoritative. Root directory mtimes for `tools` and `trials` changed; their contents are as described above.
- Lesson recorded for the next agent: stage builds with explicit absolute paths and check `pwd` before any archive
  extraction. Never extract an archive from a shell whose working directory is the workspace root.

## 1. Problem (owner report 2026-10-14)

Lightloader > Mods lists "Driver AI Toolkit (game plug-in)" (`plugins\lightloader\ai_brain_lab.dll`) with no version
number. Camera Editor (game plug-in) shows v1.0.6.

## 2. Findings

| # | Tier | Finding | Evidence |
|---|---|---|---|
| F1 | Verified (code read) | The manager reads a mod's metadata from the DLL resource `F1C_MOD_META`, type RCDATA (10), as JSON. It maps the DLL as a data file only, so DllMain never runs. | `development/lightloader/v2.0.0-hub-dev.1/manager_cs/LightloaderManager.cs` `ReadEmbeddedMeta` (about lines 1058-1080) |
| F1b | Verified (code read) | For `source == "embedded"` the JSON must have `"schemaVersion": 1` (integer), an `id` matching `^[a-z0-9_]{2,60}$`, a `name`, and a `version` matching the version regex. Otherwise the sidecar `<dll>.mod.json` is tried. | `ApplyMeta` in the same file (about lines 1110-1137); `InstalledMods` (about 1144-1170) |
| F2 | Verified (code read) | `KnownMods` supplies only id and name for `ai_brain_lab.dll` (`driver_ai_toolkit`, `Driver AI Toolkit (game plug-in)`). It sets no version. | `LightloaderManager.cs` line 143 |
| F3 | Verified (file read) | The installed/shipped DLL `6C60B666...` (the 1.0.4 and 1.0.5 plug-in) has no `F1C_MOD_META` resource and no sidecar, so the Mods list has no version. | Offline read check, section 6 (`read_meta_check.py` reports `NONE` for the workspace DLL) |
| F4 | Verified (file read) | Camera Editor embeds its metadata with `camera_editor.rc` (`F1C_MOD_META RCDATA "camera_editor.meta.json"`), compiled with `windres` into `camera_editor.res.o` and linked into the DLL. | `development/mods/camera_editor/plugin/camera_editor.rc`, `build.bat` |
| F5 | Verified (standard) | Mod standard section 5: `<id>.meta.json` embedded as `F1C_MOD_META`; semver; "any shipped byte change = a new version; never reuse a version label for different bytes." | `development/mods/MOD_STANDARD.md` section 5 |
| F6 | Verified (live catalog, read-only GET 2026-10-14) | The current public toolkit is **1.0.5** (package `driver_ai_toolkit-1.0.5.zip`, SHA-256 `9835C60A...`, 635,502 B). Live Lightloader catalog SHA-256 `9B809657...`. | Live GET of `https://mods.palloset.com/lightloader/api/catalog.json` (no write); same bytes as `release_2.3.25_2026-10-14/catalog_remove_coop_2026-10-14/catalog.json` |
| F7 | Verified (production record) | Release history: 1.0.0 (2026-10-03), 1.0.1, 1.0.2 (2026-10-04, production copy `production/mods/driver_ai_toolkit/v1.0.2`), 1.0.3, 1.0.4, 1.0.5 (2026-10-08, plug-in unchanged `6C60B666`). Production copy for 1.0.3 onward was never made (open item in the 1.0.5 manifest). | `development/lightloader/v2.0.0-hub-dev.1/release_toolkit_1.0.5_2026-10-08/MANIFEST.md`, `production/RELEASE_REGISTRY.md` |
| F8 | Inference | Adding the resource changes the plug-in bytes, so under F5 the toolkit must ship as a new version. The version embedded in the plug-in must match the package version. Choice: **1.0.6**, not 1.0.5. | Follows from F5 and F6 |

Root cause (verified by F1-F4, not yet observed live): the installed plug-in has no metadata, so the manager has no
version to show. Adding the same metadata block as Camera Editor fixes this.

Assumption to validate live: the owner's Lightloader 2.3.25 Mods list shows the version after the new DLL is
installed (section 8, pending).

## 3. Decision: version 1.0.6

- The plug-in code is unchanged. Only the resource is added, so the bytes change (`6C60B666` -> `87CE3E76`).
- By MOD_STANDARD section 5 and the toolkit's own catalog rules, `1.0.5` cannot carry different bytes. So the
  plug-in and the next package are **1.0.6**.
- The embedded `version` is `1.0.6`, the same as the package `VERSION`, so the Mods list and the toolkit agree.

## 4. Changes (workspace)

| File | Change | Before-state |
|---|---|---|
| `development\mods\ai_brain_lab\dll\ai_brain_lab.meta.json` | NEW: id `driver_ai_toolkit`, name `Driver AI Toolkit (game plug-in)`, version `1.0.6`, author `Matias`, description, homepage `https://mods.palloset.com/driver_ai_toolkit`, lightloader `>=2.2.0`, schemaVersion 1 (393 B, UTF-8, no BOM, LF) | none (new file) |
| `development\mods\ai_brain_lab\dll\ai_brain_lab.rc` | NEW: `F1C_MOD_META RCDATA "ai_brain_lab.meta.json"` | none (new file) |
| `development\mods\ai_brain_lab\dll\build.bat` | Adds one REM line, a `WINDRES` variable, a `windres` step (`ai_brain_lab.res.o`) with its errorlevel check, and adds `ai_brain_lab.res.o` to the DLL link line. Nothing else changed. | `dll\build.bat.BEFORE_20261014_dattver` (1691 B, SHA-256 `E1F55476...`) |
| `development\mods\ai_brain_lab\README.md` | One dated status paragraph added above the 2026-10-08 entries | `README.md.BEFORE_20261014_dattver` (SHA-256 `E69A06C6...`) |
| `development\mods\ai_brain_lab\dll\ai_brain_lab.c` | NOT changed | - |
| `development\mods\ai_brain_lab\dll\ai_brain_lab.dll` (in-tree dev DLL) | NOT changed: still the 1.0.5 build `6C60B666...`, so the source tree does not yet contain the 1.0.6 DLL (see section 9) | - |
| `development\lightloader\v2.0.0-hub-dev.1\release_toolkit_1.0.6_2026-10-14\` | NEW release-prep folder: `MANIFEST.md`, `public\catalog.json`, `public\driver_ai_toolkit-README.txt`, `public\SHA256SUMS.lower`, `remote\deploy_tk106.sh` (not run) | none (new) |

No game folder was written. No file was deleted. The live catalog was read, not changed. Workspace root: see section 0
(incident and repair; the root README was restored from a pre-Q21 copy and needs owner action).

## 5. Build and tests (all in %TEMP%, game not running)

Staging: `%TEMP%\abl1006_stage\F1\...` (copies of `ai_brain_lab` without `releases`, `mechanical_failures` headers,
SDK headers `f1c_race_control.h`, `f1c_broker_abi.h`, the viewer folder `ai_mind_monitor`, and the findings and
investigation files the packager reads). The existing script `dll\build.bat` was run unchanged, with its own paths,
in the staging copy (`%USERPROFILE%\AppData\Local\Temp\abl1006_stage\...\dll\build.bat`). The build itself wrote
only into `%TEMP%`. The staging step's tar extraction did write into the workspace root; see section 0 for the
verified repair.

| Step | Result |
|---|---|
| `gen_rules.py` (checks the rule table against the installed game EXE, read-only) | PASS: 23 rules, 28 sites; `abl_rules.h` identical to the workspace copy |
| `gen_levers.py` | PASS: 29 catalog entries, 15 engine rows; `abl_levers.h` identical to the workspace copy |
| `test_abl_shm.py` | PASS: 0 failures |
| `abl_logic_test.exe` | PASS: 130 checks, 0 failed |
| `abl_precedence_test.exe` (COMP-02) | PASS |
| `ai_brain_lab.dll` link (with `ai_brain_lab.res.o`) | Built, exit 0 |
| `tools\test_abl_ui.py` (after staging `ai_mind_monitor`) | PASS: 0 failures |

Staged DLL: `%TEMP%\abl1006_stage\...\dll\ai_brain_lab.dll`, 260,693 B, SHA-256 `87CE3E76B54804637FA736FC67BEB118D63C4FC2830267800A63C5FA0EA75096`.
Previous DLL (workspace, unchanged): 259,651 B, SHA-256 `6C60B6668CC820A9FC1464B17C13F56CE6B4C8BC454BC6112279C59EF08D71A8`.

Not run: the Lightloader manager's own C# `SelfTest.cs` (it needs a built manager and the `meta_smoke.dll` fixture).
Not run: in-game, live race, live install, or the Lightloader Tools install path.

## 6. Metadata read check (manager logic reproduction)

Script: `%TEMP%\abl1006_check\read_meta_check.py` (run with `python -I`). It reproduces `ReadEmbeddedMeta`
(LoadLibraryEx with AS_DATAFILE|AS_IMAGE_RESOURCE, FindResource `F1C_MOD_META` type 10, size and lock) and the
`ApplyMeta` embedded checks.

| DLL | Result |
|---|---|
| Staged 1.0.6 `ai_brain_lab.dll` | accepted: id `driver_ai_toolkit`, name `Driver AI Toolkit (game plug-in)`, version `1.0.6`, author `Matias` |
| Workspace `ai_brain_lab.dll` (1.0.5 bytes) | NONE: no `F1C_MOD_META` resource, so no version (the reported problem) |
| `development\mods\camera_editor\plugin\camera_editor.dll` (reference) | accepted: id `camera_editor`, version `1.0.6` |

The reproduction is not the manager binary itself. Its exact result still needs the owner's live Mods list check.

## 7. Release candidate (package and catalog, prepared, NOT published)

| Item | Value |
|---|---|
| Package folder | `%TEMP%\abl1006_stage\...\releases\driver_ai_toolkit\1.0.6\` (built by the existing `tools\build_toolkit.py 1.0.6`, run in the staging copy because that script writes into its own tree) |
| Package zip | `%TEMP%\abl1006_stage\...\releases\driver_ai_toolkit\1.0.6\driver_ai_toolkit-1.0.6.zip`, 804,355 B, SHA-256 `639D08123DCEE5F065A6FD5EFF4FD6C9A011FC88E14DB862541412E972016ADC` |
| Package VERSION | `1.0.6` (SHA-256 `3DD81E3D...`) |
| Package README.txt | SHA-256 `5E8463F3791D6694C5E388C4506DBC917BE01512A64AD63F0F37151D772F41DB` (its "Last updated" stamp is the system date, 2026-10-13) |
| Plug-in in package | `dll\ai_brain_lab.dll` SHA-256 `87CE3E76...` |
| Candidate catalog | `release_toolkit_1.0.6_2026-10-14\public\catalog.json`, SHA-256 `661E6A22221D7F99C5604222C8367D81CF7E5F7B4D5A6B2163D70F4AE75C579AE`. Built from the LIVE catalog `9B809657...` (not the stale 10-08 copy). Only `tools[driver_ai_toolkit]` changes: version 1.0.6, package url `/downloads/driver_ai_toolkit/v1.0.6/driver_ai_toolkit-1.0.6.zip`, sha256 and size, and a 1.0.6 changelog entry prepended. Everything else is byte-compatible with the live catalog (checked: top keys, `mods`, other tools all equal). |
| Checksums | `public\SHA256SUMS.lower` (zip, README, catalog) |
| Deploy script | `remote\deploy_tk106.sh`, copied from `deploy_tk105.sh` with version, backup/staging names and the LIVE_CAT guard (`9B809657...`) changed. `bash -n` passes. NOT run. |

Package contents check: every listed package file except `dll\ai_brain_lab.dll`, `VERSION`, `README.txt` and
`data\findings.jsonl` matches the current workspace source byte for byte (app, tools, viewer, MANUAL, bat, investigation
files). The `presets_builtin` files were copied by the script and were not separately compared here; their hashes are the
same as in the 1.0.5-r3 manifest.

Decision for the owner: **`data\findings.jsonl`** in the package is the current workspace file
(`09B7C68E...`, grown since 1.0.5, which shipped `1C291DA6...`). The pipeline copies it on every build, so 1.0.6
ships the newer findings. Either accept that, or the packager needs an explicit freeze first.

Changelog draft for `operations\release_management\changelogs\driver_ai_toolkit.json` (not edited):

```
{ "version": "1.0.6", "date": "2026-10-14", "changes": [
  "New: the game plug-in now carries its name and version, so Lightloader's Mods list shows \"Driver AI Toolkit (game plug-in)\" with its version number.",
  "No change to how the toolkit drives the AI. Needs Lightloader 2.2.0 or newer." ] }
```

## 8. Ship steps (nothing run; owner-run per the VPS deploy rule)

1. Owner decides: ship 1.0.6 now or after the live check, and whether the newer `findings.jsonl` is included (section 7).
2. Owner approves shipping in chat. Per project memory, the VPS deploy is owner-run unless the owner grants permission
   for that session.
3. Copy the exact candidate zip from TEMP to `release_toolkit_1.0.6_2026-10-14\public\driver_ai_toolkit-1.0.6.zip`.
   Do not rebuild the zip; the zip is not reproducible from a different tree (file timestamps). Check
   `sha256sum -c public\SHA256SUMS.lower` passes (all three files).
4. Make the durable build record: copy the 1.0.6 folder into `development\mods\ai_brain_lab\releases\driver_ai_toolkit\1.0.6\`
   with the TEMP-built plug-in (same bytes as the workspace copy only after the owner promotes the plug-in; see section 9).
5. Re-check the live catalog hash just before deploy. If it is no longer `9B809657...`, rebuild the candidate catalog from the new
   live file with the same rule (only `tools[driver_ai_toolkit]` changes) and redo the checksums.
6. Owner or Claude (with permission) copies `public\*` and `remote\deploy_tk106.sh` to the VPS and runs
   `deploy_tk106.sh` as f1admin. The script backs up catalog and links, installs `v1.0.6/`, repoints the two short
   links, checks each over HTTPS, and writes the catalog last. It restores on failure. Nothing is deleted.
7. External check from the PC: served catalog SHA-256; `/driver_ai_toolkit` download SHA-256 `639D0812...`; `VERSION` = 1.0.6.
8. Add the changelog entry (section 7) to the operations changelog and update the RELEASE_REGISTRY row (owner-approved).
9. Owner in-game check: Lightloader Tools > Driver AI Toolkit > Install/Update, then Mods list shows the name and `1.0.6`.

## 9. Open items and follow-ups

- The in-tree `dll\ai_brain_lab.dll` is still the 1.0.5 build. The 1.0.6 build is only in `%TEMP%`. Promote it (copy the exact
  staged DLL, hash `87CE3E76...`) only with the owner's approval.
- `dll\build.bat` still writes `ai_brain_lab.res.o`, `ai_brain_lab.dll` and the test `.exe` files into the source tree when
  run in place. AGENTS.md says builds happen only in `%TEMP%`. Follow-up: run it from a staging copy, or change the script
  to write to `%TEMP%`.
- Author: MOD_STANDARD says `Matias`. Camera Editor's embedded metadata says `Matias`. This block uses `Matias` to match
  Camera Editor. Owner to confirm.
- `governance\PRODUCT_STATUS.json` (driver-ai-toolkit) still names `production\mods\driver_ai_toolkit\v1.0.2` and says
  "Production copy not done yet" for 1.0.3-1.0.5. Not changed here (no stage change). Follow-up for the owner.
- The package README "Last updated" comes from the system date (2026-10-13) and not 2026-10-14. Cosmetic.
- `build_toolkit.py` writes into `releases\` inside the development tree. This record did not run it there.

## 10. Restoration

- `dll\build.bat`: copy `dll\build.bat.BEFORE_20261014_dattver` back over it.
- `README.md`: copy `README.md.BEFORE_20261014_dattver` back over it.
- New files (`dll\ai_brain_lab.meta.json`, `dll\ai_brain_lab.rc`, this record, the release-prep folder): if reverting, move them
  to a `_human_review\` location; do not hard-delete.
- The workspace DLL, the live catalog and the VPS were never changed, so no restoration is needed there.

## 11. Pending validation (not claimed)

- Live Mods list shows `Driver AI Toolkit (game plug-in)` with a version after install: pending owner check.
- Real manager self-test with the new DLL: not run.
- The 1.0.6 package install on a real PC via the Tools page: pending.
- Gameplay of the plug-in itself: unchanged code, not re-tested in game.
