# CO-OP remote car "delay": lap-distance origin fix (Claude, 2026-10-14 evening)

Author: Matias. Owner report: "friend data seems to have some delay" (Lap Analysis, Albert Park 1997, CO-OP host, A = player, B = remote friend).

## Evidence (FACT, owner's live session, read-only GETs from 127.0.0.1:8080 while the game ran)
- /trace/data car 0 lap 1 (player, 92.269 s, 1408 samples) and car 1 lap 3 (remote, 92.84 s, 1409 samples), saved to %TEMP%\fdelay.
- Same place on track (nearest tx,tz within 8 m, 141 matched points): remote lap distance minus player lap distance = median +150.6 m (p10 145.8, p90 159.7).
- Speed at the same place: remote minus player median -0.5 m/s, so no real time lag in the speed channel.
- Remote samples every ~61 ms, pose changes on every sample.
- => Not a delay: a constant lap-distance origin offset. The remote lap distance is the projection on the AIW waypoint polyline, measured from waypoint 0, and that is not the game's lap-distance origin at this track (the 4.4.0-rc.1 note measured only 6..11 m at its test track).
- Also found: remote `track_pos` was written as a fraction of the lap while local cars use metres (sector_of, mini-sectors), so remote sectors and mini-sectors were wrong too.

## Fix (web_telemetry.c)
- `remote_origin_learn(count)` (capture thread, before remote_gate): for up to 4 locally simulated cars per frame (moving > 20 m/s, not pitting, valid game lap distance), offset = game lap distance - own projection (wrapped to +-lap/2); median per frame; first value taken, then smoothed 10 % per update; jumps over 60 m ignored; reset on a new lap length; after 20 good frames it runs every 10th frame.
- `remote_gate`: remote projection + learned origin, wrapped to the lap; `track_pos` = metres.
- Before-copy: %TEMP%\E1_before_2026-10-14\web_telemetry.before_remorigin.c. Compiles (-Werror). NOT built into a DLL yet, NOT live-tested.

## Live check for the owner (next CO-OP session)
- Lap Analysis A = you, B = friend: speed traces line up at the corners (no ~150 m shift); Delta vs A no longer swings +-2 s per corner on similar lap times.

## Live result 2026-10-14 (FINAL3, DLL 65A865E9, owner CO-OP host, Albert Park 1997, friend driving)
- Recording on: "AUSTRALIA 1997 - CO-OP - Practice 1 - 2026-10-14 20-00" (open).
- Friend's live lap distance vs the player's earlier recorded lap at the same place (4 reads 2 s apart, position match 0.6..2.3 m): differences +10.4, -6.4, +1.6, +7.3 m (was +150.6 m before the fix). Remote track position now in metres (3051 -> 4334 m while driving). FACT: the origin offset is fixed live; the residual +-10 m is within the waypoint-projection accuracy noted in 4.4.0-rc.1 (6..11 m).

## Second bug, found from the owner's screenshot 2026-10-14 (live lap of the friend drawn as one vertical line at ~5.2 km)
- FACT (SSE /events capture, 100 s, %TEMP%\fdelay\sse.txt): for the remote car the browser's distance field `tp2` (= c->track_from_pit, workbench_ui.js:311) stayed 5179.3 m for the whole lap, while `tp` (derived, fixed) ran 7.7 -> 5288.8 m with the lap counter going up exactly at the wrap. The server traces (/trace/data, lap 4: 2.1 -> 5287 m, no jumps) were already correct.
- Fix: `remote_gate` also writes the derived distance to `track_from_pit` (JSON tp2). Viewer: a lap-counter +1 together with a wrap of more than half a lap also counts as a crossing (remote projection can step past the 50 m window).
- Build FINAL4: DLL 89F4AE0D (plain build EXIT 0), bundle %TEMP%\F1_TEST_2026-10-14_TELEMETRY_120_FINAL4. Needs the game closed to install. Full gate not rerun yet.
