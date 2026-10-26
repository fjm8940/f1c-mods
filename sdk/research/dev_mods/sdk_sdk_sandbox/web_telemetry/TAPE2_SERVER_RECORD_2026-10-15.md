# TAPE2 server record (T2-S, names N1-N3), 2026-10-15

Owner: Matias. Spec: TAPE2_SPEC_2026-10-14.md (sections 0, 2, 4). Handoff: HANDOFF_TAPE2_2026-10-15.md.
Scope: storage_api.inc, web_telemetry.c (helpers only), runtime_test.c, test_telemetry.py. Nothing installed, nothing gated.
Before-copies of the files as they were before the previous T2-S worker edited them: %TEMP%\TAPE2_before_2026-10-14\server\.
Status: NOT GATED, NOT INSTALLED. Game not touched (read-only copy of 24 .veh files only).

## 1. What changed (file:function)
- storage_api.inc ~776-783: `sa_rv_nm_t` (name, team, num, src) and `sa_rv_nm_name()` (slot out of range -> "Car N").
- storage_api.inc ~1849-1883 `sa_rv_names()`: the ONE name table per review id + stem. Rule per car, first that applies:
  1. driver EVENT (name, team)            src EVENT
  2. season .veh by race number (year from the stem) src VEH
  3. "#num" (race number, channel 97)     src NUM
  4. "Car N"                              src CAR
  Cached per (review id, stem). Built in the route dispatch (~2051).
- storage_api.inc readers now take `nmt` (old `sa_rv_name_of` removed):
  `sa_rv_laps_doc` 948 (n, l.1145 n ok), `sa_rv_status_doc` 973 (n + cn=team 988-990),
  `sa_rv_trace_doc` 1060 (driver 1084; car = team 1088, was "" before),
  `sa_rv_frame_doc` 1122 (n, 1145), `sa_rv_info_doc` 1364 (name + team 1393-1395, num from table),
  `sa_rv_pit_items` 1584 (1602), `sa_rv_pits_doc` 1622, `sa_rv_incidents_doc` 1638 (1654),
  `sa_rv_driver_doc` 1690 (n 1700, cn 1705), `sa_rv_frame_tape` 1889 (1920-1922: name, team, race_num).
- storage_api.inc `sa_rv_newest` 1264: new `max_steps` argument; its only caller is `sa_rv_names` (1862, channel 97).
- web_telemetry.c ~5819-5962: season .veh scan `vehn_walk` (tyre_walk folder rule, every .veh with Driver+Number+year
  kept; env TAPE2_SEASON_ROOT overrides the game's SEASONDATA, for tests and the review harness only), `vehn_read`,
  `vehn_clean` (drops trailing " (1997)"), `vehn_stem_year` (first standalone 19xx/20xx in the stem), `vehn_lookup`
  (refuses a number with two different driver names in one season), `vehn_reset` (tests).
- runtime_test.c: r1a fixture gets channel 97 (`r1a_num[]`, cars 7/8/18); `n1_test_names()` (new: N1 + N2 tests);
  r21 and r1b tests and the replay bench pass `nmt`; r21_old stem test uses its own review id.
- test_telemetry.py ~536-553: asserts /trace/status and /driver names equal /replay/info; /replay/frame names equal too.
  (Parses; NOT run, it needs the gate build. See open issues.)
- Session fixes made by me (on top of the previous worker's diff): trace/data `car` now the team; N2 test added;
  `r1a_num` fixture hook. The previous worker's code compiled cleanly (no backslash error at the old ~5896 line).

## 2. Doc audit (N1): shared order used?
| Review doc | Names from | Order holds | Location |
|---|---|---|---|
| /replay/info cars (name, team, num) | nmt | yes | storage_api.inc:1393-1396 |
| /replay/frame c[] (n, cn via car_name) | nmt | yes | :1920-1922 |
| /telemetry review frame c[] (n) | nmt | yes | :1145 |
| /trace/status cars (n, cn) | nmt | yes | :988-990 |
| /trace/data (driver, car=team) | nmt | yes | :1084, :1088 (car was "" before) |
| /driver (n, cn) | nmt | yes | :1700, :1705 (was "Car N" / EVENT only) |
| /pits items n | nmt | yes | :1602 |
| /incidents items n | nmt | yes | :1654 |
| /laps cars n | nmt | yes | :958 |
Before the change the dossier (/driver) said "Car N" while frames said "#num" (spec example). Fixed: every doc reads the table.
No "Car %u" left outside `sa_rv_names` (grep: storage_api.inc:781, 1878 only, both inside the rule).

## 3. Test results (offline, runtime_test built from %TEMP%\tape2_build)
Build: the exact command from the task, exit 0, no warnings (-Werror).
- Official runtime_test.exe (`run1.txt`): `FAIL line 1152: strstr(health,"\"matches\":true")` (known pin, workbench_ui.js
  sha). CHECK exits on the first FAIL, so the binary stops there and the N1/N2 tests are not reached.
- To reach the rest, a copy in %TEMP% with ONLY line 1152 made non-fatal (`rt_pinless.c`) was built:
  - with the R-S storage_writer.inc from %TEMP%\RSPLIT_before_2026-10-15\ (`rt_rsbefore.exe`, `run_rsbefore.txt`):
    EXIT 0, all checks pass, last line "PASS 8300313 checks"; "N1 map ... PASS", "N1 season by stem year ... PASS",
    "N1 lookups ... PASS", "N2 two cars with race number 8 ... PASS", "REPLAY 2.1/2.2/2.3 ... PASS".
  - with the CURRENT storage_writer.inc (`run2.txt`): FAIL `race-checkered-classification step 4: got . expected CR`
    (sim 1000000010.4). Cause: the R-S recorder split (storage_writer.inc, on hold per handoff), not T2-S. All other
    checks in that build pass up to the same point. Not fixed here (not my file).
- test_telemetry.py: parses (ast). Not executed.

## 4. N2 (same race number)
Test `n1_test_names`, block "N2": cars 1 and 2 both race 8 -> both kept, both get "Gerhard Berger / Benetton" (src VEH),
/replay/info has 2 such cars, /replay/frame has 2 such names, no crash. Real file: slots 16 and 17 both "#18" (see 5).

## 5. Real-file check (N3)
Input: 24 .veh files of 1997 (Vehicles\<team>\1997_<car>\*.veh), copied read-only from
`%USERPROFILE%\Desktop\F1\F1 Challenge V10\SeasonData\Vehicles` to %TEMP%\TAPE2_realseason\SEASONDATA\Vehicles\.
Recording: "AUSTRALIA 1997 - CO-OP - Race - 2026-10-14 20-17_2", review id v2_34c4aad20c99c576 (state ready, 24 cars, 395 laps).
Server: `rt_rsbefore.exe serve` (port 18090, RECREV_ROOT=%TEMP%\RECREV_2026-10-14\game, TELEMETRY_INI=...recrev.ini,
TAPE2_SEASON_ROOT=%TEMP%\TAPE2_realseason\SEASONDATA\, TELEMETRY_PREVIEW=1 to keep it up). Stopped by PID 17980 only.
The recording has NO driver EVENT (event types: incident, lap, pit), so every name is from the .veh files (INFERENCE:
race number + year identifies the driver).

| slot | name | team | num | source |
|---|---|---|---|---|
| 0 | Jean Alesi | Benetton | 7 | VEH (player) |
| 1 | Gerhard Berger | Benetton | 8 | VEH (remote) |
| 2 | Damon Hill | Arrows | 1 | VEH |
| 3 | Pedro Diniz | Arrows | 2 | VEH |
| 4 | Jacques Villeneuve | Williams | 3 | VEH |
| 5 | Heinz-Harald Frentzen | Williams | 4 | VEH |
| 6 | Michael Schumacher | Ferrari | 5 | VEH |
| 7 | Eddie Irvine | Ferrari | 6 | VEH |
| 8 | Mika Hakkinen | McLaren | 9 | VEH |
| 9 | David Coulthard | McLaren | 10 | VEH |
| 10 | Ralf Schumacher | Jordan | 11 | VEH |
| 11 | Giancarlo Fisichella | Jordan | 12 | VEH |
| 12 | Olivier Panis | Prost | 14 | VEH |
| 13 | Shinji Nakano | Prost | 15 | VEH |
| 14 | Johnny Herbert | Sauber | 16 | VEH |
| 15 | Gianni Morbidelli | Sauber | 17 | VEH |
| 16 | #18 | - | 18 | NUM (Tyrrell 18 and 19 both Number=18) |
| 17 | #18 | - | 18 | NUM (same) |
| 18 | Ukyo Katayama | Minardi | 20 | VEH |
| 19 | Jarno Trulli | Minardi | 21 | VEH |
| 20 | Rubens Barrichello | Stewart | 22 | VEH |
| 21 | Jan Magnussen | Stewart | 23 | VEH |
| 22 | Vincenzo Sospiri | Lola | 24 | VEH |
| 23 | Ricardo Rosset | Lola | 25 | VEH |

Endpoints checked (all 200): /recordings (list), /recordings/review (ready), /replay/info, /replay/frame?t=60,
/trace/status, /driver?id=0..3, /pits, /incidents, /laps, /trace/data?car=1001&lap=2 (driver Gerhard Berger, car Benetton),
/trace/data?car=1016 (driver "#18", car "").
Docs agree: captured JSON compared slot by slot: info, frame (n and cn), status (n and cn), laps (n), pits (n): 0 mismatches
over 24 slots. No "Car N" anywhere in those documents.
Note: the owner's live list showed "#18" twice; the file gives the same for slots 16 and 17.

## 6. Assumptions vs facts
- FACT: .veh Number/Driver/Team of the 1997 files are as listed above (read from the copied files).
- FACT: the recording has no driver EVENT, so the names come from the .veh rule.
- INFERENCE: slot 0 (the player) is Jean Alesi because race number 7 is Alesi in the 1997 files. Owner should confirm
  this matches the car he drove. If not, the mapping is wrong for the player only.
- INFERENCE: season year 1997 from the stem "AUSTRALIA 1997 - ...".
- ASSUMPTION: two cars with equal number and two driver names in the season (Tyrrell 18/19) give "#18" by design.
- Chosen: trace/data `car` field = team (contract text does not say what `car` holds; API_CONTRACT not updated).

## 7. Open issues
1. R-S (storage_writer.inc) breaks runtime scenario race-checkered-classification. Keep it on hold or restore the
   before-copy before the gate (handoff step 4). Official gate cannot pass until this is decided.
2. The official runtime_test.exe stops at the known pin line (CHECK is fatal). The gate's pin repair (workbench sha)
   must land before the full run reaches the N1/N2 tests. The rest was verified with the non-fatal copy only.
3. test_telemetry.py cross-doc asserts are not executed; run it in the gate.
4. API_CONTRACT: document trace/data `car` = team (not edited here, outside my file list).
5. Tyrrell 1997 race number 18 has two drivers in the game files; the name is "#18" by design. Possible later rule.
6. Real-file names for slot 0 and the others are INFERENCE until the owner checks them in game.
7. Temp only: %TEMP%\tape2_build (exes, run*.txt, serve_real.txt), %TEMP%\tape2_verify_rsbefore\, %TEMP%\TAPE2_realseason\,
   %TEMP%\rec_list.json etc. Disposable; nothing in the workspace was added except this record.

## Restoration
- Before-copies (state before the previous worker): %TEMP%\TAPE2_before_2026-10-14\server\. Nothing deleted.
- This session's own edits: storage_api.inc trace/data car line (~1087-1089), runtime_test.c r1a_num (~4230, case 97),
  N2 block in n1_test_names (before "restore: the serve run").

## Remote-human frame test 2026-10-15 (Haiku E)

Regression check for the sa_rv_tape_map fix: a remote human (channel 98 = 2) must not read as AI in /replay/frame.

- Test: `rte_test_remote_frame()` in `runtime_test.c` at line 4880, called from `main` at line 5404 (after `n1_test_names()`).
- Fixture: switch `r1a_remote2` (line 4231, default 0, set only by this test) makes car 2 carry channel 98 = 2 (line 4346). Car 1 keeps channel 98 = 1 (AI).
- Why a switch: the existing r1a fixture gives channel 98 = 2 to car 0 only, and car 0 is the player. The player branch already gives `"a":0`, so that fixture could not catch the bug.
- Check: `sa_rv_frame_tape` at t = 10 s (tape window 0 to 57.95 s). Car 2 must show `"cr":2` and `"a":0` (line 4936). Car 1 must show `"cr":1` and `"a":1`.
- Dump: `%TEMP%\R1_build\rte_frame_t10.json`. Car 2 (name #18): cr 2, a 0. Car 1 (name #8): cr 1, a 1.
- Fix under test: `storage_api.inc` line 1813 `if (ok[98] && c->controller_role == CTRL_REMOTE) c->ai_control = 0;`.

Results (review build in `%TEMP%\tape2_src`, line 1152 pin check neutralised there only; `-Werror`; gcc line as specified):
- Build: exit 0.
- Run `%TEMP%\tape2_build\rt_e.exe`: exit 0, `PASS 8475631 checks`, line 15 of the log: `RT-E remote human in /replay/frame at 10 s: car 2 cr 2 a 0 (remote), car 1 cr 1 a 1 (AI): PASS`.
- Bite test: line 1813 removed from the `%TEMP%\tape2_src` copy of `storage_api.inc` only. Built `rt_bite.exe`: exit 2, `FAIL line 4936: strstr(rte_seg, "\"cr\":2,") != NULL && strstr(rte_seg, "\"a\":0,") != NULL`. Then the copy was restored from the working dir (byte-identical).

Facts vs assumptions:
- FACT: the check passes with the fix and fails without it (run above).
- FACT: the working dir has changed only in `runtime_test.c`. `storage_api.inc` and the other sources were not edited.
- INFERENCE: `"cr"` is the controller role (json field order in `json_telemetry_from`). The check's values (2 remote, 1 AI) agree with the frame dump.
- NOT VERIFIED: the official suite (the line 1152 pin check still stops it, and storage_writer.inc is still on hold in the review copy); live game and real recordings; an in-game remote human.

Restoration and housekeeping:
- Before-copies: `%TEMP%\TAPE2_before_2026-10-15_rtE\` (`runtime_test.c`, `storage_api.inc`, and this record before this append). Nothing deleted.
- Extra copy: `%TEMP%\TAPE2_before_2026-10-15_rtE\runtime_test.c.check` (a before-state copy made by mistake; harmless, not used).
- Binaries and logs: `%TEMP%\tape2_build\rt_e.exe`, `rt_bite.exe`, `rt_e.log`, `rt_bite.log`.
- The serve run on port 18090 was not touched. No build.bat or TELEMETRY_TESTS run, no console windows opened.
