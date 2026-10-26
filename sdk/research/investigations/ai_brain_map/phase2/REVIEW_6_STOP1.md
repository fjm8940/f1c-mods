# Claude review 6 — phase 2 stop 1 (C2 + C3), 2026-10-02

Verdict: **PASS with a wording correction.** verify_maps_phase2: 193 files in maps/, 0 mismatches (re-run).

## C2 correction (Claude, DERIVED from AIRacingLine L224-L226 + L297-L299)

Data is right; the conclusion is phrased backwards. AIRacingLine sets 0x43F8 = 0 and 0x43FC = 9999 at the start of
EVERY tick and writes them only when it takes the brake path. So "limiting waypoint null" simply means "the planner
chose throttle this tick". The flicker is the AI's real decision: near the corner target it alternates brake / throttle
tick by tick (bang-bang control around the braking curve), not a reset artefact. Your table shows exactly that
(brake 0.20-0.40 / throttle 0.15-0.67 alternating while speed approaches 103.6 km/h).
- Edit C2_BRAKING_FLICKER.md "Answer": replace "cmd_brake is never on while a limiting waypoint is set" with
  "cmd_brake is never 0 while a limiting waypoint is set; brake ticks and limiting-waypoint ticks are the same ticks".
- Add the above explanation as "Claude review (DERIVED)".

Follow-up already done by Claude: brain_core EventDetector now reports one braking zone per corner (ends after 0.5 s
sim time without brake) with a pulse count. Live check 12 s Klien: 3 zones (1.27 s / 4 pulses, 1.04 s / 0, 0.19 s / 1).

## C3

Accepted. Keep the extractor-gap notes. Continue with C4 + C5.
