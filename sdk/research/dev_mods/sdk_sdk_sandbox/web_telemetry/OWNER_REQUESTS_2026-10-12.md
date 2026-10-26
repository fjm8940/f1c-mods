# Owner requests tracker, Web Telemetry console (2026-10-12)

Every request Matias gave in the 2026-10-12 session, with status. Builds: rc.2 and
rc.3 are built in `%TEMP%`, not installed (install only when the game is closed).

| # | Request | Status | Where |
|---|---|---|---|
| 1 | Sector display wrong (S3) everywhere | DONE rc.2 (S2/S3 cumulative fix) | CONSOLE_BACKLOG "Batch 2a" |
| 2 | First lap shown as lap 0 | DONE rc.2 (display +1); Race Control "Lap 0" text to verify | Batch 2a |
| 3 | FLAGS shows 26 zones, white box never moves; how game yellows work | PARTIAL rc.5 (JS): white-box line names the focus car zone and a stopped focus car (viewer_test PASS). Live check OPEN | RESEARCH_BATCH2B A |
| 4 | Tyre column C0 instead of Soft | PARTIAL rc.7 (source): server adds per-car `tcn` (.veh -> .hdv TireBrand -> .tbc), viewer shows the short name with full-name tooltip, C<tc> fallback. viewer_test PASS 172. Live check OPEN (driver-name match, index mapping) | RESEARCH_BATCH2B B + Correction B; Batch 2j task 1 |
| 5 | Selector shows Barrichello P24; check grid/garage order | DONE rc.5 (Batch 2h task C): selector sorted by race number veh+0x4710 (CONFIRMED), labels "#num" and "P n", gar_* not used. Supersedes rc.4 garage order. Live check pending | Batch 2g (selector decision) |
| 6 | Race Control: fastest laps, pit stops, pit failures (mod only), collisions, penalties | PARTIAL rc.8 (source): mechanical failures status line from the read-only MF host probe (active / loaded / not loaded). Pit failures OPEN: MF host struct has no pit-failure field | CONSOLE_BACKLOG Batch 2k task 1 |
| 7 | Driver Focus sector 3 wrong | DONE rc.2 (same fix as 1) | Batch 2a |
| 8 | Lap Analysis empty mid-lap | rc.5 (task A): C trace gate and browser gate accept moving lap 0. NOT live-verified. Partial first lap is expected | findings web-telemetry-console-recorder-drops-lap0-20261012, web-telemetry-recorder-lap0-race-phase-20261012 |
| 9 | Vehicle Dynamics and all analysis pages empty | rc.5 (task A and 2): player lap-0 trace gate fixed (C, test added, not run); default A skips retired or stopped player (JS PASS). Live check pending | same finding |
| 10 | Lapped cars shown clearly everywhere | PARTIAL rc.8 (source): +nL tag, hollow dot with L (rc.6), blue-flag estimate tag BLUE est. and map ring (DERIVED). Live check OPEN | CONSOLE_BACKLOG Batch 2k task 3 |
| 11 | Research button does nothing | rc.5 (JS): Research button shows ON or OFF and aria-pressed (viewer_test PASS). Live visual check pending | — |
| 12 | Barrichello lap chips dimmer than Ralf | rc.5 (JS): Compare group separate in the menu. Lap-chip explanation unchanged (partial laps) | — |
| 13 | Redesign view menu into categories | DONE rc.8 (source): 7 groups; raw/smoothed pairs merged (Dampers, Ride Heights) with a smoothed toggle; old ?page= ids still open. Live check OPEN | CONSOLE_BACKLOG Batch 2k task 4 |
| 14 | Investigate weather system like F1CUI | DONE (research) rc.12: F1CUI mechanics fully documented (reads the GDB Weather{} script and/or its regenerated weather file into a minute timeline, looks ahead with RandomWeatherFactor jitter, generates its own bell with volatility/WetFactor; forecast display is its own timeline, not measurement). Forecast still shown only when LOG\weather.txt is fresh | RESEARCH_WEATHER sections B/E, findings f1cui-weather-system-bytecode-20261012 + f1c-gdb-weather-compendium-crosscheck-20261012 |
| 15 | Track conditions: dry/wet line, on/off track | FOUND + BUILT rc.12: the dry and wet lines are AIW wp_path lateral offsets along wp_perp (read from the live AI database), per-surface dry/wet grip is terrain.ini; the Weather tab map pane draws both lines with the on-line (racing line) and off-line (surface) wetness colours plus the grip reference table. Track temp: uniform global (0x8AFEF4), answered. Full gates + install PENDING | RESEARCH_WEATHER sections A/D/E, CONSOLE_BACKLOG Batch 2l |
| 16 | Investigate all "not proven" values | RESEARCHED (16 values; 3 promotable; AI front/rear suspicious; owner tests O1-O5). Tier changes pending Claude decision | RESEARCH_UNPROVEN_VALUES |
| 17 | Left arrow on Race Overview must not jump to last layout | DONE rc.3 (no wrap) | CONSOLE_BACKLOG "Batch 2c" |
| 18 | Help Haiku when it can't find data | ACTIVE (memory saved) | — |
| 19 | Separate engine live statistics page | PARTIAL rc.6 (source): rev limit from max_rpm (FACT), HUD oil and water. HP/Nm 'not available in game data'. Live check pending | RESEARCH_ENGINE_LIVE, CONSOLE_BACKLOG Batch 2i task 7 |
| 20 | Race Control: pit stop times, failures (mod), stop count per driver | PARTIAL rc.5 (JS): stops, last box time and penalties in Race Control summary. Mod pit failures OPEN | Batch 2c |
| 21 | Check WEATHER.txt after new session | DONE: not rewritten per session (mtime 2026-10-11) | RESEARCH_WEATHER update |

Next batch (rc.4): items 3, 4, 6, 8-11, 13, 19, 20 plus weather fixes (tyre wear mislabelled as grip, weather state labels).

Owner 2026-10-12: will run the hard-stop test (O2) after install; wants Claude to install and say when. Plan: install rc.4 when built + game closed, then owner drives and Claude captures read-only.

rc.4 status (2026-10-12): task 1 and selector garage order done; tasks 2-4 and 6-13 OPEN. Batch 2g record: CONSOLE_BACKLOG_2026-10-12.md.

Owner 2026-10-12: garage order = car number order (pit exit side: Toyota ... McLaren). rc.4 gar_* sort is wrong; rc.5 must sort selector by race number veh+0x4710 (show #n), drop gar_* ordering.

Batch 2h (rc.5, 2026-10-12, resumed): see CONSOLE_BACKLOG "Batch 2h". Source changes only; full gates PENDING (Claude runs when the game is closed). Task 4 tyre names, 10 weather relabels, 4 seed from /trace NOT started; 13 API_CONTRACT partial.

Batch 2i (rc.6, 2026-10-12): see CONSOLE_BACKLOG 'Batch 2i'. Source changes only; full gates PENDING (game was running). Tyre names, mechfail and forecast OPEN.

Batch 2k (rc.8, 2026-10-12): see CONSOLE_BACKLOG 'Batch 2k'. Source changes only; full gates PENDING (game running). Rows 6, 10, 13, 14 updated.

Batch 2l (rc.12, 2026-10-12): see CONSOLE_BACKLOG 'Batch 2l'. Owner asked (a) where the occasional wet areas and wet lines with less grip come from, (b) a Weather-tab track map with the on/off dry-wet distinction, (c) how F1CUI forecasts rain/condition changes, (d) whether track temperature is uniform. All answered: RESEARCH_WEATHER 'Owner questions addendum' sections A-E (wet line = AIW wp_path along wp_perp; surface grips = terrain.ini; F1CUI = own/scheduled timeline + RandomWeatherFactor lookahead; track temp = one global float). Owner then asked to dump ALL useful weather/climate data into the Weather tab: rc.12 does that (7 panes: Now / History / Forecast / Track map / Model & effects incl. GDB tables + F1CUI / GDB session script / Track conditions), FULL GATES PASS (46361 checks, viewer 232) and INSTALLED 2026-10-12 (restore map WEB_TELEMETRY_440RC12_INSTALL_2026-10-12). Rows 14/15 updated. Live check pending the next game start. Owner also supplied a third-party compendium (J21C revision 2): its GDB weather-table claims are recorded in findings f1c-gdb-weather-compendium-crosscheck-20261012 and RESEARCH_WEATHER section E for in-game verification.

rc.14 (2026-10-12 evening): owner asked (a) fix the Weather GUI to the quality and style of the other tabs, (b) no reliance on F1CUI (reference only), (c) investigate wetter spots with heavier spray. Done in source (COMPILE-ONLY, not installed; the full gate needs the game closed): Weather page rebuilt (5 panes, Race style; the broken look came from absolute canvases overlapping); all F1CUI content removed; the forecast now uses the GAME's own weekend timeline (LOG\weather.txt is written by the game, not by F1CUI: correction to rows 14 and 21); wet spots CONFIRMED: every wheel gets its own wetness from its distance to the wp_path[1] line (16-value profile table), which drives tyre grip, tyre cooling and spray. Records: CONSOLE_BACKLOG rc.14, RESEARCH_WEATHER section F.

rc.15b/rc.16 (2026-10-12 evening): owner asked for a simple race-engineer Weather tab without decompiled data (rc.15b, installed) and a forecast that does not spoil upcoming changes and can be wrong like reality (rc.16: rain chance per time block, uncertainty grows with lead time, revised every 10 min; full gate PASS, install pending). Owner also asked whether grip factor / weather timeline / rain intensity still need investigating: yes, listed in CONSOLE_BACKLOG rc.16 open research.
