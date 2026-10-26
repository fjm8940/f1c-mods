# RESEARCH H2: Suspension channels for the web telemetry suspension page

Date: 2026-10-14. Author: Matias. Type: read-only static research (no code edits, no game run).
Component: `mods/sdk_sandbox/web_telemetry`. Scope: ride height (front/rear), damper velocity (per wheel), suspension travel (per wheel).

## Verified state (cold start)

- SDK front door: `start --json` reports stale=true (newest dated entry 2026-10-13). Checkpoint objectives treated as possibly stale.
- `evidence-query findings` for ride height, damper, suspension, spring, bump: hits used below. "susp travel" returned no entries.
- `ask.py token RideHeight|Damper`: RideHeightSetting and RideHeightRange are `.hdv` setup keys (125 files), with no parser yet. These are setup values, not live telemetry. BumpStopDamper and Front/Rear3rdBumpStopDamper are setup strings. None is a live per-wheel channel.
- `CONFIG_SCHEMA.md` `[SUSPENSION]`: only handling/setup tokens (ApplySlowToFastDampers, AdjustSuspRates, track widths). No telemetry channel.

## Channel table

| channel | mod path (file:line) | AI | player | offset / function | tier | evidence excerpt |
|---|---|---|---|---|---|---|
| Ride height, front and rear | Read: `web_telemetry.c:4546` (`pch_ride_mm`, from `PP_D(5872+8w)`), JSON `rh` at `:5819`. Viewer front/rear average `telemetry_viewer.js:5084-5090`, chart `:5148-5149`, row `:5180`. | OPEN | YES (player object only) | P=0x7C7428 (player full-physics object), P+5872+8k, k=0..3, order FL,FR,RL,RR (corner order not independently proven). Written by `sub_4423B0` lines 175-205. Front/rear average of (5872,5880) and (5888,5896) matches `DamperSpringForceComputationWithAntiRoll_004432f0.c:67/81`. | FACT | "writes height of each of 4 chassis points above the track surface". Live static read 0.032/0.033/0.060/0.060 m (ct_probe 2026-10-13, car stationary). |
| Damper velocity, per wheel | None. `web_telemetry.c` has no damper-velocity read. Only mention is the comment at `:3226-3227`, saying the f1c.trace/1 model carries no damper channel. | OPEN (no stored value found) | OPEN (no stored value found) | `TireContactForceComputation_004e3aa0.c:58` computes `v7 = dot(normal, velocity)` as a local. Line 89: `this[17] = this[59]*this[29] - v7*this[60]`, i.e. spring +0xEC x compression +0x74 minus damper +0xF0 x v7. Only load (+0x44) is stored, not v7. | DERIVED (formula); stored value OPEN | "this[17] = this[59] * this[29] - v7 * this[60]". Velocity input candidates at this+0x18..0x20 and normal at +0x38..0x40 are DERIVED from the same function, not verified live. |
| Suspension travel, per wheel | Player: read `web_telemetry.c:4547` (`pch_susp_mm`, `PW(w,488)`), JSON `sus` at `:5820`, viewer row `telemetry_viewer.js:5178`. AI: `rx_su` reads WB+0x10 at `:4638` (WB_OFF_SUSP at `:669`), which finding `ai-wheel-block-roles-static-20261013` says is wrong. | AI: YES in the game (per-vehicle wheel block, DERIVED); mod does NOT read it correctly | YES (player W+488) | Player: W(w) = 0x7C7428 + 652 + 1124w, +488. AI: vehicle slot base 0x5EC6D8, stride 0x48C0, wheel block veh+0x358C stride 0xF8, travel at +0x74 (= 13708 + 248w + 116). `TireContactForceComputation_004e3aa0.c:54-55`: this[29] = this[36] - ground distance (rest +0x90 minus ground). | Player FACT (offset, static live only); AI DERIVED | Player: "W+488 suspension deflection m (the game's own .spt recorder channel)". AI: "+0x74 = rest length +0x90 minus ground distance (compression, <0 airborne)". |
| Damper velocity fallback | Not in mod | Derive from delta of AI +0x74 over dt | Derive from delta of W+488 over dt | Sign: documented intent (manual, DERIVED) is rebound = positive, so rebound velocity = -d(travel)/dt if travel is positive in compression. | GUESS (sign and units unverified) | Needs a moving car to validate. Live reads so far were static. |

## Per-question answers

1. Damper velocity: the mod does not read it, and the reviewed game code stores no per-wheel damper velocity. Derive it as travel delta / dt and label it as derived in the UI. Tier for a stored value: OPEN.
2. Front and rear ride height: the mod already reads player ride height (`:4546`) and draws it in the viewer. AI ride height is OPEN. `RideHeightDelta_004ef680` (sub_4EF680) reads float indices this[467] and this[3428], [3460], [3490], [3522] of an object not yet identified. Do not claim an AI channel.
3. Suspension travel: player is read correctly at W+488. AI travel is held per vehicle at +0x74 (DERIVED), but the mod reads +0x10 for AI (`rx_su`). Fix candidate: change `WB_OFF_SUSP` from 0x10 to 0x74 after a live proof on a moving AI car. Until then AI `rx_su` values must not be labelled as suspension.

## Corrections and cautions

- `web_telemetry.c:659-660` labels +0x10 as "suspension deflection candidate". Finding `ai-wheel-block-roles-static-20261013` (DERIVED) says +0x10 is not suspension. The same finding says +0x50 is not tyre temperature. Both labels are suspect.
- Player travel (W+488) and ride height (P+5872) were read only with a static car. Motion proof is pending.
- Nothing here changes product status. No code was edited.

## Verification performed

- Read `web_telemetry.c` (lines 540-670, 1036-1042, 4534-4660, 5815-5822), `telemetry_viewer.js` (2026-2034, 5084-5180), `functions/RideHeightDelta_004ef680.c`, `functions/TireContactForceComputation_004e3aa0.c` (lines 1-92), `SpringDamperCompute_0043bbd0.c`, `SuspensionDamperForceAccumulation_0043e890.c`.
- Ran `start --json`, `evidence-query findings` (6 terms), `ask.py token` (RideHeight, Damper), and `inspect` (by name where it resolved, by address otherwise).

## Pending validation

- Moving-car live read of W+488 (player travel) and P+5872 (ride height).
- AI wheel block +0x74 live read on a moving AI car, with sign check against the wheel-block finding.
- Identify the AI ride-height source (`RideHeightDelta` object).
- Damper velocity: confirm whether a velocity vector at this+0x18..0x20 is stored per wheel.

## Files

- Record: `mods/sdk_sandbox/web_telemetry/RESEARCH_H2_SUSPENSION_2026-10-14.md` (this file).
- Findings: `f1c_ida_decompiled/findings.jsonl`, ids `h2-susp-*`.
