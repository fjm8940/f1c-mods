# Telemetry 1.1.0 storage v2, batch 3 record (2026-10-13)

Scope: spec `STORAGE_MANAGER_SPEC_2026-10-13.md` section 12, step 6, with the channel items of 4.1, 4.5, 4.6 and 4.7 and the
batch 2 coordinator review. Spec sections edited: 4.1 (SESSION_COMPLETE bit 1), 4.5 (tolerance text), 6.1 (copy point and
jobs row). Not in this batch: HTTP API, API_VERSION, Recordings page (batches 4 and 5).
Stage: implement and verify (gated build PASS, b3 pin). Not promoted, not installed. Coordinator review pending.

## 0. Preliminary state found at start (read this first)

A previous batch 3 attempt (ended by a usage limit, see `HANDOFF_CLAUDE_2026-10-13c.md`) had already changed
`storage_v2.inc` (pch catalog 24-63), `storage_writer.inc` (tread, gear, pch, RAW32 encode, RECOVERED/ARCHIVED state,
`sw_rebuild_req`, `sj_*` forward declarations and calls) and `CURRENT_STATE.md`, but had no record, no tests and no
`storage_jobs.inc`. That tree could not compile (undefined `sj_start`). This batch kept the earlier channel code after
checking every source against `web_telemetry.c` (section 3), and built the missing jobs module. The before-state copies in
`TELEMETRY_110_BATCH3_BEFORE_2026-10-13\` are the post-batch-2 files (verified: `storage_writer.inc` has `SW_NCH 24u`).

## 1. Facts (verified by command output)

- Game closed before every gate and runtime run: `tasklist | grep -i "f1 challenge"` returned nothing (rc 1).
- Syntax check with build.bat flags (`-Wall -Wextra -Werror -Wno-unused-function`, MinGW gcc): `runtime_test.c` and
  `web_telemetry.c` compile with rc 0.
- Standalone host run of `runtime_test.exe` (built into `%TEMP%\f1c-b3-rt`, build.bat link flags) on the final sources:
  rc 0, last line `PASS 5375987 checks` (the check count varies run to run with the U-11 ring consumption: 5,027,096 to
  5,375,987 seen in this batch; the batch 2 record reports 6,653,103).
- Final full gate (background, `TELEMETRY_BUILD_DIR=%TEMP%\f1c-web-telemetry-1.1.0-b3`, `TELEMETRY_TESTS=1`,
  `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry`): **GATE7_RC=0**. Verbatim lines:
  - `PASS payload gate: staging in %TEMP%, workspace keeps no payload, exactly 5 install files, byte-exact, manifest hashes match`
  - `PASS storage v2 core: U-1..U-9, F-1, F-2, B-1, B-2`
  - `BACKEND 3 fuzz: 40 shards x 2000 iters (21082 ok, 58918 refused, 0 crashes expected)` (PASS)
  - `Built %USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-b3\web_telemetry.dll`
  - `Build verified: %USERPROFILE%\AppData\Local\Temp\f1c-web-telemetry-1.1.0-b3\web_telemetry.dll`
- Batch 3 test lines from the standalone run (`rt5.out`, same sources as the gate):
  - `PASS repair U-12: 31 cut points (block boundaries, inside blocks, after the table): recovered or left unchanged, originals kept`
  - `PASS archive U-13: LZMS bit-exact payloads (11 blocks, 199 ticks), idempotent, LZMS-off refused with working copy unchanged`
  - `PASS delete U-14: success (FOF_ALLOWUNDO), error, abort after one item, recycle off, size mismatch, open .part: no call where refused`
  - `PASS startup order U-18: no repair before registration; repaired on the jobs thread after sw_start (50 ms)`
  - `PASS channels batch 3: tread 11-14, gear 16, pch 24-63 (player) and AI tread/gear round trip through file and decoder; flags 19 not written`
  - `PASS storage jobs batch 3: U-12, U-13, U-14, U-18, channel round trip`
- Gate history (every run is listed; none was hidden). Gate 7 also shows the fuzz PASS line: `PASS BACKEND 3 fuzz: 40 shards x 2000 iters (21082 ok, 58918 refused, 0 crashes expected)`.
  - G1: rc 1, expected payload pin (`test_telemetry.py:93`, DLL `2DBD9C0C...` pinned, new `54F4A7A2...`). `pin_sidecars.py --payload` rc 0.
  - G2: rc 1 at `runtime_test.c:2489` (batch 1 test): `tb[1024]` too small for 64 catalog channels. Fixed: `tb[4096]`.
  - G3: rc 1 at U-13: archive returned "archive stopped" (stale `sj_stop_req` from the previous writer stop). Fixed: the stop flag
    applies only while the jobs thread runs (`sj_busy`). Then U-14 test targets fixed (test bug, not code).
  - G4a: rc 1, expected payload pin for the `sj_busy` DLL change. `pin_sidecars.py --payload` rc 0 (DLL `85184353...`).
  - G4b: killed by my 580 s wrapper timeout (rc 124), not a test failure; the output up to that point had no FAIL.
  - G5: rc 0, but the build line said `Build verified: ...1.1.0-b2\web_telemetry.dll`: my `sed` on `f1c_mod.json` had not
    changed `build.dll`. Fixed with an exact replacement (section 2, O-1 of batch 2 closed).
  - G6: rc 0, `Build verified: ...1.1.0-b3` (before the test pacing change).
  - G7: **rc 0**, `Build verified: ...1.1.0-b3` (final; sources in section 2).

## 2. Files

Before-state (complete copies and `SHA256_BEFORE.txt`): `workspace_organization_audit\TELEMETRY_110_BATCH3_BEFORE_2026-10-13\`
(`storage_v2.inc`, `storage_writer.inc`, `compatibility_runtime.inc`, `web_telemetry.c`, `runtime_test.c`,
`STORAGE_MANAGER_SPEC_2026-10-13.md`, `CURRENT_STATE.md`, `f1c_mod.json`, `VERIFICATION_MANIFEST.json`, `findings.jsonl`).
Notes on the before-state: `CURRENT_STATE.md` was copied before the `HANDOFF_CLAUDE_2026-10-13c` index line; `findings.jsonl` is
a prefix of the live file (1,216 lines; one line from another session was appended after that copy).

Created:
- `storage_jobs.inc` (about 640 lines): jobs thread, startup repair (7.5), archive to LZMS (7.7), delete to the Recycle Bin
  (7.8), load selection (7.6), retired `MaxStorageMB` log (OQ-14), job queue for batch 4.
- This record.

Edited:
- `storage_writer.inc`: PLAYER_PCH header flag set when a pch channel is written (4.1 bit 5, written at close); index recordings
  carry `"bytes"` (per-recording size, spec 8.3); earlier batch 3 channel code kept (section 3).
- `web_telemetry.c`: one include line `#include "storage_jobs.inc"` after `storage_writer.inc` (CRLF kept).
- `runtime_test.c`: batch 2 `t3_set_tick` now sets tread-valid bit 16, tyre wear 0.875 (AI tread 0.5 after normalization) and
  gear 4; batch 1 table buffer `tb[1024]` to `tb[4096]`; new `test_storage_jobs()` (U-12, U-13, U-14, U-18, load, index sizes,
  channel round trip), called from `main`.
- `STORAGE_MANAGER_SPEC_2026-10-13.md`: 4.1 flag bit 1 SESSION_COMPLETE; 4.5 bound amended to the 11.1 U-1 form; 6.1 rows for
  `telemetry_worker` (copy point) and `storage_jobs` (new file, whole-file read deviation).
- `CURRENT_STATE.md`: batch 3 index line above batch 2 (newest); batch 2 line no longer "(newest)". File normalized to LF
  (before-state was LF; the earlier attempt had written CRLF).
- `f1c_mod.json`: `build.dll` to `...\f1c-web-telemetry-1.1.0-b3\web_telemetry.dll` (only change).
- `VERIFICATION_MANIFEST.json`: changed only by `pin_sidecars.py --payload` (dir b3; `web_telemetry.dll` pin
  `851843530BFF50494BAB7DAF9CBCE7B5EB64EEAE22DC69D94631085FCCE49102`).
- `findings.jsonl`: appended only (section 9).

Not edited: `compatibility_runtime.inc`, `mod.json`, `web_telemetry.dll.mod.json`, `telemetry_viewer.js`, `workbench_ui.js`,
`test_telemetry.py`, `README.md`.

Source SHA-256 after this batch (LF, UTF-8):
- `storage_v2.inc` 9B7A76E68EE5E3547A586BBD1895678E703618B4F8CD85A78C792E8136DD10BC
- `storage_writer.inc` 5FD296EDE137A1952132B19D7CF896A1102B5F8588D7E75BD9A8A9D303920CF3
- `storage_jobs.inc` 1B781D9557895B858C844D9791B621C0D0FB154895D6522156197D1422E315E4
- `compatibility_runtime.inc` 3A51CBD3A1EEA6ADC12469A3461530746B2A9D8A5735C013F6507001A6AE6159 (unchanged)
- `web_telemetry.c` 5886E4E41B9B767D2CCB16D961349094A47D6162D6F6BFA1205B7DBF7B64911F
- `runtime_test.c` 774FF535C53AC9FE521A3B110FF442FD78D3CF66E7C63B37A701616D59918B09
- DLL (`%TEMP%\f1c-web-telemetry-1.1.0-b3\web_telemetry.dll`, 1,240,814 bytes) 851843530BFF50494BAB7DAF9CBCE7B5EB64EEAE22DC69D94631085FCCE49102

Line endings: `storage_v2.inc` is LF in the working tree; the batch 2 before-state was CRLF (the earlier attempt rewrote it). Not
restored, because it would change a file after the gate. Coordinator decision (O-12).

## 3. Channel source map (A)

Verified against `web_telemetry.c` in this batch. "Written" means the channel is in the block encoder (`sw_car_from`,
`storage_writer.inc`) and was round-trip tested through file and decoder.

| id | key | type | scope | source (file:line or address) | verdict |
|---|---|---|---|---|---|
| 11-14 | tread_fl/fr/rl/rr | Q16 | player | `g_telemetry.tread[w]` from `ADDR_WHEEL_THERMAL + w*1124 + WH_OFF_TREAD (0x48)` (`web_telemetry.c:493-496`, read `:5163-5167`); gate `car_valid` bit 16 set when any tread > 0 (`:5317-5318`) | Written. Matches the handoff "player tyre block +0x48" |
| 11-14 | tread | Q16 | AI | `c->tire_wear_*` from `OFF_TIRE_WEAR_FL/FR/RL/RR` = `0x363C/0x3734/0x382C/0x3924` (`:688-691`, read `:5288-5291`), normalized `(raw-0.75)*4` clamped 0..1 (same as sample builder `:2003-2009`); gate bit 16 when all four in 0.5..1.001 (`:5320-5324`) | Written. Arithmetic check: wheel block `0x358C` + stride `0xF8` + `0xB0` = `0x363C`, `0x3734`, `0x382C`, `0x3924` (handoff sec 2: AI tread +0xB0) |
| 16 | gear | RAW32 int | all | `c->gear = RI(veh, OFF_CURRENT_GEAR = 0x39FC)` (`:731`, read `:5232`); gate 0..16 (`:2029`) | Written (exact int32) |
| 19 | flags | RAW32 | all | TRF word exists only in `trace_sample_car_at` (`:1819`), for the two trace cars, computed from several sources | NOT written: no per-car source for every car (OPEN) |
| 24-27 | pch_ride_fl..rr | RAW32 f32 bits | player | `g_telemetry.pch_ride_mm[w]` (`:4526`, field `:1034`) | Written, only while `pch_valid` (`:4523`) |
| 28-31 | pch_susp_* | RAW32 f32 | player | `pch_susp_mm[w]` (`:4527`) | Written |
| 32-35 | pch_tdef_* | RAW32 f32 | player | `pch_tdefl_mm[w]` (`:4528`) | Written |
| 36-39 | pch_press_* | RAW32 f32 | player | `pch_press[w]` (`:4529`) | Written |
| 40-43 | pch_grip_* | RAW32 f32 | player | `pch_grip[w]` (`:4531`) | Written |
| 44-47 | pch_camber_* | RAW32 f32 | player | `pch_camber_deg[w]` (field `:1035`) | Written |
| 48-51 | pch_spin_* | RAW32 f32 | player | `pch_spin[w]` (field `:1036`) | Written |
| 52-55 | pch_disc_* | RAW32 f32 | player | `pch_disc_mm[w]` (field `:1036`) | Written |
| 56-59 | pch_bend_* | RAW32 f32 | player | `pch_bend[w]` (field `:1036`) | Written |
| 60 | pch_bottom | RAW32 int | player | `pch_bottom` (field `:1038`) | Written (int32) |
| 61 | pch_compound | RAW32 int | player | `pch_compound` (field `:1038`) | Written (int32) |
| 62 | pch_fuel | RAW32 f32 | player | `pch_fuel` (field `:1040`) | Written |
| 63 | pch_rpm | RAW32 f32 | player | `pch_rpm` (field `:1040`) | Written |

Dropped from the pch set (not in the 40 slots, OQ-5): `pch_press_set`, `pch_opt_c`, `pch_roll`, `pch_disc_fail_mm`,
`pch_bent`, `pch_detach`, `pch_eng_wear`, `pch_clutch_rpm`, `pch_oil_press`, `pch_bias`, `pch_drag`, `pch_lift`,
`pch_drag_used`, `pch_in_raw`, `pch_in_used`. Field line numbers are from the struct at `web_telemetry.c:1033-1042`.

Ids 20-23 (player tyre temperature) and 1-10, 15, 17, 18 are from batch 2, unchanged.

## 4. Design choices and deviations

- 4.1 Jobs thread (`storage_jobs.inc`) runs startup repair, then drains a queue of archive and single-file delete jobs. The
  queue is in place for batch 4; no HTTP call reaches it yet. Tests call the same functions directly.
- 4.2 Repair (7.5): reads the whole `.part` into memory (6.5 chunked IO not done; deviation D-1). Output = new header
  (RECOVERED | CLOSED) + original bytes up to the valid prefix + END when missing (END `seq` = valid + skipped blocks). Written
  to `.f1t.repair`, read back and verified (reason OK, closed, tick count, RECOVERED, exact length), then renamed to `.f1t`
  (no overwrite), then the `.part` is renamed to `.part.damaged`. META is not written because batch 2 has no META block
  (deviation D-2). `last_good_t` is reported.
- 4.3 Archive (7.7): each block decoded with its file's codec (XPRESS), re-encoded with LZMS. A block whose LZMS call fails
  refuses the archive (no per-block fallback to raw). A block that LZMS does not shrink is stored raw (flag 0), as the working
  format does. Read-back verification compares every raw payload, the block types, the tick counts and `t_first` with the
  working copy (AC-2). An existing `.f1a` is accepted only when it verifies; otherwise nothing is overwritten.
- 4.4 Delete (7.8): `SHFileOperationW` loaded from shell32 at run time (no new link library); flags
  `FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT`. Refusals before any call: open `.part`, Recycle Bin check
  fails, confirmed size differs by more than 0.5 bytes. Abort (return code or `fAnyOperationsAborted`) reports the files still
  present and does not try anything else. `storage_jobs.inc` has no hard-delete call (AC-15 by inspection; the U-15 grep is
  still to run in batch 5).
- 4.5 Recycle Bin pre-check (OQ-15, INFERENCE): `SHQueryRecycleBinA` answers for the drive, and the registry value
  `HKCU\...\Explorer\BitBucket\Volume\{GUID}\NukeOnDelete` is not 1. The owner PC check is still open.
- 4.6 Load (7.6): records the folder name only, after the folder is confirmed to exist and the name is free of path separators
  and `..`. No file changes.
- 4.7 OQ-14: `sj_log_retired_keys()` logs once when `web_telemetry.ini` still has `[Mod] MaxStorageMB` (called from `sw_start`).
- 4.8 Sizes: the index carries a per-recording `"bytes"` and the existing per-file `bytes`, `total_bytes` and `warn_active`
  (1 GiB, warn only). Overflow beyond 512 entries is counted only in the total (existing limit).
- 4.9 Writer change: PLAYER_PCH (bit 5) is set when a pch channel is written; it appears in the header at close only.
- 4.10 Test seams (TELEMETRY_TEST only): `sj_test_shfo` (SHFileOperationW double, moves by rename into a test bin folder),
  `sj_test_recycle` (force off/on), `sv2_test_cabinet_forced_off` (existing), `sj_busy`/`sj_stop_req` (production logic).

## 5. Tests added (runtime_test.c)

- U-12 repair: cut at the table end, at every block boundary, 7 bytes and mid-way into every block (31 cut points). Expected
  ticks from an independent block walk; original kept (byte count checked); repaired file closed, RECOVERED, exact ticks.
- U-13 archive: bit-exact payloads (11 blocks, 199 ticks), working copy byte-unchanged, idempotent second call, LZMS forced off:
  refused before any file is created.
- U-14 delete: six cases (success with FOF_ALLOWUNDO; error; abort after one item; recycle off; size mismatch; open .part), each
  checking the number of double calls.
- U-18 startup order: `.part` present, `sj_start_count` checked, no repair before `sw_start`, repaired after it (poll, 50 ms).
- U-16 partial: index contains `"bytes"`, `"warn_bytes":1073741824`, `"warn_active":false`. Sum-per-recording not checked.
- Load (7.6): accepts an existing folder, refuses `..\x` and a missing name.
- Channels: file round trip of player tread (Q16, within one step), gear (exact), pch ride/rpm (exact f32 bits), pch
  bottom/compound (exact int); AI tread (Q16) and gear; no pch on AI; bit 19 absent.
- Batch 1 test changed only in the table buffer size (section 1, G2). Batch 2 `t3_set_tick` changed (bit 16 and tyre wear).
- Runs paced one sleep per block for the batch 3 writer runs (unpaced runs overrun the 48-slot ring and drop ticks; the drops
  are counted, but the test needs whole blocks).

## 6. Gate

Procedure (spec 11.5, handoff section 5). Gate runs: section 1, G1 to G7. The final gate is G7 (rc 0). The DLL was not changed
after the pin in G4a (`web_telemetry.c` last written 13:03, `storage_jobs.inc` 13:08; the pin came after both).

## 7. Open issues and pending validation

- O-1 CLOSED: `f1c_mod.json` `build.dll` points to the b3 build (G5 showed the stale path; G6/G7 show b3).
- O-2 Import (7.9) NOT DONE: needs the META block (spec 4.3), not written in batch 2 or 3, and the 1.0.1 `traces.bin` decode.
  U-17 is therefore not run. Next batch (or batch 4 if the coordinator agrees).
- O-3 Flags 19 unwritten (no per-car source; section 3).
- O-4 pch set trimmed to 40 slots (OQ-5); ranges for pch and tx/tz still NOT FOUND, so pch is stored as RAW32 exact.
- O-5 Repair writes no META (D-2); repaired files carry the original block data only.
- O-6 Whole-file reads in the jobs thread (D-1). Spec 6.5 chunking is pending.
- O-7 S-1 host soak (batch 2) is timing-sensitive: in the standalone runs after the pacing change, one run (rt4) failed
  `dropped == 0` and two reruns passed (writer stall vs the 48-slot ring under `Sleep(1)` bursts). Drops are counted; the test
  asserts zero drops. Not changed in batch 3 (batch 2
  semantics). Coordinator decision: enlarge the ring, or assert reconciliation only.
- O-8 Recycle Bin pre-check (OQ-15) unverified on the owner PC; `NukeOnDelete` path is INFERENCE.
- O-9 Archive with a real LZMS working set (R-5, 16 MB budget) not measured; only the host call is exercised.
- O-10 Jobs thread COM initialization (6.1 INFERENCE) not done: `SHFileOperationW` runs without `CoInitializeEx`. Unverified
  on the owner PC.
- O-11 Queue is not reachable from HTTP (batch 4). `load` is not reachable from HTTP (batch 4).
- O-12 `storage_v2.inc` LF instead of batch 2 CRLF (section 2).
- O-13 `mod.json`, `README.md`: not updated (release docs at batch 5).
- O-14 U-15 static grep (no hard delete, every SHFileOperation carries FOF_ALLOWUNDO): run in batch 5 gate, not here.
- O-15 Spec 4.6 wording (trimmed to 40 slots, pch dropped fields) is in this record, not in the spec text.
- O-16 Viewer `PRODUCT_VERSION` still 1.0.1 (batch 4).
- O-17 Live behaviour (game running) not tested: owner live test (spec 11.6) after batch 5.

## 8. Documentation gate

- This record exists in the component folder and is indexed from `CURRENT_STATE.md` (batch 3 line, above batch 2).
- Facts (section 1) are separated from design (4), deviations (4, D-1, D-2), channel verdicts (3) and open issues (7).
- Findings appended to `f1c_ida_decompiled/findings.jsonl` (section 9).
- Workspace validator (`tools/workspace/check-workspace-structure.ps1`): Errors 2, both pre-existing and outside this batch
  (`Unexpected top-level directory: .claude` and `plans`); not touched here.
- Pending validation: coordinator review; the O-2 import decision; O-7 S-1 decision; owner PC checks (O-8, O-10); owner live test.

## 9. New findings (findings.jsonl)

Appended with tier, date, claim, evidence and source (see the file for the exact lines): player tread source, AI tread
source (arithmetic check), gear source, flags unsourced, pch channels as RAW32 exact, host repair/archive/delete results,
S-1 timing observation, Recycle Bin pre-check INFERENCE.

## Coordinator review (Claude, 2026-10-13)

(pending)

## Coordinator review (Claude, 2026-10-13, game closed)
- DLL SHA256 re-hashed: 851843530BFF50494BAB7DAF9CBCE7B5EB64EEAE22DC69D94631085FCCE49102 = report.
- Channel map: addresses cited, none invented. Tread player WH_OFF_TREAD 0x48 (web_telemetry.c:496, read 5163),
  AI 0x363C/0x3734/0x382C/0x3924 (688-691), gear int32 0x39FC (731, name_index FACT). Flags 19 unwritten with reason
  (no per-car source). PASS.
- Delete: SHFileOperationW + FOF_ALLOWUNDO only (storage_jobs.inc:505); no DeleteFile/RemoveDirectory/unlink in
  storage_jobs.inc or storage_writer.inc; U-14 uses test double sj_test_shfo with %TEMP% bin folder (runtime_test.c:3240).
  PASS. Real Recycle Bin pre-check unverified on owner PC (OQ-15).
- Jobs run on own thread sj_main (storage_jobs.inc:659), not capture thread. PASS.
- telemetry_data untouched: import (7.9) not implemented, so A-2 not exercised. Import + U-17 deferred (needs META, spec 4.3).
- Verdict: batch 3 ACCEPTED with deviations 1-7. Carry to batch 4: import/U-17 + META, repair chunking (6.5),
  S-1 soak flake decision (ring size vs zero-drop assertion), U-15/U-16. Owner live test still required.
