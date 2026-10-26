# Telemetry 1.2.4 (source): replay keeps finished cars, lap chips use the game time (2026-10-18)

Owner request (chat): "fix them all". This covers the open items in `LAPS_COOP_IMPORT_FIX_1.2.3_2026-10-17.md` section 5 and the import time mismatch.

**Status.** Source and offline gate (section 5). Not released. Not live-tested.

## 1. Qualifying replay drops cars that finished their runs

**Evidence (FACT, owner recordings, read-only copies in `%TEMP%\tel_rvdump`).**
- Tool: a diagnostics harness `rv_dump.c` built on the mod's own reader. It is not part of the mod.
- `EUROPE 2002 - Race - Qualifying - 2026-10-15 19-19_2.f1t` lasts 4185 s. 20 of 22 cars have their newest tick between 1233 and 1486 s.
- `AUSTRALIA 1997 - CO-OP - Qualifying - 2026-10-16 17-45_2.f1t`: cars end between 1237 and 1425 s.

**Cause (FACT, code).**
- The writer (`storage_writer.inc sw_fill_sample`) records only `on_track` cars (finish_state 0 and not retired). After its last run a car stops being recorded.
- `/replay/frame` (`sa_rv_frame_snap`) lists only cars with a tick in the last 2 s (`SA_RV_SNAP_S`), so the car vanished from the tower and the roster.
- Its laps became unselectable because the car was no longer in the frame.

**Fix.**
- `storage_v2.inc`: the index stores for every car its newest tick (`any`, `last_block`, `t_last`).
- `storage_api.inc`: `sa_rv_frame_hold` adds every car whose newest tick is before t. It uses that car's last recorded state (one block per car, cached per review id) and marks the car held:
  - off track, speed / throttle / brake / rpm 0;
  - pit state 4 (garage, as recorded for cars in a garage);
  - best lap and total time still come from its lap EVENTs.
- Works for existing recordings (Jose's included). Nothing changes in the file format or the writer.
- Real-file check: EUROPE quali at t = 3000 s holds the 20 cars (lap, position, finish_state of their last tick); at t = 1000 s none is held.

**Limit.** A held car's position is its last recorded one. Two held cars can show the same position; their order in a qualifying tower then comes from the best lap.

## 2. Lap time on chips vs the timing tower; export / import time

**Cause (FACT).**
- The lap chips showed the time between the recorded line crossings (`end - start`).
- The tower shows the game's official time. They differ by about 0.1 s (reporter: chip 1:30.362, tower 1:30.397).
- The export writes `lap_time_src: "game"` when the game time arrived. Import ignored that field: Jose's lap 4 showed 93.599 (sample span) instead of the game's 93.484.

**Fix.**
- Viewer `lapShownTime`: chips and the "Fastest Lap" chip show the official time when known, else the crossing time.
- `fastestLap` ranks by the same time (`lapRankTime`).
- `loadTrace` keeps an imported game time as `official_time`. The lap boundaries use `derived_lap_time` when the file has it.

## 3. "Minardi's laps didn't get saved": NOT reproduced

- In the owner's CO-OP qualifying file, the player (slot 0) has 8 lap EVENTs, each with the game's last-lap time (102.675, 105.322, 103.708, 98.363, 99.234…).
- Event lists are far below their cap: 206 / 234 / 1480 events vs 4000. The lap index cap is 2048 rows.
- The tyre name "C1" comes from the car's tyre brand table (`tyre_name_for`). It is a compound name, not a fault marker.
- **Next step:** Jose's `.f1t` from `<game>\Telemetry\recordings\`.

## 4. Files

- Backups `.BEFORE_20261018_replay`: storage_v2.inc, storage_api.inc, storage_writer.inc (unchanged), telemetry_viewer.js, workbench_ui.js, runtime_test.c, web_telemetry.c.
- Version 1.2.3 → 1.2.4: web_telemetry.c, telemetry_viewer.js, mod.json, dll.mod.json, tests.
- Tests:
  - runtime `test_replay_hold_rule`;
  - viewer K2 (official time ranks and shows; import keeps the game time).

## 5. Verification

- `node viewer_test.js`: VIEWER PASS 772.
- Plain build: DLL `0CEB93F3…` in `%TEMP%\f1c-web-telemetry-1.2.4`. Payload pinned.
- Full gate (`TELEMETRY_TESTS=1`, game closed, `%TEMP%\f1c-web-telemetry-1.2.4`): **EXIT 0**.
  - VIEWER PASS 772, soak and fuzz with 0 crashes.
  - Payload DLL `0CEB93F34BA0D0AE…`.
- Direct `runtime_test.exe`: 8 417 702 checks, including `PASS replay holds cars after their last tick`.
- Not run: a live game, or a replay of the reporter's file in the browser.
