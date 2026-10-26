**Career data schema 2.1, producer 0.3.0 (2026-10-20, dev build, offline-verified, not installed):** [implementation record](docs/CAREER_DATA_IMPLEMENTATION_2026-10-20.md), [additive field contract](docs/SCHEMA_2_1.md) and [owner live checklist](docs/LIVE_TESTS_CAREER_DATA_2026-10-20.md). DLL SHA-256 `57ACADFED490862673F9356CC7A8E91DC44A988CB855F3926A6397A687049D86` (scratch build, not deployed). Lightloader career reader still requires 2.0 until updated; Python consumer test not run.

**British GP legacy export repair (2026-10-12):** [syntax repair and manual-results conversion](docs/JSON_PARSE_REPAIR_2026-10-12.md). Original preserved; corrected JSON parses; importer-ready manual-results copy passes local domain validation, with lap history omitted.

**Nurburgring manual results (2026-10-11):** [screenshot-based final order, 21 finishers and 3 unclassified](docs/NURBURGRING_RESULTS_EDIT_2026-10-11.md). Verified upload copy; owner import pending.

**Gilles Villeneuve Sheets import repair (2026-10-11):** [verified upload copy and preserved native results](docs/GILLES_IMPORT_REPAIR_2026-10-11.md). Owner upload pending.

**Google Sheets importer (2026-09-27, built, offline-verified):** [Apps Script that imports RaceData JSON into per-race result sheets and auto-sorted driver/constructor standings; editing a Pos cell recalculates points and standings](tools/google_sheets_importer/README.md). Scoring matches the hosted manager on 56/56 accepted exports; real Google Sheets run and human validation pending. Before-state: `workspace_organization_audit\SHEETS_IMPORTER_BEFORE_20260927`.

**Race Data editor and red-flag repair tool (2026-09-25, built):** [tkinter editor, strict-JSON save model, consumer checks and red-flag repair wizard](tools/race_data_editor/README.md). Implementation complete; verification in [VERIFICATION.md](tools/race_data_editor/VERIFICATION.md); rule-profile evidence and limits in [docs/RF_REPAIR.md](tools/race_data_editor/docs/RF_REPAIR.md). Definition only beforehand; no collector or Season Narrative code was changed.

**Race Data editor implementation plan (2026-09-25, current handoff):** [Resolved red-flag decisions D1–D6, Monaco expected result, work packages and tests](docs/IMPLEMENTATION_PLAN_RACE_DATA_EDITOR_2026-09-25.md). Definition only. Default profile era_2001_2004; Monaco lap-0 retirements are evidence-supported (genuine); manager half points deferred to optional WP7.

**Editor + red-flag repair prompt rev 3 (2026-09-25, requirements):** [Editor plus "race stopped" and red-flag restart repair](docs/EDITOR_RF_REPAIR_PROMPT_2026-09-25.md). Definition only; Monaco restart capture and 8 RaceData continuation captures are the fixtures; open owner decisions listed at the end.

**Editor tool prompt review (2026-09-25):** [Verification of the editor prompt against the corpus and both importers](docs/EDITOR_TOOL_PROMPT_REVIEW_2026-09-25.md). Read-only; hosted manager reads `gameResults.drivers`, local Season Narrative reads `driverStandings`; 54/62 exports pass the manager importer.

**Editor tool prompt rev 2 (2026-09-25):** [Handoff-ready implementation prompt for a tkinter race-data editor](docs/EDITOR_TOOL_PROMPT_2026-09-25.md). Definition only; no tool built, no export edited.

**Output review (2026-09-25):** [Installed 0.2.6, output locations, latest Monza export/journal inspection](docs/OUTPUT_REVIEW_2026-09-25.md). Read-only; no new race output since 2026-09-23; native acceptance unchanged.

**0.2.6 installed (2026-09-23):** [Exact candidate hash, backups and restoration](trials/build_20260923_111054_150/installation_20260923_111544/README.md). Documents game; Enabled=1 retained; native three-red-flag validation pending. Earlier not-installed notes are historical.

**0.2.6 candidate verified (2026-09-23):** [RF restart baseline fix, three-interruption regression, RF code audit and native acceptance limits](docs/RF_AUDIT_2026-09-23.md). Built, not installed; complete cross-segment classification still requires review.

**Imola 2002 classification correction (2026-09-20):** [Owner-directed edit of one export copy — Marcel Lasee classified 5th, Tester A 6th and every following finisher one position lower](docs/IMOLA_CLASSIFICATION_CORRECTION_2026-09-20.md). Lap/timing/live-order observations untouched; the file records the change in `metadata.classificationCorrection`. Not copied into RaceData and not imported anywhere.

**0.2.5 installed (2026-09-15):** [All collector executable hash/layout/checksum rejection checks removed at owner request](docs/NO_EXE_GATES_2026-09-15.md). Wrong-checksum and arbitrary-host actual DLL tests pass; data tests pass. Native customer capture pending. Supersedes0.2.4 layout restriction.

**Multiplayer status (2026-09-20):** [assessment and required validation](docs/MULTIPLAYER_ASSESSMENT_2026-09-20.md). No multiplayer capture exists yet; MP identity, AI entrants and classification are unverified.

**0.2.4 installed (2026-09-15):** [Removed exact EXE hash restriction; verified compatible-layout startup, tests, installation and friend DLL](docs/COMPATIBILITY_2026-09-15.md). SDK layout/broker checks retained. Customer native validation pending. Earlier version entries are historical.

# F1 Challenge Race Data Collector

- [Independent LLM review brief for the community collector (2026-10-02)](docs/COMMUNITY_COLLECTOR_INDEPENDENT_REVIEW_BRIEF_2026-10-02.md) — owner objective, evidence ledger, alternatives, quantitative questions and an open decision framework for a fresh reviewer.

- [Community research collector feasibility and design (2026-10-02)](docs/COMMUNITY_RESEARCH_COLLECTOR_DESIGN_2026-10-02.md) — distributed, bounded memory observations, campaign comparison, existing collector overlap and staged acceptance; proposal only, no runtime changes.

**Current standalone consumer — Championship Universe:** [launch and workflow](../../season_narrative/README.md), [verification and limitations](../../season_narrative/STATUS.md). Implemented outside the 2001 installation; 14 tests and an isolated Silverstone desktop-agent/article/finalization demonstration pass. Human validation remains pending. Earlier design milestones below retain their historical status.

**Season-independent workflow and every-driver continuity:** [original limitations and six-step workflow contract](docs/NARRATIVE_WORKFLOW_2026-09-15.md). Historical specification; current implementation is linked above.

**Wikipedia-style article reference and workflow:** [visual target, shorter article structure and renderer specification](docs/WIKIPEDIA_REFERENCE_SPEC_2026-09-15.md). Current renderer and visual verification are documented in the standalone consumer.

**0.2.2 installed, 2026-09-15:** [installation verification and fictional Wikipedia/Season Narrative readiness assessment](docs/ARTICLE_READINESS_2026-09-15.md). Includes recommended facts/canon separation, article profile and deterministic enrichment. Earlier awaiting-exit notes are historical.

**0.2.2 fixes ready:** [finalization-clock isolation, bounded tyre telemetry, actual Silverstone replay and preserved historical repair](docs/SILVERSTONE_FIXES_2026-09-14.md). Installation awaits normal game exit; native validation pending.

**Latest native Silverstone review:** [results, successful SC exclusion, incomplete history, sentinel-clock pit bug and excessive tyre events](docs/SILVERSTONE_REVIEW_2026-09-14_2240.md). Read-only evidence; fixes remain pending.

**0.2.1 installed — readable JSON:** [formatting change, existing-export preservation and validation](docs/FORMATTING_2026-09-14.md). Summaries/checkpoints use two-space indentation; JSONL journals keep one event per line.

**0.2.0 installed — Safety Car / Red Flag integration:** [interfaces, output, verification, exact installed hash and remaining native gates](docs/RACE_CONTROL_2026-09-14.md). Adds raceControl observations, identified-SC exclusion, RF finalization protection and linked restart segments. Earlier candidate links below are preserved milestones.

Standalone native Lightloader DLL for Season Narrative, replacing the F1CUI Java exporter. Version 0.1.0 in development;.

Canonical project: development/mods/race_data_collector. Pipeline: investigate -> specify -> implement -> verify; native and human validation pending. Live consumer: %USERPROFILE%/Documents/F1 Challenge V10/Custom/2001/Season Narrative. Installed game/JAR and consumer remain untouched during development.

**Installation update, 2026-09-14:** Matias authorized useful improvements and installation. The verified DLL and enabled INI are now in Documents/F1 Challenge V10/plugins/lightloader. The normal Season Narrative gui.py now loads the installed compatibility bridge; no separate launcher is needed. Earlier untouched/not-installed statements below describe the original development milestone. See [installation evidence and restoration](docs/INSTALLATION_2026-09-14.md). Actual race capture remains pending.

## Entry points

- [Definition and implementation contract](docs/PLAN.md)
- [Multiplayer assessment (2026-09-20)](docs/MULTIPLAYER_ASSESSMENT_2026-09-20.md)
- [Collector and consumer audit](docs/AUDIT.md)
- [Data coverage](docs/DATA_COVERAGE.md)
- [Validation status](STATUS.md)

## Build and use

From the F1 workspace run `powershell -NoProfile -ExecutionPolicy Bypass -File development\mods\race_data_collector\build.ps1`. Requires the existing C:/msys64/mingw32 compiler and Python. Each build creates a new preserved trials/build_* directory with the DLL, tests, logs, input hashes and PE import/export inventory.

Current tested development candidate: [race_data_collector.dll](trials/build_20260914_215428_487/race_data_collector.dll), SHA256 `fd1892d211ec0cc3bb907b945c2ee2e8e33b175beba458d9b1172b97ad48b18d`. It is x86 and has only Windows system CRT/Kernel/Advapi imports; no Java or MinGW runtime DLL dependency. It requires the existing native broker (ABI 15 capabilities), which remains a runtime dependency.

For a separately authorized native trial, place the candidate and [INI](config/race_data_collector.ini) in the configured Lightloader plugin directory with the existing broker. Do not replace another plugin. Start collection before the race; the collector writes completed JSON to game-root RaceData and recovery material under RaceData/recovery. It refuses unsupported executables. Read race_data_collector.log alongside the DLL for activation/failure evidence. Disabling Collector/Enabled in its INI takes effect on the next game start. No installation has been performed by this task.

The existing Narrative importer reads the summary JSON. The optional [bridge launcher](tools/season_narrative_bridge.py) additionally rejects incomplete/corrupt native exports and preserves capture/neutral-contact/weather evidence without editing the installed application:

```powershell
python -B "%USERPROFILE%\Desktop\F1\development\mods\race_data_collector\tools\season_narrative_bridge.py" --narrative "%USERPROFILE%\Documents\F1 Challenge V10\Custom\2001\Season Narrative"
```

Add `--check "C:\path\to\race.json"` to parse a file without launching the GUI or writing season data. The normal GUI Analyze action still has its normal season-writing behavior. Do not import both Java and native exports of the same race into the season. Nothing disables the Java exporter automatically.

[Journal inspection](tools/inspect_journal.py) accepts a journal path and `--output` pointing to a new evidence JSON file. It never changes the journal and never invents a final classification. New files and trial outputs are preserved; existing maintained documentation is backed up before edits.

- [Friend distribution and actual lap-history audit](docs/FRIEND_READINESS_2026-09-15.md): exact installed build verified; complete Brazil/Imola lap sequences, incomplete sublap coverage and deployment prerequisites.

- [0.2.3 RF restart preservation candidate](docs/RF_PRESERVATION_2026-09-15.md): immutable segment snapshots, failed-write retry, lap-restoration guards and restart bundles. Verified build ready; installation waits for normal game closure.
