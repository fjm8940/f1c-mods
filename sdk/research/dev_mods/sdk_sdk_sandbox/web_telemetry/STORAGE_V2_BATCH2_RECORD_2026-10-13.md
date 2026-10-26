# Telemetry 1.1.0 storage v2, batch 2 record (2026-10-13)

Scope: spec `STORAGE_MANAGER_SPEC_2026-10-13.md` section 12, steps 4 and 5, plus the U-1 tolerance amendment and the session-boundary rules from the 2026-10-13 live FACTs. Nothing else.
Stage: implement and verify (gated build PASS). Not promoted, not installed. Coordinator review pending.
Not in this batch: archive (7.7), repair and recovery (7.5), load (7.6), delete to Recycle Bin (7.8), import (7.9), jobs thread, HTTP API (8.x), Recordings page (9.x), API_VERSION change, pch channels (4.6), META block (4.3).

## 1. Facts (verified by command output)

- Game closed before every gate run: `tasklist | grep -i "f1 challenge"` returned nothing (exit 1). Checked before the first build, before the re-pin, and before the final gate.
- Batch-2 host run (no-argument `runtime_test.exe`, built with the build.bat flags, 32-bit mingw gcc 15.2.0, `-Wall -Wextra -Werror`): exit 0, last line `PASS 6653103 checks`. Batch-2 lines, verbatim:
  - `PASS session replay: 7 live sequences + stale, reset and invalid cases`
  - `PASS index U-10 and OS gate B-3 (6.1 refused, Telemetry file refused, folder layout)`
  - `PASS ring U-11: 10000000 pushes, 6603753 consumed, 3396247 dropped (counted exactly, no blocking)`
  - `PASS writer never blocks capture: slow disk 50 ms/block, 2000 ticks in 3.5 ms, max tick 0.007 ms, 1960 dropped (counted, GAP written)`
  - `PASS S-1 soak (host, paced one sleep per block): 36000 ticks, 24 cars, 0 dropped, producer 28122 ms, max tick 0.355 ms`
  - `PASS storage writer batch 2: U-10, U-11, B-3, S-1, slow-disk, session replay`
- Batch-1 tests still pass in the same run (`PASS storage v2 core: U-1..U-9, F-1, F-2, B-1, B-2`).
- The gate's `serve` run executes the same full test set, but `read_first_pass_line` reads only the first PASS line, so the batch-2 lines are not in the gate log. The gate exit code is the evidence that they ran (a failing CHECK exits 2 and kills the run).
- Live FACTs used (findings.jsonl, cited by id in the code comments): `session-type-ladder-1-7-live-20261013`, `exit-to-monitor-state5-to-2-not-session-end-20261013`, `session-switch-p1-to-p2-no-1e9-20261013`, `session-time-expiry-race-phase-8-20261013`, `race-checkered-race-phase-8-20261013`, `classification-screen-sim-1e9-20261013`, `quit-after-race-state2-sim-frozen-20261013`.
- Addresses used (already in the DLL): session_type 0x007C56A4, race_phase 0x007C56A8, vehicle_count 0x005C329C, session_time 0x008AFEA0 (`web_telemetry.c` mirror table). No game_state address exists in this component (see 4.3).

## 2. Files

Before-state copies (complete, relative paths kept): `workspace_organization_audit\TELEMETRY_110_BATCH2_BEFORE_2026-10-13\`
(`web_telemetry.c`, `compatibility_runtime.inc`, `runtime_test.c`, `test_telemetry.py`, `STORAGE_MANAGER_SPEC_2026-10-13.md`, `VERIFICATION_MANIFEST.json`, `web_telemetry.dll.mod.json`, `mod.json`, `f1c_mod.json`, `CURRENT_STATE.md`, `SHA256_BEFORE.txt` with the SHA256 of the three code files before this batch).

Created:
- `storage_writer.inc` (new, about 900 lines): ring, session state machine (pure), car mapping, OS gate and folder layout, index writer and reader, file writer (header, DATA blocks, GAP, END, rename), writer thread, start and stop. Included from `web_telemetry.c` after `session_archive.inc`.
- This record.

Edited:
- `web_telemetry.c`: one include line `#include "storage_writer.inc"`.
- `compatibility_runtime.inc`:
  - capture loop: `sw_capture_tick(GetTickCount())` after `ar_tick_locked` (under `g_telemetry_cs`, no IO); `sw_capture_idle` in the stale branch.
  - `ar_flush_io(0)` removed from the capture loop (see section 3).
  - `sw_start()` after broker registration and HTTP thread creation (spec 6.6).
  - `sw_stop(5000)` after the capture loop ends (spec 6.7).
  - `ar_init(path)` replaced by `sw_root_init_from_exe()` (path and folder check only).
- `runtime_test.c`: `b2_*`, `t3_*` helpers, `test_storage_writer()` (session replay, U-10, B-3, U-11, slow disk, S-1), call added in `main` after `test_storage_v2()`. Also the test act-string helper.
- `STORAGE_MANAGER_SPEC_2026-10-13.md`: U-1 tolerance amended (11.1) to `(max-min)/131070 + 1.1920929e-7 * max(|min|,|max|)` with the reason. Section 4.5 (line 370) still shows the r1 bound; not amended here (coordinator to decide).
- `CURRENT_STATE.md`: batch-2 index line added above batch 1; batch 1 keeps its line without "(newest)".
- `VERIFICATION_MANIFEST.json`: changed only by `pin_sidecars.py --payload` (payload dir `%TEMP%\f1c-web-telemetry-1.1.0-b2\payload`, `web_telemetry.dll` pin 87AAA25B... -> 2DBD9C0C...). No other edit.

Rebuilt by the tool (not hand-edited): `generated\*` (timestamps 12:27, from the build step; `mod.json` unchanged).

Not edited: `test_telemetry.py` (gate ran unchanged), `f1c_mod.json`, `mod.json`, `web_telemetry.dll.mod.json`, `telemetry_viewer.js`, `workbench_ui.js`, `storage_v2.inc`, `session_archive.inc`.

Test artifacts: the host tests wrote to `%TEMP%\f1c-b2-*` only (disposable, not cleaned; agents do not delete). Nothing was written to the game folder, `telemetry_data`, `production\` or the workspace root.

Restoration: restore the six edited files from the before-state folder; remove the include line and the hook lines; `storage_writer.inc` is new and should go to the owner's quarantine if the coordinator rejects this batch (no hard delete by agents).

## 3. Design choice: the 1.0.1 archive writer (option "superseded")

Choice: the 1.0.1 archive writer is superseded, not moved to a helper thread.
- `ar_flush_io(0)` is removed from the capture loop. `ar_init` is no longer called, so `g_ar_root` stays empty and every 1.0.1 archive path (`ar_tick_locked`, `ar_on_*`, `ar_flush_io`) returns at its first check. No file write and no memory growth from the 1.0.1 archive.
- Reason: spec 3.2 makes `<module dir>\telemetry_data\` read-only history (import source). Abort A-2 says any byte change there stops the batch. Moving the same writer to a helper thread would still write into `telemetry_data`.
- The 1.0.1 code stays compiled; its routes and tests still run (gate: `PASS 1.0.1 session archive routes`). `/sessions` will list no new sessions until batch 6-7 replace it.

## 4. Implementation notes and deviations

4.1 Tick path. Spec 6.1 says the game-thread callback copies the tick. The code's real capture loop is `telemetry_worker`, which calls `read_telemetry()` under `g_telemetry_cs`. The ring copy happens there (`sw_capture_tick`), after `read_telemetry`, with no IO and no allocation. The game-thread callback is unchanged.

4.2 Ring. 48 slots (spec 6.2 says 40 ticks). Control events (open, close, complete, shutdown) share the ring. Samples are refused when fewer than 8 slots are free, so control events have reserve space. A refused sample increments `dropped`; the writer writes a GAP before the next applied sample, and a final GAP before END. Control events refused: counter `control_lost` (must stay 0; asserted in U-11).

4.3 game_state. The DLL reads no game_state address (none in this component). The rule "5 -> 2 is not a session end" holds by construction: nothing reads or closes on it. If a later batch needs it, a verified address is required first.

4.4 Session rules, as implemented in `sw_sm_step` (pure):
- Opens only when sim time advances or drops (reset), sim < 1e8, at least one car, and session_type in 0..7.
- Closes on: session_type change (reason T), sim drop (reason D), sim >= 1e8 (reason R, results, sampling stops), no sim change for 30 s while open (reason S, from the capture loop or the idle branch).
- race_phase 8 marks the session complete (header flag bit 1, `SW_SESSION_COMPLETE`); recording continues until sim >= 1e8.
- Quit after results: sim frozen above 1e8 never opens; a new session opens when the sim drops below 1e8 with cars present.

4.5 Channels written. Catalog ids 1 (dist from `lap_dist`), 2-4 (speed, throttle, brake, when `car_valid` says valid), 5 (steer, AI only), 6-8 (glat, glon, gvert, when the acceleration bit is valid), 9-10 (yaw, yaw_rate), 15 (rpm), 17-18 (track_x, track_z), 20-23 (tyre temperatures, player only). Not written in batch 2: 11-14 tread (no per-car source in `car_telemetry_t`; `g_telemetry.tread` is player-only and not wired), 16 gear and 19 flags (not wired), 24-63 pch (spec 4.6, later). The channel table still lists all 24 catalog entries.

4.6 Blocks. DATA block = 1 s (20 ticks) or a change in the car set or masks, whichever is first (so each car's ticks are contiguous, as the scanner requires). Values quantized with the batch-1 Q16 quantizer; `dt_ms[0] = 0`.

4.7 Naming. The folder is `recordings\<Track>_<Year>_<YYYY-MM-DD_HH-MM>\` named from the first session's open time and AIW (`Unknown_0000` when the AIW is not valid yet). The file is `<stem>.f1t.part` while open and is renamed to `<stem>.f1t` at close. At close the track is re-read from the closing AIW when it is valid (spec 3.4 says the name is fixed at close). Weekend key comes from `sv2_weekend_next` (new folder on track, year or weekend change).

4.8 Header. Batch 2 writes the header at open and rewrites it at close (CLOSED flag, optional SESSION_COMPLETE bit 1, track, session name). Bit 1 is new and is not in the spec 4.1 flag list (see open issue O-6).

4.9 Index. `index.json` is written atomically (`index.json.tmp`, then `MoveFileExA` with replace), at startup, on each open and close, and at most every 5 s. Fields follow spec 3.6 (schema, updated_utc, warn_bytes 1 GiB, total_bytes, recordings, files). A cache only: when it is corrupt, a folder scan rebuilds it (U-10).

4.10 Startup folder scan. A leftover `.part` from an earlier crash is listed as `damaged` and is not repaired (7.5 is batch 6). Its state name is a batch-2 choice, not the 7.5 meaning.

4.11 OS gate and folder (spec 3.1). `RtlGetVersion` must report 6.2 or later. Path length must be under MAX_PATH - 64. A file named `Telemetry` in the game folder is refused, not renamed. The folder is created in `rfe_1d3b7a91` (path work, before registration); the writer thread starts after registration.

4.12 Test seam. `sw_test_write_delay_ms` (TELEMETRY_TEST only) adds a Sleep to every file write. This is a synthetic slow disk, not a slow volume.

4.13 S-1. The 30-minute case is 36000 ticks, 24 cars, with one `Sleep(1)` per 20 ticks, so the wall time is about 28 s, not 30 minutes. This is a host pacing test, not the in-game soak (11.6, owner).

## 5. Tests added (runtime_test.c)

- Session replay (`t3_session_replay`): the seven live sequences as synthetic streams, plus frozen-stale (30 s rule), sim reset within one type, no cars, invalid type. Checks the exact close, open, sample and complete actions for every step (87 steps in total).
- U-10 index and B-3 (`t3_u10_index_and_os_gate`): atomic write and reread; half-file corrupt cache refused; folder scan rebuild; no temp file left; OS 6.1 refused with nothing created; a file named Telemetry refused.
- U-11 ring (`t3_u11_ring`): 10^7 pushes with a producer thread and random stalls; order preserved; consumed + dropped == pushed exactly. Not run under ThreadSanitizer or Helgrind (not available here, see O-9).
- Writer never blocks capture (`t3_slow_disk`): 2000 ticks with a 50 ms fake disk per write; producer 3.5 ms total, max 0.007 ms per tick; drops counted; the file reads back with `sv2_scan` (closed, ticks + END dropped reconcile exactly).
- S-1 (`t3_s1_soak`): 36000 ticks, 24 cars, real temp file; 0 drops; the file reads back closed with 35999 samples.

## 6. Gate

Procedure (spec 11.5, handoff section 5): `TELEMETRY_BUILD_DIR=%TEMP%\f1c-web-telemetry-1.1.0-b2`, `TELEMETRY_TESTS=1`, `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry`.

1. First build: rc 1 at `test_telemetry.py:93` (expected payload pin, DLL changed): `AssertionError: ('web_telemetry.dll', '87AAA25B713CD7A288D9BF3E4EF560B72923D2EF6E3776ECDE3C1E53003CDC1C', '2DBD9C0C5E9A4CD7BE12570D96A12EA92556B52FD77233B48BE2C970A9001A6B')`.
2. `pin_sidecars.py --payload "%TEMP%\f1c-web-telemetry-1.1.0-b2"`: rc 0 (manifest changed as in section 2).
3. Final build, run in the background with rc captured to the log:
   - `PASS payload gate: staging in %TEMP%, workspace keeps no payload, exactly 5 install files, byte-exact, manifest hashes match`
   - `PASS storage v2 core: U-1..U-9, F-1, F-2, B-1, B-2`
   - `PASS served viewer sidecar Node analysis test: VIEWER PASS 416 checks`
   - `PASS contract: schemas validate every live document, SSE event and error; api_version 8 pinned`
   - `PASS 1.0.1 session archive routes: list, unknown session 404, delete guards, health`
   - `AUDIT MODEL 22/22 PASS`
   - `PASS BACKEND 3 soak: 20 workers (logical CPUs 20), 2.0h per shard, wall 16.9s, cpu 75.4%, flat rss (max warm delta 0 KB), laps>=50/side, evictions<=8`
   - `PASS BACKEND 3 fuzz: 40 shards x 2000 iters (21082 ok, 58918 refused, 0 crashes expected), wall 17.3s, cpu 17.9%, corpus %USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-import-fuzz-w96r7kh5`
   - `Built %USERPROFILE%\AppData\Local\Temp/f1c-web-telemetry-1.1.0-b2\web_telemetry.dll`
   - `Build verified: %USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.0.1\web_telemetry.dll` (see O-1: this line names a different DLL)
   - `GATE_FINAL_RC=0`

DLL: `%TEMP%\f1c-web-telemetry-1.1.0-b2\web_telemetry.dll`, 1,215,023 bytes, SHA256 `2DBD9C0C5E9A4CD7BE12570D96A12EA92556B52FD77233B48BE2C970A9001A6B` (the gate's payload pin).

## 7. Open issues and pending validation

- O-1 `f1c_mod.json` `build.dll` still points to the 1.0.1 build path (batch 1 deferred it, spec step 0 requires the 1.1.0 path). The tool therefore prints `Build verified:` for the stale 1.0.1 DLL. The payload gate and tests ran on the 1.1.0 build, but the line is misleading. Coordinator decision.
- O-2 OQ-2 remains open for quit from a mid-session to the menu (no results screen). The 30 s stale rule closes it, but it has not been live-tested.
- O-3 game_state is not read (see 4.3).
- O-4 Channels 11-14, 16, 19 and pch are not written (see 4.5).
- O-5 No META block (4.3), no lap rows, no repair (7.5), no archive (7.7), no load (7.6), no delete (7.8), no import (7.9). Leftover `.part` files stay unrepaired until batch 6.
- O-6 Header bit 1 (`SESSION_COMPLETE`) is new; spec 4.1 needs the flag added.
- O-7 Spec 6.1 wording says the game-thread callback copies the tick; the implementation copies on `telemetry_worker` (4.1). Spec wording to be updated.
- O-8 Ring is 48 slots, not 40 (4.2).
- O-9 U-11 was not run under ThreadSanitizer or Helgrind (not available on this machine). Exact-count stress only.
- O-10 The S-1 soak is host-paced (4.13). The owner live test (11.6) is still required.
- O-11 `MaxStorageMB` retired-key log line (OQ-14) not implemented (batch 6).
- O-12 `/sessions` HTTP still reads the 1.0.1 archive (empty for new sessions) until batch 6-7.
- O-13 Spec 4.5 line 370 still has the r1 tolerance text (section 2).
- O-14 Batch-1 open item: viewer `PRODUCT_VERSION` 1.0.1 (`telemetry_viewer.js:32`) still open (batch 4).
- O-15 Spec 3.3 lists a track-key and generation boundary (PROPOSAL). Batch 2 implements only the owner-listed rules (type change and sim drop). Generation change is not a close trigger here.
- O-16 README.md is not updated. CURRENT_STATE.md carries the index line.

## 8. Documentation gate

- This record exists in the component folder and is indexed from `CURRENT_STATE.md`.
- Facts (section 1) are separated from design choices (3), deviations (4) and open issues (7).
- Pending validation: coordinator review; the owner live test (11.6) is not part of this batch; O-1 and O-6 decisions.


## Coordinator review (Claude, 2026-10-13)

- Accepted: 1.0.1 archive superseded (ar_init off, no capture-thread IO), ring 48 slots, copy on telemetry_worker, SESSION_COMPLETE header bit (spec 4.1 entry to add).
- O-1 fixed by Claude: f1c_mod.json build.dll now points at %TEMP%1c-web-telemetry-1.1.0-b2 (before-state in TELEMETRY_110_BATCH1_BEFORE_2026-10-13). Update per batch until release.
- Not accepted as final: tread ids 11-14 and gear 16 unwritten. Tread exists per car (web_telemetry.c tyre block +0x48 'tyre tread remaining'; AI tread +0xB0 per handoff section 2) and gear is a captured channel. pch ids 24-63 (player chassis) also required by owner. Carried to batch 3.
