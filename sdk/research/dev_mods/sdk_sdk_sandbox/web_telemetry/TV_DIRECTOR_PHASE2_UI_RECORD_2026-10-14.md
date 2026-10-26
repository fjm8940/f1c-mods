# TV Director phase 2 UI record (2026-10-14)

Scope: follow-ups 1, 2, 3 and 5 of `TV_DIRECTOR_PHASE2_RECORD_2026-10-14.md` section 5, plus the DLL build fix and the `CarSwitch` default. The task text takes precedence where it differs (see section 2). Stage: implemented in source, plain DLL build OK, viewer tests OK. NOT live-tested. Full gate NOT RUN (game running at check time, section 4). `governance\PRODUCT_STATUS.json` not changed.

## 1. What changed

### DLL source (`director_state.inc`)
- Build error fixed. `dcam_res` copied a 480-byte message into the 240-byte `reason` field with `snprintf`, which `-Werror=format-truncation` rejects. It now uses `_snprintf` with an explicit terminator (`sizeof - 1`, then `'\0'`), as phase 1 did. Same 239-character truncation. No behaviour change.
- `runtime_test.c`: no `DCAM_CHAIN_VERIFIED` or `DcamChainFn` reference is left in any source (searched `*.c`, `*.inc`, `*.js`, `*.h`, `*.py`). The file compiles with the gate flags (`-O1 -Wall -Wextra -Werror`, output in `%TEMP%`). No edit was needed. The file is unchanged.
- `web_telemetry.c`: only the `VIEWER_UI_EXPECTED_SHA256` pin line changed (re-pinned by `pin_sidecars.py`).

### Viewer (`telemetry_viewer.js`, TV block)
- Leader gap column: `tvdDrivers` copies `tt`, `rt`, `fs` and sets `gap = tvdGapText(car, leader)`. `tvdGapText` calls the existing `gapTo(c, leader, true)` (about line 2844). Leader = the car with `p == 1`; no leader gives the dash. The cell title says the value is the gap at the last line crossing (race clock), not a live gap. The battle column stays `--`.
- Cut to another car: `tvdCutBlock(j, driverId)` returns `Car switch is off: set [Director] CarSwitch=1 in web_telemetry.ini. Nothing was sent.` when `carswitch_enabled === false` and the target is not `followed.roster_id`. `tvSend` returns before the POST in that case. Cut to the car on screen, and the angle and restore buttons, are unaffected.
- Status line: with writes on and `carswitch_enabled === false`, it says the car switch is off and that angle changes still work.
- Result line: `tvdVerifyText(verified_next_frame)` gives `checking...`, `confirmed on the next frame` or `not confirmed on the next frame` (nothing for `null`). `tvdResultText` appends it after the read-back. `not confirmed` is shown as bad (red).
- Pending verify: a POST with `verified_next_frame: "pending"` stores its `seq`. `tvRender` replaces the result line when `last_result` with that seq is no longer pending (the DLL updates `g_dcam_result` in place).
- PAST SESSION: unchanged. The two `TV Director controls the live game. Switch to LIVE to use it.` strings and the `isPast()` guard before the POST are as in the redesign (the existing test counts them).
- Note text now starts `Phase 2.`. The phase 0 test assertion was updated to match (`viewer_test.js`, the "tvd: phase note present" line).
- New exports: `tvdGapText`, `tvdCutBlock`, `tvdVerifyText`, `TVD_DASH`.

### Ini (`web_telemetry.ini`, CRLF kept)
- Under `[Director]`: one comment line and `CarSwitch=0` after `Enabled=0` (default off). `Enabled=0` is unchanged. No test pins the ini bytes. The ini is in the payload list (section 5) and in the manifest payload section (not re-pinned, section 4).

### Pins and metadata
- `pin_sidecars.py` run once. Viewer pin `0C8F2606...` written to `web_telemetry.c`, `VERIFICATION_MANIFEST.json` (`sidecar_viewer`), and the `sidecars` lists in `f1c_mod.json`, `mod.json` and `web_telemetry.dll.mod.json`. `--check` afterwards: `pins: OK`. CRLF endings kept in all files that have them.
- `VERIFICATION_MANIFEST.json` `payload` section NOT re-pinned (section 4).

### Index (`CURRENT_STATE.md`)
- Three lines added at the top of the list: this record, `TV_DIRECTOR_PHASE2_RECORD_2026-10-14.md` and `TV_DIRECTOR_PHASE2_RESEARCH_2026-10-14.md` (the last two were missing).

## 2. Deviations from the phase 2 record and the request

- Follow-up 3 of the phase 2 record (disable Cut unless `carswitch_enabled`) was NOT applied. The task says a Cut to another car shows the reason and sends nothing, while angle changes still work. Disabling Cut would also block the same-car angle path, so the Cut button stays enabled and the reason shows on click.
- Follow-up 4 (circuit owner wording) was not touched (not in the task list).
- Battle stays `--` (research C2: not verified).
- No new lines were appended to `findings.jsonl` (no new game result in this work).
- `PRODUCT_VERSION` / `VERSION` not bumped (the viewer test pins `VERSION === '4.4.0'`).

## 3. Tests and checks

| Check | Command or place | Result |
|---|---|---|
| Viewer suite | `node viewer_test.js telemetry_viewer.js workbench_ui.js` | `VIEWER PASS 525 checks` (505 before, +20 new phase 2 checks) |
| Syntax | `node --check telemetry_viewer.js`, `node --check viewer_test.js` | pass |
| DLL sources, gate flags | `gcc -c -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function` on `web_telemetry.c`, `legacy_formats.c`, `sha256.c`, `f1c_broker.c` (`%TEMP%` output) | rc 0 |
| `runtime_test.c` compile only (NOT run) | gate-flag `gcc` build into `%TEMP%` | rc 0 |
| Plain DLL build | `build.bat` from the component folder via PowerShell, no `TELEMETRY_TESTS` (runner `%TEMP%\tvd2ui_run.ps1`, log `%TEMP%\tvd2ui_build.log`) | rc 0; sidecar pins OK; payload gate OK (5 files); built `%TEMP%\f1c-web-telemetry-4.4.0\web_telemetry.dll` |
| Sidecar pins | `python pin_sidecars.py --check` | `pins: OK` |
| Workspace validator | `tools\workspace\check-workspace-structure.ps1` | 2 errors that existed before this work: top-level `.claude` and `plans`. Not changed (structural, needs owner approval) |

New phase 2 viewer checks (block "TV Director phase 2"): leader gap sequence `LEADER`, `+0.500`, `+1L`, `-` (retired), `-` (no time); row fields `tt`, `rt`, `fs`; no leader gives the dash; battle cell still the dash; Cut to another car blocked with the reason at `CarSwitch=0`; Cut to the car on screen not blocked; `CarSwitch=1` not blocked; older server without the field not blocked here; no followed car counts as another car; verify words; result lines for true, false, pending and null; source order (CarSwitch block and past check before the POST); still exactly one POST in the TV block; shipped default `CarSwitch=0`.

Not tested offline: the game-memory paths and live result text. The DLL-side refusal for `CarSwitch=0` is covered by the phase 2 record's offline block, not re-run here.

## 4. Gate

- Game check. The task's command `tasklist | findstr /I "F1C F1 Challenge"` was not a valid check: findstr took the spaced text as file names ("Cannot open F1C F1 Challenge"), so its exit code meant nothing. The corrected check (`tasklist` and `Get-Process`) at about 21:55 found `F1 Challenge Online` (PID 8896) and `Lightloader` (PID 26024) RUNNING. The workspace rule is no gate or runtime test while the game runs, so the full gate was NOT run.
- Gate result lines: `TELEMETRY_TESTS=1 build.bat`: NOT RUN (game running). Exit code: not available.
- Manifest payload section: NOT re-pinned. It still holds the hashes from the earlier gate-repin (see the redesign record section 5). Re-pin only after a passing gate: `python pin_sidecars.py --payload %USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-4.4.0`.
- To finish: close the game, then run from PowerShell in the component folder with `$env:TELEMETRY_TESTS='1'` and a log in `%TEMP%`. If the only failures are payload hashes, re-pin as above and rerun. Any other failure: fix the cause and rerun until exit 0.

## 5. Payload hashes (SHA-256, staged candidate in `%TEMP%\f1c-web-telemetry-4.4.0\payload`, UNGATED)

| File | SHA-256 |
|---|---|
| `web_telemetry.dll` | `04F923D420B29E091CE13251D82AA3CE1A08DEAF4B89F51AE3076A78B7D958BC` |
| `workbench_ui.js` | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` |
| `telemetry_viewer.js` | `0C8F2606650BD72B26027E8921950E1D0CF879A5CBB893244A35CF1235A97B7B` |
| `web_telemetry.ini` | `A7691C6E7C22F25E26A041B3E804153797DE0BD71E7986C28107763825F54F05` |
| `web_telemetry.dll.mod.json` | `40BC4635D464A5346527DB7C57A5C89F03F885E85EB5E744F688A87D68CF17B6` |

Component sources changed or checked (SHA-256):

| File | SHA-256 |
|---|---|
| `director_state.inc` | `AE39DA3A5D38F4A3119B12F5E0A0FF94D9D854422508830B0C4FA64DBB6B39D8` |
| `telemetry_viewer.js` | `0C8F2606650BD72B26027E8921950E1D0CF879A5CBB893244A35CF1235A97B7B` |
| `viewer_test.js` | `CDCEA7789CBA1E246F85D264134976B31B0289CC662DB6E9B7B0F2FF13F3679D` |
| `web_telemetry.ini` | `A7691C6E7C22F25E26A041B3E804153797DE0BD71E7986C28107763825F54F05` |
| `web_telemetry.c` | `8BB39F6C1E88FB26D563DEFEEE2E66E173113A0B253B188778EB299649619C75` |
| `runtime_test.c` (unchanged) | `C220D93D62A180439E9AA6DCCA604F9ACF2EEE60679EA483DE95033387E4C6FF` |

## 6. Backups (suffix `.BEFORE_20261014_tvd2ui`, taken before the first edit, same folder)

`director_state.inc`, `runtime_test.c`, `telemetry_viewer.js`, `viewer_test.js`, `web_telemetry.ini`, `web_telemetry.c`, `VERIFICATION_MANIFEST.json`, `CURRENT_STATE.md`, `mod.json`, `f1c_mod.json`, `web_telemetry.dll.mod.json`. No file was deleted. Other agents' backup files were not touched.

## 7. Restoration

- Viewer and its test: copy `telemetry_viewer.js.BEFORE_20261014_tvd2ui` over `telemetry_viewer.js`, and `viewer_test.js.BEFORE_20261014_tvd2ui` over `viewer_test.js`.
- Ini: copy `web_telemetry.ini.BEFORE_20261014_tvd2ui` back (removes the `CarSwitch=0` line and its comment).
- DLL source: copy `director_state.inc.BEFORE_20261014_tvd2ui` over `director_state.inc`. This reverts the build fix, so the -Werror build fails again.
- `web_telemetry.c`, `VERIFICATION_MANIFEST.json`, `mod.json`, `f1c_mod.json`, `web_telemetry.dll.mod.json`: revert only the viewer pin lines (`VIEWER_UI_EXPECTED_SHA256`, `sidecar_viewer`, the `sidecars` entries), or restore the backups and then run `python pin_sidecars.py` after restoring the viewer. Do not copy these files back wholesale: other records changed them after the backups.
- `CURRENT_STATE.md`: remove only the three index lines added at the top.
- `runtime_test.c`: no content change.

## 8. Documentation gate

- Durable record: this file. Indexed in `CURRENT_STATE.md` together with the phase 2 record and research lines.
- Fact versus inference. FACT: build, test and hash results above. DERIVED: the leader gap and the verify behaviour in the game (not run live).
- Pending validation: the full gate (section 4); owner live tests T-C1 (gap against the in-game timing screen), T4' (next-frame verify, including `redirected`), T2b (car switch with `Enabled=1` and `CarSwitch=1`, test build only), and the `CarSwitch=0` refusal text in the browser.
- No credentials, `api1.txt`, private keys or raw game output were read or copied. No game folder was read or written. Build output stayed in `%TEMP%`.
