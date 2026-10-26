# D11_PRACTICE -- E2: practice-session analysis (wet Imola)

Date: 2026-10-02. Author: DeepSeek V4.1 (BATCH C, offline). Task: handoff BATCH C item 2.
Data: `%TEMP%\ai_brain_live\watch_20261002T201617Z` -- PRACTICE, wet Imola, 19 AI cars, watcher
started mid-session, 1 Hz overview, 20 waypoints configured (overview carries no waypoint chain),
registry recorded as v1.2 (header.json). NOTES.md: mode 3 seen (garage exit 5 -> 3 -> 0, finding
`ai-brain-mode3-garage-exit-live-2026-10-02`); stopped by Claude when qualifying began.

Script: `phase3/analysis/e2_practice.py` (imports `phase2/analysis/c1b_lib.py`, no edits).

Scope note: the watcher ran into the following qualifying session. sim_t resets from 1886.5 to 0.0
at overview line 17594; the last 1026 samples are the start of qualifying (cars parked, status 5) and
are EXCLUDED here. Practice segment: 17594 samples, sim_t 946.0..1886.5, 19 drivers, per-car sample dt
median 1.018 s.

## 1. Garage exit / entry sequences (driver_status 0x4510)

- Exit sequence is always 5 -> 3 -> 0. Mode-3 drive duration: 5-6 s for 11 of 12 movers
  (Friesacher 1 s, a 1 Hz aliasing artifact); Monteiro showed 5 -> 0 with no logged mode 3.
- Entry sequence is 0 -> 5 directly; mode 4 was NEVER seen in practice (D8 saw modes 0/4 only in
  race logs; mode 3/5 are the non-race garage states here).
- Mode-3 speed range 2.2-8.6 m/s (garage box to pit exit), mode-5 speed 0 (parked).
- Mode-5 position: `track_part_from_pit_exit` 0x4450 is logged as int bits of a float (v1.2 type
  slip: first read as 1124492837 etc.). Reinterpreted, every car has a constant per-car value
  63.2-180.5 m -- its pit box distance from the pit exit (Villeneuve 119.3, Montoya 109.0,
  Barrichello 63.2, Albers 180.5); lap_distance matches within ~4 m. So mode-5 samples are at the
  team's pit box, not on track.

Selected sequences (full table in the raw log; times = sim_t, durations in s):

| Car | Sequence |
|---|---|
| Villeneuve | 5@946(86) 3@1032(6) 0@1039(783) 5@1821(65) |
| Montoya | 5@946(243) 3@1189(5) 0@1194(692) |
| Barrichello | 5@946(191) 3@1137(5) 0@1142(393) 5@1536(351) |
| Sato | 0@946(588) 5@1534(188) 3@1723(5) 0@1728(159) |
| Button | 5@946(900) 3@1846(6) 0@1853(34) (entered the track as the session ended) |
| Heidfeld / Webber | never left the garage during the practice segment |

## 2. Out-lap vs flying-lap pace and ai_pace_multiplier

Lap time = time between `lap_number` 0x403C increments (1 Hz resolution). For cars that left the
garage, the out-lap is the interval from mode-3 start to the next line crossing:

| Car | exit sim_t | out-lap (s) | flying median (s) | delta |
|---|---|---|---|---|
| Villeneuve | 1032 | 106.7 | 96.6 | +10.1 |
| Montoya | 1189 | 107.7 | 96.6 | +11.1 |
| Barrichello | 1137 | 106.7 | 95.5 | +11.2 |
| Sato | 1723 | 106.7 | 96.5 | +10.2 |
| Trulli | 1420 | 103.7 | 95.5 | +8.2 |
| Klien | 1587 | 105.7 | 96.6 | +9.1 |
| Karthikeyan | 980 | 103.7 | 95.5 | +8.2 |
| Ralf Schumacher | 1538 | 104.7 | 96.6 | +8.1 |
| Friesacher | 1560 | 104.7 | 95.5 | +9.2 |
| Alonso | 1769 | 107.7 | 94.5 | +13.2 |

Flying-lap medians 94.5-97.6 s across the field (wet). Out-laps are 103.7-107.7 s (median 106),
about 8-13 s (8-13 %) slower than the following flying lap. (Cars already on track at watcher start
have partial first laps 32-46 s; excluded. Button/Webber exited but did not reach the line before
the segment ended.)

`ai_pace_multiplier` 0x453C vs the AIPushLap non-race ramp (`phase3/QUALI_STATIC.md` section C/D):
- All 19 cars carry `ability_baseline_scalar` 0x44F4 = 0.9923 and `race_ability_push_scalar`
  0x44D8 = 0.0052 (target 0.9975) -- constant across the whole wet practice field. The wet race
  folder (`watch_20261002T194606Z`) shows the same values; dry Monza (`watch_race4fullc`) shows
  0.9603 / 0.0266. So the pair is the same for all cars within a track/weather state (candidate
  explanation: .rcd RaceAbility not the only input in wet; reported, not settled).
- Ramp model check: 16/16 non-park changes match
  `prev < target ? min(prev + 0.003*min(lap,10), target) : target` exactly:
  e.g. Villeneuve 0.9923 ->(lap 1) 0.9953 ->(lap 2) 0.9975; Montoya and Barrichello identical
  at t=1299/1246 and t=1395/1341.
- Park reset: every 0 -> 5 transition moves 0x453C to exactly 1.0000 (12 events), and leaving the
  garage has AIPushLap pull it back to 0.9975 (16 events). This is the same stopped-car 1.0 value
  D4 flagged as the 2 Monza outliers (SC/RF suspect): here it is a real, repeatable park/garage
  writer, not an SC artifact. The writing function is still unidentified (AIPushLap only writes
  target or current+step, so a third 0x453C writer runs on garage entry; candidate: session/pit
  state path). RECORDER/static follow-up, not E2.
- Verdict: the non-race ramp model is CONFIRMED on 15 independent events; no 4.1 s two-write
  pattern and no 0.01 steps in practice.

## 3. Pit-lane speed clamp (speed_limit 0x4484)

`speed_limit` is reset to 1000000.0 every tick and only occasionally carries a real value. In this
practice segment: 281 non-sentinel samples over 19 cars, dominated by exactly 22.22 m/s (Imola pit
limiter, matches D8). While 22.22 is active, speed stays within 0.63 m/s above it (one-sample
sampling error; no sustained violation); e.g. Ralf 22.24, Fisichella 22.29, Sato 22.35, Kimi 22.85.
Other sub-23 values (12.2, 13.6, 17.5-20.9, 5.0, 27.2, 29.1 ...) are one-off governor inputs, not
the pit limiter, and are not always obeyed (those samples are on flying laps). Verdict: the pit
limiter is respected in practice; the non-22.22 values remain unexplained by this log.

## 4. interaction_target 0x45FC samples

14 nonzero samples in the practice segment, only two cars:

| Car | n | sim_t range | raw value(s) | speeds (m/s) | laps | mode |
|---|---|---|---|---|---|---|
| Narain Karthikeyan | 9 | 1351-1543 | 6527896 (constant) | 24.0-39.6 | 9-11 | 0 |
| Takuma Sato | 5 | 1368-1509 | 6304408 (constant) | 22.2-56.0 | 9-10 | 0 |

All samples are at low speed with every opponent mode = 5 (hold), outside the logged battle
situations. The raw values are constant per car and do not resolve to a vehicle base by the
0x48C0 stride from 0x5EC6D8 (Narain 0x639DD8, Sato 0x603A18); the recorder stores 0x45FC as a
32-bit value, so the pointer interpretation needs a live read. These are the first 0x45FC samples
in a practice session and confirm the field is not race-only; they are consistent with the
Overtaking non-race candidate branch (`phase3/QUALI_STATIC.md` section B) rather than the blue-flag
auto-yield (race-only, session == 7).

## 5. unnamed_4554 (0x4554)

Constant 1.0 in all 17594 practice samples (v1.2 log already types it float; no int-bits
reinterpretation needed here). Matches D1/D11: 1.0 on both tracks, dry and wet.

## Verdict summary

- Garage model: exit 5->3->0 (mode 3 = 5-6 s drive from the box, box at 63-180 m from pit exit);
  entry 0->5; mode 4 never in practice. CONFIRMED live.
- Out-lap ~106 s vs flying ~95-97 s (+10 %). CONFIRMED.
- AIPushLap non-race +0.003*min(lap,10) ramp CONFIRMED (16/16); park resets 0x453C to 1.0 (new,
  explains D4's stopped-car outliers). A third 0x453C writer on garage entry is open (static).
- Pit limiter 22.22 respected. Sub-23 governor values unexplained.
- interaction_target fired 14 times in practice (2 backmarkers, laps 9-11); value decoding open.
- unnamed_4554 = 1.0. No blocker.

Open / follow-ups:
1. Identify the 0x453C park writer (static scan for `mov [veh+0x453C], 1.0`).
2. 0x44F4/0x44D8 equal across cars in wet (0.9923/0.0052) vs dry (0.9603/0.0266) -- check whether
   these fields are weather-scaled by a common factor.
3. Decode interaction_target raw value with a live read (read-only).
