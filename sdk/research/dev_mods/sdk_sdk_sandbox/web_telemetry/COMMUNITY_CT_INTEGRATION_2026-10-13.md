# Community Cheat Engine table: integration assessment for Telemetry

Date: 2026-10-13. Product: Telemetry (`web_telemetry`), shipped 1.0.1, stage
observe. This record covers investigate + specify only. No code, build,
install or release change was made.

Source: `F1 Challenge Online Telemetry.CT`, given to Matias by a community
member. Evidence copy: `community_ct/F1 Challenge Online Telemetry.CT`
(SHA-256 `633B83111AAE6C7935F0A36A66C026ADF59942F0B83D948C0CA06F25C9FB135B`).
It is plain XML: 150 entries, all static `F1 Challenge Online.exe+offset`
addresses, no pointer chains, no Lua scripts.

## 1. What the table is

- Every address is absolute (the exe is non-ASLR, base 0x400000).
- Almost everything is the **player physics object** `P = 0x7C7428` and its
  four wheel records `W(w) = P + 652 + 1124*w` (FL, FR, RL, RR). This is the
  same block Telemetry already mirrors. **Player car only**; it gives nothing
  for AI cars.
- A few entries are in vehicle slot 0 (`0x5EC6D8..`) and the input block
  `0x7CC9A0..`.
- The table is a community map with names only. It has no evidence. Several
  labels are wrong (section 4).

## 2. What Telemetry 1.0.1 already reads (no work needed)

Tyre surface temps 3 zones (`W+844/852/860`), carcass (`W+868`), tread
(`W+916`), brake disc temp (`W+1028`), tyre load (`W+796`, RESEARCH), oil and
water (`0x7C9174/0x7C91B4`), air/track temp, on/off-line wetness, session end
time. The CT agrees with all of these addresses.

## 3. New channels the CT points at, checked against our decompile

Tier: FACT = decompiled writer/reader proves the role; DERIVED = role inferred
from code use, units or sign not proven; OPEN = no proof. "Live" = not yet
read in a running game for this record.

| Channel | Address (FL) | Wheel/P offset | Our evidence | Tier | Proposal |
|---|---|---|---|---|---|
| Ride height x4 | 0x7C8B18 / B20 / B28 / B30 | P+5872/5880/5888/5896 | `sub_4423B0`:175-205 writes height of 4 floor points above the track surface; `<0` sets the bottoming latch P+5912. `DamperSpringForceComputationWithAntiRoll` 0x4432F0:67/81 averages front pair and rear pair for aero. Metres (penetration spring 385000 x depth). | FACT static, live pending | **Add** (trace + live). Big value: floor contact, rake. |
| Tyre pressure x4 (hot) | 0x7C7A28 | W+884 | `sub_438100` (called every tyre step from 0x438130:125): `W+884 = W+876 x carcass K`; `W+876 = garage pressure W+892 / 359.86 K` (0x43DA10:118-121). Pressure follows carcass temp. Units as .tbc/garage (kPa likely). | FACT static | **Add** with garage value W+892 as reference. |
| Tyre grip x4 | 0x7C7A50 | W+924 | 0x438130: grip = tread x (1 - 0.5 d^2), d from temp and pressure window. Already FACT in mech-failures map. | FACT static | **Add** (shows "in window"). |
| Optimum tyre temp x4 | 0x7C7E4C (FR) | W+820 | Used as optimum in grip and wear formula (0x438130). | FACT static | Add as constant per session (reference line on temp charts). |
| Camber x4 | 0x7C79C8 | W+788 | Written in `CollisionContactGenerationAndFrictionResponse` 0x436870:508; scaled x 5.5556 to split heat across zones (0x438130:56). Live dynamic camber, rad likely. Sign per side unproven. | DERIVED | Add as RESEARCH until a live check fixes unit and sign. |
| Tyre deflection x4 (CT "Suspension Deflexion") | 0x7C79D8 | W+804 | 0x436870:402-446 writes contact penetration, clamped >= 0 (0 = wheel in air); tyre force = W+556 x W+804. It is **tyre** compression, not suspension travel. | DERIVED | Add as RESEARCH; doubles as "wheel off ground" flag. |
| Tyre spring rate x4 (CT "TireSR") | 0x7C78E0 | W+556 | `sub_438100`: `W+556 = pressure x W+908 + W+900`. Pressure-dependent vertical stiffness. | FACT static | Low value; skip from UI, keep in research. |
| Wheel spin x4 | 0x7C7770 | W+188 | Brake heat uses abs(W+188) x brake torque (map section 4). Angular speed, rad/s likely. | DERIVED | Add; gives lock-up / wheelspin vs car speed. |
| Disc thickness x4 (CT "Brake Wear") | 0x7C7AE0 | W+1068 | Brake model: thickness -= T^3 x rate x power x dt; live 0.028 m new. | FACT | **Add** (brake wear). |
| Brake failure point x4 | 0x7C7AF8 | W+1092 | Random failure thickness, live 0.0133-0.0150 m. | FACT | Add, shown only as "margin to failure"; it is a hidden game value, owner decides if players should see it. |
| Packer contact x4 + 3rd x2 | 0x7C771C, 0x7C88F4, 0x7C896C | W+104, P+5324/5444 | Not traced this pass. | OPEN | Skip until traced. |
| Spring / damper rates (FB, SB, FR, SR), ARB, 3rd springs | W+0/8/16/40/48, P+5176.. | setup constants | Setup values, do not move on track. | OPEN | Skip for trace; possibly a setup panel later. |
| Garage ride height x4 | 0x7C8B50.. | P+5928.. | No code use found for 5928-5952 this pass. | OPEN | Skip. |
| Engine RPM, clutch RPM, rev limit, fuel, brake bias, inputs | various | | Telemetry already has rpm/inputs from the vehicle struct. CT brake bias 0x7C898C = P+5476, matching the map's P+5476 (the map's hex 0x7C899C is a typo). | n/a | No change. |

Mirror cost: the existing `ADDR_WHEEL_THERMAL` mirror (0x7C7A00, 4496 B)
already covers ride height (0x7C8B18-0x7C8B38), pressure, grip, optimum,
thickness and failure point. Camber and tyre deflection for FL/FR/RL are in
the `ADDR_WHEEL_LOAD` mirror (0x7C79D0, 3380 B), but **RR deflection
0x7C8704 is 0 bytes past its end**. Wheel spin W+188 is in neither. Simplest
change: replace both with one player-physics mirror 0x7C76B4..0x7C8B70
(5308 B).

## 4. Errors in the community table

- **Camber corners scrambled:** "Camber" = W1 (FR), "FR Camber" = W0 (FL),
  "RL Camber" = W3 (RR), "RR Camber" = W2 (RL).
- **Tyre zone labels mirrored by side:** FL zone 0 = "outer", zone 2 = "In";
  FR zone 0 = "In", zone 2 = outer. This says zone index runs car-left to
  car-right. Our map (NATIVE_SIMULATION_MAP section 5) says inner/middle/outer.
  **Conflict, OPEN.** Static heat split pushes camber heat to zone 2 for
  positive W+788 on every wheel, which would fit the CT reading only if W+788
  sign flips by side. Needs one live test.
- "Suspension Deflexion" is tyre deflection (see table).
- "Tire Pressures" is hot pressure, not garage pressure.
- "Brake Wear" is disc thickness in metres.

## 5. Correction to prior art

`development/mods/mechanical_failures/docs/NATIVE_SIMULATION_MAP_2026-10-06.md`
section 11.2 says `W+884` "never changes - no hot-pressure build-up". Static
code says it is recomputed every tyre step from carcass temperature. The live
sample behind that sentence had carcass 85.4-86.5 C, almost the 86.7 C start
value, so pressure looked constant. Recorded in `findings.jsonl`
(`tyre-hot-pressure-follows-carcass-20261013`). The map itself was not edited.

## 6. Integration plan (Telemetry 1.1.0, not started)

1. **Live probe first** (owner, game running, read-only, f1c-memory or
   `tools/poke.py`): ride height on straight vs kerb vs bottoming; pressure
   rise with carcass temp; camber sign on FL vs FR; zone order with a known
   camber change; W+188 vs speed / wheel radius; W+804 = 0 over a kerb jump.
2. DLL: one player-physics mirror; new player channels ride height x4,
   pressure x4 (+ garage), grip x4, disc thickness x4 (+ failure margin),
   wheel spin x4; camber x4 and tyre deflection x4 as RESEARCH.
3. Trace: `trace_sample_t` is 160 B; adding ~20 floats raises trace memory
   (2 cars x 7 laps x 2400 samples). Re-measure against the 2.2 MiB budget
   and the 1.5 MB worst-case trace buffer, or reuse the player's `rx[16]`
   slots (susp/slip/force are AI-only research today).
4. API version bump, contract update, viewer pages: ride height / bottoming
   chart, pressure + grip window on Tyres page, brake wear on Brakes page.
5. Full gate (`TELEMETRY_TESTS=1`), then owner live check, then release flow.

## 7. Pending / limits

- No live read was done for this record. Every "live pending" row above is
  unverified in a running game.
- Units of pressure, camber and wheel spin are not proven.
- Zone order conflict open.
- Owner decision needed: show brake failure point to players or not.

## 8. Which of these do AI cars get? (static pass, same day)

`VehiclePhysicsModeDispatcher` 0x4E4930 picks the physics per car from
`veh+0x32C8`: 0 = full physics on the **single static object 0x7C7428**
(the player), 1 = AI physics `PhysicsIntegrationStateMachine` 0x4E4E60 on
the per-car float wheel blocks `veh+0x358C` stride 0xF8, 2 = remote/replay.
So every CT channel is player-only by design. The AI path is a reduced model.

| CT channel | AI simulates it? | AI field (wheel = veh+0x358C+0xF8*k) | Evidence | Tier |
|---|---|---|---|---|
| Tyre temps 3 zones + carcass | **No** | none found | no 273.16 tyre code in AI path | DERIVED (absence) |
| Tyre pressure, optimum, grip window | **No** | none found | AI has no pressure term | DERIVED (absence) |
| Tyre wear | Yes, simpler | +0xB0 tread, floor 0.75 | `FuelConsumptionAndWear` 0x4ED960: load x rate x option, less when sliding; no temperature term | FACT static |
| Brake temperature | **Yes, per axle** | +0x5C K (front pair shared, rear pair shared) | `WheelTireForceUpdate` 0x4ED810, AI only, every 50 ticks; heat = wheel spin x brake torque | FACT static |
| Brake disc wear / failure point | No | none found | | DERIVED (absence) |
| Tyre deflection / suspension | Yes (one combined spring) | +0x74 compression (<0 airborne) | `TireContactForceComputation` 0x4E3AA0 | DERIVED |
| Tyre load | Yes | +0x44 = spring x compression - damper x velocity | same | DERIVED |
| Slide / grip use | Yes | +0x50: 1.0 gripping, <1 sliding, 0 airborne | `WheelSpringDamperForceAndSteering` 0x4E3D00 | DERIVED |
| Wheel spin | Yes | +0x30 | used in AI brake heat | DERIVED |
| Camber | Not found | | | OPEN |
| Ride height (floor points) | Not found | | | OPEN |
| Fuel | Yes | `veh+12 + 4*3678` burn rate x throttle | 0x4ED960 | DERIVED |

**Bugs this exposes in Telemetry 4.3.0+ research channels:** `WB_OFF_TEMP_K
0x50` is the slide factor, not an AI tyre temperature (the 200-700 K filter
means it always reads invalid, so nothing wrong is shown, but the channel
is mislabelled). `WB_OFF_SUSP 0x10` is not the compression; compression is
+0x74. Real AI brake temperature (+0x5C) is available and not shown.

**Ledger correction:** `CHANNEL_LEDGER_2026-10-09.md` section 1.3 says
+0x5C is a spring/damper value written by 0x4E4F40:49-51. That code writes
+0x68/+0x6C (wheel inertia). +0x5C is AI brake temperature. Ledger not
edited; see `findings.jsonl` `ai-brake-temp-wheel-5c-per-axle-20261013`.

Live test needed (game running, read-only): AI +0x5C rises in braking
zones and front != rear; +0x74 / +0x44 / +0x50 / +0x30 move as described;
player CT channels from section 6 step 1.

## 9. Live probe results (2026-10-13, read-only)

Tool: `ct_probe/ct_live_probe.py` (elevated, PROCESS_VM_READ only, exe hash
checked, equal-sim-time reads), analysis `ct_probe/ct_probe_analyze.py`.
Captures in `%TEMP%\ct_probe\run1.jsonl` (450 samples, sim 95.9-185.8 s) and
`run2.jsonl` (600 samples, sim 242.3-362.3 s), 5 Hz, PID 8112, exe
`1A70E5ED...`, 24 cars, race session, simulation advancing. Captures are
disposable per-run output (not kept in the workspace).

**Limit: the player car was stationary in both runs** (idle 4032 rpm, wheel
spin < 0.13 rad/s, brakes 30 C). Player dynamic behaviour (brake heating,
suspension and tyre deflection under load, wheel spin vs speed, engine wear
at high revs, bottoming) is NOT live-verified.

Player, verified live:
- Hot pressure: `W+884 / carcass K` is exactly `W+876` (0.3751 front,
  0.3613 rear = garage 135/130 / 359.86) on every sample; pressure fell
  130.3 -> 114.4 kPa as carcass cooled 74 -> 43 C. **FACT live.**
- Ride height P+5872..5896: front 0.032-0.033 m, rear 0.060 m vs garage
  0.03/0.06; order FL,FR,RL,RR. **FACT live (static car).**
- Suspension deflection `W+488` (game recorder channel): front 0.0033 m,
  rear 0.0068 m static. Plausible, dynamic unverified.
- Tyre deflection `W+804` 0.006-0.008 m loaded, load `W+796` 1360-1890.
- Camber `W+788`: +0.042 FL / -0.042 FR / +0.016 RL / -0.016 RR (sign
  mirrors by side). Zone 0 minus zone 2 follows the camber sign on every
  wheel (+1.9/-2.0/+0.8/-0.75 K), so the zone index runs across the car in
  one direction (car-left to car-right), as the CT says; the corpus label
  "inner/middle/outer" is wrong for one side. DERIVED live.
- Disc 0.028 m, failure thickness 0.0143-0.0152 m (per wheel), packer
  0.0116 front / 0.0278 rear (setup constant).
- Engine life drain at idle 0.00060-0.00066 s/s, stress 0.

AI, verified live (23 AI cars):
- **AI brake temperature `+0x5C`**: wheel0 == wheel1 and wheel2 == wheel3 on
  all 13,800 samples (per axle); mean step +41.8 K while brake > 0.5 vs
  -7.2 K otherwise; front up to 1795 C, rear 1538 C. **FACT live.**
- AI wheel spin `+0x30`: corr 0.996 with speed (negative sign, rad/s).
- AI slide factor `+0x50`: 0..1. AI compression `+0x74`: -0.04..0.13.
- AI tread `+0xB0` 0.996-1.0 (2 min of running).
- **AI engine timers** (veh+0x3D70..0x3D7C) drain exactly 1.000 per game
  second for every car, independent of revs or driver. FACT live.
- **AI fuel**: per-car coefficient `veh+0x3990` 5.56e-5..5.81e-5; burn
  3.74-4.42 per minute across drivers (rpm x throttle x coefficient), so
  fuel use differs by car AND by how the AI drives.

## 10. Vanilla f1_telemetry.exe (owner question)

The vanilla viewer reads **no game memory**. The game itself writes
`telemetry.spt` (writer `TelemetryDataDump` 0x4D4A80 / `sub_447F30`) with 57
player channels from P = 0x7C7428 (see
`legacy_dossier/FORMAT_SPECS_f1_telemetry_2026-10-09.md`). The viewer adds 29
computed channels (86 in `.lap`): speed, distance, weather, crown/camber
temps, average temps, front/rear ride height, damper velocity, rear wheel
speed differential, aids. The game's own recorder confirms CT addresses:
wheelspin `W+188` (rad/s), suspension deflection `W+488`, ride height
P+0x16F0+8k, tyre L/C/R `W+844/852/860`, pressure `W+884` (kPa), wear
`W+916`, corner weight `W+796`. Vanilla has **no brake temperature, brake
wear, grip, camber, fuel or engine-life channel** and nothing for AI cars.

## 11. Owner scope (2026-10-13)

Owner wants brake temperatures, brake wear, raw wheel speed, an engine
stress graph (wear rate from temperature and revs), per-driver differences
and fuel consumption. Owner directive: **vanilla game data only; do not
track Mechanical Failures values for now.**

Vanilla answer per item:
- Brake temps: player 4 wheels `W+1028`; AI per axle `+0x5C`.
- Brake wear: player disc thickness `W+1068` (+ failure `W+1092`); AI none.
- Raw wheel speed: player `W+188`; AI `+0x30`.
- Engine stress graph: player only, = drain rate of life `0x7C91DC`
  ((rpm curve + oil temp curve)/2 + stress). AI engines have no wear model
  (time-only countdown), so no AI engine-stress graph exists in vanilla.
- Per-driver differences: AI fuel burn (driving + per-car coefficient) and
  tyre wear (load and sliding) differ per driver; AI engine life is a random
  seed per car, not driving-dependent.
- Fuel: player `P+0x19C`; AI `veh+0x3984` (already in Telemetry).

Next: one more probe run with the player actually driving (hard braking,
kerbs, high revs) to verify the player dynamic channels.

## 12. Depth check of the "AI does not get X" claims (same day)

Owner asked how deep the absence claims go. Added checks:
- `SuspensionAndWheelPhysics` 0x4E5F60:463-478 (AI setup) sets wheel +0x5C to
  ambient + 273.16 K: the AI brake temperature start value. No tyre
  temperature is initialised.
- `FuelConsumptionAndWear` 0x4ED960:94-104: the prior-art "tyre temperature
  blend" is wrong. It is AI **grip** `+0x9C` = lerp(dry `+0xA0`, wet `+0xA4`,
  on-path wetness 0x8AFEE8) x tread `+0xB0` x car factor x track-grip lerp.
  AI has a grip value (weather + wear), with no temperature term.
- `TireBrushModelForceSolver` 0x4E8010 and `sub_4F18C0` (compound apply): no
  temperature globals or Kelvin conversion.
- The AI brake temperature is read only by the visual pass (`sub_4EE2D0`
  reads the glow factor at veh+12+2252); no reader makes it fade the brakes.
  INFERENCE: AI brake temperature is cosmetic (disc glow).

Still not checked: every reader of each AI wheel field across all 7138
functions; whether AI brake failure (timer code 5) relates to any wear field.
Tier of "AI has no tyre temperature / pressure / disc wear": DERIVED
(absence in the AI physics path and its setup), not FACT.

## 13. Full-corpus sweep of AI wheel-block access (same day)

Tool: `ct_probe/ai_wheel_sweep.py` over all 7138 `functions/*.c`. It flags any
byte offset or float index landing in veh+0x358C..0x396C (frames veh and
veh+12) and any 62-float / 248-byte wheel stride. 49 functions matched (list
reproducible by rerunning the tool). Each was checked for temperature,
pressure and wear signals (273.16, air/track/wetness temp globals, the
tyre-wear option, the 1/359.86 pressure constant) and the relevant ones read:

- Temperature code touching AI wheels exists in exactly two functions:
  `SuspensionAndWheelPhysics` (sets brake temp +0x5C to ambient at setup)
  and `WheelTireForceUpdate` (brake heating). Both are brake, not tyre.
- `sub_4F18C0` (AI compound apply): tyre stiffness/grip terms are computed
  **once** from the compound at a fixed reference (helper 0x4D6AE0 at load
  190 and the compound's own temperature constant). AI has no live pressure
  and no live tyre temperature.
- `BrakeCoolingPostTreatment` 0x4E5080 is misnamed: it is gear/clutch
  rev-matching from driven-wheel spin and gear ratios, not brake cooling.
- `RideHeightDelta` 0x4EF680: front-axle average of wheel +0x10 minus +0x90
  (wheel vertical position vs rest), used by its caller as a pitch/height
  delta. AI has wheel travel but no floor/undertray points.
- Remaining matches are steering, collision, sound, render, setup UI and
  force aggregation (skeleton summaries + zero temperature/pressure signals).

Result: AI has no tyre temperature, no tyre pressure, no brake disc wear and
no engine wear-rate in the vanilla game. Tier stays DERIVED (exhaustive text
search of the decompile; pointer aliasing could hide an access). Closing
step if needed: a live race-long diff of all 62 floats of each AI wheel block.

## 14. Owner decision and build plan (2026-10-13)

Owner: "apply them to players" - build the vanilla channels into Telemetry.
Vanilla data only; Mechanical Failures values out of scope.

Player channels (full physics, P = 0x7C7428, W(w) = P+652+1124w):
| Channel | Source | Unit / transform |
|---|---|---|
| Ride height x4 | P+5872+8k | m -> mm; <0 = floor contact |
| Bottoming latch | byte P+5912 | flag |
| Suspension deflection x4 | W+488 | m -> mm |
| Tyre pressure x4 | W+884 | kPa (hot) |
| Garage pressure x4 | W+892 | kPa, session constant |
| Tyre grip x4 | W+924 | 0..1 |
| Optimum tyre temp x4 | W+820 | K -> C, session constant |
| Camber x4 | W+788 | rad -> deg (unit DERIVED) |
| Wheel spin x4 | W+188 | rad/s (raw wheel speed) |
| Brake disc thickness x4 | W+1068 | m -> mm (brake wear) |
| Brake failure thickness x4 | W+1092 | mm, owner decides visibility |
| Engine life | 0x7C91DC | s remaining |
| Engine wear rate | d(engine life)/dt | s/s = the engine stress graph |
| Fuel | P+0x19C | litres, per-lap use derived |
Already shipped and kept: tyre zones/carcass, tread, brake temp, load,
oil/water.

AI channels (vanilla): brake temperature per axle (+0x5C, K -> C), wheel
spin (+0x30). Fix mislabelled research channels: +0x50 = slide factor,
compression = +0x74.

Implementation order: (1) one player-physics mirror 0x7C76B4..0x7C91E4,
(2) live/SSE fields, (3) trace samples (re-measure the trace memory
budget), (4) API version bump + contract, (5) viewer: Tyres (pressure,
grip window, optimum line), Brakes (temp, disc wear), Chassis (ride height,
suspension, bottoming), Engine (wear-rate graph, fuel per lap), (6) full
gate with the game closed, (7) owner live check, (8) release flow.

## 15. Live driving run (run 3, 2026-10-13)

`run3.jsonl`: 899 samples, sim 1101-1281 s, owner driving, 5 Hz, full AI
wheel blocks (62 floats x 4 wheels x 23 AI cars) included.

Player, now verified while driving (FACT live unless noted):
- Wheel spin `W+188`: corr 0.997-0.999 with speed; speed/|spin| = 0.345 m
  front, 0.333 m rear = rolling radius. Unit rad/s.
- Hot pressure: ratio exact on all samples; FL 114.7-133.9 kPa, carcass
  32.6-83.6 C.
- Grip `W+924` 0.939-0.998 (drops off-window).
- Ride height down to 0.0007-0.0037 m; bottoming latch P+5912 seen set.
- Suspension `W+488` 0.0004-0.034 m (rear more travel). Tyre deflection
  `W+804` 0-0.044 m; load up to 10,440.
- Brake temps up to 834 C; disc thickness 0.0280 -> 0.02794 m (brake wear
  moves, ~60 um in 3 min of driving).
- Zones while driving: FL zone2 hotter (-8.8 K), FR zone0 hotter (+8.7 K),
  RL/RR same pattern; matches the static heat split (positive camber value
  heats zone 2). Combined with the stationary runs: zone index runs one
  fixed direction across the car, so zone 0 is the left edge on every
  wheel (inner on the right-side tyres, outer on the left). DERIVED (no
  direct screen check of which edge is which).
- Camber `W+788` -0.08..+0.105 (moves with load/roll; unit rad DERIVED).
- Engine life drained 27.8 s in 179.9 s sim; wear rate 0.011-2.06 s/s,
  corr +0.59 with rpm (3537-17560) and +0.62 with oil (107-123 C). The
  engine stress graph is real and meaningful. Stress field stayed 0.
- Player fuel 2.48 per minute.

AI full-block diff (closes the "AI does not get X" check):
- Only fields that fall steadily over the race: `+0xB0` tread and `+0x9C`
  grip (84 of 92 wheels). No other wear-like field (so no brake disc wear,
  no engine wear rate in the wheel block).
- Only temperature-like field: `+0x5C` brake temperature (303-2086 K). No
  field moves in a tyre-temperature range.
- `+0xA0/+0xA4/+0xB8` changed on one car only (pit stop: compound refit).
- `+0xB4` read 0 on every wheel although tread wears: the 0x4ED960 frame
  assumption (`this` = veh+12) or the field role is off by one slot. OPEN,
  minor; Telemetry keeps reading tread at +0xB0, which is live-proven.
- Tool: `ct_probe/ai_wheel_rawdiff.py`.

Verdict: vanilla AI gets brake temperature (per axle), wheel spin, load,
compression, slide factor, tread and grip; nothing else from the CT list.
FACT live (one 3-minute race window, 23 AI cars).

## 16. Extra player channels, live run 4 (2026-10-13)

`run4.jsonl`: 600 samples, owner driving (0-82 m/s). Analysis
`ct_probe/ct_probe_extra.py`.

| Channel | Source | Result | Tier |
|---|---|---|---|
| Raw vs used throttle | P+0x48 (raw) vs P+0x08 (used); raw also at 0x7CC9A0 float (corr 1.000) | differ in 226 of 600 samples, both directions (-1..+1): traction control cuts and auto-blips are visible | FACT live |
| Raw vs used brake | P+0x50 vs P+0x10 | identical all run (no ABS intervention seen; ABS setting not recorded) | FACT live |
| Raw vs used steering | P+0x40 vs P+0x00 | differ up to 0.675 (steering help/filter) | FACT live |
| Raw vs used clutch | P+0x20 vs P+0x18 | auto-clutch visible; 0x7CC9B0 read 0 (not the raw clutch) | FACT live |
| Clutch rpm | P+0x1E38 x 9.5493 (game recorder channel) | differs from engine rpm by >200 in 107 samples (slip) | FACT live |
| CT "Clutch RPM" 0x7C908C | P+0x1C64 | equals engine rpm (1 sample off): **CT label wrong** | FACT live |
| Oil pressure | P+7540 | 200-955, corr +0.991 with rpm | FACT live |
| Brake bias | P+5476 | 0.41 constant; owner had no bias key bound, change not tested | pending |
| Aero drag in use | P+7068 | 0.41-0.56, corr +0.66 with front ride height, -0.74 with speed | FACT live (unit open) |
| Base drag / lift | P+6876 / P+7060 | constant 0.408 / -0.179 (setup) | FACT live |
| CT "Front Wing Height (?)" | P+0x1908 | 0.087-0.30, moves with speed | DERIVED, role open |
| CT "Front Wing Value" 0x8D05D0 | | oscillates -7.36..7.35 every sample: not a wing value | rejected |
| Suspension damage | P+1124w+1104/1105/1116 | 0 all run (no damage happened) | untested |

## 17. ABS test, live run 5 (2026-10-13)

Owner: one heavy stop with ABS off, then ABS on and more heavy stops
(invulnerability on). `run5_abs.jsonl`, 749 samples; `ct_probe/ct_probe_abs.py`.
Wheel "roll" = |W+188| x rolling radius (0.345 front, 0.333 rear) / car speed;
1.0 = rolling freely, 0 = locked.

- Stop 1 (t 1761-1763, 73 -> 19 m/s, ABS off): **front wheels locked**:
  FR roll 0.39 -> 0.14 -> 0.00, FL 0.38 -> 0.00, rears to 0.73-0.82.
- Stops 2-4 (ABS on): no lock; fronts stay 0.95-1.00, rears 0.90-0.98.
- Raw brake P+0x50 == used brake P+0x10 on every sample in all stops: ABS
  does not act through the pedal value. It acts at the wheel (applied brake
  torque, W+980 per the brake model; not recorded in this run).

Telemetry consequence: lock-ups and ABS work are shown through per-wheel
roll ratio (wheel speed vs car speed), not through the brake pedal channel.
The cars DO lock without ABS (owner suspected they did not). The owner
reported the order of the ABS switch; which stop was first after enabling
is inferred from the data (stop 1 is the only lock-up).

## 18. Wet / TC low / brake bias / crash / pit run (run 6, 2026-10-13)

Owner: wet track, intermediates, TC set to low, brake bias changed, crashes,
pit stop, session ended. `run6_wet_pit.jsonl`, 2995 samples;
`ct_probe/ct_probe_wetpit.py`, `ct_probe_extra.py`.

- **Brake bias P+5476 live:** 0.41 -> 0.405 -> 0.40 (t 50 s, owner key);
  back to 0.41 at the garage reset (t 937.9). FACT live.
- **TC low:** 132 throttle-cut samples (used < raw), 519 used > raw (blips);
  rear wheel roll ratio up to 7.4, 259 samples > 1.10 (wheelspin). FACT live.
- **Wetness** on-line 0.41 -> 0.29 (drying line), off-line 0.41 -> 0.39; air
  24 -> 26 C, track 30 -> 31 C. FACT live.
- **Damage (crash t 184-197 s):** P+1124w+1105 bent flags set RR, RL, FL in
  turn; RR detach flag (P+1124w+1104) set at t 196.5; bend angle
  (P+1124w+1116) RL -0.121, FL 0.0012, RR 0.0009. Body lift P+7060
  -0.179 -> -0.111 and base drag P+6876 0.408 -> 0.462 at the same moments:
  **aero damage is visible in these two values.** FACT live.
- **Car reset to garage (t 937.9; owner confirms NO pit stop was made):**
  fuel 56.75 -> 65.00 (start load); all treads 1.0; carcass 86.7 C (the
  game's pre-warmed tyre); damage flags, bend angles, lift, drag and brake
  bias (0.41) reset. FACT live. A real pit stop is still untested.
- **Compound:** P+5212 changed 1 -> 2 at session start (optimum temp
  112 -> 107 C) and stayed 2; no compound change was requested, so this is
  consistent. Live compound change at a pit stop still untested.
- **Session end (sim time jumps to ~1.0e9):** player object reads zero
  (pressure 0, optimum -273 C). Telemetry must treat a zeroed block and the
  1e9 sim-time sentinel as "no data". FACT live.
- Lift P+7060 now correlates -0.80 with front ride height (it moved with
  damage this run; constant in run 4).

## 19. Pit stop, hard -> soft (run 7, 2026-10-13)

Owner: started on hards, pitted for softs. `run7_pit.jsonl` (first 331
samples analysed, sim 3.5-69.6 s; the recorder ran on to its 480 s limit).

- t 42.0 (stationary in the pit box): compound P+5212 0 -> 1, optimum tyre
  temp W+820 114 -> 112 C, treads to 1.0, carcass to 86.7 C. FACT live.
- t 48.8: fuel 64.55 -> 65.00 (refuel after the tyre change). FACT live.
- Compound index -> optimum: 0 = 114 C (hard), 1 = 112 C (soft), both match
  the vendor manual. Index 2 = 107 C was seen in run 6 with tyres the owner
  called intermediates; the manual lists 109 C for intermediate and 107 C
  for wet. Label for index 2 OPEN; Telemetry should show the optimum
  temperature and the raw index, not a guessed compound name.
- Track drying: rain 0.13 -> 0, on-line wetness 0.13 -> 0.

## 20. Implementation phase 1: DLL channels (2026-10-13, dev only)

Owner: "apply them to players"; game closed before build. Vanilla data only.

AI wheel order, proven live before coding (run 3): the player's vehicle
wheel block copies W(0..3) spin to 0.001 rad/s at indices 0..3, so the
vehicle wheel block is **FL,FR,RL,RR from index 0**. On AI cars only
indices 0,1 steer (+0x28/+0xC0) and only 2,3 carry driven-wheel inertia
(+0x68). Telemetry 1.0.1 used the F1CUI order (index 0 = RR): **AI tyre
wear corners were mirrored (FL showed RR, FR showed RL)**, and the research
channels too. Fixed in `OFF_TIRE_WEAR_*` and `wb_index`.

Changes (`web_telemetry.c`):
- New mirror `ADDR_PLAYER_PHYS` = 0x7C7428, 0x1E40 bytes (the game's code
  references 0x7C7428 directly, so the layout anchor check still holds).
- `player_chassis_capture()` / `player_chassis_roll()`: ride height, suspension,
  tyre deflection, hot + garage pressure, grip, optimum temp, camber, wheel
  spin, roll ratio (radius learned while coasting), disc and failure
  thickness, damage flags and bend, bottoming, compound, raw/used inputs,
  fuel, engine/clutch rpm, oil pressure, brake bias, drag/lift/drag in use,
  engine wear rate (life lost per second, smoothed). Zeroed block or the
  ~1e9 session-end sim time = `v:0`.
- `/telemetry` gains top-level object `pch`; each car gains `abt` (AI brake
  temp C, front/rear axle) and `aws` (AI wheel spin, raw rad/s).
- `API_VERSION` 7 -> 8; `schemas/telemetry.json` + `schemas/version.json`
  (update_version.py 8); test expectations moved to 8.
- `runtime_test.c` `test_chassis()`: zeroed block, values, JSON, engine wear
  rate, roll ratio, AI axle temps, corrected tread order, session end.
- `mirror_regions.json` lists the new region; `VERIFICATION_MANIFEST.json`
  payload re-pinned to the dev candidate.

Verification (game closed): full SDK build with `TELEMETRY_TESTS=1` exit 0:
runtime **46,465 checks PASS**, contract v8, HTTP/SSE, endpoint audit,
soak 20 x 2 h, fuzz 80,000 iterations 0 crashes; `/telemetry` 152,589 B
(bounded). Synthetic capture+read p95 259 us (budget 500 us). Dev DLL
`BB2891CD2A0B43644367CCD09B455CD67DEEF014905B3487C7297972FB6D34F0` in
`%TEMP%\f1c-web-telemetry-1.1.0-dev` only. Not installed, not released.

Not done yet: console UI pages, lap-trace channels (engine wear graph over
a lap needs the trace), product version 1.1.0, owner live check.
Note: `f1c_mod.json` build.dll still points at the 1.0.1 TEMP path, so the
CLI's final "Build verified" line names the old DLL; harmless, fix at
release. Before-states: `workspace_organization_audit/TELEMETRY_CT_BEFORE_2026-10-13/src/`.

## 21. Implementation phase 2: console UI (2026-10-13, dev only)

`telemetry_viewer.js` (embedded into the DLL at build):
- New page **Car Chassis Live** (menu group Car): per-wheel table FL/FR/RL/RR
  (tyre temp L/C/R, ideal temp, hot pressure + garage, grip %, tread %,
  camber, brake temp, disc mm + margin to failure, suspension, tyre squash,
  ride height, wheel speed with LOCK/SPIN tags, damage BENT/DETACHED);
  driver-aid panel (your input vs what the car used: TC CUT / BLIP / HELP,
  note that ABS works per wheel); ride height / aero / damage panel (bias,
  drag, lift, drag in use, floor SCRAPING, fuel, clutch slip, oil pressure,
  compound index) with a front/rear ride-height chart. AI focus car shows
  its per-axle brake temps and wheel spin with a note that AI simulates
  nothing else.
- **Engine Live** page gains "Engine wear rate" panel: engine life used per
  second over the session, plus rpm and oil pressure chart.
- Client history (`pchPush`), 3000 samples, reset on new session.

Review: `%TEMP%\ct_ui_review` replays the owner's real run-3 drive through
the new pages (preview config `webtel-110-chassis-review` in
`.claude/launch.json`); no console errors; screenshots checked for both
pages. Synthetic race fields, real chassis values.

Gate (game closed): pins updated (`pin_sidecars.py`, then `--payload`),
`viewer_test.js` page count + chassis checks; full SDK build with
`TELEMETRY_TESTS=1` exit 0: runtime **46,472 checks PASS**, **VIEWER PASS 416**,
contract v8, HTTP/SSE, soak, fuzz. Candidate payload
`%TEMP%\f1c-web-telemetry-1.1.0-dev\payload`, DLL
`35C9AA5D9E378ECCFDE6420038607310EF11E73E72D3CD566687E3832DFE68F4`,
viewer `335A0928290CBD3A8AA1A93598AB95767488C9F1B3D1511D8A73442DDC0D3B4C`.
**Not installed. Not released.** Product version still reads 1.0.1.

Pending: owner-approved install of the dev payload + live look; lap-trace
channels (ride height / engine wear vs lap distance, compare laps);
version 1.1.0 + release flow; brake-bias meaning (rear share is DERIVED).
