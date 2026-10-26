# RECREV: can the owner's real recordings open in review mode? (2026-10-14)

Product: Telemetry (`web_telemetry`), version 1.2.0, API 10. Author: Matias.
Stage: verify (read-only diagnostic of the R1-B review adapter on real files). No product code changed.
Tags: FACT = observed in this run (command output or file bytes). INFERENCE = reasoned, to verify. OPEN = owner or Claude decision.

## 0. Answer

- **Yes, all 7 real closed recordings opened.** Every `/recordings/review` answered `ready` (no `error`, no 404), and `/laps`, `/trace/status`, `/trace/data` (where timed laps exist), `/telemetry` and `/?session=` all answered 200 (FACT, section 1).
- Earlier 503 "review index is building" answers were the designed cold-build gate, caused by a 2-slot LRU index cache (section 3, RC-3). They are not failures.
- Issues found (none blocks opening): review `laps` counts index rows, not listed laps (RC-1); `/recordings` shows `ticks: 0` for v2 sessions (RC-2); 4 of 7 files hold no lap EVENT, so they have no timed laps (section 4, FACT on the bytes); no driver or car names in the v2 index (RC-4).
- Serve run stopped. Game was not running at start (`tasklist` printed nothing) and at stop.

## 1. Result table per session (FACT)

Fixture: 6 newest AUSTRALIA folders copied by `cp -r` from `F1 Challenge V10\Telemetry\recordings\` into `%TEMP%\RECREV_2026-10-14\game\Telemetry\recordings\` (93 MB) plus `Telemetry\index.json`. Server: `runtime_test.exe serve` with `RECREV_ROOT`, port 18090 (own ini, not 8080), `TELEMETRY_PREVIEW=1`. Bodies saved in `%TEMP%\RECREV_build\rr\`.

| # | review_id | folder / file (f1t) | type | review state (after first build) | review `laps` | /laps: cars with timed laps / timed laps | /trace/status | /trace/data (car=1000+slot, lap) | /telemetry | `/?session=` |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | v2_aa2747e9afcb611d | AUSTRALIA 1997 - 2026-10-14 20-54 / Race | 7 | ready | 0 | 0 / 0 | 200 (mode archive, 24 cars) | n/a, no timed lap | 200 | 200, 1719 B |
| 2 | v2_ba9b0e03b2d73f58 | AUSTRALIA 1997 - CO-OP 19-12 / Practice 1 | 1 | ready | 103 | 19 / 68 | 200 | 200, car 1000 lap 1: count 1411 = status 1411; lap 99999: 404 | 200 | 200, 1719 B |
| 3 | v2_415c44ef288612d2 | AUSTRALIA 1997 - CO-OP 19-57 / Practice 1 | 1 | ready | 0 | 0 / 0 | 200 | n/a | 200 | 200, 1719 B |
| 4 | v2_0700c29dcf17706a | AUSTRALIA 1997 - CO-OP 20-00 / Practice 1 | 1 | ready | 68 | 13 / 43 | 200 | 200, car 1000 lap 1: count 1424 = status 1424 | 200 | 200, 1719 B |
| 5 | v2_34c4aad20c99c576 | AUSTRALIA 1997 - CO-OP 20-16 / Race (20-17 file) | 7 | ready | 395 | 23 / 372 | 200 | 200, car 1000 lap 1: count 1501 = status 1501 | 200 | 200, 1719 B |
| 6 | v2_8b49210edb297d0a | AUSTRALIA 1997 - CO-OP 20-16 / Warmup | 6 | ready | 1 | 0 / 0 | 200 | n/a | 200 | 200, 1719 B |
| 7 | v2_80e852ca68a331d1 | AUSTRALIA 1997 - Race 20-14 / Qualifying | 5 | ready | 0 | 0 / 0 | 200 (26 cars) | n/a | 200 (26 cars) | 200, 1719 B |

Notes on the table:
- `/recordings` lists 7 closed sessions with review ids; no damaged or partial entries (FACT, `full_run1.txt` and `recordings.json`).
- `/trace/data` `count` equals the status sample count and the `samples` array length in all 3 tested laps (FACT).
- `/trace/data` for a lap not recorded answers 404 `lap not recorded in this session` (FACT).
- `/telemetry` `review` object present in all 7: `{session, stem, ticks, end_t}` (FACT).
- Page `/?session=ID` returns 200 with 1719 bytes for every id; I did not compare contents (FACT on size only).
- Full raw outputs: `%TEMP%\RECREV_build\rr\full_run1.txt` and `pass2.txt`.

## 2. Pass sequence and what the 503s were (FACT)

- First query round: `/trace/status` answered `503 {"error":"review index is building"}` for all 7 ids in a row (a loop had just evicted them).
- A retry of the same request answered 200 within 1 to 2 s. The review route itself showed `state:"building"` on the first poll and `ready` on the next (pass2: `review-poll`).
- `built_ms` values: 0 to 453 ms.

## 3. Root causes and file:line

RC-0 (no failure, expected behaviour): cold builds return 503 while the index builds.
- `storage_api.inc:1157`: `if (slot < 0 || state == SA_REV_BUILDING || state == SA_REV_NONE) { ... "review index is building"; return 503; }` (in `sa_rv_ready`, line 1147 onward).

RC-3 (design limit, shows up when switching sessions): only 2 index slots, LRU.
- `storage_api.inc:551` `#define SA_REV_SLOTS 2u`.
- `storage_api.inc:690-705` picks a free slot or evicts the least recently used one; each eviction means a new cold build. Seven sessions requested in turn = a rebuild per switch.
- Impact for a timeline viewer that switches between sessions: a 503 window of up to about 0.5 s per switch (INFERENCE from built_ms values; to verify under load).

RC-1 (count mismatch): review `laps` counts index rows; `/laps` lists only timed laps.
- `storage_api.inc:642` and `:743` (`nlaps = ix.nlaps` / `sa_rev[slot].nlaps`), output at `:750-752` (`"laps":%u`).
- Listing filter: lap rows without a game time or start are dropped (R1-B record D-R1B-3; loop at `storage_api.inc:942`).
- FACT: session 2 review `laps` 103 vs 68 timed; session 4: 68 vs 43; session 5: 395 vs 372; session 6: 1 vs 0.
- Fix proposal (not applied): either report the timed count in `laps`, or add `timed` next to it. Owner or Claude decision (OPEN).

RC-2 (listing `ticks` always 0 for v2): `/recordings` shows `"ticks":0` for every v2 session, while `/telemetry` shows real ticks (for example 4095 for session 1).
- Output format at `storage_api.inc:209` (`\"ticks\":%u`); the caller path was not traced (INFERENCE: v2 path passes 0 for ticks). To verify.

RC-4 (names): no driver or car names in the v2 index. Index and frames use the generated name `"Car %u"`.
- `storage_api.inc:940` (`"n":"Car %u"`), `:967`, `:1059` (`"driver":"Car %u"`), `:1113`.
- FACT: raw-file search over all 7 copied files for 15 strings (Villeneuve, Schumacher, Hakkinen, Coulthard, Barrichello, Ferrari, Williams, McLaren, Benetton, Hill, Berger, Hunt, Mansell, "Car 0", "Car 1") gave 0 hits in every file. This checks ASCII only.
- INFERENCE: names are not stored as plain text in the v2 file. Whether they sit in binary fields is not checked (OPEN for timeline-playback design).

RC-5 (API note, not a bug): `/trace/data` needs `car=1000+slot` for recorded sessions.
- Contract line `API_CONTRACT_2026-10-09.md:523`. Bare slot gives 400 `car=1000+slot and lap=N are required` (`storage_api.inc:67-68`). My first run made this mistake; it was corrected in pass 2.

RC-6 (zero timed laps): 4 files have no timed laps. The raw bytes show why (FACT, `rr_events.py` output):

| file | size | `"type":"lap"` events | index laps |
|---|---|---|---|
| Race 20-54 (session 1) | 1,143,700 | 0 | 0 |
| Practice 19-57 (session 3) | 1,105,519 | 0 (3 pit events) | 0 |
| Warmup 20-16 (session 6) | 1,146,328 | 1 | 1 row, 0 timed |
| Qualifying 20-14 (session 7) | 83,709 | 0 | 0 |

- FACT: lap events in raw = index rows in sessions 2, 4, 5 (103 = 103, 68 = 68, 395 = 395).
- FACT: session 1 is a 282 s race (`session_end` at t 282.276, `reason` 5) with 4095 ticks and zero lap EVENTs.
- INFERENCE: the game or writer did not emit lap events for session 1. Not a reader bug, since the index matches the raw events in the other sessions. Needs an owner check or a look at the writer (`storage_writer.inc`). (OPEN)

## 4. Channels, sessions, time span, cars (FACT unless tagged)

Session times (`session_start` / `session_end` JSON, sim seconds):

| session | type | start t | end t | ticks (review) | end reason |
|---|---|---|---|---|---|
| 1 Race 20-54 | 7 | 0.000 | 282.276 | 4095 | 5 |
| 2 Practice 19-12 | 1 | 17.905 | 1075.126 | 15927 | 5 |
| 3 Practice 19-57 | 1 | 17.415 | 98.108 | 1234 | 5 |
| 4 Practice 20-00 | 1 | 17.182 | 730.607 | 10816 | 5 |
| 5 Race 20-17 | 7 | 0.000 | 2088.743 | 31520 | 4 |
| 6 Warmup 20-16 | 6 | 17.019 | 103.634 | 1306 | 1 |
| 7 Qualifying 20-14 | 5 | 0.444 | 5.877 | 83 | 5 |

- Track: `AlbertPark` in every session.
- Session type codes: 1 practice, 5 qualifying, 6 warmup, 7 race (INFERENCE from the names; the codes are not decoded in the record).
- `end reason` values are not decoded (OPEN).

Channels and players (FACT from `/trace/status`):
- `channels_seen` = 63487 (0xF7FF, bit 11 clear) for every car in all 7 sessions. Mapping bits to the 19 short trace keys: not done (OPEN).
- AI flags: 23 AI + 1 non-AI car in sessions 1, 2, 3, 4, 6; session 7 has 25 AI + 1; session 5 (CO-OP race) has 17 AI and 7 non-AI cars.
- `remote` flag = 0 for every car in every session. So the CO-OP remote players are not marked. INFERENCE: the 7 non-AI cars in session 5 are remote humans, but the index does not flag them (OPEN).
- Cars: 24 slots per session; 26 in session 7.

Blocks: block count per file not measured (not done).

## 5. Harness change (test-only, runtime_test.c)

- Before-copy: `%USERPROFILE%\AppData\Local\Temp\RECREV_before\runtime_test.c` (sha256 `af55d4cda61de7d680468668385794ccf364b22fd9f7b39abf556e44910da0e9`, byte-identical to the component file before the edit).
- Change: in `serve_real_recording()` (function starts line 4640), a new block at lines 4644-4659. If env var `RECREV_ROOT` is set, it calls `sw_root_init(root, 6, 2, ...)` on that game dir (a trailing backslash is added when missing), sets `sw_enabled=1`, `sa_init()`, resets `sw_nentries`/`sw_overflow_bytes`, runs `sw_scan_folders()` and `sa_snapshot_rebuild()`, then returns. Without the variable the original synthetic-fixture path runs unchanged (anchor `b2_base` now at line 4660).
- Line endings: CRLF kept. Before 4724 CRLF / 4724 LF; after 4740 CRLF / 4740 LF (Python byte count).
- Compile: exit 0 with the build.bat flags (exe and DLL), output to `%TEMP%\RECREV_build\`. The gcc exe line was copied as specified; the DLL line is the build.bat DLL line.
- Harness-only; no product code changed. Not in any gate. Owner can revert with the before-copy.

## 6. Serve run facts (FACT)

- Full test suite before serve: `PASS 9006054 checks`; the `serve` loop then ran (default 12 s, `TELEMETRY_PREVIEW=1` gives 180 s).
- Serve PID 15192 (the only runtime_test.exe); terminated by PID with `taskkill //PID 15192 //F`. No runtime_test left.
- Log: no FAIL line (`serve.log`, 30 lines).
- Game: `tasklist | grep -i "F1 Challenge"` printed nothing before start; not checked again after stop (OPEN, owner's game may have been started during the run; I did not check).

## 7. Restoration and what was not changed

- Real recordings: read-only. Only `cp -r` copies were made into `%TEMP%`. No file under `F1 Challenge V10\Telemetry` was modified, moved or deleted.
- Fixture kept (disposable, `%TEMP%`): `%TEMP%\RECREV_2026-10-14\`. Build: `%TEMP%\RECREV_build\` (includes `tools\*.py` scripts and `rr\*.json` response bodies).
- No product code changed. No `api1.txt` read.

## 8. Not done

- Block count per file.
- Decoding `channels_seen` bits to the 19 trace keys.
- Driver and car names in binary fields (ASCII search only).
- Session end-reason and session-type code tables.
- `test_telemetry.py` v2 cases (the gate was not run).
- Rendering or content check of the `/?session=` page (size only).
- Performance (AR-5), and the live-game state after the run.
- Cause of RC-2 in the caller (INFERENCE only).
- Cause of RC-6 session 1 in the writer (INFERENCE only).

## 9. Follow-ups (OPEN, for Claude)

1. Decide RC-1: report timed laps in `laps` or add a field (`storage_api.inc:750-752`).
2. Check RC-2 (`storage_api.inc:209` caller) and whether `ticks` in `/recordings` should be filled for v2.
3. Check RC-6 session 1 (Race 20-54): did the game emit lap events? Ask owner what happened in that race.
4. Decide if 2 index slots (`storage_api.inc:551`) are enough for the timeline viewer, or raise it.
5. Names: check binary fields in v2 for driver names before designing timeline labels.
6. Documentation: this file is not yet indexed in `CURRENT_STATE.md` (owner rule). Claude to add the index line.
