# Investigation brief - vanilla game features to make each engine failure feel different (2026-09-28)

**For:** a separate (cheaper) LLM worker. **Type:** static research only.
**Owner:** Matias. **Product:** Mechanical Failures
(`development\mods\mechanical_failures\`).

## 1. Why

We have 11 engine failure types (valve air leak, oil pressure, oil leak,
radiator blocked, water leak, fuel pressure, blow-by, exhaust crack, blow up,
over-rev, + legacy) - see `docs/ENGINE_FAILURES_RESEARCH_2026-09-28.md`.
Each has its own cause, but the player mostly *experiences* them through the
same four channels: HUD text strip, smoke/flame particles, a power cut
(stress lever or ignition byte), engine off. The goal is to find **vanilla
game features** that give each failure its own feel (sound, dash, driving
behaviour, commentary, pit interaction), so the mod builds on the game rather
than faking it.

## 2. Rules (hard)

- **Read-only static research.** Do NOT write to a running game, do NOT
  inject DLLs, do NOT edit any file outside the output file below. Do not
  edit `findings.jsonl`, `name_index.json` or fact tables.
- Never read `api1.txt` or private keys. Nothing is ever deleted.
- Every claim needs a tier: **FACT** (live-proven in `findings.jsonl` or our
  docs), **CONFIRMED** (two independent static sources), **DERIVED** (one
  static source, e.g. a decompiled line), **GUESS**. Never promote a tier.
- Cite evidence as `functions/<Name>_<addr>.c:<line>` or a findings id.
- Start every question with the corpus front door:
  `python tools/ask.py findings <keyword>`, `python tools/ask.py func <addr>`,
  `python tools/ask.py global <addr>`, `python tools/ask.py token <text>`
  (run from `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled`). Read
  `f1c_ida_decompiled\AGENTS.md` first; use `python tools/f1c_mod.py start --json`.
- Do not run broad `rg` from the workspace root (huge trees); scope searches.

## 3. What is already known (do not re-derive)

Player physics object `this = 0x7C7428` (static). Proven levers (FACT):
engine life `0x7C91DC` (double), stress `0x7C91D4` (torque flutter, cap 0.6),
emitter banks `0x7C920C/14/1C` (white/black/flames), ignition byte
`0x7C9081`, rpm `0x7C90C4`, idle `0x7C90EC`, rev limit field `0x7C9104`
(this+7388, reads 1895.4 live - unit unclear), oil temp `0x7C9174`
(this+7500, K, write persists - live-proven), oil limit `0x7C917C`
(this+7508), water temp `0x7C91B4` (this+7564), radiator coefficients
`0x7C91BC` (base, this+7572) / `0x7C91C4` (airflow, this+7580) - written by
our cooling failure, **persistence not yet proven**, fuel `0x7C75C4`
(this+412), ReasonCode `veh+0x4068` (1 engine, 2 gearbox, 4 suspension, 8
electronics, 9 fuel), pittingStatus `veh+0x3450` (3 = in box), HUD strip
`0x7CD204`, radio queue injection (eids < 291 = race engineer),
ParticleTrailEmitter `0x4CBC50` + SpecialFX table `0x8ACEA0`. Thermal model:
`functions/EngineStressSimulation_0043ef20.c` lines 60-131.

## 4. Questions (answer each separately)

| # | Question | Why it matters | Start here |
|---|---|---|---|
| Q1 | **Engine sounds:** which function plays the backfire samples (`Audio\Vehicle\*backfire*.wav`, `internal_backfire_*`), under what condition, and can a DLL trigger a backfire/pop for the player (callable function + arguments + which thread)? Any engine-damage / misfire / rattle sound? | Misfire, fuel pressure, valve float, exhaust crack would each sound different | strings `backfire`, `ask.py token backfire`, gear_failure.c shows a proven sound call (`0x4F89A0`) |
| Q2 | **Dash / gauges / warning lights:** which in-car dash channels exist (oil temp, water temp, fuel, oil pressure?, warning LEDs) and which fields feed them? Is there a warning-light flag we can set? | A real warning light per failure instead of HUD text | `TelemetryDataDump_004d4a80`, `ask.py token` for dash strings (e.g. "ot", "wt"), cockpit/dash render functions |
| Q3 | **Native rev limiter:** who writes this+7388 (`0x7C9104`)? Only at setup parse? Unit (rad/s vs rpm)? If we lower it, does the game's own limiter cut there? | Valve air leak should use the real limiter, not our ignition flicker | `ask.py offset`/global for 0x7C9104, `RevLimitRange`/`RevLimitSetting` tokens |
| Q4 | **Radiator coefficients** this+7572 / this+7580: all writers. Recomputed per tick, per session, or on pit radiator change? Can the pit menu change radiator/tape at a stop? | Cooling failure depends on the write holding; pit "clear the sidepod" realism | `ask.py global 0x7C91BC`, `RadiatorSetting` token, pit menu functions |
| Q5 | **Native heat power loss:** exactly how this+7540 (oil) and this+7588 (water) reduce torque - formula, magnitude at +10/+20 K over limit | Tune cooling/oil failures so the native power loss is felt | EngineStressSimulation, `Gearbox_00443b70` torque chain |
| Q6 | **Fuel consumption:** this+428 / this+436 - writers, meaning; can a fuel LEAK be modelled by raising consumption (runs dry early, ReasonCode 9)? Does the fuel gauge follow? | A fuel leak failure with a distinct end (out of fuel) | EngineStressSimulation lines 60-80, `sub_43C250` fuel seed |
| Q7 | **TV / engineer commentary** for the player's own car: any line about smoke, engine trouble, stopping, fuel, overheating, pitting with a problem (both PWS < 291 and TV > 292)? Blocked? Emitter? | More varied presentation per failure | `mods/extended_vanilla_features/data/full_event_dump.json`, `docs/FULL_INVENTORY.md`, `docs/VOICE_LINES.md` |
| Q8 | **Throttle:** the player's throttle input / actuator field. Is there a safe lever for "throttle stuck partly open" or "throttle dead"? (Brute-forcing the gear global 0x7C9318 once stuck the throttle - find why.) | Throttle failure (a missing family) | findings `0x7C9318`, input handler `sub_42C440`, `Gearbox_00443b70` |
| Q9 | **Force feedback / camera shake:** any vanilla hook that shakes the camera or FFB on engine events or impacts that a DLL could trigger? | Feel a blow-up / valve failure | FFB / DirectInput effect functions, camera functions |
| Q10 | **Pit menu repairs:** does the pit stop offer or perform any engine/cooling repair, and how is stop time extended for repairs? | Pit-fixable failures (radiator debris, valve air top-up) with a real time cost | `PitStopProgress_004a68b0`, `mods/pitstop_modding/docs` |
| Q11 | **Retirement display:** how ReasonCode maps to the results screen text ("Engine", "Fuel", "Electronics"...), and whether a finer reason (oil, water, hydraulics) exists | Correct classification per failure | `FailureCategoryNameLookup_004bb730` |
| Q12 | **Anything else:** other player-car systems in the vanilla code that react to engine state (clutch, anti-stall, starter, limp modes, engine braking) we could drive | Open scan | priority_index.md, `ask.py findings engine` |

## 5. Deliverable

Write **one** file:
`development\mods\mechanical_failures\docs\VANILLA_FEATURES_INVESTIGATION_RESULT_<date>.md`
with, per question: answer, tier, evidence citations, **how a DLL would use
it** (address, call signature, thread, safety risks), and an **effort/risk**
rating. End with a ranked table "biggest player-feel gain for least risk".
List every claim you could not settle as OPEN with the exact live test that
would settle it (the owner runs live tests). Do not implement anything.

## 6. Acceptance

- All 12 questions answered or marked OPEN with a concrete live test.
- Every address has a tier + citation; no uncited numbers.
- No files changed except the result file.
