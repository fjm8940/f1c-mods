# Task for DeepSeek: Camera Editor 1.0.2 — reset ONE value, then ship

Prepared 2026-10-04 by Claude for Matias. You are working in `%USERPROFILE%\Desktop\F1`. Read the root
`AGENTS.md` first and follow it (no deletions, UTF-8 without BOM, preserve before-state, record everything).

## 1. Player request (Discord, 2026-10-04, user "Tester G")

> "i do hope that in the future u can reset one thing instead of the whole. say i js wanna reset back but i have
> to reset the whole"

Screenshot context: the Position group shows three rows (left/right 0, down/up 0.26, back/forward 0.4402) and a
single "↺ back to game default" link on the group title. That link removes the whole key (all three numbers). The
player wants to reset **one number** (for example only "Down / up") and keep the others.

## 2. Where things are

Product: `development/mods/camera_editor/` (read its `README.md` first: layout, layer rules, verification record).
Only the app changes. **Do not touch `plugin/`** (the game plug-in stays 1.0.1, DLL
`E1BCA919422F3DDF254FA36E11B020FA7A0D07E60C89AE39795593EDF7241482`).

| File | What matters |
|---|---|
| `app/main.py` `class ValueRow` (~line 123) | One row = label, hint, value box, −, slider, +, note label (`self.note`, grid column 6). `show()` fills it. |
| `app/main.py` `App.group()` (~line 800-830) | Builds a group; `mine` = value set in the chosen scope, `below` = value without this scope; the "↺ back to …" label calls `self.clear_key(cam, key)` (whole key). |
| `app/main.py` `App.set_value(cam, key, index, value, commit)` (~line 874) | Writes the full tuple into **every** target file (`self.target_paths()`: one file, or several cars), pushes one undo group, live-writes, autosaves. |
| `app/main.py` `App.clear_key` (~line 896), `push_undo`, `_set_or_clear`, `after_change` | Undo groups and live write. |
| `app/layers.py` `Stack.below(camera, key, scope)` | Value the key has without the chosen scope and anything above it = what "reset" goes back to. |
| `tests/test_app.py` | Offline tests (45 checks now). Run `python tests/test_app.py`. |

## 3. What to build (spec)

1. Each `ValueRow` gets a small **"↺"** button (same style as the other small controls: `FlatButton(..., "↺",
   color=BUTTON, fg=INK, padx=8, pady=2)`), placed between "+" and the note (move the note to column 7).
   Tooltip-free; the note column may say "changed" in the scope colour when this one number differs from `below`.
2. The button is shown/enabled **only when this number differs from what it would be without the chosen scope**
   (`mine is not None and abs(mine[index] - below[index]) > 1e-9`). Otherwise show nothing in that cell (keep the
   column width so rows stay aligned).
3. Clicking it resets only that number: new tuple = current tuple of the chosen scope with `[index] = below[index]`.
   - If the resulting tuple equals `below` in every position, remove the key from the scope entirely (same as
     `clear_key`) so the file stays minimal (`format=2` = only changed keys).
   - Otherwise write the tuple (like `set_value`).
   - Several cars chosen: apply the same rule **per target file** (each file's own tuple; use that car's own
     `below` value — build it with `L.Stack(self.game_root, self.blocks, <a .veh of that car>, <its season>,
     self.cache, drivers=False).below(...)`, the same way `context()` builds the shown car). One undo group for all
     files (`push_undo` before changing).
   - Live write and autosave exactly like `set_value` (call `after_change(cam, [key], paths)`).
   - Status line: `flash(f"{label} reset to {value} (Ctrl+Z to undo)")`.
4. Keep the group-level link but rename it to **"↺ reset all three"** / **"↺ reset both"** (use the row count) so
   the two levels are clear. Its behaviour does not change.
5. Integer keys (Color, Flags) and Clear: give them the same per-number reset; Clear has one value (the group link
   is enough — no per-row button needed).
6. Nothing else changes: colours (Driver AI Toolkit theme), layout, texts, scopes, plug-in.

## 4. Tests to add (in `tests/test_app.py`, function `gui_session` or a new one)

- Global scope: change PositionOffset Y and Z of COCKPIT, press the Y row's reset → Y equals `below` Y, Z keeps the
  changed value, file still contains PositionOffset (Z only differs).
- Then reset Z too → PositionOffset disappears from the file (file keeps other keys).
- Ctrl+Z after a single-number reset restores the number.
- Two cars chosen with different values: one reset click resets that number in both files; one Ctrl+Z restores
  both.
- The ↺ button is absent on a row whose number equals `below`.
All existing checks must still pass. Also run `python -m pyflakes app/*.py`.

Render a screenshot to check the row layout (see how Claude did it: a small script that builds `main.App`, calls
`set_game_root(<a game folder>)`, `select("COCKPIT")`, overrides `app.save_now = lambda: None` so nothing is
written, and grabs the window with `PIL.ImageGrab`). Do not write into Matias's game folder for the screenshot.

## 5. Ship 1.0.2 (the ordinary procedure — copy 1.0.1, change only what differs)

1. Changelog first: add to `operations/release_management/changelogs/camera_editor.json` (newest first):
   `{"version": "1.0.2", "date": "<today>", "changes": ["Reset one number: every value now has its own ↺ button, so you can put back just 'Down / up' and keep the rest. The group link resets the whole setting as before."]}`
2. `python development/mods/camera_editor/tools/build_tool.py 1.0.2` (it refuses an existing folder; never edit a
   built release). The package must contain the unchanged plug-in hash above — check it.
3. Make `development/mods/camera_editor/releases/1.0.2/deploy/` like `releases/1.0.1/deploy/`:
   - download the live catalog with scp to `catalog.live-before.json` and note its SHA-256 (it should be
     `B7D4E055BF274190252988B07D84452B1EB4E41ECBF521B1CB7147B43E4FECCA` unless something else shipped);
   - `catalog.unmerged.json` = live catalog with all `changelog` fields removed and only the `camera_editor` tool
     entry changed (version 1.0.2, package url `https://mods.palloset.com/downloads/camera_editor/v1.0.2/camera_editor-1.0.2.zip`,
     sha256 upper-case, size); then `python operations/release_management/changelogs/merge_changelogs.py
     catalog.unmerged.json public/catalog.json`; verify mods and the toolkit entry are identical to live;
   - `public/` = zip, `camera_editor-README.txt` (from the built folder), `catalog.json`; `SHA256SUMS.lower`
     (`sha256sum` format, two spaces);
   - `remote/deploy_cam102.sh` = copy of `releases/1.0.1/deploy/remote/deploy_cam101.sh` with V/ZIP/ST/BK names
     changed to 1.0.2 and the expected live catalog hash updated. `bash -n` it.
4. Validate the catalog with the four managers (2.0.2, 2.1.1, 2.2.0, 2.2.1 in `production/lightloader/`) via
   `[Reflection.Assembly]::LoadFrom` + `Lightloader.Core.ValidateCatalog`, and install the zip with
   `Lightloader.ToolsCore.InstallBytes(root, entry, bytes, false)` into a temp folder (unwrap PowerShell objects
   with `.psobject.BaseObject`). Defender scan `public/` (`MpCmdRun.exe -Scan -ScanType 3 -File <dir> -DisableRemediation`).
5. Deploy (Matias authorised the ordinary procedure for this update):
   `ssh -i %USERPROFILE%\.ssh\server_key.pem f1admin@203.0.113.1 "mkdir -p /tmp/cam-1.0.2"`, `scp` the public
   files + `SHA256SUMS.lower` + the script to `/tmp/cam-1.0.2/`, run it, save its output as
   `releases/1.0.2/deploy/verification_deploy_output.txt`. Never read or print the key. Expect `FILES_OK`,
   `LINKS_OK`, two `LINK_OK`, `CATALOG_OK`. If anything fails the script restores; stop and report.
6. Double check after deploy: POST an update check to `https://mods.palloset.com/lightloader/api/check` (browser
   User-Agent, `Content-Type: application/json`, installId `00000000-0000-4000-8000-000000000000`, mods
   `[{"id":"camera_editor","version":"1.0.2"}]`) → 200 and catalog tool version 1.0.2; then with the 2.2.1 manager
   code: `Core.Download(url, sha)` + `InstallBytes` over a 1.0.1 install → installed version 1.0.2, `tools\.old` exists.
   The telemetry service v5 already accepts `camera_editor`; do not touch it.
7. Production: copy the zip and README to `production/mods/camera_editor/v1.0.2/` with `SHA256SUMS.txt`.
8. Records (save a copy of each file you edit under `workspace_organization_audit/CAMERA_EDITOR_1_0_2_BEFORE_<date>/`
   first):
   - `operations/release_management/camera_editor/RELEASE_1_0_2.md` (model: `RELEASE_1_0_1.md`: request, change,
     hashes, tests, deploy output, double check, not verified);
   - `production/RELEASE_REGISTRY.md`: update the Camera Editor table row (version, date, zip SHA-256) and add a
     one-line note at the top;
   - `governance/PRODUCT_STATUS.json` camera-editor: `productionPath` → v1.0.2, `evidencePath` → RELEASE_1_0_2.md
     (use exact string edits; the file uses `\\` in paths — do not round-trip through a JSON dump);
   - `operations/README.md`: one index line at the top;
   - `development/mods/camera_editor/README.md`: verification row for 1.0.2.
   - Run `powershell -NoProfile -ExecutionPolicy Bypass -File tools\workspace\check-workspace-structure.ps1`
     (the only known error is the pre-existing empty `.claude` folder).
9. Optional: copy the new `app/main.py` into Matias's `Documents\F1 Challenge V10\tools\camera_editor\app\` only if
   he asks; otherwise Lightloader's Tools page offers the update.

## 6. Pitfalls Claude hit (avoid them)

- Bash heredocs in this environment collapse `\\` → `\`: write patch scripts with a file-writing tool, not inline
  heredocs, when strings contain backslashes.
- Car folders are compared case-insensitively (`L.folder_key`); always go through `values_for(path)` for profile
  data (the cache is keyed by `folder_key`).
- Save folder comes from `Config.ini` (`L.save_dir`, `L.default_cfg`), never hard-code `Save`.
- `tests/test_app.py` uses temporary folders only; keep it that way.

## 7. Report back to Matias

What changed, the 1.0.2 zip SHA-256, test counts, deploy lines, double-check results, and anything not verified
(at least: not tried in a live game session).
