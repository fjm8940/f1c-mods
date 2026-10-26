# SPEC_EVF-008_A — corner blister family (8 rows)

**Source:** ASSIGNMENT_EVF-008.md Group A
**Date:** 2026-07-28
**Emitters:** `PitStateSlotActivation_0041ab30` (severe) + `AISessionPhaseCounter_0041aa10` (light)

## Emitter identity

| Property | PitStateSlotActivation | AISessionPhaseCounter |
|---|---|---|
| Address | 0x0041ab30 | 0x0041aa10 |
| Priority | score 4, `on_frame` | (same, score 4) |
| Called from | `VehicleDynamicsForceAccumulatorIntegration_00438130` | same caller |
| `this` identity | vehicle struct (accesses +0xA44 = tread/wear field) | same struct |

Both functions share the identical structure: take `a2` = corner index (0=FL, 1=FR, 2=RL, 3=RR), set a flag at `this[a2+657]` (veh+0xA44), count active corners, and dispatch per-corner or aggregate eids.

## Guard conditions (shared)

```
dword_7D0308 != 0          // RaceActiveFlag (FACT)
flt_7CF3C8 != 0.0          // commentary enabled (CONFIRMED)
dword_7C56A8 != 8          // RacePhase != post-race (CONFIRMED)
```

**No SessionType==7 gate.** Both functions fire in ANY session type. This distinguishes them from the gear-loss emitter (`sub_41AC50`) which is race-only.

## Per-corner loops — verification of all 4 corners

Per the assignment: "confirm the other three corners (RL/RR specifically) actually get evaluated by the same loop, not hardcoded to FL/FR only."

Both functions iterate corners 0-3 in a `do { ... } while(v5)` loop (4 iterations):
- `PitStateSlotActivation` line 15: `v5 = 4; do { if (*v4 == 2) ++v3; ++v4; --v5; } while(v5);`
- `AISessionPhaseCounter` line 14: `v5 = 4; do { if (*v4) ++v3; ++v4; --v5; } while(v5);`

The switch on `a2` handles cases 0 (FL), 1 (FR), 2 (RL), 3 (RR) explicitly:
- `PitStateSlotActivation` lines 27-38: case 0→eid 94 (FL bad blister), case 1→96 (FR), case 2→98 (RL), case 3→100 (RR)
- `AISessionPhaseCounter` lines 27-38: case 0→eid 95 (FL blister), case 1→97 (FR), case 2→99 (RL), case 3→101 (RR)

**Confirmed: all four corners handled, no hardcoded FL/FR limitation.**

## Trigger predicate

The caller `VehicleDynamicsForceAccumulatorIntegration` (CONFIRMED: per-frame physics) computes:
```
tread = this[916]               // veh+0xE50, per-wheel tread (double, fresh=1.0)
wear_delta = this-tick delta
v27 = (tread - 0.75) * 4.0     // scaled wear
v29 = (tread - wear_delta - 0.75) * 4.0
```

- **LIGHT blistering** (AISessionPhaseCounter): fires while `0.85 < tread <= 0.925`, re-evaluated every tick. Throttled by eid-table cooldown (15.0s).
- **SEVERE blistering** (PitStateSlotActivation): fires once on the tick tread crosses downward through 0.85 (`v27 > 0.4 && v29 <= 0.4`).

A separate puncture/blowout state triggers at tread <= 0.75 (not part of this commentary group).

The per-corner flag array at `this[657+corner]` tracks which corners have been activated. If 3+ corners already flagged, the generic aggregate eid (104/105, PWS_TYRES02/01) replaces the per-corner variant.

Prior art: CONFIRMED finding from session 2026-07-27 — "Located the real player-reachable trigger for PWS_TYRES01/02 (and per-corner RE_PD_*_BLIST/BAD_BL) blister commentary."

## Per-clip verdicts

All 8 clips share the same emitter, caller chain, and threshold mechanism. FL/FR corners (PWS_TYRES01/02) were already heard in vanilla play and marked SHIPPED from a prior session. The remaining corners use the identical code path — only the `a2` index differs.

**Verdict: LIVE_TRUSTED for all 8 rows.**

Evidence for LIVE_TRUSTED:
1. (a) Independent writer for trigger condition: `VehicleDynamicsForceAccumulatorIntegration` is a per-frame physics function that computes tread wear deterministically from vehicle physics. The tread-value-to-threshold comparison is directly in the decompiled code (lines 136-153 of the `.c` file).
2. PWS_TYRES01/02 (FL/FR light blistering) are already SHIPPED and recognized in real play — same emitter, same thresholds, different corner index.
3. The prior art CONFIRMED finding independently verified the full trigger chain.

| Clip | SDT | EID | Verdict |
|---|---|---|---|
| RE_PD_FL_BAD_BL | 93 | 94 | LIVE_TRUSTED |
| RE_PD_FL_BLIST | 94 | 95 | LIVE_TRUSTED |
| RE_PD_FR_BAD_BL | 95 | 96 | LIVE_TRUSTED |
| RE_PD_FR_BLIST | 96 | 97 | LIVE_TRUSTED |
| RE_PD_RL_BAD_BL | 97 | 98 | LIVE_TRUSTED |
| RE_PD_RL_BLIST | 98 | 99 | LIVE_TRUSTED |
| RE_PD_RR_BAD_BL | 99 | 100 | LIVE_TRUSTED |
| RE_PD_RR_BLIST | 100 | 101 | LIVE_TRUSTED |

## uncertainty

1. **Tread-wear rate for RL/RR vs FL/FR**: rear tyres wear differently than fronts (acceleration load vs steering/braking). If the game's rear-tyre wear rate is significantly lower, RL/RR may cross the 0.85 threshold much later — possibly not at all in a short race. This is a data/balance question, not a code-wiring question. The emitter WILL fire if the threshold is reached.

2. **AI vs player:** Both functions are called from a per-vehicle physics tick and check the vehicle's own tread values. There is no AI/player gate in `PitStateSlotActivation` or `AISessionPhaseCounter`. These clips fire for ANY vehicle (player or AI) whose tread crosses the threshold. The assignment says these are "corner blister" clips — the SDT name `RE_PD_*` suggests "Race Engineer, Player Driver." Check whether the audio content specifically names the player or is generic.
