# Spain 2002 export JSON repair - 2026-10-09

Input: `%USERPROFILE%\Downloads\SINGLE_GRAN PREMIO MARLBORO DE ESPANA 2002_20260922_200211.json` (1,775,275 bytes, unchanged).
Championship manager rejected it: `Expected ',' or ']' after array element ... line 6997 column 22`.

## Findings (verified)

- Four empty arrays were written as an opening `[` with no closing `]`; the next key followed directly. First case: `serviceMessages` (line 6995) runs into `positionHistory`; `newFastestLapAlerts` (line 7160) runs into `lapPositions`.
- File lacked the final root `}`.
- Header says `exporterVersion: "1.0"`. Current `src/collector.cpp` builds the document as a JSON object, and that code path was not shown to produce this defect. Inference: the defect comes from the legacy 1.0 exporter. Not confirmed.

## Repair

Output: `...\Downloads\SINGLE_GRAN PREMIO MARLBORO DE ESPANA 2002_20260922_200211_FIXED.json`.
Regex `("key":\s*)[(\s*)(?="key":)` replaced by `[],` (4 hits), then the missing closer was appended. Python `json.load` passes; 32 root sections are present (metadata through driverOfTheDay). No data values changed.

## Pending

- Owner uploads the FIXED file to the championship manager and confirms the standings import.
- If more 1.0-exporter files fail, apply the same repair or find the legacy writer.

## Follow-up: manager rejected the repaired file

Upload error: `Not a Race Data Collector v2 export (schemaVersion undefined)`. The manager (`development/season_narrative/domain.py` normalize) accepts only `f1c.race-data/2.0`, `f1c.manual-results/1.0` or `f1c.synthetic-reconstruction/1.0`. The legacy 1.0 file has no schemaVersion.

The fix was a new converter, `development/season_narrative/reconstructed/convert_legacy_v1_export.py`. It produces `f1c.manual-results/1.0` with provenance and copies classification, laps, grid, best laps, finisher totals and retirement codes unchanged. Output: `reconstructed/2002_Spain_legacy_v1_manual.json`, also copied to Downloads.
The local `domain.normalize` accepts it: 24 drivers, 32 laps, 6 finishers in the top 7 (Michael Schumacher P5 on 31 laps, Sato P7 on 29 laps).

Limitations: no lap history imported (`completeHistory: false`). Retirement code 0 maps to unspecified. The round number is left to the manager UI. The local domain.py was not compared against the VPS copy.
Correction: the error message comes from the Google Sheets importer (`tools/google_sheets_importer/Core.gs` normalize), not the VPS manager. Both use the same schema gate. The converted file also passes Core.gs normalize under node: 24 drivers.
Pending: owner uploads `2002_Spain_legacy_v1_manual.json` and confirms.

## Monaco 2002 red-flag file (same session)

Sheets importer error: `Restart/red-flag continuation segment: needs a combined classification`. Repaired with the existing editor tool and no code changes:
`race_data_editor.py --rf-repair <Downloads Monaco file> --mode combined --decisions fixtures/decisions/monaco_combined_era_2001_2004.json --out Downloads\repaired`.
The plan was `restart_new_race` (leader had 1 lap at the red flag, so the first part is void under the era_2001_2004 owner default). Full points.
Output is `Downloads\repaired\SINGLE_Monaco, 2002_..._144455.json` plus an `.rf-manifest.json`. The original is untouched.
Core.gs normalize accepts it: 24 drivers, Panis P1 and Sarrazin P2 as the only finishers (39 laps), with TesterA P3 classified as Ret on 27 laps.
Pending: owner uploads it and confirms the result looks right.
