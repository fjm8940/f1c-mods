# Spain export review — 2026-09-23

## Scope and authority
Read-only review requested by Matias of a race reported to have three red flags. Product: Red Flag (paired Safety Car context); published product stage observe, this task investigate/learn. No implementation, installation, release, or stage change authorized or performed. Documentation destination: this component record, indexed in README.md.

## Evidence and method
The supplied nested Downloads path did not exist. The matching flattened filename below was found and inspected. Original file remains unchanged. Full-document Python JSON parsing fails at line 6997 column 22: serviceMessages opens an array, then emits the named positionHistory property without a valid enclosing object. Individual intact sections were recovered in memory using JSONDecoder.raw_decode at their named field offsets; this does not validate the complete document or repair it.

Source: %USERPROFILE%\Downloads\SINGLE_GRAN PREMIO MARLBORO DE ESPANA 2002_20260922_200211.json
SHA-256: d72ba672e9609e4f5e44abbbfe70125ecace332cd7973dae6ae2c3828536f0d7
Export metadata: 2026-09-22, exporterVersion 1.0, Catalunya 2002, 32 laps, 24 expected/loaded cars, replay mode false. Actual RF/SC binary versions are not supplied by this export and cannot be inferred from publication dates.

## Verified findings
- driverStandings contains 24 unique drivers and positions 1–24, six not marked DNF and 18 marked DNF. Top four: Darren Manning 32 laps / 3079.1326 s; Kimi Raikkonen 32 / +46.1718 s; Luca Badoer 32 / +58.9028 s; Nick Heidfeld 32 / +62.0714 s. Michael Schumacher has 31 laps; Sato 29. Sarrazin is P6 but DNF at 29 laps; Sato P7 is not DNF. This ordering alone does not establish a classification defect.
- The player Tester A is P15, DNF, five laps, raw total 118.738014 seconds. Six drivers have zero laps; five of those nevertheless have positive best laps. No historical retirement correctness can be established from this snapshot.
- There are no red-flag/restart/suspension text markers. keyEvents contains only a WINNER record. The owner's three-red-flag observation is accepted as reported context, not independently counted from the export. Trigger reasons, laps, durations and restart order cannot be reconstructed reliably.
- positionHistory contains four samples at 146.68001–149.77751 seconds then a sample at 1420082.8 seconds (394:28:02.750 formatted). That numeric timestamp is also exported as a driver: livePositionOrder has 25 entries with driver "1420082.8" at P1, shifting Manning to P2. This is a concrete export/data interpretation defect, not an extra physical car.
- Winner time differs: driverStandings 51:19.133 versus keyEvents description 47:23.415. sessionInfo.elapsedGameTime is 1:08:15.744; metadata.session.totalTime is 7215.372. These clock fields cannot be assumed equivalent. Eight adjustedTotalTimeRaw values are negative.
- All 24 drivers have lapOfRun=2. Fourteen lapChart arrays are empty (including Manning and Raikkonen); remaining histories contain only 1–4 entries, with partial-looking 31–36 second laps and repeated last-lap values. They are not a usable full-race lap history.
- fastestLap credits Tester A at 1:43.366 while Manning's standings bestLap is faster at 1:42.968. Aggregate fastest-lap data disagrees with driver snapshots.
- dnfList has only nine entries versus 18 DNF flags / retirementReasons entries. pitStops is empty despite four drivers each having pitStops=1 in standings. Missing lists must not be read as proof of no retirements or stops.
- Fuel snapshot: Manning 115.650566/135 L with zero recorded stops; Raikkonen 123.23923/135 L with zero recorded stops; player 150/150 L after five laps. fuelData has zero consumption samples. Values warrant checking preservation across restarts but cannot prove refuelling or its cause. Snapshot tyre values likewise cannot establish historical tyre preservation.
- Embedded gameResults is sourced from LOG/Results/raceresults.txt and contains TimeString=2026/09/21 20:46:28, with Tester A winning in 45:08.729. It belongs to a differently dated result and conflicts with this export; do not use it to validate this race.
- Weather history starts at 91.29, reaches 100, and later declines; summary reports rain during the race. Rain supports the reported difficult conditions, not the exact cause of any red flag.

## Interpretation and pending validation
The export is not trustworthy as a complete multi-restart race record. Partial history and inconsistent derived fields are compatible with restart/reinitialization issues in the exporter and/or runtime, but causality is unproven. Distinguish confirmed serialization/stale-result defects from suspected reset bookkeeping. This file cannot certify or reject successful RF/SC gameplay, fuel preservation, retirement retention or fairness of restart order.
Next useful evidence is the same-session RF/SC logs and snapshots immediately before/after each restart (identity, laps, cumulative time, retirement state, fuel, pit-plan cursor), tied to exact DLL hashes. Exporter work should validate JSON, reject stale result files, separate timestamps from driver arrays, and record explicit interruption segments while preserving cumulative race history. No code fixes were requested or made.

## Changes, preservation, validation and limitations
Only this report and a README index link are added. README before-state is preserved at %USERPROFILE%\Desktop\F1\workspace_organization_audit\EXPORT_REVIEW_BEFORE_2026-09-23_105546\development\mods\red_flag\README.md; restoration means restoring that complete before-state while preserving this report under workspace rules, never deleting material. Source SHA-256 verified unchanged. No game, source code, production artifact or PRODUCT_STATUS changes. No runtime tests needed for read-only review.
Workspace validator before review reported eight existing errors: unexpected .pytest_cache and trials; missing F1 Challenge V10_clean directory and canonical path; four source-like files in production bug_report_site v1.0.0/v1.0.1. These unrelated issues are not repaired in this review. Final validator result recorded after index update.
Documentation gate: report linked from component README; facts, inference and pending native validation separated.

## Wasted-effort check
- Repeated reading: initial batched output was too broad/truncated; narrowed to named component and JSON sections.
- Broad builds/tests: none.
- Failed/retried commands: supplied path absent; matching basename recovered. Full JSON parsing failed due to a genuine file defect; intact-section decoding used afterward.
- Polling/waiting/idle: none.
