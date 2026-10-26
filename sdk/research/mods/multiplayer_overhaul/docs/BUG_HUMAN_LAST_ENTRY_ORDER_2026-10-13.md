# Bug: CO-OP host listed last instead of the replaced bot's car-number place (2026-10-13)

Owner report (live, : in CO-OP, host drives Barrichello's Ferrari but is listed P24 (last) in practice instead of Barrichello's place (P3). "This worked before."

## Evidence
- FACT (live `/telemetry`, practice session_type 2, no lap times): order = 23 bots, host (slot 0, Matthias Steppe) P24.
- FACT (code): `apply_entry_order()` sorts cars by `car_number_of(vehicle)`, which looks the vehicle's roster up in `g_season_rosters[]`; no match returns 1000 (= last). Its log line `entry order` is debug-only (`log_is_debug_only`), so the log does not show it.
- FACT (code): in the prepare loop the car a human is joining in is skipped (`vehicle_file_in_player_table` -> `continue`) BEFORE `g_season_rosters[candidate] = roster_index` is set. `build_season_candidates()` resets all rosters to -1 on the first build of a season. So the human's car never gets a roster -> host number 1000 -> last.
- FACT (backups): the prepare-time skip appeared between `coop_mode.cpp.BEFORE_20261012_joinchurn` (no skip) and `..._buildfill` (skip), 2026-10-12. None of today's (10-13) changes touched it.
- INFERENCE: "worked before" = builds before 10-12 00:20 (the bot was prepared first and released when the human joined, so its roster was stored), or later processes where an earlier session had stored that car's roster (the table is only reset on a new season build).

## Fix (dev, 2026-10-13)
`dll\coop_mode.cpp` prepare loop: inside the skip branch, resolve the skipped car's roster with `VehicleLookupByName` and store it in `g_season_rosters[candidate]` (season list only). The lookup index is restored after the loop as before.
- Build: `dll\build.bat` -> exit 0, `-Werror`. SHA256 `F4D3E01C322273EC2012EE1CAA1DF859302E94125B007CD01DFA340C9ACE5E0D` = 1.0.0 ED17FA90 + this fix. Staged: `%TEMP%\F1_HUMANPLACE_2026-10-13\multiplayer_overhaul.dll`. Not installed.
- Tests: `python -X utf8 -m pytest -q tests` 16 passed (no test covers this path; game-only).
- Restoration: `dll\coop_mode.cpp.BEFORE_20261013_humanplace`.
- Coordination: a Haiku agent (R01 menu release) edits the same file; it was told to keep this block.

## Owner live test (game closed to install)
1. Install the staged DLL (or the next combined build). Start CO-OP as host in Barrichello's car.
2. Practice before any lap: host listed at Barrichello's number place (P3), not P24. Optional: set `[Log] Debug=1` to see `entry order ... changed=N`.
3. Client side (laptop) not covered: client order comes from the host manifest path (`apply_entry_order` on manifest apply); check the client list too.

Open: race grid still puts an UNTIMED human behind timed bots by design (ISSUES_2026-10-09 #5). That is the grid rule, not this bug.

## Installed (owner PC, 2026-10-13, game closed)
`plugins\lightloader\multiplayer_overhaul.dll` = F4D3E01C. Previous ED17FA90 kept at `%TEMP%\F1_HUMANPLACE_2026-10-13\multiplayer_overhaul.dll.BEFORE_ED17FA90` (restore: copy back). Live result pending.
- Live PASS (owner, 2026-10-13 evening, F4D3E01C): host in Barrichello's car listed at his place in practice before any lap. Client list not checked.
