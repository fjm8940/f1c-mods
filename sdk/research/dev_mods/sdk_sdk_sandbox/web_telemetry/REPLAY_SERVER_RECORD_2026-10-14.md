# REPLAY server record (H-S): spec section 2, 2026-10-14

Product: Telemetry (`web_telemetry`), 1.2.0 (unreleased), API 10 (unchanged). Author: Matias.
Spec: `REPLAY_SPEC_2026-10-14.md` section 2 (contract 2.1/2.2 fixed). Previous worker: `RECREV_REAL_RECORDINGS_2026-10-14.md`.
Tags: FACT = observed in this run (command output, file bytes, source line). INFERENCE = reasoned, to verify. OPEN = owner or Claude decision.
Stage: implement (server) -> verify. Viewer (H-V) not touched.

## 0. Step 0 answers (investigation, before building)

Scratch decoder: `%TEMP%\REPLAY_step0\dump_v2.c` (includes `web_telemetry.c` with `TELEMETRY_TEST`, same flags as the
runtime_test link line; uses the component's own `sv2_*` reader). Outputs: `%TEMP%\REPLAY_step0\s1_dump.txt`, `s5_dump.txt`.
No new Python decoder was written.

### 0a. Session 1 (Race 20-54, 1143700 bytes), decoded mid-session (FACT)

Header: flags 0x22c, compression 3 (XPRESS), `channels_declared=125`, track AlbertPark, session_type 7, `block_ticks_max=20`.
Channel table ids 0..124 all present (FACT, dump "TABLE count=125 ids: 0 ... 124").

Values, block tick nearest the wanted time (FACT, dump "SAMPLE" lines):

| car (slot, scope) | t=10 s: pos68 / lap69 / x / z / spd2 / race97 / role98 | t=140 s | t=270 s |
|---|---|---|---|
| 0 (player, scope 1) | 1 / 0 / -576.3 / -510.4 / 0.00 / 7 / 3 | 1 / 0 / 425.9 / -785.1 / 0.00 / 7 / 3 | 1 / 0 / -480.2 / -96.3 / 0.00 / 7 / 3 |
| 1 (AI) | 2 / 0 / -217.7 / -522.6 / 0.00 / 8 / 3 | 2 / 0 / 434.7 / -799.1 / 0.00 / 8 / 3 | 2 / 0 / -471.4 / -137.2 / 0.00 / 8 / 3 |
| 2 (AI) | 8 / 0 / -171.1 / -0.5 / 0.00 / 1 / 3 | 8 / 0 / 954.8 / -150.3 / 0.00 / 1 / 3 | 8 / 0 / -185.7 / -504.6 / 0.00 / 1 / 3 |

Whole-file per-channel stats for slot 0 (FACT, dump "CHAN slot=0"): id 68 changes 11 times (1..23); id 69 (lap) min=max=0,
0 changes; id 2 (speed), 1 (dist), 3 (throttle), 16 (gear), 15 (rpm 1044 constant): 0 or constant; id 17/18 change ~3500 times.
Role 98 = 3 and race 97 constant for all 24 slots (FACT, dump "role98" lines, 24 of 24 slots).

Channels 64-124 (FACT): the channel table declares 125 channels and the per-car masks carry them. Slot 0 has
`id=64..68,69,70,72,74,75,77..79,80..83,84..88,93..97,98,99,100,101` present with 4095 values; union for slot 0 is
122 channel ids. The `hi` mask is 0x1fffffffffffffff for slot 0 (ids 64..124). `ANY_HI_BITS=1`.
**`channels_seen` is not a channel summary.** `sa_rv_declared_trf` (`storage_api.inc:873-878`) maps the file's declared
channel table to TRF bits (`sa_rv_trf_of`, `storage_api.inc:851`), which only covers ids 1-4, 5-18, 20-23. Result
0xF7FF = TRF bits 0..10 and 12..15 (DIST..TREAD, RPM, GEAR, TRACK, TTEMP). Bit 11 (SEGMENT) is never set by this
function; bits 16..20 (RX_*) are not mapped at all. The same value is printed for every car because it is computed from
the file table, not from the car's mask (`storage_api.inc:955`, `:1039`, printed at `:969`, `:1061`). Not a 16-bit cap.
Fix proposal (not applied, out of section 2): compute a per-car mask from `sv2_car_t.mask_lo/mask_hi` or report the
channel ids; `channels_seen` in the 1.0.1 live trace (`web_telemetry.c:2061`) uses the same TRF meaning.

### 0b. 4095 ticks, 282 s, zero lap EVENTs (FACT unless tagged)

- 4095 is the sum of DATA block ticks: 204 blocks of 20 + blocks of 11, 2, 1, 1 (= 4080 + 15). The END block payload is
  `d0 00 00 00 ff 0f 00 00 00 00 00 00` = data_blocks 208, ticks 4095 (u32 at +4), dropped 0 (u32 at +8)
  (`storage_writer.inc:1121-1123`, dump "END seq=212"). No 12-bit or 4095 literal exists in `storage_*.inc`
  or `web_telemetry.c` (grep `4095|0xFFF`): **coincidence, not a cap.** INFERENCE: the 15 ticks in the partial
  blocks come from flushes (a car-set or mask change in `sw_block_add`, `storage_writer.inc:827-840`, and every EVENT
  flushes the open block first, `storage_writer.inc:878-885`).
- Ring drops: 0 (END dropped field 0; ring drop path is `storage_writer.inc:96-97`, only SAMPLE when used >= 40).
- The rate is about 14.5 Hz. Samples are taken only when the sim time advances (`storage_writer.inc:153-181`,
  `adv`), and the capture calls `sw_capture_tick` once per fresh broker sample (`compatibility_runtime.inc:237`).
  The CO-OP race (session 5) averages 15.09 Hz with 31520 ticks in 2088.7 s (FACT, `s5_dump.txt`). So **the real rate is
  about 15 Hz, not 20** (the spec example `"hz":20` is wrong for real files; the info document will report the measured
  rate). INFERENCE: the capture sees fresh sim advances at about 15 per second.
- Channel 69 (lap) never changes in session 1 (FACT: min=max=0, changes=0 for slots 0-23 in `s1_dump.txt`).
  The writer emits a lap EVENT only when `lap != p->lap` (`storage_writer.inc:916`), so **no EVENT is the correct output
  for this data**. The writer is not dropping lap EVENTs. The value stays 0 upstream.
- Why the lap stays 0 (INFERENCE, OPEN for owner): controller role 98 = 3 for all cars and speed/throttle/gear are 0 for
  all 282 s. The source comment `web_telemetry.c:808-812` lists `3 replay` as a controller mode, so this session was
  probably recorded from a replay, not a driven race. Owner check needed: what was on screen during 20-54?
- Control check (FACT): session 5 (CO-OP Race 20-17) has the same writer, 395 lap EVENTs, 133 pit EVENTs and speeds up to
  301 km/h (`channel 2` max 45677 raw = 83.6 m/s). So the writer and reader work on a real race.

### 0c. Other facts found on the way (FACT)

- F4 in the spec is wrong: names ARE captured. `read_telemetry` writes `c->name` from `veh+0x4620` and `c->car_name`
  from `veh+0x4678` (`web_telemetry.c:5262-5272`). Names are not written to the v2 file (no name channel), so driver
  EVENTs (2.3) are still needed.
- Controller mode (`web_telemetry.c:813-817`): `OFF_CONTROLLER_MODE 0x32C8`, CTRL_LOCAL 0, CTRL_REMOTE 2; comment gives
  -1 inactive, 0 local human, 1 AI, 2 remote, 3 replay. Live `/telemetry` writes the raw value as `"cr"` (`:6061`), the
  AI flag as `ai_control` (`:2115`), remote as `controller_role == CTRL_REMOTE` (`:2116`).
- Lap EVENT with `last_lap_s = -1.000` exists (session 5, car 1 lap 2 at t=154.366): the game lap time is unknown. The index
  rejects `game < 0` (`storage_v2.inc` sv2_idx_apply_event), so that lap uses the sim-time difference, as designed.
- Writer partial blocks: each EVENT forces a DATA flush (`storage_writer.inc:878-885`). Session 5: 1881 DATA blocks, 1335 of
  them full. Size and index cost only (FACT). Not a bug, noted for the record.

### 0d. Writer issues found (not fixed; section 2.3 only changed the EVENT path)

- W1 (no change needed): `sw_events_from_sample` compares lap numbers only (`storage_writer.inc:916`). Correct.
- W2 (proposal): capture drops are only counted in the ring, not per session when the file is open: no drop observed here
  (dropped 0), so no action now.
- W3 (proposal, OPEN): `channels_seen` meaning (0a) and the `ticks` = 0 in `/recordings` for v2 (R1-A record RC-2) remain.

## 1. Changed files and what they do (2.1, implemented 2026-10-14 by T-S)

Before-state of every file: `%TEMP%\TAPE_before_2026-10-14\server\` (hashes in `SHA256_TAPE_BEFORE.txt`). The REPLAY
before-copies (`%TEMP%\REPLAY_before_2026-10-14\server\`) were not touched.

| File | Lines (this tree) | Change (FACT: source read after the edit) |
|---|---|---|
| `storage_writer.inc` | 30, 32, 54-56 | `SW_DRV_TEXT` 64, enum `SWE_DRIVER`, `sw_event_t` fields `drv_name`, `drv_team`, `drv_num`, `drv_slot` |
| `storage_writer.inc` | 305-336 | capture side: `sw_drv_sent[]`, `sw_driver_check()`: one control event per slot when its name, team or race number changes (under `g_telemetry_cs`, no allocation, deferred when the ring is full) |
| `storage_writer.inc` | 341-370 | `sw_emit`: reset of `sw_drv_sent` at SWA_OPEN (each session writes every driver again); `sw_driver_check(now_ms)` before the SAMPLE claim |
| `storage_writer.inc` | 982-988 | `sw_write_driver_event()` (writer thread): `{"type":"driver","t","car","name","team","num"}`, names through `sw_json_body` |
| `storage_writer.inc` | `sw_apply` | `case SWE_DRIVER` (written only while a session is open) |
| `storage_v2.inc` | 1578-1609 | `sv2_event_walk_rd()`: EVENT blocks in file order; other blocks: header only; EVENT: full CRC and decode (`sv2_rd_frame`) |
| `storage_api.inc` | 1120-1504 | REPLAY 2.1: `sa_rv_ev_cb` (lap, pit, incident, driver), `sa_rv_events_load`, `sa_rv_newest` (ch 97, 98), `sa_rv_span`, `sa_rv_outline_lap` + `sa_rv_outline` (decimation: spacing max(8 m, L/590), at most 600 points), `sa_rv_info_doc`, `sa_rv_window_doc` (5 Hz rule, last tick always listed), `sa_rv_parse_num` |
| `storage_api.inc` | 1492-1503 | `sa_rv_window_args()`: finite, t0 < t1, <= 30 s (400 `window too long`), clamp to the span (pure; used by the route and by the runtime test) |
| `storage_api.inc` | 1926 (signature) | `sa_api_review_route()` takes `q_id`, `q_t`, `q_t0`, `q_t1`; branches `/replay/info`, `/replay` (dispatch around 1959-1973) |
| `storage_api.inc` | route list | `sa_rv_is_api_route()`: `/replay`, `/replay/info`, `/replay/frame` added |
| `web_telemetry.c` | 9306 | dispatcher (review route call): query `id`, `t`, `t0`, `t1` passed to `sa_api_review_route` (`qs_get`, NULL when absent) |
| `schemas/replay_info.json` | new | `f1c.replay.info/1` (strict: additionalProperties false; event fields optional by type) |
| `schemas/replay.json` | new | `f1c.replay/1`: `cols` const, 12-item rows (prefixItems: t number, 11 values number-or-null) |
| `schemas/version.json` | | schema set re-hashed with `python -I schemas/update_version.py 10` (API stays 10); CRLF kept |
| `contract_validate.py` | end of `check_schema_contract` | asserts for the new schemas and the optional `t` (2.2) |
| `runtime_test.c` | 4224-4300 | fixture switch `r1a_driver` (default 0 = R1-A file unchanged); with 1: driver EVENT (slot 0 "Alex Test", team "Team A", num 7), pit EVENTs (car 0, 10 s in, 12 s out), positions (ch 2, 17, 18, 68) |
| `runtime_test.c` | 4727-4858 | `r21_test_replay()`: info, window, args, old-format pass (2.1 part) |
| `runtime_test.c` | `r21_test_replay` | see TAPE record for the 2.2 and 2.3 checks |

Facts used (FACT, read in this run):
- Driver names: `c->name` from veh+0x4620 and `c->car_name` from veh+0x4678 (`web_telemetry.c` read_telemetry; comment at
  the OFF_CAR_NAME define says "car/team name"). INFERENCE: `team` in the driver EVENT is this car/team text; its content
  was not checked against a live value (OPEN, see section 4).
- Role (channel 98 = `controller_role`): 0 local human, 2 remote (CTRL_REMOTE), 1 AI, 3 replay (comment `web_telemetry.c:808-817`).
  The `/replay/info` role map is the spec's: 0 player, 2 remote, else ai. Live `/telemetry` has no "player" string; it sends
  `cr` (raw), `ai_control` and `controller_role == 2` (`web_telemetry.c:2115-2116`, `:6078`).
- Scope flag in the file: `scope 1` = player (`sw_car_from`, `storage_writer.inc:203-206`). Not used for role (spec map).
- Session 1 of 2026-10-14 (the Race 20-54): channel 98 = 3 for all 24 slots (REPLAY_SERVER_RECORD 0a) → role `ai` for all.

Behaviour decisions (2.1):
- `events` = lap, pit, incident EVENTs only (spec list); `session_start` and `session_end` are not listed (no `car` field).
- lap item: `lap` = the COMPLETED lap (`prev_lap`, the index rule `sv2_idx_apply_event`), `last_lap_s` = game lap time.
- `hz` = ticks / (t_last - t_first), two decimals (real file: 15.09; the spec example 20 is not the real rate).
- `num` = the driver EVENT race number, else channel 97 (last value), else -1.
- `laps` = last channel 69 value of the slot (`ix->car[s].lap`).
- Index: `sv2_idx_apply_event` already ignores every type except `"lap"` (checked, `storage_v2.inc` `sv2_idx_apply_event`); no change needed.
- No cache of the info document: each request walks EVENT headers and reads the outline lap (45.8 ms on the 58 MB file, section 3).

## 2. Tests (2.1)

Commands (run from the component dir, game running: runtime_test does not attach to the game; it is an offline harness):
```
gcc -O1 -static -static-libgcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wl,--image-base,0x10000000 ... runtime_test.c ...   -> exit 0
runtime_test.exe  (full suite, 2026-10-14 22:4x)
```
Output lines (exact, from the run that passed, `%TEMP%\TAPE_build21\run13.txt`):
```
REPLAY 2.1 info: 1962 bytes, 2 pit and 3 lap events, driver names read
REPLAY 2.1 window: 9560 bytes for 3 cars, 10 s at 5 Hz (car 0 row at 10.000 s checked)
REPLAY 2.2 review docs: track, pits (t), incidents (t), driver at 11 s and 20 s: PASS
REPLAY 2.3 frame: 10 s, 12 s, 25 s and the start, 5965 bytes at 10 s: PASS
REPLAY 2.1 old format: Car N names, no driver EVENT: PASS
PASS 7686895 checks
```
Gaps: the HTTP route plumbing (gates 404/409/503, the `sa_api_review_route` dispatch) is not exercised by runtime_test
(registry needed). It is in `test_telemetry.py` (section 2.5 of the TAPE record), which is part of the gate that did not run.

## 3. Timings and sizes on real files (2.1, measured)

Real file: `AUSTRALIA 1997 - CO-OP - Race - 2026-10-14 20-17_2.f1t` (58130507 bytes, 1881 DATA blocks, 31520 ticks, span
0.000-2088.743 s, 15.09 Hz), copy in `%TEMP%\RECREV_2026-10-14`. Bench: `runtime_test.exe replay-bench <file> 1200`
(runs the builders on the file; not HTTP). Output: `%TEMP%\TAPE_build21\bench_coop_race.txt`.
```
BENCH index                  code=0 ms=452.47 bytes=58130507     (sv2_index_build_rd, once per open)
BENCH replay/info            code=200 ms=45.76 bytes=41035
BENCH replay window 10s      code=200 ms=4.21 bytes=48541       (start)
BENCH replay window 10s      code=200 ms=6.29 bytes=51022       (50 %)
BENCH replay window 10s      code=200 ms=5.64 bytes=50954       (90 %)
```
Validation of the dumped documents (`validate_bench.py`, real files): `info` against `replay_info.json`, window against
`replay.json`: VALID (see TAPE record 2.5).
