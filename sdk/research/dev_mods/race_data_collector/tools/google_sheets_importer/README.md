# Race Data -> Google Sheets importer (2026-09-27)

 — development tool; human validation in a real Google Sheet pending.

Google Apps Script bound to a spreadsheet. Imports Race Data Collector exports
(`Documents\F1 Challenge V10\RaceData\*.json`, schema `f1c.race-data/2.0`), writes a
result sheet per race, and keeps Driver and Constructor championship tables sorted.
Manual position corrections recalculate points and standings automatically.

## Files

| File | Role |
|---|---|
| `Code.gs` | Menu, import, race-sheet rendering, `onEdit` corrections, standings sheets |
| `Core.gs` | Pure validation/scoring/standings logic (also run by the Node tests) |
| `Extract.html` | Browser-side field extraction (exports reach ~7 MB; only needed fields are sent) |
| `ImportDialog.html` | File-picker dialog |
| `appsscript.json` | Manifest (V8; scopes: current spreadsheet + dialog UI only) |
| `tests/run_tests.js` | Offline tests with an in-memory fake of the Sheets API |
| `tests/manager_points.py` | Reference points from the hosted manager (`development\season_narrative\domain.py`) |
| `evidence/` | Test runs and file hashes |

## Install (once per spreadsheet)

1. Create a Google Sheet, then **Extensions > Apps Script**.
2. Rename the default `Code.gs` content to this `Code.gs`. Add script files `Core`
   (paste `Core.gs`) and HTML files `Extract` and `ImportDialog` (paste the `.html` files;
   names without extension must match exactly).
3. **Project Settings > Show "appsscript.json"**, replace it with this `appsscript.json`.
4. Save, reload the spreadsheet. A **Race Data** menu appears; the first use asks for authorization.

## Use

- **Race Data > Import race JSON...** — pick one or more exports. They are imported oldest
  first; round = the optional number typed in the dialog, else the export's championship
  round, else the next free round.
- Creates/updates: `R01 <Grand Prix>` result sheet, `Races` index, `Driver Standings`,
  `Constructor Standings`, `Settings`, hidden `_ResultsData` (the editable model).
- **Correcting a result:** on a race sheet type a new number in the **Pos** column
  (e.g. `3` on Barrichello's row). That driver moves to P3, the others shift and are
  renumbered, points/gaps/winner/pole/fastest lap are recalculated, and both championship
  tables re-sort. Type `DSQ` to disqualify (moves to the bottom, no points); typing a number
  on a DSQ row reinstates it. Corrections are logged in row 7 and edited positions are blue.
  Invalid input is reverted. Edit one Pos cell at a time; other columns are display only.
- **Settings** sheet: points table (default `10,6,4,3,2,1`), optional fastest-lap bonus,
  year-suffix stripping. After changing it use **Race Data > Recalculate everything**.

## Rules (mirrors the hosted manager)

Same gates and scoring as `development\season_narrative\domain.py` (`normalize`, `scored`,
`standings`): only `final` captures; restart/red-flag continuation segments are rejected until
combined with the Race Data editor; duplicate capture IDs and occupied rounds are rejected;
retirements are classified at >= floor(90%) of the winner's laps; ties are broken by countback,
full ties share a position. Collector issues and "classification review" flags are shown on the
race sheet and in `Races`.

## Verification (2026-09-27)

`python -B tests/manager_points.py <RaceData> <tmp.json>` then
`node tests/run_tests.js <RaceData> <tmp.json>` — see `evidence/test-run-20260927-2.txt`,
hashes in `evidence/hashes-20260927.txt`.

- Corpus 64 exports: 56 accepted, 8 rejected (all red-flag continuations) — same split as the manager.
- Points and classified flag identical to the manager for every driver in all 56 accepted exports.
- End-to-end with fake Sheets: Hockenheim import; Barrichello P1 -> P3 gives Trulli 10,
  Montoya 6, Barrichello 4 in race and standings, Renault leads constructors; DSQ and invalid
  input paths; second race (Monza) accumulates and standings stay sorted.
- `evidence/test-run-20260927-1-failed-expectation.txt`: preserved earlier run where the test
  expected the wrong signed gap (code was right: -10.698).

**Not verified:** running in real Google Sheets/Apps Script (fake API covers only calls used;
formatting calls are no-ops), dialog upload in a browser, simple-trigger timing with many races,
and human review of presentation. Limitations: gaps after manual moves come from original clocks
(may be negative); no Drive-folder auto-import; red-flag continuation files must be combined first.
