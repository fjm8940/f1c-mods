# RECORDER_REQUESTS — phase 3

Fields the phase-3 analyses need that the recordings do not provide (Claude implements; read-only
recorder, no game writes). Each request names the analysis that needs it.

| # | Prio | Field | Address | Why | Status |
|---|---|---|---|---|---|
| R1 | P2 | per-car command-queue write index | `veh+0x44B0` (int, finding `f1c-command-queue-is-per-car-not-global-ring`: -1 = empty) | `overview.jsonl` logs all 62510 samples of `command_queue_write_index` as NaN in watch_race4fullc + watch_race4fulld (D3.2); the field cannot be used to follow queue activity | CLOSED 2026-10-02 by Claude: registry v1.2 types 0x44B0 as int (REVIEW_15) |
| R2 | P2 | raw wheel-slip inputs + traction cut target | `veh+0x38C4`, `veh+0x37CC` (float), `SuspensionRatioClamp(veh+0xC)` return | D9: the logged `slip_blend_last` (0x4464) is already the post-blend smoothed value; the pre-correction throttle (0x4434) and the cut target are not logged, so the exact cut magnitude in AISteeringCorrection cannot be replayed. With the two raw inputs and the clamp return the formula `throttle = (clamp-throttle)*clamp(2-2.2222*smoothed,0,1)+throttle` can be verified like D1/D11 | OPEN (proposed in D9; no recorder change yet) |

No other field was missing for D1 or D3: the corner-speed replay (D1) found every table entry it
needed in `cars_static.jsonl` plus snapshot `extra.v3520`, and D3.2 found every GUESS field present
except R1.
