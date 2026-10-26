# DAMAGE_REACTION — D13: vanilla AI reaction to damage (BATCH B item 9)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only, static). Sources: `functions/*.c`,
`investigations/ai_interaction_role/README.md` (IR, 2026-08-18), `docs/53-...`, registry v1.2.
Mod-side material (`development/mods/mechanical_failures/docs/`) is **not** used as vanilla
evidence; the last section names it only as contrast. `P = V+0xC` = physics sub-object; wing records
live on `P` (`*this` = P where stated).

## 1. Trigger -> effect table (vanilla)

| trigger field | AI effect | function + line |
|---|---|---|
| Front wing record `P+0xCD4 != 0` && flag `P+0xD14 == 0` | **pit**: forces a stop and drops pace to 0.85*baseline; blocks normal stops and force-schedules the plan slot | `AIAbility` L86-L92 (ForcePitExit + `this[4431] = this[4413]*0.85`); `PitStopBlockingCondition` L8; `PitStopDecisionTireCompound` L45-L52 |
| Rear wing record `P+0xD18 != 0` && `P+0xD58 == 0` | **retire trigger** (IR L130-134: "the retirement trigger per PitStopBlockingCondition"); same pit force/block paths as the front record; Overtaking pass eligibility changed | `PitStopBlockingCondition` L8; `PitStopDecisionTireCompound` L45/L91; `Overtaking` L47-L51 |
| `carConditions 0x3D6C == 1` (+ wing/sub-object flags or late race) | **pit**: ForcePitExit; **pace**: multiplier = `0x44F4 * 0.85` | `AIAbility` L80-L94 (`v6 == 1` branch) |
| `carConditions 0x3D6C == 2` | **coast/stop, no retire action by the AI**: throttle 0; brake by abs(lateral 0x4428): <5 m -> 0.01, 5-7 m -> (lat-5)*0.1+0.01, >=7 m -> 0.21; gear request 0; corner heading clamps dropped; the game re-retires the car (LIMBO) | `AIMainUpdate` L142-L160 (LABEL_86 dead-coast, doc 54 section 5); `PhysicsGearShiftLogicWithThresholds` L15; `sub_426430` L186; `AICollision` L24 returns 2 |
| mean tyre wear < 0.9 (4 wheels 0x363C/0x3734/0x382C/0x3924) | **pit**: `ForcePitExit` (forced stop) | `AIPushLap` L86 |
| `Crash 0x44D0` % gate (race 7, `AICollision` L30) | incident: return 1 -> caller `EngineFailureCheck` sets event code 6 -> `PitCrewStateMachine` | `AICollision 0x4BEF60` L30; IR "Crash-profile gates" section |
| `CompletedLaps% 0x44DC` % gate (`AICollision` L37) | incident: return 2 -> randomized event code 0..9 from the noise table -> `PitCrewStateMachine` | `AICollision` L37; IR L5866/L6327 |
| AI engine-failure timer (RandomVariationScaler-seeded) below threshold | incident chain -> `AICollision` -> event code 6/7/8/9/0 (engine/brakes/gearbox/suspension variants) | `EngineFailureCheck 0x4BEC55` L1-L80 |
| `ReasonCode 0x4068` writes (fuel/mechanical wear paths) | failure category 1/2/4/5 + `sub_41AC50` event; repair path writes 6; pit progress clears 9->0 | `FuelConsumptionAndWear` L125 (1), L229 (5), L242 (2), L259 (4); `RepairServiceStateDispatcher` L12 (6); `PitStopProgress` L66-67 (9->0) |
| engine-life slots `0x3D70..0x3D80` (5 slots) | **scripted failure schedule**: per matching lap entry, life is set to `rand % budget` (`0x416C`), so the AI can suffer an engine failure at a randomized lap | `AIMainUpdate` L202-L206; `RandomVariationScaler` (D5) seeds the timer in `EngineFailureCheck` |
| collision damage / DNF detection | sets the failure slot 5 and clears damage energy fields on a heavy hit | `PhysicsCollisionDamageDNFDetection 0x438690` L40-L58 |

## 2. What the vanilla AI does / does not do

- It **does** react to damage and wear with **pitting** (wing records, carConditions 1, worn tyres,
  scripted incidents) and with **crippled control** when `carConditions == 2` (throttle 0, gentle
  brake by lateral, gear 0, dropped heading clamps).
- It does **not** decide to retire or to pull off the racing line deliberately: `carConditions == 2`
  is set by the physics/race layer, and the AI only coasts where it already is (braking grows with
  lateral distance, which may drift it off-line over time). The "retirement trigger" is the rear-wing
  record + intact flag pair, consumed by the pit/incident chain, not an AI decision.
- Wing damage changes **pass eligibility**: `Overtaking` L47-L51 requires the same-team peer
  fallback and the paired wing-record condition, so a damaged car's overtake acceptance narrows.
- All random incident gates use the startup noise table (`flt_8AAE74[dword_8AAE70++]`, D5), so the
  incident timing is deterministic per session seed.

## 3. Contrast (mod-side, NOT vanilla evidence)

`development/mods/mechanical_failures/docs/AI_SYMPTOMS_AND_REACTIONS_SPEC_2026-10-01.md` and
`AI_REACTIONS_RUN_2026-10-02.md` describe deliberate mod behaviours (park/pull-off, reaction
overrides) built on these fields; they are intentionally not cited as proof of native AI behaviour
here. The vanilla surface this task maps (wing records, carConditions branches, tyre<0.9, engine
slots, AICollision gates) is what such mods hook.

## Limitations

- Static only (batch B is offline); no live damage race exists in the recorded set (`watch_*`
  damage events were not captured; D13 = code map).
- The exact wing record structure (0xCD4/0xD18 contents) is not decoded here; the IR treats the
  pointers as pending-record markers and 0xD14/0xD58 as intact/damaged bytes.
- `0x3D70` slot semantics (one per race-event plan entry) come from AIMainUpdate L202 and D5; the
  event-code names live in `EngineFailureCheck` (unlabelled here).
