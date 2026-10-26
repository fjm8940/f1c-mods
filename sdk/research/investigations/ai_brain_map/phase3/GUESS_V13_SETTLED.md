# GUESS_V13_SETTLED -- E3: static settling of the v1.3 GUESS fields

Date: 2026-10-02. Author: DeepSeek V4.1 (BATCH C, offline). Task: handoff BATCH C item 3.
Input: `ai_brain_abi_v1_3.json` (36 GUESS fields), D15 actions in `phase3/GUESS_V12_NEXT.md`.
Method: `phase3/analysis/e3_guess_static.py` (capstone dump per offset: all memory operands with
that displacement + all `functions/*.c` references as byte/decimal/hex/dword index) and targeted

Result: 32 of 36 fields get a static meaning and a GUESS -> DERIVED proposal; 4 stay GUESS
(partial evidence). No live-only field remains; the residual live checks are in section 37.
`abi_v1_4_proposal.json` carries the machine-readable entries for Claude.

Legend: W = writer, R = reader; addresses are capstone instruction addresses in the exe.
Reminder (D10): Hex-Rays `this[N]` = veh + 4N, `this + N` = veh + N bytes.

## 1-3. placement_pose_x / y / z (0x34D4 / 0x34D8 / 0x34DC)

- Static: W `VehicleSetupInitialization` 0x4E49DE/F0/FC (from setup data), `VehicleSetupDataCopy`
  0x4F1650 L5-7, `sub_4E5AE0` 0x4E5BAE/BE/CE (`+= transform`), `AIVehiclePathPositionTracking`
  0x4EF891/8A1/8AD (`+=` path-step, L33-41), `AIRacingLinePathFollowing` L96-98 (`this[3381..3383]
  = path node position`), `VehicleYawRotationRecoveryController` L91-94. Readers: physics, collision,
  particle and steering transforms use them as a world translation (e.g. `F_3x3MatrixTransformWithTranslation`
  L6-8, `DistanceProjectionCalc` L26-28).
- Meaning: body/placement world position vector (x,y,z) -- the physics/collision reference point,
  distinct from position_x/y/z 0x34E0 (AI log position). Corrects the v1.2 "direction-like" note:
  D3.2 sampled parked/placed cars; every writer is a position integrator or a setup/path copy.
- Tier proposal: DERIVED (writer + multi-system reader identified).

## 4. opp_slot_vehicle_ptr (0x4174)

- Static: W `OpponentProximityScanTeamRecognition` 0x42A9FE `mov [esi+0x4174], edx` with
  `edx = dword_7C54F0[edi]` (the vehicle pointer array); init 0x42A82F. R `AISteering` many sites
  (`[edi+0x4174]` = slot base of the 5 x 0x44 opponent slot array, slots at 0x4174/0x41B8/0x41FC/
  0x4230/0x4274; slot 5 at 0x42B8 is the interaction candidate used by Overtaking).
- Meaning: opponent slot array base; holds the detected opponent vehicle pointer (null/0 when none).
- Tier proposal: DERIVED. (D15's live identity check is no longer required; the writer stores the
  vehicle pointer directly.)

## 5. opp_slot_distance_sq (0x41A8)

- Static: W `OpponentProximityScanTeamRecognition` 0x42A9F1 `mov [esi+0x41a8], ecx` (ecx = planar
  squared distance computed just above), init 1.0 at 0x42A823; R `AISteering` 0x427769
  (`fld [edi+esi+0x41a8]`, slot-strided), 0x42A980 (`fcomp [esi+0x41a8]`).
- Meaning: squared planar distance (m^2) to the slot-0 opponent.
- Tier proposal: DERIVED. (D15's live squared-vs-planar ratio check stays possible but is no longer
  needed to settle the meaning.)

## 6. opp_team_threshold (0x42DC)

- Static: per-team 28-byte records at 0x42CC (flag, team id, mode, state, threshold); W reset 0
  `OpponentProximityScanTeamRecognition` 0x42AC0D; R/W `AISteering` 0x427CED..0x427D77 and
  0x428BA8/0x4291F8: grows by `speed * track_aggression_effective (0x44F8)`, clamped at 450.0;
  compared as `500.0 - value >= distance` (0x428950/0x428968 region) for the overtake margin.
- Meaning: per-team overtaking margin accumulator (m), starts 0, grows with the effective track
  aggression, capped at 450; the candidate gate compares distance against `500 - value`.
  Corrects D15's "reset to 500" (500 is the compare constant; the reset value is 0).
- Tier proposal: DERIVED.

## 7. opp_state (0x42D8)

- Static: W -1.0 (0xBF800000) in `AISteering` 0x4275F8 and 0x428424 (team record at +0xC);
  W by `OpponentProximityScanTeamRecognition` 0x42ABFD (team loop); R `AISteering` 0x428442/0x428468
  compares against `this[4384]` = 0x4480 (session-time stamp, finding unnamed_4480); R 0x4284xx.
- Meaning: per-team/opponent state timestamp referenced to session time; -1.0 = unset. Not a mode.
- Tier proposal: DERIVED.

## 8. unnamed_4000 (0x4000)

- Static: R `AIAbility` 0x429670 (`this[4096] <= 0.0` pit/command gate) and 0x42977D
  (`> 0.0` -> sets `this[4122] = 6`, command 6); physics step `SuspensionAndWheelPhysics` L1240
  zeroes `veh+0x4000` together with the 0x3FF0..0x4010 contact/pose accumulator block. No AI-set
  writer of a nonzero value found (the `sub_4F1FE0` write is a pose copy on an entry object, not
  the race vehicle).
- Meaning candidate: per-tick contact/request accumulator consumed by AIAbility (nonzero -> command
  6; <=0 opens the command/pit path). Not settled -- the producer of a nonzero value is outside the
  mapped set.
- Tier proposal: GUESS (meaning refined, evidence recorded).

## 9. unnamed_4040 (0x4040)

- Static: R `AIAbility` 0x4296FD (`*(this+4112) != 2 && != -1` race-end branch condition);
  R `FuelWeightLimitCheck` L4 (`(total_laps - lap) - *(this+0x4040)/3 < fuel autonomy - 0.35`, a
  fuel-range check); R `PitStopOperationTimer` 0x4A641B (`== 2`); R `AIPitStrategyState` L12;
  R commentary/standings functions `sub_49BE50`/`sub_49BFF0`/`sub_49C0B0`/`sub_49C140`/`sub_49C280`
  (all via `dword_7C54E0` = player vehicle); R `sub_4A6D50`.
- Meaning candidate: integer pit/fuel strategy state (3x-scaled fuel/stop accounting, value 2 during
  a pit operation, -1 sentinel); exact identity still open.
- Tier proposal: GUESS.

## 10. unnamed_413c (0x413C)

- Static: init FLT_MAX (0x7F7FFFFF) in `AIDriverStructInit` 0x424A69, `CameraViewportFrustumInitialization`
  0x424B28, `RaceStructInit`; W `AICommandMessageQueueDispatcher` 0x424D23 `fstp [esi+0x413c]` after
  `fadd [0x59e800]`, with L20 `this[4175] = dbl_8AFEA0 + 2.0` for command 1; R `AIMainUpdate` 0x42521F
  (`fcomp` with session time) then 0x425230 resets FLT_MAX and calls `AIPushLap`.
- Meaning: AIPushLap trigger deadline -- set to now + 2.0 by AI command 1, consumed by AIMainUpdate
  (`now > deadline` -> AIPushLap, then FLT_MAX = disarmed). Corrects D15's "corner-clamp sentinel".
- Tier proposal: DERIVED.

## 11. unnamed_4170 (0x4170)

- Static: W `AIMainUpdate` 0x424F90 `fstp [esi+0x4170]` after `fadd [0x59e5fc]`, and L89 writes
  `this+16752 = (rand & 0x14) + now + 10.0`; R `AIMainUpdate` 0x4252F7/L218 (`value < now` triggers
  the re-plan/limbo-reset block) and `sub_4F1FE0` L85 (`< FLT_MAX`); init 0 (RaceStructInit),
  FLT_MAX at 0x425374.
- Meaning: AI re-plan/lane-change deadline (session time; set to now + 10 + {0,4,16,20} s,
  disarmed to FLT_MAX when consumed). D15's "start-phase timer" is wrong for this site.
- Tier proposal: DERIVED.

## 12. outside_corridor_flag (0x4140)

- Static: W 1 `AISteering` 0x429457 (epilogue); W 0 `sub_426430` 0x426C82/0x426CA0 and init 0 in
  `AIDriverStructInit` L23, `RaceStructInit` L11; R `sub_426430` 0x426569 (L177 branch) and the
  subsequent corridor handling.
- Meaning: off-corridor / line-reacquisition marker set by AISteering when the car leaves the
  planned corridor, cleared by the locator when it reacquires.
- Tier proposal: DERIVED.

## 13. unnamed_4364 (0x4364)

- Static: R `sub_426430` 0x426866 `mov eax,[esi+0x4364]`, L200 dereferences it and compares
  `node->0xB8` against the pointer held at 0x434C; D3.2 live values are waypoint-node addresses
  (0x31B0....), 1175 distinct. No direct writer found in the capstone scan (written through a base
  the scan cannot resolve, or by the locator via an alias).
- Meaning candidate: waypoint-node pointer scratch in the locator (compared with the 0x434C node).
- Tier proposal: GUESS (meaning refined).

## 14. unnamed_43f4 (0x43F4)

- Static: W `sub_426430` 0x426784 `fstp [esi+0x43f4]` immediately after `fsqrt`; L175 writes
  `*(this+17396) = sqrt(dx*dx + dy*dy)`; init 0 in `RaceStructInit` L14.
- Meaning: locator segment length (sqrt of the planar node delta), used by the chain test.
- Tier proposal: DERIVED.

## 15. unnamed_4400 (0x4400)

- Static: W `AIDriverStructInit` L120 `this[4352] = SpatialTreeNearestNeighborSearch(dword_5EA2C0,
  v16, 0, this[2], 0)` (nearest path node at struct init), init 0 in `RaceStructInit` L8;
  R `AIRacingLine` 0x4259AF/0x4259D5/0x425A56/0x425B47 (used as the arming object for 0x4524).
- Meaning: cached current/nearest racing-line node pointer (set once at AI struct init from the
  spatial-tree search; read by AIRacingLine). D3.2's constant-per-car pointer matches.
- Tier proposal: DERIVED. (D15's live chain-identity check becomes optional.)

## 16. unnamed_441c (0x441C)

- Static: W `sub_426430` 0x426B8A (`mov [esi+0x441c], ecx`) and 0x426BB6 `fstp`; L298-300 writes a
  rational interpolation: `(v63*k + v62 - v61*k - v64) / (v66 - v65*k)` with `k = v47/v46`; R
  `AISteering` 0x42713C/0x427174/0x4272A0/0x4272EF (added to / multiplied into steering terms) and
  0x428CAD/0x428CF1 (comparisons).
- Meaning candidate: locator-to-steering correction term (a rational interpolation of waypoint
  fields; units unclear). Static read done; the exact physical quantity is not settled.
- Tier proposal: GUESS (meaning refined, formula recorded).

## 17. unnamed_4444 (0x4444)

- Static: W `sub_426430`: 0x4266A1 `fst [esi+0x4444]` after `fdivr [esi+0x4440]` (speed / divisor,
  L147) and 1.0 at 0x4266B4 when the divisor branch fails (L149); init 1.0 (RaceStructInit L41).
  R `AISteering` 0x428023/0x42818D/0x428630/0x428693 and L875-878 `(1.0 - v) * 300` steering-gain
  blend; used in the `(1.0 - v)*0.15 + node_table` term (L192).
- Meaning: normalized speed/segment ratio (0..1), blends the steering gain and the node-table
  interpolation. Functionally a "speed ratio" scratch.
- Tier proposal: DERIVED.

## 18. time_margin (0x4448)

- Static: W `AIRingBufferFieldInit` L8 `this[4370] = rand_table[i]*0.15 + 0.15 - race_dist_pct*0.01*0.1`;
  also W `PairArrayLinearSearch` 0x424E69 (`fstp`), R/W `AIMainUpdate` 0x424EDA/0x424EE6
  (`-` a per-tick decrement candidate); R `AISteeringRacingLineCorrection` 0x42B35B L119
  (`deadline + 1.0 + value*20.0 + session_start`) used in the lap-projection test.
- Meaning: lap-time margin/allowance term (start 0.15..0.30 s randomized, minus a race-distance
  offset, then updated in the AI update); consumed in lap-time projections (x20 window).
- Tier proposal: DERIVED.

## 19. unnamed_444c (0x444C)

- Static: W `AIMainUpdate` 0x424F1E `mov [esi+0x444c], eax` where `eax = dword_7C56A8` (race phase)
  at a phase change (L50-66); init 0 (AIDriverStructInit L51, RaceStructInit L33,
  CameraViewportFrustumInitialization L56); R `AIMainUpdate` 0x424EBA and 0x424FE6 (`== 4`).
- Meaning: cached race phase (mirror of dword_7C56A8, refreshed on phase change); live values {2,4,5}
  match race-phase codes; 4 = grid/formation phase.
- Tier proposal: DERIVED.

## 20. unnamed_4454 (0x4454)

- Static: W `sub_426430` 0x4266D0 `fstp [esi+0x4454]` after `fadd [esi+0x4450]`; L150
  `*(this+17492) = (int)*(this+17496) * flt_5EA3FC + *(this+17488)` = counter * segment length +
  track_part_from_pit_exit; init 0 (RaceStructInit L35), `CameraViewportFrustumInitialization` L39
  (`v6 * 7.0`).
- Meaning: cumulative distance from pit exit (m) = 0x4450 + 0x4458 * segment length. Confirms the
  D3.2 r=0.95 distance integral result and the D2 meaning correction.
- Tier proposal: DERIVED.

## 21. unnamed_4458 (0x4458)

- Static: RW `sub_426430` 0x426552 `inc [esi+0x4458]` (L103) and 0x4266BE `fild [esi+0x4458]`
  (feeds 0x4454); init -1 (AIDriverStructInit L76 `this[4374] =-1`; CameraViewportFrustumInitialization
  L33).
- Meaning: segment/lap counter since pit exit (integer) used with the segment length to build 0x4454.
- Tier proposal: DERIVED.

## 22. unnamed_4460 (0x4460)

- Static: RW `AISteeringRacingLineCorrection` 0x42B2FD `fsub [esi+0x4460]`, 0x42B3E8
  `fstp [esi+0x4460]` with L115 `fabs(now - this[4376]) > 10.0` and L136 write `this[4376] = now`;
  init -1.0 (AIDriverStructInit L49, CameraViewportFrustumInitialization L30).
- Meaning: session-time stamp of the last racing-line-correction run (10 s cooldown gate).
- Tier proposal: DERIVED.

## 23. unnamed_4490 (0x4490)

- Static: W 0 (RaceStructInit L52); R `AISteering` (map_AISteering_part2.md L335, heading Y:
  heading_x 0x448C / heading_y 0x4490 / heading_z 0x4494 are the heading vector; registry already
  has heading_x and heading_z as DERIVED).
- Meaning: heading vector Y (vertical) component -- the missing middle element of 0x448C/0x4490/0x4494.
- Tier proposal: DERIVED. Caveat: `TerrainSurfaceRoughnessHeightSampling` 0x45CA20 writes per-wheel
  surface-roughness floats at `this[4*i+4388]` (0x4490 + 16*i) on a different object; that is a
  separate per-wheel array, do not confuse it with the vehicle heading vector.

## 24. command_queue_write_index (0x44B0)

- Static: W `AICommandMessageQueueDispatcher` 0x424DD5/0x424DDF (`v6 + 1`), 0x424E0A (-1),
  0x42994C (-1); R 0x424DAD and `PairArrayLinearSearch` 0x424E30 `this[4396]`; init -1
  (AIDriverStructInit L77/L101, RaceStructInit L62). Finding `f1c-command-queue-is-per-car-not-global-ring`
  (CONFIRMED) defines the semantics.
- Meaning: per-car AI command queue write index (-1 = empty; incremented while filling; reset to -1
  after the response handler runs).
- Tier proposal: DERIVED (static + CONFIRMED finding). Live activity values still unverified (R1
  closed the type only); a watcher restart would confirm the -1/indices over time.

## 25. unnamed_4524 (0x4524)

- Static: W/R `AIRacingLine` 0x425B1C (`fld`), 0x425C43 (`fcom`), 0x425C74 (`fst`),
  0x425D2B (=0), plus L83/103/111/155/174/187; map_AIRacingLine.md L80-196: armed with the distance
  to the object cached at 0x4400 when approaching it, re-measured while set, cleared on pass/reset;
  init 0 (AIDriverStructInit L48, CameraViewportFrustumInitialization L29).
- Meaning: AIRacingLine armed-distance state (m) to the lead/waypoint object at 0x4400; 0 = unarmed.
- Tier proposal: DERIVED.

## 26-27. unnamed_4528 / unnamed_452c (0x4528 / 0x452C)

- Static: W `sub_426430` 0x4267DD/0x4267E5 (`mov [esi+0x452c/0x4528], eax` from the just-computed
  fmul/fxch pair), 0x4268B7/0x4268D0 (-1.0 = 0xBF800000), 0x42928B0 region (L189/190 +1.0);
  R `AISteering` 0x428643 `fld [ecx+0x4528]`, 0x428664 `fld [ecx+0x452c]` and 0x428Cxx comparisons.
- Meaning: locator-to-steering sign scratch pair (+1.0 / -1.0 only in the D3.2 logs), carrying the
  side/direction of the locator interpolation into the steering decision. D2's "sign scratch"
  correction is confirmed; the sign convention (which side) still needs a live/battle correlation.
- Tier proposal: DERIVED (pair).

## 28. unnamed_4554 (0x4554)

- Static: W 1.0 `AIDriverStructInit` L56 (`this[4437] = 1065353216`), `CameraViewportFrustumInitialization`
  L45, `AISteering` 0x4273A1; R `AIRacingLine` 0x425DC1 `fmul [esi+0x4554]` on the AIComputePassScore
  result (the 0x43FC corner-target chain; D1).
- Meaning: per-corner speed-target multiplier (1.0 = neutral). Only init/reset writes found; no
  code path writes a non-1.0 value in the corpus, matching the constant 1.0 live logs.
- Tier proposal: DERIVED.

## 29. unnamed_455c (0x455C)

- Static: RW `AISteering` many: init 0 (AIDriverStructInit L67), `inc [esi+0x455c]` at
  0x427F12/0x428E15/0x428F08/0x42914E/0x42918E/0x4291C7/0x4292F4/0x429334/0x42936C,
  `mov [esi+0x455c],eax` at 0x427DFC (copy candidate's value), comparisons `==4`, `>=4`, `>=2`, `<=2`.
- Meaning: AI attack/battle phase counter on the vehicle (0..~6), advanced by the AISteering
  interaction branches and resettable from the interaction candidate; D6's battle correlation
  (2 attack / 3 approach) matches a phase counter.
- Tier proposal: DERIVED.

## 30. unnamed_4560 (0x4560)

- Static: W `AIMainUpdate` 0x425164 `fstp [esi+0x4560]` (L120, writes session time after use);
  R 0x4250E1 `fsub [esi+0x4560]` (L107 `now - value`, capped to 1.0); init 0 (AIDriverStructInit
  L72, CameraViewportFrustumInitialization L48).
- Meaning: previous-event session-time stamp used to compute a capped delta-time (dt).
- Tier proposal: DERIVED.

## 31. unnamed_456c (0x456C)

- Static: W `AISteering` 0x4283BC `mov byte [esi+0x456c],1`, 0x4273AB `mov byte [esi+0x456c],al`
  (init/clear); R 0x4270B4/0x428771/0x428824 on self and 0x4282BB on the interaction candidate
  (`*((_DWORD*)v33+4189)` = 0x4174 opponent pointer, +17772 = candidate's 0x456C); prior art
  (`map_AISteering_part1.md` L194/299, part3/4) names it an avoidance/line-follow latch.
- Meaning: per-car avoidance/line-follow latch (byte); set when AISteering commits to the avoidance
  line, cleared on init/exit; read on the interaction candidate to coordinate the pass.
- Tier proposal: DERIVED.

## 32-33. unnamed_4570 / unnamed_4574 (0x4570 / 0x4574)

- Static: W/R `AISteering` 0x427E39/0x427E9A/0x427F5F/0x4280C9 (`mov [esi+0x4570/74], ecx`
  with `ecx = [candidate+0x4570/74] + 1` where candidate = `[edi+0x4174]`), 0x42738F/0x427395
  (both 0 on the reset path); prior art (`map_AISteering_part1.md` L45/46, part3 L47/48) reads
  "opponent id + 1".
- Meaning: interaction/avoidance opponent id + 1, one per side (0x4570 = side A, 0x4574 = side B);
  0 = no opponent recorded; copied from the candidate's counter +1 on a sustained interaction.
- Tier proposal: DERIVED (pair).

## 34. unnamed_4590 (0x4590)

- Static: W `sub_429960` 0x429C6C `mov [esi+0x4590], eax` (plan build), 0x429E1C/0x429E22
  (`this[4452] = dword_8ACF8C` = total laps when no plan; loop over `4*(i+1113)` rows);
  R 0x429AA4/0x429D28/0x429DDB/0x429E0B; used by `AIPushLap` L40 and `PitStopDecisionTireCompound`
  L37 as `this[4*cursor+4452]` (next-stop lap).
- Meaning: base of the per-stop lap target array (5 x 16-byte rows starting at 0x4590; first row =
  `this[4452]`, row fields: lap, gap/time, compound flags); holds total laps when no plan is active.
- Tier proposal: DERIVED.

## 35. unnamed_45e8 (0x45E8)

- Static: W `sub_429960` 0x4299D2/0x429A10 `fst/fstp [esi+0x45e8]` (L60-63: `v5 = this[3684] *
  flt_5EA40C`, min 1.0, raised to `dword_7D0B64 * 1.2`); R as a multiplier at 0x429C38, 0x429CF7,
  0x429D47, 0x429DF8, 0x429E2E (plan times/gaps), `PitStopDecisionTireCompound` 0x42A590,
  `AISteeringRacingLineCorrection` L127 (`this[4474] * 1.5 + this[3681]`).
- Meaning: pit-strategy lap-time scale (seconds per lap) -- derived from the vehicle performance
  value and the race length; multiplies the stop gaps and plan times.
- Tier proposal: DERIVED.

## 36. unnamed_461c (0x461C)

- Static: W `AIMainUpdate` 0x425496 `fstp [esi+0x461c]`; L262-269 in the mode-3 (driver_status 3)
  route check: `v31 = dx*dx + dz*dz; if (v31 > this[4487]) mode = 0; else this[4487] = v31`;
  init 1232348144 (0x49742400 = 999999.0) in AIDriverStructInit L81 and CameraViewportFrustumInitialization
  L54, 0 in RaceStructInit L61.
- Meaning: mode-3 route-check minimum squared distance (m^2) to the target node, initialized to the
  999999.0 "no target" sentinel; the mode ends when the distance grows again.
- Tier proposal: DERIVED.

## 37. Residual live-log list for Claude's watcher

The following are now optional live confirmations (meanings already proposed statically):

1. `command_queue_write_index` 0x44B0: log -1/indices over a race start to confirm the activity
   pattern (static semantics settled).
2. `unnamed_4000` 0x4000: capture a tick where it is nonzero (contact?) next to `cmd`/command
   events, to confirm the "contact/request -> command 6" reading.
3. `unnamed_4364` 0x4364: confirm it equals one of the logged waypoint chain pointers (static
   pointer use proven; identity of the node not).
4. `unnamed_441c` 0x441C: correlate with lateral position/radius to name the quantity.
5. `unnamed_4528`/`0x452c`: battle/side correlation to fix the sign convention.
6. `unnamed_4040` 0x4040: correlate with pit situations/fuel to name the state.
7. `placement_pose_x/y/z`: one on-track sample vs `position_x/y/z` 0x34E0 to measure the reference
   point offset (body vs log position).

No blocker. `test_abi.py` is unaffected (no ABI file edited); Claude merges `abi_v1_4_proposal.json`.
