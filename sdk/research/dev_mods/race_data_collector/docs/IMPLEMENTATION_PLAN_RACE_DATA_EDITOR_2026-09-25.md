# Implementation plan — Race Data editor and red-flag repair (2026-09-25)

Status: **handoff document**. Give this whole file to the implementing LLM.
Pipeline stage: specify → implement. Nothing is built yet. No export, consumer
or production artifact was changed while writing this plan.

Requirement sources, both mandatory, read in full:

- `docs\EDITOR_TOOL_PROMPT_2026-09-25.md` (rev 2): editor, fidelity, save
  model, consumer contracts.
- `docs\EDITOR_RF_REPAIR_PROMPT_2026-09-25.md` (rev 3): red-flag "race stopped"
  and "race resumed" repair.

This plan **resolves rev 3's open decisions** (section 1), fixes the expected
Monaco result (section 2) and orders the work (sections 3–6). Where this plan and
rev 3 differ, this plan wins.

---

## 1. Resolved decisions

Evidence tiers: **V** = verified by reading data or code in this workspace;
**R** = reported by an external secondary source; **O** = owner-delegated
default chosen by the planner, reversible by the owner.

### D1 Rule profile for red-flag races — default `era_2001_2004`

Races are 2002-season. Default profile `era_2001_2004`:

| Leader's completed laps `C` when the red flag started | Resumed? | Classification |
|---|---|---|
| `C < 2` | yes | First part **void**. Restart is a new race over the scheduled distance, from the **original grid**. Result = restart only. Drivers out in the void part are placed after all restart participants, by original grid, as `dnf: true`, 0 laps, keeping their first-part retirement reason. |
| `2 ≤ C` | yes | Restart grid = order at countback lap. Result = **order at the end of the final counted segment**. Laps: native cumulative value if lap numbering continued; if numbering restarted, `countback laps + restart laps`. Times: see D6. |
| any | no (owner declares stopped, B1) | Result = order at the end of lap `C − 1` ("penultimate lap before the lap during which the signal was given"). If `C < 2` the tool warns "no result under this profile" and requires an explicit override to continue. |

Multiple suspensions are applied in sequence, oldest first.

Other profiles stay selectable, but not as default: `aggregate` (combined times;
last used in 1994) and `restart_order_plain`.

Evidence:
- R: 2001 Belgian GP, stopped on lap 5. The first start was declared null and
  void; the restart used the order after lap 4; the result consisted only of the
  restarted race ([Wikipedia](https://en.wikipedia.org/wiki/2001_Belgian_Grand_Prix)).
- R: aggregate two-part results were last used in 1994
  ([The Race](https://www.the-race.com/formula-1/rise-fall-spike-f1s-relationship-with-the-red-flag/)).
- R: the countback wording, and the two-lap/75% half-points thresholds, are
  documented for later regulations. Half points below 75% are reported for the
  earlier era ([Wikipedia list](https://en.wikipedia.org/wiki/List_of_red-flagged_Formula_One_races)).
- The 2002 regulation text itself was not found. The implementer must not claim
  the profile is the verbatim 2002 FIA rule. `RF_REPAIR.md` must state this.
- V: the game behaved consistently with the `C < 2` row at Monaco: the restart
  ran the full 39 laps with lap numbering reset.

### D2 Countback — `k = 1` on completed laps

With `C` = the leader's completed laps at the signal, stopped-race
classification uses the order at the end of lap `C − 1`. `k` stays editable
(0–2) and is recorded in `metadata.repair.parameters`. (R, as D1.)

### D3 Retirement clusters — genuine by default; flag only unsupported ones

A retirement is **supported** when the same driver has a `contact_observed`,
`car_condition_changed` or `pit_state_changed` key event in the same segment
within the 120 s window ending 0.5 s before the retirement. Events at the same
timestamp do not count; they are consequences. Supported retirements, and the
player's own retirement, are kept as recorded. Unsupported retirements are listed
for owner review, and the owner decides. No automatic reversal ever.

Evidence (V): Monaco final segment, 20 retirements. 19 have prior contact
and/or condition changes plus a pit entry. The player (`Tester A`, lap 27)
has a prior pit state change only. Segment 0's two retirements (Wilson, Coulthard)
are also supported. The seven lap-0 retirements (134–202 s)
all follow contacts at about 40–136 s and a pit entry. They are consistent with a
first-lap pile-up, not a collector artifact. This matches the prior finding in
`operations\LIVE_MASS_RETIREMENTS_2026-09-15.md` that mass native retirements
can be genuine.

### D4 Manager identity scheme — adopt the rev 3 default

Keep `schemaVersion f1c.race-data/2.0`. Move the continuation fields into
`metadata.repair`. Set `metadata.capture.combinedClassification = true`. Use the
derived captureId `<finalSegmentCaptureId>.rf-<8 hex>`. Append the
`owner_reviewed_red_flag_repair` issue. Keep `gameResults.official = false`.

Evidence (V): Monaco modified in memory this way passes `domain.normalize` with
24 drivers. This matches the manager's own rejection text ("needs a combined
classification").

Consequences to document: re-repairing with different decisions yields a new
captureId. The manager still rejects a second capture for an already-used
round, so corrections after upload need an alternate universe.

### D5 Half points — record now, manager support is a separate package

The manager awards points by position from one per-universe points table
(`domain.scored`); a race is classified if `Finished`, or `Ret` with at least
floor(90% of winner laps) laps. It has no per-race factor (V).

The editor records `metadata.repair.points = {rule: "full" | "half" | "none",
reason}`. Under `era_2001_2004` the rule is `none` if the stopped result has
`C − 1 < 2`, `half` if below 75% of the scheduled distance, else `full`. Show a
warning that the manager will award full points. Manager support is work
package **WP7**, which is optional and needs separate owner approval before any
production deploy.

### D6 Times, placeholders and fastest lap

- Rows with `lapsCompleted == 0`: set `totalTime "-"`, `totalTimeRaw null`,
  gaps `"-"`. Monaco shows copied clocks (34.381 s / 105.741 s) (V).
- `2 ≤ C` resumed: `totalTimeRaw` = the final counted segment's elapsed clock
  (restart-only race time), consistent with D1. Mark it derived.
- `fastestLap`: keep native `author`/`time`/`timeRaw`. If `lapNumber` is 0 and
  no `lapChart` lap matches `timeRaw` within 1 ms, **leave it 0** and add a
  "lap number unknown" info note. Do not invent a lap number.
  V: Monaco native fastest is Sarrazin 1:21.354, but the observed `lapChart`
  minimum is TesterA 82.262 s on lap 19. Sarrazin's fastest lap came from
  unobserved native finalization.
- Grid, `qualifyingResults` and `initialGrid` are restored from the **first
  counted segment** (V: at Monaco, segment 0 grid, qualifying and initial grid
  agree with each other and have unique positions. The final segment shifted
  everyone up two places after removing Coulthard and Wilson, then put Coulthard
  24th and Wilson a duplicate 2nd.)

## 2. Expected Monaco result (hand-derived; use as a test oracle)

Input: `%USERPROFILE%\Downloads\SINGLE_Monaco, 2002_20260925_144455-9388-11_20260925_144455.json`,
SHA256 `8fe125420f245b0a22295af4e15e50bf73efe2e1873b118343e8d397da5eb31b`.
The leader completed 1 lap before the flag (`C = 1`), so D1 row 1 applies: the
first part is void and the result is the restart race.

| Pos | Driver | Laps | Status | Grid (restored) | Total time |
|---|---|---|---|---|---|
| 1 | Olivier Panis (2002) | 39 | Finished | 24 | native 57:30.277 |
| 2 | Stephane Sarrazin (2002) | 39 | Finished | 22 | native 58:11.313 |
| 3 | Tester A | 27 | Ret | 7 | native |
| 4 | Nick Heidfeld (2002) | 25 | Ret | 14 | native |
| 5 | Michael Schumacher (2002) | 23 | Ret | 11 | native |
| 6 | Luca Badoer (2002) | 20 | Ret | 4 | native |
| 7 | Jenson Button (2002) | 14 | Ret | 17 | native |
| 8 | Kimi Raikkonen (2002) | 10 | Ret | 1 | native |
| 9 | Marcel Lasee (2002) | 10 | Ret | 6 | native |
| 10 | Fernando Alonso (2002) | 7 | Ret | 12 | native |
| 11 | Felipe Massa (2002) | 6 | Ret | 10 | native |
| 12 | Takuma Sato (2002) | 6 | Ret | 13 | native |
| 13 | Rubens Barrichello (2002) | 6 | Ret | 8 | native |
| 14 | Ricardo Zonta (2002) | 4 | Ret | 19 | native |
| 15 | Jirko Malcharek (2002) | 3 | Ret | 21 | native |
| 16 | Darren Manning (2002) | 0 | Ret | 18 | `-` / null |
| 17 | Matteo Bobbi (2002) | 0 | Ret | 23 | `-` / null |
| 18 | Jarno Trulli (2002) | 0 | Ret | 20 | `-` / null |
| 19 | Andre Lotterer (2002) | 0 | Ret | 16 | `-` / null |
| 20 | Juan Pablo Montoya (2002) | 0 | Ret | 15 | `-` / null |
| 21 | Ralf Schumacher (2002) | 0 | Ret | 9 | `-` / null |
| 22 | Luciano Burti (2002) | 0 | Ret | 3 | `-` / null |
| 23 | Justin Wilson (2002) | 0 | Ret (void part, reason 7) | 2 | `-` / null |
| 24 | David Coulthard (2002) | 0 | Ret (void part, reason 0) | 5 | `-` / null |

Positions 1–22 keep the native restart order (laps, then native order); only
Coulthard and Wilson move to the end, ordered by original grid. Expected manager
points with a 2002 table (10-6-4-3-2-1): Panis 10, Sarrazin 6. Nobody else
reaches floor(0.9 × 39) = 35 laps. `metadata.repair.points.rule = "full"`
(resumed race).

Also expected: `metadata.championshipRaceNumber` stays 0 (the owner selects
the round at upload); `fastestLap.lapNumber` stays 0 with an info note; the
`gameResults.drivers` ↔ `driverStandings` mirror is identical; the manager's
`normalize` accepts the file; the Season Narrative bridge parses it.

## 3. Work packages (in order; each ends with its gate)

Tool root: `development\mods\race_data_collector\tools\race_data_editor\`.
Suggested modules: `rde\jsondoc.py` (lexeme-preserving parse/serialize, paths),
`rde\model.py` (document, undo stack, linked edits), `rde\validate.py`,
`rde\consumers.py`, `rde\rf\chain.py`, `rde\rf\evidence.py`,
`rde\rf\profiles.py`, `rde\rf\repair.py`, `rde\save.py`, `rde\gui\*.py`,
`race_data_editor.py` (CLI/GUI entry), `run_editor.cmd`, `tests\`,
`fixtures\decisions\*.json`, `README.md`, `docs\RF_REPAIR.md`,
`VERIFICATION.md`.

**WP0 Orientation.** Do the cold start from rev 2. Record the validator
baseline (11 errors). Hash every fixture: all `RaceData\*.json` and the
Downloads Monaco file.
*Gate:* baseline and hashes recorded in `VERIFICATION.md`.

**WP1 Core JSON document.** Lexeme-preserving parse/serialize (stdlib `json` with
`parse_int`/`parse_float` capturing source text, a duplicate-key-rejecting
`object_pairs_hook`, `parse_constant` rejection). A custom serializer
reproducing `json.dumps(indent=2, ensure_ascii=False)` + LF. Path get/set/insert/
delete/move. Undo/redo stack with grouped actions.
*Gate:* 62/62 `RaceData` + Monaco round-trip byte-identical (a prototype of this
approach already reached 62/62; placeholder text must not contain control
characters). Unit tests for paths and undo.

**WP2 Validation + consumer checks.** Every rule in rev 2 "Downstream consumers"
and "Validation panel", plus rev 3 red-flag rules. `consumers.py` runs
`domain.normalize(strict_load())` and the Season Narrative bridge with
`sys.dont_write_bytecode = True` on a temp copy that keeps the file name.
*Gate:* `--check` passes on the Monza acceptance file (rev 2), reports the manager rejection on
the 9 continuation captures, and every rev 2 lint test passes. The Season
Narrative `__pycache__` mtimes are unchanged.

**WP3 Save model + CLI.** `trials\editor_edits\<timestamp>\` with the original
file name, a `source\` copy, `EDIT_MANIFEST.json` (``);
*Export copy to…* requires typed confirmation to overwrite; `--set`, `--out`.
*Gate:* rev 2 save-model tests; restoration from `source\` is byte-identical.

**WP4 Red-flag engine (headless).** `chain.py` (embedded segments; predecessor
search in `RaceData\` and `recovery\`, read-only; cycle and missing-link
reporting), `evidence.py` (per-driver per-segment laps, retirements, positions
per lap with source tier, lap-numbering continued/restarted detection,
`restart_baseline_observed` use, D3 support rule), `profiles.py` (D1/D2/D5/D6),
`repair.py` (proposal list → apply → `metadata.repair`, D4 identity).
CLI `--rf-inspect` and `--rf-repair` with a decisions file.
*Gate:* the section 4 RF tests pass, including the Monaco oracle in section 2.

**WP5 GUI.** tkinter/ttk per rev 2: task views, linked mirror, tree, raw text,
validation panel, consumer-check button, before-save diff. Rev 3 "Red flag…"
wizard: timeline, segment selection, mode/profile/parameters, proposal table,
per-driver decisions, preview with consumer check, apply as one undo step.
Heavy work runs off the UI thread.
*Gate:* rev 2 performance targets measured on the 6.8 MB Imola file; GUI
smoke test (programmatic: open, edit a cell, undo, run the Monaco wizard with the
checked-in decisions, save to temp).

**WP6 Documentation and handoff.** Tool `README.md`, `docs\RF_REPAIR.md` (profiles
and their evidence tiers, D1–D6, the identity scheme, the Monaco worked example,
"not verbatim 2002 FIA text"), `VERIFICATION.md`. Link from the component README
(preserve its before-state first under
`workspace_organization_audit\<PHASE>_BEFORE_<YYYYMMDD>\`). Re-run the validator.
*Gate:* validator error count unchanged (11); documentation gate from workspace
`AGENTS.md`.

**WP7 (optional, separate approval) Manager half-points.** Spec only unless
the owner approves: `domain.normalize` reads `metadata.repair.points.rule`;
`scored` multiplies by 0.5 or 0; tests in `development\season_narrative\tests`;
deploy per `operations\vps` practice. Do not start without explicit owner
approval; production and VPS changes are outside this tool's scope.

## 4. Test matrix (headless, `python -m unittest`, temp dirs only)

Every run hashes all fixtures before and after, and fails on any change.

| # | Test | Expectation |
|---|---|---|
| T1 | Round-trip all 62 `RaceData` + Monaco | byte-identical |
| T2 | rev 2 edit tests (swap P1/P2, rename, qual time + raw, add key, remove element, type change) | reopen passes manager `normalize` |
| T3 | rev 2 lint tests | each rule fires |
| T4 | `--rf-inspect` on 9 continuation captures | Monaco: 2 segments, `C = 1`, numbering reset, duplicate grid 2, 2 void-part DNFs reappearing as placeholders, 0 unsupported retirements (D3) |
| T5 | Monaco `--rf-repair --mode combined --profile era_2001_2004` with checked-in decisions | exact section 2 table (positions, laps, status, grid, placeholders), mirror identical, manager accepts, Narrative parses, points Panis 10 / Sarrazin 6 via `domain.scored` |
| T6 | Monaco `--mode stopped` | refuses without override (`C − 1 = 0`); with override: records `points.rule = "none"` and warning |
| T7 | Imola `115557-22412-26` combined (segment leader laps 2, 7; not hand-verified by the planner) | D1 row 2 applied; the implementer writes the expected table by hand from `--rf-inspect` output before running repair; grid restored; `normalize` accepts |
| T8 | Imola `083550-16536-47` (20 one-lap segments, final 24/24 DNF) | inspect reports no valid classification; repair refuses without owner override; deterministic captureId stable across runs, changes when a decision changes |
| T9 | Fixtures with 0 embedded segments | missing predecessor reported; repair on available segments only with warning |
| T10 | Repaired file reopened + saved unedited | byte-identical |
| T11 | Reject all proposals | document unchanged |
| T12 | Performance 6.8 MB | open, save < 3 s each, recorded |
| T13 | Consumer isolation | no writes outside temp; Narrative `__pycache__` unchanged |

## 5. Abort conditions (stop and report to the owner)

- Any fixture hash changes.
- The manager `normalize` or Narrative bridge cannot be imported read-only.
- The Monaco oracle cannot be met without changing a D1–D6 decision (report
  which one and why; do not silently change the oracle).
- Any need to modify the collector, Red Flag mod, Season Narrative, the hosted
  manager, `production\` or the VPS (except WP7 after approval).
- The workspace validator error count rises.

## 6. Owner validation after delivery (not claimable by the LLM)

1. Open the Monaco file in the GUI, run the red-flag wizard with defaults, and
   review the result against section 2.
2. Edit a qualifying and a race result by hand, review the diff, save.
3. Upload the repaired Monaco to a **test universe** in the hosted manager
   and confirm the import, classification and points.
4. Decide on WP7 (half points in the manager).

## Restoration

This plan adds one document. The component README before-state is preserved at
`workspace_organization_audit\editor_prompt_review_BEFORE_2026-09-25\r4_README.md`.
