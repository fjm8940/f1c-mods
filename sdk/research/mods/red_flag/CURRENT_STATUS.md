**Published2026-09-22: Red Flag1.1.3.** Owner-accepted exact gameplay build; versioned installer and matching broker verified. [Download](https://mods.palloset.com/redflag); [release, tests and rollback](../../../operations/release_management/red_flag/RELEASE_1_1_3.md). Prior candidate/install notes below are historical.

**2026-09-22 replay roster guard r4 (Codex R7-R9):** [production-order lifecycle fix, paused-replay protection, history/intervals evidence](trials/2026-09-22/replay-roster-guard/RESPONSE_TO_CODEX_R7_R9.md). RF review `392BCD8B...`/hardened `1947A1B2...`; SC private `5AFFBA13...`/protected `CD57AA58...`. R7 automatic-rewind re-add remains an explicit blocker. Not installed.

**2026-09-22 replay roster guard revision 3 (Codex R6-R8):** [TwoPathDispatch hook with real trampoline tests, replay-history ordering, first-live-tick restore](trials/2026-09-22/replay-roster-guard/RESPONSE_TO_CODEX_R6_R8.md). RF review `F0179ABF...` / hardened `8A165849...`; SC private `8606A9F4...` / protected `5AE982D7...`. Not installed.

**2026-09-22 replay roster guard revision 2 (Codex R1-R5):** [game-thread capture, watchdog integration, identity contract, native visibility helper; SC-side replay detour](trials/2026-09-22/replay-roster-guard/RESPONSE_TO_CODEX_R1_R5.md). RF review `509D50BD...` / hardened `77BB590C...`; SC private `D3F03CBA...` / protected `93617FF0...`. Not installed; Codex review requested.

**2026-09-22 replay roster guard candidate:** [root cause, SC/replay determination, regressions, hashes and acceptance checklist](trials/2026-09-22/replay-roster-guard/README.md). Keeps the preserved roster-transaction correction; adds a game-thread replay-exit guard that restores counted membership and the native control/visibility fields for exactly the captured live roster after native replay destroys the parked/excluded SC and shrinks VehicleCount. Not installed; Codex review requested.

**2026-09-22 roster transaction fix verified:** old source reproduces Massa's green-restore failure; new callback commits full roster only after survivor restores succeed. Hardened candidate 4ADE75A1… awaiting closure/install/native retest. [Evidence and Alonso/Montoya collision diagnosis](trials/2026-09-22/massa-stuck-r4/IMPLEMENTATION.md).

**2026-09-22 r4 gameplay failure: Massa stuck after restart.** Fuel passes owner test; live state/log expose a partial-roster transaction abort before pit-strategy restore. [Evidence and repair handoff](trials/2026-09-22/massa-stuck-r4/README.md). Stop acceptance pending correction; earlier ready-to-test review is superseded by this native failure.

**2026-09-22 r4 independent review: ready for in-game acceptance.** Remaining fallback ownership defect resolved; focused canonical and hardened tests independently pass. [Review and minimum gameplay tests](trials/2026-09-22/ai-pit-strategy-preservation/reviewer-r4/REVIEW.md). Exact hardened hash starts 2A8EF5FB. Not installed; not release-approved. Earlier review notices below concern superseded candidates.

**2026-09-22 r3 independent review: changes requested.** Timeout fallback clears RF parking ownership and lets the strategy refresh lose a legitimate future stop. [Reproduction and required correction](trials/2026-09-22/ai-pit-strategy-preservation/reviewer-r3/REVIEW.md). No installation. Earlier review notices below refer to prior candidates.

**2026-09-22 pit-strategy candidate review: changes requested.** Buffer bound, capture after forced parking, pending pit requests and game-thread timing need correction. [Review and reproductions](trials/2026-09-22/ai-pit-strategy-preservation/reviewer/REVIEW.md). No installation.

**2026-09-22 AI pit-strategy restart preservation candidate, revision 3:** [review responses, review/private builds, tests and pending native acceptance](trials/2026-09-22/ai-pit-strategy-preservation/README.md). Native restart resets the plan cursor and clears an active request; RF captures each survivor's plan/cursor/request at trigger, holds RF-armed cars through a dedicated per-sequence contamination list that survives the DriveToPits->Countdown timeout fallback clears, refreshes genuine window progress, and restores once in the game-thread green transaction before the first AI decision, with compaction and the Y checkpoint. Independent r4 review found no remaining blocker; the exact hardened hash `2A8EF5FB...` was installed for guarded in-game acceptance 2026-09-22 (existing broker/settings kept, rollback preserved); human validation pending.

**2026-09-21 TesterI pit-call investigation:** native restart resets strategy progress while RF restores late-race laps; extracted native decision test reproduces an overdue-stop request. Strong matching explanation, customer-specific native proof pending; no runtime strategy fix yet. [Evidence and repair scope](trials/2026-09-21/Tester I-pit-call/README.md).

**2026-09-21 AI pit-retirement fuel reset confirmed:** pre-constructor capture was replacing saved race fuel with full native tanks. Pre-service preservation and consumption tracking built/verified; installation awaits closure. [Live evidence, tests and native retest](trials/2026-09-21/policy-v89/pit-fuel-preservation/README.md).

**2026-09-21 fuel callback fix installed:** reproduced a constant-address callback compilation defect, corrected the call and verified player/AI fuel restoration in an isolated before/after test. Final DLL retains the native call. [Evidence, rollback and pending native retest](trials/2026-09-21/policy-v89/fuel-call-fix/README.md).

**2026-09-21 native retest: T succeeds, fuel still fails.** Restart HUD feedback and exact fuel-refusal diagnostics built/verified; installed after game closure; targeted gameplay retest pending. [Evidence and next targeted test](trials/2026-09-21/policy-v89/fuel-retest-feedback/README.md). Earlier fuel-repair acceptance remains pending.

**2026-09-21 pit-service T confirmation fix installed:** stationary native pit state 4 now permits explicit T confirmation; automated checks pass, gameplay retest pending. [Evidence, installation and retest](trials/2026-09-21/policy-v89/pit-confirm-fix/README.md).

**2026-09-21 fuel-test failure repair installed:** one-shot native grid fuel restoration and sealed-restart penalty-read ordering corrected; automated gates pass, native retest pending. [Failure evidence, limits, paired hashes and retest](trials/2026-09-21/policy-v89/fuel-failure-fix/README.md).

**2026-09-21 paired private candidates installed in Documents/F1 Challenge V10:** RF89 no-refuel + penalty hold, SC0.10.63 and compatible broker. [Installation, rollback and test checklist](trials/2026-09-21/policy-v89/test-install/README.md). Human validation pending; earlier uninstalled notes are historical.

**2026-09-21 owner scope correction:** RF restart now preserves current fuel;
full-tank refuelling is removed, including legacy INI activation. Penalties stay
preserved. Tyre/damage restoration and optional repair work were dropped by the
owner. [Change, passing tests and remaining installation gates](trials/2026-09-21/policy-v89/completion-work/no-refuel/README.md).
Not installed; earlier full-service requirements below are historical.

**2026-09-21 continuation:** penalty event capture/service/deadline hold and
`RestartService.Refuel` are implemented in review source, with isolated runtime
tests and a new broker dependency. [Evidence and remaining scope](trials/2026-09-21/policy-v89/completion-work/penalty-hold/README.md).
Not installed; full gameplay handoff remains unavailable. Earlier partial-work
notes below are historical. No published lifecycle stage change is claimed.

**2026-09-21 completion pass — full test handoff still unavailable:** [Implemented weather hold, SC handoff fault, native fuel mirror and exact-code guards; passing tests and remaining native work](trials/2026-09-21/policy-v89/completion-work/README.md). Review build only, not installed. Independent service, post-announcement penalties, race conclusion and ultimate recovery remain unfinished.

**2026-09-21 RF89 / SC0.10.63 core candidates verified; native features incomplete:** [accepted owner decisions, changed behavior, tests and remaining native gates](trials/2026-09-21/policy-v89/README.md). Core policy/runtime tests pass. Independent service options, safe race conclusion and ultimate under-map recovery remain unfinished; native gameplay and protected-build gates pending. Not installed or distributable. This entry supersedes the earlier awaiting-answers note.

**2026-09-21 behavior specification pending:** [RF/SC decision list and authorized continuation](trials/2026-09-21/behavior-questions/README.md). Added pit-readiness and forced-return policy reproductions; runtime unchanged.

**2026-09-21 SC/RF issue review:** [seven extracted-policy assertions, confirmed post-lane DNF exemption, manual pause cancellation, keybind gap and VPS triage](trials/2026-09-21/sc-rf-review/README.md). No source/DLL/INI change; reported automatic SC takeover not yet causally reproduced.

**2026-09-21 RF88 r2 installed:** [drive-to-pits wreck classification and 1 s replay readiness cutoff](trials/2026-09-21/drivetopits-wreck-v88/README.md). r1 live-rejected (missing arrival-latch allocation caused mass false DNFs and aborted the restart); r2 adds the allocation, fail-safe guards and static contracts. Installed hash `61DDC40369CA0700C9409231A2012782816F1A50156CD5AC66DB37629A3B20EF`;; gameplay validation pending. Public v1.1.2, INI and Safety Car unchanged.

**2026-09-21 live DriveToPits diagnosis:** [paused hold before the replay deadline; restart completed on unpause; Trulli retention explained](trials/2026-09-21/drivetopits-pause-trulli/README.md). Installed RF87, INI and public releases unchanged; no source, build or production change.


**RF87 / SC0.10.56 installed (2026-09-15):** [separate Safety Car fault request and verified physical handoff](trials/2026-09-15/sc-fault-v87/README.md). Private hardened RF87; settings/public releases unchanged. Native recovery and redeployment validation pending; earlier installed-version notes below are historical.

**RF86 / SC0.10.54 installed (2026-09-15):** [player excluded from automatic incidents; verified build and pending native validation](trials/2026-09-15/player-exclusion-v86/README.md). RF85 replay preparation and manual controls retained; settings/public unchanged. Earlier installation notes below are historical.

**RF85 installed for private testing (2026-09-15):** [nonblocking replay preparation, retained field hold, exact installation and pending gameplay](trials/2026-09-15/replay-prepare-v85/README.md). SC0.10.53 retained. Regression and SDK gates pass; four legacy static failures unchanged. Native SC-to-RF restart remains to be validated. Earlier installed-version notes below are historical.

**RF84 installed for private testing (2026-09-15):** [exact DLL, verified backups and pending gameplay checks](trials/2026-09-15/player-preflight-v84/installation-172057/README.md). Prevents unsupported terminal-player RF restart before roster changes. Fresh launch required; invisible-collision cause remains unconfirmed. Earlier installation notes below are historical.

**RF84 preventive candidate, not installed (2026-09-15):** [early terminal-player refusal and native regression](trials/2026-09-15/player-preflight-v84/README.md). Public v1.1.1 currently installed for reproduction; its second restart failed player-state preservation and left21/17. RF84 avoids that path by refusing unsupported player states before roster changes; full terminal-player restart support and native acceptance remain pending. Four legacy static failures are unchanged from baseline; focused native tests and SDK gate pass.

**Historical RF83 installed for beta validation:** [consistent HUD, protected build and paired package](trials/2026-09-14/messages-v83/README.md). Public1.1.1 unchanged; final gameplay/member activation pending.

**SC v0.10.34r2 / RF v79r2 installed — corpus fixes:** [handoff rearm, stall coverage, observation clocks, geometry seams and evidence](trials/2026-09-10/corpus-v79/README.md). Automated gates pass; fresh native validation and exact footprint research remain. Public releases unchanged.

**2026-09-10 corpus audit: remaining coverage/recovery gaps:** [five reproduced policy counterexamples, native evidence and whole-cycle scenario matrix](../full_safety_car/trials/corpus_adversarial_audit_20260910/README.md). SC/RF exclusivity does not prove complete coverage; no runtime change.

**SCv0.10.33r2 / RFv78r2 installed:** [coordinated ownership, acknowledged restoration, passable-incident coverage and verification](trials/2026-09-10/coordinator-v78-r2/README.md). Automated gates pass; fresh native coexistence/cancellation testing pending. Public releases unchanged.

**v77 installed:** [private installation evidence](trials/2026-09-10/stale-notice-v77/INSTALLATION_2026-09-10.md). Public v1.1.1 unchanged.

**v77 pending installation:** [stale-warning coordination fix, protected build and regression evidence](trials/2026-09-10/stale-notice-v77/README.md). Published release unchanged.

# Red Flag current status

> **Published v1.1.1 hotfix (2026-09-08).** Owner-authorized shipment of the exact installed protected v76 DLL. T requires the game foreground; manual confirmation and accepted automatic offers now have a cancellable activation timer, configurable with `Timing.ManualActivationCountdownSeconds=10`. [Download](https://mods.palloset.com/redflag), [player README](releases/v1.1.1/package/readme.txt), [changelog](CHANGELOG.md), [publication verification](../../../operations/release_management/red_flag/RELEASE_1_1_1.md). Public ZIP hash and completed delivery verified; analytics discovered the release automatically. Detailed fresh-game/HUD test results were not supplied. Earlier candidate and publication notes below are historical.

> **Installed v76 protected candidate (2026-09-08).** Owner closed the game; installed exact DLL `420C8D689B9AB1DFC382E66A576F1274CF0D0BE68E4451DD02173058BFFB4DF8` with foreground T filtering and cancellable manual activation countdown. Added only `Timing.ManualActivationCountdownSeconds=10` to the existing INI. [Installation record](trials/2026-09-08/manual-countdown-v76/INSTALLATION_2026-09-08.md). Fresh-game/HUD validation remains pending; earlier waiting-for-close notes below are historical.

> **Latest candidate: v76 cancellable manual countdown (2026-09-08).** A configurable 10-second delay follows both independent manual confirmation and T acceptance of an automatic offer. Fresh foreground T cancels, even on the expiry tick; accepted incident gates are revalidated at expiry. Includes v75 focus fix. Protected build verified; not installed while game PID 23708 remains active. [Record](trials/2026-09-08/manual-countdown-v76/README.md).

> **Latest candidate: v75 foreground-only T (2026-09-08).** Fixes T pressed in other applications opening/accepting Red Flag. Game foreground ownership and a fresh post-focus key edge are required; modifier filtering remains. Protected DLL is ready, with installation blocked by running F1 Challenge Online PID 23708 at verification. [Evidence and installation status](trials/2026-09-08/foreground-t-v75/README.md). No public release changed.

> Published v1.1.0 r5: player-facing protection details and privacy-notice link removed at owner request. Runtime and INI unchanged. [Record](../../../operations/release_management/red_flag/RELEASE_1_1_0_R5.md).

> **Changelog audited and published (2026-09-07):** [Source audit](trials/2026-09-07/changelog-audit/AUDIT.md). Public v1.1.0 revision r4 includes the corrected changelog and README; runtime/INI unchanged from r3. [Release record](../../../operations/release_management/red_flag/RELEASE_1_1_0_R4.md).

> **Published protected v1.1.0 revision 3 (2026-09-07).** Exact owner-accepted hardened DLL, garage fix and automatic-offer T acceptance enabled. [Release record](../../../operations/release_management/red_flag/RELEASE_1_1_0_R3.md). Download: https://mods.palloset.com/redflag. Earlier candidate notes below are historical.

> **Latest development candidate: IP hardening v74 (2026-09-07).** Ten Ultra regions and private event-code diagnostics. Protected garage and checkpoint harnesses pass; fresh-game testing of the exact protected DLL is still required. The owner confirmed the installed v73 garage fix works. [Hardening record](trials/2026-09-07/ip-hardening-v74/README.md). No new installation or publication.

> **Installed testing candidate: v73 garage pose fix (2026-09-07).** Stops writes to shared AIW garage heights and restores owned AI retirees through native GarPos/GarOri application. Installed for next launch; fresh game required; visual validation pending. See [v73 record](trials/2026-09-07/dnf-garage-v73/README.md). Public v1.1.0 remains unchanged.

## Published v1.1.0 — 2026-09-07

Owner-approved v72 RC2 is now published as v1.1.0. Download: https://mods.palloset.com/redflag. Includes steady icon, configurable manual/automatic triggers, restart methods, Y checkpoints, plain-T filtering and Esc cancellation.

See [changelog](CHANGELOG.md), [end-user README](releases/v1.1.0/package/readme.txt), and [publication verification](../../../operations/release_management/red_flag/RELEASE_1_1_0.md). The exact runtime remains installed in `%USERPROFILE%\Documents\F1 Challenge V10`.

## Historical development notes

The notes below describe earlier candidates and their gates; the publication record above is current.

As of: 2026-09-07

## Latest installed testing build: v72 RC2

Installed into Documents/F1 Challenge V10 with prior DLL/INI/log preserved.
Includes v71 modifier-key exclusion. Esc now cancels the pending Red Flag
countdown or pit-return sequence rather than letting it continue after exit.
Player-out/results gates cancel even if a race-type pointer array survives.
Replay-finalization return and the existing game-thread restart gate revalidate
the context; a still-owned pending byte can be withdrawn before consumption.
Cold abort now restores the icon too. 52 static contracts and native cancellation
and manual/checkpoint tests pass. Exact live Esc/countdown/normal-restart checks
remain pending. Evidence: `trials/2026-09-07/escape-cancel-v72/README.md`.

## Latest testing build: v71 plain-T input

Fixes Ctrl+T speed-up accidentally initiating/confirming Red Flag. The shared
release T edge requires no Ctrl/Alt/Shift/Windows modifier and tracks the raw
T state even while a modifier is down. Releasing a modifier while holding T
does not create a new edge. No INI change. Build, 51 static contracts and native
manual/checkpoint regression harness pass. Installation and live validation
status: `trials/2026-09-07/plain-t-v71/README.md`.

## Latest testing candidate: v70 RC2 manual control and Y checkpoints

Built DLL SHA-256: `573CB42CC891BBF6F8BAECC2978F392C5CFAF7D13A6100F4A83CA8868148C963`.
`Activation.ManualEnabled=1` enables independent T initiation. First fresh T
opens RACE WILL BE STOPPED / CONTINUE?; second fresh T confirms. Esc, pause,
unsafe session or a 15-second timeout cancels. AutomaticEnabled=0 disables
incident detection but does not disable this separate manual route.

PostRedFlag Y now restores the saved latest completed RF restart's grid,
survivor laps/fuel/visibility and DNF membership; later racing snapshots do not
replace it. Second completed RF replaces it. Zero-DNF checkpoints are covered.
FullRace restores initial qualifying positions before native whole-race reset.
Missing/stale checkpoint identity fails closed. 51 static contracts and native
isolated tests pass, including icon regression. Exact in-game feature tests are
pending; this is not a production promotion. See
`trials/2026-09-07/manual-checkpoint-v70/README.md` for installation status.

## Latest testing build: v69 steady red icon

v69 supersedes v68 for testing. It retains v68's Live HUD bridge and removes
the background writes to the native blue-flag field. Both existing renderer
windows are validated in full before a scoped one-byte branch-displacement
override makes the red sprite continuously eligible during the stop. Restart,
abort and teardown restore native gating and blue sprites without touching
the native vehicle flag. Exact x86 execution tests cover both native-bit
values, repeat lifecycle, incompatible-byte refusal and partial-failure
rollback. Existing 50 static contracts pass. Full SDK verification is blocked
by its stale Desktop/F1/F1 Challenge V10 executable path; it is not a pass.
Visual countdown/pause/restart/ESC/new-session validation remains pending.
Evidence: `trials/2026-09-07/icon-flicker/V69_IMPLEMENTATION.md`.

Authority: canonical development status for the current Red Flag trial. Earlier
session summaries and trial records remain evidence, but this file supersedes
their descriptions of current behavior.

## Public feedback under investigation

- A 2026-08-31 v18 live attempt proved that the restart request was received
  but deliberately warm-aborted because its `PitState>=3` garage requirement
  was never observed; this caused the visible `CLEARING THE TRACK` stall. That
  requirement is disproven for the native DQ path. The v34 replacement keeps
  the terminal player counted but outside active grid order, disables the
  player SessionTick hook, performs no player state or pose writes, and lets
  F1C place the player while logging GRID_READY/green observations. It retains
  the survivor-only replay roster and latches any identity/tuple failure until
  session exit. It additionally proves ordered survivor-pointer identity at
  GRID_READY and green before saved-pointer writes, moves green fuel restoration
  inside that verified transaction, and holds on every owned restart/green
  failure or timeout. Timeout cancels only an unclaimed broker callback and
  waits for a game-thread transaction already in flight. Static contracts pass
  41/41; v34 is exact-hash staged in the closed V10 clean-test profile and
  requires visual garage/restart/replay validation.
  Evidence: `trials\2026-08-31\player-dq-garage\PLAYER_DQ_GARAGE_DIAGNOSTIC_RECORD.md`.

- `feedback\BR-20260828-A0B0C6.md` records a public v1.0.0 report in which a
  user pressed the game's `Y` restart key after a completed Red Flag restart
  and multiple cars appeared at the player's position. The defect was reproduced
  on 2026-08-31 with the exact public hashes: nine DNFs were correctly excluded
  from Red Flag's 13-car restart, the complete 22-car results roster was restored,
  and a later `Y` restart kept the cars retired through the lights before moving
  them onto the grid at lights-out. The released epoch watchdog did not observe
  the same-pointer/count vanilla restart, so no DNF guards or green transaction
  ran. Fix specification and a focused game-thread transform trace remain; no
  source, candidate, or production change has been made.

## Lifecycle status

- Current F1 Live HUD compatibility trial: v68, Red Flag SHA-256
  `8D57C1185F1895CB3E7F8CF356FC023AEE5531EAA8270EE003A95251A42BB32E`,
  adapted F1CUILive v0.6.0.1 JAR SHA-256
  `99F8C5C9FE666F46AC23DF162CF44DE1C1EE15511D57C19BCC5C592BEEB9AC09`.
  Static analysis proved the stock HUD reads `VehicleCount` at `0x005C329C`
  to bound a fixed contiguous driver table, while Red Flag must temporarily
  reduce that count for safe survivor-only restart construction. v68 publishes
  the original entrant count through `F1C_REDFLAG_LIVEHUD_V1`; the exact-hash
  adapter changes only the HUD's expected-count invocation and falls back to
  the unmodified read if the bridge is absent, inactive, inconsistent or out
  of range. Static contracts pass 50/50; deterministic DLL/JAR rebuilds and
  Java named-mapping active/fallback probes pass. No installed HUD or game was
  modified. A guarded live Red Flag with DNFs remains required to prove the
  standings, lap/radio/incident state, repeated Red Flag, later `Y` restart,
  replay and final classification together.
  Evidence: `trials\2026-09-01\f1-live-hud-compat-v68\V68_F1_LIVE_HUD_COMPATIBILITY_RECORD.md`.

- Current classification-restoration diagnostic: v66 isolated debris broker
  client, SHA-256
  `9326D8E09B742F05F4BA4C87A21AC73E7E37A5D11771000F39A34DB7B3EC509E`.
  Live v64 correctly reduced 22 entrants to 11 survivors for restart
  construction, then failed every green-restoration post with stale-token
  result 4 and retained the temporary roster as `11/11`. The one-shot debris
  callback had exceeded LightLoader's 5 ms callback budget and quarantined the
  shared control client. v66 registers a separate cleanup client; full-results
  restoration and the Y guard retain the untouched control token. Static
  contracts pass 49/49. It is exact-hash installed in the closed V10 clean-test
  profile; v64 and the failure log are preserved. Required live pass: debris
  clears, restart completes, green restore publishes all 22 entrants, HUD shows
  `x/22`, and final classification contains all retirees. Evidence:
  `trials\2026-09-01\cleanup-client-isolation-v66\V66_CLEANUP_CLIENT_ISOLATION_RECORD.md`.

- Current debris-cleanup diagnostic: v64 one-shot DynMan scan, SHA-256
  `E5BD3D5E3045BC78753BD52BA78050E9AAB187633FC3CD33A687210AFEC15B6F`.
  Live v62 evidence proved the corrected manager address removed the debris and
  clamped 33 loose-part expiries, but a repeat scan wedged the restart worker on
  `CLEARING THE TRACK`. v64 records and clamps the loose objects once, prevents
  a second broker post, and retains native zero-duration cleanup plus the 300 ms
  stabilization period. It is exact-hash installed in the closed V10 clean-test
  profile; the displaced v62 DLL, INI and final log are preserved. Static
  contracts pass 49/49. Live debris disappearance and successful transition
  from cleanup to restart remain required. Evidence:
  `trials\2026-09-01\one-shot-debris-v64\V64_ONE_SHOT_DEBRIS_RECORD.md`.
  **Superseded/rejected:** live v64 produced `11/11` after its shared control
  client was quarantined by the debris callback. Do not reinstall v64.

- Current diagnostic candidate: native terminal-player placement/replay-count
  stable v34, SHA-256
  `E444FAC38E5D8CD330E0BD03C054041C8649FC019956D919EEF9829039903369`.
  It is exact-hash staged in the closed V10 clean-test profile. Installed v18
  was preserved before replacement and is rejected by live evidence. v34 must
  prove that native F1C placement keeps the
  counted terminal player in the pit garage while AI restart, stable green and
  Instant Replay proceed. Final results remain intentionally incomplete.

- Current development trial: v60 protected, hotkey-free release profile staged
  with LightLoader exclusivity ABI v2. It retains v58's live-total and
  individual per-driver lap continuity, v54's separate broker clients and the
  previously human-validated DNF classification behavior. v60 adds the safely
  bounded `Timing.RestartCountdownSeconds` setting, keeps
  `Timing.CooldownSeconds` authoritative after stable green, and removes the
  redundant visible `RACE STOP IN x SECONDS` pre-stop page. Exact-pair organic
  in-game restart validation remains required before distribution. Evidence:
  `trials\2026-08-25\V60_CONFIGURABLE_RESTART_COUNTDOWN_RECORD.md`.
- Current development trial: v45 native-replay-finalize RC2. Live v44 testing
  proved its filename hook did not save the segment ending at restart: only a
  Part 3 container existed after two restarts, while Parts 1 and 2 were absent.
  Static inspection found the cause: `Game_Restart` sets GameState 4 before
  calling the native replay state machine, whose writer refuses state 4. v45
  removes the replay filename hook. Before each restart it posts F1C's native
  replay finalizer to LightLoader's game-thread broker while the race is still
  saveable, waits for the native write to close, and moves the vanilla file to
  a collision-safe timestamped Part N. The restart then creates the next
  vanilla segment. Queued timeout requests are cancelled and an in-flight
  native writer must finish before restart. No-Red-Flag races remain vanilla.
  RC2 is exact-hash staged in the closed V10 test profile; live creation and
  playback of Parts 1/2/3 is still required before distribution.
- Superseded v43 terminal-line-authority RC1. Live v42 testing
  confirmed that Bernoldi was terminal and visibly directly on the racing line,
  with de la Rosa forming a second terminal cluster member, yet the detector
  logged a placeholder and no Red Flag. Bernoldi's physics speed was 0.852 m/s,
  close enough to the 1.0 m/s cap for jitter to reset confirmation, while the
  broader AIW `on_road_geom` result contradicted the visible line obstruction.
  v43 makes exact AIW line overlap authoritative even when the broader road
  polygon disagrees and adds a separately clamped 1-5 m/s terminal-only jitter
  threshold (default 3 m/s). Active/recoverable cars retain the strict 1 m/s
  limit and heavy-damage requirement. v42's close-pileup and NUMPAD2-DNF fixes
  remain intact.
- Superseded v42 on-track-pileup/NUMPAD2-DNF RC1. Live v41
  testing exposed two false behaviors: Panis and de la Rosa were terminal,
  approximately 10 m apart, on track under a sustained native yellow, but the
  exact-line-only policy refused the Red Flag; the subsequent manual NUMPAD2
  path classified all 22 as active and revived three terminal wrecks as
  pristine grid entrants. v42 adds a second fail-closed spatial proof: by
  default, two terminal wrecks within 40 m where every counted car is on active
  branch-0 road geometry. Grass/gravel pileups and unrelated DNFs remain
  passive. NUMPAD2 now captures unclassified terminal wrecks before arming
  temporary pit retirees, and its un-retire pass defensively refuses every
  captured DNF. The
  script contains no Safety Car action or recommendation: serious incidents
  below the exceptional Red Flag threshold either write one optional
  `RedFlag.log` placeholder or do nothing. A healthy recoverable active stall
  cannot authorize Red Flag even on the racing line; a damaged active
  direct-line blocker must remain under the strict speed cap and overlap the
  active line continuously, but no longer depends on F1C reporting severity
  level 2. This corrects the live Alesi case where a stopped car visibly
  missing a wheel was downgraded to `road hazard=1`. All effective INI values
  are now logged after
  defaulting/clamping, with a player toggle for passive placeholder notes.
  Loader initialization remains fail-closed to the exact compatible
  LightLoader SHA-256, private ABI/version, process ID, nonce/seal/proof export,
  `dinput8.dll` module identity, supported game layout and LightLoader broker.
  Ordinary DLL/ASI injection leaves inert `DllMain` and starts no worker.
  Static replay investigation proves that F1C's full `Game_Restart` invokes
  replay-controller restart unconditionally; Red Flag performs no replay-file
  or replay-memory writes. Whether vanilla overwrites, splits, or preserves the
  pre-flag segment remains an exact live-file gate. The first live comparison
  found no `.Vcr` metadata or hash change before restart, after restart, or
  after process exit, so that session produced no testable replay file and did
  not resolve continuity. RC1 builds cleanly and is
  exact-hash staged in the closed V10 test profile with its updated INI.
- Superseded v40 racing-line race-control RC3: live v39 review
  disproved its broad AIW danger-band rule: at Monza turn 1, two terminal cars
  approximately 16.1 m and 39.4 m beyond the road edge (22.8 m and 49.8 m from
  the active racing line) armed a Red Flag solely because the local incident
  contained two terminals. v40 separates the decisions. Automatic Red Flag is
  authorized only by a sustained effectively-stationary non-player car whose
  physical footprint directly overlaps the active weather racing line on main
  AIW branch 0. Default proof is below 1.0 m/s for 3000 ms and at most 1.5 m
  from the line centre; safe INI clamps allow 0.1-1.0 m/s and a 0-0.75 m
  footprint margin. Pit geometry, broad danger/pass-space bands, wheel-contact
  fallback, damage severity, weather and incident counts cannot replace direct
  line proof. Missing AIW data fails closed. Existing local multi-terminal,
  mixed and strong-wet patterns now log a Safety Car candidate and explicit
  Red Flag refusal. A sustained singleton car on usable main-track asphalt but
  outside the direct line is also a Safety Car candidate; a singleton beyond
  the AIW road footprint is not. These decisions do not deploy Safety Car
  because the SDK observer is currently passive. RC3 built cleanly but was
  superseded before staging by the stricter passive-only v41 policy.
- Superseded development history through v39: v39 severe-obstruction RC2 retained
  the v30 HUD reasons/sectors, v29 session hygiene, v27 LightLoader binding and
  v28 configurable activation. v31 and v32 are both rejected by live gameplay:
  v31's cached transforms placed DNFs in grid boxes; v32 constructed three DNFs
  on the restart grid and kept the red icon visible. The exact verified v33
  RC3 proved the restart exclusion itself, but live results contained only the
  19 survivors because its reduced `VehicleCount` remained visible to final
  classification. v34 is also rejected: restoring the full roster at
  GRID_READY revived two DNFs into the shared P22 box. v35 is also rejected:
  delaying the full-count restore avoided revival and changed the HUD from
  16/16 to 16/22, but final results still contained only the 16 survivors.
  v36 kept the complete entrant count and pointer array unchanged, but live
  testing crashed exactly when the mini-restart was triggered. Its log ends
  immediately after `Mini-restart triggered`, before GRID_READY. v36 is
  rejected and preserved as failure evidence. The V10 profile has been rolled
  back to the exact non-crashing v33 RC3 DLL
  (`3A5294E24184734F1A46B4D4F3ABFADD4EE3F785640C334F9A5A09A5983B17B3`).
  v33 remains the preserved rollback. v37 used v33's survivor-only restart
  roster and restores the original ordered entrant view inside the first
  game-thread live update, after pinning every DNF as a vanilla-shaped retiree.
  Live testing proved its private green hook never ran: it safely retained 18
  survivors, and only 18 appeared in results. v38 retains the same ordered
  transaction but posts it through the proven LightLoader game-update broker.
  Exact RC2 is staged in the closed V10 profile. Its first live test passed:
  18 survivors restarted without revival, the broker restored VehicleCount to
  22 at live phase, and all 22 original drivers appeared in final results with
  the four excluded drivers correctly retired. Repeated-red and next-session
  hygiene remain human gates. A later live probe exposed Barrichello stranded
  with two front wheels missing while F1C retained ActiveFlag=1 and chassis
  jitter near 0.7 m/s. v39 fixes that detector gap: after the configured
  blockage confirmation, an active `carConditions >= 2` vehicle below the
  safely capped 8 m/s severe-obstruction speed may satisfy reason 4 only when
  AIW independently proves a Dangerous/Critical main or pit corridor. It does
  not forge terminal state. Its exact RC2 hash is staged in the closed V10
  profile and remains pending live validation.
- Automatic trigger: compiled ON only in the explicitly named trial DLL.
- Distribution: .
- Promotion gate: configurable-trigger, wet-transition and full-race restart
  validation by Matias.
- Canonical editable source: `src\gr44.c`.
- Generated source: `generated\redflag.c`; regenerate only through the
  canonical Red Flag trial build tool.
- GUI: `tools\field_gui.py`; the DLL contains no active GUI.

## v27 LightLoader dependency and hardening

- `DllMain` is inert: loading the DLL directly does not create an event,
  timer-period request, worker thread, hook, monitor mapping, or game write.
- Startup is possible only through the private LightLoader host ABI.
- The host record is process-bound and carries a fresh nonce, seal, module
  handle and proof callback owned by the loaded LightLoader module.
- Red Flag accepts only the exact LightLoader RC binary whose SHA-256 is
  `0391640D0CFDF7154357FA3F2C27BF8BFE8AAA9AB3773F5BCDBB27568834F321`.
- Game executables are not hash-bound. A different 32-bit EXE is accepted only
  when the fixed-address layout, three original hook targets, two original
  sprite bytes, required data regions and callable regions validate at runtime.
- Unknown multiplayer state now fails closed, and all trigger paths refuse
  unreadable/corrupt fields or allocation failure; per-car storage otherwise
  scales dynamically with the current race field and has no fixed car cap.
- Non-game executables and unsupported game layouts fail before a worker
  exists. LightLoader owns the accepted DLL for the process lifetime.
- The release build exposes one deliberately opaque entry name. Diagnostic
  strings are intentionally retained and written to adjacent `RedFlag.log` so
  end users can attach a direct per-session bug report.
- Exact build and smoke-test evidence is in
  `trials\2026-08-20\LIGHTLOADER_BOUND_V27_RC5_RECORD.md`.

## v26 static-review hardening

- Release generation now preserves functional `wsprintfA` calls and fails
  closed on an unclassified destination instead of silently removing it.
- The legacy load-time `.PLR` rewrite is compiled out; the DLL no longer
  enumerates or writes player profiles.
- Automatic-trigger timestamps are shifted across pauses, so yellow,
  terminal, incident, opening-window and cooldown clocks advance in race time.
- Extreme wetness (`>=0.96`) now assigns a distinct nonzero stop reason to the
  supported one-terminal plus one-stationary-damaged pattern.
- Every automatic reason, including a blocked driving corridor, uses the same
  ten-second visible warning before the race stop.
- Both active restart hook sites and the executable layout are validated before
  any patch; unload refuses to overwrite a site now owned by something else.
- Worker startup is checked, its handle is closed, and loader-lock detach no
  longer sleeps or runs complex teardown.
- The Python viewer matches weather-assisted gates and ignores partial vehicle
  samples instead of treating failed life reads as zero/critical damage.

## Automatic activation

The reviewed activation policy is now player-editable through `redflag.ini`
beside the DLL. It is read once at plug-in startup; game restart is required
after editing. Missing/malformed values use reviewed defaults and supported
ranges clamp. The sample and complete key reference are in `redflag.ini` and
`docs\CONFIGURATION.md`. Default values preserve the v27 behavior below.

Configurable policy covers automatic enablement, the native-yellow requirement,
confirmation/cooldown/notice timing, locality radii, speed buckets, wetness
tiers, and severe-pattern enablement/counts. Loader/layout validation,
offline-only operation, settled-race state, pause/restart exclusion and player
exclusion remain immutable safety gates.

Every race owns a freshly allocated state block sized from its live vehicle
count. Leaving for the menu, ending the race, or directly rebuilding the field
aborts outstanding work, clears every timer/list/count and releases that block.
The log records matching `SESSION n START`/`END` boundaries.

Automatic qualification now captures its trigger-time sector from the active
track's LapLength/Sector1Length/Sector2Length metadata, with bounded AIW
progress fallback. During the pre-stop notice, the native HUD rotates six
short pages: warning, incident type, car/terminal counts, blockage/stationary
detail, sector or pit-lane location, and seconds until race stop. The external
field viewer appends the same sector/pit-lane location to its reason banner.

Automatic activation requires all applicable gates:

1. A settled offline race (`GameState` 5/7 and `RacePhase` 3/5), not a grid,
   completed session, multiplayer session, paused clock, restart sequence, or
   cooldown period.
2. Native yellow persistence: normally 3 seconds; the guarded opening window
   uses 1 second. At on-path wetness `>=0.90`, a verified incident needs only
   2 seconds of yellow outside the opening window.
3. One validated severe pattern:
   - two confirmed crash-terminal cars in one local incident; or
   - one confirmed terminal plus two independently stationary damaged cars; or
   - one confirmed stationary terminal car occupying a Dangerous/Critical
     main-track or active-pit AIW corridor for at least 3 seconds.
   - At on-path wetness `>=0.96` only: one confirmed terminal plus one
     independently stationary damaged car.
4. Locality for multi-car patterns: 300 m normally, 200 m during the opening
   window. Scattered retirements do not combine.
5. Final stable confirmation for 1 second; `>=0.90` wetness reduces it to
   750 ms and `>=0.96` reduces it to 500 ms.
6. A configurable visible stop-reason announcement, followed by race stop and
   a configurable restart countdown. The defaults remain 10 and 15 seconds.
   The reason pages show incident evidence and location without a redundant
   `RACE STOP IN x SECONDS` calculation.

The player is excluded from automatic incident qualification. Player location
telemetry is a calibration view of the same spatial model used for AI wrecks.

## Vehicle-state model

- `RETIRED` is the game's RetiredFlag verdict and is presented before terminal
  classification.
- `TERMINAL` requires inactive, engine-off, crash-damage evidence and no
  official retirement verdict.
- `STALLED` is engine-off without crash-terminal evidence.
- PitState is rejected as stale when a car is travelling above 30 m/s.
- Post-restart state normalization repairs stale pit/status data without
  reactivating pre-red-flag retirees.
- Wheel-less terminal cars may qualify spatially even when wheel-contact
  pointers vanish.
- A `TERMINAL` car is not a completed retirement: it is the observed
  game-native state of an engine-off, severe-damage chassis with AI disabled
  but `RetiredFlag=0`. The viewer can identify that limbo state, not prove the
  missing game-side retirement writer. Its diagnostic limitations and release
  consequences are recorded in `RELEASE_READINESS.md`.

## Location and corridor model

The detector evaluates only active traffic corridors:

- AIW branch 0: main circuit.
- AIW branch 1: active pit entry, lane, exit and merge.
- Branches 2+ representing grid rows, pit boxes, garages or auxiliary points
  are excluded.

For each corridor it projects the vehicle onto linked AIW segments, reads local
left/right road widths, applies a 1.0 m vehicle half-width, and calculates
remaining pass space on both sides. A car footprint can occupy the road even
when its centre or the game's OffTrack byte lies just beyond the edge. Mixed
asphalt/grass wheel contact does not discard the road-side contacts.

Risk bands:

- Critical: inside the local critical line envelope or under 3.0 m best pass.
- Dangerous: inside the wider danger envelope or under 5.0 m best pass.
- Near: close to a corridor but not occupying a Dangerous/Critical position.
- Clear: outside the relevant corridor envelope.

The local envelopes are:

- `critical = clamp(road_width * 0.40 + 1.0, 4.5, 7.5) m`
- `dangerous = clamp(road_width * 0.70 + 1.0, 7.0, 12.0) m`

When main and pit corridors overlap, the higher risk wins; equal bands use the
closer active line. Unsupported tracks retain the collision-material fallback.

## Weather

The detector reads the game's physics wetness values:

- OnPathWetness: `0x008AFEE8`
- OffPathWetness: `0x008AFEEC`

Both are 0..1 and feed the game's grip calculations. `TrackWetnessFactor` at
`0x007CF3A4` is explicitly not used because it was observed at 0.50 in a dry
session.

For branch 0, live on-path wetness interpolates each segment between AIW dry
`wp_path[0]` and wet `wp_path[1]`. That weather-active line feeds the same
Dangerous/Critical verdict used by automatic activation. Branch 1 uses its pit
driving line. GUI categories follow the game's tire thresholds: Dry `<0.01`,
Intermediate `<0.33`, Wet `<0.66`, Monsoon `>=0.66`.

Weather is an incident amplifier, never an independent red-flag cause. It
does not turn front-wing damage, a moving damaged car, an engine-off stall, or
weather alone into an automatic red flag. At `>=0.90` it shortens only the
native-yellow and final stable confirmations after the normal verified
incident gate has passed. At `>=0.96`, one terminal plus one independently
stationary damaged car is additionally sufficient. The `1.00` weather trial
remains a human gameplay-validation gate; no weather-only automatic stop is
implemented.

## Restart lifecycle

- The red flag displays a reason for 10 seconds before `RACE STOPPED`.
- Vehicle, debris and parts cleanup is activated when the restart countdown
  reaches zero, not when it starts.
- Existing loose wheel/wing/body-part objects already carry their Parts
  Duration expiry from spawn time. The v9 cleanup trial clamps those expiries
  through an epoch/generation-bound game-thread task and keeps cleanup at zero
  until native dynamic-manager types 0..5 disappear or the existing bounded
  timeout expires. It never calls a native object destructor or list-removal
  routine directly.
- Native vehicle-removal deadlock repair handles incomplete wheel-less cleanup.
- First-lap restart compacts surviving qualification order to `1..N`; later
  restarts use current race order. Retirees receive no restart placement but
  remain in the complete classification roster.
- Retired-car garage carryover is cumulative across repeated restarts.
- Rejected v36 attempted a fail-closed pre-grid transaction: confirmed DNFs
  are marked `role=-1`, but `VehicleCount` and `VEHICLE_ARRAY` are never reduced,
  rebuilt or appended. Both validated restart call-site guards must install
  before `RESTART_FLAG`; they skip owned DNFs inside the restart constructor.
  Any mismatch or hook failure restores saved roles and refuses the restart.
- The intended guards remained installed through GRID_READY and the remaining restart
  lifecycle, then are removed after RacePhase 5 is continuously live for one
  second. DNF roles remain `-1` for the rest of that exact race, while the full
  original entrant view remains continuously available to results/scoring.
  Saved roles were to be restored only at the first owned next-grid/menu
  transition. Live evidence rejects this design: keeping role `-1` vehicles in
  the counted roster violates another restart initializer before GRID_READY.
  The next implementation must preserve DNF classification through a separate
  results/scoring path, not by exposing quarantined cars to the live restart
  roster. v38 therefore compacts the restart roster exactly as v33 did. After
  GRID_READY, one bounded task remains queued through the LightLoader-owned
  game-update broker until it enters live RacePhase 5. It then performs one
  ordered transaction: DNF classification,
  retirement and inactive fields first; saved controller roles second; saved
  complete pointer array third; original VehicleCount last. A failed validation
  retains the survivor-only safe fallback instead of expanding the roster.
- Pre-race TV commentary/grid walkthrough is suppressed for a red-flag restart;
  the player is presented with the garage/setup flow.

## Native flag icon

The game-owned red sprite (`0x40`) is used only before restart while a Red Flag sequence is
active. The DLL no longer changes the shared blue sprite (`0x41`) at load.
`show_red_flag_icon()` swaps to red and arms the native FLAG widget; both are
explicitly restored before `RESTART_FLAG`, and every abort, session teardown
and DLL unload also restores blue. Thus normal
blue-flag procedures remain blue outside a red flag.

## Telemetry and GUI

- Core monitor: `F1C_REDFLAG_MON`, 27 fields.
- Location/weather monitor: `F1C_REDFLAG_AIW_V4`, 184 bytes / 46 fields.
- GUI distinguishes Driving, Stopping, Stationary and Game Paused, and reads
  the same packaged INI values when rendering configurable thresholds.
- It exposes every activation gate, stop reason, field status, main/pit bands,
  dry/wet/weather-active lines, road widths, footprint overlap, pass space,
  surface contacts, wetness and sequence state.

## Exact current evidence

- `trials\2026-08-24\ACTIVE_SEVERE_OBSTRUCTION_V39_RC2_RECORD.md`
- `trials\2026-08-24\FULL_ROSTER_QUARANTINE_V36_RC2_RECORD.md`
- `trials\2026-08-24\STABLE_LIVE_CLASSIFICATION_V35_RC1_RECORD.md`
- `trials\2026-08-24\V33_RESTART_ROSTER_EXCLUSION_RECORD.md`
- `trials\2026-08-24\preserved-rejected-v32-live\RedFlag.log.v32-live-failure`
- `trials\2026-08-24\V31_RETIREMENT_CARRYOVER_RECORD.md`
- `trials\2026-08-24\V10_TEST_STAGE_V28_RC4_RECORD.md`
- `trials\2026-08-24\CONFIGURABLE_ACTIVATION_V28_RECORD.md`

- `trials\2026-08-19\WEATHER_MAIN_PIT_AUTOTRIAL_V23_RECORD.md`
- `trials\2026-08-19\WEATHER_INCIDENT_AMPLIFIER_V24_RECORD.md`
- `trials\2026-08-19\SCOPED_NATIVE_ICON_V25_RECORD.md`
- `trials\2026-08-19\STATIC_REVIEW_FIXES_V26_RECORD.md`
- `trials\2026-08-19\MAIN_PIT_CORRIDOR_AUTOTRIAL_V22_RECORD.md`
- `trials\2026-08-19\EDGE_OVERLAP_AIW_AUTOTRIAL_V21_RECORD.md`
- `trials\2026-08-19\TRACK_RELATIVE_AIW_AUTOTRIAL_V20_RECORD.md`

## Remaining validation

### BR-20260828-A0B0C6 guarded-Y trial (2026-08-31)

A NOT-FOR-DISTRIBUTION trial implements repeated `Y` restarts after a
DNF-bearing Red Flag through broker ABI 13. The broker's existing RestartGate
hook holds vanilla construction while Red Flag re-snapshots the field,
quarantines cumulative DNFs, and enters the existing restart/green transaction.
Ordinary pre-red-flag `Y` remains unguarded; failed preparation blocks the
restart. Static/build/broker regression evidence passes. The exact pair is
staged only in the clean VM test installation and loaded successfully, but the
owner gameplay acceptance run is pending. See
`trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\IMPLEMENTATION_AND_TEST_RECORD.md`.

The follow-up v4 trial adds `Restart.YRestartMode=PostRedFlag|FullRace` and
`Activation.RequireManualRedFlagAcceptance=0|1`. FullRace deliberately uses
vanilla `Y` and clears prior Red Flag bookkeeping; PostRedFlag retains the
guarded roster/lap transaction. Manual acceptance uses a release-owned fresh
`T` edge and revalidates live gates. Build plus 35 static contracts pass;
gameplay validation remains pending. See
`trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\CONFIGURABLE_Y_AND_MANUAL_ACCEPT_RECORD.md`.

- Prove default-value behavior parity, then change at least one timing and one
  pattern property and confirm that a game restart applies the exact INI.

- Observe a real dynamic transition through Intermediate/Wet/Monsoon and prove
  the active line moves with wetness.
- At `>=0.90` and `>=0.96`, stage representative incidents and prove that
  weather only accelerates a qualifying incident; prove that weather alone and
  a lone front-wing loss do not create a candidate.
- Stage a terminal AI obstruction on the main circuit and pit corridor and
  prove the automatic reason-4 gate through announcement, stop and restart.
- Reproduce an active wheel-loss limbo obstruction like Barrichello's: prove
  that Dangerous/Critical corridor occupancy triggers reason 4 after the
  configured confirmation, while a severe car off the corridor, in pits, or
  moving at/above 8 m/s does not.
- Exercise first-lap, later-lap and second-red-flag retirement carryover in one
  uninterrupted race.
- Validate representative tracks whose AIW authoring differs from A1-Ring.
- Complete the ship-readiness gates in `RELEASE_READINESS.md`; v26 remains a
  trial and must not be promoted or distributed before those human validations.

### Clean HUD and detector-guard v5 (2026-08-31)

The v5 trial removes the visible `RF:` prefix from automatic HUD pages and
replaces prefix-based cleanup with exact slot/text ownership. Its detector
audit also closes three side-effect gaps: `AutomaticEnabled=0` and
multiplayer/unknown sessions now stop before notices are created, and an
automatic notice must revalidate settled race, cooldown, configured yellow and
one of the two exceptional spatial proofs at the actual stop edge. Static
contracts and release build pass; gameplay validation remains pending.

The detector still uses a full-course “any yellow zone” signal rather than
proving that the yellow belongs to the same incident. Pit-lane spatial
telemetry is not currently an automatic Red Flag authorization. See
`trials\2026-08-31\BR-20260828-A0B0C6-y-restart-fix\HUD_PREFIX_AND_DETECTOR_AUDIT_RECORD.md`.

### Remaining community-feedback review and loose-parts cleanup v9 (2026-08-31)

Three delegated read-only investigations reviewed replay/UI compatibility,
debris cleanup, and lap/championship/detection feedback. Only the debris report
had an implementation-ready root cause. Native `sub_411330` bakes Parts
Duration into body expiry when loose collision types 3/4/5 spawn; the previous
restart-time option change could not expire those existing objects and restored
all cleanup settings based only on DNF garage completion plus 300 ms.

The v9 NOT-FOR-DISTRIBUTION trial posts an epoch/generation-bound game-thread
task that clamps those loose-part expiries to current native time, keeps the
normal Debris setting at zero, and observes the dynamic manager until loose
types 0..5 disappear or the existing 10-second bound expires. Descriptor/list
and body regions, native active-list indices and session ownership are checked;
vehicle type 6 is untouched, and no native destructor/removal function is
called. v6-v8 are preserved rejected intermediates; reviewer findings exposed
an executable-page validation error and then cross-generation publication
races before v9.

Replay/GPerso UI, naturally-lapped fields, championship quit/resume and newly
retired-wreck detection remain evidence/test work, not speculative code
changes. See
`trials\2026-08-31\remaining-feedback-remediation\COORDINATED_FEEDBACK_REMEDIATION_RECORD.md`.

### Live Instant Replay crash and roster-count diagnostic (2026-08-31)

Matias reproduced the community Instant Replay report after a completed Red
Flag restart. Replay rendered for several seconds before F1C terminated with
access violation `0xC0000005` at executable VA `0x0054C230`, the native
linked-list indexed accessor. The exact v2 installed Red Flag log proves its
pre-restart native replay-save request timed out and was cancelled before the
native finalizer executed. It also records restart construction with a reduced
22-to-19 survivor roster and ends at GRID_READY without a green/full-roster
result. This shifts the leading hypothesis to replay/scene objects constructed
for 19 vehicles and later traversed after the normal 22-entry results roster is
republished.

A v10 **NOT-FOR-DISTRIBUTION** diagnostic retains the survivor-only roster
after green while still restoring survivor laps. It intentionally produces
incomplete final results and exists only to discriminate the object-count
hypothesis. A matched v11 control keeps canonical full-results behavior. The
canonical source default is `RF_DIAGNOSTIC_KEEP_SURVIVOR_ROSTER=0`; 39 static
contracts pass. No replay fix has been accepted for release. Evidence and the
test matrix are in
`trials\2026-08-31\instant-replay-live-crash\INCIDENT_RECORD.md`.

### v34 live rejection: terminal player P22 / reduced 17/17 (2026-08-31)

The native-placement diagnostic is live-rejected. In the 22-car run, five AI
DNFs were removed from restart construction, leaving 16 surviving AI plus the
still-counted terminal player (`VehicleCount=17`). F1C's constructor revived
that player and placed it at P22. The diagnostic then failed closed and retained
the reduced roster, which made the native position formatter display `17/17`.

The exact log is preserved at
`trials\2026-08-31\player-dq-garage\RedFlag.v34.player-grid-p22-and-17of17.log`
with SHA-256
`8FF54D53792CDEE8C0D71B2C4D54BEABCDF247B3F7FC36002A8F9B8E05FB036A`.
No mod teleport caused P22. v34 must not be reused or distributed. Current
investigation is split between an exact-identity native grid-placement gate,
a HUD-only original-entrant denominator, and authoritative garage-placement
evidence. Global post-constructor `VehicleCount` expansion is not accepted as
a display fix because of the open Instant Replay crash.

### v36 P22 and 17/17 diagnostic (2026-08-31)

The exact-player grid gate, game-thread terminal tuple restore and HUD-only
original entrant denominator are combined in v36. Structural `VehicleCount`
remains reduced for replay/scene stability; only the native position formatter
reads the original 22-entry shadow. No player pose is written, so garage
placement is not claimed by this candidate. v36 passed 41/41 contracts, paired
macro builds and independent review, and is exact-hash staged in the closed
V10 clean profile. See
`trials\2026-08-31\player-dq-garage\v36-grid-gate-hud-denominator\V36_LIVE_TEST_RECORD.md`.

### v38 reduced-roster epoch correction (2026-08-31)

The first v36 live run aborted before restart after committing
`VehicleCount 22 -> 7`. The preserved log proves the new player guard compared
the deliberately reduced roster hash with the original 22-entrant hash, an
impossible invariant. v38 separates original-session/player-capture identity
from reduced-roster/grid-construction identity. Static tests pass 41/41 and the
exact diagnostic DLL is staged in the closed V10 clean profile with installed
SHA-256 `6B2E515F744D053B9A1B6CBF8A3CB8EA5D7D04AC16DD55611FCCD3E7C69A179E`.
Full evidence and the pending live gate are in
`trials/2026-08-31/player-dq-garage/v38-reduced-epoch-fix/V38_REDUCED_EPOCH_FIX_RECORD.md`.

### v40 player crash-terminal detection (2026-08-31)

The next v38 live run never armed automatic activation: its preserved log
repeatedly reported zero incident cars while a player crash/DQ was present.
The detector explicitly discarded `PLAYER_VEH_PTR` before terminal, damage and
spatial evaluation. v40 admits a player only through the existing strict
crash-terminal, pit, dwell, AIW geometry, racing-line and yellow gates; an
undamaged rules DQ remains insufficient. Static contracts pass 42/42. The
diagnostic is staged in the closed V10 clean profile with installed SHA-256
`91918DBE2820047B8F502846955ED5C29128F4615C0291597F2FA569D63D44DC`. See
`trials/2026-08-31/player-dq-garage/v40-player-terminal-detection/V40_PLAYER_TERMINAL_DETECTION_RECORD.md`.

### v42 native terminal-player garage placement (2026-08-31)

v40 live-proved activation and restart, but the grid-gated terminal player was
rebuilt upside down at F1C's `(0,2,-2)` fallback. v42 adds a diagnostic-only,
game-thread call through the game's AIW garage-record reader and synchronized
vehicle/player-rigid-body pose routine. Pointer/count/index/pose and post-apply
checks fail closed. Static contracts pass 43/43; the exact diagnostic is staged
in the closed V10 clean profile with installed SHA-256
`3C562087C4579813B44D2313550DC66C34A1D16E8DFE9BC218C153C0218812FB`. See
`trials/2026-08-31/player-dq-garage/v42-native-garage-placement/V42_NATIVE_GARAGE_PLACEMENT_RECORD.md`.

### v44 warning/freshness timing correction (2026-09-01)

A v42 live run proved `MULTI-CAR CRASH` was cancelled 10.015 seconds after it
started because both the incident freshness ceiling and pre-stop warning were
configured to ten seconds. An overturned blocker then began a separate warning.
v44 extends only the age ceiling of an already-authorized warning; recovery,
movement, pit, geometry, yellow and session gates remain live and fail closed.
Static contracts pass 44/44. The diagnostic is staged in the closed V10 clean
profile with installed SHA-256
`1B877D0A7D460442391A6B65AFC56D0B78DA35C66F7011699158688EF6D6B361`. See
`trials/2026-09-01/warning-freshness-v44/V44_WARNING_FRESHNESS_RECORD.md`.

### v46 fixed three-line warning HUD (2026-09-01)

The automatic warning presentation is now exactly three simultaneous owned
HUD lines: `RED FLAG WARNING`, one canonical reason, and `SECTOR N`. Rotating
car-count/diagnosis/location pages and the manual-T prompt page were removed;
the optional T acceptance behavior itself remains intact. Three independent
slots use exact owned-text checks before reuse or clearing. Static contracts
pass 44/44. The diagnostic candidate SHA-256 is
`155E9DBAD24DDC4E5C98026ABB572855D5ABA8E209899CD4721F6A68001F278E`;
it is built but not installed because the v44 live garage-placement test is
still running. See
`trials/2026-09-01/fixed-three-line-hud-v46/V46_FIXED_THREE_LINE_HUD_RECORD.md`.

### v48 terminal-player native-garage retirement lock (2026-09-01)

The completed v44 run live-proved native terminal-player placement at pit 9,
garage ordinal 0, and Matias visually confirmed the garage result. v48 keeps
the existing exact-player `SessionTickUpdate` guard after the stable green edge
so the terminal player cannot return to vehicle physics/control for the rest
of that session. It deliberately does not copy the pit-failure module's
documented race-prone `SessionType` flip or raise `PLAYER_OUT`; classification,
roster ownership and the proven native pose remain unchanged. Static contracts
pass 45/45. The diagnostic candidate SHA-256 is
`0BFB5D00C043179CA998FE157A5D1C7E778DB703B1D044FB810991AC3A200DB8`;
it is built but not installed while the v44 game process remains open. See
`trials/2026-09-01/terminal-player-garage-lock-v48/V48_TERMINAL_PLAYER_GARAGE_LOCK_RECORD.md`.

**Superseded:** v48 incorrectly carried the survivor-only replay diagnostic,
which would omit retirees from final classification. Do not install v48.

### v50 complete post-race classification (2026-09-01)

The installed v44 log proves the 11-car classification was caused by
`RF_DIAGNOSTIC_KEEP_SURVIVOR_ROSTER=1`: the green edge intentionally retained
the reduced 11-car construction roster. v50 turns that setting off while
retaining the fixed three-line HUD, 22-car denominator, exact-player grid gate,
native garage placement and terminal-player garage lock. The established
game-thread transaction now restores the full ordered entrant array and
publishes the original count last, so retirees return to results without
receiving restart grid slots. Static contracts pass 45/45. The v50 diagnostic
SHA-256 is
`8CA7AF671E1CB16FB113845107BBA2FB331BF472DA81A544A8007DFC314BDD64`.
It is exact-hash installed in the closed V10 clean profile. See
`trials/2026-09-01/full-classification-v50/V50_FULL_CLASSIFICATION_RECORD.md`.

### v54 community-feedback correction bundle (2026-09-01)

The next development trial adds `Activation.MinimumRaceDistancePercent`
(default 25, 0 disables only this gate) using the canonical confirmed SDK
`G_RaceSetupScaling` symbol. Automatic warning creation, automatic stop and
manual T acceptance all fail closed below the threshold. It also adds
`Restart.RestartProcedure=Countdown|DriveToPits`; the latter reuses the former
NUMPAD2 pit-return lifecycle and falls back to Countdown when a
terminal/disqualified player cannot drive to the pit box.

Fuel, driver-model visibility and each survivor's own lap value are now
captured per driver and restored through a required second game-thread broker
transaction after F1C's first native live update. This closes the ordering gap
which could replace restored fuel with the initial/full load and hide the
driver's arms. The wheel-loss-only two-second cleanup shortcut is removed; both
restart procedures wait the full bounded cleanup and repeatedly suppress every
excluded DNF through stable green. The full original results roster remains
mandatory (`RF_DIAGNOSTIC_KEEP_SURVIVOR_ROSTER=0`).

Static contracts pass 49/49. The v54 diagnostic DLL SHA-256 is
`9645B498638C035A5A9547809600D69DAE3A41209725E4E35F47AA3727CB04D3`.
After the game closed, it was exact-hash installed with its matching INI in
the V10 clean-test profile; displaced v50 was preserved. Live validation is
pending. See
`trials/2026-09-01/feedback-bundle-v54/V54_COMMUNITY_FEEDBACK_BUNDLE_RECORD.md`.

### v56 Alesi damaged blocked-line correction (2026-09-01)

The first v54 live activation attempt detected a passive road hazard but
reported `0 terminal / 0 stationary damaged / 0 cars` and did not trigger.
Matias identified Jean Alesi stopped in the middle of the racing line and
visibly missing a tire. Source review found that the active direct-line path
required `veh_heavy_damage_evidence()` even after strict speed, exact AIW line
overlap and continuous dwell had passed. That depends primarily on
`carConditions >= 2`, which F1C did not reliably expose for the visibly
wheel-damaged active chassis.

v56 uses the broader reviewed `veh_damage_evidence()` only within that already
narrow direct-line proof. A healthy stall, moving or off-line damaged car,
pit-lane car and missing-AIW case still fail closed; native-yellow, freshness,
race-distance, session and final-confirmation gates are unchanged. Static
contracts pass 49/49. The diagnostic artifact is 177664 bytes, SHA-256
`E543C6EE811D12E160077CAF8015392E1509BB967B6995F529ABCB8303CABCA7`.
It retains the v54 denominator/grid/garage diagnostics and keeps the
survivor-only roster diagnostic off. After the game closed, the exact v56 hash
was installed in the V10 clean-test profile; v54 and its INI were preserved.
Evidence and the pending live protocol:
`trials/2026-09-01/alesi-blocked-line-v56/V56_ALESI_BLOCKED_LINE_RECORD.md`.

### v58 stable debris-pool scan (2026-09-01)

The first v56 restart left detached wings and debris on track. Its exact log
proves the broker callback rejected DynMan's live active pointer list, published
manager validation failure, and restored the native cleanup options after only
about 375 ms without clamping existing part expiries. The option change cannot
expire objects whose spawn-time expiry was already baked by `sub_411330`.

v58 replaces the rejected active-list traversal with the stable native
descriptor pools for loose types 0..5 at DynMan dword pairs 9/10 through 24/25.
It validates each pool and active index, clamps types 3/4/5 on the game thread,
waits for loose types to disappear, leaves vehicle type 6 untouched, and calls
no game destructor. Existing generation/epoch ownership and the 10-second bound
remain. Static contracts pass 49/49. The diagnostic DLL SHA-256 is
`C5405538383AF10D8405F3D4779EF92AE751650386BE2362290DEB8DADD0A58C`.
After the game closed, that exact hash was installed in the V10 clean-test
profile; v56 and its matching INI were preserved. Evidence and live protocol:
`trials/2026-09-01/debris-pool-scan-v58/V58_DEBRIS_POOL_SCAN_RECORD.md`.

### v60 fail-soft loose-part cleanup (2026-09-01)

Live v58 was rejected: its stable-pool traversal still aborted on a strict
validation mismatch and restored cleanup values after about 407 ms. v60 scans
only the proven PartsDuration pools 3..5, treats `descriptor+0 == -1` as the
sole inactive sentinel, skips/logs malformed entries individually, and still
clamps valid parts. An unobservable pool is not reported as empty, so the
existing 10-second bound applies instead of an immediate restart. Native types
0..2 and vehicle type 6 are untouched; no destructor is called. Static
contracts pass 49/49. Diagnostic SHA-256:
`0CBE9665948050F232B4490241E00B660B222509507C12DF7C6C488D17BF7560`.
After the game closed, that exact hash was installed in the V10 clean-test
profile; v58 and its matching INI were preserved. Evidence and live protocol:
`trials/2026-09-01/debris-failsoft-v60/V60_DEBRIS_FAILSOFT_RECORD.md`.

### v62 DynMan embedded-object correction (2026-09-01)

Live v60 proved the callback still failed before entering any per-object logic.
The repeated root failure is now explained: native `0x005EA560` is the embedded
DynMan object, while v9-v60 dereferenced dword 0 (active count) and treated that
small integer as a pointer. v62 assigns the direct object address and retains
v60's bounded fail-soft pool validation. A static contract rejects the former
indirection. All 49 contracts pass. Diagnostic SHA-256:
`2F93508C0BA638E5DD99DF87EA0141345E9DC3DE79D2AB64B96FF6C0BBC3CC32`.
After the game closed, that exact hash was installed in the V10 clean-test
profile; v60 and its matching INI were preserved. Evidence and live protocol:
`trials/2026-09-01/dynman-object-v62/V62_DYNMAN_OBJECT_RECORD.md`.




