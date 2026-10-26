# 2026-10-10 live check: CO-OP field shows 23 cars, not 24

Question (Matias): why 23 drivers instead of 24 in live CO-OP race.

## Evidence (live, read-only, f1c-memory, captured 2026-10-10, pid 1924)
- Fact: engine `vehicle_count` = 23. Slot 0 = Matias Driver (human, Jordan #10 car),
  slots 32..53 = 22 AI. No excluded/retired/hidden slots (`f1c_positions`).
- Fact: `coop_mode_log.txt` (Documents\F1 Challenge V10\plugins\lightloader):
  `season field: season=2002 candidates=24`;
  `skipped reserved human car` for `2002_williams06.veh` and `2002_jordan10.veh`;
  `prepared selected co-op field: humans=1 ai=22 requested=24 total=23`.
- Fact: `coop_mode.ini` `[HumanCars] Count=2` reserves Williams #6 and Jordan #10;
  `BotSource=season`, `ExpectedHumans=2`.

## Conclusion (first pass, corrected below)
First read as "not a bug". 24-car 2002 season minus 2 human-reserved cars = 22 AI. Only one human
(host) is in the session, so the Williams #6 seat reserved for the second player
stays empty: 1 + 22 = 23. With the second player joined the field is 24.

## Correction (owner, same day)
Intended design: always spawn full grid; a joining human takes a bot's car and the
host releases that bot (`release_bot`, "a human took this car", 2026-10-09 park/release
work). Fact: `[HumanCars]` comes from the old sandbox configurator
(`multiplayer_ai_configurator.cpp`, pre-park design) and is still in the shipped
v0.9.1 package ini, so public users get the same 23-car field.

## Fix (dev only, not shipped)
- `dll/coop_mode.cpp`: reserved-human skip now applies only to the ini `[Bots]` list,
  not `BotSource=season`. Host's own car is still excluded by `roster_is_already_active`.
  Before-state: `dll/coop_mode.cpp.BEFORE_20261010_fullgrid`.
- Build: `%TEMP%\coop_mode_build\coop_mode.dll` sha256 `d7e16f9a...ded14a0`. Tests in `tests/` PASS (4/4).
- Pending live validation: solo host shows 24 cars; friend joining Williams #6 releases
  that bot (field stays 24); friend leaving restores it. Not deployed or released.


## Install for owner live test (2026-10-10)
- Installed d7e16f9a build to `Documents\F1 Challenge V10\plugins\lightloader\coop_mode.dll`.
- Previous installed DLL (sha256 997f3ad0...cb46040) copied to `%TEMP%\coop_mode_prev_20261010\` (restore = copy back).
- Live test pending (owner).

## Follow-up request (owner): in-game "Add AI" list
Owner wants an in-game list of drivers to add AI cars on demand, separate from auto-fill.
Not started. Needs spec: where in UI (menu spinner vs overlay), allowed sessions (pre-race only;
no mid-race add — existing guard at session type 7), host-only authority, manifest sync to clients.

## Shipped as 0.9.2 (2026-10-10)
Released and deployed (owner request) before live test. Record:
`development/lightloader/v2.0.0-hub-dev.1/release_coop_0.9.2_2026-10-10/README.md`. Host game DLL replaced by exact 0.9.2 (cc98b946).
