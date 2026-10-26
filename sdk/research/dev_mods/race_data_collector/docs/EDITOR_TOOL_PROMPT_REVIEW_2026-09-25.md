# Review of EDITOR_TOOL_PROMPT_2026-09-25.md (2026-09-25)

Pipeline stage: define/specify (review of a definition prompt). Read-only against
exports; no tool built, no export edited, prompt file unchanged.

Before-state of `README.md` preserved at
`workspace_organization_audit\editor_prompt_review_BEFORE_2026-09-25\README.md`
(SHA256 `b74f916c...d455882`).

## Verified evidence (read-only scripts against `Documents\F1 Challenge V10\RaceData`)

- Monza acceptance file: 429,532 B, SHA256 `36a0c0e4...723b23` — matches prompt.
  UTF-8 no BOM, LF, trailing newline — matches.
- Largest file Imola `..._083550-16536-47_...`: 6,834,545 B — matches.
- `recovery\`: 1612 entries — matches.
- Export count: **62**, not "about 70".
- Top-level keys: **43 including `metadata`** (42 + metadata), not "41 plus metadata".
  The listed names are correct; only the count is wrong.
- `gameResults.drivers[0]`: **61 keys**, not ~70.
- `fastestLap` is an **object**, not an array; it cannot be a row table view.
- `driverOfTheDay` is `null` in Monza.
- `readOnly` exists at `raceControl.readOnly` (plus `raceControl.limitations`),
  not `metadata.readOnly`.
- `championship` holds `calendar`, `driverStandings` (empty in Monza),
  `teamStandings` — a second standings location the prompt ignores.
- Duplicate keys: none across 62 files. Non-ASCII bytes: none. CRLF: none.
- Number fidelity:
  - stdlib `json.loads` + `json.dumps(indent=2, ensure_ascii=False)`: **0/62**
    byte-identical (confirms prompt's warning).
  - `simplejson` `use_decimal=True` + same formatting: **53/62** byte-identical.
    The 9 failures are exponent lexemes, e.g. `4.1430968849454075e-05` emitted as
    `0.000041430968849454075`. 100 exponent-form lexemes exist in the corpus.
    **The prompt's allowance of `simplejson use_decimal` contradicts its own
    lexeme-identical requirement.**

## Findings (severity ordered)

1. High — Fidelity: remove the `simplejson use_decimal` allowance. Require a
   raw-lexeme number type (e.g. `parse_float`/`parse_int` returning a wrapper that
   keeps the source text) and a custom serializer. Add the 9 exponent files as
   named regression fixtures.
2. High — Table view list includes `fastestLap` (object). Replace with a
   form/object view; keep `fastestLap.fastestLapOwners` / `sectorBests` as tables
   if they are arrays.
3. Medium — Section counts wrong (41 vs 42+metadata, ~70 vs 61 driver keys,
   ~70 vs 62 exports). Correct them or state "at time of writing, approximate".
   Tests should derive counts from the corpus, not hard-code them.
4. Medium — Warn-marker example `metadata.readOnly` does not exist; real marker
   is `raceControl.readOnly`. Say "any object containing `readOnly: true`".
5. Medium — Cross-section helper (req 8) omits `championship.driverStandings`,
   `lapChart`, `personalBestLaps`, `topSpeeds`, `driverRatings`,
   `telemetrySnapshot`, `livePositionOrder`, `plannedPitStops`, `keyEvents`, and
   `metadata` identity fields. Driver identity also spans `driverId`, `carNumber`,
   and `vehFile`; rename by display name alone is ambiguous. Define the join key
   (suggest `vehFile` or `driverId`) and list every section it touches.
6. Medium — Round-trip test over "every `RaceData\*.json`" collides with the
   "copy test files to temp first" rule only if the test ever writes; say
   explicitly: read source bytes, write only to temp, hash-compare sources
   before/after.
7. Medium — Save-backup location conflict: req 7 says `tools\race_data_editor\edits\`,
   safety section says "under the tool or a component `trials\` area". Pick one.
   Real race data (and edited race data) inside a source/tool folder risks being
   committed; add an ignore rule or put edits under `trials\` with
   ``.
8. Low — "Detect and preserve a different existing convention" is under-specified
   (indent width, key separators, escaping). Corpus has one convention; narrow v1
   to canonical-only and refuse/warn on non-canonical input formatting instead.
9. Low — Overwrite of the original export "requires typed confirmation" still
   writes into `RaceData\`, the live game output store that Season Narrative
   imports. Consider making in-place overwrite a v2 feature, or require the
   manifest path be shown in the confirmation.
10. Low — Lint "fastest lap matching fastest `bestLap` among finishers" is wrong
    if a retired driver set the fastest lap; compare against all classified
    drivers with laps, report as informational.
11. Low — Performance criterion lacks a threshold. Suggest: 6.8 MB load+parse <
    3 s, tree view lazy-expands (no full render of `keyEvents`/`positionHistory`).
12. Low — GUI choice left open. tkinter `ttk.Treeview` with 6.8 MB and deep
    arrays needs lazy loading; a localhost web UI needs an explicit bind to
    `127.0.0.1` and a random port/token. State both constraints.
13. Info — No Season Narrative compatibility check in acceptance. Non-goal
    forbids import, but a read-only validation using the consumer's own strict
    loader (if callable offline) would prove "strict-JSON compatible".

## Verdict

Solid spec; preservation, safety, non-goals, and human-validation boundary are
correct. Not ready to hand off until findings 1, 2, 5, 7 are fixed; 3/4 are
cheap factual corrections.

## Pending (round 1)

- Owner decision: apply these corrections to the prompt body (prompt file needs
  its own before-state preservation first).
- No human validation applies to this review.

## Round 2 — validation for handoff (2026-09-25)

Owner clarified: the goal is a GUI for easy editing, and edited files must stay
valid for further use; exports are uploaded to the hosted championship manager
on the VPS. Prompt rewritten as rev 2. Before-states preserved in
`workspace_organization_audit\editor_prompt_review_BEFORE_2026-09-25\r2_*`
(prompt rev 1 SHA256 `697fc0fc...ab9aa3009`).

### Verified evidence (read-only; temp copies only)

- Hosted manager importer = `development\season_narrative\domain.py`
  (`strict_load`, `normalize`), `store.py` `import_race`, `race_evidence.py`,
  `retirements.py`. All four byte-identical to the latest deployed runtimes
  `production\season_narrative_management\synthetic-australia-20260916\runtime.zip`
  and `...-reader-20260916\runtime.zip`.
- The manager reads results from **`gameResults.drivers`**; hard gates listed in
  prompt rev 2. `normalize` on the corpus: **54/62 accepted, 8 rejected** with
  "Restart/red-flag continuation needs a combined classification". Monza file
  accepted (24 drivers).
- Server-state gates: same `captureId` with different digest is rejected; a
  round that already has a capture is rejected. Consequence: edits must happen
  before upload; corrections to uploaded rounds need an alternate universe.
- Local Season Narrative (`json_parser.parse_json` via
  `tools\season_narrative_bridge.py`, identical to installed copy) reads results
  from **`driverStandings`**, derives mode/GP from the file name, and `.strip()`s
  many string fields. Bridge parse of a temp Monza copy succeeded, source hash
  unchanged.
- `driverStandings` equals `gameResults.drivers` (same keys and values) in
  **62/62** exports; qualifying `gridPosition` equals results `gridPosition` in
  62/62. Editing only one copy changes only one consumer's view — rev 2 requires
  a visible linked mirror.
- `lapChart` lap entries use `lapTime` (rev 1 said `time`); the manager joins
  them to results by exact `driver` string.
- Fidelity prototype: stdlib `json` with `parse_int`/`parse_float` capturing the
  source lexeme, `json.dumps(indent=2, ensure_ascii=False)`, lexeme substitution,
  trailing LF: **62/62 byte-identical**. A first prototype that used `\x00`
  placeholders failed 0/62 because of escaping; placeholders must be
  collision-checked, non-control text (or a real custom serializer).
- `trials\` and a future `tools\race_data_editor\edits\` are not git-ignored.
  Rev 2 puts edits under `trials\editor_edits\` with ``.
  Whether trials race data should be ignored in Git is not decided here.

### Decisions encoded in rev 2 (defaults; owner may override)

- GUI fixed to tkinter/ttk (rev 1 left the choice open).
- No in-place overwrite of `RaceData\` in v1; *Save as* to trials + *Export copy to…*.
- Edited file keeps the original file name (consumer B parses it).
- Optional, default-on `metadata.editorProvenance` stamp, shown in the diff.
- Editor never rewrites `captureId` to avoid the manager's duplicate gate.
- Built-in read-only consumer check runs both importers on the edited document.

## Round 3 — red-flag scope added (2026-09-25)

Owner added: edit malformed results, declare a race stopped at a red flag, and a
built-in parser that repairs red-flag restarted races. Handoff document is now
[EDITOR_RF_REPAIR_PROMPT_2026-09-25.md](EDITOR_RF_REPAIR_PROMPT_2026-09-25.md)
(rev 3, includes rev 2 by reference).

Verified (read-only):

- Monaco `Downloads\SINGLE_Monaco, 2002_20260925_144455-9388-11_20260925_144455.json`:
  6,165,687 B, SHA256 `8fe12542...5eb31b`, collector 0.2.5, one embedded
  segment. The restart reset lap numbering to 1 and ran the full 39 laps. 22/24
  are DNF. Duplicate grid 2. Earlier DNFs reappear as 0-lap placeholders. Seven
  lap-0 retirements between 134 and 202 s. Segment 0 `lapChart` is empty. Full
  facts are in the rev 3 prompt.
- 9 continuation captures (8 RaceData + Monaco) catalogued with segment counts
  and leader laps. Three continuation reasons, 0–20 embedded segments; 3 have
  no embedded predecessor.
- Feasibility, in memory only: Monaco with continuation fields moved into
  `metadata.repair` and a derived captureId passes `domain.normalize` (24
  drivers). No file was written.

Inference, not verified: whether the lap-0/code-0 retirement cluster is a
restart artifact or genuine game behaviour. Rev 3 makes it an owner decision.

Open owner decisions: rule profile, countback `k`, retirement-cluster handling,
acceptance of the `metadata.repair` + derived-captureId identity scheme.

### Status

Rev 2 was ready for handoff; superseded by rev 3 as the handoff document. Findings 1–13 from round 1 are addressed in rev 2.
Still needs owner validation after implementation: GUI usability, and a test
upload to a test universe in the hosted manager.
