**Pit Stop Failures 1.1.4 shipped 2026-10-11 (CO-OP fix: other PCs' pit failures now shown; client bot terminal stop marks the copy retired; no in-game test):** ZIP `75C73925...`, source `release_candidates/pitstop_failures-v1.1.4-rc.1/`. [Release record](../../lightloader/v2.0.0-hub-dev.1/release_mf_1.0.6_2026-10-11/README.md). Unshipped `pitstop_failures-v1.1.3-rc.1` (usage change, other session) must rebase on 1.1.4.

**2026-10-09 correction:** use Lightloader 2.2.8 to edit all Pit 1.1.1 subtype weights in Show advanced settings; existing INIs migrate on Save or upgrade. [Evidence](../../lightloader/v2.0.0-hub-dev.1/release_2.2.8_2026-10-09/README.md).

**2026-09-29 Lightloader metadata candidate 1.0.6-rc.1 ():** Matias approved option B. The specified 1.0.5-rc.2 DLL received only an embedded `F1C_MOD_META` resource; direct PE comparison proves `.text` and every pre-existing section unchanged. Metadata, INI comments and the restricted Lightloader ZIP pass offline checks. Exact-DLL in-game load, metadata-log and one basic pit-failure validation remain pending. [Candidate evidence and hashes](release_candidates/pitstop_failures-v1.0.6-rc.1/RELEASE_CANDIDATE.md).

[2026-09-28 anim probe trial r12 (owner-validated through r10; handoff prompt inside)](trials/failure-anim-probe-20260928/README.md) · [2026-09-28 failure-synced animation/audio: owner objective, current behaviour and probe plan (define stage, no code)](docs/FAILURE_ANIMATION_SYNC_DEFINITION_2026-09-28.md)

[2026-09-28 closeout: finished, next-session notes and declined ideas](docs/CLOSEOUT_1_0_4_2026-09-28.md)

**Published 2026-09-28: Pit Stop Failures 1.0.4.** It adds crew-synced wheel guns, a volume mix that follows the player's sound settings, and race-engineer pit calls, and applies the owner two-gate policy. The owner checked the exact DLL in game and the public bytes were verified. [Release record](../../../operations/release_management/pitstop_failures/RELEASE_1_0_4.md).

**2026-09-28 wheel-gun loudness candidate ():** the game has no Miles/DS3D 3D audio; it pans and attenuates 2D samples in software and honours the PLR sound sliders, while 1.0.3 plays the wheel guns through raw waveOut at full scale (about 14-22 dB above native effects; -3 dB when passing on the straight). The `live/` candidate follows the player's SoundFX/ratio sliders, the native centred-pan level, inverse-distance falloff and on-track masking. Build and live tests pass; not listened to in-game. Follow-up: wheel-gun bursts now follow the visible gun (crew-channel sync, clip sized to the gun's stay at the nut, measured fallback); tests pass. 2026-09-28 trial r3 installed in Documents V10: player-stop sync engaged and the owner judged it "looked perfect"; r4 adds the race engineer "Gear" (EID 44) on the lollipop turn, and the owner reported it "works perfectly"; AI/fault/passing checks are pending. See [trial record](trials/wheelgun-sync-port-20260928/README.md). **Blocker:** `live/` predates the 1.0.3 Lightloader host/compat checks and must be ported first. VPS: no new reports. [Candidate, findings, levels and restoration](verification/wheelgun-audio-2026-09-28/REPORT.md); [game 3D audio model](docs/GAME_3D_AUDIO_MODEL_2026-09-28.md).

[2026-09-24 running-game check](verification/runtime-status-2026-09-24/REPORT.md): current PID matches successful Pit 1.0.3 initialization/session logs. Memory-reader freshness unavailable; actual pit-failure outcome not observed.

[2026-09-25 wing-repair behavior inspection](docs/WING_REPAIR_FAILURE_BEHAVIOR_2026-09-25.md): wing/body repair is a valid service, but not tyre work; repair-only stops can receive a stall incident while the native rear-wing duration is left unchanged. Static-source evidence only; player/AI repair-stop gameplay validation remains pending.

[2026-09-25 penalty/repair service-classification plan](docs/SERVICE_CLASSIFICATION_PLAN_2026-09-25.md): specifies fail-closed handling for stop-go/no-work and repair-only visits, and job-specific fault eligibility for player and AI. Its scoped first implementation is recorded below; repair-specific failures remain out of scope.

[2026-09-25 repair-classification candidate](verification/repair-classification-2026-09-25/REPORT.md): no-work and repair-only stops now fail closed; focused tests and the SDK gate pass. Candidate-only—guarded player/AI stop-go and repair validation remains pending.

**Published 2026-09-24: Pit Stop Failures 1.0.3.** Official Lightloader host enforcement added; game code/layout compatibility and installer upgrade repairs retained. Automated host/layout/runtime/setup/SDK gates pass, all four test folders upgraded, Defender passes, real startup/initialization logged, and public downloaded bytes verified. Full-race human validation remains outstanding. [Release and restoration](../../../operations/release_management/pitstop_failures/RELEASE_1_0_3.md).

**2026-09-24 startup audit correction — publication blocked:** installed 1.0.2 rc.2 checks required game code/layout, but does not explicitly verify Lightloader loading. Broker discovery is not host proof. Earlier claims of retained host verification were incorrect. Owner requires both checks for mods; implement/test host verification and perform exact-build gameplay before release. [Audit and evidence](docs/STARTUP_CONTRACT_AUDIT_2026-09-24.md).

**2026-09-24 actual four-game installation test passed:** Pit Stop Failures 1.0.2 rc.2 installed successfully into all four games in the owner-selected `f1 challenge testing versions` directory. Payloads, preserved settings, backups, game EXEs and unrelated plugin DLLs verified. Gameplay remains pending; publication held. [Results and restoration](verification/install-user-folders-2026-09-24-1790258329022087300/REPORT.md).

**2026-09-24 publication held for plug-and-play acceptance:** new 1.0.2 rc.2 installer automatically handles the identified old trial, Lightloader 1.0.1 and ABI12 broker. Installation tests on copies of all four reported folders pass, including repeats; 18 installer regressions and Defender also pass. Real gameplay/co-mod validation remains pending. Public download still serves r4; locally registered 1.0.1 was never uploaded. [Findings, evidence and limits](release_candidates/pitstop_failures-v1.0.2-rc.2/REPORT.md).

**2026-09-24 compatibility fix completed in candidate 1.0.1 rc.4:** installer and runtime now check required code and memory layout instead of two EXE identities. 32 compatibility cases, 18 installer checks, runtime fixtures, full SDK gate and Defender passed. Exact-DLL gameplay/human validation remains pending; public r4 is unchanged. [Implementation, evidence and remaining gate](release_candidates/pitstop_failures-v1.0.1-rc.4/REPORT.md).

[2026-09-24: how shipped mods check compatibility](docs/SHIPPED_COMPATIBILITY_COMPARISON_2026-09-24.md) — RF/SC use required layout/code checks; ESC uses Lightloader host proof plus broker checks; Pit still has the stricter two-EXE allowlist.

**Published 2026-09-24, installer revision r4:**
[Download Pit Stop Failures](https://mods.palloset.com/pitstop_failures) now serves
the standalone setup EXE, following shipped Red Flag delivery. Friendly README
is embedded; the optional ZIP contains only the installer. Eighteen setup checks,
Defender, remote hashes and full public-download equality pass. Owner authorized
shipment with pending human/UI/gameplay and broader compatibility checks disclosed.
[Release, verification, limits and rollback](../../../operations/release_management/pitstop_failures/RELEASE_1_0_0_R4.md).

**2026-09-24 shipped-installer pattern corrected:** latest local candidate is
`release_candidates/pitstop_failures-v1.0.0-r8/pitstop_failures-setup-v1_0_0.exe`.
Standalone embedded-payload installer and Red Flag-derived setup window; optional
ZIP contains only the EXE. Friendly README installs with the mod. Eighteen native
installer checks and Defender scan pass; visual/human gameplay validation pending.
[Precedent, correction, tests and limits](release_candidates/pitstop_failures-v1.0.0-r8/REPORT.md).

**2026-09-24 player instructions update:** latest review ZIP is
`release_candidates/pitstop_failures-v1.0.0-r6.2/Pit Stop Failures 1.0.0.zip`,
with `Pit Stop Failures Installer.exe` and plain-language player instructions.
Installer and mod files are unchanged from r6.1; renamed setup/package checks
and Defender scan pass. Still unpublished; prior human-validation gate remains.
[Change record and verification](release_candidates/pitstop_failures-v1.0.0-r6.2/TASK_RECORD.md).

**2026-09-24 installation repair candidate (not published):** standard manual ZIP,
small .NET setup without embedded Python, and the preserved unprotected
r5 pit DLL. Supported EXEs now install under any filename. Eleven source tests,
19 actual setup checks and local Defender scans pass. Shared Lightloader remains
unchanged/protected. Unsupported layouts, other antivirus products and human
installation/gameplay validation remain open. Public r3 remains unchanged.
[Candidate, exact hashes, failures and release gate](release_candidates/pitstop_failures-v1.0.0-r6/RELEASE_CANDIDATE.md).

**Published 2026-09-23, protected revision r3:** Pit Stop Failures 1.0.0 is available at
[mods.palloset.com/pitstop_failures](https://mods.palloset.com/pitstop_failures).
The ZIP contains the protected Pit DLL and installer launcher, reviewed broker,
editable INI and three wheel-gun WAVs. Two executable hashes are supported;
the second was human-tested for the ESC garage Leave Circuit route. Other
executables need individual validation. Earlier immutable packages remain.
The current bytes are frozen under `production/mods/pitstop_failures/v1.0.0-r3`.
[Revision delivery and limits](../../../operations/release_management/pitstop_failures/RELEASE_1_0_0_R3.md).

**2026-09-23 guarded release-preparation race:** r36 queued XPITIN EID 50 at
148.6 m, attenuated distant AI wheel-gun bursts, and queued stall EID 184 on
Michael Schumacher's confirmed engine stop before a logged restart. Owner
listening feedback is pending. A brief r37 menu load does not establish its
gameplay. [Fresh live evidence and load restoration](verification/ship-live-2026-09-23/REPORT.md).

**Prior 2026-09-23 public-build candidate (installer rc.2):** r37 removes the local force-failure
control, packages a hash-checked installer with the three wheel-gun WAVs,
and keeps the six-setting INI. SDK, actual-client, installer and archive
checks pass. At that candidate checkpoint the exact public DLL had no in-game
validation; later owner checks and the accepted release exception are in the
release record. **Historical internal candidate.**
[Exact hashes, package, evidence and release gate](verification/ship-candidate-2026-09-23/REPORT.md).

**Prior 2026-09-23 guarded trial:** r36 fixes the lost AI stall voice when
the fault notice precedes the physical engine stop. A 20,000-stop natural
fixture confirms the default 7.50% gate and v3 live adapter path. The DLL is
staged in Documents V10; audible gameplay and a current live endpoint check
are pending. [Stall and probability evidence](verification/stall-probability-2026-09-23/REPORT.md).

**2026-09-23 AI audio correction:** owner testing of r31 exposed global
wheel-gun WAVs and missing sounds on a no-refuel AI stop. r33 `5A6A4AAC…`
adds spectated-car distance attenuation, schedules normal tyre sounds on clean
and rejected-fault stops, and restores the general slow-wheel delay when
fuel overlaps. Build, actual-client and full SDK checks pass. Exact r33 is
staged in the closed Documents V10 test game; audible gameplay and the
proposed plugin layout are pending.
[Evidence, limits and restoration](verification/ai-audio-localization-2026-09-23/REPORT.md);
[internal layout](release_candidates/ai-audio-r33-20260923/README.md).

**Prior 2026-09-23 release preparation:** internal r31 package candidate
`72BD3C64…` and six-value player INI passed build/SDK and package hash
checks. The 7.5% gate and conditional outcomes are audited. Live r31/INI use,
150 m audio timing, clean Lightloader installation, public test-API removal,
and owner release validation remain pending.
[Audit and restoration](verification/ship-preparation-2026-09-23/REPORT.md);
[candidate layout](release_candidates/ship-prep-r31-20260923/README.md).

**Prior 2026-09-23 150 m voice trial:** r28 `391B40F5…` moves the XPITIN pit-entry
request from 50 m to 150 m so it can finish before `LIMITER ON`. Actual-client
radius/rearm checks, 10 GUI tests and the full SDK gate pass; the exact DLL is
installed in the closed Documents V10 test game with r27 preserved. The new
timing and limiter separation await owner gameplay validation; this is not a
release artifact. [r28 evidence](verification/xpitin-150m-2026-09-23/REPORT.md).

**Prior 2026-09-23 live repeat-call trial:** exact r27 `F08AAEAF…` was loaded in
guarded race PID 2804 as endpoint `2804-35523328`. EID 50 was accepted once
on each of two pit calls in the same race, at 49.373 m and 49.730 m from
XPITIN. The owner heard the pit-entry line on both approaches. This verifies
repeat activation and audible playback in this guarded race; broader release
validation remains pending.
[Live record](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**Prior 2026-09-23 repeat-call verification:** r27 `F08AAEAF…` passed an actual-client
two-stop sequence in one race with the same player object: EID 50 was queued
on both approaches and only once per call. The new call now clears the prior
stop's GUI queue result before attempting the next voice. Full SDK gate passed;
exact r27 is installed with r24 preserved. Native playback on a second stop
still needs owner listening. [Evidence and restoration](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**Prior 2026-09-23 XPITIN voice correction:** owner testing of r22 found the pit call
active at 35.165 m from XPITIN, but the native entry latch remained false and
EID 50 was never attempted. r22 waited for that latch despite the requirement
to call out near XPITIN. Development candidate r24 `1AFC3859…` now arms the
one-shot callout inside a measured 50 m radius and retains the latch fallback.
After the earlier paused snapshot, the r22 log later recorded EID 50 accepted
by the native queue at 38.282 m; the owner still did not hear it. Queue
acceptance does not prove playback, so the audio delivery issue remains open.
Actual-client, 10 GUI tests and full SDK gate pass. With the game closed,
r24 was installed with matching candidate, installed and SDK ownership hashes;
the prior r22 pair is preserved. Audible gameplay validation remains pending.
[Live trace, repair and restoration](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**Prior 2026-09-23 player XPITIN lookup trial:** r21 proved live distance but
misreported a native active pit call after `CallToBox` cleared; the same gate
missed EID 50 on the owner's clean stop. Corrected r22 `E2544CA5…` uses the
game's called/lane states, passed actual-client, GUI and full SDK checks, and
was loaded again in guarded game PID 25948 with the updated panel. A new pit
call and audible XPITIN check remain pending. [Live trace, repair and restoration](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**2026-09-23 XPITIN/slow-wheel/audio repair candidate:** live r13 refused
XPITIN EID 50, skipped the selected corner's clear line and placed wheel ON
bursts after a short stop's release. r19 `EE5D70C1…` fixes the SDK voice
allowlist, queues the clear before release and fits the old script's gun pattern
to native tyre duration. Build, actual-client and full SDK checks pass; r19
is preserved and superseded by r22 in the running game. Owner audible/visual
validation is pending. [Live evidence and next gate](verification/xpitin-slow-wheel-audio-2026-09-23/REPORT.md).

**2026-09-23 crew-window diagnostic trial:** r13 `04E887F2…` reads bounded,
per-stop named crew animation ranges through a new SDK helper and shows them in
the test panel. The sound schedule stays on measured service offsets; v3
probabilities and recovery rolls are unchanged. Build, actual-client, full SDK
and panel tests pass. The DLL is staged with the game closed; live window/audio
validation remains pending. [Evidence, limits and restoration](verification/crew-windows-2026-09-23/REPORT.md).

**Current 2026-09-23 wheel review candidate:** the old mod's slow-wheel
presentation is restored as a general delay with one selected corner line.
Wheel-gun faults now sequence the corner problem and generic gun lines; the
wheel-gun WAV path is enabled for a guarded trial. Candidate `28BB51B6…`
passed build, actual-client and full SDK checks and is staged in the closed
game; gameplay audio/stability is pending.
[Old-mod comparison and restoration](verification/old-mod-parity-2026-09-23/REPORT.md).

**Prior 2026-09-23 voice trial:** `0887E76D…` added the XPITIN pit-call line,
wheel gun failure line, and fuel-clear line at the full requested target. It
was staged in the closed game after build and SDK checks; playback was not
tested before this wheel review. [Voice trial](verification/pit-voice-additions-2026-09-23/REPORT.md).

**2026-09-23 fuel gameplay and feedback correction (prior candidate):** the owner tested all
four physical fuel outcomes in the `3E8FF190…` live trial and reports that
each works. The owner also found `FUEL SERVICE RESUMED` arriving on the next
stop. Candidate `7FAE43BC…` binds that message to actual physical recovery
and discards stale stop feedback. It passed build and SDK checks and is included
in the current trial; cue timing needs a new owner gameplay test.
[Fuel trial and restoration](verification/fuel-effects-2026-09-23/REPORT.md).

**2026-09-23 player stall/audio correction candidate:** the first integrated
stall trial reached the garage/results path, but its straightforward stall
did not restart and the retirement line was inaudible. A unique development
build `36AB7F7E…` applies the native starter's correct idle-speed lift and
gives retirement audio a five-second lead. The build, actual-client regression,
and full SDK gate pass. The exact candidate is live in game PID 28180 and the
test GUI is open; owner gameplay validation is pending.
[Trial record](verification/player-stall-voice-2026-09-23/REPORT.md).

**2026-09-23 Pit Stop Failures results integration:** pit `F9865907…` and broker `430115BC…` installed/staged. The pit client requires the shared, tested garage-results service; no ESC-menu client is required. Actual-client/native-hook tests and full SDK pass. Pit-failure gameplay and next-process loading pending. [Integration, evidence and rollback](verification/broker-results-port-2026-09-23/REPORT.md). Earlier candidate summaries below are historical.

**Owner gameplay pass, 2026-09-23:** installed r5 broker `C1FA9203…`; owner reports the requested retirement/garage/Leave Circuit/results flow now works properly. Native completion corroborated in logs, including maximized monitor. This supersedes pending r5 statements below; broader modes and mechanical-failure path remain separate gates. [Pass record](verification/leave-icon-native-2026-09-23/REPORT.md).

**Current r5 trial installed (2026-09-23):** r4 resumed driving because native ESC cleared UI-active. Broker `C1FA9203…` retains the UI-active/frame context and race data through results; ACCEPT exits. Focused/native UI-setter regression and full SDK pass; one-click populated results gameplay pending. [Evidence and rollback](verification/leave-icon-native-2026-09-23/REPORT.md). Earlier summaries below are historical.

**Current r4 trial installed (2026-09-23):** r3 opened empty Race Results because exit cleanup preceded the native countdown. Broker `1D22A793…` retains the session on the results page until native ACCEPT exits. Build/focused/full SDK gate pass; populated results and ACCEPT gameplay pending. [Evidence and rollback](verification/leave-icon-native-2026-09-23/REPORT.md). Earlier candidate summaries below are historical.

**Current r3 trial installed (2026-09-23):** broker `D7E50F67…` maps the on-screen Leave Circuit icon to the native ESC exit after UI processing, covering normal and maximized monitors. Build/full SDK gate pass; gameplay pending. Earlier trial failures below remain historical. [Evidence and restoration](verification/leave-icon-native-2026-09-23/REPORT.md).

**Latest owner test, 2026-09-23: FAILED.** Broker 76B0F99A intercepts normal Leave Circuit clicks without finalizing; MONITOR MAXIMIZED bypasses the new route. Earlier pending/pass statements below are historical. Review of older mods found retained ESC-results fixes but no confirmed complete garage-icon pass. See the latest section of the [investigation record](verification/leave-icon-native-2026-09-23/REPORT.md).

# Pit Stop Failures — development workspace

**Current trial:** broker `76B0F99A…` hooks the actual on-screen **LEAVE CIRCUIT** icon and arms native race finalization; original OSC restored after the UI-only trial failed. Build/full SDK gate pass; gameplay pending. Test ESC **Retire to Pits** → garage → click Leave Circuit icon → populated **RACE RESULTS** → **ACCEPT** → main menu. [Current record](verification/leave-icon-native-2026-09-23/REPORT.md).

[2026-09-23 guarded garage/results UI trial installation](verification/garage-results-osc-install-2026-09-23/REPORT.md) — owner authorized installation while the game was closed. The exact garage-restoration pit DLL `E63F8A32…` and one-action OSC trial `92F43995…` are installed and hash verified; gameplay and results-grid validation are pending. [Development-only monitor results UI trial](trials/garage-results-osc-r1/TRIAL.md) — a copy of `F12K2.osc` changes only the MONITORPAGE leave action from MAIN MENU to POST RACE MONITOR. It is installed for the authorized guarded test. The route may lack populated results data and affects normal monitor exits, so gameplay validation and restoration are required before any narrower fix.

[2026-09-23 live rejection and garage restoration](verification/garage-results-live-rejection-2026-09-23/REPORT.md) — owner testing proved the installed no-flip stall candidate goes straight to Race Results; ESC Retire reaches the garage, but X still goes straight to the main menu. The broker armed X but logged no conversion or garage update trace. A development-only `E63F8A32…` build restores the bounded garage handoff; it is not installed and does not yet fix X/results. Native garage UI input routing requires investigation.

[2026-09-23 live trial load](verification/player-garage-x-live-load-2026-09-23/REPORT.md) — the staged development DLL `69C9386D…` is loaded into game PID 28576; the live endpoint responds. Owner gameplay validation is pending. This candidate does not establish an X-to-Race-Results fix.

[2026-09-23 guarded player-garage X installation](verification/player-garage-x-install-2026-09-23/REPORT.md) — development DLL `69C9386D…` is staged in `F1 Challenge V10` with a matching installed hash. The game was closed during staging. This is and still needs the owner's guarded full-weekend test; no X-to-Race-Results fix is claimed. The previous installed DLL and ownership metadata are preserved for restoration.

[2026-09-23 player garage X investigation](verification/player-garage-x-2026-09-23/REPORT.md) — owner reports X skips Race Results after a pit-failure retirement. The broker's X route is armed only by visible ESC-menu retirement, not the pit client's programmatic command; the menu route itself still needs live validation. The pit candidate now has a race-session preflight and causal PLAYER_OUT fixture; new development DLL `69C9386D…` passes build/SDK tests and is not installed. No X/results fix is claimed.

[Existing Resume / Retire to Pits / Exit menu located](verification/stop-menu-found-2026-09-22/REPORT.md) — `f1c_ida_decompiled/mods/sdk_sandbox/esc_pause_menu` contains the requested four-option menu. Its vanilla ESC `EXIT RACE` route has live evidence of landing on race results, but the current broker source injects ESC twice and must be corrected before reuse.

[Player-results candidate review: changes required before loading](verification/review-player-retire-results-2026-09-22/REVIEW.md) — add a SessionType-7 preflight, make the threaded PLAYER_OUT fixture causally observe 0→1→0, and narrow the unproven X/results claim. Candidate `F1B54436…` was reproduced build-only and was not loaded by the reviewer.

[Player stall fallback and retirement-audio dedup loaded](verification/player-retire-garage-2026-09-22/REPORT.md) — the garage handoff worked, but the first fallback build replayed retirement every three seconds. Candidate `B8E06DC0…` permanently closes restart and retirement submission after the first handoff; regression proves later timeout windows add no commands or voices. Exact candidate loaded into PID 20804 as endpoint `20804-106384609`; GUI reopened; one-shot audio gameplay validation pending.

[Current live isolation load](verification/live-timing-stall-2026-09-22/ISOLATION_LOAD.md) — PID 25368 runs the audio-disabled candidate `2C30CB6E…`; delayed callouts and player/AI stall mechanics remain active, custom wheel WAVs are deliberately silent for this crash-isolation cycle.

[Current: post-load crash investigation and quarantine](verification/live-timing-stall-2026-09-22/CRASH_2026-09-22.md) — restarted game is healthy and the candidate is not loaded; fault was in the game's 3D sound-source destructor, with no pit/SFX/stall event recorded after the final load. Causation remains unresolved pending an audio-disabled isolation run.

[Live presentation repair loaded for testing](verification/live-timing-stall-2026-09-22/REPORT.md) — original-mod wheel-animation timings restored, fault callouts deferred into the relevant operation, and the proven player ignition stop/restart path added. Exact loaded DLL hash `2FD5CAE2…`; human audible/gameplay validation pending.

[Live v3 gameplay session started on reviewed PID/hash](verification/live-v3-gameplay-2026-09-22/START.md) — owner testing pending.

[Owner in-game checklist and current GUI controls](docs/GAMEPLAY_CHECKLIST_V3_2026-09-22.md).

[Live v3 revision-2 review: L1/L2 closed, ready for guarded gameplay testing](verification/review-live-v3-r2-2026-09-22/REVIEW.md) — exact F655BBDF… candidate, not loaded by reviewer; earlier failed reviews below are historical.

[Live v3 review: fix token reuse and restore timing precision guard before loading](verification/review-live-v3-2026-09-22/REVIEW.md). Offline R1–R5 remain closed.

[Revision-2 independent review: R1–R5 closed](verification/review-two-stage-r2-2026-09-22/REVIEW.md) — offline candidate ready for live-adapter integration development; not yet v3 gameplay/deployment approval. Earlier failed review below is preserved history.

[Independent review of two-stage candidate: changes required](verification/review-two-stage-2026-09-22/REVIEW.md) — fuel consistency, session validation/replay, event JSON, fire model and v2 preservation. No live integration approved by this review.

[Player retirement results route corrected — built and fixture-verified, NOT
loaded](verification/player-retire-results-2026-09-22/REPORT.md): the garage
handoff no longer flips SessionType, so `sub_4A0250`'s race block creates the
native post-race monitor and the X advance follows the race-results route
instead of the garage/menu exit.

[Live adapter v3 candidate — built and fixture-verified, NOT loaded, awaiting
owner review](verification/live-v3-adapter-2026-09-22/REPORT.md): the existing
live client now runs the two-stage policy (parent + optional forced
resolution, severity-driven terminal retirement, reused x87 guard, native AI
mechanics and retirement narration; fire never advertised). Review repairs L1
(cancellation sequence) and L2 (timing x87 guard) applied; status exposes
policy version/config hash/seed/gate. DLL SHA-256 `F655BBDF…`; rollback is
`-DPIT_LIVE_POLICY_MODEL=2`.

[Latest: two-stage v3 extension, revision 2 after independent review](verification/two-stage-tuning-2026-09-22/REPORT.md) —
candidate **0.4.0-two-stage.2** (, no runtime contact).
Additive versioned extension with a generated session variant; accepted v2
policy/session byte-identical and their full suite still passes. Review repairs
R1–R5: [independent review](verification/review-two-stage-2026-09-22/REVIEW.md).
[API and configuration](docs/POLICY_V3_TWO_STAGE.md).
Owner direction: [two-stage handoff](docs/TWO_STAGE_TUNING_HANDOFF_2026-09-22.md) —
implemented at the offline policy/session/simulator/GUI-bridge level; live
runtime wiring, native effects and human gameplay validation remain pending.

[Proposed outcome chart and LLM tuning implementation plan](docs/TUNING_IMPLEMENTATION_PLAN_2026-09-21.md) — draft only, not enabled.

[Current probabilities, duration ranges, recovery rules and runtime limits](docs/CURRENT_PROBABILITIES_2026-09-21.md).

[Retirement announcement repair and original voice catalogue](verification/retirement-voice-2026-09-21/REPORT.md):
generic named AI retirement call now follows confirmed DNF/garage placement,
once, with a bounded 20-second radio wait. Owner confirmed the retirement announcement played correctly.

[AI mechanical failures implemented and tested](verification/ai-mechanics-2026-09-21/REPORT.md):
native engine stop/restart and terminal-wheel diagnosis → DNF/garage now wired.
Michael and Ralf retired into their garages; Sarrazin stopped/restarted.
Choose **Terminal stuck wheel / retirement (AI only)** for retirement;
cross-thread remains recoverable. Radio/visual acceptance and remaining limits
are recorded in the report. Earlier absence statements below are historical.

[Failure behavior review: what actually works](verification/failure-behavior-2026-09-21/REPORT.md).
Current stall and fuel faults are delay/presentation effects; no real engine
shutdown or per-car fuel suspension. AI retirement and fire are unimplemented.
Cross-thread is recoverable and distinct from unavailable terminal wheel jam.

[Repeated-test clock repair](verification/repeat-fix-2026-09-21/REPORT.md):
Michael's failure after Ralf exposed the game's 24-bit x87 mode. Policy maths
now uses scoped double precision and restores the game mode; rebuilt and
reloaded into the running game. See report for native retest results.

Current GUI repair and **native Ralf slow-wheel test**:
[connection, delay, return to track and evidence limits](verification/gui-repair-2026-09-21/REPORT.md).
The open panel uses select driver → select failure → OK. After a game restart,
use **Connect to game**, then enter the driving session. Owner confirmed Ralf's
call played correctly; the full player/AI matrix remains pending and
fire/retirement remain unavailable.

Owner direction: reuse the original mod's effects with today's policy/event engine.
[Reuse implementation, provenance and limits](verification/reuse-2026-09-21/REPORT.md).
[Original located and verified](verification/original-found-2026-09-21/REPORT.md).

Latest: [live client candidate and verification](verification/live-2026-09-21/REPORT.md).
Panel flow: **select driver → select failure → OK**.
[Engineer voice connected: mapping and listening checks](verification/voice-2026-09-21/REPORT.md).
[Live connection established with 24 drivers](verification/connect-2026-09-21/REPORT.md).
[Exact-failure selector checks and limits](verification/selected-failure-2026-09-21/REPORT.md).
Run `connect-live.ps1` with the guarded game running to load the client and open
the real-time panel. Call-in and recoverable delays are implemented; native
gameplay validation, sounds, HUD, fire and retirement remain pending.
[Earlier M1–M3 repairs](verification/mock-fixes-2026-09-21/REPORT.md) remain preserved.

Canonical product: `pitstop-modding`. Development ports: **0.1.0-port.1**
(legacy DLL source), **0.2.0-policy.3** (offline policy v2, post-re-review
repairs) and **0.4.0-two-stage.2** (versioned v3 two-stage extension as a
generated session variant; offline candidate under review). **NOT FOR
DISTRIBUTION.** SDK/broker runtime adaptation and in-game validation are
pending for all. See [STATUS.md](STATUS.md).

Development test panel: `launch-test-gui.ps1` defaults to the **live** endpoint.
Use `-Offline` explicitly for virtual player/AI pit-in and synthetic timing.
[Testing instructions](docs/GUI_TESTING_2026-09-21.md) and
[native adapter contract](docs/TEST_GUI_CONTRACT.md).

This mod combines pit-exit commentary with slow-corner, wheel-gun, cross-thread,
fuel-hose, stall and terminal wheel-jam failures, plus AI failure handling.
Those features are inherited legacy implementations, not newly validated behavior.

- [Independent mock review: M1–M3 code repairs required](verification/mock-review-2026-09-21/REVIEW.md): fresh supplied tests pass; independent clock, cleanup and stale-GUI counterexamples fail. Real-policy GUI bridge remains unproven.

- [Revision-4 review and offline mock handoff](verification/integration-review-r4-2026-09-21/REVIEW.md): move to executable offline evidence; G2 same-session cleanup and suppression timing require demonstrated corrections before runtime.

- [Owner's complete player/AI gameplay acceptance target](docs/GAMEPLAY_ACCEPTANCE_2026-09-21.md): alpha cues, fault text/audio, real consequences and confirmed AI retirement/garage behavior.
- [Revision-3 independent review](verification/integration-review-r3-2026-09-21/REVIEW.md): main G1/G3/G4 design issues closed; G2 cleanup corrections remain — addressed in revision 4.

## Phase G — runtime integration specification, revision 4 (resubmitted for review)

Revision 4 applies the G2 cleanup state machine (End preflight, lease-driven
cleanup, dead-clock rules, certified fuel bound), the G1 cursor edge rules, and
the owner's complete player/AI acceptance target (alpha cues, one-shot arming,
AI narration and retirement provider). Record:
[revision 4](verification/pit-integration-r4-2026-09-21/REVISION_R4.md).

[SDK/broker capability investigation and integration specification](docs/INTEGRATION_SPEC_2026-09-21.md):
supported capabilities vs legacy experiments vs missing functionality for
identity, lifecycle, commentary, fire effect, fuel suspension, pit hold and
retirement; exact proposed SDK additions; ownership/cancellation/cleanup/
concurrency rules; blockers and acceptance tests. The rare pit-fire extension
(conditional on a fuel-pump problem, with callout and HUD) is specified against
the same provider model, with a confirmed visible announcement milestone before
ignition. No runtime integration was implemented, no game file was modified,
and no live write was performed.

- [Integration specification (revision 4)](docs/INTEGRATION_SPEC_2026-09-21.md)
- [Revision 4 record](verification/pit-integration-r4-2026-09-21/REVISION_R4.md)
- [Revision 3 record](verification/pit-integration-r3-2026-09-21/REVISION_R3.md)
- [Revision 2 record](verification/pit-integration-r2-2026-09-21/REVISION_R2.md)
- [Phase G investigation record](verification/pit-integration-2026-09-21/REPORT.md)
- [Policy v2 API and configuration](docs/POLICY_V2.md)
- [Rare pit fire: fuel-problem integration addendum](docs/PIT_FIRE_ADDENDUM_2026-09-21.md)
- [Test GUI and adapter contract](docs/TEST_GUI_CONTRACT.md)
- [Implementation plan](docs/IMPLEMENTATION_PLAN_2026-09-21.md)

## Isolated mock and executable contract tests (offline, review-r4 handoff)

The revision-4 contracts are implemented as a deterministic offline mock:
compilable declarations (`mock/pit_mock_contract.h`), a provider with the two
mandated corrections (C1 invalid-clock cleanup obligation, C2 suppression
preserving remaining fuel work), and 184 executable checks covering every
transition-table row, timeline, cursor edge, GUI one-shot arming, cue gate and
the retirement gate. Runs on x86_64 and i686 with deterministic JSONL traces
and an independent trace checker. No game, SDK or runtime contact; accepted v2
policy code untouched.

- [Mock report and evidence](verification/mock-contract-2026-09-21/REPORT.md)
- [Mock contract header](mock/pit_mock_contract.h)
- [Trace checker](tools/mock_trace_check.py)
- [Run script](test-mock.ps1), [build script](build-mock.ps1)

## Policy v2 (offline A–F accepted after independent re-review)

[Independent policy.3 acceptance](verification/review-r3-2026-09-21/REVIEW.md): R7–R9 closed, fresh acceptance and independent cancellation/copy/saturation probes pass on both architectures. Runtime capability work and human validation remain pending.

`0.2.0-policy.3` is a pure C11 policy, strict v2 configuration and session
lifecycle with a simulator/replay harness, compiled acceptance tests on two
architectures (standard and bounded-limit builds), fixture-checked 200k
pressure/eligibility runs and a 1,000,000-stop statistical matrix. It repairs
the re-review findings R7–R9 (delivery purge on cancellation, copy-out plan
ownership, drainable saturated queue with reserved history capacity). It is
**not connected** to the legacy DLL or any game process, and it makes no
runtime claim.

- [Policy v2 repair report and reviewer package (R7-R9)](verification/policy-v2-r3-20260921/IMPLEMENTATION_REPORT.md)
- [Independent re-review that requested these repairs](verification/review-r2-2026-09-21/REVIEW.md)
- [First review and R1–R6 repairs](verification/review-2026-09-21/REVIEW.md)
- [Earlier reviewer package (0.2.0-policy.1)](verification/policy-v2-20260921/IMPLEMENTATION_REPORT.md)
- [Implementation plan](docs/IMPLEMENTATION_PLAN_2026-09-21.md)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File %USERPROFILE%\Desktop\F1\development\mods\pitstop_modding\build-policy.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File %USERPROFILE%\Desktop\F1\development\mods\pitstop_modding\test-policy.ps1
```

## Legacy development (0.1.0-port.1)

Run from any directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File %USERPROFILE%\Desktop\F1\development\mods\pitstop_modding\build.ps1
```

Requires 32-bit MinGW GCC on PATH; pass `-Compiler <path-to-gcc.exe>` to override.
Every build creates a fresh `trials/port-<timestamp>` directory with compiler log,
source hash, build metadata and DLL hash. It never installs or loads the DLL.
WinMM is linked explicitly for the polyphonic wheel-gun audio.

- `src/pit_commentary.c`: active development source. Audio paths resolve beneath
  the host executable's `Audio/Vehicle` directory, independent of current directory.
- `config/pit_failures.ini`: hash-verified snapshot of the existing game tuning.
  The inherited runtime reads this filename beside the executable at DLL load.
  Snapshot comments are historical instructions, not permission to delete/install.
- `dll/`: preserved legacy source and binaries; do not overwrite or distribute.
- `tools/legacy/pit_failure_gui.py`: imported original GUI, retained as protocol
  reference. It performs direct process writes and is not a supported runtime tool.
- `docs/`: preserved historical analysis and session evidence.
- [Port specification](verification/port-2026-09-21/SPECIFICATION.md)
- [Investigation and verification](verification/port-2026-09-21/REPORT.md)
- [Import provenance](verification/port-2026-09-21/import-manifest.json)
- [Historical README](docs/LEGACY_README_2026-07-29.md)

## Runtime boundary

The old GUI launcher now fails closed with guidance rather than using an obsolete
absolute path and requesting elevation. Current runtime work must use the SDK
front door `f1c_ida_decompiled/tools/f1c_mod.py` and reviewed broker capabilities.
The live candidate uses the canonical SDK loader, broker callbacks and existing
reviewed CallToBox/PitStopEndTime fields. No new SDK runtime capability was added.

Pending adaptation: fixed addresses and slot assumptions, game-thread commentary,
owned failure commands instead of the vehicle scratch-field queue, fuel-rate
ownership/restoration and concurrent cars, session lifecycle and safe unload.
Historical claims labelled FACT or shipping in July documents do not establish
current release approval. Keep the originals as evidence.

- [Probability review and proposed rare-event calibration](docs/PROBABILITY_REVIEW_2026-09-21.md)

- [Implementation plan for the next agent](docs/IMPLEMENTATION_PLAN_2026-09-21.md) — current tension preset, offline work package, acceptance tests and runtime review gates; supersedes the earlier probability proposal.

- [Independent review: changes required before A-F acceptance](verification/review-2026-09-21/REVIEW.md) — reproduced lifecycle/seed defects and a repair checklist.

- [Rare pit fire: fuel-problem integration addendum](docs/PIT_FIRE_ADDENDUM_2026-09-21.md) — existing fire lab, callout mapping, conditional probability and reviewed SDK integration requirements; follows the current policy repairs.

- [Player terminal garage repair](verification/player-retire-garage-2026-09-22/REPORT.md) — suppresses retirement-time exit cues and ports the original guarded retire-to-garage handoff into the v3 live adapter; build and contract tests pass, gameplay validation pending.

- [Independent re-review of policy.2: R7–R9 repairs required](verification/review-r2-2026-09-21/REVIEW.md) — fresh 302-check runs on both architectures pass; independent probes expose queued cancellation, plan-pointer lifetime and saturated-queue drain defects.
**2026-09-23 pit voice candidate staged:** `0887E76D…` adds the XPITIN pit-call
line, generic wheel gun failure line, and fuel-clear line at the original
successful fill target. Build, adapter tests and full SDK gate pass; the exact
DLL is staged in the closed game. Gameplay voice timing is pending.
[Voice additions and restoration](verification/pit-voice-additions-2026-09-23/REPORT.md).

- [Live release assessment, 2026-10-09: verified 1.1.0 and missing wheel-gun assets](docs/ASSESSMENT_LIVE_RELEASE_2026-10-09.md)

- [Pit Stop Failures 1.1.1: configurable 0-100% incidents, all subtypes, verification and release](../../../operations/release_management/pitstop_failures/RELEASE_1_1_1.md)
