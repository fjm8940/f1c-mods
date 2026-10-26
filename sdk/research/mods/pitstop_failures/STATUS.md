**2026-10-11 candidate 1.1.2-rc.1 ():** CO-OP sync - the PC that drives a car rolls the stop and sends the plan; other PCs commit it unchanged. Installed on owner PC; not live-tested. [Record](../coop_mode/docs/COOP_MODS_SYNC_2026-10-11.md).

**2026-10-08 candidate: Pit Stop Failures 1.0.8 (batch P).** The terminal player pit failure (incl. the wheel-jam DNF) now uses ONLY the broker game-thread native retire route (Lightloader >=2.2.3): arm the menu, send `RETIRE_PITS`, release on `CONTINUE`. The cross-thread `player_retire_worker` (SessionType flip and vehicle retire write set) is removed entirely; an older broker gets a refusal, never a cross-thread retirement. grep proof: source/live_client.cpp contains one SessionType read and zero retire/vehicle writes. `source/live_tests.cpp` asserts RETIRE_PITS/CONTINUE leave every vehicle retire field and PLAYER_OUT byte-unchanged. Build rc 0; live_tests PASS (60,000 plans). DLL `4B814D38...`. Owner live checklist: `release_candidates/pitstop_failures-v1.0.8-rc.1/LIVE_TERMINAL_PIT_CHECKLIST.md`. Not deployed; catalog/ZIP = batch R.

**2026-10-07 current public version: Pit Stop Failures 1.0.7** (requires Lightloader 2.2.3). Player retirement after a terminal pit failure uses the broker's game-thread garage route; no SessionType flip; menu arm released. Not live-tested. [Release](release_candidates/pitstop_failures-v1.0.7-rc.1/RELEASE.md). Earlier entries are historical.

**2026-10-04 package r3 (published):** the Lightloader 1.0.6 ZIP (catalog and public link since 2026-10-02) had no wheel-gun WAVs; r3 adds `Audio/Vehicle/wheelguns_1-3.wav` (same 1.0.6 DLL/INI) and Lightloader 2.2.1 installs them. Pit also refused to start (`unsupported F1 Challenge executable layout`) when the Driver AI Toolkit plug-in had patched AIMainUpdate first; fixed on the toolkit side (1.0.2). Owner in-game pass 2026-10-04 (sounds play; WIP mechanical_failures was off, its load-time 0x424E70 detour still trips this check). [Record](../../lightloader/v2.0.0-hub-dev.1/release_2.2.1_2026-10-04/MANIFEST.md).

**2026-09-29:** Pit Stop Failures 1.0.6-rc.1 is a Lightloader 2.0 metadata candidate, . Under Matias's option B decision, `F1C_MOD_META` was added with `UpdateResource` to the specified 1.0.5-rc.2 DLL.text` and every prior PE section are unchanged. Offline metadata, B2 INI and H1 package checks pass. Exact-DLL in-game load, metadata-log and basic-function validation are pending. [Evidence](release_candidates/pitstop_failures-v1.0.6-rc.1/RELEASE_CANDIDATE.md).

**Published 2026-09-24: Pit Stop Failures 1.0.3.** Official Lightloader host enforcement added; game code/layout compatibility and installer upgrade repairs retained. Automated host/layout/runtime/setup/SDK gates pass, all four test folders upgraded, Defender passes, real startup/initialization logged, and public downloaded bytes verified. Full-race human validation remains outstanding. [Release and restoration](../../../operations/release_management/pitstop_failures/RELEASE_1_0_3.md).

**2026-09-24 startup audit correction — publication blocked:** installed 1.0.2 rc.2 checks required game code/layout, but does not explicitly verify Lightloader loading. Broker discovery is not host proof. Earlier claims of retained host verification were incorrect. Owner requires both checks for mods; implement/test host verification and perform exact-build gameplay before release. [Audit and evidence](docs/STARTUP_CONTRACT_AUDIT_2026-09-24.md).

**2026-09-24 actual four-game installation test passed:** Pit Stop Failures 1.0.2 rc.2 installed successfully into all four games in the owner-selected `f1 challenge testing versions` directory. Payloads, preserved settings, backups, game EXEs and unrelated plugin DLLs verified. Gameplay remains pending; publication held. [Results and restoration](verification/install-user-folders-2026-09-24-1790258329022087300/REPORT.md).

**2026-09-24 publication held for plug-and-play acceptance:** new 1.0.2 rc.2 installer automatically handles the identified old trial, Lightloader 1.0.1 and ABI12 broker. Installation tests on copies of all four reported folders pass, including repeats; 18 installer regressions and Defender also pass. Real gameplay/co-mod validation remains pending. Public download still serves r4; locally registered 1.0.1 was never uploaded. [Findings, evidence and limits](release_candidates/pitstop_failures-v1.0.2-rc.2/REPORT.md).

**2026-09-24 compatibility fix completed in candidate 1.0.1 rc.4:** installer and runtime now check required code and memory layout instead of two EXE identities. 32 compatibility cases, 18 installer checks, runtime fixtures, full SDK gate and Defender passed. Exact-DLL gameplay/human validation remains pending; public r4 is unchanged. [Implementation, evidence and remaining gate](release_candidates/pitstop_failures-v1.0.1-rc.4/REPORT.md).

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

**Current published 1.0.0, protected revision r3 (2026-09-23):** ZIP SHA-256
`CC321BFE233D3719D88C5AC7D8721A6896EFE3F8464F62F61FF6D0EE84E5B37B`
is frozen at `production/mods/pitstop_failures/v1.0.0-r3/` and delivered through
the stable link. Pit DLL and native installer launcher, new broker,
and two reviewed executable hashes are included. Owner confirmed the ESC
garage Leave Circuit route on the second build. Exact protected full pit-failure
gameplay remains untested in that build; other executable layouts remain open.
[Current delivery and restoration](../../../operations/release_management/pitstop_failures/RELEASE_1_0_0_R3.md).

**Prior published 1.0.0, docs revision r2 (2026-09-23):** exact r37 DLL
`32E1BAD51582CB6F75092AAA36A1BA4CD0473A1897F05F2FE3B3A4FE551277B6`
is frozen in the owner-approved current ZIP
`62E636101D4C3B697B434B87B0D9F5D9E4FD402AA4FB3E108FBD8EEA1C826E00`.
Only the README and install note changed from the first immutable release.
HTTPS download, bug-report selector, update catalog and origin request logging
are verified. A difficult AI stall did not restart in the owner test; this
remains in the internal [first release record](../../../operations/release_management/pitstop_failures/RELEASE_1_0_0.md).
[Current delivery and restoration](../../../operations/release_management/pitstop_failures/RELEASE_1_0_0_R2.md).

**Prior live r36 check (2026-09-23):** a guarded advancing race queued
XPITIN EID 50 at 148.6 m, spatial AI wheel-gun bursts at reduced gain, and
stall EID 184 for Schumacher on confirmed stop followed by restart. Owner
audibility feedback is pending; this is not a public r37 gameplay pass.
[Live record](verification/ship-live-2026-09-23/REPORT.md).

**Prior public-build candidate (2026-09-23, installer rc.2):** r37
`32E1BAD51582CB6F75092AAA36A1BA4CD0473A1897F05F2FE3B3A4FE551277B6`
has a verified internal installer and archive with wheel-gun audio. Its
in-game auto-load and gameplay remain untested; r36's Button cue and r33 AI
audio still need owner listening. Stage remains implement/prototype; no public
promotion. [Candidate report](verification/ship-candidate-2026-09-23/REPORT.md).

**Prior r36 guarded trial (2026-09-23):** Button's difficult-restart log
showed a policy notice before the actual engine stop and no AI stall voice.
r36 queues that voice on confirmed stop, retries a busy native queue, and
deduplicates late notices. The default 7.50% natural gate produced 1,453
incidents in 20,000 fully eligible fixture stops; forced GUI choices bypass
that gate. Actual-client and full SDK checks pass. Exact r36 `0BCD9A5E…` is
staged in the closed Documents V10 game; live loading, audible Button cue,
and natural gameplay sampling remain pending. No production promotion.
[Evidence and restoration](verification/stall-probability-2026-09-23/REPORT.md).

**Prior guarded AI audio trial (2026-09-23):** r33 `5A6A4AAC…` fixes
full-volume remote wheel-gun WAV playback and missed sounds on clean AI tyre
stops or ineligible forced-fuel stops. It also ensures a slow-wheel fault adds
its general delay despite overlapping fuel work. Build, actual-client and
full SDK checks pass; r33 is preserved and superseded by r36. Owner listening
and short-stop gameplay remain pending. The prior r31 package is superseded.
[Trial evidence](verification/ai-audio-localization-2026-09-23/REPORT.md).

**Prior release-preparation candidate (2026-09-23):** r31 `72BD3C64…`
adds `live/PitStopFailures.ini` with six player settings and relative failure
weights. Build, actual-client, full SDK, and candidate manifest checks pass.
Exact DLL/INI are staged in the closed Documents V10 test game; the proposed
Lightloader package remains untested. Human r31
gameplay, 150 m voice timing, public test-API removal, and clean installation
remain release gates. [Probability/package audit](verification/ship-preparation-2026-09-23/REPORT.md).

**Prior 2026-09-23 trial:** r28 `391B40F5…` sets the active-call XPITIN voice
radius to 150 m, keeping the game-latch fallback and per-call rearm. Canonical
build/actual-client fixture, 10 panel tests, and full SDK gate pass. Exact r28
is installed in the closed Documents V10 test game; the updated panel is open.
The owner still needs to judge 150 m audibility and its spacing from `LIMITER
ON` in a guarded race. No release promotion. [r28 evidence](verification/xpitin-150m-2026-09-23/REPORT.md).

**Prior 2026-09-23 live voice trial:** r27 `F08AAEAF…` was loaded in guarded
game PID 2804; endpoint `2804-35523328` responded. In one advancing race,
the native queue accepted EID 50 once on each of two separate pit approaches
(simulation times 119.243768 and 254.702780), and the owner heard the line
on both. The first stop returned to on-track pit status 0 with no active call
before the second call. Repeat activation is human-validated for this race;
broader release validation remains pending.
[Live evidence](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**Prior 2026-09-23 voice trial:** r27 `F08AAEAF…` was installed with the game
closed; candidate, installed DLL and SDK ownership hashes match. The same
player/object actual-client fixture queued EID 50 on two separate pit calls
within one race epoch, once per call, and cleared stale queue status at the
second approach. Canonical build and full SDK gate pass. Native audible
playback on a second stop is not yet proven; the last owner test of r22 was
inaudible despite queue acceptance. The previous r24 pair is preserved.
[Live evidence and restoration](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**Prior 2026-09-23 voice trial:** r24 `1AFC3859…` was built, verified and
installed with the game closed; no running process was confirmed to load it. The owner did not hear
XPITIN on an active call. Read-only game/endpoint evidence at 35.165 m showed
pit status 1, latch false, no EID 50 attempt, and pause flag 1; the sampled
car did not advance through the latch during observation. r24 uses a measured
50 m radius with latch fallback and once-per-call arming. Actual-client and
10 GUI tests pass; full SDK gate passed after the runtime change. Owner audio
test remains pending. The installed r22 DLL and SDK ownership record were
preserved before replacement.
Later r22 log evidence shows the latch triggered and EID 50 was queued at
38.282 m, despite the owner's inaudible result. The 50 m correction fixes the
late proximity trigger; it has not proven native audio delivery. A subsequent
game PID 28124 exited after an r24 load request without an r24 endpoint, so
r24 has no confirmed live-load or gameplay pass.
[Live evidence and restoration](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**Prior 2026-09-23 lookup trial:** r22 `E2544CA5…` was installed and last
confirmed live in game PID 25948, with matching SDK ownership hash and the
updated GUI open.
The owner exposed r21's false inactive-call label: native pit status was 1
(called) although the `CallToBox` input was already zero, and r21 missed EID
50 on that clean stop. r22 includes game called/lane states in the callout gate;
actual-client, ten GUI tests, GUI smoke and full SDK gate pass. New-pit-call
gameplay remains pending. [Live evidence and restoration](verification/xpitin-live-lookup-2026-09-23/REPORT.md).

**Prior 2026-09-23 audio repair candidate:** r19 `EE5D70C1…` passed the
actual-client build and full SDK gate and was loaded in prior game PID 17060;
the prior r13 is preserved for restoration. Live r13
refused XPITIN EID 50, omitted slow-wheel clear and scheduled ON gun bursts
after this short stop. r19 needs full-weekend audible/visual validation.
[Failure trace and restoration](verification/xpitin-slow-wheel-audio-2026-09-23/REPORT.md).

**Prior 2026-09-23 crew-window candidate:** r13 `04E887F2…` is installed
in the closed game for a guarded full-weekend test. The player panel can follow
named crew ranges at a committed tyre stop; raw channel/elapsed comparisons
are diagnostic and do not retime the measured wheel audio. The owner has not
yet tested this DLL. v3 probability and recovery selection remain fixed at
commit. [Verification and restoration](verification/crew-windows-2026-09-23/REPORT.md).

**Current 2026-09-23 wheel review candidate:** `28BB51B6…` restores the old
slow-wheel and wheel-gun presentation differences: general slow delay with
one corner line, corner-then-generic gun narration, and enabled WAV trial.
Build, actual-client and full SDK checks pass. The exact DLL is staged in the
closed game; owner audio/stability tests remain pending.
[Comparison and evidence](verification/old-mod-parity-2026-09-23/REPORT.md).

**Prior 2026-09-23 voice trial:** exact `0887E76D…` DLL was staged in the
closed game. XPITIN, wheel gun and full-fuel callouts passed build, actual-client
and SDK checks; gameplay clip/timing validation remains. [Trial evidence](verification/pit-voice-additions-2026-09-23/REPORT.md).

**2026-09-23 fuel gameplay and cue repair (prior candidate):** owner-confirmed all four fuel
outcomes work in the `3E8FF190…` trial. A recovery cue leaked into the next
stop. Candidate `7FAE43BC…` sends that cue on physical recovery once and
clears stop-bound feedback; build, actual-client and full SDK gates pass.
Fresh gameplay is pending. `7FAE43BC…` is included in the current trial. Audible stall
retirement was confirmed earlier; successful engine restart remains unproven.
[Evidence and next gate](verification/fuel-effects-2026-09-23/REPORT.md).

**2026-09-23 player stall/audio correction candidate:** the owner confirmed the
garage/results route, but reported that a straightforward stall failed to
restart and the retirement voice was inaudible. Development DLL `36AB7F7E…`
passes build, actual-client checks, and the full SDK gate. Fresh gameplay is
pending; the exact candidate is live in game PID 28180 with the test GUI open.
[Evidence and next gate](verification/player-stall-voice-2026-09-23/REPORT.md).

**2026-09-23 Pit Stop Failures results integration:** pit `F9865907…` and broker `430115BC…` installed/staged. The pit client requires the shared, tested garage-results service; no ESC-menu client is required. Actual-client/native-hook tests and full SDK pass. Pit-failure gameplay and next-process loading pending. [Integration, evidence and rollback](verification/broker-results-port-2026-09-23/REPORT.md). Earlier candidate summaries below are historical.

**Owner gameplay pass, 2026-09-23:** installed r5 broker `C1FA9203…`; owner reports the requested retirement/garage/Leave Circuit/results flow now works properly. Native completion corroborated in logs, including maximized monitor. This supersedes pending r5 statements below; broader modes and mechanical-failure path remain separate gates. [Pass record](verification/leave-icon-native-2026-09-23/REPORT.md).

**Current r5 trial installed (2026-09-23):** r4 resumed driving because native ESC cleared UI-active. Broker `C1FA9203…` retains the UI-active/frame context and race data through results; ACCEPT exits. Focused/native UI-setter regression and full SDK pass; one-click populated results gameplay pending. [Evidence and rollback](verification/leave-icon-native-2026-09-23/REPORT.md). Earlier summaries below are historical.

**Current r4 trial installed (2026-09-23):** r3 opened empty Race Results because exit cleanup preceded the native countdown. Broker `1D22A793…` retains the session on the results page until native ACCEPT exits. Build/focused/full SDK gate pass; populated results and ACCEPT gameplay pending. [Evidence and rollback](verification/leave-icon-native-2026-09-23/REPORT.md). Earlier candidate summaries below are historical.

**Current r3 trial installed (2026-09-23):** broker `D7E50F67…` maps the on-screen Leave Circuit icon to the native ESC exit after UI processing, covering normal and maximized monitors. Build/full SDK gate pass; gameplay pending. Earlier trial failures below remain historical. [Evidence and restoration](verification/leave-icon-native-2026-09-23/REPORT.md).

**Latest owner test, 2026-09-23: FAILED.** Broker 76B0F99A intercepts normal Leave Circuit clicks without finalizing; MONITOR MAXIMIZED bypasses the new route. Earlier pending/pass statements below are historical. Review of older mods found retained ESC-results fixes but no confirmed complete garage-icon pass. See the latest section of the [investigation record](verification/leave-icon-native-2026-09-23/REPORT.md).

# Pit Stop Failures status — 2026-09-22

**2026-09-23 icon repair installed; gameplay pending.** Owner rejected the
OSC-only trial. The X means the on-screen Leave Circuit button. Original OSC
restored; keyboard-X remap withdrawn. Broker `76B0F99A…` hooks the GUI release
and schedules native race finalization through the results-grid countdown.
Native ACCEPT remains active. Build, machine-code fixture and full SDK gate
pass; complete ESC Retire → garage → icon → results → ACCEPT gameplay pending.
[Current evidence/restoration](verification/leave-icon-native-2026-09-23/REPORT.md).

**2026-09-23 owner-corrected ESC trial scope.** Game PID 6164 is running
after the OSC installation; current ESC-menu startup log shows ABI 15 broker
discovery and the pause menu armed. The active test is ESC **Retire to Pits**
→ garage → X/**LEAVE CIRCUIT** → **RACE RESULTS** → **ACCEPT** → main menu.
The pit-failure DLL need not be loaded for this test. Gameplay outcome is
pending. [Exact steps and evidence boundary](verification/garage-results-osc-install-2026-09-23/REPORT.md).

**2026-09-23 guarded UI trial installed; gameplay pending.** The owner closed
the game and authorized the diagnostic combination. Active `F12K2.osc`
`92F43995…` sends the shared MONITORPAGE leave action to POST RACE MONITOR;
active pit DLL `E63F8A32…` restores the bounded garage handoff. Both hashes
were read back and the previous OSC, DLL and SDK metadata were preserved.
The staged DLL still needs to be loaded into the next game process. Whether X
simulates the remaining race and opens populated results is unverified; the
global monitor exit change is unsuitable for distribution. Owner gameplay
validation and restoration remain the next gates.
[Installation and restoration](verification/garage-results-osc-install-2026-09-23/REPORT.md).

**2026-09-23 UI trial preparation (historical).** Native `F12K2.osc` directs
MONITORPAGE leave circuit to MAIN MENU; a one-action development copy points
it at POST RACE MONITOR. This may show an empty result grid and affects other
sessions, so it is a bounded diagnostic, not a release fix. The game still
runs the rejected `69C9386D…` pit DLL; `E63F8A32…` garage-restoration DLL and
the UI trial are development-only. [Trial specification](trials/garage-results-osc-r1/TRIAL.md).

**2026-09-23 owner rejection.** Installed `69C9386D…` engine-stall retirement
goes straight to Race Results without the garage. Visible ESC-menu Retire to
Pits reaches the garage, but X still goes straight to the main menu; broker
logged an X arm but no conversion. Bounded 7→5 garage handoff restored in
development build `E63F8A32…` (fixtures pass, not installed). The X/results
route remains unsolved and needs native UI-path evidence before another live
remap. [Evidence and next gate](verification/garage-results-live-rejection-2026-09-23/REPORT.md).

**2026-09-23 live load.** Exact installed pit DLL `69C9386D…` was loaded into
game PID 28576 through the canonical elevated SDK loader. The live endpoint
responds; retirement and X/results have not yet been tested. No fix claim.
[Load evidence and owner test](verification/player-garage-x-live-load-2026-09-23/REPORT.md).

**2026-09-23 guarded trial staged.** At the owner's request, development DLL
`69C9386D…` is installed in the closed `F1 Challenge V10` game tree with an
exact hash match; the prior `190BABD9…` DLL and SDK ownership metadata are
preserved. Staging alone does not load the DLL into a future game process.
The player-garage X → Race Results behavior remains unverified and was not
fixed in this candidate. [Installation, restoration and test scope](verification/player-garage-x-install-2026-09-23/REPORT.md).

**2026-09-23 player garage X follow-up.** Owner reports X leaves a pit-failure
garage directly for the main menu, skipping Race Results. The broker arms its
one-shot X route only for retirement selected from the visible ESC menu; the
pit client's programmatic retirement does not arm it. The ESC route itself is
still under guarded live validation. The previous no-SessionType-flip results
claim is unproven and conflicts with an ESC-menu live observation that a
no-flip retirement went straight to results rather than the garage. Review
R1/R2 are now repaired in a development-only pit candidate (`69C9386D…`):
race-session preflight and causally observed PLAYER_OUT consumption. Build and
SDK tests pass; the installed pit DLL remains `190BABD9…`. No X/results fix
has been installed or verified. [Investigation and next gate](verification/player-garage-x-2026-09-23/REPORT.md).

**Player retirement results route (built, fixture-verified, NOT loaded).** The
garage handoff no longer flips SessionType to qualifying; the native race block
in `sub_4A0250` therefore creates the post-race monitor and its advance/results
route, so pressing X from the garage spectator view follows the race-results
path instead of exiting straight to the main menu. The write set, PLAYER_OUT
wait, restart-flag guard, EID117 and dedup are unchanged, and the worker is now
covered by actual-client fixtures (no flip, no abandon transition, consumed
gate, retired classification retained, no replay). Canonical build/tests pass;
no game contact. Awaiting guarded live validation.
[Evidence](verification/player-retire-results-2026-09-22/REPORT.md).

**Live adapter v3 candidate (built, fixture-verified, NOT loaded; review
repairs applied).** The live client now runs the two-stage policy through
`pf3_session_*` with the embedded canonical v3 preset: forced parent bypasses
only the gate and parent draw, optional forced resolution selects the leaf,
terminal retirement is driven by the severity/terminal flag, recovery stages
follow the selected resolution, and the x87 guard, native AI engine
stop/restart and owner-confirmed retirement narration are reused. Live review
L1 (monotonic service sequence across cancellation) and L2 (guarded timing
application for both models) are repaired with regressions; status exposes
policy version/config hash/seed/gate. 13 forced leaves, natural fuel/fire
contract, cancellation, repeated stops, x87 and adapter-flow fixtures pass;
the accepted v2 path stays compiled for rollback. No game process was
contacted. Awaiting owner review before loading.
[Evidence](verification/live-v3-adapter-2026-09-22/REPORT.md).

**Two-stage v3 extension revision 2 (0.4.0-two-stage.2, offline candidate).**
Independent review repairs applied: one authoritative fuel subtype selection,
session forcing validated against the frozen authoritative eligibility after
identity resolution, valid JSONL rows with identity/seed correlation,
fire capability as a hypothetical model that never mutates ordinary timing,
and accepted v2 policy/session restored byte-identical with the v3 session as
a hash-guarded generated variant. 2,892,840+ checks pass on both architectures
(including bounded-limit fixtures), 3.1M JSONL rows parse, bridge/GUI tests and
the legacy standalone build pass. Live runtime wiring, native effects and human
validation pending.
[Report](verification/two-stage-tuning-2026-09-22/REPORT.md),
[review](verification/review-two-stage-2026-09-22/REVIEW.md),
[API](docs/POLICY_V3_TWO_STAGE.md).

**Two-stage v3 extension revision 1 (0.4.0-two-stage.1, offline).** Superseded
by revision 2 after the independent review above; preserved at
`workspace_organization_audit/PIT_TWO_STAGE_REPAIR_BEFORE_2026-09-22/`.

# Pit Stop Failures status — 2026-09-21

Ralf's missing retirement announcement traced to an absent enqueue, now repaired
using existing AIRETIRE02 / event237 after confirmed garage placement.
Owner confirmed the retirement announcement played correctly. [Catalogue, verification and listening result](verification/retirement-voice-2026-09-21/REPORT.md).

AI mechanics trial loaded: native engine stop/restart and terminal pit retirement.
Michael and Ralf DNF/garage observed; Sarrazin engine zero then restart/return
to track. Owner heard stall/restart but missed Michael amid busy radio traffic;
second separated pair completed: Ralf DNF/garage and Alonso native stop/restart,
+4.905273 s then On track. Updated GUI relaunched as PID 15328.
[Evidence](verification/ai-mechanics-2026-09-21/REPORT.md).
Fire, fuel-flow suspension and player retirement remain absent. Earlier AI
absence statements below are superseded; no production promotion.

Owner gameplay review: Michael's cross-thread recovered after +26.257080 s;
Sarrazin's stall added +3.832520 s but never shut down the engine. Source confirms
stall/fuel presentation overstates the implemented mechanics. AI retirement
and actual engine shutdown/restart remain absent.
[Full failure behavior review](verification/failure-behavior-2026-09-21/REPORT.md).

Repeated-test repair: native 24-bit x87 precision reproduced a false shared-clock
rejection. Scoped policy double precision fixes the reproduced failure, with
game mode restored. Loaded replacement into PID 25252 without race restart;
[Michael retest and regression evidence](verification/repeat-fix-2026-09-21/REPORT.md).
Michael's native retest passed: +2.170044 seconds, his subject ID 10 queued with
voice 183, car back On track, adapter still active. Audible playback unconfirmed.

Current: GUI reconnection repaired, rebuilt client loaded into PID 25252.
Native Ralf slow-corner test consumed the request, queued voice 183 with his
subject ID, applied a 2.745842-second target extension and returned him to track.
Owner confirmed hearing Ralf's call correctly. Other categories/player cues remain pending. No release
or stage change. [Current evidence](verification/gui-repair-2026-09-21/REPORT.md).
The PID 12800 entries below are historical observations.

Original-effect reuse: original polyphonic wheel-gun WAV engine and corner HUD
labels are connected to today's fault/retry/recovered commands. New build loaded
into PID 12800; all three WAVs loaded. Native listening/HUD validation pending.
Probability/session originals unchanged; old RNG/escalation loops not running.
Fuel-flow ownership, fire and retirement remain unfinished.
[Evidence](verification/reuse-2026-09-21/REPORT.md).

Engineer voice update: native queue now receives player pit lane/box/release
cues, fault and wheel recovery calls, plus AI fault calls with the affected
driver's native ID. Built and loaded into PID 12800. Queue acceptance is not
proof of audible playback; listening remains pending. Fire/retirement remain
unsupported. [Evidence](verification/voice-2026-09-21/REPORT.md).

Connection update: exact selected-failure DLL loaded into PID 12800 through the
elevated canonical SDK loader. Live endpoint responds and GUI launched. Session
generation advanced to 1 with 24 drivers. Pit-stop effects validation is still
pending. [Evidence](verification/connect-2026-09-21/REPORT.md).

Latest UI revision: driver + failure + OK. The live client accepts a one-stop
exact-category override and retains normal subtype/duration behavior. Five
recoverable categories are selectable; fire/retirement remain unavailable.
[Selection evidence](verification/selected-failure-2026-09-21/REPORT.md).
Native gameplay remains unverified; no stage change.

Latest: **live trial candidate**, built as an x86 broker client. Real roster,
one-stop requests, reviewed call-to-box input and bounded recoverable holds are
implemented. Actual-client fixture tests and canonical SDK gate pass. Native
load/gameplay remain unverified: game reader reported no running game during
connection preparation. See [live report](verification/live-2026-09-21/REPORT.md).
This supersedes older statements below that native controls are wholly absent.
Stage remains implement/prototype; audio/HUD, fire, fuel suspension, terminal
retirement and the complete player/AI acceptance target remain unfinished.

Current addition: **0.3.0-testbridge.1**, an executable offline GUI bridge using
the actual policy/session and repaired transaction mock. Forced player/AI,
normal comparison, weighted selection, stream preservation and one-shot
history checks pass. [Current evidence](verification/gui-bridge-2026-09-21/REPORT.md)
supersedes earlier statements below that the real-policy GUI bridge is absent.
Native controls and full gameplay/audio acceptance remain unimplemented/pending.
The owner's current request authorizes continued implementation and testing;
earlier permission-gate wording is historical, not a request for reconfirmation.

Latest mock result supersedes the earlier 184-check submission below:
[M1–M3 repaired directly](verification/mock-fixes-2026-09-21/REPORT.md),
240 checks on each architecture, 50 traces and four checker tests pass.
Invalid-clock writes, unsafe cleanup completion and stale GUI submissions are
covered by regressions and the original review probes. Real-policy GUI bridge
and runtime implementation remain pending; whole-session mock behavior unproven.

Product ID: pitstop-modding. Versions: 0.1.0-port.1 (legacy DLL source) and
0.2.0-policy.3 (offline policy v2, post-re-review repairs). Production: none
established. Stage: implement. Maturity: development prototype; the repaired
policy v2 offline A–F package passed independent re-review.

## Legacy 0.1.0-port.1

Canonical source: src/pit_commentary.c. Offline source/build port complete.
Verified: legacy copies agree; missing GUI and game configuration imported with
hash equality; active audio paths made host-relative; x86 build succeeds with
WinMM; source and artifact hashes recorded in fresh trial output.

## Policy v2 0.2.0-policy.3 (offline only)

Repairs the re-review findings R7–R9: cancellation purges every undelivered
command for the exact identity (active or already completed into history);
`pf_session_commit` is copy-out so no borrowed plan pointer can alias another
service or freed storage; a saturated delivery queue is still drainable, with
history space reserved at commit and explicit capacity refusal. Compiled
acceptance tests run on x86_64 and i686 in standard and bounded-limit builds
(429 + 362 checks each), with fixture-checked 200k pressure/tyre-only runs,
the million-stop matrix, replay and CLI rejection checks. No game contact, no
SDK change, no runtime integration; the pit-fire addendum stays a separate
unscheduled extension.

Evidence: [R7–R9 repair report and reviewer package](verification/policy-v2-r3-20260921/IMPLEMENTATION_REPORT.md);
[re-review that requested the repairs](verification/review-r2-2026-09-21/REVIEW.md).

Independent review: [R7–R9 accepted with fresh counterprobes](verification/review-r3-2026-09-21/REVIEW.md).

## Phase G 2026-09-21 — integration specification revision 4 (resubmitted for review)

SDK/broker capability investigation completed read-only through the canonical
front door. Deliverable: [integration specification](docs/INTEGRATION_SPEC_2026-09-21.md)
and [investigation record](verification/pit-integration-2026-09-21/REPORT.md).
Findings: identity/lifecycle/session context are broker-supported; per-stop
service identity, work eligibility, normal job timing, per-car pit hold,
commentary, HUD, fuel-service control, owned effects and AI retirement/garage
are missing and are specified as six narrowly scoped broker additions. The
pit-fire extension (conditional on a fuel-pump problem, callout + HUD, owned
effect provider) is specified against the same model. No runtime integration,
game modification or live write was performed.

Revisions 2–3 applied the review amendments (pit-event protocol, service
transaction deadline/lifetime, exact batch sequencing, worked timelines).
Revision 4 applies the remaining cleanup corrections: End preflight before any
side effect, lease-expiry-driven autonomous cleanup, dead-clock invalidation
rules, no unproven fuel estimate (provider-certified remaining-work bound),
G1 cursor edge rules, and the owner's complete player/AI acceptance target
(alpha cues, one-shot test-GUI arming, correct-subject AI narration, real
retirement/garage transition with narration only after authoritative
confirmation). Record:
[revision 4](verification/pit-integration-r4-2026-09-21/REVISION_R4.md).

Revision-3 review: [G2 cleanup corrections remain](verification/integration-review-r3-2026-09-21/REVIEW.md); main G1/G3/G4 design amendments accepted. [Owner gameplay target](docs/GAMEPLAY_ACCEPTANCE_2026-09-21.md) explicitly requires alpha cues and actual AI retirement/garage behavior for terminal outcomes.

## Isolated mock implemented (review-r4 handoff, offline only)

The revision-4 contracts are implemented as a deterministic offline mock with
the two mandated corrections: C1 invalid-clock cleanup obligation (no
fabricated destruction; same-identity completion or obsolete replacement) and
C2 suppression preserving remaining fuel work (certified bound before
suspension; `max(T_ord, resume + bound)` afterwards). 24 groups / 184 checks
pass on x86_64 and i686; deterministic JSONL traces are verified by an
independent checker (44 files, all invariants hold). GUI one-shot arming,
category-specific cue gates and the unavailable retirement provider are
covered. No game/SDK/runtime contact; accepted v2 policy code untouched.
Evidence: [mock report](verification/mock-contract-2026-09-21/REPORT.md).

Pending gate: independent review of the mock evidence, then the scoped SDK
provider work and Phase H runtime adapter (still unauthorized). Blockers
unchanged: the six missing broker capabilities; fire ownership evidence;
unproven fuel suspension; no AI retirement/garage path and unmapped retirement
narration; Brake clip/trigger unmapped; no audio-start observation; broker ABI
divergence; correct-box identification missing; clean-test install and guarded
weekend pending. Human gameplay/audio acceptance and release authority remain
pending.

Revision-4 review: [ready for an isolated offline mock assignment, not runtime](verification/integration-review-r4-2026-09-21/REVIEW.md). G2 still needs demonstrated same-session invalid-clock cleanup and post-suspension completion timing.

Pending gate: corrected executable mock evidence and independent review before Phase H (scoped
runtime adapter against reviewed SDK additions) and Phase I (guarded game
verification). Blockers: the six missing broker capabilities; the fire emitter
is LEGACY with shared-descriptor/RNG side effects and requires ownership
evidence; no proven per-car fuel-suspension mechanism; no AI retirement/garage
path and unverified retirement-narration mapping; Brake clip/trigger mapping
unverified; no audio-start observation (fire needs the HUD milestone); broker
ABI divergence (11/13/15) needs a recorded negotiation matrix; correct-box
identification missing; canonical clean-test installation is missing. Human
gameplay/audio acceptance and release authority remain pending.

[Legacy evidence and restoration](verification/port-2026-09-21/REPORT.md).
**2026-09-23 pit voice trial staged:** exact `0887E76D…` DLL adds player XPITIN
approach, wheel gun failure and successful fuel-clear callouts. Build,
actual-client and full SDK checks pass. Installed with game closed and prior
DLL preserved. Owner gameplay validation of clip and timing is pending.
[Evidence and restoration](verification/pit-voice-additions-2026-09-23/REPORT.md).

## Current release: 1.1.1 (2026-10-09)

Shipped and public bytes verified. [Current state](CURRENT_STATE.md) and [release evidence](../../../operations/release_management/pitstop_failures/RELEASE_1_1_1.md) supersede earlier current-version labels. Human exact-build gameplay remains pending.
