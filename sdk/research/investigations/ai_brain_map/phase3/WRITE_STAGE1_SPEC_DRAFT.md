# WRITE_STAGE1_SPEC_DRAFT -- AI lateral-offset write, stage 1 (specification only)

Date: 2026-10-02. Author: DeepSeek V4.1 (BATCH C, offline). Task: handoff BATCH C item 4 (E4).
Status: SPECIFICATION DRAFT. No code, no build, no live write was performed in this task.
Owner approval is required for every live write (root rule); installation only after game exit + hash +
before-copy (mechanical_failures rules).

Goal: an AI car follows a commanded lateral offset through the game's own steering chain
(AISteering publishes it as the racing-line lateral target), under an in-process game-thread hook.

Read-only inputs (not edited):
- `development/mods/mechanical_failures/docs/AI_OVERRIDE_API.md` (MFAO transport + post-hook facts).
- `.../AI_PULLOFF_SYSTEM_SPEC_2026-10-02.md` (section 7 in-DLL option A, gates, scanner).
- `.../AI_PULLOFF_RUN_2026-10-02.md` (v4 chain success; live trials; writer search).
- `.../AI_RISK_SCENARIOS_LIVE_2026-10-02.md` (sections 2, 5-10: chain, crash, fighting).
- Prior art: `docs/54-ai-driving-brain-map.md`; `phase3/POINTER_WRITERS.md`; `phase3/WRITE_READINESS_V12.md`;
  `investigations/ai_brain_map/HANDOFF_2026-10-02_CLAUDE.md` item 3; findings in section 9 below.

Tier legend: FACT = read in code/finding; CONFIRMED = static + reproduced; OBSERVED = live seen;
DERIVED = interpretation; OPEN = unknown. Every claim carries its tier and source.

## 1. The chain (what we are steering)

Tick order inside `AIMainUpdate 0x424E70` (doc 54 section 1, FACT code; addresses verified again here):
stage 2 `sub_426430` waypoint locator (recomputes corridor 0x4414/0x4418 and lateral_position 0x4428) ->
stage 4 opponent scan (also resets speed_limit 0x4484 = 1e6) -> stage 5 `AIRacingLine 0x425920`
(pedals, and it also writes the avoidance field 0x4424 on some paths) -> stage 6 `AISteering 0x4270A0`
(lateral chain) -> stage 7 `sub_426100` steering law -> stage 8 `AISteeringCorrection 0x426CF0` ->
stage 10 mode switch -> stage 11 command->actuator copy (`0x442C->0x3580`, `0x4434->0x357C`, `0x4438->0x3584`).

The lateral chain in AISteering (FACT code):
- stage 6 entry 0x427114/0x42711A loads corridor bounds 0x4414/0x4418; 0x4271B0 loads request 0x4420;
  the result is the working target 0x440C (clamped/interpolated to the corridor).
- epilogue 0x4293F0-0x42942B (D10, POINTER_WRITERS.md; capstone re-checked):
  `0x4293FE 0x4408 = 0x440C`; `0x4293F0 mov al,[esi+0x4548]` -> if non-zero `0x42940C 0x4408 = 0x4420`;
  `0x429412 fld [esi+0x4424]` -> if non-zero `0x42942B 0x4408 = 0x4424` (LAST WINS).
  Then 0x429437/0x42944A set outside-corridor flag 0x4140 when 0x4408 is outside 0x4414/0x4418
  (and clear 0x4488) [FACT code; map_AISteering_part5.md].
- steering law (doc 54 section 2, FACT code + OBSERVED live 2026-10-02): target = wp.pos + 0x4408 *
  wp.right + d * wp.dir; look-ahead index 0x4468 is speed-dependent 0..3 waypoints; steer rate limit
  +-0.1 per tick (~4/s); divisor 0x44B4 = 0.4014 rad observed. So 0x4408 IS the lateral command the
  AI follows, and the only sanctioned writer is the epilogue.

Zeroing/reset constraints (the part that dictates hook timing):
- 0x4420 is zeroed every tick at `0x4253D5 mov [esi+0x4420], ebx`, guarded by
  `0x4253B1 cmp [esi+0x45FC],0` + `0x4253B9 cmp [esi+0x3D6C],2` + `0x4253C2 fld [esi+0x3984]`
  (> 1e-3): i.e. the request survives only when yield target 0x45FC != 0 OR carConditions == 2 OR
  the fuel-rate field 0x3984 <= 0.001. [FACT code, verified here; finding
  `ai-lateral-request-4420-zeroed-each-tick-2026-10-02` states the 0x45FC/carConditions part.]
- 0x4424 is reset to 0 by AIMainUpdate at 0x425369 (re-plan/limbo-reset path), by AIRacingLine
  (0x425ADC/0x425B16/0x425BD3/0x425BFF/0x425D51, stage 5), by AIAbility 0x4298D6 and by
  SessionCleanupFlagClear; RaceStructInit inits it. [FACT code, whole-.text store scan in this task]
- 0x4548 is only ever set 0/1 (init 0; AIAbility 0x42986A and PitStopDecisionTireCompound 0x42A3EF
  set 1; dispatcher sets 0/1). No per-tick clear. [FACT code, store scan + WRITE_READINESS]
- 0x440C/0x4408 need no restore: recomputed every tick. 0x45FC is never written by the AI itself
  (last in-tick writer none) but is cleared at 0x425575 in the reset block. [FACT]

## 2. Write surface and required landing stage

Two candidate surfaces; the spec recommends testing the forced one first.

### A. Forced override: 0x4424 (float, metres)
- Why: last wins in the epilogue (0x42942B); no request zeroing; no 0x45FC needed; persistence
  LATCHED (D12 rank 2, "no crash history"); bypasses the corridor clamp by construction.
- Landing stage: must be written AFTER stage 5 AIRacingLine's 0x4424 writes and BEFORE the stage-6
  AISteering epilogue. A write after stage 11 (the existing 0x424E70 post-hook) survives the rest of
  the tick and is read by the NEXT tick's epilogue only if AIRacingLine does not overwrite it first
  at next tick's stage 5 - it does on some paths, so post-hook alone is not deterministic.
- Recommended hook: 5-byte call-site detour on the AISteering call at `0x425403`
  (`E8 98 1C 00 00`), replacing it with a call to a code cave that (1) clamps and writes the commanded
  0x4424 (and, if needed, widens 0x4414/0x4418 and writes 0x4420/0x4548), (2) executes the original
  `mov ecx,esi; jmp 0x4270A0` call semantics, (3) returns to 0x425408. The call is guarded by
  `0x4550 < dword_5EA2CC` ("main chain") - keep that guard.
- Expected side effect [FACT]: the epilogue sets outside-corridor flag 0x4140 = 1 and clears 0x4488
  every tick while |0x4424| exceeds the normal corridor (~+-1 m, pull-off spec FACT live). This is
  expected for the forced path and harmless for steering; the spec accepts it but the observer should
  not treat 0x4140 = 1 as an abort by itself.

### B. Request path: 0x4548 (enable) + 0x4420 (float, metres)
- Why: the documented pull-off chain (finding `ai-pulloff-lateral-chain-v4-2026-10-02`, FACT live).
- Constraint: because of the 0x4253D5 zeroing, a write must land after the zeroing and before
  AISteering reads 0x4420 at 0x4271B0, i.e. inside the same tick. Post-hook 0x424E70 is too late for
  that tick (WRITE_READINESS: "set before stage 6, post-hook is too late for that tick").
- Same cave as A can write 0x4420/0x4548, but note the cave sits AFTER stage 5 and BEFORE stage 6: it
  satisfies the zeroing gate. If used, widen the corridor (0x4414/0x4418) in the same cave; they are
  recomputed at stage 2 (before the cave) so the cave value is what stage 6 clamps against.
- Battle caveat [FACT code]: AISteering internals (Overtaking, called from stage 6) write 0x4420 = +-3
  (0x42B0B8/0x42B0C9) and the epilogue then publishes THAT value if 0x4548 = 1. The request path is
  therefore not authoritative in battles; the forced path wins in that case.
- Yield target 0x45FC is NOT required if the write lands in-tick. If a variant needs the zeroing
  skipped (e.g. an external writer), 0x45FC must hold another car's pointer; see section 6.

### C. What NOT to do
- Never write 0x45FC = own vehicle pointer (crash finding, section 6).
- Never write carConditions 0x3D6C (limbo trap, finding `limbo-unlocked-carconditions`).
- Never write positions, heading, or the raw steering command 0x442C/actuator 0x3580 as the primary
  mechanism (FACT: raw steering swerves/overrides corner steering - trials Fisichella/Wurz; the
  H-bridge spec section 7.2). The chain exists so the AI does its own cornering.
- Do not write 0x4408 directly: it is overwritten by the epilogue every tick (D10).

## 3. Command generation and ramp limits

- Commanded offset q (metres, signed; sign convention validated live: negative = left, positive =
  right; finding `ai-pulloff-system-spec` section 10.1 F1 + owner live confirmation).
- Ramp: |dq/dt| <= 2.0 m/s (1.0 m/s above 120 km/h). At the ~40 Hz AI tick this is <= 0.05 m/tick.
  Rationale: steer rate limit 0.1/tick and the short 0-3 waypoint look-ahead (doc 54 section 2,
  DERIVED) - a step causes a sharp turn at low speed; the external trials oscillated when ramping
  faster (Frentzen overshoot; "fighting" every v1-v7 external trial).
- Clamp: |q| <= max_offset. For the first owner steps use +-2 m. For production use the scanner bound:
  `max_offset = min(road_edge + 3.0, wall_free - 1.0)` with wall_free from
  `%TEMP%\mf_dyn`/`tools/wall_scan_gui.py` math; AIW wall data is DERIVED and unreliable
  (finding `aiw-waypoint-road-vs-wall-widths-2026-10-02`, owner tests 1-5) - do not hug it.
- No brake/throttle writes in stage 1: this spec only moves the line; the AI keeps its own pedals.
- Abort/recovery: ramp q back to 0 at 2 m/s, never step.

## 4. Per-tick read-back and verification

Read back in-thread (same tick, after the AISteering call returns) and in the post-hook; log at 1 Hz
and record with brain_watch for viewer v3 (`ai_brain_gui.py --replay`, GLANCE row / LINE card;
component README, viewer v3 built 2026-10-02).

| field | offset | expected |
|---|---|---|
| published_lateral_target | 0x4408 | equals the commanded q each tick (forced path; FACT from epilogue last-wins) |
| working_lateral_target | 0x440C | starts from 0x4420 clamp; may differ from q on the request path |
| lateral_request | 0x4420 | our request on the request path; 0 on the forced path |
| hard_lateral_override | 0x4424 | our forced q; 0 after restore |
| override_enable | 0x4548 | 1 while active on the request path; 0 after restore |
| corridor bounds | 0x4414/0x4418 | normal ~+-1 m unless widened by the cave (then the widened values) |
| lateral_position | 0x4428 | converges toward q after the car moves (lag ~1-2 s at 40 Hz); does not need to equal q off-line |
| outside_corridor_flag | 0x4140 | 1 while forced beyond the corridor (expected, not an abort) |
| cmd/act steer | 0x442C/0x3580 | nonzero response; act == cmd (doc 54 OBSERVED 786/786) |
| driver_status | 0x4510 | 0 while driving; any 4/5 park or 1/6/7 recovery aborts (section 5) |
| waypoint chain | 0x4354 + 0x4358.. | non-null and advancing; chain lost = abort |
| speed | 0x4440 | for the ramp schedule and rate-abort only (stale in mode 4 - use physics/position deltas) |

Acceptance for a test: 0x4408 tracks q every sampled tick; 0x4428 moves toward q; owner sees the line
shift with no steering flicker (the two-writer symptom is ruled out by design: one writer, game thread).

## 5. Abort conditions (clear q to 0, then release)

1. Chain lost: AIMainUpdate's stage 2 returns "no waypoint" (whole tick skipped) or 0x4354 null.
2. Mode: driver_status 0x4510 in {4,5} (park), {1,6,7} (stuck/recovery); carConditions 0x3D6C == 2;
   FinishState/retired (0x4064, 0x4410) - release, do not fight the game state machine.
3. Pit lane: pittingStatus veh+0x3450 >= 1 (the game's pit path owns the car).
4. Geometry: outside_corridor_flag 0x4140 latched while |q| is small (car being pushed off), or
   |q| would exceed the current safe bound (wall_free < 0.5 m on the target side, scanner).
5. Dynamics: heading error > 60 deg (spin starting) or lateral rate > max(6 m/s, 0.9 * speed) -
   abort to q = 0 (the external trials' rate aborts; v6.1 semantics).
6. Session: pause, session end, unload, or hook layout check fails -> original AIMainUpdate only
   (existing `g_pump_frozen` / `SESSION_OVER` / `session_live` gates in engine_failure.c).
7. Any crash-risk trigger: if a future variant sets 0x45FC and it would be the self pointer, abort
   before writing (section 6).

## 6. Crash risk: 0x45FC is never a self pointer

Finding `ai-yield-target-self-pointer-crash-2026-10-02` (DERIVED, one crash event): writing the car's
own vehicle pointer into yield/interaction target 0x45FC is the prime crash suspect; the game crashed
1.5 s after the first such write (Hill, Imola, pid 9672). The game normally stores ANOTHER car's
pointer there (OpponentProximityScan lapping, session 7 only). After switching to the player car
pointer there were no crashes in 2 runs - still not a contract. [FACT code: 0x45FC consumers expect a
valid opponent pointer; DERIVED crash cause.] Consequences for this spec:
- Stage 1 must not need 0x45FC at all (in-tick cave write or forced 0x4424).
- If a later external-writer fallback is ever proposed, it must hold a real opponent pointer, be
  owner-approved separately, and be logged with the pointer's slot identity every tick.

## 7. Restoration

- Remove the 5-byte call-site patch (`0x425403` original bytes `E8 98 1C 00 00`) and the existing
  AIMainUpdate post-hook per the mechanical_failures unload path; drain in-flight calls (existing
  pattern), restore prologues, then verify byte-for-byte readback.
- Clear state: 0x4548 = 0, 0x4424 = 0. If the pre-write values were non-zero (saved at attach),
  restore them. 0x4420/0x440C/0x4408 are recomputed by the AI every tick - no restore.
- Do NOT clear 0x45F4 (game owns it; mode-4 resume byte, pull-off spec section 10).
- Before-copy: `workspace_organization_audit/<phase>_BEFORE_<date>/` for every edited source file and
  the installed DLL hash, per the product rules. Rollback = restore before-copy + rebuild/reinstall.
- Install only when the game is not running (tasklist empty), verify the exact hash, owner approval.

## 8. Owner test plan (5 steps; each step = one write the owner approves in chat)

Step 1 - hook only, no writes: install the dev DLL; verify the log lines `AI PULLOFF: AIMainUpdate
layout verified`, the new call-site hook counter, and that one AI car's 0x4408/0x4428 read back at
40 Hz. Owner watches viewer v3 LINE card; no command is written. Abort = unload, verify clean restore.

Step 2 - single forced latch: command 0x4424 = +1.0 m for 3 s, then 0. Verify 0x4408 == 1.0 every
tick, 0x4428 shifts right, 0x4140 = 1 (expected), no crash, no flicker. Owner approves this one write.

Step 3 - ramped forced offset: ramp 0 -> +3.0 m at 2 m/s on a straight at 100-150 km/h, hold 10 s,
then ramp back to 0. Verify tracking, no oscillation, 0x4408 history in the watcher replay. Owner
watches the car's line.

Step 4 - request-path comparison: same ramp through 0x4548 = 1 + 0x4420 + widened corridor (cave),
side by side with step 3. Pick the shipping path (expected: forced for authority, request for native
feel if it tracks). Owner approves.

Step 5 - abort and restore drill: command abort mid-ramp (q -> 0), then unload the DLL. Verify
0x4548 = 0, 0x4424 = 0, the car drives its own line for a full lap, hook bytes restored. Before/after
hashes recorded.

## 9. Claims and sources

- 0x4420 zeroing condition (0x45FC / carConditions 2 / fuel-rate 0x3984) - FACT code 0x4253B1-0x4253D5;
  finding `ai-lateral-request-4420-zeroed-each-tick-2026-10-02` (FACT).
- Epilogue 0x4408 = 0x440C -> 0x4420 (if 0x4548) -> 0x4424 (last) - FACT code 0x4293F0-0x42942B;
  D10 `phase3/POINTER_WRITERS.md`; doc 54 section 2.1 correction; map_AISteering_part5.md.
- Corridor recomputed each tick by sub_426430 (stage 2); normal ~+-1 m - FACT code + pull-off spec
  section 0 (live).
- Steering law look-ahead 0-3 waypoints, +-0.1/tick rate limit, 0x44B4 = 0.4014 - doc 54 section 2
  (FACT code, OBSERVED live 2026-10-02).
- Request chain live proof (corridor +0-30 m, request+target+offset ramped 2 m/s, car steered itself,
  stopped off track) - finding `ai-pulloff-lateral-chain-v4-2026-10-02` (FACT, one car).
- Request alone did nothing / ~1 m at <50 km/h with self-pointer - finding
  `ai-lateral-request-4420-zeroed-each-tick-2026-10-02` (FACT live).
- 0x45FC self-pointer crash - finding `ai-yield-target-self-pointer-crash-2026-10-02` (DERIVED).
- post-hook full brake/throttle/steer authority (one writer, game thread) - AI_OVERRIDE_API.md
  (FACT live 2026-10-02); external writer = flicker/two-writer limit - finding
  `ai-external-control-two-writer-limit-2026-10-02`.
- Hook install patterns / prologue / layout gates - AI_PULLOFF_RUN_2026-10-02.md sections 1, 9, 10
  (FACT, build + offline gates + installs).
- AIW wall width unreliable - finding `aiw-waypoint-road-vs-wall-widths-2026-10-02` + owner tests 1-5
  (FACT/DERIVED mix).
- 0x4408/0x44B4 writer closure - finding `ai-brain-4408-44b4-writers-closed-2026-10-02` (CONFIRMED).

## 10. Open / out of scope

- Request-path authority in battles (Overtaking writes 0x4420 +-3 inside stage 6) - not settled;
  the forced path is recommended exactly because of this.
- Exact interaction with AISteeringRacingLineCorrection (stage 8): it can clamp throttle when
  0x45FC is set (doc 54 section 3); with the forced path 0x45FC stays 0, so unaffected.
- Corridor widening side effects on the AI's own lane switching (0x4548 and 0x4420 are also used by
  the AI's pit/lapping logic: AIAbility 0x42986A/0x429761, Overtaking 0x42B0B8/C9,
  OpponentProximityScan 0x42A939) - the cave overwrites them for the controlled car while active;
  accept for stage 1, watch in steps 3-4.
- True wall/barrier sensing stays phase-2 research (collision grid 0x44BA40); the spec uses the AIW
  scanner with its documented uncertainty.
- This is a draft for Claude review and owner approval; it does not install, build, or write anything.
