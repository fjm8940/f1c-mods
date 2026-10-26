# Telemetry 1.1.0 storage v2, batch 5 record (2026-10-13)

Scope: owner decision A (replay-style recording names), owner decision B (recovered originals to the Recycle Bin), and
handoff items T1-T4 (`development/HANDOFF_JOINT_2026-10-13.md`, lines 157-164: track/year, weekend key, exit-time recovery,
viewer rows).
Stage: implement and verify (gated build PASS on the b5 build). Not installed, not live-tested, not promoted. No deploy.
`governance\PRODUCT_STATUS.json` not changed (no stage change is claimed by this record).

## 0. Starting state

- Game closed at start and at every gate (`tasklist | grep -i challenge` empty, rc 1).
- Workspace validator (`tools\workspace\check-workspace-structure.ps1`) reports 2 errors that predate this task:
  unexpected top-level directories `.claude` and `plans`. Not touched.
- Backups made before any edit, `cp -pn` to `<name>.BEFORE_20261013_recnames`: `storage_v2.inc`, `storage_writer.inc`,
  `storage_jobs.inc`, `compatibility_runtime.inc`, `runtime_test.c`, `telemetry_viewer.js`, `CURRENT_STATE.md`. Also made,
  but not edited: `storage_api.inc`, `viewer_test.js`, `test_telemetry.py`.
- Not touched: `F1 Challenge V10` and `Documents\F1 Challenge V10` (read only: recordings folder listed and one folder read).

## 1. Facts (verified by reading code or live files)

- FACT, T1 cause: `sv2_year_prefix` accepted only `YYYY_` (storage_v2.inc, before the fix at line 808). Installed names
  such as `1995CK_Adelaide.aiw` gave year 0 and track `Unknown`.
- FACT, related bug found while editing: `sv2_aiw_parse` reused the base-track offset variable for the parent-folder
  check, and the parent branch hard-coded `+5`. Fixed with a separate offset (`ptrack_at`). The parent branch is
  reachable only in theory (the base-match branch wins first).
- FACT, T3 cause (no close on exit): the stop event `Local\f1c_web_telemetry_stop` is created at
  `compatibility_runtime.inc:186` and is never signaled anywhere in the telemetry folder (grep). The worker loop
  (`compatibility_runtime.inc:191`) exits only when `g_running` is false, and `g_running=FALSE` is set only on the worker's
  own exit path (`compatibility_runtime.inc:277`). So the `sw_stop(5000)` call at line 279 is never reached on a normal
  game exit. `DllMain` DLL_PROCESS_DETACH (line 350-353) only wrote `process_exit` to the usage log. Nothing closed the
  open `.f1t.part`, so the next start repaired it to `recovered` and kept `.f1t.part.damaged`.
- FACT, live evidence: `Documents\F1 Challenge V10\Telemetry\recordings\Unknown_0000_2026-10-13_15-59\` holds
  `Unknown_0000_2026-10-13_15-59.f1t` (recovered, 16:00) and `...f1t.part.damaged` (15:59). The same pattern exists for
  the other two recovered sessions in the handoff (16:10, 16:18).
- DERIVED (inference, not run live): on a normal `ExitProcess`, Windows ends the other threads before the DLL
  DLL_PROCESS_DETACH call, so the writer thread is gone when DETACH runs. The new code handles both orders (section 2, T3).
- FACT, API: `sa_name_ok` (storage_api.inc) already accepts spaces, dashes and digits, rejects `..`, `\`, `/`, `:`,
  control and non-ASCII bytes, and limits names to 119 characters. No change was needed there.
- FACT, schemas: `schemas/recordings*.json` have no id pattern, so spaced ids validate.
- FACT, the lightloader export `F1CMultiplayerOverhaulSessionKind` returns 0 (mode not 5), 2 (CO-OP) or 1 (other
  multiplayer) (`development/mods/coop_mode/dll/coop_mode.cpp`, per `project_*` findings entry
  `replay-mode-kind-export-20261013`). The copied CO-OP rule (`== 2`) matches it.

## 2. Changes

### T1: track and year parsing (`storage_v2.inc`)
- `sv2_year_prefix`: 4 digits, then 0 to 6 ASCII letters, then `_`, then at least one track character. The year is the
  4 digits. `2002_Silverstone`, `1995CK_Adelaide`, `2003RH_A-1_Ring`, `1996CTDP_BUENOS` all parse. `1996TOOLONGX_...` (8
  letters) is refused, so it stays `Unknown` (year 0).
- The parent-folder branch uses its own offset (see FACT above).

### Decision A: replay-style names (logic copied from `replay_naming.c`, not linked)
- Session file stem: `<COUNTRY> <YEAR> - <Mode> - <Session> - YYYY-MM-DD HH-MM`, for example
  `BRAZIL 1995 - Championship - Race - 2026-10-13 16-14`. Then the existing unique suffix rule (`_2`, `_3` ...).
- Weekend folder: `<COUNTRY> <YEAR> - <Mode> - YYYY-MM-DD HH-MM` (the first session of the weekend).
- Country (`sv2_country_from_aiw`): folder after `Circuits\`; `Classics` uses its track folder; `_` becomes space; upper
  case; aliases Adelaide AUSTRALIA, Aida JAPAN, Estoril PORTUGAL, Jerez SPAIN. Fallback: the parsed track name.
  Characters Windows forbids and non-ASCII bytes are dropped (`sv2_clean_token`).
- Mode (`sv2_mode_token`, from the game mode global `0x7D0410`, already `RACE_MODE`): 1 Test Day; 2/3 Race; 4
  Championship; 5 Multiplayer, or CO-OP when the multiplayer export returns 2; other values omitted.
- Session label (`sv2_session_token`): 0 Test Day, 1-4 Practice 1-4, 5 Qualifying, 6 Warmup, 7 Race, else `S<n>`.
- Test Day (game mode 1): the session part is omitted (`<COUNTRY> <YEAR> - Test Day - <time>`).
- Mode and kind are captured at session OPEN on the capture side (`storage_writer.inc`, `sw_emit`, lines 304-305).
  The close uses the open-time values, so a later re-read of the AIW path cannot rename a session.
- Length: all parts are bounded (country 64, mode 24, session 24, stem 160), far below the existing caps.

### T2: weekend grouping (`storage_v2.inc`, `storage_writer.inc`)
- The weekend key is `COUNTRY|YEAR|MODE` (`sv2_weekend_key`). A change of any part starts a new weekend.
- A Test Day (mode 1) is always its own weekend, because its key differs from race keys. The old rule (Test Day after a
  race with the same key starts a new weekend) is kept.
- All-`Unknown_0000` merging is gone: keys now differ by country and mode.

### Decision B: recovered originals to the Recycle Bin (`storage_jobs.inc`, `sj_repair_part`)
- After the repaired copy is written, verified and renamed to its final name, the function reads the recovered file
  back and checks it (closed, recovered flag, same ticks and data blocks, valid bytes = file size).
- Only then: `sj_delete` on the `.f1t.part.damaged` copy (existing path: `SHFileOperationW`, `FOF_ALLOWUNDO`, pre-checks).
- If the read-back fails, or the bin move fails, the original stays and a log line says so. No hard delete was added.
- The log line `storage: recovered ...; original ... moved to the Recycle Bin` is written on success.

### T3: close on normal exit (`storage_writer.inc`, `compatibility_runtime.inc`)
- New `sw_process_exit()` (storage_writer.inc, after `sw_stop`). Called from `DllMain` on DLL_PROCESS_DETACH.
  - If the writer thread still runs: `sw_stop(2000)`, which is bounded to 2 s and then gives up with a log line.
  - If the writer has already ended: drain the ring on this thread, apply the events, close the open session (END block,
    header CLOSED, rename `.part` to `.f1t`), and write the index.
  - Does nothing when the writer never started.
- Result (expected, not yet live-tested): a normal game exit leaves `.f1t`, not `.f1t.part` and no `recovered` entry.
- A crash or `TerminateProcess` still leaves a `.part`, which the startup repair handles as before.

### T4: viewer rows (`telemetry_viewer.js`, Recordings page)
- Recording rows already show the id (`r.id`), which is now the readable name, with the weekend under it. Unchanged.
- Session rows now show the session label (for example `Race`) in bold, with the full name under it. The label is read from
  the name: second-to-last part when there are 4 or more parts, the mode when there are 3 (Test Day), none for older names
  (those show the id as before). No layout change.
- Old-style recordings (`Silverstone_2002_...`, `Unknown_0000_...`) still list, load and delete (no API change).

## 3. Tests

Run:
- `node viewer_test.js telemetry_viewer.js workbench_ui.js`: PASS, 415 checks. No check needed an update.
- Gated build, `TELEMETRY_TESTS=1`, game closed: PASS, `GATE_RC=0`, all gate lines PASS, no FAIL. Log in
  `%TEMP%\wt_b5_gate2.log`. The first gate run failed on one check (`runtime_test.c` line ~3186, repair test expected
  the original `.damaged` to stay). That test was updated for decision B: it now uses a per-cut Recycle Bin double
  (`sj_test_shfo_double`, a folder under the test folder, never the real bin), checks that the original is gone from the
  work folder, and reads the moved copy from the bin folder. The double is reset afterwards.
- Naming unit checks added to `runtime_test.c`: `t2_u8_aiw` (4 new AIW rows, incl. the letter forms and a refused
  8-letter form), `t2_u9_naming` (session, weekend and collision names in the new form; country, mode and session
  tokens; weekend key sequence). Included in the gate run above.
- `pin_sidecars.py` run after the JS edit (pins updated), and `pin_sidecars.py --payload <build dir>` after the build.
- `python -m pytest -q test_telemetry.py`: see section 6 (result recorded when the run finished).

Not run, and why:
- Live game checks of names, recycle, exit close and viewer rows: needs the owner to play a session (game closed for this
  work; the handoff asks for live checks B2-B9 style).
- Browser check of the viewer rows: needs the live recordings API; the offline viewer test covers the JS path only.
- No install and no deploy (owner rule).

## 4. Hashes (SHA-256, build `%TEMP%\f1c-web-telemetry-1.1.0-b5`)

| File | SHA-256 |
|---|---|
| `web_telemetry.dll` | `08F7E4D1899F7DD308CB5AA6F6398A48FF8163ACA22DFDEFDC152E7E9019E892` |
| `telemetry_viewer.js` | `5AF2616FB75A0AC362DC15B0FB3EF539602E8EBD986B97F24D5F79D098031ED8` |
| `workbench_ui.js` (unchanged) | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` |
| `web_telemetry.ini` (unchanged) | `CFA194343895644FE7E4C95792AFE5AA7627F6AB97D561F7219451E7785E2D01` |
| `web_telemetry.dll.mod.json` | `5C5863CF97354C2675CAB3171B46393A867583CDE3497B192A3B3775149D24DD` |

The DLL hash is the same after the gated build (the runtime test edit does not change the DLL).

## 5. Payload and install files

Payload folder: `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-b5\payload` (the 5 files above). Staged only,
not installed. The payload gate passed (exactly 5 files, byte-exact, manifest hashes match).

Install files (when the owner approves an install): `web_telemetry.dll`, `telemetry_viewer.js`, `workbench_ui.js`,
`web_telemetry.ini`, `web_telemetry.dll.mod.json`.

## 6. Pytest

- `python -m pytest -q test_telemetry.py`: not the right command. `test_telemetry.py` is a live HTTP harness script that
  reads the build folder from `sys.argv[1]`; under pytest it failed at collection (`-q` read as a path). Not a code defect
  in this batch; the batch 4 record ran it as a script.
- `python test_telemetry.py "%TEMP%\f1c-web-telemetry-1.1.0-b5"` (game closed; it starts the runtime exe as a server, which
  the owner rule allows only while the game is not running): exit 0, 35 PASS lines, 0 FAIL, including the batch 4 HTTP
  checks (`/recordings`, guards, remote-peer refusal, JSON schemas). Log: `%TEMP%\wt_b5_harness.log`.

## 7. Open items and doubts

- OPEN, live: name format in the real game (country, mode, session, weekend folder), Test Day naming, CO-OP label.
- OPEN, live: close the game normally during a session; expect a closed `.f1t` and no `recovered` row on next start.
  If a `.part` remains after a normal exit, the process ended without DETACH (for example `TerminateProcess`), and T3 is
  only partly fixed.
- OPEN, live: decision B. After a repair, the `.damaged` original should be in the Recycle Bin, and the list should show
  the recovered file only.
- DERIVED: the new DETACH close calls the multiplayer export from the capture side (inside `g_telemetry_cs`). It is a
  getter, so no lock is expected; to be confirmed in the live run.
- DEFECT, SDK (not fixed here): `tools/f1c_mod.py:1549` prints `Build verified:` with the path of the old `b4` DLL,
  while the build wrote `b5`. The b5 output was checked by hash and by the payload gate, not by that line.
- Unchanged: `sa_name_ok` (API id rules), schemas, old recordings.
- Workspace validator: 2 pre-existing errors (`.claude`, `plans` top-level directories). Not changed.
- Documentation gate: this record is indexed from `CURRENT_STATE.md` (newest entry). Findings appended to
  `f1c_ida_decompiled/findings.jsonl` (ids `sv2-year-prefix-letter-forms-20261013`,
  `web-telemetry-exit-no-stop-signal-20261013`, `web-telemetry-exit-detach-order-20261013`,
  `web-telemetry-recovered-bin-move-20261013`, `web-telemetry-replay-names-20261013`).

## Live result + follow-up (Claude, 2026-10-13)
- OBSERVED live (owner screenshot, API, log): new names work (`PACIFIC 1995 - Race - Practice 1 - 2026-10-13 17-00_2`);
  quitting the game inside P2 logged `storage: session closed at process exit` and P2 is `closed` (T3 fix works);
  recovery moved the 16-20 `.f1t.part.damaged` to the Recycle Bin (decision B works).
- Owner wants newest first (like replays): Recordings list was in folder (A-Z) order. Viewer now sorts recordings by the
  date+time inside the id, newest first (`telemetry_viewer.js` load handler; before-state `.BEFORE_20261013_recsort`).
  Viewer test 415 PASS; `pin_sidecars.py` re-pinned; rebuilt (no TELEMETRY_TESTS, game running):
  `%TEMP%\f1c-web-telemetry-1.1.0-b5s\payload` web_telemetry.dll 5D905955..., telemetry_viewer.js 66E84AC6...
- Cosmetic open: session ids keep the `_2` uniqueness suffix in the sub-line.

## Owner live B3-B5 (2026-10-13, build 5D905955 / viewer 66E84AC6), checked by Claude via /recordings, /recordings/jobs
- B3 PARTIAL: delete of the loaded recording did not happen (correct) but the page shows NO message (owner). FAIL on feedback.
- B4: archive-1 (16-02_2, 32,120 -> 28,035 B) and archive-2 (16-03_7, 3,324 -> 3,103 B) ok; `.f1a` present on disk,
  `archived_count` 2. Both jobs had recycle_working=true (owner pressed OK on the second confirm both times), so the
  keep-working-copy path was not exercised live.
- BUG (OBSERVED): job `moved` lists the `.f1a` name (e.g. `Unknown_0000_2026-10-13_16-02_2.f1a`) while the file moved to
  the bin is the working `.f1t` (bytes_moved = working size; `.f1a` still on disk). Misleading report text.
- B5: delete-3 moved `Unknown_0000_2026-10-13_16-04_3.f1t` (98,279 B) to the Recycle Bin; restore pending.
- UX (owner): archive/delete use browser confirm() popups (localized "Viesti osoitteesta 127.0.0.1:8080", OK/Peru);
  owner wants in-GUI dialogs. Second dialog's OK = destructive choice (recycle) is easy to hit by mistake.

## Batch 6: Recordings UX fixes

Scope: the four owner issues from the live B3-B5 check (section above): in-page dialogs, one archive dialog with a choice,
visible refusal and job-failure feedback, and the job report names. Stage: implement and verify (offline). Not installed, not
live-tested, not promoted. No deploy. `governance\PRODUCT_STATUS.json` not changed.

Backups (before the first edit, `cp -pn`, suffix `.BEFORE_20261013_recux`): `telemetry_viewer.js`, `storage_jobs.inc`,
`storage_api.inc` (not edited), `viewer_test.js` (not edited), `runtime_test.c`, `web_telemetry.c` (not edited),
`STORAGE_V2_BATCH5_RECORD_2026-10-13.md`, `CURRENT_STATE.md`.

Game state: `tasklist` showed `F1 Challenge Online.exe` running at the start and at the end (PID changed from 25256 to 39688
during the task). So no `runtime_test.exe`, no `TELEMETRY_TESTS=1`, no fuzz. Nothing was written into `F1 Challenge V10` or
`Documents\F1 Challenge V10`.

### 1. In-page dialogs (`telemetry_viewer.js`)
- New `Viewer.prototype.dialog(opts)`: modal in the existing `.tv-modal` style with the existing `tv-btn` classes and a small
  CSS addition (`.tv-dlg-opt`, `.tv-dlg-btns`). Returns a promise: `{ value, choice }`, or `null` for Cancel, Esc and a click
  outside the box.
- Keyboard: Esc = Cancel; Enter = the safe button (Archive for the archive dialog, Cancel for delete); Tab is kept inside the
  dialog; page keys (1-8, D, R, A, B, Esc zoom) are blocked while it is open (window capture listener).
- Replaced: `window.confirm` in `Console.prototype.archiveSelected` (2 confirms become 1 dialog) and `deleteItem`.
- Grep of all viewer and embedded page sources for `confirm(`, `alert(`, `prompt(`:
  - `telemetry_viewer.js` line 1210 `window.confirm('Import "..." into side A?')`: the Viewer trace import, NOT the Recordings
    page. Left unchanged (other page). Still a browser popup on the Viewer page.
  - `web_telemetry.c` line 7925 `alert('Action failed: ...')`: inside the `#if 0` block at lines 6059-7982 (retired `/classic`
    source, compiled out). Not reachable. Left unchanged.
  - `workbench_ui.js`: none. `web_telemetry.c` other hits are comments ("confirmed").

### 2. Archive and delete dialogs
- Archive: ONE dialog. Text: the session count, recording names, working size, and the compact-format sentence (existing
  wording). Radios: "Keep the working copy (default)" (checked) and "Move the working copy to the Recycle Bin". Buttons:
  "Archive" (safe, focused) and "Cancel". Posts `recycle_working` from the radio.
- Delete: same modal, existing confirm text, buttons "Move to Recycle Bin" and "Cancel" (Cancel focused, Enter = Cancel).
- Added an "Unload" button next to the loaded recording (the server's 409 text says "Load an empty recording name to unload
  it", and the page had no way to do that). Not in the task list; small, and it makes the refusal actionable.

### 3. Refusal and job-failure feedback (`telemetry_viewer.js`)
- Server answers (C handler, `storage_api.inc` `sa_err`, `web_telemetry.c` `http_send_json_error`): `{"error": "<message>"}`
  with 400, 403, 404, 409, 413, 415, 422, 503. Example: delete of a loaded recording gives 409 "The recording is loaded...".
- FACT (code): the old page passed these only to `Viewer.prototype.toast`, which disappears after 3.2 s. That is the most
  likely reason owner test B3 showed no message.
- Now: `Console.prototype.setNotice` keeps the last refusal (red, text "Refused (HTTP 409): ...") or success line (green) in
  the page until the next action or **Dismiss**. The toast is still shown.
- Job failures: "Last job" line is red when the newest job finished with `result: error`. The newest job is picked by its
  number (`archive-N` / `delete-N`), not by array position.
- Network failure (no answer) shows the existing "No answer from the game PC. Nothing was changed." as the notice.

### 4. Job report names (`storage_jobs.inc`, `sj_run_archive` and `sj_run`)
- FACT (owner live B4, `archive-1` 16-02_2 and `archive-2` 16-03_7, both with recycle_working=true): `moved` listed the
  `.f1a` name, while the file moved to the bin was the working `.f1t` (bytes_moved = working size; `.f1a` stayed on disk).
- Fix (archive): `.f1a` is no longer added to `moved`. The working `.f1t` name is added to `moved` only when the bin move
  succeeded, and to `not_moved` when it failed. Without recycle, `moved` stays empty (the archive is created, not moved).
- Fix (job line): a successful archive job now has a message, "archived N session(s); working copy kept" or "... moved to
  the Recycle Bin" (before: empty message).
- Delete job: `moved` now lists only items that existed before the job and are gone after it (before: items already
  missing were listed as moved). Recording delete still lists the file names, then the recording folder name (existing test).

### Tests
Run:
- `node viewer_test.js telemetry_viewer.js workbench_ui.js`: PASS, 415 checks. No check asserted confirm() use, so no check
  was changed.
- Offline browser harness (not in the workspace): a copy of `telemetry_viewer.js` and `workbench_ui.js` in
  `%TEMP%\recux\www`, served from 127.0.0.1 with `harness.html`, with fetch stubbed (no game, no server). 25 checks PASS:
  dialog focus and defaults, Enter/Esc/backdrop/click results, page keys blocked while open, archive posts
  `recycle_working` true/false, delete Enter = no post, delete confirmed posts `recording` and `confirm_bytes`, a 409 shows an
  inline red notice, job failure selection (newest job, not_moved), and a mounted console showing the notice, Dismiss,
  Unload and the red job line. Note: this harness did not use the live server or real key presses on a real page.
- Gated build without `TELEMETRY_TESTS`: `build.bat` path via `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry`
  with `TELEMETRY_BUILD_DIR` set to `%TEMP%\f1c-web-telemetry-1.1.0-b6`: rc 0, `-Wall -Wextra -Werror` clean, payload gate OK
  (5 files). The build's `Build verified:` line printed the OLD `...-b4` path (known SDK defect, recorded in Batch 5 section 7).
  Verified by hash instead (below).
- `runtime_test.c`: syntax check only, `gcc -fsyntax-only -std=c11 -Wall -Wextra -Werror` (with and without `-DTELEMETRY_TEST`):
  PASS. NOT run (game running).
- `pin_sidecars.py` (after the JS edit) and `pin_sidecars.py --payload <b6 build dir>`: pins updated; `verify_payload.py` on
  the b6 folder: payload gate OK.

Not run, and why:
- `runtime_test.exe` and the unit test added for the job report (section "job report batch 6" in `runtime_test.c`, after
  `j3_delete_cases`): game running. The test is written, compiles, and has not been executed.
- `TELEMETRY_TESTS=1` gate and `test_telemetry.py` (it starts `runtime_test.exe`): game running.
- Fuzz: game running.
- Live checks of the dialogs, the notices, Unload, the job names and the recycle archive: need the owner in the game.
- `python -m pytest`: not run (see section 6 of the Batch 5 record: the file is a script, not a pytest module).
- No install and no deploy.

### Hashes (SHA-256, build `%TEMP%\f1c-web-telemetry-1.1.0-b6`, checked by hand)

| File | SHA-256 |
|---|---|
| `web_telemetry.dll` | `B2661C9645CA538BD2C5BDAE3E9936C0AD9522EA92C416DCE419F0FD783E0E43` |
| `telemetry_viewer.js` | `37F87A6F62CC3E387744302ED88876BBB197524FB1B1F444738A40F4B6F6004D` |
| `workbench_ui.js` (unchanged) | `E72BDD77C66FFC0809D2EB734FF63AD2F59A368F2D3DC223F5D98C3C0EB808F8` |
| `web_telemetry.ini` (unchanged) | `CFA194343895644FE7E4C95792AFE5AA7627F6AB97D561F7219451E7785E2D01` |
| `web_telemetry.dll.mod.json` | `FEAC85BE37D35A7EC78EAE52BA9B735DC1890AAB7CACCF4188150C0B3B914706` |

Payload folder: `%USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-b6\payload` (5 files; the same hashes). Staged only,
not installed. Install files when the owner approves: `web_telemetry.dll`, `telemetry_viewer.js`, `workbench_ui.js`,
`web_telemetry.ini`, `web_telemetry.dll.mod.json`.

### Doubts and open items
- LIVE, to check: the dialogs (keyboard and mouse), the red notice after a 409, the Unload button, the archive job line
  "working copy kept" / "moved to the Recycle Bin", and that `moved` lists the `.f1t` after a recycle archive.
- UNTESTED in the C unit test: the recycle archive success path and the not_moved path in the job report (the test is written
  but not run). The b4 `"not_moved":[]` and delete `"moved":["S5.f1t","Rec_B"]` checks are unchanged and not re-run.
- The viewer's trace import still uses `confirm()` (line 1210). It is not on the Recordings page; left as is, per the task.
- The "Build verified" line still names the b4 path (SDK defect, not fixed here).
- Pending: owner decision on whether the Viewer import confirm should move to the same dialog.
