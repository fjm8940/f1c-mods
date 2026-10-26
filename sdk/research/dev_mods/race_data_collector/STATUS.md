**0.3.0 dev test install (2026-10-20, live validation pending):** DLL SHA-256 AC42CADD5786FA90FF5C310A77291E12F7DDADDA14B81EBE5A70FC0B68B35E49 and INI (Enabled=1) installed to Game Versions\F1 Challenge V10\plugins\lightloader\ (neither existed before). Record: [installation README](trials/build_20261020_171451_808/installation_20261020_171718/README.md). Consumer gate verify_outputs.py NOT APPLICABLE on this PC (old Season Narrative parser absent); Career rdc21 tests 353/353. Owner live Career round pending (RaceData folder must appear). Before-copy STATUS.md.BEFORE_20261020_install030.

**0.3.0 career data candidate (2026-10-20, offline-verified, NOT installed):** schema `f1c.race-data/2.1` additive fields for the career story (derived sectors, lap-end gaps, contact pairing, retirement detail, pit tyres, fuel per lap, planned stops, endReason). Core suite and DLL guard pass; Python consumer test not run; live validation pending. See [implementation record](docs/CAREER_DATA_IMPLEMENTATION_2026-10-20.md) and [live checklist](docs/LIVE_TESTS_CAREER_DATA_2026-10-20.md). Before-copies `*.BEFORE_20261020_careerdata`.

**0.2.6 installed (2026-09-23):** [Exact candidate hash, backups and restoration](trials/build_20260923_111054_150/installation_20260923_111544/README.md). Documents game; Enabled=1 retained; native three-red-flag validation pending. Earlier not-installed notes are historical.

**0.2.6 candidate verified (2026-09-23):** [RF restart baseline fix, three-interruption regression, RF code audit and native acceptance limits](docs/RF_AUDIT_2026-09-23.md). Built, not installed; complete cross-segment classification still requires review.

**0.2.5 installed (2026-09-15):** [All collector executable hash/layout/checksum rejection checks removed at owner request](docs/NO_EXE_GATES_2026-09-15.md). Wrong-checksum and arbitrary-host actual DLL tests pass; data tests pass. Native customer capture pending. Supersedes0.2.4 layout restriction.

**0.2.4 installed (2026-09-15):** [Removed exact EXE hash restriction; verified compatible-layout startup, tests, installation and friend DLL](docs/COMPATIBILITY_2026-09-15.md). SDK layout/broker checks retained. Customer native validation pending. Earlier version entries are historical.

# Status

## Current: 0.2.2 installed — article-generation goal recorded

2026-09-15: normal game exit verified; exact tested0.2.2 DLL and bridge installed, installed Narrative test passes. [Installation and article-readiness assessment](docs/ARTICLE_READINESS_2026-09-15.md). Native gameplay validation remains pending; detailed encyclopedic generation needs deterministic race dossier, separate factual/fictional canon and article templates. Earlier installation-blocker entries are historical.

## 0.2.2 verified; installation waiting for game exit

[Silverstone fixes](docs/SILVERSTONE_FIXES_2026-09-14.md): DLL `edfc4d7bbc0b3f4a037fbda80a5f0fe4de2eecb35a74792ea050d03c829e1e5e`, component and actual recorded tyre replay pass;8957→92 samples. Historical export repaired with unchanged classification and preserved original. Active game prevents DLL replacement; native validation pending.

## Current: 0.2.1 readable JSON installed

2026-09-14: [Formatting evidence](docs/FORMATTING_2026-09-14.md). Installed SHA256 `0c4f2928698f9c794d876763083bc905692f7fc011c1e34cbeec56a9237bdc88`. Component and installed Narrative tests pass. Existing native Silverstone export indented with equal parsed data and complete before-state preserved. Gameplay validation limitations remain unchanged.

## Current: 0.2.0 mod integration installed

2026-09-14: [Safety Car / Red Flag integration record](docs/RACE_CONTROL_2026-09-14.md). Installed DLL SHA256 `fee924aada1e412c4db5afa855f97047f249987e678f1518e792525fa09ec21d`; component, actual read-only mapping, installed Narrative report and full SDK gates pass. SC/RF mod binaries/settings unchanged. Native activation/gameplay and complete RF cross-segment reconstruction remain pending; legacy phase-source attribution is explicitly limited. Earlier sections describe preserved milestones.

## Current installation — 2026-09-14

Owner authorized installation and useful changes. Exact candidate hash below installed and Enabled=1 under Documents/F1 Challenge V10/plugins/lightloader. Normal Season Narrative GUI integrates the bridge automatically. Added native duplicate-key/non-finite JSON refusal. Installed GUI parsing and full isolated report generation pass; binary unchanged, original core/DLL/SDK results still apply. No game process was running; runtime activation and real-race acceptance remain pending. [Installation record](docs/INSTALLATION_2026-09-14.md) includes manifest, backups and restoration. Earlier sections record the pre-installation milestone.

2026-09-14: standalone native collector implemented; development candidate in verify. No installation or production promotion. Full semantic parity with F1CUI-owned radio/radar/VSC/DRS/deleted-lap policy is not claimed; see [coverage](docs/DATA_COVERAGE.md).

## Exact artifact and evidence

- Candidate: [build_20260914_215428_487](trials/build_20260914_215428_487/), DLL SHA256 `fd1892d211ec0cc3bb907b945c2ee2e8e33b175beba458d9b1172b97ad48b18d`.
- Target: Documents/F1 Challenge V10/F1 Challenge Online.exe, SHA256 `1a70e5ed0c4a059f7f66741b1682d0aedb14921aa6140cdb70b09549fdf3a003`.
- [Core test log](trials/build_20260914_215428_487/core-test.log): structural serialization, bounds, identity/lifecycle, missing laps, trailing finish, same-sequence session-end refresh, missing unfinished car, DSQ, capped history with monotonic journal sequences, write collision preservation, metadata path confinement pass.
- [Consumer test log](trials/build_20260914_215428_487/consumer-test.log): strict JSON including empty arrays and nonfinite handling, all 41 sections, actual installed parser and full report writer in isolated trial directories, bridge evidence and incomplete-file refusal, partial-journal inspection pass. These are synthetic races; the installed season was not modified.
- [DLL guard](trials/build_20260914_215428_487/dll-guard.log): Windows x86 LoadLibrary, exported init/stop and unsupported-host refusal pass. [PE inventory](trials/build_20260914_215428_487/pe-imports-exports.txt) confirms exports/system imports.
- [Full SDK gate](evidence/sdk-gate-20260914-2150.log): `python tools/f1c_mod.py sdk-test --exe` with the actual target from canonical repository root exited 0, including new pure-buffer observation tests. This does not prove advancing-game semantics.
- Compiler warnings remain in logs (GCC std::variant maybe-uninitialized diagnostics and formatting warnings); compilation succeeds. They are not silently treated as a warning-free build.
- [Build input manifest](trials/build_20260914_215428_487/build-inputs.json) identifies source and SDK inputs. [SDK review](../../../f1c_ida_decompiled/tools/sdk/RACE_DATA_OBSERVATION_REVIEW.md) documents provisional fields (path from workspace: f1c_ida_decompiled/tools/sdk/RACE_DATA_OBSERVATION_REVIEW.md).

## Native acceptance matrix — all pending

1. Supported executable/broker: start before formation, confirm advancing capture clocks/sequences and measured callback/frame overhead with a full grid.
2. Complete race: validate classification after every trailing car, DNF/DSQ, player retirement and native accelerated results/exit. Compare authoritative native results and manually observed laps.
3. Restart/session transition/replay/pause: separate capture identity, no contaminated histories, incomplete export kept out of imports.
4. Pit/fuel/tyres: compare lane and service times, drive-through/stop-go, refuelling/compound changes and raw units against gameplay. Supplemental offsets and condition meanings remain provisional until this passes.
5. Weather and metadata: wet/dry transitions, correct AIW/GDB/VEH/RCD identity, non-ASCII names and championship mode. Unknown round/provider remains a documented limitation.
6. Failure paths: denied output directory, disk-full/partial write, abrupt process end and clean stop; verify preserved journal/checkpoints and successful retry without duplicates.
7. Matias validates Narrative output, installation/usability and standalone operation without F1CUI collecting concurrently. No public release until exact-hash promotion authority is given.

## Changes, preservation and restoration

New component source/tests/tools/config/docs/trials; additive SDK observation header/review/test and test-runner/README/checkpoint entries; component index and product-status entry. No game, JAR, production, existing RaceData or installed Narrative files edited. Existing documents preserved under workspace_organization_audit/RACE_DATA_*_BEFORE_*; SDK generated artifacts preserved before its gate. Failed and superseded trials retained. No moves or deletions occurred. Restoration, if needed, uses those complete before-states; stop/disable a future installed collector before changing its placement, with no unload while callbacks remain registered.

The root workspace validator remains at the pre-existing 10-error baseline; see [corrected final validation log](evidence/workspace-validator-final-corrected.log). The first final run also caught a newly added nextGate spelling error (human validate versus human_validate); that entry was corrected with before-state preservation and the original 11-error log retained. No global structural repair was attempted. Documentation is indexed through the component README and development/mods/README.md.

Documentation gate passed: all README/status/docs links resolve; the component appears in development/mods/README.md; facts, inference, limitations and pending native validation are recorded. The build-input manifest was rehashed after documentation work and all source/config/test/SDK input hashes still match the verified build. SDK preservation comparison found only the intentional checkpoint entry and regenerated f1c_sdk.json _meta changing; no other preserved SDK artifacts changed. [Comparison](evidence/sdk-gate-changed-artifacts.json) includes both gate generation and this task's checkpoint update, not just test side effects.

## Wasted-effort check

- Repeated large reads: bytecode was narrowed to relevant methods after the initial inventory; no additional material waste recorded.
- Broad tests: one full canonical SDK gate required by the SDK contract; subsequent component builds followed substantive changes.
- Avoidable failed commands: first SDK gate used the workspace rather than SDK repository cwd; first PowerShell native build treated compiler stderr as a terminating error; initial consumer report test exposed a real null-formatting incompatibility, which was corrected. All evidence preserved.
- Polling/idle: bounded waits while implementation/documentation continued; no material idle loop.

2026-09-15 actual capture audit supersedes blanket no-native-evidence wording: 0.2.2 installed binary equals tested build and all build inputs match. Brazil/Imola have complete numbered lap sequences; Argentina misses two driver-laps; accelerated captures have sublap sampling gaps. Friend compatibility and full native acceptance remain unverified. [Evidence](docs/FRIEND_READINESS_2026-09-15.md). No release promotion or game changes.

## 0.2.3 restart-preservation candidate verified
2026-09-15: candidate SHA256 3b79e90bb258dee5c562c7bdf443293b7db4acab648f5214c055687188ebe2d8. Core, x86 guard, consumer and full canonical SDK gates pass. Restart fixtures are refused by championship normalization; normal Brazil remains accepted. Previous segment snapshots retained and retried across failed writes; restored lap counters do not become race laps. Game PID 13504 still runs old0.2.2, so candidate is not installed or promoted. Native RF/Y/SC validation pending. [Specification and evidence](docs/RF_PRESERVATION_2026-09-15.md).
