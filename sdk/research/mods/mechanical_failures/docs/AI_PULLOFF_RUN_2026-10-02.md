# AI pull-off / "get off the line" - ADDENDUM 15 (P1-P8) run record (2026-10-02)

Status: IMPLEMENTED + OFFLINE VERIFIED, NOT INSTALLED (dev dll `88C41421`). Pipeline stage: implement/verify.
Spec: `docs/AI_PULLOFF_SYSTEM_SPEC_2026-10-02.md` section 7 (in-DLL option A; supersedes its section 2 external-loop notes).
Work order: `docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt` ADDENDUM 15. Sources at start: `engine_failure.c 078742A4`,
`mf_ai_react.h 3B90005C`, dev dll `6CCBA4A2` (ADDENDUM 14 end state).

Owner direction built in: ONE writer (the AIMainUpdate detour, game thread) - every external-loop trial flickered because the
AI and the script wrote the same actuators (finding ai-external-control-two-writer-limit-2026-10-02).

## 1. Per-item result (files = end-state SHA-256 first 8)

| Item | Result | Evidence |
|---|---|---|
| P1 prior art + verify | DONE - no first-party mod hooks AIMainUpdate (grep over `development\mods`: comments/reads only; `full_safety_car`/`red_flag` reference the function, never detour it). Prologue from the SDK exe-disassemble of the installed `F1 Challenge Online.exe` (SHA-256 `1A70E5ED...`): `0x424E70: 83 EC 1C 53 55 56` (sub esp,0x1c; push ebx/ebp/esi), resume `0x424E76` (mov esi,ecx). Runtime layout check: `po_verify_layout()` reads the game active array `0x7C54F0` + `VehicleCount 0x5C329C` (finding native-sc-sim-bound-is-vehiclecount) and fails closed - a mismatch logs + disables the feature. | static: SDK `functions/AIMainUpdate_00424e70.c`, `exe-disassemble 0x424E70`; code section 6 below |
| P2 pure header | DONE - new `dll/mf_ai_pulloff.h` (`4791DEDD`): scanner math (true lateral wp+0x94/0x20/0xA4, road wp+0x2C/0x30, wall wp+0x34/0x38, next wp+0x110, segment wp+0x3C), side choice (SAFE room >= 4 m; street fallback), goal + wall cap, traffic pause (25 m / 3 m / 4 s then half rate), state machine NOTICE/COAST/WAIT/DRIFT/GOAL/ABORT/PARKED/ASIDE, control law 7.2. No game access. | `tools/ai_pulloff_test.c` (`FC91B69F`) + `tools/run_ai_pulloff_test.bat` (`1295DD39`): ALL PASS 58 |
| P3 hook | DONE - `ai_pulloff_hook` detours `0x424E70` (fuelcut/cam-tap pattern, stolen 6 bytes, trampoline, bounded in-flight drain + PIN on a failed restore); calls the original, then `po_apply()` overwrites command+actuator pairs steer `0x442C`/`0x3580`, brake `0x4438`/`0x3584`, throttle `0x4434`/`0x357C` (spec 7.1 corrected mapping). Never writes `0x45FC`, `0x3D6C` or positions (static audit returned no matches). Pause/session-end/unload = original only (`g_pump_frozen` / `SESSION_OVER` / `session_live` gates; detour removed at unload). Worker output crosses to the hook through a per-slot seqlock (`MfPoShare`). | static audit greps; build; gates |
| P4 surface | DONE - per-wheel material through the SDK `f1c_surface_contact.h` chain (`veh+0x3674 + i*0xF8 -> +0x54 -> +0x30`, road/wet/rumble = drivable); `build_merged.bat` now adds the SDK include path for `engine_failure.c`. | `po_surface()` + gates |
| P5 triggers | DONE - every ADDENDUM 14 Option A ending starts the pull-off after a 0.3-1.0 s notice (per-driver from the sensitivity trait): model terminals (blow/seize/stop), R4 retire, and game-clock engine kills (ReasonCode 1, finish 0). R2 limp starts ASIDE (released on fault end / pit entry / finish); blow-ups schedule the P5 fire fade (bank3 -> 0 after 10-20 s, bank1/2 kept). | code; model/reaction gates |
| P6 park | DONE - at GOAL/ABORT write steer 0 + brake 1.0 + throttle 0 until the physics speed (position deltas; `0x4440` goes stale in mode 4) is < 0.5 km/h, then write AI mode `0x4510` = 4 once (native park, finding ai-mode-4-native-park-2026-10-02) and stop writing. All state released at FinishState 2 AND when a finished/dormant car loses its readable waypoint chain, so a stale output can never keep being applied. | `test_goal_park_release`, `test_abort` |
| P7 logs + panel | PARTIAL (DLL side DONE) - edge logs `AI PULLOFF:` (begin reason/notice, coast side+room, traffic wait with car free, drift side+goal+wall free+road, goal wheels off, abort why, parked) and `AI ASIDE:` (begin goal+side, releases); EngineCtl appended at the END (ABI 13 kept, `_Static_assert` 656 -> 944): `ai_po_state[24]`, `ai_po_side[24]`, `ai_po_goal_x10[24]`. The panel `.py` row is NOT wired - the run limits forbid `.py` edits (Claude wires `docs/AI_PULLOFF_SYSTEM_SPEC_2026-10-02.md` panel row from these fields). | worker publish block; panel smoke |
| P8 gates | DONE - table section 3. | gates |

## 2. Start/end hashes

- Start: dev dll `6CCBA4A2`, `engine_failure.c 078742A4`, `mf_ai_react.h 3B90005C`.
- End: dev dll `88C41421` (build x2 identical); `engine_failure.c 3DB73FF4`, `mf_ai_pulloff.h 4791DEDD`,
  `ai_pulloff_test.c FC91B69F`, `run_ai_pulloff_test.bat 1295DD39`, `build_merged.bat E9317E98`;
  `mf_ai_react.h 3B90005C` and `mf_engine_model.h E5E74966` unchanged.
- Installed game copy untouched (no install in this run).

## 3. P8 gates

| Gate | Result |
|---|---|
| Model test (`run_engine_model_test.bat`) | ALL PASS 67577 |
| Msg test (`run_msg_test.bat`) | ALL PASS 57 |
| AI reaction test (`run_ai_react_test.bat`) | ALL PASS 40 |
| AI pull-off test (`run_ai_pulloff_test.bat`) | ALL PASS 58 |
| Normal car sweep (player rows) | out.csv SHA-256 `67E70EC2` = byte-identical to the stored `%TEMP%/mf_sweep_T3` baseline (145 cars, all profiles; scratch `%TEMP%/mf_sweep_P15`, 8.2 s -j 20) |
| grade_test | ALL PASS 306 |
| GUI unittest | Ran 20 tests OK |
| Panel smoke | PASS (pages incl. AiPage; repair sends cmd_repair only) |
| Workspace validator | PASS 0 errors / 0 warnings |
| Build x2 | `88C41421` = `88C41421` (after the pause gate) |
| Forbidden-write audit | no `0x45FC` / `0x3D6C` / position writes in the new code; the only new game writes are the command+actuator pairs and mode 4 |
| Hook count | one detour install, one remove (worker start/exit) |

## 4. Static facts used (each address carries its finding/evidence)

| Address | Meaning | Source |
|---|---|---|
| `0x424E70` | AIMainUpdate (__thiscall, ecx = vehicle; entry guard `veh+0x32C8 > 1`) | SDK `functions/AIMainUpdate_00424e70.c`; findings f1c-ai-drives-a-car-only-if-a-waypoint-query-succeeds, native-sc-aimainupdate-self-gates-no-count-change |
| `0x442C/0x3580`, `0x4438/0x3584`, `0x4434/0x357C` | steer, brake, throttle command -> actuator copies at the end of the tick | AIMainUpdate raw source lines 161-165 (spec 7.1 correction: brake is 0x4438, throttle 0x4434) |
| `0x7C54F0`, `0x5C329C` | active vehicle pointer array + VehicleCount | finding native-sc-sim-bound-is-vehiclecount |
| `wp+0x94/0x9C` active pos, `+0x20/0x28` perp, `+0xA4/0xAC` fwd, `+0x2C/0x30` road L/R, `+0x34/0x38` wall L/R, `+0x3C` segment, `+0x110` next | in-memory AIW waypoint (316 B) | doc 52 + findings aiw-waypoint-road-vs-wall-widths-2026-10-02, ai-free-space-scanner-prototype-2026-10-02 |
| `veh+0x4354` | current waypoint pointer | finding 0x4354-waypoint-array |
| `veh+0x34F0` | yaw (fwd = (-sin, -cos)) | AIMainUpdate raw source lines 181-182 |
| `veh+0x3450` | pittingStatus (>= 1 pit lane) | finding pitstop-sequence-deep-map / S1-vehicle-lifecycle-fields |
| `veh+0x3674` chain | per-wheel material | SDK `f1c_surface_contact.h` |
| `0x4510` = 4 | native park (steer 0 / brake 1 / throttle 0 every tick) | finding ai-mode-4-native-park-2026-10-02 |

## 5. Deviations and open items

- "R4 pit-retire drive" and "terminal limp" as persistent states do not exist after ADDENDUM 14: R4 is an immediate
  Option A ending and the maxSpeed limp is written only with R4. P5 therefore wires the endings chain + R2 ASIDE;
  the continue-or-retire/laps-left backstop stays an owner decision (explicitly out of this addendum).
- Panel row: DLL fields published, `.py` untouched (base limit; Claude/owner wires it).
- The apex-side sign (`po_apex_side`) and the traffic behaviour while the hook forces the controls are starting
  values - live checks.
- The pull-off hook is optional: a prologue mismatch or a failed layout check logs and disables only this feature.
- ASIDE additive-steer clamp is a starting value (spec 7.2 says only "the additive steer term"; no live tuning in
  this run).

## 6. Owner live checks (run on the next installed dev build)

Spec section 3 list + owner asks:
1. Imola, sudden failure on a straight and in a corner: car coasts, drifts off, stops off the line, no wall contact,
   no spin, no steer-back; `AI PULLOFF:` lines match what is seen.
2. One wide track: same; goal = road edge + run-off with the wall cap; check `AI PULLOFF: ... drift side ...`.
3. One failure with a car right behind: `WAIT_TRAFFIC` line, then the drift after the car has passed (or 4 s).
4. Street-type section (wall at the kerb): street fallback stop at the road edge.
5. Blow-up: fireball fades after 10-20 s (`blow-up fire faded`), light smoke stays.
6. R2 limp: `AI ASIDE: begin` - car moves off the line but stays on the road for 2 laps while the field passes;
   release at the pit stop.
7. No visible steering flicker anywhere (the former external-loop symptom).
8. Log lines: `AI PULLOFF: AIMainUpdate layout verified`, `detour installed at 0x424E70`; AI pump ms <= 6.

## 7. Restoration information

- Before-copy: `workspace_organization_audit/ENGINE_SIM_BEFORE_2026-10-02_P15/` (`engine_failure.c 078742A4`,
  `mf_ai_react.h 3B90005C`, `mf_engine_model.h E5E74966`, `build_merged.bat CAF11FB4`, README `6C932F04`,
  batch log `8F2C101B`, HANDOFF g `946D05C7`). New files have no before state.
- Rollback = restore `engine_failure.c` + `build_merged.bat` from the before-copy and rebuild; delete the new
  header/test/bat (owner decision). The installed game copy was never touched.
- Per-run sweep/parse output: `%TEMP%/mf_sweep_P15` (disposable).

## 8. Wasted-effort check

- Repeated reading: the AI pump/struct and the spec were read once per edit area; the waypoint layout needed the
  doc-52 read (one lookup, not pre-knowledge).
- Broad builds: the full model test was re-run only after the engine edits (once) and once for the gate.
- Failed/retried commands: one duplicate-macro compile error (`MF_PO_ABORT_RATE`), one misplaced reset block found
  by reading the diff and fixed in the same cycle; the test harness needed three iterations (synthetic waypoint
  geometry, notice transition, park pulse) - each failed run pointed at a real semantics question and the final
  semantics are now pinned by tests.
- Polling/idle: none observed.

## 9. Claude review (2026-10-02, HANDOFF section 0 action 1)

Verdict: CONDITIONAL PASS - fix R1 before install. Before-copy: `workspace_organization_audit/P15_REVIEW_BEFORE_2026-10-02/`.

Hashes (FACT, re-computed): dev dll `dll/mechanical_failures.dll 88C41421` (2026-10-02), `engine_failure.c 3DB73FF4`,
`mf_ai_pulloff.h 4791DEDD`, `mf_ai_react.h 3B90005C`, `mf_engine_model.h E5E74966`, `ai_pulloff_test.c FC91B69F`,
`run_ai_pulloff_test.bat 1295DD39`, `build_merged.bat E9317E98` - all match section 2. Not rebuilt (build_merged.bat can
copy into the game install).

Gates re-run (FACT): ai_react ALL PASS 40, engine_model ALL PASS 67577, msg ALL PASS 57, ai_pulloff 58 checks 0 fail.
Not re-run: sweep, grade_test, GUI, panel smoke.

Hook read (`engine_failure.c` 8005-8177):
- One writer: OK. Actuator writes only in `po_apply` on the game thread, after the original. Exception: AI mode `0x4510 = 4`
  is written once from the worker (line 7987) - one-shot, not an actuator; acceptable, noted.
- Mapping: OK. steer 0x442C/0x3580, brake 0x4438/0x3584, throttle 0x4434/0x357C match decompile lines 161-165.
- No 0x45FC / 0x3D6C writes: OK (grep).
- Pause/session end: OK. `g_pump_frozen`, `SESSION_OVER`, `session_live` return before any write = original only.
- Prologue/trampoline: OK. 6 stolen bytes position-independent; thiscall, one arg (decompile `__thiscall sub_424E70(int this)`).
- Seqlock: OK for x86 (volatile order kept, single worker writer).

Findings:
- R1 (MEDIUM, DERIVED, must fix or live-prove): additive steer accumulates. `po_apply` reads 0x442C after the original and
  writes `cmd + steer_add` back. AIMainUpdate rewrites 0x442C only on the LABEL_86 path; early returns (entry guard,
  waypoint-query fail, case 2) and the park tail never touch it, so on those ticks the hook adds onto its own last value
  (ramp to full lock = spin / wall risk in ASIDE). Fix: keep per-slot `last_written`; if 0x442C still equals it, use the
  stored base, else base = new cmd; then write base + add (clamped). Also add a pure test. Finding
  `ai-mainupdate-steer-cmd-not-rewritten-every-path-2026-10-02` (OPEN: is 0x442C rewritten by another routine each tick?).
- R2 (LOW): unload drain window. A thread that jumped into the hook but has not yet run `InterlockedIncrement` is missed by
  the drain. Same pattern as the existing fuelcut/cam-tap hooks; tiny window; no change asked.
- R3 (INFO): steer-add clamp and apex sign are untuned starting values (section 5) - live only.

Pending: R1 fix, then rebuild + owner live test (HANDOFF section 0 action 2). Install still blocked.

## 10. Claude double-verify + fixes + INSTALL (2026-10-02, owner: "double verify the whole pack, fix, then install")

Before-copies: `workspace_organization_audit/P15_FIX_BEFORE_2026-10-02/` (engine_failure.c 3DB73FF4, mf_ai_pulloff.h
4791DEDD, ai_pulloff_test.c FC91B69F, dll 88C41421, this record, HANDOFF, README). Installed before-copy:
`workspace_organization_audit/INSTALLED_BEFORE_2026-10-02_P15/mechanical_failures.dll` (D7F23A4E).

### 10.1 Defects found and fixed

| # | Severity | Defect | Evidence | Fix |
|---|---|---|---|---|
| F1 | HIGH | Left/right MIRRORED. Header used +lat = left (free_L = wallL - lat). Game frame: +lat = RIGHT. A car that picked "L" steered to +lat (physically right) but was limited by the LEFT road/wall widths - wall-contact risk wherever the widths differ. | CODE: AISteering 0x4270A0 l.508 (lat formula) + l.775-786 (upper bound to +wp+0x38 wall R, lower to -wp+0x34 wall L). DATA: Imola/Albert Park AIW wp_perp = (fwd_z, -fwd_x). Live prototype pulloff7.py used wl = wallL + lat. | `MF_PO_SIDE_L = -1`, `MF_PO_SIDE_R = +1`; free_L = wallL + lat, free_R = wallR - lat; car-free sides swapped in `po_traffic_scan`; EngineCtl `ai_po_side` comment = -1 left / +1 right. |
| F2 | HIGH | Goal check `fabs(lat) >= goal`: a car starting on the other side of the AIW line (e.g. 5 m right, drifting left, goal 4 m) went to GOAL at once and braked to a stop in the middle of the road. | mf_ai_pulloff.h DRIFT block | `side x lat >= goal` + test. |
| F3 | MEDIUM | Hook steer accumulation (review R1, section 9). | AIMainUpdate rewrites 0x442C only on LABEL_86 | pure `mf_po_steer_compose()` (reuse the AI base while 0x442C still holds our write; clamp -1..1 = AI range, AISteeringRacingLineCorrection l.214-221) + per-slot game-thread fields `po_hk_*` + tests. |
| F4 | MEDIUM | Traffic "target side" measured from the AIW line, not from our car: a car between us and the goal was ignored when we started off-line. | header traffic rule | `side x (car_lat - our_lat) > 0` + tests. |
| F5 | MEDIUM | Game-clock engine kill on a car in ASIDE (R2 limp) never started the pull-off (`!active` gate) - the dead car rolled to a stop on the line. | engine_failure.c GAMEKILL trigger | also allowed when state == ASIDE. |
| F6 | LOW | Fire fade ran only while the pull-off was active: a car released early (FinishState 2 / chain lost) kept the fireball. | pump order | fade block moved before the active check (session-live guarded). |
| F7 | LOW | Apex side from a fwd-vector cross product with an unverified sign. | po_apex_side | far look-ahead waypoint's lateral in the current waypoint frame (no handedness assumption; along > 10 m, ratio 0.15). |
| F8 | INFO | Log side label printed "R" when no side was chosen. | po_log_transition | "-" when side 0. |

Steer sign (checked, NOT changed): positive 0x442C turns toward +lat (AISteeringRacingLineCorrection 0x42B0E0 l.210:
atan2(-d.x_local, -d.z_local); car fwd = -z_local = (-sin yaw, -cos yaw); -x_local = (fwd_z, -fwd_x) = wp_perp; matches
the Herbert v6 trial: negative steer -> lat -5.3 m). DERIVED (static + one trial) - live check 10.4.

Not changed (noted): ASIDE side is chosen once (spec 1.2 says "at the next corner"; per-corner switching would cross
the track); R2 unload drain window (section 9); panel .py row still not wired.

### 10.2 Verification (FACT, this run)

- End hashes: engine_failure.c F1AFA0E4, mf_ai_pulloff.h E8A50F49, ai_pulloff_test.c 1C590BD3; dev dll `2D588299`
  (build x2 identical, -Werror).
- ai_pulloff ALL PASS 67 (was 58: +relative traffic x2, wrong-side goal, steer compose x5, side-L check).
- ai_react ALL PASS 40, engine_model ALL PASS 67577, msg ALL PASS 57.
- Sweep `%TEMP%/mf_sweep_R15` out.csv `67E70EC2` = T3 baseline (byte-identical); grade_test ALL PASS 306.
- GUI unittest 20 OK; panel `--smoke-test` PASS (EnginePage, AiPage, GearPage, SuspensionPage, ElectronicsPage).
- WAVs identical to the installed copies (not re-copied).

### 10.3 Install (owner-authorized in chat 2026-10-02)

Game not running (tasklist). Copied `dll/mechanical_failures.dll` to `Documents\F1 Challenge V10\plugins\lightloader\`;
installed hash verified `2D588299` (was D7F23A4E; this install also brings ADDENDUM 14 AI symptoms + reactions live for
the first time). Rollback: copy the INSTALLED_BEFORE file back.

### 10.4 Pending owner live checks (in addition to section 6 and ADDENDUM 14 s8)
- Log: `AI PULLOFF: AIMainUpdate layout verified` + `detour installed at 0x424E70`.
- Side check: `drift side L` -> the car physically moves LEFT (validates F1 + the steer sign).
- Car off-line at failure (other side): drifts across, no early stop mid-road (F2).
- ASIDE car: steady offset, no slow creep to full lock (F3).

## 11. Live trial 1 (2026-10-02, installed 2D588299, 1998 field, race, owner watching)

Trigger (owner-approved in chat, disposable script `%TEMP%\mf_dyn\po_trial.py`): one write per trial, AI car game engine
timer `veh+0x3D70 = 0.5` -> the game kills the engine itself (ReasonCode 1). Monitor reads only. Per-run output
`%TEMP%\mf_dyn\po_trial_s7_1.txt`, `po_trial_s6_1.txt` (disposable).

| Trial | Result | Tier |
|---|---|---|
| T1 Frentzen (slot 7), t=177 | Pull-off NOT active: panel "AI engines" (`ai_engines_enable`) is off by default (owner test gate, engine_failure.c E9p), so `po_apply` returns and no AI slot logic runs. What the owner saw = VANILLA dead-engine AI: brake 1.0 ~1.5 s after death, ~5 m swerve right (lat +4.9), stop from 121 km/h in ~2 s, steer to -1.0 while stopped, then AI mode 6. Owner: full brake at once, stuck at an odd angle on the run-off, wall far away. | FACT (memory + owner) |
| T2 Hill (slot 6), t=281, AI engines ON | Log: `scripted drop 21079 -> 0`, terminal (ReasonCode 1), `notice done -> coast (side L, room L/R 5.0/0.0)`, `drift side L goal 5.5 m`, `aborted (lateral rate, lat rate -7.9 m/s)` at t=284, `parked (mode 4)` t=285. Memory: lat 0 -> -0.6 -> -6.1 -> -8.2 m (side L = negative lat, as fixed in s10), ~160 km/h -> 0 in ~2 s after the abort (abort = steer 0 + brake 1.0). Owner: "took off the track but applied brake too heavily, didn't allow the car to steer off fully, braked as soon as it was off track (might leave it in a dangerous position)". | FACT |

Other observations:
- `AI PULLOFF: AIMainUpdate detour installed at 0x424E70` logged - but into `SeasonData\Vehicles\BAR\engine_failure_log.txt`:
  `LOG_PATH` is relative and the game changes its working directory during load, so early log lines scatter into
  vehicle folders (pre-existing defect, all families; follow-up: absolute log path from the DLL module folder).
- Physical side of T2: owner confirmed Hill went LEFT with log `side L` (negative lat) - validates the s10 F1 L/R fix and the steer sign live (FACT, owner).

### 11.1 Changes from trial 1 (dev dll BDC06AE9, INSTALLED 2026-10-02 after game exit, hash verified; rollback = INSTALLED_BEFORE_2026-10-02_LIVE1 2D588299)

Owner rule: full brake ONLY at a halt; steer off fully first.
- New state ROLL (9) replaces the full-brake GOAL: wheels off / goal reached / wall margin -> keep steering to
  `hold = max(goal, current side x lat)`, throttle 0, brake 0.05 (0.15 below 15 km/h); PARKED (mode 4 = brake hold) only
  at < 0.5 km/h. ROLL steer is ABSOLUTE (AI steer discarded, else the AI steers the slowing car back to its line):
  `-2.0 x heading_side + 0.03 x (target - lat)`, clamp +-0.4; heading_side = dot(car fwd, wp perp) (new input). Wall
  closer than the margin pulls the target inward; heading > 60 deg still aborts (spin = steer 0 + brake 1.0).
- Lateral-rate abort scales with speed: limit = max(6 m/s, 0.5 x speed) (T2: 7.9 m/s at ~45 m/s was a ~10 deg lane change).
- Goal farther when the run-off allows: road edge + clamp(0.5 x run-off, 2.1, 6.0) m, still capped at wall free - 0.5 m.
- Log `AI PULLOFF: ... roll-out side S hold H m (wheels off, lat, wall free, km/h)`.
- Before-copy `workspace_organization_audit/P15_LIVE1_BEFORE_2026-10-02/` (engine_failure.c F1AFA0E4, mf_ai_pulloff.h
  E8A50F49, ai_pulloff_test.c 1C590BD3, this record); installed 2D588299 copy in `INSTALLED_BEFORE_2026-10-02_LIVE1/`.
- Gates (FACT): ai_pulloff ALL PASS 75; ai_react 40; engine_model 67577; msg 57; GUI 20 OK; panel smoke PASS; build x2
  `BDC06AE9`. Sweep not re-run (sweep does not include mf_ai_pulloff.h; engine model unchanged).
- Untuned starting values (live): ROLL brakes 0.05/0.15, K_HEAD 2.0, K_LAT 0.03, clamp 0.4.

Next live (after install): same Hill-type trial; check ROLL line in the log, no full brake while moving, car ends
parallel to the track at the goal distance, no return toward the racing line.

## 12. Live trial 2 + owner wall-hug rule (2026-10-02, installed BDC06AE9)

T3 Takagi (slot 20), race start t=21 (pack behind): log `coast (side R)`, `drift side R`, then WAIT -> DRIFT -> WAIT x3
(each WAIT reset its 4 s budget; WAIT and COAST braked 0.1), speed bled 149 -> 0 on the line, brake 1.0 from t=26.5 (source
not our output - open), drift finally re-chosen side L at t=29 with the car already stopped. Owner: "paused in the middle of
the track". FACT (log + memory + owner).

Fixes (dev dll A39481F0, build x2; NOT installed - game running):
- WAIT budget = 4 s TOTAL per pull-off (no reset on re-entry); no wait below 30 km/h; COAST/WAIT/DRIFT no longer brake
  (throttle 0 only, AI's own corner braking stays); a car stopped in DRIFT goes PARKED (hold).
- OWNER RULE (chat 2026-10-02): every failure -> pull as far from the track as possible and hug the wall without hitting
  it. Side = more wall free space; goal = wall free - 1.0 m (wall free already minus half the car width 0.9 m; ROLL
  still pulls inward when wall free < 0.5 m). Supersedes the road-edge/run-off goal of s11. RISK: AIW wall widths are
  DERIVED (finding aiw-waypoint-road-vs-wall-widths: aiming at 0.9 x wall put two cars into the wall in the external
  trials) - live check.
- Gates: ai_pulloff ALL PASS 74 (side/goal tests rewritten for the rule, +live-2 budget tests); ai_react, engine_model,
  msg PASS; GUI 20 OK. Hashes mf_ai_pulloff.h/ai_pulloff_test.c: 08b77163 a362b7e1 . Before-copy P15_LIVE2_BEFORE_2026-10-02.
- Open: brake 1.0 source at T3 t=26.5.

## 13. Wall-scanner live GUI (2026-10-02)

`tools/wall_scan_gui.py` (new, read-only, tkinter, 10 Hz): for one slot (default 0 = owner car) shows the cross-section
(road edges, AIW wall L/R, AIW line, car, wall-hug goals), wall FREE L/R exactly as the DLL computes it (min over the
look-ahead >= 30 m / 3-12 wp, minus half car 0.9 m), per-wheel surface and a 30 s history of the closest wall free space.
Purpose: owner validation of the DERIVED AIW wall distances before trusting the wall-hug goal (s12).
First read (FACT, pid 7288, t=311): owner car lat -8.78, road L/R 5.0/12.3, AIW wall L/R 10.0/12.3, free L/R 0.27/19.4,
all 4 wheels off the drivable surface. Accuracy vs the real wall: pending owner observation.

Owner test 1 (FACT, screenshot + read, pid 7288 sim t 352.1, Imola-type gravel trap): owner car on GRAVEL (4/4 wheels off),
lat +22.45 m; AIW road R 4.6, AIW wall R 9.6 -> scanner wall free R = -13.9 m (i.e. "13.9 m beyond the wall") while
the real barrier (Agip boards) is still further out. CONCLUSION: the AIW "wall" width is NOT the physical barrier here -
it ends inside the gravel trap (an AI limit line). Wall-hug on AIW data stops the car at ~8.6 m (inside the gravel),
not at the barrier: safe here but not "hugging". Earlier trials (finding aiw-waypoint-road-vs-wall-widths) also saw the
opposite case (aiming near AIW wall put cars into the wall). So AIW wall = unreliable both ways; a true wall hug needs
real collision geometry (0x44BA40 broad-phase grid, research). GUI goal markers are wrong when free < 0 (minor).

Owner test 2 (Imola-type straight edge, FACT): car straddling the right road edge (right wheels on grass): wheels FL/RL
road, FR/RR OFF = correct; AIW road R 4.8 m vs real edge ~2.5-3 m -> AIW road edge ~2 m wide of the painted edge here.
Owner test 3 (Indianapolis infield, asphalt run-off, FACT): lat -12.06, AIW road L 10.1, wall L 15.1 (free L 1.1);
FL wheel on run-off asphalt = code 6/21/27 (drivable "road"), others 8/23/32 (OFF). Asphalt run-off reads as drivable,
so wheels-off cannot end a pull-off there. Owner chose option A (wheels off OR >= 3 m beyond the AIW road edge, AIW wall
as outer cap) - pending: does track asphalt carry a different code than run-off asphalt (GUI now shows raw codes).

Owner test 4 (Indianapolis, FACT): car on grass next to the oval wall read lat -86.75 / free -75 / 90.5 - the car's
current waypoint (veh+0x4354) was on another part of the circuit: far off the track the waypoint frame is stale/wrong.
Owner test 5 (FACT): all 4 wheels on racing-track asphalt = 6/21/27, the SAME code as the asphalt run-off -> surface cannot
separate run-off asphalt from the track. Same screen exposed a scanner bug: wall free L -3.4 m in mid-track because the
look-ahead re-projected the position into far (corner) waypoint frames.

### 13.1 Option A + scanner fix (dll CC85B004, build x2; INSTALLED 2026-10-02 after game exit; rollback INSTALLED_BEFORE_2026-10-02_LIVE3 = BDC06AE9)
- mf_po_scan: car offset measured once in the current waypoint frame and carried along the look-ahead (prototype
  behaviour); same fix in wall_scan_gui.py (same screen now reads free L/R 10.8/11.3).
- Goal (owner option A): |lat| goal = AIW road edge + 3 m, capped at side x lat + wall free - 1.0 m (the old cap mixed
  car-relative free space with line-relative goal - fixed). ROLL still starts at wheels off / goal / wall margin.
- Superseded: the s12 wall-hug goal (AIW wall is not the barrier, owner test 1).
- Gates: ai_pulloff 76 PASS (+corner look-ahead, cap units), ai_react/engine_model/msg PASS, GUI 20 OK.
  Hashes: mf_ai_pulloff.h 65BCAA5A, ai_pulloff_test.c 31E73044, wall_scan_gui.py 496C8A59.
  Before-copy P15_LIVE3_BEFORE_2026-10-02.
- Known limit: far off track the waypoint frame can be wrong (test 4); the pull-off resets the rate on a waypoint
  change but trusts lat - live watch.

## 14. Live trial 4 (2026-10-02, installed CC85B004, 2005-type field, t=60)
T4 Massa (slot 9): log `coast (side L)`, `drift side L goal 10.3 m (road L 7.6)`, `aborted (lateral rate -9.2 m/s)` t=64,
`parked` t=65. Memory: lat 0 -> -2.0 -> -8.1 -> -11.7 m (about 2 m beyond the AIW road edge), 154 -> 0 km/h, brake 1.0
from the abort. Owner: "pulled away, but brake was at 100% even though it was at a relatively slow speed". FACT.
Cause: the speed-relative rate abort (0.5 x speed) fires as the car slows while still moving sideways (~30 deg turn-out).
Fix (dev, not installed - game running): limit = max(6, 0.9 x speed) = only a slide; spins stay on the 60 deg heading
abort. ai_pulloff 77 PASS; other gates PASS; build x2 hash in the next line.
- Dev dll C1F44D52 (build x2).
- INSTALLED C1F44D52 2026-10-02 after game exit (hash verified); rollback INSTALLED_BEFORE_2026-10-02_LIVE4 = CC85B004.

## 15. Live trial 5 (2026-10-02, installed C1F44D52, t=44)
T5 Webber (slot 6): `coast (side R)`, `drift side R goal 9.6 m`, `parked` t=50; memory: brake 1.0 from t=47.3 (3 s
after death) while in DRIFT, lat 0.3 -> 6.6 m, 209 -> 0 km/h; stopped ON the road (road R 8.8). Owner: "100% brake
again, stuck in the middle of the track". FACT. Cause: the brake 1.0 is the AI's OWN dead-engine brake (same as the
vanilla T1); C1F44D52 no longer wrote any brake in COAST/WAIT/DRIFT, so it passed through (earlier builds masked it with
brake 0.1). Fix (dev, needs game exit): new output brake_cap 0.3 in COAST/WAIT/DRIFT - the hook clamps the AI's own
brake command+actuator to 0.3 (AI corner braking kept, no slam). ai_pulloff 77 PASS, other gates PASS. Before-copy
P15_LIVE5_BEFORE_2026-10-02.
- Dev dll 8fe9c8ea (build x2).

## 16. Live trial 6 (2026-10-02, installed C1F44D52, t=144)
T6 Monteiro (slot 15): `coast (side R)`, `drift side R goal 10.9 m`; memory: lat stayed +0.1..+0.5 m from 213 down to
27 km/h, steer 0.00-0.02, no AI brake slam this time. Owner: came to a near pause, "his engine is still running". FACT.
Causes: (1) the ADDITIVE drift nudge (clamp 0.018 at 200 km/h, 0.06 at <= 60) is cancelled by the AI's own
line-following - the earlier "drifts" (Hill/Massa/Webber) were mostly the AI's own dead-engine swerve; (2) the game-clock
kill only sets ReasonCode 1 and our handler restored the levers, so the engine kept idling (crawl in gear).
Fixes (dev C91097B7, build x2; needs game exit):
- DRIFT + ROLL steer ABSOLUTE via `mf_po_abs_steer`: lateral target ramped at 2 m/s (1 m/s > 120 km/h, half on the
  traffic timeout) -> desired sin(heading) = clamp(0.08 x error, +-0.25) -> steer = 2.0 x f^2 x (desired - actual),
  clamp 0.4 x f, f = min(1, 60 / km/h). Starting values, live.
- Game kill of a car on track -> Option A engine stop (power column 0 + idle 0, no fire), log `AI END: slot N game kill
  -> engine stop`.
- Also from s15: AI own brake capped at 0.3 in COAST/WAIT/DRIFT.
- Gates: ai_pulloff 81 PASS (+abs drift ramp/counter-steer), ai_react/engine_model/msg PASS. Before-copy
  P15_LIVE6_BEFORE_2026-10-02.
- INSTALLED C91097B7 2026-10-02 after game exit (hash verified); rollback INSTALLED_BEFORE_2026-10-02_LIVE6 = C1F44D52.

## 17. Live trials 7-8 (2026-10-02, installed C91097B7)
- T7 Sato (slot 18, t=27, ~79 km/h): `game kill -> engine stop` logged (engine stop WORKS); drift L, lat +0.3 -> -7.5 m
  over 7 s, steady, brake capped 0.3; ran out of momentum at -7.2 (AIW road L 7.9, goal 11.2). After park the AI's own
  logic steered +-0.6 while stationary (mode 6). FACT.
- T8 Albers (slot 3, t=104, 274 km/h): owner "didn't do a single thing to steer off the track". Memory: steer 0 during
  COAST (> 140 km/h by design), then actuator brake 1.0 at t=107 (182 km/h) -> stopped on the line (lat 0.07) at t=109.
  Causes: (1) drift waited for < 140 km/h; (2) AIMainUpdate's stop tail writes ACTUATOR 0x3584 = 1.0 directly
  (decompile l.310-312) - the brake cap only looked at the command 0x4438. FACT (memory) + DERIVED (cause).
- Fixes (dev 1A598DC5, build x2, needs game exit): cap applies when command OR actuator > 0.3; drift starts at any
  speed right after the notice (1 m/s ramp above 120 km/h, speed-scaled steer). ai_pulloff 81 PASS, other gates PASS.
  Before-copy P15_LIVE7_BEFORE_2026-10-02.
- Not on the pull-off: Button stalled at the start (vanilla StartStalls, owner).
- INSTALLED 1A598DC5 2026-10-02 after game exit (hash verified); rollback INSTALLED_BEFORE_2026-10-02_LIVE7 = C91097B7.

## 18. Live trial 9 + session wrap-up (2026-10-02, installed 1A598DC5)
T9 Fisichella (slot 15, long straight, ~240 km/h, kill armed while paused): log `game kill -> engine stop`, `drift side R
goal 11.0 m` t=72, `parked` t=76. Memory: brake ACTUATOR 1.0 from t=73.3 (~1.5 s after death) despite the 0.3 cap on
command AND actuator; 237 -> 0 km/h in ~3 s; lat 0 -> 2.95 m (road R 7.4) = stopped ON the track. Owner: "full brake in
the middle of the track" - owner ended the session. FACT.

STATUS: pull-off NOT working live. Proven: hook installs; engine stop on game kill works; side choice + L/R sign correct
(Hill, owner); our absolute steer moves the car at moderate speed (Sato 7.7 m). Unsolved: a full brake 1.0 reaches the
physics ~1.5 s after every engine death, and the 0.3 cap in the AIMainUpdate post-hook does NOT stop it -> another writer
(outside AIMainUpdate, or after our hook in the tick) sets the brake actuator 0x3584. DERIVED.

Next agent (do first, offline/read-only before any build):
1. Find every writer of veh+0x3584 / 0x4438 (SDK `f1c_mod.py inspect`, xrefs to +0x3584 = 13700) - especially the
   dead-engine / ReasonCode 1 / speed-cap path - and where it runs relative to AIMainUpdate in MainGameTickUpdate 0x42C690.
2. Only then decide where to clamp (second hook after that writer, or the writer's own condition).
3. Until fixed: owner keeps panel "AI engines" OFF for normal racing (pull-off inactive = vanilla behaviour), or roll back
   to 2D588299/D7F23A4E from workspace_organization_audit/INSTALLED_BEFORE_*.
Owner design rules collected this session: full brake only at a halt; pull far off the line (option A: >= 3 m past the
AIW road edge, AIW wall only as an outer cap - AIW wall is not the barrier); no stop on the racing line.
Disposable trial tools: %TEMP%\mf_dyn\po_trial.py (+ po_trial_*.txt). Live scanner view: tools/wall_scan_gui.py.

## 19. Brake-writer search + brake GUI (2026-10-02, installed 1A598DC5)
- FACT (capstone scan of F1 Challenge Online.exe, every exec section): writers of veh+0x3584 are only AIMainUpdate
  (0x42557B fstp, 0x425644 park tail) and a patched code cave 0x59D709/0x59D718 (network/replay state unpack, same
  logic as sub_4C73F0 flag 0x4000000). No other write with displacement 0x3570-0x3590 touches 0x3584.
- FACT: only one call into AIMainUpdate: MainGameTickUpdate 0x42C748 (other refs = switch tables + an internal tail jmp).
  So the entry hook sees every AI call; the s18 "other writer" theory is unlikely.
- DERIVED: T9 log stayed DRIFT t=72-76 (no ABORT), DRIFT emits brake_cap 0.3, yet memory showed 1.0 -> the clamp was
  not applied (po_apply early return or po_share apply=0). Silent suspects: po_share_clear on chain loss (no log line),
  seqlock skip, g_pump_frozen. OPEN - next build should log po_apply skip reasons for an active slot.
- Tool: `tools/ai_brake_gui.py` (+ `ai_brake_gui.bat`) - pick AI car (default Montoya); Brake + Throttle panels, each slider 0-1 + own OVERRIDE (throttle 0x4434+0x357C); brake OVERRIDE writes
  0x4438+0x3584 ~1 kHz; live readback. External writer = flicker expected. Needs elevated shell if game elevated.
  Disposable monitor: %TEMP%\mf_dyn\brkmon.py.
- Owner live 2026-10-02 (Montoya, ai_brake_gui, elevated): brake + throttle override held to fixed values forced a crash;
  "AI clearly tried to brake but wasn't able to". FACT (owner observation): an external ~1 kHz writer on cmd+act wins
  over the AI's per-tick brake/throttle write well enough to control the car (steering flicker, s2, is a separate case).
- CORRECTION (live 2026-10-02, Coulthard slot 13, brake 0 / throttle 1 forced by ai_brake_gui): car still slowed in gear 7
  at -18 m/s2 (longAcc veh+0x3554) with sampled thr 1.00 / brk 0.00; one sample caught brk 0.20 thr 0.00. AIMainUpdate
  copies brake cmd->actuator with no ramp (decompile l.161-165), and physics consumes it in the same thread right
  after. DERIVED: an external writer wins only for physics steps after its write; the AI's own value still acts every
  tick -> partial control only. The earlier "AI couldn't brake" crash = partial control. Full control needs an
  in-process write right after AIMainUpdate (the pull-off hook position) - which also means the hook's 0.3 cap should
  fully work, so the T9 full brake points to po_apply skipping (s19 above). Monitor: %TEMP%\mf_dyn\dcmon.py.

## 20. Diagnostic + in-hook override build (2026-10-02, dev dll 726D2A2A, NOT installed yet)
Changes (engine_failure.c only):
- A (diag): po_apply counts its outcome per ACTIVE pull-off slot (ctl-off, session/frozen, seq-skip, apply0, capped,
  brake-set, under-cap); worker logs once per second `AI PULLOFF DIAG: slot N state S brk act/cmd | counts` or
  `NO HOOK CALLS`. Chain-scan-empty release now logs (`chain scan empty -> output cleared`).
- B (override): named mapping `Local\MF_AI_OVERRIDE_V1` (AiOverride: magic 'MFAO', slot, brake_on, brake, throttle_on,
  throttle, hook_calls). Applied in the AIMainUpdate post-hook AFTER po_apply = full control of one AI car.
  tools/ai_brake_gui.py uses it when present (status "MODE: DLL hook", hook write counter), else external fallback.
Verification (offline): build -Werror clean; ai_pulloff_test 81/0 fail, ai_react_test 40 PASS, engine_model_test
67577 PASS, msg_test 57 PASS. GUI starts. Live: pending (install needs game exit; rollback = 1A598DC5).
- INSTALLED 726D2A2A 2026-10-02 after game exit (hash verified); rollback workspace_organization_audit/INSTALLED_BEFORE_2026-10-02_S20 = 1A598DC5.
- Owner live 2026-10-02 (726D2A2A, ai_brake_gui MODE DLL hook): brake 0 / throttle 1 via the in-hook override -> AI
  "just drives straight ahead into a wall". FACT: a write in the AIMainUpdate post-hook gives FULL brake/throttle
  control (unlike the external writer). So the pull-off cap at the same position can work; T9 failure = cap skipped.

## 21. Live trial 10 - Montoya engine kill (2026-10-02, installed 726D2A2A)
Kill write veh+0x3D70=0.5 at t=302.9 (slot 5), engine death in a CORNER. Log + 20 Hz monitor:
- t=303 notice: AI brake reached actuator in ~half the samples (max 0.20; DIAG apply0 = notice phase, by design).
- t=303-306 DRIFT side R goal 12.5 m: brake 0.00 throughout, DIAG `under-cap` = cap path running. FACT: the brake cap
  works when po_apply runs; speed 147 -> 36 km/h with no brake (dead engine + drag).
- Steering weaved -0.16/-0.22/-0.40 then +0.26/+0.40 (absolute steer, mf_po_abs_steer).
- t=306 ABORT (heading > 60 deg) -> brake 1.0 at 36 km/h; t=307 parked (mode 4); t=309 chain scan empty (wp 0).
- Owner: car "got pooped mid corner and is now stuck in the corner itself" = stopped ON the track in the corner.
DERIVED cause: mf_po_abs_steer steers to heading-vs-current-waypoint with no corner curvature feed-forward and full
gain below 60 km/h -> weave in a corner -> heading abort -> full brake on the racing line. T9 (straight, 240 km/h)
full brake still unexplained by this trial (different path); DIAG will show it if it repeats.
Next (owner decision): (a) add curvature feed-forward (follow the track's bend) + lower low-speed gain, (b) in a corner
don't abort on heading vs one waypoint - use a look-ahead heading, (c) abort = light brake + keep steering, not 1.0.

## 22. Steering override + keyboard drive (2026-10-02, dev dll 7F8E47A6, NOT installed)
- AiOverride gains appended steer_on + steer (-1..1 -> cmd 0x442C + act 0x3580), applied in the same post-hook.
  Old GUIs (28-byte layout) leave them 0. Build -Werror clean; GUI starts (live game, old dll).
- tools/ai_brake_gui.py (title "AI car control"): Brake / Throttle / Steering panels, invert-steering box, KEYBOARD
  DRIVE (I throttle, K brake, J/L steer ramp 2.5/s, centre 4/s; read via GetAsyncKeyState so it works with the game
  focused; turns all three overrides on). Sign of +steer on screen not yet confirmed (invert box covers it).
- Known limit: the GUI cannot tell an old dll from a new one (mapping is page-sized); with 726D2A2A steering is ignored.
- Live: pending install (game exit). Rollback 726D2A2A = dll on disk before this build? NO - the installed file is the
  rollback; keep a before-copy at install time.
- INSTALLED 7F8E47A6 2026-10-02 after game exit (hash verified); rollback workspace_organization_audit/INSTALLED_BEFORE_2026-10-02_S22 = 726D2A2A.

## 23. Gear override + per-control checkboxes + API doc (2026-10-02, dev dll F67D486D, NOT installed)
- Owner: AI still "fights" steering with 7F8E47A6. Live read (slot 7, steer override 0): 1935 reads in 3 s, actuator and
  command == our value every time. Exe scan: only AIMainUpdate writes 0x3580 (plus net-unpack cave); nothing outside
  AI code reads 0x442C; SteeringAngleController 0x425670 writes no steering. DERIVED: no second steering path in the
  input chain; the felt "fight" is still unexplained (candidates: GUI auto-centre, physics self-aligning). GUI now
  shows a live steer-fight meter to prove it either way.
- Gears: detour on AI auto-shift 0x426330, shifts via 0x4EF930(veh+0xC, gear) - see docs/AI_OVERRIDE_API.md.
- GUI: checkbox per Throttle/Brake/Steering/Gears ("user controls"); keys act only on ticked controls; U/O shifts;
  Tk state copied to plain flags on the main thread for the worker.
- Owner intent: AiOverride = future ABI for AI driving manipulation / new AI driving -> docs/AI_OVERRIDE_API.md
  (append-only layout).
- Verification: build -Werror clean; GUI syntax ok, waits for cars (game in menus). Gates not re-run (headers
  untouched). Live: pending install.
- INSTALLED F67D486D 2026-10-02 after game exit (tasklist empty, hash verified); rollback workspace_organization_audit/INSTALLED_BEFORE_2026-10-02_S23 = 7F8E47A6.
