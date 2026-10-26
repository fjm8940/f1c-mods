# Tape viewer record (T-V), 2026-10-14

Scope: TAPE_SPEC_2026-10-14.md section 3 (3.1 to 3.5), with sections 1, 4 and 5 as rules.
Files changed: `telemetry_viewer.js`, `viewer_test.js`, `viewer_review/mock_feed.js`, `viewer_review/index.html`.
Not changed: `workbench_ui.js` (no reset hook needed, see deviation 3), every server file (`.c`, `.inc`, schemas,
`runtime_test.c`, python tests), `CURRENT_STATE.md`.

Tags: FACT = checked in this session (file read, test output, browser run). INFERENCE = reasoning not yet checked
against a real recording or the live server.

## 1. Before-copies and line endings

- Before-copies (copied, nothing deleted): `%TEMP%\TAPE_before_2026-10-14\viewer\` holds `telemetry_viewer.js`,
  `viewer_test.js`, `viewer_review\mock_feed.js`, `viewer_review\index.html`. The REPLAY before-copies in
  `%TEMP%\REPLAY_before_2026-10-14\` were not touched. FACT
- Line endings and BOM, before -> after (Python byte count, FACT):
  - `telemetry_viewer.js`: LF 7576 / CRLF 0 -> LF 7733 / CRLF 0. No BOM.
  - `viewer_test.js`: LF 1246 / CRLF 0 -> LF 1315 / CRLF 0. No BOM.
  - `viewer_review/mock_feed.js`: CRLF 260 / LF 0 -> CRLF 319 / LF 0. No BOM. Kept CRLF.
  - `viewer_review/index.html`: LF 32 / CRLF 0 -> LF 32 / CRLF 0. No BOM.
- Mojibake pattern check on `telemetry_viewer.js`: none found. FACT

## 2. Tests

- Before any edit: `node viewer_test.js` -> `VIEWER PASS 658 checks`. FACT
- After all edits: `node viewer_test.js` -> `VIEWER PASS 698 checks` (658 existing, 40 new). FACT
- `node --check telemetry_viewer.js` and `node --check viewer_review/mock_feed.js`: no output (valid). FACT
- Mock check (node): `/replay/frame` at 45 s returns four cars whose x, z, speed, position and lap equal the
  replay window row for the same time; frames clamp to [0, t_last]; a frame without `t` is refused. FACT (in the test)

## 3. Changed code (line numbers after the edit)

`telemetry_viewer.js`
- 1000 `Viewer.prototype.flush` (new): the redraw of the old `loop` tick, moved out unchanged. 1009 `loop` calls it.
- 1023 `Viewer.prototype.onFrame`: `wb.frame` only when `this.wbFeed !== false`.
- 1045 `fetchTrack` block skipped when `this.tapeTrackOk` (review outline comes from `/track`).
- 3064 Console constructor: `wbFeed`, `this._rp = this.tape = this.tapeNew(review)`, and the message for a review that
  is not a v2 recording.
- 3281 `build`: `this.tapeBar = this.buildTapeBar()` appended after the stage. 3319 `tapePaintBar()` before `startPolls`.
- 3305 global `keydown`: `if (self.tape.kind === 'rec' && self.rpKey(e)) return;` (tape keys on every page in review).
- 4231 `startPolls`: `this.tapeInit()` for review.
- 4245 `pollWeather`: no request in review.
- 4275 `pollDriver`: review branch asks `/driver?id=&t=<tape t>` and keeps the server error text.
- 4421 header: `Tape <time>` next to the recording name in review.
- 4428 `refreshChrome`: bar shown only in review on the telemetry tab.
- 5232 `towerPane` pit stop counts and 5450 `rcPane` incidents: `tapeVisible` filter (no spoilers).
- 5679 `view_focus`: shows the driver error text instead of a stuck "Loading the driver dossier".
- 5954 and 5992 `pollSessions`: success clears `sessionsErr`; failure stores the message.
- 6223 Recordings click: Open without a review id -> `cannotPlay`.
- 6467 to 6482: tape constants and pure rules (`TAPE_TICK_MS`, `tapeStep` 6470, `tapeSeekResets` 6476,
  `tapeCannotPlayText` 6480, `tapeShowsAt`, `tapeFrameUrl`, `tapeFetchDue` 6482).
- 6485 `tapeNew`, 6492 `tapeInit`, 6504 `tapeOnInfo`, 6513 `tapeLoadTrack`, 6522 `tapeTick` (the one clock),
  6547 `tapeFetch` (one frame in flight), 6567 `tapeSeekReset`, 6572 `tapeVisible`, 6577 `cannotPlay`,
  6587 `buildTapeBar` (the one bar), 6633 `tapePaintBar`.
- 6659 `view_replay`: map, tower and follow only; its own bar and its own key listener removed.
- 6722 `rpKey`: tape keys (Space, Left/Right 5 s, Shift 30 s, Up/Down speed, Esc on the Replay page); returns true when used.
- 6754 `rpSeek`: seek reset check and one frame request. 6766 `rpStart`: starts the tape timer only.
- `rpAdvance` removed (the clock is `tapeStep`). `rpLoadInfo` 6782 calls `tapeOnInfo`. `rpLoadWindow` 6804 unchanged.
- 6846 `rpPaint`: map, tower and follow only. 6965 `rpPaintScrub`: draws on `this.tapeDom.sc`.
- 6993 `REPLAY_CSS`: `.f1x-tape` class.
- 2166 `CONSOLE_CSS`: root grid rows gained one `auto` row for the bar (`... minmax(0,1fr) auto 22px`). Without it the
  bar overlapped the 22 px status row (found in the browser, FACT).
- 7730 exports: `TAPE_TICK_MS`, `TAPE_SEEK_FWD_MAX`, `tapeStep`, `tapeSeekResets`, `tapeShowsAt`, `tapeFrameUrl`,
  `tapeCannotPlayText`, `tapeFetchDue`.

`viewer_test.js`
- 1252 to 1315: new block (40 checks): clock, seek reset, spoiler rule, frame throttle, tape state, race control
  filter, seek reset keeps seeded laps, cannot-play text, source checks (one timer, frame path, bar, Replay page has no
  bar, keys, Weather, Open, /track, workbench feed, t_first, header), mock frames.
- Two existing assertions changed (deviation 1): the old global-key line check and the old per-page RAF loop check.
  Both now check the new code. The count did not change for them.

`viewer_review/mock_feed.js` (createReplayMock, lines 244 to 307 and the routes in fetchJson)
- `frameDoc(t)` (244): the four synthetic cars at sim time t in live `/telemetry` shape (built from one live mock frame,
  core fields overwritten), plus `tape: {session, t, t_first, t_last}`.
- `pitsDoc` (262), `incidentsDoc` (268): items carry `t` in seconds (the live contract has none, see open item A).
- `recordingsDoc` (277): one recording, one session with a review id (Watch) and one without (cannot play).
- `driverDoc` (285). Routes: `/replay/frame` (400 without t), `/track`, `/pits`, `/incidents`, `/laps`, `/driver`,
  `/recordings` (307). `/weather`, `/trace/*` and unknown routes still reject (404).

`viewer_review/index.html`
- Line 20: `fetchTrack` is undefined in replay mode (the outline comes from `/track`). Header comment updated.

## 4. Browser check (Browser pane, FACT)

- Server: port 8765 was already serving this component folder (HTTP 200, serving the edited files). I reused it, and
  I did not start or stop any server. A hook reported another chat's dev server in this folder; the 8765 server
  answered and served the edited code, so the run used it. Opened `viewer_review/index.html?replay=1`.
- Layout: transport bar shown above the status line (fixed after the first look, see section 3).
- Replay page: track map with four cars, tower (BRA, VEL, COS, #44 with gaps), follow panel, transport bar with
  scrub marks. Playback: tape moved 1.196 s -> 3.803 s in 2.5 s at 1x, frames carried the tape time, the bar time
  followed, one frame in flight. Space toggled play on the Focus page too.
- Race: tower, map, session panel, Race Control. At 0 s: START only. After 100 s (ArrowRight x20 by key press): YELLOW
  at 60 s and the pit stop at 70 s (P4 "stops 1 - last box 14.0 s") shown; YELLOW at 120 s and GREEN at 150 s hidden.
  Spoiler rule works in the browser. Header shows `Tape 1:40.0`.
- Timing: tower and race control render; pace empty (no laps offline).
- Focus: driver panel (speed, gear, lap, tyre, fuel, radar), laps and pit stops panel, map. No stuck text.
- Overview, Braking, Compare, Dynamics, Tyres, Research and the legacy layouts: render their empty states ("No
  samples", "No braking zones yet"). Laps come from `/trace/*`, which the mock does not serve (open item C).
- Live Gauges: values from the tape frame (speed 230 km/h, gear 6, rpm 15,181).
- Engine Live: renders (rpm, rev limit); wear rows absent because the mock frame has no `pch` block (open item C).
- Car Chassis Live: "No driven session" (no `pch` in mock, open item C).
- Weather: "Weather is not recorded in this recording." No request made.
- Recordings: the card, Practice 1 (Open only), Race (Open and Watch). Open on Practice 1 showed "This recording
  cannot be played: no replay data (made before storage v2, or no work file). Nothing was changed." and the page
  stayed. Watch and the Race Open were not clicked (they navigate away).
- Console: no JavaScript exceptions. Only 404s for `/director/state` and `/recordings/review?session=` (existing
  direct `fetch` calls in the viewer that the mock does not serve; pre-existing).
- LIVE console: NOT browser-checked. `index.html` without `replay=1` mounts the base Viewer, which has no tape.
  The LIVE bar hiding rests on the `refreshChrome` rule (3 and 4428) and the source check in viewer_test.js.

Bugs found in the browser and fixed in this task (FACT):
1. Transport bar overlapped the status line (root grid had no row for it). Fixed (2166).
2. Pages did not redraw on a seek or a frame until a key was pressed: the tape timer flushed only its own dirty flag.
   Fixed (6522 `tapeTick`: `this.flush()` runs each tick).
3. Recordings page stuck on "Loading recordings" when `/recordings` fails. Fixed (5954, 5992, 7053).

## 5. Deviations from the spec

1. Two existing viewer_test.js assertions changed (section 3): the global-key line and the per-page RAF loop. The
   spec changes that behaviour (one shared clock, tape keys on every page).
2. Clock: one `setInterval` of 100 ms (`tapeStart`, 6492 to 6522) and no RAF in review. The spec allows the timer as
   the fallback; RAF does not fire in the Browser pane, and 10 Hz is enough. The base Viewer loop still runs its RAF
   as before for live pages.
3. The workbench is not fed by tape frames in review (`wbFeed`, 1023 and 3064). Reason: its laps come from
   `/trace/data`; feeding it tape frames would build duplicate laps. So there are no "Workbench live sample buffers"
   to clear. The seek reset clears only Engine and Chassis `pchHist` (6567) and keeps the seeded laps (tested).
   `workbench_ui.js` is unchanged.
4. The tape state is the same object as `this._rp` (the Replay page state), so the clock is shared. Field names:
   `t`, `playing`, `speed`, `t_first`, `t_last` (set at 6504), and `info.tf` / `info.tl`.
5. The Replay page has no bar of its own. The same bar (`f1x-rp-*` classes) shows on every page in review.
6. The Replay page's own key listener is gone. One global keydown calls `rpKey` on every page in review.
   Esc still resets zoom on non-Replay pages.
7. Reviews that are not v2 recordings (1.0.1 archives): no tape and no frames. The bar says "This saved session has no
   replay data". Their pages are unchanged (the tape filter applies only when the tape has recording info).
8. The cannot-play text ends "Nothing was changed." instead of "Still in LIVE; no file was changed." A review page is
   never LIVE. The spec's reason text is used as given.
9. `loadRecording` is kept but no longer reachable from Open (spec 3.4 allows this). The PAST notice in `renderView`
   is also kept.
10. Driver dossier in review uses `fetchLapDoc` (to show the server's error text). Live still uses `fetchJson`.
11. `pollSessions` error text (the spec asks for the server message on every page; not spelled out for Recordings).
12. The mock gained `/recordings` (not listed in spec 3.5), needed to test Open and Watch offline.
13. Frame throttle: one frame in flight (`tapeFetchDue`, 6482). A tick while one is in flight asks nothing; the next
   tick asks for the tape time of that moment (the "skip" of the spec).
14. The Race Control summary (fastest lap, stops) and the pace list (`/laps`) are not tape-filtered (open item B).

## 6. Open items

- A. CONTRACT (for T-S and Claude). The live `/incidents` items carry `tk` (a tick, FACT from `build_incidents_json`,
  `web_telemetry.c:3805`), not seconds. The live `/pits` items carry no time at all (FACT, `build_pits_json`,
  `web_telemetry.c:4243`). The viewer hides any item without a `t` in seconds while a recording is on the tape. So on a
  real recording Race Control and the pit counts stay empty until the review routes add a seconds field `t`. The mock
  adds `t`. Needs a decision: the field name and the server work (T-S), or a different rule.
- B. `/laps` (pace) feeds the fastest-lap and stop lines in the Race Control summary and is not tape-filtered (needs a
  lap time per item). INFERENCE: this shows future laps as a spoiler until fixed.
- C. Offline gaps (mock only, not a viewer defect): no `/trace/status` or `/trace/data`, so the analysis pages, Tyres,
  Dynamics and Engine wear show empty states; no `pch` block in frames, so Chassis shows "No driven session". Needs
  the real server or a richer mock before the owner's live check.
- D. Real server: every review route (`/replay/frame`, `/track`, `/pits`, `/incidents`, `/driver?t=`) is still not built
  (T-S). Until then the page shows the server error text. Timings for `/replay/frame` (target under 20 ms) not measured.
- E. LIVE console mode not browser-checked (see section 4).
- F. Owner live checks A1 to A4 (spec section 7) not run.
- G. Governance cold start: AGENTS.md was read (it is in the session context), and the spec and REPLAY record were
  read. `governance\README.md`, `PIPELINE.md`, `PRODUCT_STATUS.json` and the component README were not read in this
  task (viewer-only scope). Claude to check. The workspace structure validator was not run (no structural change).

## 7. Restoration

- Restore a file: copy it back from `%TEMP%\TAPE_before_2026-10-14\viewer\` (same relative path). The byte state of
  each before-copy is the state before this task (equal to the REPLAY state). Nothing was deleted.
- Changed blocks, for a manual revert: viewer 1000 to 1023, 1045, 3064, 3281 to 3319, 3305, 4231 to 4428 (points),
  5232 to 5992 (points), 6223, 6467 to 6967 (tape block and Replay page), 6993, 2166, 7730; test 1252 to 1315 and the two
  changed assertions; mock 244 to 307 and the routes; index.html line 20.
