# FIELD_GLOSSARY — brain-map vehicle fields (`base = this`)

Scope: every offset in `offsets_ALL.csv` with `base = this` and at least one write across the 20
in-scope functions, sorted by hex. `AIOvertake` is excluded (session-object step, not AI driving;
`REVIEW_3_GROUP2.md` item 2). Writers/readers are function names from the regenerated per-function
CSVs (2026-10-02, decision 1 of `REVIEW_3_GROUP2.md`). Names marked (doc 54)/(inspect)/(prior art) come
from existing records, not from a new tier; every meaning is GUESS until Claude review. `a1`-based
vehicle fields in `AIComputePassScore` (0x4500/0x4504/0x4580/0x453C) live in that map, not here;
0x453C still appears below through its `this`-base writers. The six section-5 functions
(`AIMainUpdate`, `sub_426430`, `sub_426100`, `AISteeringCorrection`, `SteeringAngleController`,
`sub_4258C0`) were mapped by Claude in `docs/54-ai-driving-brain-map.md`; the rest have `map_*.md` here.

| hex | dec | writers | readers | name | GUESS meaning |
|---|---|---|---|---|---|
| 0x34D4 | 13524 | AIRacingLinePathFollowing | AISteeringRacingLineCorrection | position X (pose) | GUESS car world position X; pose block written from the path node, read by racing-line correction. |
| 0x34D8 | 13528 | AIRacingLinePathFollowing | — | position Y (pose) | GUESS car world position Y. |
| 0x34DC | 13532 | AIRacingLinePathFollowing | AISteeringRacingLineCorrection | position Z (pose) | GUESS car world position Z. |
| 0x34E0 | 13536 | AIRacingLinePathFollowing | AIMainUpdate, AISteering, OpponentProximityScanTeamRecognition, SteeringAngleController, sub_426100, sub_426430 | Euler orientation X | GUESS Euler orientation X; written from the pose transform. |
| 0x34E4 | 13540 | AIRacingLinePathFollowing | AISteering, SteeringAngleController | Euler orientation Y | GUESS Euler orientation Y. |
| 0x34E8 | 13544 | AIRacingLinePathFollowing | AIMainUpdate, AISteering, OpponentProximityScanTeamRecognition, SteeringAngleController, sub_426100, sub_426430 | Euler orientation Z | GUESS Euler orientation Z. |
| 0x34F8 | 13560 | AIAbility, AIRacingLine | AISteering, SteeringAngleController, sub_426430 | velocity X | GUESS velocity X; doc 54 speed = length of 0x34F8..0x3500. |
| 0x34FC | 13564 | AIAbility, AIRacingLine | AISteering, sub_426430 | velocity Y | GUESS velocity Y. |
| 0x3500 | 13568 | AIAbility, AIRacingLine | AISteering, BrakeThermalModel, SteeringAngleController, sub_426430 | velocity Z | GUESS velocity Z. |
| 0x357C | 13692 | AIMainUpdate | — | throttle act (output) | GUESS actual throttle copied from 0x4434 at tick end (doc 54 step 11). |
| 0x3580 | 13696 | AIMainUpdate | — | steer act (output) | GUESS actual steering copied from 0x442C at tick end. |
| 0x3584 | 13700 | AIMainUpdate | — | brake act (output) | GUESS actual brake copied from 0x4438 at tick end. |
| 0x3A9C | 15004 | AIRacingLinePathFollowing | — | cached path node pointer | GUESS cached racing-line node pointer used by the path-follow walk. |
| 0x3D70 | 15728 | AIMainUpdate | — | ? AIMainUpdate state | GUESS AIMainUpdate-owned state, not yet named. |
| 0x4028 | 16424 | AIMainUpdate, AISteeringCorrection | — | brake-on-slip output | GUESS traction-control brake output (doc 54 section 3). |
| 0x4068 | 16488 | AIAbility, AIMainUpdate | AIAbility, AIMainUpdate | ? pit request (armed = 6) | GUESS pit-request state armed to 6 before servicing. |
| 0x413C | 16700 | AIMainUpdate | AIMainUpdate | ? (FLT_MAX on both cars, prior art) | GUESS AIMainUpdate-only state (live prior art saw FLT_MAX on both cars). |
| 0x4140 | 16704 | AISteering, sub_426430 | sub_426430 | outside-corridor flag | GUESS set when published steering target leaves 0x4414/0x4418 (corridor) or after collision scan. |
| 0x416C | 16748 | AIMainUpdate | AIMainUpdate | ? (FLT_MAX on both cars, prior art) | GUESS AIMainUpdate-only state (live prior art saw FLT_MAX on both cars). |
| 0x4170 | 16752 | AIMainUpdate | AIMainUpdate | ? (FLT_MAX on both cars, prior art) | GUESS AIMainUpdate-only state (live prior art saw FLT_MAX on both cars). |
| 0x4174 | 16756 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | ? proximity slot 0 base field | GUESS first field of the 68-byte proximity slot 0; exact meaning open. |
| 0x41A8 | 16808 | OpponentProximityScanTeamRecognition | AISteering, OpponentProximityScanTeamRecognition | slot 0 squared distance | GUESS squared distance to the opponent in proximity slot 0 (999999 = empty). |
| 0x41B8 | 16824 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | slot 1 vehicle ptr | GUESS vehicle pointer stored in proximity slot 1. |
| 0x41EC | 16876 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | slot 1 squared distance | GUESS squared distance for proximity slot 1. |
| 0x41FC | 16892 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | slot 2 vehicle ptr | GUESS vehicle pointer in slot 2. |
| 0x4230 | 16944 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | slot 2 squared distance | GUESS squared distance slot 2. |
| 0x4240 | 16960 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | slot 3 vehicle ptr | GUESS vehicle pointer in slot 3. |
| 0x4274 | 17012 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | slot 3 squared distance | GUESS squared distance slot 3. |
| 0x4284 | 17028 | OpponentProximityScanTeamRecognition | — | slot 4 vehicle ptr | GUESS vehicle pointer in slot 4. |
| 0x42B8 | 17080 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | slot 4 squared distance | GUESS squared distance slot 4. |
| 0x42C8 | 17096 | AISteering | AISteering | per-opponent waypoint index | GUESS per-opponent waypoint index used to select the current node. |
| 0x42CC | 17100 | OpponentProximityScanTeamRecognition | — | team record flag (1 = active/team match) | GUESS five team records of 28 bytes; flag says the record matched this car/team. |
| 0x42D4 | 17108 | AISteering | AISteering | per-opponent mode | GUESS pass/overtake mode for this opponent (1/2/3/4/7/8/16). |
| 0x42D8 | 17112 | AISteering | AISteering | per-opponent state | GUESS per-opponent working state compared against 0x4480. |
| 0x42DC | 17116 | AISteering | AISteering | per-opponent accumulator | GUESS per-opponent accumulator scaled by 0.5/0.85. |
| 0x4354 | 17236 | AISteeringRacingLineCorrection, sub_426430 | AIAbility, AIRacingLine, AISteering, SteeringAngleController, sub_426100, sub_426430 | current waypoint ptr | GUESS pointer to the current waypoint (doc 54 locator; re-found when far). |
| 0x4358 | 17240 | AISteeringRacingLineCorrection | sub_426430 | look-ahead waypoint ptr 1 | GUESS first of the 19 look-ahead waypoint pointers. |
| 0x43F4 | 17396 | sub_426430 | — | ? sub_426430 field | GUESS sub_426430 scratch field, not yet named. |
| 0x43F8 | 17400 | AIAbility, AIMainUpdate, AIRacingLine | — | limiting waypoint ptr | GUESS pointer to the waypoint that limits the braking profile. |
| 0x43FC | 17404 | AIRacingLine | — | limiting waypoint target speed (9999 = none) | GUESS target speed of the limiting waypoint (9999 = none). |
| 0x4404 | 17412 | AIMainUpdate | AIMainUpdate | AIAbility timer | GUESS AIAbility timer checked in the retired-car prior art. |
| 0x4408 | 17416 | AISteering | AISteering, sub_426100 | published steering target / racing-line lateral position | GUESS lateral position the steering law follows (live-confirmed 0x4408). |
| 0x440C | 17420 | AISteering | AISteering | working lateral target | GUESS working lateral target before overrides. |
| 0x4414 | 17428 | sub_426430 | AISteering, sub_426430 | corridor bound 1 | GUESS corridor upper/left bound (prior art). |
| 0x4418 | 17432 | sub_426430 | AISteering, sub_426430 | corridor bound 2 | GUESS corridor lower/right bound (prior art). |
| 0x441C | 17436 | sub_426430 | AISteering | ? corridor centre offset | GUESS corridor centre offset (map question; not confirmed). |
| 0x4420 | 17440 | AIAbility, AIMainUpdate, OpponentProximityScanTeamRecognition, Overtaking | AISteering | lateral request | GUESS lateral request; override-enable 0x4548 routes it to 0x4408. |
| 0x4424 | 17444 | AIAbility, AIMainUpdate, AIRacingLine | AIRacingLine, AISteering | hard lateral override | GUESS forced lateral override; wins over 0x4408 writers (AISteering epilogue). AIRacingLine writes half/full of waypoint+24 (L133-L142); AIAbility/AIMainUpdate zero it. Carried question (REVIEW_3_GROUP2): same "forced lateral" meaning? |
| 0x4428 | 17448 | sub_426430 | AIAbility, AIMainUpdate, AIRacingLine, AISteering | lateral position | GUESS measured lateral position from the waypoint frame. |
| 0x442C | 17452 | AIMainUpdate, AISteeringRacingLineCorrection, sub_426100 | AIMainUpdate, AISteeringCorrection, AISteeringRacingLineCorrection, sub_426100 | steer command | GUESS normalized steering command, clamped [-1,1]. |
| 0x4434 | 17460 | AIAbility, AIMainUpdate, AIRacingLine, AISteeringCorrection, AISteeringRacingLineCorrection, sub_4258C0 | AIMainUpdate, AIRacingLine, AISteeringCorrection, AISteeringRacingLineCorrection | throttle candidate | GUESS throttle candidate before post-processing. |
| 0x4438 | 17464 | AIAbility, AIMainUpdate, AIRacingLine, AISteeringCorrection, sub_4258C0 | AIMainUpdate, AIRacingLine, AISteering, AISteeringCorrection | brake candidate | GUESS brake candidate before post-processing. |
| 0x443C | 17468 | AIAbility, AIMainUpdate, AIRacingLine | AIRacingLine | one-shot gap request | GUESS one-shot gap request; persists until AIRacingLine consumes it (prior art). |
| 0x4440 | 17472 | sub_426430 | AIAbility, AIMainUpdate, AIRacingLine, AISteering, AISteeringCorrection, AISteeringRacingLineCorrection, SteeringAngleController, sub_4258C0, sub_426430 | Speed | GUESS car speed (doc 54 FACT). |
| 0x4444 | 17476 | sub_426430 | sub_426430 | ? sub_426430 field | GUESS sub_426430 scratch field. |
| 0x4448 | 17480 | AIMainUpdate | AIMainUpdate, AISteeringRacingLineCorrection | ? time margin | GUESS time margin used by the correction path. |
| 0x444C | 17484 | AIMainUpdate | AIMainUpdate | ? AIMainUpdate field | GUESS AIMainUpdate-owned field. |
| 0x4450 | 17488 | sub_426430 | AISteering, sub_426430 | ? waypoint lateral reference | GUESS waypoint lateral reference. |
| 0x4454 | 17492 | sub_426430 | — | ? sub_426430 field | GUESS sub_426430 scratch field. |
| 0x445C | 17500 | AIRacingLine | AIRacingLine | on-track byte (inside track width +1.5 m) | GUESS car-inside-track-width flag (+1.5 m). |
| 0x4460 | 17504 | AISteeringRacingLineCorrection | AISteeringRacingLineCorrection | ? session-time stamp | GUESS session-time stamp used to rate-limit resets. |
| 0x4464 | 17508 | AISteeringCorrection | AISteeringCorrection | last slip value | GUESS last blended slip value (traction control). |
| 0x4468 | 17512 | AIRacingLine, sub_426430 | AISteering, sub_426100, sub_426430 | look-ahead index (speed rule) | GUESS look-ahead index: 0/1/2/3 by speed thresholds. |
| 0x446C | 17516 | SteeringAngleController | AIRacingLine, AISteeringCorrection, SteeringAngleController | wrong-way counter | GUESS wrong-way watchdog counter (> 0x4558 -> recovery mode). |
| 0x4470 | 17520 | SteeringAngleController | — | saved position X | GUESS saved position X (progress check). |
| 0x4474 | 17524 | SteeringAngleController | — | saved position Y | GUESS saved position Y. |
| 0x4478 | 17528 | SteeringAngleController | — | saved position Z | GUESS saved position Z. |
| 0x447C | 17532 | AIMainUpdate, SteeringAngleController | SteeringAngleController | stuck timer | GUESS stuck watchdog timer (~4 s). |
| 0x4480 | 17536 | AIMainUpdate | AIMainUpdate, AISteering | per-opponent block reset value | GUESS per-opponent state reset value in the part-4 branch. |
| 0x4484 | 17540 | AIMainUpdate | AISteeringCorrection | speed limit (reset to 1e6 each tick) | GUESS pit/speed limit reset to 1e6 each tick; mods write here after step 4. |
| 0x4488 | 17544 | AISteering, sub_426430 | AISteering, sub_426430 | waypoint re-scan flag | GUESS waypoint re-scan flag read by sub_426430 and cleared by AISteering. |
| 0x448C | 17548 | AIMainUpdate | AISteering, OpponentProximityScanTeamRecognition, sub_426100 | heading -sin(yaw) | GUESS heading vector X = -sin(yaw). |
| 0x4494 | 17556 | AIMainUpdate | AISteering, OpponentProximityScanTeamRecognition, sub_426100 | heading -cos(yaw) | GUESS heading vector Z = -cos(yaw). |
| 0x4498 | 17560 | sub_426430 | sub_426100 | anchor X | GUESS anchor point X at waypoint change. |
| 0x44A0 | 17568 | sub_426430 | sub_426100 | anchor Z | GUESS anchor point Z at waypoint change. |
| 0x44A4 | 17572 | sub_426100 | AISteering, sub_426100 | distance since waypoint change | GUESS distance driven since the last waypoint change. |
| 0x44B0 | 17584 | AIAbility | — | NAN sentinel | GUESS NAN sentinel written every tick by AIAbility. |
| 0x4508 | 17672 | AIAbility, AISteering | AIAbility, AISteering | ? AIAbility timer/interval | GUESS AIAbility interval/timer (0x4508). |
| 0x450C | 17676 | AIAbility, Overtaking | AIAbility, Overtaking | shared speed memory | GUESS shared speed memory (kept as max of both cars' 0x44C4). |
| 0x4510 | 17680 | AIAbility, AIMainUpdate, AIRacingLine, AISteeringRacingLineCorrection, SteeringAngleController, sub_4258C0 | AIMainUpdate, PhysicsGearShiftLogicWithThresholds, SteeringAngleController, sub_426100 | driverStatus / mode | GUESS driver/session mode: 0 race, 1 recovery, 2 stopped, 3 pit approach, 4/5 pit wait, 6 reverse, 7 stop. |
| 0x4524 | 17700 | AIAbility, AIMainUpdate, AIRacingLine | AIRacingLine | ? (pit cleanup) | GUESS pit-cleanup field zeroed on release. |
| 0x4528 | 17704 | sub_426430 | — | ? sub_426430 field | GUESS sub_426430 scratch field. |
| 0x452C | 17708 | sub_426430 | — | ? sub_426430 field | GUESS sub_426430 scratch field. |
| 0x4534 | 17716 | AIAbility, AIMainUpdate | AIMainUpdate | ? time stamp | GUESS time stamp written before pit servicing. |
| 0x4538 | 17720 | AISteeringRacingLineCorrection | AISteeringCorrection | maxSpeed | GUESS speed governor target (inspect CONFIRMED; pace mod writes here). |
| 0x453C | 17724 | AIAbility | AIAbility | AI pace multiplier | GUESS dynamic pace/ability multiplier, +/-0.01 per tick (prior art CONFIRMED). |
| 0x4544 | 17732 | AIAbility | AISteering | ? pit/ability flag | GUESS pit/ability flag set to 1 then cleared by command 15. |
| 0x4548 | 17736 | AIAbility | AISteering | override-enable for request 0x4420 | GUESS override enable: AISteering uses request 0x4420 directly when non-zero. |
| 0x454C | 17740 | AIAbility, AIMainUpdate, AIRacingLine | AIRacingLine, AISteering, sub_426430 | branch id latch | GUESS branch id matched against wp+276 / zeroed on pit release. |
| 0x4550 | 17744 | AIAbility, AIMainUpdate, AIRacingLine, AISteeringRacingLineCorrection, sub_426430 | AIMainUpdate, AIRacingLine, AISteering, sub_426430 | laneMode / waypoint set selector | GUESS laneMode / waypoint-set selector; command 15 sets 1. |
| 0x4554 | 17748 | AISteering | AIRacingLine | ? tick latch (set to 1) | GUESS tick latch set to 1 by AISteering init. |
| 0x455C | 17756 | AISteering | AISteering | ? counter incremented | GUESS counter incremented when an opponent is almost alongside. |
| 0x4560 | 17760 | AIMainUpdate | AIMainUpdate | ? AIMainUpdate field | GUESS AIMainUpdate field, not named. |
| 0x4564 | 17764 | AIMainUpdate | AIMainUpdate | ? AIMainUpdate field | GUESS AIMainUpdate field, not named. |
| 0x4568 | 17768 | AIMainUpdate | AIMainUpdate | ? AIMainUpdate field | GUESS AIMainUpdate field, not named. |
| 0x456C | 17772 | AISteering | AISteering | ? avoidance/line-follow latch | GUESS avoidance/line-follow latch. |
| 0x4570 | 17776 | AISteering | AISteering | ? opponent id + 1 | GUESS stored opponent id + 1. |
| 0x4574 | 17780 | AISteering | AISteering | ? opponent id + 1 | GUESS stored opponent id + 1. |
| 0x4578 | 17784 | OpponentProximityScanTeamRecognition | OpponentProximityScanTeamRecognition | team-scan reset flag | GUESS team-scan reset flag (inspect). |
| 0x4579 | 17785 | AIAbility, AISteering | AISteering | ? flag | GUESS small flag zeroed by AIAbility and read by AISteering. |
| 0x457C | 17788 | AIAbility, AISteeringCorrection | AISteeringCorrection | difficulty cap | GUESS difficulty cap = AIDifficultyScalar() * 0.75. |
| 0x45F4 | 17908 | AIAbility, AIMainUpdate, AIRacingLine, AISteeringRacingLineCorrection | AIMainUpdate | ActiveFlag | GUESS ActiveFlag cleared on pit servicing / racing-line reset. |
| 0x45FC | 17916 | AIMainUpdate, OpponentProximityScanTeamRecognition, Overtaking | AIMainUpdate, AISteeringCorrection | interaction target | GUESS interaction target for Overtaking (prior art S1). |
| 0x461C | 17948 | AIMainUpdate | AIMainUpdate | ? AIMainUpdate field | GUESS AIMainUpdate-owned field. |

Rows: 104 (from 193 distinct `base=this` offsets touched by the in-scope functions).
