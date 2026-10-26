# TAPE2 viewer review (read-only), 2026-10-15

Reviewer: Haiku 5.5 (read-only audit). Input: TAPE2_SPEC_2026-10-14.md sections 0, 1, 3, 4, 5 and
TAPE2_VIEWER_RECORD_2026-10-14.md. Only this file was written. No source file was edited. No build, no .exe, game not touched.
Tags: FACT = read in code or run here. INFERENCE = reasoned, not checked on a real recording or the live server.

## Test run
`node viewer_test.js` -> `VIEWER PASS 726 checks`. **Final line: PASS.**
Caveat: most TAPE2 V4, V5, V7 and V2 checks are regex matches on source text (the text exists). They do not run the
behaviour. Behaviour is unit-tested only for V3 (lapsUpTo, lapInProgress, displayLaps, displayPoints), V6 (lapsDown)
and V1 (humanRole, badge text). Line numbers below are from `telemetry_viewer.js` (TV) and `workbench_ui.js` (WB).

## V1 to V7 verdicts

| Req (spec sec. 3) | Implementing code | Tests (viewer_test.js) | Verdict and reason |
|---|---|---|---|
| V1 YOU / HUMAN badge everywhere, one helper | humanRole TV 2552, humanBadgeHtml 2559, humanBadgeText 2566; tower 5381; lap row 4611-4620; Race Control 5608-5610; Focus 5808-5809; Replay 7115, 7129; race map label 5506 | 1321-1331 | PARTIAL. Helper is shared and the "(you)"/"(friend)" tags are gone. Missing: spec says HUMAN "or the friend's name when known"; code always shows HUMAN. Replay map labels show numbers only (record deviation 6). |
| V2 selector order, no rebuild while open, L<n> labels, chips not under scrollbar | fillCarSelects TV 1112-1140 (sort, 1 s limit, focus guard); lapLabel TV 2570; CSS TV 2287 | 1333-1341 | PARTIAL. Code matches. Chip-row fix checked only at 1280x720 (record sec. 6). 1920x1080 not run, although the spec asks for both. Tests are text matches. |
| V3 tape-following laps (review), pages grow while playing | lapsUpTo WB 470, lapInProgress 473, displayPoints 479, displayLaps 486, tapeSyncLaps TV 6676, tapeTick 6696 | 1343-1358 (behaviour for the pure parts) | PARTIAL. Completed-lap filter and open-lap cut work (record sec. 6 numbers). But the default "Latest (auto)" shows only completed laps, so the current lap does not grow. The owner's acceptance line says it should (record open item A, owner decision). Open-lap fetch was not done; seeded full laps are used (deviation 4). |
| V4 hover ball on every graph tab; map inset for graph-only pages; one helper | drawCursors TV 978-988 (timer); mapInset TV 1414; setCursorDist 1425; renderView 1408; trackMap hover ring 1588-1595; WB scheduleRender 1823-1825 | 1363-1374 | UNCLEAR. Timer fix is real and reproduced in the Browser pane (record sec. 2). The owner's PAST case is not reproduced; the cause on the owner's browser is an INFERENCE (record sec. 2, open item E). Inset coverage is in place. |
| V5 Back to live -> LIVE; Change -> Recordings; no "none open" on Recordings | liveReturnPage TV 4464; goLive 4468; goChange 4484; refreshSource 4505 (text 4520); setPref 3242; ?page= load 3451 | 1376-1384 (source text only) | PARTIAL. Review branch navigates to `/?page=...` and a fresh load sets source live (TV 3183). Not checked on the live server (record open item B). Also the review branch of goLive does not tell the server to clear a loaded recording (only the live branch calls `recordingPost('load','')`, TV 4473); this may be harmless but is unverified. |
| V6 "+nL" vs the P1 car; leader never gets a badge | lapsDown WB 2035-2050; used by TV 5484 (race map) and 5376 (tower) | 1386-1391 (behaviour) | DONE for the rule as written. Residual: if the inflated remote copy is itself P1, the whole field still gets "+2L". Not in the spec test case; mention to owner. Live check open (record sec. 6). |
| V7 ball on every track map, every tab, one helper, no hardcoded track | frameMapPos TV 992; drawSideBalls TV 1001-1011; called in trackMap TV 1578 and legacy drawMap WB 1711-1712; race map TV 5494-5500 (side rings) | 1362, 1367 (helper and wiring by text) | PARTIAL. No hardcoded track geometry (the only track data is the TRACK_COUNTRY name table, TV 5309, pre-existing). Weather map (TV 7457+) draws all cars but no A/B side-colour ring. Replay map has no A/B concept (record deviation 6). Coordinate frame UNCLEAR: the frame ball uses mx/mz, while the line and hover use tx/tz. API_CONTRACT lists them as different frames (track vs "map frame", line 93). Mock sets mx = px, so the test cannot show an offset. |

## Other checks
- **workbench_ui.js changed.** Allowed by the spec, and the record explains why (record sec. 3). Changes: CarSide tapeT/tapeOpen (WB 217-218), lapsUpTo/lapInProgress (470-476), displayPoints/displayLaps (479-489), seedLaps lap.recorded (722), drawMap frameBalls (1711-1712), legacy hover onCursor (1863, 1867, 1910), scheduleRender timer (1823-1825), lapsDown (2035-2050), layoutHasMap (2052), exports (2096-2098). The record's claim of "no existing assertion changed" holds: the blue-flag fallback is kept (record deviation 1).
- **Line endings and encoding.** Checked: LF only for telemetry_viewer.js, workbench_ui.js, viewer_test.js; CRLF for mock_feed.js. No BOM, no mojibake. Record sec. 1 is correct.
- **Popups (owner rule).** `window.confirm` at TV 1282 ("OK = A, Cancel = B") in the Import flow. Present in the before-copy too (pre-existing), but it breaks the no-popup rule the task asks about. Replace with an in-page choice.

## Layman labels (owner rule: every label understood without help)
- TV 1130: `'Car ' + c.id` shows an internal slot number when there is no name or race number.
- TV 6506: `'Car ?'` in the Replay tower.
- TV 1149-1150, 4634: "Latest completed lap (auto)", "Current trace", "Live lap (open)". "auto", "open", "trace" are internal terms.
- TV 4620: "[frozen]" badge. TV 1128: "(AI, remote copy)", "(retired)". TV 5382: "(AI copy)".
- TV 4573: "Reference lap" with a flag glyph; "partial lap" (4640 area) and "B off (show A only)" are clear, keep.
- TV 826 tooltip: "f1c.trace/1 JSON export or a legacy .lap/.spt"; toasts at TV 1284-1306 say "f1c.trace/1". Developer terms.
- YOU / HUMAN / L<n> / "Lapped +nL" are clear enough for this owner.

## Findings (ordered)
1. V4 owner-case cause is not proven (record open item E). Needs the owner's PAST check after install.
2. V7 frame vs trace coordinate frame (mx/mz vs tx/tz) is unverified against a real recording. A mismatch would put the frame ball off the driving line. The mock hides it.
3. V3 default "Latest (auto)" conflicts with the acceptance line "Lap Analysis grows the current lap while the tape plays". Owner decision (record item A).
4. V5 live check and server-side recording state after "Back to live" are unverified (record item B).
5. V6 residual: an inflated remote car at P1 would shift the "+nL" of the whole field.
6. V1 friend-name case missing (spec: "or the friend's name when known").
7. V7 weather map has no A/B side-colour ring.
8. Tests: most V2, V4, V5, V7 checks are source-text matches. A behaviour test for hover ball and tape-lap growth would close the gap.
9. Popup `window.confirm` (TV 1282) and the layman labels above (pre-existing, not introduced here).

## Open items for the owner or Claude
- Decide V3 default ("Latest" vs "Live lap" while playing).
- Run the live PAST check (same race: names, YOU/HUMAN, hover ball, Back to live, no +2L on the leader) and the 1920x1080 chip-row check.
- Confirm frame mx/mz and trace tx/tz share one frame on a real recording.
- Replace the popup and the jargon labels in a follow-up task.

Restoration: nothing was changed by this review. Before-copies from the previous task are at `%TEMP%\TAPE2_before_2026-10-14\viewer\`.
