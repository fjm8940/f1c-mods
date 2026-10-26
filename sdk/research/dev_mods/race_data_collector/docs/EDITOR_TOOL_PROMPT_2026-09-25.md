# Race Data JSON editor — implementation prompt for another LLM (2026-09-25, rev 2)

Purpose: hand the prompt body below to a coding LLM working in this workspace.
It defines a desktop GUI in which the owner can easily change any property of a
Race Data Collector JSON export — race classification, qualifying, grid, times,
gaps, flags, metadata, arbitrary keys — and save a copy that stays valid for
every downstream consumer, above all the hosted championship manager on the VPS.

Owner request (verbatim intent): "create a tool where I can edit the JSON and
race result properties etc, such as changing quali race results etc,
everything should be editable". Owner clarification: the point is a GUI for easy
manipulation, and edited files must remain valid for further use such as
importing into other applications; the files are uploaded to the championship
manager on the VPS.

Revision 2 corrects rev 1 after verification; see
[EDITOR_TOOL_PROMPT_REVIEW_2026-09-25.md](EDITOR_TOOL_PROMPT_REVIEW_2026-09-25.md).
Rev 1 is preserved at
`workspace_organization_audit\editor_prompt_review_BEFORE_2026-09-25\r2_EDITOR_TOOL_PROMPT_2026-09-25.md`.

---

## PROMPT BODY — give everything below this line to the implementing LLM

You are implementing a new local tool in the workspace `%USERPROFILE%\Desktop\F1`.
Before writing code read: workspace `AGENTS.md`, `README.md`,
`governance\README.md`, `governance\PIPELINE.md`, `governance\PRODUCT_STATUS.json`,
`development\AGENTS.md`, `development\mods\race_data_collector\README.md`, and
the consumer code named in "Downstream consumers". Run
`powershell -NoProfile -ExecutionPolicy Bypass -File tools\workspace\check-workspace-structure.ps1`
first and record its baseline (it currently reports pre-existing errors under
`production\`; do not repair them).

### Mission

Build a local, offline **tkinter desktop GUI** (Python 3.12 stdlib only) for
Race Data Collector exports (`metadata.schemaVersion == "f1c.race-data/2.0"`).
The owner must be able to:

1. open an export, edit results in friendly spreadsheet-like views, and edit any
   other property in a full tree editor;
2. see, before saving, whether the edited file will be accepted by the hosted
   championship manager and the local Season Narrative, and why not;
3. save a new copy that is strict JSON, keeps every unedited value exactly as it
   was, and never touches the original.

Everything is editable. Nothing is silently rewritten. Validation warns and
explains; it never auto-fixes and never blocks saving (except the strict-JSON
rules in "Fidelity").

GUI choice is fixed: tkinter/ttk (consistent with other workspace tools, no
network, double-click launch). Do not build a web UI.

### Verified data facts (2026-09-25; do not guess, re-derive counts in tests)

- Exports: `%USERPROFILE%\Documents\F1 Challenge V10\RaceData\*.json`
  (62 files, 2026-09-16..23). `%USERPROFILE%\Desktop\F1\F1 Challenge V10\RaceData`
  is a junction to the same directory. `recovery\` holds 1612 journal/snapshot
  files (out of scope for v1).
- Format of every export: UTF-8 without BOM, LF, two-space indent, `": "` and
  `","` separators as produced by Python `json.dumps(indent=2, ensure_ascii=False)`,
  one object, trailing newline. No non-ASCII bytes, no duplicate keys in the corpus.
- 43 top-level keys (42 sections + `metadata`): `blackMagicConfig, championship,
  deletedLaps, dnfList, driverOfTheDay, driverRatings, driverStandings, drsZones,
  fastestLap, fuelData, gameResults, incidents, initialGrid, keyEvents, lapChart,
  lapPositions, lastLapCrossings, livePositionOrder, metadata,
  newFastestLapAlerts, penaltyHistory, personalBestLaps, pitStops,
  plannedPitStops, playerConfig, positionHistory, qualifyingResults,
  raceControl, raceSegments, radarHistory, radioMessages, retirementReasons,
  sectorBoundaries, serviceMessages, sessionInfo, sessionInfoExtra,
  sessionTiming, teamInfo, telemetrySnapshot, topSpeeds, trackInfoExtra,
  vscPeriods, weather`. Values may be `null`, `[]`, `{}`, `""` or the `"-"`
  placeholder. Unknown keys must be preserved and editable.
- `gameResults = {drivers[], final, official, source}`; each driver has 61 keys
  (`finishPosition, gridPosition, lapsCompleted, numberOfLaps, totalTime,
  totalTimeRaw, bestLap, bestLapRaw, lastLap, lastLapRaw, gapToLeader, gapToPrev,
  dnf, dsq, dns, isPlayer, pitStops, hitsInRace, penalties, tireCompound,
  tireHistory, topSpeedKmH, driver, driverId, carNumber, team, nationality,
  vehFile, timeOfQualySession, timeOfQualySessionRaw, definition{}, rawTiming{},
  rawFinishState, rawRetiredFlag, ...`).
- **`driverStandings` is a mirror of `gameResults.drivers`**: same 61 keys and
  identical values in all 62 exports. Different consumers read different copies
  (see below), so results edits must reach both.
- `qualifyingResults[]`: `carNumber, driver, gridPosition, qualifyingTime,
  qualifyingTimeRaw, team`; its `gridPosition` equals `gameResults` `gridPosition`
  in all 62 exports. `initialGrid[]`: `carNumber, driver, position, team`.
- `fastestLap` is an **object** (`author, fastestLapOwners[], lapNumber,
  sectorBests{}, time, timeRaw`), not an array.
- `lapChart[]`: `{driver, laps[], vehFile}`; lap entries use `lap` and `lapTime`.
- `dnfList[]`: `{driver, vehFile}`; `retirementReasons[]`: `{driver, isDNS,
  isDSQ, retirementReasonCode, team, vehFile}`.
- `metadata`: `capture{captureId, final, completeHistory, previousCaptureId,
  continuationReason, issues[], ...}, championshipName, championshipRaceNumber,
  coverage, executableSha256, exportTime, exporterVersion, producer,
  racingModeName, schemaVersion, session, track`.
- `raceControl.readOnly: true` marks an observational section (warn on edit).
- Driver names carry a year suffix, e.g. `"Michael Schumacher (2002)"`.
  Consumers strip `\s*\(\d{4}\)\s*$` and compare case-insensitively.
- Fastest-lap example numbers: `2813.8330078125`, `103.21126556396484`; 100
  numbers in the corpus use exponent form, e.g. `4.1430968849454075e-05`.
- Primary acceptance file:
  `SINGLE_Monza, 2002_20260923_202210-28496-1_20260923_202210.json`
  (429,532 B, SHA256 `36a0c0e4881e3c810b23bb2c9a8fc29cc7ca5d2c36fb87620316023614723b23`).
  Largest: `SINGLE_Imola, 2002_20260921_083550-16536-47_20260921_083550.json`
  (6,834,545 B).

### Downstream consumers (the validity contract)

**A. Hosted championship manager (primary target).** Source
`development\season_narrative\` (`domain.py` `strict_load` + `normalize`,
`store.py` `import_race`, `race_evidence.py`). These files are byte-identical to
the deployed runtime `production\season_narrative_management\synthetic-australia-20260916\runtime.zip`.
Import rejects the file unless **all** hold:

- strict JSON: no duplicate keys, no `NaN`/`Infinity`;
- `metadata.schemaVersion == "f1c.race-data/2.0"` (manual/synthetic schemas exist
  but are out of scope for this editor);
- `metadata.capture.final is true` **and** `gameResults.final is true`;
- `metadata.capture.captureId` non-empty;
- `metadata.capture.previousCaptureId` and `continuationReason` absent/empty
  (8 of 62 current exports are continuation captures and are rejected today);
- `gameResults.drivers` has 1–100 rows; each has non-empty string `driver` and
  `team`; driver identity (year suffix stripped, casefolded) unique;
- `finishPosition` a JSON integer (not bool) 1–100, unique;
- `lapsCompleted` a JSON integer ≥ 0;
- `metadata.championshipRaceNumber`, if non-zero, must equal the round chosen at
  upload; the `sessionInfo.grandPrixName` year must match the season year unless
  the owner confirms a mismatch at upload.

Server-state rules the editor cannot check but must explain in the GUI:
the same `captureId` already imported with different contents is rejected, and a
round that already has a capture is rejected. **So edits must be made before a
race is uploaded; a correction to an already-uploaded round requires an
alternate universe in the manager.** The editor must never offer to change
`captureId` automatically to evade this.

Non-fatal effects to warn about: `totalTimeRaw`, `bestLapRaw`,
`timeOfQualySessionRaw` that are not finite numbers are dropped; if a driver's
`totalTimeRaw` differs from the sum of their `lapChart` `lapTime`s by > 0.1 s
(with a complete lap sequence) the manager withholds that total time;
`lapChart` rows are joined to results by exact `driver` string, so renaming only
one side loses lap evidence; `gameResults.official != true` adds a warning.
`race_evidence.summarize` raises `KeyError` if a result row lacks `driver`.

**B. Local Season Narrative desktop app.** `json_parser.parse_json` in
`%USERPROFILE%\Documents\F1 Challenge V10\Custom\2001\Season Narrative\`,
loaded through `development\mods\race_data_collector\tools\season_narrative_bridge.py`
(identical to the installed copy). It reads race results from
**`driverStandings`** (not `gameResults`), qualifying from `qualifyingResults`,
requires `metadata.capture.final`, rejects duplicate keys/non-finite numbers,
and derives mode/GP name from the **file name** (`SINGLE_<GP>_<YYYYMMDD_HHMMSS>.json`).
It calls `.strip()` on many string fields (`totalTime`, `bestLap`, `gapToLeader`,
`penalties`, `tireHistory`, `driver`...), so changing their type to a non-empty
number crashes it.

### Required deliverable

`development\mods\race_data_collector\tools\race_data_editor\` containing
`race_data_editor.py` (or a small package), `README.md`, `tests\`,
`run_editor.cmd` (double-click launcher), and `VERIFICATION.md`. Do not touch
game installs, `production\`, `archive\`, `_human_review\`, collector source/SDK,
the Season Narrative code, or any file in `RaceData\`.

### GUI requirements (must)

1. **Open**: file dialog (starting in `RaceData\`) and recent-files list. Show
   file name, size, SHA256, capture id, track, final/official flags.
2. **Task views** (the primary, easy path), each a sortable/filterable grid with
   in-cell editing, row move up/down, add/remove row:
   - *Race classification* — rows from `gameResults.drivers`, key columns first
     (`finishPosition, driver, team, carNumber, lapsCompleted, totalTime,
     totalTimeRaw, gapToLeader, gapToPrev, bestLap, bestLapRaw, gridPosition,
     dnf, dsq, dns, pitStops, penalties`), "show all columns" toggle.
     **Linked mirror (default on, visible checkbox):** every edit is applied at
     the same index to `driverStandings`; both paths appear in the diff. When the
     mirror is off or the arrays already differ, show a persistent warning.
     *Renumber* action: after drag-reordering rows, optionally rewrite
     `finishPosition` 1..N (previewed, undoable).
   - *Qualifying* — `qualifyingResults`, with optional linked update of
     `gridPosition` in results/standings and `initialGrid.position` (preview).
   - *Starting grid* — `initialGrid`.
   - *Fastest lap* — form view of the `fastestLap` object; `fastestLapOwners` and
     `sectorBests` editable inside it.
   - *Retirements* — `dnfList` and `retirementReasons` side by side.
   - *Session & metadata* — `sessionInfo`, `sessionTiming`,
     `metadata.championshipRaceNumber`, `metadata.capture` flags.
   - Any other array-of-objects section opens in a generic grid view.
3. **Time fields**: when editing a display time (`totalTime`, `bestLap`,
   `qualifyingTime`, `time`, `lastLap`, `gapTo*`) offer to update the paired
   `*Raw` number (parse `m:ss.fff`, `h:mm:ss.fff`, `+s.fff`), and vice versa.
   Offer only; show both values; the owner confirms.
4. **Driver rename / swap helper** (explicit action, preview required): matches
   by the old exact `driver` string across `gameResults.drivers`,
   `driverStandings`, `qualifyingResults`, `initialGrid`, `dnfList`,
   `retirementReasons`, `lapChart`, `lapPositions`, `lastLapCrossings`,
   `personalBestLaps`, `topSpeeds`, `driverRatings`, `telemetrySnapshot`,
   `livePositionOrder`, `plannedPitStops`, `incidents`, `newFastestLapAlerts`,
   `fastestLap.author`, `fastestLap.fastestLapOwners`, `positionHistory[].drivers`,
   `keyEvents` string values and `championship.driverStandings`. Show every hit
   path, let the owner untick hits, then apply. Do not change `driverId`,
   `vehFile` or `carNumber` unless the owner edits them.
5. **Tree editor** (advanced tab, full power): lazy-loading `ttk.Treeview`;
   add/rename/delete keys; add/remove/duplicate/move array elements; change type
   (string, integer, decimal, boolean, null, object, array); edit empty strings;
   copy/paste subtrees as JSON; jump-to-path (`gameResults.drivers[3].totalTime`);
   search keys and values. Editing any key under an object with
   `readOnly: true`, any `raw*` key, or `*Raw` key shows a non-blocking warning.
   Changing the JSON type of a field that consumer B `.strip()`s, or of a field
   consumer A requires as integer, shows a specific warning.
6. **Raw text mode**: whole document as text; parse errors show line/column;
   return to tree only when it parses under the strict rules.
7. **Undo/redo** for every edit including linked edits (one step per action);
   unsaved-changes indicator; confirm on close with unsaved changes.
8. **Validation panel** (live, grouped: *Blocks manager import*, *Blocks
   Season Narrative*, *Data warnings*, *Info*), each item clickable to jump to
   the path. Include every rule in "Downstream consumers" plus: `gridPosition`
   uniqueness; `finishPosition` contiguity 1..N; `lapsCompleted > numberOfLaps`;
   `dnf` rows vs `dnfList`/`retirementReasons`; `dsq`/`dns` vs
   `retirementReasons.isDSQ/isDNS`; display vs raw time disagreement; results vs
   standings mirror divergence; qualifying grid vs results grid;
   `fastestLap.author` vs the minimum `bestLapRaw` among drivers with laps
   (informational, retired drivers included); `completeHistory: false` and
   `official: false` informational. Warnings only; nothing auto-fixes.
9. **Consumer check button**: runs, in-process and read-only with
   `sys.dont_write_bytecode = True`, (a) `domain.normalize(domain.strict_load(text))`
   from `development\season_narrative`, and (b) the Season Narrative bridge
   `install(...)` + `parse_json(path)` on a temp copy that keeps the output file
   name. Show pass/fail and the exact exception text. If either consumer path is
   missing, report "not available" instead of failing. These calls must not
   write anywhere except the temp directory.
10. **Before-save review**: list of changed paths with old → new values, the
    validation summary, and consumer-check results. Cancel returns to editing.

### Save model (must)

- Default *Save as*: `development\mods\race_data_collector\trials\editor_edits\<YYYYMMDD_HHMMSS>\`
  containing the edited file **under the original file name** (consumer B parses
  it), an untouched byte copy of the source in `source\`, and
  `EDIT_MANIFEST.json` with: ``, source path, source SHA256
  and size, edited SHA256 and size, tool version, timestamps, changed-path list,
  validation and consumer-check results, and restoration instructions.
  Ensure the directory name is unique; never overwrite.
- *Export copy to…* lets the owner write the edited file anywhere else (e.g. for
  upload); it still creates the trials record above first. Refuse to overwrite
  an existing file unless the owner types the file name to confirm.
- Overwriting the original in `RaceData\` is **not offered in v1**.
- Optional *Record edit provenance* checkbox (default on, shown in the diff):
  adds `metadata.editorProvenance = {tool, version, editedAt, sourceSha256,
  changedPaths}`. Both consumers ignore unknown keys; the manager's content
  digest will differ from the original, which is expected.
- Never auto-save. Never modify the source on load. Detect source change on disk
  (size + mtime + SHA256) and warn.
- The tool never deletes files. Deleting keys/elements inside the document is a
  user edit with undo.

### Fidelity (must)

- Parse with the stdlib `json` module using `parse_int`/`parse_float` hooks that
  return a number wrapper retaining the **original source lexeme**, and
  `object_pairs_hook` that rejects duplicate keys (report the path) and keeps
  order. `parse_constant` rejects `NaN`/`Infinity`.
- **Do not use `simplejson use_decimal`**: verified to rewrite exponent lexemes
  (`4.1430968849454075e-05` → `0.000041430968849454075`) in 9 of 62 exports.
  Do not route unedited numbers through `float`.
- Write with a custom serializer that emits wrappers by lexeme and reproduces
  Python `json.dumps(indent=2, ensure_ascii=False)` layout exactly (including
  `[]`/`{}` for empty containers and standard string escaping), LF, trailing
  newline, UTF-8 without BOM. Input with another layout is re-emitted in this
  canonical layout, with a notice.
- New or edited numbers: integers written without a decimal point; decimals
  written as the owner typed them after validating they are finite JSON numbers.
  Keep integer vs decimal type unless the owner changes it.
- Refuse to save if the document contains non-finite numbers or duplicate keys.
- Key order preserved; "sort keys" only as an explicit action.

### Performance (must, measured)

- 6.8 MB file: open+parse+first render < 3 s on this PC; tree children load on
  expand; `keyEvents`/`positionHistory` never fully rendered at once.
- Save of 6.8 MB < 3 s; heavy work off the UI thread or with a progress
  indicator so the window stays responsive.
- Editing a 24-row classification grid updates validation in < 200 ms.

### Non-goals

- No upload to or network contact with the VPS; the owner uploads manually.
- No journal/recovery editing; no merging of continuation captures.
- No automatic recalculation of points, gaps, standings or championship.
- No edits to collector DLL/INI/SDK, Season Narrative code, game installs,
  production artifacts or `RaceData\`.

### Tests (must, headless, `python -m unittest` from the tool folder)

Tests read the real corpus but write only to a temp directory; hash every
`RaceData\*.json` before and after the run and fail if any changed.

1. Round-trip: every `RaceData\*.json` loads and saves byte-identical with no
   edits (including the 9 exponent-lexeme files, named explicitly).
2. Headless CLI: `race_data_editor.py --check <file>` prints validation and both
   consumer checks, exits 0 for the Monza file, 0 with warnings for a
   continuation capture (reports the manager rejection), non-zero only for
   invalid JSON. `--set <path>=<json> --out <dir>` applies edits for scripting.
3. Edits, each saved, reopened, and passed through consumer check A: swap P1/P2
   (positions + mirror); change a qualifying time and its raw value; rename a
   driver via the helper (all listed sections updated, lapChart join intact);
   add a key; remove an array element; change a value type.
4. Validation fires for: duplicate keys, `NaN`, duplicate `finishPosition`,
   `finishPosition` as `1.0` or `"1"`, empty team, `lapsCompleted` negative or
   decimal, `capture.final` false, `gameResults.final` false, results/standings
   divergence, renamed driver missing from `lapChart`, numeric value in a
   `.strip()` field.
5. Consumer-check isolation: no files created or changed outside the temp dir
   (check the Season Narrative directory `__pycache__` mtimes before/after).
6. Save model: source untouched, manifest hashes correct, restoration from
   `source\` reproduces the source byte-for-byte, no overwrite without typed
   confirmation, output keeps original file name.
7. Performance timings for the 6.8 MB file recorded in `VERIFICATION.md`.

### Acceptance and reporting

- Record in `VERIFICATION.md`: commands, Python version, file hashes, test
  results, measured timings, consumer-check outputs, known limitations.
- Add a tool README (launch, workflow, validation meaning, save model, safety,
  limitations, the "edit before upload" rule) and link it from
  `development\mods\race_data_collector\README.md`. Preserve the before-state of
  that README under `workspace_organization_audit\<PHASE>_BEFORE_<YYYYMMDD>\`
  before editing it.
- Re-run the workspace validator; report that errors are unchanged from baseline.
- Do not update `governance\PRODUCT_STATUS.json` unless a product stage genuinely
  changes.
- Do not claim GUI usability as validated. Owner validation: open latest Monza
  export, change qualifying and race results, review diff and consumer checks,
  save, upload the copy to a **test universe** in the hosted manager, confirm the
  imported classification matches the edit.

## END PROMPT BODY

Context for the owner: definition only. No tool built, no export or consumer
changed. Owner decision recorded here as a default the implementer must follow:
v1 writes edited copies only to `trials\editor_edits\` and owner-chosen export
paths, never over `RaceData\`.
