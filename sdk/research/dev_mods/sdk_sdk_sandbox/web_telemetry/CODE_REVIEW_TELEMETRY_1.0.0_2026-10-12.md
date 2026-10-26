# Telemetry 1.0.0 source review (Claude, 2026-10-12 evening)

Owner asked for a bug/issue review of the shipped Telemetry 1.0.0 (engine 4.4.0, API 6).
Static review of `web_telemetry.c`, `compatibility_runtime.inc`, parts of `telemetry_viewer.js`,
plus read-only inspection of the owner's installed copy (`Documents\F1 Challenge V10\plugins\lightloader`)
and a read-only GET of the live catalog. No game was running. No deploy.

## Facts checked
- Live catalog `https://mods.palloset.com/lightloader/api/catalog.json` = SHA256 AC7D5D73… (the shipped
  catalog). Entries: mod `web_telemetry` "Telemetry" 1.0.0 and tool "Telemetry" 1.0.0. Correct.
- Shipped ZIP `web_telemetry.dll.mod.json` says "Telemetry" 1.0.0. Correct.
- Owner's installed `web_telemetry.dll.mod.json` is the dev hand-install: "Web Telemetry" 4.4.0
  (DLL hash 312A6D82… = shipped DLL).
- Manager card "Web Telemetry (private compatibility test) v4.1.0-rc.2" (owner screenshot): not found in the
  live catalog, shipped files, workspace text files, manager app data or Lightloader binaries.
  Source UNRESOLVED (probably another PC with the old private build; manager shown in light theme while the
  owner's manager.ini has Dark=1 — inference).

## Findings (verified by reading code / local files)
1. **No durable review data (owner's main complaint, CONFIRMED).** All console data (lap table, traces,
   pits, incidents, weather) live in memory only and are wiped on every session boundary:
   broker generation change, session type change, car count change, session clock going back, leader lap
   going back (`session_reset_check`, `session_restart_check`, `trace_capture_epoch`). Owner's log:
   666 + 264 + 178 + 124 + 69 clears. After qualifying ends, the page has nothing for that session.
   Only durable output: `plugins\lightloader\web_telemetry_race_*.jsonl` (lap times, sectors, fuel, wear,
   pit stops, incidents; no distance traces). The console cannot open these files.
2. **Race journal file explosion.** A new JSONL per session reset, never pruned: 798 files / 91 MB in the
   owner's plugins folder since 2026-10-09, several per minute at session start. `web_telemetry_log.txt`
   also grows forever (1.75 MB; one diagnostic line per 10 s).
3. **Public branding wrong in the page.** Footer "Web Telemetry 4.4.0", brand "F1C TELEMETRY 4.4.0",
   missing-sidecar text cites PRIVATE_TEST_README. Shipped by design (no rebuild at promotion).
4. **Trace store memory (32-bit game).** 3.58 MB per car (7 laps x 3200 x 160 B); 24 cars about 86 MB.
   Growth allocates the new block before freeing the old (peak about 2x). If growth fails, the old store stays
   allocated but mode drops to "selected", so every recorded lap becomes unreachable (`trace_store_ensure`).
   Plus 5 static 2 MB HTTP buffers and 7.2 MB static `g_trace`.
5. `/events` sends `Access-Control-Allow-Origin: *`: any web page open in the browser can read the live feed
   from localhost. Low impact (game data only).
6. Startup log always says "remote Tailscale/LAN enabled" even with Remote=0 (`compatibility_runtime.inc:184`).
7. Test harness writes `web_telemetry_race_*.jsonl` into the current directory (8 in workspace root, 40+ in
   this folder): workspace bloat against the lean-workspace rule.
8. Single-threaded HTTP: one slow client holds the server up to 5 s (+3 s body). Low.

Finding 1 explains it if his game had left qualifying; an old private build (finding above) is another
candidate. Needs his `web_telemetry_log.txt` and version.

## Changes made (source only, NOT built, NOT released)
Before-state: `workspace_organization_audit/TELEMETRY_101_BEFORE_2026-10-12/` (SHA256SUMS inside).
- `telemetry_viewer.js`: `PRODUCT`/`PRODUCT_VERSION` ("Telemetry", "1.0.1"); footer + brand use them;
  "runs Web Telemetry too" -> "runs Telemetry too". `VERSION` 4.4.0 unchanged (engine/API identity).
- `web_telemetry.c`: `PRODUCT_LABEL "Telemetry 1.0.1"`; generator meta and missing-sidecar text use it.
- `web_telemetry.dll.mod.json`: name "Telemetry", version 1.0.1.
(Superseded the same evening by the full 1.0.1 candidate below.)

## Telemetry 1.0.1 candidate (owner: "do all these fixes"; accumulate data for live and post-session review)

Owner requests added during the work: a data manager to delete old data; race pace showing
qualifying times (owner screenshot during a race).

### What changed (source, this directory)
- **Session archive** (`session_archive.inc`, new): every session is written to
  `plugins\lightloader\telemetry_data\<id>\` while it runs: `traces.bin` (every completed trace lap of
  every car, all channels except RESEARCH rx), `laps.bin`, `pits.bin`, `incidents.bin`, `journal.jsonl`,
  `frame.json` + `weather.json` (every 30 s and at close), `track.json`, `meta.json`. A folder is created
  only after the first completed lap. Key = broker generation + session type + track; a clock restart starts a
  new session. All file IO on the worker thread outside `g_telemetry_cs`; HTTP reads/deletes under an SRW lock.
- **Review mode**: `/?session=<id>` opens a saved (or the recording) session in the same console; every
  console endpoint accepts `session=` and answers from disk; the live feed is not connected; a side loads every
  recorded lap (up to 200).
- **Live**: a lap the 6-lap memory ring evicted is served from the current session's disk archive.
- **Sessions & Data Manager page** (menu group Data): list (start, session, track, length, drivers, laps,
  traced laps, size, state), Open, Delete, Delete selected, delete the old 1.0.0 journal files, storage total.
  `POST /sessions/delete` only from the game PC (loopback peer), custom header + octet-stream (CORS preflight
  blocks foreign pages), the recording session refused. `[Mod] MaxStorageMB=3000` (0 = no cap) removes the
  oldest finished sessions automatically.
- **Mid-session wipes fixed** (findings `WEB-TELEMETRY-HISTORY-WIPES-MIDSESSION-20261012`): only a new broker
  generation clears history; car count change no longer resets; leader-lap rule race-only; a slot whose driver
  changes restarts only its own laps.
- **Race pace lap times fixed** (finding `WEB-TELEMETRY-LASTLAP-LAGS-LAPCOUNTER-20261012`): lap recorded when the
  last-lap value actually changes; unknown (`-`) after 8 s instead of the previous lap's time.
- Journal no longer one loose file per reset beside the DLL (goes into the session folder).
- Log rotates at 2 MB (`web_telemetry_log.old.txt`).
- Trace store growth failure keeps the existing store (laps stay reachable).
- One shared 2 MB HTTP buffer instead of five (8 MB less address space).
- `/events` no longer sends `Access-Control-Allow-Origin: *`.
- Startup log states Remote on/off correctly and the archive folder/cap.
- Branding: page shows "Telemetry 1.0.1"; engine/API identity stays 4.4.0; `api_version` 7.
- Schemas: `archive` property, `mode`/`src`/`source` value `archive`, health `product` + `archive`;
  `driver.json` gains `s2d` (emitted since rc.2 but missing from the schema: pre-existing contract gap).
- Test harness: archive off unless a test enables it, so no `web_telemetry_race_*.jsonl` in the CWD; serve
  harness seeds one archived session in %TEMP% and removes it.

### Verification (offline, 2026-10-12, game not running)
- Full gate `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry` with `TELEMETRY_TESTS=1`: exit 0,
  46,449 runtime checks (new: lap latch, reset rules, growth keeps store, archive end to end incl. rollover,
  restart, no-lap no-folder, delete guards, retention cap, legacy journals, live fallback), viewer 415 checks,
  contract api_version 7 incl. schema validation of review documents, HTTP audit, soak 20 x 2 h, fuzz 80,000.
- Browser (built-in pane, synthetic serve harness): Data Manager lists the archived session; Open switches to
  REVIEW with "Saved session ... back to live"; archived lap and race pace 1:23.250 shown; only error was
  `/track?session=` 404 because the synthetic session has no track (real sessions write track.json).
- Payload `%TEMP%\f1c-web-telemetry-1.0.1\payload`: DLL 2E6B72F1..., viewer 05EECF43..., workbench E72BDD77...
  (unchanged), ini (adds MaxStorageMB), mod.json (Telemetry 1.0.1). Exact hashes: VERIFICATION_MANIFEST.json.

### Not verified / limitations
- Not run in the game. Needs a live session: archive folder appears after the first lap, race pace times correct
  at race start, review of a finished session, delete from the page, CPU/frame cost of the disk writes.
- Disk use: about 170 KB per car-lap (24 cars x 60 laps about 250 MB); cap default 3 GB.
- Review loads every lap of a side into the browser (heavy over Tailscale for long races).
- RESEARCH rx channels are not archived.
- Friend's missing qualifying laps: cause not established for his PC (no log); the wipe bugs above are the likely
  cause. The "Web Telemetry (private compatibility test) v4.1.0-rc.2" manager card comes from the
  `web_telemetry.dll.mod.json` of the old private rc builds (copies in `F1 Challenge V10\WebTelemetryBackups\` and
  `workspace_organization_audit\WEB_TELEMETRY_*_INSTALL_*`). Not in the live catalog or the 1.0.0 ZIP. The PC that
  shows it still has a hand-installed private rc build (fact: label source; inference: that the screenshot PC is the friend's).
- HTTP stays single-threaded (slow client can hold it up to 5 s): unchanged, low.

### Next

## 1.0.0 release blocker found after a player report (2026-10-12, late)

Player screenshot: "Web Telemetry 4.4.0: telemetry_viewer.js or workbench_ui.js is missing or outdated beside the
DLL". Cause (CONFIRMED from code): `LightloaderManager.cs` `InstallPackage` (development/lightloader/
v2.0.0-hub-dev.1/manager_cs, line 2061) installs only `.dll`, `.asi`, `.ini`, readme and audio assets from a
package and skips every other file. The 1.0.0 ZIP carries the two scripts, so every manager install of 1.0.0
lacks them; the owner's PC works only because it was hand-installed. The 1.0.0 release gates checked the catalog
and ZIP hashes, not a manager install into a game folder. A stale copy from an older private build beside the DLL
gives the same message ("outdated").

Fix in the 1.0.1 candidate: both scripts are built into the DLL (`.incbin` from this directory at build time);
files beside the DLL are not read any more. `/health` still reports the pinned hashes (present/matches true).
New gate check: no `workbench_ui.js` and a stale `telemetry_viewer.js` beside the DLL still serve the pinned
built-in scripts. Full gate after the change: exit 0. DLL grows to about 1.17 MB.

Player workaround for 1.0.0: copy `telemetry_viewer.js` and `workbench_ui.js` from the Telemetry ZIP into
`plugins\lightloader`, replacing older copies.

Lightloader follow-up (not done): the manager ignores the `sidecars` list in mod metadata; either install declared
sidecars or keep the rule that mods must be single-DLL. Release gates must include a real manager install.

### Correction and release (2026-10-12)
- Correction (CONFIRMED by calling the real managers' InstallPackage on a temp game folder): Lightloader **2.3.16
  installs** the .js files; **2.3.15 and older skip** them. The rule quoted above (line 2061) is in the dev
  `manager_cs\` UI-redesign source, not in the 2.3.16 release source. So 1.0.0 is broken for players who have not
  updated Lightloader to 2.3.16 (or who hand-installed stale files). Finding: `LL-MANAGER-SKIPS-NON-DLL-PACKAGE-FILES-20261012-CORRECTION`.
- Shipped as Telemetry 1.0.1 on owner order: release record
  `development/lightloader/v2.0.0-hub-dev.1/release_telemetry_1.0.1_2026-10-12/README.md` (DLL CE8228AD..., ZIP
  427392FC..., catalog 8F03867F..., externally verified). Owner live check still pending.
