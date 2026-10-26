# Claude review 2 — REVIEW_1 fixes + B3 group 1 + B4 (2026-10-02)

Verdict: **PASS with additions.** Re-run: B1 test OK, verify_maps 0 mismatches. Maps are well cited. But the key
meaning of AIRacingLine is missed because three callees are misnamed. Claude read source L205-L349 + the callees.

## Corrections (Claude, DERIVED from raw source — DS: copy into maps as "Claude review" notes, do not re-tier)

1. `AIComputePassScore 0x42A780` is NOT a pass score. It is the **waypoint corner-speed limit** (m/s):
   `v = (TireForceSolver(wp) * veh[0x4504] + veh[0x4500]) * veh[0x4580] * veh[0x453C] * wp[+0x84]`
   (float index *4: a1[4417]=0x4504, a1[4416]=0x4500, a1[4448]=0x4580, a1[4431]=0x453C, a3[33]=wp+0x84).
   Same shape as sub_426430's speed ratio (0x4504/0x4500, cap 75). 0x453C = AIAbility dynamic multiplier (prior art).
2. The AIRacingLine profile loop is a **braking look-ahead**, not "energy decay": v46 starts at current speed 0x4440;
   for each waypoint ahead, target v50 = corner speed of that waypoint (or pit speed flt_8ACFB4 if wp+308 == 1, or 12.0
   at a branch); then v46 -= wp_len(+60) / v46 * 0x44CC * v50 * 0.915 (speed lost braking over that segment). It stops
   when the projected speed is still at/above a waypoint's target (`v46 >= v50`) = braking must start NOW:
   v46 > 0 path -> brake 0x4438 = TireGripCoefficient (ramp +0.2 per tick), throttle 0x4434 = 0. If the projection
   reaches 0 first (every corner ahead is reachable), v46 <= 0 path -> brake 0, throttle 0x4434 = 1.0 / hold value /
   0.4-0.5 off-line, ramped by 0.2 * 0x44B8 (race start) or 0x44C4. The 20 projected speeds go to 0x43A4..0x43F0
   (this+17316 + 4*i). 0x43F8 = limiting waypoint ptr, 0x43FC = its target speed (9999 = none). 0x44CC = braking
   decel factor. 0x445C (17500) byte = car inside track width +1.5 m.
3. `BrakeThermalModel 0x4E87E0` (this = veh+0xC) is NOT thermal: it returns the 0..1 **throttle needed to hold
   speed** (drag-like v^2 term), clamped; used as throttle. `TireGripCoefficient 0x4E88B0` is the **brake amount**
   (written to 0x4438). Rename both in the maps as GUESS names `AIHoldThrottle` / `AIBrakeAmount`, keep old names.

## Required additions (DeepSeek)

- Add B3 maps for `AIComputePassScore_0042a780.c`, `BrakeThermalModel_004e87e0.c`, `TireGripCoefficient_004e88b0.c`
  to group 1. For `this = veh+0xC` functions, give both the raw index and veh-relative hex (index*4 + 0xC).
- Add a section "Corner speed and braking — one page" at the top of map_AIRacingLine.md that states the formula above
  with each input field, and lists inputs still unnamed.
- Add to LIVE_LOG_PLAN (B6): log 0x4440 speed, 0x43FC target speed, 0x43A4..0x43F0 profile, 0x43F8, 0x4434/0x4438, 0x44CC,
  0x4500/0x4504/0x4580/0x453C, for one car through one slow corner.
- Then continue group 2. Next stop as planned.
