# LIVE_LOG_PLAN — read-only live logging plan (B6)

Created 2026-10-02 as part of the `REVIEW_2_GROUP1.md` required additions. This file collects the offsets a live
read could confirm (one entry per GUESS / DERIVED claim group). It is extended by B6; the entry below is the
group-1 corner-speed and braking pipeline.

Rules (handoff B6/B7): description only, nothing is run from this file; when a run happens it is READ-ONLY
(`OpenProcess` access `0x0410`, query + VM read, no writes, no injection), and only while Matias says a race is live
and names the situation. Output CSVs go to `%TEMP%\ai_brain_live\`; only a <= 15-line summary goes to
`LIVE_RESULTS.md`. If sampled values stop changing (pause/menu), the run proves nothing. Results stay GUESS until
Claude reviews.

Reader style (description only, from `%TEMP%\mf_dyn\brainlog.py`): ctypes `OpenProcess(0x0410)` +
`ReadProcessMemory`; vehicle base = `0x5EC6D8 + 0x48C0 * slot`; poll at ~40 Hz; append one CSV row per sample.

## Run A — one AI car through one slow corner (confirms: corner-speed formula, braking look-ahead)

Situation needed: full race weekend, dry, one AI car (record driver, slot, car), approach to a slow corner after a
long max-speed straight (a hairpin or chicane). Capture >= 10 s continuously across approach -> braking -> apex ->
exit, with the car on the racing line and not fighting another car.

| offset | dec | field | what the samples should show |
|---|---|---|---|
| 0x4440 | 17472 | current speed (m/s) | rises to max on approach, falls smoothly through braking, rises on exit |
| 0x43FC | 17404 | limiting waypoint target speed | > current speed early; drops below 0x4440 at the brake point; 9999 when nothing limits |
| 0x43A4..0x43F0 | 17316..17392 | 20-slot speed profile (floats, 4 bytes apart) | descending sequence ahead of the car; slot 0/1 should track measured 0x4440 during braking |
| 0x43F8 | 17400 | limiting waypoint pointer | changes when the binding corner changes; 0 when none |
| 0x4434 | 17460 | throttle command | 1.0 / hold value on approach; -> 0 when braking starts; ramps back after apex |
| 0x4438 | 17464 | brake command | 0 on approach; ramps up (+0.2/tick expected) at the brake point; -> 0 after apex |
| 0x44CC | 17612 | PassingNorm (prior art) | constant for the run (0.9 + 0.001 * .rcd Passing); multiplies the projected speed loss |
| 0x4500 | 17664 | corner-speed offset term | constant per car (unless AI state changes); used in the formula below |
| 0x4504 | 17668 | corner-speed coefficient | constant per car; used in the formula below |
| 0x4580 | 17792 | corner-speed scale | constant per car |
| 0x453C | 17724 | AIAbility dynamic multiplier | small slow variation; scales corner speed |

Formula check (DERIVED, Claude 2026-10-02): `v_corner(wp) = (TireForceSolver(wp) * 0x4504 + 0x4500) *
0x4580 * 0x453C * wp[0x84]`. If the waypoint fields and TireForceSolver inputs are logged too, recompute
`v_corner` for the waypoint at 0x43F8 and compare with 0x43FC (expect equality within float noise).

Expected pattern (braking look-ahead):

1. Approach: 0x4438 == 0, 0x4434 > 0, profile ahead descending; 0x43FC > 0x4440 while no corner is binding.
2. Brake point: a waypoint's target (`v50`) falls below the projected speed, 0x43FC drops under 0x4440, 0x4438 ramps
   up and 0x4434 goes to 0. The first profile slot begins to track 0x4440 downward.
3. Braking: measured speed loss over one segment should match `wp_len(wp+60) / v * 0x44CC * v50 * 0.915`.
4. Exit: 0x4438 returns to 0, 0x4434 ramps up by `0.2 * 0x44B8` (race start) or `0.2 * 0x44C4`, profile refills.

Evidence handling: same car, same corner across at least 3 laps; report min/max and correlations, not raw dumps.
Unchanged fields across the whole run may just mean the car never reached the condition (e.g. never braked). Open
items that this run cannot settle: meaning of 0x4500/0x4504/0x4580/wp+0x84, and the 0.4/0.5 off-line throttle split.
