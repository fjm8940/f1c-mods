# Messaging audit - HUD + radio, all mechanical failures (2026-10-01)

Owner request 2026-10-01: thorough analysis and fine-tuning of every HUD message and voice line and their
sequencing. Trigger: live water leak spammed "PIT AND RETIRE" while the car ran comfortably.
Artifact audited: installed dll 5EB02C58 (engine_failure.c 03D2BD50). Method: preprocessed the merged
engine build (`gcc -E` with build_merged.bat CFLAGS) so only ACTIVE (non-legacy) call sites are listed, plus
grep of gear/suspension/electronics sources. Status: ANALYSIS + PROPOSAL. Owner decisions pending (section 6).
Fact = read in code or seen in a live log. Inference = marked.

## 1. Machinery (fact)
| Piece | Behaviour |
|---|---|
| HUD | ONE mod line (`g_hud_msg`). `hud_show` overwrites it, holds 4 s (wall clock). Last writer in a pass wins. |
| Pump order per pass | niggle, model terminal, clockout ... then oil, heat, valve, oil-pressure, exhaust, blow-by, over-rev, cooling, fuel-pressure, fuel-leak, STOP THE CAR, wear watch, RETIRE04 pumps, hud_pump |
| `warn_after` (diagnosis) | condition must hold a random delay, then rising edge + 1 reminder / 45 s (R3c) |
| `instruct` (instruction) | same as warn_after, skipped when wrecked/stopped |
| `say_after` (radio) | condition held a random delay -> ONE radio per repair cycle |
| Colour (mf_hud.h) | red: SHIFT EARLIER. yellow: REVS DOWN, PIT FOR AIR, SHIFT EARLY, COOL THE OIL, COOL THE ENGINE, PIT - DEBRIS, EASE OFF, SAVE FUEL, PIT FOR FUEL. Everything else white (incl. PIT AND RETIRE). |
| Radio clips used | 130 MECHWARN0 "pit on this lap, the marshals are indicating you have a mechanical problem"; 49 PIT01 "pit this lap"; 77 "engine overheating"; 119 RETIRE04 "engine is blown, end of your race"; 76 MECHPROB14 (electronics); 80-93 gear lost/stuck; 75 MECHPROB16 suspension "avoid kerbs" |
| Radio queue | `commentary_say` writes the game's queue HEAD entry (one slot) |

## 2. Current messages per fault (active code)
| Fault | HUD diagnosis (white) | HUD instruction | Radio | Condition / delay |
|---|---|---|---|---|
| Valve return | LOSING REVS | REVS DOWN (yellow), VALVE FLOAT (white), SHIFT EARLIER (red) | none | valve p < 0.70 (1-3 s); revs above ceiling (6-10 s); float; h_vt < 0.6, suppressed if limiter <= shift point |
| Oil pressure | OIL PRESSURE | SHIFT EARLY (yellow), COOL THE OIL (yellow) | none | film < critical (1-3 s); bearing wear > 3 s (6-10 s); oil > opt + 5 K (8-14 s) |
| Oil leak | OIL PRESSURE, OIL TEMP | COOL THE OIL | 130 at loss >= 25 % (5-20 s); 77 when oil > limit (5-15 s) | blue smoke grows with oil lost |
| Water leak | WATER TEMP | COOL THE ENGINE (+18 K), PIT AND RETIRE (+18 K) | 130 steam (+12 K, 5-20 s); 49 PIT01 (+18 K, 5-10 s); 77 (+18 K, 5-12 s) | water over car baseline; R3g: forced on below 20 % coolant |
| Radiator debris | WATER TEMP, RADIATOR BLOCKED (wc > 105 C ABSOLUTE) | PIT - DEBRIS (8 s, yellow) | 49 PIT01 3 s after RADIATOR BLOCKED; 77 (+18 K) | RADIATOR CLEARED on clear |
| Fuel pressure | FUEL PRESSURE | SHIFT EARLY | none | fuel_cap < 0.70 (1.5-3 s / 6-10 s) |
| Fuel leak | FUEL LEAK | SAVE FUEL / PIT FOR FUEL | 49 PIT01 when pit needed (3-8 s) | fixed 20-40 s after onset; laps estimate |
| Exhaust | EXHAUST TEMP | WATCH THE REAR | 130 | ALL unconditional: radio 10-30 s, HUD 20-40 s, instruction 25-45 s after onset |
| Blow-by | ENGINE PROBLEM | EASE OFF | 130 | k > 0.3: radio 8-25 s, HUD 45-90 s, instruction 50-95 s |
| Over-rev | OVER-REV | - | - | rpm > 1.04 x limit (R3b grace) |
| Niggle | SENSOR FAULT | - | - | harmless, edge + 45 s |
| Any engine death | ENGINE FAILURE | STOP THE CAR (3-6 s after) | 119 RETIRE04 3-5 s after / stopped 2.5 s | every terminal kind |
| Save mode | ENGINE SAVE ON/OFF | - | - | panel |
| Repair | ENGINE REPAIRED | - | - | |
| Electronics | TC FAULT / LOW VOLTAGE / ELECTRONICS FAULT, RADIO COMMS LOST, ELECTRICAL FAILURE, ELECTRONICS OK | - | 76 | engineer notice delay |
| Gearbox | GEARBOX PROBLEM / GEARBOX FAILURE / NO DRIVE / ENGINE STALLED | - | 80-93 gear-specific | |
| Suspension | SUSP FAILING / SUSP FAILURE / SUSP COLLAPSED, SUSP OFF, SUSP REPAIRED, SUSP WHEELS ..., NO LIVE SESSION | - | 75 "avoid kerbs" | |

## 3. Problems found
Severity: S = owner-visible wrong/serious, M = confusing, L = polish.
1. (S) PIT AND RETIRE on temperature alone (+18 K) - live 2026-10-01 shown 400+ s while the car was fine.
   White, so it does not even look serious. ADDENDUM 12 T1.
2. (S) Water leak at +18 K fires THREE radio lines (49, 77, and 130 earlier) and two HUD lines at about the
   same moment. Live log 2026-10-01 t=172: 130, 77, 49 queued back to back. The game queue head is one slot,
   so (inference) only the last call is likely to survive - owner heard which? (open, section 6 Q5).
3. (S) Exhaust: "pit on this lap, marshals indicating a problem" radio 10-30 s after onset whether or not any
   flame was shown, and before the dash says anything. Owner live: "instant pop + PIT ON THIS LAP".
4. (S) RETIRE04 says "engine is blown" for EVERY death: seizure, fuel starvation, gasket, valve hits piston.
   Correct only for blow-up / rod. (Inference: clip wording; the clip list has no neutral retire clip found yet.)
5. (M) Lost first edges. R3c shows a line only on its rising edge. Two edges in the same pass (e.g. COOL THE
   ENGINE then PIT AND RETIRE in cooling_pump) -> the first is overwritten in the same pass and never seen
   until its 45 s reminder. Any 4 s hold can also be cut by a later pump.
6. (M) "SHIFT EARLY" (yellow, oil/fuel pressure) vs "SHIFT EARLIER" (red, valve) - nearly the same text,
   different colour and meaning.
7. (M) Valve return has 4 lines (LOSING REVS, REVS DOWN, VALVE FLOAT, SHIFT EARLIER) for one fault; REVS DOWN and
   SHIFT EARLIER both mean "rev lower". VALVE FLOAT is white although it is the act-now moment.
8. (M) Radiator debris uses an ABSOLUTE 105 C threshold for RADIATOR BLOCKED; all other water lines are car-
   relative (E9g). Cars with a cool baseline never get the diagnosis -> no PIT - DEBRIS call.
9. (M) Fuel pressure: R3h symptoms start at 0.90, warning only at 0.70 -> stumble with no dash reading; no
   message before the new R3i "fuel starvation" stop at the 0.35 floor.
10. (M) Blow-by: radio 130 says "pit on this lap" 8-25 s in, the HUD diagnosis only 45-90 s in, and blow-by
    can run a whole stint. Pit call is out of order and too strong.
11. (M) Oil leak: radio 130 "pit on this lap" at 25 % loss but nothing a pit stop can do (oil top-up not
    modelled) - inference, confirm with owner.
12. (L) Exhaust/fuel leak HUD on fixed timers, not on a symptom.
13. (L) Suspension strings are debug-style ("SUSP FAILING", "SUSP WHEELS ...", "NO LIVE SESSION") - HUD audit of
    2026-09-28 cleaned engine strings, not these. ENGINE STALLED comes from the gearbox family.
14. (L) PIT AND RETIRE / PIT - DEBRIS / PIT FOR FUEL / PIT FOR AIR: four different "pit" wordings.

## 4. Proposed message policy (for owner approval)
Tiers. Each fault gets at most ONE line per tier at a time.
| Tier | Meaning | Colour | Cadence |
|---|---|---|---|
| T0 symptom | driver feels / sees it, no text | - | - |
| T1 diagnosis | what the dash sensor / engineer sees | white | edge + 1 reminder / 60 s |
| T2 instruction | what to do; only if the driver's action changes the outcome | yellow | edge + 1 reminder / 45 s |
| T3 critical | "the engine will die soon" - pit/retire call | red | edge + 1 reminder / 90 s; one radio |
| End | engine dead | white | ENGINE FAILURE, then STOP THE CAR once; one radio |
Rules:
- T3 only on a real reason: model terminal predicted within ~90 s, or the component critical (coolant < 20 %,
  oil pickup starving, gasket critical, fuel laps < 1.8). Never on temperature alone.
- Radio: at most ONE radio line per fault before T3, one at T3, one at death. Minimum 15 s between any two
  mod radio calls (one central queue, no same-pass bursts).
- Order: symptom -> diagnosis -> instruction -> critical -> end. A radio never comes before the HUD
  diagnosis of the same fault, except marshals reporting VISIBLE smoke/flames that actually rendered.
- One central HUD arbiter: priority T3 > T2 > T1, a newly-risen line is queued, not lost; 4 s minimum on screen.
- One "pit" wording per purpose: BOX - <reason> (e.g. BOX - DEBRIS, BOX FOR FUEL), and RETIRE only at T3.

## 5. Proposed sequences per fault (draft wording; owner to approve)
| Fault | T1 diagnosis | T2 instruction | T3 critical (+radio) | Radio before T3 |
|---|---|---|---|---|
| Valve return | LOSING REVS | SHIFT EARLIER (yellow) | VALVE FLOAT -> red "SHIFT NOW" | none |
| Oil pressure | OIL PRESSURE | SHIFT EARLY -> rename "SHORT SHIFT" | bearings near end: "ENGINE CRITICAL" + 130 | none |
| Oil leak | OIL PRESSURE / OIL TEMP | COOL THE OIL | sump critical: "ENGINE CRITICAL" | 130 when blue smoke really visible |
| Water leak | WATER TEMP | COOL THE ENGINE | coolant < 20 % or gasket critical: "PIT AND RETIRE" + 49 | 130 when steam visible |
| Radiator debris | WATER TEMP -> RADIATOR BLOCKED (car-relative) | BOX - DEBRIS + 49 | water near boiling: COOL THE ENGINE escalates | none |
| Fuel pressure | FUEL PRESSURE at 0.90 | SHORT SHIFT below 0.80 | floor reached: "FUEL STARVATION" | none |
| Fuel leak | FUEL LEAK when loss measurable | SAVE FUEL / BOX FOR FUEL + 49 | laps < 1.8: as today | none |
| Exhaust | EXHAUST TEMP when pops start | WATCH THE REAR | - | 130 only after a flame actually rendered |
| Blow-by | ENGINE PROBLEM (power loss on telemetry) | EASE OFF | near terminal: "ENGINE CRITICAL" | 130 only when blow-by smoke visible, after T1 |
| Any death | ENGINE FAILURE | STOP THE CAR (once) | - | RETIRE04 only for blow-up/rod; other kinds: no radio or a neutral clip if one exists |

## 6. Owner decisions needed
Q1 Approve the tier policy (section 4)? Red for T3 critical (today red = SHIFT EARLIER only)?
Q2 Wording: keep English all caps short lines; OK with "ENGINE CRITICAL", "SHORT SHIFT", "BOX - DEBRIS",
   "BOX FOR FUEL", "SHIFT NOW"? Or other words?
Q3 Radio density: one line per fault before critical - too few / right / too many?
Q4 RETIRE04 "engine is blown": only for real blow-ups, silence for seizures/stops - OK? (Claude can search the
   game's clip table for a neutral retire line first.)
Q5 Live water leak t=172: which radio line(s) did you actually hear (pit / overheating / mechanical problem)?
Q6 Suspension/gearbox/electronics strings in scope now, or engine only first?

## 7. Next step
After owner answers: Claude writes the executable spec as ADDENDUM 12 T5 (central HUD arbiter + radio queue +
per-fault table) in docs/DEEPSEEK_AI_ENGINE_J_PROMPT.txt, with harness checks per fault (sequence order, no lost
edge, radio spacing, no T3 without a critical state). T1 of ADDENDUM 12 becomes part of T5.

## 8. IMPLEMENTED (Claude, 2026-10-01, owner: "do these fixes yourself"; defaults = Claude's proposal)
Dev dll **F9480382** (built twice, same hash). NOT installed. Before-copies:
`workspace_organization_audit/MESSAGING_BEFORE_2026-10-01/` (engine_failure.c 03D2BD50, mf_hud.h 685D7C0A,
dll 5EB02C58, README, this doc). Files after: engine_failure.c 0609C1D9, mf_hud.h 6C0C3376, new dll/mf_msg.h
A5F8285C, new tools/msg_test.c 2B60796E + tools/run_msg_test.bat FE3D085D. mf_engine_model.h untouched.
Owner addition mid-task: a live line (SHIFT NOW) must vanish the moment it no longer applies - no 1-2 s decay.

What changed (engine family only; legacy MF_LEGACY_* paths untouched):
- **HUD arbiter** (`mf_msg.h`): one line at a time, tiers diag/instr/crit/end, higher preempts (preempted line
  re-queued), equal/lower waits (no lost R3c edges), 4 s hold on the sim clock, stale queue 10 s.
  **Live lines** (`hud_live`): SHIFT NOW and STOP THE CAR show only while their condition holds and end
  within 150 ms. **Instant removal**: `mfh_drop` (mf_hud.h) zeroes our strip slot the pass a line ends or
  is replaced - no fade for any mod line.
- **Radio queue**: every call is a request; >= 15 s between mod calls, a death call drops pending ones (5 s gap),
  non-death calls dropped when wrecked/stopped, stale 30 s. Budget per fault: ONE pre-critical call
  (`say_pre`), one critical call (`say_crit`, eid 49 "pit this lap"), one death call.
- **Colours** (mf_hud.h): red = SHIFT NOW, ENGINE CRITICAL, PIT AND RETIRE, FUEL STARVATION; yellow = SHIFT
  EARLIER, SHORT SHIFT, COOL THE OIL, COOL THE ENGINE, BOX - DEBRIS, EASE OFF, SAVE FUEL, BOX FOR FUEL, WATCH THE
  REAR. Test checks mfm_tier and mfh_colour agree.
- **Cadence**: diagnosis reminder 60 s, instruction 45 s, critical 90 s. Instructions only after their diagnosis.
- Per fault:
  | Fault | Diagnosis | Instruction | Critical (red + eid 49) | Pre-critical radio |
  |---|---|---|---|---|
  | Valve | LOSING REVS | SHIFT EARLIER (limiter > shift point) | SHIFT NOW live while floating; ENGINE CRITICAL h_vt < 0.30 | none |
  | Oil pressure | OIL PRESSURE | SHORT SHIFT; COOL THE OIL only if no SHORT SHIFT | ENGINE CRITICAL h_b < 0.30 | none |
  | Oil leak | OIL PRESSURE, OIL TEMP | COOL THE OIL | ENGINE CRITICAL sump < 25 % or h_b < 0.30 | 130 at 25 % loss while running, else 77 |
  | Water leak | WATER TEMP | COOL THE ENGINE | PIT AND RETIRE coolant < 20 % or h_g < 0.35 | 130 while steam shows, else 77 |
  | Radiator debris | WATER TEMP, RADIATOR BLOCKED (car-relative +12 K, was 105 C) | BOX - DEBRIS 8 s | - | 49 call-in, else 77 |
  | Fuel pressure | FUEL PRESSURE < 0.90 (was 0.70) | SHORT SHIFT < 0.80 | FUEL STARVATION at the pump floor | none |
  | Fuel leak | FUEL LEAK | SAVE FUEL / BOX FOR FUEL | pit call = critical radio | none |
  | Exhaust | EXHAUST TEMP 3-8 s after the first pop | WATCH THE REAR after it | - | 130 after 3 flame spits |
  | Blow-by | ENGINE PROBLEM (45-90 s) | EASE OFF after it | ENGINE CRITICAL ring_s < 0.30 (stop 0.15) | 130 after diagnosis while blue smoke shows |
  | Death | ENGINE FAILURE | STOP THE CAR live, 3-8 s, while rolling | - | RETIRE04 only blow-up / game clock-out; mod stops silent |
- Retired strings: REVS DOWN, VALVE FLOAT, SHIFT EARLY, PIT - DEBRIS, PIT FOR FUEL (legacy paths keep them).

Verification (Claude, 2026-10-01): `tools\run_msg_test.bat` ALL PASS (57 checks: no lost same-pass line,
preempt + return, live on/off same pass, live ends <= 150 ms unrefreshed, end > critical, refresh hold, queue
full keeps critical, tier/colour agreement, radio gap/priority/death drop/quiet/stale/dedupe);
`tools\run_engine_model_test.bat` ALL PASS; merged build x2 F9480382; GUI unittest OK; workspace validator PASS.
Not run: car sweep (model header unchanged). Gearbox/suspension/electronics messages unchanged (Q6 default
"engine first").

Inference / limits:
- `mfh_drop` blanks the strip slot (timer 0, text empty). That the game hides a zero-timer slot at once is
  inferred from the strip fading on its timer - live check needed.
- ENGINE CRITICAL can stay up (90 s reminders) while the fairness floor holds a component near its terminal.
- The radio queue spaces calls inside the engine family only; electronics (eid 76) and gearbox calls are separate.
- Paused: the sim clock stops, so a shown line stays on screen until unpause.

OWNER LIVE CHECKS (after install of F9480382):
1. Valve: SHIFT NOW (red) appears when you rev into the float and is gone the instant you lift/shift.
2. Water leak: no PIT AND RETIRE while the car runs fine; it comes (red, once + 90 s reminder) only when the
   coolant is nearly gone; one radio line when steam shows; one "pit this lap" at critical.
3. Exhaust: no radio until flames have been seen; EXHAUST TEMP only after the first pop.
4. Any two messages at once: both shown one after another, none lost; lines vanish without a fade.
5. Death: ENGINE FAILURE, then STOP THE CAR only while rolling; "engine is blown" radio only on a blow-up.
