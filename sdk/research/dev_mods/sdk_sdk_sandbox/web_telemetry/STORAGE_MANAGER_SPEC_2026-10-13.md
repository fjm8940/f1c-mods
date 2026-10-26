# Telemetry 1.1.0 recordings manager: specification (DRAFT r2)

Date: 2026-10-13. Product: Telemetry (`web_telemetry`). Shipped: 1.0.1 (stage observe).
Dev candidate: 1.1.0 (API 8, not installed). Pipeline stage: **specify** (no code, no build, no gate, no live probe).
r2 = revision after owner answers of 2026-10-13 (changelog in section 13). Coordinator review pending.

Tags used below:
- `FACT file:line` = read in source and line-verified (r1 pass, plus coordinator check of the items it names).
- `FACT src` = verified in a named header or source file (`compressapi.h`, mingw32).
- `FACT-H` = stated in `HANDOFF_CLAUDE_2026-10-13.md`; not re-derived.
- `FACT-O` = owner statement.
- `DECISION` = owner-approved requirement.
- `PROPOSAL` = design choice here; needs coordinator or owner sign-off.
- `INFERENCE` = reasoned, not verified; must be checked before freeze.
- `ESTIMATE` = arithmetic from stated assumptions; to be measured.
- `NOT FOUND` = searched, not present in the workspace.

## 0. Context and rules

### 0.1 Workspace rules applied
- `AGENTS.md` (workspace): no hard deletes by agents. Here the owner has approved user-initiated deletion
  that goes to the Recycle Bin (section 7.8); the code never hard-deletes. UTF-8 without BOM. Before
  editing an existing governance or operational document, keep a `workspace_organization_audit\<phase>_BEFORE_<date>\` copy.
- Component `AGENTS.md`: `NOT FOUND` in `web_telemetry\`.
- Cold-start docs read: `HANDOFF_CLAUDE_2026-10-13.md` (sections 1-5), `README.md` header,
  `CURRENT_STATE.md` header, `governance\PRODUCT_STATUS.json` entry (`:398-404`), `development\mods\MOD_STANDARD.md` (sections 1-8).
- MOD_STANDARD applied: R2 (worker threads may do file I/O, post to broker), R4 (no slow work before
  broker registration), R6 (address inventory), R9 (session boundaries), R10 (settings in `web_telemetry.ini`),
  section 6 (gates), section 8 (records).
- MOD_STANDARD section 2 layout (`development\mods\<id>\`) does not match this component:
  governance lists `developmentPath = f1c_ida_decompiled\mods\sdk_sandbox\web_telemetry`
  (`governance\PRODUCT_STATUS.json:403`). Open (OQ-12).

### 0.2 Owner decisions (requirements)
1. `DECISION` (r2, replaces r1 "temporary by default"): recordings are **not** cleared after a session.
   Every session is recorded to disk in the working format. A **Recordings manager** lets the user load,
   archive, delete and size recordings. The tool never deletes anything automatically.
2. `DECISION`: archive = conversion to the compact permanent LZMS format (replaces the old SAVE).
   Unarchived recordings stay in the working XPRESS format.
3. `DECISION`: delete is user-initiated only, with a confirmation dialog naming the recording and its
   size. Files go to the Windows Recycle Bin (`SHFileOperationW`, `FOF_ALLOWUNDO`). Never hard-deleted.
   If the Recycle Bin fails, abort and tell the user. No fallback hard delete.
4. `DECISION`: sizes shown for each recording file, each session, and the total folder.
5. `DECISION`: 1 GB folder warning, warn only. `MaxStorageMB` and the 3000 MB auto-delete are retired.
6. `DECISION`: crash recovery still applies. A recording cut by a crash is repaired up to the last good
   block on next start and appears in the manager marked **recovered**.
7. `DECISION`: all version labels become 1.1.0 (section 12, step 0).
8. `DECISION` (unchanged): folder `<game dir>\Telemetry\`, not inside `plugins\lightloader`. Game dir from the DLL.
9. `DECISION` (unchanged): old 1.0.1 `telemetry_data` sessions stay in place; import offered; nothing deleted without consent.
10. `DECISION` (unchanged): session file name `Track_Year_YYYY-MM-DD_HH-MM`, e.g. `Silverstone_2002_2026-10-13_14-05`.
11. `DECISION` (unchanged): Windows 8 and later only.
12. `DECISION` (unchanged): weekend = same track + season + continuous play. Record all sessions. Selector:
    Practice 1-4, Qualifying, Warm-up, Race.
13. `DECISION` (unchanged): compression = own encoding, then Windows Compression API from `cabinet.dll`
    loaded at runtime; fallback to uncompressed blocks with a flag; never crash.
14. `DECISION` (unchanged): player-only `pch` channels; AI cars get a reduced set.
15. `DECISION` (unchanged): Export `.zip` (miniz) is a later feature, not 1.1.0.

### 0.3 Facts that drive the design
- `FACT-H`: capture rate about 20 Hz (worker `Sleep(50)`); live 18.7-19.6 Hz.
- `FACT file:line` `web_telemetry.c:1454-1455` (rate comment), `compatibility_runtime.inc:265` (`Sleep(50)`).
- `FACT file:line` `compatibility_runtime.inc:239`: `ar_flush_io(0)` runs on the capture worker thread (coordinator checked). 1.0.1 writes to disk on the capture thread. Owner rule forbids that (section 6).
- `FACT file:line` `session_archive.inc:500-517` (`ar_retention`, auto-delete at `:502`, coordinator checked). Removed by r2.
- `FACT file:line` `session_archive.inc:41`: 96 B on-disk trace sample (coordinator checked).
- `FACT src` `compressapi.h` (mingw32): `COMPRESS_ALGORITHM_XPRESS` = 3, `COMPRESS_ALGORITHM_LZMS` = 5 (coordinator checked; was INFERENCE in r1).
- `FACT-H`: session end = sim time jumps to about 1e9 and player object reads zero. Code documents this only
  in a comment and a validity test (`web_telemetry.c:4521-4524`). 1.0.1 archive does not use it (section 2.13).

---

## 1. Objective, non-goals, acceptance criteria

### 1.1 Objective
Replace the 1.0.1 session archive with a recordings store and manager that:
- records every session of a weekend to disk, in the compact working format, under `<game dir>\Telemetry\`;
- groups sessions into recordings (one weekend each) and offers load, archive, delete and size views;
- archives a recording to the compact permanent LZMS format on user request;
- never deletes automatically; deletes only on explicit user confirmation, to the Recycle Bin;
- repairs crash-cut recordings on next start and marks them recovered;
- never blocks the capture worker on disk or compression;
- warns above 1 GB total, warn only.

### 1.2 Non-goals
- NG-1 Windows 7 (and older) storage. Refused there (AC-11).
- NG-2 Any automatic deletion, cleaning, clearing or moving of user data. Includes the 1.0.1 `MaxStorageMB` cap (retired).
- NG-3 Hard delete of any kind (no `DeleteFileA`, `RemoveDirectoryA`, `_unlink`, `remove()`).
- NG-4 Export `.zip` (section 12, later).
- NG-5 Changing the capture rate (stays about 20 Hz, `Sleep(50)`). No 60 Hz design.
- NG-6 New game-memory writes. Read-only. Existing addresses only: AIW path `0x008AC438`
  (`web_telemetry.c:389`), session type `0x007C56A4` (`web_telemetry.c:298`), vehicle count `0x005C329C` (`web_telemetry.c:304`).
- NG-7 Mechanical Failures values in stored data (handoff section 1: vanilla only).
- NG-8 RESEARCH channels `rx[16]` (64 B) not stored (as 1.0.1, `session_archive.inc:41`). OQ-6.
- NG-9 The browser never decompresses. The DLL decodes; the browser gets JSON over HTTP.
- NG-10 Network upload.

### 1.3 Acceptance criteria (measurable)
- AC-1 No automatic removal: a full lifecycle test (record two sessions, unload, restart, archive, load)
  records zero calls to any delete or move API except the Recycle Bin call that the test itself requests.
- AC-2 Archive round trip: for every closed session, the archived `.f1a` decodes to the **same quantized
  integers** as the working `.f1t` (bit-exact, 100% of values, all channels), and every decoded value is
  within the section 4.5 bound of the source float. Tick count equal. Verified before the rename (7.7).
- AC-3 Crash repair: with the process killed at random times (200 kills), data lost is at most 3 s
  (ring 2 s + one 1 s block in progress). After restart, the repaired `.f1t` holds all blocks with a valid
  CRC, its tick count equals the last committed block, and it is marked `recovered` (synthetic writer test; live test in 11.6).
- AC-4 Capture isolation: with recording on, `capture_cost_us` p95 < 500 us (BACKEND4 limit, `BACKEND4_FINAL_2026-10-09.md`),
  `/health` rate >= 19.0 Hz over 30 min at 24 cars, and a static check that no file API is reachable from
  `telemetry_worker` or `capture_on_game_thread`.
- AC-5 Memory: storage adds at most 24 MB working set (`/health` `memory.working_set_kb` delta, including compressor
  state); ring plus block buffers at most 256 KB.
- AC-6 Size target: a 1-hour, 24-car, 20 Hz, all-channel session is at most 60 MB working and at most 60 MB archived
  (ESTIMATE in section 5: 29-48 MB working; archive expected similar or smaller, to be measured).
- AC-7 Warning: the manager shows the warning text (9.6) when the total folder is above 1 GiB (1,073,741,824 bytes).
  The warning never removes or moves a file (test with a synthetic 1.2 GB folder in `%TEMP%`).
- AC-8 Fallback: with `cabinet.dll` unavailable (simulated), recording, load and reading work with uncompressed
  blocks and the flag set. Archive is refused with the text in 9.6 (LZMS is needed). No crash.
- AC-9 Reader robustness: truncation at every byte offset of three reference files, plus 1,000,000 random
  mutations, cause no crash and no out-of-bounds access (64-bit ASan/UBSan host build). Every rejected block reports a reason code.
- AC-10 Legacy preserved: the 1.0.1 folder `plugins\lightloader\telemetry_data\` is byte-identical before and
  after import and after ignoring it (SHA-256 manifest).
- AC-11 OS gate: with a version stub below 6.2, no file is created under `Telemetry\` and the text in 9.6 shows.
- AC-12 Naming: parser test set (3.4) passes 100%; unparseable input yields `Unknown_0000` (or partial forms).
- AC-13 Notice: the manager notice (9.4) is present in every state (empty, recording, archived, recovery, OS refused).
- AC-14 API: `/health` reports `api_version` 9; `schemas/` gate passes; `/recordings` validates against `f1c.recordings/1`.
- AC-15 Delete path: the only delete code is one `SHFileOperationW` call with `FOF_ALLOWUNDO` (static grep: no
  `DeleteFile`, `RemoveDirectory`, `_unlink`, `remove(`, `FOF_` without `FOF_ALLOWUNDO` in the storage code).
- AC-16 Recycle Bin safety: when the Recycle Bin is off for the drive, or `SHFileOperationW` returns an error or
  an abort, nothing is deleted, the state is unchanged, and the text in 9.6 is shown (test double).
- AC-17 Delete guard: a delete request whose `confirm_bytes` differs from the current size is refused with 409
  (stale view). Test: wrong size is refused, right size is accepted (double).
- AC-18 Sizes: each file size in `GET /recordings` equals its byte size on disk; each session total equals the sum of
  its files; the folder total equals the sum of all files under `Telemetry\` (test on a synthetic folder).

---

## 2. Current 1.0.1 behaviour (as found in source)

| # | Topic | Current behaviour | Citation |
|---|---|---|---|
| 2.1 | Root folder | `<module dir>\telemetry_data\`. Module dir = folder of the DLL (`plugins\lightloader\`). | `compatibility_runtime.inc:311-317` (`GetModuleFileNameA(telemetry_module,...)` at 311, `ar_init(path)` at 317); `session_archive.inc:32`; `session_archive.inc:147-154`; `session_archive.inc:156-159`; viewer text `telemetry_viewer.js:4522-4524` |
| 2.2 | Session id | `YYYYMMDD-HHMMSS_s<ty>_g<gen % 100000>`, local time. Collision suffix `_n<k>`. | `session_archive.inc:260-276` (format 270-272); collision `session_archive.inc:377-385` |
| 2.3 | Session key and boundary | Session = (broker generation, session type, track name). Boundary on change, restart flag, or fresh sample with a different key. | `session_archive.inc:279-300` (key test 286-291) |
| 2.4 | Folder creation | Folder created only at the first completed lap. | `session_archive.inc:371-373` |
| 2.5 | Track name | AIW file name minus directory and extension, so it **includes the season** (`2002_Silverstone`). | `web_telemetry.c:4358-4363`; `web_telemetry.c:4338` (`sectors_update`) |
| 2.6 | Files | `meta.json` (atomic), `frame.json`, `weather.json`, `track.json` (atomic, every 30 s), `traces.bin`, `laps.bin`, `pits.bin`, `incidents.bin`, `journal.jsonl` (append, flush 1 s). | `session_archive.inc:9-18`; `:320-331` (atomic); `:333-342` (append); `:344-369` (meta fields); `:396-432` (flush); `:36`, `:38` cadence |
| 2.7 | Trace lap header | `ar_lap_hdr_t` 192 B: magic `F1CL`, version 1, slot, lap, flags, samples, sample_bytes (96), lap time, start, lap_len, gen, rev, team, channels, is_ai, is_remote, driver[64], car[64]. | `session_archive.inc:39-40`, `:46-53`, `:199-228` |
| 2.8 | Trace sample on disk | `AR_DISK_SAMPLE = offsetof(trace_sample_t, rx)` = **96 B**. `rx` (64 B RESEARCH) dropped. RAM struct 160 B. | `session_archive.inc:41`; `web_telemetry.c:1505-1522`; `session_archive.inc:922` |
| 2.9 | Lap-only traces | Samples archived only for **completed laps**; count <= `TRACE_SAMPLES` 3200. Open laps and non-lap time not written. | `session_archive.inc:199-206`; `web_telemetry.c:1461`; `web_telemetry.c:1450-1451` |
| 2.10 | Per-lap rows | `laps.bin` = `ar_lap_rec_t`; `pits.bin` = `ar_pit_rec_t`; `incidents.bin` = `ar_inc_rec_t`. | `session_archive.inc:55-69` |
| 2.11 | Capture worker | `telemetry_worker`, `Sleep(50)`; archive writes run on this thread. | `compatibility_runtime.inc:129`, `:265`, `:239` (coordinator checked); `web_telemetry.c:1454-1455` |
| 2.12 | Trace capture scope | Every live car traced all the time (3200-sample ring per car). | `web_telemetry.c:1446-1449` |
| 2.13 | Session end | No explicit end. Closes only on a different generation, type or track, or on unload. A stale capture (> 1000 ms) zeroes the cars but does not call `ar_tick_locked`. | `session_archive.inc:286-294`; `compatibility_runtime.inc:231`; `:233-237`; unload `:277-278` |
| 2.14 | Retention | `MaxStorageMB` default **3000**; oldest finished sessions deleted automatically until under the cap. | `web_telemetry.c:3832`; `web_telemetry.c:3931-3933`; `web_telemetry.ini:9`; `session_archive.inc:500-517` (auto-delete at 502, coordinator checked); `session_archive.inc:533` |
| 2.15 | Delete | `POST /sessions/delete`: LAN guard, `X-Telemetry-Action: delete`, octet-stream, 64 KB, newline list. **Hard delete** (`DeleteFileA`, `RemoveDirectoryA`). Also legacy `web_telemetry_race_*.jsonl`. | `web_telemetry.c:8814-8827`; `session_archive.inc:629-683` (legacy branch 645); `session_archive.inc:450-471` |
| 2.16 | List | `GET /sessions` returns `enabled, dir, current, total_bytes, max_mb, legacy_journals{files,bytes}, sessions[{id,bytes,current,meta}]`. | `web_telemetry.c:8829-8834`; `session_archive.inc:584-626`; `session_archive.inc:478-496` |
| 2.17 | Health | `/health` `archive` object: `enabled, dir, current, max_mb, dropped, retention_deleted`. | `web_telemetry.c:8573`; `session_archive.inc:976-983` |
| 2.18 | Viewer | Menu group `Data`, item `sessions` ("Sessions & Data Manager"). Poll every 10 s. Delete confirms "This cannot be undone." | `telemetry_viewer.js:2939-2960`; `:3318`; `:4521-4538` |
| 2.19 | API version | `API_VERSION "8"`. | `web_telemetry.c:56` |
| 2.20 | Contract coverage | `/sessions` and `/sessions/delete` are not in `API_CONTRACT_2026-10-09.md`; `schemas/` has no sessions schema. | `API_CONTRACT_2026-10-09.md` (only lines 389, 443 mention sessions) |
| 2.21 | Game-dir anchor | Folder of `GetModuleFileNameA(NULL)`. Used for AIW and tyre files. | `web_telemetry.c:4367-4372`; `web_telemetry.c:5487-5495` |
| 2.22 | Session type names | `fc_type_name`: 0 TestDay, 1-4 Practice1-4, 5 Qualify, 6 Warmup (comment: inference), 7 Race. | `web_telemetry.c:8347-8356`; `web_telemetry.c:2225-2229` |
| 2.23 | Log file | `web_telemetry_log.txt` beside the DLL; rotated at 2 MB. | `compatibility_runtime.inc:313-314`; `web_telemetry.c:3828-3831` |
| 2.24 | Settings | `[Mod]` keys in `web_telemetry.ini`. | `web_telemetry.ini`; `web_telemetry.c:3921-3938` |

Consequences for 1.1.0:
- Disk writes on the capture thread (2.11) move to a storage thread (section 6).
- Auto-delete (2.14) and hard delete (2.15) are removed (NG-2, NG-3, AC-1, AC-15).
- Track name includes the season (2.5); parsed in 3.4.
- Session end (2.13) gets the 1e9 signal plus existing fallbacks (7.2).
- "All session data" (owner) needs a continuous per-tick stream (section 4), not laps only (2.9). Size effect in section 5.

---

## 3. Folder and file layout

### 3.1 Game directory resolution (from the DLL)
1. `GetModuleFileNameA(NULL, path, MAX_PATH)` gives the game executable. Cut at the last `\`. Same anchor as `web_telemetry.c:4367-4372` and `:5487-5495` (FACT).
2. Game root = that folder. Telemetry root = `<game root>Telemetry\`.
3. Checks: path below `MAX_PATH - 64` characters; `Telemetry` must not exist as a file (if it does: refuse with 9.6, no rename).
4. Create `Telemetry\`, `Telemetry\recordings\` (`CreateDirectoryA`). Failure -> storage disabled, reason logged and shown. No silent fallback folder.
5. No registry or config lookup for the game root (same rule as the AIW anchor, 2.21).
6. OS check: `VerifyVersionInfoA` major 6, minor 2 (Windows 8). `RtlGetVersion` preferred. `GetVersionEx` not used.

INFERENCE: the game usually runs elevated (`HANDOFF_CLAUDE_2026-10-13.md` section 5), so writes under the game folder likely succeed. Risk R-7.

### 3.2 Layout
```
<game root>\Telemetry\
    index.json                              manifest cache, rebuilt from a folder scan if it disagrees
    recordings\
        <Track>_<Year>_<YYYY-MM-DD_HH-MM>\         one recording = one weekend (named by its first session start)
            <Track>_<Year>_<YYYY-MM-DD_HH-MM>.f1t           session, working format (XPRESS blocks)
            <Track>_<Year>_<YYYY-MM-DD_HH-MM>_2.f1t         name collision suffix (2, 3, ...)
            <session>.f1t.part                              being recorded now, or cut by a crash
            <session>.f1t.part.damaged                      original of a repaired crash file (kept, renamed only)
            <session>.f1a                                   archived, permanent format (LZMS blocks)
            <session>.f1a.partial                           archive in progress or interrupted (kept until the user deletes it)
<module dir>\telemetry_data\                1.0.1 folder: never modified; read only for import (7.9)
```
- Extensions: `.f1t` working (XPRESS), `.f1a` archived (LZMS), `.part` open or crash-cut, `.partial` archive in progress.
- The session file name uses the session start time (owner example format). The recording folder uses the
  first session's start time of the weekend.
- No temp folder. Every session is written in place, from its first tick.
- Session file created at the first tick with at least one car present (menu-only time creates no file).

### 3.3 Session key and id
- Session id `YYYYMMDD-HHMMSS_s<ty>_g<gen>` (as 1.0.1, `session_archive.inc:270-272`). `ty` = game session type (-1 becomes 99), `gen` = broker generation. Characters `[0-9A-Za-z_-]`, length 8-47 (`session_archive.inc:135-145`).
- Session boundary (PROPOSAL, extends 2.3): new session when generation, session type or track key changes, or restart flag is set. Also, a new session starts when no fresh sample arrived for more than 30 s (only together with the 1e9 end signal, 7.2).

### 3.4 Track and season parsing (AIW path, `0x008AC438`)
Input `P` = string at `0x008AC438` (up to 259 chars; `web_telemetry.c:165`, `:196`, `:389`). Example path shape `SEASONDATA\CIRCUITS\Italy\2001_Monza\2001_Monza.AIW` (`FACT-H`).

Rules, in order:
1. Empty `P`, or not ending in `.AIW` (case-insensitive) -> `Unknown`, year `0000`.
2. Split on `\` and `/`. `base` = last component without `.AIW`.
3. If `base` matches `^(\d{4})_(.+)$`: year = group 1, track = group 2.
4. Else if the parent folder matches the same pattern and equals `base` (`2001_Monza\2001_Monza.AIW`): use the parent.
5. Else: track `Unknown`, year `0000`.
6. Sanitize track: trim spaces; replace runs of spaces and of `\ / : * ? " < > |` with `-`; keep letters, digits, `-`; strip leading and trailing `-` and `_`; cap 32 characters. Empty -> `Unknown`.
7. Year must be 1950..2099, else `0000`.
8. Forms: `Unknown_0000` (both fail); `<track>_0000` (year fails); `Unknown_<year>` (track fails).
9. Raw `base` (or `P`) stored in the META block for later re-parsing (OQ-3).

Test set (AC-12):

| Input `P` | Track | Year | Prefix |
|---|---|---|---|
| `SEASONDATA\CIRCUITS\Italy\2001_Monza\2001_Monza.AIW` | Monza | 2001 | `Monza_2001` |
| `...\2002_Silverstone\2002_Silverstone.AIW` | Silverstone | 2002 | `Silverstone_2002` |
| `webtel_aiw\test.AIW` | Unknown | 0000 | `Unknown_0000` |
| `...\Silverstone\Silverstone.AIW` (no year) | Unknown | 0000 | `Unknown_0000` |
| `...\2002_Silverstone.AIW` (no folder) | Silverstone | 2002 | `Silverstone_2002` |
| `` (empty) | Unknown | 0000 | `Unknown_0000` |
| `...\2002_Silver stone\2002_Silver stone.AIW` | Silver-stone | 2002 | `Silver-stone_2002` |
| `...\9999_X\9999_X.AIW` | X | 0000 | `X_0000` |

The name is fixed at session close or archive time from META, because the AIW may become valid after
the session starts. The file name at creation uses the session id until the track is known (then the
`.part` is renamed to its final name at session end; rename only).

### 3.5 Weekend grouping
- Weekend key = `<Track>_<Year>_w<seq>`; seq starts at 1 per game process.
- New weekend when: the game process starts, or track or year changes, or (PROPOSAL, OQ-4) the session type returns to 0 (Test Day) after a race with the same key.
- A session belongs to the weekend current at its start. The recording folder is created with the weekend's first session.

### 3.6 index.json (manifest, `f1c.telemetry.index/1`)
```json
{
  "schema": "f1c.telemetry.index/1",
  "updated_utc": "2026-10-13",
  "warn_bytes": 1073741824,
  "total_bytes": 318767104,
  "recordings": [
    {"id": "Silverstone_2002_2026-10-13_10-00", "weekend": "Silverstone_2002_w1",
     "sessions": ["Silverstone_2002_2026-10-13_10-00", "Silverstone_2002_2026-10-13_14-05"]}
  ],
  "files": [
    {"name": "Silverstone_2002_2026-10-13_14-05.f1t.part", "recording": "Silverstone_2002_2026-10-13_10-00",
     "state": "open", "bytes": 2631680, "ticks": 8421}
  ]
}
```
- Written atomically (`index.json.tmp`, then `MoveFileExA` with `MOVEFILE_REPLACE_EXISTING`). Rewritten at most every 5 s and on state change.
- A cache only. On mismatch the folder scan and file headers win (7.5).
- File `state` values: `open`, `closed`, `recovered`, `damaged`, `archived`, `archive_interrupted`, `legacy_import`.

### 3.7 Deletion target: Recycle Bin only (replaces r1 disposable folder)
- Every user delete goes to the Windows Recycle Bin through `SHFileOperationW` with `FOF_ALLOWUNDO` (7.8).
- No other location is used. The code never copies, moves or deletes outside `Telemetry\` for user deletes.
- Pre-check before any delete: the Recycle Bin must be enabled for the drive of the file. Read-only check of
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\BitBucket\Volume\<drive GUID>\NukeOnDelete` (must be 0)
  and `SHQueryRecycleBinW` for the drive (must succeed). If either fails or is unknown, delete is refused (AC-16).
  INFERENCE: `NukeOnDelete` semantics and the GUID lookup; verify on the owner PC before freeze (OQ-15).

---

## 4. File format v2 (`F1CTEL02`)

All integers little-endian. Floats IEEE 754 binary32 (`f32`) or binary64 (`f64`). Strings ASCII, NUL-padded or length-prefixed.

### 4.1 File header (192 bytes, fixed)

| Off | Size | Type | Field | Value / rule |
|---|---|---|---|---|
| 0 | 8 | char[8] | magic | `F1CTEL02` |
| 8 | 2 | u16 | ver_major | 2 |
| 10 | 2 | u16 | ver_minor | 0 |
| 12 | 4 | u32 | header_bytes | 192 |
| 16 | 4 | u32 | flags | bit0 ARCHIVED (LZMS file); bit1 SESSION_COMPLETE (set when race_phase 8 was seen before close; batch 3, 2026-10-13); bit2 CLOSED (END block present); bit3 COMPRESSED_BLOCKS (file default); bit4 UNCOMPRESSED_FALLBACK; bit5 PLAYER_PCH present; bit6 RECOVERED; bit7 LEGACY_IMPORT; bit8 TICK_GAPS |
| 20 | 4 | u32 | compression_id | 0 none; 3 XPRESS (working); 5 LZMS (archived). Values `FACT src` (`compressapi.h`, mingw32) |
| 24 | 4 | u32 | block_ticks_max | 20 |
| 28 | 4 | u32 | block_ms_max | 1000 |
| 32 | 4 | u32 | channel_table_offset | 192 |
| 36 | 4 | u32 | channel_table_bytes | table size incl. its CRC (4.2) |
| 40 | 8 | f64 | start_unix_utc | session start, Unix seconds (UTC) |
| 48 | 4 | i32 | tz_offset_min | local minus UTC, minutes |
| 52 | 4 | i32 | session_type | game `ty` (-1 unknown) |
| 56 | 4 | u32 | broker_generation | `capture_generation` |
| 60 | 4 | u32 | channels_declared | entries in the channel table |
| 64 | 64 | char[64] | track | ASCII, NUL-padded (sanitized, 3.4) |
| 128 | 60 | char[60] | session_name | ASCII, NUL-padded: the final file name stem |
| 188 | 4 | u32 | header_crc32 | CRC32 of bytes 0..187 |

Reader: `magic` must match, `ver_major` must be 2, `header_bytes` 192, `header_crc32` must match. Otherwise header rejected.
Archiving rewrites the header of the new `.f1a` file (ARCHIVED set, compression_id 5); it never edits the `.f1t` in place.
Repair (7.5) writes a new header with RECOVERED set.

### 4.2 Channel table (at `channel_table_offset`)
Table header: `u16 count` (= channels_declared, max 128), `u16 reserved` (0).
Each entry:

| Off (rel.) | Size | Type | Field |
|---|---|---|---|
| 0 | 1 | u8 | id (0..127, unique) |
| 1 | 1 | u8 | type: 0 = Q16 (quantized u16), 1 = RAW32 (exact i32/u32), 2 = TICK_MS (ms, i32) |
| 2 | 1 | u8 | scope: bit0 player (full), bit1 AI (reduced) |
| 3 | 1 | u8 | reserved (0) |
| 4 | 4 | f32 | min (Q16 range; 0 for RAW32 and TICK_MS) |
| 8 | 4 | f32 | max |
| 12 | 1 | u8 | name_len (1..32) |
| 13 | name_len | ASCII | name (channel key, e.g. `speed`) |

Table trailer: `u32 crc32` over the table bytes before it.

### 4.3 Block framing (40-byte header)

| Off | Size | Type | Field | Rule |
|---|---|---|---|---|
| 0 | 4 | char[4] | magic | `BLK2` |
| 4 | 1 | u8 | block_type | 1 DATA, 2 META, 3 END, 4 GAP |
| 5 | 1 | u8 | block_flags | bit0 payload compressed |
| 6 | 2 | u16 | reserved | 0 |
| 8 | 4 | u32 | seq | 0, 1, 2 ... contiguous |
| 12 | 4 | u32 | n_ticks | DATA: 1..255; other types 0 |
| 16 | 8 | f64 | t_first | session seconds of first tick; 0 otherwise |
| 24 | 4 | u32 | raw_len | payload length before compression (<= 1 MiB) |
| 28 | 4 | u32 | stored_len | payload length as stored (<= 1 MiB) |
| 32 | 4 | u32 | payload_crc32 | CRC32 of stored payload |
| 36 | 4 | u32 | header_crc32 | CRC32 of bytes 0..35 |

- DATA: one per about 1 s (20 ticks); closes at 20 ticks or 1000 ms.
- META: initial after the channel table, plus a final META at close (latest valid wins). UTF-8 JSON, max 16 KB,
  schema `f1c.telemetry.meta/2`: weekend key, track (raw and sanitized), season, session type name, drivers
  (slot, name, car, team), counters, and the pit, incident and lap rows of the session (for the viewer pages, 8.5).
- END: `u32 data_blocks, u32 total_ticks, u32 dropped_ticks`. Sets CLOSED.
- GAP: `u32 dropped_ticks, f64 t_from, f64 t_to`. Written before the next DATA when ticks were dropped (6.2).

### 4.4 DATA payload (before compression, channel-major per car)
```
u8   n_ticks
u8   n_cars                        (1..104)
varint dt_ms[0..n_ticks-1]         TICK_MS: dt_ms[0] = 0; zigzag varint of delta to previous tick
for each car (n_cars times):
    u8   slot                      0..103
    u8   scope                     1 = player (full set), 0 = AI (reduced)
    u8   first_tick                offset in block
    u8   count                     ticks where the car is present (>= 1)
    u64  mask_lo                   channel ids 0..63 present
    u64  mask_hi                   channel ids 64..127 present
    for each present channel id ascending:
        Q16:   u16 first_value, then (count-1) zigzag varint deltas of the q value
        RAW32: u32 first_value, then (count-1) zigzag varint deltas of the 32-bit difference (wrap-safe)
```
- Keyframe at each block start (absolute first value), so every block decodes alone.
- LEB128 varints, max 5 bytes. Zigzag: `(n << 1) ^ (n >> 31)`; inverse `(z >> 1) ^ -(z & 1)`.
- Corruption checks: channel not in the table, `count` 0, `first_tick + count > n_ticks`, varint over 5 bytes.

### 4.5 Quantization (Q16)
Encode: `q = round((clamp(v, min, max) - min) * 65535 / (max - min))`, q in 0..65535.
Decode: `v = min + q * (max - min) / 65535`.
Bound for `v` in `[min, max]` (amended in batch 3 to match 11.1 U-1; batch 2 record 1.4): `|v - decode(encode(v))| <= (max - min) / 131070 + 1.1920929e-7 * max(|min|, |max|)`.
The last term is the binary32 rounding of the decoded value. The r1 bound without it cannot hold for large magnitudes (e.g. rpm 25000, where half an ulp is about 0.001).
Out-of-range values are clamped and bit 21 `TRF_OUT_OF_RANGE` is set in the RAW32 `flags` channel
(existing bits are 0..20, `web_telemetry.c:1470-1497`).

Catalog (1.1.0 proposal). Ranges are ESTIMATE from existing validators; to be checked against measured data (OQ-5).

| id | key | type | scope | min | max | max error | Source |
|---|---|---|---|---|---|---|---|
| 0 | tick_ms | TICK_MS | all | - | - | 0 | 4.4 |
| 1 | dist | Q16 | all | 0 | 8000 m | 0.061 m | `TRF_DIST` `web_telemetry.c:1470`; `dist` field `:1507` |
| 2 | speed | Q16 | all | 0 | 120 m/s | 0.0009 m/s | `speed` |
| 3 | throttle | Q16 | all | 0 | 1 | 7.6e-6 | `throttle` |
| 4 | brake | Q16 | all | 0 | 1 | 7.6e-6 | `brake` |
| 5 | steer | Q16 | AI only | -1 | 1 | 1.5e-5 | `steer` |
| 6 | glat | Q16 | all | -80 | 80 m/s^2 | 0.0012 | `glat` |
| 7 | glon | Q16 | all | -80 | 80 m/s^2 | 0.0012 | `glon` |
| 8 | gvert | Q16 | all | -60 | 60 m/s^2 | 0.0009 | `gvert` |
| 9 | yaw | Q16 | all | -3.1416 | 3.1416 rad | 2.4e-5 | `yaw` |
| 10 | yaw_rate | Q16 | all | -10 | 10 rad/s | 7.6e-5 | `yaw_rate` |
| 11-14 | tread_fl, fr, rl, rr | Q16 | all | 0 | 1 | 7.6e-6 | `tread[4]` |
| 15 | rpm | Q16 | all | 0 | 25000 | 0.19 rpm | `rpm` (`web_telemetry.c:2027`, validator `:3351`) |
| 16 | gear | RAW32 | all | - | - | exact | `gear` (`:2031`) |
| 17 | tx | Q16 | all | -10000 | 10000 m | 0.076 m (placeholder) | `tx` (`:2037`); range NOT FOUND (OQ-5) |
| 18 | tz | Q16 | all | -10000 | 10000 m | 0.076 m (placeholder) | `tz`; range NOT FOUND (OQ-5) |
| 19 | flags | RAW32 | all | - | - | exact | TRF bits (`:1470-1497`) |
| 20-23 | ttemp_fl, fr, rl, rr | Q16 | player only | 0 | 200 C | 0.0015 C | `ttemp[4]` (AI value is RESEARCH, `:2048-2049`, not stored for AI) |
| 24-63 | pch_* | Q16 / RAW32 | player only | per field | per field | per field | `web_telemetry.c:1033-1042` (4.6); ranges NOT FOUND (OQ-5) |
| 64-127 | reserved | - | - | - | - | - | - |

Gear, flags and tick are exact (no bound applies).

### 4.6 Player chassis channels (`pch`, player only)
Fields `web_telemetry.c:1033-1042`: `pch_ride_mm[4]`, `pch_susp_mm[4]`, `pch_tdefl_mm[4]`, `pch_press[4]`,
`pch_press_set[4]`, `pch_grip[4]`, `pch_opt_c[4]`, `pch_camber_deg[4]`, `pch_spin[4]`, `pch_roll[4]`,
`pch_disc_mm[4]`, `pch_disc_fail_mm[4]`, `pch_bend[4]` (about 52 floats); `pch_bent[4]`, `pch_detach[4]` (RAW32);
`pch_bottom`, `pch_compound` (RAW32); `pch_eng_wear`, `pch_fuel`, `pch_rpm`, `pch_clutch_rpm`, `pch_oil_press`,
`pch_bias`, `pch_drag`, `pch_lift`, `pch_drag_used`, `pch_in_raw[4]`, `pch_in_used[4]`. About 79 values, about 60 Q16.
Ids 24..63 give 40 slots; the set must be trimmed or the range extended (OQ-5). `pch_valid` is the per-tick gate
and is not stored (a tick without valid pch lists no pch channels for that car).

### 4.7 AI reduced set (DECISION: AI cars get a reduced set)
- Stored for AI (scope bit1): tick, dist, speed, throttle, brake, steer, glat, glon, yaw, yaw_rate, tread x4, rpm, gear, tx, tz, flags
  (channels the AI already validates, `web_telemetry.c:2027-2037`).
- Not stored for AI: gvert (8), ttemp (20-23; AI value is RESEARCH, `:2048-2049`), all pch channels.
- PROPOSAL (OQ-7): AI gvert excluded.

### 4.8 Compression (r2)
- Working format (`.f1t`): each DATA/META/END/GAP block compressed with XPRESS (`compression_id` 3) when the result is
  smaller than raw; otherwise stored raw (block bit0 = 0). Reader handles both per block.
- Archive format (`.f1a`): each block decoded from `.f1t` and recompressed with LZMS (`compression_id` 5). The file
  header sets ARCHIVED (bit0). If LZMS is unavailable or its working memory exceeds the 16 MB budget (6.5), **archive is refused**
  with the text in 9.6. There is no silent fallback to another algorithm.
- Loader: `LoadLibraryExA("cabinet.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)` on the jobs or writer thread, then
  `GetProcAddress` for `CreateCompressor`, `Compress`, `CloseCompressor`, `CreateDecompressor`, `Decompress`,
  `CloseDecompressor`. In a 32-bit process on 64-bit Windows the system32 lookup resolves to the 32-bit copy
  (INFERENCE; owner reports the copy exists on Windows 11; export availability on Windows 8 is OQ-1b).
- If a symbol is missing or create fails: the writer writes all blocks raw (file flag bit4), the notice in 9.6 shows once,
  and recording continues. Reading a compressed block without a decoder gives reason `DECODER_UNAVAILABLE` for that block only.
- Compression is never on the capture thread (section 6).

### 4.9 CRC32
Standard CRC-32 (reflected polynomial 0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF; zlib-compatible).
Unit-tested against `CRC32("123456789") = 0xCBF43926`.

### 4.10 Recovery and reader algorithm
1. Validate the header (4.1). Fail -> reason `HEADER`.
2. Read and CRC-check the channel table. Fail -> `TABLE`.
3. Loop over blocks from `channel_table_offset + channel_table_bytes`:
   a. Read 40 bytes; check magic, `header_crc32`, `seq == expected`, sizes <= 1 MiB, `block_type` 1..4. Fail -> stop, `BLOCK_HEADER` at `seq`.
   b. Read `stored_len` bytes; check `payload_crc32`. Fail -> stop, `BLOCK_CRC`.
   c. Decompress if flagged; decoded length must equal `raw_len`. Fail -> stop, `BLOCK_DECODE`.
   d. Decode DATA per 4.4 with bounds checks on every read.
4. Valid prefix = all blocks up to the last good one. CLOSED only if a valid END exists.
5. Report: `valid_blocks`, `ticks`, `last_good_t`, `truncated`, `reason`, `reason_seq`.

Limits: payload 1 MiB; channels 128; cars 104; ticks per block 255; varint 5 bytes; META 16 KB; file offsets u32
(4 GiB); reads are chunked (6.6).

---

## 5. Size estimate (ESTIMATE, to be measured)

Assumptions: 20 Hz (50 ms ticks), 24 cars, 1 hour car-present per car (worst case), all channels, player pch about 60 Q16 values.
Per car-hour = 72,000 samples; 24 cars = 1,728,000 car-samples per hour.

| Representation | Bytes per car-sample | Per car-hour | 24 cars, 1 hour | Basis |
|---|---|---|---|---|
| Live ring (1.0.1 RAM) | 160 | 11.5 MB | 276 MB | `web_telemetry.c:1505-1522` (FACT) |
| 1.0.1 archive, laps only (upper bound) | 96 | 6.9 MB | 166 MB | `session_archive.inc:41` (FACT) |
| v2 encoded, uncompressed | 36-44 | 2.6-3.2 MB | 62-76 MB | 4.4, ESTIMATE |
| v2 working `.f1t` (XPRESS, 1.6-2.2x) | 17-28 | 1.2-2.0 MB | 29-48 MB | ESTIMATE ratio |
| v2 archived `.f1a` (LZMS) | 15-26 | 1.1-1.9 MB | 26-45 MB | ESTIMATE |
| Player pch extra (about 60 Q16, player only) | about 100-120 | +7-9 MB per player hour | +7-9 MB | ESTIMATE |

Consequences:
- A full 24-car hour is about 30-50 MB working (ESTIMATE). A weekend of about 6 hours car-present is about 180-300 MB.
- With everything kept (owner decision 1), the 1 GiB warning is reached after about 3-6 full weekends (ESTIMATE), or about 20-35 hours of recording.
- The driver of size is channels and continuous recording, not the 60 Hz question (handoff 4.3).

Memory (ESTIMATE): ring 128 KB max; block buffers 2 x 64 KB; XPRESS working set small; LZMS working set NOT FOUND, budget 16 MB.

---

## 6. Writer threading and memory

### 6.1 Threads
| Thread | Existing? | Work | Locks |
|---|---|---|---|
| Game-thread callback `capture_on_game_thread` | existing | builds the tick record (existing code), copies it into the ring | `g_telemetry_cs` (existing) |
| `telemetry_worker` (capture loop) | existing (`compatibility_runtime.inc:129`) | `read_telemetry`, SSE, log, then copies one sample into the ring (`sw_capture_tick`, batch 2 decision, record 4.1). **No file IO**: `ar_flush_io(0)` removed from the loop (`compatibility_runtime.inc`) | `g_telemetry_cs` as today |
| `storage_writer` (new) | new | drain ring; quantize (4.5), encode (4.4), compress XPRESS (4.8), append blocks; index every 5 s | `g_storage_cs` (short) |
| `storage_jobs` (new, low priority) | new (`storage_jobs.inc`, batch 3) | archive (LZMS), startup repair, delete (Recycle Bin). Import (7.9) not yet (needs META 4.3). Size totals are written by the writer in the index | `g_storage_cs` (short); reads whole files into memory (batch 3 deviation, 6.5 chunking pending) |
| `http_thread` (existing, `web_telemetry.c:9274`) | existing | answers API, decodes files in chunks, queues jobs | `g_storage_cs` (snapshot copy only) |

Lock order: `g_telemetry_cs` -> `g_storage_cs`, never reversed. Storage threads never read game memory and never
hold `g_telemetry_cs`. The ring is single producer (capture) and single consumer (writer) with atomic indexes.
Delete (`SHFileOperationW`) runs on `storage_jobs`, which calls `CoInitializeEx(COINIT_APARTMENTTHREADED)` first (INFERENCE: shell
operations may need COM; verify).

### 6.2 Ring and overflow
- Element = one tick record of all live cars (about 24 x 108 B + up to 240 B player pch, about 2.8 KB).
- Capacity 40 ticks (2 s at 20 Hz), about 112 KB, allocated at start.
- Full ring: capture drops the **new** tick, increments `dropped_ticks` (atomic), sets a pending GAP. Never blocks.
- The writer writes a GAP block before the next DATA block with count and time range. Never invents data.
- Capture-side cost: one memcpy about 2.8 KB per tick (about 56 KB/s). Budget under 20 us p95 added (AC-4).

### 6.3 Writer cadence
- Wakes on an event set per tick, or every 250 ms.
- Each completed block: `fwrite` then `fflush` (process-crash durable up to the last block). No per-block `FlushFileBuffers`.
- `FlushFileBuffers` at close (END block).
- Index rewrite every 5 s or on state change.

### 6.4 Compressor memory budget
- XPRESS: small, allowed on the writer thread.
- LZMS (archive only, jobs thread): NOT FOUND for working-set size. Measure at gate. If above 16 MB in the 32-bit process, archive is refused (9.6) and the owner is told; no silent algorithm change.

### 6.5 Chunked IO
- Archive, repair, load and reads process one block at a time (at most 1 MiB + 40 B resident).
  The 1.0.1 `AR_FILE_MAX` 64 MB cap (`session_archive.inc:43`) does not apply to v2.
- HTTP responses keep the 2 MB cap (`web_telemetry.c:3228`). Large reads are paged (8.5).

### 6.6 Startup timing (R4)
- Path setup and folder check run before broker registration (path work only).
- Folder scan, repair of crash-cut files, index load and `cabinet.dll` load run on `storage_jobs` **after** registration
  (after `compatibility_runtime.inc:173`). Startup milliseconds logged.

### 6.7 Shutdown
- Unload: stop capture, writer drains the ring, writes END for open sessions, renames `.part` to `.f1t`, flushes, updates index.
  Budget 5 s (matches `compatibility_runtime.inc:272`). If exceeded, `.part` stays and is repaired at next start (7.5).
- No file is moved or removed at shutdown.
- A running archive is allowed 3 s more; if still running, its `.f1a.partial` stays (listed, 7.7).

---

## 7. Recording, load, archive, delete and recovery flows

### 7.1 Recording a session (automatic, every session)
1. Session boundary (3.3). Writer creates `<session>.f1t.part` in the recording folder, writes the header (4.1), channel table and initial META.
2. Each tick: capture copies into the ring; writer appends DATA blocks (1 s).
3. Every 5 s: index updated (state `open`, bytes, ticks).
4. Session end (7.2): END block, CLOSED, rename `.part` to `.f1t` (state `closed`). The session stays in the manager. Nothing is moved or cleared.
5. The recording folder keeps growing with each session of the weekend.

### 7.2 Session end (PROPOSAL, extends 2.13)
- A (primary, FACT-H): player object reads zero and sim time above 1e8 for more than 1 s (validity test `web_telemetry.c:4521-4524`). Verify live (OQ-2).
- B (existing): generation, type or track changes (`session_archive.inc:286-291`).
- C (new): no fresh sample for more than 30 s after data.
- On end: END block, CLOSED, rename. No delete.

### 7.3 Unload (normal game exit)
- Stop capture, drain ring, END, CLOSED, rename `.part` to `.f1t`, index. Nothing else. No clearing (r1 step removed).
- A running archive gets 3 s; otherwise `.f1a.partial` remains and is listed as `archive_interrupted`.

### 7.4 Game crash mid-session (process killed)
- Every fully written block stays. Lost: the ring (up to 2 s) and the block in progress (up to 1 s).
- No END, no rename: the file stays `<session>.f1t.part`.
- A partial block at the end fails its CRC; the reader drops it and reports `truncated`.

### 7.5 Next start: crash repair (marked recovered)
1. After registration (6.6), `storage_jobs` scans `recordings\*\*.part`. Each is validated (4.10).
2. A `.part` with at least one valid DATA block is repaired automatically (repair is not a delete):
   a. Write `<session>.f1t` from the valid blocks: copy the header with RECOVERED set (new header CRC), the channel
      table, valid blocks unchanged (their CRCs are reused), META updated with the repair point, END with `dropped_ticks = 0`. Write to `<session>.f1t.repair` first.
   b. Verify the repaired file: CRC of every block, tick count equal to the valid blocks.
   c. Rename `<session>.f1t.repair` to `<session>.f1t`.
   d. Rename the original `<session>.f1t.part` to `<session>.f1t.part.damaged`. It is kept (rename only).
   e. Index state `recovered`. The manager shows the badge "recovered" and "readable up to <last_good_t> s".
3. A `.part` with no valid block: nothing is changed; state `damaged`, listed with its size; the user may delete it (7.8).
4. A failed step (a or b): the `.part` is left untouched; state `damaged`; error logged; nothing repaired.
5. Repair runs without user action, because it only adds files and renames the original. Deleting is never part of repair.
6. Stale `<session>.f1a.partial` files are listed as `archive_interrupted`. The user may delete them (7.8).

### 7.6 Load (open a recording in the viewer pages)
1. User selects a recording (or one session) in the manager and presses **Load**. HTTP: `POST /recordings/load` (8.2).
2. The DLL records the loaded recording id in memory. No file is changed.
3. Viewer pages (laps, pits, incidents, trace, driver) read the loaded recording through `/recordings/*` (8.5).
   The weekend selector and session tabs (9.2) show that recording's sessions.
4. Loading an open (`.part`) session is allowed read-only (committed blocks only).
5. One recording is loaded at a time. Loading another replaces the selection. Live telemetry is unaffected.

### 7.7 Archive (replaces SAVE; user-initiated)
1. User selects sessions (closed only; an open session returns 409) and presses **Archive**. Confirm dialog (9.6) names the
   recording and the working size, and asks: "Keep the working copy (default)" or "Move the working copy to the Recycle Bin".
2. HTTP: `POST /recordings/archive` with `{"sessions": [...], "recycle_working": false}` (8.2). Returns `202` job.
3. For each selected session (sequential, one job):
   a. If `<session>.f1a` already exists and verifies: skip (idempotent).
   b. Decode blocks from the `.f1t` (committed, CRC-checked), recompress each with LZMS (4.8).
   c. Write `<session>.f1a.partial` with the header ARCHIVED set (compression_id 5).
   d. Verify: CRC of every block; decoded quantized integers equal those of the `.f1t` (bit-exact, AC-2); tick count equal.
   e. Rename `.f1a.partial` to `.f1a`. Index state `archived`.
4. If verification or LZMS fails: the `.f1a.partial` stays, the `.f1t` is untouched, the job reports the error and continues only with
   other sessions if the user asked (default: stop at the first error).
5. If `recycle_working` is true, after step 3 succeeds for that session the `.f1t` goes to the Recycle Bin (7.8 rules,
   same confirm-bytes check). Otherwise the working copy stays.
6. Archive never overwrites an existing file. Names are unique (collision suffix).

### 7.8 Delete (user-initiated only, Recycle Bin)
1. User selects a recording, sessions, files (`.f1t`, `.f1a`, `.part`, `.damaged`, `.partial`) and presses **Delete**.
2. Confirm dialog (9.6) names the recording (or session/file) and its total size, and says it goes to the Recycle Bin.
3. HTTP: `POST /recordings/delete` with `{"target": "recording"|"session"|"file", "id": "...", "confirm_bytes": N}`.
   `confirm_bytes` must equal the current size shown; otherwise `409` ("changed, reload the list") (AC-17).
4. The DLL re-checks: the target exists under `Telemetry\recordings\`, is not loaded, is not `open`
   (an open `.part` may not be deleted: 409), and the Recycle Bin pre-check passes (3.7). Any failure: refuse, nothing changed (AC-16).
5. `SHFileOperationW` with `wFunc = FO_DELETE`, `fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT`,
   `pFrom` a double-NUL-terminated list of the absolute paths (for a recording: its files first, then its folder).
   Note: `FOF_ALLOWUNDO` is the only delete flag. No `FOF_NOCONFIRMATION`-free hard delete path exists in the code.
6. Result:
   - return value non-zero, or `fAnyOperationsAborted` set: **abort**. Report the error and the list of files still present. No fallback.
   - success: re-check that each source path no longer exists. Any path still present is reported as "not moved", and the
     job result lists them. Nothing else is attempted.
7. Index updated. The API response lists moved and not-moved paths (8.2).

### 7.9 Import of 1.0.1 sessions (offered, source left in place)
1. Scan `<module dir>\telemetry_data\` read-only. Each folder with `meta.json` is a candidate (id gives start time, 2.2; `meta.json`
   gives track (with season, 2.5), `ty`, `gen`, laps, drivers).
2. The manager lists candidates with sizes. Import only on user request (`POST /recordings/import`).
3. Import writes a new recording folder under `recordings\` named from the legacy track and start time, with each session as
   `.f1t` (flag LEGACY_IMPORT): META with drivers and counters, the completed laps from `traces.bin` (96 B samples widened
   to channels; `rx` absent), `laps.bin`, pits and incidents in META. Non-lap time is not in 1.0.1 and is not invented.
4. The source folder is not modified (AC-10).
5. Manager label: "1.0.1 import: completed laps only."

---

## 8. HTTP API changes

### 8.1 Summary
| Method | Path | Change | Guard |
|---|---|---|---|
| GET | `/recordings` | new: full manager listing with sizes (8.3). Replaces `GET /sessions` | read |
| POST | `/recordings/load` | new: load one recording or session (7.6) | local PC + `X-Telemetry-Action: load` + JSON |
| POST | `/recordings/archive` | new: archive sessions to LZMS (async, 7.7) | local PC + `X-Telemetry-Action: archive` + JSON |
| POST | `/recordings/delete` | new: Recycle Bin delete (async, 7.8). Replaces `/sessions/delete` (hard delete removed) | local PC + `X-Telemetry-Action: delete` + JSON |
| POST | `/recordings/import` | new: import one 1.0.1 folder (async, 7.9) | local PC + `X-Telemetry-Action: import` + JSON |
| GET | `/recordings/jobs` | new: job list and state | read |
| GET | `/recordings/data` | new: decoded samples of a session, paged (8.5) | read |
| GET | `/recordings/laps`, `/recordings/pits`, `/recordings/incidents`, `/recordings/trace` | new: same JSON shapes as the 1.0.1 builders (`ar_build_laps_json` `session_archive.inc:691`, `ar_build_pits_json` `:740`, `ar_build_incidents_json` `:755`, `ar_build_trace_status_json` `:869`, `ar_build_trace_data_json` `:932`), reading v2 files | read |
| GET | `/sessions?session=<id>` | kept for 1.0.1 legacy ids only (review of old folders) | read |
| `/sessions` (list), `/sessions/delete` | **removed** in API 9 | - | - |
| GET | `/trace/*`, `/telemetry`, `/health` | unchanged except `/health` gets a `storage` object (8.4) and `api_version` 9 | as today |

Guards as 1.0.1 (`web_telemetry.c:8814-8827`): local PC only (`http_peer_is_local`), a custom header (forces a preflight
this server never approves), JSON body, 64 KB cap, JSON errors.

### 8.2 Request and response examples

Archive:
```
POST /recordings/archive HTTP/1.1
X-Telemetry-Action: archive
Content-Type: application/json

{"sessions": ["Silverstone_2002_2026-10-13_10-00", "Silverstone_2002_2026-10-13_14-05"],
 "recycle_working": false}
```
Response `202`:
```json
{"job": "archive-4", "kind": "archive", "state": "running", "files": [], "errors": []}
```
`GET /recordings/jobs` later:
```json
{"jobs": [{"job": "archive-4", "kind": "archive", "state": "done",
  "files": ["Silverstone_2002_2026-10-13_10-00.f1a", "Silverstone_2002_2026-10-13_14-05.f1a"],
  "bytes_working": 167772160, "bytes_archived": 143654912, "errors": [], "seconds": 6.8}]}
```

Load:
```
POST /recordings/load HTTP/1.1
X-Telemetry-Action: load
Content-Type: application/json

{"recording": "Silverstone_2002_2026-10-13_10-00"}
```
Response `200`: `{"loaded": "Silverstone_2002_2026-10-13_10-00", "sessions": 2}`

Delete (one recording):
```
POST /recordings/delete HTTP/1.1
X-Telemetry-Action: delete
Content-Type: application/json

{"target": "recording", "id": "Silverstone_2002_2026-10-13_10-00", "confirm_bytes": 851443712}
```
Response `202` with a job; the result:
```json
{"job": "delete-5", "kind": "delete", "state": "done", "recycle_bin": true,
 "moved": ["Silverstone_2002_2026-10-13_10-00\\Silverstone_2002_2026-10-13_10-00.f1t",
           "Silverstone_2002_2026-10-13_10-00\\Silverstone_2002_2026-10-13_14-05.f1t"],
 "not_moved": [], "moved_bytes": 851443712}
```
Delete refused, Recycle Bin off for the drive:
```json
{"error": "The Recycle Bin is turned off for this drive, so deleting would be permanent. Nothing was deleted."}
```
(`503`.) Delete refused, stale size: `409` `{"error": "The recording size changed since the list was loaded. Reload the list and confirm again."}`.
Delete when Recycle Bin call fails: `500` `{"error": "The Recycle Bin did not accept the item (code <n>). Nothing was deleted from the list; check the files listed in the job."}` with the job result listing `not_moved`.

### 8.3 GET /recordings (new, schema `f1c.recordings/1`; example values)
```json
{
  "schema": "f1c.recordings/1",
  "enabled": true,
  "dir": "C:\\Games\\F1 Challenge\\Telemetry\\recordings\\",
  "total_bytes": 318767104,
  "warn_bytes": 1073741824,
  "over_warn": false,
  "notice": "Recordings are kept until you delete them. Total size: 304 MB. Warning level: 1 GB (warning only). Nothing is deleted automatically. Deleting moves files to the Recycle Bin after you confirm.",
  "loaded": null,
  "recording": {"session": "Silverstone_2002_2026-10-13_14-05", "state": "open",
                "ticks": 8421, "dropped_ticks": 0, "bytes": 2631680, "type_name": "Practice 1"},
  "recordings": [
    {"id": "Silverstone_2002_2026-10-13_10-00", "weekend": "Silverstone_2002_w1",
     "track": "Silverstone", "year": 2002, "started_local": "2026-10-13",
     "bytes": 245366784, "sessions_count": 2, "archived_count": 1,
     "sessions": [
       {"id": "Silverstone_2002_2026-10-13_10-00", "ty": 0, "type_name": "Test Day",
        "state": "closed", "bytes": 41943040, "start_local": "2026-10-13",
        "end_local": "2026-10-13",
        "files": [
          {"name": "Silverstone_2002_2026-10-13_10-00.f1a", "format": "archived", "bytes": 36700160},
          {"name": "Silverstone_2002_2026-10-13_10-00.f1t", "format": "working", "bytes": 41943040}
        ]},
       {"id": "Silverstone_2002_2026-10-13_14-05", "ty": 1, "type_name": "Practice 1",
        "state": "recovered", "bytes": 2631680, "start_local": "2026-10-13",
        "end_local": null, "recovered": {"reason": "BLOCK_CRC", "last_good_t": 1824.0, "reason_seq": 9121},
        "files": [
          {"name": "Silverstone_2002_2026-10-13_14-05.f1t", "format": "working", "bytes": 2631680}
        ]}
     ]}
  ],
  "damaged": [
    {"file": "Silverstone_2002_2026-10-13_14-05.f1t.part.damaged", "recording": "Silverstone_2002_2026-10-13_10-00",
     "bytes": 1048576, "note": "original of a repaired file, kept"}
  ],
  "partial": [],
  "legacy": {"root": "C:\\Games\\F1 Challenge\\plugins\\lightloader\\telemetry_data\\",
             "sessions": 4, "bytes": 1395864371, "importable": true},
  "dropped_ticks": 0
}
```
Sizes: each file `bytes` = bytes on disk (AC-18); session `bytes` = sum of its files; recording `bytes` = sum of its
sessions and damaged or partial files; `total_bytes` = all files under `recordings\` (the `legacy` size is shown separately, not in the total).
Example values only.

### 8.4 /health `storage` object (added)
```json
"storage": {"enabled": true, "os_ok": true, "dir": "C:\\...\\Telemetry\\recordings\\",
            "compression": "xpress", "cabinet": "loaded", "recording": "Silverstone_2002_2026-10-13_14-05",
            "ring_drops": 0, "gap_blocks": 0, "writer_lag_ms": 12, "startup_ms": 38,
            "over_warn": false, "recycle_bin": "enabled"}
```

### 8.5 GET /recordings/data (decoded samples)
Query: `session` (id, `422` if invalid), `from`, `to` (seconds, session clock), `cars` (slots, max 24), `ch` (keys, max 40), `limit` (ticks, max 4000, default 1200).
Response: `{"schema":"f1c.recordings.data/1","session":"...","from":0,"to":60,"ticks":1200,"next":1200,
"channels":["tick_ms","dist","speed"],"cars":[{"slot":3,"scope":"player","first":0,"n":1200,
"q_error_bound":{"dist":0.061,"speed":0.0009},"v":{"dist":[...],"speed":[...]}}],"gaps":[]}`.
Paged, 2 MB cap. Decoded on the HTTP thread, chunked (6.5). Never touches the writer.

### 8.6 Errors
JSON `{"error":"..."}` (`http_send_json_error`).
- `403`: not the game PC, or missing `X-Telemetry-Action`.
- `415`: content type not JSON.
- `413`: body over 64 KB.
- `404`: unknown id or file.
- `409`: stale size (AC-17); session still open; target loaded; job already running.
- `422`: invalid parameter.
- `500`: Recycle Bin call failed (8.2); nothing deleted.
- `503`: storage disabled (OS, folder); Recycle Bin off for the drive (AC-16); LZMS unavailable (archive).
- `507`: not enough free disk for an archive (checked against 1.2 x source size).

### 8.7 API version bump (handoff section 5)
1. `#define API_VERSION "9"` at `web_telemetry.c:56`.
2. `python schemas/update_version.py 9`; add schemas `f1c.recordings/1`, `f1c.recordings.data/1`, `f1c.telemetry.index/1`, and the `/health` `storage` object.
3. Update `API_CONTRACT_2026-10-09.md` (or a dated successor): new endpoints, removed `/sessions` list and `/sessions/delete`.
4. Run `contract_validate.py` (contract v9) and the gate (section 11).
5. Viewer and sidecar pins updated together (`pin_sidecars.py`, handoff section 5).

---

## 9. Console UI changes

### 9.1 Recordings manager (new page, Data group)
Replaces the 1.0.1 "Sessions & Data Manager" page (`telemetry_viewer.js:2960`). Page name: **Recordings**.
```
+----------------------------------------------------------------------------------+
| [MANAGER NOTICE, always visible] (text 9.4)                                      |
| Folder: C:\...\F1 Challenge\Telemetry\recordings\   Total: 304 MB  (warning 1 GB) |
+----------------------------------------------------------------------------------+
| Recording                         Sessions  Size      State        [x] select    |
| > Silverstone 2002 - 13 Oct 10:00   2       245 MB    1 archived   [x]           |
|     Test Day        closed    41.9 MB   archived 36.7 MB                          |
|     Practice 1      recovered  2.6 MB   (readable to 1824 s)         [ ]           |
| > Hockenheim 2002 - 12 Oct 16:04    ...                                          |
+----------------------------------------------------------------------------------+
| [Load] [Archive selected...] [Delete selected...]        Damaged/partial: 1.0 MB |
+----------------------------------------------------------------------------------+
| Old 1.0.1 telemetry (only when present): 4 sessions, 1.3 GB  [Import...] per row  |
+----------------------------------------------------------------------------------+
```
- Each recording row: name, number of sessions, total size, state summary (count of archived, recovered, damaged).
- Each session row: type, state, working size, archived size (when present), per-file sizes on expand.
- Total folder size and the warning line always shown. Sizes in MB or GB, exact bytes on hover.
- Buttons are disabled when not applicable (open session cannot be archived or deleted; loaded recording cannot be deleted until unloaded).

### 9.2 Viewer selection and session ladder (after Load)
- When a recording is loaded, the review pages show a weekend selector (that recording only) and session tabs.
- Session tabs use the game type ladder: Practice 1-4 = types 1-4; Qualifying = 5; Warm-up = 6 (**FACT**, live 2026-10-13, finding `session-type-ladder-1-7-live-20261013`);
  Race = 7; Test Day = 0 shown only when recorded (OQ-8). Other values: "Other (type n)". Names from `web_telemetry.c:8350-8356`; numbers `FACT-H`.
- A tab with no session: disabled, "not recorded".

### 9.3 Buttons
- **Load**: enabled for one recording or session. No confirm (no file change).
- **Archive selected**: enabled for closed sessions. Opens the archive dialog (9.6): working size shown, default keep working copy, option to move it to the Recycle Bin.
- **Delete selected**: enabled for recordings, sessions or files not open and not loaded. Opens the delete dialog (9.6) with name and size. Button text says "Move to Recycle Bin".
- There is no SAVE button and no snapshot.

### 9.4 Manager notice (exact English text)
Always shown on the Recordings page, in every state, and not dismissible. Text (no data state and normal state use the same sentence; the size value changes):

> **Recordings are kept until you delete them.** Total size now: <size>. Warning level: 1 GB (warning only). Nothing is deleted automatically. Deleting moves files to the Recycle Bin after you confirm.

When over the warning level, the notice also shows the text in 9.6 (warning).

### 9.5 Recording indicator
While recording: "Recording now: <session name> (<minutes> min, <size>)."

### 9.6 Other exact strings
- Over-warning line (over 1 GiB): "The recordings folder is over 1 GB (now <size>). This is a warning only: nothing is deleted automatically. Delete the recordings you no longer need from this page."
- Archive dialog: "Archive <n> session(s) of <recording name> (working size <size>) into the compact permanent format? Archived files are smaller and kept until you delete them." Checkbox: "Move the working copy to the Recycle Bin after archiving" (default off).
- Archive done: "Archived <n> session(s). Working copies <kept | moved to the Recycle Bin>."
- Archive refused (LZMS): "Archive needs the Windows compression feature (LZMS) on this PC, and it is not available. Nothing was changed."
- Delete dialog: "Move <target> '<name>' (<size>) to the Recycle Bin? You can restore it from the Recycle Bin until you empty it." Buttons: "Move to Recycle Bin" and "Cancel". No default button.
- Delete done: "Moved <n> file(s) (<size>) to the Recycle Bin." If not all: "These files were not moved: <list>. Nothing else was deleted."
- Delete refused, Recycle Bin off: "The Recycle Bin is turned off for this drive, so deleting would be permanent. Nothing was deleted. Turn the Recycle Bin on for this drive, then try again."
- Delete refused, Recycle Bin call failed: "The Recycle Bin did not accept the item. Nothing was deleted from the list. Details are in the job result."
- Stale size: "The size changed since the list was loaded. Reload the list and confirm again."
- Crash repair (shown on the recording row): "Recovered: a previous run was cut short. Readable up to <t> s. The original damaged file is kept as <name>."
- Damaged, not repairable: "Damaged file, could not be repaired. It is kept. You may delete it from this page."
- Archive interrupted: "An archive was interrupted. The working copy is intact. You may delete the partial file from this page."
- Load note: "Loaded: <recording name>. Loading does not change any file."
- Storage refused (OS): "Telemetry storage needs Windows 8 or later. Live telemetry still works; nothing is recorded." (OQ-9)
- Folder not usable: "The Telemetry folder cannot be used (<reason>). Nothing is recorded. Check the game folder permissions."
- Compression fallback (once per run): "Compression is not available on this PC. Recordings are larger but work the same. Archive is not available."
- Gaps: "<n> ticks were not recorded (writer behind). Shown as gaps." (only when > 0)
- Import label: "1.0.1 import: completed laps only."

### 9.7 Viewer changes (list)
- Replace the 1.0.1 Sessions page (`telemetry_viewer.js:2960`, `pollSessions` `:4521`, `deleteSessions` `:4528`) with the Recordings manager (9.1).
- Review pages read the loaded recording through `/recordings/*` (8.1); JSON shapes unchanged where possible.
- The `deleteSessions` text "This cannot be undone." is removed; delete text is 9.6.
- No browser decompression (NG-9).

---

## 10. Risks, abort conditions and open questions

### 10.1 Risks
- R-1 Capture rate loss. Mitigation: ring and drop policy (6.2); AC-4. Abort A-1.
- R-2 Up to 3 s lost on a crash (AC-3). Documented in the release note.
- R-3 Disk growth: everything is kept (owner decision 1). ESTIMATE 29-48 MB per 24-car hour; 1 GB warning after 3-6 weekends. Warning only (9.6).
- R-4 cabinet.dll missing or different on some Windows 8 images. Mitigation: raw fallback, archive refused (4.8). Export on Windows 8 unverified (OQ-1b).
- R-5 LZMS working set in a 32-bit process (NOT FOUND). Mitigation: 16 MB budget; archive refused if over (6.4).
- R-6 Game folder not writable (non-elevated, Program Files). Mitigation: refuse with notice (3.1). No silent fallback.
- R-7 Session-end signal (1e9) not reproducible on all session types. Mitigation: fallbacks B and C (7.2).
- R-8 AIW naming ambiguous for custom tracks. Mitigation: `Unknown` names and raw name in META (3.4).
- R-9 Channel ranges wrong for some car or track (tx/tz, pch). Mitigation: measured ranges before freeze (OQ-5); out-of-range flag (4.5).
- R-10 1.0.1 users see hard deletes replaced by Recycle Bin moves. Changed behaviour; release note.
- R-11 Permanent deletion if the Recycle Bin is off for the drive or the NukeOnDelete check is wrong. Mitigation: pre-check and refusal (3.7, AC-16); verify on owner PC (OQ-15).
- R-12 Recycle Bin size limits or user emptying the bin: outside the tool's control; the tool only reports moves.
- R-13 Mixed compression ids in one folder (working XPRESS and archived LZMS). Mitigation: per-file header and per-block flag.
- R-14 Two new threads: a lock-order bug could block capture. Mitigation: lock order (6.1), lock-free ring, storage never holds the capture lock.

### 10.2 Abort conditions (stop and report to coordinator)
- A-1 `capture_cost_us` p95 >= 500 us, or rate < 19 Hz for 30 min at 24 cars, with recording on.
- A-2 Any byte of `plugins\lightloader\telemetry_data\` changes (AC-10).
- A-3 Reader crash in fuzz (AC-9).
- A-4 Archive verification fails for any real session (AC-2).
- A-5 Any code path calls a hard-delete API, or a delete path without `FOF_ALLOWUNDO` (AC-15).
- A-6 LZMS over budget in the 32-bit build (archive unavailable).
- A-7 The 1e9 end signal is contradicted live and no other end signal works (7.2).
- A-8 A repair writes a file without verifying it, or removes an original (7.5).

### 10.3 Open questions and resolutions
- OQ-1a RESOLVED (coordinator, `compressapi.h` mingw32): compression ids XPRESS 3, LZMS 5. Was INFERENCE. Now FACT src.
- OQ-1b OPEN: `cabinet.dll` exports on Windows 8.1 and 10 (INFERENCE). Verify before freeze.
- OQ-1c RESOLVED (owner 2026-10-13): recordings are not cleared. Clear point, temp folder and disposable folder are removed from the design. Replaced by the Recordings manager (7.1-7.8).
- OQ-2 OPEN: session end signal live on Practice, Qualifying, Race, Warm-up (`FACT-H` 1e9; code only comments it).
  - 2026-10-13 live (FACT): Exit to monitor in Practice 1 = game_state 5->2, session_type unchanged, sim time keeps advancing, no 1e9 jump. Exit is NOT a session end; keep recording. Finding `exit-to-monitor-state5-to-2-not-session-end-20261013`. Session switch P1->P2 (FACT, live): session_type 1->2 and sim_time resets to ~0.4, no 1e9 jump (finding `session-switch-p1-to-p2-no-1e9-20261013`). RULE: close a session on session_type change OR sim_time drop; 1e9 is not required. Natural time expiry (FACT, live, Qualifying): race_phase 5->8, sim time keeps running, no 1e9 (finding `session-time-expiry-race-phase-8-20261013`). RULE: race_phase 8 = session complete. Race checkered (FACT, live): race_phase 5->8, no 1e9 (finding `race-checkered-race-phase-8-20261013`). Classification screen after race (FACT, live): sim_time jumps to ~1e9 (finding `classification-screen-sim-1e9-20261013`); RULE: sim_time > 1e8 = results shown, stop sampling. Quit after race (FACT, live): game_state 5->2, sim_time freezes at ~1e9, session_type/vehicle_count keep stale values (finding `quit-after-race-state2-sim-frozen-20261013`); RULE: stale values never start a recording. OQ-2 CLOSED except quit from a mid-session (no results) to main menu.
- OQ-3 OPEN (proposed: keep raw AIW string in META).
- OQ-4 OPEN: Test Day before a Practice session with the same track and season (proposed: same weekend).
- OQ-5 OPEN: channel ranges by measurement (dist, rpm, tx, tz, all pch fields). `tx/tz` NOT FOUND in this pass; decide whether to trim pch (79 values vs 40 slots).
- OQ-6 OPEN: store RESEARCH `rx[16]`? (proposed: no, as 1.0.1).
- OQ-7 OPEN: AI `gvert` excluded? (proposed: yes).
- OQ-8 OPEN: Test Day as a tab (proposed: only when recorded).
- OQ-9 OPEN: Windows 7 behaviour: live console works without storage (proposed), or whole mod refuses (Lightloader requirement)?
- OQ-10 OPEN: import converts (as 7.9), or keeps 1.0.1 read-only in place?
- OQ-11 RESOLVED (owner: SAVE concept replaced by Archive): no snapshots. An open session cannot be archived.
- OQ-12 OPEN: governance path does not follow MOD_STANDARD section 2. Coordinator to decide: move (structural, separate approved spec) or record an exception.
- OQ-13 RESOLVED (owner): all version labels become 1.1.0 (section 12, step 0).
- OQ-14 RESOLVED (owner): `MaxStorageMB` and the 3000 MB auto-delete are retired. Warning is a fixed 1 GiB (warn only, no key). Log the retired key once at start if present in `web_telemetry.ini`.
- OQ-15 OPEN: Recycle Bin pre-check: confirm `NukeOnDelete` and the drive GUID lookup on the owner PC, and that `SHQueryRecycleBinW` behaves as expected (INFERENCE).
- OQ-16 OPEN: recovery of a `.part` whose valid prefix is a single DATA block (accept, or require a minimum of 5 s?) (proposed: accept any valid block).

### 10.4 Facts vs inference vs estimate (summary)
- FACT (code): sections 2, 3.1, 4.1-4.2 structure, 4.5 bounds rule, citations in section 2.
- FACT src: compression ids XPRESS 3, LZMS 5 (`compressapi.h`, mingw32).
- FACT-H: 20 Hz, 1e9 session end, session type numbers, live rates.
- FACT-O: owner decisions 0.2 (r2).
- INFERENCE: cabinet.dll exports on Windows 8 (OQ-1b), elevated game install (3.1), Recycle Bin pre-check (3.7, OQ-15), COM for SHFileOperationW (6.1), AI gvert (4.7).
- ESTIMATE: section 5 sizes, quantization values that depend on ranges, memory budgets.
- NOT FOUND: component `AGENTS.md`; LZMS working set (6.4); pch and tx/tz ranges (4.5, OQ-5); a real recording file (none exists yet).

---

## 11. Test and gate plan (game closed)

Order: unit tests, fuzz, fallback, then owner live test. No game run or live probe in the gate.

### 11.1 Unit tests (host build in `%TEMP%`, MinGW gcc, `-Wall -Wextra -Werror`)
- U-1 Quantizer: 1,000,000 random values per channel inside range: `|v - decode(encode(v))| <= (max-min)/131070 + 1.1920929e-7 * max(|min|, |max|)`. The last term is the binary32 rounding of the decoded value (one ulp-scale term; the r1 `+ 1e-7` cannot hold for large magnitudes, e.g. rpm 25000 where half an ulp is about 0.001). Amended in batch 2 to match the test in `runtime_test.c` (record `STORAGE_V2_BATCH2_RECORD_2026-10-13.md`). Edge values min, max; NaN and inf clamp and set the flag.
- U-2 Zigzag and varint round trip for all 32-bit edge values; maximum length 5.
- U-3 CRC32 check value `0xCBF43926`.
- U-4 DATA encode/decode round trip: random ticks (1-20), car sets, masks, `first_tick`/`count`; values within bound; RAW32 exact.
- U-5 Header and table: reject bad magic, version, CRC, `header_bytes`.
- U-6 Block framing: reject bad magic, `header_crc`, seq gap, oversize, CRC mismatch.
- U-7 XPRESS round trip; raw fallback when compression does not help; mixed file.
- U-8 AIW parser: the 3.4 table (100%) plus 200 generated strings: never crash, always a non-empty name.
- U-9 Naming: `Track_Year_YYYY-MM-DD_HH-MM`, collision suffixes `_2`..`_9`, local time offset.
- U-10 Index: atomic write, reread, corrupt JSON falls back to folder scan.
- U-11 Ring under ThreadSanitizer (or Helgrind): 10^7 pushes with random stalls; drops counted exactly, no blocking.
- U-12 Repair (7.5): `.part` cut at every block boundary and at random bytes inside blocks: repaired file holds the expected block count in 100%; original kept as `.damaged`; no original removed; repaired file verified before rename.
- U-13 Archive (7.7): archive a synthetic session; decoded integers equal the working copy bit-exact (AC-2); a forced LZMS failure leaves `.f1t` untouched and `.f1a.partial` in place.
- U-14 Delete (7.8) with a `SHFileOperationW` test double: (a) success path requests `FOF_ALLOWUNDO`; (b) double returns error: nothing reported as moved, no other call; (c) `fAnyOperationsAborted`: abort; (d) Recycle Bin pre-check fails: no call to the double at all (AC-16); (e) `confirm_bytes` mismatch: 409, no call (AC-17); (f) `open` `.part` target: 409.
- U-15 Static grep (AC-15): no `DeleteFile`, `RemoveDirectory`, `_unlink`, `remove(`, `MoveFileEx` to outside `Telemetry\`, and every `SHFileOperation` call carries `FOF_ALLOWUNDO`.
- U-16 Sizes (AC-18): synthetic folder with known file sizes; `GET /recordings` sums match.
- U-17 Legacy import (7.9): synthetic 1.0.1 folder (layout from `session_archive.inc`); source SHA-256 manifest unchanged (AC-10).
- U-18 Startup order (R4): repair and scan run only after the registration event.
- U-19 Automatic removal (AC-1): full lifecycle in the host harness (record two sessions, unload, restart, archive, load) with a delete double that fails the test on any call that the test did not request.

### 11.2 Fuzz (AC-9)
- F-1 Truncate each of three reference files at every byte offset: no crash, reason code set.
- F-2 1,000,000 random mutations (bit flips, byte replacement, block swaps, length fields 0, max, max+1): no crash, no ASan/UBSan finding.
- F-3 Corpus in `%TEMP%`, seeds fixed and recorded.
- F-4 64-bit host build with ASan/UBSan; 32-bit build without sanitizers for crash-only checks.

### 11.3 Fallback without cabinet.dll (AC-8)
- B-1 Simulate load or symbol failure: writer makes raw blocks with the flag; reader reads them; archive refused with the 9.6 text.
- B-2 Compressed block with no decoder: reason `DECODER_UNAVAILABLE`, other blocks still read.
- B-3 Version stub below 6.2 (AC-11): no file created; text 9.6.

### 11.4 Soak and capture cost (AC-4, AC-5)
- S-1 Synthetic 24-car, 20 Hz, 30 minutes with recording on (host harness): ring drops 0, writer lag under 1 s.
- S-2 In-game soak (owner live test, 11.6): `capture_cost_us` p95, working set.

### 11.5 Existing gates (must still pass)
- `runtime_test.c` (46,472 checks at the 1.1.0 dev gate, handoff section 1), `viewer_test.js` (416), `contract_validate.py` (contract v9 after the bump), HTTP/SSE tests, `robustness_backend3.py` soak and fuzz.
- Sequence: handoff section 5 (`TELEMETRY_BUILD_DIR` in `%TEMP%`, `TELEMETRY_TESTS=1`, `python tools/f1c_mod.py build ...`), with sidecar pins (`pin_sidecars.py`, build, `--payload`, gated build).
- Viewer test additions: Recordings page renders the manager notice (9.4) in every state; delete dialog names the recording and size; no "cannot be undone" text remains.

### 11.6 Owner live test (after gate PASS, game closed to start)
Owner runs the game; Claude reads logs and files, does not probe memory.
1. Fresh launch: `/health` shows `storage.enabled`, `startup_ms`, `recycle_bin` enabled, `api_version` 9.
2. Record a weekend: Practice 1, Qualifying, Race at least. Check the Recordings page: sizes, state, notice text.
3. Check session end times against the 1e9 signal in the log (OQ-2).
4. Archive one closed session, keeping the working copy. Check `.f1a` exists, sizes shown, and decoded lap count matches the working copy in the viewer.
5. Archive another with "move working copy to Recycle Bin" ticked: check it appears in the Recycle Bin and can be restored.
6. Delete one recording: confirm the dialog text and size; check the files are in the Recycle Bin, not hard-deleted; restore one file from the bin.
7. Kill the game process mid-session: next start shows the recording marked recovered; check readable-up-to time.
8. Import one 1.0.1 folder: the old folder is unchanged (hash before and after).
9. Size warning: check the line only when the folder is over 1 GB (no fake files in the game folder).
10. Logs checked by Claude: capture rate, `capture_cost_us`, memory, errors, no unexpected file operations.

Stop rule: any item fails, no release; back to implement.

### 11.7 Release gating
Gate PASS and owner live PASS on the exact build hash (MOD_STANDARD section 6). A dev build passing is not a release pass.

---

## 12. Implementation order

0. Version labels to 1.1.0 (owner decision 7). Every file, current value, new value:

| File | Field | Current | New |
|---|---|---|---|
| `web_telemetry.c:49` | `TELEMETRY_VERSION` | `"4.4.0"` | `"1.1.0"` |
| `web_telemetry.c:52` | `PRODUCT_LABEL` | `"Telemetry 1.0.1"` | `"Telemetry 1.1.0"` |
| `mod.json` | `"version"` | `"4.4.0"` | `"1.1.0"` |
| `web_telemetry.dll.mod.json` | `"version"` | `"1.0.1"` | `"1.1.0"` |
| `f1c_mod.json` | `"version"` | `"4.4.0"` | `"1.1.0"` |
| `f1c_mod.json` | `build.dll` | `C:\...\Temp\f1c-web-telemetry-1.0.1\web_telemetry.dll` | the 1.1.0 build output path (per build run, under `%TEMP%`) |
| `README.md` | title line 1 | `# Web Telemetry 4.3.0-rc.1` | `# Telemetry 1.1.0` (rest of README updated with the 1.1.0 record) |
| `CURRENT_STATE.md` | header | `Private candidate **4.3.0-rc.2**` | `1.1.0` |
| `VERIFICATION_MANIFEST.json` | `"version"` | `"4.3.0-rc.2"` | `"1.1.0"`, regenerated by the gate, not hand-edited |
| `web_telemetry_usage.h` / `usage` identity | version string | `NOT FOUND` in this pass (check at implementation) | `1.1.0` |

Rules: `TELEMETRY_VERSION` and `PRODUCT_LABEL` are the only sources in the DLL; the viewer footer reads `/health` (verify). The
governance entry is updated only when the evidence changes (AGENTS rule). Historical records keep their old labels.
The shipped 1.0.1 build in `production\` is not touched.

1. Pure C core (`storage_v2.c` / `storage_v2.h` and tests): CRC32, varint and zigzag, quantizer, channel table, block framing, DATA encode/decode, bounded reader. Tests U-1..U-7, F-1, F-2.
2. Compression adapter (`cabinet.dll` loader, raw fallback, archive refusal). Tests B-1, B-2.
3. AIW parser and naming (3.4, 3.3, 3.5). Tests U-8, U-9.
4. Writer and ring (6.1-6.3), capture hook in `capture_on_game_thread` (memcpy only), removal of `ar_flush_io(0)` from the capture loop (`compatibility_runtime.inc:239`). Tests U-11, S-1.
5. Folder layout, game-dir resolution, OS gate, index (3.1-3.6). Tests U-10, B-3.
6. Jobs: repair (7.5), archive (7.7), delete via Recycle Bin (7.8), import (7.9), load (7.6). Tests U-12..U-14, U-17, U-18.
7. HTTP API (8.x), `API_VERSION` 9, schema bump (8.7). Gate contract v9.
8. Recordings page, notice and strings (9, 9.4, 9.6). Viewer test.
9. Full gate (11.5), owner live test (11.6), release record, and the step 0 label check on the release build.
10. After release: documentation records (README, CURRENT_STATE, API contract, handoff, `f1c_ida_decompiled/findings.jsonl` for new proven results). `governance\PRODUCT_STATUS.json` only when evidence changes.

Later (not 1.1.0): Export `.zip` with miniz (own spec, licence check, owner request).

---

## 13. Changelog r1 -> r2 (owner answers 2026-10-13)

- 0.2: r1 "temporary by default" replaced by the Recordings manager; SAVE removed; delete to Recycle Bin; 1 GB warn only; `MaxStorageMB` retired; version 1.1.0.
- 1.2/1.3: NG-2 (no automatic removal) and NG-3 (no hard delete) added; acceptance criteria rewritten (AC-1, AC-2 bit-exact archive, AC-3 repair marks recovered, AC-15..AC-18 delete, recycle bin, size, sizes).
- 2: 2.14, 2.15 marked as removed in 1.1.0; coordinator-checked citations kept.
- 0.3: compression ids moved to FACT src (`compressapi.h`).
- 3: layout changed to `recordings\<weekend>\` with `.f1t` working, `.f1a` archived, `.part`, `.partial`, `.damaged`; temp folder and disposable folder removed; Recycle Bin is the only delete target (3.7, pre-check).
- 4.1, 4.3: flags and header rewritten for ARCHIVED/RECOVERED; 4.3 META carries lap, pit and incident rows for the viewer.
- 4.8: working XPRESS, archive LZMS; archive refused (no silent fallback) if LZMS unavailable.
- 6: threads and lock order updated; storage writes in place; delete on jobs thread; shutdown no longer clears.
- 7: rewritten: recording (7.1), session end (7.2), unload (7.3), crash (7.4), repair marked recovered (7.5), load (7.6), archive (7.7), delete to Recycle Bin (7.8), import (7.9). Clearing removed.
- 8: endpoints rewritten: `/recordings`, `/recordings/load|archive|delete|import|jobs|data`, the viewer JSON shapes; `/sessions` list and `/sessions/delete` removed; examples with sizes; error table.
- 9: Recordings page replaces the Sessions page; SAVE removed; notice text replaced (9.4); strings updated (9.6).
- 10.3: resolved items marked (OQ-1a, OQ-1c, OQ-11, OQ-13, OQ-14); new OQ-15 (Recycle Bin pre-check), OQ-16 (minimum repair size); OQ-1b kept open.
- 11: tests rewritten for repair, archive, Recycle Bin delete (test double), automatic removal, sizes; live checklist revised.
- 12: step 0 added: version labels to 1.1.0 with every file and current value.
