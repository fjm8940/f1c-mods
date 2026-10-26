# Vanilla hidden failure paths: RaceEvents season script + start stalls (2026-10-01)

Owner question: are there other hidden factors/timers that decide AI engine failures?
Investigator: Claude (static RE via the SDK front door + read-only data grep). No code changed.

## Facts (static code, CONFIRMED by reading the decompiled functions)
1. `EngineWearFailureSimulation` 0x4EE150 seeds the four AI life slots veh+0x3D70/74/78/7C ONLY when
   Failure Rate (EngineWearMode dword_7CEA5C) is 1 or 2. In modes 0 AND 3 all four slots = FLT_MAX (infinite).
2. Failure Rate meanings (PLR comment, game install): 0 none, 1 normal, 2 timescaled, 3 season.
   Owner PLR today = 2 (timescaled) in CHAMP/CURNT/GPRIX/MULTI/PRACT.
3. Mode 3 "season": the .rcd/driver parser sub_424080 (race session, dword_7C56A4 == 7) reads the track
   .gdb `RaceEvents { <driver name> { <lap> = <type> } }` table and fills 5 schedule pairs at
   veh+0x4144/0x4148 (lap, type). Lap = key / flt_8ACF9C (RaceLengthWearDivisor).
   Type map: Engine 0, Brakes 1, Gearbox 2, Suspension 3, Stall 4, Stall DNS / DNS 5, anything else
   (Electronics, Accident, ...) = ignored (lap -1).
   So the slot map is 0x3D70 engine, 0x3D74 BRAKES, 0x3D78 gearbox, 0x3D7C suspension.
4. `AIMainUpdate` 0x424E70 lines ~185-208: when the AI decision timer veh+0x413C expires and the car's
   lap veh+0x403C equals a scheduled lap with type 0-3, it writes
   life[type] = rand() % max(bestLap veh+0x409C, 40 s). => the component dies within about one lap.
5. Modes 1-2 (normal/timescaled): same function rolls the .rcd `StartStalls` (veh+0x44BC) at race start:
   chance StartStalls %; 90 % type 5 (stall, DNS-style, reason 7/3/8 random), 10 % type 4 (stall, restart
   after 10-30 s). Not an engine-life path.
6. The SDK offset index lists only 0x4EE150 as writer of 0x3D70 - it MISSES the indexed write
   `this + 4*idx + 15728` in AIMainUpdate. Corpus regex search found it.

## Data (read-only grep of the game install)
- `RaceEvents` exists in hundreds of SeasonData track .gdb files (1996-2005 + Custom/2002); rough type
  counts (includes backups, INFERENCE-level count): engine ~800, gearbox ~580, brakes ~450, suspension ~260.
- Example 2005 Albert Park: Sato lap 55 Engine, Button lap 56 Suspension, Schumacher lap 42 Brakes.
  Driver keys carry suffixes like `Jenson Button@@@#`; whether they match mod driver names is UNTESTED.

## Impact on mechanical_failures (INFERENCE - needs a live test in mode 3)
- Mode 2 (owner default): RaceEvents inactive; no effect. StartStalls is separate (no engine life).
- Mode 3: AI life is infinite until a scripted write makes it finite mid-race (short, < 1 lap). The mod's
  Q1 seeds ai_life from that first finite sample and Q2 timer ownership then RAISES the game timer
  (ai_life + 1200). Expected result: the scripted historical retirement is DELAYED by the fault lead
  (minimum MF_LEAD_MIN_S) or lost if the race ends first. Not verified.
- Sweeps/calibration (Q4-Q6) model modes 1-2 only; mode 3 is not covered.
- Fix options (owner decision): (a) in mode 3 skip Q2 ownership and let the scripted write kill on time
  (vanilla bang) with an instant fault label; (b) detect a sudden drop of a finite slot (> 50 % in one pump
  pass) as a scripted event in any mode and honour it with a short-lead onset.
- Brakes slot 0x3D74 is a vanilla AI brake failure timer (ReasonCode 5 per prior finding) - relevant to
  the future brakes system (plan section 7).

## Still not checked (open)
- Every OTHER native path that kills an AI engine without the life slots (collision damage -1 sentinel is
  known; overheat for AI unknown). A full xref of ReasonCode writers (veh+0x4068) would close this.
- Live proof of the mode 3 path and driver-name matching.

Sources: f1c_ida_decompiled functions/sub_424080_00424080.c, AIMainUpdate_00424e70.c,
EngineWearFailureSimulation_004ee150.c; PLR Failure Rate comment; SeasonData .gdb grep.

## Pass 2 (Claude, 2026-10-01, static + LIVE read-only)
Live target: game pid 10064, exe sha256 1A70E5ED..., 2002 mod race, session 7, Failure Rate (dword_7CEA5C) 2,
RaceLengthWearDivisor 1.0, sim t ~1630-1660 s, 24 cars, mechanical_failures 911b619d loaded (log active).
Reads: elevated f1c-memory reader (health/drivers) + %TEMP%\mf_dyn\rd.py/rate.py/wing.py (ReadProcessMemory,
disposable).

### FACT (live)
- All 4 AI life slots (engine/brakes/gearbox/suspension) drain exactly 1.000 s/s over a 10 s window for every
  AI car, independent of rpm (0.6-0.97 of limit) and also for stopped/retired cars (rpm 0).
- Schedule pairs veh+0x4144 all (-1,0) in mode 2 (RaceEvents inactive), as the static read predicts.
- Player slot 0: engine + brakes slots do not drain; gearbox + suspension slots drain 1.0 s/s. INFERENCE: that
  is the mod's own gearbox/suspension "vanilla timer" family tick (README 2026-09-28), not vanilla.
- Two AI cars retired (Bobbi lap 6, Panis lap 1: retired 1, finish 2) with ReasonCode 0x4068 = 0, life slots
  all positive, carConditions 0, wings intact at read time. Cause NOT proven. Known vanilla path that retires
  with ReasonCode 0: pit retirement (rear wing loss, finding reasoncode-not-written). The mod log has no
  engine event for slots 8/20. => engine-failure statistics that count only ReasonCode 1 miss these.

### CONFIRMED (static)
- 0x4BEF60 (SDK name "AICollision", misnamed) is a retire/repair decision used by EngineFailureCheck 0x4BEC55
  (the per-lap SIMULATED lap path, ProgressBarEventEncode): returns 1 (repair/pit) when a part record at
  veh+2100 (stride 296) is missing, 2 (retire) when veh+0x3D6C == 2; mode 3: retire if a RaceEvents entry
  exists for the lap; else rolls .rcd Crash % (veh+0x44D0) and (1 - CompletedLaps% veh+0x44DC / 100) per lap
  scaled by laps (this+860) and a factor (this+836). Retire reason drawn from a fixed table (codes 0-9).
  This is a vanilla RANDOM RETIREMENT from the driver file - not a life timer.
- OPEN: when 0x4BEC55 runs (fast-forward / sim-to-end / non-rendered cars only, or also in a live race).
  Needs a call-count probe (Frida or broker hook) during a live race. If it runs live, vanilla AI retirements
  = timers + CompletedLaps% roll, and the Q4-Q7 "vanilla Phi" baseline is incomplete.
- FuelConsumptionAndWear 0x4ED960 (timer < 0) staged failure: already documented in
  AI_SIMULATION_STATIC_ANALYSIS_2026-09-30.md (smoke banks, torque x0.9, re-arm) - not new.

### Next checks (not done)
1. Live call probe on 0x4BEC55 / 0x4BEF60 during a normal race.
2. Log every AI retirement with ReasonCode + finish + wing/part state at the moment it happens (a small
   read-only watcher) to classify causes per race.

### Live probe result (same session, 110 s window)
21 AI cars completed a lap (veh+0x403C +1) while veh+0x40B8 stayed 0.0 for every car. 0x40B8 is written only
by EngineFailureCheck 0x4BEC55 / AITickUpdate / AIBehaviorInitialization on a simulated lap. DERIVED: the
simulated-lap path (and its CompletedLaps%/Crash retirement roll) does NOT run for rendered AI cars in a live
race; it belongs to fast-forward / sim-to-end. So in a normal live race (mode 1-2) the vanilla AI mechanical
WHEN is the four life timers only (+ start stalls, + damage/pit retirements). Caveat: one session, one mod;
confirm once with a direct call probe if ever needed.

## Pass 3 (Claude, 2026-10-01): retirement handler + owner PLR
### Stationary-car retirement (CONFIRMED static, value DERIVED live)
sub_4F1FE0 (per-car retirement handler): if FinishState == 0 and the car is stationary outside the pit area
(DistanceComparison false, not in pit state 3) a dwell timer accumulates; when it exceeds flt_7D0A8C it clears
the smoke banks, calls sub_41B250 (retire commentary) and, if the car is near/over the track edge, sets
state 0x3D6C = 2 => retirement with ReasonCode 0. AI skip: veh+0x4170 pending start-stall restart. Player skip:
global 0x7CE814 set, or all 4 part records intact and rpm > 0.1.
flt_7D0A8C live = 60.0 and 0x7D0A90 = 60.0, matching PLR `Vehicle Removal="60"` and `Debris Removal="60"`
(setter order in PlayerOptionsInitializer 0x4AA270 lines 1203-1204). SDK name "PitTimerFloat" is wrong.
INFERENCE: Bobbi/Panis (ReasonCode 0) were stationary > 60 s (spin/crash/stall) and removed by this path.
Consequence for the mod: a mod-driven AI power loss that leaves a car crawling/stopped can trigger this
60 s removal; an AI "coast to stop" fault = retirement after 60 s, ReasonCode 0.

### Owner PLR (Documents\F1 Challenge V10\Save\Matias\Matias.PLR, read-only) - failure-related keys
- `<SESSION> Failure Rate` = 2 for CURNT/PRACT/QUICK/GPRIX/CHAMP/MULTI/RPLAY (0 none, 1 normal, 2 timescaled,
  3 season). Mode 2 divides lifetimes by race length (divisor 1.0 live = full-length race).
- `Damage Multiplier="100"`, `Damage Dynamics="1"`: collision damage; engine killed when impact > damage.ini
  Engine (prior finding, 0x3D70 -> -1 sentinel).
- `Vehicle Removal="60"` (stationary retirement, above), `Debris Removal="60"`, `Parts Duration="60"`,
  `Crash Recovery="0"`.
- `<SESSION> Fuel Consumption Multiplier` = 2 (CHAMP/RPLAY 1) and `AI Additional Fuel Mult="0.95"`: out-of-fuel
  stop path (Pitcrew Push=1 only in pitlane) - not investigated here.
- `<SESSION> Tire Wear Multiplier` = 2 (CHAMP/RPLAY 1): tyre wear/blowout path - separate system.
- `AI Power Calibration="7"` (power+gearing+fuel) - AI pace, not failures. `Engine Emitter Flow="0"` - visual.
- No PLR key for engine lifetime itself (that lives in each car's engine.ini) and none for CompletedLaps%/
  StartStalls (driver .rcd).

### Correction: prior art existed (owner caught it, 2026-10-01)
Vehicle Removal / 0x7D0A8C was already known and used by red_flag (`redflag.hardened.c`: VEHICLE_REMOVAL_RUNTIME,
VEHICLE_REMOVAL_ENTRY_OFF 9696, forces and restores Vehicle Removal / Debris Removal / Parts Duration) and by
full_safety_car `safety_car_guardrails.c`. Claude searched only findings.jsonl + the SDK index, where it was
absent. Lesson: search first-party mod source/docs too before re-deriving. Tool gap logged for the SDK.
Broader grep (same day) also hits: full_safety_car trials v0.10.37 panis_retirement_20260914, v0.10.41_removal_owner, v0.10.56-59; pitstop_modding dll/docs/release candidates; career_mod_platform research/raceability-review-2026-09-07. Read those before extending this topic.
