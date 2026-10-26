# H10 - Player lap signals for clean-lap skill rating (static)

Date: 2026-10-15. Agent: Haiku (H10). Stage: investigate only.
Scope: static corpus + existing mod source. No game process touched, no live reads, no writes
outside this file. `api1.txt` and keys not read. Owner question: which data lets a mod rate the
player's skill from clean laps (player = slot 0, vehicle base 0x5EC6D8, stride 0x48C0).

## Method and path corrections
- Task said `development\mods\sdk_sandbox\web_telemetry`. Actual: `f1c_ida_decompiled\mods\sdk_sandbox\web_telemetry\web_telemetry.c`. `development\mods` has no sdk_sandbox. Player-lap readers also exist in `f1c_ida_decompiled\mods\f1cui_web_remote\web_telemetry.c` (older copy, uses 0x4048 for distance).
- Task listed "player physics mirror 0x7C7428 + 0x3984" for fuel. No corpus evidence supports fuel at that object. 0x7C7428 is the static player physics object (stride-1124 wheel records, engine fields +7257..+7660). Fuel is `veh+0x3984`. See Conflicts.
- Absolute player address = 0x5EC6D8 + offset. Example: lapNumber 0x5F0714, off-track byte 0x5F07C0.
- Tier key: FACT = confirmed live and/or static with citation. DERIVED = inferred from decompile or one source. OBSERVED = seen in live mod trace, not statically proven. OPEN = unresolved.

## 1. Signal table (player validity in last column)

| Signal | Offset / type | Tier | Source (file:line) | Player valid? |
|---|---|---|---|---|
| Lap count | +0x403C int32 | FACT | f1c_sdk.h:139-140; f1c_memory_mcp.py:30 `laps`; readers incl. AIDriverSorting 004bf390 | Yes |
| Line time (elapsed at S/F) | +0x4098 float | FACT | f1c_sdk.h:159-160; AIDriverSorting; findings "Race position ... 0x4098" | Yes, driven laps only. Simulated sessions write it (see caveat A) |
| Best lap | +0x409C float | FACT | f1c_sdk.h:161-162; steward sub_4D1C40 holder logic (DERIVED, findings) | Yes |
| Last lap time | +0x404C float | OBSERVED | sdk_sandbox web_telemetry.c:773, 5307; race_data_observation.h:31 (F1CUI anchor 0x4714-1736) | Yes (used live by mod). Latch needed, see caveat B |
| Last sector S1/S2 (cumulative from line) | +0x4050 / +0x4054 float | OBSERVED | web_telemetry.c:774-775, 1296-1300 (S2 cumulative, S3 = lap - S2) | Yes |
| Best sector S1/S2 | +0x40A0 / +0x40A4 float | OBSERVED | web_telemetry.c:790; race_data_observation.h:34-35 | Yes |
| Sector boundaries | AIW keys, metres from start | FACT (Monza only) | web_telemetry.c:386-390 (1716.87 / 3605.15 / 5781.26) | Track data, not per-car |
| Lap distance (metres, resets at line) | +0x4044 float | FACT | f1c_sdk.h:143 (DERIVED tag there, FACT in findings 2026-07-31: 21-car sweep, 3 s delta test); web_telemetry.c:602-609 | Yes. Player 646.82 at both 0x4044 and 0x4048 |
| Lap distance (player-only mirror) | +0x4048 float | FACT (player-only) | findings 2026-07-31; -1.00 on all 20 AI cars | Do NOT use for AI. Use 0x4044 |
| Speed (m/s) | +0x4440 float | FACT | findings "Speed [FACT]" (docs38, f1cui); 10 readers incl. AI | Yes. Mirror +0x60 (signed) player-only, CONFIRMED (findings 2026-07-31) |
| Pit status | +0x3450 int32 | OBSERVED (player) / DISPROVEN (AI) | findings 2026-07-25: AI read 0 in 6653 samples across 20 pit stops; web_telemetry.c:626; findings 2026-07-29 player 3->4 at service completion | Yes for player (live trace). Enum 0-4 is player-only |
| On-track / off-track (cut) | +0x40E8 byte (= sub_4BCCC0 this+0xB4, this = slot0+0x4034) | FACT | findings 2026-07-28 (live 0->1->0 as player ran wide); findings 2026-10-02 (all AI slots 0); sub_4BCCC0_004bccc0.c ~L205-215 | Yes, player only. AI always 0 |
| Off-corridor (AI) | +0x445C int | FACT (AI) | AIRacingLine_00425920.c:302-326; findings 2026-10-03 | No. Only AIRacingLine writes it |
| Cut / track-limit counter | +0x4104 int32 | DERIVED (decompile) + OBSERVED (F1CUI) | sub_4BCCC0 `this+52` (this=veh+0x4034, so +0xD0 = 0x4104), increments on each cut with severity v66>1.75 (L178-200); race_data_observation.h:38 `track_limits` | Yes, player. Gate `dword_7CEF84` unknown |
| Cut-track penalty latch | veh+0x4094 (this+24) | OPEN | sub_4BCCC0 L185-190 sets `this[24]=1`; +0x4094 is lapOfRun float (f1c_sdk.h:157-158) | Conflict. Do not use |
| Cut penalty trigger | rule, not offset | FACT | findings 2026-10-07: 10 s stop-go when count>1 or severity>2.75 and session==7 | Player only |
| Penalties to process | +0x4070 int32 | CONFIRMED | f1c_sdk.h:155-156 | Yes, but it is a queue count, not a lifetime count |
| Penalty handler (player) | sub_4BCCC0 | FACT | findings 2026-07-28 (player-only; this = slot0+0x4034) | Player |
| Yellow flag per segment | steward +0x110 array `{int32 state; float end}` (stride 8), count +0x10C int, scale +0x108 float, base 0x8ACEE0 | FACT (layout, live-verified in race) | web_telemetry.c:322-345; findings 2026-08-01 (sub_4D1730, 29 zones); setter 0x4D0F00 (findings 2026-09-30); state 2 = yellow | Global (not per car). Index with player lap distance (scale = 1/segment length) |
| Pause flag | 0x7C56AC byte | CONFIRMED | f1c_sdk.h:288 `G_PauseFlag`; set by sub_42D210 (findings 2026-10-09, offline only, DERIVED) | Global |
| Sim clock | 0x8AFEA0 double | CONFIRMED | f1c_sdk.h:331 `G_SessionClockSeconds`; web_telemetry.c:1024 | Global. >=1e8 = native finalization sentinel (findings 2026-09-14) |
| Session type | 0x7C56A4 int32 | FACT | f1c_sdk.h:286; findings 2026-07-28 live: 7 = race, 5 = qualifying (seen twice) | Global. docs/38 enum is WRONG (trap) |
| Race phase / race active | 0x7C56A8 / 0x7D0308 | FACT | f1c_sdk.h:309; findings 2026-07-28 (RacePhase 5 in race) | Global |
| Fuel remaining | +0x3984 float | CONFIRMED (not poke-proven) | f1c_sdk.h:113-114; name_index; web_telemetry.c:727 (lap rows sample it) | Yes. Frozen when stopped |
| Fuel use rate | +0x3990 float | CONFIRMED | f1c_sdk.h:115-116 | Yes |
| Tyre compound | +0x397C int32 (tireChoice) | CONFIRMED | f1c_sdk.h:111-112 | Yes |
| Tyre tread (per wheel) | candidates +0x363C / +0x3734 / +0x382C / +0x3924 float | OPEN | web_telemetry.c:688-691 (FL=363C, FR=3734, RL=382C, RR=3924); findings 2026-07-28 CONFIRMED tread 1.0->0.75 with life=(tread-0.75)*4; findings 2026-07-29 CORRECTION: these are puncture 0/1, stayed 1.0; findings 2026-10-09: AI tread = wheel block +0xB0, order not proven | Unsettled. Not needed for clean-lap rule |
| Car condition | +0x3D6C int32 (carConditions) | FACT | f1c_sdk.h:127; findings 2026-07-29 (player crash 0->2 = wheel loss); 1 seen on AI (2026-10-02) | Yes |
| Reason code | +0x4068 int32 | CONFIRMED | findings "reasoncode-not-written"; 2026-07-29 player 0->6 (accident/wheel), 4 = suspension | Yes |
| Finish state | +0x4064 int32 | FACT | findings 2026-07-28 (0 running, 2 exit, 3 DSQ) | Yes |
| Retired | +0x4410 int32 | FACT | f1c_sdk.h:187-188 | Yes |
| Race position | +0x4060 int32 | CONFIRMED | findings 2026-07-31 (sorted by laps then lap distance reproduces 0x4060 exactly) | Yes |
| Slot index (not position) | +0x32D0 | FACT | findings 2026-07-31 (equals slot index) | Do not use as position |
| Engine life countdown | +0x3D70 float | DERIVED/OBSERVED | web_telemetry.c:742 family; findings 2026-07-29 player kill -1.0 | Yes (observed) |
| Wing intact flags | +0xD14 int32, +0xD58 byte | CONFIRMED / FACT | f1c_sdk.h:51-56 | Yes |
| Wheel detach flag (per wheel) | 0x7C7428 + 1124*w + 1104 byte | CONFIRMED (CORRECTION 2026-08-08) | findings 2026-08-08 | Player object only |
| Suspension bent (per wheel) | 0x7C7428 + 1124*w + 1105 byte | FACT | findings 2026-07-29 and 2026-07-31; w = FL0 FR1 RL2 RR3 | Player object only |
| Wheel-hit pointers | veh+0x3674, 4 x stride 0xF8, material at hit+0x54 | DERIVED | findings 2026-09-30 (road 6,21,27; wet 7,22,27; rumble 11,21,27) | No runtime proof |
| Body deformation list | veh+0x40C, stride 0x20 | DERIVED | findings 2026-07-29 | Populated by collisions. Unsure of event semantics |
| Orientation (up vector) | +0x71C, +0x720, +0x724 float | DERIVED | tools/f1c_memory_mcp.py:23-42 (`orientation_state`: overturned if up_y<=-0.25; side/tilt if <0.5) | Yes for overturn. Spin not identified |
| Contact event counter | none found | OPEN | race_data_collector collector.cpp:144 emits contact_observed from an unlisted source; AI-AI collision eids 225/226 via sub_41AD80 LIVE_UNPROVEN | Not a verified player signal |
| Grass / gravel / surface type | none verified | OPEN | Material tags only (above). AIW wall width is not the barrier (findings 2026-10-02) | Cannot tell gravel from road yet |
| Block-trace pit flag | fuel rise + tyre reset | OBSERVED | findings 2026-07-25 (AI pit stops: fuel +32-59 L, wear reset to 1.0) | Yes, cross-check for pit laps |

Caveats:
- A. Simulated times. When the player skips a session (FINISH SESSION / NEXT SESSION), the game writes simulated lap times to +0x4098/+0x409C/+0x40B8 (findings 2026-10-02, two sources). Accept a lap only if lap distance was advancing and speed was >0 during it.
- B. Lap-time latch. The lap counter rises a few frames before last-lap is written (web_telemetry.c ~1340-1350, 1.0.1 fix). Wait for last-lap to change, or hold 1.5 s; otherwise mark the time unknown.
- C. Session clock sentinel. Clock >=1e8 is native finalization, not race time (findings 2026-09-14).

## 2. Existing proven offsets: where they came from
- sdk_sandbox web_telemetry (live-used): last/best sectors (+0x4050/4054/40A0/40A4), wear/fuel columns sampled at lap tick, lap-row latch, sector deltas, yellow-state lookup (web_telemetry.c:322-345, 1296-1350, 1858-1873 teleport/wrap guard). Its own RE comments cite live proof.
- race_data_collector (`src/plugin.cpp:86-88`, `collector.cpp:99-156`): reads lap, position, finish, reason, best, pit, speed, elapsed, carConditions; tyre condition sampling; contact incidents. Its extra decode (`tools/sdk/f1c_race_data_observation.h:27-48`) uses the F1CUI 0x4714 anchor (OBSERVED community, not native-validated) for track_limits 0x4104, hits (0x4714-13408 = 0x12B4), last/best sectors, track_fraction (0x4048). See Conflicts.
- f1c-memory MCP (`tools/f1c_memory_mcp.py:23-32`): laps, line_time, best_lap, lap_dist (0x4044), lap_dist_player (0x4048), pit_status, speed, orientation, retired, finish, position. Its LIMITATION text says sim-time pause is not proven.
- adaptive_ai_difficulty `tools/free_air_logger.py:58-86` (AI free-air laps): reads pit_status, line_time, lap_dist, speed; gap = min((other.lap_dist - c.lap_dist) % L) / speed; yellow = any car below STOPPED_MS. Its design caveat (`docs/baseline-2001-equal-ra-2026-10-14/README.md`): traffic costs ~1 s per lap per second of gap under 1.5 s; reject laps with gap < 2 s or yellow or pit.

## 3. Conflicts and corrections
1. 0x4048 vs 0x4044. race_data_observation.h:36 uses 0x4048 as track_fraction; findings 2026-07-31 says 0x4048 is -1 on AI cars. Use 0x4044 everywhere.
2. Pit status. f1c_sdk.h:75 says FACT output; findings 2026-07-25 says the field stays 0 for AI through full pit stops. The 2026-07-29 player trace supports 3->4. Treat the enum as player-only.
3. Tyre wheel map. web_telemetry says FL=0x363C; corpus tirePuncture names say RR=0x363C; 2026-10-09 says wheel order not proven. Tread-vs-puncture also conflicts (2026-07-28 vs 2026-07-29 CORRECTION). OPEN.
4. F1CUI "hits" = 0x4714-13408 = 0x12B4. That is the damage cluster base from findings 2026-07-29 (DERIVED). Do not use until settled.
5. Fuel "physics mirror". No evidence for fuel at 0x7C7428 + 0x3984 (= 0x7CBDAC). Only veh+0x3984 (absolute 0x5F005C) is supported.
6. Penalty counter 0x4070 is a to-process queue, not a running total of cuts. Use 0x4104 for cuts.
7. Latch at this+24 (= veh+0x4094) conflicts with lapOfRun float. Unresolved.
8. Session type 7 = race is FACT, but docs/38 enum is wrong (findings 2026-07-28, repeated trap).

## 4. Proposed clean-lap rule set (for the mod spec; not implemented)
Compute per lap from samples at the mod's tick rate. A lap is CLEAN only if every rule passes.

Session and clock
1. SessionType (0x7C56A4) is race (7) or qualifying (5) with driven laps (rule A caveat).
2. Sim clock (0x8AFEA0) < 1e8 at every sample. Clock advanced during the lap. No pause flag (0x7C56AC) set during the lap.
3. Line time and lap distance both advanced; no simulated write. Reject if speed was 0 for the whole lap.
4. Lap time comes from a fresh latch (rule B). Else mark unknown and exclude.
5. Sector sum (S1 + S2 + S3) within 0.05 s of lap time. Exclude first lap from grid and the finishing lap.

Track
6. Off-track byte (+0x40E8) = 0 at every sample.
7. Cut counter (+0x4104) unchanged during the lap.
8. No penalty entry (+0x4070 increase) during the lap.
9. Yellow: steward zone state (state 2) at the player's segment is 0 at every sample. Also reject any lap with a yellow state in its range, as a conservative option.

Pit and fuel
10. Pit status (+0x3450) = 0 at every sample. Also exclude the lap before and after a stop.
11. Fuel rise (refuel) or tyre life reset during the lap (cross-check from rule 10).

Traffic
12. Gap to nearest car ahead and behind (from +0x4044 and speed, method of free_air_logger.py) >= 2.0 s at every sample. Use 2.0 s, not 1.0 s, because the baseline shows traffic costs about 1 s per lap per second under 1.5 s.
13. No car in the player's sector below stopped speed (yellow proxy) during the lap.

Damage and car state
14. Car condition (+0x3D6C) = 0 and reason code (+0x4068) = 0 at every sample.
15. Finish state (+0x4064) = 0, not retired (+0x4410 = 0).
16. Wing intact flags (+0xD14, +0xD58) unchanged and nonzero.
17. Per-wheel bent (+1105) and detach (+1104) flags zero, player object 0x7C7428.
18. Orientation state (f1c-memory `orientation_state`) = upright at every sample. Spin is OPEN, so add a yaw-rate or spin check only after validation.

Outlier
19. Lap time between 0.9x and 1.2x the session median of clean laps (or a fixed 50-200 s window, as free_air_logger). Flag, do not drop, outside 1.5x.

## 5. Open items and pending validation
- Live: confirm +0x4104 increments on a player cut and the gate dword_7CEF84 (owner drives a deliberate cut in a race).
- Live: confirm +0x40E8 edge timing vs visible track limit (gravel vs road not separated).
- Live: confirm pit status 0x3450 enum 0-4 for the player across a full stop.
- Live: resolve tread wheel map and tread-vs-puncture conflict (see Conflicts 3).
- Static: identify grass/gravel surface per wheel (material tags at veh+0x3674 are DERIVED only).
- Static: identify the spin / yaw signal; orientation fields are DERIVED from an MCP heuristic.
- Static: meaning of v68 > 2.5 branch in sub_4BCCC0 (sets 0x40E8).
- Live: sim clock sentinel at race end (>=1e8) on this build.
- Pause flag reliability: sim time unchanged is not proof of pause (MCP LIMITATION).
- Data that cannot be derived from this corpus: the steward incident flags beyond yellow (SC/RF state lives in the f1c-memory safety_car block, not verified here).

## 6. Records and documentation gate
- Changed files: this report only. No product status, findings.jsonl, or component file was edited (the task was read-only except this file).
- Pending: the new proven/disproven/open results above should be appended to `f1c_ida_decompiled\findings.jsonl` with tier and source. That needs owner approval because the task was read-only.
- Fact vs inference: FACT/CONFIRMED rows cite a live or static proof. DERIVED/OBSERVED rows are marked. OPEN rows name the test.
- No credentials, api1.txt, or raw sensitive output were copied.
