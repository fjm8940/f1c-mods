# Output review — 2026-09-25

Objective: inspect the latest installed collector build and its produced output
files without modifying gameplay, exports, or installation state.
Scope: read-only review of the installed DLL/config/log and the newest RaceData
exports and recovery journals. No game launch, no import, no promotion.
Pipeline stage: product remains `verify` / candidate; no stage change.

## Installed artifact

- Installed DLL: `%USERPROFILE%\Documents\F1 Challenge V10\plugins\lightloader\race_data_collector.dll`,
  SHA256 `56980226509de6fc07f9ac0b70a3c02b9d96ec5e47e86140a4059112b01b6323`.
  This equals the tested 0.2.6 candidate from
  `trials/build_20260923_111054_150/` (installation record
  `trials/build_20260923_111054_150/installation_20260923_111544/`).
- INI: `race_data_collector.ini`, `[Collector] Enabled=1`, sample interval 100 ms,
  worker checkpoint 30 s, read-only, no Java/F1CUI process required.
- Log: `race_data_collector.log` is active. Latest entries (2026-09-25
  14:41:15Z /  record `started v0.2.6; waiting for telemetry broker`
  and startup, with no capture, incomplete-export, or final-export lines after.
  No new race output exists since 2026-09-23.

## Output locations

- Final race summaries: `%USERPROFILE%\Documents\F1 Challenge V10\RaceData\*.json`.
- Recovery: `...\RaceData\recovery\` — one `.jsonl` event journal per capture plus
  periodically rewritten `*.incomplete.json` snapshots (1612 files at review time).
- The game-tree path `%USERPROFILE%\Desktop\F1\F1 Challenge V10\RaceData` is a
  junction to the Documents `RaceData` directory (verified `Target` attribute);
  there is one physical store, and both views show identical hashes.

## Latest export (newest final output)

File: `SINGLE_Monza, 2002_20260923_202210-28496-1_20260923_202210.json`
(429,532 bytes, SHA256 `36a0c0e4881e3c810b23bb2c9a8fc29cc7ca5d2c36fb87620316023614723b23`,
exported 2026-09-23, local 23:24:55).

- Producer: `Race Data Collector 0.2.6`, schema `f1c.race-data/2.0`.
- Session: SINGLE, Monza 2002 (Italy, Monza), 50% race length, 26 scheduled laps.
- Capture `20260923_202210-28496-1`: interval 100 ms, 268 samples, 62 sampling
  gaps over one second, `completeHistory: false`, `closeReason:
  all_observed_cars_terminal`, `classificationIncludesNativeFinalization: true`,
  `nativeValidation: pending`, `requiresClassificationReview: false`,
  `lastObservedRaceTimeRaw: 143.5`, issues:
  `native_finalization_not_observed_race_history`, `sampling_gap_over_one_second`.
- Journal `recovery\20260923_202210-28496-1.jsonl` (103,241 bytes, SHA256
  `c4d80ed17a0a6299d50f1a979a856d5cdd767d0c20cae01a9593d0000ccbd175`, 275 events):
  96 tyre samples, 39 position-order events, 27 race-control observations,
  24 contact events, 24 driver-seen, 17 pit-state changes, 14 car-condition
  changes, 14 lap completions, 7 incomplete pit visits, 7 weather samples,
  3 retirements, 1 fastest lap, 1 native-finalization start.
- Timeline: player `Matias` retired at game time 143.5 s (lap 0); native
  finalization started immediately after; retirements observed earlier for
  Jirko Malcharek (72.7 s) and Darren Manning (75.2 s). Pit visits are marked
  incomplete across the finalization boundary (last journal event).
- `gameResults`: 24 drivers, `final: true`, `official: false`,
  `source: native_observed_classification`; 18 finishers on 26 laps; 5 DNFs
  (Tester A, Nick Heidfeld, Darren Manning, Jirko Malcharek, Matias).
  Fastest lap: Michael Schumacher, 1:43.211, lap 1. `pitStops: []` (none
  observed). 41 summary sections present; F1CUI-owned sections (radio, radar,
  VSC, deleted laps, etc.) remain empty per documented coverage limits.

## Recent output history and limitations

- Three final Monza summaries were written on 2026-09-23 (17:30, 19:20, 20:22
  local capture starts); earlier 2026-09-21/22/23 Imola exports and the
  2026-09-16 exports (Imola, Interlagos, Sepang, Albert Park) remain in RaceData.
- The newest classification is native finalization after the player retired at
  lap 0, not an observed full race history; the capture covers only the first
  143.5 s with sampling gaps. It is evidence of installation/runtime output, not
  native acceptance of the pending acceptance matrix.
- `requiresClassificationReview: false` alongside `completeHistory: false` is
  recorded as an observation for review; no behavior change is proposed here.
- Facts above are direct file inspection. Session interpretation (e.g., why the
  race ended) is inferred from the export/journal and is not independently
  confirmed; no human gameplay validation is claimed.

## Preservation and documentation gate

- Before-state of the edited component README:
  `workspace_organization_audit\RACE_DATA_OUTPUT_REVIEW_BEFORE_20260925\README.md`.
- No collector, game, production, installed-Narrative, or RaceData file was
  modified, moved, or deleted. No import was performed.
- `governance\PRODUCT_STATUS.json` was not updated: no stage, maturity, path,
  gate, or blocker change resulted from this inspection.
- Workspace validator after the README edit: 11 errors, 0 warnings. Eight match
  the documented 2026-09-23 baseline (`.pytest_cache`, `trials`, missing
  `F1 Challenge V10_clean` directory/path, four production Python files). Three
  are pre-existing and unrelated to this inspection: unexpected top-level
  `f1 challenge testing versions` and two additional production Python files
  (`production\lightloader\telemetry-service-v1-20260923\telemetry_report.py`,
  `telemetry_service.py`). No structural repair attempted.
- Follow-ups unchanged: guarded full-weekend three-RF native acceptance,
  classification review policy, and Narrative human validation.
