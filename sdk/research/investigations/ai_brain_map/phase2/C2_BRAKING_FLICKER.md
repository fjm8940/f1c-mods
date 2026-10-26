# C2 — braking flicker: per-tick analysis (2026-10-02)

Question (phase-2 handoff): Claude saw `braking_start`/`braking_end` alternate
every few ticks in one braking zone. Does `cmd_brake` itself drop to 0 between
ticks, or does `limiting_waypoint_ptr` / `braking_target_speed` reset (9999)
while brake stays on?

Method: read-only analysis of the two retained logs with
`analysis/c2_braking_flicker.py` (no new reader, no game access):
`%TEMP%\ai_brain_live\viewer_20261002T151813Z` (GUI, Christian Klien, 64 samples)
and `%TEMP%\ai_brain_live\smoke` (recorder, Klien + Jenson Button, 8 s).
`speed_profile[0]`, `current_waypoint_ptr`, `cmd_throttle`, `cmd_brake`,
`limiting_waypoint_ptr` and `braking_target_speed` are read from the snapshots.
Note: the handoff's "Klien, 260 -> 172 km/h" zone is not in either retained log.
The retained viewer zone is Klien 122 -> 101 km/h; the smoke zone is Button
293 -> 102 km/h with the release at 256 -> 246 km/h. The event pair semantics
(`derived.braking.active` = limiting waypoint set AND target < 9000 AND
cmd_brake > 0, `brain_core.py` L169) are taken as given.

## Answer

`cmd_brake` is never 0 while a limiting waypoint is set; brake ticks and
limiting-waypoint ticks are the same ticks. The brake drops to 0 in exactly the
ticks where `limiting_waypoint_ptr` becomes 0 and `braking_target_speed` becomes
9999. Per the Claude review below those null ticks are simply the ticks where
the planner chose throttle, not a reset artefact: the flicker IS the native
bang-bang brake/throttle decision around the braking curve.

Whole-run state combos (corner-brake logic; `lim` = limiting waypoint pointer):

| run / driver | lim set + brake>0 | lim set + brake==0 | lim null + brake>0 | lim null + brake==0 | brake transitions |
|---|---|---|---|---|---|
| viewer / Klien (64 samples) | 8 | 0 | 0 | 56 | 9 |
| smoke / Button (158 samples) | 42 | 0 | 7 | 109 | 23 |

`lim set + brake==0` is 0 in both runs: the brake command is never 0 while a
limiting waypoint is set; brake ticks and limiting-waypoint ticks are the same
ticks. The 7 `lim null + brake>0` Button samples are not corner braking: every
one has `speed_limit` 15.2..25.4 m/s and speed far above it, i.e. the pedal
post-processor's speed-limit branch (0x4484, doc 54 section 3), which writes
0x4438 while `limiting_waypoint_ptr` is 0.

## Claude review (DERIVED, 2026-10-02 — REVIEW_6_STOP1)

Data accepted; conclusion corrected: `AIRacingLine` sets 0x43F8 = 0 and
0x43FC = 9999 at the start of EVERY tick and writes them only when it takes the
brake path. "Limiting waypoint null" therefore just means "the planner chose
throttle this tick", not "a field reset". The flicker is the AI's real decision:
near the corner target it alternates brake / throttle tick by tick (bang-bang
control around the braking curve). The per-tick tables above show exactly that
(brake 0.20-0.40 vs throttle 0.15-0.67 alternating while speed approaches
103.6 km/h). Follow-up already done by Claude: `brain_core` EventDetector now
reports one braking zone per corner (ends after 0.5 s sim time without brake)
with a pulse count; live 12 s Klien showed 3 zones (1.27 s / 4 pulses,
1.04 s / 0 pulses, 0.19 s / 1 pulse).

## Per-tick pattern A — viewer, Klien, target 103.6 km/h (events 1811.02..1811.99)

| sim_t | km/h | thr | brk | target | lim ptr | cur wp | prof0 | look |
|---|---|---|---|---|---|---|---|---|
| 1810.778 | 147.2 | 0.00 | 0.81 | 34.1 | 0x19A32CD4 | 0x19A327E4 | 40.9 | 2 |
| 1810.913 | 133.1 | 0.00 | 0.84 | 34.1 | 0x19A32CD4 | 0x19A32920 | 37.0 | 2 |
| 1811.021 | 122.4 | 0.19 | 0.00 | 9999.0 | 0x0 | 0x19A32920 | 34.0 | 2 |
| 1811.124 | 119.3 | 0.00 | 0.20 | 28.8 | 0x19A331C4 | 0x19A32A5C | 33.1 | 2 |
| 1811.228 | 116.7 | 0.52 | 0.00 | 9999.0 | 0x0 | 0x19A32B98 | 32.4 | 2 |
| 1811.336 | 112.9 | 0.15 | 0.00 | 9999.0 | 0x0 | 0x19A32CD4 | 31.3 | 2 |
| 1811.458 | 109.5 | 0.00 | 0.40 | 28.8 | 0x19A331C4 | 0x19A32CD4 | 30.4 | 2 |
| 1811.563 | 104.5 | 0.49 | 0.00 | 9999.0 | 0x0 | 0x19A32E10 | 29.0 | 1 |
| 1811.665 | 104.6 | 0.00 | 0.40 | 28.8 | 0x19A331C4 | 0x19A32E10 | 29.1 | 1 |
| 1811.773 | 102.0 | 0.67 | 0.00 | 9999.0 | 0x0 | 0x19A32F4C | 28.3 | 1 |
| 1811.876 | 103.9 | 0.00 | 0.40 | 28.8 | 0x19A331C4 | 0x19A33088 | 28.9 | 1 |
| 1811.985 | 101.1 | 0.27 | 0.00 | 9999.0 | 0x0 | 0x19A33088 | 28.1 | 1 |
| 1812.094 | 100.5 | 0.24 | 0.00 | 9999.0 | 0x0 | 0x19A331C4 | 27.9 | 1 |

The same waypoint 0x19A331C4 (target 103.6 km/h) is bound on 1811.124, 458,
665, 876 and null on the ticks between. When bound, brake ramps 0.20 -> 0.40;
when null, brake 0 and throttle 0.15..0.67. The car sits within +/-0.4 km/h of
target, so the projection test flips on nearly every tick (bang-bang at corner
speed); the events fire on each flip.

## Per-tick pattern B — smoke, Button, release at 256 -> 246 km/h

| sim_t | km/h | thr | brk | target | lim ptr | cur wp | prof0 |
|---|---|---|---|---|---|---|---|
| 1618.596 | 277.8 | 0.00 | 1.00 | 71.6 | 0x19A2DB5C | 0x19A2D530 | 77.2 |
| 1618.645 | 271.5 | 0.00 | 0.20 | 71.6 | 0x19A2DB5C | 0x19A2D66C | 75.4 |
| 1618.699 | 268.4 | 0.00 | 0.60 | 71.6 | 0x19A2DB5C | 0x19A2D7A8 | 74.5 |
| 1618.748 | 264.2 | 0.00 | 0.20 | 71.6 | 0x19A2DB5C | 0x19A2D7A8 | 73.4 |
| 1618.802 | 261.1 | 0.00 | 0.60 | 70.2 | 0x19A2DC98 | 0x19A2D8E4 | 72.5 |
| 1618.851 | 256.3 | 0.00 | 1.00 | 70.2 | 0x19A2DC98 | 0x19A2DA20 | 71.2 |
| 1618.899 | 251.2 | 0.68 | 0.00 | 9999.0 | 0x0 | 0x19A2DA20 | 69.8 |
| 1618.954..1619.360 | 250.0..246.7 | 0.62..0.68 | 0.00 | 9999.0 | 0x0 | 0x19A2DB5C..0x19A2E188 | 69.4..68.5 |
| 1619.414 | 246.3 | 0.00 | 0.20 | 42.7 | 0x19A2F058 | 0x19A2E2C4 | 68.4 |
| 1619.463 | 243.9 | 0.00 | 0.60 | 42.7 | 0x19A2F058 | 0x19A2E400 | 67.7 |
| 1619.518 | 238.7 | 0.00 | 1.00 | 48.6 | 0x19A2EF1C | 0x19A2E400 | 66.3 |

Here the limiter first switches between two adjacent waypoints (0x19A2DB5C,
target 71.6; 0x19A2DC98, target 70.2) while the brake re-ramps 0.2/0.6 each
tick, then goes null for 0.5 s while the car coasts at 68% throttle
(251 -> 247 km/h), then rebinds to a different corner (0x19A2F058, target
42.7). `speed_profile[0]` tracks the measured speed in both patterns, so the
profile is not the field that resets.

## Interpretation (GUESS, static cross-reference only)

- The null/9999 ticks contain no waypoint whose target limits the projected
  speed, so `AIRacingLine` (0x425920) leaves 0x43F8 = 0 and 0x43FC = 9999 for
  that tick; the pedal stage then produces throttle instead of brake. When it
  rebinds, brake ramps +0.2/tick (doc 54 section 3b / LIVE_LOG_PLAN expected
  pattern 2). So the visible flicker is the look-ahead controller re-electing
  (or dropping) the binding corner per tick, not output noise.
- The two shapes (viewer: same waypoint bound/null at target speed; smoke:
  adjacent waypoints then a null stretch at 250 km/h) are both consistent with
  the per-tick projection crossing a waypoint target; the smoke null stretch is
  10 ticks and is the more suspicious one (brake released at 251 km/h for a
  42.7 m/s target corner further ahead).
- Not settled here (needs C1 Run A live data): whether the oscillation period
  matches the `len/v*0x44CC*target*0.915` projection exactly, and whether the
  null stretch is a waypoint-pointer handoff artifact in `sub_426430`.

Raw logs stay in `%TEMP%\ai_brain_live\`; this record is the summary.
