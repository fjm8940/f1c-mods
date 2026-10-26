# AI pull-off and "get off the line" system - draft spec (Claude, 2026-10-02)

Status: DRAFT for owner review, then DeepSeek. Pipeline stage: specify. Product: mechanical_failures (merged DLL).
Rule from the owner: build only on what the game's AI already has by design; every lever below is tiered.
Evidence: `AI_RISK_SCENARIOS_LIVE_2026-10-02.md` sections 2, 5-7 and the v4 trial in section 0.1 below.

## 0. What the AI already has (facts this design uses)

| Building block | Where | Tier |
|---|---|---|
| Current waypoint pointer `veh+0x4354`; look-ahead waypoint pointers follow it (`veh+0x4358..`, AISteering reads 5, the AI keeps a 20-entry array) | AISteering 0x4270A0 lines 190-205, doc 52 | FACT (code) + live (6 distinct pointers read) |
| Waypoint widths `wp+0x2C/0x30` = ROAD edge left/right of the AIW line; `wp+0x34/0x38` = WALL / limit left/right; `wp+0x58/0x5C` = drivable (run-off) width, capped 30 | AIW parser (doc 52 table), live reads: road 2.6-12.1 m, wall always > road (e.g. road 3.8/3.0, wall 15/8) | DERIVED (field order = ISI AIW `wp_width`/`wp_dwidth`; live values consistent; Hill/Hakkinen hit the wall when aimed at 0.9 x wall width) |
| AI lateral chain: request `veh+0x4420` -> target `veh+0x440C` (request clamped to corridor [`0x4418`,`0x4414`], 0.2 m steps) -> `veh+0x4408`; the AI steers itself to the shifted line, corners included | AISteering lines 230-260, 1293-1309 | FACT (code) |
| Request is zeroed every AI tick unless a yield target `veh+0x45FC` is set | AIMainUpdate 0x424E70 line 241 | FACT (code + live) |
| Corridor bounds are recomputed each tick (sub_426430), normally ~+-1 m around the line | live read 20 cars | FACT |
| Opponent awareness: OpponentProximityScan (cars within 25 m, lapping sidestep +-2.5 m via the same request) | 0x42A7D0 lines 60-110 | FACT (code) |
| Surface under each wheel (road / wet road / rumble = drivable; else grass/gravel) | SDK `tools/sdk/f1c_surface_contact.h` (Red Flag chain `veh+0x3674 + i*0xF8 -> +0x54 -> +0x30`) | FACT (live: 4/4 racing, 0/4 after Frentzen's pull-off) |
| AI brake / throttle command `0x4434`/`0x4438` -> actuators `0x3584`/`0x357C` | live correlation 2026-10-01, pull-off trials | FACT (writes hold while the loop runs) |
| Car positions `veh+0x34E0/E8`, speed `0x4440`, heading via AI state | findings | FACT |
| Native "pull over on failure" behaviour | none found (AI modes 1/2/6/7 = stuck / heading recovery) | FACT (negative) |
| Barrier / object collision look-ahead for walls | none found; walls only exist for the AI as waypoint wall widths; cars are the only "objects" the AI scans | FACT (negative, static) |

### 0.1 Proof trial (v4, Frentzen, live 2026-10-02)
Sudden failure at 295 km/h -> coast on his own line with brake 0.1 to 135 km/h -> yield target = self, corridor widened to
+-30 m, request + target + offset ramped 2 m/s -> the AI steered itself to the side -> wheels on road 4 -> 2 -> 0 ->
offset frozen, brake 0.6 -> stopped off track at 5.1 m. Owner: "seemed to pause normally". FACT for one car. The earlier
methods (raw steering, additive nudge, request without the chain) all failed (section 7 of the trial record).

## 1. Behaviour (owner steps 1-4 + "ruined race")

### 1.1 Engine dies (sudden failure / model terminal)
1. **Notice**: 0.3-1.0 s after the bang (driver reaction, per-driver from the sensitivity trait). Until then the car is
   left alone (coasting on the line, the game's own behaviour).
2. **Choose a side** (once, re-checked every 0.5 s until the drift starts):
   - For each side read the look-ahead waypoints covering the stopping distance (`v^2 / (2 x 8 m/s^2)`, at least 3
     waypoints): `room = wall - road` (run-off between road edge and wall). A side is SAFE if `room >= 4 m` on every
     look-ahead waypoint; prefer the safe side with the smaller road distance (shorter drift).
   - No safe side (street circuit, wall at the kerb): target = road edge - 1.2 m on the side with more room (stop at
     the edge of the asphalt, still off the racing line).
3. **Traffic check** (the AI's own 25 m awareness, mirrored): do not start or continue the drift while another car is
   within 25 m behind or alongside AND its offset is on the target side within 3 m of our path; hold the line with brake 0.1
   instead (this is what drivers do: hand up, wait, then pull over). Re-check every 0.2 s; max wait 4 s, then drift anyway
   at half rate.
4. **Coast down**: brake 0.1 (the dead engine already decelerates hard; more brake stopped Coulthard on the line) until
   speed < 140 km/h. Steering untouched.
5. **Drift**: yield target = own vehicle, corridor = [-30, +30], request/target/offset ramped toward the side at
   `rate = 2.0 m/s` (1.0 m/s above 120 km/h), limited to `goal = min(wall - 1.5 m, road + 4 m)` of the CURRENT
   waypoint (re-read every step; if the wall closes in, the goal shrinks). Brake 0.1.
6. **Off the surface**: when >= 3 wheels are known and <= 1 is on a drivable surface -> freeze the offset (hold), brake
   0.6 to a stop. If the goal is reached first (street circuit) -> same hold + brake.
7. **Stay put** (owner step 4): keep yield target + request + corridor held after the stop until FinishState 2 (the game
   then moves the car to the garage row - FACT, ~60-100 s). Never release while the car moves (released cars steer back -
   FACT, Wurz). Then restore `veh+0x45FC` to its saved value.
8. **Abort to "brake only"**: car heading vs waypoint direction > 60 deg (spin starting), or a collision ReasonCode/impact
   fires, or the offset moves away from the goal by > 3 m: stop the drift, keep holding the current offset, brake 0.6.

### 1.2 Race ruined but the car still runs (owner add-on)
"Ruined" = AI reaction level R2 limp, the terminal limp, a scripted pit-to-retire drive (R4 via the pits), or any car whose
pace is > 8 s/lap off its own average for 2 laps (damage). Behaviour:
- Move OFF the racing line but stay ON the road: same lateral chain, goal = road edge - 1.5 m on the side the AI's line is
  NOT using at the next corner (the side away from the apex: sign of the look-ahead curvature/heading change), ramp 0.5 m/s.
- Brake points and corner speed stay the AI's own (no brake writes). PassingNorm low (already in ADDENDUM 14 R2).
- Re-center 2 waypoints before a corner apex only if the side line would leave the road (goal < 1 m); otherwise stay aside.
- On pit entry (pit status >= 1) release everything; the game's pit path takes over.

## 2. Implementation notes for DeepSeek

- New pure header `dll/mf_ai_pulloff.h`: state machine {IDLE, NOTICE, CHOOSE, WAIT_TRAFFIC, COAST, DRIFT, HOLD_STOP,
  PARKED, ASIDE} with inputs (speed, offset, waypoint widths for N look-ahead entries, surface counts, nearby cars'
  relative positions/offsets, heading error, FinishState) and outputs (request offset, corridor, yield-target flag,
  brake command or "leave alone"). No game access; unit tests for every transition.
- DLL pump: write the chain (0x45FC, 0x4414, 0x4418, 0x4420, 0x440C, 0x4408) and brake (0x4434, 0x3584, throttle
  0x4438/0x357C = 0) at >= 200 Hz while active (the external trial used a tight loop; the AI tick is 25 ms). Prefer a
  game-thread hook just before AISteering if the SDK has one (prior-art check first); otherwise the worker pump.
- Ownership: save 0x45FC at start; restore on PARKED -> FinishState 2, session end, unload. Corridor/request need no
  restore (recomputed each tick). Pause = no writes (existing R1b freeze).
- Never write steering 0x442C/0x3580 (raw steering = swerves/walls, FACT) or positions.
- Never write carConditions 0x3D6C (limbo trap).
- Wire into ADDENDUM 14: the Option A ending (sudden failures, model terminals) triggers 1.1; R4 via the pits uses 1.2
  until the pit lane; R2 limp uses 1.2.
- Logs: `AI PULLOFF: slot N <name> side L/R room x m, wait y s, drift z m, stop off-surface|edge, aborted <why>`.

## 3. Tests

- Unit (pure header): side choice with/without room, street circuit fallback, traffic wait + timeout, ramp limits,
  off-surface hold, abort on heading/impact, no release while moving, ASIDE apex rule.
- Live (owner): 6 sudden failures - 2 straights, 2 corners, 1 with a car right behind, 1 at a street-type section (wall at
  the kerb). Pass = car stops off the line, no wall contact, no spin, no steer-back, following car not hit.
  Plus 2 ruined-race cars (R2 limp) holding the side of the road for 2 laps while the field passes.

## 4. Unknowns (tiered)

- `wp+0x34/0x38` = wall distance: DERIVED (ISI convention + live consistency + 2 wall hits at 0.9 x). Confirm with one live
  read next to a known wall.
- Traffic behaviour while the chain is forced (the AI's own sidestep is overridden): UNTESTED.
- Behaviour above ~140 km/h drift start and in fast corners: UNTESTED (only Frentzen at 135 km/h).
- The 0.2 m/step target limiter in AISteering vs our direct target write: works externally (FACT), exact interplay
  inferred.
- External loop vs in-DLL cadence: the DLL must reach the same effect; live re-test after the build.

## 5. "Is the AI on the racing asphalt?" (owner question, 2026-10-02)
- veh+0x40E8 off-track byte: PLAYER-ONLY (cut-track penalty logic, called only for the player) - FACT, not usable for AI.
- Per-wheel surface material (SDK f1c_surface_contact.h): exact contact with the track mesh material, every car - FACT.
  Kerbs (rumble) count as drivable; asphalt run-off built with a road material would also count as drivable - limitation.
- AIW road edge wp+0x2C/0x30 (left/right of the AIW line, per waypoint): the track author's racing-surface width -
  DERIVED. |offset| > road edge = off the racing surface geometrically.
- Rule used by the system: off the racing surface = (<= 1 wheel on drivable material) OR (|offset| > road edge + 1.0 m).

## 6. Free-space scanner (owner request: "how much free space left/right until an object")
What the game already has (FACT unless marked):
- Cars: OpponentProximityScan 0x42A7D0 keeps the 5 nearest cars (records at veh+0x4174, stride 68, distance^2 at +52) within
  25 m, ahead/alongside only (projection filter -5.5). It is car awareness, not a 360 deg scan.
- Walls: the AI has NO runtime wall/barrier sensing. Its only wall knowledge is the AIW per-waypoint widths (DERIVED).
- Track collision mesh: the game builds a 32 x 32 broad-phase grid over the collision geometry at track load
  (0x44BA40, misnamed "CollisionGridRaycast32x32"; query sub_44A880 = box -> node list). A DLL-side mesh query for
  walls is POSSIBLE but unproven (ABI, whether walls are in that tree) - phase-2 research only.
Prototype built and run live (read-only, `%TEMP%\mf_dyn\freescan.py`, disposable):
- True lateral of a car = dot(car pos veh+0x34E0/E8 - waypoint active pos wp+0x94/9C, wp_perp wp+0x20/28) (|perp| = 1.00,
  FACT live). veh+0x4408 is the AI's INTENDED offset, not its position (Wurz: true -2.4 m vs 0x4408 -0.7).
- Wall free space L/R = min over the look-ahead (next pointer wp+0x110, segment length wp+0x3C, 30 m) of
  wall width (wp+0x34 / wp+0x38) -/+ true lateral - half car width 0.9 m.
- Car free space L/R = for every live car within +-5.6 m longitudinally and 30 m laterally, |lateral gap| - 1.8 m.
- Live result: plausible for all 19 running cars; a side-by-side pair (slots 9/10) read 1.2 m gap each, mirrored. FACT
  for the read; wall numbers inherit the DERIVED wall-width meaning.
Use in the pull-off (v5): choose the side with more wall free space over the stopping distance; never ramp the line
further than (wall free - 0.5 m) [owner's 0.5 margin]; pause the drift while car free on that side < 0.5 m; the same
numbers drive the "ruined race - stay aside" mode (1.2).

## 7. In-DLL implementation (owner decision 2026-10-02: option A) - supersedes the external-loop notes in section 2
Why: external control = two writers (AI every 25 ms + script) -> steering/brake flicker in every trial (FACT, trial record
sections 7-10). The fix is ONE writer: a game-thread hook that runs after the AI has computed its outputs.

### 7.1 Hook point (FACT, code)
- MainGameTickUpdate 0x42C690 line 48 calls `AIMainUpdate(dword_7C54F0[i])` for every car (game thread); `this` = vehicle
  base (same pointer array as VEH_BASE + slot x 0x48C0 - DS verifies at runtime and logs a mismatch, then disables).
- At the end of a normal AI tick AIMainUpdate copies its commands to the physics actuators (lines 161-165):
  steer `0x442C` -> `0x3580`, **brake `0x4438` -> `0x3584`**, **throttle `0x4434` -> `0x357C`**, and clears the yield
  target `0x45FC`. (CORRECTION: the external scripts wrote brake to 0x4434 and 0 to 0x4438 - swapped command fields.)
- Detour AIMainUpdate (__thiscall, ecx = vehicle): call the original, THEN for a slot in pull-off/aside mode overwrite
  the command AND actuator fields (0x442C/0x3580, 0x4438/0x3584, 0x4434/0x357C). Physics consumes the actuators after
  the loop -> our value is the only one it sees. No yield-target/corridor/request writes needed any more (and never a
  self pointer - crash suspect).

### 7.2 Control law inside the hook (per pull-off car)
- ai_steer = the value the original just wrote to 0x442C (it already follows corners).
- lat = true lateral (section 6 scanner, waypoint frame); goal per section 1.1 / 6 (grass: road edge + 2.1 m, capped at
  wall free - 0.5 m; cars: pause while car free < 0.5 m).
- DRIFT: steer = ai_steer + clamp(k_lat x (goal_lat - lat) - k_rate x lat_rate, +-lim(v)), lim(v) = 0.06 x min(1, 60/v_kmh)
  ... start values k_lat 0.02/m, k_rate 0.01 s/m; brake 0.1, throttle 0. (Additive nudge failed externally ONLY because it
  was overwritten each tick; post-hook it is exact.)
- GOAL (all wheels off drivable OR wall margin OR |lat| >= goal): steer = ai_steer is NOT used any more: write steer 0
  (wheels straight, owner: no steering manipulation), brake 1.0, throttle 0 until stopped; then set AI mode 0x4510 = 4
  (native park, FACT: AI itself holds steer 0 / brake 1 / throttle 0) and stop writing. Do NOT clear 0x45F4 (game owns it).
- Stop detection: physics speed (not 0x4440 - stale in mode 4); DS finds the physics velocity used by the player HUD
  or derives it from position deltas (veh+0x34E0/E8 per tick).
- Abort: lat_rate > 6 m/s over 0.25 s or heading error > 60 deg -> steer 0, brake 1.0.
- Ruined-race ASIDE mode (section 1.2): same hook, goal = road edge - 1.5 m, throttle/brake untouched (AI's own), only
  the additive steer term, released 2 waypoints before a corner if the side line would leave the road; released on pit
  entry.

### 7.3 Gates
Pure header `mf_ai_pulloff.h` + unit tests (state machine, law signs, clamps, abort, scanner math on recorded waypoint
snapshots); hook install/uninstall clean (session end, unload, pause = original only); AI pump timing unchanged; live owner
test list in section 3 + "no visible steering flicker".
