# F1C Web Telemetry — Live Dashboard on :8080

Injected DLL (`web_telemetry.dll`) that embeds a full HTTP/1.1 server in the
game process. Serves a TV-broadcast-style dashboard at `http://localhost:8080`
with live telemetry updated every 100ms via Server-Sent Events.

```
┌────────────────────────┐         ┌─────────────────────────────┐
│  F1 Challenge.exe      │  HTTP   │  Browser                    │
│                        │◄──────►│  http://localhost:8080       │
│  ┌─────────────────┐   │  SSE   │  ┌────────────────────────┐  │
│  │ web_telemetry.dll│──┤────────│──┤ standings / track map  │  │
│  │ tick thread      │   │events │  │ speedo / tach / bars   │  │
│  │   → reads memory │   │       │  │ weather / sectors /    │  │
│  │   → JSON every   │   │       │  │ focus driver detail    │  │
│  │     100ms        │   │       │  └────────────────────────┘  │
│  └─────────────────┘   │         └─────────────────────────────┘
└────────────────────────┘
```

---

## Quick Start

```bat
REM 1. Start the game
F1 Challenge.exe

REM 2. Inject DLL (from mods/f1cui_web_remote/)
..\ai_pit_retire\inject.exe web_telemetry.dll

REM 3. Open browser
start http://localhost:8080
```

---

## Build

Requires **MinGW-w32** (not w64). The game is 32-bit.

```bat
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -o web_telemetry.dll ^
  web_telemetry.c -I..\..\tools\sdk -lws2_32 -lkernel32 -luser32 -O2 -Wall
```

Or run `build.bat` from the `mods/f1cui_web_remote/` directory.

---

## Persistent Race Log (for offline / LLM analysis)

Everything reachable live over HTTP is also written to disk in real time, so a full
race can be analyzed after the session ends — nothing is lost when the game closes.

**File**: `web_telemetry_race_YYYYMMDD_HHMMSS.jsonl`, written next to the DLL. One new
file per race (opens when the race goes active, closes when it stops, on DLL unload, or
when a session boundary is detected — see Changelog #28; the old edge test compared a
flag that reads 3 against 1, so the file never rotated).

**Format**: JSONL (one JSON object per line) — deliberately chosen over a single big
JSON blob or CSV: appendable without rewriting the file, streamable line-by-line
(no need to hold the whole race in memory to read it), trivially greppable/`jq`-able,
and each line is independently valid JSON so a partial/truncated file (e.g. game
crashed mid-race) still parses everything up to the crash point.

**What's logged** — event-driven, not a firehose of every 100ms tick (that would be
~54,000 lines/car/hour of near-duplicate data for no analytical benefit):

| Line type (`"t"`) | When | Contents |
|---|---|---|
| `session_start` | Race begins | Session type, total laps, full car/driver roster |
| `lap` | Every completed lap, per car | Lap number, position, lap time, S1/S2, tire, fuel rate **and** litres in tank, per-corner tire wear (6 dp), oil/water temp, `EngineAngVel`, engine-dead flag, raw wing flags + damage tier, hits, penalties, track limits, reason code — the richest record type, naturally bounded (laps × cars) |
| `incident` | Same moment the live Race Control tab would show it | Same feed: track limits, penalties, damage, retirements, pit in/out, plus session milestones (start/halfway/chequered) |
| `heartbeat` | Every 10s | Session phase, normalized + raw race-active, raw TrackConditionState, grip factor, on/off-line wetness, track/air temp — catches state that doesn't fit a discrete event (e.g. weather changing mid-race) |
| `session_end` | Race ends (or DLL unloads mid-race) | Final classification: every car's finishing position, status, laps, best lap |

Every line is `fflush()`ed immediately on write — small lines, cheap to flush, and
durable against the DLL being unloaded or the game being force-closed mid-race.

This reuses the exact same in-memory incident/lap-history logic already built for the
live tabs (`incident_push`, `lap_history_record`) — no separate tracking code, no risk
of the live view and the log disagreeing with each other.

---

## Endpoints

| Path | Description |
|---|---|
| `/` or `/index.html` | Full HTML/CSS/JS dashboard (embedded in C source) |
| `/events` | SSE stream — JSON telemetry update every 100ms |
| `/track` | One-shot JSON: real circuit waypoint polyline (`{"wc":N,"pts":[[x,z],...]}`). Fetched by client when `wc` (waypoint count) changes. |
| `/laps` | One-shot JSON: per-car completed-lap history for the Race Pace tab (`{"cars":[{"id":N,"p":N,"n":"...","laps":[...]}]}`). Polled every 3s while that tab is open. |
| `/action?type=T&id=N&val=V` | Write-side race control actions. `type=pit\|retire` (needs `id`=vehicle slot), `type=vsc\|redflag` (needs `val`=0/1). Returns `{"ok":true}` or `{"ok":false,"err":"..."}`. **UNTESTED as of authoring** — see Changelog #15. |
| `/incidents` | One-shot JSON: rolling incident log (`{"ev":[{"tk":tick,"id":carId,"n":"name","ty":"POS\|LIMITS\|PENALTY\|DAMAGE\|RETIRE\|PIT_IN\|PIT_OUT","lp":lap,"a":from,"b":to}]}`, last 400 events). Server detects these by diffing each car's snapshot tick-to-tick — the game keeps no event history of its own. Polled every 3s on the Full Debug tab. |
| anything else | 404 |

---

## SSE Protocol

Connection: `GET /events` → keep-alive HTTP response.

Each message is a single JSON line prefixed `data: ` with terminator `\r\n\r\n`.

```json
{
  "ty": 7, "ph": 5, "ac": 1, "acr": 3, "tcf": 0.273, "tgf": 0.500, "tw": 0.000,
  "tl": 53, "vc": 22, "cm": 0, "rm": 3, "pi": 2, "ai": 92, "rt": 7200,
  "tk": 123456789,
  "c": [ /* up to 104 car entries, sorted by position */ ],
  "ll": 3
}
```

### Root-level fields (session globals)

| Key | Source | Type | Meaning |
|-----|--------|------|---------|
| `ty` | `0x007C56A4` | int | Session type: 0=Menu, 1=P1, 2=P2, 3=P3, 4=Q1, 5=Q2, 6=Warmup, 7=Race, 8=Champ |
| `ph` | `0x007C56A8` | int | Phase: 0=?, 1=Grid, 2=Race, 3=Finished, 4=Pod |
| `ac` / `acr` | `0x007D0308` | int | `acr` = raw (reads **3** on track, never 1); `ac` = normalized 0/1. Comparing the raw value against 1 killed every session-boundary edge. |
| `tcf` | `0x007CF3C8` | float | TrackConditionState raw. **It is a float** — the old `tc` int read (1049353456) was used as a weather-enum index. Displayed condition is now derived from `onw`. |
| `tw` | weather `+0x28` | float | **Real** on-line wetness (0.0 in a dry session). Was `0x007CF3A4`, which reads a constant 0.50 and is a grip factor, not a percentage — that value is still exposed as `tgf`. |
| `tl` | `0x008ACF8C` | int | Total laps in race |
| `vc` | `0x005C329C` | int | Number of vehicles in session |
| `cm` / `rm` | `0x007D0410` | int | `rm` = raw **RaceMode**; `cm` = `rm == 4` (championship, per docs/12 `DAT_007d2db0`). A single Grand Prix reads 3, which the old bool test called "champ". |
| `pi` | `0x007C54DC` | int | **Spectated** vehicle slot (the in-game camera target), NOT the human's slot and NOT a position — see Changelog #35. |
| `ps` | derived | int | **Human's** vehicle slot, derived from the AI-control bit (`+0x45F4 == 0` while racing). Use this for "my car". |
| `ai` | `0x007D01A0` | int | AI difficulty, **integer percent** (e.g. 92). Read as a float it was 1.29e-43 and printed as 0.0. |
| `rt` | `0x008ACF94` | int | Race time threshold (seconds) |
| `tk` | — | DWORD | `GetTickCount()` at read time |
| `ll` | — | int | Leader's current lap number |
| `at` | ambient_temp | `0x008AFEF0` | float | Air temperature °C (live: 16.4–18.1). **Proven by use, not by naming**: four physics functions compute `flt_8AFEF0 + 273.16` (C→Kelvin) and `PitRaceStatusMessageDispatcher` tests `> 30.0` for a hot-weather message. Sanity-clamped, `-999` sentinel out of -60..90°C. |
| `trkt` | track_temp | `0x008AFEF4` | float | Track temperature °C (live: 29.6–30.3). Same Kelvin conversion. docs/26 calls this `Flt_PhysicsBrakeBalance` — wrong; a brake-balance ratio is never converted to Kelvin. |
| `onw` | on_wetness | `0x008AFEE8` | float | On-line wetness 0..1. The physics reads it as a grip multiplier `1.0 - (onw + offw) * 0.5`, the AI's push-lap logic gates on 0.01/0.2/0.33, and a pit message fires when the mean exceeds 0.7. Reads **0.0** in a dry session — this, not `0x007CF3A4`, is the real wetness. |
| `offw` | off_wetness | `0x008AFEEC` | float | Off-line wetness 0..1, always averaged with `onw` by the game. Also 0.0 dry. |

### Per-car fields (48 fields, compact JSON keys)

| Key | C field | Offset | Type | Unit / Meaning |
|-----|---------|--------|------|----------------|
| `id` | vehicle slot index | — | int | Stable per-car identity (loop index into `VEHICLE_PTR_ARRAY`). Use this, not `p`, to track a specific driver across position changes. |
| `l` | lap_number | `+0x403C` | int | Current lap number (was read server-side but never wired into JSON until this fix — was permanently 0 client-side before) |
| `p` | position | `+0x4060` | int | Race position |
| `n` | name | `+0x4620` | string | Driver name (max 63 chars) |
| `s` | speed | `+0x4440` | float | Speed in game units (×3.6 → km/h) |
| `r` | rpm | `+0x39A0` | float | Engine RPM |
| `mr` | max_rpm | `+0x39A4` | float | Rev limiter RPM |
| `g` | gear | `+0x39FC` | **int32** | Current gear (0=N). Read as a float it always cast to 0, i.e. permanently `N`. |
| `bl` | best_lap | `+0x409C` | float | Best lap time (seconds) |
| `ll` | last_lap | `+0x404C` | float | Last lap time |
| `tt` | total_time | `+0x4098` | float | **Per-car cumulative race time — this is the correct gap source.** VERIFIED live 2026-07-25 by direct frida read: on the same lap SCH=2961.78, HAK=2964.54, COU=2965.35, AHO=3003.55 — i.e. real race gaps (+2.76 / +3.58 / +41.8). An earlier session recorded this field as "identical across all cars, use `et` instead"; that is **inverted** (see `et` below) and it made the whole GAP column render `-`. Dashboard now uses `tt`, lap-aware (lapped cars show `+NL`). |
| `tp` | track_pos | `+0x06CC` | float | Track position (0.0–1.0) |
| `bs1` | best_s1 | `+0x40A0` | float | Best sector 1 |
| `bs2` | best_s2 | `+0x40A4` | float | Best sector 2 |
| `ls1` | last_s1 | `+0x4050` | float | Last sector 1 |
| `ls2` | last_s2 | `+0x4054` | float | Last sector 2 |
| `fs` | finish_state | `+0x4064` | int | 0=racing, 1=FIN, 2=DNF, 3=DSQ |
| `a` | ai_control | `+0x45F4` | int | **AI-control bit**, not an active flag: 0 for the human driver, 1 for AI, 0 for a retired AI. |
| `ot2` | on_track | derived | int | Presence: `finishState==0 && !retired`. Use this, not `a`, to decide whether to draw or count a car. |
| `pt` | pitting | `+0x3450` | float | Enum: 0=none, 1=requested, 2=in pit lane, 3=stopped in box, 4=exiting. **Resets to 0 on formal retirement even if car is still sitting in the box** — dashboard treats `pt>=2 OR (active==0 && !retired && finish_state==0)` as "in pit" to catch that case. |
| `tc` | tire_choice | `+0x397C` | **int32** | Compound index. Read as a float it always cast to 0 and the compound rendered blank. |
| `th` | throttle | `+0x357C` | float | Throttle 0.0–1.0 |
| `br` | brake | `+0x3584` | float | Brake 0.0–1.0 |
| `rt` | retired | `+0x4410` | int | 1=retired |
| `px` | phys_x | `+0x001C` | float | World X position |
| `pz` | phys_z | `+0x0024` | float | World Z position |
| `mx` | track_x | `+0x0744` | float | Track-coordinate X (CONFIRMED) |
| `mz` | track_z | `+0x074C` | float | Track-coordinate Y (CONFIRMED) |
| `dx` | dir_x | `+0x0710` | float | Heading direction X |
| `dy` | dir_y | `+0x0718` | float | Heading direction Y |
| `pn` | penalties | `+0x4070` | int | Penalty count |
| `cb` | call_to_box | `+0x454C` | int | Pit call flag |
| `pt0` | pit_time_0 | `+0x34A0` | float | Pit stop 0 duration (s) |
| `et` | elapsed_time | `+0x4570` | float | **Reads 0.0 for every car** (verified live 2026-07-25 via frida, mid-race, all 22 cars). Do NOT use for gaps — use `tt`. Offset may be mislabeled or the field is simply unpopulated in this build. |
| `ms` | max_speed | `+0x4538` | float | Top speed in session. Game writes a ~99999 sentinel ("not recorded yet") before a real max is set — client clamps anything >130 m/s (~468 km/h) to `-`. |
| `ds` | driver_status | `+0x4510` | int | Driver status code |
| `gp` | planned_position | `+0x4058` | int | Grid position |
| `rc` | reason_code | `+0x4068` | int | DNF reason: 1=Engine,2=Susp,3=Clutch,4=Brakes,5=Gearbox,6=Accident,7=Clutch,8=Electronics,9=Fuel |
| `qt` | qualy_time | `+0x405C` | float | Qualifying lap time |
| `lr` | lap_of_run | `+0x4094` | int | Lap number of current stint |
| `cc` | car_conditions | `+0x3D6C` | int | Car damage **severity only**, no location: 0=ok, 1=wing/panel damage, 2=wheel loss (terminal). Per-corner/per-part location (front wing vs rear wing vs which wheel) was actively searched for (`mods/ai_wing_retire/README.md`, 7+ live trials) and confirmed absent from the polled struct — likely resolved against the 3D collision scene graph instead. Pair with `rc` (reason_code) at retirement for a coarse cause. |
| `tl` | track_limits | `+0x4104` | int | Track limit violations |
| `fc` | fuel_consumption | `+0x3990` | float | Fuel per lap |
| `hr` | hits_in_race | `+0x12B4` | int | Collision hits |
| `rl` | rpm_limiter | `+0x1068` | int | **Static AI difficulty setting**, not a live "currently limited" event — constantly true for AI cars, false for human. Formerly shown as a flashing `LIM` status tag (misleading, removed); still visible in the expandable detail row. |
| `hd` | human_drs | `+0x00C4` | int | Bit 0=human, bit 1=DRS in zone |
| `dd` | dnf_dsq | `+0x4063` | byte | DNF/DSQ byte flag |
| `tp2` | track_from_pit | `+0x4450` | float | Track distance from pit |
| `p1` | pit_time_1 | `+0x3480` | float | Pit stop 1 duration |
| `p2` | pit_time_2 | `+0x3470` | float | Pit stop 2 duration |
| `lp` | last_pit_time | `+0x3474` | float | Last pit stop duration |
| `ps` | pit_stop_speed | `+0x346C` | float | Pit lane speed (game units) |
| `nf` | next_stint_fuel | `+0x3484` | float | Fuel for next stint |
| `pe0` | penalty_0 | `+0x32FC` | int | Penalty slot 0 |
| `pe1` | penalty_1 | `+0x3300` | int | Penalty slot 1 |
| `pe2` | penalty_2 | `+0x3304` | int | Penalty slot 2 |
| `wfl` | tire_wear_fl | `+0x3924` | float | **Tire wear** FL, remaining tread (1.0 = fresh), ~2e-4 per lap. Was labelled a pressure and printed at 2 decimals, so it read a constant 1.00. |
| `wfr` | tire_wear_fr | `+0x382C` | float | **Tire wear** FR, remaining tread (1.0 = fresh). |
| `wrl` | tire_wear_rl | `+0x3734` | float | **Tire wear** RL, remaining tread (1.0 = fresh). |
| `wrr` | tire_wear_rr | `+0x363C` | float | **Tire wear** RR, remaining tread (1.0 = fresh). |
| `nx` | time_next_exit | `+0x4534` | float | Time until next pit exit |
| `nl` | number_of_laps | `+0x4660` | int | Total laps completed |
| `nc` | next_chosen | `+0x3378` | float | Next chosen (tire/fuel?) |
| `ut` | unlock_tires | `+0x3428` | float | Tire unlock status |
| `cn` | car_name | `+0x4678` | string | Car/team name |
| `dt` | dmg_tier | — | int | **Damage tier**, most severe wins: 0 none, 1 bodywork, 2 front wing, 3 rear wing, 4 wheel, 5 heavy-impact engine stop. Derived server-side from the per-part records below plus `cc`. |
| `fw` | fw_lost | `+0x0D14` | int | Front wing detached (latched). Flag is 1=intact / 0=armed and is **transient** — latched in the 60Hz tick because the 10Hz client would miss it. Front wing = **repairable**, car pits and rejoins. |
| `rw` | rw_lost | `+0x0D58` | int | Rear wing detached (latched). **Terminal** — `PitStopBlockingCondition_0042aee0` tests only this record, so the game retires the car at the pit box. |
| `dm` | doomed | — | int | Predictive: rear wing gone (or engine stopped) and not yet formally retired ⇒ this car **will** retire. Shown as `DOOMED` in the tower. |
| `u4574` | raw_4574 | `+0x4574` | int | **DISPROVEN as a seize flag** (2026-07-25): reads 1 on pristine cars sitting on the grid while others read 0. Static per-car property, meaning unknown. Raw passthrough only — it no longer drives damage or prognosis, which is what falsely marked healthy cars DOOMED. |
| `ed` | eng_dead | — | int | Engine actually stopped: `rpm<=0.5` **and** `EngineAngVel<0.5` and the car hasn't finished normally. Healthy cars have angvel tracking rpm (1803.7 vs 1804). Verified live against a smoking engine. |
| `eav` | eng_angvel | `+0x00A0` | float | `EngineAngVel` (FACT). 0 on a genuinely dead engine — the discriminator that makes `ed` reliable. |
| `ot` | oil_temp | `+0x3D10` | float | **Oil temperature** (DERIVED). engine.ini `LifetimeOilTemp=(123.43,2.90)` halves remaining engine life per 2.9 °C above 123.43, so this is a real reliability lever. **No fixed normal band** — baseline differs per engine manufacturer. |
| `wt` | water_temp | `+0x3D14` | float | Water temperature (DERIVED). |
| `fl` | fuel_level | `+0x3984` | float | **Fuel remaining in litres** (DERIVED). Distinct from `fc`, which is a per-lap rate. |
| `sc` | speed_cap | `+0x453C` | float | AI speed cap (0.0–1.0, 0.0=disabled). Same static-setting caveat as `rl` — not a live governor-triggered event. Formerly shown as flashing `GOV` tag (removed, misleading); still in expandable detail row. |
| `ap` | ai_pace | `+0x457C` | float | AI pace multiplier |

---

## Dashboard Features

- **Standings table** — position, driver, gap to leader, sector times, last/best lap,
  delta, speed, max speed, grid, tire compound, fuel consumption, pit stops, status
  (PIT/BOX/DNF/RET/PEN/DNF/DSQ)
- **Sector highlights** — fastest S1/S2/S3 in purple
- **Leader delta** — `+0.000` vs `LEADER` for P1, computed from `elapsed_time` (per-car), not `total_time` (was shared/broken — see Changelog)
- **Click-to-focus** — click any driver row to lock right panel on that driver, keyed by
  stable `id` (vehicle slot) so it doesn't jump when positions shuffle.
  Expanded rows show: car name, DNF reason, grid, qualy, damage, track limits,
  hits, stint, fuel, max speed, limiter, speed cap, pace mult, DRS zone,
  tire puncture flag, pit times, next stint fuel, 3 penalty slots.
- **Speedometer gauge** — Canvas radial gauge 0–360 km/h, green→yellow→red arc,
  rev-limiter red zone at 280 km/h.
- **Tachometer** — RPM gauge with dynamic max-RPM range, red zone at 85%.
- **Throttle/Brake bars** — percentage + animated fill.
- **Fuel bar** — per-lap fuel consumption rate.
- **Lap/gear/best/last/top-speed/elapsed** — numeric readout cards. Lap now actually populates
  (was permanently 0 — see Changelog).
- **Focus sectors** — S1/S2/S3 with best-sector highlighting.
- **Weather panel** — condition icon + text, wetness %, AI difficulty, champ mode.
- **Track map** — real circuit outline traced from the game's own AI waypoint database
  (`/track` endpoint), with live position dots, heading arrows, driver numbers, 3-letter
  name tags on top. Falls back to a live-position bounding-box scatter (old behavior)
  only if no waypoint data is available.
- **Race Pace tab** — lap-by-lap time grid (driver columns × lap rows, green = personal
  best), pulled from a server-side per-car lap history buffer (`/laps` endpoint).
- **Engineer tab** — spoiler-free, own-car-only view (matches player via `pi`/session
  player index): own telemetry, gap to car directly ahead/behind only (not full grid),
  sector deltas vs own best, tire pressures, fuel consumption + laps-remaining
  projection with a warning if projected fuel need exceeds planned next-stint fuel,
  damage/pit/DRS status, weather. Deliberately excludes other drivers' full data —
  mirrors what a real race engineer's pit-wall feed would show, not the omniscient view.
- **Full Debug tab** — the "know everything" view: reliability/damage table for every
  car (damage severity, DNF reason, hits, track limits, penalties, status), fuel
  strategy projection table for the whole field, a live incident log (overtakes, track
  limit violations, penalties, damage increases, retirements, pit in/out — computed
  server-side by diffing snapshots since the game has no event history), an AI Driver
  Setups &amp; Stats table (grid, pace multiplier, speed cap, rev limiter, driver
  status, DRS — everything per-driver AI config in one place), a raw unfiltered
  session-level field dump, and a raw unfiltered field dump of the focus car for
  catching offset/garbage-value bugs directly in the UI instead of guessing. Per
  user's spec this tab intentionally holds nothing back — "everything that can be
  displayed."
- **Team Principal tab** — both team cars (matched by `cn` car/team name string) side
  by side: position, tire, fuel, stops, damage, status, gap to immediate ahead/behind.
  Plus a rivals table limited to cars within 2 positions of either team car — no
  full-grid telemetry, strategic scope only, per user's explicit spec (both team cars
  + rivals' strategy, not the omniscient view).
- **Race Control tab** — FIA-stewards-bulletin-style feed. The game exposes zero
  race-control message data (confirmed, see Known Limitations), so this reformats
  the incident log into bulletin language: track limits infringements, time
  penalties, damage reports, retirements/DSQ/classified-finisher notices, pit
  in/out, plus new session-wide milestones (lights out, halfway point, chequered
  flag) that weren't tracked before this tab existed. Numbered bulletins, newest
  first, color-coded by severity.
- **Penalty tags** — red `P#` badge for drivers under penalty.
- **DRS zone indicator** — green `DRS` tag, blue `P` for player.
- **Status dot** — green (session wait), red blinking (race active), grey (off).
- **Portrait-screen layout** — table and side panel stack vertically with larger
  fonts/rows on tall-narrow viewports (`orientation:portrait` media query), instead of
  wasting vertical space on a landscape-only layout.

---

## Architecture Notes

### Threading model (2 threads)

1. **HTTP listener thread** — Winsock non-blocking accept loop on port 8080.
   Handles HTTP requests inline (non-blocking, no thread pool). SSE connections
   are stored in a client array and kept alive.
2. **Tick thread** — reads game memory every 16ms (~60Hz), generates JSON every
   100ms (~10Hz), SSE-broadcasts to all connected clients.

### Concurrency

- `g_telemetry` struct protected by `CRITICAL_SECTION g_telemetry_cs`
- `g_sse_clients[]` array protected by `CRITICAL_SECTION g_sse_cs`
- Tick thread writes telemetry, HTTP thread reads it for SSE — the CS ensures
  clients never see a half-written frame (race between JSON serialization and
  memory reads is harmless since tick repeats at 16ms).

### Memory model

All game addresses are hardcoded raw reads — no symbol resolution, no pointer
chasing beyond the vehicle pointer array at `0x007C54F0`. Vehicle slots are
indexed by `VEHICLE_PTR_ARRAY[i]` (array of pointers). Each vehicle struct is
accessed via offset from that pointer.

The Java F1CUILive plugin (also in this directory) uses the same offsets but
reads them through JNA (Java Native Access) from an external process. The DLL
version reads in-process and is faster but requires injection.

### Logging

Log file: `web_telemetry_log.txt` (written next to the DLL). Appends on each
load with `[tickcount] message`.

---

## Vehicle Array (FACT)

| Property | Value |
|---|---|
| Base pointer array | `0x007C54F0` — array of `int*` (pointers to vehicle structs) |
| Stride | `0x48C0` (18,624 bytes) between structs in contiguous memory |
| Max slots | 104 (MAX_CARS) |
| Slot 0 | Player vehicle |
| Slot active check | `int32 at +0x32C8 ≠ -1` |
| F1CUI anchor | `0x5F0DEC` = slot0 + 0x4714 |

Offsets in the per-car field table above use the Java F1CUI subtraction constant
convention (offsets relative to vehicle struct base).

---

## Embedded HTML/JS Dashboard

The entire HTML/CSS/JS is compiled into the `g_html` string constant in
`web_telemetry.c:401-914`. Key JS variables:

- `SL` — session type labels (Menu, P1, P2, P3, Q1, Q2, Wup, Race, Champ)
- `PL` — phase labels (?, Grid, Race, Fin, Pod)
- `WL` — weather labels (Dry, Damp, Wet, Rain, Storm) with Unicode icons
- `RC` — DNF reason codes (1=Engine through 9=Fuel)
- `TIRE` — tire compound labels (S/M/H/W/I)
- `PC` — position-based colors (top-10 distinct + more for field)
- `TRK` — live leader-position trail buffer (max 500 points), fallback map line only
- `TRKPATH` — real static circuit outline fetched from `/track`, primary map line
- `FOCUS` — currently selected driver's stable `id` (not position), toggled by
  `FOCUS=c2.id` in onclick
- `EX` — per-position expand state for detail rows
- `CURTAB` / `swTab()` — Standings vs Race Pace tab state

### Rebuilding the embedded UI

Edit the HTML+CSS+JS between lines 401–914 in `web_telemetry.c`, then rebuild
with `build.bat`. There is no separate frontend build step — the entire UI is
inlined as a C string.

---

## Comparison: Java F1CUILive (original)

This directory also contains the decompiled Java F1CUILive plugin
(`F1CUILive.jar` exploded) — a desktop Swing overlay that reads game memory via
JNA. Not a web server. Kept here for offset reference only.

| Feature | `web_telemetry.dll` | Java F1CUILive |
|---|---|---|
| Runtime | Injected DLL (in-process) | External JNA (out-of-process) |
| Display | Browser at :8080 | Swing overlay on game |
| Update rate | 100ms SSE | Variable |
| Data path | Direct raw address | JNA memory read |
| Modifiable | Rebuild C source | Edit decompiled Java |
| Fields | 48 per car + 10 session | ~97 documented offset mappings |

---

## Offset Source & Trust

All game addresses and struct offsets are hardcoded in `web_telemetry.c:25-116`.
They derive from the Java F1CUILive plugin's bytecode-decompiled offset table
(documented in `docs/26-f1cui-community-offsets.md`). The vehicle pointer array
and stride match the corpus's FACT-level vehicle array definition.

**Tire pressure offsets** (`OFF_TIRE_P_FL` etc.) are DERIVED — the original
F1CUILive enumerates them sequentially but their numbering relative to
corpus naming conventions may need verification.

All other offsets match the corpus skeleton facts where cross-referenced.

---

## Known Limitations (confirmed absent, not just unimplemented)

These were searched for across the whole `mods/` decompilation corpus and confirmed
not to exist in any readable/documented form — not a "todo," a dead end unless someone
does fresh IDA/Ghidra RE work:

- ~~**Yellow flag / per-sector caution state**~~ — **SOLVED 2026-07-25, this entry
  was wrong.** The state is a 29-entry flag-zone table on the **Steward** object
  (`0x008ACEE0`, which docs/26 mislabels `ForceFeedbackBase`): `+0x108` scale,
  `+0x10C` count, `+0x110` array of `{int32 state; float end_distance}`, `+0x114` the
  game's own previous-state copy. `state == 2` is yellow, live-verified with a yellow
  actually flying. `ShortComputation_bad0` is not a black box — its disassembly is
  `mov ecx, 0x8ACEE0` twice, testing the car's segment and the one behind it. Written
  by `sub_4D1730` every frame: it clears the table, then marks the flag zone of any
  non-DNF car whose forward velocity is under a threshold, i.e. yellow means "slow car
  here" — and nothing written into the table survives a frame, so it is a read-only
  observable. Now published as `sg`/`sgn`/`sgl`/`yc` plus per-car `sgi`/`yf`, with
  Race Control bulletins on the edges. Full writeup:
  `investigations/yellow_flag/README.md`.
- **Per-part damage location — PARTIALLY SOLVED since this note was written.** Front and
  rear wing records *are* readable (`0xD14` / `0xD58`, CONFIRMED) and are now used for
  the damage tiers and the retirement prediction. What remains unmapped is **which
  wheel / which panel**: the exact `partIdx` (0–3 wheel, 4 front wing, 5 rear wing,
  6–13 = `Part0`–`Part7` from `damage.ini`) is only visible by hooking
  `RepairServiceStateDispatcher_004ed250`, which the DLL does not do. A struct diff
  surfaced six more likely part flags at `0x874 / 0x8B8 / 0x99C / 0x9E0 / 0xF34 / 0xF78`
  (paired at stride 0x44, matching the wing-record shape) but the offset→part mapping is
  not established. So: damage *class* is reliable, damage *location below wing level* is
  not.
- **Engine health** — an earlier version of this section claimed no engine state existed.
  That was wrong: see Changelog #23. Oil temp, water temp and fuel level are now read,
  and `engine.ini` documents a full lifetime/wear model. The **lifetime counter itself**
  is still not located — that one remains genuinely open.

---

## Changelog

### Session fixes (2026-07-25 pass 3 — who is who)

34. **The player's car was missing from the track map, and `0x45F4` is not
    "ActiveFlag".** Live, with the human in P1 at 79 km/h, `finishState 0`,
    `retired 0`, that field reads **0** — while every AI car reads **1**, and a
    retired AI drops to 0. It is an **AI-control bit**. The map filtered dots on
    it (`if(c2.p<=0||!c2.a)continue`), so the player's own dot was skipped every
    frame. Presence is now a server-side `ot2` flag (`finishState==0 && !retired`).
    The same misreading made the pit heuristic `!active && !retired && fs==0`
    match *only* the human — flagged as pitting for an entire race, while it could
    never fire for the AI case it was written for (those cars are already
    retired). That fallback is deleted; `pittingStatus >= 2` stands on its own.
    `name_index.json` entry for `0x45F4` corrected from FACT/"ActiveFlag".
35. **`CurrentPlayerIndex` (`0x7C54DC`) is the SPECTATED vehicle slot** — not the
    human's slot, and not a position. It read 0, then 2, then 6 in a single race
    where the human never left slot 0; while spectating Barrichello it read 4, and
    slot 4 is Barrichello. The client was comparing it against each car's
    **position**, which happens to agree whenever a car's slot equals its position
    — exactly why spectating Barrichello (slot 4, P4) looked right while
    R.Schumacher (slot 6, **P5**) highlighted P6/TesterA (slot 5). Now:
    - `ps` — the **human's** slot, derived from the AI-control bit, sticky across a
      retirement, logged once when resolved. Drives the `plr` row highlight, the
      Engineer tab's "my car", the Team Principal pairing and every race-control
      "never touch the player" guard (which were all keyed off the wrong value).
    - `pi` — the **spectated** slot, kept raw. Gets its own green `vw` row outline,
      and the focus panel now follows the in-game camera until you click a row
      (clicking locks it, as before).

### Session fixes (2026-07-25 pass 2 — Pit Wall correctness)

Everything below was found by reading the SAME address both ways (int and float)
in the live process with Frida, mid-race, rather than trusting the type the code
assumed. Build: `web_telemetry_v4.dll`.

25. **A whole class of fields were read with the wrong TYPE.** A 32-bit int like
    `1` reinterpreted as a float is `1.4e-45`, and `(int)1.4e-45` is `0` — so an
    int-typed field read through the float macro silently renders as zero, never
    as an obvious error. Three fields were doing exactly this:
    | Field | Was | Is | Symptom |
    |---|---|---|---|
    | `CurrentGear` `+0x39FC` | float | **int32** | gear permanently `N` |
    | `tireChoice` `+0x397C` | float | **int32** | tire compound blank |
    | `AIDifficultySlider` `0x7D01A0` | float | **int32 percent** (92) | AI shown as `0` |
    `name_index.json` already typed `CurrentGear` as int32 — the DLL just didn't
    follow it. All three are now annotated with live evidence in the index.
26. **`0x4574` is not an engine-seize flag — DISPROVEN, and it was the cause of
    the false `DOOMED` / `WILL RETIRE` on the whole field.** At the start of a
    race, every car pristine, Coulthard and TesterA already read `1` while
    Schumacher, Hakkinen and Barrichello read `0`. A seize flag cannot be set on
    an undamaged stationary car on the grid. The DLL latched it and OR'd it into
    the retirement prognosis, so those cars were permanently doomed from lap 0.
    Now a raw passthrough (`u4574`) with no behavior attached. Prognosis is
    rear-wing-only, which is the one CONFIRMED terminal signal.
27. **Damage was also stale across races.** The wing latches, incident ring,
    milestone flags and lap history were initialised once at DLL load. Combined
    with #28 they were never cleared, so a new race inherited the previous race's
    retirements and damage — matching the "Schumacher missing a rear wing while
    pristine" report. **Note for this install specifically:** `SeasonData/damage.ini`
    has `FrontWingDetach=100000000.0` and `RearWingDetach=100000000.0`
    ("disabled for wheel-only testing"), so wings *cannot* detach at all here.
    Any wing damage on the dashboard was by definition stale. Wheels still detach
    (`WheelDetach=3500.0`) and `Engine=10000.0` still seizes.
28. **`RaceActiveFlag` reads 3, not 1 — every session-boundary edge was dead.**
    docs/38 says "nonzero = on track"; the code compared it to `1`. So START never
    fired, the JSONL race log never rotated, and nothing was ever reset. Now
    normalized to 0/1 (raw kept as `acr`), plus a real session-boundary detector:
    session type change, lap-count change, car-count change, race-active edge, or
    leader lap going backwards (catches a restart of the same session) all clear
    the history and close the old log file.
29. **SESSION panel read three different fields wrongly.**
    - `RacePhase` was rendered through a 5-entry label table but is documented
      0..8 — hence `RACE / ?`. Unmapped codes now print `ph N` rather than
      inventing names for phases nobody has decoded.
    - `TrackConditionState` `0x7CF3C8` is a **float** (0.273), was read as an int
      (1049353456) and used as a weather-table index. Condition is now derived
      from the weather struct's on-line wetness instead.
    - Wetness 50% came from `TrackWetnessFactor` `0x7CF3A4`, which read a flat
      0.50 in a bone-dry session — docs/12 calls it a grip factor consumed by
      `FUN_004c8920`, not a percentage. Real wetness (`+0x28`/`+0x2C` of the
      weather struct) read **0.0**, matching the screen.
    - `MODE: CHAMP` came from treating `0x7D0410` as a bool. It is **RaceMode**;
      docs/12 (`DAT_007d2db0`) says championship is the value **4**. A single
      Grand Prix reads **3**, which the old code saw as "truthy" = champ.
30. **Weather values regrounded on the game's own code (revised).** These were
    first read as base+offset from an assumed weather struct at `0x8AFEC0`,
    justified by a name in docs/26 — but that name column is copied from docs/12,
    which is Ghidra+LLM naming inherited from the deprecated `F1C_ghidra_project`,
    exactly the tier this corpus says not to trust (two of those names are provably
    wrong: `0x8ACEE0` is the Steward, and `0x8AFEF4` is a temperature, not
    "Flt_PhysicsBrakeBalance"). Now read as absolute addresses, each proven by how
    the decompiled code uses it: `0x8AFEF0`/`0x8AFEF4` get `+ 273.16` applied in
    four physics functions (°C→Kelvin) and one hot-weather message tests `> 30.0`;
    `0x8AFEE8`/`0x8AFEEC` are consumed as `1.0 - (a + b) * 0.5`, a grip multiplier.
    Air temp was being read from `+0x04` (a 0..1 factor, displayed as `0.6 degC`
    and then hidden); it is `+0x30` = 18.1 °C, with track temp `+0x34` = 30.3 °C.
    Air temp is now shown again.
31. **Tire wear now displays — the fields were mislabelled as pressures.**
    `0x3924/0x382C/0x3734/0x363C` are remaining tread (1.0 = fresh);
    `name_index.json`'s own note calls them a wear-rate and says F1CUI fakes
    punctures by driving them to minimum. Printed at 2 decimals as a "pressure"
    they read a constant `1.00`. Live lap 1: 0.99998 front / 0.99999 rear on a
    running car, fronts wearing faster. Now sent as `wfl/wfr/wrl/wrr` at 5
    decimals and shown as wear-used per mille. AI wear is not simulated the same
    way the player's is, so AI values are indicative only.
32. **Per-lap logging now records exact values** (was: lap time, sector, tire
    index, fuel rate, damage severity). Adds per-corner tire wear at 6 decimals,
    fuel in tank, oil/water temp and `EngineAngVel` (for the `engine.ini`
    `LifetimeOilTemp=(123.43,2.90)` wear model), raw wing flags in addition to the
    UI tier, hits, penalties, track limits and reason code. Heartbeat now logs
    raw condition/grip/on/off wetness instead of the two misread fields.
33. **`ENG ANGVEL` is now explained in the UI.** It is the crankshaft's angular
    velocity (`veh+0xA0`, FACT). On a healthy engine it tracks RPM almost exactly
    (1803.7 vs 1804), so while running it is redundant — its value is as a
    cross-check: RPM alone reads 0 for a car that is merely stopped, whereas RPM 0
    **and** angvel 0 means the crank isn't turning, i.e. the engine is genuinely
    dead. It is now displayed next to RPM so a divergence is visible. Engine-death
    detection is additionally gated on the session being live, since out of a
    session every car reads rpm 0 / angvel 0 and the whole grid showed
    `ENGINE STOPPED`.

### Session fixes (2026-07-25 pass)

17. **GAP column was dead for the entire field — root cause was an inverted fact in this
    README.** Changelog #7 (earlier session) claimed `total_time` (`+0x4098`) "reads
    identical across all cars" and switched gap calculation to `elapsed_time` (`+0x4570`).
    Direct live memory reads via the frida MCP mid-race disproved this: `elapsed_time` is
    **0.0 for every car**, while `total_time` is genuinely per-car cumulative race time
    (SCH 2961.78 / HAK 2964.54 / COU 2965.35 / AHO 3003.55 on the same lap = the real
    +2.76 / +3.58 / +41.8 gaps). Every gap readout in the UI — standings, timing tower,
    Engineer ahead/behind, Team Principal — was therefore rendering `-`. Now uses `tt`,
    lap-aware: a lapped car shows `+NL` rather than a meaningless seconds delta. Added a
    separate **Int** (interval to car ahead) column alongside Gap, as on a real timing screen.
    *Lesson for future sessions: this is exactly the "never launder a lower tier into a
    higher one" failure the corpus CLAUDE.md warns about — a DERIVED prose claim got
    treated as settled fact and propagated into code. Verify against live memory.*
21. **Damage tracking rebuilt — five tiers plus retirement prediction.** Damage was a
    single 0/1/2 severity blob. Now resolves BODYWORK / FRONT WING / REAR WING / WHEEL /
    ENGINE STOP from the CONFIRMED per-part records (`0xD14` front, `0xD58` rear), and —
    the useful part — **predicts retirements**: `PitStopBlockingCondition_0042aee0` tests
    only the rear-wing record, so a detached rear wing means the game *will* retire that
    car at the pit box. Those cars are flagged `DOOMED` before it happens.
    **Latching is server-side in the 60Hz tick**: the flags reset once a retirement
    completes, so the observable window is only detach→pit box and a 10Hz client would
    miss it entirely. Feeds Race Control bulletins and the Full Debug table.
22. **Engine death detection.** `ReasonCode` was observed changing **6 (Accident) → 1
    (Engine)** live on a car with a visibly smoking engine, while it was still running
    (`finishState 0`, `retired 0`). Detection is `rpm<=0.5 && EngineAngVel<0.5` — a
    healthy car has angvel tracking rpm exactly (1803.7 vs 1804). Two of my own bugs
    fixed here: (a) I had suppressed the reason text until a car was classified out,
    which hid exactly this failure; (b) the first version gated engine-death on
    `activeFlag`, but a car whose engine dies gets *deactivated*, so it excluded the case
    it was meant to catch.
23. **Oil temp / water temp / fuel level added** (`0x3D10`, `0x3D14`, `0x3984`, all
    DERIVED). Prompted by `engine.ini`, which documents a real engine **wear model**
    (`LifetimeAvg=8924` seconds ± `LifetimeVar`, halved per `355.3` RPM or `2.90` °C over
    base) — correcting an earlier claim in this session that engines don't wear out.
    Located by time-series drift: dead engines hold these *exactly* frozen while running
    ones drift. The ~84 vs ~100 oil-temp split is **different engine manufacturers**, not
    damage, so there is no fixed "normal" band — the UI colours against the 123.43 °C
    life-halving threshold instead. See `investigations/engine_lifetime/`.
24. **ReasonCode enum corrected from the game's own code.** `FailureCategoryNameLookup_004bb730`
    is a plain switch returning the display strings, so its order *is* the enum:
    1 Engine, 2 Gearbox, 3|7 Clutch, 4 Suspension, 5 Brakes, 6 Accident, 8 Electronics,
    9 Fuel. This disproved the `name_index.json` FACT entry (2/4/5 permuted, 6 as
    "puncture"), which had been propagated into the dashboard. `name_index.json` updated
    with the superseded values preserved. Also: these codes only label **AI mechanical
    failures**; only the wheel path (`partIdx 0-3`) writes one, which is why rear-wing
    retirements leave it at 0.
18. **Pit Wall tab** — fixed multi-pane grid (session / car telemetry / race control /
    timing tower / race pace / track map all visible simultaneously), modeled on a real
    pit-wall monitor layout rather than tab-switching. Now the default view.
19. **Standings clicks were unreliable** — the table was rebuilt via full `innerHTML`
    replacement 10x/sec, so a click whose mousedown/mouseup straddled a rebuild was
    dropped by the browser (its target node no longer existed). Replaced with keyed DOM
    diffing: `<tr>` nodes persist across ticks keyed by vehicle slot id, only changed
    cells are rewritten, reordering uses `insertBefore`. Clicks now land first time.
20. **Layout: dead space / smeared columns** — tab panes stretched tables to the full
    width of a 2000px monitor, spreading columns hundreds of px apart. Added a shared
    `.dtab` table class (content-width columns, capped at 1500px) and pane max-widths.

### Session fixes (earlier pass)

1. **Track map now shows the real circuit** — added `ADDR_AI_DB_BASE` (`0x005EA2C0`),
   reads the game's own AI waypoint array (count `+0x130`, array ptr `+0x134`, stride
   316 bytes, X/Z at `+0x00`/`+0x08`) via new `/track` endpoint. Previously the "map"
   was just a live bounding-box scatter of car positions with no real track shape.
2. **Focus panel no longer jumps drivers** — was keyed by race position (`FOCUS=p`);
   an overtake/pit/DNF reshuffle would silently swap which driver's data displayed.
   Now keyed by stable vehicle slot `id`.
3. **Lap counter wired up** — `lap_number` was read server-side but never serialized
   into JSON; client's `fc.l` was always `undefined`→0. Added `l` field.
4. **Map coordinate-space mixing fixed** — per-car `mx||px` fallback treated legitimate
   `0` as missing, silently mixing track-space and world-space coordinates within the
   same frame. Now one global track-vs-world decision for the whole car list.
5. **MAX speed sentinel garbage** — game leaves `max_speed` at a ~99999 m/s sentinel
   until a real value is recorded; displayed raw before. Clamped to `-` above 130 m/s.
6. **LIM/GOV tags removed from standings** — `rpm_limiter`/`speed_cap` are static
   per-driver AI difficulty settings, not live "currently limited/governed" events.
   They were permanently on for every AI car, which looked like a bug but was actually
   mislabeled correct data. Data still visible in the expandable row detail.
7. **Gap/LEADER column fixed** — was computed from `total_time`, which reads identical
   across all cars (shared session clock, not per-car). Switched to `elapsed_time`.
8. **Pit detection fixed** — `pitting` field resets to 0 the instant a car is formally
   retired/parked, even while it's still sitting in the pit box. Now also treats
   `active==0 && !retired && finish_state==0` as "in pit." Added a `BOX` tag (from
   `call_to_box`) for cars ordered in but not yet arrived.
9. **Portrait layout** — old `max-width:800px` breakpoint missed tall-narrow screens.
   Added `orientation:portrait` trigger that stacks panels and enlarges rows/fonts/gauges.
10. **Race Pace tab** — new server-side per-car lap history buffer + `/laps` endpoint +
    client tab showing a colored lap-time grid.
11. **Engineer tab** (spoiler-free) + **Full Debug tab** (everything) added, per user
    request to have both a realistic pit-wall view and a complete-knowledge dissection
    view for mid-race debugging. Debug tab includes a server-side incident log
    (`/incidents`) built by diffing car snapshots tick-to-tick — position changes,
    track-limit/penalty increments, damage increases, retirements, pit in/out. No new
    memory offsets were needed; everything is derived from already-confirmed fields.
    One hard limitation carried over: pit-stop history is capped at 3 stops per car —
    the game struct itself only has 3 slots (`pt0`/`p1`/`p2`), not a mod restriction.
12. **Air/track temperature added**, prompted by comparing against a real F1 pit-wall
    monitor reference (weather box showing temps + wind + precipitation %). Found a
    separate weather struct singleton at `0x008AFEC0` (`WS_` fields in
    `mods/sdk/f1c_sdk.h:10850-10863`) with ambient temp, track temp, and on/off-path
    wetness — none previously read. **Unconfirmed address** (no cross-reference
    anywhere in the corpus, unlike the CONFIRMED waypoint/vehicle-array addresses) —
    sanity-clamped, verify against real in-game values before trusting it. Wind speed,
    wind direction, and precipitation probability were searched for and confirmed
    absent from the entire decompilation — the sim doesn't appear to model them at all.
13. **Race Control tab** — since no real FIA-style message feed exists in-game, built
    one from the incident log (reformatted into stewards-bulletin language) plus new
    server-side session milestones (`START`/`HALF`/`CHEQUERED`, `car_id=-1` events) that
    detect race start, halfway point, and leader chequered flag by watching
    `race_active` transitions and leader lap/finish state — none of this was tracked
    before. `/incidents` payload extended to support session-wide (non-per-car) events.
14. **Persistent JSONL race log** — everything live-viewable was previously memory-only
    (ring buffers, overwritten on wrap or lost on DLL unload). Now every lap, incident,
    and a 10s heartbeat write straight to `web_telemetry_race_<timestamp>.jsonl` next
    to the DLL, flushed per line. One file per race, full session start/end markers
    with final classification. Built for offline/LLM analysis of a complete race after
    the fact, not just the live dashboard.
15. **Race Control write-side actions** — Force Pit, Force Retire, VSC, Red Flag as
    buttons in the Race Control tab, calling a new `GET /action?type=...&id=...&val=...`
    endpoint. Mechanism ported verbatim from `mods/red_flag` and `mods/ai_pit_retire`
    (the "armed trigger" pattern: write `+0xD58`/`+0x454C` and let the game's own
    pit/retire logic execute it, rather than forging `FinishState`/`ActiveFlag`
    directly — forging is documented to freeze cars mid-track or park them in the
    wrong spot). VSC/Red Flag apply a `speed_cap` multiplier to all AI cars every tick
    while active (0.55 / 0.30), Red Flag additionally locks race position to its value
    at activation. Player car always excluded. **UNTESTED in this DLL** — the source
    mods have confirmed in-game logs for this exact technique, but it was never fired
    from `web_telemetry.c` before this change. Verify live before trusting it in a
    real session.
16. **Team Principal tab** (both team cars + rivals' strategy, no full grid) and
    **Full Debug tab expanded** (AI setups/stats table + raw session-level dump) —
    resolves the four-view restructure discussed in conversation. User clarified: TP
    = "both team cars + rivals, strategic only"; the fourth/Full-Debug tab = "shows
    everything that can be displayed... no spoiler restraint" (not a delayed/hidden
    view as initially assumed — clarified via AskUserQuestion before building).

---

## Open Items (as of this handoff)

1. **Live-verify Race Control actions** (Changelog #15) — Force Pit/Retire/VSC/Red
   Flag are ported from mods with confirmed working precedent, but never fired from
   this DLL. Inject, start a race, try each button, confirm in-game effect.
2. **Live-verify weather struct** (Changelog #12) — `ADDR_WEATHER_BASE` (`0x008AFEC0`)
   has no cross-reference anywhere in the corpus, unlike CONFIRMED addresses. Check
   displayed track/air temp against sane values in-game.
3. **Time-penalty write test** — user asked whether writing to `penalty_0/1/2` or
   `penalties` actually gets enforced by the game's own timing (added to total time)
   or just gets stomped next tick. Not yet tested. This determines whether a real
   "Time Penalty" race-control action is buildable at all (see conversation: Tier 2
   in the FIA-action-tiers breakdown).
4. ~~Four-view restructure~~ — **done** (Changelog #16). All six tabs now exist:
   Standings, Race Pace, Engineer, Team Principal, Full Debug, Race Control.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `bind port 8080 failed` | Another process on :8080. Check `netstat -ano \| findstr :8080` |
| Blank page / "Connecting..." | Game not running or DLL not injected |
| No data / stale data | Game in menu, not on track. Start a session |
| SSE disconnects | Game closed → DLL detaches. Reload |
| Build fails | Wrong MinGW (needs 32-bit at `C:\msys64\mingw32\bin\gcc.exe`) |
| `inject.exe` not found | Copy from `mods/ai_pit_retire/inject.exe` or use any generic DLL injector |

---

## Modification Guide

### Adding a new telemetry field

1. Add the offset `#define` in the address section
2. Add the field to `car_telemetry_t` struct
3. Read it in `read_telemetry()` using `RF()` / `RI()` / `RB()` macros
4. Add it to the JSON format string in `json_telemetry()` (compact key)
5. Update the dashboard JS to consume the new key
6. Rebuild

### Changing port

Edit `#define HTTP_PORT 8080` at line 121.

### Changing update rate

Edit `#define SSE_UPDATE_MS 100` (SSE broadcast interval) and/or
`#define TICK_SLEEP_MS 16` (memory read interval).

### Adding endpoints

Add path matches to the `http_handle()` function at line 959.

---

## Files

| File | Purpose |
|---|---|
| `web_telemetry.c` | Full source — server, telemetry reader, JSON builder, embedded HTML |
| `web_telemetry_v4.dll` | **Current build** — all the type/staleness fixes in Changelog #25-33 |
| `web_telemetry.dll` | Older v3 build. Windows locks an injected DLL, so a rebuild cannot overwrite it while the game is running; that is why the current build has its own filename. |
| `web_telemetry_v3.dll` | Alt copy of v3 |
| `web_telemetry_log.txt` | Runtime log |
| `build.bat` | Build script (MinGW32) |
| `inject.exe` | (copied from `ai_pit_retire/`) — generic DLL injector |
| `F1CUILive.jar` | Original Java F1CUILive (unmodified, for offset ref) |
| `F1CUILivePlugin.ini` | Java plugin config |
| `com/`, `eu/`, `org/`, `f1p_*/` | Exploded Java classes from F1CUILive.jar |
| `resources/` | Java plugin resources |
