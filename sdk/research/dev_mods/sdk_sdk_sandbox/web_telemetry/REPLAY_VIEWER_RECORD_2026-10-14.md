# Replay viewer record (H-V), 2026-10-14

Scope: REPLAY_SPEC_2026-10-14.md section 3 (viewer). Files: telemetry_viewer.js, viewer_test.js,
viewer_review/mock_feed.js, viewer_review/index.html (route hook only). No .c/.inc/.py/schema file and no
CURRENT_STATE.md was touched. The server routes (H-S) were not waited for; the page runs on the mock.

Tags: FACT = checked in this session (test output, file read, browser run). INFERENCE = reasoning, not yet
checked against a real recording.

## 1. Before-copies and line endings

- Before-copies (cp, nothing deleted): `%TEMP%\REPLAY_before_2026-10-14\viewer\` holds telemetry_viewer.js,
  viewer_test.js, viewer_review\mock_feed.js, viewer_review\index.html. FACT
- Line endings, before -> after (python byte count, FACT):
  - telemetry_viewer.js: LF 6885 / CRLF 0 -> LF 7576 / CRLF 0 (LF kept, no BOM)
  - viewer_test.js: LF 1178 / CRLF 0 -> LF 1246 / CRLF 0
  - viewer_review/mock_feed.js: CRLF 140 / LF 0 -> CRLF 260 / LF 0 (CRLF kept; edited with python)
  - viewer_review/index.html: LF 24 / CRLF 0 -> LF 32 / CRLF 0

## 2. Tests (FACT)

- Before any edit: `node viewer_test.js` -> `VIEWER PASS 619 checks`
- After: `node viewer_test.js` -> **`VIEWER PASS 658 checks`** (619 existing + 39 new, all pass)
- `node -e "require('./telemetry_viewer.js')"` -> `require ok`; mock_feed.js also requires cleanly.
- Two existing assertions changed (viewer_test.js:168 and :170): the page count +1 and "Recordings is the
  last page" became "Recordings is directly before Replay". Needed because spec 3.1 puts Replay after
  Recordings. Deviation, see section 5.

## 3. Changed code (line numbers after the edit)

telemetry_viewer.js
- 3090 `Console.prototype.pages`: `{ id: 'replay', name: 'Replay', group: 'Data' }` after Recordings.
- 3121 style injection: `CONSOLE_CSS + SRC_CSS + REPLAY_CSS`.
- 3292 global keydown: arrows and Space are left to the Replay page when `self.view === 'replay'`.
- 3310 startup: a v2 review with no `page=` or with `page=replay` starts on Replay. Other page values keep
  the stored page.
- 4106 `setPage`: Replay does not overwrite the stored start page.
- 6213-6399 replay helpers, pure, exported (see 7569-7574): rpWindowIndex, rpWindowRange, rpPlanFetch
  (6231), rpRowsFor, rpCarAt (6251), rpRowState, rpOutlineLength, rpProgress, rpGapText, rpGapLabel (6301),
  rpTowerOrder, rpDisplayName (6316), rpShortName, rpClock (6330), rpLastLap, rpMarkKind, rpMarkText,
  rpColor, rpInfoFrom (6376), rpErrText.
- 6400 rpInit, 6412 rpTitle (name from the polled Recordings list; no &name= needed).
- 6429 `view_replay`: live-mode note (exact spec text) with "Open Recordings" button (6436); review page
  build, wiring, listeners.
- 6535 rpKey, 6555 rpTogglePlay, 6562 rpSeek (seeking into a failed/waiting window retries it).
- 6572 rpStart: one requestAnimationFrame loop; stops when the page is left or document.hidden.
- 6591 rpAdvance, 6598 rpPump (prefetch and in-flight cap), 6611 rpLoadInfo (503 -> "Preparing the
  recording" toast and retry), 6632 rpLoadWindow.
- 6656 rpCars, 6664 rpMarkNear, 6673 rpPaint, 6714 rpPaintMap, 6777 rpPaintTower, 6794 rpPaintFollow,
  6812 rpPaintScrub.
- 6839 `REPLAY_CSS` (f1x-rp-* and f1x-watch classes).
- 6962 `recCardHtml`: Watch link `href="/?session=ID&page=replay"` on each finished session row with a
  review_id.
- 7569-7574 exports for the tests.

viewer_test.js
- 168, 170: the two page-list assertions (see section 2).
- 1178-1245: new block, 39 checks: interpolation, gap rule, lap-wrap, gap text and labels, tower order,
  name fallback, short names, time format, prefetch decision (7 cases), window constants, mock round trip
  (rpCarAt on a mock window), source checks for the LIVE note, Watch href, default page, pref, keys,
  pause-on-leave loop, 503 wait, page list order.

viewer_review/mock_feed.js
- 139-258 `createReplayMock()`: SYNTHETIC closed 3-lap loop (outline 240 pts), 4 cars (one with no name:
  shows #44; one CO-OP remote), pit stop (car 2, 70-84 s), contact (cars 0 and 3, 120 s), /replay/info
  (f1c.replay.info/1) and /replay windows (f1c.replay/1, 5 Hz, 12 columns), plus `fetchJson` hook.
- 259 exports `createReplayMock`.

viewer_review/index.html
- `?replay=1` opens the synthetic Replay page: review = mock session id, fetchJson = mock, mountConsole.
  Without the flag the page is unchanged (mount, live feed).

## 4. Browser check (FACT, limited)

Served the component dir with `python -m http.server 8765 --bind 127.0.0.1` (PID 1327, killed; port closed
after). Opened `viewer_review/index.html?replay=1` in the Browser pane.
- Page renders: map with outline and cars with numbers, tower (BRA, VEL, COS, #44 with gaps), follow panel,
  transport bar, scrub bar with pit (yellow) and contact (red) marks and the loaded 30 s band.
- At 1280x720 (JS rects): replay root and bar end at y=696 (inside 720). Map 868x423 CSS px, canvas 866x421
  device px at dpr 1 (setupCanvas path).
- Follow: clicking car 7 on the map zooms to it; panel shows name, team, speed, gear, lap, last lap,
  throttle and brake bars.
- Keys (dispatched events): ArrowRight +5 s, Shift+ArrowRight +30 s, Space toggles play, ArrowUp raises the
  speed step.
- Error text: a failed window shows "Replay error: ..." in red; seeking into it retries and loads.
- Recordings card (fake sessionsDoc): a red "Watch" link shows on the closed session with review_id, not on
  the open one; href `/?session=v2_0123456789abcdef&page=replay`.
- Playback driven by the real RAF loop was NOT observed: in this Browser pane requestAnimationFrame does not
  fire (`rafFired: false` in a direct probe), so the clock stood still. Instead the same pump/advance/paint
  functions were stepped synchronously: clock 0.3 s -> 30.3 s, window 1 prefetched and ready, tower and map
  updated. FACT for the logic; the smooth 60 fps loop still needs a live look.
- Server log: the only 404s were `/recordings/review?session=v2_...` (86, existing review-index poll, not
  part of this change, the static server has no route) plus favicon and /director/state (existing probes).
  No /replay request reached the server (mocked).
- Preview server stopped, viewport reset to desktop.

## 5. Deviations from the spec

1. viewer_test.js: two page-list assertions changed (needed by spec 3.1 placement).
2. Watch is not shown on sessions that are still recording (state open). Spec section 6 excludes replay of
   open sessions; FACT from code (`s.review_id && !open`).
3. Watch is on the session rows (where Open already is), not on the recording card header. The card-level
   Open is unchanged.
4. Interpolation: spec 3.3 lists x, z, dist. spd, thr, brk are also linear (INFERENCE: cosmetic; keeps the
   follow panel smooth). pos, lap, pit, gear, fin keep the previous row, as specified.
5. Lap wrap: dist is interpolated across the start line (wrap = outline length). Spec silent; without it the
   gap would jump back by one lap for 0.2 s at each crossing (INFERENCE from the 0.2 s row spacing).
6. Gap column: the 2.2 contract has no gap column. The page derives gap metres as
   (lap-1)*L + dist difference to the car ahead, with L = outline length, and divides by spd. Text is
   "+s.s", "Leader", "PIT", "FIN", or a dash when unknown or stopped. See open item A.
7. Layout split 68 % map / 32 % side (spec said 70/30). Minor.
8. Clicking a car follows it and zooms the map 2.4x on it (spec said follow; zoom is my addition). Esc
   stops following (spec silent).
9. Playback pauses when the document is hidden, not only the loop stops (so a return to the tab does not
   resume unseen). The clock also holds while its window is still loading.
10. Lap ticks on the scrub bar are the lap events of the car with the most laps in /replay/info (the leader
    in the info doc), per spec "leader laps".
11. Title in the map corner comes from the polled Recordings list; falls back to "Past recording".

## 6. Open items (for Claude and the owner)

- A. GAP SOURCE: the replay contract (2.2) has no gap_m. If the owner wants the spec's "gap_m / speed" as
  the server value, H-S (or Claude) should add a gap column. Until then the gap is an estimate from lap
  distance and outline length (outline decimated to >= 8 m steps, so error is metres, not tens of metres).
  INFERENCE on the size of the error.
- B. SPEED UNIT: spd is treated as metres per second (channel 2 per storage_writer, FACT from spec F1). The
  follow panel shows spd x 3.6 km/h. The spec 2.2 example (71.2) is consistent with m/s. Confirm on a real
  row.
- C. 503 path: exercised by reading the code only. The mock has no 503 route. A real 503 on the review
  route should show the toast and the page wait text, then play.
- D. Real recording (spec A1-A3, race 2026-10-14 20-54): not run, needs H-S routes and the real files.
- E. Live RAF loop not observed in the Browser pane (see section 4). Owner or Claude to look once live.
- F. Not in the viewer test: the keyboard Space-on-button blur fix (rpKey) is code-checked only.
- G. Workspace structure check (AGENTS.md mandatory): `check-workspace-structure.ps1` exit 1 with 2 errors,
  both pre-existing and not created here: "Unexpected top-level directory: .claude" and "...: plans".
  Not touched (structural, needs Matias). FACT.
- H. Governance cold start (AGENTS.md list): I read AGENTS.md (in the session context) and the spec, the
  viewer, test and mock code. I did NOT read governance\README.md, PIPELINE.md, PRODUCT_STATUS.json or the
  component README/status doc in full, because the task scope is viewer-only. Claude should check this.
- I. Documentation gate: this file is the durable record. It is not indexed in CURRENT_STATE.md (Claude
  indexes, per spec 5).

## 7. Restoration

- Restore a file: copy it back from `%TEMP%\REPLAY_before_2026-10-14\viewer\` (same relative path). Nothing
  was deleted.
- The added test block is the one starting at viewer_test.js:1178; the mock block is mock_feed.js:139-258;
  the viewer block is telemetry_viewer.js:6213-6838, plus the wiring edits listed in section 3.
