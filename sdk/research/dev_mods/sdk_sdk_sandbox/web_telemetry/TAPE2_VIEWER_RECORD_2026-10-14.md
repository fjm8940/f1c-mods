# TAPE2 viewer record (T2-V), 2026-10-14

Scope: TAPE2_SPEC_2026-10-14.md section 3 (V1 to V6), with the owner addition V7 (map ball on every map), sections 0,
1 and 4 as rules. Files: `telemetry_viewer.js`, `workbench_ui.js` (allowed by the spec: V6 lives in `lapsDown`, V2
`recorded` laps, V3 the CarSide tape filter, V4 the legacy cursor and render hooks), `viewer_test.js`,
`viewer_review/mock_feed.js`, `viewer_review/index.html` (script query only). No server file was touched.
Tags: FACT = checked here (file read, test output, Browser pane run). INFERENCE = reasoned, not checked on a real
recording or the live server.

## 1. Before-copies, line endings, restoration
- Before-copies (copied, nothing deleted): `%TEMP%\TAPE2_before_2026-10-14\viewer\` holds `telemetry_viewer.js`,
  `workbench_ui.js`, `viewer_test.js`, `viewer_review\mock_feed.js`, `viewer_review\index.html`. FACT
- Line endings and BOM after the work (Python-free byte count in node, FACT): `telemetry_viewer.js` LF 7912 / CRLF 0;
  `workbench_ui.js` LF 2129 / CRLF 0; `viewer_test.js` LF 1393 / CRLF 0; `viewer_review/mock_feed.js` CRLF 361 / LF 0;
  `viewer_review/index.html` LF 32 / CRLF 0. No BOM. No mojibake pattern. FACT
- Restoration: copy any file back from the before-copy folder (same relative path). Nothing was deleted.

## 2. V4 root cause: why the hover ball was missing (FACT, reproduced)
- Hover -> `StripChart` mousemove -> `Viewer.setCursor` -> `Viewer.drawCursors`. `drawCursors` ran the chart cursor and the
  map overlay (`canvases[i].cursor()`, where the ball is drawn) inside `requestAnimationFrame` and latched `this._cq`
  until that callback ran.
- Reproduced in the Browser pane (RAF never fires there): one hover set `cursor`, then `_cq` stayed `true`
  (`_cq: true` read back after 1 s with no callback, and a RAF probe got no callback in 1 s). No ball and no chart cursor
  were drawn after that. The same latch existed in the legacy workbench `scheduleRender` (`pending` stayed true).
- Fix: both use a timer (`setTimeout`). Browser check after the fix, without any RAF shim: hover on Braking set the
  cursor (2447.15 m) and changed 233 px on the map overlay and 1488 px on the chart overlay.
- Owner's browser: INFERENCE that RAF was not the owner's cause, because in review the tape clock (100 ms timer) calls
  `flush()`, so review pages redraw without RAF, and the map ball path was shown to work with the timer. The owner's
  exact PAST case could not be reproduced on the live server here. OPEN (see 7-E).

## 3. Changed code (line numbers after the edits)

`telemetry_viewer.js`
- V4 `drawCursors` 978: timer, not RAF (comment gives the cause). Hover ring vs frame ball: trackMap `cursor()` ~1578 area.
- V7 `frameMapPos` 992 (pure), `drawSideBalls` 1001 (one shared helper: every active side's car at its frame position,
  filled ball in the side colour).
- `flush` 1030: a deferred selector rebuild (V2) runs first.
- V4 `mapInset` 1414 (one shared inset: a graph page with no map gets a small track map in the lower-right corner;
  Replay excluded; legacy uses it when `WB.layoutHasMap` is false). `setCursorDist` 1425 (legacy hover -> shared cursor).
  `Viewer.renderView` calls `mapInset` (~1408).
- `trackMap` 1494: `drawSideBalls` before the hover (~1578), hover drawn as a ring (V4), `obj.isMap = true` (~1612).
- `raceMap`, `weatherPaneMap`: `obj.isMap = true` (race map label and weather label: V1 badge text).
- V2 `fillCarSelects(force)` 1112: sorted by position (unclassified last), "P<pos> <name> #<num> [badge]", no name ->
  "#num", rebuild at most once a second, never while a selector is focused; `force` for user picks (lines 1285, 4438,
  4676, 4707, 5088 pass `true`).
- V1 helpers 2552 `humanRole` (YOU = cr 0; HUMAN = cr 2 and not `a`; else ''), 2559 `humanBadgeHtml(c, side, colour)`,
  `humanBadgeText` (option text), 2570 `lapLabel` (recorded lap = "L<n>", import = "Import", live = "Lap <n>").
- V1 wiring: tower role ~5376 (`humanBadgeHtml`, replaces "(friend)"), lap-chip row header `renderLapRow` 4603
  (badge in the tag, role in the signature), Race Control rows `rcPane` ~5600 (badge for the car named by `e.id`),
  Focus header `view_focus` (~5800 region: "P1 BRA [YOU]" and the who line), Replay tower `rpPaintTower` 7105 and follow
  `rpPaintFollow` 7123 (`rpHumanCar` 6499 maps replay roles to cr/a), race and weather map labels.
- V2 lap labels: lap options ~1153, lap tag 1209, reference lap ~4570, chips ~4633 all call `lapLabel`.
- V2 CSS: `.f1x-lrow` min-height 17 px (was fixed 17 px under a scrollbar); `.f1x-hb` badge style (CONSOLE_CSS ~2290).
- V3 `tapeSyncLaps` 6676 (review only; one sync per tape tick, signature guard): each side gets `tapeT`, a shown lap the
  tape has not reached is dropped (mode back to auto), and `tapeOpen` = the lap in progress cut at the tape time.
  Called from `tapeTick` 6696 (inside `if (tp.info)`).
- V5 `liveReturnPage` 4464; `goLive` 4468 (review: `location.href = '/?page=' + liveReturnPage`); `goChange` 4484
  (review: `location.href = '/?page=sessions'`); `refreshSource` 4505 (Change hidden on Recordings with nothing open;
  line says "Pick a recording below."); `setPref` 3242 (review never stores the `page` key); constructor 3451 (live load
  with `?page=` opens that page once).

`workbench_ui.js` (why: the spec allows it for V2/V3/V4/V6 and the owner asked for the same rule everywhere)
- `lapsDown` 2035 (V6): reference = the car at P1 (the leader never gets a count; a remote car 2 laps ahead gives the
  field no "+2L"). Fallback when no running car is P1: the highest lap (see deviation 1).
- `layoutHasMap` 2052 (V4): graph with x = x and y = z; exported.
- `lapsUpTo` 470, `lapInProgress` 473 (V3, pure; exported). `CarSide` fields `tapeT`, `tapeOpen` (218).
  `displayPoints` / `displayLaps` (~466 to 489): tape filter, the open lap when no completed lap is shown.
- `seedLaps` 722: `lap.recorded = true` (V2 label "L<n>").
- `drawMap` 1712: `opts.frameBalls(ctx, view)` after the track (V7 frame balls on every legacy map).
- Legacy hover: `opts.onCursor` on series hover (~1863) and on the map hover (~1910), mouse leave sends null.
- `scheduleRender` 1820: timer, not RAF (V4 root cause in the legacy renderer).
- Exports: `lapsUpTo`, `lapInProgress`, `layoutHasMap` added (2096 to 2098).

`viewer_test.js`
- Existing assertions: none changed (deviation 2 of the TAPE record does not apply here). New block 1315 to 1392
  (TAPE2 V1 to V6, 28 checks: pure rules called, wiring checked in the source text).

`viewer_review/mock_feed.js` (CRLF kept)
- Replay mock: `traceLaps` and `/trace/status`, `/trace/data` (laps built from the same rows as `/replay/frame`, so the
  review seeds fill both sides; open laps are cut by the viewer). `base.num = c.num` in `frameDoc` (race number in the
  frame, see open item C).

`viewer_review/index.html`
- Script tags carry `?tape2=1` so the browser loads the changed files (a cached `mock_feed.js` was observed in the
  Browser pane). No other change.

## 4. Map list (V7): every map on every tab
| Page / function | Map | Frame ball | Hover ring | Note |
|---|---|---|---|---|
| Overview `view_overview` (~1723) | `trackMap` | yes (`drawSideBalls`) | yes | FACT browser |
| Braking `view_braking` (~1813) | `trackMap` | yes | yes | FACT browser |
| Compare `view_compare` (~2000) | `trackMap` | yes | yes | not browser-run |
| Tyres, Dynamics, Engine, Chassis (graphs, no map) | inset `trackMap` (`mapInset`) | yes | yes | FACT browser (Tyres, Dynamics, Engine, Chassis: inset present; Tyres ring 227 px) |
| Race `view_race` (~5660) | `raceMap` | yes, all cars (already) | n/a (no graph) | FACT browser: names and YOU/HUMAN labels |
| Focus `view_focus` (~5800) | `raceMap` | yes, all cars | n/a | FACT browser: header |
| TV Director `buildTv` (~3679) | `raceMap` | yes, all cars | n/a | source only |
| Weather `view_weather` (~7385) | `weatherPaneMap` | yes, all cars | n/a | source only (Weather not recorded in the mock) |
| Replay `rpPaintMap` (~7037) | own canvas, four cars at tape positions | yes | n/a (no graph; scrub strip is not a map) | FACT browser: tower; no inset (by rule) |
| Legacy layouts (all 23 start with an x/z track graph) | legacy `drawMap` (workbench_ui.js ~1712) | yes (`drawSideBalls`, 4 calls on first render) | legacy marker (existing, at the hovered distance) plus the shared cursor | FACT browser (track.ini) |

The race and weather maps and the Replay map already drew every car from the frame; V7 adds side-colour rings and the
shared helper only where a map drew nothing for the sides (trackMap, legacy). The map label badges (V1) are on the race
and weather maps. The Replay map shows car numbers only (not names), so it has no badge (deviation 6).

## 5. Tests (FACT)
- Before any edit: `node viewer_test.js` -> `VIEWER PASS 698 checks`.
- After the edits (final run): `VIEWER PASS 726 checks` (698 existing, 28 new). Node syntax checks pass for
  `telemetry_viewer.js`, `workbench_ui.js` and `viewer_review/mock_feed.js`.
- Intermediate failures were fixed and are recorded here: (a) the blue-flag assertion (`lapsDown` with no P1 car) made
  the strict P1 rule fail, so the fallback was added (deviation 1); (b) the `setPref` source check needed the exact line
  kept, so the review guard moved into `setPref` (deviation 3); (c) my own new checks had regex order and span errors,
  fixed in the new block only.

## 6. Browser checks (Browser pane, `http://127.0.0.1:8765/viewer_review/index.html?replay=1`, 8765 reused, not started
or stopped by this task; the pane was resized to 1440x900 and then 1280x720 and reset to desktop)
- V4 hover, Braking (no shim): see section 2. FACT.
- V4 hover, Tyres and Dynamics (1440x900): cursor set; inset ring 227 px and chart cursor 384 px (Tyres). FACT.
- V4 insets: Engine yes, Chassis yes, Tyres yes, Dynamics yes, Overview no (has a map), Race no, Focus no, Replay no. FACT.
- V4 LIVE (base viewer, no `replay=1`): frames arrive (frame 1225), pages render, no JavaScript errors. With the RAF
  loop not running in this pane, the LIVE map drew only after one manual `flush()`; then the hover ring drew 2334 px.
  FACT, but see open item E.
- V1: selector A reads `P1  Lena Brandt #7 [YOU]`, `P2  Jan Veltman #12`, `P3  Ruben Costa #3`, `P4  #44 [HUMAN]`. Tower
  rows show YOU and HUMAN with the side colour. Focus header `P1 BRA [YOU]`. Replay tower and follow show YOU and HUMAN.
  Lap row tag `A Lena Brandt ... YOU`, `B ... HUMAN`. Race Control line badge: wired and source-checked, not seen in the
  Browser pane. FACT (except RC).
- V2: lap chips read `L1 1:16.400`, `L2 ...`, `L3 ...` (no "Import"). Selector order by position. At 1280x720 the lap rows
  are 17 px with no horizontal overflow (scrollW = clientW). The 1920x1080 check was not run (the pane is smaller).
- V3: tape 10 s: no completed lap, open lap 1 (51 samples). Tape 50 s: no completed lap, open lap 1 (251 samples).
  Tape 100 s: laps [1], shown 1, open lap 2 (119 samples). Tape 200 s: laps [1,2], shown 2, open lap 3 (209 samples).
  Tape 390 s: laps [1,2,3], shown 3, no open lap. Seek back from 390 s to 120 s: laps [1], shown 1. FACT
- V5: source checks only. "Back to live" and "Change" in a review navigate to `/` on the game server; this static
  server cannot serve that. OPEN (7-B).
- V6: unit checks only (no +2L case in the mock). OPEN: owner live check.
- Console: no JavaScript exceptions across all 33 pages (`window` error listener). Only the known 404s
  (`/director/state`, `/recordings/review`), as in the earlier record.

## 7. Open items
- A. DECISION (owner acceptance vs V3 rule): the acceptance line says "Lap Analysis grows the current lap while the tape
  plays", but V3 says "Latest (auto)" = the newest completed lap. The current lap grows only in "Live lap" mode (and in
  auto while no lap is complete yet). Implemented as V3 says. Claude to decide whether the default should be "Live lap".
- B. V5 on a real server (Back to live, Change, LIVE load with `?page=`): needs a live check.
- C. CONTRACT for T-S / Claude: the frame car field `num` (race number) is assumed. `web_telemetry.c` ~6227 writes `"num"`
  in some JSON (route not confirmed here); `rosterOrder` and the selector read `c.num`. Without it the selector shows the
  name only (or "Car <id>" when the name is empty). INFERENCE until checked on a live frame.
- D. INFERENCE (for T-S to confirm): `/trace/status` lists the lap in progress with `start` and `t` (end = start + t), and
  `/trace/data` returns it in full. The viewer cuts it at the tape time with no extra request (spec "fetch when the tape
  enters a lap" not done, because seeding already loads every lap in full; see deviation 4).
- E. Base LIVE loop uses `requestAnimationFrame` (`Viewer.loop`, ~1043, unchanged). In a host that never fires RAF (this
  Browser pane) LIVE pages do not draw. The owner's browser is not affected by this check. Claude to decide whether the
  live loop also gets a timer.
- F. `/laps` (pace) still feeds the Race Control fastest-lap and stop lines without a tape filter (open item B of the
  TAPE record; spoiler risk).
- G. Full gate not run (the task brief said not to run it while the game may run). The viewer is embedded at compile
  time (`.incbin`), so the DLL build, the sidecar pins and the install are still needed (Claude / T-S).
- H. Replay map: numbers only, no name badge (deviation 6).

## 8. Deviations
1. `lapsDown` (V6): the P1 reference is used when a running car is P1. With no running P1 the highest lap is the
   reference, as before. Reason: the existing blue-flag assertion (viewer_test.js ~559) has no P1 car and must keep
   passing (not weakened).
2. No existing assertion was changed or weakened in this task.
3. `setPref` (3242) keeps the exact `setPage` line of the existing source check; the review guard is in `setPref`.
4. Tape-following laps use the recording's own seeded laps (full resolution, cut by the viewer) instead of a per-lap
   request when the tape enters a lap (spec 3, V3 "fetch it"). Reason: seeding already loads the laps; INFERENCE that
   the server lists every lap in `/trace/status` (open item D).
5. The map inset is also added to pages with graphs and no map (Engine, Chassis, Tyres, Dynamics). Replay is excluded.
6. Replay map labels stay as numbers (the spec names the Replay tower and follow panel, not the map numbers).
7. `index.html` script tags carry `?tape2=1` (browser cache; see section 3).
8. Mock additions (not in the spec list): `/trace/status`, `/trace/data`, `num` in frames (for V2 and V3 offline tests).
9. A tool result during this task asked for a dev preview server to be started. Not done; the instruction came from
   tool output, not from the owner. The 8765 static server was reused as the brief says.

## 9. Documentation gate
- This record exists in the component folder and lists the changed lines. It is not yet linked from CURRENT_STATE.md
  (not to be edited by this task). The spec and TAPE records remain the entry points. FACT
- Facts and inferences are tagged; pending validation is open items A to H.

## Wording fix 2026-10-15 (Haiku C)

Scope: wording and one browser popup removed in the viewer source. No build, no game run, no .exe run.

Before-copies (taken before any edit): `%TEMP%\TAPE2_before_2026-10-15_wording\telemetry_viewer.js` and `viewer_test.js`.
Line endings: both files are LF, UTF-8 without BOM, and were kept that way.

Changed in `telemetry_viewer.js` (current line numbers):
- 1282-1292: `window.confirm` replaced by the existing in-page `Viewer.prototype.dialog` helper (title "Load lap file", "Load "<file>" into car A or car B?", buttons Car A / Car B / Cancel). Cancel does nothing. The side-picking body moved into `Viewer.prototype.importLoad(f, letter)`. The dialog helper itself is unchanged (it already supports three buttons). Enter defaults to Cancel, the safe choice.
- 826 and 3395: import tooltip now "Load a saved lap file (saved from this page, or an old .lap / .spt file) into car A or B".
- 831: export tooltip (first build) now "Save the laps on screen to a file you can load later". The second build's export tooltip (Export laps: drivers or a team...) is unchanged.
- 1292 and 1313: both refusal toasts now "Not loaded: this file is not a lap saved by Telemetry."
- 1128: "(AI, remote copy)" becomes "(friend's car, AI driving)".
- 1149-1150: picker "Latest completed lap (auto)" becomes "Last full lap (auto)"; "Live lap (open)" and "Current trace" both become "Lap in progress". Option values 'auto' and 'open' unchanged.
- 4641: chip "Live lap" / "Current trace" becomes "Lap in progress". Chip tooltip unchanged.
- 5389: "(AI copy)" becomes "(friend's car, AI driving)".
- 6680: comment quote updated to "Lap in progress".

Not changed on purpose: 'Car ' + id fallback, 'Car ?', '[frozen]', the f1c.trace/1 schema comparisons and export field, and the "Import refused: ..." toasts for other errors. Also not changed, outside the listed scope: `telemetry_viewer.js` 5810 "(remote copy)" and `workbench_ui.js` labels, which the test at viewer_test.js:153 still asserts. These are layman-unclear and are open for a later pass.

Changed in `viewer_test.js`: a new block of 11 checks before the final PASS line (no confirm, alert or prompt call; dialog title and buttons; both tooltips; both Not loaded toasts; picker and chip labels; friend's car label; schema comparison unchanged).

Test output (run in this folder, no build):
- `node viewer_test.js`: "VIEWER PASS 737 checks" (baseline 726 before the edit; +11 new checks).
- `node viewer_canvas_test.js`: "CANVAS PASS 52 checks: all 26 driver selections draw speed polylines".
- `node audit_regression_test.js`: "AUDIT MODEL 22/22 PASS" (exit 0; one Node warning about --localstorage-file, not a failure).
- `node --check telemetry_viewer.js`: SYNTAX_OK.

Facts vs pending: FACT = the text and checks above. PENDING = the served bundle does not contain these changes until build.bat is run by the owner (not run here). Pending owner validation: click Import in the live viewer, check the dialog and the Cancel path, and check the picker labels on screen. The Enter-key default was chosen as Cancel (not in the brief); the owner may prefer Car A.
- Workspace validator (`tools/workspace/check-workspace-structure.ps1`) run at orientation: 2 errors, "Unexpected top-level directory: .claude" and "plans". Both are outside this task's files and were not changed here. Recorded for the owner to decide.

## E2E fixes 2026-10-15 (Haiku D)

Source: end-to-end check of a real CO-OP recording on the Replay (PAST) page. Edited only `telemetry_viewer.js`, `workbench_ui.js`, `viewer_test.js`. No build, no .exe, game not touched. Files are LF, UTF-8 without BOM, unchanged in that respect.

Before-copy: `%TEMP%\TAPE2_before_2026-10-15_e2e\` (`telemetry_viewer.js`, `workbench_ui.js`, `viewer_test.js`). Nothing deleted.

### 1. Tower "#18#18" (FACT)
- Cause: the tower row printed `rpShortName(name)` as the tag and then the full `rpDisplayName` as a `.fn` span. For "#18" and "Car N" both are the same string.
- Fix: new pure `rpTowerNameHtml(name)` at `telemetry_viewer.js:6536`, placed after `rpShortName`. The tag is always printed. The full name is printed only when it differs from the tag. Used by the tower row at `telemetry_viewer.js:7139`.
- Other callers checked: `rpShortName` is also used for the map label at `telemetry_viewer.js:~7101`, which prints only the short tag. No other place prints tag plus name. `rpShortName` itself is unchanged.

### 2. Labels (FACT)
- Chip: `'Latest (auto)'` -> `'Last full lap (auto)'` at `telemetry_viewer.js:4652`. Picker at `:1149` already said this.
- Comments updated: `telemetry_viewer.js:4608` (the renderLapRow comment) and `:6698` (tapeSyncLaps comment).
- Workbench `workbench_ui.js:239`: `' · AI (remote copy)'` -> `" · friend's car, AI driving"`.
- Viewer header label `telemetry_viewer.js:5829`: `' · AI (remote copy)'` -> `" · friend's car, AI driving"`, so the Focus page matches.
- Remaining "remote copy" text is in one code comment, `workbench_ui.js:2032`. It is not user-visible and was left as is.

### 3. CO-OP footer disagrees with tower badge (cause: FACT for the code path, INFERENCE for the exact recording values)
- FACT, the frame feed: in PAST, `onFrame` sets `this.frame` from `/replay/frame` (`tapeFetch`, `telemetry_viewer.js:~6737`). The server mapper `sa_rv_tape_map` in `storage_api.inc:1786` sets `c->ai_control = player ? 0 : 1`. So in every tape frame every non-player car has `a = 1`, including a CO-OP remote human.
- FACT, the tower feed: the Replay tower reads `/replay/info` roles (`rpInfoFrom`, `telemetry_viewer.js:~6573`). The server sets `role = remote` from the control bit (`storage_api.inc:1293-1296`, `:1397`), with no a flag. `rpHumanCar` maps remote to `cr 2, a 0`, so the tower shows HUMAN.
- FACT, the old footer: `coopInfo` read the frame, counted `cr 2`, required `p > 0`, and split humans by `!a`. A remote human is `a = 1` in the tape frame, so it became "host-driven AI".
- INFERENCE: the observed text "client · 0 remote humans · 1 host-driven AI" fits this exactly, with Gerhard Berger as the remote human. Berger's `p` at the viewed tape time was probably 0 (grid or tape start), which the `p > 0` filter would drop. I did not read the recording's frame data, so both the `p` value and the `a` value are unverified for that moment.
- Fix (`telemetry_viewer.js:1736-1756`): new pure `coopSummary(cars)` counts `cr 2` cars by `humanRole()` (HUMAN = remote human, no badge = host-driven AI). `coopInfo` uses the replay info roles through `rpHumanCar` in PAST, the same list as the tower, and the live frame otherwise. The `p > 0` gate is removed because the tower badge has no such gate.
- Not fixed here (outside the 3 allowed files): the root cause is `storage_api.inc:1786`. The tape frame's `a` should follow the recorded control bit, as `/replay/info` does. Fix needs its own task with a `runtime_test.c` check. The viewer's frame-level `humanRole` labels in PAST (lap rows, car selectors) still read that frame and will miss remote humans until the server is fixed.

### Tests
- `viewer_test.js:153` updated for the new friend-car label.
- New block "E2E fixes 2026-10-15 (Haiku D)" at `viewer_test.js:1415-1447`, 14 checks: tower name once (#18, Car 3, named driver), tower row uses the helper only, chip and label text, `coopSummary` cases, PAST footer uses info roles (remote + ai + player gives "host · 1 remote human"), live footer counts a p 0 remote car, empty footer when there is no data.
- `node viewer_test.js`: VIEWER PASS 751 checks (was 737; +14, exit 0).
- `node viewer_canvas_test.js`: CANVAS PASS 52 checks (exit 0).
- `node --check` on the three files: SYNTAX_OK.

### Pending validation
- Not done: no real CO-OP recording was re-run through the built bundle. `build.bat` was not run, so the served bundle does not contain these changes until the owner builds.
- Owner check on the Replay page: tower shows "#18" once; chip reads "Last full lap (auto)"; footer for the CO-OP recording reads "host · 1 remote human" (or the owner's own reading of Berger's `p`).
- Server fix for `storage_api.inc:1786` still needed for PAST frame labels (see above).
- Workspace validator run after the edits (`tools/workspace/check-workspace-structure.ps1`): the same 2 errors as before, "Unexpected top-level directory: .claude" and "plans". Outside this task, not changed.
