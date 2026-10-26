# AI fault presentation - plan (2026-10-01)

Owner request 2026-10-01: every engine fault gets an AI-driver version (reduced revs, changed sound, its own
smoke) and the vanilla AI retirement (black smoke / fire) is REMOVED and replaced by the fault's own ending.
Author: Claude (plan). Executor: DeepSeek, ADDENDUM 13 of `DEEPSEEK_AI_ENGINE_J_PROMPT.txt`. Start dll D7F23A4E
(installed). Status: PLAN - not implemented.

## 1. Today (fact, D7F23A4E)
- AI fault = torque-table power scaling only (engine_failure.c, MF_AI_POWER_TABLE). No smoke, sound or rev change.
- AI death = mod writes the game timer 0.001 (or the game's own clock runs out) -> vanilla engine failure:
  staged smoke failure (banks veh+0x3D54/58/5C, power x0.9, re-armed 1-500 s) or power column zeroed (coasting
  death), ReasonCode 1 (finding ai-vehicle-simulation-map-2026-09-30). Live 2026-10-01: Sato black smoke + fire.

## 2. Rules (unchanged, owner)
- AI WHEN stays the vanilla clock (q6 variant 1 exact; rate 1.0). Presentation only - never a new death time.
- Vanilla AI never reacts: no reaction-dependent damage (float damage already player-only). AI gets symptoms.
- Pause = no writes (R1b). Shared torque tables: no lever (H4b). Every write ownership-checked and restored.

## 3. Levers available (DERIVED unless marked; DS must verify before use)
| Lever | Address | Use |
|---|---|---|
| Power | torque table veh+0x39B0 / count 0x39BC | already used (power loss, misfire flicker) |
| Rev limit | veh+0x39A4 (FACT: AI rpm clamped to [idle 0x39A8, limit]) | valve ceiling, fuel top-end cut. The AI engine SOUND follows rpm |
| Smoke banks | veh+0x3D54 / 0x3D58 / 0x3D5C | per-fault colour/density. Live 2026-10-01 Sato read 0.0 / 0.72 / 0.97 after the vanilla failure |
| Sound | mf_spatial.h / mf_audio.h (R3j) | pops, hiss, bang at the AI car - needs the AI world position (to find) |
| Retirement | ReasonCode veh+0x4068, FinishState veh+0x4064 | mod-driven ending instead of the vanilla one (to verify) |

## 4. Per-fault AI version
| Fault | During (symptom phase) | Ending (replaces vanilla black smoke) |
|---|---|---|
| Valve return | rev limit = model ceiling (severity plateau, R3a) - audibly lower revs; power loss above peak; rare misfire flicker + pop | "valve hits piston": short grey puff + mechanical bang at the car, power to 0 at once, coasts; no fire |
| Oil pressure | power loss + rough-running flicker as h_b falls; no smoke | bearing seizure: power to 0 at once, small grey puff, no fire |
| Oil leak | BLUE smoke growing with oil lost (same scale as the player, R2a); power loss late | seizure: blue puffs, power to 0, light blue smoke while it rolls |
| Water leak | WHITE steam growing with coolant lost + 3D hiss; power loss once dry | head gasket: big white steam plume, power to 0, steam while cooling (no black) |
| Radiator debris | power loss as it overheats; white wisps near boiling | overheat stop: white steam, power to 0 |
| Fuel pressure | stumble flicker at high load (R3h curve), rev limit lowered below 0.80 pressure | fuel starvation: power to 0, no smoke, coasts |
| Fuel leak | no visible symptom (AI fuel is NOT drained - would change the WHEN) | runs dry: power to 0, no smoke. Fire terminal (if the model rolls it): flames bank + fire, stop |
| Exhaust crack | pops + short flame spits (bank) + 3D pop sound, rate by crack | exhaust burns through: flames + grey smoke burst, power to 0 |
| Blow-by | BLUE smoke under load growing with ring loss, power loss | ring stop: blue puffs, power to 0 |
| Blow-up (conrod / over-rev / manual) | - | the ONLY fireball: bang + fire + black smoke that decays (R3q timeline), power to 0 |
| Game clock / collision drop with no mod fault | T3 emergency onset gives a fault | that fault's ending (never vanilla) |

## 5. The hard part - replacing the vanilla ending
Option A (preferred): never let the vanilla engine failure fire. Keep timer ownership to the end; at our
terminal, the mod ends the engine itself (power column 0 = the vanilla "coasting death" lever, which already
exists) and sets ReasonCode 1 so the game retires the car normally (FinishState 2 after it stops, Vehicle
Removal 60 s, results show Engine). MUST be verified live: results screen, retirement, commentary, no stuck car.
Option B (fallback / scripted + collision paths where the game fires first): let the game fire, then OVERWRITE
the smoke banks every pass with the fault's ending (black suppressed) and keep power 0.
Prior art first (owner rule): grep red_flag, full_safety_car, pitstop_modding and older mechanical_failures
builds under workspace_organization_audit for AI ReasonCode / FinishState / smoke bank writes.

## 6. Gates
- Offline: a pure function ai_present(model state, fault, severity, phase) -> {rev_cap, power, bank[3], sound
  events, ending} with a harness table per fault (both phases), no black in any ending except blow-up.
- q6 variant 1 exact (0 missed / 0 extra), sweep, grade_test, model test, msg test, GUI test unchanged.
- AI pump stays <= today's 6 ms; writes only on change.
- Live (owner + Claude): forced all-fail race (Claude writes timers) - every fault seen once, no vanilla black
  smoke, cars retire and leave the track, results show Engine.

## 7. Open questions for the owner
1. Fuel leak: no visible symptom on AI OK? (real teams saw nothing until it ran dry)
2. Blow-up: keep fire + black smoke for the real blow-ups only?
3. Sounds at AI cars: only at close range (camera distance) - OK?

## 8. LIVE LEVER EXPERIMENTS (Claude writes, owner watched, 2026-10-01, D7F23A4E, 1998 race, pid 31184)
FACT unless marked. All writes owner-approved; Hakkinen restored.
- Rev limit veh+0x39A4 alone (Hakkinen 1833 -> 1650): rpm capped at 1650, BUT the AI stayed in 1st gear at the
  limiter (owner saw it; 133 km/h). The AI upshift rpm is a separate float veh+0x3A18 = 0.975 x limit on every car
  read (1786.8/1833, 1807.2/1853.5, 1684.7/1728). Writing BOTH (1650 / 1609): normal shifts 4-7 (gear veh+0x39FC),
  261 km/h, lower revs. RULE: an AI rev cap must lower 0x39A4 and 0x3A18 together and restore both.
- Smoke banks (Irvine, owner eyes): bank1 -0.5 white, bank1 +0.5 light grey, bank2 +0.5 white/grey, bank3 +0.5
  BLACK, bank3 1.0 FIRE (flames). Vanilla death (Sato) = bank2 0.72 + bank3 0.97. Banks persist until rewritten
  (set and read back on a parked car too). NO blue from any bank: blue = SpecialFX id 2 emitter call at a world
  position (player emit_bluesmoke_burst) - needs the AI position + an in-DLL call (open, A1/A4).
- Option A ending (Fisichella, no shared table): power column -> 0 + ReasonCode 1 -> coasts 277 -> 0 km/h, no
  smoke, but the engine keeps idling (rpm clamped to idle veh+0x39A8 = 398). Idle -> 0: rpm 0, engine silent, and
  FinishState 2 (retired) appeared at the next read (~60 s after he stopped - cause, idle write vs the stop
  rule, not separated). Game commentary then said the vanilla "he's blown the engine" (the game's own retire
  line for ReasonCode 1). Option A WORKS: no vanilla smoke, clean retirement, results Engine.
- Teammates do NOT share torque tables in this 1998 field (23 distinct pointers).
Consequences for ADDENDUM 13: endings = power column 0 + idle 0 + ReasonCode 1 (+ the fault's banks); symptom
banks: steam = bank1 negative, grey = bank1/2 positive, black = bank3 ~0.5, fire = bank3 ~1.0; the only blue
path is the emitter call. Open: is the "blown the engine" line acceptable for every fault (only clip known)?

## 9. Blue smoke path for AI (Claude code read + live position read, 2026-10-01)
FACT (code, engine_failure.c D7F23A4E): the player's blue smoke is NOT a bank. emit_bluesmoke_burst() queues a
burst (pq_push, critical section); the queue is drained on the GAME thread inside the 0x447B10 detour
(fuelcut_hook, player physics, every frame) by emit_bluesmoke_now(): find_bluesmoke_descriptor() (SpecialFX
reaction id 2) -> ParticleTrailEmitter 0x4CBC50(descriptor, simTime, pos, vel, rate, ...) with pos = the car's
world position veh+0x34E0/34E4/34E8 (+ jitter). Session-generation guard on every burst.
FACT (live read pid 31184): veh+0x34E0/E4/E8 is a valid moving world position for AI slots too (slot 1/2 moved
between 1 s reads; parked slot 10 constant at -91/-8/29).
=> AI blue smoke = the same queue with a slot field: PBurst gets `slot`, emit_bluesmoke_now(slot) uses
VEH_BASE + slot*0x48C0 + 0x34E0. Spawned on the game thread only (never from the worker). The same position
feeds mf_spatial for AI 3D sounds (pops/hiss/bang). Inference: works while the player car exists in the session
(the detour is on the player physics path) - check a spectate/no-player session.

## 10. LIVE SYMPTOM EXPERIMENTS (Claude writes, owner watched TV cam, 2026-10-01 g, D7F23A4E, 1998 race, pid 31184)
FACT unless marked. One running car (rc 0) per experiment; every value saved to a snapshot and restored after
(script `%TEMP%\mf_dyn\exp.py`, disposable). Torque tables all distinct (24 pointers) - no shared-table risk.
| # | Lever | Car | Result |
|---|---|---|---|
| 1 | power column (tq pair [1]) x 1.0/0.85/0.70/0.50, 45 s each | Irvine s4 | top 304/279/258/221 km/h; owner: visibly losing power |
| 2 | misfire: power column 0 for 40-120 ms, random gaps ~0.25-0.75 s, 60 s | Villeneuve s5 | owner: works (stutter visible) |
| 3 | top-end cut: power 0 while rpm >= 1550 rad/s (lim 1801) + upshift 0x3A18 = 0.975 x 1550, 20 ms poll | Frentzen s8 | max rpm 1551, top 275 km/h; owner: early upshifts, works |
| 4 | flame spits: bank3 1.0 for 0.3 s on throttle < 0.1, >= 1 s apart | Wurz s9 | owner: real flame bursts |
| 5 | smoke density bank1 -0.1/-0.3/-0.6, 20 s each | Alesi s13 | owner: white smoke grows with magnitude (graded) |
| 6 | limp home maxSpeed veh+0x4538 = 50 m/s | Barrichello s14 | owner: capped 180 km/h, cannot go faster (1 s reads peaked 270 only at write moment, decelerating) |
| 7 | rough idle 0x39A8 wobble +-30 % | Fisichella s10 (retired) | NOT RUN validly: retired car engine not simulated (rpm 0 even after idle 0 -> 397). Owner rule: never use retired drivers. Deferred: needs a running AI stopped (pit stop / post-race) |
Consequences for ADDENDUM 13: levers 1-6 usable as-is. Misfire/top-cut/spits need fast (game-thread or <= 20 ms)
writes; top cut must move 0x3A18 with the cut. Rough idle unproven - drop or test at a pit stop.
Pending: owner decision on death radio (section 0 action 3 of HANDOFF g); experiment 7 rerun.

### 10b. Second batch (same session, 2026-10-01 g)
| # | Lever | Car | Result |
|---|---|---|---|
| 8 | gradual rev loss: lim 0x39A4 100 -> 80 % over 90 s, upshift 0x3A18 = 0.975 x lim | Herbert s17 | FACT owner: fast GEAR HUNTING (down/up/down). Cause (inference): downshift threshold not moved. Rule: rev loss needs the AI downshift rpm (field NOT FOUND) moved too, or skip this symptom |
| 9 | running hot: bank1 0 -> 0.6 grey + power 1.0 -> 0.75 over 90 s | Trulli s18 | ran, restored; owner verdict not given |
| 10a | rpm jitter: veh+0x39A0 x (1 +- 8 %) every 10 ms, 30 s | D. Hill s6 | owner: "sounds very weird" (10a vs 10b not separated) |
| 10b | limiter flutter: 0x39A4 random 85-100 % every 80 ms, 30 s | D. Hill s6 | see 10a |
| 7b | idle 0x39A8 wobble +-30 %, then +-60 % + random dips | PLAYER s0 (parked) | owner heard wobble at +-30 %, unsure at +-60 % whether it was the native sound. Player rpm read at 0x39A0 stayed 417 and player FinishState was 3 -> UNPROVEN, player may not be simulated there |
Code read (FACT, f1c_ida_decompiled): `ScaleAudioPitch` 0x4F1510 is MISLABELED - it is a one-time physics scaler
called from AIDriverStructInit, not audio. The engine sound file loader (0x4F7460) loads ENGINERPMSOUND ranges via
RPMRangeLoader 0x4F6BF0, so engine pitch is rpm-driven (inference, matches section 8). No separate pitch field found.
