# Top bar + TV Director redesign, camera rename, circuit camera (Claude, 2026-10-14 evening)

Author: Matias. Owner feedback: `OWNER_TEST_1.2.0_FINAL_2026-10-14.md` (F2, T1, T2, T3, "TV Director should have a track map and show everything that is going on").
Status: implemented in source; viewer tests PASS 580; DLL compiles clean (-Werror, -fsyntax-only); NOT built (game running), NOT live-tested.
Before-copies: `%TEMP%\topbar_before_1014\` (telemetry_viewer.js, viewer_test.js), `%TEMP%\E1_before_2026-10-14\director_state.before_T1.inc`.

## Top bar (F2), design by Claude
- Header 40 px: red-accent F1C TELEMETRY brand | LIVE/PAST segmented switch (labels now "LIVE", "PAST") | session line (live track/session with a green dot, or the past recording) | Telemetry / TV Director segmented tabs (were unstyled native buttons) | live state, clock, round "?" help. R and T shortcut icons removed (the page menu covers them).
- The live source line row is collapsed while the game runs (its text is in the header); it still shows for PAST and when the game is not running ("Open a past session").
- Toolbar right under the header (moved above the lap strip): page arrows + page menu (max 320 px) | driver A box and driver B box (colour edge, source tag, list, own Freeze button) | extra sides | tools pushed right (+ Driver, X, Research, Import, Export).
- The "A ... | B ..." line under the bar (f1x-show) is collapsed: the lap strip names the same sides. Reference-lap line toned down.
- Grid note: hidden rows are collapsed to height 0, not display:none, because the console grid maps rows by child order (display:none had collapsed the stage). The TV panel spans rows 3..end (`.f1x > .tvd{grid-row:3 / -1}`).

## TV Director (T3 + owner map request), design by Claude
- ON AIR card (driver, car, camera name; red ON AIR badge, grey when no live sample) and state pills (Control on / View only, Car switch on/off).
- Three columns (two on narrower screens): Drivers table (on-air car marked red at the left edge, selection blue) | Track map (shared race map; red ring = on air, cyan ring = selected; click a car selects it for the cut) + Race events feed (race control messages, newest first) | Cameras tiles + Cut to driver (Enter, primary red) / Camera on this car / Back to my car / Go to position.
- Technical rows (mode/index, camera engine, sample, director stored, phase note, Default.cfg source) moved into a closed "Details" block.

## Camera rename (T2)
- Pencil button on each tile: inline name box (Enter saves, Esc cancels, empty = game name). Stored in this browser (localStorage `tvd.camNames`, key = game camera name). The game name stays visible under the custom name; the ON AIR card shows the custom name with the game name under it.

## Circuit camera (T1)
- Research `RESEARCH_T1_CIRCUIT_CAM_2026-10-14.md` (findings t1-circuitcam-*). Claude verified in the decompile: KeyboardInputActionMapper 0x40BE70 case 209 (PGDN): `if (CameraLockCheck(5) && !GameActionStateMachine(5, 1))`; GameActionStateMachine case 5 activates the TrackingCam group and stores the state at manager this[3] (+0x0C).
- DLL (`director_state.inc`): angle key `CIRCUIT` -> `DCAM_MODE_CIRCUIT 5`; `dcam_set_circuit` checks 16 prologue bytes of 0x40C720 and 0x40CDE0 (read from the owner's exe: 8B15E4547C00..., 53558B6C240C...; the method gives the known 0x40D240 prologue too), saves the manager state, calls CameraLockCheck(manager,5) then GameActionStateMachine(manager,5,1), reads back +0x0C == 5 -> "applied", else "redirected". `dcam_leave_circuit` returns to the saved state through the same state machine before any director angle or Restore. Next-frame verify skips the director-mode compare for circuit (car still checked).
- Viewer: "Circuit camera" is the first camera tile; with a driver selected, Cut = car jump + circuit camera (what PGDN does on that car).
- RISK (live test needed): leaving circuit via GameActionStateMachine(previous state) is INFERENCE (same function the keys use, previous state value not proven for every state).

## Later the same evening (owner live feedback)
- E1 PASS (owner, live): the "riding with X" line is gone on TV Director car changes.
- COCKPIT for AI (owner): DLL refuses COCKPIT (fixed mode 1) unless the target / followed car is the player (`dcam_is_player`), message "COCKPIT exists only for your own car (AI cars have no simulated cockpit view)". Viewer: COCKPIT tile greyed for an AI selection; picking an AI driver moves a COCKPIT choice to TVCOCKPIT (`tvPickDriver`).
- On-air highlight (owner): the driver list marks the car on air (roster id = car id, name fallback) with a dark-red row, red left edge and an "ON AIR" tag; red ring on the map.
- HUD (T4, owner): research `RESEARCH_T4_HUD_2026-10-14.md` (findings t4-hud-*); Claude verified `sub_4D8450` flips `dword_7CF788` ("HUD On" when 1). DLL `dcam_hud_for_mode`: SWINGMAN, ONBOARD and circuit -> save the player's state once, write 0; NOSECAM, COCKPIT, TVCOCKPIT (and Restore) -> write the saved state back. Guards: 16-byte prologues of 0x4D8450 and 0x4D8880 (from the owner's exe), value in {0,1}. Ini `[Director] HideHudOnCinematic=1` (default). INFERENCE until live: that 0x7CF788 is the board the owner means (owner confirmed the interpretation 2026-10-14: "reading is right").
- All compile clean (-Werror). NOT built, NOT live-tested.

## Hide HUD switch on the TV Director page (owner request)
- Viewer: toggle "Hide HUD on cinematic cameras" in the pills row (shown when /director/state reports `hide_hud`; enabled only on the game PC with Enabled=1). POST /director/options {"hide_hud": bool}.
- DLL: `director_options_route` (web_telemetry.c, same guards as the commands: Enabled=1, Host allowlist, local only, X-Telemetry-Action: director, JSON, 4 KB cap); `g_dcam_hide_hud` is an interlocked LONG; switching off posts `dcam_hud_option_task` to give the player's HUD back on the game thread. /director/state gains `hide_hud` and `mute_commentary`. Ini value = start value; the page value lasts until the game closes.
- Viewer test pins changed: the TV block now has two POSTs (commands + option switch).

## R2: past recordings open in review (Claude, viewer)
- Recordings: a session row's Open (and the weekend card's Open, first reviewable session) navigates to `/?session=<review_id>&name=<display name>` when the DLL lists a `review_id` (closed/recovered sessions, R1-A). Sessions recording now keep the old select-only Open with a tooltip.
- Review page: `reviewTitle` shows the &name= display name for v2 ids; `pollReviewIndex` polls /recordings/review until the lap index is ready (notice "Preparing the recording..."), then reloads once (&ready=1).
- Tests: viewer PASS 619. Server side: R1-A/R1-B records (compile only; runtime + HTTP tests need the game closed).
- Incident (R1-B Haiku): deleted its own %TEMP% before-state folder and rebuilt it by reverse-applying edits (disclosed in R1B_RECORD). Workspace files were not deleted.
