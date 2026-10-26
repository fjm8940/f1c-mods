# Telemetry GUI redesign: LIVE / PAST source switch and Recordings page (2026-10-14)

Product: Telemetry (`web_telemetry`), console page (`telemetry_viewer.js`). Version string stays 1.2.0 (unchanged, not bumped).
Pipeline stage: **implement, offline verified. Gate NOT passed (blocked by in-progress TV Director C files, see section 5).**
Not installed. Not live-tested. The game was not running during this work (checked with PowerShell: no matching process).

Tags: FACT = read in code or observed in a test run this session. INFERENCE = reasoned, needs a test. OPEN = owner decision or live test.

## 1. Owner request (summary)

Owner, 2026-10-14: (1) make the Recordings page much better; (2) make it obvious whether the GUI shows LIVE (this game
session) or a PAST session (an earlier recording). Design by Claude, implemented as written except the deviations in section 3.

## 2. What changed

Files (all backups `<file>.BEFORE_20261014_gui`, see section 7):

| File | Change |
|---|---|
| `telemetry_viewer.js` | Source switch, source line, PAST banner, slot labels, past-mode notice on the pages, TV page message, Recordings page rewrite (cards, sessions, filters, sticky selection bar, notices), pure helpers exported for tests, CSS (`SRC_CSS`, root grid gets one extra row). |
| `viewer_test.js` | Old check of three row buttons replaced (rows now say `Delete...`; the sticky bar is the one visible "Move to Windows Recycle Bin" button). New block `1.2.1 GUI` (38 checks). |
| `web_telemetry.c` | Only the `VIEWER_UI_EXPECTED_SHA256` pin constant (written by `pin_sidecars.py`). No C logic changed. |
| `VERIFICATION_MANIFEST.json` | Sidecar pin for `telemetry_viewer.js` (AA2A...). Payload section re-pinned once to the gate2 build (see 5). |
| `f1c_mod.json`, `mod.json`, `web_telemetry.dll.mod.json` | `sidecars[]` pins (written by `pin_sidecars.py`). |
| `CURRENT_STATE.md` | One index line (newest). |
| this file | New. |

Not touched by this work: `director_state.inc`, the TV Director C code, `workbench_ui.js` (unchanged, hash E72BDD77...), storage C code, `storage_*.inc`.

### Part 1: LIVE / PAST (FACT, verified by smoke run and screenshots)
- Header: one switch with two big tabs, `LIVE - this session` (green when active) and `PAST SESSION` (amber when active).
- Thin accent line under the header: green in LIVE, amber in PAST (`.f1x.f1x-src-live/-past .f1x-title`).
- Line under the header:
  - LIVE: `Live: <track> <session> - recording now` / `- not recording`, or `Live: game not running` with the button `Open a past session`.
  - PAST: `Watching past session: <Track year> - <Session or mode> - <Day D Mon, HH:MM>` with `Change` and `Back to live`.
- Slot labels A/B: `Live`, or `Past: <short name>` (amber). The A/B logic itself is unchanged.
- Live-only indicators: header word `PAST` (was `LIVE`); lap strip greyed and disabled; footer greyed in PAST.
- PAST on a page (no data, see deviation D1): the page shows the note `Viewing a stored recording is not available in this
  version yet...` with `Recordings` and `Back to live`. No live data is drawn.
- TV Director tab in PAST: the note `TV Director controls the live game. Switch to LIVE to use it.` (CSS class on the TV
  element; the TV polling itself is unchanged).
- `Load` on the Recordings page switches to PAST with that recording. `Back to live` clears the storage selection
  (`POST /recordings/load` with an empty name; no file changes).

### Part 2: Recordings page (FACT, verified by smoke run and screenshot)
- Page header card: folder total, warning level text, green `Recording now: ...` pill (or grey `Not recording`), Refresh.
- Notices are one line (CSS nowrap and ellipsis). Success notices hide after 6 s (`setNotice` timer; `noticeHideMs` is 6000
  for success and 0 for errors). Errors stay with `Dismiss`.
- The duplicated label `Last job · Last job:` is fixed: the line is shown once.
- One card per recording: title from the folder name (`Brazil 1995 - Single player - Tue 13 Oct, 16:14`). Subtitle:
  `N sessions · size · Recording now · N recovered`. Names that are not storage names are shown as they are.
- Mode words come from `storage_v2.inc` `sv2_mode_token`: `Race` is shown as `Single player`, `CO-OP` and `Multiplayer` as
  they are. `Championship` and `Test Day` are shown as stored (the owner gave no mapping for them).
- Session rows in weekend order (Practice 1-4, Qualifying, Warm-up, Race). Name, state badge (Recording now green, Closed
  grey, Recovered amber with the owner tooltip, Archived grey, Archive interrupted and Damaged), size beside the badge.
  The file name is only in `Details` and in tooltips.
- Actions: group `Open` (primary) and `Delete...` (small). Session row `Open` and `Delete...`. Delete confirmations name
  the Windows Recycle Bin and keep `You can restore it from there.`
- Multi-select checkboxes (closed, recovered, archived sessions; never the open session). Sticky bar:
  `N selected: Archive - Move to Windows Recycle Bin - Clear`. One dialog for the batch, then one request per session.
- Filters on top: search box (track, year, session, plain title) and mode (All / Single player / CO-OP / Multiplayer).
  Newest first.
- Content max width 1100 px.

## 3. Deviations and reasons

- **D1 (OPEN, owner decision): PAST shows no data for storage v2 recordings.** The viewer has no reader for storage v2.
  The only past path is `?session=<id>` (1.0.1 archive, `ar_session_exists` in `web_telemetry.c`), which B1 records as no
  longer written. The owner asked for "no server API change unless strictly required". A reader is the R1/R2 work of
  `RECORDING_REVIEW_SPEC_2026-10-13.md` (spec only, owner questions open). It is not part of this batch. Result: the mode
  switch, banner and selection are real; the pages show a notice in PAST. Needs the owner's go for R1/R2.
- **D2:** `Open` on a session row opens the whole recording folder. The server's selection is per folder (`sj_loaded`).
- **D3:** Multi-delete sends one request per session (the server has no multi-target delete). The dialog is one.
- **D4:** `Live: game not running` means no fresh live sample (age over 2.5 s or sample age over 1.5 s, the same rule the page
  already uses for STALE). The live frame has no running flag (`game_running` exists only in `/director/state`). INFERENCE
  that a paused game shows as "not running" too; needs a live test.
- **D5:** Mode labels: only `Race`, `CO-OP`, `Multiplayer` were specified. `Championship` and `Test Day` are shown as stored.
- **D6:** The `/recordings` list is polled every 10 s on all pages (it was only polled on the Recordings page), so the LIVE
  line knows about recording state. One line changed in `startPolls`.
- **D7 (TV block, one guard line):** `tvSend` returns at once in PAST mode with the TV message. The owner allowed only the
  page message in the TV block; this guard stops a command being sent while the page says PAST. Nothing else in the TV
  block changed (poll, keys and C code untouched).
- **D8 (CSS):** the root grid of the console had six rows; the new source line needed a seventh (`grid-template-rows` in
  `CONSOLE_CSS`). Found by the screenshot probe: without it the stage was 20 px tall.
- **D9:** The 1.2.1 header shows the storage selection left on the game PC after a reload as one note
  (`Still selected on the game PC: ... Back to live clears the selection.`). The page does not clear it by itself.
- **D10:** Row buttons say `Delete...`; their tooltips name the Windows Recycle Bin (owner wording rule, enforced by
  `viewer_test.js`). The bar button `Move to Windows Recycle Bin` is the one visible button with that text.
- **D11:** Version not bumped (1.2.0), so the existing version checks stay valid.

## 4. Tests and checks

- `node viewer_test.js telemetry_viewer.js workbench_ui.js`: **VIEWER PASS 505 checks** (371 `ok()` calls in the file;
  38 in the new `1.2.1 GUI` block). The block covers titles, session order, mode keys, filter, subtitle, badges, notice timing,
  live line, past banner, slot labels, and source rules (TV message, no `Last job · ` duplicate, `Delete...` tooltips,
  one visible Recycle Bin button, `Change`, `Open a past session`).
- Headless Chrome smoke test (`%TEMP%\gui_smoke`, mocked `/telemetry`, `/recordings`, `/recordings/load`; synthetic data,
  not the game): LIVE line, PAST switch, banner, PAST notice on the Race page, Open, back to live, search "spa" (1 group
  card), mode filter, selection bar, Clear. No window errors after the mock was completed (the first run was blocked by
  mock gaps, the pre-change viewer shows the same gap; not a product issue).
- Screenshots (synthetic data, `%TEMP%\gui_smoke\shot_*.png`): LIVE state, PAST state with Recordings page: header card,
  cards, badges, sizes, Details, checkboxes, status line "PAST Live values are hidden in past mode."
- Gate attempts (`%TEMP%\gui_gate\`):
  - gate1: packaging audit, sidecar and payload staging PASS; `test_telemetry.py` failed on the manifest payload pin for
    `telemetry_viewer.js` (expected by the owner's rule). Payload section re-pinned to the staged build (backup taken).
  - **gate2: PASS** (36 PASS lines, 0 FAIL, `VIEWER PASS 505 checks`, storage v2 core, HTTP audits, SSE, soak 20 workers
    flat RSS, fuzz 40 x 2000 iterations 0 crashes, `Built ... web_telemetry.dll`). It covered the viewer before the last two
    small edits (footer grey, selected-session note).
  - gate3 and gate4: **FAILED, not in this work's files.** gate3: `runtime_test.c:3878` `DCAM_CHAIN_VERIFIED` undeclared;
    `director_state.inc` was changed at 21:23:09 and `runtime_test.c` at 21:23:27 by the TV Director work (no backup of that
    state exists). gate4: `director_state.inc:404` `-Werror` format truncation in the DLL build. Both are in the TV Director
    files, which this work does not edit. The gate is therefore NOT passed for the current tree.
  - The owner rule for a full gate: when the TV Director files are stable, rerun
    `set TELEMETRY_TESTS=1` then `build.bat` in the component folder (run through a `.cmd` file with PowerShell, because
    `cmd /c` with arguments dropped the command in this shell), then re-pin the payload if only the manifest payload pin fails.

## 5. Hashes (SHA-256)

| File | Hash | State |
|---|---|---|
| `telemetry_viewer.js` (current, sidecar pin) | `AA2ACAE2DA45A134C618FC451330D4A4EEBCB8BF9A1BBD214335C39B4F2CE9E9` | pinned by `pin_sidecars.py --check` = OK; not yet in a passing gate |
| `telemetry_viewer.js` (gate2 build, manifest payload pin) | `E138C23478DAC43FE04282B27A389AF3A72D838FDE3CF2636DB57AFCC1E8CFC0` | manifest payload is still this (stale) |
| `workbench_ui.js` | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` | unchanged |
| `web_telemetry.dll` (gate2 build, manifest payload pin) | `6AA6E6D962E5FFF31987492B727B51BD8F101133412BF4EA684D85409B7FC46F` | not installed |

The manifest payload section must be re-pinned to the next passing gate build (`pin_sidecars.py --payload`
`%TEMP%\f1c-web-telemetry-4.4.0`). It is stale now on purpose: it was not re-pinned to a build the gate could not verify.

## 6. Backups (`.BEFORE_20261014_gui`, taken 20:30-20:31 before the first edit, same folder)

`telemetry_viewer.js`, `viewer_test.js`, `web_telemetry.c`, `VERIFICATION_MANIFEST.json`, `f1c_mod.json`, `mod.json`,
`web_telemetry.dll.mod.json`, `CURRENT_STATE.md`. No file was deleted. Other agents' `.BEFORE_...` files were not touched.

## 7. Restoration

- Viewer: copy `telemetry_viewer.js.BEFORE_20261014_gui` back to `telemetry_viewer.js`, then `viewer_test.js` likewise.
- `web_telemetry.c`, `f1c_mod.json`, `mod.json`, `web_telemetry.dll.mod.json`, `VERIFICATION_MANIFEST.json`: **do not copy
  the whole file back**. The TV Director work changed `web_telemetry.c` and the manifest after 20:31. Revert only the pin
  lines (`VIEWER_UI_EXPECTED_SHA256`, the `sidecars` entries, `sidecar_viewer`, `payload.files`), or rerun
  `pin_sidecars.py` after restoring the viewer.
- `CURRENT_STATE.md`: copy back, then remove the index line of this record (the TV agent may have added lines since; prefer
  removing only the line).

## 8. Owner live checks (game closed to start, then while recording)

1. Header: LIVE tab green, PAST tab amber when selected; the accent line changes colour.
2. LIVE line while recording: `Live: <track> <session> - recording now`. Stop recording and check `not recording`.
   Close the game: `Live: game not running` with `Open a past session`. (Checks D4.)
3. Recordings: titles, mode words, badges, sizes beside the badges, `Details` shows the name.
4. Open a recording: PAST banner, slot labels `Past: ...`, the page shows the PAST notice (D1); TV tab shows the TV message.
5. Back to live: LIVE again, the storage selection is cleared (note D9 after a reload).
6. Delete a closed session (single, then two with the checkboxes): Recycle Bin, one notice, list updates. The open session
   has no checkbox and no delete.
7. Notices: a success line disappears after about 6 s; an error line stays until Dismiss.
8. Search and mode filter on several recordings.

## 9. Open items

- D1 decision (R1/R2 viewer reader for storage v2) and its spec questions.
- Clean gate once the TV Director files compile again; then re-pin the manifest payload to that build and record the hashes.
- Owner live checks in section 8. No live test was run in this work.
- Validator (`tools/workspace/check-workspace-structure.ps1`) reports two errors that existed before this work (top-level
  `.claude` and `plans` folders). Not fixed: needs Matias's approval (AGENTS structural guardrail).

## 10. Documentation gate

- Durable record: this file. Indexed: one line at the top of `CURRENT_STATE.md`.
- Fact versus inference: tagged in sections 2-4.
- Pending validation: gate, live checks (section 8), D1 decision.
- No secrets or raw logs copied. No `api1.txt` or private keys read.
