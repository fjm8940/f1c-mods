# Gearbox simulation - specification (2026-10-12, Claude)

Status: Historical G1-G7 specification and evidence. G8 timer-first revision in section 9
supersedes the lifetime and early-hazard rules in sections 1-5; offline verification passed,
live owner validation is pending.
Owner request (chat 2026-10-12): "give the GEARBOX family the same treatment as we did with ENGINE: use the
native lifetime value as a determining factor when the failure takes place, add wear based on gearbox use",
scalable to the player, more gearbox issues for the AI. Follows `development/mods/MOD_STANDARD.md`.
Facts: [GEARBOX_SIM_INVESTIGATION_2026-10-12.md](GEARBOX_SIM_INVESTIGATION_2026-10-12.md).

## 1. Objective, non-goals, acceptance

Objective: one gearbox model for player and AI. The game's own gearbox counter (`veh+0x3D78`) is the life
budget; how the box is used spends it; a hazard on the spent fraction decides WHEN; the damage history decides
WHAT; the fault develops through intermittent symptoms to its terminal state.

Non-goals (this spec): clutch, differential, driveshaft (separate systems); gearbox faults in CO-OP (off until a
sync design exists); new radio lines for AI lost gears; pit repair; any change to the engine family.

Acceptance:
- A1 Failure Rate none (or an unseeded counter, Failure Rate 3) = no gearbox wear and no gearbox failure.
- A2 At the reference pace the wear rate is 1.0 s/s (vanilla tick) within 3 %; nursing lowers it, abuse raises it.
- A3 Player and AI use the same model, the same budget distribution and the same hazard.
- A4 The fault mix at the reference pace is the owner's 45/30/15/10 (lost/stuck/actuator/no drive) within 5 %.
- A5 A severe over-rev downshift can break that gear on the spot.
- A6 No terminal inside the last 2 laps (race); never an automatic player retirement (GP3).
- A7 AI stuck boxes end through the game's own path (ReasonCode 2, "stuck in Nth gear", pit and retire).
- A8 Every patch reversible, prologue-checked, game-thread only for game-state writes except the counter hold
  (same exception the engine family uses for `0x3D70`).

## 2. Model (G1, `dll/mf_gearbox_model.h`, pure C)

Wear rate (consumed life seconds per game second), reference lap = rev 0.85, 60 % full throttle / 40 % coast,
0.5 shifts/s, oil 95 C:

| Component | Formula | Share at reference | Books damage to |
|---|---|---|---|
| mesh | 0.60 x rev/0.85 x load^3 / E[load^3] x f(oil) | 0.60 | current gear (teeth/dogs) |
| bearing | 0.15 x rev/0.85 x load / E[load] x f(oil) | 0.15 | bearings |
| shift | 0.5 s per shift, x2 rapid (< 0.25 s), x(1 + 2 (over/5 %)^2) over-rev downshift | 0.25 | selector 50 %, actuator 30 %, target-gear dogs 20 % (+ harsh part 80 % dogs) |
| shock | 20 s x (rpm jump rate - 1.5 limit/s) in a settled gear, max 5 s/sample | 0 | bearings + current gear |

- `load` = throttle with a coast floor 0.15 (engine braking loads the coast flanks). Throttle is bimodal on a
  lap, so the norm is the lap mean of load^3 (0.601), not 0.6^3 (G1 test caught this: 2.8x error).
- Gearbox oil: first-order, tau 240 s, equilibrium 95 C at the reference pace and 60 m/s, hotter when slow and
  loaded. Wear factor e^((T - 95 C)/20 K), clamped 0.7..2.5. Internal state only (no game telemetry exists).
- Over-rev downshift: predicted rpm = learned rpm-per-m/s of the target gear x speed (learned online per gear;
  fallback 1.18 per gear step). The game's ratio table is not used (RETRACTED as physics source).
- WHEN: Weibull hazard in x = consumed / budget, beta 5, eta `MF_GBX_HAZARD_ETA`; at most one onset per
  session. Severe downshift (over >= 15 %): instant lost-gear onset of that gear with p = 0.05 + 2 x (over - 0.15),
  capped 0.5.
- WHAT: weights = damage bucket x share multiplier (gear 0.692, selector 2.4, actuator 2.0, bearing 0.667);
  the lost gear is drawn by damage squared (the most-worn gear dominates).
- HOW: development budget = min(remaining life / wear rate, lead 120-600 s), floor 60 s, no drive x0.5; the
  stage grows at wear-rate pace (clamp 0.3-3), 15 % stabilise between 30 and 70 % (not no drive).

Symptoms by stage s (per shift attempt):

| Fault | Warning (s < 1) | Terminal |
|---|---|---|
| lost gear | gear fails to engage 20 % + 80 % s; 70 % skip past / 30 % spit to neutral | gear gone (mask) |
| selector jam | shift refused 8 % + 50 % s | jammed in the current gear |
| actuator | false neutral 4 % + 30 % s, refused 5 % + 25 % s | no pressure = jammed in gear |
| no drive | pops out (false neutral) 2 % + 10 % s | no drive |

## 3. Frequency (owner decision D1)

At the reference pace a full-distance race spends x ~= 0.4 of a typical 2002 budget (5400 s / 3 x 4500).
Per car per race: eta 0.653 -> 8.3 %, **0.72 -> 5.2 % (implemented default)**, 0.82 -> 2.9 % (= vanilla AI
average, engine Q4b). beta 5 is steep: a box worn 1.4x faster than reference sits at x 0.56 -> 25 % at 0.72.
D1: keep 0.72, use 0.653 (engine parity), or 0.82 (vanilla count)?

## 4. Player integration (G2, `dll/gear_failure.c` + `dll/gbx_sim.h`)

- Worker (30 ms): read inputs (section 8 of the investigation), step the model with game-time dt; session reset
  on the live edge; the old fixed-chance roll and the vanilla-timer path stay for the dev panel and are bypassed
  while the model is on (`[Gearbox] Model=1`, default).
- Onset -> the existing proven fault path (cmd_arm): lost = MODE_SKIP + mask, stuck = MODE_STUCK, actuator =
  MODE_ACT (new: false neutral / refusal rolls, terminal switches to MODE_STUCK), no drive = MODE_ACT with
  refusal 0, terminal switches to MODE_NEUTRAL. The filter takes its chances from the model (per-mille) instead
  of the fixed phase table; the phase shown to the panel is derived from the stage.
- HUD/radio/clunk/R7 stall unchanged (announce once total). DNF stays off (GP3).
- Logs: `GBX:` session line (budget, eta), 60 s stats (x, rate, oil, shifts, harsh, components), onset,
  terminal, harsh downshift lines.

## 5. AI integration (G3)

- Worker: per AI slot (single player only; 1..VehicleCount-1, < 24): same model, AI inputs; the selected gear
  is the shift target while the shift timer runs.
- Counter hold: while the slot is live the native `veh+0x3D78` is held at remaining + 600 s (vanilla cannot fire
  first). STUCK/ACTUATOR terminal: write 0.001 -> the game's own case 2 (ReasonCode 2, stuck byte, "stuck in Nth
  gear", pit, retire).
- Game thread: detour `0x4EF930` (10-byte prologue, own patch variant: 8-byte CAS + thread gate over 10 bytes).
  Per call: refuse (skip the original) for stuck/actuator rolls; lost gear: substitute the next working gear in
  the shift direction, or refuse a downshift whose substitute would over-rev (> 98 % of the limit); no drive
  terminal: ReasonCode 2 + drive state 2 + neutral (the native broken-car path). AI false neutral = refusal
  (investigation inference: neutral at speed re-selects 1st).
- `[Gearbox] AIFailures=1` (default). Failure Rate none, CO-OP or instant replay: off and vanilla runs. Only local AI
  (`veh+0x32C8 == 1`, engine MF-12 rule) is modelled.
- Dev only (`MF_DEV_TOOLS`): INI `[Gearbox] DebugForceSlot` (0 player, 1-23 AI), `DebugForceFault` (1-4),
  `DebugForceAfterS` (60), `DebugForceTtfS` (120) arm one fault per session for the live test.

## 6. Batches and STOPs

| Batch | Content | State |
|---|---|---|
| G1 | model header + `tools/gearbox_model_test.c` | DONE, 39/39 PASS |
| G2 | player integration | DONE in dev, compiles; live test pending |
| G3 | AI integration + 0x4EF930 detour | DONE in dev, compiles; live test pending |
| G4 | panel: model readout + force onset (new shared block, GearCtl ABI untouched) | TODO |
| G5 | usage summary counts, address inventory (MF13) entries, compat gate item for 0x4EF930 | TODO |
| G6 | release: gearbox family on in the merged release (`MF_ENGINE_ONLY` off for gear), MOD_STANDARD audit of gear_failure.c (worker writes, R2) | TODO, owner GO |

STOP after G3: owner live test (section 7) before G4-G6.

## 7. Live test plan (owner)

1. Race, Failure Rate normal, dev build: `gear_failure_log.txt` shows `GBX: session live`, budget for the player
   and every AI slot, 60 s stats with rate near 1.0 at normal pace.
2. Count shifts per lap in the stats (calibrates `MF_GBX_SHIFT_S`); throttle duty from the rate split.
3. Harsh downshifts (bang down 3 gears at 300 km/h): `GBX: HARSH` lines, sometimes an instant lost gear.
4. Force an AI fault (G4 panel or a debug INI `[Gearbox] DebugAIFault=<slot>,<fault>`): the AI skips the lost
   gear / misses shifts / gets stuck and pits / coasts with no drive. Nothing else changes for other cars.
5. Failure Rate none: no `GBX` wear lines; AI counters untouched.

## 8. Verification done

- `tools/gearbox_model_test.c` (run `tools/run_gearbox_model_test.bat`): reference rate 0.996 s/s, oil 94.6 C,
  abuse 3.4x, gentle 0.54x; hazard share 0.993 vs Weibull 0.989 at the realised x; mix 41.6/33.4/15.9/9.1;
  instant break 175/400 at 35 % over-rev, always the target gear; terminal 202 s for a 200 s budget; gentle
  374 s; guard, stabilise, floor, determinism.
- Dev merged DLL (MF_DEV_TOOLS) builds `-Wall -Wextra -Werror`: SHA-256 `55F8457BCBE182AFDB6B52F209EF7F35E5551F6F571EE4503F5841C87C38F94E` (output in %TEMP%\gbxbuild,
  not installed, not copied into the tree). Non-dev gear TU also compiles. Engine model test ALL PASS (unchanged),
  MF13 inventory PASS (engine scope; gearbox addresses join in G5).
- NOT done: live run, release build, panel, usage report, address inventory.

## 9. G8 timer-first revision (2026-10-12; supersedes earlier timing rules)

The owner approved the engine-style concept after a completed 2002 100% race
showed 16 natural model gearbox onsets among 23 AI, though only Schumacher's
original gearbox lifetime would have expired during competitive running.
The player keeps the complex wear and fault-development model. The exact
race evidence and per-car table are in
`HANDOFF_2026-10-12b_GEARBOX.md` section 10.8.

### 9.1 Normal lifetime and early failures

1. The first valid, game-seeded `veh+0x3D78` counter is the original lifetime.
   For AI, the game continues ticking this native counter at its own rate;
   G8 no longer holds it above a wear-based remainder. Native expiry remains
   the ordinary AI gearbox failure path. For the player, whose game counter
   is seeded but frozen, the model advances a matching 1.0 s/s baseline clock
   while driving. At expiry, it arms a history-selected complex player fault
   through the existing progressive player path. The deadline arms once;
   an earlier repaired fault does not erase the original deadline.
2. Wear still books mesh, bearing, shift and shock damage, tracks oil heat,
   chooses the early fault type, and changes symptom development speed. It
   does **not** shorten the original lifetime or change the normal deadline.
   A severe over-rev downshift remains an additional immediate-break path.
3. The random *early* hazard runs on native lifetime age/budget (beta 5,
   provisional eta 1.80). The incremental hazard is multiplied by current
   wear-rate stress clamped 0.5..2.0, and stops at age/budget 1.0. At
   reference stress the cumulative chance by the normal deadline is 5.2%;
   the chance within a race depends on each original budget and race length.
   `[Gearbox] HazardEtaPm` remains the tuning control; prior eta 0.720 is
   invalid under this revised definition.
4. An early AI selector jam or shift-actuator terminal still writes 0.001
   to hand the car to the game's stuck/pit/retire path. Other early faults
   leave the native lifetime running. When the native counter expires, the
   model clears its AI shift symptoms and lets the native gearbox failure
   own the car. In non-race garage repair after an early native handoff,
   restore only the remaining original lifetime, never a wear-based value.
   Finished and retired AI slots stay frozen.
5. The dev panel ABI is 3 and publishes `baseline_elapsed_s` separately
   from `consumed_s`. "Box life left" and 10-minute early risk use baseline
   age; part-wear values and current wear rate remain diagnostic.

### 9.2 Verification gates

- Pure model: original deadline unaffected by gentle or abusive wear; player
  expiry arms once; AI model never preempts native expiry; off-track and
  Failure Rate none gates; severe downshift and all fault mixes still work.
- Integration: dev and non-dev translation units build with `-Werror`;
  dev panel smoke and ABI agreement; merged dev build reproducible.
- Completed offline on 2026-10-12: model full suite 113/113, timer-first
  subset 9/9, dev/non-dev gear TU `-Werror`, panel smoke, and twice-identical
  merged dev DLL SHA-256
  `DEA9A1D4853BABC8196F9A679620B2AF3F45E767938180D45DD2BBA1D6765D56`.
  The exact DLL was installed with the game closed; no G8 game observation yet.
- Live: normal-speed and Ctrl+T native-counter traces, player baseline
  expiry and early complex fault, AI native expiry, AI early fault including
  terminal handoff, non-race garage repair, and multiple full-race seeds.
  Release promotion remains blocked until owner gameplay validation and
  frequency calibration pass. Current installed DLL is the earlier G7
  candidate and is not evidence for G8 behavior.
