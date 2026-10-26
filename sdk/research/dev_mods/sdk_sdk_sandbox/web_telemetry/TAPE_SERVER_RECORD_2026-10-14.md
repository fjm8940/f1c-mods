# TAPE server record (T-S): spec section 2, 2026-10-14

Product: Telemetry (`web_telemetry`), 1.2.0 (unreleased), API 10 (unchanged). Author: Matias.
Spec: `TAPE_SPEC_2026-10-14.md` section 2 (2.1 to 2.5), contract section 4 (fixed), contract clarification of 2026-10-14
(review `/pits` and `/incidents` items carry `t`). Step 2.1 is recorded in `REPLAY_SERVER_RECORD_2026-10-14.md` section 1 onward.
Tags: FACT = read in source or observed in this run (command output, file bytes, line). INFERENCE = reasoned, to verify.
OPEN = owner or Claude decision. Viewer (T-V) files were not touched by this worker.

## 0. Status

| Step | Status | Evidence |
|---|---|---|
| 2.1 REPLAY routes, driver EVENT | done | REPLAY record section 1-3 |
| 2.2 `/track`, `/pits`, `/incidents`, `/driver`, `/weather` message | done | runtime test lines in section 5; schemas validate real files (section 6) |
| 2.3 `/replay/frame`, `json_telemetry` split | done | golden byte identity (section 4); frame validates `telemetry.json` (section 6) |
| 2.4 menu-state check | done, no change needed | section 7 |
| 2.5 tests | runtime suite PASS; contract validation PASS on all 8 real sessions; full gate NOT RUN (game running) | section 5 |

Gate: not run. At the end of the work `tasklist | grep -i "F1 Challenge"` lists `F1 Challenge Online.exe` (PID 20164),
so per the rules the gate (`build.bat` with `TELEMETRY_TESTS=1`) was skipped. The HTTP checks added to `test_telemetry.py`
therefore did not run (OPEN, section 9).

## 1. Changed files (2.2 to 2.5), with line numbers of this tree

Before-state: `%TEMP%\TAPE_before_2026-10-14\server\` (`SHA256_TAPE_BEFORE.txt` lists the hashes taken before the first edit).

| File | Lines | Change |
|---|---|---|
| `storage_api.inc` | 1505-1699 | REPLAY 2.2 / TAPE 2.2: `sa_rv_snap_ids` (newest tick at or before t, <= 2 s old, reads the blocks around t), `sa_rv_ticks_before`, `sa_rv_name_of`, `sa_rv_pit_items` (pit entry and next exit of a car; driver form without id/n/t), `sa_rv_pits_doc`, `sa_rv_incidents_doc` (t, tk), `sa_rv_track_doc`, `sa_rv_driver_doc` |
| `storage_api.inc` | 1700-1880 | REPLAY 2.3 / TAPE 2.3: `sa_rv_frame_snap` (one pass over the blocks for all cars), `sa_rv_tape_map` (inverse of `sw_car_from`, section 2), `sa_rv_names_for` (driver names cached by review id), `sa_rv_frame_tape` (calls `json_telemetry_from`, appends `review` and `tape`) |
| `storage_api.inc` | 1926-2010 | `sa_api_review_route` branches: `/track`, `/pits`, `/incidents`, `/driver` (id 0..103, t clamped), `/replay/frame` (t clamped, 400 for non-finite) |
| `storage_api.inc` | 1942-1944 | gate: `/weather` alone is 404 with `weather is not recorded in this recording` (`/pits`, `/incidents`, `/track`, `/driver` removed from the 404 list) |
| `web_telemetry.c` | 4443-4449 | `sector_of_t(const telemetry_t *t, float)`; `sector_of(pos)` wrapper for the capture side |
| `web_telemetry.c` | 5823-5841 | `tyre_refresh()` snapshots the names under the lock; `tyre_refresh_names(cnt, nm, cn, num)` maps brands without the lock |
| `web_telemetry.c` | 5876 | `json_pch_from(o, n, const telemetry_t *t)` (was `json_pch` reading `g_telemetry`) |
| `web_telemetry.c` | 5910 | `json_telemetry_from(buf, bufsz, const telemetry_t *t, unsigned seq, unsigned gen, unsigned long age_ms, int wp_count, const int *lap_dropped)`: the body, no lock, no capture or memory reads |
| `web_telemetry.c` | 6139-6156 | `json_telemetry(buf, bufsz)`: the old entry point; `tyre_refresh()`, then under `g_telemetry_cs` reads the capture counters, the waypoint count (memory) and the lap drops, calls `json_telemetry_from(&g_telemetry, ...)` |
| `web_telemetry.c` | 9306 | dispatcher passes the query values `id`, `t`, `t0`, `t1` (`qs_get`, NULL when absent) |
| `schemas/pits.json` | properties | optional item `t` (number) |
| `schemas/incidents.json` | properties | optional item `t` (number) |
| `schemas/telemetry.json` | before `review` | optional object `tape` `{session, t, t_first, t_last}`, additionalProperties false |
| `schemas/replay_info.json`, `schemas/replay.json` | new | REPLAY 2.1 / 2.2 documents (section 2 of REPLAY record) |
| `schemas/version.json` | | re-hashed with `python -I schemas/update_version.py 10` after the last schema edit: `B3C2F4629540F9D1BD7844AC75A45B627FEE3A3F5F2C2925AB7345257631D5C9` (CRLF kept; the previous value of the hash was `03139C316D...`). Check: `check_contract_version('10')` passes |
| `contract_validate.py` | end of `check_schema_contract` | asserts: optional `t` on pits and incidents, replay schema constants |
| `test_telemetry.py` | `review` section | `/weather` 404 message; `/pits`, `/incidents`, `/track`, `/driver?id=<car>` answer 200 and validate; `/replay/info`, a 3 s window and a 31 s window (400); `/replay/frame` at 10 s validates `telemetry.json`, `t=nan` 400. NOT RUN (gate) |
| `runtime_test.c` | 4224-4300 | fixture switch `r1a_driver` (default 0): positions, speed, driver EVENT, pit EVENTs, one contact incident; the R1-A and R1-B fixtures are unchanged |
| `runtime_test.c` | 4727-4858 | `r21_test_replay()`: REPLAY 2.1, TAPE 2.2 and 2.3 checks (section 5) |
| `runtime_test.c` | 4860 | `json_golden(path)` and mode `json-golden <file>` (writes the live frame of a fixed 24-car state) |
| `runtime_test.c` | 4975, 5108 | `replay_bench(file, t)` and mode `replay-bench <file> <t>` (builders on a real file, timings to stdout, documents to `%TEMP%\REPLAY_bench\`) |
| `API_CONTRACT_2026-10-09.md` | section 18 (appended) | the routes, shapes, errors, the `t` rule and the schema changes; CRLF kept |
| `REPLAY_SERVER_RECORD_2026-10-14.md` | section 1 onward | 2.1 record |

Not changed: `storage_v2.inc` apart from `sv2_event_walk_rd` (REPLAY record), `CURRENT_STATE.md` (not edited, per the rules),
`telemetry_viewer.js`, `viewer_test.js`, `viewer_review/*` (T-V).

## 2. Channel to field map (the inverse of `sw_car_from`)

Forward: `sw_car_from` (`storage_writer.inc`, section "car mapping") writes channel id -> `car_telemetry_t` field, with the gate in
brackets. Inverse: `sa_rv_tape_map` (`storage_api.inc`, the TAPE 2.3 block). Units are those of the field (the value is stored
as written by the game-side field; Q16 channels are decoded by `sa_rv_real` to the table range, RAW32 as integers).

| id | forward (writer) | inverse (tape frame) | note |
|---|---|---|---|
| 1 | `lap_dist` | `lap_dist` (f) | |
| 2 | `speed` [car_valid&1] | `speed` (f) | |
| 3 | `throttle` [car_valid&2] | `throttle` (f) | |
| 4 | `brake` [car_valid&4] | `brake` (f) | |
| 5 | `steer` [AI, car_valid&8] | `steer` (f) | |
| 6, 7, 8 | `glat`, `glon`, `gvert` [car_valid&32] | same (f) | |
| 9, 10 | `yaw`, `yaw_rate` | same (f) | |
| 11-14 | tread: player `g_telemetry.tread[k]`, AI from tire wear | player: `tp->tread[k]`; AI: not written | AI inverse not possible (clamp) |
| 15 | `rpm` | `rpm` (f) | |
| 16 | `gear` (0..16) | `gear` (int) | |
| 17, 18 | `track_x`, `track_z` | same (f) | |
| 20-23 | `ttemp[k]` player only | `ttemp[k]` player | |
| 24-63 | player chassis when `pch_valid`: 24+k ride, 28+k susp, 32+k tdefl, 36+k press, 40+k grip, 44+k camber, 48+k spin, 52+k disc, 56+k bend, 60 bottom, 61 compound, 62 fuel, 63 rpm | `tp->pch_*` and `tp->pch_valid = 1` when channel 24 present | player only; the other pch fields stay 0 |
| 64-67 | `tire_wear_fl/fr/rl/rr` | same (f) | |
| 68-76 | `position`, `lap_number`, `pitting`, `call_to_box`, `tire_choice`, `track_limits`, `fw_flag`, `rw_flag`, `dmg_tier` | same (int) | |
| 77-81 | `oil_temp`, `water_temp`, `fuel_level`, `gap_ahead_m`, `gap_behind_m` | same (f) | |
| 82, 83 | `gap_ahead_id`, `gap_behind_id` | same (int) | |
| 84, 85 | `live_sector_delta`, `last_sector_delta` | same (f) | |
| 86 | `last_lap` | same (f) | |
| 87, 88 | `abt[0]`, `abt[1]` | same (f) | |
| 89-92 | `aws[k]` | same (f) | |
| 93-96 | `tcarc[k]` player only | `tcarc[k]` player | |
| 97, 98 | `race_num`, `controller_role` | same (int) | |
| 99, 100, 101 | `car_valid`, `vf6`, `contact_with` | same (int) | |
| 102-117 | `rx_su[k]` 102+k, `rx_ld[k]` 106+k, `rx_sl[k]` 110+k, `rx_fo[k]` 114+k | same (f) | RESEARCH values, no unit claimed |
| 118 | `penalties` | same (int) | |
| 119, 120 | `fuel_avg_per_lap`, `fuel_autonomy_laps` | same (f) | |
| 121, 122, 123, 124 | `box_soon`, `finish_state`, `driver_status`, `reason_code` | same (int) | |

Not recorded, so 0 in a tape frame (sentinel): `ai_control` is set from the file scope (`scope 0` = AI, `storage_writer.inc:203-206`),
`on_track` = 1 for cars with a tick, `name`, `car_name`, `race_num` (driver EVENT or channel 97). `top_speed_*`, `sector_elapsed`,
`elapsed_time`, `pit_time_*`, `penalty_*`, `team_id`, `garage_*`, `retired`, `phys_x/z`, `dir_x/y`, `steer` (player), `ttemp` (AI),
`max_speed`, `box_soon_reason`, `qualy_time`, `lap_of_run`, `car_conditions`, `hits_in_race`, `rpm_limiter`, `human_drs`, `dnf_dsq`,
`number_of_laps`, `time_next_exit` and the other car fields that `sw_car_from` never writes (the list is the complement of section 2).
Session fields not recorded: `race_phase` (ph), `race_active`, `total_laps`, `ambient_temp`, `track_temp`, wetness, `brake_temp`,
`tire_temp`, `blister`, `oil_temp_hud`, `water_temp_hud`, `player_engine_life_s`, `lap_len`, `sec1`, `sec2`, `track_len`, `seg_*`,
`fastest_lap_*`, `dotd_*`, `ai_difficulty`, `race_mode`. INFERENCE: these are the fields the 1.0.1 and 1.1.0 files never stored
(storage writer contract); the frame shows them as 0 (sentinel, as the review frame of R1-B does).

## 3. `json_telemetry` inventory (what the body reads, and how the tape frame supplies it)

Body = `json_telemetry_from` (`web_telemetry.c` 5910-6138). FACT from the read of the body.

| Global or input read by the body | Live (`json_telemetry`) | Tape frame (`sa_rv_frame_tape`) |
|---|---|---|
| `g_telemetry` session fields via `t->` (about 60 fields incl. `cars[]`, `seg_state[]`, `tread[]`, `tire_temp[][]`, `brake_temp[]`) | the live struct | scratch `sa_rv_tp` (memset per frame; `g_telemetry` never written) |
| `g_telemetry.track_name` | live | `h->track` (file header) |
| `g_telemetry.session_time`, `tick` | live | `t` (clamped), `tick` = ticks before t (approximate) |
| `capture_sequence` -> `seq` | under the lock, in the wrapper | approximate tick index (`sa_rv_ticks_before`), DERIVED |
| `capture_generation` -> `gen` | under the lock | FNV-1a hash of the review id |
| `GetTickCount() - capture_time` -> `age_ms` | under the lock (0 when no sample) | 0 |
| `*(volatile int *)(ADDR_AI_DB_BASE + OFF_WP_COUNT)` -> `wp_count` (memory read) | under the lock | 0 (waypoints are not in the file) |
| `g_lap_history_dropped[i]` -> `lap_dropped[i]` | copied under the lock | zeros |
| `tyre_refresh()` / `g_tyre_car_brand[]`, `g_tyre_brand[]` (tyre name cache, HTTP thread) | `tyre_refresh()` before the lock | `tyre_refresh_names(cnt, nm, cn, num)` with the driver names (tyre brand from the name, team, race number) |
| `json_pch_from(pchbuf, sizeof, t)` | `t` = live | `t` = scratch (pch from the player's channels 24-63) |
| `sector_of_t(t, ...)` | `t` = live | `t` = scratch (`lap_len` 0 -> sector 0 for every car: sentinel) |
| constants `TELEMETRY_VERSION`, `RACE_MODE_CHAMPIONSHIP`, `LF_RPM_FACTOR`, `MAX_LAP_HISTORY`, `MAX_SEGMENTS`, `MAX_CARS`, `NAME_LEN` | same | same |
| `esc_json`, `tyre_name_for` (reads the tyre caches) | same | same |

Result: the body reads no capture state and no memory except through its arguments and the tyre caches (HTTP thread only).
The live output is unchanged (section 4).

## 4. Byte identity of the live frame (2.3)

Method: `runtime_test.exe json-golden <file>` writes `json_telemetry()` of a fixed 24-car state (`json_golden`, `runtime_test.c`
4860: session 7, 24 cars, names, positions, speeds, wheel data; `capture_sequence = 0` so no clock is read; `capture_generation = 3`).
The golden was taken with the build before the split, then again after the split, from the same source tree apart from the split.

| File | Bytes | SHA-256 (first 16 hex chars) |
|---|---|---|
| `%TEMP%\TAPE_build21\golden_before.json` (before the split) | 34928 | `ab875556fce998ef...` |
| `%TEMP%\TAPE_build21\golden_after.json` (after the split) | 34928 | `ab875556fce998ef...` |

`cmp` reports the two files identical. The golden run is deterministic (two runs compared).

## 5. Tests (2.5)

Build (offline harness, no game attached; run from `%TEMP%` so the test output stays out of the workspace, see section 9):
```
gcc -O1 -static -static-libgcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wl,--image-base,0x10000000 -Wl,--export-all-symbols -I<tools/sdk> -I<mods/sdk> -o runtime_test.exe runtime_test.c legacy_formats.c sha256.c f1c_broker.c -lws2_32 -lkernel32
```
Exit 0 (warnings are errors). The same flags are used by `build.bat` for the DLL (`-shared -O2`): compiled manually, exit 0.

Full runtime suite (`%TEMP%\TAPE_build21\run13.txt`, exit 0; earlier runs `run11.txt` and `run12_1.txt`, `run12_2.txt` also see below):
```
REPLAY 2.1 info: 1962 bytes, 2 pit and 3 lap events, driver names read
REPLAY 2.1 window: 9560 bytes for 3 cars, 10 s at 5 Hz (car 0 row at 10.000 s checked)
REPLAY 2.2 review docs: track, pits (t), incidents (t), driver at 11 s and 20 s: PASS
REPLAY 2.3 frame: 10 s, 12 s, 25 s and the start, 5965 bytes at 10 s: PASS
REPLAY 2.1 old format: Car N names, no driver EVENT: PASS
PASS 7686895 checks
```
Flake (OPEN, INFERENCE: timing): in `run11.txt` (the first full run that reached the storage writer after the split) the check
`t2_rep.ticks == expect_samples` (`runtime_test.c` ~3132, soak test `t3_check_file`) failed once with `FAIL line 3132`. The same
suite passed in `run12_1.txt` (`PASS 7598740 checks`, before the pits `add` fix), `run12_2.txt` and `run13.txt` (`PASS 7686895 checks`, after it). No change in the
writer path affects that check (`t3_set_tick` clears every car each tick, so no driver EVENT is written there). The test is
timing based (soak, `Sleep`).

Test coverage of the new code (what the runtime test proves):
- 2.1: index walk with driver, pit, incident and lap EVENTs; names (driver EVENT, team, `Car N` fallback); outline non-empty;
  5 Hz window rows, the last tick always listed, `null` for absent channels, the exact row at 10.000 s; `sa_rv_window_args`
  (clamp, 400 for 31 s, t0 >= t1, NaN, +inf, missing).
- 2.2: `/track` shape; `/pits` item with `li`, `lo`, `done`, `t`; `/incidents` item with `ty`, `a`, `t`; `/driver` at 11 s
  (pit open, no laps) and 20 s (two laps, one pit, driver form without id/n/t).
- 2.3: frame at 10 s, 12 s, 25 s and at the start: lap, position, name, speed, x, z of the written ticks, `tape`
  and `review` objects, `t_first` and `t_last`.
- Not in runtime_test (HTTP): the gates 404/409/503, the 400 messages through the route, `/weather` 404 text. These are in
  `test_telemetry.py` (updated, not run, OPEN).

Contract validation (`cv.validate`, real files, `%TEMP%\REPLAY_bench\`, all 8 recordings, `validate_all_sessions.txt`):
every document of section 2 validates: `info` (replay_info), `window` (replay), `frame` (telemetry, with `tape` and `review`),
`track`, `pits`, `incidents`, `driver` (driver). `ALL VALID` for each of the 8 sessions.
Also `contract_validate.py` self-test and schema contract: `self_test True`, `schema contract True`, version `B3C2F462...` consistent.

## 6. Timings and sizes on real files

Measured with `runtime_test.exe replay-bench <file> <t>` (the same builders the HTTP routes call; the HTTP path itself was not
timed). Machine: this PC, game running in the background. Index build is a one-time cost per open file.
Full table: `%TEMP%\TAPE_build21\bench_all_sessions.txt`. Largest file (58130507 bytes, 1881 blocks, 31520 ticks, 15.09 Hz):

| Step | ms | bytes |
|---|---|---|
| index (once per open) | 452-457 | - |
| `/replay/info` | 45-46 | 41035 |
| `/replay` 10 s window (3 places) | 4.2-6.2 | 48541 / 51022 / 50954 |
| `/replay/frame` (start, 50 %, 90 %, end) | 8.1 (first, includes the name cache), 0.95, 1.26, 0.11 | 35626 / 34564 / 34549 / 4841 |
| `/track` | 29 | 7268 |
| `/pits` | 15 | 4324 |
| `/incidents` | 8 | 182 |
| `/driver` car 0 at the end | 12 | 2168 |

Other sessions (ms for info / frame at mid / pits, bytes of the frame):
- 20-54 Race (1.14 MB, 4095 ticks, 0 laps): info 0.6, frame 0.6 (35765 B), track 0.03 (empty outline: no complete lap, FACT).
- 19-12 Practice (21.7 MB, 892 blocks): info 21.0, frame 1.2 (37308 B), pits 6.4, driver 5.8.
- 19-57 Practice (1.1 MB): info 0.4, frame 1.0.
- 20-00 Practice (14.1 MB, 603 blocks): info 20.7, frame 1.1, pits 3.9.
- 20-16 CO-OP Warmup (1.1 MB): info 0.4, frame 0.9.
- 20-14 Qualifying (84 KB, 5 blocks): info 0.2, frame 0.8 (41117 B).

Every frame of every session is far below the 20 ms bound (2.3 cost bound). Largest window: 51 KB; largest info 41 KB.

## 7. Menu-state check (2.4, FACT, read in code; no change)

With the game at the menu (no race, `g_telemetry` not capturing), the routes answer from the file and the registry only:
- HTTP server: `http_thread` is started by `compatibility_runtime.inc:177` after broker discovery and registration, not on a
  session event. The console page and `/recordings` do not check a session (the registry is `sa_rid_find`, `storage_api.inc`).
- Review gates: `sa_rv_id_check` (`storage_api.inc` ~1893) and `sa_rv_ready` (~1906) use only the registry, the file system and
  the index slot. `storage_api.inc` mentions `g_telemetry` only in comments (checked with grep). The index build starts from
  `sa_rev_start_locked` (no capture state). So 404/409/503 and the 2.3 frame do not depend on the game.
- `json_telemetry` (live `/telemetry`) at the menu: unchanged behaviour (the live state is zero-filled; `vehicle_count` from
  game memory). The 2.3 tape frame never reads it.
- `tyre_walk` (`web_telemetry.c` ~5730) reads the game's SEASONDATA folder on disk (`tyre_game_path`), not game memory.
- Recordings listed at the menu: the list comes from the storage folder (`sa_list_folder`), no session state needed.
- Anything gated on a session: the live `/telemetry` and `/trace/*` answer with the menu values (existing behaviour); the review
  routes are unaffected.
Conclusion: no code change for 2.4 (the "fix only if small" rule was not triggered). OPEN for the owner live check (A1).

## 8. Deviations from the spec (all deliberate, all listed)

1. `/replay/info` is not cached: each request reads the file (45 ms on 58 MB). Allowed by the spec ("cache ... allowed").
2. `/pits`, `/incidents` `t`: added as the contract clarification asks. `incidents` keeps `tk` (approximate tick count before the
   event, DERIVED), because the live `tk` is a tick counter the file does not hold exactly.
3. `/laps` (review): no `t` added. The `/laps` item is a car with a list of game lap times; a per-lap end time would change the
   shape of `laps` (a list of numbers). The clarification allowed it only if cheap; it is not cheap, so it is not done (OPEN for Claude).
4. `/driver` pits: the driver form of the pit item (no `id`, `n`, `t`), because `driver.json` pit items are strict.
5. `/pits` and `/incidents` items keep the live field names; fields the file does not hold are 0 (sentinel), `add` only in the
   driver form (the `/pits` schema is strict and has no `add`).
6. `/track`: `wc` = number of outline points (the live `wc` is the waypoint count; the file has no waypoints).
7. `sample_sequence` in a tape frame = ticks before t (approximate tick index). Spec: "tick index". DERIVED.
8. `session_generation` in a tape frame = FNV-1a of the review id (spec: "hash of the id").
9. `hz` = ticks / span (15.09 on the 58 MB file). The spec example `20` is not the real rate (REPLAY record 0b).
10. `events` = lap, pit, incident only (not `session_start`, `session_end`: no `car` field).
11. Lap event `lap` = completed lap (`prev_lap`), the index rule. The spec example is ambiguous (`lap`, with `last_lap_s`).
12. Incident item `a` and `b` (not in the spec): `a` = partner slot (contact) or track-limit count (limits), `b` = 0.
13. Role map: spec mapping (channel 98: 0 player, 2 remote, else ai). The live `/telemetry` has no "player" string.
14. The tape frame has the `review` object (as the R1-B frame) and the `tape` object; `telemetry.json` gains `tape` as optional.
15. `tape.t` is the requested t clamped to [t_first, t_last] (the server returns the clamped value; the viewer should read `tape.t`).

## 9. Open items (owner or Claude)

- OPEN, gate: run `$env:TELEMETRY_TESTS='1'; .\build.bat` in PowerShell 7 in the component dir with the game CLOSED. It runs the
  new `test_telemetry.py` HTTP checks (section 1). If it stops at the payload pin: `python pin_sidecars.py --payload "$env:TEMP\f1c-web-telemetry-4.4.0"`, then rerun.
  `test_telemetry.py` ran nowhere in this session.
- OPEN, team semantics: the driver EVENT `team` is `car_name` (veh+0x4678, "car/team name" in the source). INFERENCE: the value is the
  car/team text; not checked against a live value. Owner check: a race with the team name visible in the viewer.
- OPEN, `/laps` `t` (deviation 3).
- OPEN, the storage soak flake (section 5, one failure in four full runs). Claude to decide whether to tighten or retime `t3_check_file`.
- OPEN, the frame's `sample_sequence` (approximate). Not a correctness issue for the viewer if `tape.t` is used.
- OPEN, the info document is not cached (45 ms on 58 MB). Acceptable; a cache is an option if the viewer polls `/replay/info`.
- OPEN, the pre-split live path has not been run through the HTTP server (only the golden bytes). The gate covers the HTTP `/telemetry`.
- OPEN, owner live check A1-A4 (TAPE spec section 7) after the gate.

## 10. Restoration

Source files: copy back from `%TEMP%\TAPE_before_2026-10-14\server\` with `cp -p` (paths relative to the component dir):
`storage_api.inc`, `storage_v2.inc`, `storage_writer.inc`, `web_telemetry.c`, `runtime_test.c`, `test_telemetry.py`,
`contract_validate.py`, `API_CONTRACT_2026-10-09.md`, `schemas/telemetry.json`, `schemas/version.json`.
Check: `sha256sum -c SHA256_TAPE_BEFORE.txt` (lists the hashes of the before-state; `API_CONTRACT` and `version.json` included).
Schemas `schemas/pits.json` and `schemas/incidents.json`: the before-state is `...\server\schemas\*.RECONSTRUCTED_BEFORE` (made by
removing the inserted `t` property; no hash of the original was taken, so this copy is not byte-verified).
New files (agents may not delete): `schemas/replay_info.json`, `schemas/replay.json`. They are used only by the replay routes;
after a restore they must be moved to quarantine by the owner (retention policy), not deleted by an agent.
After restoring the schemas, the schema-set hash returns to the value in the before copy of `version.json`.
Record: `REPLAY_SERVER_RECORD_2026-10-14.md` before-state at `%TEMP%\TAPE_before_2026-10-14\server\records\`.

## 11. Workspace hygiene (found during this run; nothing deleted)

- `__pycache__/` (contract_validate and test_telemetry `.pyc`, 22:32) was created by my `py_compile` and import runs in the
  component dir. Disposable; for the owner's `retention.py purge` (quarantine, not deleted by an agent).
- `telemetry.json` in the component root (148613 bytes, 22:50) is written by `runtime_test.c:180` (`fopen("telemetry.json","w")`,
  an existing test) when the suite runs from the component dir. The suite runs in this session were started from the component dir,
  so the file was overwritten by each run (last at 22:50). Disposable test output; the previous content is not recoverable from this run.
  Next runs must start from `%TEMP%` (cd there first).
- All bench output, golden files, build outputs and the copies of the recordings are in `%TEMP%` (no bloat in the workspace).

## 12. Claude review follow-up (2026-10-14)

Tests run from `%TEMP%` (cwd), so `telemetry.json` is no longer written into the component dir. The runtime test is compiled in the
component dir (embedded JS is relative to the cwd) and the executable is written to `%TEMP%`.

Changes:
- 0. `sa_rv_real` fix: Claude's (f32 decode of RAW32 floats, before-copy `%TEMP%\TAPE_before_2026-10-14\claude\storage_api.inc`).
  Runtime test added: fixture channel 86 (`last_lap`, f32 98.5 for car 0) and channel 24 (`ride_mm`, f32 12.5, car 0 only).
  Checks: frame `"ll":98.500` and `"pch":{"v":1,"rh":[12.50,` round trip. PASS.
- 1. `tt` per car in `/replay/frame` (`sa_rv_frame_tape`, storage_api.inc, block after the car loop): sim time of the car's newest lap
  EVENT at or before t, minus `t_first` (the first tick of the file; the `session_start` EVENT is not read, so t_first is the
  session start used). 0 when the car has no lap EVENT yet. DERIVED (line-crossing time). Live meaning: `tt` is `total_time`,
  read from veh+0x4098 (`web_telemetry.c` define `OFF_TOTAL_TIME_SF` at line 788, read at 5308; the gap code at ~6772 says
  "Gap/interval from total_time (+0x4098)"). Frame tests: `"tt":0.000` at 10 s, `"tt":12.000` at 12 s, `"tt":18.000` at 25 s.
- 2. `bl` per car: min `last_lap_s` > 0 over the car's lap EVENTs at or before t; 0 when none. Tests: 0.000 at 10 s, 12.000 at 12 s,
  6.000 at 25 s (lap 2 took 6 s). `ll` stays channel 86 (live `last_lap`).
- 3. ls1, ls2, bs1, bs2 are NOT written: `sw_car_from` has no sector times (channels 84 and 85 are sector deltas, not sector times).
  Left at 0 (sentinel); nothing invented.
- 4. Role from the index walk: `sv2_idx_apply_data` (storage_v2.inc, the `ctl` flags of `sv2_idx_car_t`) sets bit 0 when channel 98
  is 0 at a tick and bit 1 when it is 2. `sa_rv_role_of`: `remote` if bit 1, else `player` if bit 0, else `ai` (remote wins).
  No extra read. Cost: index build 452-454 ms before and after (no measurable change). Real CO-OP file (58 MB): car 0 `player`,
  car 1 `remote`, cars 2-23 `ai`. Test: fixture car 0 has channel 98 = 2 for its first 100 ticks, then 0 -> `"role":"remote"`;
  car 1 = 1 -> `ai`.
- 5. `tt` and `bl` use the cached EVENT list (`sa_rv_names_for`, sorted by t; the scan stops at the frame t). No extra file read.
- 7. `session_generation` = 0 in `/replay/frame` (was FNV-1a of the id). Reason (Claude review, FACT from the browser on the real CO-OP
  race): the review trace docs carry `gen` 0 (storage_api.inc ~937, 960, 979, 1062, 1067) and the viewer drops seeded laps whose gen
  differs from the frame. Same value as `sa_rv_frame_rest`. DEVIATION REVERSED: TAPE spec 2.3 asked for a hash of the id; withdrawn
  by Claude. Test: `"session_generation":0,` in the frame.
- Also: the `sa_rv_frame_tape` `(void)q` remains from the hash loop removal (harmless).

Tests (from `%TEMP%`, `runtime_test.exe`, exit 0):
```
REPLAY 2.1 info: 1966 bytes, 2 pit and 3 lap events, driver names read
REPLAY 2.1 window: 9560 bytes for 3 cars, 10 s at 5 Hz (car 0 row at 10.000 s checked)
REPLAY 2.2 review docs: track, pits (t), incidents (t), driver at 11 s and 20 s: PASS
REPLAY 2.3 frame: 10 s, 12 s, 25 s and the start, 5920 bytes at 10 s: PASS
REPLAY 2.1 old format: Car N names, no driver EVENT: PASS
PASS 6973320 checks
```
The check count differs from earlier runs (7686895, 6986603): the soak tests count their pushes by timing.
Live `ll`, `bl`, `tt` values: validation of the 58 MB frame against `telemetry.json`: ALL VALID (the schema is unchanged).
DLL `-shared -Werror` after the change: compiled (exit 0).

Timings on the 58 MB real file (`replay-bench`, from `%TEMP%`, two runs, `bench_followup_1.txt`, `bench_followup_2.txt`):
| Step | ms (run 1 / run 2) | bytes |
|---|---|---|
| index (once per open) | 452.9 / 454.5 | - |
| `/replay/info` | 37.4 / 37.7 | 41039 |
| window 10 s | 4.3-5.9 / 4.2-5.9 | 48541-51022 |
| `/replay/frame` first (includes name cache) | 8.0 / 8.2 | 35215 |
| `/replay/frame` 50 % / 90 % / end | 1.0 / 1.4 / 0.12 (run 1) | 33238 / 33186 / 4735 |
| `/replay/frame` tq | 1.3 / 1.2 | 33229 |
| track / pits / incidents / driver 0 | 21 / 14 / 8 / 12 | 7329 / 4324 / 182 / 2168 |

Frame target (< 20 ms): met (0.1-8.2 ms; the 8 ms is the first call of a review id).

Open (not changed here): item 3 (sector times not recorded; the writer would need a sector channel: owner/Claude decision).
