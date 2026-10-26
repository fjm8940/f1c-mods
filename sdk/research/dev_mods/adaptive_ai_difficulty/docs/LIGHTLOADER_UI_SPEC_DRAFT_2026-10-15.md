# Adaptive AI Difficulty - Lightloader settings panel (DRAFT spec)

Date: 2026-10-15. Author: Matias. Status: DRAFT, read-only investigation. No code, no build, no file outside this document was changed.
Mod: `adaptive_ai_difficulty` v0.1.0-dev (). Dev DLL and ini: `development\mods\adaptive_ai_difficulty\dll\`.

Tags: **FACT** = read in code or file (cited). **PROPOSAL** = not yet built, for owner decision.

Cold-start note: the workspace validator (`tools\workspace\check-workspace-structure.ps1`) was NOT run, because this task was limited to reading files and writing this one document. Its result is pending. The governance and README files were read.

---

## 0. Which manager source is newest

- **FACT** Installed manager is 2.3.25. The newest manager source is `development\lightloader\v2.0.0-hub-dev.1\release_2.3.25_prep_2026-10-13\source_freeze\`. It already contains the 2.3.26 edits: `LightloaderManager.cs:62` `ManagerVersion = "2.3.26"`.
- **FACT** `release_2.3.26_2026-10-14\` has no source. It holds only the record, build, shipping, changelog and verification folders (RELEASE_RECORD.md section 3).
- **FACT** 2.3.26 changes only tool-plug-in visibility in the Mods tab: `ManagerUi.cs:2051` (`Rows()`), `Tools.cs` (`IsToolPlugin`), `PluginOrigins.cs`. The settings code is not changed (RELEASE_RECORD.md:21-24).
- **FACT** Owner live test of 2.3.26 is listed as a pending step (RELEASE_RECORD.md:103). Production copy was written 2026-10-14 (RELEASE_RECORD.md:106).
- Old `v0.x` / `v1.x` folders under `development\lightloader\` were not used.

## 1. How a mod gets settings today (Q1)

- **FACT** Settings file = `<dll basename>.ini` by default, in `plugins\lightloader\`. `ManagerUi.cs:1375-1385` (`ModSettingFile`) uses `mod.Settings` when set, otherwise `<dll>.ini`. It looks in `plugins\lightloader\` first, then the game root.
- **FACT** Optional meta key `"settings"` names another `.ini` (basename only, must end `.ini`): `LightloaderManager.cs:1089`.
- **FACT** Mod metadata comes from the DLL's embedded resource (`LightloaderManager.cs:1040` `ReadEmbeddedMeta`, schemaVersion 1) or a sidecar `<dll>.mod.json` (`LightloaderManager.cs:1144-1147`, max 64 KB). Fields read: id, name, version, author, description, homepage, settings, readme, paid, requires (`LightloaderManager.cs:1067-1100`).
- **FACT** The adaptive dev meta (`dll\adaptive_ai_difficulty.meta.json`) has id, name, version, author, description, lightloader. It has NO `settings` and NO `readme` key. The default `.ini` name still applies.
- **FACT** Ini read/write: `Core.IniFields` (`LightloaderManager.cs:1654`) returns key, section, value, line and the comment block above the key as help text. `Core.ApplyIniFields` (`LightloaderManager.cs:1681`) rewrites only changed lines and keeps comments. `SaveSettings` (`ManagerUi.cs:3411-3440`) refuses while the game runs (`ManagerUi.cs:3416`), refuses if the file changed on disk (`ManagerUi.cs:3426-3427`), makes a backup (`Core.Backup`) and writes atomically (`Core.AtomicWrite`).
- **FACT** Existing per-mod settings pages are hand-coded in `ManagerSchema.cs:51-134` (`Schema.All`): Pause menu (esc_pause_menu.ini), Pit Stop Failures, Mechanical Failures, Red Flag, Safety Car, Race Data Collector, Multiplayer AI Mode (coop_mode.ini), Field Telemetry. Web Telemetry has no entries in `Schema.All`, so its ini (if any) uses the generic view.
- **FACT** Mod settings page = `ModSettingsPage()` (`ManagerUi.cs:1420`), with a file picker (`FillSettingsPicker`, `ManagerUi.cs:3257`), a "Show advanced settings" toggle, and a gear button on each installed mod card (`ManagerUi.cs:2427-2431`).

## 2. Mod list rows, WIP flag, readme link (Q2)

- **FACT** Rows come from the catalog (`ManagerUi.cs:2043-2060`) plus installed DLLs not in the catalog. Installed first, then free before paid, then by name (`ManagerUi.cs:2066`). A mod not in the catalog shows "Not in the Lightloader catalog." (`ManagerUi.cs:2063`) unless `Schema.Descriptions` has a line for it (`ManagerSchema.cs:153-164`).
- **FACT** WIP wording is text only: `ManagerSchema.cs:161-162` ("Work in progress: ..." for coop_mode and multiplayer_overhaul). The catalog flag `"optIn": true` is read by `OptIn` (`ManagerUi.cs:2572`) and only excludes the entry from `FreeIds()` (`ManagerUi.cs:2565-2567`). It does NOT hide the card.
- **FACT** Enable switch on each card = `Core.SetEnabled` (`ManagerUi.cs:2462`, `LightloaderManager.cs:1225`). It writes `plugins\lightloader\LightloaderDisabled.ini`, not the mod's own ini. The mod's own `Enabled=` is a second, separate switch.
- **FACT** Readme link: meta `"readme"` must be a plain `*_README.txt` or `*_README.md` beside the DLL (`LightloaderManager.cs:1090`, `IsReadmeName` at `LightloaderManager.cs:1104`). The Readme link appears on the card (`ManagerUi.cs:2497-2506`).

## 3. Can a mod ship a schema the manager renders generically? (Q3)

- **FACT** No. `SettingDef` (`ManagerSchema.cs:13-19`) has only File, Section, Key, Label, Help, DefaultValue, Kind, Min, Max, Step, Decimals, Options, Advanced. There is no display transform, no unit, no "show when", no per-mod group. Schema entries are C# literals.
- **FACT** Generic fallback exists: with no schema entries (`schema.Count == 0`) every ini key is shown (`ManagerUi.cs:3330-3340`). Labels are humanized keys, help is the comment text, and all values are text boxes (`ManagerUi.cs:3355-3400`). A value is shown as a switch only if it is `0`/`1` AND the comment contains the digit "1" (`ManagerUi.cs:3370`). Fragile.
- **FACT** So the current manager cannot draw: a slider, a "AI +x %" label, a 3-way Mode dropdown with labels, a per-track table, or a Reset button. The generic view gives a plain text box for `AllAIFactor`.
- **FACT** The rating file would not be listed as a setting: `SettingsFiles` (`LightloaderManager.cs:1780-1793`) lists only `*.ini` in `plugins\lightloader\`.
- **FACT** The DLL reads its own ini: `Enabled` once at load (`adaptive_ai_difficulty.c:581`, `ada_load_ini`), factors every 2 s by mtime (`adaptive_ai_difficulty.c:43`, `:654-660`). Factors clamp to 0.80..1.20 (`adaptive_ai_difficulty.c:379`, `:403`). Writes only in offline sessions (`dll\README.md`, session guard).

### Recommendation (generic vs hand-coded)

- **PROPOSAL** Hand-code it. Add an `Adaptive AI` block in `ManagerSchema.cs` for the ini keys (On/Off, Mode, Fixed strength, target offset, nudges), and a dedicated panel class in `ManagerUi.cs` for the rating view and Reset. Reason: the rating view and Reset are not ini-backed, a generic schema would need a new JSON-driven page type, and only one mod needs this now.
- **PROPOSAL** Defer a generic `settings_schema` in meta. It would touch every mod's packaging and the release gates for little gain. Revisit when a second mod asks for a panel.
- **PROPOSAL** Small generic extension still needed for the percent label: add `Scale` and `Offset` (or a `Format` string) to `SettingDef` so `AllAIFactor 0.80..1.20` displays as "AI -20 % .. +20 %" and saves back as `0.800..1.200`. Only affects new entries; existing entries keep defaults.

## 4. Panel contents (PROPOSAL)

Page title: "Adaptive AI" (under Plugins > Mod settings). Tool-style card on Mods list is out of scope.

| # | Label (player-facing) | Control | Ini / file | Live or next start |
|---|---|---|---|---|
| 1 | Adaptive AI on / off | switch | `[Adaptive] Enabled=0/1` | Next game start (DLL reads once, `adaptive_ai_difficulty.c:581`) |
| 2 | Opponent strength mode | dropdown: Adaptive / Fixed / Championship | new key `Mode=` (adaptive, fixed, championship) | Live (poll) if DLL supports it |
| 3 | Fixed strength | slider "AI +x %", range -20 to +20 %, step 1 % | `AllAIFactor=` (0.800..1.200) | Live (poll every 2 s) |
| 4 | Target gap to you (Adaptive) | number "AI ahead / behind by N s", e.g. -1.0 .. +1.0 s, step 0.1 | new key `TargetOffsetSec=` | Live (poll) |
| 5 | In-race nudges | switch, with help "small mid-race speed changes so the gap stays near the target" | new key `Nudges=0/1` | Next game start (proposed) |
| 6 | Rating per track | read-only table: track, your clean-lap median, AI clean-lap median, gap, laps used, last update | `plugins\lightloader\adaptive_ai_difficulty_ratings.json` (new, DLL writes) | Read at panel open |
| 7 | Reset rating for this track | button (per row) and "Reset all" | same JSON | Refuses while game runs (`Core.GameRunning`, pattern from `ManagerUi.cs:3416`) |

Label rules: no "AI ai_factor" jargon; show % and seconds; the help text under each control names the effect in one line ("Adaptive: AI pace follows your clean laps, aiming at the gap above"). Keep F1 terms (clean lap, gap, pace) where a race engineer would use them.

Save behaviour: same as other settings pages: "Saved. Restart the game to apply." for item 1 and 5; "Applies within a few seconds" for 2-4 (only if the DLL side is live-capable; see section 6).

## 5. Mod-side contract needed (PROPOSAL, not built)

- Add to `adaptive_ai_difficulty.ini`: `Mode=adaptive`, `TargetOffsetSec=0.0`, `Nudges=0`. Defaults keep today's behaviour (`AllAIFactor` 1.000, Enabled 0).
- Rating JSON written by the DLL only, `schemaVersion: 1`, one object per track id with median times, laps used, timestamp. The manager only reads it, and only Reset writes it (via a new atomic write, with backup).
- Keep the DLL's "Enabled read once" rule. The manager must not promise a live Enabled change.
- Championship mode is not defined anywhere in the code. **Open question** for the owner: what it does (e.g. fixed strength per championship round, or rating frozen).

## 6. Files that would change (PROPOSAL)

| File | Change | Size |
|---|---|---|
| `development\mods\adaptive_ai_difficulty\dll\adaptive_ai_difficulty.ini` | new keys (section 5) | small |
| `development\mods\adaptive_ai_difficulty\dll\adaptive_ai_difficulty.c` | read Mode, TargetOffsetSec, Nudges; write rating JSON; poll live | large (DLL) |
| `development\mods\adaptive_ai_difficulty\dll\adaptive_ai_difficulty.meta.json` | add `"settings"` and `"readme"` keys (readme file to create) | small |
| `development\lightloader\...\release_2.3.25_prep_2026-10-13\source_freeze\ManagerSchema.cs` | schema entries; `SettingDef` Scale/Offset | medium |
| `...\source_freeze\ManagerUi.cs` | new panel class / page for rating table and Reset; wire into `ModSettingsPage` | large |
| `...\source_freeze\SelfTest.cs` | new checks (schema, rating read, Reset refused while game runs) | medium |
| `...\source_freeze\LightloaderManager.cs` | `ManagerVersion` bump; maybe `KnownMods` entry (dev DLL is not in the catalog) | small |
| `...\release_2.3.xx\` record, changelog, catalog | only if the owner approves a public release | separate gate |

Not changed: loader (`lightloader_core.c`), `KnownLoaderHashes`, broker, Tools tab.

## 7. Risks

1. **Two Enabled switches.** The card toggle (`LightloaderDisabled.ini`, `LightloaderManager.cs:1225`) and the mod's `Enabled=` in the ini. Players may think one is off when the other is on. Mitigation: label the ini one "Adaptive AI on / off (applies at next start)" and show the card toggle state in the help.
2. **Fragile bool detection** in the generic view (`ManagerUi.cs:3370`). Avoid relying on it: give every key a schema entry.
3. **Write conflict with the game.** Manager writes are refused while the game runs (`ManagerUi.cs:3416`). Live nudges and rating must therefore be written by the DLL, never by the manager.
4. **Rating JSON corruption** from a crash mid-write. Mitigate with atomic write in the DLL and a parse check in the manager that shows "rating file unreadable" and offers Reset.
5. **Dev build in the catalog path.** The mod is "" and not in the catalog. A manager build containing its panel would ship the panel to users before the mod is released. Mitigate: gate the page on a build flag or only show it when the mod DLL is present with a matching meta id.
6. **Clamp mismatch.** The DLL clamps to 0.80..1.20; the slider must use the same bounds (`adaptive_ai_difficulty.c:379`, `:403`).
7. **Two-source versions.** The 2.3.26 tree is the newest source, but the owner test is pending. Building on it before that test means any new panel rides on an untested manager base. Mitigate: branch the panel off the 2.3.26 tree only after the owner test.
8. **Checks not run.** Nothing here was built, run or tested. Live game behaviour of Mode/Nudges is unknown.

## 8. Owner approval needed

1. Choose hand-coded panel (section 3) vs waiting for a generic schema.
2. Define Championship mode (section 5).
3. Decide whether Mode and TargetOffsetSec are live-applied (DLL work) or next-start only.
4. Approve a manager source change and rebuild. That means a new manager version, SelfTest run on the owner PC, owner live test, then the release gate (production copy, catalog). Agents do not publish.
5. Approve the new ini keys and the rating JSON file name.
6. Decide whether the panel ships only with the released mod (gate in section 7, risk 5).
7. Decide whether the 2.3.26 owner test must pass before this work branches off it.

## 9. Validation plan (pending, not run)

- Schema: `SelfTest` check that each new key round-trips through `IniFields` / `ApplyIniFields` with comments kept.
- Percent mapping: 0.800 shows "AI -20 %", 1.200 shows "AI +20 %", 1.000 shows "AI +0 %".
- Rating read: corrupt JSON shows the "unreadable" message and does not crash.
- Reset: refused with the game running; with the game closed it writes a backup first.
- Live: owner runs one race in Adaptive and one in Fixed, checks the DLL log (`INI:` lines) for the change within 2 s. Not run.

Records to update when work starts (per AGENTS.md documentation gate): the mod's `docs\HANDOFF_NEXT_2026-10-15.md`, and the manager's `release_2.3.26` or the next release record. Not edited in this task.
