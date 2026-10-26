# Race Data Editor

Local, offline tkinter editor for Race Data Collector v2 exports
(`metadata.schemaVersion == "f1c.race-data/2.0"`). Python 3.12 standard library
only. Nothing is uploaded; the owner still uploads manually to the hosted
championship manager.

Status: implementation complete 2026-09-25. Automated verification:
[VERIFICATION.md](VERIFICATION.md). Red-flag profiles, evidence tiers and
limits: [docs/RF_REPAIR.md](docs/RF_REPAIR.md).

## Launch

```
tools\race_data_editor\run_editor.cmd            (double-click; opens the GUI)
python -B tools\race_data_editor\race_data_editor.py [file.json]
```

The GUI opens a file dialog starting in `F1 Challenge V10\RaceData`, keeps a
recent-files list, and shows file name, size, SHA256, captureId and flags.

## What you can edit

| Tab | Contents |
|---|---|
| Race classification | `gameResults.drivers` as a grid; key columns first, "show all columns" toggle; **linked mirror** checkbox (default on) keeps `driverStandings` in step; *Renumber finish positions* is previewed and undoable |
| Qualifying | `qualifyingResults`; *Edit > Link qualifying grid...* previews and applies `gridPosition` to results, standings and `initialGrid.position` |
| Starting grid | `initialGrid` |
| Retirements | `dnfList` and `retirementReasons` side by side |
| Fastest lap | `fastestLap` form (`fastestLapOwners`/`sectorBests` live in the Tree tab) |
| Session & metadata | `sessionInfo`, `sessionTiming` (read-only), `metadata.championshipRaceNumber`, `metadata.capture` flags |
| Other sections | any top-level array of objects in a generic grid |
| Tree | lazy full-document editor: add/rename/delete keys, array add/remove/duplicate/move, type changes, JSON subtree copy/paste, jump-to-path, search |
| Raw text | whole document as text; parse errors are reported with line/column; apply only when strict JSON parses |

Editing a display time (`totalTime`, `bestLap`, `qualifyingTime`, `time`,
`lastLap`, `gapTo*`) offers to update the paired `*Raw` value, and vice versa.
Nothing is auto-fixed.

*Edit > Driver rename / swap helper* finds the exact old driver string in the
documented sections (`gameResults.drivers`, `driverStandings`,
`qualifyingResults`, `initialGrid`, `dnfList`, `retirementReasons`, `lapChart`,
`lapPositions`, `lastLapCrossings`, `personalBestLaps`, `topSpeeds`,
`driverRatings`, `telemetrySnapshot`, `livePositionOrder`, `plannedPitStops`,
`incidents`, `newFastestLapAlerts`, `fastestLap.author`,
`fastestLap.fastestLapOwners`, `positionHistory[].drivers`, `keyEvents`
driver fields and `championship.driverStandings`). Every hit is listed and can
be unticked before applying. `driverId`, `vehFile` and `carNumber` are not
changed by the helper.

## Validation and consumer checks

The right-hand panel is live and grouped:

- **Blocks manager import** - the hosted manager (`domain.strict_load` +
  `domain.normalize`) would reject the file, with the reason;
- **Blocks Season Narrative** - the installed desktop parser would crash (for
  example a number in a field it calls `.strip()` on);
- **Data warnings** - duplicate grid positions, non-contiguous finish
  positions, display/raw disagreement, results/standings divergence,
  lap-sum divergence, retirement-list mismatches, red-flag clusters;
- **Info** - `completeHistory: false`, `official: false`, fastest-lap lap 0,
  placeholder rows, enrollment notes.

Every finding names its JSON path and can be jumped to. The *Consumer checks*
button runs both real importers in-process, read-only, against a temp copy and
reports their exact result. Nothing is written to the consumers.

### Edit before upload

The hosted manager rejects a `captureId` that was already imported with
different contents and rejects a round that already has a capture. **Make edits
before uploading a race.** Correcting an already-uploaded round requires an
alternate universe in the manager. The editor never changes `captureId`
automatically to evade those gates.

## Saving

- *File > Save as (trials record)* (Ctrl+S) writes to
  `development\mods\race_data_collector\trials\editor_edits\<YYYYMMDD_HHMMSS>\`:
  the edited file **under the original file name**, an untouched byte copy in
  `source\`, and `EDIT_MANIFEST.json` marked `` with source
  and edited hashes, changed paths, validation and consumer-check results, and
  restoration instructions. The directory name is always unique; nothing is
  overwritten.
- *File > Export copy to...* writes the edited file anywhere else (for upload).
  It refuses to overwrite unless you type the exact file name.
- The provenance checkbox (default on) adds
  `metadata.editorProvenance = {tool, version, editedAt, sourceSha256,
  changedPaths}`; it is shown in the before-save diff. It is not re-stamped on
  later saves of an already-stamped file.
- Overwriting the original in `RaceData\` is not offered. The tool never
  modifies the source export and never deletes files. Source changes on disk
  are detected (size + SHA256) and warned about.

## Fidelity

Strict JSON only: duplicate keys and `NaN`/`Infinity` are rejected. Numbers
keep their original source lexemes, so an unedited export re-saves
byte-identical (verified for the whole corpus incl. exponent-form numbers).
Layout is exactly Python `json.dumps(indent=2, ensure_ascii=False)`, LF,
UTF-8 without BOM, trailing newline. Key order is preserved; sorting keys is
never automatic.

## Red-flag repair

*Tools > Red flag repair wizard* loads the segment chain (embedded
`raceSegments` plus predecessor search in `RaceData\` and `recovery\` by
captureId), shows the timeline, and lets you choose **race resumed (combined)**
or **race stopped**, plus the rule profile and parameters. Every repair is a
proposal list; only accepted proposals are applied, as **one undo step**.
Rejecting every proposal leaves the document unchanged. The output passes the
hosted manager and the Season Narrative; `metadata.repair` records sources,
profile, parameters and decisions, and the captureId becomes
`<finalCaptureId>.rf-<8 hex>` (deterministic, distinct from any raw capture).
See [docs/RF_REPAIR.md](docs/RF_REPAIR.md).

## Headless use

```
python -B race_data_editor.py --check FILE
python -B race_data_editor.py --set PATH=JSON --out DIR FILE
python -B race_data_editor.py --rf-inspect FILE
python -B race_data_editor.py --rf-repair FILE --mode combined --profile era_2001_2004 ^
       --decisions fixtures\decisions\monaco_combined_era_2001_2004.json --out DIR
python -B race_data_editor.py --hash-fixtures
```

## Tests

```
cd development\mods\race_data_collector\tools\race_data_editor
python -B -m unittest discover -s tests -p "test_*.py"
```

All tests read the real corpus, write only to temp directories, and hash the
fixtures before and after; any fixture change fails the run.

## Known limitations (v1)

- The manager has no half-points support; a stopped short race still scores
  full points there. Recorded as `metadata.repair.points`; manager-side support
  is optional work package WP7 and needs separate owner approval.
- `fastestLap.lapNumber` 0 is left 0 when no observed lap matches; the tool
  never invents a lap number.
- `lapChart` merging only happens when every counted segment has per-driver
  charts; otherwise it is kept as recorded with a note.
- Predecessor search matches the captureId tokens embedded in file names, then
  confirms the candidate's metadata; a predecessor whose file name does not
  contain its captureId is reported missing.
- GUI usability and the hosted-manager test-universe upload are owner
  validation, not claimed by the automated tests.
