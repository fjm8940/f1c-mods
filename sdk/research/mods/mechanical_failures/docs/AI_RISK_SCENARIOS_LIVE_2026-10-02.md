# AI risk scenarios + pull-off research - live trial (Claude + owner watching, 2026-10-02)

Game pid 9672, 1998 season race, installed dll D7F23A4E (ADDENDUM 14 dev dll 6CCBA4A2 NOT installed - levers
driven externally by `%TEMP%\mf_dyn\exp.py`, disposable). All writes owner-requested in chat. Every running car
restored from its snapshot; scenario cars that were ended stay retired (race state, not restorable).
Access note: the game ran elevated; external writes need an elevated shell (medium-integrity shell got error 5).

## 1. Scenarios (FACT = measured; owner = owner eyes)

| # | Scenario | Car | Result |
|---|---|---|---|
| 5 | sudden failure: bank1 0.3 puff 3 s, then power 0 + idle 0 + ReasonCode 1 + bank2 0.5 + bank3 1.0 | Tuero s20 | owner: blew up. 282 -> 0 km/h in ~20 s, stopped ON track. Fire (bank3) persists until rewritten -> real build needs a fade-out |
| 4 | retire to garage: bank1 0.4 grey 30 s, then wing path (0xD58=0 + callToBox 1) | Magnussen s19 | drove on ~100 s to the pit lane, FinishState 2, game cleared the banks. ReasonCode stayed 0 (results reason likely not "Engine") |
| 1 | hopeless limp R2: cap triple 0.85, power 0.80, PassingNorm x0.3, TrackAggression x0.5 | Alesi s13 | +16 s/lap (124 vs 108 s), P6 -> P19 in 2 laps (~1 lap lost per ~7 laps) |
| 2 | mild nurse R1: cap 0.94, power 0.95, aggression x0.8 | Fisichella s10 | +1-2 s/lap, P11 -> P12 |
| 3 | pit check R3: callToBox alone | Wurz s9 | pit 2 at +13 s, service 676-683 s (~7 s), out at +34 s; lap cost +20 s, P6 -> P17. Clean |
| 6 | mistake -> blow-up: R1 40 s, cap lifted + misfire 2 s, then blow | Rosset s21 | ran as scripted; owner verdict pending |
| 8 | limp -> retire as Engine: R2 100 s, grey smoke, ReasonCode 1 + wing path | Diniz s16 | kept driving normally with ReasonCode 1 for ~100 s, retired in the pits (FinishState 2, ReasonCode 1). Results text: owner check pending |
| 7 | late-race leader nurse (Brazil 2000) | - | NOT RUN (race lap count unknown: TotalRaceLaps 0x8ACF8C reads 0 in a race; code uses it only in session type 5) |

## 2. Pull-off research (owner question: drivers leave cars mid-track)

- FACT (static, AIAbility 0x429470): the vanilla AI has a lateral target veh+0x4420 that one branch sets to the nearest
  track edge (waypoint widths) - but the branch is gated on carConditions veh+0x3D6C != 2 and other pit/phase state;
  meaning not fully decoded. AIMainUpdate 0x424E70 ZEROES veh+0x4420 every AI tick unless a yield target veh+0x45FC is
  set or carConditions == 2 (line 241). OpponentProximityScan sets +-2.5 m sidesteps (lapping).
- FACT (static): AI mode veh+0x4510: 6 = heading recovery, 7 = brake to stop, 1 = brake then 2 = controls zeroed
  (stuck logic, SteeringAngleController 0x425670 / sub_4258C0) - no "engine failed, pull over" mode found.
- FACT (live): vanilla failures this race (Villeneuve rc 8, Barrichello rc 3) and all our endings end with the car
  moved to the garage row (all retired cars at z = 28.9, y -7.7). Salo (Option A) stopped mid-track and was in the
  garage row ~100 s later without any further write. So the "car left on track" phase = stop -> FinishState 2.
- FACT (live): writing veh+0x4420 alone does nothing (zeroed each tick). With yield target veh+0x45FC = own vehicle
  pointer held, the applied lateral (veh+0x440C) moved only ~1 m and only below ~50 km/h (Panis s23, request +4 m).
  Weak lever; DERIVED that the lateral request is rate-limited in AISteering.
- Do NOT write carConditions 0x3D6C = 2 (limbo trap, finding limbo-unlocked-carconditions).
- Best available realistic path: developing faults -> drive to the pits and retire (scenario 8, wing path +
  ReasonCode 1). Sudden failures stop where they die; the game removes the car to the garage after FinishState 2.

## 3. Consequences for ADDENDUM 15 (proposal, owner decision pending)
- Hopeless-limp backstop is mandatory (R2 costs ~16 s/lap).
- R4 default = scenario 8 path (limp to pits, retire, ReasonCode 1), not Option A on track.
- Sudden endings keep Option A; fire fade-out (bank3 -> 0 after ~10-20 s, keep light smoke).
- Pull-off: no strong native lever found; the yield-target lateral trick gives ~1 m - not worth shipping.

## 4. Open
- Results/commentary text for Magnussen (ReasonCode 0) vs Diniz (ReasonCode 1): owner check.
- Race total laps field for the laps-left rule.
- Exact time from stop to FinishState 2 (garage removal) - measure with a 1 s logger.

## 5. CORRECTION - driver names (2026-10-02, owner-confirmed)
The f1c-memory reader's names are OFF for some slots in this 1998 mod; the DLL-style name read veh+0x4620 is right
(owner saw slot 12 = Alesi blow up). Slot numbers and all measurements above stand; names corrected:
s13 = Barrichello (hopeless limp), s10 = Ralf Schumacher (mild nurse), s9 = Fisichella (pit check),
s15 = Diniz (pull-off coast), s16 = Herbert (limp -> retire ReasonCode 1). Correct as written: s19 Magnussen,
s20 Tuero, s21 Rosset, s22 Takagi, s23 Panis.

## 6. Reruns (owner request)
- Results reason: s18 Trulli wing path ReasonCode 0, s17 Herbert (2nd entry) wing path + ReasonCode 1; both retired
  in the pits (FinishState 2 ~80 s later). Results text: owner check pending.
- Pull aside rerun s22 Takagi, +8 m request, 8 s powered + coast: applied lateral max 0.6 m; owner: steering wheel
  flicked rapidly left-right (AI fighting the request). VERDICT: lever DROPPED.
- Mistake -> blow-up rerun s12 Alesi: R1 30 s, misfire 3 s, blow at 191 km/h; owner saw him retire mid-track.

## 7. Forced pull-off after a sudden failure (owner request, 2026-10-02, same race)
Every car: sudden ending first (exp.py `sudden`: puff, power 0, idle 0, ReasonCode 1, fire banks), then a pull-off
script (`%TEMP%\mf_dyn\steer*.py`, `pulloff*.py`, disposable). Names from veh+0x4620.

| Car (slot) | Method | Result |
|---|---|---|
| Alesi #2 (11) | raw steer 0.15 on 0x442C+0x3580, 3 s, at 113 km/h | path offset 0x4408 1 -> 11.75 m, stopped there; next to Barrichello's stopped car |
| Wurz (8) | raw steer until offset 8 m, then release | owner: AI actively steered back to the track after release (8.8 -> 7.5 m) |
| Frentzen (7) | raw steer holding 12 m to the stop | offset overshot 15 m then swung across to -4 m and back: not controlled |
| Hill (6) | waypoint width (veh+0x4354 -> +52 L/+56 R), 0.9 x nearer side, steer +-0.1, brake 0.35/0.7 (0x4434/0x3584) | 293 -> 0 in 3.5 s at 7-8 m; owner: hit the wall / damaged |
| Hakkinen (2) | same | 6.8 m sideways in 0.5 s at 290 km/h; owner: hit the wall |
| Coulthard (3) | slow first (brake 0.4 to 80 km/h), then 0.6 x width, ramped target, speed-scaled steer | stopped on the line (dead engine drag + brake killed the speed before the drift) |
| Irvine (4) | brake 0.1 to 140 km/h, then the same drift | clean: 0 -> 4.9 m over 2.5 s, stopped at target; owner: better, but on a straight |
| Fisichella (9) | same, in a corner | steering REPLACED the AI's corner steering -> ran wide to 14.5 m; owner: spun, tried to get off |
| Ralf (10) | v2 | not tested (blow-up at 139 km/h, stopped before the script attached; script bug fixed) |
| M. Schumacher (1) | v2: AI steering (0x442C) + small nudge on the actuator 0x3580 only, surface stop rule | AI cancelled the nudge, offset ~0, stopped on the line; owner: he followed the AIW line |

Facts learned:
- Raw steering on 0x442C + 0x3580 in a tight external loop DOES override the AI (FACT), but replacing the AI steering in a
  corner sends the car straight on, and at speed it is a violent swerve (wall contact twice).
- A small additive nudge is cancelled by the AI's closed-loop line following (FACT, Schumacher).
- Surface detection works for AI cars (SDK f1c_surface_contact.h chain: veh+0x3674 + i*0xF8 -> +0x54 -> +0x30 tags;
  running car read 4/4 drivable).
- The waypoint widths (veh+0x4354 -> +52/+56) reach 27 m at a corner: they are not the asphalt edge (likely the
  run-off / wall boundary) - never aim at them; use the surface rule instead.
- The dead engine (power 0 + idle 0) decelerates hard; braking before the drift must stay light (0.1).
Next design (untested): shift the AI's OWN tracked path offset (0x4408/0x440C) as a slow ramp so the AI itself drives a
line toward the side (corners included), stop the ramp when <= 1 wheel is on a drivable surface, then brake firm. Needs a
fresh race (all AI cars out).

## 8. Pull-off v3/v4 (new race, same pid, 2026-10-02)
- v3 Hill (6): ramped 0x4408/0x440C/0x4420 without the yield target - values followed but the AI kept its line;
  owner: tried to steer aside, AI took over, stopped on the racing line.
- v4 Frentzen (7): + yield target 0x45FC = self + corridor 0x4414/0x4418 = +-30 m: drift from 135 km/h, wheels on road
  4 -> 2 -> 0, hold + brake 0.6, stopped off track at 5.1 m. Owner: "seemed to pause normally". FIRST CONTROLLED PULL-OFF.
- Waypoint widths read live: wp+0x2C/0x30 road 2.6-12.1 m, wp+0x34/0x38 wall (always > road), wp+0x58/0x5C up to 30.
System draft: docs/AI_PULLOFF_SYSTEM_SPEC_2026-10-02.md.

## 9. Imola trials - pull-off v5 / v6 with the free-space scanner (2026-10-02, pid 9672)
Scripts (disposable, `%TEMP%\mf_dyn`): pulloff5.py (v4 chain + scanner + traffic wait + lateral-rate abort),
pulloff6.py (goal = all wheels on grass: AIW road edge + 2.1 m, capped at wall free - 0.5 m; SETTLE = gentle
outward steer max 0.05 once the first wheel is off; v6.1: goal reached -> 100 % brake ONLY, no steering/line writes).
| Car (slot) | Version | Result |
|---|---|---|
| Panis (21) | v5 | FALSE abort "lateral rate -90 m/s" (waypoint change + ms loop); stopped on the racing line (owner). Fixed: 0.25 s window, ignore samples across a waypoint change |
| Tuero (18) | v5 | side L (wall free 9.1 vs 7.6), smooth drift to -6.4 m, stop rule "beyond road edge + 1 m" with 4/4 wheels still on drivable material (asphalt run-off), 1.2 m to the wall |
| Diniz (14) | v5 | owner: not aggressive enough; car lagged the line 1-3 m and ran out of speed at -5.9 m; cmd ran to -12.7 (unclamped) - fixed: start 180 km/h, 2-3 m/s, cmd clamped to wall free - 0.5 |
| Trulli (16) | v5.1 | reached -8.1 m (1/4 wheels on road), then during HOLD the AI pulled him back to -6.3 m (owner: AI takes over heavily) |
| Magnussen (17) | - | flipped upside down on his own before any write (script was still waiting for 230 km/h; stopped); game did not retire him - vanilla behaviour, observed |
| Herbert (15) | v6 | owner: drove to the side, vanilla AI resisted heavily but it made it; lat pushed back -5 -> -1.3 m mid-drift, then SETTLE took him to -5.3 m (1/4 wheels on road); after the script ended the AI kept fighting until FinishState 2 |
Facts: the AI's off-track recovery overrides the shifted line once wheels leave the asphalt (Trulli, Herbert); a short
outward steer at < 60 km/h (SETTLE) beats it; after the stop the AI keeps "fighting" a dead car until FinishState 2 -
owner rule: at the goal, 100 % brake only, no steering manipulation (v6.1, untested).

## 10. Imola trials continued (2026-10-02, pids 9672 -> 24536)
| Car (slot) | Version | Result |
|---|---|---|
| Hill (6) | v6.1 (yield target = OWN pointer) | GAME CRASHED 1.5 s into the drift (owner: crash). No Windows event, logs end normally. Prime suspect (DERIVED, one event): yield target veh+0x45FC = the car's own pointer (normally another car). Same write ran on 5 earlier cars without a crash. Changed to the player car pointer; no crash after |
| Takagi (20) | v6.1 (yield target = player) | drove left to -6.4 m, wheels 4/4 on drivable (asphalt run-off), did not reach grass; owner: fighting again |
| Coulthard (3) | v7 (DRIFT, then AI native park mode 4 + 0x45F4=0 during SETTLE/goal) | -6.4 m, 2.0 m to the wall, asphalt run-off; owner: still fighting, kept steering back. The AI speed veh+0x4440 froze at 25 km/h in mode 4 (stale, not physics) so the script never saw the stop; game retired him ~140 s later (garage move) |
Conclusions (FACT unless marked):
- External-process control = two writers on one actuator (AI every 25 ms tick + script loop): visible steering flicker /
  "fighting" in every variant v1-v7. Not solvable from outside (owner decision 2026-10-02: option A, move it into the DLL).
- AI line-following saturates at its usable width (~6.4 m at this Imola spot) whatever the shifted line commands.
- AI mode veh+0x4510 = 4/5: AIMainUpdate skips racing line + steering and writes steer 0 / brake 1.0 / throttle 0 every tick
  (native park, code FACT); it resumes only if the AI-active byte veh+0x45F4 is set (game clears it at retirement).
- veh+0x4440 (speed) is AI-updated; it goes stale in mode 4 - use a physics speed for stop detection (open).
- Waypoint/lateral scanner and surface detection worked in every run.
