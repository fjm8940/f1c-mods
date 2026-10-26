**Current state (short, maintained): [CURRENT_STATE.md](CURRENT_STATE.md).** Everything below is dated history, newest first.

**2026-10-08 Red Flag 1.1.6-rc.1 for Lightloader 2.2.3+ ():** the re-synced shipped 1.1.5 base plus the owner's U2 consent-gated per-session usage summary (`src/rf_usage.h` on the shared `f1c_usage.h`), the shared crash VEH installed after successful init and removed on unload, and the broker ABI15 session-end flush; no gameplay change. [Candidate, hashes and pending in-game test](releases/v1.1.6-rc.1/RELEASE_CANDIDATE.md); [record](docs/RF_1_1_6_2026-10-08.md).

**2026-09-29 Red Flag 1.1.5-rc.2 for Lightloader 2.0 ():** the live 1.1.4 source with one change: the broker is found by export scan, so Red Flag works with the broker built into Lightloader 2.0. [Candidate, hashes and pending in-game test](releases/v1.1.5-rc.2/RELEASE_CANDIDATE.md). rc.1 superseded.

**2026-09-29 Lightloader metadata candidate 1.1.4 ():** Matias approved option B. The exact accepted 1.1.3 DLL received only an embedded `F1C_MOD_META` resource; direct PE comparison proves `.text` and every pre-existing section unchanged. Metadata, INI comments and the restricted Lightloader ZIP pass offline checks. Exact-DLL in-game load, metadata-log and basic Red Flag validation remain pending. [Candidate evidence and hashes](releases/v1.1.4/RELEASE_CANDIDATE.md).

**2026-09-23 collector interoperability audit:** [RF restoration harnesses pass; collector paused-release baseline defect fixed in candidate 0.2.6](../race_data_collector/docs/RF_AUDIT_2026-09-23.md). RF gameplay code unchanged; native three-RF acceptance pending.

**2026-09-23 Spain export review:** [Three reported red flags: malformed JSON, stale native results, timing/history inconsistencies and validation limits](EXPORT_REVIEW_SPAIN_2026-09-23.md). Read-only investigation; no runtime or release change.

**Published2026-09-22: Red Flag1.1.3.** Owner-accepted exact gameplay build; versioned installer and matching broker verified. [Download](https://mods.palloset.com/redflag); [release, tests and rollback](../../../operations/release_management/red_flag/RELEASE_1_1_3.md). Prior candidate/install notes below are historical.

**Installed replay follow-up 20260922T122311Z:** SC 2F4ECAD0 / RF FCC9243F verified. [Installation and rollback](../full_safety_car/trials/v0.10.66_replay_live_field/install-20260922T122311Z/README.md). Fresh-session physics/retirement acceptance pending.

**2026-09-22 player/Panis follow-up:** [SC-side instant-replay live-field repair](../full_safety_car/trials/v0.10.66_replay_live_field/README.md). RF FCC9243F remains unchanged. Candidate prevents historical replay reconciliation from rebuilding live instances; native overlap/retirement retest pending.

**2026-09-22 native acceptance failure:** [Player physically overlaps Panis after two RF restarts and replay; five DNFs lose retirement state](trials/2026-09-22/replay-panis/README.md). Live evidence preserved. Exact replay physics cause/fix pending; not release-ready.

**Installed 20260922T114515Z:** [Verified RF fix and backups](trials/2026-09-22/second-rf-restart/install-20260922T114515Z/README.md). Native acceptance pending; supersedes earlier not-installed status.

**2026-09-22 second RF handover fix ready:** [Live cause, exact candidate and verification](trials/2026-09-22/second-rf-restart/README.md). Deployed SC was captured as an entrant then falsely swept as a DNF when parked. Snapshot now excludes the shared SC identity. Hardened FCC9243F… verified, not installed; native acceptance pending.

**Installed 20260922T112332Z:** [Verified RF CFB7B247 / SC505E55B5 pair, backups and acceptance](../full_safety_car/trials/v0.10.64_replay_ownership/install-20260922T112332Z/README.md). Documents/F1 Challenge V10; private trial, native acceptance pending.

**2026-09-22 paired takeover candidates verified:** [SC replay transaction guard plus retained RF Massa green-roster correction](../full_safety_car/trials/v0.10.64_replay_ownership/VERIFICATION.md). Hardened RF CFB7B247… / protected SC505E55B5…; not installed. Canonical/hardened runtime regressions and SDK gate pass; native acceptance pending.

**2026-09-22 replay roster guard r4 (Codex R7-R9):** [production-order lifecycle fix, paused-replay protection, history/intervals evidence](trials/2026-09-22/replay-roster-guard/RESPONSE_TO_CODEX_R7_R9.md). RF review `392BCD8B...`/hardened `1947A1B2...`; SC private `5AFFBA13...`/protected `CD57AA58...`. R7 automatic-rewind re-add remains an explicit blocker. Not installed.

**2026-09-22 replay roster guard revision 3 (Codex R6-R8):** [TwoPathDispatch hook with real trampoline tests, replay-history ordering, first-live-tick restore](trials/2026-09-22/replay-roster-guard/RESPONSE_TO_CODEX_R6_R8.md). RF review `F0179ABF...` / hardened `8A165849...`; SC private `8606A9F4...` / protected `5AE982D7...`. Not installed.

**2026-09-22 replay roster guard revision 2 (Codex R1-R5):** [game-thread capture, watchdog integration, identity contract, native visibility helper; SC-side replay detour](trials/2026-09-22/replay-roster-guard/RESPONSE_TO_CODEX_R1_R5.md). RF review `509D50BD...` / hardened `77BB590C...`; SC private `D3F03CBA...` / protected `93617FF0...`. Not installed; Codex review requested.

**2026-09-22 roster transaction fix verified:** old source reproduces Massa's green-restore failure; new callback commits full roster only after survivor restores succeed. Hardened candidate 4ADE75A1… awaiting closure/install/native retest. [Evidence and Alonso/Montoya collision diagnosis](trials/2026-09-22/massa-stuck-r4/IMPLEMENTATION.md).

**2026-09-22 r4 gameplay failure: Massa stuck after restart.** Fuel passes owner test; live state/log expose a partial-roster transaction abort before pit-strategy restore. [Evidence and repair handoff](trials/2026-09-22/massa-stuck-r4/README.md). Stop acceptance pending correction; earlier ready-to-test review is superseded by this native failure.

**2026-09-22 r4 independent review: ready for in-game acceptance.** Remaining fallback ownership defect resolved; focused canonical and hardened tests independently pass. [Review and minimum gameplay tests](trials/2026-09-22/ai-pit-strategy-preservation/reviewer-r4/REVIEW.md). Exact hardened hash starts 2A8EF5FB. Not installed; not release-approved. Earlier review notices below concern superseded candidates.

**2026-09-22 r3 independent review: changes requested.** Timeout fallback clears RF parking ownership and lets the strategy refresh lose a legitimate future stop. [Reproduction and required correction](trials/2026-09-22/ai-pit-strategy-preservation/reviewer-r3/REVIEW.md). No installation. Earlier review notices below refer to prior candidates.

**2026-09-22 pit-strategy candidate review: changes requested.** Buffer bound, capture after forced parking, pending pit requests and game-thread timing need correction. [Review and reproductions](trials/2026-09-22/ai-pit-strategy-preservation/reviewer/REVIEW.md). No installation.

**2026-09-22 replay roster guard candidate:** [root cause, SC/replay determination, regressions, hashes and acceptance checklist](trials/2026-09-22/replay-roster-guard/README.md). Keeps the preserved roster-transaction correction; adds a game-thread replay-exit guard that restores counted membership and the native control/visibility fields for exactly the captured live roster after native replay destroys the parked/excluded SC and shrinks VehicleCount. Not installed; Codex review requested.

**2026-09-22 roster transaction correction built (uninstalled):** [Massa freeze investigation, correction and old/new regression](trials/2026-09-22/massa-stuck-r4/IMPLEMENTATION.md). Fallible survivor restores now run against the consistent reduced roster before the full roster/count commit.

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

**2026-09-21 continued implementation — not yet a game-test handoff:**
[Broker-owned penalty hold, optional full/latest fuel, announcement cancellation and verification](trials/2026-09-21/policy-v89/completion-work/penalty-hold/README.md).
Requires the paired new broker. Review artifacts only; not installed. Repair/tyre
preservation, race conclusion, ultimate recovery and protected packaging remain.

**2026-09-21 completion pass — full test handoff still unavailable:** [Implemented weather hold, SC handoff fault, native fuel mirror and exact-code guards; passing tests and remaining native work](trials/2026-09-21/policy-v89/completion-work/README.md). Review build only, not installed. Independent service, post-announcement penalties, race conclusion and ultimate recovery remain unfinished.

**2026-09-21 RF89 / SC0.10.63 core candidates verified; native features incomplete:** [accepted owner decisions, changed behavior, tests and remaining native gates](trials/2026-09-21/policy-v89/README.md). Core policy/runtime tests pass. Independent service options, safe race conclusion and ultimate under-map recovery remain unfinished; native gameplay and protected-build gates pending. Not installed or distributable. This entry supersedes the earlier awaiting-answers note.

**2026-09-21 fixes authorized; behavior decisions first:** [24 owner questions, additional policy gaps, extracted-code evidence and repair scope](trials/2026-09-21/behavior-questions/README.md). Awaiting requested behavior answers before runtime changes.

**2026-09-21 current-source review:** [RF88 r2 retirement boundary, SC ownership/cancellation reproduction, configurable-key gap and VPS customer reports](trials/2026-09-21/sc-rf-review/README.md). Investigation only; runtime unchanged, native causes/validation remain open.

**Published2026-09-15: Red Flag1.1.2 / RF87.** Exact owner-accepted DLL shipped as installer. [Download](https://mods.palloset.com/redflag); [verification, limits and restoration](../../../operations/release_management/red_flag/RELEASE_1_1_2.md). Safety Car remains paid with PC activation. Earlier pending/install notes below are historical.

**RF87 / SC0.10.56 installed (2026-09-15,18:54 local):** [separate Safety Car fault request and verified physical handoff](trials/2026-09-15/sc-fault-v87/README.md). Private hardened RF87; settings/public releases unchanged. Native recovery and redeployment validation pending; earlier installed-version notes below are historical.

**RF86 / SC0.10.54 installed (2026-09-15,18:22 local):** [player excluded from automatic incidents; verified build and pending native validation](trials/2026-09-15/player-exclusion-v86/README.md). RF85 replay preparation and manual controls retained; settings/public unchanged. Earlier installation notes below are historical.

**RF85 installed for private testing (2026-09-15,18:04 local):** [nonblocking replay preparation, retained field hold, exact installation and pending gameplay](trials/2026-09-15/replay-prepare-v85/README.md). SC0.10.53 retained. Regression and SDK gates pass; four legacy static failures unchanged. Native SC-to-RF restart remains to be validated. Earlier installed-version notes below are historical.

**SC53 handoff / RF84 replay-stage abort (2026-09-15):** [SC physically parked; queued replay save times out, then context check aborts before native restart](trials/2026-09-15/sc53-replay-abort/README.md). Owner denies pause/Esc; exact failed predicate unlogged. Full RF restart remains unvalidated.

**RF84 installed for private testing (2026-09-15,17:20 local):** [exact DLL, verified backups and pending gameplay checks](trials/2026-09-15/player-preflight-v84/installation-172057/README.md). Prevents unsupported terminal-player RF restart before roster changes. Fresh launch required; invisible-collision cause remains unconfirmed. Earlier installation notes below are historical.

**Sato whole-structure audit (2026-09-15):** [25 scene nodes,72 render records,14 part records and all spatial lists](trials/2026-09-15/retired-object-graph/README.md). Old visual/history coordinates remain; main collision centre moved to the garage and detached-part collision centres moved outside the track. This successful restart does not establish the invisible-collision cause.

**Retired coordinate audit (2026-09-15):** [all three excluded retirees, full Raikkonen vehicle bytes, 15 nearby position copies and collision-coverage limits](trials/2026-09-15/retired-coordinate-audit/README.md). No distant stale position found in checked copies; separate collision objects unverified. RF84 prevention remains uninstalled.

**Wrong-way DQ reproducer (2026-09-15):** [21/24 becomes21/21 after three AI retirees remain excluded by player-state abort](trials/2026-09-15/dq-21-of-21/README.md). Publicv1.1.1 still loaded; owner confirms disqualification trigger.

**RF84 preventive candidate (2026-09-15):** [terminal-player preflight, reproduced regression and validation limits](trials/2026-09-15/player-preflight-v84/README.md). Rejects unsupported player restarts before native roster construction. Built, not installed; full terminal-player restart support remains pending.

**Second restart aborted (2026-09-15):** [player-state proof failure, commentary restoration and retained 17-car roster](trials/2026-09-15/second-restart-abort/README.md). Public v1.1.1 native failure captured; invisible collision not established.

**Local CTD during reproduction setup (2026-09-15):** [parked Safety Car reload, exact bad scene-link read, preserved minidump](trials/2026-09-15/local-crash-161449/README.md). Matches known customer reload-crash pattern; occurred before any logged RF sequence. Corrupting writer unproven.

**Second customer log (2026-09-15):** [Magny-Cours restart, 23/22 vehicle mismatch, unavailable checkpoint and later field reset](trials/2026-09-15/customer-second-log/README.md). Collision cause and customer DLL identity remain unconfirmed.

**Public v1.1.1 installed for reproduction (2026-09-15):** [live download, exact RF hashes, RF83 backups and pending 2002 test](trials/2026-09-15/public-reproduction/README.md). Newer shared broker retained; game not launched. Earlier RF83 installation entries below are historical.

**Customer invisible collision triage (2026-09-15):** [decoded log, limits and required evidence](docs/CUSTOMER_INVISIBLE_COLLISION_2026-09-15.md). Cause unconfirmed; supplied excerpt contains no RF restart event.

**RF83 installed for beta validation:** [consistent HUD, protected build and paired package](trials/2026-09-14/messages-v83/README.md). Public1.1.1 unchanged; final gameplay/member activation pending.

**HUD messaging inventory:** [all current candidate textbox templates, source lines and truncation findings](HUD_TEXT_INVENTORY_2026-09-14.md). Read-only wording audit; replacements pending.

**RF v82 current candidate:** [configurable normal removal timers and slow-car geometry, build/tests/native limits](trials/2026-09-14/configurable-removal-v82/README.md). Not installed; public v1.1.1 unchanged.

**RF v81 candidate:** [read-only removal policy query for SC](trials/2026-09-14/removal-v81/README.md). Built, not installed/published.

**v0.10.38 / RF v80 installed for testing:** [verified installation and backups](../full_safety_car/trials/v0.10.38_beta_controls/INSTALLATION_2026-09-14.md). Native gameplay validation pending; not published. Earlier preparation entries below are historical.

**v80 beta candidate, not installed:** [corrected inverted-car orientation and shared geometry/diagnostics](trials/2026-09-14/orientation-v80/README.md). Public v1.1.1 unchanged.

**SC v0.10.34r2 / RF v79r2 installed — corpus fixes:** [handoff rearm, stall coverage, observation clocks, geometry seams and evidence](trials/2026-09-10/corpus-v79/README.md). Automated gates pass; fresh native validation and exact footprint research remain. Public releases unchanged.

**2026-09-10 corpus audit: remaining coverage/recovery gaps:** [five reproduced policy counterexamples, native evidence and whole-cycle scenario matrix](../full_safety_car/trials/corpus_adversarial_audit_20260910/README.md). SC/RF exclusivity does not prove complete coverage; no runtime change.

**SCv0.10.33r2 / RFv78r2 installed:** [coordinated ownership, acknowledged restoration, passable-incident coverage and verification](trials/2026-09-10/coordinator-v78-r2/README.md). Automated gates pass; fresh native coexistence/cancellation testing pending. Public releases unchanged.

**Safety Car coexistence audit:** [manual countdown, blocker veto and handoff risks](../full_safety_car/trials/coexistence_audit_20260910/README.md). Investigation only; published RF unchanged.

**v77 installed for testing:** [stale-warning fix installed and hash verified](trials/2026-09-10/stale-notice-v77/INSTALLATION_2026-09-10.md). Fresh gameplay validation pending.

**v77 private candidate:** [stale RF warnings now clear without T, releasing the warning veto on Safety Car](trials/2026-09-10/stale-notice-v77/README.md). Tests pass; installation awaits game closure.

# F1 Challenge Red Flag

> **Published v1.1.1 hotfix (2026-09-08).** Owner-authorized shipment of the exact installed protected v76 DLL. T requires the game foreground; manual confirmation and accepted automatic offers now have a cancellable activation timer, configurable with `Timing.ManualActivationCountdownSeconds=10`. [Download](https://mods.palloset.com/redflag), [player README](releases/v1.1.1/package/readme.txt), [changelog](CHANGELOG.md), [publication verification](../../../operations/release_management/red_flag/RELEASE_1_1_1.md). Public ZIP hash and completed delivery verified; analytics discovered the release automatically. Detailed fresh-game/HUD test results were not supplied. Earlier candidate and publication notes below are historical.

> **Installed v76 protected candidate (2026-09-08, 10:03 local).** Foreground-only T and the cancellable 10-second activation timer are installed in Documents/F1 Challenge V10. Previous DLL/INI preserved; all existing INI settings retained. Exact installed hash verified; fresh-game testing is pending. [Installation and test steps](trials/2026-09-08/manual-countdown-v76/INSTALLATION_2026-09-08.md). Public release unchanged.

> **Latest candidate: v76 cancellable manual countdown (2026-09-08).** Adds `Timing.ManualActivationCountdownSeconds=10` after manual confirmation or T acceptance of a detector offer, with `RED FLAG IN X SECONDS` / `PRESS T TO CANCEL`. Includes v75 foreground filtering. Protected/native verification passes; installation still waits for game closure. [Record](trials/2026-09-08/manual-countdown-v76/README.md).

> **Foreground-only T fix, v75 candidate (2026-09-08):** manual initiation and automatic-offer acceptance require the game process to own the foreground window. Returning with T held cannot trigger either action. Protected build and native/protected regressions pass; installation waits for the running game to close. [Candidate and test record](trials/2026-09-08/foreground-t-v75/README.md). Published v1.1.0-r5 is unchanged.

> Published v1.1.0 r5: player-facing protection details and privacy-notice link removed at owner request. Runtime and INI unchanged. [Record](../../../operations/release_management/red_flag/RELEASE_1_1_0_R5.md).

> **Changelog audited and published (2026-09-07):** [Source audit](trials/2026-09-07/changelog-audit/AUDIT.md). Public v1.1.0 revision r4 includes the corrected changelog and README; runtime/INI unchanged from r3. [Release record](../../../operations/release_management/red_flag/RELEASE_1_1_0_R4.md).

> **Published protected v1.1.0 revision 3 (2026-09-07).** Exact owner-accepted hardened DLL, garage fix and automatic-offer T acceptance enabled. [Release record](../../../operations/release_management/red_flag/RELEASE_1_1_0_R3.md). Download: https://mods.palloset.com/redflag. Earlier candidate notes below are historical.

> **IP hardening candidate (2026-09-07):** ten Ultra-protected routines, fused loader validation/initialization and 181 private diagnostic event codes. Protected garage/checkpoint harnesses pass; exact fresh-game validation remains pending. [Record](trials/2026-09-07/ip-hardening-v74/README.md). Not installed or uploaded.


> **Installed testing candidate: v73 garage pose fix (2026-09-07).** Stops writes to shared AIW garage heights and restores owned AI retirees through native GarPos/GarOri application. Installed for next launch; fresh game required; visual validation pending. See [v73 record](trials/2026-09-07/dnf-garage-v73/README.md). Public v1.1.0 remains unchanged.

## Published v1.1.0 â€” 2026-09-07

Owner-approved v72 RC2 is now published as v1.1.0. Download: https://mods.palloset.com/redflag. Includes steady icon, configurable manual/automatic triggers, restart methods, Y checkpoints, plain-T filtering and Esc cancellation.

See [changelog](CHANGELOG.md), [end-user README](releases/v1.1.0/package/readme.txt), and [publication verification](../../../operations/release_management/red_flag/RELEASE_1_1_0.md). The exact runtime remains installed in `%USERPROFILE%\Documents\F1 Challenge V10`.

## Historical development notes

The notes below describe earlier candidates and their gates; the publication record above is current.

> **Latest testing build: v72 RC2 Esc cancellation (2026-09-07).** Esc cancels
> the active countdown/pit-return sequence. Player exit/results also cancel;
> replay completion and the existing game-thread restart gate revalidate the
> request. Includes v71 plain-T fix. See
> `trials/2026-09-07/escape-cancel-v72/README.md` and its installation record.

> **Latest testing build: v71 plain-T input fix (2026-09-07).** Both manual
> initiation and detector approval ignore Ctrl/Alt/Shift/Windows + T. Raw T
> state stays tracked so releasing a modifier while holding T cannot trigger
> either action. See `trials/2026-09-07/plain-t-v71/README.md`.

> **Latest testing candidate (2026-09-07): v70 RC2 manual control/checkpoints.**
> Adds independent two-press T initiation through `ManualEnabled`, the requested
> confirmation text, and a saved latest-Red-Flag restart checkpoint for Y.
> FullRace restores the original qualifying grid before vanilla reset. Retains
> v69 icon and v68 HUD bridge. 51 static contracts and isolated native policy,
> checkpoint and renderer tests pass; gameplay validation and shipping remain
> pending. See `trials/2026-09-07/manual-checkpoint-v70/README.md`.

> **Previous testing build (2026-09-07): v69 steady icon.** Retains v68 HUD
> compatibility and replaces the competing native-flag writes with a scoped,
> exact-byte-checked visibility override in both existing HUD renderers.
> Native blue-flag behavior is restored at restart/abort. See
> `trials/2026-09-07/icon-flicker/V69_IMPLEMENTATION.md` for verification and
> installation evidence. Visual gameplay validation remains pending.

> **Current authority (2026-08-24):** read `CURRENT_STATUS.md` first. It
> consolidates the automatic trigger, vehicle states, main/pit AIW coverage,
> weather-aware line selection, restart lifecycle, GUI/telemetry ABI, static
> review-fixes v26 state, exact evidence and remaining validation. Sections below preserve the
> original port/session baseline and are superseded where they conflict with
> `CURRENT_STATUS.md`.

> **Release preparation:** `RELEASE_READINESS.md` is the authoritative
> ship-readiness contract. The current DLL remains a development trial and is
> not approved for distribution.

> **Latest development trial (2026-09-01):** v68 retains v66's isolated
> debris/control/replay broker clients and adds exact-version F1 Live HUD
> v0.6.0.1 compatibility. Red Flag publishes the original entrant count over
> a named read-only consumer ABI while F1C owns its temporary survivor-only
> restart roster. The HUD adapter redirects only its expected-car-count read;
> absent/invalid ABI data falls back to vanilla behavior. Static contracts,
> adapter bytecode inspection, named-mapping runtime/fallback probes and
> reproducible builds pass. Guarded in-game HUD/restart/results validation is
> still required. See
> `trials\2026-09-01\f1-live-hud-compat-v68\V68_F1_LIVE_HUD_COMPATIBILITY_RECORD.md`.

## Development port

This directory is the canonical development workspace for the Red Flag
restart/grid-reorder/DNF-handling implementation, ported on 2026-08-17 from
the preserved SDK legacy evidence at
`%USERPROFILE%\Desktop\F1\f1c_ida_decompiled\mods\red_flag`.

The original evidence remains intact and is not a development location.

## Current source

- Edit `src\gr44.c`.
- `generated\redflag.c` is generated from `src\gr44.c`; do not edit it.
- `tools\make_release.py` is preserved build-generation provenance, not an
  approved development build entry point. Use the SDK front door before adding
  or changing build support.
- `redflag.ini` is the reviewed player-facing automatic-activation template;
  `docs\CONFIGURATION.md` documents every key and safety boundary.

The canonical source remains manual-only by default. A separately named,
opt-in automatic trial is preserved under `trials\2026-08-17`; it is **not for
distribution** and still requires a guarded full-race incident test. The
archived `redflag.dll` was deliberately not copied.

## Session summary (read this first)

`SESSION_2026-08-17.md` â€” the full writeup of the 2026-08-17 session: the
automatic-activation work, the field reviewer + Python GUI, the deep damage
model, three crash classes found and fixed, the anti-removal hold, and the
open items. Start there, then this file, then `trials\2026-08-17\`.

The DLL no longer creates an in-game monitor window. It publishes the detector
state through `F1C_REDFLAG_MON`, and `tools\field_gui.py` is the only GUI. The
Python window shows every driver's live status, the incident score, cluster
size, terminal count, yellow gate, cooldown, and sequence state;
`tools\field_reviewer.py` is its read-only data module. Exact build and load
evidence is in `trials\2026-08-19\PYTHON_GUI_ONLY_BUILD_RECORD.md`.

The current viewer presents all automatic-trigger gates as a numbered,
high-contrast checklist with one large next-action/blocker banner. It includes
pause, multiplayer, opening-window thresholds, and final-confirmation progress;
see `trials\2026-08-19\QUICK_GUI_BUILD_RECORD.md` for the exact build and visual
verification.

The default red-flag action remains a 15-second countdown restart. Its duration
is player-editable through `Timing.RestartCountdownSeconds` in `redflag.ini`.

## Automatic race-control classification (v60 development trial)

The current v60 trial is compiled with automatic support and reads
`redflag.ini` from beside the DLL. Players can tune timing, locality,
classification thresholds, weather assists, restart countdown, post-restart
cooldown and severe-pattern counts/toggles, or set `AutomaticEnabled=0` for
manual-only operation. Restart the game after editing. See
`docs\CONFIGURATION.md`.

Two race-control choices are also configurable. `Restart.YRestartMode` selects
`PostRedFlag` (pre-stop retirees remain retired and survivor laps continue) or
`FullRace` (vanilla `Y`, resetting laps and retirees). Setting
`Activation.RequireManualRedFlagAcceptance=1` latches a qualified automatic
Red Flag as a HUD prompt and requires a fresh `T` press; the live incident
gates are checked again at acceptance. The default is automatic acceptance.
Automatic warning pages use plain HUD text without an `RF:` prefix. Slot
ownership is tracked by the exact slot and last text written, so cleanup cannot
erase an unrelated native or third-party message. Disabled automatic mode and
multiplayer/unknown sessions are rejected before detector HUD side effects, and
automatic mode revalidates the mutable gates when the announcement expires
before it is allowed to stop the race.

The automatic detector (`check_auto_redflag`) uses the reviewed INI defaults
below. A player may tune the numeric/property values, but the immutable safety
gates still have to pass:

1. The race is **settled** â€” evidence-verified value sets, NOT docs 49:
   `GameState 5 or 7` AND `RacePhase 3 or 5`, excluding RacePhase 2 (grid)
   and 8 (done). A live yellow-flag race reads 5/5 (sample-stable).
   Stale kill samples are dropped on the edge into a settled race.
2. By default, F1C's native yellow has been active for 3 s, read WITHOUT calling the game:
   `native_yellow_read()` (pure memory reads of the steward zone table) â€”
   `sub_49BAD0` itself HANGS when called from a foreign thread.
3. Automatic Red Flag accepts either one sustained effectively-stationary car
   whose footprint directly overlaps the active weather racing line on AIW
   branch 0, or the configured number of terminal wrecks close together on
   active branch-0 road geometry. The default pileup proof is two terminal cars
   within 40 m; grass/gravel pileups and unrelated DNFs do not count. The
   direct-line proof remains speed below 1.0 m/s for active cars and line-centre
   distance no greater than the 1.0 m car half-width plus a 0.5 m tolerance.
   A terminal wreck uses a separate bounded 3 m/s physics-jitter allowance, and
   exact line overlap remains authoritative if the broader AIW road polygon
   disagrees. Missing AIW data fails closed.
4. Mixed and strong-wet clusters can write a passive placeholder to
   `RedFlag.log` when neither spatial proof is present. A sustained
   singleton on usable main-track asphalt can do the same. These verdicts take
   no game action and never invoke a Safety Car. A healthy recoverable active
   stall cannot Red Flag even on the line. A damaged active car can qualify
   only while continuously below the strict speed cap and directly overlapping
   the active racing line for the configured confirmation interval; this does
   not depend on F1C promoting the damage to `carConditions >= 2`.
5. The incident and yellow gates remain valid for a final 1 s confirmation and
   are checked again at the actual stop/`T` edge after the announcement. The
   opening 30 s use a guarded 200 m / 1 s-yellow / 2 s-incident exception.
6. The sequence is idle, multiplayer is rejected, and the 2-minute cooldown
   (also armed by manual triggers) is clear.

The build still uses `AUTO_REDFLAG_ENABLED=1` as the compile-time capability
gate. `Activation.AutomaticEnabled` is the player-facing runtime switch. The
v60 trial remains until guarded full-race, segmented-replay playback and clean-install
human validation passes. See
`trials\2026-08-24\TERMINAL_LINE_AUTHORITY_V43_RC1_RECORD.md`.

The 2026-08-31 coordinated community-feedback pass identified one
implementation-ready cleanup defect. Existing loose parts retain a native
expiry computed when they spawn, so changing Parts Duration only at restart is
too late. The v9 development trial clamps loose type 3/4/5 expiries through a
session-owned game-thread broker task and waits, bounded, for native loose
types 0..5 to leave the dynamic manager before restoring cleanup settings.
Replay/UI compatibility, naturally-lapped fields, championship resume and
fresh-retirement detector behavior remain explicit evidence gates.

## Loader and replay boundaries

Ordinary DLL/ASI loading executes only inert `DllMain`. Initialization requires
the exact compatible LightLoader image, private ABI/version record, current
process identity, nonce/seal/proof callback and LightLoader-owned broker. A
different loaderâ€”or a modified/older LightLoaderâ€”cannot start the worker.

Red Flag does not implement a recorder. Immediately before each restart, v45
asks LightLoader's game-thread broker to invoke F1C's native replay finalizer
while the race is still saveable, then moves that closed vanilla file to a
unique timestamped Part N. `Game_Restart` creates the next vanilla segment.
The failed v44 filename hook is removed. No-Red-Flag races remain vanilla and
live creation/playback of every part remains a release gate.

## Source provenance

See `SOURCE_MANIFEST.md` for the exact origin paths and SHA-256 hashes.
The legacy implementation contains historical raw-address code and must be
adapted to reviewed SDK/broker interfaces before it can become a release
candidate.

Latest orientation review: [2026-09-07 review](docs/LATEST_REVIEW_2026-09-07.md) â€” artifact hashes confirmed; gameplay validation remains pending.







