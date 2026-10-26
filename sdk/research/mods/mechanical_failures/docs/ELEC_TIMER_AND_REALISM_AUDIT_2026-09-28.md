# Electronics constructed timer, auto-on default, realism audit (2026-09-28)

Owner requests (2026-09-28, late session):
1. Ensure the system activates the normally frozen player lifetime values
   (suspension / engine / gearbox), and give ELECTRONICS - our own,
   non-vanilla category - a similar, artificially constructed timer.
2. Audit every failure type against real-world F1 behaviour ("STAGE 1 SMOKE
   for engine seems a bit weird - go through them all").

**Status:** development trial, ``. Part 1 implemented and
built; part 2 audit recommendations R1-R7 were **APPLIED later the same day (§7)** - originally: no behaviour change
from the audit has been applied yet** (awaiting owner choice, §4).
Before-state (copy-only): `workspace_organization_audit/ELEC_TIMER_REALISM_BEFORE_2026-09-28/`
(`RESTORE.md` + SHA256 per file).

## 1. Frozen lifetime values - verification (code-read, FACT tier = source)

| Family | Counter | Driven by | Source |
|---|---|---|---|
| Gearbox | `veh+0x3D78` | `timer_tick()` 1.0 / game-second via `dbl_8AFEA0`, 5 s jump guard | `dll/gear_failure.c` |
| Engine | `veh+0x3D70` | same | `dll/engine_failure.c` |
| Suspension | `veh+0x3D7C` | same | `f1c_ida_decompiled/mods/sdk_sandbox/suspension_failure/suspension_failure.c` (the file the merged build compiles; `dll/suspension_failure.c` is an older copy and is **not** built) |
| Brakes | `veh+0x3D74` | nothing (family pending) | - |

**Gap found and fixed:** every family's `timer_tick()` only runs when
`auto_enable != 0`, and `auto_enable` defaulted to **0** in all four DLLs
(the panel checkbox also defaulted off). The release ships without the panel,
so a shipped build would never have run any timer or produced any failure.
Fix: `auto_enable = 1` at DLL init in all four families (timer source was
already the default), and all four panel checkboxes default on.

## 2. Electronics constructed timer (ABI 1 -> 2)

The game seeds only four slots per car; there is no electronics slot. The
constructed timer copies the seeder's distribution:

- The player's four slots are four independent draws of
  `Normal(3*LifetimeAvg, 3*LifetimeVar) / raceDivisor` for this car and
  session (`LIFETIME_SEEDS_INVESTIGATION_2026-09-27.md`). Their mean and
  sample SD estimate that distribution's mean and SD. **Inference:** with
  n = 4 the SD estimate is noisy, so it is clamped to 10..60% of the mean
  (the engine.ini example Avg 10324 / Var 3517 gives Var/Avg = 0.34).
- Electronics life = one Box-Muller draw from `Normal(mean, sd)`; a
  non-positive draw falls back to the mean (mirrors the game's clamp).
- Seeded at the first live tick (`RacePhase > 4`), counted down 1.0 per
  game-second in **any** on-track session, same guards as the other
  families; on expiry `arm_electronics("TIMER")` (skipped after the player
  finished or if already faulty). Needs >= 2 usable slots, else idle
  (PLR failure mode 0/3 = FLT_MAX slots).
- Stored in the DLL only - no game field written.
- Fixed-chance roll (10%, race/qualifying) remains selectable
  (`timer_enable = 0`).
- Control block appended (append-only): `timer_enable`, `timer_remaining`,
  `cmd_set_timer`, `timer_set_ms`, `timer_seed_s`; 84 -> 104 bytes,
  `abi_version` 2. Panel: timer checkbox, live countdown with seeded value,
  `SET` tamper (same as the other tabs).
- Side effect: the electronics effects (misfire, radio loss) now also pump in
  practice/warm-up, consistent with the other families' timer scope.

### Verification (off-game)

| Check | Result |
|---|---|
| `build_merged.bat` (`-Wall -Wextra -Werror`, `_Static_assert` 104) | pass |
| New strings present in the merged DLL | pass |
| `mech_failure_test_gui.py --smoke-test` | PASS |
| `unittest` (`test_mech_failure_test_gui.py`, ELEC ABI now 26 int32) | 19/19 OK |
| Merged DLL SHA256 | `d307db357ceb1f57f7d6a73e8edae16d0fe048e18e31a21ae90de24b166c10b6` (302381 B) |

**Not installed into the game** (the installed copy is still
`c3bc89e1...`, pre-change). **Not live-validated.** The legacy standalone
`electronics_failure.dll` was not rebuilt.

Pending live checks: log line `TIMER: seeded from 4 slots mean=.. sd=..`;
panel countdown moves 1 s per game-second and pauses with the game; `SET 10`
-> fires `ELECTRONICS FAULT` + eid 76; a fresh session reseeds; with no panel
running, all four families still arm their timers (auto-on default).

## 3. Realism audit

Evidence tiers: **code** = what the source does (read today); **real** =
general knowledge of 1999-2002 F1 failure behaviour (not from a primary
source in this workspace); **opinion** = design judgement.

### 3.0 Cross-cutting findings (the big ones)

1. **All families share one failure-rate distribution (code).** Every slot is
   the *engine's* `LifetimeAvg/Var` x3, and electronics now copies it. So the
   expected time-to-failure of suspension = gearbox = engine = electronics.
   **Real:** in that era engines and hydraulics/gearboxes failed far more
   often than suspension (non-contact suspension failures were rare);
   electronics sat in between. Recommendation: per-family life multipliers
   applied at seeding, e.g. engine x1.0, gearbox x1.3, electronics x1.5,
   suspension x3.0 (owner to set the numbers).
2. **HUD names the diagnosis, not the symptom (real).** A driver cannot see
   his own smoke or know he has an "oil leak"; he sees dash warnings (oil
   pressure, water temp, gear indicator) and feels the car. The engineer
   diagnoses from telemetry and tells him on the radio. Recommendation: HUD
   = dash-style symptom; radio = diagnosis (already mostly true for radio).
3. **HUD states outcomes before they happen (code).** `ENGINE FAILURE`
   appears at the *start* of the ~100 s wear ramp; gearbox `STUCK` shows
   `GEARBOX FAILURE` during the early 20%-refusal phase.

### 3.1 Engine (weights smoke 30 / wear 30 / instant 10 / seizure 10 / oil 20)

| Type | What it does (code) | Real-world comparison | Verdict / recommendation |
|---|---|---|---|
| **Smoke** (stage one) | Life set to ~1.5 s -> vanilla stage-one: ReasonCode 1, one damage bank, radio line, then re-arms; white smoke, drivable; HUD `ENGINE SMOKE` | White smoke = coolant/water (head gasket, radiator) or oil on hot parts. Real cars rarely smoke *and carry on*; smoke is the prelude, usually retirement within a few laps. The driver cannot see it. | **Weakest type.** Re-model as **overheating / water leak**: HUD `WATER TEMP` (not SMOKE), radio **eid 77 PWS_MECHPROB15 "Engine overheating"** (exists, unused), progressive power loss, smoke only as the late visible sign, ending in failure unless the session ends first. |
| **Wear to death** | Life below -15.4, game ramps stress to 0.6 cap over ~100 s; HUD `ENGINE FAILURE` at onset | Realistic - gradual power loss (valve/piston wear) then expiry. | Keep the mechanics. HUD at onset should be `ENGINE PROBLEM` / `LOW POWER`; reserve `ENGINE FAILURE` for the end. |
| **Instant blow** | Stress straight to 0.6 cap; HUD `ENGINE BLOWN` | Realistic and the *commonest* era failure (conrod, pneumatic valve) - sudden, big smoke burst, often oil/flames, car coasts. | Add a short **black-smoke / flame bank burst** at the moment of the blow (levers exist, `emitter_toggle`). Consider a higher weight than 10% (owner). |
| **Seizure** | Running byte 0, no smoke, ReasonCode 1 | Seizures usually follow oil loss; a no-warning stop is more like an ECU/electrical cut. Real seizures can lock the rear wheels. | Plausible but mislabelled. Either rename HUD to `ENGINE STOPPED` (driver's view) or fold it into oil leak's end state. Low weight is right. |
| **Oil leak** | Pressure 100->0 in 60 s, blue smoke scaling, marshal warn eid 130 at 25%, stress from 50%, seizure at 0; HUD `OIL LEAK` | Realistic sequence (smoke -> meatball flag -> pressure loss -> failure). Blue smoke correct for burning oil. 60 s is short but playable. | Best-modelled type. HUD should be `OIL PRESSURE` (dash warning), not `OIL LEAK`. Optional: drain 90-180 s. |

### 3.2 Gearbox (lost 45 / stuck 30 / false neutral 15 / no drive 10)

| Type | Code | Real | Verdict |
|---|---|---|---|
| **Lost gear** | One random gear gone; 70% skips to next, 30% drops to neutral; progressive 25->60->100% refusals | Realistic: stripped dog rings / selector fork; intermittent before it goes. Very common in the era. | **Good.** |
| **Stuck in gear** | 20% refusal phase then locked; clunk sound on each refused shift | Realistic: loss of hydraulic pressure on semi-auto boxes left cars stuck in one gear - a classic 1999-2002 failure. | Good. Real hydraulic loss usually also kills the clutch actuation (car cannot pull away after a stop) - optional extra. HUD `GEARBOX FAILURE` during the 20% phase is premature -> `GEARBOX PROBLEM` until locked. |
| **False neutral** | Next shift drops out once | Realistic (actuator/sensor glitch). | Good. |
| **No drive** | Nothing engages; HUD `NO DRIVE` | Realistic (driveshaft / diff / clutch). Car coasts to a stop. | Good. `GEARBOX CRITICAL` banner elsewhere reads like a game term -> use `GEARBOX PROBLEM`. |

Gearbox is the most realistic family.

### 3.3 Suspension (FAILING 45 / FAILURE 35 / COLLAPSED 10, 10% no-fault on roll only; rear 50 / all 30 / front 20)

| Type | Code | Real | Verdict |
|---|---|---|---|
| **FAILING** | 2 x 0.30 bend, ~1-3 deg | Mild pull / changed balance = plausible for a damper or pushrod-mount problem. | OK as a "handling problem". |
| **FAILURE** | 3 x 0.60 bend, ~3-6 deg | Real non-contact suspension failures are metal fatigue: little or no warning, then a **sudden break** under peak load. A slow progressive bend without an impact is not how metal parts fail. | **Unrealistic progression.** |
| **COLLAPSED** | 30 x 1.0, clamp +-10 deg | Realistic end state (wishbone/pushrod snap). | Should usually be sudden, ideally triggered under load (braking zone / high-g corner) rather than at an arbitrary timer instant. |
| **"all four wheels" 30%** | bends all corners | Four simultaneous corner failures do not happen without an accident. | **Remove or reduce** to single-corner (rear-biased) failures. |

No HUD message is correct (the driver feels it); radio eid 75 fits. With the
shared distribution (§3.0.1) suspension fails far too often compared with reality.

### 3.4 Electronics (new category)

| Symptom | Code | Real | Verdict |
|---|---|---|---|
| **Misfire / cut** | 80-280 ms ignition cuts every 0.8-3 s for the rest of the session | ECU / loom / sensor faults do cause cutting, but usually in episodes (a few seconds, then clean), or a limp mode. Constant cutting every 1-3 s until the end is intrusive and unlike the real thing. | Make it **episodic** (bursts of 5-15 s, gaps of 30-120 s) or progressive. |
| **Radio comms lost** | 8-15 s windows every 60-150 s | Radio failures happened, but the radio is a separate system; a loss is usually permanent or long, not periodic and rhythmic. | Keep as a *possible* symptom, not an always-on one. |
| **Fuel starvation < 1 L** | Extra cutting when fuel < 1 L | This is a fuel-system/pickup issue, not electronics, and it only happens when nearly out of fuel (rare with normal strategy). | **Wrong category**; move it out or remove it. |
| **All symptoms at once** | Every armed fault gets all of them | Real electronics faults show *one* dominant symptom. | Roll **one symptom** per fault (e.g. cutting 50 / limp-mode power cap 20 / comms loss 15 / gearshift glitch 15). |

HUD `ELECTRONICS FAULT` is acceptable (dash warnings did exist); radio eid 76
fits.

## 4. Recommended change list (all applied - see §7)

| # | Change | Effort | Risk |
|---|---|---|---|
| R1 | HUD wording to dash-style symptoms (`WATER TEMP`, `OIL PRESSURE`, `ENGINE PROBLEM`, `GEARBOX PROBLEM`, `ENGINE STOPPED`); `FAILURE` only at the end state | small | low (strings) |
| R2 | Engine "smoke" -> overheating type with eid 77 + progressive power loss | medium | medium (needs live tuning) |
| R3 | Instant blow emits a smoke/flame burst | small | low (proven levers) |
| R4 | Per-family life multipliers at seeding (§3.0.1) | small | low; changes all rates |
| R5 | Suspension: drop "all four", make FAILURE/COLLAPSED sudden, bias to load events | medium | medium |
| R6 | Electronics: one symptom per fault, episodic cutting, fuel starvation out | medium | low |
| R7 | Stuck-gear hydraulic loss also blocks pulling away after a stop | medium | medium |

## 5. Files changed

- `dll/electronics_failure.c` - constructed timer, ABI 2, auto-on default,
  timer in any on-track session.
- `dll/gear_failure.c`, `dll/engine_failure.c`,
  `f1c_ida_decompiled/mods/sdk_sandbox/suspension_failure/suspension_failure.c`
  - `auto_enable = 1` default (one line each).
- `dll/mechanical_failures.dll` - rebuilt (hash above).
- `tools/mech_failure_test_gui.py` - ELEC ABI 2 fields, electronics timer UI,
  all auto checkboxes default on.
- `tools/test_mech_failure_test_gui.py` - ELEC ABI 2 assertions.

## 6. Workspace validator

`check-workspace-structure.ps1` after this task: **12 errors, 0 warnings**
(handoff baseline said 11/0). None come from this task: all are pre-existing
production source-file findings, top-level dirs dated 2026-09-01..09-24
(`.pytest_cache`, `f1 challenge testing versions`, `trials`), and
`F1 Challenge V10_clean` currently missing on disk (environmental; not
touched here). Not investigated further.

## 7. Fixes APPLIED (owner: "do all of these fixes", 2026-09-28)

Before-state (copy-only): `workspace_organization_audit/REALISM_FIXES_BEFORE_2026-09-28/`
(`RESTORE.md` + SHA256). The pre-session installed DLL (`c3bc89e1…`) is in
`ELEC_TIMER_REALISM_BEFORE_2026-09-28/`. One-shot patch scripts:
`trials/realism_2026-09-28/` ().

| # | Applied as | Where |
|---|---|---|
| R1 | HUD: `ENGINE PROBLEM` (stage 1 / wear onset) -> `ENGINE FAILURE` when the running byte drops; `ENGINE STOPPED` (seizure); `OIL PRESSURE` (oil onset) -> `ENGINE FAILURE`; gearbox `GEARBOX PROBLEM` until the fault is total, then `GEARBOX FAILURE`; `GEARBOX CRITICAL` removed | engine, gear |
| R2 | Auto type 1 is now **overheating** (`arm_overheat`): HUD `WATER TEMP` + radio eid 77; stress floor ramps from 40 s; bank 1 negative (steam under load) from 90 s; engine fails at 150 s (running 0, ReasonCode 1, retire line, 8 s plume). Last-lap guard downgrades blow/stop to overheating. Vanilla stage 1 kept as a manual test only. New `cmd_overheat` + `heat_s` (engine ABI 5 -> 6, 192 -> 200 B) | engine |
| R3 | Instant blow: black smoke bank 8 s + flame bank 2.5 s | engine |
| R4 | Life multiplier once at timer seed: engine x1.0, gearbox x1.3, electronics x1.5, suspension x3.0 (proposed values; tune after play). **Assumes** the game reseeds the slots every session (per `LIFETIME_SEEDS_INVESTIGATION`); if it did not, the x3.0 would compound - check the `TIMER: armed, counter=` log across two sessions | all four |
| R5 | Auto picks ONE corner (RL 33 / RR 32 / FL 18 / FR 17); FAILURE/COLLAPSED wait for hard braking (decel > 12 m/s^2 above 40 m/s, speed `veh+0x4440` FACT) or 60 s max; FAILING fires immediately. Manual buttons fire immediately | suspension (sandbox source) |
| R6 | Electronics: one symptom per fault - misfire episodes 50 (5-15 s cutting, 30-120 s clean) / limp mode 20 (stress floor 0.25) / radio outage 15 (60-180 s, next start 3-8 min later) / ECU cut-out 15 (engine dead 1.5-4 s every 1-3 min). Fuel starvation removed. `symptom` + `arm_symptom` appended (ELEC ABI 3, 112 B). **Deviation:** "gearshift glitch" replaced by ECU cut-out - the only lever (gearbox false-neutral arm) announces "lost Nth gear" and shows GEARBOX FAILURE | electronics |
| R7 | Stuck gear, once total: after the car has moved (> 10 m/s) and then stands (< 0.5 m/s) for 1.5 s, the engine stalls and is held off while stationary (no hydraulics for the clutch). HUD `ENGINE STALLED`. Never retires by itself | gear |

Panel (thorough pass): auto frames renamed to lifetime-timer wording with
each family's multiplier; engine one-click `OVERHEATING` (stage 1 moved to
Advanced as "Vanilla stage 1"), overheat progress in the status line, type
explanations; gearbox `FALSE NEUTRAL` button + hydraulic-loss note;
suspension buttons renamed `HANDLING PROBLEM / PART BROKEN / COLLAPSE` +
load-event note; electronics per-symptom buttons (`MISFIRE EPISODES`,
`LIMP MODE`, `RADIO OUTAGE`, `ECU CUT-OUT`, `RANDOM SYMPTOM`) and symptom
readout.

### Verification (off-game)

| Check | Result |
|---|---|
| `build_merged.bat` (`-Wall -Wextra -Werror`, static asserts engine 200 / elec 112) | pass |
| `--smoke-test` | PASS |
| unittest (engine ABI 50 int32, elec 28 int32) | 19/19 OK |
| Installed `<game>\plugins\lightloader\mechanical_failures.dll` | SHA256 `d9bc5926e82b923ba7e78bd893643e48d5921f8508485d0753d9d8c3dbbb1b22` = build |

**Not live-validated.** Legacy standalone DLLs not rebuilt. Engine ABI 6
may matter to any launcher binder of `F1C_ENGFAIL_CTL` (only the gearbox
binder is documented).

### Owner live test checklist (pending)

1. Lightloader: `LightLoader.log` shows `Loaded plug-in mechanical_failures.dll.`; panel host line `host verified - 4/4`.
2. Engine: OVERHEATING (WATER TEMP + radio, power fade, steam, fail at 150 s); WEAR (ENGINE PROBLEM -> ENGINE FAILURE); BLOW UP (black smoke + fire); OIL LEAK (OIL PRESSURE); SEIZURE (ENGINE STOPPED).
3. Gearbox: JAM -> PROBLEM -> FAILURE; stop the car -> ENGINE STALLED; FALSE NEUTRAL; LOSE 2ND.
4. Suspension: three buttons; auto with `SET 10` -> FAILURE waits for a hard stop.
5. Electronics: each symptom button; LIMP clearly down on power; CLEAR restores.
6. Timers: every tab counts down; logs show `x1.3` / `x3.0` / `x1.5` scaled seeds; two sessions in a row do not compound.

## 8. Simple test panel (owner: "looks terrible, redesign", 2026-09-28)

New `tools/mech_test_panel.py` + launcher `mech_test_panel.bat` (self-elevates).
One screen, no tabs: four cards (gearbox / engine / suspension / electronics),
each with OK/FAULT badge, plain status, countdown to the automatic failure,
"Fire auto in 10 s", red fault buttons that say what happens, one green fix
button. Game/on-track status on top, RESET ALL, short log at the bottom.
Reuses `mech_failure_test_gui.py` plumbing (Ctl + field maps); the old panel
stays as the full/advanced tool (`mech_test_gui.bat`). Screenshot:
`trials/realism_2026-09-28/panel_v2.png`.

Old-panel bugs found and handled:
- Host line always "merged DLL: not loaded": host `Ctl` had no unload event,
  so `attach()` always failed. New panel uses the gearbox unload event as
  liveness for the host block. (Old panel left as is.)
- "ENGINE DEAD" pill red whenever the engine is off, incl. menus. New panel
  only reports ENGINE DEAD when on track (`RacePhase > 4`).
- Old suspension COLLAPSE button wrote `cmd_stage3`, which does not exist
  (KeyError, nothing armed). Fixed in the old panel -> `cmd_terminal`.

Verification: `mech_test_panel.py --smoke-test` PASS; old panel smoke PASS;
unittest OK (+1 test: simple panel shares the blocks). Not yet used in-game.

## 9. Live timer values check (owner screenshot, 2026-09-28)

Live logs (`<game>\plugins\lightloader\*_log.txt`), session type 4:
gearbox counter 10478 s x1.3; suspension 39069 s (x3.0); electronics
"mean=18058 sd=10835 -> 40496 s". Earlier race (type 7): suspension 26383 s,
electronics mean 7326.

- **Expected (vanilla):** the game divides the seeds by the race divisor only
  in races (mode 2), so practice timers are hours long - AI identical.
  Suspension x3.0 is the R4 multiplier. Gearbox/engine first = by design.
- **Bug fixed:** electronics estimated its distribution from all four slots,
  but gearbox/suspension had already written their slots back scaled (x1.3 /
  x3.0), inflating mean and SD. Now reads engine + brakes slots only
  (`LIFE_SLOT_COUNT 2`). Before-state: `workspace_organization_audit/ELEC_SEED_FIX_BEFORE_2026-09-28/`.
- Built; **not installed** (game running; DLL locked). Install after closing
  the game: `dll\build_merged.bat install`.

## 10. Spectacular engine blow-up (owner, 2026-09-28)

"There should be a spectacular engine explosion into flames ... large flames
and possibly a sound effect." Upgraded `BLOW UP` (auto type instant blow):

- **Sound:** `dll/mf_engine_blow.wav` - original synthesized audio (crack,
  falling-pitch boom, metal rattle, fire crackle; 3.2 s, 44.1 kHz mono),
  generated by `tools/make_blow_sound.py` (deterministic). The game ships no
  explosion sample (only backfires/crash hits). Played with winmm
  `PlaySoundA` from the DLL's own folder - bypasses the game's Miles audio
  (no unproven game call). Volume is Windows volume, not the game slider.
- **Flames:** direct `ParticleTrailEmitter` bursts of the game's FLAMES (id 4)
  and BLACKSMOKE (id 3) reaction descriptors: 30-particle fireball + 20 smoke
  at the bang, heavy fire 2.5 s, dying fire to 9 s, black trail to 16 s;
  banks 2/3 stay on as a base. **Evidence:** only id 2 (blue smoke) is
  live-proven through this path; ids 3/4 are the same table - the log line
  `BLOW: fireball (flames found/MISSING ...)` settles it on first test.
- Build now links `-lwinmm`; `build_merged.bat install` also copies the WAV.
- Built `mechanical_failures.dll` (includes the §9 electronics fix). WAV
  copied to `<game>\plugins\lightloader\`; DLL **not installed** (game
  running). Before-state: `workspace_organization_audit/ENGINE_EXPLOSION_BEFORE_2026-09-28/`.

## 11. Blow-up v3: engine OFF + real fireball (owner test, 2026-09-28)

Owner result on v2: "blew up but produced smoke, should be a direct EXPLOSION
WITH SOUND AND FLAMES -> ENGINE OFF". Log confirmed sound played and both
descriptors were found.

**Root cause (FACT, `SeasonData\Circuits\terrain.ini` line 121):** vanilla
`Reaction=flames` is Scale 0.40, ASDEnvelope (0.07,0.17,0.4) = ~0.6 s life,
Max 256, alpha-blended - tiny and faint; `blacksmoke` is Scale 1.0, Growth
2.0, ~2.9 s life, Max 1024 - it buried the flames. And instant blow only set
stress to the cap - the engine did not switch off at the bang.

**v3 (built, not installed - game running):**
- Bang: `ENG_RUNNING = 0`, ReasonCode 1, retire line armed; held off while it
  burns. Sound as before.
- Flames descriptor tuned in memory like `mods/sdk_sandbox/fuel_fire_lab`
  (render fields only: Scale x3.5 then x2.0, Growth x2.5/x2.2, Power x2,
  RampSpeed x2.5, colour 0xFFD24A1A orange). Max is never written (lab
  finding: raising it froze the game). Originals restored at 8 s, on repair
  and on unload; skipped if the table was rebuilt.
- No smoke for the first 3 s; black smoke light from 3 s to 16 s.
- 40-particle flash, 14/tick for 1.5 s, then 10 -> 2 per tick to 8 s.

Before-state: `workspace_organization_audit/ENGINE_FIREBALL_BEFORE_2026-09-28/`.
Patch: `trials/realism_2026-09-28/patch_fireball.py`. `terrain.ini` not edited.

## 12. Owner live validation - BLOW UP v3 (2026-09-28)

- Installed build `d99c9022…` (includes v3 fireball, electronics seed fix,
  oil-trail kill switch). Lightloader 18:30:02: mechanical_failures loaded
  (unrelated `field_telemetry.dll` failed protected init that start).
- Engine log (earlier run): sound played, vanilla flames saved (scale 0.40,
  growth 1.50, colour 0x80FFFFFF), engine OFF, fireball, flames restored.
- **Owner: "BLOW UP works just fine."** Human validation PASS for the
  blow-up presentation (sound + flames + engine off). Other v3-era items
  (overheating, electronics symptoms, gear stall, suspension load gate,
  timers across sessions) still pending.

## 13. WEAR OUT never finished the engine (owner live, 2026-09-28)

- Owner: panel "LOSING POWER 100%" but still on track. Log: `ENGINE terminal
  armed: life = -16.00, stress = 0.0500`, no death afterwards.
- FACT (live): the vanilla ramp can hold stress at the 0.6 cap with the
  running byte still 1 - the earlier assumption (game clears it itself) is
  not reliable. The panel's "100%" meant stress/cap, not power.
- Fix: `wear_watch_pump()` kills the engine (running 0, ReasonCode 1,
  retire line, HUD ENGINE FAILURE) after 5 s at the cap. Panel now shows
  "engine strain x.xx / 0.60". Built; not installed (game running).
  Before-state: `workspace_organization_audit/WEAR_DEATH_FIX_BEFORE_2026-09-28/`.

## 14. Owner live test round + "symptoms first, diagnosis later" (2026-09-28)

Owner results: (1) gearbox JAM: radio said "stuck in fourth" while jammed in
sixth; ENGINE STALLED came far too late. (2) suspension PART BROKEN works;
REPAIR SUSPENSION breaks physics (expected - bend-field writes are known
unsafe). (3) LIMP MODE still ~290 km/h - not a limp. (4) ECU CUT-OUT cut
once (as designed, 1-3 min cadence). Owner rule: symptoms must appear first;
HUD/engineer only catch on realistically later - never at the fault instant.

Built (not installed if the game is running; see install line):
- Gearbox: named gear = the gear actually engaged (`cur_gear`), not the last
  refused shift; stall at < 4 m/s after 0.2 s (was 0.5 m/s + 1.5 s); no HUD
  at the first symptom - HUD + radio together 6-15 s after the box truly
  locks; phase HUD only after that.
- Engine (new layer): all dash warnings (VALVE AIR LOW, OIL PRESSURE, OIL
  TEMP, WATER TEMP, FUEL PRESSURE, ENGINE PROBLEM) and engineer lines (eid 77)
  gated by a 15-40 s "telemetry notice" delay rolled at arm; marshal eid 130
  (visible smoke) and catastrophic events (blow up, engine failure) instant.
- Electronics: radio 76 + HUD ELECTRONICS FAULT 20-45 s after arm; RADIO
  COMMS LOST HUD only after diagnosis (manual test window excepted). Limp
  mode = stress floor 0.45 + ignition rev cap at 72% of the limiter (live rpm
  veh+0x39A0).
- Panel: REPAIR SUSPENSION labelled unreliable; limp hint updated.
Before-state: `workspace_organization_audit/DELAYED_DIAG_BEFORE_2026-09-28/`.
Installed build: d3e6178f0789be32… (includes live-rpm fix §16 of engine research, cooked-engine end for debris).

## 15. Electronics subtypes (2026-09-28) - built, not installed (game running)

Owner: "go through ELECTRONICS now in a realistic manner and implement
ELECTRONICS failure subtypes". Before-state:
`workspace_organization_audit/ELEC_SUBTYPES_BEFORE_2026-09-28/`. Patches:
`trials/realism_2026-09-28/patch_elec_subtypes.py`, `patch_panel_elec.py`.

| Subtype | Real cause | Lever | Detection | End |
|---|---|---|---|---|
| Misfires, Limp, Radio outage, ECU cut-out | (existing) | ignition / stress / gate | as §14 | continues |
| **TC failure** (5) | TC sensor/logic (TC legal from 2001 Spain) | player aid global `dword_7CE6F4` cleared, held off, restored on clear/stint end (worker #2 E1, CONFIRMED static) | engineer 8-20 s, HUD TRACTION CONTROL FAULT | continues; if TC is off the roll becomes misfires |
| **Shift electronics** (6) | shift-control glitch | gearbox block `F1C_GEARFAIL_CTL`: one-shot FALSE NEUTRAL with the gearbox's own radio/HUD muted; window 20 s, pending glitch cancelled (cmd_clear) if no shift; every 20-60 s; skipped while a real gearbox fault is engaged | engineer 10-20 s after the first glitch | continues |
| **Alternator / low voltage** (7) | charging failure | battery 100 -> 0 over 180-360 s (game time); below 40% weak-spark ignition cuts; at 0 engine off, **ReasonCode 8** if unset | engineer sees voltage 10-20 s after < 85%, HUD LOW VOLTAGE; end HUD ELECTRICAL FAILURE | electronics retirement |

- Auto mix: misfires 25 / limp 15 / radio 10 / ECU 15 / TC 10 / shift 10 /
  alternator 15. `effects_release()` restores TC, ignition, stress and the
  gearbox block on clear and when the stint ends.
- ELEC ABI 3 -> 4 (112 -> 116 B, `battery_pct`). Simple panel electronics
  card: 3 columns, 7 subtypes, battery % in status
  (`trials/realism_2026-09-28/panel_v6.png`). Tests OK.
- Not rejected but not built: pit limiter stuck (only via the AI speed
  governor `sub_42ACD0` - unproven for the player), dash failure (no lever),
  telemetry loss (cross-family coupling).
- Live observation: panel showed `over-revs: 4` in the current session -
  either real money-shifts or downshift rpm spikes; watch for early engine
  deaths (OPEN).

## 16. Owner live test of the new subtypes (2026-09-28)

Installed `79c2a565…`. Log `plugins\lightloader\electronics_failure_log.txt`.
- TC FAILURE: armed (menu TC was on) but **TC stayed on**. Cause (source):
  `BrakeBiasSetup_00438cc0` copies the aid globals into the player physics
  object only at setup (this+114 = TC byte, 0x7C749A);
  `VehicleMainPhysicsUpdate_004441f0:334` reads the copy each tick. Fix
  built: the fault also writes/holds the physics byte and restores both.
- SHIFT GLITCH: dropped to neutral as designed - owner saw it twice.
  Validated.
- ALTERNATOR: engineer notice arrived; no felt change - weak-spark cuts only
  started below 40% (~2 min in). Fix built: failure 120-240 s; cuts from 60%
  battery, 0 -> ~1.2 cuts/s as the battery empties.
Built, not installed (game running).

## 17. TC validated; radio outage made audible (2026-09-28)

- Owner: **TC went off nicely** (log `TC: fault - traction control off
  (menu 2, physics 2)`) - validated.
- Owner: radio outage "didn't work". Log shows it DID run (73.4 s window,
  gate cleared, restored) - but a silent outage is indistinguishable from a
  quiet radio. Built: original synthesized `dll/mf_radio_static.wav`
  (`tools/make_radio_static.py`, hiss + crackle + squelch + garbled
  fragments, 1.4 s) played via winmm when comms drop, every 10-25 s (game
  time) during the outage, and whenever a race-engineer line is swallowed
  ("the engineer tried - you hear static"). `build_merged.bat install` now
  also copies the static WAV. Built, not installed (game running).
- Preservation incident: the batch copy
  `ELEC_SUBTYPES_BEFORE_2026-09-28/electronics_failure.c` was overwritten by
  a later copy; reconstructed deterministically and verified byte-exact
  against its recorded SHA256 (details in that batch's RESTORE.md).
- 2026-09-28: owner - radio lines still came through with RADIO OUTAGE armed. Log: the latest arm had not yet reached its first window (20-60 s). Fix built: comms drop 3-8 s after arm and stay down (10 h window) until cleared; static bursts continue. Not installed.
- 2026-09-29: owner - static heard where a line was swallowed; wanted mute. Log confirmed the outage works (eid 225 intercepted). Built: swallowed lines are muted without static; static only at drop + every 10-25 s. Not installed.
- 2026-09-29: owner - static sounded like an EVP frequency scanner. Regenerated mf_radio_static.wav v2: steady band-limited hiss, light crackle, 2.2 s, soft fade, no gating/tones (tools/make_radio_static.py). Copied to the game (WAV only).
- 2026-09-29: owner - a beep in the static; it was the 12 ms square-wave squelch click at each end. Removed; WAV regenerated and copied to the game.
- 2026-09-29: owner - no soft fade. Envelope now a hard start/stop (3 ms de-click only); regenerated and copied.
- 2026-09-29: owner approved the mix. Built: radio outage rolls 60% dead-for-session / 40% intermittent (back after 1-4 min, may drop again 1.5-5 min later; engineer calls 5 s after comms return). Not installed.
