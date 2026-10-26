# AI failure reaction plan - draft (Claude, 2026-10-01)

Status: DRAFT for owner review. Pipeline stage: define/specify. Nothing implemented.
Product: mechanical_failures (merged DLL). Owner insight that started this: vanilla AI never
notices a failure and keeps full pace, so any failure a human survives by backing off over-retires
AI cars. Goal: AI drivers notice symptoms, react like a driver of the era would, with per-driver
variance, using only proven game levers - and the season-long retirement count stays what the
car authors intended.

## 0. In one screen

1. **Feel** - each failure produces symptoms the AI can "feel" (power loss, revs, temps, handling).
2. **Notice** - a per-driver delay (seconds to laps) before the driver reacts. Better drivers notice sooner.
3. **Decide** - one of 5 reactions, picked by severity, laps left, race position and driver personality.
4. **Act** - through proven levers only: rev cap, speed cap, pace, overtaking willingness, pit call, retire.
5. **Pay off** - reacting really slows the damage (lower revs = lower wear in the model), so a good
   reaction can bring the car home, a bad one blows it up. Same physics as the player.
6. **Stay fair** - calibrated so the average number of AI retirements per race = vanilla.

## 1. Fairness rule (decided 2026-10-01, memory project-ai-engine-fairness-rule)

- WHEN an AI engine is doomed stays vanilla: full game timer `Normal(3 x LifetimeAvg, 3 x LifetimeVar) / race factor`
  (proven live: field z mean 0.20, sd 1.05, finding ai-timer-roll-matches-engine-ini-live-2026-10-01).
- The model only redistributes WHO and HOW (driving style, grade, symptoms). Mean count = vanilla.
- Reactions may SAVE a doomed engine (nursing it to the flag). To keep the mean, the calibration (section 7)
  raises the onset rate slightly so "faults started" x "not saved" = vanilla retirements.

## 2. Levers we can use (all from the SDK journal)

| Lever | Field | Tier | Use |
|---|---|---|---|
| Speed governor (per car, field-wide proven) | sub_42ACD0(veh, N) | FACT f1c-pace-governor-scales-to-full-field | cap top speed while nursing |
| Max speed | veh+0x4538 (m/s) | FACT 0x4538-maxspeed-ms | simple cap (already a DLL fallback lever) |
| Pace scalar (RaceAbility-derived) | veh+0x44F4 | DERIVED ai-pace-formula | slower in corners too (needs live proof) |
| Overtaking willingness | veh+0x44CC PassingNorm | FACT f1c-trackaggression-is-not-the-overtaking-knob | stop attacking, let cars by |
| Following gap / side-by-side | veh+0x44F8 TrackAggression | CONFIRMED | back off from the car ahead (cooling air) |
| Power | AI torque table (veh+0x39B0/0x39BC) | DERIVED, in use since E9p | symptom: power loss |
| AI rev limit | veh+0x39A4 | read in use; write UNPROVEN | short-shift / rev cap - A0 must prove |
| Pit call | veh+0x454C callToBox | FACT (alone it does not retire) | come in to check / repair |
| Pit state | veh+0x3450 pittingStatus 0..4 | CONFIRMED | know when the car is in the lane/box |
| Retire via game path | veh+0xD58 = 0 + callToBox = 1 | FACT wing-retire-trigger | drive to garage and retire |
| Vanilla AI timers | veh+0x3D70/74/78/7C (engine/brakes/gearbox/...) | FACT | WHEN, read; terminal write 0.001 already in use |
| Vanilla AI stuck gear | byte veh+0x39FA | FACT | gearbox symptom |
| Driver traits | .rcd: RaceAbility 0x44C4, Crash 0x44D0, Passing 0x44CC, CompletedLaps% | CONFIRMED rcd-offsets-live-confirmed | personality |
| Driver/car identity | veh+0x4620 name, 0x4678 car, 0x4714 .veh | FACT | logs, panel |

### 2.1 What the earlier AI investigation adds (f1c_ida_decompiled/investigations/ai_interaction_role/README.md)

That investigation is a static (read-only code) map of the AI driver. Its own verdict: **"Safe writable core-AI
SDK contract: not proved"** - map yes, direct control writes no. Consequences for this plan:

| AI internals (static FACT) | Plan decision |
|---|---|
| AIMainUpdate runs per car every 10th internal tick (25 ms); AIAbility every ~4 s session time | reactions decided on our worker; levers written at most a few Hz |
| Drive/brake pair +0x4438 / +0x4434 copied to +0x3584 / +0x357C and consumed by physics (mode-1 path) | READ-ONLY. Never write: rewritten every 25 ms, writer arbitration unproven. Use the game's own speed governor instead |
| Steering +0x442C -> +0x3580 (local actuator for all AI unproven) | never touch |
| Pace parameter +0x453C adjusted by the AI itself (+-0.01 steps, push laps) | do not write (self-overwritten); RaceAbility-derived +0x44F4 only if A0 proves it holds |
| Mode state +0x4510 (0..7, 4/5 = pit/phase path) | read-only: tells us when the car is in a pit/phase state |
| Gear changes by the AI's own thresholds (PhysicsGearShiftLogicWithThresholds 0x426330) | "short-shift" = rev cap / power, not a gear write |
| Game's own pit triggers: RacePhasePitEntryTrigger (fuel < 1, phase 5) and AIPushLap (tyre average < 0.9) -> ForcePitExit 0x42A110 (blocked on the final lap) | A0 research: can R3 "pit to check" ride one of these native paths (game thread only) instead of a raw callToBox |
| Teammate pointer +0x45F8 (equal team name +0x46B8); teammate-aware fuel/pit-exit gate | team-order trait can use the real teammate link; never pit both teammates the same lap |
| Command queue: command 5 sets +0x4544 (blocks the race pit trigger) | avoid; note when present |

Net: the plan's levers stay the live-proven "outer" ones (governor, maxSpeed, PassingNorm, TrackAggression,
callToBox / game retire path, torque table). The investigation tells us which inner fields to leave alone and
gives one promising native pit route to prove in A0.

Not available / do not use: position or velocity writes (DISPROVEN f1c-never-write-a-car-position-from-a-mod),
de-retire tricks, AI smoke/sound (no proven per-AI particle/audio path yet - OPEN, A6).

## 3. The driver model

### 3.1 Traits (rolled once per driver per session, logged)

| Trait | From | Range | Meaning |
|---|---|---|---|
| Sensitivity | RaceAbility (better = more sensitive) + noise | notice delay x0.5..x2 | how fast they notice symptoms |
| Mechanical sympathy | Crash (low Crash = smoother) + noise | 0..1 | how well they nurse (how much they cut revs) |
| Racer | Passing + noise | 0..1 | how hard they keep pushing with a problem |
| Team order | random per team, 0..1 | - | "bring it home" bias for points positions |

Variance rule: every trait = file value mapped to 0..1, plus Normal(0, 0.15), clamped. Every reaction
choice is a weighted dice roll, never a fixed rule, so two identical situations can play out differently.

### 3.2 Notice delay

`delay = base(symptom) x sensitivity_factor x LogNormal(0, 0.4)`; symptoms that are loud (blow-by smoke,
big power loss) have a short base; quiet ones (slow water rise) a long base. Typical bases below per failure.
Radio era assumption: the car has dash warning lights (oil pressure, water temp, revs); the pit wall sees
telemetry only from late 1990s - optional "team call" shortens the delay for 1998+ season folders.

### 3.3 Reactions (the 5 options)

| Reaction | What the driver does | Levers |
|---|---|---|
| R0 Ignore | keeps racing (did not notice, or a racer near the points) | none |
| R1 Nurse | short-shift / lower revs, lift-and-coast, no attacks, longer gap | rev cap 90-97 %, speed cap, PassingNorm low, TrackAggression low |
| R2 Defend-limp | R1 harder, gives way to faster cars, holds position if possible | rev cap 80-90 %, governor, PassingNorm 0 |
| R3 Pit | comes in to check / fix (only for fixable faults) | callToBox, then clear the fault or retire in the box |
| R4 Retire | parks it (safe stop) before it blows | game retire path (wing flag + callToBox) - drives to the garage |

Decision weights = f(severity now, growth rate, laps left, position, points, traits). Re-decided when the
symptom gets clearly worse (one escalation per stage, so no flip-flopping).

### 3.4 Physics pays off

The AI reaction changes the real inputs (revs, speed, load) that drive the AI shadow model and the native
law, so nursing genuinely slows wear and fault growth - no fudge factor. Same equations as the player.

## 4. Failure by failure

Notation: notice = base delay before traits; mix = default weights R0/R1/R2/R3/R4 at medium severity,
mid-race, midfield. Effects of growing severity shift weight to the right.

### 4.1 Engine (AI shadow model, already running)

| Failure | AI feels | Notice | Mix R0/R1/R2/R3/R4 | Realistic reaction details |
|---|---|---|---|---|
| Valve return (air) | revs stop climbing, misses at top end | 5-20 s | 10/50/30/10/0 | short-shift below the float point (rev cap at the model ceiling - 3 %); pit for an air top-up only in 1995+ pneumatic era (R3 restores some return force, ~20 s stop) |
| Oil pressure | warning light, power fade at high revs | 3-10 s | 5/25/35/5/30 | strong rev cap (80-85 %); big severity -> R4 quickly (drivers stop to save the engine) |
| Oil leak | oil temp climbs, smoke later | 20-60 s | 15/45/25/0/15 | lift-and-coast, stay off kerbs; retire when the light comes on |
| Water leak | water temp climbs, steam | 30-90 s | 15/45/25/0/15 | slip-stream avoidance (longer gap = cleaner air), lift on straights |
| Radiator debris | water temp climbs fast | 10-40 s | 10/30/20/40/0 | weave / back off to clear (chance per lap the debris falls off), or pit to clear (R3 fixes) |
| Fuel pressure | stutters on full throttle | 2-8 s | 15/45/35/5/0 | smoother throttle (pace cap on exits), short-shift |
| Fuel leak | none until fuel runs low (team sees fuel) | 1-3 laps | 30/40/0/30/0 | R3 = splash of fuel if laps left > fuel left; else nurse for economy |
| Blow-by | smoke, power fade | 2-10 s | 10/40/30/0/20 | rev cap, no attacks; retire when power < ~70 % |
| Exhaust crack | flames / noise, later rear suspension heat | 10-60 s | 30/50/15/0/5 | mostly ignore or nurse; retire if the rear goes |
| Overheating (generic) | temp warnings | 5-30 s | 10/50/30/0/10 | back off from the car ahead, lift on straights |
| Over-rev events | (driver error, not a fault) | - | - | sympathy trait reduces over-rev likelihood (cap at 99 % for smooth drivers) |

### 4.2 Gearbox (vanilla AI stuck-gear byte + gearbox timer; AI shadow gearbox later)

| Failure | AI feels | Notice | Mix | Reaction |
|---|---|---|---|---|
| Lost gear | missed shift, skips a gear | 1-2 shifts | 10/60/30/0/0 | adapt: skip that gear (rev cap lower in the gear before) |
| Stuck in gear | cannot shift | instant | 0/0/40/30/30 | limp to the pits at reduced speed; retire if stuck low |
| False neutral | one-off neutral | instant | 70/30/0/0/0 | mostly carries on, shifts gently afterwards |
| No drive | no drive | instant | 0/0/0/0/100 | coasts off - vanilla retirement |

### 4.3 Electronics (AI shadow later; same symptoms as the player family)

| Failure | AI feels | Notice | Mix | Reaction |
|---|---|---|---|---|
| Misfires | bursts of cutting | 2-5 s | 30/50/20/0/0 | smoother throttle |
| Limp mode | big power cut | instant | 0/0/60/30/10 | limp, pit for a reset (R3 clears 50 % of the time) |
| ECU cut-out | engine dies, restarts | instant | 20/30/20/30/0 | pit for a check after the 2nd cut |
| TC failure | wheelspin (only if the car has TC) | 1 lap | 40/60/0/0/0 | gentler exits (pace cap) |
| Shift glitch | odd neutral | instant | 60/40/0/0/0 | gentle shifts |
| Alternator | misfires as the battery drains | 1-3 laps | 10/20/0/40/30 | pit (battery swap in era? R3 50 %) or park when it dies |

### 4.4 Suspension (AI shadow later)

| Failure | AI feels | Notice | Mix | Reaction |
|---|---|---|---|---|
| Handling problem (small bend) | car pulls | 1 lap | 30/60/10/0/0 | slower corners (pace scalar) |
| Part broken | big pull, wobble | instant | 0/10/30/50/10 | pit to check; retire if not fixable |
| Collapse | corner gives way | instant | - | game physics; vanilla retirement |

### 4.5 Brakes (vanilla AI brake timer veh+0x3D74)

| Failure | AI feels | Notice | Mix | Reaction |
|---|---|---|---|---|
| Brake wear / fade | longer stopping | 1-2 laps | 10/60/30/0/0 | earlier braking = pace scalar down; no late-braking moves (PassingNorm low) |

## 5. Race context modifiers (weights shift, never hard rules)

- Laps left small (last ~10 %): R0/R1 up ("bring it home"), R3/R4 down unless the fault is terminal.
- In the points / podium: R1 up, R4 down; team-order trait adds.
- Last place, many laps left: R4 up for engine-saving faults (era realism: teams saved engines).
- Leader with a big gap: R1/R2 up (can afford to nurse).
- Under the safety car (if active): reactions free (pace already low); R3 more likely (cheap stop).
- Weather wet: notice delay shorter for handling faults.

## 6. What the player sees / hears

- AI car visibly slower on straights (speed cap), more cautious in battles, may let you by.
- Pit-lane visits for fixable faults; retirements happen in the garage via the game's own path (commentary
  for AI retirements already works in vanilla).
- Panel AI page: "Driver reaction: NURSING (rev cap 92 %)", notice countdown, reason. Log: `AI REACT: slot N
  <driver> fault <x> sev <s> -> R1 NURSE (delay 14 s, traits s=0.7 m=0.4 r=0.6)`.
- Later (A6, OPEN research): AI smoke/sound for big symptoms.

## 7. Calibration and tests (before any live test)

1. Pure C decision module in the model header (no game access): traits, notice delay, weighted choice,
   escalation. Unit tests: fixed seeds, every failure, every reaction reachable, no flip-flop, determinism.
2. AI field simulator (extend tools/car_sweep): 22 cars x 1000 races x real .rcd + engine.ini files of
   several seasons; driver behaviour from the existing synthetic race loop plus reaction effects.
   Report: retirements per race vs vanilla (target mean within +-5 %), share of faults saved by nursing
   (target 20-40 %), reaction mix per failure, distribution by RaceAbility (better drivers save more).
3. Lever proof first (A0): live, one AI car, each lever on/off, logged before/after (speed, revs, lap time,
   pit entry). Unproven levers (rev cap write, pace scalar write) do not ship until proven.
4. Abort conditions: any lever that changes another car, breaks a pit stop, strands a car in the lane, or
   holds after session end -> lever disabled, finding recorded.

## 8. Phases

| Phase | Content | Gate |
|---|---|---|
| A0 | Live lever proof: governor, maxSpeed, PassingNorm, TrackAggression, callToBox+clear, retire path, AI rev-limit write, pace scalar write | each lever FACT or dropped |
| A1 | Fairness: AI law seeded with the full vanilla timer; mean failure count = vanilla (sim) | sim within +-5 % |
| A2 | Decision core (pure C) + traits from .rcd + tests | unit tests, determinism |
| A3 | Engine reactions R0-R2 (nursing levers) | sim + live: nursing visibly slows wear |
| A4 | R3 pit + R4 retire for engine faults | live: pit/retire clean, no stuck cars |
| A5 | Gearbox / brakes (vanilla AI timers) | sim + live |
| A6 | Electronics + suspension AI shadow families; AI smoke/sound research | later |
| A7 | Panel + commentary hooks (radio lines for AI faults, if proven) | owner feel test |

## 9. Open questions for Matias

1. Should AI ever retire a running-but-damaged car (R4), or always drive until it breaks? (Realism vs drama.)
2. Era rule for team radio/telemetry (pre-1998 slower notice)? Yes/no.
3. Do pit-fixes (R3) for debris / air top-up / ECU reset feel right, or only "pit = retire"?
4. Visible effect target: should you be able to SEE an AI car nursing (clearly slower), or subtle?

## 10. Documentation and restoration

This is a planning record only: no source, binary or game state changed. Lever tiers quote the SDK journal;
DERIVED/UNPROVEN levers are marked and gated by A0. Next step after owner answers: specification prompt for
DeepSeek per phase (A0 is a Claude live job).

## 11. Revision 1 (2026-10-01, owner answers + full AI research sources)

### 11.1 Where the earlier AI research lives (read for this revision)

| Source | What it gives this plan |
|---|---|
| `f1c_ida_decompiled/docs/41-raceability-mechanism.md` | RaceAbility -> AI pace (traits) |
| `f1c_ida_decompiled/docs/42-ai-realism-and-difficulty-model.md` (RUNTIME-VERIFIED) | pace = strength slider x AIW adjust x driver; Realism x TrackAggression = following distance / overtake frustration - the "how hard he races" dials |
| `f1c_ida_decompiled/docs/46-player-specific-mechanical-failures.md` | the vanilla AI countdown timers (11.2) |
| `f1c_ida_decompiled/docs/53-ai-pit-stop-procedure-runtime-map.md` (RUNTIME OBSERVED) | natural AI pit sequence CallToBox -> PitStatus 1,2,3 (service 7.0-9.5 s, mean 8.6 s) -> 4 -> 0; service end = `PitStopEndTime` deadline; PitRepairFlag seen 1 -> 0 in service |
| `f1c_ida_decompiled/investigations/ai_interaction_role/README.md` (static) | inner AI controls = read-only (section 2.1) |
| `f1c_ida_decompiled/investigations/ai_soft_tame/README.md` (live) | park/freeze recipes - not used (no positional tricks) |
| `development/mods/full_safety_car/*` (live, many hours) | pace governor field-wide, pace conflict/feedback lessons, overtake suppression - the proven way to slow AI cars |
| `f1c_ida_decompiled/findings.jsonl` | lever tiers (section 2) |

### 11.2 Vanilla AI timers vs our system (owner question)

FACT (doc 46 + live): every AI car has four countdowns seeded from `Normal(3 x LifetimeAvg, 3 x LifetimeVar)`
(/ race factor in mode 2), ticking 1 s/s:

| Slot | Doc 46 meaning (slot 0 live-proven, others static) | At 0 |
|---|---|---|
| veh+0x3D70 | engine | ReasonCode 1, power factors rolled 0.7-1.0, game retires ~5 s later |
| veh+0x3D74 | gearbox | ReasonCode 5 |
| veh+0x3D78 | stuck gear (byte 0x39FA) | ReasonCode 2 + MechProb commentary |
| veh+0x3D7C | random tyre damage (+0.05 wear one wheel) | ReasonCode 4 + commentary |

Note: engine_failure.c comments call slots 1-3 "brakes/gearbox/suspension" - doc 46 maps them to gearbox /
stuck gear / tyre damage. Doc 46 is the more detailed source; slots 1-3 still need a live proof (A0).
There is NO vanilla AI brake or suspension timer.

What our DLL does today (FACT, code + live logs):
- Engine slot 0x3D70: READ always (it is our WHEN in simple mode). WRITTEN only when our model kills an AI engine
  (`= 0.001`), which makes the game run its own engine failure and retirement. Otherwise untouched; in law mode the
  vanilla timer keeps running beside ours and can still kill the car on its own.
- Slots 0x3D74 / 0x3D78 / 0x3D7C: never read or written. Vanilla gearbox / stuck-gear / tyre failures happen
  exactly as in the unmodded game.
- Consequence for this plan: if an AI driver nurses an engine home (R1/R2), the vanilla engine timer would still
  kill it. The reaction system must OWN the engine slot while a fault is active: hold/extend 0x3D70 so only our
  model decides (A0: prove that writing a LARGER value extends life cleanly - only "small value fires" is proven).

### 11.3 Owner decisions (2026-10-01) - supersede section 9

1. Retire (R4) is situation-specific, never automatic: chosen only by the driver's "feel" (severity, trend,
   traits, laps left, position). Same fault can end in a nursed finish, a pit stop, or a stop.
2. NO year/era filtering (the player cannot tell either): one rule set for all seasons. Section 3.2 "team call
   1998+" removed.
3. Scope first = ENGINE faults. Pit fixes (R3) apply where realistic and ADD time to the stop: extend the
   service (`PitStopEndTime` deadline, status 3) by a fault-specific repair time + variance:
   radiator debris clear 3-8 s, ECU/electrical reset 5-15 s (later, electronics); owner 2026-10-01: NO valve air
   top-up (removed from the game model) and NO oil/water top-ups (not done in era pit stops);
   oil-pressure, blow-by, exhaust crack: no fix (pit = check, then decide continue/retire, +5-10 s).
   `PitStopEndTime` = veh+0x3470 (float32, SDK canonical field, labelled "observe only"; PitStopStartTime 0x346C;
   source f1c_ida_decompiled/runtime_memory/observations/ai_pit_physical_20260830_recipes.json). Observed changing at
   every 2->3 edge (doc 53). Writing/extending it during status 3 is UNPROVEN - A0 must prove it.
4. Visibility is situation-specific but should be CLEARLY visible for fun: nursing = noticeably slower on straights
   (speed cap) and passive in battles; limping cars let others through; pit visits visible; retirements via the
   game's own garage path (its own commentary). Big symptoms (smoke) for AI = research item A6, priority raised.

### 11.4 Phase order after revision

A0 lever proofs (governor/maxSpeed, PassingNorm, callToBox natural route, PitStopEndTime extend, 0x3D70 hold/extend,
slots 1-3 meaning) -> A1 fairness calibration (sim) -> A2 decision core -> A3 engine nursing -> A4 engine pit-fix +
retire -> A6 AI smoke (raised) -> later gearbox/tyre/electronics/suspension.

## 12. A0 live trials (Claude + owner listening, 2026-10-01, 1997 race 100 %, dll 69a5ef1a, slot 23 Rosset)

| Lever | Result | Owner ear | Verdict |
|---|---|---|---|
| Engine timer 0x3D70 extend +5000 s | holds, counts 1 s/s from the new value, exact restore | - | USE: we can own the vanilla engine clock while nursing |
| maxSpeed 0x4538 cap 200 km/h | holds (max 201); game also writes this field (22.2 m/s seen) | limiter bounce, artificial | DROP for nursing; restore-only-if-ours rule |
| Rev limit 0x39A4 -6 % | holds (max 15995 rpm) but the AI does not short-shift, it sits on the limiter (max 203 km/h) | "cruising capped 166 km/h, artificial" | DROP |
| Power column x0.90 | normal revs (16588), naturally slower (244 vs 253-256 km/h) | (pending owner) | USE: main visible nursing / limping lever |

Design consequence: "nursing" = power reduction (sounds normal) + our model treats the nursed engine as lower
load (reduced drain/growth) because we own the timer. No speed or rev caps. Findings appended to findings.jsonl
(ai-engine-timer-extend-holds-live, ai-maxspeed-cap-holds-but-sounds-artificial, ai-revlimit-write-holds-ai-does-not-shortshift,
ai-torque-power-scale-natural; all 2026-10-01).
- Trial 5 (owner request): power column x0.50 on slot 23 for 60 s (00-00-00-00-00-00): max rpm 16585, max 257 km/h = no change
  vs full power. INFERENCE: the AI may not use this torque table for its pace -> the E9p AI power lever and the -10% result
  (244 km/h) are in doubt. Owner ear check pending; if no visible effect, lever is dead and AI power symptoms need another path.
- Panel d8fc02f5: AI page "DRIVING NOW" = speed/rpm + throttle (veh+0x357C) and brake (veh+0x3584) bars. Field correlation live
  (n=485 accelerating: 0x357C 0.89 / 0x3584 0.00; n=133 hard braking: 0.14 / 0.71). F1CUI ThrottleFactor/BrakeFactor
  0x1198/0x1190 read 0 in the AI vehicle struct (different object). Window 1180x1080.
- Trial 5 CORRECTED: with power x0.50 held (peak torque 326.1 -> 163.0, own table), 60 s field comparison vs teammate
  Sospiri (full power): full-throttle accel 144-216 km/h 0.89 vs 5.01 m/s2, max 207 vs 261 km/h, mean throttle 0.91 vs 0.80.
  The power lever WORKS; the first 60 s max-speed reading (257) was a bad metric (speed carried onto the straight).

## 13. AI failure-rate source (owner question 2026-10-01: AI always drives at the same rate)
Proposal (Claude): AI WHEN = vanilla roll Normal(3 x Avg, 3 x Var)/race factor (live-proven fair); WHO = engine grade +
quality q, mean-preserving; track rev profile, heat/traffic (after the AI oil-thermostat fix) and contact damage are the
only usage terms that really vary for AI; driving-style terms dropped for AI (constant AI driving = noise). Law mode
becomes thermal/symptom display only, not the clock (removes the ~3.5x short AI lives). Open owner question: keep the
game's AI x3 life factor (vanilla) or make AI engines as fragile as the player.
- Why the AI x3 (owner asked): FACT the AI timer is Normal(3xAvg,3xVar)/factor and ticks 1 s/s (live z-test fit only with 3x).
  INFERENCE: the player's life drains by the game law 2^((rpm-base)/range) - ~3 s/s at typical race revs (1997 cars:
  base ~15900, range ~360, race ~16500 rpm), much more on the limiter - so x3 makes a constant-rate AI clock roughly
  equal to a player at race pace. Law mode double-counted (seed /3 AND rev drain) -> 3-7x too harsh. Recommendation:
  keep the vanilla x3 AI roll (pending owner OK).
- OWNER DECISION 2026-10-01: KEEP the vanilla x3 AI roll (recommended option). AI WHEN = vanilla Normal(3xAvg,3xVar)/race
  factor; WHO = grade + q mean-preserving; law mode = thermal/symptom display only (not the clock). Optional AI reliability
  slider, default 100 %.
- Rosset trial-5 restore (2026-10-01, Claude): NOT written. Table at 0x429DC3C8 (57 pairs, column 0 identical) reads
  power column peak 352.7 vs saved 510.9 and low-rpm values 19.5/27.6/35.6 vs saved 1.0/16.2/31.3 = not "saved x 0.5"
  (stable over 6 s, session time 1685 s). Unknown owner of current values -> no write; a game restart resets the table.
  Open: which column is power vs engine braking, and whether the saved bytes were the pre-trial original.
