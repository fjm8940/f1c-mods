# VERIFICATION - Race Data Editor (2026-09-25)

Scope: implementation verification for the editor and red-flag repair tool
described in `docs\IMPLEMENTATION_PLAN_RACE_DATA_EDITOR_2026-09-25.md`.
Nothing in production, the game installs, `RaceData\`, the collector source,
the Season Narrative or the VPS was modified.

## 1. Environment and baseline

- Windows, Python 3.12.10, stdlib only.
- Workspace validator
  `powershell -NoProfile -ExecutionPolicy Bypass -File tools\workspace\check-workspace-structure.ps1`
  reported **11 errors before implementation and 11 errors after** (identical
  list: `.pytest_cache`, `f1 challenge testing versions`, `trials`,
  `F1 Challenge V10_clean` missing twice, six source-like files under
  `production\`). None were repaired, per instruction.
- Fixture hash baseline: `tests\fixture_hashes_baseline.json` (WP0), 62
  `RaceData\*.json` + the Monaco Downloads fixture.
- During implementation a **new live capture appeared** (the owner ran the
  game): `SINGLE_Monza, 2002_20260925_175954-22092-1_20260925_175954.json`,
  347,574 B, SHA256
  `b0f9b876c95eb846e20a7cf4212254ea47daeed86669ccb5acf400b95dcfe667`.
  Tests derive the corpus count and include it; it round-trips byte-identical
  and passes the manager (`PASS - accepted 24 drivers`, 3 info findings).
- Monaco primary fixture: 6,165,687 B, SHA256
  `8fe125420f245b0a22295af4e15e50bf73efe2e1873b118343e8d397da5eb31b`.
- Every test run hashes all fixtures before and after and fails on any change;
  no fixture changed in any run.

## 2. Commands

```
cd development\mods\race_data_collector\tools\race_data_editor
python -B -m unittest discover -s tests -p "test_*.py"        # 49 tests, OK (26.4 s)
python -B race_data_editor.py --check <file>
python -B race_data_editor.py --rf-inspect <file>
python -B race_data_editor.py --rf-repair <file> --mode combined --profile era_2001_2004 ^
    --decisions fixtures\decisions\monaco_combined_era_2001_2004.json --out <dir>
```

## 3. Work-package gates

| Gate | Evidence | Result |
|---|---|---|
| WP1 T1 round-trip | 62/62 RaceData (baseline date) + 63rd live capture + Monaco, byte-identical; 9 named exponent-lexeme files re-verified | PASS (`tests\test_wp1_jsondoc.py`, 17 tests) |
| WP1 parse rules | duplicate keys rejected with path; `NaN`/`Infinity` rejected; lexemes kept; layout equals stdlib dumps | PASS |
| WP1 paths/undo | get/set/insert/delete/move persistent; grouped undo/redo; linked mirror, row swap, renumber, rename hits | PASS |
| WP2 T3 lint matrix | every rule fires on a synthetic document (position type/duplicate, team, laps, final flags, mirror, lapChart join, strip-type narrative block, continuation, filename, grid duplicate, contiguity, lap-sum, laps > scheduled, dnf/dsq/dns lists, display/raw, qualy grid, fastest-lap author, cluster, missing predecessor) | PASS (`test_wp2_validate.py`, 16 tests) |
| WP2 continuation block | all 9 continuation captures validate as `continuation` and the manager rejects them with the combined-classification message | PASS |
| WP2 T13 isolation | installed Season Narrative `__pycache__` mtimes/sizes unchanged across consumer checks | PASS |
| WP3 save model | unique trials dirs, `source\` byte-identical, manifest hashes correct, restoration byte-identical, original name kept, typed-confirmation overwrite refusal, provenance only once | PASS (`test_wp3_save.py`, 5 tests) |
| WP4 T4 inspect | `--rf-inspect` succeeds on all 9 continuation fixtures; Monaco reports 2 segments, C = 1, numbering restarted, duplicate grid 2, 2 void-part DNFs reappearing as placeholders, lap-0 cluster of 7, 0 unsupported retirements | PASS (`test_wp4_rf.py`) |
| WP4 T5 oracle | Monaco combined matches the plan section 2 table exactly; mirror identical; manager accepts; Narrative parses; points Panis 10 / Sarrazin 6 via `domain.scored` | PASS |
| WP4 T6 stopped | refused without override (`C - k = 0`); with override `points.rule = "none"` + warning, manager accepts | PASS |
| WP4 T7 Imola 115557 | compared against a hand-written table derived from inspect/raw data before repair (below) | PASS |
| WP4 T8 Imola 083550 | inspect reports no valid classification; repair refuses without override; captureId stable across runs, changes when a decision changes | PASS |
| WP4 T9/T11 | missing predecessor reported, repair allowed with warning; rejecting every proposal leaves the document unchanged | PASS |
| WP4 T10 | repaired file reopened and saved unedited is byte-identical | PASS |
| WP5 T12 performance | 6.8 MB Imola: open 0.39-0.46 s, save 0.37-0.44 s, validate 0.003 s (targets < 3 s / < 3 s / < 0.2 s) | PASS |
| WP5 GUI smoke | programmatic: open Monaco, header/24 rows, edit cell with mirror, undo/redo, wizard widgets, build preview, consumer check, apply as one undo step, save record | PASS (`test_wp5_gui.py`, 3 tests) |
| WP6 documentation | this file, `README.md`, `docs\RF_REPAIR.md`; component README link added; validator unchanged (11) | PASS |

Full suite: `Ran 49 tests ... OK`.

## 4. Fidelity evidence

- Stdlib `json.loads` + `json.dumps(indent=2, ensure_ascii=False)` alone: 0/62
  byte-identical (confirms the lexeme problem).
- Lexeme-preserving parse + custom serializer: 62/62 + Monaco + the new live
  capture byte-identical, including the 9 exponent-lexeme files:
  `_190304-21868-1`, `_070553-1428-1`, `_073310-7484-1`, `_080634-17532-1`,
  `_101728-6164-2`, `_103413-24764-2`, `_111002-28132-2`,
  `_142214-30448-1`, `_150648-20368-1`.
- New/edited numbers: integers without a decimal point, decimals as typed;
  non-finite values are refused at save.

## 5. Monaco oracle: expected (plan section 2) vs actual

`--rf-repair ... --mode combined --profile era_2001_2004` with
`fixtures\decisions\monaco_combined_era_2001_2004.json`.

| Pos | Driver | Laps | Status | Grid | Total time | Expected | Match |
|---|---|---|---|---|---|---|---|
| 1 | Olivier Panis (2002) | 39 | Finished | 24 | 57:30.277 | native | yes |
| 2 | Stephane Sarrazin (2002) | 39 | Finished | 22 | 58:11.313 | native | yes |
| 3 | Tester A | 27 | Ret | 7 | native | native | yes |
| 4 | Nick Heidfeld (2002) | 25 | Ret | 14 | native | native | yes |
| 5 | Michael Schumacher (2002) | 23 | Ret | 11 | native | native | yes |
| 6 | Luca Badoer (2002) | 20 | Ret | 4 | native | native | yes |
| 7 | Jenson Button (2002) | 14 | Ret | 17 | native | native | yes |
| 8 | Kimi Raikkonen (2002) | 10 | Ret | 1 | native | native | yes |
| 9 | Marcel Lasee (2002) | 10 | Ret | 6 | native | native | yes |
| 10 | Fernando Alonso (2002) | 7 | Ret | 12 | native | native | yes |
| 11 | Felipe Massa (2002) | 6 | Ret | 10 | native | native | yes |
| 12 | Takuma Sato (2002) | 6 | Ret | 13 | native | native | yes |
| 13 | Rubens Barrichello (2002) | 6 | Ret | 8 | native | native | yes |
| 14 | Ricardo Zonta (2002) | 4 | Ret | 19 | native | native | yes |
| 15 | Jirko Malcharek (2002) | 3 | Ret | 21 | native | native | yes |
| 16 | Darren Manning (2002) | 0 | Ret | 18 | 0:34.381 | native | yes |
| 17 | Matteo Bobbi (2002) | 0 | Ret | 23 | 0:34.381 | native | yes |
| 18 | Jarno Trulli (2002) | 0 | Ret | 20 | 0:34.381 | native | yes |
| 19 | Andre Lotterer (2002) | 0 | Ret | 16 | 0:34.381 | native | yes |
| 20 | Juan Pablo Montoya (2002) | 0 | Ret | 15 | 0:34.381 | native | yes |
| 21 | Ralf Schumacher (2002) | 0 | Ret | 9 | 0:34.381 | native | yes |
| 22 | Luciano Burti (2002) | 0 | Ret | 3 | 0:34.381 | native | yes |
| 23 | Justin Wilson (2002) | 0 | Ret (void part, reason 7) | 2 | `-` / null | `-` / null | yes |
| 24 | David Coulthard (2002) | 0 | Ret (void part, reason 0) | 5 | `-` / null | `-` / null | yes |

Also verified: `metadata.championshipRaceNumber` 0; `fastestLap.lapNumber` 0
with the info note ("observed minimum 82.262 s on lap 19"); mirror identical;
manager accepted 24 drivers; Narrative parsed 24; `domain.scored` with a 2002
table (10-6-4-3-2-1) gives Panis 10, Sarrazin 6, total 16.

Repaired evidence file (preserved):
`development\mods\race_data_collector\tools\race_data_editor\trials\editor_edits\20260925_rf_repair_evidence_monaco\`,
SHA256 `827ab21d29c178695acc5834d521bf668b22e27d032c2b95a4f3486195e349f4`,
captureId `20260925_144455-9388-11.rf-f3030943`.

Deviation recorded: the D6 bullet says "rows with `lapsCompleted == 0` set
totalTime `-`"; the section-2 oracle table lists native times for restart
participants (rows 16-22) and placeholders only for the two void-part rows.
The implementation follows the **oracle table** (only void-part placeholder
rows are cleared) and documents this in `RF_REPAIR.md`. Everything else in the
table matches.

## 6. Imola 115557 combined: hand-derived vs actual

Hand table written from `--rf-inspect` output and the raw segment 0 grid
before running the repair; `C = 7` (row 2), numbering continued, so native
laps/times are kept and the grid is restored from segment 0.

| Pos | Driver | Laps (expected/actual) | Grid (expected/actual) |
|---|---|---|---|
| 1 | Rubens Barrichello (2002) | 31 / 31 | 3 / 3 |
| 2 | Ralf Schumacher (2002) | 31 / 31 | 1 / 1 |
| 3 | Michael Schumacher (2002) | 31 / 31 | 2 / 2 |
| 4 | Stephane Sarrazin (2002) | 31 / 31 | 11 / 11 |
| 5 | Andre Lotterer (2002) | 31 / 31 | 6 / 6 |
| 6 | Luciano Burti (2002) | 31 / 31 | 4 / 4 |
| 7 | Justin Wilson (2002) | 31 / 31 | 16 / 16 |
| 8 | Takuma Sato (2002) | 31 / 31 | 18 / 18 |
| 9 | Marcel Lasee (2002) | 31 / 31 | 7 / 7 |
| 10 | Matteo Bobbi (2002) | 31 / 31 | 5 / 5 |
| 11 | Fernando Alonso (2002) | 31 / 31 | 20 / 20 |
| 12 | Jarno Trulli (2002) | 31 / 31 | 15 / 15 |
| 13 | Kimi Raikkonen (2002) | 31 / 31 | 14 / 14 |
| 14 | Felipe Massa (2002) | 30 / 30 | 22 / 22 |
| 15 | Nick Heidfeld (2002) | 30 / 30 | 23 / 23 |
| 16 | Luca Badoer (2002) | 23 / 23 | 19 / 19 |
| 17 | Jirko Malcharek (2002) | 23 / 23 | 21 / 21 |
| 18 | Jenson Button (2002) | 17 / 17 | 12 / 12 |
| 19 | Tester A (2002) | 16 / 16 | 10 / 10 |
| 20 | David Coulthard (2002) | 10 / 10 | 9 / 9 |
| 21 | Juan Pablo Montoya (2002) | 8 / 8 | 13 / 13 |
| 22 | Darren Manning (2002) | 2 / 2 | 8 / 8 |
| 23 | Olivier Panis (2002) | 2 / 2 | 17 / 17 |
| 24 | Matias | 0 / 0 | 24 / 24 |

Manager accepts; Narrative parses. The final segment's `lapChart` is empty in
the source, so it is kept as recorded with a note (no complete charts to
merge); the manager's lap-sum check therefore withholds nothing.

## 7. Consumer-check outputs (exact)

Plain Monza acceptance file (`--check`):

```
Validation: 0 manager block(s), 0 narrative block(s), 0 warning(s), 2 info
Hosted manager (domain.normalize): PASS - accepted 24 drivers, 5 warning(s)
Season Narrative (bridge parse_json): PASS - parsed 24 drivers, 3 anomaly note(s)
OK
```

Continuation capture (e.g. Imola `133105-22988-1` or Monaco raw):

```
Hosted manager (domain.normalize): FAIL - Restart/red-flag continuation needs a
combined classification; automatic segment merging is not supported yet.
Season Narrative (bridge parse_json): PASS - parsed 24 drivers, ...
```

Repaired Monaco evidence file: manager PASS (24 drivers, 8 warnings); Narrative
PASS (24 drivers, 4 anomalies). Validation raises 14 data warnings, 12 of them
the manager's own lap-sum withholding for drivers whose native restart clock
includes the 34.381 s pre-lap offset, plus the lap-0 retirement cluster; that
withholding already happens on the raw restart segment and does not affect
finishers' totals or points.

## 8. Timings (recorded)

- Full test suite: 49 tests in 26.4 s (fixture hashing included).
- 6,834,545 B Imola `083550-16536-47`: open 0.39-0.46 s, save 0.37-0.44 s,
  validate 0.003 s, `--rf-inspect` (21 segments) ~1.6 s.
- Monaco 6,165,687 B: parse 0.13 s; `--rf-repair` end-to-end ~1.5 s.
- Editing a 24-row classification grid and re-validating: well under 200 ms
  (validate itself 3-5 ms).

## 9. Decisions files used

- `fixtures\decisions\monaco_combined_era_2001_2004.json`
- `fixtures\decisions\monaco_stopped_override.json`
- `fixtures\decisions\imola_115557_combined_era_2001_2004.json`
- `fixtures\decisions\imola_083550_combined_override.json`

## 10. Abort conditions check

- No fixture hash changed.
- Manager and Narrative import read-only in-process; `sys.dont_write_bytecode`
  held for the whole consumer call.
- The Monaco oracle was met without changing any D1-D6 decision; the single D6
  text/oracle discrepancy is recorded above and resolved in favour of the
  plan's oracle table.
- No collector, Red Flag mod, Season Narrative, manager, `production\` or VPS
  file was modified. WP7 was not started.
- Validator error count unchanged (11).

## 11. Known limitations and pending work

- WP7 (manager half-points) is **not implemented**; the editor records the
  intended rule and warns that the manager awards full points.
- Lap-chart merging is implemented but not exercised by the corpus (no
  restarted-numbering segment with complete per-driver charts); it is kept as
  recorded with a note in that case.
- Predecessor search is filename-token based; documented in README/RF_REPAIR.
- GUI usability and a test-universe upload are owner validation.

## 12. Pending owner validation (not claimable by the LLM)

1. Open the Monaco fixture in the GUI, run the red-flag wizard with defaults,
   and review the result against the section 5 table.
2. Edit a qualifying and a race result by hand, review the diff, save.
3. Upload the repaired Monaco to a **test universe** in the hosted manager and
   confirm the import, classification and points.
4. Decide on WP7 (half points in the manager).

## 13. Restoration

- The original export is never written; each trials record keeps an untouched
  byte copy in `source\` plus `EDIT_MANIFEST.json` with the SHA256s. Copying
  `source\<name>` over any file restores the original bytes exactly.
- Component README before-state:
  `workspace_organization_audit\race_data_editor_BEFORE_2026-09-25\README.md`
  (SHA256 `7C8FC5397E3AD59926B70E09444F1CBF11217FD61BCEA0D6052DAFE3AB136653`).
- No file was deleted; only the tool tree, its tests/fixtures and documentation
  were added, plus the additive component README link.

## 14. Wasted-effort check (governance PIPELINE.md)

- Repeated reading of the same large material: none; the two long prompt
  documents were read once and the corpus was probed with scripted,
  targeted reads.
- Unnecessarily broad builds or tests: the full suite is 26 s and was run
  four times (WP gates plus final); no redundant full runs after
  documentation-only edits.
- Failed or retried commands: several PowerShell quoting failures when
  embedding multi-line Python in `-c`; switched to temporary script files in
  the approved temp directory after the second failure. One GUI path
  (`PanedWindow`/notebook parenting and the `jd.get` default sentinel) needed
  two iterations and is covered by the smoke test now.
- Polling/idle: none observed.
