# Investigation brief #2 - vanilla features for every NON-engine retirement reason (2026-09-28)

**For:** a separate (cheaper) LLM worker. **Type:** static research only.
**Owner:** Matias. **Product:** Mechanical Failures
(`development\mods\mechanical_failures\`). Companion to brief #1
(`VANILLA_FEATURES_INVESTIGATION_BRIEF_2026-09-28.md`, engine) and its result
(`VANILLA_FEATURES_INVESTIGATION_RESULT_2026-09-28.md`) - **read both first
and do not repeat their answers.**

## 1. Why

The game's retirement categories (`FailureCategoryNameLookup_004bb730`,
brief #1 Q11): 1 Engine, 2 Gearbox, 3/7 Clutch, 4 Suspension, 5 Brakes,
6 Accident, 8 Electronics, 9 Fuel. Our mod covers engine (11 types), gearbox
(lost/stuck/false neutral/no drive + stuck-gear stall), suspension
(handling problem/part broken/collapse), electronics (misfires/limp/radio/ECU
cut-out). **Clutch and brakes have no mod family; tyres/wheels, hydraulics,
steering, driveshaft/differential are not modelled.** We want each failure
to use vanilla game features so it feels real and different, and to retire
with the right category.

## 2. Rules (hard) - same as brief #1

Read-only static research; no live writes, no injection, no edits except the
result file. Never read `api1.txt` or private keys; nothing is deleted.
Tiers FACT / CONFIRMED / DERIVED / GUESS on every claim, cited as
`functions/<Name>_<addr>.c:<line>` or a findings id. Front door:
`python tools/f1c_mod.py start --json`, `python tools/ask.py findings|func|global|offset|token`
from `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled`. No broad `rg` from the
workspace root.

## 3. Known starting points (do not re-derive)

- Player physics `this = 0x7C7428`; vehicle array `0x5EC6D8`, stride
  `0x48C0`, slot 0 player; ReasonCode `veh+0x4068`, FinishState `veh+0x4064`,
  player retirement = FinishState 2 + `0x7CE258 = 1` (`retire_gearbox()` in
  `dll/gear_failure.c`).
- Gearbox: player SetGear `0x447E00` detour (`dll/gear_failure.c`).
- Suspension: `SteeringGeometryUpdate_00442060(0x7C7428, wheel, severity)`
  from a game-thread detour (sandbox `suspension_failure.c`); per-wheel
  record base `0x7C7428`, stride 1124 (`docs/SUSPENSION_DAMAGE.md`).
- Brakes: vanilla brake wear `0x438690` -> ReasonCode 5 reaches the player
  (ENGINE_FAILURE.md §7); AI brake lifetime `veh+0x3D74` (unused by us); HDV
  brake block candidates in `docs/HDV_MECHANICAL_LINK_INVENTORY_2026-09-28.md`.
- Tyre/brake block `0x7C7A00` = same wheel array (`0x7C7428 + 1496`);
  tread crossing 0.75 fires puncture lines eid 78/79/102/103
  (`EMITTER_SPECS.md`); PWS_MECHPROB18 = "overheated brakes" (eid 74).
- `damage.ini [PHYSICAL]`: Engine / Aero / Wing / Wheel / Part impulse model.

## 4. Questions

### Brakes (ReasonCode 5) - highest priority, family pending
| # | Question |
|---|---|
| B1 | Full vanilla brake model for the player: temperature, wear, fade, disc/pad fields, which `.hdv` tokens seed them, and what `0x438690` does at the limit. Which fields are writable / recalculated? |
| B2 | Can we make one corner lose braking (caliper/line failure), brakes fade (overheat), or the pedal go long (fluid loss), using vanilla fields? Where does brake torque per wheel get computed? |
| B3 | Brake balance: is the bias a live field (driver adjustable in-car?) - a "rear brakes lost" failure could move it. |
| B4 | Brake radio/dash: eid 74 MECHPROB18 emitter conditions; any brake warning or glowing-disc visual (emitter / material)? |

### Clutch (ReasonCodes 3 and 7)
| # | Question |
|---|---|
| C1 | Why two clutch codes (3 vs 7)? What sets each for the player/AI? |
| C2 | Player clutch model: engagement, slip, anti-stall, launch; fields and writers. Can we make the clutch slip (revs flare, no drive) or fail to disengage (stall on stop)? |
| C3 | Is there a vanilla stall mechanic (brief #1 Q12 "stall trip") and how does it relate to the clutch? |

### Tyres / wheels (retire as 6 Accident or 4 Suspension?)
| # | Question |
|---|---|
| T1 | Vanilla puncture / deflation model for the player: pressure, tread, blowout, detached wheel (`damage.ini Wheel`, "detach flags" in SUSPENSION_DAMAGE.md). Can a DLL cause a slow puncture, a blowout, or a wheel falling off (loose wheel nut)? Which ReasonCode results? |
| T2 | Tyre temperature/pressure fields per wheel (0x7C7A00 block): writable? Would raising pressure/temperature give a native grip change? |
| T3 | Visuals/sounds: tyre smoke, flat-tyre wobble, sparks from a rim (`Reaction=spark Max=0` in terrain.ini), detached-wheel object. |

### Hydraulics / steering / driveline
| # | Question |
|---|---|
| H1 | Is there any hydraulic system model (pressure) behind gear/clutch/throttle/diff? If not, which levers emulate a total hydraulic failure (stuck gear + clutch + throttle together)? |
| H2 | Steering: power steering, steering rack, per-wheel toe from suspension bends - can a steering failure (heavy steering, offset, no steering) be caused through vanilla fields? |
| H3 | Differential / driveshaft: diff lock/preload fields (setup) and whether a live change gives a native "diff failure" (car snaps, one wheel spinning); driveshaft = our gearbox NO DRIVE - any better lever? |

### Existing families - better vanilla use
| # | Question |
|---|---|
| G1 | Gearbox: gearbox temperature/wear exist for the player? Gear-change sound slots (brief #1 Q1 sound path) for a grinding/crunching gear? |
| S1 | Suspension: damper/spring/ride-height live fields (setup) - could a "damper failure" (bouncing, bottoming) use them instead of bending geometry? |
| E1 | Electronics: traction control / launch control / pit limiter / engine map switches in vanilla - can a failure disable TC, force the pit limiter on, or lock a map? |
| A1 | Aero / accident: `damage.ini Aero/Wing/Part` - can a DLL detach a wing endplate / bodywork piece (mechanical failure, not collision) and does the game then apply its own aero loss? |

### Cross-cutting
| # | Question |
|---|---|
| X1 | Retirement text/results: exact text per ReasonCode on the results screen and in commentary; which radio/TV lines exist per category (PWS < 291, TV > 292), blocked or not. |
| X2 | Does the game let a car with a non-engine failure keep running to the pits and retire there (vs stopping on track)? What decides where the car ends up? |
| X3 | Marshal / flag behaviour for a car stopped on track with each failure (yellow flag zones, `sub_4D1730`), and for a car dropping fluid (any "oil flag"). |

## 5. Deliverable

One file:
`development\mods\mechanical_failures\docs\RETIREMENT_REASONS_INVESTIGATION_RESULT_<date>.md`.
Per question: answer, tier, citations, **how a DLL would use it** (address,
call/write, thread, safety risk), effort/risk. Then:
1. a table **per retirement category**: failure types that are realistic and
   buildable, the vanilla levers each would use, and the ReasonCode it ends
   with;
2. a ranked "biggest player-feel gain for least risk" table;
3. OPEN items, each with the exact live test that would settle it.
Do not implement anything. Do not edit the README (the owner links it).

## 6. Acceptance

All questions answered or OPEN with a live test; every number tiered and
cited; only the result file changed; workspace validator unchanged
(`tools\workspace\check-workspace-structure.ps1`, 11 pre-existing errors).
