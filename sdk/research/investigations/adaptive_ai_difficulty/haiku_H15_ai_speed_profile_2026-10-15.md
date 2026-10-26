# H15 - AI speed profile around a lap (lap-time recipe ingredient 3)

Date: 2026-10-15. Agent: Haiku H15 (static only). Read-only: no game, poke, lab, memory-MCP or live tool.
Only this file was written; no edits to findings.jsonl, indexes or other maps. Proposed entries: section 9.
Paths relative to `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\`. Labels: FACT (cited source line), DERIVED (inference), OPEN.
Field names: decimal index x4 = byte offset (4403 -> 0x44CC = 17612).
## 0. Verdict

- Per-tick planner over the AIW waypoint chain (`AIRacingLine` 0x425920). Look-ahead at most 20 waypoints (FACT); metres =
  sum of AIW segment lengths, so per track (DERIVED).
- Corner target = `(TireForceSolver(wp)*CorneringMult + CorneringAdd) * wet * pace * wp.path` (FACT).
- Braking = forward projection: speed drops per segment by `len/v * 0x44CC * 0.915 * F(v)`, compared with the next target.
- No per-tick AI top-speed cap found. 0x4484 governors act only on nearby opponents; top speed = physics plus throttle
  ceiling 0x457C (about 0.97-1.0) (FACT code, DERIVED outcome). Corrections: section 8.
## 1. Cold start and checks

- Read `AGENTS.md`, root `README.md`, `governance\README.md`, `governance\PIPELINE.md`. Structure validator: 2 pre-existing
  ERRORS (".claude" and "plans" top-level dirs), not from this task, not touched.
- Prior art (not re-derived): `ai_brain_map/map_AIRacingLine.md`, `map_AIComputePassScore.md`, H3, H5, H6, DROPPED_ST0_SCAN line 15.
- SDK front door (read-only): `tools/f1c_mod.py start --json` reports `freshness.stale: true`; its checkpoint objectives are
  unrelated to AI speed. `evidence-query findings` gave 0 hits for "speed profile braking", "governor speed 0x4484",
  "corner target braking", and 11 reviewed hits for "0x44CC" (Passing field, `rcd-parser-writes-vehicle-struct`).
## 2. Input sources (FACT unless marked)

| Field | Index / hex | Written where | Scope |
|---|---|---|---|
| CorneringMult | 4417 / 0x4504 | `sub_424080` L122-L127 (.rcd CORNERINGMULT) | per-driver |
| CorneringAdd | 4416 / 0x4500 | `sub_424080` L116-L120 | per-driver |
| Wet factor | 4448 / 0x4580 | `sub_424080` L148-L150: `1 - flt_8AFEE8*RainAbility*0.01*k` | per-driver |
| Pace factor | 4431 / 0x453C | init 1.0 (`AIDriverStructInit` L55); REWRITTEN EVERY FRAME by `AIAbility` L40-L68 (+/-0.01), L92 | per-car, live |
| Pace target | 4413 / 0x44F4 | `sub_424080` L151-L152: `(1 - RA/100)*k` | per-driver |
| Damp / throttle ramp | 4401 / 0x44C4 | `sub_424080` L154-L158: `(22-RA)/22`, times `(s/100)^2` if slider s < 100 | per-driver + slider |
| Race-start ramp | 4398 / 0x44B8 | `sub_424080` L164-L175 (RandLCG term) | per-driver |
| Passing (decel) | 4403 / 0x44CC | L56 default 50.0 (hex 0x42480000); L110-L114 .rcd "Passing"; L160-L163 `x*0.001+0.9`, cap 1.0 | per-driver |
| Target multiplier | 4437 / 0x4554 | init 1.0 (`AIDriverStructInit` L56, `CameraViewportFrustumInitialization` L45); no other writer | constant 1.0 |
| Speed | 4368 / 0x4440 | vehicle | per-tick |
| Throttle output | 0x4434 (17460) | `AIRacingLine` L193, L339, L347; clamped `AISteeringCorrection` L131-L133 | output |
| Brake output | 0x4438 (17464) | `AIRacingLine` L175, L306, L346; `AISteeringCorrection` L82-L100 | output |
| Brake-trigger speed | 0x4484 (17540) | RESET 999999 each tick: `AIMainUpdate` L244; init `AIDriverStructInit` L62 | per-tick, opponent |
| Speed cap (command queue) | 0x4538 (17720) | init 999999 (`AIDriverStructInit` L63); set by commands (`ParameterValueSetter`) | command-driven |
| Throttle ceiling | 0x457C (17788) | `AIAbility` L12-L78 (about 0.97-1.0); `AISteeringCorrection` L22-L23 (x0.75 if flag 0x45FC) | slider + flag |
| Race-start flag | byte_5C3160 | set 1 `AIMainUpdate` L100; cleared `AIRacingLine` L297 | global |
| Strength slider | dword_7D01A0 | global | global |
| Waypoint length / path / corridor | wp+60, wp+132, wp+80/84 | AIW (read `AIRacingLine` L264, `AIComputePassScore` L4, L302-L303) | per-track |
| Tyre-force args | wp+112/116/120 | AIW | per-track |
| Pit flags | wp+308 (mode), wp+312 (speed) | AIW | per-track |
| Tyre tables / PLR corner grip (0.93) | veh+0xC base, this[3397..3921]; PLR | car setup; PLR has no consumer found (H5) | per-car; global OPEN |
## 3. Braking decision and decel model

### 3a. Planner loop (`AIRacingLine.c`)

- L213: `v46 = speed`. L226: target sentinel 9999 (0x43FC).
- L233-L234: loop top, `if (v46 >= v50) break`.
- L236-L240: next waypoint on the chain; branch chosen by CallToBox 0x454C (wp+276 vs wp+272).
- L243-L245: pit waypoint (wp+308 == 1) -> target = flt_8ACFB4. L249-L255: linked car blocks -> target 12.0.
- L259-L261: target = `AIComputePassScore(next wp)`.
- L263-L264: decel, source literal: `v46 -= wp[60]/v46 * 0x44CC * v32 * 0.915` (v46 > 1.0), floored at 0 below 0.5.
- L267-L272: store profile[i] (20 slots 0x43A4..0x43F0). L274: exit when v46 <= 0.
- L286-L293: unfilled slots padded with -1.8 m/s each. Break semantics (brake when the projection still exceeds the next target) follow the source and the 2026-10-02 map; not re-verified here.

### 3b. What F is (OPEN on meaning, FACT on correction)

- Hex-Rays prints `v32` (target) in the decel. `DROPPED_ST0_SCAN.md` line 15 (asm at 0x425EFC, `fmul st(1)`) says the
  factor is the TireForce return `F(v) = TireForce(v, wp+116, wp+120, wp+112)` (`TireForce_004e9100.c` L33, L111). Not re-run here;
  docs/54 3b live replay supports it (median 1.000, n=1936).
- DERIVED: `a(v) = K*F(v)` with `K = 0x44CC*0.915`, so K is about 0.87 (Passing 50) to 0.915 (Passing >= 100).
- Units of F not fixed by the corpus (OPEN).

### 3c. Brake and throttle outputs (`AIRacingLine.c` L301-L347)

- L301-L303: corridor flag 0x445C = `(wp[84]+1.5 > lat) && (wp[80]+1.5 > -lat)`.
- Loop broke with v46 > 0 (L343-L347): brake = TireGripCoefficient(veh+0xC), ramp <= prev+0.2/tick; throttle 0.
- Profile exhausted (L304-L339): brake 0; throttle target v42 = BTM if v26 (L205); else 1.0 in corridor (BTM if 0x446C);
  else 0.4 (speed <= 25) or 0.5 (L307-L326). Ramp-up `0.2*0x44C4` per tick (`0x44B8` race start); ramp-down instant.

### 3d. Horizon

- At most 20 waypoints; stops early when projected speed < 0.5 (L265) (FACT). Metres = sum of wp[60] (per track, DERIVED; OPEN for Monza).
## 4. Corner target

- `AIComputePassScore.c` L3-L4 (FACT): `target = (TFS(wp)*0x4504 + 0x4500) * 0x4580 * 0x453C * wp[+0x84]`,
  `TFS = TireForceSolver(wp+116, wp+120, wp+112, 16.0)`; root-finder, 128 speed clamp (`TireForceSolver_004e8ce0.c` L80), not binding.
- Live-confirmed on two tracks (D1; median 0.9993). Mult moves brake points via the comparison (3a); decel (3b) has no mult.
## 5. Governors and top speed

### 5a. 0x4484 is per-tick and per-opponent

- `AIMainUpdate.c` L243-L249: opponent scan (L243), reset 0x4484 = 999999 (L244), `AIRacingLine` (L245), `AISteering`
  (L247, if this[17744] < dword_5EA2CC), `sub_426100`, `AISteeringCorrection` (L249).
- `AISteering_004270a0.c`: about 14 `TrackParameterMinimumClamp(x)` sites on proximity slot records `v33 = &this[68*slot]`
  (L580-L605, L690-L701, L735-L759). L696/L737 gate on the OPPONENT's 0x4444 ratio (`*(v33+4189)+17476`). Clamp is [5, 0x4538]
  (`TrackParameterMinimumClamp_0042a740.c` L3-L6).
- Brake bands (`AISteeringCorrection.c` about L82-L100): 0x4484+10 -> full brake; +3 -> half; +1 -> throttle 0; 0..+1 -> 0.8*BTM.
- DERIVED: with no opponent slot, 0x4484 stays 999999, so no band fires. Governors do not cap an empty straight.
  OPEN: whether slots exist on straights in a normal race (`OpponentProximityScanTeamRecognition` not decoded here).

### 5b. Other speed items (none is a straight cap)

- `sub_426430` L139-L149: `v18 = speed*0x4504 + 0x4500`, capped at 75.0; ratio 0x4444 = speed/v18. Feeds only the opponent gate.
- Throttle ceiling 0x457C (`AISteeringCorrection.c` L131-L133): when byte_5C3160 is clear, `0x4434 = min(0x4434, 0x457C)`.
- Steer cut (`AISteeringCorrection.c` L24-L28): if speed > 20 and |steer| > 0.1, throttle *= (1 - 0.7*|steer|).

### 5c. Top speed answer

- Straight targets are TFS-based and high (solver bound 128), so they do not bind (DERIVED). Top speed = physics limited by
  the 0x457C ceiling; no per-tick AI cap found. OPEN: `PhysicsGearShiftLogicWithThresholds`, `sub_426100`, command-queue 0x4538.
## 6. Per-driver constants

- 0x44CC: 50.0 default (L56), overwritten by .rcd Passing (L110-L114), then 0.9 + 0.001*x capped at 1.0 (L160-L163).
  DERIVED: 0.95 default, 1.0 for Passing >= 100; one call site, `AIDriverStructInit` L94. The earlier 0.901 idea is WRONG.
- 0x44C4 damp: `(22-RA)/22` times `(s/100)^2` for s < 100 (L154-L158; H6 agrees). 0x4580 wet: L148-L150.
- 0x4504 / 0x4500: CORNERINGMULT / CORNERINGADD (L116-L127). Value when the key is absent: OPEN.
- 0x453C pace: walked each frame by `AIAbility` toward 0x44F4 (= `(1-RA/100)*k`) in 0.01 steps (L40-L68). Live value tracks a moving target.
## 7. Algorithm (pseudo-code)

```
# AIMainUpdate.c L243-L249 (non-pit path, per tick)
veh[0x4484] = 999999                                  # L244
OpponentProximityScan(veh)                            # L243 -> slots
AIRacingLine(veh)                                     # L245 -> 0x4434 thr, 0x4438 brk
if veh[0x4744] < dword_5EA2CC: AISteering(veh)        # L247 -> clamps 0x4484 per slot
AISteeringCorrection(veh)                             # L249 -> steer cut, thr ceiling, brake bands

# AIRacingLine.c L204-L347 (planner)
def target(wp): return (TFS(wp)*veh[0x4504] + veh[0x4500]) * veh[0x4580] * veh[0x453C] * wp[+0x84]  # AIComputePassScore
v = veh[0x4440]; tgt = target(next) * veh[0x4554]     # 0x4554 = 1.0
for i in range(20):                                    # slots 0x43A4..0x43F0
    if v >= tgt: break                                 # L234 -> brake branch
    wp = next_on_chain(wp)                             # L236-L240, branch by 0x454C
    tgt = flt_8ACFB4 if wp.pit else (12.0 if linked_car_blocks(wp) else target(wp))   # L243-L261
    v -= wp.len / v * veh[0x44CC] * TF(v, wp) * 0.915  # L264 (TF per 3b)
    if v < 0.5: v = 0
    profile[i] = v                                     # L267-L272
    if v <= 0: break
if v > 0:  brake = min(TGC(veh+0xC), prev_brake + 0.2); thr = 0          # L343-L347
else:      thr_t = BTM if v26 else (1.0 if corridor else (0.4 if spd <= 25 else 0.5))   # L304-L326
           ramp = 0.2*(0x44B8 if race_start else 0x44C4)                  # L328-L336
           thr = thr + min(ramp, thr_t - thr) if thr_t > thr else thr_t  # ramp up, instant down

# AISteeringCorrection.c (after planner)
if spd > 20 and |steer| > 0.1: thr *= 1 - 0.7*|steer|                    # L24-L28
if not race_start: thr = min(thr, veh[0x457C])                           # L131-L133
brake bands vs veh[0x4484]: +10 full, +3 half, +1 coast, 0..+1 0.8*BTM   # about L82-L100

## 8. Corrections to prior work

1. H5 sections 3c, 3g and 4 name the throttle output 0x4460 and brake output 0x4464. Per source, throttle = 0x4434
   (`AISteeringCorrection.c` L24-L28, L131-L133; `AIRacingLine.c` L193, L339) and brake = 0x4438 (`AIRacingLine.c` L175,
   L346; `AISteeringCorrection.c` brake bands). 0x4460 (17504) and 0x4464 (17508, written at SC L77) are other fields.
2. H5 3f: the governor ratio gate uses the OPPONENT's 0x4444 (`AISteering_004270a0.c` L696, L737), not "this".
3. H5 3e omits that 0x4484 is reset to 999999 every tick (`AIMainUpdate.c` L244).

## 9. Proposed findings (NOT appended; read-only scope; owner to approve)

- `ai-speed-planner-horizon-2026-10-15` FACT: 20-waypoint look-ahead, -1.8 m/s pad (AIRacingLine L222, L267, L286-L293).
- `ai-brake-decel-model-2026-10-15` DERIVED: decel = 0x44CC*0.915*TF(v) per AIW segment (TF asm not re-verified).
- `ai-0x44cc-passing-transform-2026-10-15` DERIVED: 0x44CC = min(1, 0.9+0.001*Passing); default 0.95 (sub_424080 L56, L110-L163).
- `ai-no-straight-speed-cap-2026-10-15` FACT-code / DERIVED: 0x4484 reset per tick (AIMainUpdate L244), governors per-opponent; no straight cap.
- `ai-h5-field-labels-correction-2026-10-15` FACT: throttle 0x4434, brake 0x4438 (H5 used 0x4460/0x4464).

## 10. Next static steps and live test (not done)

1. Check the asm at 0x425ED9-0x425EFC directly; settle the TF units (m/s^2 or force).
2. Decode `OpponentProximityScanTeamRecognition`: does a slot ever exist on an empty straight? (Test of 5a.)
3. Decode `PhysicsGearShiftLogicWithThresholds` and `sub_426100`; re-verify the break semantics (section 3a, OPEN).

## 11. Limitations and documentation gate

- Static only; decel relies on a prior asm check; AIW spacing and TireForce units OPEN.
- Documentation gate: this file is the record. Index entry and `findings.jsonl` append (section 9) await owner permission.
