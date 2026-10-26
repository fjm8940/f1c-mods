# Telemetry Player (phase 2): play recordings with the game closed (SPEC r1, 2026-10-14)

Author: Matias. Spec by Claude; implementation later by Haiku workers, Claude reviews each batch.
Product: Telemetry (`web_telemetry`), current dev 1.2.0 (unreleased), API 10. Stage: **specify** (spec only, no code
changed). Rules cited: `development\mods\MOD_STANDARD.md` (R11 dev tools behind a flag, section 5 versions: any shipped
byte change = new version). MOD_STANDARD section 5 names the author `Matias`; the owner rule (author always Matias)
wins here.
Tags: FACT = read in source or records during this spec (file:line). INFERENCE = reasoned, not checked. OPEN = owner.

## 0. Owner request and decisions
- D1 (owner, 2026-10-14): phase 1 = every Telemetry tab plays past recordings while the game is open (built and
  verified offline, [TAPE_CLAUDE_REVIEW_2026-10-14.md](TAPE_CLAUDE_REVIEW_2026-10-14.md); full gate pending with the
  game closed). Phase 2 = the same playback **with F1 Challenge fully closed**, via a standalone player. This spec.
- Goal: the owner (and players) open the Telemetry console and play back any storage-v2 recording (`.f1t`, archived
  `.f1a`) with no game process running.
- Non-goals: live telemetry in the player; TV Director; recording, repairing, archiving or deleting recordings;
  legacy 1.0.1 `telemetry_data` sessions and pre-2.1 files (already refused, owner Q3); remote/LAN access; any change
  to the in-game DLL's behaviour or served bytes other than the shared viewer changes in section 5.
- Prerequisite: phase 1 full gate (TAPE review section 6 item 1) passes first. The player reuses phase 1's routes;
  building on an ungated base would mix two sets of failures.

## 1. What exists (FACT)
- `runtime_test.c` `#define TELEMETRY_TEST 1` then `#include "web_telemetry.c"` (lines 2-4). Its `serve` mode
  (`runtime_test.c:5144`) runs **after the whole test suite**, calls `serve_real_recording()` (`:4700`), which with
  `RECREV_ROOT` calls `sw_root_init(root, 6, 2, ...)`, `sa_init()`, `sw_scan_folders()`, `sa_snapshot_rebuild()`
  (`:4706-4718`), then starts `http_thread` and feeds a synthetic advancing live frame for 120 s (1800 s with
  `TELEMETRY_PREVIEW`). TAPE review section 3: this served the owner's 7 real recordings end to end, offline. So the
  server, storage reader and embedded viewer run without the game. It is a test harness, not a product: it runs the
  suite first, feeds fake live data, stops after a fixed time, and links `f1c_broker.c`.
- The HTTP listener binds `htonl(INADDR_LOOPBACK)` always (`web_telemetry.c:9800`); `Remote=1` adds a second listener
  on a private `[Mod] BindAddress` only (`http_bind_resolve` `:9757`). Port fallback: configured port + up to 5
  (`http_listen_open` `:9787`). `http_thread` (`:9885`) loads settings from the ini beside the module
  (`settings_module_ini_path` `:3875`, uses `telemetry_module`; with `NULL` module that is the exe folder).
- The viewer and workbench are embedded at compile time (`.incbin`, TAPE review section 5): an exe built from the
  same sources serves the same page bytes.
- Game-coupled code (must not run in the player):
  - `compatibility_runtime.inc` (included at `web_telemetry.c:9990`): `DllMain`, Lightloader plug-in activation,
    broker discovery and registration, the capture worker (`:150-200`, `:320-360`).
  - `director_state.inc` (`web_telemetry.c:8088`): reads the game's camera state; POST `/director/*`
    (`web_telemetry.c:9007-9104`, `:9332-9340`, `:9418`).
  - `/events` SSE (`web_telemetry.c:9509`) and `/telemetry` live frame (no live source in the player).
- Game-folder anchors that assume the host exe is the game (wrong in a separate exe):
  `sw_root_init_from_exe` (`storage_writer.inc:458-469`), the AIW read (`web_telemetry.c:4391`), tyre files
  `tyre_game_path` (`web_telemetry.c:5563`). All use `GetModuleFileNameA(NULL)`.
- Disk writes in the storage code (the player must reach none of them):
  - `sw_root_init` **creates** `<game>\Telemetry\` and `recordings\` (`storage_writer.inc:443`, `:449`).
  - `sw_index_write` rewrites `index.json` (`storage_writer.inc:561-576`, called from the writer thread).
  - Jobs thread `sj_start` (`storage_jobs.inc:731`): startup repair of `.part` files (`:232-249`), archive (`:362-431`),
    Recycle Bin delete via `SHFileOperationW` (`:451`). POST `/recordings/load|archive|delete`
    (`web_telemetry.c:9252`, `storage_api.inc:2069-2232`).
  - `session_archive.inc` (1.0.1 archive, superseded, not initialized in 1.1.0+).
- Review reads open recordings with `GENERIC_READ` and `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`
  (`storage_api.inc:621`, `:900`): a reader never blocks the game's writer rename or delete.
- Lightloader Tools tab (`development\lightloader\v2.0.0-hub-dev.1\manager_cs\Tools.cs`): a catalog `tools` entry
  installs a zip into `<game>\tools\<folder>\`, entry must match `^[A-Za-z0-9_\-]{1,60}\.(bat|cmd|exe)$` (`:39`),
  install refuses while the game runs (`:102`), install runs `<entry> --install` and waits (`:183`), Open runs the
  entry with no arguments, working dir = tool folder (`:212-217`); the Open status text is fixed:
  "`<name>` is starting. Then start the game as usual." (`ManagerUi.cs:2241`).
- The live catalog already has the tool `web_telemetry_tool` 1.0.0, name "Telemetry", folder `web_telemetry`, entry
  `WebTelemetry.bat` (catalog copy in `release_telemetry_1.2.0_2026-10-14\catalog.before.json`). The package holds only
  `WebTelemetry.bat` + `VERSION`, no DLL (`release_2.3.26_2026-10-14\RELEASE_RECORD.md` section 8). The bat reads
  `Port` from `..\..\plugins\lightloader\web_telemetry.ini` and opens `http://127.0.0.1:<port>/`
  (`release_webtel_4.4.0_2026-10-12\assets\WebTelemetry.bat`). Changelog: `operations\release_management\changelogs\
  web_telemetry_tool.json`.

## 2. Build target
- New source `player_main.c` in the component dir: `#define TELEMETRY_PLAYER 1`, then `#include "web_telemetry.c"`,
  then `int main(int argc, char **argv)`. Same pattern as `runtime_test.c`; no second copy of any server code.
- Output `TelemetryPlayer.exe` (name OPEN, O1), 32-bit, built by `build.bat` right after the DLL with the DLL's flags:
  `C:\msys64\mingw32\bin\gcc.exe -O2 -static -static-libgcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function
  -Wl,--no-insert-timestamp`, sources `player_main.c legacy_formats.c sha256.c`, libs `-lws2_32 -lkernel32`
  (+ `-lshell32` only for `ShellExecuteW` to open the browser). **No `f1c_broker.c`.** Subsystem console (O3).
  Output into `%TELEMETRY_BUILD_DIR%\player\` and the payload stage; never the component dir.
- `#ifdef TELEMETRY_PLAYER` guards (each guard listed in the worker record with file:line):
  1. `compatibility_runtime.inc` not included (no `DllMain`, broker, capture worker, plug-in ABI exports).
  2. `director_state.inc` not included; every `/director/*` route answers 404
     `{"error":"the TV Director needs the game"}`.
  3. `storage_jobs.inc` and the writer thread (`sw_start`, `sw_index_write`, `sw_close_session`) not compiled;
     `session_archive.inc` not compiled.
  4. POST `/recordings/load|archive|delete` answer 403 `{"error":"read-only player: open the game to manage recordings"}`
     before any storage call. `GET /recordings/jobs` answers an empty job list.
  5. `/events` answers 404 `{"error":"no live game in the player"}`; `/telemetry` (live) the same.
  6. A `wt_game_dir()` helper replaces the three `GetModuleFileNameA(NULL)` anchors (section 3). DLL build: unchanged
     result (exe folder). Player: the resolved game folder, read-only.
- If a guard would need more than a few lines in a shared function, stop and report (abort A-2) instead of forking
  the function.
- No game memory: the player never calls `OpenProcess`, `ReadProcessMemory`, `WriteProcessMemory`,
  `VirtualProtect*`, `CreateRemoteThread`, never loads `dinput8.dll` or the broker. Proved by test P-2.
- Version identity: the exe reports the same product version as the DLL built from the same sources
  (`TELEMETRY_VERSION` string) plus `"host":"player"` (section 5). Builds are reproducible (two builds, same hash).

## 3. Finding the recordings folder (game install, read-only)
Resolution order, first hit wins; each candidate must contain `Telemetry\recordings\` as a directory:
1. Command line `--game "<folder>"`.
2. `telemetry_player.ini` beside the exe, `[Player] GameFolder=<folder>`.
3. Exe location: when the exe sits in `<X>\tools\web_telemetry\` (the Lightloader Tools install, section 6), `X`.
4. None: the player still starts and the console shows "No recordings folder found. Install the player from
   Lightloader > Tools, or set GameFolder in telemetry_player.ini." with the paths it tried. It exits only when the
   window is closed.
- New `sw_root_open_ro(game_dir, why, cap)`: fills `sw_dir`, `sw_rec_dir`, `sw_index_path` like `sw_root_init` but
  **never calls `CreateDirectoryA`**; a missing folder is a refusal with a reason. The player calls only this.
  `sw_os_ok` (Windows 8+) still applies.
- After the root is set: `sa_init()`, `sw_scan_folders()`, `sa_snapshot_rebuild()` (the `serve_real_recording`
  sequence), plus a rescan when `/recordings` is requested and the folder's last-write time changed (new races
  recorded while the player runs).
- Unfinished `.part` files: the player cannot tell "open in a running game" from "left by a crash". It lists them as
  "Unfinished (game running, or the game closed without saving)" and does not play them (O9). It never repairs them
  (repair stays the game DLL's job at its next start).
- `index.json` is never written. The player trusts the folder scan, not `index.json`.
- Track outline comes from the recording (`/replay/info` outline, phase 1), so no AIW is needed. Track/tyre name
  lookups through `wt_game_dir()` are reads only; on failure the existing fallbacks apply.

## 4. Port and ini
- File `telemetry_player.ini` beside the exe, created by the package, never by the exe:
  ```
  [Player]
  ; Port for the player page (this PC only). The game's own Telemetry page uses 8080.
  Port=8090
  ; GameFolder= (optional) the F1 Challenge folder that holds Telemetry\recordings
  ;GameFolder=C:\Games\F1 Challenge V10
  ; OpenBrowser=1 opens the page when the player starts.
  OpenBrowser=1
  ```
- Default port 8090 (O4), range and fallback as the DLL (`http_listen_open`, +5). The bound port is printed in the
  console window and used for the browser URL.
- `[Mod] Remote` and `BindAddress` are not read in the player (loopback only, section 7).
- Single instance: named mutex `Local\F1CTelemetryPlayer`. A second start probes `GET /player` on 8090..8095 (and the
  ini port..+5), opens the browser on the first that answers `"host":"player"`, prints that, and exits 0.
- Catalog `keep: ["telemetry_player.ini"]` so updates keep the user's port and folder.
- Log: `telemetry_player_log.txt` beside the exe, capped (reuse the DLL log cap). Disposable per retention policy.

## 5. Viewer and API changes (shared with the DLL)
- New route `GET /player` -> `{"host":"player","version":"<v>","game_folder_found":true|false,"recordings":N}`,
  schema `schemas/player.json`. The DLL answers 404 (unchanged behaviour), so the viewer tells the two hosts apart.
  `health.json` stays frozen at API 9. API number: new route -> API 11, or fold into API 10 if 1.2.0 is still
  unreleased when this is built (O6).
- Viewer (`telemetry_viewer.js`): at boot fetch `/player`; on 200: start in PAST on the Recordings page, LIVE button
  disabled with the tooltip "Start the game for live telemetry", never open `/events`, hide TV Director and the
  Recordings archive/delete/load controls (show "Read-only player" instead), header label "Telemetry Player - game
  closed". On 404: unchanged.
- No change to the embedded bytes beyond these viewer edits; the DLL serves the same viewer and simply never sees
  `/player` = 200.

## 6. How the user launches it
Recommended (O2 option a): reuse the existing Tools entry `web_telemetry_tool` ("Telemetry"), no Lightloader code
change.
- Package `web_telemetry_tool-<v>.zip`: `TelemetryPlayer.exe`, `WebTelemetry.bat` (updated), `telemetry_player.ini`,
  `VERSION`, `README.txt`. Folder stays `web_telemetry`, entry stays `WebTelemetry.bat` (existing installs update in
  place; `InstalledVersion` reads `tools\web_telemetry\VERSION`).
- `WebTelemetry.bat` logic: `--install` -> exit 0 (unchanged). Otherwise: if the game's Telemetry answers on the ini
  port (`..\..\plugins\lightloader\web_telemetry.ini`, default 8080) -> open that page (live + PAST, phase 1); else
  start `TelemetryPlayer.exe` (it opens the browser itself). Probe with `curl.exe -s -m 1` (Windows 10 1803+ ships it)
  or PowerShell; worker records which and why.
- Known wart: Lightloader's Open status text says "Then start the game as usual." (`ManagerUi.cs:2241`). Fixing it
  needs a manager release (O2 option c).
- Alternatives for O2: (b) a separate catalog tool "Telemetry Player" with entry `TelemetryPlayer.exe`;
  (c) (a) plus a Lightloader change for per-tool status text; (d) a desktop shortcut created by `WebTelemetry.bat
  --shortcut` (no manager change; shortcut creation via PowerShell `WScript.Shell`).
- Console window text: "Telemetry Player <v> - recordings from <folder>. Page: http://127.0.0.1:<port>/ . Close this
  window to stop." Closing the window ends the server (console ctrl handler sets `g_http_running = FALSE`, waits
  for `http_thread`).

## 7. Security (loopback only)
- One listener, `127.0.0.1:<port>`, no second listener, no `0.0.0.0`, Remote settings ignored. Loopback binds do not
  trigger a Windows Firewall prompt (INFERENCE; owner test A7 confirms nothing is reachable from the LAN).
- Host allowlist (M3) on every route, as in the DLL (`127.0.0.1:<port>`, `localhost:<port>`): blocks DNS rebinding.
- No POST route changes anything on disk (section 2 guard 4). The legacy import POST (`/trace/import/legacy`) stays only
  if the worker proves it keeps data in memory only; otherwise 403 in the player.
- File access: recordings opened `GENERIC_READ` only; request text never becomes a path (review ids are the opaque
  `v2_` ids mapped by `sa_rid_t`, `storage_api.inc:96`). Writes only: the log beside the exe.
- No outbound network, no usage or crash upload in v1 (O7). Unsigned exe: SmartScreen/antivirus behaviour recorded in
  owner test A9 (INFERENCE: files downloaded by Lightloader carry no Mark-of-the-Web, so no SmartScreen prompt).

## 8. Release pipeline and MANIFEST impact
- Release folder `development\lightloader\v2.0.0-hub-dev.1\release_telemetry_<v>_<date>\` (never reuse a folder),
  same shape as `release_telemetry_1.2.0_2026-10-14` (`make_release.py`, `DEPLOY.ps1`, `ROLLBACK.ps1`,
  `validate_all.ps1`, `RELEASE_HASHES.json`, `READY.md`). New in it: the tool zip.
- Catalog: `tools` entry `web_telemetry_tool` version bump, description "Play your recorded sessions in the browser,
  even with the game closed. Live telemetry while you drive.", `keep: ["telemetry_player.ini"]`, package URL
  `https://mods.palloset.com/downloads/web_telemetry_tool/v<v>/web_telemetry_tool-<v>.zip` (must start with
  `https://<Core.Host>/` and end `.zip`, `Tools.cs` Parse). The `mods` entry `web_telemetry` changes only if the DLL
  also ships (O6). `validate_all.ps1`: every published manager must still read the catalog as VALID.
- Changelog first: `operations\release_management\changelogs\web_telemetry_tool.json` (new version entry), then the
  catalog merge (`merge_changelogs.py`).
- `VERIFICATION_MANIFEST.json` (component): new `player` artifact block (exe SHA-256, build flags, gate result, P-1..P-12
  PASS lines); before-copy first. `pin_sidecars.py`/`verify_payload.py` learn the player file so the payload pin
  covers it.
- Production copy: `production\mods\web_telemetry\v<v>\` gets the tool zip and `SHA256SUMS.txt` entry (where the
  1.0.0 tool zip already lives), only after owner acceptance.
- Deploy: owner-run by default (`DEPLOY.ps1 -Go`), agent only on explicit owner permission in that session.
- `governance\PRODUCT_STATUS.json`: update only when the player reaches a real stage change (built+gated, shipped).

## 9. Implementation batches (Haiku), in order
- **P-A server/exe** (files: `player_main.c` new, `web_telemetry.c` guards + `/player` + `wt_game_dir`,
  `storage_writer.inc` `sw_root_open_ro`, `storage_api.inc` POST refusal, `build.bat`, `runtime_test.c`,
  `test_player.py` new, `schemas/player.json`, `contract_validate.py`). Tests P-1..P-6, P-8, P-10, P-12.
- **P-B viewer** (`telemetry_viewer.js`, `viewer_test.js`, `viewer_review/mock_feed.js`). Test P-7.
- **P-C packaging** (after owner decisions O1-O6): `WebTelemetry.bat`, `telemetry_player.ini`, README, release folder
  scripts, changelog. Test P-11. Claude reviews each batch; full gate after P-B and after P-C.
- Before-copies of every edited existing file (`<file>.BEFORE_<date>_player`, component convention), worker record
  `PLAYER_<batch>_RECORD_<date>.md`, indexed in CURRENT_STATE.md.

## 10. Tests and gate
All test output to `%TEMP%`, never the component dir. Fixture: copies of real recordings under
`%TEMP%\RECREV_2026-10-14\game` (never the live game folder) plus the synthetic R1-A file.
| # | Test | Pass condition |
|---|---|---|
| P-1 | Build | exe compiles with `-Werror`; two builds give the same SHA-256 |
| P-2 | No game coupling | `objdump -p` import table has no `OpenProcess`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualProtect*`, `CreateRemoteThread`, `MoveFileEx*`, `DeleteFile*`, `CreateDirectory*`; strings have no `SHFileOperationW`, `f1c_broker`, `dinput8` |
| P-3 | Folder resolution | arg, ini, exe-relative (`<X>\tools\web_telemetry\`), not-found; the not-found case creates no folder |
| P-4 | Read-only | tree snapshot (names, sizes, mtimes, SHA-256) of the fixture before and after a run that hits every GET route for all fixture sessions and every refused POST: identical; also passes with the fixture marked read-only |
| P-5 | Loopback | the only listening socket of the process is `127.0.0.1:<port>` (`GetExtendedTcpTable`); `Remote=1`/`BindAddress` in the ini change nothing; `Host: evil.example` refused |
| P-6 | Same page | bytes of `/` and the viewer script from the player equal the DLL build's (same build run) |
| P-7 | Viewer | `viewer_test.js`: `/player` 200 -> PAST + Recordings, LIVE disabled, no `/events`, no archive/delete/TV Director; `/player` 404 -> today's behaviour |
| P-8 | Coexistence | player on 8090 and `runtime_test.exe serve` on 8080 at once both answer; port taken -> fallback; second player start opens the first and exits 0 |
| P-9 | Every page plays | serve-mode equivalent of TAPE review section 3 through the player: `/replay/info` for all 7 fixture sessions, 5 frames, track, pits, incidents, driver, trace/data |
| P-10 | Contract | `contract_validate.py` on `/player` and every review doc the player serves |
| P-11 | Launcher | `WebTelemetry.bat` on fake game folders: `--install` exit 0; live page up -> opens live; down -> starts player |
| P-12 | Speed | start to first page < 2 s; `/replay/frame` < 20 ms on the largest fixture file (phase 1 bound) |
- Gate: game closed (`tasklist | grep -i "F1 Challenge"` empty), PowerShell 7 `$env:TELEMETRY_TESTS='1'; .\build.bat`
  in the component dir builds DLL + runtime_test + player and runs `test_telemetry.py` and `test_player.py`. EXIT 0
  required. Game running -> stop and report, do not run the gate.
- Regression: the DLL's existing gate and `json-golden` must pass unchanged (guards must not change DLL output).

## 11. Owner acceptance tests (Matias, game CLOSED unless stated)
1. A1: Lightloader > Tools > Telemetry shows the new version; Update; Open. A console window and the browser open at
   `http://127.0.0.1:8090/`, header "Telemetry Player - game closed", Recordings page listed.
2. A2: Open "AUSTRALIA 1997 - CO-OP 20-16 / Race". Race, Timing, Focus, Engine, Chassis, Lap Analysis, Braking all
   show data; play, pause, scrub and 4x speed work on every page.
3. A3: LIVE is greyed with "Start the game for live telemetry"; Weather says it is not recorded.
4. A4: Recordings has no Archive/Delete (or they say "Read-only player"); the recordings folder dates in Explorer are
   unchanged afterwards.
5. A5: Open from Tools again while it runs: no second window, the browser opens the same page. Close the console
   window: the page stops answering.
6. A6 (game OPEN, optional): start the game while the player runs; the game's page on 8080 and the player on 8090 both
   work. Tools > Telemetry > Open while the game runs opens the game's page.
7. A7: from a phone or second PC on the same network, `http://<this PC's IP>:8090/` does NOT open.
8. A8: only if a shortcut is chosen (O2 d): the desktop shortcut starts the player.
9. A9: note any SmartScreen or antivirus warning on first start.

## 12. Risks and abort conditions
- A-1: any fixture file changes in P-4 -> stop, fix, rerun; never test against the live game folder.
- A-2: a guard needs a fork of a shared function or changes DLL output (json-golden or gate fails) -> stop, report.
- A-3: the exe needs the broker or game memory to serve a recording -> stop; the design assumption is wrong.
- A-4: P-2 finds a write or memory API linked in -> stop, find the caller, guard it.
- Risk: antivirus flags a new unsigned exe that listens on a socket (INFERENCE). Mitigation: loopback only, stable
  name, owner test A9; code signing is out of scope.
- Risk: a recording in progress (`.part`) is listed but not playable; documented in the UI text.

## 13. Restoration
Additive work: new files `player_main.c`, `test_player.py`, `schemas/player.json`, package files. Edited files keep
before-copies (`.BEFORE_<date>_player`). Removing the player = drop the build.bat lines and the guards; the DLL build
is proven unchanged by its gate and json-golden. Catalog rollback = the release folder's `ROLLBACK.ps1` restores the
previous tool entry (1.0.0 bat-only package).

## 14. Open owner decisions
- O1 Name: exe `TelemetryPlayer.exe`, page header "Telemetry Player"; Tools card name stays "Telemetry"? (recommended)
- O2 Launch: (a) existing Tools > Telemetry > Open decides live page vs player (recommended, no manager change);
  (b) separate Tools entry "Telemetry Player"; (c) (a) + Lightloader change for the "start the game" status text;
  (d) add a desktop shortcut option.
- O3 Window: visible console window that you close to stop (recommended) or hidden with a tray icon (more code).
- O4 Port: 8090 default (recommended, never clashes with the game's 8080) or reuse 8080.
- O5 Versions: tool version = Telemetry version built from the same sources (recommended), or independent tool
  numbering (1.0.0 -> 1.1.0).
- O6 Release order: ship 1.2.0 (phase 1, DLL) first and the player later with API 11, or ship both together
  (then `/player` folds into API 10).
- O7 Usage and crash reports from the player: off (recommended) or the same as the DLL.
- O8 Other game folders: `--game` and `GameFolder=` only (recommended) or a folder picker.
- O9 Unfinished `.part` files: listed, not playable (recommended), or read-only playback of their finished blocks.
- O10 Player allowed while the game runs (recommended yes, read-only, other port) or refuse to start.
- O11 Legacy 1.0.1 `telemetry_data` sessions: out of scope (recommended) or include.
