> **PARKED WIP 2026-10-12 (resume here):** [releases/WIP_2026-10-12_gearbox_G8/HANDOFF.md](releases/WIP_2026-10-12_gearbox_G8/HANDOFF.md) - gearbox G8 + launch-learn fix + AI engine revival/mild-R2; dev DLL `0EC606AA...` not installed, not shippable (CO-OP gearbox sync + gearbox usage logging missing). Public is v1.0.7. Dev `dll/mf_audio.h` already carries the 1.0.7 buffer fix.

> **START HERE - G8 TIMER-FIRST GEARBOX DEV 2026-10-12:** [docs/HANDOFF_2026-10-12b_GEARBOX.md](docs/HANDOFF_2026-10-12b_GEARBOX.md) sections 10.8-10.13. The G7 full race produced 16 natural AI onsets among 23; only Schumacher had an original native gearbox lifetime inside the competitive race. Owner approved a timer-first redesign: AI native counters now run normally, the player model counts the original lifetime at 1.0 s/s, and wear shapes a separately calibrated early-failure chance and complex fault symptoms. G8 dev DLL `DEA9A1D4...` is installed; source and installed hashes match. Exact-source model 113/113, focused 9/9, panel smoke, and reproducible dev build passed. In the first live 100% G8 race, one of 23 AI (Trulli) developed an early lost gear, then reached his captured original native lifetime. A second race is in progress on Ctrl+T: two early harsh-downshift lost-1st onsets are logged. Player fault behavior and long-run eta 1.80 incidence still need owner validation; no production promotion.

> **GEARBOX SIMULATION G1-G3 DEV 2026-10-12 (not shipped, not live-tested):** the gearbox family gets the engine treatment - the game's own gearbox counter `veh+0x3D78` is the life budget, wear from use (mesh load^3, bearings, shifts, over-rev downshifts, rpm shocks, gearbox oil heat), Weibull hazard decides WHEN, damage history decides WHAT (lost gear / selector jam / shift actuator / no drive), staged symptoms; same model for player and AI (AI via a new `0x4EF930` detour + native counter hold; jam ends through the game's own stuck-and-retire path). Model `dll/mf_gearbox_model.h` (`tools/gearbox_model_test.c` 39/39 PASS), integration `dll/gbx_sim_core.h` + `dll/gbx_sim_pump.h` in `gear_failure.c`; dev merged build `55F8457B...` in %TEMP% only. [Investigation](docs/GEARBOX_SIM_INVESTIGATION_2026-10-12.md), [spec + batches + live test plan](docs/GEARBOX_SIM_SPEC_2026-10-12.md). Pending: owner decision D1 (hazard eta), live test (spec section 7), G4-G6.

> **v1.0.7 SHIPPED 2026-10-12 (hotfix: crash on an AI engine blow-up, because the 3D sound buffer overflowed on the 44.1 kHz blow sound; owner waived the in-game test):** DLL `5513874A...`, ZIP `E9449AEC...`, catalog `12A1A771...` - [release record](../../lightloader/v2.0.0-hub-dev.1/release_mf_1.0.7_2026-10-12/README.md).

> **v1.0.6 SHIPPED 2026-10-11 (CO-OP: other players' blow-up/retire HUD lines; owner "fix em all and ship", no in-game test):** DLL `536E0044...`, ZIP `4690511E...`, catalog `DD4E1D07...` - [release record](../../lightloader/v2.0.0-hub-dev.1/release_mf_1.0.6_2026-10-11/README.md). Shipped together with Pit Stop Failures 1.1.4.

> **v1.0.5 SHIPPED 2026-10-10 (CO-OP sync, owner "ship this to VPS", no in-game test):** DLL `1E296598...`, ZIP `67332C3D...`, catalog `58689CBE...` - [release record](../../lightloader/v2.0.0-hub-dev.1/release_mf_1.0.5_2026-10-10/README.md). Needs coop_mode 0.9.3+ for CO-OP; single-player path unchanged by design.

> **CO-OP SYNC 2026-10-10 (investigate/spec, no code change):** MF hardcodes slot 0 as player; on a CO-OP client slot 0 is the host car copy, and CO-OP bots (slots 32-54) are outside the AI model. Spec + phases P1-P4: [docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md](docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md). Implemented 2026-10-10 (dev only, not installed/shipped; single-player path unchanged by design): see the Implementation section of that doc for files, builds, tests and the two-PC test list.

> **v1.0.3 STAGED 2026-10-08 (not deployed):** Parts M+N - AI crawler ratchet fix (derive from the saved rev limit) + field-median crawler guard + gear-safe shift points + ASIDE stall escalation + force refusal on an active fault; AI blue smoke now starts at 25% oil lost; new AI-only **BLOW-UP** fault 10 (8% of AI onsets, dev force + panel "10 blow-up", 50% limp-retire blow roll for ruined engines) with a shared-FLAMES owner list (restore only after the LAST blow; AI fireball + black smoke via the particle queue at the car). DEV DLL `ECF1CF48...`; release `releases/v1.0.3/` DLL `ED63E448...` (built twice, byte-identical); staged into release_2.2.6 (catalog `5264D298`, MF zip `555528BD`; STOP R3). STOP M/N/M2b/R3: `../DS_PRESHIP_BATCH_2026-10-08.md`; details: `docs/ENGINE_SIM_MODEL_DEEPSEEK_HANDOFF_2026-09-30.md` section 8 + `../STOP_R3_PRESHIP_2026-10-08.md`. Owner live check pending; deploy only after the owner's "GO".

> **B3 DEV-ONLY 2026-10-08 (pre-ship batch, not shipped, not deployed):** test panel **Force** on an AI driver - `tools/mech_test_panel.py` AI ENGINES page picks a driver + failure type; the dev DLL (`dll/mechanical_failures.dll` `CCF9CE00...`, channel `Local\MF_AI_FORCE_V1`, `MF_DEV_TOOLS`) applies it on the game thread through the existing AI symptom/reaction path. The release DLL was rebuilt from the changed source **byte-identical `83AD4CEC...`** and ignores the command. [Record](../STOP_B3_PRESHIP_2026-10-08.md).

> **v1.0.4 SHIPPED 2026-10-09 (owner "just ship", no in-game test before ship):** release DLL `07071002...`, ZIP `0DB90066...`, catalog `D6E214B3...` - [release manifest](../../lightloader/v2.0.0-hub-dev.1/release_mods_2026-10-09/MANIFEST.md). Content = the DEV 2026-10-09 line below.

> **DEV 2026-10-12 (installed, owner live test pending):** retired AI that rejoins in practice/qualifying gets a slot reset + fresh engine life (no inherited ~600 s timer); mild R2 (cap >= 90 %) no longer retires on the 180 s limp timer. [details](AI_FAILURE_SURVEY_2026-10-08.md#2026-10-12-fix-dev-installed-2343c0af-ai-revival--mild-r2).

> **DEV 2026-10-09 (superseded by v1.0.4 above):** stress-share units fix (fault type now follows driving; exhaust no longer pinned at 23 %), AI uses the player's hazard (`ai_onset_model` default 1, eta 0.653 - owner decision), usage counters count hazard/impact/AI onsets, thread-gate refusals carry a typed reason. Dev candidate `3E20FB9D...` in TEMP; all gates PASS; owner live test next. [details](docs/TESTER_REPORT_TESTER_A_EXHAUST_2026-10-08.md#owner-decisions-and-changes-2026-10-09), [report verification](../../../operations/vps/REPORT_DEFECT_VERIFICATION_2026-10-09.md).

> **TESTER LOG 2026-10-08 (Tester A, EXHAUST CRACKED):** [cause analysis](docs/TESTER_REPORT_TESTER_A_EXHAUST_2026-10-08.md) - random Weibull hazard onset (x=0.508, 24.8 % odds), not the impact and not abusive driving; player fault table pinned (exhaust 23 %) by a probable kelvin-vs-dimensionless stress-share mismatch - offline check pending, no code changed.

> **v1.0.2 STAGED 2026-10-08 (not deployed):** audit close-out batch M - final source promoted to `releases/v1.0.2/src` (incl. the Lightloader broker), COMP-01b pending-aware AI lever restore, MF-13 fixed-address inventory `docs/MF13_ADDRESS_INVENTORY.md` + `tools/mf13_inventory_test.py`. Release DLL `835C33B5...`, reproducible, no `MF_DEV_TOOLS` override channel. STAGED at STOP M for review; the earlier `releases/v1.0.1/` snapshot is preserved. All gates PASS (engine_model_test 67680, msg 835, voice 406, lamps 216/0, ai_pulloff 81/0, ai_react 61, notify 180/0, audit_proof after 47/0).

> **v1.0.1 STAGED 2026-10-07 (not deployed, owner live test first):** [tester fixes + native engineer voice](docs/TESTER_REPORT_TESTER_B_2026-10-07.md) - release `releases/v1.0.1/`, deploy bundle `../../lightloader/v2.0.0-hub-dev.1/release_mf_1.0.1_2026-10-07/`.

> **Issue review 2026-10-06:** [Audio leakage and friend crash evidence, limits and next checks](docs/MOD_ISSUE_REVIEW_2026-10-06.md). Root causes remain open; installed release hashes verified.

> **RELEASED 2026-10-06: Mechanical Failures v1.0.0** (engine family) via Lightloader v2.2.2 - [release record](../../lightloader/v2.0.0-hub-dev.1/release_2.2.2_2026-10-06/MANIFEST.md), [player guide](releases/v1.0.0-rc.1/pkg/mechanical_failures-guide.txt), handoff section 16. Release build: `releases/v1.0.0-rc.1/build.bat`.
> **START HERE - HANDOFF 2026-10-06:** [docs/HANDOFF_2026-10-06.md](docs/HANDOFF_2026-10-06.md) - section 8 ship pass: installed F24F0170 (sections 9-12: blow-up freeze fix, louder bang, log fixes, EngineerVoice INI switch) + player INI `mechanical_failures.ini` + SDT 9C734314 (voice at the name-clip -9 dBFS level), louder backfire, AI engine failures ON; next = owner live check, then release package.
> **LIVE TEST NEXT (2026-10-07):** [combined checklist for audit batches 1-5](docs/LIVE_TEST_AUDIT_FIXES_2026-10-07.md) - dev DLL 9643ED6E (pit-box, voice-order, fuel-advice fixes) installed in the main game 17:57; run 1 results logged.

> **TESTER REPORT Tester C (2026-10-07):** ["sensor failure" while spectating in the pits](docs/TESTER_REPORT_TESTER_C_2026-10-07.md) - it is the player's own harmless SENSOR FAULT niggle; 1.0.0 rolls/shows it whenever the session is live; 1.0.1 candidate gates on the circuit (live check pending).

> **DEEPSEEK AUDIT BATCH 4 (2026-10-07):** [prompt](docs/DEEPSEEK_AUDIT_BATCH4_PROMPT_2026-10-07.txt) - MF-12 only local AI modelled (veh+0x32C8 == 1, CONFIRMED) and nothing during instant-replay playback (0x8AB6E4 == 2, SDK f1c_live_replay.h); MF-09 commentary queue writes posted to the game thread via the Lightloader broker (F1CBrokerPost), old direct path kept only when no broker. Run by DeepSeek and accepted offline by Claude 2026-10-07; live checks pending; DLL not rebuilt. MF-13 (full fixed-address coverage of the compat gate) deferred to the promotion gate: needs Claude's address inventory, no code change yet.

> **DEEPSEEK AUDIT BATCH 3 (2026-10-07):** [prompt](docs/DEEPSEEK_AUDIT_BATCH3_PROMPT_2026-10-07.txt) - owner D4 player race retirement resets the player only; D5 voice token safety (MF-08; MF-09 deferred to IDA research); D6 AI override + gear detour dev builds only (MF-11, flag MF_DEV_TOOLS). Run by DeepSeek and accepted offline by Claude 2026-10-07; live checks pending; DLL not rebuilt.

> **DEEPSEEK AUDIT BATCH 2 (2026-10-07):** [prompt](docs/DEEPSEEK_AUDIT_BATCH2_PROMPT_2026-10-07.txt) - owner decisions D1 garage visit resets the player only (MF-02), D2 AI timer padding released on shutdown (MF-02), D3 MF never raises an AI maxSpeed (COMP-01, MF side; SC/RF unchanged). Run by DeepSeek and accepted offline by Claude 2026-10-07; live checks pending; DLL not rebuilt.

> **DEEPSEEK AUDIT BATCH 1 (2026-10-07):** [prompt](docs/DEEPSEEK_AUDIT_BATCH1_PROMPT_2026-10-07.txt) - mechanical fixes MF-01 (AI coolant fraction units), MF-04 (fault freeze off circuit), MF-10 (VoiceVolume 0 on native path), MF-03 (lever restore generation guard) from the 2026-10-07 audit. Run by DeepSeek and accepted offline by Claude 2026-10-07 (review in the DeepSeek handoff log); live checks pending; DLL not rebuilt. MF-02/COMP-01 wait for owner decisions.

> **DEEPSEEK N RUN (2026-10-06):** [notification audit prompt](docs/DEEPSEEK_NOTIFICATION_AUDIT_PROMPT_2026-10-06.txt) - every HUD/radio/lamp trigger vs rules R1-R9, tests for all, stops after N1 for Claude review.
> **LIVE TEST CHECKLIST (2026-10-06):** [fault-by-fault owner test list](docs/LIVE_FAULT_TEST_CHECKLIST_2026-10-06.md) - results go to HANDOFF section 7.
> **NATIVE SIMULATION MAP (2026-10-06):** [engine thermal + life, fuel, brakes, tyres, floor, weather, native warnings - one consolidated map with corrections](docs/NATIVE_SIMULATION_MAP_2026-10-06.md) - read before building any new failure.
> **STEERING WHEEL DISPLAY MAP (2026-10-06):** [every wheel readout, LCD page and LED decoded](docs/STEERING_WHEEL_DISPLAY_MAP_2026-10-06.md) - static FACT map; bulb positions pending live check.
> **FAULT EXPECTATIONS + VANILLA OVERHEAT (2026-10-05):** [Symptoms, timing, lamp clearance and realism limits](docs/FAULT_EXPECTATIONS_AND_VANILLA_OVERHEAT_2026-10-05.md) - native overheating is oil-driven; current offline timings are not native calibration.
> **INSTALLED 090FFA2E 2026-10-06 (DeepSeek N run N2-N7 + Claude dt fix; was 91A2D5A6) (temp-only lamp + shift lights incl. LIMP; live per-fault test in progress; was CD4C3FC3):** [docs/HANDOFF_2026-10-05.md section 7](docs/HANDOFF_2026-10-05.md#7-install-cd4c3fc3--live-fault-by-fault-test-2026-10-05-claude) - previous 73F30551 restorable; review skipped by owner choice.
> **AUDITFIX 2026-10-05 (source fixes complete; review next):** [docs/HANDOFF_2026-10-05.md](docs/HANDOFF_2026-10-05.md#6-auditfix-results) - X1-X6, tests and before/after evidence. Candidate CD4C3FC3 in TEMP; not installed. Batch log: [section 8](docs/ENGINE_SIM_MODEL_DEEPSEEK_HANDOFF_2026-09-30.md#8-log-append-one-entry-per-batch).
> **START HERE - HANDOFF 2026-10-05 (engine family):** [docs/HANDOFF_2026-10-05.md](docs/HANDOFF_2026-10-05.md) - appended AUDITFIX results supersede the earlier resume instructions. Next = Claude review of the repaired source/candidate; owner installation and live validation follow with the game closed. Previous dev/installed DLL 73F30551 and SDT 226E5402 remain. One step at a time.
> **ENGINEER VOICE 2026-10-05 (prior audio unchanged):** [docs/ENGINEER_VOICE_INTEGRATION_2026-10-05.md](docs/ENGINEER_VOICE_INTEGRATION_2026-10-05.md) - 35 appended lines, 351 clips. AUDITFIX X2 replaces six Pit-mod donors and restores only complete owned rows plus saved-score lookup. Pending donor playback, stitch/parity and multi-mod acceptance. No audio rebuild or installation in this run.
> **DEEPSEEK L RUN 2026-10-05 (historical; AUDITFIX supersedes next action):** [docs/DEEPSEEK_NATIVE_LAMPS_HUD_CALIBRATION_2026-10-05.txt](docs/DEEPSEEK_NATIVE_LAMPS_HUD_CALIBRATION_2026-10-05.txt) - native warning lamp/icon hooks (0x49B9A0 overheat, 0x49BA50 low fuel), HUD warning calibration per fault, custom icon (FTINGAME01) research. Installed 6EAC9959 (impulse impacts, cold-arm fix, water baseline floor) - log entry "Claude 2026-10-05".
> **HANDOFF 2026-10-02 b (superseded for the engine family):** [docs/HANDOFF_2026-10-02b.md](docs/HANDOFF_2026-10-02b.md) - AI CONTROL DISCOVERY: post-hook on AIMainUpdate = full control of an AI car (brake/throttle/steer, gears built); API docs/AI_OVERRIDE_API.md; installed 7F8E47A6, dev F67D486D not installed; pull-off still broken (corner weave -> abort).
> **AI OVERRIDE API 2026-10-02 (dev):** [docs/AI_OVERRIDE_API.md](docs/AI_OVERRIDE_API.md) - shared-memory ABI to drive one AI car (brake/throttle/steer/gears) from the DLL hooks; GUI tools/ai_brake_gui.py.
> **HANDOFF 2026-10-02 (superseded):** [docs/HANDOFF_2026-10-02.md](docs/HANDOFF_2026-10-02.md) - section 0 item 0: AI pull-off NOT working live (full brake after engine death from an unknown writer); installed 1A598DC5; keep panel 'AI engines' OFF; next = find the 0x3584 brake writer (run record s18).
> **AI PULL-OFF (ADDENDUM 15) 2026-10-02 (current):** [docs/AI_PULLOFF_RUN_2026-10-02.md](docs/AI_PULLOFF_RUN_2026-10-02.md) - P1-P8 in-DLL AIMainUpdate hook + pure pull-off/ASIDE state machine (one writer, no steering flicker), park via AI mode 4, fire fade; Claude review + double-verify fixed 8 defects (L/R mirror, wrong-side goal, steer accumulation, ...) - record s9-s10; live trial 1 -> ROLL roll-out fix (s11), dll BDC06AE9 INSTALLED 2026-10-02 (before 2D588299, D7F23A4E), all gates PASS; owner live checks s6 + s10.4; panel row still needs a .py edit.
> **AI SYMPTOMS + REACTIONS (ADDENDUM 14) 2026-10-02 (superseded):** [docs/AI_REACTIONS_RUN_2026-10-02.md](docs/AI_REACTIONS_RUN_2026-10-02.md) - B1-B7 implemented + offline verified, B8 fairness gates partially met (nursing works, 2005 mean -16.2 %, owner decision needed), dev dll 6CCBA4A2 NOT installed.
> **HANDOFF 2026-10-01 g (superseded):** [docs/HANDOFF_2026-10-01g.md](docs/HANDOFF_2026-10-01g.md) - installed D7F23A4E; AI lever facts; next = 7 live AI symptom experiments, then DS ADDENDUM 13.
> **AI FAULT PRESENTATION PLAN 2026-10-01:** [docs/AI_FAULT_PRESENTATION_PLAN_2026-10-01.md](docs/AI_FAULT_PRESENTATION_PLAN_2026-10-01.md) - per-fault AI symptoms (rev limit, smoke, sound) + fault-specific endings replacing the vanilla black-smoke AI retirement; DeepSeek ADDENDUM 13 (A1-A7), not started.
> **MESSAGING AUDIT 2026-10-01:** [docs/MESSAGING_AUDIT_2026-10-01.md](docs/MESSAGING_AUDIT_2026-10-01.md) - every HUD/radio line per fault, 14 problems (PIT AND RETIRE on temp only, radio bursts, exhaust pit call, RETIRE04 "blown" for all deaths, lost R3c edges), proposed tier policy; owner chose defaults; IMPLEMENTED in dev dll F9480382 (section 8).
> **HANDOFF 2026-09-30 (superseded):** [docs/HANDOFF_2026-09-30.md](docs/HANDOFF_2026-09-30.md) - engine model v2 state, pending DeepSeek batches E8f-h, next steps, facts, traps.
> **BIG LIVE TEST v2 (2026-10-01):** [docs/BIG_LIVE_TEST_v2_2026-10-01.md](docs/BIG_LIVE_TEST_v2_2026-10-01.md) - simple owner checklist for dll EB48658A (blocks A-F, expected symptoms per fault).
> **VANILLA HIDDEN FAILURE PATHS 2026-10-01:** [docs/VANILLA_RACE_EVENTS_FINDING_2026-10-01.md](docs/VANILLA_RACE_EVENTS_FINDING_2026-10-01.md) - Failure Rate 3 "season" scripts historical AI retirements from track .gdb RaceEvents (engine/brakes/gearbox/suspension on a set lap); StartStalls roll; slot 0x3D74 = brakes. Mod Q2 ownership likely delays scripted engine retirements (untested).
> **HANDOFF 2026-10-01 f (current):** [docs/HANDOFF_2026-10-01f.md](docs/HANDOFF_2026-10-01f.md) - R2+R3a-i+R3n reviewed, dev dll 5EB02C58 built + installed 2026-10-01 (dev = game copy); next = owner live checklist (batch log) + DS R3j-R3q.
> **HANDOFF 2026-10-01 e (superseded):** [docs/HANDOFF_2026-10-01e.md](docs/HANDOFF_2026-10-01e.md) - installed 5260D802; Block A+B live results; DS next = R2+R3 (R3a-R3q).
> **HANDOFF 2026-10-01 d (superseded):** [docs/HANDOFF_2026-10-01d.md](docs/HANDOFF_2026-10-01d.md)
> **ENGINE SIM MODEL v2 - DeepSeek handoff 2026-09-30 (E1-E8 built, owner test pending; not installed):** [docs/ENGINE_SIM_MODEL_DEEPSEEK_HANDOFF_2026-09-30.md](docs/ENGINE_SIM_MODEL_DEEPSEEK_HANDOFF_2026-09-30.md) - game's own engine life clock is the only authority on when (complementary, any car/season, no car-file changes); physical per-fault models (gas, viscosity, fatigue, vapour pressure) on top of the game's thermal model; old auto sources default OFF (game clock only; test-selectable); no radio/instructions when wrecked or stopped after an impact; all sounds via the game's own level chain, BLOW UP louder. Build `DC30CEA7` (E7). Claude: reviews, all GUI.
> **LIVE TEST 2 + STRESS PLAN 2026-09-30:** [docs/LIVE_TEST2_AND_STRESS_MODEL_PLAN_2026-09-30.md](docs/LIVE_TEST2_AND_STRESS_MODEL_PLAN_2026-09-30.md) - owner results (valve/water/over-rev weird), fixed-rate load model too flat, plan to drive every failure from engine.ini stress. Installed `d14616bc`.
> **HANDOFF 2026-09-29 (current):** [docs/HANDOFF_2026-09-29.md](docs/HANDOFF_2026-09-29.md) — state, paths, validation status, next work, traps, run commands. Installed `e68e7507…`.
> **LIVE CHECKS + VARIANCE 2026-09-29:** [docs/LIVE_CHECKS_2026-09-29.md](docs/LIVE_CHECKS_2026-09-29.md) — live check results, valve leak fix, per-subtype variance (`dll/mf_variance.h`, GUI "Variance:" line). Installed `a022b9f5…`.
> **HUD COLOURS 2026-09-29 (implemented, live test pending):** [docs/HUD_COLOUR_PLAN_2026-09-29.md](docs/HUD_COLOUR_PLAN_2026-09-29.md) — why mod messages inherit random colours, proposed red/yellow/white scheme, leak isolation.

> **Containment audit (2026-09-29):** [docs/FAILURE_CATALOGUE_2026-09-29_CONTAINMENT.md](docs/FAILURE_CATALOGUE_2026-09-29_CONTAINMENT.md) — no radio/HUD/value leaks across stints or sessions.

> **Retirement reasons RESULT (worker #2, 2026-09-28):** [docs/RETIREMENT_REASONS_INVESTIGATION_RESULT_2026-09-28.md](docs/RETIREMENT_REASONS_INVESTIGATION_RESULT_2026-09-28.md) — native brake model (disc/temp/grip per wheel, RC 5), blowouts, wheel-off RC 6, clutch via native stall, aids/pit limiter; catalogue revised (rev 2).

> **Failure catalogue (2026-09-28):** [docs/FAILURE_CATALOGUE_2026-09-28.md](docs/FAILURE_CATALOGUE_2026-09-28.md) — every failure with a reliability tier (A live-validated / B built / C composable now / D one test / E research).

> **Worker brief #2 (cheap LLM, 2026-09-28):** [docs/RETIREMENT_REASONS_INVESTIGATION_BRIEF_2026-09-28.md](docs/RETIREMENT_REASONS_INVESTIGATION_BRIEF_2026-09-28.md) — vanilla features for every non-engine retirement reason: brakes, clutch, tyres/wheels, hydraulics/steering/driveline, better use in gearbox/suspension/electronics/aero, retirement text/flags.

> **Vanilla features investigation RESULT (worker, 2026-09-28):** [docs/VANILLA_FEATURES_INVESTIGATION_RESULT_2026-09-28.md](docs/VANILLA_FEATURES_INVESTIGATION_RESULT_2026-09-28.md) — backfire sound path, native rev limiter band, radiator rewrite (fixed in N7), fuel/throttle levers, ReasonCodes; 12 OPEN live tests in §13.

> **2026-09-28:** cooling + fuel pressure failures installed (`51f24138…`, research doc §10). **Worker brief (cheap LLM):** [docs/VANILLA_FEATURES_INVESTIGATION_BRIEF_2026-09-28.md](docs/VANILLA_FEATURES_INVESTIGATION_BRIEF_2026-09-28.md) — 12 static questions on vanilla features (sounds, dash, rev limiter, radiator, fuel, commentary, throttle, FFB, pit repairs).

> **Engine failures v2 INSTALLED (2026-09-28):** valve air leak, oil pressure, oil leak v2, blow-by, exhaust crack, blow up + over-rev layer, built on top of the vanilla engine model — [research + build log §7-8](docs/ENGINE_FAILURES_RESEARCH_2026-09-28.md). DLL `81570bb8…`, not live-validated.

> **Engine failures research (2026-09-28, proposal):** [docs/ENGINE_FAILURES_RESEARCH_2026-09-28.md](docs/ENGINE_FAILURES_RESEARCH_2026-09-28.md) — current types low-diversity (all end engine-off, 3 share one power lever, WEAR/OVERHEATING recycle vanilla); proposed N1-N6 (valve air leak, oil pressure, oil leak v2, exhaust crack, blow-by, overrev) built on top of the vanilla model with driver agency.

> **Oil on track - feasibility (2026-09-28, research only):** [docs/OIL_ON_TRACK_FEASIBILITY_2026-09-28.md](docs/OIL_ON_TRACK_FEASIBILITY_2026-09-28.md) — visible trail probably yes (skid/groove ground marks via the proven emitter, spike needed); real grip loss needs new per-wheel RE; AI unknown.

> **Test panel (simple, 2026-09-28):** double-click `mech_test_panel.bat` — one screen, four cards, red = break it, green = fix it. Full/advanced panel: `mech_test_gui.bat`. See audit doc §8.

> **2026-09-28 (post-handoff): electronics timer + realism fixes R1-R7 APPLIED and INSTALLED:** [docs/ELEC_TIMER_AND_REALISM_AUDIT_2026-09-28.md](docs/ELEC_TIMER_AND_REALISM_AUDIT_2026-09-28.md) — auto+timer ON by default; electronics constructed timer, one symptom per fault; engine overheating type, dash-style HUD, blow-up smoke/fire; per-family life multipliers; single-corner load-triggered suspension breaks; stuck-gear stall. Installed `d9bc5926…`; NOT live-validated (owner checklist §7).

> **Handoff 2026-09-28 (end of day):** [docs/HANDOFF_2026-09-28.md](docs/HANDOFF_2026-09-28.md) — merged DLL + Lightloader startup, vanilla timer + preview/tamper, electronics family, HUD audit, live results, traps, and pending validation.


# Mechanical Failures — Player Gearbox, Engine, Suspension and Electronics Faults

This folder holds **four independent DLLs**:

| DLL | Covers | Control | Status |
|---|---|---|---|
| `dll/gear_failure.dll` | gearbox: stuck / lost gears / neutral | GUI over shared memory | shipped, driver-confirmed |
| `dll/engine_failure.dll` | engine: staged wear death, seizure, smoke/fire emitters | GUI over shared memory, in-game HUD | shipped, driver-confirmed |
| `dll/suspension_failure.dll` | suspension: staged wheel-bend damage via the game's own input | GUI over shared memory, in-game HUD | shipped, live-proven 2026-08-08 |
| `dll/electronics_failure.dll` | electronics: periodic radio-comms loss, misfire cutout, fuel<1 L starvation cutting (player-specific) | GUI over shared memory, in-game HUD | **new 2026-09-28, live validation pending** |

**Injection target since 2026-09-28: `dll/mechanical_failures.dll` (merged).**
It links all four family sources into one module - every family keeps its own
source file, control block and unload event, so the test panel works
unchanged; `LOAD MERGED DLL` injects it once, and any tab's Unload stops all
families. The four individual DLLs remain as legacy development artifacts
(do not inject them together with the merged DLL). Build:
`dll/build_merged.bat`. **It is also installed into the game as a Lightloader
plug-in** (`plugins\lightloader\`, verified against the v1.1.0 protected host
proof; fails closed under an ordinary injector; dev-tree injector exception
in `mechanical_failures_dev.ini`, release ships without it). The panel's top
line shows `Lightloader host verified` vs `dev injector`. Record:
[docs/MERGED_DLL_2026-09-28.md](docs/MERGED_DLL_2026-09-28.md). **2026-09-28 timer preview + tamper (ABI 5):** the panel shows each family's live vanilla countdown and the pre-rolled fault that expiry will fire, with a `SET` control that rewrites the timer live (e.g. 10 s); the timer source now runs in any on-track session (roll stays race/qualifying) — [docs/TIMER_PREVIEW_AND_TAMPER_2026-09-28.md](docs/TIMER_PREVIEW_AND_TAMPER_2026-09-28.md).

They share no hook site and can be injected together. The rest of this file
documents the gearbox DLL; the engine one is summarised in
[§ Engine failures](#engine-failures-engine_failuredll) below and mapped in full
in [`docs/ENGINE_FAILURE.md`](../../docs/ENGINE_FAILURE.md); the suspension one
in [§ Suspension failures](#suspension-failures-suspension_failuredll).

---

## Gearbox failures (`gear_failure.dll`)

Gives the **player** a jammed or stripped gearbox. Two failure modes, one hook.

| Mode | Behaviour | Real-world analogue |
|------|-----------|---------------------|
| **stuck** | No gear change at all. The car is locked in whatever gear it was in. | Selector fork jammed |
| **lost gear(s)** | Any number of gears can be gone at once. Selecting one either **skips past** it to the next working gear, or **drops to neutral** (recoverable — select another gear). | Stripped dog teeth / worn dog rings |
| **neutral** | Terminal: nothing engages at all, the engine free-revs. Optional recovery timer. | Gearbox destroyed |
| **both** | stuck + lost | |

Lost gears are chosen with a tick per gear, so "lost 2nd and 3rd" gives a box
that jumps 1↔4. If *every* gear is lost the walk falls through to neutral.

## Files

```
dll/gear_failure.c      the mod (detour on the player SetGear)
dll/gear_failure.dll    build output
mechanical_failure_gui.bat  launcher (elevates itself)
../../tools/mechanical_failure_gui.py   manual trialing GUI
```

## Use

```bash
python tools/mechanical_failure_gui.py
```

1. Start the game, get in the car
2. **Inject DLL**
3. Pick a mode and parameters
4. **ARM FAILURE** while driving — `Stuck gear = 0` locks whatever gear you're in
5. **CLEAR** to release, **Unload DLL** to remove the hook and self-eject

Needs admin (`OpenProcess`, injection). Hot-swappable: `Unload DLL` restores the
original bytes and frees the module, so you can rebuild and re-inject without
restarting the game.

## How it works

`PositionLapStateUpdate` at **0x447E00** is the player's `SetGear` — misnamed in
the corpus; it is the player counterpart of the AI's `sub_4EF930`.

```
__thiscall, this = 0x7C7428 (static player physics object)
requested gear   = [esp+4]
called from VehicleMainPhysicsUpdate 0x4441F0

this+0x1EF0 = 0x7C9318   current gear (zeroed while shifting)
this+0x1EF4 = 0x7C931C   target gear
this+0x1EF8 = 0x7C9320   shift direction (+1 up / −1 down)
```

Its first line is `if (newGear == this->[0x1EF4]) return;` — the game's own
no-op path. The mod reuses it:

```c
stuck   : if (want != locked) want = locked;   /* function returns, nothing happens */
lost    : if (broken_mask & (1 << want))       /* walk to the next working gear,   */
              want = next_working(want, cur);  /* or 0 if there is none            */
neutral : want = 0;                            /* the game's own arg==0 branch     */
```

Passing `0` works because the setter has an explicit `arg == 0` branch that sets
the target to neutral — we're using the game's own path, not forcing a value.

**Every filter branch must gate on `engaged`, not `enabled`.** `CLEAR` clears
`engaged`; a branch that checks only `enabled` keeps running after the driver has
switched the fault off. That shipped once and left the car unable to upshift out
of 1st with no way to release it.

A 5-byte `jmp` replaces the prologue (`8B 44 24 04 | 56` — a whole-instruction
boundary), vectors through a stub that saves state, calls the filter, then runs
the stolen bytes and resumes at 0x447E05. The hook refuses to install if the
prologue bytes don't match what was reversed.

Because this intercepts the **decision**, nothing races the game. One
interception per shift attempt.

## Why not a memory poke (read this before "simplifying" it)

Every offset the original polling script wrote is downstream of the decision and
does nothing. All live-tested 2026-07-30, all in `findings.jsonl`:

| Offset | What it really is | Effect of writing it |
|---|---|---|
| `veh+0x3A10` | genuine shift lockout — but gates `sub_426330`, which the **player never enters** | none |
| `veh+0x39FC` | display mirror, rewritten ~400 Hz from `0x7C9318` by `0x4E4C4F` | HUD moves, car shifts normally |
| `veh+0x39DC` | genuine gear ratios from `.hdv [DRIVELINE]`, but read **once at session load** | held perfectly, changed nothing |
| `0x7C9318` | the gear itself, but written by the setter every frame | **stuck the throttle at full** and glitched the gears |

A 50 ms poll loses every frame; a 94 kHz tight loop held `veh+0x39FC` 99.9% of
the time and still didn't change the car. Persistence is not causation — three
separate offsets held under write and did nothing.

## No vanilla equivalent exists for the player

The vanilla mechanism is real but **AI-only**:

- `FuelConsumptionAndWear_004ed960` case 2 sets `ReasonCode = 2` (Gearbox) and
  raises a byte at `veh+0x39FA`
- `sub_4EF930` tests that byte at `0x4EF934` and skips all shift logic
- but the player never enters `sub_4EF930` — the flag was set and held on the
  player car for 12 s while the driver shifted freely 1→2→1→3

`.PLR` `Mechanical Failures` and `Mechanical failure rate (0-none, 1-normal,
2-timescaled, 3-season)` feed that same AI-only path. Failure categories
(`FailureCategoryNameLookup_004bb730`): 1 Engine, **2 Gearbox**, 3/7 Clutch,
4 Suspension, 5 Brakes, 6 Accident, 8 Electronics, 9 Fuel.

## Verification

Both modes driver-confirmed in-car, 2026-07-30:

- **stuck** — 3867 setter calls, 3862 shift attempts blocked, gear never moved.
  *"locked perfectly at 1."* No throttle or handling side effects.
- **skip** — broken gear 2: 22 redirects, sequence `1,3,1,3,1,3…`, 2nd never
  engaged, a normal `3→4` in the middle proving other gears are untouched.
  *"sounds like a gearbox glitch on each shift."*

## Build

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -o gear_failure.dll gear_failure.c -lkernel32
```

## Commentary — 14 cut clips restored

The player's own gearbox lines are `PWS_MECHPROB20`–`33`. **All 14 ship with
cooldown `FLT_MAX`, i.e. unplayable in vanilla** — same cut-content signature as
PWS_HIT01-06. The DLL clears the cooldown at load (idempotent), and the same
change is registered as patch `gearbox-commentary-unblock` (14 sites).

| eid | sdt | line |
|-----|-----|------|
| 80–86 | 79–85 | Lost 1st … 7th gear |
| 87–93 | 86–92 | Stuck in 1st … 7th gear |

`EID_LOST(g) = 79 + g`, `EID_STUCK(g) = 86 + g`.

Heard in-game 2026-07-30: eid 88 *"stuck in 2nd"*, 89 *"stuck in 3rd"*,
84 *"lost 5th"*. Each had `EVENT_TS = -1.0` beforehand — never played, ever.

### Two eid traps, both live-tested

- **eids 201–217 are the OTHER-DRIVER lines.** The previous README mapped
  stuck/lost to these. Injecting 213 plays *"[driver] is stuck in second gear"* —
  third-person, about another car. That family (`PWS_AIMECHPROB*`, siblings
  `RE_OD_STUCK_7`/`RE_OD_LOST_7`) is correct only for AI.
- **eids 340–383 are the TV channel.** They are unblocked duplicates of these
  very clips and look like a shortcut, but they sit above eid 291 and the channel
  is chosen purely by eid range (1-290 PWS / 292-535 TV). They resolve against
  `PC_TV_MAIN.SDT`. Same trap that cost two sessions on PWS_QUAL28/29/30.

### Injection bugs this replaces

The old script never spoke, for three compounding reasons — all fixed in
`pump_commentary()`:

1. It bailed whenever `CONTROLLER+0xAE4` (`isPlaying`) was set. That flag reads 1
   during ordinary driving and is already logged as false-positive-prone.
2. It confirmed success by reading back the queue head, which drains within one
   poll — so clips that played were reported failed.
3. It set its `fired` flag even when injection returned false, so one busy tick
   killed the line for the rest of the race.

Now: ignore `isPlaying`, retry every 500 ms for **20 s**, and confirm by
`EVENT_TS(eid) > 0`.

Two refinements learned the hard way:

- **Detect with `> 0`, not "changed from a captured value."** The injection
  itself writes `EVENT_TS = 0`, so a retry rewrites the very field the check
  depends on. Only the game writes a session time there, so any positive value
  means it spoke. The old check reported `GAVE UP` on a clip whose `EVENT_TS`
  read 5471.146 afterwards — it had played.
- **20 s, not 8 s.** While racing the queue is busy with other commentary
  (eids 123/114/222 were observed competing), and an injected clip can be
  displaced for seconds. After the fix the same clip logged
  `PLAYED after 31ms`.

## Realism model

**A stuck gear is binary; a lost gear is gradual.** Two different phase tables:

| | phase 1 | phase 2 | phase 3 |
|---|---|---|---|
| stuck | 20% of shifts refused | **100%** | 100% |
| lost | 25% | 60% | 100% |

A jammed selector either holds or it doesn't — rolling a dice on every shift
forever reads as random glitching, not a failure. Worn dog rings genuinely do
fail intermittently first, so the lost-gear ramp stays gradual. Default
escalation is 10 s per phase.

**A stuck fault may only ever refuse a change, never cause one.** It substitutes
the *current* gear, which triggers the setter's own `newGear == current → return`
path. Substituting the gear captured at arm time was a real bug: once one shift
got through during the warning phase the value was stale, so the setter actively
engaged it and yanked the car down a gear.

**A lost gear costs time.** The first grab spits into neutral; only a second
attempt within 1.5 s forces the next working gear. Without that, `1→3` was as
quick as `1→2` and the "failure" was strictly an advantage.

**Refused shifts make a noise.** The stuck path returns before both of the
setter's sound calls, so a refusal was completely silent and read as a dropped
input. The mod fires the down/neutral sound `0x4F89A0` on every refusal.

### Radio timing

The engineer speaks **only once the box has actually failed**, then after a
2.5 s delay, naming the gear at that moment.

Announcing on ARM told the driver about a fault before it had happened.
Announcing on the first refusal was also wrong — a warning-phase miss is not the
failure, and it named *"stuck in 5th"* while the box actually jammed in 6th ten
seconds later. Symptom → HUD → real failure → radio.

### HUD

Uses the game's own message strip (the one that shows `RED FLAG`), mechanism
from `mods/red_flag/gr29.c`:

```
inner = *(u32*)0x7CD204     8 slots, stride 0x5C
text  = slot + 0x3C         ASCII, max 31 chars
timer = slot + 0x90         float; write 1.5 and re-arm each tick
```

Take a slot whose timer is `<= 0`. The line fades on its own, so nothing needs
cleaning up if the DLL is ejected mid-race. Messages are short and CAPS:
`GEARBOX FAILURE`, `GEARBOX FAILING`, `GEARBOX CRITICAL`, `NO DRIVE`.

Two dead ends, both checked first: `TimerDisplayRender` (0x4D9DD0) is
grid/classification only and never runs in-race, and calling
`UITextRenderWithScale` (0x4D7E40) directly from a `DrawDebugTextOverlay` hook
drew 3285 frames with nothing visible.

## Not done yet

- **11 of the 14 clips are reachable but not individually ear-checked** — only
  3 were heard. They are `WIP` in `progress.json` with a note; use the GUI's
  *Test line* button, then flip to DONE.
- **Automatic triggering.** Currently manual via the GUI. Lap/time/random
  triggers still to come.
- `mechanical_failures.py` is the superseded poll-based script, kept only as
  the record of the disproved approach and marked DEPRECATED in its own header.

---

## Engine failures (`engine_failure.dll`)

Full map: [`docs/ENGINE_FAILURE.md`](../../docs/ENGINE_FAILURE.md). Summary only
here.

The `.PLR` "Mechanical failure rate" timer array (`veh+0x3D70/74/78/7C`) is
**AI-only** — the player never enters `FuelConsumptionAndWear_004ed960`
(Interceptor: 105 calls, 21 distinct `this`, zero for slot 0). The player's own
engine failure is `EngineStressSimulation_0043ef20`, `this = 0x7C7428`.

| address | field | role |
|---|---|---|
| `0x7C91DC` | life, double | ticks down; at ≤0 sets ReasonCode 1, rolls an emitter, re-arms. **No commentary** — see below |
| `0x7C91D4` | stress, double | **the power loss** — multiplies torque by `(1 − v36²·stress)`, cap 0.6 |
| `0x7C920C` | bank 1 | WHITE SMOKE |
| `0x7C9214` | bank 2 | BLACK SMOKE |
| `0x7C921C` | bank 3 | FLAMES |
| `0x7C9081` | running byte | **the ignition flag** — 0 stops the engine, 1 restarts it |

Two things that are easy to get wrong:

- **Power and smoke are separate channels.** Stress drives the misfire; the
  banks only drive particles. A stage-one failure smokes while driving fine.
- **Healthy banks are `0.0`, not `1.0`.** A positive bank is used directly as an
  emission rate, so "repairing" to 1.0 leaves the car smoking permanently.

### Independent re-validation, 2026-07-31

Every claim above was re-tested from scratch against a live session, because
corpus root `docs/46-player-specific-mechanical-failures.md` had been written
with a different (wrong) offset base and contradicted this folder on four
points. Results, all driver-confirmed:

| Test | Result |
|---|---|
| `Interceptor` on `0x43EF20`, 5 s | 2001 calls, **one** distinct `ecx` = `0x7C7428`. `this` is the static object, confirmed independently of the 2026-07-30 run |
| `0x7C91DC = 1.5` | drained 1.5 → 0.08 in 5 s, then life 120.78 / stress 0.1415 / bank1 **−0.3611** / ReasonCode 0→1. Driver: white smoke, slower on straights |
| **`0x7C91D4 = 0.0`, banks untouched** | Driver: *"Smoke still there, going faster now"* — **the A/B that proves stress is power and banks are cosmetic.** Stress stayed latched at 0.0 for 10 s |
| `0x7C91D4 = 0.6` | life drain 0.635/s vs 0.28/s at stress 0.1415 vs 0.14/s idle — **stress feeds back into life drain ~1:1** |
| `Interceptor` on `0x41AC50` across the whole failure | **0 calls.** Driver: no radio line |

Three corrections this produced:

1. **`EngineStressSimulation` never calls `sub_41AC50`.** The earlier "it calls
   the emitter itself, so the radio line needs no unblocking" is DISPROVEN. A
   player engine-failure radio line needs a DLL emitter.
2. **The life drain is ~0.14–0.64/s, not the folded `(2.0+2.0)*0.5`.** Doc 46's
   "≈2–2.5/s" is wrong the same way.
3. **`flt_8AAE74` is a 256-entry uniform-random table in [0,1]** and
   `dbl_8AFEA0` is `SessionTime`, so the misfire index is
   `11.5·(rpm/maxRPM)·SessionTime` — flutter frequency scales with revs
   (~11.5 Hz near the limiter). Mean torque loss `0.345·stress`, peak
   `0.997·stress`.

Also fixed: **`tools/poke.py` could not write doubles at all** (`'f' not in fmt`
routed `'<d'` through `int()`), which made a real write look like a poke that did
nothing. Every field in this table is a double. Fixed at corpus root.

### Control

**GUI only (in-game hotkeys removed 2026-09-27).** The test panel
(`tools/mech_failure_test_gui.py`) drives the Engine tab over the shared block
`F1C_ENGFAIL_CTL`: stage-one / terminal / instant / seizure, cutout and
oil-leak toggles, white / black / fire emitters, repair, state dump, telemetry
HUD and a live life/stress/torque/rpm readout. Commands are edge-triggered
through the block; the DLL polls no keys.

HUD messages use the game's own message strip (`0x7CD204`), re-armed each tick.
Self-eject: `python tools/dll_unload_signal.py F1C_ENGFAIL_UNLOAD`.

### Build

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o engine_failure.dll engine_failure.c -lkernel32
```

### Cutout and oil leak (`C` / `O`, 2026-08-07)

Two mod-authored glitches, no vanilla equivalent, both GUI-tracked (Engine
tab, `Glitches` section):

- **Cutout (`C`)** — reuses the FACT ignition byte `ENG_RUNNING` (0x7C9081),
  but flicks it off for a short random burst (80-280ms) on a random interval
  (0.8-3s) instead of holding it off. Stutters, doesn't kill the engine.
- **Oil leak (`O`)** — a pressure gauge modelled entirely in the DLL (F1C has
  no live oil-pressure telemetry, only a durability/thermal bracket unrelated
  to any trigger). Drains 100→0 over 60s: blue smoke density rises the whole
  way (reuses bluesmoke.dll's proven `ParticleTrailEmitter` path, duplicated
  here so the DLL stays self-contained), `ENG_STRESS` also climbs past 50%
  pressure, real seizure at 0%. Fires `PWS_MECHANWARN0` (eid **130**, not
  131 - see `findings.jsonl` `pws-mechanwarn0-eid130-confirmed-not-131`) once
  at the 50% mark, isolation-tested and driver-confirmed correct.
  **2026-09-28:** the oil leak is now also an automatic engine type - the
  fixed-chance roll and the vanilla timer both pick it with weight 20 (mix
  30/30/10/10/20), armed through the same deterministic `arm_oil_leak()` -
  see [docs/HUD_AUDIT_AND_ELECTRONICS_2026-09-28.md](docs/HUD_AUDIT_AND_ELECTRONICS_2026-09-28.md).
- **Engine-death commentary** — `PWS_RETIRE04` (eid **119**, not 120 - see
  `findings.jsonl` `pws-retire04-eid119-confirmed-not-120`) fires once on any
  `ReasonCode==1` death (oil-leak seizure, `Num6` manual seizure, or the
  vanilla life system's own death) once the car has actually coasted to a
  stop (`retire_commentary_pump()`, ports the proven wait-for-stop state
  machine from `dll/retire_commentary.c` rather than firing on the
  `ReasonCode` transition alone - that naive approach is that file's own
  documented v3 failure, "announced a retirement at 88 km/h with a drivable
  car").

### Not done yet (engine)

- **Suspension is FIXED — see [§ Suspension failures](#suspension-failures-suspension_failuredll) above.** The 2026-07-30 removal note below is history: writing the per-wheel bend fields directly did nothing and crashed; the proper hunt found the control input is the game's own call `SteeringGeometryUpdate(0x7C7428, wheel, severity)`, now driven from `suspension_failure.dll` (live-proven 2026-08-08).
- **Blue smoke is cut content, and the driver genuinely does not exist (not
  just unwired) — checked 2026-08-07.** `ParticleCollisionEffect` (`0x4CE930`)
  is three copy-pasted hardcoded blocks keyed on `this[8]/[9]/[10]`, no loop,
  no id parameter — compiled for exactly WHITE/BLACK/FLAMES, no 4th slot to
  claim (confirmed no spare struct field exists either: `this+7676` is reused
  by the very next statement in the caller). Restoring it for real needs
  either a binary patch to that function plus new struct storage (high blast
  radius, not attempted), or bypass the vehicle's 3-slot system: add
  `Reaction=bluesmoke Tex=smokewhite.tga Color=0x80aaccff ...` to
  `terrain.ini` `[LOOSE]` (no blue texture ships — commonmaps.mas has only
  SMOKEWHITE/SMOKEBLACK/SMOKEMARBLES/SMOKEWHITELOW/FIRE, tint reused white via
  `Color=`), then call `ParticleTrailEmitter` (`0x4CBC50`) directly from the
  DLL's own tick with the resulting id==2 descriptor pointer, found by walking
  the global reaction table at `0x8ACEA0` (`SpecialFX_Init` owner, array base
  `*(0x8ACEA0+20)`, count `*(0x8ACEA0+16)`, stride 108, id at `entry+4`) —
  DERIVED, not live-verified; see `findings.jsonl`
  `bluesmoke-no-vanilla-driver-emitter-hardcoded-3slot` and
  `bluesmoke-reaction-table-global-and-direct-emit-path`. Oil temperature
  (`OptimumOilTemp`/`LifetimeOilTemp`) is a real but **entirely unconnected**
  system — nothing in code reads it to drive any emitter, so using it as a
  trigger would be a pure mod-authored choice, not a restoration.
- **Not registered in `build_pack.py`'s `FEATURES`** — it restores no commentary
  clips, so the clip-oriented pack schema does not fit it. Revisit if the pack
  ever ships non-clip features.
- `0x7C90C4` is **not** engine RPM despite the label used while building this;
  recorded OPEN in `findings.jsonl`.

## Related

- `findings.jsonl` — 20 entries from the 2026-07-30 gearbox session (incl. one
  RETRACTED and two CORRECTION rows), plus 15 from the engine/ignition session
- `docs/ENGINE_FAILURE.md` — the full engine map
- **corpus root** `docs/IGNITION_INVESTIGATION.md` — superseded; ignition is
  `0x7C9081` (a SOLVED header was added there pointing here)
- `docs/SUSPENSION_DAMAGE.md` (this mod) — carries a read-vs-write warning header
- `functions/PositionLapStateUpdate_00447e00.c`, `functions/sub_4EF930_004ef930.c`
- `functions/FuelConsumptionAndWear_004ed960.c` — the AI wear/failure system
- `functions/EngineStressSimulation_0043ef20.c` — the player equivalent

---

## Suspension failures (`suspension_failure.dll`)

Shipped 2026-08-08, live-proven end to end. Gives the **player** a staged
suspension failure by driving the game's **own** bend input — the exact call
the game makes on a wheel impact — instead of poking the bend fields (which
is DISPROVEN: did nothing, then crashed, Num7 removed 2026-07-30).

### The input the game itself writes

```
CollisionSparkParticleEffects_004ec1a0 line 515:
    if (v89 > this+3965)                                  /* impulse test       */
        if (v89 <= this+3966 || random < this+3967)
            if (this[3247] == 0)                          /* veh+0x32C8 role 0  */
                SteeringGeometryUpdate(&dbl_7C7428, wheel, v108);   /* PLAYER ONLY */
            else RepairServiceStateDispatcher(wheel);      /* full wheel damage  */
```

`SteeringGeometryUpdate_00442060` (the misnamed "LifetimeClampUpperBound"
owner — the ±10° clamp is the bend clamp, see `docs/SUSPENSION_DAMAGE.md`) is
the **only writer** of the player's per-wheel bend state:

| field | role |
|---|---|
| `0x7C7428 + 1124*w + 1105` | bent flag (BYTE1 of dword +276; +1104 is the contact flag) |
| `+1116` / `+1124` | bend-angle doubles, clamped to **exactly ±10°** (0.1745329251994329) |
| `+1132` | accumulator; `> 0.015` → `veh+0x4068 ReasonCode = 4` (Suspension), guarded by `if (!ReasonCode)` |
| `veh+0x4068` | ReasonCode 4 |

It is **player-only by construction** (role gate `this[3247] == 0`), which is
why AI suspension damage uses the separate `veh+0x3D7C` timer.

### How the DLL uses it

- **Detour** on `VehicleMainPhysicsUpdate_004441f0` (+0, 6-byte push-imm/ret
  because the prologue has no 5-byte whole-instruction boundary; 12 stolen
  bytes, resume at +0x0C). That is the player's per-frame physics tick (calls
  Gearbox_00443b70 and PositionLapStateUpdate_00447e00).
- While armed, the detour calls `SteeringGeometryUpdate(0x7C7428, wheel, sev)`
  at ~20 Hz — **on the game thread**, the same context the game uses one frame
  deeper. A worker-thread call is exactly the crash class of 2026-07-30.
- **Stages** = applications × severity; the game's own clamp and trip do the
  rest:

  | stage | HUD | applications × severity | resulting bend |
  |---|---|---|---|
  | 1 FAILING | `SUSP FAILING` | 2 × 0.30 | ~1-3° per corner |
  | 2 FAILURE | `SUSP FAILURE` | 3 × 0.60 | ~5-7° (retuned 2026-08-08; was 8 × 0.60 = straight to the clamp) |
  | 3 COLLAPSED | `SUSP COLLAPSED` | 30 × 1.00 | at the ±10° clamp |

- **Radio**: `PWS_MECHPROB16` "Damaged suspension - avoid kerbs" (eid **75**,
  score 73, cooldown 15, unblocked — not the TV twins 345/348) once per arm,
  confirmed by `EVENT_TS > 0` with the 20 s retry pump.
- **Repair**: zero angles/accumulators/flags, clear ReasonCode — restore
  proven clean live.

### Live proof (2026-08-08, qualifying, stage 1 on RL+RR)

| wheel | bend1 | bent flag | ReasonCode | radio |
|---|---|---|---|---|
| RL | **−0.0377 rad (−2.16°)** | 1 | 0→4 on application | eid 75 **PLAYED** |
| RR | **+0.0496 rad (+2.84°)** | 1 | (drifted back to 0 later — the game re-arms it; treat as trigger, not state) | |

Same configuration as the organic crash of 2026-07-29 (RL −1.927°, RR
+3.323°). Repair restored all fields to 0.

### Driver-confirmed handling effect (2026-08-08, test day)

The bend is **real geometry, not dead telemetry** — the open question is
closed. Driver reports:

- **Stage 1** (RL −1.36°, RR +2.85°): mild, drivable, car slightly unsettled.
- **Stage 2** (original 8 × 0.60): both rears slammed to the **±10° clamp**
  (RL −0.174533, RR +0.174533) — *"The car tries to turn left"*, *"took me
  off road, nearly impossible to drive"*. This proved the mechanism beyond
  doubt and sent the stage back for a retune.
- **Stage 2 retuned** (3 × 0.60): RL −6.57°, RR +5.80° — mid-range, strong
  pull but driveable. The escalation now reads FAILING → FAILURE →
  COLLAPSED instead of a cliff.

Each 0.60 application adds up to ~3° per axis in a random direction, so the
application count is the severity knob; the game's own ±10° clamp bounds it.

### Control

**GUI only (in-game hotkeys removed 2026-09-27).** The test panel
(`tools/mech_failure_test_gui.py`) drives the Suspension tab over the shared
block `F1C_SUSFAIL_CTL`: stage buttons, wheel toggles, per-corner bend/accum
live readout, REPAIR, dump, inject/unload. Wheel mask ownership: the DLL owns
it; a GUI selection is change-detected and adopted. The sandbox build
(canonical source going forward) also carries the dev **auto trigger**
(2026-09-28, ABI 2) — see
[docs/SUSPENSION_AUTO_TRIGGER_2026-09-28.md](docs/SUSPENSION_AUTO_TRIGGER_2026-09-28.md).
- Self-eject: `python tools/dll_unload_signal.py F1C_SUSFAIL_UNLOAD`.

### Build

```bash
C:\msys64\mingw32\bin\gcc.exe -shared -O2 -Wall -o suspension_failure.dll suspension_failure.c -lkernel32
```

### Traps already settled

- `VehicleContactForceAccumulation_004ebc60` is **event-driven** (contact
  only), not per-frame — the first detour there never fired on demand. The
  per-frame player site is `VehicleMainPhysicsUpdate_004441f0`.
- The bent flag is at **+1105**, not +1104 (that byte is the contact flag) —
  reading +1104 while the corner is bent reports 0.
- Do NOT write the bend fields directly (2026-07-30 crash). Call the function.

### Not done yet

- Stage 3 (terminal, all four corners at the clamp) driver-tested only
  implicitly (stage-2-at-clamp proved that severity); the front-wheel
  selector (wheel toggles in the panel) is built and the fix is in, but a
  front-only bend has not been driven yet.
- **Artifact hygiene (status 2026-08-14):** `suspension_failure_v5.c/.dll`
  (2026-08-14, includes the auto-reset fix) is the current build, and the
  canonical `suspension_failure.dll` here is STILL the pre-retune 13:21
  build. Do NOT rebuild over the canonical name while old copies are mapped
  (a double-injection leaves the file locked). After the next game restart:
  rebuild v5 over the canonical name, then retire the `_v2`/`_v3`/`_v4`
  artifacts. The canonical SOURCE going forward is the sandbox port
  `mods/sdk_sandbox/suspension_failure/` (buildable via
  `python tools/f1c_mod.py build mods/sdk_sandbox/suspension_failure`,
  auto-reset included) — promote work there, keep this tree as evidence.

## Related

- `findings.jsonl` — 20 entries from the 2026-07-30 gearbox session (incl. one
  RETRACTED and two CORRECTION rows), plus 15 from the engine/ignition session
- `docs/ENGINE_FAILURE.md` — the full engine map
- **corpus root** `docs/IGNITION_INVESTIGATION.md` — superseded; ignition is
  `0x7C9081` (a SOLVED header was added there pointing here)
- `docs/SUSPENSION_DAMAGE.md` (this mod) — carries a read-vs-write warning header
- `functions/PositionLapStateUpdate_00447e00.c`, `functions/sub_4EF930_004ef930.c`
- `functions/FuelConsumptionAndWear_004ed960.c` — the AI wear/failure system
- `functions/EngineStressSimulation_0043ef20.c` — the player equivalent

## Release packaging

- Manual panel buttons that write `ENG_LIFE` / `ENG_STRESS` (stage one, terminal, instant, seizure, overheat) are dev/test only and must be compiled out of the release build.


## Static audit, 2026-10-07

[Safety Car, Mechanical Failures and compatibility issue report](../AUDIT_SAFETY_CAR_MECHANICAL_FAILURES_2026-10-07.md).
Issue discovery only: public and candidate findings are distinguished; no mods, builds or test harnesses were executed and no implementation fixes were made. Pending native validation is listed in the report.

- [Mechanical Failures accumulated user-data review (2026-10-13)](docs/MF_USER_DATA_REVIEW_2026-10-13.md): 1,236 rows/86 installs; latest mild-exhaust retirements and false pull-off refusal persist, audio hotfix has encouraging but limited field evidence.
